// Path:     attested - Code\BF2\External\gameswf\SwiffPlayer\SwiffPlayerModule.cpp (BP2 __FILE__).
// Contents: the module bodies, the singleton storage, and the null-terminated module table
//           initDll walks. Every body cites the shipped EA.

#ifdef BF2_IMAGE_SWIFFPLAYER

#include "SwiffPlayerModule.h"
#include "SwiffPlayer.h"

#include "../../../../RendDX9Abi/HfeAbi.h"
#include "../../../../RendDX9Abi/BfMemory.h"

namespace dice {
namespace hfe {

// The host services initDll pulled, reached by the module the same way the shipped image reaches its
// globals: classManager for register/unregisterSingleton, eventManager for the (10,*) handler.
extern IClassManager* classManager;
extern IBase*         eventManager;

namespace ui {

// g_SwiffPlayer - the singleton storage the (10,1) handler fills through the factory and (10,2)
// clears. Type IBase* because SingletonRegInfo::instance is IBase.
IBase* g_SwiffPlayer = 0;


// IBase / IModule bodies. Refcount at +8 (two vptrs precede it).

void SwiffModule::addRef()   { ++m_refCount; }
int  SwiffModule::getRef()   { return m_refCount; }

int SwiffModule::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        ::renddx9abi::engineFree(this);
    return remaining;
}

// answers iid 1 (IBase) and 40000 (IModule) only. NOT 40003 (IClassServer) - a real
// difference from RendDX9's modules, reproduced. Returns the raw this (const_cast, no adjustment).
void* SwiffModule::queryInterface(unsigned int iid) const
{
    if (iid == 1u || iid == 40000u)
        return const_cast<SwiffModule*>(this);
    return 0;
}

// Publishes the Swiff class using the native host's typed vector/string and a cdecl adapter
// around the recovered __fastcall factory (RW-49).
void SwiffModule::getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector))
{
    renddx9abi::HostVectorWriter<ClassRegInfo> classes(hostVector);

    ClassRegInfo info;
    info.classId = CID_Swiff;
    info.name    = renddx9abi::HostString("dice.hfe.ui.Swiff");
    info.create  = RD9_CLASS_FACTORY(RefFactory_ui_SwiffPlayer);
    classes.push_back(info);
}

// empty. The "SwiffPlayer" singleton is published dynamically on (10,1), not from a
// static list.
void SwiffModule::getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, /*hostVector*/))
{
}


// The EventManager is reached by RAW vtable byte offset, exactly as the shipped image does it:
//   +0x2C  registerHandler(category, handler, priority)
//   +0x30  unregisterHandler(category, handler)
// handler is `this + 4` - the ECCoreEventHandler subobject, which is what
// static_cast<ECCoreEventHandler*>(this) yields.

namespace {
typedef int (__thiscall* EmRegisterFn)  (void* self, int category, void* handler, int priority);
typedef int (__thiscall* EmUnregisterFn)(void* self, int category, void* handler);

inline void eventManagerRegister(int category, void* handler, int priority)
{
    void*  em   = eventManager;
    void** vtbl = *reinterpret_cast<void***>(em);
    reinterpret_cast<EmRegisterFn>(vtbl[0x2C / 4])(em, category, handler, priority);
}
inline void eventManagerUnregister(int category, void* handler)
{
    void*  em   = eventManager;
    void** vtbl = *reinterpret_cast<void***>(em);
    reinterpret_cast<EmUnregisterFn>(vtbl[0x30 / 4])(em, category, handler);
}
} // anonymous namespace

// register the event-handler subobject for core-module event category 10, priority 0.
bool SwiffModule::init()
{
    eventManagerRegister(10, static_cast<ECCoreEventHandler*>(this), 0);
    return true;
}

// the mirror.
bool SwiffModule::close()
{
    eventManagerUnregister(10, static_cast<ECCoreEventHandler*>(this));
    return true;
}

// on (10,1) publish "SwiffPlayer" (created inside registerSingleton, flags 2); on
// (10,2) unregister and clear the storage. Uses the classManager global directly, exactly as the
// shipped handler does - it does not need the owning module.
void SwiffModule::handleCoreModuleEvent(int category, int id, int /*a3*/, int /*a4*/)
{
    if (category != 10)
        return;

    if (id == 1)
    {
        SingletonRegInfo info;
        info.name     = renddx9abi::HostString("SwiffPlayer");
        info.instance = &g_SwiffPlayer;
        info.iid      = IID_ISwiffPlayer;
        info.classId  = CID_Swiff;
        info.flags    = 2;                  // create now, inside registerSingleton
        classManager->registerSingleton(info);
    }
    else if (id == 2)
    {
        classManager->unregisterSingleton(renddx9abi::HostString("SwiffPlayer"));
        g_SwiffPlayer = 0;
    }
}

} // namespace ui


// Null-terminated module list consumed by PluginEntry; owns one dynamically created module.

IModule* dllmodules[] =
{
    new ui::SwiffModule(),
    0
};

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_SWIFFPLAYER
