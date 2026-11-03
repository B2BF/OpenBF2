// ELF FILE symbol attests PortalEvent.cpp; directory placement is [INFERRED].
#include "PortalEvent.h"

namespace dice { namespace hfe { namespace world {

// ELF 0x8325D30.
PortalEventType PortalEvent::getType() const
{
    return m_type;
}

// ELF 0x8325D60.
unsigned int PortalEvent::getPortalId() const
{
    return m_portalId;
}

} } }
