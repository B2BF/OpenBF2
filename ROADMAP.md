# OpenBF2 roadmap

This roadmap leads from the current state to 1.0.0. The versions after 0.3.0 are a direction, not a schedule; their scope will change as work is done. Deliberate deviations from retail behaviour are decided in issues before they are implemented ([AGENTS.md](AGENTS.md)).

## Where we are: 0.2.0

- **Clients:** macOS (Metal, universal app with setup assistant, signed and notarized), Windows x64 and x86. The x86 client is the one for PunkBuster servers.
- **Dedicated servers:** Windows x86, Linux x64.
- **Game code:** every place where the game used to stop or skip work behaves as the original, each with a test.
- **Features:** voice chat, demo recording and playback, the Flash menus on the recovered SwiffPlayer engine, Special Forces night maps on macOS.
- **macOS performance:** shaders compile in the background during the level load, and the GPU runs up to two frames ahead.

## 0.3.0: online play and polish

The main goal is playing online again: with a GameSpy replacement provider, and ranked play against a self-hosted backend.

1. **Online with a provider.** The provider is selected with `+gamespy <domain>`, and the default stays the retail `gamespy.com`.
   - Testing against a local GameSpy emulator: a test-only name redirect that leaves system DNS untouched, plus the protocol fixes the emulator needs (account creation, keepalive and status messages, challenge format).
   - A dedicated server registers on the internet list, clients see it in the server browser, and a player joins with the CD-key check.
   - Account creation, login and profile flows, checked by hand.
2. **Ranked play against a self-hosted backend** (emulator plus bf2stats): server snapshots, BFHQ (stats, awards, leaderboards), unlocks and the rank shown in game. Two decisions are open first:
   - the PunkBuster requirement for ranked servers;
   - the retail ranking-authorisation check.
3. **Performance:**
   - the remaining GPU waits during level load (texture locks that read the texture back first);
   - first-use stalls of opaque draws on a fresh install.
4. **Windows tested end to end:**
   - x64 and x86 clients from the release package: sound through the bundled OpenAL Soft, the missing-DirectX message, CD keys from `Registry.dat` and from the registry;
   - the Windows installer (setup project), in per-user and all-users mode.
5. **Menu fixes:** the PREFIX label disappears after Manage Account → Back; tab header gradient; text sharpness compared with Windows.
6. **macOS:**
   - a first test on Apple-silicon hardware;
   - a decision on case-sensitive volumes, which the client can't use today.

## 0.4.0: renderer completeness

- The remaining Direct3D 9 features the game itself never reaches, completed in the Metal backend (fixed-function paths, D3DX helpers).
- Full shader lowering for the old shader models. The Special Forces night maps use an interim higher-profile route today.
- No hard stops left in the Metal renderer for unimplemented operations.
- A continuous-integration check for the default Linux build.

## 0.5.0: anti-cheat and 64-bit only

- A PunkBuster replacement written as portable, platform-independent 64-bit C++ (#23). Then x86 builds have no remaining purpose and can be dropped: one x64 client and server per platform.
- An x64 editor package.
- Windows code signing.

## Towards 1.0.0

1.0.0 means a complete, tested and releasable game on every supported platform:

- **Reconstruction:** every shipped image (game, renderer, menu engine, audio, AI, voice) is recovered or deliberately replaced, and every deliberate deviation is recorded in an issue.
- **Platforms:** macOS (Apple silicon and Intel), Windows x64 and Linux, all with signed release packages; Linux and macOS dedicated servers where useful.
- **Online:** online and ranked play work with at least one public provider and a self-hosted backend.
- **Quality:** no known crashes in normal play, and the scripted gates (render, combat, demo, menu) run in continuous integration where the platform allows.
- **Clean-up:** no temporary diagnostics left in the shipped code, and working notes kept out of the repository.
