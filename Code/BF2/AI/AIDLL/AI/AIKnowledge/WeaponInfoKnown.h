// Weapon information about another side's weapon: ammunition, magazines, readiness, heat and
// firing time are sampled from the engine weapon at most once per second; the record's security
// decays with the age of that sample.
// [INFERRED] Source placement beside WeaponInfoReal.
#ifndef AI_AIDLL_AI_AIKNOWLEDGE_WEAPONINFOKNOWN_H
#define AI_AIDLL_AI_AIKNOWLEDGE_WEAPONINFOKNOWN_H

#include <vector>

#include "WeaponInfo.h"

namespace dice { namespace hfe { namespace ai {

class WeaponInfoKnown : public WeaponInfo
{
public:
    explicit WeaponInfoKnown(IWeapon* weapon);
    virtual ~WeaponInfoKnown();

    virtual bool   update();
    virtual int    getShotsInMag(int mag);
    virtual int    getAmmo() const;
    virtual int    getCurrentAmmo() const;
    virtual int    getNumberOfMags() const;
    virtual bool   isReadyToFire() const;
    virtual double getTimeOfFiring() const;
    virtual float  getHeat() const;
    virtual bool   usesHeat() const;
    virtual float  getSecurity() const;
    virtual float  getSecurity(double time) const;

private:
    void sample();

    /* +0x0C */ std::vector<int> m_mags;   // name: AIDLL_r assert 0x1016CB30 (PE +0xC)
    /* +0x18 */ int    m_ammo;
    /* +0x1C */ int    m_currentAmmo;
    /* +0x20 */ bool   m_readyToFire;
    /* +0x24 */ float  m_heat;
    /* +0x28 */ double m_timeOfFiring;
    /* +0x30 */ double m_timeStamp;   // name: AIDLL_r assert 0x1016CD60 (PE +0x38)
};

} } }

#endif // AI_AIDLL_AI_AIKNOWLEDGE_WEAPONINFOKNOWN_H
