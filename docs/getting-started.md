# Getting started

BF2Rebuilt is an incomplete reconstruction of the Battlefield 2 1.50 engine. Start by
compiling the recovered source and running focused checks. An executable produced with
`/FORCE` can still contain unresolved calls; its existence does not establish that a
server, client, or editor session will complete.

This code builds standalone, however to build the client/editor you will need the DirectX 9 sdk.

## Pick your starting point

| Work | Build | Read next |
| --- | --- | --- |
| Recover engine behavior or verify retail layouts | MSVC x86 | [Windows x86](building.md#windows-x86) |
| Work on the native Windows dedicated server | MSVC x64 | [Windows x64](building.md#windows-x64) |
| Work on the native Linux dedicated server | Linux x86-64 | [Linux and WSL](building.md#linux-and-wsl) |
| Work on the rebuilt client or its DLLs | MSVC x86, native plugin ABI | [Client and plugins](building.md#client-and-plugins) |
| Work on editing tools | MSVC x86 | [Editor](building.md#editor) and [console commands](editor-console.md) |

The 64-bit servers use native pointer-bearing layouts. Verify retail class sizes and
offsets with x86. A plugin's **native** flavor means the rebuilt host's C++ ABI; it does
not mean the plugin is 64-bit.

## First successful check

Install MSVC with the Windows SDK, CMake 3.21 or newer, and Ninja. From the repository
root in an **x86 Visual Studio developer terminal**:

```powershell
cmake --preset msvc-x86 -B out/build/local-x86
cmake --build out/build/local-x86 --target bf2_recovered
cmake --build out/build/local-x86 --target bf2_run_checks
```

This compiles the engine archive and runs focused bitstream, ABI, object serialization,
and object/physics checks. It does not need a running server. For executable dependencies,
all platforms, output locations, and runtime setup, follow the [build guide](building.md).

The batch helpers under `Code\Build` and `Code\Tests` enter the Visual Studio environment
through `Code\Build\vsdevcmd.cmd`. Set the `BF2_VSDEVCMD` environment variable to your own
`VsDevCmd.bat` path, or let it auto-detect with `vswhere`; see the
[Windows prerequisites](building.md#windows-prerequisites).

## Find your way around

Read the [architecture](architecture.md) to trace startup, content loading, and image
boundaries. Then read the [development guide](development.md) and [AGENTS.md](../AGENTS.md)
before changing engine behavior. They explain evidence requirements, tool access for
agents, validation, and the human review required before submitting changes.

Use a separate build directory for each architecture and contributor. Keep the original
game installation as reference content, and keep rebuilt outputs outside it. Server tests
need a separate configuration and an unused port; keep **16567** free for interactive testing.
