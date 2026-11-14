// Game-input filtering. [INFERRED] Source split from Menu/BF2HudManager.cpp.
#include "BF2HudManager.h"
#if !DEDICATED
#include "BfEditData.h"
#include "HudAmmo.h"
#include "HudCommander.h"
#include "HudInformationAdapter.h"
#include "HudQuestionManager.h"
#include "HudRadio.h"
#include "HudSquad.h"
#include "HudSquadInputValue.h"
#include "Ref2System.h"
#include "../Core/EventManager.h"
#include "../Core/Ids.h"
#include "../Core/StringUtil.h"
#include "../Game/IGameLogic.h"
#include "../Game/Main/BF2.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/VoipClient/BF2VoipClient.h"         // bf2VoipClient slots (vtable 0x89C1A8)
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Game/GameLogic/SquadManager/Squad.h"
#include "../Game/SingletonServer.h"
#include "../IO/Console/Console.h"
#include "../IO/Input/GameInput.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../Sound/BfSound.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace dice { namespace hfe {
extern IBase* squadManager;
extern hud::BF2HudManager* hudManager;
namespace io { extern IBase* inputManager; }
namespace hud {
namespace {
// PE404580 uses the MSVC __allshl helper, whose count>=64 arm returns zero.
// Its fixed nonnegative callers here never enter the negative-index boundary.
bool inputPressed(const io::GameInput& input, int index)
{
    return index < 74 && static_cast<unsigned char>(index) < 64
        && (input.mappingFlags & (1ULL << static_cast<unsigned char>(index)))
        && input.inputs[index] > 0.5f;
}
void clearMappedInput(io::GameInput& input, int index)
{
    // The only callers supply2 or a value clamped38..46. PE6C8F00's range
    // diagnostic therefore cannot execute; preserve its mapping-gated store.
    if (input.mappingFlags & (1ULL << index)) input.inputs[index] = 0.0f;
}
void toggleHudMode(BF2HudManager* hud, int mode)
{
    hud->setMode(hud->getMode() == mode ? 0 : mode, true);
}
world::IPlayerControlObject* findParentControlObject(world::IObject* object)
{
    while (object) {
        auto* control = static_cast<world::IPlayerControlObject*>(object->queryInterface(world::IID_IPlayerControlObject));
        if (control) return control;
        object = object->getParent();
    }
    return nullptr;
}
}

bool BF2HudManager::filterGameInput(io::GameInput& input)
{
    if (m_field0bf || !m_field248 || !game) return true;
    auto pressed = [&input](int index) { return inputPressed(input, index); };
    if (pressed(19) && !io::mainConsole->getConsoleActive()
        && !isChatActive() && !isSquadRenameActive() && !isSquadCreateActive())
        gameLogic->togglePause();
    if (gameLogic->getGameStatus() != 1) return true;
    m_field229 = pressed(12) || pressed(11);
    m_field22a = pressed(9) || pressed(10);
    if (pressed(71)) { toggleHudMode(this, 28); goto commonTail; }
    if (pressed(72)) { toggleHudMode(this, 29); goto commonTail; }
    if (pressed(2)) {
        if (isChatActive()) {
            closeChat(); clearMappedInput(input, 2); goto commonTail;
        }
        // Retail requires both results in sequence; a stable mode cannot pass.
        if (getMode() != 13) goto commonTail;
        if (getMode() != 14) goto commonTail;
        setMode(getPreviousMode(), true);
        clearMappedInput(input, 2);
        goto commonTail;
    }
    if (isChatActive() || isSquadRenameActive()
        || (isSquadCreateActive() && !game->isPlayingDemo())) goto chatInput;

    if (pressed(60) && !game->isPlayingDemo() && getMode() != 9) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (!player) goto commonTail;
        closeChat();
        if (isOrderMode()) setMode(getPreviousMode(), true);
        if (player->getIsCommander() && player->getIsAlive() && !isPlayerAlive(player)) {
            if (!hudAmmo->canChangeWeapon(player->getVehicle())) goto commonTail;
            findParentControlObject(player->getVehicle()); // Return is discarded by758A42.
            toggleHudMode(this, 15);
        } else toggleHudMode(this, player->getIsSquadLeader() ? 17 : 18);
        goto commonTail;
    }
    if (pressed(51) && m_field138 && !game->isPlayingDemo()) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (!player || player->getIsCommander()) goto commonTail;
        if (getMode() == 17 || getMode() == 18) {
            const int selected = m_field138->m_field08c;
            if (selected > 0 && player->getSquadId() != selected) {
                const int squad = player->getSquadId();
                const int team = player->getTeam();
                if (world::playerManager->getNrOfPlayersInSquad(team, squad) > 1)
                    m_field138->leaveAndReinvite();
                m_field138->joinSquad(m_field138->m_field08c);
            }
        } else {
            setMode(player->getIsSquadLeader() ? 17 : 18, true);
            m_field242 = true;
        }
        goto commonTail;
    }
    if (pressed(50) && m_field138 && !game->isPlayingDemo()) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (!player || player->getIsCommander()) goto commonTail;
        if (player->getIsSquadLeader()) {
            const int squad = player->getSquadId();
            const int team = player->getTeam();
            if (world::playerManager->getNrOfPlayersInSquad(team, squad) > 1
                && static_cast<unsigned int>(player->getSquadId()) > 0) {
                m_field138->leaveAndReinvite();
                setMode(18, true);
                m_field138->finishSquadCreate();
                goto commonTail;
            }
        }
        if (player->getSquadId() == 0) {
            setMode(18, true); m_field138->finishSquadCreate();
        }
        goto commonTail;
    }
    if (pressed(55) && !game->isPlayingDemo()) {
        if (getMode() != 9 && getMode() != 12 && getMode() != 6
            && getMode() != 4 && getMode() != 5 && getMode() != 7) {
            if (isOrderMode()) setMode(getPreviousMode(), true);
            setMode(9, true);
        } else if (pressed(16)) {
            setMode(12, false); cacheMouseInput(false);
        }
        goto commonTail;
    }
    if (pressed(16)) {
        if (getMode() != 0 && getMode() != 9 && getMode() != 2 && getMode() != 10) {
            cacheMouseInput(false); goto commonTail;
        }
        if (!isPlayerAlive(world::playerManager->getLocalHumanPlayer()) || !gameLogic) goto commonTail;
        auto* selectedPlayer = world::playerManager->getLocalHumanPlayer();
        gameLogic->selectSpawnGroup(selectedPlayer, 0);
        setDone(false); setMode(1, true); cacheMouseInput(false);
        goto commonTail;
    }
    if (pressed(52) && !game->isPlayingDemo()) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (player && !player->getIsCommander()) toggleHudMode(this, player->getIsSquadLeader() ? 17 : 18);
        goto commonTail;
    }
    if (pressed(33)) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            if (player->getIsAlive() && !isPlayerAlive(player) && !updateRadioMode()
                && (getMode() == 0 || getMode() == 10)) openSquadRadio(false);
        } else if (game->isPlayingDemo()) openSquadRadio(true);
        goto commonTail;
    }
    if (pressed(34)) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            if (player->getIsAlive() && !isPlayerAlive(player) && !updateRadioMode()
                && (getMode() == 0 || getMode() == 10)) openRadio(false);
        } else if (game->isPlayingDemo()) openRadio(true);
        goto commonTail;
    }
    if (game->isPlayingDemo()) { setMode(0, false); goto commonTail; }
    if (pressed(35)) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (player && player->getIsAlive()) toggleHudMode(this, 2);
        goto commonTail;
    }
    if (pressed(36)) {
        int zoom = static_cast<int>(static_cast<unsigned int>(m_field104->getMapZoom()) + 1u);
        if (zoom > 2) zoom = 0;
        if (getMode() != 1) {
            m_field104->setMapZoom(zoom);
            eventManager->postEvent(ECHud, 9, this, 0.0f);
            hudManager->disableVOHelpMessage(57);
        }
        goto commonTail;
    }
    if (pressed(57)) {
        auto* nametags = SingletonServer::m_iNametagManager;
        if (nametags) {
            const bool enabled = nametags->get3dMapOn();
            nametags->set3dMapOn(!enabled);
        }
        goto commonTail;
    }
    if (pressed(29)) {
        if (isCommanderShown()) setMode(15, true);
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            if (!player->getIsAlive() || isPlayerAlive(player)) {
                toggleHudMode(this, 1);
                if (getMode() == 1 && gameLogic) {
                    hudManager->disableVOHelpMessage(75);
                    gameLogic->selectSpawnGroup(player, 0); setDone(false);
                } else setDone(true);
            } else {
                const bool modeOne = getMode() == 1;
                toggleHudMode(this, 1); setDone(modeOne);
            }
        }
        goto commonTail;
    }
    if (pressed(22) && !isChatActive() && getMode() != 14 && getMode() != 12) {
        m_field2fc = true; std::memset(input.inputs, 0, sizeof(input.inputs));
        consoleCharRead = consoleCharWrite; consoleCharCount = 0;
        toggleSayAll(isCommanderShown()); goto commonTail;
    }
    if (pressed(23) && !isChatActive() && getMode() != 14 && getMode() != 12) {
        m_field2fc = true; std::memset(input.inputs, 0, sizeof(input.inputs));
        consoleCharCount = 0; consoleCharRead = consoleCharWrite;
        toggleSayTeam(isCommanderShown()); goto commonTail;
    }
    if (pressed(24) && !isChatActive() && getMode() != 14 && getMode() != 12) {
        m_field2fc = true; std::memset(input.inputs, 0, sizeof(input.inputs));
        consoleCharCount = 0; consoleCharRead = consoleCharWrite;
        toggleSaySquad(isCommanderShown()); goto commonTail;
    }
    if (pressed(47)) { if (m_field100) m_field100->reply(true); goto commonTail; }
    if (pressed(48)) { if (m_field100) m_field100->reply(false); goto commonTail; }
    if (pressed(49) && m_field100 && m_field100->getCount(2) > 0) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (!player) goto commonTail;
        if (m_field241) {
            addTeamChat(toWideString(formatString("Autoaccept disabled")), nullptr, 0);
            m_field241 = false;
            if (m_field100) m_field100->removeQuestions(2, -1, true, 0.0f);
        } else {
            m_field241 = true;
            if (m_field100) m_field100->removeQuestions(2, -1, true, 0.0f);
            {
                const std::string message("roger_that");
                const int team = player->getTeam();
                const int commander = static_cast<ISquadManager*>(squadManager)->getCommander(team);
                game->radioMessage(static_cast<TargetChannel>(14), message, player, commander, true, false);
            } // PE75949B destroys the message before the next player callback.
            const int squad = player->getSquadId();
            const int repeatedTeam = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->setIsAccepted(repeatedTeam, squad, true,
                static_cast<OrderType>(1), static_cast<SquadOrderType>(5), false);
            const int nextSquad = player->getSquadId();
            const int nextTeam = player->getTeam();
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(nextTeam, nextSquad);
            if (info) {
                SquadOrder* order = info->getCommanderOrders();
                if (order) {
                    const int finalSquad = player->getSquadId();
                    const int finalTeam = player->getTeam();
                    static_cast<ISquadManager*>(squadManager)->issueSquadOrder(finalTeam, finalSquad, *order, false);
                }
            }
        }
        goto commonTail;
    }
    if (pressed(58) && getFlag229() && (getMode() == 15 || getMode() == 21)) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (player && player->getIsCommander() && m_field13c) m_field13c->selectAllSquads(true);
        goto commonTail;
    }
    if (pressed(59) && getFlag229() && (getMode() == 15 || getMode() == 21)) {
        auto* player = world::playerManager->getLocalHumanPlayer();
        if (player && player->getIsCommander() && m_field13c) m_field13c->selectAllSquads(false);
        goto commonTail;
    }
    if (pressed(37) || pressed(38) || pressed(39) || pressed(40) || pressed(41)
        || pressed(42) || pressed(43) || pressed(44) || pressed(45) || pressed(46)) {
        int selected = -1;
        if (pressed(38)) selected = 1; else if (pressed(39)) selected = 2;
        else if (pressed(40)) selected = 3; else if (pressed(41)) selected = 4;
        else if (pressed(42)) selected = 5; else if (pressed(43)) selected = 6;
        else if (pressed(44)) selected = 7; else if (pressed(45)) selected = 8;
        else if (pressed(46)) selected = 9;
        clearMappedInput(input, std::max(38, std::min(46, selected + 37)));
        if (selected < 0) return true; // Retail menu-zero skips the entire common tail.
        if (getMode() == 15 && m_field13c) {
            if (m_field229) m_field13c->toggleSquad(selected);
            else if (m_field22a) m_field13c->selectSquadRange(selected, -1);
            else m_field13c->selectSingleSquad(selected);
            g_bfSound->playSound("weaponSelect");
        } else if ((getMode() == 17 || getMode() == 18) && m_field138) {
            auto* player = world::playerManager->getLocalHumanPlayer();
            if (!player) goto commonTail;
            if (!m_field242) m_field244 = m_field138->toggleSelectedSquad(selected);
            else {
                m_field138->selectSquad(selected);
                m_field138->setSquadExpanded(m_field138->m_field08c, true);
            }
            if (m_field242) {
                const int current = m_field138->m_field08c;
                HudSquadInputValue value(current);
                if (m_field244 != player->getSquadId()) {
                    const int requested = value.get();
                    const int team = player->getTeam();
                    if (world::playerManager->getNrOfPlayersInSquad(team, requested) > 0) {
                        const int nextRequested = value.get();
                        const int nextTeam = player->getTeam();
                        ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(nextTeam, nextRequested);
                        if (info) {
                            if (info->getIsPrivate()) {
                                eventManager->postEvent(ECHud, 12, &value, 0.0f);
                                m_field242 = false; goto commonTail;
                            }
                            const int repeated = value.get();
                            const int repeatedTeam = player->getTeam();
                            if (world::playerManager->getNrOfPlayersInSquad(repeatedTeam, repeated) == 6) {
                                eventManager->postEvent(ECHud, 11, &value, 0.0f);
                                m_field242 = false; goto commonTail;
                            }
                            if (static_cast<unsigned int>(player->getSquadId()) > 0) m_field138->leaveAndReinvite();
                            const int finalRequested = value.get();
                            m_field138->joinSquad(finalRequested);
                        }
                    }
                }
                m_field242 = false;
            }
        }
        goto commonTail;
    }
    if (getMode() == 13 || getMode() == 14) { setMode(getPreviousMode(), false); goto commonTail; }
    if (getMode() == 9 || (getMode() == 12 && !isSquadRenameActive())) {
        if (getPreviousMode() == 10) setMode(0, true);
        else setMode(getPreviousMode(), true);
        goto commonTail;
    }
    if (pressed(54)) world::playerManager->getLocalHumanPlayer();
    goto commonTail;

chatInput:
    {
        meme::BfEditData* data;
        if (isChatActive()) data = isCommanderShown() ? m_field0b4[1] : m_field0b4[0];
        else if (isSquadRenameActive() || isSquadCreateActive()) data = m_field0e0;
        else goto afterChat;
        if (processChatInput(input, 167, data)) {
            if (m_field0b0) sendSayAll();
            else if (m_field0b1) sendSayTeam();
            else if (m_field0b2) sendSaySquad();
            else if (isSquadRenameActive()) { prepareSquadRename(false); submitSquadRename(); }
            else if (isSquadCreateActive() && m_field138) {
                prepareSquadCreate(false); m_field138->finishSquadCreate();
                const int mode = getMode(); enablePointer(m_field168, mode);
            }
            std::memset(input.inputs, 0, sizeof(input.inputs));
            static_cast<io::IInputManager*>(io::inputManager)->setPlayerInputDelay(5); // data97DCF8
            if (m_field0b0 || m_field0b1 || m_field0b2) closeChat();
        }
    }
afterChat:
    if (m_field0e7) {
        if (getPreviousMode() == 15) {
            if (m_field0a8[1] && m_field0a8[1]->getCharacter(ref2System, 0)) m_field0b4[1]->clear(ref2System);
        } else if (m_field0a8[0] && m_field0a8[0]->getCharacter(ref2System, 0)) m_field0b4[0]->clear(ref2System);
        setFlagE7(false);
    }
    if (isChatActive()) { std::memset(input.inputs, 0, sizeof(input.inputs)); return true; }

commonTail:
    if (!pressed(34)) {
        if (!m_field108->isEnabled()) resetRadio(m_field108);
        else if (!m_field10c->isEnabled()) resetRadio(m_field10c);
    }
    if (!pressed(33)) {
        if (!m_field110->isEnabled()) resetRadio(m_field110);
        else if (!m_field114->isEnabled()) resetRadio(m_field114);
        else if (!m_field118->isEnabled()) resetRadio(m_field118);
    }
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    // Push-to-talk keys require both a voice client and a local player; absence skips this tail.
    if (bf2VoipClient && local) {
        const bool leaderWas = bf2VoipClient->getUsingLeaderChannel();     // +A0
        const bool leaderKey = pressed(56);                                // GIVoipUseLeaderChannel
        const bool talkKey = pressed(61);                                  // GIVoipPushToTalk
        const bool leaderNow = bf2VoipClient->updateLeaderChannel(leaderKey); // +A4
        const char* sound = nullptr;
        if (!leaderWas) {
            if (leaderNow) { disableVOHelpMessage(35); sound = "activate_voip"; }
        } else if (!leaderNow) {
            sound = "deactivate_voip";
        }
        if (sound) {
            const std::string name(sound);
            g_bfSound->playSound(name);                    // PE759DA9: g_bfSound +2C
        }
        if (bf2VoipClient->getPushToTalkEnabled()) {                       // +A8
            bool talk;
            if (local->getIsCommander()) talk = getMode() == 15 ? talkKey : false;
            else talk = talkKey;
            if (leaderNow && (local->getIsSquadLeader() || local->getIsCommander()))
                bf2VoipClient->setPushToTalk(true);                        // +B4
            else if (talk) bf2VoipClient->setPushToTalk(true);
            else bf2VoipClient->setPushToTalk(false);
        }
    }
    return true;
}
} } }
#endif
