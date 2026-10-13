// [CHOSEN] Separate translation unit for engine, demo, variable and game console bindings.
// ModuleBF2 registers and unregisters this list. Binding availability and access levels
// differ between dedicated and client images.

#include "../../Core/ConsoleObjectMacro.h"
#include "../../Core/Ids.h"     // IID_IGame - game.radioMessage's interface query
#include "BF2Engine.h"
#include "Game.h"               // operator>>(istream&, TargetChannel&) - game.radioMessage's converter
#include "../Utility/Vars.h"    // vars.set's receiver - Vars::set(std::string, std::string)
#include "../IGameClient.h"     // the demo playback controls' receiver
#include "../../Scene/Object/PlayerManager.h"   // game.radioMessage's sender - the local human
#if !DEDICATED
#include "BF2ClientSeams.h"     // BF2Render::toggleFullscreen - game.toggleFullscreen's target
#endif
#include <string>

namespace dice {
namespace hfe {

extern BF2Engine* bf2Engine;    // ds:0x8AACBD8 - defined in Core/Globals.cpp

namespace {

// Stands in for the original's file-scope list (ELF 0x8AD45A8). ModuleBF2::init registers it.
const char* const kGameConsoleTag = "hfe.game";

} // namespace

// Demo recording commands at access level 1, available while the engine exists. Recording itself
// needs a server game; BF2Engine::recordDemo reports the refusal on a client.
BF2_CONSOLE_OBJECT("demo", 1, 0)

namespace {
void demoRecordDemo(const std::string& name)
{
    ::dice::hfe::bf2Engine->recordDemo(name);
}

void demoStopRecording()
{
    ::dice::hfe::bf2Engine->stopRecording();
}

void demoShutdownDemo()
{
    ::dice::hfe::bf2Engine->shutdownDemo();
}
} // namespace
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(recordDemo, demoRecordDemo, ::dice::hfe::bf2Engine != 0, std::string, kGameConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(stopRecording, demoStopRecording, ::dice::hfe::bf2Engine != 0, kGameConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(shutdownDemo, demoShutdownDemo, ::dice::hfe::bf2Engine != 0, kGameConsoleTag)

// Demo playback controls at access level 2, driven by the HUD's demo radio menus. They act on the
// game client, so a dedicated server never activates them.
BF2_CONSOLE_OBJECT("demo", 2, 0)

namespace {
IGameClient* queryGameClient()                          //  [CHOSEN] name
{
    return ::dice::hfe::game
        ? static_cast<IGameClient*>(::dice::hfe::game->queryInterface(IID_IGameClient)) : 0;
}

void demoRestartDemo()
{
    queryGameClient()->restartDemo();
}

// Content passes 1 for the next player and 0 for the previous one. IGameClient's name for this
// slot, setDemoPaused, predates that evidence.
void demoTogglePlayerDemo(bool next)
{
    queryGameClient()->setDemoPaused(next);
}

void demoToggleCameraDemo()
{
    queryGameClient()->slot50();
}

void demoToggleDemoPauseOrPlay()
{
    queryGameClient()->togglePause();
}
} // namespace
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(restartDemo, demoRestartDemo, queryGameClient() != 0, kGameConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(togglePlayerDemo, demoTogglePlayerDemo, queryGameClient() != 0, bool, kGameConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(toggleCameraDemo, demoToggleCameraDemo, queryGameClient() != 0, kGameConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(toggleDemoPauseOrPlay, demoToggleDemoPauseOrPlay, queryGameClient() != 0,
                                   kGameConsoleTag)

#if !DEDICATED
// bf2Engine console defaults; playMovie is registered only in client images.
BF2_CONSOLE_OBJECT("bf2Engine", 2, 0)

// playMovie takes a filename and flag. Flag 1 prevents skipping; other flags
// allow it. The binding requires a live engine.
namespace {
void enginePlayMovie(const std::string& name, int flag)
{
    ::dice::hfe::bf2Engine->playMovie(name, flag);      //  tests nothing but the count; the
}                                                       //   null case is isObjectActive's, upstream
} // namespace
BF2_CONSOLE_METHOD_2_ACTIVE(playMovie, enginePlayMovie, ::dice::hfe::bf2Engine != 0, std::string, int, kGameConsoleTag)
#endif

// lockFps belongs to the Vars console namespace.
BF2_CONSOLE_OBJECT("Vars", 2, 0)

// Sets a registered variable from script text. Discards Vars::set's return value,
// as required by the console method's void result metadata.
BF2_CONSOLE_METHOD_2(set, ::dice::hfe::Vars::set, std::string, std::string, kGameConsoleTag)

// Dedicated binding for the frame-rate limiter. Nonpositive writes are ignored.
// [UNRESOLVED] Original registration evidence places this under Game; the current
// Vars placement remains until its dedicated executors are reconciled.
#if DEDICATED
BF2_CONSOLE_PROPERTY(lockFps, int, ::dice::hfe::bf2Engine, setLockFPS, getLockFPS, kGameConsoleTag)
#endif

// Game console methods at access level 2. radioMessage and simulationRate are bound;
// unload remains unregistered.
BF2_CONSOLE_OBJECT("Game", 2, 0)

// game.radioMessage resolves IGame and forwards the local player with the selected channel/message.
// The local player may be null. Empty channel extraction retains the previous value
// in this binding, whereas the retail converter resets it to TCGlobal.
namespace {
IGame* queryGame()                                      //  [CHOSEN] name
{
    return ::dice::hfe::game
        ? static_cast<IGame*>(::dice::hfe::game->queryInterface(IID_IGame)) : 0;
}

void gameRadioMessage(TargetChannel channel, std::string message)  //  [CHOSEN] name
{
    if (queryGame())
        queryGame()->radioMessage(channel, message,
                                  world::playerManager->getLocalHumanPlayer(), 0, true, false);
}
} // namespace
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(radioMessage, gameRadioMessage, true, TargetChannel, std::string, kGameConsoleTag)

// Reads or writes the active game's simulation rate. Server writes have no effect;
// client writes apply during demo playback. Availability requires a valid game interface.
namespace {
void gameSetSimulationRate(int rate)                    //  / PE 434AE6 SET arm [CHOSEN] name
{
    queryGame()->setSimulationRate(rate);
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(simulationRate, int, queryGame() != 0, queryGame()->getSimulationRate(),
                        gameSetSimulationRate, kGameConsoleTag)

#if !DEDICATED
// Client Game properties at access level 1. [UNRESOLVED] Dedicated registration
// of these properties remains split pending executor verification; sayAll/sayTeam
// remain unbound.
BF2_CONSOLE_OBJECT("Game", 1, 0)

// game.lockFps - PE ctor 0x434E10: the ELF's vars.lockFps property (above), under the client's name.
BF2_CONSOLE_PROPERTY(lockFps, int, ::dice::hfe::bf2Engine, setLockFPS, getLockFPS, kGameConsoleTag)

// Controls the Alt+Enter gate, initially disabled. Requires a live engine.
BF2_CONSOLE_PROPERTY(allowToggleFullscreen, bool, ::dice::hfe::bf2Engine, setAllowToggleFullscreen,
                     getAllowToggleFullscreen, kGameConsoleTag)

// game.toggleFullscreen - PE ctor 0x435015: type 0, argc 0, "void", static. isObjectActive 0x435067
// `bf2Engine->bf2Render != 0` - bf2Engine itself NOT tested; executor 0x435076: argc must be 0,
// hasReturnValue cleared, then bf2Engine->bf2Render (+0x18, NOT the g_bf2Render global) vslot +0x20
// toggleFullscreen, its result discarded (0x435090).
namespace {
void gameToggleFullscreen()
{
    ::dice::hfe::bf2Engine->getBF2Render()->toggleFullscreen();
}
} // namespace
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(toggleFullscreen, gameToggleFullscreen,
                                   ::dice::hfe::bf2Engine->getBF2Render() != 0, kGameConsoleTag)
#endif

// Handed to the console registry by ModuleBF2::init, the way the original registers 0x8AD45A8.
ConsoleObjectList& gameConsoleObjects()
{
    return consoleBindingList(kGameConsoleTag);
}

} // namespace hfe
} // namespace dice
