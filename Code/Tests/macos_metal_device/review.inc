// [TEMP] [CHOSEN, not recovered] c-metal-review regression checks.
// Expected values follow Windows D3D9 results recorded by Wine's d3d9 conformance tests.

static int reviewLockChecks(IDirect3DDevice9* d) {
    const DWORD noSysLock=0x800; // D3DLOCK_NOSYSLOCK: ParticleRenderer* lock with 0x2800/0x1800.
    void* bytes=nullptr;
    IDirect3DVertexBuffer9* v=nullptr;
    CHECK(d->CreateVertexBuffer(64,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&v,nullptr)==D3D_OK);
    CHECK(v->Lock(0,0,&bytes,D3DLOCK_DISCARD|noSysLock)==D3D_OK && bytes);
    std::memset(bytes,0x31,64); CHECK(v->Unlock()==D3D_OK);
    CHECK(v->Lock(16,16,&bytes,D3DLOCK_NOOVERWRITE|noSysLock)==D3D_OK && bytes && v->Unlock()==D3D_OK);
    // Windows 7 (test_vb_lock_flags): these succeed on a DYNAMIC|WRITEONLY buffer.
    for(DWORD f:{D3DLOCK_READONLY,D3DLOCK_NOOVERWRITE|D3DLOCK_DISCARD,D3DLOCK_NOOVERWRITE|D3DLOCK_READONLY}) {
        bytes=nullptr; CHECK(v->Lock(0,0,&bytes,f)==D3D_OK && bytes && v->Unlock()==D3D_OK);
    }
    CHECK(v->Lock(0,0,&bytes,D3DLOCK_NOOVERWRITE)==D3D_OK && static_cast<unsigned char*>(bytes)[63]==0x31);
    CHECK(v->Unlock()==D3D_OK);
    CHECK(v->Lock(0,0,&bytes,D3DLOCK_READONLY|D3DLOCK_DISCARD)==D3DERR_INVALIDCALL);
    CHECK(v->Release()==0);
    // DISCARD/NOOVERWRITE on a static buffer are ignored, never an error; contents survive.
    IDirect3DIndexBuffer9* index=nullptr;
    CHECK(d->CreateIndexBuffer(32,0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&index,nullptr)==D3D_OK);
    CHECK(index->Lock(0,0,&bytes,0)==D3D_OK); std::memset(bytes,0x42,32); CHECK(index->Unlock()==D3D_OK);
    CHECK(index->Lock(0,0,&bytes,D3DLOCK_DISCARD)==D3D_OK && static_cast<unsigned char*>(bytes)[31]==0x42);
    CHECK(index->Unlock()==D3D_OK);
    CHECK(index->Lock(0,0,&bytes,D3DLOCK_NOOVERWRITE|noSysLock)==D3D_OK && index->Unlock()==D3D_OK);
    CHECK(index->Release()==0);
    // Texture/surface locks: NOSYSLOCK is a documented no-op; DISCARD on a non-dynamic
    // SYSTEMMEM surface succeeds on Windows (Wine test_reset).
    IDirect3DTexture9* t=nullptr; D3DLOCKED_RECT lock={};
    CHECK(d->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&t,nullptr)==D3D_OK);
    CHECK(t->LockRect(0,&lock,nullptr,noSysLock)==D3D_OK && lock.pBits && t->UnlockRect(0)==D3D_OK);
    CHECK(t->Release()==0);
    IDirect3DSurface9* cpu=nullptr;
    CHECK(d->CreateOffscreenPlainSurface(16,16,D3DFMT_R5G6B5,D3DPOOL_SYSTEMMEM,&cpu,nullptr)==D3D_OK);
    CHECK(cpu->LockRect(&lock,nullptr,D3DLOCK_DISCARD)==D3D_OK && lock.pBits && cpu->UnlockRect()==D3D_OK);
    CHECK(cpu->Release()==0);
    std::puts("PASS: review lock flags (NOSYSLOCK, Windows flag combinations, static DISCARD)"); return 0;
}

static int reviewQueryChecks(IDirect3DDevice9* d) {
    // Windows (Wine test_occlusion_query/test_query_support): a never-issued query answers S_OK,
    // repeated BEGIN and END without BEGIN succeed, short/long GetData sizes are accepted, and
    // unsupported types answer NOTAVAILABLE. A recreated, never-issued fence after Reset must not
    // keep RendDX9's queryWaitDone spinning on S_FALSE.
    const DWORD issueBegin=2; const auto timestamp=static_cast<D3DQUERYTYPE>(10);
    for(auto type:{D3DQUERYTYPE_EVENT,D3DQUERYTYPE_OCCLUSION}) {
        IDirect3DQuery9* q=nullptr; CHECK(d->CreateQuery(type,&q)==D3D_OK);
        CHECK(q->GetData(nullptr,0,D3DGETDATA_FLUSH)==S_OK);
        if(type==D3DQUERYTYPE_OCCLUSION) {
            CHECK(q->Issue(D3DISSUE_END)==D3D_OK);
            CHECK(q->Issue(issueBegin)==D3D_OK && q->Issue(issueBegin)==D3D_OK);
            DWORD pending=7; CHECK(q->GetData(&pending,4,0)==S_FALSE);
            CHECK(q->Issue(D3DISSUE_END)==D3D_OK && q->Issue(D3DISSUE_END)==D3D_OK);
        } else CHECK(q->Issue(D3DISSUE_END)==D3D_OK);
        // An event query's Issue submits without waiting for the GPU (frames in flight): like
        // RendDX9's queryWaitDone, poll until the GPU has reached it.
        if(type==D3DQUERYTYPE_EVENT) while(q->GetData(nullptr,0,D3DGETDATA_FLUSH)==S_FALSE) {}
        DWORD data[2]={0xffffffffu,0xffffffffu};
        CHECK(q->GetData(data,2,D3DGETDATA_FLUSH)==S_OK && (data[0]>>16)==0xffffu);
        CHECK(q->GetData(data,sizeof(data),D3DGETDATA_FLUSH)==S_OK);
        CHECK(data[0]==(type==D3DQUERYTYPE_EVENT?1u:0u) && q->Release()==0);
    }
    IDirect3DQuery9* q=reinterpret_cast<IDirect3DQuery9*>(0x1);
    CHECK(d->CreateQuery(timestamp,nullptr)==D3DERR_NOTAVAILABLE);
    CHECK(d->CreateQuery(timestamp,&q)==D3DERR_NOTAVAILABLE);
    std::puts("PASS: review queries (unissued S_OK, BEGIN/END repeats, data sizes, NOTAVAILABLE)"); return 0;
}

static int reviewViewportChecks(IDirect3DDevice9* d) {
    // Windows accepts viewports extending past the render target and MinZ > MaxZ (Wine visual
    // test_viewport passes there with 1280x960/2000x1600 and offset viewports on 640x480). A
    // refusal would leave the previous viewport silently in effect.
    D3DVIEWPORT9 old={}, vp={}, got={};
    CHECK(d->GetViewport(&old)==D3D_OK);
    for(const D3DVIEWPORT9& v:{D3DVIEWPORT9{0,0,old.Width*4,old.Height*4,0.0f,1.0f},
        D3DVIEWPORT9{100,100,old.Width,old.Height,0.0f,1.0f},D3DVIEWPORT9{0,0,old.Width,old.Height,0.5f,0.0f}}) {
        vp=v; CHECK(d->SetViewport(&vp)==D3D_OK && d->GetViewport(&got)==D3D_OK);
        CHECK(got.X==vp.X && got.Y==vp.Y && got.Width==vp.Width && got.Height==vp.Height && got.MinZ==vp.MinZ);
    }
    CHECK(d->SetViewport(&old)==D3D_OK);
    std::puts("PASS: review viewport (beyond target and inverted depth range accepted)"); return 0;
}

static int reviewStretchChecks(IDirect3DDevice9* d) {
    // Windows StretchRect rules (Wine test_stretch_rect and stretchrect_test, D3DPOOL_DEFAULT
    // unless noted): only DEFAULT surfaces; never into a plain texture; an offscreen-plain
    // destination takes only a same-size offscreen-plain source; filters beyond LINEAR refused.
    // DEFAULT texture -> render target is allowed with D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES.
    IDirect3DSurface9 *rt=nullptr,*rt2=nullptr,*off=nullptr,*off2=nullptr,*offBig=nullptr,*sys=nullptr,*tex=nullptr,*texManaged=nullptr;
    IDirect3DTexture9 *plain=nullptr,*managed=nullptr;
    CHECK(d->CreateRenderTarget(32,32,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&rt,nullptr)==D3D_OK);
    CHECK(d->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&rt2,nullptr)==D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(32,32,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&off,nullptr)==D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(32,32,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&off2,nullptr)==D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(64,64,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&offBig,nullptr)==D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(32,32,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&sys,nullptr)==D3D_OK);
    CHECK(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&plain,nullptr)==D3D_OK && plain->GetSurfaceLevel(0,&tex)==D3D_OK);
    CHECK(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&managed,nullptr)==D3D_OK && managed->GetSurfaceLevel(0,&texManaged)==D3D_OK);
    CHECK(d->StretchRect(rt,nullptr,rt2,nullptr,D3DTEXF_NONE)==D3D_OK);
    CHECK(d->StretchRect(off,nullptr,rt2,nullptr,D3DTEXF_LINEAR)==D3D_OK);
    CHECK(d->StretchRect(off,nullptr,off2,nullptr,D3DTEXF_NONE)==D3D_OK);
    CHECK(d->StretchRect(tex,nullptr,rt,nullptr,D3DTEXF_NONE)==D3D_OK);
    CHECK(d->StretchRect(rt,nullptr,tex,nullptr,D3DTEXF_NONE)==D3DERR_INVALIDCALL);
    CHECK(d->StretchRect(off,nullptr,tex,nullptr,D3DTEXF_NONE)==D3DERR_INVALIDCALL);
    CHECK(d->StretchRect(rt,nullptr,off,nullptr,D3DTEXF_NONE)==D3DERR_INVALIDCALL);
    CHECK(d->StretchRect(tex,nullptr,off,nullptr,D3DTEXF_NONE)==D3DERR_INVALIDCALL);
    CHECK(d->StretchRect(off,nullptr,offBig,nullptr,D3DTEXF_NONE)==D3DERR_INVALIDCALL);
    CHECK(d->StretchRect(sys,nullptr,rt,nullptr,D3DTEXF_NONE)==D3DERR_INVALIDCALL);
    CHECK(d->StretchRect(rt,nullptr,sys,nullptr,D3DTEXF_NONE)==D3DERR_INVALIDCALL);
    CHECK(d->StretchRect(texManaged,nullptr,rt,nullptr,D3DTEXF_NONE)==D3DERR_INVALIDCALL);
    for(DWORD f:{3u,6u,7u,0xbadfu})
        CHECK(d->StretchRect(off,nullptr,rt,nullptr,static_cast<D3DTEXTUREFILTERTYPE>(f))==D3DERR_INVALIDCALL);
    D3DCAPS9 caps={}; CHECK(d->GetDeviceCaps(&caps)==D3D_OK && (caps.DevCaps2&0x10));
    for(IUnknown* u:{static_cast<IUnknown*>(rt),static_cast<IUnknown*>(rt2),static_cast<IUnknown*>(off),
        static_cast<IUnknown*>(off2),static_cast<IUnknown*>(offBig),static_cast<IUnknown*>(sys),
        static_cast<IUnknown*>(tex),static_cast<IUnknown*>(texManaged)}) u->Release();
    CHECK(plain->Release()==0 && managed->Release()==0);
    std::puts("PASS: review StretchRect pool/usage/filter rules"); return 0;
}
