# Bundle interface dispatch on Linux

An optimized Linux dedicated server could jump to address zero while loading
`strike_at_karkand gpm_cq 32`. The reported stack reaches
`BundleTemplate::addBundleChilds` from object precaching in `GameLogic::load`.
The failing operation passes a bundle member's transformation to its sound
template before creating the child object.

## Cause and correction

`BundleTemplate.cpp` declared anonymous-namespace copies of
`ISoundObjectTemplate` and `IPlayerControlObject`. The real objects return their
actual secondary interface bases from `queryInterface`; casting those pointers
to unrelated local classes is undefined behavior, even when their virtual slot
positions happen to match.

GCC 14.2 with the Linux preset's optimization flags devirtualizes these calls to
`__cxa_pure_virtual`. The existing server, linked with a static C++ runtime,
leaves that symbol weak and undefined, allowing the call to resolve to zero. The sound call
therefore fails before dispatching to `SoundObjectTemplate`. The adjacent
`addDriverDecals` path contains the same problem.

The correction removes both local declarations and includes the existing shared
sound and player-control interfaces. No virtual declaration, object layout,
reference count, network message or asset format changes. Existing query guards,
parent traversal and call ordering remain intact. Compiler optimization and
runtime linking are unchanged.

## Original evidence

Original BF2 1.50 images were opened read-only in IDA and their instructions,
symbols and interface tables inspected directly:

- Dedicated ELF32 `ia-32`, MD5 `aa94014ed5d4308fb86655f5c890b59f`.
- Dedicated PE32 `bf2_w32ded.exe`, MD5 `3ca127b1be4275196af93347714707f9`.

| Contract | ELF32 | PE32 |
| --- | --- | --- |
| Bundle child setup | `81D8520`; sound setter call `81D86DA` | `5EE440`; sound setter call `5EE541` |
| Sound query for IID `985FD2` | `83F7220`, returns secondary base at `+30` | `7065D0`, returns secondary base at `+60` |
| Sound interface setter | Table `88D6D40`, slot `+74`, thunk `83F6B70` to `83F5660` | Table `8090D8`, slot `+74`, body `705510` |
| Driver decal setup | `81D8270`, query `81D833F`, calls `81D8361` then `81D8379` | `5EE260`, query `5EE3E2`, calls `5EE3FC` then `5EE40A` |

Both sound setters ignore a null matrix, otherwise forward its translation
column to `setPosition`, then its Z-axis column to `setDirection`. Bundle setup
passes the member transformation before `createObject`, including sound
templates that deliberately return no runtime child. Driver-decal setup searches
the supplied object and its parents for the interface, then calls
`setDriverDecalObject` and `addDriverDecalTransformation` in that order. The
existing shared interfaces express those contracts; no guessed slots are added.

The original `Game/Objects/Base/BundleTemplate.cpp` path also appears in the
driver-decal diagnostic instructions. The x86 interface evidence establishes
the methods and their ordering, not native x86-64 byte offsets.

## Validation scope

The pre-fix source, optimized object and disassembly are preserved locally under
`/home/user/bf2-linux-bundle-nullcall/before`. A second source snapshot is under
`out/bundle-sound-dispatch-20261003`; both have SHA-256
`e1510b00c99e5ab933135f6ee66784d1c7bc241fb9b0d3d7fb717fab06ece4a7`.
The original optimized TU shows direct pure-virtual relocations in both affected
methods. MSVC x86 real-header syntax validation passes for the corrected TU.
The corrected optimized Linux TU instead calls the sound interface at native
slot `+E8` and the driver-decal interface at `+230` and `+240`, with no direct
pure-virtual relocation in either method.

The isolated `linux-x64` build of `bf2` and `dice_py_check_run` passed under
`/home/user/bf2-linux-bundle-nullcall/build`. This is a strict link, with the
normal optimized preset and static C++ runtime. The Python check passed 40
builtin imports, the 64-bit PyCObject roundtrip, zlib and loopback UDP.

The optimized Linux focused fixture passed **47/47 checks** using actual
`BundleTemplate` code and collaborators derived from the shared interfaces. It
checks adjusted secondary bases, transformation identity and values, ordering,
repeated cache use, sound templates returning no child, absence of the optional
sound interface, and driver-decal registration. These test collaborators do not
claim a complete engine lifecycle.

The same focused fixture passed **47/47** on MSVC x86, and the broader Windows
runtime-pair suite passed **472/472**, all with strict links and optimization
enabled. Running the saved pre-fix TU instead reproducibly aborts through
`__cxa_pure_virtual` in both modes: `addBundleChilds` at its former line 275 and
`addDriverDecals` at its former line 334. The fixture's dynamically linked C++
runtime provides the abort handler rather than leaving it unresolved.

Repeat the focused checks from the repository root:

```bash
python3 Code/Tests/bundle_sound_dispatch_runtime.py ~/bf2-bundle-dispatch-check
```

```powershell
$env:BF2_BUILD_OUT = "$PWD\out\bundle-dispatch-check"
cmd /c Code\Tests\bundle_sound_dispatch_runtime.cmd
# The same manifest can also run the full existing Windows suite:
cmd /c Code\Tests\bundle_sound_dispatch_runtime.cmd --full-pair
Remove-Item Env:BF2_BUILD_OUT
```

The runner accepts `--mode sound` or `--mode decal`, and an optional `--baseline`
path to a saved complete production TU. It preserves that TU's bodies and only
relocates its relative include paths. It neither disables virtual-call
optimization nor suppresses linker errors. Linux results are under
`/home/user/bf2-bundle-sound-fixture-20261003/{before,after-final}`; Windows results
are under `out/bundle-sound-dispatch-20261003`.

An isolated Karkand `gpm_cq 32` server run used loopback port **17691**, private
settings and copied server content. GDB backtraces confirmed the actual
`BF2::init -> BF2Engine::startGame -> GameServer::startServer -> loadPath` path.
Loading reached 100% after about three seconds; the server reported playing at
roughly 35–36 FPS for another 45 seconds, then exited normally through console
`quit` at about 49 seconds. No frontier or crash was observed.
The game port and query port `30691` both rebound immediately after the test.
Runtime evidence is under `out/bundle-interface-nullcall-20261003/runtime`;
the private content tree is `/home/user/bf2-bundle-nullcall-runtime-20261003/game`.
AI, PunkBuster and VOIP were disabled; the run was LAN/unranked and had no clients.

The sound-setter breakpoint did not fire with that server-content copy and no
connected client. The run establishes production map loading and ticks; direct
coverage of the corrected calls comes from the focused regression and compiled
dispatch inspection. It is not a joined-client, reconnect, map-reload or soak
claim. GDB itself returned 1 because the harness requested registers after normal
process exit; this is separate from the server's normal exit.

This change does not recover unrelated player-control methods or alter their
existing frontier contracts. In particular, the interface's chosen return
types, unresolved `VehicleMod` values and `pco-getweapons-vector` retirement
condition remain as documented in its header. No sound device playback is
required by the dedicated server's NullSound implementation.

Production changes are confined to `Game/Objects/Base/BundleTemplate.cpp`.
The accompanying test changes make the two `runtime_pair.cpp` collaborators
derive from the real interfaces and add a portable focused runner. The existing
runtime-pair source lists in `runtime_pair.cmd` and `CMakeLists.txt` include the
real player-control interface TU so default-vtable and RTTI definitions resolve
in all configurations. No original images or reference game files were changed.
