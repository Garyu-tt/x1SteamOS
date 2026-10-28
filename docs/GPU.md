# GPU, Nouveau, Mesa, KWin / Wayland

## Target

Hardware GPU rendering and compositing on Switch Lite GM20B using a maintainable contemporary DRM/Mesa path. Vulkan/NVK and performance improvements are longer-term **research goals**, not guaranteed by a newer kernel alone.

## R20 facts (2026-10-28)

- `nouveau 57000000.gpu` and Tegra DRM initialize, creating card/render nodes under `/dev/dri`.
- Mesa 26.2.3 `eglinfo -B` for GBM/surfaceless reports vendor NVIDIA, renderer tegra, OpenGL 4.3 / GLES 3.2.
- A minimal offscreen EGL ES2 context is created, but the first `glClear(GL_COLOR_BUFFER_BIT)` crashes in Gallium. Therefore **real hardware 3D rendering is not proven** even though EGL strings look promising.
- The same 16x16 test with `LIBGL_ALWAYS_SOFTWARE=1` / llvmpipe performs pixel readback correctly: RGBA `64,128,191,255`.
- Existing KWin/Wayland logs contain failures to establish a stable compositor socket. This is downstream of, or at least concurrent with, the graphics failure and requires independent debugging.

## Suggested independent work

- Reproduce the GLES2 `glClear` crash with symbols and a gdb backtrace; inspect Mesa version and ioctl path to distinguish Mesa bug from kernel interface fault.
- Test GBM device selection, render nodes, buffer allocation, `drm`/ioctl trace and simpler render tests.
- Record positive readback and repeatability before claiming acceleration.
- Only then test KWin Wayland, session setup, input and multi-minute stability; consider NVK/Vulkan as a separate milestone.
- Compare against known functioning L4T GPU accelerated setup without assuming L4T equals llvmpipe.

## Acceptance gate

Actual rendered pixels from GM20B GPU, non-llvmpipe path, no crash; Wayland socket/compositor; stable usage over at least 10 minutes, with exact commit, patches, Mesa version and logs recorded.
