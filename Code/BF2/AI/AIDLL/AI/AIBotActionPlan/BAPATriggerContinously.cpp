// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E1FE0 (__tcf_0 0x1E2040, _GLOBAL__I 0x1E2070): <iostream>'s Init object only.
#include <iostream>

#include "BAPATriggerContinously.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../Main/IBotManager.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E1B20 (C1; C2 0x1E1A40 identical)
BAPATriggerContinously::BAPATriggerContinously(io::PlayerInputMap input, BAPCondition* start,
                                               BAPCondition* stop, BAPCondition* abort,
                                               unsigned long long controls, int tag, double duration)
{
    m_startCondition = start;
    m_input = input;
    m_stopCondition = stop;
    m_started = false;
    m_abortCondition = abort;
    start->setOwner(this);
    if (m_stopCondition)
        m_stopCondition->setOwner(this);
    if (m_abortCondition)
        m_abortCondition->setOwner(this);
    m_patternId = tag;
    m_controls = controls;
    m_endTime = bapinline::gameTime() + duration;
}

// ELF 0x1E1C90 (D1; D2 0x1E1C00, D0 0x1E1D20): deletes start, stop, abort (virtual D0).
BAPATriggerContinously::~BAPATriggerContinously()
{
    if (m_startCondition)
        delete m_startCondition;
    if (m_stopCondition)
        delete m_stopCondition;
    if (m_abortCondition)
        delete m_abortCondition;
}

// ELF 0x1E1DB0: runs only through EntryTriggerContinously.
bool BAPATriggerContinously::execute(Bot*, bool& ok, bool& failed)
{
    ok = false;
    failed = true;
    return false;
}

// ELF 0x1E2090
unsigned long long BAPATriggerContinously::getControls()
{
    return m_controls;
}

// ELF 0x1E20A0
BAPStatementType BAPATriggerContinously::getId() const
{
    return BAPSTTriggerContinously;   // 12
}

// ELF 0x1E20B0
bool BAPATriggerContinously::hasDefault()
{
    return false;
}

// ELF 0x1E1DD0
void BAPATriggerContinously::streamSelf(std::ostream& out) const
{
    IBotManager* bm = IBotManager::instance;
    bm->tabulate(out);
    bapinline::streamInput(bapinline::streamType(out, getId()) << "(", m_input) << ")" << std::endl;
    bm->tabulate(out);
    out << '[' << std::endl;
    bm->increaseTabLevel();
    bm->tabulate(out);
    bapinline::streamCondition(out << "StartCondition:  ", m_startCondition) << std::endl;
    if (m_stopCondition) {
        bm->tabulate(out);
        bapinline::streamCondition(out << "StopCondition:   ", m_stopCondition) << std::endl;
    }
    if (m_abortCondition) {
        bm->tabulate(out);
        bapinline::streamCondition(out << "AbortCondition:   ", m_abortCondition) << std::endl;
    }
    bm->decreaseTabLevel();
    bm->tabulate(out);
    out << ']' << std::endl;
}

} // namespace ai
} // namespace hfe
} // namespace dice
