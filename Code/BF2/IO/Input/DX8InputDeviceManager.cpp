// DirectInput device-manager lifecycle.
#include "DX8InputDeviceManager.h"
#if !DEDICATED
#include "KeyboardDevice.h"
#include "MouseDevice.h"
#include "GameControllerDevice.h"
#include "../../Core/Ids.h"

namespace dice { namespace hfe { namespace io {
DX8InputDeviceManager::DX8InputDeviceManager() : m_refCount(1), m_activeDevices(0)
{ m_devices.clear(); }
DX8InputDeviceManager::~DX8InputDeviceManager()
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        it->second->release();
    m_devices.clear();
}
void DX8InputDeviceManager::addRef() { ++m_refCount; }
int DX8InputDeviceManager::getRef() { return m_refCount; }
int DX8InputDeviceManager::release()
{
    const int count = --m_refCount;
    if (count == 0) delete this;
    return count;
}
void* DX8InputDeviceManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IInputDeviceManager)
        return const_cast<DX8InputDeviceManager*>(this);
    return 0;
}
IBase* DX8InputDeviceManager::create(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    DX8InputDeviceManager* self = new DX8InputDeviceManager;
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result) self->release();
    return result;
}

bool DX8InputDeviceManager::initDevices(unsigned int* mask)
{
    unsigned int opened = 0;
    // Retail leaks failed devices and ignores duplicate insertion failure. Do
    // not replace these with owning temporaries or assignments to existing keys.
    if (*mask & IDFMouse) {
        MouseDevice* device = new MouseDevice;
        if (device && device->init()) {
            opened = IDFMouse;
            m_devices.insert(std::make_pair(IDFMouse, device));
        }
    }
    if (*mask & IDFKeyboard) {
        KeyboardDevice* device = new KeyboardDevice;
        if (device && device->init()) {
            m_devices.insert(std::make_pair(IDFKeyboard, device));
            opened |= IDFKeyboard;
        }
    }
    if (*mask & IDFGameController_0) {
        GameControllerDevice* device = new GameControllerDevice;
        if (device && device->init(0)) {
            m_devices.insert(std::make_pair(IDFGameController_0, device));
            opened |= IDFGameController_0;
        }
    }
    if (*mask & IDFGameController_1) {
        GameControllerDevice* device = new GameControllerDevice;
        if (device && device->init(1)) {
            m_devices.insert(std::make_pair(IDFGameController_1, device));
            opened |= IDFGameController_1;
        }
    }
    // IDFFalcon: [CHOSEN, not recovered] the Novint Falcon is archived (Code/Archive/NovintFalcon).
    // The bit stays clear, as when retail's FalconDevice::init finds no device.
    m_activeDevices = opened;
    if (opened == *mask) return true;
    *mask = opened;
    return false;
}
IInputDevice* DX8InputDeviceManager::getInputDevice(InputDeviceFlags flags) const
{
    auto it = m_devices.find(flags);
    return it == m_devices.end() ? 0 : it->second;
}
unsigned int DX8InputDeviceManager::getInputDevices() const
{
    unsigned int flags = 0;
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) flags |= it->first;
    return flags;
}
bool DX8InputDeviceManager::activateDevices(unsigned int mask)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        if (it->first & mask) it->second->activate();
    return true;
}
bool DX8InputDeviceManager::deActivateDevices(unsigned int mask)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        if (it->first & mask) it->second->deActivate();
    return true;
}
void DX8InputDeviceManager::update(float dt)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) it->second->update(dt);
}
unsigned int DX8InputDeviceManager::getActiveInputDevices() const { return m_activeDevices; }
void* DX8InputDeviceManager::getAllDevices() { return &m_devices; }
} } }
#endif
