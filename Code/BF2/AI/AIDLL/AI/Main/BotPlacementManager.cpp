// Registered bot placements and the retail console spawn operation.

// static init: ELF 0x3E4B60, __tcf_0 0x3E4BF0, _GLOBAL__I 0x3E4C20: stream Init precedes the manager.
#include <iostream>

#include "BotPlacementManager.h"

#include <sstream>

#include "../../../../Core/Debug.h"
#include "../AIBot/BotMain.h"
#include "IBotManager.h"

namespace dice {
namespace hfe {
namespace ai {

// static init: ELF 0x3E4B60 (_GLOBAL__I 0x3E4C20): `operator new(8)`, the constructor, then
// the store to m_instance - the manager exists from the moment the library is loaded. BF2AI::clear
// deletes it (ELF 0x3EE1D0, the last delete there); a fresh library load makes the next one.
BotPlacementManager* BotPlacementManager::m_instance = new BotPlacementManager();

// ELF 0x3E2D30: constructs the two empty team lists and nothing else; it does not touch
// m_instance.
BotPlacementManager::BotPlacementManager()
{
}

// ELF 0x3E2EF0 (D1): clears m_instance only when it names this object, then the two lists are
// destroyed in reverse order, each destroying its placements (three strings apiece).
BotPlacementManager::~BotPlacementManager()
{
    if (m_instance == this)
        m_instance = 0;
}

// ELF 0x3E2F60
bool BotPlacementManager::addPlacement(const std::string& name, const Vec3& position,
                                      const Vec3& rotation, const std::string& string1C,
                                      int team, const std::string& string20)
{
    BotPlacement placement = {};
    placement.m_name = name;
    placement.m_position = position;
    placement.m_rotation = rotation;
    placement.m_string1C = string1C;
    placement.m_string20 = string20;
    m_placements[team - 1].insert(m_placements[team - 1].end(), placement);
    return true;
}

// ELF 0x3E30E0
bool BotPlacementManager::spawnBot(int team)
{
    std::stringstream command;
    std::stringstream count;
    std::string name;
    BotPlacement placement = {};

    Debug(DebugTypeError, "AI/AIDLL/AI/Main/BotPlacementManager.cpp", 90, "")
        << "This does not work!";

    if (m_placements[team - 1].empty())
        return false;

    const std::list<BotPlacement>::iterator it = m_placements[team - 1].begin();
    name = "FIXME";
    command << it->m_name << " " << it->m_position << " " << it->m_rotation << " "
            << it->m_string1C << " " << team << " ";
    command << name << " ";
    command << it->m_string20;

    std::stringstream position;
    std::stringstream rotation;
    std::stringstream side;
    position << it->m_position;
    rotation << it->m_rotation;
    side << team;
    placement = *it;
    m_placements[team - 1].erase(m_placements[team - 1].begin());

    count << static_cast<unsigned int>(IBotManager::instance->getBotCount());
    Bot* const bot = IBotManager::instance->getLastCreatedBot();
    if (bot)
    {
        // shipped defect: none of the command streams is executed, and the placeholder name
        // is applied to the already existing last-created bot.
        bot->setName(name);
        return true;
    }
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
