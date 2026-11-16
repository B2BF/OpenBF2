// WeatherManager, recovered from RendDX9.dll 1.50 (0x10157680..0x10157AD0 and its factory).
// [INFERRED] file placement: the image carries no __FILE__ string or symbol for the class.

#ifdef BF2_IMAGE_RENDDX9

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h" // [CHOSEN, not recovered] x87 conversion results, including invalid inputs.
#endif
#include "WeatherManager.h"
#include "LightningManager.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"

#include <cstdlib>
#include <new>

namespace dice {
namespace hfe {
namespace geom {

namespace {

const unsigned int kIID_IBase           = 1u;
const unsigned int kIID_IWeatherManager = 0xB801DD05u;   // dword_1021AEA0

// fistp under a chop control word.
inline int truncateToInt(float value)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return bf2x87::toIntTruncate(value);
#else
    return static_cast<int>(value);
#endif
}

} // anonymous namespace


// Construction - 0x10157A00. The lightning singleton is fetched here, once; ModuleGeom publishes
// LightningManager before WeatherManager, so it already exists.
// PE 0x10157A00.
WeatherManager::WeatherManager()
    : m_refCount(1), m_lightningMinInterval(1.0f), m_lightningMaxInterval(30.0f),
      m_doubleStrikeProbability(0.0f), m_doubleStrikeMinDelay(0.0f), m_doubleStrikeMaxDelay(0.0f),
      m_stormEnabled(false), m_doubleStrikeCount(0), m_time(0.0), m_nextStrikeTime(0.0),
      m_lightningManager(0)
{
    m_lightningMinPos.x = -300.0f;
    m_lightningMinPos.y = 0.0f;
    m_lightningMinPos.z = 200.0f;
    m_lightningMaxPos.x = -400.0f;
    m_lightningMaxPos.y = 0.0f;
    m_lightningMaxPos.z = 300.0f;
    m_lightningManager = static_cast<ILightningManager*>(
        classManager->getSingleton(renddx9abi::HostString("LightningManager")));
}

WeatherManager::~WeatherManager()
{
}

// PE 0x10157630: fetches the "LightningManager" singleton into +0x50 and answers true; the constructor
// carries the same fetch inline. Unreachable in 1.50: no code or data references.
bool WeatherManager::acquireLightningManager()
{
    m_lightningManager = static_cast<ILightningManager*>(
        classManager->getSingleton(renddx9abi::HostString("LightningManager")));
    return true;
}

void WeatherManager::operator delete(void* memory)
{
    ::renddx9abi::engineFree(memory);
}


// The IBase quartet.

void WeatherManager::addRef()
{
    ++m_refCount;
}

int WeatherManager::getRef()
{
    return m_refCount;
}

// sub_1015A4B0 (shared): at zero, the deleting destructor (slot +0x10, flag 1).
int WeatherManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// IBase (1) and IID_IWeatherManager answer `this`.
// PE 0x10157AA0.
void* WeatherManager::queryInterface(unsigned int iid) const
{
    if (iid == kIID_IBase)
        return const_cast<WeatherManager*>(this);
    return iid != kIID_IWeatherManager ? 0 : const_cast<WeatherManager*>(this);
}


// 0x10157680. The storm is switched off through the virtual setter.
void WeatherManager::reset()
{
    if (m_lightningManager)
        m_lightningManager->reset();
    setStormEnabled(false);
}

// 0x10157960. Nothing advances while the storm is off. A zero interval bound stops new strikes;
// the lightning keeps updating. The lightning singleton is used without a null test, as shipped.
void WeatherManager::update(double deltaTime)
{
    if (!m_stormEnabled)
        return;
    m_time = deltaTime + m_time;
    if (m_time >= m_nextStrikeTime)
    {
        if (m_lightningMinInterval != 0.0f && m_lightningMaxInterval != 0.0f)
            scheduleLightning();
    }
    m_lightningManager->update(deltaTime);
}

// 0x101576A0. No null test on the lightning singleton, as shipped.
void WeatherManager::render()
{
    if (m_stormEnabled)
        m_lightningManager->render();
}

// 0x101577A0. One strike at a random (x, z) inside [minPos, maxPos] (spans below 1 count as 1;
// z is drawn first), then the next strike time: a double strike after doubleStrikeMin..MaxDelay
// with probability ~ 1 / trunc(1 / doubleStrikeProbability), at most twice in a row, else a
// normal strike after lightningMin..MaxInterval. A probability above 1 divides by zero, as shipped.
void WeatherManager::scheduleLightning()
{
    const float spanX = m_lightningMaxPos.x - m_lightningMinPos.x;
    const float spanZ = m_lightningMaxPos.z - m_lightningMinPos.z;
    const float rangeZ = (spanZ < 1.0f) ? 1.0f : spanZ;
    const float rangeX = (spanX < 1.0f) ? 1.0f : spanX;

    Vec2 offset;
    {
        const int r = std::rand();
        offset.y = static_cast<float>(r % truncateToInt(rangeZ)) + m_lightningMinPos.z;
    }
    {
        const int r = std::rand();
        offset.x = static_cast<float>(r % truncateToInt(rangeX)) + m_lightningMinPos.x;
    }
    if (m_lightningManager)
        m_lightningManager->addLightning(offset);

    if (m_doubleStrikeProbability > 0.0f && m_doubleStrikeCount < 2)
    {
        const int r = std::rand();
        if (r % truncateToInt(1.0f / m_doubleStrikeProbability) == 0)
        {
            int range = truncateToInt((m_doubleStrikeMaxDelay - m_doubleStrikeMinDelay) * 1000.0f);
            if (range < 1)
                range = 1;
            const int delay = std::rand() % range;
            ++m_doubleStrikeCount;
            m_nextStrikeTime = static_cast<double>(delay) * static_cast<double>(0.001f) + m_time
                             + m_doubleStrikeMinDelay;
            return;
        }
    }

    int range = truncateToInt((m_lightningMaxInterval - m_lightningMinInterval) * 1000.0f);
    if (range < 1)
        range = 1;
    const int delay = std::rand() % range;
    m_doubleStrikeCount = 0;
    m_nextStrikeTime = static_cast<double>(delay) * static_cast<double>(0.001f) + m_time
                     + m_lightningMinInterval;
}


// The console properties.

// PE 0x101576B0.
void WeatherManager::setLightningMinInterval(float seconds) { m_lightningMinInterval = seconds; }
float WeatherManager::getLightningMinInterval() const { return m_lightningMinInterval; }

// PE 0x101576D0.
void WeatherManager::setLightningMaxInterval(float seconds) { m_lightningMaxInterval = seconds; }
float WeatherManager::getLightningMaxInterval() const { return m_lightningMaxInterval; }

// PE 0x10157730.
void WeatherManager::setDoubleStrikeProbability(float probability)
{
    m_doubleStrikeProbability = probability;
}
float WeatherManager::getDoubleStrikeProbability() const { return m_doubleStrikeProbability; }

// PE 0x10157750.
void WeatherManager::setDoubleStrikeMinDelay(float seconds) { m_doubleStrikeMinDelay = seconds; }
float WeatherManager::getDoubleStrikeMinDelay() const { return m_doubleStrikeMinDelay; }

// PE 0x10157770.
void WeatherManager::setDoubleStrikeMaxDelay(float seconds) { m_doubleStrikeMaxDelay = seconds; }
float WeatherManager::getDoubleStrikeMaxDelay() const { return m_doubleStrikeMaxDelay; }

// PE 0x101579C0. All three components are read before the first store.
void WeatherManager::setLightningMinPos(const Vec3& position)
{
    const float z = position.z;
    const float y = position.y;
    const float x = position.x;
    m_lightningMinPos.y = y;
    m_lightningMinPos.x = x;
    m_lightningMinPos.z = z;
}
const Vec3& WeatherManager::getLightningMinPos() const { return m_lightningMinPos; }

// PE 0x101579E0.
void WeatherManager::setLightningMaxPos(const Vec3& position)
{
    const float z = position.z;
    const float y = position.y;
    const float x = position.x;
    m_lightningMaxPos.y = y;
    m_lightningMaxPos.x = x;
    m_lightningMaxPos.z = z;
}
const Vec3& WeatherManager::getLightningMaxPos() const { return m_lightningMaxPos; }

// PE 0x10157710.
void WeatherManager::setStormEnabled(bool enabled) { m_stormEnabled = enabled; }
bool WeatherManager::getStormEnabled() const { return m_stormEnabled; }


// 0x100B0BE6: an aggregate outer is refused; a failed query releases the new object.
IBase* __fastcall createWeatherManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(WeatherManager));
    WeatherManager* const self = storage ? new (storage) WeatherManager() : 0;
    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
