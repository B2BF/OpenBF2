// Named mesh render queues and shared per-frame draw statistics.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "RendDX9Base.h"
#include "Texture.h"
#include "Ra/RaShader.h"
#include "../RendMisc/IRenderer.h"

#include <algorithm>
#include <cstring>
#include <stddef.h>
#include <new>

namespace dice {
namespace hfe {
namespace dx9rend {

// dword_1023E408 / dword_1023DE08 / dword_1023DDBC / dword_1023F3D8.  The 0xAFC
// RaShaderConstants block is recovered below.  The render-target system whose ctor writes
// dword_1023F3D8 remains withheld with its consumers still guarded at the call sites.
// [FRONTIER] dice::hfe::dx9rend - the render-target system that publishes dword_1023F3D8
// RendDX9Init.cpp, dword_1023B970 - declared per-TU here exactly as the Geom files declare it; the
// queue add reaches the Renderer's +0x6C statistics slot through it.
extern void* g_rendererBase;

void* g_renderQueues       = 0;

// BF2Render.cpp - the renderer's +0xAC, declared per-TU as Geom/StaticMesh.cpp declares it.
bool rendererIsExpansionPack();
void* g_renderEntryPool      = 0;
void* g_raShaderConstants  = 0;
void* g_renderTargetSystem = 0;

// The accumulation-map projection pair is initialized lazily and only
// once for the process, even if the render target is later replaced with a different size.
unsigned int g_accumulationProjectionInit = 0;
float g_accumulationProjectionOffset[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
float g_accumulationProjectionScale[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };

// The material-cap defaults sub_1004A270 seeds (dword_1023EE18/1023EEB8/1023ED3C/1023ED40/
// 1023ED44/1023ED48). The gate is deviceCaps query #14 through an unread bit table
// (sub_100011D0 vs capsObject+236); the supported arm is taken here - every Direct3D 9 part this
// build can meet supports it - and the unsupported arm's values (4 / 4 / 2.0f / -1.0f) are kept in
// this comment for the day the bit table is read. [CHOSEN, not recovered]
unsigned int g_materialCapA        = 14;      // dword_1023EE18
unsigned int g_materialCapB        = 14;      // dword_1023EEB8
float        g_materialCapScale    = 1.0f;    // dword_1023ED3C
float        g_materialCapBias     = 0.0f;    // dword_1023ED40
float        g_materialCapC        = 0.0f;    // dword_1023ED44 = 0x38000000 (3.0517578e-5f)
float        g_materialCapD        = 0.0f;    // dword_1023ED48

namespace {


// RaShaderConstants - 0xAFC bytes, wrapper sub_1003ACA0 and ctor sub_10038BF0.

// This is deliberately an explicit PE-layout carrier.  The object is a collection of small
// polymorphic constant records rather than a C++ aggregate suitable for a modern compiler: each
// record's first dword points at one of eight tiny binder tables and the five texture records at
// +0x268..+0x288 own raw IDirect3DBaseTexture9 references.  Keeping the carrier as dwords also
// preserves the large +0x4CC..+0xACB span that the shipped constructor intentionally leaves alone.

struct RaShaderConstants
{
    unsigned int words[0xAFC / 4];
};

static_assert(sizeof(RaShaderConstants) == 0xAFC,
              "sub_1003ACA0 allocates exactly 0xAFC bytes");

// These outer record binders are ordinary __thiscall members; the effect and the D3DX handle arrive
// as the two stack arguments RaShader::applyParameterVector passes to every holder's slot 0.
int __fastcall raBindVec4(void* self, void*, void* effect, void* handle)
{
    return static_cast<ID3DXEffect*>(effect)->SetVector(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const D3DXVECTOR4*>(
            static_cast<unsigned char*>(self) + 4));
}

int __fastcall raBindTexture(void* self, void*, void* effect, void* handle)
{
    IDirect3DBaseTexture9* const texture = *reinterpret_cast<IDirect3DBaseTexture9**>(
        static_cast<unsigned char*>(self) + 4);
    return static_cast<ID3DXEffect*>(effect)->SetTexture(
        ::renddx9abi::d3dxHandle(handle), texture);
}

int __fastcall raBindFloat(void* self, void*, void* effect, void* handle)
{
    const FLOAT value = *reinterpret_cast<const FLOAT*>(
        static_cast<unsigned char*>(self) + 4);
    return static_cast<ID3DXEffect*>(effect)->SetFloat(
        ::renddx9abi::d3dxHandle(handle), value);
}

int __fastcall raBindVec3(void* self, void*, void* effect, void* handle)
{
    return static_cast<ID3DXEffect*>(effect)->SetFloatArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const float*>(
            static_cast<unsigned char*>(self) + 4), 3);
}

int __fastcall raBindVec2(void* self, void*, void* effect, void* handle)
{
    return static_cast<ID3DXEffect*>(effect)->SetFloatArray(
        ::renddx9abi::d3dxHandle(handle), reinterpret_cast<const float*>(
            static_cast<unsigned char*>(self) + 4), 2);
}

typedef float* (__fastcall* TransformVec3Fn)(float*, const float*);

int __fastcall raBindTransformedVec3(void* self, void*, void* effect, void* handle)
{
    const unsigned int* const fields = static_cast<const unsigned int*>(self);
    const float* const source = reinterpret_cast<const float*>(fields[1] + 4);
    const TransformVec3Fn transform = reinterpret_cast<TransformVec3Fn>(fields[2]);
    float transformed[3];
    transform(transformed, source);
    return static_cast<ID3DXEffect*>(effect)->SetFloatArray(
        ::renddx9abi::d3dxHandle(handle), transformed, 3);
}

int __fastcall raBindInt(void* self, void*, void* effect, void* handle)
{
    const INT value = *reinterpret_cast<const INT*>(
        static_cast<unsigned char*>(self) + 4);
    return static_cast<ID3DXEffect*>(effect)->SetInt(
        ::renddx9abi::d3dxHandle(handle), value);
}

int __fastcall raBindEngineTexture(void* self, void*, void* effect, void* handle)
{
    EngineTexture* const texture = *reinterpret_cast<EngineTexture**>(
        static_cast<unsigned char*>(self) + 4);
    if (!texture)
        return 0;
    return static_cast<ID3DXEffect*>(effect)->SetTexture(
        ::renddx9abi::d3dxHandle(handle), texture->baseTexture);
}

float* __fastcall raScaleHalf(float* out, const float* value)
{
    out[0] = value[0] * 0.5f;
    out[1] = value[1] * 0.5f;
    out[2] = value[2] * 0.5f;
    return out;
}

float* __fastcall raScaleQuarter(float* out, const float* value)
{
    out[0] = value[0] * 0.25f;
    out[1] = value[1] * 0.25f;
    out[2] = value[2] * 0.25f;
    return out;
}

void* __fastcall raDeleteVec4Record(void* self, void*, unsigned int flags);
void* __fastcall raDeleteFloatRecord(void* self, void*, unsigned int flags);
void* __fastcall raDeleteVec3Record(void* self, void*, unsigned int flags);
void* __fastcall raDeleteVec2Record(void* self, void*, unsigned int flags);
void* __fastcall raDeleteIntRecord(void* self, void*, unsigned int flags);

// off_101E1480..off_101E14BC.  The raw/smart texture binders have no deleting-destructor slot;
// the owning RaShaderConstants destructor releases their payloads itself.
void* const kRaVec4Vtable[2] = {
    reinterpret_cast<void*>(&raBindVec4), reinterpret_cast<void*>(&raDeleteVec4Record)
};
void* const kRaTextureVtable[1] = {
    reinterpret_cast<void*>(&raBindTexture)
};
void* const kRaFloatVtable[2] = {
    reinterpret_cast<void*>(&raBindFloat), reinterpret_cast<void*>(&raDeleteFloatRecord)
};
void* const kRaVec3Vtable[2] = {
    reinterpret_cast<void*>(&raBindVec3), reinterpret_cast<void*>(&raDeleteVec3Record)
};
void* const kRaVec2Vtable[2] = {
    reinterpret_cast<void*>(&raBindVec2), reinterpret_cast<void*>(&raDeleteVec2Record)
};
void* const kRaTransformedVec3Vtable[1] = {
    reinterpret_cast<void*>(&raBindTransformedVec3)
};
void* const kRaIntVtable[2] = {
    reinterpret_cast<void*>(&raBindInt), reinterpret_cast<void*>(&raDeleteIntRecord)
};
void* const kRaEngineTextureVtable[1] = {
    reinterpret_cast<void*>(&raBindEngineTexture)
};

void* deleteRaRecord(void* self, unsigned int flags, void* const* vtable)
{
    *static_cast<void* const**>(self) = vtable;
    if (flags & 1)
        ::renddx9abi::engineFree(self);
    return self;
}

void* __fastcall raDeleteVec4Record(void* self, void*, unsigned int flags)
{
    return deleteRaRecord(self, flags, kRaVec4Vtable);
}

void* __fastcall raDeleteFloatRecord(void* self, void*, unsigned int flags)
{
    return deleteRaRecord(self, flags, kRaFloatVtable);
}

void* __fastcall raDeleteVec3Record(void* self, void*, unsigned int flags)
{
    return deleteRaRecord(self, flags, kRaVec3Vtable);
}

void* __fastcall raDeleteVec2Record(void* self, void*, unsigned int flags)
{
    return deleteRaRecord(self, flags, kRaVec2Vtable);
}

void* __fastcall raDeleteIntRecord(void* self, void*, unsigned int flags)
{
    return deleteRaRecord(self, flags, kRaIntVtable);
}

inline unsigned int ptrWord(const void* p)
{
    return reinterpret_cast<unsigned int>(p);
}

inline unsigned int floatWord(float f)
{
    unsigned int value;
    ::memcpy(&value, &f, sizeof(value));
    return value;
}

void setTextureRecord(RaShaderConstants* constants, unsigned int holderOffset,
                      IDirect3DBaseTexture9* texture)
{
    IDirect3DBaseTexture9** const field = reinterpret_cast<IDirect3DBaseTexture9**>(
        reinterpret_cast<unsigned char*>(constants) + holderOffset + 4);
    if (*field == texture)
        return;
    if (*field)
        (*field)->Release();
    *field = texture;
    if (texture)
        texture->AddRef();
}

void setEngineTextureRecord(RaShaderConstants* constants, unsigned int holderOffset,
                            EngineTexture* texture)
{
    EngineTexture** const field = reinterpret_cast<EngineTexture**>(
        reinterpret_cast<unsigned char*>(constants) + holderOffset + 4);
    if (*field == texture)
        return;
    if (*field && !--(*field)->refCount)
    {
        textureDtor(*field);
        ::renddx9abi::engineFree(*field);
    }
    *field = texture;
    if (texture)
        textureAddRef(texture);
}

void __fastcall raConstantsOnDeviceLost(void* self, void*)
{
    RaShaderConstants* const constants = static_cast<RaShaderConstants*>(self);
    setTextureRecord(constants, 0x268, 0);
    setTextureRecord(constants, 0x270, 0);
    setTextureRecord(constants, 0x278, 0);
    setTextureRecord(constants, 0x280, 0);
    setTextureRecord(constants, 0x288, 0);
}

void __fastcall raConstantsNullListener(void*, void*)
{
}

void* const kRaConstantsDeviceListenerVtable[3] = {
    reinterpret_cast<void*>(&raConstantsOnDeviceLost),
    reinterpret_cast<void*>(&raConstantsNullListener),
    reinterpret_cast<void*>(&raConstantsNullListener)
};

// dword_1023DDD0 and unk_1023DDC0: parallel VC7 vectors of names and byte offsets.  The ctor fills
// them only when the offset vector is empty; duplicates are intentional (the water constants are
// registered once in the short common prefix and again in the complete water group).
vc71::vector_layout g_raConstantNames   = { 0, 0, 0, 0 };
vc71::vector_layout g_raConstantOffsets = { 0, 0, 0, 0 };

// The ctor is their only direct PE xref; their purpose remains
// [UNRESOLVED], so retain address-derived names rather than assigning invented semantics.
int g_raStateDD9C = 0;
int g_raStateDDA0 = 0;
int g_raStateDDA4 = 0;
int g_raStateDDA8 = 0;
int g_raStateDDAC = 0;
int g_raStateDDB0 = 0;
int g_raStateDDB4 = 0;
int g_raStateDDB8 = 0;

struct RaConstantMetadata
{
    const char*  name;
    unsigned int offset;
};

const RaConstantMetadata kRaConstantMetadata[] =
{
    { "FogColor",                    184 },
    { "FogRange",                    164 },
    { "GlobalTime",                  144 },
    { "HemiMap",                     308 },
    { "HemiMapConstants",            316 },
    { "TreeSkyColor",                336 },
    { "WorldSpaceCamPos",             92 },
    { "AmbientIlluminationRange",    204 },
    { "AmbientIlluminationColor",    224 },
    { "WaterFogColor",               264 },
    { "WaterFogRange",               244 },
    { "CameraToWater",               468 },
    { "WaterTransparency",           484 },
    { "WaterReflectivity",           492 },
    { "TerrainSunColor",             280 },
    { "StaticSkyColor",              500 },
    { "StaticSkyColor_1X",           516 },
    { "StaticSpecularColor",         528 },
    { "StaticSpecularColor_1X",      544 },
    { "SinglePointColor",            556 },
    { "SinglePointColor_1X",         572 },
    { "DynamicPointColor",           584 },
    { "NightTimeMode",               600 },
    { "NightOccludedSkyIntensity",   608 },
    { "RoadFadeOut",                 296 },
    { "NormalizationCube",           696 },
    { "SpecLUT64SpecularColor",      704 },
    { "uvLevelAddX",                 356 },
    { "uvLevelMulX",                 364 },
    { "uvLevelAddY",                 372 },
    { "uvLevelMulY",                 380 },
    { "WaterCycleTime",              408 },
    { "WaterScroll",                 388 },
    { "WaterMap",                    416 },
    { "WaterHeight",                 440 },
    { "WaterColor",                  448 },
    { "CameraToWater",               468 },
    { "WaterClipDir",                476 },
    { "WaterTransparency",           484 },
    { "WaterReflectivity",           492 },
    { "WaterMapFrame0",              424 },
    { "WaterMapFrame1",              432 },
    { "ShadowMap",                   616 },
    { "ShadowOccluderMap",           624 },
    { "MultipleShadowMap",           632 },
    { "AccumulationMap",             640 },
    { "ClampMap",                    648 },
    { "AccumulationMapProjScale",    656 },
    { "AccumulationMapProjOffset",   676 }
};

void initializeRaConstantRegistry()
{
    vc71::vectorRef<unsigned int> offsets(&g_raConstantOffsets);
    if (offsets.size() != 0)
        return;

    vc71::vectorRef<vc71::string> names(&g_raConstantNames);
    for (unsigned int i = 0; i != sizeof(kRaConstantMetadata) / sizeof(kRaConstantMetadata[0]); ++i)
    {
        const vc71::string name(kRaConstantMetadata[i].name);
        names.push_back(name);
        offsets.push_back(kRaConstantMetadata[i].offset);
    }
}

struct RaConstantRegistryLifetime
{
    ~RaConstantRegistryLifetime()
    {
        for (char* p = g_raConstantNames.first; p != g_raConstantNames.last;
             p += sizeof(vc71::string))
            reinterpret_cast<vc71::string*>(p)->~string();
        if (g_raConstantNames.first)
            ::operator delete(g_raConstantNames.first);
        if (g_raConstantOffsets.first)
            ::operator delete(g_raConstantOffsets.first);
        g_raConstantNames.first = g_raConstantNames.last = g_raConstantNames.end = 0;
        g_raConstantOffsets.first = g_raConstantOffsets.last = g_raConstantOffsets.end = 0;
    }
};

RaConstantRegistryLifetime g_raConstantRegistryLifetime;

RaShaderConstants* constructRaShaderConstants(void* storage, IDirect3DDevice9* device)
{
    RaShaderConstants* const constants = static_cast<RaShaderConstants*>(storage);
    unsigned int* const w = constants->words;

    // +0x00..+0x58: DeviceLostResetListener, registry carrier and the view matrix.
    w[0] = ptrWord(kRaConstantsDeviceListenerVtable);
    w[1] = ptrWord(&g_raConstantOffsets);
    w[3] = w[4] = w[5] = 0;
    w[6] = 4;
    for (unsigned int i = 7; i <= 22; ++i)
        w[i] = 0;
    w[7] = w[12] = w[17] = w[22] = floatWord(1.0f);

    w[23] = ptrWord(kRaVec4Vtable);
    w[24] = w[25] = w[26] = w[27] = w[28] = floatWord(1.0f);
    w[29] = w[30] = floatWord(0.1f);
    w[31] = w[32] = w[33] = 0;
    w[34] = ptrWord(device);
    w[35] = floatWord(45.0f);

    w[36] = ptrWord(kRaFloatVtable); w[37] = 0;
    w[38] = w[39] = w[40] = 0;

    const unsigned int vec4Records[] = { 41, 46, 51, 56, 61, 79, 84, 97, 112, 164, 169 };
    for (unsigned int i = 0; i != sizeof(vec4Records) / sizeof(vec4Records[0]); ++i)
    {
        const unsigned int at = vec4Records[i];
        w[at] = ptrWord(kRaVec4Vtable);
        w[at + 1] = w[at + 2] = w[at + 3] = w[at + 4] = floatWord(1.0f);
    }

    const unsigned int vec3ZeroRecords[] = { 66, 70, 125, 132 };
    for (unsigned int i = 0; i != sizeof(vec3ZeroRecords) / sizeof(vec3ZeroRecords[0]); ++i)
    {
        const unsigned int at = vec3ZeroRecords[i];
        w[at] = ptrWord(kRaVec3Vtable);
        w[at + 1] = w[at + 2] = w[at + 3] = 0;
    }
    w[74] = ptrWord(kRaVec2Vtable); w[75] = w[76] = 0;

    const unsigned int smartTextureRecords[] = { 77, 104, 106, 108, 174, 176 };
    for (unsigned int i = 0; i != sizeof(smartTextureRecords) / sizeof(smartTextureRecords[0]); ++i)
    {
        const unsigned int at = smartTextureRecords[i];
        w[at] = ptrWord(kRaEngineTextureVtable);
        w[at + 1] = 0;
    }

    const unsigned int floatRecords[] = { 89, 91, 93, 95, 102, 110, 117, 119, 121, 123, 152 };
    for (unsigned int i = 0; i != sizeof(floatRecords) / sizeof(floatRecords[0]); ++i)
    {
        const unsigned int at = floatRecords[i];
        w[at] = ptrWord(kRaFloatVtable);
        w[at + 1] = 0;
    }

    w[129] = ptrWord(kRaTransformedVec3Vtable);
    w[130] = ptrWord(w + 125);
    w[131] = ptrWord(reinterpret_cast<const void*>(&raScaleHalf));
    w[136] = ptrWord(kRaTransformedVec3Vtable);
    w[137] = ptrWord(w + 132);
    w[138] = ptrWord(reinterpret_cast<const void*>(&raScaleHalf));

    w[139] = ptrWord(kRaVec3Vtable);
    w[140] = w[141] = floatWord(1.0f);
    w[142] = floatWord(0.8f);
    w[143] = ptrWord(kRaTransformedVec3Vtable);
    w[144] = ptrWord(w + 139);
    w[145] = ptrWord(reinterpret_cast<const void*>(&raScaleQuarter));

    w[146] = ptrWord(kRaVec3Vtable);
    w[147] = w[148] = w[149] = floatWord(1.0f);
    w[150] = ptrWord(kRaIntVtable); w[151] = 0;

    const unsigned int textureRecords[] = { 154, 156, 158, 160, 162 };
    for (unsigned int i = 0; i != sizeof(textureRecords) / sizeof(textureRecords[0]); ++i)
    {
        const unsigned int at = textureRecords[i];
        w[at] = ptrWord(kRaTextureVtable);
        w[at + 1] = 0;
    }

    w[306] = floatWord(128.0f);
    for (unsigned int i = 691; i != 703; ++i)
        w[i] = 0;

    initializeRaConstantRegistry();

    g_raStateDD9C = -1;
    g_raStateDDA0 = -1;
    g_raStateDDA4 = -1;
    g_raStateDDA8 = -1;
    g_raStateDDAC = 0;
    g_raStateDDB0 = 0;
    g_raStateDDB4 = 0;
    g_raStateDDB8 = 0;

    // sub_10038780: two RGB triples and their companion triples at the final 48 bytes.
    w[691] = w[692] = w[693] = floatWord(0.5f);
    w[694] = w[695] = w[696] = 0;
    w[697] = w[698] = w[699] = floatWord(1.0f);
    w[700] = w[701] = w[702] = 0;

    // The ctor zeroes exactly this 0x200-byte COM-texture array, not the untouched tail around it.
    ::memset(w + 178, 0, 0x200);

    registerDeviceLostResetListener(constants, 3);

    // These final stores intentionally override three earlier Vec4 defaults after registration.
    w[52] = floatWord(1000.0f);
    w[53] = floatWord(1024.0f);
    w[54] = floatWord(1.0f);
    w[55] = 0;
    w[57] = w[58] = w[59] = 0;
    w[60] = floatWord(1.0f);
    return constants;
}

void releaseEngineTextureRecord(RaShaderConstants* constants, unsigned int recordWord)
{
    constants->words[recordWord] = ptrWord(kRaEngineTextureVtable);
    EngineTexture* const texture = reinterpret_cast<EngineTexture*>(
        constants->words[recordWord + 1]);
    if (texture && !--texture->refCount)
    {
        textureDtor(texture);
        ::renddx9abi::engineFree(texture);
    }
}

void releaseRawTextureRecord(RaShaderConstants* constants, unsigned int recordWord)
{
    constants->words[recordWord] = ptrWord(kRaTextureVtable);
    IDirect3DBaseTexture9* const texture = reinterpret_cast<IDirect3DBaseTexture9*>(
        constants->words[recordWord + 1]);
    if (texture)
        texture->Release();
}

void restoreRaRecord(RaShaderConstants* constants, unsigned int recordWord,
                     void* const* vtable)
{
    constants->words[recordWord] = ptrWord(vtable);
}

void destroyRaShaderConstantsObject(RaShaderConstants* constants)
{
    if (!constants)
        return;

    // sub_100388E0 is a real aggregate destructor: restore the primary listener vptr, unregister,
    // then unwind every non-trivial embedded record in reverse construction order.
    constants->words[0] = ptrWord(kRaConstantsDeviceListenerVtable);
    unregisterDeviceLostResetListener(constants);

    IDirect3DBaseTexture9** array = reinterpret_cast<IDirect3DBaseTexture9**>(
        reinterpret_cast<unsigned char*>(constants) + 0x2C8);
    for (unsigned int i = 0; i != 128; ++i)
        if (array[i])
            array[i]->Release();

    for (unsigned int i = 38; i <= 40; ++i)
    {
        IUnknown* const value = reinterpret_cast<IUnknown*>(constants->words[i]);
        if (value)
            value->Release();
    }

    releaseEngineTextureRecord(constants, 176);
    releaseEngineTextureRecord(constants, 174);
    restoreRaRecord(constants, 169, kRaVec4Vtable);
    restoreRaRecord(constants, 164, kRaVec4Vtable);
    releaseRawTextureRecord(constants, 162);
    releaseRawTextureRecord(constants, 160);
    releaseRawTextureRecord(constants, 158);
    releaseRawTextureRecord(constants, 156);
    releaseRawTextureRecord(constants, 154);
    restoreRaRecord(constants, 152, kRaFloatVtable);
    restoreRaRecord(constants, 150, kRaIntVtable);
    restoreRaRecord(constants, 146, kRaVec3Vtable);
    restoreRaRecord(constants, 139, kRaVec3Vtable);
    restoreRaRecord(constants, 132, kRaVec3Vtable);
    restoreRaRecord(constants, 125, kRaVec3Vtable);
    restoreRaRecord(constants, 123, kRaFloatVtable);
    restoreRaRecord(constants, 121, kRaFloatVtable);
    restoreRaRecord(constants, 119, kRaFloatVtable);
    restoreRaRecord(constants, 117, kRaFloatVtable);
    restoreRaRecord(constants, 112, kRaVec4Vtable);
    restoreRaRecord(constants, 110, kRaFloatVtable);
    releaseEngineTextureRecord(constants, 108);
    releaseEngineTextureRecord(constants, 106);
    releaseEngineTextureRecord(constants, 104);
    restoreRaRecord(constants, 102, kRaFloatVtable);
    restoreRaRecord(constants, 97, kRaVec4Vtable);
    restoreRaRecord(constants, 95, kRaFloatVtable);
    restoreRaRecord(constants, 93, kRaFloatVtable);
    restoreRaRecord(constants, 91, kRaFloatVtable);
    restoreRaRecord(constants, 89, kRaFloatVtable);
    restoreRaRecord(constants, 84, kRaVec4Vtable);
    restoreRaRecord(constants, 79, kRaVec4Vtable);
    releaseEngineTextureRecord(constants, 77);
    restoreRaRecord(constants, 74, kRaVec2Vtable);
    restoreRaRecord(constants, 70, kRaVec3Vtable);
    restoreRaRecord(constants, 66, kRaVec3Vtable);
    restoreRaRecord(constants, 61, kRaVec4Vtable);
    restoreRaRecord(constants, 56, kRaVec4Vtable);
    restoreRaRecord(constants, 51, kRaVec4Vtable);
    restoreRaRecord(constants, 46, kRaVec4Vtable);
    restoreRaRecord(constants, 41, kRaVec4Vtable);
    restoreRaRecord(constants, 36, kRaFloatVtable);
    restoreRaRecord(constants, 23, kRaVec4Vtable);

    if (constants->words[3])
        ::renddx9abi::engineFree(reinterpret_cast<void*>(constants->words[3]));
    constants->words[3] = constants->words[4] = constants->words[5] = 0;
}


// Render queue with draw/sort modes, reusable entry storage, and a shared technique key.
// Transparent passes temporarily apply the six alpha-state overrides.

struct RenderQueue
{
    void* const*  vptr;              // +0x000
    unsigned int  field04;           // +0x004
    unsigned int  drawMode;          // +0x008  (ctor: 0)
    unsigned char profiler[0x27C - 0x0C];   // +0x00C sortMode (ctor: 1) then the bracket storage
    unsigned int* entriesFirst;      // +0x27C
    unsigned int* entriesLast;       // +0x280
    unsigned int* entriesEnd;        // +0x284
    unsigned char enabled;           // +0x288
    unsigned char pad289[3];
    vc71::string  name;              // +0x28C
    unsigned char transparentPass;   // +0x2A8
    unsigned char pad2A9[3];
    unsigned int  sharedKey;         // +0x2AC

    void flushWithArg(unsigned char runDraws);  // +0x08
    void flush();                               // +0x0C
    void setSharedString(const vc71::string* value);   // +0x10
    void drawWalkNear();                        // +0x14
    void drawWalkDefault();                     // +0x18
    void add(RenderQueueEntry* entry);          // +0x04
};

static_assert(sizeof(RenderQueue) == 0x2B0, "sub_10042400 allocates 0x2B0 per queue");
static_assert(offsetof(RenderQueue, entriesFirst) == 0x27C, "entry vector at +0x27C");
static_assert(offsetof(RenderQueue, name) == 0x28C, "queue name at +0x28C");
static_assert(offsetof(RenderQueue, sharedKey) == 0x2AC, "shared key at +0x2AC");

// Initializes queue modes, storage, and flags; drawMode and sortMode occupy +0x008/+0x00C.

extern void* const kRenderQueueVtable[8];

RenderQueue* renderQueueConstruct(void* storage, const char* queueName)
{
    RenderQueue* q = static_cast<RenderQueue*>(storage);
    q->vptr = kRenderQueueVtable;
    *reinterpret_cast<unsigned int*>(reinterpret_cast<unsigned char*>(q) + 0x08) = 0;
    *reinterpret_cast<unsigned int*>(reinterpret_cast<unsigned char*>(q) + 0x0C) = 1;
    // sub_10047F20(name) - the profiler stat registration; stat-only scaffolding, not reproduced.
    q->entriesFirst = 0;
    q->entriesLast  = 0;
    q->entriesEnd   = 0;
    q->enabled      = 1;
    new (&q->name) vc71::string(queueName);
    q->transparentPass = 0;
    q->sharedKey       = 0;
    return q;
}

inline unsigned int& queueSortMode(RenderQueue* q)
{
    return *reinterpret_cast<unsigned int*>(reinterpret_cast<unsigned char*>(q) + 0x0C);
}


// Returns the per-entry camera distance used by queue sorting.
inline float entryCameraDistance(const RenderQueueEntry* e)
{
    return *reinterpret_cast<const float*>(
        static_cast<const unsigned char*>(e->objectParameters) + 0x220);
}

// Sort mode 1: front to back.
struct RenderEntryNearerFirst
{
    bool operator()(RenderQueueEntry* a, RenderQueueEntry* b) const
    {
        return entryCameraDistance(a) < entryCameraDistance(b);
    }
};

// Sort mode 2: back to front for transparency, additive, and glow queues.
struct RenderEntryFartherFirst
{
    bool operator()(RenderQueueEntry* a, RenderQueueEntry* b) const
    {
        return entryCameraDistance(a) > entryCameraDistance(b);
    }
};

// The material's +0x04 - StaticMeshMaterial::applyStreams(shader), bool in al - by slot, exactly as
// both walks dispatch it.
inline bool materialApplyStreams(void* material, Ra::RaShader* shader)
{
    typedef unsigned char (__fastcall* ApplyStreamsFn)(void*, void*, void*);
    return reinterpret_cast<ApplyStreamsFn>((*static_cast<void***>(material))[1])(
        material, 0, shader) != 0;
}

// A queue's own +0x04 (add), by slot, as the first-person walk's fallback dispatches it.
inline void queueAddThroughTable(void* queue, RenderQueueEntry* entry)
{
    typedef void (__fastcall* AddFn)(void*, void*, RenderQueueEntry*);
    reinterpret_cast<AddFn>((*static_cast<void***>(queue))[1])(queue, 0, entry);
}

// slot +0x08, sub_1003B1F0 (0xEF B) - flush.  The empty path IS the menu path: disabled, a null
// vector or a zero count return before anything is touched.  Without draws the vector is simply
// emptied.  With them: sort by mode, walk - the first-person walk only for drawMode 1 off the
// expansion pack, else the default one, both by vtable - then empty the vector.
// sub_1003BF30 is `vector<Entry*>::resize(0, 0)` on the vector at +0x278, NOT a "latch reset" as
// an earlier comment here said; on pointers that is exactly `last = first`.
void RenderQueue::flushWithArg(unsigned char runDraws)
{
    if (!enabled)
        return;
    RenderQueueEntry** const first = reinterpret_cast<RenderQueueEntry**>(entriesFirst);
    if (!first)
        return;
    RenderQueueEntry** const last = reinterpret_cast<RenderQueueEntry**>(entriesLast);
    if (last - first == 0)
        return;
    if (!runDraws)
    {
        entriesLast = entriesFirst;                                // sub_1003BF30(0, 0)
        return;
    }

    // sub_10046EE0(this + 0x10) - the profiler bracket opens here.
    const unsigned int sortMode = queueSortMode(this);
    if (sortMode == 1u)
        std::sort(first, last, RenderEntryNearerFirst());
    else if (sortMode == 2u)
        std::sort(first, last, RenderEntryFartherFirst());

    typedef void (__fastcall* WalkFn)(void*, void*);
    if (drawMode != 1u || rendererIsExpansionPack())              // renderer +0xAC
        reinterpret_cast<WalkFn>(vptr[0x18 / sizeof(void*)])(this, 0);
    else
        reinterpret_cast<WalkFn>(vptr[0x14 / sizeof(void*)])(this, 0);

    entriesLast = entriesFirst;                                    // sub_1003BF30(0, 0)
    // sub_10046EF0(this + 0x10) - and closes here.
}

// Draws entries while batching shader, technique, pass, and material changes.
// A failed stream bind skips the entry; transparent draws reapply alpha states.
// Ends only passes and effects successfully begun.
void RenderQueue::drawWalkDefault()
{
    Ra::RaShader* shader     = 0;
    int           begunPass  = -1;
    int           activePass = -1;
    unsigned int  passCount  = 0;
    void*         lastMaterial = 0;
    unsigned int  lastKey    = 0;

    for (unsigned int i = 0; ; ++i)
    {
        RenderQueueEntry** const first = reinterpret_cast<RenderQueueEntry**>(entriesFirst);
        if (!first
            || i >= static_cast<unsigned int>(reinterpret_cast<RenderQueueEntry**>(entriesLast)
                                              - first))
            break;
        RenderQueueEntry* const e = first[i];
        bool shaderChanged = false;

        if (sharedKey)
            e->techniqueKey = sharedKey;

        if (!(shader == e->shader && lastKey == e->techniqueKey))
        {
            if (shader)
            {
                if (activePass != -1)
                {
                    shader->endPass();
                    activePass = -1;
                }
                if (begunPass != -1)
                {
                    shader->end();                                 // +0x0C
                    begunPass = -1;
                }
            }
            shader  = e->shader;
            lastKey = e->techniqueKey;
            shader->m_queueProfiler = reinterpret_cast<unsigned char*>(this) + 0x10;
            shaderChanged = true;
            passCount = shader->begin(e);                          // +0x08
            if (passCount)
                begunPass = 0;
        }

        if (passCount)
        {
            for (int pass = begunPass; pass < static_cast<int>(passCount); ++pass)
            {
                if (activePass != pass)
                {
                    if (activePass != -1)
                        shader->endPass();
                    shader->beginPass(static_cast<unsigned int>(pass));
                    activePass = pass;
                }

                bool draw;
                if (!shaderChanged && lastMaterial == e->material)
                {
                    draw = true;
                }
                else
                {
                    shaderChanged = false;
                    lastMaterial  = e->material;
                    draw = materialApplyStreams(e->material, shader);   // material +0x04
                }

                if (draw)
                {
                    shader->applyTemplateParameters(e);            // +0x14
                    shader->applyInstanceParameters(e);            // +0x10
                    if (transparentPass)
                    {
                        stateManagerSetRenderState(15, 1);         // D3DRS_ALPHATESTENABLE
                        stateManagerSetRenderState(24, 0);         // D3DRS_ALPHAREF
                        stateManagerSetRenderState(25, 5);         // D3DRS_ALPHAFUNC = greaterequal
                        stateManagerSetRenderState(27, 1);         // D3DRS_ALPHABLENDENABLE
                        stateManagerSetRenderState(19, 5);         // D3DRS_SRCBLEND = srcalpha
                        stateManagerSetRenderState(20, 6);         // D3DRS_DESTBLEND = invsrcalpha
                    }
                    shader->draw(e);                               // +0x18
                }
            }
            begunPass = 0;
        }
    }

    if (shader)
    {
        if (activePass != -1)
            shader->endPass();
        if (begunPass != -1)
            shader->end();
    }
}

// Draws first-person entries with the first entry's shader and pass 0, grouped by material.
// Consumes drawn entries; failed stream bindings requeue entries on the default shader.
void RenderQueue::drawWalkNear()
{
    if (!enabled)
        return;
    RenderQueueEntry** entries = reinterpret_cast<RenderQueueEntry**>(entriesFirst);
    if (!entries)
        return;
    const unsigned int count = static_cast<unsigned int>(
        reinterpret_cast<RenderQueueEntry**>(entriesLast) - entries);
    if (!count)
        return;

    Ra::RaShader* const shader = entries[0]->shader;
    shader->m_queueProfiler = reinterpret_cast<unsigned char*>(this) + 0x10;
    if (!shader->begin(entries[0]))                                // +0x08
        return;
    shader->beginPass(0);

    for (unsigned int i = 0; i < count; )
    {
        entries = reinterpret_cast<RenderQueueEntry**>(entriesFirst);
        void* const material = entries[i]->material;
        const bool applied = materialApplyStreams(material, shader);   // material +0x04
        shader->applyTemplateParameters(entries[i]);               // +0x14

        for (unsigned int j = i; j < count; ++j)
        {
            entries = reinterpret_cast<RenderQueueEntry**>(entriesFirst);
            RenderQueueEntry* const e = entries[j];
            if (e && e->material == material)
            {
                entries[j] = 0;
                if (!applied)
                {
                    Ra::RaShader* const fallback = Ra::getShader(
                        static_cast<Ra::RaShaderTable*>(g_renderQueues),
                        vc71::string("default"), false);
                    queueAddThroughTable(fallback->queue(), e);    // its queue's +0x04
                }
                else
                {
                    shader->applyInstanceParameters(e);            // +0x10
                    shader->draw(e);                               // +0x18
                }
            }
        }

        entries = reinterpret_cast<RenderQueueEntry**>(entriesFirst);
        while (!entries[i])
        {
            if (++i >= count)
                goto done;
        }
    }
done:
    shader->endPass();
    shader->end();                                                 // +0x0C
}


void __fastcall rq_flushWithArg(void* self, void*, unsigned char runDraws)
{
    static_cast<RenderQueue*>(self)->flushWithArg(runDraws);
}

// slot +0x0C, sub_1003ACF0 - flush(1) behind the reentrancy byte.
void RenderQueue::flush()
{
    transparentPass = 1;
    flushWithArg(1);
    transparentPass = 0;
}

void __fastcall rq_flushNoArg(void* self, void*)
{
    static_cast<RenderQueue*>(self)->flush();
}

// sub_1000D600 on the block the renderer's slot +0x6C returns (this + 0x108):
//     category = block[5]; ++block[category + 1]; ++block[4]
// - a per-category draw counter and the frame total.  Texture.cpp reaches the same block for its
// texture count; the category word is whatever the current pass set it to.
void renderStatsCountDraw()
{
    IRenderer* const renderer = static_cast<IRenderer*>(g_rendererBase);
    unsigned int* const stats = static_cast<unsigned int*>(renderer->getRenderStats());
    const unsigned int category = stats[5];
    ++stats[category + 1];
    ++stats[4];
}


// slot +0x04, sub_1003B170 - add an entry.  (it was a frontier while nothing
// recovered could enqueue).  Three steps, in shipped order: a disabled queue drops the entry on
// the floor; an entry with no shader is given the Ra table's "default" shader (getShader with
// skipFileCheck = false); the renderer's draw statistics are bumped; the entry is pushed.

// The push is sub_1003C120 - the inline "room left in the block" fast path, else _Insert_n(1) -
// on the entry vector whose allocator word sits at +0x278 and whose _First is the +0x27C member.

void RenderQueue::add(RenderQueueEntry* entry)
{
    if (!enabled)
        return;

    if (!entry->shader)
    {
        entry->shader = Ra::getShader(static_cast<Ra::RaShaderTable*>(g_renderQueues),
                                      vc71::string("default"), false);
    }

    renderStatsCountDraw();

    vc71::vector_layout& entries = *reinterpret_cast<vc71::vector_layout*>(
        reinterpret_cast<unsigned char*>(this) + 0x278);
    vc71::vectorPushBackIn(entries, sizeof(void*), &entry);
}

void __fastcall rq_add(void* self, void*, RenderQueueEntry* entry)
{
    static_cast<RenderQueue*>(self)->add(entry);
}

// slot +0x10, sub_1003ACD0 - hash the string it is handed (sub_1003ABB0) and keep the hash.
// Not a pointer park, as this comment once said: the default walk stamps the value into every
// entry's techniqueKey and RaShader::begin looks it up in the technique map, which is keyed by the
// same hash of each technique's name - so "draw_1p" selects a shader's draw_1p technique.  No
// null test on the string, as shipped.
void RenderQueue::setSharedString(const vc71::string* value)
{
    sharedKey = raHashString(*value);
}

void __fastcall rq_setSharedString(void* self, void*, const vc71::string* str)
{
    static_cast<RenderQueue*>(self)->setSharedString(str);
}

#define RD9_RQ_FRONTIER(name, slotOffset, addr)                                     \
    void __fastcall name(void* self, void*)                                         \
    {                                                                               \
        RD9_FRONTIER_AT("dice::hfe::dx9rend::RenderQueue::slot_" #slotOffset        \
                        " (" #addr ")", self, slotOffset);                          \
    }

RD9_RQ_FRONTIER(rq_slot00, 0x00, 0x1003C410)   // destroy - shutdown only

void __fastcall rq_drawWalkNear(void* self, void*)
{
    static_cast<RenderQueue*>(self)->drawWalkNear();
}

void __fastcall rq_drawWalkDefault(void* self, void*)
{
    static_cast<RenderQueue*>(self)->drawWalkDefault();
}

#undef RD9_RQ_FRONTIER

extern void* const kRenderQueueVtable[8] =
{
    reinterpret_cast<void*>(&rq_slot00),          // +0x00
    reinterpret_cast<void*>(&rq_add),             // +0x04
    reinterpret_cast<void*>(&rq_flushWithArg),    // +0x08
    reinterpret_cast<void*>(&rq_flushNoArg),      // +0x0C
    reinterpret_cast<void*>(&rq_setSharedString), // +0x10
    reinterpret_cast<void*>(&rq_drawWalkNear),    // +0x14
    reinterpret_cast<void*>(&rq_drawWalkDefault), // +0x18
    0                                             // +0x1C  null in the shipped table too
};

// The file-scope "draw_1p" string (unk_1023E704, built once behind dword_1023E720's bit 0) the
// table hands to the eight first-person-family queues.
const vc71::string& sharedDraw1pString()
{
    static const vc71::string s("draw_1p");
    return s;
}


// Collection of named render queues with compatibility pointer-slot layout.

struct RenderQueueCollection
{
    unsigned int allocatorOpaque; // PE ctor deliberately leaves +0x94/+0xA4 unwritten
    RenderQueue** first;
    RenderQueue** last;
    RenderQueue** end;
};

struct RenderQueueTable
{
    void* const*  vptr;               // +0x000
    RenderQueue*  slots[32];          // +0x004 .. +0x083
    // Shares the RaShaderTable defaultShader position.
    void*         worldQueueOwner;     // +0x084  ctor: 0; Ra::RaShaderTable::defaultShader
    unsigned int  map88;              // +0x088  comp
    void*         mapHead;            // +0x08C  the nil node (0x30, isnil, self-linked)
    unsigned int  mapSize;            // +0x090
    RenderQueueCollection worldCollections[2]; // +0x094 / +0x0A4
    unsigned char byteB4;             // +0x0B4  ctor: 1
    unsigned char padB5[3];

    void flushWorldPass();
};

static_assert(sizeof(RenderQueueTable) == 0xB8, "sub_10042CA0 allocates 0xB8 for the table");
static_assert(offsetof(RenderQueueTable, map88) == 0x88, "the map header at +0x88");
static_assert(offsetof(RenderQueueTable, worldCollections) == 0x94,
              "sub_1003AD80's first queue collection is at +0x94");
static_assert(offsetof(RenderQueueTable, byteB4) == 0xB4, "the trailing byte at +0xB4");

void __fastcall rqt_destroy(void* self, void*, int alsoFree)
{
    RD9_FRONTIER_AT("dice::hfe::dx9rend::RenderQueueTable - destroy (sub_10041A50's vtbl slot; "
                    "shutdown only)", self, alsoFree);
}

void* const kRenderQueueTableVtable[1] =
{
    reinterpret_cast<void*>(&rqt_destroy)         // +0x00  the destroy-with-flag slot
};

RenderQueue* makeQueue(const char* queueName)
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(RenderQueue));
    if (!storage)
        return 0;
    return renderQueueConstruct(storage, queueName);
}

void giveSharedString(RenderQueue* q)
{
    if (!q)
        return;
    q->setSharedString(&sharedDraw1pString());
}

} // anonymous namespace


// Case-folded hash for technique names and shared queue keys.
// Non-ASCII characters retain signed-byte extension in the rolling arithmetic.

unsigned int raHashString(const vc71::string& s)
{
    unsigned int hash = 0;
    unsigned int a = 230498u;                                      // 0x38462
    unsigned int b = 39842u;                                       // 0x9BA2
    const char* const chars = s.c_str();
    const unsigned int length = s.size();
    for (unsigned int i = 0; i < length; ++i)
    {
        int c = static_cast<signed char>(chars[i]);
        if (c >= 0x61 && c <= 0x7A)
            c -= 0x20;
        const unsigned int x = static_cast<unsigned int>(c) << ((a & 7u) - (b & 7u) + 16u);
        const unsigned int y = hash << ((b & 3u) - (a & 3u) + 15u);
        a -= 0x76DCBACCu;
        b += 0x57542289u;
        hash += i | ((x + y) >> 14);
    }
    return hash + (hash >> 16);
}

// Cross-unit adapters to queue and shader-constant helpers.
// In-place constant construction does not allocate or publish the global constants object.
namespace Ra {

void* renderQueueCreateNamed(const vc71::string& name)
{
    return makeQueue(name.c_str());
}

void* raShaderConstantsConstructInPlace(void* storage, void* device)
{
    return constructRaShaderConstants(storage, static_cast<IDirect3DDevice9*>(device));
}

void raShaderConstantsDestroyInPlace(void* object)
{
    destroyRaShaderConstantsObject(static_cast<RaShaderConstants*>(object));
}

// dword_1023DDD0 - this file's RaShaderConstants parameter-name registry, the fourth of the five
// the Ra shader build (PE 0x1004B4E0) probes.  The shipped body reaches it as `mov ecx, offset
// unk_1023DDC0` and reads the names at `this+0x10`, because the image lays the Ra registry pairs
// out on a 0x20 stride; this rebuild's two vectors are separate globals, so the name vector is
// published on its own.  Read-only to the caller - only its _First/_Last are used.
// for Ra/RaShader.cpp's parameter binding (RW-124).
const vc71::vector_layout& raShaderConstantsParameterNames()
{
    return g_raConstantNames;
}

} // namespace Ra

// sub_1003ACA0 - allocate 0xAFC, construct with the D3D device, publish dword_1023DDBC.
void* createRaShaderConstants(IDirect3DDevice9* device)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(RaShaderConstants));
    if (!storage)
    {
        g_raShaderConstants = 0;
        return 0;
    }

    RaShaderConstants* const constants = constructRaShaderConstants(storage, device);
    g_raShaderConstants = constants;
    return constants;
}

// sub_10027850 on one of the five in-object texture records.  LightManager publishes four of
// them (+0x268, +0x270, +0x278 and +0x288); device loss clears all five in shipped order.
void raShaderConstantsSetTexture(unsigned int holderOffset, IDirect3DBaseTexture9* texture)
{
    setTextureRecord(static_cast<RaShaderConstants*>(g_raShaderConstants), holderOffset, texture);
}

// sub_1015CBED..1015CC2E / 0x1015CD55..0x1015CD98 use the two smart EngineTexture records whose
// vptrs live at +0x2B8/+0x2C0 and whose owned pointers live four bytes later.
void raShaderConstantsSetEngineTexture(unsigned int holderOffset, EngineTexture* texture)
{
    setEngineTextureRecord(
        static_cast<RaShaderConstants*>(g_raShaderConstants), holderOffset, texture);
}

// sub_10014930 is EngineTexture::baseTexture and sub_100846A0 reads
// the low 16 bits of width/height to form the half-texel offset. The two Vec4 records are copied on
// every world pass, but their backing values are initialized only once behind bits 0 and 1.
void raShaderConstantsSetAccumulationMap(EngineTexture* texture)
{
    RaShaderConstants* const constants = static_cast<RaShaderConstants*>(g_raShaderConstants);
    setTextureRecord(constants, 0x280u, texture->baseTexture);

    const float halfTexelX = 0.5f / static_cast<float>(
        static_cast<unsigned short>(texture->width));
    const float halfTexelY = 0.5f / static_cast<float>(
        static_cast<unsigned short>(texture->height));

    if ((g_accumulationProjectionInit & 1u) == 0)
    {
        g_accumulationProjectionScale[0] = 0.5f;
        g_accumulationProjectionScale[1] = -0.5f;
        g_accumulationProjectionScale[2] = 1.0f;
        g_accumulationProjectionScale[3] = 1.0f;
        g_accumulationProjectionInit |= 1u;
    }
    if ((g_accumulationProjectionInit & 2u) == 0)
    {
        g_accumulationProjectionOffset[2] = 0.0f;
        g_accumulationProjectionOffset[3] = 0.0f;
        g_accumulationProjectionOffset[0] = halfTexelX + 0.5f;
        const float offsetY = halfTexelY + 0.5f;
        g_accumulationProjectionInit |= 2u;
        g_accumulationProjectionOffset[1] = offsetY;
    }

    ::memcpy(reinterpret_cast<unsigned char*>(constants) + 0x294,
             g_accumulationProjectionScale, sizeof(g_accumulationProjectionScale));
    ::memcpy(reinterpret_cast<unsigned char*>(constants) + 0x2A8,
             g_accumulationProjectionOffset, sizeof(g_accumulationProjectionOffset));
}

// The Hemi texture is an EngineTexture record (holder +0x134), not a
// raw D3D texture; its four constants are the Vec4 payload at +0x140.
void raShaderConstantsSetHemiMapTexture(EngineTexture* texture)
{
    setEngineTextureRecord(static_cast<RaShaderConstants*>(g_raShaderConstants), 0x134u, texture);
}

void raShaderConstantsSetHemiMapInfo(const float constants[4])
{
    ::memcpy(static_cast<unsigned char*>(g_raShaderConstants) + 0x140,
             constants, 4u * sizeof(float));
}

// Updates shader fog color and persistent fixed-function fog state.
// RGB bytes truncate toward zero; packed alpha is always opaque.
void raShaderConstantsSetFogColor(const float rgbw[4])
{
    unsigned char* const block = static_cast<unsigned char*>(g_raShaderConstants);
    ::memcpy(block + 0xBC, rgbw, 4u * sizeof(float));

    unsigned int packed =
        static_cast<unsigned int>(static_cast<int>(rgbw[0] * 255.0f)) | 0xFFFFFF00u;
    packed <<= 8;
    packed |= static_cast<unsigned int>(static_cast<int>(rgbw[1] * 255.0f)) & 0xFFu;
    packed <<= 8;
    packed |= static_cast<unsigned int>(static_cast<int>(rgbw[2] * 255.0f)) & 0xFFu;
    stateManagerSetRenderStatePersistent(34u, packed);        // D3DRS_FOGCOLOR
}

// sub_10037F60 - FogRange (+0xA8..+0xB4): the four calcFog coefficients, `w*xy + zw`.
void raShaderConstantsSetFogRange(const float range[4])
{
    ::memcpy(static_cast<unsigned char*>(g_raShaderConstants) + 0xA8,
             range, 4u * sizeof(float));
}

// sub_10038110 - WaterFogRange (+0xF8..+0x104).
void raShaderConstantsSetWaterFogRange(const float range[4])
{
    ::memcpy(static_cast<unsigned char*>(g_raShaderConstants) + 0xF8,
             range, 4u * sizeof(float));
}

// sub_10038140 - WaterFogColor (+0x10C..+0x114), a Vec3 holder.
void raShaderConstantsSetWaterFogColor(const float rgb[3])
{
    ::memcpy(static_cast<unsigned char*>(g_raShaderConstants) + 0x10C,
             rgb, 3u * sizeof(float));
}

// Terrain::setupWater (0x100DA210) stores four floats at +0x168, +0x170, +0x178 and +0x180: the
// value words of the float holders the constructor registers as uvLevelAddX (356), uvLevelMulX
// (364), uvLevelAddY (372) and uvLevelMulY (380) - the heightmap's world extent as a UV mapping.
void raShaderConstantsSetUvLevel(float addX, float mulX, float addY, float mulY)
{
    unsigned char* const raw = static_cast<unsigned char*>(g_raShaderConstants);
    *reinterpret_cast<float*>(raw + 0x168) = addX;
    *reinterpret_cast<float*>(raw + 0x170) = mulX;
    *reinterpret_cast<float*>(raw + 0x178) = addY;
    *reinterpret_cast<float*>(raw + 0x180) = mulY;
}

// The owned texture of an EngineTexture holder - the word after its vptr. Terrain::setupWater
// tests WaterMap (+0x1A0, the holder the constructor names at 416) before loading the water
// volume into it.
EngineTexture* raShaderConstantsGetEngineTexture(unsigned int holderOffset)
{
    return *reinterpret_cast<EngineTexture**>(
        static_cast<unsigned char*>(g_raShaderConstants) + holderOffset + 4);
}

// sub_1003AD10 -> sub_100388E0.  RendDX9_shutdownD3DSubsystems calls this at 0x1000AE5D,
// after sub_1004A080/sub_10041A50 and before detaching/destroying the Ra::EffectManager.
void destroyRaShaderConstants()
{
    RaShaderConstants* const constants = static_cast<RaShaderConstants*>(g_raShaderConstants);
    if (!constants)
        return;
    destroyRaShaderConstantsObject(constants);
    ::renddx9abi::engineFree(constants);
}


// Reusable render-entry pool. Frame end resets the cursor; acquisition grows storage on demand.

struct RenderEntryPool
{
    unsigned int        next;      // +0x00  entries handed out so far this frame
    vc71::vector_layout entries;   // +0x04  every entry ever built (the pointer vector)
};

static_assert(sizeof(RenderEntryPool) == 0x14, "sub_1003AC70 allocates 0x14");

void createRenderEntryPool()
{
    RenderEntryPool* pool = static_cast<RenderEntryPool*>(
        ::renddx9abi::engineAlloc(sizeof(RenderEntryPool)));
    if (pool)
    {
        pool->next          = 0;   // [0]
        pool->entries.first = 0;   // [2]
        pool->entries.last  = 0;   // [3]
        pool->entries.end   = 0;   // [4]   [1], the allocator word, is deliberately not written
    }
    g_renderEntryPool = pool;
}


// sub_100D1460, the entry constructor.

// In shipped order: the two head words, the 0x110 lighting block, the object-parameters pointer,
// the 0x1C light-array block, the two tail words, and then a 0x44 RaLightRecord allocated on the
// engine heap and hung on lightArray.lights.value (dword 74 = +0x128).  An allocation failure
// leaves that pointer NULL rather than stopping, as shipped.

RenderQueueEntry* renderQueueEntryConstruct(void* storage)
{
    RenderQueueEntry* const e = static_cast<RenderQueueEntry*>(storage);
    e->material           = 0;
    e->materialParameters = 0;
    Ra::raLightingParametersConstruct(&e->lighting);
    e->objectParameters   = 0;
    Ra::raLightArrayParametersConstruct(&e->lightArray);
    e->shader             = 0;
    e->techniqueKey       = 0;

    void* const record = ::renddx9abi::engineAlloc(sizeof(Ra::RaLightRecord));
    e->lightArray.lights.value = record ? Ra::raLightRecordConstruct(record) : 0;
    return e;
}


// sub_100D13E0, acquire the next entry.

// Acquires a pooled entry, resetting only lighting.tail10C and dword13C.
// Allocation failure is inserted into the pool and then dereferenced without a guard.

RenderQueueEntry* renderEntryPoolAcquire()
{
    RenderEntryPool* const pool = static_cast<RenderEntryPool*>(g_renderEntryPool);

    const unsigned int size = pool->entries.first
        ? static_cast<unsigned int>(pool->entries.last - pool->entries.first) / sizeof(void*)
        : 0u;
    if (!pool->entries.first || pool->next >= size)
    {
        void* const storage = ::renddx9abi::engineAlloc(sizeof(RenderQueueEntry));
        RenderQueueEntry* const fresh = storage ? renderQueueEntryConstruct(storage) : 0;
        vc71::vectorPushBackIn(pool->entries, sizeof(void*), &fresh);
    }

    RenderQueueEntry** const entries = reinterpret_cast<RenderQueueEntry**>(pool->entries.first);
    RenderQueueEntry* const entry = entries[pool->next];
    entry->lighting.tail10C = 0;
    entry->techniqueKey     = 0;
    ++pool->next;
    return entries[pool->next - 1];
}


// Routes transparency/first-person/delta passes to shared queues; ordinary opaque entries
// use their shader's own queue and require a non-null shader.

void renderQueueEnqueue(RenderQueueEntry* entry, bool transparent, bool firstPerson, bool deltaPass)
{
    const unsigned int index =
        (transparent ? 1u : 0u)
        | (((firstPerson ? 1u : 0u) | ((deltaPass ? 1u : 0u) << 1)) << 1);

    RenderQueue* queue;
    if ((index & ~4u) != 0u)
        queue = static_cast<RenderQueueTable*>(g_renderQueues)->slots[index];
    else
        queue = static_cast<RenderQueue*>(entry->shader->queue());

    queue->add(entry);     // through the queue's vtable slot +0x04 in the shipped code
}

// The StaticMesh draw's `mov ecx, [dword_1023E408+24h/28h]; call [[ecx]+4]` - a fixed table queue
// by index, through its add slot.
void renderQueueAddToSlot(unsigned int slotIndex, RenderQueueEntry* entry)
{
    static_cast<RenderQueueTable*>(g_renderQueues)->slots[slotIndex]->add(entry);
}


// sub_10042CA0 + sub_10042400 - allocate and build the queue table.

void createRenderQueues()
{
    RenderQueueTable* t = static_cast<RenderQueueTable*>(
        ::renddx9abi::engineAlloc(sizeof(RenderQueueTable)));
    g_renderQueues = t;
    if (!t)
        return;

    t->vptr = kRenderQueueTableVtable;
    t->worldQueueOwner = 0;
    t->map88 = 0;
    // sub_10043020 - the 0x30 nil node, isnil, self-linked in all three link slots.
    unsigned char* nil = static_cast<unsigned char*>(::renddx9abi::engineAlloc(0x30));
    if (nil)
    {
        ::memset(nil, 0, 0x30);
        void** links = reinterpret_cast<void**>(nil);
        links[0] = nil;
        links[1] = nil;
        links[2] = nil;
        nil[0x2D] = 1;                 // isnil
    }
    t->mapHead = nil;
    t->mapSize = 0;
    for (unsigned int i = 0; i != 2; ++i)
    {
        t->worldCollections[i].first = 0;
        t->worldCollections[i].last = 0;
        t->worldCollections[i].end = 0;
    }
    t->byteB4 = 1;
    ::memset(t->slots, 0, sizeof(t->slots));

    t->slots[1]  = makeQueue("RSMTTransparency");
    t->slots[2]  = makeQueue("RSMT1Person");
    t->slots[3]  = makeQueue("RSMT1PersonTransparency");
    t->slots[5]  = makeQueue("RSMTDeltaTransparency");
    t->slots[6]  = makeQueue("RSMTDelta1Person");
    t->slots[7]  = makeQueue("RSMTDelta1PersonTransparency");
    t->slots[8]  = makeQueue("RSMTRoad");
    t->slots[10] = makeQueue("RSMTAdditive");
    if (t->slots[10]) queueSortMode(t->slots[10]) = 2;
    t->slots[9]  = makeQueue("RSMTWater");
    t->slots[31] = makeQueue("RSMTGlow");
    if (t->slots[31]) queueSortMode(t->slots[31]) = 2;
    t->slots[11] = makeQueue("RSMTZOnly");
    t->slots[14] = makeQueue("RSMTFogPass");
    if (t->slots[14]) queueSortMode(t->slots[14]) = 1;
    t->slots[15] = makeQueue("RSMTFogTransparentPass");
    if (t->slots[15]) queueSortMode(t->slots[15]) = 1;
    if (t->slots[3]) queueSortMode(t->slots[3]) = 2;
    if (t->slots[1]) queueSortMode(t->slots[1]) = 2;
    if (t->slots[7]) queueSortMode(t->slots[7]) = 2;
    if (t->slots[5]) queueSortMode(t->slots[5]) = 2;
    t->slots[16] = makeQueue("RSMTAux0");
    t->slots[17] = makeQueue("RSMTAux0Transparency");
    t->slots[18] = makeQueue("RSMTAux0Person");
    t->slots[19] = makeQueue("RSMTAux0PersonTransparency");
    t->slots[20] = makeQueue("RSMTAux1");
    t->slots[21] = makeQueue("RSMTAux1Transparency");
    t->slots[22] = makeQueue("RSMTAux1Person");
    t->slots[23] = makeQueue("RSMTAux1PersonTransparency");
    t->slots[24] = makeQueue("RSMTAux2");
    t->slots[25] = makeQueue("RSMTAux2Transparency");
    t->slots[26] = makeQueue("RSMTAux2Person");
    t->slots[27] = makeQueue("RSMTAux2PersonTransparency");

    // The "draw_1p" shared string, handed to the eight first-person-family queues in the shipped
    // order: 1Person, Aux0Person, Delta1Person, Aux1Person, 1PersonTransparency,
    // Aux0PersonTransparency, Delta1PersonTransparency, Aux1PersonTransparency.
    giveSharedString(t->slots[2]);
    giveSharedString(t->slots[18]);
    giveSharedString(t->slots[6]);
    giveSharedString(t->slots[22]);
    giveSharedString(t->slots[3]);
    giveSharedString(t->slots[19]);
    giveSharedString(t->slots[7]);
    giveSharedString(t->slots[23]);

    ::renddx9abi::logf("RenderQueues: table at 0x%08X - 22 queues built, all empty; the draw-call "
                       "stats block at 0x%08X", ::renddx9abi::addr(t),
                       ::renddx9abi::addr(g_renderEntryPool));
}

// The per-frame flush seam renderFrame uses: select the queue at the shipped table offset
// (+0x08 Transparency ... +0x80 Glow), then call its concrete owner method. flushArg < 0 selects
// flush(), otherwise flushWithArg(arg); no positional vtable dispatch escapes this owner TU.
void renderQueueFlush(unsigned int slotByteOffset, int flushArg)
{
    RenderQueueTable* t = static_cast<RenderQueueTable*>(g_renderQueues);
    if (!t)
        return;
    RenderQueue* q = *reinterpret_cast<RenderQueue**>(
        reinterpret_cast<unsigned char*>(t) + slotByteOffset);
    if (!q)
        return;
    if (flushArg < 0)
        q->flush();
    else
        q->flushWithArg(static_cast<unsigned char>(flushArg));
}

// sub_1003AD80 and its sub_1003AD30 helper. The two +0x94/+0xA4 collections flush only queues
// that are enabled and currently non-empty. The optional +0x84 owner then dispatches through an
// unidentified embedded subobject, so that genuinely unknown arm remains an actual-use frontier.
// Recovered RenderQueue calls stay inside their owner instead of hand-indexing vtables.
void RenderQueueTable::flushWorldPass()
{
    for (unsigned int collectionIndex = 0; collectionIndex != 2; ++collectionIndex)
    {
        RenderQueueCollection& collection = worldCollections[collectionIndex];
        for (RenderQueue** it = collection.first; it != collection.last; ++it)
        {
            RenderQueue* const queue = *it;
            if (queue->enabled && queue->entriesFirst
                && queue->entriesLast != queue->entriesFirst)
                queue->flushWithArg(1);
        }
    }


    // The default shader's queue flush is unconditional once the shader exists.

    if (worldQueueOwner)
    {
        RenderQueue* const ownerQueue =
            *reinterpret_cast<RenderQueue**>(static_cast<unsigned char*>(worldQueueOwner) + 4);

        ownerQueue->flushWithArg(1);
    }
}

void renderQueuesFlushWorldPass()
{
    static_cast<RenderQueueTable*>(g_renderQueues)->flushWorldPass();
}

namespace {

struct HemiSmallTreeNode
{
    HemiSmallTreeNode* left;
    HemiSmallTreeNode* parent;
    HemiSmallTreeNode* right;
    unsigned int value;
    unsigned int value2;
    unsigned char color;
    unsigned char isNil;
    unsigned char pad[2];
};

static_assert(sizeof(HemiSmallTreeNode) == 0x18,
              "RenderQueue effect-state nodes are 0x18 bytes");

void destroyHemiSmallTree(HemiSmallTreeNode* node)
{
    // sub_1004CAB0: recurse right, then walk/free the left spine. Values are scalar and have no
    // destructor; only the nodes are returned to the engine heap.
    while (!node->isNil)
    {
        destroyHemiSmallTree(node->right);
        HemiSmallTreeNode* const left = node->left;
        ::renddx9abi::engineFree(node);
        node = left;
    }
}

void resetOneHemiMaterialQueue(void* queue)
{
    unsigned char* const raw = static_cast<unsigned char*>(queue);
    void*& effect = *reinterpret_cast<void**>(raw + 0x16C);
    raw[0x170] = 0;
    if (effect)
    {
        // Retail deliberately zeros the pointer without Release after OnLostDevice; this cache
        // does not own an additional COM reference here.
        static_cast<ID3DXEffect*>(effect)->OnLostDevice();           // +0x114
        effect = 0;
    }

    HemiSmallTreeNode*& head = *reinterpret_cast<HemiSmallTreeNode**>(raw + 0x15C);
    destroyHemiSmallTree(head->parent);
    head->parent = head;
    head->left = head;
    head->right = head;
    *reinterpret_cast<unsigned int*>(raw + 0x160) = 0;
}

} // anonymous namespace

// sub_10041B90, called by EffectManager's channel reset. The table's +0x88 VC7.1 map owns the
// material queues at node+0x28; the shipped body visits them in-order and resets each one.
void renderQueuesResetEffects()
{
    RenderQueueTable* const table = static_cast<RenderQueueTable*>(g_renderQueues);
    vc71::map_node* const head = static_cast<vc71::map_node*>(table->mapHead);
    for (vc71::map_node* node = head->left; node != head;
         node = vc71::map_layout::next(node))
        resetOneHemiMaterialQueue(vc71::mapNodeValue(node));
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
