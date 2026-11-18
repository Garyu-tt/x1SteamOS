#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Read-only inventory for the original Switch Lite Linux host.
set -u
echo '== x1SteamOS headless host audit (read-only) =='
printf 'kernel: '; uname -r
printf 'architecture: '; uname -m
printf 'hostname: '; hostname
echo '== virtualization =='
if [ -e /dev/kvm ]; then ls -l /dev/kvm; else echo '/dev/kvm: absent'; fi
if command -v zcat >/dev/null 2>&1 && [ -r /proc/config.gz ]; then
  zcat /proc/config.gz 2>/dev/null | grep -E '^CONFIG_(KVM|KVM_ARM_HOST|VHOST|VFIO|IOMMU_SUPPORT|ARM_SMMU|CGROUPS|NAMESPACES)=' || :
elif [ -r "/boot/config-$(uname -r)" ]; then
  grep -E '^CONFIG_(KVM|KVM_ARM_HOST|VHOST|VFIO|IOMMU_SUPPORT|ARM_SMMU|CGROUPS|NAMESPACES)=' "/boot/config-$(uname -r)" || :
else
  echo 'kernel config: not directly available'
fi
echo '== memory =='
if command -v free >/dev/null 2>&1; then free -m; else cat /proc/meminfo | head -n 8; fi
echo '== host physical devices (do not rebind) =='
for d in /dev/fb0 /dev/fb1 /dev/dri/card0 /dev/dri/renderD128 /dev/input/event0 /sys/kernel/iommu_groups; do
  if [ -e "$d" ]; then ls -ld "$d"; else echo "$d: absent"; fi
done
if command -v lspci >/dev/null 2>&1; then lspci -nn; fi
if command -v ip >/dev/null 2>&1; then ip -brief link; fi
echo '== binaries =='
for c in qemu-system-aarch64 qemu-img systemctl Xorg xinit vncviewer remote-viewer; do
  if command -v "$c" >/dev/null 2>&1; then command -v "$c"; else echo "$c: missing"; fi
done
echo '== display managers (read-only) =='
if command -v systemctl >/dev/null 2>&1; then
  systemctl get-default 2>/dev/null || :
  systemctl is-active display-manager.service 2>/dev/null || :
fi
echo '== completed: no host changes =='
