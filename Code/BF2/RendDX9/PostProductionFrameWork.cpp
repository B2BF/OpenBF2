// Post-processing settings and device/event-listener owner. [INFERRED] source placement.
// The published singleton is the owner's third interface (owner+8 on x86); preserve that adjustment.
// [CHOSEN] named storage wraps the fixed owner layout. Resource acquisition and full-screen
// rendering remain subject to the temporary boundaries beside their implementations.
//
// BF2Render.cpp declares and uses these (defined at the foot of this file):
//   unsigned int postProductionFrameWorkOwnerSize();      // the owner allocation size
//   void* postProductionFrameWorkOwnerCtor(void* mem);
//   void* postProductionFrameWorkInterface(void* owner);   // the published subobject
//   void  postProductionFrameWorkOwnerDetach(void* owner);
//   bool  postProductionFrameWorkIsActive();               // sub_1004F1C0 on the published owner

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>
#include "RendDX9Base.h"
#include "Texture.h"          // textureReleaseRef for the listener's lost slot

#include <new>
#include <stddef.h>
#include <string.h>

namespace dice {
namespace hfe {
namespace dx9rend {

// Defined in BF2Render.cpp.
extern IBase* g_PostProductionFrameWorkOwner;

namespace {

// [CHOSEN, not recovered] The owner as a struct (issue #7). On x86 every member sits at its retail
// offset (asserted below, 548 bytes in all); on x64 the interface pointers, resource pointers and
// the std::string take their native sizes. The scalar fields live in one pointer-free window that
// is addressed by retail owner offset (U/F/Bt below); on x86 that is the retail address itself.
const unsigned int kScalarsBegin          = 0x00C;
const unsigned int kScalarsEnd            = 0x150;
const unsigned int kRetailFrameWorkOffset = 0x008;      // the published interface, retail owner+8
#if defined(_M_IX86)
const unsigned int kNameStorageBytes      = 28;         // the retail VC7.1 std::string width
#else
const unsigned int kNameStorageBytes      = sizeof(std::string);
#endif

// ECGameLogic 13's payload, BfSound::TinnitusSetup (Code/BF2/Sound/BfSound.h). Pointer-free, so the
// same five dwords on x86 and x64.
struct TinnitusRecord
{
    int   state;      // +0x00 1 while the effect is started
    float attack;     // +0x04
    float hold;       // +0x08
    float release;    // +0x0C
    float volume;     // +0x10
};

struct PostProductionOwner
{
    const void* const* listenerVtable;      // +0x000 EventListener + IBase spine (kOwnerVtable)
    const void* const* deviceLostVtable;    // +0x004 DeviceLostResetListener (kDeviceLostVtable)
    const void* const* frameWorkVtable;     // +0x008 the published interface (kPostProdVtable)
    unsigned char      scalars[kScalarsEnd - kScalarsBegin];   // +0x00C..+0x14F by retail offset:
                                            //        refcount, timer, flags, the fog record
                                            //        (+0x28..+0x78), frame-work floats, matrices
    unsigned char      retailDeque[0x14];   // +0x150 [UNRESOLVED] element type: a VC7.1 std::deque the
                                            //        shipped destructor frees (sub_100514B0); not
                                            //        modeled here, stays zero
    TinnitusRecord     tinnitus;            // +0x164 the last ECGameLogic 13 payload
    void*              effect;              // +0x178 [FRONTIER] PostProduction effect, unacquired
    void*              effectParameters[29];// +0x17C [FRONTIER] TEXLAYER0..DISPLACEMENT, unacquired
    unsigned int       field1F0;            // +0x1F0 [UNRESOLVED] identity; zeroed by both default arms
    EngineTexture*     helperTextures[3];   // +0x1F4 [FRONTIER] common/textures/noise, random, sine
    EngineTexture*     noiseTexture;        // +0x200 the setNoiseTextureName texture (listener +0x1FC)
    unsigned char      noiseTextureNameStorage[kNameStorageBytes];  // +0x204 std::string (listener +0x200)
    unsigned int       field220;            // +0x220 [UNRESOLVED] identity; read by the active predicate
};

static_assert(sizeof(TinnitusRecord) == 20, "BfSound::TinnitusSetup is five dwords");
static_assert(sizeof(std::string) <= kNameStorageBytes, "the noise-texture name does not fit");
static_assert(offsetof(PostProductionOwner, noiseTextureNameStorage) % alignof(std::string) == 0,
              "the noise-texture name is misaligned");
#if defined(_M_IX86)
static_assert(offsetof(PostProductionOwner, listenerVtable)          == 0x000, "retail owner layout");
static_assert(offsetof(PostProductionOwner, deviceLostVtable)        == 0x004, "retail owner layout");
static_assert(offsetof(PostProductionOwner, frameWorkVtable)         == kRetailFrameWorkOffset,
              "retail owner layout");
static_assert(offsetof(PostProductionOwner, scalars)                 == kScalarsBegin, "retail owner layout");
static_assert(offsetof(PostProductionOwner, retailDeque)             == kScalarsEnd, "retail owner layout");
static_assert(offsetof(PostProductionOwner, tinnitus)                == 0x164, "retail owner layout");
static_assert(offsetof(PostProductionOwner, effect)                  == 0x178, "retail owner layout");
static_assert(offsetof(PostProductionOwner, effectParameters)        == 0x17C, "retail owner layout");
static_assert(offsetof(PostProductionOwner, field1F0)                == 0x1F0, "retail owner layout");
static_assert(offsetof(PostProductionOwner, helperTextures)          == 0x1F4, "retail owner layout");
static_assert(offsetof(PostProductionOwner, noiseTexture)            == 0x200, "retail owner layout");
static_assert(offsetof(PostProductionOwner, noiseTextureNameStorage) == 0x204, "retail owner layout");
static_assert(offsetof(PostProductionOwner, field220)                == 0x220, "retail owner layout");
static_assert(sizeof(PostProductionOwner) == 548, "DX9RendModule::init allocates 548 bytes");
#endif

// Scalar fields by retail owner offset; the window check is at compile time.
template<class T, unsigned int Off> T& scalarAt(PostProductionOwner* o)
{
    static_assert(Off >= kScalarsBegin && Off + sizeof(T) <= kScalarsEnd, "outside the scalar window");
    return *reinterpret_cast<T*>(o->scalars + (Off - kScalarsBegin));
}
template<unsigned int Off, unsigned int Len> unsigned char* scalarBytes(PostProductionOwner* o)
{
    static_assert(Off >= kScalarsBegin && Off + Len <= kScalarsEnd, "outside the scalar window");
    return o->scalars + (Off - kScalarsBegin);
}
template<unsigned int Off> unsigned int&  U (PostProductionOwner* o) { return scalarAt<unsigned int, Off>(o); }
template<unsigned int Off> float&         F (PostProductionOwner* o) { return scalarAt<float, Off>(o); }
template<unsigned int Off> unsigned char& Bt(PostProductionOwner* o) { return scalarAt<unsigned char, Off>(o); }

// Each thunk receives its own subobject; these undo that adjustment (retail 0 / 4 / 8 on x86).
inline PostProductionOwner* ownerAt(void* subobject, size_t offset)
{
    return reinterpret_cast<PostProductionOwner*>(static_cast<char*>(subobject) - offset);
}
inline PostProductionOwner* ownerOfEventListener(void* self)
{
    return ownerAt(self, offsetof(PostProductionOwner, listenerVtable));
}
inline PostProductionOwner* ownerOfDeviceLost(void* self)
{
    return ownerAt(self, offsetof(PostProductionOwner, deviceLostVtable));
}
inline PostProductionOwner* ownerOfFrameWork(void* self)
{
    return ownerAt(self, offsetof(PostProductionOwner, frameWorkVtable));
}
inline void* frameWorkOf(PostProductionOwner* o) { return &o->frameWorkVtable; }
inline PostProductionOwner* publishedOwner()
{
    return static_cast<PostProductionOwner*>(static_cast<void*>(g_PostProductionFrameWorkOwner));
}
inline std::string& noiseTextureName(PostProductionOwner* o)
{
    return *reinterpret_cast<std::string*>(o->noiseTextureNameStorage);
}

// The frame-work bodies' fields by the shipped `this`-relative offset (retail this = owner+8).
template<unsigned int Off> unsigned int&  PU (void* self) { return U <kRetailFrameWorkOffset + Off>(ownerOfFrameWork(self)); }
template<unsigned int Off> float&         PF (void* self) { return F <kRetailFrameWorkOffset + Off>(ownerOfFrameWork(self)); }
template<unsigned int Off> unsigned char& PBt(void* self) { return Bt<kRetailFrameWorkOffset + Off>(ownerOfFrameWork(self)); }

// A float argument of a published setter. On x86 the caller's stack dword is taken as unsigned and
// stored as a bit pattern, as the shipped `mov` stores do. [CHOSEN, not recovered] x64 passes a
// float in an XMM register, so the parameter is float there and its bits are stored unchanged
// (issue #7).
#if defined(_M_IX86)
typedef unsigned int FloatArg;
inline unsigned int floatArgBits(FloatArg v) { return v; }
#else
typedef float FloatArg;
inline unsigned int floatArgBits(FloatArg v)
{
    unsigned int bits;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}
#endif

inline void copyWord(void* dst, const void* src) { memcpy(dst, src, 4); }


// The published singleton's vtable (owner+8). Every slot `this` is owner+8.


// +0x00 deleting destructor (0x10051700 -> sub_100517C0: shutdown then free-if-flag).
// The owner block is a single allocation freed by DX9RendModule::close(); the shipped
// teardown (sub_10050830 shutdown + heap free of the MI-adjusted pointer) is part of the deferred
// render lifetime and is not reached during play (nothing releases the singleton mid-session).
// A logged, non-freeing body: it must not free owner+8 (the allocation base is owner+0).
// [RETIRE-WHEN: the post-production render path is recovered]
void* RD9_THUNK pp_dtor(void* self RD9_THUNK_EDX, int /*flags*/)
{
    static bool noted = false;
    if (!noted) { noted = true;
        ::renddx9abi::logf("PostProductionFrameWork::~ (slot 0): teardown deferred with the render "
                           "path; the owner block is freed by DX9RendModule::close"); }
    return self;
}

// +0x04 / +0x08 / +0x0C / +0x10 / +0x14 / +0x18 / +0x1C / +0x20 - eight paired float accessors.
// (this+40..43 <=> byte 160/164/168/172.)
void  RD9_THUNK pp_set160(void* self RD9_THUNK_EDX, FloatArg v) { PU<160>(self) = floatArgBits(v); }  // +0x04
float RD9_THUNK pp_get160(void* self RD9_THUNK_EDX)             { return PF<160>(self); }            // +0x08
void  RD9_THUNK pp_set164(void* self RD9_THUNK_EDX, FloatArg v) { PU<164>(self) = floatArgBits(v); }  // +0x0C
float RD9_THUNK pp_get164(void* self RD9_THUNK_EDX)             { return PF<164>(self); }            // +0x10
void  RD9_THUNK pp_set168(void* self RD9_THUNK_EDX, FloatArg v) { PU<168>(self) = floatArgBits(v); }  // +0x14
float RD9_THUNK pp_get168(void* self RD9_THUNK_EDX)             { return PF<168>(self); }            // +0x18
void  RD9_THUNK pp_set172(void* self RD9_THUNK_EDX, FloatArg v) { PU<172>(self) = floatArgBits(v); }  // +0x1C
float RD9_THUNK pp_get172(void* self RD9_THUNK_EDX)             { return PF<172>(self); }            // +0x20

// +0x24 setStateVec4 (0x1004F7A0): store a vec4 at this+11..14 (byte 44..56); mark a render-state
// dirty flag. That flag lives on dword_1024B930, a render-context global this front does not model;
// the shipped guard is `if (dword_1024B930) *(dword_1024B930+1052)=1`, so with it absent the poke is
// simply skipped - the field store, the observable half, is faithful.
void RD9_THUNK pp_setStateVec4(void* self RD9_THUNK_EDX, const float* state)
{
    PU<44>(self) = *reinterpret_cast<const unsigned*>(&state[0]);
    PF<48>(self) = state[1];
    PF<52>(self) = state[2];
    PF<56>(self) = state[3];
}

// +0x28 getStateVec4 (0x1004F7E0).
void* RD9_THUNK pp_getStateVec4(void* self RD9_THUNK_EDX, float* out)
{
    out[0] = PF<44>(self);
    out[1] = PF<48>(self);
    out[2] = PF<52>(self);
    out[3] = PF<56>(self);
    return out;
}

// Stores the near/far range.
// [FRONTIER] AmbientIllumination range/color holder updates remain unimplemented.
void RD9_THUNK pp_setRange(void* self RD9_THUNK_EDX, const float* a2)
{
    PU<144>(self) = *reinterpret_cast<const unsigned*>(&a2[0]);
    PF<148>(self) = a2[1];
    PF<152>(self) = a2[2];
    PF<156>(self) = a2[3];
}

// +0x30 get the range vec4 (0x1004F8C0).
void* RD9_THUNK pp_getRange(void* self RD9_THUNK_EDX, float* a2)
{
    a2[0] = PF<144>(self);
    a2[1] = PF<148>(self);
    a2[2] = PF<152>(self);
    a2[3] = PF<156>(self);
    return a2;
}

// +0x34 setGasMaskBlend (0x1004F330) - THE per-frame caller. Store the blend at this+216; if the
// previous blend was exactly 0, latch the "just became active" byte at this+220.
void RD9_THUNK pp_setBlend(void* self RD9_THUNK_EDX, FloatArg a2)
{
    if (PF<216>(self) == 0.0f)
        PBt<220>(self) = 1;
    PU<216>(self) = floatArgBits(a2);
}

// +0x38 getGasMaskBlend (return this[54] == byte 216).
float RD9_THUNK pp_getBlend(void* self RD9_THUNK_EDX) { return PF<216>(self); }

// +0x3C / +0x40 two 0x24-byte (3x3 matrix) copies into this+256 / this+292 (0x10051680 / 0x100516A0).
void RD9_THUNK pp_setMat0(void* self RD9_THUNK_EDX, const void* a2)
{
    memcpy(scalarBytes<kRetailFrameWorkOffset + 256, 0x24u>(ownerOfFrameWork(self)), a2, 0x24u);
}
void RD9_THUNK pp_setMat1(void* self RD9_THUNK_EDX, const void* a2)
{
    memcpy(scalarBytes<kRetailFrameWorkOffset + 292, 0x24u>(ownerOfFrameWork(self)), a2, 0x24u);
}

// +0x44 / +0x48 two dword setters (this+62 / this+63 == byte 248/252).
void RD9_THUNK pp_set248(void* self RD9_THUNK_EDX, FloatArg v) { PU<248>(self) = floatArgBits(v); }
void RD9_THUNK pp_set252(void* self RD9_THUNK_EDX, FloatArg v) { PU<252>(self) = floatArgBits(v); }

// +0x4C / +0x50 two byte flag setters (this+242 / this+241) (0x100516E0 / 0x100516F0).
void RD9_THUNK pp_setByte242(void* self RD9_THUNK_EDX, unsigned char v) { PBt<242>(self) = v; }
void RD9_THUNK pp_setByte241(void* self RD9_THUNK_EDX, unsigned char v) { PBt<241>(self) = v; }

// +0x54 / +0x58 two getters that read the live Renderer (renderer+1852/1856).
float RD9_THUNK pp_getRendererFloat1852(void* RD9_THUNK_EDX) { return rendererTailFloat(0x73C); }
float RD9_THUNK pp_getRendererFloat1856(void* RD9_THUNK_EDX) { return rendererTailFloat(0x740); }

// +0x5C setNoiseTextureName (0x1004FC30). The shipped body compares an incoming name against the
// stored one at this+127, releases the old texture at this+126, stores the new name, then acquires a
// new texture via the texture manager (sub_10019260). The acquisition is exactly the deferred half of
// the constructor; with no texture ever loaded (this+126 stays null) there is nothing to release and
// nothing to reacquire, so this is a visible no-op until the render path lands. Logged once.
// [RETIRE-WHEN: the post-production render path is recovered]
void RD9_THUNK pp_setNoiseTextureName(void* self RD9_THUNK_EDX, const void* /*name*/)
{
    (void)self;
    static bool noted = false;
    if (!noted) { noted = true;
        ::renddx9abi::logf("PostProductionFrameWork::setNoiseTextureName (slot 23): screen-distortion "
                           "texture deferred with the render path; name not stored"); }
}

const void* const kPostProdVtable[24] =
{
    reinterpret_cast<void*>(&pp_dtor),                    // +0x00
    reinterpret_cast<void*>(&pp_set160),                  // +0x04
    reinterpret_cast<void*>(&pp_get160),                  // +0x08
    reinterpret_cast<void*>(&pp_set164),                  // +0x0C
    reinterpret_cast<void*>(&pp_get164),                  // +0x10
    reinterpret_cast<void*>(&pp_set168),                  // +0x14
    reinterpret_cast<void*>(&pp_get168),                  // +0x18
    reinterpret_cast<void*>(&pp_set172),                  // +0x1C
    reinterpret_cast<void*>(&pp_get172),                  // +0x20
    reinterpret_cast<void*>(&pp_setStateVec4),            // +0x24
    reinterpret_cast<void*>(&pp_getStateVec4),            // +0x28
    reinterpret_cast<void*>(&pp_setRange),                // +0x2C
    reinterpret_cast<void*>(&pp_getRange),                // +0x30
    reinterpret_cast<void*>(&pp_setBlend),                // +0x34  gas-mask blend  <- per-frame caller
    reinterpret_cast<void*>(&pp_getBlend),                // +0x38
    reinterpret_cast<void*>(&pp_setMat0),                 // +0x3C
    reinterpret_cast<void*>(&pp_setMat1),                 // +0x40
    reinterpret_cast<void*>(&pp_set248),                  // +0x44
    reinterpret_cast<void*>(&pp_set252),                  // +0x48
    reinterpret_cast<void*>(&pp_setByte242),              // +0x4C
    reinterpret_cast<void*>(&pp_setByte241),              // +0x50
    reinterpret_cast<void*>(&pp_getRendererFloat1852),    // +0x54
    reinterpret_cast<void*>(&pp_getRendererFloat1856),    // +0x58
    reinterpret_cast<void*>(&pp_setNoiseTextureName),     // +0x5C
};


// Releases and clears the owned distortion-noise texture on device loss.
void RD9_THUNK dl_onLost(void* self RD9_THUNK_EDX)
{
    textureReleaseRef(ownerOfDeviceLost(self)->noiseTexture);        // listener +0x1FC
}

// slot 1 onDeviceReset, only while the name is non-empty (its _Mysize at
// listener+0x214 = owner+0x218; `jbe` = unsigned, so "non-zero"): force the TextureManager's managed
// flag to 0 (sub_10017E20), reload the texture BY name (sub_10019260, the string copied by value)
// into owner+0x200, and restore the flag. The old pointer is not released first - the lost slot
// already nulled it.
void RD9_THUNK dl_onReset(void* self RD9_THUNK_EDX)
{
    PostProductionOwner* const o = ownerOfDeviceLost(self);
    const std::string& name = noiseTextureName(o);                    // listener +0x200
    if (name.size() == 0)
        return;
    const int oldManaged = textureManagerSwapManaged(0);
    o->noiseTexture = static_cast<EngineTexture*>(textureManagerGetTexture(name));
    textureManagerSwapManaged(oldManaged);
}

// slot 2 (0x1004E960): nullsub in the shipped image.
void RD9_THUNK dl_slot2(void* RD9_THUNK_EDX) { }

const void* const kDeviceLostVtable[3] =
{
    reinterpret_cast<void*>(&dl_onLost),
    reinterpret_cast<void*>(&dl_onReset),
    reinterpret_cast<void*>(&dl_slot2),
};


// EventListener / IBase spine vtable (owner+0). Registration is deferred, so handleEvent is not
// reached; the reference spine at +4/+8/+C/+10 is the recovered IBase implementation. Refcount is the
// dword at this+0x0C.


// slot 0 handleEvent (0x1004F0F0): react to (class 0,id 13) device-reset and (class 5,id 1)
// GSViewDistance change. Recovered verbatim; unreached until the EventManager registration is undone
// from deferral. The IEventListener::handleEvent arguments: category, id, node, userData.
int RD9_THUNK el_handleEvent(void* self RD9_THUNK_EDX, int category, unsigned int id, void* node,
                             void* userData)
{
    PostProductionOwner* const o = ownerOfEventListener(self);
    int result = category;
    if (category)
    {
        if (category == 5 && id == 1)
        {
            const std::string* s = static_cast<const std::string*>(userData);
            return strcmp(s->c_str(), "GSViewDistance") == 0 ? 1 : 0;
        }
    }
    else if (id == 13)
    {
        // `mov eax, [edx+2Ch]`: the payload pointer at node+0x2C. [UNRESOLVED] The node type that
        // carries it is not identified (the tree's NotificationEventNode keeps its data at a
        // different offset); x64 reads a native pointer at the same retail byte offset, which is an
        // unverified assumption. Unreached while the EventManager registration stays deferred.
        const TinnitusRecord* const payload =
            *reinterpret_cast<const TinnitusRecord* const*>(static_cast<const char*>(node) + 0x2C);
        if (payload)
        {
            TinnitusRecord& t = o->tinnitus;                  // +0x164..+0x174, dword copies
            copyWord(&t.state,   &payload->state);
            copyWord(&t.attack,  &payload->attack);
            copyWord(&t.hold,    &payload->hold);
            copyWord(&t.release, &payload->release);
            copyWord(&result,    &payload->volume);
            copyWord(&t.volume,  &result);
            if (t.state == 1)
            {
                int v6 = static_cast<int>((t.release + t.hold + t.attack) * 30.0f);
                if (v6 < 20) v6 = 20;
                U<28>(o) = static_cast<unsigned>(v6) + U<16>(o);
                F<32>(o) = static_cast<float>(v6);
                result = v6;
            }
        }
    }
    return result;
}

int  RD9_THUNK el_addRef (void* self RD9_THUNK_EDX) { return ++U<12>(ownerOfEventListener(self)); }
int  RD9_THUNK el_getRc  (void* self RD9_THUNK_EDX) { return   U<12>(ownerOfEventListener(self)); }

// decrement; at zero invoke the PostProductionFrameWork subobject's deleting dtor
// (owner+8 vtable slot 0), i.e. `(*(*(this+8)))(this+8, 1)`.
int RD9_THUNK el_release(void* self RD9_THUNK_EDX)
{
    PostProductionOwner* const o = ownerOfEventListener(self);
    int v = --U<12>(o);
    if (!v)
    {
        void* sub = frameWorkOf(o);
        void** vt = *reinterpret_cast<void***>(sub);
        typedef void* (__thiscall* DtorFn)(void*, int);
        reinterpret_cast<DtorFn>(vt[0])(sub, 1);
    }
    return v;
}

// queryInterface - accepts iid 1 (IBase) only.
void* RD9_THUNK el_qi(void* self RD9_THUNK_EDX, unsigned iid) { return iid != 1u ? 0 : self; }

const void* const kOwnerVtable[5] =
{
    reinterpret_cast<void*>(&el_handleEvent),
    reinterpret_cast<void*>(&el_addRef),
    reinterpret_cast<void*>(&el_getRc),
    reinterpret_cast<void*>(&el_release),
    reinterpret_cast<void*>(&el_qi),
};

// sub_1004E860 - the second field-default arm the constructor calls after the acquisition block. Pure
// stores; reproduced with the same values (non-zero only; the block is memset to 0 first).
void applySecondaryDefaults(PostProductionOwner* owner)
{
    U <176>(owner) = 1056964608u;
    U <180>(owner) = 1056964608u;
    U <192>(owner) = 1056964608u;
    Bt< 38>(owner) = 1;
    U <172>(owner) = 1036831949u;
    U <184>(owner) = 1080033280u;
    U <216>(owner) = 1055286886u;
    U <232>(owner) = 1060320051u;
    U <236>(owner) = 1075838976u;
    U <240>(owner) = 1077936128u;
    U <244>(owner) = 1075838976u;
    // owner+224 (the gas-mask blend), owner+188/208/252/256/260, and the +212/229/249/250 bytes are 0
    // here - already zero from the initial memset.
}

} // anonymous namespace


// Initializes the preallocated owner (postProductionFrameWorkOwnerSize bytes) and returns it.
// [FRONTIER] Effect acquisition, parameter handles, helper textures, listener registration,
// and fullscreen rendering remain deferred; resource fields stay zero until implemented.
// [RETIRE-WHEN: the post-production render path is recovered]

void* postProductionFrameWorkOwnerCtor(void* mem)
{
    // The shipped ctor sets every field explicitly; we memset first and then write only the non-zero
    // finals, which is the identical resulting state.
    memset(mem, 0, sizeof(PostProductionOwner));
    PostProductionOwner* const o = static_cast<PostProductionOwner*>(mem);

    // The three interface vtables (owner+0 / +4 / +8).
    o->listenerVtable   = kOwnerVtable;
    o->deviceLostVtable = kDeviceLostVtable;
    o->frameWorkVtable  = kPostProdVtable;

    U<12>(o) = 1u;                        // this+3  refcount

    // this+10..16 (byte 40..64): the ctor sets these twice; the final values are written here.
    U<40>(o) = 1065353216u;               // 1.0f
    U<44>(o) = 1065353216u;
    U<48>(o) = 1065353216u;
    U<52>(o) = 1133903872u;
    U<56>(o) = 1140457472u;
    U<60>(o) = 1065353216u;
    // owner+64 (this+16): set 0.5f then 0 -> final 0 (memset).

    // this+17..22 (byte 68..88): the "final" second-pass values.
    U<68>(o) = 1050589266u;
    U<72>(o) = 1047233823u;
    U<76>(o) = 1034952901u;
    U<80>(o) = 1050589266u;
    U<84>(o) = 1047233823u;
    U<88>(o) = 1034952901u;

    // this+23..30 (byte 92..120): v2 block + trailing scalars (finals).
    U< 96>(o) = 1077936128u;              // v2[1]
    U<100>(o) = 1065353216u;              // v2[2]
    U<112>(o) = 1106247680u;              // this+28
    U<116>(o) = 1065353216u;              // this+29

    // this+31..51 (byte 124..204).
    U<124>(o) = 1045352145u;
    U<128>(o) = 1045352145u;
    U<132>(o) = 1056997505u;
    U<136>(o) = 1092616192u;
    U<140>(o) = 1132068864u;
    U<144>(o) = 1065353216u;
    U<152>(o) = 1092616192u;
    U<156>(o) = 1148846080u;
    U<168>(o) = 1028443341u;              // this+42
    U<172>(o) = 1036831949u;              // this+43
    U<176>(o) = 1056964608u;              // this+44
    U<180>(o) = 1056964608u;              // this+45
    U<192>(o) = 1056964608u;              // this+48

    // Byte flags set by the ctor (this+220 and this+228 both == 1).
    Bt<220>(o) = 1;
    Bt<228>(o) = 1;

    // The embedded std::string at this+129 (byte 516; a 28-byte VC7.1 string in retail) - the
    // noise-texture name, empty. postProductionFrameWorkOwnerDetach destroys it.
    new (o->noiseTextureNameStorage) std::string();

    // this+66..83 (byte 264..332): the two 3x3 matrices, set once near the end of the ctor.
    U<264>(o) = 1028443341u; U<268>(o) = 1045220557u; U<272>(o) = 1056964608u;
    U<276>(o) = 1063675494u; U<280>(o) = 1065353216u; U<284>(o) = 1063675494u;
    U<288>(o) = 1056964608u; U<292>(o) = 1045220557u; U<296>(o) = 1028443341u;
    U<300>(o) = 1028443341u; U<304>(o) = 1045220557u; U<308>(o) = 1056964608u;
    U<312>(o) = 1063675494u; U<316>(o) = 1065353216u; U<320>(o) = 1063675494u;
    U<324>(o) = 1056964608u; U<328>(o) = 1045220557u; U<332>(o) = 1028443341u;

    // The constructor's second default arm.
    applySecondaryDefaults(o);

    // the constructor's last act, 0x100507A9 `push 3` / 0x100507AB
    // `lea eax, [esi+4]` ... 0x1005080E `mov ecx, dword_1023B970; push eax; call sub_10009D60`:
    // RendDX9_registerDeviceLostResetListener(owner + 4, 3). DX9RendModule::init constructs this
    // after the Renderer (pass two of initDll), so the list at dword_1023B970+0x5C exists.
    registerDeviceLostResetListener(static_cast<void*>(&o->deviceLostVtable), 3);

    ::renddx9abi::logf("PostProductionFrameWork constructed at owner 0x%08X (singleton owner+8) - "
                       "vtables live, state defaulted, device-lost listener registered (priority "
                       "3); effect/params/textures + the EventManager handler deferred with the "
                       "render path", ::renddx9abi::addr(mem));
    return mem;
}

// The size DX9RendModule::init allocates for the owner: the retail 548 bytes on x86.
unsigned int postProductionFrameWorkOwnerSize()
{
    return static_cast<unsigned int>(sizeof(PostProductionOwner));
}

// The published PostProductionFrameWork interface inside an owner (owner + 8 on x86).
void* postProductionFrameWorkInterface(void* owner)
{
    return frameWorkOf(static_cast<PostProductionOwner*>(owner));
}

// sub_1004F1C0 - whether post-production is active, on the published owner (false while none is
// published). The rebuilt owner leaves the resource and effect fields zero, so its at-rest answer
// is false.
bool postProductionFrameWorkIsActive()
{
    PostProductionOwner* const o = publishedOwner();
    if (!o)
        return false;
    if (o->noiseTexture || Bt<36>(o))
        return true;
    return o->tinnitus.state == 1
        || Bt<39>(o)
        || o->field220
        || Bt<250>(o)
        || F<224>(o) > 0.0f;
}


// Detaches the owner's device listener before its storage is freed.
// [FRONTIER] Remaining destructor work depends on deferred event/resource acquisition.
void postProductionFrameWorkOwnerDetach(void* owner)
{
    PostProductionOwner* const o = static_cast<PostProductionOwner*>(owner);
    o->listenerVtable   = kOwnerVtable;
    o->deviceLostVtable = kDeviceLostVtable;
    o->frameWorkVtable  = kPostProdVtable;
    unregisterDeviceLostResetListener(static_cast<void*>(&o->deviceLostVtable));
    // [CHOSEN, not recovered] End the noise-texture name's lifetime before the storage is freed
    // (issue #7); the shipped destructor's own string teardown is part of the frontier above.
    noiseTextureName(o).~basic_string();
}

// THE FOG RECORD: owner +0x28..+0x40 the fog colour (normalised RGB) and start/end/base/w, +0x44 /
// +0x50 the two water-fog colours (A / B), +0x5C / +0x6C the two water-fog ranges (A / B).
// Stores 0..255 RGB as normalized fog colour. Requires the post-production owner.
// Helpers use owner-relative offsets; the public singleton is its +8 subobject.
void postProductionSetFogColor255(const float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    const float k = 1.0f / 255.0f;
    F<0x30>(o) = rgb[2] * k;
    F<0x28>(o) = rgb[0] * k;
    F<0x2C>(o) = rgb[1] * k;
}

// the read arm: the stored colour times 255 (0x437F0000).
void postProductionGetFogColor255(float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    rgb[0] = F<0x28>(o) * 255.0f;
    rgb[1] = F<0x2C>(o) * 255.0f;
    rgb[2] = F<0x30>(o) * 255.0f;
}

// renderer.waterFogColor's write arm: the one colour, divided by 255, into both
// water-fog colour slots (B at +0x50 first, then A at +0x44).
void postProductionSetWaterFogColor255(const float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    const float k = 1.0f / 255.0f;
    F<0x58>(o) = rgb[2] * k;
    F<0x50>(o) = rgb[0] * k;
    F<0x54>(o) = rgb[1] * k;
    F<0x4C>(o) = rgb[2] * k;
    F<0x44>(o) = rgb[0] * k;
    F<0x48>(o) = rgb[1] * k;
}

// its read arm reads slot B (+0x50) times 255.
void postProductionGetWaterFogColor255(float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    rgb[0] = F<0x50>(o) * 255.0f;
    rgb[1] = F<0x54>(o) * 255.0f;
    rgb[2] = F<0x58>(o) * 255.0f;
}

// Updates shader and device fog from camera depth below the water plane.
// Interpolates water fog over ten units; NaN depth selects the low bound.
// Fog coefficient division is unchecked when start and end coincide.
void postProductionUpdateFog(float underwaterDepth)
{
    PostProductionOwner* const o = publishedOwner();

    float t = underwaterDepth * 0.1f;
    if (t >= 1.0f)
        t = 1.0f;
    if (!(t >= 0.0f))
        t = 0.0f;
    const float u = 1.0f - t;

    float c[3];
    c[0] = F<0x50>(o) * u + F<0x44>(o) * t;
    c[1] = F<0x54>(o) * u + F<0x48>(o) * t;
    c[2] = F<0x58>(o) * u + F<0x4C>(o) * t;

    float r[4];
    r[0] = F<0x6C>(o) * u + F<0x5C>(o) * t;
    r[1] = F<0x70>(o) * u + F<0x60>(o) * t;
    r[2] = F<0x74>(o) * u + F<0x64>(o) * t;
    r[3] = F<0x78>(o) * u + F<0x68>(o) * t;

    const float ws = 1.0f / (r[1] - r[0]);
    float waterRange[4];
    waterRange[0] = ws;
    waterRange[1] = -(r[2] * ws);
    waterRange[2] = -(r[0] * ws);
    waterRange[3] = r[1] * ws;
    raShaderConstantsSetWaterFogRange(waterRange);
    raShaderConstantsSetWaterFogColor(c);

    if (!cameraIsUnderWater())
    {
        float rgbw[4];
        rgbw[0] = F<0x28>(o);
        rgbw[1] = F<0x2C>(o);
        rgbw[2] = F<0x30>(o);
        rgbw[3] = F<0x40>(o);
        raShaderConstantsSetFogColor(rgbw);

        const float s = 1.0f / (F<0x38>(o) - F<0x34>(o));      // 1 / (end - start)
        float range[4];
        range[0] = s;
        range[1] = -(F<0x3C>(o) * s);
        range[2] = -(F<0x34>(o) * s);
        range[3] = F<0x38>(o) * s;
        raShaderConstantsSetFogRange(range);
    }
    else
    {
        float rgbw[4];
        rgbw[0] = c[0];
        rgbw[1] = c[1];
        rgbw[2] = c[2];
        rgbw[3] = r[3];
        raShaderConstantsSetFogColor(rgbw);
        raShaderConstantsSetFogRange(waterRange);
    }
}

void postProductionGetHemiState(float outState[4])
{
    pp_getStateVec4(frameWorkOf(publishedOwner()) RD9_THUNK_ARG, outState);
}

void postProductionSetHemiState(const float state[4])
{
    pp_setStateVec4(frameWorkOf(publishedOwner()) RD9_THUNK_ARG, state);
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
