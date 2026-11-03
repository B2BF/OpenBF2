// Placeholder mobile plug-in that is not backed by an engine object: it keeps a fixed speed and a
// zero rotational speed and estimates positions from its information's position. The sound,
// sector, portal and valid-position slots (+10, +13..+16) stay pure, so the class is abstract;
// nothing in libaidll.so constructs it or derives from it.
// Evidence: libaidll.so 1.5.3153 (ia-32) 0x2EEBA0..0x2EEFCE, vtable 0x4BED00 (18 slots);
// FILE symbol IPIMobileDummy.cpp.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPIMOBILEDUMMY_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPIMOBILEDUMMY_H

#include <cmath>

#include "IPIMobile.h"

namespace dice {
namespace hfe {
namespace ai {

class IPIMobileDummy : public IPIMobile
{
public:
    // The plug-in of `information`, moving at `speed`.
    IPIMobileDummy(Information* information, Vec3& speed);
    virtual ~IPIMobileDummy();

    virtual void setTime();                                             // +3
    virtual void setTime(double) {}                                     // +4 ELF 0x2EEF50
    virtual void setHasContact() {}                                     // +5 ELF 0x2EEF30
    virtual void setLostContact() {}                                    // +6 ELF 0x2EEF40
    virtual const Vec3& getSpeed() { return m_speed; }                  // +7 ELF 0x2EEF60
    virtual const Vec3& getRotationalSpeed() { return m_rotationalSpeed; } // +8 ELF 0x2EEF70
    virtual float getSpeedMagnitude()                                   // +9 ELF 0x2EEF80
    {
        return std::sqrt(m_speed.x * m_speed.x + m_speed.y * m_speed.y + m_speed.z * m_speed.z);
    }
    virtual void getPositionEstimate(Vec3& position);                   // +11
    virtual void getPositionEstimate(double time, Vec3& position);      // +12
    // +17, new in this class. [INFERRED] bool: the body materializes 0 and has no caller.
    virtual bool getValidPosition(Vec3& position);

    // Non-virtual; empty in the image.
    void updateCurrentPolygon();

protected:
    // ELF layout; sizeof 0x60.
    /* +0x48 */ Vec3 m_speed;
    /* +0x54 */ Vec3 m_rotationalSpeed;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_IPIMOBILEDUMMY_H
