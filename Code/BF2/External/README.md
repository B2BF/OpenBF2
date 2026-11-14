# External dependencies

This directory contains vendored libraries and their engine integrations.
Preserve upstream copyright notices, license files, and source layout. Keep
local changes small and documented alongside the affected dependency.

## Inventory

| Component | Location and provenance | Used by |
| --- | --- | --- |
| CPython 2.3.4 | `python-embedded/py-src-2.3.4/`; Python 2.3.4 source distribution | Embedded interpreter, built as `dice_py.dll` or `libdice_py.so` |
| zlib 1.2.3 | `zlib-1.2.3/`; includes minizip | Compression, archives, and embedded Python |
| TinyXML | `TinyXML/`; upstream TinyXML source | XML parsing through engine wrappers |
| GameSpy SDK | `GameSpy/`; December 2007 SDK, including qr2 3.04.00 | Network service integrations |
| gameswf and base | `gameswf/` and `base/`; Thatcher Ulrich's tu-testbed snapshot of 27 February 2006 | SwiffPlayer menu playback |
| Expat 1.95.0 | `expat/`; official `libexpat` tag `R_1_95_0`, unchanged MIT sources ([provenance and hashes](expat/README.md)); [CHOSEN, not recovered] substitute for DICE's modified parser | SwiffPlayer XML |
| libogg 1.3.6 and libvorbis 1.3.7 | `libogg-1.3.6/` and `libvorbis-1.3.7/`; Xiph.Org sources | BF2Audio Ogg/Vorbis decoding |
| Speex 1.0.5 | `speex-1.0.5/`; official Xiph.Org release tarball, unmodified, BSD ([COPYING](speex-1.0.5/COPYING), [provenance, retail table match and build notes](speex-1.0.5/LOCAL_CHANGES.md)) | GameSpy Voice2 codec for the VoIP client (`gvSpeex.c`) |
| Dear ImGui | `imgui/`; `v1.92.9b-docking`, commit `b48d1af` | Editor UI |
| ImGuizmo | `ImGuizmo/`; tag `1.10`, commit `b796ac3` | Editor transform gizmo |
| FFmpeg Bink decoders | `ffmpeg-bink/`; seven unmodified files of FFmpeg 7.1.1 plus an OpenBF2 shim ([README](ffmpeg-bink/README.md)), LGPL-2.1+ | Bink movie playback on macOS and Windows x64 (`RendDX9Abi/BinkFFmpeg.cpp`); Windows x86 uses RAD's `binkw32.dll` |
| meshoptimizer | `meshoptimizer/`; `src/meshoptimizer.h` and `src/stripifier.cpp` of zeux/meshoptimizer tag `v1.3`, unmodified, MIT ([LICENSE](meshoptimizer/LICENSE.md)) | macOS D3DX subset: `D3DXConvertMeshSubsetToSingleStrip` (`Platform/Gfx/Metal/D3DXMesh.cpp`) |

The table describes the vendored inputs, not a promise that every SDK module is
part of each executable. Build manifests select the files for each image.
`BFMeme/` and `gameswf/SwiffPlayer/` contain engine-specific code; they are not
covered by the claim that an upstream library is unmodified.

The individual sources and license files govern their respective components.
Keep the [Python license](python-embedded/py-src-2.3.4/LICENSE),
[FFmpeg LGPL notice](ffmpeg-bink/COPYING.LGPLv2.1),
[Dear ImGui license](imgui/LICENSE.txt), [ImGuizmo license](ImGuizmo/LICENSE),
[meshoptimizer license](meshoptimizer/LICENSE.md) and
[Speex license](speex-1.0.5/COPYING)
with their distributions. Other dependencies carry notices in their source
files and accompanying upstream documentation.

## Build and layout contracts

### Embedded Python

The Windows build is managed by
[build_python.cmd](../../Build/python/build_python.cmd). It builds the vendored
CPython sources and zlib, names the interpreter `dice_py`, and provides a
`pycheck` consumer. Match the interpreter architecture and runtime settings to
the engine that loads it. Linux integration lives in `python-embedded/linux/`.

### TinyXML

Compile the four TinyXML implementation files without `TIXML_USE_STL`; enabling
it changes the public string types and signatures. The engine uses `TiXmlString`.
The vendored version constants and its available API do not align cleanly;
check the headers when changing a call or replacing the library.

The `dice::hfe::XMLDocument`, `XMLNode`, and `XMLAttribute` types are engine
wrappers. Keep their behavior separate from upstream TinyXML implementation.

### GameSpy

The SDK is newer than the original game's dependency. Keep the selected build
inputs and integration interfaces explicit when updating it. The
[local changes](GameSpy/LOCAL_CHANGES.md) document the maintained deltas.

### gameswf and base

Keep `gameswf/` and `base/` as siblings and put `Code/BF2/External/` on the
include path. Both libraries use `base/...` includes, including base's own
sources. `base/tu_config.h` includes the integrator-owned
`compatibility_include.h` from this directory.

[swiffplayer.cmd](../../Build/swiffplayer.cmd) builds the player separately from
the dedicated server. Its local adaptations are summarized in
[GAMESWF_DIVERGENCES.md](gameswf/GAMESWF_DIVERGENCES.md). Use the typed interfaces
of the vendored library when changing the wrapper.

### Audio codecs

[bf2audio.cmd](../../Build/bf2audio.cmd) compiles the selected Ogg/Vorbis C files
into BF2Audio; the decoder is required and the build stops if its sources are missing. The
[audio comparison harness](../../Build/bf2audio_diff.cmd) treats decoder PCM
hash differences separately from the other checks.

### Editor UI

Dear ImGui and ImGuizmo are editor-only dependencies. The editor CMake target
selects an upstream subset: ImGui core, Win32/DX9 backends, string helpers, and
ImGuizmo. They are not part of the game-image compilation sweep.

When updating a vendored library, record the exact upstream version, verify the
files against that version, preserve its notices, and review local changes
before replacing them.
