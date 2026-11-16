// [CHOSEN, not recovered] macOS port: device-manager lifecycle. The bodies follow
// DX8InputDeviceManager.cpp line for line, including its leak of devices whose init fails and its
// ignored duplicate inserts; only the device classes differ. See MacInputDeviceManager.h.

#include "MacInputDeviceManager.h"
#if !DEDICATED && (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))
#include "../../Core/Ids.h"

namespace dice { namespace hfe { namespace io {

unsigned int nextMacInputSequence()
{
    static unsigned int sequence = 0;
    return ++sequence;
}

MacInputDeviceManager::MacInputDeviceManager() : m_refCount(1), m_activeDevices(0)
{ m_devices.clear(); }

MacInputDeviceManager::~MacInputDeviceManager()
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        it->second->release();
    m_devices.clear();
}

void MacInputDeviceManager::addRef() { ++m_refCount; }
int MacInputDeviceManager::getRef() { return m_refCount; }
int MacInputDeviceManager::release()
{
    const int count = --m_refCount;
    if (count == 0) delete this;
    return count;
}

void* MacInputDeviceManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IInputDeviceManager)
        return const_cast<MacInputDeviceManager*>(this);
    return 0;
}

IBase* MacInputDeviceManager::create(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    MacInputDeviceManager* self = new MacInputDeviceManager;
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result) self->release();
    return result;
}

bool MacInputDeviceManager::initDevices(unsigned int* mask)
{
    unsigned int opened = 0;
    if (*mask & IDFMouse) {
        MacMouseDevice* device = new MacMouseDevice;
        if (device && device->init()) {
            opened = IDFMouse;
            m_devices.insert(std::make_pair(IDFMouse, device));
        }
    }
    if (*mask & IDFKeyboard) {
        MacKeyboardDevice* device = new MacKeyboardDevice;
        if (device && device->init()) {
            m_devices.insert(std::make_pair(IDFKeyboard, device));
            opened |= IDFKeyboard;
        }
    }
    if (*mask & IDFGameController_0) {
        MacGameControllerDevice* device = new MacGameControllerDevice;
        if (device && device->init(0)) {
            m_devices.insert(std::make_pair(IDFGameController_0, device));
            opened |= IDFGameController_0;
        }
    }
    if (*mask & IDFGameController_1) {
        MacGameControllerDevice* device = new MacGameControllerDevice;
        if (device && device->init(1)) {
            m_devices.insert(std::make_pair(IDFGameController_1, device));
            opened |= IDFGameController_1;
        }
    }
    // IDFFalcon: the Novint SDK is Windows-only; the bit stays clear in the result.
    m_activeDevices = opened;
    if (opened == *mask) return true;
    *mask = opened;
    return false;
}

IInputDevice* MacInputDeviceManager::getInputDevice(InputDeviceFlags flags) const
{
    auto it = m_devices.find(flags);
    return it == m_devices.end() ? 0 : it->second;
}

unsigned int MacInputDeviceManager::getInputDevices() const
{
    unsigned int flags = 0;
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) flags |= it->first;
    return flags;
}

bool MacInputDeviceManager::activateDevices(unsigned int mask)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        if (it->first & mask) it->second->activate();
    return true;
}

bool MacInputDeviceManager::deActivateDevices(unsigned int mask)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        if (it->first & mask) it->second->deActivate();
    return true;
}

void MacInputDeviceManager::update(float dt)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) it->second->update(dt);
}

unsigned int MacInputDeviceManager::getActiveInputDevices() const { return m_activeDevices; }
void* MacInputDeviceManager::getAllDevices() { return &m_devices; }

} } }
#endif
