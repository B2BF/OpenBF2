// Armament information about an object of another side: the weapon records are rebuilt lazily
// (after updateWeapons marks them stale) and refreshed on every setTime.
// [INFERRED] Source placement beside IPIArmamentReal.
#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPIARMAMENTKNOWN_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPIARMAMENTKNOWN_H

#include "IPIArmament.h"

namespace dice {
namespace hfe {
namespace ai {

class IPIArmamentKnown : public IPIArmament
{
public:
    explicit IPIArmamentKnown(Information* information);
    virtual ~IPIArmamentKnown();

    virtual void setTime(double time);                                  // +4
    virtual unsigned int getWeapon(int index);                          // +7
    virtual std::vector<unsigned int>& getWeapons();                    // +8
    virtual void updateWeapons();                                       // +12 - marks stale
    // +16, the class's own slot: rebuilds the weapon records as known ones.
    virtual void updateWeaponsInternal();

    // Non-virtual; the vtable keeps IPIArmament::getActiveWeapon (+5).
    int getActiveWeapon();

private:
    /* +0x18 */ bool m_weaponsStale;   // [INFERRED] name
    // Read by getActiveWeapon, written by no audited body (retail reads it uninitialised).
    // [CHOSEN, not recovered] zeroed by the constructor here.
    /* +0x1C */ int  m_activeWeapon;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_IPIARMAMENTKNOWN_H
