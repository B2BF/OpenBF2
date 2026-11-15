# Game frontier closure ledger

Owner rules (2026-11-12, via main):
1. Recover every site faithfully from retail, reachable or not.
2. A site no normal session reaches gets a focused test (a runtime/unit test or a loopback scenario)
   that drives it.
3. Only real third-party boundaries may remain: PunkBuster (proprietary SDK) and the Novint HFX DLL.
   BF2's side is recovered up to the boundary: the vendor DLL is loaded dynamically on Windows
   (pbsv.so on the Linux server), and other platforms take retail's not-installed / no-device branch.
   A fake DLL in the tests checks the calls. These rows read "boundary: owner signed off: approach".
4. Helpers and macros with no call sites left are removed.
5. A large site is flagged with an estimate, not shortcut.

Scope: game-owned paths. GameServer, MatchMaking and Persistence are out of scope (srv); so are
Platform/Gfx, RendDX9, GeomMisc and RendDX9Abi (render).

## How the list is computed

`[FRONTIER]` string literals in code with comments stripped, plus the call sites of the helpers and
macros that print them (scratch scanner, 2026-11-12, base mac-metal 5af6aaa3).
- Each row is one site.
- A helper's own body is not a row; it is removed once its last call site closes.
- Total: 82 sites.
- Not in this ledger: External/ffmpeg-bink (Bink RDFT audio) is render's, in its Bink row (main,
  2026-11-12).
- Helpers already without call sites, removed in batch 1: `soundSeamFrontier` (Core/SoundSeamFrontier.h)
  and `projectileFrontier` (GenericProjectile.cpp).
- `nativeMethodFrontier<Method>` (SwiffPlayer.cpp) is a template registered per unresolved Swiff
  native, so its rows are its registrations; they are enumerated when gameswf is worked.

Progress:
- Batch 1 (mac/game-frontier-1) closed 14 sites: SliderNode and BfFixedSliderNode, every frame and
  event body.
  - The HUD's Bf2FixedSliderNode inherits them. Shipped content keeps its sliders in a `beginrem`
    block, so the fixture drives them.
  - The batch also removed the two dead helpers above.
- Batch 2 (mac/game-frontier-2) closed 9 sites:
  - LocalProfileManager PE+30/+34;
  - MaterialManager's three id guards, which are retail's own int3, kept as traps;
  - X87Compat's narrow long double;
  - Node +A0;
  - the geometry-template clear;
  - the native-dispatch invariant, relabelled.
- Batch 3 (mac/game-frontier-3) closed 14 sites:
  - BF2Render's VTune bracket (profilerClient), with demo.cpp's two pauseVTune calls;
  - the VoipServer debug logs: CDebugRoutingState ×3, CDebugVoiceTrace ×5,
    HandlePacketDebugRoutingData;
  - BfSound::drawStats;
  - the three decoder-less Vorbis branches, removed with the opt-out build variant.
- Batch 4 (mac/game-frontier-4) closed 6 sites:
  - RagDoll writeConstraints, drawConstraints and drawDebugLine;
  - BfSound::drawSoundSources (with ISoundEngine +0xB4 retyped to return the sample list);
  - BfNewListBoxNode::onEvent3;
  - Bf2MenuMapNodeTypeDescriptor::create (retail-faithful null-System fault).
- Batch 5 (mac/game-frontier-5):
  - closed the list-box cell resource pair (releaseListBoxCellResource, paintSelf finalization);
  - routed ragDoll.save to the macOS client data directory.
- Batch 6 (mac/game-frontier-6) closed dice::meme::Tree (getTreeClassType).
- Batch 7 (mac/game-frontier-7), the hudBuilder batch:
  - all 34 unimplemented HUD console rows are recovered (24 manager/minimap/items/chat/radio/help,
    10 hudBuilder with their builder bodies and the new Bf2TileNode);
  - the 4 HudConsoleBinding rows are closed.
- Batch 8 (mac/game-frontier-8), the Profile console:
  - the 17 profile rows execute: typed invokers over the recovered profile store (owner,
    PlayerProfile, CharacterProfile, per-mod ModSetting; BF2.exe 0x7BF950..0x7C1340);
  - applySettings renames the local player and sets its kit, as in retail;
  - the 4 ProfileConsoleBinding rows are closed (54 of 82).
  - [CHOSEN] where retail reads undefined memory: activating a player profile with no characters,
    and ModSetting's unwritten default kit (starts at 0).

- Batch 9 (mac/game-frontier-9), GameClient:
  - the client DistributedLightRenderer is recovered whole with its factory row (it is created at
    startup, as in retail); GameClient.cpp:406 and :2486 call it;
  - parseParameters' option 35 (the lightmap-rendering-node switch) is recovered as retail has it:
    a case no registered option reaches, so both rows are recovered-but-unreachable-in-retail;
  - GameClient.cpp:878 (the PunkBuster client packet handler) moves to the PB/Novint batch;
  - 2 sites closed (56 of 82).
- Batch 10 (mac/game-frontier-10):
  - BFGame's dedicated menu arm (the interfaces it needs are declared in every image now; the dead
    AIFrontier.h is removed);
  - BF2ClientFrontiers' haptic target is the Novint DLL's effect object: proposed for the PB/Novint
    batch;
  - 1 site closed (57 of 82).
- Batch 11 (mac/game-frontier-11), gameswf group A:
  - SwiffHost slot18C (g_bfSound setMuteState), set_depth pure, the two key-state shims narrowed to
    what SwiffPlayer.dll asks, and the log callback the ctor really registers (no fscommand);
  - OfflineConnectPopup (owner's feature) waits for the owner; getListEntries is batch 12; the
    S3-bridge and VoIP groups wait for the owner's decision;
  - 5 sites closed (62 of 82).
- Batch 12 (mac/game-frontier-12): getListEntries' 15 missing branches and the retail empty default;
  1 site closed (63 of 82).
- Batch 13 (mac/game-frontier-13): OfflineConnectPopup declines quietly (owner decision 2026-11-13,
  via main). A missing popups clip, a missing connecttoip label and a failed navigation each answer
  false, open no dialog and log once without the frontier tag. offlineConnectPopupFrontier is removed.
  The owner-feature label stays. 3 sites closed (66 of 82).
- Batch 14 (mac/game-frontier-14): the voice client in full (owner decision 2026-11-13, via main).
  - BF2VoipClient (BF2.exe 0x4510F0..0x45235C, vtable 0x89C1A8, 64 slots) on its base VoipClient
    (0x736C30..0x737D4E: GameSpy Voice devices, Speex codec selection, the UDP session with the voice
    server) and the WINMM microphone mixer (0x7356B0..0x736BA0, macOS on CoreAudio).
  - GameSpy Voice 2 and Speex 1.0.5 linked as retail links them (Speex vendored unmodified, its tables
    match BF2.exe byte for byte).
  - Hooks: BF2Engine construction/registration/update/shutdown, GameLogic::startupVoipClient and its
    reset, Player::setVoipSessionId and the role keep-alives, the HUD (HudCommander's stop removed),
    and SwiffPlayer's VoIP natives, update tick and destructor tail.
  - `BF2_VOIP_CAPTURE=fake` [CHOSEN]: a synthetic GameSpy device and no mixer for tests and loopback
    gates (no microphone, no permission prompt); loopback.py sets it. The gate content keeps
    `sv.voipEnabled 0`; `loopback.py --voip` turns the local voice server on.
  - Live session (temporary probe, removed): run-20261113-074315 with `--voip`. The server announced
    voice at address 1, which the client resolved to the joined server (127.0.0.1:55125, local 55123,
    quality 3). The session started on the synthetic device and the server's session id (2772)
    reached the client through VoipSessionEvent and Player::setVoipSessionId. The per-frame update
    ran with transmit off while the player had no squad (retail rule). 0 FRONTIER.
  - The voice-setup settings for the setup assistant.
  - 8 sites closed (74 of 82).
- Batch 15 (mac/game-frontier-15), C1: the retail gameswf definition hierarchy (owner decision C1 (a),
  2026-11-13). The bounds bridge is removed (see the gameswf_impl.cpp:128 row). An ownership and cast
  audit found no reinterpret/C-style casts between the two sides of a sprite_definition, no void*
  round trips through a different base, and no pointer-identity containers of definitions. The C-style
  casts are movie_definition -> movie_definition_sub (primary base). Retail checks for a movie clip as a
  prototype (C2): docs/tracks/game/swf-proto-retail-probe.md and tools/swf/proto_probe.py. 1 site
  closed (75 of 82).
- Batch 16 (mac/game-frontier-16), N1 + P1, in progress. N1: FalconDevice::updateCurrentState is
  recovered against the retail NovintHFX ABI. The x86 image keeps retail's static import of
  NovintHFX.dll (the `Haptics` data export cannot be delay-loaded). The Mac check runs the actual
  device on a fake SDK (`BF2_NOVINT_FAKE_SDK`). 1 site closed (76 of 82).
- C2 (on mac/game-frontier-16, before P1): a movie clip on a prototype chain, from the retail probe
  (v2, 67788e01). The accessor walks end "not found" at a clip, a clip class prototype binds and runs
  the class with no constructor field, and addProperty on a clip is accepted and ignored. 3 sites
  closed (79 of 82). Remaining, all in batch 16: the two PunkBuster rows (P1, GameClient.cpp:878 and
  NetClient.cpp:527) and the Novint haptic force target (BF2ClientFrontiers.cpp:16, IHapticEffect +0x4C).
- Batch 16, haptic force targets: MainMenuLogic's zeroing of the six Novint force effects is recovered
  against NovintHFX::Vect3 and IHapticEffect +0x4C; BF2ClientFrontiers.cpp is gone. The control box is a
  BF2-side FalconControlBoxParams (retail stores its own vtable after the SDK constructor; winvm
  `falcon_vtable_restore.cmd` PASS at 0866b409). 1 site closed (80 of 82). Remaining: P1.
- Batch 16, P1: the PunkBuster client glue, the BF2-side PB_* functions, the two call sites and the three
  BF2Engine pieces (init call, command hook, frame pump). Retail loads pbcl.dll dynamically, which is
  rule 3's approach as is; macOS takes the not-installed branch. The server glue's entry points are
  srv-owned stand-ins (see "handed to srv"). 2 sites closed (82 of 82).
- Owner sign-off (2026-11-13, via main) on both boundaries: (a) PunkBuster as described above; (b) Novint,
  keeping retail's direct (static) import on Windows x86, and no-device on x64 and every non-Windows
  platform.
- 2026-11-15 owner decision (issue #17): the Novint Falcon is archived on every platform
  (`Code/Archive/NovintFalcon`). The N2 sites below and FalconDevice now take retail's no-device
  branch at their call sites; the archive README lists each one.
- Batch 17 (mac/game-frontier-17): GameLogic::killPlayer's force zeroing (N2) and the boundary sign-off
  record. The owner moved the rest of N2 to 0.3.0 and then back into v0.2.0 (2026-11-13); it was done in
  N2 batches 1-4 (see "N2 implementation").
- Batch 18 (mac/game-srv-18, merged 1994785c): the POSIX Mutex deadlock, reassigned from srv. See
  "srv items reassigned to game".
- Batch 19 (mac/game-srv-19, merged 6bb713e6): the PunkBuster server glue, reassigned from srv, with
  GameServer's two PunkBuster sites. The StatusMonitor stand-ins are gone. See "srv items reassigned to
  game".
- Batch 20 (mac/game-srv-20): build.cmd keeps PbClient.cpp out of bf2_w32ded, and the game fixture's
  FileManager fake covers the three library-presence functions.
- Batch 21 (mac/game-cam-21, on 6bb713e6): Camera::handleUpdate and its three shake/recoil helpers, plus
  CameraTemplate's constructor defaults; see "Found during closure".

Windows twin fixtures (2026-11-12): `Code/Tests/client_console_modules_runtime.cpp` still expected the
HUD stops batch 7 removed, and `profile_console_runtime.cmd` linked the HUD bindings without
`/FORCE:UNRESOLVED`. Both are fixed alongside batch 9: the stop checks are gone (the rows are covered on
macOS), and the profile twin links like its sibling. Neither is a gate. Verified on winvm at 75323d62
(MSVC x86 /O2): client_console_modules_runtime 1881/1881, profile_console_runtime 302/302.

Columns:
- **site**: file:line at the base above;
- **what**: what the stop guards;
- **retail**: the retail address(es);
- **closed by**: recovered / boundary;
- **test**: the covering test;
- **evidence**: the run or test that shows it.

## Menu (21)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `Menu/Bf2MenuMapNode.cpp:516` | Bf2MenuMapNodeTypeDescriptor::create: operator new(0xC80) + the constructor with six zeros, null on allocation failure. Retail's constructor dereferences the null System (780D8E, +0x358 loadTexture), so the path faults in retail too; no client code creates a map node by descriptor | BF2.exe 0x783410 (ctor 0x780180) | recovered (retail-faithful fault) | temporary live probe: create() from the per-frame ECSound 6000 handler | run-20261112-160845: faults at Bf2MenuMapNode.cpp:669 m_system->loadTexture, the instruction retail has at 0x780D8E; probe removed |
| `Menu/BfFixedSliderNode.cpp:76` | BfFixedSliderNode::streamProperties | BF2.exe 0x7D3D50 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `Menu/BfFixedSliderNode.cpp:79` | BfFixedSliderNode::paint | BF2.exe 0x7D4920 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `Menu/BfFixedSliderNode.cpp:82` | BfFixedSliderNode::update | BF2.exe 0x7D4A30 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `Menu/BfFixedSliderNode.cpp:85` | BfFixedSliderNode::updateSelf | BF2.exe 0x7D4250 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `Menu/BfFixedSliderNode.cpp:88` | BfFixedSliderNode::onEvent1 | BF2.exe 0x7D40A0 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `Menu/BfFixedSliderNode.cpp:92` | BfFixedSliderNode::onEvent2 | BF2.exe 0x7D4430 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `Menu/BfFixedSliderNode.cpp:95` | BfFixedSliderNode::onEvent3 | BF2.exe 0x7D4430 (shared) | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `Menu/BfListBoxData.cpp:31` | releaseListBoxCellResource: the +0x48 resource's +0x20 with the cell's resource index (typed IListBoxCellResource, [INFERRED] names). Retail never sets +0x48 (ctor 7C8034 zeroes it, not streamed, none of the 12 creation sites stores it), so the callers' null test always fails in retail | BF2.exe 0x7C4CF3 / 0x7C4A67 | recovered | `Code/Tests/game_frontier_check.cpp` listBoxCellResource (mock resource: clearCell/copyCell release, range) | 27/27 checks, macOS 2026-11-12 |
| `Menu/BfNewListBoxNode.cpp:747` | paintSelf resource finalization: the +0x48 resource's +0x2C after the rows when set (same interface; never set in retail) | BF2.exe 0x7C7906 | recovered | the null branch runs on every list-box paint (play-combat gate); slot read statically | gates of batch 5 |
| `Menu/BfNewListBoxNode.cpp:1064` | BfNewListBoxNode::onEvent3, the double-click release: onEvent2's gate minus the top-inset/pointer-owner/negative-row/empty tests, code 1 only, selectable read after the timing marks; runs the select action and clears the marks | BF2.exe 0x7C46D0 | recovered | `Code/Tests/game_frontier_check.cpp` listBoxEvent3 | 23/23 checks, macOS 2026-11-12 |
| `Menu/HudCommander.cpp:397` | HudCommander::appendLeaderVoipCells: the null-client stop is removed; retail reads bf2VoipClient +0xA0 (getUsingLeaderChannel) unguarded because the client image always constructs the voice client (BF2Engine::initEngine 0x40C351, recovered in this batch) | BF2.exe 0x79410A | recovered | `Code/Tests/hud_commander_runtime.cpp` (Windows twin `.cmd`): voice-enabled leader, default channel (palette frame, active icon) and leader channel (yellow frame), through a fake client answering slot +0xA0 | winvm pending (main) |
| `Menu/HudConsoleBindings.cpp:110` | HudConsoleBinding::execute: the base binding no longer stops; all 34 unimplemented rows of the A1781C list have typed invokers read from their tables (MSVC executeObjectMethod +5C): 24 manager/minimap/items/chat/radio/help rows and 10 hudBuilder rows, with the builder bodies (Bf2TileNode new) | BF2.exe 75C964..77007E (tables 926BB0..92F258), builder 795CC0..79C110, Bf2TileNode 7CB990..7CBBD0 | recovered | `scripts/hud-console.txt` and `scripts/hud-builder-console.txt` (full console access) | run-20261112-191139 (24 rows, 0 FRONTIER), run-20261112-200913 (16 builder commands, 0 FRONTIER; deleteNode ends at its own error-channel "Deleting" announcement, fatal with assertions as in retail) |
| `Menu/HudConsoleBindings.cpp:116` | HudConsoleBinding::executeObjectMethod: the base binding no longer stops; all 34 unimplemented rows of the A1781C list have typed invokers read from their tables (MSVC executeObjectMethod +5C): 24 manager/minimap/items/chat/radio/help rows and 10 hudBuilder rows, with the builder bodies (Bf2TileNode new) | BF2.exe 75C964..77007E (tables 926BB0..92F258), builder 795CC0..79C110, Bf2TileNode 7CB990..7CBBD0 | recovered | `scripts/hud-console.txt` and `scripts/hud-builder-console.txt` (full console access) | run-20261112-191139 (24 rows, 0 FRONTIER), run-20261112-200913 (16 builder commands, 0 FRONTIER; deleteNode ends at its own error-channel "Deleting" announcement, fatal with assertions as in retail) |
| `Menu/HudConsoleBindings.cpp:122` | HudConsoleBinding::setArgFromString: the base binding no longer stops; all 34 unimplemented rows of the A1781C list have typed invokers read from their tables (MSVC executeObjectMethod +5C): 24 manager/minimap/items/chat/radio/help rows and 10 hudBuilder rows, with the builder bodies (Bf2TileNode new) | BF2.exe 75C964..77007E (tables 926BB0..92F258), builder 795CC0..79C110, Bf2TileNode 7CB990..7CBBD0 | recovered | `scripts/hud-console.txt` and `scripts/hud-builder-console.txt` (full console access) | run-20261112-191139 (24 rows, 0 FRONTIER), run-20261112-200913 (16 builder commands, 0 FRONTIER; deleteNode ends at its own error-channel "Deleting" announcement, fatal with assertions as in retail) |
| `Menu/HudConsoleBindings.cpp:128` | HudConsoleBinding::getReturnValueAsString: the base binding no longer stops; all 34 unimplemented rows of the A1781C list have typed invokers read from their tables (MSVC executeObjectMethod +5C): 24 manager/minimap/items/chat/radio/help rows and 10 hudBuilder rows, with the builder bodies (Bf2TileNode new) | BF2.exe 75C964..77007E (tables 926BB0..92F258), builder 795CC0..79C110, Bf2TileNode 7CB990..7CBBD0 | recovered | `scripts/hud-console.txt` and `scripts/hud-builder-console.txt` (full console access) | run-20261112-191139 (24 rows, 0 FRONTIER), run-20261112-200913 (16 builder commands, 0 FRONTIER; deleteNode ends at its own error-channel "Deleting" announcement, fatal with assertions as in retail) |
| `Menu/ProfileConsoleBindings.cpp:98` | ProfileConsoleBinding::execute: every table's +54 is the shared 4C4510 (ConsoleObjectBaseImpl::execute) | BF2.exe 0x4C4510 | recovered | `tools/macos/loopback/scripts/profile-console.txt` (`--client-env BF2_CLIENT_CONSOLE_FULL_ACCESS=1 --client-seconds 240`); `Code/Tests/game_frontier_check.cpp` profileConsole | run-20261112-215606 (26 commands, getters print their values, applySettings renames the local player to Batch8Name on the scoreboard), 0 FRONTIER; 86/86 checks, macOS 2026-11-12 |
| `Menu/ProfileConsoleBindings.cpp:104` | executeObjectMethod: 17 typed invokers (bool commands, get/set rows, setIndexForKit/getIndexForKit, applySettings) over the recovered store: owner, PlayerProfile (0x38), CharacterProfile (0x30), ModSetting (0x84, [INFERRED] names) | BF2.exe 0x770358..0x771877, 0x7BF950..0x7C1340 | recovered | `tools/macos/loopback/scripts/profile-console.txt` (`--client-env BF2_CLIENT_CONSOLE_FULL_ACCESS=1 --client-seconds 240`); `Code/Tests/game_frontier_check.cpp` profileConsole | run-20261112-215606 (26 commands, getters print their values, applySettings renames the local player to Batch8Name on the scoreboard), 0 FRONTIER; 86/86 checks, macOS 2026-11-12 |
| `Menu/ProfileConsoleBindings.cpp:110` | setArgFromString: per-row, per-argument conversion slots (strings one token, ints operator>>) | BF2.exe 0x7703A3, 0x770724, 0x7715A4, 0x77176C and twins | recovered | `tools/macos/loopback/scripts/profile-console.txt` (`--client-env BF2_CLIENT_CONSOLE_FULL_ACCESS=1 --client-seconds 240`); `Code/Tests/game_frontier_check.cpp` profileConsole | run-20261112-215606 (26 commands, getters print their values, applySettings renames the local player to Batch8Name on the scoreboard), 0 FRONTIER; 86/86 checks, macOS 2026-11-12 |
| `Menu/ProfileConsoleBindings.cpp:116` | getReturnValueAsString: the shared formatters 7611C7 (bool), 439A64 (int), 7709E2 (string), 41BC76 (void) | BF2.exe 0x7611C7 / 0x439A64 / 0x7709E2 / 0x41BC76 | recovered | `tools/macos/loopback/scripts/profile-console.txt` (`--client-env BF2_CLIENT_CONSOLE_FULL_ACCESS=1 --client-seconds 240`); `Code/Tests/game_frontier_check.cpp` profileConsole | run-20261112-215606 (26 commands, getters print their values, applySettings renames the local player to Batch8Name on the scoreboard), 0 FRONTIER; 86/86 checks, macOS 2026-11-12 |
| `Menu/SwiffHost.cpp:1083` | SwiffHost::slot18C: ds:0xA23DC0 is g_bfSound, so it is a null-tested setMuteState(1, a) (+0xB4), muting sound groups 0..3; its one caller is voipStopTest, which only runs after a VoIP test starts. The last SWIFF_HOST_FRONTIER call: the macro is removed | BF2.exe 0x488920 | recovered | temporary live probe (slot18C(1) from the first menu sound) | run-20261113-013736: mute state 1 set, groups 0..3 target 0 (group 4 untouched), exit 0; probe removed |

## External/gameswf (19)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `External/gameswf/SwiffPlayer/OfflineConnectPopup.h:46` | openOfflineConnectPopup, main-menu popups clip absent: quiet decline, false and no dialog ([CHOSEN, not recovered] owner feature; retail offline never navigates: the flush gate exits for loginStatus < 2) | owner decision 2026-11-13 | recovered (owner decision) | `Code/Tests/swiff_offline_connect_popup.cpp` + `.sh`: missing-popup.swf (separate process, exit 0, one decline line) | 33/33 + 2/2, macOS 2026-11-13 |
| `External/gameswf/SwiffPlayer/OfflineConnectPopup.h:53` | openOfflineConnectPopup, popups connecttoip frame absent: quiet decline, false and no dialog ([CHOSEN, not recovered] owner feature; retail offline never navigates: the flush gate exits for loginStatus < 2) | owner decision 2026-11-13 | recovered (owner decision) | `Code/Tests/swiff_offline_connect_popup.cpp` + `.sh`: missing.swf (separate process, exit 0, one decline line) | 33/33 + 2/2, macOS 2026-11-13 |
| `External/gameswf/SwiffPlayer/OfflineConnectPopup.h:57` | openOfflineConnectPopup, popups connecttoip navigation failed: quiet decline, false and no dialog ([CHOSEN, not recovered] owner feature; retail offline never navigates: the flush gate exits for loginStatus < 2) | owner decision 2026-11-13 | recovered (owner decision) | `Code/Tests/swiff_offline_connect_popup.cpp` + `.sh`: in-process substitute clip whose goto fails after the label lookup | 33/33 + 2/2, macOS 2026-11-13 |
| `External/gameswf/SwiffPlayer/SwiffNativesRecovered.inl:854` | as_sound_voipInitialize: LocalProfileManager name, startTest(3, mixer) when idle, setVoipUpdateFlag, the per-profile AudioSettings push (playback, capture, boost, threshold) | SwiffPlayer.dll 0x10069010..0x100692A1 | recovered | temporary live probe: every VoIP native called through the running menu with the synthetic device (BF2_TEMP_PROBE_VOIP); `Code/Tests/voip_client_runtime.cpp` (CTest `bf2_voip_client_check`) for the client slots they call | run-20261113-071923 / run-20261113-072145: voipInitialize true (test started), the six setters echoed by their getters (0.6 / 0.4 / 0.25, boost refused on macOS, PTT off/on), voipStartTest loops back, voipSendingVoice true at level 0.366 over threshold 0.25 and false in the silent half, voipStopTest ends the loopback, voipSetEnable false/true flips the user switch; 0 FRONTIER; probe removed. bf2_voip_client_check 89/89 |
| `External/gameswf/SwiffPlayer/SwiffNativesRecovered.inl:866` | as_sound_voipSetEnable: AudioSettings setVoipEnabled and the client user switch (+0x98), true | SwiffPlayer.dll 0x100692B0..0x100693AF | recovered | temporary live probe: every VoIP native called through the running menu with the synthetic device (BF2_TEMP_PROBE_VOIP); `Code/Tests/voip_client_runtime.cpp` (CTest `bf2_voip_client_check`) for the client slots they call | run-20261113-071923 / run-20261113-072145: voipInitialize true (test started), the six setters echoed by their getters (0.6 / 0.4 / 0.25, boost refused on macOS, PTT off/on), voipStartTest loops back, voipSendingVoice true at level 0.366 over threshold 0.25 and false in the silent half, voipStopTest ends the loopback, voipSetEnable false/true flips the user switch; 0 FRONTIER; probe removed. bf2_voip_client_check 89/89 |
| `External/gameswf/SwiffPlayer/SwiffNativesRecovered.inl:900` | the 12 client rows (VOIP_CLIENT_ROW, removed): voipStartTest (test flag, host mute, loopback, threshold 0), set/get transmit and receive volume, set/get threshold (kept menu value, test-aware), set/get boost and boost support, set/get push to talk | SwiffPlayer.dll 0x100694E0..0x10069FD5 | recovered | temporary live probe: every VoIP native called through the running menu with the synthetic device (BF2_TEMP_PROBE_VOIP); `Code/Tests/voip_client_runtime.cpp` (CTest `bf2_voip_client_check`) for the client slots they call | run-20261113-071923 / run-20261113-072145: voipInitialize true (test started), the six setters echoed by their getters (0.6 / 0.4 / 0.25, boost refused on macOS, PTT off/on), voipStartTest loops back, voipSendingVoice true at level 0.366 over threshold 0.25 and false in the silent half, voipStopTest ends the loopback, voipSetEnable false/true flips the user switch; 0 FRONTIER; probe removed. bf2_voip_client_check 89/89 |
| `External/gameswf/SwiffPlayer/SwiffNativesRecovered.inl:925` | as_sound_voipSendingVoice: capture level against the menu threshold; toggles the loopback silence (client +0x95) | SwiffPlayer.dll 0x10069FE0..0x1006A0E4 | recovered | temporary live probe: every VoIP native called through the running menu with the synthetic device (BF2_TEMP_PROBE_VOIP); `Code/Tests/voip_client_runtime.cpp` (CTest `bf2_voip_client_check`) for the client slots they call | run-20261113-071923 / run-20261113-072145: voipInitialize true (test started), the six setters echoed by their getters (0.6 / 0.4 / 0.25, boost refused on macOS, PTT off/on), voipStartTest loops back, voipSendingVoice true at level 0.366 over threshold 0.25 and false in the silent half, voipStopTest ends the loopback, voipSetEnable false/true flips the user switch; 0 FRONTIER; probe removed. bf2_voip_client_check 89/89 |
| `External/gameswf/SwiffPlayer/SwiffNativesRecovered.inl:934` | as_sound_voipGetSendingVoiceVolume: the capture level while capturing, else 0 | SwiffPlayer.dll 0x1006A0F0..0x1006A1F2 | recovered | temporary live probe: every VoIP native called through the running menu with the synthetic device (BF2_TEMP_PROBE_VOIP); `Code/Tests/voip_client_runtime.cpp` (CTest `bf2_voip_client_check`) for the client slots they call | run-20261113-071923 / run-20261113-072145: voipInitialize true (test started), the six setters echoed by their getters (0.6 / 0.4 / 0.25, boost refused on macOS, PTT off/on), voipStartTest loops back, voipSendingVoice true at level 0.366 over threshold 0.25 and false in the silent half, voipStopTest ends the loopback, voipSetEnable false/true flips the user switch; 0 FRONTIER; probe removed. bf2_voip_client_check 89/89 |
| `External/gameswf/SwiffPlayer/SwiffPlayer.cpp:43` | the macOS GetKeyState shim: SwiffPlayer.dll calls GetKeyState once, with VK_CAPITAL (0x1004D457 pushes 0x14), so the shim is now that one query (capsLockKeyState) | SwiffPlayer.dll 0x1004D450 | recovered (no stop left) | menu smoke (isCapsLockPressed path unchanged) | menu quit run, 0 FRONTIER |
| `External/gameswf/SwiffPlayer/SwiffPlayer.cpp:186` | swiffFsCommand was a misidentification: the ctor's second gameswf registration is register_log_callback (0x1009AF40), not the fscommand setter (0x1008A1A0 has no caller). Recovered the log callback 0x10005160 ("swiffDebug" == 1 keeps each message's first line, at most 0x1FE characters, in a write-only static buffer) and dropped the fscommand registration (gameswf tests the null handler, as retail) | SwiffPlayer.dll 0x10006F79..0x10006F8F, 0x10005160 | recovered | temporary live probe (swiffDebug set to 1, copied lines printed) | run-20261113-012807: 374 first lines captured ("setFlaName mainMenu", "initialize", ...), exit 0, 0 FRONTIER; probe removed |
| `External/gameswf/SwiffPlayer/SwiffPlayer.cpp:3353` | dice.General.getListEntries' unread branches: retail's compare chain (69 compares, sub_10022610..0x10031A18) had 15 IDs with no branch here. Recovered missionList, serverHistory (no result), profileSearch, mods, teamSummary, dateOfBirth, gender, modfilter, suggestNicks, uweapons, custats, credits, age, ranklist; serverMessage and every unmatched name take the chain's shared empty exit (loc_1003147D: begin, totalCount 0, publish), which replaces the logging fallback | SwiffPlayer.dll 0x10022610..0x10031A18 | recovered | temporary live probe: every ID called through the native from the running menu, teamSummary in a joined game (it reads gameLogic unguarded, as retail 0x499380) | run-20261113-022316 (menu: missionList 8, mods 2, dateOfBirth 90/12/31, gender 2, modfilter 1, credits 1747, age 89, the rest empty offline, unknown name empty) and run-20261113-022354 (teamSummary 0/1: one row each), 0 FRONTIER; probe removed |
| `External/gameswf/SwiffPlayer/SwiffPlayer.cpp:3924` | as_sound_voipStopTest: host unmute, loopback off through VoipClient::setLocalLoopback (header-inline, as the DLL links its copy at 0x100BC8B0), threshold restored to client and AudioSettings | SwiffPlayer.dll 0x10069620..0x1006974B | recovered | temporary live probe: every VoIP native called through the running menu with the synthetic device (BF2_TEMP_PROBE_VOIP); `Code/Tests/voip_client_runtime.cpp` (CTest `bf2_voip_client_check`) for the client slots they call | run-20261113-071923 / run-20261113-072145: voipInitialize true (test started), the six setters echoed by their getters (0.6 / 0.4 / 0.25, boost refused on macOS, PTT off/on), voipStartTest loops back, voipSendingVoice true at level 0.366 over threshold 0.25 and false in the silent half, voipStopTest ends the loopback, voipSetEnable false/true flips the user switch; 0 FRONTIER; probe removed. bf2_voip_client_check 89/89 |
| `External/gameswf/SwiffPlayer/SwiffPlayer.cpp:3948` | voipStopTestFrontier removed with its last call site; the update tick (0x100060D6: base update when not in a session) and the destructor tail (0x10007030: loopback off, readVoiceSettings threshold) recovered with it | SwiffPlayer.dll 0x100060D6, 0x10007030 | recovered | temporary live probe: every VoIP native called through the running menu with the synthetic device (BF2_TEMP_PROBE_VOIP); `Code/Tests/voip_client_runtime.cpp` (CTest `bf2_voip_client_check`) for the client slots they call | run-20261113-071923 / run-20261113-072145: voipInitialize true (test started), the six setters echoed by their getters (0.6 / 0.4 / 0.25, boost refused on macOS, PTT off/on), voipStartTest loops back, voipSendingVoice true at level 0.366 over threshold 0.25 and false in the silent half, voipStopTest ends the loopback, voipSetEnable false/true flips the user switch; 0 FRONTIER; probe removed. bf2_voip_client_check 89/89 |
| `External/gameswf/gameswf_action.cpp:1466` | is_key_down outside Windows: SwiffPlayer.dll asks GetAsyncKeyState only for Enter (0x1008A247 / 0x1008CF3A); the macOS player installs the host adapter at init, and no other POSIX client builds SwiffPlayer. With no adapter (a bare test harness) Enter reads up ([CHOSEN]) | SwiffPlayer.dll 0x1008A230 | recovered (no stop left) | `Code/Tests/swf_iface_runtime.py` missing-async-input now expects "up" | macOS build 2026-11-13 |
| `External/gameswf/gameswf_action.cpp:2105` | as_object::get_property with a movie clip on the prototype chain: the walk ends "not found" (retail reads unrelated movie state at the script-object offsets, finds no getter and does not fault); get_member then finds the clip's ordinary members (tests 1 green, 2 red) | SwiffPlayer.dll 0x10099EB0 | recovered (retail-observed) | `Code/Tests/swf_proto_probe_runtime.cpp` (CTest `bf2_swf_proto_probe_check`: probe_all and probe1..9 must show retail's squares) | retail 1.50 probe v2 (67788e01, Boot Camp, `swf-proto-retail-20261113/v2-67788e01`); native 120/120, macOS 2026-11-13 |
| `External/gameswf/gameswf_action.cpp:2134` | as_object::set_property with a movie clip on the chain: no setter runs and the value becomes an own member (tests 3 red, 6 green). With it, addProperty on a clip receiver (a silent null dereference in the native build, same family) is accepted and kept nowhere: retail does not fault and never finds the getter (test 5 red) | SwiffPlayer.dll 0x10099D80, 0x1009A0F0 | recovered (retail-observed); addProperty [CHOSEN] accept-and-ignore | `Code/Tests/swf_proto_probe_runtime.cpp` (CTest `bf2_swf_proto_probe_check`: probe_all and probe1..9 must show retail's squares) | retail 1.50 probe v2 (67788e01, Boot Camp, `swf-proto-retail-20261113/v2-67788e01`); native 120/120, macOS 2026-11-13 |
| `External/gameswf/gameswf_impl.cpp:128` | get_definition_bounds and definition_bounds_provider are removed with the retail definition hierarchy: ref_counted is a shared virtual base; resource and character_def derive from it; movie_definition derives from it and is no character_def; sprite_definition has both bases; the bounds slot is pure on character_def (+0x20); callers dispatch through it. The three root-definition reads (ID, registered class, a movie definition as a character_def) answer [CHOSEN] 0 / none: retail reads movie_def_impl members that 0x10077FB0 zeroes and that load may fill (undefined) | SwiffPlayer.dll character_def table 0x100DF268 +0x20, ctors 0x1007B5A0 (sprite_definition) / 0x10077FB0 (movie_def_impl), 0x1007F240, 0x100A1F90, 0x100759CF, 0x10081911 | recovered (owner decision C1 (a), 2026-11-13) | swf twins: `Code/Tests/swf_iface_runtime.py` (iface 703 with the new hierarchy checks: abstract character_def, movie definitions not character_defs, the root definition not castable), text layout 62, font raster 340, archive 703, bitmap cache 416; offline popup 2/2 | macOS 2026-11-13; the unmapped-definition stop test is gone: an abstract character_def cannot be instantiated (retail _purecall) |
| `External/gameswf/gameswf_impl.cpp:5308` | bind_class_prototype with a movie clip as class prototype: the instance is created, the registered class runs once, and the clip is the instance's __proto__ (tests 4, 7, 8 green; 9 is the control). The constructor field retail reads at the clip is unrelated movie state, used only by super(): [CHOSEN] none | SwiffPlayer.dll 0x10081870, 0x10081A12, 0x10081A7E | recovered (retail-observed); __constructor__ [CHOSEN] none | `Code/Tests/swf_proto_probe_runtime.cpp` (CTest `bf2_swf_proto_probe_check`: probe_all and probe1..9 must show retail's squares) | retail 1.50 probe v2 (67788e01, Boot Camp, `swf-proto-retail-20261113/v2-67788e01`); native 120/120, macOS 2026-11-13 |
| `External/gameswf/gameswf_render.cpp:35` | render_handler::set_depth is pure: SwiffRenderHandler (SwiffPlayer.h) is the only render_handler subclass compiled in any image (macOS or MSVC); the tree holds no upstream backend source (no ogl/d3d/xbox handler files), and the three Windows test observers supply set_depth | SwiffPlayer.dll render interface | recovered (no stop left) | build (both images); the swf twins' observers override it | macOS build 2026-11-13 |

## External/BFMeme (10)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `External/BFMeme/NativeSavedDispatch.h:42` | not retail code: a platform invariant (virtual members only); relabelled [ASSERT] | platform layer | reclassified | `bf2_meme_dispatch_check` death mode (part of bf2_vtable_audit) | vtable audit 139/0 |
| `External/BFMeme/Node.cpp:221` | Node +A0: the 455410 false default for every class in the tree; edit nodes 7AA9B0 as before. BfEditNodeInt 7AACF0 / PathNode 836A80 come with those classes | BF2.exe 0x455410, 0x7AA9B0 | recovered | `Code/Tests/game_frontier_check.cpp` (CTest `bf2_game_frontier_check`, built on request) | 16/16 checks, macOS 2026-11-12 |
| `External/BFMeme/SliderNode.cpp:85` | SliderNode::streamProperties | BF2.exe 0x833660 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `External/BFMeme/SliderNode.cpp:88` | SliderNode::paint | BF2.exe 0x833B80 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `External/BFMeme/SliderNode.cpp:91` | SliderNode::update | BF2.exe 0x833C90 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `External/BFMeme/SliderNode.cpp:94` | SliderNode::updateSelf | BF2.exe 0x833810 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `External/BFMeme/SliderNode.cpp:97` | SliderNode::onEvent1 | BF2.exe 0x833900 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `External/BFMeme/SliderNode.cpp:101` | SliderNode::onEvent2 | BF2.exe 0x833A30 | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `External/BFMeme/SliderNode.cpp:104` | SliderNode::onEvent3 | BF2.exe 0x833A30 (shared) | recovered | `Code/Tests/slider_node_behaviour.cpp` (CTest `bf2_slider_node_check`; `.cmd` twin) | 88/88 checks, macOS 2026-11-12 |
| `External/BFMeme/Stream.cpp:21` | getTreeClassType is dice::meme::Tree::getClassType: Tree (12 bytes: root, owned name) and TreeNode (0x14: previous, next, child, parent) recovered with the delete, properties, save and load walkers (presence marks through streamBool "Bool"), createNode/deleteNode/slot 0x40; published on first use (only caller streamTree 0x837F80) | BF2.exe 0x83C8C0..0x83D0F0, tables 0x94F440/0x94F490, descriptor 0x94F4D8 | recovered | `Code/Tests/game_frontier_check.cpp` memeTree (descriptor/create/name, save marks, load round trip with links, properties, destroy count, truncated load) | 42/42 checks, macOS 2026-11-12 |

## Sound (5)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `Sound/BfSound.cpp:927` | BfSound::drawStats: the lines go to "DebugTextWriterFixed" +0x2C queueGroupColoured twice, black shadow at (1,1) then green at (0,0), writer unchecked (typed local IDebugTextWriter, as AI DebugRenderer) | BF2.exe 0x7E08D0 (0x7E0E0A..0x7E0E63) | recovered | `loopback.py --input-script sound-stats --client-env BF2_CLIENT_CONSOLE_FULL_ACCESS=1` (sound.showStats 1 after spawning) | run-20261112-145111: 23-line block drawn green with shadow, 0 FRONTIER |
| `Sound/BfSound.cpp:949` | BfSound::drawSoundSources: per playing sample the V/O/P text at the projected position (clip-volume test, black +2 / white +1), a blue 0.4 cross, blue/green circles at min/max distance, a red velocity line; ISoundEngine +0xB4 now returns the 0x2C-byte PlayingSampleInfo list (was [CHOSEN] int) | BF2.exe 0x7E0E80; BF2Audio.dll 0x10004B80 returns none | recovered | `sound-sources` script with a temporary engine patch reporting two samples (retail never has any) | run-20261112-155354: reaches the debug line/circle drawers, which stop at the render-owned Metal frontier "unmapped render state 9 = 1" (D3DRS_SHADEMODE flat), reported to main |
| `Sound/SoundEngine/OpenALSample.cpp:256` | bufferData decoder-less branch removed: the vendored Vorbis decoder is now required (bf2audio.cmd opt-out BF2AUDIO_NO_VORBIS and the BF2AUDIO_WITH_VORBIS guards gone), as the shipped DLL links its own | BF2Audio.dll (static libvorbis) | removed (dead build variant) | every sound-on session decodes retail .ogg (ambient, commander VO); play-combat gate | vtable audit 139/0; gates of this batch |
| `Sound/SoundEngine/OpenALSample.cpp:382` | streamStart decoder-less branch removed (as bufferData) | BF2Audio.dll | removed (dead build variant) | as bufferData | as bufferData |
| `Sound/SoundEngine/SoundFile.cpp:346` | loadOgg decoder-less branch removed (as bufferData) | BF2Audio.dll | removed (dead build variant) | as bufferData | as bufferData |

## Animation (3)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `Animation/BoneAnimation/ModuleBoneAnimation.cpp:463` | RagDollTemplate::writeConstraints (console ragDoll.save): one ragDoll.addConstraint <boneA> <boneB> <length> line per distance constraint through io::File to ragDollConstraints.inc (mode 0, access 2); getBoneDistance computed and discarded, as shipped | ELF 0x8341110 (caller 0x8822215) | recovered | `loopback.py --ai-spawn-only --input-script ragdoll-debug --client-env BF2_CLIENT_CONSOLE_FULL_ACCESS=1` | run-20261112-154305: 18 lines (retail ships 18 addConstraint lines). Batch 5: retail writes the relative name into the game's mods/bf2; the macOS client never writes the installed game, so it uses @HOME@ (clientDataDirectory); Windows/Linux keep retail's write. run-20261112-163003: client-home/Library/Application Support/OpenBF2/ragdollconstraints.inc, nothing under client-root |
| `Animation/BoneAnimation/RagDoll.cpp:771` | RagDoll::drawDebugLine: the debug cylinder (CID 81016) stretched from-to, 0.002 thick, coloured through IID 80007 +0xF8 (typed local interfaces); no caller in the ELF and no BF2.exe copy, kept as the ELF has it | ELF 0x8343F40 | recovered | shares queueSegment with drawConstraints (below) | as drawConstraints |
| `Animation/BoneAnimation/RagDoll.cpp:778` | RagDoll::drawConstraints: every distance constraint as a red segment between its particles; renderer looked up once (BF2.exe layout), geometry unchecked | BF2.exe 0x7E7160 (ELF 0x83458D0) | recovered | `ragdoll-debug` script (ragDoll.drawConstraints 1, ragdolls from suicide/combat) | run-20261112-153837: 400+ calls with the published renderer and 18 constraints, 0 FRONTIER (temporary probe, removed) |

## Physics (3)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `Physics/MaterialManager.cpp:102` | removeMaterial id >= 10000: retail's own trap, kept (__debugbreak / abort), no longer a frontier | BF2.exe int3, ELF 0x810E990 abort | retail trap | `Code/Tests/game_frontier_check.cpp` (CTest `bf2_game_frontier_check`, built on request) death check | 16/16 checks, macOS 2026-11-12 |
| `Physics/MaterialManager.cpp:312` | setAttMaterial id >= 10000: retail's own trap, kept | BF2.exe 0x5ECB10 int3 | retail trap | `Code/Tests/game_frontier_check.cpp` (CTest `bf2_game_frontier_check`, built on request) death check | 16/16 checks, macOS 2026-11-12 |
| `Physics/MaterialManager.cpp:332` | setDefMaterial id >= 10000: retail's own trap, kept | BF2.exe int3 | retail trap | `Code/Tests/game_frontier_check.cpp` (CTest `bf2_game_frontier_check`, built on request) death check | 16/16 checks, macOS 2026-11-12 |

## Game/Main (3)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `Game/Main/BF2ClientFrontiers.cpp:16` | hapticTarget_vslot0x4C: the six targets are the NovintHFX::IHapticEffect objects the haptic-effect code creates (Stack::CreateNewEffect("CONSTANT", ConstantEffectParams, "ShakeForce", &target), 5A415C..5A41C5; ledger N2). MainMenuLogic::activate builds one NovintHFX::Vect3(0, 0, 0) and calls each non-null effect's +0x4C (SetForce [INFERRED]) in retail's order 0x9EBF00, F04, F08, F0C, EC8, ECC. Now `io::zeroHapticForceTargets` in FalconDevice.cpp ([CHOSEN] out of line, because MainMenuLogic builds on every platform; the POSIX body is empty since no Falcon effect exists there). The seam file is removed | BF2.exe 0x4A3490..0x4A3535 (and the users 4ED64E, 5AB641, 5ADDA5, N2) | recovered; boundary: owner signed off 2026-11-13: Novint — retail's direct (static) import on Windows x86, no-device on x64 and every non-Windows platform | `Code/Tests/falcon_update_runtime.cpp` (CTest `bf2_falcon_update_check`: order, null skip, one shared Vect3), CTest `bf2_novint_msvc_x86_layout` (Vect3 24 bytes) | 40/40 checks, macOS 2026-11-13 |
| `Game/Main/BF2Engine.cpp:869` | GeometryTemplateManager::clear through the typed interface in every client build (the POSIX non-static branch is gone) | BF2.exe ds:0x9A5578 +0x28 | recovered | menu-quit gate (every client shutdown) | run-20261112 g7 gates |
| `Game/Main/BF2Render.cpp:74` | BF2Render::render VTune bracket: ds:0x9A5C6C is profilerClient, published by Core Module::init (PE 0x68CC83 write, ELF 0x8ABBCA8) from the "Profiler" singleton; +0x2C pauseVTune unguarded as in retail. demo.cpp begin/endMainLoop pauseVTune(false) recovered with it (0x8141CF0 / 0x8142EB3) | BF2.exe 0x45342B / 0x4534DC | recovered | `loopback.py --play-demo <demo> --demo-config-line "demo.timedemo 1" --demo-config-line "demo.vtuneframe 1000" --demo-config-line "demo.vtunerendering 1|0" --demo-config-line "demo.vtunerepeatcount 3"` (frame 1000: frame 30 is still in level load, where this block does not render) | run-20261112-140744 (rendering 1: bracket false/true around 3 passes), run-20261112-140055 (rendering 0: begin/end pauseVTune), 0 FRONTIER, verified with temporary probes |

## Game/GameClient (3)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `Game/GameClient/GameClient.cpp:406` | GameClient::update on a lightmap-rendering node: DistributedLightRenderer +0x28 update, then quit once a connection that was up has gone (byte_9A7758). Recovered with the whole class (BF2.exe 0x46B5B0..0x46C690, ELF 0x80C15A0..0x80C4300) and its factory row; recovered-but-unreachable-in-retail: the node switch is option 35, which retail never registers | BF2.exe 0x4D57B6..0x4D5806; 0x46BED0 | recovered (unreachable in retail) | `Code/Tests/game_frontier_check.cpp` distributedLightRenderer (factory/identity, object and terrain queues, resources, block 4 contents, overlay lines, node trace and reports); temporary live probe (node flag forced at startup) | 133/133 checks, macOS 2026-11-12; run-20261112-234332: the node banner "Distributed lightmapGeneration in progress...connected to server" draws from t60 while connected, 0 FRONTIER; run-20261112-235048: the loss returns to the menu (the game shuts down, so no further update reaches the latch in one session); probe removed |
| `Game/GameClient/GameClient.cpp:878` | GameClient::receive's PunkBuster packets: PB_handlePacket(data, size, from) (Game/PunkBuster/PbGame.cpp). ffffffff datagrams of 8+ bytes: types other than S 2 G I Y B L go to the client glue (event 0x0D), types other than C 1 J to the server glue with the matching player's index. The BF2-side PB_* functions (paths, version, cl_punkbuster, text, sending, PB_examineSetting) are recovered with it. The GAMECLIENT_FRONTIER macro is gone | BF2.exe 0x4D4F43; 0x4D9DE0; PB_* 0x4D94D0..0x4DA49B | recovered; boundary: owner signed off 2026-11-13: PunkBuster — retail's dynamic pbcl.dll load on Windows, the not-installed branch elsewhere, fake-DLL test | `Code/Tests/punkbuster_game_runtime.cpp` (CTest `bf2_punkbuster_game_check`: fake games, console and glue entry points); the server-side player match is covered since batch 19 (generated PlayerManager stubs) | macOS 2026-11-13: punkbuster client 42/42, punkbuster game 34/34; mutations fail both |
| `Game/GameClient/GameClient.cpp:2486` | GameClient::handleDataBlock block 4 on a lightmap-rendering node: DistributedLightRenderer +0x24 performTraceObject(data, size), then return. Unreachable in retail (option 35, as above; the server half that sends block 4 is srv's) | BF2.exe 0x4D5FAE; 0x46B5B0 | recovered (unreachable in retail) | `Code/Tests/game_frontier_check.cpp` distributedLightRenderer (factory/identity, object and terrain queues, resources, block 4 contents, overlay lines, node trace and reports) (performTraceObject with object, terrain, missing-object and missing-generator jobs) | 133/133 checks, macOS 2026-11-12 |

## Game/LocalProfileManager (2)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `Game/LocalProfileManager.h:47` | LocalProfileManager PE+30, hasCurrentProfile(int): m_currentProfile != 0 | BF2.exe 0x4A7690 | recovered | `Code/Tests/game_frontier_check.cpp` (CTest `bf2_game_frontier_check`, built on request) | 16/16 checks, macOS 2026-11-12 |
| `Game/LocalProfileManager.h:48` | LocalProfileManager PE+34, slot34(int,int): true | BF2.exe 0x4553F0 (shared) | recovered | `Code/Tests/game_frontier_check.cpp` (CTest `bf2_game_frontier_check`, built on request) | 16/16 checks, macOS 2026-11-12 |

## IO (Falcon / PunkBuster boundary) (2)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `IO/Input/FalconDevice.cpp:24` | dice::hfe::io::FalconDevice::updateCurrentState: the sample loop (smoothed deltas over a three-sample window, the input latency), mouse mode (StartMouse) or control-box mode (a ControlBoxEffectParams "BOX"/"ControlBox" effect built from HapticSettings, with dead zones and turn sensitivity), the button state, and the axis scaling, including retail's fourth scale over `m_axisTravel[0..1]`. NovintHFX.h now declares the effect classes; the 44-slot vtable and all 32 new import names match BF2.exe (vtable 0x9116E0, IAT) | BF2.exe 0x6A52A0 (ELF 0x8499C80, loop compiled out) | recovered; boundary: owner signed off 2026-11-13: Novint — retail's direct (static) import on Windows x86, no-device on x64 and every non-Windows platform | `Code/Tests/falcon_update_runtime.cpp` (CTest `bf2_falcon_update_check`, fake SDK; Windows twin `falcon_device_runtime.cmd`, stock DLL), CTest `bf2_novint_msvc_x86_layout` | 37/37 checks, macOS 2026-11-13; mutations of the delta scale, the fourth scale and the capture return each fail |
| `IO/Network/ProtocolLayer/NetClient.cpp:527` | handleConnectAccept with the server's PunkBuster bit: PbClientInitialize(0) then PbClientConnect() (event 0x75). The client glue (Game/PunkBuster/PbClient.cpp) loads pbcl.dll dynamically as retail does (pbclnew.dll installed over pbcl.dll first, GetProcAddress "ca"/"cb"), hands it the six game callbacks and pumps it. Without the DLL, and on every platform but Windows (the library seam has no loader there and touches no file), it shows "PunkBuster Client Not Installed" once and turns cl_punkbuster off | BF2.exe 0x6BD461..0x6BD470; glue 0x435900..0x436434, static image ds:0x96E5A8 | recovered; boundary: owner signed off 2026-11-13: PunkBuster — retail's dynamic pbcl.dll load on Windows, the not-installed branch elsewhere, fake-DLL test | `Code/Tests/punkbuster_client_runtime.cpp` (CTest `bf2_punkbuster_client_check`: fake pbcl.dll through the library seam, fake PB_* and server trap) | macOS 2026-11-13: punkbuster client 42/42, punkbuster game 34/34; mutations fail both |

## Found during closure, not a [FRONTIER]

Silent omissions count under the strict policy (main, 2026-11-13). These have no stop in the tree, so the
`[FRONTIER]` scan above does not list them.

| gap | what | retail | planned | test | evidence |
|---|---|---|---|---|---|
| N2: Novint haptic effects | About 140 BF2.exe sites drive NovintHFX force-feedback effects from game systems. The tree had none of these calls, and no [FRONTIER] stop marked the gap. Done in batch 17: GameLogic::killPlayer (0x4ED340): every return after the kill latch zeroes the six force effects when the local player dies (8116e0f2); MainMenuLogic::activate's zeroing came with batch 16 (fb97399e). The owner first moved the rest to 0.3.0, then back into v0.2.0 (2026-11-13). The rest is now done (N2 batches 1-4): Soldier constructor / onEnter / onExit, FreeCamera, Camera ShakeForce, Soldier Recoil and CraftPhysics, the RecoilPunch envelopes (soldier and weapon recoil components, kits), the Damage envelope, and the vehicle CraftPhysics. All 143 recorded sites are covered; see "N2 implementation" below. Windows x86 Falcon only | BF2.exe NovintHFX imports 0x87F674..0x87F7F0; sites listed below | done (N2 batches 1-4; MSVC check on winvm) | bf2_falcon_update_check, bf2_soldier_haptic_profile_check, bf2_camera_shake_check, falcon_vtable_restore.cmd (winvm) | macOS 2026-11-13 |
| P1: BF2Engine PunkBuster pieces | Three silent omissions, labelled [CHOSEN] with no stop: init's PbServerInitialize + mainConsole->setHandleCommandHook(HandleCommandHookPB); HandleCommandHookPB (PB_SV_ to the server trap, other PB_ to the client trap, true for any PB_ command; the dedicated ELF has only the server arm); the per-frame pump (client glue, then the server glue when the game is a server) | BF2.exe 0x40C9EB..0x40C9FD, 0x402FD0 (ELF 0x812E690), 0x40D5CB..0x40D5FE | recovered in batch 16 ([CHOSEN] placement: the hook lives in Game/PunkBuster/PbCommandHook.cpp so it can be tested alone) | `bf2_punkbuster_client_check` (routing with fakes on both sides) | macOS 2026-11-13 |
| Camera::handleUpdate (batch 21) | Camera had no handleUpdate (it inherited RotationalBundle's), a silent omission found during N2: the inside camera's four roll components stayed zero (no speed, explosion, close-vehicle shake, no recoil bob), m_frameTime never advanced (the trace camera's recording reads it) and the occupant's free-camera view switch never ran. Recovered: handleUpdate, updateCameraRecoil, updateCameraShake, updateCloseVehicleShake, the TU-local HeavyEnginePredicator / HeavyEngineForcePredicator, the reference-backed tuning constants with their shipped values. CameraTemplate gets retail's constructor defaults (shake sensitivity 0.1, start/max speed 0, bounce, inside lighting); without them a camera whose content sets no shake speeds shook constantly. BF2.exe's Novint ShakeForce block in the same function belongs to N2 | ELF 0x81DBA90, 0x81DC1D0, 0x81DC270, 0x81DC550, 0x81E00E0..0x81E0250, ctor 0x81DE0C0; BF2.exe 0x5A3EA0, 0x5A1A10, 0x5A1A90, 0x5A3830, ctor 0x5A3210 | recovered (batch 21) | live probe (temporary, never committed), scripts `tools/macos/loopback/scripts/camshake-*.txt`. Batch 21: `jeep_faav_PilotCamera` reads start 20 / max 30 and its speed shake runs only above 20 m/s; the frame clock advances. Batch 22: recoil (Karkand, `usapc_lav25_Driver_Camera`: one sine period per shot, peak 0.179 = 0.6 x cameraRecoilSize 0.3), explosion shake (own grenade: decaying explRoll, peaks about 1.56), close-vehicle shake (Karkand Gasstation while bots board the armour: the HeavyEngineForcePredicator term 0.348, soldier camera pitch +/-0.06). Not exercised: the HeavyEnginePredicator term (mass >= 10000, the same code path with a higher threshold; no occupied vehicle that heavy came within 7 m in two tries). Captures before/after (the jeep drive, the grenade window) are archived; the views (a wall, a jeep below 20 m/s) do not show the roll, so the numbers are the evidence. Regression test: `bf2_camera_shake_check` (Code/Tests/camera_shake_runtime.cpp; the real Camera.cpp, generated fakes, the rest of the engine as aborting stubs from tools/macos/link_stubs.py): recoil, speed shake (start/max, clamp, no range, no physics), explosion decay, both close-vehicle scans (mass 10000 / 2000, mobile, occupied, flags, distance, own root, view modes), the on-foot phases, and handleUpdate (frame clock, local occupant, scan interval, flags & 4); 28/28, eleven mutations each fail it | macOS 2026-11-13: runs -161517..-164038 (probe, batch 21), -170958/-171435/-172145/-172543/-173021 (probe, batch 22), captures -173633/-174241, -174609/-175200, -175437/-175626 (before/after), gate -164701 (play-combat 0 FRONTIER), -165401 (menu quit), demo_cycle -165537/-170047 PASS |
| handed to srv: PunkBuster server glue | Was: stand-ins in Game/Main/StatusMonitor.cpp tagged "OWNER: srv". Reassigned to game and recovered in batch 19; see "srv items reassigned to game" below | BF2.exe 0x436440..0x437349 | recovered (batch 19) | see below | see below |

## srv items reassigned to game (2026-11-13)

main moved these from srv to this track after the frontier closure. GameServer is otherwise srv's scope.

| site | what | retail | state | test | evidence |
|---|---|---|---|---|---|
| `Core/Mutex.cpp` (batch 18) | POSIX mutexes are recursive: [CHOSEN, not recovered], a deliberate divergence from the Linux retail image (null attributes, ELF 0x83D2CC0) in favour of the Windows behaviour (critical sections). The voice server's Shutdown -> ReleaseAllInstances relocks (ELF 0x84EC920 -> 0x84EC6CD) and hung on Darwin's default mutex. A dedicated quit never reaches that shutdown (the ELF main has no engine shutdown), which is retail behaviour. [CHOSEN, not recovered] owner-approved 2026-11-13 (in place of a linked issue): matches the Windows retail image (CRITICAL_SECTION recursion) on every platform; deviates from the Linux retail image, whose non-recursive mutex self-deadlocks in CBF2VoipServer::Shutdown. [PLATFORM] retail defect of the Linux image: Mutex::Mutex (ELF 0x83D2CA0 / 0x83D2CE0) calls pthread_mutex_init(p, NULL); Shutdown (ELF 0x84EC900) locks this+8 at 0x84EC920 and ReleaseAllInstances relocks it at 0x84EC6CD | ELF 0x83D2CA0 / 0x83D2CE0 (init 0x83D2CC0), 0x84EC900 / 0x84EC920 / 0x84EC6CD | divergence by decision, owner-approved 2026-11-13 | `bf2_mutex_check` (relock with a watchdog); `bf2_voip_server_shutdown_check` (real CBF2VoipServer start/stop; the old mutex exits 2) | macOS 2026-11-13: mutex 5/5, voip server shutdown 4/4; merged 1994785c (MSVC x86/x64 clean) |
| `Game/PunkBuster/PbServer.cpp` (batch 19) | The server glue, both images, one body: stPbSv (static image: magic 0x357AFE20, "PunkBuster Server", reload 1, state 999; x86 offsets asserted), makefn (home pb folder created and seeded with pbsv/pbcl/pbag from the base folder when they differ), LoadServerDll (pbsvnew installed over pbsv, keeping pbsvold; symbols "sa"/"sb"), AddPbEvent / ProcessPbEvents, the five game callbacks, and every exported entry point (PbServerInitialize, PbSvAddEvent, PbSvAddStatEvent, PbServerTrapConsole, PbServerProcessEvents, PbServerForceProcess, PbServerCompleteCommand, PbPassConnectString, PbAuthClient, PbCaptureConsoleOutput, isPbSvEnabled, EnablePbSv, DisablePbSv). Per image: library names follow the platform (pbsv.dll, '\\', chmod 0x180 / pbsv.so, '/', 0x1FF); the console capture pair lives in globals in BF2.exe (ds:0x99EF78/7C) and in pbsv +0x16C/+0x170 in the ELF. Same boundary as the client glue: retail's dynamic load on Windows; elsewhere the library seam has no loader, so engine init takes the not-installed path (the notice is dropped: PB_outputText needs a game, which does not exist yet, as in retail; the loopback server log has no PunkBuster line), sv_punkbuster is switched off and no file in the pb folder is touched | BF2.exe 0x436440..0x437349, ds:0x96E758; ELF 0x84F6020..0x84F7340 (symbols), ds:0x8AC82A0 | recovered | `bf2_punkbuster_server_check` / `bf2_punkbuster_server_client_check` (fake pbsv through the seam, scratch pb folders, fake PB_*); `bf2_punkbuster_msvc_x86_layout` / `bf2_punkbuster_elf_x86_layout` | macOS 2026-11-13: 44/44 in both images; four mutations each fail a check; gate run-20261113-144216 (play-combat, 0 FRONTIER), -144546 (menu quit), demo_cycle -144613/-144722 PASS |
| `Game/PunkBuster/PbGame.cpp` (batch 19) | The BF2-side PB_* functions now build in both images. Added: PB_set_sv_punkbuster, PB_maxClients, PB_getPlayerInfo (name, CD-key hash, "ip:port" or "localhost", "team=N"), PB_issueConsoleCommand, PB_DropClient (PResult 0x22), PB_sv_sendToClient, PB_get_sv_punkbusterInstalled. The client's return-0 callback is PB_GlQuery ([INFERRED] by its place in the ELF's function order). Dedicated image: no PB_cl_sendToServer, no client forward in PB_handlePacket, no "team" setting, and PB_homepath reads GSPunkBusterPath | BF2.exe 0x4D94D0..0x4DA49B; ELF 0x84F7340..0x84F8A90 | recovered | `bf2_punkbuster_game_check` (client) / `bf2_punkbuster_game_dedicated_check`; since batch 20 a generated FileManager fake covers PB_set_sv_punkbuster, PB_get_sv_punkbusterInstalled and PB_set_cl_punkbuster (library missing / present) | macOS 2026-11-13: client 49/49, dedicated 48/48; four mutations fail the image they target. Batch 20: client 54/54, dedicated 53/53; two more mutations fail both |
| `Game/GameServer/GameServer.cpp` cdkey_auth_callback (batch 19) | The `[FRONTIER]` punkBusterAuthClient stand-in is gone: an authorized client on a PunkBuster server goes through PbAuthClient(ip:port, 1, key hash); a text kicks with PResult 34, 0 admits | ELF 0x80A4CB7 | recovered | server glue check (auth hook and its absence) | macOS 2026-11-13 |
| `Game/GameServer/GameServer.cpp` processReceivedPackets (batch 19) | PB packet delivery, previously disabled by a project decision: each filtered PunkBuster datagram goes to PB_handlePacket(data, size, sender name) while m_isPunkBusterEnabled, then is freed | ELF 0x80AA199 / 0x80AAC94 | recovered | game check (server-side player match) | macOS 2026-11-13 |
| `Game/GameServer/GameServer.cpp` init (batch 19) | The inline pb/pbsv file test is PB_get_sv_punkbusterInstalled again | ELF 0x80A7500, BF2.exe 0x4CF940 | recovered | game check (PB_get_sv_punkbusterInstalled, batch 20) | macOS 2026-11-13 |
| `Code/Build/build.cmd` (batch 20) | The MSVC dedicated link (bf2_w32ded) no longer takes PbClient.cpp: the client glue is BF2.exe-only, every caller is `#if !DEDICATED`, and CMake already lists it in bf2_client_only_sources (it was the only entry build.cmd lacked) | BF2.exe 0x435900..0x436434 (client only) | fixed | bf2_w32ded link on winvm | pending (orchestrator) |

### N2 implementation (v0.2.0; was the 0.3.0 handoff)

All remaining sites only matter on Windows x86 with a Falcon connected. Effect creation is guarded by the
device (ds:0xA03940) and the force effects stay null without it, so x64 and non-Windows keep retail's
device-null path: the blocks become no-ops there, and nothing runs today.

| PE function | sites | what | identification |
|---|---|---|---|
| 0x5A3EA0 | 13 | creates the two CONSTANT "ShakeForce" effects (ds:0x9EBEC8/CC) and sets Vect3(x, y, 0) from HapticSettings +0x7C4..+0x7E4 times object fields +0x2EC/+0x34C/+0x354/+0x358 | **Camera::handleUpdate** (slot +0xAC of vtable 0x8E7840; constructor 0x5A4F90; createObject 0x5A5610 new(0x360); template constructor 0x5A3210 registered as "dice.hfe.world.ObjectTemplate.Camera" at 0x55EFC2) |
| 0x63EC60 | 7 | zeroes the six effects when the local player handles input in an unparented object, then server-side trigger-9 work | **FreeCamera::handlePlayerInput** (slot +0xFC of vtable 0x8FE7D0; constructor 0x63EEC0; createObject 0x63F570 new(0x2F4); template constructor 0x63DCD0 registered as "…ObjectTemplate.FreeCamera" at 0x55F0BE) |
| 0x5AB490 | 8 | zeroes the effects and sets the control-box rebuild flag (ds:0x978FC8) | **Soldier::onExit()** (2026-11-13): slot +0x2C of Soldier's IPlayerControlObject vtable 0x8E7F80 at +0x154 (ELF Soldier secondary at offset 0x13C, same interface, no destructor, so the same index). Soldier by the template anchor: constructor 0x5AC3A0 <- createObject 0x5F4840 new(0x3F0) <- template vtable 0x8F17C8 +0x50 <- template constructor 0x5F4A90 <- creator 0x506DC4 "dice.hfe.world.ObjectTemplate.Soldier" |
| 0x5AC3A0 | 1 | sets the rebuild flag | **Soldier::Soldier** (2026-11-13): the constructor in the Soldier anchor above (base constructor 0x626E70, then the five vtable stores) |
| 0x5AC9F0..0x5ADE5E | 8 | rebuild flag + zeroing | **Soldier::onEnter** (2026-11-13): one function (the int3s inside are inline debug breaks, not padding), reached through slot +0x28 of the +0x154 vtable (`lea ecx, [edi - 0x154]`) |
| 0x5ADEA0..0x5AF5D9 (part) | 9 | "Recoil" Constant into ds:0x9EBF00 | **Soldier::handlePlayerInput** (2026-11-13): Soldier primary slot 63 (ELF slot 64, after the two-slot destructor); 0x5ADF55 is an early return, the body runs to 0x5AF5D9 |
| 0x5ADEA0..0x5AF5D9 (part) | 28 | "CraftPhysicsX/Y/Z" Constant into ds:0x9EBF04/08/0C | **Soldier::handlePlayerInput** (2026-11-13), the same function as the row above |
| 0x5E28D0 | 6 | kit/wrench "RecoilPunch" Envelope | **GenericFireArm::postFire** (0x5E28D0 itself; ret 0x1C, its seven parameters; the block is the else of m_recoilComp->postFire at +0x198, ELF +0x180). Earlier partial note (2026-11-13): called from 0x5E3510, which 0x5E3ED0 calls; 0x5E3ED0 is slot +0xAC (handleUpdate's slot, as for PlayerControlObject) of vtable 0x8ED768 (constructors 0x5E4830 / 0x5E4DC0; 0x5E4830 is also the base constructor of the class whose template vtable 0x8985B0 creates it via 0x540EB0). That template is instantiated at runtime (0x613D55), not through a registration string, so the class is not named yet. 0x5E2EA0 is the params' deleting destructor |
| 0x5E5DA0 | 9 | "Damage" Envelope | caller identified (2026-11-13): **ServerGameLogic::giveDamage** (0x4E0B20, slot +0xE4 of vtable 0x8AA9E8; PE and ELF slots agree here; class registered as "dice.bf.serverGameLogic", creator 0x43734C new(0x37C), constructor 0x4E18E0). The helper itself is unnamed |
| 0x5FAA10 | 26 | "CraftPhysicsX/Y/Z" Constant | **PlayerControlObject::handleUpdate** (2026-11-13): PE slot +0xAC of PlayerControlObject's vtable 0x8F2430 (constructors 0x5F97D0 / 0x5FA210), inherited by GrapplingHookRopeContainer (vtable 0x8EB9F8, the 0x8EBAA4 reference). Anchor: GHRC constructor 0x5D4310 <- createObject 0x5D48D0 new(0x470) <- template vtable 0x8EBCE8 +0x50 <- template constructor 0x5D44F0 <- creator 0x5072AA "ObjectTemplate.GrapplingHookRopeContainer"; the ELF GHRC constructor calls PlayerControlObject's, and ELF +0xB0 (PE + 4 after the destructor) is PlayerControlObject::handleUpdate, not overridden by GHRC, as in the PE |
| 0x6092B0 | 5 | "RecoilPunch" Envelope | **SoldierBasedRecoilComp::postFire** (2026-11-13): slot +0x38 of the component's +0x10 vtable 0x8F6138 (primary 0x8F61E4; createComponent 0x6090A0 -> constructor 0x608F80; template SoldierBasedRecoilCompTemplate vtable 0x8F6388, creator 0x50767C "TemplateComponent.SoldierBasedRecoilComp"). ELF secondary +0x38 is SoldierBasedRecoilComp::postFire, the only override in that window. The earlier "0x8F60F0 +0x80" was the same address read from the template base vtables |
| 0x609F50 | 5 | firing-port "RecoilPunch" Envelope | **WeaponBasedRecoilComp::postFire** (2026-11-13): slot +0x38 of the component's +0x10 vtable 0x8F63E0 (primary 0x8F648C; createComponent 0x60A690 at template vtable 0x8F65DC +0x18; creator 0x5076BB "TemplateComponent.WeaponBasedRecoilComp"); ELF secondary +0x38 is WeaponBasedRecoilComp::postFire |

Implemented on mac/game-n2-impl (lands after the v0.2.0 cut): 17a, the Soldier constructor / onEnter / onExit
profile switch and zeroing plus FreeCamera's zeroing (bf2_soldier_haptic_profile_check); 17b, 0x5A3EA0's
ShakeForce block as io::updateShakeForce, called from Camera::handleUpdate for the local occupant
(bf2_falcon_update_check: creation, the scaled terms, the caps and their sign, the soldier/vehicle choice;
bf2_camera_shake_check: the hand-off of the four terms); 17b, Soldier::handlePlayerInput's "Recoil"
(io::updateRecoilForce + updateSoldierRecoilForce) and "CraftPhysics" blocks
(io::updateSoldierCraftPhysicsForce + updateSoldierCraftPhysics; bf2_falcon_update_check and
bf2_soldier_haptic_profile_check). Not under test: the two one-line hand-offs in Soldier.cpp itself.
17b, the "RecoilPunch" envelopes of SoldierBasedRecoilComp::postFire (0x6092B0) and WeaponBasedRecoilComp::postFire
(0x609F50) and serverGiveDamage's "Damage" envelope (0x5E5DA0): io::soldierRecoilPunch, io::weaponRecoilPunch,
io::fireRecoilPunch, io::updateDamageForce (bf2_falcon_update_check); the three call sites are not under test.
17b, the kit "RecoilPunch" (0x5E28D0, now identified as GenericFireArm::postFire: the else of
m_recoilComp->postFire): io::kitRecoilPunch (bf2_falcon_update_check); the call site is not under test.
17b, PlayerControlObject::handleUpdate's "CraftPhysics" (0x5FAC8C..0x5FBCDB): updateVehicleCraftPhysics +
io::updateVehicleCraftPhysicsForce, sharing the soldier's core (bf2_falcon_update_check, bf2_soldier_haptic_profile_check);
the call site is not under test.

Method that works (the template anchor): constructor → its createObject (`new(size)` + constructor call) → the
template vtable slot holding createObject → the template constructor (its vtable store) → the per-type creator
that news the template → the registration code that pushes "dice.hfe.world.ObjectTemplate.<Name>". Components
register the same way under createComponent names. Then align the PE vtable with the ELF class vtable
(tools and notes from the batch kept outside the repository: elfslot.py, vtmatch2.py,
harvest2.py, pe2elf.txt, n2_sites.txt with all 143 site addresses; ELF index = PE index + 1 after the destructor).
Second pass (2026-11-13, game track, low priority): ten rows named, two partial; scratchpad tool `anchor.py` (constructor -> createObject -> template vtable -> creator -> registration) copied to the n2-tools folder. Inline `int3` debug breaks split functions in a linear listing: take boundaries from `ret` plus references, not from int3 runs.
Methods that did not work: slot-count matching (adjacent PE vtables merge), BF2_r.exe (2006, no haptic code),
the ELF (haptic code and strings compiled out), REA/Ghidra (provider JVM fails to start).
SDK surface still to declare: ConstantEffectParams and EnvelopeEffectParams (the same 44-slot shape as
ControlBox; retail import names in the IAT), Vect3 operator[] / operator= / operator* / Normalize, and the
remaining IHapticEffect slots as they are met.

## AI (1)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `AI/AIInterface/BFKnowledge/BFGame.cpp:106` | BFGame::getAISettingsFromGame, the menu arm in the dedicated image: IGeneralSettings and ILocalProfileManager are now declared in every image (implementations stay client-only), so both images compile the one arm. No dedicated module registers "GeneralSettings", so the dedicated image never takes it. The last AIInterface `BF2_AI_FRONTIER` call, so AIFrontier.h and its 4 unused includes are removed | BF2.exe 0x815C50, bf2 0x83FED10 (identical calls and slots) | recovered | `Code/Tests/game_frontier_check.cpp` bfGameMenuArm (no profile manager / no profile, plain mode, sp1 forced 16, IAISettings override); dedicated build compiles the arm | 143/143 checks, macOS 2026-11-13 |

## VoipServer (9)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `VoipServer/VoipDebug.cpp:78` | CDebugRoutingState::Start: opens ./mods/bf2/Logs/Voip/VoipServerRouting_Instance_<stamp>.log; bool, false while running | ELF 0x84F2800 | recovered | `Code/Tests/voip_debug_runtime.cpp` (CTest `bf2_voip_debug_check`; twin `voip_debug_runtime.cmd`), log text read back | 38/38 checks, macOS 2026-11-12 |
| `VoipServer/VoipDebug.cpp:86` | CDebugRoutingState::OutputFullState: packet name, [ROUTING BLOCK], [CLIENT TABLES] (inet_ntoa of the session-id key, as shipped), [OUTGOING CHANNEL TABLES]; bool true | ELF 0x84F2BF0 | recovered | `Code/Tests/voip_debug_runtime.cpp` (CTest `bf2_voip_debug_check`; twin `voip_debug_runtime.cmd`), log text read back | 38/38 checks, macOS 2026-11-12 |
| `VoipServer/VoipDebug.cpp:98` | CDebugRoutingState::OutputRDMismatch: old/new routing byte of one player; bool true | ELF 0x84F29A0 | recovered | `Code/Tests/voip_debug_runtime.cpp` (CTest `bf2_voip_debug_check`; twin `voip_debug_runtime.cmd`), log text read back | 38/38 checks, macOS 2026-11-12 |
| `VoipServer/VoipDebug.cpp:117` | CDebugVoiceTrace::Start: VoiceTraffic_Player_<name|NO NAME>_<id>_<stamp>.log; bool | ELF 0x84F3B60 | recovered | `Code/Tests/voip_debug_runtime.cpp` (CTest `bf2_voip_debug_check`; twin `voip_debug_runtime.cmd`), log text read back | 38/38 checks, macOS 2026-11-12 |
| `VoipServer/VoipDebug.cpp:129` | CDebugVoiceTrace::Output: one line per frame to/from the traced connection, frame word at length-4, stops past 29999 frames; bool true | ELF 0x84F3D90 | recovered | `Code/Tests/voip_debug_runtime.cpp` (CTest `bf2_voip_debug_check`; twin `voip_debug_runtime.cmd`), log text read back | 38/38 checks, macOS 2026-11-12 |
| `VoipServer/VoipDebug.cpp:134` | CDebugVoiceTrace::Reset: close/delete, clear the three fields (also the destructor; the class has no vtable) | ELF 0x84F4100 / 0x84F3B00 | recovered | `Code/Tests/voip_debug_runtime.cpp` (CTest `bf2_voip_debug_check`; twin `voip_debug_runtime.cmd`), log text read back | 38/38 checks, macOS 2026-11-12 |
| `VoipServer/VoipDebug.cpp:140` | CDebugVoiceTrace::Stop: Reset when the connection matches; bool | ELF 0x84F3D60 | recovered | `Code/Tests/voip_debug_runtime.cpp` (CTest `bf2_voip_debug_check`; twin `voip_debug_runtime.cmd`), log text read back | 38/38 checks, macOS 2026-11-12 |
| `VoipServer/VoipDebug.cpp:145` | CDebugVoiceTrace::IsRunning: m_stream != 0 | ELF 0x84F40E0 | recovered | `Code/Tests/voip_debug_runtime.cpp` (CTest `bf2_voip_debug_check`; twin `voip_debug_runtime.cmd`), log text read back | 38/38 checks, macOS 2026-11-12 |
| `VoipServer/VoipServerInstance.cpp:1010` | CBF2VoipServerInstance::HandlePacketDebugRoutingData: 1625-byte name table only (else -4), starts the routing dump, copies the names | ELF 0x84EFAB0 | recovered | `Code/Tests/voip_debug_runtime.cpp` (CTest `bf2_voip_debug_check`; twin `voip_debug_runtime.cmd`), log text read back | 38/38 checks, macOS 2026-11-12 |

## Platform/X87Compat (1)

| site | what | retail | closed by | test | evidence |
|---|---|---|---|---|---|
| `Platform/X87Compat.h:42` | a GCC/Clang long double narrower than x87 extended selects the soft ExtendedFloat Real (BF2X87_SOFT_EXTENDED) instead of aborting; static_assert otherwise, MSVC exempt (binary64 long double is retail's own compiler semantics; the Windows arms never enter bf2x87; MSVC unchanged from before) | platform layer (retail is x87) | recovered | `Code/Tests/game_frontier_check.cpp` (CTest `bf2_game_frontier_check`, built on request) (game_frontier_x87_narrow.cpp, __LDBL_MANT_DIG__=53) | 16/16 checks, macOS 2026-11-12 |


## Follow-ups after v0.2.0

- **build.cmd's dedicated link vs CMake's client-only list.** CMake's `bf2_client_only_sources` has 180
  files; build.cmd's diagnostic bf2_w32ded link still takes 108 of them (all of External/BFMeme,
  Menu/Bf2* and Hud*, LocalProfileManager*, ProfileManager, AudioSettings, DistributedLightRenderer).
  They cause no unresolved externals today. Game/VoipClient and PbClient are already excluded
  (419ac8c7, batch 20). Align the two lists with a winvm build.cmd run before and after. Agreed with the
  orchestrator 2026-11-13.
- **N2 call sites.** Each N2 block's logic is under test through its helper (bf2_falcon_update_check,
  bf2_soldier_haptic_profile_check, bf2_camera_shake_check), but the one-line hand-offs in Soldier.cpp,
  SoldierBasedRecoilComp.cpp, WeaponBasedRecoilComp.cpp, GenericFireArm.cpp, GameLogicDamage.cpp and
  PlayerControlObject.cpp are checked against the disassembly only. Camera's hand-off is covered.
