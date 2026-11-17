# Linux client distribution

The Linux application combines the native SDL3/DXVK client with the graphical
installer and launcher from `kr4uzi/openbf2-setup`. This is intentional build and
distribution policy under [issue #36](https://github.com/kr4uzi/OpenBF2/issues/36#issuecomment-6932201666),
not a recovery claim. Game behavior, protocols and content formats are unchanged.

The application package is `openbf2-linux-x86_64-client.tar.gz`. The existing
`openbf2-linux.zip` dedicated-server package keeps its name and contents. Linux
ARM64, native musl, AppImage/Flatpak wrappers and Gaming Mode certification are
not established by this job; the portable application is the input for later
wrapper validation.

## Build boundary

The client leg of the `linux` matrix in [build.yml](../.github/workflows/build.yml) checks out
the event's exact engine commit and pins setup to
`3d3e6b4313f8ca52748b05391d87b6d7a6aa127d` (`v0.4.0`). That setup revision includes the Qt
frontend, SDL microphone backend, explicit build-root audits and isolation of
diagnostic scratch configuration from the production application, plus the
GCC 15 text-reader lookup fix and Complete Edition support. Its
[artifact contract](https://github.com/kr4uzi/openbf2-setup/blob/3d3e6b4313f8ca52748b05391d87b6d7a6aa127d/docs/linux-distribution.md)
is checked by setup's own packager; this repository supplies its engine and
runtime inputs rather than duplicating the installer.

The baseline is generic x86-64 and glibc 2.35, built in an Ubuntu 22.04 OCI image.
The SDK uses GCC 15.2.0, Rust 1.90.0 and Boost 1.89.0. The engine retains C++14
with its existing isolated C++17 effect libraries; setup uses its own C++23
contract. These are separate build requirements, not requirements to install
compilers on the user's machine.

[Containerfile](../tools/linux/release/Containerfile),
[sdk-sources.json](../tools/linux/release/sdk-sources.json) and
[build-sdk.py](../tools/linux/release/build-sdk.py) define the pinned SDK inputs.
Compiler/runtime packages and source-built dependencies carry their actual
versions and hashes; a compiler version does not imply the same package version
for its dynamically linked runtime. All shared components come from the same
baseline builder. GPU drivers, libc and the explicit host window-system boundary
remain supplied by the supported host.
The SDK builds GNU readelf 2.46 to inspect SDL's standardized FDO dependency
notes, which the baseline OS's older reader rejects. The existing linker and
strip tools continue to build and stage the libraries. SDK construction checks
that every installed runtime ELF's notes can be inspected before packaging.
The host also needs a functioning desktop and audio environment, including its
ALSA configuration and device services; bundling the client libraries does not
install those services.

CI caches the exported SDK OCI archive by its recipe and source pins. The
completed SDK is saved before client compilation, so a later client/setup
failure does not discard the successful dependency build. The
[identity helper](../tools/linux/release/sdk-image.py) verifies the archive's
manifest and image configuration and rejects ambiguous metadata. Both produced
artifacts record the OCI manifest digest; the image configuration digest is used
only to select that local image for execution. No SDK registry publication or
registry credential is required. A changed SDK recipe invalidates the cache.

## Engine and runtime inputs

[build-client.sh](../tools/linux/release/build-client.sh) runs in the SDK. It
requires clean committed engine and setup checkouts and fresh output directories.
It builds the patched native effect dependencies, configures `linux-client`,
strictly links `bf2_client`, and runs `bf2_linux_client_checks` without game media.
It enables `BF2_FRONTIER_DEFAULT_CONTINUE`, matching the existing Windows/macOS
release packaging policy for the plugin kit's diagnostic trap. An explicit
`BF2_RENDDX9_FRONTIER=abort` still arms that trap; ordinary development builds
retain the abort default. This does not implement any remaining frontier.

[stage-artifacts.py](../tools/linux/release/stage-artifacts.py) then creates two
inputs using the SDK's provenance catalog:

| Input | Ownership |
| --- | --- |
| Engine | `bin/bf2_client`, Python/AI libraries, DXVK Native, SDL3, OpenAL, dynamic audio roots and their non-host dependency closure; engine notices and provenance |
| Setup runtime | Qt libraries/platform plugins, SDL and the corresponding non-host dependency closure; runtime notices and provenance, including the statically used Boost and Rust SDK inputs |

The stager inspects ELF metadata without executing the inputs or using `ldd`.
It includes explicitly loaded libraries such as DXVK/OpenAL/PulseAudio/ALSA,
since executable link dependencies alone do not identify them. SONAME aliases
are materialized as ordinary files. Staging copies are stripped and receive
relative `$ORIGIN` runtime paths; original build products remain untouched.
Files shared by the two inputs must have identical bytes.

Setup's auditor verifies architecture, glibc ceiling, symbol-version providers,
library closure, relative loader paths, file inventories and dependency
provenance. `SETUP_ARTIFACT_BUILD_ROOTS_FILE` supplies a local JSON list of actual
engine/SDK source and build roots. Setup adds its own source/build/SDK roots.
These concrete prefixes detect build leakage while allowing legitimate runtime
temporary-file paths used by SDL and ALSA. The roots file is not part of either
artifact or the final application.

The build fetches setup's locked Cargo inputs into the fresh setup checkout's
ignored `out/linux-release-cargo` cache, beneath its Rust source-prefix remapping,
before invoking its offline release entry point. Setup compiles/tests the GUI, embeds the engine, installs into a
staging tree, audits the combined package, and runs offscreen `--check-client`
and GUI smoke checks. It writes the archive and a filename-relative SHA-256
sidecar. The release workflow checks that checksum and the expected application
files and baseline metadata before attaching the asset.

GitHub groups the matrix entries as **Linux x64 server** and **Linux x64 client**.
Their `packages-linux-x64` and `packages-linux-client` artifacts feed the
downstream **Linux package** job. That job assembles the server ZIP and verifies
the unchanged client archive's checksum, engine/setup revisions and baseline,
then publishes separate `release-linux` and `release-linux-client` artifacts.
Both matrix legs must succeed before this aggregation runs.

## Local reproduction

Use a separate committed checkout for release validation. The sibling setup
checkout may contain owner notes or ongoing changes; do not clean or overwrite it.
Pin a fresh checkout to the setup revision above. Build and export the SDK:

```sh
podman build --format oci --layers \
  --build-arg SDK_SOURCE_REVISION=<engine-commit> \
  --tag localhost/openbf2-linux-sdk:ci \
  --file tools/linux/release/Containerfile tools/linux/release
podman save --format oci-archive --output /absolute/scratch/sdk.oci \
  localhost/openbf2-linux-sdk:ci
python3 tools/linux/release/sdk-image.py /absolute/scratch/sdk.oci
```

Use the resulting `image_id` to run the container and `reference` as the fourth
build argument. Bind the engine, setup and scratch directories explicitly; the
result directory must not exist yet:

```sh
podman run --rm \
  --volume /absolute/engine:/work/engine:rw \
  --volume /absolute/setup:/work/setup:rw \
  --volume /absolute/scratch:/work/release:rw \
  <image_id> bash /work/engine/tools/linux/release/build-client.sh \
  /work/engine /work/setup /work/release/result <reference>
```

The first SDK build is expensive because it compiles graphics and GUI
dependencies. Subsequent builds can reuse the unchanged exported image. Build
logs and cache artifacts belong in isolated scratch/output locations, never in
game content or source directories.

## User files and validation limits

Extract the archive into a user-owned application directory and start
`OpenBF2/bin/OpenBF2`. Its optional `--install-desktop` action installs a user
menu entry. Users supply supported BF2 media and patches; neither the archive
nor CI contains game content, profiles or keys. Setup `v0.4.0` supports Complete
Edition media and includes its content-verification manifest; the installer
remains responsible for media recognition and patch validation.

Application replacement preserves `$XDG_DATA_HOME/OpenBF2` (normally
`~/.local/share/OpenBF2`). Profiles, keys, setup records and writable game-data
generations remain there. The native effect cache uses
`$XDG_CACHE_HOME/openbf2/FxNative`, normally `~/.cache/openbf2/FxNative`. The
launcher runs the client with the selected writable content generation as its
working directory. No original retail installation is overwritten.

Build and package checks establish the artifact contract and selected CPU/UI
paths. They do not establish a real-media installation, hardware microphone
capture, GPU rendering, joining a server or gameplay on every Linux system.
The development client has the Steam Deck local-bot playtest recorded in the
[Linux guide](linux-vulkan-client.md); repeat acceptance with the produced
release package and record its checksum, revisions, OS, display backend and GPU.
Any agent-run local-server test must explicitly use an unused port other than
16567. Windows/macOS SDK gates remain separate from Linux packaging checks.
