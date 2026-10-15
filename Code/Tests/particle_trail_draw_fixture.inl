// [TEMP] Actual Trail prepare/collect/shader/68-byte indexed draw path.
// External GPU/view/light collaborators are observed fixtures; this produces no pixels.
#include <algorithm>
namespace {
int __fastcall trailDrawKind(void*, void*) { return 2; }
IParticleSystemTemplate* __fastcall trailDrawTemplate(void* self, void*)
{ return psmDrawField<IParticleSystemTemplate*>(static_cast<unsigned char*>(self),0x2C); }
struct TrailScalarParameter : ParticleParameterFixture
{
    float scalar = -1;
    unsigned char* clearFlag = 0;
    void setValue(const void* p, unsigned int n) override
    {
        ParticleParameterFixture::setValue(p,n);
        if (p) scalar = *static_cast<const float*>(p);
        if (clearFlag) *clearFlag = 0;
    }
};

void verifyTrailParticles(int fault)
{
    using namespace dice::hfe::dx9rend;
    using namespace particle_renderer_fixture;
    allowGpu = true; gpuStride = 68;
    ParticleSystemManager manager = {};
    g_ParticleSystemManager = reinterpret_cast<IBase*>(&manager);
    manager.m_farDistances[0] = 100;
    manager.m_inverseFarRanges[0] = 0.01f;
    manager.m_shaderQuality = 2;
    manager.m_value8CA4 = 2;
    unsigned char* renderer = manager.m_trailRenderer;
    renderer[0x884] = 1;
    unsigned char* table[208] = {};
    psmDrawField<unsigned char**>(renderer,0x18) = table;
    psmDrawField<unsigned int>(renderer,0x8AC) = 128;
    ParticleParameterFixture shader;
    TrailScalarParameter fresnel, eye, sun;
    const unsigned int offsets[] = {0x8DC,0x8E0,0x8E4,0x8E8,0x8EC,0x8F0,0x8F4,0x8F8,0x8FC,0x900,0x904,0x908,0x90C};
    for (unsigned int offset : offsets) psmDrawField<ParameterOwner*>(renderer,offset) = &shader;
    psmDrawField<ParameterOwner*>(renderer,0x8F4) = &fresnel;
    psmDrawField<ParameterOwner*>(renderer,0x900) = &eye;
    psmDrawField<ParameterOwner*>(renderer,0x908) = &sun;
    for (unsigned int i=1; i<=3; ++i)
        psmDrawField<void*>(renderer,0x8B8+4*i) = reinterpret_cast<void*>(0x78000000u+i);
    void* localViewTable[0x9C/4] = {}, *localViewObject = localViewTable;
    localViewTable[0x34/4] = reinterpret_cast<void*>(&activeNear);
    localViewTable[0x64/4] = reinterpret_cast<void*>(&activeView);
    localViewTable[0x68/4] = reinterpret_cast<void*>(&activeInverse);
    localViewTable[0x6C/4] = reinterpret_cast<void*>(&activeProjection);
    localViewTable[0x74/4] = reinterpret_cast<void*>(&activeFrustum);
    IRenderView* view = reinterpret_cast<IRenderView*>(&localViewObject);
    activeIdentity = Mat4::getIdentity();
    activeViewReads = activeInverseReads = activeProjectionReads = hemiReads = 0;
    void* hemiTable[14] = {}, *hemiObject = hemiTable;
    hemiTable[6] = reinterpret_cast<void*>(&activeHemi);
    g_HemiMapManager = reinterpret_cast<IHemiMapManager*>(&hemiObject);
    void* lightTable[0x274/4] = {}, *lightObject = lightTable;
    lightTable[0x260/4] = lightTable[0x268/4] = reinterpret_cast<void*>(&activeLightColor);
    g_LightManager = reinterpret_cast<LightManager*>(&lightObject);
    rendererTable[0x54/4] = reinterpret_cast<void*>(&activeFrame);
    unsigned char descriptor[0x238] = {};
    psmDrawField<int>(descriptor,8) = -1;
    psmDrawField<unsigned short>(descriptor,4) = 123;
    psmDrawField<unsigned int>(descriptor,0x220) = 3;
    psmDrawField<void*>(descriptor,0x21C) = reinterpret_cast<void*>(0x7070);
    unsigned char instance[0x19C] = {};
    void* instanceTable[51] = {};
    instanceTable[1] = reinterpret_cast<void*>(&trailDrawKind);
    instanceTable[0x8C/4] = reinterpret_cast<void*>(&trailDrawTemplate);
    psmDrawField<void*>(instance,0) = instanceTable;
    unsigned char particles[3][168] = {};
    unsigned char* ring[4] = {particles[1],particles[2],0,particles[0]};
    psmDrawField<unsigned char**>(instance,0x14) = ring;
    psmDrawField<unsigned int>(instance,0x28) = 3;
    psmDrawField<unsigned char*>(instance,0x2C) = descriptor;
    psmDrawField<unsigned int>(instance,0x174) = 2;
    psmDrawField<unsigned int>(instance,0x178) = 3;
    psmDrawField<unsigned int>(instance,0x17C) = 3;
    psmDrawField<unsigned int>(instance,0x180) = 0x11223344;
    psmDrawField<unsigned int>(instance,0x184) = 0x55667788;
    psmDrawField<float>(instance,0x18C) = 0.625f;
    const float depths[] = {0.5f,150,300};
    for (unsigned int i=0; i<3; ++i)
    {
        unsigned char* p = particles[i];
        psmDrawField<float>(p,0x10) = 7+float(i);
        psmDrawField<float>(p,0x14) = 8+float(i);
        psmDrawField<float>(p,0x18) = depths[i];
        for (unsigned int j=0;j<18;++j) psmDrawField<float>(p,0x1C+j*4)=float(100*i+j);
        psmDrawField<Vec3>(p,0x64) = Vec3{3,4,5};
        psmDrawField<float>(p,0x70) = 0.75f;
        psmDrawField<float>(p,0x74) = 0.125f;
        psmDrawField<float>(p,0x78) = 0.5f;
        psmDrawField<float>(p,0x80) = float(i)*2;
        psmDrawField<float>(p,0x88) = 0.8f;
    }
    IParticleSystemInstance* item = reinterpret_cast<IParticleSystemInstance*>(instance);
    ParticleSystemInstanceList list(1,item);
    if (fault==3) renderer[0x886]=1;
    psmPrepareTrailRenderer(renderer,view,&list,0.25f);
    CHECK(psmDrawField<unsigned int>(renderer,0x31C)==3 && psmDrawField<unsigned int>(renderer,0x320)==1);
    CHECK(table[0]==particles[0] && table[1]==particles[1] && table[2]==particles[2]);
    CHECK(psmDrawField<unsigned int>(renderer,0x20)==2 && psmDrawField<unsigned int>(renderer,0x24)==3);
    CHECK(psmDrawField<unsigned int>(instance,0x120)==1234 && instance[0x156]==1);
    CHECK(activeViewReads==2 && activeInverseReads==1 && activeProjectionReads==0);
    CHECK(psmDrawField<unsigned short>(particles[1],0x86)==123);
    CHECK(psmDrawField<int>(descriptor,8)==0 && psmDrawField<unsigned int>(renderer,0x854)==1);
    CHECK(psmDrawField<float>(particles[0],0x7C)==0.8f); // No near-plane rejection.
    CHECK(std::fabs(psmDrawField<float>(particles[1],0x7C)-0.2f)<0.000001f);
    CHECK(psmDrawField<float>(particles[2],0x7C)==0);
    if (fault==7) failGpuLockAt=1;
    resetParticleGpu();
    CHECK(psmSelectTrailRenderer(renderer,0xFFFFFFFFu) && psmDrawTrailRenderer(renderer,view));
    CHECK(gpuIndexedDraws.size()==1 && gpuLocks.size()==1);
    CHECK(gpuIndexedDraws[0].count==12 && gpuIndexedDraws[0].type==4);
    CHECK(gpuIndexedDraws[0].base==0 && gpuIndexedDraws[0].minimum==0 && gpuIndexedDraws[0].start==0);
    CHECK(gpuIndexedDraws[0].vertices==18 && gpuIndexedDraws[0].bytes.size()==18*68);
    CHECK(gpuLocks[0].count==18 && gpuLocks[0].flags==10240);
    CHECK(gpuIndexBuffers[0]==reinterpret_cast<void*>(0x78000003u));
    CHECK(gpuEvents.front()=="push" && gpuEvents.back()=="pop");
    CHECK(std::find(gpuEvents.begin(),gpuEvents.end(),"technique:TrailMedium")!=gpuEvents.end());
    CHECK(hemiReads==2 && renderer[0x84C]==1 && renderer[0x858]==0);
    CHECK(activeProjectionReads==2 && activeInverseReads==3 && activeViewReads==3);
    CHECK(psmDrawField<unsigned int>(renderer,0x24)==0 && psmDrawField<unsigned int>(renderer,0x20)==0xFFFFFFFFu);
    CHECK(psmDrawField<unsigned int>(renderer,0x724)==3 && psmDrawField<unsigned int>(renderer,0x8B0)==18);
    CHECK(manager.m_drawCounters[3]==1); // retail +0x8C50, the trail draw counter
    CHECK(fresnel.scalar==0);
    for (unsigned int i=0;i<18;++i)
    {
        unsigned char* v = gpuIndexedDraws[0].bytes.data()+i*68;
        unsigned char* p = particles[2-i/6];
        CHECK(::memcmp(v,p+0x10,12)==0);
        CHECK(::memcmp(v+0xC,p+0x1C+(i%6)*12,12)==0);
        CHECK(::memcmp(v+0x18,p+0x64,12)==0);
        CHECK(psmDrawField<float>(v,0x24)==0.875f && psmDrawField<float>(v,0x28)==0.5f);
        CHECK(psmDrawField<float>(v,0x2C)==0.625f && psmDrawField<float>(v,0x30)==psmDrawField<float>(p,0x7C));
        CHECK(psmDrawField<unsigned int>(v,0x34)==0x11223344 && psmDrawField<unsigned int>(v,0x38)==0x55667788);
        CHECK(psmDrawField<float>(v,0x3C)==psmDrawField<float>(p,0x80));
        CHECK(psmDrawField<float>(v,0x40)==(i%2 ? -1.0f : 1.0f));
    }

    // All plane counts, and the second group's no-overwrite lock and indexed base vertex.
    for (unsigned int planes=1;planes<=3;++planes)
    {
        psmDrawField<unsigned int>(descriptor,0x220)=planes;
        psmPrepareTrailRenderer(renderer,view,&list,0.1f);
        psmCollectTrailParticles(renderer,view,instance,descriptor);
        resetParticleGpu();
        CHECK(psmSelectTrailRenderer(renderer,0) && psmDrawTrailRenderer(renderer,view));
        CHECK(gpuIndexedDraws.size()==2 && gpuIndexedDraws[1].count==planes*4);
        CHECK(gpuLocks[0].count==planes*6 && gpuLocks[1].flags==6144);
        CHECK(gpuLocks[1].offset==planes*6 && gpuIndexedDraws[1].base==int(planes*6));
        CHECK(gpuIndexBuffers[0]==reinterpret_cast<void*>(0x78000000u+planes));
        CHECK(fresnel.scalar==(planes==1 ? 1.0f : 0.0f));
    }

    // Collector cap is tested after writing the candidates; rejection retains the totals.
    psmPrepareTrailRenderer(renderer,view,&list,0.1f);
    for (unsigned int i=1;i<65;++i) psmCollectTrailParticles(renderer,view,instance,descriptor);
    CHECK(psmDrawField<unsigned int>(renderer,0x320)==64 && psmDrawField<unsigned int>(renderer,0x31C)==192);
    CHECK(table[192]==particles[0] && table[194]==particles[2]);
    psmDrawField<unsigned int>(instance,0x174)=1; // Wrapped ring now contains only two entries.
    psmPrepareTrailRenderer(renderer,view,&list,0.1f);
    CHECK(psmDrawField<unsigned int>(renderer,0x31C)==0 && psmDrawField<unsigned int>(renderer,0x320)==0);
    CHECK(table[0]==particles[0] && table[1]==particles[1]);
    psmDrawField<unsigned int>(instance,0x174)=2;
    psmDrawField<float>(particles[0],0x18)=std::numeric_limits<float>::quiet_NaN();
    psmPrepareTrailRenderer(renderer,view,&list,0.1f);
    CHECK(psmDrawField<float>(particles[0],0x7C)==0.8f);
    CHECK(psmDrawField<unsigned int>(renderer,0x854)==1); // Descriptor serial is retained.

    // Empty/disabled input preserves all state; an active zero-count group still returns true.
    renderer[0x884]=0;
    unsigned char saved[0x914]; ::memcpy(saved,renderer,sizeof(saved));
    psmPrepareTrailRenderer(renderer,0,&list,9);
    CHECK(::memcmp(saved,renderer,sizeof(saved))==0 && !psmDrawTrailRenderer(renderer,0));
    renderer[0x884]=1;
    ParticleSystemInstanceList empty = {};
    ::memcpy(saved,renderer,sizeof(saved));
    psmPrepareTrailRenderer(renderer,0,&empty,9);
    CHECK(::memcmp(saved,renderer,sizeof(saved))==0);
    psmDrawField<unsigned int>(renderer,0x24)=0;
    resetParticleGpu();
    CHECK(!psmDrawTrailGroup(renderer,0,0,0));
    renderer[0x858]=1;
    CHECK(psmDrawTrailRenderer(renderer,view) && gpuIndexedDraws.empty());

    // Parameter callbacks may clear eligibility; the wrapper skips binding/submission then.
    resetParticleGpu(); renderer[0x858]=1; eye.clearFlag=renderer+0x858;
    CHECK(psmDrawTrailRenderer(renderer,view) && gpuEvents.size()==2);
    CHECK(gpuEvents[0]=="push" && gpuEvents[1]=="pop");
    eye.clearFlag=0;
    const char* techniques[]={"TrailShowFill","TrailLow","TrailMedium","TrailHigh"};
    for (unsigned int quality=0;quality<=4;++quality)
    {
        manager.m_shaderQuality=quality; resetParticleGpu();
        psmBindTrailParameters(renderer);
        if (quality<4) CHECK(std::find(gpuEvents.begin(),gpuEvents.end(),std::string("technique:")+techniques[quality])!=gpuEvents.end());
        else CHECK(std::none_of(gpuEvents.begin(),gpuEvents.end(),[](const std::string& x){return x.find("technique:")==0;}));
        if (!quality) CHECK(sun.scalar==0.5f);
    }
    allowGpu=false; gpuStride=64;
    g_HemiMapManager=0; g_ParticleSystemManager=0; g_LightManager=0;
}
}
