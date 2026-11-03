// BAPFlowCIf: recovered by package A-core from libaidll.so (TU BAPFlowCIf.cpp, 0x1E3550..0x1E3F80).
// Every body carries its ELF address.

// static init: ELF 0x1E3E20 (__tcf_0 0x1E3E80, _GLOBAL__I 0x1E3EB0): <iostream>'s Init object only.
#include <iostream>

#include "BAPFlowCIf.h"
#include <ostream>
#include "../Main/IBotManager.h"
#include "../Main/AIStreamOperators.h"

#include "BAPCondition.h"
#include "../AIBot/BotMain.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E3550 (C2; C1 0x1E35F0). The condition's owner is set unconditionally, the branches' only
// when present.
BAPFlowCIf::BAPFlowCIf(BAPCondition* condition, BAPStatement* trueStatement, BAPStatement* falseStatement,
                       int patternId, bool reevaluate, bool keepElse)
    : BAPFlowControl(patternId)
    , m_condition(condition)
    , m_trueStatement(trueStatement)
    , m_falseStatement(falseStatement)
    , m_activeStatement(0)
    , m_activeStatementDebug(0)
    , m_reevaluate(reevaluate)
    , m_keepElse(keepElse)
{
    condition->m_owner = this;
    if (m_trueStatement)
        m_trueStatement->m_owner = this;
    if (m_falseStatement)
        m_falseStatement->m_owner = this;
}

// ELF 0x1E3690 (D2; D1 0x1E3720, D0 0x1E37B0).
BAPFlowCIf::~BAPFlowCIf()
{
    if (m_condition)
        delete m_condition;
    if (m_trueStatement)
        delete m_trueStatement;
    if (m_falseStatement)
        delete m_falseStatement;
}

// ELF 0x1E3850 (77 instructions); the rules are in the header. Always returns 1.
bool BAPFlowCIf::execute(Bot* bot, bool& ok, bool& finished)
{
    bool test;
    if (!m_reevaluate)
        test = (m_activeStatement == 0);
    else if (m_keepElse && m_activeStatement == m_falseStatement)
        test = (m_activeStatement == 0);
    else
        test = true;

    if (test)
    {
        m_activeStatement = m_condition->evaluate(bot) ? m_trueStatement : m_falseStatement;
        m_activeStatementDebug = m_activeStatement;
        if (m_activeStatement == 0)
        {
            ok = true;
            finished = true;
            return true;
        }
    }

    // BotMain::getPlanInterpreter (non-virtual, 0x18A780) on the Bot*, call at 0x1E38EA.
    static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(m_activeStatement, bot, ok, finished);
    if (finished)
        m_activeStatement = 0;
    return true;
}

// ELF 0x1E3950: the union of both branches' controls.
unsigned long long BAPFlowCIf::getControls()
{
    unsigned long long controls = 0;
    if (m_trueStatement)
        controls = m_trueStatement->getControls();
    if (m_falseStatement)
        controls |= m_falseStatement->getControls();
    return controls;
}

// ELF 0x1E3F70 (emitted after the TU's initialiser).
BAPStatementType BAPFlowCIf::getId() const
{
    return BAPSTIf;
}

// ELF 0x1E39B0.
bool BAPFlowCIf::hasDefault()
{
    return true;
}

// ELF 0x1E3A10.
void BAPFlowCIf::debug(BotScriptDebugger::DebugLevel, std::string& text)
{
    if (m_activeStatementDebug == 0)
    {
        text = "If, now about to test condition...";
        return;
    }
    const std::string branch(m_activeStatementDebug == m_trueStatement ? "Then" : "Else");
    std::string line = "If, currently executing " + branch;
    line.append(" block...");
    text = line;
}

// ELF 0x1E3B60 (192 instructions). With +0x28 set, a tabulated "/*Force update*/" line first
// ("/*Force update, but freeze false-statement*/" with +0x29). Then tabulate, os << getId() << "(",
// "[true]"/"[false]" when a branch is active (+0x20 == +0x18 picks "[true]"), *condition when set,
// ")" endl; '{', the true branch one tab level deeper when set, '}'; and for a false branch
// (+0x1C) "else", '{', the branch one level deeper, '}', each on its own tabulated line.
void BAPFlowCIf::streamSelf(std::ostream& os) const
{
    IBotManager* const manager = IBotManager::instance;
    if (m_reevaluate)
    {
        manager->tabulate(os);
        os << (m_keepElse ? "/*Force update, but freeze false-statement*/" : "/*Force update*/") << std::endl;
    }
    manager->tabulate(os);
    os << getId() << "(";
    if (m_activeStatement)
        os << (m_activeStatement == m_trueStatement ? "[true]" : "[false]");
    if (m_condition)
        os << *m_condition;
    os << ")" << std::endl;
    manager->tabulate(os);
    os << '{' << std::endl;
    if (m_trueStatement)
    {
        manager->increaseTabLevel();
        os << *m_trueStatement;
        manager->decreaseTabLevel();
    }
    manager->tabulate(os);
    os << '}' << std::endl;
    if (m_falseStatement)
    {
        manager->tabulate(os);
        os << "else" << std::endl;
        manager->tabulate(os);
        os << '{' << std::endl;
        manager->increaseTabLevel();
        os << *m_falseStatement;
        manager->decreaseTabLevel();
        manager->tabulate(os);
        os << '}' << std::endl;
    }
}

// ELF 0x1E39C0.
BAPCondition* BAPFlowCIf::getCondition() const
{
    return m_condition;
}

// ELF 0x1E39D0.
BAPStatement* BAPFlowCIf::getTrueStatement() const
{
    return m_trueStatement;
}

// ELF 0x1E39E0.
BAPStatement* BAPFlowCIf::getFalseStatement() const
{
    return m_falseStatement;
}

// ELF 0x1E39F0.
BAPStatement* BAPFlowCIf::getActiveStatement() const
{
    return m_activeStatement;
}

// ELF 0x1E3A00.
BAPStatement* BAPFlowCIf::getActiveStatementDebug() const
{
    return m_activeStatementDebug;
}

} // namespace ai
} // namespace hfe
} // namespace dice
