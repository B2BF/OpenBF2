# macOS client: directories, written files and launch contract

This note is for tools that install and launch the macOS client (`bf2_client`) as a binary,
such as the setup app. It lists the directories the client uses, every file it creates or
writes, and what a launcher must and must not do.

The client follows retail Battlefield 2 1.5 on Windows. Retail writes most things into
its game folder (its working directory). Profiles and a few per-user caches go to
`Documents\Battlefield 2`. On macOS the support root below stands in for
`Documents\Battlefield 2`.

## Directories

| Name | Location | Role |
| --- | --- | --- |
| Support root | `~/Library/Application Support/OpenBF2` | Per-user data, the counterpart of `Documents\Battlefield 2`. `~` is `$HOME`, or the account's home directory if `HOME` is unset. |
| Profiles | `<support root>/Profiles` | Player profiles and global settings. |
| Generation (game folder) | `<support root>/GameData/<generation>` | One installed copy of the game data. **The client's working directory.** The client writes into it. |
| FxNative | `<support root>/FxNative` | The client's native shader cache. It lives outside the generation on purpose, so it survives reinstalls. |

The client creates the support root itself (mode 0700) if it is missing. It never writes
to `<support root>/GameData` outside the generation it was started in, and it never writes
to the setup app's own folders.

File names: when the engine writes through its own file layer, everything after the
support root, or the whole relative path, is lower-cased (for example
`mods/bf2/settings/usersettings.con`). Names below `Profiles/` keep their case. Files the
client opens directly keep the exact names shown below.

## What the client writes

### In the generation (working directory)

| Path | When | Regenerable | Notes |
| --- | --- | --- | --- |
| `Client.lock` | every start | yes | Empty file holding the single-instance lock (an advisory `flock`). The lock is released when the client exits or dies. The file is never removed. Starting a second client in the same generation fails unless `+multi 1` is passed. Do not delete it while a client runs. |
| `RendDX9Rebuilt.log` | every start, rewritten | yes | Renderer diagnostic log, always on. Useful for bug reports. |
| `mods/<mod>/settings/*.con` (e.g. `usersettings.con`) | when settings are saved | partly | Game settings saved by the engine with relative paths, as retail does. Deleting them resets those settings to the shipped defaults. |
| `ragdollconstraints.inc` | only via the debug console command `ragDoll.save` | yes | Developer tool output. |
| `timedemo_report.txt` | only when a timedemo runs | yes | Benchmark output. |

Any other file that appears in the generation was written by the engine through a
relative path, the same way retail writes into its game folder.

### In the support root

| Path | When | Regenerable | Notes |
| --- | --- | --- | --- |
| `Profiles/` | first start and whenever profiles or settings change | **no** | `Global.con`, one folder per profile (`0001/`, …, `Default/`), `Default/demos/`, `BattleCommoDefaultUserSettings.con`. This is user data. The client creates a `Default` profile and `Global.con` if they are missing. |
| `mods/<mod>/cache/{<adapter GUID>}_3153_<n>/*.cfx` | while effects compile | yes | Retail's compiled-effect cache. Retail also keeps it under `Documents\Battlefield 2`. It is safe to delete, and only costs recompiles. `<mod>` is `bf2`, or `xpack` for Special Forces. |
| `FxNative/` | while effects compile | yes, but slow | Native shader cache. Rebuilding it takes minutes of stutter on the next play, so keep it across reinstalls and updates. Deleting it is safe but costly. |
| `screenshots/` | when the player takes a screenshot | **no** (user content) | |
| `logocache/` | when servers send logos | yes | |
| `ProtectedData.key` | first time protected data is used | **no** | Random per-user key that stands in for Windows DPAPI. It is created with owner-only permissions. Deleting it makes previously protected values unreadable. |
| `Registry.con` | **never written by the client** | n/a | Plain-text stand-in for the Windows registry (CD keys). The client only reads it. See below. |

## What the launcher must do

Before launch:
- Install the generation and make sure it is **writable by the user**. The client writes
  into it.
- To enable online play, write `<support root>/Registry.con` with the CD key(s), mode 0600.
  The client never creates or changes this file. Format: one `path\name = value` line per
  value.

  ```text
  HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\Battlefield 2\ergc = YOUR-CD-KEY
  HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\Battlefield 2 Special Forces\ergc = YOUR-XPACK-KEY
  ```

- Nothing else needs to be created. The client creates the support root, `Profiles/` and a
  default profile.

### `Registry.con` syntax

Each entry is `HIVE\registry\path\value-name = value`. Hives are `HKEY_LOCAL_MACHINE` and
`HKEY_CURRENT_USER`; abbreviated hive names are not accepted. Use single literal backslashes,
with no escaping. A final `@` denotes a key's default (unnamed) value, so the CD-key subkey
fallback can also be written as
`HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\Battlefield 2\ergc\@ = YOUR-CD-KEY`.

- The named value takes precedence over the subkey default, also when it is explicitly empty.
- Hive, path and value names match ASCII case-insensitively; value text keeps its case and bytes.
- The first `=` separates key and value. Spaces, tabs and carriage returns around both sides are
  trimmed; an empty right side is a present empty string. Quotes are literal; there are no
  escapes and no inline comments.
- Blank lines and lines starting with `#` or `;` are ignored. LF, CRLF and a missing final
  newline are accepted, and so is one leading UTF-8 byte-order mark. Duplicate entries use the
  last valid value.
- Lines without `=`, unknown hives, embedded NULs, empty value names or doubled backslashes are
  ignored with a line-number-only diagnostic on stderr; values never appear in diagnostics.
- Each lookup reopens the file, so nothing is cached. A missing file or key keeps the Windows
  absent-key result (`Language` defaults to `English`, `ergc` to an empty string). Use the plain
  key text, not a DPAPI-protected `x9392` value.

The file is data, not an engine console script, despite the `.con` extension. The Windows
client reads the same text from a DPAPI-protected `Registry.dat`
([client registry file](registry-file.md)).

Never:
- Delete `Profiles/`, `screenshots/`, `ProtectedData.key` or `Registry.con` during an
  update, repair or reinstall of the game data. Keep `FxNative/` too (see above).
- Verify the generation strictly. Extra files in it are expected (see the first table).
  Checks should cover only the shipped files and should ignore extra ones, especially
  `Client.lock`, `RendDX9Rebuilt.log` and `mods/*/settings/`.
- Delete or replace a generation while a client runs in it. `Client.lock` is held for the
  whole session.

The two `.cfx` and `FxNative` caches may be offered as a "clear caches" action. The client
rebuilds both.

## Launch contract

- **Working directory**: the generation, `<support root>/GameData/<generation>`. All
  relative paths, both game data and written files, resolve against it.
- **HOME**: the user's home directory, inherited as usual. The client derives the support
  root from it. Setting a different `HOME` moves all per-user data, which is how isolated
  test runs work.
- **Arguments**: the usual BF2 command line, such as `+modPath mods/bf2`, `+fullscreen 0|1`
  and `+multi 1` (skip the single-instance check).
- **Environment variables the client reads.** All are optional, and launchers should
  normally set none of them.

  | Variable | Effect |
  | --- | --- |
  | `HOME` | Base of the support root (see above). |
  | `BF2_FX_NATIVE_CACHE` | Overrides the `FxNative` location (absolute path). Empty or unset means `<support root>/FxNative`. |
  | `BF2_RENDDX9_FRONTIER` | `continue` keeps running when the renderer reaches an unsupported path. By default the client aborts so the problem is visible, and logs the reason to `RendDX9Rebuilt.log`. |
  | `PYTHONPATH`, `PYTHONCASEOK` | Read by the embedded Python 2.3. **Do not set them**: a foreign `PYTHONPATH` can break the game scripts. |
  | `BF2_*` names not listed here | Developer diagnostics and test hooks (traces, input scripts, capture). Leave them unset. |
