# Verified upstream patch revisions (2026-11-24)

Target: Enigma R20 / Mariko. Research and review only; no build or hardware validation.

## Nouveau fuse clock: prefer v2, not v1

- [v2 cover letter, 6 patches](https://lists.openwall.net/linux-kernel/2026/10/16/2362) is newer than the previously documented October 13 v1.
- [v2 patch 5/6](https://lists.openwall.net/linux-kernel/2026/10/16/2357) adds `TEGRA210_CLK_FUSE` to `tegra210.dtsi` GPU clock list, after GPU/pwr/ref.
- [v2 patch 6/6](https://lkml.rescloud.iu.edu/2610.2/01934.html) keeps optional `fuse` clock enabled while GPU is powered and improves error cleanup.
- [v2 patch 1/6](https://lists.openwall.net/linux-kernel/2026/10/16/2369) updates DT bindings.
- Important: upstream testing was on **Jetson TX1**, not Mariko. No guarantee that Mariko DT clock provider exposes an equivalent clock or that this fixes `glClear`.

**Minimal import candidate**: the v2 Tegra210 GPU DT clock declaration plus v2 Nouveau lifetime patch, after checking the actual Enigma 6.18 source for equivalent changes and Mariko clock mapping. Do not cherry-pick the entire 6-patch cross-SoC series.

## Clock controller: Oct 18 v1

- [Aaron Kling v1 0/2](https://lists.openwall.net/linux-kernel/2026/10/18/1233) introduces independent Tegra210B01 clock driver (~3524 lines), updated USB handling and binding changes.
- Source baseline is NVIDIA downstream 5.10, **not** a ready-made patch for Enigma 6.18.
- Audit `tegra210b01` clock/reset compatible, existing clock driver, GPU fuse clock ID, PCIe REFCLK and USB clock consumers before porting.

## PMC: v4 supersedes v3

- [PMC v4 0/2](https://lkml.iu.edu/hypermail/linux/kernel/2607.3/06809.html) changes Mariko powergate map and adds VENC; prefer v4 over previously noted v3.
- Do not merge PMC or clock-controller changes without separate compile and boot gates.

## Immediate implementation gates

1. Reconstruct pinned Linux-NX tree and run `git grep` for `clk_fuse`, `TEGRA210_CLK_FUSE`, `tegra210b01`, GPU node clock names and relevant PMC compatible.
2. Apply only missing v2 Nouveau/DT changes in disposable worktree; `git apply --check`, compile Image/DTB, and test GPU initialization and hardware `glClear`.
3. For PCIe, first boot isolated R21 REFCLK test image; compare LTSSM and TX/CLKREQ programming against known-good L4T.
4. Preserve rescue boot, no eMMC writes, no automatic merge to main.

**State:** no actual kernel source patch applied, compiled or hardware-tested in this change.
