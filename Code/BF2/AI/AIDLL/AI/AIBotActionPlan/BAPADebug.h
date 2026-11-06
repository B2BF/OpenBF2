// A completed debug marker whose integer value appears in plan dumps.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPADEBUG_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPADEBUG_H

#include <iosfwd>
#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4BA540 (8 words), package H-deferred.
class BAPADebug : public BAPAction
{
public:
    explicit BAPADebug(int);   // 0x1FB6F0
    virtual ~BAPADebug();   // 0x1FB780

    virtual bool execute(Bot*, bool&, bool&);   // 0x1FB810, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1FB9A0, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1FB9B0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1FB9C0, slot +0x14; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1FB830, slot +0x1C; from the base declaration
private:
    /* +0x14 */ int m_value;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPADEBUG_H
