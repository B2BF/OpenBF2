// Win32 host/plugin ABI declarations. File organization [CHOSEN, not recovered].
// [PLATFORM] MSVC uses one deleting-destructor slot where Itanium uses two; ClassManager has
// 26 PE slots, so methods after its destructor sit four bytes below their ELF counterparts.
// Keep this boundary's native VC7.1 containers and declaration order.

#ifndef RENDDX9ABI_HFEABI_H
#define RENDDX9ABI_HFEABI_H

#include "PluginAbi.h"

namespace dice {
namespace hfe {

#if !BF2_PLUGIN_ABI_NATIVE

class IBase;
class IClassServer;

// Host registration record: class ID, VC7.1 name string and factory pointer; 36 bytes on Win32.
// The factory is __fastcall: IID arrives in ECX and outer in EDX. Do not change it to __cdecl.
// An in-process harness using the same typedef cannot independently establish the calling convention.
struct ClassRegInfo
{
    /*  0 */ unsigned int classId;
    /*  4 */ vc71::string name;
    /* 32 */ IBase*     (__fastcall *create)(unsigned int iid, IBase* outer);
};

// 44-byte VC7.1 singleton registration record. Flag 0x2 creates immediately; 0x4 publishes an
// existing object with IID/CID zero. Keep host-visible field offsets unchanged.
struct SingletonRegInfo
{
    /*  0 */ vc71::string name;
    /* 28 */ IBase**      instance;   // address of the global that receives the pointer
    /* 32 */ unsigned int iid;
    /* 36 */ unsigned int classId;
    /* 40 */ int          flags;
};

// Four-method reference-count interface with no virtual destructor. Lifetime is managed
// through release; preserve this slot order across the host boundary.
#ifndef BF2_HFE_IBASE_DEFINED
#define BF2_HFE_IBASE_DEFINED
class IBase
{
public:
    virtual void  addRef() = 0;                                  // +0x00
    virtual int   getRef() = 0;                                  // +0x04
    virtual int   release() = 0;                                 // +0x08
    virtual void* queryInterface(unsigned int iid) const = 0;    // +0x0C

protected:
    ~IBase() {}
};
#endif

// IClassServer adds the two list slots. The whole of registerServer's interest in a module is these
// two calls.
class IClassServer : public IBase
{
public:
    // the parameter is the host'S std::vector, passed by reference, and it is not OUR std::vector.
    // It arrives as a pointer to a 16-byte VC7.1 vector object - see Vc71Vector.h. Declared void*
    // rather than as a typed reference so that no call site can accidentally bind a native
    // container to it.
    virtual void getClassList(void* hostVectorOfClassRegInfo) = 0;          // +0x10
    virtual void getSingletonList(void* hostVectorOfSingletonRegInfo) = 0;  // +0x14

protected:
    ~IClassServer() {}
};

#endif // !BF2_PLUGIN_ABI_NATIVE

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

    virtual void  registerServer(IClassServer* server) = 0;                 // +0x14  read 0x100741BD
    virtual void  unregisterServer(IClassServer* server) = 0;               // +0x18  read 0x100742BD

    virtual void  reserved_setDefaultClass() = 0;                           // +0x1C
    virtual void  reserved_getDefaultClass() = 0;                           // +0x20
    virtual void  reserved_getClassName() = 0;                              // +0x24
    // Resolves a class name, writes its id through the output pointer, and reports success.
    virtual bool  getClassID(const renddx9abi::HostString& name,
                             unsigned int*       classIdOut) = 0;            // +0x28  read

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

    virtual void  reserved_getSingletonAs() = 0;                            // +0x4C
    // Publishes `object` under `name` (Core/ClassManager.h: registerSimpleSingleton(const
    // std::string&, void*), ELF +0x54). LightmapGeneration's creator calls it.
    virtual void  registerSimpleSingleton(const renddx9abi::HostString& name,
                                          void* object) = 0;                // +0x50  read 0x100A8759
    virtual void  reserved_unregisterSimpleSingleton() = 0;                 // +0x54
    virtual void  reserved_getSimpleSingleton() = 0;                        // +0x58
    virtual void  reserved_getPrintableSingletonList() = 0;                 // +0x5C
    virtual void  reserved_getAllClassIncludingSubstring() = 0;             // +0x60
    virtual void  reserved_initSingletons() = 0;                            // +0x64

protected:
    ~IClassManager() {}
};

} // namespace hfe
} // namespace dice

#endif // RENDDX9ABI_HFEABI_H
