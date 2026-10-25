// GameLogic console bindings used by level scripts. [CHOSEN] Separate translation
// unit; the object name is normalized to gameLogic by console registration.
// togglePause uses access level 1; the following setters use access level 2.
#include "../../Core/ConsoleObjectMacro.h"
#include "../../Core/SettingsRepostitory.h"   // settings->boolGet("GSIsServer") - the getters' gate
#include "../IGameLogic.h"
#include "GameLogic.h"            // the messages.* bindings reach the concrete class
#include "ServerGameLogic.h"      // ServerGameLogic::createObject - the gameLogic.createObject receiver
#include "VoiceMessages.h"
#include "../../Scene/Object/PlayerManager.h"   // world::playerManager - player.kit's receiver
#include "../Objects/IPlayer.h"                 // world::IPlayer - player.kit's active player
#include "../../Math/Vec3.h"      // Vec4 - the fogStartEndAndBase property's complete type
#include <string>

namespace dice {
namespace hfe {

namespace {

// Stands in for the original's file-scope list. ModuleGame::init registers it - see
// Core/ModuleRegistry.cpp.
const char* const kGameLogicConsoleTag = "hfe.gameLogic";

// KitType is an integer with a distinct console metadata spelling. The typedef
// retains integer conversion while exposing that script argument type name.
typedef int KitType;
typedef unsigned int U32;

// The receiver. ConsoleClass179::isObjectActive (0x853C760) is `gameLogic != 0` and nothing more -
// eleven bytes, no interface query, unlike the sv object's client/server fork.
void gameLogicSetKit(int team, KitType kit,
                     const std::string& kitTemplate,
                     const std::string& soldierTemplate)
{
    gameLogic->setKit(team, kit, kitTemplate, soldierTemplate);          // IGameLogic +0x140
}

// Stock server/Init.con files set all three flag templates through this command before control
// points are precached. PE executor 0x415780 takes exactly (int, std::string) and dispatches
// GameLogic +0x200; its availability test is the same live gameLogic-pointer check as setKit.
void gameLogicSetTeamFlag(int team, const std::string& flagTemplate)
{
    gameLogic->setTeamFlag(team, flagTemplate);
}

void gameLogicSetTeamName(int team, const std::string& name)
{
    gameLogic->setTeamName(team, name);
}

// PE ctor 0x414E20, executor 0x414F50, shared gate 0x416B40 (`dword_83BF50 != 0`, i.e. gameLogic).
// argc exactly 2; dispatches IGameLogic +0x1F8 GameLogic::setTeamLanguage(int, const std::string&).
void gameLogicSetTeamLanguage(int team, const std::string& language)
{
    gameLogic->setTeamLanguage(team, language);
}

// Pre-spawn free-camera transform. Linux64 server ELF 0x87D9A0 (ConsoleClass199) and win32 client
// 0x43B055 both read two Vec3 arguments and call GameLogic::setBeforeSpawnCamera directly.
void gameLogicSetBeforeSpawnCamera(Vec3 position, Vec3 direction)
{
    static_cast<GameLogic*>(gameLogic)->setBeforeSpawnCamera(position, direction);
}

// ConsoleClass195 (PE ctor 0x419620, executor 0x419710, gate 0x418D60). The executor re-derives the
// server gameLogic behind the GSIsServer gate and calls the concrete class's createObject. The same
// single body is present in the client PE (ctor 0x43AA9A, executor 0x43AB0B, target 0x4DCB40): the
// receiver is always ServerGameLogic because the gate requires GSIsServer, and when GSIsServer is
// set BF2Engine::startGame calls setGameLogic(CID_ServerGameLogic) (the client's non-server path
// uses CID_ClientGameLogic = 121006). One std::string, no return value.
void gameLogicCreateObject(std::string name)
{
    static_cast<ServerGameLogic*>(gameLogic)->createObject(name);
}

// The shared `GSIsServer && gameLogic` gate: returns the published gameLogic only while the
// process is a server, else null. The two camera getters, the createObject method and its
// property bindings all select their receiver through it (PE executors 0x88F7C0/0x88F620 and
// gate 0x418D60).
GameLogic* serverGameLogic()
{
    if (!settings->boolGet(std::string("GSIsServer"), false))
        return 0;
    return static_cast<GameLogic*>(gameLogic);
}

Vec3 gameLogicGetBeforeSpawnCameraPosition()
{
    return serverGameLogic()->getBeforeSpawnCameraPosition();
}

Vec3 gameLogicGetBeforeSpawnCameraDirection()
{
    return serverGameLogic()->getBeforeSpawnCameraDirection();
}

// Stock GameLogicInit.con and level Init.con ticket policy. PE constructors/executors:
// default tickets Ex 0x41A5E0/0x41A820 (U32,U32,U32 -> +0x9C), end loss
// /0x41B290 (float -> +0xAC), ordinary loss 0x41B380/0x41B550
// (U32,float -> +0xB4). Each is access bit 2, static, and active iff gameLogic is live.
void gameLogicSetDefaultNumberOfTickets(U32 team, U32 tickets)
{
    gameLogic->setDefaultNumberOfTickets(team, tickets);
}
void gameLogicSetDefaultNumberOfTicketsEx(U32 maxPlayers, U32 team, U32 tickets)
{
    gameLogic->setDefaultNumberOfTicketsEx(maxPlayers, team, tickets);
}

U32 gameLogicGetDefaultNumberOfTicketsEx(U32 maxPlayers, U32 team)
{
    return gameLogic->getDefaultNumberOfTicketsEx(maxPlayers, team);
}
void gameLogicSetTicketLossPerMin(U32 team, float rate)
{
    gameLogic->setTicketLossPerMin(team, rate);
}
void gameLogicSetTicketLossAtEndPerMin(float rate)
{
    gameLogic->setTicketLossAtEndPerMin(rate);
}

// Commander-support console bindings require a live gameLogic instance.
void gameLogicAddInvalidDropVehicleObject(const std::string& name, bool invalid)
{
    gameLogic->AddInvalidDropVehicleObject(name, invalid);
}

void gameLogicSetDropVehicleDeviationRadius(int minRadius, int radius)
{
    gameLogic->setDropVehicleDeviationRadius(minRadius, radius);
}

void gameLogicSetNumDropVehiclesForTeam(int team, int count)
{
    gameLogic->setNumDropVehiclesForTeam(team, count);
}

std::string gameLogicGetTeamDropVehicle(int team)
{
    return gameLogic->getTeamDropVehicle(team);
}

void gameLogicSetTeamDropVehicle(int team, const std::string& name)
{
    gameLogic->setTeamDropVehicle(team, name);
}

// The no-vehicles omit list. Takes the name BY value because the virtual does - the executor
// (PE 0x416F70) copy-constructs a std::string from its argument slot and pushes all seven dwords
// of it onto the stack before the vtable call, which is the by-value ABI for this 28-byte type.
void gameLogicNoVehiclesOmit(std::string name)
{
    gameLogic->noVehiclesOmit(name);
}

} // namespace

BF2_CONSOLE_OBJECT("GameLogic", 2, 0)

// ConsoleClass179 - executor 0x853D040, isObjectActive 0x853C760, argc exactly 4, returns void.
BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(setKit, gameLogicSetKit, gameLogic != 0,
                                   int, KitType, std::string, std::string, kGameLogicConsoleTag)

BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setTeamFlag, gameLogicSetTeamFlag, gameLogic != 0,
                                   int, std::string, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setTeamName, gameLogicSetTeamName, gameLogic != 0,
                                   int, std::string, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setTeamLanguage, gameLogicSetTeamLanguage, gameLogic != 0,
                                   int, std::string, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setBeforeSpawnCamera, gameLogicSetBeforeSpawnCamera,
                                   gameLogic != 0, Vec3, Vec3, kGameLogicConsoleTag)
// ConsoleClass195 - exec 0x419710, gate 0x418D60 (GSIsServer && gameLogic), argc exactly 1,
// returns void. Only caller of ServerGameLogic::createObject.
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(createObject, gameLogicCreateObject,
                                   serverGameLogic() != 0, std::string, kGameLogicConsoleTag)
// ConsoleClass201/202 - exec 0x88F7C0 / 0x88F620, 0 args, dynamic, return Vec3; active only while
// GSIsServer is set. The setter above uses the plain gameLogic check instead.
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getBeforeSpawnCameraPosition,
                                gameLogicGetBeforeSpawnCameraPosition, Vec3,
                                serverGameLogic() != 0, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getBeforeSpawnCameraDirection,
                                gameLogicGetBeforeSpawnCameraDirection, Vec3,
                                serverGameLogic() != 0, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setDefaultNumberOfTickets,
                                   gameLogicSetDefaultNumberOfTickets, gameLogic != 0,
                                   U32, U32, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setDefaultNumberOfTicketsEx,
                                   gameLogicSetDefaultNumberOfTicketsEx, gameLogic != 0,
                                   U32, U32, U32, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(getDefaultNumberOfTicketsEx,
                                gameLogicGetDefaultNumberOfTicketsEx, U32,
                                gameLogic != 0, U32, U32, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setTicketLossPerMin, gameLogicSetTicketLossPerMin,
                                   gameLogic != 0, U32, float, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setTicketLossAtEndPerMin,
                                   gameLogicSetTicketLossAtEndPerMin, gameLogic != 0,
                                   float, kGameLogicConsoleTag)

// Commander vehicle-drop commands. The mixed dynamic flags are shipped behavior: the getter
// stores a return value and is dynamic, while the four mutation-only methods are not.
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(AddInvalidDropVehicleObject,
                                   gameLogicAddInvalidDropVehicleObject, gameLogic != 0,
                                   std::string, bool, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setDropVehicleDeviationRadius,
                                   gameLogicSetDropVehicleDeviationRadius, gameLogic != 0,
                                   int, int, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setNumDropVehiclesForTeam,
                                   gameLogicSetNumDropVehiclesForTeam, gameLogic != 0,
                                   int, int, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(getTeamDropVehicle, gameLogicGetTeamDropVehicle, std::string,
                                gameLogic != 0, int, kGameLogicConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setTeamDropVehicle, gameLogicSetTeamDropVehicle, gameLogic != 0,
                                   int, std::string, kGameLogicConsoleTag)

// PE executors 0x414C00/0x415D40/0x415EF0/0x4160A0: argc 1 calls the virtual setter, argc 0
// calls the virtual getter and publishes the float return value.
BF2_CONSOLE_PROPERTY(extraUAVVehicleFlightHeight, float, gameLogic,
                     setExtraUAVVehicleFlightHeight, getExtraUAVVehicleFlightHeight,
                     kGameLogicConsoleTag)
BF2_CONSOLE_PROPERTY(vehicleDropHeight, float, gameLogic,
                     setVehicleDropHeight, getVehicleDropHeight, kGameLogicConsoleTag)
BF2_CONSOLE_PROPERTY(supplyDropNumSecsToLive, float, gameLogic,
                     setSupplyDropNumSecsToLive, getSupplyDropNumSecsToLive,
                     kGameLogicConsoleTag)
BF2_CONSOLE_PROPERTY(supplyDropHeight, float, gameLogic,
                     setSupplyDropHeight, getSupplyDropHeight,
                     kGameLogicConsoleTag)

// ConsoleClass142
BF2_CONSOLE_PROPERTY(maximumLevelViewDistance, float, gameLogic,
                     setMaximumLevelViewDistance, getMaximumLevelViewDistance,
                     kGameLogicConsoleTag)
// ELF ConsoleClass156 0x8534C40 / PE executor 0x4383FF. The write arm calls the nonvirtual
// GameLogic::setEnableVoHelp; the read arm dispatches IGameLogic +0x22C.
BF2_CONSOLE_PROPERTY(enableVoHelp, bool, static_cast<GameLogic*>(gameLogic),
                     setEnableVoHelp, getEnableVoHelp, kGameLogicConsoleTag)

// Adds a vehicle template to the no-vehicles omission list. Matching spawners use
// the dummy template when the server setting or game mode disables vehicles.
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(noVehiclesOmit, gameLogicNoVehiclesOmit, gameLogic != 0,
                                   std::string, kGameLogicConsoleTag)

// Voice-message commands belong to gamelogic with a messages. method prefix.
// They are active only after the voice registry exists. addRadioVoice accepts three
// to five arguments, defaulting the local sample to empty and flags to zero.
namespace {

VoiceMessages* getVoiceMessages()
{
    return gameLogic ? static_cast<GameLogic*>(gameLogic)->getVoiceMessages() : 0;
}

void messagesSetVolumes(float voVolume, float radioVolume)            // 's target
{
    getVoiceMessages()->setVolumes(voVolume, radioVolume);
}
int messagesAddMessage(const std::string& name)                      // 's target
{
    return getVoiceMessages()->addMessage(name);
}
int messagesAddRadioVoice(const std::string& voice, const std::string& radioSample,
                          const std::string& text, const std::string& localVOSample,
                          int voFlags)                               // 's target
{
    return getVoiceMessages()->addRadioVoice(voice, radioSample, text, localVOSample, voFlags);
}
int messagesAddRadioFilter(const std::string& name, const std::string& sample)
{
    return getVoiceMessages()->addRadioFilter(name, sample);
}
int messagesAddHelpMessage(const std::string& name, const std::string& sample)
{
    return getVoiceMessages()->addHelpMessage(name, sample);
}

} // namespace

// ConsoleClass171 - exec 0x8538DE0, isObjectActive 0x8538920, argc exactly 2, returns void.
// Unlike the four return-valued adders below, its PE/ELF constructor stores m_isDynamic=false.
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(messages.setVolumes, messagesSetVolumes, getVoiceMessages() != 0,
                                   float, float, kGameLogicConsoleTag)
// ConsoleClass172 - exec 0x8539390, isObjectActive 0x8539060, argc exactly 1, returns int.
BF2_CONSOLE_METHOD_1_RET_ACTIVE(messages.addMessage, messagesAddMessage, int,
                                getVoiceMessages() != 0, std::string, kGameLogicConsoleTag)
// ConsoleClass173 - exec 0x853A170, isObjectActive 0x8539650, argc 3..5, returns int.
BF2_CONSOLE_METHOD_3_5_RET_ACTIVE(messages.addRadioVoice, messagesAddRadioVoice, int,
                                  getVoiceMessages() != 0,
                                  std::string, std::string, std::string, std::string, int,
                                  std::string(), 0, kGameLogicConsoleTag)
// ConsoleClass174 - exec 0x853ABC0, isObjectActive 0x853A680, argc exactly 2, returns int.
BF2_CONSOLE_METHOD_2_RET_ACTIVE(messages.addRadioFilter, messagesAddRadioFilter, int,
                                getVoiceMessages() != 0, std::string, std::string,
                                kGameLogicConsoleTag)
// ConsoleClass175 - exec 0x853B400, isObjectActive 0x853AEC0, argc exactly 2, returns int.
BF2_CONSOLE_METHOD_2_RET_ACTIVE(messages.addHelpMessage, messagesAddHelpMessage, int,
                                getVoiceMessages() != 0, std::string, std::string,
                                kGameLogicConsoleTag)

std::list<ConsoleObject*>& gameLogicConsoleObjects()
{
    return consoleBindingList(kGameLogicConsoleTag);
}

// Renderer fog property backed by game logic. Level scripts use this Vec4 setter
// to update both settings and the renderer's fog range.
BF2_CONSOLE_OBJECT("Renderer", 2, 0)

BF2_CONSOLE_PROPERTY(fogStartEndAndBase, Vec4, gameLogic,
                     setFogStartEndAndBase, getFogStartEndAndBase, kGameLogicConsoleTag)

// player.kit - reads or sets the active player's kit index. Dedicated PE defaults 0x75A5C0; ctor
// 0x41C520 (vtable off_7886E8, +0xA1 = 1); the shared gate 0x41C040 is `playerManager &&
// getActivePlayer() != 0`; executor 0x41C670 dispatches IPlayer::getKit / setKit on the active
// player. Retail appends this binding to the same gameLogic list (0x83C4EC).
BF2_CONSOLE_OBJECT("Player", 2, 0)
namespace {
world::IPlayer* activeGamePlayer()
{
    return world::playerManager ? world::playerManager->getActivePlayer() : 0;
}
void gameLogicSetActivePlayerKit(int kit)
{
    activeGamePlayer()->setKit(kit);
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(kit, int, activeGamePlayer() != 0, activeGamePlayer()->getKit(),
                        gameLogicSetActivePlayerKit, kGameLogicConsoleTag)

} // namespace hfe
} // namespace dice
