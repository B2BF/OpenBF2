// [TEMP] [CHOSEN, not recovered]
// Tests emitter lifecycle through the real particle manager, including a real type-0 child.
// Descriptors, view, frame counter, and allocator are fixtures; no GPU draw or engine startup.
#define BF2_PSM_DRAW_FIXTURE
#define main particleUpdateFixtureMain
#include "particle_update_runtime.cpp"
#undef main
#include <new>

namespace dice { namespace hfe { namespace geom {
void* staticMeshRootParent(void* object) { return object; }
} } }
namespace dice { namespace hfe { namespace dx9rend {
int readIntSetting(const char*, int) { std::abort(); }
IVideoOptions* rendererGetVideoOptions() { std::abort(); }
TextureAtlas* textureAtlasCtor(void*) { std::abort(); }
} } }

namespace {
// view / renderer collaborators
void* g1ViewTable[0x9C / 4] = {};
void* g1ViewObject = g1ViewTable;
Mat4 g1Camera;
float g1Far = 321.0f;
int g1FarWrites = 0;
unsigned int g1Frame = 100;
void* g1RendererTable[64] = {};
void* g1RendererObject = g1RendererTable;
Mat4* __fastcall g1Matrix(void*, void*) { return &g1Camera; }
float __fastcall g1Fov(void*, void*) { return 1.0f; }
float __fastcall g1Aspect(void*, void*) { return 1.333f; }
float __fastcall g1GetFar(void*, void*) { return g1Far; }
void __fastcall g1SetFar(void*, void*, float value) { g1Far = value; ++g1FarWrites; }
unsigned int __fastcall g1FrameRead(void*, void*) { return g1Frame; }

// descriptors
void* g1Type3Table[99] = {};
void* g1Type0Table[180] = {};
int __fastcall g1Three(void*, void*) { return 3; }
int __fastcall g1Zero(void*, void*) { return 0; }
float __fastcall g1ZeroF(void*, void*) { return 0.0f; }
alignas(16) unsigned char g1Emitter[0x1A8];
alignas(16) unsigned char g1Orphan[0x1A8];
alignas(16) unsigned char g1Child[0x2A4];

template<class T> T& at(void* object, unsigned int offset) { return psmRuntimeField<T>(object, offset); }
void vec4(void* object, unsigned int offset, float a, float b, float c, float d)
{ at<float>(object,offset)=a; at<float>(object,offset+4)=b; at<float>(object,offset+8)=c; at<float>(object,offset+12)=d; }

void buildEmitterDescriptor(unsigned char* d, const char* childName)
{
    std::memset(d, 0, 0x1A8);
    *reinterpret_cast<void***>(d) = g1Type3Table;
    // Host strings, as ParticleSystemTemplateType3.cpp now keeps them (std::string natively, the VC7.1
    // layout in the compatibility flavour) - so the native build tests the layout the client passes.
    new (d + 0x6C) ::renddx9abi::HostString(childName);
    new (d + 0x16C) ::renddx9abi::HostString("em_g1_test_emitter");
    // A stock "firefingers"-shaped emitter (see evidence/stock_emitter_options.txt).
    at<float>(d,0x0C)=30; at<float>(d,0x58)=360; at<float>(d,0x8C)=0.1f; at<float>(d,0x90)=0.3f;
    at<float>(d,0x98)=-8; at<float>(d,0x9C)=0.1f; at<unsigned char>(d,0xC4)=1;
    at<float>(d,0xC8)=0.7f; at<float>(d,0xCC)=0.8f; at<float>(d,0xC0)=0.03f;
    vec4(d,0xD4,0,0,0,1); vec4(d,0xE4,0,0,0.41f,0.58f); vec4(d,0xF4,0,0,0,1);
    vec4(d,0x104,-0.55f,0,-0.46f,1); vec4(d,0x114,-0.19f,-0.7f,-0.1f,1);
    at<float>(d,0x130)=20; at<float>(d,0x134)=60; at<float>(d,0x13C)=60; at<float>(d,0x140)=40;
    at<float>(d,0x144)=0.1f; at<float>(d,0x15C)=1;                      // emitDirection (0,1,0)
    at<unsigned char>(d,0x164)=1; at<float>(d,0x168)=0.3f;              // align to speed, inherit 0.3
}
void buildChildDescriptor(unsigned char* d)
{
    std::memset(d, 0, 0x2A4);
    *reinterpret_cast<void***>(d) = g1Type0Table;
    at<float>(d,0x10)=20; at<float>(d,0x84)=0.5f;                      // 20 sprites/s, 0.5 s life
}

unsigned int freeCount(void* head)
{ unsigned int n=0; for (void* p=head; p; p=*static_cast<void**>(p)) ++n; return n; }

unsigned int countType(ParticleSystemManager& m, int type)
{
    unsigned int n=0;
    for (unsigned int i=0;i<m.m_instances.size();++i) if (m.m_instances[i]->reserved01()==type) ++n;
    return n;
}

void frame(ParticleSystemManager& m, float dt=1.0f/60)
{ reinterpret_cast<IParticleSystemManager*>(&m)->update(dt); }
}

namespace dice { namespace hfe { namespace dx9rend {
IRenderView* rendererGetRenderViewDup() { return reinterpret_cast<IRenderView*>(&g1ViewObject); }
} } }

int main(int argc, char** argv)
{
    static ParticleSystemManager manager;
    manager.m_vptr=const_cast<const void**>(kVtable);
    manager.m_poolsInitialized=1; manager.m_qualityLevel=2; manager.m_effectiveViewDistance=1000;
    manager.m_value8C08=0.75f;
    manager.m_tierThresholds[0]=0.4f; manager.m_tierThresholds[1]=0.55f; manager.m_tierThresholds[2]=0.7f;
    manager.m_type3PoolCount=16;
    psmAllocatePool(manager.m_type3PoolBacking,manager.m_type3FreeList,manager.m_type3PoolCount,176,3);
    manager.m_type0PoolCount=512;
    psmAllocatePool(manager.m_type0PoolBacking,manager.m_type0FreeList,manager.m_type0PoolCount,148,0);
    g_ParticleSystemManager=reinterpret_cast<IBase*>(&manager);
    static unsigned char constants[0x200]={}; at<float>(constants,0x1BC)=-1000.0f;
    dx9rend::g_raShaderConstants=constants;
    g1RendererTable[0x54/4]=reinterpret_cast<void*>(&g1FrameRead);
    dx9rend::g_rendererBase=&g1RendererObject;
    g1ViewTable[0x20/4]=reinterpret_cast<void*>(&g1Fov);
    g1ViewTable[0x38/4]=reinterpret_cast<void*>(&g1SetFar);
    g1ViewTable[0x3C/4]=reinterpret_cast<void*>(&g1GetFar);
    g1ViewTable[0x44/4]=reinterpret_cast<void*>(&g1Aspect);
    g1ViewTable[0x64/4]=reinterpret_cast<void*>(&g1Matrix);
    g1Camera=Mat4::getIdentity(); g1Camera.m[14]=-20;
    g1Type3Table[3]=reinterpret_cast<void*>(&g1Three); g1Type3Table[9]=reinterpret_cast<void*>(&g1Zero);
    g1Type0Table[3]=reinterpret_cast<void*>(&g1Zero); g1Type0Table[9]=reinterpret_cast<void*>(&g1Zero);
    g1Type0Table[0x130/4]=reinterpret_cast<void*>(&g1Zero);             // getGasCloudType
    g1Type0Table[0x148/4]=reinterpret_cast<void*>(&g1ZeroF);            // getMinGasCloudRadius
    buildChildDescriptor(g1Child);
    g_templatesByName["em_child"]=reinterpret_cast<IParticleSystemTemplate*>(g1Child);

    if (argc>1 && std::strcmp(argv[1],"nullchild")==0)
    {
        // Negative path: an unregistered child name. The emitter must stop at its named diagnostic
        // on the first particle advance (retail would fault there); the fixture's frontier trap exits.
        buildEmitterDescriptor(g1Orphan,"em_not_registered");
        IParticleSystemInstance* orphan=psm_createInstance(&manager,0,reinterpret_cast<IParticleSystemTemplate*>(g1Orphan));
        CHECK(at<void*>(g1Orphan,0x88)==0);
        orphan->reserved03(false);
        for (int i=0;i<10;++i) frame(manager);          // the first particle is born on frame 3
        std::printf("nullchild: the named stop was NOT reached (%d checks, %d failures)\n",checks,failures);
        return 3;
    }

    buildEmitterDescriptor(g1Emitter,"EM_Child");                        // case differs from the key
    IParticleSystemTemplate* const descriptor=reinterpret_cast<IParticleSystemTemplate*>(g1Emitter);
    const unsigned int allocationsBefore=static_cast<unsigned int>(allocationSizes.size());
    IParticleSystemInstance* const e=psm_createInstance(&manager,0,descriptor);
    CHECK(e && e->reserved01()==3 && manager.m_instances.size()==1 && at<unsigned char>(e,5)==0);
    CHECK(allocationSizes.size()==allocationsBefore+2 && allocationSizes[allocationsBefore]==0x220);
    CHECK(allocationSizes[allocationsBefore+1]==13*4+4 && e->reserved34()==13);   // min(0.4,0.1)*30+10
    CHECK(at<void*>(g1Emitter,0x88)==g1Child);                            // lazy stricmp resolve
    CHECK(!e->reserved05() && e->reserved39()==0 && e->reserved10() && !e->reserved07());
    CHECK(at<unsigned int>(e,0xCC)==0xA5A5A5A5u && at<unsigned int>(e,0x13C)==0xA5A5A5A5u); // ctor-untouched
    CHECK(e->getTemplate()==descriptor && e->getGasCloudRadius()==1 && e->getMaterial()==-1 && !e->getWaterSide());
    CHECK(e->reserved31()==1 && e->reserved25()==1 && e->reserved27()==1 && e->reserved23()==1);
    CHECK(e->reserved29().x==1 && e->reserved17().z==1 && e->reserved38()==e->reserved38());
    CHECK(at<float>(e,0x204)==1 && at<float>(e,0x164)==1 && at<float>(e,0x1F4)==1);   // planes (0,1,0,0)

    // Start: classification first (world bucket 1), far plane saved/set/restored, visible.
    e->reserved03(false);
    CHECK(e->reserved05() && manager.m_renderBuckets[1].size()==1 && at<unsigned char>(e,4)==1);
    CHECK(g1Far==321.0f && g1FarWrites==2 && at<unsigned int>(e,0x124)==100 && e->reserved10());
    CHECK(at<float>(e,0xE8)==0.1f && at<float>(e,0xEC)==0 && at<float>(e,0xF0)==0);

    unsigned int maxLive=0, maxChildren=0, boundChecks=0, boundOk=0, matrixOk=0;
    for (int i=0;i<30;++i)
    {
        frame(manager);
        const unsigned int live=static_cast<unsigned int>(e->reserved39());
        if (live>maxLive) maxLive=live;
        const unsigned int children=countType(manager,0);
        if (children>maxChildren) maxChildren=children;
        unsigned char** const records=at<unsigned char**>(e,0x14);
        for (unsigned int r=0;r<live;++r)
        {
            unsigned char* const record=records[r];
            IParticleSystemInstance* const child=at<IParticleSystemInstance*>(record,0xAC);
            ++boundChecks;
            if (child && at<unsigned char>(child,5)==1 && at<void*>(child,0x30)==record+4
                && child->reserved05() && at<unsigned char>(child,8)==at<unsigned char>(e,8)) ++boundOk;
            if (at<float>(record,0x34)==at<float>(record,0x50) && at<float>(record,0x38)==at<float>(record,0x54)
                && at<float>(record,0x3C)==at<float>(record,0x58)) ++matrixOk;
        }
        CHECK(records[live]==0);
    }
    std::printf("emitter: max live %u, max children %u, bound %u/%u, matrix %u/%u\n",
                maxLive,maxChildren,boundOk,boundChecks,matrixOk,boundChecks);
    CHECK(maxLive>=2 && maxChildren>=2 && boundChecks>0 && boundOk==boundChecks && matrixOk==boundChecks);
    CHECK(!e->reserved05());                                              // 0.1 s emit time elapsed

    // Let everything expire and drain: released children fade under the manager, then are destroyed.
    for (int i=0;i<180;++i) frame(manager);
    CHECK(e->reserved39()==0 && countType(manager,0)==0 && manager.m_instances.size()==1);
    CHECK(freeCount(manager.m_type3FreeList)==16);
    CHECK(manager.m_type0LiveCount==0 && freeCount(manager.m_type0FreeList)==512);
    CHECK(manager.m_renderBuckets[1].size()==0 && at<unsigned char>(e,4)==0);   // queued inactive

    // Restart, then clear while particles (and children) are alive: every child is released.
    e->reserved03(false);
    for (int i=0;i<4;++i) frame(manager);
    const unsigned int beforeClear=static_cast<unsigned int>(e->reserved39());
    CHECK(beforeClear>0 && countType(manager,0)==beforeClear);
    e->reserved02();
    CHECK(e->reserved39()==0 && !e->reserved05() && at<unsigned char**>(e,0x14)[0]==0);
    CHECK(freeCount(manager.m_type3FreeList)==16 && at<float>(e,0x12C)==1 && e->reserved10());
    bool allReleased=true;
    for (unsigned int i=0;i<manager.m_instances.size();++i)
    {
        IParticleSystemInstance* const other=manager.m_instances[i];
        if (other!=e && at<unsigned char>(other,5)!=0) allReleased=false;
    }
    CHECK(allReleased);
    for (int i=0;i<120;++i) frame(manager);
    CHECK(countType(manager,0)==0 && freeCount(manager.m_type0FreeList)==512);

    // The 60-frame gate: nothing refreshes +0x124 for type 3, so a late frame stops emission.
    e->reserved03(false); g1Frame=161; frame(manager);
    CHECK(e->reserved39()==0 && !e->reserved10() && e->reserved05());
    g1Frame=100; e->reserved04();
    for (int i=0;i<3;++i) frame(manager);

    // Removal with no live particles queues destruction; the next update destroys it.
    reinterpret_cast<IParticleSystemManager*>(&manager)->removeInstance(e);
    frame(manager);
    CHECK(manager.m_instances.size()==0);

    freeList(manager.m_instances); freeList(manager.m_pendingDestroy); freeList(manager.m_pendingClassification);
    for (int i=0;i<10;++i) freeList(manager.m_renderBuckets[i]);
    typedef ::renddx9abi::HostString FixtureHostString;   // a pseudo-destructor needs an unqualified name
    reinterpret_cast<FixtureHostString*>(g1Emitter+0x6C)->~FixtureHostString();
    reinterpret_cast<FixtureHostString*>(g1Emitter+0x16C)->~FixtureHostString();
    g_templatesByName.clear(); cleanup();
    std::printf("G1 type-3 emitter smoke: %d/%d checks passed\n",checks-failures,checks);
    return failures?1:0;
}
