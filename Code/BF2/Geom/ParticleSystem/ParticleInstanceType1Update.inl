// Mesh-particle integration uses 0.01-second steps and reevaluates gravity each step.
// Collision history rotates unless the clock is strictly below one third; NaN rotates it.
void psmType1RememberPlane(void* self)
{
    if (psmInstanceB<float>(self,0x214)<0.33333001f) return;
    const unsigned char next=psmInstanceB<unsigned char>(self,0x211);
    psmInstanceB<unsigned char>(self,0x210)=next;
    psmInstanceB<float>(self,0x214)=static_cast<float>(
        static_cast<double>(psmInstanceB<float>(self,0x214))-0.33333001f);
    const float* const source=&psmInstanceB<float>(self,0x200);
    const float distance=source[3];
    float* const target=&psmInstanceB<float>(self,0x160+16u*next);
    target[0]=source[0]; target[1]=source[1]; target[2]=source[2];
    psmNormalize3(target);
    target[3]=distance;
    psmInstanceB<unsigned char>(self,0x211)=
        static_cast<unsigned char>((psmInstanceB<unsigned char>(self,0x211)+1)%10);
}

// PE 10184270, __thiscall (particle, index, step, steps), retn 10h. Returns 0 when the particle
// expired and was handed back to the pool (the caller then re-reads the same index), else 1.
int psmType1AdvanceParticle(void* self,float* particle,int index,float step,unsigned int steps)
{
    // fild + the 2^32 fix-up is the unsigned conversion. Compared, not stored.
    // Dead only for an ordered `age >= lifetime`; a NaN age keeps the particle alive.
    const double tentativeAge=static_cast<double>(steps)*step+psmRuntimeField<float>(particle,0x90);
    if (tentativeAge>=psmRuntimeField<float>(particle,0x94))
    {
        float** const array=psmInstanceB<float**>(self,0x14);
        const unsigned int last=--psmInstanceB<unsigned int>(self,0x28);
        array[index]=array[last];
        psmInstanceB<float**>(self,0x14)[psmInstanceB<unsigned int>(self,0x28)]=0;
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        psmRecordLink(particle, 1)=manager->m_type1FreeList;
        manager->m_type1FreeList=particle;
        ++psmInstanceB<unsigned int>(self,0x140);
        return 0;
    }

    void* descriptor=psmInstanceB<void*>(self,0x2C);
    float windX=0, windY=0, windZ=0;
    if (!psmDescriptor1<unsigned char>(descriptor,0x1F1) && psmDescriptor1<float>(descriptor,0xCC)>0)
    {
        ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
        const Vec3 wind=reinterpret_cast<world::WindManager*>(manager->m_windManager)->getWind(
            psmRuntimeField<Vec3>(particle,0x44));
        windX=wind.x; windY=wind.y; windZ=wind.z;
    }
    descriptor=psmInstanceB<void*>(self,0x2C);
    // 10184341..1018436A scale even the zero (no-wind) vector: a NaN or infinite affection
    // factor still turns the wind terms into NaN here, unlike the sprite integrator.
    const float affection=psmDescriptor1<float>(descriptor,0xCC);
    const float windXScaled=static_cast<float>(static_cast<double>(affection)*windX);
    const float windYScaled=static_cast<float>(static_cast<double>(windY)*affection);
    const float windZScaled=static_cast<float>(static_cast<double>(windZ)*affection);
    const float gravityStep=static_cast<float>(static_cast<double>(step)*psmDescriptor1<float>(descriptor,0xA4));
    // Air-resistance graph (+0x11C t^3, +0x120 t^2, +0x124 t, +0x128 constant) at the particle's
    // current age fraction, once per call. t^2/t^3 stay unrounded on the x87 stack.
    const double t=psmRuntimeField<float>(particle,0x5C);
    const float dragCurve=static_cast<float>(t*t*psmDescriptor1<float>(descriptor,0x120)
        +t*psmDescriptor1<float>(descriptor,0x124)+t*t*t*psmDescriptor1<float>(descriptor,0x11C)
        +psmDescriptor1<float>(descriptor,0x128));
    const float drag=static_cast<float>(static_cast<double>(psmEmissionClamp(dragCurve))
        *psmDescriptor1<float>(psmInstanceB<void*>(self,0x2C),0xA8)*step);

    for (unsigned int remaining=steps;remaining!=0;--remaining)
    {
        // FST keeps the unrounded age and age fraction for the gravity graph below.
        const double age=static_cast<double>(step)+psmRuntimeField<float>(particle,0x90);
        psmRuntimeField<float>(particle,0x90)=static_cast<float>(age);
        const double fraction=age*psmRuntimeField<float>(particle,0x98);
        psmRuntimeField<float>(particle,0x5C)=static_cast<float>(fraction);
        // Gravity graph (+0x10C t^3, +0x110 t^2, +0x114 t, +0x118 constant), descriptor re-read.
        void* const graphOwner=psmInstanceB<void*>(self,0x2C);
        const float gravityCurve=static_cast<float>(fraction*fraction*psmDescriptor1<float>(graphOwner,0x110)
            +fraction*psmDescriptor1<float>(graphOwner,0x114)
            +fraction*fraction*fraction*psmDescriptor1<float>(graphOwner,0x10C)
            +psmDescriptor1<float>(graphOwner,0x118));
        const float gravity=static_cast<float>(static_cast<double>(psmEmissionClamp(gravityCurve))*gravityStep);
        float relative[3]={
            static_cast<float>(static_cast<double>(psmRuntimeField<float>(particle,0x84))-windXScaled),
            static_cast<float>(static_cast<double>(psmRuntimeField<float>(particle,0x88))-windYScaled),
            static_cast<float>(static_cast<double>(psmRuntimeField<float>(particle,0x8C))-windZScaled)};
        const float speedSquared=static_cast<float>(static_cast<double>(relative[0])*relative[0]
            +static_cast<double>(relative[2])*relative[2]+static_cast<double>(relative[1])*relative[1]);
        psmNormalize3(relative);
        const double negative=-static_cast<double>(speedSquared);
        const float forceX=static_cast<float>(relative[0]*negative);
        const float forceY=static_cast<float>(relative[1]*negative);
        const float forceZ=static_cast<float>(relative[2]*negative);
        const float accelX=static_cast<float>(static_cast<double>(forceX)*drag);
        const float accelY=static_cast<float>(static_cast<double>(
            static_cast<float>(static_cast<double>(forceY)*drag))+gravity);
        const double accelZ=static_cast<double>(forceZ)*drag;
        const float halfZ=static_cast<float>(accelZ*0.5);
        const float midX=static_cast<float>(static_cast<double>(accelX)*0.5+psmRuntimeField<float>(particle,0x84));
        const double midY=static_cast<double>(accelY)*0.5+psmRuntimeField<float>(particle,0x88);
        const double midZ=static_cast<double>(halfZ)+psmRuntimeField<float>(particle,0x8C);
        const float moveX=static_cast<float>(static_cast<double>(midX)*step);
        const float moveY=static_cast<float>(midY*step);
        const double moveZ=midZ*step;
        psmRuntimeField<float>(particle,0x44)=static_cast<float>(static_cast<double>(moveX)+psmRuntimeField<float>(particle,0x44));
        psmRuntimeField<float>(particle,0x48)=static_cast<float>(static_cast<double>(moveY)+psmRuntimeField<float>(particle,0x48));
        psmRuntimeField<float>(particle,0x4C)=static_cast<float>(moveZ+psmRuntimeField<float>(particle,0x4C));
        psmRuntimeField<float>(particle,0x84)=static_cast<float>(static_cast<double>(accelX)+psmRuntimeField<float>(particle,0x84));
        psmRuntimeField<float>(particle,0x88)=static_cast<float>(static_cast<double>(accelY)+psmRuntimeField<float>(particle,0x88));
        psmRuntimeField<float>(particle,0x8C)=static_cast<float>(accelZ+psmRuntimeField<float>(particle,0x8C));
    }

    descriptor=psmInstanceB<void*>(self,0x2C);
    if (psmDescriptor1<unsigned char>(descriptor,0xEC))
    {
        // 10184551..101845CB. `test ah,1` routes an unordered difference into the negative arm,
        // where fistp yields 0x80000000; the index arithmetic is 32-bit wraparound throughout,
        // including the address (+0x16)<<4. Retail has no range check on the resulting plane.
        const unsigned char current=psmInstanceB<unsigned char>(self,0x210);
        const double difference=static_cast<double>(psmInstanceB<float>(self,0x214))
            -psmRuntimeField<float>(particle,0x90);
        unsigned int selected=current;
        if (!(difference>=0))
        {
            selected=current-static_cast<unsigned int>(static_cast<int>(-3.0*difference))-1u;
            if (static_cast<int>(selected)<0)
                selected+=10u*((0xFFFFFFFFu-selected)/10u)+10u;
        }
        const float* const plane=selected==current
            ? &psmInstanceB<float>(self,0x200)
            : reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(self)+((selected+0x16u)<<4));
        const double velocityDot=static_cast<double>(psmRuntimeField<float>(particle,0x8C))*plane[2]
            +static_cast<double>(psmRuntimeField<float>(particle,0x88))*plane[1]
            +static_cast<double>(plane[0])*psmRuntimeField<float>(particle,0x84);
        if (velocityDot<0)
        {
            const double distance=static_cast<double>(psmRuntimeField<float>(particle,0x4C))*plane[2]
                +static_cast<double>(psmRuntimeField<float>(particle,0x48))*plane[1]
                +static_cast<double>(plane[0])*psmRuntimeField<float>(particle,0x44)+plane[3];
            if (distance<0)
            {
                const float correctionZ=static_cast<float>(distance*plane[2]);
                psmRuntimeField<float>(particle,0x44)=static_cast<float>(psmRuntimeField<float>(particle,0x44)-distance*plane[0]);
                psmRuntimeField<float>(particle,0x48)=static_cast<float>(psmRuntimeField<float>(particle,0x48)-distance*plane[1]);
                psmRuntimeField<float>(particle,0x4C)=static_cast<float>(
                    static_cast<double>(psmRuntimeField<float>(particle,0x4C))-correctionZ);
                const double reflected=static_cast<double>(psmRuntimeField<float>(particle,0x8C))*plane[2]
                    +static_cast<double>(psmRuntimeField<float>(particle,0x88))*plane[1]
                    +static_cast<double>(plane[0])*psmRuntimeField<float>(particle,0x84);
                if (reflected<0)
                {
                    void* const collision=psmInstanceB<void*>(self,0x2C);
                    const float normalFactor=psmDescriptor1<float>(collision,0xF0);
                    const double normalX=reflected*plane[0], normalY=reflected*plane[1];
                    const float normalZ=static_cast<float>(reflected*plane[2]);
                    const float tangentX=static_cast<float>(psmRuntimeField<float>(particle,0x84)-normalX);
                    const float tangentY=static_cast<float>(psmRuntimeField<float>(particle,0x88)-normalY);
                    const float tangentZ=static_cast<float>(static_cast<double>(psmRuntimeField<float>(particle,0x8C))-normalZ);
                    const float bounceX=static_cast<float>(normalX*normalFactor);
                    const float bounceY=static_cast<float>(normalY*normalFactor);
                    const double bounceZ=static_cast<double>(normalZ)*normalFactor;
                    const float along=psmDescriptor1<float>(collision,0xF4);
                    const float slideX=static_cast<float>(static_cast<double>(tangentX)*along);
                    const float slideY=static_cast<float>(static_cast<double>(tangentY)*along);
                    const float slideZ=static_cast<float>(static_cast<double>(tangentZ)*along);
                    psmRuntimeField<float>(particle,0x8C)=static_cast<float>(static_cast<double>(slideZ)-bounceZ);
                    psmRuntimeField<float>(particle,0x84)=static_cast<float>(static_cast<double>(slideX)-bounceX);
                    psmRuntimeField<float>(particle,0x88)=static_cast<float>(static_cast<double>(slideY)-bounceY);
                }
            }
        }
    }

    // flag the renderer's per-particle draw state (10175860 clears bit 7) and publish
    // the world position.
    reinterpret_cast<unsigned char*>(particle)[0x60]|=0x80;
    psmRuntimeField<Vec3>(particle,0x50)=psmRuntimeField<Vec3>(particle,0x44);
    if (psmDescriptor1<unsigned char>(psmInstanceB<void*>(self,0x2C),0x1F1))
    {
        const float* const m=&psmInstanceB<float>(self,0x74);
        const float x=psmRuntimeField<float>(particle,0x50);
        const float y=psmRuntimeField<float>(particle,0x54);
        const float z=psmRuntimeField<float>(particle,0x58);
        const double worldZ=static_cast<double>(m[10])*z+static_cast<double>(m[2])*x+static_cast<double>(m[6])*y+m[14];
        const double worldY=static_cast<double>(m[9])*z+static_cast<double>(m[1])*x+static_cast<double>(m[5])*y+m[13];
        const double worldX=static_cast<double>(m[8])*z+static_cast<double>(m[4])*y+static_cast<double>(x)*m[0]+m[12];
        psmRuntimeField<float>(particle,0x50)=static_cast<float>(worldX);
        psmRuntimeField<float>(particle,0x54)=static_cast<float>(worldY);
        psmRuntimeField<float>(particle,0x58)=static_cast<float>(worldZ);
    }
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    if (psmDescriptor1<unsigned char>(psmInstanceB<void*>(self,0x2C),0xD0) && manager->m_heightmapCluster)
        psmRuntimeField<float>(particle,0x54)=static_cast<float>(static_cast<double>(
            reinterpret_cast<world::IHeightmapCluster*>(manager->m_heightmapCluster)
                ->getWaterLevelInWorldCoords(psmRuntimeField<Vec3>(particle,0x50)))
            +psmDescriptor1<float>(psmInstanceB<void*>(self,0x2C),0xD4));
    // ordered extrema, x max/min, then y, then z; NaN updates nothing.
    float* const bounds=&psmInstanceB<float>(self,0xFC);
    for (int i=0;i<3;++i)
    {
        const float value=psmRuntimeField<float>(particle,0x50+4u*i);
        if (value>bounds[3+i]) bounds[3+i]=value;
        if (value<bounds[i]) bounds[i]=value;
    }
    return 1;
}

// PE 10185D60 (table +0xA4).
void psmType1Update(void* self,float dt)
{
    IParticleSystemInstance* const instance=static_cast<IParticleSystemInstance*>(self);
    float elapsed=static_cast<float>(static_cast<double>(dt)*psmInstanceB<float>(self,0x128));
    psmDescriptor1<int>(psmInstanceB<void*>(self,0x2C),8)=-1;
    if (!psmInstanceB<unsigned int>(self,0x28) && !psmInstanceB<unsigned char>(self,0x144))
    {
        psmQueueInactive(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager),instance);
        if (psmInstanceB<unsigned char>(self,0x18))
            reinterpret_cast<IParticleSystemManager*>(g_ParticleSystemManager)->removeInstance(instance);
        return;
    }
    // The refusal tests the RAW dt (like type 2), and a NaN dt is not refused (AH&41 parity).
    if (psmInstanceB<unsigned char>(self,0x21D) || dt<=0) return;
    void* descriptor=psmInstanceB<void*>(self,0x2C);
    if (psmDescriptor1<unsigned char>(descriptor,0xEC))
    {
        // The history clock advances by the unclamped scaled time.
        psmInstanceB<float>(self,0x214)=static_cast<float>(
            static_cast<double>(elapsed)+psmInstanceB<float>(self,0x214));
        psmType1RememberPlane(self);
    }
    if (!psmInstanceB<unsigned char>(self,0x21F) && psmInstanceB<unsigned char>(self,0x21E))
        psmInstanceB<unsigned char>(self,0x21F)=psmInstanceB<unsigned char>(self,0x21E);
    descriptor=psmInstanceB<void*>(self,0x2C);
    psmInstanceB<unsigned int>(self,0x140)=0; psmInstanceB<unsigned int>(self,0x13C)=0;
    const double maximum=static_cast<double>(psmDescriptor1<float>(descriptor,0x9C))+psmDescriptor1<float>(descriptor,0x98);
    if (elapsed>maximum) elapsed=static_cast<float>(maximum);
    for (int i=0;i<3;++i)
    {
        psmInstanceB<unsigned int>(self,0xFC+4*i)=0x4B18967F;   //  9999999.0f
        psmInstanceB<unsigned int>(self,0x108+4*i)=0xCB18967F;  // -9999999.0f
    }
    psmInstanceB<Vec3>(self,0xC0)=psmInstanceB<Vec3>(self,0xA4);
    psmType1RefreshTransform(self);
    psmInstanceB<Vec3>(self,0xB4)=psmInstanceB<Vec3>(self,0xA4);
    const double total=static_cast<double>(elapsed)+psmInstanceB<float>(self,0xD0);
    const unsigned int steps=static_cast<unsigned int>(static_cast<int>(100.0*total));
    const double advanced=static_cast<double>(steps)*psmType1FixedStep;
    psmInstanceB<float>(self,0xCC)=psmInstanceB<float>(self,0xF4);
    psmInstanceB<float>(self,0xD0)=static_cast<float>(total-advanced);
    psmInstanceB<float>(self,0xF4)=static_cast<float>(advanced+psmInstanceB<float>(self,0xF4));
    for (unsigned int i=0;psmInstanceB<float**>(self,0x14)[i];)
        i+=psmType1AdvanceParticle(self,psmInstanceB<float**>(self,0x14)[i],static_cast<int>(i),
            psmType1FixedStep,steps);
    const double delay=static_cast<double>(psmInstanceB<float>(self,0xF0))-elapsed;
    psmInstanceB<float>(self,0xF0)=static_cast<float>(delay);   // FST: the test keeps x87 precision
    if (delay>0) { psmInstanceB<float>(self,0xF8)=psmInstanceB<float>(self,0xF4); return; }
    // replay the armed prewarm in 1/60 s virtual updates (the flag word is
    // cleared first, so the recursion cannot re-enter this arm).
    if (psmInstanceB<float>(self,0xEC)>0)
    {
        float remaining=psmInstanceB<float>(self,0xEC);
        psmInstanceB<float>(self,0xEC)=0;
        if (remaining>0)
            for (;;)
            {
                instance->reserved41(remaining<0.016666668f ? remaining : 0.016666668f);
                const double next=static_cast<double>(remaining)-0.016666668f;
                remaining=static_cast<float>(next);       // FST keeps the unrounded value for the test
                if (!(next>0)) break;
            }
    }
    psmType1Emit(self,elapsed);
    psmInstanceB<Vec3>(self,0xA4)=psmInstanceB<Vec3>(self,0xB4);
    float* const bounds=&psmInstanceB<float>(self,0xFC);
    if (psmInstanceB<unsigned int>(self,0x28))
    {
        // Every term re-reads the descriptor pointer, as 10186041..101860B1 do.
        for (int i=0;i<3;++i)
            bounds[i]=static_cast<float>(static_cast<double>(bounds[i])
                -psmDescriptor1<float>(psmInstanceB<void*>(self,0x2C),0x1E8));
        for (int i=3;i<6;++i)
            bounds[i]=static_cast<float>(static_cast<double>(psmDescriptor1<float>(
                psmInstanceB<void*>(self,0x2C),0x1E8))+bounds[i]);
    }
    else
    {
        psmInstanceB<Vec3>(self,0xFC)=psmInstanceB<Vec3>(self,0xA4);
        psmInstanceB<Vec3>(self,0x108)=psmInstanceB<Vec3>(self,0xA4);
    }
    // PE 1016EB70 (radius) then 1016EBC0 (centre), in that order.
    const float deltaZ=bounds[5]-bounds[2];
    const float halfX=static_cast<float>((static_cast<double>(bounds[3])-bounds[0])*0.5);
    const double halfY=(static_cast<double>(bounds[4])-bounds[1])*0.5;
    psmInstanceB<float>(self,0x120)=static_cast<float>(::sqrt(deltaZ*0.5*(deltaZ*0.5)
        +static_cast<double>(halfX)*halfX+halfY*halfY));
    const float sumZ=bounds[5]+bounds[2];
    psmInstanceB<Vec3>(self,0x114)={static_cast<float>((static_cast<double>(bounds[3])+bounds[0])*0.5),
        static_cast<float>((static_cast<double>(bounds[4])+bounds[1])*0.5),sumZ*0.5f};
}
