// Sprite-particle descriptor with derived properties and sorted/additive collector selection.
// Owns direct textures when the requested region is not in the particle atlas.

#ifdef BF2_IMAGE_RENDDX9
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Compiler-typed native table entries.
#include "../../../RendDX9Abi/NativeVtable.h"
#include "ParticleSystemManager.h"
#endif

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "../XpLightManager.h"
#include "ParticleTemplateResources.h"
#include "ParticleRendererCallbacks.h"
#include "../../Platform/X87Compat.h"
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

// Geom singletons this descriptor consults. LightManager now publishes its instruction-proven
// +0x270 getter; the retail null guard remains. g_ParticleSystemManager holds the singleton whose
// +0x8BA8 flag can override the particle-type -> update-fn selection in slot 114.
extern LightManager* g_LightManager;
extern IBase* g_ParticleSystemManager;

namespace {

// [CHOSEN, not recovered] BF2 passes std::string through the descriptor interface. Each object owns
// its host strings in the original 28-byte reserved spans. Current x86 strings occupy 24 bytes, so
// every subsequent descriptor offset is retained; x64 places them in 32-byte tail slots
// (ParticleRawLayout.h).
using ParticleHostString = ::renddx9abi::HostString;
static_assert(sizeof(ParticleHostString) <= kPsmDescriptorStringSpan, "particle host string exceeds its reserved span");

// Byte-offset field accessors over the raw 0x2A4 object; offsets copied verbatim from the shipped
// bodies and placed through ParticleRawLayout.h. `self` is the object base.
inline unsigned&      U (void* p, unsigned off) { return psmDescriptor0<unsigned>(p, off); }
inline float&         F (void* p, unsigned off) { return psmDescriptor0<float>(p, off); }
inline unsigned char& Bt(void* p, unsigned off) { return psmDescriptor0<unsigned char>(p, off); }
inline short&         S16(void* p, unsigned off){ return psmDescriptor0<short>(p, off); }
inline unsigned       fbits(float f) { unsigned u; ::memcpy(&u, &f, 4); return u; }

// Cross-called methods (recompute / cascade) - forward declared.
void RD9_THUNK pt_s2  (void* s RD9_THUNK_EDX);
void RD9_THUNK pt_s45 (void* s RD9_THUNK_EDX, PsmFloatArg v);
void RD9_THUNK pt_s114(void* s RD9_THUNK_EDX, int type);
void RD9_THUNK pt_s150(void* s RD9_THUNK_EDX, unsigned a);
void            pt_particleUpdateDeferred();
void            templateRecomputeUV(void* self);

// The repetitive property accessors. Every scalar arg arrives as a 4-byte stack slot (float bits read
// as unsigned store identically); byte setters store the low byte. All match __thiscall callee-cleans
// (the unused 2nd param is the EDX shadow).
#define PT_SETF(name, off) void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ U(s,(off)) = v; }
#define PT_SETFLT(name, off) void RD9_THUNK name(void* s RD9_THUNK_EDX, PsmFloatArg v){ U(s,(off)) = psmFloatArgBits(v); }
#define PT_GETF(name, off) float    RD9_THUNK name(void* s RD9_THUNK_EDX){ return F(s,(off)); }
#define PT_SETD(name, off) void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ U(s,(off)) = v; }
#define PT_GETD(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return U(s,(off)); }
#define PT_SETB(name, off) void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ Bt(s,(off)) = static_cast<unsigned char>(v); }
#define PT_GETB(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return Bt(s,(off)); }
#define PT_LEA(name, off)  void*    RD9_THUNK name(void* s RD9_THUNK_EDX){ return &psmDescriptor0<unsigned char>(s, (off)); }
#define PT_SETVEC(name, off, n) void RD9_THUNK name(void* s RD9_THUNK_EDX, const float* v) \
{ \
    if ((n)==3 || (n)==4) \
    { \
        const float z=v[2],y=v[1],x=v[0]; \
        const float w=(n)==4 ? v[3] : 0.0f; \
        F(s,(off)+4)=y; F(s,(off))=x; F(s,(off)+8)=z; \
        if ((n)==4) F(s,(off)+12)=w; \
    } \
    else for (int i=0;i<(n);++i) F(s,(off)+4u*i)=v[i]; \
}
#define PT_GETVEC(name, off, n) void* RD9_THUNK name(void* s RD9_THUNK_EDX, float* o) \
{ \
    const float y=F(s,(off)+4),x=F(s,(off)); \
    const float z=(n)==3 ? F(s,(off)+8) : 0.0f; \
    o[1]=y; o[0]=x; if ((n)==3) o[2]=z; return o; \
}

// simple slots (index; byte offset in the comment)
const ParticleHostString& RD9_THUNK pt_s1(void* self RD9_THUNK_EDX)
{ return psmDescriptor0<ParticleHostString>(self, 0x240); }
// PE 0x10178B60
PT_SETD(pt_s4,  0x60)
PT_GETD(pt_s5,  0x60)
PT_SETD(pt_s6,  0x64)
PT_GETD(pt_s7,  0x64)
PT_SETD(pt_s8,  0x68)
PT_GETD(pt_s9,  0x68)
// PE 0x10178F70
void RD9_THUNK pt_s10(void* s RD9_THUNK_EDX, PSM_VEC3_PARAMS){ U(s,0x18)=PSM_VEC3_X; U(s,0x1C)=PSM_VEC3_Y; U(s,0x20)=PSM_VEC3_Z; } // Vec3 by value
// PE 0x10178F90
PT_GETVEC(pt_s11, 0x18, 3)
PT_SETFLT(pt_s12, 0x74)
PT_GETF(pt_s13, 0x74)
// PE 0x10178A90
PT_SETFLT(pt_s14, 0x70)
PT_GETF(pt_s15, 0x70)
// PE 0x10177270
PT_SETD(pt_s16, 0x264)
// PE 0x10178B90
PT_SETVEC(pt_s17, 0x24, 5)
PT_LEA (pt_s18, 0x24)
// PE 0x10177290
PT_SETVEC(pt_s19, 0x38, 5)
PT_LEA (pt_s20, 0x38)
// PE 0x101772D0
PT_SETVEC(pt_s21, 0x4C, 5)
PT_LEA (pt_s22, 0x4C)
PT_GETF(pt_s24, 0x10)
// PE 0x10177490
PT_SETFLT(pt_s25, 0x1D0)
PT_GETF(pt_s26, 0x1D0)
// PE 0x10178AC0
PT_SETFLT(pt_s27, 0x0C)
PT_GETF(pt_s28, 0x0C)
PT_GETF(pt_s30, 0x1D4)
// PE 0x10177210
PT_SETB(pt_s31, 0x6C)
PT_GETB(pt_s32, 0x6C)
// PE 0x10177230
PT_SETFLT(pt_s33, 0x1D8)
PT_GETF(pt_s34, 0x1D8)
// PE 0x101776D0
PT_SETVEC(pt_s35, 0x1C4, 3)
// PE 0x10177700
PT_GETVEC(pt_s36, 0x1C4, 3)
// PE 0x10176A20
PT_GETVEC(pt_s38, 0x20C, 3)
// PE 0x10176930
PT_SETVEC(pt_s39, 0xFC, 4)
PT_LEA (pt_s40, 0xFC)
// PE 0x10177550
PT_SETB(pt_s41, 0x21C)
PT_GETB(pt_s42, 0x21C)
// PE 0x10176060
PT_SETFLT(pt_s43, 0x80)
PT_GETF(pt_s44, 0x80)
PT_GETF(pt_s46, 0x84)
// PE 0x10178C00
PT_SETFLT(pt_s47, 0x8C)
PT_GETF(pt_s48, 0x8C)
// PE 0x10177300 (shipped NaN assert dropped in release builds; both arms store the argument)
PT_SETFLT(pt_s49, 0x90)   // shipped has a NaN assert; a plain store is the release behavior
PT_GETF(pt_s50, 0x90)
// PE 0x101773E0
PT_SETFLT(pt_s51, 0x1C0)
PT_GETF(pt_s52, 0x1C0)
// PE 0x10176960
PT_SETVEC(pt_s53, 0x10C, 4)
PT_LEA (pt_s54, 0x10C)
// PE 0x10176990
PT_SETVEC(pt_s55, 0x11C, 4)
PT_LEA (pt_s56, 0x11C)
// PE 0x101769C0
PT_SETVEC(pt_s57, 0x12C, 4)
PT_LEA (pt_s58, 0x12C)
// PE 0x101768D0
PT_SETVEC(pt_s59, 0x18C, 4)
PT_LEA (pt_s60, 0x18C)
// PE 0x10188300
PT_SETFLT(pt_s61, 0xA4)
PT_GETF(pt_s62, 0xA4)
// PE 0x101773F0
PT_SETB(pt_s63, 0xEC)
PT_GETB(pt_s64, 0xEC)
// PE 0x10177400
PT_SETFLT(pt_s65, 0xF0)
PT_GETF(pt_s66, 0xF0)
// PE 0x10177410
PT_SETFLT(pt_s67, 0xF4)
PT_GETF(pt_s68, 0xF4)
PT_SETB(pt_s69, 0x21D)
PT_GETB(pt_s70, 0x21D)
// PE 0x10177420
PT_SETB(pt_s71, 0xA8)
PT_GETB(pt_s72, 0xA8)
// PE 0x10177440
PT_SETFLT(pt_s73, 0xAC)
PT_GETF(pt_s74, 0xAC)
// PE 0x10178F10
PT_SETD(pt_s75, 0x26C)
PT_GETD(pt_s76, 0x26C)
// PE 0x10177630
PT_SETFLT(pt_s77, 0x270)
PT_GETF(pt_s78, 0x270)
// PE 0x10178F40
PT_SETFLT(pt_s79, 0x274)
PT_GETF(pt_s80, 0x274)
// PE 0x10177640
PT_SETFLT(pt_s81, 0x278)
PT_GETF(pt_s82, 0x278)
// PE 0x10177650
PT_SETFLT(pt_s83, 0x27C)
PT_GETF(pt_s84, 0x27C)
// PE 0x10177670
PT_SETFLT(pt_s85, 0x280)
PT_GETF(pt_s86, 0x280)
// PE 0x10177690
PT_SETFLT(pt_s87, 0x284)
PT_GETF(pt_s88, 0x284)
// PE 0x101760A0
PT_SETFLT(pt_s89, 0x148)
PT_GETF(pt_s90, 0x148)
// PE 0x10177360
PT_SETFLT(pt_s91, 0x1B4)
PT_GETF(pt_s92, 0x1B4)
// PE 0x101773A0
PT_SETFLT(pt_s93, 0x1B8)
PT_GETF(pt_s94, 0x1B8)
// PE 0x10176900
PT_SETVEC(pt_s95, 0x19C, 4)
PT_LEA (pt_s96, 0x19C)
// PE 0x10186D80
PT_SETFLT(pt_s97, 0x9C)
PT_GETF(pt_s98, 0x9C)
// PE 0x101760C0
PT_SETFLT(pt_s99, 0x1F8)
PT_GETF(pt_s100, 0x1F8)
// PE 0x10176790
PT_GETVEC(pt_s102, 0x1DC, 3)
// PE 0x101768A0
PT_SETVEC(pt_s106, 0x17C, 4)
PT_LEA (pt_s107, 0x17C)
// PE 0x10176080
PT_SETFLT(pt_s108, 0x144)
PT_GETF(pt_s109, 0x144)
// PE 0x10177580
PT_SETFLT(pt_s110, 0x218)
PT_GETF(pt_s111, 0x218)
// PE 0x101775E0
PT_SETB(pt_s112, 0x269)
PT_GETB(pt_s113, 0x269)
PT_LEA (pt_s115, 0x78)
// PE 0x10188280
PT_SETD(pt_s116, 0x7C)
PT_GETD(pt_s117, 0x7C)
// PE 0x10175D20
PT_SETVEC(pt_s118, 0x13C, 2)
// PE 0x10177330
PT_SETFLT(pt_s120, 0x94)
PT_GETF(pt_s121, 0x94)
// PE 0x10178C80
PT_SETFLT(pt_s122, 0x98)
PT_GETF(pt_s123, 0x98)
// PE 0x10177340
PT_SETVEC(pt_s124, 0x1AC, 2)
// PE 0x101776B0
PT_GETVEC(pt_s125, 0x1AC, 2)
// PE 0x10177380
PT_SETFLT(pt_s126, 0xA0)
PT_GETF(pt_s127, 0xA0)
// PE 0x101773C0
PT_SETFLT(pt_s128, 0x1BC)
PT_GETF(pt_s129, 0x1BC)
const ParticleHostString& RD9_THUNK pt_s131(void* self RD9_THUNK_EDX)
{ return psmDescriptor0<ParticleHostString>(self, 0x224); }
PT_GETB(pt_s133, 0xCC)
// PE 0x101774D0
PT_SETB(pt_s134, 0xD4)
PT_GETB(pt_s135, 0xD4)
// PE 0x101774B0
PT_SETB(pt_s136, 0xCD)
PT_GETB(pt_s137, 0xCD)
// PE 0x101774F0
PT_SETFLT(pt_s138, 0xD0)
PT_GETF(pt_s139, 0xD0)
// PE 0x10177510
PT_SETFLT(pt_s144, 0xD8)
PT_GETF(pt_s145, 0xD8)
// PE 0x10177530
PT_SETFLT(pt_s146, 0xDC)
PT_GETF(pt_s147, 0xDC)
PT_GETD(pt_s149, 0xE0)
PT_GETD(pt_s151, 0xE4)
// PE 0x101775A0
PT_SETF(pt_s152, 0xB4)
PT_GETF(pt_s153, 0xB4)
// PE 0x101775C0
PT_SETF(pt_s154, 0xB0)
PT_GETF(pt_s155, 0xB0)
// PE 0x10177600
PT_SETB(pt_s156, 0x29C)
PT_GETB(pt_s157, 0x29C)

// complex slots (recovered bodies)

// slot 3 (0x100ED3B0): shared const-0 stub.
unsigned RD9_THUNK pt_s3(void* RD9_THUNK_EDX) { return 0; }

// slot 2 (0x10176F00): recompute cached/derived scalars, then re-drive slot45/slot114 and the UV
// recompute. The +0x168 term consults g_LightManager when f+0x1F8 < 0. The published boot surface
// provides the typed +0x270 getter; the null fallback remains the shipped guarded arm.
void RD9_THUNK pt_s2(void* self RD9_THUNK_EDX)
{
    if (F(self, 0x1F8) < 0.0f)
    {
        if (g_LightManager)
            F(self, 0x168) =
                g_LightManager->getDefaultEffectLightAffectionFactor();
        else
            U(self, 0x168) = 0;
    }
    else
        U(self, 0x168) = U(self, 0x1F8);

    F(self, 0xC0) = (F(self, 0xB8) == F(self, 0xBC)) ? 0.0f : 1.0f / (F(self, 0xB8) - F(self, 0xBC));
    F(self, 0x14) = (F(self, 0x1D4) <= 0.0f) ? 0.2f : 1.0f / F(self, 0x1D4);
    if (U(self, 0xE4)) F(self, 0xE8) = 1.0f / static_cast<float>(U(self, 0xE4));
    else               U(self, 0xE8) = 0;

    pt_s45 (self RD9_THUNK_ARG, psmFloatArgFromBits(U(self, 0x84)));
    pt_s114(self RD9_THUNK_ARG, static_cast<int>(U(self, 0x78)));
    templateRecomputeUV(self);
}

// slots 23/29 (0x10177460/0x101771E0): set a scalar then recompute.
void RD9_THUNK pt_s23(void* self RD9_THUNK_EDX, PsmFloatArg v) { U(self, 0x10)  = psmFloatArgBits(v); pt_s2(self RD9_THUNK_ARG); }
// PE 0x101771E0
void RD9_THUNK pt_s29(void* self RD9_THUNK_EDX, PsmFloatArg v)
{
    U(self, 0x1D4) = psmFloatArgBits(v);
#if !defined(_MSC_VER)
    static_cast<IParticleSystemTemplate*>(self)->refreshDerivedState();
#else
    typedef void (__thiscall* Refresh)(void*);
    reinterpret_cast<Refresh>((*static_cast<void***>(self))[2])(self);
#endif
}

// slot 37 (0x101769F0): store a Vec3 at +0x20C then normalize it in place (sub_100777A0).
void RD9_THUNK pt_s37(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x20C) = v[0]; F(self, 0x210) = v[1]; F(self, 0x214) = v[2];
    float* p = &psmDescriptor0<float>(self, 0x20C);
    float s = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
    if (fabsf(s - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(s) < FLT_EPSILON) { p[0] = p[1] = p[2] = 0.0f; }
        else { float inv = 1.0f / sqrtf(s); p[0] *= inv; p[1] *= inv; p[2] *= inv; }
    }
}

// slot 45 (0x10175FF0): set +0x84 float; recompute its reciprocal at +0x88 and, when the +0xCC flag
// is set, the +0xD0 rate.
void RD9_THUNK pt_s45(void* self RD9_THUNK_EDX, PsmFloatArg a)
{
    U(self, 0x84) = psmFloatArgBits(a);
    float fa = F(self, 0x84);
    if (fa > 0.0f) F(self, 0x88) = 1.0f / fa;
    if (Bt(self, 0xCC) && fa > 0.0f) F(self, 0xD0) = static_cast<float>(U(self, 0xE0)) / fa;
}

// slot 101 (0x10176740): Vec3 written to both +0x1DC and the mirror +0x15C.
void RD9_THUNK pt_s101(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1DC) = v[0]; F(self, 0x1E0) = v[1]; F(self, 0x1E4) = v[2];
    F(self, 0x15C) = v[0]; F(self, 0x160) = v[1]; F(self, 0x164) = v[2];
}

// slot 103 (0x101767C0): Vec3(rgb) packed to a Vec4 {r,g,b,1} at +0x1E8 and mirror +0x16C
// (sub_1008DD80 is a Vec3+scalar -> Vec4 pack with w = 1.0).
void RD9_THUNK pt_s103(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1E8) = v[0]; F(self, 0x1EC) = v[1]; F(self, 0x1F0) = v[2]; F(self, 0x1F4) = 1.0f;
    F(self, 0x16C) = v[0]; F(self, 0x170) = v[1]; F(self, 0x174) = v[2]; F(self, 0x178) = 1.0f;
}

// slot 104 (0x10176830): get the +0x1E8 color as a Vec3 into out (sub_100E11B0 is an identity copy).
void* RD9_THUNK pt_s104(void* self RD9_THUNK_EDX, float* out)
{
    out[0] = F(self, 0x1E8); out[1] = F(self, 0x1EC); out[2] = F(self, 0x1F0);
    return out;
}

// slot 105 (0x10176860): Vec3 packed to a Vec4 {r,g,b,1} at +0x1FC (no mirror).
void RD9_THUNK pt_s105(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1FC) = v[0]; F(self, 0x200) = v[1]; F(self, 0x204) = v[2]; F(self, 0x208) = 1.0f;
}

// slot 114 (PE 10175EF0, full instruction stream read): +2A0 is a renderer collector,
// not a simulation callback. The manager's +8BA8 flag forces sorted collection.
void RD9_THUNK pt_s114(void* self RD9_THUNK_EDX, int type)
{
    U(self, 0x78) = static_cast<unsigned>(type);
    const bool mgrFlag = particleSystemManagerForceSortedParticles(g_ParticleSystemManager) != 0;

    if (mgrFlag || type == 0)
        psmDescriptor0<void*>(self, 0x2A0) = reinterpret_cast<void*>(&particleRendererCollectSorted);
    else if (type == 1 || type == 2)
        psmDescriptor0<void*>(self, 0x2A0) = reinterpret_cast<void*>(&particleRendererCollectAdditive);
    else
    {
        psmDescriptor0<void*>(self, 0x2A0) = 0;
        static bool noted = false;
        if (!noted) { noted = true; ::renddx9abi::logf("ParticleSystemTemplate::setParticleType: unknown particle type %d", type); }
    }
    pt_s89(self RD9_THUNK_ARG, psmFloatArgFromBits(U(self, 0x148)));   // slot 89, arg = current +0x148
}

// slot 119 (0x10176590): get the +0x13C Vec2 into out (the retail scratch-static bounce is a compiler
// temp, not shared state - modelled as a plain copy).
void* RD9_THUNK pt_s119(void* self RD9_THUNK_EDX, float* out)
{
    out[0] = F(self, 0x13C); out[1] = F(self, 0x140);
    return out;
}

// slot 130 (0x10176DD0), loader a changed name is assigned before loading.
// A failed load clears the stored name. Passing empty preserves the old texture/region pair.
void RD9_THUNK pt_s130(void* self RD9_THUNK_EDX, const ParticleHostString& source)
{
    ParticleHostString* dst = &psmDescriptor0<ParticleHostString>(self, 0x224);
    if (dst->size() != source.size() || ::memcmp(dst->c_str(), source.c_str(), source.size()) != 0)
    {
        *dst = source;
        // The resource managers take the renderer's own string type.
        const std::string& resourceName = ::renddx9abi::fromHostString(source);
        dx9rend::EngineTexture*& texture = psmDescriptor0<dx9rend::EngineTexture*>(self, 0x25C);
        dx9rend::TextureAtlasEntry*& region = psmDescriptor0<dx9rend::TextureAtlasEntry*>(self, 0x260);
        if (!particleTemplateLoadTexture(texture, region, resourceName,
                "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\ParticleSystem\\ParticleSystemTemplate.cpp", 300))
            *dst = "";
    }
}

// slot 132 (0x10175D90): set the +0xCC bool; when set (and +0x84 > 0) recompute the +0xD0 rate.
void RD9_THUNK pt_s132(void* self RD9_THUNK_EDX, unsigned b)
{
    Bt(self, 0xCC) = static_cast<unsigned char>(b);
    if (psmBoolArg(b) && F(self, 0x84) > 0.0f) F(self, 0xD0) = static_cast<float>(U(self, 0xE0)) / F(self, 0x84);
}

// slots 140/141/142/143 (0x101765F0/0x10176630/0x10176690/0x101766E0): set/get the +0xD8/+0xDC rates
// scaled by direct-texture dimensions at (+0x25C)+0x28/+0x2C. The atlas-region path leaves this
// pointer null, retaining the shipped no-op/zero branches.
void RD9_THUNK pt_s140(void* self RD9_THUNK_EDX, unsigned a)
{
    void* atlas = psmDescriptor0<void*>(self, 0x25C);
    if (atlas) F(self, 0xD8) = static_cast<float>(a) / static_cast<float>(static_cast<dx9rend::EngineTexture*>(atlas)->width);
}
// PE 0x10176630
unsigned RD9_THUNK pt_s141(void* self RD9_THUNK_EDX)
{
    void* atlas = psmDescriptor0<void*>(self, 0x25C);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Keep unsigned FILD and FMUL live through FISTP.
    if (atlas) return static_cast<unsigned>(bf2x87::toIntTruncate(
        bf2x87::wide(static_cast<dx9rend::EngineTexture*>(atlas)->width)*bf2x87::wide(F(self, 0xD8))));
#else
    if (atlas) return static_cast<unsigned>(static_cast<int>(static_cast<double>(static_cast<dx9rend::EngineTexture*>(atlas)->width) * F(self, 0xD8)));
#endif
    return 0;
}
// PE 0x10176690
void RD9_THUNK pt_s142(void* self RD9_THUNK_EDX, unsigned a)
{
    void* atlas = psmDescriptor0<void*>(self, 0x25C);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] The numerator spills to float; the unsigned divisor stays live.
    if (atlas) F(self, 0xDC) = static_cast<float>(bf2x87::wide(static_cast<float>(a))
        /bf2x87::wide(static_cast<dx9rend::EngineTexture*>(atlas)->height));
#else
    if (atlas) F(self, 0xDC) = static_cast<float>(static_cast<double>(static_cast<float>(a))
        /static_cast<double>(static_cast<dx9rend::EngineTexture*>(atlas)->height));
#endif
}
// PE 0x101766E0
unsigned RD9_THUNK pt_s143(void* self RD9_THUNK_EDX)
{
    void* atlas = psmDescriptor0<void*>(self, 0x25C);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Keep unsigned FILD and FMUL live through FISTP.
    if (atlas) return static_cast<unsigned>(bf2x87::toIntTruncate(
        bf2x87::wide(static_cast<dx9rend::EngineTexture*>(atlas)->height)*bf2x87::wide(F(self, 0xDC))));
#else
    if (atlas) return static_cast<unsigned>(static_cast<int>(static_cast<double>(static_cast<dx9rend::EngineTexture*>(atlas)->height) * F(self, 0xDC)));
#endif
    return 0;
}

// slot 150 (0x10175E70): set the +0xE4 count; when nonzero cache 1/count at +0xE8 and push it to both
// rate fields via slot144/slot146.
void RD9_THUNK pt_s150(void* self RD9_THUNK_EDX, unsigned a)
{
    U(self, 0xE4) = a;
    if (a)
    {
        float inv = 1.0f / static_cast<float>(a);
        F(self, 0xE8) = inv;
        pt_s144(self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(inv)));
        pt_s146(self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(inv)));
    }
}

// slot 148 (0x10175DF0): set the +0xE0 count; cascade to slot150 when +0xE4 is set, then recompute
// the +0xD0 rate when the +0xCC flag is set.
void RD9_THUNK pt_s148(void* self RD9_THUNK_EDX, int a)
{
    U(self, 0xE0) = static_cast<unsigned>(a);
    if (U(self, 0xE4)) pt_s150(self RD9_THUNK_ARG, U(self, 0xE4));
    if (Bt(self, 0xCC) && F(self, 0x84) > 0.0f) F(self, 0xD0) = static_cast<float>(U(self, 0xE0)) / F(self, 0x84);
}

// sub_10176E20 (tail of slot 2): recompute UV base/scale from the borrowed +0x260 texture region.
// The direct-texture/no-texture branch clears the shorts and uses rates or {1,1}.
void templateRecomputeUV(void* self)
{
    const dx9rend::TextureAtlasEntry* region = psmDescriptor0<dx9rend::TextureAtlasEntry*>(self, 0x260);
    float u, v;
    if (region)
    {
        S16(self, 0x21E) = static_cast<short>(region->uOffset * 32767.0f);
        S16(self, 0x220) = static_cast<short>(region->vOffset * 32767.0f);
        if (Bt(self, 0xD4)) { u = F(self, 0xD8) * region->width; v = F(self, 0xDC) * region->height; }
        else                { u = region->width;                 v = region->height; }
    }
    else
    {
        S16(self, 0x21E) = 0;
        S16(self, 0x220) = 0;
        if (Bt(self, 0xD4)) { u = F(self, 0xD8); v = F(self, 0xDC); }
        else                { u = 1.0f;          v = 1.0f; }
    }
    float uv[2] = { u, v };
    pt_s118(self RD9_THUNK_ARG, uv);
}

// Type 0 descriptor members that nothing calls or references in 1.50: unreachable in 1.50: no code or
// data references. Each body is the listing's own. The names are [CHOSEN, not recovered]: no caller
// exposes them.

// PE 0x10175D40: recompute the +0xD0 rate (+0xE0 count over the +0x84 value) when the +0xCC flag is set
// and +0x84 is positive; the same arithmetic is inlined in slots 45, 132 and 148.
void templateRecomputeRate(void* self)
{
    if (Bt(self, 0xCC) && F(self, 0x84) > 0.0f) F(self, 0xD0) = static_cast<float>(U(self, 0xE0)) / F(self, 0x84);
}

// PE 0x10175ED0: store the +0xE8 float (the reciprocal cache that slot 150 writes). The float argument
// type is [CHOSEN, not recovered]: the paired getter at 0x10175EE0 answers st0.
void templateSetReciprocalCache(void* self, float value)
{
    U(self, 0xE8) = fbits(value);
}

// PE 0x10176160: store the +0xC8 float; the paired getter at 0x10176170 answers st0.
void templateSetC8(void* self, float value)
{
    U(self, 0xC8) = fbits(value);
}

// PE 0x10176EE0: load the texture named by the stored +0x224 name (the result is not tested), then
// recompute the UV base and scale (tail jump to templateRecomputeUV, PE 0x10176E20).
void templateReloadTexture(void* self)
{
    ParticleHostString& name = psmDescriptor0<ParticleHostString>(self, 0x224);
    dx9rend::EngineTexture*& texture = psmDescriptor0<dx9rend::EngineTexture*>(self, 0x25C);
    dx9rend::TextureAtlasEntry*& region = psmDescriptor0<dx9rend::TextureAtlasEntry*>(self, 0x260);
    particleTemplateLoadTexture(texture, region, ::renddx9abi::fromHostString(name),
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\ParticleSystem\\ParticleSystemTemplate.cpp", 300);
    templateRecomputeUV(self);
}

// [CHOSEN, not recovered] Reader that PE 0x10175CE0 calls through its slot 4 (+0x10) with a destination
// and a byte count. No caller exists in any image, so the real type of the argument is unobservable;
// the first four slots are placeholders that keep the slot position.
struct TemplateByteSource
{
    virtual void unusedSlot0() = 0;
    virtual void unusedSlot1() = 0;
    virtual void unusedSlot2() = 0;
    virtual void unusedSlot3() = 0;
    virtual void read(void* destination, unsigned int size) = 0;
};

// PE 0x10175CE0: read the 0x214-byte block at +0x10 from the supplied source, then re-drive slot 114
// (particle type, argument +0x78).
void templateLoadFromSource(void* self, TemplateByteSource* source)
{
    source->read(&Bt(self, 0x10), 0x214);
    pt_s114(self RD9_THUNK_ARG, static_cast<int>(U(self, 0x78)));
}

// PE 0x10177730, 0x10176BA0
// slot 0 (the deleting wrapper 0x10177730 and the destructor body 0x10176BA0; the shipped body releases
// the texture, nulls the field and
// repeats the guarded release, which is a no-op): release the direct texture, destroy the two embedded
// strings, then free the block when the deleting flag is set. The atlas region is borrowed.
void* RD9_THUNK pt_dtor(void* self RD9_THUNK_EDX, int flags)
{
    dx9rend::EngineTexture*& texture = psmDescriptor0<dx9rend::EngineTexture*>(self, 0x25C);
    dx9rend::textureReleaseRef(texture);
    psmDescriptor0<ParticleHostString>(self, 0x240).~ParticleHostString();
    psmDescriptor0<ParticleHostString>(self, 0x224).~ParticleHostString();
    if (flags & 1)
        ::renddx9abi::engineFree(self);
    return self;
}

// The 158-slot primary vtable (off_1021F300).
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Match the shared consumer interface under the native ABI.
const void* const kVtable[] =
{
    BF2_NATIVE_DTOR(pt_dtor, complete),
    BF2_NATIVE_DTOR(pt_dtor, deleting),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getName, pt_s1),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, refreshDerivedState, pt_s2),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getSystemType, pt_s3),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitPrio, pt_s4),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitPrio, pt_s5),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setLodDistance, pt_s6),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getLodDistance, pt_s7),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setQualityLevel, pt_s8),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getQualityLevel, pt_s9),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitRadius, pt_s10),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitRadius, pt_s11),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRadialDirection, pt_s12),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRadialDirection, pt_s13),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRandomRadialDirection, pt_s14),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRandomRadialDirection, pt_s15),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitterType, pt_s16),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitRangeX, pt_s17),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitRangeX, pt_s18),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitRangeY, pt_s19),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitRangeY, pt_s20),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitRangeZ, pt_s21),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitRangeZ, pt_s22),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitFrequency, pt_s23),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitFrequency, pt_s24),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitSpeed, pt_s25),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitSpeed, pt_s26),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setPreWarmTime, pt_s27),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getPreWarmTime, pt_s28),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitTime, pt_s29),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitTime, pt_s30),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setIsLooping, pt_s31),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getIsLooping, pt_s32),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitDelay, pt_s33),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitDelay, pt_s34),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRandomDirectionAngle, pt_s35),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRandomDirectionAngle, pt_s36),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitDirection, pt_s37),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitDirection, pt_s38),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRotationGraph, pt_s39),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRotationGraph, pt_s40),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAlignRotationToSpeed, pt_s41),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAlignRotationToSpeed, pt_s42),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRandomTimeToLive, pt_s43),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRandomTimeToLive, pt_s44),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setTimeToLive, pt_s45),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getTimeToLive, pt_s46),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setGravity, pt_s47),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getGravity, pt_s48),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAirResistance, pt_s49),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAirResistance, pt_s50),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRandomSpeed, pt_s51),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRandomSpeed, pt_s52),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setGravityGraph, pt_s53),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getGravityGraph, pt_s54),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAirResistanceGraph, pt_s55),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAirResistanceGraph, pt_s56),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setEmitSpeedGraph, pt_s57),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getEmitSpeedGraph, pt_s58),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setTransparencyGraph, pt_s59),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getTransparencyGraph, pt_s60),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setWindAffectionFactor, pt_s61),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getWindAffectionFactor, pt_s62),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setCollisionEnable, pt_s63),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getCollisionEnable, pt_s64),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setCollisionSpeedAlongNormal, pt_s65),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getCollisionSpeedAlongNormal, pt_s66),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setCollisionSpeedAlongPlane, pt_s67),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getCollisionSpeedAlongPlane, pt_s68),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setIsLocalSystem, pt_s69),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getIsLocalSystem, pt_s70),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setForceToWaterSurface, pt_s71),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getForceToWaterSurface, pt_s72),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setWaterSurfaceOffset, pt_s73),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getWaterSurfaceOffset, pt_s74),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setGasCloudType, pt_s75),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getGasCloudType, pt_s76),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setBlurryVisionTime, pt_s77),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getBlurryVisionTime, pt_s78),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setBlurryVisionHoldTime, pt_s79),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getBlurryVisionHoldTime, pt_s80),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setMinGasCloudRadius, pt_s81),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getMinGasCloudRadius, pt_s82),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setMaxGasCloudRadius, pt_s83),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getMaxGasCloudRadius, pt_s84),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setGasCloudRadiusTime, pt_s85),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getGasCloudRadiusTime, pt_s86),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setGasCloudDamage, pt_s87),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getGasCloudDamage, pt_s88),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setParticleMaxSize, pt_s89),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getParticleMaxSize, pt_s90),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRandomSize, pt_s91),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRandomSize, pt_s92),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRandomIntensity, pt_s93),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRandomIntensity, pt_s94),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setSizeGraph, pt_s95),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getSizeGraph, pt_s96),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAlphaCull, pt_s97),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAlphaCull, pt_s98),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setLightAffectionFactor, pt_s99),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getLightAffectionFactor, pt_s100),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setColor1, pt_s101),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getColor1, pt_s102),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setColor2, pt_s103),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getColor2, pt_s104),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, reserved105, pt_s105),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setColorBlendGraph, pt_s106),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getColorBlendGraph, pt_s107),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setLightMapIntensityOffset, pt_s108),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getLightMapIntensityOffset, pt_s109),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setDirectionalScale, pt_s110),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getDirectionalScale, pt_s111),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setIsStaticSystem, pt_s112),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getIsStaticSystem, pt_s113),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setParticleType, pt_s114),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getParticleTypeRef, pt_s115),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setDrawOrder, pt_s116),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getDrawOrder, pt_s117),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, reserved118, pt_s118),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, reserved119, pt_s119),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRandomRotationSpeed, pt_s120),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRandomRotationSpeed, pt_s121),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRotationSpeed, pt_s122),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRotationSpeed, pt_s123),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setPivotOffset, pt_s124),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getPivotOffset, pt_s125),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setSelfShadowIntensity, pt_s126),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getSelfShadowIntensity, pt_s127),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setRandomRotation, pt_s128),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getRandomRotation, pt_s129),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setTextureName, pt_s130),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getTextureName, pt_s131),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationPlayOnce, pt_s132),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationPlayOnce, pt_s133),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationEnable, pt_s134),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationEnable, pt_s135),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationRandomizedStartFrame, pt_s136),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationRandomizedStartFrame, pt_s137),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationSpeed, pt_s138),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationSpeed, pt_s139),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationFrameWidth, pt_s140),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationFrameWidth, pt_s141),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationFrameHeight, pt_s142),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationFrameHeight, pt_s143),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationFrameWidthRelative, pt_s144),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationFrameWidthRelative, pt_s145),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationFrameHeightRelative, pt_s146),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationFrameHeightRelative, pt_s147),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationFrameCount, pt_s148),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationFrameCount, pt_s149),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, setAnimationFrameCountX, pt_s150),
    BF2_NATIVE_SLOT(::dice::hfe::geom::IParticleSystemTemplateType0, getAnimationFrameCountX, pt_s151),
    reinterpret_cast<void*>(&pt_s152),
    reinterpret_cast<void*>(&pt_s153),
    reinterpret_cast<void*>(&pt_s154),
    reinterpret_cast<void*>(&pt_s155),
    reinterpret_cast<void*>(&pt_s156),
    reinterpret_cast<void*>(&pt_s157),
};
#else
const void* const kVtable[158] =
{
    reinterpret_cast<void*>(&pt_dtor), reinterpret_cast<void*>(&pt_s1),   reinterpret_cast<void*>(&pt_s2),   reinterpret_cast<void*>(&pt_s3),
    reinterpret_cast<void*>(&pt_s4),   reinterpret_cast<void*>(&pt_s5),   reinterpret_cast<void*>(&pt_s6),   reinterpret_cast<void*>(&pt_s7),
    reinterpret_cast<void*>(&pt_s8),   reinterpret_cast<void*>(&pt_s9),   reinterpret_cast<void*>(&pt_s10),  reinterpret_cast<void*>(&pt_s11),
    reinterpret_cast<void*>(&pt_s12),  reinterpret_cast<void*>(&pt_s13),  reinterpret_cast<void*>(&pt_s14),  reinterpret_cast<void*>(&pt_s15),
    reinterpret_cast<void*>(&pt_s16),  reinterpret_cast<void*>(&pt_s17),  reinterpret_cast<void*>(&pt_s18),  reinterpret_cast<void*>(&pt_s19),
    reinterpret_cast<void*>(&pt_s20),  reinterpret_cast<void*>(&pt_s21),  reinterpret_cast<void*>(&pt_s22),  reinterpret_cast<void*>(&pt_s23),
    reinterpret_cast<void*>(&pt_s24),  reinterpret_cast<void*>(&pt_s25),  reinterpret_cast<void*>(&pt_s26),  reinterpret_cast<void*>(&pt_s27),
    reinterpret_cast<void*>(&pt_s28),  reinterpret_cast<void*>(&pt_s29),  reinterpret_cast<void*>(&pt_s30),  reinterpret_cast<void*>(&pt_s31),
    reinterpret_cast<void*>(&pt_s32),  reinterpret_cast<void*>(&pt_s33),  reinterpret_cast<void*>(&pt_s34),  reinterpret_cast<void*>(&pt_s35),
    reinterpret_cast<void*>(&pt_s36),  reinterpret_cast<void*>(&pt_s37),  reinterpret_cast<void*>(&pt_s38),  reinterpret_cast<void*>(&pt_s39),
    reinterpret_cast<void*>(&pt_s40),  reinterpret_cast<void*>(&pt_s41),  reinterpret_cast<void*>(&pt_s42),  reinterpret_cast<void*>(&pt_s43),
    reinterpret_cast<void*>(&pt_s44),  reinterpret_cast<void*>(&pt_s45),  reinterpret_cast<void*>(&pt_s46),  reinterpret_cast<void*>(&pt_s47),
    reinterpret_cast<void*>(&pt_s48),  reinterpret_cast<void*>(&pt_s49),  reinterpret_cast<void*>(&pt_s50),  reinterpret_cast<void*>(&pt_s51),
    reinterpret_cast<void*>(&pt_s52),  reinterpret_cast<void*>(&pt_s53),  reinterpret_cast<void*>(&pt_s54),  reinterpret_cast<void*>(&pt_s55),
    reinterpret_cast<void*>(&pt_s56),  reinterpret_cast<void*>(&pt_s57),  reinterpret_cast<void*>(&pt_s58),  reinterpret_cast<void*>(&pt_s59),
    reinterpret_cast<void*>(&pt_s60),  reinterpret_cast<void*>(&pt_s61),  reinterpret_cast<void*>(&pt_s62),  reinterpret_cast<void*>(&pt_s63),
    reinterpret_cast<void*>(&pt_s64),  reinterpret_cast<void*>(&pt_s65),  reinterpret_cast<void*>(&pt_s66),  reinterpret_cast<void*>(&pt_s67),
    reinterpret_cast<void*>(&pt_s68),  reinterpret_cast<void*>(&pt_s69),  reinterpret_cast<void*>(&pt_s70),  reinterpret_cast<void*>(&pt_s71),
    reinterpret_cast<void*>(&pt_s72),  reinterpret_cast<void*>(&pt_s73),  reinterpret_cast<void*>(&pt_s74),  reinterpret_cast<void*>(&pt_s75),
    reinterpret_cast<void*>(&pt_s76),  reinterpret_cast<void*>(&pt_s77),  reinterpret_cast<void*>(&pt_s78),  reinterpret_cast<void*>(&pt_s79),
    reinterpret_cast<void*>(&pt_s80),  reinterpret_cast<void*>(&pt_s81),  reinterpret_cast<void*>(&pt_s82),  reinterpret_cast<void*>(&pt_s83),
    reinterpret_cast<void*>(&pt_s84),  reinterpret_cast<void*>(&pt_s85),  reinterpret_cast<void*>(&pt_s86),  reinterpret_cast<void*>(&pt_s87),
    reinterpret_cast<void*>(&pt_s88),  reinterpret_cast<void*>(&pt_s89),  reinterpret_cast<void*>(&pt_s90),  reinterpret_cast<void*>(&pt_s91),
    reinterpret_cast<void*>(&pt_s92),  reinterpret_cast<void*>(&pt_s93),  reinterpret_cast<void*>(&pt_s94),  reinterpret_cast<void*>(&pt_s95),
    reinterpret_cast<void*>(&pt_s96),  reinterpret_cast<void*>(&pt_s97),  reinterpret_cast<void*>(&pt_s98),  reinterpret_cast<void*>(&pt_s99),
    reinterpret_cast<void*>(&pt_s100), reinterpret_cast<void*>(&pt_s101), reinterpret_cast<void*>(&pt_s102), reinterpret_cast<void*>(&pt_s103),
    reinterpret_cast<void*>(&pt_s104), reinterpret_cast<void*>(&pt_s105), reinterpret_cast<void*>(&pt_s106), reinterpret_cast<void*>(&pt_s107),
    reinterpret_cast<void*>(&pt_s108), reinterpret_cast<void*>(&pt_s109), reinterpret_cast<void*>(&pt_s110), reinterpret_cast<void*>(&pt_s111),
    reinterpret_cast<void*>(&pt_s112), reinterpret_cast<void*>(&pt_s113), reinterpret_cast<void*>(&pt_s114), reinterpret_cast<void*>(&pt_s115),
    reinterpret_cast<void*>(&pt_s116), reinterpret_cast<void*>(&pt_s117), reinterpret_cast<void*>(&pt_s118), reinterpret_cast<void*>(&pt_s119),
    reinterpret_cast<void*>(&pt_s120), reinterpret_cast<void*>(&pt_s121), reinterpret_cast<void*>(&pt_s122), reinterpret_cast<void*>(&pt_s123),
    reinterpret_cast<void*>(&pt_s124), reinterpret_cast<void*>(&pt_s125), reinterpret_cast<void*>(&pt_s126), reinterpret_cast<void*>(&pt_s127),
    reinterpret_cast<void*>(&pt_s128), reinterpret_cast<void*>(&pt_s129), reinterpret_cast<void*>(&pt_s130), reinterpret_cast<void*>(&pt_s131),
    reinterpret_cast<void*>(&pt_s132), reinterpret_cast<void*>(&pt_s133), reinterpret_cast<void*>(&pt_s134), reinterpret_cast<void*>(&pt_s135),
    reinterpret_cast<void*>(&pt_s136), reinterpret_cast<void*>(&pt_s137), reinterpret_cast<void*>(&pt_s138), reinterpret_cast<void*>(&pt_s139),
    reinterpret_cast<void*>(&pt_s140), reinterpret_cast<void*>(&pt_s141), reinterpret_cast<void*>(&pt_s142), reinterpret_cast<void*>(&pt_s143),
    reinterpret_cast<void*>(&pt_s144), reinterpret_cast<void*>(&pt_s145), reinterpret_cast<void*>(&pt_s146), reinterpret_cast<void*>(&pt_s147),
    reinterpret_cast<void*>(&pt_s148), reinterpret_cast<void*>(&pt_s149), reinterpret_cast<void*>(&pt_s150), reinterpret_cast<void*>(&pt_s151),
    reinterpret_cast<void*>(&pt_s152), reinterpret_cast<void*>(&pt_s153), reinterpret_cast<void*>(&pt_s154), reinterpret_cast<void*>(&pt_s155),
    reinterpret_cast<void*>(&pt_s156), reinterpret_cast<void*>(&pt_s157),
};
#endif

// sub_10176180 - the init helper: field defaults + ~18 setter calls establishing the descriptor's
// at-rest state, transcribed in shipped statement order. Runs at the tail of the constructor.
void initTemplateDefaults(void* self)
{
    U (self, 0x7C)  = 1u;
    U (self, 0xC8)  = static_cast<unsigned>(-1082130432);
    U (self, 0x0C)  = 0u;
    U (self, 0x264) = 0u;
    U (self, 0x24)  = 0u;
    U (self, 0x28)  = 0u;
    U (self, 0x2C)  = 0u;
    Bt(self, 0x30)  = 0;
    U (self, 0x38)  = 0u;
    U (self, 0x3C)  = 0u;
    U (self, 0x40)  = 0u;
    Bt(self, 0x44)  = 0;
    U (self, 0x4C)  = 0u;
    U (self, 0x50)  = 0u;
    U (self, 0x54)  = 0u;
    Bt(self, 0x58)  = 0;
    U (self, 0x70)  = 1135869952u;
    U (self, 0x74)  = 0u;
    Bt(self, 0x269) = 0;
    Bt(self, 0x6C)  = 0;
    U (self, 0x10)  = 1107296256u;
    U (self, 0x68)  = 0u;
    U (self, 0x60)  = 2u;
    U (self, 0x64)  = 2u;
    U (self, 0x18)  = 0u;
    U (self, 0x1C)  = 0u;
    U (self, 0x20)  = 0u;
    U (self, 0x1D0) = 0u;
    U (self, 0x1D4) = 0u;
    U (self, 0x1D8) = 0u;
    U (self, 0xB8)  = 1020054733u;
    U (self, 0xC0)  = 1116034389u;
    U (self, 0xC4)  = 1028443341u;
    U (self, 0x288) = 1086324736u;
    U (self, 0x28C) = 1056964608u;
    U (self, 0x290) = 1041865114u;
    U (self, 0x294) = 1082130432u;
    Bt(self, 0x29C) = 0;
    U (self, 0xA4)  = 0u;
    Bt(self, 0xA8)  = 0;
    U (self, 0xAC)  = 1022739087u;
    U (self, 0xB0)  = 0u;
    U (self, 0xB4)  = 1092616192u;
    U (self, 0x218) = 0u;
    Bt(self, 0x21C) = 0;
    U (self, 0x8C)  = static_cast<unsigned>(-1055056200);
    U (self, 0x90)  = 0u;
    U (self, 0x98)  = 1048576000u;
    U (self, 0x94)  = 0u;
    U (self, 0x9C)  = 1045220557u;
    U (self, 0xA0)  = 1065353216u;
    U (self, 0xBC)  = 1008981770u;
    U (self, 0x298) = 1008981770u;
    U (self, 0x1AC) = 0u;
    U (self, 0x1B0) = 0u;
    U (self, 0x1B4) = 0u;
    U (self, 0x1B8) = 0u;
    U (self, 0x1BC) = 0u;
    U (self, 0x1C0) = 0u;
    U (self, 0x1C4) = 0u;
    U (self, 0x1C8) = 0u;
    U (self, 0x1CC) = 0u;

    pt_s99(self RD9_THUNK_ARG, psmFloatArgFromBits(static_cast<unsigned>(-1082130432)));          // +0x1F8 = -1.0
    { float v[3] = { 1.0f, 1.0f, 1.0f }; pt_s101(self RD9_THUNK_ARG, v); }
    { float v[3] = { 1.0f, 1.0f, 1.0f }; pt_s103(self RD9_THUNK_ARG, v); }
    { float v[3] = { 1.0f, 1.0f, 1.0f }; pt_s105(self RD9_THUNK_ARG, v); }

    U (self, 0x158) = 0u;
    { ParticleHostString empty; psmDescriptor0<ParticleHostString>(self, 0x224) = empty; }
    Bt(self, 0xEC)  = 0;
    U (self, 0xF0)  = 1060320051u;
    U (self, 0xF4)  = 1061997773u;
    U (self, 0xF8)  = 2u;
    Bt(self, 0xCD)  = 0;
    U (self, 0xD0)  = 1065353216u;
    Bt(self, 0xCC)  = 0;
    Bt(self, 0xD4)  = 0;
    U (self, 0xD8)  = 0u;
    U (self, 0xDC)  = 0u;
    U (self, 0xE0)  = 0u;
    U (self, 0xE4)  = 0u;
    U (self, 0xE8)  = 0u;

    { float v[2] = { 1.0f, 1.0f };                 pt_s118(self RD9_THUNK_ARG, v); }
    pt_s89 (self RD9_THUNK_ARG, psmFloatArgFromBits(1065353216u));                 // +0x148 = 1.0
    pt_s108(self RD9_THUNK_ARG, psmFloatArgFromBits(0u));                           // +0x144 = 0
    pt_s114(self RD9_THUNK_ARG, 0);                            // particle type 0
    pt_s45 (self RD9_THUNK_ARG, psmFloatArgFromBits(1082130432u));                 // +0x84 = 4.0
    pt_s43 (self RD9_THUNK_ARG, psmFloatArgFromBits(0u));                           // +0x80 = 0
    { float v[4] = { 0.0f, 0.0f, 1.0f, 0.0f };     pt_s106(self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f };     pt_s59 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f };     pt_s95 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f };     pt_s39 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f };     pt_s53 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f };     pt_s55 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f };     pt_s57 (self RD9_THUNK_ARG, v); }

    U (self, 0x20C) = 0u;
    U (self, 0x210) = 1065353216u;
    U (self, 0x214) = 0u;
    U (self, 0x270) = 1101004800u;
    U (self, 0x274) = 1101004800u;
    Bt(self, 0x21D) = 0;
    U (self, 0x26C) = 0u;
    U (self, 0x278) = 1077936128u;
    U (self, 0x27C) = 1092616192u;
    U (self, 0x280) = 1077936128u;
    U (self, 0x284) = 1073741824u;

    pt_s2(self RD9_THUNK_ARG);                  // final recompute
}

} // anonymous namespace


// PE 0x10188150 (thiscall on the block): the 112-byte sub-block the type-0, type-2 and type-4
// template constructors embed, every dword stored zero. The block holds no string or pointer
// field, so its retail offsets are the native ones on every target. [INFERRED] name.
void psmZeroTemplateBlock(void* block)
{
    unsigned int* const words = static_cast<unsigned int*>(block);
    for (unsigned int i = 0; i < 28; ++i)
        words[i] = 0;
}

// geom_ParticleSystem_type0_ctor, 0x10177010. Installs the vtable, the ctor's own scalar defaults,
// the two embedded name strings (string2 = the template name), the zeroed sub-block at +0x13C
// (sub_10188150), and runs the init helper. A leading memset zeroes the fields retail hand-writes to
// 0 plus the .con property-load gaps; it is observably identical (those gaps are always written by the
// property loader before first read) and makes construction deterministic. [CHOSEN: leading memset]

void* constructParticleSystemTemplateType0(void* mem, const void* nameString)
{
    ::memset(mem, 0, PSM_DESCRIPTOR_SIZE(0x2A4u));

    *static_cast<const void* const**>(mem) = kVtable;

    // sub_10177010 non-zero scalar defaults.
    U(mem, 0x28) = 1065353216u;   // 1.0
    U(mem, 0x34) = 1973272912u;
    U(mem, 0x3C) = 1065353216u;   // 1.0
    U(mem, 0x48) = 1973272912u;
    U(mem, 0x50) = 1065353216u;   // 1.0
    U(mem, 0x5C) = 1973272912u;
    psmZeroTemplateBlock(&psmDescriptor0<unsigned char>(mem, 0x13C));   // 0x101770C5, redundant after the memset

    // The manager supplies the renderer's own string; the getters expose an owned HostString.
    new (&psmDescriptor0<unsigned char>(mem, 0x224)) ParticleHostString();
    new (&psmDescriptor0<unsigned char>(mem, 0x240)) ParticleHostString(
        ::renddx9abi::toHostString(*static_cast<const std::string*>(nameString)));

    initTemplateDefaults(mem);
    return mem;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
