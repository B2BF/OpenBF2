// Recovered from libaidll.so 1.5.3153 (0x2EA000..0x2EA0A1).

#include "IgnoreListPredicate.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2EA0B0, __tcf_0 0x2EA110, _GLOBAL__I 0x2EA140).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2EA000 / 0x2EA030: the vptr, then the list's address at +4.
IgnoreListPredicate::IgnoreListPredicate(std::vector<IAIObject*>& ignore)
    : m_ignore(&ignore)
{
}

// ELF 0x2EA060. Index walk; the size is read once, the data pointer on every step.
bool IgnoreListPredicate::predicate(IAIObject* object)
{
    const unsigned int count = static_cast<unsigned int>(m_ignore->size());
    for (unsigned int i = 0; i < count; ++i)
    {
        if (object == (*m_ignore)[i])
            return false;
    }
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
