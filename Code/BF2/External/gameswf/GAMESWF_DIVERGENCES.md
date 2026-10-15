# gameswf and base local adaptations

The upstream base is Thatcher Ulrich's tu-testbed snapshot of 27 February 2006.
The vendored libraries are adapted for BF2's menu movies and the SwiffPlayer
host. Keep upstream notices intact; the source files retain their original
licensing declarations.

Local changes use two searchable source markers:

- `[BUILD-FIX SwiffPlayer: ...]` for build integration changes.
- `[DICE-MOD SwiffPlayer: ...]` for menu-runtime and host compatibility changes.

Run `rg "BUILD-FIX SwiffPlayer|DICE-MOD SwiffPlayer" Code/BF2/External`
from the repository root to locate the affected code.

## Integration

Keep `gameswf/` and `base/` as siblings, with their parent directory on the
include path. `compatibility_include.h` supplies the integrator configuration.
The engine-specific wrapper lives in `SwiffPlayer/`.

Build the image with
[swiffplayer.cmd](../../../Build/swiffplayer.cmd). A dedicated-server build does
not validate this library. The wrapper uses typed gameswf calls and maps renderer
operations by function; raw vtable positions are not portable between library
versions.

## Adaptation map

| Area | Main files | Behavior to preserve |
| --- | --- | --- |
| Build configuration | `gameswf_shape.cpp`, build manifest | Disable the optional OpenGL shape-debug path. Exclude the inactive XMLSocket translation unit, whose unconditional SDL include is unavailable in this build. |
| Host access and images | `gameswf.h`, `gameswf_action.*`, `gameswf_impl.*`, `gameswf_render.*` | Global-object access, named bitmap exports, external bitmap creation policy, texture loading, and background-color readback. Exported names must own their string storage. |
| ActionScript objects | `gameswf_action.*`, `gameswf_string.cpp` | Properties and prototype accessors, class registration, object literals, inheritance, constructor dispatch, `this`/`super`, array/string methods, enumeration, and object conversion. |
| VM state | `gameswf_action.*` | Tolerant stack underflow, timeline variables, caller register banks, and function register save/restore. |
| Timers | `gameswf_timers.*`, `gameswf_impl.cpp` | Global timer registration, object-method callbacks, and millisecond intervals. |
| Sprite lifetime and timelines | `gameswf_impl.*`, `gameswf_dlist.*` | Deferred removals, dead-clip access, construct/load ordering, label and numeric goto behavior, timeline state changes, and action-queue draining before execution. |
| Drawing and bounds | `gameswf_shape.*`, `gameswf_styles.h`, `gameswf_impl.*` | Runtime shapes, image clips, owned runtime definitions, local clip extents, and scale-aware width/height updates. |
| Text and localization | `gameswf_text.cpp`, `gameswf_font.h`, `gameswf_string.cpp`, render interfaces | Renderer-managed text lines, editing/focus/selection, independent narrow and wide values, and font-leading terms for caret and auto-height. |
| Mouse input | `gameswf_impl.*`, `gameswf_button.*`, `gameswf_text.cpp` | Per-character dispatch, capture across press/release, handler names, and capture release when a containing sprite is hidden. |
| x64 portability ([issue #7](https://github.com/B2BF/OpenBF2/issues/7)) | `base/container.h`, `base/utility.*`, `gameswf_action.cpp` | Hash entries store the functor's full `size_t` hash, the value lookups compare (the upstream `int` round trip sign-extended on x64 and lost keys). Object and function address strings print the whole pointer, so object identity through string comparison holds above 4 GB. The debug-only assert breakpoint uses `__debugbreak` where inline asm is unavailable. x86 code generation and output are unchanged. Marked `[CHOSEN, not recovered] x64`. |

## Constraints for changes

Function calls borrow the caller's register bank while name lookup keeps the
defining environment. Function2 calls save and restore registers 0 through 9;
higher registers remain shared. Keep this distinction when changing nested calls
or constructor dispatch.

Text fields keep narrow and wide values independently. A wide-only string can
provide display text while converting to an empty narrow string. Keep the
renderer selection result as a Boolean; equal or uninitialized selection
endpoints do not replace it.

Numeric gotos are queued; label gotos apply immediately. Action queues remove a
buffer before executing it so reentrant navigation cannot execute the same
buffer twice. Runtime-created definitions must remain alive through deferred
display-list removal.

This integration is incomplete. Examples include solid-color fallback for
gradient fills, unsupported custom array-sort comparators, and remaining
timeline/lifetime differences documented at their source boundaries. Preserve
those markers and validate the affected menu flows when changing these areas.
