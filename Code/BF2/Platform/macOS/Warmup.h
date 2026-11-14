// [CHOSEN, not recovered] macOS port addition, not retail: +warmupLevel <level> (BF2Engine option 74).
// The client hosts <level> locally through the retail non-menu start path (BF2Engine::startGame's
// shared body, as a developer command-line host does), lets the level load build every shader
// (ResourceManager::postLoadLevel runs the RaShader builds), waits for the background effect
// compiles to finish, and exits. The setup assistant runs it once per shader set to fill the
// native effect cache before the first game. No UI: machine-readable lines on stdout, e.g.
//   warmup: start strike_at_karkand
//   warmup: effects 812/1397
//   warmup: level loaded 61.2 s
//   warmup: done effects 1397/1397 failed 0 63.4 s
// and the exit status says how it ended (ExitCode below).
#pragma once
#include <string>

namespace bf2warmup {

enum ExitCode {
    ExitOk = 0,            // level loaded, every effect build finished and published
    ExitStartFailed = 2,   // the local host / level start failed
    ExitTimeout = 3,       // no finished level load within the time limit
    ExitBuildFailures = 4, // finished, but some D3DX-accepted effects failed to build natively
};

// Configures the warm-up (parseParameters); empty level = off.
void configure(const std::string& level);
bool active();
const std::string& level();
// The profile whose settings the warm-up runs with (empty: none, engine defaults).
void profile(const std::string& name);
// Called by the renderer at the end of ResourceManager::postLoadLevel.
void levelLoaded();
// The listen server created the local player (GameServer startup).
void localPlayerJoined();
// The renderer drew a world frame from the player's view (LightManager's shadow-caster pass).
void worldFrame();
// True once per attempt while the warm-up waits at the map-loaded screen (level loaded, local player
// joined, no world frame yet): the caller then issues the JOIN GAME action. Attempts are 5 s apart.
bool wantsJoinGame();
// Effect builds (Platform/Gfx native route): a native compile was started / finished.
void effectStarted();
void effectFinished(bool ok);
// A game-ending menu event (renderMenu type 8/13/14/17) was dropped (BF2Engine::renderMenu).
void menuEventDropped(int type);
// Start path result (BF2Engine::startGame); a failure exits with ExitStartFailed.
void startResult(bool ok);
// Once per frame (BF2Engine::mainLoop): progress lines; exits once the level has loaded, the local
// player has joined, a world frame has been drawn and every started effect build has finished with
// 10 s quiet, or when the time limit (BF2_WARMUP_TIMEOUT s, default 3600) passes.
void tick();

} // namespace bf2warmup
