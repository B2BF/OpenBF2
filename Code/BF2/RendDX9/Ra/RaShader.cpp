// Path: attested by RendDX9.dll __FILE__ at 0x101E2740 (shader build, line 410).
// Ra shader lookup, construction, parameter registration and binding, recovered
// from RendDX9.dll 1.50 (PE only). Each shader owns a queue in the shared Ra table.
// Lookup preserves the default fallback and repeated find-or-create call.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "RaShader.h"
#include "../Texture.h"                    // EngineTexture - the engine-texture holder apply
#include "../../Geom/MeshTemplate.h"       // geom::RaMaterialParameters - the constructor's temporary

#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/PluginAbi.h"
#include "../../../RendDX9Abi/PluginEntry.h"

#include <new>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace dice {
namespace hfe {
namespace dx9rend {

extern void* g_raShaderConstants;    // RenderQueues.cpp - dword_1023DDBC, the process-wide block

// RendDX9Init.cpp - dword_1023B970, the base object.  Two bytes of it are read from this file:
// +0x0C (the D3D device the shader constructor copies) and +0x102 (the [UNRESOLVED] flag
// find-or-create's tail gates on, the same byte BF2Render.cpp's step 14 tests).
extern void* g_rendererBase;

namespace Ra {

namespace {


// Parameter storage is usable, but unknown holder dispatch remains a stopping boundary.
// Lighting's device-listener table is implemented separately.
// [RETIRE-WHEN: the Ra parameter/holder translation unit is recovered.]
// Replace diagnostic tables after holder signatures and behavior are implemented and validated.

void RD9_THUNK raHolderFrontier(void* self RD9_THUNK_EDX)
{
    RD9_FRONTIER_AT("dice::hfe::dx9rend::Ra parameter holder "
                    "(off_101DD3FC / 1E1480 / 1E1488 / 1E148C / 1E1494 / 1E14A8 / 1E1E80 / "
                    "1E1E88 / 1E1E8C / 1E1E94 / 1E1E9C / 1E1EA4 / 1E1EAC / 1E1EB4 / 1E2210 - "
                    "the fifteen unrecovered holder classes of the Ra parameter blocks)",
                    self, 0x00);
}

// Eight slots - longer than any of the shipped tables in this family, so a dispatch through any of
// them lands on a body that names itself and stops rather than running off the end.
void* const kRaHolderVtable[8] =
{
    reinterpret_cast<void*>(&raHolderFrontier), reinterpret_cast<void*>(&raHolderFrontier),
    reinterpret_cast<void*>(&raHolderFrontier), reinterpret_cast<void*>(&raHolderFrontier),
    reinterpret_cast<void*>(&raHolderFrontier), reinterpret_cast<void*>(&raHolderFrontier),
    reinterpret_cast<void*>(&raHolderFrontier), reinterpret_cast<void*>(&raHolderFrontier)
};

void* raHolder() { return const_cast<void*>(static_cast<const void*>(kRaHolderVtable)); }


// Releases lighting texture references on device loss so default-pool resources can reset.
// The next draw binds new textures; reset itself does not recreate them.

void RD9_THUNK raLightingOnDeviceLost(void* self RD9_THUNK_EDX)
{
    RaLightingParameters* const p = static_cast<RaLightingParameters*>(self);
    if (p->terrainLightMap.value)
    {
        static_cast<IUnknown*>(p->terrainLightMap.value)->Release();       // [eax+8]
        p->terrainLightMap.value = 0;
    }
    if (p->lightMap.value)
    {
        static_cast<IUnknown*>(p->lightMap.value)->Release();
        p->lightMap.value = 0;
    }
}

void RD9_THUNK raLightingListenerNullsub(void* RD9_THUNK_EDX)
{
    // 0x10037D40 - `ret`, empty in the original too.
}

// Destroys lighting parameters and frees storage when flags bit zero is set.
void* RD9_THUNK raLightingScalarDeletingDtor(void* self RD9_THUNK_EDX, unsigned int flags)
{
    raLightingParametersDestroy(self);
    if (flags & 1u)
        ::renddx9abi::engineFree(self);
    return self;
}

void* const kRaLightingParametersVtable[4] =
{
    reinterpret_cast<void*>(&raLightingOnDeviceLost),       // +0x00
    reinterpret_cast<void*>(&raLightingListenerNullsub),    // +0x04
    reinterpret_cast<void*>(&raLightingListenerNullsub),    // +0x08
    reinterpret_cast<void*>(&raLightingScalarDeletingDtor), // +0x0C
};

void* raLightingParametersVtable()
{
    return const_cast<void*>(static_cast<const void*>(kRaLightingParametersVtable));
}

// Clears the destination before copying sixteen floats, preserving the original operation order.
void raMat4Assign(float* destination, const float* source)
{
    for (int i = 0; i < 16; ++i)
        destination[i] = 0.0f;
    for (int i = 0; i < 16; ++i)
        destination[i] = source[i];
}

void raMat4Identity(float* m)
{
    // The stack image sub_10045240 and sub_100458E0 build before each call: four 1.0f corners with
    // three memset-zeroed runs of four between them.
    for (int i = 0; i < 16; ++i)
        m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// Four {offsets, names} vector pairs on a 0x20 stride; this file owns three of them (the fourth,
// 0x1023E7B0/0x1023E7C0, belongs to the material block in Geom/StaticMeshMaterial.cpp and is NOT
// redefined here).  The guard every tail uses is "the OFFSET vector is still empty"; the names
// vector is never consulted.
// The offset registries hold each named holder's byte offset inside its block; every block keeps
// its registry's address at the word after its vptr (RaShader::applyParameterVector).

std::vector<int>          g_raLightArrayOffsets;                    // unk_1023E770
std::vector<std::string> g_raLightArrayNames;                      // dword_1023E780
std::vector<int>          g_raLightingOffsets;                      // unk_1023E790
std::vector<std::string> g_raLightingNames;                        // dword_1023E7A0
std::vector<int>          g_raObjectOffsets;                        // unk_1023E7D0
std::vector<std::string> g_raObjectNames;                          // dword_1023E7E0

struct RaParameterName { const char* name; int offset; };

// Register while offsets is empty, including when it has reserved but unused capacity.
void raRegisterNames(std::vector<int>& offsets, std::vector<std::string>& names,
                     const RaParameterName* table, unsigned int count)
{
    if (!offsets.empty())
        return;

    for (unsigned int i = 0; i < count; ++i)
    {
        // Each iteration builds the std::string, pushes it, pushes the offset, and destroys the
        // string - in that order, which is why the temporary is scoped this tightly.
        std::string name(table[i].name);
        names.push_back(name);
        offsets.push_back(table[i].offset);
    }
}


// Registry of shader-visible lighting-block parameter offsets. The retail offsets are the
// RaShader.h member comments (28, 36, 44, 64, 76, 96, 164, 184, 204, 224, 244).

#define RA_LIGHTING_OFFSET(member) static_cast<int>(offsetof(RaLightingParameters, member))
const RaParameterName kRaLightingNames[11] =
{
    { "TerrainLightMap",     RA_LIGHTING_OFFSET(terrainLightMap)     },
    { "LightMap",            RA_LIGHTING_OFFSET(lightMap)            },
    { "LightMapOffset",      RA_LIGHTING_OFFSET(lightMapOffset)      },
    { "MatBones",            RA_LIGHTING_OFFSET(matBones)            },
    { "OverGrowthAmbient",   RA_LIGHTING_OFFSET(overGrowthAmbient)   },
    { "ShadowViewProj",      RA_LIGHTING_OFFSET(shadowViewProj)      },
    { "ShadowViewProjArray", RA_LIGHTING_OFFSET(shadowViewProjArray) },
    { "ClampViewProjArray",  RA_LIGHTING_OFFSET(clampViewProjArray)  },
    { "ShadowPosArray",      RA_LIGHTING_OFFSET(shadowPosArray)      },
    { "ShadowSizeArray",     RA_LIGHTING_OFFSET(shadowSizeArray)     },
    { "ShadowCount",         RA_LIGHTING_OFFSET(shadowCount)         }
};
#undef RA_LIGHTING_OFFSET


// Registry of object transform, wind, and hemi-light parameter offsets. The retail offsets are
// the RaShader.h member comments (8 .. 528).

#define RA_OBJECT_OFFSET(member) static_cast<int>(offsetof(RaObjectParameters, member))
const RaParameterName kRaObjectNames[15] =
{
    { "World",               RA_OBJECT_OFFSET(world)               },
    { "ViewProjection",      RA_OBJECT_OFFSET(viewProjection)      },
    { "WorldViewProjection", RA_OBJECT_OFFSET(worldViewProjection) },
    { "ShadowProjMat",       RA_OBJECT_OFFSET(shadowProjMat)       },
    { "ShadowOccProjMat",    RA_OBJECT_OFFSET(shadowOccProjMat)    },
    { "ShadowTrapMat",       RA_OBJECT_OFFSET(shadowTrapMat)       },
    { "GeomBones",           RA_OBJECT_OFFSET(geomBones)           },
    { "Transparency",        RA_OBJECT_OFFSET(transparency)        },
    { "ObjectSpaceCamPos",   RA_OBJECT_OFFSET(objectSpaceCamPos)   },
    { "WindSpeed",           RA_OBJECT_OFFSET(windSpeed)           },
    { "WindDir",             RA_OBJECT_OFFSET(windDir)             },
    { "UserData",            RA_OBJECT_OFFSET(userData)            },
    { "HeightOverTerrain",   RA_OBJECT_OFFSET(heightOverTerrain)   },
    { "HemiMapSkyColor",     RA_OBJECT_OFFSET(hemiMapSkyColor)     },
    { "ObjectSpaceWorldUp",  RA_OBJECT_OFFSET(objectSpaceWorldUp)  }
};
#undef RA_OBJECT_OFFSET


// Registry for the light-array parameter block and its lazy resolver (retail offsets 8 and 16).

const RaParameterName kRaLightArrayNames[2] =
{
    { "Lights",    static_cast<int>(offsetof(RaLightArrayParameters, lights))       },
    { "Lights_1X", static_cast<int>(offsetof(RaLightArrayParameters, lights1xVptr)) }
};


} // anonymous namespace


// Parameter-holder slot 0 applies a value to an effect handle and returns its HRESULT.
// Fastcall adapters preserve the thiscall table ABI. Remaining destructor slots keep
// their named stops until implemented; the current call paths do not dispatch them.

namespace {

// The value after the holder's binder-table pointer (+0x04 on x86).
inline unsigned char* holderPayload(void* self)
{
    return static_cast<unsigned char*>(self) + sizeof(void*);
}

// off_101E1480 slot 0 - sub_10037E20: SetVector(handle, &value).
int RD9_THUNK raApplyVec4(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetVector(
        ::renddx9abi::d3dxHandle(handle),
        reinterpret_cast<const D3DXVECTOR4*>(holderPayload(self))));
}

// off_101E1488 slot 0 - sub_10037E40: SetTexture(handle, value) - the value IS the D3D base texture.
int RD9_THUNK raApplyTexture(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetTexture(
        ::renddx9abi::d3dxHandle(handle),
        *reinterpret_cast<IDirect3DBaseTexture9**>(holderPayload(self))));
}

// off_101E148C slot 0 - sub_1003A170: SetFloat(handle, value).
int RD9_THUNK raApplyFloat(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetFloat(
        ::renddx9abi::d3dxHandle(handle), *reinterpret_cast<const float*>(holderPayload(self))));
}

// off_101E1494 slot 0 - sub_1003A1A0: SetFloatArray(handle, value, 3).
int RD9_THUNK raApplyVec3(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetFloatArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const float*>(holderPayload(self)), 3u));
}

// off_101E149C slot 0 - sub_1003A1C0: SetFloatArray(handle, value, 2).  This is the table the
// material block installs at its +0x140 ("NormalUnpack"), so that holder is a TWO-float one.
int RD9_THUNK raApplyVec2(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetFloatArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const float*>(holderPayload(self)), 2u));
}

// off_101E14A8 slot 0 - sub_1003A1E0: SetInt(handle, value).
int RD9_THUNK raApplyInt(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetInt(
        ::renddx9abi::d3dxHandle(handle), *reinterpret_cast<const int*>(holderPayload(self))));
}

// off_101E14BC slot 0 - sub_1003A140: nothing (and 0) for a null texture, else SetTexture(handle,
// sub_10014930(texture)) - the engine texture's D3D base object.
int RD9_THUNK raApplyEngineTexture(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    EngineTexture* const texture = *reinterpret_cast<EngineTexture**>(holderPayload(self));
    if (!texture)
        return 0;
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetTexture(
        ::renddx9abi::d3dxHandle(handle), texture->baseTexture));
}

// off_101E1E78 slot 0 - sub_10046580: SetBool(handle, byte).
int RD9_THUNK raApplyBool(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetBool(
        ::renddx9abi::d3dxHandle(handle), static_cast<BOOL>(*holderPayload(self))));
}

// off_101E1E80 slot 0 - sub_100465A0 -> sub_100465C0: SetVector(handle, {f, f, f, f}).
int RD9_THUNK raApplyFloatSplat(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    const float f = *reinterpret_cast<const float*>(holderPayload(self));
    const float splat[4] = { f, f, f, f };
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetVector(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const D3DXVECTOR4*>(splat)));
}

// off_101E1E88 slot 0 - sub_10046450.  The holder is {vptr, lightsHolder, resolver}: the resolver
// (raLightArrayUpdate) runs with ecx = &lightsHolder->value and answers the record handed to
// SetValue(handle, record, 68).
int RD9_THUNK raApplyLightRecordResolved(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    typedef void* (__thiscall* ResolveFn)(void*);
    void** const words = static_cast<void**>(self);
    void* const record = reinterpret_cast<ResolveFn>(words[2])(
        static_cast<unsigned char*>(words[1]) + offsetof(RaParamRef, value));   // +0x04
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetValue(
        ::renddx9abi::d3dxHandle(handle), record, 68u));
}

// off_101E1E8C slot 0 - sub_100465F0: SetMatrixPointerArray(handle, pointers, count), the pair's
// +0x04 and +0x08.
int RD9_THUNK raApplyMatrixPointerArray(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    const RaParamPair* const pair = static_cast<const RaParamPair*>(self);
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetMatrixPointerArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const D3DXMATRIX**>(pair->a), pair->b));
}

// off_101E1E94 slot 0 - sub_10046620: SetMatrix(handle, &value).
int RD9_THUNK raApplyMat4(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetMatrix(
        ::renddx9abi::d3dxHandle(handle),
        reinterpret_cast<const D3DXMATRIX*>(holderPayload(self))));
}

// off_101E1E9C slot 0 - sub_100469F0 -> sub_10046A10: SetValue(handle, bytes, count), and
// SetValue(handle, 0, 0) for an empty vector - the call is made either way.
int RD9_THUNK raApplyBytes(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    const std::vector<unsigned char>& v = static_cast<const RaParamVector*>(self)->value;
    if (v.data())
        return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetValue(
            ::renddx9abi::d3dxHandle(handle), v.data(),
            static_cast<unsigned int>(v.size())));
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetValue(
        ::renddx9abi::d3dxHandle(handle), 0, 0u));
}

// off_101E1EA4 slot 0 - sub_10046BC0: SetMatrixArray(handle, first, bytes / 64) (sub_10046410).
int RD9_THUNK raApplyMat4Array(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    const std::vector<unsigned char>& v = static_cast<const RaParamVector*>(self)->value;
    const unsigned int count = static_cast<unsigned int>(v.size()) >> 6;
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetMatrixArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const D3DXMATRIX*>(v.data()), count));
}

// off_101E1EAC slot 0 - sub_10046BF0: SetVectorArray(handle, first, bytes / 16) (sub_10046430).
int RD9_THUNK raApplyVec4Array(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    const std::vector<unsigned char>& v = static_cast<const RaParamVector*>(self)->value;
    const unsigned int count = static_cast<unsigned int>(v.size()) >> 4;
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetVectorArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const D3DXVECTOR4*>(v.data()), count));
}

// off_101DD3FC slot 0 - sub_1000BE00: SetValue(handle, pointer, 68) - the pointed-at RaLightRecord.
int RD9_THUNK raApplyLightRecordPointer(void* self RD9_THUNK_EDX, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetValue(
        ::renddx9abi::d3dxHandle(handle), *reinterpret_cast<void**>(holderPayload(self)), 68u));
}

#define RD9_RA_HOLDER_TABLE(name, apply)                                                       \
    void* const name[8] =                                                                      \
    {                                                                                          \
        reinterpret_cast<void*>(&apply),                                                       \
        reinterpret_cast<void*>(&raHolderFrontier), reinterpret_cast<void*>(&raHolderFrontier), \
        reinterpret_cast<void*>(&raHolderFrontier), reinterpret_cast<void*>(&raHolderFrontier), \
        reinterpret_cast<void*>(&raHolderFrontier), reinterpret_cast<void*>(&raHolderFrontier), \
        reinterpret_cast<void*>(&raHolderFrontier)                                             \
    }

RD9_RA_HOLDER_TABLE(kRaHolderVec4Vtable,                raApplyVec4);
RD9_RA_HOLDER_TABLE(kRaHolderTextureVtable,             raApplyTexture);
RD9_RA_HOLDER_TABLE(kRaHolderFloatVtable,               raApplyFloat);
RD9_RA_HOLDER_TABLE(kRaHolderVec3Vtable,                raApplyVec3);
RD9_RA_HOLDER_TABLE(kRaHolderVec2Vtable,                raApplyVec2);
RD9_RA_HOLDER_TABLE(kRaHolderIntVtable,                 raApplyInt);
RD9_RA_HOLDER_TABLE(kRaHolderEngineTextureVtable,       raApplyEngineTexture);
RD9_RA_HOLDER_TABLE(kRaHolderBoolVtable,                raApplyBool);
RD9_RA_HOLDER_TABLE(kRaHolderFloatSplatVtable,          raApplyFloatSplat);
RD9_RA_HOLDER_TABLE(kRaHolderLightRecordResolvedVtable, raApplyLightRecordResolved);
RD9_RA_HOLDER_TABLE(kRaHolderMatrixPointerArrayVtable,  raApplyMatrixPointerArray);
RD9_RA_HOLDER_TABLE(kRaHolderMat4Vtable,                raApplyMat4);
RD9_RA_HOLDER_TABLE(kRaHolderBytesVtable,               raApplyBytes);
RD9_RA_HOLDER_TABLE(kRaHolderMat4ArrayVtable,           raApplyMat4Array);
RD9_RA_HOLDER_TABLE(kRaHolderVec4ArrayVtable,           raApplyVec4Array);
RD9_RA_HOLDER_TABLE(kRaHolderLightRecordPointerVtable,  raApplyLightRecordPointer);

#undef RD9_RA_HOLDER_TABLE

inline void* holderTable(void* const* table)
{
    return const_cast<void*>(static_cast<const void*>(table));
}

} // anonymous namespace

void* raHolderVec4()                { return holderTable(kRaHolderVec4Vtable); }
void* raHolderTexture()             { return holderTable(kRaHolderTextureVtable); }
void* raHolderFloat()               { return holderTable(kRaHolderFloatVtable); }
void* raHolderVec3()                { return holderTable(kRaHolderVec3Vtable); }
void* raHolderVec2()                { return holderTable(kRaHolderVec2Vtable); }
void* raHolderInt()                 { return holderTable(kRaHolderIntVtable); }
void* raHolderEngineTexture()       { return holderTable(kRaHolderEngineTextureVtable); }
void* raHolderBool()                { return holderTable(kRaHolderBoolVtable); }
void* raHolderFloatSplat()          { return holderTable(kRaHolderFloatSplatVtable); }
void* raHolderLightRecordResolved() { return holderTable(kRaHolderLightRecordResolvedVtable); }
void* raHolderMatrixPointerArray()  { return holderTable(kRaHolderMatrixPointerArrayVtable); }
void* raHolderMat4()                { return holderTable(kRaHolderMat4Vtable); }
void* raHolderBytes()               { return holderTable(kRaHolderBytesVtable); }
void* raHolderMat4Array()           { return holderTable(kRaHolderMat4ArrayVtable); }
void* raHolderVec4Array()           { return holderTable(kRaHolderVec4ArrayVtable); }
void* raHolderLightRecordPointer()  { return holderTable(kRaHolderLightRecordPointerVtable); }

// Copies the selected light into shared scratch storage and halves its color/intensity words.
// Returns borrowed static storage, overwritten by the next call.
void* RD9_THUNK raLightArrayUpdate(void* lightsValue RD9_THUNK_EDX)
{
    static float s_lights1x[17] =
    {
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 1.0f
    };
    const float* const source = *static_cast<const float* const*>(lightsValue);
    for (int i = 0; i < 17; ++i)
        s_lights1x[i] = source[i];
    s_lights1x[6] *= 0.5f;
    s_lights1x[7] *= 0.5f;
    s_lights1x[8] *= 0.5f;
    s_lights1x[9] *= 0.5f;
    return s_lights1x;
}


// Shared vertex-element descriptors. Aliased semantic names intentionally share addresses;
// the shader constructor stores these pointers alongside its semantic-name vector.

RaVertexElementDesc g_desc_1023ED4C( 2,  1, 5, "Vtx_TBase");   // TBase2D  init 0x101CCC20
RaVertexElementDesc g_desc_1023ED74( 9,  0, 1, "Vtx_BoneWeights");   // Bone2Weights  init 0x101CD240
RaVertexElementDesc g_desc_1023ED9C( 0,  7, 0, "Vtx_PositionPacked");   // PositionPacked, PositionPacked0, PositionPacked1  init 0x101CCA70
RaVertexElementDesc g_desc_1023EDC4( 7,  7, 6, "Vtx_TangentSpacePacked16");   // TangentSpacePacked16  init 0x101CD160
RaVertexElementDesc g_desc_1023EDEC( 2,  6, 5, "Vtx_TBasePacked");   // TBasePacked2D  init 0x101CCC90
// declType 17 is D3DDECLTYPE_UNUSED and is a placeholder. Its real value is now known:
// sub_1004A270 overwrites dword_1023EE18 - which IS this descriptor's declType field, at +0x04 -
// with 14 (D3DDECLTYPE_DEC3N) when caps bit 0x80 is set, else 4 (D3DCOLOR). See the derivation
// beside raNormalUnpackConstants in Geom/StaticMeshMaterial.cpp. What is still missing is the
// call site for the other half of sub_1004A270 (an object reset), so the fix-up is not wired
// here yet. [RETIRE-WHEN: RW-ra-normal-format-writer]
RaVertexElementDesc g_desc_1023EE14( 1, 17, 3, "Vtx_NormalPacked8");   // NormalPacked8  init 0x101CCB40
RaVertexElementDesc g_desc_1023EE3C( 5,  6, 5, "Vtx_TCrackPacked");   // TCrackPacked2D  init 0x101CCF30
RaVertexElementDesc g_desc_1023EE64( 6,  6, 5, "Vtx_TLightMapPacked");   // TLightMapPacked2D  init 0x101CD010
RaVertexElementDesc g_desc_1023EE8C( 3,  6, 5, "Vtx_TDetailPacked");   // TDetailPacked2D, TUVAnimationPacked2D  init 0x101CCD70
// Same placeholder, same writer: dword_1023EEB8 is this descriptor's declType (+0x04).
RaVertexElementDesc g_desc_1023EEB4( 7, 17, 6, "Vtx_TangentSpacePacked8");   // TangentSpacePacked8  init 0x101CD0F0
RaVertexElementDesc g_desc_1023EEDC( 8,  4, 2, "Vtx_BoneIdcs");   // Bone4Idcs  init 0x101CD1D0
RaVertexElementDesc g_desc_1023EF04( 0,  2, 0, "Vtx_Position");   // Position, Position0, Position1  init 0x101CCA10
RaVertexElementDesc g_desc_1023EF2C( 6,  1, 5, "Vtx_TLightMap");   // TLightMap2D  init 0x101CCFA0
RaVertexElementDesc g_desc_1023EF54( 1,  7, 3, "Vtx_NormalPacked16");   // NormalPacked16  init 0x101CCBB0
RaVertexElementDesc g_desc_1023EF7C( 1,  2, 3, "Vtx_Normal");   // Normal  init 0x101CCAD0
RaVertexElementDesc g_desc_1023EFA4( 4,  6, 5, "Vtx_TDirtPacked");   // TDirtPacked2D  init 0x101CCE50
RaVertexElementDesc g_desc_1023EFCC( 4,  1, 5, "Vtx_TDirt");   // TDirt2D  init 0x101CCDE0
RaVertexElementDesc g_desc_1023EFF4( 7,  2, 6, "Vtx_TangentSpace");   // TangentSpace  init 0x101CD080
RaVertexElementDesc g_desc_1023F01C( 3,  1, 5, "Vtx_TDetail");   // TDetail2D, TUVAnimation2D  init 0x101CCD00
RaVertexElementDesc g_desc_1023F044( 5,  1, 5, "Vtx_TCrack");   // TCrack2D  init 0x101CCEC0

namespace {

struct RaVertexElementEntry { const char* name; const RaVertexElementDesc* desc; };

// Preserve registration order; five temporary blocks are constructed between entries 20 and 21.
const RaVertexElementEntry kRaVertexElements[26] =
{
    { "Position",             &g_desc_1023EF04 },   //  1
    { "PositionPacked",       &g_desc_1023ED9C },   //  2
    { "Normal",               &g_desc_1023EF7C },   //  3
    { "NormalPacked8",        &g_desc_1023EE14 },   //  4
    { "NormalPacked16",       &g_desc_1023EF54 },   //  5
    { "TBase2D",              &g_desc_1023ED4C },   //  6
    { "TBasePacked2D",        &g_desc_1023EDEC },   //  7
    { "TDetail2D",            &g_desc_1023F01C },   //  8
    { "TDetailPacked2D",      &g_desc_1023EE8C },   //  9
    { "TDirt2D",              &g_desc_1023EFCC },   // 10
    { "TDirtPacked2D",        &g_desc_1023EFA4 },   // 11
    { "TCrack2D",             &g_desc_1023F044 },   // 12
    { "TCrackPacked2D",       &g_desc_1023EE3C },   // 13
    { "TLightMap2D",          &g_desc_1023EF2C },   // 14
    { "TLightMapPacked2D",    &g_desc_1023EE64 },   // 15
    { "TangentSpace",         &g_desc_1023EFF4 },   // 16
    { "TangentSpacePacked8",  &g_desc_1023EEB4 },   // 17
    { "TangentSpacePacked16", &g_desc_1023EDC4 },   // 18  <- first row using the out-of-line push
    { "Bone4Idcs",            &g_desc_1023EEDC },   // 19
    { "Bone2Weights",         &g_desc_1023ED74 },   // 20  <- THE five stack blocks GO here
    { "TUVAnimation2D",       &g_desc_1023F01C },   // 21
    { "TUVAnimationPacked2D", &g_desc_1023EE8C },   // 22
    { "Position0",            &g_desc_1023EF04 },   // 23
    { "Position1",            &g_desc_1023EF04 },   // 24
    { "PositionPacked0",      &g_desc_1023ED9C },   // 25
    { "PositionPacked1",      &g_desc_1023ED9C }    // 26
};

const unsigned int kRaVertexElementSplit = 20;   // rows [0,20) run before the blocks, [20,26) after

// sub_10011220 (PE 0x10011220) - push_back on the name vector.  The `0x92492493` multiply in the
// shipped body is the compiler's division by 28, i.e. VC7.1's sizeof(std::string).
void raPushName(std::vector<std::string>& v, const std::string& name)
{
    v.push_back(name);
}

// sub_1004D3E0 (PE 0x1004D3E0) and its seventeen inlined copies - push_back of one dword.
void raPushDescriptor(std::vector<const RaVertexElementDesc*>& v, const RaVertexElementDesc* desc)
{
    v.push_back(desc);
}


// [FRONTIER] Primary shader slots other than build retain diagnostic abort thunks.
// Preserve the seven-slot table and its trailing null entry.

#define RD9_RASHADER_FRONTIER(fn, slotOffset, addr)                                   \
    void RD9_THUNK fn(void* self RD9_THUNK_EDX)                                             \
    {                                                                                 \
        RD9_FRONTIER_AT("dice::hfe::dx9rend::Ra::RaShader::slot_" #slotOffset          \
                        " (" #addr ")", self, slotOffset);                            \
    }

RD9_RASHADER_FRONTIER(ras_slot00, 0x00, 0x1004DB40)

// +0x08 .. +0x18 - ; thin __fastcall adapters over the typed members below, as
// slot +0x04's is.  +0x08 and +0x10 .. +0x18 take the entry on the stack (`retn 4`); +0x0C takes
// nothing.
unsigned int RD9_THUNK ras_slot08(void* self RD9_THUNK_EDX, RenderQueueEntry* entry)
{
    return static_cast<RaShader*>(self)->begin(entry);
}
void RD9_THUNK ras_slot0C(void* self RD9_THUNK_EDX)
{
    static_cast<RaShader*>(self)->end();
}
void RD9_THUNK ras_slot10(void* self RD9_THUNK_EDX, RenderQueueEntry* entry)
{
    static_cast<RaShader*>(self)->applyInstanceParameters(entry);
}
void RD9_THUNK ras_slot14(void* self RD9_THUNK_EDX, RenderQueueEntry* entry)
{
    static_cast<RaShader*>(self)->applyTemplateParameters(entry);
}
void RD9_THUNK ras_slot18(void* self RD9_THUNK_EDX, RenderQueueEntry* entry)
{
    static_cast<RaShader*>(self)->draw(entry);
}

#undef RD9_RASHADER_FRONTIER

// +0x04, sub_1004B4E0.  `__thiscall`, no stack arguments (the body's terminator is a bare `retn`),
// the result in al.  Written as the table's other entries are: a thin __fastcall adapter over the
// typed member, so nothing in the table is a raw seam with an invented signature.
unsigned char RD9_THUNK ras_slot04(void* self RD9_THUNK_EDX)
{
    return static_cast<RaShader*>(self)->build() ? 1u : 0u;
}

void* const kRaShaderVtable[8] =
{
    reinterpret_cast<void*>(&ras_slot00),
    reinterpret_cast<void*>(&ras_slot04),
    reinterpret_cast<void*>(&ras_slot08),
    reinterpret_cast<void*>(&ras_slot0C),
    reinterpret_cast<void*>(&ras_slot10),
    reinterpret_cast<void*>(&ras_slot14),
    reinterpret_cast<void*>(&ras_slot18),
    0                                        // +0x1C - null in the shipped table too
};

// Compatibility FileInfo storage must include its live string and all 64 bytes.
// Existence checks discard the fields, but the file manager still writes the whole record.

bool fileManagerResolves(const std::string& path)
{
    // [CHOSEN, not recovered] Permit loading before the host publishes FileManager.
    if (!::dice::hfe::fileManager)
        return false;

    const ::renddx9abi::HostString hostPath = ::renddx9abi::makeHostString(path);

    ::dice::hfe::io::FileInfo info;
    return ::dice::hfe::fileManager->getFileInfo(hostPath, info);
}

// sub_10074E80, PE 0x10074E80 - `std::string lowered(in); pathToLower(lowered); return lowered;`.
// pathToLower is sub_10074D10, which RendDX9Base.h already publishes.
void raToLower(std::string& out, const std::string& in)
{
    out = in.c_str();
    pathToLower(out);
}

// sub_10075110, case-insensitive find, reproduced here rather than reached because
// Geom/StaticMeshTemplate.cpp's copy of it is file-local to that translation unit.  Both lower a
// copy of each string with pathToLower and run std::string::find on the lowered pair.
bool raContainsInsensitive(const std::string& haystack, const std::string& needle)
{
    std::string loweredHaystack(haystack.c_str());
    std::string loweredNeedle(needle.c_str());
    pathToLower(loweredHaystack);
    pathToLower(loweredNeedle);
    return ::strstr(loweredHaystack.c_str(), loweredNeedle.c_str()) != 0;
}


// Shared formatting buffer; overflow replaces the message with a fixed warning.
char g_raFormatBuffer[0x1000];

void raFormatString(std::string& out, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = ::_vsnprintf(g_raFormatBuffer, 0xFFF, format, args);
    va_end(args);

    if (written != -1)
        g_raFormatBuffer[written] = '\0';
    else
        ::sprintf(g_raFormatBuffer, "%s",
                  "WARNING: The original message was too long for formatString(). \n");

    out = g_raFormatBuffer;
}

// std::operator==<char>(const string&, const string&) - size first, then a byte compare, as the
// shipped inline expansion does.
bool raStringEqual(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && ::memcmp(a.c_str(), b.c_str(), a.size()) == 0;
}

// sub_1004C990, std::find over a std::string array.  Returns the element index or
// -1.  The shipped body returns the iterator through a hidden ecx return slot and every caller then
// does the same `(found - _First) / 28` division; folding that in here changes nothing observable.
int raFindString(const std::vector<std::string>& names, const std::string& wanted)
{
    std::vector<std::string>::const_iterator it = names.begin();
    for (; it != names.end(); ++it)
        if (raStringEqual(*it, wanted))
            break;
    if (it == names.end())
        return -1;
    return static_cast<int>(it - names.begin());
}

// sub_1004CA60, the plain-name lookup, `retn 4`.
template <typename Names>
int raRegistryIndexOfPlain(const Names& names, const std::string& parameter)
{
    return raFindString(names, parameter);
}

// Returns the name-registry index, trying the shader-model suffix before the plain name.
// The result is an index into the parallel offset table, not a byte offset.
template <typename Names>
int raRegistryIndexOf(const Names& names, const std::string& parameter, int shaderModel)
{
    int index = -1;
    {
        std::string versioned;
        raFormatString(versioned, "%s_%iX", parameter.c_str(), shaderModel);
        index = raFindString(names, versioned);
    }
    if (index < 0)
        return raRegistryIndexOfPlain(names, parameter);
    return index;
}

// One registry probe, written the way the shipped body writes it: build a temporary std::string,
// ask, destroy the temporary; and on a hit build a second temporary and ask again for the value.
// Two calls and two temporaries per hit - folded into one lookup it would still answer the same,
// but the doubling is what the instruction stream does.
template <typename Names>
void raProbeRegistry(const Names& names, unsigned int blockIndex,
                     const char* parameter, int shaderModel,
                     RaBoundParameter& record, unsigned int& block)
{
    {
        const std::string probe(parameter);
        if (raRegistryIndexOf(names, probe, shaderModel) == -1)
            return;
    }
    block = blockIndex;
    {
        const std::string fetch(parameter);
        record.offset = raRegistryIndexOf(names, fetch, shaderModel);
    }
    record.name = parameter;
}

// Monotonically assigned shader-build serial.
unsigned int g_raShaderBuildSerial = 0;

} // anonymous namespace


// sub_10045240, PE 0x10045240.

RaLightingParameters::RaLightingParameters()
{
    RaLightingParameters* const p = this;

    p->registry = &g_raLightingOffsets;      // &unk_1023E790 - see the file header
    p->vptr     = raLightingParametersVtable();   // off_101E1EB4 - the listener table (dc_core)

    p->unnamed008.vptr = raHolderVec4();
    p->unnamed008.value[0] = 1.0f; p->unnamed008.value[1] = 1.0f;
    p->unnamed008.value[2] = 1.0f; p->unnamed008.value[3] = 1.0f;

    p->terrainLightMap.vptr  = raHolderTexture();
    p->terrainLightMap.value = 0;
    p->lightMap.vptr         = raHolderTexture();
    p->lightMap.value        = 0;

    p->lightMapOffset.vptr = raHolderVec4();
    p->lightMapOffset.value[0] = 1.0f; p->lightMapOffset.value[1] = 1.0f;
    p->lightMapOffset.value[2] = 1.0f; p->lightMapOffset.value[3] = 1.0f;

    // Initialize the value pair before publishing its holder table.
    p->matBones.a    = 0;
    p->matBones.b    = 0;
    p->matBones.vptr = raHolderMatrixPointerArray();

    p->overGrowthAmbient.vptr = raHolderVec4();
    p->overGrowthAmbient.value[0] = 1.0f; p->overGrowthAmbient.value[1] = 1.0f;
    p->overGrowthAmbient.value[2] = 1.0f; p->overGrowthAmbient.value[3] = 1.0f;

    {
        float identity[16];
        raMat4Identity(identity);
        p->shadowViewProj.vptr = raHolderMat4();
        raMat4Assign(p->shadowViewProj.value, identity);
    }

    // The four array holders, each copy-constructed from an empty vector: they start empty. The
    // 64/16-byte strides are the `sar 6` / `sar 4` in sub_10046CC0 and sub_10046D60.
    p->shadowViewProjArray.vptr = raHolderMat4Array();    // off_101E1EA4, stride 64
    p->clampViewProjArray.vptr  = raHolderMat4Array();    // off_101E1EA4, stride 64
    p->shadowPosArray.vptr      = raHolderVec4Array();    // off_101E1EAC, stride 16
    p->shadowSizeArray.vptr     = raHolderVec4Array();    // off_101E1EAC, stride 16

    p->shadowCount.vptr  = raHolderInt();
    p->shadowCount.value = 0;

    p->tail10C       = 0;                                  // tail0FC starts empty

    raRegisterNames(g_raLightingOffsets, g_raLightingNames, kRaLightingNames, 11);

    // The last instruction of the shipped body.  Priority 3, the same one RaShaderConstants and
    // BufferManager register with.  The matching detach is the first thing sub_10044120 does, so a
    // stack-lifetime block leaves the list exactly as it found it.
    registerDeviceLostResetListener(static_cast<void*>(p), 3);
}

void raLightingParametersConstruct(void* storage)
{
    new (storage) RaLightingParameters();
}


// sub_10044120, PE 0x10044120.  An ordinary VC7.1 destructor: detach, free the owned buffers,
// republish each member's vptr on the way out, and release the two reference holders through their
// own vtable slot +0x08.

// The vectors' storage is released at the shipped points; their destructors then find them empty.
RaLightingParameters::~RaLightingParameters()
{
    RaLightingParameters* const p = this;

    p->vptr = raLightingParametersVtable();             // off_101E1EB4, republished first
    unregisterDeviceLostResetListener(static_cast<void*>(p));

    std::vector<unsigned char>().swap(p->tail0FC);

    p->shadowCount.vptr = raHolderInt();

    p->shadowSizeArray.vptr = raHolderVec4Array();
    std::vector<unsigned char>().swap(p->shadowSizeArray.value);

    p->shadowPosArray.vptr = raHolderVec4Array();
    std::vector<unsigned char>().swap(p->shadowPosArray.value);

    p->clampViewProjArray.vptr = raHolderMat4Array();
    std::vector<unsigned char>().swap(p->clampViewProjArray.value);

    p->shadowViewProjArray.vptr = raHolderMat4Array();
    std::vector<unsigned char>().swap(p->shadowViewProjArray.value);

    p->shadowViewProj.vptr    = raHolderMat4();
    p->overGrowthAmbient.vptr = raHolderVec4();
    p->matBones.vptr          = raHolderMatrixPointerArray();
    p->lightMapOffset.vptr    = raHolderVec4();

    // Restore the holder table before releasing its texture through IBase::release.
    {
        void* const held = p->lightMap.value;
        p->lightMap.vptr = raHolderTexture();
        if (held)
        {
            void** const vtbl = *reinterpret_cast<void***>(held);
            reinterpret_cast<int (__stdcall*)(void*)>(vtbl[2])(held);
        }
    }
    {
        void* const held = p->terrainLightMap.value;
        p->terrainLightMap.vptr = raHolderTexture();
        if (held)
        {
            void** const vtbl = *reinterpret_cast<void***>(held);
            reinterpret_cast<int (__stdcall*)(void*)>(vtbl[2])(held);
        }
    }

    p->unnamed008.vptr = raHolderVec4();
}

void raLightingParametersDestroy(void* storage)
{
    static_cast<RaLightingParameters*>(storage)->~RaLightingParameters();
}


// sub_100458E0, PE 0x100458E0.

RaObjectParameters::RaObjectParameters()
{
    RaObjectParameters* const p = this;
    void* const holder = raHolder();

    p->registry = &g_raObjectOffsets;        // &unk_1023E7D0
    p->vptr     = holder;

    {
        // The same identity matrix is rebuilt on the stack before every one of the six assigns,
        // which is what the shipped body does rather than building it once.
        float identity[16];

        raMat4Identity(identity);
        p->world.vptr = raHolderMat4();
        raMat4Assign(p->world.value, identity);

        raMat4Identity(identity);
        p->viewProjection.vptr = raHolderMat4();
        raMat4Assign(p->viewProjection.value, identity);

        raMat4Identity(identity);
        p->worldViewProjection.vptr = raHolderMat4();
        raMat4Assign(p->worldViewProjection.value, identity);

        raMat4Identity(identity);
        p->shadowProjMat.vptr = raHolderMat4();
        raMat4Assign(p->shadowProjMat.value, identity);

        raMat4Identity(identity);
        p->shadowOccProjMat.vptr = raHolderMat4();
        raMat4Assign(p->shadowOccProjMat.value, identity);

        raMat4Identity(identity);
        p->shadowTrapMat.vptr = raHolderMat4();
        raMat4Assign(p->shadowTrapMat.value, identity);
    }

    p->geomBones.vptr = raHolderMatrixPointerArray();
    p->geomBones.a    = 0;
    p->geomBones.b    = 0;

    p->transparency.vptr  = raHolderFloatSplat();
    p->transparency.value = 1.0f;

    p->objectSpaceCamPos.vptr = raHolderVec4();
    p->objectSpaceCamPos.value[0] = 1.0f; p->objectSpaceCamPos.value[1] = 1.0f;
    p->objectSpaceCamPos.value[2] = 1.0f; p->objectSpaceCamPos.value[3] = 1.0f;

    // Initialize the value before installing the holder table.
    p->windSpeed.value = 0.0f;
    p->windSpeed.vptr  = raHolderFloat();

    p->windDir.vptr = raHolderVec3();
    p->windDir.value[0] = 0.0f; p->windDir.value[1] = 0.0f; p->windDir.value[2] = 0.0f;

    p->userData.vptr = raHolderBytes();               // off_101E1E9C - a byte vector, empty

    p->heightOverTerrain.vptr  = raHolderFloat();
    p->heightOverTerrain.value = 0.0f;

    p->hemiMapSkyColor.vptr = raHolderVec4();
    p->hemiMapSkyColor.value[0] = 1.0f; p->hemiMapSkyColor.value[1] = 1.0f;
    p->hemiMapSkyColor.value[2] = 1.0f; p->hemiMapSkyColor.value[3] = 1.0f;

    p->objectSpaceWorldUp.vptr = raHolderVec3();
    p->objectSpaceWorldUp.value[0] = 0.0f;
    p->objectSpaceWorldUp.value[1] = 0.0f;
    p->objectSpaceWorldUp.value[2] = 0.0f;
    p->tail220 = 0;

    raRegisterNames(g_raObjectOffsets, g_raObjectNames, kRaObjectNames, 15);
}

void raObjectParametersConstruct(void* storage)
{
    new (storage) RaObjectParameters();
}


// sub_1004D900, PE 0x1004D900.  A pure teardown: republish thirteen member vptrs in reverse
// declaration order and free the one owned buffer.  It does NOT detach a device-lost listener,
// because sub_100458E0 never registers one.

RaObjectParameters::~RaObjectParameters()
{
    RaObjectParameters* const p = this;

    p->objectSpaceWorldUp.vptr = raHolderVec3();
    p->hemiMapSkyColor.vptr    = raHolderVec4();
    p->heightOverTerrain.vptr  = raHolderFloat();

    p->userData.vptr = raHolderBytes();
    std::vector<unsigned char>().swap(p->userData.value);   // freed here; its destructor finds it empty

    p->windDir.vptr           = raHolderVec3();
    p->windSpeed.vptr         = raHolderFloat();
    p->objectSpaceCamPos.vptr = raHolderVec4();
    p->transparency.vptr      = raHolderFloatSplat();
    p->geomBones.vptr         = raHolderMatrixPointerArray();

    p->shadowTrapMat.vptr       = raHolderMat4();
    p->shadowOccProjMat.vptr    = raHolderMat4();
    p->shadowProjMat.vptr       = raHolderMat4();
    p->worldViewProjection.vptr = raHolderMat4();
    p->viewProjection.vptr      = raHolderMat4();
    p->world.vptr               = raHolderMat4();
}

void raObjectParametersDestroy(void* storage)
{
    static_cast<RaObjectParameters*>(storage)->~RaObjectParameters();
}


// sub_10044F70, PE 0x10044F70.

void raLightArrayParametersConstruct(void* storage)
{
    RaLightArrayParameters* const p = static_cast<RaLightArrayParameters*>(storage);
    void* const holder = raHolder();

    p->registry = &g_raLightArrayOffsets;    // &unk_1023E770
    p->vptr     = holder;

    p->lights.vptr  = raHolderLightRecordPointer();
    p->lights.value = 0;

    p->lights1xVptr = raHolderLightRecordResolved();
    p->lights1xSelf = &p->lights;            // this + 8
    p->lights1xFn   = reinterpret_cast<void*>(&raLightArrayUpdate);

    raRegisterNames(g_raLightArrayOffsets, g_raLightArrayNames, kRaLightArrayNames, 2);
}

// The one-instruction inlined destructor: republish the +0x08 holder's vptr and nothing else.
void raLightArrayParametersDestroyInline(void* storage)
{
    static_cast<RaLightArrayParameters*>(storage)->lights.vptr = raHolderLightRecordPointer();
}


// sub_10046540, the 0x44 light record.  Fourteen zero stores
// (dwords 0..13), then 1.0f at dword 15 and dword 16.  Dword 14 (+0x38, the inverse squared
// range) is deliberately NOT written, exactly as shipped; the StaticMesh draw stores it per light.

RaLightRecord* raLightRecordConstruct(void* storage)
{
    RaLightRecord* const r = static_cast<RaLightRecord*>(storage);
    r->position[0] = 0.0f; r->position[1] = 0.0f; r->position[2] = 0.0f;
    r->direction[0] = 0.0f; r->direction[1] = 0.0f; r->direction[2] = 0.0f;
    r->color[0] = 0.0f; r->color[1] = 0.0f; r->color[2] = 0.0f;
    r->scaled24 = 0.0f;
    r->unread28[0] = 0.0f; r->unread28[1] = 0.0f; r->unread28[2] = 0.0f; r->unread28[3] = 0.0f;
    r->float3C = 1.0f;
    r->float40 = 1.0f;
    return r;
}

unsigned int raLightingParametersSize()   { return sizeof(RaLightingParameters); }
unsigned int raObjectParametersSize()     { return sizeof(RaObjectParameters); }
unsigned int raLightArrayParametersSize() { return sizeof(RaLightArrayParameters); }


// sub_1004BDC0, the 0x178-byte shader's constructor.

RaShader::RaShader(const std::string& shaderName, void* renderQueue)
    : m_name(shaderName)     // +0x118 - a copy of the caller's lowered name.  The shipped body
                             // builds it between the seventeen vector clears and the +0x134 byte;
                             // a member initialiser is the only way to say that in source without
                             // constructing the string twice.
{
    m_queue = renderQueue;                       // +0x04, written before the vptr
    m_vptr  = const_cast<void*>(static_cast<const void*>(kRaShaderVtable));

    // The seventeen vectors at +0x08 .. +0x108 are constructed empty, which is what the shipped
    // constructor's _First/_Last/_End clears amount to.

    m_byte134  = 1;
    m_pad135[0] = m_pad135[1] = m_pad135[2] = 0;
    m_dword138 = 0;
    m_float150 = 1.0f;
    m_queueProfiler = 0;

    // The technique map at +0x158 is constructed empty too (sub_1004CAF0's 24-byte sentinel).

    m_activeTechnique = 0;
    m_technique = 0;
    m_effect    = 0;
    m_built     = 0;
    m_pad171[0] = m_pad171[1] = m_pad171[2] = 0;
    m_buildSerial = -1;

    // g_rendererBase[+0x0C] - the D3D device.  Read straight out of the base object, exactly as
    // Texture.cpp and QueryManager.cpp read it, rather than from any renderer accessor.  The
    // shipped pair is two instructions with no test (`mov edx, dword_1023B970; mov eax, [edx+0Ch]`);
    // the null test below is the same deviation every other recovered reader of that global in this
    // tree takes, including BF2Render.cpp's step 14 and Ra/EffectManager.cpp's define builder.
    m_device = 0;
    if (g_rendererBase)
    {
        m_device = static_cast<const RendDX9Base*>(g_rendererBase)->device;
    }

    // Rows 1..20 of the vertex-element table.
    for (unsigned int i = 0; i < kRaVertexElementSplit; ++i)
    {
        std::string name(kRaVertexElements[i].name);
        raPushName(m_vertexElementNames, name);
        raPushDescriptor(m_vertexElementDescriptors, kRaVertexElements[i].desc);
    }


    // Temporary parameter blocks trigger one-time registry initialization.
    // Preserve construction and destruction order, including light-array teardown first.
    // The material block is 0x150 bytes and the constants block 0xAFC on x86; both are native
    // sizes on x64 (issue #7).
    alignas(geom::RaMaterialParameters)
        unsigned char materialStorage[sizeof(geom::RaMaterialParameters)];
    alignas(void*) unsigned char constantsStorage[kRaShaderConstantsSize];
    alignas(RaLightingParameters) unsigned char lightingStorage[sizeof(RaLightingParameters)];
    alignas(RaObjectParameters) unsigned char   objectStorage[sizeof(RaObjectParameters)];
    RaLightArrayParameters lightArrayStorage;

    raMaterialParametersConstruct(materialStorage);
    raLightingParametersConstruct(lightingStorage);
    raObjectParametersConstruct(objectStorage);

    // sub_10038BF0(stack, 0xFFFFFFFF) - a whole RaShaderConstants built on this frame with the
    // device argument -1.  Not a typo in the transcription: `push 0FFFFFFFFh`.  All ones on x64.
    raShaderConstantsConstructInPlace(constantsStorage,
                                      reinterpret_cast<void*>(~static_cast<uintptr_t>(0)));

    raLightArrayParametersConstruct(&lightArrayStorage);

    // Rows 21..26 of the vertex-element table, with the blocks alive.
    for (unsigned int i = kRaVertexElementSplit; i < 26; ++i)
    {
        std::string name(kRaVertexElements[i].name);
        raPushName(m_vertexElementNames, name);
        raPushDescriptor(m_vertexElementDescriptors, kRaVertexElements[i].desc);
    }

    raLightArrayParametersDestroyInline(&lightArrayStorage);  // the one inlined store
    raShaderConstantsDestroyInPlace(constantsStorage);
    raObjectParametersDestroy(objectStorage);
    raLightingParametersDestroy(lightingStorage);
    raMaterialParametersDestroy(materialStorage);
}


// Render-queue shader application helpers.

namespace {

// dword_1023F3D4 - read by sub_1004B340 alone and written by nothing in the image: the entry's
// clip planes are never released after use in retail.  Kept as the word it is.
unsigned int g_raReleaseClipPlanesAfterUse = 0;

} // anonymous namespace

// Maps each valid effect technique's case-insensitive name hash to its handle.
void RaShader::fillTechniqueMap()
{
    ID3DXEffect* const effect = static_cast<ID3DXEffect*>(m_effect);

    D3DXHANDLE handle = 0;
    for (long hr = effect->FindNextValidTechnique(0, &handle);
         hr >= 0;
         hr = effect->FindNextValidTechnique(handle, &handle))
    {
        if (!handle)
            break;
        D3DXTECHNIQUE_DESC desc = { 0, 0, 0 };
        effect->GetTechniqueDesc(handle, &desc);
        const unsigned int key = raHashString(std::string(desc.Name));
        m_techniques[key] = ::renddx9abi::d3dxHandleValue(handle);   // sub_1004D9C0 / sub_1004D460
    }
}

// sub_1004B3D0, PE 0x1004B3D0.  An empty map is filled first.  A miss inserts a null entry for
// the key (operator[] semantics, as shipped) and answers false with nothing else changed.
bool RaShader::selectTechnique(unsigned int key)
{
    if (m_techniques.empty())
        fillTechniqueMap();
    void* const handle = m_techniques[key];
    if (!handle)
        return false;
    m_activeTechnique = handle;
    return true;
}

// +0x08, sub_1004B410 (0xC9 B).
unsigned int RaShader::begin(RenderQueueEntry* entry)
{
    if (!m_built && !slot04())                                    // the +0x04 build, by vtable
        return 0;

    unsigned int passes = 0;
    if (m_effect)
    {
        if (entry->techniqueKey)
        {
            if (!selectTechnique(entry->techniqueKey))
                m_activeTechnique = m_technique;
        }
        else
        {
            m_activeTechnique = m_technique;
        }
        stateManagerPush();
        shaderManagerSetActiveRaEffect(m_effect);
        static_cast<ID3DXEffect*>(m_effect)->SetTechnique(
            ::renddx9abi::d3dxHandle(m_activeTechnique));
        static_cast<ID3DXEffect*>(m_effect)->Begin(&passes, 7u);
        applyParameterSet(m_globalParameters, entry);            // sub_1004B210(this + 0x58)
    }

    return passes;
}

// +0x0C, sub_1004AEA0.
void RaShader::end()
{
    if (!m_effect)
        return;
    static_cast<ID3DXEffect*>(m_effect)->End();
    shaderManagerSetActiveRaEffect(0);
    stateManagerPop();
}


void RaShader::beginPass(unsigned int pass)
{
    if (m_effect)
        static_cast<ID3DXEffect*>(m_effect)->BeginPass(pass);
}

void RaShader::endPass()
{
    if (m_effect)
        static_cast<ID3DXEffect*>(m_effect)->EndPass();
}

// +0x10, sub_1004B340 (0x8A B).  The instance set (effect-gated), then every 16-byte plane of the
// entry's lighting.tail0FC onto the device as a clip plane, indexed from 0.
void RaShader::applyInstanceParameters(RenderQueueEntry* entry)
{
    if (m_effect)
        applyParameterSet(m_instanceParameters, entry);           // sub_1004B210(this + 0x08)

    std::vector<unsigned char>& planes = entry->lighting.tail0FC;
    const unsigned int planeCount = static_cast<unsigned int>(planes.size()) / 16;
    for (unsigned int index = 0; index < planeCount; ++index)
        m_device->SetClipPlane(index, reinterpret_cast<const float*>(&planes[16 * index]));   // device +0xDC

    if (g_raReleaseClipPlanesAfterUse)                            // dword_1023F3D4, never set
        std::vector<unsigned char>().swap(planes);
}

// +0x14, sub_1004B280.
void RaShader::applyTemplateParameters(RenderQueueEntry* entry)
{
    if (m_effect)
        applyParameterSet(m_templateParameters, entry);           // sub_1004B210(this + 0xA8)
}

// sub_1004B210, PE 0x1004B210.  One five-vector set against the entry's blocks, in the shipped
// order: the material block, the object block, the process-wide RaShaderConstants, the light
// array, the lighting block.
void RaShader::applyParameterSet(std::vector<RaBoundParameter>* set, RenderQueueEntry* entry)
{
    applyParameterVector(entry->materialParameters, set[0]);
    applyParameterVector(entry->objectParameters,   set[1]);
    applyParameterVector(g_raShaderConstants,       set[2]);      // dword_1023DDBC
    applyParameterVector(&entry->lightArray,        set[4]);
    applyParameterVector(&entry->lighting,          set[3]);
}

// sub_1004D280, PE 0x1004D280.  For every 80-byte bound parameter: its registry index, through the
// block's own offset vector (the word after the block's vptr -> std::vector<int>), names the byte
// offset of the holder inside the block; the holder's slot 0 applies it to this shader's effect
// under the bound D3DXHANDLE.  No null test on the block, the registry or the holder, as shipped.
void RaShader::applyParameterVector(void* block, const std::vector<RaBoundParameter>& parameters)
{
    const unsigned int count = static_cast<unsigned int>(parameters.size());
    if (!count)
        return;

    typedef int (__thiscall* ApplyFn)(void*, void*, void*);
    struct BlockHeader { void* vptr; const std::vector<int>* registry; };
    unsigned char* const bytes = static_cast<unsigned char*>(block);
    const std::vector<int>& offsets = *reinterpret_cast<const BlockHeader*>(bytes)->registry;
    for (unsigned int i = 0; i < count; ++i)
    {
        const RaBoundParameter& bound = parameters[i];
        unsigned char* const holder = bytes + offsets[bound.offset];
        reinterpret_cast<ApplyFn>((*reinterpret_cast<void***>(holder))[0])(
            holder, m_effect, bound.handle);
    }
}

// +0x18, sub_1004AF30 (0x8C B).  The material's primitive count decides everything; the vertex
// count is fetched beside it and not used until the argument list.  Then the state manager's
// pre-draw apply and the device's DrawIndexedPrimitive straight from the material's getters, the
// entry's lighting.tail10C added to the index start - no renderer wrapper, no statistics.  The
// getters are the StaticMeshMaterial table's, by slot: +0x10 primitives, +0x08 vertices, +0x20
// index start, +0x1C the zero slot, +0x18 vertex start, +0x14 the primitive type word.
void RaShader::draw(RenderQueueEntry* entry)
{
    typedef int (__thiscall* IntSlotFn)(void*);
    void* const material = entry->material;
    void** const slots = *static_cast<void***>(material);

    const int primitiveCount = reinterpret_cast<IntSlotFn>(slots[0x10 / 4])(material);
    (void)reinterpret_cast<IntSlotFn>(slots[0x08 / 4])(material);
    if (!primitiveCount)
        return;

    stateManagerPreDrawApply();

    const int startIndex  = reinterpret_cast<IntSlotFn>(slots[0x20 / 4])(material)
                          + static_cast<int>(entry->lighting.tail10C);
    const int numVertices = reinterpret_cast<IntSlotFn>(slots[0x08 / 4])(material);
    const int minIndex    = reinterpret_cast<IntSlotFn>(slots[0x1C / 4])(material);
    const int baseVertex  = reinterpret_cast<IntSlotFn>(slots[0x18 / 4])(material);
    const int type        = reinterpret_cast<IntSlotFn>(slots[0x14 / 4])(material);
    m_device->DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(type), baseVertex,
                                   static_cast<UINT>(minIndex), static_cast<UINT>(numVertices),
                                   static_cast<UINT>(startIndex),
                                   static_cast<UINT>(primitiveCount));   // device +0x148
}

// sub_1004AFD0, read in full. GetParameterByName(0, "ShaderVersion") is D3DX slot +0x24 and GetInt
// slot +0x6C.
int* RaShader::shaderVersion()
{
    if (!m_effect)
    {
        slot04();
        if (!m_effect)
        {
            m_byte134 = 1;
            m_dword138 = 0;
            return reinterpret_cast<int*>(&m_dword138);
        }
    }
    if (m_byte134)
    {
        const D3DXHANDLE handle =
            static_cast<ID3DXEffect*>(m_effect)->GetParameterByName(0, "ShaderVersion");
        int value = 0;
        int read = 0;
        if (handle)
        {
            if (static_cast<ID3DXEffect*>(m_effect)->GetInt(handle, &read) >= 0)
                value = read;
            else
                value = 0;
        }
        m_dword138 = static_cast<unsigned int>(value);
        m_byte134 = 0;
    }
    return reinterpret_cast<int*>(&m_dword138);
}

// off_101E2958 slot +0x04.  The shipped lookups reach the body only through the vtable, so the
// dispatch is kept even though the body is now written.
bool RaShader::slot04()
{
    void** const vtbl = *reinterpret_cast<void***>(this);
    return reinterpret_cast<unsigned char (__thiscall*)(void*)>(vtbl[1])(this) != 0;
}


// Loads "shaders/rashader" + lowercase name + ".fx"; the prefix has no trailing slash.
// Retains initialization of the unused mod-prefix static.

void* RaShader::loadEffect()
{
    // unk_1023F3B4 / unk_1023F398 / unk_1023F37C.  The shipped guards are three bits of
    // dword_1023F3D0 plus an atexit() per string; a function-local static is the same lazy
    // construct-once with the same lifetime, and the destructor registration differs only in which
    // mechanism runs it.
    static const std::string s_prefix("shaders/rashader");
    static const std::string s_modPrefix("mods/bf2/");      // constructed, never read
    static const std::string s_extension(".fx");
    (void)s_modPrefix;

    // `s_prefix + m_name + s_extension`.  MSVC expanded that expression into four extra
    // copy-constructed temporaries in the shipped body; the bytes it produces are these.
    std::string path(s_prefix);
    path += m_name;
    path += s_extension;

    return effectManagerGetEffect(path);                     // sub_10035900 on dword_1023D954
}

// PE 0x1004B4E0, slot +0x04: load the effect, choose a technique, measure its shader
// model with an empty pass, then rebuild vertex elements and parameter bindings.
// OnResetDevice runs before technique validation; retail calls GetPass, not SetTechnique.
// Keep the unchecked 16 KiB shader buffer, name-as-D3DXHANDLE lookup and all five
// registry probes: later hits overwrite earlier ones; unknown names keep block/offset 0.
bool RaShader::build()
{
    // -- 1. the effect
    ID3DXEffect* const effect = static_cast<ID3DXEffect*>(loadEffect());
    m_effect      = effect;                                              // +0x16C
    m_buildSerial = -1;                                                  // +0x174
    if (!effect)
        return false;

    m_byte134 = 1;                                                       // +0x134

    // -- 2. the technique
    m_technique = ::renddx9abi::d3dxHandleValue(effect->GetTechniqueByName("defaultTechnique"));
    if (!m_technique)
    {
        D3DXHANDLE next = ::renddx9abi::d3dxHandle(m_technique);
        effect->FindNextValidTechnique(0, &next);
        m_technique = ::renddx9abi::d3dxHandleValue(next);
    }

    effect->OnResetDevice();

    if (!m_technique)
        return false;

    // +0x38 - GetPass(technique, 0).  The answer is discarded; see the note above.
    effect->GetPass(::renddx9abi::d3dxHandle(m_technique), 0);

    // -- 3. the bound pixel-shader model
    stateManagerPush();
    {
        unsigned int passes = 0;
        effect->Begin(&passes, 7);
        effect->BeginPass(0);
        effect->EndPass();
        effect->End();
    }

    int shaderModel = 0;
    if (IDirect3DPixelShader9* const pixelShader =
            static_cast<IDirect3DPixelShader9*>(stateManagerCurrentPixelShader()))
    {
        unsigned long function[16384 / sizeof(unsigned long)];
        UINT          size = 0;
        pixelShader->GetFunction(0, &size);
        pixelShader->GetFunction(function, &size);
        shaderModel = static_cast<int>((::D3DXGetShaderVersion(function) >> 8) & 0xFFu);
    }
    stateManagerPop();

    // Elements are borrowed pointers, so emptying the vector releases nothing else.
    m_requiredVertexElements.clear();

    const D3DXHANDLE required = effect->GetParameterByName(0, "reqVertexElement");
    if (required)
    {
        D3DXPARAMETER_DESC requiredDesc;
        effect->GetParameterDesc(required, &requiredDesc);

        for (unsigned int i = 0; i < requiredDesc.Elements; ++i)
        {
            const D3DXHANDLE element = effect->GetParameterElement(required, i);
            const char* value = 0;
            effect->GetString(element, &value);

            int index = -1;
            {
                const std::string wanted(value);
                index = raFindString(m_vertexElementNames, wanted);
            }
            if (index != -1)
            {
                raPushDescriptor(m_requiredVertexElements, m_vertexElementDescriptors[index]);
            }
        }
    }

    // -- 5a. drop the previous binding, all fifteen vectors (each element's name string with it)
    for (int slot = 0; slot < 5; ++slot)
    {
        m_instanceParameters[slot].clear();
        m_globalParameters[slot].clear();
        m_templateParameters[slot].clear();
    }

    // -- 5b. re-bind
    // The shipped body builds a local vector<std::string> of the three struct names and walks it;
    // the order it pushes them in is Instance, Template, Global, and that is the order the effect is
    // interrogated in.
    std::vector<std::string> blockNames;
    { const std::string n("InstanceParameters"); raPushName(blockNames, n); }
    { const std::string n("TemplateParameters"); raPushName(blockNames, n); }
    { const std::string n("GlobalParameters");   raPushName(blockNames, n); }

    for (std::vector<std::string>::const_iterator cursor = blockNames.begin();
         cursor != blockNames.end(); ++cursor)
    {
        const std::string& blockName = *cursor;

        const D3DXHANDLE block = effect->GetParameterByName(0, blockName.c_str());
        if (!block)
            continue;

        // case-insensitive, AND NOT else-if.  Three independent _stricmp tests against the same
        // three literals the vector holds, so the target can only be one of the three - but nothing
        // guards the case where it is none, and the shipped body would then index off address zero.
        // The missing guard is preserved.
        std::vector<RaBoundParameter>* scope = 0;
        if (!::_stricmp(blockName.c_str(), "InstanceParameters")) scope = m_instanceParameters;
        if (!::_stricmp(blockName.c_str(), "TemplateParameters")) scope = m_templateParameters;
        if (!::_stricmp(blockName.c_str(), "GlobalParameters"))   scope = m_globalParameters;

        D3DXPARAMETER_DESC blockDesc;
        effect->GetParameterDesc(block, &blockDesc);

        for (unsigned int member = 0; member < blockDesc.Elements; ++member)
        {
            const D3DXHANDLE element = effect->GetParameterElement(block, member);
            const char* memberName = 0;
            effect->GetString(element, &memberName);

            // Initialize only the handle, descriptor's first word, and offset.
            RaBoundParameter record;                    // record.name default-constructs to ""
            unsigned int     blockIndex = 0;
            record.handle    = 0;
            record.desc.Name = 0;
            record.offset    = 0;

            record.handle =
                ::renddx9abi::d3dxHandleValue(effect->GetParameterByName(0, memberName));
            if (!record.handle)
            {
                // RaShader.cpp:410, severity 6, empty channel.  The shipped diagnostic goes through
                // the unread debug-stream family (sub_10074300 / sub_10074690 and the ~4 KB of
                // std::ostream machinery behind them); this image renders every recovered diagnostic
                // through its own log instead - see Texture.cpp:484 and RaShaderPermutator.cpp:81
                // for the precedent.  The text and the operand order are the shipped ones.
                ::renddx9abi::logf(
                    "RaShader.cpp:410: Required .fx parameter %s in %s didn't match any parameter "
                    "declared in the effect: %s",
                    memberName ? memberName : "(null)", blockName.c_str(), m_name.c_str());
            }
            else
            {
                record.name = memberName;

                // THE name IS passed AS THE handle.  See the note above the body.
                effect->GetParameterDesc(memberName, &record.desc);

                // The five probes, in the shipped order.  The index each one assigns is its block's
                // slot inside the scope array; see RaShader.h.
                raProbeRegistry(raMaterialParameterNames(),         0, memberName, shaderModel,
                                record, blockIndex);   // unk_1023E7B0
                raProbeRegistry(g_raLightingNames,                  3, memberName, shaderModel,
                                record, blockIndex);   // unk_1023E790
                raProbeRegistry(g_raObjectNames,                    1, memberName, shaderModel,
                                record, blockIndex);   // unk_1023E7D0
                raProbeRegistry(raShaderConstantsParameterNames(),  2, memberName, shaderModel,
                                record, blockIndex);   // unk_1023DDC0
                raProbeRegistry(g_raLightArrayNames,                4, memberName, shaderModel,
                                record, blockIndex);   // unk_1023E770

                // `ecx = scope + blockIndex * 16` - written as the byte arithmetic the image does,
                // so that a null `scope` faults the way the shipped body would instead of being
                // folded away as undefined array indexing.
                std::vector<RaBoundParameter>* const target =
                    reinterpret_cast<std::vector<RaBoundParameter>*>(
                        reinterpret_cast<char*>(scope)
                        + blockIndex * sizeof(std::vector<RaBoundParameter>));
                target->push_back(record);
            }
        }
    }

    // Publish the built flag and serial before the local vector is destroyed on return.
    m_built       = 1;                                                   // +0x170
    m_buildSerial = static_cast<int>(g_raShaderBuildSerial++);           // dword_1023F078

    return true;
}


// sub_10041C70, find-or-create.

RaShader* findOrCreateShader(RaShaderTable* table, const std::string& name, bool skipFileCheck)
{
    std::string lowered;
    raToLower(lowered, name);

    // sub_101C0AD0 is lower_bound on the map at +0x88; the caller then applies the second half of
    // std::map::find by hand - "not end, and not (key < node->key)".  std::map::find is that pair.
    const std::map<std::string, RaShader*>::const_iterator found = table->shaders.find(lowered);
    if (found != table->shaders.end())
        return found->second;

    if (!skipFileCheck)
    {
        // The name supplies the separator after "shaders/rashader".
        std::string path("shaders/rashader");
        path += lowered;
        path += ".fx";
        if (!fileManagerResolves(path))
            return 0;
    }

    // One 0x2B0 render queue per shader, named after it (sub_1005F320(0x2B0) + sub_1003B330), then
    // its +0x08 draw mode forced to 1.

    // Queue creation is not null-checked before setting its flag.
    void* const queue = renderQueueCreateNamed(lowered);
    renderQueueSetDrawMode(queue, 1u);

    // sub_10075110 against "pointlight", then push_back into ONE of the two collections.
    {
        const std::string needle("pointlight");
        std::vector<void*>& collection = raContainsInsensitive(lowered, needle)
            ? table->pointLightQueues      // +0xA4
            : table->worldQueues;          // +0x94
        collection.push_back(queue);
    }

    void* const storage = ::renddx9abi::engineAlloc(sizeof(RaShader));   // sub_1005F320(0x178)
    RaShader* shader = 0;
    if (storage)
        shader = new (storage) RaShader(lowered, queue);

    // sub_10043600 - map::insert(value_type(lowered, shader)).  The shipped body builds the pair
    // through three copy-constructed temporaries; the observable result is one node keyed by the
    // lowered name whose value is the shader.  The find above guarantees the key is new.
    table->shaders.insert(std::make_pair(lowered, shader));

    // Creation tests the renderer base's [UNRESOLVED] flag at +0x102;
    // lookup of an existing shader instead tests that shader's built flag.
    if (!readBoolSetting("GSDeferredShaderPermutation", false))
    {
        const RendDX9Base* const base = static_cast<const RendDX9Base*>(g_rendererBase);
        if (base && base->flag102 == 0)                          // +0x102
        {
            if (shader)
                shader->slot04();
        }
    }

    return shader;
}


// sub_10041F20, the outer entry.

RaShader* getShader(RaShaderTable* table, const std::string& name, bool skipFileCheck)
{
    std::string lowered;
    raToLower(lowered, name);

    // Empty names and failed creation return the default shader.
    RaShader* result = table->defaultShader;                    // +0x84

    if (!lowered.empty())                                       // std::operator!=(lowered, "")
    {
        // sub_10036410 - the map's own find on the 0x88 map, compared against the +0x8C end.
        const std::map<std::string, RaShader*>::const_iterator node =
            table->shaders.find(lowered);
        if (node == table->shaders.end())
        {
            // Preserve the repeated lookup after the successful creation test.
            if (findOrCreateShader(table, lowered, skipFileCheck))
                result = findOrCreateShader(table, lowered, skipFileCheck);
        }
        else
        {
            result = node->second;

            // The found arm's gate: the setting, then the shader's own byte at +0x170.
            if (!readBoolSetting("GSDeferredShaderPermutation", false))
            {
                if (result && result->m_built == 0)
                    result->slot04();
            }
        }
    }

    return result;
}

} // namespace Ra
} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
