# Contributing to Enigma / x1SteamOS

Thank you for helping! Kernel contributors, device-tree reviewers, DRM/Mesa developers and users willing to record controlled hardware evidence are welcome.

## Workflow

1. Read [STATUS](docs/STATUS.md) and [TASKS](docs/TASKS.md) first. Work on **one hypothesis per revision**.
2. Use `scripts/fetch-enigma.sh` to get the exact upstream base and apply the verified R20 series.
3. Create a descriptive branch and make an isolated patch. Include affected hardware revision, source baseline, exact reproduction steps, good/bad behavior, and relevant logs.
4. Open a GitHub issue (diagnosis/question) or pull request (code/docs). In PRs, clearly state whether tested **on real Switch Lite**, compiled only, or untested. A successful `make` is not proof of boot or graphics acceleration.
5. Never change the owner's recovery boot profile or default Hekate entry. Ask the hardware tester to opt into physical boot experiments.

## Useful issue / PR template

**Hardware:** Switch Lite Mariko / other (exact revision)

**Build ID and base:** `80f1e68e163ac77d688f0244b03cf05f791b4781`, patch list and hashes

**Subsystem:** PCIe, Wi-Fi, Nouveau/Mesa, Wayland, USB, power, touch, etc.

**Expected / actual results:**

**Evidence:** dmesg excerpts, register reads, backtraces (redact MACs, serial numbers, user names, IPs, tokens)

**Validation:** source-only / compile pass / live boot / functional test / regression test

**Rescue path:** working L4T 4.9 entry preserved

## Rules

- Reproduce before changing multiple hardware constants.
- Prefer small scoped patches and an explanation of why each register / bit changes.
- Never submit proprietary console firmware, production keys, Nintendo files, private SSH keys, personal dumps, or full unredacted system logs.
- Respect existing kernel source SPDX headers and upstream licenses. Project patches follow the licensing of modified kernel files.
- No pressure on the original maintainer for immediate hands-on tests. CPU-only checks / static reviews / analysis of recorded evidence can be done independently.
