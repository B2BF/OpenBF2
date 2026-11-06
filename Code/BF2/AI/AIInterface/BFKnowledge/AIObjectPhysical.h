// Exposes health, mass, armour and pathfinding occupancy for an AI object.
// init assigns the cached health and physics node; construction alone does not initialize them.
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_AIOBJECTPHYSICAL_H
#define AI_AIINTERFACE_BFKNOWLEDGE_AIOBJECTPHYSICAL_H

#include "IAIObjectPlugIn.h"

namespace dice {
namespace hfe {

namespace world { class IPhysicsNode; }

namespace ai {

class AITemplatePhysical;

class AIObjectPhysical : public IAIObjectPhysical
{
public:
    AIObjectPhysical(IAIObject* object, AITemplatePhysical* aiTemplate);
    virtual ~AIObjectPhysical();

    virtual int getId() const;                                  // +0x08
    virtual void createInformationPlugIn(unsigned int handle, bool real);   // +0x0C
    virtual void init();                                        // +0x10
    virtual void update();                                      // +0x14, - empty

    virtual float getHealth();                                  // +0x18
    virtual float getMaxHealth() const;                         // +0x1C
    virtual float getMass() const;                              // +0x20
    virtual float getVolume() const;                            // +0x24
    virtual float getArmour(const Vec3& direction) const;       // +0x28
    virtual float getBestArmour() const;                        // +0x2C
    virtual const LocalBoundingBox* getLocalBoundingBox() const;    // +0x30
    // Returns a new transformed box; the caller owns it.
    virtual BoundingBox* getCurrentBoundingBox();               // +0x34
    virtual BoundingBox* getBoundingBox(const Mat4& transformation);   // +0x38
    virtual float getRadius() const;                            // +0x3C
    virtual Vec3 getSphereLocalOffset() const;                  // +0x40
    virtual float getSmallestRadius() const;                    // +0x44
    virtual float getSubSpheres2d(std::vector<Vec2>& out) const;    // +0x48
    virtual int getStrType() const;                             // +0x4C
    virtual bool isTouchingTerrain(Vec3& out) const;            // +0x50
    virtual bool isInAir() const;                               // +0x54
    virtual bool isInWater() const;                             // +0x58
    virtual int getMaterial() const;                            // +0x5C
    virtual float getManDownTimeLeft() const;                   // +0x60
    virtual float getMaxManDownTime() const;                    // +0x64
    virtual bool getManDown() const;                            // +0x68
    virtual bool doesAffectPathfinding() const;                 // +0x6C
    virtual int getMapPosId() const;                            // +0x70
    virtual bool enablePhysics(int side);                       // +0x74
    virtual bool disablePhysics(int side);                      // +0x78
    virtual bool hasPhysicsEnabled();                           // +0x7C

private:
    /* +0x08 */ AITemplatePhysical* m_template;
    /* +0x0C */ float m_maxHealth;
    /* +0x10 */ world::IPhysicsNode* m_physicsNode;
    /* +0x14 */ int m_mapPosId;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_AIOBJECTPHYSICAL_H
