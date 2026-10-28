# Enigma R20 — selected kernel log excerpt (2026-10-28)

This is a **small, manually selected** technical excerpt from the recorded R20 physical-boot dmesg. It omits build usernames, hostnames, networking and unrelated device messages. No additional live test was performed for repository publication.

```text
[    1.205937] tegra-xusb-padctl 7009f000.padctl: x1-r09-uphy: CAL_DONE CTL2=0x00000007
[    1.238015] tegra-pcie 1003000.pcie: x1-r08-wlan: WL_REG_ON asserted, waiting for POR
[    1.394090] tegra-pcie 1003000.pcie: x1-r20-ltssm initial port=1 dbg=00000000 trace_ctl=00000001 trace_status=001c8000
[    1.796531] tegra-pcie 1003000.pcie: x1-r20-ltssm retry port=1 dbg=00000001 trace_ctl=00000001 trace_status=00090830
[    2.221496] tegra-pcie 1003000.pcie: x1-r20-ltssm retry port=1 dbg=00000001 trace_ctl=00000001 trace_status=00090801
[    2.646007] tegra-pcie 1003000.pcie: x1-r20-ltssm retry port=1 dbg=00000001 trace_ctl=00000001 trace_status=00090801
[    2.668095] tegra-pcie 1003000.pcie: link 1 down, ignoring
[    2.678286] nouveau 57000000.gpu: Looking up vdd-supply from device node /gpu@57000000
[    2.724744] nouveau 57000000.gpu: drm: VRAM: 0 MiB
[    2.724798] nouveau 57000000.gpu: drm: GART: 1048576 MiB
[    2.732408] nouveau 57000000.gpu: drm: MM: using COPY for buffer copies
[    2.734283] [drm] Initialized nouveau 1.4.0 for 57000000.gpu on minor 0
```

Interpretation: calibration and GPU DRM init occur, but PCIe endpoint link does not come up. GPU DRM init does **not** establish working hardware rendering. Do not interpret a trace-buffer snapshot as an instantaneous continuous LTSSM state. For registered hypotheses see [PCIE](../docs/PCIE.md) and [GPU](../docs/GPU.md).

Recorded original file in the owner lab: `r20-live-618-firstboot-20261028.log` (67,570 bytes), SHA256 `8b1e69788de0b48cd04c480b3af7fd9cb232cc4f904febcc7498f90d13885cad`. Full original is **not** published due to potential private identifiers; interested maintainers can request targeted redacted evidence.
