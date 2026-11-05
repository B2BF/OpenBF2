// [CHOSEN, not recovered] Shared opaque declarations for AI DLL interfaces.
// Incomplete types may be used only through pointers or references.
// [INFERRED] Source placement.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining AIDllTypes operations.

#ifndef AI_AIDLL_AI_MAIN_AIDLLTYPES_H
#define AI_AIDLL_AI_MAIN_AIDLLTYPES_H

#include "../../../AIInterface/BFKnowledge/AIAdapterTypes.h"

namespace dice {
namespace hfe {

namespace world {
// AIEventCourier::eventMissileLock's second parameter. Passed by pointer only.
class MissileLockMessage;
} // namespace world

namespace ai {

// knowledge records
class Information;            // the SpHandlePool payload; see Main/InformationManager.h
class WeaponInfo;             // likewise; see AIUtils/WeaponInfoManager.h

// the bot itself
// `Bot*` and `unsigned int` appear as alternative first arguments on the same DPManager creators
// (createDPDynamicCircle has one of each). So the AI addresses a bot two ways - by pointer and by
// handle - and the pair of overloads is the seam between them. Chunk 06 owns the class.
class Bot;

// draw primitives
// The seven _Rb_trees inside DPManager hold these. `DrawPrimitive` is the base every `delete*` and
// `get*Attributes` slot takes; the three named subclasses are the return types of the creators that
// materialise one, and are the only evidence that the hierarchy has subclasses at all.
class DrawPrimitive;
class DPStaticCircle;
class DPStaticLine;
class DPStaticMesh;

// `createStaticMesh(float, unsigned int*, Polygon*, Vec3*, unsigned int*, int, bool)` - the one
// signature in the draw-primitive set that names a geometry type.
class Polygon;

// DPManager's dynamic creators all take one; [UNRESOLVED] what the values are.
enum DPDynamicTypes : int;

// radio
// Sixteen bytes, contents unrecovered - AI/AIInterface/Module/BFConsole.h owns the definition and
// derives the size from the map node it lives in. Only ever passed here by const reference.
struct AIRadioMessageSettings;

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIDLLTYPES_H
