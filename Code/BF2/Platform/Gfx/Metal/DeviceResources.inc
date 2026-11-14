// [CHOSEN, not recovered] G3b resource factories, declarations, queries and subresource copies.
    Query* activeQuery=nullptr;
    // [CHOSEN, not recovered] A format this device does not offer (CheckDeviceFormat answers
    // NOTAVAILABLE for it) is refused with D3DERR_INVALIDCALL, as the D3D9 runtime refuses a
    // resource whose format the driver lacks; the first refusal per format is logged.
    static HRESULT formatRefused(const char* method,D3DFORMAT format) {
        char name[200]; std::snprintf(name,sizeof(name),"%s format 0x%08x",method,unsigned(format));
        bf2gfx::unsupported(name); return D3DERR_INVALIDCALL;
    }
    HRESULT image(unsigned w,unsigned h,unsigned d,unsigned& levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,
                  D3DRESOURCETYPE kind,HANDLE* shared,std::shared_ptr<Image>& out,unsigned samples=1) {
        const char* method=kind==D3DRTYPE_TEXTURE?"IDirect3DDevice9::CreateTexture":
            kind==D3DRTYPE_CUBETEXTURE?"IDirect3DDevice9::CreateCubeTexture":
            kind==D3DRTYPE_VOLUMETEXTURE?"IDirect3DDevice9::CreateVolumeTexture":
            usage==D3DUSAGE_RENDERTARGET?"IDirect3DDevice9::CreateRenderTarget":
            usage==D3DUSAGE_DEPTHSTENCIL?"IDirect3DDevice9::CreateDepthStencilSurface":"IDirect3DDevice9::CreateOffscreenPlainSurface";
        if(!bf2metal::formatSupported(format)) return formatRefused(method,format);
        // D3DPOOL_SCRATCH: CPU storage like SYSTEMMEM; the device-access checks (StretchRect,
        // UpdateSurface, GetRenderTargetData, sampling) name DEFAULT/SYSTEMMEM and so refuse it, as
        // the D3D9 runtime does. The screenshot capture loads the back buffer into one (PE 0x10005200).
        // D3D9 rules: another pool value is invalid, and shared handles exist only on D3D9Ex.
        if(pool!=D3DPOOL_DEFAULT && pool!=D3DPOOL_MANAGED && pool!=D3DPOOL_SYSTEMMEM && pool!=D3DPOOL_SCRATCH)
            return D3DERR_INVALIDCALL;
        if(shared) return D3DERR_INVALIDCALL;
        if(!w || !h || !d || w>8192 || h>8192 || d>256 || (kind==D3DRTYPE_VOLUMETEXTURE && (w>256 || h>256)) ||
            (usage&~(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL|D3DUSAGE_DYNAMIC)) ||
            ((usage&3) && pool!=D3DPOOL_DEFAULT) || ((usage&3)==3) || ((usage&D3DUSAGE_DYNAMIC)&&pool!=D3DPOOL_DEFAULT)) return D3DERR_INVALIDCALL;
        const bool depth=format==D3DFMT_D24S8 || format==D3DFMT_D16;
        if(bool(usage&D3DUSAGE_DEPTHSTENCIL)!=depth || (depth && kind==D3DRTYPE_VOLUMETEXTURE) ||
            ((usage&3) && !bf2metal::formatRenderable(format)) || (kind==D3DRTYPE_VOLUMETEXTURE && bf2metal::formatBlock(format)>1))
            return formatRefused(method,format);
        unsigned maxLevels=1; for(unsigned n=std::max(w,std::max(h,d));n>1;n>>=1) ++maxLevels;
        if(!levels) levels=maxLevels; if(levels>maxLevels) return D3DERR_INVALIDCALL;
        if(bf2metal::formatBlock(format)>1 && (w%4 || h%4)) return D3DERR_INVALIDCALL;
        out=std::make_shared<Image>();
        if(pool!=D3DPOOL_SYSTEMMEM && pool!=D3DPOOL_SCRATCH) {
            out->native=bf2metal::createTexture(native,w,h,d,levels,kind,format,usage,samples);
            if(!out->native) return D3DERR_OUTOFVIDEOMEMORY;
        }
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT w,UINT h,UINT levels,DWORD u,D3DFORMAT f,D3DPOOL p,IDirect3DTexture9** out,HANDLE* shared) override {
        mainThread("IDirect3DDevice9::CreateTexture"); if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        std::shared_ptr<Image> im; const HRESULT hr=image(w,h,1,levels,u,f,p,D3DRTYPE_TEXTURE,shared,im); if(FAILED(hr)) return hr;
        auto* t=new Texture2D(this,native,"IDirect3DTexture9",D3DRTYPE_TEXTURE,im,w,h,1,levels,u,f,p);
        *out=t; t->AddRef(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT edge,UINT levels,DWORD u,D3DFORMAT f,D3DPOOL p,IDirect3DCubeTexture9** out,HANDLE* shared) override {
        mainThread("IDirect3DDevice9::CreateCubeTexture"); if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        std::shared_ptr<Image> im; const HRESULT hr=image(edge,edge,1,levels,u,f,p,D3DRTYPE_CUBETEXTURE,shared,im); if(FAILED(hr)) return hr;
        auto* t=new CubeTexture(this,native,"IDirect3DCubeTexture9",D3DRTYPE_CUBETEXTURE,im,edge,edge,1,levels,u,f,p);
        *out=t; t->AddRef(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT w,UINT h,UINT d,UINT levels,DWORD u,D3DFORMAT f,D3DPOOL p,IDirect3DVolumeTexture9** out,HANDLE* shared) override {
        mainThread("IDirect3DDevice9::CreateVolumeTexture"); if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        if(u&3) return D3DERR_INVALIDCALL; // D3D9: volume textures cannot be render or depth targets.
        std::shared_ptr<Image> im; const HRESULT hr=image(w,h,d,levels,u,f,p,D3DRTYPE_VOLUMETEXTURE,shared,im); if(FAILED(hr)) return hr;
        auto* t=new VolumeTexture(this,native,"IDirect3DVolumeTexture9",D3DRTYPE_VOLUMETEXTURE,im,w,h,d,levels,u,f,p);
        *out=t; t->AddRef(); return D3D_OK;
    }
    HRESULT surface(unsigned w,unsigned h,D3DFORMAT f,DWORD usage,D3DPOOL pool,bool lockable,IDirect3DSurface9** out,HANDLE* shared,
                    unsigned samples=1) {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        unsigned levels=1; std::shared_ptr<Image> im; const HRESULT hr=image(w,h,1,levels,usage,f,pool,D3DRTYPE_SURFACE,shared,im,samples); if(FAILED(hr)) return hr;
        auto* s=new Surface(this,native,usage);
        s->plane=std::make_shared<Plane>(Plane{native,im,w,h,1,0,0,f,usage,pool,false,0,{}});
        s->plane->samples=samples;
        s->desc=s->plane->desc(); s->lockable=lockable; *out=s; s->AddRef(); return D3D_OK;
    }
    // [CHOSEN, not recovered] Multisampled surfaces (MetalBridge createTexture samples): a sample
    // count and quality CheckDeviceMultiSampleType does not offer is D3DERR_NOTAVAILABLE (the
    // runtime's answer, Wine wined3d_texture_init). A quality without multisampling (NONE offers
    // no quality levels) and a lockable multisampled render target (a Metal multisample texture
    // has no CPU storage) are refused with D3DERR_INVALIDCALL. Retail creates neither: its AA
    // targets come from the video options' (type, quality) pairs and are never lockable.
    HRESULT multisample(const char* method,D3DFORMAT f,D3DMULTISAMPLE_TYPE samples,DWORD quality,bool lockable,unsigned& count) {
        count=1;
        if(samples==D3DMULTISAMPLE_NONE) {
            if(quality) { bf2gfx::unsupported(std::string(method)+" quality without multisample"); return D3DERR_INVALIDCALL; }
            return D3D_OK;
        }
        DWORD levels=0;
        if(FAILED(parent->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,f,TRUE,samples,&levels)) || quality>=levels)
            return D3DERR_NOTAVAILABLE;
        if(lockable) { bf2gfx::unsupported(std::string(method)+" lockable multisample"); return D3DERR_INVALIDCALL; }
        count=unsigned(samples); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT w,UINT h,D3DFORMAT f,D3DMULTISAMPLE_TYPE samples,DWORD quality,BOOL lockable,IDirect3DSurface9** out,HANDLE* shared) override {
        mainThread("IDirect3DDevice9::CreateRenderTarget");
        if(out) *out=nullptr;
        if(f==D3DFMT_D24S8 || f==D3DFMT_D16) return D3DERR_INVALIDCALL;
        unsigned count=1; const HRESULT hr=multisample("IDirect3DDevice9::CreateRenderTarget",f,samples,quality,lockable!=0,count);
        if(FAILED(hr)) return hr;
        return surface(w,h,f,D3DUSAGE_RENDERTARGET,D3DPOOL_DEFAULT,lockable!=0,out,shared,count);
    }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT w,UINT h,D3DFORMAT f,D3DMULTISAMPLE_TYPE samples,DWORD quality,BOOL,IDirect3DSurface9** out,HANDLE* shared) override {
        mainThread("IDirect3DDevice9::CreateDepthStencilSurface");
        if(out) *out=nullptr;
        if(f!=D3DFMT_D24S8 && f!=D3DFMT_D16) return formatRefused("IDirect3DDevice9::CreateDepthStencilSurface",f);
        unsigned count=1; const HRESULT hr=multisample("IDirect3DDevice9::CreateDepthStencilSurface",f,samples,quality,false,count);
        if(FAILED(hr)) return hr;
        return surface(w,h,f,D3DUSAGE_DEPTHSTENCIL,D3DPOOL_DEFAULT,false,out,shared,count);
    }
    HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(UINT w,UINT h,D3DFORMAT f,D3DPOOL p,IDirect3DSurface9** out,HANDLE* shared) override {
        mainThread("IDirect3DDevice9::CreateOffscreenPlainSurface");
        if(p==D3DPOOL_MANAGED) return D3DERR_INVALIDCALL;
        return surface(w,h,f,0,p,true,out,shared);
    }
    static HRESULT bufferArgs(unsigned length,DWORD usage,D3DPOOL pool,HANDLE* shared) {
        // D3D9 rules: shared handles exist only on D3D9Ex; buffers take DEFAULT, MANAGED or
        // SYSTEMMEM (SCRATCH and any other value are invalid).
        if(shared) return D3DERR_INVALIDCALL;
        if(pool!=D3DPOOL_DEFAULT && pool!=D3DPOOL_MANAGED && pool!=D3DPOOL_SYSTEMMEM) return D3DERR_INVALIDCALL;
        if(!length || (usage&~(D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY)) || ((usage&D3DUSAGE_DYNAMIC)&&pool!=D3DPOOL_DEFAULT)) return D3DERR_INVALIDCALL;
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT length,DWORD usage,DWORD fvf,D3DPOOL pool,IDirect3DVertexBuffer9** out,HANDLE* shared) override {
        mainThread("IDirect3DDevice9::CreateVertexBuffer"); if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        const HRESULT hr=bufferArgs(length,usage,pool,shared); if(FAILED(hr)) return hr;
        auto* b=new VertexBuffer(this,native,"IDirect3DVertexBuffer9",D3DVERTEXBUFFER_DESC{D3DFMT_VERTEXDATA,D3DRTYPE_VERTEXBUFFER,usage,pool,length,fvf});
        if(!b->native) { delete b; return D3DERR_OUTOFVIDEOMEMORY; } *out=b; b->AddRef(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT length,DWORD usage,D3DFORMAT f,D3DPOOL pool,IDirect3DIndexBuffer9** out,HANDLE* shared) override {
        mainThread("IDirect3DDevice9::CreateIndexBuffer"); if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        if(f!=D3DFMT_INDEX16 && f!=D3DFMT_INDEX32) return D3DERR_INVALIDCALL; // D3D9: index formats are 16 or 32 bit
        if(length%(f==D3DFMT_INDEX16?2:4)) return D3DERR_INVALIDCALL;
        const HRESULT hr=bufferArgs(length,usage,pool,shared); if(FAILED(hr)) return hr;
        auto* b=new IndexBuffer(this,native,"IDirect3DIndexBuffer9",D3DINDEXBUFFER_DESC{f,D3DRTYPE_INDEXBUFFER,usage,pool,length});
        if(!b->native) { delete b; return D3DERR_OUTOFVIDEOMEMORY; } *out=b; b->AddRef(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(const D3DVERTEXELEMENT9* elements,IDirect3DVertexDeclaration9** out) override {
        mainThread("IDirect3DDevice9::CreateVertexDeclaration"); if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        if(!elements) return D3DERR_INVALIDCALL;
        std::unique_ptr<Declaration> d(new Declaration(this,native));
        for(unsigned i=0;i<=64;++i) {
            const auto& e=elements[i];
            d->elements.push_back(e);
            if(e.Stream==255 && e.Type==D3DDECLTYPE_UNUSED) {
                if(i==0 || e.Offset || e.Method || e.Usage || e.UsageIndex) return D3DERR_INVALIDCALL;
                d->native=bf2metal::createLayout(d->mapped.data(),unsigned(d->mapped.size()));
                if(!d->native) { bf2gfx::unsupported("IDirect3DDevice9::CreateVertexDeclaration type/attribute limit"); return D3DERR_INVALIDCALL; }
                *out=d.release(); (*out)->AddRef(); return D3D_OK;
            }
            if(i==64 || e.Stream>=16 || e.UsageIndex>=16) return D3DERR_INVALIDCALL;
            // [CHOSEN, not recovered] Tessellation methods (no patch caps), the FOG/DEPTH/SAMPLE
            // usages and UDEC3/DEC3N (absent from caps DeclTypes) are refused with
            // D3DERR_INVALIDCALL. The shipped meshes declare types 0, 1, 2 and 4 only and the
            // tree's declarations use usages 0..10.
            if(e.Method || e.Usage>10 || e.Type==13 || e.Type==14 || e.Type>16 || e.Offset>2047) {
                bf2gfx::unsupported("IDirect3DDevice9::CreateVertexDeclaration type/usage/method");
                return D3DERR_INVALIDCALL;
            }
            for(const auto& prev:d->mapped) if(prev.usage==e.Usage && prev.index==e.UsageIndex) return D3DERR_INVALIDCALL;
            d->mapped.push_back({e.Stream,e.Offset,e.Type,e.Usage,e.UsageIndex});
        }
        return D3DERR_INVALIDCALL;
    }
    HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE type,IDirect3DQuery9** out) override {
        mainThread("IDirect3DDevice9::CreateQuery"); if(out) *out=nullptr;
        // A support probe must answer, as CheckDeviceFormat does (Wine test_query_support).
        if(type!=D3DQUERYTYPE_EVENT && type!=D3DQUERYTYPE_OCCLUSION) return D3DERR_NOTAVAILABLE;
        if(!out) return D3D_OK; // D3D9 support probe.
        auto* q=new Query(this,native,type,&activeQuery); *out=q; q->AddRef(); return D3D_OK;
    }
    static bool rectangle(const RECT* r,const Plane& p,RECT& out) {
        out=r?*r:RECT{0,0,LONG(p.w),LONG(p.h)};
        return out.left>=0 && out.top>=0 && out.left<out.right && out.top<out.bottom && unsigned(out.right)<=p.w && unsigned(out.bottom)<=p.h;
    }
    HRESULT copySurface(IDirect3DSurface9* source,const RECT* sourceRect,IDirect3DSurface9* dest,const RECT* destRect,D3DTEXTUREFILTERTYPE filter) {
        auto* s=dynamic_cast<Surface*>(source); auto* d=dynamic_cast<Surface*>(dest);
        if(!s || !d || s->owner!=this || d->owner!=this || s==d || s->plane->locked || d->plane->locked) return D3DERR_INVALIDCALL;
        auto& a=*s->plane; auto& b=*d->plane; RECT sr,dr;
        if(!rectangle(sourceRect,a,sr) || !rectangle(destRect,b,dr)) return D3DERR_INVALIDCALL;
        if(filter!=D3DTEXF_NONE && filter!=D3DTEXF_POINT && filter!=D3DTEXF_LINEAR) return D3DERR_INVALIDCALL; // as StretchRect
        // X8R8G8B8 <-> A8R8G8B8 share their byte layout; the CPU copy reads X as opaque, as D3D9
        // samples it. Retail Terrain::drawMain copies the X8R8G8B8 back buffer into its A8R8G8B8
        // scratch target this way under multisampling (TerrainDraw.cpp, device +0x88).
        const bool xrgb=(a.format==D3DFMT_X8R8G8B8 && b.format==D3DFMT_A8R8G8B8) ||
            (a.format==D3DFMT_A8R8G8B8 && b.format==D3DFMT_X8R8G8B8);
        // [CHOSEN, not recovered] Other conversions are refused (CheckDeviceFormatConversion
        // offers only the X8/A8 pair); retail copies only same-format or X8/A8 surfaces.
        if(a.format!=b.format && !xrgb) { bf2gfx::unsupported("IDirect3DDevice9::StretchRect format conversion"); return D3DERR_INVALIDCALL; }
        // A multisampled source resolves (its read() goes through a resolve). A multisampled
        // destination: same-size, same-format GPU copies between targets go through copyColor
        // below like single-sample ones when the bridge supports them; the CPU paths cannot
        // write a multisample texture, so those are refused with D3DERR_INVALIDCALL.
        const bool multisampledDest=b.samples>1;
        const unsigned block=a.block(), size=bf2metal::formatBytes(a.format);
        const unsigned sw=sr.right-sr.left, sh=sr.bottom-sr.top, dw=dr.right-dr.left, dh=dr.bottom-dr.top;
        if(a.format==D3DFMT_D24S8 || a.format==D3DFMT_D16) {
            // D3D9 rules: depth/stencil StretchRect copies whole same-size surfaces, unfiltered,
            // between DEFAULT-pool surfaces.
            if(sw!=dw || sh!=dh || filter!=D3DTEXF_NONE || a.pool==D3DPOOL_SYSTEMMEM || b.pool==D3DPOOL_SYSTEMMEM)
                return D3DERR_INVALIDCALL;
            return bf2metal::copyTextureRegion(native,a.image->native,a.level,a.slice,sr.left,sr.top,
                b.image->native,b.level,b.slice,dr.left,dr.top,sw,sh)?D3D_OK:D3DERR_DEVICELOST;
        }
        if(block>1 && (sr.left%block || sr.top%block || dr.left%block || dr.top%block ||
            (unsigned(sr.right)!=a.w && sr.right%block) || (unsigned(sr.bottom)!=a.h && sr.bottom%block) ||
            (unsigned(dr.right)!=b.w && dr.right%block) || (unsigned(dr.bottom)!=b.h && dr.bottom%block))) return D3DERR_INVALIDCALL;
        if(a.image->native && b.image->native && sw==dw && sh==dh && block==1 && !a.level && !a.slice &&
            (b.usage&D3DUSAGE_RENDERTARGET)) {
            // [CHOSEN, not recovered] Same-size copies between GPU surfaces stay on the GPU; the CPU
            // round trip (two readbacks, each waiting for the GPU, and an upload) cost a frame's
            // worth under multisampling, where Terrain::drawMain copies the back buffer every frame.
            // X8 formats read as opaque, as the CPU path converts them.
            const bool opaque=a.format==D3DFMT_X8R8G8B8 || a.format==D3DFMT_X8B8G8R8 ||
                b.format==D3DFMT_X8R8G8B8 || b.format==D3DFMT_X8B8G8R8;
            if(!multisampledDest && bf2metal::copyColor(native,a.image->native,sr.left,sr.top,b.image->native,b.level,b.slice,
                    dr.left,dr.top,sw,sh,opaque)) return D3D_OK;
        }
        if(multisampledDest) { bf2gfx::unsupported("IDirect3DDevice9::StretchRect into a multisampled surface"); return D3DERR_INVALIDCALL; }
        if(!a.image->native && b.image->native && sw==dw && sh==dh) {
            // [CHOSEN, not recovered] A same-size copy out of a SYSTEMMEM surface (UpdateSurface)
            // uploads just the rectangle. Reading the whole destination back and writing all of
            // it again cost ~90% of an in-game frame with nametags.
            if(!a.read()) return D3DERR_DEVICELOST;      // SYSTEMMEM: sizes the CPU storage only
            const unsigned char* from=a.bytes.data()+(sr.top/block)*a.pitch()+(sr.left/block)*size;
            return bf2metal::uploadRegion(native,b.image->native,b.level,b.slice,dr.left,dr.top,sw,sh,from,a.pitch())
                ?D3D_OK:D3DERR_DEVICELOST;
        }
        if(!a.read() || !b.read()) return D3DERR_DEVICELOST;
        if(sw==dw && sh==dh) {
            for(unsigned y=0;y<(sh+block-1)/block;++y)
                std::memcpy(b.bytes.data()+(dr.top/block+y)*b.pitch()+(dr.left/block)*size,
                    a.bytes.data()+(sr.top/block+y)*a.pitch()+(sr.left/block)*size,((sw+block-1)/block)*size);
        } else {
            if(block>1) return D3DERR_INVALIDCALL; // D3D9: compressed surfaces cannot be stretched
            if(!bf2metal::resample(a.format,a.bytes.data()+sr.top*a.pitch()+sr.left*size,sw,sh,a.pitch(),
                b.bytes.data()+dr.top*b.pitch()+dr.left*size,dw,dh,b.pitch(),filter==D3DTEXF_LINEAR)) {
                bf2gfx::unsupported("IDirect3DDevice9::StretchRect scaling format");
                return D3DERR_INVALIDCALL;
            }
        }
        return b.write()?D3D_OK:D3DERR_DEVICELOST;
    }
    HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DSurface9* a,const RECT* ar,IDirect3DSurface9* b,const RECT* br,D3DTEXTUREFILTERTYPE f) override {
        mainThread("IDirect3DDevice9::StretchRect");
        // Windows rules (Wine test_stretch_rect/stretchrect_test): DEFAULT pool only, never into
        // a plain texture, an offscreen-plain destination takes only a same-size offscreen-plain
        // source, filters beyond LINEAR are refused. Caps advertise CAN_STRETCHRECT_FROM_TEXTURES.
        auto* s=dynamic_cast<Surface*>(a); auto* d=dynamic_cast<Surface*>(b);
        if(!s || !d || f>D3DTEXF_LINEAR || s->desc.Pool!=D3DPOOL_DEFAULT || d->desc.Pool!=D3DPOOL_DEFAULT) return D3DERR_INVALIDCALL;
        if(!((s->desc.Usage|d->desc.Usage)&D3DUSAGE_DEPTHSTENCIL)) {
            const bool targetDest=(d->desc.Usage&D3DUSAGE_RENDERTARGET)!=0;
            if(!targetDest) {
                RECT sr,dr;
                if(d->container || s->container || (s->desc.Usage&D3DUSAGE_RENDERTARGET) ||
                    !rectangle(ar,*s->plane,sr) || !rectangle(br,*d->plane,dr) ||
                    sr.right-sr.left!=dr.right-dr.left || sr.bottom-sr.top!=dr.bottom-dr.top) return D3DERR_INVALIDCALL;
            }
        }
        return copySurface(a,ar,b,br,f);
    }
    HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DSurface9* a,const RECT* ar,IDirect3DSurface9* b,const POINT* point) override {
        mainThread("IDirect3DDevice9::UpdateSurface"); auto* s=dynamic_cast<Surface*>(a); auto* d=dynamic_cast<Surface*>(b);
        if(!s || !d || s->desc.Pool!=D3DPOOL_SYSTEMMEM || d->desc.Pool!=D3DPOOL_DEFAULT) return D3DERR_INVALIDCALL;
        RECT r; if(!rectangle(ar,*s->plane,r)) return D3DERR_INVALIDCALL;
        POINT p=point?*point:POINT{0,0};
        if(p.x<0 || p.y<0 || unsigned(p.x)>d->desc.Width || unsigned(p.y)>d->desc.Height ||
            unsigned(r.right-r.left)>d->desc.Width-unsigned(p.x) || unsigned(r.bottom-r.top)>d->desc.Height-unsigned(p.y)) return D3DERR_INVALIDCALL;
        RECT dr={p.x,p.y,p.x+(r.right-r.left),p.y+(r.bottom-r.top)};
        return copySurface(a,&r,b,&dr,D3DTEXF_NONE);
    }
    HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DSurface9* a,IDirect3DSurface9* b) override {
        mainThread("IDirect3DDevice9::GetRenderTargetData"); auto* s=dynamic_cast<Surface*>(a); auto* d=dynamic_cast<Surface*>(b);
        if(!s || !d || !(s->desc.Usage&D3DUSAGE_RENDERTARGET) || d->desc.Pool!=D3DPOOL_SYSTEMMEM ||
            s->desc.Width!=d->desc.Width || s->desc.Height!=d->desc.Height || s->desc.Format!=d->desc.Format ||
            s->desc.MultiSampleType!=D3DMULTISAMPLE_NONE) return D3DERR_INVALIDCALL; // D3D9: "the render target is multisampled"
        return copySurface(a,nullptr,b,nullptr,D3DTEXF_NONE);
    }
