// Strategic grid square. Evidence: libaidll.so (ia-32) 0x3663F0..0x366508.

#include "SAIMapSquare.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x366460, __tcf_0 0x3664C0, _GLOBAL__I 0x3664F0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x3663F0 (C2; C1 0x366420): only the tree is constructed.
SAIMapSquare::SAIMapSquare()
{
}

// ELF 0x366450.
const TemperatureTree& SAIMapSquare::getTemperatureTree() const
{
    return m_temperatureTree;
}

} // namespace ai
} // namespace hfe
} // namespace dice
