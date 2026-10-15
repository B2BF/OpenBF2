// Mesh-particle emission shares the renderer's random stream with all particle types.
// Scales the descriptor's normalized direction by the instance's per-axis direction scale.
void psmType1EmissionDirection(float* output,void* descriptor,void* self)
{
    output[0]=psmDescriptor1<float>(descriptor,0x1DC)*psmInstanceB<float>(self,0x148);
    output[1]=psmDescriptor1<float>(descriptor,0x1E0)*psmInstanceB<float>(self,0x14C);
    output[2]=psmDescriptor1<float>(descriptor,0x1E4)*psmInstanceB<float>(self,0x150);
}

// Five draws even when all radii are zero; radii are emitRadius (+0x14/+0x18/+0x1C).
void psmType1EmissionSphere(float* output,void* descriptor,void* self,float* particle)
{
    const float phi=static_cast<float>(psmEmissionRandomInteger()*static_cast<double>(3.7450703871400037e-7f));
    const double theta=psmEmissionRandomInteger()*static_cast<double>(1.8725351935700019e-7f);
    const float savedTheta=static_cast<float>(theta);
    const float sineTheta=static_cast<float>(::sin(theta));    // fsin on the unrounded angle
    const float yRandom=static_cast<float>(psmEmissionUnit());
    const float xRandom=static_cast<float>(psmEmissionUnit());
    const double zRadius=psmEmissionRandomInteger()*static_cast<double>(psmDescriptor1<float>(descriptor,0x1C))
        *0.000000059604644775390625;
    const double xRadius=static_cast<double>(xRandom)*psmDescriptor1<float>(descriptor,0x14);
    const double yRadius=static_cast<double>(yRandom)*psmDescriptor1<float>(descriptor,0x18);
    psmRuntimeField<float>(particle,0x44)=static_cast<float>(::cos(phi)*xRadius*sineTheta);
    psmRuntimeField<float>(particle,0x48)=static_cast<float>(::sin(phi)*yRadius*sineTheta);
    psmRuntimeField<float>(particle,0x4C)=static_cast<float>(::cos(savedTheta)*zRadius);
    psmType1EmissionDirection(output,descriptor,self);
}

// PE 10186290, emitter types 1 (radial) and 2. Largest-axis ties keep the earlier axis; the
// distributions are sampled Z, Y, X (+0x48, +0x34, +0x20); the angle is
// (sym * randomRadialDirection +0x5C + radialDirection +0x60) degrees.
// 101862DB..101862E6 first compares emitRadius.x with itself and discards the answer - a dead
// `axis = 0` step of the shipped largest-component search, with no observable effect.
void psmType1EmissionCylinder(float* output,void* descriptor,void* self,float* particle)
{
    const double angle=(psmEmissionSymmetric()*psmDescriptor1<float>(descriptor,0x5C)
        +psmDescriptor1<float>(descriptor,0x60))*static_cast<double>(psmType1DegreesToRadians);
    const float cosine=static_cast<float>(::cos(angle)), sine=static_cast<float>(::sin(angle));
    unsigned int axis=0;
    if(psmDescriptor1<float>(descriptor,0x18)>psmDescriptor1<float>(descriptor,0x14))axis=1;
    if(psmDescriptor1<float>(descriptor,0x1C)>psmDescriptor1<float>(descriptor,0x14+4*axis))axis=2;
    unsigned char* const bytes=static_cast<unsigned char*>(descriptor);
    const float z=static_cast<float>(psmEmissionDistribution(bytes+0x48));
    const float y=static_cast<float>(psmEmissionDistribution(bytes+0x34));
    const double x=psmEmissionDistribution(bytes+0x20);
    float* const position=&psmRuntimeField<float>(particle,0x44);
    if(axis==0){position[0]=static_cast<float>(x);position[1]=y*sine;position[2]=z*cosine;}
    else if(axis==1){position[0]=static_cast<float>(x*sine);position[1]=y;position[2]=z*cosine;}
    else{position[0]=static_cast<float>(x*sine);position[1]=y*cosine;position[2]=z;}
    if(psmDescriptor1<int>(descriptor,0x214)==1)
    {
        // Radial: the emitted offset with the cylinder axis zeroed, normalised in place.
        output[0]=position[0];output[1]=position[1];output[2]=position[2];
        output[axis]=0;
        psmNormalize3(output);
    }
    else psmType1EmissionDirection(output,descriptor,self);
}

// Z, Y, X distributions, then the configured direction.
void psmType1EmissionBox(float* output,void* descriptor,void* self,float* particle)
{
    unsigned char* const bytes=static_cast<unsigned char*>(descriptor);
    const float z=static_cast<float>(psmEmissionDistribution(bytes+0x48));
    const float y=static_cast<float>(psmEmissionDistribution(bytes+0x34));
    psmRuntimeField<float>(particle,0x44)=static_cast<float>(psmEmissionDistribution(bytes+0x20));
    psmRuntimeField<float>(particle,0x48)=y;psmRuntimeField<float>(particle,0x4C)=z;
    psmType1EmissionDirection(output,descriptor,self);
}

// PE 101857D0 (table +0x94, void) and its inlined twin inside 10184F00. The inline copy continues
// with the particle pointer, so this spelling returns it; +0x94 discards it.
// the two copies are NOT instruction-identical. They add
// the three rotated-direction terms in a different order (10185BA4..10185BEA vs 10185505..1018554E),
// which can change the emitted velocity in the last bit. `inlinedCopy` selects the inline copy's order.
float* psmType1NewParticle(void* self, bool inlinedCopy)
{
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    float* const particle=static_cast<float*>(manager->m_type1FreeList);
    manager->m_type1FreeList=particle ? psmRecordLink(particle, 1) : 0;
    if(!particle)return 0;
    ++psmInstanceB<unsigned int>(self,0x13C);
    void* descriptor=psmInstanceB<void*>(self,0x2C);
    // Lifetime = TTL (+0x9C) + unit * randomTTL (+0x98); the reciprocal divides the unrounded sum.
    const double lifetime=psmEmissionRandomInteger()*static_cast<double>(psmDescriptor1<float>(descriptor,0x98))
        *0.000000059604644775390625+psmDescriptor1<float>(descriptor,0x9C);
    psmRuntimeField<float>(particle,0x94)=static_cast<float>(lifetime);
    psmRuntimeField<float>(particle,0x98)=static_cast<float>(1.0/lifetime);
    psmRuntimeField<float>(particle,0x90)=psmInstanceB<float>(self,0xE0);
    psmRuntimeField<float>(particle,0x5C)=psmInstanceB<float>(self,0xE0)*psmRuntimeField<float>(particle,0x98);
    // Plain dword moves in the PE (10185864..10185882).
    psmRuntimeField<unsigned int>(particle,0x74)=psmInstanceB<unsigned int>(self,0x154);
    psmRuntimeField<unsigned int>(particle,0x6C)=psmInstanceB<unsigned int>(self,0x158);
    psmRuntimeField<unsigned int>(particle,0x70)=psmInstanceB<unsigned int>(self,0x158);
    psmRuntimeField<unsigned int>(particle,0xB4)=psmDescriptor1<unsigned int>(psmInstanceB<void*>(self,0x2C),0xC8);
    // The first symmetric draw is kept as a float: it scales the size jitter now and the speed below.
    const float firstRandom=static_cast<float>(psmEmissionSymmetric());
    psmRuntimeField<float>(particle,0x68)=firstRandom*psmDescriptor1<float>(psmInstanceB<void*>(self,0x2C),0x16C);
    psmRuntimeField<float>(particle,0x78)=static_cast<float>(psmEmissionSymmetric()
        *psmDescriptor1<float>(psmInstanceB<void*>(self,0x2C),0x170));
    {
        // One draw scales all three randomRotation components (+0x174); stored z, x, y.
        const double shared=psmEmissionSymmetric();
        void* const d=psmInstanceB<void*>(self,0x2C);
        const double spinX=shared*psmDescriptor1<float>(d,0x174);
        const double spinY=shared*psmDescriptor1<float>(d,0x178);
        psmRuntimeField<float>(particle,0xB0)=static_cast<float>(shared*psmDescriptor1<float>(d,0x17C));
        psmRuntimeField<float>(particle,0xA8)=static_cast<float>(spinX);
        psmRuntimeField<float>(particle,0xAC)=static_cast<float>(spinY);
    }
    {
        // Spin rate: random part (+0xAC) plus or minus the base rate (+0xB8) by the draw's sign.
        // 1018596D tests the draw with `test ah,1`, so an unordered value would take the minus
        // arm. The X product stays unrounded; Y and Z are rounded before the base is applied.
        const float draw=static_cast<float>(psmEmissionSymmetric());
        void* const d=psmInstanceB<void*>(self,0x2C);
        const double productX=static_cast<double>(draw)*psmDescriptor1<float>(d,0xAC);
        const float productY=static_cast<float>(static_cast<double>(draw)*psmDescriptor1<float>(d,0xB0));
        const float productZ=static_cast<float>(static_cast<double>(draw)*psmDescriptor1<float>(d,0xB4));
        double rateX,rateY,rateZ;
        if(!(draw>=0))
        {
            rateX=productX+static_cast<double>(-psmDescriptor1<float>(d,0xB8));
            rateY=static_cast<double>(-psmDescriptor1<float>(d,0xBC))+productY;
            rateZ=static_cast<double>(-psmDescriptor1<float>(d,0xC0))+productZ;
        }
        else
        {
            rateX=productX+psmDescriptor1<float>(d,0xB8);
            rateY=static_cast<double>(productY)+psmDescriptor1<float>(d,0xBC);
            rateZ=static_cast<double>(productZ)+psmDescriptor1<float>(d,0xC0);
        }
        psmRuntimeField<float>(particle,0xA4)=static_cast<float>(rateZ);
        psmRuntimeField<float>(particle,0x9C)=static_cast<float>(rateX);
        psmRuntimeField<float>(particle,0xA0)=static_cast<float>(rateY);
    }
    float direction[3]={0,0,0};
    descriptor=psmInstanceB<void*>(self,0x2C);
    switch(psmDescriptor1<int>(descriptor,0x214))
    {
    case 0:psmType1EmissionSphere(direction,descriptor,self,particle);break;
    case 1:case 2:psmType1EmissionCylinder(direction,descriptor,self,particle);break;
    case 3:psmType1EmissionBox(direction,descriptor,self,particle);break;
    }
    // Three more draws, then X-Y-Z random rotation of the direction by randomDirectionAngle
    // (+0x184/+0x188/+0x18C, degrees).
    const float randomX=static_cast<float>(psmEmissionSymmetric());
    const float randomY=static_cast<float>(psmEmissionSymmetric());
    const float randomZ=static_cast<float>(psmEmissionSymmetric());
    descriptor=psmInstanceB<void*>(self,0x2C);
    Mat4 rotation,axis,copy;
    psmEmissionRotation(rotation,randomX*psmDescriptor1<float>(descriptor,0x184),0);
    psmEmissionRotation(axis,randomY*psmDescriptor1<float>(psmInstanceB<void*>(self,0x2C),0x188),1);
    copy=rotation;psmEmissionRotateMultiply(rotation,copy,axis);
    psmEmissionRotation(axis,randomZ*psmDescriptor1<float>(psmInstanceB<void*>(self,0x2C),0x18C),2);
    copy=rotation;psmEmissionRotateMultiply(rotation,copy,axis);
    const float x=direction[0],y=direction[1],z=direction[2];
    const double m0x=static_cast<double>(rotation.m[0])*x, m4y=static_cast<double>(rotation.m[4])*y,
                 m8z=static_cast<double>(rotation.m[8])*z, m1x=static_cast<double>(rotation.m[1])*x,
                 m5y=static_cast<double>(rotation.m[5])*y, m9z=static_cast<double>(rotation.m[9])*z,
                 m2x=static_cast<double>(rotation.m[2])*x, m6y=static_cast<double>(rotation.m[6])*y,
                 m10z=static_cast<double>(rotation.m[10])*z;
    double rotatedX, rotatedY, rotatedZ;
    if (!inlinedCopy)
    {
        // Z=(m2x+m6y)+m10z, Y=(m1x+m9z)+m5y, X=(m0x+m8z)+m4y.
        rotatedZ=m2x+m6y+m10z; rotatedY=m1x+m9z+m5y; rotatedX=m0x+m8z+m4y;
    }
    else
    {
        // The copy 10184F00 inlines, Z=(m6y+m10z)+m2x, Y=(m9z+m5y)+m1x,
        // X=(m8z+m4y)+m0x.
        rotatedZ=m6y+m10z+m2x; rotatedY=m9z+m5y+m1x; rotatedX=m8z+m4y+m0x;
    }
    direction[0]=static_cast<float>(rotatedX);
    direction[1]=static_cast<float>(rotatedY);direction[2]=static_cast<float>(rotatedZ);
    // Speed = (emitSpeed +0x190 - first draw * randomSpeed +0x180) * instance speed scale +0x138,
    // then scaled by the clamped emitSpeedGraph (+0x15C t^3, +0x160 t^2, +0x164 t, +0x168 const)
    // at t = 1 - timeLeft/emitTime (0 when emitTime is not an ordered positive number).
    descriptor=psmInstanceB<void*>(self,0x2C);
    float speed=static_cast<float>((psmDescriptor1<float>(descriptor,0x190)
        -static_cast<double>(firstRandom)*psmDescriptor1<float>(descriptor,0x180))*psmInstanceB<float>(self,0x138));
    const float emitTime=psmDescriptor1<float>(descriptor,0x194);
    const double t=emitTime>0?1.0-static_cast<double>(psmInstanceB<float>(self,0xE8))/emitTime:0;
    const float envelope=static_cast<float>(t*t*psmDescriptor1<float>(descriptor,0x160)
        +t*psmDescriptor1<float>(descriptor,0x164)+t*t*t*psmDescriptor1<float>(descriptor,0x15C)
        +psmDescriptor1<float>(descriptor,0x168));
    speed*=psmEmissionClamp(envelope);
    if(!psmDescriptor1<unsigned char>(psmInstanceB<void*>(self,0x2C),0x1F1))
    {
        // World system: PE 10079610 moves the emitted offset through the world matrix in place,
        // PE 100A6120 rotates the direction, then the owner's inherited velocity is added.
        const float* const matrix=psmInstanceB<Mat4>(self,0x74).m;
        float* const position=&psmRuntimeField<float>(particle,0x44);
        const float px=position[0],py=position[1],pz=position[2];
        const double worldZ=static_cast<double>(matrix[2])*px+static_cast<double>(matrix[10])*pz+static_cast<double>(matrix[6])*py+matrix[14];
        const double worldY=static_cast<double>(matrix[1])*px+static_cast<double>(matrix[9])*pz+static_cast<double>(matrix[5])*py+matrix[13];
        const double worldX=static_cast<double>(matrix[8])*pz+static_cast<double>(matrix[4])*py+static_cast<double>(px)*matrix[0]+matrix[12];
        position[0]=static_cast<float>(worldX);position[1]=static_cast<float>(worldY);position[2]=static_cast<float>(worldZ);
        const float dx=direction[0],dy=direction[1],dz=direction[2];
        direction[2]=static_cast<float>(static_cast<double>(matrix[10])*dz+static_cast<double>(matrix[2])*dx+static_cast<double>(matrix[6])*dy);
        direction[1]=static_cast<float>(static_cast<double>(matrix[9])*dz+static_cast<double>(matrix[1])*dx+static_cast<double>(matrix[5])*dy);
        direction[0]=static_cast<float>(static_cast<double>(matrix[8])*dz+static_cast<double>(matrix[4])*dy+static_cast<double>(dx)*matrix[0]);
        const double scale=psmInstanceB<float>(self,0xE4);
        const float inheritedX=static_cast<float>(scale*psmInstanceB<float>(self,0xD4));
        const float inheritedY=static_cast<float>(scale*psmInstanceB<float>(self,0xD8));
        const float velocityY=direction[1]*speed,velocityZ=direction[2]*speed;
        psmRuntimeField<float>(particle,0x8C)=static_cast<float>(velocityZ+scale*psmInstanceB<float>(self,0xDC));
        psmRuntimeField<float>(particle,0x84)=static_cast<float>(static_cast<double>(direction[0])*speed+inheritedX);
        psmRuntimeField<float>(particle,0x88)=velocityY+inheritedY;
    }
    else
    {
        // Local system: the offset stays local (the integrator transforms +0x50 each step).
        psmRuntimeField<float>(particle,0x8C)=direction[2]*speed;
        psmRuntimeField<float>(particle,0x84)=direction[0]*speed;
        psmRuntimeField<float>(particle,0x88)=direction[1]*speed;
    }
    float** const list=psmInstanceB<float**>(self,0x14);
    list[psmInstanceB<unsigned int>(self,0x28)++]=particle;
    psmInstanceB<float**>(self,0x14)[psmInstanceB<unsigned int>(self,0x28)]=0;
    return particle;
}

// PE 10184F00 (retn 4; the float argument is never read). Visibility timeout, lower-bounded
// coverage/rate scales, renderer over-budget throttle, and timed emission along the path from the
// previous to the current emitter position, each new particle catching up to "now".
void psmType1Emit(void* self,float /*dt*/)
{
    if(!psmInstanceB<unsigned char>(self,0x144))return;
    const unsigned int frame=dx9rend::rendererFrameOrdinal();
    if(psmInstanceB<unsigned int>(self,0x124)+60u<frame)
    {
        psmInstanceB<unsigned char>(self,0x21F)=psmInstanceB<unsigned char>(self,0x21E);
        psmInstanceB<unsigned char>(self,0x21E)=0;return;
    }
    void* const descriptor=psmInstanceB<void*>(self,0x2C);
    // Descriptor +0xE4 (0.05 from the type-1 defaults, no setter slot) floors both scales.
    if(psmInstanceB<float>(self,0x12C)<psmDescriptor1<float>(descriptor,0xE4))
        psmInstanceB<float>(self,0x12C)=psmDescriptor1<float>(descriptor,0xE4);
    if(psmInstanceB<float>(self,0x130)<psmDescriptor1<float>(descriptor,0xE4))
        psmInstanceB<float>(self,0x130)=psmDescriptor1<float>(descriptor,0xE4);
    double frequency=static_cast<double>(psmDescriptor1<float>(descriptor,0x10))
        *psmInstanceB<float>(self,0x12C)*psmInstanceB<float>(self,0x130);
    // Manager +0x7B48 = MeshParticlesRenderer +0x1C18, the over-budget byte its prepare (10174DA0)
    // writes last; descriptor +0x1A0 is the emit-priority scale (0.5 / 0.7 / 1.0) that the
    // descriptor's refreshDerivedState derives from +0x64. There is no minimum-frequency clamp here.
    if(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_meshParticlesRenderer[0x1C18])
        frequency*=psmDescriptor1<float>(descriptor,0x1A0);
    const float interval=static_cast<float>(1.0/frequency);
    if(psmInstanceB<unsigned char>(self,0x21C))
    {psmInstanceB<Vec3>(self,0xC0)=psmInstanceB<Vec3>(self,0xB4);psmInstanceB<unsigned char>(self,0x21C)=0;}
    IParticleSystemInstance* const instance=static_cast<IParticleSystemInstance*>(self);
    while(static_cast<double>(interval)+psmInstanceB<float>(self,0xF8)<=psmInstanceB<float>(self,0xF4))
    {
        if(psmInstanceB<float>(self,0xE8)!=0)
        {
            const double remaining=static_cast<double>(psmInstanceB<float>(self,0xE8))-interval;
            psmInstanceB<float>(self,0xE8)=static_cast<float>(remaining);
            if(remaining<=0)
            {
                instance->reserved04();
                if(psmDescriptor1<unsigned char>(psmInstanceB<void*>(self,0x2C),0x198))
                    instance->reserved03(psmInstanceB<unsigned char>(self,6)!=0);
                return;
            }
        }
        const float next=static_cast<float>(static_cast<double>(interval)+psmInstanceB<float>(self,0xF8));
        psmInstanceB<float>(self,0xF8)=next;
        if(psmInstanceB<unsigned char>(self,0x21E)
            &&psmInstanceB<unsigned int>(self,0x28)<psmInstanceB<unsigned int>(self,0x218))
        {
            float fraction=0;
            if(psmInstanceB<float>(self,0xF4)>psmInstanceB<float>(self,0xCC))
                fraction=static_cast<float>((static_cast<double>(next)-psmInstanceB<float>(self,0xCC))
                    /(static_cast<double>(psmInstanceB<float>(self,0xF4))-psmInstanceB<float>(self,0xCC)));
            // previous (+0xC0) towards current (+0xB4); Y keeps the unrounded product.
            const Vec3& from=psmInstanceB<Vec3>(self,0xC0);
            const Vec3& to=psmInstanceB<Vec3>(self,0xB4);
            const float zDifference=to.z-from.z;
            const float xScaled=static_cast<float>((static_cast<double>(to.x)-from.x)*fraction);
            const double yScaled=(static_cast<double>(to.y)-from.y)*fraction;
            const float zScaled=zDifference*fraction;
            psmInstanceB<Vec3>(self,0xA4)={xScaled+from.x,static_cast<float>(yScaled+from.y),zScaled+from.z};
            float* const particle=psmType1NewParticle(self,true);    // the inlined copy's sums
            if(particle)
            {
                // Added emission velocity (+0x1C, shared slot +0x90); no 0.001 snap on this type.
                psmRuntimeField<float>(particle,0x84)=psmInstanceB<float>(self,0x1C)+psmRuntimeField<float>(particle,0x84);
                psmRuntimeField<float>(particle,0x88)=psmInstanceB<float>(self,0x20)+psmRuntimeField<float>(particle,0x88);
                psmRuntimeField<float>(particle,0x8C)=psmInstanceB<float>(self,0x24)+psmRuntimeField<float>(particle,0x8C);
                const unsigned int steps=static_cast<unsigned int>(static_cast<int>(
                    (static_cast<double>(psmInstanceB<float>(self,0xF4))-psmInstanceB<float>(self,0xF8))*100.0));
                if(psmType1AdvanceParticle(self,particle,static_cast<int>(psmInstanceB<unsigned int>(self,0x28)-1u),
                    psmType1FixedStep,steps))
                {
                    const float remainder=static_cast<float>((static_cast<double>(psmInstanceB<float>(self,0xF4))
                        -psmInstanceB<float>(self,0xF8))-static_cast<double>(steps)*psmType1FixedStep);
                    psmType1AdvanceParticle(self,particle,static_cast<int>(psmInstanceB<unsigned int>(self,0x28)-1u),
                        remainder,1);
                }
            }
        }
    }
}
