// [CHOSEN, not recovered] Native D3DX math, row vectors and row-major matrices.
#if (defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))) && defined(BF2_IMAGE_RENDDX9)
#include "D3DXHelpers.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <cctype>

namespace bf2d3dx {
[[noreturn]] void stop(const char* name) {
    std::fprintf(stderr,"[FRONTIER] %s reached: native D3DX operation is unimplemented\n",name);
    std::fflush(stderr); std::abort();
}
std::uint16_t half(float f) {
    std::uint32_t u; std::memcpy(&u,&f,4);
    const unsigned sign=(u>>16)&0x8000, e=(u>>23)&255, m=u&0x7fffff;
    if(e==255) return sign|0x7c00|(m?((m>>13)|0x200):0);
    if(e>142) return sign|0x7c00;
    if(e<102) return sign;
    if(e<113) {
        const unsigned shift=126-e, mant=m|0x800000;
        const unsigned q=mant>>shift, rem=mant&((1u<<shift)-1), tie=1u<<(shift-1);
        return sign|(q+(rem>tie || (rem==tie && (q&1))));
    }
    unsigned v=((e-112)<<10)|(m>>13), rem=m&8191;
    v+=rem>4096 || (rem==4096 && (v&1));
    return sign|v;
}
float unhalf(std::uint16_t h) {
    const unsigned e=(h>>10)&31,m=h&1023;
    float f=e==31?(m?NAN:INFINITY):e?std::ldexp(float(1024+m),int(e)-25):std::ldexp(float(m),-24);
    return h&0x8000?-f:f;
}
class Buffer final : public ID3DXBuffer {
    ULONG refs=1;
    Bytes bytes;
public:
    explicit Buffer(DWORD n):bytes(n) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        const GUID unknown={0,0,0,{0xc0,0,0,0,0,0,0,0x46}};
        const GUID buffer={0x8ba5fb08,0x5195,0x40e2,{0xac,0x58,0x0d,0x98,0x9c,0x3a,0x01,0x02}};
        if(std::memcmp(&id,&unknown,16) && std::memcmp(&id,&buffer,16)) return E_NOINTERFACE;
        *out=this; AddRef(); return D3D_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { const auto n=--refs; if(!n) delete this; return n; }
    void* STDMETHODCALLTYPE GetBufferPointer() override { return bytes.data(); }
    DWORD STDMETHODCALLTYPE GetBufferSize() override { return DWORD(bytes.size()); }
};
}
HRESULT WINAPI D3DXCreateBuffer(DWORD n,ID3DXBuffer** out) {
    if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
    try { *out=new bf2d3dx::Buffer(n); return D3D_OK; } catch(const std::bad_alloc&) { return E_OUTOFMEMORY; }
}
DWORD WINAPI D3DXGetShaderVersion(const DWORD* p) { return p?*p:0; }
HRESULT WINAPI D3DXAssembleShader(LPCSTR source,UINT length,const D3DXMACRO* defines,ID3DXInclude* include,DWORD flags,ID3DXBuffer** shader,ID3DXBuffer** errors) {
    if(shader) *shader=nullptr; if(errors) *errors=nullptr;
    if(!source || !shader) return D3DERR_INVALIDCALL;
    // VideoOptions::apply reaches the two AA probes during startup. Support
    // their complete SM1 assembly, leaving other assembly an explicit frontier.
    // [RETIRE-WHEN: macos-d3dx-assembler] Replace this narrow assembler after a
    // general token assembler has public shader-bytecode golden tests.
    if(defines || include || flags) bf2d3dx::stop("D3DXAssembleShader preprocessing");
    std::string text;
    for(unsigned i=0;i<length;++i) if(!std::isspace(static_cast<unsigned char>(source[i]))) text+=char(std::tolower(static_cast<unsigned char>(source[i])));
    std::vector<DWORD> words;
    // D3DX9_25 retains DCL_POSITION0 in SM1 bytecode. DXVK needs it to map v0
    // to the vertex declaration; omitting it makes the renderer's AA probe draw no pixels.
    if(text=="vs.1.1dcl_position0v0movopos,v0") words={0xfffe0101,0x1f,0x80000000,0x900f0000,1,0xc00f0000,0x90e40000,0x0000ffff};
    else if(text=="ps.1.1defc0,1,1,1,0movr0,c0" || text=="ps.1.1defc0,1,1,1,0.5movr0,c0") {
        const float alpha=text.find("0.5")!=std::string::npos?.5f:0.f; DWORD bits; std::memcpy(&bits,&alpha,4);
        words={0xffff0101,81,0xa00f0000,0x3f800000,0x3f800000,0x3f800000,bits,1,0x800f0000,0xa0e40000,0x0000ffff};
    } else bf2d3dx::stop("D3DXAssembleShader assembly outside renderer AA probes");
    HRESULT hr=D3DXCreateBuffer(DWORD(words.size()*4),shader);
    if(SUCCEEDED(hr)) std::memcpy((*shader)->GetBufferPointer(),words.data(),words.size()*4); return hr;
}
D3DXFLOAT16* WINAPI D3DXFloat32To16Array(D3DXFLOAT16* out,const FLOAT* in,UINT n) {
    if(!out || !in) return nullptr;
    for(unsigned i=0;i<n;++i) out[i].value=bf2d3dx::half(in[i]); return out;
}
D3DXMATRIX* WINAPI D3DXMatrixMultiply(D3DXMATRIX* out,const D3DXMATRIX* a,const D3DXMATRIX* b) {
    if(!out || !a || !b) return nullptr;
    D3DXMATRIX t={};
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<4;++j)
        t.m[i][j]=a->m[i][0]*b->m[0][j]+a->m[i][1]*b->m[1][j]+a->m[i][2]*b->m[2][j]+a->m[i][3]*b->m[3][j];
    *out=t; return out;
}
D3DXMATRIX* WINAPI D3DXMatrixTranspose(D3DXMATRIX* out,const D3DXMATRIX* a) {
    if(!out || !a) return nullptr; D3DXMATRIX t;
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<4;++j) t.m[i][j]=a->m[j][i]; *out=t; return out;
}
D3DXMATRIX* WINAPI D3DXMatrixInverse(D3DXMATRIX* out,FLOAT* det,const D3DXMATRIX* a) {
    if(!out || !a) return nullptr;
    D3DXMATRIX cof;
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<4;++j) {
        float v[9]; unsigned n=0;
        for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) if(y!=i && x!=j) v[n++]=a->m[y][x];
        cof.m[i][j]=((i+j)&1?-1.f:1.f)*(v[0]*(v[4]*v[8]-v[5]*v[7])-v[1]*(v[3]*v[8]-v[5]*v[6])+v[2]*(v[3]*v[7]-v[4]*v[6]));
    }
    const float d=a->m[0][0]*cof.m[0][0]+a->m[0][1]*cof.m[0][1]+a->m[0][2]*cof.m[0][2]+a->m[0][3]*cof.m[0][3];
    if(det) *det=d; if(d==0) return nullptr;
    D3DXMatrixTranspose(out,&cof);
    for(auto& row:out->m) for(float& f:row) f/=d; return out;
}
D3DXMATRIX* WINAPI D3DXMatrixRotationZ(D3DXMATRIX* out,FLOAT a) {
    if(!out) return nullptr; *out={}; const float c=std::cos(a),s=std::sin(a);
    out->m[0][0]=out->m[1][1]=c; out->m[0][1]=s; out->m[1][0]=-s;
    out->m[2][2]=out->m[3][3]=1; return out;
}
D3DXMATRIX* WINAPI D3DXMatrixOrthoLH(D3DXMATRIX* out,FLOAT w,FLOAT h,FLOAT zn,FLOAT zf) {
    if(!out) return nullptr; *out={};
    out->m[0][0]=2/w; out->m[1][1]=2/h; out->m[2][2]=1/(zf-zn); out->m[3][2]=zn/(zn-zf); out->m[3][3]=1; return out;
}
D3DXMATRIX* WINAPI D3DXMatrixLookAtLH(D3DXMATRIX* out,const D3DXVECTOR3* eye,const D3DXVECTOR3* at,const D3DXVECTOR3* up) {
    if(!out || !eye || !at || !up) return nullptr;
    auto norm=[](D3DXVECTOR3 v) { const float n=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); return n?D3DXVECTOR3(v.x/n,v.y/n,v.z/n):D3DXVECTOR3(0,0,0); };
    auto cross=[](D3DXVECTOR3 a,D3DXVECTOR3 b) { return D3DXVECTOR3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x); };
    const auto z=norm(D3DXVECTOR3(at->x-eye->x,at->y-eye->y,at->z-eye->z)),x=norm(cross(*up,z)),y=cross(z,x);
    *out={}; const D3DXVECTOR3 axes[]={x,y,z};
    for(unsigned j=0;j<3;++j) { const auto v=axes[j]; out->m[0][j]=v.x; out->m[1][j]=v.y; out->m[2][j]=v.z; out->m[3][j]=-(v.x*eye->x+v.y*eye->y+v.z*eye->z); }
    out->m[3][3]=1; return out;
}
HRESULT WINAPI D3DXComputeBoundingBox(const D3DXVECTOR3* first,DWORD n,DWORD stride,D3DXVECTOR3* lo,D3DXVECTOR3* hi) {
    if(!first || !n || stride<12 || !lo || !hi) return D3DERR_INVALIDCALL;
    D3DXVECTOR3 a=*first,b=a;
    for(unsigned i=1;i<n;++i) { D3DXVECTOR3 p; std::memcpy(&p,reinterpret_cast<const unsigned char*>(first)+std::size_t(i)*stride,12);
        a.x=std::min(a.x,p.x); a.y=std::min(a.y,p.y); a.z=std::min(a.z,p.z);
        b.x=std::max(b.x,p.x); b.y=std::max(b.y,p.y); b.z=std::max(b.z,p.z); }
    *lo=a; *hi=b; return D3D_OK;
}
HRESULT WINAPI D3DXComputeBoundingSphere(const D3DXVECTOR3* first,DWORD n,DWORD stride,D3DXVECTOR3* center,FLOAT* radius) {
    if(!first || !n || stride<12 || !center || !radius) return D3DERR_INVALIDCALL;
    D3DXVECTOR3 c(0,0,0),p;
    for(unsigned i=0;i<n;++i) { std::memcpy(&p,reinterpret_cast<const unsigned char*>(first)+std::size_t(i)*stride,12); c.x+=p.x; c.y+=p.y; c.z+=p.z; }
    c.x/=n; c.y/=n; c.z/=n; float r2=0;
    for(unsigned i=0;i<n;++i) { std::memcpy(&p,reinterpret_cast<const unsigned char*>(first)+std::size_t(i)*stride,12);
        const float x=p.x-c.x,y=p.y-c.y,z=p.z-c.z; r2=std::max(r2,x*x+y*y+z*z); }
    *center=c; *radius=std::sqrt(r2); return D3D_OK;
}
#endif
