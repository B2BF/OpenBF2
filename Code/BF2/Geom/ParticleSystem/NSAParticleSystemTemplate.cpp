// Type-4 particle descriptor and derived properties; no simulation or draw state.
// Textures resolve through the atlas or remain owned direct references.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "ParticleRawLayout.h"
#include "../XpLightManager.h"
#include "ParticleTemplateResources.h"
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

extern LightManager* g_LightManager; // published boot surface; slot 2 retains retail's null guard.

namespace {

// [CHOSEN, not recovered] Per-descriptor host strings occupy the same reserved 28-byte spans (x64:
// 32-byte tail slots, ParticleRawLayout.h).
// std::string is 24 bytes on the supported x86 toolchain.
// Returning the object itself preserves reference lifetime without a shared conversion buffer.
using ParticleHostString = ::renddx9abi::HostString;
static_assert(sizeof(ParticleHostString) <= kPsmDescriptorStringSpan, "particle host string exceeds its reserved span");

inline unsigned&      U (void* p, unsigned off) { return psmDescriptor4<unsigned>(p, off); }
inline float&         F (void* p, unsigned off) { return psmDescriptor4<float>(p, off); }
inline unsigned char& Bt(void* p, unsigned off) { return psmDescriptor4<unsigned char>(p, off); }
inline short&         S16(void* p, unsigned off){ return psmDescriptor4<short>(p, off); }
inline unsigned       fbits(float f) { unsigned u; ::memcpy(&u, &f, 4); return u; }

#define PT_SET(name, off)  void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ U(s,(off)) = v; }
#define PT_SETFLT(name, off) void RD9_THUNK name(void* s RD9_THUNK_EDX, PsmFloatArg v){ U(s,(off)) = psmFloatArgBits(v); }
#define PT_GETF(name, off) float    RD9_THUNK name(void* s RD9_THUNK_EDX){ return F(s,(off)); }
#define PT_GETD(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return U(s,(off)); }
#define PT_SETB(name, off) void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ Bt(s,(off)) = static_cast<unsigned char>(v); }
#define PT_GETB(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return Bt(s,(off)); }
#define PT_LEA(name, off)  void*    RD9_THUNK name(void* s RD9_THUNK_EDX){ return &psmDescriptor4<unsigned char>(s, (off)); }
#define PT_SETVEC(name, off, n) void RD9_THUNK name(void* s RD9_THUNK_EDX, const float* v){ for (int i=0;i<(n);++i) F(s,(off)+4u*i)=v[i]; }
#define PT_GETVEC(name, off, n) void* RD9_THUNK name(void* s RD9_THUNK_EDX, float* o){ for (int i=0;i<(n);++i) o[i]=F(s,(off)+4u*i); return o; }

// cross-called - forward declared
void RD9_THUNK n4_s2  (void* s RD9_THUNK_EDX);
void RD9_THUNK n4_s45 (void* s RD9_THUNK_EDX, PsmFloatArg a);
void RD9_THUNK n4_s146(void* s RD9_THUNK_EDX, unsigned a);
void            n4_recomputeUV(void* self);

// simple slots
const ParticleHostString& RD9_THUNK n4_s1(void* self RD9_THUNK_EDX)
{ return psmDescriptor4<ParticleHostString>(self, 0x234); }
PT_SET (n4_s4,  0x68)
PT_GETD(n4_s5,  0x68)
PT_SET (n4_s6,  0x6C)
PT_GETD(n4_s7,  0x6C)
PT_SET (n4_s8,  0x70)
PT_GETD(n4_s9,  0x70)
void RD9_THUNK n4_s10(void* s RD9_THUNK_EDX, PSM_VEC3_PARAMS){ U(s,0x18)=PSM_VEC3_X; U(s,0x1C)=PSM_VEC3_Y; U(s,0x20)=PSM_VEC3_Z; } // Vec3 by value
PT_GETVEC(n4_s11, 0x18, 3)
PT_SETFLT(n4_s12, 0x64)
PT_GETF(n4_s13, 0x64)
PT_SETFLT(n4_s14, 0x60)
PT_GETF(n4_s15, 0x60)
PT_SET (n4_s16, 0x258)
PT_SETVEC(n4_s17, 0x24, 5)
PT_LEA (n4_s18, 0x24)
PT_SETVEC(n4_s19, 0x38, 5)
PT_LEA (n4_s20, 0x38)
PT_SETVEC(n4_s21, 0x4C, 5)
PT_LEA (n4_s22, 0x4C)
PT_GETF(n4_s24, 0x10)
PT_SETFLT(n4_s25, 0x1C0)
PT_GETF(n4_s26, 0x1C0)
PT_SETFLT(n4_s27, 0x0C)
PT_GETF(n4_s28, 0x0C)
PT_GETF(n4_s30, 0x1C4)
PT_SETB(n4_s31, 0x74)
PT_GETB(n4_s32, 0x74)
PT_SETFLT(n4_s33, 0x1CC)
PT_GETF(n4_s34, 0x1CC)
PT_SETVEC(n4_s35, 0x1B4, 3)
PT_GETVEC(n4_s36, 0x1B4, 3)
PT_GETVEC(n4_s38, 0x200, 3)
PT_SETVEC(n4_s39, 0xEC, 4)
PT_LEA (n4_s40, 0xEC)
PT_SETB(n4_s41, 0x210)
PT_GETB(n4_s42, 0x210)
PT_SETFLT(n4_s43, 0x78)
PT_GETF(n4_s44, 0x78)
PT_GETF(n4_s46, 0x7C)
PT_SETFLT(n4_s47, 0x84)
PT_GETF(n4_s48, 0x84)
PT_SETFLT(n4_s49, 0x88)
PT_GETF(n4_s50, 0x88)
PT_SETFLT(n4_s51, 0x1B0)
PT_GETF(n4_s52, 0x1B0)
PT_SETVEC(n4_s53, 0xFC, 4)
PT_LEA (n4_s54, 0xFC)
PT_SETVEC(n4_s55, 0x10C, 4)
PT_LEA (n4_s56, 0x10C)
PT_SETVEC(n4_s57, 0x11C, 4)
PT_LEA (n4_s58, 0x11C)
PT_SETVEC(n4_s59, 0x17C, 4)
PT_LEA (n4_s60, 0x17C)
PT_SETFLT(n4_s61, 0x9C)
PT_GETF(n4_s62, 0x9C)
PT_SETB(n4_s63, 0xDC)
PT_GETB(n4_s64, 0xDC)
PT_SETFLT(n4_s65, 0xE0)
PT_GETF(n4_s66, 0xE0)
PT_SETFLT(n4_s67, 0xE4)
PT_GETF(n4_s68, 0xE4)
PT_SETB(n4_s69, 0x211)
PT_GETB(n4_s70, 0x211)
PT_SETB(n4_s71, 0xA0)
PT_GETB(n4_s72, 0xA0)
PT_SETFLT(n4_s73, 0xA4)
PT_GETF(n4_s74, 0xA4)
PT_SET (n4_s75, 0x260)
PT_GETD(n4_s76, 0x260)
PT_SETFLT(n4_s77, 0x264)
PT_GETF(n4_s78, 0x264)
PT_SETFLT(n4_s79, 0x268)
PT_GETF(n4_s80, 0x268)
PT_SETFLT(n4_s81, 0x26C)
PT_GETF(n4_s82, 0x26C)
PT_SETFLT(n4_s83, 0x270)
PT_GETF(n4_s84, 0x270)
PT_SETFLT(n4_s85, 0x274)
PT_GETF(n4_s86, 0x274)
PT_SETFLT(n4_s87, 0x278)
PT_GETF(n4_s88, 0x278)
PT_SETFLT(n4_s89, 0x138)
PT_GETF(n4_s90, 0x138)
PT_SETFLT(n4_s91, 0x1A4)
PT_GETF(n4_s92, 0x1A4)
PT_SETFLT(n4_s93, 0x1A8)
PT_GETF(n4_s94, 0x1A8)
PT_SETVEC(n4_s95, 0x18C, 4)
PT_LEA (n4_s96, 0x18C)
PT_SETFLT(n4_s97, 0x94)
PT_GETF(n4_s98, 0x94)
PT_SETFLT(n4_s99, 0x1D0)
PT_GETF(n4_s100, 0x1D0)
PT_GETVEC(n4_s102, 0x1D4, 3)
PT_SETVEC(n4_s106, 0x16C, 4)
PT_LEA (n4_s107, 0x16C)
PT_SETFLT(n4_s108, 0x134)
PT_GETF(n4_s109, 0x134)
PT_SETFLT(n4_s110, 0x20C)
PT_GETF(n4_s111, 0x20C)
PT_SETB(n4_s112, 0x25D)
PT_GETB(n4_s113, 0x25D)
PT_SETVEC(n4_s114, 0x12C, 2)
PT_GETVEC(n4_s115, 0x12C, 2)
PT_SETFLT(n4_s116, 0x8C)
PT_GETF(n4_s117, 0x8C)
PT_SETFLT(n4_s118, 0x90)
PT_GETF(n4_s119, 0x90)
PT_SETFLT(n4_s120, 0x1AC)
PT_GETF(n4_s121, 0x1AC)
PT_SETVEC(n4_s122, 0x19C, 2)
PT_GETVEC(n4_s123, 0x19C, 2)
PT_SETFLT(n4_s124, 0x98)
PT_GETF(n4_s125, 0x98)
const ParticleHostString& RD9_THUNK n4_s127(void* self RD9_THUNK_EDX)
{ return psmDescriptor4<ParticleHostString>(self, 0x218); }
PT_GETB(n4_s129, 0xBC)
PT_SETB(n4_s130, 0xC4)
PT_GETB(n4_s131, 0xC4)
PT_SETB(n4_s132, 0xBD)
PT_GETB(n4_s133, 0xBD)
PT_SETFLT(n4_s134, 0xC0)
PT_GETF(n4_s135, 0xC0)
PT_SETFLT(n4_s140, 0xC8)
PT_GETF(n4_s141, 0xC8)
PT_SETFLT(n4_s142, 0xCC)
PT_GETF(n4_s143, 0xCC)
PT_GETD(n4_s145, 0xD0)
PT_GETD(n4_s147, 0xD4)

// complex slots

unsigned RD9_THUNK n4_s3(void* RD9_THUNK_EDX) { return 4; }   // emitter type id (const)

// slot 2: recompute/finalize derived fields.
void RD9_THUNK n4_s2(void* self RD9_THUNK_EDX)
{
    if (F(self, 0x1D0) < 0.0f)          // "auto" sentinel -> ask LightManager (null -> 0)
    {
        if (g_LightManager)
            F(self, 0x158) =
                g_LightManager->getDefaultEffectLightAffectionFactor();
        else
            U(self, 0x158) = 0;
    }
    else
        U(self, 0x158) = U(self, 0x1D0);

    unsigned mode = U(self, 0x68);
    F(self, 0x1C8) = (mode == 0) ? 0.5f : (mode == 1) ? 0.7f : 1.0f;

    n4_s45(self RD9_THUNK_ARG, psmFloatArgFromBits(U(self, 0x7C)));

    F(self, 0xB0) = (F(self, 0xA8) == F(self, 0xAC)) ? 0.0f : 1.0f / (F(self, 0xA8) - F(self, 0xAC));
    F(self, 0x14) = (F(self, 0x1C4) <= 0.0f) ? 0.2f : 1.0f / F(self, 0x1C4);
    if (U(self, 0xD4)) F(self, 0xD8) = 1.0f / static_cast<float>(U(self, 0xD4));
    else               U(self, 0xD8) = 0;

    n4_recomputeUV(self);
}

void RD9_THUNK n4_s23(void* self RD9_THUNK_EDX, PsmFloatArg v) { U(self, 0x10)  = psmFloatArgBits(v); n4_s2(self RD9_THUNK_ARG); }
void RD9_THUNK n4_s29(void* self RD9_THUNK_EDX, PsmFloatArg v) { U(self, 0x1C4) = psmFloatArgBits(v); n4_s2(self RD9_THUNK_ARG); }

// slot 37: store Vec3 at +0x200, normalize in place.
void RD9_THUNK n4_s37(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x200) = v[0]; F(self, 0x204) = v[1]; F(self, 0x208) = v[2];
    float* p = &psmDescriptor4<float>(self, 0x200);
    float s = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
    if (fabsf(s - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(s) < FLT_EPSILON) { p[0] = p[1] = p[2] = 0.0f; }
        else { float inv = 1.0f / sqrtf(s); p[0] *= inv; p[1] *= inv; p[2] *= inv; }
    }
}

// slot 45: set lifetime +0x7C; recompute 1/x at +0x80 and, if +0xBC set, the +0xC0 rate.
void RD9_THUNK n4_s45(void* self RD9_THUNK_EDX, PsmFloatArg a)
{
    U(self, 0x7C) = psmFloatArgBits(a);
    float fa = F(self, 0x7C);
    if (fa > 0.0f) F(self, 0x80) = 1.0f / fa;
    if (Bt(self, 0xBC) && fa > 0.0f) F(self, 0xC0) = static_cast<float>(U(self, 0xD0)) / fa;
}

// slot 101: Vec3 to +0x1D4 and mirror +0x14C.
void RD9_THUNK n4_s101(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1D4) = v[0]; F(self, 0x1D8) = v[1]; F(self, 0x1DC) = v[2];
    F(self, 0x14C) = v[0]; F(self, 0x150) = v[1]; F(self, 0x154) = v[2];
}

// slot 103: Vec3 packed to Vec4{r,g,b,1} at +0x1E0 and mirror +0x15C.
void RD9_THUNK n4_s103(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1E0) = v[0]; F(self, 0x1E4) = v[1]; F(self, 0x1E8) = v[2]; F(self, 0x1EC) = 1.0f;
    F(self, 0x15C) = v[0]; F(self, 0x160) = v[1]; F(self, 0x164) = v[2]; F(self, 0x168) = 1.0f;
}

// slot 104: get the +0x1E0 color as Vec3 (drops w).
void* RD9_THUNK n4_s104(void* self RD9_THUNK_EDX, float* out)
{
    out[0] = F(self, 0x1E0); out[1] = F(self, 0x1E4); out[2] = F(self, 0x1E8);
    return out;
}

// slot 105: Vec3 packed to Vec4{r,g,b,1} at +0x1F0 (no mirror).
void RD9_THUNK n4_s105(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1F0) = v[0]; F(self, 0x1F4) = v[1]; F(self, 0x1F8) = v[2]; F(self, 0x1FC) = 1.0f;
}

// slot 126 (0x10178690), loader changed-name load, retaining the empty-name
// resource behavior and clearing the stored name if the load fails.
void RD9_THUNK n4_s126(void* self RD9_THUNK_EDX, const ParticleHostString& source)
{
    ParticleHostString* dst = &psmDescriptor4<ParticleHostString>(self, 0x218);
    if (dst->size() != source.size() || ::memcmp(dst->c_str(), source.c_str(), source.size()) != 0)
    {
        *dst = source;
        const std::string& resourceName = ::renddx9abi::fromHostString(source);
        dx9rend::EngineTexture*& texture = psmDescriptor4<dx9rend::EngineTexture*>(self, 0x250);
        dx9rend::TextureAtlasEntry*& region = psmDescriptor4<dx9rend::TextureAtlasEntry*>(self, 0x254);
        if (!particleTemplateLoadTexture(texture, region, resourceName,
                "dice::hfe::geom::NSAParticleSystemTemplate::setTexture", 181))
            *dst = "";
    }
}

// slot 128: set +0xBC bool; when set (and +0x7C > 0) recompute the +0xC0 rate.
void RD9_THUNK n4_s128(void* self RD9_THUNK_EDX, unsigned b)
{
    Bt(self, 0xBC) = static_cast<unsigned char>(b);
    if (psmBoolArg(b) && F(self, 0x7C) > 0.0f) F(self, 0xC0) = static_cast<float>(U(self, 0xD0)) / F(self, 0x7C);
}

// slots 136-139: direct-texture-relative UV rates; width/height at (+0x250)+0x28/+0x2C.
// Atlas-backed regions leave the direct texture null, preserving the no-op/zero branches.
void RD9_THUNK n4_s136(void* self RD9_THUNK_EDX, unsigned a)
{
    void* t = psmDescriptor4<void*>(self, 0x250);
    if (t) F(self, 0xC8) = static_cast<float>(a) / static_cast<float>(static_cast<dx9rend::EngineTexture*>(t)->width);
}
unsigned RD9_THUNK n4_s137(void* self RD9_THUNK_EDX)
{
    void* t = psmDescriptor4<void*>(self, 0x250);
    if (t) return static_cast<unsigned>(static_cast<int>(static_cast<float>(static_cast<dx9rend::EngineTexture*>(t)->width) * F(self, 0xC8)));
    return 0;
}
void RD9_THUNK n4_s138(void* self RD9_THUNK_EDX, unsigned a)
{
    void* t = psmDescriptor4<void*>(self, 0x250);
    if (t) F(self, 0xCC) = static_cast<float>(a) / static_cast<float>(static_cast<dx9rend::EngineTexture*>(t)->height);
}
unsigned RD9_THUNK n4_s139(void* self RD9_THUNK_EDX)
{
    void* t = psmDescriptor4<void*>(self, 0x250);
    if (t) return static_cast<unsigned>(static_cast<int>(static_cast<float>(static_cast<dx9rend::EngineTexture*>(t)->height) * F(self, 0xCC)));
    return 0;
}

// slot 144: set +0xD0; cascade to slot146 when +0xD4 set; recompute +0xC0 when +0xBC set.
void RD9_THUNK n4_s144(void* self RD9_THUNK_EDX, unsigned a)
{
    U(self, 0xD0) = a;
    if (U(self, 0xD4)) n4_s146(self RD9_THUNK_ARG, U(self, 0xD4));
    if (Bt(self, 0xBC) && F(self, 0x7C) > 0.0f) F(self, 0xC0) = static_cast<float>(U(self, 0xD0)) / F(self, 0x7C);
}

// slot 146: set +0xD4 count; cache 1/count at +0xD8; fill the UV steps +0xC8/+0xCC when still unset.
void RD9_THUNK n4_s146(void* self RD9_THUNK_EDX, unsigned a)
{
    U(self, 0xD4) = a;
    if (a)
    {
        float inv = 1.0f / static_cast<float>(a);
        F(self, 0xD8) = inv;
        if (U(self, 0xC8) == 0) n4_s140(self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(inv)));
        if (U(self, 0xCC) == 0) n4_s142(self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(inv)));
    }
}

// sub_101786E0 (slot 2 tail): recompute UV from the borrowed +0x254 region (null -> the {1,1} path),
// then push through slot114 into +0x12C.
void n4_recomputeUV(void* self)
{
    const dx9rend::TextureAtlasEntry* region = psmDescriptor4<dx9rend::TextureAtlasEntry*>(self, 0x254);
    float u, v;
    if (region)
    {
        S16(self, 0x212) = static_cast<short>(region->uOffset * 32767.0f);
        S16(self, 0x214) = static_cast<short>(region->vOffset * 32767.0f);
        if (Bt(self, 0xC4)) { u = region->width * F(self, 0xC8); v = region->height * F(self, 0xCC); }
        else                { u = region->width;                 v = region->height; }
    }
    else
    {
        S16(self, 0x212) = 0;
        S16(self, 0x214) = 0;
        if (Bt(self, 0xC4)) { u = F(self, 0xC8); v = F(self, 0xCC); }
        else                { u = 1.0f;          v = 1.0f; }
    }
    float uv[2] = { u, v };
    n4_s114(self RD9_THUNK_ARG, uv);
}

// slot 0 (sub_10178460): release the owned +0x250 texture, destroy the two strings,
// free when the deleting flag is set. The atlas region is borrowed.
void* RD9_THUNK n4_dtor(void* self RD9_THUNK_EDX, int flags)
{
    dx9rend::EngineTexture*& texture = psmDescriptor4<dx9rend::EngineTexture*>(self, 0x250);
    dx9rend::textureReleaseRef(texture);
    psmDescriptor4<ParticleHostString>(self, 0x234).~ParticleHostString();
    psmDescriptor4<ParticleHostString>(self, 0x218).~ParticleHostString();
    if (flags & 1)
        ::renddx9abi::engineFree(self);
    return self;
}

const void* const kVtable[148] =
{
    reinterpret_cast<void*>(&n4_dtor), reinterpret_cast<void*>(&n4_s1),   reinterpret_cast<void*>(&n4_s2),   reinterpret_cast<void*>(&n4_s3),
    reinterpret_cast<void*>(&n4_s4),   reinterpret_cast<void*>(&n4_s5),   reinterpret_cast<void*>(&n4_s6),   reinterpret_cast<void*>(&n4_s7),
    reinterpret_cast<void*>(&n4_s8),   reinterpret_cast<void*>(&n4_s9),   reinterpret_cast<void*>(&n4_s10),  reinterpret_cast<void*>(&n4_s11),
    reinterpret_cast<void*>(&n4_s12),  reinterpret_cast<void*>(&n4_s13),  reinterpret_cast<void*>(&n4_s14),  reinterpret_cast<void*>(&n4_s15),
    reinterpret_cast<void*>(&n4_s16),  reinterpret_cast<void*>(&n4_s17),  reinterpret_cast<void*>(&n4_s18),  reinterpret_cast<void*>(&n4_s19),
    reinterpret_cast<void*>(&n4_s20),  reinterpret_cast<void*>(&n4_s21),  reinterpret_cast<void*>(&n4_s22),  reinterpret_cast<void*>(&n4_s23),
    reinterpret_cast<void*>(&n4_s24),  reinterpret_cast<void*>(&n4_s25),  reinterpret_cast<void*>(&n4_s26),  reinterpret_cast<void*>(&n4_s27),
    reinterpret_cast<void*>(&n4_s28),  reinterpret_cast<void*>(&n4_s29),  reinterpret_cast<void*>(&n4_s30),  reinterpret_cast<void*>(&n4_s31),
    reinterpret_cast<void*>(&n4_s32),  reinterpret_cast<void*>(&n4_s33),  reinterpret_cast<void*>(&n4_s34),  reinterpret_cast<void*>(&n4_s35),
    reinterpret_cast<void*>(&n4_s36),  reinterpret_cast<void*>(&n4_s37),  reinterpret_cast<void*>(&n4_s38),  reinterpret_cast<void*>(&n4_s39),
    reinterpret_cast<void*>(&n4_s40),  reinterpret_cast<void*>(&n4_s41),  reinterpret_cast<void*>(&n4_s42),  reinterpret_cast<void*>(&n4_s43),
    reinterpret_cast<void*>(&n4_s44),  reinterpret_cast<void*>(&n4_s45),  reinterpret_cast<void*>(&n4_s46),  reinterpret_cast<void*>(&n4_s47),
    reinterpret_cast<void*>(&n4_s48),  reinterpret_cast<void*>(&n4_s49),  reinterpret_cast<void*>(&n4_s50),  reinterpret_cast<void*>(&n4_s51),
    reinterpret_cast<void*>(&n4_s52),  reinterpret_cast<void*>(&n4_s53),  reinterpret_cast<void*>(&n4_s54),  reinterpret_cast<void*>(&n4_s55),
    reinterpret_cast<void*>(&n4_s56),  reinterpret_cast<void*>(&n4_s57),  reinterpret_cast<void*>(&n4_s58),  reinterpret_cast<void*>(&n4_s59),
    reinterpret_cast<void*>(&n4_s60),  reinterpret_cast<void*>(&n4_s61),  reinterpret_cast<void*>(&n4_s62),  reinterpret_cast<void*>(&n4_s63),
    reinterpret_cast<void*>(&n4_s64),  reinterpret_cast<void*>(&n4_s65),  reinterpret_cast<void*>(&n4_s66),  reinterpret_cast<void*>(&n4_s67),
    reinterpret_cast<void*>(&n4_s68),  reinterpret_cast<void*>(&n4_s69),  reinterpret_cast<void*>(&n4_s70),  reinterpret_cast<void*>(&n4_s71),
    reinterpret_cast<void*>(&n4_s72),  reinterpret_cast<void*>(&n4_s73),  reinterpret_cast<void*>(&n4_s74),  reinterpret_cast<void*>(&n4_s75),
    reinterpret_cast<void*>(&n4_s76),  reinterpret_cast<void*>(&n4_s77),  reinterpret_cast<void*>(&n4_s78),  reinterpret_cast<void*>(&n4_s79),
    reinterpret_cast<void*>(&n4_s80),  reinterpret_cast<void*>(&n4_s81),  reinterpret_cast<void*>(&n4_s82),  reinterpret_cast<void*>(&n4_s83),
    reinterpret_cast<void*>(&n4_s84),  reinterpret_cast<void*>(&n4_s85),  reinterpret_cast<void*>(&n4_s86),  reinterpret_cast<void*>(&n4_s87),
    reinterpret_cast<void*>(&n4_s88),  reinterpret_cast<void*>(&n4_s89),  reinterpret_cast<void*>(&n4_s90),  reinterpret_cast<void*>(&n4_s91),
    reinterpret_cast<void*>(&n4_s92),  reinterpret_cast<void*>(&n4_s93),  reinterpret_cast<void*>(&n4_s94),  reinterpret_cast<void*>(&n4_s95),
    reinterpret_cast<void*>(&n4_s96),  reinterpret_cast<void*>(&n4_s97),  reinterpret_cast<void*>(&n4_s98),  reinterpret_cast<void*>(&n4_s99),
    reinterpret_cast<void*>(&n4_s100), reinterpret_cast<void*>(&n4_s101), reinterpret_cast<void*>(&n4_s102), reinterpret_cast<void*>(&n4_s103),
    reinterpret_cast<void*>(&n4_s104), reinterpret_cast<void*>(&n4_s105), reinterpret_cast<void*>(&n4_s106), reinterpret_cast<void*>(&n4_s107),
    reinterpret_cast<void*>(&n4_s108), reinterpret_cast<void*>(&n4_s109), reinterpret_cast<void*>(&n4_s110), reinterpret_cast<void*>(&n4_s111),
    reinterpret_cast<void*>(&n4_s112), reinterpret_cast<void*>(&n4_s113), reinterpret_cast<void*>(&n4_s114), reinterpret_cast<void*>(&n4_s115),
    reinterpret_cast<void*>(&n4_s116), reinterpret_cast<void*>(&n4_s117), reinterpret_cast<void*>(&n4_s118), reinterpret_cast<void*>(&n4_s119),
    reinterpret_cast<void*>(&n4_s120), reinterpret_cast<void*>(&n4_s121), reinterpret_cast<void*>(&n4_s122), reinterpret_cast<void*>(&n4_s123),
    reinterpret_cast<void*>(&n4_s124), reinterpret_cast<void*>(&n4_s125), reinterpret_cast<void*>(&n4_s126), reinterpret_cast<void*>(&n4_s127),
    reinterpret_cast<void*>(&n4_s128), reinterpret_cast<void*>(&n4_s129), reinterpret_cast<void*>(&n4_s130), reinterpret_cast<void*>(&n4_s131),
    reinterpret_cast<void*>(&n4_s132), reinterpret_cast<void*>(&n4_s133), reinterpret_cast<void*>(&n4_s134), reinterpret_cast<void*>(&n4_s135),
    reinterpret_cast<void*>(&n4_s136), reinterpret_cast<void*>(&n4_s137), reinterpret_cast<void*>(&n4_s138), reinterpret_cast<void*>(&n4_s139),
    reinterpret_cast<void*>(&n4_s140), reinterpret_cast<void*>(&n4_s141), reinterpret_cast<void*>(&n4_s142), reinterpret_cast<void*>(&n4_s143),
    reinterpret_cast<void*>(&n4_s144), reinterpret_cast<void*>(&n4_s145), reinterpret_cast<void*>(&n4_s146), reinterpret_cast<void*>(&n4_s147),
};

// sub_10177AE0 - init helper: defaults + ~18 setter calls, in shipped statement order.
void initNsaDefaults(void* self)
{
    F (self, 0xB8)  = -1.0f;
    U (self, 0x0C)  = 0u;
    U (self, 0x258) = 0u;
    U (self, 0x24)=0u; U(self,0x28)=0u; U(self,0x2C)=0u; Bt(self,0x30)=0;
    U (self, 0x38)=0u; U(self,0x3C)=0u; U(self,0x40)=0u; Bt(self,0x44)=0;
    U (self, 0x4C)=0u; U(self,0x50)=0u; U(self,0x54)=0u; Bt(self,0x58)=0;
    F (self, 0x60)  = 360.0f;
    U (self, 0x64)  = 0u;
    Bt(self, 0x74)  = 0;
    F (self, 0x10)  = 32.0f;
    U (self, 0x70)  = 0u;
    Bt(self, 0x25C) = 0;
    Bt(self, 0x25D) = 0;
    U (self, 0x68)  = 2u;
    U (self, 0x6C)  = 2u;
    F (self, 0x1C8) = 1.0f;
    U (self, 0x18)=0u; U(self,0x1C)=0u; U(self,0x20)=0u;
    U (self, 0x1C0)=0u; U(self,0x1C4)=0u; U(self,0x1CC)=0u;
    F (self, 0xA8)  = 0.025f;
    F (self, 0xAC)  = 0.01f;
    F (self, 0xB0)  = 66.6667f;
    F (self, 0xB4)  = 0.05f;
    U (self, 0x9C)  = 0u;
    Bt(self, 0xA0)  = 0;
    F (self, 0xA4)  = 0.03f;
    U (self, 0x20C) = 0u;
    Bt(self, 0x210) = 0;
    F (self, 0x84)  = -9.82f;
    U (self, 0x88)  = 0u;
    F (self, 0x90)  = 0.25f;
    U (self, 0x8C)  = 0u;
    F (self, 0x94)  = 0.2f;
    F (self, 0x98)  = 1.0f;
    U (self, 0x19C)=0u; U(self,0x1A0)=0u; U(self,0x1A4)=0u; U(self,0x1A8)=0u;
    U (self, 0x1AC)=0u; U(self,0x1B0)=0u; U(self,0x1B4)=0u; U(self,0x1B8)=0u; U(self,0x1BC)=0u;

    n4_s99(self RD9_THUNK_ARG, psmFloatArgFromBits(static_cast<unsigned>(-1082130432)));       // +0x1D0 = -1.0
    { float v[3] = { 1.0f, 1.0f, 1.0f }; n4_s101(self RD9_THUNK_ARG, v); }
    { float v[3] = { 1.0f, 1.0f, 1.0f }; n4_s103(self RD9_THUNK_ARG, v); }
    { float v[3] = { 1.0f, 1.0f, 1.0f }; n4_s105(self RD9_THUNK_ARG, v); }

    U (self, 0x148) = 0u;
    { ParticleHostString empty; psmDescriptor4<ParticleHostString>(self, 0x218) = empty; }
    Bt(self, 0xDC)  = 0;
    F (self, 0xE0)  = 0.7f;
    F (self, 0xE4)  = 0.8f;
    U (self, 0xE8)  = 2u;
    Bt(self, 0xBD)  = 0;
    F (self, 0xC0)  = 1.0f;
    Bt(self, 0xBC)  = 0;
    Bt(self, 0xC4)  = 0;
    U (self, 0xC8)=0u; U(self,0xCC)=0u; U(self,0xD0)=0u; U(self,0xD4)=0u; U(self,0xD8)=0u;

    { float v[2] = { 1.0f, 1.0f };             n4_s114(self RD9_THUNK_ARG, v); }
    n4_s89 (self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(1.0f)));             // +0x138 = 1.0
    n4_s108(self RD9_THUNK_ARG, psmFloatArgFromBits(0u));                       // +0x134 = 0
    n4_s45 (self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(4.0f)));             // +0x7C = 4.0
    n4_s43 (self RD9_THUNK_ARG, psmFloatArgFromBits(0u));                       // +0x78 = 0
    { float v[4] = { 0.0f, 0.0f, 1.0f, 0.0f }; n4_s106(self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n4_s59 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n4_s95 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n4_s39 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n4_s53 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n4_s55 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n4_s57 (self RD9_THUNK_ARG, v); }

    F (self, 0x264) = 20.0f;
    F (self, 0x268) = 20.0f;
    U (self, 0x260) = 0u;
    F (self, 0x26C) = 3.0f;
    F (self, 0x270) = 10.0f;
    F (self, 0x274) = 3.0f;
    F (self, 0x278) = 2.0f;
    U (self, 0x200) = 0u;
    F (self, 0x204) = 1.0f;
    U (self, 0x208) = 0u;
    Bt(self, 0x211) = 0;

    n4_s2(self RD9_THUNK_ARG);   // final recompute
}

} // anonymous namespace


// geom_NSAParticleSystemTemplate_ctor, 0x101788F0. Vtable + scalar defaults + the two name strings
// (string2 = the name arg) + the zeroed sub-block at +0x12C (sub_10188150) + the init helper.
// Leading memset covers retail's hand-written zeros and the .con property-load gaps (observably
// identical; deterministic). [CHOSEN: leading memset]

void* constructParticleSystemType4(void* mem, const void* nameString)
{
    ::memset(mem, 0, PSM_DESCRIPTOR_SIZE(0x27Cu));

    *static_cast<const void* const**>(mem) = kVtable;

    U(mem, 0x28) = 1065353216u;   // 1.0
    U(mem, 0x34) = 1973272912u;   // bounds sentinel (== type 0)
    U(mem, 0x3C) = 1065353216u;   // 1.0
    U(mem, 0x48) = 1973272912u;
    U(mem, 0x50) = 1065353216u;   // 1.0
    U(mem, 0x5C) = 1973272912u;
    // sub_10188150(this+0x12C): a 112-byte zero-fill - covered by the memset above.

    new (&psmDescriptor4<unsigned char>(mem, 0x218)) ParticleHostString();
    new (&psmDescriptor4<unsigned char>(mem, 0x234)) ParticleHostString(
        ::renddx9abi::toHostString(*static_cast<const std::string*>(nameString)));

    initNsaDefaults(mem);
    return mem;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
