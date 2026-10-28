# Safe hardware recovery / experiment policy

1. Preserve the working Linux 4.9 L4T `Rescue` Hekate profile. Never modify it as part of an experimental boot slot.
2. Do not alter the default / current boot selection when adding Enigma versions; use separate numbered profile entries.
3. Back up Hekate configs and boot artifacts; verify every image, DTB and package with SHA256 before installation.
4. Use a physical/manual boot with a clear fallback path. Confirm battery/charging and that the USB/serial diagnostic channel is available.
5. Treat lost USB/SSH as **unknown device state** until directly observed. Do not assume panic, and do not auto-reboot the console remotely.
6. Isolate each candidate: R20 baseline vs one R21 PAD change; do not combine multiple driver experiments in one variant.
7. During troubleshooting, perform only read-only probes unless a volunteer has explicitly opted into write operations on their own hardware.

No guarantees of safety are made. Incorrect boot files can make the device unbootable without recovery tools. The project does not provide console keys or proprietary Nintendo firmware.
