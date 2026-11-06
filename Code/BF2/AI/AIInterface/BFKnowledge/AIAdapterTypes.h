// Shared forward declarations for the engine/AI boundary.
// Opaque classes are used only by pointer or reference. Value enums use 32-bit storage.
// [CHOSEN, not recovered] Declaration grouping; original header placement is [INFERRED].
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_AIADAPTERTYPES_H
#define AI_AIINTERFACE_BFKNOWLEDGE_AIADAPTERTYPES_H

// Use the complete Math vector and matrix types: Vec2/Vec3 are aliases, and some adapter
// interfaces pass matrices by value.
#include "../../../Math/Vec3.h"          // Math::Vec2 / Vec3 and the hfe-level typedefs
#include "../../../Math/Mat4.h"
#include "../../../Math/Frustum.h"

namespace dice {
namespace hfe {

namespace world {
class IObject;
class IPlayer;
class IProjectileObject;
} // namespace world

// `dice::hfe::ChannelMessageType` - BFRadioComm::sendAIRadioMessage's second parameter
// (`NS0_18ChannelMessageType`, i.e. hfe scope, not ai). Passed by value; an opaque enum with an
// explicit `int` base gives it the 4-byte slot the call frame shows. [FRONTIER] its enumerators.
enum ChannelMessageType : int;

// `dice::hfe::KitType` - KitTemplateManager::getTemplate's parameter (`NS0_7KitType`).
enum KitType : int;

namespace ai {

// opaque AI-side classes, each attested by a mangled name
class IAIPlayer;              // BFGameEffector, BFCommander, BFSquad, BFTerrain, AIPlayerFactory
class IAIObject;              // BFGameEffector, BFCommander, BFSquad, BFObjectEnvironment

// Predicate used to filter engine objects for AI queries.
// Its only virtual method is includeObject; the destructor is nonvirtual.
class AIObjectPredicate
{
public:
    virtual bool predicate(IAIObject* object) = 0;   // +0x00, both images
};

// Object-local navigation bounds and transformation, returned by getAllDestroyableObjects.
// ELF bf2 0x8401390 writes the byte, both vectors and the complete matrix; libaidll's
// drawDynamicAIObjects consumes a 92-byte element. Member names are [INFERRED].
class DestroyableObject
{
public:
    /* +0x00 */ bool initiallyWalkable;
    /* +0x04 */ Vec3 min;
    /* +0x10 */ Vec3 max;
    /* +0x1C */ Mat4 transform;
};
class TargetData;             // BFObjectEnvironment::getTargetObjects, likewise
class IWeapon;                // BFObjectEnvironment::hasIndirectLineOfFire
class IKitTemplate;           // KitTemplateManager's two containers
class IBFGraph;               // BFGraphCreator::createGraph / destroyGraph

// DebugRenderer's two alignment enums, both passed by value.
enum AITextAlignmentX : int;
enum AITextAlignmentY : int;

// BFRadioComm::sendAIRadioMessage's third parameter (`NS1_18AIRadioChannelType`), by value.
enum AIRadioChannelType : int;

// Forward declaration for the three-value elements in BFTerrain polygon lists.
// The complete template is defined by the pathfinding interface.
template <class A, class B, class C> struct triple;

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_AIADAPTERTYPES_H
