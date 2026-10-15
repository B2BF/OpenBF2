// Coordinates renderer resource loading, atlas acquisition, and video-memory reservation.
// The defer flag postpones texture reads until the map-load cache sweep.

#ifdef BF2_IMAGE_RENDDX9

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

#include <new>
#include <list>
#include <string>
#include <cstring>


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
void textureManagerRefreshFileSuffix();
void overGrowthPreLoadResources();
void overGrowthPostLoadResources();
void pathGetExtension(std::string&, const std::string&);
void pathToLower(std::string&);

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
    void reserveVideoMemory();

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

ResourceManager::ResourceManager()
    : m_ready(0), m_dword08(0), m_reservedSurface(0), m_defaultTexturePriority(0)
{
    // registration is part of construction, before any map events.
    if (!eventManager->registerEventHandler(ECCore, this, 9000))
        ::renddx9abi::logf("ResourceManager.cpp:29: Failed to register ResourceManager as event handler for ECCore.");
}

ResourceManager::~ResourceManager()
{
    // The deleting destructor frees the object, not its reserved surface.
    eventManager->unregisterEventHandler(ECCore, this);
}

void ResourceManager::handleEvent(EventCategory category, unsigned int id, EventNode*, void*)
{
    if (category == ECCore) {
        if (id == 2) preLoadLevel();
        else if (id == 3) postLoadLevel();
    }
}

void ResourceManager::reserveVideoMemory()
{
    // PE +0x70 is CreateRenderTarget, not CreateTexture. The three zero arguments are
    // multisample type, quality and lockability; the surface is released after texture loading.
    ::renddx9abi::logf("ResourceManager.cpp:265: tl: don't reserve first memory, will mess up "
                      "\"fastmem\" allocation in driver. should not be used.");
    m_dword08 = 0x1000000u;
    const HRESULT result = static_cast<RendDX9Base*>(g_rendererBase)->device->CreateRenderTarget(
        2048, 2048, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &m_reservedSurface, 0);
    // [CHOSEN, not recovered] diagnostic formatting follows RendDX9Draw's existing raw-HRESULT
    // convention; retail prints DXGetErrorDescription9A. The failure branch itself is unchanged.
    if (result != 0)
        ::renddx9abi::logf("ResourceManager.cpp:277: dep: Failed to reserve video memory of size %u: 0x%08X",
                          m_dword08, static_cast<unsigned int>(result));
}

void ResourceManager::loadAtlases(const std::string& path, bool recursive)
{
    // Keep enumeration order; atlas-named DDS files suppress the fallback DDS list.
    std::list<std::pair<std::string, bool> > entries;
    std::list<io::FileInfo> files;
    fileManager->findFiles(path, false, files);
    for (const io::FileInfo& file : files)
        entries.emplace_back(file.name, file.isDirectory);
    std::list<std::string> directories, atlases;
    for (const auto& file : entries) {
        if (file.second) {
            directories.push_back(file.first);
        } else {
            std::string extension;
            pathGetExtension(extension, std::string(file.first.c_str()));
            if (std::strcmp(extension.c_str(), "dds") != 0) continue;
            std::string lower(file.first.c_str());
            // PE 10075110 -> the five Latin-1 substitutions precede CRT tolower.
            pathToLower(lower);
            if (std::string(lower.c_str(), lower.size()).find("atlas") != std::string::npos)
                atlases.push_back(file.first);
        }
    }
    const auto acquire = [&path](const std::string& file) {
        // PE 100752B0 -> remove directory AND final extension.
        const std::string::size_type slash = file.find_last_of("/\\");
        std::string name = slash == std::string::npos ? file : file.substr(slash + 1);
        const std::string::size_type dot = name.rfind('.');
        if (dot != std::string::npos) name.erase(dot);
        EngineTexture* texture = static_cast<EngineTexture*>(
            textureManagerGetTexture(std::string((path + name).c_str())));
        textureReleaseRef(texture);
    };
    if (atlases.empty()) {
        // Retail does not repeat the directory test in this fallback pass.
        for (const auto& file : entries) {
            std::string extension;
            pathGetExtension(extension, std::string(file.first.c_str()));
            if (std::strcmp(extension.c_str(), "dds") == 0) acquire(file.first);
        }
    } else {
        for (const std::string& name : atlases) acquire(name);
    }
    if (recursive)
        for (const std::string& name : directories) loadAtlases(path + name + "/", true);
}

void ResourceManager::preLoadLevel()
{
    m_ready = 1;
    static_cast<geom::IGeometryTemplateManager*>(geom::g_GeometryTemplateManager)->resetTemplateUse();
    // PE calls TextureManager nullsub_4 here: the shipped body is just RET.
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
        m_reservedSurface->Release();
        m_dword08 = 0;
        m_reservedSurface = 0;
        textureManagerSwapManaged(1);
    }
    overGrowthPostLoadResources();
    geom::g_LightManager->loadSimpleShadowMaps();
    bufferManagerCollectUnused();
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
