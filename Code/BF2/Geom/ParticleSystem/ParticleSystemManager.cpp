// Owns particle templates, runtime queues, pools and four embedded renderers.
// Coordinates wind, pause and resource lifecycle events.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "ParticleSystemManager.h"
#include "../XpLightManager.h"
#include "../StaticMesh.h"
#include "../HemiMapManager/HemiMapManager.h"
#include "../../Math/Mat4.h"
#include "../../Math/Frustum.h"
#include "../../RendDX9/ShaderParameter.h"
#include "ParticleTemplateResources.h"
#include "ParticleRendererCallbacks.h"
#include "MeshParticleMesh/MeshParticleMesh.h"          // G1: the mesh renderer draws through it
#include "MeshParticleMesh/MeshParticleMeshTemplate.h"  // G1: g_meshParticleMeshEffectState
#include "../../Game/Objects/Fx/IEffectController.h"
#include "../../Game/Objects/IObject.h"
#include "../../Game/Objects/ObjectLayoutAccess.h"   // psmOwnerRootDisabled - typed (RW-266)
#include "../../Scene/Object/WindManager.h"
#include "../../Physics/HeightmapCluster.h"

#include "../../Game/Main/IVideoOptions.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/TextureAtlas/TextureAtlas.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include <string>
#include "../../RendMisc/IRenderView.h"
#include "../../RendMisc/IRenderer.h"

#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <string.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#if !defined(_M_IX86)
#include <xmmintrin.h>
#endif

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_rendererBase;       // RendDX9Init.cpp, dword_1023B970
extern void* g_raShaderConstants;  // RenderQueues.cpp, dword_1023DDBC
extern void* g_shaderManager;      // ShaderManager.cpp
// Existing direct BufferManager seams; these calls never dispatch through a particle vtable.
int bufferManagerSwapPool(int value);
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                     unsigned int usage, const char* name);
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage, unsigned int format);
void* indexBufferLock(void* wrapper, unsigned int offset, unsigned int count, unsigned int flags);
bool indexBufferUnlock(void* wrapper);
void indexBufferBind(void* wrapper);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool vertexBufferUnlock(void* wrapper);
void vertexBufferBindStream(void* wrapper, unsigned int stream);
void vertexDeclarationBind(void* wrapper);
void rendererDrawPrimitive(unsigned int primCount, unsigned int type, unsigned int startVertex);
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                 unsigned int minIndex, unsigned int numVertices, unsigned int startIndex);
// Buffer ownership operations used during particle level teardown.
void vertexBufferRelease(void* wrapper);
void indexBufferRelease(void* wrapper);
}

namespace geom {

extern LightManager* g_LightManager;                 // ModuleGeom.cpp, dword_1024B914
extern IBase* g_ParticleSystemManager;               // ModuleGeom.cpp, dword_1024B900

// getClassList: sub_100518A0(821017, "dice.hfe.geom.ParticleSystemManager", factory). getSingletonList
// { "ParticleSystemManager", &g_ParticleSystemManager, iid 820016, classId 821017, flags 2 }.
// queryInterface (0x100D7440) accepts iid 1 (IBase) and 820016.
extern const unsigned int CID_ParticleSystemManager  = 821017u;
extern const unsigned int IID_IParticleSystemManager = 820016u;

// The retail object is 0x8CB8 bytes (the factory's engine-alloc, push 8CB8h; 0x100B070F). The
// native object holds std containers, so it is allocated at sizeof and placement-constructed.

// The manager's internal instance lists: the render buckets and the two pending queues.
typedef std::vector<IParticleSystemInstance*> ParticleSystemInstanceVector;

// Members in shipped order, placed from the constructor/destructor and public-body accesses; the
// `+0xNN` comments are the retail offsets. The active-instance list is the std::vector that
// getInstances (+0x38) hands to BF2.exe. The two template registries the retail
// object embeds at +0xC0 and +0xF8 are the file-scope maps below. The four embedded renderers stay
// opaque byte spans in their recovered layout (x64 placement: ParticleRawLayout.h); their pointer
// lists are std::vectors constructed in place (particleRendererList).
struct ParticleSystemManager
{
    int release();
    void* deletingDestructor(int flags);

    /* +0x00 */ const void**              m_vptr;
    /* +0x04 */ int                       m_refCount;
    /* +0x08 */ float                     m_deltaTime;
    /* +0x0C */ unsigned char             m_paused;
    /* +0x0D */ unsigned char             m_pad0D[3];
    /* +0x10 */ ParticleSystemInstanceList m_instances;
    /* +0x20 */ ParticleSystemInstanceVector m_renderBuckets[10];
    /* +0xCC */ ParticleSystemInstanceVector m_pendingDestroy;
    /* +0xDC */ ParticleSystemInstanceVector m_pendingClassification;
    /* +0xEC */ std::set<IParticleSystemInstance*> m_pendingReclassification;
    /* +0x104 */ void*                     m_type0PoolBacking;
    /* +0x108 */ alignas(void*) unsigned char m_particleSystemRenderer[kPsmSpriteRendererSize];
    /* +0x5F20 */ void*                     m_type0FreeList;
    /* +0x5F24 */ int                       m_type0PoolCount;
    /* +0x5F28 */ unsigned int              m_type0LiveCount;    // [INFERRED] name: pool items in use
    /* +0x5F2C */ void*                     m_type1PoolBacking;
    /* +0x5F30 */ alignas(void*) unsigned char m_meshParticlesRenderer[kPsmMeshRendererSize];
    /* +0x7CC0 */ void*                     m_type1FreeList;
    /* +0x7CC4 */ int                       m_type1PoolCount;
    /* +0x7CC8 */ void*                     m_type2PoolBacking;
    /* +0x7CCC */ alignas(void*) unsigned char m_trailRenderer[kPsmTrailRendererSize];
    /* +0x85E0 */ void*                     m_type2FreeList;
    /* +0x85E4 */ int                       m_type2PoolCount;
    /* +0x85E8 */ void*                     m_type3PoolBacking;
    /* +0x85EC */ void*                     m_type3FreeList;
    /* +0x85F0 */ int                       m_type3PoolCount;
    /* +0x85F4 */ void*                     m_type4PoolBacking;
    /* +0x85F8 */ alignas(void*) unsigned char m_nsaParticleSystemRenderer[kPsmNsaRendererSize];
    /* +0x8B98 */ void*                     m_type4FreeList;
    /* +0x8B9C */ int                       m_type4PoolCount;
    /* +0x8BA0 */ unsigned int              m_type4LiveCount;    // [INFERRED] name: pool items in use
    /* +0x8BA4 */ int                       m_poolCount8BA4;     // applySettings: profile poolCount30
    /* +0x8BA8 */ unsigned char            m_forceSortedParticleType;
    /* +0x8BA9 */ unsigned char            m_pad8BA9;
    /* +0x8BAA */ unsigned short           m_nextTemplateId;
    /* +0x8BAC */ float                    m_lodScales[3];
    /* +0x8BB8 */ float                    m_baseDistances[3];
    /* +0x8BC4 */ float                    m_inverseNearRanges[3];
    /* +0x8BD0 */ float                    m_nearDistances[3];
    /* +0x8BDC */ float                    m_nearRatio;
    /* +0x8BE0 */ float                    m_inverseFarRanges[3];
    /* +0x8BEC */ float                    m_farDistances[3];
    /* +0x8BF8 */ float                    m_farRatio;
    /* +0x8BFC */ float                    m_tierThresholds[3];
    /* +0x8C08 */ float                    m_value8C08;
    /* +0x8C0C */ IBase*                   m_windManager;
    /* +0x8C10 */ unsigned char            m_windEnabled;
    /* +0x8C11 */ unsigned char            m_pad8C11[3];
    /* +0x8C14 */ IBase*                   m_heightmapCluster;
    /* +0x8C18 */ unsigned char            m_opaque8C18[0x2C];
    // [INFERRED] names: per-renderer draw-call counters (+0x8C44 sorted sprites, +0x8C48 NSA,
    // +0x8C4C additive sprites, +0x8C50 trails), each incremented by psmCountParticleDraw.
    /* +0x8C44 */ unsigned int             m_drawCounters[4];
    /* +0x8C54 */ unsigned char            m_opaque8C54[0x34];
    /* +0x8C88 */ unsigned int             m_value8C88;
    /* +0x8C8C */ unsigned int             m_value8C8C;
    /* +0x8C90 */ unsigned char            m_tearingDown;
    /* +0x8C91 */ unsigned char            m_state8C91;
    /* +0x8C92 */ unsigned char            m_poolsInitialized;
    /* +0x8C93 */ unsigned char            m_settingsApplied;
    /* +0x8C94 */ unsigned char            m_pad8C94[8];
    /* +0x8C9C */ float                    m_cachedLightValue;
    /* +0x8CA0 */ float                    m_effectiveViewDistance;
    /* +0x8CA4 */ float                    m_value8CA4;
    /* +0x8CA8 */ float                    m_distanceFactor;
    /* +0x8CAC */ unsigned int             m_shaderQuality;
    /* +0x8CB0 */ unsigned int             m_qualityLevel;
    /* +0x8CB4 */ void*                    m_textureAtlasHelper;
};

namespace {

// PE .CRT initializer 0x101D23D0 / destructor 0x101D8FC0, global 0x10258F3C. Every
// observed use copies this string; its source-level name remains [INFERRED].
const std::string g_particleAtlasPrefix("Objects/Effects/Textures/Atlas/");

// Native-offset renderer helpers (the x86 source fixtures address fields through them); this TU
// reaches renderer fields through the per-renderer layouts of ParticleRawLayout.h.
inline unsigned int& psmRendererDword(unsigned char* renderer, unsigned int offset)
{
    return *reinterpret_cast<unsigned int*>(renderer + offset);
}

// Ends the lifetime of a renderer pointer list the renderer constructor built in place.
template<class T>
void psmDestroyRendererList(unsigned char* renderer, unsigned int offset)
{
    typedef std::vector<T> List;
    particleRendererList<T>(renderer, offset).~List();
}

// Both retail end helpers temporarily set x87 rounding to truncate before fistp.  Renderer counts
// are non-negative and remain in signed range in every practical path; this spelling preserves the
// observable truncation and the subsequent unsigned setnbe comparison without importing CRT x87
// helpers across the plugin boundary.
inline unsigned int psmScaledLimit(unsigned int capacity, float factor)
{
    return static_cast<unsigned int>(
        static_cast<int>(static_cast<double>(capacity) * static_cast<double>(factor)));
}

} // anonymous namespace

void particleSystemManagerBeginFrame(IParticleSystemManager* managerInterface)
{
    ParticleSystemManager* const manager =
        reinterpret_cast<ParticleSystemManager*>(managerInterface);
    unsigned char* const particles = manager->m_particleSystemRenderer;
    unsigned char* const nsa = manager->m_nsaParticleSystemRenderer;

    // 0x100D2E80 -> 0x1016F6C0, then tail-call 0x1016D580.  All seven writes are
    // unconditional in retail, including before the manager's pool initializer has run.
    particles[0x0000] = 1;
    psmSpriteField<unsigned int>(particles, 0x1C2C) = 0;
    psmSpriteField<unsigned int>(particles, 0x1C30) = 0;
    psmSpriteField<unsigned int>(particles, 0x5E14) = 0;
    nsa[0x0000] = 1;
    psmNsaField<unsigned int>(nsa, 0x0420) = 0;
    psmNsaField<unsigned int>(nsa, 0x0444) = 0;
}

void particleSystemManagerEndFrame(IParticleSystemManager* managerInterface)
{
    ParticleSystemManager* const manager =
        reinterpret_cast<ParticleSystemManager*>(managerInterface);
    // PE helpers 0x1016F6E0 / 0x1016D5A0 intentionally read the scale through the published
    // singleton, rather than through their embedded-renderer owner.  Preserve that quirk.
    ParticleSystemManager* const published =
        reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    const float factor = published->m_value8C08;

    unsigned char* const particles = manager->m_particleSystemRenderer;
    particles[0x0000] = 0;
    const unsigned int particleLimit =
        psmScaledLimit(psmSpriteField<unsigned int>(particles, 0x1E10), factor);
    const unsigned int particleCount =
        psmSpriteField<unsigned int>(particles, 0x1C2C) + psmSpriteField<unsigned int>(particles, 0x1C30);
    particles[0x1C34] = particleCount > particleLimit;

    unsigned char* const nsa = manager->m_nsaParticleSystemRenderer;
    nsa[0x0000] = 0;
    const unsigned int nsaLimit = psmScaledLimit(psmNsaField<unsigned int>(nsa, 0x0004), factor);
    nsa[0x0424] = psmNsaField<unsigned int>(nsa, 0x0420) > nsaLimit;
}

// Defined in the emitter-type .cpp files: the recovered per-type constructors. Each takes the
// pre-allocated block (type-specific size) and the template-name std::string (std::string*),
// installs the vtable + defaults, and returns the object.
void* constructParticleSystemTemplateType0(void* mem, const void* nameString);   // 0x2A4, sub_10177010
void* constructParticleSystemType1(void* mem, const void* nameString);           // 0x238, sub_10179B70
// G1: the type-1 descriptor's lazy geometry fetch, ParticleSystemTemplate-
// Type1.cpp owns it; the type-1 instance constructor (10184BF0) calls it while descriptor +0x8C is null.
void particleSystemType1ResolveGeometry(void* descriptor);
void* constructParticleSystemType2(void* mem, const void* nameString);           // 0x250, sub_10187F00 (TrailSystemTemplate)
void* constructParticleSystemType3(void* mem, const void* nameString);           // 0x1A8, sub_10186AA0
void* constructParticleSystemType4(void* mem, const void* nameString);           // 0x27C, sub_101788F0 (NSAParticleSystemTemplate)

namespace {

// The complete 18-slot primary vtable (0x10219978). Slots 0-3 are the reference spine and slot 17
// is the deleting destructor. Known-signature frontiers below retain their real argument lists so
// even the diagnostic continue mode cannot corrupt ESP.

// +0x00 addRef, +0x04 getRefCount - the shared reference-count thunks (0x1000CB20 / 0x101A9A10).
int RD9_THUNK psm_addRef(void* self RD9_THUNK_EDX) { return ++static_cast<ParticleSystemManager*>(self)->m_refCount; }
int RD9_THUNK psm_getRc (void* self RD9_THUNK_EDX) { return   static_cast<ParticleSystemManager*>(self)->m_refCount; }

// +0x08 release (0x100D7480): decrement, and at zero invoke the deleting destructor (slot 17).
int RD9_THUNK psm_release(void* self RD9_THUNK_EDX)
{
    return static_cast<ParticleSystemManager*>(self)->release();
}

// +0x0C queryInterface (0x100D7440): accepts iid 1 (IBase) and 820016.
void* RD9_THUNK psm_qi(void* self RD9_THUNK_EDX, unsigned int iid)
{
    if (iid == RD9_IID_IBASE || iid == IID_IParticleSystemManager)
        return self;
    return 0;
}

// +0x20, the BF2 build deliberately does nothing, but it does consume the template
// pointer (`retn 4`). The old stand-in used a no-argument ret and was ABI-wrong.
void RD9_THUNK psm_removeParticleSystem(void* RD9_THUNK_EDX, IParticleSystemTemplate*) { }

// +0x30 resetStats(), the BF2 build's genuine no-argument nullsub. The Editor body
// clears counters and its console registration supplies the exact name.
void RD9_THUNK psm_resetStats(void* RD9_THUNK_EDX) { }

// +0x10, virtual-call resetStats(), then reacquire WindManager. `postLoadLevel` is NOT
// this slot: Editor ResourceManager::postLoadLevel names and directly calls the 0x101C3470 twin of
// BF2 0x100D2FD0, which only reacquires HeightmapCluster at +0x8C14. The ctor invokes this +0x10
// behavior inline, so `init` is the supported interpretation; the exact source spelling remains
// unresolved. BF2 0x100D38D0 is instead the unnamed lazy pool/atlas initializer.
void RD9_THUNK psm_reserved10(void* self RD9_THUNK_EDX)
{
    reinterpret_cast<IParticleSystemManager*>(self)->resetStats();
    renddx9abi::HostString name("WindManager");
    static_cast<ParticleSystemManager*>(self)->m_windManager =                // +0x8C0C
        classManager->getSingleton(name);
}

// Concrete instance constructors for the five particle-system types.
IParticleSystemInstance* psmCreateType0Instance(IParticleSystemTemplate* descriptor);
IParticleSystemInstance* psmCreateType1Instance(IParticleSystemTemplate* descriptor);
IParticleSystemInstance* psmCreateType2Instance(IParticleSystemTemplate* descriptor);
IParticleSystemInstance* psmCreateType3Instance(IParticleSystemTemplate* descriptor);
IParticleSystemInstance* psmCreateType4Instance(IParticleSystemTemplate* descriptor);
const char* psmRegisteredTemplateName(IParticleSystemTemplate* descriptor);
IParticleSystemInstance* RD9_THUNK psm_createInstance(void* self RD9_THUNK_EDX, IParticleSystemTemplate* systemTemplate)
{
    ParticleSystemManager* const manager = static_cast<ParticleSystemManager*>(self);
    if (!manager->m_poolsInitialized)
        particleSystemManagerPreLoadLevel(reinterpret_cast<IParticleSystemManager*>(manager));

    // the null check follows pool initialization and returns null exactly.
    if (!systemTemplate)
        return 0;

    const int type=systemTemplate->getSystemType();
    // Unsigned type validation rejects negative values as well as values above four.
    if (static_cast<unsigned int>(type) <= 4u)
    {
        IParticleSystemInstance* const instance = type == 0
            ? psmCreateType0Instance(systemTemplate) : type == 1
            ? psmCreateType1Instance(systemTemplate) : type == 2
            ? psmCreateType2Instance(systemTemplate) : type == 3
            ? psmCreateType3Instance(systemTemplate) : psmCreateType4Instance(systemTemplate);
        if (instance)
        {
            psmInstance<unsigned char>(instance, 5) = 0;
            manager->m_instances.push_back(instance);
        }
        return instance;
    }

    // Invalid template types log and return null.
    ::renddx9abi::logf("ParticleSystemManager.cpp:470 Invalid template type");
    return 0;
}

// The draw and trail renderers skip an instance whose owner's root object is disabled: IObject m_flags
// bit 0, which the shipped code reads as the byte at +4. IObject's prefix is
// retail-aligned in both flavours, so the flag is read through ObjectLayoutAccess (same bit, dword read).
inline bool psmOwnerRootDisabled(void* owner)
{
    return (world::object_detail::LayoutAccess::flags(
                static_cast<world::IObject*>(staticMeshRootParent(owner))) & 1u) != 0;
}

#include "ParticleManagerRuntime.inl"
#include "ParticleInstanceType0.inl"
#include "ParticleInstanceType0Update.inl"
#include "ParticleInstanceType0Emission.inl"
#include "ParticleInstanceType4.inl"
#include "ParticleInstanceType4Update.inl"
#include "ParticleInstanceType4Emission.inl"
#include "ParticleInstanceType2.inl"
#include "ParticleInstanceType2Update.inl"
#include "ParticleInstanceType2Emission.inl"
// G1: the mesh (type 1) and emitter (type 3) instances. The shared file first - both
// tables point at its eleven bodies by address.
#include "ParticleInstanceShared13.inl"
#include "ParticleInstanceType1.inl"
#include "ParticleInstanceType1Update.inl"
#include "ParticleInstanceType1Emission.inl"
#include "ParticleInstanceType3.inl"
#include "ParticleInstanceType3Update.inl"
#include "ParticleInstanceType3Emission.inl"

// +0x18, 0x100D4F40. Instances with live particles stop emitting and detach from their owner.
// Otherwise detach the controller before queuing inactive removal and destruction.
void RD9_THUNK psm_removeInstance(void* self RD9_THUNK_EDX, IParticleSystemInstance* instance)
{
    ParticleSystemManager* const manager = static_cast<ParticleSystemManager*>(self);
    if (!manager->m_poolsInitialized || manager->m_tearingDown)
        return;

    if (static_cast<unsigned int>(instance->reserved39()) > 0)
    {
        if (instance->getEffectController())
            instance->getEffectController()->decNumEmittersPlaying();
        instance->reserved08();
        instance->reserved04();
    }
    else
    {
        instance->setEffectController(0);
        if (psmInstance<unsigned char>(instance, 4)) psmQueueInactive(manager, instance);
        psmPush(manager->m_pendingDestroy, instance);
    }
}

// +0x24, stop/release every instance, advance each by 0.1, drain twice, then assert
// that the active list is empty. Both loops retain the same initial instance-count snapshot.
void RD9_THUNK psm_reserved24(void* self RD9_THUNK_EDX)
{
    ParticleSystemManager* const manager = static_cast<ParticleSystemManager*>(self);
    psmRemoveActive(manager, false);
    psmDrainDestroy(manager);
    const unsigned int count = static_cast<unsigned int>(manager->m_instances.size());
    for (unsigned int i = 0; i < count; ++i) manager->m_instances[i]->reserved02();
    for (unsigned int i = 0; i < count; ++i) manager->m_instances[i]->reserved41(0.1f);
    psmRemoveActive(manager, false);
    psmDrainDestroy(manager);
    if (manager->m_instances.size())
        psmRuntimeError(manager, "dice::hfe::geom::ParticleSystemManager::releaseAllInstances "
                                "(Debug: All particle systems have not been released!)");
}

void RD9_THUNK psm_onPathDraw(void* self RD9_THUNK_EDX, bool firstPerson, bool secondPass);

// +0x34, flip byte +0x0C, then broadcast it to active instances in vector order.
void RD9_THUNK psm_togglePause(void* self RD9_THUNK_EDX)
{
    ParticleSystemManager* manager = static_cast<ParticleSystemManager*>(self);
    manager->m_paused = manager->m_paused ? 0 : 1;

    const unsigned int count = static_cast<unsigned int>(manager->m_instances.size());
    for (unsigned int i = 0; i < count; ++i)
        manager->m_instances[i]->setPaused(manager->m_paused != 0);
}

// +0x40, 0x100D74A0: `vector@this+0x7CB0.resize(0, 0)` - the MeshParticles renderer's retained
// mesh list (renderer +0x1D80). Its elements are pointer-sized and have trivial destruction, so the
// exact observable effect is to retain storage and empty the list.
void RD9_THUNK psm_reserved40(void* self RD9_THUNK_EDX)
{
    meshParticlesRendererMeshes(static_cast<ParticleSystemManager*>(self)->m_meshParticlesRenderer).clear();
}

// The two template registries. The retail object embeds a std::map<std::string,ParticleSystem*>
// (by name; the find/insert use stricmp, so it is case-insensitive) at this+196 and a
// std::map<uint16,ParticleSystem*> (by id). The PSM is a singleton (getSingletonList flags 2), so we
// model both here at file scope - only PSM methods ever touch them. The id counter mirrors the
// retail this+35754 word. [CHOSEN, not recovered] internal representation of the recovered maps.
struct ParticleSystemNameLess
{
    bool operator()(const std::string& left, const std::string& right) const
    {
        return ::_stricmp(left.c_str(), right.c_str()) < 0;
    }
};

typedef std::map<std::string, IParticleSystemTemplate*, ParticleSystemNameLess>
    ParticleSystemNameMap;

ParticleSystemNameMap g_templatesByName;

// [CHOSEN, not recovered] diagnostic identity only, read from the registry key rather than through
// a descriptor's getName.
const char* psmRegisteredTemplateName(IParticleSystemTemplate* descriptor)
{
    for (ParticleSystemNameMap::const_iterator it = g_templatesByName.begin();
         it != g_templatesByName.end(); ++it)
        if (it->second == descriptor)
            return it->first.c_str();
    return "<unregistered>";
}
std::map<unsigned short, IParticleSystemTemplate*> g_templatesById;
#include "ParticleInstanceType3Registry.inl"   // G1: the emitter's child lookup (PE 100D3410)

// +0x44, scalar deleting destructor 0x100D7880. Its scalar wrapper is simple, but the complete
// destructor it calls (0x100D5610) owns the three trees, thirteen vectors, four embedded renderers,
// five pool buffers and TextureAtlas. Running that body over this replacement's
// value-initialised, partly unconstructed subobjects would itself be fabricated behavior. Free the
// inert object only while no deferred owner exists; otherwise stop exactly at the first missing
// teardown use. A list "owns storage" when its capacity is non-zero.
bool psmHasUnrecoveredOwnedState(ParticleSystemManager* self)
{
    if (!g_templatesByName.empty() || !g_templatesById.empty()
        || self->m_instances.capacity() || self->m_pendingDestroy.capacity()
        || self->m_pendingClassification.capacity() || self->m_poolsInitialized
        || self->m_type0PoolBacking || self->m_type1PoolBacking
        || self->m_type2PoolBacking || self->m_type3PoolBacking
        || self->m_type4PoolBacking || self->m_type0FreeList
        || self->m_type1FreeList || self->m_type2FreeList
        || self->m_type3FreeList || self->m_type4FreeList
        || self->m_textureAtlasHelper)
        return true;

    // Constructed renderers own allocations, declaration/effect handles and Trail's registered
    // listener even before pool allocation. They must reach the existing teardown frontier too.
    if (*reinterpret_cast<void* const*>(self->m_trailRenderer)
        || particleRendererList<unsigned char*>(self->m_particleSystemRenderer, psmSpriteRendererOffset(0x08)).capacity()
        || psmSpriteField<void*>(self->m_particleSystemRenderer, 0x1DC0)
        || psmSpriteField<void*>(self->m_particleSystemRenderer, 0x1DC8)
        || psmSpriteField<void*>(self->m_particleSystemRenderer, 0x1E0C)
        || psmSpriteField<void*>(self->m_particleSystemRenderer, 0x1DD0)
        || particleRendererList<unsigned char*>(self->m_nsaParticleSystemRenderer, psmNsaRendererOffset(0x0C)).capacity()
        || psmNsaField<void*>(self->m_nsaParticleSystemRenderer, 0x55C)
        || psmNsaField<void*>(self->m_nsaParticleSystemRenderer, 0x564)
        || psmNsaField<void*>(self->m_nsaParticleSystemRenderer, 0x59C)
        || psmNsaField<void*>(self->m_nsaParticleSystemRenderer, 0x56C))
        return true;

    for (unsigned int i = 0; i < 10; ++i)
        if (self->m_renderBuckets[i].capacity())
            return true;

    // The pending-reclassification set is constructed with the manager and owns its sentinel even
    // when the logical tree is empty, so a constructed manager always reaches the frontier.
    return true;
}

void* RD9_THUNK psm_deletingDtor(void* rawSelf RD9_THUNK_EDX, int flags)
{
    return static_cast<ParticleSystemManager*>(rawSelf)->deletingDestructor(flags);
}

// 0x100D3460, called by +0x3C after copying the quality profile. The distance math and repeated
// LightManager read are exact. Retail dereferences that singleton unconditionally. Keep an explicit
// publication-failure stop for diagnostics, then use the typed evidence-scoped interface view.
void psmRecomputeDerivedSettings(ParticleSystemManager* self)
{
    for (unsigned int i = 0; i < 3; ++i)
    {
        const float lodScale = self->m_lodScales[i];
        const float view = self->m_effectiveViewDistance;
        const float nearRatio = self->m_nearRatio;
        const float farRatio = self->m_farRatio;
        const float one = 1.0f;
        float* const base = &self->m_baseDistances[i];
        float* const nearDistance = &self->m_nearDistances[i];
        float* const inverseNear = &self->m_inverseNearRanges[i];
        float* const farDistance = &self->m_farDistances[i];
        float* const inverseFar = &self->m_inverseFarRanges[i];
        // PE 100D3468..100D357A uses FST, not fstp: the products remain at the
        // caller's x87 precision even though rounded copies are published to the fields.
        // The inverse denominators reload the rounded base and subtract the retained product.
#if defined(_M_IX86)
        __asm {
            fld lodScale
            fmul view
            mov eax, base
            fst dword ptr [eax]
            fmul nearRatio
            mov ecx, nearDistance
            fst dword ptr [ecx]
            fld dword ptr [eax]
            fsub st(0), st(1)
            fdivr one
            mov edx, inverseNear
            fstp dword ptr [edx]
            fstp st(0)
            fld farRatio
            fmul dword ptr [eax]
            mov ecx, farDistance
            fst dword ptr [ecx]
            fld dword ptr [eax]
            fsub st(0), st(1)
            fdivr one
            mov edx, inverseFar
            fstp dword ptr [edx]
            fstp st(0)
        }
#else
        // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates). The products stay
        // unrounded doubles; only the published copies are rounded to float, as with FST.
        const double product = static_cast<double>(lodScale) * view;
        *base = static_cast<float>(product);
        const double nearProduct = product * nearRatio;
        *nearDistance = static_cast<float>(nearProduct);
        *inverseNear = static_cast<float>(one / (static_cast<double>(*base) - nearProduct));
        const double farProduct = static_cast<double>(farRatio) * *base;
        *farDistance = static_cast<float>(farProduct);
        *inverseFar = static_cast<float>(one / (static_cast<double>(*base) - farProduct));
#endif
    }

    if (!g_LightManager)
    {
        RD9_FRONTIER_AT("dice::hfe::geom::ParticleSystemManager::applySettings "
                        "(LightManager singleton publication failed)", self, 0x3C);
        std::abort();
    }

    // PE snapshots the old cache before the first callback and reloads the published
    // LightManager for the second callback (100D357A..100D35B3).
    const float previousLight = self->m_cachedLightValue;
    const bool changed = previousLight != g_LightManager->getDefaultEffectLightAffectionFactor();
    self->m_cachedLightValue = g_LightManager->getDefaultEffectLightAffectionFactor();

    if (changed)
    {
        for (ParticleSystemNameMap::iterator it =
                 g_templatesByName.begin(); it != g_templatesByName.end(); ++it)
            it->second->refreshDerivedState();
    }
}

// +0x3C applySettings, 0x100D37A0. Copies all fifteen profile words, computes the effective view
// through Renderer +0x74 / VideoOptions +0x28, conditionally updates the five pool capacities, then
// runs 0x100D3460 above and sets +0x8C93. A missing LightManager stops at the named dependency;
// retail has no null guard there either.
void RD9_THUNK psm_applySettings(void* rawSelf RD9_THUNK_EDX, const ParticleSystemSettings* config)
{
    ParticleSystemManager* const self = static_cast<ParticleSystemManager*>(rawSelf);
    self->m_distanceFactor = config->distanceFactor;                       // +0x8CA8

    const int gsViewDistance = dx9rend::readIntSetting("GSViewDistance", 500);
    IVideoOptions* const videoOptions = dx9rend::rendererGetVideoOptions();
    const float rendererFactor = videoOptions->getViewDistanceScale();
    double effectiveView = rendererFactor * static_cast<float>(gsViewDistance) *
                           config->distanceFactor;
    if (effectiveView <= 200.0)
        effectiveView = 200.0;
    self->m_effectiveViewDistance = static_cast<float>(effectiveView);   // +0x8CA0

    for (unsigned int i = 0; i < 3; ++i)
    {
        self->m_tierThresholds[i] = config->tierThresholds[i];             // +0x8BFC
        self->m_lodScales[i] = config->lodScales[i];                       // +0x8BAC
    }
    self->m_nearRatio = config->nearRatio;                                 // +0x8BDC
    self->m_farRatio = config->farRatio;                                   // +0x8BF8

    if (!self->m_poolsInitialized)                                         // +0x8C92
    {
        self->m_type0PoolCount = config->poolCount04;                      // +0x5F24
        self->m_type2PoolCount = config->poolCount0C;                      // +0x85E4
        self->m_type4PoolCount = config->poolCount04;                      // +0x8B9C
        self->m_type1PoolCount = config->poolCount08;                      // +0x7CC4
        self->m_poolCount8BA4 = config->poolCount30;                       // +0x8BA4
    }

    self->m_shaderQuality = config->shaderQuality;                         // +0x8CAC
    self->m_qualityLevel = config->qualityLevel;                           // +0x8CB0
    psmRecomputeDerivedSettings(self);
    self->m_settingsApplied = 1;                                           // +0x8C93
}


// These are the five non-polymorphic particle record constructors, not the runtime-instance
// constructors behind createInstance. Only the words written by their instructions are touched;
// in particular, the free-chain word is assigned by the manager after all items are constructed.
void psmConstructPoolRecord(unsigned int* item, unsigned int type)
{
    static const unsigned char zero0[] = { 3,4,6,7,8,13,14,18,19,20,24,25,26,27 };
    static const unsigned char zero4[] = {
        1,2,3,4,5,6,18,19,20,21,23,24,25,26,27,28,29,
        38,39,40,41,42,43,44,45,46,47,48,49 };
    static const unsigned char zero1[] = {
        1,5,2,6,3,7,4,8,9,10,11,12,13,14,15,16,
        17,18,19,20,21,22,29,30,31,32,33,34,35,39,40,41,42,43,44 };
    static const unsigned char zero2[] = {
        1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,
        23,24,25,26,27,28,29,35,36,37 };
    static const unsigned char zero3[] = {
        1,5,2,6,3,7,4,8,9,10,11,12,13,14,15,16,
        17,18,19,20,21,22,28,29,30,34,35,36,37,38,39 };
    const unsigned char* words = 0;
    unsigned int count = 0;
    switch (type)
    {
    case 0: words = zero0; count = sizeof(zero0); break;
    case 1: words = zero1; count = sizeof(zero1); break;
    case 2: words = zero2; count = sizeof(zero2); break;
    case 3: words = zero3; count = sizeof(zero3); break;
    case 4: words = zero4; count = sizeof(zero4); break;
    }
    for (unsigned int i = 0; i < count; ++i)
        item[words[i]] = 0;
    if (type == 1 || type == 3)
    {
        item[16] = 0x3F800000u;
        item[11] = 0x3F800000u;
        item[6] = 0x3F800000u;
        item[1] = 0x3F800000u;
    }
}

// new Record[count], followed by the shipped unsigned count-1 free-chain loop. There is no
// allocation-failure or zero-count guard in PE 0x100D38D0; neither is manufactured here.
// `retailStride` is the type's retail record size (148/192/168/176/200); x64 uses the native stride
// and free-chain link slot of ParticleRawLayout.h instead.
void psmAllocatePool(void*& backing, void*& freeList, int& count,
                     unsigned int retailStride, unsigned int type)
{
    const unsigned int stride = sizeof(void*) == 4 ? retailStride : psmRecordStride(type);
    unsigned char* const items = static_cast<unsigned char*>(
        ::renddx9abi::engineAlloc(stride * static_cast<unsigned int>(count)));
    if (items)
        for (int i = 0; i < count; ++i)
            psmConstructPoolRecord(reinterpret_cast<unsigned int*>(items + stride * i), type);
    backing = items;
    for (unsigned int i = 0; i < static_cast<unsigned int>(count) - 1u; ++i)
        psmRecordLink(items + stride * i, type) = items + stride * (i + 1u);
    psmRecordLink(items + stride * static_cast<unsigned int>(count) - stride, type) = 0;
    freeList = backing;
}

template <typename T>
T& psmRendererField(unsigned char* renderer, unsigned int offset)
{
    return *reinterpret_cast<T*>(renderer + offset);
}

// Direct TrailRenderer helper the index count is in 16-bit index units. Its buffer
// null gate, failed-lock behavior and unconditional unlock after a lock attempt are shipped.
void psmFillTrailIndices(unsigned char* renderer, unsigned int sides)
{
    const unsigned int count = psmTrailField<unsigned int>(renderer, 0x8AC);
    void* const buffer = psmTrailField<void*>(renderer, 0x8B8 + 4u * sides);
    if (!count || !buffer)
        return;
    unsigned short* const indices = static_cast<unsigned short*>(
        dx9rend::indexBufferLock(buffer, 0, 6u * sides * (count - 1u), 0));
    if (indices)
    {
        unsigned int out = 0;
        unsigned short base = 0;
        for (unsigned int segment = 0; segment < count - 1u; ++segment)
        {
            const unsigned short next = static_cast<unsigned short>(base + 2u * sides);
            for (unsigned int side = 0; side < sides; ++side)
            {
                const unsigned int offset = 2u * side;
                indices[out++] = static_cast<unsigned short>(base + offset);
                indices[out++] = static_cast<unsigned short>(base + offset + 1u);
                indices[out++] = static_cast<unsigned short>(next + offset + 1u);
                indices[out++] = static_cast<unsigned short>(base + offset);
                indices[out++] = static_cast<unsigned short>(next + offset + 1u);
                indices[out++] = static_cast<unsigned short>(next + offset);
            }
            base = static_cast<unsigned short>(base + 2u * sides);
        }
    }
    dx9rend::indexBufferUnlock(buffer);
}

#include "ParticleRendererConstruction.inl"

// The paired radial vertices intentionally coincide, and the angle step is
// 360/(2*sides), so this is a half-circle fan rather than a conventional full-circle ring.
void psmCreateTrailSide(unsigned char* renderer, unsigned int sides)
{
    const int oldPool = dx9rend::bufferManagerSwapPool(0);
    psmTrailField<void*>(renderer, 0x8B8 + 4u * sides) =
        dx9rend::bufferManagerCreateIndexBuffer(
            6u * sides * (psmTrailField<unsigned int>(renderer, 0x8AC) - 1u), 8u, 101u);
    dx9rend::bufferManagerSwapPool(oldPool);
    psmFillTrailIndices(renderer, sides);

    Vec3* const vertices = static_cast<Vec3*>(::renddx9abi::engineAlloc(24u * sides));
    if (vertices)
        for (unsigned int i = 0; i < 2u * sides; ++i)
            new (vertices + i) Vec3{ 0.0f, 0.0f, 0.0f };
    psmTrailField<Vec3*>(renderer, 0x8C8 + 4u * sides) = vertices;
    if (!vertices)
    {
        // TrailRenderer.cpp:233, DebugTypeWarning; the caller discards the false result.
        ::renddx9abi::logf("TrailRenderer.cpp:233: Failed to craete originalVertices");
        return;
    }
    const float step = static_cast<float>(360.0 / static_cast<double>(2u * sides));
    for (unsigned int side = 0; side < sides; ++side)
    {
        const double angle = (static_cast<double>(side) * step + step) *
                             static_cast<double>(0.01745329238474369049f);
        // The PE rounds the sin argument through a float local, while cos consumes the
        // retained x87 intermediate. Keep that otherwise surprising asymmetry.
        const float sine = static_cast<float>(std::sin(static_cast<float>(angle)));
        const float cosine = static_cast<float>(std::cos(angle));
        vertices[side * 2u] = Vec3{ cosine, sine, 0.0f };
        vertices[side * 2u + 1u] = Vec3{ cosine, sine, 0.0f };
    }
}

// one 68-byte-stride VB followed by the 1-, 2-, and 3-sided trail IBs.
void psmCreateTrailBuffers(unsigned char* renderer)
{
    const int oldPool = dx9rend::bufferManagerSwapPool(0);
    psmTrailField<unsigned int>(renderer, 0x898) = 6u * psmTrailField<unsigned int>(renderer, 0x8AC);
    psmTrailField<unsigned int>(renderer, 0x894) = 68;
    psmTrailField<unsigned int>(renderer, 0x89C) = 68u * psmTrailField<unsigned int>(renderer, 0x898);
    psmTrailField<void*>(renderer, 0x8B4) = dx9rend::bufferManagerCreateVertexBuffer(
        psmTrailField<unsigned int>(renderer, 0x898), 68, 0x208, "TrailRenderer");
    for (unsigned int sides = 1; sides <= 3; ++sides)
        psmCreateTrailSide(renderer, sides);
    dx9rend::bufferManagerSwapPool(oldPool);
}

// A field of the sprite renderer, or of the NSA renderer when `nsa`, by retail offset.
template <typename T>
T& psmParticleOrNsaField(unsigned char* renderer, bool nsa, unsigned int offset)
{
    return psmRawField<T>(renderer, nsa ? psmNsaRendererOffset(offset) : psmSpriteRendererOffset(offset));
}

// Allocation/reset helpers 0x1016F990, 0x1016D7C0, 0x10174AA0 and 0x10172ED0.
// They do not construct the embedded renderers or use their still-unrecovered shader state.
void psmInitializeParticleRenderer(unsigned char* renderer, int count, bool nsa)
{
    psmParticleOrNsaField<unsigned int>(renderer, nsa, nsa ? 0x004 : 0x1E10) = count;
    psmParticleOrNsaField<void*>(renderer, nsa, nsa ? 0x01C : 0x018) =
        ::renddx9abi::engineAlloc(kPsmPointerSize * static_cast<unsigned int>(count));
    if (!nsa)
        psmParticleOrNsaField<void*>(renderer, nsa, 0x01C) =
            ::renddx9abi::engineAlloc(kPsmPointerSize * static_cast<unsigned int>(count));
    const int oldPool = dx9rend::bufferManagerSwapPool(0);
    const unsigned int stride = nsa ? 72u : 64u;
    const unsigned int descriptor = nsa ? 0x550u : 0x1DB4u;
    psmParticleOrNsaField<unsigned int>(renderer, nsa, descriptor + 4) = 6u * static_cast<unsigned int>(count);
    psmParticleOrNsaField<unsigned int>(renderer, nsa, descriptor) = stride;
    psmParticleOrNsaField<unsigned int>(renderer, nsa, descriptor + 8) = stride * 6u * static_cast<unsigned int>(count);
    psmParticleOrNsaField<void*>(renderer, nsa, nsa ? 0x568 : 0x1DCC) =
        dx9rend::bufferManagerCreateVertexBuffer(6u * static_cast<unsigned int>(count),
            stride, 0x208, nsa ? "NSAParticleSystemRenderer" : "ParticleSystemRenderer");
    dx9rend::bufferManagerSwapPool(oldPool);
    const unsigned int counters = nsa ? 0x528u : 0x1D80u;
    psmParticleOrNsaField<unsigned int>(renderer, nsa, counters) = 0;
    psmParticleOrNsaField<unsigned int>(renderer, nsa, counters + 4) = 0;
    for (unsigned int i = 0; i < 4; ++i)
        psmParticleOrNsaField<float>(renderer, nsa, counters + 12 + i * 4) = 0.5f;
    psmParticleOrNsaField<unsigned int>(renderer, nsa, nsa ? 0x54C : 0x1DA4) = 0;
}

void psmInitializeMeshRenderer(unsigned char* renderer, int count)
{
    psmMeshField<unsigned int>(renderer, 0x1D78) = count;
    psmMeshField<void*>(renderer, 4) =
        ::renddx9abi::engineAlloc(kPsmPointerSize * static_cast<unsigned int>(count));
    psmMeshField<unsigned int>(renderer, 0x1D50) = 0;
    psmMeshField<unsigned int>(renderer, 0x1D54) = 0;
    for (unsigned int i = 0; i < 4; ++i)
        psmMeshField<float>(renderer, 0x1D5C + i * 4) = 0.5f;
    psmMeshField<unsigned int>(renderer, 0x1D7C) = 0;
}

void psmInitializeTrailRenderer(unsigned char* renderer, int count)
{
    psmTrailField<unsigned int>(renderer, 0x8AC) = count;
    psmTrailField<void*>(renderer, 24) =
        ::renddx9abi::engineAlloc(kPsmPointerSize * static_cast<unsigned int>(count));
    psmCreateTrailBuffers(renderer);
    psmTrailField<float>(renderer, 0x868) = 300.0f;
    psmTrailField<unsigned int>(renderer, 0x860) = 0;
    psmTrailField<unsigned int>(renderer, 0x864) = 0;
    for (unsigned int i = 0; i < 4; ++i)
        psmTrailField<float>(renderer, 0x874 + i * 4) = 0.5f;
    psmTrailField<unsigned int>(renderer, 0x890) = 0;
    psmTrailField<float>(renderer, 0x86C) =
        psmTrailField<float>(renderer, 0x868) * psmTrailField<float>(renderer, 0x868);
}

// Direct PE 0x100D38D0. The image allocates fresh backing without deleting the old backing;
// preserve that ordering and leak on repeated calls. Names here are [INFERRED] from the caller.
std::string psmAtlasPath(const std::string& filename)
{
    // The shipped operator+ expansion: preserve its copy/append/result-copy sequence from
    // 0x100D3D03..0x100D3D26, with the intermediate destroyed before loadTAI.
    std::string prefix(g_particleAtlasPrefix);
    return std::string(prefix += filename);
}

void psmPreLoadLevel(ParticleSystemManager* self)
{
    const int savedPriority = dx9rend::resourceManagerSwapDefaultPriority(5);
    if (!self->m_settingsApplied)
    {
        // The exact 60-byte profile at PE 0x10239124, including the unusual first value 500.
        static const ParticleSystemSettings defaults = {
            500.0f, 1000, 700, 600, { 0.4f, 0.55f, 0.7f }, { 0.3f, 0.6f, 1.0f },
            0.4f, 0.85f, 0, 3, 2 };
        reinterpret_cast<IParticleSystemManager*>(self)->applySettings(&defaults);
    }
    psmRecomputeDerivedSettings(self);
    psmAllocatePool(self->m_type0PoolBacking, self->m_type0FreeList, self->m_type0PoolCount, 148, 0);
    psmInitializeParticleRenderer(self->m_particleSystemRenderer, self->m_type0PoolCount, false);
    psmAllocatePool(self->m_type4PoolBacking, self->m_type4FreeList, self->m_type4PoolCount, 200, 4);
    psmInitializeParticleRenderer(self->m_nsaParticleSystemRenderer, self->m_type4PoolCount, true);
    psmAllocatePool(self->m_type1PoolBacking, self->m_type1FreeList, self->m_type1PoolCount, 192, 1);
    psmInitializeMeshRenderer(self->m_meshParticlesRenderer, self->m_type1PoolCount);
    psmAllocatePool(self->m_type2PoolBacking, self->m_type2FreeList, self->m_type2PoolCount, 168, 2);
    psmInitializeTrailRenderer(self->m_trailRenderer, self->m_type2PoolCount);
    self->m_type3PoolCount = 400;
    psmAllocatePool(self->m_type3PoolBacking, self->m_type3FreeList, self->m_type3PoolCount, 176, 3);

    if (dx9rend::readBoolSetting("GSUseEffectTextureAtlas", false))
    {
        const std::string filename("ParticleAtlas.tai");
        if (!filename.empty())
        {
            const std::string path = psmAtlasPath(filename);
            dx9rend::textureAtlasLoadTAI(
                static_cast<dx9rend::TextureAtlas*>(self->m_textureAtlasHelper), path);
        }
    }
    self->m_poolsInitialized = 1;
    dx9rend::resourceManagerSwapDefaultPriority(savedPriority);
}

// +0x1C createParticleSystem(type, templateName) (0x100D4D00). Look the template up by name (dedup;
// one descriptor per named effect); if absent, allocate the type's object, construct it, assign a
// 16-bit id, and register it in both maps. Type 0 is dice::hfe::geom::ParticleSystemTemplate
// (0x2A4 bytes); the other four emitter types are reconstructed as they are first reached.
// Returns the template pointer.
IParticleSystemTemplate* RD9_THUNK psm_createParticleSystem(void* self RD9_THUNK_EDX, int type, const ParticleSystemName* name)
{
    ParticleSystemManager* const manager = static_cast<ParticleSystemManager*>(self);
    const char* const rawName = name->c_str();
    const std::string key(rawName);

    ParticleSystemNameMap::iterator it = g_templatesByName.find(key);
    if (it != g_templatesByName.end())
        return it->second;              // the shipped "already present" early return

    if (manager->m_nextTemplateId >= 0x800u)
    {
        // Manager.cpp:371, DebugTypeError: "Too many ParticleSystemManager templates".
        RD9_FRONTIER_AT("dice::hfe::geom::ParticleSystemManager::createParticleSystem "
                        "(template id limit 0x800)", self, 0x1C);
        std::abort();
    }

    // Every descriptor constructor below takes the name as the renderer's own string, converted
    // once here from the host-facing name.
    const std::string internalName(rawName);
    const void* const renderName = &internalName;
    void* mem = 0;
    switch (type)
    {
    case 0:
        mem = ::renddx9abi::engineAlloc(PSM_DESCRIPTOR_SIZE(0x2A4u));
        if (mem) constructParticleSystemTemplateType0(mem, renderName);
        break;
    case 1:
        mem = ::renddx9abi::engineAlloc(PSM_DESCRIPTOR_SIZE(0x238u));
        if (mem) constructParticleSystemType1(mem, renderName);
        break;
    case 2:
        mem = ::renddx9abi::engineAlloc(PSM_DESCRIPTOR_SIZE(0x250u));
        if (mem) constructParticleSystemType2(mem, renderName);
        break;
    case 3:
        mem = ::renddx9abi::engineAlloc(PSM_DESCRIPTOR_SIZE(0x1A8u));
        if (mem) constructParticleSystemType3(mem, renderName);
        break;
    case 4:
        mem = ::renddx9abi::engineAlloc(PSM_DESCRIPTOR_SIZE(0x27Cu));
        if (mem) constructParticleSystemType4(mem, renderName);
        break;
    default:
        ::renddx9abi::logf("createParticleSystem: unknown particle type %d name='%s'", type, rawName);
        RD9_FRONTIER_AT("dice::hfe::geom::ParticleSystem unknown emitter type", self, 0x1C);
        }

    // No allocation-null or invalid-type guard follows the retail switch: both paths run into this
    // store through a null descriptor. Preserve that shipped defect rather than returning nullptr.
    IParticleSystemTemplate* const system = static_cast<IParticleSystemTemplate*>(mem);
    g_templatesByName[key] = system;

    const unsigned short id = manager->m_nextTemplateId;
    psmDescriptor<unsigned short>(mem, 4) = id;
    ++manager->m_nextTemplateId;
    g_templatesById[id]    = system;

    ::renddx9abi::logf("createParticleSystem: built template type=%d name='%s' id=%u at 0x%08X",
                       type, rawName, static_cast<unsigned>(id), ::renddx9abi::addr(system));
    return system;
}

// +0x38 getter (0x100D7470). The shipped body is literally `return this + 16`: a reference/pointer
// ABI return to the active-instance list (retail byte +0x10), the std::vector ParticleSystemManager.h
// declares.
ParticleSystemInstanceList* RD9_THUNK psm_getInstances(void* self RD9_THUNK_EDX)
{
    return &static_cast<ParticleSystemManager*>(self)->m_instances;
}

// +0x28 update(dt), PE 100D5150. No pool-initialized or paused manager guard exists here.
// Queue draining, preparation and all ten bucket walks retain their shipped order.
void RD9_THUNK psm_update(void* rawSelf RD9_THUNK_EDX, float dt)
{
    ParticleSystemManager* const self = static_cast<ParticleSystemManager*>(rawSelf);
    psmRemoveActive(self, false);
    psmDrainDestroy(self);
    psmReclassify(self);
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();
    psmPrepareClassifiedVectors(self, view);
    self->m_deltaTime = dt;
    const unsigned int order[10] = {7, 9, 8, 0, 1, 3, 5, 2, 4, 6};
    for (unsigned int i = 0; i < 10; ++i) psmUpdateBucket(self->m_renderBuckets[order[i]], dt);
    psmRemoveActive(self, false);
}

#include "ParticleRendererDraw.inl"

// Unlike the older BF2Render wrapper, this path has no chosen null guard.
// The view is acquired afresh on each call, including between the two first-person passes.
bool psmCameraIsUnderWater()
{
    dx9rend::IRenderView* const view =
        static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderView();
    const float* const matrix = reinterpret_cast<const float*>(view->getViewMatrixPtr());
    return matrix[13] < *reinterpret_cast<const float*>(dx9rend::raShaderConstantsAt(0x1BC));
}

// Each preparation updates the derived settings and temporarily expands the
// supplied view's far plane. Empty input still executes every renderer's preparation prologue.
void psmPreparePath(ParticleSystemManager* self, dx9rend::IRenderView* view,
                    bool firstPerson, bool alternateView, bool secondPass, bool underwater)
{
    psmRecomputeDerivedSettings(self);
    const float oldFarPlane = view->getFarPlane();
    view->setFarPlane(self->m_effectiveViewDistance);

    if (!firstPerson)
    {
        if (!secondPass)
            psmPrepareNsaParticleRenderer(self->m_nsaParticleSystemRenderer, view,
                &self->m_renderBuckets[0], self->m_deltaTime, false);

        const unsigned int waterSide = secondPass != underwater ? 1u : 0u;
        // PE 100F7B50 writes the shared particle-pass sign even when every bucket is empty.
        *reinterpret_cast<float*>(dx9rend::raShaderConstantsAt(0x1E0)) = waterSide ? -1.0f : 1.0f;
        psmPrepareParticleRenderer(self->m_particleSystemRenderer, view,
            &self->m_renderBuckets[1 + waterSide], self->m_deltaTime, false);
        psmPrepareMeshParticleRenderer(self->m_meshParticlesRenderer, view,
            &self->m_renderBuckets[3 + waterSide], self->m_deltaTime);
        psmPrepareTrailRenderer(self->m_trailRenderer, view,
            &self->m_renderBuckets[5 + waterSide], self->m_deltaTime);
    }
    else
    {
        psmPrepareNsaParticleRenderer(self->m_nsaParticleSystemRenderer, view,
            &self->m_renderBuckets[8], self->m_deltaTime, alternateView);
        psmPrepareParticleRenderer(self->m_particleSystemRenderer, view,
            &self->m_renderBuckets[7], self->m_deltaTime, alternateView);
        psmPrepareMeshParticleRenderer(self->m_meshParticlesRenderer, view,
            &self->m_renderBuckets[9], self->m_deltaTime);
    }

    view->setFarPlane(oldFarPlane);
}

// The selector/draw order is observable even with empty classified buckets;
// their genuine empty returns depend on the embedded renderer constructors and retained state.
void psmDrawPath(ParticleSystemManager* self, dx9rend::IRenderView* view, bool firstPerson)
{
    psmSelectParticleRenderer(self->m_particleSystemRenderer, 0);
    psmSelectNsaParticleRenderer(self->m_nsaParticleSystemRenderer, 0);
    psmSelectMeshParticleRenderer(self->m_meshParticlesRenderer, 0);
    psmDrawNsaParticleRenderer(self->m_nsaParticleSystemRenderer, view);
    psmDrawMeshParticleRenderer(self->m_meshParticlesRenderer, view, false);
    if (!firstPerson)
    {
        psmDrawMeshParticleExtras(self->m_meshParticlesRenderer, false);
        psmSelectTrailRenderer(self->m_trailRenderer, 0);
        psmDrawTrailRenderer(self->m_trailRenderer, view);
    }
    psmDrawParticleRenderer(self->m_particleSystemRenderer, view);
    psmDrawMeshParticleRenderer(self->m_meshParticlesRenderer, view, true);
    if (firstPerson)
        psmDrawMeshParticleExtras(self->m_meshParticlesRenderer, true);
}

// +0x2C, PE 100D4920. Both bool arguments follow the three BF2Render call sites. Only the
// uninitialized-pools return is shipped; initialized empty state takes the complete CPU path.
void RD9_THUNK psm_onPathDraw(void* rawSelf RD9_THUNK_EDX, bool firstPerson, bool secondPass)
{
    ParticleSystemManager* const self = static_cast<ParticleSystemManager*>(rawSelf);
    if (!self->m_poolsInitialized)
        return;

    // Retail manager +0x1E78/+0x1E7C, +0x8B14, +0x7C70/+0x7C74 and +0x851C/+0x8520: counters
    // inside the four embedded renderers.
    psmSpriteField<unsigned int>(self->m_particleSystemRenderer, 0x1D70) = 0;
    psmSpriteField<unsigned int>(self->m_particleSystemRenderer, 0x1D74) = 0;
    psmNsaField<unsigned int>(self->m_nsaParticleSystemRenderer, 0x51C) = 0;
    psmMeshField<unsigned int>(self->m_meshParticlesRenderer, 0x1D40) = 0;
    psmMeshField<unsigned int>(self->m_meshParticlesRenderer, 0x1D44) = 0;
    psmTrailField<unsigned int>(self->m_trailRenderer, 0x850) = 0;
    psmTrailField<unsigned int>(self->m_trailRenderer, 0x854) = 0;

    dx9rend::IRenderView* view =
        static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderViewDup();
    const float globalViewDistance =
        static_cast<float>(dx9rend::readIntSetting("GSViewDistance", 500));
    IVideoOptions* const videoOptions =
        static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getVideoOptions();
    const float rendererDistance = videoOptions->getViewDistanceScale();
    const float distanceFactor = self->m_distanceFactor;
    const float minimumDistance = 200.0f;
    float effective;
    // PE 100D49B9..100D49DA keeps both products at the current x87 precision and
    // selects 200 for unordered as well as <=200. Do not spill an intermediate to double.
#if defined(_M_IX86)
    __asm {
        fld rendererDistance
        fmul globalViewDistance
        fmul distanceFactor
        fld minimumDistance
        fcomp st(1)
        fnstsw ax
        test ah, 5
        jnp psm_distance_selected
        fstp st(0)
        fld minimumDistance
    psm_distance_selected:
        fstp effective
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates). Only an ordered
    // 200 < product keeps the product; NaN selects 200, as the C0/C2 parity test does.
    const double product = static_cast<double>(rendererDistance) * globalViewDistance * distanceFactor;
    effective = static_cast<double>(minimumDistance) < product ? static_cast<float>(product)
                                                                : minimumDistance;
#endif
    self->m_effectiveViewDistance = effective;

    bool underwater = psmCameraIsUnderWater();
    psmPreparePath(self, view, firstPerson, false, secondPass, underwater);
    psmDrawPath(self, view, firstPerson);
    if (firstPerson)
    {
        view = static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderView2();
        underwater = psmCameraIsUnderWater();
        psmPreparePath(self, view, firstPerson, true, secondPass, underwater);
        psmDrawPath(self, view, firstPerson);
    }
}

const void* const kVtable[18] =
{
    reinterpret_cast<void*>(&psm_addRef),        // +0x00
    reinterpret_cast<void*>(&psm_getRc),         // +0x04
    reinterpret_cast<void*>(&psm_release),       // +0x08
    reinterpret_cast<void*>(&psm_qi),            // +0x0C
    reinterpret_cast<void*>(&psm_reserved10),    // +0x10  call +30, reacquire WindManager
    reinterpret_cast<void*>(&psm_createInstance),// +0x14  createInstance             (all five types, G1)
    reinterpret_cast<void*>(&psm_removeInstance),// +0x18  removeInstance
    reinterpret_cast<void*>(&psm_createParticleSystem), // +0x1C create template      [CHOSEN maps]
    reinterpret_cast<void*>(&psm_removeParticleSystem), // +0x20 exact one-arg nullsub
    reinterpret_cast<void*>(&psm_reserved24),    // +0x24 release-all operation
    reinterpret_cast<void*>(&psm_update),        // +0x28 update
    reinterpret_cast<void*>(&psm_onPathDraw),    // +0x2C  onPathDraw; empty rendering recovered
    reinterpret_cast<void*>(&psm_resetStats),    // +0x30  resetStats (BF2 nullsub)
    reinterpret_cast<void*>(&psm_togglePause),   // +0x34  togglePause
    reinterpret_cast<void*>(&psm_getInstances),  // +0x38  active-vector getter
    reinterpret_cast<void*>(&psm_applySettings), // +0x3C  applySettings             recovered boot surface
    reinterpret_cast<void*>(&psm_reserved40),    // +0x40  resize aux vector to zero
    reinterpret_cast<void*>(&psm_deletingDtor),  // +0x44  deleting destructor       [FRONTIER if owned]
};

} // anonymous namespace

// G1: the type-3 descriptor's slot 89 (ParticleSystemTemplateType3.cpp, PE 101868F0)
// resolves its child through this manager's name registry at once - PE 100D3410, a manager member it
// calls on ds:1024B900. The registry is g_templatesByName (ParticleInstanceType3Registry.inl).
IParticleSystemTemplate* particleSystemManagerFindTemplateByName(const char* name)
{
    return psmFindTemplateByName(name);
}

void particleSystemManagerPreLoadLevel(IParticleSystemManager* manager)
{
    psmPreLoadLevel(reinterpret_cast<ParticleSystemManager*>(manager));
}

// PE 0x100D2FD0, named by the symbolized Editor ResourceManager::postLoadLevel caller.
// This singleton is borrowed: there is no addRef, release of the old value, or null guard.
void particleSystemManagerPostLoadLevel(IParticleSystemManager* manager)
{
    ParticleSystemManager* const self = reinterpret_cast<ParticleSystemManager*>(manager);
    const renddx9abi::HostString name("HeightmapCluster");
    self->m_heightmapCluster = classManager->getSingleton(name);
}

IBase* particleSystemManagerHeightmapCluster(IBase* manager)
{
    return reinterpret_cast<ParticleSystemManager*>(manager)->m_heightmapCluster;   // +0x8C14
}

unsigned char* particleSystemManagerMeshParticlesRenderer(IBase* manager)
{
    return reinterpret_cast<ParticleSystemManager*>(manager)->m_meshParticlesRenderer; // +0x5F30
}

unsigned char particleSystemManagerForceSortedParticles(IBase* manager)
{
    return reinterpret_cast<ParticleSystemManager*>(manager)->m_forceSortedParticleType; // +0x8BA8
}

// Releases level resources allocated by psmPreLoadLevel.
// Both level unload and the manager destructor must run this sweep.
namespace {

// sub_1016FA80 (92 B), on the ParticleSystemRenderer at manager+0x108. The inverse of
// psmInitializeParticleRenderer(nsa = false).
//   The +0x1C array is freed inside the +0x18 test, without a test of its own: the two arrays are
//     allocated together, and a renderer whose +0x18 is null keeps its +0x1C. As shipped.
//   The vertex-buffer slot is zeroed whenever it was non-null, whether or not the release was the
//     last reference (the store sits after the refcount branch's join).
void psmReleaseParticleRenderer(unsigned char* renderer)
{
    void*& first  = psmSpriteField<void*>(renderer, 0x018);
    void*& second = psmSpriteField<void*>(renderer, 0x01C);
    if (first)
    {
        ::renddx9abi::engineFree(first);
        ::renddx9abi::engineFree(second);                                  // sub_1005F370, unguarded
        first  = 0;
        second = 0;
    }
    void*& vertexBuffer = psmSpriteField<void*>(renderer, 0x1DCC);
    if (vertexBuffer)
    {
        dx9rend::vertexBufferRelease(vertexBuffer);                        // --[+4]; 10014510; free
        vertexBuffer = 0;
    }
}

// sub_10174520 (28 B), on the MeshParticles renderer at manager+0x5F30. The inverse of
// psmInitializeMeshRenderer: the one pointer array at +0x04.
void psmReleaseMeshRenderer(unsigned char* renderer)
{
    void*& items = psmMeshField<void*>(renderer, 0x04);
    if (items)
    {
        ::renddx9abi::engineFree(items);
        items = 0;
    }
}

// Releases the trail buffers and clears the segment count last.
// Vertex arrays are freed only when the corresponding index buffer exists; their slots stay dangling.
void psmReleaseTrailRenderer(unsigned char* renderer)
{
    void*& items = psmTrailField<void*>(renderer, 0x018);
    if (items)
    {
        ::renddx9abi::engineFree(items);
        items = 0;
    }
    void*& vertexBuffer = psmTrailField<void*>(renderer, 0x8B4);
    if (vertexBuffer)
    {
        dx9rend::vertexBufferRelease(vertexBuffer);                        // --[+4]; 10014510; free
        vertexBuffer = 0;
    }
    for (unsigned int side = 0; side < 3u; ++side)                         // [ebp-4] = 3, counted down
    {
        void*& indexBuffer = psmTrailField<void*>(renderer, 0x8BC + 4u * side);
        if (indexBuffer)
        {
            dx9rend::indexBufferRelease(indexBuffer);                      // --[+4]; 10014300; free
            void* const vertices = psmTrailField<void*>(renderer, 0x8CC + 4u * side);
            indexBuffer = 0;                                               // 10172FC8, before the free
            ::renddx9abi::engineFree(vertices);                            // sub_1005F370 - NOT nulled
        }
    }
    psmTrailField<unsigned int>(renderer, 0x8AC) = 0;
}

// sub_1016D8A0 (76 B), on the NSAParticleSystemRenderer at manager+0x85F8. The inverse of
// psmInitializeParticleRenderer(nsa = true): the +0x1C array and the +0x568 vertex buffer.
void psmReleaseNsaParticleRenderer(unsigned char* renderer)
{
    void*& items = psmNsaField<void*>(renderer, 0x01C);
    if (items)
    {
        ::renddx9abi::engineFree(items);
        items = 0;
    }
    void*& vertexBuffer = psmNsaField<void*>(renderer, 0x568);
    if (vertexBuffer)
    {
        dx9rend::vertexBufferRelease(vertexBuffer);                        // --[+4]; 10014510; free
        vertexBuffer = 0;
    }
}

} // anonymous namespace

// Releases level particle instances, descriptors, registries, pools and atlas contents.
// Does nothing before pool initialization. Descriptors are destroyed before remaining instances;
// instance destructors must not read them. Teardown suppresses removeInstance callbacks.
// Pending classification and stale pool pointers remain until preLoadLevel reinitializes them.
// [CHOSEN, not recovered] registry storage uses the file-scope maps with case-insensitive names.
void particleSystemManagerUnloadLevel(IParticleSystemManager* manager)
{
    ParticleSystemManager* const self = reinterpret_cast<ParticleSystemManager*>(manager);
    if (!self->m_poolsInitialized)
        return;

    manager->reserved_releaseAllInstances();                               // vtbl +0x24, 0x100D4820
    self->m_poolsInitialized = 0;                                          // +0x8C92
    self->m_tearingDown      = 1;                                          // +0x8C90
    psmDrainDestroy(self);

    for (ParticleSystemNameMap::iterator it = g_templatesByName.begin();
         it != g_templatesByName.end(); ++it)
    {
        if (it->second)
            it->second->destroy();                                         // +0x00 (1)
    }

    for (ParticleSystemInstanceList::iterator cursor = self->m_instances.begin();
         cursor != self->m_instances.end(); ++cursor)
    {
        if (*cursor)
            (*cursor)->destroy();                                          // +0x00 (1)
    }

    // sub_100D6B90(0, 0) = resize(0) on a vector of raw pointers: emptied, storage kept.
    self->m_pendingDestroy.clear();                                        // +0xCC
    static const unsigned int kBucketOrder[10] = { 0, 1, 3, 5, 2, 4, 6, 7, 9, 8 };
    for (unsigned int i = 0; i < 10u; ++i)
    {
        ParticleSystemInstanceVector& bucket = self->m_renderBuckets[kBucketOrder[i]];
        bucket.clear();                                                    // +0x20 + 0x10 * bucket
    }

    g_templatesByName.clear();                                             // sub_100D6C20 + relink
    g_templatesById.clear();                                               // sub_100D5F40 walk + relink

    // The shipped body frees the active list's storage and zeroes its three words.
    ParticleSystemInstanceList().swap(self->m_instances);
    self->m_nextTemplateId       = 0;                                      // +0x8BAA

    psmReleaseParticleRenderer(self->m_particleSystemRenderer);
    psmReleaseMeshRenderer(self->m_meshParticlesRenderer);
    psmReleaseTrailRenderer(self->m_trailRenderer);
    psmReleaseNsaParticleRenderer(self->m_nsaParticleSystemRenderer);

    ::renddx9abi::engineFree(self->m_type0PoolBacking);                    // +0x104  sub_1005F370
    ::renddx9abi::engineFree(self->m_type1PoolBacking);                    // +0x5F2C
    ::renddx9abi::engineFree(self->m_type2PoolBacking);                    // +0x7CC8
    ::renddx9abi::engineFree(self->m_type3PoolBacking);                    // +0x85E8
    ::renddx9abi::engineFree(self->m_type4PoolBacking);                    // +0x85F4

    dx9rend::textureAtlasReleaseAll(
        static_cast<dx9rend::TextureAtlas*>(self->m_textureAtlasHelper));
    self->m_tearingDown = 0;                                               // +0x8C90
}

// the atlas is borrowed and deliberately not null-checked. Its empty entry map
// returns no region; otherwise the descriptor's extensionless resource name gains ".dds".
dx9rend::TextureAtlasEntry* particleSystemFindTextureRegion(const std::string& name)
{
    ParticleSystemManager* const self = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    dx9rend::TextureAtlas* const atlas = static_cast<dx9rend::TextureAtlas*>(self->m_textureAtlasHelper);
    if (dx9rend::textureAtlasEntryCount(atlas) == 0)
        return 0;
    std::string path(name);
    path += ".dds";
    return dx9rend::textureAtlasFind(atlas, path);
}

void RD9_THUNK particleRendererCollectSorted(void* self RD9_THUNK_EDX, dx9rend::IRenderView* view,
    IParticleSystemInstance* instance, IParticleSystemTemplate* descriptor)
{
    psmCollectSortedParticles(static_cast<unsigned char*>(self), view,
        reinterpret_cast<unsigned char*>(instance), reinterpret_cast<unsigned char*>(descriptor));
}

void RD9_THUNK particleRendererCollectAdditive(void* self RD9_THUNK_EDX, dx9rend::IRenderView* view,
    IParticleSystemInstance* instance, IParticleSystemTemplate* descriptor)
{
    psmCollectAdditiveParticles(static_cast<unsigned char*>(self), view,
        reinterpret_cast<unsigned char*>(instance), reinterpret_cast<unsigned char*>(descriptor));
}

int ParticleSystemManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        deletingDestructor(1);
    return remaining;
}

void* ParticleSystemManager::deletingDestructor(int flags)
{
    if (psmHasUnrecoveredOwnedState(this))
    {
        RD9_FRONTIER_AT("dice::hfe::geom::ParticleSystemManager::~ParticleSystemManager "
                        "(renderer/tree/pool teardown)", this, 0x44);
        std::abort();
    }

    // Native lifetime of what this rebuild constructs: the renderers' in-place lists, then the
    // std members.
    psmDestroyRendererList<unsigned char*>(m_particleSystemRenderer, psmSpriteRendererOffset(0x08));
    psmDestroyRendererList<MeshParticleMesh*>(m_meshParticlesRenderer, psmMeshRendererOffset(0x1D80));
    psmDestroyRendererList<unsigned char*>(m_trailRenderer, psmTrailRendererOffset(0x08));
    psmDestroyRendererList<unsigned char*>(m_nsaParticleSystemRenderer, psmNsaRendererOffset(0x0C));
    this->~ParticleSystemManager();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
    return this;
}


// geom_ParticleSystemManager_ctor, 0x100D5870. The vtable/refcount, frame/pause fields, scalar
// defaults, four embedded renderer constructors, no-op hook and WindManager lookup are recovered.
// The vectors and the pending-reclassification set (PE 100D594B..100D598D) are constructed with
// the object; the two template registries are the file-scope maps above.

namespace {
void constructParticleSystemManager(ParticleSystemManager* self)
{
    self->m_vptr     = const_cast<const void**>(kVtable);
    self->m_refCount = 1;
    self->m_deltaTime = 0.0f;
    self->m_paused = 0;

    // Construction includes shader/declaration requests and Trail's
    // device listener; allocating the pools later is a separate operation.
    self->m_type0PoolBacking = 0;
    psmConstructParticleRenderer(self->m_particleSystemRenderer);
    self->m_type0FreeList = 0;
    self->m_type0PoolCount = 0;
    self->m_type0LiveCount = 0;                                            // +0x5F28
    self->m_type1PoolBacking = 0;
    psmConstructMeshParticleRenderer(self->m_meshParticlesRenderer);
    self->m_type1FreeList = 0;
    self->m_type1PoolCount = 0;
    self->m_type2PoolBacking = 0;
    psmConstructTrailRenderer(self->m_trailRenderer);
    self->m_type2FreeList = 0;
    self->m_type2PoolCount = 0;
    self->m_type3PoolBacking = 0;
    self->m_type3FreeList = 0;
    self->m_type3PoolCount = 0;
    self->m_type4PoolBacking = 0;
    psmConstructNsaParticleRenderer(self->m_nsaParticleSystemRenderer);
    self->m_type4FreeList = 0;
    self->m_type4PoolCount = 0;
    self->m_type4LiveCount = 0;                                            // +0x8BA0
    self->m_poolCount8BA4 = 0;                                             // +0x8BA4

    // Scalar defaults from 0x100D5870. The factory value-initialises the unrecovered opaque spans
    // and padding; that zero fill is [CHOSEN, not recovered], not a retail memset.
    self->m_forceSortedParticleType = 0;                                   // +0x8BA8
    self->m_nextTemplateId = 0;                                            // +0x8BAA
    self->m_lodScales[0] = 0.3f;                                           // +0x8BAC
    self->m_lodScales[1] = 0.6f;                                           // +0x8BB0
    self->m_lodScales[2] = 1.0f;                                           // +0x8BB4
    self->m_nearRatio = 0.7f;                                              // +0x8BDC
    self->m_farRatio = 0.85f;                                              // +0x8BF8
    self->m_tierThresholds[0] = 0.4f;                                      // +0x8BFC
    self->m_tierThresholds[1] = 0.55f;                                     // +0x8C00
    self->m_tierThresholds[2] = 0.7f;                                      // +0x8C04
    self->m_value8C08 = 0.75f;                                             // +0x8C08
    self->m_windManager = 0;                                               // +0x8C0C
    self->m_windEnabled = 1;                                               // +0x8C10
    self->m_value8C88 = 1024u;                                             // +0x8C88
    self->m_value8C8C = 1024u;                                             // +0x8C8C
    self->m_tearingDown = 0;                                               // +0x8C90
    self->m_state8C91 = 0;                                                 // +0x8C91
    self->m_poolsInitialized = 0;                                          // +0x8C92
    self->m_settingsApplied = 0;                                           // +0x8C93
    self->m_cachedLightValue = -1.0f;                                      // +0x8C9C
    self->m_value8CA4 = 10.0f;                                             // +0x8CA4
    self->m_distanceFactor = 1.0f;                                         // +0x8CA8
    self->m_shaderQuality = 3u;                                            // +0x8CAC
    self->m_qualityLevel = 2u;                                             // +0x8CB0

    // PE 0x100D5B7F..0x100D5B9C constructs the real 20-byte TextureAtlas before +0x30 and
    // the WindManager lookup. The preLoadLevel atlas arm consumes this object.
    self->m_textureAtlasHelper = 0;
    void* const atlasStorage = ::renddx9abi::engineAlloc(sizeof(dx9rend::TextureAtlas));
    if (atlasStorage)
        self->m_textureAtlasHelper = dx9rend::textureAtlasCtor(atlasStorage);

    // The shipped ctor now calls the virtual +0x30 hook and performs the same singleton lookup as
    // +0x10.
    reinterpret_cast<IParticleSystemManager*>(self)->resetStats();
    renddx9abi::HostString windName("WindManager");
    self->m_windManager = classManager->getSingleton(windName);           // +0x8C0C
}
} // anonymous namespace

// RefFactory_geom_ParticleSystemManager, 0x100B070F. Retail allocates the shipped 0x8CB8 bytes and
// then its complete ctor constructs every byte-owning subobject. This rebuild allocates the native
// object, value-initialises it (zero-filling the opaque spans and constructing the std members) and
// runs the recovered ctor subset. [CHOSEN, not recovered] zero-fill; queryInterface/release
// ordering is recovered.
IBase* __fastcall createParticleSystemManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(sizeof(ParticleSystemManager));
    ParticleSystemManager* self = ::new (storage) ParticleSystemManager();
    constructParticleSystemManager(self);

    void* itf = psm_qi(self RD9_THUNK_ARG, iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

// Shared renderer RNG used by particle emission and decal rotation/size.
unsigned int rendererRandomRaw()
{
    return psmEmissionRaw();
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
