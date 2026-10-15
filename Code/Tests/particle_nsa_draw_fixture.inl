// [TEMP] Actual NSA prepare/collect/pack/GPU bodies, with observed external GPU collaborators.
namespace {
int __fastcall nsaKind(void*, void*) { return 4; }

void verifyNsaParticles()
{
    using namespace dice::hfe::dx9rend;
    using namespace particle_renderer_fixture;
    allowGpu = true; gpuStride = 72;
    ParticleSystemManager manager = {};
    g_ParticleSystemManager = reinterpret_cast<IBase*>(&manager);
    manager.m_farDistances[0] = 100;
    manager.m_inverseFarRanges[0] = 0.01f;
    manager.m_shaderQuality = 2;
    manager.m_value8CA4 = 1;
    unsigned char* renderer = manager.m_nsaParticleSystemRenderer;
    renderer[0x544] = 1;
    unsigned char* table[32] = {};
    unsigned char mapping[0x800], parameters[1120] = {};
    psmDrawField<unsigned char**>(renderer,0x1C) = table;
    psmDrawField<unsigned char*>(renderer,0x55C) = mapping;
    psmDrawField<unsigned char*>(renderer,0x564) = parameters;
    ParticleParameterFixture shader;
    const unsigned int offsets[] = {0x570,0x574,0x578,0x57C,0x580,0x584,0x588,0x58C,0x590,0x594,0x598};
    for (unsigned int offset : offsets) psmDrawField<ParameterOwner*>(renderer,offset) = &shader;
    void* viewTable[0x9C/4] = {}, *viewObject = viewTable;
    viewTable[0x34/4] = reinterpret_cast<void*>(&activeNear);
    viewTable[0x64/4] = reinterpret_cast<void*>(&activeView);
    viewTable[0x68/4] = reinterpret_cast<void*>(&activeInverse);
    viewTable[0x6C/4] = reinterpret_cast<void*>(&activeProjection);
    viewTable[0x74/4] = reinterpret_cast<void*>(&activeFrustum);
    IRenderView* view = reinterpret_cast<IRenderView*>(&viewObject);
    activeIdentity = Mat4::getIdentity();
    activeViewReads = activeInverseReads = activeProjectionReads = hemiReads = 0;
    void* hemiTable[14] = {}, *hemiObject = hemiTable;
    hemiTable[6] = reinterpret_cast<void*>(&activeHemi);
    g_HemiMapManager = reinterpret_cast<IHemiMapManager*>(&hemiObject);
    void* lightTable[0x274/4] = {}, *lightObject = lightTable;
    lightTable[0x260/4] = lightTable[0x268/4] = reinterpret_cast<void*>(&activeLightColor);
    g_LightManager = reinterpret_cast<LightManager*>(&lightObject);
    rendererTable[0x54/4] = reinterpret_cast<void*>(&activeFrame);
    unsigned char descriptor[0x27C] = {};
    psmDrawField<int>(descriptor,8) = -1;
    psmDrawField<float>(descriptor,0xF8) = 1;
    descriptor[0xC4] = 1;
    psmDrawField<float>(descriptor,0xC0) = 4;
    psmDrawField<unsigned int>(descriptor,0xD0) = 4;
    psmDrawField<unsigned int>(descriptor,0xD4) = 2;
    psmDrawField<float>(descriptor,0xD8) = 0.5f;
    psmDrawField<float>(descriptor,0x12C) = psmDrawField<float>(descriptor,0x130) = 0.5f;
    g_templatesById[0] = reinterpret_cast<IParticleSystemTemplate*>(descriptor);
    unsigned char instance[0x170] = {};
    void* instanceTable[2] = {0,reinterpret_cast<void*>(&nsaKind)};
    psmDrawField<void*>(instance,0) = instanceTable;
    unsigned char particles[3][200] = {};
    unsigned char* particlePointers[3] = {particles[0],particles[1],particles[2]};
    psmDrawField<unsigned char**>(instance,0x24) = particlePointers;
    psmDrawField<unsigned int>(instance,0x28) = 3;
    psmDrawField<unsigned char*>(instance,0x2C) = descriptor;
    const float depths[] = {2,20,10};
    const float corners[12] = {-1,0,1, 1,0,1, 1,0,-1, -1,0,-1};
    for (unsigned int i=0; i<3; ++i)
    {
        unsigned char* p = particles[i];
        psmDrawField<float>(p,0x18) = depths[i];
        psmDrawField<float>(p,0x24) = 0.5f;
        psmDrawField<unsigned int>(p,0x28) = 0x80;
        psmDrawField<float>(p,0x30) = 0.75f;
        psmDrawField<float>(p,0x38) = 1;
        psmDrawField<float>(p,0x48) = 3;
        psmDrawField<float>(p,0x4C) = 4;
        psmDrawField<float>(p,0x8C) = 2;
        ::memcpy(p+0x98,corners,sizeof(corners));
    }
    instance[0x14C] = 1;
    IParticleSystemInstance* item = reinterpret_cast<IParticleSystemInstance*>(instance);
    ParticleSystemInstanceList list(1,item);
    psmPrepareNsaParticleRenderer(renderer,view,&list,0.25f,false);
    CHECK(psmDrawField<unsigned int>(renderer,0x428)==3 && psmDrawField<int>(renderer,8)==2);
    CHECK(table[0]==particles[0] && table[1]==particles[2] && table[2]==particles[1]);
    CHECK(psmDrawField<unsigned int>(instance,0x124)==1234 && instance[0x128]==1);
    CHECK(activeViewReads==6 && activeInverseReads==2 && activeProjectionReads==1);
    CHECK(psmDrawField<float>(particles[0],0x80)==1 && !(psmDrawField<unsigned int>(particles[0],0x28)&0x80));
    CHECK(psmDrawField<short>(particles[0],0x1C)==0 && psmDrawField<short>(particles[0],0x1E)==32767);
    CHECK(psmDrawField<short>(particles[0],0x40)==16383);
    resetParticleGpu();
    CHECK(psmSelectNsaParticleRenderer(renderer,0) && psmDrawNsaParticleRenderer(renderer,view));
    CHECK(gpuDraws.size()==1 && gpuDraws[0].count==6 && gpuDraws[0].bytes.size()==18*72);
    CHECK(gpuLocks.size()==1 && gpuLocks[0].count==18 && gpuLocks[0].flags==10240);
    CHECK(gpuEvents.front()=="push" && gpuEvents.back()=="pop");
    CHECK(std::find(gpuEvents.begin(),gpuEvents.end(),"technique:NSAParticleMedium")!=gpuEvents.end());
    CHECK(hemiReads==2 && renderer[0x518]==1 && renderer[0x520]==0);
    CHECK(psmDrawField<int>(renderer,8)==-1 && psmDrawField<unsigned int>(renderer,0x428)==0);
    CHECK(psmDrawField<unsigned int>(renderer,0x420)==3 && psmDrawField<unsigned int>(renderer,0x444)==18);
    CHECK(manager.m_drawCounters[1]==1); // retail +0x8C48, the NSA draw counter
    const unsigned int cornerOrder[] = {0,1,3,3,1,2};
    for (unsigned int i=0;i<6;++i)
    {
        unsigned char* v=gpuDraws[0].bytes.data()+i*72;
        CHECK(psmDrawField<float>(v,8)==20);
        CHECK(::memcmp(v+0x20,corners+cornerOrder[i]*3,12)==0);
        CHECK(psmDrawField<float>(v,0x14)==0.75f && psmDrawField<float>(v,0x18)==1);
        CHECK(psmDrawField<float>(v,0x1C)==0); // fraction, not the unfaded-alpha field.
        CHECK(psmDrawField<float>(v,0x2C)==(i==0||i==1||i==4 ? 3 : 2));
        CHECK(psmDrawField<short>(v,0x36)==32767);
    }

    // Actual first-person exclusion, near rejection, far quadratic fade and retained dirty state.
    instance[8]=1;
    psmPrepareNsaParticleRenderer(renderer,view,&list,0.25f,false);
    CHECK(psmDrawField<unsigned int>(renderer,0x428)==0);
    psmDrawField<float>(descriptor,0x94)=0.3f;
    psmDrawField<float>(particles[0],0x18)=0.5f;
    psmDrawField<float>(particles[1],0x18)=150;
    psmPrepareNsaParticleRenderer(renderer,view,&list,0.25f,true);
    CHECK(psmDrawField<unsigned int>(renderer,0x428)==1 && table[0]==particles[2]);
    CHECK(std::fabs(psmDrawField<float>(particles[1],0x34)-0.25f)<0.000001f);
    CHECK(psmDrawField<float>(particles[2],0x80)==1);
    psmDrawField<unsigned int>(renderer,0x428)=0;

    // Motion-aligned orientation projects onto the particle plane; altitude packing uses camera Y.
    descriptor[0x210]=descriptor[0xA0]=1;
    descriptor[0xC4]=0;
    unsigned char* p=particles[0];
    psmDrawField<float>(p,0x68)=1;
    psmDrawField<float>(p,0x6C)=psmDrawField<float>(p,0x70)=0;
    psmDrawField<unsigned int>(p,0x28)|=0x80;
    activeIdentity.m[13]=200;
    psmUpdateNsaDrawState(renderer,p,descriptor,instance,view);
    CHECK(std::fabs(psmDrawField<float>(p,0x88)-1.57079637f)<0.000001f);
    CHECK(psmDrawField<short>(p,0x1C)==32766 || psmDrawField<short>(p,0x1C)==32767);
    CHECK(psmDrawField<short>(p,0x22)==16383);
    activeIdentity.m[13]=0;

    // Ten-template cap, texture changes, unsigned threshold split and retained VB offset.
    unsigned char more[11][200] = {}, descriptors[11][0x27C] = {};
    for (unsigned int i=0;i<11;++i)
    {
        psmDrawField<unsigned short>(more[i],0x2C)=static_cast<unsigned short>(i);
        psmDrawField<float>(more[i],0x94)=static_cast<float>(i+1);
        table[i]=more[i];
        g_templatesById[static_cast<unsigned short>(i)]=reinterpret_cast<IParticleSystemTemplate*>(descriptors[i]);
    }
    psmDrawField<unsigned int>(renderer,0x428)=11; psmDrawField<int>(renderer,8)=10;
    psmDrawField<unsigned int>(renderer,0x444)=0;
    resetParticleGpu();
    CHECK(psmDrawNsaParticles(renderer,0));
    CHECK(gpuDraws.size()==2 && gpuDraws[0].count==20 && gpuDraws[1].count==2);
    CHECK(gpuLocks[0].count==66 && gpuLocks[1].count==6);
    CHECK(gpuLocks[0].flags==10240 && gpuLocks[1].flags==4096 && gpuLocks[1].offset==60);
    CHECK(psmDrawField<unsigned int>(renderer,0x444)==66 && psmDrawField<int>(renderer,8)==-1);
    psmDrawField<unsigned int>(renderer,0x428)=3; psmDrawField<int>(renderer,8)=2;
    resetParticleGpu();
    CHECK(psmDrawNsaParticles(renderer,bits(2.0f)));
    CHECK(gpuDraws.size()==1 && gpuDraws[0].count==4 && gpuLocks[0].offset==66 && gpuLocks[0].flags==6144);
    CHECK(psmDrawField<unsigned int>(renderer,0x428)==1 && psmDrawField<int>(renderer,8)==0);
    psmDrawField<void*>(more[0],0x58)=reinterpret_cast<void*>(0x1010);
    psmDrawField<unsigned int>(renderer,0x428)=2; psmDrawField<int>(renderer,8)=1;
    resetParticleGpu();
    CHECK(psmDrawNsaParticles(renderer,0));
    CHECK(gpuDraws.size()==2 && gpuDraws[0].count==2 && gpuDraws[1].count==2);

    // Retail lock failure returns false after effect begin, and the wrapper still pops state.
    psmDrawField<unsigned int>(renderer,0x428)=2; psmDrawField<int>(renderer,8)=1;
    renderer[0x520]=1; resetParticleGpu(); failGpuLockAt=1;
    CHECK(!psmDrawNsaParticleRenderer(renderer,view));
    CHECK(psmDrawField<unsigned int>(renderer,0x428)==2 && psmDrawField<int>(renderer,8)==1);
    CHECK(renderer[0x520]==0 && gpuEvents.back()=="pop");
    CHECK(std::find(gpuEvents.begin(),gpuEvents.end(),"end")==gpuEvents.end());
    resetParticleGpu(); failGpuLockAt=2;
    CHECK(!psmDrawNsaParticles(renderer,0));
    CHECK(gpuDraws.size()==1 && psmDrawField<unsigned int>(renderer,0x428)==1);
    CHECK(psmDrawField<int>(renderer,8)==1); // Retail writes the final index only on success.
    CHECK(std::find(gpuEvents.begin(),gpuEvents.end(),"end")==gpuEvents.end());
    failGpuLockAt=0;
    g_templatesById.clear();
    std::vector<unsigned char*>().swap(particleRendererList<unsigned char*>(renderer,0xC));
    allowGpu=false; gpuStride=64;
    g_HemiMapManager=0; g_ParticleSystemManager=0; g_LightManager=0;
}
}
