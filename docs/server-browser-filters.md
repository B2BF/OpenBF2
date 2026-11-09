# Server browser filters

The internet server browser's filter checkboxes are stored as one integer bit mask in the
profile's `General.con`:

```
GeneralSettings.setToggleFilters 63616
```

The engine does not define what the bits mean. `GeneralSettings` only stores, tests and sets
them. The meaning of each bit, and the query clause it adds, come from the Flash main menu
(`External/FlashMenu/mainMenu.swf` in the mod's `menu_client.zip`).

## Bit assignments

The masks and clauses below are taken from the menu's ActionScript. The retail BF2
`mods/bf2/Menu_client.zip` and Forgotten Hope 2 `mods/fh2/menu_client.zip` use the same
values.

| Bit | Value | Checkbox label key | Menu setter | Query clause |
| --- | ---: | --- | --- | --- |
| 0 | 1 | none (no checkbox) | `setBotFilter` | `bf2_bots = 1` |
| 1 | 2 | `FILTERS_ranked` | `setRankedFilter` | `bf2_ranked = 1` |
| 2 | 4 | `FILTERS_battlerecorder` | `setBattlerecFilter` | `bf2_autorec = 1` |
| 3 | 8 | `FILTERS_battlecom` | `setBattlecomFilter` | `bf2_voip = 1` |
| 4 | 16 | `FILTERS_nopassword` | `setPasswordFilter` | `password = 0` |
| 5 | 32 | none (no checkbox) | `setDedicatedFilter` | `bf2_dedicated = 1` |
| 6 | 64 | `FILTERS_novehicles` (FH2: "Light combat mode") | `setNoVehiclesFilter` | `bf2_novehicles = 1` |
| 7 | 128 | `FILTERS_punkbuster` | `setPunkbusterFilter` | `bf2_anticheat = 1` |
| 8 | 256 | `FILTERS_populated` | `setPopulatedFilter` | `numplayers > 0` |
| 9 | 512 | `FILTERS_notfull` | `setNotfullFilter` | `numplayers < maxplayers` |
| 10 | 1024 | none (no checkbox) | `setPureFilter` | `bf2_pure = 1` |
| 11 | 2048 | `FILTERS_smallmaps` | `setSmallmapFilter` | `(bf2_mapsize = 16 or bf2_mapsize = 128)` |
| 12 | 4096 | `FILTERS_mediummaps` | `setMediummapFilter` | `bf2_mapsize = 32` |
| 13 | 8192 | `FILTERS_largemaps` | `setLargemapFilter` | `bf2_mapsize = 64` |
| 14 | 16384 | `FILTERS_sameversiononly` | `setSameVersionFilter` | `gamever = '<Logic.getModVersion()>'` |
| 15 | 32768 | `FILTERS_samemod` | `setSameMod` | `gamevariant = '<Mod.getCurrentModName()>'` |

The menu sets no bits above 15.

Examples:

- `0`: no filters.
- `30`: Ranked, BattleRecorder, BattleComm and No password (2 + 4 + 8 + 16).
- `63616`: the retail default. PunkBuster, Small, Medium and Large maps, Same version only
  and Same mod (128 + 2048 + 4096 + 8192 + 16384 + 32768).

## How the mask is used

- `GeneralSettings::setToggleFilters(int)` replaces the whole mask. It is the
  `GeneralSettings.setToggleFilters` console method that `General.con` runs when a profile
  loads, and the line `GeneralSettings` writes back when the profile is saved.
- `dice.bf2.Multiplay.getToggleFilter(mask)` returns whether any bit in `mask` is set. When the
  filters page opens, the menu's `toggleFilterInit` uses it to set each checkbox.
- `dice.bf2.Multiplay.setToggleFilter(mask, on, clause)` sets or clears one bit and stores its
  clause. An empty clause (or `%%`) clears the bit whatever `on` is, and removes the clause.
  Each checkbox setter passes an empty clause when the box is unchecked.
- `dice.bf2.Multiplay.updateFilter()` builds the query from the stored clauses whose bit is
  set, in ascending bit order. Clauses are joined with `and`. The three map-size clauses form
  one group joined with `or`, in parentheses when more than one is set. The free-text filter
  from `Multiplay.setFilter` is appended last.

The implementation is in `Code/BF2/Game/GeneralSettings.h` and the `as_multiplay_*Filter`
bindings in `Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayer.cpp`.

The stored mask does not filter anything by itself. `updateFilter` only uses clauses held in
memory, and only the menu's `setToggleFilter` calls put them there. In the shipped menu those
calls come from the checkbox setters. `toggleFilterInit` sets each checkbox from the mask,
which runs the setter for that box. `toggleFilterInit` runs on the `filters` frame of the
browser sprite, so a profile's saved filters take effect in a session once the player opens
the Filters page. Bits that have no checkbox (1, 32 and 1024) are never turned into clauses
by the shipped menu.

## Default for new profiles

Retail BF2 1.50 hard-codes `63616` in two places: the `GeneralSettings` constructor and
`GeneralSettings::setDefaultValues`. Creating a profile calls `setDefaults()` and then saves
the profile, so every new profile's `General.con` starts with `setToggleFilters 63616`.
Existing profiles keep whatever their `General.con` holds.

This engine uses `0` instead, so new profiles start with no filters. This is a deliberate
departure from retail behavior. The two assignments in `GeneralSettings.cpp` and the
expectations in `Code/Tests/general_settings_runtime.cpp` change together.

The menu movie only reads and changes the mask; it never supplies a default. A mod running
the retail executable can still control the filters without engine changes:

- **Menu.** Change `toggleFilterInit` to set every checkbox to unchecked instead of reading
  the mask. Each setter then calls `setToggleFilter(mask, false, "")`, which clears the bit
  and drops its clause, and the cleared mask is saved with the profile. Disabling the
  checkboxes as well keeps the filters off for every profile, including ones whose
  `General.con` was edited by hand.
- **Launcher.** Write `GeneralSettings.setToggleFilters 0` into each profile's `General.con`
  before starting the game. A profile created in the game during that session still starts
  with the executable's default until the next launch.
- **Executable patch.** In the retail BF2 1.50 executable the value is the 32-bit immediate
  of two `mov dword ptr [esi+0F4h], 0F880h` instructions, one in `setDefaultValues` and one
  in the constructor. Setting both immediates to `0` changes the default for new profiles
  only.

## Reading the menu ActionScript

`Code/Tools/SwfDisasm/avm1_disasm.py` lists the ActionScript bytecode of a Flash menu movie,
with constant-pool strings resolved. It runs on Python 2.7 or 3 and needs no extra packages.

```
python Code/Tools/SwfDisasm/avm1_disasm.py "<game>/mods/fh2/menu_client.zip" ^
    --member External/FlashMenu/mainMenu.swf -o mainMenu.asm
```

Write the output outside `Code/BF2`. To find the filter masks, search the listing for
`setToggleFilter`. Each checkbox setter pushes `r1, r2, <mask>, 3` before calling
`dice.bf2.Multiplay.setToggleFilter`. AVM1 pushes arguments last-first, so the call is
`setToggleFilter(<mask>, r2, r1)`: `r2` is the checkbox state and `r1` the clause.

The listing marks each timeline frame with a `Frame <n>` line and shows its `FrameLabel`,
so frame actions can be matched to `gotoAndStop` targets. A frame's label can appear after
the frame's actions.

The tool decodes `DoAction`, `DoInitAction` and nested sprite actions. It does not decode
button actions or clip-event actions, and it does not resolve branch targets or
`DefineFunction2` parameter registers.
