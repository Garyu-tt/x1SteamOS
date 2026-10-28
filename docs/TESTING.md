# Reproducible testing and recorded checksums

**Status is evidence-specific.** A test conducted in WSL or a compiled object is not equal to hardware success.

## Offline source-patch check (performed 2026-10-28)

Upstream commit: `80f1e68e163ac77d688f0244b03cf05f791b4781`.

The following sequence was applied to a *temporary Git index* based on the exact upstream HEAD (without mutating the working source):

1. `patches/enigma-r20-base.patch` — **PASS** (SHA256 `a5002215490e81d653697bbd43d256f75f1ad0f0a26f6ff9ba2364d9f010485c`)
2. `patches/enigma-r20-ltssm.patch` — **PASS** (SHA256 `4e60b61b4fb808f524035b6f41863c155c4accfd8daf4ef5d661786f0270b2b2`)
3. `patches/experimental/r21-pad-refclk.patch` — **PASS** (SHA256 `0fa0717a9e1a250b1773967f24409669bc6f3fb7d49ac7c1465d2ae46476480b`)

Combined changes: 16 kernel files changed; 324 insertions, 58 deletions. This only verifies patch applicability, **not compile success in this check**.

## Artifact evidence previously collected

- R21 raw Image size: `34,417,152` bytes, SHA256 `89d337ccbc539ac0c0d1af5ee31d2c393cca197c798f8318fb08a63852582293`.
- R21 uImage SHA256: `285dabe81fc7c902ada1ec42e85256c2d05c3c8575df4392d5e8ce54c7cb68b4`.
- R21 VALI DTB SHA256: `87cf1162bf002b472ddd8c5f2f85a607e2e7a384fa5b92aa89fb05ad8b45a3e9` (same as R20).
- R21 DTIMG SHA256: `f67ade50d93924814af9e06bc1bb163dedc322f0615c2fc90836941dda971066`.
- R21 `Image dtbs` compiled, package checked; **no R21 device boot verified**.
- R20 previously boot-tested on Switch Lite: USB SSH works, PCIe link fails, Nouveau/Mesa real GPU draw fails.

## Hardware test result fields

Record UTC date; console model/revision; exact kernel version & commit; patch hashes; Hekate profile; baseline; action; outcomes for boot, USB, PCIe `lspci`, Wi-Fi, `/dev/dri`, EGL context, `glClear`, readback, KWin socket, input, uptime, thermal and battery. Never infer result from an SSH timeout alone. Redact MACs, serial numbers, user paths and credentials before sharing logs.
