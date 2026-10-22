# HUD icon lookup tracing

The normal client draws 3D map icons through `NameTagRenderer::drawIcons` in
the native x86 RendDX9 image. Its manifest is `Code/Build/renddx9.cmd`.
This correction removes three synchronous diagnostic calls that are absent from
the original renderer: `Effect::getShaderParameter`,
`DebugLineGraphD3D::setFlag05`, and `DebugLineGraphD3D::addSample`.
The lookup cache, ownership rules, graph samples, drawing order, ABI, protocol
and file formats are unchanged.

## Why the Alt toggle matters

The client maps Alt to `GI3dMap` (action 57), which toggles NametagManager's
3D-map flag. Hiding these icons skips their draw calls while manager scanning,
crosshair targeting and line-of-sight work continue. Each drawn icon performs
seven or eight shader-parameter lookups. Those lookups were logging even on a
cache hit.

The added `RD9_TRACE` path takes a lock, formats the message, sends debugger
output and writes and flushes `RendDX9Rebuilt.log` on every call. The inspected
normal-client log from October 3, 2026, 12:27:04 through 12:32:14 contains
369,787 shader-parameter traces and 95,604 combined graph flag/sample traces.
The Nametag effect accounts for 223,265 of the lookups. One one-second interval
contains 4,945 Nametag lookups across 20 graph-frame samples; another interval
contains nine lookups per frame at about 100 frames per second.

This is evidence of substantial work on the affected path, not a controlled
gameplay comparison: the log contains no timestamped Alt events. Particle
effects also use the same getter and benefit from removing its trace.

## Original evidence

The instructions and callers were inspected directly in IDA using the original
BF2 1.50 x86 Windows images. RendDX9 MD5 is
`aaf43741c3e1504f8d1498d366f9f4fa`; BF2.exe MD5 is
`4c843dcee79388341f72c3a7adb1ace6`.

| Image and location | Observed behavior |
| --- | --- |
| BF2.exe `7590C3–7590ED` | Poll action 57, read the 3D-map flag, negate it and call its setter |
| RendDX9 `101B1BB0` | Draw icons with the existing shader-parameter lookup sequence |
| RendDX9 `1001E3A0–1001E40E` | Return an existing parameter or create and cache one; no diagnostic call |
| RendDX9 `1001E420` | Owning wrapper adds its caller's reference |
| RendDX9 `10096480–1009648A` | Store and return the graph flag byte; no calls |
| RendDX9 `100A2520–100A25C9` | Update sample extrema and the ring buffer; no calls |
| RendDX9 `10130360`, `1012CD50`, `1012E350` | Conditional text rebuilding, distance hysteresis and selection refresh match the current manager |

RendDX9 is a Windows client image; the dedicated Linux ELF supplies no matching
renderer implementation. No ELF parity claim is made. This change restores
absence of extra diagnostic calls, without introducing a new rendering policy.

## Validation and limits

The focused experiment is `Code/Tests/effect_parameter_cache_runtime.cmd`.
It uses the actual Effect implementation, parameter wrappers and diagnostic
logger, with isolated output and a strict native x86 link. Its timing measures
warmed parameter lookups, not GPU rendering or gameplay frame time.

Both baseline and patched runs passed **71/71 checks** for cached handles,
registry nodes, borrowed/owned references and cached allocation-failure results.
The baseline explicitly enables the expectation of trace output; normal runs
assert zero log growth during repeated lookups.

| Warm lookups | Baseline time / log growth | Patched time / log growth |
| --- | --- | --- |
| 1,000 | 17.791 ms / 116,917 bytes | 0.024 ms / 0 bytes |
| 10,000 | 176.502 ms / 1,181,009 bytes | 0.228 ms / 0 bytes |

These are single local measurements with no timing threshold in the test. The
baseline source and logs are under `Code/Build/obj_hud_trace_cost`; each run uses
its own `before` or `after` directory, not the game's log directory.

The isolated renderer build passed **126/126 translation units**, strict DLL
linking and native ABI/import checks, using MSVC x86, C++14, `/MD`, `DEDICATED=0`
and the native plugin ABI. Logs are under `Code/Build/obj_hud_icon_renderer`.
There were no warnings in the changed translation units or the link log.

The DLL and matching PDB from `Code/Build/bin_hud_icon_renderer` were staged into
`cmake-build-debug/bin_client`, with matching SHA-256 hashes. The DLL hash is
`146b270553eb60288da889819c6b508aeb678243f40ccb69344f4d8a0ffb4fc3`.
This validates the renderer build and component behavior, not the full client
executable link or a live game session. Reference game files were not changed.

To repeat the patched component check after an isolated renderer build:

```powershell
$env:BF2_RENDERER_OBJECTS = "$PWD\Code\Build\obj_hud_icon_renderer"
$env:BF2_BUILD_OUT = "$PWD\Code\Build\obj_effect_cache_check"
try {
    cmd /c Code\Tests\effect_parameter_cache_runtime.cmd
} finally {
    Remove-Item Env:BF2_RENDERER_OBJECTS, Env:BF2_BUILD_OUT
}
```

The fixture deliberately does not destroy its Effect: `Effect::~Effect` remains
a named frontier. It also does not exercise the existing effect-rebind,
nonempty-technique-cache or pending-value-application frontiers, nor the
unrecovered DebugLineGraph virtual slots (`14`, `20`, `24`, `2C`, `30`, `34`,
`38`, `48`, `54`, `58`, `5C`, `60`, `64`, hexadecimal byte offsets).
Their diagnostics remain intact.
No logging policy or frontier implementation was changed.

Only `Code/BF2/RendDX9/Effect.cpp` and
`Code/BF2/GeomMisc/DebugLineGraphD3d.cpp` change production behavior. The focused
fixture's `.cpp` and `.cmd` and this note accompany those deletions.
No live joined-client frame-time comparison has been performed. A gameplay
retest should use the same map, camera position, resolution and settings while
comparing visible and hidden 3D icons.
