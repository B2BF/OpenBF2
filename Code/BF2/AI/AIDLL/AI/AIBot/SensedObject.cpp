// One sensed-object record. Evidence: libaidll.so 1.5.3153 (ia-32), see SensedObject.h.

#include "SensedObject.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1A29C0, __tcf_0 0x1A2A20, _GLOBAL__I 0x1A2A50).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1A2860 (C1; C2 0x1A28F0 is identical). The three Vec3 are zeroed, +0x3C gets -1000.0f
// (`flds 0x49CDC8; fstpl 0x3c`), +0x50 = 2, +0x54 / +0x5C = 0.0, +0x64 = 0. The flag word comes
// from the setne/incl/cmov chain: base = seen + 1, then +4 if sided, +8 and +16 if not seen.
SensedObject::SensedObject(unsigned int handle, bool seen, bool sided)
    : m_handle(handle)
{
    m_seenPosition.x = 0.0f;
    m_seenPosition.y = 0.0f;
    m_seenPosition.z = 0.0f;
    m_lastPosition.x = 0.0f;
    m_lastPosition.y = 0.0f;
    m_lastPosition.z = 0.0f;
    m_lastSeenPosition.x = 0.0f;
    m_lastSeenPosition.y = 0.0f;
    m_lastSeenPosition.z = 0.0f;
    m_lastAttackTime = -1000.0f;
    unsigned int flags = seen ? SENSE_SEEN : SENSE_HEARD;
    if (sided)
        flags |= SENSE_SIDED;
    if (!seen)
        flags |= SENSE_INDIRECT;
    if (!seen)
        flags |= SENSE_HEARD_NOW;
    m_lastSense = SENSE_SEEN;
    m_lastSeenTime = 0.0f;
    m_lastPositionTime = 0.0f;
    m_flags = flags;
    m_hasContact = false;
}

// ELF 0x1A2980 (D1; D2 0x1A2990): `push ebp; mov ebp,esp; pop ebp; ret`.
SensedObject::~SensedObject()
{
}

// ELF 0x1A29A0: `mov $7,%eax; L: dec %eax; jns L` - eight passes, no body left.
void SensedObject::initStrengths()
{
    for (int i = 7; i >= 0; --i)
    {
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
