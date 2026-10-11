// Path: attested by RendDX9.dll __FILE__ at 0x10219E48 (lines 772 and 3313).
// Terrain creation, loading, water setup and teardown, recovered from RendDX9.dll 1.50.
// Per-frame drawing lives in TerrainDraw.cpp. Memory belongs to the engine heap.
// [FRONTIER] legacy load helpers remain: pre-0x1A sub_100D87B0 and pre-0x18 sub_100E68B0.
#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "Terrain.h"
#include "QuadPatch.h"
#include "XpLightManager.h"
#include "../Core/EventManager.h"
#include "../Physics/HeightmapCluster.h"
#include "../Physics/Heightmap.h"
#include "../Game/Main/IVideoOptions.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendDX9/Ra/RaShader.h"
#include "../RendDX9/Ra/RaStreamDataEditable.h"
#include "../IO/Tokenizer.h"

#include <new>
#include <cstdlib>
#include <cstring>

namespace dice {
namespace hfe {

namespace dx9rend {

// The globals this file reaches, declared the way the neighbouring Geom TUs declare them.
extern void* g_shaderManager;      // ShaderManager.cpp  - dword_1023B97C
extern void* g_rendererBase;       // RendDX9Init.cpp    - dword_1023B970 (the Renderer object)
extern void* g_renderQueues;       // RenderQueues.cpp   - dword_1023E408, also the Ra shader table
extern void* g_raShaderConstants;  // RenderQueues.cpp   - dword_1023DDBC (the Ra constants block; releaseWater reads its WaterMap holder)

// BufferManager.cpp - the same seams SwiffRender.cpp and MeshTemplate.cpp declare.
int   bufferManagerSwapPool(int value);
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);          // sub_10011FF0 on the global
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferRelease(void* wrapper);                                             // --ref, sub_10014510, free
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  indexBufferRelease(void* wrapper);                                              // --ref, sub_10014300, free

// BF2Render.cpp - Renderer +0xAC (0x10032740), the byte at +0x764.
bool  rendererIsExpansionPack();

} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;                                                  // ModuleGeom.cpp, dword_1024B914

namespace {

const unsigned int kTerrainClassId = 821004u;            // 0x000C870C  dice.hfe.geom.Terrain
const unsigned int kTerrainIid     = 820004u;

// The eight D3DVERTEXELEMENT9 tables init() and load() hand to the buffer manager, decoded from
// the image (unk_10219A30, word_10219A58 / A80 / AB0 / AD0 / AF8 / B20 / B38). Type 4 = D3DCOLOR,
// 0 = FLOAT1, 1 = FLOAT2, 2 = FLOAT3, 3 = FLOAT4, 6 = SHORT2, 15 = FLOAT16_2; usage 0 = position,
// 3 = normal, 5 = texcoord.
const D3DVERTEXELEMENT9 kDeclPatchCompact[] =        // unk_10219A30 - base grid + the 20-byte patch stream
{ {0,0,4,0,0,0}, {1,0,0,0,0,1}, {1,4,2,0,0,2}, {1,16,4,0,3,0}, D3DDECL_END() };
const D3DVERTEXELEMENT9 kDeclPatchSingle[] =         // word_10219A58 - the 24-byte single-stream patch
{ {0,0,4,0,0,0}, {0,4,0,0,0,1}, {0,8,2,0,0,2}, {0,20,4,0,3,0}, D3DDECL_END() };
const D3DVERTEXELEMENT9 kDeclSurroundingFull[] =     // word_10219A80 - +0x2A8
{ {0,0,6,0,0,0}, {0,4,1,0,5,0}, {1,0,0,0,0,1}, {2,0,3,0,0,2}, {3,0,2,0,3,0}, D3DDECL_END() };
const D3DVERTEXELEMENT9 kDeclLowStyle[] =            // word_10219AB0 - +0x2A4, low render style
{ {0,0,4,0,0,0}, {1,0,0,0,0,1}, {2,0,3,0,0,2}, D3DDECL_END() };
const D3DVERTEXELEMENT9 kDeclSurroundingHalf[] =     // word_10219AD0 - +0x2A4, FLOAT16 texcoords
{ {0,0,6,0,0,0}, {0,4,15,0,5,0}, {1,0,0,0,0,1}, {2,0,3,0,0,2}, D3DDECL_END() };
const D3DVERTEXELEMENT9 kDeclSurroundingFloat[] =    // word_10219AF8 - +0x2A4, float texcoords
{ {0,0,6,0,0,0}, {0,4,1,0,5,0}, {1,0,0,0,0,1}, {2,0,3,0,0,2}, D3DDECL_END() };
const D3DVERTEXELEMENT9 kDeclLowStyleGrid[] =        // word_10219B20 - +0x3B0, low render style
{ {0,0,4,0,0,0}, {1,0,0,0,0,1}, D3DDECL_END() };
const D3DVERTEXELEMENT9 kDeclSurroundingGrid[] =     // word_10219B38 - +0x3B0
{ {0,0,6,0,0,0}, {1,0,0,0,0,1}, D3DDECL_END() };

// The four shadow-array handles init() publishes as globals (dword_10258F60..dword_10258F6C).
void* g_terrainShadowViewProjArray = 0;
void* g_terrainShadowPosArray      = 0;
void* g_terrainShadowSizeArray     = 0;
void* g_terrainShadowCount         = 0;

// Latches applied water quality across map changes. Only updateWaterConstants changes it;
// setupWater may therefore reuse cached quality on a later map.
unsigned char g_waterQualityLatched = 0;

// dword_102592A0 - tested by setupWater before it pushes the renderer's +0x7B4 texture into the
// material's DiffuseMap holder. nothing in the image writes it (its only reference is that read),
// so the arm is dead in the shipped DLL too; it is transcribed behind the same test. [UNRESOLVED]
const int g_terrainWaterUsesRendererTexture = 0;

// The bit patterns the constructor stores. They are not round decimals, so they are kept as bits.
inline float bits(unsigned int u)
{
    float f;
    ::memcpy(&f, &u, sizeof(f));
    return f;
}

// sub_10031180 - the refcounted assignment every Ra texture holder uses: drop the old texture
// (destroying it on the last reference), take the new one and add a reference to it.
void holderSetTexture(void* holder, dx9rend::EngineTexture* texture)
{
    dx9rend::EngineTexture** const field = reinterpret_cast<dx9rend::EngineTexture**>(
        static_cast<unsigned char*>(holder) + 4);
    dx9rend::EngineTexture* const old = *field;
    if (old != texture)
    {
        if (old && !--old->refCount)
        {
            dx9rend::textureDtor(old);
            ::renddx9abi::engineFree(old);
        }
        *field = texture;
        if (texture)
            dx9rend::textureAddRef(texture);
    }
}

inline dx9rend::Ra::RaShaderTable* raShaderTable()
{
    return static_cast<dx9rend::Ra::RaShaderTable*>(dx9rend::g_renderQueues);
}

inline dx9rend::Ra::RaShader* raShaderNamed(const char* name)
{
    return dx9rend::Ra::getShader(raShaderTable(), vc71::string(name), false);
}

inline ::dice::hfe::world::IHeightmapCluster* heightmapCluster()
{
    IBase* const found = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("HeightmapCluster"));
    return static_cast< ::dice::hfe::world::IHeightmapCluster*>(static_cast<void*>(found));
}

// sub_100E11B0's guarded file static (dword_1025926C..74, guard bit 0 of dword_10259278).
unsigned int g_originScratchGuard = 0;
Vec3         g_originScratch;

} // anonymous namespace

unsigned char& terrainWaterQualityLatched()
{
    return g_waterQualityLatched;
}

// sub_100E11B0 - copies its argument into a file static and returns the static's address. The
// QuadPatch and SurroundingPatch loaders seed their centres through it.
const Vec3* terrainOriginScratch(const Vec3& in)
{
    if ((g_originScratchGuard & 1u) == 0)
    {
        g_originScratchGuard |= 1u;
        g_originScratch.x = 0.0f;
        g_originScratch.y = 0.0f;
        g_originScratch.z = 0.0f;
    }
    g_originScratch.x = in.x;
    g_originScratch.y = in.y;
    g_originScratch.z = in.z;
    return &g_originScratch;
}


// TerrainRawReader - the sub_10075DE0..sub_100763A0 family over the host stream.

void TerrainRawReader::open(const vc71::string& path, int mode, int access, int /*unusedFourth*/)
{
    // sub_100763A0 (`retn 10h`). The fourth argument is popped and never read: FileManager +0x18
    // receives a literal 0 (0x100763A8) whatever the caller passed.
    m_stream = 0;
    ::renddx9abi::HostStream* const opened = ::dice::hfe::fileManager->open(
        ::renddx9abi::makeHostString(path),
        static_cast< ::renddx9abi::HostFileMode>(mode),
        static_cast< ::renddx9abi::HostFileAccess>(access), 0);
    if (m_stream)
    {
        m_stream->release();
        m_stream = 0;
    }
    m_stream = opened;
}

void TerrainRawReader::close()
{
    // sub_10075E00. (The destructor, sub_10075DE0, releases again through the already-nulled
    // slot and so does nothing.)
    if (m_stream)
    {
        m_stream->release();
        m_stream = 0;
    }
}

unsigned int TerrainRawReader::read(void* dst, unsigned int n)
{
    return m_stream->read(dst, n);                                    // +0x10, unguarded as shipped
}

bool TerrainRawReader::readLine(vc71::string& out, char delimiter)
{
    // sub_10076000: one byte at a time into a 0x1000-byte stack buffer until the delimiter or a
    // short read; an empty line answers false without touching `out`.
    char buffer[0x1000];
    unsigned int n = 0;
    for (;;)
    {
        if (m_stream->read(buffer + n, 1) != 1)
            break;
        if (buffer[n] == delimiter)
            break;
        ++n;
    }
    buffer[n] = 0;
    if (n == 0)
        return false;
    out = buffer;
    return true;
}

int TerrainRawReader::tell()
{
    return static_cast< ::renddx9abi::HostNonlinearStream*>(m_stream)->getPosition();   // +0x34
}

void TerrainRawReader::seek(int origin, int offset)
{
    static_cast< ::renddx9abi::HostNonlinearStream*>(m_stream)->seek(
        static_cast< ::renddx9abi::HostSeekOrigin>(origin), offset);                     // +0x30
}


// TerrainDetailTexture - the 0x2C-byte record and its 23 table slots (Terrain.h has the table).

TerrainDetailTexture::TerrainDetailTexture()
{
    // (53 B), and the same stores inlined at the loader's two allocation sites
    // (0x100DD2A3.., 0x100DD38F..): four null textures, the two bytes, then 32.0f / 2.0f / 2.0f /
    // 0 / 0. +0x18 is 0x42000000 = 32.0f in all three initialisers
    // (0x100D8C61, 0x100DD2BE, 0x100DD39D); the tree stored 24.0f (0x41C00000). Every record the
    // loader builds has the word overwritten from the file before anything reads it, so a loaded
    // level never saw the difference; TerrainEditable's records (R1) keep the default.
    texture = 0;
    normalTexture = 0;
    sideTexture = 0;
    sideNormalTexture = 0;
    materialId = 0;
    planeMap = 0;
    topTiling = 32.0f;
    sideTiling.x = 2.0f;
    sideTiling.y = 2.0f;
    yOffset = 0.0f;                                                          // an integer 0 store
    envMap = 0;
}

TerrainDetailTexture::~TerrainDetailTexture()
{
    // sub_100D8C80: each of the four textures is released (destroyed on the last reference) and
    // then handed - the same pointer value, released or not - to the cache eviction.
    void* const slots[4] = { texture, normalTexture, sideTexture, sideNormalTexture };
    for (int i = 0; i < 4; ++i)
    {
        dx9rend::EngineTexture* const t = static_cast<dx9rend::EngineTexture*>(slots[i]);
        if (t && !--t->refCount)
        {
            dx9rend::textureDtor(t);
            ::renddx9abi::engineFree(t);
        }
        dx9rend::textureManagerEvictFromCache(t);
    }
}

// The 22 accessor slots, The five "TBM" bodies each build the debug-stream
// line sub_10074300(6, "...\geom\Terrain.h", line, "") << "TBM: ONLY FOR TERRAINEDITABLE!" and
// flush it (sub_10074690) before they build their answer - kept as one log line, as every recovered
// diagnostic in this image is. They are the shipped bodies of the plain record, not stubs of this
// tree: only the editable record (0x1021B780, package R1) stores the fields they refuse.
void TerrainDetailTexture::setName(vc71::string) {}                                      // +0x04 (`ret 1Ch`)

vc71::string TerrainDetailTexture::getName()                                             // +0x08
{
    ::renddx9abi::logf("[Terrain.h:144] TBM: ONLY FOR TERRAINEDITABLE!");
    return vc71::string("");
}

void TerrainDetailTexture::setColorTextureName(vc71::string) {}                          // +0x0C 0x100E1200 again

vc71::string TerrainDetailTexture::getColorTextureName()                                 // +0x10
{
    ::renddx9abi::logf("[Terrain.h:147] TBM: ONLY FOR TERRAINEDITABLE!");
    return vc71::string("");
}

void TerrainDetailTexture::setEditableInt68(int) {}                                      // +0x14 0x100E13B0 nullsub (`ret 4`)

int TerrainDetailTexture::getEditableInt68()                                             // +0x18
{
    ::renddx9abi::logf("[Terrain.h:150] TBM: ONLY FOR TERRAINEDITABLE!");
    return 0;                                                                // `xor eax, eax`
}

void TerrainDetailTexture::setMaterialId(unsigned char id) { materialId = id; }          // +0x1C
unsigned char TerrainDetailTexture::getMaterialId() { return materialId; }               // +0x20 (ICF)
void TerrainDetailTexture::setPlaneMap(unsigned char value) { planeMap = value; }        // +0x24
unsigned char TerrainDetailTexture::getPlaneMap() { return planeMap; }                   // +0x28
void TerrainDetailTexture::setTopTiling(float tiling) { topTiling = tiling; }            // +0x2C
float TerrainDetailTexture::getTopTiling() { return topTiling; }                         // +0x30 (ICF)
void TerrainDetailTexture::setSideTiling(Vec2 tiling) { sideTiling = tiling; }           // +0x34 (`ret 8`)
Vec2 TerrainDetailTexture::getSideTiling() { return sideTiling; }                        // +0x38 (hidden pointer)
void TerrainDetailTexture::setYOffset(float offset) { yOffset = offset; }                // +0x3C (ICF)
float TerrainDetailTexture::getYOffset() { return yOffset; }                             // +0x40 (ICF)

void TerrainDetailTexture::setColorTextureTiling(Vec2) {}                                // +0x44 0x100E1480 nullsub (`ret 8`)

Vec2 TerrainDetailTexture::getColorTextureTiling()                                       // +0x48
{
    ::renddx9abi::logf("[Terrain.h:171] TBM: ONLY FOR TERRAINEDITABLE!");
    Vec2 zero;                                                               // both words stored 0
    zero.x = 0.0f;
    zero.y = 0.0f;
    return zero;
}

void TerrainDetailTexture::setLowDetailType(int) {}                                      // +0x4C 0x100E13B0 nullsub (`ret 4`)

int TerrainDetailTexture::getLowDetailType()                                             // +0x50
{
    ::renddx9abi::logf("[Terrain.h:153] TBM: ONLY FOR TERRAINEDITABLE!");
    return 0;                                                                // `xor eax, eax`
}

// +0x54 / +0x58, the byte at +0x28, moved as it is (`mov [ecx+28h], al`
// / `mov al, [ecx+28h]`). The interface types are bool (the 2006 getLIInfo prints the getter through
// operator<<(bool)); the field stays the file's raw byte, so this getter differs from the shipped
// raw move only for a stored byte other than 0 or 1 (it answers 1). No 1.50 body calls slot +0x58:
// the draw and the TerrainEditable writer read the field.
void TerrainDetailTexture::setEnvMap(bool value) { envMap = value; }
bool TerrainDetailTexture::getEnvMap() { return envMap != 0; }


// Construction - 0x100DF160, 0x798 B. Every store in the shipped order.

Terrain::Terrain()
{
    m_refCount = 1;
    m_primaryWorldScale.y = bits(0x3B23D70Au);                                    // 0.0025
    m_primaryWorldScale.x = 2.0f;
    m_primaryWorldScale.z = 2.0f;
    m_secondaryWorldScale.y = 1.0f;
    m_secondaryWorldScale.x = 8.0f;
    m_secondaryWorldScale.z = 8.0f;
    m_maxHeight = 0.0f;                                                           // integer 0 stores
    m_minHeight = 0.0f;
    m_patchSize = 128;
    m_subdividePatches = 1;
    m_patchCount = 0;
    m_patchDetailmapSize = 512;
    m_patchColormapSize = 512;
    m_lowDetailmapSize = 512;
    m_surroundingTextureSize = 512;
    // the four base-name strings default-construct (+0x54, +0x70, +0x8C, +0xA8)
    m_terrainWaterColor.x = 0.0f; m_terrainWaterColor.y = 0.0f; m_terrainWaterColor.z = 0.0f;
    m_baseGridVertexBuffer = 0;
    m_legacyBaseGridTable = 0;
    // The handles the constructor clears - an explicit list, not a range: +0x224, +0x26C, +0x270,
    // +0x27C..+0x288 and +0x290..+0x2A0 are skipped (init() writes most of them anyway).
    m_effect = 0;
    m_hScaleBaseUV = 0; m_hScaleTransXZ = 0; m_hScaleTransY = 0; m_hStScaleTransY = 0;
    m_hTexScale = 0; m_hYPlaneTexScaleAndFarTile = 0; m_hStTexScale = 0;
    m_hMatViewProj = 0; m_hMatView = 0;
    for (int i = 0; i < 7; ++i) m_hTexLayer[i] = 0;
    m_hShadowMap = 0; m_hMultipleShadowMap = 0; m_hComponentSelector = 0;
    m_hTexProjOffset = 0; m_hTexProjScale = 0; m_hCameraPos = 0;
    m_hSunColor = 0; m_hGIColor = 0; m_hAmbientColor = 0;
    m_hSinglePointColor1X = 0; m_hDynamicPointColor = 0; m_hFogRange = 0;
    m_hFogColor = 0; m_hAmbientIlluminationRange = 0; m_hAmbientIlluminationColor = 0;
    m_hWaterFogRange = 0; m_hWaterFogColor = 0;
    m_hMorphDeltaSelector = 0; m_hMorphDeltaAdder = 0; m_hNearFarMorphLimits = 0;
    m_hFarTexTiling = 0; m_hStFarTexTiling = 0; m_hNearTexTiling = 0;
    m_hLightViewProj = 0; m_hLightViewProjOrtho = 0; m_hViewportMap = 0; m_hStTransXZ = 0;
    m_hPointLight = 0; m_hSpotLight = 0;
    m_hLightPos1 = 0; m_hLightAttSqrInv1 = 0;
    m_hColorLightTex = 0;
    m_surroundingDeclaration = 0;
    m_surroundingFullDeclaration = 0;
    m_flag2AC = 1;
    m_flag2AD = 1;
    // sub_100E1760 x3: each map gets its 0x18-byte head node, marked nil and self-linked, size 0.
    m_map2B0.construct(0x18);
    m_map2BC.construct(0x18);
    m_map2C8.construct(0x18);
    m_lowDetailTexture = 0;
    m_heightmapCluster = 0;
    m_cameraHeightAboveTerrain = 0.0f;
    m_sunColor.x = 1.0f; m_sunColor.y = 1.0f; m_sunColor.z = 1.0f;
    // Default GI color: 0.4, 0.35, 0.3. Loading replaces these values.
    m_giColor.x = bits(0x3ECCCCCDu); m_giColor.y = bits(0x3EB33333u); m_giColor.z = bits(0x3E99999Au);
    m_ambientColor.x = 0.0f; m_ambientColor.y = 0.0f; m_ambientColor.z = 0.0f;
    for (int i = 0; i < 6; ++i) m_unresolved308[i] = 0;
    m_farSideTiling.x = 0.0f; m_farSideTiling.y = 0.0f;
    m_detailTextures.first = 0; m_detailTextures.last = 0; m_detailTextures.end = 0;
    m_currentPass = 0;
    m_primaryHeightmap = 0;
    m_heightmapSizeX = 0;
    m_heightmapSizeZ = 0;
    m_drawMain = 1;
    m_flag359 = 1;
    m_flag35A = 1;
    m_flag35B = 0;
    m_surroundingBuilt = 0;
    m_flag35D = 0;
    m_lowQualityB = 0;
    m_lowQuality = 0;
    m_drawDebugFlags = 0;
    m_flag364 = 0;
    m_indexBuffer = 0;
    m_shadowRanges.first = 0; m_shadowRanges.last = 0; m_shadowRanges.end = 0;
    m_patches = 0;
    m_renderStyle = 2;
    m_halfFloatSurrounding = 0;
    m_gridDeclaration = 0;
    m_vertexDeclaration = 0;
    m_flag3B8 = 0;
    m_culler = 0;
    m_mergeDrawRanges = 1;
    for (int i = 0; i < 16; ++i) m_viewProj[i] = 0.0f;
    m_viewProj[15] = 1.0f; m_viewProj[10] = 1.0f; m_viewProj[5] = 1.0f; m_viewProj[0] = 1.0f;
    for (int i = 0; i < 4; ++i) m_cameraPosition[i] = 0.0f;
    m_deviceResetPending = 0;
    m_lightingDirty = 1;
    m_currentDetailLayer = -1;
    m_currentLod = -1;
    for (int i = 0; i < 6; ++i) m_morphLimits[i] = 0.0f;
    m_unresolved440 = -1.0f;
    m_shadowPatches.first = 0; m_shadowPatches.last = 0; m_shadowPatches.end = 0;
    m_indexBufferCount = 49152u;
    m_shadowGatherThreshold = 4.0f;
    m_shadowExtrusionScale = 4.0f;
    m_unresolved464 = 512;
    m_unresolved46C = 0;
    m_lodBlockIndices.first = 0; m_lodBlockIndices.last = 0; m_lodBlockIndices.end = 0;
    m_indexBufferLocked = 0;
    m_indexBufferBits = 0;
    m_flag489 = 0;
    m_compactVertices = 0;
    m_lowDetailTextureMode = 0;
    m_waterGridGeometry = 0;
    m_waterGridGeometryTemp = 0;
    m_materialParameters = 0;
    dx9rend::Ra::raObjectParametersConstruct(m_objectParameters);                 // sub_100458E0(this + 1180)
    m_waterEnvMap = 0;
    m_waterAnimSpeed = 10.0f;
    m_waterScroll.x = bits(0x3DCCCCCDu); m_waterScroll.y = bits(0x3DCCCCCDu);      // 0.1
    m_waterVolumeName = "watervolume";
    for (int i = 0; i < 4; ++i) m_unresolved6EC[i] = 0;
    m_unresolved700 = 0.5f;
    m_waterRecord = 0;
    m_waterTextureList = 0;
    m_waterCullHeightOffset = 0.0f; m_waterObjectRingCursor = 0; m_waterObjectRing = 0; m_waterAlphaHeightMul = 0.0f;
    m_waterAlphaAdd = bits(0x3EA3D70Au);                                          // 0.32
    // m_levelPath default-constructs
    m_lodIndexBuffer = 0;
    m_primitiveType = 4;
    m_lodTables = 0;
    for (int i = 0; i < 3; ++i) m_lodRangeStart[i] = 0;                          // +0x104..+0x124,
    for (int i = 0; i < 3; ++i) m_lodRangeIndexCount[i] = 0;                     // nine zero words
    for (int i = 0; i < 3; ++i) m_lodRangeTriangleCount[i] = 0;
    for (int i = 0; i < 8; ++i) m_surrounding[i] = 0;
    dx9rend::registerDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this), 1);
    dx9rend::registerRenderPathListener(static_cast<dx9rend::RenderPathListener*>(this));
    m_hiddenDetailLayer = -1;
    m_farSideTiling.x = 2.0f; m_farSideTiling.y = 2.0f;
    m_farTopTilingHi  = 16.0f;
    m_farTopTilingLow = 32.0f;
    m_farYOffset = 0.0f;
    m_cornerVertexBuffer = 0;
    m_sideVertexBuffer = 0;
    m_cornerIndexBuffer = 0;
    m_sideIndexBuffer = 0;
    m_sideVertexBuffer2 = 0;
    // The 0x150-byte Ra material block, engine heap, constructed in place; two defaults the body
    // then patches: the +0x70..+0x7C vec4 to (1,1,1,1) and the +0x84 float to 64.0f. Neither
    // store tests the pointer, as shipped.
    {
        void* const storage = ::renddx9abi::engineAlloc(0x150);
        float* block = 0;
        if (storage)
        {
            dx9rend::Ra::raMaterialParametersConstruct(storage);
            block = static_cast<float*>(storage);
        }
        m_materialParameters = block;
        block[28] = 1.0f; block[29] = 1.0f; block[30] = 1.0f; block[31] = 1.0f;
        *reinterpret_cast<float*>(static_cast<unsigned char*>(m_materialParameters) + 132) = 64.0f;
    }
    m_waterTechniqueMargin = 40.0f;
    m_waterTechnique            = raShaderNamed("Water2D");
    m_waterDistantTechnique     = raShaderNamed("Water2D");
    m_waterDistantTechnique2    = raShaderNamed("WaterDistant2D");
    m_waterSurroundingTechnique = raShaderNamed("WaterSurrounding2D");
    m_waterQuality = -1;
    m_waterQualityApplied = 0;
    m_water3D = 0;
    {
        // sub_1018F8A0 - the seven-float water record (1, 1, 1, 0.8, 0.5, 0.5, 0.5). The shipped
        // exchange frees the previous record whenever it differs from the new one - including the
        // initial null, which the engine free accepts.
        float* const record = static_cast<float*>(::renddx9abi::engineAlloc(0x1C));
        float* value = 0;
        if (record)
        {
            record[0] = 1.0f; record[1] = 1.0f; record[2] = 1.0f; record[3] = bits(0x3F4CCCCDu);
            record[4] = 0.5f; record[5] = 0.5f; record[6] = 0.5f;
            value = record;
        }
        if (value != m_waterRecord)
            ::renddx9abi::engineFree(m_waterRecord);
        m_waterRecord = value;
    }
}


// IBase - primary slots 0..3.

void  Terrain::addRef()  { ++m_refCount; }
int   Terrain::getRef()  { return m_refCount; }

int Terrain::release()
{
    const int left = --m_refCount;
    if (left == 0)
        deletingDestructor(1);                                                    // slot +0xB0 with flags 1
    return left;
}

void* Terrain::queryInterface(unsigned int iid) const
{
    if (iid == 1u || iid == kTerrainIid)
        return const_cast<Terrain*>(this);
    return 0;
}


// The small slots.

unsigned int Terrain::getReflectionTypeId() { return kTerrainClassId; }
void  Terrain::slot07_noop() {}
void  Terrain::setRenderStyle(int style)
{
    if (style < 2 && style >= 0)
        m_renderStyle = style;
}
int   Terrain::getRenderStyle() { return m_renderStyle; }
void  Terrain::slot11_noop() {}
int   Terrain::getPatchCount() { return m_patchCount; }
int   Terrain::getPatchSize()  { return m_patchSize; }
const Vec3* Terrain::getPrimaryWorldScale()   { return &m_primaryWorldScale; }
const Vec3* Terrain::getSecondaryWorldScale() { return &m_secondaryWorldScale; }
float Terrain::getPatchField9C(int index) { return m_patches[index]->minHeight; }                                      // 0x100D8920  fld +0x9C
float Terrain::getPatchField98(int index) { return m_patches[index]->maxHeight; }                                      // 0x100D8940  fld +0x98
void* Terrain::getPatchColormap(int index) { return m_patches[index]->colormap; }
void* Terrain::getPatchLightmap(int index) { return m_patches[index]->lightmap; }
bool  Terrain::getPatchHasNoDetail(int index) { return m_patches[index]->hasNoDetailLayers != 0; }
void  Terrain::setFarSideTiling(float x, float y) { m_farSideTiling.x = x; m_farSideTiling.y = y; }
Vec2* Terrain::getFarSideTiling(Vec2* out) { *out = m_farSideTiling; return out; }
void  Terrain::setFarTopTilingHi(float v)  { m_farTopTilingHi = v; }
float Terrain::getFarTopTilingHi()         { return m_farTopTilingHi; }
void  Terrain::setFarTopTilingLow(float v) { m_farTopTilingLow = v; }
float Terrain::getFarTopTilingLow()        { return m_farTopTilingLow; }
void  Terrain::setFarYOffset(float v)      { m_farYOffset = v; }
float Terrain::getFarYOffset()             { return m_farYOffset; }

// 0x100DADC0 - `terrain.sunColor r/g/b`: stored at a quarter, then each component pinned to 1
// when >= 1 and to 0 when below 0 or unordered (the FPU C0|C2 test), and the lighting goes dirty.
// The GI setter (0x100DAEC0) is the same body at a half.
void Terrain::setSunColor(float r, float g, float b)
{
    m_sunColor.z = b * 0.25f;
    m_sunColor.x = r * 0.25f;
    m_sunColor.y = g * 0.25f;
    float* const c = &m_sunColor.x;
    for (int i = 0; i < 3; ++i)
    {
        float v = c[i];
        if (v >= 1.0f) v = 1.0f;
        c[i] = v;
        if (!(v >= 0.0f)) v = 0.0f;
        c[i] = v;
    }
    m_lightingDirty = 1;
}

Vec3* Terrain::getSunColor(Vec3* out)                                             // times four
{
    out->x = m_sunColor.x * 4.0f; out->y = m_sunColor.y * 4.0f; out->z = m_sunColor.z * 4.0f;
    return out;
}
Vec3* Terrain::getSunColorRaw(Vec3* out) { *out = m_sunColor; return out; }

void Terrain::setGIColor(float r, float g, float b)
{
    m_giColor.z = b * 0.5f;
    m_giColor.x = r * 0.5f;
    m_giColor.y = g * 0.5f;
    float* const c = &m_giColor.x;
    for (int i = 0; i < 3; ++i)
    {
        float v = c[i];
        if (v >= 1.0f) v = 1.0f;
        c[i] = v;
        if (!(v >= 0.0f)) v = 0.0f;
        c[i] = v;
    }
    m_lightingDirty = 1;
}
Vec3* Terrain::getGIColorTimes2(Vec3* out)
{
    out->x = m_giColor.x + m_giColor.x; out->y = m_giColor.y + m_giColor.y; out->z = m_giColor.z + m_giColor.z;
    return out;
}
Vec3* Terrain::getGIColorRaw(Vec3* out) { *out = m_giColor; return out; }
Vec2* Terrain::getWaterScroll(Vec2* out) { *out = m_waterScroll; return out; }
void  Terrain::setWaterScroll(float x, float y) { m_waterScroll.x = x; m_waterScroll.y = y; }
float Terrain::getWaterAnimSpeed()        { return m_waterAnimSpeed; }
void  Terrain::setWaterAnimSpeed(float v) { m_waterAnimSpeed = v; }

vc71::string Terrain::getWaterNormalMap()
{
    return vc71::string(m_waterVolumeName.c_str(), m_waterVolumeName.size());
}

void Terrain::setWaterNormalMap(vc71::string name)
{
    m_waterVolumeName = name;
    setupWater();                                                                 // vslot +0xD4
}

Vec4* Terrain::getWaterSpecularColor(Vec4* out)                                   // material +0x70
{
    const float* const v = reinterpret_cast<const float*>(static_cast<unsigned char*>(m_materialParameters) + 112);
    out->x = v[0]; out->y = v[1]; out->z = v[2]; out->w = v[3];
    return out;
}

void Terrain::setWaterSpecularColor(float r, float g, float b, float a)
{
    float* const v = reinterpret_cast<float*>(static_cast<unsigned char*>(m_materialParameters) + 112);
    v[0] = r; v[1] = g; v[2] = b; v[3] = a;
}

float Terrain::getWaterSpecularPower()
{
    return *reinterpret_cast<const float*>(static_cast<unsigned char*>(m_materialParameters) + 132);
}

void Terrain::setWaterSpecularPower(float v)
{
    *reinterpret_cast<float*>(static_cast<unsigned char*>(m_materialParameters) + 132) = v;
}

// The unread bodies. Known signatures, so each names itself and stops (the client startup
// exception: a diagnostic-and-abort body at actual use, never a fabricated result). Neither is
// reached by the teardown (sub_100DD6C0 calls no slot but +0xDC).
void Terrain::drawDepthComplexity(int)  { RD9_FRONTIER("dice::hfe::geom::Terrain::drawDepthComplexity (0x100DB2F0, read - not written)", 0xB4); }
void Terrain::slot48_unread_100DB810(int, int, int) { RD9_FRONTIER("dice::hfe::geom::Terrain::slot48 (0x100DB810, the technique chooser)", 0xC0); }
void Terrain::slot49_noop(int) {}
// Device listeners; per-frame rendering is implemented in TerrainDraw.cpp.
void Terrain::onDeviceLost() {}
// Marks lighting constants dirty so the next frame reapplies them after device reset.
void Terrain::onDeviceReset() { m_lightingDirty = 1; }
void Terrain::onDeviceDestroy() {}
void Terrain::onRenderPathActivated(int path)   { m_renderPath = path; }
void Terrain::onRenderPathDeactivated() {}
void Terrain::onRenderPathChanged(int path)     { m_renderPath = path; }
void Terrain::onRenderPathNotification(int)
{
    // sub_10074300(6, __FILE__, 3313, "") << "TBM: Dont use this!" - dice's debug stream, kept as
    // one log line the way every recovered diagnostic in this image is.
    ::renddx9abi::logf("[Terrain.cpp:3313] TBM: Dont use this!");
}
int  Terrain::getCurrentRenderPath() const      { return m_renderPath; }
vc71::string Terrain::getRenderPathListenerName() const { return vc71::string("Terrain"); }


// init - 0x100D7920, 3,063 B. Console `terrain.init` and load()'s first call.

void Terrain::init()
{
    // The heightmap cluster singleton, its primary heightmap and that map's size. No test on any
    // of the three answers, as shipped.
    ::dice::hfe::world::IHeightmap* const heightmap = heightmapCluster()->getPrimaryHeightmap();   // +0x40
    m_primaryHeightmap = heightmap;
    heightmap->getSize(m_heightmapSizeX, m_heightmapSizeZ);                                   // +0x2C

    // Render style 2 means "decide from the caps": a vertex shader major below 2 is the low style.
    if (m_renderStyle == 2)
        m_renderStyle = (static_cast<unsigned int>(dx9rend::deviceCapsVertexShaderMajor()) < 2u) ? 1 : 0;

    m_effect = dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "Shaders/TerrainShader");

    dx9rend::Effect* const fx = static_cast<dx9rend::Effect*>(m_effect);
#define TERRAIN_PARAM(member, name) member = dx9rend::effectGetShaderParameter(fx, vc71::string(name))
    TERRAIN_PARAM(m_hScaleBaseUV,               "SCALEBASEUV");
    TERRAIN_PARAM(m_hScaleTransXZ,              "SCALETRANSXZ");
    TERRAIN_PARAM(m_hScaleTransY,               "SCALETRANSY");
    TERRAIN_PARAM(m_hStScaleTransY,             "STSCALETRANSY");
    TERRAIN_PARAM(m_hTexScale,                  "TEXSCALE");
    TERRAIN_PARAM(m_hYPlaneTexScaleAndFarTile,  "YPLANETEXSCALEANDFARTILE");
    TERRAIN_PARAM(m_hStTexScale,                "STTEXSCALE");
    TERRAIN_PARAM(m_hMatViewProj,               "matVIEWPROJ");
    TERRAIN_PARAM(m_hMatView,                   "matVIEW");
    TERRAIN_PARAM(m_hShadowMap,                 "SHADOWMAP");
    TERRAIN_PARAM(m_hMultipleShadowMap,         "MULTIPLESHADOWMAP");
    TERRAIN_PARAM(m_hTexLayer[0],               "TEXLAYER0");
    TERRAIN_PARAM(m_hTexLayer[1],               "TEXLAYER1");
    TERRAIN_PARAM(m_hTexLayer[2],               "TEXLAYER2");
    TERRAIN_PARAM(m_hTexLayer[3],               "TEXLAYER3");
    TERRAIN_PARAM(m_hTexLayer[4],               "TEXLAYER4");
    TERRAIN_PARAM(m_hTexLayer[5],               "TEXLAYER5");
    TERRAIN_PARAM(m_hTexLayer[6],               "TEXLAYER6");
    TERRAIN_PARAM(m_hComponentSelector,         "COMPONENTSELECTOR");
    TERRAIN_PARAM(m_hTexProjOffset,             "TEXPROJOFFSET");
    TERRAIN_PARAM(m_hTexProjScale,              "TEXPROJSCALE");
    TERRAIN_PARAM(m_hCameraPos,                 "CAMERAPOS");
    TERRAIN_PARAM(m_hSunColor,                  "SUNCOLOR");
    TERRAIN_PARAM(m_hGIColor,                   "GICOLOR");
    TERRAIN_PARAM(m_hAmbientColor,              "AMBIENTCOLOR");
    TERRAIN_PARAM(m_hSinglePointColor1X,        "SINGLEPOINTCOLOR_1X");
    TERRAIN_PARAM(m_hDynamicPointColor,         "DYNAMICPOINTCOLOR");
    TERRAIN_PARAM(m_hFogRange,                  "fogRange");
    TERRAIN_PARAM(m_hFogColor,                  "fogColor");
    TERRAIN_PARAM(m_hAmbientIlluminationRange,  "AmbientIlluminationRange");
    TERRAIN_PARAM(m_hAmbientIlluminationColor,  "AmbientIlluminationColor");
    TERRAIN_PARAM(m_hWaterFogRange,             "WaterFogRange");
    TERRAIN_PARAM(m_hWaterFogColor,             "WaterFogColor");
    TERRAIN_PARAM(m_hMorphDeltaSelector,        "MORPHDELTASELECTOR");
    TERRAIN_PARAM(m_hMorphDeltaAdder,           "MORPHDELTAADDER");
    TERRAIN_PARAM(m_hNearFarMorphLimits,        "NEARFARMORPHLIMITS");
    TERRAIN_PARAM(m_hFarTexTiling,              "FARTEXTILING");
    TERRAIN_PARAM(m_hStFarTexTiling,            "STFARTEXTILING");
    TERRAIN_PARAM(m_hNearTexTiling,             "NEARTEXTILING");
    TERRAIN_PARAM(m_hLightViewProj,             "LIGHTVIEWPROJ");
    TERRAIN_PARAM(m_hLightViewProjOrtho,        "LIGHTVIEWPROJORTHO");
    TERRAIN_PARAM(m_hViewportMap,               "VIEWPORTMAP");
    TERRAIN_PARAM(m_hStTransXZ,                 "STTRANSXZ");
    TERRAIN_PARAM(m_hPointLight,                "POINTLIGHT");
    TERRAIN_PARAM(m_hSpotLight,                 "SPOTLIGHT");
    TERRAIN_PARAM(m_hTerrainWaterColor,         "TerrainWaterColor");
    TERRAIN_PARAM(m_hWaterHeight,               "WaterHeight");
    TERRAIN_PARAM(m_hCameraToWater,             "CameraToWater");
    TERRAIN_PARAM(m_hColorLightTex,             "COLORLIGHTTEX");
    TERRAIN_PARAM(m_hNightTimeMode,             "NightTimeMode");
    TERRAIN_PARAM(m_hNightOccludedSkyIntensity, "NightOccludedSkyIntensity");
    TERRAIN_PARAM(m_hCausticsMap,               "CausticsMap");
    TERRAIN_PARAM(m_hCausticsMapFrame0,         "CausticsMapFrame0");
    TERRAIN_PARAM(m_hCausticsMapFrame1,         "CausticsMapFrame1");
    TERRAIN_PARAM(m_hWaterScroll,               "WaterScroll");
    TERRAIN_PARAM(m_hWaterCycleTime,            "WaterCycleTime");
    TERRAIN_PARAM(g_terrainShadowViewProjArray, "ShadowViewProjArray");
    TERRAIN_PARAM(g_terrainShadowPosArray,      "ShadowPosArray");
    TERRAIN_PARAM(g_terrainShadowSizeArray,     "ShadowSizeArray");
    TERRAIN_PARAM(g_terrainShadowCount,         "ShadowCount");
    TERRAIN_PARAM(m_hLightPos1,                 "LIGHTPOS1");
    TERRAIN_PARAM(m_hLightAttSqrInv1,           "LIGHTATTSQRINV1");
#undef TERRAIN_PARAM

    // The declarations. The low style pairs kDeclLowStyle with kDeclLowStyleGrid; the high style
    // pairs the SHORT2 grid with a surrounding declaration whose texcoords are halves when +0x3A8
    // says so. Both styles then take the full surrounding declaration.
    if (m_renderStyle == 1)
    {
        m_surroundingDeclaration = dx9rend::vertexDeclarationCreate(kDeclLowStyle);
        m_gridDeclaration        = dx9rend::vertexDeclarationCreate(kDeclLowStyleGrid);
    }
    else
    {
        m_gridDeclaration        = dx9rend::vertexDeclarationCreate(kDeclSurroundingGrid);
        m_surroundingDeclaration = dx9rend::vertexDeclarationCreate(
            m_halfFloatSurrounding == 0 ? kDeclSurroundingFloat : kDeclSurroundingHalf);
    }
    m_surroundingFullDeclaration = dx9rend::vertexDeclarationCreate(kDeclSurroundingFull);

    // The culler, 0x7C bytes on the engine heap.
    void* const storage = ::renddx9abi::engineAlloc(0x7C);
    if (storage)
        m_culler = new (storage) TerrainCuller();
    else
        m_culler = 0;
}


// load - 0x100DE070, 1,332 B. Console `terrain.load <path>`.

void Terrain::load(const vc71::string& path)
{
    // The resource manager's default texture priority is forced to 10 for the duration.
    const int savedPriority = dx9rend::resourceManagerSwapDefaultPriority(10);

    // Video option 0 - the "Terrain" quality - at 1 selects the low-detail arms.
    if (dx9rend::rendererGetVideoOptions()->getQuality(0) == 1)
    {
        m_lowQuality  = 1;
        m_lowQualityB = 1;
    }
    m_levelPath = path;

    ::dice::hfe::eventManager->postEvent(ECHud, 7u, const_cast<char*>("Loading Terrain.."), 0.0f);
    init();                                                                       // vslot +0x14

    TerrainRawReader reader;
    reader.open(path, 2, 0, 0);                                                   // sub_100763A0(path, 2, 0, 0) - 0x100DE0EC
    reader.readU32(&m_rawVersion);                                               // the header pair: version, flag
    if (m_rawHasVersion <= 1u && m_rawVersion <= 0x15u)
    {
        // sub_10074300(6, __FILE__, 772, "") << the message - dice's debug stream, kept as one line.
        ::renddx9abi::logf("[Terrain.cpp:772] TBM: Terrain raw data is not latest version! Most be recompiled!");
    }
    reader.readVec3(&m_primaryWorldScale);
    reader.readVec3(&m_secondaryWorldScale);
    reader.readU32(&m_fileWord2C);
    reader.readU32(&m_maxHeight);
    reader.readU32(&m_minHeight);
    reader.readU32(&m_patchSize);
    reader.readByte(&m_subdividePatches);
    reader.readU32(&m_patchCount);
    reader.readU32(&m_patchDetailmapSize);                                        // +0x44
    reader.readU32(&m_patchColormapSize);                                         // +0x48
    reader.readLine(m_colormapBaseName, 10);
    reader.readLine(m_detailmapBaseName, 10);
    reader.readLine(m_lowDetailmapBaseName, 10);
    reader.readLine(m_lightmapBaseName, 10);
    loadLowDetailTexture();
    reader.readVec2(&m_farSideTiling);
    reader.readU32(&m_farTopTilingHi);
    m_farTopTilingLow = m_farTopTilingHi;
    if (m_rawHasVersion == 0 || m_rawVersion < 0x17u)
        m_farTopTilingLow = m_farTopTilingHi;
    else
        reader.readU32(&m_farTopTilingLow);
    reader.readU32(&m_farYOffset);
    reader.readVec3(&m_sunColor);
    m_sunColor.x *= 0.25f; m_sunColor.y *= 0.25f; m_sunColor.z *= 0.25f;
    reader.readVec3(&m_giColor);
    m_giColor.x *= 0.5f; m_giColor.y *= 0.5f; m_giColor.z *= 0.5f;
    reader.readVec3(&m_terrainWaterColor);
    reader.readU32(&m_detailTextureCount);
    m_fileOffsetDetail = reader.tell();
    loadDetailTextures(reader);

    if (m_rawHasVersion && m_rawVersion >= 0x18u)
    {
        loadLodTablesAndBaseGrid(reader);
    }
    else
    {
        // Legacy content uses a single stream, border-cleared grid, and generated LOD indices.
        m_vertexDeclaration = dx9rend::vertexDeclarationCreate(kDeclPatchSingle);
        buildLegacyBaseGridVertexTable();                                         // vslot +0xD0
        for (int i = 0; i < (m_patchSize + 1) * (m_patchSize + 1); ++i)
        {
            static_cast<unsigned char*>(m_legacyBaseGridTable)[4 * i + 3] = 0;
            static_cast<unsigned char*>(m_legacyBaseGridTable)[4 * i] = 0;
        }
        createLodIndexBuffer();
    }

    if (m_rawHasVersion && m_rawVersion >= 0x1Au)
    {
        m_lodTablesFromFile = 1;
    }
    else
    {
        // The pre-0x1A arm: one 400-byte LOD record whose three 64-byte rows are seeded from the
        // LOD range starts (+0x104) and index counts (+0x110) and built by sub_100D87B0. 1.50
        // content never comes here.
        void* const storage = ::renddx9abi::engineAlloc(0x190);
        unsigned char* table = 0;
        if (storage)
        {
            ::memset(storage, 0, 0x190u);
            table = static_cast<unsigned char*>(storage);
        }
        m_lodTables = table;
        for (int row = 0; row < 3; ++row)
        {
            *reinterpret_cast<int*>(m_lodTables + 64 * row)       = m_lodRangeStart[row];
            *reinterpret_cast<int*>(m_lodTables + 64 * row + 192) = m_lodRangeIndexCount[row];
            RD9_FRONTIER("dice::hfe::geom::Terrain::load (sub_100D87B0 - the pre-0x1A LOD record builder)", 0x18);
        }
        m_lodTablesFromFile = 0;
    }

    m_culler->init(this, 32, 64);
    m_fileOffsetPatches = reader.tell();
    loadPatches(reader);
    loadSurroundingPatches(reader);
    if (m_rawHasVersion && m_rawVersion >= 0x18u)
        m_culler->load(reader);
    else
        RD9_FRONTIER_AT("dice::hfe::geom::Terrain::load (sub_100E68B0 - the pre-0x18 culler builder)", m_culler, 0x18);

    if (!m_rawHasVersion || m_rawVersion < 0x18u)
    {
        for (int j = 0; j < m_patchCount * m_patchCount; ++j)
        {
            if (m_patches[j]->legacyVertexTable)
                ::renddx9abi::engineFree(m_patches[j]->legacyVertexTable);
        }
        ::renddx9abi::engineFree(m_legacyBaseGridTable);
    }
    for (int s = 0; s < 8; ++s)
    {
        if (m_surrounding[s])
            ::renddx9abi::engineFree(m_surrounding[s]->xzBytes);
    }

    setupWater();                                                                 // vslot +0xD4

    if (!m_indexBuffer)
    {
        const int savedPool = dx9rend::bufferManagerSwapPool(0);
        m_indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(m_indexBufferCount, 520u, 101u);   // dynamic|writeonly, INDEX16
        dx9rend::bufferManagerSwapPool(savedPool);
    }
    ::dice::hfe::eventManager->postEvent(ECHud, 7u, const_cast<char*>("Loading Templates.."), 0.0f);
    reader.close();
    dx9rend::resourceManagerSwapDefaultPriority(savedPriority);
}


// sub_100DB100 - the level-wide low-detail texture: <GLGameLevelPath>lowdetailtexture{low,hi}.dds,
// then lowdetailtexture.dds, then common/terrain/textures/Default.dds. Mode 1/2/3/4 records which.

void Terrain::loadLowDetailTexture()
{
    vc71::string levelPath;
    dx9rend::readStringSettingDefault("GLGameLevelPath", " FEEEEL ", levelPath);

    int mode;
    const char* suffix;
    if (m_lowQuality)
    {
        mode = 1;
        if (m_lowDetailTextureMode == 1)
            return;
        suffix = "lowdetailtexturelow.dds";
    }
    else
    {
        mode = 2;
        if (m_lowDetailTextureMode == 2)
            return;
        suffix = "lowdetailtexturehi.dds";
    }
    vc71::string name(levelPath.c_str());
    name += suffix;
    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(name)))
    {
        const int savedClass = dx9rend::textureManagerSwapDefaultMipClass(3);
        void* const texture = dx9rend::textureManagerGetTexture(name);
        m_lowDetailTextureMode = mode;
        m_lowDetailTexture = texture;
        dx9rend::textureManagerSwapDefaultMipClass(savedClass);
    }
    if (!m_lowDetailTexture && m_lowDetailTextureMode != 3)
    {
        vc71::string fallback(levelPath.c_str());
        fallback += "lowdetailtexture.dds";
        void* texture;
        if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(fallback)))
        {
            const int savedClass = dx9rend::textureManagerSwapDefaultMipClass(3);
            texture = dx9rend::textureManagerGetTexture(fallback);
            m_lowDetailTextureMode = 3;
            m_lowDetailTexture = texture;
            dx9rend::textureManagerSwapDefaultMipClass(savedClass);
        }
        else
        {
            const int savedClass = dx9rend::textureManagerSwapDefaultMipClass(3);
            fallback = "common/terrain/textures/Default.dds";
            texture = dx9rend::textureManagerGetTexture(fallback);
            m_lowDetailTextureMode = 4;
            m_lowDetailTexture = texture;
            dx9rend::textureManagerSwapDefaultMipClass(savedClass);
        }
    }
}


// sub_100DD280 - the detail texture records. One record per m_detailTextureCount, its name a
// '\n'-terminated line, its textures loaded under mip class 5 with the optional _normal / _side /
// _sideNormal companions; then five file fields. The low-quality arm parses the same fields into
// a stack record and loads nothing.

void Terrain::loadDetailTextures(TerrainRawReader& reader)
{
    if (m_lowQuality == 0)
    {
        for (int n = 0; n < m_detailTextureCount; ++n)
        {
            TerrainDetailTexture* record = 0;
            void* const storage = ::renddx9abi::engineAlloc(0x2C);
            if (storage)
                record = new (storage) TerrainDetailTexture();
            vc71::vectorPushBackIn(m_detailTextures, 4u, &record);              // the +0x338 vector

            vc71::string name;
            reader.readLine(name, 10);
            if (name.size())
            {
                const int savedClass = dx9rend::textureManagerSwapDefaultMipClass(5);
                record->texture = dx9rend::textureManagerGetTexture(name);
                vc71::string normal(name.c_str()); normal += "_normal";
                if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(normal)))
                    record->normalTexture = dx9rend::textureManagerGetTexture(normal);
                vc71::string side(name.c_str()); side += "_side";
                if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(side)))
                {
                    record->sideTexture = dx9rend::textureManagerGetTexture(side);
                    vc71::string sideNormal(name.c_str()); sideNormal += "_sideNormal";
                    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(sideNormal)))
                        record->sideNormalTexture = dx9rend::textureManagerGetTexture(sideNormal);
                }
                else
                {
                    // No side pair: the base pair serves for it, with a reference each.
                    record->sideTexture       = record->texture;
                    record->sideNormalTexture = record->normalTexture;
                    if (record->sideTexture)
                        dx9rend::textureAddRef(static_cast<dx9rend::EngineTexture*>(record->sideTexture));
                    if (record->sideNormalTexture)
                        dx9rend::textureAddRef(static_cast<dx9rend::EngineTexture*>(record->sideNormalTexture));
                }
                dx9rend::textureManagerSwapDefaultMipClass(savedClass);
            }
            reader.readByte(&record->planeMap);                                   // +0x15
            reader.readVec2(&record->sideTiling);                                 // +0x1C and +0x20
            reader.readU32(&record->topTiling);                                   // +0x18
            reader.readU32(&record->yOffset);                                     // +0x24
            reader.readByte(&record->envMap);                                     // +0x28
        }
    }
    else
    {
        TerrainDetailTexture record;
        vc71::string name;
        for (int n = 0; n < m_detailTextureCount; ++n)
        {
            reader.readLine(name, 10);
            reader.readByte(&record.planeMap);
            reader.readVec2(&record.sideTiling);
            reader.readU32(&record.topTiling);
            reader.readU32(&record.yOffset);
            reader.readByte(&record.envMap);
        }
        // the stack record's destructor (sub_100D8C80) runs here: four null releases, four evictions
    }
}


// sub_100DA9C0 - the LOD tables, the LOD index buffer and the base grid.

void Terrain::loadLodTablesAndBaseGrid(TerrainRawReader& reader)
{
    if (m_rawHasVersion && m_rawVersion >= 0x1Au)
    {
        // 85 records of 400 bytes. The first 21 link to four children each (records 1+4i..4+4i)
        // and read their three 64-byte rows as u32 pairs in the shipped interleaving; records 21..84
        // are read as plain (row, row+192) pairs.
        m_lodTables = static_cast<unsigned char*>(::renddx9abi::engineAlloc(34000));
        int record = 0;
        int offset = 0;
        for (int child = 400; child < 34000; child += 1600)
        {
            unsigned int* const table = reinterpret_cast<unsigned int*>(m_lodTables + offset);
            ::memset(table, 0, 0x190u);
            if (record < 21)
            {
                table[96] = reinterpret_cast<unsigned int>(m_lodTables + child);
                table[97] = reinterpret_cast<unsigned int>(m_lodTables + child + 400);
                table[98] = reinterpret_cast<unsigned int>(m_lodTables + child + 800);
                table[99] = reinterpret_cast<unsigned int>(m_lodTables + child + 1200);
            }
            unsigned int* row = table + 1;
            for (int r = 0; r < 3; ++r)
            {
                reader.readU32(row - 1);
                reader.readU32(row + 47);
                if (r)
                {
                    reader.readU32(row);      reader.readU32(row + 48);
                    reader.readU32(row + 1);  reader.readU32(row + 49);
                    reader.readU32(row + 3);  reader.readU32(row + 51);
                    reader.readU32(row + 7);  reader.readU32(row + 55);
                    reader.readU32(row + 4);  reader.readU32(row + 52);
                    reader.readU32(row + 8);  reader.readU32(row + 56);
                    reader.readU32(row + 5);  reader.readU32(row + 53);
                    reader.readU32(row + 9);  reader.readU32(row + 57);
                }
                row += 16;
            }
            ++record;
            offset += 400;
        }
        if (record < 85)
        {
            for (int o = 400 * record; o < 34000; o += 400)
            {
                unsigned char* const table = m_lodTables + o;
                ::memset(table, 0, 0x190u);
                unsigned char* row = table;
                for (int r = 0; r < 3; ++r)
                {
                    reader.readU32(row);
                    reader.readU32(row + 192);
                    row += 64;
                }
            }
        }
    }
    else
    {
        // The 0x18/0x19 arm reads three triples straight into +0x104.. / +0x11C.. / +0x110..: per
        // LOD the range start, its triangle count, its index count.
        for (int i = 0; i < 3; ++i)
        {
            reader.readU32(&m_lodRangeStart[i]);
            reader.readU32(&m_lodRangeTriangleCount[i]);
            reader.readU32(&m_lodRangeIndexCount[i]);
        }
    }

    reader.readU32(&m_lodIndexCount);
    m_lodIndexBuffer = dx9rend::bufferManagerCreateIndexBuffer(m_lodIndexCount, 8u, 101u);   // writeonly, INDEX16
    void* const indices = dx9rend::indexBufferLock(m_lodIndexBuffer, 0, 0, 0);
    if (indices)
        reader.read(indices, 2u * m_lodIndexCount);
    dx9rend::indexBufferUnlock(m_lodIndexBuffer);

    if (m_rawHasVersion && m_rawVersion >= 0x19u)
        reader.readByte(&m_compactVertices);

    const D3DVERTEXELEMENT9* declaration;
    if (m_compactVertices)
    {
        // "Terrain_BaseGrid": (patchSize+1)^2 four-byte vertices - x and z as bytes, a border
        // code in byte 0 (row or column position mod 4, 3 folded to 1; 0 inside), byte 3 clear.
        const int n = m_patchSize;
        m_baseGridVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
            static_cast<unsigned int>((n + 1) * (n + 1)), 4u, 8u, "Terrain_BaseGrid");
        unsigned char* const grid = static_cast<unsigned char*>(
            dx9rend::vertexBufferLock(m_baseGridVertexBuffer, 0, 0, 0));
        if (grid)
        {
            for (int row = 0; row < m_patchSize + 1; ++row)
            {
                for (int col = 0; col < m_patchSize + 1; ++col)
                {
                    unsigned char* const v = grid + 4 * (col + row * (m_patchSize + 1));
                    v[2] = static_cast<unsigned char>(col);
                    v[1] = static_cast<unsigned char>(row);
                    if (col == 0 || col == m_patchSize)
                    {
                        v[0] = static_cast<unsigned char>(row % 4);
                        if (v[0] == 3) v[0] = 1;
                    }
                    else if (row == 0 || row == m_patchSize)
                    {
                        v[0] = static_cast<unsigned char>(col % 4);
                        if (v[0] == 3) v[0] = 1;
                    }
                    else
                        v[0] = 0;
                    v[3] = 0;
                }
            }
        }
        dx9rend::vertexBufferUnlock(m_baseGridVertexBuffer);
        declaration = kDeclPatchCompact;
    }
    else
    {
        declaration = kDeclPatchSingle;
    }
    m_vertexDeclaration = dx9rend::vertexDeclarationCreate(declaration);
}


// (slot +0xD0) - the pre-0x18 base grid table: the same border coding as above into a
// host array at +0xD4.

void Terrain::buildLegacyBaseGridVertexTable()
{
    const int n = m_patchSize;
    unsigned char* const table = static_cast<unsigned char*>(
        ::renddx9abi::engineAlloc(static_cast<unsigned int>(4 * (n + 1) * (n + 1))));
    m_legacyBaseGridTable = table;
    for (int row = 0; row < n + 1; ++row)
    {
        for (int col = 0; col < n + 1; ++col)
        {
            unsigned char* const v = table + 4 * (col + row * (n + 1));
            v[2] = static_cast<unsigned char>(col);
            v[1] = static_cast<unsigned char>(row);
            if (col == 0 || col == n)
            {
                v[0] = static_cast<unsigned char>(row % 4);
                if (v[0] == 3) v[0] = 1;
            }
            else if (row == 0 || row == n)
            {
                v[0] = static_cast<unsigned char>(col % 4);
                if (v[0] == 3) v[0] = 1;
            }
            else
                v[0] = 0;
            v[3] = 0;
        }
    }
}


// Legacy terrain LOD-index construction requires the base-grid table at +0xD4.
// TerrainEditable builds a different grid and leaves this table null; its path must
// avoid this optimizer until editor-only handling supplies a compatible route.


// Replaces the INDEX16 buffer with three optimized LOD ranges.
// Failed locks skip generation; unlock still runs.

void Terrain::createLodIndexBuffer()
{
    int size = m_patchSize;
    m_primitiveType = 4;                                                          // D3DPT_TRIANGLELIST
    m_lodIndexCount = 0;
    for (int lod = 0; lod < 3; ++lod)
    {
        if (lod)
            m_lodRangeStart[lod] = m_lodRangeIndexCount[lod - 1] + m_lodRangeStart[lod - 1];
        else
            m_lodRangeStart[0] = 0;
        const int triangles = size * size * 2;
        m_lodRangeTriangleCount[lod] = triangles;
        m_lodRangeIndexCount[lod] = triangles * 3;
        m_lodIndexCount += static_cast<unsigned int>(triangles * 3);
        size >>= 1;
    }

    if (m_lodIndexBuffer)
        dx9rend::indexBufferRelease(m_lodIndexBuffer);                            // --ref, sub_10014300 + free at zero
    m_lodIndexBuffer = dx9rend::bufferManagerCreateIndexBuffer(m_lodIndexCount, 8u, 101u);   // writeonly, INDEX16
    unsigned short* cursor = static_cast<unsigned short*>(
        dx9rend::indexBufferLock(m_lodIndexBuffer, 0, 0, 0));
    for (int lod = 0; cursor && lod < 3; ++lod)
        createLodIndices(cursor, lod, 0, 0, 0);
    dx9rend::indexBufferUnlock(m_lodIndexBuffer);
}


// Appends optimized indices for one LOD's quadtree block and advances cursor.
// Visits leaves in quadrant order and retains the final leaf in scratch storage.

void Terrain::createLodIndices(unsigned short*& cursor, int lod, int x, int z, int depth)
{
    const int size = m_patchSize >> depth;
    const int next = depth + 1;
    if (next < 4)
    {
        createLodIndices(cursor, lod, x, z, next);
        createLodIndices(cursor, lod, x + size / 2, z, next);
        createLodIndices(cursor, lod, x, z + size / 2, next);
        createLodIndices(cursor, lod, x + size / 2, z + size / 2, next);         // the shipped image's loop
        return;
    }

    const int stride = m_patchSize + 1;
    const int step = m_patchSize / (m_patchSize >> lod);
    if (m_lodBlockIndices.first)                                                  // resize(0) - sub_100D20A0(0, 0)
        m_lodBlockIndices.last = m_lodBlockIndices.first;
    for (int row = z; row < z + size; row += step)
    {
        for (int col = x; col < x + size; col += step)
        {
            const unsigned short a = static_cast<unsigned short>(row * stride + col);
            const unsigned short b = static_cast<unsigned short>(row * stride + step + col);
            const unsigned short c = static_cast<unsigned short>((row + step) * stride + col);
            const unsigned short d = static_cast<unsigned short>((row + step) * stride + step + col);
            vc71::vectorPushBackIn(m_lodBlockIndices, 2u, &a);
            vc71::vectorPushBackIn(m_lodBlockIndices, 2u, &b);
            vc71::vectorPushBackIn(m_lodBlockIndices, 2u, &c);
            vc71::vectorPushBackIn(m_lodBlockIndices, 2u, &c);
            vc71::vectorPushBackIn(m_lodBlockIndices, 2u, &b);
            vc71::vectorPushBackIn(m_lodBlockIndices, 2u, &d);
        }
    }
    optimizeLodIndices(m_lodBlockIndices);
    const unsigned int count = vc71::vectorSizeIn(m_lodBlockIndices, 2u);
    ::memcpy(cursor, m_lodBlockIndices.first, 2u * count);                       // rep movsd / movsb
    cursor += vc71::vectorSizeIn(m_lodBlockIndices, 2u);
}


// Reorders one leaf's indices in place for the vertex cache through D3DX meshes.
// Requires the legacy base grid and successful device allocations; HRESULTs are unchecked.
// D3DXCreateMeshFVF, D3DXCreateMesh and D3DXCreateBuffer are static imports of the shipped image
// (IAT 0x101DB47C, 0x101DB450, 0x101DB41C).

void Terrain::optimizeLodIndices(vc71::vector_layout& indices)
{
    ID3DXMesh* mesh = 0;
    {
        IDirect3DDevice9* const device = static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device;
        const int side = m_patchSize + 1;
        D3DXCreateMeshFVF(vc71::vectorSizeIn(indices, 2u) / 3u, static_cast<unsigned long>(side * side),
                          0x110u, 2u, device, &mesh);
    }
    void* data = 0;
    mesh->LockIndexBuffer(0, &data);
    ::memcpy(data, indices.first, 2u * vc71::vectorSizeIn(indices, 2u));
    mesh->UnlockIndexBuffer();

    float* position = 0;
    mesh->LockVertexBuffer(0, reinterpret_cast<void**>(&position));
    for (int i = 0; i < (m_patchSize + 1) * (m_patchSize + 1); ++i)
    {
        position[0] = static_cast<float>(static_cast<const unsigned char*>(m_legacyBaseGridTable)[4 * i + 2]);
        position[1] = 0.0f;
        position[2] = static_cast<float>(static_cast<const unsigned char*>(m_legacyBaseGridTable)[4 * i + 1]);
        position += 3;
    }
    mesh->UnlockVertexBuffer();

    ID3DXBuffer* adjacency = 0;
    D3DXCreateBuffer(12u * mesh->GetNumFaces(), &adjacency);
    mesh->GenerateAdjacency(0.1f, static_cast<DWORD*>(adjacency->GetBufferPointer()));
    mesh->Release();

    mesh = 0;
    {
        IDirect3DDevice9* const device = static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device;
        const int side = m_patchSize + 1;
        D3DXCreateMesh(vc71::vectorSizeIn(indices, 2u) / 3u, static_cast<unsigned long>(side * side),
                       0x110u, kDeclPatchSingle, device, &mesh);
    }
    data = 0;
    mesh->LockIndexBuffer(0, &data);
    ::memcpy(data, indices.first, 2u * vc71::vectorSizeIn(indices, 2u));
    mesh->UnlockIndexBuffer();
    mesh->OptimizeInplace(0x14400000u, static_cast<const DWORD*>(adjacency->GetBufferPointer()), 0, 0, 0);
    adjacency->Release();

    const DWORD indexCount = 3u * mesh->GetNumFaces();
    mesh->LockIndexBuffer(0, &data);
    ::memcpy(indices.first, data, 2u * indexCount);
    mesh->UnlockIndexBuffer();
    mesh->Release();
}


// sub_100D8B20 - the patch grid: patchCount^2 QuadPatches, each constructed then loaded.
// A failed allocation leaves a null in the table and the body still calls load on it, as shipped.

void Terrain::loadPatches(TerrainRawReader& reader)
{
    m_patches = static_cast<QuadPatch**>(
        ::renddx9abi::engineAlloc(static_cast<unsigned int>(4 * m_patchCount * m_patchCount)));
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
    {
        void* const storage = ::renddx9abi::engineAlloc(0x120);
        QuadPatch* const patch = storage ? new (storage) QuadPatch(this) : 0;
        m_patches[i] = patch;
        patch->load(reader);
    }
}


// sub_100D8BA0 - the surrounding ring: slot indices until -1, the ring geometry built once
// (slot +0xCC) before the first one.

void Terrain::loadSurroundingPatches(TerrainRawReader& reader)
{
    int slot = 0;
    reader.readU32(&slot);
    while (slot != -1)
    {
        if (!m_surroundingBuilt)
        {
            buildSurroundingGeometry();                                           // vslot +0xCC
            m_surroundingBuilt = 1;
        }
        void* const storage = ::renddx9abi::engineAlloc(0x168);
        SurroundingPatch* const patch = storage ? new (storage) SurroundingPatch(this) : 0;
        m_surrounding[slot] = patch;
        patch->load(slot, reader);
        reader.readU32(&slot);
    }
}


// sub_100DE740 - the ring grid generator: vertices (x, z, u, v) over [x0..x1] x [z0..z1] at the
// given steps into `vertices` (16-byte records), and the two triangles per cell as u32 indices
// into `indices`, offset by the vertices already present.

void Terrain::generateSurroundingGrid(int x0, int z0, int x1, int z1, int stepX, int stepZ,
                                      vc71::vector_layout& indices, vc71::vector_layout& vertices)
{
    const int   spanInt = static_cast<int>(m_primaryWorldScale.x) * (m_heightmapSizeX - 1);
    const float span    = static_cast<float>(spanInt);
    const unsigned int firstVertex = vc71::vectorSizeIn(vertices, 16u);

    for (int z = z0; z < z1 + 1; z += stepZ)
    {
        const float zf = static_cast<float>(z);
        for (int x = x0; x < x1 + 1; x += stepX)
        {
            const float xf = static_cast<float>(x);
            float v[4];
            v[0] = xf;
            v[1] = zf;
            v[2] = static_cast<float>(static_cast<double>(xf) * m_secondaryWorldScale.x / span);
            v[3] = static_cast<float>(static_cast<double>(zf) * m_secondaryWorldScale.z / span);
            vc71::vectorPushBackIn(vertices, 16u, v);
        }
    }

    const int rows = (z1 - z0) / stepZ;
    const int cols = (x1 - x0) / stepX;
    for (int row = 0; row < rows; ++row)
    {
        int nextRow = (cols + 1) * (row + 1) + static_cast<int>(firstVertex) + 1;
        int current = row * (cols + 1) + static_cast<int>(firstVertex) + 1;
        for (int col = 0; col < cols; ++col)
        {
            const int a = current;
            const int b = current + cols;
            const int c = current - 1;
            const int d = nextRow;
            vc71::vectorPushBackIn(indices, 4u, &a);
            vc71::vectorPushBackIn(indices, 4u, &b);
            vc71::vectorPushBackIn(indices, 4u, &c);
            vc71::vectorPushBackIn(indices, 4u, &a);
            vc71::vectorPushBackIn(indices, 4u, &d);
            vc71::vectorPushBackIn(indices, 4u, &b);
            ++nextRow;
            ++current;
        }
    }
}


// (slot +0xCC) - the surrounding ring's shared geometry: the side grid (three strips of
// falling resolution away from the terrain edge, "Terrain_Surrounding") and the corner grid (five
// blocks growing from the corner, "TerrainEditable"), each a vertex/index buffer pair, plus a
// third vertex buffer that is created and never filled.

void Terrain::buildSurroundingGeometry()
{
    if (m_sideVertexBuffer)   { dx9rend::vertexBufferRelease(m_sideVertexBuffer);   m_sideVertexBuffer = 0; }
    if (m_cornerVertexBuffer) { dx9rend::vertexBufferRelease(m_cornerVertexBuffer); m_cornerVertexBuffer = 0; }
    if (m_sideIndexBuffer)    { dx9rend::indexBufferRelease(m_sideIndexBuffer);     m_sideIndexBuffer = 0; }
    if (m_cornerIndexBuffer)  { dx9rend::indexBufferRelease(m_cornerIndexBuffer);   m_cornerIndexBuffer = 0; }

    const int span = static_cast<int>(
        static_cast<double>(m_heightmapSizeX - 1) * m_primaryWorldScale.x / m_secondaryWorldScale.x);
    const int quarter = span / 4;
    const int half    = span / 2;

    vc71::vector_layout vertices = { 0, 0, 0, 0 };
    vc71::vector_layout indices  = { 0, 0, 0, 0 };
    generateSurroundingGrid(0, 0,       span, quarter, 1, 1, indices, vertices);
    generateSurroundingGrid(0, quarter, span, half,    2, 2, indices, vertices);
    generateSurroundingGrid(0, half,    span, span,    4, 4, indices, vertices);

    unsigned int vertexCount = vc71::vectorSizeIn(vertices, 16u);
    m_sideVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        vertexCount, m_halfFloatSurrounding ? 8u : 12u, 8u, "Terrain_Surrounding");
    uploadSurroundingVertices(m_sideVertexBuffer, vertices);
    m_sideVertexCount = vc71::vectorSizeIn(vertices, 16u);

    unsigned int indexCount = vc71::vectorSizeIn(indices, 4u);
    m_sideIndexBuffer = dx9rend::bufferManagerCreateIndexBuffer(indexCount, 8u, 101u);
    uploadSurroundingIndices(m_sideIndexBuffer, indices);
    m_sideIndexCount = vc71::vectorSizeIn(indices, 4u);

    // The third buffer: created from the same count, never uploaded.
    vertexCount = vc71::vectorSizeIn(vertices, 16u);
    m_sideVertexBuffer2 = dx9rend::bufferManagerCreateVertexBuffer(
        vertexCount, m_halfFloatSurrounding ? 8u : 16u, 8u, "Terrain_Surrounding");

    if (vertices.first) ::renddx9abi::engineFree(vertices.first);
    vertices.first = 0; vertices.last = 0; vertices.end = 0;
    if (indices.first) ::renddx9abi::engineFree(indices.first);
    indices.first = 0; indices.last = 0; indices.end = 0;

    generateSurroundingGrid(0,       0,       quarter, quarter, 1, 1, indices, vertices);
    generateSurroundingGrid(quarter, 0,       half,    quarter, 2, 2, indices, vertices);
    generateSurroundingGrid(0,       quarter, half,    half,    2, 2, indices, vertices);
    generateSurroundingGrid(half,    0,       span,    half,    4, 4, indices, vertices);
    generateSurroundingGrid(0,       half,    span,    span,    4, 4, indices, vertices);

    vertexCount = vc71::vectorSizeIn(vertices, 16u);
    m_cornerVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        vertexCount, m_halfFloatSurrounding ? 8u : 12u, 8u, "TerrainEditable");
    uploadSurroundingVertices(m_cornerVertexBuffer, vertices);
    m_cornerVertexCount = vc71::vectorSizeIn(vertices, 16u);

    indexCount = vc71::vectorSizeIn(indices, 4u);
    m_cornerIndexBuffer = dx9rend::bufferManagerCreateIndexBuffer(indexCount, 8u, 101u);
    uploadSurroundingIndices(m_cornerIndexBuffer, indices);
    m_cornerIndexCount = vc71::vectorSizeIn(indices, 4u);

    if (indices.first)  ::renddx9abi::engineFree(indices.first);
    if (vertices.first) ::renddx9abi::engineFree(vertices.first);
}

// The two upload loops 0x100DEAB0 runs twice: x and z as shorts (SHORT2), then u and v as halves
// (+0x3A8 set) or floats; indices as the low halves of the u32 records.
void Terrain::uploadSurroundingVertices(void* vertexBuffer, const vc71::vector_layout& vertices)
{
    unsigned char* dst = static_cast<unsigned char*>(dx9rend::vertexBufferLock(vertexBuffer, 0, 0, 0));
    if (dst)
    {
        const unsigned int count = vc71::vectorSizeIn(vertices, 16u);
        const float* src = reinterpret_cast<const float*>(vertices.first);
        for (unsigned int i = 0; i < count; ++i, src += 4)
        {
            reinterpret_cast<short*>(dst)[0] = static_cast<short>(static_cast<long long>(src[0]));
            reinterpret_cast<short*>(dst)[1] = static_cast<short>(static_cast<long long>(src[1]));
            if (m_halfFloatSurrounding)
            {
                float u = src[2];
                float v = src[3];
                D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(dst + 4), &u, 1);
                D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(dst + 6), &v, 1);
                dst += 8;
            }
            else
            {
                reinterpret_cast<float*>(dst)[1] = src[2];
                reinterpret_cast<float*>(dst)[2] = src[3];
                dst += 12;
            }
        }
    }
    dx9rend::vertexBufferUnlock(vertexBuffer);
}

void Terrain::uploadSurroundingIndices(void* indexBuffer, const vc71::vector_layout& indices)
{
    unsigned short* const dst = static_cast<unsigned short*>(dx9rend::indexBufferLock(indexBuffer, 0, 0, 0));
    if (dst)
    {
        const unsigned int count = vc71::vectorSizeIn(indices, 4u);
        const unsigned short* src = reinterpret_cast<const unsigned short*>(indices.first);
        for (unsigned int i = 0; i < count; ++i)
            dst[i] = src[2 * i];
    }
    dx9rend::indexBufferUnlock(indexBuffer);
}


// (slot +0xD4) - the water setup load() closes with: the water Ra shaders by quality,
// the heightmap bounds into the uvLevel constants, the water volume texture or frame list, the
// "Water"/"WaterBase" shader into the material block, the level's water envmap.

void Terrain::setupWater()
{
    if (!g_waterQualityLatched)
        m_waterQuality = dx9rend::rendererGetVideoOptions()->getQuality(0);

    switch (m_waterQuality)
    {
    case 0:
    case 1:
        m_water3D = 0;
        m_waterTechnique            = raShaderNamed("Water2D");
        m_waterDistantTechnique     = raShaderNamed("WaterDistant2D");
        m_waterDistantTechnique2    = raShaderNamed("WaterDistant2D");
        m_waterSurroundingTechnique = raShaderNamed("WaterSurrounding2D");
        break;
    case 2:
        m_water3D = 1;
        m_waterTechnique            = raShaderNamed("Water3D");
        m_waterDistantTechnique     = raShaderNamed("WaterDistant3D");
        m_waterDistantTechnique2    = raShaderNamed("WaterDistant3D");
        m_waterSurroundingTechnique = raShaderNamed("WaterSurrounding3D");
        break;
    case 3:
    case 4:
        m_water3D = 1;
        m_waterTechnique            = raShaderNamed("WaterHighEnd3D");
        m_waterDistantTechnique     = raShaderNamed("WaterDistant3D");
        m_waterDistantTechnique2    = raShaderNamed("WaterDistant3D");
        m_waterSurroundingTechnique = raShaderNamed("WaterSurrounding3D");
        break;
    default:
        break;
    }

    // The heightmap's world bounds into the uvLevel holders: -lowX, 1/(highX-lowX), lowZ,
    // 1/(highZ-lowZ).
    ::dice::hfe::world::IHeightmap* const heightmap = heightmapCluster()->getPrimaryHeightmap();
    const float lowX  = heightmap->getLowBoundaryX();
    const float highX = heightmap->getHighBoundaryX();
    const float lowZ  = heightmap->getLowBoundaryZ();
    const float highZ = heightmap->getHighBoundaryZ();
    dx9rend::raShaderConstantsSetUvLevel(
        -lowX, static_cast<float>(1.0 / (highX - lowX)), lowZ, static_cast<float>(1.0 / (highZ - lowZ)));

    // The material block's +0x24 vec4 to (1, 0, 0, 1).
    float* const materialVec = reinterpret_cast<float*>(static_cast<unsigned char*>(m_materialParameters) + 36);
    materialVec[0] = 1.0f; materialVec[1] = 0.0f; materialVec[2] = 0.0f; materialVec[3] = 1.0f;

    if (m_water3D)
    {
        if (!dx9rend::raShaderConstantsGetEngineTexture(0x1A0))               // the WaterMap holder
        {
            vc71::string name("common/textures/");
            name += m_waterVolumeName;
            dx9rend::EngineTexture* const volume =
                static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(name));
            dx9rend::raShaderConstantsSetEngineTexture(0x1A0, volume);          // sub_10031180 on the holder
        }
    }
    else
    {
        if (!m_waterTextureList)
        {
            // the 2D water's animated normal-map frames, listed in
            // "common/textures/<volume>.txt" (stock: watervolume.txt, eight Waterbump frames).
            // The member is stored before the name is built (0x100DA61E), as shipped.
            void* const storage = ::renddx9abi::engineAlloc(0x48);
            m_waterTextureList = storage ? new (storage) TerrainWaterFrameList() : 0;
            // `std::string("common/textures/") + m_waterVolumeName + ".txt"`: the two inlined
            // operator+ (0x100DA632..0x100DA67E; copy, +=, copy) - the carrier has no operator+.
            vc71::string name("common/textures/");
            name += m_waterVolumeName;
            name += ".txt";
            // The list is not null-tested after the allocation (0x100DA687 reloads the member
            //   and calls), and load()'s answer is dropped: a missing or malformed file leaves
            //   the list with no frames, which the 2D samplers do not survive (TerrainDraw.cpp).
            m_waterTextureList->load(name);
        }
    }

    // "Water" on the expansion pack (Renderer +0xAC), else "WaterBase", into the material's +0x14C.
    {
        vc71::string shaderName(dx9rend::rendererIsExpansionPack() ? "Water" : "WaterBase");
        *reinterpret_cast<dx9rend::Ra::RaShader**>(static_cast<unsigned char*>(m_materialParameters) + 332) =
            dx9rend::Ra::getShader(raShaderTable(), shaderName, false);
    }

    // <GSLevelDirectory><GSLoadLevel> - the level directory the envmap sits under.
    vc71::string loadLevel;
    vc71::string levelDir;
    dx9rend::readStringSetting("GSLoadLevel", loadLevel);
    dx9rend::readStringSetting("GSLevelDirectory", levelDir);
    vc71::string levelPath(levelDir.c_str());
    levelPath += loadLevel;

    if (!m_waterEnvMap)
    {
        void* const storage = ::renddx9abi::engineAlloc(0x14);
        TerrainEnvMapRecord* record = 0;
        if (storage)
        {
            record = static_cast<TerrainEnvMapRecord*>(storage);
            record->unresolved00 = 0; record->unresolved04 = 0; record->unresolved08 = 0;
            record->isDefaultEnvMap = 0; record->texture = 0;
        }
        m_waterEnvMap = record;
        record->isDefaultEnvMap = 0;                                              // unguarded, as shipped
        vc71::string envmapDds(levelPath.c_str()); envmapDds += "/Water/envmap.dds";
        if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(envmapDds)))
        {
            vc71::string envmap(levelPath.c_str()); envmap += "/Water/envmap";
            m_waterEnvMap->texture = dx9rend::textureManagerGetTexture(envmap);
        }
        else
        {
            vc71::string envmap0Dds(levelPath.c_str()); envmap0Dds += "/Envmaps/EnvMap0.dds";
            if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(envmap0Dds)))
            {
                vc71::string envmap0(levelPath.c_str()); envmap0 += "/Envmaps/EnvMap0";
                m_waterEnvMap->texture = dx9rend::textureManagerGetTexture(envmap0);
                m_waterEnvMap->isDefaultEnvMap = 1;
            }
            else
            {
                m_waterEnvMap->texture = 0;
            }
        }
        if (!m_waterEnvMap->texture)
        {
            ::renddx9abi::engineFree(m_waterEnvMap);
            m_waterEnvMap = 0;
        }
        // The cube map: the level's envmap, or LightManager's at +0x248 (the cube its constructor
        // builds; XpLightManager.cpp writes that field and publishes no getter) [UNRESOLVED member].
        dx9rend::EngineTexture* const cube = m_waterEnvMap
            ? static_cast<dx9rend::EngineTexture*>(m_waterEnvMap->texture)
            : *reinterpret_cast<dx9rend::EngineTexture**>(reinterpret_cast<unsigned char*>(g_LightManager) + 0x248);
        holderSetTexture(static_cast<unsigned char*>(m_materialParameters) + 0x11C, cube);   // CubeMap
        if (g_terrainWaterUsesRendererTexture)
        {
            // Dead in the shipped image (nothing writes dword_102592A0): Renderer +0x7B4 into the
            // material's DiffuseMap holder. [UNRESOLVED] both the global and the renderer field.
            holderSetTexture(static_cast<unsigned char*>(m_materialParameters) + 0xB8,
                             *reinterpret_cast<dx9rend::EngineTexture**>(
                                 static_cast<unsigned char*>(dx9rend::g_rendererBase) + 0x7B4));
        }
    }
}


// Initializes an empty uniformly timed water animation; frame slots remain uninitialized.
TerrainWaterFrameList::TerrainWaterFrameList()
    : frameCount(0),
      phase(0.0f)
{
}


// Loads water-frame textures, ignoring the interleaved duration tokens.
// Returns false if the file cannot be opened, leaving existing frames unchanged.
// Requires an initialized texture manager and at most 16 frames; storage is unchecked.

bool TerrainWaterFrameList::load(vc71::string path)
{
    Tokenizer tokenizer;
    if (!tokenizer.init(path.c_str()))
        return false;                                                          // sub_100768E0, then the argument

    frameCount = 0;
    Token token = tokenizer.token(0);                                          // the first token, copied
    while (token != tokenizer.token(tokenizer.count()))                        // line and column only
    {
        vc71::string name;
        dx9rend::pathStripExtension(name, vc71::string(token.text.c_str()));
        frames[frameCount] = static_cast<dx9rend::EngineTexture*>(
            dx9rend::textureManagerGetTexture(name));
        ++frameCount;                                                          // unbounded
        ++token;
        ::atoi(token.text.c_str());                                            // parsed and dropped
        ++token;
    }
    return true;
}


// Returns the frame at truncated frameCount * phase. Requires frames and phase in [0, 1);
// sibling samplers also assume a positive count. No bounds or zero-count checks are made.
dx9rend::EngineTexture* TerrainWaterFrameList::currentFrame() const
{
    const float scaled = static_cast<float>(frameCount) * phase;               // fild, fmul
    return frames[static_cast<int>(scaled)];
}

// sub_10014EF0, 58 B. The count is read once (0x10014EF7) and is both the fild source and the
// divisor; `cdq / idiv` - a signed remainder.
dx9rend::EngineTexture* TerrainWaterFrameList::nextFrame() const
{
    const int count = frameCount;
    const float scaled = static_cast<float>(count) * phase;                    // fild, fmul
    return frames[(static_cast<int>(scaled) + 1) % count];
}

// sub_10014F30, 49 B. `fld st(0)` keeps the product, the truncation is fisub'd from it (one more
// float rounding - exact here, the difference of a float and its own truncation); the answer
// leaves in st(0) and the caller stores it with its `fstp dword` (0x100DBC38).
float TerrainWaterFrameList::frameBlend() const
{
    const float scaled = static_cast<float>(frameCount) * phase;               // fild, fmul
    return scaled - static_cast<float>(static_cast<int>(scaled));              // fistp (chop), fisub
}


// Terrain teardown releases owner references; manager caches retain their own references.
// Water volume and animation textures are evicted first, and detail-layer index buffers
// are also removed from BufferManager tracking.

namespace {

// dword_1023DDBC + 0x1A4 - the texture word of the Ra constants' WaterMap holder (+0x1A0), which
// setupWater's 3D arm fills with the water volume. Re-read at every use, as the shipped body
// re-reads the global.
inline dx9rend::EngineTexture*& raWaterMapTexture()
{
    return *reinterpret_cast<dx9rend::EngineTexture**>(
        static_cast<unsigned char*>(dx9rend::g_raShaderConstants) + 0x1A4);
}

// A parameter handle's release: vslot +0x08 of the ShaderParameter init() took a reference on
// (effectGetShaderParameter adds one for every caller).
inline void releaseParameter(void* handle)
{
    if (handle)
        static_cast<dx9rend::ShaderParameter*>(handle)->release();
}

// The _Tidy every VC7.1 vector member gets from its destructor - and from vector::clear(), which
// IS _Tidy in VC7.1: free the storage when there is any, then zero the three words.
inline void tidyVector(vc71::vector_layout& v)
{
    if (v.first)
        ::renddx9abi::engineFree(v.first);
    v.first = 0;
    v.last  = 0;
    v.end   = 0;
}

// sub_100E1800 - VC7.1 _Tree::_Erase over the 0x18-byte nodes of the maps at +0x2B0: the right
// subtree (recursively), then the left child - read before the node is freed - becomes the next
// node. The 4-byte values need no destructor. _Isnil is the byte at +0x15.
void eraseMapSubtree(unsigned char* node)
{
    while (node[0x15] == 0)
    {
        eraseMapSubtree(*reinterpret_cast<unsigned char**>(node + 8));         // _Right
        unsigned char* const left = *reinterpret_cast<unsigned char**>(node);  // _Left
        ::renddx9abi::engineFree(node);
        node = left;
    }
}

// The inlined ~map (0x100DDF28..0x100DDF51): `erase(begin(), end())` - sub_100E1D10, called with
// _Myhead->_Left and _Myhead, so it takes its whole-range arm: _Erase(root), the head relinked and
// the size zeroed - then the head freed and both words zeroed. The iterator it answers is dropped.
void destroyTerrainMap(vc71::map_layout& m)
{
    unsigned char* const head = reinterpret_cast<unsigned char*>(m.head);
    unsigned char** const links = reinterpret_cast<unsigned char**>(head);
    eraseMapSubtree(links[1]);                                                  // _Erase(_Myhead->_Parent)
    links[1] = head;                                                            // _Parent   0x100E1D35
    m.size   = 0;
    links[0] = head;                                                            // _Left     0x100E1D42
    links[2] = head;                                                            // _Right    0x100E1D47
    ::renddx9abi::engineFree(m.head);                                           // 0x100DDF41, no null test
    m.head = 0;
    m.size = 0;
}

} // anonymous namespace

void terrainReleaseTexture(void* texture)
{
    dx9rend::EngineTexture* const t = static_cast<dx9rend::EngineTexture*>(texture);
    if (t && !--t->refCount)
    {
        dx9rend::textureDtor(t);
        ::renddx9abi::engineFree(t);
    }
}


// sub_10014F70 - every frame out of the TextureManager's cache (sub_100191C0 on dword_1023B974),
// then the list's own reference through the re-read slot; the count zeroed on both paths.
// Neither the eviction nor the release tests the frame for null. The bound is re-read and
//   compared signed (`jle` before the loop, `jl` at its foot). (The tree's textureManagerEvictFromCache
//   itself skips a null frame; the release right after it dereferences the frame in both images, so a
//   null still faults at the same step.)

void TerrainWaterFrameList::destroy()
{
    for (int i = 0; i < frameCount; ++i)
    {
        dx9rend::textureManagerEvictFromCache(frames[i]);
        dx9rend::EngineTexture* const frame = frames[i];
        if (!--frame->refCount)
        {
            dx9rend::textureDtor(frame);
            ::renddx9abi::engineFree(frame);
        }
    }
    frameCount = 0;
}


// Releases water resources, evicting volume/frames before their owner references.
// The volume slot owns two references, released separately before it is cleared.

void Terrain::releaseWater()
{
    dx9rend::EngineTexture* const volume = raWaterMapTexture();
    if (volume)
    {
        dx9rend::textureManagerEvictFromCache(volume);
        dx9rend::EngineTexture* const again = raWaterMapTexture();              // re-read, unguarded
        if (!--again->refCount)
        {
            dx9rend::textureDtor(again);
            ::renddx9abi::engineFree(again);
        }
    }
    {
        dx9rend::EngineTexture*& slot = raWaterMapTexture();
        dx9rend::EngineTexture* const held = slot;
        if (held)
        {
            if (!--held->refCount)
            {
                dx9rend::textureDtor(held);
                ::renddx9abi::engineFree(held);
            }
            slot = 0;
        }
    }

    TerrainWaterFrameList* const frames = m_waterTextureList;
    if (frames)
    {
        frames->destroy();
        ::renddx9abi::engineFree(frames);
    }
    m_waterTextureList = 0;

    if (m_waterEnvMap)
    {
        terrainReleaseTexture(m_waterEnvMap->texture);
        ::renddx9abi::engineFree(m_waterEnvMap);                                // re-read at 0x100DA1EC
        m_waterEnvMap = 0;
    }
}


// sub_100D8890 (thiscall, recursive) - the destructor's pre-0x1A arm. The pre-0x1A LOD records are
// separate 400-byte blocks linked through +0x180..+0x18C (their builder, sub_100D87B0, is a
// [FRONTIER] of load()): the four children's subtrees first when the first link is set - only
// that one is tested; each recursion tests its own argument - then the record itself. Dead on 1.50
// content, whose flat 85-record array takes the array free instead.

void Terrain::freeLodTableTree(unsigned char* table)
{
    if (!table)
        return;
    unsigned char* const* const children = reinterpret_cast<unsigned char* const*>(table + 0x180);
    if (children[0])
    {
        freeLodTableTree(children[0]);
        freeLodTableTree(children[1]);
        freeLodTableTree(children[2]);
        freeLodTableTree(children[3]);
    }
    ::renddx9abi::engineFree(table);
}


// (slot +0xB0) - the scalar deleting destructor: the body, the free when bit 0 of the
// flags is set (`test byte ptr [ebp+8], 1`), and `this` answered. release() is its only caller.

void* Terrain::deletingDestructor(int flags)
{
    this->~Terrain();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
    return this;
}


// sub_100DD6C0, 2,247 B - the destructor body. On entry the compiler has re-pointed the three vptrs
// at this class's tables (0x100DD6C9..0x100DD6D6: 0x1021A198 / 0x1021A188 / 0x1021A170), so the
// listener notifications the unregistrations send land on Terrain's own bodies. Only the render-path
// removal sends one (+0x04 = 0x100D8660, and only when the list's +0x10 byte is set); the device
// listener's removal (0x10002410) sends none.
// The explicit body runs to 0x100DDE50; from 0x100DDE53 on is the compiler's member teardown.

Terrain::~Terrain()
{
    // (1) The water resources - slot +0xDC, called directly (call 0x100DA120).
    Terrain::releaseWater();

    // Destroy the water-parameter array in reverse order, then free its allocation header.
    // The member remains unchanged.
    if (m_waterObjectRing)
    {
        unsigned char* const block = m_waterObjectRing - 4;
        const int count = *reinterpret_cast<const int*>(block);
        for (int i = count - 1; i >= 0; --i)                                    // `sub eax,1 / js` - signed
            dx9rend::Ra::raObjectParametersDestroy(m_waterObjectRing + 548 * i);
        ::renddx9abi::engineFree(block);
    }

    // (3) The water grid's stream material, then its construction slot: vslot 0 (0x10049E50, the
    //     deleting destructor) with 1. Neither member is nulled.
    if (m_waterGridGeometry)
        m_waterGridGeometry->destroy(1);
    if (m_waterGridGeometryTemp)
        m_waterGridGeometryTemp->destroy(1);

    // (4) The constructor's Ra material block: sub_10043F00 (which drops the CubeMap/DiffuseMap
    //     holders' references setupWater took), then the scalar free. Not nulled.
    if (m_materialParameters)
    {
        void* const material = m_materialParameters;
        dx9rend::Ra::raMaterialParametersDestroy(material);
        ::renddx9abi::engineFree(material);
    }

    // (5) The culler: its destructor (0x100E6800), the free, and the member nulled (inside the
    //     test, 0x100DD784). The explicit body's only other stores are the detail vector's clear
    //     (9), the ring slots (10) and the four shadow-handle globals (13) - plus releaseWater's.
    if (m_culler)
    {
        TerrainCuller* const culler = m_culler;
        culler->~TerrainCuller();
        ::renddx9abi::engineFree(culler);
        m_culler = 0;
    }

    // (6) The LOD tables: the flat 85-record array of 0x1A content (array free), else the pre-0x1A
    //     record tree (sub_100D8890). +0x468 is written only by load() - a terrain destroyed
    //     unloaded takes either arm on heap garbage; both are harmless on the constructor's null.
    if (m_lodTablesFromFile)
        ::renddx9abi::engineFree(m_lodTables);
    else
        freeLodTableTree(m_lodTables);

    // (7) The listeners, render path first (this + 8, 0x10009DC0 -> 0x1000D020), then device-lost
    //     (this + 4, 0x10009D70 -> 0x10002410) - the reverse of the constructor's registration.
    dx9rend::unregisterRenderPathListener(static_cast<dx9rend::RenderPathListener*>(this));
    dx9rend::unregisterDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this));

    // (8) The patch grid: each non-null patch through vslot 0 with 1 (0x1018E520), the bound
    //     patchCount^2 re-evaluated every pass; then the array free. m_patches is not nulled.
    if (m_patches)
    {
        for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        {
            QuadPatch* const patch = m_patches[i];
            if (patch)
            {
                patch->~QuadPatch();                                            // vslot 0: 0x1018E520
                ::renddx9abi::engineFree(patch);
            }
        }
        ::renddx9abi::engineFree(m_patches);
    }

    // (9) The detail-texture records - only when the vector holds any (VC7.1 empty(): a null
    //     first or a zero size skips the loop AND the clear), then records [0, detailTextureCount)
    //     through vslot 0 with 1 (0x100E1730), then vector::clear() - _Tidy in VC7.1.
    //     The bound is the file's count (+0x320), re-read every pass, not the vector's size: a
    //     high-quality reloadForLowQuality appends a second set whose records are never deleted.
    if (vc71::vectorSizeIn(m_detailTextures, 4u) != 0)
    {
        for (int i = 0; i < m_detailTextureCount; ++i)
        {
            TerrainDetailTexture* const record =
                reinterpret_cast<TerrainDetailTexture**>(m_detailTextures.first)[i];
            if (record)
            {
                record->~TerrainDetailTexture();                                // vslot 0: 0x100E1730
                ::renddx9abi::engineFree(record);
            }
        }
        tidyVector(m_detailTextures);
    }

    // (10) The eight ring patches, each through vslot 0 with 1 (0x1018F870) and its slot nulled.
    for (int s = 0; s < 8; ++s)
    {
        SurroundingPatch* const ring = m_surrounding[s];
        if (ring)
        {
            ring->~SurroundingPatch();
            ::renddx9abi::engineFree(ring);
            m_surrounding[s] = 0;
        }
    }

    // (11) The level-wide low-detail texture - the owner's reference only. Not nulled.
    terrainReleaseTexture(m_lowDetailTexture);

    // (12) Buffers and declarations, owner references only, in the shipped order (0x100DD8D6..
    //      0x100DDA54). None is nulled; each release is the null test plus the inline dance.
    if (m_lodIndexBuffer)             dx9rend::indexBufferRelease(m_lodIndexBuffer);                 // +0xD8
    if (m_baseGridVertexBuffer)       dx9rend::vertexBufferRelease(m_baseGridVertexBuffer);          // +0xD0
    if (m_surroundingDeclaration)     dx9rend::vertexDeclarationRelease(m_surroundingDeclaration);   // +0x2A4
    if (m_surroundingFullDeclaration) dx9rend::vertexDeclarationRelease(m_surroundingFullDeclaration); // +0x2A8
    if (m_vertexDeclaration)          dx9rend::vertexDeclarationRelease(m_vertexDeclaration);        // +0x3B4
    if (m_gridDeclaration)            dx9rend::vertexDeclarationRelease(m_gridDeclaration);          // +0x3B0
    if (m_sideVertexBuffer)           dx9rend::vertexBufferRelease(m_sideVertexBuffer);              // +0xE0
    if (m_cornerVertexBuffer)         dx9rend::vertexBufferRelease(m_cornerVertexBuffer);            // +0xE4
    if (m_sideIndexBuffer)            dx9rend::indexBufferRelease(m_sideIndexBuffer);                // +0xE8
    if (m_cornerIndexBuffer)          dx9rend::indexBufferRelease(m_cornerIndexBuffer);              // +0xEC
    if (m_sideVertexBuffer2)          dx9rend::vertexBufferRelease(m_sideVertexBuffer2);             // +0xF0

    // (13) The parameter handles init() took, in the shipped order (0x100DDA57..0x100DDD9A). It is
    //      not init()'s lookup order: shadowmap / multipleshadowmap precede TEXLAYER0..6 in both,
    //      but WaterHeight / CameraToWater / TerrainWaterColor run in field order here and
    //      NightTimeMode / NightOccludedSkyIntensity follow WaterCycleTime. +0x224 and +0x27C are
    //      never written and never released. A terrain destroyed before init() would release
    //      the handles the constructor leaves uncleared (+0x26C, +0x270, +0x280..+0x288,
    //      +0x290..+0x2A0) - heap garbage.
    releaseParameter(m_hScaleBaseUV);                    // +0x1B4
    releaseParameter(m_hScaleTransXZ);                   // +0x1B8
    releaseParameter(m_hScaleTransY);                    // +0x1BC
    releaseParameter(m_hStScaleTransY);                  // +0x1C0
    releaseParameter(m_hTexScale);                       // +0x1C4
    releaseParameter(m_hYPlaneTexScaleAndFarTile);       // +0x1C8
    releaseParameter(m_hStTexScale);                     // +0x1CC
    releaseParameter(m_hMatViewProj);                    // +0x1D0
    releaseParameter(m_hMatView);                        // +0x1D4
    releaseParameter(m_hShadowMap);                      // +0x1F4
    releaseParameter(m_hMultipleShadowMap);              // +0x1F8
    for (int i = 0; i < 7; ++i)
        releaseParameter(m_hTexLayer[i]);                // +0x1D8..+0x1F0 (unrolled in the image)
    releaseParameter(m_hComponentSelector);              // +0x1FC
    releaseParameter(m_hTexProjOffset);                  // +0x200
    releaseParameter(m_hTexProjScale);                   // +0x204
    releaseParameter(m_hCameraPos);                      // +0x208
    releaseParameter(m_hSunColor);                       // +0x20C
    releaseParameter(m_hGIColor);                        // +0x210
    releaseParameter(m_hAmbientColor);                   // +0x214
    releaseParameter(m_hSinglePointColor1X);             // +0x218
    releaseParameter(m_hDynamicPointColor);              // +0x21C
    releaseParameter(m_hFogRange);                       // +0x220
    releaseParameter(m_hFogColor);                       // +0x228
    releaseParameter(m_hAmbientIlluminationRange);       // +0x22C
    releaseParameter(m_hAmbientIlluminationColor);       // +0x230
    releaseParameter(m_hWaterFogRange);                  // +0x234
    releaseParameter(m_hWaterFogColor);                  // +0x238
    releaseParameter(m_hMorphDeltaSelector);             // +0x23C
    releaseParameter(m_hMorphDeltaAdder);                // +0x240
    releaseParameter(m_hNearFarMorphLimits);             // +0x244
    releaseParameter(m_hFarTexTiling);                   // +0x248
    releaseParameter(m_hStFarTexTiling);                 // +0x24C
    releaseParameter(m_hNearTexTiling);                  // +0x250
    releaseParameter(m_hLightViewProj);                  // +0x254
    releaseParameter(m_hLightViewProjOrtho);             // +0x258
    releaseParameter(m_hViewportMap);                    // +0x25C
    releaseParameter(m_hStTransXZ);                      // +0x260
    releaseParameter(m_hPointLight);                     // +0x264
    releaseParameter(m_hSpotLight);                      // +0x268
    releaseParameter(m_hWaterHeight);                    // +0x280
    releaseParameter(m_hCameraToWater);                  // +0x284
    releaseParameter(m_hTerrainWaterColor);              // +0x288
    releaseParameter(m_hColorLightTex);                  // +0x28C
    releaseParameter(m_hCausticsMap);                    // +0x290
    releaseParameter(m_hCausticsMapFrame0);              // +0x294
    releaseParameter(m_hCausticsMapFrame1);              // +0x298
    releaseParameter(m_hWaterScroll);                    // +0x29C
    releaseParameter(m_hWaterCycleTime);                 // +0x2A0
    releaseParameter(m_hNightTimeMode);                  // +0x26C
    releaseParameter(m_hNightOccludedSkyIntensity);      // +0x270
    // The four shadow-array handles init() publishes as file globals (dword_10258F60..6C): each
    // released and then cleared, so the next terrain's init() looks them up afresh.
    releaseParameter(g_terrainShadowViewProjArray);  g_terrainShadowViewProjArray = 0;
    releaseParameter(g_terrainShadowPosArray);       g_terrainShadowPosArray      = 0;
    releaseParameter(g_terrainShadowSizeArray);      g_terrainShadowSizeArray     = 0;
    releaseParameter(g_terrainShadowCount);          g_terrainShadowCount         = 0;
    releaseParameter(m_hLightPos1);                      // +0x274
    releaseParameter(m_hLightAttSqrInv1);                // +0x278

    // (14) The effect - its vslot +0x08 - and the dynamic shadow-range index buffer load() made.
    if (m_effect)
        dx9rend::effectRelease(static_cast<dx9rend::Effect*>(m_effect));
    if (m_indexBuffer)
        dx9rend::indexBufferRelease(m_indexBuffer);                             // +0x368

    // the members' own destructors (0x100DDE53..0x100DDF7D), in reverse declaration order
    // +0x740 m_levelPath and +0x6D0 m_waterVolumeName: vc71::string - see the at the end.
    ::renddx9abi::engineFree(m_waterRecord);                                    // +0x710: delete, no null test
    dx9rend::Ra::raObjectParametersDestroy(m_objectParameters);                 // +0x49C: sub_1004D900
    tidyVector(m_lodBlockIndices);                                              // +0x470
    tidyVector(m_shadowPatches);                                                // +0x448
    tidyVector(m_shadowRanges);                                                 // +0x36C
    tidyVector(m_detailTextures);                                               // +0x338 (already clear when (9) ran)
    destroyTerrainMap(m_map2C8);                                                // +0x2C8
    destroyTerrainMap(m_map2BC);                                                // +0x2BC
    destroyTerrainMap(m_map2B0);                                                // +0x2B0
    // The six vc71::string members are destroyed by the compiler after this body, in reverse
    // declaration order: +0x740, +0x6D0, +0xA8, +0x8C, +0x70, +0x54 - the shipped order among
    // themselves. The shipped teardown interleaves the first two with the lines above (+0x740,
    // +0x710, +0x6D0, +0x49C, ...); every block involved is a separate allocation and nothing runs
    // in between that could see the difference.
}


// The reflection factory - RefFactory_geom_Terrain 0x100B0544: 0x768 bytes on the engine heap,
// the constructor, then queryInterface(iid); a refused iid releases the object.

IBase* __fastcall createTerrain(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(0x768);
    Terrain* const self = storage ? new (storage) Terrain() : 0;
    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
