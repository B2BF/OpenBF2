// Manages hemisphere textures and editor/offline hemisphere-map baking.

#ifdef BF2_IMAGE_RENDDX9

#include "HemiMapManager.h"
#include "HemiMapDependencies.h"

#include "../../GeomMisc/FSQuadDrawer.h"
#include "../../Physics/HeightmapCluster.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/Texture.h"
#include "../../RendMisc/IRenderView.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/EngineFile.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_deviceCaps;
extern void* g_rendererBase;
}

namespace geom {

const unsigned int IID_IHemiMapManager = 820019u;
const unsigned int CID_HemiMapManager  = 821019u;

namespace {

void releaseTexture(dx9rend::EngineTexture* texture)
{
    if (texture && --texture->refCount == 0)
    {
        dx9rend::textureDtor(texture);
        ::renddx9abi::engineFree(texture);
    }
}

void destroyHemiMap(HemiMap* map)
{
    if (!map)
        return;

    dx9rend::EngineTexture* const texture = map->texture;
    releaseTexture(texture);
    // PE 0x1012B5A0 does this after dropping the map's own texture reference. If the texture was
    // cached, this removes and releases the cache's independent reference; otherwise the cache
    // walk simply reaches its sentinel.
    dx9rend::textureManagerEvictFromCache(texture);
    map->name.~basic_string();
    ::renddx9abi::engineFree(map);
}

HemiMap* createHemiMap(const HemiMapName& name, const Vec3& position,
                       float range, float falloff)
{
    HemiMap* map = static_cast<HemiMap*>(::renddx9abi::engineAlloc(sizeof(HemiMap)));
    if (map)
    {
        map->position.x = 0.0f;
        map->position.y = 0.0f;
        map->position.z = 0.0f;
        new (&map->name) std::string();
    }

    // Retail 0x1012B600 continues through these stores after a failed allocation. Preserve that
    // missing OOM guard rather than turning allocation failure into a fabricated normal result.
    map->range = range;
    map->position = position;
    map->name = name.c_str();
    map->falloff = falloff;

    std::string probe(map->name);
    probe += ".dds";
    if (::renddx9abi::engineFileExists(probe))
    {
        map->texture = static_cast<dx9rend::EngineTexture*>(
            dx9rend::textureManagerGetTexture(map->name));
        return map;
    }

    map->texture = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerCreateEmpty(8u, 8u, D3DFMT_A8R8G8B8, 1u, 0u, 0));
    dx9rend::TextureLockRecord* const lock =
        dx9rend::textureLock(map->texture, 0u, 0x7FFFFFFFu);
    unsigned int* pixel = static_cast<unsigned int*>(lock->pBits);
    unsigned int* const end = pixel + lock->width * lock->height;
    while (pixel != end)
        *pixel++ = 0xFFFFFFFFu;
    dx9rend::textureUnlock(map->texture, lock);

    std::string saveDirectory;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", " ERROR ", saveDirectory);
    saveDirectory += "/";
    saveDirectory += map->name;
    // Save through TextureManager; the result is intentionally discarded.
    dx9rend::textureManagerSaveTexture(map->texture, saveDirectory);
    return map;
}

} // anonymous namespace

class HemiMapManager : public IHemiMapManager
{
public:
    HemiMapManager();
    ~HemiMapManager();

    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;

    void clear() override;
    void setBaseHemiMap(const HemiMapName& name, const Vec3& position,
                        float range, float falloff) override;
    HemiMap* getBaseHemiMap() const override;
    const HemiMapInfo& getHemiMapInfo(const HemiMap* map) override;
    bool slot20(unsigned int, unsigned int, unsigned int, unsigned int) override;
    bool slot24(unsigned int) override;
    HemiMap* slot28(unsigned int) override;
    void setHemiLerpBias(float bias) override;
    float getHemiLerpBias() const override;
    bool makeHemiMap(unsigned long resolution, HemiMapName outputName,
                     Vec3 position, float orthoExtent) override;

private:
    friend struct HemiMapManagerLayoutAssertions;

    int m_refCount;                        // +0x04
    HemiMap* m_baseHemiMap;                // +0x08
    std::vector<HemiMap*> m_hemiMaps;      // +0x0C
    float m_hemiLerpBias;                  // +0x1C
    dx9rend::HemiRenderState m_savedState; // +0x20
    HemiMapInfo m_info;                    // +0x4C
};

#if defined(_M_IX86)
struct HemiMapManagerLayoutAssertions
{
    static_assert(offsetof(HemiMapManager, m_refCount) == 0x04, "Hemi manager ref offset drift");
    static_assert(offsetof(HemiMapManager, m_baseHemiMap) == 0x08, "Hemi base offset drift");
    static_assert(offsetof(HemiMapManager, m_hemiMaps) == 0x0C, "Hemi vector offset drift");
};
#endif

HemiMapManager::HemiMapManager()
    : m_refCount(1)
    , m_baseHemiMap(0)
    , m_hemiLerpBias(0.3f)
{
    // The constructor writes only state dwords +0x38..+0x44; the other state bytes retain the
    // allocator's contents until rendererCaptureHemiState fills the fields it owns.
    *reinterpret_cast<unsigned int*>(m_savedState.bytes + 24) = 0u;
    *reinterpret_cast<unsigned int*>(m_savedState.bytes + 28) = 0u;
    *reinterpret_cast<unsigned int*>(m_savedState.bytes + 32) = 0u;
    *reinterpret_cast<unsigned int*>(m_savedState.bytes + 36) = 0u;

    m_info.positionX = 0.0f;
    m_info.positionZ = 0.0f;
    m_info.range = 0.0f;
    m_info.lerpBias = 0.0f;
}

HemiMapManager::~HemiMapManager()
{
    clear();
}

void HemiMapManager::addRef()
{
    ++m_refCount;
}

int HemiMapManager::getRef()
{
    return m_refCount;
}

int HemiMapManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~HemiMapManager();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

void* HemiMapManager::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IHemiMapManager)
        return const_cast<HemiMapManager*>(this);
    return 0;
}

void HemiMapManager::clear()
{
    if (m_baseHemiMap)
    {
        destroyHemiMap(m_baseHemiMap);
        m_baseHemiMap = 0;
    }

    std::vector<HemiMap*>::iterator it = m_hemiMaps.begin();
    const std::vector<HemiMap*>::iterator last = m_hemiMaps.end();
    while (it != last)
        destroyHemiMap(*it++);

    m_hemiMaps.clear();
}

void HemiMapManager::setBaseHemiMap(const HemiMapName& name, const Vec3& position,
                                    float range, float falloff)
{
    if (m_baseHemiMap)
        destroyHemiMap(m_baseHemiMap);
    m_baseHemiMap = createHemiMap(name, position, range, falloff);
}

HemiMap* HemiMapManager::getBaseHemiMap() const
{
    return m_baseHemiMap;
}

const HemiMapInfo& HemiMapManager::getHemiMapInfo(const HemiMap* map)
{
    if (map)
    {
        m_info.positionX = map->position.x;
        m_info.positionZ = map->position.z;
        m_info.range = map->range;
        m_info.lerpBias = m_hemiLerpBias;
    }
    return m_info;
}

bool HemiMapManager::slot20(unsigned int, unsigned int, unsigned int, unsigned int)
{
    return false;
}

bool HemiMapManager::slot24(unsigned int)
{
    return false;
}

HemiMap* HemiMapManager::slot28(unsigned int)
{
    // PE 0x1012C190 re-dispatches through this object's +0x18 slot rather than reading +0x08.
    return getBaseHemiMap();
}

void HemiMapManager::setHemiLerpBias(float bias)
{
    m_hemiLerpBias = bias;
}

float HemiMapManager::getHemiLerpBias() const
{
    return m_hemiLerpBias;
}

bool HemiMapManager::makeHemiMap(unsigned long resolution, HemiMapName outputName,
                                 Vec3 position, float orthoExtent)
{
    D3DCAPS9* const caps = static_cast<D3DCAPS9*>(dx9rend::g_deviceCaps);
    if (resolution > caps->MaxTextureHeight)
        resolution = caps->MaxTextureHeight;
    if (resolution > caps->MaxTextureWidth)
        resolution = caps->MaxTextureWidth;

    dx9rend::IRenderView* const view = static_cast<dx9rend::IRenderView*>(
        static_cast<void*>(classManager->createInstance(820301u, 820201u, 0)));
    if (!view)
        return false;

    dx9rend::IRenderView* const savedPrimary = dx9rend::rendererGetHemiPrimaryView();
    dx9rend::IRenderView* const savedSecondary = dx9rend::rendererGetHemiSecondaryView();

    Mat4 viewMatrix = Mat4::getIdentity();
    viewMatrix.m[0] = 1.0f;  viewMatrix.m[1] = 0.0f;  viewMatrix.m[2] = 0.0f;
    viewMatrix.m[4] = 0.0f;  viewMatrix.m[5] = 0.0f;  viewMatrix.m[6] = 1.0f;
    viewMatrix.m[8] = 0.0f;  viewMatrix.m[9] = -1.0f; viewMatrix.m[10] = 0.0f;
    viewMatrix.m[12] = position.x;
    viewMatrix.m[13] = position.y;
    viewMatrix.m[14] = position.z;

    const float viewportSide = static_cast<float>(resolution);
    view->setViewport(0.0f, 0.0f, viewportSide, viewportSide);
    view->setProjectionMode(1u);
    view->setNearPlane(0.01f);
    view->setFarPlane(2000.0f);
    view->setOrthoWidth(orthoExtent);
    view->setOrthoHeight(orthoExtent);
    view->setViewMatrix(viewMatrix.m);
    view->setFov(0.01745329238474369f * 45.0f);
    dx9rend::rendererInstallHemiView(view);

    dx9rend::RendDX9Base* const renderer =
        static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase);
    IDirect3DSurface9* oldDepthStencil;
    IDirect3DSurface9* newDepthStencil;
    renderer->device->GetDepthStencilSurface(&oldDepthStencil);
    renderer->device->CreateDepthStencilSurface(
        resolution, resolution, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0u, FALSE,
        &newDepthStencil, 0);
    renderer->device->SetDepthStencilSurface(newDepthStencil);

    dx9rend::EngineTexture* const savedScratch = dx9rend::rendererGetHemiScratchTexture();
    const int savedManaged = dx9rend::textureManagerSwapManaged(0);
    dx9rend::EngineTexture* const lightingTexture =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            resolution, resolution, D3DFMT_A8R8G8B8, 1u, 1u, 0));
    dx9rend::EngineTexture* const sourceTexture =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            resolution, resolution, D3DFMT_A8R8G8B8, 1u, 1u, 0));
    dx9rend::EngineTexture* const scratchTexture =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            resolution, resolution, D3DFMT_A8R8G8B8, 1u, 1u, 0));
    dx9rend::rendererSetHemiScratchTexture(scratchTexture);
    dx9rend::textureManagerSwapManaged(savedManaged);

    const HemiTerrainOverride terrainState = beginHemiTerrainOverride();
    dx9rend::stateManagerPush();
    const int savedRenderMode = dx9rend::rendererGetHemiRenderMode();
    dx9rend::rendererCaptureHemiState(m_savedState);
    dx9rend::rendererApplyHemiState();
    const float savedMinCullDistance = dx9rend::rendererSwapMinCullDistance(2000.0f);

    dx9rend::rendererSetHemiTarget(sourceTexture);
    dx9rend::rendererSetHemiRenderMode(1);
    dx9rend::effectManagerSetShowChannel(6u);

    IBase* const heightmapBase = classManager->getSingleton(
        renddx9abi::HostString("HeightmapCluster"));
    world::IHeightmapCluster* const heightmap = static_cast<world::IHeightmapCluster*>(
        static_cast<void*>(heightmapBase));
    const float savedSeaWaterLevel = heightmap->getSeaWaterLevel();
    heightmap->setSeaWaterLevel(-1000.0f);
    dx9rend::rendererSetHemiRoadPass(false);
    roadCompiledRendererSetHemiEnabled(0u);
    dx9rend::rendererRenderHemiFrame();
    heightmap->setSeaWaterLevel(savedSeaWaterLevel);

    dx9rend::rendererSetHemiTarget(lightingTexture);
    dx9rend::rendererSetHemiRenderMode(4);
    dx9rend::effectManagerSetShowChannel(1u);
    dx9rend::rendererSetHemiRoadPass(true);
    roadCompiledRendererSetHemiEnabled(1u);
    dx9rend::rendererRenderHemiFrame();

    dx9rend::rendererSwapMinCullDistance(savedMinCullDistance);
    dx9rend::rendererRestoreHemiState(m_savedState);
    dx9rend::rendererSetHemiRenderMode(savedRenderMode);
    dx9rend::effectManagerSetShowChannel(0u);
    dx9rend::stateManagerPop();
    endHemiTerrainOverride(terrainState);

    fsQuadDrawerDrawHemiMap(sourceTexture, lightingTexture);
    renderer->device->SetDepthStencilSurface(oldDepthStencil);
    dx9rend::rendererSetHemiTarget(0);
    dx9rend::rendererRestoreHemiViews(savedPrimary, savedSecondary);
    dx9rend::rendererPresentHemiResult();
    view->release();

    IDirect3DSurface9* surface =
        dx9rend::textureGetLevelSurface(sourceTexture, 0u, 0x7FFFFFFFu);
    D3DXSaveSurfaceToFileA("lightingTex.dds", D3DXIFF_DDS, surface, 0, 0);
    surface->Release();
    releaseTexture(sourceTexture);
    newDepthStencil->Release();

    // Retail releases the texture while it is still published at Renderer +0x774, then restores
    // the old pointer. This ordering is observable and intentionally not expressed as a swap.
    releaseTexture(dx9rend::rendererGetHemiScratchTexture());
    dx9rend::rendererSetHemiScratchTexture(savedScratch);

    const int conversionManaged = dx9rend::textureManagerSwapManaged(1);
    const unsigned int* const dimensions = dx9rend::textureDims(lightingTexture);
    dx9rend::EngineTexture* const conversionTexture =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            dimensions[0], dimensions[1], D3DFMT_DXT3, 0u, 0u, 0));
    if (!conversionTexture)
    {
        // The shipped severity-6 diagnostic falls through and immediately dereferences the null
        // texture. Preserve that defect: this is a report, not a recovery guard.
        ::renddx9abi::logf("HemiMapManager.cpp:413 Failed to create conversion texture");
    }

    IDirect3DSurface9* const conversionSurface =
        dx9rend::textureGetLevelSurface(conversionTexture, 0u, 0x7FFFFFFFu);
    IDirect3DSurface9* const lightingSurface =
        dx9rend::textureGetLevelSurface(lightingTexture, 0u, 0x7FFFFFFFu);
    D3DXLoadSurfaceFromSurface(
        conversionSurface, 0, 0, lightingSurface, 0, 0, 4u, 1u);
    conversionSurface->Release();
    lightingSurface->Release();
    dx9rend::textureManagerSwapManaged(conversionManaged);

    std::string outputPath(outputName.c_str());
    ::renddx9abi::engineFilePrepareWrite(outputPath);
    surface = dx9rend::textureGetLevelSurface(conversionTexture, 0u, 0x7FFFFFFFu);
    D3DXSaveSurfaceToFileA(outputPath.c_str(), D3DXIFF_DDS, surface, 0, 0);
    surface->Release();
    releaseTexture(lightingTexture);
    releaseTexture(conversionTexture);
    return true;
}

IBase* __fastcall createHemiMapManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(HemiMapManager));
    HemiMapManager* const manager = storage ? new (storage) HemiMapManager() : 0;
    if (!manager)
        return 0;

    if (!manager->queryInterface(iid))
    {
        manager->release();
        return 0;
    }
    return static_cast<IBase*>(manager);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
