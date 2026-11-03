// AI debug primitive base. Evidence: libaidll.so (ia-32) 0x3811C0..0x38133C.
// [INFERRED] Source placement.

#include "DrawPrimitive.h"

#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x3811C0 (C2; C1 0x381210).
DrawPrimitive::DrawPrimitive(float lifeTime, unsigned int colour)
    : m_creationTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime())
    , m_lifeTime(lifeTime)
    , m_colour(colour)
{
}

// ELF 0x381260 (D2; D1 0x381280, D0 0x3812A0).
DrawPrimitive::~DrawPrimitive()
{
}

// ELF 0x381320. The sum creation + lifetime is formed in x87 precision from the double and the
// float, then compared unsigned-above against `time`.
bool DrawPrimitive::expired(double time) const
{
    return time > m_creationTime + m_lifeTime;
}

// ELF 0x3812E0. Calls the out-of-line overload through the PLT.
bool DrawPrimitive::expired() const
{
    return expired(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

} // namespace ai
} // namespace hfe
} // namespace dice
