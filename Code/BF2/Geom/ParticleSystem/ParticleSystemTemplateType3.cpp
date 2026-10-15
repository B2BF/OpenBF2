// Emitter descriptor whose child template is resolved by name through the manager.
// Every emitted particle creates an instance of that child template.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "ParticleRawLayout.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include <string>

#include <new>
#include <string.h>
#include <math.h>
#include <float.h>

namespace dice {
namespace hfe {
namespace geom {

// G1: the manager's find-by-name (PE 100D3410), exported by ParticleSystemManager.cpp.
class IParticleSystemTemplate;
IParticleSystemTemplate* particleSystemManagerFindTemplateByName(const char* name);

namespace {

// [CHOSEN, not recovered] BF2 passes std::string through the descriptor interface. The two strings
// are host strings in their original 28-byte spans (x64: ParticleRawLayout.h tail slots) (+0x6C..+0x87 and
// +0x16C..+0x187), exactly as the type-0/1/2/4 descriptors carry theirs, so every later offset is kept.
using ParticleHostString = ::renddx9abi::HostString;
static_assert(sizeof(ParticleHostString) <= kPsmDescriptorStringSpan, "particle host string exceeds its reserved span");

inline unsigned&      U (void* p, unsigned off) { return psmDescriptor3<unsigned>(p, off); }
inline float&         F (void* p, unsigned off) { return psmDescriptor3<float>(p, off); }
inline unsigned char& Bt(void* p, unsigned off) { return psmDescriptor3<unsigned char>(p, off); }
inline unsigned       fbits(float f) { unsigned u; ::memcpy(&u, &f, 4); return u; }

#define PT_SET(name, off)  void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ U(s,(off)) = v; }
#define PT_SETFLT(name, off) void RD9_THUNK name(void* s RD9_THUNK_EDX, PsmFloatArg v){ U(s,(off)) = psmFloatArgBits(v); }
#define PT_GETF(name, off) float    RD9_THUNK name(void* s RD9_THUNK_EDX){ return F(s,(off)); }
#define PT_GETD(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return U(s,(off)); }
#define PT_SETB(name, off) void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ Bt(s,(off)) = static_cast<unsigned char>(v); }
#define PT_GETB(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return Bt(s,(off)); }
#define PT_LEA(name, off)  void*    RD9_THUNK name(void* s RD9_THUNK_EDX){ return &psmDescriptor3<unsigned char>(s, (off)); }
#define PT_SETVEC(name, off, n) void RD9_THUNK name(void* s RD9_THUNK_EDX, const float* v){ for (int i=0;i<(n);++i) F(s,(off)+4u*i)=v[i]; }
#define PT_GETVEC(name, off, n) void* RD9_THUNK name(void* s RD9_THUNK_EDX, float* o){ for (int i=0;i<(n);++i) o[i]=F(s,(off)+4u*i); return o; }
#define PT_VEC3A(name, off) void RD9_THUNK name(void* s RD9_THUNK_EDX, PSM_VEC3_PARAMS){ U(s,(off))=PSM_VEC3_X; U(s,(off)+4)=PSM_VEC3_Y; U(s,(off)+8)=PSM_VEC3_Z; }

// simple slots
PT_LEA (n3_s1,  0x16C)
PT_SET (n3_s4,  0x60)
PT_GETD(n3_s5,  0x60)
PT_SET (n3_s6,  0x64)
PT_GETD(n3_s7,  0x64)
PT_SET (n3_s8,  0x68)
PT_GETD(n3_s9,  0x68)
PT_VEC3A(n3_s10, 0x10)
PT_GETVEC(n3_s11, 0x10, 3)
PT_SETFLT(n3_s12, 0x5C)
PT_GETF(n3_s13, 0x5C)
PT_SETFLT(n3_s14, 0x58)
PT_GETF(n3_s15, 0x58)
PT_SET (n3_s16, 0x188)
PT_SETVEC(n3_s17, 0x1C, 5)
PT_LEA (n3_s18, 0x1C)
PT_SETVEC(n3_s19, 0x30, 5)
PT_LEA (n3_s20, 0x30)
PT_SETVEC(n3_s21, 0x44, 5)
PT_LEA (n3_s22, 0x44)
PT_GETF(n3_s24, 0x0C)
PT_SETFLT(n3_s25, 0x140)
PT_GETF(n3_s26, 0x140)
PT_SETFLT(n3_s27, 0x08)
PT_GETF(n3_s28, 0x08)
PT_SETFLT(n3_s29, 0x144)
PT_GETF(n3_s30, 0x144)
PT_SETB(n3_s31, 0x148)
PT_GETB(n3_s32, 0x148)
PT_SETFLT(n3_s33, 0x154)
PT_GETF(n3_s34, 0x154)
PT_SETVEC(n3_s35, 0x134, 3)
PT_GETVEC(n3_s36, 0x134, 3)
PT_GETVEC(n3_s38, 0x158, 3)
PT_SETVEC(n3_s39, 0xD4, 4)
PT_LEA (n3_s40, 0xD4)
PT_SETB(n3_s41, 0x164)
PT_GETB(n3_s42, 0x164)
PT_SETFLT(n3_s43, 0x8C)
PT_GETF(n3_s44, 0x8C)
PT_GETF(n3_s46, 0x90)
PT_SETFLT(n3_s47, 0x98)
PT_GETF(n3_s48, 0x98)
PT_SETFLT(n3_s49, 0x9C)
PT_GETF(n3_s50, 0x9C)
PT_SETFLT(n3_s51, 0x130)
PT_GETF(n3_s52, 0x130)
PT_SETVEC(n3_s53, 0xE4, 4)
PT_LEA (n3_s54, 0xE4)
PT_SETVEC(n3_s55, 0xF4, 4)
PT_LEA (n3_s56, 0xF4)
PT_SETVEC(n3_s57, 0x104, 4)
PT_LEA (n3_s58, 0x104)
PT_SETVEC(n3_s59, 0x114, 4)
PT_LEA (n3_s60, 0x114)
PT_SETFLT(n3_s61, 0xB8)
PT_GETF(n3_s62, 0xB8)
PT_SETB(n3_s63, 0xC4)
PT_GETB(n3_s64, 0xC4)
PT_SETFLT(n3_s65, 0xC8)
PT_GETF(n3_s66, 0xC8)
PT_SETFLT(n3_s67, 0xCC)
PT_GETF(n3_s68, 0xCC)
PT_SETB(n3_s69, 0x165)
PT_GETB(n3_s70, 0x165)
PT_SETB(n3_s71, 0xBC)
PT_GETB(n3_s72, 0xBC)
PT_SETFLT(n3_s73, 0xC0)
PT_GETF(n3_s74, 0xC0)
PT_SET (n3_s75, 0x18C)
PT_GETD(n3_s76, 0x18C)
PT_SETFLT(n3_s77, 0x190)
PT_GETF(n3_s78, 0x190)
PT_SETFLT(n3_s79, 0x194)
PT_GETF(n3_s80, 0x194)
PT_SETFLT(n3_s81, 0x198)
PT_GETF(n3_s82, 0x198)
PT_SETFLT(n3_s83, 0x19C)
PT_GETF(n3_s84, 0x19C)
PT_SETFLT(n3_s85, 0x1A0)
PT_GETF(n3_s86, 0x1A0)
PT_SETFLT(n3_s87, 0x1A4)
PT_GETF(n3_s88, 0x1A4)
PT_LEA (n3_s90, 0x6C)
PT_VEC3A(n3_s91, 0x124)
PT_GETVEC(n3_s92, 0x124, 3)
PT_VEC3A(n3_s93, 0xA0)
PT_GETVEC(n3_s94, 0xA0, 3)
PT_VEC3A(n3_s95, 0xAC)
PT_GETVEC(n3_s96, 0xAC, 3)
PT_SETFLT(n3_s97, 0x168)
PT_GETF(n3_s98, 0x168)

// complex slots

unsigned RD9_THUNK n3_s3(void* RD9_THUNK_EDX) { return 3; }   // emitter type id

// slot 2 (0x101865F0): mode@+0x60 -> scale @+0x150 {0.5,0.7,1.0}; inverse lifetime @+0x14C. No
// external calls (type 3 has no LightManager dependency).
void RD9_THUNK n3_s2(void* self RD9_THUNK_EDX)
{
    unsigned mode = U(self, 0x60);
    F(self, 0x150) = (mode == 0) ? 0.5f : (mode == 1) ? 0.7f : 1.0f;
    if (F(self, 0x0C) >= 1.1920929e-7f) F(self, 0x14C) = 1.0f / F(self, 0x0C);
    else                                U(self, 0x14C) = 0;
}

// slot 23 (0x10186DB0): setLifetime - store +0x0C, cache 1/lifetime at +0x14C (unconditional divide).
void RD9_THUNK n3_s23(void* self RD9_THUNK_EDX, PsmFloatArg a)
{
    U(self, 0x0C) = psmFloatArgBits(a);
    F(self, 0x14C) = 1.0f / F(self, 0x0C);
}

// slot 37 (0x10186A10): store Vec3 at +0x158, normalize in place.
void RD9_THUNK n3_s37(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x158) = v[0]; F(self, 0x15C) = v[1]; F(self, 0x160) = v[2];
    float* p = &psmDescriptor3<float>(self, 0x158);
    float s = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
    if (fabsf(s - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(s) < FLT_EPSILON) { p[0] = p[1] = p[2] = 0.0f; }
        else { float inv = 1.0f / sqrtf(s); p[0] *= inv; p[1] *= inv; p[2] *= inv; }
    }
}

// slot 45 (0x10186650): store +0x90; cache 1/x at +0x94 when positive.
void RD9_THUNK n3_s45(void* self RD9_THUNK_EDX, PsmFloatArg a)
{
    U(self, 0x90) = psmFloatArgBits(a);
    if (F(self, 0x90) > 0.0f) F(self, 0x94) = 1.0f / F(self, 0x90);
}

// slot 89 (0x101868F0, 83 B): setParticleSystemTemplate - from every
// instruction. Assign the +0x6C name, drop the cached child (+0x88) when it is set, then - for a
// non-empty name (`cmp [esi+80h], 0`, the VC7.1 size word) - look the child up at once: a BY-value
// copy of the name to 100D3410 on the manager (ds:1024B900), whose answer lands in +0x88. A child
// defined later answers null here, and the instance constructor's re-resolve (10186540) picks it up.
void RD9_THUNK n3_s89(void* self RD9_THUNK_EDX, const ParticleHostString& name)
{
    ParticleHostString& dst = psmDescriptor3<ParticleHostString>(self, 0x6C);
    dst = name;
    IParticleSystemTemplate*& child = psmDescriptor3<IParticleSystemTemplate*>(self, 0x88);
    if (child)
        child = 0;
    if (!dst.empty())
        child = particleSystemManagerFindTemplateByName(dst.c_str());
}

// slot 0 (0x10187080): destroy the two strings, restore base vtable, free-if-flag.
void* RD9_THUNK n3_dtor(void* self RD9_THUNK_EDX, int flags)
{
    psmDescriptor3<ParticleHostString>(self, 0x16C).~ParticleHostString();
    psmDescriptor3<ParticleHostString>(self, 0x6C).~ParticleHostString();
    if (flags & 1)
        ::renddx9abi::engineFree(self);
    return self;
}

const void* const kVtable[99] =
{
    reinterpret_cast<void*>(&n3_dtor), reinterpret_cast<void*>(&n3_s1),  reinterpret_cast<void*>(&n3_s2),  reinterpret_cast<void*>(&n3_s3),
    reinterpret_cast<void*>(&n3_s4),   reinterpret_cast<void*>(&n3_s5),  reinterpret_cast<void*>(&n3_s6),  reinterpret_cast<void*>(&n3_s7),
    reinterpret_cast<void*>(&n3_s8),   reinterpret_cast<void*>(&n3_s9),  reinterpret_cast<void*>(&n3_s10), reinterpret_cast<void*>(&n3_s11),
    reinterpret_cast<void*>(&n3_s12),  reinterpret_cast<void*>(&n3_s13), reinterpret_cast<void*>(&n3_s14), reinterpret_cast<void*>(&n3_s15),
    reinterpret_cast<void*>(&n3_s16),  reinterpret_cast<void*>(&n3_s17), reinterpret_cast<void*>(&n3_s18), reinterpret_cast<void*>(&n3_s19),
    reinterpret_cast<void*>(&n3_s20),  reinterpret_cast<void*>(&n3_s21), reinterpret_cast<void*>(&n3_s22), reinterpret_cast<void*>(&n3_s23),
    reinterpret_cast<void*>(&n3_s24),  reinterpret_cast<void*>(&n3_s25), reinterpret_cast<void*>(&n3_s26), reinterpret_cast<void*>(&n3_s27),
    reinterpret_cast<void*>(&n3_s28),  reinterpret_cast<void*>(&n3_s29), reinterpret_cast<void*>(&n3_s30), reinterpret_cast<void*>(&n3_s31),
    reinterpret_cast<void*>(&n3_s32),  reinterpret_cast<void*>(&n3_s33), reinterpret_cast<void*>(&n3_s34), reinterpret_cast<void*>(&n3_s35),
    reinterpret_cast<void*>(&n3_s36),  reinterpret_cast<void*>(&n3_s37), reinterpret_cast<void*>(&n3_s38), reinterpret_cast<void*>(&n3_s39),
    reinterpret_cast<void*>(&n3_s40),  reinterpret_cast<void*>(&n3_s41), reinterpret_cast<void*>(&n3_s42), reinterpret_cast<void*>(&n3_s43),
    reinterpret_cast<void*>(&n3_s44),  reinterpret_cast<void*>(&n3_s45), reinterpret_cast<void*>(&n3_s46), reinterpret_cast<void*>(&n3_s47),
    reinterpret_cast<void*>(&n3_s48),  reinterpret_cast<void*>(&n3_s49), reinterpret_cast<void*>(&n3_s50), reinterpret_cast<void*>(&n3_s51),
    reinterpret_cast<void*>(&n3_s52),  reinterpret_cast<void*>(&n3_s53), reinterpret_cast<void*>(&n3_s54), reinterpret_cast<void*>(&n3_s55),
    reinterpret_cast<void*>(&n3_s56),  reinterpret_cast<void*>(&n3_s57), reinterpret_cast<void*>(&n3_s58), reinterpret_cast<void*>(&n3_s59),
    reinterpret_cast<void*>(&n3_s60),  reinterpret_cast<void*>(&n3_s61), reinterpret_cast<void*>(&n3_s62), reinterpret_cast<void*>(&n3_s63),
    reinterpret_cast<void*>(&n3_s64),  reinterpret_cast<void*>(&n3_s65), reinterpret_cast<void*>(&n3_s66), reinterpret_cast<void*>(&n3_s67),
    reinterpret_cast<void*>(&n3_s68),  reinterpret_cast<void*>(&n3_s69), reinterpret_cast<void*>(&n3_s70), reinterpret_cast<void*>(&n3_s71),
    reinterpret_cast<void*>(&n3_s72),  reinterpret_cast<void*>(&n3_s73), reinterpret_cast<void*>(&n3_s74), reinterpret_cast<void*>(&n3_s75),
    reinterpret_cast<void*>(&n3_s76),  reinterpret_cast<void*>(&n3_s77), reinterpret_cast<void*>(&n3_s78), reinterpret_cast<void*>(&n3_s79),
    reinterpret_cast<void*>(&n3_s80),  reinterpret_cast<void*>(&n3_s81), reinterpret_cast<void*>(&n3_s82), reinterpret_cast<void*>(&n3_s83),
    reinterpret_cast<void*>(&n3_s84),  reinterpret_cast<void*>(&n3_s85), reinterpret_cast<void*>(&n3_s86), reinterpret_cast<void*>(&n3_s87),
    reinterpret_cast<void*>(&n3_s88),  reinterpret_cast<void*>(&n3_s89), reinterpret_cast<void*>(&n3_s90), reinterpret_cast<void*>(&n3_s91),
    reinterpret_cast<void*>(&n3_s92),  reinterpret_cast<void*>(&n3_s93), reinterpret_cast<void*>(&n3_s94), reinterpret_cast<void*>(&n3_s95),
    reinterpret_cast<void*>(&n3_s96),  reinterpret_cast<void*>(&n3_s97), reinterpret_cast<void*>(&n3_s98),
};

// sub_101866B0 - init helper: defaults + setter calls, in shipped order.
void initType3Defaults(void* self)
{
    U (self, 0x08) = 0u;
    U (self, 0x188) = 0u;
    Bt(self, 0xBC) = 0;
    F (self, 0xC0) = 0.03f;
    U (self, 0x1C)=0u; U(self,0x20)=0u; U(self,0x24)=0u; Bt(self,0x28)=0;
    U (self, 0x30)=0u; U(self,0x34)=0u; U(self,0x38)=0u; Bt(self,0x3C)=0;
    U (self, 0x44)=0u; U(self,0x48)=0u; U(self,0x4C)=0u; Bt(self,0x50)=0;
    F (self, 0x58) = 360.0f;
    U (self, 0x5C) = 0u;

    n3_s45(self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(4.0f)));         // +0x90 = 4.0, +0x94 = 0.25
    n3_s43(self RD9_THUNK_ARG, psmFloatArgFromBits(0u));                   // +0x8C = 0

    F (self, 0x0C) = 32.0f;
    F (self, 0x14C) = 0.03125f;            // 1/32
    U (self, 0x68) = 0u;
    F (self, 0x150) = 1.0f;
    U (self, 0x60) = 2u;
    U (self, 0x64) = 2u;
    U (self, 0x10)=0u; U(self,0x14)=0u; U(self,0x18)=0u;
    U (self, 0x140)=0u; U(self,0x144)=0u; U(self,0x154)=0u; Bt(self,0x148)=0;
    U (self, 0xB8)=0u; Bt(self,0x164)=0;
    F (self, 0x98) = -9.82f;
    U (self, 0x9C)=0u;
    U (self, 0xAC)=0u; U(self,0xB0)=0u; U(self,0xB4)=0u;
    U (self, 0xA0)=0u; U(self,0xA4)=0u; U(self,0xA8)=0u;
    U (self, 0x124)=0u; U(self,0x128)=0u; U(self,0x12C)=0u;
    U (self, 0x130)=0u;
    U (self, 0x134)=0u; U(self,0x138)=0u; U(self,0x13C)=0u;
    U (self, 0xD0) = 2u;                   // internal count (no accessor slot)
    Bt(self, 0xC4) = 0;
    F (self, 0xC8) = 0.7f;
    F (self, 0xCC) = 0.8f;

    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n3_s39(self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n3_s53(self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n3_s55(self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n3_s57(self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n3_s59(self RD9_THUNK_ARG, v); }

    U (self, 0x168)=0u; U(self,0x158)=0u; U(self,0x160)=0u;
    F (self, 0x15C) = 1.0f;
    F (self, 0x190) = 20.0f;
    F (self, 0x194) = 20.0f;
    Bt(self, 0x165) = 0;
    U (self, 0x18C) = 0u;
    F (self, 0x198) = 3.0f;
    F (self, 0x19C) = 10.0f;
    F (self, 0x1A0) = 3.0f;
    F (self, 0x1A4) = 2.0f;
}

} // anonymous namespace

// geom_ParticleSystem_type3_ctor, 0x10186AA0.
void* constructParticleSystemType3(void* mem, const void* nameString)
{
    ::memset(mem, 0, PSM_DESCRIPTOR_SIZE(0x1A8u));
    *static_cast<const void* const**>(mem) = kVtable;

    U(mem, 0x20) = 1065353216u;   // 1.0
    U(mem, 0x2C) = 1973272912u;   // range sentinel
    U(mem, 0x34) = 1065353216u;
    U(mem, 0x40) = 1973272912u;
    U(mem, 0x48) = 1065353216u;
    U(mem, 0x54) = 1973272912u;

    // The manager supplies the renderer's own string; the getters expose an owned HostString.
    new (&psmDescriptor3<unsigned char>(mem, 0x6C)) ParticleHostString();
    new (&psmDescriptor3<unsigned char>(mem, 0x16C)) ParticleHostString(
        ::renddx9abi::toHostString(*static_cast<const std::string*>(nameString)));

    initType3Defaults(mem);
    return mem;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
