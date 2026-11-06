// Placeholder physical plug-in that answers from values given at construction. It owns the
// bounding box and local bounding box it is given. The man-down slots (+21..+23) stay pure, so
// the class is abstract; nothing in libaidll.so constructs it or derives from it.
// Evidence: libaidll.so 1.5.3153 (ia-32) 0x2EFC50..0x2F018B, vtable 0x4BEEE0 (27 slots);
// FILE symbol IPIPhysicalDummy.cpp.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICALDUMMY_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICALDUMMY_H

#include "IPIPhysical.h"
#include "../../../../Math/LocalBoundingBox.h"

namespace dice {
namespace hfe {
namespace ai {

class IPIPhysicalDummy : public IPIPhysical
{
public:
    // Takes ownership of `boundingBox` and `localBoundingBox`. The modifier and the in-air and
    // in-water flags are left as allocated.
    IPIPhysicalDummy(Information* information, float health, BoundingBox* boundingBox,
                     LocalBoundingBox* localBoundingBox, float radius, float armour,
                     float bestArmour, int strengthType, float volume, float mass,
                     bool touchingTerrain, int material);
    virtual ~IPIPhysicalDummy();

    virtual void   setTime();                                           // +3
    virtual void   setTime(double time);                                // +4
    virtual float  getHealth() const { return m_health; }               // +5 ELF 0x2F0020
    virtual void   setHealth(float health);                             // +6
    virtual float  getMaxHealth();                                      // +7
    virtual float  getMod() { return m_mod; }                           // +8 ELF 0x2F0030
    virtual BoundingBox* getBoundingBox() { return m_boundingBox; }     // +9 ELF 0x2F0040
    virtual const LocalBoundingBox* getLocalBoundingBox()               // +10 ELF 0x2F0050
    {
        return m_localBoundingBox;
    }
    virtual float  getRadius() { return m_radius; }                     // +11 ELF 0x2F0060
    virtual Vec3   getSphereLocalOffset() const                         // +12 ELF 0x2F0070
    {
        if (m_localBoundingBox->m_derivedDataDirty)
            m_localBoundingBox->calcRadius();
        return m_localBoundingBox->m_centre;
    }
    virtual float  getSmallestRadius() const                            // +13 ELF 0x2F00D0
    {
        if (m_localBoundingBox->m_derivedDataDirty)
            m_localBoundingBox->calcRadius();
        return m_localBoundingBox->m_radiusFromCentre;
    }
    virtual float  getSubSpheres2d(std::vector<Vec2>&) const { return 0.0f; } // +14 ELF 0x2F0000
    virtual float  getArmour(Vec3&) { return m_armour; }                // +15 ELF 0x2F0110
    virtual float  getBestArmour() { return m_bestArmour; }             // +16 ELF 0x2F0120
    virtual int    getStrengthType() { return m_strengthType; }         // +17 ELF 0x2F0130
    virtual float  getVolume() { return m_volume; }                     // +18 ELF 0x2F0140
    virtual float  getMass() { return m_mass; }                         // +19 ELF 0x2F0150
    virtual int    getMaterial() const;                                 // +20
    // +21..+23 (getManDownTimeLeft, getMaxManDownTime, getManDown) stay pure.
    virtual bool   isTouchingTerrain(Vec3&) const { return m_touchingTerrain; } // +24 ELF 0x2F0160
    virtual bool   isInAir() const { return m_inAir; }                  // +25 ELF 0x2F0170
    virtual bool   isInWater() const { return m_inWater; }              // +26 ELF 0x2F0180

protected:
    // ELF layout; sizeof 0x38.
    /* +0x08 */ float             m_mod;            // set by setTime(double)
    /* +0x0C */ float             m_health;
    /* +0x10 */ BoundingBox*      m_boundingBox;    // owned
    /* +0x14 */ LocalBoundingBox* m_localBoundingBox; // owned
    /* +0x18 */ float             m_radius;
    /* +0x1C */ float             m_armour;
    /* +0x20 */ float             m_bestArmour;
    /* +0x24 */ int               m_strengthType;
    /* +0x28 */ float             m_volume;
    /* +0x2C */ float             m_mass;
    /* +0x30 */ bool              m_touchingTerrain;
    /* +0x31 */ bool              m_inAir;          // no writer in the image
    /* +0x32 */ bool              m_inWater;        // no writer in the image
    /* +0x34 */ int               m_material;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICALDUMMY_H
