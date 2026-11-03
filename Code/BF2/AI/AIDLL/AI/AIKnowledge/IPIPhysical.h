// Physical information interface. getType returns zero; other queries use concrete implementations.
// getSphereLocalOffset returns a Vec3 by value. Preserve the virtual order and return types.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICAL_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICAL_H

#include <vector>

#include "../../../../Math/Vec3.h"      // Vec2 and Vec3 are both typedefs in here
#include "InformationPlugIn.h"

namespace dice {
namespace hfe {

class BoundingBox;
struct LocalBoundingBox;                // Math/LocalBoundingBox.h - a struct, as the engine declares it

namespace ai {

// The slot numbers in the comments are this table's, counted from the vtable's first entry.
class IPIPhysical : public InformationPlugIn
{
public:
    virtual ~IPIPhysical() {}                                           // +0 / +1 ( / ), inline

    virtual int    getType();                                           // +2 not pure
    virtual void   setTime() = 0;                                       // +3
    virtual void   setTime(double time) = 0;                            // +4
    virtual float  getHealth() const = 0;                               // +5
    virtual void   setHealth(float health) = 0;                         // +6
    virtual float  getMaxHealth() = 0;                                  // +7
    virtual float  getMod() = 0;                                        // +8 float (corrected)
    virtual BoundingBox*            getBoundingBox() = 0;               // +9
    virtual const LocalBoundingBox* getLocalBoundingBox() = 0;          // +10

    // Returns the footprint radius stored by information-grid insertion and movement updates.
    virtual float  getRadius() = 0;                                     // +11

    virtual Vec3   getSphereLocalOffset() const = 0;                    // +12 by value (corrected)
    virtual float  getSmallestRadius() const = 0;                       // +13
    // float: IPIPhysicalReal/Known (ELF 0x2F0CA0 / 0x2F05F0) leave the engine call's x87 result
    // as theirs (no pop), and IPIPhysicalDummy (ELF 0x2F0000) loads 0.0f.
    virtual float  getSubSpheres2d(std::vector<Vec2>&) const = 0;       // +14 float (corrected)
    virtual float  getArmour(Vec3&) = 0;                                // +15
    virtual float  getBestArmour() = 0;                                 // +16
    virtual int    getStrengthType() = 0;                               // +17

    // Keep virtual slot order: plan execution queries water/airborne state here.
    // [UNRESOLVED] Slots 21 and 22 preserve positions but their full types remain unknown.
    virtual float  getVolume() = 0;                                     // +18
    virtual float  getMass() = 0;                                       // +19
    virtual int    getMaterial() const = 0;                             // +20 type [INFERRED]
    virtual float  getManDownTimeLeft() const = 0;                      // +21 type [UNRESOLVED]
    virtual float  getMaxManDownTime() const = 0;                       // +22 type [UNRESOLVED]
    virtual bool   getManDown() const = 0;                              // +23
    virtual bool   isTouchingTerrain(Vec3& position) const = 0;         // +24
    virtual bool   isInAir() const = 0;                                 // +25
    virtual bool   isInWater() const = 0;                               // +26

    // The concrete tables end at slot 26; nothing is known to follow in the base.
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICAL_H
