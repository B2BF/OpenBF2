// [CHOSEN] Separate translation unit for engine, demo, variable and game console bindings.
// ModuleBF2 registers and unregisters this list. Binding availability and access levels
// differ between dedicated and client images.

#include "../../Core/ConsoleObjectMacro.h"
#include "../../Core/Ids.h"     // IID_IGame - game.radioMessage's interface query
#include "BF2Engine.h"
#include "Game.h"               // operator>>(istream&, TargetChannel&) - game.radioMessage's converter
#include "../IGameServer.h"     // IGameServer::sayAll/sayTeam - game.sayAll/sayTeam's receiver interface
#include "../Utility/Vars.h"    // vars.set's receiver - Vars::set(std::string, std::string)
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

// bf2Engine console defaults and the engine lifecycle commands, registered in both images.
BF2_CONSOLE_OBJECT("bf2Engine", 2, 0)

// hostServer selects server mode and a level (dedicated PE executor 0x412330). The optional second
// argument is the game mode; the one-argument form defaults it to "conquest", as shipped.
namespace {
void engineHostServer(const std::string& level, const std::string& gameMode = "conquest")
{
    ::dice::hfe::bf2Engine->hostServer(level, gameMode);
}
// startGame and shutdownGame take no console arguments (PE executors 0x412140 / 0x4121F0); the
// shipped arms call startGame(false, false) and shutdownGame(true).
void engineStartGame()
{
    ::dice::hfe::bf2Engine->startGame(false, false);
}
void engineShutdownGame()
{
    ::dice::hfe::bf2Engine->shutdownGame(true);
}
} // namespace
BF2_CONSOLE_METHOD_1_2_ACTIVE(hostServer, engineHostServer, ::dice::hfe::bf2Engine != 0,
                              std::string, std::string, kGameConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(startGame, engineStartGame, ::dice::hfe::bf2Engine != 0,
                            kGameConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(shutdownGame, engineShutdownGame, ::dice::hfe::bf2Engine != 0,
                            kGameConsoleTag)

#if !DEDICATED
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

// game.sayAll / game.sayTeam are file-static methods on the access-level-1 Game object (dedicated
// PE defaults 0x759920, ctors 0x410E60 / 0x411040, vtable off_785128 / off_7851B8). The shared
// gate 0x411170 requires IGameServer from the game interface; the executors (0x410F50 / 0x4111A0)
// re-resolve it and call the interface's sayAll / sayTeam, discarding the bool result.
BF2_CONSOLE_OBJECT("Game", 1, 0)
namespace {
IGameServer* queryGameServer()                          //  [CHOSEN] name
{
    return ::dice::hfe::game
        ? static_cast<IGameServer*>(::dice::hfe::game->queryInterface(IID_IGameServer)) : 0;
}

void gameSayAll(const std::string& text) { queryGameServer()->sayAll(text); }
void gameSayTeam(int team, const std::string& text) { queryGameServer()->sayTeam(team, text); }
} // namespace
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(sayAll, gameSayAll, queryGameServer() != 0,
                                   std::string, kGameConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(sayTeam, gameSayTeam, queryGameServer() != 0,
                                   int, std::string, kGameConsoleTag)

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
