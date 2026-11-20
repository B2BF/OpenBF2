# GameSpy SDK local changes

This tree uses the December 2007 GameSpy SDK; qr2 identifies itself as version
3.04.00 in [its changelog](qr2/changelog.txt). Preserve the upstream notices and
keep integration changes identifiable in the source.

| File and function | Local change | Reason |
| --- | --- | --- |
| `serverbrowsing/sb_serverbrowsing.c`: `ServerBrowserAuxUpdateIP` | Check for an existing server address before starting and appending a new server. | Avoid rejecting the newly appended server as its own duplicate before the synchronous wait. |
| `qr2/qr2.c`: `qr2_init_socketA` | Initialize `denyresp2_ip_callback` to `NULL` with the other callbacks. | The enabled query-parser hook must not call an uninitialized function pointer. |
| `common/gsPlatformSocket.h`: `_MACOSX` socket-call macros | Apply the `socklen_t` casting macros to C translation units only. | Macro definitions such as `connect(s,a,al)` rewrite C++ member functions of the same name in engine sources that include the SDK; the engine already passes `socklen_t`. Affects only the macOS port. |
| `common/gsAvailable.c`: `GSIStartAvailableCheckA`, `GSIAvailableCheckThink` | `[CHOSEN, not recovered]` Once a check has ended without an answer (the host name did not resolve, or both tries timed out), later checks in the same process end at once with the same result, `GSIACAvailable`. | The game repeats the check before every use of a service: eight server lists at client start-up, each login, each server start. A host that resolves but does not answer costs two 2-second tries per check, 32 seconds at client start-up. |
| `gp/gpiConnect.c`: `gpiStartConnect`; `gp/gpiSearch.c`: `gpiStartProfileSearch` | `[CHOSEN, not recovered]` The two host-lookup error texts read "manager" instead of "mananger". | `ProfileManager::reportError` maps the texts of the SDK revision Battlefield 2 shipped with ("Could not resolve connection manager host name.", "Could not resolve search manager host name.") to the localised connection-failed message. |
| `serverbrowsing/sb_serverlist.c`: `ServerListConnect` | `[CHOSEN, not recovered]` The master-list connect is non-blocking with a 5-second limit (`SB_CONNECT_TIMEOUT`), then blocking again as before; a timeout returns `sbe_connecterror`. | The SDK's blocking `connect` waits for the operating system's TCP timeout (75 s on macOS, about 21 s on Windows) when the name resolves to an address that never answers, with the menu frozen. |
| `gp/gpi.h`, `gp/gpiConnect.c`: `gpiStartConnect`, `gpiCheckConnect`; `gp/gpiSearch.c`: `gpiProcessSearch` | `[CHOSEN, not recovered]` A connection-manager or search-manager connect that has not completed after 5 seconds (`GPI_CONNECT_TIMEOUT`) fails: login with `GP_LOGIN_CONNECTION_FAILED` ("The server has not answered the connection."), search with `GP_SEARCH_TIMED_OUT`. | Same as above; Battlefield 2 runs these operations blocking. |
| `ghttp/ghttpConnection.h`, `ghttp/ghttpProcess.c`: `ghiDoConnecting` | `[CHOSEN, not recovered]` A connect that has not completed after 5 seconds (`GHI_CONNECT_TIMEOUT`) ends the request with `GHTTPConnectFailed`. | Same as above; most stats requests are blocking. |
| `common/gsPlatformSocket.{h,c}`: new `gsiSetHostRedirect`, `gsiRedirectHostname`, `gsiGetHostByName`; the host-name lookups in `common/gsAvailable.c`, `common/gsPlatformUtil.c` (both resolver-thread arms, the `getaddrinfo` arm and the synchronous arm), `qr2/qr2.c`, `gcdkey/gcdkeys.c`, `gstats/gstats.c`, `natneg/natneg.c`, `natneg/NATify.c`, `gp/gpiConnect.c`, `gp/gpiSearch.c`, `serverbrowsing/sb_serverlist.c` and `ghttp/ghttpProcess.c` | `[CHOSEN, not recovered]` Every lookup of a service name goes through `gsiRedirectHostname`. Without an installed redirect it returns the name itself, so the lookup is unchanged. A redirect returns the name to look up instead (a dotted IPv4 address, which the system answers without DNS), or `NULL` to make the lookup fail. | `+gamespyRedirect` and the `+gamespyRedirect<label>` options (`Game/MatchMaking/OnlineProvider.h`) resolve the retail names elsewhere inside the process, keeping the names (and so the HTTP Host headers) retail. The redirect function maps each name to an address or another host name by its label itself, so the SDK passes only the name and resolves the returned name at every lookup. Not routed: `gsPlatformSocket.c` `getlocalhost` (this machine's own name); `gt2/gt2Utility.c` (linked with presence, but only given peer addresses, never a service name); and `chat` and `pinger`, which no image compiles. |

Both client and dedicated-server integrations can use these shared SDK files.

The native Linux client adds `Voice2/gvSDLAudio.{c,h}`, selected by `BF2_LINUX_CLIENT` in
`gvMain.c`, and appends `GVHardwareSDL` to `gv.h` without changing existing enum values.
This is [CHOSEN, not recovered] platform integration under
[issue #36](https://github.com/kr4uzi/OpenBF2/issues/36). It opens SDL3's default playback
and recording devices and feeds the SDK's existing custom-device codec/source paths from
`gvThink` and capture calls on the game thread. SDL handles device conversion on its own threads;
SDK filters, mixing and encoding stay on the game thread. Playback queues about 100 ms and
capture retains at most one second when pumped. Device failures report to stderr and refuse
or unplug the device. Codec changes flush playback sources before replacing the codec's callbacks;
voice-quality changes reuse the SDL streams. PCM frame-size/sample-rate changes require closing
devices first. The original
Windows/macOS backends and network packet formats are unchanged. Linux's system mixer controls
remain unavailable; per-device voice volume uses the SDK's software gain.

`Code/Tests/linux_audio` checks SDL dummy-device lifecycle, Raw and Speex packet buffers,
filter receivers, unavailable-device failures and cleanup without microphone or network use.

S2.10 adds these POSIX-only changes, all [CHOSEN, not recovered]:

- `common/{linux,macosx}/gsThread*.c`: sequentially consistent atomic increment/decrement,
  returning the new value. The SDK previously asserted and returned 1.
- `common/gsCore.c`: sequentially consistent atomic loads/stores for initialization/shutdown
  flags and the reference-count read; the C record layout and task-list critical section stay intact.
- `natneg/natneg.c`, `NNProcessData`: reject input shorter than the base header before reading
  its magic/type. Packed payloads already copy with `memcpy`.

The service domain is the SDK's own `GSI_DOMAIN_NAME` (`gamespy.com`), and the game never
changes it or the SDK's host-name variables: every name stays retail, and the redirects act in
`gsiRedirectHostname` only. A new SDK revision must keep the retail name formats (the labels
`OnlineProvider.cpp` lists, including the master-server index hash of `sb_serverlist.c`, which
gives `ms14` for `battlefield2`) and route every new host-name lookup through
`gsiRedirectHostname` (`gsiGetHostByName`), as listed in the table above.

Changing the SDK revision requires checking the interfaces and behavior used by
those integrations.
