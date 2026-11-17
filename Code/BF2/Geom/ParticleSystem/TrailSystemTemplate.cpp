// Type-2 trail descriptor with property, texture, and UV state.
// Contains no simulation or per-particle update function.

#ifdef BF2_IMAGE_RENDDX9
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Compiler-typed native table entries.
#include "../../../RendDX9Abi/NativeVtable.h"
#include "ParticleSystemManager.h"
#endif

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "ParticleRawLayout.h"
#include "../../Platform/X87Compat.h"
#include "../XpLightManager.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include <string>
#include "../../../RendDX9Abi/PluginAbi.h"    // exact case: a Linux checkout is case-sensitive
#include "../../../RendDX9Abi/EngineFile.h"
#include "../../RendDX9/Texture.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../GeomMisc/Debug.h"
#include "ParticleSystemManager.h"
#include <cstdio>
#include <cstdlib>

#include <new>
#include <string.h>
#include <math.h>
#include <float.h>

namespace dice {
namespace hfe {
namespace geom {

void psmZeroTemplateBlock(void* block);   // ParticleSystemTemplate.cpp

extern LightManager* g_LightManager; // Retail refresh dereferences this when affection is negative/NaN.
const Vec3* terrainOriginScratch(const Vec3& value);
const float* staticMeshPackVec4(const float value[3], float w);

namespace {

using ParticleHostString = ::renddx9abi::HostString;
static_assert(sizeof(ParticleHostString) <= kPsmDescriptorStringSpan, "trail host string exceeds its reserved span");

inline unsigned&      U (void* p, unsigned off) { return psmDescriptor2<unsigned>(p, off); }
inline float&         F (void* p, unsigned off) { return psmDescriptor2<float>(p, off); }
inline unsigned char& Bt(void* p, unsigned off) { return psmDescriptor2<unsigned char>(p, off); }
inline short&         S16(void* p, unsigned off){ return psmDescriptor2<short>(p, off); }
inline unsigned       fbits(float f) { unsigned u; ::memcpy(&u, &f, 4); return u; }

#define PT_SET(name, off)  void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ U(s,(off)) = v; }
#define PT_SETFLT(name, off) void RD9_THUNK name(void* s RD9_THUNK_EDX, PsmFloatArg v){ U(s,(off)) = psmFloatArgBits(v); }
#define PT_GETF(name, off) float    RD9_THUNK name(void* s RD9_THUNK_EDX){ return F(s,(off)); }
#define PT_GETD(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return U(s,(off)); }
#define PT_SETB(name, off) void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ Bt(s,(off)) = static_cast<unsigned char>(v); }
#define PT_GETB(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return Bt(s,(off)); }
#define PT_LEA(name, off)  void*    RD9_THUNK name(void* s RD9_THUNK_EDX){ return &psmDescriptor2<unsigned char>(s, (off)); }
#define PT_SETVEC(name, off, n) void RD9_THUNK name(void* s RD9_THUNK_EDX, const float* v) \
{ \
    if ((n)==3 || (n)==4) \
    { \
        const float z=v[2],y=v[1],x=v[0],w=(n)==4 ? v[3] : 0.0f; \
        F(s,(off)+4)=y; F(s,(off))=x; F(s,(off)+8)=z; \
        if ((n)==4) F(s,(off)+12)=w; \
    } \
    else for (int i=0;i<(n);++i) F(s,(off)+4u*i)=v[i]; \
}
#define PT_GETVEC(name, off, n) void* RD9_THUNK name(void* s RD9_THUNK_EDX, float* o) \
{ \
    if ((n)==3) \
    { \
        const float z=F(s,(off)+8),y=F(s,(off)+4),x=F(s,(off)); \
        o[1]=y; o[0]=x; o[2]=z; \
    } \
    else for (int i=0;i<(n);++i) o[i]=F(s,(off)+4u*i); \
    return o; \
}
#define PT_VEC3A(name, off) void RD9_THUNK name(void* s RD9_THUNK_EDX, PSM_VEC3_PARAMS){ U(s,(off))=PSM_VEC3_X; U(s,(off)+4)=PSM_VEC3_Y; U(s,(off)+8)=PSM_VEC3_Z; }

void RD9_THUNK n2_s2 (void* s RD9_THUNK_EDX);
void RD9_THUNK n2_s45(void* s RD9_THUNK_EDX, PsmFloatArg a);
void            n2_recomputeUV(void* self);

// shared trivial slots
// PE 100E13B0 is RET 4, shared by one-argument setters 41/43/51/57/110.
void     RD9_THUNK n2_null(void* RD9_THUNK_EDX, unsigned) { }
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] This shipped RET 4 body discards its argument. Give
// each native register category a typed entry instead of converting float bits
// or a graph address into the PE's unused integer stack carrier.
void n2_nullFloat(void* self, float) { n2_null(self, 0); }
void n2_nullGraph(void* self, const Vec4&) { n2_null(self, 0); }
#endif
float    RD9_THUNK n2_ret0(void* RD9_THUNK_EDX) { return 0.0f; }
unsigned RD9_THUNK n2_ret1(void* RD9_THUNK_EDX) { return 1; }

// simple slots
const ParticleHostString& RD9_THUNK n2_s1(void* self RD9_THUNK_EDX)
{ return psmDescriptor2<ParticleHostString>(self, 0x200); }
PT_SET (n2_s4,  0x64)
PT_GETD(n2_s5,  0x64)
PT_SET (n2_s6,  0x68)
PT_GETD(n2_s7,  0x68)
// PE 0x10178A70
PT_SET (n2_s8,  0x6C)
PT_GETD(n2_s9,  0x6C)
PT_VEC3A(n2_s10, 0x14)
PT_GETVEC(n2_s11, 0x14, 3)
PT_SETFLT(n2_s12, 0x60)
PT_GETF(n2_s13, 0x60)
PT_SETFLT(n2_s14, 0x5C)
PT_GETF(n2_s15, 0x5C)
// PE 0x10188210
PT_SET (n2_s16, 0x224)
PT_SETVEC(n2_s17, 0x20, 5)
PT_LEA (n2_s18, 0x20)
// PE 0x10188220
PT_SETVEC(n2_s19, 0x34, 5)
PT_LEA (n2_s20, 0x34)
// PE 0x10188250
PT_SETVEC(n2_s21, 0x48, 5)
PT_LEA (n2_s22, 0x48)
PT_GETF(n2_s24, 0x10)
// PE 0x10188330
PT_SETFLT(n2_s25, 0x164)
PT_GETF(n2_s26, 0x164)
PT_SETFLT(n2_s27, 0x0C)
PT_GETF(n2_s28, 0x0C)
PT_SETFLT(n2_s29, 0x168)
PT_GETF(n2_s30, 0x168)
// PE 0x101881E0
PT_SETB(n2_s31, 0x16C)
PT_GETB(n2_s32, 0x16C)
// PE 0x101881C0
PT_SETFLT(n2_s33, 0x178)
PT_GETF(n2_s34, 0x178)
// PE 0x101884E0
PT_SETVEC(n2_s35, 0x158, 3)
// PE 0x10188510
PT_GETVEC(n2_s36, 0x158, 3)
// PE 0x10187920
PT_GETVEC(n2_s38, 0x1AC, 3)
// PE 0x10187800
PT_SETVEC(n2_s39, 0x11C, 4)
PT_LEA (n2_s40, 0x11C)
PT_GETF(n2_s46, 0x70)
PT_SETFLT(n2_s47, 0x78)
PT_GETF(n2_s48, 0x78)
PT_SETFLT(n2_s49, 0x7C)
PT_GETF(n2_s50, 0x7C)
// PE 0x10187890
PT_SETVEC(n2_s53, 0x12C, 4)
PT_LEA (n2_s54, 0x12C)
// PE 0x101878C0
PT_SETVEC(n2_s55, 0x13C, 4)
PT_LEA (n2_s56, 0x13C)
// PE 0x10187830
PT_SETVEC(n2_s59, 0xFC, 4)
PT_LEA (n2_s60, 0xFC)
// PE 0x10178BE0
PT_SETFLT(n2_s61, 0x88)
PT_GETF(n2_s62, 0x88)
// PE 0x101882E0
PT_SETB(n2_s63, 0x9C)
PT_GETB(n2_s64, 0x9C)
PT_SETFLT(n2_s65, 0xA0)
PT_GETF(n2_s66, 0xA0)
PT_SETFLT(n2_s67, 0xA4)
PT_GETF(n2_s68, 0xA4)
// PE 0x10188370
PT_SETB(n2_s69, 0x1C0)
PT_GETB(n2_s70, 0x1C0)
// PE 0x10188390
PT_SETB(n2_s71, 0x8C)
PT_GETB(n2_s72, 0x8C)
// PE 0x10178C20
PT_SETFLT(n2_s73, 0x90)
PT_GETF(n2_s74, 0x90)
// PE 0x1017A0A0
PT_SET (n2_s75, 0x234)
PT_GETD(n2_s76, 0x234)
// PE 0x10188420
PT_SETFLT(n2_s77, 0x238)
PT_GETF(n2_s78, 0x238)
// PE 0x10188440
PT_SETFLT(n2_s79, 0x23C)
PT_GETF(n2_s80, 0x23C)
// PE 0x10188460
PT_SETFLT(n2_s81, 0x240)
PT_GETF(n2_s82, 0x240)
// PE 0x10188480
PT_SETFLT(n2_s83, 0x244)
PT_GETF(n2_s84, 0x244)
// PE 0x101884A0
PT_SETFLT(n2_s85, 0x248)
PT_GETF(n2_s86, 0x248)
// PE 0x101884C0
PT_SETFLT(n2_s87, 0x24C)
PT_GETF(n2_s88, 0x24C)
PT_GETF(n2_s90, 0x1B8)
// PE 0x101882A0
PT_SETFLT(n2_s91, 0x14C)
PT_GETF(n2_s92, 0x14C)
// PE 0x101882C0
PT_SETFLT(n2_s93, 0x150)
PT_GETF(n2_s94, 0x150)
// PE 0x10187860
PT_SETVEC(n2_s95, 0x10C, 4)
PT_LEA (n2_s96, 0x10C)
PT_SETFLT(n2_s97, 0x80)
PT_GETF(n2_s98, 0x80)
// PE 0x10187210
PT_SETFLT(n2_s99, 0x17C)
PT_GETF(n2_s100, 0x17C)
// PE 0x101876C0
PT_GETVEC(n2_s102, 0x180, 3)
// PE 0x101877D0
PT_SETVEC(n2_s106, 0xEC, 4)
PT_LEA (n2_s107, 0xEC)
// PE 0x10187110
PT_SETFLT(n2_s108, 0xB4)
PT_GETF(n2_s109, 0xB4)
// PE 0x10188400
PT_SETB(n2_s112, 0x228)
PT_GETB(n2_s113, 0x228)
// PE 0x10187290
PT_SETVEC(n2_s114, 0xAC, 2)
// PE 0x10187950
void* RD9_THUNK n2_s115(void* self RD9_THUNK_EDX, float* out)
{
    static float scratch[2];
    scratch[0]=F(self,0xAC); scratch[1]=F(self,0xB0);
    const float y=scratch[1],x=scratch[0];
    out[0]=x; out[1]=y; return out;
}
const ParticleHostString& RD9_THUNK n2_s117(void* self RD9_THUNK_EDX)
{ return psmDescriptor2<ParticleHostString>(self, 0x1E4); }
// PE 0x101883B0
PT_SETB(n2_s118, 0x1C8)
PT_GETB(n2_s119, 0x1C8)
// PE 0x101883D0
PT_SETFLT(n2_s120, 0x1C4)
PT_GETF(n2_s121, 0x1C4)
// PE 0x10178B20
PT_SETFLT(n2_s126, 0x1CC)
PT_GETF(n2_s127, 0x1CC)
PT_SETFLT(n2_s128, 0x1D0)
PT_GETF(n2_s129, 0x1D0)
// PE 0x101883E0
PT_SET (n2_s130, 0x1D4)
PT_GETD(n2_s131, 0x1D4)
PT_GETD(n2_s133, 0x1D8)
// PE 0x10187190
PT_SETFLT(n2_s134, 0xBC)
PT_GETF(n2_s135, 0xBC)
// PE 0x101871B0
PT_SETFLT(n2_s136, 0xC0)
PT_GETF(n2_s137, 0xC0)
// PE 0x101871D0
PT_SETFLT(n2_s138, 0xC4)
PT_GETF(n2_s139, 0xC4)
// PE 0x101871F0
PT_SETFLT(n2_s140, 0xC8)
PT_GETF(n2_s141, 0xC8)
PT_GETD(n2_s143, 0x220)
PT_GETF(n2_s145, 0x22C)

// complex slots

unsigned RD9_THUNK n2_s3(void* RD9_THUNK_EDX) { return 2; }   // emitter type id

// PE 0x10188060
void RD9_THUNK n2_s2(void* self RD9_THUNK_EDX)
{
    if (!(F(self, 0x17C) >= 0.0f))
    {
        F(self, 0xD8) = g_LightManager->getDefaultEffectLightAffectionFactor();
    }
    else
        U(self, 0xD8) = U(self, 0x17C);

    unsigned mode = U(self, 0x64);
    if (mode == 0) F(self, 0x174) = 0.5f;
    else if (mode == 1) F(self, 0x174) = 0.7f;
    else if (mode == 2) F(self, 0x174) = 1.0f;

    static_cast<IParticleSystemTemplateType2*>(self)->setTimeToLive(F(self, 0x70));

    if (F(self, 0x10) < FLT_EPSILON) U(self, 0x170) = 0;
    else F(self, 0x170) = 1.0f / F(self, 0x10);
    if (U(self, 0x1D8)) F(self, 0x1DC) = 1.0f / static_cast<float>(U(self, 0x1D8));
    else                U(self, 0x1DC) = 0;

    n2_recomputeUV(self);
}

// PE 0x10188310
void RD9_THUNK n2_s23(void* self RD9_THUNK_EDX, PsmFloatArg a) { U(self, 0x10) = psmFloatArgBits(a); F(self, 0x170) = 1.0f / F(self, 0x10); }

// PE 0x101878F0
void RD9_THUNK n2_s37(void* self RD9_THUNK_EDX, const float* v)
{
    const float z=v[2],y=v[1],x=v[0];
    F(self,0x1B0)=y; F(self,0x1AC)=x; F(self,0x1B4)=z;
    float* p = &psmDescriptor2<float>(self, 0x1AC);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] The squared sum spills once; the inverse remains live.
    using bf2x87::wide;
    const float s=static_cast<float>(wide(p[0])*wide(p[0])+wide(p[1])*wide(p[1])+wide(p[2])*wide(p[2]));
    const bf2x87::Real error=bf2x87::abs(wide(s)-wide(1));
#else
    const float s=static_cast<float>(static_cast<double>(p[0])*p[0]+static_cast<double>(p[1])*p[1]
        +static_cast<double>(p[2])*p[2]);
    const double error=::fabs(static_cast<double>(s)-1.0);
#endif
    if (!(error < FLT_EPSILON))
    {
        if (fabsf(s) < FLT_EPSILON) { p[0] = p[1] = p[2] = 0.0f; }
        else
        {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const bf2x87::Real inv=wide(1)/bf2x87::squareRoot(wide(s));
            p[0]=static_cast<float>(inv*wide(p[0])); p[1]=static_cast<float>(inv*wide(p[1]));
            p[2]=static_cast<float>(inv*wide(p[2]));
#else
            const double inv=1.0/::sqrt(static_cast<double>(s));
            p[0]=static_cast<float>(inv*p[0]); p[1]=static_cast<float>(inv*p[1]); p[2]=static_cast<float>(inv*p[2]);
#endif
        }
    }
}

// PE 0x101870D0
void RD9_THUNK n2_s45(void* self RD9_THUNK_EDX, PsmFloatArg a) { U(self, 0x70) = psmFloatArgBits(a); F(self, 0x74) = 1.0f / F(self, 0x70); }

// a virtual tail call to slot 56, retaining possible overrides.
const Vec4& RD9_THUNK n2_s58(void* self RD9_THUNK_EDX)
{ return static_cast<IParticleSystemTemplateType2*>(self)->getAirResistanceGraph(); }

// PE 0x10187130
// slot 89: dual write.
void RD9_THUNK n2_s89(void* self RD9_THUNK_EDX, PsmFloatArg a) { U(self, 0xB8) = psmFloatArgBits(a); U(self, 0x1B8) = psmFloatArgBits(a); }

// slot 101: Vec3 at +0x180 + mirror +0xCC.
// PE 0x10187670
void RD9_THUNK n2_s101(void* self RD9_THUNK_EDX, const float* v)
{
    const float z=v[2],y=v[1],x=v[0];
    F(self,0x184)=y; F(self,0x180)=x; F(self,0x188)=z;
    F(self,0xCC)=x; F(self,0xD0)=F(self,0x184); F(self,0xD4)=F(self,0x188);
}

// slot 103: Vec3 -> Vec4 {x,y,z,1} at +0x18C + mirror +0xDC.
// PE 0x101876F0
void RD9_THUNK n2_s103(void* self RD9_THUNK_EDX, const float* v)
{
    const float* const packed=staticMeshPackVec4(v,1.0f);
    const float w=packed[3],z=packed[2],y=packed[1],x=packed[0];
    F(self,0x190)=y; F(self,0x18C)=x; F(self,0x194)=z; F(self,0x198)=w;
    F(self,0xDC)=x; F(self,0xE0)=F(self,0x190);
    F(self,0xE4)=F(self,0x194); F(self,0xE8)=F(self,0x198);
}

// slot 104: get the +0x18C color as Vec3.
// PE 0x10187760
void* RD9_THUNK n2_s104(void* self RD9_THUNK_EDX, float* out)
{
    const Vec3* const packed=terrainOriginScratch(psmDescriptor2<Vec3>(self,0x18C));
    const float z=packed->z,y=packed->y,x=packed->x;
    out[1]=y; out[0]=x; out[2]=z;
    return out;
}

// slot 105: Vec3 -> Vec4 {x,y,z,1} at +0x19C (no mirror).
// PE 0x10187790
void RD9_THUNK n2_s105(void* self RD9_THUNK_EDX, const float* v)
{
    const float* const packed=staticMeshPackVec4(v,1.0f);
    const float w=packed[3],z=packed[2],y=packed[1],x=packed[0];
    F(self,0x1A0)=y; F(self,0x19C)=x; F(self,0x1A4)=z; F(self,0x1A8)=w;
}

// unlike the sprite/NSA loader, trails use a direct texture without an atlas lookup.
// PE 0x10187CD0
bool n2_loadTexture(void* self, const std::string& name)
{
    if (name.empty())
        return false;
    dx9rend::EngineTexture*& texture = psmDescriptor2<dx9rend::EngineTexture*>(self, 0x21C);
    dx9rend::textureReleaseRef(texture);
    std::string path(name);
    path += ".dds";
    if (::renddx9abi::engineFileExists(path))
    {
        dx9rend::EngineTexture* const loaded =
            static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(name));
        dx9rend::textureReleaseRef(texture);
        texture = loaded;
    }
    else
    {
        // PE 0x10187DE9.
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\ParticleSystem\\TrailSystemTemplate.cpp",
            169, "") << "File " << name << " not found!";
    }
    return texture != 0;
}

// Keep the authored name copy before loading and the empty-name refusal behavior.
// PE 0x10187E50
void RD9_THUNK n2_s116(void* self RD9_THUNK_EDX, const ParticleHostString& source)
{
    ParticleHostString* dst = &psmDescriptor2<ParticleHostString>(self, 0x1E4);
    if (dst->size() != source.size() || ::memcmp(dst->c_str(), source.c_str(), source.size()) != 0)
    {
        *dst = source;
        const std::string& resourceName = ::renddx9abi::fromHostString(source);
        if (!n2_loadTexture(self, resourceName))
            *dst = ParticleHostString();
    }
}

// slots 122-125: texture-relative UV; null texture -> setters no-op, getters 0,
// width/height at (+0x21C)+0x28/+0x2C.
// PE 0x101879B0
void RD9_THUNK n2_s122(void* self RD9_THUNK_EDX, unsigned a)
{
    void* t = psmDescriptor2<void*>(self, 0x21C);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Both unsigned conversions remain live in the PE.
    if (t) F(self,0x1CC)=static_cast<float>(bf2x87::wide(a)
        /bf2x87::wide(static_cast<dx9rend::EngineTexture*>(t)->width));
#else
    if (t) F(self,0x1CC)=static_cast<float>(static_cast<double>(a)
        /static_cast<double>(static_cast<dx9rend::EngineTexture*>(t)->width));
#endif
}
// PE 0x101879F0
unsigned RD9_THUNK n2_s123(void* self RD9_THUNK_EDX)
{
    void* t = psmDescriptor2<void*>(self, 0x21C);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Keep the unsigned dimension live through truncating FISTP.
    if (t) return static_cast<unsigned>(bf2x87::toIntTruncate(
        bf2x87::wide(static_cast<dx9rend::EngineTexture*>(t)->width)*bf2x87::wide(F(self,0x1CC))));
#else
    if (t) return static_cast<unsigned>(static_cast<int>(static_cast<double>(static_cast<dx9rend::EngineTexture*>(t)->width)*F(self,0x1CC)));
#endif
    return 0;
}
// PE 0x10187A50
void RD9_THUNK n2_s124(void* self RD9_THUNK_EDX, unsigned a)
{
    void* t = psmDescriptor2<void*>(self, 0x21C);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Only this setter spills its numerator to float first.
    if (t) F(self,0x1D0)=static_cast<float>(bf2x87::wide(static_cast<float>(a))
        /bf2x87::wide(static_cast<dx9rend::EngineTexture*>(t)->height));
#else
    if (t) F(self,0x1D0)=static_cast<float>(static_cast<double>(static_cast<float>(a))
        /static_cast<double>(static_cast<dx9rend::EngineTexture*>(t)->height));
#endif
}
// PE 0x10187AA0
unsigned RD9_THUNK n2_s125(void* self RD9_THUNK_EDX)
{
    void* t = psmDescriptor2<void*>(self, 0x21C);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Keep the unsigned dimension live through truncating FISTP.
    if (t) return static_cast<unsigned>(bf2x87::toIntTruncate(
        bf2x87::wide(static_cast<dx9rend::EngineTexture*>(t)->height)*bf2x87::wide(F(self,0x1D0))));
#else
    if (t) return static_cast<unsigned>(static_cast<int>(static_cast<double>(static_cast<dx9rend::EngineTexture*>(t)->height)*F(self,0x1D0)));
#endif
    return 0;
}

// PE 0x101872B0
// slot 132: set +0x1D8 count; cache 1/count at +0x1DC.
void RD9_THUNK n2_s132(void* self RD9_THUNK_EDX, unsigned a)
{
    U(self, 0x1D8) = a;
    if (a) F(self, 0x1DC) = 1.0f / static_cast<float>(a);
    else   U(self, 0x1DC) = 0;
}

// PE 0x10187160
// slot 142: clamped 1..3.
void RD9_THUNK n2_s142(void* self RD9_THUNK_EDX, unsigned a) { if (a && a <= 3) U(self, 0x220) = a; }

// PE 0x10188540
// slot 144: radius + radius^2.
void RD9_THUNK n2_s144(void* self RD9_THUNK_EDX, PsmFloatArg a) { U(self, 0x22C) = psmFloatArgBits(a); F(self, 0x230) = F(self, 0x22C) * F(self, 0x22C); }

// PE 10187EA0 (slot 2 tail): animation enable, not the independent static-system flag, selects UV.
void n2_recomputeUV(void* self)
{
    void* res = psmDescriptor2<void*>(self, 0x21C);
    if (res)
    {
        S16(self, 0x1E0) = 0;
        S16(self, 0x1E2) = 0;
        float uv[2];
        if (Bt(self, 0x1C8)) { uv[0] = F(self, 0x1CC); uv[1] = F(self, 0x1D0); }
        else                 { uv[0] = 1.0f;           uv[1] = 1.0f; }
        const Vec2 scale = { uv[0], uv[1] };
        static_cast<IParticleSystemTemplateType2*>(self)->reserved114(scale);
    }
}

// Trail descriptor members that nothing calls or references in 1.50: unreachable in 1.50: no code or
// data references. Each body is the listing's own. The names are [CHOSEN, not recovered]: no caller
// exposes them.

// PE 0x10187300: store the +0x1DC float (the reciprocal cache of the +0x1D8 count, slot 132); the paired
// getter at 0x10187310 answers st0, so float is the [CHOSEN, not recovered] argument type.
void n2_setCountReciprocal(void* self, float value)
{
    U(self, 0x1DC) = fbits(value);
}

// PE 0x10187330: store the +0x98 float; the paired getter at 0x10187340 answers st0.
void n2_set98(void* self, float value)
{
    U(self, 0x98) = fbits(value);
}

// PE 0x10188040: load the texture named by the stored +0x1E4 name (the result is not tested), then
// recompute the UV state (tail jump to n2_recomputeUV, PE 0x10187EA0).
void n2_reloadTexture(void* self)
{
    n2_loadTexture(self, ::renddx9abi::fromHostString(psmDescriptor2<ParticleHostString>(self, 0x1E4)));
    n2_recomputeUV(self);
}

// PE 0x10188560, 0x10187C50
// slot 0 (PE 0x10188560 -> PE 0x10187C50): release the texture and destroy both host strings.
void* RD9_THUNK n2_dtor(void* self RD9_THUNK_EDX, int flags)
{
    dx9rend::EngineTexture*& texture = psmDescriptor2<dx9rend::EngineTexture*>(self, 0x21C);
    dx9rend::textureReleaseRef(texture);
    // Retail rereads the slot after the first release before destroying the embedded members.
    dx9rend::textureReleaseRef(texture);
    psmDescriptor2<ParticleHostString>(self, 0x200).~ParticleHostString();
    psmDescriptor2<ParticleHostString>(self, 0x1E4).~ParticleHostString();
    if (flags & 1)
        ::renddx9abi::engineFree(self);
    return self;
}

#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Match the shared consumer interface under the native ABI.
const void* const kVtable[] =
{
    BF2_NATIVE_DTOR(n2_dtor, complete),
    BF2_NATIVE_DTOR(n2_dtor, deleting),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getName, n2_s1),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, refreshDerivedState, n2_s2),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getSystemType, n2_s3),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitPrio, n2_s4),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitPrio, n2_s5),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setLodDistance, n2_s6),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getLodDistance, n2_s7),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setQualityLevel, n2_s8),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getQualityLevel, n2_s9),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitRadius, n2_s10),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitRadius, n2_s11),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setRadialDirection, n2_s12),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getRadialDirection, n2_s13),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setRandomRadialDirection, n2_s14),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getRandomRadialDirection, n2_s15),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitterType, n2_s16),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitRangeX, n2_s17),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitRangeX, n2_s18),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitRangeY, n2_s19),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitRangeY, n2_s20),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitRangeZ, n2_s21),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitRangeZ, n2_s22),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitFrequency, n2_s23),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitFrequency, n2_s24),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitSpeed, n2_s25),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitSpeed, n2_s26),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setPreWarmTime, n2_s27),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getPreWarmTime, n2_s28),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitTime, n2_s29),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitTime, n2_s30),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setIsLooping, n2_s31),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getIsLooping, n2_s32),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitDelay, n2_s33),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitDelay, n2_s34),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setRandomDirectionAngle, n2_s35),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getRandomDirectionAngle, n2_s36),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitDirection, n2_s37),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitDirection, n2_s38),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setRotationGraph, n2_s39),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getRotationGraph, n2_s40),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAlignRotationToSpeed, n2_null),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAlignRotationToSpeed, n2_ret1),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setRandomTimeToLive, n2_nullFloat),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getRandomTimeToLive, n2_ret0),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setTimeToLive, n2_s45),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getTimeToLive, n2_s46),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setGravity, n2_s47),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getGravity, n2_s48),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAirResistance, n2_s49),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAirResistance, n2_s50),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setRandomSpeed, n2_nullFloat),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getRandomSpeed, n2_ret0),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setGravityGraph, n2_s53),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getGravityGraph, n2_s54),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAirResistanceGraph, n2_s55),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAirResistanceGraph, n2_s56),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setEmitSpeedGraph, n2_nullGraph),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getEmitSpeedGraph, n2_s58),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setTransparencyGraph, n2_s59),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getTransparencyGraph, n2_s60),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setWindAffectionFactor, n2_s61),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getWindAffectionFactor, n2_s62),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setCollisionEnable, n2_s63),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getCollisionEnable, n2_s64),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setCollisionSpeedAlongNormal, n2_s65),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getCollisionSpeedAlongNormal, n2_s66),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setCollisionSpeedAlongPlane, n2_s67),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getCollisionSpeedAlongPlane, n2_s68),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setIsLocalSystem, n2_s69),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getIsLocalSystem, n2_s70),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setForceToWaterSurface, n2_s71),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getForceToWaterSurface, n2_s72),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setWaterSurfaceOffset, n2_s73),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getWaterSurfaceOffset, n2_s74),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setGasCloudType, n2_s75),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getGasCloudType, n2_s76),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setBlurryVisionTime, n2_s77),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getBlurryVisionTime, n2_s78),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setBlurryVisionHoldTime, n2_s79),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getBlurryVisionHoldTime, n2_s80),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setMinGasCloudRadius, n2_s81),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getMinGasCloudRadius, n2_s82),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setMaxGasCloudRadius, n2_s83),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getMaxGasCloudRadius, n2_s84),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setGasCloudRadiusTime, n2_s85),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getGasCloudRadiusTime, n2_s86),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setGasCloudDamage, n2_s87),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getGasCloudDamage, n2_s88),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setParticleMaxSize, n2_s89),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getParticleMaxSize, n2_s90),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setRandomSize, n2_s91),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getRandomSize, n2_s92),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setRandomIntensity, n2_s93),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getRandomIntensity, n2_s94),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setSizeGraph, n2_s95),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getSizeGraph, n2_s96),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAlphaCull, n2_s97),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAlphaCull, n2_s98),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setLightAffectionFactor, n2_s99),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getLightAffectionFactor, n2_s100),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setColor1, n2_s101),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getColor1, n2_s102),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setColor2, n2_s103),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getColor2, n2_s104),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, reserved105, n2_s105),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setColorBlendGraph, n2_s106),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getColorBlendGraph, n2_s107),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setLightMapIntensityOffset, n2_s108),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getLightMapIntensityOffset, n2_s109),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setDirectionalScale, n2_nullFloat),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getDirectionalScale, n2_ret0),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setIsStaticSystem, n2_s112),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getIsStaticSystem, n2_s113),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, reserved114, n2_s114),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, reserved115, n2_s115),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setTextureName, n2_s116),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getTextureName, n2_s117),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAnimationEnable, n2_s118),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAnimationEnable, n2_s119),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAnimationSpeed, n2_s120),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAnimationSpeed, n2_s121),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAnimationFrameWidth, n2_s122),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAnimationFrameWidth, n2_s123),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAnimationFrameHeight, n2_s124),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAnimationFrameHeight, n2_s125),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAnimationFrameWidthRelative, n2_s126),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAnimationFrameWidthRelative, n2_s127),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAnimationFrameHeightRelative, n2_s128),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAnimationFrameHeightRelative, n2_s129),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAnimationFrameCount, n2_s130),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAnimationFrameCount, n2_s131),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setAnimationFrameCountX, n2_s132),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getAnimationFrameCountX, n2_s133),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setFadeInFactor, n2_s134),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getFadeInFactor, n2_s135),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setFadeOutFactor, n2_s136),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getFadeOutFactor, n2_s137),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setTextureTileFactor, n2_s138),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getTextureTileFactor, n2_s139),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setTextureOffsetVelocity, n2_s140),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getTextureOffsetVelocity, n2_s141),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setNumPlanes, n2_s142),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getNumPlanes, n2_s143),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, setMinTrailVelocity, n2_s144),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType2, getMinTrailVelocity, n2_s145),
};
#else
const void* const kVtable[146] =
{
    reinterpret_cast<void*>(&n2_dtor), reinterpret_cast<void*>(&n2_s1),   reinterpret_cast<void*>(&n2_s2),   reinterpret_cast<void*>(&n2_s3),
    reinterpret_cast<void*>(&n2_s4),   reinterpret_cast<void*>(&n2_s5),   reinterpret_cast<void*>(&n2_s6),   reinterpret_cast<void*>(&n2_s7),
    reinterpret_cast<void*>(&n2_s8),   reinterpret_cast<void*>(&n2_s9),   reinterpret_cast<void*>(&n2_s10),  reinterpret_cast<void*>(&n2_s11),
    reinterpret_cast<void*>(&n2_s12),  reinterpret_cast<void*>(&n2_s13),  reinterpret_cast<void*>(&n2_s14),  reinterpret_cast<void*>(&n2_s15),
    reinterpret_cast<void*>(&n2_s16),  reinterpret_cast<void*>(&n2_s17),  reinterpret_cast<void*>(&n2_s18),  reinterpret_cast<void*>(&n2_s19),
    reinterpret_cast<void*>(&n2_s20),  reinterpret_cast<void*>(&n2_s21),  reinterpret_cast<void*>(&n2_s22),  reinterpret_cast<void*>(&n2_s23),
    reinterpret_cast<void*>(&n2_s24),  reinterpret_cast<void*>(&n2_s25),  reinterpret_cast<void*>(&n2_s26),  reinterpret_cast<void*>(&n2_s27),
    reinterpret_cast<void*>(&n2_s28),  reinterpret_cast<void*>(&n2_s29),  reinterpret_cast<void*>(&n2_s30),  reinterpret_cast<void*>(&n2_s31),
    reinterpret_cast<void*>(&n2_s32),  reinterpret_cast<void*>(&n2_s33),  reinterpret_cast<void*>(&n2_s34),  reinterpret_cast<void*>(&n2_s35),
    reinterpret_cast<void*>(&n2_s36),  reinterpret_cast<void*>(&n2_s37),  reinterpret_cast<void*>(&n2_s38),  reinterpret_cast<void*>(&n2_s39),
    reinterpret_cast<void*>(&n2_s40),  reinterpret_cast<void*>(&n2_null), reinterpret_cast<void*>(&n2_ret1), reinterpret_cast<void*>(&n2_null),
    reinterpret_cast<void*>(&n2_ret0), reinterpret_cast<void*>(&n2_s45),  reinterpret_cast<void*>(&n2_s46),  reinterpret_cast<void*>(&n2_s47),
    reinterpret_cast<void*>(&n2_s48),  reinterpret_cast<void*>(&n2_s49),  reinterpret_cast<void*>(&n2_s50),  reinterpret_cast<void*>(&n2_null),
    reinterpret_cast<void*>(&n2_ret0), reinterpret_cast<void*>(&n2_s53),  reinterpret_cast<void*>(&n2_s54),  reinterpret_cast<void*>(&n2_s55),
    reinterpret_cast<void*>(&n2_s56),  reinterpret_cast<void*>(&n2_null), reinterpret_cast<void*>(&n2_s58),  reinterpret_cast<void*>(&n2_s59),
    reinterpret_cast<void*>(&n2_s60),  reinterpret_cast<void*>(&n2_s61),  reinterpret_cast<void*>(&n2_s62),  reinterpret_cast<void*>(&n2_s63),
    reinterpret_cast<void*>(&n2_s64),  reinterpret_cast<void*>(&n2_s65),  reinterpret_cast<void*>(&n2_s66),  reinterpret_cast<void*>(&n2_s67),
    reinterpret_cast<void*>(&n2_s68),  reinterpret_cast<void*>(&n2_s69),  reinterpret_cast<void*>(&n2_s70),  reinterpret_cast<void*>(&n2_s71),
    reinterpret_cast<void*>(&n2_s72),  reinterpret_cast<void*>(&n2_s73),  reinterpret_cast<void*>(&n2_s74),  reinterpret_cast<void*>(&n2_s75),
    reinterpret_cast<void*>(&n2_s76),  reinterpret_cast<void*>(&n2_s77),  reinterpret_cast<void*>(&n2_s78),  reinterpret_cast<void*>(&n2_s79),
    reinterpret_cast<void*>(&n2_s80),  reinterpret_cast<void*>(&n2_s81),  reinterpret_cast<void*>(&n2_s82),  reinterpret_cast<void*>(&n2_s83),
    reinterpret_cast<void*>(&n2_s84),  reinterpret_cast<void*>(&n2_s85),  reinterpret_cast<void*>(&n2_s86),  reinterpret_cast<void*>(&n2_s87),
    reinterpret_cast<void*>(&n2_s88),  reinterpret_cast<void*>(&n2_s89),  reinterpret_cast<void*>(&n2_s90),  reinterpret_cast<void*>(&n2_s91),
    reinterpret_cast<void*>(&n2_s92),  reinterpret_cast<void*>(&n2_s93),  reinterpret_cast<void*>(&n2_s94),  reinterpret_cast<void*>(&n2_s95),
    reinterpret_cast<void*>(&n2_s96),  reinterpret_cast<void*>(&n2_s97),  reinterpret_cast<void*>(&n2_s98),  reinterpret_cast<void*>(&n2_s99),
    reinterpret_cast<void*>(&n2_s100), reinterpret_cast<void*>(&n2_s101), reinterpret_cast<void*>(&n2_s102), reinterpret_cast<void*>(&n2_s103),
    reinterpret_cast<void*>(&n2_s104), reinterpret_cast<void*>(&n2_s105), reinterpret_cast<void*>(&n2_s106), reinterpret_cast<void*>(&n2_s107),
    reinterpret_cast<void*>(&n2_s108), reinterpret_cast<void*>(&n2_s109), reinterpret_cast<void*>(&n2_null), reinterpret_cast<void*>(&n2_ret0),
    reinterpret_cast<void*>(&n2_s112), reinterpret_cast<void*>(&n2_s113), reinterpret_cast<void*>(&n2_s114), reinterpret_cast<void*>(&n2_s115),
    reinterpret_cast<void*>(&n2_s116), reinterpret_cast<void*>(&n2_s117), reinterpret_cast<void*>(&n2_s118), reinterpret_cast<void*>(&n2_s119),
    reinterpret_cast<void*>(&n2_s120), reinterpret_cast<void*>(&n2_s121), reinterpret_cast<void*>(&n2_s122), reinterpret_cast<void*>(&n2_s123),
    reinterpret_cast<void*>(&n2_s124), reinterpret_cast<void*>(&n2_s125), reinterpret_cast<void*>(&n2_s126), reinterpret_cast<void*>(&n2_s127),
    reinterpret_cast<void*>(&n2_s128), reinterpret_cast<void*>(&n2_s129), reinterpret_cast<void*>(&n2_s130), reinterpret_cast<void*>(&n2_s131),
    reinterpret_cast<void*>(&n2_s132), reinterpret_cast<void*>(&n2_s133), reinterpret_cast<void*>(&n2_s134), reinterpret_cast<void*>(&n2_s135),
    reinterpret_cast<void*>(&n2_s136), reinterpret_cast<void*>(&n2_s137), reinterpret_cast<void*>(&n2_s138), reinterpret_cast<void*>(&n2_s139),
    reinterpret_cast<void*>(&n2_s140), reinterpret_cast<void*>(&n2_s141), reinterpret_cast<void*>(&n2_s142), reinterpret_cast<void*>(&n2_s143),
    reinterpret_cast<void*>(&n2_s144), reinterpret_cast<void*>(&n2_s145),
};
#endif

// sub_10187350 - init helper.
void initType2Defaults(void* self)
{
    Bt(self, 0x8C) = 0;
    F (self, 0x90)  = 0.03f;
    F (self, 0x98)  = -1.0f;
    U (self, 0x0C)  = 0u;
    U (self, 0x224) = 0u;
    U (self, 0x20)=0u; U(self,0x24)=0u; U(self,0x28)=0u; Bt(self,0x2C)=0;
    U (self, 0x34)=0u; U(self,0x38)=0u; U(self,0x3C)=0u; Bt(self,0x40)=0;
    U (self, 0x48)=0u; U(self,0x4C)=0u; U(self,0x50)=0u; Bt(self,0x54)=0;
    F (self, 0x5C)  = 360.0f;
    U (self, 0x60)  = 0u;
    F (self, 0x10)  = 32.0f;
    F (self, 0x170) = 0.03125f;
    U (self, 0x64)  = 2u;
    U (self, 0x68)  = 2u;
    U (self, 0x6C)  = 0u;
    F (self, 0x174) = 1.0f;
    U (self, 0x1C)  = 0u;
    F (self, 0x14)  = 10.0f;
    F (self, 0x18)  = 10.0f;
    U (self, 0x164)=0u; U(self,0x168)=0u; U(self,0x178)=0u; Bt(self,0x16C)=0;
    U (self, 0x220) = 3u;
    F (self, 0x94)  = 0.05f;
    U (self, 0x1BC)=0u; U(self,0x88)=0u;
    U (self, 0x78)  = 0xC11D1EB8u;
    U (self, 0x7C)  = 0u;
    F (self, 0x80)  = 0.2f;
    F (self, 0x84)  = 1.0f;
    U (self, 0x14C)=0u; U(self,0x150)=0u; U(self,0x154)=0u; U(self,0x158)=0u; U(self,0x15C)=0u; U(self,0x160)=0u;

    n2_s99 (self RD9_THUNK_ARG, psmFloatArgFromBits(static_cast<unsigned>(-1082130432)));   // +0x17C = -1.0
    { float v[3] = { 1.0f, 1.0f, 1.0f }; n2_s101(self RD9_THUNK_ARG, v); }
    { float v[3] = { 1.0f, 1.0f, 1.0f }; n2_s103(self RD9_THUNK_ARG, v); }
    { float v[3] = { 1.0f, 1.0f, 1.0f }; n2_s105(self RD9_THUNK_ARG, v); }

    Bt(self, 0x9C)=0; F(self,0xA0)=0.7f; F(self,0xA4)=0.8f; U(self,0xA8)=2u;
    n2_s89 (self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(1.0f)));   // +0xB8 & +0x1B8 = 1.0
    n2_s108(self RD9_THUNK_ARG, psmFloatArgFromBits(0u));            // +0xB4 = 0
    n2_s45 (self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(4.0f)));   // +0x70 = 4.0, +0x74 = 0.25
    n2_null(self RD9_THUNK_ARG, 0u);            // slot 43 nullsub (one float argument)
    { float v[4] = { 0.0f, 0.0f, 1.0f, 0.0f }; n2_s106(self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n2_s59 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n2_s95 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n2_s53 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n2_s55 (self RD9_THUNK_ARG, v); }

    U (self, 0x1AC)=0u; F(self,0x1B0)=1.0f; U(self,0x1B4)=0u; Bt(self,0x1C0)=0; Bt(self,0x228)=0;
    { ParticleHostString empty; psmDescriptor2<ParticleHostString>(self, 0x1E4) = empty; }
    F (self, 0x1C4)=1.0f; Bt(self,0x1C8)=0; U(self,0x1CC)=0u; U(self,0x1D0)=0u; U(self,0x1D4)=0u; U(self,0x1D8)=0u; U(self,0x1DC)=0u;

    { float v[2] = { 1.0f, 1.0f }; n2_s114(self RD9_THUNK_ARG, v); }   // +0xAC/+0xB0 = 1.0
    n2_s134(self RD9_THUNK_ARG, psmFloatArgFromBits(0u));            // +0xBC = 0
    n2_s136(self RD9_THUNK_ARG, psmFloatArgFromBits(0u));            // +0xC0 = 0
    n2_s138(self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(1.0f)));   // +0xC4 = 1.0

    F (self, 0x238)=20.0f; F(self,0x23C)=20.0f; U(self,0x22C)=0u; U(self,0x230)=0u; U(self,0x234)=0u;
    F (self, 0x240)=3.0f; F(self,0x244)=10.0f; F(self,0x248)=3.0f; F(self,0x24C)=2.0f;
}

} // anonymous namespace

// geom_TrailSystemTemplate_ctor, 0x10187F00.
void* constructParticleSystemType2(void* mem, const void* nameString)
{
    ::memset(mem, 0, PSM_DESCRIPTOR_SIZE(0x250u));
    *static_cast<const void* const**>(mem) = kVtable;

    U(mem, 0x24) = 1065353216u;   // 1.0
    U(mem, 0x30) = 1973272912u;   // range sentinel
    U(mem, 0x38) = 1065353216u;
    U(mem, 0x44) = 1973272912u;
    U(mem, 0x4C) = 1065353216u;
    U(mem, 0x58) = 1973272912u;
    psmZeroTemplateBlock(&psmDescriptor2<unsigned char>(mem, 0xAC));   // redundant after the memset

    new (&psmDescriptor2<unsigned char>(mem, 0x1E4)) ParticleHostString();
    new (&psmDescriptor2<unsigned char>(mem, 0x200)) ParticleHostString(
        ::renddx9abi::toHostString(*static_cast<const std::string*>(nameString)));

    initType2Defaults(mem);
    return mem;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
