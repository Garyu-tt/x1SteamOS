#!/usr/bin/env bash
# Read-only kernel feature audit for Enigma R20/R21. No patching, fetching or boot changes.
set -euo pipefail
if [[ $# -ne 1 || ! -d "$1" ]]; then
  echo "Usage: $0 /path/to/reconstructed/linux-enigma" >&2
  exit 2
fi
SRC="$(cd "$1" && pwd)"
if [[ ! -f "$SRC/Makefile" || ! -d "$SRC/drivers/clk/tegra" ]]; then
  echo "Not a Linux kernel source tree: $SRC" >&2
  exit 2
fi
cd "$SRC"
echo "Enigma upstream audit: $(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
echo "Worktree dirty: $(if git diff --quiet 2>/dev/null; then echo no; else echo yes; fi)"
printf '\n%-28s | %-8s | %s\n' "Feature" "Detected" "Evidence"
printf '%s\n' "------------------------------------------------------------------------------------"
check() {
  local label="$1" path="$2" pattern="$3" result
  if [[ ! -f "$path" ]]; then
    printf '%-28s | %-8s | %s\n' "$label" MISSING "$path"
    return
  fi
  result="$(grep -nE -m 1 "$pattern" "$path" || true)"
  if [[ -n "$result" ]]; then
    printf '%-28s | %-8s | %s:%s\n' "$label" FOUND "$path" "$result"
  else
    printf '%-28s | %-8s | %s\n' "$label" ABSENT "$path"
  fi
}
check "Nouveau fuse member" drivers/gpu/drm/nouveau/include/nvkm/core/tegra.h 'clk_fuse'
check "Nouveau fuse enable" drivers/gpu/drm/nouveau/nvkm/engine/device/tegra.c 'clk_prepare_enable\([^)]*fuse|clk_fuse'
check "GPU DT fuse clock" arch/arm64/boot/dts/nvidia/tegra210.dtsi 'TEGRA210_CLK_FUSE'
check "T210B01 clock driver" drivers/clk/tegra/clk-tegra210b01.c 'tegra210b01|TEGRA210B01'
check "T210B01 PMC" drivers/soc/tegra/pmc.c 'tegra210b01'
check "T210B01 speedo" drivers/soc/tegra/fuse/speedo-tegra210.c 'B01|b01'
check "T210B01 thermal" drivers/thermal/tegra/soctherm.c 'tegra210b01|T210B01'
check "PCIe driver" drivers/pci/controller/pci-tegra.c 'tegra_pcie'
check "XUDC DMA coherent" drivers/usb/gadget/udc/tegra-xudc.c 'dma_alloc_coherent'
printf '\nNOTES: FOUND is a string match, NOT proof of an upstream fix or compatibility.\n'
printf 'Review Mariko board DTS/DTB separately. No build or hardware validation performed.\n'
