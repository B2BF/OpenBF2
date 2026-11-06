// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAAIMATOBJECT3D_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAAIMATOBJECT3D_H

#include "../../../../Math/Vec3.h"
#include "BAPAAimAt3d.h"

namespace dice {
namespace hfe {

struct Mat4;
namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B7900 (19 words), package G-vehicles.
// Aims at an information object: its position, or the bot's sensed point on it, with its velocity
// (or its primary object's).
class BAPAAimAtObject3d : public BAPAAimAt3d
{
public:
    BAPAAimAtObject3d(Bot*, unsigned int, const Vec3&, BAPCondition*, unsigned long long, int, float, float);   // 0x1CF410
    virtual ~BAPAAimAtObject3d();   // 0x1CF560

    virtual int getAimAtType();   // 0x1CFD40, slot +0x20
    virtual bool getVectorToTarget(const Vec3&, Vec3&) const;   // 0x1CFBC0, slot +0x24
    virtual bool getAimVec(const Mat4&, const Vec3&, Vec3&, float, float, float, float, bool);   // 0x1CF660, slot +0x28

private:
    // [INFERRED] friendship: EntryHeliAimAt reads +0x1D8 inline (ELF 0x2B5420).
    friend class EntryHeliAimAt;
    // [INFERRED] friendship: EntryPlaneAimAt reads +0x1D8 inline (ELF 0x2BFC70).
    friend class EntryPlaneAimAt;
    /* +0x1D4 */ Bot*         m_bot;
    /* +0x1D8 */ unsigned int m_target;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAAIMATOBJECT3D_H
