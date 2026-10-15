// HUD voice-chat status.
#include "HudVoip.h"
#if !DEDICATED
#include "BF2HudManager.h"
#include "BfListBoxData.h"
#include "HudColors.h"
#include "../Core/StringUtil.h"
#include "../Core/IBase.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Game/Objects/IPlayer.h"
#include "../Scene/Object/PlayerManager.h"
#include "../External/BFMeme/System.h"
#include <list>
#include <new>
#include <string>
namespace dice { namespace hfe {
extern IBase* squadManager;                    // Core/Globals.cpp, PE ds:99EF84
extern hud::BF2HudManager* hudManager;         // PE ds:A10890
namespace hud {
namespace {
// The row painter builds one of these per squad id, once.
const char* const squadIconFormat = "Ingame/Player/Icons/Minimap/mini_Squad_%d_small.tga";
// 97DE40, an .rdata literal: opaque yellow. The commander's icon tint and the
// colour every non-talking cell and the speaker cell use.
const meme::Color commanderColor = {1.0f, 1.0f, 0.0f, 1.0f};
}

// The three texture loads happen only when a System was supplied, and
// their store order is +30, +2C, +28 - commander, speaker, empty - the reverse
// of the declaration order.
HudVoip::HudVoip(meme::System* system)
    : m_system(system), m_field008(0), m_visibleRows(9), m_field010(0),
      m_visible(false), m_enabled(false), m_rowHeight(0),
      m_speakerColumnWidth(15), m_nameColumnWidth(30), m_field024(-1.0f)
{
    for (int i = 0; i < 10; ++i) m_squadTextures[i] = 0;
    if (m_system) {
        m_commanderTexture = m_system->loadTexture("Ingame/Kits/Icons/kit_Commander.tga", true);
        m_speakerTexture = m_system->loadTexture("Ingame/Respawn/icon_speaker.tga", true);
        m_emptyTexture = m_system->loadTexture("Ingame/GeneralIcons/empty.tga", true);
    } else {
        m_commanderTexture = 0;
        m_speakerTexture = 0;
        m_emptyTexture = 0;
    }
    m_data = nullptr; // +5C is not written by the constructor; registerData fills it.
}

// Runs base teardown. The existing implementation leaves the list data allocated.
HudVoip::~HudVoip() {}

void HudVoip::setSystem(meme::System* system) { m_system = system; }
bool HudVoip::registerFunctions() { return true; }

// The list ends up with three columns, not two: BfListBoxData's own
// constructor adds a width-0 column first, so index 0 is that hidden one, index
// 1 is the 15-wide speaker column and index 2 the 30-wide name column - which is
// exactly what the row painter writes.
// The shipped body re-reads +5C for the second addColumn and again for the
// +2C store rather than keeping the pointer in a register.
bool HudVoip::registerData()
{
    void* allocation = ::operator new(sizeof(meme::BfListBoxData));
    m_data = allocation ? new(allocation) meme::BfListBoxData("voipList") : nullptr;
    if (!m_data) return false;
    m_data->addColumn(m_speakerColumnWidth, true, false);
    m_data->addColumn(m_nameColumnWidth, true, false);
    m_data->m_visibleRows = m_visibleRows;
    return true;
}

// Three shipped behaviours are preserved:
//   - a cleared +15 returns without storing the visible flag, while every other
//     exit stores it;
//   - the list at +5C is cleared with no null test;
//   - the nine-member cap is tested inside the loop against the collection's
//     size, which does not change. So it is not "at most nine rows": a squad
//     with ten or more members paints nothing at all.
void HudVoip::update(bool visible)
{
    if (!m_enabled) return;
    if (visible) {
        world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
        if (local) {
            m_data->clearRows();
            const int team = local->getTeam();
            const int commander = static_cast<ISquadManager*>(squadManager)->getCommander(team);
            world::IPlayer* commanderPlayer = world::playerManager->getPlayerFromId(commander);
            if (commanderPlayer) paintRow(m_data, commanderPlayer);
            // Retail null-tests the collection it gets back (7A52F6). These
            // two slots are typed as returning a reference, so that test cannot
            // be written here; it is recorded instead.
            const std::list<world::IPlayer*>* members;
            if (local->getIsCommander()) {
                const int leaderTeam = local->getTeam();
                members = &world::playerManager->getTeamSquadLeaders(leaderTeam);
            } else {
                const int squad = local->getSquadId();
                const int memberTeam = local->getTeam();
                members = &world::playerManager->getPlayersInSquad(memberTeam, squad);
            }
            if (!members->empty()) {
                for (std::list<world::IPlayer*>::const_iterator it = members->begin();
                     it != members->end(); ++it)
                    if (members->size() <= 9) paintRow(m_data, *it);
            }
        }
    }
    m_visible = visible;
}

// Draws the voice-chat row in column order 0, 2, 1.
void HudVoip::paintRow(meme::BfListBoxData* list, world::IPlayer* player)
{
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (!local) return;
    if (!local->getIsAlive()) return;          // +68: the whole list stays empty while dead
    if (!player) return;
    if (player->getId() == local->getId()) return;   // +50: the local player is not painted

    bool talking = false;
    if (player->getVoipEnabled()) {
        if (!player->getIsMuted()) {
            if (player->getIsTalking()) {
                hudManager->enableVOHelpMessage(34);
                talking = true;
            }
        }
    }

    std::intptr_t icon = m_emptyTexture;
    if (!talking) {
        list->appendCell(0, std::string(), 1.0f, 1.0f, 1.0f, 1.0f, 0);
        list->appendCell(1, std::string(), 1.0f, 1.0f, 1.0f, 1.0f, 0);
        list->appendCell(2, std::string(), 1.0f, 1.0f, 1.0f, 1.0f, 0);
        return;
    }

    meme::Color nameColor;
    if (player->getIsCommander()) {
        list->appendImageCell0x7c8300(0, m_commanderTexture, 15.0f, 15.0f,
            static_cast<float>(m_rowHeight), m_field024,
            commanderColor.r, commanderColor.g, commanderColor.b, commanderColor.a);
        nameColor = commanderColor;
    } else {
        if (player->getIsSquadLeader()) {
            const int squad = player->getSquadId();
            if (!m_squadTextures[squad]) {
                // getSquadId() is asked a second time for the format argument.
                const std::string name = formatString(squadIconFormat, player->getSquadId());
                m_squadTextures[squad] = m_system->loadTexture(name.c_str(), true);
            }
            icon = m_squadTextures[squad];
        }
        const meme::Color& tint = squadChatColors[2];   // A1C758, a fixed index
        list->appendImageCell0x7c8300(0, icon, 16.0f, 16.0f,
            static_cast<float>(m_rowHeight), m_field024 - 1.0f,
            tint.r, tint.g, tint.b, tint.a);
        nameColor = squadChatColors[player->getSquadId()];
    }
    list->appendCell0x7c81c0(2, player->getName(),
        nameColor.r, nameColor.g, nameColor.b, nameColor.a, 0);
    list->appendImageCell0x7c8300(1, m_speakerTexture, 15.0f, 15.0f, 0.0f, m_field024,
        commanderColor.r, commanderColor.g, commanderColor.b, commanderColor.a);
}

} } }
#endif
