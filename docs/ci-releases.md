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
| Windows 2022, MSVC x86 | `msvc-x86` / `Reconstruction` | Python interpreter check; `bf2_run_checks`; `bf2_editor_run_checks`; `bf2_w32ded`; `bf2_aidll`; with a usable D3DX source: `bf2_client` and plugins, `bf2_editor` and plugins |
| Windows 2022, MSVC x64 | `msvc-x64` / `Release` | Python interpreter check; strict `bf2_w64ded` and `bf2_aidll` links; AI export listing and staged-copy comparison; with a usable D3DX source: `bf2_client` and plugins |
| Ubuntu 22.04, GCC x64 | `linux-x64` / `RelWithDebInfo` | Strict `bf2` link; `dice_py_check_run` |

Windows builds initialize MSVC through the repository's `vsdevcmd.cmd`. They build
Python into an architecture-specific output directory before CMake configuration,
then pass its DLL/import-library paths explicitly. Linux installs CMake, Ninja,
GCC and ncurses headers and builds Python through CMake. No game installation or
retail binaries are required for compilation.

The client and editor renderer plugins need the
[April 2005 DirectX SDK or the D3DX fallback](building.md#d3dx-without-the-directx-sdk).
Both Windows jobs probe with `Code\Build\dxsdk.cmd <arch> fallback`. When neither
source is usable, the job posts a notice, skips the client (and the editor on x86),
and packages only its server. Otherwise it builds and packages those images.

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
playability. CI builds and packages the Windows x64 client when D3DX is usable;
the x64 editor target exists but is not built by CI. Linux client/editor targets do not exist.

## Packages

The [packaging helper](../Code/Build/package_release.py) requires every listed runtime
file to exist and be nonempty. It uses explicit manifests rather than copying output
directories, so test executables, import libraries, maps, PDBs, and any locally staged
retail companions are excluded. `--omit ROLE` (repeatable) skips one package of the
selected platform, for example `--omit client`; every other role stays
required. CI passes it for the client and editor only when neither D3DX source is usable.

| ZIP filename | Runtime contents |
| --- | --- |
| `openbf2-windows-x86-server.zip` | `bf2_w32ded.exe`, `dice_py.dll`, `AIDLL_w32ded.dll`, `BF2VoipServer_w32ded.dll` |
| `openbf2-windows-x86-client.zip` | `BF2.exe`, `dice_py.dll`, `RendDX9.dll`, `SwiffPlayer.dll`, `BF2Audio.dll`, `TextureAtlasBuilder.dll`, `BF2VoipServer.dll` |
| `openbf2-windows-x86-editor.zip` | `BF2Editor.exe`, `dice_py.dll`, editor `RendDX9.dll`, `TextureAtlasBuilder.dll`, `BF2VoipServer.dll` |
| `openbf2-windows-x64-server.zip` | `bf2_w64ded.exe`, `dice_py.dll`, `aidll.dll` |
| `openbf2-windows-x64-client.zip` | `BF2.exe`, `dice_py.dll`, `RendDX9.dll`, `SwiffPlayer.dll`, `BF2Audio.dll`, `TextureAtlasBuilder.dll`, `aidll.dll` |
| `openbf2-linux-x64-server.zip` | `bf2`, `libdice_py.so` |
| `openbf2-macos-arm64-client.zip`, `openbf2-macos-x86_64-client.zip` | `bf2_client`, `libdice_py.dylib`, `libaidll.dylib` |
| `openbf2-macos-arm64-server.zip`, `openbf2-macos-x86_64-server.zip` | `bf2`, `libdice_py.dylib`, `libaidll.dylib` |

The Windows x64 client files are staged by CMake into `bin_client`. Both x64
packages require the native `aidll.dll`; the dedicated build stages the same DLL
into `bin_w64ded`. CI prints its exports and compares both copies. The x64 AI link
is strict; MSVC and runtime results await coordinator validation documented in
[Building](building.md). Never load retail x86 AI into an x64 host.
The x64 renderer statically links the FFmpeg-based Bink replacement; its package
includes `FFmpeg-COPYING.LGPLv2.1.txt` alongside the other dependency notices.

Every ZIP has a matching `.zip.sha256` checksum written with LF line endings on all
platforms, so Linux `sha256sum --check` can verify Windows-built packages too.
Inside a package-named directory are
the binaries, license notices, a runtime README and `build-info.json` recording the
source commit, source URL, link contract and hashes of the runtime files. Linux ZIP
entries retain executable permission bits; an extractor that ignores these may need
`chmod +x bf2` after extraction. Linux binaries target the Ubuntu 22.04 system-library
baseline; compiler runtimes use the preset's static-runtime default.

`link_contract` describes the executable. Each x86 README separately identifies the
voice-server DLL's diagnostic link, and the server README identifies the AI DLL's
diagnostic link. The voice DLLs are rebuilt artifacts, not retail redistributions.

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

## Installing a package

Use your own Battlefield 2 **1.50** installation. Packages contain no game content
and do not download any. Each archive's `README.txt` has commands for that specific
platform and role. The procedures below are intentional packaging policy
`[CHOSEN, not recovered]`.

### Windows client, server and editor (x86 and x64)

1. Extract the package to `C:\OpenBF2\<package-name>`, keeping every packaged DLL
   beside its EXE. Keep each architecture and role in its own folder. Supply the
   matching Visual C++ runtime and applicable client/editor dependencies listed above.
2. In Command Prompt, change to your retail game installation, or your 1.50
   dedicated-server installation for a server. This is the folder containing
   `mods\`, `python\` and `pylib-2.3.4.zip`, **not** `mods\bf2`:

   ```bat
   cd /d "C:\Games\Battlefield 2"
   ```

3. Run the appropriate command below. Replace the example installation and package
   paths with your own. Servers use their stock settings as described below.

   ```bat
   "C:\OpenBF2\openbf2-windows-x86-client\BF2.exe" +modPath mods/bf2 +fullscreen 0
   "C:\OpenBF2\openbf2-windows-x64-client\BF2.exe" +modPath mods/bf2 +fullscreen 0
   "C:\OpenBF2\openbf2-windows-x86-editor\BF2Editor.exe" +loadMod bf2
   "C:\OpenBF2\openbf2-windows-x86-server\bf2_w32ded.exe" +modPath mods/bf2
   "C:\OpenBF2\openbf2-windows-x64-server\bf2_w64ded.exe" +modPath mods/bf2
   ```

A shortcut uses that command as **Target** and the game/server installation root as
**Start in**. An independently built x64 editor uses the same `+loadMod bf2` procedure;
CI does not package it. `+modPath` selects the mod; it does not relocate the root Python
files. These steps replace no original EXE or DLL. To return to retail, launch the
original executable or shortcut. Do not copy retail engine plugins into the package.
The original game folder supplies `binkw32.dll`, `d3dx9_25.dll`, `NovintHFX.dll`
and the OpenAL DLLs through the working directory where required (Bink/Novint
are x86 only). This setup was checked against the code, without a runtime test here.

### macOS client and server (arm64 and x86_64)

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

   For a server use `openbf2-macos-arm64-server` and `chmod +x bf2`; substitute
   `x86_64` for `arm64` on Intel. Release packages are Developer ID signed and
   notarized, so the `xattr` line is only needed for CI artifacts, which are
   ad-hoc signed (the package README says which applies).
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
2. For Linux, extract to `~/Games/openbf2-linux-x64-server`. Copy your 1.50 server's
   `mods/`, `python/`, `pylib-2.3.4.zip`, and any `Settings/`, `Profiles/` and used
   `admin/` into it. Use Linux server content with lowercase game paths.
   Keep `libdice_py.so` beside `bf2` and run:

   ```sh
   cd "$HOME/Games/openbf2-linux-x64-server"
   chmod +x bf2
   ./bf2 +modPath mods/bf2
   ```

## Publishing a release

1. Commit and push the workflows and packaging helper along with the intended source.
2. Create a release tag containing those files and publish a GitHub Release for it.
   Publishing either a regular release or a prerelease triggers the workflow; merely
   saving a draft does not. A tag push by itself starts no workflow.
3. The workflow rebuilds that tagged commit, then attaches the ZIP/checksum pairs
   only after every platform succeeds: the server and macOS client packages always,
   and the Windows x86 client/editor and x64 client packages when D3DX was usable. A missing
   client or editor package is reported as a notice. Re-running the release workflow replaces the
   same named assets on that release; it never creates or edits a release's notes.

[Workflow reruns](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/re-run-workflows-and-jobs)
use the original commit. Pushing a packaging fix to the default branch does not change
an existing release run; a new release tag must include the fix. For manual recovery of
older artifacts with CRLF checksums, convert only the checksum files to LF, verify every
ZIP with `sha256sum --check`, then upload the ZIP/checksum pairs to the existing release.

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

Release runs sign the macOS binaries with a Developer ID and notarize them; every other
run keeps the ad-hoc signature and uses no secrets. A run is a release run when the
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
| `NOTARY_KEY_P8` | base64 of an App Store Connect API key (.p8, Developer access or higher) |
| `NOTARY_KEY_ID` | that key's ID |
| `NOTARY_ISSUER_ID` | the issuer ID shown with the App Store Connect API keys |

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
  packaged with `--macos-signing developer-id` (README and `build-info.json` say so)
  and each is submitted with `notarytool --wait`. A ZIP cannot carry a stapled ticket;
  Gatekeeper fetches it online on first launch.
- **App** (`macos-app` job): the setup repository's `build-release.sh` gets
  `SETUP_SIGN_IDENTITY` (the certificate's SHA-1), `SETUP_ENTITLEMENTS_CLIENT` (the
  file above) and `SETUP_ENTITLEMENTS_APP` (the setup repository's own file) and signs
  the dylibs, `bf2_client` and the app with the hardened runtime and a timestamp. The
  job then checks the image's app (Developer ID, one Team ID, runtime, timestamp,
  audio-input entitlement), signs the disk image, notarizes it, staples and validates
  the ticket, runs `spctl -a -t open --context context:primary-signature` and writes
  the `.sha256` last, after stapling.

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
