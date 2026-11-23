# November 2026 upstream Mariko kernel patch candidates

Target: Enigma R20 Linux-NX 6.18, Nintendo Switch Lite / Tegra210B01. **No source changes applied or hardware tests performed.**

## P0: Nouveau GM20B fuse clock lifetime

Upstream patch: [Sasha Levin, 2026-10-13](https://lists.openwall.net/linux-kernel/2026/10/13/2069), `drm/nouveau/tegra: Keep the fuse clock enabled while the GPU is powered`.

Changes two files:
- `drivers/gpu/drm/nouveau/include/nvkm/core/tegra.h` (optional `clk_fuse`)
- `drivers/gpu/drm/nouveau/nvkm/engine/device/tegra.c` (enable before GPU rail, disable after power down; error unwind)

Tested upstream on Jetson TX1 and Linux 6.18; reported GPU initialization failures 27/109 without vs 11/291 with. **Not validated on Mariko.** It addresses probe/clock instability, not necessarily the Mesa `glClear` SIGSEGV.

**Gate:** check whether Enigma R20 already carries equivalent change; verify Mariko DT clock named `fuse` and relevant clock provider; run `git apply --check` on exact reconstructed kernel and compile. Log GPU probe and EGL draw results before and after. Preserve safe boot entry.

## P0: Tegra210B01 dedicated clock controller

Upstream series: [Aaron Kling, 2026-10-18](https://lore-kernel.gnuweeb.org/linux-clk/OlYyksyGQYOHR0TeOntmWQ%40nvidia.com/T/), `[PATCH 0/2] clk: tegra: Add Tegra210B01 support`. Adds `drivers/clk/tegra/clk-tegra210b01.c` (~3524 lines) plus binding and common clock framework changes. Based on downstream NVIDIA 5.10 and explicitly targets **T210B01**, not original T210.

**Gate:** inspect existing Enigma clock driver and Kconfig/DT compatibles first; determine which clock IDs and rates differ; compile as a separate experimental series. Audit USB and PCIe clocks, REFCLK, GPU clocks and power domains. Do not replace working clock tree wholesale without boot diagnostics. Check mailing-list review status and newer revisions.

## P1: Tegra210B01 PMC / powergates

Upstream [PMC v3 patch, 2026-07-23](https://lists.openwall.net/linux-kernel/2026/07/23/300) by Alexandre Hamamdjian. T210B01 PMC differs from T210 in SATA/VENC/VE2 powergates. Check against existing R20 PMC implementation; avoid incorrect powergate assumptions.

## P1: PCIe / Wi-Fi baseline

Enigma [PCIe investigation](PCIE.md) establishes:
- R20 CAL_DONE but no endpoint link / BCM4356 enumeration.
- R21 changes PAD REFCLK from `0x90b890b8` to downstream `0xb0b880b8`, **not boot-tested**.
- L4T uses TX driver settings `0xe80`/`0xea0` and dynamic CLKREQ pinctrl states.
- Working L4T PCIe link 2.5 GT/s x1.

**Gate:** run R21 in isolated Hekate slot, then compare exact register dumps and CLKREQ sequencing to working L4T. A generic Kconfig fix alone cannot resolve observed LTSSM failure.

## P2: Switchroot mainline reference

[Patch inventory](https://notdouble.com/p/alarm-tegra-x1/file/reference/switchroot-mainline/README.md) includes XUDC coherent DMA, DFLL, display and thermal fixes. Source DT is Erista; do not replace Mariko board DTB. Check whether XUDC fix is already present in Linux-NX 6.18.

## Verification

For every candidate record upstream commit SHA, version, license, already-present detection, compile status, and hardware evidence. Keep rescue L4T 4.9 intact; no eMMC writes, no main branch merge until tested.
