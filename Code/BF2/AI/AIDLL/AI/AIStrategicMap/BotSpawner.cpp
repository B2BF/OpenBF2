#include "BotSpawner.h"
#include <cfloat>
#include <cmath>
// <iostream>'s ios_base::Init object, m_kitWeightVector and m_pointWeightVector, in that order, are
// this TU's static initialisation (ELF 0x35ED80, _GLOBAL__I 0x35EF60) and teardown (__tcf_0
// 0x35EE60, __tcf_1 0x35EE90, __tcf_2 0x35EEF0).
#include <iostream>
#include "../Main/AIDllFrontier.h"
#include "../Main/AIMain.h"
#include "../Main/AIRandom.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadManager.h"
#include "../Main/IAIPathfinding.h"
#include "../AIKnowledge/AIInformationGrid.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIUtils/VehicleOccupancy.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"     // getInformation(side)
#include "../../../AIInterface/BFKnowledge/KitTemplateManager.h"
#include "../../../AIInterface/BFKnowledge/KitTemplate.h"

namespace dice { namespace hfe { namespace ai {

std::vector<KitWeight> BotSpawner::m_kitWeightVector;
std::vector<SpawnChoice> BotSpawner::m_pointWeightVector;

namespace {
// ELF table : one bool(Information*) const slot
// no virtual destructor. Construction in the selector stores side and zero
// float accumulator; the accumulator is reset before each radius query.
class FreeUnitPred : public IAIInformationGridPredicate
{
public:
    explicit FreeUnitPred(int side) : m_side(side), m_strength(0.0f) {}
    // Guards apply only
    // to the root. Secondary records deliberately get no repeated unit/mobile
    // flag checks; their typed forwarding and occupancy calls retain stops.
    bool includeInformation(Information* information) const override
    {
        if (!information->getUnit() || !information->hasMobile())
            return false;
        if (information->isOfType(ITNaval) || information->isOfType(ITSoldier))
            return false;

        bool anyFree = false;
        float strength = 0.0f;
        Information* current = information;
        int iterator = information->getFirstSecondaryObjectIterator();
        while (current)
        {
            if (!current->getUnit()->isOccupied())
            {
                anyFree = true;
                // Reload after occupancy, then store to float on every entry.
                strength = static_cast<float>(static_cast<double>(strength) + current->getStrength());
            }
#if defined(_MSC_VER)
            // [PLATFORM] PE checks the low handle word before fetching the
            // current manager, after getSecondaryObject's callback.
            const unsigned int handle = information->getSecondaryObject(iterator, m_side);
            current = (handle & 0xFFFFu) ? informationManager->getInformation(handle) : 0;
#else
            // ELF captures the manager before the secondary-object callback
            // and performs its pool lookup even for a zero low handle word.
            InformationManager* const manager = informationManager;
            const unsigned int handle = information->getSecondaryObject(iterator, m_side);
            current = manager->getInformation(handle);
#endif
        }
        if (anyFree)
            m_strength = static_cast<float>(static_cast<double>(strength) + m_strength);
        return anyFree;
    }
    int m_side;
    mutable float m_strength;
};
}

// The primary/strategic/predicate callers and radius wrappers are recovered;
// information forwarding, path and kit policies retain stops. Side/game/SAI
// are captured at the shipped callback boundaries. The two spawn-point getters and improving position queries are
// deliberately repeated. The discarded kit-template query is observable too.
bool BotSpawner::findSuitableSpawnGroup(float* enemyStrengths, float* enemyCounts,
    float* friendlyStrengths, float* friendlyCounts, int strategicObjectId, Bot* bot)
{
    AIStrategicObject* object = 0;
    int group = 0;
    Vec3 position = { 0.0f, 0.0f, 0.0f };
    const int side = bot->getSide();
    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    if (bot->getPlayer()->getSpawnGroup() > 0)
        return true;

    static_cast<IKitTemplateManager*>(CommonSingletonServer::m_iKitTemplateManager)->getAllTemplates();
    SAI* const sai = IAIMain::m_instance->getSAI(side);
    // Compare the full unsigned random draw scaled to double; nextFloat would round too early.
    const double random = static_cast<double>(AIRandom::m_instance->next()) * (1.0 / 4294967296.0);
    if (random > 0.8f)
    {
        object = AIStrategicObjectManager::getInstance()->getStrategicObject(strategicObjectId);
        if (game->isEligibleSpawnpoint(side, bot->getSpawnPoint()))
        {
            sai->addSpawnToPoint(bot->getSpawnPoint());
            return true;
        }
    }

    const std::list<int>& groups = game->getSpawnPointList(side);
    if (groups.empty())
    {
        bot->setSpawnTarget(-1, 13);
        return false;
    }

    if (object)
    {
        const Vec2& target = object->getPosition(side);
        float best = 1.0e15f; // paired float bits
        bool found = false;
        for (std::list<int>::const_iterator it = groups.begin(); it != groups.end(); ++it)
        {
            if (game->isEligibleSpawnpoint(side, *it))
            {
                const Vec3& candidate = game->getSpawnPointPosition(*it);
                const Vec2 xz = { candidate.x, candidate.z };
                // x87 retains the differences/squares through the comparison.
                // Keep them wider than float, then store only an improving best.
                // The borrowed target is read after the position callback.
                const double dx = static_cast<double>(xz.x) - target.x;
                const double dz = static_cast<double>(xz.y) - target.y;
                const double distance = dx * dx + dz * dz;
                if (distance < best)
                {
                    best = static_cast<float>(distance);
                    group = *it;
                    found = true;
                    position = game->getSpawnPointPosition(*it);
                }
            }
        }
        if (!found)
        {
            bot->setSpawnTarget(-1, 13);
            return false;
        }
        sai->addSpawnToPoint(group);
        object = AIStrategicObjectManager::getInstance()->getClosestObject(position, side);
    }
    else
        group = findSuitableSpawnGroup(side, object, bot);

    const int kit = findSuitableKit(enemyStrengths, enemyCounts, friendlyStrengths,
        friendlyCounts, object, side, bot);
    if (object)
        bot->setSpawnTarget(object->getIndex(), group, kit);
    else
        bot->setSpawnTarget(group, kit);
    return true;
}

// ELF 0x35D1B0. Spawn selection for a bot that must come back near a position (PatrolPath::botIsDead
// is the only caller and discards the result): the eligible spawn point nearest in XZ, then the
// strategic object closest to that point's position. The strength rows come from the bot's SAI by
// bot id. The game singleton is read twice, as shipped: once for the eligibility and empty-list
// checks, again for the search. Returns true unless the side has no spawn points; a bot that
// already has a spawn group, or whose own spawn point is still eligible, is left as it is.
bool BotSpawner::findSuitableSpawnGroup(const Vec3& position, Bot* bot)
{
    Vec3 spawnPosition = { 0.0f, 0.0f, 0.0f };
    const int side = bot->getSide();
    SAI* const sai = IAIMain::m_instance->getSAI(side);
    const int botId = bot->getBotId();
    float* const enemyStrengths = sai->getEnemyStrengths(botId);
    float* const enemyCounts = sai->getEnemyTypes(botId);
    float* const friendlyStrengths = sai->getFriendlyStrengths(botId);
    float* const friendlyCounts = sai->getFriendlyTypes(botId);
    if (bot->getPlayer()->getSpawnGroup() > 0)
        return true;

    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    if (game->isEligibleSpawnpoint(side, bot->getSpawnPoint()))
    {
        sai->addSpawnToPoint(bot->getSpawnPoint());
        return true;
    }
    if (game->getSpawnPointList(side).empty())
    {
        bot->setSpawnTarget(-1, 13);
        return false;
    }

    IAIGame* const searchGame = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    const std::list<int>& groups = searchGame->getSpawnPointList(side);
    int group = 0;
    float best = 1.0e9f;
    for (std::list<int>::const_iterator it = groups.begin(); it != groups.end(); ++it)
    {
        if (searchGame->isEligibleSpawnpoint(side, *it))
        {
            const float distance = distXZSqr(searchGame->getSpawnPointPosition(*it), position);
            if (distance < best)
            {
                best = distance;
                group = *it;
                spawnPosition = searchGame->getSpawnPointPosition(group);
            }
        }
    }

    AIStrategicObject* const object =
        AIStrategicObjectManager::getInstance()->getClosestObject(spawnPosition, side);
    const int kit = findSuitableKit(enemyStrengths, enemyCounts, friendlyStrengths,
        friendlyCounts, object, side, bot);
    if (object)
        bot->setSpawnTarget(object->getIndex(), group, kit);
    else
        bot->setSpawnTarget(group, kit);
    return true;
}

// ELF 0x35DDE0. Over the six strength types: the squared shortfall of values below counts goes
// to sqrDist, the surplus (including NaN differences) to excess. No caller in the image.
void BotSpawner::sqrDistAndExcess(float* values, int* counts, float& sqrDist, float& excess)
{
    sqrDist = 0.0f;
    excess = 0.0f;
    for (int i = 0; i < 6; ++i)
    {
        const float difference = values[i] - static_cast<float>(counts[i]);
        if (difference < 0.0f)
            sqrDist += difference * difference;
        else
            excess += difference;
    }
}

// Empty input returns 0 without touching the output or static candidates. A
// failed nonempty search returns -1, retaining any last output object write.
// Deep path and Information forwarding bodies still stop at their actual calls.
int BotSpawner::findSuitableSpawnGroup(int side, AIStrategicObject*& object, Bot* bot)
{
    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    const std::list<int>& groups = game->getSpawnPointList(side);
    if (groups.empty())
        return 0;

    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    std::vector<unsigned int> information;
    SAI* const sai = IAIMain::m_instance->getSAI(side);
    SquadMember* const member = bot->getSquadMember();
    Vec3 squadPosition = { 0.0f, 0.0f, 0.0f };
    member->getSquad()->getSquadPosition(squadPosition);
    const Vec2 squadXZ = { squadPosition.x, squadPosition.z };
    m_pointWeightVector.resize(0, SpawnChoice{});

    const double draw = static_cast<double>(AIRandom::m_instance->next()) * (1.0 / 4294967296.0);
    if ((draw > 0.3f || !member->getSquad()->getSquadLeader()->getPlayer()->getIsAIPlayer()) &&
        !member->isSquadLeader() &&
        member->getSquad()->getSquadLeader()->getPlayer()->getIsAlive() &&
        !member->getSquad()->getSquadLeader()->getPlayer()->isManDown())
    {
        object = 0;
        IAIPathfinding* const pathfinding = IAIPathfinding::instance;
        float best = FLT_MAX;
        Vec3 position = { 0.0f, 0.0f, 0.0f };
        std::list<int>::const_iterator selected;
        bool found = false;
        for (std::list<int>::const_iterator it = groups.begin(); it != groups.end(); ++it)
        {
            if (game->isEligibleSpawnpoint(side, *it))
            {
                const Vec3& candidate = game->getSpawnPointPosition(*it);
#if defined(_MSC_VER)
                // [PLATFORM] PE 0x10070AA0 materializes the Vec3 subtraction
                // before length-squared. ELF keeps the differences in x87.
                const Vec3 delta = { squadPosition.x - candidate.x,
                    squadPosition.y - candidate.y, squadPosition.z - candidate.z };
                const double dx = delta.x, dy = delta.y, dz = delta.z;
#else
                const double dx = static_cast<double>(squadPosition.x) - candidate.x;
                const double dy = static_cast<double>(squadPosition.y) - candidate.y;
                const double dz = static_cast<double>(squadPosition.z) - candidate.z;
#endif
                const float distance = static_cast<float>(dx * dx + dy * dy + dz * dz);
                // Repeated for every eligible point, even when its distance
                // will lose. The second result is what path validation sees.
                position = game->getSpawnPointPosition(*it);
                if (distance < best)
                {
                    const int traceMap = pathfinding->getTraceMapIndex();
                    if (pathfinding->isValidPosition(traceMap, 3, position, true))
                    {
                        best = distance;
                        selected = it;
                        found = true;
                    }
                }
            }
        }
        if (found)
        {
            sai->addSpawnToPoint(*selected);
            return *selected; // reload after the callback, not the saved integer
        }
    }

    IAIInformationEnvironment* const environment =
        static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);
    FreeUnitPred predicate(side);
#if defined(_MSC_VER)
    float greatestValue = 0.0f; // PE maintains this otherwise unused maximum
#endif
    for (std::list<int>::const_iterator it = groups.begin(); it != groups.end(); ++it)
    {
        if (game->isEligibleSpawnpoint(side, *it))
        {
            const Vec3& position = game->getSpawnPointPosition(*it);
            if (game->getControlPointId(*it) != -1)
            {
                object = manager->getClosestObject(position, side);
                if (object)
                {
                    // Both ports inline +0xA0 +4*side: native m_f164[side-1].
                    const float strategicValue = object->m_f164[side - 1];
                    information.resize(0);
                    predicate.m_strength = 0.0f;
                    environment->getObjectsWithinRadius(information, position, 50.0f,
                        side, true, &predicate);
                    const int count = static_cast<int>(information.size());
                    const Vec2& target = object->getPosition(side);
                    const double dx = static_cast<double>(squadXZ.x) - target.x;
                    const double dz = static_cast<double>(squadXZ.y) - target.y;
                    const float distance = static_cast<float>(std::sqrt(dx * dx + dz * dz));
                    const SpawnChoice choice = { *it, strategicValue, count,
                        predicate.m_strength, object, distance, 0.0f };
                    m_pointWeightVector.push_back(choice);
#if defined(_MSC_VER)
                    if (greatestValue < strategicValue)
                        greatestValue = strategicValue;
#endif
                }
            }
        }
    }
    if (m_pointWeightVector.empty())
        return -1;

    // [PLATFORM] ELF stores its total to float on every addition; PE retains
    // it through the loop, spilling to float only across a random refill.
#if defined(_MSC_VER)
    double total = 0.0;
#else
    float total = 0.0f;
#endif
    for (std::vector<SpawnChoice>::iterator it = m_pointWeightVector.begin();
         it != m_pointWeightVector.end(); ++it)
    {
        double weight = (static_cast<double>(it->freeStrength) * 0.033f + 1.0f) *
            it->strategicValue - static_cast<double>(it->distance) * 0.1f;
        if (weight < 1.0f)
            weight = 1.0f; // strict comparison retains NaN
        it->weight = static_cast<float>(weight);
#if defined(_MSC_VER)
        total += weight; // unrounded weight, not the stored record value
#else
        total = static_cast<float>(total + weight);
#endif
    }
    AIRandom* const random = AIRandom::m_instance;
#if defined(_MSC_VER)
    if (static_cast<unsigned int>(random->m_index) >= AIRandom::kStateSize)
        total = static_cast<float>(total);
    const double raw = random->next();
    double remaining = (total * raw) * (1.0 / 4294967296.0);
#else
    const double raw = random->next();
    double remaining = (raw * (1.0 / 4294967296.0)) * total;
#endif
    std::vector<SpawnChoice>::iterator selected = m_pointWeightVector.begin();
    for (; selected != m_pointWeightVector.end(); ++selected)
    {
        remaining -= selected->weight;
        if (remaining <= 0.0)
            break;
    }
    if (selected == m_pointWeightVector.end())
        selected = m_pointWeightVector.end() - 1;
    sai->addSpawnToPoint(selected->group);
    object = selected->object;
    return selected->group;
}

// Chooses a random kit excluding type 5 and null templates; ignores the member argument.
// Requires at least one candidate and preserves repeated template lookups.
KitType BotSpawner::GetRandomKit(SquadMember*)
{
    const std::vector<IKitTemplate*>& templates =
        static_cast<IKitTemplateManager*>(CommonSingletonServer::m_iKitTemplateManager)->getAllTemplates();
    std::vector<KitType> choices;
    for (std::vector<IKitTemplate*>::const_iterator it = templates.begin();
         it != templates.end(); ++it)
    {
        if (*it && (*it)->getType() != static_cast<KitType>(5))
            choices.push_back((*it)->getType());
    }
    AIRandom* const random = AIRandom::m_instance;
#if defined(_MSC_VER)
    // [PLATFORM] PE converts the wide negative product through __ftol2 and
    // subtracts that index. It never makes ELF's final float store.
    const unsigned int count = static_cast<unsigned int>(choices.size());
    // Both paired refill branches interpret the stored index as unsigned.
    if (static_cast<unsigned int>(random->m_index) >= AIRandom::kStateSize)
        random->generateNumbers();
    const double raw = random->next();
    const int offset = static_cast<int>((raw * count) * (-1.0 / 4294967296.0));
    return *(choices.begin() - offset);
#else
    const float count = static_cast<float>(static_cast<unsigned int>(choices.size()));
    if (static_cast<unsigned int>(random->m_index) >= AIRandom::kStateSize)
        random->generateNumbers();
    const double raw = random->next();
    const float index = static_cast<float>((raw * (1.0 / 4294967296.0)) * count);
    return choices[static_cast<int>(index)];
#endif
}

// ELF 0x35ED30. Squad leaders may not take kits 4 and 5; everyone else may take any kit. No
// caller in the image.
bool BotSpawner::isAllowedKit(SquadMember* member, KitType kit)
{
    bool allowed = true;
    if (member->isSquadLeader() && static_cast<unsigned int>(kit - 4) <= 1u)
        allowed = false;
    return allowed;
}

// Chooses a kit from squad needs and nearby equipment. Always consumes the initial
// random-kit selection; only nonleaders can take the medic early return.
// The assigned-squad arm scores the squad's area (ELF 0x35E3C0).
int BotSpawner::findSuitableKit(const float*, const float*, const float*, const float*,
    AIStrategicObject* object, int side, Bot* bot)
{
    GetRandomKit(bot->getSquadMember());
    const std::vector<IKitTemplate*>& templates =
        static_cast<IKitTemplateManager*>(CommonSingletonServer::m_iKitTemplateManager)->getAllTemplates();
#if defined(_MSC_VER)
    // [PLATFORM] PE captures the singleton before the virtual player-id query;
    // ELF reloads it afterwards. A callback may change the published manager.
    SquadManager* const manager = SquadManager::m_instance;
    const int playerId = bot->getPlayerId();
#else
    const int playerId = bot->getPlayerId();
    SquadManager* const manager = SquadManager::m_instance;
#endif
    SquadCoordinator* const squad = manager->getSquadFromPlayer(playerId, side);
    if (squad && !bot->getSquadMember()->isSquadLeader())
    {
        if (!squad->hasMemberWithKit(static_cast<KitType>(5)) ||
            bot->getPlayer()->getSpawnKit() == 5)
            return 5;
    }
    // Both ports keep this vector alive across a selected-13 shared fallback.
    std::vector<unsigned int> information;
    if (object)
    {
        // [INFERRED] unchecked source downcast: both ports directly copy the
        // area's m_centre (ELF +0x110 / PE +0x118), with no type query or guard.
        // Copy before getPosition: the callback may change the area's storage.
        Vec2 minimum = static_cast<AIStrategicArea*>(object)->m_centre;
        const Vec2 position = object->getPosition(side);
        const double dx = static_cast<double>(position.x) - minimum.x;
#if defined(_MSC_VER)
        // [PLATFORM] PE spills only the Y delta to float before multiplying;
        // ELF retains both wide originals despite spilling unused copies.
        const double dy = static_cast<float>(static_cast<double>(position.y) - minimum.y);
#else
        const double dy = static_cast<double>(position.y) - minimum.y;
#endif
        minimum.x = static_cast<float>(static_cast<double>(minimum.x) - dx * 3.0f);
        minimum.y = static_cast<float>(static_cast<double>(minimum.y) - dy * 3.0f);
        Vec2 maximum = {};
        maximum = getBoxMaxCorner(minimum, position, maximum);
        IAIInformationEnvironment* const environment =
            static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);
        environment->getObjectsWithinRectangle(information, minimum, maximum, side, true, 0);

        // Counter names are [INFERRED].
        // Only kit 13 skips its unchecked bin.
        int kitCounts[13] = {};
        int enemyVehicles = 0, enemyInfantry = 0;
        int friendlyVehicles = 0, friendlyInfantry = 0, damagedFriendlyVehicles = 0;
        const int enemySide = (side == 1) + 1;
        std::vector<unsigned int>::const_iterator end = information.end();
        for (std::vector<unsigned int>::const_iterator it = information.begin(); it != end; ++it)
        {
#if defined(_MSC_VER)
            // [PLATFORM] PE skips the manager access for a zero low word.
            const unsigned int handle = *it;
            Information* const entry = (handle & 0xFFFFu) ?
                informationManager->getInformation(handle) : 0;
#else
            Information* const entry = informationManager->getInformation(*it);
#endif
            if (entry)
            {
                IPIPhysical* const physical = entry->getPhysical();
                if (vehicleOccupied(*it, side))
                {
                    if (entry->getSide() == enemySide)
                    {
                        switch (physical->getStrengthType())
                        {
                        case 0: ++enemyInfantry; break;
                        case 1: case 2: case 4: ++enemyVehicles; break;
                        }
                    }
                    else
                    {
                        switch (physical->getStrengthType())
                        {
                        case 0:
                        {
                            ++friendlyInfantry;
                            const int kit = entry->getKitType();
                            if (kit != 13)
                                ++kitCounts[kit];
                            physical->getHealth(); // shipped discarded call, even for kit 13
                            break;
                        }
                        case 1: case 2: case 4:
                            ++friendlyVehicles;
                            // Only health strictly below 0.5 counts; equality and NaN are excluded.
                            if (physical->getHealth() < 0.5f)
                                ++damagedFriendlyVehicles;
                            break;
                        }
                    }
                }
#if !defined(_MSC_VER)
                // ELF refreshes after every nonnull record, including occupancy
                // refusal; a null/stale lookup retains its previously cached end.
                end = information.end();
#endif
            }
#if defined(_MSC_VER)
            // PE refreshes on every iteration, including null/stale records.
            end = information.end();
#endif
        }

        // Copy the actual map before callbacks, retaining
        // a snapshot while the source map can change. Its second payload is kit.
        int selectedKit = 13;
        {
            const std::map<int, std::pair<int, int> > deadBots =
                IAIMain::m_instance->getSAI(side)->getDeadBots();
            for (std::map<int, std::pair<int, int> >::const_iterator it = deadBots.begin();
                 it != deadBots.end(); ++it)
            {
                if (static_cast<unsigned int>(it->first) != bot->getBotId())
                {
                    Bot* const other = IBotManager::instance->getBotFromId(static_cast<unsigned int>(it->first));
                    if (other)
                    {
#if defined(_MSC_VER)
                        // [PLATFORM] PE retains the area index before the callback;
                        // ELF reads it after the callback has returned its target.
                        const int areaIndex = object->getIndex();
                        const int target = other->getSpawnToStrategicAreaId();
#else
                        const int target = other->getSpawnToStrategicAreaId();
                        const int areaIndex = object->getIndex();
#endif
                        if (target == areaIndex && it->second.second != 13)
                            ++kitCounts[it->second.second];
                    }
                }
            }

            // Retain the vector returned at entry
            // but read its current storage after the preceding callbacks.
            if (enemyVehicles > friendlyVehicles + kitCounts[6] && templates[6])
                selectedKit = 6;
            else
            {
#if defined(_MSC_VER)
                // [PLATFORM] PE converts the signed integers directly to x87;
                // ELF first rounds each to float via cvtsi2ss before the product.
                const double enemyCount = enemyInfantry;
                const double friendlyCount = friendlyInfantry;
#else
                const double enemyCount = static_cast<float>(enemyInfantry);
                const double friendlyCount = static_cast<float>(friendlyInfantry);
#endif
                if (enemyCount > friendlyCount * 1.5f)
                {
                    AIRandom* const random = AIRandom::m_instance;
                    if (static_cast<unsigned int>(random->m_index) >= AIRandom::kStateSize)
                        random->generateNumbers();
                    // Exact unsigned scale; no float narrowing before comparisons.
                    const double draw = static_cast<double>(random->next()) * (1.0 / 4294967296.0);
                    if (draw > 0.85f && templates[0])
                        selectedKit = 0;
                    else if (draw > 0.55f && templates[2])
                        selectedKit = 2;
                    else if (draw > 0.25f && templates[3])
                        selectedKit = 3;
                    else if (templates[4])
                        selectedKit = 4;
                }
                else if (damagedFriendlyVehicles > kitCounts[4] && templates[4])
                    selectedKit = 4;
            }
        } // Destroy the copied map before testing the result, as both ports do.
        if (selectedKit != 13)
            return selectedKit;
    }

    // Shared roster/fallback continuation: null object or object scoring gave 13.
    // Count is captured once and compared as signed;
    // each indexed member/player is fresh. Only kit 13 skips its unchecked bin.
    if (squad)
    {
        const int members = squad->getNumberOfSquadMembers();
        int kitCounts[13] = {};
        for (int i = 0; i < members; ++i)
        {
            const int kit = squad->getSquadMember(i)->getPlayer()->getSpawnKit();
            if (kit != 13)
                ++kitCounts[kit];
        }
        if (squad->isSquadAssignedToAnyArea())
        {
            // ELF 0x35E3C0..0x35E6F4: the members' current vehicles are counted (strength types
            // 1/2/4 vehicles, damaged below 0.5f health; 0 infantry, health read and discarded),
            // then the enemies around the assigned area (rectangle centre - 2 (pos - centre),
            // the object arm uses 3), then the same kit ladder as the object arm with an integer
            // infantry comparison and a vehicle-repair test against kitCounts[4].
            int friendlyVehicles = 0, damagedFriendlyVehicles = 0, friendlyInfantry = 0;
            for (int i = 0; i < members; ++i)
            {
                IAIObject* const vehicle = squad->getSquadMember(i)->getPlayer()->getCurrentVehicle();
                if (!vehicle)
                    continue;
                Information* const entry = informationManager->getInformation(vehicle->getInformation(side));
                if (!entry || !entry->getUnit())
                    continue;
                IPIPhysical* const physical = entry->getPhysical();
                switch (physical->getStrengthType())
                {
                case 1: case 2: case 4:
                    ++friendlyVehicles;
                    if (physical->getHealth() < 0.5f)
                        ++damagedFriendlyVehicles;
                    break;
                case 0:
                    ++friendlyInfantry;
                    physical->getHealth();      // shipped discarded call
                    break;
                }
            }
            AIStrategicObject* const area = AIStrategicObjectManager::getInstance()->getStrategicObject(
                squad->getSquadStrategicAreaAssignment()->getIndex());
            Vec2 minimum = static_cast<AIStrategicArea*>(area)->m_centre;
            const Vec2 position = area->getPosition(side);
            const double dx = static_cast<double>(position.x) - minimum.x;
            const double dy = static_cast<double>(position.y) - minimum.y;
            minimum.x = static_cast<float>(static_cast<double>(minimum.x) - dx * 2.0f);
            minimum.y = static_cast<float>(static_cast<double>(minimum.y) - dy * 2.0f);
            Vec2 maximum = {};
            maximum = getBoxMaxCorner(minimum, position, maximum);
            IAIInformationEnvironment* const environment =
                static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);
            environment->getObjectsWithinRectangle(information, minimum, maximum, side, true, 0);
            int enemyVehicles = 0, enemyInfantry = 0;
            const int enemySide = (side == 1) + 1;
            std::vector<unsigned int>::const_iterator end = information.end();
            for (std::vector<unsigned int>::const_iterator it = information.begin(); it != end; ++it)
            {
                Information* const entry = informationManager->getInformation(*it);
                if (!entry)
                    continue;
                IPIPhysical* const physical = entry->getPhysical();
                if (vehicleOccupied(*it, side) && entry->getSide() == enemySide)
                {
                    switch (physical->getStrengthType())
                    {
                    case 0: ++enemyInfantry; break;
                    case 1: case 2: case 4: ++enemyVehicles; break;
                    }
                }
                end = information.end();        // ELF refreshes after every nonnull record
            }
            if (enemyVehicles > friendlyVehicles + kitCounts[6] && templates[6])
                return 6;
            if (enemyInfantry > friendlyInfantry)
            {
                AIRandom* const random = AIRandom::m_instance;
                if (static_cast<unsigned int>(random->m_index) >= AIRandom::kStateSize)
                    random->generateNumbers();
                const double draw = static_cast<double>(random->next()) * (1.0 / 4294967296.0);
                if (draw > 0.85f && templates[0])
                    return 0;
                if (draw > 0.55f && templates[2])
                    return 2;
                if (draw > 0.25f && templates[3])
                    return 3;
                if (templates[4])
                    return 4;
            }
            else if (damagedFriendlyVehicles + friendlyVehicles > kitCounts[4] * 2 && templates[4])
            {
                return 4;
            }
        }
    }

    // the final fallback draw
    // through a newly queried member and current kit manager, is the result.
    return GetRandomKit(bot->getSquadMember());
}

} } }
