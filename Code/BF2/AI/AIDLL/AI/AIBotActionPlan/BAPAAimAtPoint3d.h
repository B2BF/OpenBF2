// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAAIMATPOINT3D_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAAIMATPOINT3D_H

#include "../../../../Math/Vec3.h"
#include "BAPAAimAt3d.h"

namespace dice {
namespace hfe {

struct Mat4;
namespace ai {

class BAPCondition;

// ELF: vtable 0x4B7A00 (19 words), package G-vehicles.
// Aims at a fixed point. The constructor adds the offset to the point and keeps it in m_offset,
// so getAimVec applies it twice, as shipped.
class BAPAAimAtPoint3d : public BAPAAimAt3d
{
public:
    BAPAAimAtPoint3d(Vec3&, Vec3&, BAPCondition*, unsigned long long, int, float, float);   // 0x1D0870
    virtual ~BAPAAimAtPoint3d();   // 0x1D09F0

    virtual int getAimAtType();   // 0x1D0E00, slot +0x20
    virtual bool getVectorToTarget(const Vec3&, Vec3&) const;   // 0x1D0D00, slot +0x24
    virtual bool getAimVec(const Mat4&, const Vec3&, Vec3&, float, float, float, float, bool);   // 0x1D0AF0, slot +0x28

private:
    /* +0x1D4 */ Vec3 m_point;   // the constructor's point plus offset
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAAIMATPOINT3D_H
