// [CHOSEN, not recovered] IDirect3DDevice9 methods outside the programmable path BF2 draws with.
// Included in class Device. Each follows the public D3D9 contract where that is cheap: the
// fixed-function vertex state (transforms, material, lights, clip status, palettes, FVF, N-patch
// mode, software vertex processing) is stored and read back; draws without a vertex shader read the
// transforms and the FVF declaration (FixedFunctionDraw.inc, 0.3.0 piece 1). The rest answer what a D3D9 device without the
// capability answers, and the first call of each is logged (Unsupported.h). The tree calls only
// SetTransform (LoadingScreen::draw, identity matrices) and CreateAdditionalSwapChain (one per
// extra window; the game creates one window) among them; imgui's DX9 backend is not built here.
    std::map<DWORD,D3DMATRIX> transforms;
    D3DMATERIAL9 material={};
    static constexpr std::size_t kLightBytes=104; // sizeof(D3DLIGHT9): Type, three colours, two vectors, seven floats
    std::map<DWORD,std::array<unsigned char,kLightBytes>> lights;
    std::map<DWORD,BOOL> lightsEnabled;
    D3DCLIPSTATUS9 clipStatus={};
    std::map<UINT,std::array<PALETTEENTRY,256>> palettes;
    UINT currentPalette=0xffff;
    BOOL softwareVertexProcessing=FALSE;
    float nPatchSegments=0.0f;
    DWORD fvf=0;
    D3DGAMMARAMP gamma={};
    bool gammaSet=false;
    BOOL cursorVisible=FALSE;
    static bool transformType(DWORD t) { return t==D3DTS_VIEW || t==D3DTS_PROJECTION || (t>=16 && t<=23) || (t>=256 && t<=511); }
    static D3DMATRIX identityMatrix() { D3DMATRIX m={}; m._11=m._22=m._33=m._44=1.0f; return m; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        static const GUID iidDevice={0xd0223b96,0xbf7a,0x43fd,{0x92,0xbd,0xa4,0x3b,0x0d,0x82,0xb9,0xeb}};
        if(!sameIID(id,iidUnknown) && !sameIID(id,iidDevice)) return E_NOINTERFACE;
        *out=static_cast<IDirect3DDevice9*>(this); AddRef(); return D3D_OK;
    }
    // [CHOSEN, not recovered] Metal does not report free video memory; this answers the GPU's
    // recommended working set (MetalBridge), as D3D9 answers an approximate figure.
    UINT STDMETHODCALLTYPE GetAvailableTextureMem() override { return bf2metal::availableTextureMemory(native); }
    HRESULT STDMETHODCALLTYPE EvictManagedResources() override { return D3D_OK; } // no managed residency on Metal
    // [CHOSEN, not recovered] The hardware cursor is not drawn: BF2 draws its own cursor. The
    // properties are accepted and the visibility is tracked so ShowCursor answers as D3D9 does.
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT,UINT,IDirect3DSurface9* bitmap) override {
        if(!bitmap) return D3DERR_INVALIDCALL; bf2gfx::unsupported("IDirect3DDevice9::SetCursorProperties (hardware cursor not drawn)"); return D3D_OK;
    }
    void STDMETHODCALLTYPE SetCursorPosition(int,int,DWORD) override {}
    BOOL STDMETHODCALLTYPE ShowCursor(BOOL show) override { const BOOL old=cursorVisible; cursorVisible=show; return old; }
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*,IDirect3DSwapChain9** out) override {
        if(out) *out=nullptr;
        bf2gfx::unsupported("IDirect3DDevice9::CreateAdditionalSwapChain (one window per device)");
        return D3DERR_NOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE GetRasterStatus(UINT chainIndex,D3DRASTER_STATUS* out) override {
        if(chainIndex || !out) return D3DERR_INVALIDCALL; *out={FALSE,0}; return D3D_OK; // no scan-line position on Metal
    }
    HRESULT STDMETHODCALLTYPE SetDialogBoxMode(BOOL) override { return D3D_OK; } // GDI dialogs over the device do not exist here
    // [CHOSEN, not recovered] The ramp is stored and read back but not applied to the display.
    // Before a SetGammaRamp, GetGammaRamp answers the identity ramp.
    void STDMETHODCALLTYPE SetGammaRamp(UINT chainIndex,DWORD,const D3DGAMMARAMP* ramp) override {
        if(chainIndex || !ramp) return;
        bf2gfx::unsupported("IDirect3DDevice9::SetGammaRamp (stored, not applied)"); gamma=*ramp; gammaSet=true;
    }
    void STDMETHODCALLTYPE GetGammaRamp(UINT chainIndex,D3DGAMMARAMP* ramp) override {
        if(chainIndex || !ramp) return;
        if(!gammaSet) for(unsigned i=0;i<256;++i) gamma.red[i]=gamma.green[i]=gamma.blue[i]=WORD(i*257);
        *ramp=gamma;
    }
    // D3D9 semantics for 2D and cube textures: every level of the SYSTEMMEM source that the
    // DEFAULT destination has (matched from the smallest level up) is copied with UpdateSurface.
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture9* source,IDirect3DBaseTexture9* dest) override {
        mainThread("IDirect3DDevice9::UpdateTexture");
        if(!source || !dest || source->GetType()!=dest->GetType()) return D3DERR_INVALIDCALL;
        const D3DRESOURCETYPE type=source->GetType();
        if(type!=D3DRTYPE_TEXTURE && type!=D3DRTYPE_CUBETEXTURE) {
            bf2gfx::unsupported("IDirect3DDevice9::UpdateTexture of a volume texture"); return D3DERR_INVALIDCALL;
        }
        const UINT sl=source->GetLevelCount(), dl=dest->GetLevelCount();
        if(sl<dl) return D3DERR_INVALIDCALL;
        const UINT faces=type==D3DRTYPE_CUBETEXTURE?6:1;
        for(UINT face=0;face<faces;++face) for(UINT l=0;l<dl;++l) {
            IDirect3DSurface9* s=nullptr; IDirect3DSurface9* d=nullptr; HRESULT hr;
            if(type==D3DRTYPE_TEXTURE) {
                hr=static_cast<IDirect3DTexture9*>(source)->GetSurfaceLevel(l+sl-dl,&s);
                if(SUCCEEDED(hr)) hr=static_cast<IDirect3DTexture9*>(dest)->GetSurfaceLevel(l,&d);
            } else {
                hr=static_cast<IDirect3DCubeTexture9*>(source)->GetCubeMapSurface(D3DCUBEMAP_FACES(face),l+sl-dl,&s);
                if(SUCCEEDED(hr)) hr=static_cast<IDirect3DCubeTexture9*>(dest)->GetCubeMapSurface(D3DCUBEMAP_FACES(face),l,&d);
            }
            if(SUCCEEDED(hr)) {
                D3DSURFACE_DESC a={},b={}; s->GetDesc(&a); d->GetDesc(&b);
                hr=a.Width!=b.Width || a.Height!=b.Height || a.Format!=b.Format ? D3DERR_INVALIDCALL : UpdateSurface(s,nullptr,d,nullptr);
            }
            if(s) s->Release(); if(d) d->Release();
            if(FAILED(hr)) return hr;
        }
        return D3D_OK;
    }
    // [CHOSEN, not recovered] Metal exposes no front buffer to read (no BF2 caller; the
    // screenshot reads the back buffer).
    HRESULT STDMETHODCALLTYPE GetFrontBufferData(UINT,IDirect3DSurface9*) override {
        bf2gfx::unsupported("IDirect3DDevice9::GetFrontBufferData (no front buffer on Metal)"); return D3DERR_INVALIDCALL;
    }
    // D3D9 semantics for DEFAULT-pool render-target surfaces (a GPU clear of the rectangle);
    // [CHOSEN, not recovered] other surfaces are refused (no BF2 caller).
    HRESULT STDMETHODCALLTYPE ColorFill(IDirect3DSurface9* target,const RECT* r,D3DCOLOR color) override {
        mainThread("IDirect3DDevice9::ColorFill");
        auto* s=dynamic_cast<Surface*>(target);
        if(!s || s->owner!=this || s->desc.Pool!=D3DPOOL_DEFAULT || s->plane->locked) return D3DERR_INVALIDCALL;
        if(!(s->desc.Usage&D3DUSAGE_RENDERTARGET) || s->plane->samples>1 || !s->plane->image->native) {
            bf2gfx::unsupported("IDirect3DDevice9::ColorFill of a surface that is not a single-sample render target"); return D3DERR_INVALIDCALL;
        }
        RECT rect; if(!rectangle(r,*s->plane,rect)) return D3DERR_INVALIDCALL;
        if(s->desc.Format==D3DFMT_X8R8G8B8 || s->desc.Format==D3DFMT_X8B8G8R8) color|=0xff000000u;
        const bf2metal::ClearRect region={unsigned(rect.left),unsigned(rect.top),unsigned(rect.right-rect.left),unsigned(rect.bottom-rect.top)};
        return bf2metal::clearTargets(native,s->plane->image->native,s->plane->level,s->plane->slice,nullptr,0,0,
            D3DCLEAR_TARGET,color,0.0f,0,&region,1) ? D3D_OK : D3DERR_DEVICELOST;
    }
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE t,const D3DMATRIX* m) override {
        if(!m || !transformType(t)) return D3DERR_INVALIDCALL; transforms[t]=*m; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE t,D3DMATRIX* m) override {
        if(!m || !transformType(t)) return D3DERR_INVALIDCALL;
        const auto it=transforms.find(t); *m=it==transforms.end()?identityMatrix():it->second; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE t,const D3DMATRIX* m) override {
        if(!m || !transformType(t)) return D3DERR_INVALIDCALL;
        D3DMATRIX current; GetTransform(t,&current);
        D3DMATRIX r={}; // D3D9: the new state is m x current
        for(int i=0;i<4;++i) for(int j=0;j<4;++j) {
            float v=0; for(int k=0;k<4;++k) v+=m->m[i][k]*current.m[k][j]; r.m[i][j]=v;
        }
        transforms[t]=r; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL9* m) override { if(!m) return D3DERR_INVALIDCALL; material=*m; return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL9* m) override { if(!m) return D3DERR_INVALIDCALL; *m=material; return D3D_OK; }
    HRESULT STDMETHODCALLTYPE SetLight(DWORD i,const D3DLIGHT9* l) override {
        if(!l) return D3DERR_INVALIDCALL; std::memcpy(lights[i].data(),l,kLightBytes); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetLight(DWORD i,D3DLIGHT9* l) override {
        const auto it=lights.find(i); if(!l || it==lights.end()) return D3DERR_INVALIDCALL;
        std::memcpy(l,it->second.data(),kLightBytes); return D3D_OK;
    }
    // D3D9: enabling a light that was never set gives it the default directional light.
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD i,BOOL on) override {
        if(!lights.count(i)) {
            std::array<unsigned char,kLightBytes> l={};
            const DWORD directional=3; const float one=1.0f; std::memcpy(l.data(),&directional,4);
            for(unsigned c=0;c<3;++c) std::memcpy(l.data()+4+c*4,&one,4);          // Diffuse 1,1,1,0
            std::memcpy(l.data()+4+48+12+8,&one,4);                                 // Direction 0,0,1
            lights[i]=l;
        }
        lightsEnabled[i]=on?TRUE:FALSE; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD i,BOOL* on) override {
        const auto it=lightsEnabled.find(i); if(!on || !lights.count(i)) return D3DERR_INVALIDCALL;
        *on=it!=lightsEnabled.end() && it->second; return D3D_OK;
    }
    // [CHOSEN, not recovered] State blocks are not modelled (no BF2 caller: the engine's
    // StateManager keeps its own blocks). Refused as by a device that cannot record them.
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE,IDirect3DStateBlock9** out) override {
        if(out) *out=nullptr; bf2gfx::unsupported("IDirect3DDevice9::CreateStateBlock (state blocks not modelled)"); return D3DERR_INVALIDCALL;
    }
    HRESULT STDMETHODCALLTYPE BeginStateBlock() override {
        bf2gfx::unsupported("IDirect3DDevice9::BeginStateBlock (state blocks not modelled)"); return D3DERR_INVALIDCALL;
    }
    HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9** out) override {
        if(out) *out=nullptr; return D3DERR_INVALIDCALL; // no block is being recorded
    }
    HRESULT STDMETHODCALLTYPE SetClipStatus(const D3DCLIPSTATUS9* s) override { if(!s) return D3DERR_INVALIDCALL; clipStatus=*s; return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS9* s) override { if(!s) return D3DERR_INVALIDCALL; *s=clipStatus; return D3D_OK; }
    // Every pass this device can draw is a single pass (D3D9: one pass on success).
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* passes) override { if(!passes) return D3DERR_INVALIDCALL; *passes=1; return D3D_OK; }
    // Palettes: stored and read back (no P8 format is offered, so none is sampled).
    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT n,const PALETTEENTRY* e) override {
        if(!e || n>0xffff) return D3DERR_INVALIDCALL; std::copy(e,e+256,palettes[n].begin()); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT n,PALETTEENTRY* e) override {
        const auto it=palettes.find(n); if(!e || it==palettes.end()) return D3DERR_INVALIDCALL;
        std::copy(it->second.begin(),it->second.end(),e); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT n) override {
        if(!palettes.count(n)) return D3DERR_INVALIDCALL; currentPalette=n; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT* n) override { if(!n) return D3DERR_INVALIDCALL; *n=currentPalette; return D3D_OK; }
    // D3D9: only a mixed-vertex-processing device may switch; this device is created hardware-only.
    HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(BOOL on) override {
        if(on && !(creation.BehaviorFlags&0x80)) return D3DERR_INVALIDCALL; // D3DCREATE_MIXED_VERTEXPROCESSING
        softwareVertexProcessing=on?TRUE:FALSE; return D3D_OK;
    }
    BOOL STDMETHODCALLTYPE GetSoftwareVertexProcessing() override { return softwareVertexProcessing; }
    HRESULT STDMETHODCALLTYPE SetNPatchMode(float segments) override { nPatchSegments=segments; return D3D_OK; }
    float STDMETHODCALLTYPE GetNPatchMode() override { return nPatchSegments; }
    // [CHOSEN, not recovered] Software vertex processing into a buffer is not modelled (no BF2
    // caller); refused as on a hardware-only device.
    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT a,UINT b,UINT n,IDirect3DVertexBuffer9* d,IDirect3DVertexDeclaration9* o,DWORD f) override {
        mainThread("IDirect3DDevice9::ProcessVertices"); return processVertices(a,b,n,d,o,f); // FixedFunctionDraw.inc
    }
    // The FVF is stored; a draw without a vertex declaration refuses itself (DeviceDraw.inc).
    HRESULT STDMETHODCALLTYPE SetFVF(DWORD f) override { mainThread("IDirect3DDevice9::SetFVF"); return setFvf(f); } // FixedFunctionDraw.inc
    HRESULT STDMETHODCALLTYPE GetFVF(DWORD* f) override { if(!f) return D3DERR_INVALIDCALL; *f=fvf; return D3D_OK; }
    // No patch caps are advertised (DevCaps has no RTPATCHES/NPATCHES), so D3D9 refuses patches.
    HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT,const float*,const D3DRECTPATCH_INFO*) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT,const float*,const D3DTRIPATCH_INFO*) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE DeletePatch(UINT) override { return D3DERR_INVALIDCALL; }
