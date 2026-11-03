// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BFPathfindingCourier operations.

// static init: ELF 0x2E8670 (__tcf_0 0x2E86D0, _GLOBAL__I 0x2E8700): <iostream>'s Init object only.
#include <iostream>

#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"
#include "BFPathfindingCourier.h"

#include <cfloat>
#include <list>
#include <string>

#include "../../../../Core/ClassManager.h"
#include "CommonSingletonServer.h"
#include "AIDllFrontier.h"
#include "IAIPathfinding.h"            // getVehicleNumberFromName's one forward
#include "AISettingsInterfaces.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"      // IAISettings::getVehicle - findClosestValidInfantryPolygon

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// IAIPathfindingCourier
IAIPathfindingCourier::IAIPathfindingCourier()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!CommonSingletonServer::m_iaiPathfindingCourier)
        classManager->registerSimpleSingleton("IAIPathfindingCourier", this);
}

IAIPathfindingCourier::~IAIPathfindingCourier()
{
    if (CommonSingletonServer::m_iaiPathfindingCourier == this)
        classManager->unregisterSimpleSingleton("IAIPathfindingCourier");
}

// Stateless pathfinding courier; construction requires only the base-class initialization.
BFPathfindingCourier::BFPathfindingCourier()
{
}

// The vptr reset and the base call, which again is what C++ emits.
BFPathfindingCourier::~BFPathfindingCourier()
{
}

// ELF 0x2E8200. A "vehicle" here is a pathfinding map.
int BFPathfindingCourier::getNumberOfVehicles() const
{
    return IAIPathfinding::instance->getNumberOfMaps();
}

// ELF 0x2E8230.
bool BFPathfindingCourier::isVehicleUsed(unsigned int map) const
{
    return IAIPathfinding::instance->isMapUsed(static_cast<int>(map));
}

// ELF 0x2E8270. Ignores materials.
bool BFPathfindingCourier::isValidPosition(int map, int vehicleId, const Vec3& position) const
{
    return IAIPathfinding::instance->isValidPosition(map, vehicleId, position, true);
}

// ELF 0x2E82C0. On the trace map, as vehicle 1, ignoring materials.
bool BFPathfindingCourier::isValidPosition(const Vec3& position) const
{
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    return pathfinding->isValidPosition(pathfinding->getTraceMapIndex(), 1, position, true);
}

namespace {

// [TEMP] [RETIRE-WHEN: ai-bot-squad-leader-closest] Neither callee is declared yet (request to
// ai-bot in coord/ai-requests.md); each stops by name until it is.
SquadMember* squadLeaderClosestTo(SquadManager* manager, const Vec3& position)
{
    return manager->getSquadLeaderClosestTo(position);
}

int sideOf(SquadMember* member)
{
    return member->getSide();
}

} // anonymous namespace

// ELF 0x2E8320. The squad leader closest to `position` (no null test, as in retail): the last
// valid infantry position of its vehicle's information for its side when the vehicle, that
// information, its unit and mobile plug-ins exist and the position's polygon is valid.
// Otherwise the eligible spawn point of its side closest (strictly, squared) to the leader's
// position; (-4000, -4000, -4000) when the leader has no position or no spawn point qualifies.
Vec3 BFPathfindingCourier::getLastValidSquadleaderPosition(const Vec3& position) const
{
    SquadMember* const leader = squadLeaderClosestTo(SquadManager::m_instance, position);
    const int side = sideOf(leader);

    IAIObject* const vehicle = leader->getPlayer()->getCurrentVehicle();
    if (vehicle != 0)
    {
        Information* const information =
            informationManager->getInformation(vehicle->getInformation(side));
        if (information != 0 && information->getUnit() != 0 && information->getMobile() != 0)
        {
            const IPIMobile::ValidPos& valid = information->getMobile()->getLastValidInfantryPos();
            if (valid.polygon != 0xFFFFFFFFu)
                return valid.position;
        }
    }

    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    const std::list<int>& spawnPoints = game->getSpawnPointList(side);
    float bestDistance = FLT_MAX;
    std::list<int>::const_iterator best = spawnPoints.end();
    Vec3 leaderPosition;
    leaderPosition.x = 0.0f;
    leaderPosition.y = 0.0f;
    leaderPosition.z = 0.0f;
    if (leader->getPosition(leaderPosition))
    {
        for (std::list<int>::const_iterator i = spawnPoints.begin(); i != spawnPoints.end(); ++i)
        {
            if (!game->isEligibleSpawnpoint(side, *i))
                continue;
            const Vec3& spawn = game->getSpawnPointPosition(*i);
            const float dx = leaderPosition.x - spawn.x;
            const float dy = leaderPosition.y - spawn.y;
            const float dz = leaderPosition.z - spawn.z;
            const float distance = dx * dx + dy * dy + dz * dz;
            if (bestDistance > distance)
            {
                bestDistance = distance;
                best = i;
            }
        }
        if (best != spawnPoints.end())
            return game->getSpawnPointPosition(*best);
    }

    Vec3 nowhere;
    nowhere.x = -4000.0f;
    nowhere.y = -4000.0f;
    nowhere.z = -4000.0f;
    return nowhere;
}

// Returns the case-insensitive pathfinding map id, or -1 for an unknown name.
// Requires IAIPathfinding::instance to exist.
int BFPathfindingCourier::getVehicleNumberFromName(const std::string& name) const
{
    return IAIPathfinding::instance->getMapIdFromName(name);
}

// Finds the nearest valid infantry polygon using the vehicle settings for Infantery.
bool BFPathfindingCourier::findClosestValidInfantryPolygon(Vec3& closest, const Vec3& from,
                                                           const Vec3& to, unsigned int limit) const
{
    const int vehicle = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
                            ->getVehicle(std::string("Infantery"));
    unsigned int quadTree;
    unsigned int polygon;
    return IAIPathfinding::instance->findClosestValidPolygon(
        quadTree, polygon, closest, IAIPathfinding::instance->getTraceMapIndex(), from, to, limit,
        vehicle);
}

} // namespace ai
} // namespace hfe
} // namespace dice
