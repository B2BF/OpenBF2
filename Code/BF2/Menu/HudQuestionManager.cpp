// Client HUD questions and responses. [INFERRED] Names and source placement.
#include "HudQuestionManager.h"
#if !DEDICATED
#include "../Core/System.h"
#include "../Core/ClassManager.h"
#include "../Core/StringUtil.h"
#include "../Game/Common/ClientVoteManager.h"
#include "../Game/Common/GameEvent.h"
#include "../Game/IGame.h"
#include "../Game/IGameLogic.h"
#include "../Game/MapList.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Scene/Object/PlayerManager.h"
#include "BF2HudManager.h"
#include "HudSquad.h"
#include "../Game/GeneralSettings.h"   // getOutOfVoting, the +0x80 bridge (was HudQuestionSettings.h's seam)
#include <cmath>
#if !defined(_M_IX86)
#include <emmintrin.h>
#endif

namespace dice { namespace hfe {
extern IBase* squadManager;
extern ClassManager* classManager;
extern hud::BF2HudManager* hudManager;
namespace hud {
namespace {
int clampCategory(int category)
{
    if (category < 0) return 0;
    if (category > 6) return 6;
    return category;
}
void replyWithoutQuestion(bool affirmative) // PE787FF0, complete100 bytes
{
    if (game) {
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            const std::string message(affirmative ? "roger_that" : "negative");
            game->radioMessage(static_cast<TargetChannel>(1), message, player, 0, true, false);
        }
    }
}
int voteMinutes(float seconds)
{
    // PE7876C7 materializes the float product before ceil(float)->double463930;
    // the final x87 conversion truncates and preserves the caller's control word.
    const float fraction = 0.01666666753590107f;
    float minutes;
#if defined(_M_IX86)
    __asm {
        fld seconds
        fmul fraction
        fstp minutes
    }
#else
    // The product of two floats is exact in double; the float store is the only rounding.
    minutes = static_cast<float>(static_cast<double>(seconds) * fraction);
#endif
    const double rounded = std::ceil(static_cast<double>(minutes));
#if defined(_M_IX86)
    unsigned short saved, truncated;
    int result;
    __asm { fnstcw saved }
    truncated = saved | 0x0c00;
    __asm {
        fld rounded
        fldcw truncated
        fistp result
        fldcw saved
    }
    return result;
#else
    // cvttsd2si truncates like the chopped fistp, including the 0x80000000 indefinite result.
    return _mm_cvttsd_si32(_mm_set_sd(rounded));
#endif
}
}

void HudQuestionManager::displayQuestion(HudQuestion* question)
{
    if (!question) return;
    switch (question->category) {
    case 0: {
        world::IPlayer* player = world::playerManager->getPlayerFromId(question->id);
        if (!player) return;
        hudManager->refreshChatCategory(8);
        hudManager->addServerTeamChat(hudManager->replaceWideTokenFromString(
            hudManager->replaceWideInputKeys(hudManager->getLocalizedString("HUD_HUD_SQUAD_HAS_ASKED_TO_JOIN")),
            "#PLAYERNAME#", player->getName().c_str()), 3);
        hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString("HUD_HUD_SQUAD_RECIEVED_ACCEPT_REJECT")), 3);
        break;
    }
    case 1: {
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        if (!player) return;
        hudManager->refreshChatCategory(8);
        ISquadInfo* squad = static_cast<ISquadManager*>(squadManager)->getSquadInfo(player->getTeam(), question->id);
        if (!squad) return;
        hudManager->addServerTeamChat(hudManager->replaceWideTokenFromString(
            hudManager->replaceWideInputKeys(hudManager->getLocalizedString("HUD_HUD_SQUAD_INVITED_TO")),
            "#SQUADNAME#", squad->getSquadName().c_str()), 3);
        hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString("HUD_HUD_SQUAD_RECIEVED_ACCEPT_REJECT")), 3);
        break;
    }
    case 2:
        hudManager->refreshChatCategory(8);
        hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString("HUD_HUD_SQUAD_RECIEVED_NEW_ORDER_COMMANDER")), 2);
        hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString("HUD_HUD_SQUAD_RECIEVED_ACCEPT_REJECT")), 2);
        break;
    case 3: {
        if (!gameLogic) return;
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        if (!player) return;
        hudManager->refreshChatCategory(8);
        {
            const std::string team = getLowerCase(gameLogic->getTeamName(player->getTeam()));
            const std::wstring localized = hudManager->getLocalizedString(formatString("HUD_TEAMNAME_%s", team.c_str()));
            hudManager->addServerTeamChat(hudManager->replaceWideToken(
                hudManager->replaceWideInputKeys(hudManager->getLocalizedString("HUD_HUD_COMMANDER_ACCEPTED")),
                "#TEAMNAME#", localized.c_str()), 2);
        }
        hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString("HUD_HUD_SQUAD_RECIEVED_ACCEPT_REJECT")), 2);
        break;
    }
    case 4:
        hudManager->refreshChatCategory(8);
        hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString("HUD_HUD_SQUADLEADER_ACCEPTED")), 1);
        hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString("HUD_HUD_SQUAD_RECIEVED_ACCEPT_REJECT")), 1);
        break;
    case 5:
        hudManager->refreshChatCategory(8);
        hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString("HUD_TEXT_TEAMKILLED_CHOICE")), 2);
        hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString("HUD_HUD_YESNO")), 2);
        break;
    case 6: {
        // Read the user's voting preference from GeneralSettings.
        GeneralSettings* settings;
        {
            const std::string name("GeneralSettings");
            settings = static_cast<GeneralSettings*>(classManager->getSingleton(name));
        }
        if (settings && settings->getOutOfVoting()) return;                  // +0x80
        const std::string issuer = ClientVoteManager::getInstance()->getVoteIssuerName();
        const std::string target = ClientVoteManager::getInstance()->getVoteTargetName();
        const int minutes = voteMinutes(ClientVoteManager::getInstance()->m_voteTimeRemaining);
        const int voteKind = question->id;
        switch (voteKind) {
        case 1: {
            hudManager->refreshChatCategory(8);
            const MapList::Entry* entry = mapList->getEntry(ClientVoteManager::getInstance()->m_dword08);
            std::string mapName;
            if (entry) mapName = entry->name;
            const std::string key = formatString("HUD_LEVELNAME_%s", mapName.c_str());
            hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(hudManager->replaceWideTokenFromInt(
                hudManager->replaceWideToken(hudManager->replaceWideTokenFromString(
                    hudManager->getLocalizedString("HUD_HUD_KICKVOTE_MAP_START_REMOTE"), "#PLAYERNAME#", issuer.c_str()),
                    "#MAPNAME#", hudManager->getLocalizedString(key.c_str())), "#MINUTES#", minutes)), 0);
            break;
        }
        case 2:
        case 3:
            hudManager->refreshChatCategory(8);
            hudManager->addServerTeamChat(hudManager->replaceWideInputKeys(hudManager->replaceWideTokenFromInt(
                hudManager->replaceWideTokenFromString(hudManager->replaceWideTokenFromString(
                    hudManager->getLocalizedString(voteKind == 2 ? "HUD_HUD_KICKVOTE_PLAYER_START_REMOTE" : "HUD_HUD_KICKVOTE_COMMANDER_START_REMOTE"),
                    "#PLAYERNAME1#", issuer.c_str()), "#PLAYERNAME2#", target.c_str()), "#MINUTES#", minutes)), 0);
            break;
        default:
            removeQuestion(question, true);
            break;
        }
        break;
    }
    }
}

void HudQuestionManager::expireQuestion(HudQuestion* question)
{
    if (!question) return;
    const char* message;
    switch (question->category) {
    case 0: message = "HUD_TEXT_MENU_INVITE_REQUEST_TIMED_OUT"; break;
    case 1: message = "HUD_TEXT_MENU_INVITE_PROPOSAL_TIMED_OUT"; break;
    case 2: message = "HUD_TEXT_MENU_COMMANDER_ORDER_TIMED_OUT"; break;
    case 3: message = "HUD_TEXT_MENU_COMMANDER_APPLICATION_TIMED_OUT"; break;
    case 4: case 6: replyQuestion(question, false, true, true); return;
    case 5: replyQuestion(question, true, true, true); return;
    default: return;
    }
    replyQuestion(question, false, true, true);
    hudManager->addServerChat(hudManager->getLocalizedString(message), 0);
}

void HudQuestionManager::replyQuestion(HudQuestion* question, bool affirmative,
                                      bool silent, bool redisplay)
{
    if (!question) { replyWithoutQuestion(affirmative); return; }
    hudManager->refreshChatCategory(8);
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    switch (question->category) {
    case 0: {
        int id = question->id;
        world::IPlayer* remote = world::playerManager->getPlayerFromId(id);
        removeQuestion(question, redisplay);
        if (!remote || !player->getIsSquadLeader()) return;
        player->getSquadId(); // Shipped discarded query still runs before eligibility.
        if (!gameLogic || !game) return;
        const bool server = gameLogic->isServer();
        PostRemoteEvent* event = new PostRemoteEvent(ECHud, affirmative ? 19 : 18, false, &id, 4, 0.0f);
        if (server) game->sendGameEventIfConnected(remote->getId(), event);
        else game->sendGameEventIfConnected(static_cast<unsigned>(-1), event);
        if (!silent) {
            hudManager->addTeamChat(hudManager->replaceWideTokenFromString(
                hudManager->getLocalizedString(affirmative ? "HUD_HUD_SQUAD_GRANTED_TO_ACCESS_SQUAD" : "HUD_HUD_SQUAD_DENIED_TO_ACCESS_SQUAD"),
                "#PLAYERNAME#", remote->getName().c_str()), nullptr, 0);
        }
        if (affirmative) {
            const int squad = player->getSquadId();
            const int team = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->inviteToSquad(team, squad, id, true, false);
        }
        break;
    }
    case 1: {
        const int id = question->id;
        removeQuestion(question, redisplay);
        if (affirmative) {
            if (hudManager->getSquad()) hudManager->getSquad()->joinSquad(id);
        } else {
            const int playerId = player->getId();
            const int team = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->inviteToSquad(team, id, playerId, false, false);
        }
        if (!silent && game) {
            const std::string message(affirmative ? "roger_that" : "negative");
            const int leader = static_cast<ISquadManager*>(squadManager)->getSquadLeader(player->getTeam(), id);
            game->radioMessage(static_cast<TargetChannel>(14), message, player, leader, true, false);
        }
        break;
    }
    case 2: {
        removeQuestion(question, redisplay);
        int state = 0;
        if (affirmative) {
            const int squadId = player->getSquadId();
            const int team = player->getTeam();
            ISquadInfo* squad = static_cast<ISquadManager*>(squadManager)->getSquadInfo(team, squadId);
            if (squad) {
                SquadOrder* order = squad->getCommanderOrders();
                if (order) {
                    order->flag2C = true;
                    const int currentSquad = player->getSquadId();
                    const int currentTeam = player->getTeam();
                    static_cast<ISquadManager*>(squadManager)->issueSquadOrder(currentTeam, currentSquad, *order, false);
                    state = 2;
                    if (!silent && game) {
                        const std::string message("roger_that");
                        const int commander = static_cast<ISquadManager*>(squadManager)->getCommander(player->getTeam());
                        game->radioMessage(static_cast<TargetChannel>(14), message, player, commander, true, false);
                    }
                }
            }
        } else {
            state = 3;
            if (!silent && game) {
                const std::string message("negative");
                const int commander = static_cast<ISquadManager*>(squadManager)->getCommander(player->getTeam());
                game->radioMessage(static_cast<TargetChannel>(14), message, player, commander, true, false);
            }
        }
        const int squadId = player->getSquadId();
        const int team = player->getTeam();
        hudManager->sendRequestEvent(team, squadId, static_cast<RequestEvent::RequestTypes>(state));
        const int currentSquad = player->getSquadId();
        const int currentTeam = player->getTeam();
        static_cast<ISquadManager*>(squadManager)->setIsAccepted(currentTeam, currentSquad, false,
            static_cast<OrderType>(1), static_cast<SquadOrderType>(5), false);
        break;
    }
    case 3:
        removeQuestion(question, redisplay);
        if (affirmative) {
            if (player->getSquadId()) hudManager->leaveSquad();
            const int id = player->getId();
            const int team = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->acceptCommander(team, id);
        } else {
            if (!silent) hudManager->addServerChat(hudManager->getLocalizedString("HUD_HUD_DECLINED_COMMANDER"), 0);
            const int id = player->getId();
            const int team = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->declineCommander(team, id);
        }
        break;
    case 4: {
        removeQuestion(question, redisplay);
        if (!affirmative || player->getIsSquadLeader()) return;
        const int team = player->getTeam();
        const int squad = player->getSquadId();
        world::IPlayer* leader = world::playerManager->getPlayerFromId(
            static_cast<ISquadManager*>(squadManager)->getSquadLeader(team, squad));
        if (leader && leader->getIsAIPlayer()) {
            static_cast<ISquadManager*>(squadManager)->setSquadLeader(team, squad, player->getId(), false);
            hudManager->toggleSquadCommander();
        }
        break;
    }
    case 5: {
        const int id = question->id;
        removeQuestion(question, redisplay);
        if (!gameLogic) return;
        gameLogic->remoteCommand(affirmative ? 101 : 100, 0, 0, 0, 0);
        hudManager->addServerChat(hudManager->getLocalizedString(affirmative ? "HUD_TEXT_TEAMKILL_FORGIVE" :
            id == 255 ? "HUD_ERRORMESSAGE_TKPUNISHONLOCALHOST" : "HUD_TEXT_TEAMKILL_PUNISH"), 0);
        break;
    }
    case 6: {
        const int id = question->id;
        removeQuestion(question, redisplay);
        if (id && !player->getVotingStatus())
            ClientVoteManager::getInstance()->sendVote(affirmative ? Vote::VOFor : Vote::VOAgainst);
        break;
    }
    default: replyWithoutQuestion(affirmative); break;
    }
}

void HudQuestionManager::reply(bool affirmative)
{
    replyQuestion(selectQuestion(), affirmative, false, true);
}

void HudQuestionManager::replyAll(bool affirmative) // 7891F0, complete 78 bytes
{
    if (!world::playerManager->getLocalHumanPlayer()) clear();
    for (HudQuestion* question = selectQuestion(); question; question = selectQuestion())
        replyQuestion(question, affirmative, true, false);
}

void HudQuestionManager::replyCategory(int category, bool affirmative,
                                      bool silent, bool redisplay)
{
    category = clampCategory(category);
    auto& questions = m_questions[category];
    // Keep the shipped live end/empty checks and iterator advance across replies.
    // A reply may mutate this vector; no iterator repair was present in retail.
    for (auto it = questions.begin(); it != questions.end(); ++it) {
        if (questions.empty()) break;
        replyQuestion(*it, affirmative, silent, redisplay);
    }
    clearCategory(category);
}

HudQuestionManager::HudQuestionManager()
    : m_durations{20.0, 20.0, 30.0, 20.0, -1.0, 15.0, -1.0},
      m_displayDelay(0.0)
{
    clear();
}

// 7883C0 destroys the seven vector buffers in reverse order. The records are
// not deleted; preserve the shipped lifetime behavior with native STL members.
HudQuestionManager::~HudQuestionManager() = default;

void HudQuestionManager::setDuration(int category, double duration)
{
    m_durations[clampCategory(category)] = duration;
}

int HudQuestionManager::getCount(int category)
{
    return static_cast<int>(m_questions[clampCategory(category)].size());
}

HudQuestion* HudQuestionManager::selectQuestion()
{
    HudQuestion* selected = nullptr;
    const double now = WorldPref::getWorldTime();
    double youngestAge = 1000.0;
    double shortestTime = 1000.0;
    for (int category = 0; category != 7; ++category) {
        const auto& questions = m_questions[category];
        for (auto it = questions.begin(), end = questions.end(); it != end; ++it) {
            if (questions.empty()) break;
            HudQuestion* question = *it;
            if (question && question->id > -1) {
                const double age = now - question->createdAt;
                if (age < youngestAge &&
                    (question->remainingTime < shortestTime || shortestTime == -1.0)) {
                    selected = question;
                    shortestTime = question->remainingTime;
                    youngestAge = age;
                }
            }
        }
    }
    return selected;
}

void HudQuestionManager::removeQuestion(HudQuestion* question, bool redisplay)
{
    if (question) {
        // The pointed-to category is not clamped or validated in this entry.
        auto& questions = m_questions[question->category];
        int index = 0;
        for (auto it = questions.begin();
             index < static_cast<int>(questions.size()) && it != questions.end();
             ++it, ++index) {
            if (*it == question) {
                questions.erase(questions.begin() + index);
                break;
            }
        }
    }
    if (redisplay) displayQuestion(selectQuestion());
}

void HudQuestionManager::clearCategory(int category)
{
    // Retail releases the backing allocation, rather than retaining capacity.
    std::vector<HudQuestion*>().swap(m_questions[clampCategory(category)]);
}

void HudQuestionManager::clear()
{
    for (int category = 0; category < 7; ++category) clearCategory(category);
}

void HudQuestionManager::update(float elapsed)
{
    for (int category = 0; category != 7; ++category) {
        auto& questions = m_questions[category];
        for (auto it = questions.begin(); it != questions.end(); ++it) {
            if (questions.empty()) break;
            HudQuestion* question = *it;
            if (question && question->remainingTime > 0.0) {
                question->remainingTime -= elapsed;
                if (question->remainingTime <= 0.0) expireQuestion(question);
            }
        }
    }
    if (m_displayDelay > 0.0) {
        m_displayDelay -= elapsed;
        if (m_displayDelay <= 0.0) displayQuestion(selectQuestion());
    }
}

void HudQuestionManager::removeMatchingQuestions(int category, int id)
{
    auto& questions = m_questions[category];
    // Shipped iterator invalidation is intentional here: the iterator advances
    // even after erase, skipping the shifted record. Do not replace with erase's
    // returned iterator or add an end guard absent from PE78912C/78912F.
    for (auto it = questions.begin(); it != questions.end(); ++it) {
        if (questions.empty()) break;
        HudQuestion* question = *it;
        if (question && question->id == id) removeQuestion(question, false);
    }
}

void HudQuestionManager::removeQuestions(int category, int id,
                                        bool redisplay, float delay)
{
    category = clampCategory(category);
    if (!m_questions[category].empty()) {
        if (id < 0) clearCategory(category);
        else removeMatchingQuestions(category, id);
    }
    if (redisplay) {
        if (delay > 0.0f) m_displayDelay = delay;
        else displayQuestion(selectQuestion());
    }
}

void HudQuestionManager::addQuestion(int category, int id, bool redisplay)
{
    category = clampCategory(category);
    clearCategory(category);
    HudQuestion* question = new HudQuestion;
    question->category = category;
    question->id = id;
    question->createdAt = WorldPref::getWorldTime();
    question->remainingTime = m_durations[category];
    m_questions[category].push_back(question);
    if (redisplay) displayQuestion(selectQuestion());
}

} } }
#endif
