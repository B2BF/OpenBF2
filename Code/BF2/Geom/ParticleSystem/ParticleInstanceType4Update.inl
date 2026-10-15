// [CHOSEN, not recovered] private split; full PE1017CBF0/1017DA40/1017E6A0 read.
// Type4 uses 200-byte particles, water-aligned oriented corners and a distinct descriptor layout.

void psmType4RememberPlane(void* self)
{
    if (psmInstanceA<float>(self,0x21C)>=0.33333001f)
    {
        const unsigned char next=psmInstanceA<unsigned char>(self,0x219);
        psmInstanceA<unsigned char>(self,0x218)=next;
        psmInstanceA<float>(self,0x21C)-=0.33333001f;
        float* target=&psmInstanceA<float>(self,0x168+16*next);
        const float* source=&psmInstanceA<float>(self,0x208);
        target[0]=source[0]; target[1]=source[1]; target[2]=source[2];
        const float distance=source[3]; psmNormalize3(target); target[3]=distance;
        psmInstanceA<unsigned char>(self,0x219)=(psmInstanceA<unsigned char>(self,0x219)+1)%10;
    }
}

int psmType4AdvanceParticle(void* self,float* particle,int index,float step,unsigned int steps)
{
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    const double age=static_cast<double>(steps)*step+particle[29];
    particle[29]=static_cast<float>(age);
    if (age>=particle[30])
    {
        float** const array=psmInstanceA<float**>(self,0x24);
        const unsigned int last=--psmInstanceA<unsigned int>(self,0x28);
        array[index]=array[last]; array[last]=0;
        psmRecordLink(particle, 4)=manager->m_type4FreeList;
        --manager->m_type4LiveCount; manager->m_type4FreeList=particle;
        ++psmInstanceA<int>(self,0x148);
        return 0;
    }
    void* descriptor=psmInstanceA<void*>(self,0x2C);
    Vec3 wind={0,0,0};
    // This PE does not consult manager.m_windEnabled; the descriptor decides whether to query.
    if (!psmDescriptor4<unsigned char>(descriptor,0x211) && psmDescriptor4<float>(descriptor,0x9C)>0)
        wind=reinterpret_cast<world::WindManager*>(manager->m_windManager)->getWind(*reinterpret_cast<Vec3*>(particle+1));
    descriptor=psmInstanceA<void*>(self,0x2C);
    const float affection=psmDescriptor4<float>(descriptor,0x9C);
    wind.x*=affection; wind.y*=affection; wind.z*=affection;
    const float gravityStep=step*psmDescriptor4<float>(descriptor,0x84);
    const double t=particle[9];
    const float dragCurve=static_cast<float>(t*psmDescriptor4<float>(descriptor,0x114)
        +t*t*psmDescriptor4<float>(descriptor,0x110)+t*t*t*psmDescriptor4<float>(descriptor,0x10C)
        +psmDescriptor4<float>(descriptor,0x118));
    const float drag=static_cast<float>(static_cast<double>(psmType0Clamp(dragCurve))*psmDescriptor4<float>(descriptor,0x88)*step);
    const float gravityCurve=static_cast<float>(t*psmDescriptor4<float>(descriptor,0x104)
        +t*t*psmDescriptor4<float>(descriptor,0x100)+t*t*t*psmDescriptor4<float>(descriptor,0xFC)
        +psmDescriptor4<float>(descriptor,0x108));
    const float gravity=psmType0Clamp(gravityCurve)*gravityStep;
    particle[9]=particle[31]*particle[29];
    while (steps--)
    {
        const float dx=particle[23]-wind.x, dy=particle[24]-wind.y, dz=particle[25]-wind.z;
        const double speedSquared=static_cast<double>(dx)*dx+static_cast<double>(dy)*dy+static_cast<double>(dz)*dz;
        const float speed=static_cast<float>(speedSquared);
        double ax=0,ay=0; float az=0;
        if (::fabs(speedSquared)>=1.1920929e-7f)
        {
            const double negative=-static_cast<double>(speed);
            const float vy=static_cast<float>(dy*negative), vz=static_cast<float>(dz*negative);
            const float fx=static_cast<float>(dx*negative*drag), fy=vy*drag, fz=vz*drag;
            const float inverse=static_cast<float>(1.0/::sqrt(speedSquared));
            ax=static_cast<double>(fx)*inverse; ay=static_cast<double>(fy)*inverse; az=fz*inverse;
        }
        ay+=gravity;
        const float halfZ=static_cast<float>(az*0.5);
        const float vx=static_cast<float>(ax*0.5+particle[23]);
        const double vy=ay*0.5+particle[24], vz=static_cast<double>(halfZ)+particle[25];
        const float px=vx*step, py=static_cast<float>(vy*step);
        particle[1]=px+particle[1]; particle[2]=py+particle[2];
        particle[3]=static_cast<float>(vz*step+particle[3]);
        particle[23]=static_cast<float>(ax+particle[23]); particle[24]=static_cast<float>(ay+particle[24]);
        particle[25]=az+particle[25];
        // NSA snaps after every substep, unlike the ordinary instance's post-loop snap.
        for (int i=23;i<=25;++i) if (::fabs(particle[i])<0.001f) particle[i]=0;
    }
    descriptor=psmInstanceA<void*>(self,0x2C);
    if (psmDescriptor4<unsigned char>(descriptor,0xDC))
    {
        const double difference=static_cast<double>(psmInstanceA<float>(self,0x21C))-particle[29];
        const int current=psmInstanceA<unsigned char>(self,0x218);
        int selected=current;
        if (difference<0)
        {
            selected-=static_cast<int>(-3.0*difference)+1;
            if (selected<0) selected+=10*((-1-selected)/10)+10;
        }
        const float* plane=&psmInstanceA<float>(self,selected==current?0x208:0x168+16*selected);
        double velocityDot=static_cast<double>(particle[25])*plane[2]+static_cast<double>(particle[24])*plane[1]+static_cast<double>(particle[23])*plane[0];
        if (velocityDot<0)
        {
            const double distance=static_cast<double>(particle[3])*plane[2]+static_cast<double>(particle[2])*plane[1]+static_cast<double>(particle[1])*plane[0]+plane[3];
            if (distance<0)
            {
                const float zDistance=static_cast<float>(distance*plane[2]);
                particle[1]=static_cast<float>(particle[1]-distance*plane[0]);
                particle[2]=static_cast<float>(particle[2]-distance*plane[1]); particle[3]-=zDistance;
                velocityDot=static_cast<double>(particle[25])*plane[2]+static_cast<double>(particle[24])*plane[1]+static_cast<double>(particle[23])*plane[0];
                if (velocityDot<0)
                {
                    const double nx=velocityDot*plane[0], ny=velocityDot*plane[1];
                    const float nz=static_cast<float>(velocityDot*plane[2]);
                    const float normal=psmDescriptor4<float>(descriptor,0xE0), along=psmDescriptor4<float>(descriptor,0xE4);
                    const float tx=static_cast<float>(particle[23]-nx), ty=static_cast<float>(particle[24]-ny), tz=particle[25]-nz;
                    const float rx=static_cast<float>(nx*normal), ry=static_cast<float>(ny*normal);
                    particle[23]=static_cast<float>(tx*along)-rx; particle[24]=static_cast<float>(ty*along)-ry;
                    particle[25]=static_cast<float>(static_cast<float>(tz*along)-static_cast<double>(nz)*normal);
                }
            }
        }
    }
    reinterpret_cast<unsigned char*>(particle)[0x28]|=0x80;
    particle[4]=particle[1]; particle[5]=particle[2]; particle[6]=particle[3];
    particle[26]=particle[23]; particle[27]=particle[24]; particle[28]=particle[25];
    if (psmDescriptor4<unsigned char>(psmInstanceA<void*>(self,0x2C),0x211))
    {
        const float* m=&psmInstanceA<float>(self,0x74);
        const double z=static_cast<double>(m[6])*particle[5]+static_cast<double>(m[2])*particle[4]+static_cast<double>(m[10])*particle[6]+m[14];
        const double y=static_cast<double>(m[5])*particle[5]+static_cast<double>(m[1])*particle[4]+static_cast<double>(m[9])*particle[6]+m[13];
        particle[4]=static_cast<float>(static_cast<double>(m[4])*particle[5]+static_cast<double>(m[8])*particle[6]+static_cast<double>(m[0])*particle[4]+m[12]);
        particle[5]=static_cast<float>(y); particle[6]=static_cast<float>(z);
        // PE1023A414..440, read as raw floats. Each oriented corner is transformed independently.
        static const float corners[4][3]={{-1,0,1},{1,0,1},{1,0,-1},{-1,0,-1}};
        for (int i=0;i<4;++i)
        {
            const float* const c=corners[i]; float* const out=particle+38+3*i;
            out[0]=static_cast<float>(static_cast<double>(c[1])*m[4]+static_cast<double>(c[2])*m[8]+static_cast<double>(c[0])*m[0]);
            out[1]=static_cast<float>(static_cast<double>(c[0])*m[1]+static_cast<double>(c[1])*m[5]+static_cast<double>(c[2])*m[9]);
            out[2]=static_cast<float>(static_cast<double>(c[0])*m[2]+static_cast<double>(c[1])*m[6]+static_cast<double>(c[2])*m[10]);
        }
        const double vz=static_cast<double>(m[10])*particle[28]+static_cast<double>(m[6])*particle[27]+static_cast<double>(m[2])*particle[26];
        const double vy=static_cast<double>(m[9])*particle[28]+static_cast<double>(m[5])*particle[27]+static_cast<double>(m[1])*particle[26];
        particle[26]=static_cast<float>(static_cast<double>(m[8])*particle[28]+static_cast<double>(m[4])*particle[27]+static_cast<double>(m[0])*particle[26]);
        particle[27]=static_cast<float>(vy); particle[28]=static_cast<float>(vz);
    }
    if (psmDescriptor4<unsigned char>(psmInstanceA<void*>(self,0x2C),0xA0) && manager->m_heightmapCluster)
        particle[5]=reinterpret_cast<world::IHeightmapCluster*>(manager->m_heightmapCluster)->getWaterLevelInWorldCoords(*reinterpret_cast<Vec3*>(particle+4))
            +psmDescriptor4<float>(psmInstanceA<void*>(self,0x2C),0xA4);
    float* bounds=&psmInstanceA<float>(self,0xFC);
    for (int i=0;i<3;++i)
    { if(particle[4+i]>bounds[3+i]) bounds[3+i]=particle[4+i]; if(particle[4+i]<bounds[i]) bounds[i]=particle[4+i]; }
    return 1;
}

void psmType4Update(void* self,float dt)
{
    IParticleSystemInstance* const instance=static_cast<IParticleSystemInstance*>(self);
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    const float scaled=dt*psmInstanceA<float>(self,0x12C);
    void* descriptor=psmInstanceA<void*>(self,0x2C);
    psmDescriptor4<int>(descriptor,8)=-1;
    float elapsed=scaled;
    if (!psmInstanceA<unsigned int>(self,0x28) && !psmInstanceA<unsigned char>(self,0x14C))
    {
        psmQueueInactive(manager,instance);
        if (psmInstanceA<unsigned char>(self,0x14))
            reinterpret_cast<IParticleSystemManager*>(manager)->removeInstance(instance);
        return;
    }
    if (psmInstanceA<unsigned char>(self,0x225) || scaled<=0) return;
    if (!psmInstanceA<unsigned char>(self,0x129) && psmInstanceA<unsigned char>(self,0x128))
    {
        psmInstanceA<unsigned char>(self,0x129)=psmInstanceA<unsigned char>(self,0x128);
        if (psmDescriptor4<unsigned char>(descriptor,0x25D))
            for (float time=psmDescriptor4<float>(descriptor,0x7C);time>0;time-=0.1f)
                instance->reserved41(time>=0.1f?0.1f:time);
    }
    descriptor=psmInstanceA<void*>(self,0x2C);
    if (psmDescriptor4<unsigned char>(descriptor,0xDC))
    { psmInstanceA<float>(self,0x21C)+=scaled; psmType4RememberPlane(self); }
    psmInstanceA<unsigned int>(self,0x148)=0; psmInstanceA<unsigned int>(self,0x144)=0;
    const double maximum=static_cast<double>(psmDescriptor4<float>(descriptor,0x7C))+psmDescriptor4<float>(descriptor,0x78);
    if (scaled>maximum) elapsed=static_cast<float>(maximum);
    for (int i=0;i<3;++i) { psmInstanceA<unsigned int>(self,0xFC+i*4)=0x4B18967F; psmInstanceA<unsigned int>(self,0x108+i*4)=0xCB18967F; }
    psmInstanceA<Vec3>(self,0xC0)=psmInstanceA<Vec3>(self,0xA4);
    psmType4RefreshTransform(self);
    psmInstanceA<Vec3>(self,0xB4)=psmInstanceA<Vec3>(self,0xA4);
    const double total=static_cast<double>(elapsed)+psmInstanceA<float>(self,0xF4);
    const unsigned int steps=static_cast<unsigned int>(static_cast<int>(100.0*total));
    const double advanced=static_cast<double>(steps)*0.01f;
    psmInstanceA<float>(self,0xEC)=psmInstanceA<float>(self,0xE8);
    psmInstanceA<float>(self,0xF4)=static_cast<float>(total-advanced);
    psmInstanceA<float>(self,0xE8)=static_cast<float>(advanced+psmInstanceA<float>(self,0xE8));
    for (unsigned int i=0;psmInstanceA<float**>(self,0x24)[i];)
        i+=psmType4AdvanceParticle(self,psmInstanceA<float**>(self,0x24)[i],i,0.01f,steps);
    const double delay=static_cast<double>(psmInstanceA<float>(self,0xE4))-elapsed;
    psmInstanceA<float>(self,0xE4)=static_cast<float>(delay);
    if (delay>0) { psmInstanceA<float>(self,0xF0)=psmInstanceA<float>(self,0xE8); return; }
    if (psmInstanceA<float>(self,0xF8)>0)
    {
        float remaining=psmInstanceA<float>(self,0xF8); psmInstanceA<float>(self,0xF8)=0;
        for (;remaining>0;remaining-=0.033333335f) instance->reserved41(remaining>=0.033333335f?0.033333335f:remaining);
    }
    psmType4Emit(self,elapsed);
    psmInstanceA<Vec3>(self,0xA4)=psmInstanceA<Vec3>(self,0xB4);
    float* bounds=&psmInstanceA<float>(self,0xFC);
    if (psmInstanceA<unsigned int>(self,0x28))
    {
        for (int i=0;i<3;++i) bounds[i]-=psmDescriptor4<float>(psmInstanceA<void*>(self,0x2C),0x138);
        for (int i=3;i<6;++i) bounds[i]+=psmDescriptor4<float>(psmInstanceA<void*>(self,0x2C),0x138);
    }
    else { psmInstanceA<Vec3>(self,0xFC)=psmInstanceA<Vec3>(self,0xA4); psmInstanceA<Vec3>(self,0x108)=psmInstanceA<Vec3>(self,0xA4); }
    const float dz=bounds[5]-bounds[2], dx=static_cast<float>((static_cast<double>(bounds[3])-bounds[0])*0.5);
    const double dy=(static_cast<double>(bounds[4])-bounds[1])*0.5;
    psmInstanceA<float>(self,0x120)=static_cast<float>(::sqrt(dz*0.5*(dz*0.5)+static_cast<double>(dx)*dx+dy*dy));
    const float zsum=bounds[5]+bounds[2];
    psmInstanceA<Vec3>(self,0x114)={static_cast<float>((static_cast<double>(bounds[3])+bounds[0])*0.5),static_cast<float>((static_cast<double>(bounds[4])+bounds[1])*0.5),zsum*0.5f};
}
