// Direct object-move point source; mobile availability is captured at construction.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETOOBJECTDIRECT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETOOBJECTDIRECT_H

#include "BAPAMoveToObject.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B85C0 (40 words), package H-deferred.
class BAPAMoveToObjectDirect : public BAPAMoveToObject
{
public:
    BAPAMoveToObjectDirect(Bot*, unsigned int, float, float, float, BAPCondition*, unsigned long long, int);   // 0x1DACE0
    virtual ~BAPAMoveToObjectDirect();   // 0x1DADE0

    virtual Vec2& getCurrentPoint();   // 0x1DAE70, slot +0x28; from the base declaration
    virtual int usesPathfinding();   // 0x1DB020, slot +0x4C; from the base declaration
private:
    // [UNRESOLVED] Unaccessed bytes before the mobile flag. Both images leave the span unwritten;
    // retain its extent without assigning a semantic member type (ia-32 +0xC1..+0xC7).
    unsigned char m_unwrittenC1[7];
    /* +0xC8 */ bool m_isMobile; // [INFERRED] name; amd-64 +0xE8
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETOOBJECTDIRECT_H
