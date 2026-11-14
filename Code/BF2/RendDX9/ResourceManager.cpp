// Coordinates renderer resource loading, atlas acquisition, and video-memory reservation.
// The defer flag postpones texture reads until the map-load cache sweep.

#ifdef BF2_IMAGE_RENDDX9

#if defined(__APPLE__) && !DEDICATED
#include "../Platform/macOS/Warmup.h"   // [CHOSEN] +warmupLevel (port addition)
#endif
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"
#include "Texture.h"
#include "../Core/EventManager.h"
#include "../Core/System.h"
#include "../Geom/GeometryTemplateManager.h"
#include "../Geom/ParticleSystem/ParticleSystemManager.h"
#include "../Geom/XpLightManager.h"
#include "../Core/SettingsRepostitory.h"
#include "../Core/StringUtil.h"
#include "../GeomMisc/Debug.h"
#if defined(_WIN32)
#include <dxerr9.h>
#pragma comment(lib, "dxerr9.lib")
#endif

#include <new>
#include <list>
#include <string>
#include <cstring>
#include <cstdio>


namespace dice {
namespace hfe {
namespace geom {
extern IBase* g_ParticleSystemManager;
extern LightManager* g_LightManager;
}
namespace dx9rend {
extern void* g_rendererBase;
void bufferManagerCollectUnused();
void textureManagerSweep();
void textureManagerPreLoadLevelHook();
void textureManagerRefreshFileSuffix();
void overGrowthPreLoadResources();
void overGrowthPostLoadResources();
void pathToLower(std::string&);

namespace {
const char kResourceFile[] =
    "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\ResourceManager.cpp";

const char* resourceErrorDescription(HRESULT result)
{
#if defined(_WIN32)
    return ::DXGetErrorDescription9A(result);
#else
    // [CHOSEN, not recovered] DXErr9 is a Windows SDK library; native diagnostics retain HRESULT.
    static char description[32];
    std::snprintf(description, sizeof(description), "HRESULT 0x%08X",
                  static_cast<unsigned int>(result));
    return description;
#endif
}
}

// Source-level adapter for the load callbacks' unguarded SettingsRepostitory::boolGet.
// No fabricated default for an absent manager/result.
bool readLevelLoadBoolSetting(const char* name, bool fallback)
{
    return settings->boolGet(std::string(name), fallback);
}

// dword_1023B988. The texture sweep loads it directly to read the flag.
void* g_resourceManager = 0;


// ECCore listener coordinating render-resource loading. The object is the listener;
// it has no separate IBase interface or reference count.

class ResourceManager : public IEventListener
{
public:
    ResourceManager();
    void handleEvent(EventCategory category, unsigned int id, EventNode*, void*) override;
    virtual ~ResourceManager();
    static void operator delete(void* value) { ::renddx9abi::engineFree(value); }
    void preLoadLevel();                         // PE 1000E2D0, [INFERRED] name
    void postLoadLevel();                        // PE 1000DB70, [INFERRED] name
    void loadAtlases(const std::string& path, bool recursive);
    void loadObjectLightmapAtlases();            // PE 0x1000E110, [INFERRED] name
    void loadMenuAtlases();                      // PE 0x1000E290, [INFERRED] name
    void reserveVideoMemory();
    void releaseReservedVideoMemory();           // PE 0x1000DAB0, [INFERRED] name

    // sub_1000D950 / sub_1000D960 - a getter and a setter over the byte at +0x04, one instruction
    // each. The texture sweep's entire body is gated on the getter - AND SO IS THE loader'S file
    // read, with the opposite polarity: TextureManager::getTexture (sub_10019260) reads the file
    // only when this byte is 0. It is 1 from initializeD3DSubsystems until Renderer::initialize
    // step 16 clears it, so init-time loads are deferred (the sweep re-uploads them) and menu-time
    // loads are immediate.
    bool isReady() const          { return m_ready != 0; }
    void setReady(unsigned char v) { m_ready = v; }

    // +0x10 - by its two readers: getTexture stamps it into every new
    // Texture's +0x54, and create2DOrVolumeTexture calls SetPriority with that value when the pool
    // is managed and it is >= 15. AtlasCache::loadAtlases swaps 15 in around atlas-page loads
    // (sub_10017550's first/last statements), which is the only writer that makes it non-zero.
    int  defaultPriority() const  { return m_defaultTexturePriority; }
    void setDefaultPriority(int v) { m_defaultTexturePriority = v; }

private:
    unsigned char m_ready;       // +0x04
    unsigned char m_pad05[3];
    unsigned int  m_dword08;     // +0x08  ctor: 0
    IDirect3DSurface9* m_reservedSurface; // +0x0C, CreateRenderTarget out parameter
    int           m_defaultTexturePriority; // +0x10  ctor: 0 - see defaultPriority() above
};

// Retail x86 evidence. createResourceManager allocates sizeof(ResourceManager) and other TUs use the
// resourceManager* seams, so the x64 object is native (issue #7).
#if defined(_M_IX86)
static_assert(sizeof(ResourceManager) == 0x14,
              "initializeD3DSubsystems allocates 20 - see the push 14h at 0x1000B4D0");
#endif

// PE 0x1000DAD0.
ResourceManager::ResourceManager()
    : m_ready(0), m_dword08(0), m_reservedSurface(0), m_defaultTexturePriority(0)
{
    // registration is part of construction, before any map events.
    if (!eventManager->registerEventHandler(ECCore, this, 9000))
        ::renddx9abi::Debug(DebugTypeError2, kResourceFile, 29, "")
            << "Failed to register ResourceManager as event handler for ECCore.";
}

ResourceManager::~ResourceManager()
{
    // The deleting destructor frees the object, not its reserved surface.
    eventManager->unregisterEventHandler(ECCore, this);
}

// [CHOSEN, not recovered] Owner-TU seam for PE 0x1000B078..0x1000B089.
// Native virtual destruction retains the compiler's own destructor-slot convention.
void destroyResourceManager()
{
    delete static_cast<ResourceManager*>(g_resourceManager);
}

// PE 0x1000E380 (listing read: category 5, id 2 then 3).
void ResourceManager::handleEvent(EventCategory category, unsigned int id, EventNode*, void*)
{
    if (category == ECCore) {
        if (id == 2) preLoadLevel();
        else if (id == 3) postLoadLevel();
    }
}

// PE 0x1000D970.
void ResourceManager::reserveVideoMemory()
{
    // PE +0x70 is CreateRenderTarget, not CreateTexture. The three zero arguments are
    // multisample type, quality and lockability; the surface is released after texture loading.
    ::renddx9abi::Debug(DebugTypeError, kResourceFile, 265, "")
        << "tl: don't reserve first memory, will mess up \"fastmem\" allocation in driver. should not be used.";
    m_dword08 = 0x1000000u;
    const HRESULT result = static_cast<RendDX9Base*>(g_rendererBase)->device->CreateRenderTarget(
        2048, 2048, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &m_reservedSurface, 0);
    if (result != 0)
    {
        ::renddx9abi::Debug diagnostic(DebugTypeError, kResourceFile, 277, "");
        const char* const description = resourceErrorDescription(result);
        const unsigned int reservedBytes = m_dword08;
        diagnostic << "dep: Failed to reserve video memory of size " << reservedBytes
                   << ": " << description;
    }
}

// Drops the surface reserveVideoMemory created (no null test) and clears both members.
// postLoadLevel inlines it; the outlined copy has no code or data references.
// PE 0x1000DAB0
void ResourceManager::releaseReservedVideoMemory()
{
    m_reservedSurface->Release();
    m_dword08 = 0;
    m_reservedSurface = 0;
}

// PE 0x1000DCB0.
void ResourceManager::loadAtlases(const std::string& path, bool recursive)
{
    // Keep enumeration order; atlas-named DDS files suppress the fallback DDS list.
    static const std::string ddsExtension("dds");
    std::list<io::FileInfo> entries;
    fileManager->findFiles(path, false, entries);
    std::list<std::string> directories, atlases;
    const auto hasDdsExtension = [](const std::string& name) {
        std::string extension;
        extension = extractFileExt(name);
        return extension == ddsExtension;
    };
    const auto containsAtlas = [](const std::string& name) {
        std::string lower(name), needle("atlas");
        pathToLower(lower);
        pathToLower(needle);
        return lower.find(needle) != std::string::npos;
    };
    for (const auto& file : entries) {
        if (file.isDirectory)
            directories.push_back(file.name);
        else if (hasDdsExtension(file.name) && containsAtlas(file.name))
            atlases.push_back(file.name);
    }
    const auto acquire = [&path](const std::string& file) {
        // PE 100752B0 -> remove directory AND final extension.
        const std::string::size_type slash = file.find_last_of("/\\");
        std::string name = slash == std::string::npos ? file : file.substr(slash + 1);
        const std::string::size_type dot = name.rfind('.');
        if (dot != std::string::npos) name.erase(dot);
        EngineTexture* texture = static_cast<EngineTexture*>(
            textureManagerGetTexture(path + name));
        textureReleaseRef(texture);
    };
    if (atlases.empty()) {
        // Retail does not repeat the directory test in this fallback pass.
        for (const auto& file : entries) {
            if (hasDdsExtension(file.name)) acquire(file.name);
        }
    } else {
        for (const std::string& name : atlases) acquire(name);
    }
    if (recursive)
        for (const std::string& name : directories) loadAtlases(path + name + "/", true);
}

// PE 0x1000E110. Unreachable in 1.50: no code or data references. Loads the atlases below
// <GSLevelDirectory>/<GSLoadLevel>/Lightmaps/Objects (both settings read with an empty default).
void ResourceManager::loadObjectLightmapAtlases()
{
    const std::string levelDirectory(settings->stringGet(std::string("GSLevelDirectory"), std::string("")));
    const std::string levelName(settings->stringGet(std::string("GSLoadLevel"), std::string("")));
    std::string path(std::string(levelDirectory + "/") + levelName);
    path += "/Lightmaps/Objects";
    fixPath(path, '/');                          // PE 0x100752C0 with dl = 0x2f
    loadAtlases(path, true);
}

// PE 0x1000E290. Unreachable in 1.50: no code or data references. The same call as the one
// preLoadLevel (PE 0x1000E2D0) makes inline.
void ResourceManager::loadMenuAtlases()
{
    loadAtlases(std::string("menu/atlas/"), true);
}

void ResourceManager::preLoadLevel()
{
    m_ready = 1;
    static_cast<geom::IGeometryTemplateManager*>(geom::g_GeometryTemplateManager)->resetTemplateUse();
    textureManagerPreLoadLevelHook();
    if (readLevelLoadBoolSetting("GSManagedTextures", true)) {
        textureManagerSwapManaged(1);
    } else {
        reserveVideoMemory();
        textureManagerSwapManaged(0);
    }
    if (m_ready) loadAtlases("menu/atlas/", true);
    geom::particleSystemManagerPreLoadLevel(
        static_cast<geom::IParticleSystemManager*>(geom::g_ParticleSystemManager));
}

void ResourceManager::postLoadLevel()
{
    overGrowthPreLoadResources();
    eventManager->postEvent(ECHud, 7, const_cast<char*>("Loading Geometries.."), 0.0f);
    bufferManagerCollectUnused();
    geom::particleSystemManagerPostLoadLevel(
        static_cast<geom::IParticleSystemManager*>(geom::g_ParticleSystemManager));
    // Four real System +0x18 (PE) calls. Even though the results are discarded, preserve calls.
    g_system->getWallClockTime();
    static_cast<geom::IGeometryTemplateManager*>(geom::g_GeometryTemplateManager)->loadUsedTemplates();
    g_system->getWallClockTime();
    eventManager->postEvent(ECHud, 7, const_cast<char*>("Loading Textures.."), 0.0f);
    g_system->getWallClockTime();
    textureManagerSweep();
    g_system->getWallClockTime();
    m_ready = 0;
    if (readLevelLoadBoolSetting("GSManagedTextures", true)) {
        textureManagerRefreshFileSuffix();
    } else {
        releaseReservedVideoMemory();
        textureManagerSwapManaged(1);
    }
    overGrowthPostLoadResources();
    geom::g_LightManager->loadSimpleShadowMaps();
    bufferManagerCollectUnused();
#if defined(__APPLE__) && !DEDICATED
    bf2warmup::levelLoaded();   // [CHOSEN, not recovered] +warmupLevel (port addition, Warmup.h)
#endif
}


// The RendDX9Init.cpp:256 seam. [CHOSEN, not recovered] as a function - the original inlines the
// 20-byte allocation and the constructor call - matching createTextureManager next door.

bool createResourceManager()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(ResourceManager));
    if (!storage)
    {
        g_resourceManager = 0;
        return false;
    }

    g_resourceManager = new (storage) ResourceManager();

    // initializeD3DSubsystems does this immediately after the constructor returns - `push 1;
    // mov ecx, eax; call sub_1000D960` at 0x1000B536. So the flag is 1 for the whole of the rest
    // of the boot, which is what makes step 16's sweep actually run.
    static_cast<ResourceManager*>(g_resourceManager)->setReady(1);

    ::renddx9abi::logf("ResourceManager: built at 0x%08X - ready flag set to 1 by "
                       "initializeD3DSubsystems", ::renddx9abi::addr(g_resourceManager));
    return true;
}

bool resourceManagerIsReady()
{
    return g_resourceManager && static_cast<ResourceManager*>(g_resourceManager)->isReady();
}

void resourceManagerSetReady(unsigned char value)
{
    if (g_resourceManager)
        static_cast<ResourceManager*>(g_resourceManager)->setReady(value);
}

// RM+0x10, for the texture loader (getTexture's priority stamp) and the atlas loader's swap-15
// bracket. Swap returns the OLD value, mirroring the shipped save/set/restore at 0x10017550.
int resourceManagerDefaultPriority()
{
    if (!g_resourceManager)
        return 0;
    return static_cast<ResourceManager*>(g_resourceManager)->defaultPriority();
}

int resourceManagerSwapDefaultPriority(int value)
{
    if (!g_resourceManager)
        return 0;
    ResourceManager* rm = static_cast<ResourceManager*>(g_resourceManager);
    const int old = rm->defaultPriority();
    rm->setDefaultPriority(value);
    return old;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
