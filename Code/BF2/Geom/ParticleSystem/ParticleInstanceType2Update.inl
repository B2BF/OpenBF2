// [CHOSEN, not recovered] private TrailSystem update split. Full BF2 RendDX9
// PE1017A9D0/1017BC70 and bounds/transform/floor dependencies read; no dedicated ELF body.
void psmType2RefreshTransform(void*);
void psmType2Emit(void*,float);

// PE1017A9D0: expiry always removes the oldest ring entry; the supplied index is unused.
int psmType2AdvanceParticle(void* self,float* particle,int /*index*/,float step,unsigned int steps)
{
    const double age=static_cast<double>(steps)*step+particle[38];
    particle[38]=static_cast<float>(age);
    if(age>=particle[39])
    {
        float** const ring=psmInstanceB<float**>(self,0x14);
        unsigned int& head=psmInstanceB<unsigned int>(self,0x178);
        float* const retired=ring[head];ring[head]=nullptr;
        --psmInstanceB<unsigned int>(self,0x28);
        head=(head+1)&psmInstanceB<unsigned int>(self,0x17C);
        if(ring[head])ring[head][34]=0;
        psmRecordLink(retired, 2)=
            reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_type2FreeList;   // +0x85E0
        reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_type2FreeList=retired;
        ++psmInstanceB<unsigned int>(self,0x13C);
        return 0;
    }
    // Unlike ordinary/NSA particles, Trail samples the global speed and direction unconditionally.
    const float windSpeed=reinterpret_cast<world::WindManager*>(
        reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_windManager)->getGlobalWindSpeed();
    const Vec3& direction=reinterpret_cast<world::WindManager*>(
        reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_windManager)->getGlobalWindDirection();
    void* descriptor=psmInstanceB<void*>(self,0x2C);
    const float affection=psmDescriptor2<float>(descriptor,0x88);
    const float windZ=windSpeed*direction.z;
    const float wx=static_cast<float>(static_cast<double>(windSpeed)*direction.x*affection);
    const float wy=static_cast<float>(static_cast<double>(windSpeed)*direction.y*affection);
    const float wz=windZ*affection;
    const float gravityStep=step*psmDescriptor2<float>(descriptor,0x78);
    const float drag=step*psmDescriptor2<float>(descriptor,0x7C);
    const double t=particle[30];
    const float curve=static_cast<float>(t*t*psmDescriptor2<float>(descriptor,0x130)
        +t*psmDescriptor2<float>(descriptor,0x134)+t*t*t*psmDescriptor2<float>(descriptor,0x12C)
        +psmDescriptor2<float>(descriptor,0x138));
    const float gravity=psmType0Clamp(curve)*gravityStep;
    particle[30]=particle[40]*particle[38];
    while(steps--)
    {
        const float dx=particle[35]-wx,dz=particle[37]-wz;
        const double dy=static_cast<double>(particle[36])-wy;
        const double squared=dy*dy+static_cast<double>(dz)*dz+static_cast<double>(dx)*dx;
        const float speedSquared=static_cast<float>(squared);
        double ax=0,ay=0;float az=0;
        if(::fabs(squared)>=1.1920929e-7f)
        {
            const double negative=-static_cast<double>(speedSquared);
            const float vy=static_cast<float>(dy*negative),vz=static_cast<float>(dz*negative);
            const float fy=vy*drag,fz=vz*drag;
            const float inverse=static_cast<float>(1.0/::sqrt(speedSquared));
            ax=dx*negative*drag*inverse;ay=static_cast<double>(fy)*inverse;az=fz*inverse;
        }
        ay+=gravity;
        const float halfZ=static_cast<float>(az*0.5);
        const float vx=static_cast<float>(ax*0.5+particle[35]);
        const double vy=ay*0.5+particle[36],vz=static_cast<double>(halfZ)+particle[37];
        const float px=vx*step,py=static_cast<float>(vy*step);
        particle[1]=px+particle[1];particle[2]=py+particle[2];particle[3]=static_cast<float>(vz*step+particle[3]);
        particle[35]=static_cast<float>(ax+particle[35]);particle[36]=static_cast<float>(ay+particle[36]);particle[37]=az+particle[37];
    }
    for(int i=35;i<=37;++i)if(::fabs(particle[i])<0.001f)particle[i]=0;
    particle[4]=particle[1];particle[5]=particle[2];particle[6]=particle[3];
    if(psmDescriptor2<unsigned char>(psmInstanceB<void*>(self,0x2C),0x1C0))
    {
        const float* m=psmInstanceB<Mat4>(self,0x74).m;
        const float x=particle[4],y=particle[5],z=particle[6];
        const double rz=static_cast<double>(m[2])*x+static_cast<double>(m[10])*z+static_cast<double>(m[6])*y+m[14];
        const double ry=static_cast<double>(m[1])*x+static_cast<double>(m[9])*z+static_cast<double>(m[5])*y+m[13];
        particle[4]=static_cast<float>(static_cast<double>(m[8])*z+static_cast<double>(m[4])*y+static_cast<double>(m[0])*x+m[12]);
        particle[5]=static_cast<float>(ry);particle[6]=static_cast<float>(rz);
    }
    if(psmDescriptor2<unsigned char>(psmInstanceB<void*>(self,0x2C),0x8C)
        &&reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_heightmapCluster)
    {
        const double water=reinterpret_cast<world::IHeightmapCluster*>(
            reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_heightmapCluster)
            ->getWaterLevelInWorldCoords(*reinterpret_cast<Vec3*>(particle+4));
        particle[5]=static_cast<float>(water+psmDescriptor2<float>(psmInstanceB<void*>(self,0x2C),0x90)+0.1f);
    }
    // PE1017A2B0 includes ordered extrema, preserving NaN's no-update behavior.
    float* bounds=&psmInstanceB<float>(self,0xF8);
    for(int i=0;i<3;++i){if(particle[4+i]>bounds[3+i])bounds[3+i]=particle[4+i];if(particle[4+i]<bounds[i])bounds[i]=particle[4+i];}
    return 1;
}

// PE1017BC70: source dt controls refusal; the separately scaled/clamped dt drives simulation.
void psmType2Update(void* self,float dt)
{
    float elapsed=dt*psmInstanceB<float>(self,0x124);
    psmDescriptor2<int>(psmInstanceB<void*>(self,0x2C),8)=-1;
    IParticleSystemInstance* instance=static_cast<IParticleSystemInstance*>(self);
    if(!psmInstanceB<unsigned int>(self,0x28)&&!psmInstanceB<unsigned char>(self,0x140))
    {
        psmQueueInactive(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager),instance);
        if(psmInstanceB<unsigned char>(self,0x18))reinterpret_cast<IParticleSystemManager*>(g_ParticleSystemManager)->removeInstance(instance);
        return;
    }
    if(psmInstanceB<unsigned char>(self,0x155)||dt<=0)return;
    if(!psmInstanceB<unsigned char>(self,0x157)&&psmInstanceB<unsigned char>(self,0x156))
        psmInstanceB<unsigned char>(self,0x157)=psmInstanceB<unsigned char>(self,0x156);
    psmInstanceB<unsigned int>(self,0x13C)=0;psmInstanceB<unsigned int>(self,0x138)=0;
    void* descriptor=psmInstanceB<void*>(self,0x2C);
    if(elapsed>psmDescriptor2<float>(descriptor,0x70))elapsed=psmDescriptor2<float>(descriptor,0x70);
    for(int i=0;i<3;++i){psmInstanceB<unsigned int>(self,0xF8+4*i)=0x4B18967F;psmInstanceB<unsigned int>(self,0x104+4*i)=0xCB18967F;}
    psmInstanceB<Vec3>(self,0xC0)=psmInstanceB<Vec3>(self,0xA4);
    psmType2RefreshTransform(self);
    psmInstanceB<Vec3>(self,0xB4)=psmInstanceB<Vec3>(self,0xA4);
    const double total=static_cast<double>(elapsed)+psmInstanceB<float>(self,0xD0);
    const unsigned int steps=static_cast<unsigned int>(static_cast<int>(100.0*total));
    const double advanced=static_cast<double>(steps)*0.01f;
    psmInstanceB<float>(self,0xCC)=psmInstanceB<float>(self,0xF0);
    psmInstanceB<float>(self,0xD0)=static_cast<float>(total-advanced);
    psmInstanceB<float>(self,0xF0)=static_cast<float>(advanced+psmInstanceB<float>(self,0xF0));
    for(unsigned int i=psmInstanceB<unsigned int>(self,0x178)&psmInstanceB<unsigned int>(self,0x17C);
        i!=psmInstanceB<unsigned int>(self,0x174);i=(i+1)&psmInstanceB<unsigned int>(self,0x17C))
        psmType2AdvanceParticle(self,psmInstanceB<float**>(self,0x14)[i],i,0.01f,steps);
    const double delay=static_cast<double>(psmInstanceB<float>(self,0xEC))-elapsed;
    psmInstanceB<float>(self,0xEC)=static_cast<float>(delay);
    if(delay>0){psmInstanceB<float>(self,0xF4)=psmInstanceB<float>(self,0xF0);return;}
    psmType2Emit(self,elapsed);
    psmInstanceB<Vec3>(self,0xA4)=psmInstanceB<Vec3>(self,0xB4);
    float* bounds=&psmInstanceB<float>(self,0xF8);
    if(psmInstanceB<unsigned int>(self,0x28))
    {
        for(int i=0;i<3;++i)bounds[i]-=psmDescriptor2<float>(psmInstanceB<void*>(self,0x2C),0x1B8);
        for(int i=3;i<6;++i)bounds[i]+=psmDescriptor2<float>(psmInstanceB<void*>(self,0x2C),0x1B8);
    }
    else{psmInstanceB<Vec3>(self,0xF8)=psmInstanceB<Vec3>(self,0xA4);psmInstanceB<Vec3>(self,0x104)=psmInstanceB<Vec3>(self,0xA4);}
    const float dz=bounds[5]-bounds[2],dx=static_cast<float>((static_cast<double>(bounds[3])-bounds[0])*0.5);
    const double dy=(static_cast<double>(bounds[4])-bounds[1])*0.5;
    psmInstanceB<float>(self,0x11C)=static_cast<float>(::sqrt(dz*0.5*(dz*0.5)+static_cast<double>(dx)*dx+dy*dy));
    const float zsum=bounds[5]+bounds[2];
    psmInstanceB<Vec3>(self,0x110)={static_cast<float>((static_cast<double>(bounds[3])+bounds[0])*0.5),static_cast<float>((static_cast<double>(bounds[4])+bounds[1])*0.5),zsum*0.5f};
    descriptor=psmInstanceB<void*>(self,0x2C);
    if(psmDescriptor2<unsigned char>(descriptor,0x1C8)&&psmDescriptor2<unsigned int>(descriptor,0x1D4))
    {
        const float time=static_cast<float>(static_cast<double>(elapsed)*psmDescriptor2<float>(descriptor,0x1C4)+psmInstanceB<float>(self,0x190));
        psmInstanceB<float>(self,0x190)=time;
        const unsigned int frame=static_cast<unsigned int>(static_cast<int>(::floor(time)));
        psmInstanceB<float>(self,0x18C)=static_cast<float>(static_cast<double>(time)-frame);
        descriptor=psmInstanceB<void*>(self,0x2C);
        const unsigned int current=frame%psmDescriptor2<unsigned int>(descriptor,0x1D4);
        if(current!=psmInstanceB<unsigned int>(self,0x188))
        {
            psmInstanceB<unsigned int>(self,0x188)=current;
            const unsigned int next=(current+1)%psmDescriptor2<unsigned int>(descriptor,0x1D4);
            const unsigned int width=psmDescriptor2<unsigned int>(descriptor,0x1D8);
            const unsigned int cx=current%width,nx=next%width;
            const double cy=static_cast<double>(current-cx)*psmDescriptor2<float>(descriptor,0x1DC);
            const float ny=static_cast<float>(static_cast<double>(next-nx)*psmDescriptor2<float>(descriptor,0x1DC));
            const double oneOverShort=0.000030518509447574615;
            const double cu=(static_cast<double>(cx)*psmDescriptor2<float>(descriptor,0xAC)
                +psmDescriptor2<short>(descriptor,0x1E0)*oneOverShort)*32767.0;
            psmInstanceB<short>(self,0x180)=static_cast<short>(static_cast<__int64>(cu));
            descriptor=psmInstanceB<void*>(self,0x2C);
            const double cv=(psmDescriptor2<short>(descriptor,0x1E2)*oneOverShort+cy*psmDescriptor2<float>(descriptor,0xB0))*32767.0;
            psmInstanceB<short>(self,0x182)=static_cast<short>(static_cast<__int64>(cv));
            descriptor=psmInstanceB<void*>(self,0x2C);
            const double nu=(static_cast<double>(nx)*psmDescriptor2<float>(descriptor,0xAC)
                +psmDescriptor2<short>(descriptor,0x1E0)*oneOverShort)*32767.0;
            psmInstanceB<short>(self,0x184)=static_cast<short>(static_cast<__int64>(nu));
            descriptor=psmInstanceB<void*>(self,0x2C);
            const double nv=(psmDescriptor2<short>(descriptor,0x1E2)*oneOverShort+static_cast<double>(ny)*psmDescriptor2<float>(descriptor,0xB0))*32767.0;
            psmInstanceB<short>(self,0x186)=static_cast<short>(static_cast<__int64>(nv));
        }
    }
    else
    {
        for(int i=0;i<4;++i)psmInstanceB<short>(self,0x180+2*i)=psmDescriptor2<short>(psmInstanceB<void*>(self,0x2C),0x1E0+2*(i%2));
    }
    if(psmInstanceB<unsigned int>(self,0x28)&&psmInstanceB<unsigned char>(self,0x140))
    {
        float* newest=psmInstanceB<float**>(self,0x14)[psmInstanceB<unsigned int>(self,0x194)];
        *reinterpret_cast<Vec3*>(newest+4)=psmInstanceB<Vec3>(self,0xA4);
    }
}
