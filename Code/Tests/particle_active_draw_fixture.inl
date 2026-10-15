// [TEMP] Actual sprite collectors, preparation, descriptor packing and GPU submission.
// Only external view/light/shader/buffer collaborators are fixtures; no pixel/game claim.
namespace {
struct ParticleParameterFixture : dice::hfe::dx9rend::ParameterOwner
{
    std::vector<unsigned int> counts;
    std::vector<const void*> values;
    int getRef() const { return 1; }
    int addRef() { return 2; }
    int release() { return 1; }
    void* getD3DXParam() const { return 0; }
    void setD3DXParam(void*) { std::abort(); }
    bool isParameterUsed() const { return true; }
    void setValue(const void* p,unsigned int n) { values.push_back(p); counts.push_back(n); }
    void setBool(unsigned char) { std::abort(); }
    void setInt(int) { std::abort(); }
    void setFloat(float) {}
    void setMatrixPointerArray(const void* const*,unsigned int) { std::abort(); }
    unsigned int getTypeCode() const { return 0; }
};
Mat4 activeIdentity;
unsigned char activeFrustumStorage[sizeof(Frustum)] = {};
float activeViewport[4] = {0,0,800,600};
int activeViewReads = 0, activeInverseReads = 0, activeProjectionReads = 0, hemiReads = 0;
Mat4* __fastcall activeView(void*,void*) { ++activeViewReads; return &activeIdentity; }
Mat4* __fastcall activeInverse(void*,void*) { ++activeInverseReads; return &activeIdentity; }
Mat4* __fastcall activeProjection(void*,void*) { ++activeProjectionReads; return &activeIdentity; }
float __fastcall activeNear(void*,void*) { return 1.0f; }
float* __fastcall activeViewportRead(void*,void*) { return activeViewport; }
Frustum* __fastcall activeFrustum(void*,void*) { return reinterpret_cast<Frustum*>(activeFrustumStorage); }
int __fastcall activeKind(void*,void*) { return 0; }
unsigned int __fastcall activeFrame(void*,void*) { return 1234; }
HemiMap* __fastcall activeHemi(void*,void*) { ++hemiReads; return 0; }
Vec3 activeColor = {1,1,1};
const Vec3& __fastcall activeLightColor(void*,void*) { return activeColor; }

void resetParticleGpu()
{
    using namespace particle_renderer_fixture;
    gpuLocks.clear(); gpuDraws.clear(); gpuStates.clear(); gpuEvents.clear(); gpuBytes.clear();
    gpuIndexedDraws.clear(); gpuIndexBuffers.clear();
}

void verifyActiveParticles()
{
    using namespace dice::hfe::dx9rend;
    using namespace particle_renderer_fixture;
    allowGpu = true;
    ParticleSystemManager manager = {};
    g_ParticleSystemManager = reinterpret_cast<IBase*>(&manager);
    manager.m_farDistances[0] = 100.0f;
    manager.m_inverseFarRanges[0] = 0.01f;
    manager.m_shaderQuality = 2;
    manager.m_value8CA4 = 1;
    unsigned char* renderer = manager.m_particleSystemRenderer;
    renderer[0x1D9C] = 1;
    unsigned char* sorted[32] = {};
    unsigned char* additive[32] = {};
    unsigned char mapping[0x800];
    unsigned char parameters[1120] = {};
    psmDrawField<unsigned char**>(renderer,0x18) = sorted;
    psmDrawField<unsigned char**>(renderer,0x1C) = additive;
    psmDrawField<unsigned char*>(renderer,0x1DC0) = mapping;
    psmDrawField<unsigned char*>(renderer,0x1DC8) = parameters;
    psmDrawField<unsigned int>(renderer,0x1DB8) = 4096;
    psmDrawField<unsigned int>(renderer,0x1E14) = 0x7FFF0000;
    ParticleParameterFixture shader;
    const unsigned int parameterOffsets[] = {0x1DD4,0x1DD8,0x1DDC,0x1DE0,0x1DE4,0x1DE8,
        0x1DEC,0x1DF0,0x1DF4,0x1DF8,0x1DFC,0x1E00,0x1E04,0x1E08};
    for (unsigned int offset:parameterOffsets) psmDrawField<ParameterOwner*>(renderer,offset)=&shader;
    void* localViewTable[0x9C/4] = {};
    void* localViewObject = localViewTable;
    localViewTable[0x14/4]=reinterpret_cast<void*>(&activeViewportRead);
    localViewTable[0x34/4]=reinterpret_cast<void*>(&activeNear);
    localViewTable[0x64/4]=reinterpret_cast<void*>(&activeView);
    localViewTable[0x68/4]=reinterpret_cast<void*>(&activeInverse);
    localViewTable[0x6C/4]=reinterpret_cast<void*>(&activeProjection);
    localViewTable[0x74/4]=reinterpret_cast<void*>(&activeFrustum);
    IRenderView* view = reinterpret_cast<IRenderView*>(&localViewObject);
    activeIdentity = Mat4::getIdentity();
    void* hemiTable[14] = {}; hemiTable[6]=reinterpret_cast<void*>(&activeHemi);
    void* hemiObject=hemiTable;
    g_HemiMapManager=reinterpret_cast<IHemiMapManager*>(&hemiObject);
    void* lightTable[0x274/4] = {};
    lightTable[0x260/4]=lightTable[0x268/4]=reinterpret_cast<void*>(&activeLightColor);
    void* lightObject=lightTable; g_LightManager=reinterpret_cast<LightManager*>(&lightObject);
    rendererTable[0x54/4]=reinterpret_cast<void*>(&activeFrame);
    unsigned char descriptor[0x2A4] = {};
    psmDrawField<int>(descriptor,8)=-1;
    psmDrawField<float>(descriptor,0x108)=1;
    psmDrawField<float>(descriptor,0xD0)=4;
    descriptor[0xD4]=1;
    psmDrawField<unsigned int>(descriptor,0xE0)=4;
    psmDrawField<unsigned int>(descriptor,0xE4)=2;
    psmDrawField<float>(descriptor,0xE8)=0.5f;
    psmDrawField<float>(descriptor,0x13C)=0.5f;
    psmDrawField<float>(descriptor,0x140)=0.5f;
    psmDrawField<void*>(descriptor,0x2A0)=reinterpret_cast<void*>(&particleRendererCollectSorted);
    g_templatesById[0]=reinterpret_cast<IParticleSystemTemplate*>(descriptor);
    unsigned char instance[0x170] = {};
    void* instanceTable[2] = {0,reinterpret_cast<void*>(&activeKind)};
    psmDrawField<void*>(instance,0)=instanceTable;
    unsigned char particles[3][0x94] = {};
    unsigned char* particlePointers[3] = {particles[0],particles[1],particles[2]};
    psmDrawField<unsigned char**>(instance,0x24)=particlePointers;
    psmDrawField<unsigned int>(instance,0x28)=3;
    psmDrawField<unsigned char*>(instance,0x2C)=descriptor;
    const float depths[3] = {2,20,10};
    for(unsigned int i=0;i<3;++i)
    {
        unsigned char* p=particles[i];
        psmDrawField<float>(p,8)=2;
        psmDrawField<float>(p,0xC)=3;
        psmDrawField<float>(p,0x10)=4;
        psmDrawField<float>(p,0x20)=depths[i];
        psmDrawField<float>(p,0x24)=0.5f;
        psmDrawField<float>(p,0x2C)=psmDrawField<float>(p,0x58)=1;
        psmDrawField<unsigned int>(p,0x54)=0x80;
    }
    instance[0x14C]=1;
    IParticleSystemInstance* item=reinterpret_cast<IParticleSystemInstance*>(instance);
    ParticleSystemInstanceList list(1,item);
    psmPrepareParticleRenderer(renderer,view,&list,0.25f,false);
    CHECK(psmDrawField<unsigned int>(renderer,0x1C38)==3);
    CHECK(sorted[0]==particles[0] && sorted[1]==particles[2] && sorted[2]==particles[1]);
    CHECK(psmDrawField<unsigned int>(instance,0x124)==1234 && instance[0x128]==1);
    CHECK(activeViewReads==6 && activeInverseReads==2 && activeProjectionReads==1);
    CHECK(psmDrawField<float>(particles[0],0x78)==1 && !(psmDrawField<unsigned int>(particles[0],0x54)&0x80));
    CHECK(psmDrawField<unsigned short>(particles[0],0x40)==16383);
    CHECK(psmSelectParticleRenderer(renderer,0));
    resetParticleGpu();
    CHECK(psmDrawParticleRenderer(renderer,view));
    CHECK(gpuDraws.size()==1 && gpuDraws[0].count==6 && gpuDraws[0].type==4);
    CHECK(gpuLocks.size()==1 && gpuLocks[0].count==18 && gpuLocks[0].flags==10240);
    CHECK(gpuEvents.front()=="push" && gpuEvents.back()=="pop");
    CHECK(hemiReads==2 && renderer[0x1D6C]==1 && renderer[0x1D78]==0);
    CHECK(psmDrawField<int>(renderer,4)==-1 && psmDrawField<unsigned int>(renderer,0x1C38)==0);
    CHECK(psmDrawField<float>(gpuDraws[0].bytes.data(),8)==20);
    CHECK(psmDrawField<float>(gpuDraws[0].bytes.data()+6*64,8)==10);
    CHECK(psmDrawField<float>(gpuDraws[0].bytes.data()+12*64,8)==2);
    CHECK(psmDrawField<float>(gpuDraws[0].bytes.data(),0x28)==3);
    CHECK(psmDrawField<float>(gpuDraws[0].bytes.data()+2*64,0x28)==2);
    CHECK(psmDrawField<unsigned int>(gpuDraws[0].bytes.data(),0x30)==0x7FFF0000);
    CHECK(psmDrawField<unsigned short>(gpuDraws[0].bytes.data(),0x3C)==0x8000);

    // First-person filtering and far-alpha culling happen before collection.
    instance[8]=1;
    psmPrepareParticleRenderer(renderer,view,&list,0.5f,false);
    CHECK(psmDrawField<unsigned int>(renderer,0x1C38)==0);
    psmDrawField<float>(descriptor,0x9C)=0.3f;
    psmDrawField<float>(particles[0],0x20)=0.5f;
    psmDrawField<float>(particles[1],0x20)=150;
    psmPrepareParticleRenderer(renderer,view,&list,0.5f,true);
    CHECK(psmDrawField<unsigned int>(renderer,0x1C38)==1 && sorted[0]==particles[2]);
    CHECK(std::fabs(psmDrawField<float>(particles[1],0x2C)-0.25f)<0.000001f);
    psmDrawField<unsigned int>(renderer,0x1C38)=0;

    // Additive groups retain the pre-fade alpha test and use integer distance bits.
    instance[8]=0;
    psmDrawField<float>(descriptor,0x9C)=0;
    psmDrawField<void*>(descriptor,0x2A0)=reinterpret_cast<void*>(&particleRendererCollectAdditive);
    for(unsigned int i=0;i<3;++i) psmDrawField<float>(particles[i],0x20)=depths[i];
    psmPrepareParticleRenderer(renderer,view,&list,0.25f,false);
    CHECK(psmDrawField<unsigned int>(renderer,0x1824)==1 && psmDrawField<unsigned int>(renderer,0x28)==3);
    resetParticleGpu();
    CHECK(psmSelectParticleRenderer(renderer,0) && psmDrawParticleRenderer(renderer,view));
    CHECK(gpuDraws.size()==1 && gpuDraws[0].count==6);
    CHECK(psmDrawField<unsigned int>(renderer,0x28)==0 && psmDrawField<int>(renderer,0x24)==-1);
    CHECK(gpuStates.size()==3 && gpuStates[0].second==2 && gpuStates[1].second==2 && gpuStates[2].second==0);

    // Eleven descriptor IDs split at the shipped ten-descriptor shader limit.
    unsigned char moreParticles[11][0x94] = {};
    unsigned char descriptors[11][0x2A4] = {};
    for(unsigned int i=0;i<11;++i)
    {
        psmDrawField<unsigned short>(moreParticles[i],4)=static_cast<unsigned short>(i);
        psmDrawField<float>(moreParticles[i],0x14)=static_cast<float>(i+1);
        sorted[i]=moreParticles[i];
        g_templatesById[static_cast<unsigned short>(i)]=reinterpret_cast<IParticleSystemTemplate*>(descriptors[i]);
    }
    psmDrawField<unsigned int>(renderer,0x1C38)=11;
    psmDrawField<int>(renderer,4)=10;
    psmDrawField<unsigned int>(renderer,0x5E14)=60;
    psmDrawField<unsigned int>(renderer,0x1DB8)=100;
    resetParticleGpu();
    CHECK(psmDrawSortedParticles(renderer,0));
    CHECK(gpuLocks.size()==2 && gpuLocks[0].count==60 && gpuLocks[1].count==6);
    CHECK(gpuLocks[0].offset==0 && gpuLocks[0].flags==10240 && gpuLocks[1].offset==60 && gpuLocks[1].flags==6144);
    CHECK(gpuDraws.size()==2 && gpuDraws[0].count==20 && gpuDraws[1].count==2);
    CHECK(psmDrawField<unsigned int>(renderer,0x1DC4)==1 && mapping[0]==0);
    g_templatesById.clear();
    // Release the retained-instance list the prepare pass grew in the zeroed renderer span.
    std::vector<unsigned char*>().swap(particleRendererList<unsigned char*>(renderer,8));
    allowGpu=false;
    g_HemiMapManager=0; g_ParticleSystemManager=0; g_LightManager=0;
}
}

namespace dice { namespace hfe { namespace geom {
void* staticMeshRootParent(void*) { std::abort(); }
} } }
