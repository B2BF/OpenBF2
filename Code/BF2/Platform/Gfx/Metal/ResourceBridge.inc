// [CHOSEN, not recovered] Private Metal resources with synchronous staging transfers.
// Included by MetalBridge.mm after Frame and command helpers.
static constexpr unsigned fourCC(char a,char b,char c,char d) {
    return unsigned(a)|(unsigned(b)<<8)|(unsigned(c)<<16)|(unsigned(d)<<24);
}
unsigned formatBlock(unsigned f) {
    return f == fourCC('D','X','T','1') || f == fourCC('D','X','T','3') || f == fourCC('D','X','T','5') ? 4 : 1;
}
unsigned formatBytes(unsigned f) {
    switch (f) {
    case 20: return 3;
    case 21: case 22: case 31: case 32: case 33: case 34: case 35: case 75: case 112: case 114: return 4;
    case 23: case 24: case 25: case 26: case 30: case 51: case 80: case 81: case 111: return 2;
    case 28: case 50: return 1;
    case 36: case 113: case 115: return 8;
    case 116: return 16;
    default:
        if (f == fourCC('D','X','T','1')) return 8;
        if (f == fourCC('D','X','T','3') || f == fourCC('D','X','T','5')) return 16;
        return 0;
    }
}
bool formatSupported(unsigned f) { return formatBytes(f) != 0; }
bool formatRenderable(unsigned f) { return formatSupported(f) && formatBlock(f) == 1; }
static MTLPixelFormat pixelFormat(unsigned f) {
    switch (f) {
    case 21: case 22: return MTLPixelFormatBGRA8Unorm;
    case 31: return MTLPixelFormatRGB10A2Unorm;
    case 35: return MTLPixelFormatBGR10A2Unorm;
    case 36: case 34: return MTLPixelFormatRGBA16Unorm;
    case 75: return MTLPixelFormatDepth32Float_Stencil8;
    case 80: return MTLPixelFormatDepth16Unorm;
    case 81: return MTLPixelFormatRGBA16Unorm;
    case 111: return MTLPixelFormatR16Float;
    case 112: return MTLPixelFormatRG16Float;
    case 113: return MTLPixelFormatRGBA16Float;
    case 114: return MTLPixelFormatR32Float;
    case 115: return MTLPixelFormatRG32Float;
    case 116: return MTLPixelFormatRGBA32Float;
    default:
        if (f == fourCC('D','X','T','1')) return MTLPixelFormatBC1_RGBA;
        if (f == fourCC('D','X','T','3')) return MTLPixelFormatBC2_RGBA;
        if (f == fourCC('D','X','T','5')) return MTLPixelFormatBC3_RGBA;
        return MTLPixelFormatRGBA8Unorm;
    }
}
static unsigned nativeBytes(unsigned f) {
    switch (f) {
    case 20: case 23: case 24: case 25: case 26: case 28: case 30: case 50: case 51: return 4;
    case 34: case 81: return 8;
    default: return formatBytes(f);
    }
}
Texture* createTexture(Frame* frame,unsigned w,unsigned h,unsigned d,unsigned levels,unsigned kind,unsigned f,unsigned usage,unsigned samples) {
    mainThread("IDirect3DDevice9::CreateTexture");
    bf2hitch::Scope timed(bf2hitch::TextureCreate, "create");
    @autoreleasepool {
        if (!formatSupported(f)) return nullptr;
        MTLTextureDescriptor* desc = [MTLTextureDescriptor new];
        desc.textureType = kind == 5 ? MTLTextureTypeCube : kind == 4 ? MTLTextureType3D : MTLTextureType2D;
        desc.width=w; desc.height=h; desc.depth=d; desc.mipmapLevelCount=levels;
        desc.pixelFormat=pixelFormat(f); desc.storageMode=MTLStorageModePrivate;
        desc.usage=MTLTextureUsageShaderRead | ((usage&3) ? MTLTextureUsageRenderTarget : 0);
        if (samples > 1) {
            // [CHOSEN, not recovered] A multisampled D3D9 surface is never sampled; StretchRect
            // and readbacks resolve it (resolvedCopy).
            if (kind != 1 || levels != 1 || !(usage&3)) return nullptr;
            desc.textureType = MTLTextureType2DMultisample; desc.sampleCount = samples;
            desc.usage = MTLTextureUsageRenderTarget;
        }
        id<MTLTexture> object = [frame->device newTextureWithDescriptor:desc];
        if (!object) return nullptr;
        return new Texture{object,f};
    }
}
void destroyTexture(Texture* t) { mainThread("IDirect3DResource9::Release"); delete t; }
Texture* defaultTarget(Frame* f,bool depth) { return depth ? &f->depthResource : &f->colorResource; }
void defaultColorFormat(Frame* f,unsigned format) { f->colorResource.format=format; }
bool copyTextureRegion(Frame* f,Texture* s,unsigned sl,unsigned ss,unsigned x,unsigned y,
    Texture* d,unsigned dl,unsigned ds,unsigned dx,unsigned dy,unsigned w,unsigned h) {
    mainThread("IDirect3DDevice9::StretchRect");
    @autoreleasepool {
    if(s->object.pixelFormat!=d->object.pixelFormat) frontier("IDirect3DDevice9::StretchRect native depth format conversion");
    // Only depth/stencil copies come here. A multisampled depth surface cannot be blitted, and
    // its D3D9 StretchRect contract is not established.
    if(s->object.sampleCount>1 || d->object.sampleCount>1)
        frontier("IDirect3DDevice9::StretchRect of a multisampled depth/stencil surface");
    if(!commands(f)) return false;
    auto blit=[f->commands blitCommandEncoder]; if(!blit) return false;
    [blit copyFromTexture:s->object sourceSlice:ss sourceLevel:sl sourceOrigin:MTLOriginMake(x,y,0) sourceSize:MTLSizeMake(w,h,1)
        toTexture:d->object destinationSlice:ds destinationLevel:dl destinationOrigin:MTLOriginMake(dx,dy,0)];
    [blit endEncoding]; return finish(f);
    }
}
bool waitIdle(Frame* f) {
    mainThread("bf2metal::waitIdle"); @autoreleasepool { return finish(f); }
}
std::uint64_t submitFence(Frame* f) {
    mainThread("IDirect3DQuery9::Issue");
    @autoreleasepool { if (f->commands) submit(f, false); return f->committed; }
}
bool fenceReached(Frame* f, std::uint64_t fence) { mainThread("IDirect3DQuery9::GetData"); return f->completed->load() >= fence; }
struct Buffer { id<MTLBuffer> object; };
Buffer* createBuffer(Frame* f,unsigned bytes) {
    mainThread("IDirect3DDevice9::CreateVertexBuffer");
    @autoreleasepool {
    id<MTLBuffer> b = [f->device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    return b ? new Buffer{b} : nullptr;
    }
}
void destroyBuffer(Buffer* b) { mainThread("IDirect3DResource9::Release"); delete b; }
void* bufferBytes(Buffer* b) { mainThread("bf2metal::bufferBytes"); return b->object.contents; }

// Converted legacy formats retain D3D packing in CPU locks and use renderable RGBA storage.
static unsigned expand(unsigned v,unsigned max) { return (v*255+max/2)/max; }
static unsigned shrink(unsigned v,unsigned max) { return (v*max+127)/255; }
static void convertPixel(unsigned f,unsigned char* d,unsigned char* n,bool upload) {
    if (f == 34 || f == 81) {
        std::uint16_t v[4] = {};
        if (upload) {
            std::memcpy(v,d,f==34?4:2);
            if (f==81) v[1]=v[2]=v[0];
            v[3]=65535; std::memcpy(n,v,8);
        } else { std::memcpy(v,n,8); std::memcpy(d,v,f==34?4:2); }
        return;
    }
    if (f==20 || f==23 || f==24 || f==25 || f==26 || f==30 || f==28 || f==50 || f==51) {
        unsigned v=0; const unsigned size=formatBytes(f);
        if (upload) {
            std::memcpy(&v,d,size);
            n[0]=n[1]=n[2]=0; n[3]=255;
            switch (f) {
            case 20: n[0]=d[2]; n[1]=d[1]; n[2]=d[0]; break;
            case 23: n[0]=expand((v>>11)&31,31); n[1]=expand((v>>5)&63,63); n[2]=expand(v&31,31); break;
            case 24: case 25:
                n[0]=expand((v>>10)&31,31); n[1]=expand((v>>5)&31,31); n[2]=expand(v&31,31);
                n[3]=f==24?255:((v&0x8000)?255:0); break;
            case 26: case 30:
                n[0]=expand((v>>8)&15,15); n[1]=expand((v>>4)&15,15); n[2]=expand(v&15,15);
                n[3]=f==30?255:expand(v>>12,15); break;
            case 28: n[0]=n[1]=n[2]=255; n[3]=d[0]; break;
            case 50: case 51: n[0]=n[1]=n[2]=d[0]; n[3]=f==50?255:d[1]; break;
            }
        } else {
            switch (f) {
            case 20: d[2]=n[0]; d[1]=n[1]; d[0]=n[2]; return;
            case 23: v=(shrink(n[0],31)<<11)|(shrink(n[1],63)<<5)|shrink(n[2],31); break;
            case 24: case 25: v=(shrink(n[0],31)<<10)|(shrink(n[1],31)<<5)|shrink(n[2],31)|((f==24||n[3]>=128)?0x8000:0); break;
            case 26: case 30: v=(shrink(n[0],15)<<8)|(shrink(n[1],15)<<4)|shrink(n[2],15)|((f==30?15:shrink(n[3],15))<<12); break;
            case 28: v=n[3]; break;
            case 50: v=n[0]; break;
            case 51: v=n[0]|(unsigned(n[3])<<8); break;
            }
            std::memcpy(d,&v,size);
        }
        return;
    }
    if (upload) { std::memcpy(n,d,formatBytes(f)); if (f==22 || f==33) n[3]=255; }
    else { std::memcpy(d,n,formatBytes(f)); if (f==22 || f==33) d[3]=255; }
}

// The copy helpers preserve D3D channel packing through the same conversion as texture upload.
static void decodeNative(unsigned f,const unsigned char* p,double* c) {
    c[0]=c[1]=c[2]=0; c[3]=1;
    switch(pixelFormat(f)) {
    case MTLPixelFormatBGRA8Unorm:
        c[0]=p[2]/255.0; c[1]=p[1]/255.0; c[2]=p[0]/255.0; c[3]=p[3]/255.0; break;
    case MTLPixelFormatRGBA8Unorm:
        for(unsigned i=0;i<4;++i) c[i]=p[i]/255.0; break;
    case MTLPixelFormatRGB10A2Unorm: case MTLPixelFormatBGR10A2Unorm: {
        std::uint32_t v; std::memcpy(&v,p,4);
        c[0]=(v&1023)/1023.0; c[1]=((v>>10)&1023)/1023.0; c[2]=((v>>20)&1023)/1023.0; c[3]=(v>>30)/3.0;
        if(f==35) std::swap(c[0],c[2]); break;
    }
    case MTLPixelFormatRGBA16Unorm:
        for(unsigned i=0;i<4;++i) { std::uint16_t v; std::memcpy(&v,p+i*2,2); c[i]=v/65535.0; } break;
    case MTLPixelFormatR16Float: case MTLPixelFormatRG16Float: case MTLPixelFormatRGBA16Float:
        for(unsigned i=0;i<nativeBytes(f)/2;++i) { _Float16 v; std::memcpy(&v,p+i*2,2); c[i]=float(v); } break;
    case MTLPixelFormatR32Float: case MTLPixelFormatRG32Float: case MTLPixelFormatRGBA32Float:
        for(unsigned i=0;i<nativeBytes(f)/4;++i) { float v; std::memcpy(&v,p+i*4,4); c[i]=v; } break;
    default: break;
    }
}
static unsigned unorm(double v,unsigned max) { return unsigned(std::max(0.0,std::min(1.0,v))*max+0.5); }
static void encodeNative(unsigned f,unsigned char* p,const double* c) {
    switch(pixelFormat(f)) {
    case MTLPixelFormatBGRA8Unorm:
        p[2]=unorm(c[0],255); p[1]=unorm(c[1],255); p[0]=unorm(c[2],255); p[3]=unorm(c[3],255); break;
    case MTLPixelFormatRGBA8Unorm:
        for(unsigned i=0;i<4;++i) p[i]=unorm(c[i],255); break;
    case MTLPixelFormatRGB10A2Unorm: case MTLPixelFormatBGR10A2Unorm: {
        const auto v=std::uint32_t(unorm(c[f==35?2:0],1023)|(unorm(c[1],1023)<<10)|
            (unorm(c[f==35?0:2],1023)<<20)|(unorm(c[3],3)<<30)); std::memcpy(p,&v,4); break;
    }
    case MTLPixelFormatRGBA16Unorm:
        for(unsigned i=0;i<4;++i) { const auto v=std::uint16_t(unorm(c[i],65535)); std::memcpy(p+i*2,&v,2); } break;
    case MTLPixelFormatR16Float: case MTLPixelFormatRG16Float: case MTLPixelFormatRGBA16Float:
        for(unsigned i=0;i<nativeBytes(f)/2;++i) { const _Float16 v=static_cast<_Float16>(c[i]); std::memcpy(p+i*2,&v,2); } break;
    case MTLPixelFormatR32Float: case MTLPixelFormatRG32Float: case MTLPixelFormatRGBA32Float:
        for(unsigned i=0;i<nativeBytes(f)/4;++i) { const float v=float(c[i]); std::memcpy(p+i*4,&v,4); } break;
    default: break;
    }
}
bool resample(unsigned f,const void* src,unsigned sw,unsigned sh,unsigned sp,void* dst,unsigned dw,unsigned dh,unsigned dp,bool linear) {
    if(!formatSupported(f) || formatBlock(f)>1 || f==75 || f==80) return false;
    const unsigned bytes=formatBytes(f);
    const auto* a=static_cast<const unsigned char*>(src); auto* b=static_cast<unsigned char*>(dst);
    auto read=[&](int x,int y,double* c) {
        x=std::max(0,std::min(int(sw)-1,x)); y=std::max(0,std::min(int(sh)-1,y));
        unsigned char packed[16]={}, native[16]={}; std::memcpy(packed,a+y*sp+x*bytes,bytes);
        convertPixel(f,packed,native,true); decodeNative(f,native,c);
    };
    for(unsigned y=0;y<dh;++y) for(unsigned x=0;x<dw;++x) {
        const double u=(x+0.5)*sw/dw-0.5, v=(y+0.5)*sh/dh-0.5;
        if(!linear) {
            const unsigned sx=std::min(sw-1,unsigned(u+0.5)), sy=std::min(sh-1,unsigned(v+0.5));
            std::memcpy(b+y*dp+x*bytes,a+sy*sp+sx*bytes,bytes); continue;
        }
        const int x0=int(std::floor(u)), y0=int(std::floor(v)); const double fx=u-x0, fy=v-y0;
        double c[4]={}, p[4][4]; read(x0,y0,p[0]); read(x0+1,y0,p[1]); read(x0,y0+1,p[2]); read(x0+1,y0+1,p[3]);
        for(unsigned i=0;i<4;++i) c[i]=(p[0][i]*(1-fx)+p[1][i]*fx)*(1-fy)+(p[2][i]*(1-fx)+p[3][i]*fx)*fy;
        unsigned char native[16]={}; encodeNative(f,native,c); convertPixel(f,b+y*dp+x*bytes,native,false);
    }
    return true;
}

static bool blitBytes(Frame* f,Texture* t,unsigned level,unsigned slice,id<MTLBuffer> b,
                      unsigned row,unsigned image,MTLSize size,bool upload,MTLBlitOption options) {
    if (!commands(f)) return false;
    auto blit = [f->commands blitCommandEncoder];
    if (!blit) return false;
    if (upload)
        [blit copyFromBuffer:b sourceOffset:0 sourceBytesPerRow:row sourceBytesPerImage:image sourceSize:size
            toTexture:t->object destinationSlice:slice destinationLevel:level destinationOrigin:MTLOriginMake(0,0,0) options:options];
    else
        [blit copyFromTexture:t->object sourceSlice:slice sourceLevel:level sourceOrigin:MTLOriginMake(0,0,0) sourceSize:size
            toBuffer:b destinationOffset:0 destinationBytesPerRow:row destinationBytesPerImage:image options:options];
    [blit endEncoding];
    // An upload stays in the frame's command buffer (it retains the staging buffer; encoder order
    // keeps it between the draws before and after it); a readback waits for the GPU.
    return upload || finish(f);
}
// [CHOSEN, not recovered] The single-sample resolve of a multisampled colour surface, in a
// temporary texture: what StretchRect from a multisampled render target reads (D3D9 resolves).
static id<MTLTexture> resolvedCopy(Frame* f,id<MTLTexture> ms) {
    if (ms.pixelFormat==MTLPixelFormatDepth24Unorm_Stencil8 || ms.pixelFormat==MTLPixelFormatDepth32Float_Stencil8 ||
        ms.pixelFormat==MTLPixelFormatDepth16Unorm)
        frontier("IDirect3DDevice9 readback of a multisampled depth/stencil surface");
    MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:ms.pixelFormat
        width:ms.width height:ms.height mipmapped:NO];
    d.storageMode=MTLStorageModePrivate; d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    id<MTLTexture> single=[f->device newTextureWithDescriptor:d];
    if (!single || !commands(f)) return nil;
    auto* pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=ms; pass.colorAttachments[0].loadAction=MTLLoadActionLoad;
    pass.colorAttachments[0].resolveTexture=single; pass.colorAttachments[0].storeAction=MTLStoreActionStoreAndMultisampleResolve;
    auto e=[f->commands renderCommandEncoderWithDescriptor:pass];
    if (!e) return nil;
    [e endEncoding];
    return single;
}
static bool seedBackBufferSamples(Frame* f); // MenuBridge.inc
static bool transferSingle(Frame* f,Texture* t,unsigned level,unsigned slice,void* bytes,unsigned pitch,unsigned image,bool upload);
bool transfer(Frame* f,Texture* t,unsigned level,unsigned slice,void* bytes,unsigned pitch,unsigned image,bool upload) {
    mainThread(upload?"IDirect3DResource9::Unlock upload":"IDirect3DResource9::Lock readback");
    bf2hitch::Scope timed(bf2hitch::TextureCreate, upload?"upload":"readback");
    @autoreleasepool {
        if (t && t->object && t->object.sampleCount>1) {
            if (upload) frontier("IDirect3DDevice9 write into a multisampled render target");
            Texture single{resolvedCopy(f,t->object),t->format};
            return single.object && transferSingle(f,&single,0,0,bytes,pitch,image,false);
        }
        // [CHOSEN, not recovered] The multisampled back buffer renders into its twin: a read or
        // write of the single-sample colour target resolves the twin first, and a write is copied
        // back into every sample of the twin, or the next resolve would overwrite it.
        if (t && t->object==f->color && !resolveBackBuffer(f)) return false;
        if (!transferSingle(f,t,level,slice,bytes,pitch,image,upload)) return false;
        return !upload || t->object!=f->color || !f->colorMS || seedBackBufferSamples(f);
    }
}
static bool transferSingle(Frame* f,Texture* t,unsigned level,unsigned slice,void* bytes,unsigned pitch,unsigned image,bool upload) {
    @autoreleasepool {
        if ((!upload && !finish(f)) || !t || !t->object) return false;
        const unsigned w=std::max(1u,unsigned(t->object.width)>>level);
        const unsigned h=std::max(1u,unsigned(t->object.height)>>level);
        const unsigned d=std::max(1u,unsigned(t->object.depth)>>level);
        const unsigned block=formatBlock(t->format), bpp=nativeBytes(t->format);
        const unsigned rows=(h+block-1)/block, columns=(w+block-1)/block;
        const unsigned row=(columns*bpp+255)&~255u, sliceBytes=row*rows;
        const MTLSize size=MTLSizeMake(w,h,d);
        id<MTLBuffer> staging=[f->device newBufferWithLength:std::size_t(sliceBytes)*d options:MTLResourceStorageModeShared];
        if (!staging) return false;
        auto* cpu=static_cast<unsigned char*>(bytes);
        auto* gpu=static_cast<unsigned char*>(staging.contents);
        if (t->format==75) {
            // D24S8 is not lockable in D3D9; the private bridge's chosen readback representation
            // is high 24 bits normalized depth, low 8 stencil. Public locks refuse depth surfaces.
            if (t->object.pixelFormat != MTLPixelFormatDepth32Float_Stencil8)
                frontier("IDirect3DResource9 depth24 native readback");
            id<MTLBuffer> stencil=[f->device newBufferWithLength:std::size_t(sliceBytes)*d options:MTLResourceStorageModeShared];
            if (!stencil) return false;
            auto* st=static_cast<unsigned char*>(stencil.contents);
            if (upload) for (unsigned z=0;z<d;++z) for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                std::uint32_t v; std::memcpy(&v,cpu+z*image+y*pitch+x*4,4);
                float depth=(v>>8)/16777215.0f; std::memcpy(gpu+z*sliceBytes+y*row+x*4,&depth,4);
                st[z*sliceBytes+y*row+x]=v&255;
            }
            if (!blitBytes(f,t,level,slice,staging,row,sliceBytes,size,upload,MTLBlitOptionDepthFromDepthStencil) ||
                !blitBytes(f,t,level,slice,stencil,row,sliceBytes,size,upload,MTLBlitOptionStencilFromDepthStencil)) return false;
            if (!upload) for (unsigned z=0;z<d;++z) for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                float depth; std::memcpy(&depth,gpu+z*sliceBytes+y*row+x*4,4);
                const std::uint32_t v=(std::uint32_t(double(depth)*16777215.0+0.5)<<8)|st[z*sliceBytes+y*row+x];
                std::memcpy(cpu+z*image+y*pitch+x*4,&v,4);
            }
            return true;
        }
        auto convert = [&] {
            for (unsigned z=0;z<d;++z) for (unsigned y=0;y<rows;++y) {
                auto* a=cpu+z*image+y*pitch; auto* b=gpu+z*sliceBytes+y*row;
                if (block==4) { if(upload) std::memcpy(b,a,columns*bpp); else std::memcpy(a,b,columns*bpp); }
                else for(unsigned x=0;x<w;++x) convertPixel(t->format,a+x*formatBytes(t->format),b+x*bpp,upload);
            }
        };
        if(upload) convert();
        if(!blitBytes(f,t,level,slice,staging,row,sliceBytes,size,upload,MTLBlitOptionNone)) return false;
        if(!upload) convert();
        return true;
    }
}
bool uploadRegion(Frame* f,Texture* t,unsigned level,unsigned slice,unsigned x,unsigned y,
                  unsigned w,unsigned h,const void* bytes,unsigned pitch) {
    mainThread("IDirect3DDevice9::UpdateSurface upload");
    @autoreleasepool {
        if (!t || !t->object || t->format==75 || !w || !h) return false;
        const unsigned block=formatBlock(t->format), bpp=nativeBytes(t->format), size=formatBytes(t->format);
        const unsigned rows=(h+block-1)/block, columns=(w+block-1)/block;
        const unsigned row=(columns*bpp+255)&~255u;
        id<MTLBuffer> staging=[f->device newBufferWithLength:std::size_t(row)*rows options:MTLResourceStorageModeShared];
        if (!staging) return false;
        auto* cpu=static_cast<unsigned char*>(const_cast<void*>(bytes));
        auto* gpu=static_cast<unsigned char*>(staging.contents);
        for (unsigned r=0;r<rows;++r) {
            auto* a=cpu+std::size_t(r)*pitch; auto* b=gpu+std::size_t(r)*row;
            if (block==4) std::memcpy(b,a,columns*bpp);
            else for (unsigned c=0;c<w;++c) convertPixel(t->format,a+c*size,b+c*bpp,true);
        }
        // The staging buffer is retained by the command buffer; encoder order keeps the upload
        // between the draws before and after it, as D3D9 orders UpdateSurface.
        if (!commands(f) || (t->object==f->color && !resolveBackBuffer(f))) return false;
        auto blit=[f->commands blitCommandEncoder];
        if (!blit) return false;
        [blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:row sourceBytesPerImage:std::size_t(row)*rows
            sourceSize:MTLSizeMake(w,h,1) toTexture:t->object destinationSlice:slice destinationLevel:level
            destinationOrigin:MTLOriginMake(x,y,0)];
        [blit endEncoding];
        return t->object!=f->color || !f->colorMS || seedBackBufferSamples(f);
    }
}
static bool fullClearTargets(Frame* f,Texture* color,unsigned cl,unsigned cs,Texture* depth,unsigned dl,unsigned ds,
                  unsigned flags,std::uint32_t value,float z,unsigned stencil) {
    mainThread("IDirect3DDevice9::Clear");
    @autoreleasepool {
    if (!commands(f)) return false;
    if ((flags&1) && (flags&6) && samplesMismatch(f,color->object,depth->object))
        return fullClearTargets(f,color,cl,cs,depth,dl,ds,flags&1,value,z,stencil) &&
            fullClearTargets(f,color,cl,cs,depth,dl,ds,flags&6,value,z,stencil);
    (void) targetSamples(f,(flags&1)?color->object:nil,(flags&6)?depth->object:nil);
    auto* pass=[MTLRenderPassDescriptor renderPassDescriptor];
    if(flags&1) {
        auto* a=pass.colorAttachments[0]; attachColor(f,a,color->object); a.level=cl; a.slice=cs;
        a.loadAction=MTLLoadActionClear;
        double channels[4]={((value>>16)&255)/255.0,((value>>8)&255)/255.0,(value&255)/255.0,(value>>24)/255.0};
        unsigned char packed[16]={}, native[16]={};
        encodeNative(color->format,native,channels);
        convertPixel(color->format,packed,native,false);
        convertPixel(color->format,packed,native,true);
        decodeNative(color->format,native,channels);
        a.clearColor=MTLClearColorMake(channels[0],channels[1],channels[2],channels[3]);
    }
    if(flags&6) {
        auto* a=pass.depthAttachment; a.texture=depth->object; a.level=dl; a.slice=ds;
        a.loadAction=flags&2?MTLLoadActionClear:MTLLoadActionLoad; a.storeAction=MTLStoreActionStore;
        a.clearDepth=depth->format==75?std::uint32_t(z*16777215.0+0.5)/16777215.0:z;
        if(depth->format==75) {
            auto* s=pass.stencilAttachment; s.texture=depth->object; s.level=dl; s.slice=ds;
            s.loadAction=flags&4?MTLLoadActionClear:MTLLoadActionLoad; s.storeAction=MTLStoreActionStore; s.clearStencil=stencil&255;
        }
    }
    auto e=[f->commands renderCommandEncoderWithDescriptor:pass];
    if(!e) return false; [e endEncoding]; return true;
    }
}
