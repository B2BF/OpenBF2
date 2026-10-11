// [TEMP] [CHOSEN, not recovered] Focused x86 RendDX9 LightManager/XpLight runtime fixture.
// Production bodies are linked from Geom/XpLightManager.cpp.  The records below model only the
// instruction-proven PE fields and virtual entries consumed by that translation unit; they are
// deliberately not engine-side C++ LightSource/ObjectManager implementations.

#include "../BF2/Geom/XpLightManager.h"
#include "../BF2/Game/Main/IVideoOptions.h"
#include "../BF2/Math/Frustum.h"
#include "../RendDX9Abi/Frontier.h"
#include "../RendDX9Abi/Vc71Map.h"
#include "../RendDX9Abi/Vc71Vector.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// Harness-only TU inclusion exposes the coordinator's anonymous-namespace state for exact
// assertions without adding observers or test conditionals to production.
#include "../BF2/Geom/XpLightManager.cpp"

// [TEMP] The production LightManager now contains the direct `init` debug-mesh builder.  Existing
// focused tests do not call that D3DX path, but its imported symbols are retained by the manager
// vtable/object file.  Keep these harness-only refusal bodies explicit rather than pulling the
// retail D3DX DLL into the deterministic fixture.
extern "C" HRESULT WINAPI D3DXCreateSphere(
    LPDIRECT3DDEVICE9, FLOAT, UINT, UINT, LPD3DXMESH*, LPD3DXBUFFER*)
{
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3DXCreateCylinder(
    LPDIRECT3DDEVICE9, FLOAT, FLOAT, FLOAT, UINT, UINT, LPD3DXMESH*, LPD3DXBUFFER*)
{
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3DXCreateBox(
    LPDIRECT3DDEVICE9, FLOAT, FLOAT, FLOAT, LPD3DXMESH*, LPD3DXBUFFER*)
{
    return E_NOTIMPL;
}

// [TEMP] [CHOSEN, not recovered]
// Shadow-rendering link stubs report and abort if reached; ordinary checks do not render shadow maps.
// The overgrowth, static-mesh, and terrain globals remain null. Tests driving quality 3 must provide them.

namespace {
__declspec(noreturn) void g3FixtureRefusal(const char* name)
{
    std::fprintf(stderr, "[FRONTIER] fixture %s reached (G3 link stub)\n", name);
    std::fflush(stderr);
    std::abort();
}
}

extern "C" D3DXMATRIX* WINAPI D3DXMatrixLookAtLH(
    D3DXMATRIX*, CONST D3DXVECTOR3*, CONST D3DXVECTOR3*, CONST D3DXVECTOR3*)
{
    g3FixtureRefusal("D3DXMatrixLookAtLH");
}

extern "C" D3DXMATRIX* WINAPI D3DXMatrixOrthoLH(D3DXMATRIX*, FLOAT, FLOAT, FLOAT, FLOAT)
{
    g3FixtureRefusal("D3DXMatrixOrthoLH");
}

namespace dice { namespace hfe {
namespace dx9rend {
void* g_OverGrowth = 0;
bool effectSetTechniqueByName(void*, const vc71::string&)
{
    g3FixtureRefusal("dice::hfe::dx9rend::effectSetTechniqueByName");
}
}
namespace geom {
IBase* g_StaticMeshRenderer = 0;
IBase* g_Terrain = 0;
void meshBoundsFinalize(MeshBounds&)
{
    g3FixtureRefusal("dice::hfe::geom::meshBoundsFinalize");
}
}
} }

// The focused fixture does not link ModuleGeomMisc's renderer service publication.  Supply the
// image-local PE global consumed only by the recovered Spot-culler debug slot; covered paths do
// not invoke that slot.
namespace dice { namespace hfe { namespace geom {
IBase* g_DebugRenderer = 0;

// [TEMP] [CHOSEN, not recovered] The fixture does not link StaticMesh or exercise the
// shadow predicate's root walk. Keep that dependency fatal if a later test reaches it.
void* staticMeshRootParent(void*)
{
    std::fprintf(stderr, "[FRONTIER] fixture dice::hfe::geom::staticMeshRootParent reached\n");
    std::fflush(stderr);
    std::abort();
}
} } }

using dice::hfe::Frustum;
using dice::hfe::IBase;
using dice::hfe::Mat4;
using dice::hfe::Vec3;
using dice::hfe::geom::LightManager;

namespace dice { namespace hfe { namespace geom { extern LightManager* g_LightManager; } } }

namespace {

int g_checks;
int g_failures;
int g_frontierCalls;
int g_engineAllocations;
int g_engineFrees;

enum LifecycleEvent
{
    LifecycleRegisterDevice,
    LifecycleRegisterRenderPath,
    LifecycleUnregisterDevice,
    LifecycleUnregisterRenderPath
};

std::vector<LifecycleEvent> g_lifecycleEvents;
void* g_deviceListener;
void* g_renderPathListener;
int g_deviceListenerPriority = -1;
int g_deviceRegistrations;
int g_deviceUnregistrations;
int g_renderPathRegistrations;
int g_renderPathUnregistrations;

int g_deviceAddRefs;
int g_deviceReleases;
int g_textureCreates;
int g_textureDestroys;
int g_textureAddRefs;
int g_textureReleaseCalls;
int g_textureLocks;
int g_textureUnlocks;
int g_surfaceCreates;
int g_surfaceDestroys;
int g_surfaceAddRefs;
int g_surfaceReleases;
int g_surfaceCopies;
int g_managedTextureMode = 1;
int g_managedTextureSwaps;
int g_raTextureBinds;
int g_statePushes;
int g_statePops;
int g_renderStateSets;
int g_renderTargetSets;
int g_depthSurfaceSets;
int g_stateClears;
int g_renderTargetFlushes;
int g_effectCreates;
int g_effectDestroys;
int g_effectLookups;
int g_parameterCreates;
int g_parameterDestroys;
int g_parameterFloatSets;
int g_nextEffectIdentity;
std::vector<int> g_effectReferenceEvents;

struct FakeHemiMapManager : public dice::hfe::geom::IHemiMapManager
{
    float bias;
    int setCalls;
    int getCalls;

    FakeHemiMapManager() : bias(0.0f), setCalls(0), getCalls(0) {}

    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return 0; }
    void clear() override {}
    void setBaseHemiMap(const dice::hfe::geom::HemiMapName&, const Vec3&,
                        float, float) override {}
    dice::hfe::geom::HemiMap* getBaseHemiMap() const override { return 0; }
    const dice::hfe::geom::HemiMapInfo& getHemiMapInfo(
        const dice::hfe::geom::HemiMap*) override
    {
        static const dice::hfe::geom::HemiMapInfo info = { 0, 0, 0, 0 };
        return info;
    }
    bool slot20(unsigned int, unsigned int, unsigned int, unsigned int) override
    { return false; }
    bool slot24(unsigned int) override { return false; }
    dice::hfe::geom::HemiMap* slot28(unsigned int) override { return 0; }
    void setHemiLerpBias(float value) override
    {
        bias = value;
        ++setCalls;
    }
    float getHemiLerpBias() const override
    {
        ++const_cast<FakeHemiMapManager*>(this)->getCalls;
        return bias;
    }
    bool makeHemiMap(unsigned long, dice::hfe::geom::HemiMapName,
                     Vec3, float) override { return false; }
};

std::vector<std::string> g_loadedTextureNames;
std::vector<std::string> g_parameterNames;

static const unsigned int kTemplateVtableBytes = 0x1FCu;
static const unsigned int kLightVtableBytes = 0x1CCu;

void check(bool condition, const char* label)
{
    ++g_checks;
    if (!condition)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", label);
    }
}

template <typename T>
T& rawField(void* object, unsigned int offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset);
}

template <typename T>
void destroyFixtureObject(T* object)
{
    object->~T();
}

enum FakeComKind
{
    FakeComDevice,
    FakeComBaseTexture,
    FakeComSurface
};

struct FakeComObject
{
    void** table;
    ULONG references;
    FakeComKind kind;
    bool destroyAtZero;
};

struct FakeTextureState
{
    unsigned int width;
    unsigned int height;
    unsigned int format;
    bool cube;
    std::vector<unsigned int> pixels[6];
};

struct FakeSurface : FakeComObject
{
    FakeTextureState* texture;
    unsigned int face;
};

void* g_fakeComTable[30];
FakeComObject g_fakeDevice;
std::map<dice::hfe::dx9rend::EngineTexture*, FakeTextureState*> g_textureStates;

struct FakeEffectObject;
void __fastcall fakeEffectAddRef(FakeEffectObject* self, void*);
int __fastcall fakeEffectRelease(FakeEffectObject* self, void*);
void __fastcall fakeParameterSetFloat(FakeEffectObject* self, void*, float value);
void* g_effectTable[10];

HRESULT __stdcall fakeComQueryInterface(FakeComObject*, REFIID, void** output)
{
    if (output)
        *output = 0;
    return E_NOINTERFACE;
}

ULONG __stdcall fakeComAddRef(FakeComObject* self)
{
    if (self->kind == FakeComDevice)
        ++g_deviceAddRefs;
    else if (self->kind == FakeComSurface)
        ++g_surfaceAddRefs;
    return ++self->references;
}

ULONG __stdcall fakeComRelease(FakeComObject* self)
{
    if (self->kind == FakeComDevice)
        ++g_deviceReleases;
    else if (self->kind == FakeComSurface)
    {
        ++g_surfaceReleases;
        if (self->references == 1)
            ++g_surfaceDestroys;
    }

    const ULONG remaining = --self->references;
    if (!remaining && self->destroyAtZero)
        std::free(self);
    return remaining;
}

HRESULT __stdcall fakeCreateDepthStencilSurface(
    FakeComObject*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL,
    IDirect3DSurface9** output, HANDLE*)
{
    FakeSurface* const surface = static_cast<FakeSurface*>(std::calloc(1, sizeof(FakeSurface)));
    surface->table = g_fakeComTable;
    surface->references = 1;
    surface->kind = FakeComSurface;
    surface->destroyAtZero = true;
    ++g_surfaceCreates;
    *output = reinterpret_cast<IDirect3DSurface9*>(surface);
    return D3D_OK;
}

HRESULT __stdcall fakeSurfaceLockRect(
    FakeSurface* self, D3DLOCKED_RECT* locked, const RECT*, DWORD)
{
    if (!self->texture || !locked)
        return D3DERR_INVALIDCALL;
    locked->Pitch = static_cast<int>(self->texture->width * sizeof(unsigned int));
    locked->pBits = &self->texture->pixels[self->face][0];
    return D3D_OK;
}

HRESULT __stdcall fakeSurfaceUnlockRect(FakeSurface*)
{
    return D3D_OK;
}

void initializeFakeCom()
{
    std::memset(g_fakeComTable, 0, sizeof(g_fakeComTable));
    g_fakeComTable[0] = reinterpret_cast<void*>(&fakeComQueryInterface);
    g_fakeComTable[1] = reinterpret_cast<void*>(&fakeComAddRef);
    g_fakeComTable[2] = reinterpret_cast<void*>(&fakeComRelease);
    g_fakeComTable[13] = reinterpret_cast<void*>(&fakeSurfaceLockRect);
    g_fakeComTable[14] = reinterpret_cast<void*>(&fakeSurfaceUnlockRect);
    g_fakeComTable[29] = reinterpret_cast<void*>(&fakeCreateDepthStencilSurface);
    g_fakeDevice.table = g_fakeComTable;
    g_fakeDevice.references = 1;
    g_fakeDevice.kind = FakeComDevice;
    g_fakeDevice.destroyAtZero = false;

    std::memset(g_effectTable, 0, sizeof(g_effectTable));
    g_effectTable[0] = reinterpret_cast<void*>(&fakeEffectAddRef);
    g_effectTable[2] = reinterpret_cast<void*>(&fakeEffectRelease);
    g_effectTable[0x24u / 4u] = reinterpret_cast<void*>(&fakeParameterSetFloat);
}

dice::hfe::dx9rend::EngineTexture* createFakeTexture(
    unsigned int width, unsigned int height, unsigned int format, bool cube)
{
    using dice::hfe::dx9rend::EngineTexture;
    EngineTexture* const texture = static_cast<EngineTexture*>(
        std::calloc(1, sizeof(EngineTexture)));
    FakeComObject* const base = static_cast<FakeComObject*>(
        std::calloc(1, sizeof(FakeComObject)));
    base->table = g_fakeComTable;
    base->references = 1;
    base->kind = FakeComBaseTexture;
    base->destroyAtZero = true;

    FakeTextureState* const state = new FakeTextureState;
    state->width = width;
    state->height = height;
    state->format = format;
    state->cube = cube;
    const unsigned int faces = cube ? 6u : 1u;
    for (unsigned int face = 0; face < faces; ++face)
        state->pixels[face].assign(width * height, 0u);

    texture->refCount = 1;
    texture->baseTexture = reinterpret_cast<IDirect3DBaseTexture9*>(base);
    texture->width = width;
    texture->height = height;
    texture->depth = 1;
    texture->mipLevels = 1;
    texture->format = format;
    texture->resourceType = cube ? D3DRTYPE_CUBETEXTURE : D3DRTYPE_TEXTURE;
    texture->face = 0x7FFFFFFFu;
    g_textureStates.insert(std::make_pair(texture, state));
    ++g_textureCreates;
    return texture;
}

FakeTextureState* textureState(dice::hfe::dx9rend::EngineTexture* texture)
{
    std::map<dice::hfe::dx9rend::EngineTexture*, FakeTextureState*>::iterator found =
        g_textureStates.find(texture);
    return found == g_textureStates.end() ? 0 : found->second;
}

struct FakeEffectObject
{
    void** table;
    int references;
    bool parameter;
    float value;
    int identity;
    int addRefCalls;
    int releaseCalls;
};

void __fastcall fakeEffectAddRef(FakeEffectObject* self, void*)
{
    ++self->references;
    ++self->addRefCalls;
    g_effectReferenceEvents.push_back(self->identity);
}

int __fastcall fakeEffectRelease(FakeEffectObject* self, void*)
{
    ++self->releaseCalls;
    g_effectReferenceEvents.push_back(-self->identity);
    const int remaining = --self->references;
    if (!remaining)
    {
        if (self->parameter)
            ++g_parameterDestroys;
        else
            ++g_effectDestroys;
        std::free(self);
    }
    return remaining;
}

void __fastcall fakeParameterSetFloat(FakeEffectObject* self, void*, float value)
{
    self->value = value;
    ++g_parameterFloatSets;
}

FakeEffectObject* createFakeEffectObject(bool parameter)
{
    FakeEffectObject* const object = static_cast<FakeEffectObject*>(
        std::calloc(1, sizeof(FakeEffectObject)));
    object->table = g_effectTable;
    object->references = 1;
    object->parameter = parameter;
    object->identity = ++g_nextEffectIdentity;
    if (parameter)
        ++g_parameterCreates;
    else
        ++g_effectCreates;
    return object;
}

struct FakeLightTemplate
{
    void** table;
    unsigned int flags;
    int type;
    bool dynamic;
    bool enabled;
    float spotCone;
    mutable int flagsCalls;
    mutable int typeCalls;
    mutable int dynamicCalls;
    mutable int enabledCalls;
    mutable int spotConeCalls;
};

unsigned int __fastcall fakeTemplateFlags(const FakeLightTemplate* self, void*)
{
    ++self->flagsCalls;
    return self->flags;
}

int __fastcall fakeTemplateType(const FakeLightTemplate* self, void*)
{
    ++self->typeCalls;
    return self->type;
}

bool __fastcall fakeTemplateDynamic(const FakeLightTemplate* self, void*)
{
    ++self->dynamicCalls;
    return self->dynamic;
}

bool __fastcall fakeTemplateEnabled(const FakeLightTemplate* self, void*)
{
    ++self->enabledCalls;
    return self->enabled;
}

float __fastcall fakeTemplateSpotCone(const FakeLightTemplate* self, void*)
{
    ++self->spotConeCalls;
    return self->spotCone;
}

void initializeTemplate(FakeLightTemplate& object, void** table, int type,
                        bool dynamic, bool enabled, unsigned int flags)
{
    std::memset(table, 0, kTemplateVtableBytes);
    table[0x44u / 4u] = reinterpret_cast<void*>(&fakeTemplateFlags);
    table[0xE8u / 4u] = reinterpret_cast<void*>(&fakeTemplateType);
    table[0xF0u / 4u] = reinterpret_cast<void*>(&fakeTemplateDynamic);
    table[0x178u / 4u] = reinterpret_cast<void*>(&fakeTemplateSpotCone);
    table[0x1F8u / 4u] = reinterpret_cast<void*>(&fakeTemplateEnabled);
    object.table = table;
    object.flags = flags;
    object.type = type;
    object.dynamic = dynamic;
    object.enabled = enabled;
    object.spotCone = 0.75f;
    object.flagsCalls = object.typeCalls = object.dynamicCalls = object.enabledCalls = 0;
    object.spotConeCalls = 0;
}

// BF2.exe PE LightSource has its template pointer at +0x144.  Test-only counters live beyond the
// recovered prefix and are reached solely by the fixture callbacks.
struct FakeLight
{
    void** table;                         // +0x000
    unsigned char prefix[0x140];          // +0x004..+0x143
    FakeLightTemplate* objectTemplate;    // +0x144
    int references;
    int addRefCalls;
    int releaseCalls;
    int positionCalls;
    int radiusCalls;
    int transformCalls;
    int directionCalls;
    int attenuationCalls;
    int colorCalls;
    int specularColorCalls;
    int setDirectionCalls;
    int setColorCalls;
    int setSpecularColorCalls;
    int updateFlagsCalls;
    int innerConeCalls;
    int outerConeCalls;
    int setEnabledCalls;
    int getEnabledCalls;
    Vec3 position;
    Vec3 direction;
    float radius;
    Mat4 transform;
    float attenuation;
    Vec3 color;
    Vec3 specularColor;
    float innerCone;
    float outerCone;
    bool sourceEnabled;
};

static_assert(offsetof(FakeLight, objectTemplate) == 0x144,
              "fake LightSource must preserve the PE template field");

void __fastcall fakeLightAddRef(FakeLight* self, void*)
{
    ++self->addRefCalls;
    ++self->references;
}

int __fastcall fakeLightRelease(FakeLight* self, void*)
{
    ++self->releaseCalls;
    return --self->references;
}

const Vec3* __fastcall fakeLightPosition(FakeLight* self, void*)
{
    ++self->positionCalls;
    return &self->position;
}

float __fastcall fakeLightRadius(FakeLight* self, void*)
{
    ++self->radiusCalls;
    return self->radius;
}

const Mat4* __fastcall fakeLightTransform(FakeLight* self, void*)
{
    ++self->transformCalls;
    return &self->transform;
}

const Vec3* __fastcall fakeLightDirection(FakeLight* self, void*)
{
    ++self->directionCalls;
    return &self->direction;
}

void __fastcall fakeLightSetDirection(FakeLight* self, void*, const Vec3* value)
{
    ++self->setDirectionCalls;
    self->direction = *value;
}

float __fastcall fakeLightAttenuation(FakeLight* self, void*)
{
    ++self->attenuationCalls;
    return self->attenuation;
}

const Vec3* __fastcall fakeLightColor(FakeLight* self, void*)
{
    ++self->colorCalls;
    return &self->color;
}

void __fastcall fakeLightSetColor(FakeLight* self, void*, const Vec3* value)
{
    ++self->setColorCalls;
    self->color = *value;
}

const Vec3* __fastcall fakeLightSpecularColor(FakeLight* self, void*)
{
    ++self->specularColorCalls;
    return &self->specularColor;
}

void __fastcall fakeLightSetSpecularColor(FakeLight* self, void*, const Vec3* value)
{
    ++self->setSpecularColorCalls;
    self->specularColor = *value;
}

void __fastcall fakeLightUpdateFlags(FakeLight* self, void*, unsigned int set,
                                     unsigned int clear)
{
    ++self->updateFlagsCalls;
    unsigned int& flags = rawField<unsigned int>(self, 0x04u);
    flags = (flags | set) & ~clear;
}

float __fastcall fakeLightInnerCone(FakeLight* self, void*)
{
    ++self->innerConeCalls;
    return self->innerCone;
}

float __fastcall fakeLightOuterCone(FakeLight* self, void*)
{
    ++self->outerConeCalls;
    return self->outerCone;
}

void __fastcall fakeLightSetEnabled(FakeLight* self, void*, bool enabled)
{
    ++self->setEnabledCalls;
    self->sourceEnabled = enabled;
}

bool __fastcall fakeLightGetEnabled(FakeLight* self, void*)
{
    ++self->getEnabledCalls;
    return self->sourceEnabled;
}

void initializeLight(FakeLight& light, void** table, FakeLightTemplate& objectTemplate,
                     const Vec3& position, float radius, int references = 10,
                     unsigned int objectFlags = 0, void* geometry = 0)
{
    std::memset(table, 0, kLightVtableBytes);
    table[0x00u / 4u] = reinterpret_cast<void*>(&fakeLightAddRef);
    table[0x08u / 4u] = reinterpret_cast<void*>(&fakeLightRelease);
    table[0x68u / 4u] = reinterpret_cast<void*>(&fakeLightPosition);
    table[0x80u / 4u] = reinterpret_cast<void*>(&fakeLightTransform);
    table[0x90u / 4u] = reinterpret_cast<void*>(&fakeLightRadius);
    table[0x5Cu / 4u] = reinterpret_cast<void*>(&fakeLightUpdateFlags);
    table[0x178u / 4u] = reinterpret_cast<void*>(&fakeLightSetDirection);
    table[0x17Cu / 4u] = reinterpret_cast<void*>(&fakeLightDirection);
    table[0x190u / 4u] = reinterpret_cast<void*>(&fakeLightAttenuation);
    table[0x194u / 4u] = reinterpret_cast<void*>(&fakeLightSetColor);
    table[0x198u / 4u] = reinterpret_cast<void*>(&fakeLightColor);
    table[0x19Cu / 4u] = reinterpret_cast<void*>(&fakeLightSetSpecularColor);
    table[0x1A0u / 4u] = reinterpret_cast<void*>(&fakeLightSpecularColor);
    table[0x1B0u / 4u] = reinterpret_cast<void*>(&fakeLightInnerCone);
    table[0x1BCu / 4u] = reinterpret_cast<void*>(&fakeLightOuterCone);
    table[0x1C4u / 4u] = reinterpret_cast<void*>(&fakeLightSetEnabled);
    table[0x1C8u / 4u] = reinterpret_cast<void*>(&fakeLightGetEnabled);
    std::memset(&light, 0, sizeof(light));
    light.table = table;
    rawField<unsigned int>(&light, 0x04u) = objectFlags;
    rawField<void*>(&light, 0x24u) = geometry;
    light.objectTemplate = &objectTemplate;
    light.references = references;
    light.position = position;
    light.direction = Vec3{0.0f, -0.7f, -0.7f};
    light.radius = radius;
    light.transform = Mat4::getIdentity();
    light.transform.m[12] = position.x;
    light.transform.m[13] = position.y;
    light.transform.m[14] = position.z;
    light.attenuation = radius;
    light.color = Vec3{1.0f, 0.5f, 0.25f};
    light.specularColor = Vec3{0.25f, 0.5f, 1.0f};
    light.innerCone = 0.5f;
    light.outerCone = 0.75f;
    light.sourceEnabled = true;
}

dice::hfe::world::LightSource* asLight(FakeLight& light)
{
    return reinterpret_cast<dice::hfe::world::LightSource*>(&light);
}

struct FakeRenderView
{
    void** table;
    Mat4 cameraTransform;
    Frustum* frustum;
    int cameraTransformCalls;
    int frustumCalls;
};

Mat4* __fastcall fakeGetCameraTransform(FakeRenderView* self, void*)
{
    ++self->cameraTransformCalls;
    return &self->cameraTransform;
}

Frustum* __fastcall fakeGetFrustum(FakeRenderView* self, void*)
{
    ++self->frustumCalls;
    return self->frustum;
}

FakeRenderView g_renderView;
void* g_renderViewTable[0x78u / 4u];
bool g_expansionPack;
int g_expansionPackCalls;
int g_renderViewCalls;

void initializeRenderView(Frustum& frustum)
{
    std::memset(g_renderViewTable, 0, sizeof(g_renderViewTable));
    g_renderViewTable[0x64u / 4u] = reinterpret_cast<void*>(&fakeGetCameraTransform);
    g_renderViewTable[0x74u / 4u] = reinterpret_cast<void*>(&fakeGetFrustum);
    g_renderView.table = g_renderViewTable;
    g_renderView.cameraTransform = Mat4::getIdentity();
    g_renderView.frustum = &frustum;
    g_renderView.cameraTransformCalls = 0;
    g_renderView.frustumCalls = 0;
    g_renderViewCalls = 0;
}

struct FakeCandidate
{
    void** table;
    unsigned char prefix[0x34];
    void* geometry;                     // +0x38
    int addRefCalls;
    int releaseCalls;
};

static_assert(offsetof(FakeCandidate, geometry) == 0x38,
              "fake IObject must preserve the PE geometry field");

void __fastcall fakeCandidateAddRef(FakeCandidate* self, void*)
{
    ++self->addRefCalls;
}

int __fastcall fakeCandidateRelease(FakeCandidate* self, void*)
{
    ++self->releaseCalls;
    return 1;
}

void initializeCandidate(FakeCandidate& candidate, void** table, void* geometry)
{
    std::memset(table, 0, 0x0Cu);
    table[0x00u / 4u] = reinterpret_cast<void*>(&fakeCandidateAddRef);
    table[0x08u / 4u] = reinterpret_cast<void*>(&fakeCandidateRelease);
    std::memset(&candidate, 0, sizeof(candidate));
    candidate.table = table;
    candidate.geometry = geometry;
}

struct ObjectQuery
{
    enum Kind { FrustumQuery, SphereQuery } kind;
    Vec3 position;
    float radius;
    double time;
};

struct FakeObjectManager
{
    void** table;
    std::vector<void*> frustumCandidates;
    std::vector<void*> sphereCandidates;
    std::vector<ObjectQuery> queries;
};

void appendCandidates(vc71::vector_layout* output, const std::vector<void*>& candidates)
{
    vc71::vectorRef<void*> view(output);
    for (std::vector<void*>::const_iterator it = candidates.begin();
         it != candidates.end(); ++it)
        view.push_back(*it);
}

void __fastcall fakeFrustumQuery(FakeObjectManager* self, void*, const Frustum*,
                                 vc71::vector_layout* output, void*, bool,
                                 float firstDistance, float secondDistance)
{
    ObjectQuery query = {ObjectQuery::FrustumQuery, Vec3{0.0f, 0.0f, 0.0f},
                         firstDistance, secondDistance};
    self->queries.push_back(query);
    appendCandidates(output, self->frustumCandidates);
}

void __fastcall fakeSphereQuery(FakeObjectManager* self, void*, const Vec3* position,
                                float radius, vc71::vector_layout* output, void*)
{
    ObjectQuery query = {ObjectQuery::SphereQuery, *position, radius, 0.0};
    self->queries.push_back(query);
    appendCandidates(output, self->sphereCandidates);
}

FakeObjectManager g_objectManager;
void* g_objectManagerTable[0x48u / 4u];

void initializeObjectManager()
{
    std::memset(g_objectManagerTable, 0, sizeof(g_objectManagerTable));
    g_objectManagerTable[0x40u / 4u] = reinterpret_cast<void*>(&fakeFrustumQuery);
    g_objectManagerTable[0x44u / 4u] = reinterpret_cast<void*>(&fakeSphereQuery);
    g_objectManager.table = g_objectManagerTable;
    g_objectManager.frustumCandidates.clear();
    g_objectManager.sphereCandidates.clear();
    g_objectManager.queries.clear();
}

struct FakePlayer
{
    void** table;
    void* controlledObject;
    int controlledObjectCalls;
};

void* __fastcall fakeControlledObject(FakePlayer* self, void*)
{
    ++self->controlledObjectCalls;
    return self->controlledObject;
}

struct FakePlayerManager
{
    void** table;
    FakePlayer* localPlayer;
    int localPlayerCalls;
};

FakePlayer* __fastcall fakeLocalPlayer(FakePlayerManager* self, void*)
{
    ++self->localPlayerCalls;
    return self->localPlayer;
}

FakePlayer g_localPlayer;
void* g_localPlayerTable[0x48u / 4u];
FakePlayerManager g_playerManager;
void* g_playerManagerTable[0x34u / 4u];

void initializePlayerManager(void* controlledObject)
{
    std::memset(g_localPlayerTable, 0, sizeof(g_localPlayerTable));
    g_localPlayerTable[0x44u / 4u] = reinterpret_cast<void*>(&fakeControlledObject);
    g_localPlayer.table = g_localPlayerTable;
    g_localPlayer.controlledObject = controlledObject;
    g_localPlayer.controlledObjectCalls = 0;

    std::memset(g_playerManagerTable, 0, sizeof(g_playerManagerTable));
    g_playerManagerTable[0x30u / 4u] = reinterpret_cast<void*>(&fakeLocalPlayer);
    g_playerManager.table = g_playerManagerTable;
    g_playerManager.localPlayer = &g_localPlayer;
    g_playerManager.localPlayerCalls = 0;
}

struct FakeVideoOptions
{
    void** table;
    int quality[8];
    int getQualityCalls;
    int lastQualityIndex;
};

int __fastcall fakeGetQuality(FakeVideoOptions* self, void*, int index)
{
    ++self->getQualityCalls;
    self->lastQualityIndex = index;
    return self->quality[index];
}

FakeVideoOptions g_videoOptions;
void* g_videoOptionsTable[0x08u / 4u];

struct FakeRendererBase
{
    void** table;                             // +0x000
    int renderViewCalls;                      // +0x004
    int videoOptionsCalls;                    // +0x008
    IDirect3DDevice9* device;                 // +0x00C
    unsigned char pad010[0x0F4 - 0x010];
    IDirect3DSurface9* primaryRenderTarget;   // +0x0F4
    unsigned char pad0F8[0x76C - 0x0F8];
    IDirect3DSurface9* fallbackRenderTarget;  // +0x76C
};

static_assert(offsetof(FakeRendererBase, device) == 0x0C,
              "fake renderer must preserve the PE device field");
static_assert(offsetof(FakeRendererBase, primaryRenderTarget) == 0x0F4,
              "fake renderer must preserve the PE primary render target field");
static_assert(offsetof(FakeRendererBase, fallbackRenderTarget) == 0x76C,
              "fake renderer must preserve the PE fallback render target field");

void* __fastcall fakeRendererView(FakeRendererBase* self, void*)
{
    ++self->renderViewCalls;
    return &g_renderView;
}

FakeVideoOptions* __fastcall fakeRendererVideoOptions(FakeRendererBase* self, void*)
{
    ++self->videoOptionsCalls;
    return &g_videoOptions;
}

FakeRendererBase g_rendererBaseObject;
void* g_rendererBaseTable[0x78u / 4u];

void initializeRendererBase()
{
    std::memset(g_videoOptionsTable, 0, sizeof(g_videoOptionsTable));
    g_videoOptionsTable[0x04u / 4u] = reinterpret_cast<void*>(&fakeGetQuality);
    g_videoOptions.table = g_videoOptionsTable;
    for (unsigned int i = 0; i < 8; ++i)
        g_videoOptions.quality[i] = 3;
    g_videoOptions.getQualityCalls = 0;
    g_videoOptions.lastQualityIndex = -1;

    std::memset(g_rendererBaseTable, 0, sizeof(g_rendererBaseTable));
    g_rendererBaseTable[0x48u / 4u] = reinterpret_cast<void*>(&fakeRendererView);
    g_rendererBaseTable[0x74u / 4u] = reinterpret_cast<void*>(&fakeRendererVideoOptions);
    g_rendererBaseObject.table = g_rendererBaseTable;
    g_rendererBaseObject.renderViewCalls = 0;
    g_rendererBaseObject.videoOptionsCalls = 0;
    g_rendererBaseObject.device = reinterpret_cast<IDirect3DDevice9*>(&g_fakeDevice);
    if (!g_rendererBaseObject.primaryRenderTarget)
    {
        FakeSurface* const surface = static_cast<FakeSurface*>(
            std::calloc(1, sizeof(FakeSurface)));
        surface->table = g_fakeComTable;
        surface->references = 1;
        surface->kind = FakeComSurface;
        surface->destroyAtZero = true;
        g_rendererBaseObject.primaryRenderTarget =
            reinterpret_cast<IDirect3DSurface9*>(surface);
        ++g_surfaceCreates;
    }
}

vc71::set_layout& ownedLights(LightManager* manager)
{
    return rawField<vc71::set_layout>(manager, 0x1B8u);
}

vc71::set_layout& activeLights(LightManager* manager)
{
    return rawField<vc71::set_layout>(manager, 0x1C4u);
}

vc71::vector_layout& visibleLights(LightManager* manager)
{
    return rawField<vc71::vector_layout>(manager, 0x1D0u);
}

unsigned int visibleCount(LightManager* manager)
{
    vc71::vector_layout& values = visibleLights(manager);
    return values.first
        ? static_cast<unsigned int>((values.last - values.first) /
                                    sizeof(dice::hfe::world::LightSource*))
        : 0u;
}

dice::hfe::world::LightSource* visibleAt(LightManager* manager, unsigned int index)
{
    return reinterpret_cast<dice::hfe::world::LightSource**>(
        visibleLights(manager).first)[index];
}

LightManager* createManager()
{
    IBase* const base = dice::hfe::geom::createLightManager(
        dice::hfe::geom::IID_ILightManager, 0);
    return dice::hfe::geom::asLightManager(base);
}

int releaseManager(LightManager* manager)
{
    return manager->release();
}

void registerShadowEffect(LightManager* manager, int reflectionTypeId,
                          FakeEffectObject* effect)
{
    manager->registerShadowEffect(
        reflectionTypeId, reinterpret_cast<dice::hfe::dx9rend::Effect*>(effect));
}

dice::hfe::geom::XpLight* registeredXp(FakeLight& light)
{
    std::map<dice::hfe::world::LightSource*, dice::hfe::geom::XpLight*>::iterator found =
        dice::hfe::geom::g_xpCoordinator.bySource.find(asLight(light));
    return found == dice::hfe::geom::g_xpCoordinator.bySource.end() ? 0 : found->second;
}

unsigned int xpCandidateCount(const dice::hfe::geom::XpLight* light)
{
    return dice::hfe::geom::vectorSize<dice::hfe::world::IObject*>(light->candidates);
}

void setLightPosition(FakeLight& light, const Vec3& position)
{
    light.position = position;
    light.transform.m[12] = position.x;
    light.transform.m[13] = position.y;
    light.transform.m[14] = position.z;
}

bool closeFloat(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 0.0001f;
}

void invokeDeviceListener(unsigned int slot)
{
    typedef void (__thiscall *ListenerFn)(void*);
    void** const table = *reinterpret_cast<void***>(g_deviceListener);
    reinterpret_cast<ListenerFn>(table[slot])(g_deviceListener);
}

void invokeRenderPathListener(int path)
{
    typedef void (__thiscall *ActivatedFn)(void*, int);
    void** const table = *reinterpret_cast<void***>(g_renderPathListener);
    reinterpret_cast<ActivatedFn>(table[0])(g_renderPathListener, path);
}

struct SimpleShadowQuery
{
    int x;
    int z;
    bool io;
};
std::vector<SimpleShadowQuery> g_simpleShadowQueries;

float __fastcall recordSimpleShadowTexel(void* self, void*, int x, int z)
{
    g_simpleShadowQueries.push_back({ x, z, false });
    return dice::hfe::geom::lm_sampleSimpleShadowMapTexel(self, 0, x, z);
}
float __fastcall recordSimpleShadowIOTexel(void* self, void*, int x, int z)
{
    g_simpleShadowQueries.push_back({ x, z, true });
    return dice::hfe::geom::lm_sampleSimpleShadowMapIOTexel(self, 0, x, z);
}

void testSimpleShadowSamplerAbi()
{
    // [TEMP] Only the instruction-read CPU map/settings fields are modeled here. Authored packed
    // bytes cover texel centres around zero; no device, loader or fabricated map fallback is used.
    alignas(unsigned int) unsigned char storage[0x980] = {};
    LightManager* const manager = reinterpret_cast<LightManager*>(storage);
    *reinterpret_cast<const void* const**>(storage) = dice::hfe::geom::kPrimaryVtable;
    unsigned char packed[16] = {
        0xE4, 0xE4, 0xE4, 0xE4, 0xE4, 0xE4, 0x00, 0x01,
        0x80, 0x03, 0xE4, 0xE4, 0xE4, 0xE4, 0xE4, 0xE4
    };
    unsigned int ioWords[2] = { 0x90000000u, 0x00000009u };
    rawField<unsigned char*>(manager, 0x190) = packed;
    rawField<unsigned int*>(manager, 0x184) = ioWords;
    rawField<float>(manager, 0x194) = 6.0f;
    rawField<unsigned int>(manager, 0x198) = 8;
    rawField<unsigned int>(manager, 0x19C) = 4;
    rawField<unsigned int>(manager, 0x1A4) = 4;
    rawField<unsigned int>(manager, 0x1A8) = 2;
    rawField<unsigned int>(manager, 0x1AC) = 1;
    rawField<unsigned int>(manager, 0x1B0) = 4;
    void** const table = *reinterpret_cast<void***>(manager);
    typedef float (__thiscall *SampleFn)(void*, float, float);
    typedef float (__thiscall *TexelFn)(void*, int, int);
    SampleFn sample = reinterpret_cast<SampleFn>(table[0x2C4 / 4]);
    SampleFn sampleIO = reinterpret_cast<SampleFn>(table[0x2C8 / 4]);
    TexelFn texelIO = reinterpret_cast<TexelFn>(table[0x2CC / 4]);
    TexelFn texel = reinterpret_cast<TexelFn>(table[0x2D0 / 4]);
    check(texel(manager, -4, -4) == 0.0f && closeFloat(texel(manager, -3, -4), 1.0f / 3.0f)
              && closeFloat(texel(manager, -2, -4), 2.0f / 3.0f) && texel(manager, -1, -4) == 1.0f,
          "packed texel ABI decodes four low-first 2-bit levels with signed negative-x byte shifts");
    check(texelIO(manager, 3, -1) == 1.0f && texelIO(manager, -4, 0) == 1.0f
              && texelIO(manager, 0, 0) == 0.0f,
          "IO texel ABI crosses the 31/32 bit-word boundary and preserves zero cells");
    check(sample(manager, -1.0f, -1.0f) == 0.0f && sample(manager, 1.0f, 1.0f) == 1.0f,
          "filtered packed map uses the shipped two-world-unit texel-centre coordinates");
    check(closeFloat(sample(manager, 0.0f, 0.0f), 0.5f)
              && closeFloat(sample(manager, -0.5f, 0.5f), 7.0f / 12.0f),
          "packed bilinear sampler floors negative coordinates and blends both axes");
    check(closeFloat(sampleIO(manager, 0.0f, 0.0f), 0.5f)
              && closeFloat(sampleIO(manager, -0.5f, 0.5f), 0.625f),
          "filtered IO ABI bilinearly blends four independently packed binary cells");
    check(closeFloat(sample(manager, 100.0f, 100.0f), 5.0f / 6.0f)
              && closeFloat(sample(manager, -100.0f, -100.0f), 1.0f / 6.0f),
          "world coordinates clamp to both shipped map limits before cell-centre conversion");

    // Observe the shipped self-virtual boundary while forwarding every query to its real body.
    // Even at a cell centre the filter issues all four calls in lower-row then upper-row order.
    const void* observedTable[sizeof(dice::hfe::geom::kPrimaryVtable) / sizeof(void*)];
    std::memcpy(observedTable, dice::hfe::geom::kPrimaryVtable, sizeof(observedTable));
    observedTable[0x2CC / 4] = reinterpret_cast<const void*>(&recordSimpleShadowIOTexel);
    observedTable[0x2D0 / 4] = reinterpret_cast<const void*>(&recordSimpleShadowTexel);
    *reinterpret_cast<const void* const**>(storage) = observedTable;
    g_simpleShadowQueries.clear();
    sample(manager, -1.0f, -1.0f);
    sampleIO(manager, -1.0f, -1.0f);
    bool ordered = g_simpleShadowQueries.size() == 8;
    for (unsigned int i = 0; ordered && i < 8; ++i)
    {
        ordered = g_simpleShadowQueries[i].x == static_cast<int>(i & 1u) - 1
               && g_simpleShadowQueries[i].z == static_cast<int>((i >> 1) & 1u) - 1
               && g_simpleShadowQueries[i].io == (i >= 4);
    }
    check(ordered, "both filters retain all four ordered self-virtual texel calls at exact centres");
    *reinterpret_cast<const void* const**>(storage) = dice::hfe::geom::kPrimaryVtable;

    typedef void (__thiscall *SetIntFn)(void*, int);
    typedef int (__thiscall *GetIntFn)(void*);
    typedef void (__thiscall *SetFloatFn)(void*, float);
    typedef float (__thiscall *GetFloatFn)(void*);
    typedef void (__thiscall *SetRangeFn)(void*, const dice::hfe::Vec2*);
    typedef const dice::hfe::Vec2* (__thiscall *GetRangeFn)(void*);
    reinterpret_cast<SetIntFn>(table[0x28C / 4])(manager, -2);
    check(reinterpret_cast<GetIntFn>(table[0x290 / 4])(manager) == -2,
          "skinned shadow LOD host pair preserves negative values without invented clamp");
    reinterpret_cast<SetFloatFn>(table[0x2A4 / 4])(manager, 2.75f);
    check(reinterpret_cast<GetFloatFn>(table[0x2A8 / 4])(manager) == 2.75f,
          "shadow sun exponent host pair returns the stored float through ST(0)");
    const dice::hfe::Vec2 fade = { 8.5f, 2.0f };
    reinterpret_cast<SetRangeFn>(table[0x2BC / 4])(manager, &fade);
    const dice::hfe::Vec2* const range = reinterpret_cast<GetRangeFn>(table[0x2C0 / 4])(manager);
    check(range == reinterpret_cast<const dice::hfe::Vec2*>(storage + 0x174)
              && range->x == 8.5f && range->y == 2.0f,
          "height fade host pair uses a borrowed Vec2 reference and preserves reversed endpoints");
}

void testConstructionResourcesAndDestruction()
{
    using dice::hfe::dx9rend::EngineTexture;

    const int deviceAddRefs = g_deviceAddRefs;
    const int deviceReleases = g_deviceReleases;
    const int textureCreates = g_textureCreates;
    const int textureDestroys = g_textureDestroys;
    const int textureLocks = g_textureLocks;
    const int textureUnlocks = g_textureUnlocks;
    const int surfaceCreates = g_surfaceCreates;
    const int surfaceDestroys = g_surfaceDestroys;
    const int deviceRegistrations = g_deviceRegistrations;
    const int deviceUnregistrations = g_deviceUnregistrations;
    const int renderRegistrations = g_renderPathRegistrations;
    const int renderUnregistrations = g_renderPathUnregistrations;
    const unsigned int lifecycleStart = static_cast<unsigned int>(g_lifecycleEvents.size());
    const unsigned int liveTextures = static_cast<unsigned int>(g_textureStates.size());

    LightManager* const manager = createManager();
    check(manager != 0 && rawField<int>(manager, 0x10u) == 1,
          "full LightManager construction publishes the primary interface with refcount one");
    // The live SkinnedMesh failure reached the explicit host table's +0x80 entry. Exercise
    // that exact thiscall/ST(0) boundary and the paired stack-float setter, not only wrappers.
    typedef void (__thiscall *SetShaderLodDistanceFn)(void*, float);
    typedef float (__thiscall *GetShaderLodDistanceFn)(void*);
    void** const primaryTable = *reinterpret_cast<void***>(manager);
    SetShaderLodDistanceFn setShaderLodDistance =
        reinterpret_cast<SetShaderLodDistanceFn>(primaryTable[0x7Cu / 4u]);
    GetShaderLodDistanceFn getShaderLodDistance =
        reinterpret_cast<GetShaderLodDistanceFn>(primaryTable[0x80u / 4u]);
    check(getShaderLodDistance(manager) == 10.0f,
          "host +0x80 returns the shipped 10.0 shader LOD distance through ST(0)");
    setShaderLodDistance(manager, -3.5f);
    check(getShaderLodDistance(manager) == -3.5f &&
              manager->getSkinnedMeshShaderLodDistance() == -3.5f,
          "host shader LOD pair preserves negative values without neighbouring getter clamps");
    manager->setSkinnedMeshShaderLodDistance(18.25f);
    check(getShaderLodDistance(manager) == 18.25f,
          "typed setter and exact host getter share the recovered shader LOD property");
    setShaderLodDistance(manager, 10.0f);

    check(g_deviceListener == static_cast<unsigned char*>(static_cast<void*>(manager)) + 4 &&
              g_renderPathListener == static_cast<unsigned char*>(static_cast<void*>(manager)) + 8 &&
              g_deviceListenerPriority == 4,
          "construction registers the exact +4/+8 secondary interfaces at device priority four");
    check(g_deviceRegistrations == deviceRegistrations + 1 &&
              g_renderPathRegistrations == renderRegistrations + 1 &&
              g_lifecycleEvents[lifecycleStart] == LifecycleRegisterDevice &&
              g_lifecycleEvents[lifecycleStart + 1] == LifecycleRegisterRenderPath,
          "listener registration preserves shipped device-then-render-path order");
    check(g_deviceAddRefs == deviceAddRefs + 1 &&
              rawField<IDirect3DDevice9*>(manager, 0x908u) ==
                  reinterpret_cast<IDirect3DDevice9*>(&g_fakeDevice),
          "construction retains the renderer device exactly once");

    typedef renddx9abi::HostString* (__thiscall *ListenerNameFn)(
        void*, renddx9abi::HostString*);
    alignas(renddx9abi::HostString) unsigned char listenerNameStorage[
        sizeof(renddx9abi::HostString)];
    void* const renderPathListener = static_cast<unsigned char*>(
        static_cast<void*>(manager)) + 8;
    void** const renderPathTable = *static_cast<void***>(renderPathListener);
    renddx9abi::HostString* const listenerName =
        reinterpret_cast<ListenerNameFn>(renderPathTable[5])(
            renderPathListener,
            reinterpret_cast<renddx9abi::HostString*>(listenerNameStorage));
    check(std::strcmp(listenerName->c_str(), "aLightManager") == 0,
          "render-path listener returns the exact shipped aLightManager name");
    destroyFixtureObject(listenerName);

    check(closeFloat(rawField<float>(manager, 0x174u), 3.0f) &&
              closeFloat(rawField<float>(manager, 0x178u), 6.0f) &&
              rawField<unsigned int>(manager, 0x204u) == 1024u &&
              rawField<unsigned int>(manager, 0x208u) == 256u &&
              rawField<unsigned int>(manager, 0x910u) == 0u,
          "constructor materializes the recovered float, atlas and NVIDIA mode defaults");

    vc71::vector_layout& firstShadowRecords =
        rawField<vc71::vector_layout>(manager, 0x79Cu);
    check(firstShadowRecords.first != 0 &&
              firstShadowRecords.last == firstShadowRecords.first &&
              firstShadowRecords.end - firstShadowRecords.first == 150 * 0x498,
          "constructor reserves exactly 150 0x498-byte primary shadow records");
    check(g_textureCreates == textureCreates + 8 &&
              g_textureLocks == textureLocks + 7 &&
              g_textureUnlocks == textureUnlocks + 7 &&
              g_surfaceCopies >= 6,
          "constructor builds four LUTs, three 2D helpers and one six-face cube");

    EngineTexture* const lut = rawField<EngineTexture*>(manager, 0x254u);
    EngineTexture* const white = rawField<EngineTexture*>(manager, 0x23Cu);
    EngineTexture* const black = rawField<EngineTexture*>(manager, 0x240u);
    EngineTexture* const framed = rawField<EngineTexture*>(manager, 0x244u);
    EngineTexture* const cube = rawField<EngineTexture*>(manager, 0x248u);
    FakeTextureState* const lutState = textureState(lut);
    FakeTextureState* const whiteState = textureState(white);
    FakeTextureState* const blackState = textureState(black);
    FakeTextureState* const framedState = textureState(framed);
    FakeTextureState* const cubeState = textureState(cube);
    check(lutState && lutState->pixels[0].front() == 0xFF000000u &&
              lutState->pixels[0].back() == 0xFFFFFFFFu,
          "power LUT spans opaque black through opaque white");
    check(whiteState && blackState && framedState && cubeState &&
              whiteState->pixels[0][0] == 0xFFFFFFFFu &&
              blackState->pixels[0][0] == 0u &&
              framedState->pixels[0][0] == 0xFFFFFFFFu &&
              framedState->pixels[0][3 * 8 + 3] == 0xFF000000u &&
              cubeState->cube && cubeState->pixels[5][0] == 0u,
          "small helper textures preserve white, black, framed and copied-cube contents");

    const int shadowTextureCreates = g_textureCreates;
    const int shadowManagedSwaps = g_managedTextureSwaps;
    const int shadowRaBinds = g_raTextureBinds;
    check(dice::hfe::geom::createShadowResources(manager),
          "NVIDIA shadow-resource construction succeeds through the recovered mode-zero branch");
    check(g_textureCreates == shadowTextureCreates + 4 &&
              rawField<EngineTexture*>(manager, 0x234u) ==
                  rawField<EngineTexture*>(manager, 0x224u) &&
              g_textureAddRefs > 0,
          "equal-sized mode-zero resources alias and retain the colour render target");
    check(g_managedTextureSwaps == shadowManagedSwaps + 2 &&
              g_managedTextureMode == 1 && g_raTextureBinds == shadowRaBinds + 4,
          "shadow creation brackets managed mode and publishes four shader texture holders");
    RECT& shadowRect = rawField<RECT>(manager, 0x78Cu);
    check(shadowRect.left == 3 && shadowRect.top == 3 &&
              shadowRect.right == 1021 && shadowRect.bottom == 1021,
          "shadow resource creation records the requested atlas inset by three pixels");

    invokeDeviceListener(0);
    bool lostClearedAll = true;
    for (unsigned int offset = 0x21Cu; offset <= 0x238u; offset += 4)
        lostClearedAll = lostClearedAll && rawField<void*>(manager, offset) == 0;
    check(lostClearedAll,
          "device-lost callback releases and nulls every shadow resource owner");
    invokeDeviceListener(1);
    check(rawField<void*>(manager, 0x21Cu) != 0 &&
              rawField<void*>(manager, 0x230u) != 0 &&
              rawField<void*>(manager, 0x238u) != 0,
          "device-reset callback reconstructs primary and secondary shadow resources");

    const int effectLookups = g_effectLookups;
    const int parameterCreates = g_parameterCreates;
    const int parameterFloatSets = g_parameterFloatSets;
    const unsigned int loadedNames = static_cast<unsigned int>(g_loadedTextureNames.size());
    invokeRenderPathListener(2);
    check(g_effectLookups == effectLookups + 1 &&
              g_parameterCreates == parameterCreates + 28 &&
              g_parameterFloatSets == parameterFloatSets + 2,
          "render-path two loads one effect, its 28 semantics and both bias defaults");
    check(g_loadedTextureNames.size() == loadedNames + 2 &&
              g_loadedTextureNames[loadedNames] == "Common/Textures/RadialAtt2" &&
              g_loadedTextureNames[loadedNames + 1] == "Common/Textures/ConicalAtt",
          "render-path activation loads radial then conical attenuation textures");

    check(releaseManager(manager) == 0,
          "full manager release runs the recovered lifecycle destructor");
    check(g_deviceUnregistrations == deviceUnregistrations + 1 &&
              g_renderPathUnregistrations == renderUnregistrations + 1 &&
              g_lifecycleEvents[lifecycleStart + 2] == LifecycleUnregisterDevice &&
              g_lifecycleEvents[lifecycleStart + 3] == LifecycleUnregisterRenderPath,
          "destruction unregisters device before render-path listener");
    check(g_deviceReleases == deviceReleases + 1 &&
              g_fakeDevice.references == 1,
          "destruction releases its single device ownership reference");
    check(g_textureCreates - textureCreates == g_textureDestroys - textureDestroys &&
              g_textureStates.size() == liveTextures,
          "destruction releases constructor, shadow and render-path textures without residue");
    check(g_surfaceCreates - surfaceCreates == g_surfaceDestroys - surfaceDestroys,
          "constructor and shadow resource surfaces balance across reset and destruction");
    check(g_effectCreates == g_effectDestroys &&
              g_parameterCreates == g_parameterDestroys,
          "effect and every shader-parameter owner release during destruction");
}

void testSunPropertyBodies()
{
    using dice::hfe::dx9rend::EngineTexture;

    void* templateTable[kTemplateVtableBytes / sizeof(void*)];
    FakeLightTemplate objectTemplate;
    initializeTemplate(objectTemplate, templateTable, 0, false, true, 0);

    void* lightTable[kLightVtableBytes / sizeof(void*)];
    FakeLight sun;
    initializeLight(sun, lightTable, objectTemplate, Vec3{0.0f, 0.0f, 0.0f}, 1.0f);

    const unsigned int liveTextures = static_cast<unsigned int>(g_textureStates.size());
    LightManager* const manager = createManager();
    rawField<FakeLight*>(manager, 0x840u) = &sun;

    const Vec3 color = { 0.2f, 0.4f, 0.6f };
    manager->setSunColor(color);
    const Vec3 colorRead = manager->getSunColor();
    check(sun.setColorCalls == 1 && sun.colorCalls == 1 &&
              closeFloat(colorRead.x, color.x) && closeFloat(colorRead.y, color.y) &&
              closeFloat(colorRead.z, color.z),
          "sunColor forwards the by-value setter and materializes the by-value getter result");

    manager->setSunEnabled(false);
    check(!manager->getSunEnabled() &&
              (rawField<unsigned int>(&sun, 0x04u) & 1u) != 0,
          "enableSun false sets Object disabled flag bit zero");
    manager->setSunEnabled(true);
    check(manager->getSunEnabled() && sun.updateFlagsCalls == 2 &&
              (rawField<unsigned int>(&sun, 0x04u) & 1u) == 0,
          "enableSun true clears Object disabled flag bit zero");

    EngineTexture* const firstLut = rawField<EngineTexture*>(manager, 0x254u);
    const int specLocks = g_textureLocks;
    const int specBinds = g_raTextureBinds;
    const Vec3 specular = { 0.5f, 0.25f, 0.75f };
    manager->setSunSpecColor(specular);
    FakeTextureState* const lutState = textureState(firstLut);
    check(rawField<EngineTexture*>(manager, 0x250u) == firstLut &&
              rawField<unsigned int>(manager, 0x264u) == 1u &&
              g_textureLocks == specLocks + 1 && g_raTextureBinds == specBinds + 1 &&
              lutState->pixels[0].back() == 0xFF7F3FBFu &&
              sun.setSpecularColorCalls == 1,
          "sunSpecColor rewrites and publishes the selected power-LUT ring entry before forwarding");
    manager->setSunSpecColor(specular);
    check(g_textureLocks == specLocks + 1 && g_raTextureBinds == specBinds + 1 &&
              sun.setSpecularColorCalls == 1 && sun.specularColorCalls == 1,
          "epsilon-equal sunSpecColor returns before LUT publication and LightSource mutation");
    const Vec3 specularRead = manager->getSunSpecColor();
    check(closeFloat(specularRead.x, specular.x) &&
              closeFloat(specularRead.y, specular.y) &&
              closeFloat(specularRead.z, specular.z),
          "sunSpecColor getter materializes the live LightSource value");

    const int directionTextures = g_textureCreates;
    const int directionSurfaces = g_surfaceCreates;
    const int directionSurfaceReleases = g_surfaceReleases;
    const int directionBinds = g_raTextureBinds;
    const Vec3 direction = { 0.1f, -0.9f, -0.3f };
    manager->setSunDirection(direction);
    EngineTexture* const normalCube = rawField<EngineTexture*>(manager, 0x24Cu);
    FakeTextureState* const normalState = textureState(normalCube);
    const Vec3 directionRead = manager->getSunDirection();
    check(sun.setDirectionCalls == 1 && sun.directionCalls == 1 &&
              closeFloat(directionRead.x, direction.x) &&
              closeFloat(directionRead.y, direction.y) &&
              closeFloat(directionRead.z, direction.z),
          "sunDirection forwards the by-value setter and materializes the getter result");
    check(g_textureCreates == directionTextures + 1 && normalState && normalState->cube &&
              normalCube->mipLevels == 3 &&
              g_surfaceCreates == directionSurfaces + 18 &&
              g_surfaceReleases == directionSurfaceReleases + 18 &&
              g_raTextureBinds == directionBinds + 1 &&
              (normalState->pixels[0][0] & 0xFF000000u) == 0xFF000000u,
          "sunDirection builds all six faces of three normalization-cube mips and publishes it");

    check(releaseManager(manager) == 0 && g_textureStates.size() == liveTextures,
          "sun property textures balance with the recovered LightManager destructor");
}

void testHemiLerpBiasForwarding()
{
    FakeHemiMapManager hemi;
    dice::hfe::geom::g_HemiMapManager = &hemi;

    const unsigned int liveTextures = static_cast<unsigned int>(g_textureStates.size());
    LightManager* const manager = createManager();
    manager->setHemiLerpBias(0.375f);
    check(hemi.setCalls == 1 && closeFloat(hemi.bias, 0.375f),
          "LightManager hemiLerpBias setter calls typed HemiMapManager method");
    check(closeFloat(manager->getHemiLerpBias(), 0.375f) && hemi.getCalls == 1,
          "LightManager hemiLerpBias getter calls typed HemiMapManager method");

    check(releaseManager(manager) == 0 && g_textureStates.size() == liveTextures,
          "hemiLerpBias forwarding leaves LightManager resource ownership balanced");
    dice::hfe::geom::g_HemiMapManager = 0;
}

void testShadowEffectRegistrationOwnership()
{
    const int effectCreates = g_effectCreates;
    const int effectDestroys = g_effectDestroys;
    LightManager* const manager = createManager();
    FakeEffectObject* const first = createFakeEffectObject(false);
    FakeEffectObject* const second = createFakeEffectObject(false);
    const int reflectionTypeId = 0x13579BDF;

    unsigned int eventStart = static_cast<unsigned int>(g_effectReferenceEvents.size());
    registerShadowEffect(manager, reflectionTypeId, first);
    check(first->references == 2 && first->addRefCalls == 1 &&
              first->releaseCalls == 0 &&
              g_effectReferenceEvents.size() == eventStart + 1 &&
              g_effectReferenceEvents[eventStart] == first->identity,
          "first +0x2F0 registration retains the published shadow effect exactly once");

    eventStart = static_cast<unsigned int>(g_effectReferenceEvents.size());
    registerShadowEffect(manager, reflectionTypeId, second);
    check(first->references == 1 && first->releaseCalls == 1 &&
              second->references == 2 && second->addRefCalls == 1 &&
              g_effectReferenceEvents.size() == eventStart + 2 &&
              g_effectReferenceEvents[eventStart] == -first->identity &&
              g_effectReferenceEvents[eventStart + 1] == second->identity,
          "same-key replacement releases the old effect before retaining the new effect");

    eventStart = static_cast<unsigned int>(g_effectReferenceEvents.size());
    registerShadowEffect(manager, reflectionTypeId, second);
    check(second->references == 2 && second->releaseCalls == 1 &&
              second->addRefCalls == 2 &&
              g_effectReferenceEvents.size() == eventStart + 2 &&
              g_effectReferenceEvents[eventStart] == -second->identity &&
              g_effectReferenceEvents[eventStart + 1] == second->identity,
          "same-pointer replacement preserves its count through retail release-then-retain order");

    eventStart = static_cast<unsigned int>(g_effectReferenceEvents.size());
    check(releaseManager(manager) == 0 && second->references == 1 &&
              second->releaseCalls == 2 &&
              g_effectReferenceEvents.size() == eventStart + 1 &&
              g_effectReferenceEvents[eventStart] == -second->identity,
          "LightManager destruction releases the effect still stored in the registration tree");
    check(first->references == 1 && first->releaseCalls == 1,
          "LightManager destruction does not release a shadow effect already replaced in the tree");

    fakeEffectRelease(first, 0);
    fakeEffectRelease(second, 0);
    check(g_effectCreates == effectCreates + 2 &&
              g_effectDestroys == effectDestroys + 2,
          "fixture-owned shadow effects balance after registration ownership is returned");
}

void testOwnershipAndSetSymmetry()
{
    void* templateTable[kTemplateVtableBytes / 4u];
    FakeLightTemplate objectTemplate;
    initializeTemplate(objectTemplate, templateTable, 0, false, true, 0);

    void* firstTable[kLightVtableBytes / 4u];
    void* secondTable[kLightVtableBytes / 4u];
    FakeLight first;
    FakeLight second;
    initializeLight(first, firstTable, objectTemplate, Vec3{0.0f, 0.0f, 5.0f}, 1.0f);
    initializeLight(second, secondTable, objectTemplate, Vec3{0.0f, 0.0f, 6.0f}, 1.0f);

    LightManager* const manager = createManager();
    check(manager != 0 && ownedLights(manager).size == 0 && activeLights(manager).size == 0,
          "factory constructs two distinct empty PE set facades");
    check(ownedLights(manager).head != activeLights(manager).head,
          "owned and active light sets have distinct sentinels");

    manager->addLight(asLight(first));
    manager->addLight(asLight(first));
    check(ownedLights(manager).size == 1 && first.addRefCalls == 2 && first.references == 12,
          "duplicate +0x8C insertion is rejected by the set but still addRefs twice");
    manager->removeLight(asLight(first));
    check(ownedLights(manager).size == 0 && first.releaseCalls == 1 && first.references == 11,
          "+0x90 erases the unique owning node and releases exactly its stored reference");
    manager->removeLight(asLight(first));
    check(first.releaseCalls == 1 && first.references == 11,
          "removing a missing owning light is a reference-count no-op");

    manager->addLight(asLight(first));
    manager->setSkyLight(asLight(first));
    check(first.references == 13 && manager->getSkyLight() == asLight(first),
          "owning-set and sky-light references are independent");
    manager->removeLight(asLight(first));
    check(first.references == 11 && first.releaseCalls == 3 && manager->getSkyLight() == 0,
          "removing a sky light releases sky ownership before owning-set ownership");

    manager->setSkyLight(asLight(second));
    manager->setSkyLight(asLight(second));
    check(second.addRefCalls == 2 && second.references == 12,
          "same-pointer sky assignment preserves the shipped duplicate addRef leak");
    manager->addLight(asLight(second));
    manager->removeLight(asLight(second));
    check(second.releaseCalls == 2 && second.references == 11 && manager->getSkyLight() == 0,
          "sky plus owning removal is symmetric while the duplicate sky addRef remains visible");

    check(releaseManager(manager) == 0,
          "LightManager final release destroys both set sentinels and the manager allocation");
}

void testIneligibleRegistrationAndActiveSet()
{
    LightManager* const manager = createManager();
    g_expansionPack = false;
    g_expansionPackCalls = 0;

    void* templateTable[kTemplateVtableBytes / 4u];
    void* lightTable[kLightVtableBytes / 4u];
    FakeLightTemplate objectTemplate;
    FakeLight light;

    initializeTemplate(objectTemplate, templateTable, 1, false, false, 0x800u);
    initializeLight(light, lightTable, objectTemplate, Vec3{0.0f, 0.0f, 5.0f}, 1.0f);
    manager->enableLight(asLight(light));
    check(objectTemplate.enabledCalls == 1 && objectTemplate.dynamicCalls == 1 &&
              objectTemplate.typeCalls == 1 && objectTemplate.flagsCalls == 0 &&
              g_expansionPackCalls == 0,
          "disabled static Point light fails the first Xp eligibility gate with retail short-circuiting");
    check(activeLights(manager).size == 1 && light.addRefCalls == 0,
          "Xp ineligibility does not prevent non-owning active-set insertion");

    manager->enableLight(asLight(light));
    check(activeLights(manager).size == 1,
          "duplicate activation leaves one active-set node");
    manager->disableLight(asLight(light));
    check(activeLights(manager).size == 0,
          "disable erases the matching active node");
    manager->disableLight(asLight(light));
    check(activeLights(manager).size == 0,
          "repeated disable takes the missing-node no-op branch");

    initializeTemplate(objectTemplate, templateTable, 0, false, true, 0x800u);
    manager->enableLight(asLight(light));
    check(objectTemplate.typeCalls == 1 && objectTemplate.flagsCalls == 0 &&
              g_expansionPackCalls == 0,
          "non Point/Spot type is rejected by +0x94 before entering the coordinator gates");
    manager->disableLight(asLight(light));

    initializeTemplate(objectTemplate, templateTable, 1, false, true, 0);
    manager->enableLight(asLight(light));
    check(objectTemplate.typeCalls == 2 && objectTemplate.flagsCalls == 1 &&
              objectTemplate.dynamicCalls == 1 &&
              g_expansionPackCalls == 0,
          "static light without template flag 0x800 fails before the renderer gate");
    manager->disableLight(asLight(light));

    initializeTemplate(objectTemplate, templateTable, 1, false, true, 0x800u);
    manager->enableLight(asLight(light));
    check(g_expansionPackCalls == 1 && light.addRefCalls == 0,
          "eligible-by-template static light still fails when the renderer is not expansion-pack");
    manager->disableLight(asLight(light));

    check(releaseManager(manager) == 0,
          "ineligible registration fixture tears down with no coordinator ownership");
}

void testXpRegistrationOwnershipAndCollectorSymmetry()
{
    Frustum frustum;
    frustum.setPerspective(1.0f, 1.0f, 0.1f, 500.0f, Mat4::getIdentity());
    initializeRenderView(frustum);
    initializeRendererBase();
    initializeObjectManager();

    void* controlledTemplateTable[kTemplateVtableBytes / 4u];
    void* controlledTable[kLightVtableBytes / 4u];
    FakeLightTemplate controlledTemplate;
    FakeLight controlled;
    initializeTemplate(controlledTemplate, controlledTemplateTable, 0, false, true, 0);
    initializeLight(controlled, controlledTable, controlledTemplate,
                    Vec3{1000.0f, 1000.0f, 1000.0f}, 1.0f);
    initializePlayerManager(&controlled);

    LightManager* const manager = createManager();
    dice::hfe::geom::g_LightManager = manager;

    void* staticTemplateTable[kTemplateVtableBytes / 4u];
    void* staticTable[kLightVtableBytes / 4u];
    FakeLightTemplate staticTemplate;
    FakeLight staticLight;
    initializeTemplate(staticTemplate, staticTemplateTable, 1, false, true, 0x800u);
    initializeLight(staticLight, staticTable, staticTemplate,
                    Vec3{0.0f, 0.0f, 8.0f}, 4.0f);

    g_expansionPack = true;
    g_expansionPackCalls = 0;
    manager->enableLight(asLight(staticLight));
    check(staticLight.addRefCalls == 1 && staticLight.references == 11,
          "eligible static Xp registration owns exactly one LightSource reference");
    check(staticLight.getEnabledCalls == 0 && staticLight.setEnabledCalls == 1 &&
              !staticLight.sourceEnabled,
          "static collector short-circuits flags bit zero and disables the registered source");
    check(g_videoOptions.getQualityCalls == 1 && g_videoOptions.lastQualityIndex == 4,
          "Xp policy reads renderer quality index four through the PE VideoOptions surface");

    manager->enableLight(asLight(staticLight));
    check(staticLight.addRefCalls == 1 && staticLight.setEnabledCalls == 1 &&
              activeLights(manager).size == 1,
          "duplicate Xp registration is a coordinator and active-set no-op");

    manager->disableLight(asLight(staticLight));
    check(staticLight.releaseCalls == 1 && staticLight.references == 10 &&
              activeLights(manager).size == 0,
          "static Xp removal leaves its collector and releases its sole source reference");
    manager->disableLight(asLight(staticLight));
    check(staticLight.releaseCalls == 1,
          "repeated static removal does not release or search a missing Xp wrapper again");

    void* gatedTemplateTable[kTemplateVtableBytes / 4u];
    void* gatedTable[kLightVtableBytes / 4u];
    FakeLightTemplate gatedTemplate;
    FakeLight gatedStatic;
    initializeTemplate(gatedTemplate, gatedTemplateTable, 1, false, true, 0x800u);
    initializeLight(gatedStatic, gatedTable, gatedTemplate,
                    Vec3{0.0f, 0.0f, 9.0f}, 4.0f, 10, 1u);
    gatedStatic.sourceEnabled = false;

    manager->enableLight(asLight(gatedStatic));
    check(gatedStatic.addRefCalls == 1 && gatedStatic.getEnabledCalls == 1 &&
              gatedStatic.setEnabledCalls == 0,
          "flags-bit-one static source stays outside its collector when getEnabled is false");
    manager->disableLight(asLight(gatedStatic));
    check(gatedStatic.releaseCalls == 1 && gatedStatic.references == 10,
          "collector-rejected static wrapper remains coordinator-owned and removes symmetrically");

    void* dynamicTemplateTable[kTemplateVtableBytes / 4u];
    void* dynamicTable[kLightVtableBytes / 4u];
    FakeLightTemplate dynamicTemplate;
    FakeLight dynamicLight;
    initializeTemplate(dynamicTemplate, dynamicTemplateTable, 1, true, false, 0);
    initializeLight(dynamicLight, dynamicTable, dynamicTemplate,
                    Vec3{0.0f, 0.0f, 10.0f}, 4.0f);

    g_expansionPack = false;
    manager->enableLight(asLight(dynamicLight));
    check(dynamicLight.addRefCalls == 1 && dynamicLight.setEnabledCalls == 0 &&
              dynamicLight.getEnabledCalls == 0,
          "dynamic registration bypasses static template, expansion and source-enabled gates");
    manager->enableLight(asLight(dynamicLight));
    check(dynamicLight.addRefCalls == 1 && activeLights(manager).size == 1,
          "duplicate dynamic registration preserves one wrapper and one owned reference");
    manager->disableLight(asLight(dynamicLight));
    check(dynamicLight.releaseCalls == 1 && dynamicLight.references == 10,
          "dynamic collector removal releases the wrapper's source reference exactly once");

    dice::hfe::geom::g_LightManager = 0;
    check(releaseManager(manager) == 0,
          "Xp registration fixture releases its manager after all wrappers are removed");
}

void testXpPointQueriesCullingAndBorrowedCandidates()
{
    Frustum frustum;
    frustum.setPerspective(1.0f, 1.0f, 0.1f, 500.0f, Mat4::getIdentity());
    initializeRenderView(frustum);
    initializeRendererBase();
    initializeObjectManager();

    void* controlledTemplateTable[kTemplateVtableBytes / 4u];
    void* controlledTable[kLightVtableBytes / 4u];
    FakeLightTemplate controlledTemplate;
    FakeLight controlled;
    initializeTemplate(controlledTemplate, controlledTemplateTable, 0, false, true, 0);
    initializeLight(controlled, controlledTable, controlledTemplate,
                    Vec3{1000.0f, 1000.0f, 1000.0f}, 1.0f);
    initializePlayerManager(&controlled);

    int sharedGeometry = 0;
    void* firstCandidateTable[0x0Cu / 4u];
    void* secondCandidateTable[0x0Cu / 4u];
    FakeCandidate firstCandidate;
    FakeCandidate secondCandidate;
    initializeCandidate(firstCandidate, firstCandidateTable, &sharedGeometry);
    initializeCandidate(secondCandidate, secondCandidateTable, &sharedGeometry);
    g_objectManager.sphereCandidates.push_back(&firstCandidate);
    g_objectManager.sphereCandidates.push_back(&secondCandidate);

    void* templateTable[kTemplateVtableBytes / 4u];
    void* lightTable[kLightVtableBytes / 4u];
    FakeLightTemplate objectTemplate;
    FakeLight light;
    initializeTemplate(objectTemplate, templateTable, 1, true, false, 0);
    initializeLight(light, lightTable, objectTemplate, Vec3{0.0f, 0.0f, 10.0f}, 4.0f);

    LightManager* const manager = createManager();
    dice::hfe::geom::g_LightManager = manager;
    g_expansionPack = false;
    manager->enableLight(asLight(light));

    dice::hfe::geom::XpLight* const xp = registeredXp(light);
    check(xp != 0 && xp->category == 0,
          "eligible dynamic Point source constructs a category-zero Xp wrapper");
    if (xp)
    {
        dice::hfe::geom::prepareLightFrame(12.5);
        check(xp->active == 1 && xpCandidateCount(xp) == 2,
              "visible Point wrapper retains both ObjectManager candidates");
        check(g_objectManager.queries.size() == 1 &&
                  g_objectManager.queries[0].kind == ObjectQuery::SphereQuery &&
                  closeFloat(g_objectManager.queries[0].position.z, 10.0f) &&
                  closeFloat(g_objectManager.queries[0].radius, 4.0f),
              "Point collector issues one sphere query with source position and effective radius");
        check(g_playerManager.localPlayerCalls == 1 &&
                  g_localPlayer.controlledObjectCalls == 1 && controlled.positionCalls == 1,
              "Point collection performs the local controlled-object proximity probe after grid query");

        check(dice::hfe::geom::g_xpCoordinator.geometryPairs.size() == 2 &&
                  dice::hfe::geom::g_xpCoordinator.geometryPairs.count(&sharedGeometry) == 2 &&
                  g_frontierCalls == 0,
              "public nonempty prepare retains duplicate +0xA4 geometry keys without a B0 frontier");
        bool bothValuesAreSource = true;
        std::pair<std::multimap<void*, dice::hfe::world::LightSource*>::const_iterator,
                  std::multimap<void*, dice::hfe::world::LightSource*>::const_iterator> range =
            dice::hfe::geom::g_xpCoordinator.geometryPairs.equal_range(&sharedGeometry);
        for (std::multimap<void*, dice::hfe::world::LightSource*>::const_iterator it = range.first;
             it != range.second; ++it)
            bothValuesAreSource = bothValuesAreSource && it->second == asLight(light);
        check(bothValuesAreSource && firstCandidate.addRefCalls == 0 &&
                  firstCandidate.releaseCalls == 0 && secondCandidate.addRefCalls == 0 &&
                  secondCandidate.releaseCalls == 0,
              "geometry pairs carry the source while candidate IObjects remain entirely borrowed");

        g_objectManager.queries.clear();
        setLightPosition(light, Vec3{0.0f, 0.0f, 150.0f});
        dice::hfe::geom::updateXpCoordinator(13.0);
        check(xp->active == 0 && g_objectManager.queries.empty(),
              "Point range cull rejects a source beyond the shipped 100-unit threshold before query");

        setLightPosition(light, Vec3{20.0f, 0.0f, 10.0f});
        dice::hfe::geom::updateXpCoordinator(13.5);
        check(xp->active == 0 && g_objectManager.queries.empty(),
              "Point frustum cull rejects an in-range source outside the camera side planes");

        setLightPosition(light, Vec3{0.0f, 0.0f, 10.0f});
        g_objectManager.sphereCandidates.clear();
        g_objectManager.queries.clear();
        dice::hfe::geom::prepareLightFrame(14.0);
        check(g_objectManager.queries.size() == 1 && xp->active == 0 &&
                  xpCandidateCount(xp) == 0 &&
                  dice::hfe::geom::g_xpCoordinator.geometryPairs.empty(),
              "empty Point query clears candidates, deactivates the wrapper and avoids finalizer pairs");
    }

    manager->disableLight(asLight(light));
    dice::hfe::geom::prepareLightFrame(0.0);
    check(light.addRefCalls == 1 && light.releaseCalls == 1 && light.references == 10,
          "Point wrapper alone owns and releases its source; candidates acquire no ownership");
    dice::hfe::geom::g_LightManager = 0;
    check(releaseManager(manager) == 0,
          "Point query fixture removes its wrapper before manager destruction");
}

void testXpCollectorCapsAndTransientActiveFlags()
{
    Frustum frustum;
    frustum.setPerspective(1.0f, 1.0f, 0.1f, 500.0f, Mat4::getIdentity());
    initializeRenderView(frustum);
    initializeRendererBase();
    initializeObjectManager();

    void* controlledTemplateTable[kTemplateVtableBytes / 4u];
    void* controlledTable[kLightVtableBytes / 4u];
    FakeLightTemplate controlledTemplate;
    FakeLight controlled;
    initializeTemplate(controlledTemplate, controlledTemplateTable, 0, false, true, 0);
    initializeLight(controlled, controlledTable, controlledTemplate,
                    Vec3{1000.0f, 1000.0f, 1000.0f}, 1.0f);
    initializePlayerManager(&controlled);

    int sharedGeometry = 0;
    void* candidateTable[0x0Cu / 4u];
    FakeCandidate candidate;
    initializeCandidate(candidate, candidateTable, &sharedGeometry);
    g_objectManager.sphereCandidates.push_back(&candidate);

    void* staticTemplateTable[kTemplateVtableBytes / 4u];
    void* dynamicTemplateTable[kTemplateVtableBytes / 4u];
    FakeLightTemplate staticTemplate;
    FakeLightTemplate dynamicTemplate;
    initializeTemplate(staticTemplate, staticTemplateTable, 1, false, true, 0x800u);
    initializeTemplate(dynamicTemplate, dynamicTemplateTable, 1, true, false, 0);

    const unsigned int kCount = 5;
    void* staticTables[kCount][kLightVtableBytes / 4u];
    void* dynamicTables[kCount][kLightVtableBytes / 4u];
    FakeLight staticLights[kCount];
    FakeLight dynamicLights[kCount];

    LightManager* const manager = createManager();
    dice::hfe::geom::g_LightManager = manager;
    g_expansionPack = true;
    for (unsigned int i = 0; i < kCount; ++i)
    {
        const float staticDistance = 10.0f + 10.0f * static_cast<float>(i);
        const float dynamicDistance = staticDistance + 1.0f;
        initializeLight(staticLights[i], staticTables[i], staticTemplate,
                        Vec3{0.0f, 0.0f, staticDistance}, 2.0f);
        initializeLight(dynamicLights[i], dynamicTables[i], dynamicTemplate,
                        Vec3{0.0f, 0.0f, dynamicDistance}, 2.0f);
        manager->enableLight(asLight(staticLights[i]));
        manager->enableLight(asLight(dynamicLights[i]));
    }

    check(dice::hfe::geom::g_xpCoordinator.staticLights.size() == kCount &&
              dice::hfe::geom::g_xpCoordinator.dynamicLights.size() == kCount,
          "static and dynamic collectors uniquely contain all five registered wrappers");

    g_objectManager.queries.clear();
    dice::hfe::geom::g_xpCoordinator.geometryPairs.clear();
    dice::hfe::geom::updateXpCoordinator(21.0);
    check(dice::hfe::geom::g_xpCoordinator.staticFrame.size() == 3 &&
              dice::hfe::geom::g_xpCoordinator.dynamicFrame.size() == 3,
          "both collectors retain the shipped cap of three nearest wrappers");

    bool staticOrder = true;
    bool dynamicOrder = true;
    for (unsigned int i = 0; i < 3; ++i)
    {
        staticOrder = staticOrder &&
            dice::hfe::geom::g_xpCoordinator.staticFrame[i] == registeredXp(staticLights[i]) &&
            closeFloat(dice::hfe::geom::g_xpCoordinator.staticFrame[i]->cameraDistance,
                       10.0f + 10.0f * static_cast<float>(i));
        dynamicOrder = dynamicOrder &&
            dice::hfe::geom::g_xpCoordinator.dynamicFrame[i] == registeredXp(dynamicLights[i]) &&
            closeFloat(dice::hfe::geom::g_xpCoordinator.dynamicFrame[i]->cameraDistance,
                       11.0f + 10.0f * static_cast<float>(i));
    }
    check(staticOrder && dynamicOrder,
          "collector frame vectors are sorted by ascending camera distance before truncation");
    check(g_objectManager.queries.size() == 6,
          "only the three retained static and three retained dynamic wrappers query candidates");

    bool survivorState = true;
    for (unsigned int i = 0; i < 3; ++i)
    {
        dice::hfe::geom::XpLight* const staticXp = registeredXp(staticLights[i]);
        dice::hfe::geom::XpLight* const dynamicXp = registeredXp(dynamicLights[i]);
        survivorState = survivorState && staticXp && dynamicXp &&
            staticXp->active == 1 && dynamicXp->active == 1 &&
            xpCandidateCount(staticXp) == 1 && xpCandidateCount(dynamicXp) == 1;
    }
    check(survivorState,
          "retained wrappers stay active and receive one borrowed candidate each");

    bool staticPoppedStayActive = true;
    bool dynamicPoppedClear = true;
    for (unsigned int i = 3; i < kCount; ++i)
    {
        dice::hfe::geom::XpLight* const staticXp = registeredXp(staticLights[i]);
        dice::hfe::geom::XpLight* const dynamicXp = registeredXp(dynamicLights[i]);
        staticPoppedStayActive = staticPoppedStayActive && staticXp &&
            staticXp->active == 1 && xpCandidateCount(staticXp) == 0;
        dynamicPoppedClear = dynamicPoppedClear && dynamicXp &&
            dynamicXp->active == 0 && xpCandidateCount(dynamicXp) == 0;
    }
    check(staticPoppedStayActive,
          "static cap pops farthest wrappers without clearing their current-frame active byte");
    check(dynamicPoppedClear,
          "dynamic cap pops farthest wrappers and clears their current-frame active byte");

    dice::hfe::geom::buildXpGeometryPairs();
    check(dice::hfe::geom::g_xpCoordinator.geometryPairs.size() == 6 &&
              dice::hfe::geom::g_xpCoordinator.geometryPairs.count(&sharedGeometry) == 6,
          "pair construction walks only active prepared wrappers and keeps duplicate geometry keys");
    check(candidate.addRefCalls == 0 && candidate.releaseCalls == 0,
          "collector cap and grouping paths never take ownership of candidate objects");

    for (unsigned int i = 0; i < kCount; ++i)
    {
        manager->disableLight(asLight(staticLights[i]));
        manager->disableLight(asLight(dynamicLights[i]));
    }
    dice::hfe::geom::prepareLightFrame(0.0);
    bool referencesBalanced = true;
    for (unsigned int i = 0; i < kCount; ++i)
    {
        referencesBalanced = referencesBalanced &&
            staticLights[i].addRefCalls == 1 && staticLights[i].releaseCalls == 1 &&
            staticLights[i].references == 10 && dynamicLights[i].addRefCalls == 1 &&
            dynamicLights[i].releaseCalls == 1 && dynamicLights[i].references == 10;
    }
    check(referencesBalanced && dice::hfe::geom::g_xpCoordinator.bySource.empty() &&
              dice::hfe::geom::g_xpCoordinator.staticLights.empty() &&
              dice::hfe::geom::g_xpCoordinator.dynamicLights.empty(),
          "all capped wrappers remove symmetrically and release exactly their source ownership");

    dice::hfe::geom::g_LightManager = 0;
    check(releaseManager(manager) == 0,
          "collector-cap fixture releases its manager after coordinator cleanup");
}

void testActiveLightCulling()
{
    Frustum frustum;
    frustum.setPerspective(1.0f, 1.0f, 0.1f, 500.0f, Mat4::getIdentity());
    initializeRenderView(frustum);

    void* templateTable[kTemplateVtableBytes / 4u];
    FakeLightTemplate objectTemplate;
    initializeTemplate(objectTemplate, templateTable, 0, false, true, 0);

    void* insideTable[kLightVtableBytes / 4u];
    void* sideTable[kLightVtableBytes / 4u];
    void* farTable[kLightVtableBytes / 4u];
    FakeLight inside;
    FakeLight side;
    FakeLight farAway;
    initializeLight(inside, insideTable, objectTemplate, Vec3{0.0f, 0.0f, 10.0f}, 0.5f);
    initializeLight(side, sideTable, objectTemplate, Vec3{100.0f, 0.0f, 10.0f}, 0.5f);
    initializeLight(farAway, farTable, objectTemplate, Vec3{0.0f, 0.0f, 80.0f}, 0.5f);

    LightManager* const manager = createManager();
    manager->enableLight(asLight(inside));
    manager->enableLight(asLight(side));
    manager->enableLight(asLight(farAway));
    manager->setLightCullDistance(50.0f);
    manager->update(7.25);

    check(g_renderViewCalls == 1 && g_renderView.frustumCalls == 1,
          "nonzero cull distance refreshes the active RenderView frustum once");
    check(inside.radiusCalls == 1 && inside.positionCalls == 1 &&
              side.radiusCalls == 1 && side.positionCalls == 1 &&
              farAway.radiusCalls == 1 && farAway.positionCalls == 1,
          "culling queries every active light's radius before position");
    check(visibleCount(manager) == 1 && visibleAt(manager, 0) == asLight(inside),
          "sphere culling retains only the light inside side planes and configured far distance");

    manager->setLightCullDistance(0.0f);
    manager->update(-3.0);
    check(visibleCount(manager) == 0 && g_renderViewCalls == 1,
          "zero cull distance clears stale visible storage and returns before RenderView lookup");

    manager->disableLight(asLight(inside));
    manager->disableLight(asLight(side));
    manager->disableLight(asLight(farAway));
    check(activeLights(manager).size == 0,
          "culling fixture removes all active-set nodes symmetrically");
    check(releaseManager(manager) == 0,
          "culling fixture releases manager after scratch-vector allocation");
}

} // anonymous namespace

extern "C" HRESULT WINAPI D3DXLoadSurfaceFromSurface(
    IDirect3DSurface9* destination, const PALETTEENTRY*, const RECT*,
    IDirect3DSurface9* source, const PALETTEENTRY*, const RECT*, DWORD, D3DCOLOR)
{
    ++g_surfaceCopies;
    FakeSurface* const destinationSurface = reinterpret_cast<FakeSurface*>(destination);
    FakeSurface* const sourceSurface = reinterpret_cast<FakeSurface*>(source);
    if (destinationSurface && sourceSurface &&
        destinationSurface->texture && sourceSurface->texture)
    {
        destinationSurface->texture->pixels[destinationSurface->face] =
            sourceSurface->texture->pixels[sourceSurface->face];
    }
    return D3D_OK;
}

namespace renddx9abi {

void* engineAlloc(unsigned int size)
{
    ++g_engineAllocations;
    return std::malloc(size);
}

void engineFree(void* pointer)
{
    if (pointer)
        ++g_engineFrees;
    std::free(pointer);
}

void logOpen() {}
void logClose() {}
void logf(const char*, ...) {}
void trace(const char*, const void*, unsigned int) {}
bool frontierTrapEnabled() { return true; }
unsigned int addr(const void* pointer) { return reinterpret_cast<unsigned int>(pointer); }
void* crtMalloc(size_t size) { return std::malloc(size); }
void crtFree(void* pointer) { std::free(pointer); }

void frontier(const char*, const void*, unsigned int)
{
    ++g_frontierCalls;
}

} // namespace renddx9abi

namespace dice {
namespace hfe {

// Reserved for the XpLight/ObjectManager selection surface recovered by the production TU.  The
// fixture installs a PE-layout record here once a test needs that dependency.
IClassManager* classManager = 0;
IBase* settings = 0;
renddx9abi::HostFileManager* fileManager = 0;
IBase* objectManager = reinterpret_cast<IBase*>(&g_objectManager);
IBase* playerManager = reinterpret_cast<IBase*>(&g_playerManager);

namespace dx9rend {

// The production TU deliberately calls the recovered ShaderParameter members directly.  This
// fixture stores a PE-shaped fake in those fields, so its two exercised member bodies forward to
// the fake counters instead of linking the full Effect subsystem.
int ShaderParameter::release()
{
    return fakeEffectRelease(reinterpret_cast<FakeEffectObject*>(this), 0);
}

void ShaderParameter::setFloat(float value)
{
    fakeParameterSetFloat(reinterpret_cast<FakeEffectObject*>(this), 0, value);
}

void* g_rendererBase = &g_rendererBaseObject;
void* g_shaderManager = reinterpret_cast<void*>(0x1234u);

IRenderView* rendererGetRenderView()
{
    ++g_renderViewCalls;
    return reinterpret_cast<IRenderView*>(&g_renderView);
}

unsigned int rendererFrameOrdinal()
{
    return 0;
}

bool rendererIsExpansionPack()
{
    ++g_expansionPackCalls;
    return g_expansionPack;
}

IVideoOptions* rendererGetVideoOptions()
{
    return reinterpret_cast<IVideoOptions*>(&g_videoOptions);
}

void registerDeviceLostResetListener(DeviceLostResetListener* listener, int priority)
{
    g_deviceListener = listener;
    g_deviceListenerPriority = priority;
    ++g_deviceRegistrations;
    g_lifecycleEvents.push_back(LifecycleRegisterDevice);
}

void unregisterDeviceLostResetListener(DeviceLostResetListener* listener)
{
    check(listener == g_deviceListener,
          "device-listener unregister receives the registered secondary this pointer");
    g_deviceListener = 0;
    ++g_deviceUnregistrations;
    g_lifecycleEvents.push_back(LifecycleUnregisterDevice);
}

void registerRenderPathListener(RenderPathListener* listener)
{
    g_renderPathListener = listener;
    ++g_renderPathRegistrations;
    g_lifecycleEvents.push_back(LifecycleRegisterRenderPath);
}

void unregisterRenderPathListener(RenderPathListener* listener)
{
    check(listener == g_renderPathListener,
          "render-path unregister receives the registered tertiary this pointer");
    g_renderPathListener = 0;
    ++g_renderPathUnregistrations;
    g_lifecycleEvents.push_back(LifecycleUnregisterRenderPath);
}

bool rendererPerformanceFlag(unsigned int)
{
    return false;
}

int deviceCapsPixelShaderMajor()
{
    return 3;
}

bool deviceCapsIsAti()
{
    return false;
}

bool deviceCapsIsNvidia()
{
    return true;
}

bool deviceCapsSupportsTextureFormat(unsigned int)
{
    return true;
}

int textureManagerSwapManaged(int value)
{
    const int previous = g_managedTextureMode;
    g_managedTextureMode = value;
    ++g_managedTextureSwaps;
    return previous;
}

void* textureManagerCreateEmpty(unsigned int width, unsigned int height,
                                unsigned int format, unsigned int,
                                unsigned int, void*)
{
    return createFakeTexture(width, height, format, false);
}

void* textureManagerCreateCube(unsigned int side, unsigned int format,
                               unsigned int mips, unsigned int, void*)
{
    EngineTexture* const texture = createFakeTexture(side, side, format, true);
    texture->mipLevels = mips;
    return texture;
}

void* textureManagerGetTexture(const vc71::string& name)
{
    g_loadedTextureNames.push_back(name.c_str());
    return createFakeTexture(4, 4, D3DFMT_A8R8G8B8, false);
}

TextureLockRecord* textureLock(EngineTexture* texture, unsigned int mip,
                               unsigned int face)
{
    FakeTextureState* const state = textureState(texture);
    TextureLockRecord* const lock = static_cast<TextureLockRecord*>(
        std::calloc(1, sizeof(TextureLockRecord)));
    lock->width = state->width;
    lock->height = state->height;
    lock->depth = 1;
    lock->bpp = 32;
    lock->mip = mip;
    lock->face = face;
    lock->pBits = &state->pixels[face == 0x7FFFFFFFu ? 0u : face][0];
    ++g_textureLocks;
    return lock;
}

bool textureUnlock(EngineTexture*, TextureLockRecord* lock)
{
    ++g_textureUnlocks;
    std::free(lock);
    return true;
}

IDirect3DSurface9* textureGetLevelSurface(EngineTexture* texture,
                                          unsigned int, unsigned int face)
{
    FakeSurface* const surface = static_cast<FakeSurface*>(
        std::calloc(1, sizeof(FakeSurface)));
    surface->table = g_fakeComTable;
    surface->references = 1;
    surface->kind = FakeComSurface;
    surface->destroyAtZero = true;
    surface->texture = textureState(texture);
    surface->face = face == 0x7FFFFFFFu ? 0u : face;
    ++g_surfaceCreates;
    return reinterpret_cast<IDirect3DSurface9*>(surface);
}

void textureAddRef(EngineTexture* texture)
{
    ++texture->refCount;
    ++g_textureAddRefs;
}

void textureReleaseRef(EngineTexture*& texture)
{
    if (!texture)
        return;
    ++g_textureReleaseCalls;
    if (--texture->refCount == 0)
    {
        texture->baseTexture->Release();
        std::map<EngineTexture*, FakeTextureState*>::iterator found =
            g_textureStates.find(texture);
        if (found != g_textureStates.end())
        {
            delete found->second;
            g_textureStates.erase(found);
        }
        std::free(texture);
        ++g_textureDestroys;
    }
    texture = 0;
}

void raShaderConstantsSetTexture(unsigned int, IDirect3DBaseTexture9*)
{
    ++g_raTextureBinds;
}

void raShaderConstantsSetEngineTexture(unsigned int, EngineTexture*)
{
    ++g_raTextureBinds;
}

void* shaderManagerGetEffect(void*, const char*)
{
    ++g_effectLookups;
    return createFakeEffectObject(false);
}

void* effectGetShaderParameter(Effect*, const vc71::string& name)
{
    g_parameterNames.push_back(name.c_str());
    return createFakeEffectObject(true);
}

void effectAddRef(Effect* effect)
{
    fakeEffectAddRef(reinterpret_cast<FakeEffectObject*>(effect), 0);
}

int effectRelease(Effect* effect)
{
    return fakeEffectRelease(reinterpret_cast<FakeEffectObject*>(effect), 0);
}

unsigned int effectBegin(void*) { return 0; }
void effectBeginPass(void*, unsigned int) {}
void effectEnd(void*) {}

void stateManagerPush() { ++g_statePushes; }
void stateManagerPop() { ++g_statePops; }
void stateManagerSetRenderState(unsigned int, unsigned int) { ++g_renderStateSets; }
void stateManagerSetRenderTarget(unsigned int, IDirect3DSurface9*) { ++g_renderTargetSets; }
void stateManagerSetDepthStencilSurface(IDirect3DSurface9*) { ++g_depthSurfaceSets; }
void stateManagerClear(unsigned int, unsigned int, float, unsigned int) { ++g_stateClears; }
void flushRenderTargetsSeam() { ++g_renderTargetFlushes; }

} // namespace dx9rend

namespace geom {

LightManager* g_LightManager = 0;
IHemiMapManager* g_HemiMapManager = 0;

} // namespace geom
} // namespace hfe
}
// Raw geometry/template/player-control tables expose the slots used by typed shadow helpers, checking MSVC placement.
struct FakeShadowGeometry
{
    void** table;
    void* templateObject;
    dice::hfe::LocalBoundingBox bounds;
    void* owner;
    void* root;
    bool acceptsShadow;
    unsigned int stampedFrame;
    void* attachedRecord;
    int calls[0x100 / 4];
    const void* drawArgs[4];
    unsigned int drawFlags[2];
};
void* __fastcall fsgTemplate(FakeShadowGeometry* s, void*) { ++s->calls[0x38 / 4]; return s->templateObject; }
const void* __fastcall fsgBounds(FakeShadowGeometry* s, void*) { ++s->calls[0x40 / 4]; return &s->bounds; }
void* __fastcall fsgOwner(FakeShadowGeometry* s, void*) { ++s->calls[0xA0 / 4]; return s->owner; }
void* __fastcall fsgRoot(FakeShadowGeometry* s, void*) { ++s->calls[0xA4 / 4]; return s->root; }
bool __fastcall fsgAccepts(FakeShadowGeometry* s, void*) { ++s->calls[0xA8 / 4]; return s->acceptsShadow; }
void __fastcall fsgStamp(FakeShadowGeometry* s, void*, unsigned int frame)
{ ++s->calls[0xAC / 4]; s->stampedFrame = frame; }
void __fastcall fsgAttach(FakeShadowGeometry* s, void*, void* record)
{ ++s->calls[0x24 / 4]; s->attachedRecord = record; }
void __fastcall fsgDraw(FakeShadowGeometry* s, void*, const void* a1, const void* a2, const void* a3,
                        const void* a4, unsigned int a5, unsigned int a6)
{
    ++s->calls[0x20 / 4];
    s->drawArgs[0] = a1; s->drawArgs[1] = a2; s->drawArgs[2] = a3; s->drawArgs[3] = a4;
    s->drawFlags[0] = a5; s->drawFlags[1] = a6;
}

struct FakeShadowTemplate { void** table; unsigned int classId; bool sphere; int idCalls, sphereCalls; };
unsigned int __fastcall fstClassId(FakeShadowTemplate* t, void*) { ++t->idCalls; return t->classId; }
bool __fastcall fstSphere(FakeShadowTemplate* t, void*) { ++t->sphereCalls; return t->sphere; }

struct FakeShadowControl { void** table; vc71::vector_layout weapons; int calls; };
vc71::vector_layout* __fastcall fscWeapons(FakeShadowControl* c, void*) { ++c->calls; return &c->weapons; }

void testShadowGeometrySlots()
{
    static void* geometryTable[63] = {};
    static void* templateTable[60] = {};
    static void* controlTable[0x100 / 4] = {};
    geometryTable[0x20 / 4] = reinterpret_cast<void*>(&fsgDraw);
    geometryTable[0x24 / 4] = reinterpret_cast<void*>(&fsgAttach);
    geometryTable[0x38 / 4] = reinterpret_cast<void*>(&fsgTemplate);
    geometryTable[0x40 / 4] = reinterpret_cast<void*>(&fsgBounds);
    geometryTable[0xA0 / 4] = reinterpret_cast<void*>(&fsgOwner);
    geometryTable[0xA4 / 4] = reinterpret_cast<void*>(&fsgRoot);
    geometryTable[0xA8 / 4] = reinterpret_cast<void*>(&fsgAccepts);
    geometryTable[0xAC / 4] = reinterpret_cast<void*>(&fsgStamp);
    templateTable[0x10 / 4] = reinterpret_cast<void*>(&fstClassId);    // IGeometryTemplate::getClassId
    templateTable[0x9C / 4] = reinterpret_cast<void*>(&fstSphere);     // IGeometryTemplate::slot_0x9C
    controlTable[0x70 / 4] = reinterpret_cast<void*>(&fscWeapons);     // IPlayerControlObject::getWeapons()

    FakeShadowTemplate objectTemplate = {templateTable, 0x870CA652u, true, 0, 0};
    FakeShadowGeometry geometry;
    std::memset(&geometry, 0, sizeof(geometry));
    geometry.table = geometryTable;
    geometry.templateObject = &objectTemplate;
    int owner = 0, root = 0;
    geometry.owner = &owner;
    geometry.root = &root;
    geometry.acceptsShadow = true;

    check(dice::hfe::geom::geometryReflectionType(&geometry) == 0x870CA652u
          && geometry.calls[0x38 / 4] == 1 && objectTemplate.idCalls == 1,
          "G2: reflection type = geometry +0x38, then the template's typed +0x10");
    check(&dice::hfe::geom::geometryLocalBounds(&geometry) == &geometry.bounds,
          "G2: +0x40 answers the geometry's own local bounds");
    check(dice::hfe::geom::geometryAcceptsXpShadow(&geometry) && geometry.calls[0xA8 / 4] == 1,
          "G2: +0xA8 answers the caster flag");
    geometry.acceptsShadow = false;
    check(!dice::hfe::geom::geometryAcceptsXpShadow(&geometry), "G2: +0xA8 false is false");
    check(dice::hfe::geom::geometryObject(&geometry) == reinterpret_cast<dice::hfe::world::IObject*>(&owner)
          && dice::hfe::geom::geometryFinalizerObject(&geometry) == reinterpret_cast<dice::hfe::world::IObject*>(&root),
          "G2: +0xA0 is the owner and +0xA4 the root");
    dice::hfe::geom::geometryPrepareShadow(&geometry, 0x1234u);
    check(geometry.stampedFrame == 0x1234u && geometry.calls[0xAC / 4] == 1,
          "G2: +0xAC receives the renderer frame token");
    {
        dice::hfe::geom::XpRenderRecordLayout record;
        dice::hfe::geom::geometryAttachShadowRecord(&geometry, &record);
        check(geometry.attachedRecord == &record, "G2: +0x24 receives the record");
        for (int repeat = 0; repeat < 64; ++repeat)      // a wrong arity would unbalance the stack
            dice::hfe::geom::geometryDrawShadow(&geometry, &record, 0, 0);
        check(geometry.calls[0x20 / 4] == 64 && geometry.drawArgs[0] == &record
              && geometry.drawArgs[1] == &record.matrices[1]
              && geometry.drawArgs[2] == &record.matrices[2]
              && geometry.drawArgs[3] == &record.frustum
              && geometry.drawFlags[0] == 0 && geometry.drawFlags[1] == 0,
              "G2: the point/spot +0x20 draw passes record, +0x238, +0x278, +0x10, 0, 0");
        check(reinterpret_cast<const unsigned char*>(&record.matrices[1])
                  - reinterpret_cast<const unsigned char*>(&record) == 0x238
              && reinterpret_cast<const unsigned char*>(&record.matrices[2])
                  - reinterpret_cast<const unsigned char*>(&record) == 0x278
              && reinterpret_cast<const unsigned char*>(&record.frustum)
                  - reinterpret_cast<const unsigned char*>(&record) == 0x10,
              "G2: the three record offsets the PE pushes");
    }
    check(!dice::hfe::geom::geometryUsesShadowSphere(0), "G2: a null geometry uses no shadow sphere");
    check(dice::hfe::geom::geometryUsesShadowSphere(&geometry) && objectTemplate.sphereCalls == 1,
          "G2: the shadow sphere is the template's typed +0x9C");
    objectTemplate.sphere = false;
    check(!dice::hfe::geom::geometryUsesShadowSphere(&geometry), "G2: template +0x9C false is false");

    dice::hfe::world::IObject* weapons[3] = {};
    FakeShadowControl control;
    std::memset(&control, 0, sizeof(control));
    control.table = controlTable;
    control.weapons.first = reinterpret_cast<char*>(weapons);
    control.weapons.last = reinterpret_cast<char*>(weapons + 3);
    check(dice::hfe::geom::controlFirstPersonObjectsBegin(&control) == weapons
          && dice::hfe::geom::controlFirstPersonObjectsEnd(&control) == weapons + 3
          && control.calls == 2,
          "G2: the exclusion list is the PCO's typed getWeapons() at MSVC +0x70 (VC7.1 first/last)");
}

int main()
{
    static_assert(sizeof(void*) == 4, "LightManager runtime fixture is x86-only");
    static_assert(sizeof(vc71::set_layout) == 12, "VC7 set facade drift");
    static_assert(sizeof(vc71::vector_layout) == 16, "VC7 vector facade drift");

    initializeFakeCom();
    initializeRendererBase();

    testConstructionResourcesAndDestruction();
    testSimpleShadowSamplerAbi();
    testSunPropertyBodies();
    testHemiLerpBiasForwarding();
    testShadowEffectRegistrationOwnership();
    testOwnershipAndSetSymmetry();
    testIneligibleRegistrationAndActiveSet();
    testXpRegistrationOwnershipAndCollectorSymmetry();
    testXpPointQueriesCullingAndBorrowedCandidates();
    testXpCollectorCapsAndTransientActiveFlags();
    testActiveLightCulling();
    testShadowGeometrySlots();

    if (g_rendererBaseObject.primaryRenderTarget)
    {
        g_rendererBaseObject.primaryRenderTarget->Release();
        g_rendererBaseObject.primaryRenderTarget = 0;
    }
    check(g_frontierCalls == 0, "covered LightManager paths do not enter a named frontier");
    check(g_textureStates.empty(), "all harness texture wrappers are released");
    check(g_surfaceCreates == g_surfaceDestroys,
          "all temporary and renderer-owned fake surfaces are released");
    check(g_engineAllocations == g_engineFrees,
          "all manager, set sentinel and set node allocations are released");

    std::printf("light manager runtime: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
