// BAPCombinerParallel: recovered by package A-core from libaidll.so (TU BAPCombinerParallel.cpp,
// 0x1E4910..0x1E6150). Every body carries its ELF address.

// static init: ELF 0x1E5C20 (__tcf_0 0x1E5C80, _GLOBAL__I 0x1E5CB0): <iostream>'s Init object only.
#include <iostream>

#include "BAPCombinerParallel.h"

#include "../AIBot/BotMain.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"
#include "../Main/IBotManager.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E4910 (C2; C1 0x1E4AA0). The controls are zeroed again and the list cleared in the body.
BAPCombinerParallel::BAPCombinerParallel(int patternId, int debugId, bool neverFinish)
    : BAPCombiner(patternId, true)
    , m_debugId(debugId)
    , m_neverFinish(neverFinish)
{
    m_controls = 0;
    m_statements.clear();
}

// ELF 0x1E4B70 (C2; C1 0x1E4C50).
BAPCombinerParallel::BAPCombinerParallel(int patternId, bool waitForChild, int debugId, bool neverFinish)
    : BAPCombiner(patternId, waitForChild)
    , m_debugId(debugId)
    , m_neverFinish(neverFinish)
{
    m_controls = 0;
    m_statements.clear();
}

// ELF 0x1E4D30 (C2; C1 0x1E4F10). Takes ownership; the controls are the union of the children's.
BAPCombinerParallel::BAPCombinerParallel(std::list<BAPStatement*>& statements, int patternId, int debugId,
                                         bool neverFinish)
    : BAPCombiner(patternId, true)
    , m_debugId(debugId)
    , m_neverFinish(neverFinish)
{
    m_controls = 0;
    m_statements = statements;
    for (std::list<BAPStatement*>::iterator it = m_statements.begin(); it != m_statements.end(); ++it)
    {
        (*it)->m_owner = this;
        m_controls |= (*it)->getControls();
        m_isFinished.insert(m_isFinished.end(), false);
    }
}

// ELF 0x1E50F0 (C2; C1 0x1E52E0).
BAPCombinerParallel::BAPCombinerParallel(std::list<BAPStatement*>& statements, int patternId, bool waitForChild,
                                         int debugId, bool neverFinish)
    : BAPCombiner(patternId, waitForChild)
    , m_debugId(debugId)
    , m_neverFinish(neverFinish)
{
    m_controls = 0;
    m_statements = statements;
    for (std::list<BAPStatement*>::iterator it = m_statements.begin(); it != m_statements.end(); ++it)
    {
        (*it)->m_owner = this;
        m_controls |= (*it)->getControls();
        m_isFinished.insert(m_isFinished.end(), false);
    }
}

// ELF 0x1E54D0 (D2; D1 0x1E55C0, D0 0x1E56B0). Deletes the children (slot +8).
BAPCombinerParallel::~BAPCombinerParallel()
{
    for (std::list<BAPStatement*>::iterator it = m_statements.begin(); it != m_statements.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

// ELF 0x1E57B0 (108 instructions); the rules are in the header. Always returns 1.
bool BAPCombinerParallel::execute(Bot* bot, bool& ok, bool& finished)
{
    bool childFinished = false;
    bool childOk = true;
    finished = m_waitForChild;
    ok = true;

    // BotMain::getPlanInterpreter (non-virtual, 0x18A780) on the Bot*, call at 0x1E57FF.
    PlanInterpreter* interpreter = static_cast<BotMain*>(bot)->getPlanInterpreter();
    std::list<bool>::iterator finishedFlag = m_isFinished.begin();   // never advanced (retail)

    for (m_current = m_statements.begin(); m_current != m_statements.end(); ++m_current)
    {
        (*m_current)->getControls();   // called, result unused (0x1E5858)
        interpreter->execute(*m_current, bot, childOk, childFinished);
        ok = ok && childOk;
        if (m_waitForChild)
            finished = finished && childFinished;
        else
            finished = finished || childFinished;
        *finishedFlag = childFinished;
    }

    if (m_neverFinish)
        finished = false;
    return true;
}

// ELF 0x1E5940.
unsigned long long BAPCombinerParallel::getControls()
{
    return m_controls;
}

// ELF 0x1E6140.
BAPStatementType BAPCombinerParallel::getId() const
{
    return BAPSTParallel;
}

// ELF 0x1E5950.
bool BAPCombinerParallel::hasDefault()
{
    return true;
}

// ELF 0x1E5A70. "Parallell" is the image's spelling.
void BAPCombinerParallel::streamSelf(std::ostream& os) const
{
    IBotManager::instance->tabulate(os);
    os << "Parallell combiner";
    if (m_debugId != -1)
        os << "(" << m_debugId << ")";
    os << std::endl;

    IBotManager::instance->tabulate(os);
    os << '{' << std::endl;
    IBotManager::instance->increaseTabLevel();
    for (std::list<BAPStatement*>::const_iterator it = m_statements.begin(); it != m_statements.end(); ++it)
        os << **it;
    IBotManager::instance->decreaseTabLevel();
    IBotManager::instance->tabulate(os);
    os << '}' << std::endl;
}

// ELF 0x1E5960. A null statement is ignored.
void BAPCombinerParallel::addStatement(BAPStatement* statement)
{
    if (statement == 0)
        return;
    statement->m_owner = this;
    m_statements.insert(m_statements.end(), statement);
    m_controls |= statement->getControls();
    m_isFinished.insert(m_isFinished.end(), false);
}

// ELF 0x1E5A60.
const std::list<bool>& BAPCombinerParallel::getIsFinished() const
{
    return m_isFinished;
}

} // namespace ai
} // namespace hfe
} // namespace dice
