// [CHOSEN, not recovered] G3 Metal clear/present backend. Compile with ARC.
// The D3D subset stays in Device.cpp: its BOOL must never enter an Objective-C TU.
#include "MetalBridge.h"
#include "DrawBridge.h"
#include "EffectShaders.h"          // drawTraceLabel: the effect pass a compile belongs to
#include "../../macOS/HitchTrace.h" // [CHOSEN] BF2_HITCH_TRACE (port addition, measurement)
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#include <IOKit/IOKitLib.h>
#include <CommonCrypto/CommonDigest.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <atomic>
#include <chrono>
#include <deque>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace bf2metal {
[[noreturn]] void frontier(const char* method)
{
    std::fprintf(stderr, "[FRONTIER] %s reached: Metal operation is unimplemented\n", method);
    std::fflush(stderr);
    std::abort();
}
void mainThread(const char* method)
{
    if (![NSThread isMainThread]) {
        std::fprintf(stderr, "[FRONTIER] %s reached: Metal/AppKit requires the main thread\n", method);
        std::fflush(stderr);
        std::abort();
    }
}
struct Adapter { id<MTLDevice> device; };
struct Texture { id<MTLTexture> object; unsigned format; };
struct DrawableRequest {
    std::mutex mutex;
    std::condition_variable condition;
    id<CAMetalDrawable> drawable;
    bool acquiring=false;
};
struct Frame {
    id<MTLDevice> device;
    NSView* view;
    CAMetalLayer* layer;
    id<MTLCommandQueue> queue;
    id<MTLTexture> color, depth;
    // [CHOSEN, not recovered] MSAA: colorMS is what passes to the back buffer render into; they
    // resolve into color, which present, readbacks and copies keep using. depth is multisampled too.
    id<MTLTexture> colorMS;
    unsigned samples = 1;
    bool unresolved = false; // colorMS holds samples color does not have yet (resolveBackBuffer)
    id<MTLCommandBuffer> commands;
    id<MTLCommandBuffer> presentation;
    // [CHOSEN, not recovered] Frames in flight: the command buffers of presented frames that the GPU
    // may still execute (submit). Any committed buffer that fails latches gpuFailed.
    std::deque<id<MTLCommandBuffer>> inFlight;
    std::shared_ptr<std::atomic<bool>> gpuFailed = std::make_shared<std::atomic<bool>>(false);
    // Command buffers committed so far, and the highest one the GPU has completed (fences).
    std::uint64_t committed = 0;
    std::shared_ptr<std::atomic<std::uint64_t>> completed = std::make_shared<std::atomic<std::uint64_t>>(0);
    // Traced runs: submissions by present and by mid-frame flushes, waits for a frame in flight.
    unsigned long frameSubmits = 0, flushSubmits = 0, inFlightWaits = 0, inFlightAtSubmit = 0;
    double inFlightWaitMs = 0;
    std::map<void*,std::shared_ptr<DrawableRequest>> drawableRequests;
    bool failed = false;
#if defined(BF2_METAL_DEVICE_TESTING)
    bool failNextSubmission = false;
#endif
    Texture colorResource, depthResource;
    struct DrawCaches* draws = nullptr; // G4 open render pass and pipeline caches (DrawBridge.inc)
    struct Prebuild* prebuild = nullptr; // shader libraries and pipelines built ahead of draws (ShaderPrebuild.inc)
    unsigned long presents = 0;          // frames presented so far
    // DONOTWAIT presents (renderer.presentAsync, level loads): shown, and answered busy.
    unsigned long presentsNoWait = 0, presentsBusy = 0;
    // The last DONOTWAIT frame has reached the display (its drawable's presented handler ran), and
    // when it was presented. present() answers further DONOTWAIT calls busy until then.
    std::shared_ptr<std::atomic<bool>> noWaitOnScreen = std::make_shared<std::atomic<bool>>(true);
    std::chrono::steady_clock::time_point noWaitPresentedAt;
};
Adapter* createAdapter()
{
    mainThread("Direct3DCreate9");
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) return nullptr;
        Adapter* adapter = new (std::nothrow) Adapter;
        if (adapter) adapter->device = device;
        return adapter;
    }
}
void destroyAdapter(Adapter* a) { mainThread("IDirect3D9::Release"); delete a; }
const char* adapterName(Adapter* a) { mainThread("IDirect3D9::GetAdapterIdentifier"); return a->device.name.UTF8String; }
// A 32-bit little-endian IORegistry data property ("vendor-id", "device-id" of a PCI device).
static bool registryWord(io_registry_entry_t e, CFStringRef key, unsigned& out)
{
    CFTypeRef v = IORegistryEntryCreateCFProperty(e, key, kCFAllocatorDefault, 0);
    if (!v) return false;
    bool ok = false;
    if (CFGetTypeID(v) == CFDataGetTypeID() && CFDataGetLength(static_cast<CFDataRef>(v)) >= 2) {
        unsigned char b[4] = {};
        CFDataGetBytes(static_cast<CFDataRef>(v), CFRangeMake(0, std::min<CFIndex>(4, CFDataGetLength(static_cast<CFDataRef>(v)))), b);
        out = unsigned(b[0]) | unsigned(b[1]) << 8 | unsigned(b[2]) << 16 | unsigned(b[3]) << 24;
        ok = true;
    }
    CFRelease(v);
    return ok;
}
// [CHOSEN, not recovered] Owner decision 2026-11-12: the adapter reports the real GPU vendor and
// device. Intel and AMD GPUs are PCI devices: the MTLDevice's IORegistry entry or one of its
// ancestors carries "vendor-id"/"device-id". Apple silicon GPUs (AGX) have no PCI identity: they
// report Apple's PCI vendor ID 0x106B and device 0.
void adapterPciIds(Adapter* a, unsigned& vendor, unsigned& device)
{
    mainThread("IDirect3D9::GetAdapterIdentifier");
    vendor = 0x106B; device = 0;
    io_registry_entry_t e = IOServiceGetMatchingService(MACH_PORT_NULL, IORegistryEntryIDMatching(a->device.registryID));
    for (int depth = 0; e && depth < 8; ++depth) {
        unsigned v = 0, d = 0;
        if (registryWord(e, CFSTR("vendor-id"), v) && v && v != 0xffff) {
            vendor = v & 0xffff;
            if (registryWord(e, CFSTR("device-id"), d)) device = d & 0xffff;
            IOObjectRelease(e);
            return;
        }
        io_registry_entry_t parent = 0;
        const kern_return_t k = IORegistryEntryGetParentEntry(e, kIOServicePlane, &parent);
        IOObjectRelease(e);
        e = k == KERN_SUCCESS ? parent : 0;
    }
    if (e) IOObjectRelease(e);
}
bool supportsSamples(Adapter* a, unsigned samples)
{
    mainThread("IDirect3D9::CheckDeviceMultiSampleType");
    return samples >= 2 && samples <= 16 && [a->device supportsTextureSampleCount:samples];
}
bool supportsBC(Adapter* a) { mainThread("IDirect3D9::CheckDeviceFormat"); return a->device.supportsBCTextureCompression; }
unsigned availableTextureMemory(Frame* f)
{
    mainThread("IDirect3DDevice9::GetAvailableTextureMem");
    const unsigned long long bytes = f->device.recommendedMaxWorkingSetSize;
    return bytes > 0xffff0000ull ? 0xffff0000u : unsigned(bytes & ~0xfffffull); // D3D9 rounds to 1 MB
}

static void endDraws(Frame* f);
// [CHOSEN, not recovered] Binds a pass's colour attachment; the multisampled back buffer is
// drawn into colorMS, which resolves into color once something reads color (resolveBackBuffer):
// present, readbacks, copies. Resolving at the end of every pass cost a full-screen resolve per
// pass. An explicitly created multisampled render target keeps its samples (StretchRect resolves
// it, ResourceBridge.inc).
static void attachColor(Frame* f, MTLRenderPassColorAttachmentDescriptor* a, id<MTLTexture> t)
{
    if (t && t == f->color && f->colorMS) {
        a.texture = f->colorMS; a.storeAction = MTLStoreActionStore;
        f->unresolved = true;
    } else {
        a.texture = t; a.storeAction = MTLStoreActionStore;
    }
}
// The sample count a pass renders a colour attachment with (the back buffer's is its twin's).
static unsigned colorSamples(Frame* f, id<MTLTexture> color)
{
    if (!color) return 0;
    return color == f->color && f->colorMS ? f->samples : unsigned(color.sampleCount);
}
// D3D9 asks a render target and its depth surface to share the multisample type, but the runtime
// accepts a mismatched pair (Wine d3d9 visual test_multisample_mismatch on Windows: clears of
// either buffer work, partial ones too; draws with ZENABLE off render; with the depth test on,
// vendors differ). [CHOSEN, not recovered] Clears clear each buffer in its own pass; draws
// render without the depth surface, as with ZENABLE off. Reported once, never silent.
static bool samplesMismatch(Frame* f, id<MTLTexture> color, id<MTLTexture> depth)
{
    if (!color || !depth || colorSamples(f, color) == unsigned(depth.sampleCount)) return false;
    static bool reported = false;
    if (!reported) {
        reported = true;
        std::fprintf(stderr, "[METAL] render target (%u samples) and depth surface (%u samples) differ: "
            "clears run per buffer, draws ignore the depth surface\n", colorSamples(f, color), unsigned(depth.sampleCount));
        std::fflush(stderr);
    }
    return true;
}
// The raster sample count of a pass over these attachments. Callers drop the depth surface of a
// mismatched pair first (samplesMismatch).
static unsigned targetSamples(Frame* f, id<MTLTexture> color, id<MTLTexture> depth)
{
    if (color && depth && colorSamples(f, color) != unsigned(depth.sampleCount))
        frontier("IDirect3DDevice9 pass: multisampled and single-sample render target/depth surface combined");
    return color ? colorSamples(f, color) : depth ? unsigned(depth.sampleCount) : 1u;
}
static bool commands(Frame* f);
// Brings color up to date with the samples drawn into colorMS since the last resolve.
static bool resolveBackBuffer(Frame* f)
{
    if (!f->colorMS || !f->unresolved) return true;
    if (!commands(f)) return false;
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = f->colorMS; pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    pass.colorAttachments[0].resolveTexture = f->color;
    pass.colorAttachments[0].storeAction = MTLStoreActionStoreAndMultisampleResolve;
    id<MTLRenderCommandEncoder> encoder = [f->commands renderCommandEncoderWithDescriptor:pass];
    if (!encoder) return false;
    [encoder endEncoding];
    f->unresolved = false;
    return true;
}
// Commits a command buffer. When it completes, a failure latches the frame's gpuFailed and its
// serial advances the completed fence.
static void commitTracked(Frame* f, id<MTLCommandBuffer> b)
{
    std::shared_ptr<std::atomic<bool>> failed = f->gpuFailed;
    std::shared_ptr<std::atomic<std::uint64_t>> completed = f->completed;
    const std::uint64_t serial = ++f->committed;
    [b addCompletedHandler:^(id<MTLCommandBuffer> done) {
        if (done.status == MTLCommandBufferStatusError) {
            std::fprintf(stderr, "Metal command failed: %s\n", done.error.localizedDescription.UTF8String);
            std::fflush(stderr);
            failed->store(true);
        }
        std::uint64_t seen = completed->load();
        while (seen < serial && !completed->compare_exchange_weak(seen, serial)) {}
    }];
    [b commit];
}
// Drops the frames in flight the GPU has completed and waits for the oldest while more than `keep`
// remain (traced as GPU waits unless the caller traces them).
static bool retire(Frame* f, std::size_t keep, bool traced = true)
{
    while (!f->inFlight.empty()) {
        id<MTLCommandBuffer> oldest = f->inFlight.front();
        if (oldest.status < MTLCommandBufferStatusCompleted) {
            if (f->inFlight.size() <= keep) break;
            const auto started = std::chrono::steady_clock::now();
            if (traced) { bf2hitch::Scope timed(bf2hitch::GpuWait); [oldest waitUntilCompleted]; }
            else [oldest waitUntilCompleted];
            if (traced) {
                ++f->inFlightWaits;
                f->inFlightWaitMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            }
        }
        f->inFlight.pop_front();
    }
    if (f->gpuFailed->load()) f->failed = true;
    return !f->failed;
}
// [CHOSEN, not recovered] Present submits the frame without waiting for the GPU, as a D3D9 driver
// queues frames, and waits only while kFramesInFlight earlier frames are still executing. What the
// CPU writes is safe meanwhile: DISCARD locks rename the buffer (Resources.inc), other buffer locks
// and readbacks wait for the GPU first (finish), texture uploads and constants go through copies
// the command buffer retains, and the command buffers retain every resource they reference, so a
// D3D9 Release while the GPU still reads it frees nothing early.
static constexpr std::size_t kFramesInFlight = 2;
static bool submit(Frame* f, bool frameEnd)
{
    endDraws(f);
    if (f->failed) return false;
    ++(frameEnd ? f->frameSubmits : f->flushSubmits);
    if (f->commands) {
        commitTracked(f, f->commands); f->inFlight.push_back(f->commands);
#if defined(BF2_METAL_DEVICE_TESTING)
        // [TEMP] [CHOSEN, not recovered] An injected failure latches at the submission it was armed
        // for, as finish does (a real one latches when the GPU reports it).
        if (f->failNextSubmission) {
            f->failNextSubmission = false;
            [f->commands waitUntilCompleted];
            std::fprintf(stderr, "Metal command failed: injected command-buffer failure\n");
            f->gpuFailed->store(true); // as the completion handler of a real failure does
            f->failed = true;
        }
#endif
        f->commands = nil;
        if (frameEnd) f->inFlightAtSubmit += f->inFlight.size();
    }
    return retire(f, kFramesInFlight);
}
// BF2_FX_NATIVE_TRACE / BF2_HITCH_TRACE: the frames-in-flight counts so far, on stderr.
static void framesSummary(Frame* f)
{
    if (!(std::getenv("BF2_FX_NATIVE_TRACE") || bf2hitch::enabled()) || !f->frameSubmits) return;
    std::fprintf(stderr, "[metal] frames in flight: %lu frame submissions (%.2f command buffers in flight after each), "
                 "%lu mid-frame flushes, %lu waits for a frame in flight (%.1f s)\n", f->frameSubmits,
                 double(f->inFlightAtSubmit) / double(f->frameSubmits), f->flushSubmits, f->inFlightWaits, f->inFlightWaitMs / 1000.0);
    if (f->presentsNoWait || f->presentsBusy)
        std::fprintf(stderr, "[metal] DONOTWAIT presents: %lu shown, %lu busy (no free drawable or the previous one still on the GPU)\n",
                     f->presentsNoWait, f->presentsBusy);
}
static bool finish(Frame* f)
{
    endDraws(f);
    bf2hitch::Scope timed(bf2hitch::GpuWait);
    if(f->presentation) {
        [f->presentation waitUntilCompleted];
        if(f->presentation.status==MTLCommandBufferStatusError) f->failed=true;
        f->presentation=nil;
    }
    // The frames in flight were committed earlier: they complete first.
    retire(f, 0, false);
    if (!f->commands) return !f->failed;
    commitTracked(f, f->commands);
    // finish waits for the GPU (readbacks, locks that do not rename, Reset, teardown): it bounds
    // resource lifetime and surfaces command errors to HRESULT callers. Present uses submit.
    [f->commands waitUntilCompleted];
    bool commandFailed = f->commands.status == MTLCommandBufferStatusError;
    const char* error = f->commands.error.localizedDescription.UTF8String;
#if defined(BF2_METAL_DEVICE_TESTING)
    // [TEMP] [CHOSEN, not recovered] Exercise the real error latch after GPU completion.
    if (f->failNextSubmission) {
        f->failNextSubmission = false;
        commandFailed = true;
        error = "injected command-buffer failure";
        f->gpuFailed->store(true); // as the completion handler of a real failure does
    }
#endif
    if (commandFailed) {
        // A GPU failure was reported by the completion handler (commitTracked) already.
        if (f->commands.status != MTLCommandBufferStatusError) { std::fprintf(stderr, "Metal command failed: %s\n", error); std::fflush(stderr); }
        f->failed = true;
    }
    f->commands = nil;
    return !f->failed;
}
static bool commands(Frame* f)
{
    if (f->failed) return false;
    endDraws(f); // Every non-draw encoder closes the open G4 render pass first.
    if (!f->commands) f->commands = [f->queue commandBuffer];
    return f->commands != nil;
}
#include "StateMapping.inc"
#include "ResourceBridge.inc"
#include "ShaderPrebuild.inc"
#include "DrawBridge.inc"
#include "MenuBridge.inc"
bool reset(Frame* f, unsigned width, unsigned height, bool vsync, bool depth, unsigned samples)
{
    mainThread("IDirect3DDevice9::Reset");
    @autoreleasepool {
        // [CHOSEN, not recovered] Drain the old submission even if it failed. Only a
        // successful replacement of the default targets clears the device-loss latch.
        finish(f);
        if (!renderable(f)) return false;
        f->drawableRequests.clear();
        const CGSize current = f->layer.drawableSize;
        if (!width) width = static_cast<unsigned>(current.width);
        if (!height) height = static_cast<unsigned>(current.height);
        if (!width || !height || width > 8192 || height > 8192) return false;
        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width:width height:height mipmapped:NO];
        desc.storageMode = MTLStorageModePrivate;
        desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        id<MTLTexture> color = [f->device newTextureWithDescriptor:desc];
        id<MTLTexture> colorMS = nil;
        if (samples > 1) {
            MTLTextureDescriptor* ms = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                width:width height:height mipmapped:NO];
            ms.textureType = MTLTextureType2DMultisample; ms.sampleCount = samples;
            ms.storageMode = MTLStorageModePrivate; ms.usage = MTLTextureUsageRenderTarget;
            colorMS = [f->device newTextureWithDescriptor:ms];
            if (!colorMS) return false;
        }
        id<MTLTexture> z = nil;
        if (depth) {
            // Native D24S8 on Intel; Apple GPUs use D32Float+S8 storage for the D24S8
            // contract. Clear depth is quantized to 24 normalized bits below.
            desc.pixelFormat = f->device.depth24Stencil8PixelFormatSupported
                ? MTLPixelFormatDepth24Unorm_Stencil8 : MTLPixelFormatDepth32Float_Stencil8;
            desc.usage = MTLTextureUsageRenderTarget;
            if (samples > 1) { desc.textureType = MTLTextureType2DMultisample; desc.sampleCount = samples; }
            z = [f->device newTextureWithDescriptor:desc];
        }
        if (!color || (depth && !z)) return false;
        f->layer.device = f->device;
        f->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        f->layer.framebufferOnly = NO; // Present blits into the drawable.
        f->layer.displaySyncEnabled = vsync;
        f->layer.drawableSize = CGSizeMake(width, height);
        f->color = color;
        f->colorMS = colorMS;
        f->unresolved = false;
        f->samples = samples > 1 ? samples : 1;
        // [TEMP] BF2_HOST_TRACE=1: the default targets each reset builds. [RETIRE-WHEN: macos-options-validated]
        if (const char* trace = std::getenv("BF2_HOST_TRACE"); trace && trace[0] == '1') {
            std::fprintf(stderr, "[TEMP] [METAL] reset %ux%u samples=%u depth=%d\n", width, height, f->samples, depth ? 1 : 0);
            std::fflush(stderr);
        }
        f->depth = z;
        f->colorResource = {color,22};
        f->depthResource = {z,75};
        // The old submissions were drained above (finish): a failure one of them reported to the
        // asynchronous latch belongs to the replaced targets, so the reset device starts clean.
        f->gpuFailed = std::make_shared<std::atomic<bool>>(false);
        f->failed = false;
        return true;
    }
}
Frame* createFrame(Adapter* a, void* view, unsigned width, unsigned height, bool vsync, bool depth, unsigned samples)
{
    mainThread("IDirect3D9::CreateDevice");
    @autoreleasepool {
        NSView* nativeView = (__bridge NSView*)view;
        if (!nativeView || ![nativeView isKindOfClass:[NSView class]] ||
            ![nativeView.layer isKindOfClass:[CAMetalLayer class]]) return nullptr;
        Frame* f = new (std::nothrow) Frame;
        if (!f) return nullptr;
        f->device = a->device;
        f->view = nativeView;
        f->layer = (CAMetalLayer*)nativeView.layer;
        f->queue = [f->device newCommandQueue];
        if (!f->queue || !reset(f, width, height, vsync, depth, samples)) { delete f; return nullptr; }
        return f;
    }
}
void destroyFrame(Frame* f)
{
    mainThread("IDirect3DDevice9::Release");
    @autoreleasepool { finish(f); framesSummary(f); destroyDraws(f); destroyPrebuild(f); delete f; }
}
void dimensions(Frame* f, unsigned& width, unsigned& height)
{
    mainThread("IDirect3DDevice9::GetBackBuffer");
    width = static_cast<unsigned>(f->color.width);
    height = static_cast<unsigned>(f->color.height);
}
bool resized(Frame* f)
{
    mainThread("IDirect3DDevice9::TestCooperativeLevel");
    return f->layer.drawableSize.width != f->color.width || f->layer.drawableSize.height != f->color.height;
}
bool healthy(Frame* f) { mainThread("IDirect3DDevice9::TestCooperativeLevel"); return !f->failed; }
bool renderable(Frame* f)
{
    mainThread("bf2metal::renderable");
    // [CHOSEN, not recovered] Hidden windows remain usable by the GPU harness;
    // miniaturized, detached or zero-sized views/layers cannot recover yet.
    const CGSize size = f->layer.drawableSize;
    const NSSize bounds = f->view.bounds.size;
    return f->view.window && !f->view.window.miniaturized &&
        size.width > 0 && size.height > 0 && bounds.width > 0 && bounds.height > 0;
}
#if defined(BF2_METAL_DEVICE_TESTING)
void armCommandBufferFailure(Frame* f)
{
    mainThread("bf2metal::armCommandBufferFailure");
    f->failNextSubmission = true;
}
#endif
bool clear(Frame* f, unsigned flags, std::uint32_t color, float depth, unsigned stencil)
{
    mainThread("IDirect3DDevice9::Clear");
    @autoreleasepool {
        if (!commands(f)) return false;
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        if (flags & 1) {
            attachColor(f, pass.colorAttachments[0], f->color);
            pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass.colorAttachments[0].clearColor = MTLClearColorMake(((color >> 16) & 255) / 255.0,
                ((color >> 8) & 255) / 255.0, (color & 255) / 255.0, (color >> 24) / 255.0);
        }
        if (flags & 6) {
            pass.depthAttachment.texture = f->depth;
            pass.depthAttachment.loadAction = flags & 2 ? MTLLoadActionClear : MTLLoadActionLoad;
            pass.depthAttachment.storeAction = MTLStoreActionStore;
            if (flags & 2)
                pass.depthAttachment.clearDepth = std::uint32_t(depth * 16777215.0 + 0.5) / 16777215.0;
            pass.stencilAttachment.texture = f->depth;
            pass.stencilAttachment.loadAction = flags & 4 ? MTLLoadActionClear : MTLLoadActionLoad;
            pass.stencilAttachment.storeAction = MTLStoreActionStore;
            pass.stencilAttachment.clearStencil = stencil & 255;
        }
        id<MTLRenderCommandEncoder> encoder = [f->commands renderCommandEncoderWithDescriptor:pass];
        if (!encoder) return false;
        [encoder endEncoding];
        return true;
    }
}
// [TEMP] [CHOSEN, not recovered] Debug capture of the presented back buffer. Set
// BF2_PRESENT_PNG=<directory> (inside out/ under the sandboxed runner) to write
// present_<n>.png every BF2_PRESENT_PNG_EVERY presents (default 60), at most
// BF2_PRESENT_PNG_MAX files (default 10). The copy is blitted into the present's own
// command buffer and written after that buffer completes. Not a renderer feature.
// [TEMP] BF2_INPUT_SCRIPT "capture <label>" requests (Platform/macOS/MacInputScript.mm). This
// weak default answers no request in images without the script.
extern "C" __attribute__((weak)) bool openbf2InputScriptTakeCapture(char*, unsigned long) { return false; }
namespace {
struct PresentCapture { id<MTLBuffer> buffer; unsigned width=0, height=0, index=0; char label[64]={}; };
PresentCapture* presentCaptureBegin(Frame* f)
{
    static const char* dir=std::getenv("BF2_PRESENT_PNG");
    if(!dir || !*dir) return nullptr;
    static unsigned presents=0, written=0;
    static const unsigned every=[]{const char* e=std::getenv("BF2_PRESENT_PNG_EVERY"); int v=e?std::atoi(e):60; return unsigned(v>0?v:60);}();
    static const unsigned limit=[]{const char* e=std::getenv("BF2_PRESENT_PNG_MAX"); int v=e?std::atoi(e):10; return unsigned(v>0?v:10);}();
    const unsigned n=++presents;
    char label[64]={};
    const bool requested=openbf2InputScriptTakeCapture(label,sizeof(label));
    if(!requested && (written>=limit || (n!=1 && n%every))) return nullptr;
    const unsigned w=unsigned(f->color.width), h=unsigned(f->color.height);
    id<MTLBuffer> buffer=[f->device newBufferWithLength:NSUInteger(w)*h*4 options:MTLResourceStorageModeShared];
    id<MTLBlitCommandEncoder> blit=buffer?[f->commands blitCommandEncoder]:nil;
    if(!blit) return nullptr;
    [blit copyFromTexture:f->color sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(w,h,1) toBuffer:buffer destinationOffset:0
        destinationBytesPerRow:NSUInteger(w)*4 destinationBytesPerImage:NSUInteger(w)*h*4];
    [blit endEncoding];
    if(!requested) ++written;
    auto* c=new PresentCapture; c->buffer=buffer; c->width=w; c->height=h; c->index=n;
    std::memcpy(c->label,label,sizeof(label));
    return c;
}
void presentCaptureWrite(PresentCapture* c, id<MTLCommandBuffer> done)
{
    if(!c) return;
    // Owned before the trace scope, so the scope (which reads the label) ends first.
    const std::unique_ptr<PresentCapture> owned(c);
    bf2hitch::Scope timed(bf2hitch::Capture, c->label);
    if(done) [done waitUntilCompleted];
    const char* dir=std::getenv("BF2_PRESENT_PNG");
    const unsigned char* bgra=static_cast<const unsigned char*>(c->buffer.contents);
    NSBitmapImageRep* rep=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr pixelsWide:c->width
        pixelsHigh:c->height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:NSInteger(c->width)*4 bitsPerPixel:32];
    unsigned char* rgba=rep.bitmapData;
    for(size_t i=0, n=size_t(c->width)*c->height; i<n; ++i) {
        rgba[4*i]=bgra[4*i+2]; rgba[4*i+1]=bgra[4*i+1]; rgba[4*i+2]=bgra[4*i]; rgba[4*i+3]=255;
    }
    NSData* png=[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    char path[1024];
    if(c->label[0]) std::snprintf(path,sizeof(path),"%s/present_%05u_%s.png",dir,c->index,c->label);
    else std::snprintf(path,sizeof(path),"%s/present_%05u.png",dir,c->index);
    const bool ok=png && [png writeToFile:[NSString stringWithUTF8String:path] atomically:YES];
    std::fprintf(stderr,"[TEMP] BF2_PRESENT_PNG: %s %s (%ux%u)\n",ok?"wrote":"FAILED to write",path,c->width,c->height);
}
}
PresentResult present(Frame* f,void* window,bool doNotWait)
{
    mainThread("IDirect3DDevice9::Present");
    @autoreleasepool {
        NSView* view=window?(__bridge NSView*)window:f->view;
        if(![view isKindOfClass:[NSView class]] || ![view.layer isKindOfClass:[CAMetalLayer class]])
            return PresentResult::invalidWindow;
        CAMetalLayer* layer=(CAMetalLayer*)view.layer;
        // Device-loss latch (g3d): a miniaturized/detached/zero-sized view or a failed
        // submission is lost until a successful Reset replaces the default targets.
        if(!renderable(f) || resized(f) || f->failed) return PresentResult::lost;
        // [CHOSEN, not recovered] https://github.com/kr4uzi/OpenBF2/issues/41
        // A DONOTWAIT frame is not shown before the previous one has reached
        // the display: a level load repaints far faster than the display refreshes, with or without
        // vsync, and each repaint the display cannot show still cost a full loading-screen draw. The
        // busy answer makes the renderer skip that draw (RendDX9Draw rendererPresent). After 100 ms
        // without the presented handler (an occluded window), the next frame goes out regardless.
        if(doNotWait && !f->noWaitOnScreen->load() &&
           std::chrono::steady_clock::now()-f->noWaitPresentedAt<std::chrono::milliseconds(100)) {
            ++f->presentsBusy; return PresentResult::busy;
        }
        if(f->presentation) {
            const auto status=f->presentation.status;
            if(doNotWait && status<MTLCommandBufferStatusCompleted) { ++f->presentsBusy; return PresentResult::busy; }
            if(!doNotWait) [f->presentation waitUntilCompleted];
            if(f->presentation.status==MTLCommandBufferStatusError) { f->failed=true; return PresentResult::lost; }
            f->presentation=nil;
        }
        layer.device=f->device; layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly=NO; layer.displaySyncEnabled=f->layer.displaySyncEnabled;
        if(layer.drawableSize.width<=0 || layer.drawableSize.height<=0) return PresentResult::lost;
        auto& request=f->drawableRequests[(__bridge void*)layer];
        if(!request) request=std::make_shared<DrawableRequest>();
        // nextDrawable can block on WindowServer/vsync. Only the worker waits; a DONOTWAIT caller
        // gets WASSTILLDRAWING and retains its pending commands. Call with request->mutex held.
        const auto acquireAsync=[layer](const std::shared_ptr<DrawableRequest>& pending) {
            pending->acquiring=true;
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE,0),^{
                @autoreleasepool {
                    id<CAMetalDrawable> next=[layer nextDrawable];
                    std::lock_guard<std::mutex> guard(pending->mutex);
                    pending->drawable=next; pending->acquiring=false;
                    pending->condition.notify_all();
                }
            });
        };
        id<CAMetalDrawable> drawable=nil;
        {
            std::unique_lock<std::mutex> lock(request->mutex);
            if(!doNotWait) request->condition.wait(lock,[&]{return !request->acquiring;});
            drawable=request->drawable; request->drawable=nil;
            if(!drawable && !request->acquiring) {
                bf2hitch::Scope timed(bf2hitch::Drawable);
                if(!doNotWait) drawable=[layer nextDrawable];
                else acquireAsync(request);
            }
        }
        if(!drawable) {
            if(doNotWait) ++f->presentsBusy;
            return doNotWait?PresentResult::busy:PresentResult::lost;
        }
        if(!commands(f) || !resolveBackBuffer(f)) return PresentResult::lost;
        if(drawable.texture.width==f->color.width && drawable.texture.height==f->color.height) {
            id<MTLBlitCommandEncoder> blit=[f->commands blitCommandEncoder];
            if(!blit) return PresentResult::lost;
            [blit copyFromTexture:f->color sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                sourceSize:MTLSizeMake(f->color.width,f->color.height,1) toTexture:drawable.texture
                destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
            [blit endEncoding];
        } else {
            auto pipeline=menuPipeline(f,drawable.texture.pixelFormat,MTLPixelFormatInvalid,1,true);
            auto* pass=[MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture=drawable.texture;
            pass.colorAttachments[0].loadAction=MTLLoadActionDontCare; pass.colorAttachments[0].storeAction=MTLStoreActionStore;
            auto e=[f->commands renderCommandEncoderWithDescriptor:pass];
            if(!e) return PresentResult::lost;
            const float z=0;
            [e setRenderPipelineState:pipeline]; [e setVertexBytes:&z length:sizeof(z) atIndex:0];
            [e setViewport:(MTLViewport){0,0,double(drawable.texture.width),double(drawable.texture.height),0,1}];
            [e setFragmentTexture:f->color atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4]; [e endEncoding];
        }
        if(f->prebuild) f->prebuild->presents=f->presents+1;
        if(++f->presents%1200==0) { prebuildSummary(f); framesSummary(f); } // traced runs only
        PresentCapture* capture=presentCaptureBegin(f);  // [TEMP] BF2_PRESENT_PNG
        if(doNotWait) {
            auto onScreen=std::make_shared<std::atomic<bool>>(false);
            f->noWaitOnScreen=onScreen; f->noWaitPresentedAt=std::chrono::steady_clock::now();
            [drawable addPresentedHandler:^(id<MTLDrawable>) { onScreen->store(true); }];
        } else f->noWaitOnScreen->store(true);
        [f->commands presentDrawable:drawable];
        if(doNotWait) {
            commitTracked(f,f->commands); f->presentation=f->commands; f->commands=nil;
            // [CHOSEN, not recovered] https://github.com/kr4uzi/OpenBF2/issues/41
            // Ask for the next drawable now, so the next DONOTWAIT present
            // (a level load's next repaint, RendDX9Draw rendererPresent) finds it ready when the
            // layer has one free instead of answering busy once per frame.
            {
                std::lock_guard<std::mutex> lock(request->mutex);
                if(!request->drawable && !request->acquiring) acquireAsync(request);
            }
            ++f->presentsNoWait;
            presentCaptureWrite(capture,f->presentation);
            return PresentResult::ok;
        }
        id<MTLCommandBuffer> submitted=f->commands;
        const bool ok=submit(f,true);
        presentCaptureWrite(capture,submitted);
        return ok?PresentResult::ok:PresentResult::lost;
    }
}
}
