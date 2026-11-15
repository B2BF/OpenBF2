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

Both client and dedicated-server integrations can use these shared SDK files.
S2.10 adds these POSIX-only changes, all [CHOSEN, not recovered]:

- `common/{linux,macosx}/gsThread*.c`: sequentially consistent atomic increment/decrement,
  returning the new value. The SDK previously asserted and returned 1.
- `common/gsCore.c`: sequentially consistent atomic loads/stores for initialization/shutdown
  flags and the reference-count read; the C record layout and task-list critical section stay intact.
- `natneg/natneg.c`, `NNProcessData`: reject input shorter than the base header before reading
  its magic/type. Packed payloads already copy with `memcpy`.

The service domain is the SDK's own `GSI_DOMAIN_NAME` (`gamespy.com`). The game replaces it at
run time without a source change: `Game/MatchMaking/OnlineProviderGameSpy.cpp` writes the service
names into the SDK's host-name variables (`GSIACHostname`, `qr2_hostname`, `gcd_hostname`,
`StatsServerHostname`, `GPConnectionManagerHostname`, `GPSearchManagerHostname`,
`SBOverrideMasterServer`, `Matchup1Hostname` to `Matchup3Hostname`). A new SDK revision must keep
these variables, the name formats `OnlineProvider.cpp` mirrors, and the master-server index hash
of `sb_serverlist.c`.

Changing the SDK revision requires checking the interfaces and behavior used by
those integrations.
