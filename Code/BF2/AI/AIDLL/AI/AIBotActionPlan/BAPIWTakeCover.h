// Owns a take-cover plan and stores either a fixed point or an object's initial position.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWTAKECOVER_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWTAKECOVER_H

#include "../../../../Math/Vec3.h"
#include "BAPInfoWrapper.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;

// ELF: vtable 0x4BA160 (9 words), operator new 0x2C, package H-deferred.
class BAPIWTakeCover : public BAPInfoWrapper
{
public:
    BAPIWTakeCover(const Vec3&, BAPStatement*, int);   // 0x1F7CB0
    BAPIWTakeCover(unsigned int, BAPStatement*, int);   // 0x1F7E00
    virtual ~BAPIWTakeCover();   // 0x1F7F20

    virtual int getInfoType() const;   // 0x1F8080, slot +0x20; from the base declaration
private:
    // [INFERRED] friendship for the inline +0x18/+0x24/+0x28 readers in BBPTakeCover::createPlan
    // (ELF 0x29F98C, 0x29F9E3, 0x29FA73, 0x29FAE7, 0x29FB4F).
    friend class BBPTakeCover;
    // The infantry pattern repeats the same readers (ELF 0x2A664D, 0x2A669E, 0x2A6724).
    friend class BBPTakeCoverInfantry;
    /* +0x18 */ Vec3 m_position;
    /* +0x24 */ unsigned int m_object; // initialized only by the object-taking constructor
    /* +0x28 */ bool m_useObject;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWTAKECOVER_H
