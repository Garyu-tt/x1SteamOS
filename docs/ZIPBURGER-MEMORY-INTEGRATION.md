# ZipBurger memory integration design for x1SteamOS (research R00)

Issue: [#7](https://github.com/Garyu-tt/x1SteamOS/issues/7). Target: Switch Lite / Mariko (T210B01), Enigma 6.18. **No kernel/driver integration has been tested.**

## Protocol provenance (important correction)

**ZipBurger / NeuroCrunch ZipBurger** is the project's existing experimental structured tensor codec, **not** a generic zram replacement or a verified universal page compressor. Previous experimental work is associated with [DedupArc](https://github.com/Garyu-tt/DedupArc) and its experimental PR/issue history. Obtain the exact codec source, format specification, commit and tests before any integration. Do not implement a new wire format under the same name.

Previously described experimental features requiring source-level verification:
- Shared basis + residual representation of quantized tensor blocks, conceptually `W = A B + R`.
- MXFP4 / E8M0 scales, signed-zero preservation and bit-exact roundtrip tests.
- Selective access / direct operations on packed weights without full decompression.
- Layer-/expert-level packing and paging; experimental results do **not** establish arbitrary memory-page compression or GPU driver compatibility.
- A previously tested lossy expert packing mode reportedly failed activation-error acceptance. **Do not enable lossy transformations for arbitrary OS memory.**

## Two distinct integration tracks

### Track A: generic RAM (kernel memory pressure)

1. Baseline with existing Linux `zram` and/or `zswap` using supported codecs (e.g. lz4, zstd). Record `/proc/meminfo`, `/proc/vmstat`, `/sys/block/zram*/mm_stat`, PSI, OOM and workload latency.
2. Benchmark actual ZipBurger codec **on representative anonymous page data** first. If it does not compress losslessly, efficiently and independently at page granularity, **do not** integrate it into swap or kernel page reclaim.
3. A codec backend for zram/zswap is conditional on a safe, bounded, lossless block API with verified decode, no unbounded allocations, deterministic failure behavior and suitable locking/concurrency. Until then, keep it as a userspace library.
4. CPU overhead and power consumption on 4 GiB shared memory are acceptance gates, not just compression ratio.

### Track B: GPU / model memory (userspace-first)

1. Tegra GM20B shares physical DRAM with the CPU. There is no separate discrete VRAM pool to magically expand. Driver-managed GEM/DMA-BUF/host1x allocations require synchronization and must not be transparently rewritten while active.
2. Start with **immutable or quiescent model weights and expert blocks** stored in ZipBurger format, with on-demand decode or verified direct computation. Page/release inactive experts at application boundaries, not from arbitrary GPU mappings.
3. Profile compressed resident bytes, temporary decode buffers, peak RSS, total memory, GPU memory allocations, bandwidth, latency and energy. Avoid whole-model decompression that erases the savings.
4. GPU-specific backend must wait for stable Nouveau/GM20B execution on Mariko. GPU-side compressed compute is **research**, not an existing supported capability.
5. Never transform framebuffers, scanout buffers, DMA-visible allocations or active command buffers without a proven synchronized migration mechanism.

## Acceptance matrix

| Test | Must pass |
|---|---|
| Source provenance | Exact ZipBurger repo/commit/license and codec format version |
| Correctness | Bit-exact roundtrip for lossless modes, including signed zero/scales where relevant |
| Fault handling | Corrupt blocks, truncated input, OOM, suspend/resume, concurrency |
| RAM baseline | Compare off / zram-lz4 / zram-zstd / ZipBurger (only if valid page codec) |
| Model weights | Real MXFP4 tensors, representative layer/expert sets, quality and direct-compute checks |
| GPU correctness | Nouveau rendering regression suite and synchronization checks after GPU is stable |
| Device safety | Separate boot slot, L4T 4.9 rescue, no eMMC writes, feature off by default |

## Immediate next implementation step

Locate and audit the existing ZipBurger R03–R14 code and its license. Produce a small, independently runnable **userspace** `encode/decode/verify/bench` adapter for ARM64 before attempting kernel changes. Maintain a strict separation between **general-purpose lossless memory pages** and **structured neural-network weights**.

## Related Switch-specific precedent

[Switchdeck](https://github.com/BandiFee/SwitchVN-Switchdeck) documents opt-in `SD_ZRAM` and `SD_SWAP` modes for 4 GiB shared-memory Switch systems. These are baseline alternatives, not ZipBurger integrations. [ArchWiki zram](https://wiki.archlinux.org/title/ZRAM) documents available kernel compression options and metrics.
