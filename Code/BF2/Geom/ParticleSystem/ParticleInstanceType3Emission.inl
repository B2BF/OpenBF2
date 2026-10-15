// Type-3 particle birth and timed emission. Included within the manager's private namespace;
// shares renderer RNG, distribution, and rotation helpers with type-0 emission.

int psmType3AdvanceParticle(void*, unsigned char*, int, float, unsigned int);

// a .data float (0x3C8EFA35) with one reader (10183CD9) and no writer.
const float psmType3DegreesToRadians = 0.01745329238474369f;

// The shared non-radial tail of all three shapes: emitDirection (+0x158, normalised by descriptor
// slot 37) scaled per axis by the instance's direction scale (+0x148, virtual +0x70). z is computed
// first, then y, then x; each is a float product.
void psmType3EmissionDirection(float* output, void* descriptor, void* self)
{
    const float z=psmDescriptor3<float>(descriptor,0x160)*psmInstanceB<float>(self,0x150);
    const float y=psmDescriptor3<float>(descriptor,0x15C)*psmInstanceB<float>(self,0x14C);
    const float x=psmDescriptor3<float>(descriptor,0x158)*psmInstanceB<float>(self,0x148);
    output[0]=x; output[1]=y; output[2]=z;
}

// PE 10183B70, 296 B: five draws even when the radius is zero; phi and the cos(theta) argument are
// rounded to float, sin(theta) is taken on the unrounded angle (as in the type-0 sphere).
void psmType3EmissionSphere(float* output, void* descriptor, void* self, unsigned char* record)
{
    const float phi=static_cast<float>(psmEmissionRandomInteger()*static_cast<double>(3.7450703871400037e-7f));
    const double theta=psmEmissionRandomInteger()*static_cast<double>(1.8725351935700019e-7f);
    const float savedTheta=static_cast<float>(theta), sineTheta=static_cast<float>(::sin(theta));
    const float yRandom=static_cast<float>(psmEmissionUnit()), xRandom=static_cast<float>(psmEmissionUnit());
    const double zRadius=psmEmissionRandomInteger()*static_cast<double>(psmDescriptor3<float>(descriptor,0x18))
        *0.000000059604644775390625;
    const double xRadius=static_cast<double>(xRandom)*psmDescriptor3<float>(descriptor,0x10);
    const double yRadius=static_cast<double>(yRandom)*psmDescriptor3<float>(descriptor,0x14);
    psmRuntimeField<float>(record,0x44)=static_cast<float>(::cos(phi)*xRadius*sineTheta);
    psmRuntimeField<float>(record,0x48)=static_cast<float>(::sin(phi)*yRadius*sineTheta);
    psmRuntimeField<float>(record,0x4C)=static_cast<float>(::cos(savedTheta)*zRadius);
    psmType3EmissionDirection(output,descriptor,self);
}

// PE 10183CA0, 495 B: emitter types 1 (radial - the direction is the normalised position with the
// dominant axis removed) and 2 (the configured direction). The axis is the first strictly largest
// radius component; the distributions are sampled Z, Y, X.
void psmType3EmissionCylinder(float* output, void* descriptor, void* self, unsigned char* record)
{
    const double angle=(psmEmissionSymmetric()*psmDescriptor3<float>(descriptor,0x58)
        +psmDescriptor3<float>(descriptor,0x5C))*psmType3DegreesToRadians;
    const float cosine=static_cast<float>(::cos(angle)), sine=static_cast<float>(::sin(angle));
    unsigned int axis=0;                          // 10183CEB compares radius.x with itself: no effect
    if (psmDescriptor3<float>(descriptor,0x14)>psmDescriptor3<float>(descriptor,0x10)) axis=1;
    if (psmDescriptor3<float>(descriptor,0x18)>psmDescriptor3<float>(descriptor,0x10+4*axis)) axis=2;
    unsigned char* const bytes=static_cast<unsigned char*>(descriptor);
    const float z=static_cast<float>(psmEmissionDistribution(bytes+0x44));
    const float y=static_cast<float>(psmEmissionDistribution(bytes+0x30));
    const double x=psmEmissionDistribution(bytes+0x1C);
    float* const position=&psmRuntimeField<float>(record,0x44);
    if (axis==0) { position[0]=static_cast<float>(x); position[1]=y*sine; position[2]=z*cosine; }
    else if (axis==1) { position[0]=static_cast<float>(x*sine); position[1]=y; position[2]=z*cosine; }
    else { position[0]=static_cast<float>(x*sine); position[2]=z; position[1]=y*cosine; }
    if (psmDescriptor3<int>(descriptor,0x188)==1)
    {
        output[0]=position[0]; output[1]=position[1]; output[2]=position[2];
        output[axis]=0;
        psmNormalize3(output);
    }
    else psmType3EmissionDirection(output,descriptor,self);
}


void psmType3EmissionBox(float* output, void* descriptor, void* self, unsigned char* record)
{
    unsigned char* const bytes=static_cast<unsigned char*>(descriptor);
    const float z=static_cast<float>(psmEmissionDistribution(bytes+0x44));
    const float y=static_cast<float>(psmEmissionDistribution(bytes+0x30));
    psmRuntimeField<float>(record,0x44)=static_cast<float>(psmEmissionDistribution(bytes+0x1C));
    psmRuntimeField<float>(record,0x48)=y; psmRuntimeField<float>(record,0x4C)=z;
    psmType3EmissionDirection(output,descriptor,self);
}

// The particle birth shared by PE 10183080 (virtual +0x94) and the copy 101827F0 inlines at
// The two compiled copies are identical instruction for instruction except for
// the association of the direction rotation's three-term sums, which `emissionLoopOrder` selects.
// [CHOSEN, not recovered] packaging: one body with that flag instead of two near-identical copies.
void psmType3InitRecord(void* self, unsigned char* record, bool emissionLoopOrder)
{
    ++psmInstanceB<unsigned int>(self,0x13C);
    void* descriptor=psmInstanceB<void*>(self,0x2C);
    const double lifetime=psmEmissionRandomInteger()*static_cast<double>(psmDescriptor3<float>(descriptor,0x8C))
        *0.000000059604644775390625+psmDescriptor3<float>(descriptor,0x90);
    psmRuntimeField<float>(record,0x80)=static_cast<float>(lifetime);
    psmRuntimeField<float>(record,0x84)=static_cast<float>(1.0/lifetime);      // unrounded divisor
    psmRuntimeField<float>(record,0x7C)=psmInstanceB<float>(self,0xE0);
    psmRuntimeField<float>(record,0x5C)=psmRuntimeField<float>(record,0x7C)*psmRuntimeField<float>(record,0x84);
    psmRuntimeField<unsigned int>(record,0x68)=psmInstanceB<unsigned int>(self,0x158);
    psmRuntimeField<unsigned int>(record,0x6C)=psmInstanceB<unsigned int>(self,0x158);
    const float firstRandom=static_cast<float>(psmEmissionSymmetric());
    psmEmissionRaw();                            // a discarded draw (10183149), as in the siblings
    const double spin=psmEmissionSymmetric();
    descriptor=psmInstanceB<void*>(self,0x2C);
    const double spinX=spin*psmDescriptor3<float>(descriptor,0x124), spinY=spin*psmDescriptor3<float>(descriptor,0x128);
    psmRuntimeField<float>(record,0x9C)=static_cast<float>(spin*psmDescriptor3<float>(descriptor,0x12C));
    psmRuntimeField<float>(record,0x94)=static_cast<float>(spinX);
    psmRuntimeField<float>(record,0x98)=static_cast<float>(spinY);
    // Rotation speed: base (+0xAC) plus a signed random part (+0xA0); a negative draw subtracts the
    // base so the magnitude grows either way. The draw is exactly representable, so its sign is
    // unambiguous (the compare runs on the unrounded value).
    const float rate=static_cast<float>(psmEmissionSymmetric());
    descriptor=psmInstanceB<void*>(self,0x2C);
    const double rateX=static_cast<double>(rate)*psmDescriptor3<float>(descriptor,0xA0);
    const float rateY=static_cast<float>(static_cast<double>(rate)*psmDescriptor3<float>(descriptor,0xA4));
    const float rateZ=static_cast<float>(static_cast<double>(rate)*psmDescriptor3<float>(descriptor,0xA8));
    if (!(rate<0))
    {
        psmRuntimeField<float>(record,0x90)=static_cast<float>(static_cast<double>(rateZ)+psmDescriptor3<float>(descriptor,0xB4));
        psmRuntimeField<float>(record,0x88)=static_cast<float>(rateX+psmDescriptor3<float>(descriptor,0xAC));
        psmRuntimeField<float>(record,0x8C)=static_cast<float>(static_cast<double>(rateY)+psmDescriptor3<float>(descriptor,0xB0));
    }
    else
    {
        const float negativeY=-psmDescriptor3<float>(descriptor,0xB0), negativeZ=-psmDescriptor3<float>(descriptor,0xB4);
        psmRuntimeField<float>(record,0x90)=static_cast<float>(static_cast<double>(negativeZ)+rateZ);
        psmRuntimeField<float>(record,0x88)=static_cast<float>(rateX+(-static_cast<double>(psmDescriptor3<float>(descriptor,0xAC))));
        psmRuntimeField<float>(record,0x8C)=static_cast<float>(static_cast<double>(negativeY)+rateY);
    }
    // An emitter type outside 0..3 writes no position: the record keeps its previous occupant's.
    float direction[3]={0,0,0};
    descriptor=psmInstanceB<void*>(self,0x2C);
    switch (psmDescriptor3<int>(descriptor,0x188))
    {
    case 0: psmType3EmissionSphere(direction,descriptor,self,record); break;
    case 1: case 2: psmType3EmissionCylinder(direction,descriptor,self,record); break;
    case 3: psmType3EmissionBox(direction,descriptor,self,record); break;
    }
    const float randomX=static_cast<float>(psmEmissionSymmetric());
    const float randomY=static_cast<float>(psmEmissionSymmetric());
    const float randomZ=static_cast<float>(psmEmissionSymmetric());
    descriptor=psmInstanceB<void*>(self,0x2C);
    Mat4 rotation,axis,copy;
    psmEmissionRotation(rotation,randomX*psmDescriptor3<float>(descriptor,0x134),0);
    psmEmissionRotation(axis,randomY*psmDescriptor3<float>(descriptor,0x138),1);
    copy=rotation; psmEmissionRotateMultiply(rotation,copy,axis);
    psmEmissionRotation(axis,randomZ*psmDescriptor3<float>(descriptor,0x13C),2);
    copy=rotation; psmEmissionRotateMultiply(rotation,copy,axis);
    const float* const m=rotation.m;
    const double dx=direction[0], dy=direction[1], dz=direction[2];
    if (!emissionLoopOrder)
    {

        direction[2]=static_cast<float>(m[2]*dx+m[6]*dy+m[10]*dz);
        direction[1]=static_cast<float>(m[1]*dx+m[9]*dz+m[5]*dy);
        direction[0]=static_cast<float>(m[0]*dx+m[8]*dz+m[4]*dy);
    }
    else
    {
        // the same products, associated from the other end.
        direction[2]=static_cast<float>(m[6]*dy+m[10]*dz+m[2]*dx);
        direction[1]=static_cast<float>(m[9]*dz+m[5]*dy+m[1]*dx);
        direction[0]=static_cast<float>(m[8]*dz+m[4]*dy+m[0]*dx);
    }
    descriptor=psmInstanceB<void*>(self,0x2C);
    float speed=static_cast<float>((psmDescriptor3<float>(descriptor,0x140)
        -static_cast<double>(firstRandom)*psmDescriptor3<float>(descriptor,0x130))*psmInstanceB<float>(self,0x138));
    const float emitTime=psmDescriptor3<float>(descriptor,0x144);
    const double t=emitTime>0 ? 1.0-static_cast<double>(psmInstanceB<float>(self,0xE8))/emitTime : 0.0;
    const float envelope=static_cast<float>(t*psmDescriptor3<float>(descriptor,0x10C)
        +t*t*psmDescriptor3<float>(descriptor,0x108)+t*t*t*psmDescriptor3<float>(descriptor,0x104)
        +psmDescriptor3<float>(descriptor,0x110));
    speed=static_cast<float>(static_cast<double>(psmEmissionClamp(envelope))*speed);
    descriptor=psmInstanceB<void*>(self,0x2C);
    if (!psmDescriptor3<unsigned char>(descriptor,0x165))
    {
        // PE 10079610 (point, in place) and 100A6120 (vector, in place) through the world matrix.
        const float* const w=&psmInstanceB<float>(self,0x74);
        float* const position=&psmRuntimeField<float>(record,0x44);
        const double px=position[0], py=position[1], pz=position[2];
        const double rz=static_cast<double>(w[2])*px+static_cast<double>(w[10])*pz+static_cast<double>(w[6])*py+w[14];
        const double ry=static_cast<double>(w[1])*px+static_cast<double>(w[9])*pz+static_cast<double>(w[5])*py+w[13];
        position[0]=static_cast<float>(static_cast<double>(w[8])*pz+static_cast<double>(w[4])*py+static_cast<double>(w[0])*px+w[12]);
        position[1]=static_cast<float>(ry); position[2]=static_cast<float>(rz);
        const double vx=direction[0], vy=direction[1], vz=direction[2];
        direction[2]=static_cast<float>(static_cast<double>(w[10])*vz+static_cast<double>(w[2])*vx+static_cast<double>(w[6])*vy);
        direction[1]=static_cast<float>(static_cast<double>(w[9])*vz+static_cast<double>(w[1])*vx+static_cast<double>(w[5])*vy);
        direction[0]=static_cast<float>(static_cast<double>(w[8])*vz+static_cast<double>(w[4])*vy+static_cast<double>(w[0])*vx);
        // Inherited velocity: +0xE4 (virtual +0x4C) times the direction at +0xD4 (virtual +0x40).
        const double scale=psmInstanceB<float>(self,0xE4);
        const float inheritedX=static_cast<float>(scale*psmInstanceB<float>(self,0xD4));
        const float inheritedY=static_cast<float>(scale*psmInstanceB<float>(self,0xD8));
        const float vyScaled=direction[1]*speed, vzScaled=direction[2]*speed;
        psmRuntimeField<float>(record,0x78)=static_cast<float>(vzScaled+scale*psmInstanceB<float>(self,0xDC));
        psmRuntimeField<float>(record,0x70)=static_cast<float>(static_cast<double>(direction[0])*speed+inheritedX);
        psmRuntimeField<float>(record,0x74)=vyScaled+inheritedY;
    }
    else
    {
        // Local system: the position stays local and nothing is inherited.
        psmRuntimeField<float>(record,0x78)=direction[2]*speed;
        psmRuntimeField<float>(record,0x70)=direction[0]*speed;
        psmRuntimeField<float>(record,0x74)=direction[1]*speed;
    }
    // No capacity check here: 10183080 appends even when count == capacity (the emission loop
    // checks before it pops a record, the virtual +0x94 path does not). Same in the type-0 sibling.
    psmInstanceB<unsigned char**>(self,0x14)[psmInstanceB<unsigned int>(self,0x28)]=record;
    ++psmInstanceB<unsigned int>(self,0x28);
    psmInstanceB<unsigned char**>(self,0x14)[psmInstanceB<unsigned int>(self,0x28)]=0;

    // THE child. createInstance is the manager's own virtual (+0x14): pool initialisation, the
    // per-type factory and the master-list append all apply. The resolved template can be of ANY type.
    descriptor=psmInstanceB<void*>(self,0x2C);
    IParticleSystemTemplate* const childTemplate=psmDescriptor3<IParticleSystemTemplate*>(descriptor,0x88);
    if (childTemplate)
    {
        IParticleSystemInstance* const child=
            reinterpret_cast<IParticleSystemManager*>(g_ParticleSystemManager)->createInstance(childTemplate);
        psmRecordPointer<IParticleSystemInstance*>(record, 3)=child;
        // No null guard in retail: 101835D4 (slot +0x94) / 10182F3A (the copy timed emission
        // inlines) dereference the answer at once - it is null only when the engine allocator fails.
        // [CHOSEN, not recovered] A named stop at the fault point, as in advanceParticle.
        if (!child)
        {
            RD9_FRONTIER_AT("dice::hfe::geom::ParticleSystemEmitter::newParticle (createInstance returned "
                            "null; retail faults at 101835D4, or 10182F3A on the timed-emission copy)",
                            self, 0x94);
            std::abort();
        }
        child->reserved13(reinterpret_cast<intptr_t>(record+4));       // parent matrix = this particle's
        // Every following use re-reads record+0xAC, as the instructions do.
        psmInstance<unsigned char>(psmRecordPointer<IParticleSystemInstance*>(record, 3),5)=1;   // not bucket-updated
        psmInstance<unsigned char>(psmRecordPointer<IParticleSystemInstance*>(record, 3),8)=
            psmInstanceB<unsigned char>(self,8);                // FOV-override byte (setFovOverride)
        psmRecordPointer<IParticleSystemInstance*>(record, 3)->reserved03(psmInstanceB<unsigned char>(self,6)!=0);
        psmRecordPointer<IParticleSystemInstance*>(record, 3)->reserved30(psmInstanceB<float>(self,0x128));
        psmRecordPointer<IParticleSystemInstance*>(record, 3)->reserved22(psmInstanceB<float>(self,0x158));
    }
    else psmRecordPointer<IParticleSystemInstance*>(record, 3)=0;
}

// PE 10183080, 1467 B (virtual +0x94): pop a record and initialise it at the current emitter
// position. No interpolation, no velocity offset (+0x1C) and no catch-up advance - those belong to
// the emission loop below.
void psmType3NewParticle(void* self)
{
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    unsigned char* const record=static_cast<unsigned char*>(manager->m_type3FreeList);
    manager->m_type3FreeList=record ? psmRecordLink(record, 3) : 0;
    if (!record) return;
    psmType3InitRecord(self,record,false);
}

// PE 101827F0, 2187 B: timed emission. `dt` is unused, as in types 0 and 4.
void psmType3Emit(void* self, float /*dt*/)
{
    if (!psmInstanceB<unsigned char>(self,0x144)) return;
    const unsigned int frame=static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getFrameOrdinal();
    // Nothing refreshes +0x124 for a type-3 instance after start: the sprite renderer's prepare
    // walk skips reserved01() != 0, and no other type-3 path writes it. An emitter therefore emits
    // for at most ~60 frames after start() (stock emitters use 0.1-0.3 s emit times).
    if (psmInstanceB<unsigned int>(self,0x124)+60u<frame)
    {
        psmInstanceB<unsigned char>(self,0x21F)=psmInstanceB<unsigned char>(self,0x21E);
        psmInstanceB<unsigned char>(self,0x21E)=0;
        return;
    }
    // No pool-pressure throttle (types 0/4 scale the frequency by the tier thresholds); the second
    // factor (+0x12C) is only ever 1.0 for type 3 - the coverage pass that writes the sprite
    // equivalent skips non-zero types - but the shipped multiply is kept.
    const float interval=static_cast<float>(1.0/(static_cast<double>(psmDescriptor3<float>(psmInstanceB<void*>(self,0x2C),0x0C))
        *psmInstanceB<float>(self,0x130)*psmInstanceB<float>(self,0x12C)));
    if (psmInstanceB<unsigned char>(self,0x21C))
    {
        psmInstanceB<Vec3>(self,0xC0)=psmInstanceB<Vec3>(self,0xB4);
        psmInstanceB<unsigned char>(self,0x21C)=0;
    }
    while (static_cast<double>(interval)+psmInstanceB<float>(self,0xF8)<=psmInstanceB<float>(self,0xF4))
    {
        if (psmInstanceB<float>(self,0xE8)!=0)                 // `fucompp`: NaN counts down too
        {
            const double remaining=static_cast<double>(psmInstanceB<float>(self,0xE8))-interval;
            psmInstanceB<float>(self,0xE8)=static_cast<float>(remaining);
            if (remaining<=0)
            {
                IParticleSystemInstance* const instance=static_cast<IParticleSystemInstance*>(self);
                instance->reserved04();
                if (psmDescriptor3<unsigned char>(psmInstanceB<void*>(self,0x2C),0x148))
                    instance->reserved03(psmInstanceB<unsigned char>(self,6)!=0);
                return;
            }
        }
        const bool visible=psmInstanceB<unsigned char>(self,0x21E)!=0;
        const float emitted=static_cast<float>(static_cast<double>(interval)+psmInstanceB<float>(self,0xF8));
        psmInstanceB<float>(self,0xF8)=emitted;
        if (!visible || psmInstanceB<unsigned int>(self,0x28)>=psmInstanceB<unsigned int>(self,0x218))
            continue;
        float fraction=0;
        if (psmInstanceB<float>(self,0xF4)>psmInstanceB<float>(self,0xCC))
            fraction=static_cast<float>((static_cast<double>(emitted)-psmInstanceB<float>(self,0xCC))
                /(static_cast<double>(psmInstanceB<float>(self,0xF4))-psmInstanceB<float>(self,0xCC)));
        // lerp(previous +0xC0, current +0xB4, fraction) into +0xA4, before the pool pop.
        {
            const Vec3 from=psmInstanceB<Vec3>(self,0xC0), to=psmInstanceB<Vec3>(self,0xB4);
            const float zDifference=to.z-from.z;
            const float xScaled=static_cast<float>((static_cast<double>(to.x)-from.x)*fraction);
            const double yScaled=(static_cast<double>(to.y)-from.y)*fraction;
            const float zScaled=zDifference*fraction;
            psmInstanceB<Vec3>(self,0xA4)={xScaled+from.x,static_cast<float>(yScaled+from.y),zScaled+from.z};
        }
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        unsigned char* const record=static_cast<unsigned char*>(manager->m_type3FreeList);
        manager->m_type3FreeList=record ? psmRecordLink(record, 3) : 0;
        if (!record) continue;                                   // an empty pool skips, not stops
        psmType3InitRecord(self,record,true);
        // The velocity offset (+0x1C, virtual +0x90) is added with no near-zero snap (type 0 snaps).
        psmRuntimeField<float>(record,0x70)=psmInstanceB<float>(self,0x1C)+psmRuntimeField<float>(record,0x70);
        psmRuntimeField<float>(record,0x74)=psmInstanceB<float>(self,0x20)+psmRuntimeField<float>(record,0x74);
        psmRuntimeField<float>(record,0x78)=psmInstanceB<float>(self,0x24)+psmRuntimeField<float>(record,0x78);
        // Catch the new particle up from its birth time to the accumulated time: whole 0.01 s steps,
        // then the float remainder, both at the newest index.
        const unsigned int steps=static_cast<unsigned int>(static_cast<int>((static_cast<double>(psmInstanceB<float>(self,0xF4))
            -psmInstanceB<float>(self,0xF8))*100.0));
        if (psmType3AdvanceParticle(self,record,static_cast<int>(psmInstanceB<unsigned int>(self,0x28)-1u),
                                    psmType3TimeStep,steps))
        {
            const float remainder=static_cast<float>((static_cast<double>(psmInstanceB<float>(self,0xF4))
                -psmInstanceB<float>(self,0xF8))-static_cast<double>(steps)*psmType3TimeStep);
            psmType3AdvanceParticle(self,record,static_cast<int>(psmInstanceB<unsigned int>(self,0x28)-1u),
                                    remainder,1);
        }
    }
}
