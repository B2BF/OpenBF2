// [CHOSEN, not recovered] Native mesh utilities backed by G3b vertex/index buffers.
#if (defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))) && defined(BF2_IMAGE_RENDDX9)
#include "D3DXHelpers.h"
#include "D3DXMesh.h"
#if defined(__APPLE__)
#include "RefTrace.h" // [TEMP] [RETIRE-WHEN: rend-deinit-reftrace]
#endif
#include "../../../External/meshoptimizer/meshoptimizer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <memory>
namespace bf2d3dx {
static unsigned elementSize(unsigned type) {
    static const unsigned sizes[]={4,8,12,16,4,4,4,8,4,4,8,4,8,4,4,4,8};
    return type<17?sizes[type]:0;
}
static bool declaration(const D3DVERTEXELEMENT9* p,std::vector<D3DVERTEXELEMENT9>& out,unsigned& stride) {
    if(!p) return false; stride=0; out.clear();
    for(unsigned i=0;i<=64;++i) {
        auto e=p[i]; out.push_back(e);
        if(e.Stream==255 && e.Type==D3DDECLTYPE_UNUSED) return i>0;
        if(i==64 || e.Stream || e.Method || !elementSize(e.Type)) return false;
        if(e.Type==13 || e.Type==14) stop("D3DXCreateMesh packed DEC3 declaration");
        for(unsigned j=0;j<i;++j) if(out[j].Usage==e.Usage && out[j].UsageIndex==e.UsageIndex) return false;
        stride=std::max(stride,e.Offset+elementSize(e.Type));
    }
    return false;
}
static bool fvfDecl(DWORD f,std::vector<D3DVERTEXELEMENT9>& out) {
    unsigned offset=0;
    auto add=[&](BYTE type,BYTE usage,BYTE index=0) { out.push_back({0,WORD(offset),type,0,usage,index}); offset+=elementSize(type); };
    if((f&0x400e)==2) add(2,0);
    else if((f&0x400e)==4) add(3,9);
    else if((f&0x400e)==0x4002) add(3,0);
    else return false;
    if(f&0x10) add(2,3); if(f&0x20) add(0,4); if(f&0x40) add(4,10); if(f&0x80) add(4,10,1);
    unsigned tex=(f>>8)&15; if(tex>8) return false;
    for(unsigned i=0;i<tex;++i) { unsigned size=(f>>(16+i*2))&3; add(size==0?1:size==1?2:size==2?3:0,5,i); }
    if(f&0x0000b001u) return false;
    out.push_back(D3DDECL_END()); return true;
}
static Color vertexRead(const unsigned char* p,unsigned type) {
    Color c;
    if(type<=3) for(unsigned i=0;i<=type;++i) { float f; std::memcpy(&f,p+i*4,4); c[i]=f; }
    else if(type==4) c=decode(D3DFMT_A8R8G8B8,p);
    else if(type==5 || type==8) for(unsigned i=0;i<4;++i) c[i]=type==5?p[i]:p[i]/255.;
    else if(type==15 || type==16) for(unsigned i=0;i<(type==15?2u:4u);++i) { WORD h; std::memcpy(&h,p+i*2,2); c[i]=unhalf(h); }
    else for(unsigned i=0;i<elementSize(type)/2;++i) {
        WORD u; std::memcpy(&u,p+i*2,2); auto s=static_cast<std::int16_t>(u);
        c[i]=type==6 || type==7?s:type==9 || type==10?std::max(-1.,s/32767.):u/65535.;
    }
    return c;
}
static void vertexWrite(unsigned char* p,unsigned type,const Color& c) {
    if(type<=3) for(unsigned i=0;i<=type;++i) { float f=float(c[i]); std::memcpy(p+i*4,&f,4); }
    else if(type==4) encode(D3DFMT_A8R8G8B8,p,c);
    else if(type==5 || type==8) for(unsigned i=0;i<4;++i) p[i]=std::max(0.,std::min(255.,std::round(c[i]*(type==5?1:255))));
    else if(type==15 || type==16) for(unsigned i=0;i<(type==15?2u:4u);++i) { WORD h=half(float(c[i])); std::memcpy(p+i*2,&h,2); }
    else for(unsigned i=0;i<elementSize(type)/2;++i) {
        WORD u=type==11 || type==12?WORD(std::max(0.,std::min(65535.,std::round(c[i]*65535)))):
            WORD(static_cast<std::int16_t>(std::max(-32768.,std::min(32767.,std::round(c[i]*(type==9 || type==10?32767:1))))));
        std::memcpy(p+i*2,&u,2);
    }
}
class Mesh final : public ID3DXMesh {
    ULONG refs=1;
public:
    IDirect3DDevice9* device;
    IDirect3DVertexBuffer9* vb=nullptr;
    IDirect3DIndexBuffer9* ib=nullptr;
    IDirect3DVertexDeclaration9* layout=nullptr;
    DWORD faces,vertices,options,fvf; unsigned stride=0;
    std::vector<D3DVERTEXELEMENT9> decl;
    std::vector<DWORD> attributes; bool attributeLock=false;
    Mesh(IDirect3DDevice9* d,DWORD f,DWORD v,DWORD o,DWORD vf):device(d),faces(f),vertices(v),options(o),fvf(vf),attributes(f,0) {
#if defined(__APPLE__)
        { bf2metal::reftrace::Owned o; d->AddRef(); } bf2metal::reftrace::born(this,"ID3DXMesh",[](const void*) { return 1ul; });
#else
        d->AddRef();
#endif
    }
    ~Mesh() { if(layout) layout->Release(); if(vb) vb->Release(); if(ib) ib->Release();
#if defined(__APPLE__)
        bf2metal::reftrace::died(this); bf2metal::reftrace::Owned o;
#endif
        device->Release(); }
    HRESULT initialize(const D3DVERTEXELEMENT9* d) {
        if(!declaration(d,decl,stride) || !stride || faces>std::numeric_limits<UINT>::max()/12 || vertices>std::numeric_limits<UINT>::max()/stride || (!(options&1) && vertices>65535)) return D3DERR_INVALIDCALL;
        // The inventory uses SYSTEMMEM and MANAGED, optionally WRITEONLY.
        if(options&~0x771u) stop("D3DXCreateMesh options");
        const auto vp=options&0x10?D3DPOOL_SYSTEMMEM:options&0x20?D3DPOOL_MANAGED:D3DPOOL_DEFAULT;
        const auto ip=options&0x100?D3DPOOL_SYSTEMMEM:options&0x200?D3DPOOL_MANAGED:D3DPOOL_DEFAULT;
        HRESULT hr=device->CreateVertexBuffer(vertices*stride,options&0x40?D3DUSAGE_WRITEONLY:0,fvf,vp,&vb,nullptr);
        if(SUCCEEDED(hr)) hr=device->CreateIndexBuffer(faces*3*(options&1?4:2),options&0x400?D3DUSAGE_WRITEONLY:0,options&1?D3DFMT_INDEX32:D3DFMT_INDEX16,ip,&ib,nullptr);
        if(SUCCEEDED(hr)) hr=device->CreateVertexDeclaration(decl.data(),&layout); return hr;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        const GUID unknown={0,0,0,{0xc0,0,0,0,0,0,0,0x46}},base={0x7ed943dd,0x52e8,0x40b5,{0xa8,0xd8,0x76,0x68,0x5c,0x40,0x63,0x30}},mesh={0x4020e5c2,0x1403,0x4929,{0x88,0x3f,0xe2,0xe8,0x49,0xfa,0xc1,0x95}};
        if(std::memcmp(&id,&unknown,16) && std::memcmp(&id,&base,16) && std::memcmp(&id,&mesh,16)) return E_NOINTERFACE; *out=this; AddRef(); return D3D_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { const auto n=--refs; if(!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE DrawSubset(DWORD attribute) override {
        HRESULT hr=device->SetStreamSource(0,vb,0,stride); if(SUCCEEDED(hr)) hr=device->SetIndices(ib); if(SUCCEEDED(hr)) hr=device->SetVertexDeclaration(layout);
        for(unsigned f=0;SUCCEEDED(hr) && f<faces;) { if(attributes[f]!=attribute) { ++f; continue; } unsigned first=f; while(f<faces && attributes[f]==attribute) ++f;
            hr=device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,vertices,first*3,f-first); } return hr;
    }
    DWORD STDMETHODCALLTYPE GetNumFaces() override { return faces; }
    DWORD STDMETHODCALLTYPE GetNumVertices() override { return vertices; }
    DWORD STDMETHODCALLTYPE GetFVF() override { return fvf; }
    HRESULT STDMETHODCALLTYPE GetDeclaration(D3DVERTEXELEMENT9* out) override { if(!out) return D3DERR_INVALIDCALL; std::copy(decl.begin(),decl.end(),out); return D3D_OK; }
    DWORD STDMETHODCALLTYPE GetNumBytesPerVertex() override { return stride; }
    DWORD STDMETHODCALLTYPE GetOptions() override { return options; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override { if(!out) return D3DERR_INVALIDCALL; *out=device; device->AddRef(); return D3D_OK; }
    HRESULT clone(DWORD o,DWORD vf,const D3DVERTEXELEMENT9* declaration,IDirect3DDevice9* d,ID3DXMesh** out) {
        if(!out || !d) return D3DERR_INVALIDCALL; *out=nullptr;
        auto copy=std::unique_ptr<Mesh>(new Mesh(d,faces,vertices,o,vf)); HRESULT hr=copy->initialize(declaration); if(FAILED(hr)) return hr;
        void *a=nullptr,*b=nullptr;
        hr=vb->Lock(0,0,&a,options&0x40?0:D3DLOCK_READONLY); if(FAILED(hr)) return hr;
        hr=copy->vb->Lock(0,0,&b,0); if(FAILED(hr)) { vb->Unlock(); return hr; }
        std::memset(b,0,std::size_t(vertices)*copy->stride);
        for(unsigned i=0;i<vertices;++i) for(const auto& dst:copy->decl) {
            if(dst.Stream==255) break; Color value; bool matched=false;
            for(const auto& src:decl) if(src.Stream!=255 && src.Usage==dst.Usage && src.UsageIndex==dst.UsageIndex) {
                const auto* p=static_cast<const unsigned char*>(a)+std::size_t(i)*stride+src.Offset;
                auto* q=static_cast<unsigned char*>(b)+std::size_t(i)*copy->stride+dst.Offset;
                if(src.Type==dst.Type) std::memcpy(q,p,elementSize(src.Type)); else vertexWrite(q,dst.Type,vertexRead(p,src.Type)); matched=true; break;
            }
            if(!matched) vertexWrite(static_cast<unsigned char*>(b)+std::size_t(i)*copy->stride+dst.Offset,dst.Type,value);
        }
        HRESULT ah=vb->Unlock(),bh=copy->vb->Unlock(); if(FAILED(ah)) return ah; if(FAILED(bh)) return bh;
        std::vector<DWORD> indices; hr=readIndices(indices); if(FAILED(hr)) return hr;
        hr=copy->writeIndices(indices); if(FAILED(hr)) return hr;
        copy->attributes=attributes; *out=copy.release(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CloneMeshFVF(DWORD o,DWORD f,IDirect3DDevice9* d,ID3DXMesh** out) override { std::vector<D3DVERTEXELEMENT9> v; if(!fvfDecl(f,v)) return D3DERR_INVALIDCALL; return clone(o,f,v.data(),d,out); }
    HRESULT STDMETHODCALLTYPE CloneMesh(DWORD o,const D3DVERTEXELEMENT9* v,IDirect3DDevice9* d,ID3DXMesh** out) override { return clone(o,0,v,d,out); }
    HRESULT STDMETHODCALLTYPE GetVertexBuffer(IDirect3DVertexBuffer9** out) override { if(!out) return D3DERR_INVALIDCALL; *out=vb; vb->AddRef(); return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetIndexBuffer(IDirect3DIndexBuffer9** out) override { if(!out) return D3DERR_INVALIDCALL; *out=ib; ib->AddRef(); return D3D_OK; }
    HRESULT STDMETHODCALLTYPE LockVertexBuffer(DWORD f,void** out) override { return vb->Lock(0,0,out,f); }
    HRESULT STDMETHODCALLTYPE UnlockVertexBuffer() override { return vb->Unlock(); }
    HRESULT STDMETHODCALLTYPE LockIndexBuffer(DWORD f,void** out) override { return ib->Lock(0,0,out,f); }
    HRESULT STDMETHODCALLTYPE UnlockIndexBuffer() override { return ib->Unlock(); }
    // Native ranges describe consecutive face runs and their actual vertex span.
    HRESULT STDMETHODCALLTYPE GetAttributeTable(D3DXATTRIBUTERANGE* out,DWORD* count) override {
        if(!count || attributeLock) return D3DERR_INVALIDCALL;
        std::vector<DWORD> ix; HRESULT hr=readIndices(ix); if(FAILED(hr)) return hr;
        std::vector<D3DXATTRIBUTERANGE> ranges;
        for(DWORD first=0;first<faces;) {
            DWORD end=first,lo=vertices,hi=0;
            do { for(unsigned e=0;e<3;++e) { DWORD v=ix[end*3+e]; if(v>=vertices) return D3DERR_INVALIDCALL; lo=std::min(lo,v); hi=std::max(hi,v); } ++end; }
            while(end<faces && attributes[end]==attributes[first]);
            ranges.push_back({attributes[first],first,end-first,lo,hi-lo+1}); first=end;
        }
        if(!out) { *count=DWORD(ranges.size()); return D3D_OK; }
        const DWORD capacity=*count; *count=DWORD(ranges.size());
        if(capacity<ranges.size()) return D3DERR_INVALIDCALL;
        std::copy(ranges.begin(),ranges.end(),out); return D3D_OK;
    }
    HRESULT readIndices(std::vector<DWORD>& out) {
        void* p; HRESULT hr=ib->Lock(0,0,&p,options&0x400?0:D3DLOCK_READONLY); if(FAILED(hr)) return hr; out.resize(std::size_t(faces)*3);
        for(unsigned i=0;i<out.size();++i) { if(options&1) std::memcpy(&out[i],static_cast<const unsigned char*>(p)+std::size_t(i)*4,4); else { WORD v; std::memcpy(&v,static_cast<const unsigned char*>(p)+std::size_t(i)*2,2); out[i]=v; } }
        return ib->Unlock();
    }
    HRESULT writeIndices(const std::vector<DWORD>& v) {
        for(auto i:v) if(i>=vertices || (!(options&1) && i>65535)) return D3DERR_INVALIDCALL;
        void* p; HRESULT hr=ib->Lock(0,0,&p,0); if(FAILED(hr)) return hr;
        for(unsigned i=0;i<v.size();++i) { if(options&1) std::memcpy(static_cast<unsigned char*>(p)+std::size_t(i)*4,&v[i],4); else { WORD n=v[i]; std::memcpy(static_cast<unsigned char*>(p)+std::size_t(i)*2,&n,2); } }
        return ib->Unlock();
    }
    HRESULT adjacency(const DWORD* reps,DWORD* out) {
        if(!out) return D3DERR_INVALIDCALL; std::vector<DWORD> ix; HRESULT hr=readIndices(ix); if(FAILED(hr)) return hr;
        for(auto i:ix) if(i>=vertices || (reps && reps[i]>=vertices)) return D3DERR_INVALIDCALL;
        std::fill(out,out+std::size_t(faces)*3,0xffffffffu);
        std::map<std::pair<DWORD,DWORD>,std::pair<DWORD,DWORD>> edges;
        for(DWORD f=0;f<faces;++f) for(DWORD e=0;e<3;++e) {
            DWORD a=ix[f*3+e],b=ix[f*3+(e+1)%3]; if(reps) { a=reps[a]; b=reps[b]; } if(a==b) continue;
            auto it=edges.find({b,a});
            if(it!=edges.end() && it->second.first!=f) { auto other=it->second; out[f*3+e]=other.first; out[other.first*3+other.second]=f; edges.erase(it); }
            else edges.emplace(std::make_pair(a,b),std::make_pair(f,e));
        }
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE ConvertPointRepsToAdjacency(const DWORD* reps,DWORD* out) override { if(!reps) return D3DERR_INVALIDCALL; return adjacency(reps,out); }
    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: macos-mesh-point-reps]
    // Implement and validate adjacency-driven vertex equivalence before removal.
    HRESULT STDMETHODCALLTYPE ConvertAdjacencyToPointReps(const DWORD*,DWORD*) override { stop("ID3DXMesh::ConvertAdjacencyToPointReps"); }
    HRESULT STDMETHODCALLTYPE GenerateAdjacency(FLOAT epsilon,DWORD* out) override {
        if(!out || epsilon<0 || !std::isfinite(epsilon)) return D3DERR_INVALIDCALL;
        const D3DVERTEXELEMENT9* position=nullptr;
        for(const auto& e:decl) if(e.Stream!=255 && e.Usage==0 && e.UsageIndex==0) { position=&e; break; }
        if(!position) return D3DERR_INVALIDCALL;
        void* p; HRESULT hr=vb->Lock(0,0,&p,options&0x40?0:D3DLOCK_READONLY); if(FAILED(hr)) return hr;
        std::vector<DWORD> reps(vertices); std::vector<Color> positions(vertices);
        using Cell=std::tuple<long long,long long,long long>; std::map<Cell,std::vector<DWORD>> grid;
        std::map<std::tuple<double,double,double>,DWORD> exact;
        bool valid=true;
        for(DWORD i=0;i<vertices;++i) {
            Color c=vertexRead(static_cast<const unsigned char*>(p)+std::size_t(i)*stride+position->Offset,position->Type); positions[i]=c; reps[i]=i;
            if(!std::isfinite(c.r) || !std::isfinite(c.g) || !std::isfinite(c.b)) { valid=false; break; }
            if(epsilon==0) { auto result=exact.emplace(std::make_tuple(c.r,c.g,c.b),i); reps[i]=result.first->second; }
            else {
                double a=std::floor(c.r/epsilon),b=std::floor(c.g/epsilon),d=std::floor(c.b/epsilon);
                if(std::abs(a)>9e18 || std::abs(b)>9e18 || std::abs(d)>9e18) { valid=false; break; }
                long long x=a,y=b,z=d;
                for(int dz=-1;dz<=1;++dz) for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                    auto it=grid.find({x+dx,y+dy,z+dz}); if(it==grid.end()) continue;
                    for(DWORD j:it->second) { auto q=positions[j]; double vx=c.r-q.r,vy=c.g-q.g,vz=c.b-q.b;
                        if(vx*vx+vy*vy+vz*vz<=double(epsilon)*epsilon) reps[i]=std::min(reps[i],j); }
                }
                if(reps[i]==i) grid[{x,y,z}].push_back(i);
            }
        }
        hr=vb->Unlock(); if(FAILED(hr)) return hr; return valid?adjacency(reps.data(),out):D3DERR_INVALIDCALL;
    }
    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: macos-mesh-semantics]
    // Rebuild the native declaration and validate compatible semantic updates.
    HRESULT STDMETHODCALLTYPE UpdateSemantics(D3DVERTEXELEMENT9*) override { stop("ID3DXMesh::UpdateSemantics"); }
    // The documented flags are DISCARD, NO_DIRTY_UPDATE, NOSYSLOCK and READONLY; none changes a
    // CPU-side attribute array. The mesh optimizers lock with NOSYSLOCK (0x800).
    HRESULT STDMETHODCALLTYPE LockAttributeBuffer(DWORD flags,DWORD** out) override { if(!out || attributeLock || (flags&~(D3DLOCK_READONLY|0x800u|D3DLOCK_DISCARD|0x8000u))) return D3DERR_INVALIDCALL; attributeLock=true; *out=attributes.data(); return D3D_OK; }
    HRESULT STDMETHODCALLTYPE UnlockAttributeBuffer() override { if(!attributeLock) return D3DERR_INVALIDCALL; attributeLock=false; return D3D_OK; }
    HRESULT STDMETHODCALLTYPE Optimize(DWORD flags,const DWORD* adjacencyIn,DWORD* adjacencyOut,
        DWORD* faceRemap,ID3DXBuffer** vertexRemap,ID3DXMesh** out) override {
        if(vertexRemap) *vertexRemap=nullptr;
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        // Supported operations: attribute/cache sorting, optional vertex preservation
        // and no-split. Unsupported COMPACT/STRIPREORDER/SHAREVB flags refuse explicitly.
        const DWORD operation=flags&0xffff0000u, storage=flags&0xffffu;
        if(attributeLock || !adjacencyIn || (operation&~0x36000000u) || !(operation&0x06000000u) ||
            (storage && storage!=D3DXMESH_SYSTEMMEM && storage!=D3DXMESH_MANAGED)) return D3DERR_INVALIDCALL;
        std::vector<DWORD> ix; HRESULT hr=readIndices(ix); if(FAILED(hr)) return hr;
        for(DWORD v:ix) if(v>=vertices) return D3DERR_INVALIDCALL;
        for(std::size_t e=0;e<ix.size();++e) if(adjacencyIn[e]!=0xffffffffu && adjacencyIn[e]>=faces) return D3DERR_INVALIDCALL;
        // [CHOSEN, not recovered] Stable attribute order, then the native 16-entry
        // cache policy within each group. Proprietary D3DX face ordering is not claimed.
        std::map<DWORD,std::vector<DWORD>> groups;
        for(DWORD f=0;f<faces;++f) groups[attributes[f]].push_back(f);
        std::vector<DWORD> order,cache;
        std::vector<std::vector<DWORD>> uses(vertices);
        for(DWORD f=0;f<faces;++f) for(unsigned e=0;e<3;++e) uses[ix[f*3+e]].push_back(f);
        std::vector<unsigned char> done(faces,0);
        for(const auto& group:groups) {
            std::set<DWORD> candidates; cache.clear(); std::size_t next=0;
            for(std::size_t n=0;n<group.second.size();++n) {
                DWORD best=0xffffffffu; int score=-1;
                if(operation&0x04000000u) for(DWORD f:candidates) {
                    int hit=0; for(unsigned e=0;e<3;++e) if(std::find(cache.begin(),cache.end(),ix[f*3+e])!=cache.end()) ++hit;
                    if(hit>score) { best=f; score=hit; }
                }
                if(best==0xffffffffu) { while(done[group.second[next]]) ++next; best=group.second[next]; }
                done[best]=1; order.push_back(best);
                for(unsigned e=0;e<3;++e) { DWORD v=ix[best*3+e]; cache.erase(std::remove(cache.begin(),cache.end(),v),cache.end()); cache.insert(cache.begin(),v); }
                if(cache.size()>16) cache.resize(16);
                candidates.clear();
                for(DWORD v:cache) for(DWORD f:uses[v]) if(!done[f] && attributes[f]==group.first) candidates.insert(f);
            }
        }
        std::vector<DWORD> inverse(faces),reordered(ix.size()),newToOld;
        std::map<std::pair<DWORD,DWORD>,DWORD> vertexIds;
        const bool preserve=(operation&0x10000000u)!=0,split=(operation&0x20000000u)==0;
        if(preserve) for(DWORD v=0;v<vertices;++v) newToOld.push_back(v);
        std::vector<unsigned char> used(vertices,0);
        for(DWORD f=0;f<faces;++f) {
            inverse[order[f]]=f;
            for(unsigned e=0;e<3;++e) {
                DWORD v=ix[order[f]*3+e]; used[v]=1;
                if(preserve) reordered[f*3+e]=v;
                else {
                    auto key=std::make_pair(split?attributes[order[f]]:0u,v);
                    auto inserted=vertexIds.emplace(key,DWORD(newToOld.size()));
                    if(inserted.second) newToOld.push_back(v);
                    reordered[f*3+e]=inserted.first->second;
                }
            }
        }
        // COMPACT was not requested: retain unused vertices after the used runs.
        if(!preserve) for(DWORD v=0;v<vertices;++v) if(!used[v]) newToOld.push_back(v);
        DWORD outputOptions=storage?(storage|(options&D3DXMESH_32BIT)):options;
        auto copy=std::unique_ptr<Mesh>(new Mesh(device,faces,DWORD(newToOld.size()),outputOptions,fvf));
        hr=copy->initialize(decl.data()); if(FAILED(hr)) return hr;
        void *src=nullptr,*dst=nullptr;
        hr=vb->Lock(0,0,&src,options&0x40?0:D3DLOCK_READONLY); if(FAILED(hr)) return hr;
        hr=copy->vb->Lock(0,0,&dst,0); if(FAILED(hr)) { vb->Unlock(); return hr; }
        for(std::size_t v=0;v<newToOld.size();++v) std::memcpy(static_cast<unsigned char*>(dst)+v*stride,
            static_cast<const unsigned char*>(src)+std::size_t(newToOld[v])*stride,stride);
        HRESULT a=vb->Unlock(),b=copy->vb->Unlock(); if(FAILED(a)) return a; if(FAILED(b)) return b;
        hr=copy->writeIndices(reordered); if(FAILED(hr)) return hr;
        for(DWORD f=0;f<faces;++f) copy->attributes[f]=attributes[order[f]];
        if(vertexRemap) {
            hr=D3DXCreateBuffer(DWORD(newToOld.size()*4),vertexRemap); if(FAILED(hr)) return hr;
            std::memcpy((*vertexRemap)->GetBufferPointer(),newToOld.data(),newToOld.size()*4);
        }
        // Read all input first, so in/out adjacency may alias.
        std::vector<DWORD> adj(adjacencyIn,adjacencyIn+ix.size());
        if(adjacencyOut) for(DWORD f=0;f<faces;++f) for(unsigned e=0;e<3;++e) {
            DWORD old=adj[order[f]*3+e]; adjacencyOut[f*3+e]=old==0xffffffffu?old:inverse[old];
        }
        if(faceRemap) std::copy(order.begin(),order.end(),faceRemap);
        *out=copy.release(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE OptimizeInplace(DWORD flags,const DWORD* adjacencyIn,DWORD* adjacencyOut,DWORD* faceRemap,ID3DXBuffer** vertexRemap) override {
        // Terrain requests 0x14400000: native cache ordering, keeping vertices.
        // The extra legacy bit does not alter the native ordering policy.
        if(flags!=0x14400000 && flags!=0x14000000 && flags!=0x04000000) stop("ID3DXMesh::OptimizeInplace flags");
        if(attributeLock) return D3DERR_INVALIDCALL; if(vertexRemap) *vertexRemap=nullptr;
        std::vector<DWORD> ix; HRESULT hr=readIndices(ix); if(FAILED(hr)) return hr;
        std::vector<std::vector<DWORD>> uses(vertices);
        for(unsigned i=0;i<ix.size();++i) { if(ix[i]>=vertices) return D3DERR_INVALIDCALL; uses[ix[i]].push_back(i/3); }
        std::vector<unsigned char> done(faces,0); std::vector<DWORD> order,cache; std::set<DWORD> candidates; DWORD next=0;
        while(order.size()<faces) {
            DWORD best=0xffffffff; int score=-1;
            for(auto f:candidates) if(!done[f]) { int hit=0; for(unsigned e=0;e<3;++e) if(std::find(cache.begin(),cache.end(),ix[f*3+e])!=cache.end()) ++hit;
                if(hit>score) { score=hit; best=f; } }
            if(best==0xffffffff) { while(next<faces && done[next]) ++next; best=next; }
            done[best]=1; order.push_back(best);
            for(unsigned e=0;e<3;++e) { DWORD v=ix[best*3+e]; cache.erase(std::remove(cache.begin(),cache.end(),v),cache.end()); cache.insert(cache.begin(),v); }
            if(cache.size()>16) cache.resize(16);
            candidates.clear(); for(auto v:cache) for(auto f:uses[v]) if(!done[f]) candidates.insert(f);
        }
        std::vector<DWORD> reordered(ix.size()),attrs(faces),inverse(faces),adj;
        if(adjacencyIn) adj.assign(adjacencyIn,adjacencyIn+std::size_t(faces)*3);
        for(unsigned f=0;f<faces;++f) { inverse[order[f]]=f; attrs[f]=attributes[order[f]]; if(faceRemap) faceRemap[f]=order[f]; for(unsigned e=0;e<3;++e) reordered[f*3+e]=ix[order[f]*3+e]; }
        hr=writeIndices(reordered); if(FAILED(hr)) return hr; attributes.swap(attrs);
        if(adjacencyOut) {
            if(!adj.empty()) for(unsigned f=0;f<faces;++f) for(unsigned e=0;e<3;++e) { DWORD old=adj[order[f]*3+e]; adjacencyOut[f*3+e]=old<faces?inverse[old]:0xffffffff; }
            else { hr=adjacency(nullptr,adjacencyOut); if(FAILED(hr)) return hr; }
        }
        if(vertexRemap) { hr=D3DXCreateBuffer(vertices*4,vertexRemap); if(FAILED(hr)) return hr; auto* map=static_cast<DWORD*>((*vertexRemap)->GetBufferPointer()); for(DWORD i=0;i<vertices;++i) map[i]=i; }
        return D3D_OK;
    }
};
static HRESULT shape(IDirect3DDevice9* device,const std::vector<MeshVertex>& vertices,const std::vector<DWORD>& indices,ID3DXMesh** out,ID3DXBuffer** adjacency) {
    if(adjacency) *adjacency=nullptr; if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
    HRESULT hr=D3DXCreateMeshFVF(DWORD(indices.size()/3),DWORD(vertices.size()),D3DXMESH_MANAGED|(vertices.size()>65535?1:0),0x12,device,out); if(FAILED(hr)) return hr;
    auto* mesh=static_cast<Mesh*>(*out); void* p;
    hr=mesh->LockVertexBuffer(0,&p); if(SUCCEEDED(hr)) { std::memcpy(p,vertices.data(),vertices.size()*sizeof(MeshVertex)); hr=mesh->UnlockVertexBuffer(); }
    if(SUCCEEDED(hr)) hr=mesh->writeIndices(indices);
    if(SUCCEEDED(hr) && adjacency) { hr=D3DXCreateBuffer(mesh->faces*12,adjacency); if(SUCCEEDED(hr)) hr=mesh->GenerateAdjacency(0,static_cast<DWORD*>((*adjacency)->GetBufferPointer())); }
    if(FAILED(hr)) { if(adjacency && *adjacency) { (*adjacency)->Release(); *adjacency=nullptr; } mesh->Release(); *out=nullptr; } return hr;
}
}
using namespace bf2d3dx;
HRESULT WINAPI D3DXCreateMesh(DWORD faces,DWORD vertices,DWORD options,const D3DVERTEXELEMENT9* decl,IDirect3DDevice9* device,ID3DXMesh** out) {
    if(!out) return D3DERR_INVALIDCALL; *out=nullptr; if(!device || !faces || !vertices) return D3DERR_INVALIDCALL;
    auto m=std::unique_ptr<Mesh>(new Mesh(device,faces,vertices,options,0)); HRESULT hr=m->initialize(decl); if(SUCCEEDED(hr)) *out=m.release(); return hr;
}
HRESULT WINAPI D3DXCreateMeshFVF(DWORD faces,DWORD vertices,DWORD options,DWORD fvf,IDirect3DDevice9* device,ID3DXMesh** out) {
    if(!out) return D3DERR_INVALIDCALL; *out=nullptr; if(!device || !faces || !vertices) return D3DERR_INVALIDCALL;
    std::vector<D3DVERTEXELEMENT9> decl; if(!fvfDecl(fvf,decl)) return D3DERR_INVALIDCALL;
    auto m=std::unique_ptr<Mesh>(new Mesh(device,faces,vertices,options,fvf)); HRESULT hr=m->initialize(decl.data()); if(SUCCEEDED(hr)) *out=m.release(); return hr;
}
HRESULT WINAPI D3DXCreateSphere(IDirect3DDevice9* d,FLOAT radius,UINT slices,UINT stacks,ID3DXMesh** out,ID3DXBuffer** adjacency) {
    if(out) *out=nullptr; if(adjacency) *adjacency=nullptr;
    Geometry g; if(!d || !out || !sphereGeometry(radius,slices,stacks,g)) return D3DERR_INVALIDCALL;
    return shape(d,g.vertices,g.indices,out,adjacency);
}
HRESULT WINAPI D3DXCreateBox(IDirect3DDevice9* d,FLOAT w,FLOAT h,FLOAT depth,ID3DXMesh** out,ID3DXBuffer** adjacency) {
    if(out) *out=nullptr; if(adjacency) *adjacency=nullptr;
    Geometry g; if(!d || !out || !boxGeometry(w,h,depth,g)) return D3DERR_INVALIDCALL;
    return shape(d,g.vertices,g.indices,out,adjacency);
}
HRESULT WINAPI D3DXCreateCylinder(IDirect3DDevice9* d,FLOAT r1,FLOAT r2,FLOAT length,UINT slices,UINT stacks,ID3DXMesh** out,ID3DXBuffer** adjacency) {
    if(out) *out=nullptr; if(adjacency) *adjacency=nullptr;
    Geometry g; if(!d || !out || !cylinderGeometry(r1,r2,length,slices,stacks,g)) return D3DERR_INVALIDCALL;
    return shape(d,g.vertices,g.indices,out,adjacency);
}

// D3DX shape contract: managed XYZ|NORMAL, origin centred, LH coordinates.
// [CHOSEN, not recovered] Deterministic centre-first fan in XY, facing -Z;
// first rim vertex is +Y and the remaining vertices run clockwise from there.
// No byte-identical d3dx9_25 vertex/index ordering is claimed.
HRESULT WINAPI D3DXCreatePolygon(IDirect3DDevice9* d,FLOAT length,UINT sides,ID3DXMesh** out,ID3DXBuffer** adjacency) {
    if(out) *out=nullptr; if(adjacency) *adjacency=nullptr;
    if(!d || !out || !(length>=0) || !std::isfinite(length) || sides<3 || sides>10000000) return D3DERR_INVALIDCALL;
    constexpr double pi=3.14159265358979323846;
    const double radius=double(length)/(2*std::sin(pi/sides));
    if(radius>std::numeric_limits<float>::max()) return D3DERR_INVALIDCALL;
    Geometry g; g.vertices.reserve(std::size_t(sides)+1); g.indices.reserve(std::size_t(sides)*3);
    g.vertices.push_back({{0,0,0},{0,0,-1}});
    for(UINT v=0;v<sides;++v) {
        const double angle=pi/2-2*pi*v/sides;
        g.vertices.push_back({{float(radius*std::cos(angle)),float(radius*std::sin(angle)),0},{0,0,-1}});
        g.indices.insert(g.indices.end(),{0,v+1,(v+1)%sides+1});
    }
    return shape(d,g.vertices,g.indices,out,adjacency);
}

// [CHOSEN, not recovered] Ring-major periodic grid without duplicate seams.
// Z is the torus axis; outerRadius measures to the cross-section centre,
// innerRadius is its radius. The first point is (outer+inner,0,0).
// Analytic outward normals and winding are independent of the chosen ordering.
HRESULT WINAPI D3DXCreateTorus(IDirect3DDevice9* d,FLOAT innerRadius,FLOAT outerRadius,UINT sides,UINT rings,ID3DXMesh** out,ID3DXBuffer** adjacency) {
    if(out) *out=nullptr; if(adjacency) *adjacency=nullptr;
    if(!d || !out || !(innerRadius>=0) || !(outerRadius>=0) || !std::isfinite(innerRadius+outerRadius) ||
        sides<3 || rings<3 || std::uint64_t(sides)*rings>10000000) return D3DERR_INVALIDCALL;
    constexpr double pi=3.14159265358979323846;
    Geometry g; g.vertices.reserve(std::size_t(sides)*rings); g.indices.reserve(std::size_t(sides)*rings*6);
    for(UINT ring=0;ring<rings;++ring) {
        const double a=-2*pi*ring/rings,x=std::cos(a),y=std::sin(a);
        for(UINT side=0;side<sides;++side) {
            const double b=2*pi*side/sides,c=std::cos(b),s=std::sin(b),r=outerRadius+innerRadius*c;
            g.vertices.push_back({{float(r*x),float(r*y),float(innerRadius*s)},{float(c*x),float(c*y),float(s)}});
            DWORD v=ring*sides+side,n=ring*sides+(side+1)%sides,next=((ring+1)%rings)*sides+side,nn=((ring+1)%rings)*sides+(side+1)%sides;
            g.indices.insert(g.indices.end(),{v,n,next,n,nn,next});
        }
    }
    return shape(d,g.vertices,g.indices,out,adjacency);
}

// Legacy D3DXComputeNormals averages incident face normals, weighted by corner
// angle. Adjacency joins replicated edge vertices; a null array keeps seams.
// https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxcomputenormals
HRESULT WINAPI D3DXComputeNormals(ID3DXBaseMesh* mesh,const DWORD* adjacent) {
    if(!mesh) return D3DERR_INVALIDCALL;
    D3DVERTEXELEMENT9 elements[65]; HRESULT hr=mesh->GetDeclaration(elements); if(FAILED(hr)) return hr;
    unsigned position=0,normal=0; bool hasPosition=false,hasNormal=false;
    for(const auto& e:elements) {
        if(e.Stream==255) break;
        if(e.Stream || e.Method) return D3DERR_INVALIDCALL;
        if(e.UsageIndex==0 && (e.Usage==D3DDECLUSAGE_POSITION || e.Usage==D3DDECLUSAGE_NORMAL)) {
            if(e.Type!=D3DDECLTYPE_FLOAT3) return D3DERR_INVALIDCALL;
            if(e.Usage==D3DDECLUSAGE_POSITION) { position=e.Offset; hasPosition=true; }
            else { normal=e.Offset; hasNormal=true; }
        }
    }
    const DWORD vertices=mesh->GetNumVertices(),faces=mesh->GetNumFaces(),stride=mesh->GetNumBytesPerVertex();
    if(!hasPosition || !hasNormal || position+12>stride || normal+12>stride) return D3DERR_INVALIDCALL;
    void* raw=nullptr;
    hr=mesh->LockIndexBuffer(mesh->GetOptions()&0x400?0:D3DLOCK_READONLY,&raw); if(FAILED(hr)) return hr;
    std::vector<DWORD> ix(std::size_t(faces)*3); bool valid=true;
    for(std::size_t i=0;i<ix.size();++i) {
        if(mesh->GetOptions()&D3DXMESH_32BIT) std::memcpy(&ix[i],static_cast<unsigned char*>(raw)+i*4,4);
        else { WORD v; std::memcpy(&v,static_cast<unsigned char*>(raw)+i*2,2); ix[i]=v; }
        if(ix[i]>=vertices || (adjacent && adjacent[i]!=0xffffffffu && adjacent[i]>=faces)) valid=false;
    }
    hr=mesh->UnlockIndexBuffer(); if(FAILED(hr)) return hr; if(!valid) return D3DERR_INVALIDCALL;
    hr=mesh->LockVertexBuffer(0,&raw); if(FAILED(hr)) return hr;
    std::vector<std::array<double,3>> p(vertices),sum(vertices,{{0,0,0}});
    std::vector<DWORD> reps(vertices);
    for(DWORD v=0;v<vertices;++v) {
        reps[v]=v; float xyz[3]; std::memcpy(xyz,static_cast<unsigned char*>(raw)+std::size_t(v)*stride+position,12);
        for(unsigned c=0;c<3;++c) { p[v][c]=xyz[c]; if(!std::isfinite(xyz[c])) valid=false; }
    }
    auto root=[&](DWORD v) { while(reps[v]!=v) { reps[v]=reps[reps[v]]; v=reps[v]; } return v; };
    if(adjacent) for(DWORD f=0;f<faces;++f) for(unsigned e=0;e<3;++e) {
        DWORD other=adjacent[f*3+e]; if(other==0xffffffffu) continue;
        DWORD a=ix[f*3+e],b=ix[f*3+(e+1)%3]; bool matched=false;
        for(unsigned j=0;j<3;++j) {
            DWORD c=ix[other*3+j],d=ix[other*3+(j+1)%3];
            if(adjacent[other*3+j]==f && p[a]==p[d] && p[b]==p[c]) {
                reps[root(a)]=root(d); reps[root(b)]=root(c); matched=true; break;
            }
        }
        if(!matched) valid=false;
    }
    if(valid) for(DWORD f=0;f<faces;++f) {
        const auto &a=p[ix[f*3]],&b=p[ix[f*3+1]],&c=p[ix[f*3+2]];
        double u[3],v[3],n[3];
        for(unsigned j=0;j<3;++j) { u[j]=b[j]-a[j]; v[j]=c[j]-a[j]; }
        n[0]=u[1]*v[2]-u[2]*v[1]; n[1]=u[2]*v[0]-u[0]*v[2]; n[2]=u[0]*v[1]-u[1]*v[0];
        double length=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]); if(!length) continue;
        for(double& component:n) component/=length;
        for(unsigned e=0;e<3;++e) {
            double dot=0,uu=0,vv=0;
            for(unsigned j=0;j<3;++j) { u[j]=p[ix[f*3+(e+1)%3]][j]-p[ix[f*3+e]][j]; v[j]=p[ix[f*3+(e+2)%3]][j]-p[ix[f*3+e]][j]; dot+=u[j]*v[j]; uu+=u[j]*u[j]; vv+=v[j]*v[j]; }
            const double angle=std::acos(std::max(-1.,std::min(1.,dot/std::sqrt(uu*vv))));
            auto& target=sum[root(ix[f*3+e])]; for(unsigned j=0;j<3;++j) target[j]+=n[j]*angle;
        }
    }
    if(valid) for(DWORD v=0;v<vertices;++v) {
        const auto& n=sum[root(v)]; const double length=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        float xyz[3]; for(unsigned j=0;j<3;++j) xyz[j]=length?float(n[j]/length):0;
        std::memcpy(static_cast<unsigned char*>(raw)+std::size_t(v)*stride+normal,xyz,12);
    }
    hr=mesh->UnlockVertexBuffer(); return FAILED(hr)?hr:valid?D3D_OK:D3DERR_INVALIDCALL;
}
// D3DX contract (Microsoft docs, D3DXConvertMeshSubsetToSingleStrip): the faces of attribute
// subset AttribId become one triangle strip whose separate runs are joined by degenerate
// triangles. The new index buffer keeps the mesh's 16- or 32-bit index format; IBOptions takes the
// D3DXMESH index-buffer flags (D3DXMESH_32BIT is refused) and selects pool and usage. The strip
// index count is returned through pNumIndices.
// [CHOSEN, not recovered] Microsoft library behaviour, not BF2 code. The strip is built by the
// vendored meshoptimizer stripifier (External/meshoptimizer, MIT); its triangle order and strip
// joins are not claimed to match d3dx9_25.dll. An empty subset returns D3DERR_INVALIDCALL.
HRESULT WINAPI D3DXConvertMeshSubsetToSingleStrip(ID3DXBaseMesh* meshIn,DWORD attribId,DWORD ibOptions,
    IDirect3DIndexBuffer9** out,DWORD* numIndices) {
    if(out) *out=nullptr; if(numIndices) *numIndices=0;
    if(!meshIn || !out || (ibOptions&D3DXMESH_32BIT)) return D3DERR_INVALIDCALL;
    // The attribute buffer is an ID3DXMesh method; every base mesh of this backend is one.
    const GUID meshId={0x4020e5c2,0x1403,0x4929,{0x88,0x3f,0xe2,0xe8,0x49,0xfa,0xc1,0x95}};
    ID3DXMesh* mesh=nullptr;
    if(FAILED(meshIn->QueryInterface(meshId,reinterpret_cast<void**>(&mesh))) || !mesh) return D3DERR_INVALIDCALL;
    struct Hold { ID3DXMesh* m; ~Hold() { m->Release(); } } hold{mesh};
    const DWORD faces=mesh->GetNumFaces(),vertices=mesh->GetNumVertices();
    const bool wide=(mesh->GetOptions()&D3DXMESH_32BIT)!=0;
    DWORD* attributes=nullptr; HRESULT hr=mesh->LockAttributeBuffer(D3DLOCK_READONLY,&attributes); if(FAILED(hr)) return hr;
    std::vector<DWORD> subset;
    void* raw=nullptr; hr=mesh->LockIndexBuffer(mesh->GetOptions()&0x400?0:D3DLOCK_READONLY,&raw);
    if(SUCCEEDED(hr)) {
        for(DWORD f=0;f<faces;++f) if(attributes[f]==attribId) for(unsigned e=0;e<3;++e) {
            DWORD v; if(wide) std::memcpy(&v,static_cast<const unsigned char*>(raw)+(std::size_t(f)*3+e)*4,4);
            else { WORD w; std::memcpy(&w,static_cast<const unsigned char*>(raw)+(std::size_t(f)*3+e)*2,2); v=w; }
            subset.push_back(v);
        }
        hr=mesh->UnlockIndexBuffer();
    }
    const HRESULT ah=mesh->UnlockAttributeBuffer(); if(FAILED(hr)) return hr; if(FAILED(ah)) return ah;
    if(subset.empty()) return D3DERR_INVALIDCALL;
    for(DWORD v:subset) if(v>=vertices) return D3DERR_INVALIDCALL;
    std::vector<unsigned int> source(subset.begin(),subset.end()),strip(meshopt_stripifyBound(source.size()));
    strip.resize(meshopt_stripify(strip.data(),source.data(),source.size(),vertices,0));
    if(strip.empty() || strip.size()>std::numeric_limits<UINT>::max()/4) return D3DERR_INVALIDCALL;
    const D3DPOOL pool=ibOptions&0x100?D3DPOOL_SYSTEMMEM:ibOptions&0x200?D3DPOOL_MANAGED:D3DPOOL_DEFAULT;
    const DWORD usage=(ibOptions&0x400?D3DUSAGE_WRITEONLY:0)|(ibOptions&0x800?D3DUSAGE_DYNAMIC:0);
    IDirect3DDevice9* device=nullptr; hr=mesh->GetDevice(&device); if(FAILED(hr)) return hr;
    IDirect3DIndexBuffer9* ib=nullptr;
    hr=device->CreateIndexBuffer(UINT(strip.size()*(wide?4:2)),usage,wide?D3DFMT_INDEX32:D3DFMT_INDEX16,pool,&ib,nullptr);
    device->Release(); if(FAILED(hr)) return hr;
    void* p=nullptr; hr=ib->Lock(0,0,&p,0);
    if(SUCCEEDED(hr)) {
        for(std::size_t i=0;i<strip.size();++i) {
            if(wide) std::memcpy(static_cast<unsigned char*>(p)+i*4,&strip[i],4);
            else { const WORD w=WORD(strip[i]); std::memcpy(static_cast<unsigned char*>(p)+i*2,&w,2); }
        }
        hr=ib->Unlock();
    }
    if(FAILED(hr)) { ib->Release(); return hr; }
    *out=ib; if(numIndices) *numIndices=DWORD(strip.size()); return D3D_OK;
}
#endif
