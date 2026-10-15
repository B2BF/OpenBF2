// Owns the HUD modules and their shared menu system.
#include "BF2HudManager.h"
#if !DEDICATED
#include "HudBlinker.h"
#include "Ref2System.h"
#include "HudInformationAdapter.h"
#include "HudChat.h"
#include "HudKitSelection.h"
#include "HudCommander.h"
#include "HudSquad.h"
#include "HudAmmo.h"
#include "HudVehicleInformation.h"
#include "Bf2MenuMapNode.h"
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/Pointer.h"
#include "../IO/Input/Input.h"
#include "../Core/StringUtil.h"
#include "../Core/System.h"
#include "../Core/Ids.h"
#include "../Core/EventManager.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/ControlSettings.h"
#include "../Game/LocalProfileManager.h"
#include "../Game/IGame.h"
#include "../Game/IGameLogic.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ISoldier.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Game/Objects/GameLogicComp/IArmorGLComp.h"
#include "../Game/Objects/Game/Kit.h"
#include "../Game/Objects/IVehicleHudComp.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Game/GameLogic/SquadManager/Squad.h"
#include "../Game/GameLogic/ScoreSystem/ScoreManager.h"
#include "../Sound/BfSound.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/PlayerScoreData.h"
#include "../IO/Console/Console.h"
#include "../IO/Localization/ExcelLexicon.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <new>
#if !defined(_M_IX86)
#include <emmintrin.h>
#endif

namespace dice { namespace hfe { extern hud::BF2HudManager* hudManager; } }
namespace dice { namespace hfe { class IMenuLogic; extern IMenuLogic* menuLogic; } }
namespace dice { namespace hfe { extern IBase* squadManager; } }
namespace dice { namespace hfe { namespace world { extern IBase* scoreManager; } } }
namespace dice { namespace hfe { namespace io { extern IBase* controlMapManager; } } }
namespace dice { namespace hfe { namespace hud {
namespace {
// PE869FE0 constructs this TU's 27 strings at A212A8; destructor87C390.
// HudSquad has its own identically initialized array at A1E380.
const std::string defaultSquadNames[27] = {
    "Unassigned", "Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot",
    "Gamma", "Hotel", "India", "Juliet", "Kilo", "Lima", "Mike", "November",
    "Oscar", "Papa", "Quebec", "Romeo", "Sierra", "Tango", "Uniform", "Victor",
    "Whiskey", "Xray", "Yankee", "Zulu"
};

// PE7B1CAA..1CC1 compares the unrounded x87 expression, including unordered
// values. Only the rejected arm then materializes the double passed to ceil.
bool commanderWaitRemaining(double last, double cooldown, double now, double& remaining)
{
    const double minutesPerSecond = 1.0 / 60.0;
    double value;
    bool denied;
#if defined(_M_IX86)
    __asm {
        fld now
        fsub last
        fmul minutesPerSecond
        fsubr cooldown
        ftst
        fnstsw ax
        test ah, 41h
        setp denied
        jnp no_wait
        fstp value
        jmp wait_done
    no_wait:
        fstp st(0)
    wait_done:
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // setp after test ah,41h: denied for a positive or unordered wait, not for <= 0.
    value = cooldown - (now - last) * minutesPerSecond;
    denied = !(value <= 0.0);
#endif
    if (denied) remaining = value;
    return denied;
}
int commanderWaitInteger(double remaining)
{
    const double rounded = std::ceil(remaining);
#if defined(_M_IX86)
    unsigned short original, truncating;
    int result;
    __asm {
        fld rounded
        fnstcw original
        mov ax, original
        or ax, 0C00h
        mov truncating, ax
        fldcw truncating
        fistp result
        fldcw original
    }
    return result;
#else
    // cvttsd2si truncates like the chopped fistp, including the 0x80000000 indefinite result.
    return _mm_cvttsd_si32(_mm_set_sd(rounded));
#endif
}
}

void BF2HudManager::cacheMouseInput(bool button)
{
    io::IControlMap* map = static_cast<io::IControlMapManager*>(io::controlMapManager)->getActive();
    if (map) {
        io::PlayerInputMap mapped = static_cast<io::PlayerInputMap>(button);
        if (map->getInputMappedToMouseButton(button, mapped)) m_field328 = mapped;
    }
}

void BF2HudManager::commanderApply() // 7B1C50; complete PE instructions read
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player || player->getIsCommander()) return;
    const double last = player->getLastTimeCommander();
    const double wait = player->getCommanderReapplyWaitTime();
    double remaining;
    if (commanderWaitRemaining(last, wait, g_system->getExactTime(), remaining)) {
        const int minutes = commanderWaitInteger(remaining);
        const char* key = minutes <= 1 ? "HUD_HUD_COMMANDER_CANNOT_APPLY_FOR_MINUTE"
                                      : "HUD_HUD_COMMANDER_CANNOT_APPLY_FOR_MINUTES";
        const char* token = minutes <= 1 ? "#MINUTE#" : "#MINUTES#";
        addServerChat(replaceWideTokenFromInt(getLocalizedString(key), token, minutes), 0);
    } else {
        hudManager->enableVOHelpMessage(22);
        hudManager->disableVOHelpMessage(23);
        const int id = player->getId();
        const int team = player->getTeam();
        static_cast<ISquadManager*>(squadManager)->applyForCommander(team, id);
        g_system->getExactTime();
        player->getLastTimeCommander(); // Retail discards both reads; no timestamp write.
        const std::string teamName = getLowerCase(gameLogic->getTeamName(player->getTeam()));
        const std::wstring localizedTeam = getLocalizedString(formatString("HUD_TEAMNAME_%s", teamName.c_str()));
        addServerChat(replaceWideToken(getLocalizedString("HUD_HUD_COMMANDER_APPLIED"),
                                      "#TEAMNAME#", std::wstring(localizedTeam.c_str())), 0);
    }
    setMode(0, true);
    cacheMouseInput(true);
    hudManager->enableVOHelpMessage(22);
}

void BF2HudManager::toggleUAVState()
{
    if (!isCommanderShown()) return;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    if (getMode() == 23) {
        setMode(15, true);
    } else if (getUAVEnabled() && getUAVAvailability() > 0) {
        if (hudMap && player->getOverheadCameraEnabled() && !hudMap->m_uavReloading) {
            setMode(23, true);
        } else {
            const std::string message("uav_in_use");
            game->localGameInfoMessage(static_cast<TargetChannel>(14), message);
        }
    } else {
        setMode(15, true);
        const std::string message("uav_unavailable");
        game->localGameInfoMessage(static_cast<TargetChannel>(14), message);
    }
}

void BF2HudManager::paint() // 74F110, complete21-instruction body
{
    if (!menuLogic && game && gameLogic) {
        world::playerManager->getLocalHumanPlayer(); // Discarded before testing the paint flag.
        if (m_field248 && ref2System)
            ref2System->paint(); // Retail ignores the UI system's bool result.
    }
}
bool BF2HudManager::isSayAllActive() { return m_field0b0; }
bool BF2HudManager::isSayTeamActive() { return m_field0b1; }
bool BF2HudManager::isSaySquadActive() { return m_field0b2; }

void BF2HudManager::sendKitMessage()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    const int kit = player->getKit();
    const std::wstring message = replaceWideTokenFromInt(
        getLocalizedString("HUD_HUD_CHAT_KIT_MESSAGE"), "KITNUMBER", kit);
    const std::string narrow = makeNarrow(message);
    game->radioMessage(TCTeam, narrow, player, 0, true, false);
}

void BF2HudManager::refreshChatCategory(int category)
{
    if (!hudChat) return;
    for (int i = 0; i < hudChat->getCapacity(category); ++i) {
        HudChat* chat = hudChat;
        int* count = chat->getCount(category);
        const int firstRow = chat->getFirstRow(category);
        hudChat->dropRow(firstRow, count);
    }
}

void BF2HudManager::sendRequestForOrder()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    {
        const std::string message("req_new_order");
        game->radioMessage(TCCommander, message, player, 0, true, false);
    }
    const int squad = player->getSquadId();
    const int team = player->getTeam();
    sendRequestEvent(team, squad, static_cast<RequestEvent::RequestTypes>(1));
}

void BF2HudManager::sendRequestEvent(int team, int squad, RequestEvent::RequestTypes type)
{
    const bool server = gameLogic->isServer();
    void* storage = ::operator new(sizeof(RequestEvent));
    RequestEvent* event = storage ? new(storage) RequestEvent(team, squad, type) : nullptr;
    if (server) {
        event->executeServer(-1);
        event->release();
    } else {
        game->sendGameEvent(static_cast<unsigned int>(-1), event);
    }
}

void BF2HudManager::submitSquadRename()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player && !m_field0c0.empty()) {
        const int id = player->getId();
        const int squad = player->getSquadId();
        const int team = player->getTeam();
        static_cast<ISquadManager*>(squadManager)->setSquadName(team, squad, id, m_field0c0, false);
    }
}

void BF2HudManager::leaveSquad()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player || !player->getSquadId()) return;
    const int squad = player->getSquadId();
    const int team = player->getTeam();
    const int id = player->getId();
    static_cast<ISquadManager*>(squadManager)->removeFromSquad(team, squad, id, false);
    const int newId = player->getId();
    const int newTeam = player->getTeam();
    static_cast<ISquadManager*>(squadManager)->inviteToSquad(newTeam, squad, newId, false, false);
}

int BF2HudManager::findEmptySquad(int team)
{
    for (int squad = 1; squad < 10; ++squad)
        if (world::playerManager->getPlayersInSquad(team, squad).empty()) return squad;
    return -1;
}

void BF2HudManager::createSquad(std::string name)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player || player->getIsCommander()) return;
    int squad = findEmptySquad(player->getTeam());
    if (player->getSquadId() != 0) {
        const int currentSquad = player->getSquadId();
        const int team = player->getTeam();
        if (world::playerManager->getNrOfPlayersInSquad(team, currentSquad) == 1)
            squad = player->getSquadId();
        m_field138->leaveAndReinvite();
    }
    if (squad > -1 && world::playerManager->getPlayersInSquad(player->getTeam(), squad).empty()) {
        if (name.empty()) name = defaultSquadNames[findEmptySquad(player->getTeam())].c_str();
        const int id = player->getId();
        const int team = player->getTeam();
        static_cast<ISquadManager*>(squadManager)->setSquadName(team, squad, id, name, false);
    }
}

void BF2HudManager::setDone(bool done)
{
    m_field24a = done;
    if (done) {
        setMode(0, true);
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        if (player && m_field228 && gameLogic) {
            gameLogic->selectSpawnGroup(player, m_field11c->m_selectedSpawnGroup[m_field11c->m_selectedTeam]);
            m_field2a8 = m_field2ac;
            hudManager->disableVOHelpMessage(60);
        }
        m_field138->setCreateMode(false);
    }
}

void BF2HudManager::toggleArtilleryState()
{
    if (!isCommanderShown() || !world::playerManager->getLocalHumanPlayer()) return;
    if (getMode() == 22) setMode(15, true);
    else if (!getArtilleryEnabled() || getArtilleryAvailability() == 0.0f) {
        setMode(15, true);
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "artillery_unavailable");
    } else if (!m_field13c || m_field13c->m_artilleryReloading)
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "artillery_in_use");
    else setMode(22, true);
}
void BF2HudManager::toggleSupplyState()
{
    if (!isCommanderShown()) return;
    if (getMode() == 24) setMode(15, true);
    else if (!m_field13c || m_field13c->m_supplyReloading) {
        setMode(15, true);
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "supplies_in_use");
    } else setMode(24, true);
}
void BF2HudManager::toggleVehicleDropState()
{
    if (!isCommanderShown()) return;
    if (getMode() == 25) setMode(15, true);
    else if (!m_field13c || m_field13c->m_vehicleDropReloading) {
        setMode(15, true);
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "vehicledrop_in_use");
    } else setMode(25, true);
}

void BF2HudManager::onManDown()
{
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->setDeathMessage(
        replaceWideInputKeys(getLocalizedString("HUD_TEXT_MENU_YOU_ARE_IN_MAN_DOWN")));
    m_field24c = true;
    hudAmmo->hide(false);
    hudVehicleInformation->reset(false);
    if (isCommanderShown()) setMode(15, true);
    if (getMode() != 12) setMode(0, true);
}
void BF2HudManager::setSquadOrder(const SquadOrder& order)
{
    m_order.orderId = order.orderId;
    m_order.targetNetId = order.targetNetId;
    m_order.points[0] = order.points[0];
    m_order.points[1] = order.points[1];
    m_order.points[2] = order.points[2];
    m_order.flag2C = order.flag2C;
    // The retail ushort/byte stores leave the two padding regions untouched.
}
void BF2HudManager::toggleSquadCommander()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    if (player->getIsSquadLeader() && getMode() == 18) {
        setMode(17, false);
        return;
    }
    if (player->getIsCommander() && player->getIsAlive() && !isPlayerAlive(player)) {
        setMode(15, false);
        return;
    }
    if (getMode() != 17) {
        if (!isCommanderShown()) return;
        if (getMode() != 15) setMode(15, true);
    }
    setMode(18, true);
}

void BF2HudManager::onSpawn() // 751560, complete76 instructions
{
    const float previous = m_field2ac;
    HudKitSelection* selection = m_field11c;
    m_field24c = true;
    m_field228 = true;
    m_field2a8 = previous;
    selection->selectKit(selection->m_selectedKit[selection->m_selectedTeam]);
    if (isCommanderShown()) setMode(15, true);
    if (getMode() == 1 && gameLogic) {
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        gameLogic->selectSpawnGroup(player, 0);
    } else if (getMode() != 9) {
        setMode(0, true);
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        m_field24a = player && (!player->getSpawnAtTick() || player->getTimeToSpawn() > 2.0f);
    }
}

void BF2HudManager::playRoundResultSound() // 751650, complete69 instructions
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    const int winner = static_cast<unsigned char>(
        static_cast<world::IScoreManager*>(world::scoreManager)->getWinner());
    const char* result = winner > 0 && player->getTeam() == winner ? "win" : "loose";
    const int team = player->getTeam(); // Repeated even when winner was nonzero.
    {
        const std::string formatted = formatString("%s_%s", gameLogic->getTeamName(team).c_str(), result);
        const std::string lower = getLowerCase(formatted);
        m_field304 = lower;
    }
    g_bfSound->playSound(m_field304);
}

void BF2HudManager::showRoundResult() // 751720, complete146 instructions
{
    if (m_field320 || !world::playerManager->getLocalHumanPlayer()) return;
    const int winner = static_cast<unsigned char>(
        static_cast<world::IScoreManager*>(world::scoreManager)->getWinner());
    if (winner == 0) {
        showConnectionMessage(getLocalizedString("HUD_HUD_DRAW_STRING"));
    } else if (winner > 0) {
        const int winnerTickets = static_cast<world::IScoreManager*>(world::scoreManager)
            ->getTeamScore(winner)->m_tickets;
        const int loserTickets = static_cast<world::IScoreManager*>(world::scoreManager)
            ->getTeamScore(winner == 1 ? 2 : 1)->m_tickets;
        std::wstring message;
        {
            const std::string team = getLowerCase(gameLogic->getTeamName(winner));
            const std::wstring localizedTeam = getLocalizedString(formatString("HUD_TEAMNAME_%s", team.c_str()));
            const std::wstring replacement(localizedTeam.c_str());
            message = replaceWideToken(replaceWideInputKeys(getLocalizedString("HUD_HUD_WIN_STRING")),
                "#TEAMNAME#", replacement);
        }
        showConnectionMessage(formatString(L"%s (%d - %d)", message.c_str(), winnerTickets, loserTickets));
    }
}

void BF2HudManager::callForMedic()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    game->callForSpecialist(player, static_cast<HudNodeTypes>(46));
}
void BF2HudManager::callForRevive()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    game->callForSpecialist(player, static_cast<HudNodeTypes>(49));
}
void BF2HudManager::callForAmmo()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    game->callForSpecialist(player, static_cast<HudNodeTypes>(47));
}
void BF2HudManager::callForRepair()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    game->callForSpecialist(player, static_cast<HudNodeTypes>(48));
}

bool BF2HudManager::isPlayerAlive(world::IPlayer* player)
{
    if (!player) return false;
    world::IObject* object = player->getDefaultVehicle();
    if (!object || !object->m_armorComponent) return false;
    return static_cast<world::IArmorGLComp*>(object->m_armorComponent)->getStatus() == 1;
}

std::intptr_t BF2HudManager::getPlayerKitIcon(world::IPlayer* player, bool mapIcon, bool spotted)
{
    if (!player) return 0;
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (!mapIcon) {
        if (player->getIsAlive() && !isPlayerAlive(player)) {
            if (local) {
                const int team = local->getTeam();
                if (player->getTeam() != team) {
                    if (!m_field184 && ref2System)
                        m_field184 = ref2System->loadTexture("Ingame/Scoreboard/Icons/icon_Faded.tga", true);
                    return static_cast<std::intptr_t>(m_field184);
                }
            }
        } else {
            if (!m_field188 && ref2System)
                m_field188 = ref2System->loadTexture("Ingame/Scoreboard/Icons/icon_Death.tga", true);
            return static_cast<std::intptr_t>(m_field188);
        }
    }
    world::IObject* object = player->getDefaultVehicle();
    if (!object) return 0;
    world::ISoldier* soldier = static_cast<world::ISoldier*>(object->queryInterface(world::IID_ISoldier));
    if (!soldier) return 0;
    world::Kit* kit = soldier->getKit();
    if (!kit || !kit->m_vehicleHudComponent) return 0;
    world::IVehicleHudComp* hud = static_cast<world::IVehicleHudComp*>(kit->m_vehicleHudComponent);
    if (!mapIcon) return hud->getVehicleIconPtr(0);
    if (spotted && hud->getHudIcons()->spottedIconPtr)
        return hud->getHudIcons()->spottedIconPtr; // Deliberate second virtual query.
    return hud->getHudIcons()->miniMapIconPtr;
}

bool BF2HudManager::isWithinAimThreshold(world::IObject* object, float threshold)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!object || !player) return false;
    world::ICameraObject* camera = player->getCamera();
    double dot = 0.0;
    if (camera) {
        const Mat4& basis = camera->getTransformation(0.0f);
        const Vec3 forward = { basis.m[8], basis.m[9], basis.m[10] };
        const Vec3 position = object->getAbsolutePosition();
        const Mat4& origin = camera->getTransformation(0.0f); // Reread after object callback.
        Vec3 direction = { position.x - origin.m[12], position.y - origin.m[13],
                           position.z - origin.m[14] };
        // Inlined Vec3 normalization from 4639B0; do not normalize an already
        // unit vector, and zero a degenerate one. Its status result is discarded.
        const float lengthSquared = static_cast<float>(
            static_cast<double>(direction.x) * direction.x +
            static_cast<double>(direction.y) * direction.y +
            static_cast<double>(direction.z) * direction.z);
        if (!(std::fabs(static_cast<double>(lengthSquared) - 1.0) < FLT_EPSILON)) {
            if (std::fabs(lengthSquared) < FLT_EPSILON) direction = Vec3::getZero();
            else {
                const double inverse = 1.0 / std::sqrt(static_cast<double>(lengthSquared));
                direction.x = static_cast<float>(direction.x * inverse);
                direction.y = static_cast<float>(direction.y * inverse);
                direction.z = static_cast<float>(direction.z * inverse);
            }
        }
        dot = static_cast<double>(direction.z) * forward.z +
              static_cast<double>(direction.y) * forward.y +
              static_cast<double>(direction.x) * forward.x;
    }
    return dot > threshold; // A missing camera still compares zero with threshold.
}

void BF2HudManager::queueConsoleCommand(const std::string& command)
{
    m_field2e0.push_back(command);
}
void BF2HudManager::executeQueuedConsoleCommands()
{
    if (!m_field2e0.empty()) {
        // Iterate the live list: a callback's append runs in this same pass.
        // Erasing the active iterator from a callback remains unsafe, as retail.
        for (auto it = m_field2e0.begin(); it != m_field2e0.end(); ++it)
            io::mainConsole->executeLine(*it, io::Console::ignoredString_,
                                         false, false, false, false, false);
        m_field2e0.clear();
    }
}
void BF2HudManager::addTextureAtlas(const std::string& name)
{
    bool found = false;
    for (const std::string& entry : m_field33c)
        if (entry == name) found = true; // Retail scans to the end even after a match.
    if (!found) m_field33c.push_back(name);
}
const std::list<std::string>& BF2HudManager::getAtlasList()
{
    return m_field33c;
}
void BF2HudManager::addFavouriteServer(bool favorite) // 751B50 tail call
{
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->setServerFavorite(favorite);
}

// all three replace the same primary input data, then
// dispatch their own no-argument send slot. The data callback result is ignored.
void BF2HudManager::sendSayAllText(std::string text)
{
    m_field08c = text;
    m_field0a8[0]->setString(ref2System, m_field08c.c_str());
    sendSayAll();
}
void BF2HudManager::sendSayTeamText(std::string text)
{
    m_field08c = text;
    m_field0a8[0]->setString(ref2System, m_field08c.c_str());
    sendSayTeam();
}
void BF2HudManager::sendSaySquadText(std::string text)
{
    m_field08c = text;
    m_field0a8[0]->setString(ref2System, m_field08c.c_str());
    sendSaySquad();
}

// 7AF4A0..7AF8DB: first dispatch the ordinary chat, then always query the
// local player, even with neither chat owner present. The second owner is the
// commander view. Preserve the asymmetric category forwarding and null player.
void BF2HudManager::addGlobalChat(std::wstring text, world::IPlayer* player, int team)
{
    if (hudChat) hudChat->addPlayerMessage(text, player, team);
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (m_field140 && local && local->getIsCommander())
        m_field140->addPlayerMessage(text, player, team);
}
void BF2HudManager::addTeamChat(std::wstring text, world::IPlayer* player, int team)
{
    if (hudChat) hudChat->addGameMessage(text, player, team);
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (m_field140 && local && local->getIsCommander())
        m_field140->addGameMessage(text, player, team);
}
void BF2HudManager::addSquadChat(std::wstring text, world::IPlayer* player, int team)
{
    if (hudChat) hudChat->addKillMessage(text, player, team);
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (m_field140 && local && local->getIsCommander())
        m_field140->addKillMessage(text, player, team);
}
void BF2HudManager::addRadioChat(std::wstring text, world::IPlayer* player, int team, int alternateCategory)
{
    if (hudChat) hudChat->addRankMessage(text, player, team, alternateCategory);
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (m_field140 && local && local->getIsCommander())
        m_field140->addRankMessage(text, player, team, 0);
}
void BF2HudManager::addCommanderChat(std::wstring text, world::IPlayer* player, int team)
{
    if (hudChat) hudChat->addPersonalMessage(text, player, team);
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (m_field140 && local && local->getIsCommander())
        m_field140->addGameMessage(text, player, team);
}
void BF2HudManager::addServerChat(std::wstring text, int team)
{
    if (hudChat) hudChat->addNotice(text, team);
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (m_field140 && local && local->getIsCommander())
        m_field140->addGameMessage(text, nullptr, team);
}
void BF2HudManager::addServerTeamChat(std::wstring text, int team)
{
    if (hudChat) hudChat->addRequest(text, team);
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (m_field140 && local && local->getIsCommander())
        m_field140->addGameMessage(text, nullptr, team);
}

bool BF2HudManager::getChatFlag089() { return m_field089; }

// 5AB2A0 is exactly RET 4. Caller756150 retains update's float dt in EDI;
// 756325..756328 pushes that same word. This empty hook is retail, not a stub.
void BF2HudManager::tickChatHook(float) {}

void BF2HudManager::enableVOHelpMessage(int id)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player && !player->getVOHelpMessagePlayed(id))
        player->enableVOHelpMessage(id);
}
void BF2HudManager::disableVOHelpMessage(int id)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player && !player->getVOHelpMessagePlayed(id))
        player->disableVOHelpMessage(id);
}

void BF2HudManager::showDisconnectMessage(std::wstring text)
{
    if (!m_field0bd) {
        m_field0bd = true;
        static_cast<HudInformationLayer*>(g_hudInformationLayer)->setDisconnectMessage(text);
    }
}
void BF2HudManager::clearDisconnectMessage()
{
    if (m_field0bd) {
        m_field0bd = false;
        static_cast<HudInformationLayer*>(g_hudInformationLayer)->setDisconnectMessage(L"");
    }
}
void BF2HudManager::clearDeathMessage()
{
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->setDeathMessage(L"");
}
void BF2HudManager::showConnectionMessage(std::wstring text)
{
    if (!m_field0be) {
        m_field0be = true;
        hudVehicleInformation->m_killedState = false;
    }
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->setDisconnectMessage(text);
}
void BF2HudManager::clearConnectionMessage()
{
    if (m_field0be) {
        m_field0be = false;
        static_cast<HudInformationLayer*>(g_hudInformationLayer)->setDisconnectMessage(L"");
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        if (player) hudVehicleInformation->m_killedState = !player->getIsAlive() || player->getIsManDown();
    }
}
meme::BfOccupiedVehicleData* BF2HudManager::getOccupiedVehicleData(int selector)
{ return selector == 1 ? hudVehicleInformation->m_occupiedData : nullptr; }
void BF2HudManager::setMenuLoading(bool loading) { m_field0bf = loading; }
bool BF2HudManager::isMenuLoading() { return m_field0bf; }
void BF2HudManager::setSpottedObjectId(unsigned short id) { m_field1e4 = id; }
void BF2HudManager::setSpottedPosition(Vec3 position) { m_field1e8 = position; }
void BF2HudManager::setAimThreshold(float threshold) { m_field1f4 = threshold; }
bool BF2HudManager::isPointerEnabled() { return m_field15c; }
void BF2HudManager::disablePointer()
{
    m_field15c = false;
    if (ref2System)
        static_cast<meme::Pointer*>(ref2System->getPointerObject())->setEnabled(false);
    // Retail guards the system only; its pointer object is unconditionally used.
}

BF2HudManager::BF2HudManager()
    : m_field010(), m_field038(), m_field060(), m_field084(150), m_field088(false),
      m_field08c(), m_field0b0(false), m_field0b1(false), m_field0b2(false),
      m_field0bc(false), m_field0bd(false), m_field0be(false), m_field0bf(false),
      m_field0c0(), m_field0e4(false), m_field0e5(false), m_field0e6(false), m_field0e7(false),
      m_field0e8(), ref2System(0), hudMap(0), m_field0fc(0), m_field100(0),
      m_field104(0), m_field108(0), m_field10c(0), m_field110(0), m_field114(0),
      m_field118(0), m_field11c(0), m_field120(0), m_field124(0), m_field128(0),
      hudAmmo(0), hudVehicleInformation(0), m_field134(0), m_field138(0), m_field13c(0),
      m_field140(0), hudChat(0), m_field148(0), m_field14c(0), m_field150(10),
      m_field154(32.0f), m_field158(32.0f), m_field15c(false), m_field17c(20),
      m_field180(40), m_field184(0), m_field188(0), m_field1e4(0),
      m_field1e8{0.0f, 0.0f, 0.0f}, m_field1f4(0.995f), m_field1f8(0),
      m_field1fc(0), m_field200(250.0f), m_field204(250.0f), m_field208(-125.0f),
      m_field20c(-125.0f), m_field210(0), m_field214(0), m_field218(250.0f),
      m_field21c(250.0f), m_field220(-125.0f), m_field224(-125.0f), m_field228(true),
      m_field229(false), m_field22a(false), m_field22c(0), m_field230(false),
      m_field234(0), m_field238(0), m_field23c(0), m_field240(false), m_field241(false),
      m_field242(false), m_field244(-1), m_field248(true), m_field249(true),
      m_field24a(false), m_field24b(true), m_field24c(true), m_field258(0),
      m_field25c(0), m_field260(0), m_field264(-4000.0f), m_field268(-4000.0f),
      m_field26c(-4000.0f),
      // Preserve the existing copy of indeterminate bits before writing defaults.
      // memcpy avoids evaluating them as floating-point values.
      m_field2a4((m_order.points[0] = Vec3{0.0f, 0.0f, 0.0f},
                  m_order.points[1] = Vec3{0.0f, 0.0f, 0.0f},
                  m_order.points[2] = Vec3{0.0f, 0.0f, 0.0f},
                  std::memcpy(&m_field2a8, &m_field2ac, sizeof(float)),
                  std::memcpy(&m_field2a0, &m_field2a4, sizeof(float)), 5.0f)),
      m_field2ac(5.0f), m_field2b0(false), m_field2b4(3.0f), m_field2b8(3.0f),
      m_field2bc(80.0f), m_field2c0(false), m_field2c1(false), m_field2c2(false),
      m_field2c4(0), m_field2c8(0), m_field2cc(0), m_field2d0(1.0f), m_field2d4(100.0f),
      m_field2d8(100.0f), m_field2e0(), m_field2ec(-1), m_field2f0(0),
      m_field2f4(false), m_field2f5(false), m_field2f8(0), m_field2fc(false),
      m_field300(0), m_field304(""), m_field320(false), m_field324(1), m_field328(64),
      m_field32c(), m_field33c(), m_field348(0), m_field34c(0), m_field350(1.0f),
      m_field354(1.0f), m_field358(1.0f), m_field35c(false)
{
    m_field164 = 0;
    m_field168 = 0;
    m_field16c = 0;
    m_field170 = 0;
    m_field174 = 0;
    m_field250 = 0;
    m_field24d = true;
    m_field254 = m_field250;
    m_field25c = 0;                                    // repeated stores in retail
    m_field260 = 0;
    std::memset(m_field18c, 0, sizeof(m_field18c));

    m_field2dc = new HudBlinkManager();
    m_field2dc->add(new HudBlinker(&m_field2b8, &m_field2b0, m_field2b4,
                                  0.0f, 0.7f, 0.3f, true));
}

bool BF2HudManager::onLevelLoaded()
{
    if (m_field104)
    {
        m_field104->setTeam(1);
        if (!g_localProfileManager || !static_cast<ILocalProfileManager*>(g_localProfileManager)->getCurrentProfile())
            m_field104->setMapStatic(true);
    }
    if (game)
        configureMap(game->getCurrentMapName());
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->activateHud();
    eventManager->postEvent(ECHud, 2u, this, 0.0f);
    eventManager->postEvent(ECHud, 10u, this, 0.0f);
    eventManager->postEvent(ECHud, 1u, this, 0.0f);
    return true;
}

// The singleton call is unconditional, then the shipped body returns true.
bool BF2HudManager::onLevelUnloaded()
{
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->deactivateHud();
    return true;
}

// Keep registration order, repeated global reads and the retail spelling.
bool BF2HudManager::registerHudEvents()
{
    eventManager->registerEvent(ECHud, 1, "ECHudHECloseToControlPoint");
    eventManager->registerEvent(ECHud, 2, "ECHudHEHudStateChanged");
    eventManager->registerEvent(ECHud, 9, "ECHudHEMapZoomChanged");
    eventManager->registerEvent(ECHud, 10, "ECHudHEMapStaticChanged");
    eventManager->registerEvent(ECHud, 3, "ECHudHEHudDisable");
    eventManager->registerEvent(ECHud, 4, "ECHudHEHudEnable");
    eventManager->registerEvent(ECHud, 5, "ECHudHEInitLoadingScreen");
    eventManager->registerEvent(ECHud, 6, "ECHudHEUpdateLoadingScreen");
    eventManager->registerEvent(ECHud, 7, "ECHudHESetLoadingScreenDebug");
    eventManager->registerEvent(ECHud, 8, "ECHudHERemoveLoadingScreen");
    eventManager->registerEvent(ECHud, 11, "ECHudHESquadIsFull");
    eventManager->registerEvent(ECHud, 12, "ECHudHESquadIsPrivate");
    eventManager->registerEvent(ECHud, 13, "ECHudHESquadCreationFailed");
    eventManager->registerEvent(ECHud, 14, "ECHudHESquadCreationSuccess");
    eventManager->registerEvent(ECHud, 15, "ECHudHESquadInvalidName");
    eventManager->registerEvent(ECHud, 16, "ECHudHESquadInvitation");
    eventManager->registerEvent(ECHud, 17, "ECHudHESquadJoinRequest");
    eventManager->registerEvent(ECHud, 18, "ECHudHESquadJoinRequestDenied");
    eventManager->registerEvent(ECHud, 19, "ECHudHESquadJoinRequestAccepted");
    eventManager->registerEvent(ECHud, 25, "ECHudHESquadLeaderRequest");
    eventManager->registerEvent(ECHud, 20, "ECHudHEKickedFromSquad");
    eventManager->registerEvent(ECHud, 21, "ECHudHELeftSquad");
    eventManager->registerEvent(ECHud, 22, "ECHudHEJoinedSquad");
    eventManager->registerEvent(ECHud, 23, "ECHudHESquadOrdersReceived");
    eventManager->registerEvent(ECHud, 24, "ECHudHENewSquadLeader");
    eventManager->registerEvent(ECHud, 26, "ECHudHECommanderOrdersReceived");
    eventManager->registerEvent(ECHud, 27, "ECHudHECommanderOrdersSent");
    eventManager->registerEvent(ECHud, 29, "ECHudHERequestRecieved");
    eventManager->registerEvent(ECHud, 30, "ECHudHERequestSent");
    eventManager->registerEvent(ECHud, 28, "ECHudHESquadOrdersSent");
    eventManager->registerEvent(ECHud, 31, "ECHudHEUpdateHud");
    eventManager->registerEvent(ECHud, 32, "ECHudHEWeaponChanged");
    eventManager->registerEvent(ECHud, 33, "ECHudHECommanderAccept");
    eventManager->registerEvent(ECHud, 35, "ECHudHECommanderInvitation");
    eventManager->registerEvent(ECHud, 36, "ECHudHESquadLeaderInvitation");
    eventManager->registerEvent(ECHud, 37, "ECHudHECommanderResign");
    eventManager->registerEvent(ECHud, 34, "ECHudHECommanderDecline");
    eventManager->registerEvent(ECHud, 38, "ECHudHERankChange");
    eventManager->registerEvent(ECHud, 39, "ECHudHEMedalReward");
    eventManager->registerEvent(ECHud, 68, "ECHudHEKickBan");
    eventManager->registerEvent(ECHud, 44, "ECHudHEMissileLock");
    eventManager->registerEvent(ECHud, 43, "ECHudHEUnitSpotted");
    eventManager->registerEvent(ECHud, 40, "ECHudHEUAV");
    eventManager->registerEvent(ECHud, 41, "ECHudHESatellite");
    eventManager->registerEvent(ECHud, 42, "ECHudHEArtillery");
    eventManager->registerEvent(ECHud, 45, "ECHudHENewUnlock");
    eventManager->registerEvent(ECHud, 46, "ECHudHECallForMedic");
    eventManager->registerEvent(ECHud, 48, "ECHudHECallForRepair");
    eventManager->registerEvent(ECHud, 47, "ECHudHECallForAmmo");
    eventManager->registerEvent(ECHud, 49, "ECHudHECallForRevive");
    eventManager->registerEvent(ECHud, 50, "ECHudHEAutoCallForRevive");
    eventManager->registerEvent(ECHud, 51, "ECHudHERadioCall");
    eventManager->registerEvent(ECHud, 52, "ECHudHEWasHit");
    eventManager->registerEvent(ECHud, 57, "ECHudHEIsCloseToMine");
    eventManager->registerEvent(ECHud, 58, "ECHudHENotCloseToMine");
    eventManager->registerEvent(ECHud, 59, "ECHudHEMutedByRemote");
    eventManager->registerEvent(ECHud, 61, "ECHudHETKOption");
    eventManager->registerEvent(ECHud, 62, "ECHudHETKResponse");
    eventManager->registerEvent(ECHud, 64, "ECHudHEControlPointTaken");
    eventManager->registerEvent(ECHud, 65, "ECHudHEControlPointLost");
    eventManager->registerEvent(ECHud, 66, "ECHudHEEnableHelpMessage");
    eventManager->registerEvent(ECHud, 67, "ECHudHEEnableHelp");
    eventManager->registerEvent(ECHud, 70, "ECHudHETicketBleed");
    eventManager->registerEvent(ECHud, 71, "ECHudHETicketStateChanged");
    eventManager->registerEvent(ECHud, 72, "ECHudHEBeginRound");
    eventManager->registerEvent(ECHud, 76, "ECHudHEMapChangeComplete");
    eventManager->registerEvent(ECHud, 77, "ECHudHEMapZoomComplete");
    eventManager->registerEvent(ECHud, 82, "ECHudHEUAVVehicleDestroyed");
    return true;
}

// 0x5A5E20; the console descriptor 0x75C964 names refresh and returns bool.
// This is the entire shipped body, not a fabricated initialization success.
bool BF2HudManager::refresh()
{
    return true;
}

// preserve virtual forwarding, including overrides of setUpdate.
void BF2HudManager::setPaint(bool enabled)
{
    m_field248 = enabled;
    setUpdate(enabled);
}

void BF2HudManager::setUpdate(bool enabled)
{
    m_field249 = enabled;
}

menu::Ref2System* BF2HudManager::getSystem()
{
    return ref2System;
}

bool BF2HudManager::hasActiveMenu()
{
    if (isChatActive())
        return true;
    const int state = getMode();
    return state != 0 && state != 2 && state != 9 && state != 10;
}

std::intptr_t BF2HudManager::getPlayerVehicleIcon(world::IPlayer* player, bool typeIcon)
{
    if (!player)
        return 0;
    world::IObject* defaultVehicle = player->getDefaultVehicle();
    if (defaultVehicle == player->getVehicle())
        return 0;
    world::IObject* vehicle = player->getVehicle();
    if (!vehicle || !vehicle->m_vehicleHudComponent)
        return 0;
    world::IVehicleHudComp* component = static_cast<world::IVehicleHudComp*>(vehicle->m_vehicleHudComponent);
    return typeIcon ? component->getHudIcons()->typeIconPtr : component->getHudIcons()->miniMapIconPtr;
}

world::ICameraObject* BF2HudManager::getCameraObject(int team)
{
    world::IObject* object = static_cast<world::IObject*>(
        static_cast<ISquadManager*>(squadManager)->getCommanderCam(team));
    if (!object)
        return nullptr;
    world::ICameraObject* camera = static_cast<world::ICameraObject*>(
        object->queryInterface(world::IID_ICameraObject));
    if (!camera)
        __debugbreak(); // retail INT3; deliberately no fabricated recovery
    return camera;
}

std::intptr_t BF2HudManager::getPlayerRankIcon(world::IPlayer* player)
{
    if (player)
    {
        world::playerManager->getLocalHumanPlayer(); // shipped call, result discarded
        const int rank = player->getScoreData()->m_rank;
        if (rank >= 0 && rank < 22)
        {
            // The valid-rank arm loads on every call, even when already cached.
            m_field18c[rank] = ref2System->loadTexture(
                formatString("Ingame/GeneralIcons/Ranks/rank_%02i.tga", rank).c_str(), true);
            if (m_field18c[rank])
                return m_field18c[rank];
        }
    }
    if (!m_field18c[0])
        m_field18c[0] = ref2System->loadTexture("Ingame/GeneralIcons/Ranks/rank_00.tga", true);
    return m_field18c[0];
}

std::intptr_t BF2HudManager::getVehicleTypeIcon(int type)
{
    switch (type)
    {
    case 0: return ref2System->loadTexture("Ingame/Vehicles/Icons/Minimap/mini_Tank.tga", true);
    case 1: return ref2System->loadTexture("Ingame/Vehicles/Icons/Minimap/mini_Jeep.tga", true);
    case 3: return ref2System->loadTexture("Ingame/Vehicles/Icons/Minimap/mini_APC.tga", true);
    case 5: return ref2System->loadTexture("Ingame/Vehicles/Icons/Minimap/mini_Jet.tga", true);
    case 20: return ref2System->loadTexture("Ingame/Vehicles/Icons/Minimap/mini_AAVehicle.tga", true);
    case 21: return ref2System->loadTexture("Ingame/Vehicles/Icons/Minimap/mini_rib.tga", true);
    case 22:
    case 23: return ref2System->loadTexture("Ingame/Vehicles/Icons/Minimap/mini_attack_Heli.tga", true);
    case 24: return ref2System->loadTexture("Ingame/Kits/Icons/kit_Sniper_outline.tga", true);
    case 25: return ref2System->loadTexture("Ingame/Player/Icons/Minimap/mini_Soldier.dds", true);
    case 26: return ref2System->loadTexture("Ingame/Minimap/Icons/unitSpotted.tga", true);
    default: return 0;
    }
}

// The second default-vehicle and repeated current-vehicle reads are
// observable virtual calls. A missing default vehicle is the special value 7.
int BF2HudManager::getPlayerVehicleType(world::IPlayer* player)
{
    if (!player)
        return -1;
    if (!player->getDefaultVehicle())
        return 7;
    world::IObject* defaultVehicle = player->getDefaultVehicle();
    world::IObject* object;
    if (player->getVehicle() == defaultVehicle)
        object = getPlayerKit(player);
    else
        object = player->getVehicle();
    if (!object || !object->m_vehicleHudComponent)
        return -1;
    return static_cast<world::IVehicleHudComp*>(object->m_vehicleHudComponent)->getHudInfo()->vehicleType;
}

void BF2HudManager::selectBoolColor(bool value, bool comparison,
    float& r, float& g, float& b, float& a,
    float unequalR, float unequalG, float unequalB, float unequalA,
    float equalR, float equalG, float equalB, float equalA)
{
    if (value == comparison)
    {
        r = equalR; g = equalG; b = equalB; a = equalA;
    }
    else
    {
        r = unequalR; g = unequalG; b = unequalB; a = unequalA;
    }
}

void BF2HudManager::selectFloatColor(float value, float comparison,
    float& r, float& g, float& b, float& a,
    float otherR, float otherG, float otherB, float otherA,
    float lessR, float lessG, float lessB, float lessA)
{
    // fnstsw/test AH,5/jp selects this arm for greater, equal and unordered.
    // `value >= comparison` would incorrectly reverse the NaN behavior.
    if (!(value < comparison))
    {
        r = otherR; g = otherG; b = otherB; a = otherA;
    }
    else
    {
        r = lessR; g = lessG; b = lessB; a = lessA;
    }
}

world::Kit* BF2HudManager::getPlayerKit(world::IPlayer* player)
{
    world::ISoldier* soldier = getPlayerSoldier(player);
    return soldier ? soldier->getKit() : nullptr;
}

world::ISoldier* BF2HudManager::getPlayerSoldier(world::IPlayer* player)
{
    if (!player || !player->getDefaultVehicle())
        return nullptr;
    return static_cast<world::ISoldier*>(player->getDefaultVehicle()->queryInterface(world::IID_ISoldier));
}

bool BF2HudManager::isPlayerOnFoot(world::IPlayer* player)
{
    if (!player)
        return false;
    world::IObject* defaultVehicle = player->getDefaultVehicle();
    return defaultVehicle == player->getVehicle(); // both null also compares equal
}

int BF2HudManager::getLocalPlayerTeam()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    return player ? player->getTeam() : 0;
}

void BF2HudManager::ignorePlayer(int id)
{
    if (isPlayerIgnored(id))
        outputChatMessage(formatString("Error: Player is already ignored!"));
    else if (world::playerManager->getPlayerFromId(id))
        m_field0e8.push_front(id);
    else
        outputChatMessage(formatString("Error: Could not locate player: %d", id));
}

void BF2HudManager::unignorePlayer(int id)
{
    const auto found = std::find(m_field0e8.begin(), m_field0e8.end(), id);
    if (found != m_field0e8.end())
        m_field0e8.erase(found);
}

bool BF2HudManager::isPlayerIgnored(int id)
{
    return std::find(m_field0e8.begin(), m_field0e8.end(), id) != m_field0e8.end();
}

std::string BF2HudManager::listIgnoredPlayers()
{
    std::string result;
    for (auto it = m_field0e8.begin(); it != m_field0e8.end(); ++it)
    {
        const int id = *it;
        world::IPlayer* player = world::playerManager->getPlayerFromId(id);
        if (player)
            result += formatString("id: %d name: %s\n", id, player->getName().c_str());
        else
            unignorePlayer(id);
        // Shipped defect 0x7B0645..0x7B064B: the virtual removal erases this
        // iterator, then the loop advances it anyway. Do not repair that here.
    }
    return std::string(result); // PE explicitly copy-constructs the result
}

namespace {
// [INFERRED] Local instantiation of Core's 0x431530 replacement helper. Native
// string operations preserve its search position and replacement advancement.
std::string& replaceChatMarker(std::string& text, const std::string& marker,
                               const std::string& replacement)
{
    std::string::size_type position = 0;
    while ((position = text.find(marker.c_str(), position, marker.size())) != std::string::npos)
    {
        text.replace(position, marker.size(), replacement);
        position += replacement.size();
    }
    return text;
}
}

void BF2HudManager::outputChatMessage(std::string text)
{
    // Exact two-byte markers at 0x9357B4/B0/AC/A8, in shipped order.
    text = replaceChatMarker(text, std::string("\xA7" "0"), std::string(""));
    text = replaceChatMarker(text, std::string("\xA7" "1"), std::string(""));
    text = replaceChatMarker(text, std::string("\xA7" "2"), std::string(""));
    text = replaceChatMarker(text, std::string("\xA7" "3"), std::string(""));
    std::ostringstream output;
    output << text;
    io::mainConsole->output(output.str()); // unconditional; no newline is added
}

bool BF2HudManager::isChatActive()
{
    return m_field0b0 || m_field0b1 || m_field0b2;
}

void BF2HudManager::showRankUpIcon(std::string path, float seconds)
{
    if (seconds > 0.0f)
    {
        if (m_field104->getBool("ShowMedalIcon"))
            m_field104->setBool("ShowMedalIcon", false);
        m_field104->setString("RankUpIconPathString", path);
        m_field104->setBool("ShowRankUpIcon", true);
        m_field234 = seconds;
    }
    else
        m_field104->setBool("ShowRankUpIcon", false);
}

void BF2HudManager::showMedalIcon(std::string path, float seconds)
{
    if (seconds > 0.0f)
    {
        if (m_field104->getBool("ShowRankUpIcon"))
            m_field104->setBool("ShowRankUpIcon", false);
        m_field104->setString("MedalIconPathString", path);
        m_field104->setBool("ShowMedalIcon", true);
        m_field238 = seconds;
    }
    else
        m_field104->setBool("ShowMedalIcon", false);
}

void BF2HudManager::showWideHelp(std::wstring text, float seconds)
{
    // PE +DC is ELF +E0: the single MSVC destructor slot shifts isPlayingDemo.
    if (!text.empty() && seconds > 0.0f && game && !game->isPlayingDemo())
    {
        m_field104->setWideString("IngameHelpString", text);
        m_field104->setBool("IngameHelpShow", true);
        m_field22c = seconds;
    }
    else
        m_field104->setBool("IngameHelpShow", false);
}

void BF2HudManager::showHelp(std::string text, float seconds)
{
    showWideHelp(hfe::toWideString(text), seconds);
}

void BF2HudManager::getString(meme::Data* data, std::string& out)
{
    char buffer[256];
    data->getString(ref2System, buffer, 256);
    const std::string text(buffer);
    out = text;
}

// Lookup is unconditional; the HUD-global test follows the nonempty
// result test. The fallback is the Core conversion, not this object's +0x54.
std::wstring BF2HudManager::getLocalizedString(std::string key)
{
    std::wstring localized = static_cast<io::ILexicon*>(io::lexicon)->getString(key);
    if (!localized.empty() && hudManager)
        return localized;
    return hfe::toWideString(key);
}

std::wstring BF2HudManager::replaceWideToken(std::wstring text, std::string token,
                                            std::wstring replacement)
{
    std::wstring result(text);
    std::wstring wideToken = toWideString(token);
    std::wstring wideReplacement(replacement);
    const std::wstring::size_type position = result.find(wideToken, 0);
    if (position != std::wstring::npos)
        result.replace(position, wideToken.size(), wideReplacement);
    return result;
}

std::wstring BF2HudManager::replaceWideTokenFromString(std::wstring text, std::string token,
                                                       std::string replacement)
{
    std::wstring result(text);
    std::wstring wideToken = toWideString(token);
    std::wstring wideReplacement = toWideString(replacement);
    const std::wstring::size_type position = result.find(wideToken, 0);
    if (position != std::wstring::npos)
        result.replace(position, wideToken.size(), wideReplacement);
    return result;
}

std::wstring BF2HudManager::replaceWideTokenFromInt(std::wstring text, std::string token,
                                                   int replacement)
{
    char buffer[64];
    std::sprintf(buffer, "%d", replacement);
    return replaceWideTokenFromString(text, token, std::string(buffer));
}

std::wstring BF2HudManager::replaceStringTokenFromInt(std::string text, std::string token,
                                                     int replacement)
{
    char buffer[64];
    std::sprintf(buffer, "%d", replacement);
    return replaceStringToken(text, token, std::string(buffer));
}

std::wstring BF2HudManager::replaceStringToken(std::string text, std::string token,
                                              std::string replacement)
{
    std::wstring result = toWideString(text);
    std::wstring wideToken = toWideString(token);
    std::wstring wideReplacement = toWideString(replacement);
    const std::wstring::size_type position = result.find(wideToken, 0);
    if (position != std::wstring::npos)
        result.replace(position, wideToken.size(), wideReplacement);
    return result;
}

// Preserve the retail allocation bound (wide length + 1 bytes).
// [UNRESOLVED] In a multibyte locale a truncated successful conversion can lack
// a terminator; the shipped code still assigns it as a C string. Do not harden it.
std::string BF2HudManager::toNarrowString(const std::wstring& text)
{
    char* buffer = new char[text.size() + 1];
    const std::size_t converted = std::wcstombs(buffer, text.c_str(), text.size() + 1);
    std::string result;
    if (converted == static_cast<std::size_t>(-1))
        result = "";
    else
        result = buffer;
    delete[] buffer;
    return result;
}

std::wstring BF2HudManager::toWideString(const std::string& text)
{
    wchar_t* buffer = new wchar_t[text.size() + 1];
    const std::size_t converted = std::mbstowcs(buffer, text.c_str(), text.size() + 1);
    std::wstring result;
    if (converted == static_cast<std::size_t>(-1))
        result = L"";
    else
        result = buffer;
    delete[] buffer;
    return result;
}


// The signed selection favors GI when both markers exist,
// even when PI occurs first. Keep the shipped fallback and virtual call order.
std::string BF2HudManager::replaceInputKeys(std::string text)
{
    for (;;)
    {
        if (text.find("#GI") == std::string::npos && text.find("#PI") == std::string::npos)
            return text;
        int position = static_cast<int>(text.find("#GI"));
        const int playerPosition = static_cast<int>(text.find("#PI"));
        int smaller = position;
        if (position > playerPosition)
            smaller = playerPosition;
        if (smaller < 0 && playerPosition >= 0)
            position = playerPosition;
        if (position == -1)
            continue;
        const auto end = text.find('#', position + 1);
        if (end == std::string::npos)
            return text;
        std::string rawToken = text.substr(position + 1, end - position - 1);
        std::string token(rawToken);
        bool second = false;
        if (rawToken.find('_') != std::string::npos && rawToken.find('_') != std::string::npos)
        {
            if (rawToken.find("_2") != std::string::npos)
                second = true;
            if (rawToken.find('_') != std::string::npos)
                token = rawToken.substr(0, rawToken.find('_'));
        }
        std::stringstream stream(token, std::ios::in | std::ios::out);
        bool gameInput = false;
        int input;
        if (token.find("GI") != std::string::npos)
        {
            io::GameInputMap parsed;
            stream >> parsed;
            input = parsed;
            gameInput = true;
        }
        else if (token.find("PI") != std::string::npos)
        {
            io::PlayerInputMap parsed;
            stream >> parsed;
            input = parsed;
        }
        else
            return text;
        if (input == -1)
            return text;
        io::IControlMap* map = gameInput
            ? static_cast<io::IControlMapManager*>(io::controlMapManager)->getControlMap("defaultGameControlMap")
            : static_cast<io::IControlMapManager*>(io::controlMapManager)->getActive();
        std::string bindingKey;
        std::wstring bindingText;
        IControlSettings* controlSettings = static_cast<IControlSettings*>(g_controlSettings);
        if (!controlSettings->getBindingDescription(map, input, false, second, gameInput, bindingKey, bindingText))
        {
            map = static_cast<io::IControlMapManager*>(io::controlMapManager)->getControlMap("defaultPlayerInputControlMap");
            controlSettings->getBindingDescription(map, input, false, second, gameInput, bindingKey, bindingText);
        }
        if (bindingText.empty())
            bindingText = getLocalizedString(bindingKey);
        bindingText = bindingText.substr(bindingText.find(L'_') + 1);
        std::string marker = std::string("#") += rawToken;
        std::string replacement = hudManager->toNarrowString(bindingText);
        std::string fullMarker(marker);
        fullMarker += "#";
        std::wstring localized = getLocalizedString(text);
        std::wstring replaced = replaceWideTokenFromString(localized, fullMarker, replacement);
        text = toNarrowString(replaced);
    }
}

// The signed selection favors GI when both markers exist,
// even when PI occurs first. Keep the shipped fallback and virtual call order.
std::wstring BF2HudManager::replaceWideInputKeys(std::wstring text)
{
    for (;;)
    {
        if (text.find(L"#GI") == std::wstring::npos && text.find(L"#PI") == std::wstring::npos)
            return text;
        int position = static_cast<int>(text.find(L"#GI"));
        const int playerPosition = static_cast<int>(text.find(L"#PI"));
        int smaller = position;
        if (position > playerPosition)
            smaller = playerPosition;
        if (smaller < 0 && playerPosition >= 0)
            position = playerPosition;
        if (position == -1)
            continue;
        const auto end = text.find(L'#', position + 1);
        if (end == std::wstring::npos)
            return text;
        std::wstring rawToken = text.substr(position + 1, end - position - 1);
        std::wstring token(rawToken);
        bool second = false;
        if (rawToken.find(L'_') != std::wstring::npos && rawToken.find(L'_') != std::wstring::npos)
        {
            if (rawToken.find(L"_2") != std::wstring::npos)
                second = true;
            if (rawToken.find(L'_') != std::wstring::npos)
                token = rawToken.substr(0, rawToken.find(L'_'));
        }
        std::wstringstream stream(token, std::ios::in | std::ios::out);
        bool gameInput = false;
        int input;
        if (token.find(L"GI") != std::wstring::npos)
        {
            io::GameInputMap parsed;
            stream >> parsed;
            input = parsed;
            gameInput = true;
        }
        else if (token.find(L"PI") != std::wstring::npos)
        {
            io::PlayerInputMap parsed;
            stream >> parsed;
            input = parsed;
        }
        else
            return text;
        if (input == -1)
            return text;
        io::IControlMap* map = gameInput
            ? static_cast<io::IControlMapManager*>(io::controlMapManager)->getControlMap("defaultGameControlMap")
            : static_cast<io::IControlMapManager*>(io::controlMapManager)->getActive();
        std::string bindingKey;
        std::wstring bindingText;
        IControlSettings* controlSettings = static_cast<IControlSettings*>(g_controlSettings);
        if (!controlSettings->getBindingDescription(map, input, false, second, gameInput, bindingKey, bindingText))
        {
            map = static_cast<io::IControlMapManager*>(io::controlMapManager)->getControlMap("defaultPlayerInputControlMap");
            controlSettings->getBindingDescription(map, input, false, second, gameInput, bindingKey, bindingText);
        }
        if (bindingText.empty())
            bindingText = getLocalizedString(bindingKey);
        bindingText = bindingText.substr(bindingText.find(L'_') + 1);
        std::string marker = std::string("#") += hfe::makeNarrow(rawToken);
        text = replaceWideToken(text, std::string(marker) += "#", bindingText);
    }
}

} } }
#endif
