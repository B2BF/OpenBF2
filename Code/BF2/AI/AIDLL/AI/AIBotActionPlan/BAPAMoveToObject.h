// Object-targeted move base; owns its optional end condition through BAPAMoveTo.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETOOBJECT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETOOBJECT_H

#include <iosfwd>
#include "BAPAMoveTo.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B8500 (40 words), package C-actions.
// Moves to an object until within m_distance (or the end condition holds).
class BAPAMoveToObject : public BAPAMoveTo
{
public:
    BAPAMoveToObject(Bot*, unsigned int, float, float, float, BAPCondition*, unsigned long long, int);   // 0x1DA690
    virtual ~BAPAMoveToObject();   // 0x1DAB50

    virtual bool execute(Bot*, bool&, bool&);   // 0x1DA7D0, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1DAB10, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1DAB20, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1DAB30, slot +0x14; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1DA9A0, slot +0x1C; from the base declaration
    virtual bool isFinished(Bot*);   // 0x1DA750, slot +0x30; from the base declaration
    virtual unsigned int getObject();   // 0x1DAB40, slot +0x40; from the base declaration
    virtual float getSimpleTargetDistance(Bot*);   // 0x1DA7F0, slot +0x44; from the base declaration
    virtual float getSimpleTargetDistanceSqr(Bot*);   // 0x1DA8E0, slot +0x48; from the base declaration


    /* +0xB4 */ unsigned int m_object;     // information handle
    /* +0xB8 */ float m_distance;
    /* +0xBC */ float m_distanceSqr;
    /* +0xC0 */ bool m_pathValid2;         // not written here; BAPAMoveToObjectFinding sets it (initPathfinding's answer); [UNRESOLVED] name

};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETOOBJECT_H
