// [CHOSEN, not recovered] implementation split of the ordinary SpriteParticleSystem instance.
// PE constructor 1017F870, table1021FE48 (51 entries), lifecycle1017EF30/F100/FC90;
// all bodies read in full. The data bytes retain the renderer's fixed0x23C layout.

void psmType0Update(void* self, float dt);
float* psmType0NewParticle(void* self);
void psmType0Emit(void* self, float dt);
int psmType0AdvanceParticle(void* self, float* particle, int index, float step, unsigned int steps);

void psmNormalize3(float* v)
{
    const float length = static_cast<float>(static_cast<double>(v[0]) * v[0]
        + static_cast<double>(v[1]) * v[1] + static_cast<double>(v[2]) * v[2]);
    // PE100777D0..DB skips only an ordered less-than comparison; NaN still normalizes.
    if (!(::fabs(static_cast<double>(length) - 1.0) < 1.1920929e-7f))
    {
        if (::fabs(static_cast<double>(length)) < 1.1920929e-7f) v[0] = v[1] = v[2] = 0;
        else
        {
            const double inverse = 1.0 / ::sqrt(length);
            v[0] = static_cast<float>(inverse * v[0]);
            v[1] = static_cast<float>(inverse * v[1]);
            v[2] = static_cast<float>(inverse * v[2]);
        }
    }
}

// Local transform is composed with the owner's visual transform, not physics pose.
void psmType0RefreshTransform(void* self)
{
    if (psmInstanceA<unsigned char>(self, 0x14)) return;
    Mat4& output = psmInstanceA<Mat4>(self, 0x74);
    const Mat4& local = psmInstanceA<Mat4>(self, 0x34);
    world::IObject* const owner = psmInstanceA<world::IObject*>(self, 0x10);
    if (owner) output.mult(local, owner->getVisualAbsoluteTransformation());
    else if (psmInstanceA<const Mat4*>(self, 0x30))
        output.mult(local, *psmInstanceA<const Mat4*>(self, 0x30));
    else output = local;
}

class ParticleInstanceType0 : public IParticleSystemInstance
{
    unsigned char m_data[PSM_INSTANCE_DATA(0x23C)];
    template<class T> T& at(unsigned int offset) { return psmInstanceA<T>(this, offset); }
    template<class T> const T& at(unsigned int offset) const
    { return psmInstanceA<T>(const_cast<ParticleInstanceType0*>(this), offset); }
public:
    explicit ParticleInstanceType0(IParticleSystemTemplate* descriptor)
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
        at<float>(0x22C)=0; at<void*>(0x230)=0; at<int>(0x234)=-1;
        at<world::IObject*>(0x10)=0; at<world::IEffectController*>(0xC)=0; at<unsigned char>(8)=0;
        const double lifetime = psmDescriptor0<float>(descriptor,0x84)+static_cast<double>(psmDescriptor0<float>(descriptor,0x80));
        double capacityTime=lifetime;
        const float emitTime=psmDescriptor0<float>(descriptor,0x1D4);
        if (emitTime!=0 && !psmDescriptor0<unsigned char>(descriptor,0x6C) && capacityTime>emitTime)
            capacityTime=emitTime;
        at<int>(0x220)=static_cast<int>(capacityTime*psmDescriptor0<float>(descriptor,0x10)+10.0);
        at<float**>(0x24)=static_cast<float**>(::renddx9abi::engineAlloc(at<int>(0x220)*kPsmPointerSize+kPsmPointerSize));
        at<float**>(0x24)[0]=0;
        at<Vec3>(0x18)={0,0,0};
        at<Vec3>(0xFC)={0,0,0}; at<Vec3>(0x108)={0,0,0};
        at<Mat4>(0x34)=Mat4::getIdentity();
        at<unsigned char>(0x218)=0; at<unsigned char>(0x219)=0; at<float>(0x21C)=0;
        for (unsigned int offset=0x168;offset<=0x208;offset+=0x10)
        { at<Vec3>(offset)={0,1,0}; at<float>(offset+12)=0; }
        at<unsigned char>(4)=0;
        at<float>(0x228)=descriptor->getMinGasCloudRadius();
    }
    ~ParticleInstanceType0() override
    {
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        float** particles=at<float**>(0x24);
        for (unsigned int i=0;particles[i];++i)
        {
            psmRecordLink(particles[i], 0)=manager->m_type0FreeList;
            --manager->m_type0LiveCount;
            manager->m_type0FreeList=particles[i];
        }
        ::renddx9abi::engineFree(particles);
    }
    static void operator delete(void* self) { ::renddx9abi::engineFree(self); }
    int reserved01() const override { return 0; }
    void reserved02() const override
    {
        ParticleInstanceType0* self=const_cast<ParticleInstanceType0*>(this);
        if (self->at<unsigned char>(0x14C)) self->reserved04();
        ParticleSystemManager* manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        while (self->at<float**>(0x24)[0])
        {
            float** array=self->at<float**>(0x24); float* particle=array[0];
            array[0]=array[--self->at<unsigned int>(0x28)]; array[self->at<unsigned int>(0x28)]=0;
            psmRecordLink(particle, 0)=manager->m_type0FreeList;
            manager->m_type0FreeList=particle; --manager->m_type0LiveCount;
        }
        self->at<float>(0x21C)=0; self->at<unsigned char>(0x218)=0; self->at<unsigned char>(0x219)=0;
        self->at<unsigned int>(0x140)=0; self->at<unsigned int>(0x28)=0; self->at<float>(0x120)=0;
        self->at<float>(0x134)=1; self->at<float>(0x138)=1; self->at<Vec3>(0x114)={0,0,0};
        self->at<Vec3>(0xFC)={0,0,0}; self->at<Vec3>(0x108)={0,0,0}; self->at<float**>(0x24)[0]=0;
        self->at<float>(0xF8)=0; self->at<unsigned char>(0x225)=0;
        self->at<unsigned char>(0x128)=1; self->at<unsigned char>(0x129)=1;
    }
    void reserved03(bool firstPerson) override
    {
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        IParticleSystemTemplate* descriptor=at<IParticleSystemTemplate*>(0x2C);
        if (descriptor->getQualityLevel()>static_cast<int>(manager->m_qualityLevel)) return;
        at<unsigned char>(0x226)=0; at<unsigned char>(0x14C)=1; at<unsigned char>(0x224)=1;
        at<float>(0xE0)=psmDescriptor0<float>(descriptor,0x1D4);
        at<float>(0xE4)=psmDescriptor0<float>(descriptor,0x1D8);
        at<float>(0xE8)=0; at<float>(0xF0)=0; at<float>(0xF4)=0;
        at<float>(0xF8)=psmDescriptor0<float>(descriptor,0xC);
        psmType0RefreshTransform(this);
        if (!at<unsigned char>(4))
        {
            at<unsigned char>(0x238)=at<Vec3>(0xA4).y<*reinterpret_cast<const float*>(dx9rend::raShaderConstantsAt(0x1BC));
            psmAddActive(manager,this,firstPerson);
        }
        dx9rend::IRenderView* const view=dx9rend::rendererGetRenderViewDup();
        const float maximumDistance=manager->m_effectiveViewDistance;
        at<float>(0x120)=0;
        at<Vec3>(0x114)=at<Vec3>(0xA4); at<Vec3>(0xFC)=at<Vec3>(0xA4); at<Vec3>(0x108)=at<Vec3>(0xA4);
        const float* matrix=reinterpret_cast<const float*>(view->getViewMatrixPtr());
        const Vec3& center=at<Vec3>(0x114);
        const double x=static_cast<double>(center.x)-matrix[12], y=static_cast<double>(center.y)-matrix[13], z=static_cast<double>(center.z)-matrix[14];
        const double distance=static_cast<double>(maximumDistance)-at<float>(0x120);
        if (distance*distance>=x*x+y*y+z*z)
        {
            at<unsigned int>(0x124)=dx9rend::rendererFrameOrdinal();
            at<unsigned char>(0x128)=1; at<unsigned char>(0x129)=1;
        }
        else { at<unsigned int>(0x124)=0; at<unsigned char>(0x128)=0; at<unsigned char>(0x129)=0; }
    }
    void reserved04() override { at<unsigned char>(0x14C)=0; at<unsigned char>(0x224)=0; at<float>(0xE0)=0; }
    bool reserved05() const override { return at<unsigned char>(0x14C)!=0; }
    void setPaused(bool value) override { at<unsigned char>(0x225)=value; }
    bool reserved07() const override { return at<unsigned char>(0x225)!=0; }
    void reserved08() override { at<unsigned char>(0x14)=1; at<void*>(0xC)=0; at<void*>(0x10)=0; at<void*>(0x30)=0; }
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
    void reserved37() override { psmType0NewParticle(this); }
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
    void reserved41(float value) override { psmType0Update(this,value); }
    void reserved42(bool value) override
    {
        if (at<unsigned char>(4) && value!=static_cast<bool>(at<unsigned char>(6)))
        { at<unsigned char>(7)=value; psmReclassification(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)).insert(this); }
    }
    float getGasCloudRadius() const override { return at<float>(0x228); }
    void setPlayer(void* value) override { at<void*>(0x230)=value; }
    void* getPlayer() const override { return at<void*>(0x230); }
    void setMaterial(int value) override { at<int>(0x234)=value; }
    int getMaterial() const override { return at<int>(0x234); }
    void setEffectController(world::IEffectController* value) override
    { at<world::IEffectController*>(0xC)=value; reserved11(reinterpret_cast<intptr_t>(value ? value->getObject() : 0)); }
    world::IEffectController* getEffectController() const override { return at<world::IEffectController*>(0xC); }
    bool getWaterSide() const override { return at<unsigned char>(0x238)!=0; }
};
#if defined(_M_IX86)
static_assert(sizeof(ParticleInstanceType0)==0x23C,"ordinary particle instance PE1017F870 layout");
#else
static_assert(sizeof(ParticleInstanceType0)==kPsmInstanceSize,"x64 instance layout, ParticleRawLayout.h");
#endif

IParticleSystemInstance* psmCreateType0Instance(IParticleSystemTemplate* descriptor)
{
    void* const storage=::renddx9abi::engineAlloc(sizeof(ParticleInstanceType0));
    return storage ? ::new(storage) ParticleInstanceType0(descriptor) : 0;
}
