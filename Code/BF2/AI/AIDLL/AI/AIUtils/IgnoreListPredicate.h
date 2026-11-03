// Object predicate that rejects the objects of a borrowed ignore list (line-of-fire and aiming
// traces). ELF: vtable 0x4BE8A8 (base AIObjectPredicate, one slot), ctor C1/C2 0x2EA000/0x2EA030,
// predicate 0x2EA060; attested FILE symbol IgnoreListPredicate.cpp (directory [INFERRED]).
#ifndef AI_AIDLL_AI_AIUTILS_IGNORELISTPREDICATE_H
#define AI_AIDLL_AI_AIUTILS_IGNORELISTPREDICATE_H

#include <vector>

#include "../../../AIInterface/BFKnowledge/AIAdapterTypes.h"   // AIObjectPredicate

namespace dice {
namespace hfe {
namespace ai {

class IgnoreListPredicate : public AIObjectPredicate
{
public:
    explicit IgnoreListPredicate(std::vector<IAIObject*>& ignore);

    // False for an object in the list, true otherwise.
    virtual bool predicate(IAIObject* object);

private:
    /* +0x04 */ std::vector<IAIObject*>* m_ignore;   // borrowed; the constructor stores the address
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif
