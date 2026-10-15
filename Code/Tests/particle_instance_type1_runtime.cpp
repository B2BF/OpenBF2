// [TEMP] [CHOSEN, not recovered]
// Tests mesh-particle manager lifecycle with the real type-1 descriptor.
// View, frame counter, and allocator are fixtures; empty geometry avoids resource lookup. No GPU draw or engine startup.
#define BF2_PSM_DRAW_FIXTURE
#define BF2_PSM_TYPE1_DESCRIPTOR_LINKED
#define main particleUpdateFixtureMain
#include "particle_update_runtime.cpp"
#undef main
#include <new>

namespace dice { namespace hfe { namespace geom {
void* staticMeshRootParent(void* object) { return object; }
// [TEMP] the descriptor TU's two outside references. The geometry template manager is read only for a
// NON-empty geometry name (this descriptor has none); the texture release runs only in the descriptor's
// destructor, on +0x210, which this fixture never runs.
IBase* g_GeometryTemplateManager = 0;
} } }
namespace dice { namespace hfe { namespace dx9rend {
struct EngineTexture;
int readIntSetting(const char*, int) { std::abort(); }
IVideoOptions* rendererGetVideoOptions() { std::abort(); }
TextureAtlas* textureAtlasCtor(void*) { std::abort(); }
void textureReleaseRef(EngineTexture*&)
{ particle_renderer_fixture::meshParticleUnreached("dx9rend::textureReleaseRef"); }
} } }

namespace {
// view / renderer collaborators (the type-3 fixture's)
void* t1ViewTable[0x9C / 4] = {};
void* t1ViewObject = t1ViewTable;
Mat4 t1Camera;
float t1Far = 321.0f;
int t1FarWrites = 0;
unsigned int t1Frame = 100;
void* t1RendererTable[64] = {};
void* t1RendererObject = t1RendererTable;
Mat4* __fastcall t1Matrix(void*, void*) { return &t1Camera; }
float __fastcall t1Fov(void*, void*) { return 1.0f; }
float __fastcall t1Aspect(void*, void*) { return 1.333f; }
float __fastcall t1GetFar(void*, void*) { return t1Far; }
void __fastcall t1SetFar(void*, void*, float value) { t1Far = value; ++t1FarWrites; }
unsigned int __fastcall t1FrameRead(void*, void*) { return t1Frame; }

alignas(16) unsigned char t1Descriptor[0x238];

template<class T> T& at(void* object, unsigned int offset) { return psmRuntimeField<T>(object, offset); }
// The descriptor's own setters, called through its retail-ordered table like the host does.
void descriptorSet(unsigned int slot, float value)
{
    unsigned int bits; std::memcpy(&bits, &value, 4);
    void** const table = *reinterpret_cast<void***>(t1Descriptor);
    reinterpret_cast<void(__fastcall*)(void*, void*, unsigned int)>(table[slot])(t1Descriptor, 0, bits);
}
unsigned int freeCount(void* head)
{ unsigned int n=0; for (void* p=head; p; p=*static_cast<void**>(p)) ++n; return n; }
void frame(ParticleSystemManager& m, float dt=1.0f/60)
{ reinterpret_cast<IParticleSystemManager*>(&m)->update(dt); }
bool finite(float v) { return v == v && v - v == 0.0f; }
}

namespace dice { namespace hfe { namespace dx9rend {
IRenderView* rendererGetRenderViewDup() { return reinterpret_cast<IRenderView*>(&t1ViewObject); }
} } }

int main()
{
    static ParticleSystemManager manager;
    manager.m_vptr=const_cast<const void**>(kVtable);
    manager.m_poolsInitialized=1; manager.m_qualityLevel=2; manager.m_effectiveViewDistance=1000;
    manager.m_value8C08=0.75f;
    manager.m_tierThresholds[0]=0.4f; manager.m_tierThresholds[1]=0.55f; manager.m_tierThresholds[2]=0.7f;
    manager.m_type1PoolCount=64;
    psmAllocatePool(manager.m_type1PoolBacking,manager.m_type1FreeList,manager.m_type1PoolCount,192,1);
    g_ParticleSystemManager=reinterpret_cast<IBase*>(&manager);
    static unsigned char constants[0x200]={}; at<float>(constants,0x1BC)=-1000.0f;
    dx9rend::g_raShaderConstants=constants;
    t1RendererTable[0x54/4]=reinterpret_cast<void*>(&t1FrameRead);
    dx9rend::g_rendererBase=&t1RendererObject;
    t1ViewTable[0x20/4]=reinterpret_cast<void*>(&t1Fov);
    t1ViewTable[0x38/4]=reinterpret_cast<void*>(&t1SetFar);
    t1ViewTable[0x3C/4]=reinterpret_cast<void*>(&t1GetFar);
    t1ViewTable[0x44/4]=reinterpret_cast<void*>(&t1Aspect);
    t1ViewTable[0x64/4]=reinterpret_cast<void*>(&t1Matrix);
    t1Camera=Mat4::getIdentity(); t1Camera.m[14]=-20;

    // A real type-1 descriptor: its constructor runs initType1Defaults; four setters then make it a
    // short, non-looping burst - 20 per second (slot 23 caches 1/x at +0x19C), 0.5 s of life (slot 45
    // caches 1/x at +0xA0), 0.2 s of emission (slot 29), speed 1 (slot 25).
    const std::string name("mp_g1_test");
    constructParticleSystemType1(t1Descriptor, &name);
    IParticleSystemTemplate* const descriptor=reinterpret_cast<IParticleSystemTemplate*>(t1Descriptor);
    CHECK(descriptor->getSystemType()==1);
    descriptorSet(23, 20.0f); descriptorSet(45, 0.5f); descriptorSet(29, 0.2f); descriptorSet(25, 1.0f);
    CHECK(at<float>(t1Descriptor,0x19C)==0.05f && at<float>(t1Descriptor,0xA0)==2.0f);
    CHECK(at<void*>(t1Descriptor,0x8C)==0);                               // no geometry name

    const unsigned int allocationsBefore=static_cast<unsigned int>(allocationSizes.size());
    IParticleSystemInstance* const e=psm_createInstance(&manager,0,descriptor);
    CHECK(e && e->reserved01()==1 && manager.m_instances.size()==1 && at<unsigned char>(e,5)==0);
    CHECK(allocationSizes.size()>allocationsBefore && allocationSizes[allocationsBefore]==0x224);
    CHECK(e->getTemplate()==descriptor && at<void*>(t1Descriptor,0x8C)==0);   // resolve ran, did nothing
    CHECK(!e->reserved05() && e->reserved39()==0);
    const unsigned int capacity=static_cast<unsigned int>(e->reserved34());
    CHECK(capacity>0);

    e->reserved03(false);
    CHECK(e->reserved05());                                               // emitting

    unsigned int maxLive=0, recordChecks=0, recordsOk=0;
    for (int i=0;i<30;++i)
    {
        frame(manager);
        const unsigned int live=static_cast<unsigned int>(e->reserved39());
        if (live>maxLive) maxLive=live;
        CHECK(live<=capacity);
        unsigned char** const records=at<unsigned char**>(e,0x14);
        for (unsigned int r=0;r<live;++r)
        {
            unsigned char* const record=records[r];
            ++recordChecks;
            const float age=at<float>(record,0x5C);
            if (record && age>=0.0f && age<=1.0f && finite(at<float>(record,0x50))
                && finite(at<float>(record,0x54)) && finite(at<float>(record,0x58))) ++recordsOk;
        }
    }
    std::printf("mesh particles: max live %u of capacity %u, records sane %u/%u\n",
                maxLive,capacity,recordsOk,recordChecks);
    CHECK(maxLive>=2 && recordChecks>0 && recordsOk==recordChecks);

    // Everything expires; every record goes back to the pool.
    for (int i=0;i<120;++i) frame(manager);
    CHECK(e->reserved39()==0 && freeCount(manager.m_type1FreeList)==64);

    // Restart, then clear while particles are alive: all of them return at once.
    e->reserved03(false);
    for (int i=0;i<6;++i) frame(manager);
    CHECK(e->reserved39()>0);
    e->reserved02();
    CHECK(e->reserved39()==0 && freeCount(manager.m_type1FreeList)==64);

    // Removal with no live particles queues destruction; the next update destroys it.
    reinterpret_cast<IParticleSystemManager*>(&manager)->removeInstance(e);
    frame(manager);
    CHECK(manager.m_instances.size()==0);

    freeList(manager.m_instances); freeList(manager.m_pendingDestroy); freeList(manager.m_pendingClassification);
    for (int i=0;i<10;++i) freeList(manager.m_renderBuckets[i]);
    cleanup();
    std::printf("G1 type-1 mesh-particle runtime: %d/%d checks passed\n",checks-failures,checks);
    return failures?1:0;
}
