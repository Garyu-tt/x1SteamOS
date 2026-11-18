# Mandatory resource boundary

The working L4T Linux 4.9 host owns the physical platform devices:

- PCIe root controller and BCM4356 Wi-Fi endpoint, driver brcmfmac; keeps SSH/network route
- Tegra framebuffer, DRM, GPU/nvgpu and physical panel
- power, charger, battery, touch, gamepad and sound controllers
- physical storage partitions and recovery boot files

The guest only receives virtual QEMU devices; network is user-mode NAT, with no TAP/bridge/network configuration changes. VNC is bound to loopback only.

This is not direct GPU passthrough. Do not detach the Wi-Fi driver, modify PCIe BAR mappings, or experiment with VFIO until KVM/IOMMU isolation is separately proved and an independent recovery channel exists.
