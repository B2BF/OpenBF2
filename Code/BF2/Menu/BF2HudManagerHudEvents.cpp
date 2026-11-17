// HUD event payload handling. [INFERRED] Source split. Unimplemented leaves stop when invoked.
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h" // [CHOSEN, not recovered]
#endif
#include "BF2HudManager.h"
#if !DEDICATED
#include "Bf2MenuMapNode.h"
#include "HudAwardCatalogue.h"
#include "HudCommander.h"
#include "HudInformationAdapter.h"
#include "HudLoadingScreen.h"
#include "HudLocalRadioMessage.h"
#include "HudMessages.h"
#include "HudQuestionManager.h"
#include "HudScoreboard.h"
#include "HudSquad.h"
#include "HudSquadInputValue.h"
#include "HudVehicleInformation.h"
#include "../Core/EventManager.h"
#include "../Core/LoadStat.h"
#include "../Core/StringUtil.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/IGameLogic.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Game/Objects/Game/ControlPoint.h"
#include "../Game/Objects/IPlayer.h"
#include "../IO/Network/NetworkManager.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/PlayerScoreData.h"
#include "../Sound/BfSound.h"
#include "../Sound/NullSound.h"
#include <cstdlib>
#include <cstring>
#include <new>
#include <sstream>

namespace dice { namespace hfe {
extern IGameLogic* gameLogic;
extern IBase* squadManager;
extern hud::BF2HudManager* hudManager;
namespace hud {
namespace {
// Payload words/bytes are copied, not overlaid with a retail STL object.
template<class T> T eventValue(const void* data, unsigned offset = 0)
{
    T value;
    std::memcpy(&value, static_cast<const unsigned char*>(data) + offset, sizeof value);
    return value;
}
Vec3 translation(const Mat4& matrix)
{ return Vec3{matrix.m[12], matrix.m[13], matrix.m[14]}; }

float stingerDelay(const char* name)
{
    // The temporary lookup key is destroyed before either sound callback.
    ISound* sound = static_cast<ISound*>(g_bfSound->getSound(std::string(name)));
    float delay = 0;
    if (sound) {
        const float* sample = static_cast<const float*>(sound->getSampleInfo(0));
        if (sample) {
            // PE75BA4D/75BC73 retains z+y+x in x87 until the one float store.
#if defined(_MSC_VER) && defined(_M_IX86)
            __asm {
                mov eax, sample
                fld dword ptr [eax+8]
                fadd dword ptr [eax+4]
                fadd dword ptr [eax]
                fstp delay
            }
#else
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Retain x87-width values on both Mac architectures.
            delay = static_cast<float>(static_cast<bf2x87::Real>(sample[2]) + sample[1] + sample[0]);
#else
            delay = static_cast<float>(static_cast<long double>(sample[2]) + sample[1] + sample[0]);
#endif
#endif
        }
        sound->play(); // PE+14 is the no-argument overload in MSVC order.
    }
    return delay;
}

}

// Converts HUD text to wide characters using a buffer sized from the narrow string length.
std::wstring convertHudEventText(const std::string& text)
{
    wchar_t* buffer = new wchar_t[text.size() + 1];
    const std::size_t count = std::mbstowcs(buffer, text.c_str(), text.size() + 1);
    std::wstring result;
    result = count == static_cast<std::size_t>(-1) ? L"" : buffer;
    delete[] buffer;
    return result;
}

void BF2HudManager::handleHudEvent(unsigned int id, EventNode*, void* data)
{
    if (!game) return;
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    switch (id) {
    case 2:
        if (m_field249) applyModeTransition();
        return;
    case 3: m_field248 = false; return;
    case 4: m_field248 = true; return;
    case 5: onLevelUnloaded(); return;
    case 6:
        if (m_field134) m_field134->setProgress(eventValue<float>(data));
        return;
    case 7:
        if (m_field134) {
            m_field134->setDebug(static_cast<const char*>(data));
            loadStat->executeLoadCallback();
        }
        return;
    case 8: onLevelLoaded(); return;
    case 32: {
        const int first = eventValue<int>(data), second = eventValue<int>(data, 4);
        if (hudVehicleInformation && hudVehicleInformation->canSelectWeapon(false)) {
            if (first == -1 || second == -1) return;
            m_field148->m_duration = 2.0f;
            setMode(10, true);
        }
        if (getMode() != 10) return;
        if (m_field34c != static_cast<unsigned>(second)) {
            m_field148->selectItem(second, false);
            m_field34c = second;
        }
        if (m_field348 != static_cast<unsigned>(first)) {
            m_field148->selectItem(first, false);
            m_field348 = first;
        }
        return;
    }
    case 76:
        if (isCommanderShown()) {
            world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
            if (player) static_cast<ISquadManager*>(squadManager)->setCommanderToOverheadCamMode(player->getId());
        }
        return;
    case 36: {
        auto* player = world::playerManager->getPlayerFromId(eventValue<int>(data));
        if (m_field100 && player && local) {
            const int team = player->getTeam();
            if (team == local->getTeam()) {
                const int playerId = player->getId();
                if (playerId == local->getId()) m_field100->addQuestion(4, 1, true);
            }
        }
        return;
    }
    case 35:
        if (m_field100) m_field100->addQuestion(3, 1, true);
        return;
    case 33: {
        auto* player = world::playerManager->getPlayerFromId(eventValue<int>(data));
        if (!player || !local) return;
        const int team = player->getTeam();
        if (team != local->getTeam()) return;
        const int playerId = player->getId();
        if (playerId != local->getId()) game->localGameInfoMessage(TCPlayer, "new_commander");
        else {
            if (m_field100) {
                m_field100->replyCategory(2, false, true, false);
                m_field100->replyCategory(0, false, true, false);
                m_field100->replyCategory(1, false, true, false);
                m_field100->replyCategory(4, false, true, true);
            }
            game->localGameInfoMessage(TCPlayer, "commander_assign");
            m_field120->setPage(1);
            if (m_field13c && hudMap) {
                m_field13c->selectAllSquads(false);
                m_field13c->setArtilleryReloading(true);
                m_field13c->setSuppliesReloading(true);
                m_field13c->setVehicleDropReloading(true);
                hudMap->resetSatelliteReload();
                hudMap->resetUAVReload();
            }
        }
        if (m_field138) m_field138->m_field09f = true;
        return;
    }
    case 34:
        addServerChat(getLocalizedString("HUD_COMMANDERAPPLY_POSFILLED"), 0);
        refreshChatCategory(8);
        if (m_field100) m_field100->removeQuestions(3, -1, true, 0.0f);
        return;
    case 37: {
        auto* player = world::playerManager->getPlayerFromId(eventValue<int>(data));
        if (!player || !local) return;
        const int playerId = player->getId();
        if (playerId != local->getId() || !gameLogic) return;
        {
            const auto team = getLowerCase(gameLogic->getTeamName(local->getTeam()));
            const auto label = getLocalizedString(formatString("HUD_TEAMNAME_%s", team.c_str()));
            addServerChat(replaceWideToken(replaceWideInputKeys(getLocalizedString(
                "HUD_HUD_COMMANDER_NO_LONGER")), "#TEAMNAME#", label.c_str()), 0);
        }
        game->gameInfoMessage(TCTeam, "commander_resign", local->getTeam(), 0, false);
        configureMap(game->getCurrentMapName());
        if (hudMap) {
            hudMap->m_field690 = false;
            hudMap->applyZoom(0, true);
            hudMap->m_savedSquadZoom = 0;
            hudMap->m_savedCommanderZoom = 0;
            hudMap->m_savedZoom = 0;
            hudMap->setExpanded(false);
            meme::dispatchMapSetSelectionWord(hudMap, -1, 0);
            m_field104->setField014(0);
        }
        toggleSquadCommander();
        setMode(0, true);
        return;
    }
    case 11:
        if (local && gameLogic) {
            const int squad = static_cast<HudSquadInputValue*>(data)->get();
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(local->getTeam(), squad);
            if (info) {
                const char* name = info->getSquadName().c_str();
                const auto team = getLowerCase(gameLogic->getTeamName(local->getTeam()));
                const auto label = getLocalizedString(formatString("HUD_TEAMNAME_%s", team.c_str()));
                addServerChat(replaceWideTokenFromString(replaceWideToken(replaceWideInputKeys(
                    getLocalizedString("HUD_HUD_SQUAD_FULL")), "#TEAMNAME#", label.c_str()), "#SQUADNAME#", name), 0);
            }
            g_bfSound->playSound("warning");
        }
        return;
    case 15: {
        const int squad = eventValue<int>(data);
        if (squad && local) {
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(local->getTeam(), squad);
            addServerChat(replaceWideTokenFromString(replaceWideInputKeys(getLocalizedString(
                "HUD_HUD_SQUAD_EXISTS")), "#SQUADNAME#", info->getSquadName().c_str()), 0);
            g_bfSound->playSound("warning");
        }
        return;
    }
    case 17:
        // PE reads the encoded payload word even with no question owner.
        (void)*reinterpret_cast<const volatile unsigned int*>(static_cast<const unsigned char*>(data) + 4);
        if (m_field100) m_field100->addQuestion(0, static_cast<HudSquadInputValue*>(data)->get(), true);
        return;
    case 14: {
        const int squad = eventValue<int>(data);
        if (local) {
            hudManager->disableVOHelpMessage(10);
            hudManager->enableVOHelpMessage(11);
            m_field120->setPage(1);
            if (m_field138) {
                const bool locked = m_field138->m_createLocked;
                static_cast<ISquadManager*>(squadManager)->setIsPrivate(local->getTeam(), squad, locked, false);
            }
        }
        return;
    }
    case 51:
        if (data) {
            const std::string message(static_cast<const char*>(data));
            game->radioMessage(eventValue<TargetChannel>(data, 0x20), message, local, 0, true, false);
        }
        return;
    case 18: case 19: {
        const int squad = eventValue<int>(data);
        world::IPlayer* player = id == 19 ? world::playerManager->getLocalHumanPlayer() : local;
        if (player) {
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(player->getTeam(), squad);
            if (info) addServerChat(replaceWideTokenFromString(replaceWideInputKeys(getLocalizedString(
                id == 19 ? "HUD_HUD_SQUAD_JOIN_ACCEPTED" : "HUD_HUD_SQUAD_JOIN_DENIED")),
                "#SQUADNAME#", info->getSquadName().c_str()), 0);
            g_bfSound->playSound(id == 19 ? "accepted" : "warning");
        }
        return;
    }
    case 16:
        if (m_field100) m_field100->addQuestion(1, eventValue<int>(data), true);
        return;
    case 12:
        (void)*reinterpret_cast<const volatile unsigned int*>(static_cast<const unsigned char*>(data) + 4);
        if (local) {
            const int squad = static_cast<HudSquadInputValue*>(data)->get();
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(local->getTeam(), squad);
            if (info) {
                hudManager->addServerChat(hudManager->replaceWideTokenFromString(hudManager->replaceWideInputKeys(
                    hudManager->getLocalizedString("HUD_HUD_SQUAD_PRIVATE_REQUEST_JOIN")),
                    "#SQUADNAME#", info->getSquadName().c_str()), 0);
                g_bfSound->playSound("warning");
            }
        }
        return;
    case 23:
        if (local) {
            const int squad = local->getSquadId();
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(local->getTeam(), squad);
            if (info && !local->getIsSquadLeader()) addServerChat(replaceWideInputKeys(
                getLocalizedString("HUD_HUD_SQUAD_RECIEVED_NEW_ORDER_SQUADLEADER")), 0);
        }
        return;
    case 28:
        if (local && !local->getIsThisASpam(false)) {
            const int squad = local->getSquadId();
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(local->getTeam(), squad);
            if (info) {
                addServerChat(replaceWideInputKeys(getLocalizedString("HUD_HUD_SQUAD_ORDER_SENT_MEMBERS")), 0);
                hudMap->clearOrder();
                SquadOrder* order = info->getSquadOrders();
                announceSquadOrder(order, info->getSquadId());
            }
        }
        return;
    case 26:
        if (local && local->getIsSquadLeader()) {
            if (m_field241) {
                addServerTeamChat(replaceWideInputKeys(getLocalizedString("HUD_HUD_SQUAD_RECIEVED_NEW_ORDER_COMMANDER")), 2);
                addServerTeamChat(replaceWideInputKeys(getLocalizedString("HUD_HUD_SQUAD_RECIEVED_ACCEPT_REJECT")), 2);
                {
                    const std::string message("roger_that");
                    const int commander = static_cast<ISquadManager*>(squadManager)->getCommander(local->getTeam());
                    game->radioMessage(TCPlayer, message, local, commander, true, false);
                }
                const int squad = local->getSquadId();
                static_cast<ISquadManager*>(squadManager)->setIsAccepted(local->getTeam(), squad, true,
                    static_cast<OrderType>(1), static_cast<SquadOrderType>(5), false);
                if (m_field100) m_field100->removeQuestions(2, -1, true, 0.0f);
            } else if (m_field100) m_field100->addQuestion(2, 1, true);
        }
        return;
    case 27: {
        const int squad = eventValue<int>(data);
        if (local && !local->getIsThisASpam(false)) {
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(local->getTeam(), squad);
            if (info) {
                addServerChat(replaceWideTokenFromString(replaceWideInputKeys(getLocalizedString(
                    "HUD_HUD_SQUAD_ORDER_SENT_SQUAD")), "#SQUADNAME#", info->getSquadName().c_str()), 0);
                hudMap->clearOrder();
                SquadOrder* order = info->getCommanderOrders();
                announceSquadOrder(order, info->getSquadId());
            }
        }
        return;
    }
    case 29:
        if (data) {
            const int type = eventValue<int>(data, 4), squad = eventValue<int>(data);
            announceSquadRequest(squad, type);
            enableVOHelpMessage(37);
        }
        return;
    case 30: {
        const int type = eventValue<int>(data);
        if (local && !local->getIsThisASpam(false)) {
            const int squad = local->getSquadId();
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(local->getTeam(), squad);
            if (info) {
                addServerChat(replaceWideInputKeys(getLocalizedString("HUD_HUD_SQUAD_REQUEST_TO_COMMANDER")), 0);
                hudMap->clearOrder();
                SquadOrder* order = info->getRequest(static_cast<SquadOrderType>(type));
                announceSquadOrder(order, info->getSquadId());
            }
        }
        return;
    }
    case 20: {
        const int playerId = eventValue<int>(data);
        auto* player = world::playerManager->getPlayerFromId(playerId);
        if (!player) return;
        if (local && playerId == local->getId()) {
            addServerChat(replaceWideInputKeys(getLocalizedString("HUD_HUD_SQUAD_IS_KICKED")), 0);
            toggleSquadCommander();
            g_bfSound->playSound("kicked");
        } else addServerChat(replaceWideTokenFromString(replaceWideInputKeys(getLocalizedString(
            "HUD_SQUAD_PLAYERKICKEDFROMSQUAD")), "#PLAYERNAME#", player->getName().c_str()), 0);
        return;
    }
    case 21: {
        auto* player = world::playerManager->getPlayerFromId(eventValue<int>(data));
        if (!player || !local) return;
        const int team = player->getTeam();
        if (team != local->getTeam()) return;
        if (static_cast<unsigned>(local->getSquadId()) > 0 && player != local)
            addServerChat(replaceWideTokenFromString(replaceWideInputKeys(getLocalizedString(
                "HUD_HUD_SQUAD_HAS_LEFT")), "#PLAYERNAME#", player->getName().c_str()), 0);
        else if (m_field138 && player == local) {
            m_field138->setSquadExpanded(local->getSquadId(), false);
            if (m_field100) {
                m_field100->replyCategory(2, false, true, false);
                m_field100->replyCategory(1, false, true, false);
                m_field100->replyCategory(0, false, true, true);
            }
            toggleSquadCommander();
        }
        return;
    }
    case 22: {
        auto* player = world::playerManager->getPlayerFromId(eventValue<int>(data));
        if (!player || !local || static_cast<unsigned>(local->getSquadId()) == 0) return;
        const int team = player->getTeam();
        if (team != local->getTeam()) return;
        if (m_field138 && player == local) {
            m_field138->setSquadExpanded(local->getSquadId(), true);
            toggleSquadCommander();
        } else addServerChat(replaceWideTokenFromString(replaceWideInputKeys(getLocalizedString(
            "HUD_HUD_SQUAD_HAS_JOINED")), "#PLAYERNAME#", player->getName().c_str()), 0);
        return;
    }
    case 24: {
        const int playerId = eventValue<int>(data);
        auto* player = world::playerManager->getPlayerFromId(playerId);
        if (!local) return;
        if (playerId == local->getId()) {
            if (m_field138) m_field138->setSquadExpanded(local->getSquadId(), true);
            const int squad = player->getSquadId(); // No null guard on the lookup result.
            if (world::playerManager->getNrOfPlayersInSquad(player->getTeam(), squad) > 1)
                game->radioMessage(TCSquad, "new_squadleader", local, 0, true, false);
            addServerChat(replaceWideInputKeys(getLocalizedString("HUD_HUD_SQUAD_SQUADLEADER")), 0);
        } else if (player) addTeamChat(replaceWideTokenFromString(replaceWideInputKeys(getLocalizedString(
            "HUD_HUD_SQUAD_NEW_SQUADLEADER")), "#PLAYERNAME#", player->getName().c_str()), nullptr, 0);
        return;
    }
    case 42:
        if (data) {
            if (eventValue<unsigned char>(data, 12)) {
                auto* player = world::playerManager->getLocalHumanPlayer();
                if (!player) return;
                if (player->getIsCommander()) game->localGameInfoMessage(TCCommander, "artillery_fire");
                else {
                    auto* root = world::getRootParent(player->getVehicle());
                    if (root) {
                        const Mat4& transform = root->getAbsoluteTransformation();
                        if (static_cast<HudInformationLayer*>(g_hudInformationLayer)->isWithinRadius(
                                eventValue<Vec3>(data), translation(transform), m_field2d4)
                            && !player->getIsAIPlayer())
                            game->localGameInfoMessage(TCCommander, "COMMSL_COMRADIO_artilleryheadsup");
                    }
                }
                if (player->getIsCommander()) {
                    enableVOHelpMessage(19);
                    m_field13c->setArtilleryReloading(true);
                }
            } else {
                auto* player = world::playerManager->getLocalHumanPlayer();
                if (player && player->getIsCommander()) {
                    m_field13c->setArtilleryReloading(false);
                    g_bfSound->playSound("HUD_Artillery_fail");
                }
            }
        }
        return;
    case 83: case 74:
        if (data) {
            auto* player = world::playerManager->getLocalHumanPlayer();
            if (!player) return;
            auto* root = world::getRootParent(player->getVehicle());
            if (!root) return;
            const Mat4& transform = root->getAbsoluteTransformation();
            world::IObject* dropped = id == 74 ? io::networkManager->getObject(eventValue<unsigned short>(data)) : nullptr;
            if (player->getIsAIPlayer()) return;
            if (player->getIsCommander()) game->localGameInfoMessage(TCCommander,
                id == 74 ? "vehicledrop_dropinitiated" : "supplies_dropinitiated");
            else if (id == 83 || dropped) {
                const Vec3 origin = translation(transform); // Captured before getPosition callback.
                const Vec3 position = id == 74 ? dropped->getAbsolutePosition() : eventValue<Vec3>(data);
                if (static_cast<HudInformationLayer*>(g_hudInformationLayer)->isWithinRadius(
                    position, origin, id == 74 ? 100.0f : m_field2d8))
                    game->localGameInfoMessage(TCCommander, id == 74 ? "vehicledrop_incoming" : "supplies_incoming");
            }
        }
        return;
    case 40:
        if (data) {
            auto* player = world::playerManager->getLocalHumanPlayer();
            if (!player) return;
            auto* root = world::getRootParent(player->getVehicle());
            if (!root) return;
            root->getAbsoluteTransformation(); // Discarded retail callback.
            if (!player->getIsAIPlayer()) game->localGameInfoMessage(TCCommander, "uav_online");
        }
        return;
    case 38: {
        auto* player = static_cast<world::IPlayer*>(data);
        if (local && player && local == player && !local->getIsAIPlayer()) {
            if (!player->getScoreData()) return;
            const int rank = player->getScoreData()->m_rank;
            const auto key = formatString("RANK_%i", rank);
            addRadioChat(replaceWideToken(replaceWideInputKeys(getLocalizedString("HUD_HUD_RANK_RANK_DEFAULT")),
                "#RANK#", getLocalizedString(key)), player, rank, true);
            showRankUpIcon(formatString("Ingame/GeneralIcons/Ranks/rank_%02i_Large.tga", rank), 10.0f);
            const float delay = stingerDelay("promotionStinger");
            void* storage = ::operator new(sizeof(HudLocalRadioMessage));
            auto* message = storage ? new(storage) HudLocalRadioMessage : nullptr;
            message->channel = TCCommander;
            message->message = "AUTO_RULES_youbeenrecommendedforpromotion";
            eventManager->postEvent(ECHud, 73u, message, delay);
            m_field230 = true;
        } else {
            const int rank = player->getScoreData()->m_rank; // Retail leaves player/score unchecked.
            const auto key = formatString("RANK_%i", rank);
            const char* name = player->getName().c_str();
            addRadioChat(replaceWideToken(replaceWideTokenFromString(replaceWideInputKeys(
                getLocalizedString("HUD_HUD_RANK_PROMOTED")), "#PLAYERNAME#", name),
                "#RANK#", getLocalizedString(key)), player, rank, false);
        }
        return;
    }
    case 39:
        if (data) {
            auto* player = world::playerManager->getPlayerFromId(eventValue<int>(data));
            if (!local || !player || local != player) return;
            HudAwardDescription* award = m_field14c->findAward(eventValue<unsigned>(data, 4), eventValue<unsigned>(data, 8));
            if (award && !local->getIsAIPlayer()) {
                const float delay = stingerDelay("awardStinger");
                void* storage = ::operator new(sizeof(HudLocalRadioMessage));
                auto* message = storage ? new(storage) HudLocalRadioMessage : nullptr;
                message->channel = TCCommander;
                message->message = award->notification;
                eventManager->postEvent(ECHud, 73u, message, delay);
                showMedalIcon(award->iconPath, 10.0f);
                addCommanderChat(award->message, player, eventValue<int>(data, 8));
            } else {
                std::ostringstream text;
                text << "Medal not found (MedalType:" << eventValue<unsigned>(data, 4)
                     << " Value:" << eventValue<unsigned>(data, 8) << ")";
                addCommanderChat(convertHudEventText(text.str()), player, eventValue<int>(data, 8));
            }
        }
        return;
    case 73:
        if (data) {
            auto* message = static_cast<HudLocalRadioMessage*>(data);
            game->localRadioMessage(message->channel, message->message);
            delete message;
        }
        return;
    case 68:
        if (data) {
            auto* player = world::playerManager->getPlayerFromId(eventValue<int>(data));
            if (player) {
                const char* key = eventValue<unsigned char>(data, 4) ? "HUD_TKPUNISH_banned" : "HUD_TKPUNISH_kicked";
                const int playerId = player->getId();
                addTeamChat(replaceWideTokenFromInt(replaceWideTokenFromString(getLocalizedString(key),
                    "#PLAYERNAME#", player->getName()), "#KEY#", playerId), nullptr, 0);
            }
        }
        return;
    case 61:
        if (m_field100) {
            m_field100->setDuration(5, static_cast<double>(eventValue<int>(data, 12)));
            m_field100->addQuestion(5, eventValue<int>(data, 8), true);
        }
        return;
    case 62:
        if (!eventValue<unsigned char>(data)) addServerChat(getLocalizedString("HUD_TEXT_TEAMKILL_PUNISHED"), 2);
        return;
    case 45: {
        auto* player = world::playerManager->getPlayerFromId(eventValue<int>(data));
        if (local && player && player == local && !local->getIsAIPlayer())
            addServerChat(replaceWideInputKeys(getLocalizedString(eventValue<unsigned char>(data, 4)
                ? "HUD_HUD_FIRST_UNLOCK" : "HUD_HUD_NEW_UNLOCK")), 0);
        return;
    }
    case 63: {
        const bool show = eventValue<int>(data) > 0;
        getSquad()->m_showCommander = show;
        return;
    }
    case 64: {
        auto* point = static_cast<world::ControlPoint*>(gameLogic->getControlPoint(eventValue<unsigned>(data)));
        auto* player = world::playerManager->getPlayerFromId(eventValue<int>(data, 4));
        if (!player || !point) return;
        auto* capture = static_cast<world::IControlPoint*>(point);
        if (!capture->getAllowCaptureByTeam(1) || !capture->getAllowCaptureByTeam(2)
            || !local || local->getIsAIPlayer() || !gameLogic || gameLogic->getGameStatus() != 1) return;
        const int team = player->getTeam();
        if (team != local->getTeam()) return;
        {
            const char* name = player->getName().c_str();
            const auto label = getLocalizedString(point->getName());
            const int messageTeam = local->getTeam();
            addTeamChat(replaceWideTokenFromString(replaceWideToken(replaceWideInputKeys(
                getLocalizedString("AUTO_RULES_wecapturedacp")), "#CPNAME#", label.c_str()),
                "#PLAYERNAME#", name), local, messageTeam);
        }
        game->localRadioMessage(TCCommander, "AUTO_RULES_wecapturedacp");
        return;
    }
    case 65: {
        auto* point = static_cast<world::ControlPoint*>(gameLogic->getControlPoint(eventValue<unsigned>(data)));
        world::playerManager->getPlayerFromId(eventValue<int>(data, 4)); // Discarded lookup precedes point guard.
        if (!point) return;
        auto* capture = static_cast<world::IControlPoint*>(point);
        if (!capture->getAllowCaptureByTeam(1) || !capture->getAllowCaptureByTeam(2)
            || !local || local->getIsAIPlayer() || !gameLogic || gameLogic->getGameStatus() != 1) return;
        bool lost = false;
        if (capture->getTeam() != 0) {
            const int team = local->getTeam();
            if (team != capture->getTeam()) return;
            lost = true;
        }
        {
            const auto message = replaceWideToken(replaceWideInputKeys(replaceWideInputKeys(
                getLocalizedString(lost ? "AUTO_RULES_welostacp" : "AUTO_RULES_cptaken"))),
                "#CPNAME#", getLocalizedString(point->getName()));
            const wchar_t* text = message.c_str();
            addTeamChat(text, local, local->getTeam());
        }
        if (lost) game->localRadioMessage(TCCommander, "AUTO_RULES_welostacp");
        return;
    }
    case 66: {
        int message = eventValue<int>(data);
        if (local && !local->getIsAIPlayer()) {
            if (message < 0) message = 0;
            else if (message > 82) message = 82;
            local->enableVOHelpMessage(message);
        }
        return;
    }
    case 67:
        if (local && !local->getIsAIPlayer()) local->enableVOHelp(gameLogic->getEnableVoHelp());
        return;
    case 72:
        if (gameLogic) {
            const int state = gameLogic->getBeginRoundState();
            if (state == 1) setMode(0, true);
            else if (state != 2) clearConnectionMessage();
        }
        return;
    default: return;
    }
}
} } }
#endif
