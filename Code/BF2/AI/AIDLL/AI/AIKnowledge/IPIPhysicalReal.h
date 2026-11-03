// Physical information backed by a live engine object. Requires a valid owner and physical plug-in.
// getMod refreshes the health-based modifier; construction does not initialize its cached value.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICALREAL_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICALREAL_H

#include "IPIPhysical.h"

namespace dice {
namespace hfe {
namespace ai {

class IPIPhysicalReal : public IPIPhysical
{
public:
    explicit IPIPhysicalReal(Information* information);
    virtual ~IPIPhysicalReal();

    virtual void   setTime();                                           // +3 - now
    virtual void   setTime(double time);                                // +4
    virtual float  getHealth() const;                                   // +5 - a fraction
    virtual void   setHealth(float health);                             // +6 - empty
    virtual float  getMaxHealth();                                      // +7
    virtual float  getMod();                                            // +8
    virtual BoundingBox*            getBoundingBox();                   // +9
    virtual const LocalBoundingBox* getLocalBoundingBox();              // +10
    virtual float  getRadius();                                         // +11
    virtual Vec3   getSphereLocalOffset() const;                        // +12
    virtual float  getSmallestRadius() const;                           // +13
    virtual float  getSubSpheres2d(std::vector<Vec2>& out) const;       // +14
    virtual float  getArmour(Vec3& direction);                          // +15
    virtual float  getBestArmour();                                     // +16
    virtual int    getStrengthType();                                   // +17
    virtual float  getVolume();                                         // +18
    virtual float  getMass();                                           // +19
    virtual int    getMaterial() const;                                 // +20
    virtual float  getManDownTimeLeft() const;                          // +21
    virtual float  getMaxManDownTime() const;                           // +22
    virtual bool   getManDown() const;                                  // +23
    virtual bool   isTouchingTerrain(Vec3& position) const;             // +24
    virtual bool   isInAir() const;                                     // +25
    virtual bool   isInWater() const;                                   // +26

private:
    /* +0x08 */ float m_mod;                // not initialised by the constructor
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICALREAL_H
