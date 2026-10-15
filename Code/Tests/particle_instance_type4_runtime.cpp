// [TEMP] Actual NSA instance/factory/physics/queues. Descriptor, camera, wind and water are
// explicit collaborators; this console fixture does not launch a game, level or GPU.
#define main particleManagerFixtureMain
#include "particle_update_runtime.cpp"
#undef main

namespace {
void* descriptorTable[89]={};
void* windTable[19]={}; void* windObject=windTable;
void* waterTable[48]={}; void* waterObject=waterTable;
int windReads=0,waterReads=0;
float farPlane=321;
std::vector<float> farWrites;
int __fastcall descriptorFour(void*,void*) { return 4; }
int __fastcall descriptorZero(void*,void*) { return 0; }
Mat4* __fastcall instanceViewMatrix(void*,void*) { return &cameraMatrix; }
float __fastcall instanceFarRead(void*,void*) { return farPlane; }
void __fastcall instanceFarWrite(void*,void*,float value) { farPlane=value; farWrites.push_back(value); }
Vec3* __fastcall windRead(void* self,void*,Vec3* result,const Vec3& position)
{ CHECK(self==&windObject); ++windReads; *result={0,0,0}; return result; }
float __fastcall waterRead(void* self,void*,const Vec3& position)
{ CHECK(self==&waterObject); ++waterReads; return 7.0f; }
}

int main()
{
    ParticleSystemManager manager={}; manager.m_vptr=const_cast<const void**>(kVtable);
    manager.m_poolsInitialized=1; manager.m_qualityLevel=2; manager.m_effectiveViewDistance=1000;
    manager.m_type4PoolCount=8;
    psmAllocatePool(manager.m_type4PoolBacking,manager.m_type4FreeList,manager.m_type4PoolCount,200,4);
    g_ParticleSystemManager=reinterpret_cast<IBase*>(&manager);
    unsigned char descriptorBytes[0x264]={};
    descriptorTable[3]=reinterpret_cast<void*>(&descriptorFour);
    descriptorTable[9]=reinterpret_cast<void*>(&descriptorZero);
    *reinterpret_cast<void***>(descriptorBytes)=descriptorTable;
    IParticleSystemTemplate* descriptor=reinterpret_cast<IParticleSystemTemplate*>(descriptorBytes);
    psmRuntimeField<float>(descriptor,0x7C)=1;
    psmRuntimeField<float>(descriptor,0x10)=2;
    psmRuntimeField<float>(descriptor,0x118)=1;
    psmRuntimeField<float>(descriptor,0x108)=1;
    psmRuntimeField<float>(descriptor,0x128)=1;
    psmRuntimeField<float>(descriptor,0x1C0)=1;
    cameraMatrix=Mat4::getIdentity();
    viewTable[0x64/4]=reinterpret_cast<void*>(&instanceViewMatrix);
    viewTable[0x38/4]=reinterpret_cast<void*>(&instanceFarWrite);
    viewTable[0x3C/4]=reinterpret_cast<void*>(&instanceFarRead);
    windTable[0x3C/4]=reinterpret_cast<void*>(&windRead);
    waterTable[0x94/4]=reinterpret_cast<void*>(&waterRead);
    currentManager=&manager; previousDeltaBits=0;

    IParticleSystemInstance* instance=psm_createInstance(&manager,0,descriptor);
    CHECK(instance!=0 && manager.m_instances.size()==1 && instance->reserved01()==4);
    CHECK(allocationSizes[allocationSizes.size()-2]==0x228);
    CHECK(instance->reserved34()==12 && instance->reserved39()==0);
    CHECK(instance->reserved31()==1 && instance->reserved25()==1 && instance->reserved27()==1);
    CHECK(psmRuntimeField<unsigned int>(instance,0xC)==0xA5A5A5A5u
        && psmRuntimeField<unsigned int>(instance,0x10)==0xA5A5A5A5u);
    instance->setPlayer(reinterpret_cast<void*>(17)); instance->setMaterial(18);
    CHECK(instance->getPlayer()==0 && instance->getMaterial()==-1
        && instance->getGasCloudRadius()==1 && !instance->getWaterSide());
    TestController controller; instance->setEffectController(&controller);
    instance->reserved03(true);
    CHECK(instance->reserved05() && controller.starts==1 && manager.m_renderBuckets[8].size()==1);
    CHECK(farPlane==321 && farWrites.size()==2 && farWrites[0]==1000 && farWrites[1]==321);
    CHECK(psmRuntimeField<unsigned char>(instance,0x128)==1);
    cameraMatrix.m[12]=fromBits(0x7FC00000);
    instance->reserved03(true);
    CHECK(psmRuntimeField<unsigned char>(instance,0x128)==1
        && psmRuntimeField<unsigned int>(instance,0x124)==100 && farPlane==321);
    cameraMatrix=Mat4::getIdentity();
    float* particle=psmType4NewParticle(instance);
    CHECK(particle && instance->reserved39()==1 && manager.m_type4LiveCount==1);
    particle[1]=particle[2]=particle[3]=0; particle[23]=4; particle[24]=particle[25]=0;
    particle[29]=0; particle[30]=1; particle[31]=1; particle[9]=0;
    CHECK(psmType4AdvanceParticle(instance,particle,0,0.1f,1)==1);
    CHECK(::fabs(particle[1]-0.4f)<0.000001f && particle[23]==4 && particle[4]==particle[1]);
    CHECK(::fabs(particle[29]-0.1f)<0.000001f && (reinterpret_cast<unsigned char*>(particle)[0x28]&0x80));
    instance->reserved04(); instance->reserved41(0.035f);
    CHECK(::fabs(particle[29]-0.13f)<0.000001f && ::fabs(particle[1]-0.52f)<0.000001f);
    CHECK(::fabs(psmRuntimeField<float>(instance,0xF4)-0.005f)<0.000001f);
    instance->reserved41(0.005f);
    CHECK(::fabs(particle[29]-0.14f)<0.000001f && ::fabs(particle[1]-0.56f)<0.000001f);
    instance->setPaused(true); const float age=particle[29];
    psmRuntimeField<int>(descriptor,8)=7; instance->reserved41(0.2f);
    CHECK(particle[29]==age && psmRuntimeField<int>(descriptor,8)==-1);
    instance->setPaused(false);

    // NSA queries the borrowed host wind object even when the manager wind-enabled byte is false.
    manager.m_windManager=reinterpret_cast<IBase*>(&windObject);
    psmRuntimeField<float>(descriptor,0x9C)=1;
    CHECK(!manager.m_windEnabled && psmType4AdvanceParticle(instance,particle,0,0,0)==1);
    CHECK(windReads==1);
    psmRuntimeField<float>(descriptor,0x9C)=0;
    psmRuntimeField<unsigned char>(descriptor,0xDC)=1;
    psmRuntimeField<float>(descriptor,0xE0)=0.5f; psmRuntimeField<float>(descriptor,0xE4)=0.8f;
    const float plane[4]={0,1,0,0}; instance->reserved18(plane);
    particle[1]=0; particle[2]=-0.1f; particle[3]=0;
    particle[23]=2; particle[24]=-2; particle[25]=0;
    particle[29]=0; psmRuntimeField<float>(instance,0x21C)=0;
    CHECK(psmType4AdvanceParticle(instance,particle,0,0,0)==1);
    CHECK(particle[2]==0 && particle[24]==1 && ::fabs(particle[23]-1.6f)<0.000001f);

    psmRuntimeField<unsigned char>(descriptor,0xDC)=0;
    psmRuntimeField<unsigned char>(descriptor,0x211)=1;
    Mat4 placement=Mat4::getIdentity(); placement.m[12]=10; placement.m[13]=20; placement.m[14]=30;
    instance->reserved14(&placement); psmType4RefreshTransform(instance);
    particle[1]=1; particle[2]=2; particle[3]=3;
    particle[23]=particle[24]=particle[25]=0;
    CHECK(psmType4AdvanceParticle(instance,particle,0,0,0)==1);
    CHECK(particle[4]==11 && particle[5]==22 && particle[6]==33);
    CHECK(particle[38]==-1 && particle[39]==0 && particle[40]==1
        && particle[47]==-1 && particle[48]==0 && particle[49]==-1);

    psmRuntimeField<unsigned char>(descriptor,0xA0)=1;
    psmRuntimeField<float>(descriptor,0xA4)=0.25f;
    manager.m_heightmapCluster=reinterpret_cast<IBase*>(&waterObject);
    psmType4RefreshTransform(instance);
    CHECK(psmRuntimeField<Mat4>(instance,0x74).m[0]==-1
        && psmRuntimeField<Mat4>(instance,0x74).m[5]==1);
    CHECK(psmType4AdvanceParticle(instance,particle,0,0,0)==1);
    CHECK(waterReads==1 && particle[5]==7.25f && particle[2]==2 && particle[4]==9);
    const Mat4 parallel=psmType4WaterMatrix(Vec3{4,5,6},Vec3{0,1,0},Vec3{0,1,0});
    CHECK(parallel.m[0]==1 && parallel.m[5]==1 && parallel.m[10]==1 && parallel.m[12]==4);

    particle[29]=fromBits(0x7FC00000);
    CHECK(psmType4AdvanceParticle(instance,particle,0,0,0)==1 && instance->reserved39()==1);
    particle[29]=0.9f;
    CHECK(psmType4AdvanceParticle(instance,particle,0,0.2f,1)==0);
    CHECK(instance->reserved39()==0 && psmRuntimeField<float**>(instance,0x24)[0]==0);
    CHECK(manager.m_type4FreeList==particle && manager.m_type4LiveCount==0);
    instance->reserved41(0.1f); CHECK(manager.m_pendingClassification.size()==1);
    psmRemoveActive(&manager,false); CHECK(controller.stops==1 && manager.m_renderBuckets[8].size()==0);
    psm_removeInstance(&manager,0,instance); psmDrainDestroy(&manager);
    CHECK(manager.m_instances.size()==0);
    freeList(manager.m_instances); freeList(manager.m_pendingDestroy); freeList(manager.m_pendingClassification);
    for(int i=0;i<10;++i) freeList(manager.m_renderBuckets[i]);
    cleanup();
    std::printf("NSA particle lifecycle/integration: %d/%d checks passed\n",checks-failures,checks);
    return failures?1:0;
}
