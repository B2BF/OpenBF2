// [CHOSEN, not recovered] G4 Metal draw encoding: programs, pipeline/depth/sampler caches and
// one render pass kept open across consecutive draws to the same attachments.
// Included by MetalBridge.mm after ResourceBridge.inc.

struct Program {
    ProgramDesc desc;                      // without its MSL, which the library entry holds
    std::uint64_t serial = 0;
    std::shared_ptr<LibraryEntry> library; // shared by every program with the same MSL (ShaderPrebuild.inc)
    id<MTLFunction> function;
    bool compiled = false;
    std::string error;
};
// Visibility results for D3D9 occlusion queries: one zeroed buffer of slots per command buffer,
// attached to the render passes that contain query draws. Metal refuses to count into one offset
// twice in an encoder, so every run of consecutive draws of a query gets its own slot; a query
// sums its slots once the command buffers holding them have completed.
struct VisibilityBatch {
    id<MTLBuffer> buffer;
    id<MTLCommandBuffer> commands;
    unsigned used = 0;
    static constexpr unsigned capacity = 256;
};
struct Occlusion {
    Frame* frame = nullptr;
    std::vector<std::pair<std::shared_ptr<VisibilityBatch>, unsigned>> slots; // batch, byte offset
};
struct DrawCaches {
    id<MTLRenderCommandEncoder> encoder;
    std::shared_ptr<VisibilityBatch> visibility; // attached to the open pass, or null
    Occlusion* counting = nullptr; // the query the open pass is counting into, if any
    id<MTLTexture> color, depth;
    unsigned colorLevel = 0, colorSlice = 0, depthLevel = 0, depthSlice = 0;
    std::map<std::string, id<MTLLibrary>> libraries;
    std::map<std::string, id<MTLRenderPipelineState>> pipelines;
    std::map<std::string, id<MTLDepthStencilState>> depthStates;
    std::map<std::string, id<MTLSamplerState>> samplers;
    id<MTLTexture> unbound[3];
};
static std::uint64_t programSerial = 0;

static void endDraws(Frame* f)
{
    if (f->draws && f->draws->encoder) { [f->draws->encoder endEncoding]; f->draws->encoder = nil; f->draws->visibility.reset(); f->draws->counting = nullptr; }
}
static void destroyDraws(Frame* f) { endDraws(f); delete f->draws; f->draws = nullptr; }

Program* createProgram(Frame* f, const ProgramDesc& desc)
{
    mainThread("IDirect3DDevice9::CreateShader");
    auto* p = new Program;
    p->desc = desc; p->serial = ++programSerial;
    p->library = internLibrary(f, desc);
    p->desc.msl.clear(); p->desc.msl.shrink_to_fit();
    return p;
}
void destroyProgram(Program* p) { mainThread("IDirect3DShader9::Release"); releaseLibrary(p->library); delete p; }
bool backgroundBuilds(Frame* f) { mainThread("IDirect3DDevice9::CreatePixelShader"); return prebuild(f)->background; }

template<class T> static void keyAppend(std::string& k, const T& v) { k.append(reinterpret_cast<const char*>(&v), sizeof(v)); }

// The program's entry function for a draw: its library is ready (compiled in the background), or
// is compiled now, or another thread is compiling it and the draw waits.
static bool compileProgram(Frame* f, Program* p)
{
    if (p->compiled) return p->error.empty();
    p->compiled = true;
    Prebuild* pb = prebuild(f);
    LibraryEntry& e = *p->library;
    std::unique_lock<std::mutex> lock(pb->mutex);
    if (e.state == BuildState::Queued || e.state == BuildState::Building) {
        const bool queued = e.state == BuildState::Queued;
        const std::string traced = bf2hitch::enabled() ? std::string(queued ? "" : "wait ") + (p->desc.vertex ? "vs " : "ps ") +
            std::to_string(e.msl.size()) + " " + drawTraceLabel() : std::string();
        bf2hitch::Scope timed(bf2hitch::MetalLibrary, traced.c_str());
        ++(queued ? pb->libraryNow : pb->libraryWaited);
        e.used = true;
        settleLibrary(pb, lock, e, true);
    } else if (!e.used) {
        e.used = true; ++pb->libraryReady;
        bf2hitch::add(bf2hitch::MetalLibraryReady, 0);
    }
    if (e.state != BuildState::Ready) { p->error = e.error; return false; }
    p->function = e.function;
    return true;
}
// D3D9 samples an unbound stage as opaque black; Metal requires a texture of the shader's type.
static id<MTLTexture> unboundTexture(Frame* f, unsigned kind)
{
    auto& t = f->draws->unbound[kind];
    if (t) return t;
    MTLTextureDescriptor* d = [MTLTextureDescriptor new];
    d.textureType = kind == 1 ? MTLTextureTypeCube : kind == 2 ? MTLTextureType3D : MTLTextureType2D;
    d.pixelFormat = MTLPixelFormatRGBA8Unorm; d.width = d.height = d.depth = 1;
    d.storageMode = MTLStorageModeManaged; d.usage = MTLTextureUsageShaderRead;
    t = [f->device newTextureWithDescriptor:d];
    const std::uint8_t black[4] = {0, 0, 0, 255};
    for (unsigned s = 0; s < (kind == 1 ? 6u : 1u); ++s)
        [t replaceRegion:MTLRegionMake3D(0, 0, 0, 1, 1, 1) mipmapLevel:0 slice:s withBytes:black bytesPerRow:4 bytesPerImage:4];
    return t;
}
// Whether the program's library is compiled (does not wait).
static bool libraryReady(Frame* f, Program* p)
{
    Prebuild* pb = prebuild(f);
    std::lock_guard<std::mutex> lock(pb->mutex);
    return p->library->state == BuildState::Ready;
}
// The pipeline state of a draw. The per-device cache is keyed by the draw's own objects; a miss
// describes the pipeline as a recipe (ShaderPrebuild.inc), which finds the state an equal pair of
// programs already built, or builds it now. With `defer` a state that is not built is left to a
// worker instead: nil with `skipped` set, and the draw is skipped.
static id<MTLRenderPipelineState> pipeline(Frame* f, const DrawCall& c, std::string& error, bool defer, bool& skipped)
{
    const MTLPixelFormat color = c.color->object.pixelFormat;
    const MTLPixelFormat depth = c.depth ? c.depth->object.pixelFormat : MTLPixelFormatInvalid;
    const auto& s = c.state;
    std::string key;
    keyAppend(key, c.vertex->serial); keyAppend(key, c.fragment ? c.fragment->serial : 0);
    const unsigned samples = targetSamples(f, c.color->object, c.depth ? c.depth->object : nil);
    keyAppend(key, color); keyAppend(key, depth); keyAppend(key, samples);
    const bool writes = c.fragment && s.colorMask;
    keyAppend(key, writes); keyAppend(key, s.colorMask); keyAppend(key, s.blend);
    if (s.blend) for (unsigned v : {s.srcRGB, s.dstRGB, s.srcAlpha, s.dstAlpha, s.blendRGB, s.blendAlpha}) keyAppend(key, v);
    for (const auto& e : c.layout->elements) keyAppend(key, e);
    for (const auto& st : c.streams) keyAppend(key, st.stride);
    auto& cached = f->draws->pipelines[key];
    if (cached) return cached;
    PipelineRecipe r;
    r.vs = c.vertex->library->hash;
    if (writes) r.fs = c.fragment->library->hash;
    r.color = unsigned(color); r.depth = unsigned(depth); r.samples = samples;
    r.writeMask = writes ? s.colorMask : 0;
    r.blend = writes && s.blend;
    if (r.blend) {
        const unsigned factors[6] = {s.srcRGB, s.dstRGB, s.srcAlpha, s.dstAlpha, s.blendRGB, s.blendAlpha};
        std::memcpy(r.factors, factors, sizeof factors);
    }
    for (const auto& in : c.vertex->desc.inputs) {
        const VertexElement* match = nullptr;
        for (const auto& e : c.layout->elements) if (e.usage == in.usage && e.index == in.index) { match = &e; break; }
        if (!match) {
            error = "vertex shader input usage " + std::to_string(in.usage) + " index " + std::to_string(in.index) +
                " is not in the vertex declaration";
            f->draws->pipelines.erase(key); return nil;
        }
        const unsigned stride = c.streams[match->stream].stride;
        if (!stride || stride % 4) {
            error = "stream " + std::to_string(match->stream) + " stride " + std::to_string(stride) + " is not a positive multiple of 4";
            f->draws->pipelines.erase(key); return nil;
        }
        r.attributes.push_back({in.reg, unsigned(vertexFormat(match->type)), match->offset, match->stream});
        bool listed = false;
        for (const auto& st : r.strides) listed = listed || st.first == match->stream;
        if (!listed) r.strides.push_back({match->stream, stride});
    }
    Prebuild* pb = prebuild(f);
    const std::string text = r.text();
    auto& slot = pb->pipelines[text];
    if (!slot) {
        slot = std::make_shared<PipelineEntry>();
        slot->recipe = r; slot->vs = c.vertex->library;
        if (writes) slot->fs = c.fragment->library;
    }
    const std::shared_ptr<PipelineEntry> e = slot;
    std::unique_lock<std::mutex> lock(pb->mutex);
    if (e->state == BuildState::Ready) {
        ++pb->pipelineShared;
        bf2hitch::add(bf2hitch::MetalPipelineReady, 0);
    } else if (defer && e->state != BuildState::Failed) {
        if (e->state == BuildState::Queued && !e->queued) {
            e->queued = true; pb->jobs.push_back(e); pb->work.notify_all(); startWorkers(pb);
        }
        if (!e->firstSkip) e->firstSkip = f->presents + 1;
        ++pb->skippedDraws; skipped = true;
        f->draws->pipelines.erase(key); return nil;
    } else if (e->state != BuildState::Failed) {
        const bool queued = e->state == BuildState::Queued;
        const std::string traced = bf2hitch::enabled() ? std::string(queued ? "" : "wait ") + drawTraceLabel() : std::string();
        bf2hitch::Scope timed(bf2hitch::MetalPipeline, traced.c_str());
        ++(queued ? pb->pipelineNow : pb->pipelineWaited);
        settlePipeline(pb, lock, *e, true);
    }
    if (e->state != BuildState::Ready) {
        error = e->error;
        lock.unlock();
        pb->pipelines.erase(text); f->draws->pipelines.erase(key); return nil;
    }
    // Frames the skipped draw was missing: from its first skip through the frame in which the worker
    // finished the state (it is drawn from the next one on, when the game draws it again).
    if (e->firstSkip) {
        const unsigned long built = e->readyFrame ? e->readyFrame + 1 : f->presents + 1;
        pb->framesMissing.push_back(built >= e->firstSkip ? built - e->firstSkip + 1 : 1);
        e->firstSkip = 0;
    }
    cached = e->object;
    return cached;
}
static id<MTLDepthStencilState> depthState(Frame* f, const DrawCall& c)
{
    const auto& s = c.state;
    const MTLPixelFormat depth = c.depth ? c.depth->object.pixelFormat : MTLPixelFormatInvalid;
    const bool hasDepth = depth != MTLPixelFormatInvalid, stencil = s.stencil && depthHasStencil(depth);
    std::string key;
    const unsigned compare = hasDepth ? s.depthCompare : unsigned(MTLCompareFunctionAlways);
    const bool write = hasDepth && s.depthWrite;
    keyAppend(key, compare); keyAppend(key, write); keyAppend(key, stencil);
    if (stencil) { keyAppend(key, s.front); keyAppend(key, s.back); keyAppend(key, s.stencilReadMask); keyAppend(key, s.stencilWriteMask); }
    auto& cached = f->draws->depthStates[key];
    if (cached) return cached;
    bf2hitch::Scope timed(bf2hitch::MetalState, "depth-stencil");
    MTLDepthStencilDescriptor* d = [MTLDepthStencilDescriptor new];
    d.depthCompareFunction = MTLCompareFunction(compare); d.depthWriteEnabled = write;
    if (stencil) {
        auto face = [&](const StencilMapping& m) {
            MTLStencilDescriptor* x = [MTLStencilDescriptor new];
            x.stencilFailureOperation = MTLStencilOperation(m.fail); x.depthFailureOperation = MTLStencilOperation(m.depthFail);
            x.depthStencilPassOperation = MTLStencilOperation(m.pass); x.stencilCompareFunction = MTLCompareFunction(m.compare);
            x.readMask = s.stencilReadMask; x.writeMask = s.stencilWriteMask; return x;
        };
        // D3D's clockwise faces are front faces (the encoder uses clockwise front winding).
        d.frontFaceStencil = face(s.front); d.backFaceStencil = face(s.back);
    }
    cached = [f->device newDepthStencilStateWithDescriptor:d];
    return cached;
}
static id<MTLSamplerState> samplerState(Frame* f, const SamplerMapping& m)
{
    std::string key; keyAppend(key, m.address); keyAppend(key, m.minFilter); keyAppend(key, m.magFilter);
    keyAppend(key, m.mipFilter); keyAppend(key, m.anisotropy); keyAppend(key, m.border); keyAppend(key, m.minLod);
    auto& cached = f->draws->samplers[key];
    if (cached) return cached;
    bf2hitch::Scope timed(bf2hitch::MetalState, "sampler");
    MTLSamplerDescriptor* d = [MTLSamplerDescriptor new];
    d.sAddressMode = MTLSamplerAddressMode(m.address[0]); d.tAddressMode = MTLSamplerAddressMode(m.address[1]);
    d.rAddressMode = MTLSamplerAddressMode(m.address[2]);
    d.minFilter = MTLSamplerMinMagFilter(m.minFilter); d.magFilter = MTLSamplerMinMagFilter(m.magFilter);
    d.mipFilter = MTLSamplerMipFilter(m.mipFilter); d.maxAnisotropy = std::max(1u, m.anisotropy);
    d.borderColor = MTLSamplerBorderColor(m.border);
    d.lodMinClamp = m.minLod; // D3DSAMP_MAXMIPLEVEL: the most detailed level that may be sampled
    cached = [f->device newSamplerStateWithDescriptor:d];
    return cached;
}
static id<MTLRenderCommandEncoder> passFor(Frame* f, const DrawCall& c)
{
    auto& d = *f->draws;
    id<MTLTexture> color = c.color->object, depth = c.depth ? c.depth->object : nil;
    // A query draw needs a pass with a visibility buffer and a free slot (or its slot in this pass).
    const bool counted = !c.occlusion || (d.visibility &&
        (d.counting == c.occlusion || d.visibility->used < VisibilityBatch::capacity));
    if (d.encoder && counted && d.color == color && d.depth == depth && d.colorLevel == c.colorLevel && d.colorSlice == c.colorSlice &&
        d.depthLevel == c.depthLevel && d.depthSlice == c.depthSlice) return d.encoder;
    endDraws(f);
    if (f->failed) return nil;
    if (!f->commands) f->commands = [f->queue commandBuffer];
    if (!f->commands) return nil;
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    if (c.occlusion) {
        auto& batch = f->draws->visibility;
        batch = std::make_shared<VisibilityBatch>();
        batch->buffer = [f->device newBufferWithLength:VisibilityBatch::capacity * 8 options:MTLResourceStorageModeShared];
        if (!batch->buffer) return nil;
        std::memset(batch->buffer.contents, 0, VisibilityBatch::capacity * 8);
        batch->commands = f->commands;
        pass.visibilityResultBuffer = batch->buffer;
    }
    auto* a = pass.colorAttachments[0];
    (void) targetSamples(f, color, depth);
    attachColor(f, a, color); a.level = c.colorLevel; a.slice = c.colorSlice;
    a.loadAction = MTLLoadActionLoad;
    if (depth) {
        pass.depthAttachment.texture = depth; pass.depthAttachment.level = c.depthLevel; pass.depthAttachment.slice = c.depthSlice;
        pass.depthAttachment.loadAction = MTLLoadActionLoad; pass.depthAttachment.storeAction = MTLStoreActionStore;
        if (depthHasStencil(depth.pixelFormat)) {
            pass.stencilAttachment.texture = depth; pass.stencilAttachment.level = c.depthLevel; pass.stencilAttachment.slice = c.depthSlice;
            pass.stencilAttachment.loadAction = MTLLoadActionLoad; pass.stencilAttachment.storeAction = MTLStoreActionStore;
        }
    }
    d.encoder = [f->commands renderCommandEncoderWithDescriptor:pass];
    d.color = color; d.depth = depth; d.colorLevel = c.colorLevel; d.colorSlice = c.colorSlice;
    d.depthLevel = c.depthLevel; d.depthSlice = c.depthSlice;
    return d.encoder;
}
static void bindBytes(id<MTLRenderCommandEncoder> e, Frame* f, bool vertex, const void* bytes, std::size_t n, unsigned index)
{
    if (!n) return;
    if (n <= 4096) {
        if (vertex) [e setVertexBytes:bytes length:n atIndex:index]; else [e setFragmentBytes:bytes length:n atIndex:index];
        return;
    }
    id<MTLBuffer> b = [f->device newBufferWithBytes:bytes length:n options:MTLResourceStorageModeShared];
    if (vertex) [e setVertexBuffer:b offset:0 atIndex:index]; else [e setFragmentBuffer:b offset:0 atIndex:index];
}
// Windows samples one/two-channel formats with the missing channels as one: R16F/R32F as
// (r,1,1,1), G16R16/G16R16F/G32R32F as (r,g,1,1). Metal's own defaults are zero, so these are
// sampled through a swizzled view (created per draw; the formats are rare render targets).
static id<MTLTexture> sampled(Texture* t)
{
    MTLTextureSwizzleChannels channels;
    switch (t->format) {
    case 111: case 114:
        channels = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleOne, MTLTextureSwizzleOne, MTLTextureSwizzleOne); break;
    case 34: case 112: case 115:
        channels = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleOne, MTLTextureSwizzleOne); break;
    default: return t->object;
    }
    id<MTLTexture> o = t->object;
    const NSUInteger slices = o.textureType == MTLTextureTypeCube ? 6 : o.arrayLength;
    return [o newTextureViewWithPixelFormat:o.pixelFormat textureType:o.textureType levels:NSMakeRange(0, o.mipmapLevelCount)
        slices:NSMakeRange(0, slices) swizzle:channels];
}
static bool bindTextures(Frame* f, id<MTLRenderCommandEncoder> e, const DrawCall& c, const Program* p, std::string& error)
{
    for (const auto& s : p->desc.samplers) {
        const unsigned index = p->desc.vertex ? 17 + s.slot : s.slot;
        if (index >= 21 || (p->desc.vertex && s.slot >= 4)) { error = "sampler slot out of range"; return false; }
        id<MTLTexture> t = c.textures[index] ? sampled(c.textures[index]) : unboundTexture(f, s.kind);
        const MTLTextureType want = s.kind == 1 ? MTLTextureTypeCube : s.kind == 2 ? MTLTextureType3D : MTLTextureType2D;
        if (t.textureType != want) {
            auto name = [](MTLTextureType x) { return x == MTLTextureTypeCube ? "cube" : x == MTLTextureType3D ? "volume" : "2D"; };
            error = std::string(p->desc.vertex ? "vertex" : "pixel") + " sampler " + std::to_string(s.slot) +
                " texture dimension differs from the shader's (shader " + name(want) + ", bound " + name(t.textureType) + ")";
            return false;
        }
        id<MTLSamplerState> ss = samplerState(f, c.state.sampler[index]);
        if (p->desc.vertex) { [e setVertexTexture:t atIndex:s.slot]; [e setVertexSamplerState:ss atIndex:s.slot]; }
        else { [e setFragmentTexture:t atIndex:s.slot]; [e setFragmentSamplerState:ss atIndex:s.slot]; }
    }
    return true;
}
bool draw(Frame* f, const DrawCall& call, std::string& error)
{
    mainThread("IDirect3DDevice9::Draw");
    @autoreleasepool {
    if (!f->draws) f->draws = new DrawCaches;
    // A depth surface whose sample count differs from the render target's is not bound
    // (samplesMismatch, MetalBridge.mm).
    DrawCall unmatched;
    const bool drop = call.depth && samplesMismatch(f, call.color->object, call.depth->object);
    if (drop) { unmatched = call; unmatched.depth = nullptr; }
    const DrawCall& c = drop ? unmatched : call;
    // [CHOSEN, not recovered] A colour-only blended draw (blending on, no depth or stencil writes, no
    // occlusion query: particles, muzzle flashes, glows) whose programs or pipeline state are not
    // built yet is skipped while a worker builds them (ShaderPrebuild.inc), instead of stalling the
    // frame; it is drawn from the first frame after that. Nothing later in the frame depends on
    // what it would have written but the colour it blends into. Every other draw waits as before.
    const auto& st = c.state;
    const bool deferrable = c.fragment && st.colorMask && st.blend && !st.depthWrite && !st.stencil && !c.occlusion &&
                            prebuild(f)->background;
    const bool ready = !deferrable || (libraryReady(f, c.vertex) && libraryReady(f, c.fragment));
    if (ready) {
        if (!compileProgram(f, c.vertex)) { error = "vertex shader " + c.vertex->error; return false; }
        // Without colour writes the pipeline has no fragment function (pipeline()), so none is compiled.
        if (c.fragment && c.state.colorMask && !compileProgram(f, c.fragment)) { error = "pixel shader " + c.fragment->error; return false; }
    }
    bool skipped = false;
    id<MTLRenderPipelineState> state = pipeline(f, c, error, deferrable, skipped);
    if (skipped) return true;
    if (!state) return false;
    id<MTLRenderCommandEncoder> e = passFor(f, c);
    if (!e) { error = "render pass"; return false; }
    auto& d = *f->draws;
    if (c.occlusion && d.counting != c.occlusion) {
        c.occlusion->slots.push_back({d.visibility, d.visibility->used++ * 8});
        [e setVisibilityResultMode:MTLVisibilityResultModeCounting offset:c.occlusion->slots.back().second];
        d.counting = c.occlusion;
    } else if (!c.occlusion && d.counting) {
        [e setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
        d.counting = nullptr;
    }
    const auto& s = c.state;
    [e setRenderPipelineState:state];
    [e setDepthStencilState:depthState(f, c)];
    [e setFrontFacingWinding:MTLWindingClockwise];
    [e setCullMode:MTLCullMode(s.cull)];
    [e setTriangleFillMode:MTLTriangleFillMode(s.fill)];
    [e setViewport:(MTLViewport){c.viewport[0], c.viewport[1], c.viewport[2], c.viewport[3], c.viewport[4], c.viewport[5]}];
    [e setScissorRect:(MTLScissorRect){c.scissor[0], c.scissor[1], c.scissor[2], c.scissor[3]}];
    [e setDepthBias:s.depthBias * c.depthBiasUnits slopeScale:s.slopeBias clamp:0];
    [e setStencilReferenceValue:s.stencilRef];
    [e setBlendColorRed:s.blendColor[0] green:s.blendColor[1] blue:s.blendColor[2] alpha:s.blendColor[3]];
    for (unsigned i = 0; i < 16; ++i) {
        const auto& st = c.streams[i];
        if (st.buffer) {
            if (st.offset % 4) { error = "stream offset is not 4-byte aligned"; return false; }
            [e setVertexBuffer:st.buffer->object offset:st.offset atIndex:i + 1];
        } else if (st.bytes) bindBytes(e, f, true, st.bytes, st.length, i + 1);
    }
    bindBytes(e, f, true, c.vertexConstants.data(), c.vertexConstants.size(), 0);
    [e setVertexBytes:c.vertexFixup length:sizeof(c.vertexFixup) atIndex:20];
    if (!bindTextures(f, e, c, c.vertex, error)) return false;
    if (c.fragment && s.colorMask) {
        bindBytes(e, f, false, c.fragmentConstants.data(), c.fragmentConstants.size(), 0);
        [e setFragmentBytes:c.fragmentAlpha length:sizeof(c.fragmentAlpha) atIndex:1];
        [e setFragmentBytes:c.fragmentFactor length:sizeof(c.fragmentFactor) atIndex:2];
        [e setFragmentBytes:c.fragmentFog length:sizeof(c.fragmentFog) atIndex:3];
        if (!bindTextures(f, e, c, c.fragment, error)) return false;
    }
    static const MTLPrimitiveType types[] = {MTLPrimitiveTypePoint, MTLPrimitiveTypePoint, MTLPrimitiveTypeLine,
        MTLPrimitiveTypeLineStrip, MTLPrimitiveTypeTriangle, MTLPrimitiveTypeTriangleStrip};
    if (c.primitive < 1 || c.primitive > 5) { error = "primitive type"; return false; }
    const MTLPrimitiveType type = types[c.primitive];
    if (!c.indexSize) {
        [e drawPrimitives:type vertexStart:c.first vertexCount:c.count];
        return true;
    }
    id<MTLBuffer> indices = c.indexBuffer ? c.indexBuffer->object : nil;
    NSUInteger offset = c.indexOffset;
    if (!indices || offset % 4) {
        // Metal index offsets are 4-byte aligned; copy unaligned or client-memory ranges.
        const auto* base = c.indexBuffer ? static_cast<const std::uint8_t*>(c.indexBuffer->object.contents) + offset
                                         : static_cast<const std::uint8_t*>(c.indexBytes);
        indices = [f->device newBufferWithBytes:base length:std::size_t(c.count) * c.indexSize options:MTLResourceStorageModeShared];
        offset = 0;
    }
    [e drawIndexedPrimitives:type indexCount:c.count indexType:c.indexSize == 2 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
        indexBuffer:indices indexBufferOffset:offset instanceCount:1 baseVertex:c.baseVertex baseInstance:0];
    return true;
    }
}

Occlusion* createOcclusion(Frame* f) { mainThread("IDirect3DDevice9::CreateQuery"); auto* o = new Occlusion; o->frame = f; return o; }
// The open pass must not keep counting into a query that restarts or dies.
static void stopCounting(Occlusion* o)
{
    auto* d = o->frame->draws;
    if (!d || d->counting != o) return;
    if (d->encoder) [d->encoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
    d->counting = nullptr;
}
void destroyOcclusion(Occlusion* o) { mainThread("IDirect3DQuery9::Release"); stopCounting(o); delete o; }
void occlusionBegin(Occlusion* o) { stopCounting(o); o->slots.clear(); }
int occlusionResult(Frame* f, Occlusion* o, bool flush, std::uint64_t& samples)
{
    mainThread("IDirect3DQuery9::GetData");
    @autoreleasepool {
    for (const auto& s : o->slots) {
        if (s.first->commands.status == MTLCommandBufferStatusNotEnqueued) {
            // Still in the open command buffer: D3DGETDATA_FLUSH submits it (G3 submissions are
            // serialized, so the submission also completes); without FLUSH it stays pending.
            if (!flush) return 0;
            if (!finish(f)) return -1;
        }
        const auto status = s.first->commands.status;
        if (status == MTLCommandBufferStatusError) return -1;
        if (status != MTLCommandBufferStatusCompleted) return 0;
    }
    samples = 0;
    for (const auto& s : o->slots) {
        std::uint64_t v; std::memcpy(&v, static_cast<const std::uint8_t*>(s.first->buffer.contents) + s.second, 8); samples += v;
    }
    return 1;
    }
}
