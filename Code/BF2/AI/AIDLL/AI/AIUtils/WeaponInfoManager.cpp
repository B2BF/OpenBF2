// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\AIUtils\WeaponInfoManager.cpp" (0x1007D910).

#include "WeaponInfoManager.h"

#include "checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

WeaponInfoManager* WeaponInfoManager::m_instance = 0;

// Creates the weapon-information handle pool and checks allocation before use.
WeaponInfoManager::WeaponInfoManager()
{
    m_pool = new Pool();
    checkPointer(m_pool);

    if (!m_instance)
        m_instance = this;
}

// Frees the non-owning pool and clears the singleton only when it points to this manager.
// WeaponInfo objects must be released separately.
WeaponInfoManager::~WeaponInfoManager()
{
    if (m_pool)
        operator delete(m_pool);

    if (m_instance == this)
        m_instance = 0;
}

// Returns 0 when the pool is full - the one failure this class reports.
WeaponInfoManager::Handle WeaponInfoManager::registerInformation(WeaponInfo* info)
{
    return m_pool->acquire(info);
}

// Null for a stale or zero handle; there is no separate validity query.
WeaponInfo* WeaponInfoManager::getInformation(Handle handle) const
{
    return m_pool->get(handle);
}

// 0x36D3A0, 56 B. Return type [UNRESOLVED]; `void` because every path leaves a different leftover
// in eax rather than one materialised value. See Core/SpHandlePool.h::set.
void WeaponInfoManager::setInformation(Handle handle, WeaponInfo* info)
{
    m_pool->set(handle, info);
}

// Bumps the generation, so every outstanding handle to this slot goes stale.
// Return type [UNRESOLVED]; `void` for the same reason as setInformation.
void WeaponInfoManager::freeInformation(Handle handle)
{
    m_pool->release(handle);
}

// setInformation again, with a bool. Both ship; see the header.
bool WeaponInfoManager::changeInformation(Handle handle, WeaponInfo* info)
{
    return m_pool->trySet(handle, info);
}

// Both handles validated before either store, so no half-swap is possible.
bool WeaponInfoManager::swapInformation(Handle a, Handle b)
{
    return m_pool->swap(a, b);
}

// Rebuilds the free chain but does not clear values or generations - so every
// handle issued before the reset stays correctly invalid. See Core/SpHandlePool.h::reset.
bool WeaponInfoManager::reset()
{
    return m_pool->reset();
}

} // namespace ai
} // namespace hfe
} // namespace dice
