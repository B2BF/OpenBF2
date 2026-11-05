// Movement information attached to an AI information record.
// [INFERRED] Source placement.
#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPIMOBILE_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPIMOBILE_H

#include "../../../../Math/Vec3.h"
#include "InformationPlugIn.h"

namespace dice {
namespace hfe {
namespace ai {

class BFInformationCourier;
class IPIMobileKnown;

class IPIMobile : public InformationPlugIn
{
public:
    // The first two fields follow IAIPathfinding::getPolygon's output order. Both x86 images use a
    // 20-byte stride and -1 as the invalid value for each index.
    struct ValidPos
    {
        unsigned int polygon;
        unsigned int island;    // name: AIDLL_r assert "oldVehicleVP.island != invalidIslandId" (0x100CDD00; +4)
        Vec3 position;
    };

    explicit IPIMobile(Information* information);
    virtual ~IPIMobile();                                               // ELF

    virtual int getType();                                              // +2: 2
    virtual void setTime();                                             // +3: current game time
    virtual void setTime(double time) = 0;                              // +4
    virtual void setHasContact() = 0;                                   // +5
    virtual void setLostContact() = 0;                                  // +6
    virtual const Vec3& getSpeed() = 0;                                 // +7
    virtual const Vec3& getRotationalSpeed() = 0;                       // +8
    virtual float getSpeedMagnitude() = 0;                              // +9
    virtual float getSoundSphereRadius() const = 0;                     // +10
    virtual void getPositionEstimate(Vec3& position) = 0;               // +11
    virtual void getPositionEstimate(double time, Vec3& position) = 0;  // +12
    virtual unsigned int getSector() const = 0;                         // +13
    virtual int getLastPassedPortal() const = 0;                        // +14
    virtual void setLastValidInfantryPos(const ValidPos& position) = 0; // +15
    virtual void setLastValidVehiclePos(const ValidPos& position) = 0;  // +16

    int getVehicleNumber() const;
    float getMaxSpeed();
    float getTurnRadius();
    float getDrag();
    float getCoverSearchRadius();
    float getSoundSphereRadiusMax() const;
    float getSoundSphereRadiusMin() const;
    float getHearingProbabilityMax() const;
    float getHearingProbabilityMin() const;
    bool isTurnable() const;
    float getAvoidCollisionLookAhead() const;
    float getSprintStamina() const;

    // [INFERRED] inline reader of +0x08: BFPathfindingCourier::getLastValidSquadleaderPosition
    // (ELF 0x2E83BD) reads `mobile+8 != -1` and copies +0x10..+0x18 without a call; also
    // BotNavigation::reset / initPathfinding (ELF 0x190A70 / 0x1902B0) read the infantry
    // polygon and island (+0x08 / +0x0C) directly.
    const ValidPos& getLastValidInfantryPos() const { return m_lastValidInfantryPos; }
    const ValidPos& getLastValidPosition() const;
    const ValidPos& getLastValidPosition(int vehicleNumber) const;
    void setLastValidInfantryPosInternal(const ValidPos& position);
    void setLastValidVehiclePosInternal(const ValidPos& position);
    void setLastValidVehiclePosInternal(const ValidPos& first, const ValidPos& second,
                                        bool useFirst);
    void setLastValidPositions(const ValidPos& infantry, const ValidPos& firstVehicle,
                               const ValidPos& secondVehicle);

protected:
    /* +0x08 */ ValidPos m_lastValidInfantryPos;
    /* +0x1C */ ValidPos m_lastValidVehiclePosA;
    /* +0x30 */ ValidPos m_lastValidVehiclePosB;
    // Deliberately not initialized by the constructor in either shipped image.
    /* +0x44 */ bool m_useFirstVehicleValidPos;

private:
    friend class BFInformationCourier;
    friend class IPIMobileKnown;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_IPIMOBILE_H
