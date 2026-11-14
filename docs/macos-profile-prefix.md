# Login PREFIX row and saved-profile preselection on macOS

Follow-up to the ui-prefix3 investigation. The owner's macOS LOGIN dialog
showed the account list unselected: no PREFIX field, no SELECT ACCOUNT button. Retail
showed the saved offline profile highlighted with both controls present.

## What the movie needs

The stock `mainMenu.swf` places `prefixField` (InputField sprite 336, `LOGIN_prefix`) and
the SELECT ACCOUNT button only in the `offline`/`online` frames of account sprite 402.
`selectProfile(obj)` reaches them after a row click or after ColumnList (sprite 42)
preselects the row whose native `getListEntries("localProfiles")` metadata has
`selected=1`. The native marks the row whose directory equals
`GlobalSettings.defaultUser` (SwiffPlayer.dll `0x10026E45`). So the missing PREFIX row and
the missing SELECT ACCOUNT button are one symptom: no saved default user at list time.

## Root cause

1. `UnixFileSystem::fixUnixPath` kept only the absolute `clientDataDirectory()` prefix and
   lowercased the rest. A fresh macOS home therefore got `Profiles/default/profile.con` and
   `Profiles/global.con` (the owner's install has exactly these names).
2. `LocalProfileManager::getLocalProfiles` enumerates the real directory name `default`.
   `validateProfileFiles` accepts the default directory by case-sensitive equality with
   `"Default"` (BF2.exe `0x4A8596`); `default` has no General.con, so it is invalid, is
   filtered out and `m_hasDefaultProfile` stays false.
3. `init` then runs `createDefaultProfile` on every start. Its `saveCurrentProfile` writes
   Global.con from the in-memory GlobalSettings before Global.con was loaded, so the saved
   default user becomes empty. Only a later login restores it.
4. The login list is built with an empty default user, nothing is preselected, and the
   account sprite stays in its unselected frame.

The Windows client's file system never lowers names, so retail keeps `Default` and none of
this happens. Earlier loopback captures did not show the problem because the harness copies
the content's `Profiles/Local/Default` into the run home, which a real install does not have.

## Change

`fixUnixPath` keeps the case of paths below `<clientDataDirectory>/Profiles/` on the macOS
client `[CHOSEN, not recovered]`, matching the Windows client. Content paths elsewhere below
the data directory are still lowered.

Owner decision 2026-11-14: an existing install whose earlier build created
`Profiles/default` keeps that lowercase name (APFS is case-insensitive, so writes land in it);
the owner renames it once to `Default`. No migration code is added.

Names below Profiles/ that now keep the case the code spells: the `Default` directory and
`Global.con`, `Profile.con`, `General.con`, `ServerSettings.con`, `Audio.con`, `Video.con`,
`Haptic.con`, `Controls.con`, `DemoBookmarks.con` and the map list. On the default
case-insensitive APFS volume existing lowercase files are still found under either spelling
and keep their old on-disk names. On a case-sensitive volume an install written by an earlier
build would no longer be found (for example `0001/profile.con` versus `0001/Profile.con`, so
the profile fails validation); such an install must rename its files or start fresh.

## Harness and covering check

`[TEMP]` loopback options model a real install instead of copying the content's
`Profiles/Local/Default`: `--default-profile none` is a first start (the client creates the
directory), `--default-profile default` an install written by an earlier build, and
`--profiles-from RUN_DIR` starts with the Profiles an earlier run left (a second start of the
same install). The covering check is two starts of one fresh home:

```sh
python3 -I tools/macos/loopback/loopback.py --build-dir BUILD --no-server \
  --default-profile none --input-script profile-first-start
python3 -I tools/macos/loopback/loopback.py --build-dir BUILD --no-server \
  --default-profile none --profiles-from out/loopback/run-FIRST --input-script profile-second-start
```

The first start creates `Profiles/Default`; its script selects the offline row and logs in,
which saves `defaultUser "0001"`. The second start's `second-start` capture must show the row
preselected with the PREFIX field and SELECT ACCOUNT, with no click, and Global.con must still
hold `"0001"`. `scripts/profile-login.txt` captures the connecting screen.

## Evidence

All runs at 800x600 with `scripts/profile-login.txt` and `BF2_SWIFFPLAYER_ASLOG=1`:

| Build | Run | Profiles after the run | Login screen at start |
| --- | --- | --- | --- |
| before | content Default copy (old harness) | `Default`, `defaultUser "0001"` | PREFIX, row selected, SELECT ACCOUNT |
| before | `--default-profile default` | `default`, `defaultUser ""` | unselected, no PREFIX (the owner's screen) |
| before | fresh home, first start + login | `default`, `defaultUser "0001"` | unselected (expected on a first start) |
| before | same home, second start | `default`, `defaultUser ""` | unselected, no PREFIX, no SELECT ACCOUNT |
| after | fresh home, first start + login | `Default`, `defaultUser "0001"` | unselected (expected on a first start) |
| after | same home, second start | `Default`, `defaultUser "0001"` | PREFIX, row selected, SELECT ACCOUNT |

A first start can't preselect anything, on retail either: `createDefaultProfile` saves the
still-empty global settings before any profile is used.

## Connecting screen timing

SELECT ACCOUNT goes to root frame `connectingProfile`: the message (sprite 410,
`SPECIAL_connecting`) and a 38-frame bar animation (sprite 409, 30 fps). Its frame 37 goes to
`connectingProfile2`, which calls `Profile.loginOfflineAccount` (or `loginOnlineAccount*`)
synchronously and then `gotoPage` removes the message. Visible time is about 37/30 s plus the
blocking login. Measured on macOS for an offline profile: 1.25 s of animation and 4 ms of
login. Offline login touches no network. Online login additionally blocks on the GameSpy
available check (2000 ms timeout, one retry), `gpConnectUniqueNick` and `gpGetInfo` in
blocking mode. A longer retail screen comes from that blocking call (profile and video
settings apply on Windows, GameSpy for online profiles), not from the movie.
