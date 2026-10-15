// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: RW-52]
// Renderer-plugin registration harness with a stand-in class manager.
// Checks module order, factories, singleton lookup, cross-image allocation, and clean unload.
// It uses the native ABI's shared-CRT STL records.
// Shared typedefs cannot independently validate calling conventions. No device, renderer initialization, or client boot is exercised.
// Retire once end-to-end client coverage makes this harness redundant.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "../../RendDX9Abi/HfeAbi.h"

using namespace dice::hfe;
using renddx9abi::HostString;


static int g_failures = 0;

static void check(bool ok, const char* what)
{
    printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok)
        ++g_failures;
}


// A minimal IBase for the singletons the host owns.

class HostObject : public IBase
{
public:
    explicit HostObject(const char* name) : m_name(name), m_refCount(1) {}

    virtual void  addRef()  { ++m_refCount; }
    virtual int   getRef()  { return m_refCount; }
    virtual int   release() { return --m_refCount; }
    virtual void* queryInterface(unsigned int) const { return const_cast<HostObject*>(this); }

    const char* name() const { return m_name; }

private:
    const char* m_name;
    int         m_refCount;
};

// Settings observer retains every virtual slot: the DLL uses raw getInt +0x10 and getBool +0x50.
// Unused slots report their offsets and abort.
class HostSettings : public IBase
{
public:
    virtual void  addRef()  {}
    virtual int   getRef()  { return 1; }
    virtual int   release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return const_cast<HostSettings*>(this); }

    // +0x10 - returns a pointer to the value; the caller dereferences it. Handing back the
    // caller's own default pointer is what a repository with no stored value would do.
    virtual int* getInt(const HostString& name, int* defaultValue)
    {
        printf("      settings.getInt(\"%s\") -> %d (default)\n", name.c_str(),
               defaultValue ? *defaultValue : 0);
        return defaultValue;
    }

    virtual void slot_0x14() { unexpected(0x14); }
    virtual void slot_0x18() { unexpected(0x18); }
    virtual void slot_0x1C() { unexpected(0x1C); }
    virtual void slot_0x20() { unexpected(0x20); }
    virtual void slot_0x24() { unexpected(0x24); }
    virtual void slot_0x28() { unexpected(0x28); }
    virtual void slot_0x2C() { unexpected(0x2C); }
    virtual void slot_0x30() { unexpected(0x30); }
    virtual void slot_0x34() { unexpected(0x34); }
    virtual void slot_0x38() { unexpected(0x38); }
    virtual void slot_0x3C() { unexpected(0x3C); }
    virtual void slot_0x40() { unexpected(0x40); }
    virtual void slot_0x44() { unexpected(0x44); }
    virtual void slot_0x48() { unexpected(0x48); }
    virtual void slot_0x4C() { unexpected(0x4C); }

    // +0x50
    virtual bool* getBool(const HostString& name, bool* defaultValue)
    {
        printf("      settings.getBool(\"%s\") -> %d (default)\n", name.c_str(),
               defaultValue ? *defaultValue : 0);
        return defaultValue;
    }

private:
    static void unexpected(unsigned int slot)
    {
        printf("  [FAIL] the DLL dispatched an unmodelled SettingsRepostitory slot +0x%02X\n", slot);
        ++g_failures;
    }
};

// File observer returns null from open, false from exists, and an unchanged mod-prefix path.
// Other slots abort. Missing archives intentionally leave shader effects unbound.
class HostFileManager : public IBase
{
public:
    virtual void  addRef()  {}
    virtual int   getRef()  { return 1; }
    virtual int   release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return const_cast<HostFileManager*>(this); }

    virtual void slot_0x10() { unexpected(0x10); }
    virtual void slot_0x14() { unexpected(0x14); }

    // +0x18
    virtual void* open(const HostString& name, int mode, int access, unsigned int flags)
    {
        printf("      fileManager.open(\"%s\", %d, %d, %u) -> null (no archives here)\n",
               name.c_str(), mode, access, flags);
        return 0;
    }

    // +0x1C
    virtual unsigned char exists(const HostString& name)
    {
        printf("      fileManager.exists(\"%s\") -> 0\n", name.c_str());
        return 0;
    }

    virtual void slot_0x20() { unexpected(0x20); }
    virtual void slot_0x24() { unexpected(0x24); }
    virtual void slot_0x28() { unexpected(0x28); }
    virtual void slot_0x2C() { unexpected(0x2C); }
    virtual void slot_0x30() { unexpected(0x30); }
    virtual void slot_0x34() { unexpected(0x34); }
    virtual void slot_0x38() { unexpected(0x38); }
    virtual void slot_0x3C() { unexpected(0x3C); }
    virtual void slot_0x40() { unexpected(0x40); }
    virtual void slot_0x44() { unexpected(0x44); }
    virtual void slot_0x48() { unexpected(0x48); }
    virtual void slot_0x4C() { unexpected(0x4C); }
    virtual void slot_0x50() { unexpected(0x50); }
    virtual void slot_0x54() { unexpected(0x54); }
    virtual void slot_0x58() { unexpected(0x58); }
    virtual void slot_0x5C() { unexpected(0x5C); }
    virtual void slot_0x60() { unexpected(0x60); }
    virtual void slot_0x64() { unexpected(0x64); }
    virtual void slot_0x68() { unexpected(0x68); }
    virtual void slot_0x6C() { unexpected(0x6C); }
    virtual void slot_0x70() { unexpected(0x70); }
    virtual void slot_0x74() { unexpected(0x74); }
    virtual void slot_0x78() { unexpected(0x78); }
    virtual void slot_0x7C() { unexpected(0x7C); }
    virtual void slot_0x80() { unexpected(0x80); }
    virtual void slot_0x84() { unexpected(0x84); }
    virtual void slot_0x88() { unexpected(0x88); }
    virtual void slot_0x8C() { unexpected(0x8C); }

    // +0x90 - HostString* strip(HostString* out, const HostString& in). The identity copy: a
    // harness with no mod mounts has nothing to strip. HostString selects the active host ABI.
    virtual HostString* stripModPrefix(HostString* out, const HostString& in)
    {
        *out = in;
        return out;
    }

private:
    static void unexpected(unsigned int slot)
    {
        printf("  [FAIL] the DLL dispatched an unmodelled FileManager slot +0x%02X\n", slot);
        ++g_failures;
    }
};

// Event observer implements registration at +0x2C and removal at +0x30; unused slots abort.
class HostEventManager : public IBase
{
public:
    virtual void  addRef()  {}
    virtual int   getRef()  { return 1; }
    virtual int   release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return const_cast<HostEventManager*>(this); }

    virtual void slot_0x10() { unexpected(0x10); }
    virtual void slot_0x14() { unexpected(0x14); }
    virtual void slot_0x18() { unexpected(0x18); }
    virtual void slot_0x1C() { unexpected(0x1C); }
    virtual void slot_0x20() { unexpected(0x20); }
    virtual void slot_0x24() { unexpected(0x24); }
    virtual void slot_0x28() { unexpected(0x28); }

    // +0x2C - registerHandler(category, handler, priority). Accept and report.
    virtual int registerHandler(int category, void* handler, int priority)
    {
        printf("      eventManager.registerHandler(cat=%d, handler=%p, priority=%d) -> ok\n",
               category, handler, priority);
        return 1;
    }

    // +0x30 - unregisterHandler(category, handler).
    virtual int unregisterHandler(int category, void* handler)
    {
        printf("      eventManager.unregisterHandler(cat=%d, handler=%p) -> ok\n",
               category, handler);
        return 1;
    }

    virtual void slot_0x34() { unexpected(0x34); }
    virtual void slot_0x38() { unexpected(0x38); }
    virtual void slot_0x3C() { unexpected(0x3C); }
    virtual void slot_0x40() { unexpected(0x40); }

private:
    static void unexpected(unsigned int slot)
    {
        printf("  [FAIL] the DLL dispatched an unmodelled EventManager slot +0x%02X\n", slot);
        ++g_failures;
    }
};


// The fake ClassManager. Derived from the real interface, so MSVC lays its vtable out in exactly
// the order HfeAbi.h declares - which is the order read out of BF2.exe and RendDX9.dll.

class HostClassManager : public IClassManager
{
public:
    HostClassManager() : m_classCount(0), m_singletonCount(0), m_serverCount(0) {}

    virtual void  addRef() {}
    virtual int   getRef() { return 1; }
    virtual int   release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return const_cast<HostClassManager*>(this); }
    virtual void  reserved_vectorDeletingDestructor() { fatal("~ClassManager"); }

    // The host destroys registration vectors whose buffers are allocated by the DLL.
    // This exercises the shared cross-image heap contract.
    virtual void registerServer(IClassServer* server)
    {
        if (m_serverCount < 8)
            m_servers[m_serverCount++] = server;

        std::vector<ClassRegInfo> classes;
        std::vector<SingletonRegInfo> singles;

        server->addRef();                       // ClassManager::addServer addRefs - 1 -> 2

        server->getClassList(classes);
        server->getSingletonList(singles);

        for (std::vector<ClassRegInfo>::const_iterator it = classes.begin();
             it != classes.end(); ++it)
        {
            const ClassRegInfo& c = *it;
            if (m_classCount < 64)
            {
                m_classIds[m_classCount]    = c.classId;
                m_classCreate[m_classCount] = c.create;
                m_classNames[m_classCount]  = c.name;
                ++m_classCount;
            }
            printf("      class    cid=%-12u %s\n", c.classId, c.name.c_str());
        }

        for (std::vector<SingletonRegInfo>::const_iterator it = singles.begin();
             it != singles.end(); ++it)
            registerSingleton(*it);
    }

    virtual void unregisterServer(IClassServer* server)
    {
        for (int i = 0; i < m_serverCount; ++i)
            if (m_servers[i] == server)
            {
                m_servers[i] = 0;
                server->release();              // back down to 1; deinitDll's own release frees it
                return;
            }
        fatal("unregisterServer: server was never registered");
    }

    virtual void  reserved_setDefaultClass()  { fatal("setDefaultClass"); }
    virtual void  reserved_getDefaultClass()  { fatal("getDefaultClass"); }
    virtual void  reserved_getClassName()     { fatal("getClassName"); }
    // +0x28. A real lookup rather than a fatal(): GeometryTemplateManager::registerOptimizer calls
    // it on every boot, and it is expected to miss for classes that are not registered - so a
    // fixture that aborted on the miss would fail the very path the test is here to exercise.
    // [TEMP] harness-only: linear over the same table createInstance uses.
    virtual bool getClassID(const HostString& name, unsigned int* classIdOut)
    {
        for (int i = 0; i < m_classCount; ++i)
        {
            if (m_classNames[i] == name)
            {
                if (classIdOut)
                    *classIdOut = m_classIds[i];
                return true;
            }
        }
        return false;
    }
    virtual void  reserved_createInstance_cid() { fatal("createInstance(cid)"); }

    virtual IBase* createInstance(unsigned int classId, unsigned int iid, IBase* outer)
    {
        for (int i = 0; i < m_classCount; ++i)
            if (m_classIds[i] == classId)
                return m_classCreate[i](iid, outer);
        return 0;                               // class not registered - the null path, not a fault
    }

    virtual void  reserved_createInstance_name() { fatal("createInstance(name)"); }
    virtual void  reserved_setSingletonClass()   { fatal("setSingletonClass"); }
    virtual void  reserved_getSingletonClass()   { fatal("getSingletonClass"); }

    // flags bit 1 -> instantiate now through the registered factory.
    // flags bit 2 -> publish only; the object already exists and *instance already holds it.
    virtual void registerSingleton(const SingletonRegInfo& info)
    {
        IBase* created = 0;
        if (info.flags & 2)
        {
            created = createInstance(info.classId, info.iid, 0);
            if (info.instance)
                *info.instance = created;
        }

        if (m_singletonCount < 64)
        {
            m_singletonNames[m_singletonCount] = info.name;
            m_singletonSlots[m_singletonCount] = info.instance;
            ++m_singletonCount;
        }

        printf("      singleton %-24s iid=%-11u cid=%-11u flags=%d -> %s\n",
               info.name.c_str(), info.iid, info.classId, info.flags,
               (info.flags & 2) ? (created ? "created" : "NULL (class not registered)")
                                : "published");
    }

    virtual void unregisterSingleton(const HostString& name)
    {
        for (int i = 0; i < m_singletonCount; ++i)
            if (m_singletonNames[i] == name)
            {
                m_singletonNames[i].clear();
                return;
            }
    }

    virtual IBase* getSingleton(const HostString& name)
    {
        for (int i = 0; i < m_singletonCount; ++i)
            if (m_singletonNames[i] == name)
                return m_singletonSlots[i] ? *m_singletonSlots[i] : 0;
        return 0;
    }

    virtual void  reserved_getSingletonAs()               { fatal("getSingletonAs"); }
    virtual void  registerSimpleSingleton(const HostString&, void*) { fatal("registerSimpleSingleton"); }
    virtual void  reserved_unregisterSimpleSingleton()    { fatal("unregisterSimpleSingleton"); }
    virtual void  reserved_getSimpleSingleton()           { fatal("getSimpleSingleton"); }
    virtual void  reserved_getPrintableSingletonList()    { fatal("getPrintableSingletonList"); }
    virtual void  reserved_getAllClassIncludingSubstring(){ fatal("getAllClassIncludingSubstring"); }
    virtual void  reserved_initSingletons()               { fatal("initSingletons"); }

    // Publish a singleton the host itself owns - the 16 initDll requires.
    void publish(const char* name, IBase** slot)
    {
        m_singletonNames[m_singletonCount] = name;
        m_singletonSlots[m_singletonCount] = slot;
        ++m_singletonCount;
    }

    int serverCount() const
    {
        int n = 0;
        for (int i = 0; i < m_serverCount; ++i)
            if (m_servers[i])
                ++n;
        return n;
    }

private:
    static void fatal(const char* slot)
    {
        printf("  [FAIL] the DLL called an IClassManager slot this host does not implement: %s\n",
               slot);
        ++g_failures;
    }


    IClassServer* m_servers[8];
    int           m_serverCount;

    unsigned int  m_classIds[64];
    HostString    m_classNames[64];       // [TEMP] native registry storage; no fixed-size truncation
    // It must match ClassRegInfo::create exactly: __fastcall for the retail PE record and the
    // ordinary Core factory type for the rebuilt-client ABI. The first harness spelled __cdecl on
    // both retail sides, passed 20/20, and then failed in BF2. decltype keeps this consumer tied to
    // the selected record instead of repeating that mistake.
    decltype(ClassRegInfo::create) m_classCreate[64];
    int           m_classCount;

    HostString    m_singletonNames[64];   // [TEMP] exercises the shared native std::string ABI
    IBase**       m_singletonSlots[64];
    int           m_singletonCount;
};

// The native rebuilt-client boundary shares the compiler's std::string and CRT across both images.
// Exercise a name longer than both MSVC's small-string buffer and the legacy harness's fixed
// singleton-name buffer, then prove that the manager receives it intact in both directions.
static void testNativeStringRegistry()
{
    static const char kLongName[] =
        "RendDX9.native.registry.name.longer.than.the.small.string.and.legacy.fixed.buffers";

    HostClassManager manager;
    HostObject object(kLongName);
    IBase* slot = &object;
    HostString name(kLongName);

    check(name.size() == strlen(kLongName) && name.size() > 64,
          "native HostString preserves a registry name longer than 64 bytes");
    manager.publish(name.c_str(), &slot);
    check(manager.getSingleton(name) == &object,
          "native HostString publish/getSingleton round trip is exact");
    manager.unregisterSingleton(name);
    check(manager.getSingleton(name) == 0,
          "native HostString unregisterSingleton removes the exact long name");
}


int main()
{
    printf("=== RendDX9 replacement host harness ===\n\n");

    char path[MAX_PATH];
    printf("[setup] native rebuilt-client plugin ABI (shared /MD CRT)\n");
    printf("\n[TEST 0] native HostString registry round trip\n");
    testNativeStringRegistry();

    // the host's own singletons: the 16 initDll requires, plus the 3 it tolerates missing
    printf("\n[TEST 1] initDll\n");
    static HostSettings settingsObject;
    static HostFileManager fileManagerObject;
    static HostEventManager eventManagerObject;

    static HostObject objects[] =
    {
        HostObject("SettingsRepostitory"), HostObject("MainConsoleObjects"),
        HostObject("MainConsole"),         HostObject("FileManager"),
        HostObject("FileChangeManager"),   HostObject("BfGlobal_callback"),
        HostObject("BfGlobal_callbackData"), HostObject("DebugOn"),
        HostObject("DebugModules"),        HostObject("Profiler"),
        HostObject("System"),              HostObject("EventManager"),
        HostObject("FrameEventManager"),   HostObject("ObjectManager"),
        HostObject("ObjectTemplateManager"), HostObject("PlayerManager"),
    };
    static IBase* slots[16];

    HostClassManager cm;
    for (int i = 0; i < 16; ++i)
    {
        slots[i] = &objects[i];
        cm.publish(objects[i].name(), &slots[i]);
    }
    // Slot 0 is "SettingsRepostitory" - swap in the object that actually has the vtable shape.
    slots[0] = &settingsObject;
    // File/event observers cover shader-path lookups and handler registration while unused slots abort.
    slots[3]  = &fileManagerObject;
    slots[11] = &eventManagerObject;

    // Omit hWndBf2TempSlt to skip D3D device creation; this harness covers registration only.
    // LoadStat, Game, and AIConnection also stay absent to test successful initialization without them.

    sprintf_s(path, "%s\\..\\..\\Code\\Build\\bin_renddx9\\RendDX9.dll", ".");
    HMODULE dll = ::LoadLibraryA("RendDX9.dll");
    if (!dll)
    {
        printf("  [FAIL] LoadLibraryA(\"RendDX9.dll\") failed, GetLastError=%lu\n",
               ::GetLastError());
        printf("         Run this from Code\\Build\\bin_renddx9, or copy the DLL beside the exe.\n");
        return 1;
    }
    check(true, "LoadLibraryA(\"RendDX9.dll\")");

    typedef bool (__cdecl* InitDllFn)(IClassManager*);
    typedef void (__cdecl* DeinitDllFn)();
    InitDllFn   initDll   = reinterpret_cast<InitDllFn>  (::GetProcAddress(dll, "initDll"));
    DeinitDllFn deinitDll = reinterpret_cast<DeinitDllFn>(::GetProcAddress(dll, "deinitDll"));
    check(initDll   != 0, "GetProcAddress(\"initDll\")   - undecorated, exactly as BF2 resolves it");
    check(deinitDll != 0, "GetProcAddress(\"deinitDll\")");
    if (!initDll || !deinitDll)
        return 1;

    printf("    registration walk:\n");
    const bool ok = initDll(&cm);
    check(ok, "initDll returned TRUE with 16 required singletons present and 3 absent");
    check(cm.serverCount() == 4, "all four modules registered as class servers");
    printf("      hWndBf2TempSlt remains absent; renderer device construction stays skipped\n");

    // test 2: the three services BF2Render::initRenderer resolves
    printf("\n[TEST 2] the three services BF2Render::initRenderer resolves (vslot +0x48)\n");
    IBase* gtm  = cm.getSingleton(HostString("GeometryTemplateManager"));
    IBase* rend = cm.getSingleton(HostString("Renderer"));
    IBase* ppfw = cm.getSingleton(HostString("PostProductionFrameWork"));
    printf("      GeometryTemplateManager=%p  Renderer=%p  PostProductionFrameWork=%p\n",
           (void*)gtm, (void*)rend, (void*)ppfw);
    check(gtm  != 0, "getSingleton(\"GeometryTemplateManager\") - BF2 dereferences this later");
    check(rend != 0, "getSingleton(\"Renderer\") - BF2 dereferences this IMMEDIATELY, unchecked");
    check(ppfw != 0, "getSingleton(\"PostProductionFrameWork\")");

    // queryInterface must answer for the iids the registrations asked for.
    if (rend)
        check(rend->queryInterface(830001u) == rend, "Renderer::queryInterface(IID_IRenderer 830001)");
    if (gtm)
        check(gtm->queryInterface(0xAD9665BCu) == gtm,
              "GeometryTemplateManager::queryInterface(0xAD9665BC)");

    // test 3: inspect, but do not dispatch, Renderer +0x1C
    printf("\n[TEST 3] Renderer vtable slot +0x1C (inspection only)\n");
    if (rend)
    {
        void** vtable = *reinterpret_cast<void***>(rend);
        printf("      vptr=%p  slot+0x1C=%p\n", (void*)vtable, vtable[7]);
        check(vtable[7] != 0, "slot +0x1C (initialize) is populated");
        printf("      NOT CALLED. This registry harness supplies neither the temporary HWND nor\n"
               "      the device-lifecycle prerequisites owned by the real client path.\n");
    }

    // test 4: unload
    printf("\n[TEST 4] deinitDll + FreeLibrary\n");
    deinitDll();
    check(cm.serverCount() == 0, "deinitDll unregistered all four modules");
    check(::FreeLibrary(dll) != 0, "FreeLibrary");

    printf("\n=== %s: %d failure(s) ===\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
