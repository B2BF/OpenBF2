
// Editable surrounding terrain patches, brushes, texture updates and export.

#ifdef BF2_IMAGE_RENDDX9

#include "SurroundingPatchEditable.h"
#include "../RendDX9/RendDX9Math.h"   // rendmath::setRows (sub_10079460)
#include "../Core/StringUtil.h"         // formatString (sub_10074AB0)
#include "TerrainEditable.h"
#include "Terrain.h"
#include "QuadPatch.h"
#include "QuadPatchEditable.h"                   // ceilChop (sub_100A2FE0)
#include "XpLightManager.h"                   // LightManager +0xEC / +0xF0 - the burn's shadow bracket (R3)
#include "HemiMapManager/HemiMapDependencies.h"   // the three renderers' enable seams (R3)
#include "../GeomMisc/Debug.h"
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h"
#endif

#include "../Physics/Heightmap.h"
#include "../Physics/HeightmapCluster.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "../../RendDX9Abi/Frontier.h"        // renddx9abi::logf - dice's debug stream
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"     // classManager, fileManager
#include <string>

#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace dice {
namespace hfe {

namespace dx9rend {
// BufferManager.cpp, declared per TU as every other client does.
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);  // sub_10011FF0 on the global
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferRelease(void* wrapper);                                     // --ref, sub_10014510, free
void  bufferManagerRemoveVertexBuffer(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);

extern IBase* g_Renderer;                                                     // dword_1023D098 - BF2Render.cpp

// [CHOSEN, not recovered] (R3) the ortho top-down render's seam for the burn (0x101A5060 calls
// sub_10029DA0) - a token-FOR-token copy of BF2Render.cpp's RenderFlags (the ODR requires it; comments
// aside), exactly as QuadPatchEditable.cpp carries one. It must change with BF2Render.cpp's and retires
// with QuadPatchEditable.cpp's copy [RETIRE-WHEN: RW-277] - when BF2Render.cpp's owner publishes the
// struct and the seam in a header both TUs include.
struct RenderFlags
{
    unsigned char useRenderView;
    unsigned char field01;
    unsigned char world;
    unsigned char worldAlt;
    unsigned char field04;
    unsigned char staticMeshes;
    unsigned char skinnedMeshes;
    unsigned char decals;
    unsigned char particles;
    unsigned char field09;
    unsigned char field0a;
    unsigned char skyDome;
    unsigned char undergrowth;
    unsigned char field0d;
    unsigned char postProduction;
    unsigned char colourCorrection;
    unsigned char hud;
    unsigned char aiDebug;
    unsigned char field12;
    unsigned char pad13;
    void*         renderView;
};
#if defined(_M_IX86)
static_assert(sizeof(RenderFlags) == 24, "renderScene's toggle block is 24 bytes - see BF2Render.cpp");
#endif
void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil,
                                Vec3 position, float orthoExtent, RenderFlags flags);
} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;                                          // ModuleGeom.cpp, dword_1024B914

namespace {

inline dx9rend::ShaderParameter* param(void* handle)
{
    return static_cast<dx9rend::ShaderParameter*>(handle);
}

inline dx9rend::EngineTexture* engineTexture(void* texture)
{
    return static_cast<dx9rend::EngineTexture*>(texture);
}

inline ::dice::hfe::world::IHeightmapCluster* heightmapClusterByName()
{
    IBase* const found = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("HeightmapCluster"));
    return static_cast< ::dice::hfe::world::IHeightmapCluster*>(static_cast<void*>(found));
}

inline dx9rend::ShaderParameter* effectParameter(void* effect, const char* name)
{
    return dx9rend::effectGetShaderParameterBorrowedTyped(static_cast<dx9rend::Effect*>(effect),
                                                          std::string(name));
}


// The "lock, fill every texel of mip 0, unlock" the texture-creating arms open-code (textureLock
// sub_10014BA0 over the whole record, textureUnlock sub_10014D30). [INFERRED] Shared spelling of
// init's three inlined fills and reloadLightmap's: the owner slot is reread after locking for the
// unlock receiver, as each listing does.
template<class TexturePointer>
void fillTextureSlot(TexturePointer& texture, unsigned int value)
{
    dx9rend::TextureLockRecord* const record = dx9rend::textureLock(engineTexture(texture), 0, 0x7FFFFFFF);
    unsigned int* texel = static_cast<unsigned int*>(record->pBits);
    unsigned int* const end = texel + record->width * record->height;
    for (; texel != end; ++texel)
        *texel = value;
    dx9rend::textureUnlock(engineTexture(texture), record);
}

// the engine's in-place normalise (QuadPatch.cpp carries the read): |v|^2 =
// (x*x + y*y) + z*z as a float; within FLT_EPSILON of 1 left alone; below FLT_EPSILON zeroed (answer
// -20001); otherwise each component scaled by 1 / sqrt(|v|^2). The callers here drop the answer.
int normalizeInPlace(Vec3& v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return 0;
    if (std::fabs(lengthSquared) < FLT_EPSILON)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return -20001;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    v.x = inverse * v.x;
    v.y = inverse * v.y;
    v.z = inverse * v.z;
    return 0;
}

// A locked rect's row length in texels: `Pitch >> 2`, unsigned as shipped (`shr`).
inline int texelPitch(const D3DLOCKED_RECT& locked)
{
    return static_cast<int>(static_cast<unsigned int>(locked.Pitch) >> 2);
}

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [INFERRED] Shared spelling of the three brushes' FCOS/clamp/strength arithmetic.
// [CHOSEN, not recovered] Native widths use the existing bf2x87 transcendental policy.
bf2x87::Real surroundingBrushWeight(bf2x87::Real distance, float inverse, int strength)
{
    bf2x87::Real falloff = bf2x87::cosValue(bf2x87::mul(bf2x87::mul(distance,
        bf2x87::Real(inverse)), bf2x87::Real(1.5707964f)));
    if (!bf2x87::less(bf2x87::Real(0), falloff))
        falloff = bf2x87::Real(0);
    return bf2x87::mul(bf2x87::mul(bf2x87::Real(strength), falloff), bf2x87::Real(0.0039215689f));
}
#endif

// The placement transform heightSubGrid's two crack-fix samples use (0x101A3647.. / 0x101A36C4..):
// wz = ((pz * m5 + px * m1) + m9 * 0) + m13 and wx = ((pz * m4 + px * m0) + m8 * 0) + m12, then the
// cluster height at (wx, wz) (+0x6C).
float crackSampleHeight(::dice::hfe::world::IHeightmapCluster* cluster, const float* m, float px, float pz)
{
    const float wz = ((pz * m[5] + px * m[1]) + m[9] * 0.0f) + m[13];
    const float wx = ((pz * m[4] + px * m[0]) + m[8] * 0.0f) + m[12];
    return cluster->getHeightInWorldCoords(wx, wz);
}

} // anonymous namespace


// Construction and teardown (R1)

// (`retn 4`) - SurroundingPatch's constructor, the owner, three texture slots zeroed, the
// three layer sets (records created = 0, data = 0; the merged set's source is not set), then the body:
// +0x178 / +0x17C zeroed and +0x170 / +0x168 zeroed again (the shipped store sequence), and the two
// flag bytes. +0x2F8 / +0x2F9 are not touched.
// PE 0x101A2400
SurroundingPatchEditable::SurroundingPatchEditable(TerrainEditable* owner)
    : SurroundingPatch(owner),
      m_colormapEdit(0),
      m_colormap(0),
      m_lightmap(0),
      editableTerrain(owner)
{
    m_texture178 = 0;
    m_texture17C = 0;
    m_lightmap = 0;
    m_colormapEdit = 0;
    m_colormapReady = 0;
    m_burnPass = 0;
}

// (slot +0x00 is 0x101A5F80, which adds the free) - five texture owner references in the
// shipped order (+0x178, +0x17C, +0x16C, +0x170, +0x168; none nulled), then the members - the merged,
// edit and imported sets, each freeing its maps last to first (0x1019B000) - and ~SurroundingPatch.
// PE 0x101A24E0
SurroundingPatchEditable::~SurroundingPatchEditable()
{
    terrainReleaseTexture(m_texture178);
    terrainReleaseTexture(m_texture17C);
    terrainReleaseTexture(m_colormap);
    terrainReleaseTexture(m_lightmap);
    terrainReleaseTexture(m_colormapEdit);
}


// Places the ring patch and loads managed editor textures, creating missing defaults.
// Rebuilds bounds and buffers after restoring the texture flag. Existing resources are
// not released first; the constructed game-colormap path is intentionally unused.
// PE 0x101A4710
void SurroundingPatchEditable::init(int newSlot)
{
    slot = newSlot;
    if (editableTerrain->m_surroundingTerrainEngineMode)
        setupPlacement();
    else
        setupEditorPlacement();

    std::string colormapPath;
    {
        const std::string base = editableTerrain->getEditorColormapBaseName(); // P+0xFC
        std::string name;
        name = formatString("%s_s%i", base.c_str(), slot);
        colormapPath = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) + name; // E+0x90
    }
    std::string unusedGamePath;
    {
        TerrainEditableNameResult base = editableTerrain->getColormapBaseName(); // E+0x6C
        std::string name;
        name = formatString("%s_s%i", ::renddx9abi::fromHostString(base).c_str(), slot);
        unusedGamePath = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) + name;
    }
    const int savedManaged = dx9rend::textureManagerSwapManaged(1);

    bool exists;
    {
        std::string dds(colormapPath);
        dds += ".dds";
        exists = ::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds));
    }
    {
        if (exists)
        {
            m_colormap = engineTexture(dx9rend::textureManagerGetTexture(colormapPath));
        }
        else
        {
            m_colormap = engineTexture(dx9rend::textureManagerCreateEmpty(
                static_cast<unsigned int>(editableTerrain->m_surroundingTextureSize),
                static_cast<unsigned int>(editableTerrain->m_surroundingTextureSize), 21, 1, 0, 0));
            fillTextureSlot(m_colormap, 0xFF00FF00u);
        }
    }
    m_colormapReady = 1;

    std::string lightmapPath;
    {
        const std::string base = editableTerrain->getEditorLightmapBaseName(); // P+0x104
        std::string name;
        name = formatString("%s_s%i", base.c_str(), slot);
        lightmapPath = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) + name;
    }
    {
        std::string dds(lightmapPath);
        dds += ".dds";
        exists = ::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds));
    }
    {
        if (exists)
        {
            m_lightmap = engineTexture(dx9rend::textureManagerGetTexture(lightmapPath));
        }
        else
        {
            m_lightmap = engineTexture(dx9rend::textureManagerCreateEmpty(
                static_cast<unsigned int>(editableTerrain->m_surroundingTextureSize),
                static_cast<unsigned int>(editableTerrain->m_surroundingTextureSize), 21, 1, 0, 0));
            fillTextureSlot(m_lightmap, 0xFFFFFFFFu);
        }
    }
    m_colormapEdit = 0;
    m_texture178 = 0;
    m_texture17C = 0;

    std::string suffix;
    suffix = formatString("_s%i", slot);
    std::string lowDetailPath;
    {
        const std::string base = editableTerrain->getEditorLowDetailmapBaseName(); // P+0x10C
        lowDetailPath = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) + base + suffix; // E+0x90
    }
    {
        std::string dds(lowDetailPath);
        dds += ".dds";
        exists = ::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds));
    }
    {
        if (exists)
        {
            lowDetailmap = dx9rend::textureManagerGetTexture(lowDetailPath);
        }
        else
        {
            lowDetailmap = dx9rend::textureManagerCreateEmpty(
                static_cast<unsigned int>(editableTerrain->m_surroundingTextureSize),
                static_cast<unsigned int>(editableTerrain->m_surroundingTextureSize), 21, 1, 0, 0);
            fillTextureSlot(lowDetailmap, 0x00FF0000u);
        }
    }
    dx9rend::textureManagerSwapManaged(savedManaged);
    computeBounds();
    buildVertexBuffers();
}

// Places slots 0..7 around the terrain using an unrotated secondary-scale grid.
// Invalid slots retain the previous origin. Updates the local box but not derived bounds.
// PE 0x101A2610
void SurroundingPatchEditable::setupEditorPlacement()
{
    TerrainEditable* const te = editableTerrain;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] FISTP's invalid result and 32-bit integer wrap on native ports.
    secondaryScaleX = bf2x87::toIntTruncate(bf2x87::Real(te->m_secondaryWorldScale.x));
    span = static_cast<int>(static_cast<unsigned int>(bf2x87::toIntTruncate(
        bf2x87::Real(te->m_primaryWorldScale.x))) * (static_cast<unsigned int>(te->m_heightmapSizeX) - 1u));
    unresolved48 = span / bf2x87::toIntTruncate(bf2x87::Real(te->m_secondaryWorldScale.x));
    spanPlusOne = static_cast<int>(static_cast<unsigned int>(span) + 1u);
    sideCount = static_cast<int>(static_cast<unsigned int>(span / secondaryScaleX) + 1u);
    sideCountSquared = static_cast<int>(static_cast<unsigned int>(sideCount) * static_cast<unsigned int>(sideCount));
    const int half = span / 2;
    const int left = static_cast<int>(0u - static_cast<unsigned int>(span) - static_cast<unsigned int>(half));
    const int right = static_cast<int>(static_cast<unsigned int>(span) - static_cast<unsigned int>(half));
    switch (slot)
    {
    case 0: originX = left;  originZ = right; break;
    case 1: originX = -half; originZ = right; break;
    case 2: originX = right; originZ = right; break;
    case 3: originX = right; originZ = -half; break;
    case 4: originX = right; originZ = left;  break;
    case 5: originX = -half; originZ = left;  break;
    case 6: originX = left;  originZ = left;  break;
    case 7: originX = left;  originZ = -half; break;
    default: break;
    }
#else
    secondaryScaleX = static_cast<int>(te->m_secondaryWorldScale.x);
    span = static_cast<int>(te->m_primaryWorldScale.x) * (te->m_heightmapSizeX - 1);
    unresolved48 = span / static_cast<int>(te->m_secondaryWorldScale.x);
    spanPlusOne = span + 1;
    sideCount = span / secondaryScaleX + 1;
    sideCountSquared = sideCount * sideCount;

    const int h = span;
    switch (slot)
    {
    case 0: originX = -h - h / 2; originZ = h - h / 2;  break;
    case 1: originX = h / -2;     originZ = h - h / 2;  break;
    case 2: originX = h - h / 2;  originZ = h - h / 2;  break;
    case 3: originX = h - h / 2;  originZ = h / -2;     break;
    case 4: originX = h - h / 2;  originZ = -h - h / 2; break;
    case 5: originX = h / -2;     originZ = -h - h / 2; break;
    case 6: originX = -h - h / 2; originZ = -h - h / 2; break;
    case 7: originX = -h - h / 2; originZ = h / -2;     break;
    default: break;
    }
#endif
    if (static_cast<unsigned int>(slot) > 7u)
    {
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\SurroundingPatchEditable.cpp",
            133, std::string()) << "TBM: unknown surrounding terrain patch type!";
    }

    const float step = static_cast<float>(secondaryScaleX);
    const float row0[4] = { step, 0.0f, 0.0f, 0.0f };
    const float row1[4] = { 0.0f, step, 0.0f, 0.0f };
    const float row2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    const float row3[4] = { static_cast<float>(originX), static_cast<float>(originZ), 0.0f, 1.0f };
    rendmath::setRows(placement, row0, row1, row2, row3);
    isCorner = 0;

    originXf = static_cast<float>(originX);
    zero5C = 0.0f;
    zero64 = 0.0f;
    originZf = static_cast<float>(originZ);
    const Vec3* const origin = terrainOriginScratch(*reinterpret_cast<const Vec3*>(&originXf));
    center.x = origin->x;
    center.y = origin->y;
    center.z = origin->z;
    center.x = static_cast<float>(static_cast<double>(span) * 0.5 + center.x);
    center.z = static_cast<float>(static_cast<double>(span) * 0.5 + center.z);

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    endX = static_cast<int>(static_cast<unsigned int>(originX) + static_cast<unsigned int>(span));
    endZ = static_cast<int>(static_cast<unsigned int>(originZ) + static_cast<unsigned int>(span));
#else
    endX = originX + span;
    endZ = originZ + span;
#endif
    TerrainAabb box;
    box.min.x = static_cast<float>(originX);
    box.min.y = fileMinY;
    box.min.z = static_cast<float>(originZ);
    box.max.x = static_cast<float>(endX);
    box.max.y = fileMaxY;
    box.max.z = static_cast<float>(endZ);
    box.cornerLength = 0.0f;
    box.radius = 0.0f;
    box.dirty = 1;
    box.extent.x = 0.0f; box.extent.y = 0.0f; box.extent.z = 0.0f;
    box.centre.x = 0.0f; box.centre.y = 0.0f; box.centre.z = 0.0f;
    aabbCorners(corners, &box.min.x);                                         // sub_101C6750 on +0x108
}

// the file height range and the bounding sphere over the side grid: the secondary scale
// asked through E+0x3C (x and z kept), new Vec3[sideCount^2] (engine array heap, zeroed when the
// allocation answered - used unguarded when it did not), max seeded FLT_MIN and min FLT_MAX, then
// for z, x stepping by +0x34 while below span + 1: the cluster height at (originX + x, originZ + z)
// (+0x6C) - `max < h` raises the maximum, `!(min < h)` lowers the minimum (a NaN lands in the
// minimum) - and the point ((originX + x) * scale.x, h, (originZ + z) * scale.z).
// D3DXComputeBoundingSphere into +0x70 / +0x7C, the array freed, and the corners of a local box.
// PE 0x101A4150
void SurroundingPatchEditable::computeBounds()
{
    const Vec3& secondary = editableTerrain->getSecondaryWorldScale();       // E+0x3C
    const float scaleX = secondary.x;
    const float scaleZ = secondary.z;
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Keep the x86 allocation/count arithmetic in 32 bits.
    const int count = static_cast<int>(static_cast<unsigned int>(sideCount) * static_cast<unsigned int>(sideCount));
#else
    const int count = sideCount * sideCount;
#endif
    Vec3* points = 0;
    {
        void* const storage = ::renddx9abi::engineAlloc(12u * static_cast<unsigned int>(count));
        if (storage)
        {
            points = static_cast<Vec3*>(storage);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const int last = static_cast<int>(static_cast<unsigned int>(count) - 1u);
            if (last >= 0)
            {
                Vec3* point = points;
                unsigned int remaining = static_cast<unsigned int>(last) + 1u;
                do
                {
                    point->x = 0.0f;
                    point->y = 0.0f;
                    point->z = 0.0f;
                    ++point;
                }
                while (--remaining);
            }
#else
            for (int i = 0; i < count; ++i)
            {
                points[i].x = 0.0f;
                points[i].y = 0.0f;
                points[i].z = 0.0f;
            }
#endif
        }
    }
    // shipped defect: an all-negative grid keeps the positive FLT_MIN maximum.
    fileMaxY = FLT_MIN;
    fileMinY = FLT_MAX;
    int next = 0;
    if (spanPlusOne > 0)
    {
        int z = 0;
        do
        {
            if (spanPlusOne > 0)
            {
                Vec3* point = points + next;
                int x = 0;
                do
                {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                    const float worldZ = static_cast<float>(static_cast<int>(static_cast<unsigned int>(z) + static_cast<unsigned int>(originZ)));
                    const float worldX = static_cast<float>(static_cast<int>(static_cast<unsigned int>(x) + static_cast<unsigned int>(originX)));
#else
                    const float worldZ = static_cast<float>(z + originZ);
                    const float worldX = static_cast<float>(x + originX);
#endif
                    const float height = cluster->getHeightInWorldCoords(worldX, worldZ);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                    // [CHOSEN, not recovered] Retain FILD products until their actual stores.
                    if (bf2x87::less(bf2x87::Real(fileMaxY), bf2x87::Real(height)))
                        fileMaxY = height;
                    if (!bf2x87::less(bf2x87::Real(fileMinY), bf2x87::Real(height)))
                        fileMinY = height;
                    next = static_cast<int>(static_cast<unsigned int>(next) + 1u);
                    const int pointZ = static_cast<int>(static_cast<unsigned int>(z) + static_cast<unsigned int>(originZ));
                    const int pointX = static_cast<int>(static_cast<unsigned int>(x) + static_cast<unsigned int>(originX));
                    const bf2x87::Real zProduct = bf2x87::mul(bf2x87::Real(pointZ), bf2x87::Real(scaleZ));
                    point->x = bf2x87::store(bf2x87::mul(bf2x87::Real(pointX), bf2x87::Real(scaleX)));
                    point->y = height;
                    point->z = bf2x87::store(zProduct);
                    ++point;
                    x = static_cast<int>(static_cast<unsigned int>(x) + static_cast<unsigned int>(secondaryScaleX));
#else
                    if (fileMaxY < height)
                        fileMaxY = height;
                    if (!(fileMinY < height))
                        fileMinY = height;
                    ++next;
                    point->x = static_cast<float>(x + originX) * scaleX;
                    point->y = height;
                    point->z = static_cast<float>(z + originZ) * scaleZ;
                    ++point;
                    x += secondaryScaleX;
#endif
                }
                while (x < spanPlusOne);
            }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            z = static_cast<int>(static_cast<unsigned int>(z) + static_cast<unsigned int>(secondaryScaleX));
#else
            z += secondaryScaleX;
#endif
        }
        while (z < spanPlusOne);
    }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const DWORD sphereCount = static_cast<unsigned int>(sideCount) * static_cast<unsigned int>(sideCount);
#else
    const DWORD sphereCount = static_cast<DWORD>(sideCount * sideCount);
#endif
    D3DXComputeBoundingSphere(reinterpret_cast<const D3DXVECTOR3*>(points), sphereCount,
                              12, reinterpret_cast<D3DXVECTOR3*>(&boundsCenter), &boundsRadius);
    ::renddx9abi::engineFree(points);

    TerrainAabb box;
    box.min.x = static_cast<float>(originX);
    box.min.y = fileMinY;
    box.min.z = static_cast<float>(originZ);
    box.max.x = static_cast<float>(endX);
    box.max.y = fileMaxY;
    box.max.z = static_cast<float>(endZ);
    box.cornerLength = 0.0f;
    box.radius = 0.0f;
    box.dirty = 1;
    box.extent.x = 0.0f; box.extent.y = 0.0f; box.extent.z = 0.0f;
    box.centre.x = 0.0f; box.centre.y = 0.0f; box.centre.z = 0.0f;
    aabbCorners(corners, &box.min.x);
}

// PE 0x101A3580: appends inclusive height-grid samples to out, advancing cursor for each value.
// Heights divide by secondary y scale. Flag bits 0/1 average odd samples along the
// final row/column to close cracks between grid resolutions.
void SurroundingPatchEditable::heightSubGrid(int x0, int z0, int x1, int z1, int stepX, int stepZ,
                                             float* out, int* cursor, int flags)
{
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const float secondaryY = editableTerrain->m_secondaryWorldScale.y;      // +0x24, read once
    const float* const m = placement;
    int zIndex = 0;
    for (int z = z0; z < z1 + 1; z += stepZ, ++zIndex)
    {
        const float inverse = 1.0f / secondaryY;
        int xIndex = 0;
        for (int x = x0, xPrevious = x0 - stepX; x < x1 + 1; x += stepX, xPrevious += stepX, ++xIndex)
        {
            float value;
            if ((flags & 1) && z == z1 && xIndex % 2 != 0)
            {
                const float pz = static_cast<float>(z);
                const float first = crackSampleHeight(cluster, m, static_cast<float>(xPrevious), pz) * inverse;
                value = (crackSampleHeight(cluster, m, static_cast<float>(x + stepX), pz) * inverse + first) * 0.5f;
            }
            else if ((flags & 2) && x == x1 && zIndex % 2 != 0)
            {
                const float px = static_cast<float>(x);
                const float first = crackSampleHeight(cluster, m, px, static_cast<float>(z - stepZ)) * inverse;
                value = (crackSampleHeight(cluster, m, px, static_cast<float>(z + stepZ)) * inverse + first) * 0.5f;
            }
            else
            {
                const float px = static_cast<float>(x);
                const float pz = static_cast<float>(z);
                const float worldZ = ((pz * m[5] + px * m[1]) + m[9] * 0.0f) + m[13];
                const float worldX = ((pz * m[4] + m[8] * 0.0f) + px * m[0]) + m[12];
                value = cluster->getHeightInWorldCoords(worldX, worldZ) * inverse;
            }
            out[(*cursor)++] = value;
        }
    }
}

// PE 0x101A38A0: appends normals from a bordered world-space sample grid and advances cursor.
// Each normal averages six neighboring triangle crosses before normalization; flags are unused.
void SurroundingPatchEditable::normalSubGrid(int x0, int z0, int x1, int z1, int stepX, int stepZ,
                                             Vec3* out, int* cursor, int)
{
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const int rows = (z1 - z0) / stepZ + 1;
    const int columns = (x1 - x0) / stepX + 1;
    const int stride = (x1 - x0) / stepX + 3;
    const int total = stride * ((z1 - z0) / stepZ + 3);
    Vec3* grid = 0;
    {
        void* const storage = ::renddx9abi::engineAlloc(static_cast<unsigned int>(12 * total));
        if (storage)
        {
            grid = static_cast<Vec3*>(storage);
            for (int i = 0; i < total; ++i)
            {
                grid[i].x = 0.0f;
                grid[i].y = 0.0f;
                grid[i].z = 0.0f;
            }
        }
    }
    const float* const m = placement;
    if (rows + 1 > -1)
    {
        int next = 0;
        int z = z0 - stepZ;
        for (int rowCount = rows + 2; rowCount; --rowCount, z += stepZ)
        {
            if (columns + 1 > -1)
            {
                const float pz = static_cast<float>(z);
                Vec3* sample = grid + next;
                next += columns + 2;
                int x = x0 - stepX;
                for (int columnCount = columns + 2; columnCount; --columnCount, x += stepX, ++sample)
                {
                    const float px = static_cast<float>(x);
                    const float worldZ = ((pz * m[5] + px * m[1]) + m[9] * 0.0f) + m[13];
                    const float worldX = ((px * m[0] + pz * m[4]) + m[8] * 0.0f) + m[12];
                    sample->x = worldX;
                    sample->y = cluster->getHeightInWorldCoords(worldX, worldZ);
                    sample->z = worldZ;
                }
            }
        }
    }
    if (rows + 1 > 1)
    {
        for (int r = 1; r <= rows; ++r)
        {
            if (columns + 1 <= 1)
                continue;
            for (int c = 1; c <= columns; ++c)
            {
                const Vec3& centre = grid[r * stride + c];
                const Vec3& upCell = grid[(r - 1) * stride + c];
                const Vec3& upRightCell = grid[(r - 1) * stride + c + 1];
                const Vec3& rightCell = grid[r * stride + c + 1];
                const Vec3& downCell = grid[(r + 1) * stride + c];
                const Vec3& downLeftCell = grid[(r + 1) * stride + c - 1];
                const Vec3& leftCell = grid[r * stride + c - 1];
                const float ux = upCell.x - centre.x,       uy = upCell.y - centre.y,       uz = upCell.z - centre.z;
                const float ax = upRightCell.x - centre.x,  ay = upRightCell.y - centre.y,  az = upRightCell.z - centre.z;
                const float rx = rightCell.x - centre.x,    ry = rightCell.y - centre.y,    rz = rightCell.z - centre.z;
                const float dx = downCell.x - centre.x,     dy = downCell.y - centre.y,     dz = downCell.z - centre.z;
                const float bx = downLeftCell.x - centre.x, by = downLeftCell.y - centre.y, bz = downLeftCell.z - centre.z;
                const float lx = leftCell.x - centre.x,     ly = leftCell.y - centre.y,     lz = leftCell.z - centre.z;
                // n1 = U x UR, n2 = UR x R, n3 = R x D, n4 = D x DL, n5 = DL x L, n6 = L x U.
                const float n1x = az * uy - ay * uz, n1y = uz * ax - az * ux, n1z = ay * ux - uy * ax;
                const float n2x = rz * ay - ry * az, n2y = az * rx - rz * ax, n2z = ry * ax - ay * rx;
                const float n3x = dz * ry - dy * rz, n3y = rz * dx - dz * rx, n3z = dy * rx - ry * dx;
                const float n4x = bz * dy - by * dz, n4y = dz * bx - bz * dx, n4z = by * dx - dy * bx;
                const float n5x = lz * by - ly * bz, n5y = bz * lx - lz * bx, n5z = ly * bx - by * lx;
                const float n6x = ly * uz - lz * uy, n6y = lz * ux - uz * lx, n6z = uy * lx - ly * ux;
                Vec3 normal;
                normal.x = (((((n2x + n1x) + n3x) + n4x) + n5x) + n6x) * 0.16666667f;
                normal.y = (((((n2y + n1y) + n3y) + n4y) + n5y) + n6y) * 0.16666667f;
                normal.z = (((((n2z + n1z) + n3z) + n4z) + n5z) + n6z) * 0.16666667f;
                normalizeInPlace(normal);
                out[(*cursor)++] = normal;
            }
        }
    }
    ::renddx9abi::engineFree(grid);
}

// PE 0x101A3D70: rebuilds height and normal streams using editor or stepped engine-mode grids.
// Editor placement must initialize the step count first; an engine-only first placement
// leaves it uninitialized. Old wrappers are released without clearing their pointers.
void SurroundingPatchEditable::buildVertexBuffers()
{
    if (xzVertexBuffer)
    {
        dx9rend::bufferManagerRemoveVertexBuffer(xzVertexBuffer);
        dx9rend::vertexBufferRelease(xzVertexBuffer);
    }
    if (heightVertexBuffer)
    {
        dx9rend::bufferManagerRemoveVertexBuffer(heightVertexBuffer);
        dx9rend::vertexBufferRelease(heightVertexBuffer);
    }
    *reinterpret_cast<unsigned char*>(&unresolved80) = 1;                     // byte store at +0x80
    heightmapClusterByName();                                                 // answer unused

    TerrainEditable* const te = editableTerrain;
    const unsigned int count = isCorner ? te->m_cornerVertexCount : te->m_sideVertexCount;
    xzVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(count, 4u, 8u, "SurroundingPatchEditable");
    float* const heights = static_cast<float*>(dx9rend::vertexBufferLock(xzVertexBuffer, 0, 0, 0));
    int cursor = 0;
    if (!te->m_surroundingTerrainEngineMode)
    {
        heightSubGrid(0, 0, unresolved48, unresolved48, 1, 1, heights, &cursor, 0);
    }
    else if (isCorner)
    {
        heightSubGrid(0, 0, unresolved48 / 4, unresolved48 / 4, 1, 1, heights, &cursor, 3);
        heightSubGrid(unresolved48 / 4, 0, unresolved48 / 2, unresolved48 / 4, 2, 2, heights, &cursor, 2);
        heightSubGrid(0, unresolved48 / 4, unresolved48 / 2, unresolved48 / 2, 2, 2, heights, &cursor, 3);
        heightSubGrid(unresolved48 / 2, 0, unresolved48, unresolved48 / 2, 4, 4, heights, &cursor, 0);
        heightSubGrid(0, unresolved48 / 2, unresolved48, unresolved48, 4, 4, heights, &cursor, 0);
    }
    else
    {
        heightSubGrid(0, 0, unresolved48, unresolved48 / 4, 1, 1, heights, &cursor, 1);
        heightSubGrid(0, unresolved48 / 4, unresolved48, unresolved48 / 2, 2, 2, heights, &cursor, 1);
        heightSubGrid(0, unresolved48 / 2, unresolved48, unresolved48, 4, 4, heights, &cursor, 0);
    }
    dx9rend::vertexBufferUnlock(xzVertexBuffer);

    heightVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(count, 12u, 8u, "SurroundingPatchEditable");
    Vec3* const normals = static_cast<Vec3*>(dx9rend::vertexBufferLock(heightVertexBuffer, 0, 0, 0));
    cursor = 0;
    if (!te->m_surroundingTerrainEngineMode)
    {
        normalSubGrid(0, 0, unresolved48, unresolved48, 1, 1, normals, &cursor, 0);
    }
    else if (isCorner)
    {
        normalSubGrid(0, 0, unresolved48 / 4, unresolved48 / 4, 1, 1, normals, &cursor, 0);
        normalSubGrid(unresolved48 / 4, 0, unresolved48 / 2, unresolved48 / 4, 2, 2, normals, &cursor, 0);
        normalSubGrid(0, unresolved48 / 4, unresolved48 / 2, unresolved48 / 2, 2, 2, normals, &cursor, 0);
        normalSubGrid(unresolved48 / 2, 0, unresolved48, unresolved48 / 2, 4, 4, normals, &cursor, 0);
        normalSubGrid(0, unresolved48 / 2, unresolved48, unresolved48, 4, 4, normals, &cursor, 0);
    }
    else
    {
        normalSubGrid(0, 0, unresolved48, unresolved48 / 4, 1, 1, normals, &cursor, 0);
        normalSubGrid(0, unresolved48 / 4, unresolved48, unresolved48 / 2, 2, 2, normals, &cursor, 0);
        normalSubGrid(0, unresolved48 / 2, unresolved48, unresolved48, 4, 4, normals, &cursor, 0);
    }
    dx9rend::vertexBufferUnlock(heightVertexBuffer);
}


// R2 - the draws

// PE 0x101A20F0 (slot +0x04) - only when SurroundingPatch::isVisible (0x1018F420): TEXLAYER1 = the
// lightmap, TEXLAYER4 = the low-detail map (+0x14) and settransxz = the placement (both by name),
// the height stream on 1 and the normal stream on 2, TEXLAYER0 = the edit copy when there is one,
// else the colormap; pass 1 when +0x2FB is set, else 0 (the ring draw opened the technique); one
// triangle-list draw of the patch kind's grid (+0xF4 / +0xFC + 4 * isCorner).
void SurroundingPatchEditable::draw(int)
{
    if (!isVisible(0))
        return;
    TerrainEditable* const te = editableTerrain;
    param(te->m_hTexLayer[1])->setValue(m_lightmap, 0);                       // +0x1DC
    effectParameter(te->m_effect, "TEXLAYER4")->setValue(lowDetailmap, 0);
    effectParameter(te->m_effect, "SETTRANSXZ")->setValue(placement, 0);
    dx9rend::vertexBufferBindStream(xzVertexBuffer, 1);                       // +0x1C - FLOAT1 heights
    dx9rend::vertexBufferBindStream(heightVertexBuffer, 2);                   // +0x20 - normals
    param(te->m_hTexLayer[0])->setValue(m_colormapEdit ? m_colormapEdit : m_colormap, 0);   // +0x1D8
    if (m_burnPass)
        dx9rend::effectBeginPass(te->m_effect, 1);
    else
        dx9rend::effectBeginPass(te->m_effect, 0);
    const unsigned int primitives = (isCorner ? te->m_cornerIndexCount : te->m_sideIndexCount) / 3;
    const unsigned int vertices = isCorner ? te->m_cornerVertexCount : te->m_sideVertexCount;
    dx9rend::rendererDrawIndexedPrimitive(primitives, 4u, 0, 0u, vertices, 0u);
}

// Two orientation maps of a point (x, z) for the patch's slot, picked by a jump table over slot 0..7 and
// returning one of x, z, span - x or span - z (span is the int +0x38, converted with fild). Any other slot
// raises the retail Error "TBM: unknown surrounding terrain patch type!" (SurroundingPatchEditable.cpp,
// lines 529 / 553) and answers 0. Slot 0 and 7 share an arm, as do 1 / 2, 3 / 4 and 5 / 6.
// unreachable in 1.50: no code or data references. [INFERRED] names.
// PE 0x101A1F30: slot 0 / 7 -> z, 1 / 2 -> x, 3 / 4 -> span - z, 5 / 6 -> span - x.
float SurroundingPatchEditable::orientedX(float x, float z)
{
    switch (slot)
    {
    case 0:
    case 7: return z;
    case 1:
    case 2: return x;
    case 3:
    case 4: return static_cast<float>(span) - z;
    case 5:
    case 6: return static_cast<float>(span) - x;
    default: break;
    }
    ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\SurroundingPatchEditable.cpp",
        529, std::string()) << "TBM: unknown surrounding terrain patch type!";
    return 0.0f;
}

// PE 0x101A2010: slot 0 / 7 -> span - x, 1 / 2 -> z, 3 / 4 -> x, 5 / 6 -> span - z.
float SurroundingPatchEditable::orientedZ(float x, float z)
{
    switch (slot)
    {
    case 0:
    case 7: return static_cast<float>(span) - x;
    case 1:
    case 2: return z;
    case 3:
    case 4: return x;
    case 5:
    case 6: return static_cast<float>(span) - z;
    default: break;
    }
    ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\SurroundingPatchEditable.cpp",
        553, std::string()) << "TBM: unknown surrounding terrain patch type!";
    return 0.0f;
}

// the lightmap-generation pass: when visible, TEXLAYER0 = the colormap, settransxz = the
// placement, the height stream on 1, and one draw over the side grid's counts (+0xF4 / +0xFC) whatever
// the patch kind (TerrainEditable binds only the side buffers for this pass). The index count is
// divided signed (`imul 0x55555556`, 0x101A22E9).
// PE 0x101A2250
void SurroundingPatchEditable::drawForLightmapGeneration()
{
    if (!isVisible(0))
        return;
    TerrainEditable* const te = editableTerrain;
    param(te->m_hTexLayer[0])->setValue(m_colormap, 0);
    effectParameter(te->m_effect, "SETTRANSXZ")->setValue(placement, 0);
    dx9rend::vertexBufferBindStream(xzVertexBuffer, 1);
    dx9rend::rendererDrawIndexedPrimitive(
        static_cast<unsigned int>(static_cast<int>(te->m_sideIndexCount) / 3), 4u, 0, 0u,
        te->m_sideVertexCount, 0u);
}

// The placement rectangle at height 0, in the shipped store order.
// PE 0x101A2320
void SurroundingPatchEditable::getWorldBounds(TerrainAabb& box)
{
    box.min.y = 0.0f;
    box.min.x = static_cast<float>(originX);                                  // +0x24
    box.min.z = static_cast<float>(originZ);                                  // +0x28
    box.max.y = 0.0f;
    box.max.x = static_cast<float>(endX);                                     // +0x2C
    box.max.z = static_cast<float>(endZ);                                     // +0x30
}


// R2 - textures

// only when there is none: a +0x50-sized A8R8G8B8 copy (stored first) of the colormap,
// loaded by D3DX, the copy's surface released first.
// PE 0x101A1E90
void SurroundingPatchEditable::makeColormapEditCopy()
{
    if (m_colormapEdit)
        return;
    const unsigned int size = static_cast<unsigned int>(editableTerrain->m_surroundingTextureSize);
    m_colormapEdit = engineTexture(dx9rend::textureManagerCreateEmpty(size, size, 21, 1, 0, 0));
    IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_colormap, 0, 0x7FFFFFFF);
    D3DXLoadSurfaceFromSurface(destination, 0, 0, source, 0, 0, 0xFFFFFFFFu, 0);
    destination->Release();
    source->Release();
}

// PE 0x101A2360: with a colormap edit copy, every texel of its level 0 zeroed over (0, 0, size, size),
// the size read from the terrain's +0x48 (the patch colormap size, not the +0x50 surrounding size the
// commit uses); x outer, z inner, the pitch honoured; unlocked and released. Without a copy nothing
// happens. unreachable in 1.50: no code or data references. [INFERRED] name.
void SurroundingPatchEditable::clearColormapEdit()
{
    if (!m_colormapEdit)
        return;
    const int size = editableTerrain->m_patchColormapSize;                    // +0x48
    RECT rect = { 0, 0, size, size };
    IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT locked;
    surface->LockRect(&locked, &rect, 0);
    unsigned int* const bits = static_cast<unsigned int*>(locked.pBits);
    for (int x = 0; x < size; ++x)
        for (int z = 0; z < size; ++z)
            bits[x + z * texelPitch(locked)] = 0;
    surface->UnlockRect();
    surface->Release();
}

// PE 0x101A29F0: the +0x178 texture, when set, loses its owner reference, is evicted from the texture
// cache and is nulled. unreachable in 1.50: no code or data references. [INFERRED] name; the texture
// itself is [UNRESOLVED] (the constructor zeroes it and nothing else stores it).
void SurroundingPatchEditable::releaseTexture178()
{
    if (m_texture178)
    {
        terrainReleaseTexture(m_texture178);
        dx9rend::textureManagerEvictFromCache(m_texture178);
        m_texture178 = 0;
    }
}

// PE 0x101A2A40: the same for the +0x17C texture. unreachable in 1.50: no code or data references.
void SurroundingPatchEditable::releaseTexture17C()
{
    if (m_texture17C)
    {
        terrainReleaseTexture(m_texture17C);
        dx9rend::textureManagerEvictFromCache(m_texture17C);
        m_texture17C = 0;
    }
}

// with a copy: both locked over (0, 0, size, size) (size = +0x50), flags 0, the colormap
// first; per texel (x outer, z inner, pitch honoured) the colormap texel lerped toward the copy's by
// the copy'S alpha, packed back, and the copy's texel zeroed. Unlocked and released, the colormap's
// first. Then the copy dropped - evicted before its owner reference is released - and nulled.
void SurroundingPatchEditable::commitColormapEdit()
{
    const int size = editableTerrain->m_surroundingTextureSize;
    if (!m_colormapEdit)
        return;
    RECT rect = { 0, 0, size, size };
    IDirect3DSurface9* const colormapSurface = dx9rend::textureGetLevelSurface(m_colormap, 0, 0x7FFFFFFF);
    IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT colormapLocked;
    colormapSurface->LockRect(&colormapLocked, &rect, 0);
    D3DLOCKED_RECT editLocked;
    editSurface->LockRect(&editLocked, &rect, 0);
    unsigned int* const colormapBits = static_cast<unsigned int*>(colormapLocked.pBits);
    unsigned int* const editBits = static_cast<unsigned int*>(editLocked.pBits);
    if (size > 0)
    {
        for (int x = 0; x < size; ++x)
        {
            int z = 0;
            do
            {
                unsigned int* const target = colormapBits + x + z * texelPitch(colormapLocked);
                unsigned int* const painted = editBits + x + z * texelPitch(editLocked);
                float base[4];
                terrainEditUnpackColor(base, *target);
                float paint[4];
                terrainEditUnpackColor(paint, *painted);
                float result[4] = { base[0], base[1], base[2], base[3] };
                terrainEditLerpColor(result, paint, paint[3]);
                *target = terrainEditPackColor(result[0], result[1], result[2], result[3]);
                *painted = 0;
            }
            while (++z < size);
        }
    }
    colormapSurface->UnlockRect();
    colormapSurface->Release();
    editSurface->UnlockRect();
    editSurface->Release();
    if (m_colormapEdit)
    {
        dx9rend::textureManagerEvictFromCache(m_colormapEdit);
        terrainReleaseTexture(m_colormapEdit);
        m_colormapEdit = 0;
    }
}

// the lightmap dropped (owner release + evict, nulled) and reloaded under a managed-flag
// swap to 1: "<save><editor lightmap base>_s<slot>" (P+0x104 asked first), else a +0x50-sized texture
// filled white (the unlock re-reads the +0x170 slot, 0x101A46DD). TerrainEditable::rebuildTerrain's
// ring step.
// PE 0x101A4520
void SurroundingPatchEditable::reloadLightmap()
{
    TerrainEditable* const te = editableTerrain;
    if (m_lightmap)
    {
        terrainReleaseTexture(m_lightmap);
        dx9rend::textureManagerEvictFromCache(m_lightmap);
        m_lightmap = 0;
    }
    const int savedManaged = dx9rend::textureManagerSwapManaged(1);
    std::string path;
    {
        const std::string base = te->getEditorLightmapBaseName();            // P+0x104
        std::string name;
        name = formatString("%s_s%i", base.c_str(), slot);
        path = ::renddx9abi::fromHostString(te->getLevelSavePath()) + name;    // E+0x90
    }
    bool exists;
    {
        const std::string dds(path + ".dds");                                 // destroyed before the test
        exists = ::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds));
    }
    if (exists)
    {
        m_lightmap = engineTexture(dx9rend::textureManagerGetTexture(path));
    }
    else
    {
        m_lightmap = engineTexture(dx9rend::textureManagerCreateEmpty(
            static_cast<unsigned int>(te->m_surroundingTextureSize),
            static_cast<unsigned int>(te->m_surroundingTextureSize), 21, 1, 0, 0));
        fillTextureSlot(m_lightmap, 0xFFFFFFFFu);
    }
    dx9rend::textureManagerSwapManaged(savedManaged);
}

// not for slots 1 and 2: the edit copy committed, then the colormap, the lightmap and the
// low-detail map re-oriented for the (new) engine mode.
// PE 0x101A4FF0
void SurroundingPatchEditable::onModeSwitch()
{
    if (slot == 1 || slot == 2)
        return;
    commitColormapEdit();
    rotateTexture(&m_colormap, editableTerrain->m_surroundingTerrainEngineMode != 0);
    rotateTexture(&m_lightmap, editableTerrain->m_surroundingTerrainEngineMode != 0);
    rotateTexture(reinterpret_cast<dx9rend::EngineTexture**>(&lowDetailmap),
                  editableTerrain->m_surroundingTerrainEngineMode != 0);
}

// (`retn 8`) - not for slots 1 and 2, nor for a texture whose format (+0x38) is not
// A8R8G8B8: a size^2 scratch (size = the texture's height), the texture copied into it, then written
// back re-oriented - destination (x, y) for source (col, row), rr = size-1-row, rc = size-1-col:
//   engine mode: slots 3/4 (rr, col), slots 5/6 (rc, rr), else (row, rc);
//   editor mode: slots 3/4 (row, rc), slots 5/6 (rc, rr), else (rr, col).
// Both locked whole, flags 0, the scratch first; unlocked and released the scratch's first; the
// scratch then evicted before its owner reference is released.
// PE 0x101A4DE0
void SurroundingPatchEditable::rotateTexture(dx9rend::EngineTexture** texture, bool engineMode)
{
    if (slot == 1 || slot == 2)
        return;
    const int size = static_cast<int>(dx9rend::textureDims(*texture)[1]);
    if (dx9rend::textureDims(*texture)[4] != 21)
        return;
    dx9rend::EngineTexture* const scratch = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(size), static_cast<unsigned int>(size), 21, 1, 0, 0));
    IDirect3DSurface9* const scratchSurface = dx9rend::textureGetLevelSurface(scratch, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT scratchLocked;
    scratchSurface->LockRect(&scratchLocked, 0, 0);
    IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(*texture, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT locked;
    surface->LockRect(&locked, 0, 0);
    unsigned int* const scratchBits = static_cast<unsigned int*>(scratchLocked.pBits);
    unsigned int* const bits = static_cast<unsigned int*>(locked.pBits);
    if (size > 0)
    {
        for (int row = 0; row < size; ++row)
            for (int col = 0; col < size; ++col)
                scratchBits[col + row * texelPitch(scratchLocked)] = bits[col + row * texelPitch(locked)];
    }
    if (size > 0)
    {
        for (int row = 0; row < size; ++row)
        {
            const int reversedRow = size - 1 - row;
            for (int col = 0; col < size; ++col)
            {
                const int reversedCol = size - 1 - col;
                const unsigned int* const source = scratchBits + col + row * texelPitch(scratchLocked);
                int targetX;
                int targetY;
                if (engineMode)
                {
                    if (slot == 3 || slot == 4)      { targetX = reversedRow; targetY = col; }
                    else if (slot == 5 || slot == 6) { targetX = reversedCol; targetY = reversedRow; }
                    else                             { targetX = row;         targetY = reversedCol; }
                }
                else
                {
                    if (slot == 3 || slot == 4)      { targetX = row;         targetY = reversedCol; }
                    else if (slot == 5 || slot == 6) { targetX = reversedCol; targetY = reversedRow; }
                    else                             { targetX = reversedRow; targetY = col; }
                }
                bits[targetX + targetY * texelPitch(locked)] = *source;
            }
        }
    }
    scratchSurface->UnlockRect();
    scratchSurface->Release();
    surface->UnlockRect();
    surface->Release();
    dx9rend::textureManagerEvictFromCache(scratch);
    terrainReleaseTexture(scratch);
}


// Paints the ring colormap with cosine falloff; the caller supplies a quartered radius.
// Positive strength blends toward color, negative restores the original, and zero stores zero.
// Uses the supplied channel order directly, unlike the inner patch brush.
// PE 0x101A2E80
void SurroundingPatchEditable::paintColor(float x, float z, float radius, int strength, unsigned int color)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Preserve the inspected x87 loads, spills and comparisons.
    const float xr = bf2x87::store(bf2x87::sub(bf2x87::Real(x), bf2x87::Real(originX)));
    const float zr = bf2x87::store(bf2x87::sub(bf2x87::Real(z), bf2x87::Real(originZ)));
#else
    const float xr = x - static_cast<float>(originX);                         // +0x24
    const float zr = z - static_cast<float>(originZ);                         // +0x28
#endif
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    if (!bf2x87::greater(bf2x87::add(bf2x87::Real(xr), bf2x87::Real(radius)), bf2x87::Real(0)))
#else
    if (!(xr + radius > 0.0f))
#endif
        return;
    const float spanF = static_cast<float>(span);                             // +0x38
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    if (!bf2x87::less(bf2x87::sub(bf2x87::Real(xr), bf2x87::Real(radius)), bf2x87::Real(spanF)) ||
        !bf2x87::greater(bf2x87::add(bf2x87::Real(zr), bf2x87::Real(radius)), bf2x87::Real(0)) ||
        !bf2x87::less(bf2x87::sub(bf2x87::Real(zr), bf2x87::Real(radius)), bf2x87::Real(spanF)) || !m_colormapReady)
#else
    if (!(xr - radius < spanF) || !(zr + radius > 0.0f) || !(zr - radius < spanF) || !m_colormapReady)
#endif
        return;
    if (!m_colormapEdit)
        makeColormapEditCopy();
    const int size = editableTerrain->m_surroundingTextureSize;               // +0x50
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const bf2x87::Real scale = bf2x87::div(bf2x87::Real(size), bf2x87::Real(span));
    const float xs = bf2x87::store(bf2x87::mul(bf2x87::Real(xr), bf2x87::Real(scale)));
    const float zs = bf2x87::store(bf2x87::mul(bf2x87::Real(zr), bf2x87::Real(scale)));
    const float rs = bf2x87::store(bf2x87::mul(bf2x87::Real(scale), bf2x87::Real(radius)));
    const float inverse = bf2x87::store(bf2x87::div(bf2x87::Real(1), bf2x87::Real(rs)));
#else
    const float scale = static_cast<float>(size) / static_cast<float>(span);
    const float xs = xr * scale;
    const float zs = zr * scale;
    const float rs = scale * radius;
    const float inverse = 1.0f / rs;
#endif
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    int x0 = bf2x87::toIntTruncate(bf2x87::sub(bf2x87::Real(xs), bf2x87::Real(rs)));
#else
    int x0 = static_cast<int>(xs - rs);
#endif
    if (x0 <= 0)
        x0 = 0;
    int xEnd = size;
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const int top = ceilChop(bf2x87::store(bf2x87::add(bf2x87::Real(xs), bf2x87::Real(rs))));
#else
        const int top = ceilChop(xs + rs);
#endif
        if (size >= top)
            xEnd = top;
    }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    int z0 = bf2x87::toIntTruncate(bf2x87::sub(bf2x87::Real(zs), bf2x87::Real(rs)));
#else
    int z0 = static_cast<int>(zs - rs);
#endif
    if (z0 <= 0)
        z0 = 0;
    int zEnd = size;
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const int top = ceilChop(bf2x87::store(bf2x87::add(bf2x87::Real(zs), bf2x87::Real(rs))));
#else
        const int top = ceilChop(zs + rs);
#endif
        if (size >= top)
            zEnd = top;
    }
    IDirect3DSurface9* const colormapSurface = dx9rend::textureGetLevelSurface(m_colormap, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT colormapLocked;
    colormapSurface->LockRect(&colormapLocked, 0, 0);
    IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT editLocked;
    editSurface->LockRect(&editLocked, 0, 0);
    const unsigned int* const colormapBits = static_cast<const unsigned int*>(colormapLocked.pBits);
    unsigned int* const editBits = static_cast<unsigned int*>(editLocked.pBits);

    for (int px = x0; px < xEnd; ++px)
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const bf2x87::Real dx = bf2x87::sub(bf2x87::Real(px), bf2x87::Real(xs));
        const float dxSquared = bf2x87::store(bf2x87::mul(dx, dx));
#else
        const float dx = static_cast<float>(px) - xs;
        const float dxSquared = dx * dx;
#endif
        for (int pz = z0; pz < zEnd; ++pz)
        {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const bf2x87::Real dz = bf2x87::sub(bf2x87::Real(pz), bf2x87::Real(zs));
            const bf2x87::Real distance = bf2x87::squareRoot(bf2x87::add(bf2x87::mul(dz, dz), bf2x87::Real(dxSquared)));
            if (bf2x87::greater(distance, bf2x87::Real(rs)))
#else
            const float dz = static_cast<float>(pz) - zs;
            const float distance = std::sqrt(dz * dz + dxSquared);
            if (distance > rs)
#endif
                continue;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const float t = bf2x87::store(surroundingBrushWeight(distance, inverse, strength));
            const unsigned int* const original = colormapBits + px + pz * texelPitch(colormapLocked);
            unsigned int* const edit = editBits + px + pz * texelPitch(editLocked);
#else
            double falloff = std::cos(static_cast<double>(distance * inverse * 1.5707964f));   // flt_102212AC
            const unsigned int* const original = colormapBits + px + pz * texelPitch(colormapLocked);
            unsigned int* const edit = editBits + px + pz * texelPitch(editLocked);
            if (!(0.0 < falloff))
                falloff = 0.0;
            const float t = static_cast<float>(static_cast<double>(strength) * falloff) * 0.0039215689f;
#endif
            float target[4];
            terrainEditUnpackColor(target, color);
            float result[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float current[4];
            terrainEditUnpackColor(current, *edit);
            float base[4];
            terrainEditUnpackColor(base, *original);
            if (strength > 0)
            {
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, target, t);
            }
            else if (strength < 0)
            {
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, base, std::fabs(t));
            }
            *edit = terrainEditPackColor(result[0], result[1], result[2], result[3]);
        }
    }
    colormapSurface->UnlockRect();
    colormapSurface->Release();
    editSurface->UnlockRect();
    editSurface->Release();
}

// (`retn 14h`) - the ring patch's detail brush paints its LOW-detail MAP (+0x14) directly:
// the same overlap test (no +0x2FA gate, no edit copy); the brush centre chopped to a texel (xs, zs
// integral), x0 = max(0, ceil(xs - rs)), end = min(size, (int)(xs + rs)) - the opposite rounding
// to the colour brushes; the map locked over (0, 0, size, size) with flags 0. Per texel within the
// radius: t = |cos weight * strength / 255|, nothing unless t > 0; a positive strength lerps the texel
// toward the colour R/B-swapped ({c2, c1, c0, c3}: the slot's 0xFF paints 0x00FF0000), a negative
// one toward zero. Unlocked and released.
// PE 0x101A3200
void SurroundingPatchEditable::paintDetail(float x, float z, float radius, int strength, unsigned int color)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Preserve the inspected x87 loads, spills and comparisons.
    const float xr = bf2x87::store(bf2x87::sub(bf2x87::Real(x), bf2x87::Real(originX)));
    const float zr = bf2x87::store(bf2x87::sub(bf2x87::Real(z), bf2x87::Real(originZ)));
#else
    const float xr = x - static_cast<float>(originX);
    const float zr = z - static_cast<float>(originZ);
#endif
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    if (!bf2x87::greater(bf2x87::add(bf2x87::Real(xr), bf2x87::Real(radius)), bf2x87::Real(0)))
#else
    if (!(xr + radius > 0.0f))
#endif
        return;
    const float spanF = static_cast<float>(span);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    if (!bf2x87::less(bf2x87::sub(bf2x87::Real(xr), bf2x87::Real(radius)), bf2x87::Real(spanF)) ||
        !bf2x87::greater(bf2x87::add(bf2x87::Real(zr), bf2x87::Real(radius)), bf2x87::Real(0)) ||
        !bf2x87::less(bf2x87::sub(bf2x87::Real(zr), bf2x87::Real(radius)), bf2x87::Real(spanF)))
#else
    if (!(xr - radius < spanF) || !(zr + radius > 0.0f) || !(zr - radius < spanF))
#endif
        return;
    const int size = editableTerrain->m_surroundingTextureSize;               // +0x50
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const bf2x87::Real scale = bf2x87::div(bf2x87::Real(size), bf2x87::Real(spanF));
    const int xCentre = bf2x87::toIntTruncate(bf2x87::mul(bf2x87::Real(xr), scale));
    const int zCentre = bf2x87::toIntTruncate(bf2x87::mul(bf2x87::Real(zr), scale));
    const float rs = bf2x87::store(bf2x87::mul(scale, bf2x87::Real(radius)));
    const float inverse = bf2x87::store(bf2x87::div(bf2x87::Real(1), bf2x87::Real(rs)));
#else
    const float scale = static_cast<float>(size) / spanF;
    const int xCentre = static_cast<int>(xr * scale);
    const int zCentre = static_cast<int>(zr * scale);
    const float rs = scale * radius;
    const float inverse = 1.0f / rs;
#endif
    const float xs = static_cast<float>(xCentre);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    int x0 = bf2x87::toIntTruncate(bf2x87::Real(std::ceil(bf2x87::storeDouble(
        bf2x87::sub(bf2x87::Real(xCentre), bf2x87::Real(rs))))));
#else
    int x0 = static_cast<int>(std::ceil(static_cast<double>(xs - rs)));
#endif
    if (x0 <= 0)
        x0 = 0;
    int xEnd = size;
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const int top = bf2x87::toIntTruncate(bf2x87::add(bf2x87::Real(xs), bf2x87::Real(rs)));
#else
        const int top = static_cast<int>(xs + rs);
#endif
        if (size >= top)
            xEnd = top;
    }
    const float zs = static_cast<float>(zCentre);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    int z0 = bf2x87::toIntTruncate(bf2x87::Real(std::ceil(bf2x87::storeDouble(
        bf2x87::sub(bf2x87::Real(zCentre), bf2x87::Real(rs))))));
#else
    int z0 = static_cast<int>(std::ceil(static_cast<double>(zs - rs)));
#endif
    if (z0 <= 0)
        z0 = 0;
    int zEnd = size;
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const int top = bf2x87::toIntTruncate(bf2x87::add(bf2x87::Real(zs), bf2x87::Real(rs)));
#else
        const int top = static_cast<int>(zs + rs);
#endif
        if (size >= top)
            zEnd = top;
    }
    RECT rect = { 0, 0, size, size };
    IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(engineTexture(lowDetailmap), 0, 0x7FFFFFFF);
    D3DLOCKED_RECT locked;
    surface->LockRect(&locked, &rect, 0);
    unsigned int* const bits = static_cast<unsigned int*>(locked.pBits);

    for (int px = x0; px < xEnd; ++px)
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const bf2x87::Real dx = bf2x87::sub(bf2x87::Real(px), bf2x87::Real(xs));
        const float dxSquared = bf2x87::store(bf2x87::mul(dx, dx));
#else
        const float dx = static_cast<float>(px) - xs;
        const float dxSquared = dx * dx;
#endif
        for (int pz = z0; pz < zEnd; ++pz)
        {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const bf2x87::Real dz = bf2x87::sub(bf2x87::Real(pz), bf2x87::Real(zs));
            const bf2x87::Real distance = bf2x87::squareRoot(bf2x87::add(bf2x87::mul(dz, dz), bf2x87::Real(dxSquared)));
            if (bf2x87::greater(distance, bf2x87::Real(rs)))
#else
            const float dz = static_cast<float>(pz) - zs;
            const float distance = std::sqrt(dz * dz + dxSquared);
            if (distance > rs)
#endif
                continue;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const bf2x87::Real weight = bf2x87::abs(surroundingBrushWeight(distance, inverse, strength));
            const float t = bf2x87::store(weight);
            if (!bf2x87::greater(weight, bf2x87::Real(0)))
                continue;
            unsigned int* const texel = bits + px + pz * texelPitch(locked);
#else
            double falloff = std::cos(static_cast<double>(distance * inverse * 1.5707964f));
            if (!(0.0 < falloff))
                falloff = 0.0;
            const float t = std::fabs(static_cast<float>(falloff * static_cast<double>(strength)) * 0.0039215689f);
            if (!(t > 0.0f))
                continue;
            unsigned int* const texel = bits + px + pz * texelPitch(locked);
#endif
            float result[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float current[4];
            terrainEditUnpackColor(current, *texel);
            if (strength > 0)
            {
                float colour[4];
                terrainEditUnpackColor(colour, color);
                const float target[4] = { colour[2], colour[1], colour[0], colour[3] };
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, target, t);
            }
            else if (strength < 0)
            {
                const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, zero, t);
            }
            *texel = terrainEditPackColor(result[0], result[1], result[2], result[3]);
        }
    }
    surface->UnlockRect();
    surface->Release();
}

// (`retn 14h`) - the ring patch's paint-from-texture brush: paintColor's gates, edit copy
// and window; the colormap locked first, then the copy, then the layer record's colour texture
// (+0x64, all whole, flags 0). Per texel within the radius the source texel is (u, v) = floor((texel
// / scale + origin) * tiling + 0.5) wrapped - a negative v corrected by the texture's width
// (shipped, as in the patch brush). t = cos weight * strength / 255 (NOT made absolute); a positive
// strength lerps the copy toward the texture, a negative one back toward the colormap by |t|.
// Unlocked and released colormap, copy, texture.
// PE 0x101A5AB0
void SurroundingPatchEditable::paintColorFromTexture(float x, float z, float radius, int strength, int layer)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Preserve the inspected x87 loads, spills and comparisons.
    const float xr = bf2x87::store(bf2x87::sub(bf2x87::Real(x), bf2x87::Real(originX)));
    const float zr = bf2x87::store(bf2x87::sub(bf2x87::Real(z), bf2x87::Real(originZ)));
#else
    const float xr = x - static_cast<float>(originX);
    const float zr = z - static_cast<float>(originZ);
#endif
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    if (!bf2x87::greater(bf2x87::add(bf2x87::Real(xr), bf2x87::Real(radius)), bf2x87::Real(0)))
#else
    if (!(xr + radius > 0.0f))
#endif
        return;
    const float spanF = static_cast<float>(span);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    if (!bf2x87::less(bf2x87::sub(bf2x87::Real(xr), bf2x87::Real(radius)), bf2x87::Real(spanF)) ||
        !bf2x87::greater(bf2x87::add(bf2x87::Real(zr), bf2x87::Real(radius)), bf2x87::Real(0)) ||
        !bf2x87::less(bf2x87::sub(bf2x87::Real(zr), bf2x87::Real(radius)), bf2x87::Real(spanF)) || !m_colormapReady)
#else
    if (!(xr - radius < spanF) || !(zr + radius > 0.0f) || !(zr - radius < spanF) || !m_colormapReady)
#endif
        return;
    if (!m_colormapEdit)
        makeColormapEditCopy();
    const int size = editableTerrain->m_surroundingTextureSize;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const float scale = bf2x87::store(bf2x87::div(bf2x87::Real(size), bf2x87::Real(span)));
    const float xs = bf2x87::store(bf2x87::mul(bf2x87::Real(xr), bf2x87::Real(scale)));
    const float zs = bf2x87::store(bf2x87::mul(bf2x87::Real(zr), bf2x87::Real(scale)));
    const float rs = bf2x87::store(bf2x87::mul(bf2x87::Real(scale), bf2x87::Real(radius)));
    const float inverse = bf2x87::store(bf2x87::div(bf2x87::Real(1), bf2x87::Real(rs)));
#else
    const float scale = static_cast<float>(size) / static_cast<float>(span);
    const float xs = xr * scale;
    const float zs = zr * scale;
    const float rs = scale * radius;
    const float inverse = 1.0f / rs;
#endif
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    int x0 = bf2x87::toIntTruncate(bf2x87::sub(bf2x87::Real(xs), bf2x87::Real(rs)));
#else
    int x0 = static_cast<int>(xs - rs);
#endif
    if (x0 <= 0)
        x0 = 0;
    int xEnd = size;
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const int top = ceilChop(bf2x87::store(bf2x87::add(bf2x87::Real(xs), bf2x87::Real(rs))));
#else
        const int top = ceilChop(xs + rs);
#endif
        if (size >= top)
            xEnd = top;
    }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    int z0 = bf2x87::toIntTruncate(bf2x87::sub(bf2x87::Real(zs), bf2x87::Real(rs)));
#else
    int z0 = static_cast<int>(zs - rs);
#endif
    if (z0 <= 0)
        z0 = 0;
    int zEnd = size;
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const int top = ceilChop(bf2x87::store(bf2x87::add(bf2x87::Real(zs), bf2x87::Real(rs))));
#else
        const int top = ceilChop(zs + rs);
#endif
        if (size >= top)
            zEnd = top;
    }
    IDirect3DSurface9* const colormapSurface = dx9rend::textureGetLevelSurface(m_colormap, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT colormapLocked;
    colormapSurface->LockRect(&colormapLocked, 0, 0);
    IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT editLocked;
    editSurface->LockRect(&editLocked, 0, 0);
    const TerrainDetailTextureEditable* const record = static_cast<const TerrainDetailTextureEditable*>(
        editableTerrain->m_detailTextures.data()[layer]);
    const int textureHeight = static_cast<int>(dx9rend::textureDims(record->colorTexture)[1]);
    const int textureWidth = static_cast<int>(dx9rend::textureDims(record->colorTexture)[0]);
    IDirect3DSurface9* const textureSurface = dx9rend::textureGetLevelSurface(record->colorTexture, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT textureLocked;
    textureSurface->LockRect(&textureLocked, 0, 0);
    const unsigned int* const colormapBits = static_cast<const unsigned int*>(colormapLocked.pBits);
    unsigned int* const editBits = static_cast<unsigned int*>(editLocked.pBits);
    const unsigned int* const textureBits = static_cast<const unsigned int*>(textureLocked.pBits);

    for (int px = x0; px < xEnd; ++px)
    {
        const float pxF = static_cast<float>(px);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const bf2x87::Real dx = bf2x87::sub(bf2x87::Real(px), bf2x87::Real(xs));
        const float dxSquared = bf2x87::store(bf2x87::mul(dx, dx));
#else
        const float dx = pxF - xs;
        const float dxSquared = dx * dx;
#endif
        for (int pz = z0; pz < zEnd; ++pz)
        {
            const float pzF = static_cast<float>(pz);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const bf2x87::Real dz = bf2x87::sub(bf2x87::Real(pz), bf2x87::Real(zs));
            const bf2x87::Real distance = bf2x87::squareRoot(bf2x87::add(bf2x87::mul(dz, dz), bf2x87::Real(dxSquared)));
            const float storedDistance = bf2x87::store(distance);
            if (bf2x87::greater(distance, bf2x87::Real(rs)))
#else
            const float dz = pzF - zs;
            const float distance = std::sqrt(dz * dz + dxSquared);
            if (distance > rs)
#endif
                continue;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const bf2x87::Real inverseScaleRetained = bf2x87::div(bf2x87::Real(1), bf2x87::Real(scale));
            const float inverseScale = bf2x87::store(inverseScaleRetained);
            int u = bf2x87::toIntTruncate(bf2x87::Real(std::floor(bf2x87::storeDouble(bf2x87::add(
                bf2x87::mul(bf2x87::add(bf2x87::mul(inverseScaleRetained, bf2x87::Real(pxF)),
                    bf2x87::Real(originX)), bf2x87::Real(record->colorTextureTiling.x)), bf2x87::Real(0.5f))))));
#else
            const float inverseScale = 1.0f / scale;
            int u = static_cast<int>(std::floor(static_cast<double>(
                (inverseScale * pxF + static_cast<float>(originX)) * record->colorTextureTiling.x + 0.5f)));
#endif
            u = u % textureWidth;
            if (u < 0)
                u += textureWidth;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            int v = bf2x87::toIntTruncate(bf2x87::Real(std::floor(bf2x87::storeDouble(bf2x87::add(
                bf2x87::mul(bf2x87::add(bf2x87::mul(bf2x87::Real(inverseScale), bf2x87::Real(pzF)),
                    bf2x87::Real(originZ)), bf2x87::Real(record->colorTextureTiling.y)), bf2x87::Real(0.5f))))));
#else
            int v = static_cast<int>(std::floor(static_cast<double>(
                (inverseScale * pzF + static_cast<float>(originZ)) * record->colorTextureTiling.y + 0.5f)));
#endif
            v = v % textureHeight;
            if (v < 0)
                v += textureWidth;                                            // shipped defect: adds width.
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const float t = bf2x87::store(surroundingBrushWeight(bf2x87::Real(storedDistance), inverse, strength));
            const unsigned int* const source = textureBits + u + v * texelPitch(textureLocked);
            const unsigned int* const original = colormapBits + px + pz * texelPitch(colormapLocked);
            unsigned int* const edit = editBits + px + pz * texelPitch(editLocked);
#else
            double falloff = std::cos(static_cast<double>(distance * inverse * 1.5707964f));
            const unsigned int* const source = textureBits + u + v * texelPitch(textureLocked);
            const unsigned int* const original = colormapBits + px + pz * texelPitch(colormapLocked);
            unsigned int* const edit = editBits + px + pz * texelPitch(editLocked);
            if (!(0.0 < falloff))
                falloff = 0.0;
            const float t = static_cast<float>(static_cast<double>(strength) * falloff) * 0.0039215689f;
#endif
            float paint[4];
            terrainEditUnpackColor(paint, *source);
            float result[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float current[4];
            terrainEditUnpackColor(current, *edit);
            float base[4];
            terrainEditUnpackColor(base, *original);
            if (strength > 0)
            {
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, paint, t);
            }
            else if (strength < 0)
            {
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, base, std::fabs(t));
            }
            *edit = terrainEditPackColor(result[0], result[1], result[2], result[3]);
        }
    }
    colormapSurface->UnlockRect();
    colormapSurface->Release();
    editSurface->UnlockRect();
    editSurface->Release();
    textureSurface->UnlockRect();
    textureSurface->Release();
}


// R3 - saving, every body read in RendDX9.dll 1.50 ("RendDX9.dll (2).i64"). The suffix is
// "_s<slot>"; paths are the terrain's getLevelSavePath() + a level-relative name.


// the low-detail map (+0x14) saved as "<save><editor low-detail base>_s<slot>" (the
// suffix formatted first, then P+0x10C, then the save path).
// PE 0x101A2A90
void SurroundingPatchEditable::saveLowDetailMap()
{
    std::string suffix;
    suffix = formatString("_s%i", slot);
    std::string name;
    {
        const std::string editorBase = editableTerrain->getEditorLowDetailmapBaseName(); // P+0x10C
        name = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) + editorBase + suffix; // E+0x90
    }
    dx9rend::textureManagerSaveTexture(engineTexture(lowDetailmap), name);
}

// the game low-detail map compiled from the in-memory one: "<save><low-detail base
// (E+0x84)>_s<slot>" saved uncompressed (sub_10017F90), nvdxt (E+0xF4) into "<save><low-detail
// directory>" (0x101A6260) with the surrounding low-detail settings (+0x838), the uncompressed input
// deleted, "_.dds" moved onto ".dds" and deleted. Nothing is deleted first.
// PE 0x101A2B90
void SurroundingPatchEditable::compileLowDetailMap()
{
    std::string suffix;
    suffix = formatString("_s%i", slot);
    std::string name;
    {
        TerrainEditableNameResult lowBase = editableTerrain->getLowDetailmapBaseName(); // E+0x84
        name = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) +
            ::renddx9abi::fromHostString(lowBase) + suffix; // E+0x90
    }
    std::string outputDirectory;
    {
        const std::string directory = editableTerrain->getLowDetailmapDirectory();
        outputDirectory = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) + directory;
    }
    dx9rend::textureManagerSaveTexture(engineTexture(lowDetailmap), name);
    {
        // The argument blocks are built right to left: the options, the directory, the file.
        const TerrainEditableString options(editableTerrain->m_surroundingLowDetailmapSettings); // +0x838
        const std::string directory(outputDirectory);
        std::string file(name);
        file += ".dds";
        editableTerrain->runNvdxt(::renddx9abi::toHostString(file), ::renddx9abi::toHostString(directory), options); // E+0xF4
    }
    {
        std::string input(name);
        input += ".dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(input));
    }
    {
        std::string target(name);
        target += ".dds";
        std::string source(name);
        source += "_.dds";
        ::dice::hfe::fileManager->moveFile(::renddx9abi::makeHostString(source),
                                           ::renddx9abi::makeHostString(target));
    }
    {
        std::string leftover(name);
        leftover += "_.dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(leftover));
    }
}

// only once the colormap exists (+0x2FA): the edit copy committed by alpha (0x101A4370),
// then the colormap (+0x16C) saved as "<save><editor colormap base>_s<slot>" (P+0xFC asked first).
// PE 0x101A4CC0
void SurroundingPatchEditable::saveEditedColormap()
{
    if (!m_colormapReady)
        return;
    commitColormapEdit();
    std::string suffix;
    suffix = formatString("_s%i", slot);
    std::string name;
    {
        const std::string editorBase = editableTerrain->getEditorColormapBaseName(); // P+0xFC
        name = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) + editorBase + suffix;
    }
    dx9rend::textureManagerSaveTexture(m_colormap, name);
}

// Burns and compiles the surrounding-patch colormap, restoring draw state afterward.
// Ready patches use the terrain colormap path; otherwise base selects the destination.
// [CHOSEN, not recovered] Unused render flags are zero-initialized.
// PE 0x101A5060
void SurroundingPatchEditable::burnColormap(const std::string& base)
{
    std::string suffix;
    suffix = formatString("_s%i", slot);
    const int size = editableTerrain->m_surroundingTextureSize;              // +0x50
    if (!m_colormapReady)
    {
        std::string target(::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) + base + suffix);
        bool missing;
        {
            std::string dds(target);
            dds += ".dds";
            missing = !::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds)); // +0x1C
        }
        if (missing)
            dx9rend::textureManagerSaveTexture(m_colormap, target);
        return;
    }

    dx9rend::stateManagerPush();
    const unsigned char saved1693 = dx9rend::rendererRenderSwitch(1693);
    const unsigned char saved1713 = dx9rend::rendererRenderSwitch(1713);
    const unsigned char saved1705 = dx9rend::rendererRenderSwitch(1705);
    const unsigned char saved1697 = dx9rend::rendererRenderSwitch(1697);
    const unsigned char saved1717 = dx9rend::rendererRenderSwitch(1717);
    const unsigned char saved1702 = dx9rend::rendererRenderSwitch(1702);
    const unsigned char saved1703 = dx9rend::rendererRenderSwitch(1703);
    const unsigned char saved1698 = dx9rend::rendererRenderSwitch(1698);
    const unsigned char saved1714 = dx9rend::rendererRenderSwitch(1714);
    const unsigned char saved1700 = dx9rend::rendererRenderSwitch(1700);
    const float savedSeaLevel = editableTerrain->m_primaryHeightmap->getSeaWaterLevel(); // +0x64
    const int savedRenderMode = dx9rend::rendererGetHemiRenderMode();
    const bool savedShadows = g_LightManager->getDynamicShadowsEnabled();     // +0xF0
    const int savedTechnique = editableTerrain->getShaderTechnique();        // E+0x8C
    const unsigned char savedPass = m_burnPass;                               // +0x2FB

    dx9rend::rendererRenderSwitch(1693) = 1;
    dx9rend::rendererRenderSwitch(1713) = 0;
    dx9rend::rendererRenderSwitch(1705) = 0;
    dx9rend::rendererRenderSwitch(1697) = 0;
    bundledMeshSetHemiEnabled(0);
    dx9rend::rendererRenderSwitch(1717) = 1;
    dx9rend::rendererRenderSwitch(1702) = 0;
    dx9rend::rendererRenderSwitch(1703) = 0;
    dx9rend::rendererRenderSwitch(1698) = 0;
    staticMeshRendererSetHemiEnabled(0);
    dx9rend::rendererRenderSwitch(1714) = 0;
    dx9rend::rendererRenderSwitch(1700) = 1;
    roadCompiledRendererSetHemiEnabled(1);
    editableTerrain->m_primaryHeightmap->setSeaWaterLevel(-110.0f);
    dx9rend::rendererSetHemiRenderMode(0);
    g_LightManager->setDynamicShadowsEnabled(false);                          // +0xEC
    editableTerrain->setShaderTechnique(0);                                  // E+0x88
    m_burnPass = 1;
    dx9rend::RenderFlags flags;
    std::memset(&flags, 0, sizeof(flags));
    flags.field01 = 1;                                                        // the terrain arm
    const int savedManaged = dx9rend::textureManagerSwapManaged(1);

    IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice());
    IDirect3DSurface9* renderTarget = 0;
    IDirect3DSurface9* depthStencil = 0;
    device->CreateRenderTarget(static_cast<UINT>(size), static_cast<UINT>(size), D3DFMT_A8R8G8B8,
                               D3DMULTISAMPLE_NONE, 0, FALSE, &renderTarget, 0);              // +0x70
    static_cast<IDirect3DDevice9*>(dx9rend::renderDevice())->CreateDepthStencilSurface(
                                      static_cast<UINT>(size), static_cast<UINT>(size), D3DFMT_D24S8,
                                      D3DMULTISAMPLE_NONE, 0, FALSE, &depthStencil, 0);       // +0x74
    const float extent = static_cast<float>(span);                            // +0x38, fild
    Vec3 position;
    position.x = center.x;                                                    // +0x4C
    position.y = 500.0f;
    position.z = center.z;                                                    // +0x54
    dx9rend::rendererRenderOrthoTopDown(renderTarget, depthStencil, position, extent, flags);

    dx9rend::rendererRenderSwitch(1693) = saved1693;
    dx9rend::rendererRenderSwitch(1713) = saved1713;
    dx9rend::rendererRenderSwitch(1705) = saved1705;
    dx9rend::rendererRenderSwitch(1697) = saved1697;
    bundledMeshSetHemiEnabled(saved1697);
    dx9rend::rendererRenderSwitch(1717) = saved1717;
    dx9rend::rendererRenderSwitch(1702) = saved1702;
    dx9rend::rendererRenderSwitch(1703) = saved1703;
    dx9rend::rendererRenderSwitch(1698) = saved1698;
    staticMeshRendererSetHemiEnabled(saved1698);
    dx9rend::rendererRenderSwitch(1714) = saved1714;
    dx9rend::rendererRenderSwitch(1700) = saved1700;
    roadCompiledRendererSetHemiEnabled(saved1700);
    editableTerrain->m_primaryHeightmap->setSeaWaterLevel(savedSeaLevel);
    dx9rend::rendererSetHemiRenderMode(savedRenderMode);
    g_LightManager->setDynamicShadowsEnabled(savedShadows);
    editableTerrain->setShaderTechnique(savedTechnique);
    m_burnPass = savedPass;

    std::string path;
    {
        std::string pathSuffix;
        pathSuffix = formatString("_s%i", slot);                               // formatted again
        TerrainEditableNameResult colormapBase = editableTerrain->getColormapBaseName(); // E+0x6C
        path = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) +
            ::renddx9abi::fromHostString(colormapBase) + pathSuffix; // E+0x90
    }
    dx9rend::EngineTexture* const big = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(size), static_cast<unsigned int>(size), 21, 0, 0, 0));   // levels 0: the full chain
    IDirect3DSurface9* const bigSurface = dx9rend::textureGetLevelSurface(big, 0, 0x7FFFFFFF);
    D3DXLoadSurfaceFromSurface(bigSurface, 0, 0, renderTarget, 0, 0, 0xFFFFFFFFu, 0);
    dx9rend::EngineTexture* flipped = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(size), static_cast<unsigned int>(size), 21, 0, 0, 0));
    IDirect3DSurface9* const flippedSurface = dx9rend::textureGetLevelSurface(flipped, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT bigLocked;
    bigSurface->LockRect(&bigLocked, 0, 0);
    D3DLOCKED_RECT flippedLocked;
    flippedSurface->LockRect(&flippedLocked, 0, 0);
    const unsigned int* const bigBits = static_cast<const unsigned int*>(bigLocked.pBits);
    unsigned int* const flippedBits = static_cast<unsigned int*>(flippedLocked.pBits);
    for (int x = 0; x < size; ++x)
    {
        int row = size - 1;
        for (int z = 0; z < size; ++z, --row)
            flippedBits[x + z * texelPitch(flippedLocked)] = bigBits[x + row * texelPitch(bigLocked)] & 0x00FFFFFFu;
    }
    flippedSurface->UnlockRect();
    bigSurface->UnlockRect();
    rotateTexture(&flipped, true);
    {
        std::string name(path);
        name += "_u8888";
        dx9rend::textureManagerSaveTexture(flipped, name);
    }
    {
        // The argument blocks are built right to left: the options, the directory, the file.
        const TerrainEditableString options(editableTerrain->m_surroundingColormapSettings); // +0x81C
        std::string outputDirectory;
        {
            const std::string directory = editableTerrain->getColormapDirectory(); // P+0xEC
            outputDirectory = ::renddx9abi::fromHostString(editableTerrain->getLevelSavePath()) + directory;
        }
        std::string file(path);
        file += "_u8888.dds";
        editableTerrain->runNvdxt(::renddx9abi::toHostString(file), ::renddx9abi::toHostString(outputDirectory), options); // E+0xF4
    }
    {
        std::string target(path);
        target += ".dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(target));
    }
    {
        std::string target(path);
        target += ".dds";
        std::string source(path);
        source += "_u8888_.dds";
        ::dice::hfe::fileManager->moveFile(::renddx9abi::makeHostString(source),
                                           ::renddx9abi::makeHostString(target));   // +0x24
    }
    {
        std::string input(path);
        input += "_u8888.dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(input));
    }
    {
        std::string leftover(path);
        leftover += "_u8888_.dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(leftover));
    }
    flippedSurface->Release();
    bigSurface->Release();
    terrainReleaseTexture(big);
    dx9rend::textureManagerEvictFromCache(big);
    terrainReleaseTexture(flipped);
    dx9rend::textureManagerEvictFromCache(flipped);
    dx9rend::textureManagerSwapManaged(savedManaged);
    renderTarget->Release();
    depthStencil->Release();
    dx9rend::stateManagerPop();
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
