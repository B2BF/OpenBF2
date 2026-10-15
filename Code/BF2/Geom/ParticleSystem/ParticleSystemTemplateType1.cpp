// Mesh-particle emitter descriptor; simulation and rendering live in its instances.
// [INFERRED] source placement. Owns its resolved geometry instance and releases it on rename.
// Instance construction retries geometry lookup if the template was unavailable earlier.
// [CHOSEN] ParticleHostString keeps host-facing names compatible with both plugin ABIs.
// The optional texture at +0x210 starts null and has no known assignment path.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "ParticleRawLayout.h"
#include "../XpLightManager.h"
#include "../MeshTemplate.h"                                  // G1: IGeometryTemplate
#include "../GeometryTemplateManager.h"                       // G1: IGeometryTemplateManager
#include "../../RendDX9/Texture.h"                            // G1: textureReleaseRef
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
extern IBase* g_GeometryTemplateManager;                      // G1: ModuleGeom.cpp, dword_1024B8F4

namespace {

// G1-e: the siblings' carrier (ParticleSystemTemplate.cpp, TrailSystemTemplate.cpp,
// NSAParticleSystemTemplate.cpp). [CHOSEN, not recovered] BF2 passes std::string through the
// descriptor interface. Each object owns its host strings in the original 28-byte reserved spans
// (x64: 32-byte tail slots, ParticleRawLayout.h);
// every later offset is retained.
using ParticleHostString = ::renddx9abi::HostString;
static_assert(sizeof(ParticleHostString) <= kPsmDescriptorStringSpan, "particle host string exceeds its reserved span");

inline unsigned&      U (void* p, unsigned off) { return psmDescriptor1<unsigned>(p, off); }
inline float&         F (void* p, unsigned off) { return psmDescriptor1<float>(p, off); }
inline unsigned char& Bt(void* p, unsigned off) { return psmDescriptor1<unsigned char>(p, off); }
inline unsigned       fbits(float f) { unsigned u; ::memcpy(&u, &f, 4); return u; }

// G1: the two typed views the geometry hookup uses.
inline ParticleHostString& geometryName(void* self)
{ return psmDescriptor1<ParticleHostString>(self, 0x70); }
inline IBase*& geometryInstance(void* self)
{ return psmDescriptor1<IBase*>(self, 0x8C); }

#define PT_SET(name, off)  void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ U(s,(off)) = v; }
#define PT_SETFLT(name, off) void RD9_THUNK name(void* s RD9_THUNK_EDX, PsmFloatArg v){ U(s,(off)) = psmFloatArgBits(v); }
#define PT_GETF(name, off) float    RD9_THUNK name(void* s RD9_THUNK_EDX){ return F(s,(off)); }
#define PT_GETD(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return U(s,(off)); }
#define PT_SETB(name, off) void     RD9_THUNK name(void* s RD9_THUNK_EDX, unsigned v){ Bt(s,(off)) = static_cast<unsigned char>(v); }
#define PT_GETB(name, off) unsigned RD9_THUNK name(void* s RD9_THUNK_EDX){ return Bt(s,(off)); }
#define PT_LEA(name, off)  void*    RD9_THUNK name(void* s RD9_THUNK_EDX){ return &psmDescriptor1<unsigned char>(s, (off)); }
#define PT_SETVEC(name, off, n) void RD9_THUNK name(void* s RD9_THUNK_EDX, const float* v){ for (int i=0;i<(n);++i) F(s,(off)+4u*i)=v[i]; }
#define PT_GETVEC(name, off, n) void* RD9_THUNK name(void* s RD9_THUNK_EDX, float* o){ for (int i=0;i<(n);++i) o[i]=F(s,(off)+4u*i); return o; }
#define PT_VEC3A(name, off) void RD9_THUNK name(void* s RD9_THUNK_EDX, PSM_VEC3_PARAMS){ U(s,(off))=PSM_VEC3_X; U(s,(off)+4)=PSM_VEC3_Y; U(s,(off)+8)=PSM_VEC3_Z; }

void RD9_THUNK n1_s2 (void* s RD9_THUNK_EDX);
void RD9_THUNK n1_s45(void* s RD9_THUNK_EDX, PsmFloatArg a);

// simple slots
// G1-e: slot 1 (0x101790D0, `lea eax,[ecx+1F4h]`) - the name, as the host's string type.
const ParticleHostString& RD9_THUNK n1_s1(void* self RD9_THUNK_EDX)
{ return psmDescriptor1<ParticleHostString>(self, 0x1F4); }
PT_SET (n1_s4,  0x64)
PT_GETD(n1_s5,  0x64)
PT_SET (n1_s6,  0x68)
PT_GETD(n1_s7,  0x68)
PT_SET (n1_s8,  0x6C)
PT_GETD(n1_s9,  0x6C)
PT_VEC3A(n1_s10, 0x14)
PT_GETVEC(n1_s11, 0x14, 3)
PT_SETFLT(n1_s12, 0x60)
PT_GETF(n1_s13, 0x60)
PT_SETFLT(n1_s14, 0x5C)
PT_GETF(n1_s15, 0x5C)
PT_SET (n1_s16, 0x214)
PT_SETVEC(n1_s17, 0x20, 5)
PT_LEA (n1_s18, 0x20)
PT_SETVEC(n1_s19, 0x34, 5)
PT_LEA (n1_s20, 0x34)
PT_SETVEC(n1_s21, 0x48, 5)
PT_LEA (n1_s22, 0x48)
PT_GETF(n1_s24, 0x10)
PT_SETFLT(n1_s25, 0x190)
PT_GETF(n1_s26, 0x190)
PT_SETFLT(n1_s27, 0x0C)
PT_GETF(n1_s28, 0x0C)
PT_SETFLT(n1_s29, 0x194)
PT_GETF(n1_s30, 0x194)
PT_SETB(n1_s31, 0x198)
PT_GETB(n1_s32, 0x198)
PT_SETFLT(n1_s33, 0x1A4)
PT_GETF(n1_s34, 0x1A4)
PT_SETVEC(n1_s35, 0x184, 3)
PT_GETVEC(n1_s36, 0x184, 3)
PT_GETVEC(n1_s38, 0x1DC, 3)
PT_SETVEC(n1_s39, 0xFC, 4)
PT_LEA (n1_s40, 0xFC)
PT_SETB(n1_s41, 0x1F0)
PT_GETB(n1_s42, 0x1F0)
PT_SETFLT(n1_s43, 0x98)
PT_GETF(n1_s44, 0x98)
PT_GETF(n1_s46, 0x9C)
PT_SETFLT(n1_s47, 0xA4)
PT_GETF(n1_s48, 0xA4)
PT_SETFLT(n1_s49, 0xA8)
PT_GETF(n1_s50, 0xA8)
PT_SETFLT(n1_s51, 0x180)
PT_GETF(n1_s52, 0x180)
PT_SETVEC(n1_s53, 0x10C, 4)
PT_LEA (n1_s54, 0x10C)
PT_SETVEC(n1_s55, 0x11C, 4)
PT_LEA (n1_s56, 0x11C)
PT_SETVEC(n1_s57, 0x15C, 4)
PT_LEA (n1_s58, 0x15C)
PT_SETVEC(n1_s59, 0x13C, 4)
PT_LEA (n1_s60, 0x13C)
PT_SETFLT(n1_s61, 0xCC)
PT_GETF(n1_s62, 0xCC)
PT_SETB(n1_s63, 0xEC)
PT_GETB(n1_s64, 0xEC)
PT_SETFLT(n1_s65, 0xF0)
PT_GETF(n1_s66, 0xF0)
PT_SETFLT(n1_s67, 0xF4)
PT_GETF(n1_s68, 0xF4)
PT_SETB(n1_s69, 0x1F1)
PT_GETB(n1_s70, 0x1F1)
PT_SETB(n1_s71, 0xD0)
PT_GETB(n1_s72, 0xD0)
PT_SETFLT(n1_s73, 0xD4)
PT_GETF(n1_s74, 0xD4)
PT_SET (n1_s75, 0x21C)
PT_GETD(n1_s76, 0x21C)
PT_SETFLT(n1_s77, 0x220)
PT_GETF(n1_s78, 0x220)
PT_SETFLT(n1_s79, 0x224)
PT_GETF(n1_s80, 0x224)
PT_SETFLT(n1_s81, 0x228)
PT_GETF(n1_s82, 0x228)
PT_SETFLT(n1_s83, 0x22C)
PT_GETF(n1_s84, 0x22C)
PT_SETFLT(n1_s85, 0x230)
PT_GETF(n1_s86, 0x230)
PT_SETFLT(n1_s87, 0x234)
PT_GETF(n1_s88, 0x234)
PT_SETFLT(n1_s89, 0x1E8)
PT_GETF(n1_s90, 0x1E8)
PT_SETFLT(n1_s91, 0x16C)
PT_GETF(n1_s92, 0x16C)
PT_SETFLT(n1_s93, 0x170)
PT_GETF(n1_s94, 0x170)
PT_SETVEC(n1_s95, 0x14C, 4)
PT_LEA (n1_s96, 0x14C)
PT_SETFLT(n1_s97, 0xC4)
PT_GETF(n1_s98, 0xC4)
PT_SETFLT(n1_s99, 0x1A8)
PT_GETF(n1_s100, 0x1A8)
PT_SETVEC(n1_s101, 0x1AC, 3)
PT_GETVEC(n1_s102, 0x1AC, 3)
PT_GETVEC(n1_s104, 0x1BC, 3)
PT_SETVEC(n1_s106, 0x12C, 4)
PT_LEA (n1_s107, 0x12C)
PT_SETFLT(n1_s110, 0x1EC)
PT_GETF(n1_s111, 0x1EC)
PT_SETB(n1_s112, 0x218)
PT_GETB(n1_s113, 0x218)
// G1-e: slot 115 (0x10179F40, `lea eax,[ecx+70h]`) - the geometry name, as the host's string type.
const ParticleHostString& RD9_THUNK n1_s115(void* self RD9_THUNK_EDX)
{ return geometryName(self); }
PT_SET (n1_s116, 0x94)
PT_GETD(n1_s117, 0x94)
PT_SETB(n1_s118, 0x1F2)
PT_GETB(n1_s119, 0x1F2)
PT_SET (n1_s120, 0x90)
PT_GETD(n1_s121, 0x90)
PT_VEC3A(n1_s122, 0x174)
PT_GETVEC(n1_s123, 0x174, 3)
PT_VEC3A(n1_s124, 0xAC)
PT_GETVEC(n1_s125, 0xAC, 3)
PT_VEC3A(n1_s126, 0xB8)
PT_GETVEC(n1_s127, 0xB8, 3)

// complex slots

unsigned RD9_THUNK n1_s3(void* RD9_THUNK_EDX) { return 1; }        // emitter type id
void     RD9_THUNK n1_s108(void* RD9_THUNK_EDX, PsmFloatArg) { }   // nullsub (1-arg dispatch, ignored)
float    RD9_THUNK n1_s109(void* RD9_THUNK_EDX) { return -1.0f; }  // shipped returns -1.0

// slot 2 (0x10179240): LightManager auto-value + mode scale + recomputes.
void RD9_THUNK n1_s2(void* self RD9_THUNK_EDX)
{
    if (F(self, 0x1A8) < 0.0f)
    {
        if (g_LightManager)
            F(self, 0x1B8) =
                g_LightManager->getDefaultEffectLightAffectionFactor();
        else
            U(self, 0x1B8) = 0;
    }
    else
        U(self, 0x1B8) = U(self, 0x1A8);

    unsigned mode = U(self, 0x64);
    F(self, 0x1A0) = (mode == 0) ? 0.5f : (mode == 1) ? 0.7f : 1.0f;

    n1_s45(self RD9_THUNK_ARG, psmFloatArgFromBits(U(self, 0x9C)));

    F(self, 0xE0) = (F(self, 0xD8) == F(self, 0xDC)) ? 0.0f : 1.0f / (F(self, 0xD8) - F(self, 0xDC));
    if (F(self, 0x10) >= 1.1920929e-7f) F(self, 0x19C) = 1.0f / F(self, 0x10);
    else                                U(self, 0x19C) = 0;
}

// slot 23 (0x10179E90): store +0x10, cache 1/x at +0x19C (unconditional).
void RD9_THUNK n1_s23(void* self RD9_THUNK_EDX, PsmFloatArg a)
{
    U(self, 0x10) = psmFloatArgBits(a);
    F(self, 0x19C) = 1.0f / F(self, 0x10);
}

// slot 37 (0x10179960): store Vec3 at +0x1DC, normalize in place.
void RD9_THUNK n1_s37(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1DC) = v[0]; F(self, 0x1E0) = v[1]; F(self, 0x1E4) = v[2];
    float* p = &psmDescriptor1<float>(self, 0x1DC);
    float s = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
    if (fabsf(s - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(s) < FLT_EPSILON) { p[0] = p[1] = p[2] = 0.0f; }
        else { float inv = 1.0f / sqrtf(s); p[0] *= inv; p[1] *= inv; p[2] *= inv; }
    }
}

// slot 45 (0x101790E0): store +0x9C, cache 1/x at +0xA0 when positive.
void RD9_THUNK n1_s45(void* self RD9_THUNK_EDX, PsmFloatArg a)
{
    U(self, 0x9C) = psmFloatArgBits(a);
    if (F(self, 0x9C) > 0.0f) F(self, 0xA0) = 1.0f / F(self, 0x9C);
}

// slots 103/105 (0x10179760/0x101797D0): Vec3 arg promoted to Vec4 {x,y,z,1} at +0x1BC / +0x1CC.
void RD9_THUNK n1_s103(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1BC) = v[0]; F(self, 0x1C0) = v[1]; F(self, 0x1C4) = v[2]; F(self, 0x1C8) = 1.0f;
}
void RD9_THUNK n1_s105(void* self RD9_THUNK_EDX, const float* v)
{
    F(self, 0x1CC) = v[0]; F(self, 0x1D0) = v[1]; F(self, 0x1D4) = v[2]; F(self, 0x1D8) = 1.0f;
}


// Replaces the shared particle geometry instance resolved from its template name.
// A failed lookup leaves null after releasing the old instance; the manager must exist.

void resolveGeometry(void* self)
{
    if (!geometryName(self).empty())                                  // [esi+84h] != 0
    {
        IGeometryTemplateManager* const manager = static_cast<IGeometryTemplateManager*>(
            static_cast<void*>(g_GeometryTemplateManager));
        IGeometryTemplate* const geometryTemplate =
            manager->getTemplate(geometryName(self));                 // [edx+18h] (&this->+0x70)
        if (geometryTemplate)
        {
            void* const instance = geometryTemplate->createInstance(); // [edx+24h]
            if (instance)
                geometryInstance(self) = static_cast<IBase*>(instance);
        }
    }
}

// slot 114 (0x101796D0, 95 B, retn 4): setGeometryName.
void RD9_THUNK n1_s114(void* self RD9_THUNK_EDX, const ParticleHostString& name)
{
    geometryName(self) = name;                                        // MSVCP71 operator=
    if (IBase* const old = geometryInstance(self))
    {
        old->release();                                               // [edx+8]
        geometryInstance(self) = 0;
    }
    resolveGeometry(self);
}

// slot 0 (0x1017A280 -> sub_10179B10): release the geometry instance @+0x8C (IBase +0x08; the slot
// is NOT nulled - the object is going away), release the refcounted texture @+0x210 (G1-d: always
// null in 1.50), destroy the two strings, restore the base vtable (off_1021F120, not observable),
// free-if-flag.
void* RD9_THUNK n1_dtor(void* self RD9_THUNK_EDX, int flags)
{
    if (IBase* const geometry = geometryInstance(self))
        geometry->release();                                          // [eax+8]

    // `add [esi+4], -1; cmp [esi+4], 0; jnz; call 10014E20 (texture dtor); push esi; call
    // 1005F360` - exactly textureReleaseRef's body. The slot is dead in the destructor either way;
    // the local copy keeps the shipped "not nulled" shape.
    dx9rend::EngineTexture* texture =
        psmDescriptor1<dx9rend::EngineTexture*>(self, 0x210);
    dx9rend::textureReleaseRef(texture);

    psmDescriptor1<ParticleHostString>(self, 0x1F4).~ParticleHostString();
    geometryName(self).~ParticleHostString();
    if (flags & 1)
        ::renddx9abi::engineFree(self);
    return self;
}

const void* const kVtable[128] =
{
    reinterpret_cast<void*>(&n1_dtor), reinterpret_cast<void*>(&n1_s1),  reinterpret_cast<void*>(&n1_s2),  reinterpret_cast<void*>(&n1_s3),
    reinterpret_cast<void*>(&n1_s4),   reinterpret_cast<void*>(&n1_s5),  reinterpret_cast<void*>(&n1_s6),  reinterpret_cast<void*>(&n1_s7),
    reinterpret_cast<void*>(&n1_s8),   reinterpret_cast<void*>(&n1_s9),  reinterpret_cast<void*>(&n1_s10), reinterpret_cast<void*>(&n1_s11),
    reinterpret_cast<void*>(&n1_s12),  reinterpret_cast<void*>(&n1_s13), reinterpret_cast<void*>(&n1_s14), reinterpret_cast<void*>(&n1_s15),
    reinterpret_cast<void*>(&n1_s16),  reinterpret_cast<void*>(&n1_s17), reinterpret_cast<void*>(&n1_s18), reinterpret_cast<void*>(&n1_s19),
    reinterpret_cast<void*>(&n1_s20),  reinterpret_cast<void*>(&n1_s21), reinterpret_cast<void*>(&n1_s22), reinterpret_cast<void*>(&n1_s23),
    reinterpret_cast<void*>(&n1_s24),  reinterpret_cast<void*>(&n1_s25), reinterpret_cast<void*>(&n1_s26), reinterpret_cast<void*>(&n1_s27),
    reinterpret_cast<void*>(&n1_s28),  reinterpret_cast<void*>(&n1_s29), reinterpret_cast<void*>(&n1_s30), reinterpret_cast<void*>(&n1_s31),
    reinterpret_cast<void*>(&n1_s32),  reinterpret_cast<void*>(&n1_s33), reinterpret_cast<void*>(&n1_s34), reinterpret_cast<void*>(&n1_s35),
    reinterpret_cast<void*>(&n1_s36),  reinterpret_cast<void*>(&n1_s37), reinterpret_cast<void*>(&n1_s38), reinterpret_cast<void*>(&n1_s39),
    reinterpret_cast<void*>(&n1_s40),  reinterpret_cast<void*>(&n1_s41), reinterpret_cast<void*>(&n1_s42), reinterpret_cast<void*>(&n1_s43),
    reinterpret_cast<void*>(&n1_s44),  reinterpret_cast<void*>(&n1_s45), reinterpret_cast<void*>(&n1_s46), reinterpret_cast<void*>(&n1_s47),
    reinterpret_cast<void*>(&n1_s48),  reinterpret_cast<void*>(&n1_s49), reinterpret_cast<void*>(&n1_s50), reinterpret_cast<void*>(&n1_s51),
    reinterpret_cast<void*>(&n1_s52),  reinterpret_cast<void*>(&n1_s53), reinterpret_cast<void*>(&n1_s54), reinterpret_cast<void*>(&n1_s55),
    reinterpret_cast<void*>(&n1_s56),  reinterpret_cast<void*>(&n1_s57), reinterpret_cast<void*>(&n1_s58), reinterpret_cast<void*>(&n1_s59),
    reinterpret_cast<void*>(&n1_s60),  reinterpret_cast<void*>(&n1_s61), reinterpret_cast<void*>(&n1_s62), reinterpret_cast<void*>(&n1_s63),
    reinterpret_cast<void*>(&n1_s64),  reinterpret_cast<void*>(&n1_s65), reinterpret_cast<void*>(&n1_s66), reinterpret_cast<void*>(&n1_s67),
    reinterpret_cast<void*>(&n1_s68),  reinterpret_cast<void*>(&n1_s69), reinterpret_cast<void*>(&n1_s70), reinterpret_cast<void*>(&n1_s71),
    reinterpret_cast<void*>(&n1_s72),  reinterpret_cast<void*>(&n1_s73), reinterpret_cast<void*>(&n1_s74), reinterpret_cast<void*>(&n1_s75),
    reinterpret_cast<void*>(&n1_s76),  reinterpret_cast<void*>(&n1_s77), reinterpret_cast<void*>(&n1_s78), reinterpret_cast<void*>(&n1_s79),
    reinterpret_cast<void*>(&n1_s80),  reinterpret_cast<void*>(&n1_s81), reinterpret_cast<void*>(&n1_s82), reinterpret_cast<void*>(&n1_s83),
    reinterpret_cast<void*>(&n1_s84),  reinterpret_cast<void*>(&n1_s85), reinterpret_cast<void*>(&n1_s86), reinterpret_cast<void*>(&n1_s87),
    reinterpret_cast<void*>(&n1_s88),  reinterpret_cast<void*>(&n1_s89), reinterpret_cast<void*>(&n1_s90), reinterpret_cast<void*>(&n1_s91),
    reinterpret_cast<void*>(&n1_s92),  reinterpret_cast<void*>(&n1_s93), reinterpret_cast<void*>(&n1_s94), reinterpret_cast<void*>(&n1_s95),
    reinterpret_cast<void*>(&n1_s96),  reinterpret_cast<void*>(&n1_s97), reinterpret_cast<void*>(&n1_s98), reinterpret_cast<void*>(&n1_s99),
    reinterpret_cast<void*>(&n1_s100), reinterpret_cast<void*>(&n1_s101),reinterpret_cast<void*>(&n1_s102),reinterpret_cast<void*>(&n1_s103),
    reinterpret_cast<void*>(&n1_s104), reinterpret_cast<void*>(&n1_s105),reinterpret_cast<void*>(&n1_s106),reinterpret_cast<void*>(&n1_s107),
    reinterpret_cast<void*>(&n1_s108), reinterpret_cast<void*>(&n1_s109),reinterpret_cast<void*>(&n1_s110),reinterpret_cast<void*>(&n1_s111),
    reinterpret_cast<void*>(&n1_s112), reinterpret_cast<void*>(&n1_s113),reinterpret_cast<void*>(&n1_s114),reinterpret_cast<void*>(&n1_s115),
    reinterpret_cast<void*>(&n1_s116), reinterpret_cast<void*>(&n1_s117),reinterpret_cast<void*>(&n1_s118),reinterpret_cast<void*>(&n1_s119),
    reinterpret_cast<void*>(&n1_s120), reinterpret_cast<void*>(&n1_s121),reinterpret_cast<void*>(&n1_s122),reinterpret_cast<void*>(&n1_s123),
    reinterpret_cast<void*>(&n1_s124), reinterpret_cast<void*>(&n1_s125),reinterpret_cast<void*>(&n1_s126),reinterpret_cast<void*>(&n1_s127),
};

// sub_10179350 - init helper.
void initType1Defaults(void* self)
{
    F (self, 0xE8) = -1.0f;
    U (self, 0x0C) = 0u;
    U (self, 0x214) = 0u;
    n1_s120(self RD9_THUNK_ARG, 0u);   // +0x90 = 0
    n1_s116(self RD9_THUNK_ARG, 1u);   // +0x94 = 1
    Bt(self, 0x1F2)=0; U(self,0x20)=0u; U(self,0x24)=0u; U(self,0x28)=0u; Bt(self,0x2C)=0;
    U (self, 0x34)=0u; U(self,0x38)=0u; U(self,0x3C)=0u; Bt(self,0x40)=0;
    U (self, 0x48)=0u; U(self,0x4C)=0u; U(self,0x50)=0u; Bt(self,0x54)=0;
    F (self, 0x5C) = 360.0f;
    U (self, 0x60) = 0u;
    F (self, 0x10) = 32.0f;
    F (self, 0x19C) = 0.03125f;
    F (self, 0x1A0) = 1.0f;
    U (self, 0x64) = 2u;
    U (self, 0x68) = 2u;
    U (self, 0x14)=0u; U(self,0x18)=0u; U(self,0x1C)=0u; U(self,0x190)=0u; U(self,0x194)=0u; Bt(self,0x198)=0; U(self,0x1A4)=0u;
    F (self, 0xD8) = 0.025f;
    F (self, 0xDC) = 0.01f;
    F (self, 0xE0) = 66.6667f;
    F (self, 0xE4) = 0.05f;
    U (self, 0xCC)=0u; U(self,0x1EC)=0u; Bt(self,0x1F0)=0;
    F (self, 0xA4) = -9.82f;
    U (self, 0xA8)=0u; U(self,0xB8)=0u; U(self,0xBC)=0u; U(self,0xC0)=0u; U(self,0xAC)=0u; U(self,0xB0)=0u; U(self,0xB4)=0u;
    F (self, 0xC4) = 0.2f;
    F (self, 0xC8) = 1.0f;
    U (self, 0x16C)=0u; U(self,0x170)=0u; U(self,0x174)=0u; U(self,0x178)=0u; U(self,0x17C)=0u; U(self,0x180)=0u; U(self,0x184)=0u; U(self,0x188)=0u; U(self,0x18C)=0u;

    n1_s99 (self RD9_THUNK_ARG, psmFloatArgFromBits(static_cast<unsigned>(-1082130432)));   // +0x1A8 = -1.0
    { float v[3] = { 1.0f, 1.0f, 1.0f }; n1_s101(self RD9_THUNK_ARG, v); }
    { float v[3] = { 1.0f, 1.0f, 1.0f }; n1_s103(self RD9_THUNK_ARG, v); }
    { float v[3] = { 1.0f, 1.0f, 1.0f }; n1_s105(self RD9_THUNK_ARG, v); }

    Bt(self, 0xEC)=0; F(self,0xF0)=0.7f; F(self,0xF4)=0.8f; U(self,0xF8)=2u;
    n1_s89 (self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(1.0f)));   // +0x1E8 = 1.0
    n1_s108(self RD9_THUNK_ARG, psmFloatArgFromBits(0u));            // nullsub
    n1_s45 (self RD9_THUNK_ARG, psmFloatArgFromBits(fbits(4.0f)));   // +0x9C = 4.0
    n1_s43 (self RD9_THUNK_ARG, psmFloatArgFromBits(0u));            // +0x98 = 0
    { float v[4] = { 0.0f, 0.0f, 1.0f, 0.0f }; n1_s106(self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n1_s59 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n1_s95 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n1_s39 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n1_s53 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n1_s55 (self RD9_THUNK_ARG, v); }
    { float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; n1_s57 (self RD9_THUNK_ARG, v); }

    Bt(self, 0xD0)=0; F(self,0xD4)=0.03f;
    U (self, 0x1DC)=0u; U(self,0x1E4)=0u; F(self,0x1E0)=1.0f;   // default dir {0,1,0}
    F (self, 0x220)=20.0f; F(self,0x224)=20.0f; Bt(self,0x1F1)=0; Bt(self,0x218)=0; U(self,0x21C)=0u;
    F (self, 0x228)=3.0f; F(self,0x22C)=10.0f; F(self,0x230)=3.0f; F(self,0x234)=2.0f;
}

} // anonymous namespace

// G1-b: 0x10179080, 75 B - the resolve without the assignment. Called by the type-1 instance
// constructor (0x10184810; the call is at 0x10184BF8) when +0x8C is still null.
void particleSystemType1ResolveGeometry(void* descriptor)
{
    if (IBase* const old = geometryInstance(descriptor))
    {
        old->release();                                               // [eax+8]
        geometryInstance(descriptor) = 0;
    }
    resolveGeometry(descriptor);
}

// G1: the geometry instance the MeshParticlesRenderer draws with (descriptor +0x8C). The renderer
// reads the field raw in retail (`mov eax,[esi+8Ch]` at 0x10174743); published typed for that agent.
void* particleSystemType1Geometry(void* descriptor)
{
    return geometryInstance(descriptor);
}

// geom_ParticleSystem_type1_ctor, 0x10179B70.
void* constructParticleSystemType1(void* mem, const void* nameString)
{
    ::memset(mem, 0, PSM_DESCRIPTOR_SIZE(0x238u));
    *static_cast<const void* const**>(mem) = kVtable;

    U(mem, 0x24) = 1065353216u;   // 1.0
    U(mem, 0x30) = 1973272912u;   // range sentinel
    U(mem, 0x38) = 1065353216u;
    U(mem, 0x44) = 1973272912u;
    U(mem, 0x4C) = 1065353216u;
    U(mem, 0x58) = 1973272912u;

    // G1-e: both strings are host strings. The manager supplies the renderer's own string, exactly
    // as for the type-0/2/4 descriptors.
    new (&psmDescriptor1<unsigned char>(mem, 0x70)) ParticleHostString();                                   // geomName
    new (&psmDescriptor1<unsigned char>(mem, 0x1F4)) ParticleHostString(
        ::renddx9abi::toHostString(*static_cast<const std::string*>(nameString)));                // name
    // +0x8C (geometry) and +0x210 (texture) are the retail constructor's explicit zero stores
    // (0x10179BC8 / 0x10179D2A); the memset above already holds them.

    initType1Defaults(mem);
    return mem;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
