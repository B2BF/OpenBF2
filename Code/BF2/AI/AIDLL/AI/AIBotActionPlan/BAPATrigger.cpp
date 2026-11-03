// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E1960 (__tcf_0 0x1E19C0, _GLOBAL__I 0x1E19F0): <iostream>'s Init object only.
#include <iostream>

#include "BAPATrigger.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../Main/IBotManager.h"

#include <ostream>
#include <sstream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E10F0 (C1; C2 0x1E0FE0 region)
BAPATrigger::BAPATrigger(io::PlayerInputMap input, BAPCondition* start, BAPCondition* stop,
                         unsigned long long controls, int tag, double duration, bool flag)
{
    m_startCondition = start;
    m_input = input;
    m_started = false;
    m_stopCondition = stop;
    m_flag = flag;
    if (start)
        start->setOwner(this);
    if (m_stopCondition)
        m_stopCondition->setOwner(this);
    m_patternId = tag;
    m_controls = controls;
    m_endTime = bapinline::gameTime() + duration;
}

// ELF 0x1E1240 (D1; D0 0x1E12C0): deletes both conditions.
BAPATrigger::~BAPATrigger()
{
    if (m_startCondition)
        delete m_startCondition;
    if (m_stopCondition)
        delete m_stopCondition;
}

// ELF 0x1E1340: runs only through EntryTrigger.
bool BAPATrigger::execute(Bot*, bool& ok, bool& failed)
{
    ok = false;
    failed = true;
    return false;
}

// ELF 0x1E1A10
unsigned long long BAPATrigger::getControls()
{
    return m_controls;
}

// ELF 0x1E1A20
BAPStatementType BAPATrigger::getId() const
{
    return BAPSTTrigger;   // 11
}

// ELF 0x1E1A30
bool BAPATrigger::hasDefault()
{
    return false;
}

// ELF 0x1E1360: any level but 0 writes "Pushed button with id '<input>'".
void BAPATrigger::debug(BotScriptDebugger::DebugLevel level, std::string& text)
{
    if (level != 0) {
        std::ostringstream os;
        bapinline::streamInput(os << "Pushed button with id '", m_input) << "'";
        text = os.str();
    }
}

// ELF 0x1E17A0
void BAPATrigger::streamSelf(std::ostream& out) const
{
    IBotManager* bm = IBotManager::instance;
    bm->tabulate(out);
    bapinline::streamInput(bapinline::streamType(out, getId()) << "(", m_input) << ")" << std::endl;
    bm->tabulate(out);
    out << '[' << std::endl;
    bm->increaseTabLevel();
    bm->tabulate(out);
    out << "StartCondition:  ";
    if (m_startCondition)
        bapinline::streamCondition(out, m_startCondition);
    out << std::endl;
    bm->tabulate(out);
    out << "StopCondition:   ";
    if (m_stopCondition)
        bapinline::streamCondition(out, m_stopCondition);
    out << std::endl;
    bm->decreaseTabLevel();
    bm->tabulate(out);
    out << ']' << std::endl;
}

} // namespace ai
} // namespace hfe
} // namespace dice
