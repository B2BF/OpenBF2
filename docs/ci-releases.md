# CI and release packages

[CI](../.github/workflows/ci.yml) runs on pushes to `main`, pull request updates
and manual dispatch. Superseded runs of the same event/ref are cancelled. Fork pull
requests use read-only permissions and require no secrets. Repository settings may
require a maintainer to approve a first-time contributor's workflow run.

CI and [Release packages](../.github/workflows/release.yml) call the same
[build workflow](../.github/workflows/build.yml). It builds the event's exact commit
with the existing CMake image manifests and presets. This is build maintenance;
it does not recover or change engine behavior, ABI, protocols, or file formats.

## Build coverage

| Runner | Configuration | Targets and checks |
| --- | --- | --- |
| Windows 2022, MSVC x86 | `msvc-x86` / `Reconstruction` | Python interpreter check; `bf2_run_checks`; `bf2_editor_run_checks`; `bf2_w32ded`; `bf2_aidll`; with a usable D3DX source: `bf2_client` and plugins, OpenAL Soft, `bf2_editor` and plugins, then `dumpbin /dependents /imports` of the client's `BF2.exe`, `RendDX9.dll`, `TextureAtlasBuilder.dll` and `BF2OpenAL.dll` and of the editor's `BF2Editor.exe`, `RendDX9.dll` and `TextureAtlasBuilder.dll`, and the embedded manifests of `BF2.exe` and `BF2Editor.exe` (`mt.exe`), in the log |
| Windows 2022, MSVC x64 | `msvc-x64` / `Release` | Python interpreter check; strict `bf2_w64ded` and `bf2_aidll` links; AI export listing and staged-copy comparison; with a usable D3DX source: `bf2_client` and plugins, OpenAL Soft, then `dumpbin /dependents /imports` of `BF2.exe`, `RendDX9.dll`, `TextureAtlasBuilder.dll` and `BF2OpenAL.dll`, and the embedded manifest of `BF2.exe` (`mt.exe`), in the log |
| Ubuntu 22.04, GCC x64 | `linux-x64` / `RelWithDebInfo` | Strict `bf2` link; `dice_py_check_run`; the Linux release package |
| Ubuntu 22.04 (Windows package) | none | Packaging fixtures; assembles the Windows release package from both Windows jobs' per-role ZIPs |

Windows builds initialize MSVC through the repository's `vsdevcmd.cmd`. They build
Python into an architecture-specific output directory before CMake configuration,
then pass its DLL/import-library paths explicitly. Linux installs CMake, Ninja,
GCC and ncurses headers and builds Python through CMake. No game installation or
retail binaries are required for compilation.

The client and editor renderer plugins need the
[April 2005 DirectX SDK or the D3DX fallback](building.md#d3dx-without-the-directx-sdk).
Both Windows jobs probe with `Code\Build\dxsdk.cmd <arch> fallback`. When neither
source is usable, the job posts a notice, skips the client (and the editor on x86),
and packages only its server. Otherwise it builds and packages those images. In a
release run a missing D3DX source fails the job instead
([D3DX in release runs](#d3dx-in-release-runs)).

Windows ships both clients. Both bundle OpenAL Soft as `BF2OpenAL.dll` (see below). The x86
client takes retail's other 32-bit companions from the game folder (D3DX, Bink, PunkBuster).
The x64 client has **no PunkBuster support**. PunkBuster exists only as 32-bit DLLs, so the x64 client cannot join
servers that require PunkBuster; use the x86 client for those. An x64 editor package is a
future goal.

The import listing fails the job if any of those images imports `NovintHFX.dll`, or if
`BF2OpenAL.dll` imports the shared MSVC runtime. The
manifests are only printed. Any `requestedExecutionLevel` turns off UAC file virtualization,
which retail's manifest-less `BF2.exe` gets, so write access to the game folder is the
installer's job.

Each hosted job has its own checkout. Client/plugin and editor builds run sequentially
inside the x86 job because the client's plugin outputs use shared source-tree paths.
Compilation uses two parallel jobs to limit memory usage. CI does not run a game server,
client or editor, and does not exercise gameplay. The Python fixture uses an ephemeral
loopback UDP port; automated server sessions must never use reserved port 16567.

The Windows client links strictly on both x86 and x64. The coordinator measured zero
unresolveds on both with MSVC 14.44 and the matching `dice_py.lib`; the earlier 46
unresolveds were CPython imports from a build missing that library.
`bf2_client_linkcheck` remains a buildable compatibility alias for `bf2_client`.
The x86 server now links strictly: the coordinator measured zero unresolveds with
MSVC 14.44 and the matching Python import library. `bf2_w32ded_linkcheck` remains
a buildable compatibility alias for `bf2_w32ded`, with no separate check executable.
The editor also requires a strict link on both Windows architectures, with the
shared client engine's voice dependency supplied as described in
[Building](building.md#editor). Its new links await VM verification.
`bf2_editor_linkcheck` remains a manual, `EXCLUDE_FROM_ALL` strict companion.
The x86 AI and voice-server DLLs retain independent diagnostic `/FORCE` links;
the executable's strict link does not prove their completeness.
Existing engine frontiers, including `swiff-native-as-host`,
`swiff-host-bodies` and `RW-88`, remain. Green CI establishes only the builds and focused
checks above, not complete symbol recovery, production startup, loading, joining or
playability. CI builds and packages both Windows clients when D3DX is usable;
the x64 editor target exists but is not built by CI. Linux client/editor targets do not exist.

## Packages

`[CHOSEN, not recovered]` A release carries one package per platform, named by the platform
only (owner decision 2026-11-15). The build jobs package each platform's roles separately;
those per-role ZIPs stay CI artifacts for development, and the release packages are assembled
from them.

### Release assets

| Asset | Contents |
| --- | --- |
| `openbf2-macos.dmg` | `OpenBF2.app`: the setup assistant and launcher with the universal (arm64 + x86_64) client; the app carries its own instructions |
| `openbf2-windows.zip` | `x64\` the x64 client, `x86\` the x86 client, `server\` the x86 dedicated server, `editor\` the x86 editor, `licenses\`, `LICENSE`, `README.txt`, `build-info.json` |
| `openbf2-linux.zip` | the x64 dedicated server: `bf2`, `libdice_py.so`, `licenses/`, `LICENSE`, `README.txt`, `build-info.json` |
| `openal-soft-1.25.2.tar.bz2` | the LGPL source of both Windows clients' `BF2OpenAL.dll` ([OpenAL Soft](#openal-soft-in-the-windows-clients)) |

Each asset has a `.sha256` beside it. Releases do not carry the per-architecture Windows ZIPs,
the x64 Windows server (PunkBuster's server library exists only as 32-bit `pbsv.dll`), the
macOS ZIPs or the macOS server. The Windows package's tree:

```text
openbf2-windows/
  x64/       BF2.exe, dice_py.dll, RendDX9.dll, SwiffPlayer.dll, BF2Audio.dll,
             TextureAtlasBuilder.dll, aidll.dll, BF2OpenAL.dll, OpenAL-Soft-NOTICE.txt
  x86/       BF2.exe, dice_py.dll, RendDX9.dll, SwiffPlayer.dll, BF2Audio.dll,
             TextureAtlasBuilder.dll, BF2VoipServer.dll, BF2OpenAL.dll, OpenAL-Soft-NOTICE.txt
  server/    bf2_w32ded.exe, dice_py.dll, AIDLL_w32ded.dll, BF2VoipServer_w32ded.dll
  editor/    BF2Editor.exe, dice_py.dll, RendDX9.dll, TextureAtlasBuilder.dll, BF2VoipServer.dll
  licenses/  Python, zlib, Minizip, TinyXML, RSA MD5, GameSpy; libogg, libvorbis, gameswf
             (clients); FFmpeg (x64 client); OpenAL Soft (clients); ImGui, ImGuizmo (editor)
  LICENSE  README.txt  build-info.json
```

`package_release.py --release windows --packages-dir <dir>` builds it from the four per-role
ZIPs below `<dir>` (searched recursively, so each CI artifact can keep its own folder). It
checks each ZIP against its `.sha256`, its `build-info.json` revision and the SHA-256 of every
runtime file, and refuses unknown entries. Runtime files go to the component's folder. Licence
files are merged into one `licenses/` and must be identical wherever two components carry the
same one. Each client's `OpenAL-Soft-NOTICE.txt` stays beside its `BF2OpenAL.dll`. The OpenAL
Soft tarball must match the SHA-256 in those notices; when both legs supply a copy, the copies
must be identical. The combined `README.txt` merges the per-role READMEs: contents, Visual C++
runtime per architecture, the x64 client's 64-bit `d3dx9_25.dll`, PunkBuster only with the x86
client, CD keys, install commands per folder, the server settings, the editor's dependencies,
the link contracts and the licence list. `build-info.json` records the revision, the source
URL, `windows_signing`, each component (folder, per-role package, link contract) and the
SHA-256 of every runtime file under its folder path. Every input is verified before anything is
written. The `Windows package` job runs it on Ubuntu after both Windows jobs and uploads the
`release-windows` artifact.

`--release linux` does the same for the Linux server in the Linux job (`release-linux`). The
macOS release package is the `macos-app` job's disk image (`release-macos`). The setup build
copies only the client's three files into the app, so the image cannot hold a build-info file;
`--release macos` writes `openbf2-macos.build-info.json` beside it instead (revision, the setup
app's repository and revision, the universal client files' SHA-256, the image's SHA-256, the
signing mode and whether it is notarized and stapled). It is the `build-info-macos` artifact
and is not attached to releases.

#### D3DX in release runs

The Windows release package needs both clients and the editor, so a release run fails at the
D3DX probe when neither D3DX source is usable, with an error annotation, and nothing is
attached. The packager without `--allow-missing` refuses a missing component too, and the
release job checks that `openbf2-windows.zip` holds all four executables and no `omitted` list.
A server-only archive under the same name would quietly ship a different product. The D3DX
fallback lives in the repository (`Code/RendDX9Abi`), so a release run without a D3DX source
points to a repository or configuration fault (for example `BF2_D3DX=sdk`). Fix it and publish
a new tag: a rerun uses the same commit.

Other runs (pull requests, branch pushes, manual dispatch) stay green without D3DX. The
`Windows package` job passes `--allow-missing`: `openbf2-windows.zip` holds the folders that
were built, its README names the missing ones, and `build-info.json` lists them under
`omitted`. Without a client the OpenAL Soft tarball is left out.

### Per-role packages (CI artifacts)

The [packaging helper](../Code/Build/package_release.py) requires every listed runtime
file to exist and be nonempty. It uses explicit manifests rather than copying output
directories, so test executables, import libraries, maps, PDBs, and any locally staged
retail companions are excluded. `--omit ROLE` (repeatable) skips one package of the
selected platform, for example `--omit client`; every other role stays
required. CI passes it for the client and editor only when neither D3DX source is usable.

| ZIP filename | Runtime contents | Release package |
| --- | --- | --- |
| `openbf2-windows-x86-server.zip` | `bf2_w32ded.exe`, `dice_py.dll`, `AIDLL_w32ded.dll`, `BF2VoipServer_w32ded.dll` | `openbf2-windows.zip`, `server\` |
| `openbf2-windows-x86-client.zip` | `BF2.exe`, `dice_py.dll`, `RendDX9.dll`, `SwiffPlayer.dll`, `BF2Audio.dll`, `TextureAtlasBuilder.dll`, `BF2VoipServer.dll`, `BF2OpenAL.dll` (OpenAL Soft) | `openbf2-windows.zip`, `x86\` |
| `openbf2-windows-x86-editor.zip` | `BF2Editor.exe`, `dice_py.dll`, editor `RendDX9.dll`, `TextureAtlasBuilder.dll`, `BF2VoipServer.dll` | `openbf2-windows.zip`, `editor\` |
| `openbf2-windows-x64-server.zip` | `bf2_w64ded.exe`, `dice_py.dll`, `aidll.dll` | none |
| `openbf2-windows-x64-client.zip` | `BF2.exe`, `dice_py.dll`, `RendDX9.dll`, `SwiffPlayer.dll`, `BF2Audio.dll`, `TextureAtlasBuilder.dll`, `aidll.dll`, `BF2OpenAL.dll` (OpenAL Soft); no PunkBuster, so it cannot join servers that require it | `openbf2-windows.zip`, `x64\` |
| `openbf2-linux-x64-server.zip` | `bf2`, `libdice_py.so` | `openbf2-linux.zip` |
| `openbf2-macos-arm64-client.zip`, `openbf2-macos-x86_64-client.zip` | `bf2_client`, `libdice_py.dylib`, `libaidll.dylib` | merged into the app in `openbf2-macos.dmg` |
| `openbf2-macos-arm64-server.zip`, `openbf2-macos-x86_64-server.zip` | `bf2`, `libdice_py.dylib`, `libaidll.dylib` | none |

None of these ZIPs is notarized; release runs sign the macOS binaries with the Developer ID
before packaging, other runs sign them ad hoc.

The Windows x64 client files are staged by CMake into `bin_client`. Both x64
packages require the native `aidll.dll`; the dedicated build stages the same DLL
into `bin_w64ded`. CI prints its exports and compares both copies. The x64 AI link
is strict; MSVC and runtime results await coordinator validation documented in
[Building](building.md). Never load retail x86 AI into an x64 host.
The x64 renderer statically links the FFmpeg-based Bink replacement; its package
includes `FFmpeg-COPYING.LGPLv2.1.txt` alongside the other dependency notices.

### OpenAL Soft in the Windows clients

`[CHOSEN, not recovered]` Retail's `BF2OpenAL.dll` is Creative's 32-bit OpenAL wrapper, renamed.
An x64 process cannot load it. Both Windows client packages, x86 and x64, therefore carry an
unmodified [OpenAL Soft](https://openal-soft.org) build under that name, beside `BF2.exe`, so
both clients use the same provider. `BF2.exe` runs from the package folder and Windows searches
the application folder first, so the x86 client takes the bundled DLL, not the game folder's
retail copy. The default
audio providers ("Software Audio", "Hardware Audio", "+ EAX") load it. OpenAL Soft maps
their legacy device names to its default device and implements the EAX extensions. The
"System OpenAL" rows still load an installed `OpenAL32.dll`.

- **Pin.** `Code/Build/build_openal_soft.py` holds the version (1.25.2), the release tarball
  URL (`openal-soft.org/openal-releases`) and its SHA-256. The tarball's contents match the
  upstream `1.25.2` tag. The script refuses any other download. Changing the pin means editing
  those three constants.
- **Build.** CMake/Ninja with MSVC, in each leg for its architecture: `LIBTYPE=SHARED`, `ALSOFT_EAX=ON`, the static MSVC
  runtime (`FORCE_STATIC_VCRT=ON`, `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`; the script checks
  that the DLL has no VC++ runtime imports), with `ALSOFT_UTILS`, `ALSOFT_EXAMPLES`, `ALSOFT_TESTS`,
  `ALSOFT_INSTALL` and C++20 modules off. `OpenAL32.dll` is copied as `BF2OpenAL.dll`. CI caches
  the result per architecture (`out/build/ci-<arch>/openal-soft`), keyed on the script.
- **LGPL.** Each client package contains `licenses/OpenAL-Soft-COPYING.txt` and
  `OpenAL-Soft-NOTICE.txt` (version, source URL and SHA-256, build options, how to replace the
  DLL); in `openbf2-windows.zip` the notices are `x64\OpenAL-Soft-NOTICE.txt` and
  `x86\OpenAL-Soft-NOTICE.txt`. The packager writes the exact source tarball and its `.sha256`
  beside the ZIP. Both legs build from the same pinned file; the Windows package job checks that
  the copies match and releases attach one.
- **Local builds.** In the developer prompt of the target architecture, run
  `python Code/Build/build_openal_soft.py --arch <x86|x64> --stage-dir <build>/openal-soft
  --work-dir <scratch> --client-dir <build>/bin_client` before packaging that client. Packaging
  a Windows client requires that stage.

Every ZIP and release asset has a matching `.sha256` checksum written with LF line endings on all
platforms, so Linux `sha256sum --check` can verify Windows-built packages too.
Inside a package-named directory are
the binaries, license notices, a runtime README and `build-info.json` recording the
source commit, source URL, link contract and hashes of the runtime files (in the Windows
release package, per component folder). Linux ZIP
entries retain executable permission bits; an extractor that ignores these may need
`chmod +x bf2` after extraction. Linux binaries target the Ubuntu 22.04 system-library
baseline; compiler runtimes use the preset's static-runtime default.

`link_contract` describes the executable. Each x86 README separately identifies the
voice-server DLL's diagnostic link, and the server README identifies the AI DLL's
diagnostic link; the Windows release README lists all of them. The voice DLLs are rebuilt
artifacts, not retail redistributions.

The packaging helper preserves dependency notices from the vendored source. The client
also includes the unchanged upstream COPYING files for [libogg 1.3.6](https://github.com/xiph/ogg/blob/v1.3.6/COPYING)
and [libvorbis 1.3.7](https://github.com/xiph/vorbis/blob/v1.3.7/COPYING), restored beside
their existing vendored code for binary distribution.

These packages require compatible Battlefield 2 1.50 content and game Python scripts.
Windows also needs the matching Visual C++ runtime and, for client/editor use,
separately supplied Bink, D3DX25 and OpenAL components where applicable.
Do not mix the rebuilt host's engine plugins with retail engine plugins. Follow
[the build guide](building.md) for runtime setup and preserve original game images.

Successful CI keeps these artifacts for 14 days: the per-role ZIPs as `packages-windows-x86`,
`packages-windows-x64`, `packages-linux-x64`, `packages-macos-arm64` and `packages-macos-x86_64`;
the release packages as `release-windows`, `release-linux` and `release-macos`; and
`build-info-macos`. Download these from the Actions run. Available failure diagnostics are
retained for seven days; compiler console output remains in the job log. Failed jobs do not
upload packages.

For local packaging after building the matching targets:

```powershell
python Code/Build/package_release.py --platform windows-x86 `
  --build-dir out/build/local-x86 --output-dir out/packages `
  --revision (git rev-parse HEAD) --repository B2BF/OpenBF2
```

Use `windows-x64` or `linux-x64` with the corresponding build directory; on Linux use
`python3`. Run from the repository root and use a fresh output directory when changing
platforms or source revisions. To assemble a release package from per-role ZIPs (both Windows
legs' outputs below one directory for Windows):

```sh
python3 Code/Build/package_release.py --release windows --packages-dir out/packages \
  --output-dir out/release --revision "$(git rev-parse HEAD)" --repository B2BF/OpenBF2
```

Add `--allow-missing` to package an incomplete set for testing; releases never use it.

## Installing a package

Use your own Battlefield 2 **1.50** installation. Packages contain no game content
and do not download any. Each archive's `README.txt` has commands for that specific
platform and its folders. The procedures below are intentional packaging policy
`[CHOSEN, not recovered]`.

### Windows: `openbf2-windows.zip`

1. Extract the archive to `C:\OpenBF2`, which gives `C:\OpenBF2\openbf2-windows` with the
   `x64\`, `x86\`, `server\` and `editor\` folders. Keep every DLL beside its EXE and do not
   move files between folders. Supply the Visual C++ runtime of the folder's architecture
   (x64 for `x64\`; x86 for `x86\`, `server\` and `editor\`) and the client/editor
   dependencies listed below.
2. In Command Prompt, change to your retail game installation, or your 1.50
   dedicated-server installation for the server. This is the folder containing
   `mods\`, `python\` and `pylib-2.3.4.zip`, **not** `mods\bf2`:

   ```bat
   cd /d "C:\Games\Battlefield 2"
   ```

3. Run the appropriate command below. Replace the example installation and package
   paths with your own. Servers use their stock settings as described below.

   ```bat
   "C:\OpenBF2\openbf2-windows\x64\BF2.exe" +modPath mods/bf2 +fullscreen 0
   "C:\OpenBF2\openbf2-windows\x86\BF2.exe" +modPath mods/bf2 +fullscreen 0
   "C:\OpenBF2\openbf2-windows\editor\BF2Editor.exe" +loadMod bf2
   "C:\OpenBF2\openbf2-windows\server\bf2_w32ded.exe" +modPath mods/bf2
   ```

The per-role CI artifacts install the same way from `C:\OpenBF2\<package-name>`; the x64
server artifact's executable is `bf2_w64ded.exe`.
A shortcut uses that command as **Target** and the game/server installation root as
**Start in**. An independently built x64 editor uses the same `+loadMod bf2` procedure;
CI does not package it. `+modPath` selects the mod; it does not relocate the root Python
files. These steps replace no original EXE or DLL. To return to retail, launch the
original executable or shortcut. Do not copy retail engine plugins into the package.
For the x86 client and editor, the original game folder supplies `binkw32.dll` and
`d3dx9_25.dll` (and the x86 client's PunkBuster `pb\` folder) through the working directory
where required. The editor also takes `BF2OpenAL.dll` from there. The x86 client uses its
bundled OpenAL Soft `BF2OpenAL.dll`. No package needs
`NovintHFX.dll`: the Novint Falcon is archived (`Code/Archive/NovintFalcon`). The x64
client uses none of the game folder's 32-bit DLLs. It needs the 64-bit `d3dx9_25.dll`
from the DirectX End-User Runtime ([Native x64 client](building.md#native-x64-client)). Sound
uses the packaged `BF2OpenAL.dll` (OpenAL Soft, see above). The x64 client has no
PunkBuster: for servers that require it, use the x86 client.
Both clients read CD keys from the retail registry (all-users installs) or from a per-user
`%LOCALAPPDATA%\OpenBF2\Registry.dat` ([client registry file](registry-file.md)).
This setup was checked against the code, without a runtime test here.

### macOS: `openbf2-macos.dmg`

Open the disk image and drag `OpenBF2.app` to Applications. Its setup assistant guides the
rest and launches the embedded universal client; the app carries its own instructions.
Release images are Developer ID signed, notarized and stapled.

### macOS per-architecture ZIPs (CI artifacts)

The macOS client and server ZIPs are development artifacts, not release assets.

1. On macOS 11 or later, extract the matching package to
   `~/Games/<package-name>`: `arm64` for Apple silicon, `x86_64` for Intel.
2. Copy `mods/`, `python/` and `pylib-2.3.4.zip` from your existing Windows/Wine
   Battlefield 2 1.50 installation into that folder, beside `bf2_client` or `bf2`.
   Copy `Settings/` and `Profiles/` if present, and `admin/` if using its server
   scripts. Keep the complete `mods/` tree, including ZIP archives and movies.
   Use a case-insensitive volume: the engine lowercases game paths. Preserve file
   and folder names. Keep the bundled `.dylib` files beside the executable.
3. In Terminal, use your extracted package's actual name:

   ```sh
   cd "$HOME/Games/openbf2-macos-arm64-client"
   xattr -dr com.apple.quarantine .
   chmod +x bf2_client
   ```

   For a server use `openbf2-macos-arm64-server` and `chmod +x bf2`; substitute `x86_64`
   for `arm64` on Intel. These ZIPs are never notarized (release runs sign their binaries
   with the Developer ID, other runs ad hoc; the package README says which applies), so
   keep the `xattr` line.
4. Start the client with `./bf2_client +modPath mods/bf2 +fullscreen 0`.
   New user profiles are stored separately in
   `~/Library/Application Support/OpenBF2/Profiles/`; copying the installation's
   `Profiles/` does not migrate user profiles. For joining, configure your own CD
   key in `~/Library/Application Support/OpenBF2/Registry.con` using this format
   (single backslashes, no quotes; keep it private):

   ```text
   HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\Battlefield 2\ergc = YOUR-CD-KEY
   ```

   Use your own unmarked key, not a Windows DPAPI `x9392` value.

   To join Special Forces servers (`+modPath mods/xpack`), also add your Special Forces key.
   Without it, the client drops the join silently and returns to the menu, as retail does:

   ```text
   HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\Battlefield 2 Special Forces\ergc = YOUR-SF-CD-KEY
   ```

   For a server, use the stock settings below, then run:

   ```sh
   ./bf2 +modPath mods/bf2
   ```

### Server configuration and Linux

1. Run from the content root (alongside `mods/` and `python/`) with the server's
   stock `mods/bf2/Settings/ServerSettings.con` and `mods/bf2/Settings/maplist.con`.
   Optional `+config path/to/ServerSettings.con` and `+mapList path/to/maplist.con`
   override these files; paths are relative to the content root. Choose settings,
   maps and ports for your own server. Automated server tests must never use port
   16567; this restriction applies to our tests, not end-user servers. macOS agent tests
   must also use the [offline sandbox](macos-port.md#smoke-configuration).
2. For Linux, extract `openbf2-linux.zip` to `~/Games`, which gives `~/Games/openbf2-linux`
   (the CI artifact `openbf2-linux-x64-server.zip` installs the same way under its own name).
   Copy your 1.50 server's `mods/`, `python/`, `pylib-2.3.4.zip`, and any `Settings/`,
   `Profiles/` and used `admin/` into it. Use Linux server content with lowercase game paths.
   Keep `libdice_py.so` beside `bf2` and run:

   ```sh
   cd "$HOME/Games/openbf2-linux"
   chmod +x bf2
   ./bf2 +modPath mods/bf2
   ```

## Publishing a release

1. Commit and push the workflows and packaging helper along with the intended source.
2. Create a release tag containing those files and publish a GitHub Release for it.
   Publishing either a regular release or a prerelease triggers the workflow; merely
   saving a draft does not. A tag push by itself starts no workflow.
3. The workflow rebuilds that tagged commit, then, only after every platform succeeds,
   attaches exactly the [release assets](#release-assets): `openbf2-macos.dmg`,
   `openbf2-windows.zip`, `openbf2-linux.zip` and the OpenAL Soft source tarball, each with
   its `.sha256`. It downloads only the `release-*` artifacts, checks every checksum, refuses
   any other file, and checks that the Windows package holds both clients, the server and the
   editor ([D3DX in release runs](#d3dx-in-release-runs)). Re-running the release workflow
   replaces the same named assets on that release; it never creates or edits a release's notes.
   Assets of earlier releases under other names (per-role ZIPs, `openbf2-macos-app.dmg`) stay
   until deleted by hand.

[Workflow reruns](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/re-run-workflows-and-jobs)
use the original commit. Pushing a packaging fix to the default branch does not change
an existing release run; a new release tag must include the fix. For manual recovery of
older artifacts with CRLF checksums, convert only the checksum files to LF, verify every
asset with `sha256sum --check`, then upload the asset/checksum pairs to the existing release.

The build jobs have `contents: read`. Only the asset-upload job has `contents: write`,
using the built-in `GITHUB_TOKEN`; no personal token is needed. The macOS signing
secrets are confined to the `release` environment (next section). Actions
are pinned to verified release commit SHAs. Release tags are passed through environment variables
instead of being inserted into shell code.

This publication-triggered design requires mutable release assets. If the repository
enables immutable releases, change the process to build and attach assets to a draft
before publishing: GitHub prohibits adding/replacing assets after an immutable release
is published. See [GitHub's release events](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#release)
and [immutable releases](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases).

## macOS signing and notarization

Release runs sign the macOS binaries with a Developer ID and notarize the disk image;
every other run keeps the ad-hoc signature and uses no secrets. A run is a release run when the
event is a published release or the ref is a `v*` tag (`BF2_RELEASE_SIGNING` in
[build.yml](../.github/workflows/build.yml)). Only then do the `macos` and `macos-app`
jobs use the protected environment `release`; pull requests, branch pushes and forks
run without an environment and never see its secrets. A release run with missing
secrets fails instead of shipping ad-hoc binaries.

`build.yml` is a reusable workflow, and a reusable workflow only receives environment
secrets when its caller passes secrets on, so `ci.yml` and `release.yml` call it with
`secrets: inherit`. A v* tag is signed in CI only through a manual dispatch on that tag;
publishing a release or prerelease is the normal way.

Secrets of the `release` environment (Settings > Environments > release):

| Secret | Content |
| --- | --- |
| `MACOS_CERT_P12` | base64 of the exported "Developer ID Application" certificate with its private key (.p12) |
| `MACOS_CERT_PASSWORD` | password of that .p12 |
| `NOTARY_KEY_P8` | base64 of an App Store Connect API key (.p8, Developer access or higher); `macos-app` only |
| `NOTARY_KEY_ID` | that key's ID; `macos-app` only |
| `NOTARY_ISSUER_ID` | the issuer ID shown with the App Store Connect API keys; `macos-app` only |

Restrict the environment's deployment branches and tags to the tag pattern `v*`.
Required reviewers are optional; with them, each release run waits for an approval
of the two `macos` legs and again for `macos-app`.

[tools/macos/ci/release-signing.sh](../tools/macos/ci/release-signing.sh) does the work.
It imports the .p12 into a temporary keychain (random password, added to the search
list) and writes the API key to a file under `RUNNER_TEMP`; a final `if: always()`
step deletes both. Secret values are passed to the steps that need them only.

- **Standalone packages** (`macos` job): after the build, `libdice_py.dylib`,
  `libaidll.dylib`, `bf2` and `bf2_client` are signed with the hardened runtime and a
  secure timestamp, `bf2_client` with
  [bf2_client.entitlements](../Code/Build/macos/bf2_client.entitlements). The ZIPs are
  packaged with `--macos-signing developer-id` (README and `build-info.json` say so).
  They are CI artifacts, not release assets, so they are not notarized: the job imports
  only the certificate, no notary key, and makes no Apple submission. The signed client
  files are what the `macos-app` job merges into the app.
- **App** (`macos-app` job): the setup repository's `build-release.sh` gets
  `SETUP_SIGN_IDENTITY` (the certificate's SHA-1), `SETUP_ENTITLEMENTS_CLIENT` (the
  file above) and `SETUP_ENTITLEMENTS_APP` (the setup repository's own file) and signs
  the dylibs, `bf2_client` and the app with the hardened runtime and a timestamp. The
  job then checks the image's app (Developer ID, one Team ID, runtime, timestamp,
  audio-input entitlement), signs the disk image, notarizes it, staples and validates
  the ticket, runs `spctl -a -t open --context context:primary-signature` and writes
  the `.sha256` last, after stapling. It then writes `openbf2-macos.build-info.json`
  (`--macos-signing developer-id`: notarized and stapled) as the `build-info-macos`
  artifact.

Any notarization status other than `Accepted` fails the job and prints `notarytool log`.

Entitlements: the client needs only `com.apple.security.device.audio-input` (voice
chat). It generates no code at run time and reads no `DYLD_*` variables, and both of
its libraries carry the same Team ID, so library validation stays on. The dedicated
server and the libraries get none; entitlements on a dylib have no effect.

Rotating the certificate or key: create the new Developer ID Application certificate
(or API key), export it, base64-encode it (`base64 -i cert.p12 | pbcopy`) and replace
the environment secrets; nothing in the repository changes. Let an old certificate
expire rather than revoking it, unless its key was exposed: revoking can make Gatekeeper
reject software it already signed.

## Windows signing later

Windows binaries are currently unsigned by this pipeline. The build workflow marks
the place for a future signing step: after all Windows outputs and DLLs are staged,
before ZIP creation and checksums. Signing must cover each packaged EXE/DLL, verify
the resulting signatures, and update the package metadata/README accordingly. Keep
signing credentials confined to trusted release jobs when that feature is introduced.
