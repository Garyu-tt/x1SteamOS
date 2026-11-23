# Mainline Switch integration audit (2026-11-24)

Target: **Switch Lite / Mariko / T210B01**. Baseline: **Enigma R20 (Linux-NX 6.18)**. This is a source-review workstream, **not a hardware validation or claim of GPU support**.

## Verified baseline

- R20 boots physical Mariko; USB gadget and SSH work.
- PCIe / BCM4356 Wi-Fi link training is blocked.
- Nouveau is detected but Gallium crashes on `glClear`; llvmpipe works.
- R21 PAD/REFCLK patch has not been boot-tested.
- Preserve known-good L4T 4.9 rescue entry and do not write eMMC.

Sources: [upstream README](../README.md), [GPU](GPU.md), [PCIe](PCIE.md).

## Candidate sources and transplant policy

| Source | Potential benefit | Evidence / limitation | Next gate |
|---|---|---|---|
| [mojyack/gentoo-note misc/nx](https://github.com/mojyack/gentoo-note/tree/main/misc/nx) | Modern Tegra210 Nouveau/Mesa/NVK stack; benchmark documents Linux 7.3-next and ~220 Switch patches | Erista (T210), **not** proven Mariko (T210B01); userspace and kernel changes may be coupled | Inventory exact patch series, licenses, changed paths, kernel baseline, GPU firmware requirements; isolate portable changes |
| [JonasNowak/chimera-switch-6.18](https://github.com/JonasNowak/chimera-switch-6.18) | Reproducible rootfs/initramfs/SD image packaging, CI and upstream tracker | Uses Enigma R20; README explicitly says Erista physical boot **UNTESTED**; no demonstrated Mariko PCIe/GPU fix | Reuse independent build/verification concepts, **not** hardware success claims |
| [l4t-community/kernel/linux-nx](https://gitlab.com/l4t-community/kernel/linux-nx) | Canonical kernel base, bootchain compatibility | Existing Enigma base; rebases can break ABI or boot | Pin SHA and run patch apply/build/boot gates |
| [alarm-tegra-x1](https://notdouble.com/p/alarm-tegra-x1/file/BRINGUP.md) | Bring-up diagnostics, kernel configuration validation | Erista-focused; independently verify any suggested symbols in the **target** Kconfig | Check `CONFIG_PCI_TEGRA` against exact source; do not blindly copy configs |

## First integration gates

1. **Source provenance:** record upstream commit, patch IDs, SPDX/license, exact kernel baseline, and whether the change targets T210 or T210B01. Never import a whole downstream tree as a replacement.
2. **Static compatibility:** `git apply --check` in a disposable worktree, inspect DT compatible strings, clocks, regulators, IOMMU, GPU firmware, PCIe PHY and board-specific pinmux.
3. **Build:** compile Image, DTBs, modules; run Kconfig assertions against the actual generated `.config`. Distinguish built-in from module drivers required before root mount.
4. **Safe boot:** separate Hekate test profile and removable microSD; keep L4T 4.9 rescue untouched. Capture UART/earlycon, `dmesg`, `uname -a`, loaded modules and `/sys/kernel/debug/dri` where available.
5. **GPU acceptance:** verify renderer is **not llvmpipe**, then run EGL context, `glClear`, triangle draw, swap buffers, repeated rendering and a Vulkan smoke test; save full logs. A detected Nouveau device alone is not a pass.
6. **PCIe acceptance:** log link state and LTSSM transitions; only mark Wi-Fi working after stable PCIe enumeration, firmware load, scan, association and sustained traffic.
7. **Rollback:** no automatic promotion to main; compare boot logs and retain known-good images and checksums.

## Specific Chimera review

`scripts/fetch-enigma.sh` clones upstream x1SteamOS at pinned SHA, verifies bundled Enigma patch hashes and applies them to Linux-NX. It does **not** establish a new fix for Mariko GPU or PCIe. It also contains `git reset --hard` and `git clean -fd` against its kernel worktree: reuse only inside an explicitly disposable, isolated checkout.

Chimera's `VERSIONS` pins upstream `80f1e68e163ac77d688f0244b03cf05f791b4781`, x1SteamOS `743408bbcbd26f091ad0772fd862d2079e2efa3e`, and Enigma `r20`. The upstream tracker can attempt three-way rebases; these require manual validation before adopting kernel changes.

## Current outcome

**Research only.** No external kernel patch imported, no cross-compile performed, no physical Switch test performed. Priority next: inspect the actual mojyack patch inventory and port a **minimal GPU dependency set** into a dedicated experimental branch, not directly into main.
