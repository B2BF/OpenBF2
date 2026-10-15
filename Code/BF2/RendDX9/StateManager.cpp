// Path: [INFERRED]; StateManager is named by RendDX9Init.cpp:232.
// Deferred D3D state cache, recovered from RendDX9.dll (0x10025980..0x10027790).
// Setters queue changes; draw wrappers apply them. Pop schedules restoration for
// the next draw. Transparency-AA globals begin at zero and are set by the later probe.

// See Code/BF2/RendDX9/BF2Render.cpp for the BF2_IMAGE_RENDDX9 guard and why it is here.
#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"

#include <cstring>
#include <stddef.h>
#include <stdint.h>

namespace dice {
namespace hfe {
namespace dx9rend {

extern void* g_rendererBase;    // dword_1023B970 - RendDX9Init.cpp; device at +0x0C
extern void* g_shaderManager;   // dword_1023B97C - ShaderManager.cpp

// ShaderManager.cpp - the pre-draw CommitChanges walk over the three current-effect slots
// (+0xA0 raw ID3DXEffect, +0x98 Effect wrapper, +0x9C the states-effect arm).
void shaderManagerCommitCurrentEffects();

// dword_1023B984. Everything below publishes and consumes it through this name.
void* g_stateManager = 0;

// dword_1023D08C, char[4] at 0x1023D090, char[4] at all zero until the vendor probe
// runs (see the header note). The on/off buffers are adjacent in the shipped image. Declared in
// RendDX9Base.h: the probe writes them, the ShaderManager and the Ra
// EffectManager print them.
unsigned int  g_transparencyAAState = 0;
unsigned char g_transparencyAAOn[4]  = { 0, 0, 0, 0 };
unsigned char g_transparencyAAOff[4] = { 0, 0, 0, 0 };


// Cached state value and dirty tracking, with fixed-size mirror blocks per state category.

// One mirrored value. The texture block's cells carry the raw IDirect3DBaseTexture9* the setter is
// handed, so the value is pointer-sized: uintptr_t is unsigned int on x86 (the retail 4-byte cell
// word) and 64 bits on x64, where the render-state, sampler and TTF cells hold their 32-bit D3D
// values zero-extended. [CHOSEN, not recovered] x64 cell width (issue #7).
typedef uintptr_t StateValue;

static_assert(sizeof(StateValue) == sizeof(void*), "a cell value holds a texture pointer");

struct StateCell
{
    StateValue    f0Device;    // what the device has been told         (issue walk writes)
    StateValue    f1Base;      // what pop restores to                  (reset seeds; D3D defaults)
    StateValue    f2Wanted;    // what the next draw will issue         (setters write)
    unsigned char flags;       // bit0 touched-list, bit1 pending-list, bit2 base-seeded
    unsigned char pad[3];
};

// Two-slot block vtable. Both ends of every dispatch are in this repository, so the slots use the
// front's __fastcall shim shape (ecx -> self, edx dead, stack args, callee-clean) which is
// call-compatible with the shipped __thiscall sites.
//     slot 0 = issue(index, value)  - the one D3D call this block family makes
//     slot 1 = copy(dst, src)       - *dst = *src; the texture block overrides it refcounted
typedef void (__thiscall* BlockIssueFn)(void* self, unsigned int index,
                                        StateValue value);
typedef void (__thiscall* BlockCopyFn)(void* self, StateValue* dst,
                                       StateValue* src);

template <unsigned int N>
struct StateMirrorBlock
{
    void* const*  vptr;
    StateCell     cells[N];
    unsigned char listTouched[N];
    unsigned char countTouched;
    unsigned char listPending[N];
    unsigned char countPending;
};

// The two indexed families carry their D3D stage in a trailing dword the issuer reads
// (sub_10027740 reads this[74] of the 300-byte block; sub_10027770 reads this[6] of the 28-byte
// one). The constructor stamps 0..7 into them.
struct SamplerBlock : StateMirrorBlock<16> { unsigned int stage; };
struct TtfBlock     : StateMirrorBlock<1>  { unsigned int stage; };

typedef StateMirrorBlock<255> RenderStateBlock;
typedef StateMirrorBlock<16>  TextureBlock;

// The retail x86 layout. x64 cells and blocks are native; nothing reads them by byte offset.
#if defined(_M_IX86)
static_assert(sizeof(StateCell)        == 16,   "the mirror cell is 16 bytes");
static_assert(sizeof(RenderStateBlock) == 4596, "render-state block: this+0x10..+0x1203");
static_assert(sizeof(TextureBlock)     == 296,  "texture block: this+0x1204..+0x132B");
static_assert(sizeof(SamplerBlock)     == 300,  "one sampler block per stage");
static_assert(sizeof(TtfBlock)         == 28,   "one texture-transform block per stage");
static_assert(offsetof(RenderStateBlock, listTouched) == 4084, "touched list at block+4084");
static_assert(offsetof(RenderStateBlock, countTouched) == 4339, "touched count at block+4339");
static_assert(offsetof(RenderStateBlock, listPending) == 4340, "pending list at block+4340");
static_assert(offsetof(RenderStateBlock, countPending) == 4595, "pending count at block+4595");
static_assert(offsetof(SamplerBlock, stage) == 296, "sampler stage index at block+296");
static_assert(offsetof(TtfBlock, stage)     == 24,  "TTF stage index at block+24");
#endif


// The StateManager object - 16032 (0x3EA0) bytes, every field placed by the constructor
// (0x10026AB0), the reset (0x10025D00) and the consumers that read them back.

struct StateManager
{
    /* 0x0000 */ void* const*     vptr;            // 26 slots at off_101DFEA8
    /* 0x0004 */ void* const*     listenerVptr;    // 3 slots at off_101DFE98 (lost/reset/destroy)
    /* 0x0008 */ int              refCount;        // ctor: 1
    /* 0x000C */ IDirect3DDevice9* device;         // AddRef'd by the ctor
    /* 0x0010 */ RenderStateBlock renderStates;
    /* 0x1204 */ TextureBlock     textures;        // 16 stages; refcounted copy override
    /* 0x132C */ SamplerBlock     samplers[8];
    /* 0x1C8C */ TtfBlock         textureTransforms[8];
    /* 0x1D6C */ unsigned int     pushDepth;       // this[1883]; pop at zero runs the restore walk

    // plain value+dirty mirrors, flushed by the pre-draw apply
    /* 0x1D70 */ void*            streamVB[16];      // reset: 0
    /* 0x1DB0 */ unsigned int     streamOffset[16];  // reset: -1
    /* 0x1DF0 */ unsigned int     streamStride[16];  // reset: -1
    /* 0x1E30 */ unsigned int     streamDirty[16];   // reset: 0
    /* 0x1E70 */ void*            indices;
    /* 0x1E74 */ unsigned int     indicesDirty;
    /* 0x1E78 */ void*            vertexDecl;
    /* 0x1E7C */ unsigned int     vertexDeclDirty;
    /* 0x1E80 */ void*            vertexShader;
    /* 0x1E84 */ unsigned int     vertexShaderDirty;
    /* 0x1E88 */ void*            pixelShader;
    /* 0x1E8C */ unsigned int     pixelShaderDirty;
    /* 0x1E90 */ IDirect3DSurface9* renderTarget;   // refcounted (slot +0x60)
    /* 0x1E94 */ unsigned char    renderTargetDirty;
    /* 0x1E95 */ unsigned char    pad1E95[3];
    /* 0x1E98 */ IDirect3DSurface9* depthStencil;   // refcounted (slot +0x64)
    /* 0x1E9C */ unsigned char    depthStencilDirty;
    /* 0x1E9D */ unsigned char    pad1E9D[3];
    /* 0x1EA0 */ unsigned int     maxStreamTouched;  // this[1960]; SetStreamSource raises it
    /* 0x1EA4 */ unsigned int     maxStageTouched;   // this[1961]; SetSamplerState raises it;
                                                     // pop AND pre-draw loop stages 0..this value

    // the shader-constant register files
    // Batched flush: scan dirty bytes min..max, issue each contiguous dirty run as ONE
    // Set*ShaderConstantF, clear as you go. min only ever moves down (start -1 = 0xFFFFFFFF, so the
    // unsigned compare adopts the first register written), max only up.
    /* 0x1EA8 */ float            vsConst[256][4];
    /* 0x2EA8 */ unsigned char    vsConstDirty[256];
    /* 0x2FA8 */ unsigned int     vsConstMin;      // ctor: -1
    /* 0x2FAC */ unsigned int     vsConstMax;      // ctor: 0
    /* 0x2FB0 */ void*            vsConstIssueFn;  // sub_100272C0 - device SetVertexShaderConstantF
    /* 0x2FB4 */ float            psConst[224][4];
    /* 0x3DB4 */ unsigned char    psConstDirty[224];
    /* 0x3E94 */ unsigned int     psConstMin;      // ctor: -1
    /* 0x3E98 */ unsigned int     psConstMax;      // ctor: 0
    /* 0x3E9C */ void*            psConstIssueFn;  // sub_100272B0 - device SetPixelShaderConstantF

    void setRenderStatePersistent(unsigned int state, unsigned int value);
    void setTransparencyAA(bool enabled);
};

// The retail x86 layout. createStateManager allocates sizeof(StateManager); every other TU reaches
// the object through the seams below (the listener and RT0 accessors included), so the x64 object is
// native.
#if defined(_M_IX86)
static_assert(sizeof(StateManager) == 16032,
              "RendDX9Init.cpp:232 allocates 0x3EA0 for the StateManager");
static_assert(offsetof(StateManager, renderStates)      == 0x0010, "RS block at +0x10");
static_assert(offsetof(StateManager, textures)          == 0x1204, "texture block at +0x1204");
static_assert(offsetof(StateManager, samplers)          == 0x132C, "sampler blocks at +0x132C");
static_assert(offsetof(StateManager, textureTransforms) == 0x1C8C, "TTF blocks at +0x1C8C");
static_assert(offsetof(StateManager, pushDepth)         == 0x1D6C, "push depth at +0x1D6C");
static_assert(offsetof(StateManager, streamVB)          == 0x1D70, "stream VBs at +0x1D70");
static_assert(offsetof(StateManager, indices)           == 0x1E70, "indices mirror at +0x1E70");
static_assert(offsetof(StateManager, renderTarget)      == 0x1E90, "RT mirror at +0x1E90");
static_assert(offsetof(StateManager, maxStageTouched)   == 0x1EA4, "stage watermark at +0x1EA4");
static_assert(offsetof(StateManager, vsConst)           == 0x1EA8, "VS constants at +0x1EA8");
static_assert(offsetof(StateManager, vsConstMin)        == 0x2FA8, "VS min at +0x2FA8");
static_assert(offsetof(StateManager, psConst)           == 0x2FB4, "PS constants at +0x2FB4");
static_assert(offsetof(StateManager, psConstIssueFn)    == 0x3E9C, "the object ends at +0x3EA0");
static_assert(offsetof(StateManager, listenerVptr)      == 0x0004, "listener subobject at +0x04");
static_assert(offsetof(StateManager, renderTargetDirty) == 0x1E94, "RT dirty byte at +0x1E94");
#endif

// The two vtable arrays, defined below the slot bodies; smRelease restores them at destruction.
extern void* const kStateManagerVtable[26];
extern void* const kStateManagerListenerVtable[3];

namespace {

inline IDirect3DDevice9* rendererDevice()
{
    return static_cast<RendDX9Base*>(g_rendererBase)->device;
}

inline StateManager* sm() { return static_cast<StateManager*>(g_stateManager); }


// The block vtable slot bodies.


// Shared copy, 0x10027350 (comdat-folded into every non-texture vtable): *dst = *src.
void RD9_THUNK blockCopyPlain(void* RD9_THUNK_EDX, StateValue* dst, StateValue* src)
{
    *dst = *src;
}

// The texture block's override, Release the outgoing reference, AddRef the incoming.
// The shipped body calls vtable +0x08 to release and +0x04 to add - IUnknown's Release/AddRef
// slots, __stdcall with the object pushed - because what travels through these cells is the RAW
// IDirect3DBaseTexture9 the setter was handed (see smSetTexture).
void RD9_THUNK blockCopyTextureRef(void* RD9_THUNK_EDX, StateValue* dst, StateValue* src)
{
    if (*dst)
        reinterpret_cast<IUnknown*>(*dst)->Release();
    *dst = *src;
    if (*src)
        reinterpret_cast<IUnknown*>(*src)->AddRef();
}

// Applies render state with vendor-specific transparency AA codes.
// Any nonzero unsigned request enables it when multisample AA is active.
void RD9_THUNK rsIssue(void* self RD9_THUNK_EDX, unsigned int state, StateValue value)
{
    RenderStateBlock* block = static_cast<RenderStateBlock*>(self);
    IDirect3DDevice9* dev = rendererDevice();

    if (state != g_transparencyAAState)
    {
        dev->SetRenderState(static_cast<D3DRENDERSTATETYPE>(state), static_cast<DWORD>(value));
        return;
    }

    const bool msaaWasOn = block->cells[161].f0Device == 1;   // D3DRS_MULTISAMPLEANTIALIAS
    if (!msaaWasOn)
        dev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, 1);

    unsigned int onCode, offCode;
    ::memcpy(&onCode, g_transparencyAAOn, 4);
    ::memcpy(&offCode, g_transparencyAAOff, 4);

    unsigned int snapped = static_cast<unsigned int>(value);   // a render-state value is 32-bit
    if (value != onCode && value != offCode)
        snapped = (static_cast<double>(value) >= 0.5) ? onCode : offCode;
    dev->SetRenderState(static_cast<D3DRENDERSTATETYPE>(state), snapped);

    if (!msaaWasOn)
        dev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, 0);
}

// Texture issue, 0x10027720. The cell value is an EngineTexture wrapper; the D3D base texture is
// unwrapped by the pre-draw's own SetTexture caller? No - it is not: the shipped issuer hands the
// value straight to the device, and the setter (slot +0x20) is given the D3D base texture already
// unwrapped by its callers. What travels through the texture cells is therefore the RAW
// IDirect3DBaseTexture9 on the issue side and the refcounted wrapper never reaches the device.
// See smSetTexture below - D3DX calls it with the raw pointer it got from FXParameter's unwrap.
void RD9_THUNK texIssue(void* RD9_THUNK_EDX, unsigned int stage, StateValue value)
{
    rendererDevice()->SetTexture(stage, reinterpret_cast<IDirect3DBaseTexture9*>(value));
}

// Sampler issue, the stage rides in the block's own tail dword.
void RD9_THUNK samplerIssue(void* self RD9_THUNK_EDX, unsigned int type, StateValue value)
{
    rendererDevice()->SetSamplerState(static_cast<SamplerBlock*>(self)->stage,
                                      static_cast<D3DSAMPLERSTATETYPE>(type),
                                      static_cast<DWORD>(value));
}

// Texture-transform issue, always D3DTSS 24 (texturetransformflags), index ignored.
void RD9_THUNK ttfIssue(void* self RD9_THUNK_EDX, unsigned int, StateValue value)
{
    rendererDevice()->SetTextureStageState(static_cast<TtfBlock*>(self)->stage,
                                           D3DTSS_TEXTURETRANSFORMFLAGS,
                                           static_cast<DWORD>(value));
}

// The five block vtables, mirroring off_101DFF10..off_101DFF30. The abstract base's issue slot is
// pure (0x101C450E is _purecall); nothing here ever dispatches through the base vtable because the
// constructor immediately overwrites it, so the base slot aborts if it is ever reached.
void RD9_THUNK blockIssuePure(void* self RD9_THUNK_EDX, unsigned int, StateValue)
{
    RD9_FRONTIER_AT("dice::hfe::dx9rend::StateManager - _purecall on a mirror block "
                    "(the base vtable at off_101DFF10 was left installed)", self, 0x00);
}

void* const kBlockVtableBase[2]    = { reinterpret_cast<void*>(&blockIssuePure),
                                       reinterpret_cast<void*>(&blockCopyPlain) };
void* const kBlockVtableRs[2]      = { reinterpret_cast<void*>(&rsIssue),
                                       reinterpret_cast<void*>(&blockCopyPlain) };
void* const kBlockVtableTexture[2] = { reinterpret_cast<void*>(&texIssue),
                                       reinterpret_cast<void*>(&blockCopyTextureRef) };
void* const kBlockVtableSampler[2] = { reinterpret_cast<void*>(&samplerIssue),
                                       reinterpret_cast<void*>(&blockCopyPlain) };
void* const kBlockVtableTtf[2]     = { reinterpret_cast<void*>(&ttfIssue),
                                       reinterpret_cast<void*>(&blockCopyPlain) };

inline BlockIssueFn blockIssueSlot(void* block)
{
    return reinterpret_cast<BlockIssueFn>((*static_cast<void* const**>(block))[0]);
}
inline BlockCopyFn blockCopySlot(void* block)
{
    return reinterpret_cast<BlockCopyFn>((*static_cast<void* const**>(block))[1]);
}


// The block algorithms - each written once over the template, exactly as the shipped three
// instantiations behave.


// Block constructor body (0x100272D0 / 0x10027310 / the ctor's inline loops via sub_10001000 +
// sub_10027290): zero all three values, clear the low three flag bits, zero both list counts.
template <unsigned int N>
void blockConstruct(StateMirrorBlock<N>* b, void* const* vtbl)
{
    b->vptr = vtbl;
    for (unsigned int i = 0; i < N; ++i)
    {
        b->cells[i].f0Device = 0;
        b->cells[i].f1Base   = 0;
        b->cells[i].f2Wanted = 0;
        b->cells[i].flags   &= 0xF8u;
    }
    b->countTouched = 0;
    b->countPending = 0;
}

// The setter body shared by SetRenderState/SetTexture/SetTextureStageState/SetSamplerState
// (0x10026500 / 0x10026580 / 0x10026600 / 0x10026690): redundant-set filter against the device
// value, wanted <- value through the copy slot, file on both lists.
template <unsigned int N>
void blockSet(StateMirrorBlock<N>* b, unsigned int index, StateValue value)
{
    StateCell& cell = b->cells[index];
    if ((cell.flags & 2u) == 0)
    {
        if (cell.f0Device == value)
            return;
        b->listPending[b->countPending++] = static_cast<unsigned char>(index);
        cell.flags |= 2u;
    }
    blockCopySlot(b)(b, &cell.f2Wanted, &value);
    if ((cell.flags & 1u) == 0)
    {
        b->listTouched[b->countTouched++] = static_cast<unsigned char>(index);
        cell.flags |= 1u;
    }
}

// The pre-draw issue walk (0x10026DD0 / 0x10026ED0 / 0x10027200): for every pending cell whose
// wanted value differs from the device value, adopt it (f0 <- f2, refcounted for textures) and
// make the one real D3D call. Returns the number of issues, as the shipped bodies do.
template <unsigned int N>
int blockIssuePending(StateMirrorBlock<N>* b)
{
    const unsigned char count = b->countPending;
    if (!count)
        return 0;
    int issued = 0;
    for (unsigned char i = 0; i < count; ++i)
    {
        const unsigned char index = b->listPending[i];
        StateCell& cell = b->cells[index];
        cell.flags &= ~2u;
        if (cell.f2Wanted != cell.f0Device)
        {
            blockCopySlot(b)(b, &cell.f0Device, &cell.f2Wanted);
            blockIssueSlot(b)(b, index, cell.f0Device);
            ++issued;
        }
    }
    b->countPending = 0;
    return issued;
}

// The POP restore walk (0x10026C90 / 0x10026FD0 / 0x10027100). Three phases:
//   A. drain the pending list, remembering entries that are neither freshly touched nor converged
//      (still diverged from base with no new order standing);
//   B. for every touched cell whose device value differs from its base, schedule the restore
//      (wanted <- base) and re-file it pending;
//   C. re-file the remembered stragglers.
// No D3D calls happen here - the next pre-draw apply issues the restores.
template <unsigned int N>
void blockScheduleRestore(StateMirrorBlock<N>* b)
{
    if (!b->countTouched)
        return;

    unsigned char saved[N];
    unsigned char savedCount = 0;

    const unsigned char pending = b->countPending;
    if (pending)
    {
        for (unsigned char i = 0; i < pending; ++i)
        {
            const unsigned char index = b->listPending[i];
            StateCell& cell = b->cells[index];
            cell.flags &= ~2u;
            if ((cell.flags & 1u) == 0 && cell.f0Device != cell.f1Base)
                saved[savedCount++] = index;
        }
        b->countPending = 0;
    }

    const unsigned char touched = b->countTouched;
    if (touched)
    {
        for (unsigned char i = 0; i < touched; ++i)
        {
            const unsigned char index = b->listTouched[i];
            StateCell& cell = b->cells[index];
            cell.flags &= ~1u;
            if (cell.f0Device != cell.f1Base)
            {
                b->listPending[b->countPending++] = index;
                cell.flags |= 2u;
                blockCopySlot(b)(b, &cell.f2Wanted, &cell.f1Base);
            }
        }
    }
    b->countTouched = 0;

    for (unsigned char i = 0; i < savedCount; ++i)
    {
        const unsigned char index = saved[i];
        b->cells[index].flags |= 2u;
        b->listPending[b->countPending++] = index;
    }
}

// The reset reissue walk (0x10026E60 / 0x10026F60): for every cell holding a base value, adopt it
// as the device value and push it at the device, clearing both list bits and both lists. Run after
// the reset seeds so the fresh device and all three mirrors agree.
template <unsigned int N>
void blockReissueSeeded(StateMirrorBlock<N>* b)
{
    for (unsigned int i = 0; i < N; ++i)
    {
        StateCell& cell = b->cells[i];
        if (cell.flags & 4u)
        {
            blockCopySlot(b)(b, &cell.f0Device, &cell.f1Base);
            cell.flags &= 0xFCu;
            blockIssueSlot(b)(b, i, cell.f0Device);
        }
    }
    b->countPending = 0;
    b->countTouched = 0;
}

// Seed one cell's base value and mark it held - the reset's `|= 4; f1 = value` store pair.
template <unsigned int N>
inline void blockSeedBase(StateMirrorBlock<N>* b, unsigned int index, StateValue value)
{
    b->cells[index].flags |= 4u;
    b->cells[index].f1Base = value;
}


// Seeds and reissues defaults, then clears mirrors. Before vendor probing, zero AA globals
// intentionally cause a harmless rejected SetRenderState(0, 0).

void resetToDefaults(StateManager* self)
{
    RenderStateBlock& rs = self->renderStates;

    blockSeedBase(&rs,   7u, 1u);            // D3DRS_ZENABLE            = true
    blockSeedBase(&rs,   8u, 3u);            // D3DRS_FILLMODE           = solid
    blockSeedBase(&rs,   9u, 2u);            // D3DRS_SHADEMODE          = gouraud
    blockSeedBase(&rs,  14u, 1u);            // D3DRS_ZWRITEENABLE       = true
    blockSeedBase(&rs,  15u, 0u);            // D3DRS_ALPHATESTENABLE    = false
    blockSeedBase(&rs,  19u, 2u);            // D3DRS_SRCBLEND           = ONE
    blockSeedBase(&rs,  20u, 1u);            // D3DRS_DESTBLEND          = zero
    blockSeedBase(&rs,  22u, 3u);            // D3DRS_CULLMODE           = CCW
    blockSeedBase(&rs,  23u, 4u);            // D3DRS_ZFUNC              = lessequal
    blockSeedBase(&rs,  24u, 0u);            // D3DRS_ALPHAREF           = 0
    blockSeedBase(&rs,  25u, 5u);            // D3DRS_ALPHAFUNC          = greater (dice's, not D3D's 8)
    blockSeedBase(&rs,  27u, 0u);            // D3DRS_ALPHABLENDENABLE   = false
    blockSeedBase(&rs,  52u, 0u);            // D3DRS_STENCILENABLE      = false
    blockSeedBase(&rs,  53u, 1u);            // D3DRS_STENCILFAIL        = keep
    blockSeedBase(&rs,  54u, 1u);            // D3DRS_STENCILZFAIL       = keep
    blockSeedBase(&rs,  55u, 1u);            // D3DRS_STENCILPASS        = keep
    blockSeedBase(&rs,  56u, 8u);            // D3DRS_STENCILFUNC        = always
    blockSeedBase(&rs,  57u, 0u);            // D3DRS_STENCILREF         = 0
    blockSeedBase(&rs,  58u, 0xFFFFFFFFu);   // D3DRS_STENCILMASK
    blockSeedBase(&rs,  59u, 0xFFFFFFFFu);   // D3DRS_STENCILWRITEMASK
    blockSeedBase(&rs,  60u, 0xFFFFFFFFu);   // D3DRS_TEXTUREFACTOR
    for (unsigned int wrap = 128u; wrap <= 135u; ++wrap)
        blockSeedBase(&rs, wrap, 0u);        // D3DRS_WRAP0..WRAP7
    blockSeedBase(&rs, 152u, 0u);            // D3DRS_CLIPPLANEENABLE
    blockSeedBase(&rs, 168u, 15u);           // D3DRS_COLORWRITEENABLE
    blockSeedBase(&rs, 171u, 1u);            // D3DRS_BLENDOP            = ADD
    blockSeedBase(&rs, 174u, 0u);            // D3DRS_SCISSORTESTENABLE
    blockSeedBase(&rs, 175u, 0u);            // D3DRS_SLOPESCALEDEPTHBIAS
    blockSeedBase(&rs, 185u, 0u);            // D3DRS_TWOSIDEDSTENCILMODE
    blockSeedBase(&rs, 186u, 1u);            // D3DRS_CCW_STENCILFAIL    = keep
    blockSeedBase(&rs, 187u, 1u);            // D3DRS_CCW_STENCILZFAIL   = keep
    blockSeedBase(&rs, 188u, 1u);            // D3DRS_CCW_STENCILPASS    = keep
    blockSeedBase(&rs, 189u, 8u);            // D3DRS_CCW_STENCILFUNC    = always
    blockSeedBase(&rs, 190u, 15u);           // D3DRS_COLORWRITEENABLE1
    blockSeedBase(&rs, 191u, 15u);           // D3DRS_COLORWRITEENABLE2
    blockSeedBase(&rs, 192u, 15u);           // D3DRS_COLORWRITEENABLE3
    blockSeedBase(&rs, 193u, 0xFFFFFFFFu);   // D3DRS_BLENDFACTOR
    blockSeedBase(&rs, 195u, 0u);            // D3DRS_DEPTHBIAS

    // The transparency-AA state, seeded to its vendor OFF code - raw, unguarded, exactly as
    // shipped (see the function comment for the zero-globals shape).
    {
        unsigned int offCode;
        ::memcpy(&offCode, g_transparencyAAOff, 4);
        blockSeedBase(&rs, g_transparencyAAState, offCode);
    }

    blockSeedBase(&rs,  28u, 0u);            // D3DRS_FOGENABLE          = false
    blockSeedBase(&rs,  34u, 0xFF00FF00u);   // D3DRS_FOGCOLOR           = opaque green
    blockSeedBase(&rs,  36u, 0u);            // D3DRS_FOGSTART           = 0.0f
    blockSeedBase(&rs,  37u, 0x3F800000u);   // D3DRS_FOGEND             = 1.0f

    blockReissueSeeded(&rs);

    // Texture stages 0..7 revert to "no texture".
    for (unsigned int stage = 0; stage < 8; ++stage)
        blockSeedBase(&self->textures, stage, 0u);
    blockReissueSeeded(&self->textures);

    // Sampler defaults - the D3D9 runtime's own - then reissue per stage. The TTF blocks reproduce
    // the shipped inline shape: force the device value to 0 (a fresh device's default), fold the
    // base onto it through the copy slot, clear the list bits, reissue, and drop both lists.
    for (unsigned int stage = 0; stage < 8; ++stage)
    {
        SamplerBlock& sb = self->samplers[stage];
        blockSeedBase(&sb,  1u, 1u);         // D3DSAMP_ADDRESSU   = wrap
        blockSeedBase(&sb,  2u, 1u);         // D3DSAMP_ADDRESSV   = wrap
        blockSeedBase(&sb,  3u, 1u);         // D3DSAMP_ADDRESSW   = wrap
        blockSeedBase(&sb,  5u, 1u);         // D3DSAMP_MAGFILTER  = point
        blockSeedBase(&sb,  6u, 1u);         // D3DSAMP_MINFILTER  = point
        blockSeedBase(&sb,  7u, 0u);         // D3DSAMP_MIPFILTER  = none
        blockSeedBase(&sb,  8u, 0u);         // D3DSAMP_MIPMAPLODBIAS
        blockSeedBase(&sb,  9u, 0u);         // D3DSAMP_MAXMIPLEVEL
        blockSeedBase(&sb, 10u, 1u);         // D3DSAMP_MAXANISOTROPY
        blockSeedBase(&sb, 12u, 0u);         // D3DSAMP_ELEMENTINDEX
        blockReissueSeeded(&sb);

        TtfBlock& tb = self->textureTransforms[stage];
        StateCell& cell = tb.cells[0];
        cell.flags |= 4u;
        cell.f1Base = 0;
        blockCopySlot(&tb)(&tb, &cell.f0Device, &cell.f1Base);
        const StateValue value = cell.f0Device;
        cell.flags &= 0xFCu;
        blockIssueSlot(&tb)(&tb, 0, value);
        tb.countTouched = 0;
        tb.countPending = 0;
    }

    // The plain mirrors: streams to {0,-1,-1,clean}, everything else null and clean.
    for (unsigned int i = 0; i < 16; ++i)
    {
        self->streamVB[i]     = 0;
        self->streamOffset[i] = 0xFFFFFFFFu;
        self->streamStride[i] = 0xFFFFFFFFu;
        self->streamDirty[i]  = 0;
    }
    self->indices           = 0;  self->indicesDirty      = 0;
    self->vertexDecl        = 0;  self->vertexDeclDirty   = 0;
    self->vertexShader      = 0;  self->vertexShaderDirty = 0;
    self->pixelShader       = 0;  self->pixelShaderDirty  = 0;
    self->renderTarget      = 0;  self->renderTargetDirty = 0;
    self->depthStencil      = 0;  self->depthStencilDirty = 0;
}


// The shader-constant register files (sub_10027370 / sub_100274C0 mirrors, sub_100273F0 /
// sub_10027540 flushes). One body each, parameterized by the file the caller names.

void constFileWrite(float (*data)[4], unsigned char* dirty, unsigned int* minReg,
                    unsigned int* maxReg, unsigned int reg, const void* src, unsigned int count)
{
    ::memcpy(data[reg], src, 16u * count);
    ::memset(dirty + reg, 0xFF, count);
    if (*minReg >= reg)          // unsigned: the initial -1 adopts the first register written
        *minReg = reg;
    if (*maxReg < reg + count)
        *maxReg = reg + count;
}

void constFileFlush(StateManager* self, float (*data)[4], unsigned char* dirty,
                    unsigned int* minReg, unsigned int* maxReg, bool pixel,
                    int* outRegs, int* outCalls)
{
    unsigned int cursor = *minReg;
    const unsigned int limit = *maxReg;
    if (cursor < limit)
    {
        while (cursor < limit)
        {
            while (cursor < limit && !dirty[cursor])
                ++cursor;
            const unsigned int runStart = cursor;
            while (cursor < limit && dirty[cursor])
                dirty[cursor++] = 0;
            if (pixel)
                self->device->SetPixelShaderConstantF(runStart, data[runStart],
                                                      cursor - runStart);
            else
                self->device->SetVertexShaderConstantF(runStart, data[runStart],
                                                       cursor - runStart);
            ++*outCalls;
            *outRegs += static_cast<int>(cursor - runStart);
            ++cursor;
        }
    }
    *minReg = 0xFFFFFFFFu;
    *maxReg = 0;
}


// The 26 primary vtable slots. COM shape: __stdcall with `this` as the first stack argument, which
// is exactly what the shipped bodies decompile to and what D3DX will call through the vtable.


const unsigned char kIID_IUnknown[16] =
{ 0x00,0x00,0x00,0x00, 0x00,0x00, 0x00,0x00, 0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46 };

// +0x00 QueryInterface, 0x10025B20. Answers only IID_IUnknown - not ID3DXEffectStateManager's
// own IID. D3DX takes the interface pointer without QI'ing for the derived IID, so this is enough.
int __stdcall smQueryInterface(StateManager* self, const void* iid, void** out)
{
    *out = 0;
    if (::memcmp(iid, kIID_IUnknown, 16) != 0)
        return static_cast<int>(0x80004002);   // E_NOINTERFACE
    *out = self;
    return 0;
}

// +0x04 AddRef, 0x10026490 / +0x08 Release, 0x100264B0. Release restores both vtables, detaches
// the device-lost listener, releases the device and frees through the engine allocator. It does
// NOT release the texture/RT/DS references still held in the mirrors - shipped behaviour.
int __stdcall smAddRef(StateManager* self) { return ++self->refCount; }

int __stdcall smRelease(StateManager* self)
{
    const int remaining = --self->refCount;
    if (!remaining && self)
    {
        self->vptr = kStateManagerVtable;
        self->listenerVptr = kStateManagerListenerVtable;
        // 0x100264C5..0x100264DB: `lea eax, [esi+4]` ... `mov ecx,
        // dword_1023B970; push eax; call sub_10009D70` - the device-lost detach, now that the
        // constructor registers (see createStateManager).
        unregisterDeviceLostResetListener(static_cast<void*>(&self->listenerVptr));
        self->device->Release();
        ::renddx9abi::engineFree(self);
    }
    return remaining;
}

// +0x0C SetTransform / +0x10 SetMaterial / +0x14 SetLight / +0x18 LightEnable / +0x2C SetNPatchMode
// / +0x30 SetFVF - all E_NOTIMPL one-liners in the shipped image (0x10025B60..0x10025BE0). The
// fixed-function paths simply are not routed through the StateManager.
int __stdcall smSetTransform(StateManager*, unsigned int, const void*) { return static_cast<int>(0x80004001); }
int __stdcall smSetMaterial(StateManager*, const void*)                { return static_cast<int>(0x80004001); }
int __stdcall smSetLight(StateManager*, unsigned int, const void*)     { return static_cast<int>(0x80004001); }
int __stdcall smLightEnable(StateManager*, unsigned int, int)          { return static_cast<int>(0x80004001); }
int __stdcall smSetNPatchMode(StateManager*, float)                    { return static_cast<int>(0x80004001); }
int __stdcall smSetFVF(StateManager*, unsigned int)                    { return static_cast<int>(0x80004001); }

// +0x1C SetRenderState, 0x10026500.
int __stdcall smSetRenderState(StateManager* self, unsigned int state, unsigned int value)
{
    blockSet(&self->renderStates, state, value);
    return 0;
}

// +0x20 SetTexture, 0x10026580. The value D3DX passes is the RAW IDirect3DBaseTexture9 (FXParameter
// unwraps the EngineTexture before SetTexture reaches the effect), so the refcounted copy slot
// AddRef/Releases D3D objects here - their IUnknown slots 1/2 line up with the wrapper convention.
int __stdcall smSetTexture(StateManager* self, unsigned int stage, void* texture)
{
    blockSet(&self->textures, stage, reinterpret_cast<StateValue>(texture));
    return 0;
}

// +0x24 SetTextureStageState, 0x10026600. Only D3DTSS_TEXTURETRANSFORMFLAGS (24) is mirrored;
// every other stage state goes straight through to the device.
int __stdcall smSetTextureStageState(StateManager* self, unsigned int stage, unsigned int type,
                                     unsigned int value)
{
    if (type != 24u)
    {
        self->device->SetTextureStageState(stage, static_cast<D3DTEXTURESTAGESTATETYPE>(type),
                                           value);
        return 0;
    }
    blockSet(&self->textureTransforms[stage], 0u, value);
    return 0;
}

// +0x28 SetSamplerState, raises the stage watermark the pop and pre-draw loops share.
int __stdcall smSetSamplerState(StateManager* self, unsigned int stage, unsigned int type,
                                unsigned int value)
{
    if (self->maxStageTouched < stage)
        self->maxStageTouched = stage;
    blockSet(&self->samplers[stage], type, value);
    return 0;
}

// +0x34 SetVertexShader, 0x10025BF0 / +0x44 SetPixelShader, 0x10025C20.
int __stdcall smSetVertexShader(StateManager* self, void* shader)
{
    if (self->vertexShader != shader)
    {
        self->vertexShader = shader;
        self->vertexShaderDirty = 1;
    }
    return 0;
}
int __stdcall smSetPixelShader(StateManager* self, void* shader)
{
    if (self->pixelShader != shader)
    {
        self->pixelShader = shader;
        self->pixelShaderDirty = 1;
    }
    return 0;
}

// +0x38 SetVertexShaderConstantF, 0x10026A50 -> mirror. +0x48 SetPixelShaderConstantF, 0x10026A80.
int __stdcall smSetVertexShaderConstantF(StateManager* self, unsigned int reg, const float* data,
                                         unsigned int count)
{
    constFileWrite(self->vsConst, self->vsConstDirty, &self->vsConstMin, &self->vsConstMax,
                   reg, data, count);
    return 0;
}
int __stdcall smSetPixelShaderConstantF(StateManager* self, unsigned int reg, const float* data,
                                        unsigned int count)
{
    constFileWrite(self->psConst, self->psConstDirty, &self->psConstMin, &self->psConstMax,
                   reg, data, count);
    return 0;
}

// The I and B constant slots pass straight through to the device (0x10026730 / 0x10026760 /
// 0x10026790 / 0x100267C0) - no mirror, no dirty tracking.
int __stdcall smSetVertexShaderConstantI(StateManager* self, unsigned int reg, const int* data,
                                         unsigned int count)
{
    self->device->SetVertexShaderConstantI(reg, data, count);
    return 0;
}
int __stdcall smSetVertexShaderConstantB(StateManager* self, unsigned int reg, const BOOL* data,
                                         unsigned int count)
{
    self->device->SetVertexShaderConstantB(reg, data, count);
    return 0;
}
int __stdcall smSetPixelShaderConstantI(StateManager* self, unsigned int reg, const int* data,
                                        unsigned int count)
{
    self->device->SetPixelShaderConstantI(reg, data, count);
    return 0;
}
int __stdcall smSetPixelShaderConstantB(StateManager* self, unsigned int reg, const BOOL* data,
                                        unsigned int count)
{
    self->device->SetPixelShaderConstantB(reg, data, count);
    return 0;
}

// the dice extension slots

// +0x54 SetIndices, 0x10025A10.
int __stdcall smSetIndices(StateManager* self, void* ib)
{
    if (self->indices != ib)
    {
        self->indices = ib;
        self->indicesDirty = 1;
    }
    return 0;
}

// Suppresses redundant stream bindings without comparing stride; stride-only changes are ignored.
int __stdcall smSetStreamSource(StateManager* self, unsigned int stream, void* vb,
                                unsigned int offset, unsigned int stride)
{
    if (self->maxStreamTouched < stream)
        self->maxStreamTouched = stream;
    if (self->streamVB[stream] != vb || self->streamOffset[stream] != offset)
    {
        self->streamVB[stream]     = vb;
        self->streamOffset[stream] = offset;
        self->streamStride[stream] = stride;
        self->streamDirty[stream]  = 1;
    }
    return 0;
}

// +0x5C SetVertexDeclaration, 0x10025A40.
int __stdcall smSetVertexDeclaration(StateManager* self, void* decl)
{
    if (self->vertexDecl != decl)
    {
        self->vertexDecl = decl;
        self->vertexDeclDirty = 1;
    }
    return 0;
}

// Maintains a refcounted render-target mirror. Only index 0 has valid storage;
// higher indices overlap the dirty bytes. The shipped body addresses `this + 0x1E90 + 4 * index`
// and `this + 0x1E94 + index`; measuring from the named members keeps that aliasing on x64.
int __stdcall smSetRenderTarget(StateManager* self, unsigned int index, IDirect3DSurface9* surface)
{
    IDirect3DSurface9** value =
        reinterpret_cast<IDirect3DSurface9**>(reinterpret_cast<unsigned char*>(self)
                                              + offsetof(StateManager, renderTarget)
                                              + sizeof(IDirect3DSurface9*) * index);
    unsigned char* dirtyByte = reinterpret_cast<unsigned char*>(self)
                             + offsetof(StateManager, renderTargetDirty) + index;
    if (*value != surface)
    {
        if (*value)
            (*value)->Release();
        *value = surface;
        if (surface)
            surface->AddRef();
        *dirtyByte = 1;
    }
    return 0;
}

// +0x64 SetDepthStencilSurface, refcounted mirror.
int __stdcall smSetDepthStencilSurface(StateManager* self, IDirect3DSurface9* surface)
{
    if (self->depthStencil != surface)
    {
        if (self->depthStencil)
            self->depthStencil->Release();
        self->depthStencil = surface;
        if (surface)
            surface->AddRef();
        self->depthStencilDirty = 1;
    }
    return 0;
}


// The render-target flush, sub_10026240. A null set for target 0 goes straight out; a non-null set
// first walks all 16 texture cells and unbinds any texture (wanted or device-bound) whose level-0
// surface IS the incoming render target - the make-a-texture-a-render-target hazard - then issues
// SetRenderTarget. Depth-stencil last.

IDirect3DSurface9* topSurfaceOf(void* baseTexture)
{
    IDirect3DBaseTexture9* base = static_cast<IDirect3DBaseTexture9*>(baseTexture);
    IDirect3DSurface9* surface = 0;
    const D3DRESOURCETYPE type = base->GetType();
    if (type == D3DRTYPE_TEXTURE)
        static_cast<IDirect3DTexture9*>(baseTexture)->GetSurfaceLevel(0, &surface);
    if (type == D3DRTYPE_CUBETEXTURE)
        static_cast<IDirect3DCubeTexture9*>(baseTexture)
            ->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &surface);
    if (surface)
        surface->Release();     // the shipped body drops the GetSurfaceLevel reference immediately
    return surface;             // compared by identity only - never dereferenced
}

void flushRenderTargets(StateManager* self)
{
    if (self->renderTargetDirty)
    {
        if (!self->renderTarget)
        {
            // The shipped null arm issues SetRenderTarget(0, NULL) - which D3D rejects for target
            // 0 - and clears the dirty flag either way.
            self->device->SetRenderTarget(0, 0);
            self->renderTargetDirty = 0;
        }
        else
        {
            for (unsigned int stage = 0; stage < 16; ++stage)
            {
                StateCell& cell = self->textures.cells[stage];
                if ((cell.flags & 4u) == 0)
                    continue;
                if ((cell.flags & 2u) != 0 && cell.f2Wanted)
                {
                    if (topSurfaceOf(reinterpret_cast<void*>(cell.f2Wanted))
                        == self->renderTarget)
                        cell.f2Wanted = 0;
                }
                if (cell.f0Device)
                {
                    if (topSurfaceOf(reinterpret_cast<void*>(cell.f0Device))
                        == self->renderTarget)
                    {
                        reinterpret_cast<IUnknown*>(cell.f0Device)->Release();
                        cell.f0Device = 0;
                        self->device->SetTexture(stage, 0);
                    }
                }
            }
            self->device->SetRenderTarget(0, self->renderTarget);
            self->renderTargetDirty = 0;
        }
    }
    if (self->depthStencilDirty)
    {
        self->device->SetDepthStencilSurface(self->depthStencil);
        self->depthStencilDirty = 0;
    }
}


// The pre-draw apply, sub_10026880 - the ONE function that issues deferred state, called by the
// base renderer's DrawPrimitive/DrawIndexedPrimitive wrappers immediately before the device draw.

void preDrawApply(StateManager* self)
{
    // First the ShaderManager's current effects get their D3DX-side CommitChanges - so parameter
    // sets made since BeginPass land in this draw.
    shaderManagerCommitCurrentEffects();

    blockIssuePending(&self->renderStates);
    flushRenderTargets(self);
    blockIssuePending(&self->textures);
    for (unsigned int stage = 0; stage <= self->maxStageTouched; ++stage)
    {
        blockIssuePending(&self->samplers[stage]);
        blockIssuePending(&self->textureTransforms[stage]);
    }

    for (unsigned int stream = 0; stream <= self->maxStreamTouched; ++stream)
    {
        if (self->streamDirty[stream])
        {
            self->device->SetStreamSource(
                stream, static_cast<IDirect3DVertexBuffer9*>(self->streamVB[stream]),
                self->streamOffset[stream], self->streamStride[stream]);
            self->streamDirty[stream] = 0;
        }
    }
    if (self->indicesDirty)
    {
        self->device->SetIndices(static_cast<IDirect3DIndexBuffer9*>(self->indices));
        self->indicesDirty = 0;
    }
    if (self->vertexDeclDirty)
    {
        self->device->SetVertexDeclaration(
            static_cast<IDirect3DVertexDeclaration9*>(self->vertexDecl));
        self->vertexDeclDirty = 0;
    }
    if (self->vertexShaderDirty)
    {
        self->device->SetVertexShader(static_cast<IDirect3DVertexShader9*>(self->vertexShader));
        self->vertexShaderDirty = 0;
    }
    if (self->pixelShaderDirty)
    {
        self->device->SetPixelShader(static_cast<IDirect3DPixelShader9*>(self->pixelShader));
        self->pixelShaderDirty = 0;
    }

    // The constant-run flushes. The shipped body collects registers-set/call counters into locals
    // it then discards; the out-parameters exist to keep that shape visible.
    int regs = 0, calls = 0;
    constFileFlush(self, self->vsConst, self->vsConstDirty, &self->vsConstMin, &self->vsConstMax,
                   false, &regs, &calls);
    regs = 0; calls = 0;
    constFileFlush(self, self->psConst, self->psConstDirty, &self->psConstMin, &self->psConstMax,
                   true, &regs, &calls);
}


// push (sub_100261A0) / pop (sub_100261B0). Pop's zero-depth arm schedules the restore-to-base of
// everything touched; the D3D calls happen at the next pre-draw apply.

void pushBracket(StateManager* self) { ++self->pushDepth; }

void popBracket(StateManager* self)
{
    if (self->pushDepth-- == 1)
    {
        blockScheduleRestore(&self->renderStates);
        blockScheduleRestore(&self->textures);
        for (unsigned int stage = 0; stage <= self->maxStageTouched; ++stage)
        {
            blockScheduleRestore(&self->samplers[stage]);
            blockScheduleRestore(&self->textureTransforms[stage]);
        }
    }
}


// Listener addresses are relative to the StateManager subobject at +4.
// Loss clears held resources and mirrors; reset reissues base state; destruction delegates to loss.

inline StateManager* fromListener(void* listener)
{
    return reinterpret_cast<StateManager*>(static_cast<unsigned char*>(listener)
                                           - offsetof(StateManager, listenerVptr));
}

// sub_10025980 - release all three references of all 16 texture cells.
void dropTextureBlockRefs(TextureBlock* block)
{
    for (unsigned int stage = 0; stage < 16; ++stage)
    {
        StateCell& cell = block->cells[stage];
        if (cell.f0Device) reinterpret_cast<IUnknown*>(cell.f0Device)->Release();
        cell.f0Device = 0;
        if (cell.f1Base) reinterpret_cast<IUnknown*>(cell.f1Base)->Release();
        cell.f1Base = 0;
        if (cell.f2Wanted) reinterpret_cast<IUnknown*>(cell.f2Wanted)->Release();
        cell.f2Wanted = 0;
    }
}

void RD9_THUNK listenerOnDeviceLost(void* listener RD9_THUNK_EDX)
{
    StateManager* self = fromListener(listener);
    dropTextureBlockRefs(&self->textures);
    if (self->renderTarget)
        self->renderTarget->Release();
    self->renderTarget = 0;
    if (self->depthStencil)
        self->depthStencil->Release();
    self->depthStencil = 0;
    for (unsigned int i = 0; i < 16; ++i)
    {
        self->streamVB[i]     = 0;
        self->streamOffset[i] = 0xFFFFFFFFu;
        self->streamStride[i] = 0xFFFFFFFFu;
        self->streamDirty[i]  = 0;
    }
    self->indices      = 0;  self->indicesDirty      = 0;
    self->vertexDecl   = 0;  self->vertexDeclDirty   = 0;
    self->vertexShader = 0;  self->vertexShaderDirty = 0;
    self->pixelShader  = 0;  self->pixelShaderDirty  = 0;
}

void RD9_THUNK listenerOnDeviceReset(void* listener RD9_THUNK_EDX)
{
    StateManager* self = fromListener(listener);
    blockReissueSeeded(&self->renderStates);
    blockReissueSeeded(&self->textures);
    for (unsigned int stage = 0; stage < 8; ++stage)
    {
        blockReissueSeeded(&self->samplers[stage]);
        TtfBlock& tb = self->textureTransforms[stage];
        StateCell& cell = tb.cells[0];
        if (cell.flags & 4u)
        {
            blockCopySlot(&tb)(&tb, &cell.f0Device, &cell.f1Base);
            const StateValue value = cell.f0Device;
            cell.flags &= 0xFCu;
            blockIssueSlot(&tb)(&tb, 0, value);
        }
        tb.countPending = 0;
        tb.countTouched = 0;
    }
}

void RD9_THUNK listenerOnDeviceDestroy(void* listener RD9_THUNK_EDX)
{
    listenerOnDeviceLost(listener RD9_THUNK_ARG);
}

} // anonymous namespace

// The primary vtable, off_101DFEA8 - ID3DXEffectStateManager's 21 slots then the five dice
// extensions, in the shipped order.
void* const kStateManagerVtable[26] =
{
    reinterpret_cast<void*>(&smQueryInterface),           // +0x00
    reinterpret_cast<void*>(&smAddRef),                   // +0x04
    reinterpret_cast<void*>(&smRelease),                  // +0x08
    reinterpret_cast<void*>(&smSetTransform),             // +0x0C  0x10025B60  E_NOTIMPL
    reinterpret_cast<void*>(&smSetMaterial),              // +0x10  0x10025B70  E_NOTIMPL
    reinterpret_cast<void*>(&smSetLight),                 // +0x14  0x10025B80  E_NOTIMPL
    reinterpret_cast<void*>(&smLightEnable),              // +0x18  0x10025B90  E_NOTIMPL
    reinterpret_cast<void*>(&smSetRenderState),           // +0x1C
    reinterpret_cast<void*>(&smSetTexture),               // +0x20
    reinterpret_cast<void*>(&smSetTextureStageState),     // +0x24
    reinterpret_cast<void*>(&smSetSamplerState),          // +0x28
    reinterpret_cast<void*>(&smSetNPatchMode),            // +0x2C  0x10025BD0  E_NOTIMPL
    reinterpret_cast<void*>(&smSetFVF),                   // +0x30  0x10025BE0  E_NOTIMPL
    reinterpret_cast<void*>(&smSetVertexShader),          // +0x34
    reinterpret_cast<void*>(&smSetVertexShaderConstantF), // +0x38
    reinterpret_cast<void*>(&smSetVertexShaderConstantI), // +0x3C  0x10026730  passthrough
    reinterpret_cast<void*>(&smSetVertexShaderConstantB), // +0x40  0x10026760  passthrough
    reinterpret_cast<void*>(&smSetPixelShader),           // +0x44
    reinterpret_cast<void*>(&smSetPixelShaderConstantF),  // +0x48
    reinterpret_cast<void*>(&smSetPixelShaderConstantI),  // +0x4C  0x10026790  passthrough
    reinterpret_cast<void*>(&smSetPixelShaderConstantB),  // +0x50  0x100267C0  passthrough
    reinterpret_cast<void*>(&smSetIndices),               // +0x54  0x10025A10  [DICE]
    reinterpret_cast<void*>(&smSetStreamSource),          // +0x58  0x10026430  [DICE]
    reinterpret_cast<void*>(&smSetVertexDeclaration),     // +0x5C  0x10025A40  [DICE]
    reinterpret_cast<void*>(&smSetRenderTarget),          // +0x60  0x10025A90  [DICE]
    reinterpret_cast<void*>(&smSetDepthStencilSurface)    // +0x64  0x10025AE0  [DICE]
};

void* const kStateManagerListenerVtable[3] =
{
    reinterpret_cast<void*>(&listenerOnDeviceLost),
    reinterpret_cast<void*>(&listenerOnDeviceReset),
    reinterpret_cast<void*>(&listenerOnDeviceDestroy)
};


// Construction - sub_10026AB0 wrapped by the RendDX9Init.cpp:232 call site (0x1000B31E..0x1000B35C:
// allocate 0x3EA0, run the constructor, publish dword_1023B984, log the failure line on null).

bool createStateManager(void* device)
{
    StateManager* self = static_cast<StateManager*>(
        ::renddx9abi::engineAlloc(sizeof(StateManager)));
    g_stateManager = self;
    if (!self)
    {
        ::renddx9abi::logf("RendDX9Init.cpp:232 Failed to create StateManager.");
        return false;
    }

    self->vptr = kStateManagerVtable;
    self->listenerVptr = kStateManagerListenerVtable;   // the ctor's double-write collapses to one
    self->refCount = 1;
    self->device = static_cast<IDirect3DDevice9*>(device);

    blockConstruct(&self->renderStates, kBlockVtableBase);
    self->renderStates.vptr = kBlockVtableRs;
    blockConstruct(&self->textures, kBlockVtableBase);
    self->textures.vptr = kBlockVtableTexture;
    for (unsigned int stage = 0; stage < 8; ++stage)
    {
        blockConstruct(static_cast<StateMirrorBlock<16>*>(&self->samplers[stage]),
                       kBlockVtableBase);
        self->samplers[stage].vptr = kBlockVtableSampler;
        self->samplers[stage].stage = stage;
        blockConstruct(static_cast<StateMirrorBlock<1>*>(&self->textureTransforms[stage]),
                       kBlockVtableBase);
        self->textureTransforms[stage].vptr = kBlockVtableTtf;
        self->textureTransforms[stage].stage = stage;
    }

    self->pushDepth = 0;
    self->maxStreamTouched = 0;
    self->maxStageTouched = 0;

    self->vsConstMin = 0xFFFFFFFFu;
    self->vsConstMax = 0;
    self->vsConstIssueFn = 0;    // the flush calls the device directly here
    ::memset(self->vsConstDirty, 0, sizeof(self->vsConstDirty));
    self->psConstMin = 0xFFFFFFFFu;
    self->psConstMax = 0;
    self->psConstIssueFn = 0;
    ::memset(self->psConstDirty, 0, sizeof(self->psConstDirty));

    self->device->AddRef();

    resetToDefaults(self);

    // the constructor's last act, 0x10026C70..0x10026C7B:
    //     mov ecx, dword_1023B970; push edi (= 2); lea eax, [esi+4]; push eax; call sub_10009D60
    // i.e. RendDX9_registerDeviceLostResetListener(this + 4, 2). Priority 2 puts the StateManager's
    // loss after every priority-3..5 listener has let go of its resources (so the texture / RT / DS
    // references its mirrors still hold are the last to drop) and its reset re-issue before them.
    registerDeviceLostResetListener(static_cast<void*>(&self->listenerVptr), 2);

#if defined(_M_IX86)
    ::renddx9abi::logf("StateManager: up at 0x%08X (16032 bytes, RendDX9Init.cpp:232) - device "
                       "0x%08X, defaults seeded and reissued",
                       reinterpret_cast<unsigned int>(self),
                       reinterpret_cast<unsigned int>(device));
#else
    ::renddx9abi::logf("StateManager: up at 0x%p (%u bytes, RendDX9Init.cpp:232) - device "
                       "0x%p, defaults seeded and reissued", static_cast<void*>(self),
                       static_cast<unsigned int>(sizeof(StateManager)), device);
#endif
    return true;
}


// Cross-TU seams. The shipped callers reach these as direct near calls with ecx = the object; here
// they are plain functions over the published global, in the front's seam style.

void stateManagerPush()
{
    pushBracket(sm());
}

void stateManagerPop()
{
    popBracket(sm());
}

// Returns the mirrored pixel shader used to identify the effect's bound shader model.
void* stateManagerCurrentPixelShader()
{
    return sm()->pixelShader;
}

// PE 0x10026500 / primary slot +0x1C.  LightManager uses this ordinary deferred setter for
// D3DRS_COLORWRITEENABLE-style shadow-pass state; it is not the persistent/base-changing helper.
void stateManagerSetRenderState(unsigned int state, unsigned int value)
{
    smSetRenderState(sm(), state, value);
}

// PE 0x10025A90 / +0x60 and 0x10025AE0 / +0x64.  The mirror owns COM references and defers the
// device calls until flushRenderTargets, exactly like the primary-vtable entry points.
void stateManagerSetRenderTarget(unsigned int index, IDirect3DSurface9* surface)
{
    smSetRenderTarget(sm(), index, surface);
}

void stateManagerSetDepthStencilSurface(IDirect3DSurface9* surface)
{
    smSetDepthStencilSurface(sm(), surface);
}

// sub_10025A70(0) - the desired render-target-0 mirror, without an AddRef. [CHOSEN, not recovered]
// as a free seam (issue #7): its callers read the member instead of `g_stateManager + 0x1E90`.
IDirect3DSurface9* stateManagerRenderTarget0()
{
    return static_cast<StateManager*>(g_stateManager)->renderTarget;
}

// The DeviceLostResetListener subobject (`dword_1023B984 + 4`) that Renderer slot +0xB8 drives
// directly. [CHOSEN, not recovered] seam (issue #7): the subobject's native offset differs on x64.
DeviceLostResetListener* stateManagerDeviceLostListener()
{
    return reinterpret_cast<DeviceLostResetListener*>(&sm()->listenerVptr);
}

// The helper first commits deferred render-target/depth state, then calls the D3D
// device Clear with no rectangles.  Its argument order is normalized here; every value still
// reaches the same D3D parameter.
void stateManagerClear(unsigned int flags, unsigned int colour, float depth,
                       unsigned int stencil)
{
    flushRenderTargets(sm());
    rendererDevice()->Clear(0, 0, flags, colour, depth, stencil);
}

// sub_10025A00 - true when no push bracket is open (renderFrame's :991 sanity check).
bool stateManagerIsFullyRestored()
{
    return sm()->pushDepth == 0;
}

// sub_10026220 - collapse any open bracket to depth 1 and pop it, scheduling every restore.
void stateManagerForceRestore()
{
    StateManager* self = sm();
    if (self->pushDepth)
    {
        self->pushDepth = 1;
        popBracket(self);
    }
}

// sub_10025BA0 - the persistent render-state set: write the device mirror AND the base value, mark
// the cell held, and issue immediately - bypassing the pending machinery.  The concrete block is
// known to be RenderStateBlock here, so call its recovered issuer body directly instead of
// reconstructing the PE's hand-indexed virtual dispatch. renderFrame uses this for the
// multisampleantialias force and the atoc brackets.
void StateManager::setRenderStatePersistent(unsigned int state, unsigned int value)
{
    StateCell& cell = renderStates.cells[state];
    cell.flags |= 4u;
    cell.f0Device = value;
    cell.f1Base   = value;
    rsIssue(&renderStates RD9_THUNK_ARG, state, value);
}

void stateManagerSetRenderStatePersistent(unsigned int state, unsigned int value)
{
    sm()->setRenderStatePersistent(state, value);
}

// PE renderFrame 0x1003027D/0x100302E9/0x1003032D/0x10030388.  The caller decides whether the
// transparency-AA probe succeeded; this owner operation preserves the probe-selected state and
// exact four-byte vendor code while keeping the StateManager's persistent update internal.
void StateManager::setTransparencyAA(bool enabled)
{
    unsigned int code;
    ::memcpy(&code, enabled ? g_transparencyAAOn : g_transparencyAAOff, sizeof(code));
    setRenderStatePersistent(g_transparencyAAState, code);
}

void stateManagerSetTransparencyAA(bool enabled)
{
    sm()->setTransparencyAA(enabled);
}

// sub_10026240 as a seam - RendDX9Draw.cpp's beginScene flushes the render-target mirror before a
// frame clear so the clear lands on the current target.
void flushRenderTargetsSeam()
{
    flushRenderTargets(sm());
}

void stateManagerPreDrawApply()
{
    preDrawApply(sm());
}

// Owner-TU entry points for the three dice binding slots.  Their retail callers dispatch through
// the primary vtable, but the concrete owner and exact slot bodies are recovered here; direct
// calls avoid duplicating an x86-only raw-vtable ABI at every wrapper boundary.
void stateManagerSetIndices(IDirect3DIndexBuffer9* indices)
{
    smSetIndices(sm(), indices);
}

void stateManagerSetStreamSource(unsigned int stream, IDirect3DVertexBuffer9* vertexBuffer,
                                 unsigned int offset, unsigned int stride)
{
    smSetStreamSource(sm(), stream, vertexBuffer, offset, stride);
}

void stateManagerSetVertexDeclaration(IDirect3DVertexDeclaration9* declaration)
{
    smSetVertexDeclaration(sm(), declaration);
}

// the +0x34 / +0x44 slots (0x10025BF0 / 0x10025C20) for the display-mode render
// probe (RendDX9Init.cpp, sub_10009FF0: 0x1000A150 `call [ecx+34h]`, 0x1000A15F `call [ecx+44h]`),
// which binds the shaders it has just assembled. Same deferred mirror as every other caller: the
// next pre-draw apply issues them.
void stateManagerSetVertexShader(IDirect3DVertexShader9* shader)
{
    smSetVertexShader(sm(), shader);
}

void stateManagerSetPixelShader(IDirect3DPixelShader9* shader)
{
    smSetPixelShader(sm(), shader);
}

// FXParameter.cpp's sparse-matrix fast path uploads bone rows through vslot +0x38.
void stateManagerSetVertexShaderConstantF(unsigned int reg, const float* data, unsigned int count)
{
    smSetVertexShaderConstantF(sm(), reg, data, count);
}


// The base renderer's draw wrappers, 0x10005870 (DrawPrimitive) and 0x10005820
// (DrawIndexedPrimitive). Both live in the base TU in the shipped image but belong to this
// subsystem's story: pre-draw apply, the device draw, then the frame-stats bump (base+0x108 block:
// enable byte +644; DIP counters +452/+456, DP counters +460/+464, shared prim total +448 -
// sub_1000D5B0 / sub_1000D620). A zero primitive count skips everything, wrappers included.

void rendererDrawPrimitive(unsigned int primCount, unsigned int type, unsigned int startVertex)
{
    if (!primCount)
        return;
    preDrawApply(sm());
    rendererDevice()->DrawPrimitive(static_cast<D3DPRIMITIVETYPE>(type), startVertex, primCount);
    unsigned char* stats = static_cast<RendDX9Base*>(g_rendererBase)->renderStats;
    if (stats[644])
    {
        ++*reinterpret_cast<unsigned int*>(stats + 460);
        *reinterpret_cast<unsigned int*>(stats + 464) += primCount;
        *reinterpret_cast<unsigned int*>(stats + 448) += primCount;
    }
}

// Submits an indexed draw. Argument order follows this declaration, which differs from
// the original helper's call convention: keep type and numVertices in their named positions.
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices,
                                  unsigned int startIndex)
{
    if (!primCount)
        return;
    preDrawApply(sm());
    rendererDevice()->DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(type), baseVertex,
                                           minIndex, numVertices, startIndex, primCount);
    unsigned char* stats = static_cast<RendDX9Base*>(g_rendererBase)->renderStats;
    if (stats[644])
    {
        ++*reinterpret_cast<unsigned int*>(stats + 452);
        *reinterpret_cast<unsigned int*>(stats + 456) += primCount;
        *reinterpret_cast<unsigned int*>(stats + 448) += primCount;
    }
}

// sub_100058B0 - the UP (user-pointer) variant, for SwiffRender's line strips.
// Read in full: clear stream 0 through the StateManager first (D3D requires no bound stream for a
// UP draw), pre-draw apply, DrawPrimitiveUP, then the UP stat pair (+468/+472; sub_1000D650) and
// the shared prim total (+448).
void rendererDrawPrimitiveUP(unsigned int primCount, const void* data,
                             unsigned int stride, unsigned int type)
{
    if (!primCount)
        return;
    stateManagerSetStreamSource(0, 0, 0, 0);
    preDrawApply(sm());
    rendererDevice()->DrawPrimitiveUP(static_cast<D3DPRIMITIVETYPE>(type), primCount, data,
                                      stride);
    unsigned char* stats = static_cast<RendDX9Base*>(g_rendererBase)->renderStats;
    if (stats[644])
    {
        ++*reinterpret_cast<unsigned int*>(stats + 468);
        *reinterpret_cast<unsigned int*>(stats + 472) += primCount;
        *reinterpret_cast<unsigned int*>(stats + 448) += primCount;
    }
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
