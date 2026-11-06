// BAPCombinerSerial: recovered by package A-core from libaidll.so (TU BAPCombinerSerial.cpp,
// 0x1E6150..0x1E6CF0). Every body carries its ELF address.

// static init: ELF 0x1E6C30 (__tcf_0 0x1E6C90, _GLOBAL__I 0x1E6CC0): <iostream>'s Init object only.
#include <iostream>

#include "BAPCombinerSerial.h"

#include "../AIBot/BotMain.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"
#include "../Main/IBotManager.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E6150 (C2; C1 0x1E6200). The body clears the (new, empty) list, then rewinds.
BAPCombinerSerial::BAPCombinerSerial(int patternId, int debugId)
    : BAPCombiner(patternId, true)
    , m_debugId(debugId)
{
    m_statements.clear();
    m_current = m_statements.begin();
}

// ELF 0x1E62B0 (C2; C1 0x1E6360).
BAPCombinerSerial::BAPCombinerSerial(int patternId, bool waitForChild, int debugId)
    : BAPCombiner(patternId, waitForChild)
    , m_debugId(debugId)
{
    m_statements.clear();
    m_current = m_statements.begin();
}

// ELF 0x1E6410 (C2; C1 0x1E6500). Copies the list and takes ownership of its statements.
BAPCombinerSerial::BAPCombinerSerial(std::list<BAPStatement*>& statements, int patternId, int debugId)
    : BAPCombiner(patternId, true)
    , m_debugId(debugId)
{
    m_statements = statements;
    m_current = m_statements.begin();
    for (std::list<BAPStatement*>::iterator it = m_statements.begin(); it != m_statements.end(); ++it)
        (*it)->mpOwner = this;
}

// ELF 0x1E65F0 (D2; D1 0x1E66B0, D0 0x1E6770). Deletes the children (slot +8).
BAPCombinerSerial::~BAPCombinerSerial()
{
    for (std::list<BAPStatement*>::iterator it = m_statements.begin(); it != m_statements.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

// ELF 0x1E6840 (84 instructions). The child's own result is ignored; this always returns 1.
bool BAPCombinerSerial::execute(Bot* bot, bool& ok, bool& finished)
{
    if (m_current == m_statements.end())
    {
        ok = true;
        finished = true;
        m_current = m_statements.begin();
        return true;
    }

    // BotMain::getPlanInterpreter (non-virtual, 0x18A780) on the Bot*, call at 0x1E68BA.
    static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(*m_current, bot, ok, finished);

    if (m_waitForChild)
    {
        if (!finished)
            return true;
        ++m_current;
        if (m_current != m_statements.end())
        {
            finished = false;
            return true;
        }
    }
    else
    {
        ++m_current;
        if (!finished && m_current != m_statements.end())
            return true;
    }

    finished = true;
    m_current = m_statements.begin();
    return true;
}

// ELF 0x1E6970: the current child's controls. Retail reads through m_current without an end()
// check (only an empty combiner can be at end() here, see execute).
unsigned long long BAPCombinerSerial::getControls()
{
    return (*m_current)->getControls();
}

// ELF 0x1E6CE0.
BAPStatementType BAPCombinerSerial::getId() const
{
    return BAPSTSerial;
}

// ELF 0x1E6990.
bool BAPCombinerSerial::hasDefault()
{
    return true;
}

// ELF 0x1E6A20. IBotManager::tabulate/increaseTabLevel/decreaseTabLevel are +0x88/+0x80/+0x84.
void BAPCombinerSerial::streamSelf(std::ostream& os) const
{
    IBotManager::instance->tabulate(os);
    os << "Serial combiner";
    if (m_debugId != -1)
        os << "(" << m_debugId << ")";
    os << std::endl;

    IBotManager::instance->tabulate(os);
    os << '{' << std::endl;
    IBotManager::instance->increaseTabLevel();
    for (std::list<BAPStatement*>::const_iterator it = m_statements.begin(); it != m_statements.end(); ++it)
    {
        if (std::list<BAPStatement*>::const_iterator(m_current) == it)
        {
            IBotManager::instance->tabulate(os);
            os << "/*Active serial statement*/" << std::endl;
        }
        os << **it;
    }
    IBotManager::instance->decreaseTabLevel();
    IBotManager::instance->tabulate(os);
    os << '}' << std::endl;
}

// ELF 0x1E69A0. A null statement is ignored; adding rewinds.
void BAPCombinerSerial::addStatement(BAPStatement* statement)
{
    if (statement == 0)
        return;
    statement->mpOwner = this;
    m_statements.insert(m_statements.end(), statement);
    m_current = m_statements.begin();
}

} // namespace ai
} // namespace hfe
} // namespace dice
