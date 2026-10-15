// Emitter particles own child systems bound to each particle's transform.
// Children bypass manager updates while attached and advance with their parent particle.
// Expiration returns them to manager updates so their remaining particles can fade.

void psmType3Update(void* self, float dt);
void psmType3NewParticle(void* self);
void psmType3RefreshTransform(void* self);

// ParticleSystemManager's private find-by-name over its +0xC0 template tree, taking the
// std::string BY value (`retn 1Ch`). Defined in ParticleInstanceType3Registry.inl, which must be
// included after g_templatesByName (the tree's [CHOSEN] representation of that registry).
IParticleSystemTemplate* psmFindTemplateByName(const char* name);

// Child-template names use the host's string ABI.
typedef ::renddx9abi::HostString PsmType3ChildName;

// PE 10186540, 65 B - a non-virtual method of the type-3 descriptor (thiscall on the descriptor);
// the instance constructor is its only caller. It is slot 89's (PE 101868F0) tail without the
// assignment: clear the cached child, then re-resolve the stored name when it is non-empty.
// The retail test `cmp dword ptr [esi+80h], 0` is the VC7.1 _Mysize of the +0x6C string.
void psmType3ResolveChildTemplate(void* descriptor)
{
    if (psmDescriptor3<IParticleSystemTemplate*>(descriptor, 0x88))
        psmDescriptor3<IParticleSystemTemplate*>(descriptor, 0x88) = 0;
    const PsmType3ChildName& name = psmDescriptor3<PsmType3ChildName>(descriptor, 0x6C);
    if (!name.empty())
        psmDescriptor3<IParticleSystemTemplate*>(descriptor, 0x88) = psmFindTemplateByName(name.c_str());
}

class ParticleInstanceType3 : public IParticleSystemInstance
{
    unsigned char m_data[PSM_INSTANCE_DATA(0x220)];
    template<class T> T& at(unsigned int offset) { return psmInstanceB<T>(this, offset); }
    template<class T> const T& at(unsigned int offset) const
    { return psmInstanceB<T>(const_cast<ParticleInstanceType3*>(this), offset); }
public:
    // PE 10182110, 1019 B. Every store below is in the shipped order; bytes +5/+6/+7 are not written
    // here (the factory tail writes +5, psmAddActive writes +6/+7) and neither are +0xCC, +0xD0,
    // +0xE8..+0xF8, +0x13C, +0x140: start/update write those before any read.
    explicit ParticleInstanceType3(IParticleSystemTemplate* descriptor)
    {
        at<unsigned char>(0x18)=0;                                   // detached
        at<Vec3>(0x1C)={0,0,0};                                      // velocity offset (+0x90)
        at<unsigned int>(0x28)=0;                                    // live particles
        at<IParticleSystemTemplate*>(0x2C)=descriptor;
        at<const Mat4*>(0x30)=0;                                     // parent matrix (+0x34 slot)
        at<Mat4>(0x34)=Mat4::getIdentity();                          // local, copy-ctor 10030DD0
        at<Mat4>(0x74)=Mat4::getIdentity();                          // world; translation = +0xA4
        at<Vec3>(0xB4)={0,0,0}; at<Vec3>(0xC0)={0,0,0};
        at<float>(0xD4)=0; at<float>(0xD8)=0; at<float>(0xDC)=1;     // inherited direction (+0x40)
        at<float>(0xE0)=0; at<float>(0xE4)=0;
        for (unsigned int offset=0xFC;offset<=0x124;offset+=4) at<unsigned int>(offset)=0;
        at<float>(0x128)=1; at<float>(0x12C)=1; at<float>(0x130)=1;
        at<unsigned int>(0x134)=0; at<float>(0x138)=1;
        at<unsigned char>(0x144)=0;
        for (unsigned int offset=0x148;offset<=0x15C;offset+=4) at<float>(offset)=1;
        for (unsigned int offset=0x160;offset<=0x200;offset+=0x10) at<Vec3>(offset)={0,0,0};
        at<unsigned char>(0x21E)=1; at<unsigned char>(0x21F)=1;
        at<unsigned char>(0x21C)=0; at<unsigned char>(0x21D)=0;
        at<void*>(0x10)=0; at<void*>(0x0C)=0;

        // fucompp against 0: an exactly-zero emit time takes the lifetime-only
        // arm, NaN takes the emit-time arm (and then keeps the lifetime, `fcom` being unordered).
        // Both arms truncate (RC=11) (capacityTime * emitFrequency + 10).
        const float emitTime=psmDescriptor3<float>(descriptor,0x144);
        double capacityTime=static_cast<double>(psmDescriptor3<float>(descriptor,0x90))
            +psmDescriptor3<float>(descriptor,0x8C);
        if (emitTime!=0 && !psmDescriptor3<unsigned char>(descriptor,0x148) && capacityTime>emitTime)
            capacityTime=emitTime;
        at<int>(0x218)=static_cast<int>(capacityTime*psmDescriptor3<float>(descriptor,0x0C)+10.0);
        at<unsigned char>(8)=0;
        // No allocation-null guard: the shipped body stores through the answer at 10182393.
        at<unsigned char**>(0x14)=static_cast<unsigned char**>(
            ::renddx9abi::engineAlloc(at<unsigned int>(0x218)*kPsmPointerSize+kPsmPointerSize));
        at<unsigned char**>(0x14)[0]=0;
        at<Vec3>(0x1C)={0,0,0};
        at<Vec3>(0xFC)={0,0,0}; at<Vec3>(0x108)={0,0,0};
        at<Mat4>(0x34)=Mat4::getIdentity();                          // operator= 10030E80
        at<unsigned char>(0x210)=0; at<unsigned char>(0x211)=0; at<float>(0x214)=0;
        at<float>(0x200)=0; at<float>(0x204)=1; at<float>(0x208)=0; at<float>(0x20C)=0;
        for (unsigned int offset=0x160;offset<=0x1F0;offset+=0x10)
        { at<float>(offset)=0; at<float>(offset+4)=1; at<float>(offset+8)=0; at<float>(offset+12)=0; }
        // lazy child resolution when slot 89 did not (or could not yet) resolve the name.
        if (!psmDescriptor3<IParticleSystemTemplate*>(descriptor,0x88))
            psmType3ResolveChildTemplate(descriptor);
        at<unsigned char>(4)=0;
    }
    // PE 101814B0 (+ scalar deleting wrapper 10183B40). Returns every live record to the type-3 free
    // list (no counter: unlike types 0/4 there is none for this pool) and frees the pointer list.
    // The records' child instances are NOT released here. Retail destroys an emitter only through
    // the drained pending list, which it reaches with zero live particles, so no child is orphaned
    // on that path; the unrecovered manager teardown is the only other route.
    ~ParticleInstanceType3() override
    {
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        unsigned char** const records=at<unsigned char**>(0x14);
        if (records[0])
            for (unsigned int i=0;at<unsigned char**>(0x14)[i];++i)
            {
                unsigned char* const record=at<unsigned char**>(0x14)[i];
                psmRecordLink(record, 3)=manager->m_type3FreeList;
                manager->m_type3FreeList=record;
            }
        ::renddx9abi::engineFree(at<unsigned char**>(0x14));
    }
    static void operator delete(void* self) { ::renddx9abi::engineFree(self); }
    int reserved01() const override { return 3; }
    // PE 10181660, 277 B. Stop if emitting, run one update of (ttl + randomTtl) when particles
    // remain, then release every record and its child. Unlike Trail (type 2) there is no restart.
    void reserved02() const override
    {
        ParticleInstanceType3* const self=const_cast<ParticleInstanceType3*>(this);
        if (self->at<unsigned char>(0x144)) self->reserved04();
        if (self->at<unsigned int>(0x28))
        {
            void* const descriptor=self->at<void*>(0x2C);
            self->reserved41(static_cast<float>(static_cast<double>(psmDescriptor3<float>(descriptor,0x90))
                +psmDescriptor3<float>(descriptor,0x8C)));
        }
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        while (self->at<unsigned char**>(0x14)[0])
        {
            unsigned char* const record=self->at<unsigned char**>(0x14)[0];
            if (psmRecordPointer<IParticleSystemInstance*>(record, 3))
            {
                // The record's child is read twice, exactly as 101816A5/101816B8 do.
                psmInstance<unsigned char>(psmRecordPointer<IParticleSystemInstance*>(record, 3),5)=0;
                reinterpret_cast<IParticleSystemManager*>(g_ParticleSystemManager)
                    ->removeInstance(psmRecordPointer<IParticleSystemInstance*>(record, 3));
            }
            --self->at<unsigned int>(0x28);
            unsigned char** const records=self->at<unsigned char**>(0x14);
            records[0]=records[self->at<unsigned int>(0x28)];
            self->at<unsigned char**>(0x14)[self->at<unsigned int>(0x28)]=0;
            psmRecordLink(record, 3)=manager->m_type3FreeList;
            manager->m_type3FreeList=record;
        }
        self->at<float>(0x214)=0; self->at<unsigned char>(0x210)=0; self->at<unsigned char>(0x211)=0;
        self->at<unsigned int>(0x134)=0; self->at<unsigned int>(0x28)=0; self->at<float>(0x120)=0;
        self->at<float>(0x12C)=1; self->at<Vec3>(0x114)={0,0,0};
        self->at<Vec3>(0xFC)={0,0,0}; self->at<Vec3>(0x108)={0,0,0};
        self->at<unsigned char**>(0x14)[0]=0;
        self->at<unsigned char>(0x21D)=0; self->at<unsigned char>(0x21E)=1; self->at<unsigned char>(0x21F)=1;
    }
    // PE 10182510, 511 B. Quality refusal, classification before the resets (as type 4), no prewarm
    // loop here (the update runs it, from +0xEC), save/set/restore of the view far plane.
    void reserved03(bool firstPerson) override
    {
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        const int allowedQuality=static_cast<int>(manager->m_qualityLevel);    // read before the call
        if (at<IParticleSystemTemplate*>(0x2C)->getQualityLevel()>allowedQuality) return;  // signed jg
        if (!at<unsigned char>(4)) psmAddActive(manager,this,firstPerson);
        void* const descriptor=at<void*>(0x2C);
        at<unsigned char>(0x144)=1; at<unsigned char>(0x21C)=1;
        at<unsigned int>(0xE8)=psmDescriptor3<unsigned int>(descriptor,0x144);   // emit time
        at<unsigned int>(0xF0)=psmDescriptor3<unsigned int>(descriptor,0x154);   // emit delay
        at<unsigned int>(0xEC)=psmDescriptor3<unsigned int>(descriptor,0x08);    // prewarm time
        at<float>(0xF4)=0; at<float>(0xF8)=0; at<float>(0xD0)=0;
        at<unsigned char>(0x21E)=1; at<unsigned char>(0x21F)=1;
        at<unsigned int>(0x124)=dx9rend::rendererFrameOrdinal();
        psmType3RefreshTransform(this);
        dx9rend::IRenderView* const view=dx9rend::rendererGetRenderViewDup();
        const float oldFar=view->getFarPlane();
        view->setFarPlane(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_effectiveViewDistance);
        at<float>(0x120)=0;
        at<Vec3>(0x114)=at<Vec3>(0xA4); at<Vec3>(0xFC)=at<Vec3>(0xA4); at<Vec3>(0x108)=at<Vec3>(0xA4);
        const float* const matrix=reinterpret_cast<const float*>(view->getViewMatrixPtr());
        const Vec3& center=at<Vec3>(0x114);
        const float x=center.x-matrix[12], y=center.y-matrix[13], z=center.z-matrix[14];
        const double distance=static_cast<double>(view->getFarPlane())-at<float>(0x120);
        // 101826B1 `test ah,5; jp`: unordered takes the visible arm, exactly like type 4.
        if (!(distance*distance<static_cast<double>(x)*x+static_cast<double>(z)*z+static_cast<double>(y)*y))
        {
            at<unsigned int>(0x124)=dx9rend::rendererFrameOrdinal();          // a second call
            at<unsigned char>(0x21E)=1; at<unsigned char>(0x21F)=1;
        }
        else { at<unsigned int>(0x124)=0; at<unsigned char>(0x21E)=0; at<unsigned char>(0x21F)=0; }
        view->setFarPlane(oldFar);
    }
    // note it also clears the first-emission byte +0x21C.
    void reserved04() override { at<unsigned char>(0x144)=0; at<unsigned char>(0x21C)=0; at<float>(0xE8)=0; }
    bool reserved05() const override { return psmShared13IsEmitting(this); }          // 101839D0 (t1)
    void setPaused(bool value) override { psmShared13SetPaused(this,value); }          // 101839E0 (t1)
    bool reserved07() const override { return psmShared13IsPaused(this); }             // 10177570 (t1)
    // detach - the emitter keeps simulating what it has, without owner or parent.
    void reserved08() override { at<unsigned char>(0x18)=1; at<void*>(0x10)=0; at<void*>(0x0C)=0; at<void*>(0x30)=0; }
    void reserved09(bool value) override { psmShared13SetVisible(this,value); }        // 101839F0 (t1)
    bool reserved10() const override { return psmShared13IsVisible(this); }            // 10183A00 (t1)
    void reserved11(intptr_t value) override { at<intptr_t>(0x10)=value; }                       // 1017A500 (all)
    intptr_t reserved12() const override { return at<intptr_t>(0x10); }                          // 1010DE20 (all)
    void reserved13(intptr_t value) override { at<intptr_t>(0x30)=value; }                       // 1017C200 (all)
    // PE 10181790: `add ecx,34h; jmp Mat4::operator=` - the assignment's own `this` is returned.
    void* reserved14(const void* value) override { at<Mat4>(0x34)=*static_cast<const Mat4*>(value); return &at<Mat4>(0x34); }
    const void* reserved15() const override { return &at<Mat4>(0x34); }                // 10179DD0 (all)
    // the inherited-velocity direction. Loads z, y, x, then stores y, x, z.
    void reserved16(const Vec3& value) override
    {
        const float z=value.z, y=value.y, x=value.x;
        at<float>(0xD8)=y; at<float>(0xD4)=x; at<float>(0xDC)=z;
    }
    const Vec3& reserved17() const override { return at<Vec3>(0xD4); }
    // the current collision plane. All four source words are loaded before the first
    // store (z, x, y, then the distance at 10182725); xyz land at +0x200 in the order y, z, x, are
    // normalised in place, and only then is the saved distance written to +0x20C.
    void reserved18(const void* value) override
    {
        const float* const source=static_cast<const float*>(value);
        const float z=source[2], x=source[0], y=source[1], distance=source[3];
        at<float>(0x204)=y; at<float>(0x208)=z; at<float>(0x200)=x;
        psmNormalize3(&at<float>(0x200));
        at<float>(0x20C)=distance;
    }
    void reserved19(float value) override { at<float>(0xE4)=value; }
    float reserved20() const override { return at<float>(0xE4); }
    void reserved21(int,int) override { at<float>(0x154)=1; }                          // 10181580, `retn 8`
    void reserved22(float value) override { at<float>(0x158)=value; }
    float reserved23() const override { return at<float>(0x158); }
    void reserved24(float value) override { at<float>(0x130)=value; }
    float reserved25() const override { return at<float>(0x130); }
    void reserved26(float value) override { psmShared13SetSpeedScale(this,value); }   // 10186110 (t1)
    float reserved27() const override { return psmShared13SpeedScale(this); }        // 10183A10 (t1)
    void reserved28(const Vec3& value) override { psmShared13SetDirectionScale(this,value); } // 10183A30 (t1)
    const Vec3& reserved29() const override { return psmShared13DirectionScale(this); }      // 10183A20 (t1)
    void reserved30(float value) override { at<float>(0x128)=value; }
    float reserved31() const override { return at<float>(0x128); }
    void reserved32(float value) override { at<float>(0xE0)=value; }
    float reserved33() const override { return at<float>(0xE0); }
    int reserved34() const override { return at<int>(0x218); }
    IParticleSystemTemplate* getTemplate() const override { return at<IParticleSystemTemplate*>(0x2C); }
    void reserved36(const Vec3& value) override { psmShared13SetVelocityOffset(this,value); } // 1017C360 (t1,t2)
    void reserved37() override { psmType3NewParticle(this); }                         // 10183080 (own body)
    float reserved38() const override { return psmShared13BoundsRadius(this); }      // 10183A60 (t0,t1,t4)
    int reserved39() const override { return at<int>(0x28); }                         // 101C8440 (all)
    const Vec3& getPosition() const override { return at<Vec3>(0xB4); }               // 1017C240 (all)
    void reserved41(float value) override { psmType3Update(this,value); }
    // the common reclassification body (same shape as types 0/2/4).
    void reserved42(bool value) override
    {
        if (at<unsigned char>(4) && value!=static_cast<bool>(at<unsigned char>(6)))
        { at<unsigned char>(7)=value; psmReclassification(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)).insert(this); }
    }
    float getGasCloudRadius() const override { return 1; }                            // 1017A510 (t1,t2,t4)
    void setPlayer(void*) override {}                                                  // 100E13B0 null body
    void* getPlayer() const override { return 0; }
    void setMaterial(int) override {}                                                  // 100E13B0 null body
    int getMaterial() const override { return -1; }
    void setEffectController(world::IEffectController* value) override               // 101BB440 (all)
    { at<world::IEffectController*>(0xC)=value; reserved11(reinterpret_cast<intptr_t>(value ? value->getObject() : 0)); }
    world::IEffectController* getEffectController() const override { return at<world::IEffectController*>(0xC); }
    bool getWaterSide() const override { return false; }                               // 10083AF0 (t4)
};
#if defined(_M_IX86)
static_assert(sizeof(ParticleInstanceType3)==0x220,"emitter instance PE 100D3E5D allocation / 10182110 layout");
#else
static_assert(sizeof(ParticleInstanceType3)==kPsmInstanceSize,"x64 instance layout, ParticleRawLayout.h");
#endif

// Factory case 3 (PE 100D3E5D): allocate 0x220 and construct; the caller's common tail at 100D3E03
// writes byte +5 = 0 and appends to the master list, as for types 0/2/4.
IParticleSystemInstance* psmCreateType3Instance(IParticleSystemTemplate* descriptor)
{
    void* const storage=::renddx9abi::engineAlloc(sizeof(ParticleInstanceType3));
    return storage ? ::new(storage) ParticleInstanceType3(descriptor) : 0;
}
