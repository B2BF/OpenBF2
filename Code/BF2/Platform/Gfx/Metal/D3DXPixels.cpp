// [CHOSEN, not recovered] D3D-packed color conversion and CPU image filtering.
#if (defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))) && defined(BF2_IMAGE_RENDDX9)
#include "D3DXHelpers.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace bf2d3dx {
unsigned blockSize(D3DFORMAT f) { return f==D3DFMT_DXT1 || f==D3DFMT_DXT3 || f==D3DFMT_DXT5?4:1; }
unsigned pixelBytes(D3DFORMAT f) {
    switch(f) {
    case D3DFMT_R8G8B8: return 3;
    case D3DFMT_A8: case D3DFMT_L8: return 1;
    case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4: case D3DFMT_A8L8: case D3DFMT_L16: case D3DFMT_R16F: return 2;
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8:
    case D3DFMT_G16R16: case D3DFMT_A2R10G10B10: case D3DFMT_A2B10G10R10: case D3DFMT_R32F: case D3DFMT_G16R16F: return 4;
    case D3DFMT_A16B16G16R16: case D3DFMT_A16B16G16R16F: case D3DFMT_G32R32F: case D3DFMT_DXT1: return 8;
    case D3DFMT_A32B32G32R32F: case D3DFMT_DXT3: case D3DFMT_DXT5: return 16;
    default: return 0;
    }
}
unsigned rowBytes(D3DFORMAT f,unsigned w) { return ((w+blockSize(f)-1)/blockSize(f))*pixelBytes(f); }
std::size_t imageBytes(D3DFORMAT f,unsigned w,unsigned h,unsigned d) { return std::size_t(rowBytes(f,w))*((h+blockSize(f)-1)/blockSize(f))*d; }
static unsigned quant(double c,unsigned max) { return unsigned(std::max(0.,std::min(1.,c))*max+0.5); }
static unsigned get(const unsigned char* p,unsigned n) { unsigned v=0; std::memcpy(&v,p,n); return v; }
static void put(unsigned char* p,unsigned v,unsigned n) { std::memcpy(p,&v,n); }
Color decode(D3DFORMAT f,const unsigned char* p) {
    Color c; unsigned v=0;
    if(pixelBytes(f)<=4) v=get(p,pixelBytes(f));
    switch(f) {
    case D3DFMT_R8G8B8: case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8:
        c.r=p[2]/255.; c.g=p[1]/255.; c.b=p[0]/255.; if(f==D3DFMT_A8R8G8B8) c.a=p[3]/255.; break;
    case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8:
        c.r=p[0]/255.; c.g=p[1]/255.; c.b=p[2]/255.; if(f==D3DFMT_A8B8G8R8) c.a=p[3]/255.; break;
    case D3DFMT_R5G6B5: c.r=(v>>11)/31.; c.g=((v>>5)&63)/63.; c.b=(v&31)/31.; break;
    case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5:
        c.r=((v>>10)&31)/31.; c.g=((v>>5)&31)/31.; c.b=(v&31)/31.; if(f==D3DFMT_A1R5G5B5) c.a=v>>15; break;
    case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4:
        c.r=((v>>8)&15)/15.; c.g=((v>>4)&15)/15.; c.b=(v&15)/15.; if(f==D3DFMT_A4R4G4B4) c.a=(v>>12)/15.; break;
    case D3DFMT_A8: c.r=c.g=c.b=1; c.a=p[0]/255.; break;
    case D3DFMT_L8: case D3DFMT_A8L8: c.r=c.g=c.b=p[0]/255.; if(f==D3DFMT_A8L8) c.a=p[1]/255.; break;
    case D3DFMT_L16: c.r=c.g=c.b=v/65535.; break;
    case D3DFMT_G16R16: case D3DFMT_A16B16G16R16:
        for(unsigned i=0;i<(f==D3DFMT_G16R16?2u:4u);++i) c[i]=get(p+2*i,2)/65535.; break;
    case D3DFMT_A2B10G10R10: case D3DFMT_A2R10G10B10:
        c.r=(v&1023)/1023.; c.g=((v>>10)&1023)/1023.; c.b=((v>>20)&1023)/1023.; c.a=(v>>30)/3.;
        if(f==D3DFMT_A2R10G10B10) std::swap(c.r,c.b); break;
    case D3DFMT_R16F: case D3DFMT_G16R16F: case D3DFMT_A16B16G16R16F:
        for(unsigned i=0;i<pixelBytes(f)/2;++i) c[i]=unhalf(get(p+2*i,2)); break;
    case D3DFMT_R32F: case D3DFMT_G32R32F: case D3DFMT_A32B32G32R32F:
        for(unsigned i=0;i<pixelBytes(f)/4;++i) { float x; std::memcpy(&x,p+4*i,4); c[i]=x; } break;
    default: stop("bf2d3dx::decode format");
    }
    return c;
}
void encode(D3DFORMAT f,unsigned char* p,const Color& c) {
    unsigned v=0;
    switch(f) {
    case D3DFMT_R8G8B8: case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8:
        p[2]=quant(c.r,255); p[1]=quant(c.g,255); p[0]=quant(c.b,255);
        if(f!=D3DFMT_R8G8B8) p[3]=f==D3DFMT_X8R8G8B8?255:quant(c.a,255); return;
    case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8:
        p[0]=quant(c.r,255); p[1]=quant(c.g,255); p[2]=quant(c.b,255); p[3]=f==D3DFMT_X8B8G8R8?255:quant(c.a,255); return;
    case D3DFMT_R5G6B5: v=(quant(c.r,31)<<11)|(quant(c.g,63)<<5)|quant(c.b,31); break;
    case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5:
        v=(quant(c.r,31)<<10)|(quant(c.g,31)<<5)|quant(c.b,31)|((f==D3DFMT_X1R5G5B5?1:quant(c.a,1))<<15); break;
    case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4:
        v=(quant(c.r,15)<<8)|(quant(c.g,15)<<4)|quant(c.b,15)|((f==D3DFMT_X4R4G4B4?15:quant(c.a,15))<<12); break;
    case D3DFMT_A8: v=quant(c.a,255); break;
    case D3DFMT_L8: v=quant(c.r,255); break;
    case D3DFMT_A8L8: v=quant(c.r,255)|(quant(c.a,255)<<8); break;
    case D3DFMT_L16: v=quant(c.r,65535); break;
    case D3DFMT_G16R16: case D3DFMT_A16B16G16R16:
        for(unsigned i=0;i<(f==D3DFMT_G16R16?2u:4u);++i) put(p+2*i,quant(c[i],65535),2); return;
    case D3DFMT_A2B10G10R10: case D3DFMT_A2R10G10B10:
        v=quant(c[f==D3DFMT_A2R10G10B10?2:0],1023)|(quant(c.g,1023)<<10)|
            (quant(c[f==D3DFMT_A2R10G10B10?0:2],1023)<<20)|(quant(c.a,3)<<30); break;
    case D3DFMT_R16F: case D3DFMT_G16R16F: case D3DFMT_A16B16G16R16F:
        for(unsigned i=0;i<pixelBytes(f)/2;++i) put(p+2*i,half(float(c[i])),2); return;
    case D3DFMT_R32F: case D3DFMT_G32R32F: case D3DFMT_A32B32G32R32F:
        for(unsigned i=0;i<pixelBytes(f)/4;++i) { float x=float(c[i]); std::memcpy(p+4*i,&x,4); } return;
    default: stop("bf2d3dx::encode format");
    }
    put(p,v,pixelBytes(f));
}
static Color mix(const Color& a,const Color& b,double t) {
    Color c; for(unsigned k=0;k<4;++k) c[k]=a[k]*(1-t)+b[k]*t; return c;
}
static void bcPalette(const unsigned char* p,Color* c,bool alpha) {
    unsigned a=get(p,2),b=get(p+2,2); c[0]=decode(D3DFMT_R5G6B5,p); c[1]=decode(D3DFMT_R5G6B5,p+2);
    if(a>b || !alpha) { c[2]=mix(c[0],c[1],1./3); c[3]=mix(c[0],c[1],2./3); }
    else { c[2]=mix(c[0],c[1],.5); c[3]={0,0,0,0}; }
}
static void alphaPalette(unsigned a,unsigned b,double* p) {
    p[0]=a/255.; p[1]=b/255.;
    if(a>b) for(unsigned i=2;i<8;++i) p[i]=((8-i)*a+(i-1)*b)/(7.*255);
    else { for(unsigned i=2;i<6;++i) p[i]=((6-i)*a+(i-1)*b)/(5.*255); p[6]=0; p[7]=1; }
}
static Color readPixel(const unsigned char* src,D3DFORMAT f,unsigned pitch,unsigned x,unsigned y) {
    if(blockSize(f)==1) return decode(f,src+std::size_t(y)*pitch+x*pixelBytes(f));
    const auto* p=src+std::size_t(y/4)*pitch+(x/4)*pixelBytes(f); const unsigned i=(y%4)*4+x%4;
    const auto* rgb=p+(f==D3DFMT_DXT1?0:8); Color palette[4]; bcPalette(rgb,palette,f==D3DFMT_DXT1);
    Color c=palette[(get(rgb+4,4)>>(2*i))&3];
    if(f==D3DFMT_DXT3) c.a=((p[i/2]>>(4*(i%2)))&15)/15.;
    if(f==D3DFMT_DXT5) { double a[8]; alphaPalette(p[0],p[1],a); std::uint64_t bits=0; std::memcpy(&bits,p+2,6); c.a=a[(bits>>(3*i))&7]; }
    return c;
}
// Deterministic BC encoder. Endpoint choice is native policy, not a claim about
// the proprietary D3DX compressor's optimization or byte-identical output.
static void writeBlock(D3DFORMAT f,unsigned char* p,const Color* c) {
    Color lo{1,1,1,1},hi{0,0,0,0}; bool transparent=false;
    for(unsigned i=0;i<16;++i) { transparent|=c[i].a<.5; for(unsigned k=0;k<4;++k) { lo[k]=std::min(lo[k],c[i][k]); hi[k]=std::max(hi[k],c[i][k]); } }
    auto* rgb=p+(f==D3DFMT_DXT1?0:8); encode(D3DFMT_R5G6B5,rgb,hi); encode(D3DFMT_R5G6B5,rgb+2,lo);
    unsigned a=get(rgb,2),b=get(rgb+2,2);
    if(f==D3DFMT_DXT1 && transparent) { if(a>b) std::swap(a,b); }
    else { if(a<b) std::swap(a,b); if(a==b) { if(a<65535) ++a; else --b; } }
    put(rgb,a,2); put(rgb+2,b,2); Color pal[4]; bcPalette(rgb,pal,f==D3DFMT_DXT1); unsigned bits=0;
    for(unsigned i=0;i<16;++i) {
        unsigned best=0; double dist=std::numeric_limits<double>::infinity();
        if(f==D3DFMT_DXT1 && transparent && c[i].a<.5) best=3;
        else for(unsigned j=0;j<(f==D3DFMT_DXT1 && transparent?3u:4u);++j) {
            double d=0; for(unsigned k=0;k<3;++k) { double t=c[i][k]-pal[j][k]; d+=t*t; } if(d<dist) { best=j; dist=d; }
        }
        bits|=best<<(2*i);
    }
    put(rgb+4,bits,4);
    if(f==D3DFMT_DXT3) { std::memset(p,0,8); for(unsigned i=0;i<16;++i) p[i/2]|=quant(c[i].a,15)<<(4*(i%2)); }
    if(f==D3DFMT_DXT5) {
        p[0]=quant(hi.a,255); p[1]=quant(lo.a,255); double palA[8]; alphaPalette(p[0],p[1],palA); std::uint64_t abits=0;
        for(unsigned i=0;i<16;++i) { unsigned best=0; double dist=2; for(unsigned j=0;j<8;++j) { double d=std::abs(c[i].a-palA[j]); if(d<dist) { dist=d; best=j; } } abits|=std::uint64_t(best)<<(3*i); }
        std::memcpy(p+2,&abits,6);
    }
}
struct Weight { unsigned index; double value; };
static unsigned boundary(int i,unsigned n,bool mirror) {
    int period=int(n)*(mirror?2:1); i%=period; if(i<0) i+=period;
    return mirror && unsigned(i)>=n?2*n-1-unsigned(i):unsigned(i);
}
static std::vector<Weight> weights(unsigned x,unsigned src,unsigned dst,DWORD filter,bool mirror) {
    std::vector<Weight> out;
    auto add=[&](int i,double w) { if(w>0) out.push_back({boundary(i,src,mirror),w}); };
    const double scale=double(src)/dst,u=(x+.5)*scale-.5;
    if(src==dst) add(x,1);
    else if(filter==D3DX_FILTER_POINT) add(int(std::floor(u+.5)),1);
    else if(filter==D3DX_FILTER_LINEAR) { int a=int(std::floor(u)); double t=u-a; add(a,1-t); add(a+1,t); }
    else if(filter==D3DX_FILTER_BOX) { add(x*2,src==1?1:.5); if(src>1) add(x*2+1,.5); }
    else {
        // Integrate a triangular reconstruction kernel over the destination
        // footprint. This gives each covered source texel its area contribution.
        auto integral=[](double t) { if(t<=-1) return 0.; if(t<0) return .5*(t+1)*(t+1); if(t<1) return 1-.5*(1-t)*(1-t); return 1.; };
        const double a=x*scale-.5,b=(x+1)*scale-.5;
        for(int i=int(std::floor(a))-1;i<=int(std::ceil(b))+1;++i) add(i,(integral(b-i)-integral(a-i))/scale);
    }
    return out;
}
bool convert(const void* src,D3DFORMAT sf,unsigned sw,unsigned sh,unsigned sp,void* dst,D3DFORMAT df,unsigned dw,unsigned dh,unsigned dp,DWORD filter,D3DCOLOR key) {
    if(!src || !dst || !sw || !sh || !dw || !dh || !pixelBytes(sf) || !pixelBytes(df) || sp<rowBytes(sf,sw) || dp<rowBytes(df,dw)) return false;
    if(filter==D3DX_DEFAULT) filter=D3DX_FILTER_TRIANGLE;
    if(filter&~0x6f000fu) stop("bf2d3dx::convert filter flags");
    const unsigned mode=filter&15;
    if(mode<1 || mode>5) return false;
    if(mode==5 && (dw!=std::max(1u,sw/2) || dh!=std::max(1u,sh/2)) && (sw!=dw || sh!=dh)) return false;
    const auto* a=static_cast<const unsigned char*>(src); auto* b=static_cast<unsigned char*>(dst);
    // Preserve compressed blocks and floating-point payloads for exact copies.
    if(sw==dw && sh==dh && sf==df && !key && !(filter&(D3DX_FILTER_SRGB_IN|D3DX_FILTER_SRGB_OUT))) {
        const unsigned rows=(sh+blockSize(sf)-1)/blockSize(sf),n=rowBytes(sf,sw);
        Bytes copy(std::size_t(rows)*n); for(unsigned y=0;y<rows;++y) std::memcpy(copy.data()+y*n,a+y*sp,n);
        for(unsigned y=0;y<rows;++y) std::memcpy(b+y*dp,copy.data()+y*n,n); return true;
    }
    if(std::size_t(dw)*dh>std::numeric_limits<std::size_t>::max()/sizeof(Color)) return false;
    std::vector<Color> colors(std::size_t(dw)*dh);
    auto read=[&](unsigned x,unsigned y) {
        Color c=readPixel(a,sf,sp,x,y);
        if(key) { unsigned char packed[4]; encode(D3DFMT_A8R8G8B8,packed,c); if(get(packed,4)==key) c={0,0,0,0}; }
        if(filter&D3DX_FILTER_SRGB_IN) for(unsigned i=0;i<3;++i) c[i]=std::pow(c[i],2.2); return c;
    };
    std::vector<std::vector<Weight>> wx(dw),wy(dh);
    if(mode!=1) {
        for(unsigned x=0;x<dw;++x) wx[x]=weights(x,sw,dw,mode,filter&D3DX_FILTER_MIRROR_U);
        for(unsigned y=0;y<dh;++y) wy[y]=weights(y,sh,dh,mode,filter&D3DX_FILTER_MIRROR_V);
    }
    for(unsigned y=0;y<dh;++y) for(unsigned x=0;x<dw;++x) {
        Color c{0,0,0,0};
        if(mode==1) { if(x<sw && y<sh) c=read(x,y); }
        else for(const auto yy:wy[y]) for(const auto xx:wx[x]) { auto p=read(xx.index,yy.index); for(unsigned k=0;k<4;++k) c[k]+=p[k]*xx.value*yy.value; }
        if(filter&D3DX_FILTER_SRGB_OUT) for(unsigned i=0;i<3;++i) c[i]=std::pow(c[i],1/2.2);
        colors[std::size_t(y)*dw+x]=c;
    }
    if(blockSize(df)==1) {
        for(unsigned y=0;y<dh;++y) for(unsigned x=0;x<dw;++x) encode(df,b+std::size_t(y)*dp+x*pixelBytes(df),colors[std::size_t(y)*dw+x]);
    } else for(unsigned y=0;y<dh;y+=4) for(unsigned x=0;x<dw;x+=4) {
        Color block[16]; for(unsigned j=0;j<4;++j) for(unsigned i=0;i<4;++i) block[j*4+i]=colors[std::size_t(std::min(y+j,dh-1))*dw+std::min(x+i,dw-1)];
        writeBlock(df,b+std::size_t(y/4)*dp+(x/4)*pixelBytes(df),block);
    }
    return true;
}
}
#endif
