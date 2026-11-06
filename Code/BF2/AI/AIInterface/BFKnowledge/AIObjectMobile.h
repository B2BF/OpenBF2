// Exposes movement state and pathfinding information for an AI object.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_AIOBJECTMOBILE_H
#define AI_AIINTERFACE_BFKNOWLEDGE_AIOBJECTMOBILE_H

#include "IAIObjectPlugIn.h"

#include "../../../Scene/Object/WorldObjTemplPortal/PortalManager.h"   // world::IPortalEventCallback

namespace dice {
namespace hfe {

namespace world { class IPhysicsNode; }

namespace ai {

class AITemplateMobile;

class AIObjectMobile : public IAIObjectMobile
{
public:
    // PortalEventCallback - ELF vtable : one slot. Built inline by the constructor
    // (`new`, : vptr + owner) and deleted by the destructor.
    class PortalEventCallback : public world::IPortalEventCallback
    {
    public:
        explicit PortalEventCallback(AIObjectMobile* owner) : m_owner(owner) {}
        virtual void handleEvent(world::PortalEvent* event);

    private:
        /* +0x04 */ AIObjectMobile* m_owner;
    };

    AIObjectMobile(IAIObject* object, AITemplateMobile* aiTemplate);
    virtual ~AIObjectMobile();

    virtual int getId() const;                                  // +0x08
    virtual void createInformationPlugIn(unsigned int handle, bool real);   // +0x0C
    virtual void init();                                        // +0x10
    virtual void update();                                      // +0x14, - empty

    virtual const Vec3& getSpeed();                             // +0x18
    virtual const Vec3& getRotationalSpeed();                   // +0x1C
    virtual float getSpeedMagnitude();                          // +0x20
    virtual int getVehicleNumber() const;                       // +0x24
    virtual float getMaxSpeed() const;                          // +0x28
    virtual float getTurnRadius() const;                        // +0x2C
    virtual float getDrag() const;                              // +0x30
    virtual float getCoverSearchRadius() const;                 // +0x34
    virtual float getLodHeight() const;                         // +0x38
    virtual bool isTurnable() const;                            // +0x3C
    virtual float getSoundSphereRadius() const;                 // +0x40
    virtual float getSoundSphereRadiusMin() const;              // +0x44
    virtual float getSoundSphereRadiusMax() const;              // +0x48
    virtual float getHearingProbabilityMax() const;             // +0x4C
    virtual float getHearingProbabilityMin() const;             // +0x50
    virtual float getAvoidCollisionLookAhead() const;           // +0x54
    virtual IAIObject* getSector() const;                       // +0x58
    virtual int getLastPassedPortal() const;                    // +0x5C
    virtual void positionChanged();                             // +0x60
    virtual float getSprintStamina() const;                     // +0x64

private:
    friend class PortalEventCallback;

    /* +0x08 */ int m_unknown08;                                // [UNRESOLVED] ctor 0
    /* +0x0C */ int m_unknown0C;                                // [UNRESOLVED] ctor 0
    /* +0x10 */ int m_unknown10;                                // [UNRESOLVED] ctor 0
    /* +0x14 */ AITemplateMobile* m_template;
    /* +0x18 */ int m_lastPassedPortal;
    /* +0x1C */ PortalEventCallback* m_portalCallback;
    /* +0x20 */ world::IObject* m_worldObject;
    /* +0x24 */ world::IPhysicsNode* m_physicsNode;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_AIOBJECTMOBILE_H
