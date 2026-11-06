// Runs a child statement through the bot's interpreter and supplies a plan information type.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPINFOWRAPPER_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPINFOWRAPPER_H

#include <iosfwd>
#include <string>
#include "BAPStatement.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;
class Bot;

// BSS 0x4CE220: "Fire", "DefuseMines", "MoveTo", "Avoid", "Idle", "Roam", "Scout", "TakeCover",
// "Change", "Default" (static initialiser 0x1F5740), indexed by getInfoType().
extern std::string infoTypes[10];

// ELF: vtable 0x4B9F60 (9 words), package C-actions.
// Wraps one statement and tags it with an information type (getInfoType, an index into
// infoTypes); runs the child through the bot's plan interpreter.
class BAPInfoWrapper : public BAPStatement
{
public:
    BAPInfoWrapper(BAPStatement*, int);   // 0x1F5470
    virtual ~BAPInfoWrapper();   // 0x1F5510

    virtual bool execute(Bot*, bool&, bool&);   // 0x1F55A0, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1F5990, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1F59B0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1F59C0, slot +0x14; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1F5600, slot +0x1C; from the base declaration
    virtual int getInfoType() const;   // 0x1F59D0, slot +0x20; [INFERRED] ELF int

    // [CHOSEN, not recovered] added by ai-w1-e-fire for BBPFireInfantery: inline in the image
    // (createPlan reads +0x10, BAPStatement::m_patternId).
    int getGeneratorId() const { return m_patternId; }

    /* +0x14 */ BAPStatement* m_statement;   // the wrapped child; deleted by the subclasses' destructors

};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPINFOWRAPPER_H
