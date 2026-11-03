// Placeholder armament plug-in that is not backed by an engine object: it keeps one weapon
// transformation of its own and has no weapons. getWeapon / getWeapons (+7 / +8) stay pure, so
// the class is abstract; nothing in libaidll.so constructs it or derives from it.
// Evidence: libaidll.so 1.5.3153 (ia-32) 0x2EC850..0x2ECD44, vtable 0x4BEA40 (16 slots);
// FILE symbol IPIArmamentDummy.cpp.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPIARMAMENTDUMMY_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPIARMAMENTDUMMY_H

#include "IPIArmament.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

class Bot;

class IPIArmamentDummy : public IPIArmament
{
public:
    // No information; the weapon transformation starts as `transformation`.
    explicit IPIArmamentDummy(Mat4& transformation);
    virtual ~IPIArmamentDummy();

    virtual void setTime();                                             // +3
    virtual void setActiveWeapon(int, int) {}                           // +6 ELF 0x2ECD20
    virtual int  isAntiAircraft() const;                                // +11
    virtual void updateWeapons() {}                                     // +12 ELF 0x2ECD40
    virtual void rearm(WeaponInfo*, float, int) {}                      // +13 ELF 0x2ECD30

    // Non-virtual. Both answer true for any bot.
    bool getDirection(Bot* bot, Vec3& direction);
    bool getTransformation(Bot* bot, Mat4& transformation);
    void setTime(float time);

protected:
    // ELF layout; sizeof 0x58.
    /* +0x18 */ Mat4 m_transformation;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_IPIARMAMENTDUMMY_H
