// [INFERRED] path/name; see VehicleOccupancy.h. Full instructions read from
// libaidll.so and AIDLL_w32ded.dll .
#include "VehicleOccupancy.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x36CE90, __tcf_0 0x36CEF0, _GLOBAL__I 0x36CF20).
#include <iostream>
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"

namespace dice { namespace hfe { namespace ai {

bool vehicleOccupied(unsigned int handle, int side)
{
#if defined(_MSC_VER)
    // [PLATFORM] PE refuses a zero low word before loading/dereferencing the manager.
    Information* primary = (handle & 0xFFFFu) ? informationManager->getInformation(handle) : nullptr;
#else
    // ELF fetches the manager's pool even for a zero low word.
    Information* primary = informationManager->getInformation(handle);
#endif
    if (!primary)
        return false;

    if (!primary->isPrimary())
    {
#if defined(_MSC_VER)
        const unsigned int primaryHandle = primary->getPrimaryObject(side);
        primary = (primaryHandle & 0xFFFFu) ? informationManager->getInformation(primaryHandle) : nullptr;
#else
        // [PLATFORM] ELF retains the manager across each owner-forwarding callback.
        InformationManager* const manager = informationManager;
        const unsigned int primaryHandle = primary->getPrimaryObject(side);
        primary = manager->getInformation(primaryHandle);
#endif
    }

    // The resolved primary deliberately has no null guard .
    IPIUnit* unit = primary->getUnit();
    if (!unit)
        return false;
    if (unit->isOccupied())
        return true;

    int iterator; // The first-secondary callback initializes this output, as shipped.
#if defined(_MSC_VER)
    const unsigned int firstHandle = primary->getFirstSecondaryObject(iterator, side);
    InformationManager* const firstManager = informationManager;
    // PE's out-of-line lookup loads the manager before entry, but tests
    // the low word before dereferencing it. Do not use the unguarded manager wrapper here.
    Information* secondary = (firstHandle & 0xFFFFu) ? firstManager->getInformation(firstHandle) : nullptr;
#else
    InformationManager* const firstManager = informationManager;
    const unsigned int firstHandle = primary->getFirstSecondaryObject(iterator, side);
    Information* secondary = firstManager->getInformation(firstHandle);
#endif
    while (secondary)
    {
        unit = secondary->getUnit();
        if (unit && unit->isOccupied())
            return true;
#if defined(_MSC_VER)
        const unsigned int nextHandle = primary->getSecondaryObject(iterator, side);
        secondary = (nextHandle & 0xFFFFu) ? informationManager->getInformation(nextHandle) : nullptr;
#else
        InformationManager* const manager = informationManager;
        const unsigned int nextHandle = primary->getSecondaryObject(iterator, side);
        secondary = manager->getInformation(nextHandle);
#endif
    }
    return false;
}

// ELF 0x36CAB0: a primary occupation/booking decides immediately; otherwise inspect the
// secondary objects until a squad match. The resolved primary is deliberately unchecked.
int vehicleOccupiedBySquadMember(unsigned int handle, SquadMember* member, int side)
{
    int result = -1;
    SquadManager* const squads = SquadManager::m_instance;
    Information* const original = informationManager->getInformation(handle);
    if (!original)
        return result;
    Information* primary = original;
    if (!original->isPrimary())
    {
        InformationManager* const manager = informationManager;
        const unsigned int primaryHandle = original->getPrimaryObject(side);
        primary = manager->getInformation(primaryHandle);
    }
    IPIUnit* unit = primary->getUnit();
    if (!unit)
        return result;
    if (unit->isOccupied())
    {
        const unsigned int occupierHandle = unit->getOccupier(side);
        Information* const occupier = informationManager->getInformation(occupierHandle);
        if (occupier && occupier->getSide() == side)
        {
            const int playerId = unit->getOccupyingPlayerId(side);
            SquadMember* const occupyingMember = squads->getSquadMember(playerId, side);
            if (!occupyingMember)
                return 0;
            const int ownPlayerId = member->getPlayerId();
            if (ownPlayerId != occupyingMember->getPlayerId())
            {
                const int ownSquadId = member->getSquadId();
                return ownSquadId == occupyingMember->getSquadId() ? 1 : 0;
            }
        }
    }
    else if (original->isBooked())
    {
        const int playerId = original->getBookingPlayerId();
        SquadMember* const bookingMember = squads->getSquadMember(playerId, side);
        if (bookingMember)
        {
            const int ownSquadId = member->getSquadId();
            return ownSquadId == bookingMember->getSquadId() ? 1 : 0;
        }
    }

    int iterator;
    InformationManager* manager = informationManager;
    const unsigned int firstHandle = primary->getFirstSecondaryObject(iterator, side);
    Information* secondary = manager->getInformation(firstHandle);
    while (secondary && result != 1)
    {
        unit = secondary->getUnit();
        if (unit && unit->isOccupied())
        {
            const unsigned int occupierHandle = unit->getOccupier(side);
            Information* const occupier = informationManager->getInformation(occupierHandle);
            if (occupier && occupier->getSide() == side)
            {
                const int playerId = unit->getOccupyingPlayerId(side);
                SquadMember* const occupyingMember = squads->getSquadMember(playerId, side);
                if (!occupyingMember)
                    result = 0;
                else
                {
                    const int ownPlayerId = member->getPlayerId();
                    if (ownPlayerId != occupyingMember->getPlayerId())
                    {
                        const int ownSquadId = member->getSquadId();
                        result = ownSquadId == occupyingMember->getSquadId() ? 1 : 0;
                    }
                }
            }
        }
        else if (secondary->isBooked())
        {
            const int playerId = secondary->getBookingPlayerId();
            SquadMember* const bookingMember = squads->getSquadMember(playerId, side);
            if (bookingMember)
            {
                const int ownSquadId = member->getSquadId();
                result = ownSquadId == bookingMember->getSquadId() ? 1 : 0;
            }
        }
        manager = informationManager;
        const unsigned int nextHandle = primary->getSecondaryObject(iterator, side);
        secondary = manager->getInformation(nextHandle);
    }
    return result;
}

} } }
