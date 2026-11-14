// [CHOSEN, not recovered] State setters record D3D values. Unsupported values stop only at draws.
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE n,DWORD v) override {
        mainThread("IDirect3DDevice9::SetRenderState");
        if(!unsigned(n)) return D3DERR_INVALIDCALL; // StateManager's pre-vendor-probe zero ID.
        if(StateCache::renderID(n)) state.raw.render[n]=v; else state.extraRender[n]=v; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE n,DWORD* out) override {
        if(!out || !unsigned(n)) return D3DERR_INVALIDCALL;
        if(StateCache::renderID(n)) *out=state.raw.render[n];
        else { auto it=state.extraRender.find(n); if(it==state.extraRender.end()) return D3DERR_INVALIDCALL; *out=it->second; }
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD s,D3DSAMPLERSTATETYPE n,DWORD v) override {
        mainThread("IDirect3DDevice9::SetSamplerState"); const int i=StateCache::samplerIndex(s);
        if(i<0) return D3DERR_INVALIDCALL;
        if(unsigned(n)>=1 && unsigned(n)<14) state.raw.sampler[i][n]=v; else state.extraSampler[{s,n}]=v; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetSamplerState(DWORD s,D3DSAMPLERSTATETYPE n,DWORD* out) override {
        const int i=StateCache::samplerIndex(s); if(i<0 || !out) return D3DERR_INVALIDCALL;
        if(unsigned(n)>=1 && unsigned(n)<14) *out=state.raw.sampler[i][n];
        else { auto it=state.extraSampler.find({s,n}); if(it==state.extraSampler.end()) return D3DERR_INVALIDCALL; *out=it->second; }
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD s,D3DTEXTURESTAGESTATETYPE n,DWORD v) override {
        mainThread("IDirect3DDevice9::SetTextureStageState"); if(s>=8) return D3DERR_INVALIDCALL;
        if(StateCache::stageID(n)) state.raw.stage[s][n]=v; else state.extraStage[{s,n}]=v; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD s,D3DTEXTURESTAGESTATETYPE n,DWORD* out) override {
        if(s>=8 || !out) return D3DERR_INVALIDCALL;
        if(StateCache::stageID(n)) *out=state.raw.stage[s][n];
        else { auto it=state.extraStage.find({s,n}); if(it==state.extraStage.end()) return D3DERR_INVALIDCALL; *out=it->second; }
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD s,IDirect3DBaseTexture9* t) override {
        mainThread("IDirect3DDevice9::SetTexture"); const int i=StateCache::samplerIndex(s); if(i<0) return D3DERR_INVALIDCALL;
        auto* life=dynamic_cast<Life*>(t); if(t && (!life || life->owner!=this)) return D3DERR_INVALIDCALL;
        state.textures[i].set(t); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD s,IDirect3DBaseTexture9** out) override {
        const int i=StateCache::samplerIndex(s); return i<0?D3DERR_INVALIDCALL:state.textures[i].get(out);
    }
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT n,IDirect3DVertexBuffer9* p,UINT offset,UINT stride) override {
        mainThread("IDirect3DDevice9::SetStreamSource"); if(n>=16) return D3DERR_INVALIDCALL;
        auto* b=dynamic_cast<VertexBuffer*>(p); if(p && (!b || b->owner!=this || offset>=b->desc.Size || stride>255)) return D3DERR_INVALIDCALL;
        state.streams[n].set(p); state.offsets[n]=offset; state.strides[n]=stride; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT n,IDirect3DVertexBuffer9** p,UINT* offset,UINT* stride) override {
        if(n>=16 || !offset || !stride || !p) return D3DERR_INVALIDCALL;
        *offset=state.offsets[n]; *stride=state.strides[n]; return state.streams[n].get(p);
    }
    HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(UINT n,UINT f) override {
        if(n>=16 || !f) return D3DERR_INVALIDCALL; state.frequencies[n]=f; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(UINT n,UINT* f) override {
        if(n>=16 || !f) return D3DERR_INVALIDCALL; *f=state.frequencies[n]; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer9* p) override {
        mainThread("IDirect3DDevice9::SetIndices"); auto* b=dynamic_cast<IndexBuffer*>(p);
        if(p && (!b || b->owner!=this)) return D3DERR_INVALIDCALL; state.indices.set(p); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer9** out) override { return state.indices.get(out); }
    HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9* p) override {
        mainThread("IDirect3DDevice9::SetVertexDeclaration"); auto* d=dynamic_cast<Declaration*>(p);
        if(p && (!d || d->owner!=this)) return D3DERR_INVALIDCALL; state.declaration.set(p); fvf=0; return D3D_OK; // GetFVF: 0 after a declaration
    }
    HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DVertexDeclaration9** out) override { return state.declaration.get(out); }
    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9* p) override {
        mainThread("IDirect3DDevice9::SetVertexShader"); state.vs.set(p); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9** out) override { return state.vs.get(out); }
    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9* p) override {
        mainThread("IDirect3DDevice9::SetPixelShader"); state.ps.set(p); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9** out) override { return state.ps.get(out); }
    template<class T,unsigned N,unsigned Width> HRESULT constants(T (&data)[N][Width],UINT start,const T* values,UINT count) {
        if(start>N || count>N-start || (count && !values)) return D3DERR_INVALIDCALL;
        if(count) std::memcpy(data+start,values,count*Width*sizeof(T)); return D3D_OK;
    }
    template<class T,unsigned N,unsigned Width> HRESULT getConstants(T (&data)[N][Width],UINT start,T* values,UINT count) {
        if(start>N || count>N-start || (count && !values)) return D3DERR_INVALIDCALL;
        if(count) std::memcpy(values,data+start,count*Width*sizeof(T)); return D3D_OK;
    }
#define CONSTANTS(Stage,Prefix,Type,Array) \
    HRESULT STDMETHODCALLTYPE Set##Stage##ShaderConstant##Prefix(UINT s,const Type* v,UINT n) override { return constants(state.Array,s,v,n); } \
    HRESULT STDMETHODCALLTYPE Get##Stage##ShaderConstant##Prefix(UINT s,Type* v,UINT n) override { return getConstants(state.Array,s,v,n); }
    CONSTANTS(Vertex,F,float,vf)
    CONSTANTS(Pixel,F,float,pf)
    CONSTANTS(Vertex,I,int,vi)
    CONSTANTS(Pixel,I,int,pi)
#undef CONSTANTS
    HRESULT booleans(BOOL* data,UINT start,const BOOL* values,UINT count) {
        if(start>16 || count>16-start || (count && !values)) return D3DERR_INVALIDCALL;
        for(unsigned i=0;i<count;++i) data[start+i]=values[i]?TRUE:FALSE; return D3D_OK;
    }
    HRESULT getBooleans(BOOL* data,UINT start,BOOL* values,UINT count) {
        if(start>16 || count>16-start || (count && !values)) return D3DERR_INVALIDCALL;
        if(count) std::memcpy(values,data+start,count*sizeof(BOOL)); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT s,const BOOL* v,UINT n) override { return booleans(state.vb,s,v,n); }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT s,const BOOL* v,UINT n) override { return booleans(state.pb,s,v,n); }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(UINT s,BOOL* v,UINT n) override { return getBooleans(state.vb,s,v,n); }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(UINT s,BOOL* v,UINT n) override { return getBooleans(state.pb,s,v,n); }
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD n,const float* v) override {
        if(n>=6 || !v) return D3DERR_INVALIDCALL; std::memcpy(state.clip[n],v,16); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD n,float* v) override {
        if(n>=6 || !v) return D3DERR_INVALIDCALL; std::memcpy(v,state.clip[n],16); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetScissorRect(const RECT* r) override {
        if(!r || r->left<0 || r->top<0 || r->right<=r->left || r->bottom<=r->top) return D3DERR_INVALIDCALL;
        const auto& target=static_cast<Surface*>(colorTarget.ptr)->desc;
        if(unsigned(r->right)>target.Width || unsigned(r->bottom)>target.Height) return D3DERR_INVALIDCALL;
        state.scissor=*r; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetScissorRect(RECT* r) override { if(!r) return D3DERR_INVALIDCALL; *r=state.scissor; return D3D_OK; }
    // G4 draws: DeviceDraw.inc.
