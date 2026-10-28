# Contribution tasks / work queue

The initial owner cannot provide continuous physical testing. Prefer offline diagnostics and focused PRs; hardware testing should be opt-in and scheduled by volunteers with recovery.

## P0: PCIe link / BCM4356

- [ ] Review R20 LTSSM snapshots against working L4T 4.9 and upstream `pci-tegra.c` behavior.
- [ ] Test the **single-variable R21 PAD/REFCLK hypothesis** on real hardware in an isolated slot; capture link status and evidence.
- [ ] Independently compare CLKREQ pin direction, RP ECTL TX amplitude and missing `mselect` clock. No speculative merged patch until individually tested.
- [ ] When PCIe enumeration works, verify BCM4356 `brcmfmac` and Wi-Fi network connectivity.

## P0: GM20B Nouveau/Mesa rendering

- [ ] Reproduce and triage `glClear` SIGSEGV in Gallium with symbols / userspace stack trace.
- [ ] Validate actual GPU-rendered pixel readback (not merely EGL renderer string).
- [ ] Compare DRM render node selection, GBM device, mesa environment and ioctl trace.

## P1: Desktop and peripherals

- [ ] Stable KWin / Wayland session and hardware compositing.
- [ ] Touchscreen and rotated input mapping, controllers, audio, USB, storage, battery/charging, suspend and clocks.
- [ ] Optional performance / thermal / power measurement; Vulkan/NVK investigation after working GL path.

## P1: Making collaboration easy

- [ ] Add reproducible Hekate packaging and an explicit safety/rollback contract.
- [ ] Build a hardware test matrix with kernel config, hash, revision, exact device and measured result.
- [ ] Add GitHub Actions for patch applicability and cross-builds, without real-hardware assumptions.
- [ ] Attach redacted logs and minimal GPU reproducer with controlled provenance.
- [ ] Evaluate transition from patch-stack source distribution to a maintained kernel fork with commits.

Contributors: open an Issue or PR for one task, link evidence, and mark source-only vs compile-only vs tested-on-device.
