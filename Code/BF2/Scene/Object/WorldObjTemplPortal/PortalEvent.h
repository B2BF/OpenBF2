// Portal transition payload. Header placement beside PortalManager is [INFERRED].
#ifndef SCENE_OBJECT_WORLDOBJTEMPLPORTAL_PORTALEVENT_H
#define SCENE_OBJECT_WORLDOBJTEMPLPORTAL_PORTALEVENT_H

#include "PortalManager.h"

namespace dice { namespace hfe { namespace world {

class PortalEvent
{
public:
    // Declaration-only construction/destruction; the AI callback only borrows events.
    PortalEvent(PortalEventType type, IObject* passingObject, IObject* portalObject,
                unsigned int portalId);
    virtual ~PortalEvent();

    PortalEventType getType() const;
    unsigned int getPortalId() const;

private:
    // ELF 0x8325C90/0x8325CC0 establish these fields and their ordering.
    PortalEventType m_type;     // x86 +0x04
    IObject* m_passingObject;   // x86 +0x08
    IObject* m_portalObject;    // x86 +0x0C
    unsigned int m_portalId;   // x86 +0x10
};

} } }
#endif
