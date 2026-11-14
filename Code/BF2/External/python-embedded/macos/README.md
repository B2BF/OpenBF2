# macOS embedded Python

This directory holds the Darwin `pyconfig.h` for `libdice_py.dylib`, the embedded
CPython 2.3.4 of the macOS dedicated-server port. The port is an intentional,
owner-requested platform change, `[CHOSEN, not recovered]`: there was no retail
macOS server. The build description itself is shared with Linux and lives in
[../linux/CMakeLists.txt](../linux/CMakeLists.txt); it selects this directory's
`pyconfig.h` when CMake runs on Darwin. See
[docs/macos-port.md](../../../../../docs/macos-port.md) for the port as a whole.

## Build and check

From the repository root, the root project builds the library beside `bf2`:

```sh
cmake --preset macos-x64
cmake --build out/build/macos-x64 --target dice_py dice_py_check_run
```

For both slices, use `cmake --preset macos-universal` and build
`out/build/macos-universal` instead. This `[CHOSEN, not recovered]` preset passes
`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`. The `VA_LIST_IS_ARRAY` override is selected
by compiler architecture macros separately for each slice; it does not depend on
the configure host. Native arm64 execution still requires the owner's M2 run.

Standalone, as on Linux:

```sh
cmake -S Code/BF2/External/python-embedded/linux -B out/build/dice_py-macos \
      -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_SYSROOT=macosx
cmake --build out/build/dice_py-macos --target dice_py_check_run
```

The library's install name is `@rpath/libdice_py.dylib`; `bf2` and `dice_py_check`
carry an `@loader_path` runtime search path, the Mach-O equivalent of `$ORIGIN`.

## Differences from the Linux build

| Item | macOS | Reason |
| --- | --- | --- |
| `pyconfig.h` | Vendored configure output for Darwin x86_64, plus two marked overrides | See the provenance block at the top of `pyconfig.h`. |
| `HAVE_BROKEN_POSIX_SEMAPHORES` | Defined by hand | Darwin defines `_POSIX_SEMAPHORES` as `-1` and `sem_init` fails with `ENOSYS`; the 2.3 thread module would otherwise build every lock on it. |
| `VA_LIST_IS_ARRAY` | Undefined on arm64 only | arm64 Darwin's `va_list` is a pointer. Untested: the reference machine is x86_64. |
| `Modules/_localemodule.c` | Compiled with `-U__APPLE__` | Its Apple branch needs `macglue.h` and the Carbon toolbox glue from a framework build. Without it the module is the generic POSIX one used on Linux. |
| `sys.platform` | `linux2`, as on Linux | The game's scripts were written for `win32` and `linux2`; keeping the Linux value keeps them on the retail Linux server's paths. |
| `--no-undefined` | Not passed | `ld64` already rejects undefined symbols in a dylib. |
| SDK sysroot | `CMAKE_OSX_SYSROOT=macosx` in the preset | Apple clang otherwise searches `/usr/local/include` (Homebrew) first; configure was run with the same sysroot. |

`dice_py_check_run` passes on macOS 15.7 x86_64: initialization, 64-bit pointer
round trip, 40 builtin modules, zlib and loopback UDP. Prefix-search warnings appear
as on Linux, because the engine sets `sys.path` itself.

## POSIX publication (S2.10)

[CHOSEN, not recovered] `Python/ceval.c` uses C11 atomics for pending-call queue indices,
the work flag and its producer try-lock. A failed try-lock returns the existing refusal (-1),
including in a nested signal handler. Sequential consistency publishes the queue entries
before the consumer reads them and retires an entry before a producer reuses it.
Every POSIX `_Py_Ticker` access in `ceval.c` and `Objects/longobject.c` uses `__atomic`;
its exported scalar declaration stays intact. `Objects/obmalloc.c` publishes the arena
vector before its count using C11 atomics; old vectors remain deliberately retained as
upstream requires. These changes apply on Linux too. Windows branches remain unchanged.

The focused check in `Code/Tests/s2_10_threads` drives four concurrent producers and
40,000 callbacks against the real library.
