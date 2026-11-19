// Win32 host/plugin ABI declarations. File organization [CHOSEN, not recovered].
// [PLATFORM] MSVC uses one deleting-destructor slot where Itanium uses two; ClassManager has
// 26 PE slots, so methods after its destructor sit four bytes below their ELF counterparts.
// Keep this boundary's declaration order; its containers are the shared std types (issue #7).

#ifndef RENDDX9ABI_HFEABI_H
#define RENDDX9ABI_HFEABI_H

#include "PluginAbi.h"

namespace dice {
namespace hfe {


// Eight-slot module interface. init and close return bool; preserve inherited slot order.
class IModule : public IClassServer
{
public:
    virtual bool init() = 0;                                     // +0x18
    virtual bool close() = 0;                                    // +0x1C

protected:
    ~IModule() {}
};

// Borrowed host class-manager interface. Unconfirmed slots remain reserved; only known
// call signatures may cross this ABI boundary.
class IClassManager
{
public:
    virtual void  addRef() = 0;                                             // +0x00
    virtual int   getRef() = 0;                                             // +0x04
    virtual int   release() = 0;                                            // +0x08
    virtual void* queryInterface(unsigned int iid) const = 0;               // +0x0C

    // the destructor slot. Declared as a reserved pure virtual rather than as a real
    // "virtual ~IClassManager()", because we must never call it and a real destructor declaration
    // would let "delete" compile. The slot must exist or every entry below shifts up by four.
    virtual void  reserved_vectorDeletingDestructor() = 0;                  // +0x10  ELF +0x10/+0x14
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] The Itanium C++ ABI gives the host's virtual ~ClassManager two
    // slots (complete and deleting destructor); MSVC gives it one. Without this slot every
    // later call lands one entry early: getSingleton became unregisterSingleton on macOS.
    virtual void  reserved_itaniumDeletingDestructor() = 0;                 // ELF +0x14
#endif

    virtual void  registerServer(IClassServer* server) = 0;                 // +0x14  read 0x100741BD
    virtual void  unregisterServer(IClassServer* server) = 0;               // +0x18  read 0x100742BD

    virtual void  reserved_setDefaultClass() = 0;                           // +0x1C
    virtual void  reserved_getDefaultClass() = 0;                           // +0x20
    virtual void  reserved_getClassName() = 0;                              // +0x24
    // Resolves a class name, writes its id through the output pointer, and reports success.
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Match the native host's reference signature. Keep the
    // pointer spelling as a nonvirtual adapter for the recovered plugin callers.
    virtual bool getClassID(const renddx9abi::HostString& name, unsigned int& classIdOut) = 0;
    bool getClassID(const renddx9abi::HostString& name, unsigned int* classIdOut)
    {
        return getClassID(name, *classIdOut);
    }
#else
    virtual bool  getClassID(const renddx9abi::HostString& name,
                             unsigned int*       classIdOut) = 0;            // +0x28  read
#endif

    virtual void  reserved_createInstance_cid() = 0;                        // +0x2C
    // Renderer::initialize (0x1002C8C0) calls this one four times, as createInstance(cid, iid,
    // outer). Named because a call site was read; the other two overloads were not.
    virtual IBase* createInstance(unsigned int classId,
                                  unsigned int iid,
                                  IBase*       outer) = 0;                  // +0x30
    virtual void  reserved_createInstance_name() = 0;                       // +0x34

    virtual void  reserved_setSingletonClass() = 0;                         // +0x38
    virtual void  reserved_getSingletonClass() = 0;                         // +0x3C

    virtual void   registerSingleton(const SingletonRegInfo& info) = 0;     // +0x40  read BF2 0x454370
    virtual void   unregisterSingleton(const renddx9abi::HostString& name) = 0; // +0x44  read BF2 0x454370
    virtual IBase* getSingleton(const renddx9abi::HostString& name) = 0;        // +0x48  read initDll x19

#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Typed natively: Core/ClassManager.h getSingletonAs (ELF +0x50,
    // PE +0x4C). A raw [0x4C/4] on the native host table selected getSingleton instead.
    virtual void* getSingletonAs(const renddx9abi::HostString& name, unsigned int iid) = 0;
#else
    virtual void  reserved_getSingletonAs() = 0;                            // +0x4C
#endif
    // Publishes `object` under `name` (Core/ClassManager.h: registerSimpleSingleton(const
    // std::string&, void*), ELF +0x54). LightmapGeneration's creator calls it.
    virtual void  registerSimpleSingleton(const renddx9abi::HostString& name,
                                          void* object) = 0;                // +0x50  read 0x100A8759
    virtual void  unregisterSimpleSingleton(const renddx9abi::HostString& name) = 0;                 // +0x54
    // Returns the object published under `name`, or null (Core/ClassManager.h:
    // getSimpleSingleton(const std::string&) const, ELF +0x5C). Renderer::renderScene and
    // renderFrame look up "AI" through it.
    virtual void* getSimpleSingleton(const renddx9abi::HostString& name) const = 0; // +0x58  read 0x1002C504
    virtual void  reserved_getPrintableSingletonList() = 0;                 // +0x5C
    virtual void  reserved_getAllClassIncludingSubstring() = 0;             // +0x60
    virtual void  reserved_initSingletons() = 0;                            // +0x64

protected:
    ~IClassManager() {}
};

} // namespace hfe
} // namespace dice

#endif // RENDDX9ABI_HFEABI_H
