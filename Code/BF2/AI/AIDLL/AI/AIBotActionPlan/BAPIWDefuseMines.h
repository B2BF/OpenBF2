// Owns a mine-defusing plan and brackets its lifetime with the bot's firing/mine-target state.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWDEFUSEMINES_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWDEFUSEMINES_H

#include "BAPInfoWrapper.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;
class Bot;

// ELF: vtable 0x4BA020 (9 words), operator new 0x20, package F-infantry.
class BAPIWDefuseMines : public BAPInfoWrapper
{
public:
    BAPIWDefuseMines(Bot*, unsigned int, BAPStatement*, int);   // 0x1F6A70
    virtual ~BAPIWDefuseMines();   // 0x1F6B80

    virtual int getInfoType() const;   // 0x1F6D40, slot +0x20; from the base declaration
private:
    // [INFERRED] friendship for the inline mine reader in createPlan (ELF 0x243FB0).
    friend class BBPDefuseMines;
    /* +0x18 */ unsigned int m_mine;
    /* +0x1C */ Bot* m_bot;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWDEFUSEMINES_H
