// [CHOSEN, not recovered] private split of the non-screen-aligned particle instance.
// Full BF2 RendDX9 PE constructor1017D430, table1021FD48 (51 entries), lifecycleC840/CA10/D810.
// Corresponding renderer implementations are absent from the dedicated ELF.

void psmType4Update(void* self,float dt);
float* psmType4NewParticle(void* self);
void psmType4Emit(void* self,float dt);
int psmType4AdvanceParticle(void* self,float* particle,int index,float step,unsigned int steps);

// PE100D3150 returns shared matrix scratch; the owner immediately copies it. Preserve the
// unusual forward-cross-up order and the parallel-axis identity result, including NaN routing.
const Mat4& psmType4WaterMatrix(const Vec3& position,const Vec3& up,const Vec3& forward)
{
    static Mat4 result;
    result=Mat4::getIdentity();
    const double dot=static_cast<double>(up.x)*forward.x+static_cast<double>(up.z)*forward.z
        +static_cast<double>(up.y)*forward.y;
    if (!(::fabs(dot)>0.99989998f))
    {
        float right[3]={static_cast<float>(static_cast<double>(up.z)*forward.y-static_cast<double>(up.y)*forward.z),
            static_cast<float>(static_cast<double>(up.x)*forward.z-static_cast<double>(up.z)*forward.x),
            static_cast<float>(static_cast<double>(up.y)*forward.x-static_cast<double>(up.x)*forward.y)};
        psmNormalize3(right);
        result.m[0]=right[0]; result.m[1]=right[1]; result.m[2]=right[2];
        result.m[4]=static_cast<float>(static_cast<double>(right[1])*forward.z-static_cast<double>(right[2])*forward.y);
        result.m[5]=static_cast<float>(static_cast<double>(right[2])*forward.x-static_cast<double>(right[0])*forward.z);
        result.m[6]=static_cast<float>(static_cast<double>(right[0])*forward.y-static_cast<double>(right[1])*forward.x);
        result.m[8]=forward.x; result.m[9]=forward.y; result.m[10]=forward.z;
    }
    result.m[12]=position.x; result.m[13]=position.y; result.m[14]=position.z;
    return result;
}

// PE1017CB20 matches the ordinary visual-owner composition, then aligns water effects.
void psmType4RefreshTransform(void* self)
{
    if (psmInstanceA<unsigned char>(self,0x14)) return;
    Mat4& output=psmInstanceA<Mat4>(self,0x74);
    const Mat4& local=psmInstanceA<Mat4>(self,0x34);
    world::IObject* const owner=psmInstanceA<world::IObject*>(self,0x10);
    if (owner) output.mult(local,owner->getVisualAbsoluteTransformation());
    else if (psmInstanceA<const Mat4*>(self,0x30)) output.mult(local,*psmInstanceA<const Mat4*>(self,0x30));
    else output=local;
    if (psmDescriptor4<unsigned char>(psmInstanceA<void*>(self,0x2C),0xA0))
        output=psmType4WaterMatrix(psmInstanceA<Vec3>(self,0xA4),Vec3{0,1,0},psmInstanceA<Vec3>(self,0x94));
}

class ParticleInstanceType4 : public IParticleSystemInstance
{
    unsigned char m_data[PSM_INSTANCE_DATA(0x228)];
    template<class T> T& at(unsigned int offset) { return psmInstanceA<T>(this, offset); }
    template<class T> const T& at(unsigned int offset) const
    { return psmInstanceA<T>(const_cast<ParticleInstanceType4*>(this), offset); }
public:
    explicit ParticleInstanceType4(IParticleSystemTemplate* descriptor)
    {
        at<unsigned char>(0x14)=0;
        at<Vec3>(0x18)={0,0,0}; at<unsigned int>(0x28)=0;
        at<IParticleSystemTemplate*>(0x2C)=descriptor; at<const Mat4*>(0x30)=0;
        at<Mat4>(0x34)=Mat4::getIdentity(); at<Mat4>(0x74)=Mat4::getIdentity();
        at<Vec3>(0xB4)={0,0,0}; at<Vec3>(0xC0)={0,0,0}; at<Vec3>(0xCC)={0,0,1};
        at<float>(0xD8)=0; at<float>(0xDC)=0;
        for (unsigned int offset=0xF8;offset<=0x124;offset+=4) at<unsigned int>(offset)=0;
        at<float>(0x12C)=1; at<float>(0x134)=1; at<float>(0x138)=1; at<float>(0x13C)=1;
        at<unsigned int>(0x140)=0; at<unsigned char>(0x14C)=0;
        at<unsigned char>(0x128)=1; at<unsigned char>(0x129)=1;
        for (unsigned int offset=0x150;offset<=0x164;offset+=4) at<float>(offset)=1;
        for (unsigned int offset=0x168;offset<=0x208;offset+=0x10) at<Vec3>(offset)={0,0,0};
        at<unsigned char>(0x224)=0; at<unsigned char>(0x225)=0;
        // PE1017D430 leaves owner+10/controller+C untouched; EffectBundle binds these next.
        at<unsigned char>(8)=0;
        const double lifetime = psmDescriptor4<float>(descriptor,0x7C)+static_cast<double>(psmDescriptor4<float>(descriptor,0x78));
        double capacityTime=lifetime;
        const float emitTime=psmDescriptor4<float>(descriptor,0x1C4);
        if (emitTime!=0 && !psmDescriptor4<unsigned char>(descriptor,0x74) && capacityTime>emitTime)
            capacityTime=emitTime;
        at<int>(0x220)=static_cast<int>(capacityTime*psmDescriptor4<float>(descriptor,0x10)+10.0);
        at<float**>(0x24)=static_cast<float**>(::renddx9abi::engineAlloc(at<int>(0x220)*kPsmPointerSize+kPsmPointerSize));
        at<float**>(0x24)[0]=0;
        at<Vec3>(0x18)={0,0,0};
        at<Vec3>(0xFC)={0,0,0}; at<Vec3>(0x108)={0,0,0};
        at<Mat4>(0x34)=Mat4::getIdentity();
        at<unsigned char>(0x218)=0; at<unsigned char>(0x219)=0; at<float>(0x21C)=0;
        for (unsigned int offset=0x168;offset<=0x208;offset+=0x10)
        { at<Vec3>(offset)={0,1,0}; at<float>(offset+12)=0; }
        at<unsigned char>(4)=0;
    }
    ~ParticleInstanceType4() override
    {
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        float** particles=at<float**>(0x24);
        for (unsigned int i=0;particles[i];++i)
        {
            psmRecordLink(particles[i], 4)=manager->m_type4FreeList;
            --manager->m_type4LiveCount;
            manager->m_type4FreeList=particles[i];
        }
        ::renddx9abi::engineFree(particles);
    }
    static void operator delete(void* self) { ::renddx9abi::engineFree(self); }
    int reserved01() const override { return 4; }
    void reserved02() const override
    {
        ParticleInstanceType4* self=const_cast<ParticleInstanceType4*>(this);
        if (self->at<unsigned char>(0x14C)) self->reserved04();
        ParticleSystemManager* manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        while (self->at<float**>(0x24)[0])
        {
            float** array=self->at<float**>(0x24); float* particle=array[0];
            array[0]=array[--self->at<unsigned int>(0x28)]; array[self->at<unsigned int>(0x28)]=0;
            psmRecordLink(particle, 4)=manager->m_type4FreeList;
            manager->m_type4FreeList=particle; --manager->m_type4LiveCount;
        }
        self->at<float>(0x21C)=0; self->at<unsigned char>(0x218)=0; self->at<unsigned char>(0x219)=0;
        self->at<unsigned int>(0x140)=0; self->at<unsigned int>(0x28)=0; self->at<float>(0x120)=0;
        self->at<float>(0x134)=1; self->at<float>(0x138)=1; self->at<Vec3>(0x114)={0,0,0};
        self->at<Vec3>(0xFC)={0,0,0}; self->at<Vec3>(0x108)={0,0,0}; self->at<float**>(0x24)[0]=0;
        self->at<float>(0xF8)=0; self->at<unsigned char>(0x225)=0;
        self->at<unsigned char>(0x128)=1; self->at<unsigned char>(0x129)=1;
    }
    // PE1017D810: classification precedes resets; save/set/read/restore view far plane.
    void reserved03(bool firstPerson) override
    {
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        if (at<IParticleSystemTemplate*>(0x2C)->getQualityLevel()>static_cast<int>(manager->m_qualityLevel)) return;
        if (!at<unsigned char>(4)) psmAddActive(manager,this,firstPerson);
        at<unsigned char>(0x14C)=1; at<unsigned char>(0x224)=1; at<unsigned char>(0x226)=0;
        void* const descriptor=at<void*>(0x2C);
        at<float>(0xE0)=psmDescriptor4<float>(descriptor,0x1C4);
        at<float>(0xE4)=psmDescriptor4<float>(descriptor,0x1CC);
        at<float>(0xE8)=0; at<float>(0xF0)=0; at<float>(0xF4)=0;
        at<float>(0xF8)=psmDescriptor4<float>(descriptor,0xC);
        psmType4RefreshTransform(this);
        dx9rend::IRenderView* const view=dx9rend::rendererGetRenderViewDup();
        const float oldFar=view->getFarPlane();
        view->setFarPlane(manager->m_effectiveViewDistance);
        at<float>(0x120)=0;
        at<Vec3>(0x114)=at<Vec3>(0xA4); at<Vec3>(0xFC)=at<Vec3>(0xA4); at<Vec3>(0x108)=at<Vec3>(0xA4);
        const float* matrix=reinterpret_cast<const float*>(view->getViewMatrixPtr());
        const Vec3& center=at<Vec3>(0x114);
        const float x=center.x-matrix[12], y=center.y-matrix[13], z=center.z-matrix[14];
        const double distance=static_cast<double>(view->getFarPlane())-at<float>(0x120);
        // PE1017D99A tests AH&5 parity: unordered, like greater/equal, takes the visible arm.
        if (!(distance*distance<static_cast<double>(x)*x+static_cast<double>(z)*z+static_cast<double>(y)*y))
        {
            at<unsigned int>(0x124)=dx9rend::rendererFrameOrdinal();
            at<unsigned char>(0x128)=1; at<unsigned char>(0x129)=1;
        }
        else { at<unsigned int>(0x124)=0; at<unsigned char>(0x128)=0; at<unsigned char>(0x129)=0; }
        view->setFarPlane(oldFar);
    }
    void reserved04() override { at<unsigned char>(0x14C)=0; at<unsigned char>(0x224)=0; at<float>(0xE0)=0; }
    bool reserved05() const override { return at<unsigned char>(0x14C)!=0; }
    void setPaused(bool value) override { at<unsigned char>(0x225)=value; }
    bool reserved07() const override { return at<unsigned char>(0x225)!=0; }
    void reserved08() override { at<unsigned char>(0x14)=1; at<void*>(0x10)=0; at<void*>(0xC)=0; at<void*>(0x30)=0; }
    void reserved09(bool value) override { at<unsigned char>(0x128)=value; }
    bool reserved10() const override { return at<unsigned char>(0x128)!=0; }
    void reserved11(intptr_t value) override { at<intptr_t>(0x10)=value; }
    intptr_t reserved12() const override { return at<intptr_t>(0x10); }
    void reserved13(intptr_t value) override { at<intptr_t>(0x30)=value; }
    void* reserved14(const void* value) override { at<Mat4>(0x34)=*static_cast<const Mat4*>(value); return &at<Mat4>(0x34); }
    const void* reserved15() const override { return &at<Mat4>(0x34); }
    void reserved16(const Vec3& value) override { at<Vec3>(0xCC)=value; }
    const Vec3& reserved17() const override { return at<Vec3>(0xCC); }
    void reserved18(const void* value) override
    { at<Vec3>(0x208)=*static_cast<const Vec3*>(value); psmNormalize3(&at<float>(0x208)); at<float>(0x214)=static_cast<const float*>(value)[3]; }
    void reserved19(float value) override { at<float>(0xDC)=value; }
    float reserved20() const override { return at<float>(0xDC); }
    void reserved21(int,int) override { at<float>(0x15C)=1; }
    void reserved22(float value) override { at<float>(0x160)=value; }
    float reserved23() const override { return at<float>(0x160); }
    void reserved24(float value) override { at<float>(0x138)=value; }
    float reserved25() const override { return at<float>(0x138); }
    void reserved26(float value) override { at<float>(0x13C)=value; }
    float reserved27() const override { return at<float>(0x13C); }
    void reserved28(const Vec3& value) override { at<Vec3>(0x150)=value; }
    const Vec3& reserved29() const override { return at<Vec3>(0x150); }
    void reserved30(float value) override { at<float>(0x12C)=value; }
    float reserved31() const override { return at<float>(0x12C); }
    void reserved32(float value) override { at<float>(0xD8)=value; }
    float reserved33() const override { return at<float>(0xD8); }
    int reserved34() const override { return at<int>(0x220); }
    IParticleSystemTemplate* getTemplate() const override { return at<IParticleSystemTemplate*>(0x2C); }
    void reserved36(const Vec3& value) override { at<Vec3>(0x18)=value; }
    void reserved37() override { psmType4NewParticle(this); }
    float reserved38() const override
    {
        const Vec3& low=at<Vec3>(0xFC), &high=at<Vec3>(0x108), &position=at<Vec3>(0xA4);
        const float x=static_cast<float>((static_cast<double>(low.x)+high.x)*0.5)-position.x;
        const float y=static_cast<float>((static_cast<double>(low.y)+high.y)*0.5)-position.y;
        const float zsum=low.z+high.z;
        const double z=zsum*0.5-position.z;
        const float dx=static_cast<float>((static_cast<double>(high.x)-low.x)*0.5), dz=high.z-low.z;
        const double dy=(static_cast<double>(high.y)-low.y)*0.5;
        return static_cast<float>(::sqrt(dz*0.5*(dz*0.5)+dy*dy+static_cast<double>(dx)*dx)
            +::sqrt(z*z+static_cast<double>(y)*y+static_cast<double>(x)*x));
    }
    int reserved39() const override { return at<int>(0x28); }
    const Vec3& getPosition() const override { return at<Vec3>(0xB4); }
    void reserved41(float value) override { psmType4Update(this,value); }
    void reserved42(bool value) override
    {
        if (at<unsigned char>(4) && value!=static_cast<bool>(at<unsigned char>(6)))
        { at<unsigned char>(7)=value; psmReclassification(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)).insert(this); }
    }
    // Proven retail shared constant/null bodies: 1017A510/100E13B0/100ED3B0/10082D40.
    float getGasCloudRadius() const override { return 1.0f; }
    void setPlayer(void*) override {}
    void* getPlayer() const override { return 0; }
    void setMaterial(int) override {}
    int getMaterial() const override { return -1; }
    void setEffectController(world::IEffectController* value) override
    { at<world::IEffectController*>(0xC)=value; reserved11(reinterpret_cast<intptr_t>(value ? value->getObject() : 0)); }
    world::IEffectController* getEffectController() const override { return at<world::IEffectController*>(0xC); }
    bool getWaterSide() const override { return false; }
};
#if defined(_M_IX86)
static_assert(sizeof(ParticleInstanceType4)==0x228,"NSA particle instance PE1017D430 layout");
#else
static_assert(sizeof(ParticleInstanceType4)==kPsmInstanceSize,"x64 instance layout, ParticleRawLayout.h");
#endif

IParticleSystemInstance* psmCreateType4Instance(IParticleSystemTemplate* descriptor)
{
    void* const storage=::renddx9abi::engineAlloc(sizeof(ParticleInstanceType4));
    return storage ? ::new(storage) ParticleInstanceType4(descriptor) : 0;
}
