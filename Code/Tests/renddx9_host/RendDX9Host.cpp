// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: RW-52]
// Renderer-plugin registration harness with a stand-in class manager.
// Checks module order, factories, singleton lookup, cross-image allocation, and clean unload.
// Retail ABI mode compares records and strings with MSVCP71; native mode uses shared-CRT STL records.
// Shared typedefs cannot independently validate calling conventions. No device, renderer initialization, or client boot is exercised.
// Retire once end-to-end client coverage makes this harness redundant.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/DisplayModeAbi.h"
#if !BF2_PLUGIN_ABI_NATIVE
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/BfMemory.h"
#endif

using namespace dice::hfe;
using renddx9abi::HostString;

#if !BF2_PLUGIN_ABI_NATIVE
static const char* kGameDir = "D:\\B2BF Launcher\\Battlefield2";
#endif

static int g_failures = 0;

static void check(bool ok, const char* what)
{
    printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok)
        ++g_failures;
}

// [TEMP]
// Copies display-mode ranges through the production helper, destroys the producer, then checks independent ownership and repeated cleanup.
static void testDisplayModeOwnership()
{
    vc71::vector_layout ownedDepth   = { 0, 0, 0, 0 };
    vc71::vector_layout ownedMulti   = { 0, 0, 0, 0 };
    vc71::vector_layout ownedWindows = { 0, 0, 0, 0 };

    const void* sourceDepthAddress = 0;
    const void* sourceMultiAddress = 0;
    const void* sourceWindowAddress = 0;

    {
#if BF2_PLUGIN_ABI_NATIVE
        renddx9abi::HostDisplayMode source;
        source.depthFormats.push_back(75u);       // D3DFMT_D24S8
        renddx9abi::DisplayModeMultiSample sample = { 2u, 7u };
        source.multiSamples.push_back(sample);
        source.windows.push_back(reinterpret_cast<void*>(0x12345678u));
#else
        renddx9abi::HostDisplayMode source = {};
        vc71::vectorRef<unsigned int>(&source.depthFormats).push_back(75u);
        renddx9abi::DisplayModeMultiSample sample = { 2u, 7u };
        vc71::vectorRef<renddx9abi::DisplayModeMultiSample>(&source.multiSamples).push_back(sample);
        vc71::vectorRef<void*>(&source.windows).push_back(reinterpret_cast<void*>(0x12345678u));
#endif

        sourceDepthAddress  = renddx9abi::hostDepthFirst(source);
        sourceMultiAddress  = renddx9abi::hostMultiSampleFirst(source);
        sourceWindowAddress = renddx9abi::hostWindowFirst(source);

        renddx9abi::assignOwnedDisplayModeVectors(ownedDepth, ownedMulti, ownedWindows, source);

        check(ownedDepth.first != sourceDepthAddress,
              "display depth range is a renderer-owned copy, not a producer alias");
        check(ownedMulti.first != sourceMultiAddress,
              "display multisample range is a renderer-owned copy, not a producer alias");
        check(ownedWindows.first != sourceWindowAddress,
              "display HWND range is a renderer-owned copy, not a producer alias");

#if BF2_PLUGIN_ABI_NATIVE
        source.depthFormats[0] = 21u;
        source.multiSamples[0].type = 9u;
        source.multiSamples[0].quality = 10u;
        source.windows[0] = reinterpret_cast<void*>(0x87654321u);
#else
        *reinterpret_cast<unsigned int*>(source.depthFormats.first) = 21u;
        renddx9abi::DisplayModeMultiSample* sourceSample =
            reinterpret_cast<renddx9abi::DisplayModeMultiSample*>(source.multiSamples.first);
        sourceSample->type = 9u;
        sourceSample->quality = 10u;
        *reinterpret_cast<void**>(source.windows.first) = reinterpret_cast<void*>(0x87654321u);

        renddx9abi::clearOwnedDisplayModeVectors(source.depthFormats,
                                                  source.multiSamples,
                                                  source.windows);
#endif
    }

    const unsigned int* depth = reinterpret_cast<const unsigned int*>(ownedDepth.first);
    const renddx9abi::DisplayModeMultiSample* multi =
        reinterpret_cast<const renddx9abi::DisplayModeMultiSample*>(ownedMulti.first);
    void* const* windows = reinterpret_cast<void* const*>(ownedWindows.first);

    check(depth && depth[0] == 75u,
          "owned depth range survives producer mutation and destruction");
    check(multi && multi[0].type == 2u && multi[0].quality == 7u,
          "owned multisample range survives producer mutation and destruction");
    check(windows && windows[0] == reinterpret_cast<void*>(0x12345678u),
          "owned HWND range survives producer mutation and destruction");

    renddx9abi::clearOwnedDisplayModeVectors(ownedDepth, ownedMulti, ownedWindows);
    check(!ownedDepth.first && !ownedDepth.last && !ownedDepth.end &&
          !ownedMulti.first && !ownedMulti.last && !ownedMulti.end &&
          !ownedWindows.first && !ownedWindows.last && !ownedWindows.end,
          "owned display ranges release and reset every published pointer");

    // The recovered sub_1000C9B0 destructor is null-safe; keep that property explicit.
    renddx9abi::clearOwnedDisplayModeVectors(ownedDepth, ownedMulti, ownedWindows);
    check(true, "owned display cleanup is idempotent on empty ranges");
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

#if BF2_PLUGIN_ABI_NATIVE
        std::vector<ClassRegInfo> classes;
        std::vector<SingletonRegInfo> singles;
#else
        vc71::vector_layout classes   = { 0, 0, 0, 0 };
        vc71::vector_layout singles   = { 0, 0, 0, 0 };
#endif

        server->addRef();                       // ClassManager::addServer addRefs - 1 -> 2

#if BF2_PLUGIN_ABI_NATIVE
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
#else
        server->getClassList(&classes);
        server->getSingletonList(&singles);

        {
            ClassRegInfo* first = reinterpret_cast<ClassRegInfo*>(classes.first);
            ClassRegInfo* last  = reinterpret_cast<ClassRegInfo*>(classes.last);
            for (ClassRegInfo* c = first; c != last; ++c)
            {
                if (m_classCount < 64)
                {
                    m_classIds[m_classCount]     = c->classId;
                    m_classCreate[m_classCount]  = c->create;
                    // The name is kept so getClassID can answer by name. [TEMP] harness storage.
                    {
                        const char* n = c->name.c_str();
                        unsigned k = 0;
                        for (; n[k] && k < sizeof(m_classNames[0]) - 1; ++k)
                            m_classNames[m_classCount][k] = n[k];
                        m_classNames[m_classCount][k] = 0;
                    }
                    ++m_classCount;
                }
                printf("      class    cid=%-12u %s\n", c->classId, c->name.c_str());
            }
        }
        {
            SingletonRegInfo* first = reinterpret_cast<SingletonRegInfo*>(singles.first);
            SingletonRegInfo* last  = reinterpret_cast<SingletonRegInfo*>(singles.last);
            for (SingletonRegInfo* s = first; s != last; ++s)
                registerSingleton(*s);
        }

        destroyVector<ClassRegInfo>(classes);
        destroyVector<SingletonRegInfo>(singles);
#endif
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
#if BF2_PLUGIN_ABI_NATIVE
            if (m_classNames[i] == name)
#else
            const char* a = m_classNames[i];
            const char* b = name.c_str();
            while (*a && *a == *b) { ++a; ++b; }
            if (*a == 0 && *b == 0)
#endif
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
#if BF2_PLUGIN_ABI_NATIVE
            m_singletonNames[m_singletonCount] = info.name;
#else
            strncpy_s(m_singletonNames[m_singletonCount],
                      sizeof(m_singletonNames[0]), info.name.c_str(), _TRUNCATE);
#endif
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
#if BF2_PLUGIN_ABI_NATIVE
            if (m_singletonNames[i] == name)
            {
                m_singletonNames[i].clear();
                return;
            }
#else
            if (strcmp(m_singletonNames[i], name.c_str()) == 0)
            {
                m_singletonNames[i][0] = '\0';
                return;
            }
#endif
    }

    virtual IBase* getSingleton(const HostString& name)
    {
        for (int i = 0; i < m_singletonCount; ++i)
#if BF2_PLUGIN_ABI_NATIVE
            if (m_singletonNames[i] == name)
#else
            if (strcmp(m_singletonNames[i], name.c_str()) == 0)
#endif
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
#if BF2_PLUGIN_ABI_NATIVE
        m_singletonNames[m_singletonCount] = name;
#else
        strncpy_s(m_singletonNames[m_singletonCount],
                  sizeof(m_singletonNames[0]), name, _TRUNCATE);
#endif
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

#if !BF2_PLUGIN_ABI_NATIVE
    template <typename T>
    static void destroyVector(vc71::vector_layout& v)
    {
        T* first = reinterpret_cast<T*>(v.first);
        T* last  = reinterpret_cast<T*>(v.last);
        for (T* p = first; p != last; ++p)
            p->~T();
        if (v.first)
            ::operator delete(v.first);         // -> bf_free. The block came from the DLL.
        v.first = v.last = v.end = 0;
    }
#endif

    IClassServer* m_servers[8];
    int           m_serverCount;

    unsigned int  m_classIds[64];
#if BF2_PLUGIN_ABI_NATIVE
    HostString    m_classNames[64];       // [TEMP] native registry storage; no fixed-size truncation
#else
    char          m_classNames[64][96];   // [TEMP] see getClassID
#endif
    // It must match ClassRegInfo::create exactly: __fastcall for the retail PE record and the
    // ordinary Core factory type for the rebuilt-client ABI. The first harness spelled __cdecl on
    // both retail sides, passed 20/20, and then failed in BF2. decltype keeps this consumer tied to
    // the selected record instead of repeating that mistake.
    decltype(ClassRegInfo::create) m_classCreate[64];
    int           m_classCount;

#if BF2_PLUGIN_ABI_NATIVE
    HostString    m_singletonNames[64];   // [TEMP] exercises the shared native std::string ABI
#else
    char          m_singletonNames[64][64];
#endif
    IBase**       m_singletonSlots[64];
    int           m_singletonCount;
};

// Compares vc71::string with strings constructed by MSVCP71's exported constructor.
#if !BF2_PLUGIN_ABI_NATIVE
typedef void (__thiscall* P71StringCtor)(void* self, const char* s);
typedef void (__thiscall* P71StringDtor)(void* self);

static void dumpBytes(const char* label, const unsigned char* p)
{
    printf("      %-8s", label);
    for (int i = 0; i < 28; ++i)
        printf("%02X%s", p[i], ((i % 4) == 3) ? " " : "");
    printf("\n");
}

static void testStringAbi(HMODULE p71)
{
    P71StringCtor ctor = reinterpret_cast<P71StringCtor>(::GetProcAddress(p71,
        "??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBD@Z"));
    P71StringDtor dtor = reinterpret_cast<P71StringDtor>(::GetProcAddress(p71,
        "??1?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@XZ"));

    if (!ctor || !dtor)
    {
        check(false, "msvcp71.dll basic_string ctor/dtor resolved");
        return;
    }

    // short: "Renderer", 8 chars, must live in the SSO buffer
    {
        unsigned char theirs[28];
        memset(theirs, 0xCD, sizeof(theirs));
        ctor(theirs, "Renderer");

        vc71::string ours("Renderer");
        const unsigned char* oursBytes = reinterpret_cast<const unsigned char*>(&ours);

        dumpBytes("MSVCP71", theirs);
        dumpBytes("ours",    oursBytes);

        // Compare characters, terminator, size, and capacity only.
        // Allocator bytes and unused SSO tail bytes are unspecified and must be excluded.
        const size_t len = strlen("Renderer");
        check(memcmp(theirs + 4, oursBytes + 4, len + 1) == 0,
              "short string \"Renderer\": characters + terminator identical to MSVCP71's");
        check(memcmp(theirs + 20, oursBytes + 20, 8) == 0,
              "short string \"Renderer\": _Mysize and _Myres identical to MSVCP71's");
        check(*reinterpret_cast<const unsigned int*>(theirs + 24) < 16,
              "short string \"Renderer\": MSVCP71 leaves _Myres < 16, i.e. data is IN the object");
        dtor(theirs);
    }

    // long: "GeometryTemplateManager", 23 chars, must go to the CRT heap
    {
        unsigned char theirs[28];
        memset(theirs, 0xCD, sizeof(theirs));
        ctor(theirs, "GeometryTemplateManager");

        vc71::string ours("GeometryTemplateManager");
        const unsigned char* oursBytes = reinterpret_cast<const unsigned char*>(&ours);

        dumpBytes("MSVCP71", theirs);
        dumpBytes("ours",    oursBytes);

        const unsigned int theirSize = *reinterpret_cast<const unsigned int*>(theirs + 20);
        const unsigned int theirRes  = *reinterpret_cast<const unsigned int*>(theirs + 24);
        const unsigned int ourSize   = *reinterpret_cast<const unsigned int*>(oursBytes + 20);
        const unsigned int ourRes    = *reinterpret_cast<const unsigned int*>(oursBytes + 24);
        const char* theirPtr = *reinterpret_cast<char* const*>(theirs + 4);

        printf("      MSVCP71  _Mysize=%u _Myres=%u   ours _Mysize=%u _Myres=%u\n",
               theirSize, theirRes, ourSize, ourRes);

        check(theirSize == 23 && ourSize == 23, "long string: _Mysize == 23 on both sides");
        check(theirRes >= 16 && ourRes >= 16,   "long string: _Myres >= 16, so both are heap, not SSO");
        // This settles the [UNRESOLVED] capacity rounding noted in Vc71String.h. If it fails, our
        // value is wrong but harmless - MSVCP71's deallocate discards the count.
        check(theirRes == ourRes,               "long string: _Myres matches MSVCP71's rounding exactly");
        check(strcmp(theirPtr, "GeometryTemplateManager") == 0, "long string: MSVCP71 stores via _Ptr at +4");
        dtor(theirs);
    }
}
#else
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
#endif


int main()
{
    printf("=== RendDX9 replacement host harness ===\n\n");

    char path[MAX_PATH];
#if BF2_PLUGIN_ABI_NATIVE
    printf("[setup] native rebuilt-client plugin ABI (shared /MD CRT)\n");
    printf("\n[TEST 0] native HostString registry round trip\n");
    testNativeStringRegistry();
#else
    // Memory.dll and msvcr71.dll must be resolvable exactly as they are inside BF2's process.
    ::SetDllDirectoryA(kGameDir);

    sprintf_s(path, "%s\\msvcr71.dll", kGameDir);
    HMODULE r71 = ::LoadLibraryA(path);
    sprintf_s(path, "%s\\msvcp71.dll", kGameDir);
    HMODULE p71 = ::LoadLibraryA(path);
    printf("[setup] msvcr71=%p msvcp71=%p\n", (void*)r71, (void*)p71);
    check(r71 != 0, "msvcr71.dll loaded (the DLL resolves malloc/free from it by GetModuleHandle)");
    check(p71 != 0, "msvcp71.dll loaded");

    printf("\n[TEST 0] std::string ABI vs the real MSVCP71\n");
    if (p71)
        testStringAbi(p71);
#endif

    printf("\n[TEST 0B] display-mode carrier ownership\n");
    testDisplayModeOwnership();

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
