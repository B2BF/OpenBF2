// WeaponInfoKnown. Recovered from libaidll.so 1.5.3153 (ia-32), 0x2DD420..0x2DDAB7; vtable
// 0x4BE2E0 (same 13 slots as WeaponInfoReal's).

#include "WeaponInfoKnown.h"

#include "../AIUtils/CustomCurve.h"                          // SCurve::calculate (static)
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice { namespace hfe { namespace ai {

namespace {

double gameTime()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

} // anonymous namespace

// [INFERRED] helper: the constructor and update read the same five values in this order.
void WeaponInfoKnown::sample()
{
    m_ammo = m_weapon->getAmmo();                                           // +0xB8
    m_currentAmmo = m_weapon->getCurrentAmmo();                             // +0xBC
    m_readyToFire = m_weapon->isReadyToFire();                              // +0x100
    m_heat = m_weapon->getHeat();                                           // +0xF4
    m_timeOfFiring = m_weapon->getTimeOfFiring();                           // +0xE8
}

// 0x2DD420: samples the weapon now, then the shots of each magazine (the magazine count is
// re-read every iteration).
WeaponInfoKnown::WeaponInfoKnown(IWeapon* weapon)
    : WeaponInfo(weapon), m_mags()
{
    sample();
    m_timeStamp = gameTime();
    for (int i = 0; i < m_weapon->getNumberOfMags(); ++i)                   // +0xCC
        m_mags.push_back(m_weapon->getAmmoInMag(i));                  // +0xC0
}

WeaponInfoKnown::~WeaponInfoKnown()
{
}

// 0x2DD7B0: re-samples when more than a second has passed since the last sample. Always true.
bool WeaponInfoKnown::update()
{
    const double now = gameTime();
    if (now > m_timeStamp + 1.0f)
    {
        sample();
        m_timeStamp = now;
        m_mags.resize(0);
        for (int i = 0; i < m_weapon->getNumberOfMags(); ++i)               // +0xCC
            m_mags.push_back(m_weapon->getAmmoInMag(i));              // +0xC0
    }
    return true;
}

// 0x2DD990 (unchecked index).
int WeaponInfoKnown::getShotsInMag(int mag)
{
    return m_mags[mag];
}

int WeaponInfoKnown::getAmmo() const
{
    return m_ammo;
}

int WeaponInfoKnown::getCurrentAmmo() const
{
    return m_currentAmmo;
}

// 0x2DD9D0: the number of sampled magazines.
int WeaponInfoKnown::getNumberOfMags() const
{
    return static_cast<int>(m_mags.size());
}

bool WeaponInfoKnown::isReadyToFire() const
{
    return m_readyToFire;
}

double WeaponInfoKnown::getTimeOfFiring() const
{
    return m_timeOfFiring;
}

float WeaponInfoKnown::getHeat() const
{
    return m_heat;
}

// 0x2DDA20: live.
bool WeaponInfoKnown::usesHeat() const
{
    return m_weapon->usesHeat();                                            // +0xFC
}

// 0x2DDA40: the decay at the current game time.
float WeaponInfoKnown::getSecurity() const
{
    return getSecurity(gameTime());                                         // +12
}

// 0x2DDA80: one minus the SCurve of a fifth of the sample's age.
float WeaponInfoKnown::getSecurity(double time) const
{
    return 1.0f - SCurve::calculate(static_cast<float>((time - m_timeStamp) * 0.2));
}

} } }
