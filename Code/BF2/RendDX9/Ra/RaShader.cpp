// Path: attested by RendDX9.dll __FILE__ at 0x101E2740 (shader build, line 410).
// Ra shader lookup, construction, parameter registration and binding, recovered
// from RendDX9.dll 1.50 (PE only). Each shader owns a queue in the shared Ra table.
// Lookup preserves the default fallback and repeated find-or-create call.

#ifdef BF2_IMAGE_RENDDX9

#include "RaShader.h"
#include "../Texture.h"                    // EngineTexture - the engine-texture holder apply

#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/PluginAbi.h"
#include "../../../RendDX9Abi/PluginEntry.h"

#include <new>
#include <stdarg.h>
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

void __fastcall raHolderFrontier(void* self, void*)
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

void __fastcall raLightingOnDeviceLost(void* self, void*)
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

void __fastcall raLightingListenerNullsub(void*, void*)
{
    // 0x10037D40 - `ret`, empty in the original too.
}

// Destroys lighting parameters and frees storage when flags bit zero is set.
void* __fastcall raLightingScalarDeletingDtor(void* self, void*, unsigned int flags)
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

// sub_10046780 / sub_100EAF70 / the inlined body of sub_10046AC0 are all MSVC's vector `_Buy(n)`:
// clear the three pointers, and allocate only when n is non-zero.  Every call this file reproduces
// passes a source whose _First is null, i.e. n == 0, so the whole copy-construct collapses to the
// clear.  Recorded as a function so the shape is visible rather than implied.
void raVectorBuyEmpty(vc71::vector_layout& v)
{
    v.first = 0;
    v.last  = 0;
    v.end   = 0;
}


// Four {offsets, names} vector pairs on a 0x20 stride; this file owns three of them (the fourth,
// 0x1023E7B0/0x1023E7C0, belongs to the material block in Geom/StaticMeshMaterial.cpp and is NOT
// redefined here).  The guard every tail uses is "the OFFSET vector is still empty"; the names
// vector is never consulted.

vc71::vector_layout g_raLightArrayOffsets = { 0, 0, 0, 0 };   // unk_1023E770
vc71::vector_layout g_raLightArrayNames   = { 0, 0, 0, 0 };   // dword_1023E780
vc71::vector_layout g_raLightingOffsets   = { 0, 0, 0, 0 };   // unk_1023E790
vc71::vector_layout g_raLightingNames     = { 0, 0, 0, 0 };   // dword_1023E7A0
vc71::vector_layout g_raObjectOffsets     = { 0, 0, 0, 0 };   // unk_1023E7D0
vc71::vector_layout g_raObjectNames       = { 0, 0, 0, 0 };   // dword_1023E7E0

// sub_1003A450 / sub_1004CD70 / sub_10011160 - the three `_Insert_n(where, 1, value)` grow paths in
// this family, all the same VC7.1 shape: half again, clamped up to what was asked for, engine heap.
// Written once over a byte stride instead of three times over three element types.  This is the
// same helper Geom/StaticMeshMaterial.cpp carries as raRegistryPush; that copy is file-local there.
typedef void (*RaCopyFn)(void* destination, const void* source);

void raRegistryPush(vc71::vector_layout& v, const void* element, unsigned int stride,
                    RaCopyFn copyConstruct)
{
    const unsigned int size  = v.first ? static_cast<unsigned int>((v.last - v.first) / stride) : 0u;
    const unsigned int capac = v.first ? static_cast<unsigned int>((v.end  - v.first) / stride) : 0u;
    if (!v.first || size >= capac)
    {
        unsigned int grown = capac + (capac >> 1);
        if (grown < size + 1u)
            grown = size + 1u;
        char* const buffer = static_cast<char*>(::renddx9abi::engineAlloc(grown * stride));
        if (!buffer)
            return;
        if (size)
            ::memcpy(buffer, v.first, size * stride);
        if (v.first)
            ::renddx9abi::engineFree(v.first);
        v.first = buffer;
        v.last  = buffer + size * stride;
        v.end   = buffer + grown * stride;
    }
    copyConstruct(v.last, element);
    v.last += stride;
}

void raCopyString(void* destination, const void* source)
{
    new (destination) vc71::string(*static_cast<const vc71::string*>(source));
}

void raCopyPointer(void* destination, const void* source)
{
    *static_cast<const void**>(destination) = *static_cast<const void* const*>(source);
}

void raCopyInt(void* destination, const void* source)
{
    *static_cast<int*>(destination) = *static_cast<const int*>(source);
}

struct RaParameterName { const char* name; int offset; };

// Register while offsets is empty, including when it has reserved but unused capacity.
void raRegisterNames(vc71::vector_layout& offsets, vc71::vector_layout& names,
                     const RaParameterName* table, unsigned int count)
{
    if (offsets.first &&
        static_cast<unsigned int>((offsets.last - offsets.first) >> 2) != 0u)
        return;

    for (unsigned int i = 0; i < count; ++i)
    {
        // Each iteration builds the std::string, pushes it, pushes the offset, and destroys the
        // string - in that order, which is why the temporary is scoped this tightly.
        vc71::string name(table[i].name);
        raRegistryPush(names, &name, sizeof(vc71::string), &raCopyString);
        raRegistryPush(offsets, &table[i].offset, 4, &raCopyInt);
    }
}


// Registry of shader-visible lighting-block parameter offsets.

const RaParameterName kRaLightingNames[11] =
{
    { "TerrainLightMap",      28 }, { "LightMap",             36 },
    { "LightMapOffset",       44 }, { "MatBones",             64 },
    { "OverGrowthAmbient",    76 }, { "ShadowViewProj",       96 },
    { "ShadowViewProjArray", 164 }, { "ClampViewProjArray",  184 },
    { "ShadowPosArray",      204 }, { "ShadowSizeArray",     224 },
    { "ShadowCount",         244 }
};


// Registry of object transform, wind, and hemi-light parameter offsets.

const RaParameterName kRaObjectNames[15] =
{
    { "World",                 8 }, { "ViewProjection",       76 },
    { "WorldViewProjection", 144 }, { "ShadowProjMat",       212 },
    { "ShadowOccProjMat",    280 }, { "ShadowTrapMat",       348 },
    { "GeomBones",           416 }, { "Transparency",        428 },
    { "ObjectSpaceCamPos",   436 }, { "WindSpeed",           456 },
    { "WindDir",             464 }, { "UserData",            480 },
    { "HeightOverTerrain",   500 }, { "HemiMapSkyColor",     508 },
    { "ObjectSpaceWorldUp",  528 }
};


// Registry for the light-array parameter block and its lazy resolver.

const RaParameterName kRaLightArrayNames[2] =
{
    { "Lights", 8 }, { "Lights_1X", 16 }
};


} // anonymous namespace


// Parameter-holder slot 0 applies a value to an effect handle and returns its HRESULT.
// Fastcall adapters preserve the thiscall table ABI. Remaining destructor slots keep
// their named stops until implemented; the current call paths do not dispatch them.

namespace {

inline unsigned char* holderPayload(void* self)
{
    return static_cast<unsigned char*>(self) + 4;
}

// off_101E1480 slot 0 - sub_10037E20: SetVector(handle, &value).
int __fastcall raApplyVec4(void* self, void*, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetVector(
        ::renddx9abi::d3dxHandle(handle),
        reinterpret_cast<const D3DXVECTOR4*>(holderPayload(self))));
}

// off_101E1488 slot 0 - sub_10037E40: SetTexture(handle, value) - the value IS the D3D base texture.
int __fastcall raApplyTexture(void* self, void*, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetTexture(
        ::renddx9abi::d3dxHandle(handle),
        *reinterpret_cast<IDirect3DBaseTexture9**>(holderPayload(self))));
}

// off_101E148C slot 0 - sub_1003A170: SetFloat(handle, value).
int __fastcall raApplyFloat(void* self, void*, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetFloat(
        ::renddx9abi::d3dxHandle(handle), *reinterpret_cast<const float*>(holderPayload(self))));
}

// off_101E1494 slot 0 - sub_1003A1A0: SetFloatArray(handle, value, 3).
int __fastcall raApplyVec3(void* self, void*, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetFloatArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const float*>(holderPayload(self)), 3u));
}

// off_101E149C slot 0 - sub_1003A1C0: SetFloatArray(handle, value, 2).  This is the table the
// material block installs at its +0x140 ("NormalUnpack"), so that holder is a TWO-float one.
int __fastcall raApplyVec2(void* self, void*, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetFloatArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const float*>(holderPayload(self)), 2u));
}

// off_101E14A8 slot 0 - sub_1003A1E0: SetInt(handle, value).
int __fastcall raApplyInt(void* self, void*, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetInt(
        ::renddx9abi::d3dxHandle(handle), *reinterpret_cast<const int*>(holderPayload(self))));
}

// off_101E14BC slot 0 - sub_1003A140: nothing (and 0) for a null texture, else SetTexture(handle,
// sub_10014930(texture)) - the engine texture's D3D base object.
int __fastcall raApplyEngineTexture(void* self, void*, void* effect, void* handle)
{
    EngineTexture* const texture = *reinterpret_cast<EngineTexture**>(holderPayload(self));
    if (!texture)
        return 0;
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetTexture(
        ::renddx9abi::d3dxHandle(handle), texture->baseTexture));
}

// off_101E1E78 slot 0 - sub_10046580: SetBool(handle, byte).
int __fastcall raApplyBool(void* self, void*, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetBool(
        ::renddx9abi::d3dxHandle(handle), static_cast<BOOL>(*holderPayload(self))));
}

// off_101E1E80 slot 0 - sub_100465A0 -> sub_100465C0: SetVector(handle, {f, f, f, f}).
int __fastcall raApplyFloatSplat(void* self, void*, void* effect, void* handle)
{
    const float f = *reinterpret_cast<const float*>(holderPayload(self));
    const float splat[4] = { f, f, f, f };
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetVector(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const D3DXVECTOR4*>(splat)));
}

// off_101E1E88 slot 0 - sub_10046450.  The holder is {vptr, lightsHolder, resolver}: the resolver
// (raLightArrayUpdate) runs with ecx = &lightsHolder->value and answers the record handed to
// SetValue(handle, record, 68).
int __fastcall raApplyLightRecordResolved(void* self, void*, void* effect, void* handle)
{
    typedef void* (__fastcall* ResolveFn)(void*, void*);
    void** const words = static_cast<void**>(self);
    void* const record = reinterpret_cast<ResolveFn>(words[2])(
        static_cast<unsigned char*>(words[1]) + 4, 0);
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetValue(
        ::renddx9abi::d3dxHandle(handle), record, 68u));
}

// off_101E1E8C slot 0 - sub_100465F0: SetMatrixPointerArray(handle, pointers, count).
int __fastcall raApplyMatrixPointerArray(void* self, void*, void* effect, void* handle)
{
    void** const words = static_cast<void**>(self);
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetMatrixPointerArray(
        ::renddx9abi::d3dxHandle(handle), static_cast<const D3DXMATRIX**>(words[1]),
        static_cast<unsigned int>(reinterpret_cast<size_t>(words[2]))));
}

// off_101E1E94 slot 0 - sub_10046620: SetMatrix(handle, &value).
int __fastcall raApplyMat4(void* self, void*, void* effect, void* handle)
{
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetMatrix(
        ::renddx9abi::d3dxHandle(handle),
        reinterpret_cast<const D3DXMATRIX*>(holderPayload(self))));
}

// off_101E1E9C slot 0 - sub_100469F0 -> sub_10046A10: SetValue(handle, bytes, count), and
// SetValue(handle, 0, 0) for an empty vector - the call is made either way.
int __fastcall raApplyBytes(void* self, void*, void* effect, void* handle)
{
    const vc71::vector_layout& v =
        *reinterpret_cast<const vc71::vector_layout*>(holderPayload(self));
    if (v.first)
        return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetValue(
            ::renddx9abi::d3dxHandle(handle), v.first,
            static_cast<unsigned int>(v.last - v.first)));
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetValue(
        ::renddx9abi::d3dxHandle(handle), 0, 0u));
}

// off_101E1EA4 slot 0 - sub_10046BC0: SetMatrixArray(handle, first, bytes / 64) (sub_10046410).
int __fastcall raApplyMat4Array(void* self, void*, void* effect, void* handle)
{
    const vc71::vector_layout& v =
        *reinterpret_cast<const vc71::vector_layout*>(holderPayload(self));
    const unsigned int count = v.first ? static_cast<unsigned int>(v.last - v.first) >> 6 : 0u;
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetMatrixArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const D3DXMATRIX*>(v.first), count));
}

// off_101E1EAC slot 0 - sub_10046BF0: SetVectorArray(handle, first, bytes / 16) (sub_10046430).
int __fastcall raApplyVec4Array(void* self, void*, void* effect, void* handle)
{
    const vc71::vector_layout& v =
        *reinterpret_cast<const vc71::vector_layout*>(holderPayload(self));
    const unsigned int count = v.first ? static_cast<unsigned int>(v.last - v.first) >> 4 : 0u;
    return static_cast<int>(static_cast<ID3DXEffect*>(effect)->SetVectorArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const D3DXVECTOR4*>(v.first), count));
}

// off_101DD3FC slot 0 - sub_1000BE00: SetValue(handle, pointer, 68) - the pointed-at RaLightRecord.
int __fastcall raApplyLightRecordPointer(void* self, void*, void* effect, void* handle)
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
void* __fastcall raLightArrayUpdate(void* lightsValue, void*)
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

// sub_10011220 (PE 0x10011220) - push_back on the 28-byte name vector.  The `0x92492493` multiply
// in the shipped body is the compiler's division by 28, i.e. sizeof(std::string).
void raPushName(vc71::vector_layout& v, const vc71::string& name)
{
    raRegistryPush(v, &name, sizeof(vc71::string), &raCopyString);
}

// sub_1004D3E0 (PE 0x1004D3E0) and its seventeen inlined copies - push_back of one dword.
void raPushDescriptor(vc71::vector_layout& v, const RaVertexElementDesc* desc)
{
    const void* value = desc;
    raRegistryPush(v, &value, 4, &raCopyPointer);
}


// [FRONTIER] Primary shader slots other than build retain diagnostic abort thunks.
// Preserve the seven-slot table and its trailing null entry.

#define RD9_RASHADER_FRONTIER(fn, slotOffset, addr)                                   \
    void __fastcall fn(void* self, void*)                                             \
    {                                                                                 \
        RD9_FRONTIER_AT("dice::hfe::dx9rend::Ra::RaShader::slot_" #slotOffset          \
                        " (" #addr ")", self, slotOffset);                            \
    }

RD9_RASHADER_FRONTIER(ras_slot00, 0x00, 0x1004DB40)

// +0x08 .. +0x18 - ; thin __fastcall adapters over the typed members below, as
// slot +0x04's is.  +0x08 and +0x10 .. +0x18 take the entry on the stack (`retn 4`); +0x0C takes
// nothing.
unsigned int __fastcall ras_slot08(void* self, void*, RenderQueueEntry* entry)
{
    return static_cast<RaShader*>(self)->begin(entry);
}
void __fastcall ras_slot0C(void* self, void*)
{
    static_cast<RaShader*>(self)->end();
}
void __fastcall ras_slot10(void* self, void*, RenderQueueEntry* entry)
{
    static_cast<RaShader*>(self)->applyInstanceParameters(entry);
}
void __fastcall ras_slot14(void* self, void*, RenderQueueEntry* entry)
{
    static_cast<RaShader*>(self)->applyTemplateParameters(entry);
}
void __fastcall ras_slot18(void* self, void*, RenderQueueEntry* entry)
{
    static_cast<RaShader*>(self)->draw(entry);
}

#undef RD9_RASHADER_FRONTIER

// +0x04, sub_1004B4E0.  `__thiscall`, no stack arguments (the body's terminator is a bare `retn`),
// the result in al.  Written as the table's other entries are: a thin __fastcall adapter over the
// typed member, so nothing in the table is a raw seam with an invented signature.
unsigned char __fastcall ras_slot04(void* self, void*)
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
#if !BF2_PLUGIN_ABI_NATIVE
struct RaShaderPeFileInfo
{
    vc71::string  name;          // +0x00
    unsigned char isDirectory;   // +0x1C
    unsigned char pad1[3];
    unsigned int  size;          // +0x20
    unsigned int  pad2;          // +0x24  MSVC 8-aligns the filetime block that follows
    unsigned int  times[6];      // +0x28  creation / last access / last write, low dword first
};

static_assert(sizeof(RaShaderPeFileInfo) == 64, "the PE io::FileInfo the shipped frame reserves");
#endif

bool fileManagerResolves(const vc71::string& path)
{
    // [CHOSEN, not recovered] Permit loading before the host publishes FileManager.
    if (!::dice::hfe::fileManager)
        return false;

    const ::renddx9abi::HostString hostPath = ::renddx9abi::makeHostString(path);

#if BF2_PLUGIN_ABI_NATIVE
    ::dice::hfe::io::FileInfo info;
    return ::dice::hfe::fileManager->getFileInfo(hostPath, info);
#else
    RaShaderPeFileInfo info;
    void** const vtbl = *reinterpret_cast<void***>(::dice::hfe::fileManager);
    typedef unsigned char (__thiscall* GetFileInfoFn)(void*, const ::renddx9abi::HostString&,
                                                      RaShaderPeFileInfo&);
    return reinterpret_cast<GetFileInfoFn>(vtbl[0x38 / 4])(
               ::dice::hfe::fileManager, hostPath, info) != 0;
#endif
}

// sub_10074E80, PE 0x10074E80 - `std::string lowered(in); pathToLower(lowered); return lowered;`.
// pathToLower is sub_10074D10, which RendDX9Base.h already publishes.
void raToLower(vc71::string& out, const vc71::string& in)
{
    out = in.c_str();
    pathToLower(out);
}

// sub_10075110, case-insensitive find, reproduced here rather than reached because
// Geom/StaticMeshTemplate.cpp's copy of it is file-local to that translation unit.  Both lower a
// copy of each string with pathToLower and run std::string::find on the lowered pair.
bool raContainsInsensitive(const vc71::string& haystack, const vc71::string& needle)
{
    vc71::string loweredHaystack(haystack.c_str());
    vc71::string loweredNeedle(needle.c_str());
    pathToLower(loweredHaystack);
    pathToLower(loweredNeedle);
    return ::strstr(loweredHaystack.c_str(), loweredNeedle.c_str()) != 0;
}


// Shared formatting buffer; overflow replaces the message with a fixed warning.
char g_raFormatBuffer[0x1000];

void raFormatString(vc71::string& out, const char* format, ...)
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

// std::operator==<char>(const string&, const string&) - size first, then a byte compare.  Written
// out because vc71::string is an ABI carrier and deliberately carries no relational operators.
bool raStringEqual(const vc71::string& a, const vc71::string& b)
{
    return a.size() == b.size() && ::memcmp(a.c_str(), b.c_str(), a.size()) == 0;
}

// sub_1004C990, std::find over a vc71::string array.  Returns the element index or
// -1.  The shipped body returns the iterator through a hidden ecx return slot and every caller then
// does the same `(found - _First) / 28` division; folding that in here changes nothing observable.
int raFindString(const vc71::vector_layout& names, const vc71::string& wanted)
{
    const char* it = names.first;
    for (; it != names.last; it += sizeof(vc71::string))
        if (raStringEqual(*reinterpret_cast<const vc71::string*>(it), wanted))
            break;
    if (it == names.last)
        return -1;
    return static_cast<int>((it - names.first) / static_cast<int>(sizeof(vc71::string)));
}

// sub_1004CA60, the plain-name lookup, `retn 4`.
int raRegistryIndexOfPlain(const vc71::vector_layout& names, const vc71::string& parameter)
{
    return raFindString(names, parameter);
}

// Returns the name-registry index, trying the shader-model suffix before the plain name.
// The result is an index into the parallel offset table, not a byte offset.
int raRegistryIndexOf(const vc71::vector_layout& names, const vc71::string& parameter,
                      int shaderModel)
{
    int index = -1;
    {
        vc71::string versioned;
        raFormatString(versioned, "%s_%iX", parameter.c_str(), shaderModel);
        index = raFindString(names, versioned);
    }
    if (index < 0)
        return raRegistryIndexOfPlain(names, parameter);
    return index;
}

// Copies a bound parameter, including its owned string, for compatibility-vector insertion.
void raCopyBoundParameter(void* destination, const void* source)
{
    const RaBoundParameter* const s = static_cast<const RaBoundParameter*>(source);
    unsigned char* const          d = static_cast<unsigned char*>(destination);
    ::memcpy(d, s, offsetof(RaBoundParameter, name));      // handle + desc + offset, all POD
    new (d + offsetof(RaBoundParameter, name)) vc71::string(s->name);
}

void raPushBoundParameter(vc71::vector_layout& v, const RaBoundParameter& record)
{
    raRegistryPush(v, &record, sizeof(RaBoundParameter), &raCopyBoundParameter);
}

// The clear half of the same loop: destroy every element's std::string at +0x34, free the buffer,
// null the triple.  sub_1004B4E0 runs it over all fifteen vectors before it re-binds anything.
void raClearBoundParameters(vc71::vector_layout& v)
{
    if (v.first)
    {
        for (char* p = v.first; p != v.last; p += sizeof(RaBoundParameter))
            reinterpret_cast<RaBoundParameter*>(p)->name.~string();
        ::renddx9abi::engineFree(v.first);
    }
    v.first = 0;
    v.last  = 0;
    v.end   = 0;
}

// One registry probe, written the way the shipped body writes it: build a temporary std::string,
// ask, destroy the temporary; and on a hit build a second temporary and ask again for the value.
// Two calls and two temporaries per hit - folded into one lookup it would still answer the same,
// but the doubling is what the instruction stream does.
void raProbeRegistry(const vc71::vector_layout& names, unsigned int blockIndex,
                     const char* parameter, int shaderModel,
                     RaBoundParameter& record, unsigned int& block)
{
    {
        const vc71::string probe(parameter);
        if (raRegistryIndexOf(names, probe, shaderModel) == -1)
            return;
    }
    block = blockIndex;
    {
        const vc71::string fetch(parameter);
        record.offset = raRegistryIndexOf(names, fetch, shaderModel);
    }
    record.name = parameter;
}

// Monotonically assigned shader-build serial.
unsigned int g_raShaderBuildSerial = 0;

} // anonymous namespace


// sub_10045240, PE 0x10045240.

void raLightingParametersConstruct(void* storage)
{
    RaLightingParameters* const p = static_cast<RaLightingParameters*>(storage);

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

    // The four array holders.  Each shipped call copy-constructs the member vector from an empty
    // stack vector ({_First, _Last, _End} all zero), so `_Buy(0)` clears the three pointers and
    // returns false and nothing is copied.  The 64/16-byte strides are the `sar 6` / `sar 4` in
    // sub_10046CC0 and sub_10046D60 and are recorded only in this comment - no element is moved.
    p->shadowViewProjArray.vptr = raHolderMat4Array();    // off_101E1EA4, stride 64
    raVectorBuyEmpty(p->shadowViewProjArray.value);
    p->clampViewProjArray.vptr  = raHolderMat4Array();    // off_101E1EA4, stride 64
    raVectorBuyEmpty(p->clampViewProjArray.value);
    p->shadowPosArray.vptr      = raHolderVec4Array();    // off_101E1EAC, stride 16
    raVectorBuyEmpty(p->shadowPosArray.value);
    p->shadowSizeArray.vptr     = raHolderVec4Array();    // off_101E1EAC, stride 16
    raVectorBuyEmpty(p->shadowSizeArray.value);

    p->shadowCount.vptr  = raHolderInt();
    p->shadowCount.value = 0;

    p->tail0FC.first = 0;
    p->tail0FC.last  = 0;
    p->tail0FC.end   = 0;
    p->tail10C       = 0;

    raRegisterNames(g_raLightingOffsets, g_raLightingNames, kRaLightingNames, 11);

    // The last instruction of the shipped body.  Priority 3, the same one RaShaderConstants and
    // BufferManager register with.  The matching detach is the first thing sub_10044120 does, so a
    // stack-lifetime block leaves the list exactly as it found it.
    registerDeviceLostResetListener(static_cast<void*>(p), 3);
}


// sub_10044120, PE 0x10044120.  An ordinary VC7.1 destructor: detach, free the owned buffers,
// republish each member's vptr on the way out, and release the two reference holders through their
// own vtable slot +0x08.

void raLightingParametersDestroy(void* storage)
{
    RaLightingParameters* const p = static_cast<RaLightingParameters*>(storage);

    p->vptr = raLightingParametersVtable();             // off_101E1EB4, republished first
    unregisterDeviceLostResetListener(static_cast<void*>(p));

    if (p->tail0FC.first)
        ::renddx9abi::engineFree(p->tail0FC.first);
    p->tail0FC.first = 0;
    p->tail0FC.last  = 0;
    p->tail0FC.end   = 0;

    p->shadowCount.vptr = raHolderInt();

    p->shadowSizeArray.vptr = raHolderVec4Array();
    if (p->shadowSizeArray.value.first)
        ::renddx9abi::engineFree(p->shadowSizeArray.value.first);
    raVectorBuyEmpty(p->shadowSizeArray.value);

    p->shadowPosArray.vptr = raHolderVec4Array();
    if (p->shadowPosArray.value.first)
        ::renddx9abi::engineFree(p->shadowPosArray.value.first);
    raVectorBuyEmpty(p->shadowPosArray.value);

    p->clampViewProjArray.vptr = raHolderMat4Array();
    if (p->clampViewProjArray.value.first)
        ::renddx9abi::engineFree(p->clampViewProjArray.value.first);
    raVectorBuyEmpty(p->clampViewProjArray.value);

    p->shadowViewProjArray.vptr = raHolderMat4Array();
    if (p->shadowViewProjArray.value.first)
        ::renddx9abi::engineFree(p->shadowViewProjArray.value.first);
    raVectorBuyEmpty(p->shadowViewProjArray.value);

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


// sub_100458E0, PE 0x100458E0.

void raObjectParametersConstruct(void* storage)
{
    RaObjectParameters* const p = static_cast<RaObjectParameters*>(storage);
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

    p->userData.vptr = raHolderBytes();               // off_101E1E9C - a byte vector, no element stride
    raVectorBuyEmpty(p->userData.value);

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


// sub_1004D900, PE 0x1004D900.  A pure teardown: republish thirteen member vptrs in reverse
// declaration order and free the one owned buffer.  It does NOT detach a device-lost listener,
// because sub_100458E0 never registers one.

void raObjectParametersDestroy(void* storage)
{
    RaObjectParameters* const p = static_cast<RaObjectParameters*>(storage);
    void* const holder = raHolder();

    p->objectSpaceWorldUp.vptr = raHolderVec3();
    p->hemiMapSkyColor.vptr    = raHolderVec4();
    p->heightOverTerrain.vptr  = raHolderFloat();

    p->userData.vptr = raHolderBytes();
    if (p->userData.value.first)
        ::renddx9abi::engineFree(p->userData.value.first);
    raVectorBuyEmpty(p->userData.value);

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

RaShader::RaShader(const vc71::string& shaderName, void* renderQueue)
    : m_name(shaderName)     // +0x118 - a copy of the caller's lowered name.  The shipped body
                             // builds it between the seventeen vector clears and the +0x134 byte;
                             // a member initialiser is the only way to say that in source without
                             // constructing the string twice.
{
    m_queue = renderQueue;                       // +0x04, written before the vptr
    m_vptr  = const_cast<void*>(static_cast<const void*>(kRaShaderVtable));

    // Seventeen vectors at +0x08 .. +0x108.  The constructor zeroes _First/_Last/_End of each and
    // never touches the empty allocator dword at the base - the VC7.1 shape Vc71Vector.h records.
    vc71::vector_layout* const vectors[17] =
    {
        &m_instanceParameters[0], &m_instanceParameters[1], &m_instanceParameters[2],
        &m_instanceParameters[3], &m_instanceParameters[4],
        &m_globalParameters[0],   &m_globalParameters[1],   &m_globalParameters[2],
        &m_globalParameters[3],   &m_globalParameters[4],
        &m_templateParameters[0], &m_templateParameters[1], &m_templateParameters[2],
        &m_templateParameters[3], &m_templateParameters[4],
        &m_vertexElementDescriptors, &m_vertexElementNames
    };
    for (int i = 0; i < 17; ++i)
    {
        vectors[i]->first = 0;
        vectors[i]->last  = 0;
        vectors[i]->end   = 0;
    }

    m_byte134  = 1;
    m_pad135[0] = m_pad135[1] = m_pad135[2] = 0;
    m_dword138 = 0;
    m_requiredVertexElements.first = 0;
    m_requiredVertexElements.last  = 0;
    m_requiredVertexElements.end   = 0;
    m_float150 = 1.0f;
    m_queueProfiler = 0;

    // Allocate the 24-byte technique-map sentinel, with color/nil bytes at +0x14/+0x15.
    {
        unsigned char* const raw = static_cast<unsigned char*>(::renddx9abi::engineAlloc(0x18));
        RaTechniqueNode* node = 0;
        if (raw)
        {
            for (unsigned int i = 0; i < 0x18; ++i)
                raw[i] = 0;
            node = reinterpret_cast<RaTechniqueNode*>(raw);
            node->color = 1;                     // _Color = black
            node->isNil = 0;                     // sub_1004CAF0 leaves it 0 ...
        }
        m_techniques.comp = 0;
        m_techniques.head = node;
        // [CHOSEN, not recovered] Guard allocation failure because this allocator may return null.
        if (node)
        {
            node->isNil  = 1;
            node->parent = node;
            node->left   = node;
            node->right  = node;
        }
        m_techniques.size = 0;
    }

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
        m_device = *reinterpret_cast<IDirect3DDevice9* const*>(
            static_cast<const unsigned char*>(g_rendererBase) + 0x0C);
    }

    // Rows 1..20 of the vertex-element table.
    for (unsigned int i = 0; i < kRaVertexElementSplit; ++i)
    {
        vc71::string name(kRaVertexElements[i].name);
        raPushName(m_vertexElementNames, name);
        raPushDescriptor(m_vertexElementDescriptors, kRaVertexElements[i].desc);
    }


    // Temporary parameter blocks trigger one-time registry initialization.
    // Preserve construction and destruction order, including light-array teardown first.
    unsigned int materialStorage[0x150 / 4];
    unsigned int constantsStorage[0xAFC / 4];
    RaLightingParameters   lightingStorage;
    RaObjectParameters     objectStorage;
    RaLightArrayParameters lightArrayStorage;

    raMaterialParametersConstruct(materialStorage);
    raLightingParametersConstruct(&lightingStorage);
    raObjectParametersConstruct(&objectStorage);

    // sub_10038BF0(stack, 0xFFFFFFFF) - a whole RaShaderConstants built on this frame with the
    // device argument -1.  Not a typo in the transcription: `push 0FFFFFFFFh`.
    raShaderConstantsConstructInPlace(constantsStorage, reinterpret_cast<void*>(~0u));

    raLightArrayParametersConstruct(&lightArrayStorage);

    // Rows 21..26 of the vertex-element table, with the blocks alive.
    for (unsigned int i = kRaVertexElementSplit; i < 26; ++i)
    {
        vc71::string name(kRaVertexElements[i].name);
        raPushName(m_vertexElementNames, name);
        raPushDescriptor(m_vertexElementDescriptors, kRaVertexElements[i].desc);
    }

    raLightArrayParametersDestroyInline(&lightArrayStorage);  // the one inlined store
    raShaderConstantsDestroyInPlace(constantsStorage);
    raObjectParametersDestroy(&objectStorage);
    raLightingParametersDestroy(&lightingStorage);
    raMaterialParametersDestroy(materialStorage);
}


// Render-queue shader application helpers.

namespace {

// VC7.1 map::operator[] on the 24-byte technique tree: sub_1004D9C0 for the descent and the
// equal-key hit, sub_1004D460 for the insert.  The descent is the shipped one - `node->key >= key`
// goes left, the hit test is `key >= parent->key` on the lower bound, keys unsigned - and so are
// the link and the leftmost/rightmost maintenance; the inserted node's red-black recolouring is
// [CHOSEN, not recovered], exactly as Vc71Map.h's mapInsertU32 is (RW-74).
void*& raTechniqueMapAt(RaTechniqueMap& map, unsigned int key)
{
    RaTechniqueNode* const head = map.head;
    RaTechniqueNode* parent = head;
    for (RaTechniqueNode* cur = head->parent; !cur->isNil; )
    {
        if (cur->key >= key)
        {
            parent = cur;
            cur = cur->left;
        }
        else
        {
            cur = cur->right;
        }
    }
    if (parent != head && key >= parent->key)
        return parent->value;

    // sub_1004D460 - _Insert({key, 0}) under the lower bound; the node is sub_1004CAF0's 0x18 bytes.
    unsigned char* const raw = static_cast<unsigned char*>(::renddx9abi::engineAlloc(0x18));
    for (unsigned int i = 0; i < 0x18; ++i)
        raw[i] = 0;
    RaTechniqueNode* const node = reinterpret_cast<RaTechniqueNode*>(raw);
    node->key   = key;
    node->value = 0;
    node->left  = head;
    node->right = head;
    node->color = 0;
    node->isNil = 0;
    if (parent == head)
    {
        node->parent = head;
        head->parent = node;
        head->left   = node;
        head->right  = node;
    }
    else
    {
        node->parent = parent;
        if (key < parent->key)
        {
            parent->left = node;
            if (head->left == parent)
                head->left = node;
        }
        else
        {
            parent->right = node;
            if (head->right == parent)
                head->right = node;
        }
    }
    ++map.size;
    return node->value;
}

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
        const unsigned int key = raHashString(vc71::string(desc.Name));
        raTechniqueMapAt(m_techniques, key) = ::renddx9abi::d3dxHandleValue(handle);
    }
}

// sub_1004B3D0, PE 0x1004B3D0.  An empty map is filled first.  A miss inserts a null entry for
// the key (operator[] semantics, as shipped) and answers false with nothing else changed.
bool RaShader::selectTechnique(unsigned int key)
{
    if (m_techniques.size == 0)
        fillTechniqueMap();
    void* const handle = raTechniqueMapAt(m_techniques, key);
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

    vc71::vector_layout& planes = entry->lighting.tail0FC;
    unsigned int index = 0;
    for (const char* plane = planes.first; plane != planes.last; plane += 16, ++index)
        m_device->SetClipPlane(index, reinterpret_cast<const float*>(plane));   // device +0xDC

    if (g_raReleaseClipPlanesAfterUse)                            // dword_1023F3D4, never set
    {
        if (planes.first)
            ::renddx9abi::engineFree(planes.first);
        planes.first = 0;
        planes.last  = 0;
        planes.end   = 0;
    }
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
void RaShader::applyParameterSet(vc71::vector_layout* set, RenderQueueEntry* entry)
{
    applyParameterVector(entry->materialParameters, set[0]);
    applyParameterVector(entry->objectParameters,   set[1]);
    applyParameterVector(g_raShaderConstants,       set[2]);      // dword_1023DDBC
    applyParameterVector(&entry->lightArray,        set[4]);
    applyParameterVector(&entry->lighting,          set[3]);
}

// sub_1004D280, PE 0x1004D280.  For every 80-byte bound parameter: its registry index, through the
// block's own offset vector (block + 4 -> vector, ->first[index]), names the byte offset of the
// holder inside the block; the holder's slot 0 applies it to this shader's effect under the bound
// D3DXHANDLE.  No null test on the block, the registry or the holder, as shipped.
void RaShader::applyParameterVector(void* block, const vc71::vector_layout& parameters)
{
    const unsigned int count = parameters.first
        ? static_cast<unsigned int>(parameters.last - parameters.first) / sizeof(RaBoundParameter)
        : 0u;
    if (!count)
        return;

    typedef int (__fastcall* ApplyFn)(void*, void*, void*, void*);
    unsigned char* const bytes = static_cast<unsigned char*>(block);
    const vc71::vector_layout* const registry =
        *reinterpret_cast<const vc71::vector_layout* const*>(bytes + 4);
    const unsigned int* const offsets = reinterpret_cast<const unsigned int*>(registry->first);
    for (unsigned int i = 0; i < count; ++i)
    {
        const RaBoundParameter& bound =
            reinterpret_cast<const RaBoundParameter*>(parameters.first)[i];
        unsigned char* const holder = bytes + offsets[bound.offset];
        reinterpret_cast<ApplyFn>((*reinterpret_cast<void***>(holder))[0])(
            holder, 0, m_effect, bound.handle);
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
    typedef int (__fastcall* IntSlotFn)(void*, void*);
    void* const material = entry->material;
    void** const slots = *static_cast<void***>(material);

    const int primitiveCount = reinterpret_cast<IntSlotFn>(slots[0x10 / 4])(material, 0);
    (void)reinterpret_cast<IntSlotFn>(slots[0x08 / 4])(material, 0);
    if (!primitiveCount)
        return;

    stateManagerPreDrawApply();

    const int startIndex  = reinterpret_cast<IntSlotFn>(slots[0x20 / 4])(material, 0)
                          + static_cast<int>(entry->lighting.tail10C);
    const int numVertices = reinterpret_cast<IntSlotFn>(slots[0x08 / 4])(material, 0);
    const int minIndex    = reinterpret_cast<IntSlotFn>(slots[0x1C / 4])(material, 0);
    const int baseVertex  = reinterpret_cast<IntSlotFn>(slots[0x18 / 4])(material, 0);
    const int type        = reinterpret_cast<IntSlotFn>(slots[0x14 / 4])(material, 0);
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
    return reinterpret_cast<unsigned char (__fastcall*)(void*, void*)>(vtbl[1])(this, 0) != 0;
}


// Loads "shaders/rashader" + lowercase name + ".fx"; the prefix has no trailing slash.
// Retains initialization of the unused mod-prefix static.

void* RaShader::loadEffect()
{
    // unk_1023F3B4 / unk_1023F398 / unk_1023F37C.  The shipped guards are three bits of
    // dword_1023F3D0 plus an atexit() per string; a function-local static is the same lazy
    // construct-once with the same lifetime, and the destructor registration differs only in which
    // mechanism runs it.
    static const vc71::string s_prefix("shaders/rashader");
    static const vc71::string s_modPrefix("mods/bf2/");      // constructed, never read
    static const vc71::string s_extension(".fx");
    (void)s_modPrefix;

    // `s_prefix + m_name + s_extension`.  MSVC expanded that expression into four extra
    // copy-constructed temporaries in the shipped body; the bytes it produces are these.
    vc71::string path(s_prefix);
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

    // Elements are borrowed pointers, so only the old backing storage is freed.
    if (m_requiredVertexElements.first)
        ::renddx9abi::engineFree(m_requiredVertexElements.first);
    m_requiredVertexElements.first = 0;
    m_requiredVertexElements.last  = 0;
    m_requiredVertexElements.end   = 0;

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
                const vc71::string wanted(value);
                index = raFindString(m_vertexElementNames, wanted);
            }
            if (index != -1)
            {
                raPushDescriptor(
                    m_requiredVertexElements,
                    reinterpret_cast<const RaVertexElementDesc* const*>(
                        m_vertexElementDescriptors.first)[index]);
            }
        }
    }

    // -- 5a. drop the previous binding, all fifteen vectors
    for (int slot = 0; slot < 5; ++slot)
    {
        raClearBoundParameters(m_instanceParameters[slot]);
        raClearBoundParameters(m_globalParameters[slot]);
        raClearBoundParameters(m_templateParameters[slot]);
    }

    // -- 5b. re-bind
    // The shipped body builds a local vector<std::string> of the three struct names and walks it;
    // the order it pushes them in is Instance, Template, Global, and that is the order the effect is
    // interrogated in.
    vc71::vector_layout blockNames = { 0, 0, 0, 0 };
    { const vc71::string n("InstanceParameters"); raPushName(blockNames, n); }
    { const vc71::string n("TemplateParameters"); raPushName(blockNames, n); }
    { const vc71::string n("GlobalParameters");   raPushName(blockNames, n); }

    for (char* cursor = blockNames.first; cursor != blockNames.last;
         cursor += sizeof(vc71::string))
    {
        const vc71::string& blockName = *reinterpret_cast<const vc71::string*>(cursor);

        const D3DXHANDLE block = effect->GetParameterByName(0, blockName.c_str());
        if (!block)
            continue;

        // case-insensitive, AND NOT else-if.  Three independent _stricmp tests against the same
        // three literals the vector holds, so the target can only be one of the three - but nothing
        // guards the case where it is none, and the shipped body would then index off address zero.
        // The missing guard is preserved.
        vc71::vector_layout* scope = 0;
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
                vc71::vector_layout* const target = reinterpret_cast<vc71::vector_layout*>(
                    reinterpret_cast<char*>(scope) + blockIndex * sizeof(vc71::vector_layout));
                raPushBoundParameter(*target, record);
            }
        }
    }

    // Publish the built flag and serial before destroying the local vector.
    m_built       = 1;                                                   // +0x170
    m_buildSerial = static_cast<int>(g_raShaderBuildSerial++);           // dword_1023F078

    for (char* cursor = blockNames.first; cursor != blockNames.last;
         cursor += sizeof(vc71::string))
        reinterpret_cast<vc71::string*>(cursor)->~string();
    if (blockNames.first)
        ::renddx9abi::engineFree(blockNames.first);

    return true;
}


// sub_10041C70, find-or-create.

RaShader* findOrCreateShader(RaShaderTable* table, const vc71::string& name, bool skipFileCheck)
{
    vc71::string lowered;
    raToLower(lowered, name);

    // sub_101C0AD0 is lower_bound on the 0x30-node map at +0x88; the caller then applies the second
    // half of std::map::find by hand - "not end, and not (key < node->key)".  Vc71Map.h's mapFind
    // is that pair written once, over the same node shape and the same strcmp ordering.
    vc71::map_node* const found = vc71::mapFind(table->shaders, lowered.c_str());
    if (found)
        return static_cast<RaShader*>(vc71::mapNodeValue(found));   // node+40

    if (!skipFileCheck)
    {
        // The name supplies the separator after "shaders/rashader".
        vc71::string path("shaders/rashader");
        path += lowered;
        path += ".fx";
        if (!fileManagerResolves(path))
            return 0;
    }

    // One 0x2B0 render queue per shader, named after it (sub_1005F320(0x2B0) + sub_1003B330), then
    // its +0x08 draw mode forced to 1.

    // Queue creation is not null-checked before setting its flag.
    void* const queue = renderQueueCreateNamed(lowered);
    *reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(queue) + 0x08) = 1u;

    // sub_10075110 against "pointlight", then push_back into ONE of the two collections.
    {
        const vc71::string needle("pointlight");
        vc71::vector_layout& collection = raContainsInsensitive(lowered, needle)
            ? table->pointLightQueues      // +0xA4
            : table->worldQueues;          // +0x94
        const void* value = queue;
        raRegistryPush(collection, &value, 4, &raCopyPointer);
    }

    void* const storage = ::renddx9abi::engineAlloc(sizeof(RaShader));   // sub_1005F320(0x178)
    RaShader* shader = 0;
    if (storage)
        shader = new (storage) RaShader(lowered, queue);

    // sub_10043600 - map::insert(value_type(lowered, shader)).  The shipped body builds the pair
    // through three copy-constructed temporaries; the observable result is one node keyed by the
    // lowered name whose value word is the shader.
    {
        vc71::map_node* const node = vc71::mapInsert(table->shaders, 0x30, lowered.c_str());
        if (node)
            vc71::mapNodeValueRef(node) = shader;
    }

    // Creation tests the renderer base's [UNRESOLVED] flag at +0x102;
    // lookup of an existing shader instead tests that shader's built flag.
    if (!readBoolSetting("GSDeferredShaderPermutation", false))
    {
        const unsigned char* const base = static_cast<const unsigned char*>(g_rendererBase);
        if (base && base[0x102] == 0)
        {
            if (shader)
                shader->slot04();
        }
    }

    return shader;
}


// sub_10041F20, the outer entry.

RaShader* getShader(RaShaderTable* table, const vc71::string& name, bool skipFileCheck)
{
    vc71::string lowered;
    raToLower(lowered, name);

    // Empty names and failed creation return the default shader.
    RaShader* result = table->defaultShader;                    // +0x84

    if (!lowered.empty())                                       // std::operator!=(lowered, "")
    {
        // sub_10036410 - the map's own find on the 0x88 map, compared against the +0x8C end.
        vc71::map_node* const node = vc71::mapFind(table->shaders, lowered.c_str());
        if (!node)
        {
            // Preserve the repeated lookup after the successful creation test.
            if (findOrCreateShader(table, lowered, skipFileCheck))
                result = findOrCreateShader(table, lowered, skipFileCheck);
        }
        else
        {
            result = static_cast<RaShader*>(vc71::mapNodeValue(node));   // node+40

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
