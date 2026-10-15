// Commander HUD controls. [INFERRED] Owner, name and source placement.
#include "HudCommander.h"
#if !DEDICATED
#include "BF2HudManager.h"
#include "Bf2MenuMapNode.h"
#include "BfListBoxData.h"
#include "HudColors.h"                                  // Squad-row palettes.
#include "../External/BFMeme/System.h"
#include "../Core/EventManager.h"
#include "../Core/StringUtil.h"                         // formatString (4310D0)
#include "../Core/System.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/IGame.h"
#include "../Game/IGameLogic.h"
#include "../Game/GameEvents/VoipPlayerMuteEvent.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"  // getSquadInfo +28, getSquadLeader +3C
#include "../Game/Objects/IObject.h"                    // getArmorComponent (+3C)
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/GameLogicComp/IArmorGLComp.h" // hit points +3C / +84
#include "../Scene/Object/PlayerManager.h"
#include <algorithm>
#include <cstdio>     // appendLeaderVoipCells [FRONTIER] diagnostic.
#include <cstdlib>
#include <cstring>
#include <new>
namespace dice { namespace hfe {
extern IBase* squadManager;                             // ds:99EF84
namespace hud {
void HudCommander::setArtilleryReloading(bool active)
{
    if (active) {
        const double now = WorldPref::getWorldTime();
        const float duration = m_artilleryDuration;
        m_artilleryReloading = true;
        m_artilleryDeadline = static_cast<float>(now + duration);
        m_artilleryRemaining = duration;
    } else {
        m_artilleryReloading = false;
        m_artilleryRemaining = 0;
    }
}
void HudCommander::setSuppliesReloading(bool active)
{
    if (active) {
        const double now = WorldPref::getWorldTime();
        const float duration = m_supplyDuration;
        m_supplyReloading = true;
        m_supplyDeadline = static_cast<float>(now + duration);
        m_supplyRemaining = duration;
    } else {
        m_supplyReloading = false;
        m_supplyRemaining = 0;
    }
}
void HudCommander::setVehicleDropReloading(bool active)
{
    if (active) {
        const double now = WorldPref::getWorldTime();
        const float duration = m_vehicleDropDuration;
        m_vehicleDropReloading = true;
        m_vehicleDropDeadline = static_cast<float>(now + duration);
        m_vehicleDropRemaining = duration;
    } else {
        m_vehicleDropReloading = false;
        m_vehicleDropRemaining = 0;
    }
}
namespace {
// Expression shared by the retail cooldown arms. The fraction uses the x87
// subtraction before its separate float store, not the rounded remaining field.
void updateCooldown(float deadline, double now, float duration, float& remaining, float& fraction)
{
#if defined(_M_IX86)
    float* remainingValue = &remaining;
    float* fractionValue = &fraction;
    __asm {
        fld deadline
        fsub now
        mov eax, remainingValue
        fst dword ptr [eax]
        fdiv duration
        mov eax, fractionValue
        fstp dword ptr [eax]
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // The fraction divides the unrounded difference, not the stored remaining value.
    const double left = static_cast<double>(deadline) - now;
    remaining = static_cast<float>(left);
    fraction = static_cast<float>(left / duration);
#endif
}
}
HudCommander::HudCommander(meme::System* system)
    : m_system(system), m_artilleryState(false), m_noArtillery(false),
      m_noSatellite(false), m_noVehicleDrop(false), m_noUAV(false),
      m_uavArmor(1), m_satelliteArmor(1), m_artilleryArmor(1), m_previousArmor(1),
      m_warningActive(false), m_warningPending(false), m_warningTime(0), m_warningInterval(30),
      m_artilleryDuration(60), m_artilleryFraction(0), m_artilleryRemaining(0),
      m_artilleryReloading(false), m_artilleryDeadline(0), m_supplyState(false),
      m_supplyRemaining(0), m_supplyFraction(0), m_supplyDuration(60), m_supplyReloading(false),
      m_supplyDeadline(0), m_vehicleDropReloading(false), m_vehicleDropDuration(1),
      m_vehicleDropFraction(0), m_vehicleDropState(false), m_vehicleDropDeadline(0),
      m_vehicleDropRemaining(0), m_vehiclesAvailable(1), m_blinkInterval(0.5f), m_lastBlink(0),
      m_blink(false), m_radioShown(false), m_radioSquad(-1), m_talkingLeaders(0), m_rowCount(0),
      m_visibleRows(10), m_sortByScore(false), m_expandedSquad(-1), m_lastSelectedSquad(-1),
      m_team(2), m_field0b4(-1), m_field0b8(23), m_field0bc(4), m_field0c0(0),
      m_colors{1,1,1,1,1,1,1,1}
{
    eventManager->registerEventHandler(ECHud, this, 0);
    const int positions[] = {13,38,63,88,113,145,168,174,194,214,220};
    std::copy(positions, positions + 11, m_columnPositions);
    m_slotColumns = 5;
    m_selectedSquads.reset();
    // These texture names include voice indicators even when the optional VoIP
    // client is omitted. Loading their images is part of this retail UI owner.
    const char* names[] = {
        "Ingame/Commander/squadFrame.tga", "Ingame/Commander/squadEmptyBox.tga",
        "Ingame/Kits/Icons/kit_Squad_big.tga", "Ingame/Commander/squadmemberHealtbar.tga",
        "Ingame/Respawn/iconframe.tga", "Ingame/Scoreboard/Icons/checkbox_voipMuteGreyCommander.tga",
        "Ingame/Scoreboard/Icons/checkbox_voipMuteCommander.tga", "Ingame/Scoreboard/Icons/checkbox_voipTalkCommander.tga",
        "Ingame/Scoreboard/Icons/checkbox_voipActiveCommander.tga", "Ingame/GeneralIcons/empty.tga",
        "Ingame/GeneralIcons/full.tga", "Ingame/Commander/icon_request.tga",
        "Ingame/Commander/icon_deny.tga", "Ingame/Commander/icon_accept.tga"};
    const int slots[] = {0,1,2,12,3,4,5,6,7,8,13,9,10,11};
    if (m_system) {
        for (int i = 0; i < 14; ++i) m_textures[slots[i]] = m_system->loadTexture(names[i], true);
    } else {
        for (int i = 0; i < 14; ++i) m_textures[slots[i]] = 0;
    }
}
HudCommander::~HudCommander() { eventManager->unregisterEventHandler(ECHud, this); }
void HudCommander::setSystem(meme::System* system) { m_system = system; }
bool HudCommander::registerData()
{
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("ArtilleryState", &m_artilleryState);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("CommanderNoArtilleryShow", &m_noArtillery);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("ArtilleryArmor", &m_artilleryArmor);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("CommanderNoSatelliteShow", &m_noSatellite);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("SatelliteArmor", &m_satelliteArmor);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("CommanderNoUAVShow", &m_noUAV);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("UAVArmor", &m_uavArmor);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("ArtilleryTimeUntilReloaded", &m_artilleryFraction);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("ArtilleryReloading", &m_artilleryReloading);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("CommanderRadioShow", &m_radioShown);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("VehiclesAvailable", &m_vehiclesAvailable);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("CommanderNoVehicleDropShow", &m_noVehicleDrop);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("SupplyTimeUntilReloaded", &m_supplyFraction);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("SupplyState", &m_supplyState);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("SupplyReloading", &m_supplyReloading);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("VehicleDropTimeUntilReloaded", &m_vehicleDropFraction);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("VehicleDropState", &m_vehicleDropState);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("VehicleDropReloading", &m_vehicleDropReloading);
    void* allocation = ::operator new(sizeof(meme::BfListBoxData));
    m_data = allocation ? new (allocation) meme::BfListBoxData("SquadList") : nullptr;
    if (!m_data) return false;
    m_data->addIconColumn(m_columnPositions[0], 23, false);
    for (int i = 0; i < 4; ++i) m_data->addColumn(m_columnPositions[0], true, false);
    for (int position = 1; position <= 2; ++position) {
        m_data->addColumn(m_columnPositions[position], false, false);
        for (int i = 0; i < 4; ++i) m_data->addColumn(m_columnPositions[position], true, false);
    }
    m_data->addColumn(m_columnPositions[3], false, false);
    for (int i = 0; i < 3; ++i) m_data->addColumn(m_columnPositions[3], true, false);
    m_data->addColumn(m_columnPositions[3], false, false);
    for (int position = 4; position <= 5; ++position) {
        for (int i = 0; i < 4; ++i) m_data->addColumn(m_columnPositions[position], true, false);
        m_data->addColumn(m_columnPositions[position], false, false);
    }
    m_data->addColumn(m_columnPositions[6], true, false);
    for (int position = 7; position <= 8; ++position)
        for (int i = 0; i < 3; ++i) m_data->addColumn(m_columnPositions[position], true, false);
    m_data->addColumn(m_columnPositions[9], true, false);
    m_data->addColumn(m_columnPositions[10], true, false);
    m_data->addColumn(m_columnPositions[10], true, false);
    m_data->addIconColumn(m_columnPositions[10], 20, false);
    m_data->addHiddenColumn();
    m_data->addHiddenColumn();
    m_data->m_visibleRows = m_visibleRows;
    m_data->m_field021 = false;
    m_data->m_field023 = false;
    return true;
}
bool HudCommander::registerFunctions() { return true; }
void HudCommander::handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data)
{ if (category == ECHud) handleHudEvent(id, node, data); }
void HudCommander::handleHudEvent(unsigned int id, EventNode*, void* data)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (id == 8) {
        m_artilleryDeadline = static_cast<float>(WorldPref::getWorldTime() + m_artilleryDuration);
        m_artilleryRemaining = m_artilleryDuration;
        m_artilleryReloading = true;
        const double supplyDeadline = WorldPref::getWorldTime() + m_supplyDuration;
        m_supplyRemaining = m_supplyDuration;
        m_supplyDeadline = static_cast<float>(supplyDeadline);
        m_supplyReloading = true;
        const double dropDeadline = WorldPref::getWorldTime() + m_vehicleDropDuration;
        m_vehicleDropReloading = true;
        m_vehicleDropDeadline = static_cast<float>(dropDeadline);
        m_vehicleDropRemaining = m_vehicleDropDuration;
    } else if (id == 59) {
        const bool muted = *static_cast<unsigned char*>(data) != 0;
        int playerId;
        std::memcpy(&playerId, static_cast<unsigned char*>(data) + 4, sizeof(playerId));
        if (player->getId() != playerId) {
            if (muted) m_mutedPlayers.push_back(playerId);
            else if (!m_mutedPlayers.empty()) {
                auto it = std::find(m_mutedPlayers.begin(), m_mutedPlayers.end(), playerId);
                if (it != m_mutedPlayers.end()) m_mutedPlayers.erase(it);
            }
        }
    }
}
void HudCommander::update(bool refreshRows)
{
    if (refreshRows) {
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            m_team = player->getTeam();
            rebuildSquadRows(m_team);
            if (m_selectedSquads.count() == 0) setSquadSelected(1, true);
        }
    }
    if (WorldPref::getWorldTime() - m_lastBlink > m_blinkInterval) {
        m_blink = !m_blink;
        m_lastBlink = static_cast<float>(WorldPref::getWorldTime());
    }
}
// Rebuilds commander squad rows for squads one through nine, sorted by score or rank.
// Uses the player manager's borrowed scratch list.
void HudCommander::rebuildSquadRows(int team)
{
    m_data->clearRows();                                                          // 7C89B0, no null test
    m_rowCount = 0;                                                               // +94
    m_talkingLeaders = 0;                                                         // +90
    for (int squad = 1; squad < 10; ++squad)
        appendSquadRow(m_sortByScore                                              // +A0
                ? world::playerManager->getPlayersInSquadSortedByScore(team, squad)   // PE +0x58
                : world::playerManager->getPlayersInSquadSortedByRank(team, squad),   // PE +0x5C
            team, squad);
}

// Appends a squad row. The row entries must be non-null.
void HudCommander::appendSquadRow(const std::list<world::IPlayer*>& players, int team,
                                  int squad)
{
    int slot = 4;
    if (players.size() > 0)                                                       // [list+8], jbe
        for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
            if (!(*it)->getIsSquadLeader())                                       // PE +0x138
                appendMemberCells(*it, slot--, squad);
    for (; slot >= 0; --slot)
        appendMemberCells(nullptr, slot, squad);
    appendMemberCells(world::playerManager->getPlayerFromId(                      // PE +0x20
            static_cast<ISquadManager*>(squadManager)->getSquadLeader(team, squad)),   // +0x3C
        5, squad);
    m_data->appendCell(31, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
    appendLeaderVoipCells(squad);
    appendRequestCells(squad);
    m_data->appendCell(38, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
    appendSquadNumberCells(squad);
    appendSquadFrameCells(squad);
    const bool empty = world::playerManager->getNrOfPlayersInSquad(m_team, squad) <= 0;  // PE +0x9C
    m_data->appendCell(43, formatString("%i", static_cast<int>(empty)), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    ++m_rowCount;                                                                 // +94
}

// Appends the squad member cells.
void HudCommander::appendMemberCells(world::IPlayer* player, int slot, int squad)
{
    const int sel = isSquadSelected(squad);                                       // +0x30
    meme::Color text = commanderIconColors[sel];                                  // A1C6B8, copied now
    meme::TextureHandle frame = m_textures[3];                                    // +FC iconframe
    if (player && world::playerManager->getNrOfPlayersInSquad(m_team, squad) > 0) {   // PE +0x9C
        meme::TextureHandle icon = hudManager->getPlayerVehicleIcon(player, true); // +0x60
        if (!icon) {
            icon = hudManager->getPlayerKitIcon(player, false, false);            // +0x64
            if (!icon)
                icon = m_textures[8];                                             // +110 empty.tga
        }
        if (!player->getIsAlive())                                                // PE +0x68
            frame = m_textures[8];
        else if (player->getId() == m_expandedSquad)                              // PE +0x50 vs +A8
            text = mapWhiteColors[m_team];
        float health = 0.0f;
        if (world::IObject* const vehicle = player->getVehicle())                 // PE +0x44
            if (world::IArmorGLComp* const armor =
                    static_cast<world::IArmorGLComp*>(vehicle->getArmorComponent())) {   // +3C member
                const float hitPoints = armor->getHitPoints();                    // +0x3C
                health = hitPoints / armor->getMaxHitPoints();                    // +0x84, no zero test
            }
        float r, g, b, a;
        if (isSquadSelected(squad)) {                                             // asked again
            a = 1.0f;
            b = 0.0f;
            if (health > 0.5f) {                                                  // NaN -> the else arm
                g = 0.615686297416687f;
                r = (health - 0.5f) * 2.0f - 0.7058823704719543f;                 // ds:9324B8, 180/255
            } else {
                r = 0.7490196228027344f;
                g = 1.0f - ((0.5f - health) * 2.0f - 0.7764706015586853f);        // ds:9324B4, 198/255
            }
        } else {
            const meme::Color& fill = commanderFrameColors[1];                    // A1C808, fixed [1]
            r = fill.r;
            g = fill.g;
            b = fill.b;
            a = fill.a;
        }
        const meme::Color& cell = commanderCellColors[sel];
        m_data->appendImageCell0x7c8300(m_slotColumns * slot + 1, m_textures[13],  // +124 full.tga
            15.0f, 15.0f, 0.0f, m_field0bc, cell.r, cell.g, cell.b, cell.a);
        const meme::Color& frameColor = commanderFrameColors[sel];
        m_data->appendImageCell0x7c8300(m_slotColumns * slot + 2, frame, 15.0f, 15.0f, 0.0f,
            m_field0bc, frameColor.r, frameColor.g, frameColor.b, frameColor.a);
        m_data->appendImageCell0x7c8300(m_slotColumns * slot + 3, icon, 15.0f, 15.0f, 0.0f,
            m_field0bc, text.r, text.g, text.b, text.a);
        m_data->appendImageCell0x7c8300(m_slotColumns * slot + 4, m_textures[12],  // +120 healthbar
            2.0f, 15.0f, 16.0f, m_field0bc, cell.r, cell.g, cell.b, cell.a);
        const float barHeight = health * 15.0f;                                   // stored at 793510
        m_data->appendImageCell(m_slotColumns * slot + 5, m_textures[12],          // 7C8260 itself
            2.0f, barHeight, 16.0f, 15.0f - barHeight + m_field0bc, r, g, b, a);
        return;
    }
    if (world::playerManager->getNrOfPlayersInSquad(m_team, squad) > 0) {         // asked again
        const meme::Color& cell = commanderCellColors[sel];
        m_data->appendImageCell0x7c8300(m_slotColumns * slot + 1, m_textures[13],
            15.0f, 15.0f, 0.0f, m_field0bc, cell.r, cell.g, cell.b, cell.a);
        m_data->appendImageCell0x7c8300(m_slotColumns * slot + 2, frame,          // cell palette
            15.0f, 15.0f, 0.0f, m_field0bc, cell.r, cell.g, cell.b, cell.a);
        m_data->appendCell(m_slotColumns * slot + 3, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_data->appendImageCell0x7c8300(m_slotColumns * slot + 4, m_textures[12],
            2.0f, 15.0f, 16.0f, m_field0bc, cell.r, cell.g, cell.b, cell.a);
        m_data->appendCell(m_slotColumns * slot + 5, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        return;
    }
    for (int column = 1; column <= 5; ++column)
        m_data->appendCell(m_slotColumns * slot + column, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
}

// Appends the squad leader's voice-chat indicators.
// [RETIRE-WHEN: client-voip-optional] Complete and validate the corresponding HudCommander operation.
void HudCommander::appendLeaderVoipCells(int squad)
{
    const int sel = isSquadSelected(squad);                                       // +0x30
    const std::list<world::IPlayer*>& players =
        world::playerManager->getPlayersInSquad(m_team, squad);                   // PE +0x54
    world::IPlayer* leader = nullptr;
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
        if (*it && (*it)->getIsSquadLeader()) {                                   // PE +0x138
            leader = *it;
            break;
        }
    if (!leader || !leader->getVoipEnabled()) {                                   // PE +0x268
        m_data->appendCell(32, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_data->appendCell(33, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_data->appendCell(34, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        return;
    }
    bool mutedLocally = false;                                                    // ECHud 59's list
    if (!m_mutedPlayers.empty())
        for (std::list<int>::const_iterator it = m_mutedPlayers.begin(); it != m_mutedPlayers.end(); ++it)
            if (*it == leader->getId()) {                                         // PE +0x50, per entry
                mutedLocally = true;
                break;
            }
    meme::TextureHandle icon;
    if (mutedLocally)
        icon = m_textures[4];                                                    // +100 voipMuteGreyCommander
    else if (leader->getIsMuted())                                                // PE +0x274
        icon = m_textures[5];                                                     // +104 voipMuteCommander
    else if (leader->getIsTalking()) {                                            // PE +0x278
        ++m_talkingLeaders;
        hudManager->enableVOHelpMessage(35);                                      // +F8
        if (m_talkingLeaders > 1)
            hudManager->enableVOHelpMessage(36);
        icon = m_textures[6];                                                     // +108 voipTalkCommander
    } else
        icon = m_textures[7];                                                     // +10C voipActiveCommander
    const meme::Color& cell = commanderCellColors[sel];
    m_data->appendImageCell0x7c8300(32, m_textures[13], 15.0f, 15.0f, 0.0f, m_field0bc,
        cell.r, cell.g, cell.b, cell.a);
    if (!bf2VoipClient) {                                                         // see the banner
        std::fprintf(stderr, "[FRONTIER] dice::hfe::hud::HudCommander::appendLeaderVoipCells "
                             "reached with no voice client (BF2.exe 79410A reads bf2VoipClient "
                             "+0xA0 unguarded; client-voip-optional, RW-94)\n");
        std::fflush(stderr);
        std::abort();
    }
    // ds:97DE40, a never-written .data word: opaque yellow while the leader channel is in use.
    static const meme::Color kLeaderChannelColor = {1.0f, 1.0f, 0.0f, 1.0f};
    const meme::Color& frameColor = bf2VoipClient->getUsingLeaderChannel()       // retail +0xA0
        ? kLeaderChannelColor : commanderFrameColors[sel];
    m_data->appendImageCell0x7c8300(33, m_textures[3], 15.0f, 15.0f, 0.0f, m_field0bc,
        frameColor.r, frameColor.g, frameColor.b, frameColor.a);
    const meme::Color& tint = commanderIconColors[sel];
    m_data->appendImageCell0x7c8300(34, icon, 15.0f, 15.0f, 0.0f, m_field0bc,
        tint.r, tint.g, tint.b, tint.a);
}

// Appends the pending request indicators.
void HudCommander::appendRequestCells(int squad)
{
    meme::TextureHandle icon = m_textures[8];                                     // +110 empty.tga
    const int sel = isSquadSelected(squad);
    static_cast<ISquadManager*>(squadManager)->getSquadInfo(m_team, squad);       // +0x28, dropped
    if (world::playerManager->getNrOfPlayersInSquad(m_team, squad) == 0) {
        m_data->appendCell(35, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_data->appendCell(36, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_data->appendCell(37, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        return;
    }
    const meme::Color& cell = commanderCellColors[sel];
    m_data->appendImageCell0x7c8300(35, m_textures[13], 15.0f, 15.0f, 0.0f, m_field0bc,
        cell.r, cell.g, cell.b, cell.a);
    const meme::Color& frame = commanderFrameColors[sel];
    m_data->appendImageCell0x7c8300(36, m_textures[3], 15.0f, 15.0f, 0.0f, m_field0bc,
        frame.r, frame.g, frame.b, frame.a);
    int type = 0;
    if (hudManager->getMap()) {                                                   // +250
        type = hudManager->getMap()->getSquadRequestType(squad);                  // +250, map +F4
        if (type == 1) {
            hudManager->enableVOHelpMessage(38);
            if (m_blink)                                                          // +88
                icon = m_textures[9];                                             // +114 icon_request
        } else if (type == 3) {
            icon = m_textures[10];                                                // +118 icon_deny
        } else if (type == 2) {
            icon = m_textures[11];                                                // +11C icon_accept
        }
    }
    const meme::Color& tint = mapSquadRequestColors[type][sel];                   // A1CB68 + 16*(sel + 2*type)
    m_data->appendImageCell0x7c8300(37, icon, 15.0f, 15.0f, 0.0f, m_field0bc,
        tint.r, tint.g, tint.b, tint.a);
}

// 792E90 (500 B). Columns 39..41, then the row highlight.
// An empty squad is deselected here, while painting, through the real setSquadSelected (+0x28).
//   That can drop the radio mode.
// The highlight is stamped before columns 42, 0 and 43 exist for this row, so those three cells
//   are never highlighted.
// An unselected row's highlight is commanderHighlightColors[0], which is all zero.
void HudCommander::appendSquadNumberCells(int squad)
{
    const int sel = isSquadSelected(squad);
    if (world::playerManager->getNrOfPlayersInSquad(m_team, squad) == 0) {
        setSquadSelected(squad, false);                                           // +0x28
        m_data->appendCell(39, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_data->appendCell(40, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_data->appendCell(41, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
    } else {
        const meme::Color& icon = commanderSquadIconColors[sel];
        m_data->appendImageCell0x7c8300(39, m_textures[2], 15.0f, 15.0f, 0.0f, m_field0bc,   // kit_Squad_big
            icon.r, icon.g, icon.b, icon.a);
        m_data->appendCell(40, "", 1.0f, 1.0f, 1.0f, 1.0f, 0);
        const meme::Color& number = commanderNumberColors[sel];
        m_data->appendPositionedCell0x7c8210(41, formatString("%i", squad), 0.0f, m_field0c0,
            number.r, number.g, number.b, number.a, 1);
    }
    const meme::Color& highlight = commanderHighlightColors[sel];
    m_data->setRowHighlight0x7c4ba0(m_rowCount, highlight.r, highlight.g, highlight.b, highlight.a,
        0.0f, m_field0b4, m_field0b8);
}

// 793090 (365 B). Column 42 (the hidden squad number), then column 0.
// A populated squad gets squadFrame.tga (239x23). An empty squad gets squadEmptyBox.tga (23x23,
// white), and its caption is rewritten into the row's existing column-1 cell through 7C8640.
// isSquadSelected is asked at each use, not cached.
void HudCommander::appendSquadFrameCells(int squad)
{
    m_data->appendCell(42, formatString("%i", squad), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    if (world::playerManager->getNrOfPlayersInSquad(m_team, squad) > 0) {
        const meme::Color& frame = commanderRowFrameColors[isSquadSelected(squad)];
        m_data->appendImageCell0x7c8300(0, m_textures[0], 239.0f, 23.0f, 0.0f, 0.0f,  // squadFrame
            frame.r, frame.g, frame.b, frame.a);
        return;
    }
    m_data->appendImageCell0x7c8300(0, m_textures[1], 23.0f, 23.0f, 0.0f, 0.0f,  // squadEmptyBox
        1.0f, 1.0f, 1.0f, 1.0f);
    const meme::Color& number = commanderNumberColors[isSquadSelected(squad)];
    // ds:9324AC is A7 31 25 69 00. The literal is split so that "\xA7" "1" is not read as one escape.
    m_data->setPositionedCell0x7c8640(1, m_rowCount, formatString("\xA7" "1%i", squad),
        -static_cast<float>(m_columnPositions[0]), m_field0c0,                   // fild +C4, fchs
        number.r, number.g, number.b, number.a, 1, 0);
}
void HudCommander::refreshAbilities() // 793D20, exact order
{ refreshArtillery(); refreshSupplies(); refreshUAV(); refreshSatellite(); refreshVehicleDrop(); }
void HudCommander::refreshArtillery()
{
    m_artilleryState = hudManager->getMode() == 22;
    if (hudManager->getArtilleryEnabled() && hudManager->getArtilleryAvailability() > 0) {
        m_noArtillery = false;
        m_artilleryArmor = hudManager->getArtilleryAvailability();
        if (m_artilleryArmor < 1) hudManager->enableVOHelpMessage(53);
        if (m_artilleryArmor < m_previousArmor) {
            if (m_artilleryArmor == 0 && !m_warningPending) {
                m_warningPending = true;
            } else if (!m_warningActive) {
                {
                    const std::string message("COMMSL_ARTILLERY_artillerycondred");
                    game->localRadioMessage(TCTeam, message);
                }
                m_warningActive = true;
                m_warningTime = WorldPref::getWorldTime();
            }
        } else if (m_artilleryArmor > m_previousArmor) {
            m_warningActive = false;
            m_warningPending = false;
        }
        if (WorldPref::getWorldTime() - m_warningTime > m_warningInterval) m_warningActive = false;
        m_previousArmor = m_artilleryArmor;
        if (m_artilleryRemaining > 0 && m_artilleryReloading) {
            const float deadline = m_artilleryDeadline;
            const double now = WorldPref::getWorldTime();
            updateCooldown(deadline, now, m_artilleryDuration, m_artilleryRemaining, m_artilleryFraction);
        } else {
            m_artilleryReloading = false;
            m_artilleryFraction = 0;
        }
    } else {
        m_artilleryDeadline = static_cast<float>(WorldPref::getWorldTime() + m_artilleryDuration);
        m_artilleryRemaining = m_artilleryDuration;
        m_artilleryReloading = true;
        m_noArtillery = true;
    }
}
void HudCommander::refreshSupplies()
{
    m_supplyState = hudManager->getMode() == 24;
    if (m_supplyRemaining > 0 && m_supplyReloading) {
        const float deadline = m_supplyDeadline;
        const double now = WorldPref::getWorldTime();
        updateCooldown(deadline, now, m_supplyDuration, m_supplyRemaining, m_supplyFraction);
    } else {
        m_supplyReloading = false;
        m_supplyFraction = 0;
    }
}
void HudCommander::refreshSatellite()
{
    if (hudManager->getSatelliteEnabled()) {
        m_noSatellite = false;
        m_satelliteArmor = hudManager->getSatelliteAvailability();
        if (m_satelliteArmor < 1) hudManager->enableVOHelpMessage(52);
        if (m_satelliteArmor > 0) return;
    }
    if (hudManager->getMap()) hudManager->getMap()->resetSatelliteReload();
    m_noSatellite = true;
}
void HudCommander::refreshUAV()
{
    if (hudManager->getUAVEnabled()) {
        m_uavArmor = hudManager->getUAVAvailability();
        if (m_uavArmor < 1) hudManager->enableVOHelpMessage(51);
        // The shipped parity branch also treats an unordered value as available.
        if (!(m_uavArmor <= 0)) {
            m_noUAV = false;
        } else if (!m_noUAV) {
            m_noUAV = true;
            if (hudManager->getMap()) hudManager->getMap()->resetUAVReload();
        }
    }
}
void HudCommander::refreshVehicleDrop()
{
    m_vehicleDropState = hudManager->getMode() == 25;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    // PE+14 is the returned native string's length, not a team-info field.
    const bool noVehicle = gameLogic->getTeamDropVehicle(player->getTeam()).empty();
    m_vehiclesAvailable = hudManager->getVehicleDropAvailability();
    if (m_vehiclesAvailable <= 0 || noVehicle) {
        m_vehicleDropDeadline = static_cast<float>(WorldPref::getWorldTime() + m_vehicleDropDuration);
        m_vehicleDropRemaining = m_vehicleDropDuration;
        m_vehicleDropReloading = true;
        if (m_noVehicleDrop) return; // Retail leaves the previous fraction untouched.
        m_noVehicleDrop = true;
    } else {
        m_noVehicleDrop = false;
    }
    if (m_vehicleDropRemaining > 0 && m_vehicleDropReloading) {
        const float deadline = m_vehicleDropDeadline;
        const double now = WorldPref::getWorldTime();
        updateCooldown(deadline, now, m_vehicleDropDuration, m_vehicleDropRemaining, m_vehicleDropFraction);
    } else {
        m_vehicleDropReloading = false;
        m_vehicleDropFraction = 0;
    }
}
int HudCommander::getSelectedRowFlag()
{
    return std::atoi(m_data->getSelectedString(43).c_str());
}
int HudCommander::getSelectedSquad()
{
    if (std::atoi(m_data->getSelectedString(43).c_str())) return -1;
    return std::atoi(m_data->getSelectedString(42).c_str());
}
void HudCommander::selectRadioSquad(int squad)
{
    if (!isSquadSelected(squad)) {
        selectAllSquads(false);
        setSquadSelected(squad, true);
    }
    hudManager->setMode(16, true);
    m_radioSquad = squad;
}
void HudCommander::singleClick()
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    const int squad = getSelectedSquad();
    if (hudManager->getMode() == 16 || !local || !hudManager->isCommanderShown()
        || hudManager->isScoreboardMode()) {
        if (hudManager->getMode() == 16) hudManager->setMode(15, true);
        return;
    }
    if (getSelectedRowFlag()) return;

    if (m_data->m_column == 34) {
        const std::list<world::IPlayer*>& players =
            world::playerManager->getPlayersInSquad(m_team, squad);
        // Retail retains the last visited player when no squad leader is
        // found; only an empty list (or a trailing null) leaves this null.
        world::IPlayer* leader = nullptr;
        for (world::IPlayer* player : players) {
            leader = player;
            if (leader && leader->getIsSquadLeader()) break;
        }
        if (!leader || !leader->getVoipEnabled()) return;

        // Retail walks this list and repeatedly reloads the leader id, but
        // does not use the iterator it finds before consulting Player state.
        if (m_mutedPlayers.size()) {
            for (std::list<int>::const_iterator it = m_mutedPlayers.begin();
                 it != m_mutedPlayers.end(); ++it) {
                if (*it == leader->getId()) break;
            }
        }

        const bool muted = !leader->getIsMuted();
        leader->setIsMuted(muted);
        void* const storage = ::operator new(sizeof(VoipPlayerMuteEvent));
        VoipPlayerMuteEvent* const event = storage
            ? new(storage) VoipPlayerMuteEvent(muted, local->getId(), leader->getId())
            : nullptr;
        game->sendGameEvent(static_cast<unsigned int>(-1), event);
        if (muted) hudManager->disableVOHelpMessage(36);
        return;
    }

    if (hudManager->getFlag229()) toggleSquad(squad);
    else if (hudManager->getFlag22A()) selectSquadRange(squad, -1);
    else selectSingleSquad(squad);
    hudManager->disableVOHelpMessage(25);
}
void HudCommander::rightClick()
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    if (local && hudManager->isCommanderShown() && !hudManager->isScoreboardMode()) {
        const int squad = getSelectedSquad();
        if (getSelectedRowFlag()) {
            deselect();
        } else if (world::playerManager->getNrOfPlayersInSquad(m_team, squad) > 0) {
            selectRadioSquad(squad);
        }
    }
}
void HudCommander::doubleClick() {} // 83D270, actual shared RET-only body
void HudCommander::deselect()
{
    if (hudManager->getMode() == 16) hudManager->setMode(15, true);
}
void HudCommander::sendRadioMessage(std::string message, TargetChannel channel)
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    if (!local) return;
    bool first = true;
    for (int squad = 0; squad < 10; ++squad) {
        if (isSquadSelected(squad)) {
            game->radioMessage(channel, message, local, squad, first, false);
            first = false;
        }
    }
    m_radioSquad = -1;
    if (hudManager->getMode() == 16) hudManager->setMode(15, true);
}
void HudCommander::selectSingleSquad(unsigned int squad)
{
    if (world::playerManager->getNrOfPlayersInSquad(m_team, squad)) {
        const bool selected = m_selectedSquads.test(squad);
        m_selectedSquads.reset();
        setSquadSelected(squad, selected);
        setSquadSelected(squad, !selected);
        m_lastSelectedSquad = isSquadSelected(squad) ? squad : -1;
    }
}
void HudCommander::toggleSquad(unsigned int squad)
{
    setSquadSelected(squad, !m_selectedSquads.test(squad));
    m_lastSelectedSquad = isSquadSelected(squad) ? squad : -1;
}
void HudCommander::toggleExpandedSquad(int squad)
{ m_expandedSquad = m_expandedSquad == squad ? -1 : squad; }
void HudCommander::selectAllSquads(bool selected)
{ if (selected) m_selectedSquads.set(); else m_selectedSquads.reset(); }
void HudCommander::setSquadSelected(int squad, bool selected)
{
    squad = squad < 0 ? 0 : squad > 9 ? 9 : squad;
    if (selected && world::playerManager->getNrOfPlayersInSquad(m_team, squad) > 0) {
        m_selectedSquads.set(squad);
    } else {
        if (m_radioSquad == squad && hudManager->getMode() == 16) {
            m_radioSquad = -1;
            hudManager->setMode(15, true);
        }
        if (m_selectedSquads.count() > 1) m_selectedSquads.reset(squad);
    }
}
void HudCommander::selectSquadRange(int first, int last)
{
    if (last == -1) last = m_lastSelectedSquad;
    if (first > 0 && last > 0) {
        if (first > last) {
            for (int i = last; i <= first; ++i) setSquadSelected(i, true);
        } else {
            for (int i = first; i <= last; ++i) setSquadSelected(i, true);
        }
    }
    m_lastSelectedSquad = isSquadSelected(last) ? last : -1;
}
bool HudCommander::isSquadSelected(int squad)
{
    if (squad == 0) return false;
    squad = squad < 0 ? 0 : squad > 9 ? 9 : squad;
    return m_selectedSquads.test(squad);
}
int HudCommander::getLastSelectedSquad() { return m_lastSelectedSquad; }
int HudCommander::getExpandedSquad() { return m_expandedSquad; }
} } }
#endif
