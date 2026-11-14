# gameswf platform choices

The gameswf/base sources began with Thatcher Ulrich's tu-testbed snapshot of
27 February 2006. S3 supplies recovered bodies where recorded in the ledger;
S4 removes the unused upstream
features and retires the old DICE-MOD patch annotations. Existing upstream
copyright and licensing notices remain in the retained files.

This file lists only retained platform adaptations and owner decisions. Each is
`[CHOSEN, not recovered]`. Retail instructions and their addresses belong beside
the implementation and in the [track ledger](../../../../docs/tracks/swf/ledger.csv).
The S4 work recorded removals, evidence,
validation and unfinished recovery. Retirement of a patch annotation does not
certify a whole function or remove a named frontier.

| Choice | Contract and limits |
| --- | --- |
| Native rebuilt ABI ([issue #7](https://github.com/B2BF/OpenBF2/issues/7)) | Native C++ interfaces, member-based containers, strings, virtual bases, type-adjustment helpers and allocation replace the VC7.1 x86 representation. Shared register storage and separately owned character arguments adapt fixed/embedded retail storage. Strong function/class/prototype links and function-property object adapters remain explicit in source. These choices apply to rebuilt images on every host; they are not retail object-layout claims. |
| Native value/host adapters (issue #7) | Existing NULLTYPE-to-tag-zero mapping, tolerant operand storage, the checked movie cast where retail adjusts a found script object to its character unchecked (StartDrag 0x10092E68), native receiver/environment adapters, definition-bounds providers and text extent/character conversions remain where their callers still need them. Text CRT conversion keeps the recovered C-locale byte widening; the non-C POSIX locale/wchar_t adapter retains native conversion and error semantics. |
| Native widths and build integration (issue #7 and the owner-requested macOS port) | Hash entries retain the functor's full `size_t` hash; object/function identity strings retain full pointers. x86-64/AArch64 are known little-endian targets. Supported C++ libraries supply math declarations; obsolete OS X math macros stay disabled. `tu_util` comparison helpers and qualified `::hash` friends avoid standard-library collisions. Debug breaks use the supported compiler primitive. Standard-library headers precede compatibility macros. |
| Native movie lifetime and collector (issue #7; owner decision 2026-11-09) | Reference counting and the script-object registry stay active. Last-player teardown clears interval records, script graph edges and library class bindings, releases the player's references and resets action globals. With another live player, global teardown is skipped and logged. Movie libraries stay cached. The five collector rows remain `chosen`: `0x10077700`, `0x1008D950`, `0x1007B320`, `0x1008A160` and the GC portion of `0x1007AED0`. No direct-deletion sweep is activated or claimed recovered, and the collector-list link that retail performs when the VM or action_init creates an object (common slot +0xC, e.g. NewObject 0x10093714, NewMethod 0x100944FF, action_init 0x10091030) is not performed. |
| Renderer and input platform seams (owner-requested macOS port; issue #7) | Metal/Cocoa replace Win32 rendering/input services at their host boundaries. Generated RGBA/A8 pixels are retained and uploaded once per live handler registry, with weak cached-definition registry links and fresh RGBA copies because RendDX9 swaps channels in place. Handler/descriptor destruction releases uploads. The native asynchronous key callback supplies the platform input state; the retail Enter predicate itself is shared code. |
| Expat substitute (owner decision) | Official Expat R_1_95_0 and C callback adapters stand in for the unidentified retail parser revision. The vendored files remain unchanged. Unicode classification and parser internals can differ; see [provenance](../expat/README.md). The DICE XML wrapper is separately recovered; a passing XML fixture does not certify parser equivalence. |
| Numeric and CRT edges (owner decision 2026-11-09) | The 13 designated numeric/CRT rows retain standard math and native CRT behavior without x87 emulation. Last-bit results, inherited precision/rounding, nonfinite/range behavior, formatting, locale and errors can differ. Other native x87/storage adapters retain their documented limits; ordinary-input agreement is not an equivalence claim. |
| Hash-capacity mode binding (owner decision) | Native policy selects the normal client 24-bit/nearest mode. The integer capacity results for that mode are recovered, including early large transitions, no minimum-16 clamp and the exponent-32 masked shift. Nonpositive requests retain the native clear adapter until each old dangling-word/new null-word template variant is attributed. Entry widths/ownership remain native. Alternate inherited FP modes are not emulated. See `Code/BF2/External/base/hash_capacity.h`. |
| Unattributed inventory (owner decision 2026-11-09) | All 104 category-(d) rows retain `unattributed, no recorded reference` as their final inventory disposition. This neither recovers a body nor proves runtime unreachability. No source owner, signature or callable implementation is invented. |

Build the client movie image through CMake or
[swiffplayer.cmd](../../../Build/swiffplayer.cmd); the manifests list the same
remaining gameswf/base sources. The standalone 2006 SDL Makefiles and tools are
removed. Keep `gameswf/` and `base/` as siblings and their parent on the include
path. `compatibility_include.h` remains the integrator configuration and heap
seam. A dedicated-server build alone does not validate this library.
