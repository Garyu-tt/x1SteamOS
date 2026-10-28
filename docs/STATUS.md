# Enigma status — 2026-10-28

## Hardware and base

- Tested hardware: **Nintendo Switch Lite (Mariko, Tegra210B01 / X1+, GM20B Maxwell GPU)**.
- Kernel base: Linux-NX 6.18, git commit `80f1e68e163ac77d688f0244b03cf05f791b4781`.
- Loader: Hekate with isolated revision profiles; L4T 4.9 rescue remains essential.
- Source delivery: upstream commit + R20 patches; R21 is opt-in only.

## Verified R20 hardware observation (2026-10-28)

- Booted `6.18.0-g80f1e68e163a-dirty` from R20 experimental entry with `swr_dir=/switchroot/x1-618-ltssm-r20/`.
- USB gadget `usb0` with SSH available; onboard Wi-Fi `wlan0` not present.
- UPHY calibration reports `CAL_DONE CTL2=0x00000007`; Wi-Fi GPIO WL_REG_ON asserted, but PCIe root port 1 never reaches a usable link (DL_UP / DL_LINK_ACTIVE not asserted at logged observations), and no BCM4356 enumerates.
- Nouveau GPU / DRM devices initialize. Mesa 26.2.3 `eglinfo -B` shows `tegra`, GL 4.3 / GLES 3.2, but a 16x16 offscreen ES2 test fails with SIGSEGV in `libgallium-26.2.3-arch1.1.so` on `glClear`. Under `LIBGL_ALWAYS_SOFTWARE=1`, the same test returns valid pixels (llvmpipe).
- KWin Wayland startup did not establish a stable working compositor in this R20 test.
- No successful NVK/Vulkan or accelerated graphics demonstration on 6.18 yet.

## Verified earlier baseline

- Working L4T **4.9.140-l4t** includes a Broadcom BCM4356 PCIe Wi-Fi endpoint at `01:00.0`, PCIe 2.5 GT/s x1, functioning `brcmfmac` and network access.
- L4T GPU driver `nvgpu` initializes; do **not** infer that all L4T distributions are CPU-only renderers. The R20 Mesa crash is specific to this experimental stack.
- Earlier R16/R17 trials also had PCIe link-down, despite multiple resets/UPHY/order variants. Treat R08–R17 approaches as documented unsuccessful experiments, not known fixes.

## R21 candidate

- R21 isolates a single `PADS_REFCLK_CFG0` candidate constant `0x90b890b8 -> 0xb0b880b8`, based on a working L4T pad-setting comparison.
- R21 build `Image`, DTB, uImage, DTIMG passed offline packaging and hash verification.
- **R21 is NOT installed / NOT boot-tested** according to the most recent verified packaging report. No assertion of PCIe improvement.
- It is an experimental follow-up to R20; manual recovery-aware boot validation remains necessary.

## Source-of-truth caveat

This document describes the latest **recorded hardware evidence**, not current live console status. The Switch was not reachable for a new live interrogation during this public-repository preparation. Preserve test dates and evidence levels when reporting updates.
