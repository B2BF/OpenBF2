// HUD chat controls. [INFERRED] Owner, name and source placement.
#include "HudChat.h"
#if !DEDICATED
#include "BfListBoxData.h"
#include "BF2HudManager.h"
#include "HudColors.h"
#include "Ref2System.h"
#include "../Core/StringUtil.h"
#include "../Core/SmartPtr.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Game/Objects/IPlayer.h"
#include <new>
namespace dice { namespace hfe { extern hud::BF2HudManager* hudManager; } }
namespace dice { namespace hfe { namespace hud {
namespace {
// 79F3A0 and its inlined7A04FD copy. Do not substitute the Object descriptor.
meme::System* castChatSystem(meme::System* system)
{
    if (!system) return nullptr;
    meme::TypeDescriptor* required = meme::System::getClassType();
    for (meme::TypeDescriptor* type = system->getType(); type; type = type->getBaseType())
        if (type == required) return system;
    return nullptr;
}
}
HudChat::HudChat(meme::System* system) : m_system(system), m_data(nullptr), m_field3bc(), m_historyOutput(true)
{
    for (meme::Color& color : m_field008) color = {1,1,1,1};
    for (auto& variant : m_colors) for (meme::Color& color : variant) color = {1,1,1,1};
    for (int i = 0; i < 9; ++i) { m_capacity[i] = 0; m_spacing[i] = 0; m_duration[i] = 5; }
}
HudChat::~HudChat() { if (m_data) delete m_data; }
bool HudChat::init(std::string name, int firstWidth, int secondWidth)
{
    void* allocation = ::operator new(sizeof(meme::BfListBoxData));
    m_data = allocation ? new(allocation) meme::BfListBoxData(name.c_str()) : nullptr;
    if (!m_data) return false;
    m_data->m_field021 = false;
    m_data->addColumn(firstWidth, false, false);
    m_data->addColumn(secondWidth, true, false);
    m_data->m_field03c = 6;
    m_data->m_field040 = false;
    rebuildRows();
    return true;
}
void HudChat::update(float delta) { advanceTimers(delta); }
void HudChat::setDuration(int kind, float value) { m_duration[kind] = value; }
void HudChat::setColor(int kind, int variant, float r, float g, float b, float a)
{ m_colors[variant][kind] = {r,g,b,a}; }
void HudChat::setCapacity(int kind, int value) { m_capacity[kind] = value; m_field3b8 = false; }
void HudChat::setSpacing(int kind, int value) { m_spacing[kind] = value; m_field3b8 = false; }
int HudChat::getRowsUsed()
{
    int result = m_spacing[8];
    for (int i = 7; i >= 0; --i) result += m_spacing[i];
    for (int i = 8; i >= 0; --i) result += m_capacity[i];
    return result;
}
std::string HudChat::getHistoryInfo()
{
    std::string result;
    result += formatString("number of lines used: %d\n", getRowsUsed());
    result += formatString("chat message history size: %d\n", m_capacity[0]);
    result += formatString("gameInfo message history size: %d\n", m_capacity[1]);
    result += formatString("kill message history size: %d\n", m_capacity[2]);
    result += formatString("personal message history size: %d\n", m_capacity[7]);
    result += formatString("request message history size: %d\n", m_capacity[8]);
    return result;
}
void HudChat::addMessage(std::string text, int kind)
{
    switch (kind) {
    case 1: addPlayerMessage(hudManager->toWideString(text), nullptr, 0); break;
    case 2: addGameMessage(hudManager->toWideString(text), nullptr, 0); break;
    case 3: addKillMessage(hudManager->toWideString(text), nullptr, 0); break;
    case 5: addNotice(hudManager->toWideString(text), 0); break;
    case 6: addRequest(hudManager->toWideString(text), 0); break;
    }
}
void HudChat::addGameMessage(std::wstring text, world::IPlayer* player, int team)
{
    const int capacity = m_capacity[1];
    m_remaining[1] = m_duration[1];
    if (!team) {
        std::wstring decorated = std::wstring(L"\xA7" L"1") + text;
        appendMessage(decorated + L"\xA7" L"0", 6, 0, m_firstRow[1], &m_count[1], capacity, &m_remaining[1], 1, nullptr);
    } else {
        selectPlayerColors(0, player, team);
        std::wstring decorated = std::wstring(L"\xA7" L"1") + text;
        appendMessage(decorated + L"\xA7" L"0", 0, team, m_firstRow[1], &m_count[1], capacity, &m_remaining[1], 1, nullptr);
        restoreColors(0);
    }
}
void HudChat::addPlayerMessage(std::wstring text, world::IPlayer* player, int)
{
    if (player && hudManager->isPlayerIgnored(player->getId())) return;
    m_remaining[0] = m_duration[0];
    const int capacity = m_capacity[0];
    std::wstring stripped, formatted;
    // Retail's name call is unconditional, even after the null-guarded ignore query.
    std::wstring prefix = hudManager->toWideString(formatString("%s: ", player->getName().c_str()));
    std::string squad("HUD_TEXT_CHAT_SQUAD"), team("HUD_TEXT_CHAT_TEAM"), dead("HUD_CHAT_DEADPREFIX");
    if (text.find(dice::hfe::toWideString(dead)) != std::wstring::npos)
        stripped = text.substr(dead.size(), text.size() - dead.size());
    else { dead.clear(); stripped = text; }
    if (player->getIsCommander()) {
        for (int variant = 0; variant < 3; ++variant)
            setColor(0, variant, commanderChatColor.r, commanderChatColor.g, commanderChatColor.b, commanderChatColor.a);
        std::string commander("HUD_TEXT_CHAT_COMMANDER");
        if (text.find(dice::hfe::toWideString(commander)) != std::wstring::npos) {
            stripped = stripped.substr(commander.size(), stripped.size() - commander.size());
            std::wstring group = hudManager->getLocalizedString(commander);
            formatted = hudManager->getLocalizedString(dead) + group + prefix + stripped;
        } else if (text.find(dice::hfe::toWideString(team)) != std::wstring::npos) {
            stripped = stripped.substr(team.size(), stripped.size() - team.size());
            std::wstring group = hudManager->getLocalizedString(team);
            formatted = hudManager->getLocalizedString(dead) + group + prefix + stripped;
        } else formatted = prefix + text; // shipped commander path ignores the stripped dead prefix
    } else if (text.find(dice::hfe::toWideString(squad)) != std::wstring::npos) {
        for (int variant = 0; variant < 3; ++variant)
            setColor(0, variant, squadChatColors[variant].r, squadChatColors[variant].g,
                squadChatColors[variant].b, squadChatColors[variant].a);
        stripped = stripped.substr(squad.size(), stripped.size() - squad.size());
        std::wstring group = hudManager->getLocalizedString(squad);
        formatted = hudManager->getLocalizedString(dead) + group + prefix + stripped;
    } else {
        restoreColors(0);
        if (text.find(dice::hfe::toWideString(team)) != std::wstring::npos) {
            stripped = stripped.substr(team.size(), stripped.size() - team.size());
            std::wstring group = hudManager->getLocalizedString(team);
            formatted = hudManager->getLocalizedString(dead) + group + prefix + stripped;
        } else formatted = hudManager->getLocalizedString(dead) + prefix + stripped;
    }
    if (!formatted.empty()) {
        const int first = m_firstRow[0]; // loaded before the player callback in79FC4D
        const int teamId = player->getTeam();
        appendMessage(formatted, 0, teamId, first, &m_count[0], capacity, &m_remaining[0], 0, nullptr);
    }
    restoreColors(0);
}
void HudChat::addKillMessage(std::wstring text, world::IPlayer* player, int team)
{
    const int capacity = m_capacity[0];
    m_remaining[0] = m_duration[0];
    selectPlayerColors(0, player, team);
    if (!text.empty()) appendMessage(text, 0, team, m_firstRow[0], &m_count[0], capacity, &m_remaining[0], 0, nullptr);
    restoreColors(0);
}
void HudChat::addRankMessage(std::wstring text, world::IPlayer* player, int, int alternate)
{
    const int kind = alternate ? 7 : 1;
    const int capacity = m_capacity[kind];
    m_remaining[kind] = m_duration[kind];
    appendMessage(text, 4, 0, m_firstRow[kind], &m_count[kind], capacity, &m_remaining[kind], kind, player);
}
void HudChat::addPersonalMessage(std::wstring text, world::IPlayer* player, int)
{
    m_remaining[7] = m_duration[7];
    appendMessage(text, 5, 0, m_firstRow[7], &m_count[7], m_capacity[7], &m_remaining[7], 7, player);
}
void HudChat::addNotice(std::wstring text, int)
{
    m_remaining[7] = m_duration[7];
    appendMessage(text, 7, 0, m_firstRow[7], &m_count[7], m_capacity[7], &m_remaining[7], 7, nullptr);
}
void HudChat::addRequest(std::wstring text, int team)
{
    const int capacity = m_capacity[8];
    m_remaining[8] = m_duration[8];
    std::wstring converted = hudManager->toWideString(getUpperCase(hudManager->toNarrowString(text)));
    if (converted.empty()) converted = text;
    appendMessage(converted, 8, team, m_firstRow[8], &m_count[8], capacity, &m_remaining[8], 8, nullptr);
}
void HudChat::selectPlayerColors(int kind, world::IPlayer* player, int)
{
    if (player && player->getIsCommander()) {
        for (int variant = 0; variant < 3; ++variant)
            setColor(kind, variant, commanderChatColor.r, commanderChatColor.g,
                commanderChatColor.b, commanderChatColor.a);
        return;
    }
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    bool useSquadColor = false;
    if (local && player && player->getSquadId() != 0) {
        const int localTeam = local->getTeam();
        if (localTeam == player->getTeam()) {
            const int localSquad = local->getSquadId();
            if (localSquad == player->getSquadId()) useSquadColor = true;
            else if (local->getIsCommander() && player->getIsSquadLeader()) useSquadColor = true;
        }
    }
    if (useSquadColor) {
        for (int variant = 0; variant < 3; ++variant)
            setColor(kind, variant, squadChatColors[variant].r, squadChatColors[variant].g,
                squadChatColors[variant].b, squadChatColors[variant].a);
    } else {
        restoreColors(kind);
    }
}
void HudChat::restoreColors(int kind)
{
    // Reload each mutable color after the preceding virtual callback.
    for (int variant = 0; variant < 3; ++variant)
        setColor(kind, variant, defaultChatColors[variant].r, defaultChatColors[variant].g,
            defaultChatColors[variant].b, defaultChatColors[variant].a);
}
void HudChat::appendMessage(std::wstring text, int kind, int team, int first,
    int* count, int capacity, float*, int, world::IPlayer* player)
{
    if (text.empty()) return;
    std::string path;
    SmartPtr<IBase> unusedResource; // 66E7D0/6ACD10, never populated in this body
    const int row = nextRow(first, count, capacity);
    switch (kind) {
    case 0: case 3:
        if (team != 1 && team != 2) break;
        m_data->setWideCell(0, row, L"", 1,1,1,1, 0,0);
        wrapMessage(kind, text, team, first, count, capacity, row);
        break;
    case 6:
        m_data->setWideCell(0, row, L"", 1,1,1,1, 0,0);
        wrapMessage(6, text, team, first, count, capacity, row);
        break;
    case 4: {
        const meme::TextureHandle icon = hudManager->getPlayerRankIcon(player);
        m_data->setImageCell(0, row, icon, 16,16,1,-1, 1,1,1,1, 0);
        std::wstring decorated = std::wstring(L"\xA7" L"1") + text;
        wrapMessage(4, decorated + L"\xA7" L"0", team, first, count, capacity, row);
        break;
    }
    case 5: {
        path = "Ingame/GeneralIcons/empty.dds";
        const meme::TextureHandle icon = m_system->loadTexture(std::string(path).c_str(), true);
        m_data->setImageCell(0, row, icon, 18,11,1,7, 1,1,1,1, 0);
        std::wstring decorated = std::wstring(L"\xA7" L"1") + text;
        wrapMessage(5, decorated + L"\xA7" L"0", team, first, count, capacity, row);
        break;
    }
    case 7: case 8: {
        m_data->setWideCell(0, row, L"", 1,1,1,1, 0,0);
        std::wstring decorated = std::wstring(L"\xA7" L"1") + text;
        wrapMessage(kind, decorated + L"\xA7" L"0", team, first, count, capacity, row);
        break;
    }
    }
}
void HudChat::wrapMessage(int kind, std::wstring text, int team, int first,
    int* count, int capacity, int row)
{
    int mode = 0;
    const int fitting = m_data->measureText(castChatSystem(hudManager->getSystem()), text, 1, row, &mode);
    int variant = team;
    if (world::playerManager->getLocalHumanPlayer() && kind != 8)
        variant = (team == world::playerManager->getLocalHumanPlayer()->getTeam()) + 1;
    const meme::Color* color = &m_colors[variant][kind];
    if (text.size() <= static_cast<unsigned int>(fitting)) {
        m_data->setWideCell0x7c87f0(1, row, text, color->r,color->g,color->b,color->a, 0,0);
        ++*count;
        return;
    }
    unsigned int offset = 0;
    int nextMode = 0, previousMode = 0;
    bool firstLine = true;
    do {
        if (offset > 0) {
            row = nextRow(first, count, capacity);
            m_data->setWideCell(0, row, L"", 1,1,1,1, 0,0);
        }
        meme::System* system = castChatSystem(hudManager->getSystem());
        unsigned int consumed = m_data->measureText(system, text.substr(offset, text.size() - offset), 1, row, &nextMode);
        // 32-bit counts as on x86; a missing space (npos) still narrows to 0xFFFFFFFF on x64.
        const unsigned int remaining = static_cast<unsigned int>(text.size() - offset);
        bool skipSpace = false;
        if (consumed >= remaining) consumed = remaining;
        else {
            const unsigned int space = static_cast<unsigned int>(
                text.substr(offset, consumed + 1).rfind(L' ', consumed));
            if (static_cast<int>(space) > 0 && consumed - space < 20) {
                consumed = space; skipSpace = true;
            }
        }
        m_data->setWideCell0x7c87f0(1, row, text.substr(offset, consumed),
            color->r,color->g,color->b,color->a, 0, firstLine ? 0 : previousMode);
        offset += consumed;
        firstLine = false; previousMode = nextMode;
        if (skipSpace) ++offset;
        ++*count;
    } while (offset < text.size()); // no zero-progress guard in the retail loop
}
int HudChat::nextRow(int first, int* count, int capacity)
{
    const int saved = *count;
    if (saved >= capacity) { dropRow(first, count); return first + saved - 1; }
    return first + saved;
}
void HudChat::resetRows() { m_data->clearRows(); rebuildRows(); }
void HudChat::dropRow(int first, int* count)
{
    std::wstring wide = m_data->getWideCell(1, first);
    std::string text = hudManager->toNarrowString(wide);
    if (!text.empty() && m_historyOutput) hudManager->outputChatMessage(text);
    const int saved = *count;
    if (saved) {
        if (saved == 1) m_data->clearRow(first);
        else for (int row = first; row < first + *count - 1; ++row) {
            std::wstring rowWide = m_data->getWideCell(1, row);
            std::string rowText = hudManager->toNarrowString(rowWide);
            if (!rowText.empty() && row != first && m_historyOutput) hudManager->outputChatMessage(rowText);
            m_data->moveRow(row + 1, row);
        }
        --*count;
    }
}
void HudChat::advanceTimers(float delta)
{
    for (int i = 0; i < 9; ++i) if (m_count[i] > 0) {
        m_remaining[i] -= delta;
        if (m_remaining[i] <= 0) { dropRow(m_firstRow[i], &m_count[i]); m_remaining[i] = 0; }
    }
}
void HudChat::rebuildRows()
{
    m_firstRow[1] = m_spacing[1];
    m_firstRow[2] = m_spacing[1] + m_spacing[2] + m_capacity[1];
    m_firstRow[0] = m_firstRow[2] + m_capacity[2] + m_spacing[0];
    m_firstRow[7] = m_firstRow[0] + m_spacing[7] + m_capacity[0];
    m_firstRow[8] = m_firstRow[7] + m_spacing[8] + m_capacity[7];
    for (int i = 0; i < 9; ++i) m_count[i] = 0;
    for (int i = 0; i < 9; ++i) m_remaining[i] = 0;
    m_field3b8 = false;
    // Inlined retail sum order is retained, including re-reading after callbacks.
    const auto total = [this]() {
        return m_spacing[7] + m_spacing[4] + m_capacity[5] + m_capacity[2]
            + m_spacing[8] + m_spacing[5] + m_capacity[6] + m_capacity[0]
            + m_spacing[6] + m_spacing[2] + m_capacity[8] + m_capacity[7]
            + m_capacity[3] + m_spacing[3] + m_spacing[1] + m_spacing[0]
            + m_capacity[4] + m_capacity[1];
    };
    for (int row = 0; row < total(); ++row) {
        m_data->appendWideCell(0, L"", 1,1,1,1, 0);
        m_data->appendWideCell(1, L"", 1,1,1,1, 0);
    }
    m_data->m_visibleRows = total();
}
void HudChat::clearMessages()
{
    for (int row = 0; row < m_data->getRowCount(); ++row) {
        std::wstring wide = m_data->getWideCell(1, row);
        std::string text = hudManager->toNarrowString(wide);
        if (!text.empty() && m_historyOutput) hudManager->outputChatMessage(text);
        m_data->clearRow(row);
    }
    for (int i = 0; i < 9; ++i) m_count[i] = 0;
    for (int i = 0; i < 9; ++i) m_remaining[i] = 0;
}
void HudChat::setHistoryOutput(bool enabled) { m_historyOutput = enabled; }
int HudChat::getCapacity(int kind) { return m_capacity[kind]; }
int HudChat::getFirstRow(int kind) { return m_firstRow[kind]; }
int* HudChat::getCount(int kind) { return &m_count[kind]; }
meme::BfListBoxData* HudChat::getData() { return m_data; }
} } }
#endif
