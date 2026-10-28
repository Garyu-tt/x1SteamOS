# x1SteamOS — Enigma kernel

Community development of modern Linux for Nintendo Switch, focusing on **Switch Lite / Mariko (Tegra X1+, T210B01; GM20B Maxwell)**.

**Status (2026-10-28): EXPERIMENTAL — not a daily-driver release.** Linux 6.18 has booted on real hardware (R20); USB gadget networking and SSH are confirmed. PCIe/Wi-Fi link training and Nouveau/Mesa hardware rendering are **not working**. R21 PAD/REFCLK is **built but not boot-tested**. Keep a working L4T 4.9 rescue boot entry.

**Enigma** is the kernel project; **x1SteamOS** is the intended Linux distribution. Contributors are welcome: see [CONTRIBUTING.md](CONTRIBUTING.md), [status](docs/STATUS.md), and [open technical tasks](docs/TASKS.md).

## Source and reproducibility

The Git-tracked source distribution is **upstream Linux-NX pinned to an exact commit + our complete source patches**, rather than duplicate copies of the upstream kernel or multi-gigabyte build outputs.

- Upstream: https://gitlab.com/l4t-community/kernel/linux-nx
- Base revision: `80f1e68e163ac77d688f0244b03cf05f791b4781`
- Mainline baseline for this project: Linux 6.18-based Linux-NX
- **R20, confirmed booting:** apply `patches/enigma-r20-base.patch`, then `patches/enigma-r20-ltssm.patch`.
- **R21, experimental:** apply `patches/experimental/r21-pad-refclk.patch` *after* R20.

Run `./scripts/fetch-enigma.sh r20` or `./scripts/fetch-enigma.sh r21` to reconstruct the full patched kernel sources in `work/linux-enigma`. The patches were validated as applying cleanly in order to the exact upstream base commit. Build configuration and instructions are in `configs/` and `docs/BUILD.md`; rebuilding a functional boot image still requires the appropriate ARM64 toolchain and Switch-specific Hekate/boot packaging. **Do not assume this repository is a turnkey bootable release.**

## Confirmed and unconfirmed

| Component | Status |
|---|---|
| Hekate -> Linux 6.18 R20 kernel | **Booted on physical Switch Lite** |
| ARM64 userspace / USB gadget / SSH | **Working on R20** |
| PCIe endpoint / Broadcom BCM4356 Wi-Fi | **Blocked: PCIe link down** |
| Nouveau GPU DRM initialization / EGL renderer string | **Detected, not proof of working 3D** |
| GPU OpenGL command submission | **Fails**: Mesa Gallium SIGSEGV on `glClear` |
| CPU `llvmpipe` offscreen rendering | **Passes** |
| KWin Wayland session | **Not stable/validated on R20** |
| R21 REFCLK hypothesis | **Built offline, not yet tested on Switch** |
| Working L4T 4.9 rescue profile | **Preserved** |

See [STATUS](docs/STATUS.md), [PCIe findings](docs/PCIE.md), [GPU findings](docs/GPU.md), and [test requirements](docs/TESTING.md).

## Participation

Work is split into independent issues / patches. You do **not** need to own the entire project to contribute. Reproduction logs, focused improvements, hardware comparisons, kernel-driver review, and documentation are especially welcome.

This is an independent community project, **not affiliated with Nintendo or NVIDIA**. Upstream Linux sources remain under their respective licenses; preserve SPDX headers and license notices. No Nintendo proprietary firmware, keys, prodinfo, or copyrighted boot binaries are provided here.

## Critical recovery rule

**Never replace the default Hekate entry or the known-good L4T rescue setup during experimentation.** Only create separately named test slots, verify hashes, use physical/manual boot with a recovery route, and log actual hardware outcomes. See [RECOVERY.md](docs/RECOVERY.md).
