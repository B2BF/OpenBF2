// HUD chat-input routing. [INFERRED] Source split. Named send and descriptor frontiers remain at their call sites.
#include "BF2HudManager.h"
#if !DEDICATED
#include "BfEditData.h"
#include "HudInformationAdapter.h"
#include "HudCommander.h"
#include "HudSquad.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "Ref2System.h"
#include "../Core/StringUtil.h"
#include "../Core/System.h"
#include "../Game/GameServer/ServerInfo.h"
#include "../Game/Main/BF2.h"
#include "../Game/Objects/IPlayer.h"
#include "../IO/Input/GameInput.h"
#include "../Scene/Object/PlayerManager.h"
#include <cstring>
#if !defined(_M_IX86)
#include <emmintrin.h>
#endif
namespace dice { namespace hfe { extern IBase* squadManager; namespace hud {
namespace {
// Expression helper for PE7B371D..373A /7B3980..399D, not a separate retail
// method. Keep the x87 subtraction, truncation and saved rounding control.
int chatSecondsRemaining(double deadline, double now)
{
#if defined(_M_IX86)
    unsigned short original, truncating;
    int result;
    __asm {
        fld deadline
        fsub now
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
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates). cvttsd2si truncates
    // like the chopped fistp, including the 0x80000000 indefinite result.
    return _mm_cvttsd_si32(_mm_set_sd(deadline - now));
#endif
}

// Identical spam-display arms in sendSayAll7B36C4 and sendSayTeam7B3926.
void displayChatSpam(BF2HudManager* hud, world::IPlayer* player)
{
    if (game) {
        const std::string message("spam");
        game->localGameInfoMessage(static_cast<TargetChannel>(14), message);
    }
    const int duration = serverInfo->m_block.radioBlockedDuration;
    const double deadline = static_cast<double>(player->getRadioSpamFilterFlagTimer()) + duration + 1.0;
    const int remaining = chatSecondsRemaining(deadline, WorldPref::getWorldTime());
    const std::wstring message = hud->replaceWideTokenFromInt(
        hud->getLocalizedString("HUD_HUD_MENU_CHAT_SPAM_MESSAGE"), "#SEC#", remaining);
    const int team = player->getTeam();
    hud->addServerChat(message, team);
}
}

bool BF2HudManager::sendSayAll() // 7B3530, complete209 instructions
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    std::wstring message = getLocalizedString("");
    if (isCommanderShown()) getString(m_field0a8[1], m_field08c);
    else getString(m_field0a8[0], m_field08c);
    if (player && !player->getIsAlive())
        message += getLocalizedString("HUD_CHAT_DEADPREFIX");
    if (!m_field08c.empty()) {
        player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            if (player->getIsThisASpam(true)) displayChatSpam(this, player);
            else {
                message += hfe::toWideString(m_field08c);
                game->setCurrentFlags(0);
                game->setCurrentChannel(TCGlobal);
                game->setCurrentType(static_cast<ChannelMessageType>(2));
                {
                    const std::string narrow = makeNarrow(message);
                    game->say(narrow);
                }
                player = world::playerManager->getLocalHumanPlayer();
                if (player) {
                    const int team = player->getTeam();
                    addGlobalChat(message, player, team);
                }
            }
        }
    }
    return true;
}

bool BF2HudManager::sendSayTeam() // 7B37C0, complete190 instructions
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    std::wstring message(L"");
    if (isCommanderShown()) {
        getString(m_field0a8[1], m_field08c);
        message = L"HUD_TEXT_CHAT_COMMANDER";
    } else {
        getString(m_field0a8[0], m_field08c);
        message = L"HUD_TEXT_CHAT_TEAM";
    }
    // These team-channel prefixes are literal keys in retail, unlike sendSayAll.
    if (player && !player->getIsAlive()) message += L"HUD_CHAT_DEADPREFIX";
    if (!m_field08c.empty()) {
        player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            if (player->getIsThisASpam(true)) displayChatSpam(this, player);
            else {
                message += hfe::toWideString(m_field08c);
                game->setCurrentFlags(0);
                game->setCurrentChannel(TCTeam);
                game->setCurrentType(static_cast<ChannelMessageType>(2));
                {
                    const std::string narrow = makeNarrow(message);
                    game->say(narrow);
                }
                const int team = player->getTeam();
                addGlobalChat(message, player, team); // Retail uses +11C here as well.
            }
        }
    }
    return true;
}

bool BF2HudManager::sendSaySquad()
{
    std::wstring message(L"");
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (isCommanderShown()) {
        game->setCurrentChannel(static_cast<TargetChannel>(5));
        game->setCurrentType(static_cast<ChannelMessageType>(2));
        getString(m_field0a8[1], m_field08c);
        message = L"HUD_TEXT_CHAT_COMMANDER";
    } else {
        game->setCurrentChannel(static_cast<TargetChannel>(3));
        game->setCurrentType(static_cast<ChannelMessageType>(2));
        getString(m_field0a8[0], m_field08c);
        message = L"HUD_TEXT_CHAT_SQUAD";
    }
    if (player && !player->getIsAlive()) message += L"HUD_CHAT_DEADPREFIX";
    if (!m_field08c.empty()) {
        player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            if (player->getIsThisASpam(true)) displayChatSpam(this, player);
            else {
                message += hfe::toWideString(m_field08c);
                if (game->getCurrentChannel() == static_cast<TargetChannel>(5)) {
                    for (int squad = 1; squad < 10; ++squad) {
                        if (m_field13c && m_field13c->isSquadSelected(squad)) {
                            game->setCurrentFlags(squad);
                            const std::string narrow = makeNarrow(message);
                            game->say(narrow);
                        }
                    }
                } else {
                    const std::string narrow = makeNarrow(message);
                    game->say(narrow);
                }
                const int team = player->getTeam();
                addGlobalChat(message, player, team);
            }
        }
    }
    return true;
}

void BF2HudManager::toggleSayAll(bool alternateInput)
{
    if (m_field0b0 && m_field081) {
        closeChat();
        return;
    }
    closeChat();
    m_field104->setBool("SayAllShow", true);
    m_field0b4[alternateInput ? 1 : 0]->setFocus(true);
    m_field0b0 = true;
    m_field081 = true;
}
void BF2HudManager::toggleSayTeam(bool alternateInput)
{
    if (m_field0b1 && m_field081) {
        closeChat();
        return;
    }
    closeChat();
    m_field104->setBool("SayTeamShow", true);
    m_field0b4[alternateInput ? 1 : 0]->setFocus(true);
    m_field0b1 = true;
    m_field081 = true;
}
void BF2HudManager::toggleSaySquad(bool alternateInput)
{
    if (m_field0b2 && m_field081) {
        closeChat();
        return;
    }
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player && (player->getSquadId() != 0 || alternateInput)) {
        closeChat();
        m_field104->setBool("SaySquadShow", true);
        m_field0b4[alternateInput ? 1 : 0]->setFocus(true);
        m_field0b2 = true;
        m_field081 = true;
    }
}
bool BF2HudManager::closeChat()
{
    m_field104->setBool("SayAllShow", false);
    m_field104->setBool("SayTeamShow", false);
    m_field104->setBool("SaySquadShow", false);
    m_field104->setBool("RenameSquadShow", false);
    m_field0b4[0]->clear(ref2System);
    m_field0b4[1]->clear(ref2System);
    m_field0e0->clear(ref2System);
    m_field0b0 = false;
    m_field0b1 = false;
    m_field0b2 = false;
    m_field0e5 = false;
    m_field0e4 = false;
    m_field0b4[0]->setFocus(false);
    m_field0b4[1]->setFocus(false);
    m_field0e0->setFocus(false);
    m_field081 = false;
    return true;
}
bool BF2HudManager::processChatInput(io::GameInput& input, int ignoredCharacter,
                                   meme::BfEditData* edit)
{
    if (!edit) return false;
    const unsigned long long flags = input.mappingFlags;
    if ((flags & 0x20) && input.inputs[io::GILeft] > 0.5f)
        edit->queueKey(5);
    else if ((flags & 0x40) && input.inputs[io::GIRight] > 0.5f)
        edit->queueKey(7);
    bool enter = false;
    for (;;) {
        int read = consoleCharRead;
        if (consoleCharCount == 0 && read == consoleCharWrite) break;
        if (read > 255) read = 0; // signed one-sided bound, not a masked index
        const unsigned char character = static_cast<unsigned char>(consoleCharBuffer[read]);
        --consoleCharCount;
        consoleCharRead = read + 1;
        if (consoleCharCount < 0) consoleCharCount = 0;
        if (!character) break; // consumed NUL ends this drain, even with queued bytes left

        // Both conversions precede the ignored-character and range filters.
        // Preserve the client432010 helper and the temporary's destruction order.
        std::wstring wide = hfe::toWideString(formatString("%c", static_cast<int>(character)));
        std::string narrow = makeNarrow(wide);
        if (character != ignoredCharacter &&
            (m_field0bc || (((character >= 32 && character <= 127) ||
                            character == 8 || character == 13) &&
                           narrow != "no_conversion_exists"))) {
            if (character == 8) {
                if (m_field0e6) {
                    setSquadEditText("");
                    m_field0e6 = false;
                }
                edit->queueKey(15);
            } else if (character != 9) {
                if (character == 13) enter = true;
                else {
                    if (m_field0e6) {
                        setSquadEditText("");
                        m_field0e6 = false;
                    }
                    edit->queueCharacter(character);
                }
            }
        }
    }
    return enter;
}
void BF2HudManager::setSquadEditText(std::string text)
{
    m_field0e0->clear(ref2System);
    m_field0dc->setString(ref2System, text.c_str());
    const int length = static_cast<int>(std::strlen(text.c_str()));
    m_field0e0->setCursorPosition(length); // reload after Data callbacks; no node guard
}
std::string BF2HudManager::getSquadEditText()
{
    getString(m_field0dc, m_field0c0);
    return m_field0c0;
}
void BF2HudManager::submitSquadCreate()
{
    createSquad(m_field0c0);
    closeChat();
    m_field0c0 = "";
}
void BF2HudManager::prepareSquadCreate(bool clear)
{
    if (clear) {
        m_field0e0->clear(ref2System);
        m_field0dc->setString(ref2System, "");
        return;
    }
    if (isSquadCreateActive()) getSquadEditText(); // observable discarded string copy
    m_field0e0->setFocus(false);
    setSquadCreateActive(false);
}
// Selects the squad-name edit control and discards console characters queued before the selection.
void BF2HudManager::selectSquadCreateName()
{
    setMode(13, true);                           // +1DC
    m_field0e0->setFocus(true);                  // +E0, 7AADB0
    setSquadCreateActive(true);                  // +278, byte E4
    setFlagE7(true);                             // +268, byte E7
    m_field2fc = true;                           // +2FC
    consoleCharCount = 0;                        // ds:9FFC90
    consoleCharRead = consoleCharWrite;          // ds:9FFC94 <- ds:9FFC98
}
void BF2HudManager::prepareSquadRename(bool active)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (active) {
        setSquadEditText("");
        setMode(14, true);
        if (player) {
            const int squad = player->getSquadId();
            const int team = player->getTeam();
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(team, squad);
            if (info) setSquadEditText(info->getSquadName());
        }
    } else if (isSquadRenameActive()) {
        getSquadEditText(); // Retail discards the returned copy before closing.
        closeChat();
    }
    m_field0e0->setFocus(active);
    setSquadRenameActive(active);
    m_field138->m_renameVisible = active;
}
} } }
#endif
