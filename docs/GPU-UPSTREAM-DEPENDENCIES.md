# GPU bring-up evidence and dependency checklist (2026-11-24)

**Target:** Mariko T210B01, Enigma R20 on Linux-NX 6.18. **Status: investigation only.** No hardware test or imported patch.

## Upstream performance evidence

[mojyack's October 26 benchmark](https://github.com/mojyack/gentoo-note/blob/main/misc/nx/docs/benchmark-20261026.md) compares **the same Erista HAC-001** on Linux 7.3-next + 220 Tegra210/Switch patches, Mesa 27-devel (Nouveau/NVK), and Switchroot L4T 4.9.140. Published figures include:

| Benchmark | Mainline | L4T | Interpretation |
|---|---:|---:|---|
| vkmark fullscreen | 2566 | 1592 | 1.61x on **Erista only** |
| vkmark offscreen | 5372 | 4488 | 1.20x |
| glmark2 fullscreen | 1744 | 1694 | 1.03x |
| Xonotic 720p normal FPS | 149.1 | 140.2 | 1.06x |

This is a **GPU execution result** (unlike merely detecting a Nouveau device), but does **not** establish Mariko compatibility or Linux 6.18 compatibility. Benchmark text has a suspicious duplicated GPU clock line (1785 MHz and 921.6 MHz), so do not extrapolate clock or power assumptions.

## Specific dependencies discovered

- [mojyack firmware installer](https://github.com/mojyack/gentoo-note/blob/main/misc/nx/static/usr/lib/firmware/install.sh) copies `nvidia/gm20b` firmware and `nvidia/tegra210/xusb.bin` from a supplied firmware source, plus Broadcom BCM4356 and RTL8153 firmware. This is an **installation recipe, not a license to redistribute proprietary blobs**.
- [Installation notes](https://github.com/mojyack/gentoo-note/blob/main/misc/nx/docs/install.md) separately specify NVDEC/VIC firmware, Bluetooth `BCM4356A3.hcd`, and `brcmfmac4356-pcie.nintendo,switch.txt` board configuration.
- [Boot config](https://github.com/mojyack/gentoo-note/blob/main/misc/nx/static/boot/extlinux/extlinux.conf) uses `tegra210-nintendo-switch.dtb`; **do not** substitute this Erista DTB for Mariko's `tegra210b01-vali.dtb`.
- [Audio UCM](https://github.com/mojyack/gentoo-note/blob/main/misc/nx/static/usr/share/alsa/ucm2/Tegra/rt5640/Nintendo-Switch.conf) targets HAC-001 codec routing; not Mariko drop-in.
- mojyack's `misc/nx/patches/switch-atf` includes CL-DVFS reset, SC7 resume and Cortex-A57 erratum workarounds. These affect firmware/boot sequencing, **not** standalone GPU fixes. Audit separately; boot firmware is safety-critical.

## What to test next

1. Locate the **actual 220 kernel patches**, upstream repository and exact revision. The public benchmark describes them but does not itself supply an importable patch manifest. Do not claim they are already vendored.
2. Compare Nouveau GM20B firmware loading, DRM node selection, host1x, memory/IOMMU, clocks, regulators, device-tree and Mesa versions between Erista mainline and Mariko Enigma.
3. Preserve `tegra210b01-vali.dtb` and Mariko-specific PCIe/clock settings; stage only independently understood kernel patches.
4. In isolated builds, check for missing `CONFIG_DRM_NOUVEAU`, `CONFIG_DRM_TEGRA`, `CONFIG_TEGRA_HOST1X`, `CONFIG_IOMMU_SUPPORT` and firmware-loading paths **against actual target Kconfig** (symbol availability may vary).
5. Record `dmesg`, firmware requests, `/dev/dri`, EGL renderer, `glClear`, triangle, swap and `vulkaninfo`; never count llvmpipe as GPU acceleration.
6. Keep PCIe investigation separate: correct kernel Kconfig symbol and a loaded driver do **not** imply working Mariko LTSSM or Wi-Fi.

## Chimera source-level check

[Chimera's `scripts/fetch-enigma.sh`](https://github.com/JonasNowak/chimera-switch-6.18/blob/main/scripts/fetch-enigma.sh) applies **our own R20/R21 patches** on the pinned Linux-NX tree. Its rootfs, CI and updater are worth adapting, but no new Mariko GPU/PCIe fix is demonstrated. Its `reset --hard` / `clean -fd` path must stay in a disposable worktree.

**No merge recommendation until the actual kernel patch series has been identified and tested.**
