// [CHOSEN, not recovered] private split of attested Geom/ParticleSystem/TrailSystemInstance.cpp.
// BF2 RendDX9 PE ctor1017B5D0, table1021FBC0 (all51slots), lifecycleA520/A780/ADD0 and
// transformA8D0 read in full. The dedicated ELF has no corresponding renderer implementation.

void psmType2Update(void* self,float dt);
float* psmType2NewParticle(void* self);
void psmType2Emit(void* self,float dt);
int psmType2AdvanceParticle(void* self,float* particle,int index,float step,unsigned int steps);

void psmType2RefreshTransform(void* self)
{
    if (psmInstanceB<unsigned char>(self,0x18)) return;
    Mat4& output=psmInstanceB<Mat4>(self,0x74);
    const Mat4& local=psmInstanceB<Mat4>(self,0x34);
    world::IObject* const owner=psmInstanceB<world::IObject*>(self,0x10);
    if (owner) output.mult(local,owner->getVisualAbsoluteTransformation());
    else if (psmInstanceB<const Mat4*>(self,0x30)) output.mult(local,*psmInstanceB<const Mat4*>(self,0x30));
    else output=local;
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    void* const descriptor=psmInstanceB<void*>(self,0x2C);
    if (psmDescriptor2<unsigned char>(descriptor,0x8C) && manager->m_heightmapCluster)
    {
        output=psmType4WaterMatrix(psmInstanceB<Vec3>(self,0xA4),Vec3{0,1,0},psmInstanceB<Vec3>(self,0x94));
        psmInstanceB<float>(self,0xA8)=static_cast<world::IHeightmapCluster*>(manager->m_heightmapCluster)
            ->getWaterLevelInWorldCoords(psmInstanceB<Vec3>(self,0xA4))+psmDescriptor2<float>(descriptor,0x90);
    }
}

class ParticleInstanceType2 : public IParticleSystemInstance
{
    unsigned char m_data[PSM_INSTANCE_DATA(0x19C)];
    template<class T> T& at(unsigned int offset) { return psmInstanceB<T>(this,offset); }
    template<class T> const T& at(unsigned int offset) const
    { return psmInstanceB<T>(const_cast<ParticleInstanceType2*>(this), offset); }
public:
    explicit ParticleInstanceType2(IParticleSystemTemplate* descriptor)
    {
        at<unsigned char>(0x18)=0; at<Vec3>(0x1C)={0,0,0}; at<unsigned int>(0x28)=0;
        at<IParticleSystemTemplate*>(0x2C)=descriptor; at<const Mat4*>(0x30)=0;
        at<Mat4>(0x34)=Mat4::getIdentity(); at<Mat4>(0x74)=Mat4::getIdentity();
        at<Vec3>(0xB4)={0,0,0}; at<Vec3>(0xC0)={0,0,0}; at<Vec3>(0xD4)={0,0,1};
        at<float>(0xE0)=0; at<float>(0xE4)=0;
        for (unsigned int offset=0xF8;offset<=0x120;offset+=4) at<unsigned int>(offset)=0;
        at<float>(0x124)=1; at<float>(0x128)=1; at<float>(0x12C)=1; at<float>(0x134)=1;
        at<unsigned int>(0x130)=0; at<unsigned char>(0x140)=0;
        at<float>(0x144)=1; at<float>(0x148)=1; at<float>(0x14C)=1;
        at<unsigned char>(0x154)=0; at<unsigned char>(0x155)=0;
        at<unsigned char>(0x156)=1; at<unsigned char>(0x157)=1;
        at<Vec3>(0x15C)={0,0,0}; at<Vec3>(0x168)={1,1,1};
        at<void*>(0x10)=0; at<void*>(0xC)=0; at<unsigned char>(8)=0;
        const float emitTime=psmDescriptor2<float>(descriptor,0x168);
        const float lifeTime=psmDescriptor2<float>(descriptor,0x70);
        double capacityTime=lifeTime;
        if (emitTime!=0 && !psmDescriptor2<unsigned char>(descriptor,0x16C))
            capacityTime=emitTime<lifeTime ? emitTime : lifeTime;
        const unsigned int requested=static_cast<unsigned int>(static_cast<int>(
            capacityTime*psmDescriptor2<float>(descriptor,0x10)+10.0));
        static const unsigned int capacities[]={2,4,8,16,32,64,128,256,512,1024,2048,0};
        unsigned int index=0;
        while (capacities[index] && requested>capacities[index]) ++index;
        if (!capacities[index])
        {
            // Retail reports Debug type6, TrailSystemInstance.cpp:85. That dispatcher remains
            // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: particle-trail-capacity-debug].
            std::fprintf(stderr,"[FRONTIER] dice::hfe::geom::TrailSystemInstance::TrailSystemInstance "
                "(Debug type 6, particle capacity %u exceeds 2048) reached\n",requested);
            std::fflush(stderr); std::abort();
        }
        at<unsigned int>(0x150)=capacities[index];
        at<unsigned int>(0x174)=0; at<unsigned int>(0x178)=0; at<unsigned int>(0x17C)=capacities[index]-1;
        at<float**>(0x14)=static_cast<float**>(::renddx9abi::engineAlloc(capacities[index]*kPsmPointerSize+kPsmPointerSize));
        ::memset(at<float**>(0x14),0,capacities[index]*kPsmPointerSize+kPsmPointerSize);
        at<Vec3>(0x1C)={0,0,0}; at<Vec3>(0xF8)={0,0,0}; at<Vec3>(0x104)={0,0,0};
        at<Mat4>(0x34)=Mat4::getIdentity(); at<unsigned char>(4)=0;
    }
    ~ParticleInstanceType2() override
    {
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        while (at<unsigned int>(0x178)!=at<unsigned int>(0x174))
        {
            float* const particle=at<float**>(0x14)[at<unsigned int>(0x178)];
            psmRecordLink(particle, 2)=manager->m_type2FreeList;
            manager->m_type2FreeList=particle;
            at<unsigned int>(0x178)=(at<unsigned int>(0x178)+1)&at<unsigned int>(0x17C);
        }
        ::renddx9abi::engineFree(at<float**>(0x14));
    }
    static void operator delete(void* self) { ::renddx9abi::engineFree(self); }
    int reserved01() const override { return 2; }
    void reserved02() const override
    {
        ParticleInstanceType2* const self=const_cast<ParticleInstanceType2*>(this);
        const bool wasPlaying=self->at<unsigned char>(0x140)!=0;
        if (wasPlaying) self->reserved04();
        if (self->at<unsigned int>(0x28)) self->reserved41(psmInstanceB<float>(self->at<void*>(0x2C),0x70));
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        while (self->at<unsigned int>(0x178)!=self->at<unsigned int>(0x174))
        {
            float** const array=self->at<float**>(0x14);
            float* const particle=array[self->at<unsigned int>(0x178)];
            array[self->at<unsigned int>(0x178)]=0; --self->at<unsigned int>(0x28);
            const unsigned int next=(self->at<unsigned int>(0x178)+1)&self->at<unsigned int>(0x17C);
            self->at<unsigned int>(0x178)=next;
            if (array[next]) psmRuntimeField<unsigned int>(array[next],0x88)=0;
            psmRecordLink(particle, 2)=manager->m_type2FreeList; manager->m_type2FreeList=particle;
        }
        self->at<unsigned int>(0x174)=0; self->at<unsigned int>(0x178)=0;
        self->at<float>(0x128)=1; self->at<unsigned int>(0x130)=0; self->at<unsigned int>(0x28)=0;
        self->at<float>(0x11C)=0; self->at<Vec3>(0x110)={0,0,0};
        self->at<Vec3>(0xF8)={0,0,0}; self->at<Vec3>(0x104)={0,0,0}; self->at<float**>(0x14)[0]=0;
        self->at<unsigned char>(0x156)=1; self->at<unsigned char>(0x157)=1; self->at<unsigned char>(0x155)=0;
        if (wasPlaying) self->reserved03(false);
    }
    void reserved03(bool firstPerson) override
    {
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        IParticleSystemTemplate* const descriptor=at<IParticleSystemTemplate*>(0x2C);
        if (descriptor->getQualityLevel()>static_cast<int>(manager->m_qualityLevel)) return;
        at<unsigned char>(0x140)=1; at<unsigned char>(0x154)=1;
        at<float>(0xE8)=psmDescriptor2<float>(descriptor,0x168); at<float>(0xEC)=psmDescriptor2<float>(descriptor,0x178);
        at<float>(0xF0)=0; at<float>(0xF4)=0; at<float>(0xD0)=0;
        at<unsigned char>(0x156)=1; at<unsigned char>(0x157)=1;
        at<unsigned int>(0x120)=dx9rend::rendererFrameOrdinal();
        psmType2RefreshTransform(this);
        if (!at<unsigned char>(4))
        {
            at<unsigned char>(0x198)=at<float>(0xA8)<*reinterpret_cast<const float*>(dx9rend::raShaderConstantsAt(0x1BC));
            psmAddActive(manager,this,firstPerson);
        }
        at<Vec3>(0x15C)=at<Vec3>(0xA4); at<float>(0x158)=0;
        const int random=psmEmissionRandomInteger();
        const unsigned int frames=psmDescriptor2<unsigned int>(descriptor,0x1D4)-1;
        at<unsigned int>(0x18C)=0; at<int>(0x188)=-1;
        at<float>(0x190)=static_cast<float>(static_cast<double>(random)*frames*0.000000059604644775390625);
        dx9rend::IRenderView* const view=dx9rend::rendererGetRenderViewDup();
        const float oldFar=view->getFarPlane(); view->setFarPlane(manager->m_effectiveViewDistance);
        at<float>(0x11C)=0; at<Vec3>(0x110)=at<Vec3>(0xA4); at<Vec3>(0xF8)=at<Vec3>(0xA4); at<Vec3>(0x104)=at<Vec3>(0xA4);
        const float* const matrix=view->getViewMatrixPtr()->m;
        const Vec3& center=at<Vec3>(0x110);
        const float x=center.x-matrix[12],y=center.y-matrix[13],z=center.z-matrix[14];
        const double distance=static_cast<double>(view->getFarPlane())-at<float>(0x11C);
        // AH&5 parity at1017AFF7 takes the visible arm for unordered comparisons too.
        if (!(distance*distance<static_cast<double>(x)*x+static_cast<double>(z)*z+static_cast<double>(y)*y))
        { at<unsigned int>(0x120)=dx9rend::rendererFrameOrdinal(); at<unsigned char>(0x156)=1; at<unsigned char>(0x157)=1; }
        else { at<unsigned int>(0x120)=0; at<unsigned char>(0x156)=0; at<unsigned char>(0x157)=0; }
        view->setFarPlane(oldFar);
        float remaining=psmDescriptor2<float>(descriptor,0xC);
        if (remaining>0)
            for (;;)
            {
                reserved41(0.016666999f);
                const double next=static_cast<double>(remaining)-0.016666999f;
                remaining=static_cast<float>(next); // FST keeps the unrounded result for the test.
                if (!(next>0)) break;
            }
    }
    void reserved04() override
    {
        if (at<unsigned int>(0x28)) psmRuntimeField<unsigned int>(at<float**>(0x14)[at<unsigned int>(0x194)],0x88)=0;
        at<unsigned char>(0x140)=0; at<unsigned char>(0x154)=0; at<float>(0xE8)=0;
    }
    bool reserved05() const override { return at<unsigned char>(0x140)!=0; }
    void setPaused(bool value) override { at<unsigned char>(0x155)=value; }
    bool reserved07() const override { return at<unsigned char>(0x155)!=0; }
    void reserved08() override { at<unsigned char>(0x18)=1; at<void*>(0x10)=0; at<void*>(0xC)=0; at<void*>(0x30)=0; }
    void reserved09(bool value) override { at<unsigned char>(0x156)=value; }
    bool reserved10() const override { return at<unsigned char>(0x156)!=0; }
    void reserved11(intptr_t value) override { at<intptr_t>(0x10)=value; }
    intptr_t reserved12() const override { return at<intptr_t>(0x10); }
    void reserved13(intptr_t value) override { at<intptr_t>(0x30)=value; }
    void* reserved14(const void* value) override { at<Mat4>(0x34)=*static_cast<const Mat4*>(value); return &at<Mat4>(0x34); }
    const void* reserved15() const override { return &at<Mat4>(0x34); }
    void reserved16(const Vec3& value) override { at<Vec3>(0xD4)=value; }
    const Vec3& reserved17() const override { return at<Vec3>(0xD4); }
    void reserved18(const void*) override {} // PE1017A6A0 genuine collision-plane null body
    void reserved19(float value) override { at<float>(0xE4)=value; }
    float reserved20() const override { return at<float>(0xE4); }
    void reserved21(int,int) override { at<float>(0x144)=1; }
    void reserved22(float value) override { at<float>(0x148)=value; }
    float reserved23() const override { return at<float>(0x148); }
    void reserved24(float value) override { at<float>(0x12C)=value; }
    float reserved25() const override { return at<float>(0x12C); }
    void reserved26(float value) override { at<float>(0x134)=value; }
    float reserved27() const override { return at<float>(0x134); }
    void reserved28(const Vec3& value) override { at<Vec3>(0x168)=value; }
    const Vec3& reserved29() const override { return at<Vec3>(0x168); }
    void reserved30(float value) override { at<float>(0x124)=value; }
    float reserved31() const override { return at<float>(0x124); }
    void reserved32(float value) override { at<float>(0xE0)=value; }
    float reserved33() const override { return at<float>(0xE0); }
    int reserved34() const override { return at<int>(0x150); }
    IParticleSystemTemplate* getTemplate() const override { return at<IParticleSystemTemplate*>(0x2C); }
    void reserved36(const Vec3& value) override { at<Vec3>(0x1C)=value; }
    void reserved37() override { psmType2NewParticle(this); }
    float reserved38() const override
    {
        const Vec3& low=at<Vec3>(0xF8),&high=at<Vec3>(0x104),&position=at<Vec3>(0xA4);
        const float x=static_cast<float>((static_cast<double>(low.x)+high.x)*0.5)-position.x;
        const float y=static_cast<float>((static_cast<double>(low.y)+high.y)*0.5)-position.y;
        const float sumZ=low.z+high.z; const double z=sumZ*0.5-position.z;
        const float dx=static_cast<float>((static_cast<double>(high.x)-low.x)*0.5),dz=high.z-low.z;
        const double dy=(static_cast<double>(high.y)-low.y)*0.5;
        return static_cast<float>(::sqrt(dz*0.5*(dz*0.5)+dy*dy+static_cast<double>(dx)*dx)
            +::sqrt(z*z+static_cast<double>(y)*y+static_cast<double>(x)*x));
    }
    int reserved39() const override { return at<int>(0x28); }
    const Vec3& getPosition() const override { return at<Vec3>(0xB4); }
    void reserved41(float value) override { psmType2Update(this,value); }
    void reserved42(bool value) override
    {
        if (at<unsigned char>(4) && value!=static_cast<bool>(at<unsigned char>(6)))
        { at<unsigned char>(7)=value; psmReclassification(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)).insert(this); }
    }
    float getGasCloudRadius() const override { return 1; }
    void setPlayer(void*) override {} // PE100E13B0 genuine null body
    void* getPlayer() const override { return 0; }
    void setMaterial(int) override {} // PE100E13B0 genuine null body
    int getMaterial() const override { return -1; }
    void setEffectController(world::IEffectController* value) override
    { at<world::IEffectController*>(0xC)=value; reserved11(reinterpret_cast<intptr_t>(value ? value->getObject() : 0)); }
    world::IEffectController* getEffectController() const override { return at<world::IEffectController*>(0xC); }
    bool getWaterSide() const override { return at<unsigned char>(0x198)!=0; }
};
#if defined(_M_IX86)
static_assert(sizeof(ParticleInstanceType2)==0x19C,"TrailSystemInstance PE1017B5D0 layout");
#else
static_assert(sizeof(ParticleInstanceType2)==kPsmInstanceSize,"x64 instance layout, ParticleRawLayout.h");
#endif

IParticleSystemInstance* psmCreateType2Instance(IParticleSystemTemplate* descriptor)
{
    void* const storage=::renddx9abi::engineAlloc(sizeof(ParticleInstanceType2));
    return storage ? ::new(storage) ParticleInstanceType2(descriptor) : 0;
}
