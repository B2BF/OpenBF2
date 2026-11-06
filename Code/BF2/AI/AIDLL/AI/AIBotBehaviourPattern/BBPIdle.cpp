// Idle pattern base (AI/AIDLL/AI/AIBotBehaviourPattern/BBPIdle.cpp in the image). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address. Package D.

// static init: ELF 0x28C620 (__tcf_0 0x28C680, _GLOBAL__I 0x28C6B0): <iostream>'s Init object only.
#include <iostream>

#include "BBPIdle.h"

#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x28C1E0
BBPIdle::BBPIdle(int id)
    : m_patternId(id)
{
}

// ELF 0x28C260
BBPIdle::~BBPIdle()
{
}

// ELF 0x28C6D0
int BBPIdle::getType()
{
    return 0;
}

// ELF 0x28C2E0. Retail quirk: the primary-object lookup is not null-checked.
bool BBPIdle::freePosition(unsigned int equipment, int side)
{
    Information* info = informationManager->getInformation(equipment);
    if (!info)
        return false;
    if (!info->isPrimary())
        info = informationManager->getInformation(info->getPrimaryObject(side));

    if (!info->getUnit())
        return false;
    if (!info->getUnit()->isOccupied())
        return true;

    int iterator;
    for (Information* seat = informationManager->getInformation(info->getFirstSecondaryObject(iterator, side)); seat;
         seat = informationManager->getInformation(info->getSecondaryObject(iterator, side)))
    {
        if (seat->getUnit() && !seat->getUnit()->isOccupied())
            return true;
    }
    return false;
}

// ELF 0x28C480. Same walk as freePosition, testing Information::isBooked.
bool BBPIdle::anyBookedPositions(unsigned int equipment, int side)
{
    Information* info = informationManager->getInformation(equipment);
    if (!info)
        return false;
    if (!info->isPrimary())
        info = informationManager->getInformation(info->getPrimaryObject(side));

    if (info->isBooked())
        return true;

    int iterator;
    for (Information* seat = informationManager->getInformation(info->getFirstSecondaryObject(iterator, side)); seat;
         seat = informationManager->getInformation(info->getSecondaryObject(iterator, side)))
    {
        if (seat->isBooked())
            return true;
    }
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
