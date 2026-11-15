// [CHOSEN, not recovered] Private native bridge: no D3D or Objective-C value types cross it.
#ifndef BF2_METAL_BRIDGE_H
#define BF2_METAL_BRIDGE_H
#include <cstdint>
#include <cstddef>
namespace bf2metal {
struct Adapter;
struct Frame;
[[noreturn]] void frontier(const char* method);
void mainThread(const char* method);
Adapter* createAdapter();
void destroyAdapter(Adapter*);
const char* adapterName(Adapter*);
// The GPU's PCI vendor and device IDs (Apple silicon: 0x106B, 0).
void adapterPciIds(Adapter*, unsigned& vendor, unsigned& device);
bool supportsBC(Adapter*);
// The view is a borrowed Cocoa NSView with a CAMetalLayer, retained by Frame.
// samples > 1: a multisampled back buffer (and auto depth) resolved into the single-sample
// colour target at the end of every pass. [CHOSEN, not recovered]
Frame* createFrame(Adapter*, void* view, unsigned width, unsigned height, bool vsync, bool depth,
                   unsigned samples = 1);
void destroyFrame(Frame*);
bool reset(Frame*, unsigned width, unsigned height, bool vsync, bool depth, unsigned samples = 1);
// True when the GPU renders a colour (BGRA8) and D24S8-class depth target at this sample count.
bool supportsSamples(Adapter*, unsigned samples);
// The GPU's recommended working set in bytes (IDirect3DDevice9::GetAvailableTextureMem).
unsigned availableTextureMemory(Frame*);
void dimensions(Frame*, unsigned& width, unsigned& height);
bool resized(Frame*);
bool healthy(Frame*);
bool renderable(Frame*);
#if defined(BF2_METAL_DEVICE_TESTING)
// [TEMP] Harness-only fault injection at the next completed command submission.
void injectCommandBufferFailure(void* device);
void armCommandBufferFailure(Frame*);
#endif
bool clear(Frame*, unsigned flags, std::uint32_t color, float depth, unsigned stencil);
enum class PresentResult { ok, busy, lost, invalidWindow };
PresentResult present(Frame*, void* window, bool doNotWait);
// [CHOSEN, not recovered] G3b resource/state bridge. All sizes refer to D3D bytes.
struct Texture;
struct Buffer;
struct VertexLayout;
struct VertexElement { unsigned stream, offset, type, usage, index; };
struct RawState {
    std::uint32_t render[256] = {};
    std::uint32_t sampler[21][14] = {}; // pixel 0..15, displacement, vertex 0..3
    std::uint32_t stage[8][33] = {};
};
struct SamplerMapping {
    unsigned address[3], minFilter, magFilter, mipFilter, anisotropy, border;
    float lodBias, minLod;
    bool srgb;
};
struct StencilMapping { unsigned fail, depthFail, pass, compare; };
struct StateMapping {
    unsigned srcRGB, dstRGB, srcAlpha, dstAlpha, blendRGB, blendAlpha;
    unsigned depthCompare, cull, fill, colorMask;
    StencilMapping front, back;
    bool blend, depthWrite, stencil, alphaTest, scissor, srgbWrite;
    unsigned stencilReadMask, stencilWriteMask, stencilRef, alphaCompare, clipMask;
    float alphaRef, depthBias, slopeBias, blendColor[4];
    SamplerMapping sampler[21];
};
// Returns a qualified unsupported-state name without aborting; draws raise it.
const char* mapState(const RawState&, StateMapping&);
// Read-only harness access; these do not encode commands or execute draws.
bool inspectState(void* device, RawState&, StateMapping&, const char** refusal);
bool inspectDeclaration(void* declaration, unsigned element, VertexElement&, unsigned& metalFormat);
bool inspectSurface(void* surface, void* bytes, std::size_t size);
VertexLayout* createLayout(const VertexElement*, unsigned);
void destroyLayout(VertexLayout*);
unsigned layoutFormat(VertexLayout*, unsigned);
bool formatSupported(unsigned format);
unsigned formatBytes(unsigned format);
unsigned formatBlock(unsigned format);
bool formatRenderable(unsigned format);
// samples > 1: a multisampled 2D render target or depth surface (one level, not sampleable).
Texture* createTexture(Frame*, unsigned width, unsigned height, unsigned depth,
    unsigned levels, unsigned kind, unsigned format, unsigned usage, unsigned samples = 1);
void destroyTexture(Texture*);
// Borrowed wrappers, refreshed in place by Reset.
Texture* defaultTarget(Frame*, bool depth);
void defaultColorFormat(Frame*, unsigned format);
bool copyTextureRegion(Frame*,Texture* source,unsigned sourceLevel,unsigned sourceSlice,unsigned x,unsigned y,
    Texture* destination,unsigned destLevel,unsigned destSlice,unsigned dx,unsigned dy,unsigned width,unsigned height);
// [CHOSEN, not recovered] A same-size colour rectangle copied on the GPU (StretchRect between
// GPU surfaces): a multisampled source resolves first; opaque writes alpha 1 (X8 formats); the
// multisampled back buffer receives it in every sample. False when the pair needs the CPU path.
bool copyColor(Frame*, Texture* source, unsigned sx, unsigned sy, Texture* destination,
    unsigned destLevel, unsigned destSlice, unsigned dx, unsigned dy, unsigned width, unsigned height, bool opaque);
void defaultState(RawState&, bool depth, unsigned width, unsigned height);
bool transfer(Frame*, Texture*, unsigned level, unsigned slice, void* bytes,
    unsigned rowPitch, unsigned slicePitch, bool upload);
// [CHOSEN, not recovered] One rectangle from CPU bytes (D3D packing) into a texture, encoded into the
// open submission without a readback or a wait (UpdateSurface from a SYSTEMMEM surface).
bool uploadRegion(Frame*, Texture*, unsigned level, unsigned slice, unsigned x, unsigned y,
    unsigned width, unsigned height, const void* bytes, unsigned rowPitch);
struct ClearRect { unsigned x, y, width, height; };
bool clearTargets(Frame*, Texture* color, unsigned colorLevel, unsigned colorSlice,
    Texture* depth, unsigned depthLevel, unsigned depthSlice,
    unsigned flags, std::uint32_t colorValue, float z, unsigned stencil,
    const ClearRect*, std::size_t count);
Buffer* createBuffer(Frame*, unsigned length);
void destroyBuffer(Buffer*);
void* bufferBytes(Buffer*);
bool waitIdle(Frame*);
// [CHOSEN, not recovered] D3D9 event queries: Issue(D3DISSUE_END) submits the work recorded so far
// without waiting and returns a fence; GetData polls whether the GPU has completed it.
std::uint64_t submitFence(Frame*);
bool fenceReached(Frame*, std::uint64_t fence);
bool resample(unsigned format,const void* source,unsigned sw,unsigned sh,unsigned sourcePitch,
    void* destination,unsigned dw,unsigned dh,unsigned destPitch,bool linear);
}
#endif
