# Remaining work

- GPU: benchmark first, then evaluate texture display and optional GPU effects with CPU fallback. Document rasterization currently comes from plugins through Cairo.
- Girara removal: approximately 9–15 developer-weeks; replace settings/bindings and command UI behind adapters while preserving configuration behavior.

Implemented: physical shortcuts, fullscreen cleanup, image effects/recolor module,
book reflow/margins, content bookmarks/jumps, view history, shared cache budget,
clock, quiet selection, distribution packers and container CI.

Measured CPU adjustments: ~79 ms at 1920×2880, ~311 ms at 3840×5760 on the
[scanned-page workload](../benchmarks/2026-10-03.csv). GPU effects should retain
original textures and change uniforms without rerasterization. Use GdkGLTextureBuilder
or GtkGLArea; GskGLShader is deprecated since GTK 4.16. Keep a CPU fallback;
GPU document rasterization would need additional engine/API work.
