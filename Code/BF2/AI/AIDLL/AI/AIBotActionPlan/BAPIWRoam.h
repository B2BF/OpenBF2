// Owns a roaming plan and retains its object handle.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWROAM_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWROAM_H

#include "BAPInfoWrapper.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;

// ELF: vtable 0x4BA120 (9 words), operator new 0x1C, package H-deferred.
class BAPIWRoam : public BAPInfoWrapper
{
public:
    BAPIWRoam(unsigned int, BAPStatement*, int);   // 0x1F7AC0
    virtual ~BAPIWRoam();   // 0x1F7B70

    virtual int getInfoType() const;   // 0x1F7C20, slot +0x20; from the base declaration
private:
    // [INFERRED] friendship for createPlan's inline target reader (ELF 0x29BFD1).
    friend class BBPRoam3d;
    /* +0x18 */ unsigned int m_object;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWROAM_H
