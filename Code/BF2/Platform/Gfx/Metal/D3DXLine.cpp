// [CHOSEN, not recovered] Native D3DX line subset over the device's programmable draw path.
#if (defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))) && defined(BF2_IMAGE_RENDDX9)
#include "../D3DX9Subset.h"
#include "../Effect/D3DAsm.h"
#if defined(__APPLE__)
#include "RefTrace.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>

namespace {
template<class T> void release(T*& p) { if(p) p->Release(); p=nullptr; }
void unsupported(unsigned mode) {
    static unsigned reported=0;
    if(reported&mode) return;
    reported|=mode;
    std::fprintf(stderr,"[metal] ID3DXLine::%s\n",
                 mode==1?"SetAntialias(TRUE): flag stored; edge antialiasing unsupported":"SetGLLines(TRUE) unsupported");
    std::fflush(stderr);
}
struct Point { float x,y,z,w; };
struct Vertex { Point position; D3DCOLOR colour; };
// Save only the state this helper changes. UP draws also unbind stream zero.
const std::pair<unsigned,DWORD> lineStates[] = {
    {7,0},{8,3},{14,0},{15,0},{19,5},{20,6},{22,1},{27,1},{28,0},
    {52,0},{152,0},{168,15},{174,0},{175,0},{176,0},{185,0},{194,0},{195,0},{206,0},{209,1}
};
class Line final : public ID3DXLine {
    ULONG refs=1;
    IDirect3DDevice9* device;
    IDirect3DVertexShader9 *vertexShader=nullptr,*savedVertexShader=nullptr;
    IDirect3DPixelShader9 *pixelShader=nullptr,*savedPixelShader=nullptr;
    IDirect3DVertexDeclaration9 *declaration=nullptr,*savedDeclaration=nullptr;
    IDirect3DVertexBuffer9* savedStream=nullptr;
    UINT savedOffset=0,savedStride=0;
    DWORD savedStates[sizeof(lineStates)/sizeof(*lineStates)]={};
    DWORD savedSamplers[21][2]={};
    UINT savedFrequencies[16]={};
    D3DVIEWPORT9 viewport={};
    DWORD pattern=0xffffffff;
    float scale=1,width=1;
    BOOL antialias=FALSE;
    bool begun=false;
public:
    explicit Line(IDirect3DDevice9* d):device(d) {
#if defined(__APPLE__)
        bf2metal::reftrace::Owned owned;
#endif
        device->AddRef();
#if defined(__APPLE__)
        bf2metal::reftrace::born(this,"ID3DXLine",[](const void* p) { return static_cast<unsigned long>(static_cast<const Line*>(p)->refs); });
#endif
    }
    ~Line() {
        if(begun) End();
        release(vertexShader); release(pixelShader); release(declaration);
#if defined(__APPLE__)
        bf2metal::reftrace::died(this);
        bf2metal::reftrace::Owned owned;
#endif
        device->Release();
    }
    HRESULT initialize() {
        const D3DVERTEXELEMENT9 elements[]={
            {0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITION,0},
            {0,16,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},D3DDECL_END()};
        HRESULT hr=device->CreateVertexDeclaration(elements,&declaration);
        const char* shaders[]={"vs_1_1\ndcl_position v0\ndcl_color v1\nmov oPos, v0\nmov oD0, v1\n",
                               "ps_1_1\nmov r0, v0\n"};
        for(unsigned i=0;i<2 && SUCCEEDED(hr);++i) {
            bf2fx::Bytes code; std::string error;
            if(!bf2fx::assemble(shaders[i],code,error)) {
                std::fprintf(stderr,"[metal] ID3DXLine shader: %s\n",error.c_str());
                return D3DERR_INVALIDCALL;
            }
            hr=i?device->CreatePixelShader(reinterpret_cast<const DWORD*>(code.data()),&pixelShader):
                 device->CreateVertexShader(reinterpret_cast<const DWORD*>(code.data()),&vertexShader);
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL;
        *out=nullptr;
        const GUID unknown={0,0,0,{0xc0,0,0,0,0,0,0,0x46}},line={0xd379ba7f,0x9042,0x4ac4,{0x9f,0x5e,0x58,0x19,0x2a,0x4c,0x6b,0xd8}};
        if(std::memcmp(&id,&unknown,sizeof(id)) && std::memcmp(&id,&line,sizeof(id))) return E_NOINTERFACE;
        *out=this; AddRef(); return D3D_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG n=--refs; if(!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override {
        if(!out) return D3DERR_INVALIDCALL;
        *out=device; device->AddRef(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE Begin() override {
        if(begun) return D3DERR_INVALIDCALL;
        HRESULT hr=device->GetViewport(&viewport);
        if(FAILED(hr) || !viewport.Width || !viewport.Height) return D3DERR_INVALIDCALL;
        hr=device->GetVertexShader(&savedVertexShader);
        if(SUCCEEDED(hr)) hr=device->GetPixelShader(&savedPixelShader);
        if(SUCCEEDED(hr)) hr=device->GetVertexDeclaration(&savedDeclaration);
        if(SUCCEEDED(hr)) hr=device->GetStreamSource(0,&savedStream,&savedOffset,&savedStride);
        for(unsigned i=0;i<sizeof(lineStates)/sizeof(*lineStates) && SUCCEEDED(hr);++i)
            hr=device->GetRenderState(D3DRENDERSTATETYPE(lineStates[i].first),&savedStates[i]);
        // The shared draw validator checks every sampler and stream, even when our shader
        // reads no texture. Do not inherit a scene effect's LOD/sRGB or instancing state.
        for(unsigned i=0;i<21 && SUCCEEDED(hr);++i) {
            const unsigned slot=i<16?i:i+240;
            hr=device->GetSamplerState(slot,D3DSAMP_MIPMAPLODBIAS,&savedSamplers[i][0]);
            if(SUCCEEDED(hr)) hr=device->GetSamplerState(slot,D3DSAMP_SRGBTEXTURE,&savedSamplers[i][1]);
        }
        for(unsigned i=0;i<16 && SUCCEEDED(hr);++i) hr=device->GetStreamSourceFreq(i,&savedFrequencies[i]);
        if(FAILED(hr)) {
            release(savedVertexShader); release(savedPixelShader); release(savedDeclaration); release(savedStream);
            return hr;
        }
        begun=true;
        hr=device->SetVertexShader(vertexShader);
        if(SUCCEEDED(hr)) hr=device->SetPixelShader(pixelShader);
        if(SUCCEEDED(hr)) hr=device->SetVertexDeclaration(declaration);
        for(const auto& s:lineStates) if(SUCCEEDED(hr)) hr=device->SetRenderState(D3DRENDERSTATETYPE(s.first),s.second);
        for(unsigned i=0;i<21 && SUCCEEDED(hr);++i) {
            const unsigned slot=i<16?i:i+240;
            hr=device->SetSamplerState(slot,D3DSAMP_MIPMAPLODBIAS,0);
            if(SUCCEEDED(hr)) hr=device->SetSamplerState(slot,D3DSAMP_SRGBTEXTURE,0);
        }
        for(unsigned i=0;i<16 && SUCCEEDED(hr);++i) hr=device->SetStreamSourceFreq(i,1);
        if(FAILED(hr)) End();
        return hr;
    }
    HRESULT STDMETHODCALLTYPE End() override {
        if(!begun) return D3DERR_INVALIDCALL;
        HRESULT result=D3D_OK;
        auto check=[&](HRESULT hr) { if(FAILED(hr) && SUCCEEDED(result)) result=hr; };
        for(unsigned i=0;i<sizeof(lineStates)/sizeof(*lineStates);++i)
            check(device->SetRenderState(D3DRENDERSTATETYPE(lineStates[i].first),savedStates[i]));
        for(unsigned i=0;i<21;++i) {
            const unsigned slot=i<16?i:i+240;
            check(device->SetSamplerState(slot,D3DSAMP_MIPMAPLODBIAS,savedSamplers[i][0]));
            check(device->SetSamplerState(slot,D3DSAMP_SRGBTEXTURE,savedSamplers[i][1]));
        }
        for(unsigned i=0;i<16;++i) check(device->SetStreamSourceFreq(i,savedFrequencies[i]));
        check(device->SetVertexShader(savedVertexShader)); check(device->SetPixelShader(savedPixelShader));
        check(device->SetVertexDeclaration(savedDeclaration)); check(device->SetStreamSource(0,savedStream,savedOffset,savedStride));
        release(savedVertexShader); release(savedPixelShader); release(savedDeclaration); release(savedStream);
        begun=false; return result;
    }
    HRESULT quad(Point a,Point b,D3DCOLOR colour) {
        float dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);
        if(!length) return D3D_OK;
        const float nx=-dy/length*width/2,ny=dx/length*width/2;
        auto vertex=[&](Point p,float sign) {
            // CPU coordinates are D3D pixels; the device shader path applies its half-pixel fixup.
            return Vertex{{(2*(p.x+sign*nx-viewport.X)/viewport.Width-1)*p.w,
                           (1-2*(p.y+sign*ny-viewport.Y)/viewport.Height)*p.w,p.z*p.w,p.w},colour};
        };
        const Vertex v[]={vertex(a,1),vertex(a,-1),vertex(b,1),vertex(b,-1)};
        return device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));
    }
    HRESULT segment(Point a,Point b,D3DCOLOR colour,double& phase) {
        const float length=std::hypot(b.x-a.x,b.y-a.y);
        if(!std::isfinite(length)) return D3DERR_INVALIDCALL;
        if(pattern==0xffffffff) return quad(a,b,colour);
        if(!pattern) { phase+=length; return D3D_OK; }
        double t=0;
        while(t<length) {
            const double bit=std::floor((phase+t)/scale);
            const double next=std::min(double(length),(bit+1)*scale-phase);
            if(next<=t) return D3DERR_INVALIDCALL;
            if(pattern&(DWORD(1)<<unsigned(std::fmod(bit,32.0)))) {
                auto at=[&](double d) {
                    float f=float(d/length);
                    // Perspective-correct depth is linear in screen coordinates after division.
                    return Point{a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f,a.z+(b.z-a.z)*f,1};
                };
                HRESULT hr=quad(at(t),at(next),colour); if(FAILED(hr)) return hr;
            }
            t=next;
        }
        phase+=length; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE Draw(const D3DXVECTOR2* points,DWORD count,D3DCOLOR colour) override {
        if(!begun || !points || count<2) return D3DERR_INVALIDCALL;
        double phase=0;
        for(DWORD i=1;i<count;++i) {
            if(!std::isfinite(points[i-1].x+points[i-1].y+points[i].x+points[i].y)) return D3DERR_INVALIDCALL;
            HRESULT hr=segment({points[i-1].x,points[i-1].y,0,1},{points[i].x,points[i].y,0,1},colour,phase);
            if(FAILED(hr)) return hr;
        }
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawTransform(const D3DXVECTOR3* points,DWORD count,const D3DXMATRIX* matrix,D3DCOLOR colour) override {
        if(!begun || !points || !matrix || count<2) return D3DERR_INVALIDCALL;
        auto transform=[&](const D3DXVECTOR3& p) {
            const auto& m=matrix->m;
            return Point{p.x*m[0][0]+p.y*m[1][0]+p.z*m[2][0]+m[3][0],
                         p.x*m[0][1]+p.y*m[1][1]+p.z*m[2][1]+m[3][1],
                         p.x*m[0][2]+p.y*m[1][2]+p.z*m[2][2]+m[3][2],
                         p.x*m[0][3]+p.y*m[1][3]+p.z*m[2][3]+m[3][3]};
        };
        double phase=0;
        for(DWORD i=1;i<count;++i) {
            Point a=transform(points[i-1]),b=transform(points[i]);
            if(!std::isfinite(a.x+a.y+a.z+a.w+b.x+b.y+b.z+b.w)) return D3DERR_INVALIDCALL;
            // Clip before division, including the D3D near/far planes. A segment crossing the
            // camera must never turn into a giant screen-filling quad.
            float lo=0,hi=1;
            const float da[]={a.x+a.w,a.w-a.x,a.y+a.w,a.w-a.y,a.z,a.w-a.z,a.w-1e-6f};
            const float db[]={b.x+b.w,b.w-b.x,b.y+b.w,b.w-b.y,b.z,b.w-b.z,b.w-1e-6f};
            for(unsigned p=0;p<7;++p) {
                if(da[p]<0 && db[p]<0) { hi=-1; break; }
                if(da[p]<0) lo=std::max(lo,da[p]/(da[p]-db[p]));
                if(db[p]<0) hi=std::min(hi,da[p]/(da[p]-db[p]));
            }
            if(lo>hi) continue;
            auto project=[&](float t) {
                Point p={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
                return Point{viewport.X+(p.x/p.w+1)*viewport.Width/2,
                             viewport.Y+(1-p.y/p.w)*viewport.Height/2,p.z/p.w,1};
            };
            HRESULT hr=segment(project(lo),project(hi),colour,phase); if(FAILED(hr)) return hr;
        }
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPattern(DWORD v) override { pattern=v; return D3D_OK; }
    DWORD STDMETHODCALLTYPE GetPattern() override { return pattern; }
    HRESULT STDMETHODCALLTYPE SetPatternScale(FLOAT v) override { if(!(v>0) || !std::isfinite(v)) return D3DERR_INVALIDCALL; scale=v; return D3D_OK; }
    FLOAT STDMETHODCALLTYPE GetPatternScale() override { return scale; }
    HRESULT STDMETHODCALLTYPE SetWidth(FLOAT v) override { if(!(v>0) || !std::isfinite(v)) return D3DERR_INVALIDCALL; width=v; return D3D_OK; }
    FLOAT STDMETHODCALLTYPE GetWidth() override { return width; }
    HRESULT STDMETHODCALLTYPE SetAntialias(BOOL v) override { antialias=v?TRUE:FALSE; if(v) unsupported(1); return D3D_OK; }
    BOOL STDMETHODCALLTYPE GetAntialias() override { return antialias; }
    HRESULT STDMETHODCALLTYPE SetGLLines(BOOL v) override { if(v) { unsupported(2); return E_NOTIMPL; } return D3D_OK; }
    BOOL STDMETHODCALLTYPE GetGLLines() override { return FALSE; }
    // Metal has no D3D9 lost-device resources. End still restores a pending batch's state.
    HRESULT STDMETHODCALLTYPE OnLostDevice() override { return begun?End():D3D_OK; }
    HRESULT STDMETHODCALLTYPE OnResetDevice() override { return begun?D3DERR_INVALIDCALL:D3D_OK; }
};
}
extern "C" HRESULT WINAPI D3DXCreateLine(IDirect3DDevice9* device,ID3DXLine** out) {
    if(!out) return D3DERR_INVALIDCALL;
    *out=nullptr;
    if(!device) return D3DERR_INVALIDCALL;
    auto* line=new(std::nothrow) Line(device);
    if(!line) return E_OUTOFMEMORY;
    HRESULT hr=line->initialize();
    if(FAILED(hr)) { line->Release(); return hr; }
    *out=line; return D3D_OK;
}
#endif
