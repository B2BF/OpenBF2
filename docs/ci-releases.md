# CI and release packages

[CI](../.github/workflows/ci.yml) runs on every branch/tag push, pull request update,
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
| Windows 2022, MSVC x86 | `msvc-x86` / `Reconstruction` | Python interpreter check; `bf2_run_checks`; `bf2_editor_run_checks`; `bf2_w32ded`; `bf2_aidll`; with the April 2005 DirectX SDK only: `bf2_client` and plugins, `bf2_editor` and plugins |
| Windows 2022, MSVC x64 | `msvc-x64` / `Release` | Python interpreter check; strict `bf2_w64ded` link |
| Ubuntu 22.04, GCC x64 | `linux-x64` / `RelWithDebInfo` | Strict `bf2` link; `dice_py_check_run` |

Windows builds initialize MSVC through the repository's `vsdevcmd.cmd`. They build
Python into an architecture-specific output directory before CMake configuration,
then pass its DLL/import-library paths explicitly. Linux installs CMake, Ninja,
GCC and ncurses headers and builds Python through CMake. No game installation or
retail binaries are required for compilation.

The client and editor renderer plugins need the
[April 2005 DirectX SDK](building.md#directx-sdk), which GitHub-hosted runners do not
provide. The x86 job probes for it with `Code\Build\dxsdk.cmd`. Without it, the job
posts a notice, skips `bf2_client` and `bf2_editor`, and packages only the x86 server.
A runner with that SDK installed builds and packages both.

Each hosted job has its own checkout. Client/plugin and editor builds run sequentially
inside the x86 job because the client's plugin outputs use shared source-tree paths.
Compilation uses two parallel jobs to limit memory usage. CI does not run a game server,
client or editor, and does not exercise gameplay. The Python fixture uses an ephemeral
loopback UDP port; automated server sessions must never use reserved port 16567.

The x86 executables and AI library retain their existing diagnostic `/FORCE` links.
`bf2_client_linkcheck` and `bf2_editor_linkcheck` remain manual strict-link worklists;
they are not CI gates. Existing engine frontiers, including `swiff-native-as-host`,
`swiff-host-bodies` and `RW-88`, remain. Green CI establishes only the builds and focused
checks above, not complete symbol recovery, production startup, loading, joining or
playability. Linux client/editor and Windows x64 client/editor targets do not exist yet.

## Packages

The [packaging helper](../Code/Build/package_release.py) requires every listed runtime
file to exist and be nonempty. It uses explicit manifests rather than copying output
directories, so test executables, import libraries, maps, PDBs, and any locally staged
retail companions are excluded. `--omit ROLE` (repeatable) skips one package of the
selected platform, for example `--omit client-diagnostic`; every other role stays
required. CI passes it for the client and editor only when the DirectX SDK is absent.

| ZIP filename | Runtime contents |
| --- | --- |
| `openbf2-windows-x86-server-diagnostic.zip` | `bf2_w32ded.exe`, `dice_py.dll`, `AIDLL_w32ded.dll` |
| `openbf2-windows-x86-client-diagnostic.zip` | `BF2.exe`, `dice_py.dll`, `RendDX9.dll`, `SwiffPlayer.dll`, `BF2Audio.dll`, `TextureAtlasBuilder.dll` |
| `openbf2-windows-x86-editor-diagnostic.zip` | `BF2Editor.exe`, `dice_py.dll`, editor `RendDX9.dll` and `TextureAtlasBuilder.dll` |
| `openbf2-windows-x64-server.zip` | `bf2_w64ded.exe`, `dice_py.dll` |
| `openbf2-linux-x64-server.zip` | `bf2`, `libdice_py.so` |

Every ZIP has a matching `.zip.sha256` checksum written with LF line endings on all
platforms, so Linux `sha256sum --check` can verify Windows-built packages too.
Inside a package-named directory are
the binaries, license notices, a runtime README and `build-info.json` recording the
source commit, source URL, link contract and hashes of the runtime files. Linux ZIP
entries retain executable permission bits; an extractor that ignores these may need
`chmod +x bf2` after extraction. Linux binaries target the Ubuntu 22.04 system-library
baseline; compiler runtimes use the preset's static-runtime default.

The packaging helper preserves dependency notices from the vendored source. The client
also includes the unchanged upstream COPYING files for [libogg 1.3.6](https://github.com/xiph/ogg/blob/v1.3.6/COPYING)
and [libvorbis 1.3.7](https://github.com/xiph/vorbis/blob/v1.3.7/COPYING), restored beside
their existing vendored code for binary distribution.

These packages require compatible Battlefield 2 1.50 content and game Python scripts.
Windows also needs the matching Visual C++ runtime and, for client/editor use,
separately supplied Bink, Novint, D3DX25 and OpenAL components where applicable.
Do not mix the rebuilt host's engine plugins with retail engine plugins. Follow
[the build guide](building.md) for runtime setup and preserve original game images.

Successful CI uploads `packages-windows-x86`, `packages-windows-x64` and
`packages-linux-x64` artifacts for 14 days. Download these from the Actions run to get
the ZIP/checksum pairs. Available failure diagnostics are retained for seven days;
compiler console output remains in the job log. Failed jobs do not upload packages.

For local packaging after building the matching targets:

```powershell
python Code/Build/package_release.py --platform windows-x86 `
  --build-dir out/build/local-x86 --output-dir out/packages `
  --revision (git rev-parse HEAD) --repository B2BF/OpenBF2
```

Use `windows-x64` or `linux-x64` with the corresponding build directory; on Linux use
`python3`. Run from the repository root and use a fresh output directory when changing
platforms or source revisions.

## Publishing a release

1. Commit and push the workflows and packaging helper along with the intended source.
2. Create a release tag containing those files and publish a GitHub Release for it.
   Publishing either a regular release or a prerelease triggers the workflow; merely
   saving a draft does not. A tag push by itself runs CI without attaching release assets.
3. The workflow rebuilds that tagged commit, then attaches the ZIP/checksum pairs
   only after every platform succeeds: the three server packages always, and the x86
   client and editor packages when the build runner had the DirectX SDK. A missing
   client or editor package is reported as a notice. Re-running the release workflow replaces the
   same named assets on that release; it never creates or edits a release's notes.

[Workflow reruns](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/re-run-workflows-and-jobs)
use the original commit. Pushing a packaging fix to the default branch does not change
an existing release run; a new release tag must include the fix. For manual recovery of
older artifacts with CRLF checksums, convert only the checksum files to LF, verify every
ZIP with `sha256sum --check`, then upload the ZIP/checksum pairs to the existing release.

The build jobs have `contents: read`. Only the asset-upload job has `contents: write`,
using the built-in `GITHUB_TOKEN`; no personal token or signing secret is needed. Actions
are pinned to verified release commit SHAs. Release tags are passed through environment variables
instead of being inserted into shell code.

This publication-triggered design requires mutable release assets. If the repository
enables immutable releases, change the process to build and attach assets to a draft
before publishing: GitHub prohibits adding/replacing assets after an immutable release
is published. See [GitHub's release events](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#release)
and [immutable releases](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases).

## Windows signing later

Windows binaries are currently unsigned by this pipeline. The build workflow marks
the place for a future signing step: after all Windows outputs and DLLs are staged,
before ZIP creation and checksums. Signing must cover each packaged EXE/DLL, verify
the resulting signatures, and update the package metadata/README accordingly. Keep
signing credentials confined to trusted release jobs when that feature is introduced.
