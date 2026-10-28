# Experiment history (partial)

**These are research artifacts, not a linear series of patches to apply in order.** The historical patches in `patches/history/` often overlap; use only the documented R20 series for reconstruction.

| Revision | Focus | Evidence / outcome |
|---|---|---|
| R04 | Early modified Linux-NX, SDMMC trace and hardware setup | Source + diagnostic patches retained |
| R08 | WLAN cold reset / PCIe | Tested concept; did not achieve reliable PCIe enumeration |
| R09 | Mariko UPHY calibration and lanes | `CAL_DONE` achieved, link still down |
| R11 | PLL_E arbitration | Link remained down |
| R12 | L4T-style power-rail comparison | Booted, link down |
| R13 | Read-only link register trace | Link down, trace for debugging |
| R14–R16 | B01 ECTL/PCA hypotheses | Isolated modifications, no working link shown |
| R17 | Presence/power/PCIe combinations, KWin experiments | Booted experiments; PCIe link still down; some UI experiments, unstable |
| R18–R19 | Isolations including GPU or PCIe disable | Diagnostics / stability experiments, not feature-complete releases |
| **R20** | Read-only LTSSM capture | **Physical boot confirmed**; USB gadget SSH works, PCIe link down, hardware GL draw fails |
| **R21** | Isolated PAD REFCLK configuration candidate | **Offline build/package completed; hardware test pending** |

Source baseline R20 is upstream commit + `enigma-r20-base.patch` + `enigma-r20-ltssm.patch`. R21 adds one separate patch. Tests must be rerun on the specific device and userland; not all historical artifacts have complete independently repeatable builds.
