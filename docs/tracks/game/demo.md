# Demo recording and playback (game track, v0.2.0)

Branch `mac/game-demo`. Goal: retail-faithful BF2 demo recording and playback.

## What works

- **Record.** The dedicated server records a `.bf2demo` as retail does.
  - `sv.autoRecord 1` makes the AutoRecorder start `Demo::recordDemo` at round start.
  - The console command `demo.recordDemo <name>` starts a recording by hand (registered since the
    `demo.*` batch, see below).
  - The file is a zlib stream: the `DemoHeader` (version 3153, server name, date, map), then frames
    of `(tick, size, payload)`. A tick of -2 marks a special packet.
- **Play back from the command line.** `+demo <con>` names a playback config, for example a file
  containing `demo.demofile @HOME@/Profiles/Default/demos/<file>.bf2demo`.
  - The level loads from the demo's own data blocks. The loaded-map screen waits for JOIN GAME, as
    on a live join.
  - The spectator camera then shows the round, and kill messages and other events play.
- **Play back from the menu.** The menu's demo list (`demos`) lists `@HOME@/Profiles/Default/demos/`.
  Its play button sends menu event 12 with the entry's path to `BF2Engine::playDemo`. This path is
  not scripted yet; it is an owner check.

## Recovered in this batch

- **`DemoRecorder` playback half** (Linux `bf2` with symbols, checked against BF2.exe 0x54A800-0x54D400):
  - stream setup: `initPlayback`, the `initRecord` camera arm, `openPlaybackStream`,
    `recordCameraToStream`, `playCameraFromStream`;
  - reading: `readData`, `internalReadData`, `readNextFrameHeader`, `handleSpecialPacket`;
  - `restartPlayback`, `close` and the destructor;
  - every spectator-camera forwarder.
  
  BF2.exe offsets are the ELF offsets + 0x78 from +0x4C on, and + 0x80 from +0x80 on.
- **`DemoSpectatorCamera`** (new; ELF 0x8151F50-0x8154610, PE twins about 0x6298E0-0x62ACF0):
  free camera and follow-player camera, the recorded camera track (`.bf2cam`), demo keys and
  slow-motion rates.
- **`DemoAvailableInfo`** (new, `Game/DemoAvailableInfo.*`; ELF 0x8054B10): data block 9, the url
  and map of a recorded demo that the server announces.
- **`GameClient`** (BF2.exe):
  - the demo paths of `load`, `update`, `receive`, `setSimulationRate`, `playDemo` and `handleDataBlock`;
  - block 9;
  - the IGameClient demo slots `+0x48`..`+0x6C`.
  
  Four slot names were misleading and are renamed to what the bodies do:
  - `setDemoPaused` → `demoSwitchPlayer`, because 4D26C0 calls `nextPlayer`/`previousPlayer`;
  - `isDemoPlaying` → `isDemoAtEnd`, because 4D2800 tests the recorder's end-of-stream byte. BF2Engine
    pauses on it, and `setPause` refuses to unpause while it is set.
  - `slot50` / `slot54` / `slot58` → `demoNextCamera` / `demoGetCurrentPlayerId` / `demoIsFreeFlying`;
  - `slot6C` → `addDemoBookmark(url, mapName)`.
- **`HudInformationLayer::update`'s demo arm** (PE 467641): the camera button label and the name of
  the followed player.
- **`BF2Render::render`'s playback camera** (PE 0x4532BB): it now calls
  `DemoRecorder::getPlaybackMatrixAndFOV`. The tree had a stand-in name for this,
  `getCameraTransformation`.

## Recovered in batch 2

- `DemoRecorder::initRender` and `initRenderSound` (ELF 0x81506E0 / 0x8150A40, PE 0x54BE60 /
  0x54C000): the movie and sound render modes 2 and 3.
- `DemoLibrary::download` with its free-name helper (PE 0x4C2DF0 / 0x4C26E0).
  - The POSIX client spells the target folder with `/` and in lower case: `[PLATFORM]`, because the
    GameSpy HTTP SDK opens it with `fopen`.
  - It is not exercised: that needs a server whose auto-demo hook publishes a URL.
- GameClient's non-demo sites:
  - the load-failure report: `LoadStat::unregisterLoadCallback` then `resetLoadStats`, PE 4D3064;
  - the net-graph stats update, PE 4D5A15, on `m_networkableStats`, which is what the PE calls (the
    tree's stop had named `m_bitStream`);
  - the player-name resend (ECCore 1, node id 1007): a `ChangePlayerNameEvent` on the event
    manager, PE 4D668F.
- Left in GameClient, all off the player path:
  - the two lightmap-farm sites (`DistributedLightRenderer`), reached only on a distributed
    lightmap render node;
  - the PunkBuster client packet handler, which is unreachable because PunkBuster is omitted.
- Game/Main's five sites are not on the playback path (see the main-session report). The watchdog
  `CheckServerAliveThread::run` went to the srv track as R-main-4.

## Playback controls (static recovery, checked against the owner's retail run)

There is no demo HUD bar. The controls are the spectator camera's keys plus two comm roses.
`scripts/demo-controls.txt` drives all of them (run-20261112-072750).

- **Input path.** `BF2Engine::updateInput` hands each player sample to `Demo::addPlayerInput`
  (ELF 0x8141C60), then to `DemoRecorder::addPlayerInput`, which queues it only while a demo plays.
  `DemoSpectatorCamera::handlePlayerInput` (ELF 0x8152490) reads the sample. The active maps are
  `defaultPlayerInputControlMap` plus `InfantryPlayerInputControlMap`, which `GameClient::playDemo`
  selects (PE 4D41EA). There is no local player.
- **Keys** (shipped `mods/bf2/Settings/Controls.con`):

  | Key | Input | What it does |
  |---|---|---|
  | Right mouse | `PIAltFire` | `nextCamera`: from free camera, follow the first live player with a vehicle, re-testing the current id first; from follow, free camera at the current view |
  | Space | `PIAction` | next player (`nextPlayer`) |
  | Shift+Space | `PIAction` + `PISprint` | previous player |
  | 1..9 | `PIWeaponSelect1..9` | simulation rate 0, 100, 5, 25, 50, 75, 150, 300, 1000 (ELF 0x8893E20, PE file offset 0x4FB138) |
  | R | `PIReload` | stop camera recording, `renderer.stopRenderMovie` |
  | W A S D + mouse | free camera | fly, no collision (`MultiPlayerFreeCamera`) |
  
  The earlier "PIAction and PIAltFire stay 0" was wrong: E is `PIUse`, not `PIAction`. Also, the
  test presses landed before the recorded bots spawned, so the follow camera fell back to free flight.
- **Roses.** `BF2HudManager::filterGameInput` (PE 758F15 / 758FB5) opens them while a demo plays
  and there is no local player:
  - Q (`GIRadioComm`) → `openRadio(true)` → HUD mode 31 → `DemoRecInterfaceShow`,
    `HudElementsDemoRecComm.con`:
    - centre: pause/play (`demo.toggleDemoPauseOrPlay`; the label is PLAY while the rate is 0);
    - top: RESTART;
    - bottom: QUIT (`demo.shutdownDemo`);
    - right side: NORMAL 100%, FAST 150%, FAST 300%;
    - left side: SLOW 50%, SLOW 25%, and the upper-left `game.simulationRate 10` button.
      That button reads "SLOW 5%": `English.utxt` defines `HUD_HUD_DEMO_SLOW_3` twice ("SLOW 5%",
      then "SLOW 10%"), and the lexicon keeps the first.
  - T (`GITacticalComm`) → `openSquadRadio(true)` → HUD mode 30 → `DemoCameraInterfaceShow`,
    `HudElementsDemoCameraComm.con`:
    - centre: `demo.toggleCameraDemo`, labelled with the other mode (PLAYER CAM / FREE CAM);
    - left: PREV PLAYER (`demo.togglePlayerDemo 0`);
    - right: NEXT PLAYER (`demo.togglePlayerDemo 1`).
    
    The two side items are shown only with `DemoPlayerNameShow`, i.e. in follow mode.
  - An item runs when it is clicked with the left button (`GIOk`, pointer event 1 →
    `Bf2ButtonNode::onEvent1` → its console command) while the rose key is held.
  - The FAST labels' trailing " *" is literal text: `English.utxt` has "FAST 150% *",
    "FAST 300% *" and "FAST 1000% *". No code reads it; a rate other than 100 only makes
    `GameClient::setSimulationRate` force a sound update.
- **The `demo.*` console commands** (recovered in this batch; the `Demo` console object had been
  left `#if 0` in `Game/Main/GameConsole.cpp`, so every rose item except the speeds did nothing):
  - Both images register `demo.recordDemo <name>` and `demo.stopRecording`, which call
    `BF2Engine::recordDemo` / `stopRecording` (dedicated ctors 0x411970 / 0x411B40, BF2.exe
    0x43525C / 0x435381).
  - BF2.exe adds five more (ctors 0x4353EE..0x435639):
    - `demo.shutdownDemo` calls `BF2Engine::shutdownDemo`, which ends the game. Playback then
      returns to the LOGIN screen, as in retail.
    - `demo.restartDemo`, `demo.togglePlayerDemo <bool>`, `demo.toggleCameraDemo` and
      `demo.toggleDemoPauseOrPlay` call IGameClient +0x48 / +0x4C / +0x50 / +0x5C (`restartDemo`,
      `demoSwitchPlayer`, `demoNextCamera`, `togglePause`) on the game's IGameClient.
      They are active only while that interface exists.
  - The retail `addNoDemoveShowEffect` lines are not a typo for the console. The console is
    case-insensitive, and `addnodemoveshoweffect` is the move show effect.
- **Every other key falls through** to `setMode(0, false)` (PE 758FE9). The game-input filter does
  nothing else while a demo plays.
- **No help prompts.** `updateDemoHelp` asks for "Press Q / T ..." at 10 s and 20 s, but
  `showWideHelp` refuses while a demo plays (PE 74F821, `game` +0xDC = `GameClient::isPlayingDemo`
  4D2630 at vtable 0x8AA178 + 0xDC). Retail never shows them.
- **Start pose.** The free camera starts at the origin, 75 m up, identity rotation
  (`ensureCamerasCreated`). On dalian_plant that is under the water surface, which matches the
  owner's "under water" on Karkand.
- **The followed name follows the player.** `HudInformationLayer::update`'s demo arm (PE 467641)
  sets `DemoPlayerNameString` from `demoGetCurrentPlayerId` on every HUD update, as retail does, and
  the `DemoPlayerName` text node reads the string at every paint. A suspected stale name after next /
  previous player (run-20261113-214704, -215551, and ctl-next of run-20261112-072750) was the
  scenario: a capture is taken at the next presented frame, and the following Shift+Space landed
  first, so "follow-next" showed the state after the previous-player switch. With a 300 ms wait after
  each capture the name is right (run-20261113-223038 shows "R. Lopez"), and the gate checks it from
  the trace.
- **Pointer.** Loading the level enters HUD mode 1, whose transition enables the arrow pointer.
  During playback the filter forces mode 0 with `setMode(0, false)`, which skips the leave-mode
  transition, so the arrow stays at the screen centre. Leaving rose mode 30/31 is also a no-transition
  `setMode(0, false)`, and the pointer stays enabled with the rose texture. Prediction: retail shows
  the same arrow until a rose is used. This is an owner check.

- **The T rose's PREV/NEXT PLAYER items are drawn** (follow mode).
  - An earlier note said they were missing. That came from the scenario, not the game:
    `capture ctl-rose-t` was followed directly by `key-up t`.
  - A capture is taken at the next presented frame. When the script ran the key-up first, the
    captured frame was the first one after the release:
    - `DemoCameraInterfaceShow` was already false, and the side items (no out-time) were gone;
    - the centre circle was still fading out over its 0.1 s.
  - That happened in about half the runs. The scenario now holds each rose 300 ms after its
    capture, and 4 of 4 runs show both items.

## Still open

- **The announcement.** On the server, `GameServer::onAutoRecordingAvailable` (srv track) stops.
  It can now use `DemoAvailableInfo`.
- **Config paths.** `loadDemoConfig` turns `/` into `\` before it opens the config. On macOS only a
  bare file name in the working directory opens; the harness writes `demo-play.con` into the client
  root for that reason.

## Harness and gate

- `loopback.py --auto-record` sets the server's `sv.autoRecord 1` and copies the recordings to
  `<run>/demos/`.
- `loopback.py --play-demo FILE` runs the client only. It copies the demo and a `demo-play.con`
  into the client root and starts the client with `+demo demo-play.con`.
  - It does not use the profile demo folder that the menu list scans.
  - Creating `Profiles/Default` before the client's first start makes `LocalProfileManager::init`
    skip `createDefaultProfile`. No audio settings exist then, and sound never initializes; that
    was the `sound=not-initialized` of the first gate runs.
  - The `+demo` path itself brings sound up through `LocalProfileManager::init`'s
    `applySettings(true)`; run-20261112-014943 has `sound=on`.
- `scripts/demo-record.txt` (phase 1): join and spawn, then aim, fire and walk (play-combat's moves),
  so the recording has a moving player next to the bots.
- `scripts/demo-play.txt` (phase 2): wait for the level load and JOIN GAME; 15 s later right mouse
  (follow camera), four captures a second apart, Space (next player), Shift+Space (previous
  player), 7 (150 %), 1 (pause, two captures), 2 (normal speed, two captures).
- The [TEMP] `BF2_PLAY_TRACE` line has a demo arm: with no local player during playback it prints
  `[PLAY] demo free=… follow=<id> followAlive=… followPos=x/y/z cam=x/y/z camInv=… hudName=…| followName=…`
  once a second (`BF2Engine.cpp`), from `demoIsFreeFlying`, `demoGetCurrentPlayerId`, the followed
  player and the last playback matrix `BF2Render::render` set (a [TEMP] static).
- `scripts/demo-controls.txt` (`loopback.py --play-demo FILE --input-script demo-controls`):
  - presses right mouse, Space, Shift+Space and 1 / 2;
  - holds T and Q;
  - clicks NEXT PLAYER, PAUSE and finally QUIT (which ends at the LOGIN screen);
  - captures each step; the client exits after about 85 s.

  It is not part of the gate; the captures are the check.
- `demo_cycle.py [--build-dir ...]` is the gate.
  - Phase 1 records 100 s of an 8-bot round while the client joins, spawns, moves and fires.
  - Phase 2 plays the recording with `demo-play.txt`.
  - It passes when phase 1 produced a recording, the playback client runs to its timeout with the
    script complete, there is no `[FRONTIER]` on either side and no crash report, and playback is
    shown to move (owner request, 2026-11-13):
    - consecutive follow captures differ below the horizon by more than 0.5 mean |diff| (a still view
      gives 0.000 there, a slow follow second about 1.4). Only rows
      300..599 count, because the sky animates by itself: the old free-camera views differed by
      about 5 over the whole frame and by 0.0 below the horizon;
    - a followed live player moves more than 1 m;
    - Space and then Shift+Space each change the followed player, and in every traced follow-mode
      second the render camera (`cam=`, the matrix `BF2Render` hands the view) is within 10 m of
      the followed player, and the HUD's `DemoPlayerNameString` (`hudName=`) names that player;
    - while paused the followed position stays put, and the two paused captures match below the
      horizon (at most 0.5).
  - The numbers go to `<phase 2 run>/demo-judge.txt`.
  - `demo_cycle.py --judge RUN` re-judges an existing phase 2 run. The old gate's playback
    (run-20261113-212143) fails every movement check; the first strengthened run
    (run-20261113-213351) passes: follow diffs 9.7-21.9, paused 0.000, followed id 0 → 248 → 0.

## Owner checklist

1. In the menu, open the demo list with the recorded demo in
   `~/Library/Application Support/OpenBF2/Profiles/Default/demos/`, then play it.
2. Playback visuals compared with retail:
   - the camera's start pose;
   - the free camera and the follow camera;
   - the demo HUD bar;
   - kill messages and nametags;
   - sound during playback (on in the harness since run-20261112-014943).
3. The camera controls and slow motion. Confirmed on retail by the owner:
   - right mouse = follow/free;
   - 1-9 = rates;
   - Space / Shift+Space;
   - the T and Q roses, with the Q rose's labels;
   - the arrow pointer until a rose is used;
   - QUIT → LOGIN.
   
   Still to compare: clicking RESTART, PAUSE/PLAY, the camera toggle and PREV/NEXT PLAYER in
   ours against retail.
4. The end of a demo: the client should pause when the stream ends.
5. Restarting a demo (IGameClient `restartDemo`, from the HUD or menu).
