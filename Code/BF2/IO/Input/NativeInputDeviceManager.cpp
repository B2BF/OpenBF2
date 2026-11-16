// [CHOSEN, not recovered] Native device-manager lifecycle. The bodies follow
// DX8InputDeviceManager.cpp line for line, including its leak of devices whose init fails and its
// ignored duplicate inserts; only the device classes differ. See NativeInputDeviceManager.h.

#include "NativeInputDeviceManager.h"
#if !DEDICATED && (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))
#include "../../Core/Ids.h"

namespace dice { namespace hfe { namespace io {

unsigned int nextNativeInputSequence()
{
    static unsigned int sequence = 0;
    return ++sequence;
}

NativeInputDeviceManager::NativeInputDeviceManager() : m_refCount(1), m_activeDevices(0)
{ m_devices.clear(); }

NativeInputDeviceManager::~NativeInputDeviceManager()
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        it->second->release();
    m_devices.clear();
}

void NativeInputDeviceManager::addRef() { ++m_refCount; }
int NativeInputDeviceManager::getRef() { return m_refCount; }
int NativeInputDeviceManager::release()
{
    const int count = --m_refCount;
    if (count == 0) delete this;
    return count;
}

void* NativeInputDeviceManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IInputDeviceManager)
        return const_cast<NativeInputDeviceManager*>(this);
    return 0;
}

IBase* NativeInputDeviceManager::create(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    NativeInputDeviceManager* self = new NativeInputDeviceManager;
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result) self->release();
    return result;
}

bool NativeInputDeviceManager::initDevices(unsigned int* mask)
{
    unsigned int opened = 0;
    if (*mask & IDFMouse) {
        NativeMouseDevice* device = new NativeMouseDevice;
        if (device && device->init()) {
            opened = IDFMouse;
            m_devices.insert(std::make_pair(IDFMouse, device));
        }
    }
    if (*mask & IDFKeyboard) {
        NativeKeyboardDevice* device = new NativeKeyboardDevice;
        if (device && device->init()) {
            m_devices.insert(std::make_pair(IDFKeyboard, device));
            opened |= IDFKeyboard;
        }
    }
    if (*mask & IDFGameController_0) {
        NativeGameControllerDevice* device = new NativeGameControllerDevice;
        if (device && device->init(0)) {
            m_devices.insert(std::make_pair(IDFGameController_0, device));
            opened |= IDFGameController_0;
        }
    }
    if (*mask & IDFGameController_1) {
        NativeGameControllerDevice* device = new NativeGameControllerDevice;
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

IInputDevice* NativeInputDeviceManager::getInputDevice(InputDeviceFlags flags) const
{
    auto it = m_devices.find(flags);
    return it == m_devices.end() ? 0 : it->second;
}

unsigned int NativeInputDeviceManager::getInputDevices() const
{
    unsigned int flags = 0;
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) flags |= it->first;
    return flags;
}

bool NativeInputDeviceManager::activateDevices(unsigned int mask)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        if (it->first & mask) it->second->activate();
    return true;
}

bool NativeInputDeviceManager::deActivateDevices(unsigned int mask)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        if (it->first & mask) it->second->deActivate();
    return true;
}

void NativeInputDeviceManager::update(float dt)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) it->second->update(dt);
}

unsigned int NativeInputDeviceManager::getActiveInputDevices() const { return m_activeDevices; }
void* NativeInputDeviceManager::getAllDevices() { return &m_devices; }

} } }
#endif
