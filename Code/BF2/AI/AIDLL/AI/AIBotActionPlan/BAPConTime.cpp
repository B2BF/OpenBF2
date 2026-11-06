// BAPConTime: recovered by package A-core from libaidll.so (TU without a FILE symbol,
// 0x1EACB0..0x1EAEF0). Every body carries its ELF address.

#include "BAPConTime.h"

#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EACB0 (C2; C1 0x1EAD50). The deadline is a double: -1.0f widened, or now + duration.
BAPConTime::BAPConTime(double duration, bool flag19, bool armed, bool fireFirst)
{
    if (fireFirst)
        m_deadline = -1.0f;
    else
        m_deadline = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime() + duration;
    m_duration = duration;
    m_armed = armed || fireFirst;
    m_flag19 = flag19;
}

// ELF 0x1EADF0 (D2/D1; D0 0x1EAE30): the vptr store only.
BAPConTime::~BAPConTime()
{
}

// ELF 0x1EAE70. `now < deadline` is the image's jb after fucomip (unordered: true -> not yet).
bool BAPConTime::evaluate(Bot*)
{
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (!m_armed)
    {
        m_armed = true;
        m_deadline = now + m_duration;
    }
    if (now < m_deadline)
        return false;
    m_armed = false;
    return true;
}

// ELF 0x1EAEE0.
BAPConditionType BAPConTime::getConditionType() const
{
    return BAPCTTime;
}

} // namespace ai
} // namespace hfe
} // namespace dice
