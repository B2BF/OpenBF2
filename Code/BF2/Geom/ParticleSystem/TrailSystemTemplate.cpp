// Type-2 trail descriptor with property, texture, and UV state.
// Contains no simulation or per-particle update function.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "ParticleRawLayout.h"
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

extern LightManager* g_LightManager; // Retail refresh dereferences this when affection is negative/NaN.

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
#define PT_SETVEC(name, off, n) void RD9_THUNK name(void* s RD9_THUNK_EDX, const float* v){ for (int i=0;i<(n);++i) F(s,(off)+4u*i)=v[i]; }
#define PT_GETVEC(name, off, n) void* RD9_THUNK name(void* s RD9_THUNK_EDX, float* o){ for (int i=0;i<(n);++i) o[i]=F(s,(off)+4u*i); return o; }
#define PT_VEC3A(name, off) void RD9_THUNK name(void* s RD9_THUNK_EDX, PSM_VEC3_PARAMS){ U(s,(off))=PSM_VEC3_X; U(s,(off)+4)=PSM_VEC3_Y; U(s,(off)+8)=PSM_VEC3_Z; }

void RD9_THUNK n2_s2 (void* s RD9_THUNK_EDX);
void RD9_THUNK n2_s45(void* s RD9_THUNK_EDX, PsmFloatArg a);
void            n2_recomputeUV(void* self);

// shared trivial slots
// PE 100E13B0 is RET 4, shared by one-argument setters 41/43/51/57/110.
void     RD9_THUNK n2_null(void* RD9_THUNK_EDX, unsigned) { }
float    RD9_THUNK n2_ret0(void* RD9_THUNK_EDX) { return 0.0f; }
unsigned RD9_THUNK n2_ret1(void* RD9_THUNK_EDX) { return 1; }

// simple slots
const ParticleHostString& RD9_THUNK n2_s1(void* self RD9_THUNK_EDX)
{ return psmDescriptor2<ParticleHostString>(self, 0x200); }
PT_SET (n2_s4,  0x64)
PT_GETD(n2_s5,  0x64)
PT_SET (n2_s6,  0x68)
PT_GETD(n2_s7,  0x68)
PT_SET (n2_s8,  0x6C)
PT_GETD(n2_s9,  0x6C)
PT_VEC3A(n2_s10, 0x14)
PT_GETVEC(n2_s11, 0x14, 3)
PT_SETFLT(n2_s12, 0x60)
PT_GETF(n2_s13, 0x60)
PT_SETFLT(n2_s14, 0x5C)
PT_GETF(n2_s15, 0x5C)
PT_SET (n2_s16, 0x224)
PT_SETVEC(n2_s17, 0x20, 5)
PT_LEA (n2_s18, 0x20)
PT_SETVEC(n2_s19, 0x34, 5)
PT_LEA (n2_s20, 0x34)
PT_SETVEC(n2_s21, 0x48, 5)
PT_LEA (n2_s22, 0x48)
PT_GETF(n2_s24, 0x10)
PT_SETFLT(n2_s25, 0x164)
PT_GETF(n2_s26, 0x164)
PT_SETFLT(n2_s27, 0x0C)
PT_GETF(n2_s28, 0x0C)
PT_SETFLT(n2_s29, 0x168)
PT_GETF(n2_s30, 0x168)
PT_SETB(n2_s31, 0x16C)
PT_GETB(n2_s32, 0x16C)
PT_SETFLT(n2_s33, 0x178)
PT_GETF(n2_s34, 0x178)
PT_SETVEC(n2_s35, 0x158, 3)
PT_GETVEC(n2_s36, 0x158, 3)
PT_GETVEC(n2_s38, 0x1AC, 3)
PT_SETVEC(n2_s39, 0x11C, 4)
PT_LEA (n2_s40, 0x11C)
PT_GETF(n2_s46, 0x70)
PT_SETFLT(n2_s47, 0x78)
PT_GETF(n2_s48, 0x78)
PT_SETFLT(n2_s49, 0x7C)
PT_GETF(n2_s50, 0x7C)
PT_SETVEC(n2_s53, 0x12C, 4)
PT_LEA (n2_s54, 0x12C)
PT_SETVEC(n2_s55, 0x13C, 4)
PT_LEA (n2_s56, 0x13C)
PT_SETVEC(n2_s59, 0xFC, 4)
PT_LEA (n2_s60, 0xFC)
PT_SETFLT(n2_s61, 0x88)
PT_GETF(n2_s62, 0x88)
PT_SETB(n2_s63, 0x9C)
PT_GETB(n2_s64, 0x9C)
PT_SETFLT(n2_s65, 0xA0)
PT_GETF(n2_s66, 0xA0)
PT_SETFLT(n2_s67, 0xA4)
PT_GETF(n2_s68, 0xA4)
PT_SETB(n2_s69, 0x1C0)
PT_GETB(n2_s70, 0x1C0)
PT_SETB(n2_s71, 0x8C)
PT_GETB(n2_s72, 0x8C)
PT_SETFLT(n2_s73, 0x90)
PT_GETF(n2_s74, 0x90)
PT_SET (n2_s75, 0x234)
PT_GETD(n2_s76, 0x234)
PT_SETFLT(n2_s77, 0x238)
PT_GETF(n2_s78, 0x238)
PT_SETFLT(n2_s79, 0x23C)
PT_GETF(n2_s80, 0x23C)
PT_SETFLT(n2_s81, 0x240)
PT_GETF(n2_s82, 0x240)
PT_SETFLT(n2_s83, 0x244)
PT_GETF(n2_s84, 0x244)
PT_SETFLT(n2_s85, 0x248)
PT_GETF(n2_s86, 0x248)
PT_SETFLT(n2_s87, 0x24C)
PT_GETF(n2_s88, 0x24C)
PT_GETF(n2_s90, 0x1B8)
PT_SETFLT(n2_s91, 0x14C)
PT_GETF(n2_s92, 0x14C)
PT_SETFLT(n2_s93, 0x150)
PT_GETF(n2_s94, 0x150)
PT_SETVEC(n2_s95, 0x10C, 4)
PT_LEA (n2_s96, 0x10C)
PT_SETFLT(n2_s97, 0x80)
PT_GETF(n2_s98, 0x80)
PT_SETFLT(n2_s99, 0x17C)
PT_GETF(n2_s100, 0x17C)
PT_GETVEC(n2_s102, 0x180, 3)
PT_SETVEC(n2_s106, 0xEC, 4)
PT_LEA (n2_s107, 0xEC)
PT_SETFLT(n2_s108, 0xB4)
PT_GETF(n2_s109, 0xB4)
PT_SETB(n2_s112, 0x228)
PT_GETB(n2_s113, 0x228)
PT_SETVEC(n2_s114, 0xAC, 2)
PT_GETVEC(n2_s115, 0xAC, 2)
const ParticleHostString& RD9_THUNK n2_s117(void* self RD9_THUNK_EDX)
{ return psmDescriptor2<ParticleHostString>(self, 0x1E4); }
PT_SETB(n2_s118, 0x1C8)
PT_GETB(n2_s119, 0x1C8)
PT_SETFLT(n2_s120, 0x1C4)
PT_GETF(n2_s121, 0x1C4)
PT_SETFLT(n2_s126, 0x1CC)
PT_GETF(n2_s127, 0x1CC)
PT_SETFLT(n2_s128, 0x1D0)
PT_GETF(n2_s129, 0x1D0)
PT_SET (n2_s130, 0x1D4)
PT_GETD(n2_s131, 0x1D4)
PT_GETD(n2_s133, 0x1D8)
PT_SETFLT(n2_s134, 0xBC)
PT_GETF(n2_s135, 0xBC)
PT_SETFLT(n2_s136, 0xC0)
PT_GETF(n2_s137, 0xC0)
PT_SETFLT(n2_s138, 0xC4)
PT_GETF(n2_s139, 0xC4)
PT_SETFLT(n2_s140, 0xC8)
PT_GETF(n2_s141, 0xC8)
PT_GETD(n2_s143, 0x220)
PT_GETF(n2_s145, 0x22C)

// complex slots

unsigned RD9_THUNK n2_s3(void* RD9_THUNK_EDX) { return 2; }   // emitter type id

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

void RD9_THUNK n2_s23(void* self RD9_THUNK_EDX, PsmFloatArg a) { U(self, 0x10) = psmFloatArgBits(a); F(self, 0x170) = 1.0f / F(self, 0x10); }

void RD9_THUNK n2_s37(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1AC) = v[0]; F(self, 0x1B0) = v[1]; F(self, 0x1B4) = v[2];
    float* p = &psmDescriptor2<float>(self, 0x1AC);
    float s = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
    if (fabsf(s - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(s) < FLT_EPSILON) { p[0] = p[1] = p[2] = 0.0f; }
        else { float inv = 1.0f / sqrtf(s); p[0] *= inv; p[1] *= inv; p[2] *= inv; }
    }
}

void RD9_THUNK n2_s45(void* self RD9_THUNK_EDX, PsmFloatArg a) { U(self, 0x70) = psmFloatArgBits(a); F(self, 0x74) = 1.0f / F(self, 0x70); }

// a virtual tail call to slot 56, retaining possible overrides.
const Vec4& RD9_THUNK n2_s58(void* self RD9_THUNK_EDX)
{ return static_cast<IParticleSystemTemplateType2*>(self)->getAirResistanceGraph(); }

// slot 89: dual write.
void RD9_THUNK n2_s89(void* self RD9_THUNK_EDX, PsmFloatArg a) { U(self, 0xB8) = psmFloatArgBits(a); U(self, 0x1B8) = psmFloatArgBits(a); }

// slot 101: Vec3 at +0x180 + mirror +0xCC.
void RD9_THUNK n2_s101(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x180) = v[0]; F(self, 0x184) = v[1]; F(self, 0x188) = v[2];
    F(self, 0xCC)  = v[0]; F(self, 0xD0)  = v[1]; F(self, 0xD4)  = v[2];
}

// slot 103: Vec3 -> Vec4 {x,y,z,1} at +0x18C + mirror +0xDC.
void RD9_THUNK n2_s103(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x18C) = v[0]; F(self, 0x190) = v[1]; F(self, 0x194) = v[2]; F(self, 0x198) = 1.0f;
    F(self, 0xDC)  = v[0]; F(self, 0xE0)  = v[1]; F(self, 0xE4)  = v[2]; F(self, 0xE8)  = 1.0f;
}

// slot 104: get the +0x18C color as Vec3.
void* RD9_THUNK n2_s104(void* self RD9_THUNK_EDX, float* out)
{
    out[0] = F(self, 0x18C); out[1] = F(self, 0x190); out[2] = F(self, 0x194);
    return out;
}

// slot 105: Vec3 -> Vec4 {x,y,z,1} at +0x19C (no mirror).
void RD9_THUNK n2_s105(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x19C) = v[0]; F(self, 0x1A0) = v[1]; F(self, 0x1A4) = v[2]; F(self, 0x1A8) = 1.0f;
}

// unlike the sprite/NSA loader, trails use a direct texture without an atlas lookup.
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
        // [TEMP] [CHOSEN, not recovered] Only the configurable diagnostic dispatch remains.
        // [RETIRE-WHEN: particle-template-texture-debug-dispatch]
        std::fprintf(stderr, "[FRONTIER] dice::hfe::geom::TrailSystemTemplate::setTextureName (Debug type 6, line 169): File %s not found!\n", name.c_str());
        std::fflush(stderr);
        std::abort();
    }
    return texture != 0;
}

// Keep the authored name copy before loading and the empty-name refusal behavior.
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
void RD9_THUNK n2_s122(void* self RD9_THUNK_EDX, unsigned a)
{
    void* t = psmDescriptor2<void*>(self, 0x21C);
    if (t) F(self, 0x1CC) = static_cast<float>(a) / static_cast<float>(static_cast<dx9rend::EngineTexture*>(t)->width);
}
unsigned RD9_THUNK n2_s123(void* self RD9_THUNK_EDX)
{
    void* t = psmDescriptor2<void*>(self, 0x21C);
    if (t) return static_cast<unsigned>(static_cast<int>(static_cast<float>(static_cast<dx9rend::EngineTexture*>(t)->width) * F(self, 0x1CC)));
    return 0;
}
void RD9_THUNK n2_s124(void* self RD9_THUNK_EDX, unsigned a)
{
    void* t = psmDescriptor2<void*>(self, 0x21C);
    if (t) F(self, 0x1D0) = static_cast<float>(a) / static_cast<float>(static_cast<dx9rend::EngineTexture*>(t)->height);
}
unsigned RD9_THUNK n2_s125(void* self RD9_THUNK_EDX)
{
    void* t = psmDescriptor2<void*>(self, 0x21C);
    if (t) return static_cast<unsigned>(static_cast<int>(static_cast<float>(static_cast<dx9rend::EngineTexture*>(t)->height) * F(self, 0x1D0)));
    return 0;
}

// slot 132: set +0x1D8 count; cache 1/count at +0x1DC.
void RD9_THUNK n2_s132(void* self RD9_THUNK_EDX, unsigned a)
{
    U(self, 0x1D8) = a;
    if (a) F(self, 0x1DC) = 1.0f / static_cast<float>(a);
    else   U(self, 0x1DC) = 0;
}

// slot 142: clamped 1..3.
void RD9_THUNK n2_s142(void* self RD9_THUNK_EDX, unsigned a) { if (a && a <= 3) U(self, 0x220) = a; }

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

// slot 0 (PE 10188560 -> 10187C50): release the texture and destroy both host strings.
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
    // sub_10188150(this+0xAC): 112-byte zero-fill - covered by the memset.

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
