// [CHOSEN, not recovered] private NSA emission split included in ParticleSystemManager.cpp.
// Full BF2 RendDX9 PE1017DAE0(new particle),1017E2F0(emission),1017EB70/ECA0/EE90
// (sphere/cylinder/box) read. No dedicated ELF renderer body. Fixed200-byte pool records.
// The same PE1023B618 stream, distribution/math calls and their order are shared with type0;
// reuse only those attested helpers from ParticleInstanceType0Emission.inl.
int psmType4AdvanceParticle(void*,float*,int,float,unsigned int);

void psmType4EmissionDirection(float* output, void* descriptor, void* self)
{
    output[0]=psmDescriptor4<float>(descriptor,0x200)*psmInstanceA<float>(self,0x150);
    output[1]=psmDescriptor4<float>(descriptor,0x204)*psmInstanceA<float>(self,0x154);
    output[2]=psmDescriptor4<float>(descriptor,0x208)*psmInstanceA<float>(self,0x158);
}
// PE1017EB70: five draws even when all dimensions are zero.
void psmType4EmissionSphere(float* output, void* descriptor, void* self, float* particle)
{
    const float phi = static_cast<float>(psmEmissionRandomInteger() * static_cast<double>(3.7450703871400037e-7f));
    const double theta = psmEmissionRandomInteger() * static_cast<double>(1.8725351935700019e-7f);
    const float savedTheta = static_cast<float>(theta);
    const float sineTheta = static_cast<float>(::sin(theta));
    const float yRandom = static_cast<float>(psmEmissionUnit());
    const float xRandom = static_cast<float>(psmEmissionUnit());
    const double zRadius = psmEmissionRandomInteger() * static_cast<double>(psmDescriptor4<float>(descriptor,0x20))
        * 0.000000059604644775390625;
    const double xRadius = static_cast<double>(xRandom) * psmDescriptor4<float>(descriptor,0x18);
    const double yRadius = static_cast<double>(yRandom) * psmDescriptor4<float>(descriptor,0x1C);
    particle[1]=static_cast<float>(::cos(phi)*xRadius*sineTheta);
    particle[2]=static_cast<float>(::sin(phi)*yRadius*sineTheta);
    particle[3]=static_cast<float>(::cos(savedTheta)*zRadius);
    psmType4EmissionDirection(output,descriptor,self);
}
// PE1017ECA0. Shape1 emits radially; shape2 uses the configured direction. Largest-axis ties
// select the first component. The distributions are sampled Z,Y,X, not X,Y,Z.
void psmType4EmissionCylinder(float* output, void* descriptor, void* self, float* particle)
{
    const double angle=(psmEmissionSymmetric()*psmDescriptor4<float>(descriptor,0x60)
        +psmDescriptor4<float>(descriptor,0x64))*0.01745329238474369f;
    const float sine=static_cast<float>(::sin(angle)), cosine=static_cast<float>(::cos(angle));
    unsigned int axis=0;
    if(psmDescriptor4<float>(descriptor,0x1C)>psmDescriptor4<float>(descriptor,0x18))axis=1;
    if(psmDescriptor4<float>(descriptor,0x20)>psmDescriptor4<float>(descriptor,0x18+4*axis))axis=2;
    unsigned char* const bytes=static_cast<unsigned char*>(descriptor);
    const float z=static_cast<float>(psmEmissionDistribution(bytes+0x4C));
    const float y=static_cast<float>(psmEmissionDistribution(bytes+0x38));
    const double x=psmEmissionDistribution(bytes+0x24);
    if(axis==0){particle[1]=static_cast<float>(x);particle[2]=y*sine;particle[3]=z*cosine;}
    else if(axis==1){particle[1]=static_cast<float>(x*sine);particle[2]=y;particle[3]=z*cosine;}
    else{particle[1]=static_cast<float>(x*sine);particle[2]=y*cosine;particle[3]=z;}
    if(psmDescriptor4<int>(descriptor,0x258)==1)
    {
        output[0]=particle[1];output[1]=particle[2];output[2]=particle[3];
        output[axis]=0;
        psmNormalize3(output);
    }
    else psmType4EmissionDirection(output,descriptor,self);
}
void psmType4EmissionBox(float* output, void* descriptor, void* self, float* particle)
{
    unsigned char* const bytes=static_cast<unsigned char*>(descriptor);
    const float z=static_cast<float>(psmEmissionDistribution(bytes+0x4C));
    const float y=static_cast<float>(psmEmissionDistribution(bytes+0x38));
    particle[1]=static_cast<float>(psmEmissionDistribution(bytes+0x24));
    particle[2]=y;particle[3]=z;
    psmType4EmissionDirection(output,descriptor,self);
}


float* psmType4NewParticle(void* self)
{
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    float* const particle=static_cast<float*>(manager->m_type4FreeList);                  // +0x8B98
    manager->m_type4FreeList=particle?static_cast<float*>(psmRecordLink(particle, 4)):nullptr;
    manager->m_type4LiveCount+=particle!=nullptr;                                         // +0x8BA0
    if(!particle)return nullptr;
    ++psmInstanceA<unsigned int>(self,0x144);
    void* descriptor=psmInstanceA<void*>(self,0x2C);
    if(psmDescriptor4<unsigned char>(descriptor,0xBD))
    {
        const int random=psmEmissionRandomInteger();
        descriptor=psmInstanceA<void*>(self,0x2C);
        const unsigned int count=psmDescriptor4<unsigned int>(descriptor,0xD0)-1;
        particle[32]=static_cast<float>(::floor(static_cast<double>(random)*count*0.000000059604644775390625));
    }
    else particle[32]=0;
    psmRuntimeField<unsigned int>(particle,0x28)|=0xFu;
    particle[15]=0;
    descriptor=psmInstanceA<void*>(self,0x2C);
    const double lifetime=psmEmissionRandomInteger()*static_cast<double>(psmDescriptor4<float>(descriptor,0x78))
        *0.000000059604644775390625+psmDescriptor4<float>(descriptor,0x7C);
    particle[30]=static_cast<float>(lifetime);particle[31]=static_cast<float>(1.0/lifetime);
    particle[29]=psmInstanceA<float>(self,0xD8);particle[9]=particle[29]*particle[31];
    particle[18]=psmInstanceA<float>(self,0x15C);
    descriptor=psmInstanceA<void*>(self,0x2C);
    particle[20]=psmDescriptor4<float>(descriptor,0x19C);particle[21]=psmDescriptor4<float>(descriptor,0x1A0);
    particle[13]=psmInstanceA<float>(self,0x160);particle[14]=psmInstanceA<float>(self,0x160);
    particle[35]=psmDescriptor4<float>(descriptor,0x98);
    psmRuntimeField<unsigned short>(particle,0x40)=psmDescriptor4<unsigned short>(descriptor,0x212);
    psmRuntimeField<unsigned short>(particle,0x42)=psmDescriptor4<unsigned short>(descriptor,0x214);
    const float firstRandom=static_cast<float>(psmEmissionSymmetric());
    particle[12]=firstRandom*psmDescriptor4<float>(descriptor,0x1A4);
    particle[19]=static_cast<float>(psmEmissionSymmetric()*psmDescriptor4<float>(descriptor,0x1A8));
    particle[34]=static_cast<float>(psmEmissionSymmetric()*psmDescriptor4<float>(descriptor,0x1AC));
    const double spin=psmEmissionSymmetric()*psmDescriptor4<float>(descriptor,0x8C);
    particle[33]=static_cast<float>(spin>=0?spin+psmDescriptor4<float>(descriptor,0x90)
        :spin-psmDescriptor4<float>(descriptor,0x90));
    float direction[3]={0,0,0};
    switch(psmDescriptor4<int>(descriptor,0x258))
    {
    case 0:psmType4EmissionSphere(direction,descriptor,self,particle);break;
    case 1:case 2:psmType4EmissionCylinder(direction,descriptor,self,particle);break;
    case 3:psmType4EmissionBox(direction,descriptor,self,particle);break;
    }
    const float randomX=static_cast<float>(psmEmissionSymmetric());
    const float randomY=static_cast<float>(psmEmissionSymmetric());
    const float randomZ=static_cast<float>(psmEmissionSymmetric());
    Mat4 rotation,axis,copy;
    psmEmissionRotation(rotation,randomX*psmDescriptor4<float>(descriptor,0x1B4),0);
    psmEmissionRotation(axis,randomY*psmDescriptor4<float>(descriptor,0x1B8),1);
    copy=rotation;psmEmissionRotateMultiply(rotation,copy,axis);
    psmEmissionRotation(axis,randomZ*psmDescriptor4<float>(descriptor,0x1BC),2);
    copy=rotation;psmEmissionRotateMultiply(rotation,copy,axis);
    const float x=direction[0],y=direction[1],z=direction[2];
    direction[2]=static_cast<float>(static_cast<double>(rotation.m[2])*x+static_cast<double>(rotation.m[6])*y+static_cast<double>(rotation.m[10])*z);
    direction[1]=static_cast<float>(static_cast<double>(rotation.m[1])*x+static_cast<double>(rotation.m[9])*z+static_cast<double>(rotation.m[5])*y);
    direction[0]=static_cast<float>(static_cast<double>(rotation.m[0])*x+static_cast<double>(rotation.m[8])*z+static_cast<double>(rotation.m[4])*y);
    float speed=static_cast<float>((psmDescriptor4<float>(descriptor,0x1C0)
        -static_cast<double>(firstRandom)*psmDescriptor4<float>(descriptor,0x1B0))*psmInstanceA<float>(self,0x13C));
    const float emitTime=psmDescriptor4<float>(descriptor,0x1C4);
    const double t=emitTime>0?1.0-static_cast<double>(psmInstanceA<float>(self,0xE0))/emitTime:0;
    const float envelope=static_cast<float>(t*t*psmDescriptor4<float>(descriptor,0x120)+t*psmDescriptor4<float>(descriptor,0x124)
        +t*t*t*psmDescriptor4<float>(descriptor,0x11C)+psmDescriptor4<float>(descriptor,0x128));
    speed*=psmEmissionClamp(envelope);
    if(!psmDescriptor4<unsigned char>(descriptor,0x211))
    {
        const float* const matrix=psmInstanceA<Mat4>(self,0x74).m;
        const float px=particle[1],py=particle[2],pz=particle[3];
        particle[3]=static_cast<float>(static_cast<double>(matrix[2])*px+static_cast<double>(matrix[10])*pz+static_cast<double>(matrix[6])*py+matrix[14]);
        particle[2]=static_cast<float>(static_cast<double>(matrix[1])*px+static_cast<double>(matrix[9])*pz+static_cast<double>(matrix[5])*py+matrix[13]);
        particle[1]=static_cast<float>(static_cast<double>(matrix[8])*pz+static_cast<double>(matrix[4])*py+static_cast<double>(matrix[0])*px+matrix[12]);
        const float dx=direction[0],dy=direction[1],dz=direction[2];
        direction[2]=static_cast<float>(static_cast<double>(matrix[10])*dz+static_cast<double>(matrix[2])*dx+static_cast<double>(matrix[6])*dy);
        direction[1]=static_cast<float>(static_cast<double>(matrix[9])*dz+static_cast<double>(matrix[1])*dx+static_cast<double>(matrix[5])*dy);
        direction[0]=static_cast<float>(static_cast<double>(matrix[8])*dz+static_cast<double>(matrix[4])*dy+static_cast<double>(matrix[0])*dx);
        const double scale=psmInstanceA<float>(self,0xDC);
        const float inheritedX=static_cast<float>(scale*psmInstanceA<float>(self,0xCC));
        const float inheritedY=static_cast<float>(scale*psmInstanceA<float>(self,0xD0));
        const float vy=direction[1]*speed,vz=direction[2]*speed;
        particle[25]=static_cast<float>(vz+scale*psmInstanceA<float>(self,0xD4));
        particle[23]=static_cast<float>(static_cast<double>(direction[0])*speed+inheritedX);
        particle[24]=vy+inheritedY;
    }
    else {particle[25]=direction[2]*speed;particle[23]=direction[0]*speed;particle[24]=direction[1]*speed;}
    descriptor=psmInstanceA<void*>(self,0x2C);
    void* const region=psmDescriptor4<void*>(descriptor,0x254);
    psmRecordPointer<void*>(particle, 4)=region?static_cast<void*>(static_cast<dx9rend::TextureAtlasEntry*>(region)->page):psmDescriptor4<void*>(descriptor,0x250);
    // PE1023A414..440: four oriented unit corners (-1,0,1),(1,0,1),(1,0,-1),(-1,0,-1).
    // These are transformed even for local systems; retain the zero terms and retail sum order.
    static const float corners[4][3]={{-1,0,1},{1,0,1},{1,0,-1},{-1,0,-1}};
    const float* const basis=psmInstanceA<Mat4>(self,0x74).m;
    for(unsigned int i=0;i<4;++i)
    {
        const float* const corner=corners[i];
        const double z=static_cast<double>(corner[0])*basis[2]+static_cast<double>(corner[1])*basis[6]+static_cast<double>(corner[2])*basis[10];
        const double y=static_cast<double>(corner[0])*basis[1]+static_cast<double>(corner[1])*basis[5]+static_cast<double>(corner[2])*basis[9];
        particle[38+3*i]=static_cast<float>(static_cast<double>(corner[1])*basis[4]+static_cast<double>(corner[2])*basis[8]+static_cast<double>(corner[0])*basis[0]);
        particle[39+3*i]=static_cast<float>(y);
        particle[40+3*i]=static_cast<float>(z);
    }
    float** const list=psmInstanceA<float**>(self,0x24);
    list[psmInstanceA<unsigned int>(self,0x28)++]=particle;
    list[psmInstanceA<unsigned int>(self,0x28)]=nullptr;
    return particle;
}

// PE1017E2F0; dt is intentionally unused.
void psmType4Emit(void* self,float /*dt*/)
{
    if(!psmInstanceA<unsigned char>(self,0x14C))return;
    const unsigned int frame=static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getFrameOrdinal();
    if(psmInstanceA<unsigned int>(self,0x124)+60u<frame)
    {
        psmInstanceA<unsigned char>(self,0x129)=psmInstanceA<unsigned char>(self,0x128);
        psmInstanceA<unsigned char>(self,0x128)=0;return;
    }
    void* const descriptor=psmInstanceA<void*>(self,0x2C);
    const float lower=psmDescriptor4<float>(descriptor,0xB4);
    if(psmInstanceA<float>(self,0x134)<lower)psmInstanceA<float>(self,0x134)=lower;
    if(psmInstanceA<float>(self,0x138)<lower)psmInstanceA<float>(self,0x138)=lower;
    double frequency=static_cast<double>(psmDescriptor4<float>(descriptor,0x10))*psmInstanceA<float>(self,0x134)*psmInstanceA<float>(self,0x138);
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    if(psmDescriptor4<unsigned char>(descriptor,0x74)||psmInstanceA<float>(self,0xE8)>1.0f)
    {
        // Retail manager +0x8A1C (the NSA renderer's over-limit byte), +0x8BA0, +0x8B9C, +0x8C08, +0x8BFC.
        if(manager->m_nsaParticleSystemRenderer[0x424]
            ||manager->m_type4LiveCount>static_cast<unsigned int>(static_cast<int>(
                static_cast<double>(static_cast<unsigned int>(manager->m_type4PoolCount))*manager->m_value8C08)))
            frequency*=manager->m_tierThresholds[psmDescriptor4<int>(descriptor,0x68)];
    }
    if(!(frequency>psmDescriptor4<float>(descriptor,0x14)))frequency=psmDescriptor4<float>(descriptor,0x14);
    const float interval=static_cast<float>(1.0/frequency);
    if(psmInstanceA<unsigned char>(self,0x224))
    {psmInstanceA<Vec3>(self,0xC0)=psmInstanceA<Vec3>(self,0xB4);psmInstanceA<unsigned char>(self,0x224)=0;}
    while(static_cast<double>(interval)+psmInstanceA<float>(self,0xF0)<=psmInstanceA<float>(self,0xE8))
    {
        if(psmInstanceA<float>(self,0xE0)!=0)
        {
            const double remaining=static_cast<double>(psmInstanceA<float>(self,0xE0))-interval;
            psmInstanceA<float>(self,0xE0)=static_cast<float>(remaining);
            if(remaining<=0)
            {
                static_cast<IParticleSystemInstance*>(self)->reserved04();
                if(psmDescriptor4<unsigned char>(psmInstanceA<void*>(self,0x2C),0x74))
                    static_cast<IParticleSystemInstance*>(self)->reserved03(psmInstanceA<unsigned char>(self,6)!=0);
                return;
            }
        }
        psmInstanceA<float>(self,0xF0)=interval+psmInstanceA<float>(self,0xF0);
        if(psmInstanceA<unsigned char>(self,0x128)&&psmInstanceA<unsigned int>(self,0x28)<psmInstanceA<unsigned int>(self,0x220))
        {
            float fraction=0;
            if(psmInstanceA<float>(self,0xE8)>psmInstanceA<float>(self,0xEC))
                fraction=static_cast<float>((static_cast<double>(psmInstanceA<float>(self,0xF0))-psmInstanceA<float>(self,0xEC))
                    /(static_cast<double>(psmInstanceA<float>(self,0xE8))-psmInstanceA<float>(self,0xEC)));
            const Vec3& from=psmInstanceA<Vec3>(self,0xC0);
            const Vec3& to=psmInstanceA<Vec3>(self,0xB4);
            const float zDifference=to.z-from.z;
            const float xScaled=static_cast<float>((static_cast<double>(to.x)-from.x)*fraction);
            const double yScaled=(static_cast<double>(to.y)-from.y)*fraction;
            const float zScaled=zDifference*fraction;
            psmInstanceA<Vec3>(self,0xA4)={xScaled+from.x,static_cast<float>(yScaled+from.y),zScaled+from.z};
            float* const particle=psmType4NewParticle(self);
            if(particle)
            {
                particle[23]+=psmInstanceA<float>(self,0x18);
                particle[24]+=psmInstanceA<float>(self,0x1C);
                particle[25]+=psmInstanceA<float>(self,0x20);
                for(unsigned int axis=23;axis<=25;++axis)if(::fabs(particle[axis])<0.001f)particle[axis]=0;
                const unsigned int steps=static_cast<unsigned int>(static_cast<int>((static_cast<double>(psmInstanceA<float>(self,0xE8))
                    -psmInstanceA<float>(self,0xF0))*100.0));
                if(psmType4AdvanceParticle(self,particle,psmInstanceA<unsigned int>(self,0x28)-1,0.01f,steps))
                {
                    const float remainder=static_cast<float>((static_cast<double>(psmInstanceA<float>(self,0xE8))
                        -psmInstanceA<float>(self,0xF0))-static_cast<double>(steps)*0.01f);
                    psmType4AdvanceParticle(self,particle,psmInstanceA<unsigned int>(self,0x28)-1,remainder,1);
                }
            }
        }
    }
}


