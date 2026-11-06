// Bot-script debugger: per-statement debug levels for two debug places and a statement stack.
// Evidence: libaidll.so (ia-32) 0x2A83E0..0x2A90DF and 0x2A95D0. Placement attested by the __FILE__
// string "AI/AIDLL/AI/AIPlanInterpreter/BAPDebugger.cpp".

// static init: ELF 0x2A9030 (__tcf_0 0x2A9090, _GLOBAL__I 0x2A90C0): <iostream>'s Init object only.
#include <iostream>

#include "BAPDebugger.h"

#include <algorithm>
#include <string>

#include "../../../../Core/Debug.h"
#include "../AIBotActionPlan/BAPStatement.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2A8560 (C1; C2 0x2A83E0).
BAPDebugger::BAPDebugger()
    : m_debugLevel(static_cast<BotScriptDebugger::DebugLevel>(3))
{
}

// ELF 0x2A8720 (D1; D2 0x2A8620, D0 0x2A8820). The members are destroyed in reverse order.
BAPDebugger::~BAPDebugger()
{
}

// ELF 0x2A8930. Place 0 uses the first map, place 1 the second; any other place does nothing.
void BAPDebugger::debug(BotScriptDebugger::DebugLevel level, BotScript* script,
                        BotScriptDebugger::DebugPlace place, int value)
{
    switch (place)
    {
    case 0:
        internalDebug(level, script, value, m_placeZeroStatements);
        break;
    case 1:
        internalDebug(level, script, value, m_placeOneStatements);
        break;
    default:
        break;
    }
}

// ELF 0x2A8C90.
void BAPDebugger::clear()
{
    clearDebugStatement();
    clearStack();
}

// ELF 0x2A95D0.
int BAPDebugger::getImplementation()
{
    return 1;
}

// ELF 0x2A89B0. Same place dispatch as debug().
void BAPDebugger::addDebugStatement(BotScriptDebugger::DebugLevel level, BAPStatement* statement,
                                    BotScriptDebugger::DebugPlace place, int value)
{
    switch (place)
    {
    case 0:
        internalAddStatement(level, statement, value, m_placeZeroStatements);
        break;
    case 1:
        internalAddStatement(level, statement, value, m_placeOneStatements);
        break;
    default:
        break;
    }
}

// ELF 0x2A8A30.
void BAPDebugger::clearDebugStatement()
{
    m_placeZeroStatements.clear();
    m_placeOneStatements.clear();
}

// ELF 0x2A8AE0.
void BAPDebugger::pushStatement(BAPStatement* statement)
{
    m_stack.insert(m_stack.begin(), statement);
}

// ELF 0x2A8B30. Answers 0 on an empty stack (counted with size()). When the back is not the
// statement, it is searched from the front; the search result is dereferenced even when it is
// end() (shipped defect). On a match the back is popped, not the matching element (shipped).
int BAPDebugger::popStatement(BAPStatement* statement)
{
    if (m_stack.size() == 0)
        return 0;
    if (m_stack.back() != statement)
    {
        if (*std::find(m_stack.begin(), m_stack.end(), statement) != statement)
            return 0;
    }
    m_stack.pop_back();
    return 1;
}

// ELF 0x2A8C70.
void BAPDebugger::setDebugLevel(BotScriptDebugger::DebugLevel level)
{
    m_debugLevel = level;
}

// ELF 0x2A8C80.
int BAPDebugger::getDebugLevel()
{
    return m_debugLevel;
}

// ELF 0x2A8CD0.
void BAPDebugger::clearStack()
{
    m_stack.clear();
}

// ELF 0x2A8D00. A registered statement whose value filter is -1 or matches prints itself at the
// lowest of the requested, registered and debugger levels; level 2 output goes to the error log.
void BAPDebugger::internalDebug(BotScriptDebugger::DebugLevel level, BotScript* script, int value,
                                std::map<BAPStatement*, std::pair<BotScriptDebugger::DebugLevel, int> >& statements)
{
    std::string text;
    BAPStatement* statement = static_cast<BAPStatement*>(script);
    StatementMap::iterator found = statements.find(statement);
    if (found == statements.end())
        return;
    if (found->second.second != -1 && found->second.second != value)
        return;
    BotScriptDebugger::DebugLevel effective = level;
    if (found->second.first <= level)
        effective = found->second.first;
    if (effective > m_debugLevel)
        effective = m_debugLevel;
    statement->debug(effective, text);
    if (effective == 2)
        Debug(DebugTypeError, "AI/AIDLL/AI/AIPlanInterpreter/BAPDebugger.cpp", 248, "") << text;
}

// ELF 0x2A8F00. Registers a statement once; an existing entry is left unchanged.
void BAPDebugger::internalAddStatement(BotScriptDebugger::DebugLevel level, BAPStatement* statement,
                                       int value,
                                       std::map<BAPStatement*, std::pair<BotScriptDebugger::DebugLevel, int> >& statements)
{
    if (statements.find(statement) != statements.end())
        return;
    statements[statement] = std::make_pair(level, value);
}

} // namespace ai
} // namespace hfe
} // namespace dice
