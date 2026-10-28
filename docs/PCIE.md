# PCIe / Wi-Fi investigation: Tegra210B01 Mariko

## Symptom

Linux 6.18 R20 initializes PCIe-related structures and UPHY calibration, but the root-port link is down, so Broadcom BCM4356 `14e4:43ec` does not enumerate. Firmware loading for `brcmfmac` is **not** the immediate blocker: there is no PCI endpoint to bind to.

## Known-good baseline (L4T 4.9.140)

- NVIDIA Tegra PCI root device `10de:0faf`, endpoint `14e4:43ec` on `01:00.0`.
- Link at **2.5 GT/s, x1**, `brcmfmac` functional.
- GPIO and regulator baseline: `vdd-pex 1.0V`, `vdd-1v8 1.8V`, `vdd-3v3 3.3V` enabled during a successful observation.
- Working L4T device-tree PCIe pad programming has `prod_c_pad` offset `0xc8`, value `0xb0b880b8`; downstream L4T programs dynamic pinctrl CLKREQ states and root port TX driver values at `0xe80` and `0xea0`.

## R20 6.18 observation

- `CAL_DONE CTL2=0x7`, `WL_REG_ON` asserted, then root-port 1 link remains down.
- `XP=0x19800000`, `LNK=0x10110000`, `LNK2=0x00010001`; DL_UP and DL_LINK_ACTIVE not observed.
- LTSSM trace snapshots: initial `dbg=00000000 ctl=00000001 status=001c8000`; retry `dbg=00000001 status=00090830`; subsequent `status=00090801`.
- Decoder reports trace data interpreted as Detect.Active on valid reads. **TRACE_STATUS is a trace-buffer interface, not proof that the live FSM never entered other states**. Avoid over-interpreting one snapshot.

## Root-cause candidates — unverified

1. **R21 PADS_REFCLK_CFG0:** 6.18 sets `0x90b890b8`; live 4.9 DT describes `0xb0b880b8`. The R21 patch changes this *single* value. It compiles but has not yet been boot-tested.
2. **Root port TX amplitude:** L4T `prod_c_rp` masks lower 6 bits at `0xe80`/`0xea0` to `0x27`, but mainline does not perform equivalent ECTL setup in the tested configuration. Need register proof before patching.
3. **CLKREQ pinctrl direction:** L4T dynamically switches named CLKREQ states; current 6.18 setup does not reproduce the same sequence.
4. **Missing `mselect` clock reference** versus L4T: may be relevant, but no causal evidence yet.

## Next meaningful tests

- Record PCIe registers and rails at equivalent times under working 4.9 and R20/one isolated candidate.
- Validate R21 with a separate Hekate entry after a safe manual boot; measure port link, DL_UP, endpoint enumeration.
- Only if link establishes, debug BCM4356 firmware, driver binding and WLAN userspace.
- Avoid repeating R08–R17 variations without new measured evidence.
