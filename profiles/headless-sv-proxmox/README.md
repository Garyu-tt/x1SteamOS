# x1SteamOS Headless / sv-proxmox (experimental)

This is a **separate headless host profile** for Nintendo Switch Lite / Tegra210B01. It preserves the known-good Linux 4.9.140-l4t hardware enablement (PCIe BCM4356 Wi-Fi, Tegra GPU/display/input) on the **host**. The guest receives **virtual** CPU, storage, NIC, USB input and a 2D display.

> Status: branch + prototype specification, NOT installed or validated on Switch. No Hekate/boot files or running system are modified.

## Device ownership

| Resource | Owner | Guest interface |
| --- | --- | --- |
| BCM4356 PCIe Wi-Fi / brcmfmac | Host | QEMU SLIRP/NAT `virtio-net` |
| Tegra nvgpu / DRM / physical panel | Host | `virtio-gpu` -> loopback VNC -> optional host kiosk viewer |
| Joy-Cons, touchscreen, battery, power | Host | Optional virtual input, later mediated events |
| SD/eMMC and boot partition | Host | Separate file-backed guest disk |
| KVM/EL2 | Host prerequisite | `/dev/kvm` if available; otherwise explicit QEMU TCG experiment only |

**No VFIO/PCI passthrough of the Wi-Fi NIC.** Tegra display/GPU is not a conventional PCI endpoint. A guest-local `virtio-gpu` is not GPU passthrough or 3D acceleration.

## The important limitation

Proxmox VE 9.2 ARM64 is officially provided for supported newer Arm server platforms, not Tegra X1+ or Linux 4.9. Proxmox VE packages expect their own supported Debian / kernel / architecture stack. This branch therefore starts with a **Proxmox-style independently managed QEMU instance**, called `sv-proxmox`; it does not imply official Proxmox is installed. The feasibility of using actual PVE components must be validated against real Switch features and userspace ABI before attempting installation.

## Plan

1. Keep Hekate's known-good L4T 4.9 rescue entry **untouched**.
2. Read-only audit of /dev/kvm, EL2, cgroups, QEMU/UEFI and display nodes. See `profiles/headless-sv-proxmox/host-audit.sh`.
3. Prepare a **separate boot/rootfs slot** based on working L4T/Arch userspace. Run systemd multi-user.target without Plasma or a display manager, but keep all kernel/device packages. Do not deactivate the active rootfs or Wi-Fi.
4. Install the opt-in `sv-proxmox` systemd VM runner on the separate slot. It uses an independent ARM64 guest disk and localhost-only VNC. Do **not** auto-enable the VM service before testing.
5. For the Switch panel, start only a tiny viewer session when needed (e.g. minimal Xorg + VNC viewer), not a full desktop. A direct DRM/KMS or fbdev viewer is a future alternative, not yet proven on L4T.
6. If KVM is absent, don't silently fall back to slow TCG. Explicitly evaluate whether rebuilding/patching KVM is necessary, rather than claiming zero recompilation.
7. Validate SSH survivability, RAM consumption, input, GPU rendering and guest shutdown before switching the primary boot slot.

## Non-goals

- No direct PCIe passthrough to guest; no `vfio-pci` binding of BCM4356.
- No modification of firmware, Hekate default profile, working L4T rescue, partition table or NVIDIA binaries.
- No assumption that a headless host means no display code: the host still owns panel and display drivers.
- No claim of an installed or booted guest until device-side validation has succeeded.

See the standalone `sv-proxmox-switch-poc` package for the opt-in VM runner. This profile intentionally lives separately from the Enigma 6.18 kernel work.
