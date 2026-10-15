// [CHOSEN, not recovered] private implementation split; included inside ParticleSystemManager.cpp.
// Ordinary SpriteParticleSystem emission: RendDX9 PE 10180500/1017FF30 and shape helpers
// 10181100/10181230/10181420, read in full. The dedicated ELF does not contain this renderer.
// Fixed offsets below belong to renderer-owned descriptor/instance/pool records, never host STL.

// Renderer-local Random at PE1023B618, initialized to this seed. Sharing the client executable's
// Random would couple two independent streams. PE1009EF30/1009EFC0/1017A330/360/420/480.
struct PsmEmissionRandom
{
    unsigned int seed;
    bool normalReady;
    float normal;
};
static PsmEmissionRandom psmType0Random = { 0x759DC150u, false, 0 };

unsigned int psmEmissionRandomStep(unsigned int value, unsigned int multiplier)
{
    const unsigned int low = (value & 0xFFFFu) * multiplier;
    const unsigned int high = static_cast<unsigned int>(static_cast<int>(value) >> 16) * multiplier;
    const int middle = static_cast<int>((low >> 16) + high);
    const unsigned int next = (low & 0xFFFFu) + ((static_cast<unsigned int>(middle) & 0x7FFFu) << 16)
        + static_cast<unsigned int>(middle >> 15);
    const int reduced = static_cast<int>(next - 0x7FFFFFFFu);
    return reduced < 0 ? next : static_cast<unsigned int>(reduced);
}
unsigned int psmEmissionRaw()
{
    psmType0Random.seed = psmEmissionRandomStep(psmEmissionRandomStep(psmType0Random.seed,24112),26143);
    return psmType0Random.seed;
}
int psmEmissionRandomInteger() { return (static_cast<int>(psmEmissionRaw() | 0x80u) >> 7) + 1; }
double psmEmissionUnit() { return static_cast<double>(psmEmissionRandomInteger()) * 0.000000059604644775390625; }
double psmEmissionSymmetric() { return static_cast<double>(psmEmissionRandomInteger()) * 0.00000011920928955078125 - 1.0; }
double psmEmissionNormal()
{
    if (psmType0Random.normalReady)
    {
        psmType0Random.normalReady = false;
        return psmType0Random.normal;
    }
    float first;
    double second, radius;
    do
    {
        first = static_cast<float>(psmEmissionSymmetric());
        second = psmEmissionSymmetric();
        radius = second * second + static_cast<double>(first) * first;
    } while (radius > 1.0);
    // Retail accepts radius==0; there is deliberately no extra rejection guard.
    const double scale = ::sqrt(::log(radius) * -2.0 / radius);
    psmType0Random.normal = static_cast<float>(scale * second);
    psmType0Random.normalReady = true;
    return scale * first;
}
double psmEmissionDistribution(void* distribution)
{
    const float sign = psmRuntimeField<unsigned char>(distribution,12)
        ? (psmEmissionUnit() > 0.5 ? 1.0f : -1.0f) : 1.0f;
    const float value = psmRuntimeField<float>(distribution,4);
    const float variation = psmRuntimeField<float>(distribution,8);
    double result;
    switch (psmRuntimeField<int>(distribution,0))
    {
    case 0: result = value; break;
    case 1: result = psmEmissionRandomInteger() * (static_cast<double>(variation)-value)
        * 0.000000059604644775390625 + value; break;
    case 2: result = -::log(psmEmissionUnit()) * value; break;
    case 3: result = psmEmissionNormal() * variation + value; break;
    default: result = 0; break;
    }
    return result * sign;
}

void psmEmissionDirection(float* output, void* descriptor, void* self)
{
    output[0]=psmDescriptor0<float>(descriptor,0x20C)*psmInstanceA<float>(self,0x150);
    output[1]=psmDescriptor0<float>(descriptor,0x210)*psmInstanceA<float>(self,0x154);
    output[2]=psmDescriptor0<float>(descriptor,0x214)*psmInstanceA<float>(self,0x158);
}
// PE10181100: five draws even when all dimensions are zero.
void psmEmissionSphere(float* output, void* descriptor, void* self, float* particle)
{
    const float phi = static_cast<float>(psmEmissionRandomInteger() * static_cast<double>(3.7450703871400037e-7f));
    const double theta = psmEmissionRandomInteger() * static_cast<double>(1.8725351935700019e-7f);
    const float savedTheta = static_cast<float>(theta);
    const float sineTheta = static_cast<float>(::sin(theta));
    const float yRandom = static_cast<float>(psmEmissionUnit());
    const float xRandom = static_cast<float>(psmEmissionUnit());
    const double zRadius = psmEmissionRandomInteger() * static_cast<double>(psmDescriptor0<float>(descriptor,0x20))
        * 0.000000059604644775390625;
    const double xRadius = static_cast<double>(xRandom) * psmDescriptor0<float>(descriptor,0x18);
    const double yRadius = static_cast<double>(yRandom) * psmDescriptor0<float>(descriptor,0x1C);
    particle[18]=static_cast<float>(::cos(phi)*xRadius*sineTheta);
    particle[19]=static_cast<float>(::sin(phi)*yRadius*sineTheta);
    particle[20]=static_cast<float>(::cos(savedTheta)*zRadius);
    psmEmissionDirection(output,descriptor,self);
}
// PE10181230. Shape1 emits radially; shape2 uses the configured direction. Largest-axis ties
// select the first component. The distributions are sampled Z,Y,X, not X,Y,Z.
void psmEmissionCylinder(float* output, void* descriptor, void* self, float* particle)
{
    const double angle=(psmEmissionSymmetric()*psmDescriptor0<float>(descriptor,0x70)
        +psmDescriptor0<float>(descriptor,0x74))*0.01745329238474369f;
    const float sine=static_cast<float>(::sin(angle)), cosine=static_cast<float>(::cos(angle));
    unsigned int axis=0;
    if(psmDescriptor0<float>(descriptor,0x1C)>psmDescriptor0<float>(descriptor,0x18))axis=1;
    if(psmDescriptor0<float>(descriptor,0x20)>psmDescriptor0<float>(descriptor,0x18+4*axis))axis=2;
    unsigned char* const bytes=static_cast<unsigned char*>(descriptor);
    const float z=static_cast<float>(psmEmissionDistribution(bytes+0x4C));
    const float y=static_cast<float>(psmEmissionDistribution(bytes+0x38));
    const double x=psmEmissionDistribution(bytes+0x24);
    if(axis==0){particle[18]=static_cast<float>(x);particle[19]=y*sine;particle[20]=z*cosine;}
    else if(axis==1){particle[18]=static_cast<float>(x*sine);particle[19]=y;particle[20]=z*cosine;}
    else{particle[18]=static_cast<float>(x*sine);particle[19]=y*cosine;particle[20]=z;}
    if(psmDescriptor0<int>(descriptor,0x264)==1)
    {
        output[0]=particle[18];output[1]=particle[19];output[2]=particle[20];
        output[axis]=0;
        psmNormalize3(output);
    }
    else psmEmissionDirection(output,descriptor,self);
}
void psmEmissionBox(float* output, void* descriptor, void* self, float* particle)
{
    unsigned char* const bytes=static_cast<unsigned char*>(descriptor);
    const float z=static_cast<float>(psmEmissionDistribution(bytes+0x4C));
    const float y=static_cast<float>(psmEmissionDistribution(bytes+0x38));
    particle[18]=static_cast<float>(psmEmissionDistribution(bytes+0x24));
    particle[19]=y;particle[20]=z;
    psmEmissionDirection(output,descriptor,self);
}

// PE10077DA0/E30/EC0 use the x87 fsincos on an explicitly rounded float angle.
void psmEmissionRotation(Mat4& matrix, float degrees, unsigned int axis)
{
    const float radians=degrees*0.01745329238474369f;
    float sine,cosine;
#if defined(_M_IX86)
    __asm
    {
        fld radians
        fsincos
        fstp cosine
        fstp sine
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    cosine=static_cast<float>(::cos(static_cast<double>(radians)));
    sine=static_cast<float>(::sin(static_cast<double>(radians)));
#endif
    matrix=Mat4::getIdentity();
    if(axis==0){matrix.m[5]=cosine;matrix.m[6]=sine;matrix.m[9]=-sine;matrix.m[10]=cosine;}
    else if(axis==1){matrix.m[0]=cosine;matrix.m[2]=-sine;matrix.m[8]=sine;matrix.m[10]=cosine;}
    else{matrix.m[0]=cosine;matrix.m[1]=-sine;matrix.m[4]=sine;matrix.m[5]=cosine;}
}
// PE10078180: 3x3 product; order differs for the first row's first/last components.
void psmEmissionRotateMultiply(Mat4& output,const Mat4& left,const Mat4& right)
{
    for(unsigned int column=0;column<3;++column)
        for(unsigned int row=0;row<3;++row)
        {
            if(row==0 && column!=1)
                output.m[row*4+column]=static_cast<float>(static_cast<double>(left.m[row*4])*right.m[column]
                    +static_cast<double>(left.m[row*4+1])*right.m[4+column]
                    +static_cast<double>(left.m[row*4+2])*right.m[8+column]);
            else if(row==0)
                output.m[column]=static_cast<float>(static_cast<double>(left.m[1])*right.m[5]
                    +static_cast<double>(right.m[1])*left.m[0]+static_cast<double>(right.m[9])*left.m[2]);
            else output.m[row*4+column]=static_cast<float>(static_cast<double>(left.m[row*4+2])*right.m[8+column]
                    +static_cast<double>(left.m[row*4+1])*right.m[4+column]
                    +static_cast<double>(left.m[row*4])*right.m[column]);
        }
}
float psmEmissionClamp(float value)
{
    if(value>=1.0f)value=1.0f;
    if(!(value>=0.0f))value=0.0f; // PE1000ED40 fcmovb also selects the lower bound for NaN.
    return value;
}

float* psmType0NewParticle(void* self)
{
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    float* const particle=static_cast<float*>(manager->m_type0FreeList);                  // +0x5F20
    manager->m_type0FreeList=particle?static_cast<float*>(psmRecordLink(particle, 0)):nullptr;
    manager->m_type0LiveCount+=particle!=nullptr;                                         // +0x5F28
    if(!particle)return nullptr;
    ++psmInstanceA<unsigned int>(self,0x144);
    void* descriptor=psmInstanceA<void*>(self,0x2C);
    if(psmDescriptor0<unsigned char>(descriptor,0xCD))
    {
        const int random=psmEmissionRandomInteger();
        descriptor=psmInstanceA<void*>(self,0x2C);
        const unsigned int count=psmDescriptor0<unsigned int>(descriptor,0xE0)-1;
        particle[30]=static_cast<float>(::floor(static_cast<double>(random)*count*0.000000059604644775390625));
    }
    else particle[30]=0;
    psmRuntimeField<unsigned int>(particle,0x54)|=0xFu;
    particle[12]=0;
    descriptor=psmInstanceA<void*>(self,0x2C);
    const double lifetime=psmEmissionRandomInteger()*static_cast<double>(psmDescriptor0<float>(descriptor,0x80))
        *0.000000059604644775390625+psmDescriptor0<float>(descriptor,0x84);
    particle[28]=static_cast<float>(lifetime);particle[29]=static_cast<float>(1.0/lifetime);
    particle[27]=psmInstanceA<float>(self,0xD8);particle[9]=particle[27]*particle[29];
    particle[3]=psmInstanceA<float>(self,0x15C);
    descriptor=psmInstanceA<void*>(self,0x2C);
    particle[13]=psmDescriptor0<float>(descriptor,0x1AC);particle[14]=psmDescriptor0<float>(descriptor,0x1B0);
    particle[11]=psmInstanceA<float>(self,0x160);particle[22]=psmInstanceA<float>(self,0x160);
    particle[2]=psmDescriptor0<float>(descriptor,0xA0);
    psmRuntimeField<unsigned short>(particle,0x40)=psmDescriptor0<unsigned short>(descriptor,0x21E);
    psmRuntimeField<unsigned short>(particle,0x42)=psmDescriptor0<unsigned short>(descriptor,0x220);
    const float firstRandom=static_cast<float>(psmEmissionSymmetric());
    particle[10]=firstRandom*psmDescriptor0<float>(descriptor,0x1B4);
    particle[4]=static_cast<float>(psmEmissionSymmetric()*psmDescriptor0<float>(descriptor,0x1B8));
    particle[32]=static_cast<float>(psmEmissionSymmetric()*psmDescriptor0<float>(descriptor,0x1BC));
    const double spin=psmEmissionSymmetric()*psmDescriptor0<float>(descriptor,0x94);
    particle[31]=static_cast<float>(spin>=0?spin+psmDescriptor0<float>(descriptor,0x98)
        :spin-psmDescriptor0<float>(descriptor,0x98));
    float direction[3]={0,0,0};
    switch(psmDescriptor0<int>(descriptor,0x264))
    {
    case 0:psmEmissionSphere(direction,descriptor,self,particle);break;
    case 1:case 2:psmEmissionCylinder(direction,descriptor,self,particle);break;
    case 3:psmEmissionBox(direction,descriptor,self,particle);break;
    }
    const float randomX=static_cast<float>(psmEmissionSymmetric());
    const float randomY=static_cast<float>(psmEmissionSymmetric());
    const float randomZ=static_cast<float>(psmEmissionSymmetric());
    Mat4 rotation,axis,copy;
    psmEmissionRotation(rotation,randomX*psmDescriptor0<float>(descriptor,0x1C4),0);
    psmEmissionRotation(axis,randomY*psmDescriptor0<float>(descriptor,0x1C8),1);
    copy=rotation;psmEmissionRotateMultiply(rotation,copy,axis);
    psmEmissionRotation(axis,randomZ*psmDescriptor0<float>(descriptor,0x1CC),2);
    copy=rotation;psmEmissionRotateMultiply(rotation,copy,axis);
    const float x=direction[0],y=direction[1],z=direction[2];
    direction[2]=static_cast<float>(static_cast<double>(rotation.m[2])*x+static_cast<double>(rotation.m[6])*y+static_cast<double>(rotation.m[10])*z);
    direction[1]=static_cast<float>(static_cast<double>(rotation.m[1])*x+static_cast<double>(rotation.m[9])*z+static_cast<double>(rotation.m[5])*y);
    direction[0]=static_cast<float>(static_cast<double>(rotation.m[0])*x+static_cast<double>(rotation.m[8])*z+static_cast<double>(rotation.m[4])*y);
    float speed=static_cast<float>((psmDescriptor0<float>(descriptor,0x1D0)
        -static_cast<double>(firstRandom)*psmDescriptor0<float>(descriptor,0x1C0))*psmInstanceA<float>(self,0x13C));
    const float emitTime=psmDescriptor0<float>(descriptor,0x1D4);
    const double t=emitTime>0?1.0-static_cast<double>(psmInstanceA<float>(self,0xE0))/emitTime:0;
    const float envelope=static_cast<float>(t*t*psmDescriptor0<float>(descriptor,0x130)+t*psmDescriptor0<float>(descriptor,0x134)
        +t*t*t*psmDescriptor0<float>(descriptor,0x12C)+psmDescriptor0<float>(descriptor,0x138));
    speed*=psmEmissionClamp(envelope);
    if(!psmDescriptor0<unsigned char>(descriptor,0x21D))
    {
        const float* const matrix=psmInstanceA<Mat4>(self,0x74).m;
        const float px=particle[18],py=particle[19],pz=particle[20];
        particle[20]=static_cast<float>(static_cast<double>(matrix[2])*px+static_cast<double>(matrix[10])*pz+static_cast<double>(matrix[6])*py+matrix[14]);
        particle[19]=static_cast<float>(static_cast<double>(matrix[1])*px+static_cast<double>(matrix[9])*pz+static_cast<double>(matrix[5])*py+matrix[13]);
        particle[18]=static_cast<float>(static_cast<double>(matrix[8])*pz+static_cast<double>(matrix[4])*py+static_cast<double>(matrix[0])*px+matrix[12]);
        const float dx=direction[0],dy=direction[1],dz=direction[2];
        direction[2]=static_cast<float>(static_cast<double>(matrix[10])*dz+static_cast<double>(matrix[2])*dx+static_cast<double>(matrix[6])*dy);
        direction[1]=static_cast<float>(static_cast<double>(matrix[9])*dz+static_cast<double>(matrix[1])*dx+static_cast<double>(matrix[5])*dy);
        direction[0]=static_cast<float>(static_cast<double>(matrix[8])*dz+static_cast<double>(matrix[4])*dy+static_cast<double>(matrix[0])*dx);
        const double scale=psmInstanceA<float>(self,0xDC);
        const float inheritedX=static_cast<float>(scale*psmInstanceA<float>(self,0xCC));
        const float inheritedY=static_cast<float>(scale*psmInstanceA<float>(self,0xD0));
        const float vy=direction[1]*speed,vz=direction[2]*speed;
        particle[26]=static_cast<float>(vz+scale*psmInstanceA<float>(self,0xD4));
        particle[24]=static_cast<float>(static_cast<double>(direction[0])*speed+inheritedX);
        particle[25]=vy+inheritedY;
    }
    else {particle[26]=direction[2]*speed;particle[24]=direction[0]*speed;particle[25]=direction[1]*speed;}
    descriptor=psmInstanceA<void*>(self,0x2C);
    void* const region=psmDescriptor0<void*>(descriptor,0x260);
    psmRecordPointer<void*>(particle, 0)=region?static_cast<void*>(static_cast<dx9rend::TextureAtlasEntry*>(region)->page):psmDescriptor0<void*>(descriptor,0x25C);
    psmRuntimeField<unsigned short>(particle,0x90)=psmDescriptor0<int>(descriptor,0x78)==2;
    float** const list=psmInstanceA<float**>(self,0x24);
    list[psmInstanceA<unsigned int>(self,0x28)++]=particle;
    list[psmInstanceA<unsigned int>(self,0x28)]=nullptr;
    return particle;
}

void psmType0Emit(void* self,float /*dt*/)
{
    if(!psmInstanceA<unsigned char>(self,0x14C))return;
    const unsigned int frame=static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getFrameOrdinal();
    if(psmInstanceA<unsigned int>(self,0x124)+60u<frame)
    {
        psmInstanceA<unsigned char>(self,0x129)=psmInstanceA<unsigned char>(self,0x128);
        psmInstanceA<unsigned char>(self,0x128)=0;return;
    }
    void* const descriptor=psmInstanceA<void*>(self,0x2C);
    const float lower=psmDescriptor0<float>(descriptor,0xC4);
    if(psmInstanceA<float>(self,0x134)<lower)psmInstanceA<float>(self,0x134)=lower;
    if(psmInstanceA<float>(self,0x138)<lower)psmInstanceA<float>(self,0x138)=lower;
    double frequency=static_cast<double>(psmDescriptor0<float>(descriptor,0x10))*psmInstanceA<float>(self,0x134)*psmInstanceA<float>(self,0x138);
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    if(psmDescriptor0<unsigned char>(descriptor,0x6C)||psmInstanceA<float>(self,0xE8)>1.0f)
    {
        // Retail manager +0x1D3C (the sprite renderer's over-limit byte), +0x5F28, +0x5F24, +0x8C08, +0x8BFC.
        if(manager->m_particleSystemRenderer[0x1C34]
            ||manager->m_type0LiveCount>static_cast<unsigned int>(static_cast<int>(
                static_cast<double>(static_cast<unsigned int>(manager->m_type0PoolCount))*manager->m_value8C08)))
            frequency*=manager->m_tierThresholds[psmDescriptor0<int>(descriptor,0x60)];
    }
    if(!(frequency>psmDescriptor0<float>(descriptor,0x14)))frequency=psmDescriptor0<float>(descriptor,0x14);
    const float interval=static_cast<float>(1.0/frequency);
    if(psmInstanceA<unsigned char>(self,0x224))
    {psmInstanceA<Vec3>(self,0xC0)=psmInstanceA<Vec3>(self,0xB4);psmInstanceA<unsigned char>(self,0x224)=0;}
    if(psmDescriptor0<unsigned char>(descriptor,0x268))
    {
        // [TEMP][CHOSEN, not recovered][RETIRE-WHEN: particle-emission-debug-draw]
        // PE10180621 draws emission statistics through the unrecovered debug owner10247C50+28.
        std::fprintf(stderr,"[FRONTIER] dice::hfe::geom::SpriteParticleSystem::emit(debug statistics) reached\n");
        std::fflush(stderr);std::abort();
    }
    while(static_cast<double>(interval)+psmInstanceA<float>(self,0xF0)<=psmInstanceA<float>(self,0xE8))
    {
        if(psmInstanceA<float>(self,0xE0)!=0)
        {
            const double remaining=static_cast<double>(psmInstanceA<float>(self,0xE0))-interval;
            psmInstanceA<float>(self,0xE0)=static_cast<float>(remaining);
            if(remaining<=0)
            {
                static_cast<IParticleSystemInstance*>(self)->reserved04();
                if(psmDescriptor0<unsigned char>(psmInstanceA<void*>(self,0x2C),0x6C))
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
            float* const particle=psmType0NewParticle(self);
            if(particle)
            {
                particle[24]+=psmInstanceA<float>(self,0x18);
                particle[25]+=psmInstanceA<float>(self,0x1C);
                particle[26]+=psmInstanceA<float>(self,0x20);
                for(unsigned int axis=24;axis<=26;++axis)if(::fabs(particle[axis])<0.001f)particle[axis]=0;
                const unsigned int steps=static_cast<unsigned int>(static_cast<int>((static_cast<double>(psmInstanceA<float>(self,0xE8))
                    -psmInstanceA<float>(self,0xF0))*100.0));
                if(psmType0AdvanceParticle(self,particle,psmInstanceA<unsigned int>(self,0x28)-1,0.01f,steps))
                {
                    const float remainder=static_cast<float>((static_cast<double>(psmInstanceA<float>(self,0xE8))
                        -psmInstanceA<float>(self,0xF0))-static_cast<double>(steps)*0.01f);
                    psmType0AdvanceParticle(self,particle,psmInstanceA<unsigned int>(self,0x28)-1,remainder,1);
                }
            }
        }
    }
}
