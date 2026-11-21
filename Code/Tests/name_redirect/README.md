# Test-only host-name redirect (macOS)

`name_redirect.c` is a test tool. It is not part of any game image or release package, and
it must not be shipped. It lets a locally built `bf2` or `bf2_client` reach local test
services for its online names without changing `/etc/hosts` or DNS.

The game's online host names are retail, `<service>.gamespy.com`, and a provider file
(`+onlineProviderConfig <file>`, [Online services](../../../docs/building.md#online-services))
whose `"*"` entry is a domain resolves each of them as `<service>.<domain>`. Examples are `BF2Web.<domain>`, `gpcm.<domain>`,
`gpsp.<domain>` and `battlefield2.master.<domain>`. The library replaces
`getaddrinfo`, `gethostbyname` and `connect` in the process it is loaded into:

| Variable | Effect |
| --- | --- |
| `OPENBF2_TEST_REDIRECT_DOMAIN` | That domain and every name ending in `.<domain>` resolve to `127.0.0.1`. Other names use the system resolver unchanged. Unset: the library does nothing. |
| `OPENBF2_TEST_REDIRECT_HTTP_PORT` | Optional. TCP connections to `127.0.0.1:80` go to this loopback port instead. The stats web requests use port 80, and an unprivileged process cannot listen on `127.0.0.1:80`. |
| `OPENBF2_TEST_REDIRECT_LOG` | `1` writes one stderr line per redirected name or connection. |

Use a domain that does not exist, such as `test.openbf2`, so that a process started
without the library cannot reach a real host by mistake.

## Build

```sh
cmake --build out/build/<tag> --target bf2_test_name_redirect
```

The library is written to `out/build/<tag>/test_tools/libbf2_test_name_redirect.dylib`,
outside `bin_macos`. The target is excluded from the default build.

## Run

Write a provider file for the test domain, then start the game with it:

```sh
printf '{"schema": 1, "provider": "Local test", "services": {"*": "test.openbf2"}}\n' > "$PWD/test-provider.json"
OPENBF2_TEST_REDIRECT_DOMAIN=test.openbf2 \
OPENBF2_TEST_REDIRECT_HTTP_PORT=18080 \
DYLD_INSERT_LIBRARIES=out/build/<tag>/test_tools/libbf2_test_name_redirect.dylib \
out/build/<tag>/bin_macos/bf2_client +onlineProviderConfig "$PWD/test-provider.json" +noSound 1
```

`DYLD_INSERT_LIBRARIES` only works on binaries that are not built with the hardened runtime,
which holds for local builds. Signed release builds ignore it.

### Under `sandbox-exec`

`/usr/bin/sandbox-exec` is a protected system binary. The system removes `DYLD_*` variables
when it starts, so the game process it launches never sees them. Set the variables after
`sandbox-exec` instead, through `/usr/bin/env`:

```sh
/usr/bin/sandbox-exec -f profile.sb /usr/bin/env \
    DYLD_INSERT_LIBRARIES=$PWD/out/build/<tag>/test_tools/libbf2_test_name_redirect.dylib \
    OPENBF2_TEST_REDIRECT_DOMAIN=test.openbf2 \
    out/build/<tag>/bin_macos/bf2_client +onlineProviderConfig "$PWD/test-provider.json"
```

The loopback harness does this with `--name-redirect <domain>`
(`tools/macos/loopback/loopback.py`). That option builds nothing: build
`bf2_test_name_redirect` first. The option also writes the provider file
`online-provider.json` (`"*"`: the domain) into the run directory and adds
`+onlineProviderConfig <that file>` to both command lines. It lets the sandbox profiles read that
file and reach the GameSpy service ports on loopback only, plus the HTTP port given by
`--name-redirect-http-port`.

The headless checks `bf2_persistence_stats_check` and `bf2_swiff_leaderboard_check` do not
need this library. They replace the resolver inside their own process.
