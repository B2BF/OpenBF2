// Grass patch streaming and rendering.
// [CHOSEN, not recovered] split from UndergrowthSystem.cpp for file size.
// A patch cache builds nearby density cells, normally one per frame; forced updates build all.
// Rendering culls patches, sorts near-to-far and groups draws by lighting/shadow technique.
// Preserve float evaluation order and unordered-comparison behavior. Profiling brackets are omitted.
#ifdef BF2_IMAGE_RENDDX9

#include "UndergrowthSystem.h"

#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>
#include "Terrain.h"
#include "MeshTemplate.h"
#include "StaticMesh.h"
#include "XpLightManager.h"
// The host (BF2.exe) light interfaces render dispatches into - typed in both flavours (RW-266); the
// retail-ABI flavour takes HfeAbi's ClassManager carriers in place of Core/ClassManager.h, exactly as
// SkinnedMesh.cpp includes the same two headers.
#include "../Game/Objects/Visual/LightSource.h"
#include "../Game/Objects/Visual/LightSourceTemplate.h"
#include "../Math/Frustum.h"
#include "../Math/Mat4.h"
#include "../Math/Vec3.h"
#include "../Physics/HeightmapCluster.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderView.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <utility>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {

extern void* g_rendererBase;       // dword_1023B970 - the Renderer (RendDX9Init.cpp)
extern void* g_raShaderConstants;  // dword_1023DDBC - the Ra constants block (RenderQueues.cpp)
extern IBase* g_ObjectDrawer;      // dword_1025A39C - BF2Render.cpp publishes the drawer here

// BufferManager.cpp / StateManager.cpp, declared the way TerrainDraw.cpp and SkyDome.cpp declare them
// (RendDX9Base.h publishes the state-manager seams but not these).
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  indexBufferBind(void* wrapper);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);

} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;                                         // dword_1024B914

namespace {


// constants


// .data values with no writer anywhere in the image (every cross-reference ), kept as
// constants the way SkyDome.cpp keeps its never-written .data bytes. Both arms of every branch they
// select are transcribed.
const float kCellBoundsMargin      = 1.0f;     // flt_10239CF8 - readers: 0x10145AC0 only
const float kBuildReachFactor      = 1.1f;     // flt_10239D00 (0x3F8CCCCD) - reader: 0x10148D2E only
const bool  kPointSampleTerrainMaps = true;    // byte_10239CFC = 1 - reader: 0x10145FC2 only
// byte_1025E6C0 - .bss, so 0 at run time, and NO instruction in the image writes it (its two
// readers are 0x10149902 and 0x101499AD). A "freeze the culling camera" debug latch; always off.
const bool  kFreezeCulling         = false;

// .rdata
const float kPositionScale         = 131.068f;     // flt_1021DB44 = 0x43031168 (32767 / 250)
const float kHeightByteScale       = 85.0f;        // flt_1021DB48
const float kJitterUnit            = 0.0078125f;   // flt_1021DB4C (1 / 128)
const float kUniformUnit           = 0.00390625f;  // flt_1021DB50 (1 / 256)
const float kRotationStepDegrees   = 11.25f;       // flt_1021DB54 (360 / 32)
const float kSizeRandomUnit        = 0.015625f;    // flt_1021A14C (1 / 64)
const float kOffscreenPenalty      = 10000.0f;     // flt_1020D648
const float kAlphaRefScale         = 255.0f;       // flt_101E14C0
const float kTransparencyX8Scale   = 0.125f;       // flt_1021DD38
const float kPosOffsetScaleW       = 250.0f;       // 0x437A0000, pushed as an immediate
const unsigned int kBuildVertexBudget = 999999999u; // see buildPatch's exit


// Render statics retain the function-local initialization guards.


// dword_1025E6C4 / C8 / CC - the camera position render samples each frame - and immediately after it
// dword_1025E6D0, the guard word (bit 1: this Vec3, zeroed once; bit 2: the frustum below). They are
// ONE struct on purpose: render uploads the position with CameraPos->setValue(&position, 0), and a
// vector parameter's setValue is ID3DXEffect::SetVector, which reads a whole float4 - so in retail the
// parameter's w IS the guard word. Keeping the adjacency keeps those sixteen bytes identical.
struct RenderStatics
{
    Vec3         cameraPosition;   // dword_1025E6C4..CC
    unsigned int onceFlags;        // dword_1025E6D0
};
RenderStatics g_renderStatics = { { 0.0f, 0.0f, 0.0f }, 0u };

// dword_1025E4D8 - the 0x1E8-byte culling frustum, constructed on first use under guard bit 2 with
// sub_10030F30 (Frustum::Frustum) and NO atexit (Frustum has no destructor). Placement storage so the
// construction happens where retail's does.
union CullFrustumStorage
{
    double        align;
    unsigned char bytes[sizeof(Frustum)];
};
CullFrustumStorage g_cullFrustumStorage;

inline Frustum& cullFrustum()
{
    return *reinterpret_cast<Frustum*>(g_cullFrustumStorage.bytes);
}


// Technique names are sorted by pointer when drawing.
// [CHOSEN, not recovered] One aggregate preserves their relative address order and spacing.

struct TechniqueNames
{
    char t0_l4_ds_simple[16];
    char t0_l3_ds_simple[16];
    char t0_l2_ds_simple[16];
    char t0_l1_ds_simple[16];
    char t0_l0_ds_simple[16];
    char t0_l4_simple[16];
    char t0_l3_simple[16];
    char t0_l2_simple[16];
    char t0_l1_simple[16];
    char t0_l0_simple[16];
    char t0_l4_ds[12];
    char t0_l3_ds[12];
    char t0_l2_ds[12];
    char t0_l1_ds[12];
    char t0_l0_ds[12];
    char t0_l4[8];
    char t0_l3[8];
    char t0_l2[8];
    char t0_l1[8];
    char t0_l0[8];
};

const TechniqueNames kTechniqueNames =
{
    "t0_l4_ds_simple", "t0_l3_ds_simple", "t0_l2_ds_simple", "t0_l1_ds_simple", "t0_l0_ds_simple",
    "t0_l4_simple",    "t0_l3_simple",    "t0_l2_simple",    "t0_l1_simple",    "t0_l0_simple",
    "t0_l4_ds",        "t0_l3_ds",        "t0_l2_ds",        "t0_l1_ds",        "t0_l0_ds",
    "t0_l4",           "t0_l3",           "t0_l2",           "t0_l1",           "t0_l0"
};

const char* const kTechniques[4][5] =
{
    { kTechniqueNames.t0_l0, kTechniqueNames.t0_l1, kTechniqueNames.t0_l2,
      kTechniqueNames.t0_l3, kTechniqueNames.t0_l4 },
    { kTechniqueNames.t0_l0_ds, kTechniqueNames.t0_l1_ds, kTechniqueNames.t0_l2_ds,
      kTechniqueNames.t0_l3_ds, kTechniqueNames.t0_l4_ds },
    { kTechniqueNames.t0_l0_simple, kTechniqueNames.t0_l1_simple, kTechniqueNames.t0_l2_simple,
      kTechniqueNames.t0_l3_simple, kTechniqueNames.t0_l4_simple },
    { kTechniqueNames.t0_l0_ds_simple, kTechniqueNames.t0_l1_ds_simple,
      kTechniqueNames.t0_l2_ds_simple, kTechniqueNames.t0_l3_ds_simple,
      kTechniqueNames.t0_l4_ds_simple }
};


// Per-draw entry in the local multimap. Neither this record nor its lights vector crosses the ABI.

struct UndergrowthDrawItem
{
    UndergrowthCell*                  cell;          // node +0x10
    std::vector<world::LightSource*>  lights;        // node +0x14 (a 16-byte VC7.1 vector in retail)
    unsigned int                      lightCount;    // node +0x24  min(lights.size(), 4)
    MeshBounds                        bounds;        // node +0x28
    const char*                       technique;     // node +0x64  the same pointer as the key
    bool                              shadowed;      // node +0x68
    bool                              zPrepassed;    // node +0x69
};

typedef std::multimap<const char*, UndergrowthDrawItem> UndergrowthDrawList;

// The comparator render and rebuildPatches hand std::sort: a one-byte functor taking the elements BY
// value (the insertion sort 0x1014F290 copies both 8-byte records before its `fcomp`), ascending key.
struct UndergrowthCellDistanceLess
{
    bool operator()(UndergrowthCellDistance a, UndergrowthCellDistance b) const
    {
        return a.key < b.key;
    }
};


// UndergrowthVertex::texCoord - two shorts - as the ONE dword buildPatch copies into the patch vertex
// (`mov ecx, [eax+0Ch]; mov [edx+8], ecx`).
inline unsigned int packedTexCoord(const UndergrowthVertex& v)
{
    unsigned int bits;
    std::memcpy(&bits, v.texCoord, sizeof(bits));
    return bits;
}

// sub_1000ED40 (__stdcall, retn 0Ch) - fld low; fld high; fld value; fcomi st,st1; fcmovnb st,st1;
// fcomi st,st2; fcmovb st,st2. fcomi sets CF for "below" AND for unordered, so a NaN survives the
// high clamp and is replaced BY `low` by the second (SkyDome.cpp's copy says a NaN passes through
// unchanged; the fcmovb says otherwise).
float clampFloat(float value, float low, float high)
{
    float result = value;
    if (result >= high)
        result = high;
    if (!(result >= low))
        result = low;
    return result;
}

// lcgrand_step, RendDX9's copy of dice::hfe::Random::getRaw: two Schrage steps, 24112
// then 26143, on the seed IN place, answering the new seed. The same arithmetic NametagManager.cpp's
// randomStep carries (file-local there, so reproduced rather than linked).
unsigned int lcgStep(unsigned int value, unsigned int multiplier)
{
    const unsigned int low = (value & 0xFFFFu) * multiplier;
    const unsigned int high = static_cast<unsigned int>(static_cast<int>(value) >> 16) * multiplier;
    const int middle = static_cast<int>((low >> 16) + high);
    const unsigned int next = (low & 0xFFFFu) + ((static_cast<unsigned int>(middle) & 0x7FFFu) << 16)
        + static_cast<unsigned int>(middle >> 15);
    const int reduced = static_cast<int>(next - 0x7FFFFFFFu);
    return reduced < 0 ? next : static_cast<unsigned int>(reduced);
}

unsigned int lcgNext(unsigned int& seed)
{
    seed = lcgStep(lcgStep(seed, 24112u), 26143u);
    return seed;
}

// (ecx = a, edx = b; the answer on the x87 stack) - |b - a|. The sum is (dx^2 + dz^2) +
// dy^2: the body squares z first and adds x, then y (faddp order), which associates like this.
float distanceBetween(const Vec3& a, const Vec3& b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    return std::sqrt(dx * dx + dz * dz + dy * dy);
}

// the same body without the fsqrt (outside the attested range: a Math helper).
float distanceSquaredBetween(const Vec3& a, const Vec3& b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    return dx * dx + dz * dz + dy * dy;
}

// (ecx = point, edx = origin, two stack arguments, retn 8) - the squared distance from a
// point to the parallelogram origin + s*edge1 + t*edge2, s and t each clamped to [0, 1] separately
// (exact for perpendicular edges, which is how rebuildPatches calls it: the top face of a cell box).

// Uses edge1's squared length for both axes; behavior differs for nonsquare cells.
float squaredDistanceToQuad(const Vec3& point, const Vec3& origin, const Vec3& edge1,
                            const Vec3& edge2)
{
    const float dx = origin.x - point.x;
    const float dy = origin.y - point.y;
    const float dz = origin.z - point.z;
    const float edge1LengthSq = edge1.x * edge1.x + edge1.y * edge1.y + edge1.z * edge1.z;
    const float edge2LengthSq = edge1LengthSq;                            // see above
    const float along1 = dz * edge1.z + dx * edge1.x + dy * edge1.y;
    const float along2 = dz * edge2.z + dy * edge2.y + dx * edge2.x;
    const float s = -along1;
    const float t = -along2;
    float distanceSq = dx * dx + dz * dz + dy * dy;
    if (!(s < 0.0f))                                                      // test ah,5 / jnp
    {
        if (s <= edge1LengthSq)                                           // test ah,41h / jp
            distanceSq = distanceSq + s / edge1LengthSq * along1;
        else
            distanceSq = distanceSq + (along1 + along1) + edge1LengthSq;
    }
    if (!(t < 0.0f))
    {
        if (t <= edge2LengthSq)
            distanceSq = distanceSq + t / edge2LengthSq * along2;
        else
            distanceSq = distanceSq + (along2 + along2) + edge2LengthSq;
    }
    return std::fabs(distanceSq);
}

// (retn 4) - scale the 3x3 part's columns by s; write order m0 m4 m8 m1 m5 m9 m2 m6 m10.
// ParticleRendererMesh.inl carries the same body file-locally (psmMeshScaleColumns).
void mat4ScaleColumns3x3(Mat4& m, const Vec3& s)
{
    m.m[0]  = s.x * m.m[0];
    m.m[4]  = m.m[4] * s.x;
    m.m[8]  = s.x * m.m[8];
    m.m[1]  = s.y * m.m[1];
    m.m[5]  = s.y * m.m[5];
    m.m[9]  = s.y * m.m[9];
    m.m[2]  = s.z * m.m[2];
    m.m[6]  = m.m[6] * s.z;
    m.m[10] = m.m[10] * s.z;
}

// sub_1000A6B0 on the Renderer - is multisampling active: the forced-on byte +0x514 wins, then the
// forced-off +0x515, else the first entry of the base's multisample list (+0xC0) must be > 0. The same
// file-local probe TerrainDraw.cpp and FSQuadDrawer.cpp carry.
bool isMultisampleActive()
{
    const dx9rend::RendDX9Base* const base =
        static_cast<const dx9rend::RendDX9Base*>(dx9rend::g_rendererBase);
    if (base->forceMultisample)
        return true;
    if (base->disableMultisample)
        return false;
    return dx9rend::multiSampleListActive(base);
}

// The Ra constants block (g_raShaderConstants) words, by their retail x86 byte offsets.
inline unsigned char* raConstantAt(unsigned int byteOffset)
{
    return dx9rend::raShaderConstantsAt(byteOffset);
}

inline float raConstantF(unsigned int byteOffset)
{
    return *reinterpret_cast<const float*>(raConstantAt(byteOffset));
}

// The "HeightmapCluster" singleton by name - rebuildPatches and buildPatch look it up on every call
// and neither guards the answer.
world::IHeightmapCluster* heightmapClusterByName()
{
    IBase* const found = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("HeightmapCluster"));
    return static_cast<world::IHeightmapCluster*>(static_cast<void*>(found));
}

// The zeroed box every caller builds before cellBounds (fifteen dword stores and the dirty byte;
// the three bytes after `dirty` are not written, as in retail).
void resetBounds(MeshBounds& bounds)
{
    bounds.min.x = 0.0f;  bounds.min.y = 0.0f;  bounds.min.z = 0.0f;
    bounds.max.x = 0.0f;  bounds.max.y = 0.0f;  bounds.max.z = 0.0f;
    bounds.originRadius = 0.0f;
    bounds.radius = 0.0f;
    bounds.dirty = 1;
    bounds.extent.x = 0.0f;  bounds.extent.y = 0.0f;  bounds.extent.z = 0.0f;
    bounds.center.x = 0.0f;  bounds.center.y = 0.0f;  bounds.center.z = 0.0f;
}

// RW-266: use typed light virtuals in both builds. The template field differs in native layout,
// so native builds use the accessor while plugin builds read the fixed retail offset.
const world::LightSourceTemplate* lightSourceTemplate(world::LightSource* light)
{
    return static_cast<const world::LightSourceTemplate*>(light->getTemplate());
}

// Queries drawer occlusion; dirty bounds may be finalized in place.
// Requires a live drawer and a compatibility-layout MeshBounds.
inline bool objectDrawerOccludesBounds(IBase* drawer, MeshBounds* bounds)
{
    typedef bool (__thiscall* OccludedFn)(void*, MeshBounds*);
    void** const table = *reinterpret_cast<void***>(drawer);
    return reinterpret_cast<OccludedFn>(table[0x3C / 4])(drawer, bounds);
}


// THE terrain-MAP sampler (simple-shader vertices only) - the colour and light maps are DXT1 textures
// read back through LockRect and decoded on the CPU.


// (ecx = block, edx = out, one stack argument = the out row stride in texels; retn 4) -
// decode one DXT1 block into sixteen D3DCOLORs. Three shipped quirks, preserved: the 5/6-bit
// channels are widened by a plain shift (no bit replication, so white is 0xF8FCF8); the 3-colour arm
// (c0 <= c1) leaves colour 3 opaque black (alpha 0xFF, not the specification's transparent black);
// and the interpolants are integer (2a + b) / 3 and (a + b) / 2 on the widened bytes.
void decodeDxt1Block(const unsigned char* block, unsigned int* out, unsigned int stride)
{
    unsigned char palette[4][4];                                          // [ebp-14h], B G R A
    palette[0][3] = 0xFF;
    palette[1][3] = 0xFF;
    palette[2][3] = 0xFF;
    palette[3][3] = 0xFF;
    unsigned short c0;
    unsigned short c1;
    std::memcpy(&c0, block, 2);
    std::memcpy(&c1, block + 2, 2);
    palette[0][0] = static_cast<unsigned char>(block[0] << 3);
    palette[1][2] = static_cast<unsigned char>((c1 >> 11) << 3);
    palette[1][1] = static_cast<unsigned char>((c1 >> 5) << 2);
    palette[0][2] = static_cast<unsigned char>((c0 >> 11) << 3);
    palette[0][1] = static_cast<unsigned char>((c0 >> 5) << 2);
    palette[1][0] = static_cast<unsigned char>(block[2] << 3);
    if (c0 > c1)                                                          // `cmp si, di; jbe`
    {
        palette[2][2] = static_cast<unsigned char>((palette[1][2] + palette[0][2] * 2) / 3);
        palette[2][1] = static_cast<unsigned char>((palette[1][1] + palette[0][1] * 2) / 3);
        palette[2][0] = static_cast<unsigned char>((palette[1][0] + palette[0][0] * 2) / 3);
        palette[3][2] = static_cast<unsigned char>((palette[0][2] + palette[1][2] * 2) / 3);
        palette[3][1] = static_cast<unsigned char>((palette[0][1] + palette[1][1] * 2) / 3);
        palette[3][0] = static_cast<unsigned char>((palette[0][0] + palette[1][0] * 2) / 3);
    }
    else
    {
        palette[2][2] = static_cast<unsigned char>((palette[1][2] + palette[0][2]) / 2);
        palette[2][1] = static_cast<unsigned char>((palette[1][1] + palette[0][1]) / 2);
        palette[2][0] = static_cast<unsigned char>((palette[1][0] + palette[0][0]) / 2);
        palette[3][2] = 0;
        palette[3][1] = 0;
        palette[3][0] = 0;
    }
    unsigned int colors[4];
    std::memcpy(colors, palette, sizeof(colors));
    unsigned int bits;
    std::memcpy(&bits, block + 4, 4);
    for (unsigned int texel = 0; texel < 16; ++texel)
        out[(texel >> 2) * stride + (texel & 3u)] = colors[(bits >> (2 * texel)) & 3u];
}

// (__cdecl) - out = a * w + b * (256 - w), per 8-bit channel, w from t's float bits.
// The weight is `(bits >> (16 + 127 - exponent)) & 0xFF` - it keeps the exponent's low bits where
// the implicit 1 belongs, so it is only roughly t * 256; t >= 1 answers a, t < 2^-8 (or t >= 2, or
// negative) answers b. And a is weighted by t, not (1 - t): callers get texel(x) * frac +
// texel(x + 1) * (1 - frac). All shipped; reproduced bit for bit.
void lerpColor(unsigned int* out, const unsigned int* a, const unsigned int* b, float t)
{
    unsigned int bits;
    std::memcpy(&bits, &t, 4);
    const unsigned int shift = 0x7Fu - ((bits >> 23) & 0xFFu);
    if (shift == 0)
    {
        *out = *a;
        return;
    }
    if (shift >= 8)
    {
        *out = *b;
        return;
    }
    const unsigned int weight = (bits >> (shift + 16)) & 0xFFu;
    const unsigned int inverse = 0x100u - weight;
    const unsigned int ca = *a;
    const unsigned int cb = *b;
    const unsigned int blue  = (cb & 0xFFu) * inverse + (ca & 0xFFu) * weight;
    const unsigned int green = (cb & 0xFF00u) * inverse + (ca & 0xFF00u) * weight;
    const unsigned int red   = (cb & 0xFF0000u) * inverse + (ca & 0xFF0000u) * weight;
    const unsigned int alpha = ((cb >> 8) & 0xFF0000u) * inverse + ((ca >> 8) & 0xFF0000u) * weight;
    *out = (((red & 0xFF000000u) | (green & 0xFF0000u) | (blue & 0xFF00u)) >> 8)
         | (alpha & 0xFF000000u);
}

// The address of block `index` of a DXT1 image, in retail's 32-bit arithmetic: the index is formed
// unsigned and scaled by 8 with wrap-around, so the bilinear arm's (x - 2) >> 2 for x < 2 (0x3FFFFFFF)
// lands 8 bytes before the row it names - an out-of-bounds read at the top-left, shipped. On x86 the
// uintptr_t arithmetic wraps exactly like retail's `lea`.
inline const unsigned char* dxt1BlockAt(const unsigned char* bits, unsigned int index)
{
#if defined(_M_IX86)
    return reinterpret_cast<const unsigned char*>(
        reinterpret_cast<std::uintptr_t>(bits) + static_cast<std::uintptr_t>(index * 8u));
#else
    // [PLATFORM] x64: the wrapped 32-bit byte offset is sign-extended, so the underflowed index still
    // lands 8 bytes before its row as on x86, not 4 GiB past the image.
    return bits + static_cast<std::ptrdiff_t>(static_cast<int>(index * 8u));
#endif
}

// (ecx = the locked bits, edx = width; stack: height, uv, out; retn 0Ch) - the terrain map
// colour under uv. byte_10239CFC (always 1) selects the point arm: one block decoded, one texel read.
// The bilinear arm is transcribed too - dead in the shipped image - with its own quirks: the 2x2
// block neighbourhood starts at ((x - 2) >> 2, (y - 2) >> 2), unsigned; the lerps are 0x101456C0's.
// The row pitch LockRect returned is ignored; blocks are addressed as (width / 4) per row.
void sampleDxt1(const unsigned char* bits, unsigned int width, unsigned int height,
                const float uv[2], unsigned int* out)
{
    const float u = clampFloat(uv[0], 0.0f, 1.0f) * (static_cast<float>(width) - 1.0f);
    const float v = clampFloat(uv[1], 0.0f, 1.0f) * (static_cast<float>(height) - 1.0f);
    const int x = static_cast<int>(u);
    const int y = static_cast<int>(v);
    if (kPointSampleTerrainMaps)
    {
        const unsigned int blockX = static_cast<unsigned int>(x) >> 2;
        const unsigned int blockY = static_cast<unsigned int>(y) >> 2;
        unsigned int texels[16];                                          // [ebp-58h], filled 0xFF
        std::memset(texels, 0xFF, sizeof(texels));
        decodeDxt1Block(dxt1BlockAt(bits, (width >> 2) * blockY + blockX), texels, 4u);
        *out = texels[static_cast<unsigned int>(x)
                      + 4u * (static_cast<unsigned int>(y) - 4u * blockY) - 4u * blockX];
        return;
    }

    const unsigned int blockX = static_cast<unsigned int>(x - 2) >> 2;
    const unsigned int blockY = static_cast<unsigned int>(y - 2) >> 2;
    const unsigned int blocksPerRow = width >> 2;
    unsigned int texels[64];                                              // [ebp-158h], filled 0xFF
    std::memset(texels, 0xFF, sizeof(texels));
    const unsigned char* const upper = dxt1BlockAt(bits, blocksPerRow * blockY + blockX);
    decodeDxt1Block(upper, &texels[0], 8u);
    decodeDxt1Block(upper + 8, &texels[4], 8u);
    const unsigned char* const lower = dxt1BlockAt(bits, (blockY + 1u) * blocksPerRow + blockX);
    decodeDxt1Block(lower, &texels[32], 8u);
    decodeDxt1Block(lower + 8, &texels[36], 8u);
    // floor is the CRT's double floor (the value goes to the stack as a double, 0x10146084).
    const float fracU = static_cast<float>(static_cast<double>(u) - std::floor(static_cast<double>(u)));
    const float fracV = static_cast<float>(static_cast<double>(v) - std::floor(static_cast<double>(v)));
    const unsigned int t = (static_cast<unsigned int>(x) - blockX * 4u)
                         + (static_cast<unsigned int>(y) - blockY * 4u) * 8u;
    unsigned int top = 0xFFFFFFFFu;                                       // [ebp-8]
    unsigned int bottom = 0xFFFFFFFFu;                                    // [ebp-4]
    lerpColor(&top, &texels[t], &texels[t + 1u], fracU);
    lerpColor(&bottom, &texels[t + 8u], &texels[t + 9u], fracU);
    lerpColor(out, &top, &bottom, fracV);
}

} // anonymous namespace


// Writes a row-major cell's world bounds; margin adds one unit in x/z only.
// Leaves bounds untouched when the cell array is absent. Index division is unsigned,
// while centering uses signed half-grid arithmetic.

void UndergrowthSystem::cellBounds(const UndergrowthCell* cell, MeshBounds* bounds, bool margin) const
{
    if (!m_cells)
        return;
    const unsigned int index = static_cast<unsigned int>(cell - m_cells);  // imul 4EC4EC4Fh (/ 104)
    const int half = static_cast<int>(m_cellsPerSide) / 2;
    const int row = static_cast<int>(index / m_cellsPerSide) - half;
    const int column = static_cast<int>(index % m_cellsPerSide) - half;
    bounds->min.x = static_cast<float>(column) * m_cellSize;
    bounds->min.y = cell->minHeight;
    bounds->min.z = static_cast<float>(row) * m_cellSize;
    bounds->max.x = m_cellSize + bounds->min.x;
    bounds->max.y = cell->maxHeight;
    bounds->max.z = m_cellSize + bounds->min.z;
    if (margin)
    {
        bounds->min.x = bounds->min.x - kCellBoundsMargin;
        bounds->min.z = bounds->min.z - kCellBoundsMargin;
        bounds->max.x = kCellBoundsMargin + bounds->max.x;
        bounds->max.z = kCellBoundsMargin + bounds->max.z;
    }
}


// worldToDensityCell - 0x10145B90 (203 B, retn 18h). World x/z to a density-map texel and the
// fractions within it. The map is centred: the texel is floor(p * invScale) + size / 2.

// Uses an unsigned half-size shift and signed bounds tests. The far edge is accepted here;
// consumers reject coordinates at size. z retains x87 precision while x is rounded to float.

void UndergrowthSystem::worldToDensityCell(float x, float z, int* col, int* row, float* fracX,
                                           float* fracZ)
{
    const float scaledX = x * m_invHeightmapScale.x;
    const float scaledZ = z * m_invHeightmapScale.z;
    const float floorX = static_cast<float>(std::floor(static_cast<double>(scaledX)));
    const double floorZ = std::floor(static_cast<double>(scaledZ));
    *fracX = scaledX - floorX;
    *fracZ = static_cast<float>(static_cast<double>(scaledZ) - floorZ);
    *col = static_cast<int>(floorX);
    *row = static_cast<int>(floorZ);
    *col += static_cast<int>(static_cast<unsigned int>(m_densityWidth) >> 1);
    *row += static_cast<int>(static_cast<unsigned int>(m_densityHeight) >> 1);
    if (*col > m_densityWidth || *col < 0)
        *col = 0;
    if (*row > m_densityHeight || *row < 0)
        *row = 0;
}


// Assigns nearby cells to reusable patch-cache entries.
// Forced updates build every pending patch; ordinary updates build only the nearest candidate.
// Disabled, unloaded or viewless systems do nothing.

void UndergrowthSystem::rebuildPatches(bool force, float viewDistance)
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();          // Renderer +0x50
    if (!view || !m_enabled || !m_loaded)
        return;

    world::IHeightmapCluster* const cluster = heightmapClusterByName();
    (void)cluster->getPrimaryHeightmap();                                            // +0x40 (host)

    m_visibleCells.resize(0);                                                        // (0, 0)

    // the camera's cell, x by division and z by the reciprocal.
    const Mat4* const cameraForX = view->getViewMatrixPtr();                         // +0x64
    const int cameraColumn = static_cast<int>(
        (static_cast<float>(m_cellsPerSide >> 1) * m_cellSize + cameraForX->m[12]) / m_cellSize);
    const Mat4* const cameraForZ = view->getViewMatrixPtr();                         // +0x64 again
    const float inverseCellSize = 1.0f / m_cellSize;
    const int cameraRow = static_cast<int>(
        (static_cast<float>(m_cellsPerSide >> 1) * m_cellSize + cameraForZ->m[14]) * inverseCellSize);
    const int span = 2 * static_cast<int>(
        std::ceil(static_cast<double>(inverseCellSize * viewDistance))) + 1;

    for (int row = 0; row < span; ++row)
    {
        for (int column = 0; column < span; ++column)
        {
            const int x = column - static_cast<int>(
                std::ceil(static_cast<double>(viewDistance / m_cellSize))) + cameraColumn;
            const int z = row - static_cast<int>(
                std::ceil(static_cast<double>(viewDistance / m_cellSize))) + cameraRow;
            if (x >= 0 && x < static_cast<int>(m_cellsPerSide) && z >= 0
                && z < static_cast<int>(m_cellsPerSide))
            {
                UndergrowthCell* const cell = m_cells + (m_cellsPerSide * static_cast<unsigned int>(z)
                                                         + static_cast<unsigned int>(x));
                m_visibleCells.push_back(cell);
            }
        }
    }

    // free the entries whose cell left the list. `size()` is re-read on
    // every probe (the loop reloads +0x2DC), as shipped.
    for (unsigned int i = 0; i < m_patchCount; ++i)
    {
        UndergrowthPatch& patch = m_patches[i];
        UndergrowthCell* const cell = patch.cell;
        if (!cell)
            continue;
        bool listed = false;
        for (unsigned int k = 0; k < m_visibleCells.size(); ++k)
        {
            if (m_visibleCells[k] == cell)
            {
                listed = true;
                break;
            }
        }
        if (!listed)
        {
            cell->patch = 0;
            patch.cell = 0;
        }
    }

    // give each newly listed cell the least recently drawn free entry.
    for (unsigned int k = 0; k < m_visibleCells.size(); ++k)
    {
        UndergrowthCell* const cell = m_visibleCells[k];
        if (cell->patch)
            continue;
        UndergrowthPatch* best = 0;
        UndergrowthPatch* candidate = m_patches;
        for (unsigned int remaining = m_patchCount; remaining != 0; --remaining, ++candidate)
        {
            if (candidate->cell)
                continue;
            if (!best || candidate->lastDrawnFrame <= best->lastDrawnFrame)
                best = candidate;
        }
        if (best)
        {
            best->cell = cell;
            best->built = 0;
            best->primitiveCount = 0;
            best->buildMaterial = 0;
            best->buildType = 0;
            best->buildRow = 0;
            best->buildColumn = 0;
            best->vertexCount = 0;
            best->indexCount = 0;
            cell->patch = best;
        }
    }

    if (force)
    {

        for (unsigned int i = 0; i < m_patchCount; ++i)
        {
            UndergrowthPatch& patch = m_patches[i];
            if (patch.cell && !patch.built)
                buildPatch(patch.cell, true);
        }
        return;
    }

    // m_drawOrder.resize(0), inlined here (the out-of-line instantiation
    // 0x1014FE60 is what render calls for the same statement).
    m_drawOrder.resize(0);

    // the view frustum at viewDistance, copy-constructed (0x100F58A0) into a
    // local while the far plane is swapped, then the far plane restored.
    const float savedFarPlane = view->getFarPlane();                                 // +0x3C
    view->setFarPlane(viewDistance);                                                 // +0x38
    const Frustum frustum(*view->getFrustumPtr());                                   // +0x74
    view->setFarPlane(savedFarPlane);

    for (unsigned int i = 0; i < m_patchCount; ++i)
    {
        UndergrowthPatch& patch = m_patches[i];
        if (!patch.cell || patch.built)
            continue;

        MeshBounds bounds;
        resetBounds(bounds);
        cellBounds(patch.cell, &bounds, true);

        // the nearest point of the box's TOP face: corner (min.x, max.y,
        // min.z) with the x and z edges.
        Vec3 corner;
        corner.x = bounds.min.x;
        corner.y = bounds.max.y;
        corner.z = bounds.min.z;
        Vec3 edgeX;
        edgeX.x = bounds.max.x - bounds.min.x;
        edgeX.y = 0.0f;
        edgeX.z = 0.0f;
        Vec3 edgeZ;
        edgeZ.x = 0.0f;
        edgeZ.y = 0.0f;
        edgeZ.z = bounds.max.z - bounds.min.z;
        const Mat4* const camera = view->getViewMatrixPtr();                         // +0x64
        const float reach = std::sqrt(squaredDistanceToQuad(
            *reinterpret_cast<const Vec3*>(&camera->m[12]), corner, edgeX, edgeZ));
        if (kBuildReachFactor * viewDistance < reach)                                // fcompp; jnp
            continue;

        const Mat4* const cameraAgain = view->getViewMatrixPtr();                    // +0x64 again
        if (bounds.dirty)
            meshBoundsFinalize(bounds);
        const float dx = cameraAgain->m[12] - bounds.center.x;
        const float dy = cameraAgain->m[13] - bounds.center.y;
        const float dz = cameraAgain->m[14] - bounds.center.z;
        float key = dx * dx + dz * dz + dy * dy;
        if (!terrainFrustumIntersectsBox(&frustum, &bounds.min.x))
            key = key + kOffscreenPenalty;

        UndergrowthCellDistance entry;
        entry.key = key;
        entry.cell = patch.cell;
        m_drawOrder.push_back(entry);            // inlined push_back (_Ufill 0x1014E460 / 0x1014EC90)
    }

    // std::sort(begin, end) nearest first, unconditionally (VC7.1 _Sort 0x10150100:
    // _ISORT_MAX 32, introsort; an empty or one-element range is a no-op there as here).
    std::sort(m_drawOrder.begin(), m_drawOrder.end(), UndergrowthCellDistanceLess());

    // stream in the nearest one.
    if (m_drawOrder.size() != 0)
        buildPatch(m_drawOrder[0].cell, false);
}

// Builds a cell's cached grass geometry from density, slope and mesh settings.
// cell, its patch, textures and heightmap must be valid; unusedFlag has no effect.
// Per-cell seeding makes placement deterministic. Skew requires the expected eight vertices.
// The resumable cursor retains the current column on budget exhaustion; a resume repeats it.
// Buffer-lock failure still runs both unlocks.
void UndergrowthSystem::buildPatch(UndergrowthCell* cell, bool unusedFlag)
{
    (void)unusedFlag;
    UndergrowthPatch* const patch = cell->patch;                             // [ebp-0Ch], read first

    world::IHeightmapCluster* const cluster = heightmapClusterByName();      // [ebp-54h]
    (void)cluster->getPrimaryHeightmap();                                    // +0x40, discarded

    const unsigned int index = static_cast<unsigned int>(cell - m_cells);
    if (cell->noDetail)                                                     // byte +0x65
        return;

    // the cell's corner: every term converted unsigned (the 2^32 fix-ups).
    const unsigned int half = m_cellsPerSide >> 1;
    const float halfSpan = static_cast<float>(half) * m_cellSize;
    const float originX = static_cast<float>(index % m_cellsPerSide) * m_cellSize - halfSpan;   // [ebp-5Ch]
    m_random.m_seed = index & 0x7FFFFFFFu;                                      // +0x2D0
    const float originZ = static_cast<float>(index / m_cellsPerSide) * m_cellSize - halfSpan;   // [ebp-58h]

    unsigned char* vertexWrite = static_cast<unsigned char*>(dx9rend::vertexBufferLock(
        patch->vertexBuffer, patch->vertexCount, m_patchVertexCount - patch->vertexCount, 0));
    unsigned char* indexWrite = static_cast<unsigned char*>(dx9rend::indexBufferLock(
        patch->indexBuffer, patch->indexCount, m_patchIndexCount - patch->indexCount, 0));
    if (!vertexWrite || !indexWrite)
    {
        dx9rend::vertexBufferUnlock(patch->vertexBuffer);
        dx9rend::indexBufferUnlock(patch->indexBuffer);
        return;
    }

    const unsigned int startVertexCount = patch->vertexCount;               // [ebp-0C0h]

    MeshBounds bounds;                                                       // [ebp-124h]
    resetBounds(bounds);
    cellBounds(cell, &bounds, false);
    // Two inlined accessors each test the dirty byte (the first answer is dead), then the centre.
    if (bounds.dirty)
        meshBoundsFinalize(bounds);
    if (bounds.dirty)
        meshBoundsFinalize(bounds);
    const Vec3 center = bounds.center;                                       // [ebp-78h]

    // both maps' dimensions and D3D textures (sub_10014940 twice each,
    // sub_10014930 once each), then - simple shader only - both locked read-only.
    const unsigned int colorWidth = dx9rend::textureDims(cell->colorMap)[0];   // [ebp-0E4h]
    const unsigned int colorHeight = dx9rend::textureDims(cell->colorMap)[1];  // [ebp-0BCh]
    const unsigned int lightWidth = dx9rend::textureDims(cell->lightMap)[0];   // [ebp-0DCh]
    const unsigned int lightHeight = dx9rend::textureDims(cell->lightMap)[1];  // [ebp-0E8h]
    IDirect3DTexture9* const colorTexture =
        static_cast<IDirect3DTexture9*>(cell->colorMap->baseTexture);        // [ebp-0E0h]
    IDirect3DTexture9* const lightTexture =
        static_cast<IDirect3DTexture9*>(cell->lightMap->baseTexture);        // [ebp-44h]
    // [ebp-138h] / [ebp-140h]. Retail leaves both uninitialised; each is read only under the same
    // m_simpleShaderEnable test that locked it, so these zeroes are never observed - [CHOSEN, not
    // recovered], they only settle C4701.
    D3DLOCKED_RECT colorRect;
    D3DLOCKED_RECT lightRect;
    colorRect.Pitch = 0;
    colorRect.pBits = 0;
    lightRect.Pitch = 0;
    lightRect.pBits = 0;
    if (m_simpleShaderEnable)
    {
        colorTexture->LockRect(0, &colorRect, 0, D3DLOCK_READONLY);         // vtable +0x4C
        lightTexture->LockRect(0, &lightRect, 0, D3DLOCK_READONLY);
    }

    // thirty-two Y rotations, 11.25 degrees apart. Retail default-constructs
    // the array first (sub_10001000, the eh vector constructor, with Mat4::Mat4 = identity,
    // sub_10030D80); this tree's Mat4 is an aggregate, and setRotateYDeg (0x10077E30) rewrites every
    // element in full anyway.
    Mat4 rotations[32];                                                      // [ebp-968h]
    for (unsigned int i = 0; i < 32u; ++i)
        setRotateYDeg(rotations[i], static_cast<float>(i) * kRotationStepDegrees);

    const float uniformStep = m_cellSize * kUniformUnit;                     // [ebp-50h]

    for (; patch->buildMaterial < m_materials.size(); ++patch->buildMaterial)
    {
        const UndergrowthMaterial& material = m_materials[patch->buildMaterial];      // [ebp-6Ch]
        if (cell->typeFractions[material.id] == 0.0f)                            // fucompp; test ah,44h
            continue;

        for (; patch->buildType < material.types.size(); ++patch->buildType)
        {
            const UndergrowthType& type = material.types[patch->buildType];
            if (!type.vertices.data() || type.vertices.size() == 0)
                continue;
            if (type.density == 0.0f)
                continue;

            const float root = std::sqrt(type.density);
            const float jitter = type.variation / root;                      // [ebp-68h]
            const unsigned int lattice = static_cast<unsigned int>(
                static_cast<int>(root * m_cellSize));                        // [ebp-48h], fistp
            const float latticeF = static_cast<float>(lattice);              // [ebp-64h], unsigned

            for (; patch->buildRow < lattice; ++patch->buildRow, patch->buildColumn = 0)
            {
                const float rowFraction = static_cast<float>(patch->buildRow) / latticeF;      // [ebp-40h]
                for (; patch->buildColumn < lattice; ++patch->buildColumn)
                {
                    const float columnFraction =
                        static_cast<float>(patch->buildColumn) / latticeF;                     // [ebp-4]
                    const unsigned int random = lcgNext(m_random.m_seed);                         // [ebp-8]

                    float x;                                                                   // [ebp-1Ch]
                    float z;                                                                   // [ebp-14h]
                    if (type.variation < 0.0f)                               // test ah,5 / jp
                    {
                        x = static_cast<float>(random & 0xFFu) * uniformStep + originX;
                        z = static_cast<float>((random >> 8) & 0xFFu) * uniformStep + originZ;
                    }
                    else
                    {
                        x = (static_cast<float>(random & 0xFFu) * kJitterUnit - 1.0f) * jitter
                            + columnFraction * m_cellSize + originX;
                        z = (static_cast<float>((random >> 8) & 0xFFu) * kJitterUnit - 1.0f) * jitter
                            + rowFraction * m_cellSize + originZ;
                        if (x > bounds.max.x)
                            x = x - m_cellSize;
                        else if (x < bounds.min.x)
                            x = x + m_cellSize;
                        if (z > bounds.max.z)
                            z = z - m_cellSize;
                        else if (z < bounds.min.z)
                            z = z + m_cellSize;
                    }

                    int texelColumn;                                                           // [ebp-0C4h]
                    int texelRow;                                                              // [ebp-60h]
                    float fracX;                                                               // [ebp-20h]
                    float fracZ;                                                               // [ebp-4]
                    worldToDensityCell(x, z, &texelColumn, &texelRow, &fracX, &fracZ);
                    int texel = 0;
                    if (texelColumn >= 0 && texelColumn < m_densityWidth && texelRow >= 0
                        && texelRow < m_densityHeight)
                        texel = m_densityMap[texelRow * m_densityWidth + texelColumn];
                    if (texel != material.id)
                        continue;

                    const unsigned int meshVertices = type.vertices.size();
                    if (patch->vertexCount + meshVertices > m_patchVertexCount)                // ja
                        continue;
                    const unsigned int meshIndices = type.indices.size();
                    if (patch->indexCount + meshIndices > m_patchIndexCount)
                        continue;

                    Vec3 normal;                                                               // [ebp-34h]
                    normal.x = 0.0f;
                    normal.y = 0.0f;
                    normal.z = 0.0f;
                    cluster->getNormalInWorldCoords(x, z, normal);                             // +0x7C (host)
                    if (normal.y < type.slopeRange.x)
                        continue;
                    if (normal.y > type.slopeRange.y)
                        continue;

                    const float groundHeight = cluster->getHeightInWorldCoords(x, z);          // +0x6C (host)
                    const float size = (type.randomSizeScale.y - type.randomSizeScale.x)
                        * static_cast<float>((random >> 16) & 0x3Fu) * kSizeRandomUnit
                        + type.randomSizeScale.x;                                                  // [ebp-38h]

                    // the rotation copied (Mat4 copy constructor, 0x10030DD0), then
                    // re-based on the terrain normal. Every product is the shipped operand order.
                    Mat4 basis = rotations[(random >> 26) & 0x1Fu];                            // [ebp-0B8h]
                    basis.m[4] = normal.x;
                    basis.m[5] = normal.y;
                    basis.m[6] = normal.z;
                    basis.m[0] = basis.m[10] * normal.y - basis.m[9] * normal.z;
                    basis.m[1] = basis.m[8] * normal.z - basis.m[10] * normal.x;
                    basis.m[2] = basis.m[9] * normal.x - basis.m[8] * normal.y;
                    basis.m[8] = basis.m[1] * normal.z - basis.m[2] * normal.y;
                    basis.m[9] = basis.m[2] * normal.x - basis.m[0] * normal.z;
                    basis.m[10] = basis.m[0] * normal.y - basis.m[1] * normal.x;
                    Vec3 scale;                                                                // [ebp-130h]
                    scale.x = size;
                    scale.y = size;
                    scale.z = size;
                    mat4ScaleColumns3x3(basis, scale);

                    const unsigned char swayIndex =
                        static_cast<unsigned char>((random >> 22) & 0xFu);                     // [ebp-0Dh]
                    const unsigned char heightByte = static_cast<unsigned char>(
                        static_cast<long long>(size * type.size.y * kHeightByteScale));         // __ftol2
                    const float heightF = static_cast<float>(static_cast<int>(heightByte));    // [ebp-4]
                    const float relativeX = x - center.x;                                      // [ebp-1Ch]
                    const float relativeY = groundHeight - center.y;                           // [ebp-18h]
                    const float relativeZ = z - center.z;                                      // [ebp-14h]

                    const UndergrowthVertex* const source =
                        type.vertices.data();
                    if (m_simpleShaderEnable)
                    {
                        // 0x101473C3..0x10147611 - 24-byte vertices. tx = (m4 y + m8 z) + m0 x.
                        UndergrowthPatchVertexSimple* out =
                            reinterpret_cast<UndergrowthPatchVertexSimple*>(vertexWrite);
                        for (unsigned int k = 0;
                             type.vertices.data() && k < type.vertices.size();
                             ++k, ++out)
                        {
                            const UndergrowthVertex& v = source[k];
                            const float tx = basis.m[4] * v.position.y + basis.m[8] * v.position.z
                                           + basis.m[0] * v.position.x;
                            const float ty = basis.m[5] * v.position.y + basis.m[9] * v.position.z
                                           + basis.m[1] * v.position.x;
                            const float tz = basis.m[6] * v.position.y + basis.m[10] * v.position.z
                                           + basis.m[2] * v.position.x;
                            const float px = tx + relativeX;                                   // [ebp-0D0h]
                            const float pz = tz + relativeZ;                                   // [ebp-0C8h]
                            out->base.position[0] = static_cast<short>(
                                static_cast<long long>(px * kPositionScale));
                            out->base.position[1] = static_cast<short>(
                                static_cast<long long>(ty * kPositionScale));
                            out->base.position[2] = static_cast<short>(
                                static_cast<long long>(pz * kPositionScale));
                            out->base.position[3] = static_cast<short>(
                                static_cast<long long>(relativeY * kPositionScale));
                            out->base.texCoord = packedTexCoord(v);
                            out->base.swayIndex = swayIndex;
                            out->base.swayAmount = static_cast<unsigned char>(
                                static_cast<long long>(heightF * v.sway));
                            out->base.heightByte = heightByte;
                            out->base.colorScale = v.colorScale;
                            float uv[2];                                                       // [ebp-0D8h]
                            uv[0] = (px + center.x) * cell->texScaleU + cell->texOffsetU;
                            uv[1] = (pz + center.z) * cell->texScaleV + cell->texOffsetV;
                            sampleDxt1(static_cast<const unsigned char*>(colorRect.pBits),
                                       colorWidth, colorHeight, uv, &out->colorSample);
                            sampleDxt1(static_cast<const unsigned char*>(lightRect.pBits),
                                       lightWidth, lightHeight, uv, &out->lightSample);
                        }
                        if (type.skew)
                        {
                            // the six position bytes (x, y, z) of vertices
                            // 1, 0, 5, 4 onto 2, 3, 6, 7, then each copy's y raised.
                            UndergrowthPatchVertexSimple* const v =
                                reinterpret_cast<UndergrowthPatchVertexSimple*>(vertexWrite);
                            std::memcpy(v[2].base.position, v[1].base.position, 6);
                            std::memcpy(v[3].base.position, v[0].base.position, 6);
                            std::memcpy(v[6].base.position, v[5].base.position, 6);
                            std::memcpy(v[7].base.position, v[4].base.position, 6);
                            const int lift = static_cast<int>(size * type.size.y * kPositionScale);  // fistp
                            v[2].base.position[1] = static_cast<short>(v[2].base.position[1] + lift);
                            v[3].base.position[1] = static_cast<short>(v[3].base.position[1] + lift);
                            v[6].base.position[1] = static_cast<short>(v[6].base.position[1] + lift);
                            v[7].base.position[1] = static_cast<short>(v[7].base.position[1] + lift);
                        }
                    }
                    else
                    {
                        // 0x10147616..0x101477C4 - 16-byte vertices. tx = (m0 x + m4 y) + m8 z here -
                        // the other association from the simple arm; both are the shipped ones.
                        UndergrowthPatchVertex* out = reinterpret_cast<UndergrowthPatchVertex*>(vertexWrite);
                        for (unsigned int k = 0;
                             type.vertices.data() && k < type.vertices.size();
                             ++k, ++out)
                        {
                            const UndergrowthVertex& v = source[k];
                            const float tx = basis.m[0] * v.position.x + basis.m[4] * v.position.y
                                           + basis.m[8] * v.position.z;
                            const float ty = basis.m[1] * v.position.x + basis.m[5] * v.position.y
                                           + basis.m[9] * v.position.z;
                            const float tz = basis.m[2] * v.position.x + basis.m[6] * v.position.y
                                           + basis.m[10] * v.position.z;
                            const float px = tx + relativeX;
                            const float pz = tz + relativeZ;                                   // [ebp-144h]
                            out->position[0] = static_cast<short>(static_cast<long long>(px * kPositionScale));
                            out->position[1] = static_cast<short>(static_cast<long long>(ty * kPositionScale));
                            out->position[2] = static_cast<short>(static_cast<long long>(pz * kPositionScale));
                            out->position[3] = static_cast<short>(
                                static_cast<long long>(relativeY * kPositionScale));
                            out->texCoord = packedTexCoord(v);
                            out->swayIndex = swayIndex;
                            out->swayAmount = static_cast<unsigned char>(
                                static_cast<long long>(heightF * v.sway));
                            out->heightByte = heightByte;
                            out->colorScale = v.colorScale;
                        }
                        if (type.skew)
                        {
                            UndergrowthPatchVertex* const v = reinterpret_cast<UndergrowthPatchVertex*>(vertexWrite);
                            std::memcpy(v[2].position, v[1].position, 6);
                            std::memcpy(v[3].position, v[0].position, 6);
                            std::memcpy(v[6].position, v[5].position, 6);
                            std::memcpy(v[7].position, v[4].position, 6);
                            const int lift = static_cast<int>(size * type.size.y * kPositionScale);
                            v[2].position[1] = static_cast<short>(v[2].position[1] + lift);
                            v[3].position[1] = static_cast<short>(v[3].position[1] + lift);
                            v[6].position[1] = static_cast<short>(v[6].position[1] + lift);
                            v[7].position[1] = static_cast<short>(v[7].position[1] + lift);
                        }
                    }

                    // the mesh's indices, rebased by the entry's running
                    // vertex count in 16-bit arithmetic.
                    const unsigned short* const sourceIndices = type.indices.data();
                    unsigned short* const indexOut = reinterpret_cast<unsigned short*>(indexWrite);
                    for (unsigned int k = 0;
                         type.indices.data() && k < type.indices.size(); ++k)
                        indexOut[k] = static_cast<unsigned short>(
                            static_cast<unsigned short>(patch->vertexCount) + sourceIndices[k]);

                    // advance both cursors and the counts, then the budget.
                    const unsigned int stride = m_simpleShaderEnable ? 24u : 16u;
                    vertexWrite += type.vertices.size() * stride;
                    indexWrite += type.indices.size() * 2u;
                    patch->vertexCount += type.vertices.size();
                    const unsigned int written = patch->vertexCount - startVertexCount;
                    patch->indexCount += type.indices.size();
                    if (written >= kBuildVertexBudget)
                        goto unlock;                          // without ++buildColumn - see the head
                }
            }
            patch->buildRow = 0;
        }
        patch->buildType = 0;
    }

    // complete.
    patch->primitiveCount = patch->indexCount / 3u;
    patch->buildMaterial = 0;
    patch->built = 1;

unlock:
    dx9rend::vertexBufferUnlock(patch->vertexBuffer);
    dx9rend::indexBufferUnlock(patch->indexBuffer);
    if (m_simpleShaderEnable)
    {
        colorTexture->UnlockRect(0);                                         // vtable +0x50
        lightTexture->UnlockRect(0);
    }
}


// Streams and draws visible grass patches; time is unused.
// Skips unloaded/disabled systems and rebuilds the cache after large camera moves.
// Draws near patches in a depth prepass, then batches lighting/shadow techniques.
// Preserve origin-based light-gather radius and the existing camera/water-fog constant behavior.

void UndergrowthSystem::render(float time)
{
    (void)time;
    if (!m_loaded || !m_enabled)
        return;

    // the stat bracket's open (sub_10046EE0 on +0x300): not reproduced.

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();          // Renderer +0x50
    const float viewDistance = m_viewDistanceScale * m_viewDistance;                 // [ebp-1Ch]
    const float halfViewDistance = viewDistance * 0.5f;                              // [ebp-0Ch]

    RenderStatics& statics = g_renderStatics;
    if ((statics.onceFlags & 1u) == 0)
    {
        statics.onceFlags |= 1u;
        statics.cameraPosition.x = 0.0f;
        statics.cameraPosition.y = 0.0f;
        statics.cameraPosition.z = 0.0f;
    }
    if (!kFreezeCulling)
    {
        const Mat4* const camera = view->getViewMatrixPtr();                         // +0x64
        statics.cameraPosition.x = camera->m[12];
        statics.cameraPosition.y = camera->m[13];
        statics.cameraPosition.z = camera->m[14];
    }

    if (distanceBetween(statics.cameraPosition, m_lastRebuildPosition) > halfViewDistance)
        rebuildPatches(true, viewDistance);
    m_lastRebuildPosition = statics.cameraPosition;
    rebuildPatches(false, viewDistance);

    if (viewDistance < 1.0f)                                                 // test ah,5 / jnp
        return;

    if ((statics.onceFlags & 2u) == 0)
    {
        statics.onceFlags |= 2u;
        new (g_cullFrustumStorage.bytes) Frustum();
    }
    Frustum& frustum = cullFrustum();
    if (!kFreezeCulling)
    {
        const float savedFarPlane = view->getFarPlane();                     // +0x3C
        view->setFarPlane(viewDistance);                                     // +0x38
        frustum = *view->getFrustumPtr();                                    // +0x74,
        view->setFarPlane(savedFarPlane);
    }

    // the visible, built entries, keyed by squared camera distance.
    m_drawOrder.resize(0);                                                   // (0, {0, 0})
    for (unsigned int i = 0; i < m_patchCount; ++i)
    {
        UndergrowthPatch& patch = m_patches[i];
        if (!patch.cell)
            continue;
        MeshBounds bounds;                                                   // [ebp-64h]
        resetBounds(bounds);
        cellBounds(patch.cell, &bounds, true);
        if (!terrainFrustumIntersectsBox(&frustum, &bounds.min.x))
            continue;
        if (!patch.primitiveCount)
            continue;
        if (objectDrawerOccludesBounds(dx9rend::g_ObjectDrawer, &bounds))   // ObjectDrawer +0x3C
            continue;
        const Mat4* const camera = view->getViewMatrixPtr();                 // +0x64
        if (bounds.dirty)
            meshBoundsFinalize(bounds);
        UndergrowthCellDistance entry;
        entry.key = distanceSquaredBetween(bounds.center,
                                           *reinterpret_cast<const Vec3*>(&camera->m[12]));
        entry.cell = patch.cell;
        m_drawOrder.push_back(entry);
    }

    const unsigned int drawCount = static_cast<unsigned int>(m_drawOrder.size());
    if (drawCount == 0)
        return;
    {
        UndergrowthCellDistance* const first = m_drawOrder.data();
        std::sort(first, first + drawCount, UndergrowthCellDistanceLess());
    }

    // 6. the frame constants
    // the first dynamic-shadow record (LightManager +0x79C, 1176 bytes each),
    // NOT gated on dynamicShadowsEnable; the record vector is fetched three times, as shipped.
    {
        const std::vector<LightShadowRecord>& records = lightManagerShadowRecords(g_LightManager);
        if (records.size() != 0)
        {
            m_paramShadowMap->setValue(lightManagerShadowMapTexture(g_LightManager), 0);   // +0x21C
            m_paramShadowProjMatrix->setValue(
                &lightManagerShadowRecords(g_LightManager)[0].matrices[4], 0);            // +0x2F8
            m_paramShadowTrapMatrix->setValue(
                &lightManagerShadowRecords(g_LightManager)[0].matrices[7], 0);            // +0x3B8
        }
    }

    Vec4 color;                                                              // [ebp-94h], zeroed once
    color.x = 0.0f;
    color.y = 0.0f;
    color.z = 0.0f;
    color.w = 0.0f;
    Vec3 rgb;                                                                // [ebp-0ACh]
    m_terrain->getSunColorRaw(&rgb);                                         // Terrain +0x78 (RVO)
    color.x = rgb.x * m_lightingScale;
    color.w = 1.0f;
    color.y = rgb.y * m_lightingScale;
    color.z = rgb.z * m_lightingScale;
    m_paramSunColor->setValue(&color, 0);

    Vec3 giScratch;                                                          // [ebp-14h]
    rgb = *m_terrain->getGIColorRaw(&giScratch);                             // Terrain +0x84, copied
    color.x = rgb.x * m_lightingScale;
    color.w = 1.0f;
    color.y = rgb.y * m_lightingScale;
    color.z = rgb.z * m_lightingScale;
    m_paramGIColor->setValue(&color, 0);

    rgb.x = raConstantF(0x230);                                              // the static point colour
    rgb.y = raConstantF(0x234);
    rgb.z = raConstantF(0x238);
    color.w = 1.0f;
    color.x = rgb.x;
    color.y = rgb.y;
    color.z = rgb.z;
    m_paramStaticPointColor->setValue(&color, 0);

    m_paramFogRange->setValue(raConstantAt(0xA8), 0);                     // FogRange holder

    Vec4 fogColor;                                                           // [ebp-18h], a copy
    fogColor.x = raConstantF(0xBC);
    fogColor.y = raConstantF(0xC0);
    fogColor.z = raConstantF(0xC4);
    fogColor.w = raConstantF(0xC8);
    m_paramFogColor->setValue(&fogColor, 0);

    Vec4 ambientRange;                                                       // [ebp-120h] - sub_100380E0
    ambientRange.x = raConstantF(0xD0);
    ambientRange.y = raConstantF(0xD4);
    ambientRange.z = raConstantF(0xD8);
    ambientRange.w = raConstantF(0xDC);
    m_paramAmbientIlluminationRange->setValue(&ambientRange, 0);

    dx9rend::postProductionUpdateFog(dx9rend::underwaterDepth());           // sub_1004FDA0(sub_100F7B00())

    m_paramWaterFogRange->setValue(raConstantAt(0xF8), 0);
    m_paramTexLayer0->setValue(m_atlasTexture, 0);
    m_paramWorldViewProjection->setValue(view->getViewProjectionMatrixPtr(), 0);   // +0x84
    m_paramWorldView->setValue(view->getInverseViewMatrixPtr(), 0);                // +0x68

    Vec4 fade;                                                               // [ebp-14Ch]
    fade.x = viewDistance;
    fade.y = 1.0f / (viewDistance * m_viewDistanceFade);
    fade.z = 0.0f;
    fade.w = 0.0f;
    m_paramFadeAndHeightScaleOffset->setValue(&fade, 0);

    m_paramSwayOffset->setValue(m_windWaves, 0);                             // +0x1CC, 16 float4
    m_paramCameraPos->setValue(&statics.cameraPosition, 0);                 // w = the guard word

    int alphaRef = static_cast<int>(m_alphaRef * kAlphaRefScale);            // [ebp-10Ch], fistp
    float transparencyScale = 1.0f;                                          // [ebp-8]
    if (isMultisampleActive()
        && reinterpret_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->hasAlphaToCoverage
        && m_alphaRef > 0.0f)
    {
        alphaRef = 0;
        transparencyScale = 1.0f / m_alphaRef;
    }
    const float transparencyX8 = transparencyScale * kTransparencyX8Scale;
    Vec4 transparency;                                                       // [ebp-13Ch]
    transparency.x = transparencyX8;
    transparency.y = transparencyX8;
    transparency.z = transparencyX8;
    transparency.w = transparencyX8;
    m_paramTransparencyX8->setValue(&transparency, 0);
    m_paramAlphaRef->setValue(&alphaRef, 0);
    m_paramCameraToWater->setFloat(raConstantF(0x1D8));                      // +0x24 setFloat

    // 7. the depth prepass
    if (m_zPassPatchCount != 0)                                              // `jbe` - unsigned
    {
        dx9rend::stateManagerPush();
        dx9rend::effectSetTechniqueByName(m_effect,
            std::string(m_simpleShaderEnable ? "ZOnly_Simple" : "ZOnly"));
        dx9rend::effectBegin(m_effect);
        dx9rend::effectBeginPass(m_effect, 0);
        if (dx9rend::deviceCapsIsNvidia())
        {
            dx9rend::stateManagerSetRenderState(D3DRS_STENCILENABLE, TRUE);
            dx9rend::stateManagerSetRenderState(D3DRS_STENCILFUNC, D3DCMP_GREATEREQUAL);
            dx9rend::stateManagerSetRenderState(D3DRS_STENCILREF, 10);
            dx9rend::stateManagerSetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
            dx9rend::stateManagerSetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
            dx9rend::stateManagerSetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        }
        for (unsigned int i = 0;
             i < m_drawOrder.size() && i < m_zPassPatchCount; ++i)
        {
            UndergrowthCell* const cell = m_drawOrder[i].cell;
            MeshBounds bounds;
            resetBounds(bounds);
            cellBounds(cell, &bounds, false);
            if (bounds.dirty)
                meshBoundsFinalize(bounds);
            const float* const packed = staticMeshPackVec4(&bounds.center.x, kPosOffsetScaleW);
            Vec4 offset;                                                     // [ebp-18h]
            offset.x = packed[0];
            offset.y = packed[1];
            offset.z = packed[2];
            offset.w = packed[3];
            m_paramPosOffsetAndScale->setValue(&offset, 0);
            dx9rend::vertexBufferBindStream(cell->patch->vertexBuffer, 0);
            dx9rend::indexBufferBind(cell->patch->indexBuffer);
            dx9rend::rendererDrawIndexedPrimitive(cell->patch->primitiveCount, D3DPT_TRIANGLELIST,
                                                  0, 0, cell->patch->vertexCount, 0);
            ++terrainRendererStat(0x27C);                                    // Renderer +0x6C, twice
            terrainRendererStat(0x280) += cell->patch->primitiveCount;
        }
        dx9rend::effectEnd(m_effect);
        dx9rend::stateManagerPop();
    }

    // 8. classify every sorted entry into the draw list
    dx9rend::stateManagerPush();
    UndergrowthDrawList drawList;                                            // 0x1014E370 head node
    for (unsigned int i = 0; i < m_drawOrder.size(); ++i)
    {
        const bool zPrepassed = i < m_zPassPatchCount;                      // `setb`
        std::vector<world::LightSource*> lights;                             // [ebp-104h]
        UndergrowthCell* const cell = m_drawOrder[i].cell;
        MeshBounds bounds;                                                   // [ebp-0F0h]
        resetBounds(bounds);
        cellBounds(cell, &bounds, false);
        // The gather's two box accessors, radius first (right-to-left argument order), each testing
        // the dirty byte; the radius is originRadius (+0x18), as shipped.
        if (bounds.dirty)
            meshBoundsFinalize(bounds);
        const float gatherRadius = bounds.originRadius;
        if (bounds.dirty)
            meshBoundsFinalize(bounds);
        g_LightManager->gatherLights(bounds.center, gatherRadius, lights, 4u, 0xB09u, 0u, true);  // +0x1C

        const unsigned int gathered = static_cast<unsigned int>(lights.size());
        const unsigned int lightCount = gathered >= 4u ? 4u : gathered;      // [ebp-0F4h]

        bool shadowed = false;                                               // [ebp-0B0h]
        if (m_dynamicShadowsEnable)
        {
            const std::vector<LightShadowRecord>& records =
                lightManagerShadowRecords(g_LightManager);                   // sub_1015C550, once
            for (unsigned int k = 0; k < records.size(); ++k)
            {
                const Frustum* const shadowFrustum = &records[k].frustum;
                if (terrainFrustumIntersectsBox(shadowFrustum, &bounds.min.x))
                {
                    shadowed = true;
                    break;
                }
            }
        }

        const char* const technique =
            kTechniques[(m_simpleShaderEnable ? 2 : 0) + (shadowed ? 1 : 0)][lightCount];

        // the item is built as a temporary, copied into a named pair
        // (the temporary dies there, 0x1014A5AB), and the pair converted to value_type for insert.
        const std::pair<const char*, UndergrowthDrawItem> entry(
            technique,
            UndergrowthDrawItem{ cell,
                                 std::vector<world::LightSource*>(
                                     lights.data(), lights.data() + gathered),
                                 lightCount, bounds, technique, shadowed, zPrepassed });
        drawList.insert(entry);
    }

    // 9. draw, grouped by technique
    const char* currentTechnique = 0;                                        // [ebp-4]
    int currentStencilState = -1;                                            // [ebp-0Ch]
    for (UndergrowthDrawList::iterator it = drawList.begin(); it != drawList.end(); ++it)
    {
        UndergrowthDrawItem& item = it->second;

        Vec4 lightPositions[4];                                              // [ebp-0ECh]
        Vec4 lightColors[4];                                                 // [ebp-68h]
        for (unsigned int k = 0; k < 4u; ++k)
        {
            lightPositions[k].x = 0.0f;  lightPositions[k].y = 0.0f;
            lightPositions[k].z = 0.0f;  lightPositions[k].w = 0.0f;
            lightColors[k].x = 0.0f;     lightColors[k].y = 0.0f;
            lightColors[k].z = 0.0f;     lightColors[k].w = 0.0f;
        }
        // Walks every gathered light (the gather is capped at four by its own argument).
        for (unsigned int k = 0; k < item.lights.size(); ++k)
        {
            world::LightSource* const light = item.lights[k];
            float range = light->getAttenuationRange2();                     // +0x190 (host)
            const world::LightSourceTemplate* const lightTemplate = lightSourceTemplate(light);
            if (lightTemplate && lightTemplate->getIsDynamic())             // template +0xF0 (host)
                range = g_LightManager->getDynamicLightRangeScale() * range; // +0x2E0
            const Mat4& transform = light->getAbsoluteTransformation();      // +0x80 (host)
            // The pack below is 0x1008DD80 inlined in retail, on the same static (dword_1024B100).
            const float* const position = staticMeshPackVec4(&transform.m[12], 1.0f / (range * range));
            lightPositions[k].x = position[0];
            lightPositions[k].y = position[1];
            lightPositions[k].z = position[2];
            lightPositions[k].w = position[3];
            const float* const lightColor = staticMeshPackVec4(&light->getColor().x, 1.0f);   // +0x198 (host)
            lightColors[k].x = lightColor[0];
            lightColors[k].y = lightColor[1];
            lightColors[k].z = lightColor[2];
            lightColors[k].w = lightColor[3];
        }
        m_paramPointLightPosAtten->setValue(lightPositions, item.lightCount);
        m_paramPointLightColor->setValue(lightColors, item.lightCount);

        item.cell->patch->lastDrawnFrame = dx9rend::rendererFrameOrdinal();  // Renderer +0x54
        m_paramTexLayer1->setValue(item.cell->colorMap, 0);
        m_paramTexLayer2->setValue(item.cell->lightMap, 0);
        m_paramTerrainTexCoordScaleAndOffset->setValue(&item.cell->texScaleU, 0);   // cell +0x0C

        if (item.bounds.dirty)
            meshBoundsFinalize(item.bounds);
        const float* const packed = staticMeshPackVec4(&item.bounds.center.x, kPosOffsetScaleW);
        Vec4 offset;                                                         // [ebp-120h]
        offset.x = packed[0];
        offset.y = packed[1];
        offset.z = packed[2];
        offset.w = kPosOffsetScaleW;
        m_paramPosOffsetAndScale->setValue(&offset, 0);

        int stencilState;                                                    // ebx
        if (currentTechnique != item.technique)
        {
            if (currentTechnique)
                dx9rend::effectEnd(m_effect);
            dx9rend::effectSetTechniqueByName(m_effect, std::string(item.technique));
            dx9rend::effectBegin(m_effect);
            dx9rend::effectBeginPass(m_effect, 0);
            stencilState = -1;
        }
        else
        {
            stencilState = currentStencilState;
        }

        if (dx9rend::deviceCapsIsNvidia())
        {
            if (static_cast<int>(item.zPrepassed) != stencilState)
            {
                if (item.zPrepassed)
                {
                    dx9rend::stateManagerSetRenderState(D3DRS_STENCILENABLE, TRUE);
                    dx9rend::stateManagerSetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
                    dx9rend::stateManagerSetRenderState(D3DRS_STENCILREF, 10);
                    dx9rend::stateManagerSetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
                    dx9rend::stateManagerSetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
                    dx9rend::stateManagerSetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
                    dx9rend::stateManagerSetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
                }
                else
                {
                    dx9rend::stateManagerSetRenderState(D3DRS_STENCILENABLE, FALSE);
                    dx9rend::stateManagerSetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
                }
            }
        }

        UndergrowthPatch* const patch = item.cell->patch;
        dx9rend::vertexBufferBindStream(patch->vertexBuffer, 0);
        dx9rend::indexBufferBind(patch->indexBuffer);
        dx9rend::rendererDrawIndexedPrimitive(patch->primitiveCount, D3DPT_TRIANGLELIST, 0, 0,
                                              patch->vertexCount, 0);
        currentTechnique = item.technique;
        currentStencilState = item.zPrepassed ? 1 : 0;
        ++terrainRendererStat(0x27C);
        terrainRendererStat(0x280) += item.cell->patch->primitiveCount;
    }
    if (currentTechnique)
        dx9rend::effectEnd(m_effect);
    dx9rend::stateManagerPop();

    // the stat bracket's close: not reproduced. The draw list is destroyed at scope
    // end (0x10150D20 then the head node's free), after the pop, as shipped.
}

// The foreign layouts this file depends on (the class's own offsets are pinned by the header's
// UndergrowthLayoutCheck).
#if defined(_M_IX86)
static_assert(offsetof(MeshBounds, originRadius) == 0x18, "the gather radius field at +0x18");
static_assert(offsetof(MeshBounds, center) == 0x30, "box centre at +0x30");
static_assert(sizeof(Frustum) == 0x1E8, "dword_1025E4D8 spans 0x1E8 bytes");
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
