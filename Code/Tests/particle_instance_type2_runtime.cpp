// [TEMP] Actual Trail instance/factory/transform/ring lifecycle; explicit camera/descriptor/host
// collaborators only. No game, level, GPU or network startup.
#define main particleManagerFixtureMain
#include "particle_update_runtime.cpp"
#undef main

namespace {
void* trailDescriptorTable[89]={};
void* trailWaterTable[48]={}; void* trailWaterObject=trailWaterTable;
float trailFarPlane=321; int trailQuality=0,trailWaterReads=0;
std::vector<float> trailFarWrites;
int __fastcall trailType(void*,void*) { return 2; }
int __fastcall trailQualityRead(void*,void*) { return trailQuality; }
Mat4* __fastcall trailViewMatrix(void*,void*) { return &cameraMatrix; }
float __fastcall trailFarRead(void*,void*) { return trailFarPlane; }
void __fastcall trailFarWrite(void*,void*,float value) { trailFarPlane=value; trailFarWrites.push_back(value); }
float __fastcall trailWaterRead(void* self,void*,const Vec3& position)
{ CHECK(self==&trailWaterObject); ++trailWaterReads; CHECK(position.x==10 && position.z==30); return 7; }
float* trailTake(ParticleSystemManager& manager)
{
    float* const p=static_cast<float*>(manager.m_type2FreeList);
    manager.m_type2FreeList=*reinterpret_cast<void**>(p); ::memset(p,0,168); return p;
}
}

int main()
{
    ParticleSystemManager manager={}; manager.m_vptr=const_cast<const void**>(kVtable);
    manager.m_poolsInitialized=1; manager.m_qualityLevel=2; manager.m_effectiveViewDistance=1000;
    manager.m_type2PoolCount=8;
    psmAllocatePool(manager.m_type2PoolBacking,manager.m_type2FreeList,manager.m_type2PoolCount,168,2);
    g_ParticleSystemManager=reinterpret_cast<IBase*>(&manager);
    unsigned char constants[0x200]={}; dx9rend::g_raShaderConstants=constants;
    unsigned char descriptorBytes[0x250]={};
    trailDescriptorTable[3]=reinterpret_cast<void*>(&trailType);
    trailDescriptorTable[9]=reinterpret_cast<void*>(&trailQualityRead);
    *reinterpret_cast<void***>(descriptorBytes)=trailDescriptorTable;
    IParticleSystemTemplate* descriptor=reinterpret_cast<IParticleSystemTemplate*>(descriptorBytes);
    psmRuntimeField<float>(descriptor,0x70)=1; psmRuntimeField<float>(descriptor,0x10)=2;
    psmRuntimeField<unsigned int>(descriptor,0x1D4)=4;
    cameraMatrix=Mat4::getIdentity();
    viewTable[0x64/4]=reinterpret_cast<void*>(&trailViewMatrix);
    viewTable[0x38/4]=reinterpret_cast<void*>(&trailFarWrite);
    viewTable[0x3C/4]=reinterpret_cast<void*>(&trailFarRead);
    trailWaterTable[0x94/4]=reinterpret_cast<void*>(&trailWaterRead);
    currentManager=&manager; previousDeltaBits=0;

    IParticleSystemInstance* instance=psm_createInstance(&manager,0,descriptor);
    CHECK(instance && instance->reserved01()==2 && manager.m_instances.size()==1);
    CHECK(allocationSizes[allocationSizes.size()-2]==0x19C);
    CHECK(allocationSizes.back()==16*4+4 && instance->reserved34()==16 && instance->reserved39()==0);
    CHECK(psmRuntimeField<unsigned int>(instance,0x17C)==15);
    CHECK(instance->getEffectController()==0 && instance->reserved12()==0 && !instance->reserved05());
    CHECK(instance->reserved31()==1 && instance->reserved25()==1 && instance->reserved27()==1 && instance->reserved23()==1);
    CHECK(psmRuntimeField<unsigned int>(instance,0xCC)==0xA5A5A5A5u
        && psmRuntimeField<unsigned int>(instance,0x180)==0xA5A5A5A5u
        && psmRuntimeField<unsigned int>(instance,0x194)==0xA5A5A5A5u);
    float** const ring=psmRuntimeField<float**>(instance,0x14);
    bool ringZero=true; for(unsigned i=0;i<=16;++i) ringZero=ringZero && ring[i]==0;
    CHECK(ringZero);
    instance->setPlayer(reinterpret_cast<void*>(17)); instance->setMaterial(18);
    CHECK(instance->getPlayer()==0 && instance->getMaterial()==-1 && instance->getGasCloudRadius()==1);
    instance->reserved19(2); instance->reserved22(3); instance->reserved24(4); instance->reserved26(5);
    instance->reserved30(6); instance->reserved32(7);
    CHECK(instance->reserved20()==2 && instance->reserved23()==3 && instance->reserved25()==4
        && instance->reserved27()==5 && instance->reserved31()==6 && instance->reserved33()==7);
    instance->reserved16(Vec3{1,2,3}); instance->reserved28(Vec3{4,5,6}); instance->reserved36(Vec3{7,8,9});
    CHECK(instance->reserved17().z==3 && instance->reserved29().y==5 && psmRuntimeField<float>(instance,0x1C)==7);
    unsigned char beforePlane[0x19C]; ::memcpy(beforePlane,instance,sizeof(beforePlane));
    instance->reserved18(reinterpret_cast<void*>(1)); // Proven null body must not touch even this pointer.
    CHECK(::memcmp(beforePlane,instance,sizeof(beforePlane))==0);
    instance->reserved24(1); instance->reserved26(1); instance->reserved30(1); instance->reserved32(0);

    Mat4 local=Mat4::getIdentity(); local.m[12]=10; local.m[13]=20; local.m[14]=30;
    CHECK(instance->reserved14(&local)==reinterpret_cast<unsigned char*>(instance)+0x34);
    psmType2RefreshTransform(instance);
    CHECK(psmRuntimeField<Mat4>(instance,0x74).m[13]==20);
    Mat4 parent=Mat4::getIdentity(); parent.m[12]=1; parent.m[13]=2; parent.m[14]=3;
    instance->reserved13(reinterpret_cast<int>(&parent)); psmType2RefreshTransform(instance);
    CHECK(psmRuntimeField<Mat4>(instance,0x74).m[12]==11 && psmRuntimeField<Mat4>(instance,0x74).m[14]==33);
    instance->reserved13(0);
    psmRuntimeField<unsigned char>(descriptor,0x8C)=1; psmType2RefreshTransform(instance);
    CHECK(psmRuntimeField<Mat4>(instance,0x74).m[0]==1 && psmRuntimeField<float>(instance,0xA8)==20);
    manager.m_heightmapCluster=reinterpret_cast<IBase*>(&trailWaterObject);
    psmRuntimeField<float>(descriptor,0x90)=0.25f; psmType2RefreshTransform(instance);
    CHECK(trailWaterReads==1 && psmRuntimeField<Mat4>(instance,0x74).m[0]==-1
        && psmRuntimeField<float>(instance,0xA8)==7.25f);
    psmRuntimeField<unsigned char>(descriptor,0x8C)=0;

    TestController controller; instance->setEffectController(&controller);
    trailQuality=3; const unsigned seed=psmType0Random.seed;
    instance->reserved03(true);
    CHECK(!instance->reserved05() && controller.starts==0 && psmType0Random.seed==seed && trailFarWrites.empty());
    trailQuality=0; instance->reserved03(true);
    CHECK(instance->reserved05() && controller.starts==1 && manager.m_renderBuckets[5].size()==1);
    CHECK(psmRuntimeField<unsigned char>(instance,6)==0 && !instance->getWaterSide()); // Trails classify as world effects.
    CHECK(trailFarPlane==321 && trailFarWrites.size()==2 && trailFarWrites[0]==1000 && trailFarWrites[1]==321);
    CHECK(psmRuntimeField<unsigned int>(instance,0x120)==100 && instance->reserved10());
    CHECK(psmType0Random.seed!=seed && psmRuntimeField<float>(instance,0x190)>0
        && psmRuntimeField<float>(instance,0x190)<=3 && psmRuntimeField<int>(instance,0x188)==-1);
    cameraMatrix.m[12]=fromBits(0x7FC00000); instance->reserved03(false);
    CHECK(instance->reserved10() && psmRuntimeField<unsigned int>(instance,0x120)==100 && trailFarPlane==321);
    cameraMatrix=Mat4::getIdentity(); cameraMatrix.m[12]=5000; instance->reserved03(false);
    CHECK(!instance->reserved10() && psmRuntimeField<unsigned int>(instance,0x120)==0 && trailFarPlane==321);
    cameraMatrix=Mat4::getIdentity(); instance->reserved03(false);

    // Wrapped ring cleanup: paused clear preserves the actual update refusal, then drains oldest
    // records and severs the next segment's backlink. Active clear restarts(false), as retail does.
    float* first=trailTake(manager); float* second=trailTake(manager);
    ring[15]=first; ring[0]=second; psmRuntimeField<void*>(second,0x88)=first;
    psmRuntimeField<unsigned int>(instance,0x178)=15; psmRuntimeField<unsigned int>(instance,0x174)=1;
    psmRuntimeField<unsigned int>(instance,0x194)=0; psmRuntimeField<unsigned int>(instance,0x28)=2;
    instance->setPaused(true); instance->reserved02();
    CHECK(instance->reserved05() && !instance->reserved07() && instance->reserved39()==0 && controller.starts==1);
    CHECK(psmRuntimeField<unsigned int>(instance,0x174)==0 && psmRuntimeField<unsigned int>(instance,0x178)==0);
    CHECK(ring[0]==0 && ring[15]==0 && psmRuntimeField<void*>(second,0x88)==0);
    CHECK(manager.m_type2FreeList==second && psmRuntimeField<void*>(second,0)==first);
    CHECK(instance->reserved25()==1 && psmRuntimeField<float>(instance,0x128)==1);
    instance->reserved04(); instance->reserved02(); CHECK(!instance->reserved05());

    instance->reserved42(true); CHECK(!psmReclassification(&manager).empty());
    psmReclassify(&manager); CHECK(controller.starts==2 && controller.stops==1 && manager.m_renderBuckets[5].size()==1);
    CHECK(psmRuntimeField<unsigned char>(instance,6)==0);
    instance->reserved08(); CHECK(psmRuntimeField<unsigned char>(instance,0x18)==1 && instance->getEffectController()==0);
    local.m[12]=100; instance->reserved14(&local); psmType2RefreshTransform(instance);
    CHECK(psmRuntimeField<Mat4>(instance,0x74).m[12]==10);
    psmQueueInactive(&manager,instance); psmRemoveActive(&manager,false);
    CHECK(manager.m_renderBuckets[5].size()==0 && psmRuntimeField<unsigned char>(instance,4)==0);
    // Destructor has its own wrapped-ring free walk; it must not depend on null termination.
    first=trailTake(manager); second=trailTake(manager); ring[15]=first; ring[0]=second;
    psmRuntimeField<unsigned int>(instance,0x178)=15; psmRuntimeField<unsigned int>(instance,0x174)=1;
    instance->destroy(); manager.m_instances.clear();
    CHECK(manager.m_type2FreeList==second && psmRuntimeField<void*>(second,0)==first);

    // Capacity selection uses lifetime or the shorter finite emit time, then a power-of-two ring.
    psmRuntimeField<float>(descriptor,0x70)=100; psmRuntimeField<float>(descriptor,0x168)=1;
    psmRuntimeField<float>(descriptor,0x10)=1;
    instance=psmCreateType2Instance(descriptor); CHECK(instance->reserved34()==16); instance->destroy();
    psmRuntimeField<unsigned char>(descriptor,0x16C)=1;
    instance=psmCreateType2Instance(descriptor); CHECK(instance->reserved34()==128); instance->destroy();
    psmRuntimeField<float>(descriptor,0x70)=2038;
    instance=psmCreateType2Instance(descriptor); CHECK(instance->reserved34()==2048); instance->destroy();
    freeList(manager.m_instances); freeList(manager.m_pendingDestroy); freeList(manager.m_pendingClassification);
    for(int i=0;i<10;++i) freeList(manager.m_renderBuckets[i]);
    cleanup();
    std::printf("Trail particle lifecycle/transform: %d/%d checks passed\n",checks-failures,checks);
    return failures?1:0;
}
