// BAPFlowCWhile: recovered by package A-core from libaidll.so (TU BAPFlowCWhile.cpp,
// 0x1E4200..0x1E4910). Every body carries its ELF address.

// static init: ELF 0x1E4850 (__tcf_0 0x1E48B0, _GLOBAL__I 0x1E48E0): <iostream>'s Init object only.
#include <iostream>

#include "BAPFlowCWhile.h"
#include <ostream>
#include "../Main/IBotManager.h"
#include "../Main/AIStreamOperators.h"

#include "BAPCondition.h"
#include "../AIBot/BotMain.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E4200 (C2; C1 0x1E4280). The condition's owner is set unconditionally, the body's only
// when present.
BAPFlowCWhile::BAPFlowCWhile(BAPCondition* condition, BAPStatement* loopStatement, int patternId)
    : BAPFlowControl(patternId)
    , m_condition(condition)
    , m_loopStatement(loopStatement)
    , m_break(false)
    , m_testCondition(true)
    , m_testOnFinish(true)
{
    condition->m_owner = this;
    if (m_loopStatement)
        m_loopStatement->m_owner = this;
}

// ELF 0x1E4300 (C2; C1 0x1E4390).
BAPFlowCWhile::BAPFlowCWhile(BAPCondition* condition, BAPStatement* loopStatement, bool testOnFinish,
                             int patternId)
    : BAPFlowControl(patternId)
    , m_condition(condition)
    , m_loopStatement(loopStatement)
    , m_break(false)
    , m_testCondition(true)
    , m_testOnFinish(testOnFinish)
{
    condition->m_owner = this;
    if (m_loopStatement)
        m_loopStatement->m_owner = this;
}

// ELF 0x1E4420 (D2; D1 0x1E4490, D0 0x1E4500).
BAPFlowCWhile::~BAPFlowCWhile()
{
    if (m_condition)
        delete m_condition;
    if (m_loopStatement)
        delete m_loopStatement;
}

// ELF 0x1E4580 (82 instructions); the rules are in the header. Always returns 1.
bool BAPFlowCWhile::execute(Bot* bot, bool& ok, bool& finished)
{
    if (m_loopStatement == 0)
    {
        ok = true;
        m_break = false;
        finished = !m_condition->evaluate(bot);
        return true;
    }

    if (m_break)
    {
        ok = true;
        finished = true;
        m_break = false;
        return true;
    }

    if (m_testCondition && !m_condition->evaluate(bot))
    {
        ok = true;
        finished = true;
        return true;
    }

    // BotMain::getPlanInterpreter (non-virtual, 0x18A780) on the Bot*, call at 0x1E4616.
    static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(m_loopStatement, bot, ok, finished);
    if (m_testOnFinish)
        m_testCondition = finished;
    else
        m_testCondition = true;
    finished = false;
    return true;
}

// ELF 0x1E4690.
unsigned long long BAPFlowCWhile::getControls()
{
    if (m_loopStatement == 0)
        return 0;
    return m_loopStatement->getControls();
}

// ELF 0x1E4900 (emitted after the TU's initialiser).
BAPStatementType BAPFlowCWhile::getId() const
{
    return BAPSTWhile;
}

// ELF 0x1E46C0.
bool BAPFlowCWhile::hasDefault()
{
    return true;
}

// ELF 0x1E4700 (97 instructions): tabulate, os << getId() << "(" << *condition << ")" << endl (the
// condition is not null-checked), then '{', the body one tab level deeper when there is one
// (+0x18, 0x1E47CA), and '}', each on its own tabulated line.
void BAPFlowCWhile::streamSelf(std::ostream& os) const
{
    IBotManager* const manager = IBotManager::instance;
    manager->tabulate(os);
    os << getId() << "(" << *m_condition << ")" << std::endl;
    manager->tabulate(os);
    os << '{' << std::endl;
    if (m_loopStatement)
    {
        manager->increaseTabLevel();
        os << *m_loopStatement;
        manager->decreaseTabLevel();
    }
    manager->tabulate(os);
    os << '}' << std::endl;
}

// ELF 0x1E46D0.
void BAPFlowCWhile::breakLoop()
{
    m_break = true;
}

// ELF 0x1E46E0.
BAPCondition* BAPFlowCWhile::getCondition()
{
    return m_condition;
}

// ELF 0x1E46F0.
BAPStatement* BAPFlowCWhile::getLoopStatement()
{
    return m_loopStatement;
}

} // namespace ai
} // namespace hfe
} // namespace dice
