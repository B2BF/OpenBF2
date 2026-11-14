# vkd3d-shader patches (macOS port)

[CHOSEN, not recovered] Small patches to vkd3d-shader `vkd3d-2.1`
(`fb7f9ff3c1f8d972ee6f1b8c6a92caf6c8ba1e10`), LGPL-2.1-or-later like vkd3d itself.
`tools/macos/nativefx/build-vkd3d.sh` copies the pristine source from
`out/third_party/vkd3d`, applies these in order with `patch -p1 -F0` and builds
`libvkd3d-shader.a` under `out/build/nativefx/vkd3d`. Each patch starts with a
description.

| Patch | Fixes |
| --- | --- |
| 0001 | vs_1_x relative addressing wrote an address token that only vs_2_0+ has |
| 0002 | sm2 texld coordinate must be unswizzled (allocation + encoding) |
| 0003 | vs_1_x frc may only write .y/.xy |
| 0004 | generic `sampler` argument for a typed sampler parameter |
| 0005 | keep a component-reordering copy in front of sm2 texld |
| 0006 | keep the replicating copy in front of sm1/2 texkill (clip of a scalar) |
| 0007 | constant folding through intermediate infinities (pow(0, n)) is a warning |
| 0008 | CTAB struct member table after its type info (MojoShader layout check) |
| 0009 | local CSE before sm1-3 register allocation (skinning matrix loads) |
| 0010 | pow(x, 2..4) as multiplications, like d3dx9 (sign of negative bases: fog) |
| 0011 | SM2/3 loops with an int-uniform limiter as `rep 255` with the body's conditional break |
| 0012 | unrolling bounded by array indexing behind a uniform exit test |
| 0013 | debug log channel macros compiled out |
| 0014 | SM1/2 jump lowering: 0012 for every profile; flattened stores to undefined old values keep the new value (no read of an unwritten temp) |
| 0015 | d3dbc legalisation for the D3D9 validator: unwritten temp reads, one c# per vs instruction, ps_2_0 swizzles, oC/oDepth by mov |
