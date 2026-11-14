# Work tracks after the AI subproject: shared rules

Owner decision (2026-11-03): the AI subproject is closed. The remaining work against retail 1.50 is split into
tracks. Each track runs as its own session in its own worktree, organised like the AI subproject. A main
coordinating session merges green track states into `mac-metal` and runs the owner's hand-play sessions on request.

| Track | Session name | Branch / worktree | Owns (edits only here) | Start |
|---|---|---|---|---|
| rend: renderer + Apple silicon | `rend-track` | `rend-track` | `Code/BF2/RendDX9`, `RendMisc`, `Geom`, `GeomMisc` (after the PR #60 sync; decided 2026-11-03), `Scene`, `Platform/Gfx`, Metal bridge, `tools/macos/nativefx`, `Code/Tests/macos_c_arm64`, `Code/StaticPlugins` (the `__APPLE__` exit-time deinit, 2026-11-04) | now (priority) |
| srv: server, network, stability | `srv-track` | `srv-track` | `Game/GameServer`, `IO`, `Platform` net/thread/process code, `Core/Thread*`, `Core/Mutex`, `Core/SystemInfo`, `Core/Timer`, `Core/PluginDll`, `Core/Module`, `Core/ModuleRegistry` (decided 2026-11-03) | second |
| game: gameplay and UI frontiers | `game-track` | `game-track` | `Game` (except GameServer, DemoRecorder, MatchMaking, Persistence), `Menu` (except HudVoip), `Physics`, `Sound`, `Animation`, `Core` (except the srv files), `External/BFMeme`, `External/gameswf` (except VoIP natives) | third |
| feat: demo, VoIP, PunkBuster | `feat-track` | `feat-track` | `Game/DemoRecorder`, `VoipServer`, `Menu/HudVoip`, Swiff VoIP natives, PunkBuster interface | after the owner's decisions (see its START) |
| online: ranked, stats, GameSpy | `online-track` | `online-track` | `Game/MatchMaking`, `Game/Persistence`, GameSpy SDK glue | last |
| swf: SwiffPlayer gameswf full recovery | `swf-track` | `swf-track` | `External/gameswf` (except the `SwiffPlayer/` natives and VoIP natives), `External/base` (taken from game while it runs, owner 2026-11-05) | after rend reports 100% |

Not a goal for now: the matching (byte-identical) builds. The owner dropped that goal on 2026-11-05 and may take
it up later; the AI subproject's matching-build plan (kept outside the repository) stays as the model. Exact behaviour (rule 3) still applies.

## Rules for every track

1. **Branches.** Work on your track branch; agents use sub-worktrees `<track>-<tag>` on
   branches `<track>/<tag>`, merged into the track branch by the track session. Never commit to `main`,
   `mac-metal` or `mac/c-asan-heap`. **Never push anything** (owner, 2026-11-02: no pushes to origin until the
   owner says so).
2. **Stay current.** After the main session reports a new `mac-metal` head, merge `mac-metal` into your track
   branch before the next agent starts (agent worktrees branched from an old head showed already-fixed bugs
   to the owner).
3. **Fidelity.** As close to retail as possible; change only for processor-architecture or platform reasons.
   Keep the original TU/function structure where it costs nothing (a matching build may come back later). Platform
   code goes behind `#if` with a `[CHOSEN, not recovered]` label. AGENTS.md evidence rules apply. Never copy
   from other reconstructions (inspiration only). Never commit game-derived content.
4. **Ownership.** Edit only the paths your track owns (table above). Need a change elsewhere: message the
   owning track session (and copy the main session), and record it in your `HANDOFF.md`. Shared headers that
   several tracks touch: the main session decides. `Game/Common/GhostManager.cpp` stays game-owned; srv changes to
   its weak `io::NetworkManager` members go through game-track (decided 2026-11-03).
5. **Ledger and progress (recoverability).** Each track keeps `docs/tracks/<track>/ledger.csv` (one row per
   binary function it owns: `image,address,size,symbol,file,status,commit`, status
   `todo|done|stop|skip|chosen`), generated with a script under `tools/tracks/` (model:
   `tools/ai-reconstruct/build_ledger.py`), and a progress log (a dated line per merge, running agents, next
   items) with the track's other notes outside the repository (rule 11). On resume: read this README and the
   track's start brief, progress log and hand-off notes; check `git worktree list` for unmerged
   `<track>-*` worktrees; continue.
6. **Gates before you report a state green:**
   - Build in your worktree: `cmake --build out/build/<track> --target bf2_client bf2 bf2_aidll bf2_vtable_audit`
     (configure like the integration build, incl. `-DBF2_FX_NATIVE_DEPS=<nativefx build>` (see
     `docs/building.md`) and `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`). **Required:** `-DBF2_MACOS_LINK_RENDDX9=ON -DCMAKE_OSX_ARCHITECTURES=x86_64`;
     without the renderer the client crashes at login in `HudInformationAdapter::setMapStatic` (this=0), seen 2026-11-04. The vtable audit stays at 0 failures.
     Optional for new build dirs: ccache (`-DCMAKE_C_COMPILER_LAUNCHER=ccache
     -DCMAKE_CXX_COMPILER_LAUNCHER=ccache`). Adding it to an existing dir rebuilds it once. Cached
     objects can carry another worktree's path in debug info and `__FILE__`, so don't use a path in a log as evidence
     of which worktree built it. Don't install or upgrade anything through brew (it broke cmake once, 2026-11-03).
   - Smoke: `python3 tools/macos/loopback/loopback.py --build <track> --ai --bots 16 --ai-spawn-only
     --input-script play-combat --client-seconds 200`: client runs to its timeout, no new `[FRONTIER]`.
   - Demo: `python3 tools/macos/loopback/demo_cycle.py --build-dir out/build/<track>`: records 60 s with 8 bots
     on the server, plays the file back on the client; must print `demo_cycle: PASS`.
   - Your track's own test (in its START).
7. **Runs.** Only through `tools/macos/loopback/` (sandboxed, offline, port 16890/29911, never 16567; the
   machine-wide lock serialises runs, queueing is normal). The game installation is read-only.
   Judge scripted runs from logs. **Non-trivial mechanics are tested by the owner by hand** (vehicles,
   flying, revive, capture, chat, menus): prepare the session and a short numbered checklist; ask the main
   session to start it, or ask the owner directly in your session.
8. **Shared tools are serialised (owner, 2026-11-03).** The rea MCP/Ghidra session and `tools/tracks/` are shared
   by all tracks: only one track works on shared tooling or runs Ghidra analysis at a time. A track that needs another
   track's tool or export waits until that track has committed it to mac-metal, then imports the committed version.
9. **Budget.** Shared account limits. At most 2 track sessions active at the same time besides the main
   session, and each track runs at most 1 agent at a time. Filter logs, never dump them.

10. **Cleanup after merge (owner, 2026-11-04).** The disk filled at 06:06 on 2026-11-04 (ENOSPC for every session)
   from ~20 finished agent worktrees with ~1.5 GB build dirs each. Right after a track merges an agent branch into
   its track branch: (1) check the branch is merged (`git merge-base --is-ancestor <branch> HEAD`) and the worktree
   has nothing uncommitted outside `out/`; (2) copy what the notes cite (run directories they name:
   client.log, client.result, client.command.txt, a few key present PNGs, any bundle artifact) into the track
   worktree's `out/evidence-runs/`; (3) `git worktree remove --force <track>-<tag>` (the
   branch stays). Check `df -h ~` before starting a batch of agents and keep at least ~20 GB free; never let
   finished agent worktrees accumulate. Build dirs and worktrees fill the disk; loopback run folders are mostly
   APFS clones and free little. Remove only your own track's merged, clean worktrees.
11. **Keep slice notes out of the repository (owner, 2026-11-14).** Slice reports, progress logs, hand-off
   notes, agent task prompts and progress files, audit tables and run logs are working material: keep them
   outside the repository, never under `docs/`. `docs/` holds only the guides listed in `docs/README.md` and the
   data tables that committed tools and tests read or write. Anything a developer needs later goes into a
   source comment beside the code (with its retail evidence) or into one of those guides; never cite a note
   from code. Before you report, check that `git status` shows no new files under `docs/`.

## Communication (same as the AI subproject)

- **Live:** a message to the session name (main session, or the track names in the table).
- **Durable:** the track's hand-off note, two sections: "Requests from main / other tracks" (R1, R2, ...,
  appended by the sender) and "Reports to main" (the track's merge-ready states and findings). It lives with
  the track's notes outside the repository (rule 11) and is never committed.
- **Merge request to main:** message the main session: track branch, commit hash, what changed, gate results
  (build, audit, smoke, track test, run directories). The main session merges via `mac/c-asan-heap`, reruns
  build + audit + smoke, fast-forwards `mac-metal` (local only), and replies with the new head.
- **Hold:** if you find a regression in something you reported green, message the main session at once
  ("hold <hash>"), as the AI session did with e2.
