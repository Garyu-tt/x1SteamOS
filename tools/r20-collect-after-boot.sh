#!/bin/sh
# x1SteamOS R20 manual diagnostics. Read only, prints to stdout.
# Safe on 4.9 too; deliberately makes no mount/reboot/kernel parameter changes.
set -u
echo "===== X1 R20 LIVE COLLECT ====="
date -u 2>/dev/null || true
uname -a
echo "=== DEVICE ==="
tr '\000' '\n' </proc/device-tree/model 2>/dev/null || true
echo "=== CMDLINE ==="
cat /proc/cmdline
echo "=== R20 SNAPSHOTS ==="
dmesg 2>/dev/null | grep -E 'x1-r20-ltssm|x1-r13-link|x1-r17-presence|x1-r08-wlan' | tail -120 || true
echo "=== PCIe PHY, WIFI ==="
dmesg 2>/dev/null | grep -Ei 'tegra-pcie|pcie|uphy|cal_done|tegra-xusb-padctl|brcmfmac|bcm4356|link [0-9]+ down' | tail -180 || true
echo "=== PCI DEVICES ==="
if command -v lspci >/dev/null 2>&1; then lspci -nn; fi
echo "=== SYSFS PCI ==="
for d in /sys/bus/pci/devices/*; do
 [ -d "$d" ] || continue
 echo "BDF $d"
 for n in vendor device current_link_speed current_link_width; do
   [ -r "$d/$n" ] && { printf "%s=" "$n"; cat "$d/$n"; }
 done
done
echo "=== NETWORK ==="
ip -br addr 2>/dev/null || true
echo "=== USB ==="
ip -br addr show usb0 2>/dev/null || true
for f in /sys/class/udc/*/state; do
 [ -f "$f" ] && { printf '%s=' "$f"; cat "$f"; }
done
echo "=== GPU ==="
ls -l /dev/dri/ 2>/dev/null || true
dmesg 2>/dev/null | grep -Ei 'nouveau|nvgpu|57000000.gpu|drm' | tail -70 || true
echo "=== EGL / RENDERER ==="
if command -v eglinfo >/dev/null 2>&1; then
  timeout 8 eglinfo -B 2>&1 | head -55 || true
fi
echo "=== POWER ==="
for f in /sys/class/power_supply/*/capacity /sys/class/power_supply/*/status; do
 [ -r "$f" ] && { printf '%s=' "$f"; cat "$f"; }
done
echo "=== UPTIME ==="
cat /proc/uptime
echo "===== X1 R20 END ====="
