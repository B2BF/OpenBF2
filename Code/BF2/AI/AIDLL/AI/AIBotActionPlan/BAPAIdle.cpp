// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

#include "BAPAIdle.h"

#include "BAPCondition.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D28C0 (C1; C2 0x1D2810 region): the condition's owner is set unconditionally.
BAPAIdle::BAPAIdle(BAPCondition* condition, unsigned long long controls, int patternId)
{
    m_condition = condition;
    condition->setOwner(this);
    m_patternId = patternId;
    m_controls = controls;
}

// ELF 0x1D2990 (D0 0x1D29F0): deletes the condition.
BAPAIdle::~BAPAIdle()
{
    if (m_condition)
        delete m_condition;
}

// ELF 0x1D2A60
bool BAPAIdle::execute(Bot* bot, bool& ok, bool& failed)
{
    ok = true;
    failed = m_condition->evaluate(bot);
    return true;
}

// ELF 0x1D2AA0
unsigned long long BAPAIdle::getControls()
{
    return m_controls;
}

// ELF 0x1D2B10 (weak)
BAPStatementType BAPAIdle::getId() const
{
    return BAPSTIdle;   // 13
}

// ELF 0x1D2AB0
bool BAPAIdle::hasDefault()
{
    return true;
}

// ELF 0x1D2AC0: every level.
void BAPAIdle::debug(BotScriptDebugger::DebugLevel, std::string& text)
{
    text = "Idling...";
}

} // namespace ai
} // namespace hfe
} // namespace dice
