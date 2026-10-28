# Source distribution, licensing and privacy

## What is included

This repo tracks the complete **delta sources** for Enigma against an exact public Linux-NX Git commit, reproducible checkout helper, a kernel configuration snapshot, diagnostic scripts, hardware evidence summaries and experiment history. `fetch-enigma.sh` reconstructs a full kernel source tree.

## What is deliberately not included

- Many gigabytes of duplicate Linux checkouts, build caches and intermediate objects.
- Console keys, proprietary firmware, boot partitions, production data, serial-numbered dumps or private device images.
- Raw unredacted dmesg / network captures that can contain private hostnames, MAC addresses, or routes.
- Large binary build artifacts without adequate release packaging / recovery checks.

## Licensing

Original Linux and Linux-NX source files retain their own copyrights, SPDX and upstream license terms. Modifications represented by kernel patches are derivatives of their respective upstream files and must preserve notices / applicable GPL terms. No rights to Nintendo/NVIDIA proprietary components are granted. Before distributing independently written utilities under an explicit new license, the project owner should choose that license.

## Intended improvement

Maintainers may later publish a complete fork of Linux-NX with committed changes rather than a patch stack; the patch format is reproducible and reviewable in the meantime.
