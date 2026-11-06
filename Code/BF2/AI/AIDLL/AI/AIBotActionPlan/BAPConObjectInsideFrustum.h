// Object visibility against a camera frustum initialized on the first eligible evaluation.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTINSIDEFRUSTUM_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTINSIDEFRUSTUM_H

#include "BAPConObject.h"
#include "../../../../Math/Frustum.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9738 (5 words), package H-deferred.
class BAPConObjectInsideFrustum : public BAPConObject
{
public:
    enum TransformType : int;   // [UNRESOLVED] enumerators
    BAPConObjectInsideFrustum(unsigned int, BAPConObjectInsideFrustum::TransformType);   // 0x1EED40
    virtual ~BAPConObjectInsideFrustum();   // 0x1EEDC0

    virtual bool evaluate(Bot*);   // 0x1EEE20, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EF000, slot +0x04; from the base declaration
private:
    /* +0x00C */ TransformType m_transformType;
    /* +0x010 */ Frustum m_frustum;
    /* +0x1F8 */ bool m_initialize;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTINSIDEFRUSTUM_H
