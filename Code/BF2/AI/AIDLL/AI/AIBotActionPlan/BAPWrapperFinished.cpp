// Owns a child statement and overrides its finished result after interpreter execution.

// static init: ELF 0x1FA260 (__tcf_0 0x1FA2C0, _GLOBAL__I 0x1FA2F0): <iostream>'s Init object only.
#include <iostream>

#include "BAPWrapperFinished.h"

#include "../AIBot/BotMain.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"
#include "../Main/IBotManager.h"
#include "../Main/AIStreamOperators.h"
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F9F00 (C1; C2 0x1F9E90)
BAPWrapperFinished::BAPWrapperFinished(BAPStatement* statement, bool finished)
    : BAPStatement(UninitializedPatternId()), m_statement(statement), m_finished(finished)
{
    if (m_statement)
        m_statement->setParent(this);
}

// ELF 0x1F9FE0 (D1; D2 0x1F9F70, D0 0x1FA050)
BAPWrapperFinished::~BAPWrapperFinished()
{
    delete m_statement;
}

// ELF 0x1FA0C0
bool BAPWrapperFinished::execute(Bot* bot, bool& success, bool& finished)
{
    if (m_statement)
        static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(m_statement, bot, success, finished);
    finished = m_finished;
    return true;
}

// ELF 0x1FA130
BAPStatementType BAPWrapperFinished::getId() const
{
    return BAPSTFinishedWrapper;
}

// ELF 0x1FA140
bool BAPWrapperFinished::hasDefault()
{
    return true;
}

// ELF 0x1FA150
void BAPWrapperFinished::streamSelf(std::ostream& out) const
{
    IBotManager::instance->tabulate(out);
    out << getId() << std::endl;
    IBotManager::instance->tabulate(out);
    out << '{' << std::endl;
    if (m_statement) {
        IBotManager::instance->increaseTabLevel();
        out << *m_statement;
        IBotManager::instance->decreaseTabLevel();
    }
    IBotManager::instance->tabulate(out);
    out << '}' << std::endl;
}

} // namespace ai
} // namespace hfe
} // namespace dice
