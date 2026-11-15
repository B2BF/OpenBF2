// [CHOSEN, not recovered] G3b native COM storage and ownership. Included only in Device.cpp.
// Bindings retain objects internally without retaining the owning device, avoiding COM cycles.
struct Life {
    IDirect3DDevice9* owner;
    bf2metal::Frame* frame;
    ULONG refs=0, held=0;
    bool implicit=false;
    const char* iface;
    Life(IDirect3DDevice9* d,bf2metal::Frame* f,const char* name,bool in=false)
        : owner(d),frame(f),implicit(in),iface(name) {
        bf2metal::reftrace::born(this,name,[](const void* p) { return (unsigned long)static_cast<const Life*>(p)->refs; });
    }
    virtual ~Life() { bf2metal::reftrace::died(this); }
    void keep() { ++held; }
    void drop() { if (!--held && !refs && !implicit) delete this; }
    ULONG add() { mainThread(iface); { bf2metal::reftrace::Owned o; owner->AddRef(); } return ++refs; }
    ULONG release() {
        mainThread(iface); auto* d=owner; const ULONG n=--refs;
        if(!n && !held && !implicit) delete this;
        bf2metal::reftrace::Owned o; d->Release(); return n;
    }
    HRESULT device(IDirect3DDevice9** out) {
        if(!out) return D3DERR_INVALIDCALL; *out=owner; owner->AddRef(); return D3D_OK;
    }
};
// D3D9 private data (IDirect3DResource9::Set/Get/FreePrivateData): bytes per GUID, or with
// D3DSPD_IUNKNOWN an interface pointer the resource references until it is freed or replaced.
struct PrivateData {
    struct Entry { std::vector<unsigned char> bytes; IUnknown* object=nullptr; };
    std::map<std::string,Entry> entries;
    static std::string key(REFGUID id) { return std::string(reinterpret_cast<const char*>(&id),sizeof(GUID)); }
    ~PrivateData() { for(auto& e:entries) if(e.second.object) e.second.object->Release(); }
    HRESULT set(REFGUID id,const void* data,DWORD size,DWORD flags) {
        if(flags&~1u) return D3DERR_INVALIDCALL;
        Entry e;
        if(flags&1) { // D3DSPD_IUNKNOWN
            if(size!=sizeof(IUnknown*) || !data) return D3DERR_INVALIDCALL;
            std::memcpy(&e.object,data,sizeof(e.object)); if(!e.object) return D3DERR_INVALIDCALL;
            e.object->AddRef(); e.bytes.assign(static_cast<const unsigned char*>(data),static_cast<const unsigned char*>(data)+size);
        } else {
            if(!data && size) return D3DERR_INVALIDCALL;
            if(size) e.bytes.assign(static_cast<const unsigned char*>(data),static_cast<const unsigned char*>(data)+size);
        }
        free(id); entries[key(id)]=std::move(e); return D3D_OK;
    }
    HRESULT get(REFGUID id,void* data,DWORD* size) const {
        if(!size) return D3DERR_INVALIDCALL;
        const auto it=entries.find(key(id)); if(it==entries.end()) return D3DERR_NOTFOUND;
        const DWORD n=DWORD(it->second.bytes.size());
        if(!data) { *size=n; return D3D_OK; }
        if(*size<n) { *size=n; return D3DERR_MOREDATA; }
        if(it->second.object) it->second.object->AddRef(); // D3D9 hands out a reference
        if(n) std::memcpy(data,it->second.bytes.data(),n); *size=n; return D3D_OK;
    }
    HRESULT free(REFGUID id) {
        const auto it=entries.find(key(id)); if(it==entries.end()) return D3DERR_NOTFOUND;
        if(it->second.object) it->second.object->Release();
        entries.erase(it); return D3D_OK;
    }
};
template<class I> struct Binding {
    I* ptr=nullptr;
    Binding()=default;
    Binding(const Binding&)=delete;
    Binding& operator=(const Binding&)=delete;
    ~Binding() { set(nullptr); }
    void set(I* p) {
        if(p==ptr) return;
        if(p) { if(auto* l=dynamic_cast<Life*>(p)) l->keep(); else p->AddRef(); }
        I* old=ptr; ptr=p;
        if(old) { if(auto* l=dynamic_cast<Life*>(old)) l->drop(); else old->Release(); }
    }
    HRESULT get(I** out) const {
        if(!out) return D3DERR_INVALIDCALL; *out=ptr; if(ptr) ptr->AddRef(); return D3D_OK;
    }
};
static bool sameIID(REFIID a,const GUID& b) { return !std::memcmp(&a,&b,sizeof(GUID)); }
static const GUID iidUnknown={0,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID iidResource={0x05eec05d,0x8f7d,0x4362,{0xb9,0x99,0xd1,0xba,0xf3,0x57,0xc7,0x04}};
static const GUID iidBaseTexture={0x580ca87e,0x1d3c,0x4d54,{0x99,0x1d,0xb7,0xd3,0xe3,0xc2,0x98,0xce}};
static const GUID iidTexture={0x85c31227,0x3de5,0x4f00,{0x9b,0x3a,0xf1,0x1a,0xc3,0x8c,0x18,0xb5}};
static const GUID iidCube={0xfff32f81,0xd953,0x473a,{0x92,0x23,0x93,0xd6,0x52,0xab,0xa9,0x3f}};
static const GUID iidVolumeTexture={0x2518526c,0xe789,0x4111,{0xa7,0xb9,0x47,0xef,0x32,0x8d,0x13,0xe6}};
static const GUID iidSurface={0x0cfbaf3a,0x9ff6,0x429a,{0x99,0xb3,0xa2,0x79,0x6a,0xf8,0xb8,0x9b}};
static const GUID iidVertexBuffer={0xb64bb1b5,0xfd70,0x4df6,{0xbf,0x91,0x19,0xd0,0xa1,0x24,0x55,0xe3}};
static const GUID iidIndexBuffer={0x7c9dd65e,0xd3f7,0x4529,{0xac,0xee,0x78,0x58,0x30,0xac,0xde,0x35}};
static const GUID iidDeclaration={0xdd13c59c,0x36fa,0x4098,{0xa8,0xfb,0xc7,0xed,0x39,0xdc,0x85,0x46}};
static const GUID iidQuery={0xd9771460,0xa695,0x4f26,{0xbb,0xd3,0x27,0xb8,0x40,0xb5,0x41,0xcc}};
// D3DLOCK_NOSYSLOCK | D3DLOCK_DONOTWAIT | D3DLOCK_NO_DIRTY_UPDATE: valid D3D9 lock hints.
static constexpr DWORD lockHints=0x800|0x4000|0x8000;

template<class I> class Resource : public I, public Life {
public:
    D3DRESOURCETYPE kind;
    Resource(IDirect3DDevice9* d,bf2metal::Frame* f,const char* name,D3DRESOURCETYPE k,bool in=false)
        : Life(d,f,name,in),kind(k) {}
    ULONG STDMETHODCALLTYPE AddRef() override { return add(); }
    ULONG STDMETHODCALLTYPE Release() override { return release(); }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override { return device(out); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        if(sameIID(id,iidUnknown) || sameIID(id,iidResource) ||
            (kind==D3DRTYPE_TEXTURE && (sameIID(id,iidTexture)||sameIID(id,iidBaseTexture))) ||
            (kind==D3DRTYPE_CUBETEXTURE && (sameIID(id,iidCube)||sameIID(id,iidBaseTexture))) ||
            (kind==D3DRTYPE_VOLUMETEXTURE && (sameIID(id,iidVolumeTexture)||sameIID(id,iidBaseTexture))) ||
            (kind==D3DRTYPE_SURFACE && sameIID(id,iidSurface)) ||
            (kind==D3DRTYPE_VERTEXBUFFER && sameIID(id,iidVertexBuffer)) ||
            (kind==D3DRTYPE_INDEXBUFFER && sameIID(id,iidIndexBuffer))) {
            *out=static_cast<I*>(this); add(); return D3D_OK;
        }
        return E_NOINTERFACE;
    }
    PrivateData privateData;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID id,const void* data,DWORD size,DWORD flags) override { return privateData.set(id,data,size,flags); }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID id,void* data,DWORD* size) override { return privateData.get(id,data,size); }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID id) override { return privateData.free(id); }
    // No managed residency eviction exists in Metal. These are deliberately inert hints.
    DWORD STDMETHODCALLTYPE SetPriority(DWORD) override { return 0; }
    DWORD STDMETHODCALLTYPE GetPriority() override { return 0; }
    void STDMETHODCALLTYPE PreLoad() override {}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return kind; }
};
struct Image {
    bf2metal::Texture* native=nullptr;
    bool borrowed=false;
    ~Image() { if(native && !borrowed) bf2metal::destroyTexture(native); }
};
struct Plane {
    bf2metal::Frame* frame;
    std::shared_ptr<Image> image;
    unsigned w,h,d,level,slice;
    D3DFORMAT format;
    DWORD usage;
    D3DPOOL pool;
    bool locked=false;
    DWORD flags=0;
    std::vector<unsigned char> bytes;
    unsigned samples=1; // > 1: a multisampled render target or depth surface (DeviceResources.inc)
    unsigned block() const { return bf2metal::formatBlock(format); }
    unsigned pitch() const { return ((w+block()-1)/block())*bf2metal::formatBytes(format); }
    unsigned slicePitch() const { return pitch()*((h+block()-1)/block()); }
    bool read() {
        bytes.resize(std::size_t(slicePitch())*d);
        return !image->native || bf2metal::transfer(frame,image->native,level,slice,bytes.data(),pitch(),slicePitch(),false);
    }
    bool write() {
        return !image->native || bf2metal::transfer(frame,image->native,level,slice,bytes.data(),pitch(),slicePitch(),true);
    }
    D3DSURFACE_DESC desc() const {
        return {format,D3DRTYPE_SURFACE,usage,pool,samples>1?D3DMULTISAMPLE_TYPE(samples):D3DMULTISAMPLE_NONE,0,w,h};
    }
    HRESULT lock(D3DLOCKED_BOX* out,const D3DBOX* region,DWORD f) {
        mainThread("IDirect3DResource9::Lock");
        if(!out) return D3DERR_INVALIDCALL; *out={};
        f&=~lockHints; // Locks are synchronous; NOSYSLOCK/DONOTWAIT/NO_DIRTY_UPDATE change nothing.
        // Windows ignores DISCARD on a non-dynamic resource (Wine test_reset: SYSTEMMEM surface).
        if((f&D3DLOCK_DISCARD) && !(f&D3DLOCK_READONLY) && !(usage&D3DUSAGE_DYNAMIC)) f&=~D3DLOCK_DISCARD;
        if(locked || (f&~(D3DLOCK_READONLY|D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE)) ||
            (f&D3DLOCK_NOOVERWRITE) || ((f&D3DLOCK_READONLY)&&(f&D3DLOCK_DISCARD)) ||
            ((f&D3DLOCK_DISCARD) && (!(usage&D3DUSAGE_DYNAMIC) || region)) ||
            format==D3DFMT_D24S8 || format==D3DFMT_D16) return D3DERR_INVALIDCALL;
        D3DBOX b=region?*region:D3DBOX{0,0,w,h,0,d};
        if(b.Left>=b.Right || b.Top>=b.Bottom || b.Front>=b.Back || b.Right>w || b.Bottom>h || b.Back>d ||
            b.Left%block() || b.Top%block() || (b.Right!=w && b.Right%block()) || (b.Bottom!=h && b.Bottom%block())) return D3DERR_INVALIDCALL;
        if(f&D3DLOCK_DISCARD) bytes.assign(std::size_t(slicePitch())*d,0);
        else if(!read()) return D3DERR_DEVICELOST;
        locked=true; flags=f;
        out->RowPitch=pitch(); out->SlicePitch=slicePitch();
        out->pBits=bytes.data()+b.Front*slicePitch()+(b.Top/block())*pitch()+(b.Left/block())*bf2metal::formatBytes(format);
        return D3D_OK;
    }
    HRESULT lockRect(D3DLOCKED_RECT* out,const RECT* r,DWORD f) {
        if(!out) return D3DERR_INVALIDCALL; *out={};
        if(r && (r->left<0 || r->top<0 || r->right<0 || r->bottom<0)) return D3DERR_INVALIDCALL;
        D3DBOX b=r?D3DBOX{UINT(r->left),UINT(r->top),UINT(r->right),UINT(r->bottom),0,1}:D3DBOX{0,0,w,h,0,1};
        D3DLOCKED_BOX box={}; const HRESULT hr=lock(&box,r?&b:nullptr,f);
        if(SUCCEEDED(hr)) { out->Pitch=box.RowPitch; out->pBits=box.pBits; } return hr;
    }
    HRESULT unlock() {
        mainThread("IDirect3DResource9::Unlock");
        if(!locked) return D3DERR_INVALIDCALL;
        locked=false;
        return (flags&D3DLOCK_READONLY) || write() ? D3D_OK : D3DERR_DEVICELOST;
    }
};

class Surface final : public Resource<IDirect3DSurface9> {
public:
    D3DSURFACE_DESC desc={};
    std::shared_ptr<Plane> plane;
    Life* container=nullptr;
    IUnknown* containerInterface=nullptr;
    Surface** cacheSlot=nullptr;
    bool lockable=true;
    Surface(IDirect3DDevice9* d,bf2metal::Frame* f,DWORD u,bool in=false)
        : Resource(d,f,"IDirect3DSurface9",D3DRTYPE_SURFACE,in) { desc.Usage=u; }
    ~Surface() override { if(cacheSlot) *cacheSlot=nullptr; if(container) container->drop(); }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=desc; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetContainer(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        return containerInterface ? containerInterface->QueryInterface(id,out) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* out,const RECT* r,DWORD f) override {
        if(!lockable || !plane) return D3DERR_INVALIDCALL; return plane->lockRect(out,r,f);
    }
    HRESULT STDMETHODCALLTYPE UnlockRect() override { return plane?plane->unlock():D3DERR_INVALIDCALL; }
};

template<class I> class TextureBase : public Resource<I> {
public:
    std::shared_ptr<Image> image;
    std::vector<std::shared_ptr<Plane>> planes;
    std::vector<Surface*> views;
    unsigned levels;
    DWORD lod=0;
    TextureBase(IDirect3DDevice9* d,bf2metal::Frame* f,const char* name,D3DRESOURCETYPE kind,
                std::shared_ptr<Image> im,unsigned w,unsigned h,unsigned depth,unsigned count,DWORD usage,D3DFORMAT fmt,D3DPOOL pool)
        : Resource<I>(d,f,name,kind),image(im),levels(count) {
        const unsigned faces=kind==D3DRTYPE_CUBETEXTURE?6:1;
        for(unsigned face=0;face<faces;++face) for(unsigned l=0;l<count;++l)
            planes.push_back(std::make_shared<Plane>(Plane{f,im,std::max(1u,w>>l),std::max(1u,h>>l),
                std::max(1u,depth>>l),l,face,fmt,usage,pool,false,0,{}}));
        views.resize(faces*count,nullptr);
    }
    DWORD STDMETHODCALLTYPE SetLOD(DWORD n) override {
        if(planes[0]->pool!=D3DPOOL_MANAGED) return 0;
        const auto old=lod; lod=std::min(n,levels-1); return old;
    }
    DWORD STDMETHODCALLTYPE GetLOD() override { return lod; }
    DWORD STDMETHODCALLTYPE GetLevelCount() override { return levels; }
    // D3D9 rules for a texture created without D3DUSAGE_AUTOGENMIPMAP, which this device never
    // creates (CreateTexture refuses the usage): the filter cannot be set and reads NONE, and
    // GenerateMipSubLevels does nothing.
    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE) override { return D3DERR_INVALIDCALL; }
    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override { return D3DTEXF_NONE; }
    void STDMETHODCALLTYPE GenerateMipSubLevels() override {}
    HRESULT surface(unsigned l,unsigned face,IDirect3DSurface9** out) {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        if(l>=levels || face>=planes.size()/levels) return D3DERR_INVALIDCALL;
        const unsigned index=face*levels+l;
        if(!views[index]) {
            auto* s=new Surface(this->owner,this->frame,planes[index]->usage);
            s->plane=planes[index]; s->desc=s->plane->desc();
            s->container=this; s->containerInterface=static_cast<I*>(this); this->keep();
            s->cacheSlot=&views[index]; views[index]=s;
        }
        *out=views[index]; views[index]->AddRef(); return D3D_OK;
    }
    HRESULT levelDesc(UINT l,D3DSURFACE_DESC* out) {
        if(!out || l>=levels) return D3DERR_INVALIDCALL; *out=planes[l]->desc(); return D3D_OK;
    }
};
class Texture2D final : public TextureBase<IDirect3DTexture9> {
public:
    using TextureBase::TextureBase;
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT l,D3DSURFACE_DESC* out) override { return levelDesc(l,out); }
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT l,IDirect3DSurface9** out) override { return surface(l,0,out); }
    HRESULT STDMETHODCALLTYPE LockRect(UINT l,D3DLOCKED_RECT* out,const RECT* r,DWORD f) override {
        return l<levels?planes[l]->lockRect(out,r,f):D3DERR_INVALIDCALL;
    }
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT l) override { return l<levels?planes[l]->unlock():D3DERR_INVALIDCALL; }
};
class CubeTexture final : public TextureBase<IDirect3DCubeTexture9> {
public:
    using TextureBase::TextureBase;
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT l,D3DSURFACE_DESC* out) override { return levelDesc(l,out); }
    HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES f,UINT l,IDirect3DSurface9** out) override { return surface(l,f,out); }
    HRESULT STDMETHODCALLTYPE LockRect(D3DCUBEMAP_FACES face,UINT l,D3DLOCKED_RECT* out,const RECT* r,DWORD f) override {
        return unsigned(face)<6 && l<levels?planes[unsigned(face)*levels+l]->lockRect(out,r,f):D3DERR_INVALIDCALL;
    }
    HRESULT STDMETHODCALLTYPE UnlockRect(D3DCUBEMAP_FACES face,UINT l) override {
        return unsigned(face)<6 && l<levels?planes[unsigned(face)*levels+l]->unlock():D3DERR_INVALIDCALL;
    }
};
class VolumeTexture final : public TextureBase<IDirect3DVolumeTexture9> {
public:
    using TextureBase::TextureBase;
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT l,D3DVOLUME_DESC* out) override {
        if(!out || l>=levels) return D3DERR_INVALIDCALL; const auto& p=*planes[l];
        *out={p.format,D3DRTYPE_VOLUME,p.usage,p.pool,p.w,p.h,p.d}; return D3D_OK;
    }
    // [CHOSEN, not recovered] No IDirect3DVolume9 object is modelled; the levels are reached
    // through LockBox. No tree or retail call reads a volume level (the volume textures are
    // filled by D3DX and LockBox).
    HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT,IDirect3DVolume9** out) override {
        if(out) *out=nullptr;
        bf2gfx::unsupported("IDirect3DVolumeTexture9::GetVolumeLevel (no volume objects)");
        return D3DERR_INVALIDCALL;
    }
    HRESULT STDMETHODCALLTYPE LockBox(UINT l,D3DLOCKED_BOX* out,const D3DBOX* b,DWORD f) override {
        return l<levels?planes[l]->lock(out,b,f):D3DERR_INVALIDCALL;
    }
    HRESULT STDMETHODCALLTYPE UnlockBox(UINT l) override { return l<levels?planes[l]->unlock():D3DERR_INVALIDCALL; }
};
template<class I,class Desc> class BufferResource final : public Resource<I> {
public:
    bf2metal::Buffer* native;
    Desc desc;
    unsigned locks=0;
    BufferResource(IDirect3DDevice9* d,bf2metal::Frame* f,const char* name,const Desc& description)
        : Resource<I>(d,f,name,description.Type),native(bf2metal::createBuffer(f,description.Size)),desc(description) {}
    ~BufferResource() override { bf2metal::destroyBuffer(native); }
    HRESULT STDMETHODCALLTYPE GetDesc(Desc* out) override { if(!out) return D3DERR_INVALIDCALL; *out=desc; return D3D_OK; }
    HRESULT STDMETHODCALLTYPE Lock(UINT offset,UINT size,void** out,DWORD flags) override {
        mainThread("IDirect3DBuffer9::Lock");
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        // Windows results (Wine test_vb_lock_flags, sanitise_map_flags): hints are no-ops,
        // READONLY|DISCARD is the one refused combination, NOOVERWRITE wins over DISCARD,
        // and DISCARD/NOOVERWRITE on a static buffer are ignored rather than refused.
        flags&=~lockHints;
        if(offset>=desc.Size || size>desc.Size-offset ||
            (flags&~(D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE|D3DLOCK_READONLY)) ||
            ((flags&D3DLOCK_READONLY) && (flags&D3DLOCK_DISCARD))) return D3DERR_INVALIDCALL;
        if(!(desc.Usage&D3DUSAGE_DYNAMIC)) flags&=~(D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE);
        if(flags&D3DLOCK_NOOVERWRITE) flags&=~D3DLOCK_DISCARD;
        if(locks && (flags&D3DLOCK_DISCARD)) return D3DERR_INVALIDCALL;
        if(flags&D3DLOCK_DISCARD) {
            auto* fresh=bf2metal::createBuffer(this->frame,desc.Size);
            if(!fresh) return E_OUTOFMEMORY;
            // [CHOSEN, not recovered] DISCARD renames without waiting, as D3D9 drivers do. Each
            // in-flight generation stays alive on the GPU side: draws bind the MTLBuffer at encode
            // time (DrawBridge setVertexBuffer) and the frame's command buffers retain their
            // references ([queue commandBuffer]); draws resolve `native` per call (DeviceDraw).
            // The former waitIdle here committed and waited on the GPU for every discard lock
            // (each font run and Swiff shape), so a loading-screen repaint cost ~43 ms and the
            // level load, which repaints on each file open, never finished.
            bf2metal::destroyBuffer(native); native=fresh;
        } else if(!(flags&D3DLOCK_NOOVERWRITE) && !bf2metal::waitIdle(this->frame)) return D3DERR_DEVICELOST;
        ++locks; *out=static_cast<unsigned char*>(bf2metal::bufferBytes(native))+offset; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock() override { if(!locks) return D3DERR_INVALIDCALL; --locks; return D3D_OK; }
};
using VertexBuffer=BufferResource<IDirect3DVertexBuffer9,D3DVERTEXBUFFER_DESC>;
using IndexBuffer=BufferResource<IDirect3DIndexBuffer9,D3DINDEXBUFFER_DESC>;

class Declaration final : public IDirect3DVertexDeclaration9, public Life {
public:
    std::vector<D3DVERTEXELEMENT9> elements;
    std::vector<bf2metal::VertexElement> mapped;
    bf2metal::VertexLayout* native=nullptr;
    Declaration(IDirect3DDevice9* d,bf2metal::Frame* f) : Life(d,f,"IDirect3DVertexDeclaration9") {}
    ~Declaration() override { bf2metal::destroyLayout(native); }
    ULONG STDMETHODCALLTYPE AddRef() override { return add(); }
    ULONG STDMETHODCALLTYPE Release() override { return release(); }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override { return device(out); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        if(!sameIID(id,iidUnknown) && !sameIID(id,iidDeclaration)) return E_NOINTERFACE;
        *out=static_cast<IDirect3DVertexDeclaration9*>(this); add(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDeclaration(D3DVERTEXELEMENT9* out,UINT* count) override {
        if(!count) return D3DERR_INVALIDCALL;
        // Count is an output, including the terminator, per the public D3D9 contract.
        *count=UINT(elements.size()); if(out) std::copy(elements.begin(),elements.end(),out); return D3D_OK;
    }
};

class Query final : public IDirect3DQuery9, public Life {
public:
    D3DQUERYTYPE type;
    Query** active;
    bf2metal::Occlusion* occlusion=nullptr; // occlusion queries: the draws' visibility slots
    bool ready=false, issued=false, pending=false;
    DWORD value=0;
    std::uint64_t fence=0; // event queries: the submission Issue(END) waits for (MetalBridge.h)
    Query(IDirect3DDevice9* d,bf2metal::Frame* f,D3DQUERYTYPE t,Query** current)
        : Life(d,f,"IDirect3DQuery9"),type(t),active(current) {
        if(t==D3DQUERYTYPE_OCCLUSION) occlusion=bf2metal::createOcclusion(f);
    }
    ~Query() override { if(*active==this) *active=nullptr; if(occlusion) bf2metal::destroyOcclusion(occlusion); }
    ULONG STDMETHODCALLTYPE AddRef() override { return add(); }
    ULONG STDMETHODCALLTYPE Release() override { return release(); }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override { return device(out); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        if(!sameIID(id,iidUnknown) && !sameIID(id,iidQuery)) return E_NOINTERFACE;
        *out=static_cast<IDirect3DQuery9*>(this); add(); return D3D_OK;
    }
    D3DQUERYTYPE STDMETHODCALLTYPE GetType() override { return type; }
    DWORD STDMETHODCALLTYPE GetDataSize() override { return sizeof(DWORD); }
    HRESULT STDMETHODCALLTYPE Issue(DWORD flags) override {
        mainThread("IDirect3DQuery9::Issue");
        // Windows (Wine test_occlusion_query): BEGIN restarts a building query, and END
        // without BEGIN succeeds; the latter measures an empty interval (0 samples).
        if(type==D3DQUERYTYPE_OCCLUSION && flags==2) {
            if(*active && *active!=this) return D3DERR_INVALIDCALL;
            *active=this; bf2metal::occlusionBegin(occlusion); ready=pending=false; issued=true; return D3D_OK;
        }
        if(flags!=D3DISSUE_END) return D3DERR_INVALIDCALL;
        ready=false; issued=true;
        if(type==D3DQUERYTYPE_EVENT) {
            // D3D9 drivers answer an event query once the GPU has reached it; games poll it to
            // keep the CPU from running more than a frame or two ahead. Waiting for the GPU here
            // made every frame wait for the previous one (no frames in flight).
            fence=bf2metal::submitFence(frame); value=TRUE; pending=true;
        } else {
            // The draws issued since BEGIN counted samples passing the depth/stencil tests into
            // Metal visibility results (DrawBridge.inc); GetData reads them after completion.
            if(*active==this) { *active=nullptr; pending=true; }
            else { bf2metal::occlusionBegin(occlusion); pending=true; } // END without BEGIN
        }
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetData(void* out,DWORD size,DWORD flags) override {
        mainThread("IDirect3DQuery9::GetData");
        if(flags&~D3DGETDATA_FLUSH || (size && !out)) return D3DERR_INVALIDCALL;
        // Windows answers S_OK for a never-issued query and copies at most GetDataSize bytes
        // (Wine test_occlusion_query). The data of a never-issued query is undefined: untouched.
        if(!issued) return D3D_OK;
        if(pending && type==D3DQUERYTYPE_EVENT) {
            if(!bf2metal::fenceReached(frame,fence)) return S_FALSE;
            pending=false; ready=true;
        }
        if(pending) {
            std::uint64_t samples=0;
            const int r=bf2metal::occlusionResult(frame,occlusion,(flags&D3DGETDATA_FLUSH)!=0,samples);
            if(r<0) return D3DERR_DEVICELOST;
            if(r==0) return S_FALSE;
            value=DWORD(std::min(samples,std::uint64_t(0xffffffffu))); pending=false; ready=true;
        }
        if(!ready) return S_FALSE; // between BEGIN and END
        if(size) std::memcpy(out,&value,std::min<DWORD>(size,sizeof(value))); return D3D_OK;
    }
};
