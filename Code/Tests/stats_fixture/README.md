# Local stats fixture

This test service supplies chosen statistics to the rebuilt client's BFHQ views.
It listens on `127.0.0.1`, writes no files, and makes no outbound requests.

## Run

From the repository root:

```powershell
python Code/Tests/stats_fixture/bf2_stats_fixture.py --score 20 --weapon 0 --kills 5 --deaths 6
```

Use `--help` for all options. `--selftest` prints sample player, awards, and
leaderboard responses and exits. Weapon 0 is the assault rifle, kit 1 is Assault,
and map 0 is Kubra Dam.

Start a locally built client with `+gamespy test.openbf2` and the test-only name redirect
library (`Code/Tests/name_redirect/README.md`), on macOS:

```sh
OPENBF2_TEST_REDIRECT_DOMAIN=test.openbf2 \
OPENBF2_TEST_REDIRECT_HTTP_PORT=18123 \
DYLD_INSERT_LIBRARIES=out/build/<tag>/test_tools/libbf2_test_name_redirect.dylib \
out/build/<tag>/bin_macos/bf2_client +gamespy test.openbf2 +noSound 1
```

The stats host `BF2Web.test.openbf2` then resolves to `127.0.0.1`, and its port 80 is moved
to the fixture. Login, server browsing and joining use their own `*.test.openbf2` names,
which this fixture does not serve. Log in through the normal client flow. Player statistics
are cached for an hour, so restart the client after changing fixture values.

`bf2_swiff_leaderboard_check` (CMake, macOS) starts this fixture on port 18124 and checks the
menu host's leaderboard and awards caches against it.

## Responses

| Endpoint | Fixture data |
| --- | --- |
| `getplayerinfo.aspx` | The three player-statistics queries used by the client |
| `getawardsinfo.aspx?pid=N` | Nine sample awards using the stock catalogue IDs; `--no-awards` returns none |
| `getleaderboard.aspx` | Global pages and the requested player's row, including score, team, kit, vehicle, and weapon boards |
| Other endpoints | `E 404` |

Leaderboard defaults are 57 players, the fixture player at position 42, and
player ID 1. Set `--lb-size`, `--lb-position`, and `--lb-pid` to change them.
Use the client's profile ID for `--lb-pid` when testing its highlighted row.

## Format constraints

Player-info responses contain `O`, the `H asof`/`D <time>` pair, a header and
data row, and a trailing `$ n $`. The favourites query reads columns by position,
so preserve their order. Omit unavailable keys instead of emitting empty tab
fields, which the client's tokenizer would collapse.

Awards use the `award level when first` columns. Leaderboards include the
`H size asof`/`D <size> <asof>` pair before their columns and rows. The fixture
sends `deathsby`; the client reads the vehicle/weapon column as `detahsby`, so
that displayed value remains zero.

These responses are test data. They do not validate a ranked-server statistics
pipeline.
