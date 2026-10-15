// Static-mesh material lifecycle, stream construction and material-list operations.
// [CHOSEN] split from StaticMeshTemplate.cpp.
// Preserve the shared nine-slot material interface. Destruction omits the final abstract-base
// vptr store because the corresponding base type is not represented here.


#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include <string>
#include "../RendDX9/Texture.h"
#include "../RendDX9/Ra/RaShader.h"   // the twenty vertex-element descriptors
#include "StaticMeshTemplate.h"
#include "../Game/Main/IVideoOptions.h"   // setupTechnique reads getQuality(2)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <math.h>
#include <new>
#include <set>
#include <string.h>

namespace dice {
namespace hfe {

namespace dx9rend {
// BufferManager.cpp - declared here the way MeshTemplate.cpp declares the same three entry points.
extern void* g_bufferManager;                                              // dword_1023B978
void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const std::string& name, unsigned int usage);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexDeclarationRelease(void* wrapper);                             // the 0x10014790 dance
// RendDX9Init.cpp - dword_1023B980, the DeviceCaps record. buildVertexBuffers needs the feature
// mask at +0xEC and there is no typed accessor for it, so it is read by documented raw offset the
// same way deviceCapsPixelShaderMajor reads +0xCC.
extern void* g_deviceCaps;
// BF2Render.cpp - the two typed Renderer seams StaticMeshMaterial::setupTechnique's
// lightmap gate needs (+0x74 then VideoOptions +0x04, and +0xAC). Declared here exactly
// the way Geom/XpLightManager.cpp declares the same pair.
IVideoOptions* rendererGetVideoOptions();
bool rendererIsExpansionPack();
// BufferManager.cpp - the two stream binds applyStreams makes, declared here the way SkyDome.cpp
// declares them: the wrapper's buffer at offset 0 with its stride (state manager +0x58), the
// index buffer (+0x54).
void vertexBufferBindStream(void* wrapper, unsigned int stream);
void indexBufferBind(void* wrapper);
// BufferManager.cpp - the wrapper fields (+0x08 D3D declaration, +0x20 desc.Size).
unsigned int vertexBufferByteSize(void* wrapper);
IDirect3DVertexDeclaration9* vertexDeclarationD3D(void* wrapper);
} // namespace dx9rend

namespace geom {


// dword_1025B590 / dword_1025B594[] - two file-scope counters this translation unit owns.

//   dword_1025B590  finalizeMaterials accumulates its per-template stream-bytes figure into it.
//                   Read back at 0x10118DE7 (the `+=`) and nowhere else in the image.
//   dword_1025B594  mergeMaterials increments [lod] once per merged pair. write-only: the single
//                   cross-reference in the whole image is the `++` at 0x10119CE9. [INFERRED] extent
//                   of five dwords, bounded by the next named datum at 0x1025B5A8.

namespace {
unsigned int g_staticMeshStreamBytes = 0;          // dword_1025B590
unsigned int g_staticMeshMergedCount[5] = { 0, 0, 0, 0, 0 };   // dword_1025B594
} // anonymous namespace


// Material parameter storage has the retail 0x150-byte layout on x86 (RaMaterialParameters).
// Holder data is available, but dispatch through the unpublished holder classes aborts.
// [RETIRE-WHEN: the Ra material/parameter translation unit is recovered.]
// Replace the diagnostic tables after holder signatures and behavior are implemented and validated.

namespace {

void RD9_THUNK raParameterHolderFrontier(void* self RD9_THUNK_EDX)
{
    RD9_FRONTIER_AT("dice::hfe::dx9rend::Ra parameter holder "
                    "(off_101E1480 / 148C / 149C / 14A8 / 14BC / 1E78 / 1E80 / 2210 - "
                    "the eight unrecovered holder classes of the 0x150-byte material "
                    "parameter block)",
                    self, 0x00);
}

// Eight slots: longer than any of the shipped tables (the longest observed is five), so a dispatch
// through any of them lands on a body that names itself and stops rather than running off the end.
void* const kRaParameterHolderVtable[8] =
{
    reinterpret_cast<void*>(&raParameterHolderFrontier),
    reinterpret_cast<void*>(&raParameterHolderFrontier),
    reinterpret_cast<void*>(&raParameterHolderFrontier),
    reinterpret_cast<void*>(&raParameterHolderFrontier),
    reinterpret_cast<void*>(&raParameterHolderFrontier),
    reinterpret_cast<void*>(&raParameterHolderFrontier),
    reinterpret_cast<void*>(&raParameterHolderFrontier),
    reinterpret_cast<void*>(&raParameterHolderFrontier)
};

void* const* raHolderVtable() { return kRaParameterHolderVtable; }

// The block itself is RaMaterialParameters (MeshTemplate.h); this file assigns its texture holders.
typedef RaMaterialParameters::Texture RaParamTexture;

// Each parameter block stores its own offset-vector address as its descriptor.
// Apply resolves registry indices through this table before accessing holder data.
extern std::vector<int> g_raParameterOffsets;                  // defined a few lines below
const void* const kRaMaterialDescriptor = &g_raParameterOffsets;


// Process-wide parameter names and matching offsets, initialized while the offset vector is empty.

std::vector<std::string> g_raParameterNames;                  // dword_1023E7C0
std::vector<int>          g_raParameterOffsets;                // dword_1023E7B4..BC

struct RaParameterName { const char* name; int offset; };

// Each entry is its holder's offset in the native block; the x86 values are the shipped ones.
#define RA_MATERIAL_OFFSET(member) static_cast<int>(offsetof(RaMaterialParameters, member))
const RaParameterName kRaParameterNames[31] =
{
    { "AlphaBlendEnable",       RA_MATERIAL_OFFSET(alphaBlendEnable)       },
    { "SrcBlend",               RA_MATERIAL_OFFSET(srcBlend)               },
    { "DestBlend",              RA_MATERIAL_OFFSET(destBlend)              },
    { "DepthWrite",             RA_MATERIAL_OFFSET(depthWrite)             },
    { "CullMode",               RA_MATERIAL_OFFSET(cullMode)               },
    { "AlphaTest",              RA_MATERIAL_OFFSET(alphaTest)              },
    { "AlphaTestRef",           RA_MATERIAL_OFFSET(alphaTestRef)           },
    { "DiffuseColor",           RA_MATERIAL_OFFSET(diffuseColor)           },
    { "Ambient",                RA_MATERIAL_OFFSET(ambient)                },
    { "PointColor",             RA_MATERIAL_OFFSET(pointColor)             },
    { "PointSun",               RA_MATERIAL_OFFSET(pointSun)               },
    { "DisableLightmapShadows", RA_MATERIAL_OFFSET(disableLightmapShadows) },
    { "SpecularColor",          RA_MATERIAL_OFFSET(specularColor)          },
    { "SpecularPower",          RA_MATERIAL_OFFSET(specularPower)          },
    { "Opacity",                RA_MATERIAL_OFFSET(opacity)                },
    { "Reflectivity",           RA_MATERIAL_OFFSET(reflectivity)           },
    { "DiffuseMap",             RA_MATERIAL_OFFSET(diffuseMap)             },
    { "DetailMap",              RA_MATERIAL_OFFSET(detailMap)              },
    { "NormalMap",              RA_MATERIAL_OFFSET(normalMap)              },
    { "CubeMap",                RA_MATERIAL_OFFSET(cubeMap)                },
    { "DirtMap",                RA_MATERIAL_OFFSET(dirtMap)                },
    { "CrackMap",               RA_MATERIAL_OFFSET(crackMap)               },
    { "CrackNormalMap",         RA_MATERIAL_OFFSET(crackNormalMap)         },
    { "ParallaxScaleBias",      RA_MATERIAL_OFFSET(parallaxScaleBias)      },
    { "GIMap",                  RA_MATERIAL_OFFSET(giMap)                  },
    { "StaticGloss",            RA_MATERIAL_OFFSET(staticGloss)            },
    { "ObjRadius",              RA_MATERIAL_OFFSET(objRadius)              },
    { "InvHemiHeightScale",     RA_MATERIAL_OFFSET(invHemiHeightScale)     },
    { "PosUnpack",              RA_MATERIAL_OFFSET(posUnpack)              },
    { "TexUnpack",              RA_MATERIAL_OFFSET(texUnpack)              },
    { "NormalUnpack",           RA_MATERIAL_OFFSET(normalUnpack)           }
};

#if defined(_M_IX86)
static_assert(RA_MATERIAL_OFFSET(alphaBlendEnable) == 8 && RA_MATERIAL_OFFSET(srcBlend) == 168 &&
              RA_MATERIAL_OFFSET(destBlend) == 176 && RA_MATERIAL_OFFSET(depthWrite) == 16 &&
              RA_MATERIAL_OFFSET(cullMode) == 152 && RA_MATERIAL_OFFSET(alphaTest) == 24 &&
              RA_MATERIAL_OFFSET(alphaTestRef) == 160 && RA_MATERIAL_OFFSET(diffuseColor) == 32 &&
              RA_MATERIAL_OFFSET(ambient) == 52 && RA_MATERIAL_OFFSET(pointColor) == 72 &&
              RA_MATERIAL_OFFSET(pointSun) == 92 && RA_MATERIAL_OFFSET(disableLightmapShadows) == 100 &&
              RA_MATERIAL_OFFSET(specularColor) == 108 && RA_MATERIAL_OFFSET(specularPower) == 128 &&
              RA_MATERIAL_OFFSET(opacity) == 136 && RA_MATERIAL_OFFSET(reflectivity) == 144 &&
              RA_MATERIAL_OFFSET(diffuseMap) == 184 && RA_MATERIAL_OFFSET(detailMap) == 192 &&
              RA_MATERIAL_OFFSET(normalMap) == 216 && RA_MATERIAL_OFFSET(cubeMap) == 284 &&
              RA_MATERIAL_OFFSET(dirtMap) == 200 && RA_MATERIAL_OFFSET(crackMap) == 208 &&
              RA_MATERIAL_OFFSET(crackNormalMap) == 224 && RA_MATERIAL_OFFSET(parallaxScaleBias) == 264 &&
              RA_MATERIAL_OFFSET(giMap) == 232 && RA_MATERIAL_OFFSET(staticGloss) == 240 &&
              RA_MATERIAL_OFFSET(objRadius) == 248 && RA_MATERIAL_OFFSET(invHemiHeightScale) == 256 &&
              RA_MATERIAL_OFFSET(posUnpack) == 292 && RA_MATERIAL_OFFSET(texUnpack) == 312 &&
              RA_MATERIAL_OFFSET(normalUnpack) == 320,
              "the shipped material registry offsets (sub_10044270)");
#endif
#undef RA_MATERIAL_OFFSET

// sub_10044270's one-time tail.
void raRegisterParameterNames()
{
    if (!g_raParameterOffsets.empty())
        return;

    for (int i = 0; i < 31; ++i)
    {
        std::string name(kRaParameterNames[i].name);
        g_raParameterNames.push_back(name);
        g_raParameterOffsets.push_back(kRaParameterNames[i].offset);
    }
}

// sub_10044270 - construct. Defaults read straight out of the instruction stream: 1.0f is
// 0x3F800000, 20.0f is 0x41A00000 (1101004800) and 0.1f is 0x3DCCCCCD (1045220557).
void raParametersConstruct(void* storage)
{
    RaMaterialParameters* const p = static_cast<RaMaterialParameters*>(storage);
    void* const holder = const_cast<void*>(static_cast<const void*>(raHolderVtable()));

    p->descriptor = kRaMaterialDescriptor;
    p->vptr       = holder;

    p->alphaBlendEnable.vptr = dx9rend::Ra::raHolderBool();  p->alphaBlendEnable.value = 0;
    p->depthWrite.vptr       = dx9rend::Ra::raHolderBool();  p->depthWrite.value       = 1;
    p->alphaTest.vptr        = dx9rend::Ra::raHolderBool();  p->alphaTest.value        = 0;

    p->diffuseColor.vptr = dx9rend::Ra::raHolderVec4();
    p->diffuseColor.value[0] = 1.0f; p->diffuseColor.value[1] = 1.0f;
    p->diffuseColor.value[2] = 1.0f; p->diffuseColor.value[3] = 1.0f;

    p->ambient.vptr = dx9rend::Ra::raHolderVec4();
    p->ambient.value[0] = 1.0f; p->ambient.value[1] = 1.0f;
    p->ambient.value[2] = 1.0f; p->ambient.value[3] = 1.0f;

    p->pointColor.vptr = dx9rend::Ra::raHolderVec4();
    p->pointColor.value[0] = 0.0f; p->pointColor.value[1] = 0.0f;
    p->pointColor.value[2] = 0.0f; p->pointColor.value[3] = 0.0f;

    // Shipped order: the value at index 24 is written before the vptr at index 23, and likewise for
    // the pair at 25/26. Kept.
    p->pointSun.value = 0;               p->pointSun.vptr = dx9rend::Ra::raHolderInt();
    p->disableLightmapShadows.value = 0.0f; p->disableLightmapShadows.vptr = dx9rend::Ra::raHolderFloat();

    p->specularColor.vptr = dx9rend::Ra::raHolderVec4();
    p->specularColor.value[0] = 1.0f; p->specularColor.value[1] = 1.0f;
    p->specularColor.value[2] = 1.0f; p->specularColor.value[3] = 1.0f;

    p->specularPower.vptr = dx9rend::Ra::raHolderFloat();  p->specularPower.value = 20.0f;
    p->opacity.vptr       = dx9rend::Ra::raHolderFloat();  p->opacity.value       = 1.0f;
    p->reflectivity.vptr  = dx9rend::Ra::raHolderFloat();  p->reflectivity.value  = 0.0f;

    p->cullMode.vptr     = dx9rend::Ra::raHolderInt();  p->cullMode.value     = 3;
    p->alphaTestRef.vptr = dx9rend::Ra::raHolderInt();  p->alphaTestRef.value = 0;
    p->srcBlend.vptr     = dx9rend::Ra::raHolderInt();  p->srcBlend.value     = 5;
    p->destBlend.vptr    = dx9rend::Ra::raHolderInt();  p->destBlend.value    = 6;

    p->diffuseMap.value    = 0; p->diffuseMap.vptr     = dx9rend::Ra::raHolderEngineTexture();
    p->detailMap.vptr      = dx9rend::Ra::raHolderEngineTexture(); p->detailMap.value     = 0;
    p->dirtMap.vptr        = dx9rend::Ra::raHolderEngineTexture(); p->dirtMap.value       = 0;
    p->crackMap.vptr       = dx9rend::Ra::raHolderEngineTexture(); p->crackMap.value      = 0;
    p->normalMap.vptr      = dx9rend::Ra::raHolderEngineTexture(); p->normalMap.value     = 0;
    p->crackNormalMap.vptr = dx9rend::Ra::raHolderEngineTexture(); p->crackNormalMap.value= 0;
    p->giMap.vptr          = dx9rend::Ra::raHolderEngineTexture(); p->giMap.value         = 0;

    p->staticGloss.vptr        = dx9rend::Ra::raHolderFloatSplat(); p->staticGloss.value        = 0.1f;
    p->objRadius.vptr          = dx9rend::Ra::raHolderFloat(); p->objRadius.value          = 0.0f;
    p->invHemiHeightScale.vptr = dx9rend::Ra::raHolderFloat(); p->invHemiHeightScale.value = 0.0f;

    p->parallaxScaleBias.vptr = dx9rend::Ra::raHolderVec4();
    p->parallaxScaleBias.value[0] = 1.0f; p->parallaxScaleBias.value[1] = 1.0f;
    p->parallaxScaleBias.value[2] = 1.0f; p->parallaxScaleBias.value[3] = 1.0f;

    p->cubeMap.vptr = dx9rend::Ra::raHolderEngineTexture(); p->cubeMap.value = 0;

    p->posUnpack.vptr = dx9rend::Ra::raHolderVec4();
    p->posUnpack.value[0] = 1.0f; p->posUnpack.value[1] = 1.0f;
    p->posUnpack.value[2] = 1.0f; p->posUnpack.value[3] = 1.0f;

    p->texUnpack.vptr = dx9rend::Ra::raHolderFloat(); p->texUnpack.value = 0.0f;

    p->normalUnpack.vptr = dx9rend::Ra::raHolderVec2();
    p->normalUnpack.value[0] = 0.0f;
    p->normalUnpack.value[1] = 0.0f;
    p->normalUnpack.value[2] = 0.0f;
#if !defined(_M_IX86)
    p->shader = 0;                       // x86 clears it with value[2] above
#endif

    raRegisterParameterNames();
}

// The texture-holder release the destructor performs eight times, inline: the same explicit
// `if (t && !--t->refCount)` pattern the rest of this subsystem uses.
void raReleaseTexture(void*& slot)
{
    ::dice::hfe::dx9rend::EngineTexture* const tex =
        static_cast<::dice::hfe::dx9rend::EngineTexture*>(slot);
    if (tex && --tex->refCount == 0)
    {
        ::dice::hfe::dx9rend::textureDtor(tex);
        ::renddx9abi::engineFree(tex);
    }
}

// sub_10043F00 - destroy. It republishes every holder's vtable on the way out (the compiler's
// base-class unwind) and releases the eight texture references in the shipped order: CubeMap first,
// then GIMap, CrackNormalMap, NormalMap, CrackMap, DirtMap, DetailMap and DiffuseMap.
void raParametersDestroy(void* storage)
{
    RaMaterialParameters* const p = static_cast<RaMaterialParameters*>(storage);
    void* const holder = const_cast<void*>(static_cast<const void*>(raHolderVtable()));

    p->vptr = holder;
    p->normalUnpack.vptr = dx9rend::Ra::raHolderVec2();
    p->texUnpack.vptr    = dx9rend::Ra::raHolderFloat();
    p->posUnpack.vptr    = dx9rend::Ra::raHolderVec4();
    raReleaseTexture(p->cubeMap.value);
    p->cubeMap.vptr = dx9rend::Ra::raHolderEngineTexture();

    p->parallaxScaleBias.vptr    = dx9rend::Ra::raHolderVec4();
    p->invHemiHeightScale.vptr   = dx9rend::Ra::raHolderFloat();
    p->objRadius.vptr            = dx9rend::Ra::raHolderFloat();
    p->staticGloss.vptr          = dx9rend::Ra::raHolderFloatSplat();
    raReleaseTexture(p->giMap.value);            p->giMap.vptr          = dx9rend::Ra::raHolderEngineTexture();
    raReleaseTexture(p->crackNormalMap.value);   p->crackNormalMap.vptr = dx9rend::Ra::raHolderEngineTexture();
    raReleaseTexture(p->normalMap.value);        p->normalMap.vptr      = dx9rend::Ra::raHolderEngineTexture();
    raReleaseTexture(p->crackMap.value);         p->crackMap.vptr       = dx9rend::Ra::raHolderEngineTexture();
    raReleaseTexture(p->dirtMap.value);          p->dirtMap.vptr        = dx9rend::Ra::raHolderEngineTexture();
    raReleaseTexture(p->detailMap.value);        p->detailMap.vptr      = dx9rend::Ra::raHolderEngineTexture();
    raReleaseTexture(p->diffuseMap.value);       p->diffuseMap.vptr     = dx9rend::Ra::raHolderEngineTexture();

    p->destBlend.vptr    = dx9rend::Ra::raHolderInt();
    p->srcBlend.vptr     = dx9rend::Ra::raHolderInt();
    p->alphaTestRef.vptr = dx9rend::Ra::raHolderInt();
    p->cullMode.vptr     = dx9rend::Ra::raHolderInt();
    p->pointSun.vptr     = dx9rend::Ra::raHolderInt();
    p->reflectivity.vptr = dx9rend::Ra::raHolderFloat();
    p->opacity.vptr      = dx9rend::Ra::raHolderFloat();
    p->specularPower.vptr= dx9rend::Ra::raHolderFloat();
    p->specularColor.vptr= dx9rend::Ra::raHolderVec4();
    p->disableLightmapShadows.vptr = dx9rend::Ra::raHolderFloat();
    p->pointColor.vptr   = dx9rend::Ra::raHolderVec4();
    p->ambient.vptr      = dx9rend::Ra::raHolderVec4();
    p->diffuseColor.vptr = dx9rend::Ra::raHolderVec4();
    p->alphaTest.vptr    = dx9rend::Ra::raHolderBool();
    p->depthWrite.vptr   = dx9rend::Ra::raHolderBool();
    p->alphaBlendEnable.vptr = dx9rend::Ra::raHolderBool();
}

// Copies texture pointers without retaining them; destroying both copies over-releases textures.
void raParametersCopyConstruct(void* storage, const void* source)
{
    RaMaterialParameters* const p = static_cast<RaMaterialParameters*>(storage);
    const RaMaterialParameters* const s = static_cast<const RaMaterialParameters*>(source);
    void* const holder = const_cast<void*>(static_cast<const void*>(raHolderVtable()));

    p->vptr       = holder;
    p->descriptor = s->descriptor;

    p->alphaBlendEnable.vptr = dx9rend::Ra::raHolderBool(); p->alphaBlendEnable.value = s->alphaBlendEnable.value;
    p->depthWrite.vptr       = dx9rend::Ra::raHolderBool(); p->depthWrite.value       = s->depthWrite.value;
    p->alphaTest.vptr        = dx9rend::Ra::raHolderBool(); p->alphaTest.value        = s->alphaTest.value;

    p->diffuseColor.vptr = dx9rend::Ra::raHolderVec4();
    for (int i = 0; i < 4; ++i) p->diffuseColor.value[i] = s->diffuseColor.value[i];
    p->ambient.vptr = dx9rend::Ra::raHolderVec4();
    for (int i = 0; i < 4; ++i) p->ambient.value[i] = s->ambient.value[i];
    p->pointColor.vptr = dx9rend::Ra::raHolderVec4();
    for (int i = 0; i < 4; ++i) p->pointColor.value[i] = s->pointColor.value[i];

    p->pointSun.vptr = dx9rend::Ra::raHolderInt();               p->pointSun.value = s->pointSun.value;
    p->disableLightmapShadows.vptr = dx9rend::Ra::raHolderFloat(); p->disableLightmapShadows.value =
                                                 s->disableLightmapShadows.value;

    p->specularColor.vptr = dx9rend::Ra::raHolderVec4();
    for (int i = 0; i < 4; ++i) p->specularColor.value[i] = s->specularColor.value[i];

    p->specularPower.vptr = dx9rend::Ra::raHolderFloat(); p->specularPower.value = s->specularPower.value;
    p->opacity.vptr       = dx9rend::Ra::raHolderFloat(); p->opacity.value       = s->opacity.value;
    p->reflectivity.vptr  = dx9rend::Ra::raHolderFloat(); p->reflectivity.value  = s->reflectivity.value;
    p->cullMode.vptr      = dx9rend::Ra::raHolderInt(); p->cullMode.value      = s->cullMode.value;
    p->alphaTestRef.vptr  = dx9rend::Ra::raHolderInt(); p->alphaTestRef.value  = s->alphaTestRef.value;
    p->srcBlend.vptr      = dx9rend::Ra::raHolderInt(); p->srcBlend.value      = s->srcBlend.value;
    p->destBlend.vptr     = dx9rend::Ra::raHolderInt(); p->destBlend.value     = s->destBlend.value;

    p->diffuseMap.vptr     = dx9rend::Ra::raHolderEngineTexture(); p->diffuseMap.value     = s->diffuseMap.value;
    p->detailMap.vptr      = dx9rend::Ra::raHolderEngineTexture(); p->detailMap.value      = s->detailMap.value;
    p->dirtMap.vptr        = dx9rend::Ra::raHolderEngineTexture(); p->dirtMap.value        = s->dirtMap.value;
    p->crackMap.vptr       = dx9rend::Ra::raHolderEngineTexture(); p->crackMap.value       = s->crackMap.value;
    p->normalMap.vptr      = dx9rend::Ra::raHolderEngineTexture(); p->normalMap.value      = s->normalMap.value;
    p->crackNormalMap.vptr = dx9rend::Ra::raHolderEngineTexture(); p->crackNormalMap.value = s->crackNormalMap.value;
    p->giMap.vptr          = dx9rend::Ra::raHolderEngineTexture(); p->giMap.value          = s->giMap.value;

    p->staticGloss.vptr        = dx9rend::Ra::raHolderFloatSplat(); p->staticGloss.value        = s->staticGloss.value;
    p->objRadius.vptr          = dx9rend::Ra::raHolderFloat(); p->objRadius.value          = s->objRadius.value;
    p->invHemiHeightScale.vptr = dx9rend::Ra::raHolderFloat(); p->invHemiHeightScale.value = s->invHemiHeightScale.value;

    p->parallaxScaleBias.vptr = dx9rend::Ra::raHolderVec4();
    for (int i = 0; i < 4; ++i) p->parallaxScaleBias.value[i] = s->parallaxScaleBias.value[i];

    p->cubeMap.vptr = dx9rend::Ra::raHolderEngineTexture(); p->cubeMap.value = s->cubeMap.value;

    p->posUnpack.vptr = dx9rend::Ra::raHolderVec4();
    for (int i = 0; i < 4; ++i) p->posUnpack.value[i] = s->posUnpack.value[i];

    p->texUnpack.vptr = dx9rend::Ra::raHolderFloat(); p->texUnpack.value = s->texUnpack.value;

    p->normalUnpack.vptr = dx9rend::Ra::raHolderVec2();
    p->normalUnpack.value[0] = s->normalUnpack.value[0];
    p->normalUnpack.value[1] = s->normalUnpack.value[1];
    p->normalUnpack.value[2] = s->normalUnpack.value[2];
#if !defined(_M_IX86)
    p->shader = s->shader;               // x86 copies it with value[2] above
#endif
}

// sub_10110F70 - the counted texture-holder assignment: release the old, take the new, addRef.
void raAssignTextureValue(RaParamTexture& dst, void* incoming)
{
    ::dice::hfe::dx9rend::EngineTexture* const old =
        static_cast<::dice::hfe::dx9rend::EngineTexture*>(dst.value);
    if (old != incoming)
    {
        if (old && --old->refCount == 0)
        {
            ::dice::hfe::dx9rend::textureDtor(old);
            ::renddx9abi::engineFree(old);
        }
        dst.value = incoming;
        if (dst.value)
            ::dice::hfe::dx9rend::textureAddRef(
                static_cast<::dice::hfe::dx9rend::EngineTexture*>(dst.value));
    }
}

void raAssignTexture(RaParamTexture& dst, const RaParamTexture& src)
{
    if (&dst == &src)
        return;
    raAssignTextureValue(dst, src.value);
}

// sub_10111110 - copy-assign. No vtable stores (the objects already carry theirs) and the eight
// texture slots DO go through the counted assignment.
void raParametersAssign(void* storage, const void* source)
{
    RaMaterialParameters* const p = static_cast<RaMaterialParameters*>(storage);
    const RaMaterialParameters* const s = static_cast<const RaMaterialParameters*>(source);

    p->descriptor              = s->descriptor;
    p->alphaBlendEnable.value  = s->alphaBlendEnable.value;
    p->depthWrite.value        = s->depthWrite.value;
    p->alphaTest.value         = s->alphaTest.value;
    for (int i = 0; i < 4; ++i) p->diffuseColor.value[i]   = s->diffuseColor.value[i];
    for (int i = 0; i < 4; ++i) p->ambient.value[i]        = s->ambient.value[i];
    for (int i = 0; i < 4; ++i) p->pointColor.value[i]     = s->pointColor.value[i];
    p->pointSun.value               = s->pointSun.value;
    p->disableLightmapShadows.value = s->disableLightmapShadows.value;
    for (int i = 0; i < 4; ++i) p->specularColor.value[i]  = s->specularColor.value[i];
    p->specularPower.value = s->specularPower.value;
    p->opacity.value       = s->opacity.value;
    p->reflectivity.value  = s->reflectivity.value;
    p->cullMode.value      = s->cullMode.value;
    p->alphaTestRef.value  = s->alphaTestRef.value;
    p->srcBlend.value      = s->srcBlend.value;
    p->destBlend.value     = s->destBlend.value;

    raAssignTexture(p->diffuseMap,     s->diffuseMap);
    raAssignTexture(p->detailMap,      s->detailMap);
    raAssignTexture(p->dirtMap,        s->dirtMap);
    raAssignTexture(p->crackMap,       s->crackMap);
    raAssignTexture(p->normalMap,      s->normalMap);
    raAssignTexture(p->crackNormalMap, s->crackNormalMap);
    raAssignTexture(p->giMap,          s->giMap);

    p->staticGloss.value        = s->staticGloss.value;
    p->objRadius.value          = s->objRadius.value;
    p->invHemiHeightScale.value = s->invHemiHeightScale.value;
    for (int i = 0; i < 4; ++i) p->parallaxScaleBias.value[i] = s->parallaxScaleBias.value[i];
    raAssignTexture(p->cubeMap, s->cubeMap);
    for (int i = 0; i < 4; ++i) p->posUnpack.value[i] = s->posUnpack.value[i];
    p->texUnpack.value       = s->texUnpack.value;
    p->normalUnpack.value[0] = s->normalUnpack.value[0];
    p->normalUnpack.value[1] = s->normalUnpack.value[1];
    p->normalUnpack.value[2] = s->normalUnpack.value[2];
#if !defined(_M_IX86)
    p->shader = s->shader;               // x86 copies it with value[2] above
#endif
}

inline RaMaterialParameters& params(StaticMeshMaterial* m)
{
    return *reinterpret_cast<RaMaterialParameters*>(m->m_parameters);
}
inline const RaMaterialParameters& params(const StaticMeshMaterial* m)
{
    return *reinterpret_cast<const RaMaterialParameters*>(m->m_parameters);
}

} // anonymous namespace


// [CHOSEN, not recovered] Shared forwards let RaShader construct/destroy the material
// parameter block and trigger its one-time parameter-name registration.
} // namespace geom

namespace dx9rend {
namespace Ra {

void raMaterialParametersConstruct(void* storage) { ::dice::hfe::geom::raParametersConstruct(storage); }
void raMaterialParametersDestroy  (void* storage) { ::dice::hfe::geom::raParametersDestroy(storage); }
// Published for SkinnedMeshTemplate.cpp's material copy-assign: sub_10111110 is the same body for
// both material classes (they share the base whose vptr both destructors restore), and it sat in
// this file's anonymous namespace only because StaticMesh was its sole caller.
void raMaterialParametersAssign   (void* storage, const void* source)
{ ::dice::hfe::geom::raParametersAssign(storage, source); }

// Returns material parameter names populated by the block's one-time constructor tail.
// Initialization must precede shader binding; an empty registry silently misbinds offsets.
const std::vector<std::string>& raMaterialParameterNames()
{
    return ::dice::hfe::geom::g_raParameterNames;
}

} // namespace Ra
} // namespace dx9rend

namespace geom {


// StaticMeshMaterial


// The vptr is published by the compiler, the two vectors (+0x04, +0x14) and the declaration map
// (+0x1F8, sub_1011B560) are default-constructed empty; everything else is the shipped store order.
StaticMeshMaterial::StaticMeshMaterial()
{
    m_flags24        = 0;
    m_shaderMaterial = 0;

    raParametersConstruct(m_parameters);

    m_alphaMode      = 0;
    m_vertexStart    = 0;
    m_indexStart     = 0;
    m_primitiveCount = 0;
    m_vertexCount    = 0;

    m_bounds.min.x = 0.0f; m_bounds.min.y = 0.0f; m_bounds.min.z = 0.0f;
    m_bounds.max.x = 0.0f; m_bounds.max.y = 0.0f; m_bounds.max.z = 0.0f;
    m_bounds.originRadius = 0.0f;
    m_bounds.radius       = 0.0f;
    m_bounds.dirty        = 1;      // only the byte; pad21[3] stays as allocated, as shipped
    m_bounds.extent.x = 0.0f; m_bounds.extent.y = 0.0f; m_bounds.extent.z = 0.0f;
    m_bounds.center.x = 0.0f; m_bounds.center.y = 0.0f; m_bounds.center.z = 0.0f;

    // m_dword1D4 and m_dword1D8 are NOT written here. Shipped; the copy constructor still copies
    // them, so an uninitialised value propagates.
    m_vegetationVertices    = 0;
    m_vegetationIndices     = 0;
    m_vegetationVertexCount = 0;
    m_vegetationIndexCount  = 0;
    m_vegetationNonTiled    = 0;
    m_dword1F0 = 0;
    m_template = 0;
}

StaticMeshMaterial::~StaticMeshMaterial()
{
    destroyMembers();
}

// the destructor body. Order is shipped: the map, then the two vegetation buffers,
// then the parameter block, then the 4-byte vector, then the vector-of-vectors. The containers
// are emptied at their shipped points; their storage goes with their own destructors afterwards.

// The shipped tail (`*(_DWORD *)this = &off_101E2428`) is NOT reproduced - see the file header.
void StaticMeshMaterial::destroyMembers()
{
    m_map1F8.clear();                                  // sub_1011E470's erase(begin, end)

    ::renddx9abi::engineFree(m_vegetationIndices);
    ::renddx9abi::engineFree(m_vegetationVertices);
    m_vegetationVertices    = 0;
    m_vegetationIndices     = 0;
    m_vegetationVertexCount = 0;
    m_vegetationIndexCount  = 0;

    raParametersDestroy(m_parameters);

    m_vector14.clear();
    m_vector04.clear();                                // sub_100ED1C0 on each inner list
}

// the copy constructor.

// Copies shader and vegetation pointers without retaining or duplicating their resources.
// Copies the two trailing words even though the default constructor leaves them uninitialized.
// The two vectors are deep copies (sub_1011E740 / sub_1011C4E0).
StaticMeshMaterial::StaticMeshMaterial(const StaticMeshMaterial& other)
    : m_vector04(other.m_vector04)
    , m_vector14(other.m_vector14)
{
    m_flags24        = other.m_flags24;
    m_shaderMaterial = other.m_shaderMaterial;

    raParametersCopyConstruct(m_parameters, other.m_parameters);

    m_alphaMode      = other.m_alphaMode;
    m_vertexStart    = other.m_vertexStart;
    m_indexStart     = other.m_indexStart;
    m_primitiveCount = other.m_primitiveCount;
    m_vertexCount    = other.m_vertexCount;
    m_u4             = other.m_u4;
    m_u5u6           = other.m_u5u6;

    meshBoundsCopy(m_bounds, other.m_bounds);

    m_dword1D4              = other.m_dword1D4;
    m_dword1D8              = other.m_dword1D8;
    m_vegetationVertices    = other.m_vegetationVertices;
    m_vegetationIndices     = other.m_vegetationIndices;
    m_vegetationVertexCount = other.m_vegetationVertexCount;
    m_vegetationIndexCount  = other.m_vegetationIndexCount;
    m_vegetationNonTiled    = other.m_vegetationNonTiled;
    m_dword1F0              = other.m_dword1F0;
    m_template              = other.m_template;

    // sub_1011ECC0 - a fresh head node (the default-constructed m_map1F8), then the source's nodes
    // are re-inserted. The re-insertion (sub_1011E320) walks the source in order and copies each
    // {key, vector} pair.
    // [FRONTIER] the ordered copy-insert itself. The node's 8-byte key is not identified by any
    // recovered reader in this tranche, so inserting one would mean inventing a comparator.
    if (!other.m_map1F8.empty())
    {
        RD9_FRONTIER("dice::hfe::geom::StaticMeshMaterial::copyMap "
                     "(sub_1011ECC0 -> sub_1011E320, the 0x28-byte +0x1F8 map's copy-insert)",
                     0x00);
    }
}

// the copy assignment.

// IT does NOT clear m_vector04 first. The shipped loop (PE 0x1011EB70..0x1011EBFE) appends one
// copy of every source element to whatever the destination already holds, so repeated assignment -
// which is exactly what mergeMaterials' erase does - grows the vector without bound. Reproduced.
// m_vector14 is not assigned at all, and neither is the +0x1F8 map.
StaticMeshMaterial& StaticMeshMaterial::operator=(const StaticMeshMaterial& other)
{
    m_shaderMaterial     = other.m_shaderMaterial;
    m_alphaMode          = other.m_alphaMode;
    m_vertexStart        = other.m_vertexStart;
    m_indexStart         = other.m_indexStart;
    m_primitiveCount     = other.m_primitiveCount;
    m_vertexCount        = other.m_vertexCount;
    m_template           = other.m_template;
    m_dword1F0           = other.m_dword1F0;
    m_flags24            = other.m_flags24;
    m_vegetationNonTiled = other.m_vegetationNonTiled;

    meshBoundsCopy(m_bounds, other.m_bounds);

    m_vegetationVertices    = other.m_vegetationVertices;
    m_vegetationIndices     = other.m_vegetationIndices;
    m_vegetationVertexCount = other.m_vegetationVertexCount;
    m_vegetationIndexCount  = other.m_vegetationIndexCount;

    raParametersAssign(m_parameters, other.m_parameters);

    for (std::vector<ElementList>::const_iterator it = other.m_vector04.begin();
         it != other.m_vector04.end(); ++it)
    {
        // push_back(scratch) - a deep copy of the temporary, then the temporary is destroyed.
        const ElementList scratch(*it);
        m_vector04.push_back(scratch);
    }
    return *this;
}

// the nine virtual slots of off_1021C4E4
// +0x00  the deleting destructor.
void* StaticMeshMaterial::destroy(unsigned char flags)
{
    this->~StaticMeshMaterial();
    if ((flags & 1) != 0)
        ::renddx9abi::engineFree(this);
    return this;
}

// Applies material streams and a cached declaration for the shader's build serial.
// Missing required streams report the material/shader mismatch.

// Binds the declaration, material streams, and template index buffer for the shader.
// Caches bindings by shader build serial. A cache miss with missing elements returns
// false after insertion; later hits bind the partial record and return true.
bool StaticMeshMaterial::applyStreams(void* shaderState)
{
    namespace Ra = ::dice::hfe::dx9rend::Ra;
    Ra::RaShader* const shader = static_cast<Ra::RaShader*>(shaderState);
    const unsigned int key = static_cast<unsigned int>(shader->m_buildSerial);        // +0x174

    StreamDeclarationMap::iterator node = m_map1F8.find(key);
    if (node == m_map1F8.end())
    {
        Ra::RaStreamRequirements* const required =
            Ra::raStreamRequirementsCreate(shader->m_requiredVertexElements);         // sub_10049330 + sub_1004A0F0
        Ra::RaDeclarationBuilder builder;
        Ra::raDeclarationBuilderConstruct(builder);
        std::vector<unsigned int> streamIndices;

        const unsigned int streamCount = static_cast<unsigned int>(m_vector04.size());
        for (unsigned int i = 0; i < streamCount; ++i)
        {
            const ElementList& descriptors = m_vector04[i];
            if (Ra::raStreamRequirementsConsume(required, descriptors))
            {
                streamIndices.push_back(i);
                Ra::raDeclarationBuilderAddStream(builder, descriptors);
            }
        }

        IDirect3DDevice9* const device = Ra::raRendererDevice();                     // renderer +0x0C
        IDirect3DVertexDeclaration9* const declaration = Ra::raVertexDeclarationCached(
            Ra::raDeclarationBuilderFinish(builder), device);

        // operator[] (a zeroed value on a fresh node, sub_1011EC20), then the vector assignment
        // (sub_100EC310), then the find again.
        StaticMeshStreamDeclaration& inserted = m_map1F8[key];
        inserted.declaration = declaration;
        inserted.streams = streamIndices;
        node = m_map1F8.find(key);

        const bool starving = Ra::raStreamRequirementsStarving(required);
        if (starving)
        {
            char missing[1024];
            Ra::raStreamRequirementsDescribe(required, missing, sizeof(missing));
            ::renddx9abi::logf("StaticMeshTemplate.cpp:202 Starving streams in the shader %s, %s",
                               shader->m_name.c_str(), missing);
        }
        Ra::raDeclarationBuilderDestroy(builder);
        Ra::raStreamRequirementsDestroy(required);
        if (starving)
            return false;
    }

    const StaticMeshStreamDeclaration& record = node->second;
    ::dice::hfe::dx9rend::stateManagerSetVertexDeclaration(record.declaration);      // +0x5C
    const unsigned int count = static_cast<unsigned int>(record.streams.size());
    for (unsigned int i = 0; i < count; ++i)
    {
        void* const wrapper = m_vector14[record.streams[i]];
        ::dice::hfe::dx9rend::vertexBufferBindStream(wrapper, i);                    // +0x58
    }
    ::dice::hfe::dx9rend::indexBufferBind(m_template->m_indexBuffer);                // +0x54
    return true;
}

int StaticMeshMaterial::getVertexCount() const    { return static_cast<int>(m_vertexCount); }
int StaticMeshMaterial::slot_0x0C() const         { return -1; }
int StaticMeshMaterial::getPrimitiveCount() const { return static_cast<int>(m_primitiveCount); }
int StaticMeshMaterial::getDword1F0() const       { return static_cast<int>(m_dword1F0); }
int StaticMeshMaterial::getVertexStart() const    { return static_cast<int>(m_vertexStart); }
int StaticMeshMaterial::slot_0x1C() const         { return 0; }
int StaticMeshMaterial::getIndexStart() const     { return static_cast<int>(m_indexStart); }


// The material-container walk every StaticMeshTemplate body below performs:
//     m_materials[geom][lod][material], unguarded, as shipped.

namespace {

inline StaticMeshMaterialList& materialVector(StaticMeshTemplate* t, unsigned int geom,
                                              unsigned int lod)
{
    return t->m_materials.data()[geom].data()[lod];
}

inline unsigned int materialCount(const StaticMeshMaterialList& v)
{
    return static_cast<unsigned int>(v.size());
}

inline StaticMeshMaterial* materialAt(const StaticMeshMaterialList& v, unsigned int i)
{
    return const_cast<StaticMeshMaterial*>(v.data()) + i;
}

inline unsigned int geomCount(StaticMeshTemplate* t)
{
    return static_cast<unsigned int>(t->m_lodCounts.size());
}

inline unsigned int lodCount(StaticMeshTemplate* t, unsigned int geom)
{
    return static_cast<unsigned int>(t->m_lodCounts.data()[geom]);
}

} // anonymous namespace


// StaticMeshTemplate::getPrimitiveCount - 0x10115C60 (slot +0x58)

int StaticMeshTemplate::getPrimitiveCount(int geom, int lod, int material)
{
    const StaticMeshMaterialList& materials =
        materialVector(this, static_cast<unsigned int>(geom), static_cast<unsigned int>(lod));
    if (material != -1)
        return materialAt(materials, static_cast<unsigned int>(material))->getPrimitiveCount();

    int total = 0;
    const unsigned int count = materialCount(materials);
    for (unsigned int i = 0; i < count; ++i)
        total += materialAt(materials, i)->getPrimitiveCount();
    return total;
}


// With geom and lod both -1, sums every geometry/LOD through this virtual slot.

int StaticMeshTemplate::getVertexCount(int geom, int lod, int material)
{
    if (geom == -1 && lod == -1)
    {
        int total = 0;
        const unsigned int geoms = geomCount(this);
        for (unsigned int g = 0; g < geoms; ++g)
        {
            const unsigned int lods = lodCount(this, g);
            for (unsigned int l = 0; l < lods; ++l)
                total += this->getVertexCount(static_cast<int>(g), static_cast<int>(l), -1);
        }
        return total;
    }

    const StaticMeshMaterialList& materials =
        materialVector(this, static_cast<unsigned int>(geom), static_cast<unsigned int>(lod));
    if (material != -1)
        return materialAt(materials, static_cast<unsigned int>(material))->getVertexCount();

    int total = 0;
    const unsigned int count = materialCount(materials);
    for (unsigned int i = 0; i < count; ++i)
        total += materialAt(materials, i)->getVertexCount();
    return total;
}


// Reports bytes for mask bits 1 (vertices), 2 (indices), and 4 (unique textures).
// -1 sums those through virtual calls. Bit 8 instead returns triangle count, discarding bytes.

unsigned int StaticMeshTemplate::getMemoryUsage(int mask)
{
    if (mask == -1)
    {
        const unsigned int a = this->getMemoryUsage(1);
        const unsigned int b = this->getMemoryUsage(2) + a;
        return b + this->getMemoryUsage(4);
    }

    unsigned int total = 0;

    if ((mask & 1) != 0)
    {
        // The wrapper's D3DVERTEXBUFFER_DESC::Size (retail wrapper +0x20).
        void* const streams[7] =
        {
            m_vertexStream, m_tangentStream,
            m_uvStreams[0], m_uvStreams[1], m_uvStreams[2], m_uvStreams[3], m_uvStreams[4]
        };
        for (int i = 0; i < 7; ++i)
        {
            if (streams[i])
                total += ::dice::hfe::dx9rend::vertexBufferByteSize(streams[i]);
        }
    }

    if ((mask & 2) != 0 && m_indexBuffer)
        total += 2u * m_indexCount;

    if ((mask & 8) != 0 && m_indexBuffer)
        return m_indexCount / 3u;      // shipped: `total` is discarded on this arm

    if ((mask & 4) != 0)
    {
        std::set<const void*> unique;
        const unsigned int geoms = static_cast<unsigned int>(m_geoms.size());
        for (unsigned int g = 0; g < geoms; ++g)
        {
            const StaticMeshGeom& geomRecord = m_geoms[g];
            if (geomRecord.lods.empty())
                continue;
            const unsigned int lods = static_cast<unsigned int>(geomRecord.lods.size());
            for (unsigned int l = 0; l < lods; ++l)
            {
                const StaticMeshMaterialList& materials = materialVector(this, g, l);
                const unsigned int count = materialCount(materials);
                for (unsigned int m = 0; m < count; ++m)
                {
                    const ShaderMaterial* const shader = materialAt(materials, m)->m_shaderMaterial;
                    for (int t = 0; t < 8; ++t)
                    {
                        if (shader->textures[t])
                            unique.insert(shader->textures[t]);
                    }
                }
            }
        }
        for (std::set<const void*>::const_iterator it = unique.begin(); it != unique.end(); ++it)
        {
            total += static_cast<const ::dice::hfe::dx9rend::EngineTexture*>(*it)->totalBytes;
        }
    }

    return total;
}


// Merges materials with equal quality-masked shader flags, alpha mode, and active textures.
// After each merge, restart the inner scan at the current outer material.

namespace {

// sub_10115870 - add `delta` to a run of indices in the template's index data.
void mergeShiftIndices(StaticMeshTemplate* t, short delta, int indexStart, int primitiveCount)
{
    unsigned short* const indices = t->m_indexData;
    const int end = indexStart + primitiveCount + 2 * primitiveCount;   // indexStart + 3 * count
    for (int i = indexStart; i < end; ++i)
        indices[i] = static_cast<unsigned short>(indices[i] + delta);
}

// Shifts subsequent material starts by primitiveDelta * 3 indices and vertexDelta vertices.
void mergeShiftStarts(StaticMeshTemplate* t, unsigned int fromGeom, unsigned int fromLod,
                      unsigned int fromMaterial, int primitiveDelta, int vertexDelta)
{
    const unsigned int geoms = geomCount(t);
    for (unsigned int g = fromGeom; g < geoms; ++g)
    {
        const unsigned int lods = lodCount(t, g);
        unsigned int l = (g != fromGeom) ? 0u : fromLod;
        for (; l < lods; ++l)
        {
            const StaticMeshMaterialList& materials = materialVector(t, g, l);
            const unsigned int count = materialCount(materials);
            const unsigned int firstMaterial =
                (g == fromGeom && l == fromLod) ? fromMaterial : 0u;
            for (unsigned int m = firstMaterial; m < count; ++m)
            {
                StaticMeshMaterial* const material = materialAt(materials, m);
                material->m_vertexStart += static_cast<unsigned int>(vertexDelta);
                material->m_indexStart  += static_cast<unsigned int>(3 * primitiveDelta);
            }
        }
    }
}

// sub_10116890 - move material `to`'s vertex and index runs so they sit immediately after material
// `from`'s. Two rotations, each done through a scratch buffer: one over the interleaved vertex data
// at +0x90, one over the 16-bit index data at +0x98.
void mergeRotateGeometry(StaticMeshTemplate* t, unsigned int geom, unsigned int lod,
                         unsigned int to, unsigned int from)
{
    const StaticMeshMaterialList& materials = materialVector(t, geom, lod);
    const StaticMeshMaterial* const src = materialAt(materials, from);
    const StaticMeshMaterial* const dst = materialAt(materials, to);

    const unsigned int stride = t->m_vertexStride;
    const unsigned int firstVertex = dst->m_vertexStart;
    const unsigned int firstIndex  = dst->m_indexStart;

    // vertices
    {
        const unsigned int tailBytes =
            stride * (t->m_vertexCount - src->m_vertexCount - firstVertex);
        char* const scratch = static_cast<char*>(::renddx9abi::engineAlloc(tailBytes));
        char* const base    = static_cast<char*>(t->m_vertexData);

        const unsigned int headBytes = stride * (src->m_vertexStart - firstVertex);
        ::memcpy(scratch, base + stride * firstVertex, headBytes);

        const unsigned int afterSrc = stride * (src->m_vertexStart + src->m_vertexCount);
        ::memcpy(scratch + headBytes, base + afterSrc, stride * t->m_vertexCount - afterSrc);

        const unsigned int movedBytes = stride * src->m_vertexCount;
        ::memmove(base + stride * firstVertex, base + stride * src->m_vertexStart, movedBytes);
        ::memcpy(base + stride * firstVertex + movedBytes, scratch, tailBytes);
        ::renddx9abi::engineFree(scratch);
    }

    // indices
    {
        unsigned short* const base = t->m_indexData;
        const unsigned int tailBytes =
            2u * (t->m_indexCount - firstIndex - 3u * src->m_primitiveCount);
        char* const scratch = static_cast<char*>(::renddx9abi::engineAlloc(tailBytes));

        const unsigned int headBytes = 2u * (src->m_indexStart - firstIndex);
        ::memcpy(scratch, base + firstIndex, headBytes);

        const unsigned int afterSrc = 2u * (src->m_indexStart + 3u * src->m_primitiveCount);
        ::memcpy(scratch + headBytes,
                 reinterpret_cast<const char*>(base) + afterSrc,
                 2u * t->m_indexCount - afterSrc);

        const unsigned int movedBytes = 6u * src->m_primitiveCount;
        ::memmove(base + firstIndex, base + src->m_indexStart, movedBytes);
        ::memcpy(reinterpret_cast<char*>(base + firstIndex) + movedBytes, scratch, tailBytes);
        ::renddx9abi::engineFree(scratch);
    }
}

// sub_10119950 - move material `from` to slot `to` in the record vector, re-basing every start on
// the way. The shipped body keeps a full copy of the moved material on its own stack and
// re-bases that copy's two start fields before re-inserting it - `v10`/`v11` in the decompiler's
// frame are the scratch material's +0x180 / +0x184, not arguments to the shift.
void mergeMoveMaterial(StaticMeshTemplate* t, unsigned int geom, unsigned int lod,
                       unsigned int to, unsigned int from)
{
    StaticMeshMaterialList& materials = materialVector(t, geom, lod);

    StaticMeshMaterial scratch(*materialAt(materials, from));
    const StaticMeshMaterial* const previous = materialAt(materials, to - 1);

    // Everything after the moved material slides back by its own run.
    mergeShiftStarts(t, geom, lod, from + 1u,
                     -static_cast<int>(scratch.m_primitiveCount),
                     -static_cast<int>(scratch.m_vertexCount));

    // sub_1011F810 - erase(first + from): assign the tail down one slot, destroy the duplicate at
    // the end. Storage is kept, so `previous` stays valid.
    materials.erase(materials.begin() + from);

    // The moved copy takes the slot immediately after `to - 1`.
    scratch.m_vertexStart = previous->m_vertexStart + previous->m_vertexCount;
    scratch.m_indexStart  = previous->m_indexStart + 3u * previous->m_primitiveCount;

    // sub_101207D0 -> sub_10120360 - insert(first + to, scratch).
    materials.insert(materials.begin() + to, scratch);

    // ... and everything after it slides forward again by the same run.
    mergeShiftStarts(t, geom, lod, to + 1u,
                     static_cast<int>(scratch.m_primitiveCount),
                     static_cast<int>(scratch.m_vertexCount));

    // The scratch copy is destroyed as the function returns.
}

// sub_10119A80 - fold material `*j` into material `i`.
bool mergeFold(StaticMeshTemplate* t, unsigned int geom, unsigned int lod,
               unsigned int i, unsigned int* j)
{
    if (*j - i > 1u)
    {
        mergeRotateGeometry(t, geom, lod, i + 1u, *j);
        mergeMoveMaterial(t, geom, lod, i + 1u, *j);
        *j = i + 1u;
    }

    const StaticMeshMaterialList& materials = materialVector(t, geom, lod);
    StaticMeshMaterial* const dst = materialAt(materials, i);
    StaticMeshMaterial* const src = materialAt(materials, *j);

    mergeShiftIndices(t, static_cast<short>(dst->m_vertexCount),
                      static_cast<int>(src->m_indexStart),
                      static_cast<int>(src->m_primitiveCount));

    dst->m_primitiveCount += src->m_primitiveCount;
    dst->m_vertexCount    += src->m_vertexCount;

    meshBoundsExpand(dst->m_bounds, src->m_bounds.min);
    meshBoundsExpand(dst->m_bounds, src->m_bounds.max);
    return true;
}

} // anonymous namespace

void StaticMeshTemplate::mergeMaterials()
{
    const unsigned int geoms = geomCount(this);
    for (unsigned int geom = 0; geom < geoms; ++geom)
    {
        const unsigned int lods = lodCount(this, geom);
        for (unsigned int lod = 0; lod < lods; ++lod)
        {
            StaticMeshMaterialList& materials = materialVector(this, geom, lod);
            const unsigned int mask = staticMeshQualityMask();

            // The end is re-read on every step: folding erases from the vector, never growing it,
            // so the cursors stay valid.
            unsigned int i = 0;
            for (StaticMeshMaterial* a = materials.data();
                 a != materials.data() + materials.size(); ++a, ++i)
            {
                unsigned int j = i + 1;
                for (StaticMeshMaterial* b = a + 1; b != materials.data() + materials.size(); ++b, ++j)
                {
                    const StaticMeshMaterial* const left  = a;
                    const StaticMeshMaterial* const right = b;
                    const ShaderMaterial* const ls = left->m_shaderMaterial;
                    const ShaderMaterial* const rs = right->m_shaderMaterial;

                    const unsigned int flags = mask & g_shaderComboFlags[ls->shaderCombo];
                    if (flags == (mask & g_shaderComboFlags[rs->shaderCombo])
                        && left->m_alphaMode == right->m_alphaMode
                        && ((flags & 0x002u) == 0 || ls->textures[1] == rs->textures[1])
                        && ((flags & 0x080u) == 0 || ls->textures[5] == rs->textures[5])
                        && ((flags & 0x004u) == 0 || ls->textures[2] == rs->textures[2])
                        && ((flags & 0x008u) == 0 || ls->textures[3] == rs->textures[3])
                        && ((flags & 0x100u) == 0 || ls->textures[6] == rs->textures[6])
                        && ((flags & 0x010u) == 0 || ls->textures[4] == rs->textures[4])
                        && mergeFold(this, geom, lod, i, &j))
                    {
                        ++g_staticMeshMergedCount[lod];

                        // Release the shader, shift by assignment, then destroy the duplicate tail element.
                        StaticMeshMaterial* const dead = materialAt(materials, j);
                        if (dead->m_shaderMaterial)
                            shaderMaterialRelease(dead->m_shaderMaterial);
                        materials.erase(materials.begin() + j);

                        // Shipped: the inner cursor goes back to `i`, and the ++ below moves it to
                        // the slot the erase just filled.
                        j = i;
                        b = a;
                    }
                }
            }
        }
    }
}


// buildVertexBuffers - 0x10117410, 3421 bytes: the largest body in this file.

namespace {

// sub_10049F20 - the 10:10:10 packed normal.
unsigned int packNormal10(const float* n)
{
    return (static_cast<int>(n[0] * 511.0f) & 0x3FF)
         | ((((static_cast<int>(n[1] * 511.0f) & 0x3FF)
              | ((static_cast<int>(n[2] * 511.0f) & 0x3FF) << 10)) << 10));
}

// sub_10049E80 - the D3DCOLOR packed normal.
unsigned int packNormalColor(const float* n)
{
    const unsigned int x = static_cast<unsigned int>(127 - static_cast<int>(n[0] * -127.0f));
    const unsigned int y = static_cast<unsigned int>(127 - static_cast<int>(n[1] * -127.0f));
    const unsigned int z = static_cast<unsigned int>(127 - static_cast<int>(n[2] * -127.0f));
    return (x << 16) | ((y & 0xFF) << 8) | (z & 0xFF) | 0xFF000000u;
}

// Packs a normal using the encoding supported by device feature 14 (capability bit 0x80).
unsigned int packNormal(const float* n)
{
    const unsigned int featureMask = *reinterpret_cast<const unsigned int*>(
        static_cast<const unsigned char*>(::dice::hfe::dx9rend::g_deviceCaps) + 0xEC);
    return (featureMask & 0x80u) != 0 ? packNormal10(n) : packNormalColor(n);
}

// sub_100777A0 - Vec3 normalize in place, with the shipped |v|^2 within-epsilon short circuit.
void normalizeInPlace(float* v)
{
    const float lengthSquared = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (::fabs(static_cast<double>(lengthSquared) - 1.0) < 0.00000011920929)
        return;
    if (::fabs(static_cast<double>(lengthSquared)) < 0.00000011920929)
    {
        v[0] = 0.0f; v[1] = 0.0f; v[2] = 0.0f;
        return;
    }
    const float inverse = static_cast<float>(1.0 / ::sqrt(static_cast<double>(lengthSquared)));
    v[0] *= inverse;
    v[1] *= inverse;
    v[2] *= inverse;
}

inline float maxAbs(float a, float b)
{
    const float fa = static_cast<float>(::fabs(static_cast<double>(a)));
    const float fb = static_cast<float>(::fabs(static_cast<double>(b)));
    return fa >= fb ? fa : fb;
}

const float kMinCompressExtent = 0.00000011920929f;

} // anonymous namespace

void StaticMeshTemplate::buildVertexBuffers()
{
    // the declaration wrapper's D3D object (retail wrapper +0x08), and GetDeclaration is
    // slot +0x10 of IDirect3DVertexDeclaration9.
    D3DVERTEXELEMENT9 elements[256];
    {
        IDirect3DVertexDeclaration9* const decl =
            ::dice::hfe::dx9rend::vertexDeclarationD3D(m_vertexDeclaration);
        decl->GetDeclaration(elements, &m_declElementCount);
    }

    const bool compress = this->getCompressVertexData();
    const int  maxRepeat = this->getMaxTextureRepeat();
    const float uvScale = 32767.0f / static_cast<float>(maxRepeat);

    if (compress)
    {
        float z = maxAbs(m_bounds.max.z, m_bounds.min.z);
        float y = maxAbs(m_bounds.max.y, m_bounds.min.y);
        float x = maxAbs(m_bounds.max.x, m_bounds.min.x);
        if (z < kMinCompressExtent) z = kMinCompressExtent;
        if (y < kMinCompressExtent) y = kMinCompressExtent;
        if (x < kMinCompressExtent) x = kMinCompressExtent;

        // sub_1007B220 returns the lazily-built shared Vec3(1,1,1) at dword_10247BFC.
        m_compressScale.z = (1.0f / z) * 32767.0f;
        m_compressScale.x = (1.0f / x) * 32767.0f;
        m_compressScale.y = (1.0f / y) * 32767.0f;

        m_decompressScale.x = 1.0f / m_compressScale.x;
        m_decompressScale.z = 1.0f / m_compressScale.z;
        m_decompressScale.y = 1.0f / m_compressScale.y;
        m_uvDecompress = 1.0f / uvScale;
    }
    else
    {
        m_compressScale.x = 1.0f; m_compressScale.y = 1.0f; m_compressScale.z = 1.0f;
        m_decompressScale.x = 1.0f; m_decompressScale.y = 1.0f; m_decompressScale.z = 1.0f;
        m_uvDecompress = 1.0f;
    }

    // classify the declaration's elements from index 3 on
    // The source vertex begins with position (12 B), normal (12 B) and four unread bytes; the first
    // texcoord therefore lands at byte 28, which is what the cursor starts at.
    // Both lists are bounded by the element array, so they cannot overflow where the shipped
    // vectors would merely grow.
    unsigned int texcoordOffsets[256];
    unsigned int texcoordCount = 0;
    unsigned int tangentOffsets[256];
    unsigned int tangentCount = 0;
    {
        unsigned int cursor = 28;
        for (unsigned int i = 3; i < m_declElementCount; ++i)
        {
            switch (elements[i].Usage)
            {
            case D3DDECLUSAGE_TEXCOORD:                  // 5
                texcoordOffsets[texcoordCount++] = cursor;
                cursor += 8;
                break;
            case D3DDECLUSAGE_TANGENT:                   // 6
                tangentOffsets[tangentCount++] = cursor;
                cursor += 12;
                break;
            case D3DDECLUSAGE_BINORMAL:                  // 7
                // sub_100E22D0 pushes onto the tangent list as well.
                tangentOffsets[tangentCount++] = cursor;
                cursor += 12;
                break;
            default:
                break;
            }
        }
    }

    m_extraUvCount      = static_cast<int>(texcoordCount) - 1;
    const unsigned int tangentStrideRaw = 12u * tangentCount;
    m_hasTwoTangentSets = static_cast<unsigned char>(tangentCount == 2);

    const unsigned int stream0Stride = compress ? 16u : 32u;

    // create the buffers
    {
        std::string name("StaticMesh : ");
        name += m_name;
        m_vertexStream = ::dice::hfe::dx9rend::createVertexBuffer(
            ::dice::hfe::dx9rend::g_bufferManager, m_vertexCount, stream0Stride, name, 8);
    }

    // The three gating masks, in the shipped order {4, 8, 16}: an extra uv set beyond the first
    // three is always created, the first three only when the template's used-flag set matches its
    // whole-flag set under that bit.
    {
        static const unsigned int kUvGateMasks[3] = { 4u, 8u, 16u };
        for (unsigned int i = 0; i < texcoordCount - 1u; ++i)
        {
            unsigned int gated = texcoordCount - 2u;
            if (gated > 3u)
                gated = 3u;
            if (i >= gated ||
                (kUvGateMasks[i] & m_shaderFlagsAll) == (kUvGateMasks[i] & m_shaderFlagsUsed))
            {
                std::string name("StaticMesh : ");
                name += m_name;
                m_uvStreams[i] = ::dice::hfe::dx9rend::createVertexBuffer(
                    ::dice::hfe::dx9rend::g_bufferManager, m_vertexCount,
                    compress ? 4u : 8u, name, 8);
            }
        }
    }

    {
        std::string name("StaticMesh : ");
        name += m_name;
        m_tangentStream = ::dice::hfe::dx9rend::createVertexBuffer(
            ::dice::hfe::dx9rend::g_bufferManager, m_vertexCount,
            compress ? 4u * tangentCount : tangentStrideRaw, name, 8);
    }

    // lock everything
    char* const stream0 = static_cast<char*>(
        ::dice::hfe::dx9rend::vertexBufferLock(m_vertexStream, 0, m_vertexCount, 0));
    char* const tangentStream = static_cast<char*>(
        ::dice::hfe::dx9rend::vertexBufferLock(m_tangentStream, 0, m_vertexCount, 0));
    char* uvStreams[5] = { 0, 0, 0, 0, 0 };
    for (unsigned int i = 0; i < texcoordCount - 1u; ++i)
    {
        if (m_uvStreams[i])
            uvStreams[i] = static_cast<char*>(
                ::dice::hfe::dx9rend::vertexBufferLock(m_uvStreams[i], 0, m_vertexCount, 0));
    }

    if (stream0 && tangentStream)
    {
        // vegetation pass 1: per-material normal recompute around the material centroid

        // Vegetation processing visits only geometry 0.
        if (m_isVegetation)
        {
            const StaticMeshLodMaterialLists& geom0 = *m_materials.data();   // unguarded, as shipped
            const unsigned int lods = static_cast<unsigned int>(geom0.size());
            for (unsigned int l = 0; l < lods; ++l)
            {
                const StaticMeshMaterialList& materials = geom0.data()[l];
                const unsigned int count = materialCount(materials);
                for (unsigned int m = 0; m < count; ++m)
                {
                    StaticMeshMaterial* const material = materialAt(materials, m);
                    const ShaderMaterial* const shader = material->m_shaderMaterial;
                    if (shader->textures[2])
                        continue;
                    const ::dice::hfe::dx9rend::EngineTexture* const base =
                        static_cast<const ::dice::hfe::dx9rend::EngineTexture*>(shader->textures[1]);
                    if (::strstr(base->name.c_str(), "tile_"))
                        continue;

                    material->m_vegetationNonTiled = 1;
                    if (m_keepVegetationNormals)
                        continue;

                    const unsigned int vertices = material->m_vertexCount;
                    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
                    if (vertices)
                    {
                        const char* cursor = static_cast<const char*>(m_vertexData)
                                           + m_vertexStride * material->m_vertexStart;
                        for (unsigned int v = 0; v < vertices; ++v, cursor += m_vertexStride)
                        {
                            const float* const p = reinterpret_cast<const float*>(cursor);
                            cx += p[0]; cy += p[1]; cz += p[2];
                        }
                        const float inverse = 1.0f / static_cast<float>(vertices);
                        cx *= inverse; cy *= inverse; cz *= inverse;

                        for (unsigned int v = 0; v < vertices; ++v)
                        {
                            float* const p = reinterpret_cast<float*>(
                                static_cast<char*>(m_vertexData)
                                + m_vertexStride * (v + material->m_vertexStart));
                            float n[3];
                            n[0] = p[0] - cx;
                            n[1] = p[1] - cy;
                            n[2] = p[2] - cz;
                            normalizeInPlace(n);
                            p[3] = n[0];
                            p[4] = n[1];
                            p[5] = n[2];
                        }
                    }
                }
            }

            // vegetation pass 2: the 32-byte per-vertex copies into +0x1DC..+0x1E8
            // The shipped body re-tests m_isVegetation here although it is already inside the same
            // guard; kept.
            if (m_isVegetation)
            {
                const unsigned int lods2 = static_cast<unsigned int>(geom0.size());
                for (unsigned int l = 0; l < lods2; ++l)
                {
                    const StaticMeshMaterialList& materials = geom0.data()[l];
                    const unsigned int count = materialCount(materials);
                    for (unsigned int m = 0; m < count; ++m)
                    {
                        StaticMeshMaterial* const material = materialAt(materials, m);
                        const unsigned int vertices = material->m_vertexCount;
                        material->m_vegetationVertexCount = vertices;

                        char* const copy =
                            static_cast<char*>(::renddx9abi::engineAlloc(32u * vertices));
                        if (copy)
                        {
                            float* fill = reinterpret_cast<float*>(copy);
                            for (unsigned int v = 0; v < vertices; ++v, fill += 8)
                            {
                                for (int k = 0; k < 8; ++k)
                                    fill[k] = -1.0f;
                            }
                        }
                        material->m_vegetationVertices = copy;

                        for (unsigned int v = 0; v < vertices; ++v)
                        {
                            const unsigned int* const src = reinterpret_cast<const unsigned int*>(
                                static_cast<const char*>(m_vertexData)
                                + m_vertexStride * (v + material->m_vertexStart));
                            unsigned int* const dst =
                                reinterpret_cast<unsigned int*>(copy + 32u * v);
                            dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
                            dst[3] = src[3]; dst[4] = src[4]; dst[5] = src[5];
                            dst[6] = src[7]; dst[7] = src[8];        // the uv set at byte 28
                        }
                    }
                }
            }
        }

        // the packing loop
        // Both cursors advance every iteration regardless of which branch runs - shipped.
        char* compressedCursor   = stream0 + 6;
        char* uncompressedCursor = stream0;
        unsigned int uvCursor    = 0;      // the byte offset into every extra uv stream
        char* tangentCursor      = tangentStream;

        for (unsigned int v = 0, sourceOffset = 0; v < m_vertexCount; ++v)
        {
            const char* const source = static_cast<const char*>(m_vertexData) + sourceOffset;

            if (compress)
            {
                const float* const p = reinterpret_cast<const float*>(source);
                short* const out = reinterpret_cast<short*>(compressedCursor);
                out[-3] = static_cast<short>(static_cast<int>(p[0] * m_compressScale.x));
                out[-2] = static_cast<short>(static_cast<int>(p[1] * m_compressScale.y));
                out[-1] = static_cast<short>(static_cast<int>(p[2] * m_compressScale.z));
                out[0]  = static_cast<short>(m_isRoad
                              ? 1
                              : *reinterpret_cast<const unsigned char*>(source + 26));

                *reinterpret_cast<unsigned int*>(compressedCursor + 2) =
                    packNormal(reinterpret_cast<const float*>(source + 12));

                unsigned int sourceUv = 28;
                for (unsigned int t = 0; t < texcoordCount; ++t, sourceUv += 8)
                {
                    const float* const uv = reinterpret_cast<const float*>(source + sourceUv);
                    const short u = static_cast<short>(
                        static_cast<int>(::floor(uvScale * uv[0] + 0.5f)));
                    const short w = static_cast<short>(
                        static_cast<int>(::floor(uvScale * uv[1] + 0.5f)));
                    if (t == 0)
                    {
                        *reinterpret_cast<short*>(compressedCursor + 6) = u;
                        *reinterpret_cast<short*>(compressedCursor + 8) = w;
                    }
                    else if (uvStreams[t - 1])
                    {
                        short* const slot =
                            reinterpret_cast<short*>(uvStreams[t - 1] + uvCursor);
                        slot[0] = u;
                        slot[1] = w;
                    }
                }

                const char* const tangentSource = source + sourceUv;
                *reinterpret_cast<unsigned int*>(tangentCursor) =
                    packNormal(reinterpret_cast<const float*>(tangentSource));
                tangentCursor += 4;
                if (tangentCount > 1)
                {
                    *reinterpret_cast<unsigned int*>(tangentCursor) =
                        packNormal(reinterpret_cast<const float*>(tangentSource + 12));
                    tangentCursor += 4;
                }
                uvCursor += 4;
            }
            else
            {
                const unsigned int* const src = reinterpret_cast<const unsigned int*>(source);
                unsigned int* const dst = reinterpret_cast<unsigned int*>(uncompressedCursor);
                dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
                dst[3] = src[3]; dst[4] = src[4]; dst[5] = src[5];

                unsigned int sourceUv = 28;
                const unsigned int* uv = src + 7;
                for (unsigned int t = 0; t < texcoordCount; ++t, sourceUv += 8, uv += 2)
                {
                    if (t == 0)
                    {
                        dst[6] = uv[0];
                        dst[7] = uv[1];
                    }
                    else if (uvStreams[t - 1])
                    {
                        unsigned int* const slot =
                            reinterpret_cast<unsigned int*>(uvStreams[t - 1] + uvCursor);
                        slot[0] = uv[0];
                        slot[1] = uv[1];
                    }
                }
                uvCursor += 8;
                ::memcpy(tangentCursor, source + sourceUv, tangentStrideRaw);
                tangentCursor += tangentStrideRaw;
            }

            sourceOffset       += m_vertexStride;
            compressedCursor   += 16;
            uncompressedCursor += 32;
        }

        ::dice::hfe::dx9rend::vertexBufferUnlock(m_vertexStream);
        ::dice::hfe::dx9rend::vertexBufferUnlock(m_tangentStream);
        for (unsigned int i = 0; i < texcoordCount - 1u; ++i)
        {
            if (m_uvStreams[i])
                ::dice::hfe::dx9rend::vertexBufferUnlock(m_uvStreams[i]);
        }
    }

    // m_vertexData IS freed BUT NOT nulled - only the declaration pointer is cleared. Shipped

    ::dice::hfe::dx9rend::vertexDeclarationRelease(m_vertexDeclaration);
    void* const vertexData = m_vertexData;
    m_vertexDeclaration = 0;
    ::renddx9abi::engineFree(vertexData);
}


// Copies index runs into vegetation materials in geometry 0, then builds the base buffer.
// Only primitive types 4 and 5 assign a count; other types reuse the previous value,
// including an uninitialized value for the first material. Allocations are not checked.

void StaticMeshTemplate::buildIndexBuffer()
{
    if (m_isVegetation)
    {
        const StaticMeshLodMaterialLists& geom0 = *m_materials.data();   // unguarded, as shipped
        int indexCount = 0;      // var_10: the shipped slot is NOT initialised - see the note above
        unsigned int lod = 0;
        while (lod < static_cast<unsigned int>(geom0.size()))
        {
            const StaticMeshMaterialList& materials = geom0.data()[lod];
            const unsigned int count = materialCount(materials);
            for (unsigned int i = 0; i < count; ++i)
            {
                StaticMeshMaterial* const material = materialAt(materials, i);
                if (m_primitiveType == 4)
                    indexCount = 3 * static_cast<int>(material->m_primitiveCount);
                else if (m_primitiveType == 5)
                    indexCount = static_cast<int>(material->m_primitiveCount) + 2;

                material->m_vegetationIndexCount = static_cast<unsigned int>(indexCount);
                void* const copy = ::renddx9abi::engineAlloc(2u * indexCount);
                material->m_vegetationIndices = static_cast<unsigned short*>(copy);
                ::memcpy(copy, m_indexData + material->m_indexStart, 2u * indexCount);
            }
            ++lod;
        }
    }

    MeshTemplate::buildIndexBuffer();     // sub_10191A80, the unconditional tail call
}


// finalizeMaterials - 0x10118B00 (slot +0xD0)

namespace {

// Writes four normal-unpack constants matching the device's packed-normal format.
// DEC3N uses (1,0), D3DCOLOR uses (2,-1); both use (1/32768,0) for the second pair.
// out must hold four floats and device caps must be initialized.
// [RETIRE-WHEN: RW-ra-normal-format-writer] Replace on-demand calculation when the shared
// reset/format writer is implemented and verified, including both packed-8 descriptor updates.
void raNormalUnpackConstants(float out[4])
{
    const unsigned int featureMask = *reinterpret_cast<const unsigned int*>(
        static_cast<const unsigned char*>(::dice::hfe::dx9rend::g_deviceCaps) + 0xEC);

    if ((featureMask & 0x80u) != 0)
    {
        out[0] = 1.0f;
        out[1] = 0.0f;
    }
    else
    {
        out[0] = 2.0f;
        out[1] = -1.0f;
    }

    out[2] = 3.0517578125e-05f;   // 0x38000000 = 2^-15, both arms
    out[3] = 0.0f;
}


// The pushes setupTechnique makes: sub_1004D3E0 (Ra) / sub_1008A9B0 (here) / sub_1004CD70 for one
// pointer, and sub_1011E7E0 for one element list on the vector OF lists at material +0x04. That
// element is copy-constructed (sub_1011D490), which is why every local list below outlives its push.

// Builds parallel vertex-element lists and vertex-buffer references for a material.
// Each element list is copied; buffer entries hold wrapper values, not addresses of slots.
// Packed and unpacked descriptors share element IDs and differ in declaration type.
void staticMeshMaterialSetupTechnique(StaticMeshMaterial* material)
{
    namespace Ra = ::dice::hfe::dx9rend::Ra;

    // the quality mask AND the combo's flag word.  Both halves are already recovered
    // in this file; the shipped expression is the one finalizeMaterials' sort also uses.
    const unsigned int quality =
        staticMeshQualityMask() & g_shaderComboFlags[material->m_shaderMaterial->shaderCombo];

    StaticMeshTemplate* const tmpl = material->m_template;          // +0x1F4

    // template vslot +0x7C.  ONE call, and every arm below reads the answer - the shipped body
    // keeps it in a byte on the frame rather than asking again.
    const bool compressed = tmpl->getCompressVertexData();

    // the base stream: position, normal, base uv.  Always.
    StaticMeshMaterial::ElementList baseElements;
    if (compressed)
    {
        baseElements.push_back(&Ra::g_desc_1023ED9C);      // Vtx_PositionPacked
        baseElements.push_back(&Ra::g_desc_1023EE14);      // Vtx_NormalPacked8
        baseElements.push_back(&Ra::g_desc_1023EDEC);      // Vtx_TBasePacked
    }
    else
    {
        baseElements.push_back(&Ra::g_desc_1023EF04);      // Vtx_Position
        baseElements.push_back(&Ra::g_desc_1023EF7C);      // Vtx_Normal
        baseElements.push_back(&Ra::g_desc_1023ED4C);      // Vtx_TBase
    }
    material->m_vector04.push_back(baseElements);
    material->m_vector14.push_back(tmpl->m_vertexStream);                 // +0x154

    // the detail uv set, on quality bit 2
    if ((quality & 0x004u) != 0)
    {
        StaticMeshMaterial::ElementList detail;
        detail.push_back(compressed ? &Ra::g_desc_1023EE8C     // Vtx_TDetailPacked
                                    : &Ra::g_desc_1023F01C);   // Vtx_TDetail
        material->m_vector04.push_back(detail);
        material->m_vector14.push_back(tmpl->m_uvStreams[tmpl->m_streamIndexA]);
    }

    // the tangent stream.  Always.
    StaticMeshMaterial::ElementList tangentElements;
    tangentElements.push_back(compressed ? &Ra::g_desc_1023EEB4   // Vtx_TangentSpacePacked8
                                         : &Ra::g_desc_1023EFF4); // Vtx_TangentSpace
    material->m_vector04.push_back(tangentElements);
    material->m_vector14.push_back(tmpl->m_tangentStream);               // +0x16C

    // the dirt uv set, on quality bit 3
    if ((quality & 0x008u) != 0)
    {
        StaticMeshMaterial::ElementList dirt;
        dirt.push_back(compressed ? &Ra::g_desc_1023EFA4       // Vtx_TDirtPacked
                                  : &Ra::g_desc_1023EFCC);     // Vtx_TDirt
        material->m_vector04.push_back(dirt);
        material->m_vector14.push_back(tmpl->m_uvStreams[tmpl->m_streamIndexB]);
    }

    // the crack uv set, on quality bit 4
    if ((quality & 0x010u) != 0)
    {
        StaticMeshMaterial::ElementList crack;
        crack.push_back(compressed ? &Ra::g_desc_1023EE3C      // Vtx_TCrackPacked
                                   : &Ra::g_desc_1023F044);    // Vtx_TCrack
        material->m_vector04.push_back(crack);
        material->m_vector14.push_back(tmpl->m_uvStreams[tmpl->m_streamIndexC]);
    }

    // Lightmap UVs require finished materials, the lighting/water/expansion gate,
    // and a separate non-road check in this order.
    if (tmpl->m_finished)
    {
        const bool wantLightmap =
            tmpl->m_isWater != 0
            || ::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(2) > 1  // +0x74, +0x04
            || ::dice::hfe::dx9rend::rendererIsExpansionPack();                    // +0xAC
        if (wantLightmap)
        {
            if (!tmpl->m_isRoad)
            {
                StaticMeshMaterial::ElementList lightmap;
                lightmap.push_back(compressed ? &Ra::g_desc_1023EE64   // Vtx_TLightMapPacked
                                              : &Ra::g_desc_1023EF2C); // Vtx_TLightMap
                material->m_vector04.push_back(lightmap);

                // `&template->dword[extraUvCount + 85]`, which is &m_uvStreams[extraUvCount - 1]
                // because m_uvStreams starts one dword after m_vertexStream.  The off-by-one is the
                // shipped arithmetic, not a correction of it.
                material->m_vector14.push_back(tmpl->m_uvStreams[tmpl->m_extraUvCount - 1]);
            }
        }
    }
}

// sub_101158B0 - bind the shader record's textures into the parameter block, set AlphaTest, fill
// ParallaxScaleBias when the combo carries bit 5 (0x20), and publish ObjRadius from the template's
// bounds.
void staticMeshMaterialBindTextures(StaticMeshMaterial* material, int geom, int lod, int index)
{
    const ShaderMaterial* const shader = material->m_shaderMaterial;
    RaMaterialParameters& p = params(material);

    // The first slot is assigned inline by the shipped body rather than through sub_10031180, but
    // with the identical release/assign/addRef shape.
    {
        void* const wanted = shader->textures[1];
        if (p.diffuseMap.value != wanted)
        {
            ::dice::hfe::dx9rend::EngineTexture* const old =
                static_cast<::dice::hfe::dx9rend::EngineTexture*>(p.diffuseMap.value);
            if (old && --old->refCount == 0)
            {
                ::dice::hfe::dx9rend::textureDtor(old);
                ::renddx9abi::engineFree(old);
            }
            p.diffuseMap.value = wanted;
            if (wanted)
                ::dice::hfe::dx9rend::textureAddRef(
                    static_cast<::dice::hfe::dx9rend::EngineTexture*>(wanted));
        }
    }

    // sub_10031180 for slots 2..6 - and only when the shader record actually holds one, which is
    // the shipped guard (a null slot leaves the holder untouched rather than clearing it).
    RaParamTexture* const holders[5] =
        { &p.detailMap, &p.dirtMap, &p.crackMap, &p.normalMap, &p.crackNormalMap };
    for (int i = 0; i < 5; ++i)
    {
        if (shader->textures[2 + i])
            raAssignTextureValue(*holders[i], shader->textures[2 + i]);
    }

    p.alphaTest.value =
        static_cast<unsigned char>(material->m_alphaMode == 2 || material->m_alphaMode == 1);

    if ((g_shaderComboFlags[shader->shaderCombo] & 0x20u) != 0)
    {
        // sub_10084660 - {1/width, 1/height} of the detail map, scaled by the template's material
        // value, and then the bias pair at -0.5 * each.
        const ::dice::hfe::dx9rend::EngineTexture* const detail =
            static_cast<const ::dice::hfe::dx9rend::EngineTexture*>(shader->textures[2]);
        const unsigned int* const dims = ::dice::hfe::dx9rend::textureDims(detail);
        const float invW = static_cast<float>(1.0 / static_cast<double>(dims[0]));
        const float invH = static_cast<float>(1.0 / static_cast<double>(dims[1]));

        const float value = material->m_template->getMaterialValue(
            geom, lod, static_cast<unsigned char>(index));
        const float sx = invW * value;
        const float sy = invH * value;
        p.parallaxScaleBias.value[3] = sy * -0.5f;
        p.parallaxScaleBias.value[2] = sx * -0.5f;
        p.parallaxScaleBias.value[0] = sx;
        p.parallaxScaleBias.value[1] = sy;
    }

    StaticMeshTemplate* const owner = material->m_template;
    if (owner->m_bounds.dirty)
    {
        meshBoundsFinalize(owner->m_bounds);
        p.objRadius.value = owner->m_bounds.radius;
    }
    else
    {
        p.objRadius.value = owner->m_bounds.radius;
    }
}

} // anonymous namespace

void StaticMeshTemplate::finalizeMaterials()
{
    const unsigned int geoms = geomCount(this);

    // The three extra uv stream indices, allocated in flag order: detail, dirt, crack.
    int next = 0;
    if ((m_shaderFlagsAll & 4u) != 0)
    {
        m_streamIndexA = 0;
        next = 1;
    }
    if ((m_shaderFlagsAll & 8u) != 0)
        m_streamIndexB = next++;
    if ((m_shaderFlagsAll & 0x10u) != 0)
        m_streamIndexC = next;

    for (unsigned int geom = 0; geom < geoms; ++geom)
    {
        const unsigned int lods = lodCount(this, geom);
        for (unsigned int lod = 0; lod < lods; ++lod)
        {
            const StaticMeshMaterialList& materials = materialVector(this, geom, lod);
            for (unsigned int i = 0; i < materialCount(materials); ++i)
            {
                StaticMeshMaterial* const material = materialAt(materials, i);
                material->m_template = this;
                material->m_dword1F0 = m_primitiveType;

                staticMeshMaterialSetupTechnique(material);
                staticMeshMaterialBindTextures(material, static_cast<int>(geom),
                                               static_cast<int>(lod), static_cast<int>(i));

                RaMaterialParameters& p = params(material);
                p.posUnpack.value[1] = m_decompressScale.y;
                p.posUnpack.value[0] = m_decompressScale.x;
                p.posUnpack.value[2] = m_decompressScale.z;
                p.texUnpack.value    = m_uvDecompress;

                if (this->getCompressVertexData())
                {
                    float constants[4];
                    raNormalUnpackConstants(constants);
                    if (this->slot_0x84())
                    {
                        p.normalUnpack.value[0] = constants[2];
                        p.normalUnpack.value[1] = constants[3];
                    }
                    else
                    {
                        p.normalUnpack.value[0] = constants[0];
                        p.normalUnpack.value[1] = constants[1];
                    }
                }
                else
                {
                    p.normalUnpack.value[0] = 1.0f;
                    p.normalUnpack.value[1] = 0.0f;
                }
            }
        }
    }

    // The second pass: eight bytes per vertex for every uv set the material's flags do NOT drive.
    // It uses the RAW combo flags, not the quality-masked ones, and it returns 0 without touching
    // the accumulator when the template has no geoms at all. Both shipped.
    if (!geoms)
        return;

    unsigned int bytes = 0;
    for (unsigned int geom = 0; geom < geoms; ++geom)
    {
        const unsigned int lods = lodCount(this, geom);
        if (!lods)
            continue;
        for (unsigned int lod = 0; lod < lods; ++lod)
        {
            const StaticMeshMaterialList& materials = materialVector(this, geom, lod);
            const unsigned int count = materialCount(materials);
            for (unsigned int i = 0; i < count; ++i)
            {
                const StaticMeshMaterial* const material = materialAt(materials, i);
                const unsigned int flags =
                    g_shaderComboFlags[material->m_shaderMaterial->shaderCombo];
                int used = (flags & 2u) != 0 ? 1 : 0;
                if ((flags & 4u)    != 0) ++used;
                if ((flags & 8u)    != 0) ++used;
                if ((flags & 0x10u) != 0) ++used;
                bytes += 8u * static_cast<unsigned int>(
                    static_cast<int>(material->m_vertexCount) * (m_extraUvCount - used));
            }
        }
    }
    g_staticMeshStreamBytes += bytes;
}

} // namespace geom

namespace dx9rend { namespace Ra {
// The same retail encoder is called by both mesh families. Keep its implementation owned above.
unsigned int packVertexNormal(const float* normal)
{
    return ::dice::hfe::geom::packNormal(normal);
}
} }

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
