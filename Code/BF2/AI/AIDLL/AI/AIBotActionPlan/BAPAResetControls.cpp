// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1DE790 (__tcf_0 0x1DE7F0, _GLOBAL__I 0x1DE820): <iostream>'s Init object only.
#include <iostream>

#include "BAPAResetControls.h"

#include "BAPActionInline.h"
#include "../Main/IBotManager.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DE3D0 (C1; C2 0x1DE350): group form, no single input, controls = 0.
BAPAResetControls::BAPAResetControls(bool resetMove, bool resetLook, bool resetTrigger, int tag)
{
    m_resetMove = resetMove;
    m_resetLook = resetLook;
    m_input = static_cast<io::PlayerInputMap>(0);
    m_resetTrigger = resetTrigger;
    m_controls = 0;
    m_patternId = tag;
}

// ELF 0x1DE4D0 (C1; C2 0x1DE450): one input. `movl $1,%eax; shll %cl,%eax` then `sarl $31` into
// the high word: the 32-bit shift (count & 31) sign-extended to 64 bits, kept as in the image.
BAPAResetControls::BAPAResetControls(io::PlayerInputMap input, int tag)
{
    m_input = input;
    m_resetMove = false;
    m_resetLook = false;
    m_resetTrigger = false;
    m_patternId = tag;
    const int bit = static_cast<int>(1u << (static_cast<unsigned int>(input) & 31u));
    m_controls = static_cast<unsigned long long>(static_cast<long long>(bit));
}

// ELF 0x1DE590 (D1; D2 0x1DE550, D0 0x1DE5D0)
BAPAResetControls::~BAPAResetControls()
{
}

// ELF 0x1DE660
bool BAPAResetControls::execute(Bot*, bool& ok, bool& failed)
{
    ok = true;
    failed = true;
    return true;
}

// ELF 0x1DE620
unsigned long long BAPAResetControls::getControls()
{
    return m_controls;
}

// ELF 0x1DE840
BAPStatementType BAPAResetControls::getId() const
{
    return BAPSTResetControls;   // 14
}

// ELF 0x1DE850
bool BAPAResetControls::hasDefault()
{
    return false;
}

// ELF 0x1DE680: "<tabs><id>(" then the single input or the group labels, then ")" endl.
void BAPAResetControls::streamSelf(std::ostream& out) const
{
    IBotManager::instance->tabulate(out);
    bapinline::streamType(out, getId()) << "(";
    if (hasSingleInput()) {
        bapinline::streamInput(out, getInput());
    } else {
        if (m_resetMove)
            out << "Move  ";
        if (m_resetLook)
            out << "Look  ";
        if (m_resetTrigger)
            out << "Trigger";
    }
    out << ")" << std::endl;
}

// ELF 0x1DE630: `(+0x0C | +0x08) != 0`
bool BAPAResetControls::hasSingleInput() const
{
    return m_controls != 0;
}

// ELF 0x1DE650
io::PlayerInputMap BAPAResetControls::getInput() const
{
    return m_input;
}

} // namespace ai
} // namespace hfe
} // namespace dice
