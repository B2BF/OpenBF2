// Ra shader interfaces and material, lighting and object parameter blocks.

#ifndef BF2_RENDDX9_RA_RASHADER_H
#define BF2_RENDDX9_RA_RASHADER_H

#ifdef BF2_IMAGE_RENDDX9

#include "../RendDX9Base.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "../../../RendDX9Abi/Vc71Map.h"
#include "../../../RendDX9Abi/Vc71String.h"
#include "../../../RendDX9Abi/Vc71Vector.h"

#include <stddef.h>

namespace dice {
namespace hfe {
namespace dx9rend {

struct RenderQueueEntry;


// Returns a cached/compiled D3DX effect for the normalized path, or null.
void* effectManagerGetEffect(const vc71::string& path);
void  effectManagerResetEffects();

// Returns StateManager's current pixel shader for shader-model inspection.
void* stateManagerCurrentPixelShader();

namespace Ra {


// Parameter holders pair a binder table with fixed-layout value storage.

struct RaParamFloat  { void* vptr; float value; };                        // off_101E148C / 1E1E80
struct RaParamInt    { void* vptr; int   value; };
struct RaParamRef    { void* vptr; void* value; };
struct RaParamVec3   { void* vptr; float value[3]; };
struct RaParamVec4   { void* vptr; float value[4]; };
struct RaParamPair   { void* vptr; unsigned int a; unsigned int b; };
struct RaParamMat4   { void* vptr; float value[16]; };
struct RaParamVector { void* vptr; vc71::vector_layout value; };

#if defined(_M_IX86)
static_assert(sizeof(RaParamFloat)  == 0x08, "float holder");
static_assert(sizeof(RaParamInt)    == 0x08, "int holder");
static_assert(sizeof(RaParamRef)    == 0x08, "ref holder");
static_assert(sizeof(RaParamVec3)   == 0x10, "vec3 holder");
static_assert(sizeof(RaParamVec4)   == 0x14, "vec4 holder");
static_assert(sizeof(RaParamPair)   == 0x0C, "pair holder");
static_assert(sizeof(RaParamMat4)   == 0x44, "mat4 holder");
static_assert(sizeof(RaParamVector) == 0x14, "vector holder");
#endif

// Technique lookup node. Its 24-byte layout is distinct from the shared 0x30-byte map node;
// using the larger node helpers would write past its flags.
// RaShader's +0x134 byte is set on construction/build but its purpose is [UNRESOLVED].
// The unused dword at +0x164 is also [UNRESOLVED].
struct RaTechniqueNode
{
    RaTechniqueNode* left;      // +0x00
    RaTechniqueNode* parent;    // +0x04
    RaTechniqueNode* right;     // +0x08
    unsigned int     key;       // +0x0C
    void*            value;     // +0x10
    unsigned char    color;     // +0x14
    unsigned char    isNil;     // +0x15
    unsigned char    pad[2];    // +0x16
};

// The 24-byte tree's header.  Same three fields as vc71::map_layout, different node size, so it is
// written out separately rather than reusing that type - see the warning above.
struct RaTechniqueMap
{
    void*            comp;      // +0x00  never written
    RaTechniqueNode* head;      // +0x04  the nil sentinel, self-linked in all three links
    unsigned int     size;      // +0x08
};

// Shared vertex-element descriptor; shaders retain pointers to these static records.
// Packed/unpacked variants share an element ID. Declaration type/usage names are [INFERRED].
// [FRONTIER] Packed-8 normal/tangent declaration types require the device-capability seed:
// DEC3N when supported, otherwise D3DCOLOR. Their initial unused value is not the final format.
struct RaVertexElementDesc
{
    RaVertexElementDesc(int element, int type, int usage, const char* elementName)
        : elementId(element), declType(type), declUsage(usage), name(elementName) {}

    int          elementId;   // +0x00
    int          declType;    // +0x04  D3DDECLTYPE   [INFERRED]
    int          declUsage;   // +0x08  D3DDECLUSAGE  [INFERRED]
    vc71::string name;        // +0x0C  "Vtx_<element>"
};

// The twenty objects themselves.  They are at namespace scope rather than file-local because the
// Geom translation unit takes their addresses too - StaticMeshMaterial::setupTechnique (PE
// 0x101187D0) names sixteen of them directly, which is evidence that dice declared them in a shared
// header exactly like this one.
extern RaVertexElementDesc g_desc_1023ED4C;   // TBase2D
extern RaVertexElementDesc g_desc_1023ED74;   // Bone2Weights
extern RaVertexElementDesc g_desc_1023ED9C;   // PositionPacked, PositionPacked0, PositionPacked1
extern RaVertexElementDesc g_desc_1023EDC4;   // TangentSpacePacked16
extern RaVertexElementDesc g_desc_1023EDEC;   // TBasePacked2D
extern RaVertexElementDesc g_desc_1023EE14;   // NormalPacked8
extern RaVertexElementDesc g_desc_1023EE3C;   // TCrackPacked2D
extern RaVertexElementDesc g_desc_1023EE64;   // TLightMapPacked2D
extern RaVertexElementDesc g_desc_1023EE8C;   // TDetailPacked2D, TUVAnimationPacked2D
extern RaVertexElementDesc g_desc_1023EEB4;   // TangentSpacePacked8
extern RaVertexElementDesc g_desc_1023EEDC;   // Bone4Idcs
extern RaVertexElementDesc g_desc_1023EF04;   // Position, Position0, Position1
extern RaVertexElementDesc g_desc_1023EF2C;   // TLightMap2D
extern RaVertexElementDesc g_desc_1023EF54;   // NormalPacked16
extern RaVertexElementDesc g_desc_1023EF7C;   // Normal
extern RaVertexElementDesc g_desc_1023EFA4;   // TDirtPacked2D
extern RaVertexElementDesc g_desc_1023EFCC;   // TDirt2D
extern RaVertexElementDesc g_desc_1023EFF4;   // TangentSpace
extern RaVertexElementDesc g_desc_1023F01C;   // TDetail2D, TUVAnimation2D
extern RaVertexElementDesc g_desc_1023F044;   // TCrack2D


// D3DXPARAMETER_DESC, 44 bytes.  The size is read from the image twice over - the build body
// reserves ebp-0x21C..ebp-0x1F0 for one, and the 80-byte record below holds one at +0x04 with an
// int at +0x30 - and Elements at +0x18 is the only field any recovered body reads (`cmp
// [ebp+var_C0], ebx` and `cmp [ebp+var_204], 0`, both of which are desc+0x18).

#if defined(_M_IX86)
static_assert(sizeof(D3DXPARAMETER_DESC) == 44, "D3DXPARAMETER_DESC is 44 bytes");
#endif


// Bound shader parameter. offset is a registry name index, not a byte offset.

struct RaBoundParameter
{
    void*               handle;   // +0x00  the D3DXHANDLE of the member, by name
    D3DXPARAMETER_DESC  desc;     // +0x04  GetParameterDesc's answer for that member
    int                 offset;   // +0x30  the registry index, or 0 when no registry knew it
    vc71::string        name;     // +0x34  the member's name
};

#if defined(_M_IX86)
static_assert(sizeof(RaBoundParameter) == 0x50,
              "sub_1004B4E0's clear loop strides 0x50 and destroys a string at +0x34");
#endif

class RaShader
{
public:
    // sub_1004BDC0, PE 0x1004BDC0.  `queue` is the 0x2B0 render queue the caller has just built.
    RaShader(const vc71::string& name, void* queue);

    const vc71::string& name() const { return m_name; }
    void*               queue() const { return m_queue; }

    // off_101E2958 slot +0x04 - the only slot either lookup body dispatches.  Kept as the RAW
    // dispatcher it is in the image: both lookups reach the body through the vtable, never directly.
    bool slot04();

    // sub_1004AFD0, the cached int parameter "ShaderVersion" of the compiled effect
    // (+0x138, re-read while +0x134 is set; a missing effect is built through the +0x04 slot first
    // and, still missing, answers 0 with the re-read left pending). Terrain::drawWater selects its
    // water arm on it. No shipped shader defines the parameter, so it reads 0 on retail content.
    int* shaderVersion();

    // sub_1004B4E0, PE 0x1004B4E0, 2262 bytes - what slot +0x04 actually runs.  Compiles/fetches
    // the effect, picks the technique, measures the bound pixel-shader model, rebuilds the required
    // vertex-element list and re-binds every .fx struct member.  The name is [INFERRED]; the body
    // and its signature (`__thiscall`, no stack arguments, bool in al) are read.
    bool build();

    // sub_1004B060, PE 0x1004B060.  "shaders/rashader" + name + ".fx" -> Ra::EffectManager.
    // [INFERRED] name; `__thiscall`, no stack arguments, the ID3DXEffect in eax.
    void* loadEffect();

    // Builds if needed, selects a technique, and applies global parameters for entry.
    // Returns the pass count, or zero when no effect is available.
    unsigned int begin(RenderQueueEntry* entry);
    // +0x0C, sub_1004AEA0: End, no active Ra effect, pop the state manager - all effect-gated.
    void end();
    // +0x10, sub_1004B340: the instance parameter set, then the entry's clip planes onto the device.
    void applyInstanceParameters(RenderQueueEntry* entry);
    // +0x14, sub_1004B280: the template parameter set (effect-gated).
    void applyTemplateParameters(RenderQueueEntry* entry);
    // +0x18, sub_1004AF30: the state manager's pre-draw apply, then the device's
    // DrawIndexedPrimitive from the material's getters; nothing for a material with no primitives.
    void draw(RenderQueueEntry* entry);
    // sub_1004AEE0 / sub_1004AF10 - BeginPass / EndPass, effect-gated.
    void beginPass(unsigned int pass);
    void endPass();
    // sub_1004B3D0: the technique for `key` (raHashString of a technique name) from the map,
    // filled on first use (sub_1004B2A0); false and nothing changed on a miss.
    bool selectTechnique(unsigned int key);
    void fillTechniqueMap();
    // sub_1004B210: one five-vector set against the entry's blocks - material, object, the
    // process-wide constants, light array, lighting, in that order.
    void applyParameterSet(vc71::vector_layout* set, RenderQueueEntry* entry);
    // sub_1004D280: every bound parameter of one vector, resolved through the block's registry to
    // its holder and dispatched through the holder's slot 0 (the typed apply).
    void applyParameterVector(void* block, const vc71::vector_layout& parameters);

    void*                m_vptr;                     // +0x000
    void*                m_queue;                    // +0x004
    vc71::vector_layout  m_instanceParameters[5];    // +0x008 .. +0x057
    vc71::vector_layout  m_globalParameters[5];      // +0x058 .. +0x0A7
    vc71::vector_layout  m_templateParameters[5];    // +0x0A8 .. +0x0F7
    vc71::vector_layout  m_vertexElementDescriptors; // +0x0F8
    vc71::vector_layout  m_vertexElementNames;       // +0x108
    vc71::string         m_name;                     // +0x118
    unsigned char        m_byte134;                  // +0x134  ctor: 1; build: 1
    unsigned char        m_pad135[3];
    unsigned int         m_dword138;                 // +0x138  ctor: 0
    vc71::vector_layout  m_requiredVertexElements;   // +0x13C
    IDirect3DDevice9*    m_device;                   // +0x14C
    float                m_float150;                 // +0x150  ctor: 1.0f
    void*                m_queueProfiler;            // +0x154  ctor: 0; the draw walk stores the
                                                     //         queue's +0x10 bracket here before begin
    RaTechniqueMap       m_techniques;               // +0x158
    void*                m_activeTechnique;          // +0x164  what begin hands SetTechnique: the
                                                     //         key's technique, else m_technique
    void*                m_technique;                // +0x168  the D3DXHANDLE
    void*                m_effect;                   // +0x16C  the ID3DXEffect
    unsigned char        m_built;                    // +0x170  ctor: 0; build: 1
    unsigned char        m_pad171[3];
    int                  m_buildSerial;              // +0x174  ctor: -1
};

#if defined(_M_IX86)
static_assert(sizeof(RaShader) == 0x178,
              "sub_10041C70 engine-allocates exactly 0x178 for it");
static_assert(offsetof(RaShader, m_instanceParameters) == 0x008, "instance array at +0x08");
static_assert(offsetof(RaShader, m_globalParameters)   == 0x058, "global array at +0x58");
static_assert(offsetof(RaShader, m_templateParameters) == 0x0A8, "template array at +0xA8");
static_assert(offsetof(RaShader, m_vertexElementDescriptors) == 0x0F8, "descriptors at +0xF8");
static_assert(offsetof(RaShader, m_vertexElementNames)       == 0x108, "names at +0x108");
static_assert(offsetof(RaShader, m_requiredVertexElements)   == 0x13C, "required list at +0x13C");
static_assert(offsetof(RaShader, m_technique)   == 0x168, "technique handle at +0x168");
static_assert(offsetof(RaShader, m_effect)      == 0x16C, "effect at +0x16C");
static_assert(offsetof(RaShader, m_built)       == 0x170, "built flag at +0x170");
static_assert(offsetof(RaShader, m_buildSerial) == 0x174, "build serial at +0x174");
#endif


// Shared layout view of the queue table's default shader, name cache and queue collections.

struct RaShaderTable
{
    unsigned char       opaque000[0x84];   // vptr + the thirty-two render-queue slots
    RaShader*           defaultShader;     // +0x084
    vc71::map_layout    shaders;           // +0x088
    vc71::vector_layout worldQueues;       // +0x094
    vc71::vector_layout pointLightQueues;  // +0x0A4
    unsigned char       byteB4;            // +0x0B4
    unsigned char       padB5[3];
};

#if defined(_M_IX86)
static_assert(sizeof(RaShaderTable) == 0xB8,
              "sub_10042CA0 allocates 0xB8 for dword_1023E408");
#endif

// sub_10041F20, the outer entry every Ra technique lookup goes through.
RaShader* getShader(RaShaderTable* table, const vc71::string& name, bool skipFileCheck);

// sub_10041C70, find-or-create.  `skipFileCheck` true means "build it even if
// shaders/rashader<name>.fx is not on disk".
RaShader* findOrCreateShader(RaShaderTable* table, const vc71::string& name, bool skipFileCheck);

// sub_10042150, the table's full "load every shaders/* file" pass.  Lives in
// Ra/EffectManager.cpp because every other call it makes is a member of the EffectManager.
// Bit 0 registers the .mfx metaShaders, bit 1 creates an Ra shader per rashader*.fx, bit 2
// builds each one eagerly.  Renderer::handleLevelPreLoad passes 3.  Returns the number of Ra
// shaders created; every shipped caller discards it.
int raLoadAllShaders(RaShaderTable* table, unsigned char flags);


// Parameter-block constructors publish name/offset registries once per process.
// Shared callers provide storage and pair construction with destruction.
void raLightingParametersConstruct(void* storage);
void raLightingParametersDestroy(void* storage);

// sub_100458E0 / sub_1004D900 - 0x224 bytes.  The per-object transform / wind / hemi block.
void raObjectParametersConstruct(void* storage);
void raObjectParametersDestroy(void* storage);

// sub_10044F70 - 0x1C bytes.  The light-array block; its destructor is inlined at every call site
// (one store, republishing the +0x08 holder vptr), so there is no separate destroy entry point.
void raLightArrayParametersConstruct(void* storage);
void raLightArrayParametersDestroyInline(void* storage);

unsigned int raLightingParametersSize();
unsigned int raObjectParametersSize();
unsigned int raLightArrayParametersSize();


// Shared helpers implemented by the owning renderer translation units.


// RenderQueues.cpp - sub_1005F320(0x2B0) + sub_1003B330 ("one render queue, 688 bytes, ctor
// sub_1003B330, vtable off_101E1848"), i.e. its file-local makeQueue() taking the shader's name.
void* renderQueueCreateNamed(const vc71::string& name);

// RenderQueues.cpp - sub_10038BF0 / sub_100388E0, its file-local constructRaShaderConstants() and
// destroyRaShaderConstantsObject().  The shader constructor builds ONE 0xAFC-byte block on its
// stack with the device argument 0xFFFFFFFF and destroys it again.
void* raShaderConstantsConstructInPlace(void* storage, void* device);
void  raShaderConstantsDestroyInPlace(void* object);

// Code/BF2/Geom/StaticMeshMaterial.cpp - sub_10044270 / sub_10043F00, its file-local
// raParametersConstruct() / raParametersDestroy() for the 0x150-byte material block.
void raMaterialParametersConstruct(void* storage);
void raMaterialParametersDestroy(void* storage);
void raMaterialParametersAssign(void* storage, const void* source);
// sub_10049FB0, shared by StaticMesh and BundledMesh vertex compression.
unsigned int packVertexNormal(const float* normal);


// Borrowed parameter-name registries; indices select fields through the matching
// registry's offset table. Lookup checks material, lighting, object, shader constants,
// then light-array names, with later matches taking precedence.
const vc71::vector_layout& raMaterialParameterNames();

// RendDX9/RenderQueues.cpp - its file-local `g_raConstantNames` (dword_1023DDD0).
const vc71::vector_layout& raShaderConstantsParameterNames();


// Lighting parameters for lightmaps, bones, and shadow cascades.
// The holder at +0x08 has no shader-visible name.
struct RaLightingParameters
{
    void*        vptr;                 // +0x000  off_101E1EB4
    const void*  registry;             // +0x004  &unk_1023E790 - this block's offset vector
    RaParamVec4  unnamed008;           // +0x008  off_101E1480, 1,1,1,1  (no registered name)
    RaParamRef   terrainLightMap;      // +0x01C  off_101E1488            "TerrainLightMap"      28
    RaParamRef   lightMap;             // +0x024  off_101E1488            "LightMap"             36
    RaParamVec4  lightMapOffset;       // +0x02C  off_101E1480, 1,1,1,1   "LightMapOffset"       44
    RaParamPair  matBones;             // +0x040  off_101E1E8C, 0, 0      "MatBones"             64
    RaParamVec4  overGrowthAmbient;    // +0x04C  off_101E1480, 1,1,1,1   "OverGrowthAmbient"    76
    RaParamMat4  shadowViewProj;       // +0x060  off_101E1E94, identity  "ShadowViewProj"       96
    RaParamVector shadowViewProjArray; // +0x0A4  off_101E1EA4, stride 64 "ShadowViewProjArray" 164
    RaParamVector clampViewProjArray;  // +0x0B8  off_101E1EA4, stride 64 "ClampViewProjArray"  184
    RaParamVector shadowPosArray;      // +0x0CC  off_101E1EAC, stride 16 "ShadowPosArray"      204
    RaParamVector shadowSizeArray;     // +0x0E0  off_101E1EAC, stride 16 "ShadowSizeArray"     224
    RaParamInt   shadowCount;          // +0x0F4  off_101E14A8, 0         "ShadowCount"         244
    vc71::vector_layout tail0FC;       // +0x0FC  the destructor frees its _First
    unsigned int tail10C;              // +0x10C  the render-entry pool zeroes it on every acquire
};

#if defined(_M_IX86)
static_assert(sizeof(RaLightingParameters) == 0x110,
              "sub_1004BDC0's stack slot for it spans ebp-0x148 .. ebp-0x38");
#endif

// block 2 - sub_100458E0 / sub_1004D900.  0x224 bytes.  The per-object transform, wind and hemi
// block; six identity matrices and nine scalar/vector holders.  A StaticMesh instance embeds one
// at its +0x38 (Geom/StaticMesh.h), and its draw writes world, viewProjection,
// worldViewProjection, shadowProjMat, shadowTrapMat, objectSpaceCamPos, windSpeed,
// objectSpaceWorldUp and tail220 by name.
struct RaObjectParameters
{
    void*        vptr;                 // +0x000  off_101E2210
    const void*  registry;             // +0x004  &unk_1023E7D0
    RaParamMat4  world;                // +0x008  off_101E1E94, identity  "World"                 8
    RaParamMat4  viewProjection;       // +0x04C  off_101E1E94, identity  "ViewProjection"       76
    RaParamMat4  worldViewProjection;  // +0x090  off_101E1E94, identity  "WorldViewProjection" 144
    RaParamMat4  shadowProjMat;        // +0x0D4  off_101E1E94, identity  "ShadowProjMat"       212
    RaParamMat4  shadowOccProjMat;     // +0x118  off_101E1E94, identity  "ShadowOccProjMat"    280
    RaParamMat4  shadowTrapMat;        // +0x15C  off_101E1E94, identity  "ShadowTrapMat"       348
    RaParamPair  geomBones;            // +0x1A0  off_101E1E8C, 0, 0      "GeomBones"           416
    RaParamFloat transparency;         // +0x1AC  off_101E1E80, 1.0f      "Transparency"        428
    RaParamVec4  objectSpaceCamPos;    // +0x1B4  off_101E1480, 1,1,1,1   "ObjectSpaceCamPos"   436
    RaParamFloat windSpeed;            // +0x1C8  off_101E148C, 0         "WindSpeed"           456
    RaParamVec3  windDir;              // +0x1D0  off_101E1494, 0,0,0     "WindDir"             464
    RaParamVector userData;            // +0x1E0  off_101E1E9C, bytes     "UserData"            480
    RaParamFloat heightOverTerrain;    // +0x1F4  off_101E148C, 0         "HeightOverTerrain"   500
    RaParamVec4  hemiMapSkyColor;      // +0x1FC  off_101E1480, 1,1,1,1   "HemiMapSkyColor"     508
    RaParamVec3  objectSpaceWorldUp;   // +0x210  off_101E1494, 0,0,0     "ObjectSpaceWorldUp"  528
    // +0x220.  NOT a holder and not registered: the StaticMesh draw (0x100FC7D0) stores the
    // instance's camera distance here as a float, and no recovered body reads it back.  Kept as
    // the raw word the constructor leaves alone.
    unsigned int tail220;              // +0x220
};

#if defined(_M_IX86)
static_assert(sizeof(RaObjectParameters) == 0x224,
              "sub_1004BDC0's stack slot for it spans ebp-0x4BC .. ebp-0x298");
#endif

// block 3 - sub_10044F70.  0x1C bytes.  The light-array block; its destructor is inlined at
// every call site (one store, republishing the +0x08 holder vptr).
struct RaLightArrayParameters
{
    void*       vptr;        // +0x00  off_101E2210
    const void* registry;    // +0x04  &unk_1023E770
    RaParamRef  lights;      // +0x08  off_101DD3FC, 0          "Lights"      8   -> one RaLightRecord
    void*       lights1xVptr;// +0x10  off_101E1E88             "Lights_1X"  16
    void*       lights1xSelf;// +0x14  this + 8
    void*       lights1xFn;  // +0x18  sub_10046660
};

#if defined(_M_IX86)
static_assert(sizeof(RaLightArrayParameters) == 0x1C,
              "sub_1004BDC0's stack slot for it spans ebp-0x38 .. ebp-0x1C");
#endif


// Object-space light record shared by render entries and mesh light arrays.
// Attenuation at +0x38 is left for the writer; colour's fourth component is not padding.

struct RaLightRecord
{
    float position[3];    // +0x00
    float direction[3];   // +0x0C
    float color[3];       // +0x18
    float scaled24;       // +0x24  scaled together with color by the draw; role [UNRESOLVED]
    float unread28[4];    // +0x28  zeroed by the constructor, never read by a recovered body
    float invRangeSq;     // +0x38  not written by the constructor
    float float3C;        // +0x3C  1.0f
    float float40;        // +0x40  1.0f
};

#if defined(_M_IX86)
static_assert(sizeof(RaLightRecord) == 0x44, "sub_100D1460 allocates 0x44 for it");
static_assert(offsetof(RaLightRecord, invRangeSq) == 0x38, "the draw stores 1/r^2 at +0x38");
#endif

// sub_10046540, PE 0x10046540.  Defined in RaShader.cpp beside the block constructors.
RaLightRecord* raLightRecordConstruct(void* storage);

// The parameter holder vtables, one per shipped table, each with the typed apply in slot 0
// (`__thiscall apply(ID3DXEffect*, D3DXHANDLE)`, ) and the unread slots still
// stopping by name.  Defined in RaShader.cpp; the three block constructors here and the material
// block's in Geom/StaticMeshMaterial.cpp install them field by field.
void* raHolderVec4();                 // off_101E1480  SetVector
void* raHolderTexture();              // off_101E1488  SetTexture of a raw base texture
void* raHolderFloat();                // off_101E148C  SetFloat
void* raHolderVec3();                 // off_101E1494  SetFloatArray(3)
void* raHolderVec2();                 // off_101E149C  SetFloatArray(2)
void* raHolderInt();                  // off_101E14A8  SetInt
void* raHolderEngineTexture();        // off_101E14BC  SetTexture of an EngineTexture's base, null-skipped
void* raHolderBool();                 // off_101E1E78  SetBool
void* raHolderFloatSplat();           // off_101E1E80  SetVector of {f, f, f, f}
void* raHolderLightRecordResolved();  // off_101E1E88  SetValue(68) of what the stored resolver answers
void* raHolderMatrixPointerArray();   // off_101E1E8C  SetMatrixPointerArray(pointers, count)
void* raHolderMat4();                 // off_101E1E94  SetMatrix
void* raHolderBytes();                // off_101E1E9C  SetValue of the byte vector
void* raHolderMat4Array();            // off_101E1EA4  SetMatrixArray, 64-byte stride
void* raHolderVec4Array();            // off_101E1EAC  SetVectorArray, 16-byte stride
void* raHolderLightRecordPointer();   // off_101DD3FC  SetValue(68) of the pointed-at record

// sub_10046660 - the "Lights_1X" resolver the light-array block stores at +0x18: the pointed-at
// RaLightRecord copied over a file-scope record with its colour halved.  Defined in RaShader.cpp.
void* __fastcall raLightArrayUpdate(void* lightsValue, void*);


// Vertex-stream requirements used by StaticMeshMaterial::applyStreams.

struct RaStreamRequirements;                     // the shader's still-required elements (a list)
RaStreamRequirements* raStreamRequirementsCreate(const vc71::vector_layout& requiredDescriptors);
bool raStreamRequirementsConsume(RaStreamRequirements* r, const vc71::vector_layout& streamDescriptors);
bool raStreamRequirementsStarving(const RaStreamRequirements* r);
void raStreamRequirementsDescribe(const RaStreamRequirements* r, char* out, unsigned int size);
void raStreamRequirementsDestroy(RaStreamRequirements* r);

// sub_1004A270's 0x58-byte declaration builder.
struct RaDeclarationBuilder
{
    unsigned char       finished;         // +0x00  D3DDECL_END appended once by the finish
    unsigned char       pad01[3];
    unsigned int        stream;           // +0x04  the next stream number
    unsigned int        offset;           // +0x08  the running byte offset within the stream
    unsigned int        usageCounts[15];  // +0x0C  the next UsageIndex per D3DDECLUSAGE
    vc71::vector_layout elements;         // +0x48  vector<D3DVERTEXELEMENT9>
};
void raDeclarationBuilderConstruct(RaDeclarationBuilder& b);
void raDeclarationBuilderAddStream(RaDeclarationBuilder& b, const vc71::vector_layout& streamDescriptors);
const vc71::vector_layout& raDeclarationBuilderFinish(RaDeclarationBuilder& b);
void raDeclarationBuilderDestroy(RaDeclarationBuilder& b);
IDirect3DVertexDeclaration9* raVertexDeclarationCached(const vc71::vector_layout& elements,
                                                        IDirect3DDevice9* device);
IDirect3DDevice9* raRendererDevice();            // the renderer base's +0x0C

} // namespace Ra


// Pooled submission for one material of one mesh instance, with its parameter blocks.

struct RenderQueueEntry
{
    void*                      material;            // +0x000  the StaticMeshMaterial
    void*                      materialParameters;  // +0x004  its Ra block (material + 0x2C)
    Ra::RaLightingParameters   lighting;            // +0x008 .. +0x117  sub_10045240
    void*                      objectParameters;    // +0x118  the instance's RaObjectParameters
    Ra::RaLightArrayParameters lightArray;          // +0x11C .. +0x137  sub_10044F70; lights.value
                                                    //         -> the 0x44 record the ctor allocates
    Ra::RaShader*              shader;              // +0x138  the technique the queue draws with
    unsigned int               techniqueKey;        // +0x13C  zeroed on every acquire; the default
                                                    //         walk stamps the queue's shared key here
                                                    //         (raHashString of "draw_1p") and
                                                    //         RaShader::begin selects the technique
};

#if defined(_M_IX86)
static_assert(sizeof(RenderQueueEntry) == 0x140, "sub_100D13E0 allocates 0x140 per entry");
static_assert(offsetof(RenderQueueEntry, objectParameters) == 0x118, "entry+0x118");
static_assert(offsetof(RenderQueueEntry, lightArray) == 0x11C, "entry+0x11C");
static_assert(offsetof(RenderQueueEntry, shader) == 0x138, "entry+0x138");
#endif

// RenderQueues.cpp - the pool's owner defines all three.

// sub_100D13E0 on dword_1023DE08: the next unused entry this frame, growing the pool by one
// freshly constructed entry when it runs dry.  Zeroes lighting.tail10C and dword13C on the way out.
RenderQueueEntry* renderEntryPoolAcquire();

// sub_10041A70, PE 0x10041A70.  Route an entry to its queue: bit 0 transparent, bit 1 first-person,
// bit 2 the delta (point-light) pass.  Index 0 and 4 go to the entry's shader's OWN queue; every
// other index is a fixed table queue (RSMTTransparency .. RSMTDelta1PersonTransparency).
void renderQueueEnqueue(RenderQueueEntry* entry, bool transparent, bool firstPerson, bool deltaPass);

// The direct form the StaticMesh draw uses for its road and water materials: the table queue at
// `slots[slotIndex]` (8 = RSMTRoad at +0x24, 9 = RSMTWater at +0x28), through the queue's own add
// slot.  Nothing but the index differs from what renderQueueEnqueue does for its table queues.
void renderQueueAddToSlot(unsigned int slotIndex, RenderQueueEntry* entry);

// sub_1003ABB0, the case-folded string hash that keys every Ra technique map and
// every queue's shared string.  Defined in RenderQueues.cpp.
unsigned int raHashString(const vc71::string& s);

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
#endif // BF2_RENDDX9_RA_RASHADER_H
