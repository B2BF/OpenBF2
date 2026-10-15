// [TEMP] actual ordinary instance, pool, integration and manager queue bodies. Resource descriptor
// values and camera/host callbacks are explicit models; no game process or GPU is launched.
#define main particleManagerFixtureMain
#include "particle_update_runtime.cpp"
#undef main

namespace {
void* descriptorTable[89]={};
int __fastcall descriptorZero(void*,void*) { return 0; }
float __fastcall descriptorFloatZero(void*,void*) { return 0; }
Mat4* __fastcall instanceViewMatrix(void*,void*) { return &cameraMatrix; }
}

int main()
{
    ParticleSystemManager manager={}; manager.m_vptr=const_cast<const void**>(kVtable);
    manager.m_poolsInitialized=1; manager.m_qualityLevel=2; manager.m_effectiveViewDistance=1000;
    manager.m_type0PoolCount=8;
    psmAllocatePool(manager.m_type0PoolBacking,manager.m_type0FreeList,manager.m_type0PoolCount,148,0);
    g_ParticleSystemManager=reinterpret_cast<IBase*>(&manager);
    unsigned char descriptorBytes[0x2A4]={};
    descriptorTable[3]=reinterpret_cast<void*>(&descriptorZero);
    descriptorTable[9]=reinterpret_cast<void*>(&descriptorZero);
    descriptorTable[0x130/4]=reinterpret_cast<void*>(&descriptorZero);
    descriptorTable[0x148/4]=reinterpret_cast<void*>(&descriptorFloatZero);
    *reinterpret_cast<void***>(descriptorBytes)=descriptorTable;
    IParticleSystemTemplate* descriptor=reinterpret_cast<IParticleSystemTemplate*>(descriptorBytes);
    psmRuntimeField<float>(descriptor,0x84)=1;
    psmRuntimeField<float>(descriptor,0x10)=2;
    psmRuntimeField<float>(descriptor,0x98)=1;
    psmRuntimeField<float>(descriptor,0x128)=1;
    psmRuntimeField<float>(descriptor,0x118)=1;
    psmRuntimeField<float>(descriptor,0x138)=1;
    psmRuntimeField<float>(descriptor,0x1D0)=1;
    psmRuntimeField<float>(descriptor,0x1F8)=1;
    cameraMatrix=Mat4::getIdentity();
    viewTable[0x64/4]=reinterpret_cast<void*>(&instanceViewMatrix);
    unsigned char shaderConstants[0x1C0]={};
    dx9rend::g_raShaderConstants=shaderConstants;
    currentManager=&manager; previousDeltaBits=0;

    IParticleSystemInstance* instance=psm_createInstance(&manager,0,descriptor);
    CHECK(instance!=0 && manager.m_instances.size()==1);
    CHECK(instance->reserved34()==12 && instance->reserved39()==0);
    CHECK(instance->reserved31()==1 && instance->reserved25()==1 && instance->reserved27()==1);
    CHECK(!psmRuntimeField<unsigned char>(instance,5) && !psmRuntimeField<unsigned char>(instance,8));
    TestController controller; instance->setEffectController(&controller);
    instance->reserved03(true);
    CHECK(instance->reserved05() && controller.starts==1 && manager.m_renderBuckets[7].size()==1);
    CHECK(psmRuntimeField<unsigned char>(instance,0x128)==1);
    float* particle=psmType0NewParticle(instance);
    CHECK(particle && instance->reserved39()==1 && manager.m_type0LiveCount==1);
    particle[18]=particle[19]=particle[20]=0; particle[24]=4; particle[25]=particle[26]=0;
    particle[27]=0; particle[28]=1; particle[29]=1; particle[9]=0;
    CHECK(psmType0AdvanceParticle(instance,particle,0,0.1f,1)==1);
    CHECK(::fabs(particle[18]-0.4f)<0.000001f && particle[24]==4 && particle[6]==particle[18]);
    CHECK(::fabs(particle[27]-0.1f)<0.000001f && (reinterpret_cast<unsigned char*>(particle)[0x54]&0x80));

    // Exercise the virtual frame update and its retained 0.01-second remainder, rather than
    // only calling the integrator directly. Stopped emission still advances existing particles.
    instance->reserved04();
    instance->reserved41(0.035f);
    CHECK(::fabs(particle[27]-0.13f)<0.000001f && ::fabs(particle[18]-0.52f)<0.000001f);
    CHECK(::fabs(psmRuntimeField<float>(instance,0xF4)-0.005f)<0.000001f);
    instance->reserved41(0.005f);
    CHECK(::fabs(particle[27]-0.14f)<0.000001f && ::fabs(particle[18]-0.56f)<0.000001f);
    CHECK(::fabs(psmRuntimeField<float>(instance,0x114)-particle[6])<0.000001f);

    instance->setPaused(true); const float age=particle[27];
    psmRuntimeField<int>(descriptor,8)=7; instance->reserved41(0.2f);
    CHECK(particle[27]==age && psmRuntimeField<int>(descriptor,8)==-1);
    instance->setPaused(false);
    psmRuntimeField<unsigned char>(descriptor,0xEC)=1;
    psmRuntimeField<float>(descriptor,0xF0)=0.5f; psmRuntimeField<float>(descriptor,0xF4)=0.8f;
    const float plane[4]={0,1,0,0}; instance->reserved18(plane);
    particle[18]=0; particle[19]=-0.1f; particle[20]=0;
    particle[24]=2; particle[25]=-2; particle[26]=0;
    particle[27]=0; psmRuntimeField<float>(instance,0x21C)=0;
    CHECK(psmType0AdvanceParticle(instance,particle,0,0.0f,0)==1);
    CHECK(particle[19]==0 && particle[25]==1 && ::fabs(particle[24]-1.6f)<0.000001f);

    psmRuntimeField<unsigned char>(descriptor,0xEC)=0;
    psmRuntimeField<unsigned char>(descriptor,0x21D)=1;
    Mat4 placement=Mat4::getIdentity(); placement.m[12]=10; placement.m[13]=20; placement.m[14]=30;
    instance->reserved14(&placement); psmType0RefreshTransform(instance);
    particle[18]=1; particle[19]=2; particle[20]=3;
    particle[24]=particle[25]=particle[26]=0;
    CHECK(psmType0AdvanceParticle(instance,particle,0,0.0f,0)==1);
    CHECK(particle[6]==11 && particle[7]==22 && particle[8]==33);
    particle[27]=0.9f;
    CHECK(psmType0AdvanceParticle(instance,particle,0,0.2f,1)==0);
    CHECK(instance->reserved39()==0 && psmRuntimeField<float**>(instance,0x24)[0]==0);
    CHECK(manager.m_type0FreeList==particle && manager.m_type0LiveCount==0);
    instance->reserved04(); instance->reserved41(0.1f);
    CHECK(manager.m_pendingClassification.size()==1);
    psmRemoveActive(&manager,false); CHECK(controller.stops==1 && manager.m_renderBuckets[7].size()==0);
    psm_removeInstance(&manager,0,instance); psmDrainDestroy(&manager);
    CHECK(manager.m_instances.size()==0);
    freeList(manager.m_instances); freeList(manager.m_pendingDestroy); freeList(manager.m_pendingClassification);
    for(int i=0;i<10;++i) freeList(manager.m_renderBuckets[i]);
    cleanup();
    std::printf("Ordinary particle lifecycle/integration: %d/%d checks passed\n",checks-failures,checks);
    return failures?1:0;
}
