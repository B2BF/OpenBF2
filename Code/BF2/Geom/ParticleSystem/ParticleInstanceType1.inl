// Mesh-particle instance lifecycle and transforms.
// [CHOSEN, not recovered] included split; the original instance class name is [INFERRED].
// Include after the shared/type-0 helpers, before type-1 update and emission helpers.
// [INFERRED] field names describe a 0x224-byte instance and 192-byte pooled particle records.
// The engine-allocated particle pointer array has capacity+1 entries and a null terminator.
// Instances borrow descriptors and external parent transforms. The manager writes LOD coverage;
// the mesh renderer marks recent visibility. Pool free links are reused by the draw sorter.

void psmType1Update(void* self,float dt);
float* psmType1NewParticle(void* self, bool inlinedCopy=false);
void psmType1Emit(void* self,float dt);
int psmType1AdvanceParticle(void* self,float* particle,int index,float step,unsigned int steps);

// PE .data 1023A4E8 and 1023A4C8. The mesh code reads both as memory operands (fmul / mov) and no
// instruction in the image writes them; they stay writable statics rather than folded literals.
static float psmType1FixedStep=0.01f;
static float psmType1DegreesToRadians=0.01745329238474369f;

// Refreshes the instance transform unless detached: owner's visual transform, borrowed
// parent transform, or the local transform when neither is available.
void psmType1RefreshTransform(void* self)
{
    if (psmInstanceB<unsigned char>(self,0x18)) return;
    Mat4& output=psmInstanceB<Mat4>(self,0x74);
    const Mat4& local=psmInstanceB<Mat4>(self,0x34);
    world::IObject* const owner=psmInstanceB<world::IObject*>(self,0x10);
    if (owner) output.mult(local,owner->getVisualAbsoluteTransformation());
    else if (psmInstanceB<const Mat4*>(self,0x30))
        output.mult(local,*psmInstanceB<const Mat4*>(self,0x30));
    else output=local;
}

class ParticleInstanceType1 : public IParticleSystemInstance
{
    unsigned char m_data[PSM_INSTANCE_DATA(0x224)];
    template<class T> T& at(unsigned int offset) { return psmInstanceB<T>(this,offset); }
    template<class T> const T& at(unsigned int offset) const
    { return psmInstanceB<T>(const_cast<ParticleInstanceType1*>(this), offset); }
public:
    // Cells it never writes (+0x06/+0x07, +0xCC/+0xD0, +0xE8..+0xF8, +0x13C/+0x140,
    // +0x220, the history planes' distances before the second pass) stay untouched here too.
    explicit ParticleInstanceType1(IParticleSystemTemplate* descriptor)
    {
        at<unsigned char>(0x18)=0;
        at<Vec3>(0x1C)={0,0,0}; at<unsigned int>(0x28)=0;
        at<IParticleSystemTemplate*>(0x2C)=descriptor; at<const Mat4*>(0x30)=0;
        at<Mat4>(0x34)=Mat4::getIdentity(); at<Mat4>(0x74)=Mat4::getIdentity();
        at<Vec3>(0xB4)={0,0,0}; at<Vec3>(0xC0)={0,0,0}; at<Vec3>(0xD4)={0,0,1};
        at<float>(0xE0)=0; at<float>(0xE4)=0;
        for (unsigned int offset=0xFC;offset<=0x124;offset+=4) at<unsigned int>(offset)=0;
        at<float>(0x128)=1; at<float>(0x12C)=1; at<float>(0x130)=1;
        at<unsigned int>(0x134)=0; at<float>(0x138)=1;
        at<unsigned char>(0x144)=0;
        for (unsigned int offset=0x148;offset<=0x15C;offset+=4) at<float>(offset)=1;
        // the Vec3 parts of the ten history planes and of the current plane
        // are default-constructed first; their distance words are only written further down.
        for (unsigned int offset=0x160;offset<=0x200;offset+=0x10) at<Vec3>(offset)={0,0,0};
        at<unsigned char>(0x21E)=1; at<unsigned char>(0x21F)=1;
        at<unsigned char>(0x21C)=0; at<unsigned char>(0x21D)=0;
        at<void*>(0x10)=0; at<void*>(0x0C)=0; at<unsigned char>(8)=0;
        // Capacity = frequency x (the shorter of the particle lifetime and the
        // emission time, unless the emission time is zero or the system loops) + 10, truncated.
        // NaN routing is retail's: an unordered lifetime is kept, an unordered emit time counts as
        // non-zero.
        const float emitTime=psmDescriptor1<float>(descriptor,0x194);
        double capacityTime=static_cast<double>(psmDescriptor1<float>(descriptor,0x9C))
            +psmDescriptor1<float>(descriptor,0x98);
        if (emitTime!=0 && !psmDescriptor1<unsigned char>(descriptor,0x198) && capacityTime>emitTime)
            capacityTime=emitTime;
        at<int>(0x218)=static_cast<int>(capacityTime*psmDescriptor1<float>(descriptor,0x10)+10.0);
        // no allocation-failure or capacity guard, exactly as shipped.
        at<float**>(0x14)=static_cast<float**>(::renddx9abi::engineAlloc(at<int>(0x218)*kPsmPointerSize+kPsmPointerSize));
        at<float**>(0x14)[0]=0;
        at<Vec3>(0x1C)={0,0,0};
        at<Vec3>(0xFC)={0,0,0}; at<Vec3>(0x108)={0,0,0};
        at<Mat4>(0x34)=Mat4::getIdentity();
        at<unsigned char>(0x210)=0; at<unsigned char>(0x211)=0; at<float>(0x214)=0;
        at<Vec3>(0x200)={0,1,0}; at<float>(0x20C)=0;
        for (unsigned int offset=0x160;offset<=0x1F0;offset+=0x10)
        { at<Vec3>(offset)={0,1,0}; at<float>(offset+12)=0; }
        // fetch the descriptor's geometry instance on first use (thiscall on the
        // descriptor, PE 10179080; the lookup is retried per instance while +0x8C stays null).
        if (!psmDescriptor1<void*>(descriptor,0x8C))
            particleSystemType1ResolveGeometry(descriptor);
        at<unsigned char>(4)=0;
    }
    // PE 10183F20 (+ deleting wrapper 10186130). Unlike type 0 there is no pool counter to
    // decrement, and the null-terminated walk rereads +0x14 for every element.
    ~ParticleInstanceType1() override
    {
        if (at<float**>(0x14)[0])
        {
            unsigned int index=0;
            do
            {
                ParticleSystemManager* const manager=
                    reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
                float* const particle=at<float**>(0x14)[index];
                psmRecordLink(particle, 1)=manager->m_type1FreeList;
                manager->m_type1FreeList=particle;
                ++index;
            } while (at<float**>(0x14)[index]);
        }
        ::renddx9abi::engineFree(at<float**>(0x14));
    }
    static void operator delete(void* self) { ::renddx9abi::engineFree(self); }
    int reserved01() const override { return 1; }
    // Stop, flush the live particles through one full-lifetime virtual update, then
    // hand every survivor back to the pool. Unlike type 2 it does NOT restart afterwards.
    void reserved02() const override
    {
        ParticleInstanceType1* const self=const_cast<ParticleInstanceType1*>(this);
        if (self->at<unsigned char>(0x144)) self->reserved04();
        if (self->at<unsigned int>(0x28))
        {
            void* const descriptor=self->at<void*>(0x2C);
            self->reserved41(static_cast<float>(static_cast<double>(
                psmDescriptor1<float>(descriptor,0x9C))+psmDescriptor1<float>(descriptor,0x98)));
        }
        // No count guard: a non-null head with a zero count underflows the index, as shipped.
        while (self->at<float**>(0x14)[0])
        {
            float** const array=self->at<float**>(0x14);
            float* const particle=array[0];
            const unsigned int last=--self->at<unsigned int>(0x28);
            array[0]=array[last];
            self->at<float**>(0x14)[self->at<unsigned int>(0x28)]=0;
            ParticleSystemManager* const manager=
                reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
            psmRecordLink(particle, 1)=manager->m_type1FreeList;
            manager->m_type1FreeList=particle;
        }
        self->at<float>(0x214)=0;
        self->at<unsigned char>(0x210)=0; self->at<unsigned char>(0x211)=0;
        self->at<unsigned int>(0x134)=0; self->at<unsigned int>(0x28)=0; self->at<float>(0x120)=0;
        self->at<float>(0x12C)=1;
        self->at<Vec3>(0x114)={0,0,0};
        self->at<Vec3>(0xFC)={0,0,0}; self->at<Vec3>(0x108)={0,0,0};
        self->at<float**>(0x14)[0]=0;
        self->at<unsigned char>(0x21D)=0;
        self->at<unsigned char>(0x21E)=1; self->at<unsigned char>(0x21F)=1;
    }
    // Quality refusal, emission timers, frame stamp, transform, first-time
    // classification, then the far-plane-bracketed visibility test (same shape as type 4's
    // 1017D810). The prewarm time is only armed (+0xEC); the frame update consumes it.
    void reserved03(bool firstPerson) override
    {
        // 10184C24 reads the manager's quality word before the descriptor's virtual call.
        const int quality=static_cast<int>(
            reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_qualityLevel);
        if (at<IParticleSystemTemplate*>(0x2C)->getQualityLevel()>quality) return;
        void* const descriptor=at<void*>(0x2C);
        at<unsigned char>(0x144)=1; at<unsigned char>(0x21C)=1;
        at<float>(0xE8)=psmDescriptor1<float>(descriptor,0x194);
        at<float>(0xF0)=psmDescriptor1<float>(descriptor,0x1A4);
        at<float>(0xEC)=psmDescriptor1<float>(descriptor,0x0C);
        at<float>(0xF4)=0; at<float>(0xF8)=0; at<float>(0xD0)=0;
        at<unsigned char>(0x21E)=1; at<unsigned char>(0x21F)=1;
        at<unsigned int>(0x124)=dx9rend::rendererFrameOrdinal();
        psmType1RefreshTransform(this);
        if (!at<unsigned char>(4))
        {
            // an ordered `position.y < water height`; NaN reads as dry.
            at<unsigned char>(0x220)=at<float>(0xA8)<*reinterpret_cast<const float*>(dx9rend::raShaderConstantsAt(0x1BC));
            psmAddActive(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager),this,firstPerson);
        }
        dx9rend::IRenderView* const view=dx9rend::rendererGetRenderViewDup();
        const float oldFar=view->getFarPlane();
        view->setFarPlane(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_effectiveViewDistance);
        at<float>(0x120)=0;
        at<Vec3>(0x114)=at<Vec3>(0xA4); at<Vec3>(0xFC)=at<Vec3>(0xA4); at<Vec3>(0x108)=at<Vec3>(0xA4);
        const float* const matrix=reinterpret_cast<const float*>(view->getViewMatrixPtr());
        const Vec3& centre=at<Vec3>(0x114);
        const float x=centre.x-matrix[12], y=centre.y-matrix[13], z=centre.z-matrix[14];
        const double distance=static_cast<double>(view->getFarPlane())-at<float>(0x120);
        // 10184DC2 tests AH&5 parity: unordered, like greater/equal, takes the visible arm.
        if (!(distance*distance<static_cast<double>(x)*x+static_cast<double>(z)*z+static_cast<double>(y)*y))
        {
            at<unsigned int>(0x124)=dx9rend::rendererFrameOrdinal();
            at<unsigned char>(0x21E)=1; at<unsigned char>(0x21F)=1;
        }
        else { at<unsigned int>(0x124)=0; at<unsigned char>(0x21E)=0; at<unsigned char>(0x21F)=0; }
        view->setFarPlane(oldFar);
    }

    void reserved04() override { at<unsigned char>(0x144)=0; at<unsigned char>(0x21C)=0; at<float>(0xE8)=0; }
    bool reserved05() const override { return psmShared13IsEmitting(this); }          // 101839D0 shared
    void setPaused(bool value) override { psmShared13SetPaused(this,value); }         // 101839E0 shared
    bool reserved07() const override { return psmShared13IsPaused(this); }           // 10177570 shared

    void reserved08() override { at<unsigned char>(0x18)=1; at<void*>(0x10)=0; at<void*>(0xC)=0; at<void*>(0x30)=0; }
    void reserved09(bool value) override { psmShared13SetVisible(this,value); }          // 101839F0 shared
    bool reserved10() const override { return psmShared13IsVisible(this); }             // 10183A00 shared
    void reserved11(intptr_t value) override { at<intptr_t>(0x10)=value; }                         // 1017A500 (all five)
    intptr_t reserved12() const override { return at<intptr_t>(0x10); }                          // 1010DE20 (all five)
    void reserved13(intptr_t value) override { at<intptr_t>(0x30)=value; }                         // 1017C200 (all five)
    void* reserved14(const void* value) override
    { at<Mat4>(0x34)=*static_cast<const Mat4*>(value); return &at<Mat4>(0x34); }
    const void* reserved15() const override { return &at<Mat4>(0x34); }                // 10179DD0 (all five)
    // PE 10184240 stores y, x, z - the order the shared-13 setters keep too.
    void reserved16(const Vec3& value) override
    { const float z=value.z,y=value.y,x=value.x; at<float>(0xD8)=y; at<float>(0xD4)=x; at<float>(0xDC)=z; }
    const Vec3& reserved17() const override { return at<Vec3>(0xD4); }
    // the distance word is read before the normal is normalised in place.
    void reserved18(const void* value) override
    {
        const float* const plane=static_cast<const float*>(value);
        const float distance=plane[3];
        at<Vec3>(0x200)=*static_cast<const Vec3*>(value);
        psmNormalize3(&at<float>(0x200));
        at<float>(0x20C)=distance;
    }
    void reserved19(float value) override { at<float>(0xE4)=value; }
    float reserved20() const override { return at<float>(0xE4); }
    void reserved21(int,int) override { at<float>(0x154)=1; }                            // 10183FF0, retn 8
    void reserved22(float value) override { at<float>(0x158)=value; }
    float reserved23() const override { return at<float>(0x158); }
    void reserved24(float value) override { at<float>(0x130)=value; }
    float reserved25() const override { return at<float>(0x130); }
    void reserved26(float value) override { psmShared13SetSpeedScale(this,value); }   // 10186110 shared
    float reserved27() const override { return psmShared13SpeedScale(this); }     // 10183A10 shared
    void reserved28(const Vec3& value) override { psmShared13SetDirectionScale(this,value); } // 10183A30 shared
    const Vec3& reserved29() const override { return psmShared13DirectionScale(this); }   // 10183A20 shared
    void reserved30(float value) override { at<float>(0x128)=value; }
    float reserved31() const override { return at<float>(0x128); }
    void reserved32(float value) override { at<float>(0xE0)=value; }
    float reserved33() const override { return at<float>(0xE0); }
    int reserved34() const override { return at<int>(0x218); }
    IParticleSystemTemplate* getTemplate() const override { return at<IParticleSystemTemplate*>(0x2C); }
    void reserved36(const Vec3& value) override { psmShared13SetVelocityOffset(this,value); } // 1017C360 shared
    void reserved37() override { psmType1NewParticle(this); }
    float reserved38() const override { return psmShared13BoundsRadius(this); }          // 10183A60 shared
    int reserved39() const override { return at<int>(0x28); }                           // 101C8440 (all five)
    const Vec3& getPosition() const override { return at<Vec3>(0xB4); }                 // 1017C240 (all five)
    void reserved41(float value) override { psmType1Update(this,value); }
    // PE 10183FA0 -> manager 100D3650 (which re-tests byte +4 before inserting).
    void reserved42(bool value) override
    {
        if (at<unsigned char>(4) && value!=static_cast<bool>(at<unsigned char>(6)))
        {
            at<unsigned char>(7)=value;
            psmReclassification(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager))
                .insert(this);
        }
    }
    // Proven retail shared constant/null bodies: 1017A510 / 100E13B0 / 100ED3B0 / 10082D40.
    float getGasCloudRadius() const override { return 1.0f; }
    void setPlayer(void*) override {}
    void* getPlayer() const override { return 0; }
    void setMaterial(int) override {}
    int getMaterial() const override { return -1; }
    // PE 101BB440 / 101BB480, shared by all five tables.
    void setEffectController(world::IEffectController* value) override
    { at<world::IEffectController*>(0xC)=value; reserved11(reinterpret_cast<intptr_t>(value ? value->getObject() : 0)); }
    world::IEffectController* getEffectController() const override { return at<world::IEffectController*>(0xC); }
    bool getWaterSide() const override { return at<unsigned char>(0x220)!=0; }
};
#if defined(_M_IX86)
static_assert(sizeof(ParticleInstanceType1)==0x224,"mesh particle instance PE 10184810 layout");
#else
static_assert(sizeof(ParticleInstanceType1)==kPsmInstanceSize,"x64 instance layout, ParticleRawLayout.h");
#endif

// engine alloc 0x224, null returns null, else construct. The shared tail
// (byte +5 = 0, push into manager +0x10) stays in psm_createInstance.
IParticleSystemInstance* psmCreateType1Instance(IParticleSystemTemplate* descriptor)
{
    void* const storage=::renddx9abi::engineAlloc(sizeof(ParticleInstanceType1));
    return storage ? ::new(storage) ParticleInstanceType1(descriptor) : 0;
}
