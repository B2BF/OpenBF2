// Bot spawn selection and placement.
// [INFERRED] Source placement.
#ifndef AI_AIDLL_AI_AISTRATEGICMAP_BOTSPAWNER_H
#define AI_AIDLL_AI_AISTRATEGICMAP_BOTSPAWNER_H

#include <vector>

#include "../../../../Math/Vec3.h"

namespace dice { namespace hfe {
enum KitType : int;
namespace ai {
class Bot;
class SquadMember;
class AIStrategicObject;

// Name/type attested by ELF vector<SpawnChoice> instantiations. Field names and
// this header placement are [INFERRED]; both selectors' stores pin the 28-byte
// x86 record. The information count is recorded but not used in this weight.
struct SpawnChoice
{
    int group;
    float strategicValue;
    int informationCount;
    float freeStrength;
    AIStrategicObject* object;
    float distance;
    float weight;
};

// Element of BotSpawner::m_kitWeightVector. Only the 8-byte, trivially destructible element is
// attested (the teardown at ELF 0x35EE90 steps by 8 and frees capacity * 8); nothing in the image
// reads or writes the vector. The name and fields are [INFERRED].
struct KitWeight
{
    int kit;
    float weight;
};

// These static contracts imply no instance layout, lifecycle or factory.
class BotSpawner
{
public:
    static bool findSuitableSpawnGroup(float* enemyStrengths, float* enemyCounts,
                                      float* friendlyStrengths, float* friendlyCounts,
                                      int strategicObjectId, Bot* bot);
    // Return type bool as in the sibling overload [INFERRED]; the only caller discards it.
    static bool findSuitableSpawnGroup(const Vec3& position, Bot* bot);
    static int findSuitableSpawnGroup(int side, AIStrategicObject*& object, Bot* bot);
    static int findSuitableKit(const float* enemyStrengths, const float* enemyCounts,
                               const float* friendlyStrengths, const float* friendlyCounts,
                               AIStrategicObject* object, int side, Bot* bot);
    static KitType GetRandomKit(SquadMember* member);
    static void sqrDistAndExcess(float* values, int* counts, float& sqrDist, float& excess);
    static bool isAllowedKit(SquadMember* member, KitType kit);

    // Shared static vector. Ordinary
    // static construction/destruction; retained capacity across selector calls.
    // Public access is [CHOSEN, not recovered], not an instance-layout claim.
    static std::vector<KitWeight> m_kitWeightVector;
    static std::vector<SpawnChoice> m_pointWeightVector;
};

} } }
#endif
