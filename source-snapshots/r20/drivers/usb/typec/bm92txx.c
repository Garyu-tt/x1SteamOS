// SPDX-License-Identifier: GPL-2.0
/*
 * Rohm BM92TXX USB Type-C and Power Delivery controller driver
 *
 * Copyright (c) 2020-2023 CTCaer <ctcaer@gmail.com>
 * Copyright (c) 2026 Alexandre Hamamdjian <azkali.limited@gmail.com>
 *
 * The BM92T36 (and pin/protocol-compatible siblings) integrate an MCU that
 * runs the USB-PD state machine autonomously.  The host configures the chip
 * over I2C and observes a level-triggered ALERT line; this driver is a thin
 * adaptor between the chip's status registers and the Linux USB Type-C and
 * power_supply frameworks.
 *
 * This is the introductory driver: it brings up the Type-C port, the
 * partner on attach, the negotiated PD contract, and exposes that to user
 * space through a power_supply.  Alternate-mode handling (DisplayPort and
 * the Nintendo-dock SVID) is left for follow-up changes that plug into the
 * typec altmode framework.
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/cleanup.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/property.h>
#include <linux/regulator/consumer.h>
#include <linux/unaligned.h>
#include <linux/usb/pd.h>
#include <linux/usb/pd_vdo.h>
#include <linux/usb/role.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_altmode.h>

/* ------------------------------ Registers ------------------------------- */

#define BM92T_REG_ALERT_STATUS		0x02
#define BM92T_REG_STATUS1		0x03
#define BM92T_REG_STATUS2		0x04
#define BM92T_REG_COMMAND		0x05
#define BM92T_REG_CONFIG1		0x06
#define BM92T_REG_DEV_CAPS		0x07
#define BM92T_REG_READ_PDOS_SRC		0x08	/* up to 28 bytes */
#define BM92T_REG_CONFIG2		0x17
#define BM92T_REG_DP_STATUS		0x18
#define BM92T_REG_DP_ALERT_EN		0x19
#define BM92T_REG_VENDOR_CONFIG		0x1A
#define BM92T_REG_AUTO_NGT_FIXED	0x20	/* 4 bytes */
#define BM92T_REG_AUTO_NGT_BATT		0x23	/* 4 bytes */
#define BM92T_REG_SYS_CONFIG1		0x26
#define BM92T_REG_SYS_CONFIG2		0x27
#define BM92T_REG_CURRENT_PDO		0x28	/* 4 bytes */
#define BM92T_REG_CURRENT_RDO		0x2B	/* 4 bytes */
#define BM92T_REG_ALERT_ENABLE		0x2E
#define BM92T_REG_SYS_CONFIG3		0x2F
#define BM92T_REG_SET_RDO		0x30	/* 4 bytes */
#define BM92T_REG_FW_TYPE		0x4B
#define BM92T_REG_FW_REVISION		0x4C
#define BM92T_REG_MAN_ID		0x4D
#define BM92T_REG_DEV_ID		0x4E
#define BM92T_REG_REV_ID		0x4F
#define BM92T_REG_INCOMING_VDM		0x50	/* length-prefixed, up to 28 bytes */
#define BM92T_REG_OUTGOING_VDM		0x60	/* length-prefixed, up to 28 bytes */

/* ALERT_STATUS bits */
#define BM92T_ALERT_SNK_FAULT		BIT(0)
#define BM92T_ALERT_SRC_FAULT		BIT(1)
#define BM92T_ALERT_CMD_DONE		BIT(2)
#define BM92T_ALERT_PLUGPULL		BIT(3)
#define BM92T_ALERT_DP_EVENT		BIT(6)
#define BM92T_ALERT_DR_SWAP		BIT(10)
#define BM92T_ALERT_VDM_RECEIVED	BIT(11)
#define BM92T_ALERT_CONTRACT		BIT(12)
#define BM92T_ALERT_SRC_PLUGIN		BIT(13)
#define BM92T_ALERT_PDO			BIT(14)

/* STATUS1 bits */
#define BM92T_S1_FAULT_MASK		GENMASK(1, 0)
#define BM92T_S1_LASTCMD_MASK		GENMASK(6, 4)
#define BM92T_S1_INSERTED		BIT(7)
#define BM92T_S1_DATA_ROLE_MASK		GENMASK(9, 8)
#define BM92T_S1_VSAFE			BIT(10)
#define BM92T_S1_CSIDE			BIT(11)
#define BM92T_S1_SRC_MODE		BIT(12)
#define BM92T_S1_CMD_BUSY		BIT(13)
#define BM92T_S1_SPDSNK			BIT(14)
#define BM92T_S1_SPDSRC1		BIT(15)

#define BM92T_DR_NONE			0
#define BM92T_DR_UFP			1
#define BM92T_DR_DFP			2
#define BM92T_DR_ACC			3

/* STATUS2 bits */
#define BM92T_S2_PDOI_MASK		BIT(3)
#define BM92T_S2_VCONN_ON		BIT(9)
#define BM92T_S2_OTG_INSERT		BIT(13)

/* CONFIG1 fields */
#define BM92T_CFG1_SPDSRC_MASK		GENMASK(15, 14)
#define BM92T_CFG1_SPDSRC12_ON		0
#define BM92T_CFG1_SPDSRC12_OFF		3

/* VENDOR_CONFIG */
#define BM92T_VENDOR_OCP_DISABLE	BIT(2)

/* Commands written to BM92T_REG_COMMAND.  Each command byte is duplicated. */
#define BM92T_CMD_PS_RDY		0x0505
#define BM92T_CMD_SEND_RDO		0x0707
#define BM92T_CMD_PD_HARD_RST		0x0808
#define BM92T_CMD_SYS_RESET		0x0D0D
#define BM92T_CMD_SEND_VDM		0x1111
#define BM92T_CMD_ACCEPT_VDM		0x1616

/*
 * The BM92T returns variable-length payloads with a length byte prefix.
 * 29 bytes is the largest documented register block (incoming VDM).
 */
#define BM92T_MAX_PAYLOAD_LEN		28
#define BM92T_PAYLOAD_BUF_LEN		(1 + BM92T_MAX_PAYLOAD_LEN)

/* ------------------------------ Driver state ---------------------------- */

/**
 * struct bm92t_pdata - DT-derived constraints
 * @vconn_en_gpio: optional external VCONN switch enable, asserted while a
 *     PD partner is attached.
 * @pd_input_current_limit_microamp: per-PDO-voltage input current caps,
 *     indexed by [0]=5V, [1]=9V, [2]=12V, [3]=15V.  A zero entry means
 *     "no driver-imposed cap, use the source's advertised current".
 */
struct bm92t_pdata {
	struct gpio_desc *vconn_en_gpio;
	u32 pd_input_current_limit_microamp[4];
};

/*
 * Discovery state walks the partner from "just attached" through Discover
 * Identity, Discover SVIDs and per-SVID Discover Mode, and lands back in
 * IDLE once every advertised altmode has been registered with the typec
 * framework.  ENTER/EXIT VDMs initiated by an altmode driver short-circuit
 * the state machine because they are tied to a specific registered
 * typec_altmode and do not feed back here.
 */
enum bm92t_disc_state {
	BM92T_DISC_IDLE,
	BM92T_DISC_ID_SENT,
	BM92T_DISC_SVIDS_SENT,
	BM92T_DISC_MODE_SENT,
	BM92T_DISC_DONE,
};

#define BM92T_MAX_SVIDS		16
#define BM92T_MAX_ALTMODES	8

struct bm92t_pd {
	struct i2c_client		*client;
	struct device			*dev;
	/* Serialises chip access and driver state against the IRQ thread. */
	struct mutex			lock;

	struct bm92t_pdata		pdata;

	struct typec_port		*port;
	struct typec_partner		*partner;
	struct typec_capability		cap;
	struct usb_pd_identity		partner_identity;
	bool				identity_known;

	/* Discovery / alternate mode bookkeeping. */
	enum bm92t_disc_state		disc_state;
	u16				svids[BM92T_MAX_SVIDS];
	unsigned int			num_svids;
	unsigned int			cur_svid_idx;
	struct typec_altmode		*altmodes[BM92T_MAX_ALTMODES];
	unsigned int			num_altmodes;

	struct usb_role_switch		*role_sw;

	struct power_supply		*psy;
	struct power_supply_desc	psy_desc;

	struct regulator		*vbus;
	struct regulator		*vbus_src;
	struct regulator		*chg_reg;

	/* cached, last-known values from the chip */
	u32				active_pdo;
	u32				active_rdo;
	unsigned int			contract_voltage_mv;
	unsigned int			contract_current_ma;
	unsigned int			input_current_limit_ua;

	bool				attached;
	bool				is_source;
	bool				vconn_supplied;
};

/* The four PD voltage rails the BM92T can negotiate to as a sink. */
static const unsigned int bm92t_pd_voltages_mv[4] = { 5000, 9000, 12000, 15000 };

#define BM92T_PSY_USB_TYPES					\
	(BIT(POWER_SUPPLY_USB_TYPE_C) |				\
	 BIT(POWER_SUPPLY_USB_TYPE_PD) |			\
	 BIT(POWER_SUPPLY_USB_TYPE_PD_DRP))

static const enum power_supply_property bm92t_psy_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

/* ----------------------------- I2C helpers ------------------------------ */

/*
 * Read a register block.  The BM92T returns multi-byte registers with a
 * one-byte length prefix; @buf is the raw transfer buffer of size @len, and
 * the caller is responsible for inspecting buf[0] before consuming the
 * payload.  Validate len up-front so we never write past the caller's
 * buffer when the chip reports a bogus length.
 */
static int bm92t_read_block(struct bm92t_pd *bm, u8 reg, u8 *buf, size_t len)
{
	struct i2c_msg msgs[2] = {
		{
			.addr	= bm->client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
		},
		{
			.addr	= bm->client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		},
	};
	int ret;

	if (len < 1 || len > BM92T_PAYLOAD_BUF_LEN)
		return -EINVAL;

	ret = i2c_transfer(bm->client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;
	return 0;
}

/* Read a 16-bit register (little-endian, like every BM92T register). */
static int bm92t_read16(struct bm92t_pd *bm, u8 reg, u16 *val)
{
	u8 buf[2];
	int ret = bm92t_read_block(bm, reg, buf, sizeof(buf));

	if (ret)
		return ret;
	*val = get_unaligned_le16(buf);
	return 0;
}

/* Read a 32-bit PDO/RDO register: 1-byte length prefix + 4 LE payload bytes. */
static int bm92t_read32_lp(struct bm92t_pd *bm, u8 reg, u32 *val)
{
	u8 buf[5];
	int ret = bm92t_read_block(bm, reg, buf, sizeof(buf));

	if (ret)
		return ret;
	if (buf[0] < 4)
		return -EPROTO;
	*val = get_unaligned_le32(&buf[1]);
	return 0;
}

static int bm92t_write_block(struct bm92t_pd *bm, u8 reg,
			     const u8 *payload, size_t len)
{
	u8 stack[1 + BM92T_PAYLOAD_BUF_LEN];

	if (len > BM92T_PAYLOAD_BUF_LEN)
		return -EINVAL;

	stack[0] = reg;
	memcpy(&stack[1], payload, len);
	return i2c_master_send(bm->client, stack, 1 + len) == 1 + len ? 0 : -EIO;
}

static int bm92t_write16(struct bm92t_pd *bm, u8 reg, u16 val)
{
	u8 buf[2];

	put_unaligned_le16(val, buf);
	return bm92t_write_block(bm, reg, buf, sizeof(buf));
}

static int bm92t_send_cmd(struct bm92t_pd *bm, u16 cmd)
{
	return bm92t_write16(bm, BM92T_REG_COMMAND, cmd);
}

/* ------------------------------ VDM transit ----------------------------- */

/*
 * Push a Structured VDM into the BM92T's outgoing buffer and kick the chip
 * to send it.  The buffer layout is identical for SOP / SOP' / SOP'';
 * higher layers select the right SEND_VDM_* command if they need cable
 * VDMs.  @nvdos is the number of 32-bit Vendor Defined Objects after the
 * 32-bit VDM header.
 */
static int bm92t_send_vdm(struct bm92t_pd *bm, u32 header,
			  const u32 *vdos, unsigned int nvdos)
{
	u8 buf[1 + 4 + 7 * 4];	/* len + header + max 7 VDOs */
	unsigned int payload = 4 + nvdos * 4;
	unsigned int i;
	int ret;

	if (nvdos > 7)
		return -EINVAL;

	buf[0] = payload;
	put_unaligned_le32(header, &buf[1]);
	for (i = 0; i < nvdos; i++)
		put_unaligned_le32(vdos[i], &buf[1 + 4 + i * 4]);

	ret = bm92t_write_block(bm, BM92T_REG_OUTGOING_VDM, buf, 1 + payload);
	if (ret)
		return ret;
	return bm92t_send_cmd(bm, BM92T_CMD_SEND_VDM);
}

/*
 * Build a Structured VDM header for an Initiator command targeting the
 * given SVID.  Object position is 0 for non-modal commands (Discover ID,
 * Discover SVIDs); the framework can pass an explicit one for mode-aware
 * commands later via the altmode ops.
 */
static u32 bm92t_make_svdm_header(u16 svid, u8 cmd)
{
	return VDO(svid, /* SVDM */ 1, /* SVDM ver 1.0 */ 0, cmd);
}

static int bm92t_start_discover_identity(struct bm92t_pd *bm)
{
	u32 hdr = bm92t_make_svdm_header(USB_SID_PD, CMD_DISCOVER_IDENT);

	bm->identity_known = false;
	bm->disc_state = BM92T_DISC_ID_SENT;
	return bm92t_send_vdm(bm, hdr, NULL, 0);
}

static int bm92t_start_discover_svids(struct bm92t_pd *bm)
{
	u32 hdr = bm92t_make_svdm_header(USB_SID_PD, CMD_DISCOVER_SVID);

	bm->disc_state = BM92T_DISC_SVIDS_SENT;
	return bm92t_send_vdm(bm, hdr, NULL, 0);
}

static int bm92t_start_discover_mode(struct bm92t_pd *bm)
{
	u32 hdr;

	if (bm->cur_svid_idx >= bm->num_svids) {
		bm->disc_state = BM92T_DISC_DONE;
		return 0;
	}

	hdr = bm92t_make_svdm_header(bm->svids[bm->cur_svid_idx],
				     CMD_DISCOVER_MODES);
	bm->disc_state = BM92T_DISC_MODE_SENT;
	return bm92t_send_vdm(bm, hdr, NULL, 0);
}

static const struct typec_altmode_ops bm92t_altmode_ops;

static int bm92t_register_altmode(struct bm92t_pd *bm, u16 svid,
				  u8 mode_pos, u32 vdo)
{
	struct typec_altmode_desc desc = {
		.svid	= svid,
		.mode	= mode_pos,
		.vdo	= vdo,
	};
	struct typec_altmode *alt;

	if (bm->num_altmodes >= ARRAY_SIZE(bm->altmodes))
		return -ENOSPC;
	if (!bm->partner)
		return -ENODEV;

	alt = typec_partner_register_altmode(bm->partner, &desc);
	if (IS_ERR(alt))
		return PTR_ERR(alt);

	/*
	 * Stash the port-driver context on the partner-side altmode so that
	 * the ops below can reach back into the BM92T over the typec bus
	 * without having to walk the device hierarchy.
	 */
	typec_altmode_set_drvdata(alt, bm);
	typec_altmode_set_ops(alt, &bm92t_altmode_ops);

	bm->altmodes[bm->num_altmodes++] = alt;
	return 0;
}

static void bm92t_unregister_altmodes(struct bm92t_pd *bm)
{
	unsigned int i;

	for (i = 0; i < bm->num_altmodes; i++) {
		if (bm->altmodes[i])
			typec_unregister_altmode(bm->altmodes[i]);
		bm->altmodes[i] = NULL;
	}
	bm->num_altmodes = 0;
}

/* ----------------------------- Altmode ops ------------------------------ */

/*
 * Forward a VDM on behalf of an alternate-mode driver bound to one of our
 * partner altmodes.  Reached through the typec bus when the altmode driver
 * calls typec_altmode_vdm(); the BM92T is just the SVID-agnostic relay.
 */
static int bm92t_altmode_vdm(struct typec_altmode *alt, const u32 header,
			     const u32 *vdo, int count)
{
	struct bm92t_pd *bm = typec_altmode_get_drvdata(alt);

	if (!bm)
		return -ENODEV;
	if (count < 0 || count > 7)
		return -EINVAL;

	guard(mutex)(&bm->lock);
	return bm92t_send_vdm(bm, header, vdo, count);
}

static int bm92t_altmode_enter(struct typec_altmode *alt, u32 *vdo)
{
	u32 hdr = VDO(alt->svid, /* SVDM */ 1, /* SVDM ver 1.0 */ 0,
		      CMD_ENTER_MODE) | VDO_OPOS(alt->mode);

	return bm92t_altmode_vdm(alt, hdr, vdo, vdo ? 1 : 0);
}

static int bm92t_altmode_exit(struct typec_altmode *alt)
{
	u32 hdr = VDO(alt->svid, /* SVDM */ 1, /* SVDM ver 1.0 */ 0,
		      CMD_EXIT_MODE) | VDO_OPOS(alt->mode);

	return bm92t_altmode_vdm(alt, hdr, NULL, 0);
}

static const struct typec_altmode_ops bm92t_altmode_ops = {
	.vdm	= bm92t_altmode_vdm,
	.enter	= bm92t_altmode_enter,
	.exit	= bm92t_altmode_exit,
};

/*
 * Parse a structured VDM reply read out of INCOMING_VDM_REG.  Returns the
 * number of VDOs (object after the header) on success, or a negative
 * errno; on success @header is filled with the VDM header and @vdos[]
 * with up to @max VDO words.
 */
static int bm92t_read_incoming_vdm(struct bm92t_pd *bm, u32 *header,
				   u32 *vdos, unsigned int max)
{
	u8 raw[BM92T_PAYLOAD_BUF_LEN];
	unsigned int payload, nvdos, i;
	int ret;

	ret = bm92t_read_block(bm, BM92T_REG_INCOMING_VDM, raw, sizeof(raw));
	if (ret)
		return ret;
	payload = raw[0];
	if (payload < 4 || payload > sizeof(raw) - 1)
		return -EPROTO;

	*header = get_unaligned_le32(&raw[1]);
	nvdos = (payload - 4) / 4;
	if (nvdos > max)
		nvdos = max;
	for (i = 0; i < nvdos; i++)
		vdos[i] = get_unaligned_le32(&raw[1 + 4 + i * 4]);
	return nvdos;
}

static void bm92t_consume_identity_reply(struct bm92t_pd *bm,
					 const u32 *vdos, int nvdos)
{
	unsigned int i;

	if (!bm->partner || nvdos < 3)
		return;

	bm->partner_identity.id_header = vdos[0];
	bm->partner_identity.cert_stat = vdos[1];
	bm->partner_identity.product   = vdos[2];
	for (i = 0; i < ARRAY_SIZE(bm->partner_identity.vdo); i++)
		bm->partner_identity.vdo[i] =
			(i + 3 < (unsigned int)nvdos) ? vdos[i + 3] : 0;
	bm->identity_known = true;
	typec_partner_set_identity(bm->partner);

	dev_info(bm->dev,
		 "partner ID: 0x%08x cert 0x%08x prod 0x%08x\n",
		 bm->partner_identity.id_header,
		 bm->partner_identity.cert_stat,
		 bm->partner_identity.product);
}

/*
 * Each Discover SVIDs response packs up to six SVIDs in three VDOs, two
 * SVIDs per VDO (low16 then high16).  A 0x0000 entry terminates the list;
 * if the response is a full set without a terminator we send another
 * Discover SVIDs to continue, exactly like the framework expects.
 */
static void bm92t_consume_svids_reply(struct bm92t_pd *bm,
				      const u32 *vdos, int nvdos)
{
	bool terminated = false;
	int i;

	for (i = 0; i < nvdos; i++) {
		u16 low  = vdos[i] & 0xffff;
		u16 high = vdos[i] >> 16;

		if (high == 0)
			terminated = true;
		else if (bm->num_svids < ARRAY_SIZE(bm->svids))
			bm->svids[bm->num_svids++] = high;

		if (low == 0)
			terminated = true;
		else if (!terminated && bm->num_svids < ARRAY_SIZE(bm->svids))
			bm->svids[bm->num_svids++] = low;

		if (terminated)
			break;
	}

	if (!terminated && bm->num_svids < ARRAY_SIZE(bm->svids)) {
		/* Partner has more SVIDs to advertise. */
		bm92t_start_discover_svids(bm);
		return;
	}

	bm->cur_svid_idx = 0;
	bm92t_start_discover_mode(bm);
}

/*
 * Discover Mode reply: each VDO is one Mode VDO for the current SVID,
 * indexed 1..N.  Register every mode with the typec framework so altmode
 * drivers (DisplayPort at 0xFF01, the future Nintendo SVID driver at
 * 0x057E, ...) can bind.  Then advance to the next SVID.
 */
static void bm92t_consume_mode_reply(struct bm92t_pd *bm,
				     const u32 *vdos, int nvdos)
{
	u16 svid;
	int i;

	if (bm->cur_svid_idx >= bm->num_svids)
		return;

	svid = bm->svids[bm->cur_svid_idx];
	for (i = 0; i < nvdos && i < 6; i++) {
		int ret = bm92t_register_altmode(bm, svid, i + 1, vdos[i]);

		if (ret)
			dev_warn(bm->dev,
				 "altmode SVID 0x%04x mode %d: %d\n",
				 svid, i + 1, ret);
	}

	bm->cur_svid_idx++;
	bm92t_start_discover_mode(bm);
}

/*
 * Match a VDM SVID against our registered partner altmodes and hand the
 * VDM to that altmode's driver via the typec bus.  Returns true if the
 * VDM was claimed by an altmode driver.
 */
static bool bm92t_route_vdm_to_altmode(struct bm92t_pd *bm, u32 header,
				       const u32 *vdos, int nvdos)
{
	u16 svid = PD_VDO_VID(header);
	unsigned int i;

	for (i = 0; i < bm->num_altmodes; i++) {
		struct typec_altmode *alt = bm->altmodes[i];

		if (alt && alt->svid == svid) {
			typec_altmode_vdm(alt, header, vdos, nvdos);
			return true;
		}
	}
	return false;
}

/*
 * Handle a VDM the chip just dropped into INCOMING_VDM_REG.  Called from
 * the IRQ thread with bm->lock held.  We drive Discover Identity / SVIDs
 * / Mode ourselves; any other Structured VDM is forwarded to the matching
 * partner altmode through the typec bus so dedicated altmode drivers
 * (e.g. drivers/usb/typec/altmodes/displayport.c) see it.
 */
static void bm92t_handle_vdm_rx(struct bm92t_pd *bm)
{
	u32 vdos[7];
	u32 header;
	u16 svid;
	u8 cmd, cmdt;
	int nvdos;

	nvdos = bm92t_read_incoming_vdm(bm, &header, vdos, ARRAY_SIZE(vdos));
	if (nvdos < 0) {
		dev_dbg(bm->dev, "VDM read: %d\n", nvdos);
		goto ack;
	}

	svid = PD_VDO_VID(header);

	/*
	 * Unstructured VDMs do not carry CMD / CMDT fields; they belong
	 * to whichever altmode driver owns the SVID (e.g. the Nintendo
	 * dock altmode driver uses Unstructured VDMs for LED and hub
	 * control over SVID 0x057E).  Hand them off without further
	 * inspection here.
	 */
	if (!PD_VDO_SVDM(header)) {
		bm92t_route_vdm_to_altmode(bm, header, vdos, nvdos);
		goto ack;
	}

	cmd  = PD_VDO_CMD(header);
	cmdt = PD_VDO_CMDT(header);

	if (svid == USB_SID_PD && cmdt == CMDT_RSP_ACK) {
		switch (cmd) {
		case CMD_DISCOVER_IDENT:
			bm92t_consume_identity_reply(bm, vdos, nvdos);
			if (bm->disc_state == BM92T_DISC_ID_SENT)
				bm92t_start_discover_svids(bm);
			goto ack;
		case CMD_DISCOVER_SVID:
			if (bm->disc_state == BM92T_DISC_SVIDS_SENT)
				bm92t_consume_svids_reply(bm, vdos, nvdos);
			goto ack;
		}
	} else if (cmdt == CMDT_RSP_ACK && cmd == CMD_DISCOVER_MODES &&
		   bm->disc_state == BM92T_DISC_MODE_SENT &&
		   bm->cur_svid_idx < bm->num_svids &&
		   bm->svids[bm->cur_svid_idx] == svid) {
		bm92t_consume_mode_reply(bm, vdos, nvdos);
		goto ack;
	} else if (cmdt == CMDT_RSP_NAK || cmdt == CMDT_RSP_BUSY) {
		/* Partner refused / busy; advance the state machine. */
		if (bm->disc_state == BM92T_DISC_SVIDS_SENT) {
			bm->disc_state = BM92T_DISC_DONE;
		} else if (bm->disc_state == BM92T_DISC_MODE_SENT) {
			bm->cur_svid_idx++;
			bm92t_start_discover_mode(bm);
		}
		goto ack;
	}

	/* Anything else: hand off to the matching altmode driver. */
	bm92t_route_vdm_to_altmode(bm, header, vdos, nvdos);

ack:
	/* Tell the chip we've consumed the VDM so ALERT_N de-asserts. */
	bm92t_send_cmd(bm, BM92T_CMD_ACCEPT_VDM);
}

/* --------------------------- PD / power_supply -------------------------- */

/*
 * Subtract the static power reserve (host platform draw) from the source's
 * advertised current and apply the per-voltage DT cap.  All math is done in
 * microamps with explicit underflow guards: a 5 V source advertising less
 * than the reserve is treated as 0, not 4 billion.
 */
static unsigned int bm92t_clamp_input_current(struct bm92t_pd *bm,
					      unsigned int voltage_mv,
					      unsigned int source_ma)
{
	unsigned int reserved_ua = (voltage_mv > 5000) ? 4500000 : 2500000;
	unsigned int reserve_ma = DIV_ROUND_UP(reserved_ua, voltage_mv);
	unsigned int limit_ma = source_ma;
	int rail;

	if (limit_ma <= reserve_ma)
		limit_ma = 0;
	else
		limit_ma -= reserve_ma;

	for (rail = 0; rail < ARRAY_SIZE(bm92t_pd_voltages_mv); rail++) {
		u32 cap_ua;

		if (bm92t_pd_voltages_mv[rail] != voltage_mv)
			continue;
		cap_ua = bm->pdata.pd_input_current_limit_microamp[rail];
		if (cap_ua)
			limit_ma = min(limit_ma, cap_ua / 1000);
		break;
	}

	return limit_ma;
}

/*
 * Read the in-effect PDO and RDO, update the cached contract figures, and
 * push the negotiated current limit to the downstream battery-charger
 * regulator (if one is wired in DT).  Called under bm->lock.
 */
static int bm92t_update_contract(struct bm92t_pd *bm)
{
	unsigned int voltage_mv, current_ma, limit_ma;
	u32 pdo, rdo;
	int ret;

	ret = bm92t_read32_lp(bm, BM92T_REG_CURRENT_PDO, &pdo);
	if (ret)
		return ret;
	ret = bm92t_read32_lp(bm, BM92T_REG_CURRENT_RDO, &rdo);
	if (ret)
		return ret;

	if (pdo_type(pdo) != PDO_TYPE_FIXED) {
		dev_dbg(bm->dev,
			"non-Fixed PDO 0x%08x in contract; not advertising\n",
			pdo);
		bm->active_pdo = pdo;
		bm->active_rdo = rdo;
		bm->contract_voltage_mv = 0;
		bm->contract_current_ma = 0;
		bm->input_current_limit_ua = 0;
		return 0;
	}

	voltage_mv = pdo_fixed_voltage(pdo);
	current_ma = pdo_max_current(pdo);
	limit_ma   = bm92t_clamp_input_current(bm, voltage_mv, current_ma);

	bm->active_pdo		   = pdo;
	bm->active_rdo		   = rdo;
	bm->contract_voltage_mv	   = voltage_mv;
	bm->contract_current_ma	   = current_ma;
	bm->input_current_limit_ua = limit_ma * 1000u;

	if (bm->chg_reg) {
		ret = regulator_set_current_limit(bm->chg_reg,
						  0,
						  bm->input_current_limit_ua);
		if (ret)
			dev_warn(bm->dev,
				 "chg-supply rejected %u uA: %d\n",
				 bm->input_current_limit_ua, ret);
	}

	dev_info(bm->dev,
		 "PD contract: %u mV, source %u mA, host limit %u mA\n",
		 voltage_mv, current_ma, limit_ma);

	/*
	 * The PD contract is the framework-defined moment for Discover
	 * Identity.  Fire it once and let the reply walk back to the
	 * partner identity through the VDM RX path.
	 */
	if (!bm->identity_known && bm->partner) {
		int id_ret = bm92t_start_discover_identity(bm);

		if (id_ret)
			dev_dbg(bm->dev, "Discover Identity: %d\n", id_ret);
	}
	return 0;
}

static int bm92t_psy_get_property(struct power_supply *psy,
				  enum power_supply_property prop,
				  union power_supply_propval *val)
{
	struct bm92t_pd *bm = power_supply_get_drvdata(psy);

	guard(mutex)(&bm->lock);

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = bm->attached && !bm->is_source;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = bm->contract_voltage_mv
				? POWER_SUPPLY_USB_TYPE_PD
				: POWER_SUPPLY_USB_TYPE_C;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = bm->contract_voltage_mv * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = bm->contract_current_ma * 1000;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = bm->input_current_limit_ua;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* ---------------------------- Type-C wiring ----------------------------- */

static void bm92t_apply_data_role(struct bm92t_pd *bm, u16 status1)
{
	enum typec_data_role dr;
	enum usb_role usb;

	switch (FIELD_GET(BM92T_S1_DATA_ROLE_MASK, status1)) {
	case BM92T_DR_UFP:
		dr = TYPEC_DEVICE;
		usb = USB_ROLE_DEVICE;
		break;
	case BM92T_DR_DFP:
		dr = TYPEC_HOST;
		usb = USB_ROLE_HOST;
		break;
	default:
		dr = TYPEC_DEVICE;
		usb = USB_ROLE_NONE;
		break;
	}

	typec_set_data_role(bm->port, dr);
	if (bm->role_sw)
		usb_role_switch_set_role(bm->role_sw, usb);
}

static void bm92t_apply_pwr_role(struct bm92t_pd *bm, u16 status1)
{
	enum typec_role pr = (status1 & BM92T_S1_SRC_MODE)
				? TYPEC_SOURCE : TYPEC_SINK;

	bm->is_source = (pr == TYPEC_SOURCE);
	typec_set_pwr_role(bm->port, pr);
}

static int bm92t_attach(struct bm92t_pd *bm, u16 status1)
{
	struct typec_partner_desc desc = {
		.usb_pd = true,
		.accessory = TYPEC_ACCESSORY_NONE,
		.identity = &bm->partner_identity,
	};

	if (bm->partner)
		return 0;

	memset(&bm->partner_identity, 0, sizeof(bm->partner_identity));
	bm->identity_known = false;

	bm->partner = typec_register_partner(bm->port, &desc);
	if (IS_ERR(bm->partner)) {
		int err = PTR_ERR(bm->partner);

		bm->partner = NULL;
		dev_err(bm->dev, "typec_register_partner: %d\n", err);
		return err;
	}

	bm->attached = true;
	bm92t_apply_pwr_role(bm, status1);
	bm92t_apply_data_role(bm, status1);
	return 0;
}

static void bm92t_detach(struct bm92t_pd *bm)
{
	if (!bm->partner)
		return;

	bm92t_unregister_altmodes(bm);

	typec_unregister_partner(bm->partner);
	bm->partner = NULL;

	bm->attached		   = false;
	bm->contract_voltage_mv	   = 0;
	bm->contract_current_ma	   = 0;
	bm->input_current_limit_ua = 0;
	bm->active_pdo		   = 0;
	bm->active_rdo		   = 0;
	bm->identity_known	   = false;
	bm->disc_state		   = BM92T_DISC_IDLE;
	bm->num_svids		   = 0;
	bm->cur_svid_idx	   = 0;
	memset(&bm->partner_identity, 0, sizeof(bm->partner_identity));

	if (bm->chg_reg)
		regulator_set_current_limit(bm->chg_reg, 0, 0);

	typec_set_data_role(bm->port, TYPEC_DEVICE);
	typec_set_pwr_role(bm->port, TYPEC_SINK);
	if (bm->role_sw)
		usb_role_switch_set_role(bm->role_sw, USB_ROLE_NONE);
}

/* ----------------------------- IRQ handler ------------------------------ */

/*
 * One-shot threaded handler.  The line is level-low and the kernel keeps it
 * masked from the moment we enter here until we return IRQ_HANDLED, courtesy
 * of IRQF_ONESHOT.  We must therefore drain any condition that's keeping
 * ALERT_N asserted before returning -- if we leave with the line still low
 * the kernel will simply re-enter the handler.
 */
static irqreturn_t bm92t_irq_thread(int irq, void *data)
{
	struct bm92t_pd *bm = data;
	u16 alert, status1, status2;
	int ret;

	guard(mutex)(&bm->lock);

	ret = bm92t_read16(bm, BM92T_REG_ALERT_STATUS, &alert);
	if (ret) {
		dev_warn_ratelimited(bm->dev, "alert read: %d\n", ret);
		return IRQ_HANDLED;
	}
	ret = bm92t_read16(bm, BM92T_REG_STATUS1, &status1);
	if (ret)
		return IRQ_HANDLED;
	ret = bm92t_read16(bm, BM92T_REG_STATUS2, &status2);
	if (ret)
		return IRQ_HANDLED;

	dev_dbg(bm->dev,
		"alert=0x%04x status1=0x%04x status2=0x%04x\n",
		alert, status1, status2);

	if (alert & BM92T_ALERT_SRC_FAULT) {
		dev_err_ratelimited(bm->dev, "PD source fault\n");
		bm92t_send_cmd(bm, BM92T_CMD_PD_HARD_RST);
	}
	if (alert & BM92T_ALERT_SNK_FAULT)
		dev_err_ratelimited(bm->dev, "PD sink fault\n");

	if (alert & BM92T_ALERT_PLUGPULL) {
		if (status1 & BM92T_S1_INSERTED)
			bm92t_attach(bm, status1);
		else
			bm92t_detach(bm);
	}

	if (alert & BM92T_ALERT_CONTRACT) {
		if (status1 & BM92T_S1_INSERTED) {
			if (!bm->partner)
				bm92t_attach(bm, status1);
			bm92t_apply_pwr_role(bm, status1);
			bm92t_apply_data_role(bm, status1);
			bm92t_update_contract(bm);
			if (bm->psy)
				power_supply_changed(bm->psy);
		}
	}

	if (alert & BM92T_ALERT_DR_SWAP)
		bm92t_apply_data_role(bm, status1);

	if (alert & BM92T_ALERT_VDM_RECEIVED)
		bm92t_handle_vdm_rx(bm);

	return IRQ_HANDLED;
}

/* ------------------------------- Probe ---------------------------------- */

static int bm92t_parse_dt(struct bm92t_pd *bm)
{
	struct device *dev = bm->dev;
	int i, ret;

	bm->pdata.vconn_en_gpio = devm_gpiod_get_optional(dev, "rohm,vconn-en",
							  GPIOD_OUT_LOW);
	if (IS_ERR(bm->pdata.vconn_en_gpio))
		return dev_err_probe(dev, PTR_ERR(bm->pdata.vconn_en_gpio),
				     "vconn-en GPIO\n");

	for (i = 0; i < ARRAY_SIZE(bm92t_pd_voltages_mv); i++) {
		char prop[64];
		u32 v = 0;

		snprintf(prop, sizeof(prop),
			 "rohm,pd-%uv-input-current-limit-microamp",
			 bm92t_pd_voltages_mv[i] / 1000u);
		ret = device_property_read_u32(dev, prop, &v);
		if (ret && ret != -EINVAL)
			return dev_err_probe(dev, ret, "%s\n", prop);
		bm->pdata.pd_input_current_limit_microamp[i] = v;
	}

	return 0;
}

static int bm92t_get_resources(struct bm92t_pd *bm)
{
	struct device *dev = bm->dev;
	struct fwnode_handle *fwnode = dev_fwnode(dev);

	bm->vbus = devm_regulator_get_optional(dev, "vbus");
	if (IS_ERR(bm->vbus)) {
		if (PTR_ERR(bm->vbus) != -ENODEV)
			return dev_err_probe(dev, PTR_ERR(bm->vbus), "vbus\n");
		bm->vbus = NULL;
	}

	bm->vbus_src = devm_regulator_get_optional(dev, "vbus-source");
	if (IS_ERR(bm->vbus_src)) {
		if (PTR_ERR(bm->vbus_src) != -ENODEV)
			return dev_err_probe(dev, PTR_ERR(bm->vbus_src),
					     "vbus-source\n");
		bm->vbus_src = NULL;
	}

	bm->chg_reg = devm_regulator_get_optional(dev, "pd_bat_chg");
	if (IS_ERR(bm->chg_reg)) {
		if (PTR_ERR(bm->chg_reg) != -ENODEV)
			return dev_err_probe(dev, PTR_ERR(bm->chg_reg),
					     "pd-bat-chg\n");
		bm->chg_reg = NULL;
	}

	bm->role_sw = fwnode_usb_role_switch_get(fwnode);
	if (IS_ERR(bm->role_sw))
		return dev_err_probe(dev, PTR_ERR(bm->role_sw),
				     "usb-role-switch\n");

	return 0;
}

/*
 * Downstream-only: the Switch's bq24190 has no DT-side knowledge of which
 * upstream supply to query for the negotiated PD current limit, so declare
 * ourselves as its supplier here.  Not mainline-acceptable; lives only on
 * the nx/v6.16-bm92t-v3 test branch.
 */
static const char * const bm92t_supplied_to[] = {
	"bq24190-charger",
};

static int bm92t_register_psy(struct bm92t_pd *bm)
{
	struct power_supply_config cfg = {
		.drv_data	  = bm,
		.fwnode		  = dev_fwnode(bm->dev),
		.supplied_to	  = (char **)bm92t_supplied_to,
		.num_supplicants  = ARRAY_SIZE(bm92t_supplied_to),
	};

	bm->psy_desc.name		= "bm92txx-pd";
	bm->psy_desc.type		= POWER_SUPPLY_TYPE_USB;
	bm->psy_desc.usb_types		= BM92T_PSY_USB_TYPES;
	bm->psy_desc.properties		= bm92t_psy_props;
	bm->psy_desc.num_properties	= ARRAY_SIZE(bm92t_psy_props);
	bm->psy_desc.get_property	= bm92t_psy_get_property;

	bm->psy = devm_power_supply_register(bm->dev, &bm->psy_desc, &cfg);
	if (IS_ERR(bm->psy))
		return dev_err_probe(bm->dev, PTR_ERR(bm->psy),
				     "power_supply\n");
	return 0;
}

static int bm92t_register_port(struct bm92t_pd *bm)
{
	bm->cap.revision	= USB_TYPEC_REV_1_2;
	bm->cap.pd_revision	= 0x0200;
	bm->cap.type		= TYPEC_PORT_DRP;
	bm->cap.data		= TYPEC_PORT_DRD;
	bm->cap.fwnode		= dev_fwnode(bm->dev);
	bm->cap.prefer_role	= TYPEC_SINK;

	bm->port = typec_register_port(bm->dev, &bm->cap);
	if (IS_ERR(bm->port))
		return dev_err_probe(bm->dev, PTR_ERR(bm->port), "typec port\n");
	return 0;
}

static int bm92t_probe(struct i2c_client *client)
{
	struct bm92t_pd *bm;
	u16 fw_type, fw_rev, status1;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EOPNOTSUPP;

	bm = devm_kzalloc(&client->dev, sizeof(*bm), GFP_KERNEL);
	if (!bm)
		return -ENOMEM;

	bm->client = client;
	bm->dev    = &client->dev;
	mutex_init(&bm->lock);
	i2c_set_clientdata(client, bm);

	ret = bm92t_parse_dt(bm);
	if (ret)
		return ret;

	ret = bm92t_get_resources(bm);
	if (ret)
		return ret;

	ret = bm92t_read16(bm, BM92T_REG_FW_TYPE, &fw_type);
	if (ret)
		return dev_err_probe(bm->dev, ret, "fw_type\n");
	ret = bm92t_read16(bm, BM92T_REG_FW_REVISION, &fw_rev);
	if (ret)
		return dev_err_probe(bm->dev, ret, "fw_rev\n");
	dev_info(bm->dev, "Rohm BM92T fw_type=0x%04x fw_rev=0x%04x\n",
		 fw_type, fw_rev);

	ret = bm92t_register_psy(bm);
	if (ret)
		return ret;

	ret = bm92t_register_port(bm);
	if (ret)
		return ret;

	ret = bm92t_read16(bm, BM92T_REG_STATUS1, &status1);
	if (ret == 0 && (status1 & BM92T_S1_INSERTED)) {
		guard(mutex)(&bm->lock);
		bm92t_attach(bm, status1);
		bm92t_update_contract(bm);
	}

	ret = devm_request_threaded_irq(bm->dev, client->irq, NULL,
					bm92t_irq_thread,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					dev_name(bm->dev), bm);
	if (ret) {
		typec_unregister_port(bm->port);
		return dev_err_probe(bm->dev, ret, "request_threaded_irq\n");
	}

	return 0;
}

static void bm92t_remove(struct i2c_client *client)
{
	struct bm92t_pd *bm = i2c_get_clientdata(client);

	/*
	 * The IRQ is devm-managed and will be released before the typec port
	 * is torn down by devm_*, but partner registration is manual: undo it
	 * here so userspace sees a clean detach.
	 */
	scoped_guard(mutex, &bm->lock)
		bm92t_detach(bm);

	if (bm->port)
		typec_unregister_port(bm->port);

	if (bm->role_sw)
		usb_role_switch_put(bm->role_sw);
}

static const struct of_device_id bm92t_of_match[] = {
	{ .compatible = "rohm,bm92t" },
	{ .compatible = "rohm,bm92t36" },
	{ .compatible = "rohm,bm92t40" },
	{ }
};
MODULE_DEVICE_TABLE(of, bm92t_of_match);

static const struct i2c_device_id bm92t_i2c_id[] = {
	{ "bm92t36" },
	{ "bm92t40" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bm92t_i2c_id);

static struct i2c_driver bm92t_driver = {
	.driver = {
		.name		= "bm92txx",
		.of_match_table	= bm92t_of_match,
	},
	.probe		= bm92t_probe,
	.remove		= bm92t_remove,
	.id_table	= bm92t_i2c_id,
};
module_i2c_driver(bm92t_driver);

MODULE_AUTHOR("CTCaer <ctcaer@gmail.com>");
MODULE_AUTHOR("Alexandre Hamamdjian <azkali.limited@gmail.com>");
MODULE_DESCRIPTION("Rohm BM92TXX USB Type-C/PD controller driver");
MODULE_LICENSE("GPL");
