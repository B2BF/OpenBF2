// [TEMP] [CHOSEN, not recovered] Hand-computed API fixtures; no game content.
#include "../../BF2/Platform/Gfx/Metal/D3DXHelpers.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>
#include <algorithm>
#include <csignal>
#include <unistd.h>
#include <zlib.h>
using namespace bf2d3dx;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#include "menu.inc"
static bool near(double a,double b,double eps=1e-6) { return std::abs(a-b)<eps; }
static int math() {
    const float values[]={0,-0.f,1,-2,65504,65520,INFINITY,-INFINITY,std::ldexp(1.f,-24),std::ldexp(1.f,-25),1+std::ldexp(1.f,-11),1+3*std::ldexp(1.f,-11)};
    const WORD expected[]={0,0x8000,0x3c00,0xc000,0x7bff,0x7c00,0x7c00,0xfc00,1,0,0x3c00,0x3c02};
    D3DXFLOAT16 result[12]; CHECK(D3DXFloat32To16Array(result,values,12)==result);
    for(unsigned i=0;i<12;++i) CHECK(result[i].value==expected[i]);
    for(unsigned h=0;h<65536;++h) {
        if((h&0x7c00)==0x7c00 && (h&1023)) CHECK(std::isnan(unhalf(h)));
        else CHECK(half(unhalf(h))==h);
    }
    CHECK((half(NAN)&0x7fff)>0x7c00);
    D3DXMATRIX a={},b={},c={},inverse={};
    for(unsigned i=0;i<4;++i) a.m[i][i]=1;
    a.m[3][0]=3; a.m[3][1]=4; a.m[3][2]=5;
    CHECK(D3DXMatrixRotationZ(&b,1.5707963267948966f)==&b);
    CHECK(near(b.m[0][1],1) && near(b.m[1][0],-1));
    D3DXMatrixMultiply(&c,&a,&b); CHECK(near(c.m[3][0],-4) && near(c.m[3][1],3));
    D3DXMatrixMultiply(&a,&a,&b); CHECK(!std::memcmp(&a,&c,64));
    float det; CHECK(D3DXMatrixInverse(&inverse,&det,&a)==&inverse && near(det,1));
    D3DXMatrixMultiply(&c,&a,&inverse);
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<4;++j) CHECK(near(c.m[i][j],i==j?1:0));
    a={}; inverse.m[0][0]=123; CHECK(!D3DXMatrixInverse(&inverse,&det,&a) && det==0 && inverse.m[0][0]==123);
    D3DXMatrixOrthoLH(&a,4,8,2,10); CHECK(a.m[0][0]==.5 && a.m[1][1]==.25 && a.m[2][2]==.125 && a.m[3][2]==-.25);
    D3DXMatrixTranspose(&a,&a); CHECK(a.m[2][3]==-.25 && a.m[3][2]==0);
    D3DXVECTOR3 eye(1,2,3),at(1,2,4),up(0,1,0); D3DXMatrixLookAtLH(&a,&eye,&at,&up);
    CHECK(a.m[0][0]==1 && a.m[1][1]==1 && a.m[2][2]==1 && a.m[3][0]==-1 && a.m[3][1]==-2 && a.m[3][2]==-3);
    struct Position { D3DXVECTOR3 p; unsigned sentinel; } p[]={{{-1,-2,-3},42},{{1,2,3},43}};
    D3DXVECTOR3 lo,hi,center; float radius;
    CHECK(D3DXComputeBoundingBox(&p[0].p,2,sizeof(Position),&lo,&hi)==D3D_OK && lo.x==-1 && hi.z==3);
    CHECK(D3DXComputeBoundingSphere(&p[0].p,2,sizeof(Position),&center,&radius)==D3D_OK && center.x==0 && center.y==0 && center.z==0 && near(radius,std::sqrt(14.)));
    ID3DXBuffer* buffer=nullptr; CHECK(D3DXCreateBuffer(17,&buffer)==D3D_OK && buffer && buffer->GetBufferSize()==17);
    const GUID iidBuffer={0x8ba5fb08,0x5195,0x40e2,{0xac,0x58,0x0d,0x98,0x9c,0x3a,0x01,0x02}};
    void* queried=nullptr; CHECK(buffer->QueryInterface(iidBuffer,&queried)==D3D_OK && queried==buffer && buffer->Release()==1);
    std::memset(buffer->GetBufferPointer(),42,17); CHECK(buffer->AddRef()==2 && buffer->Release()==1 && buffer->Release()==0);
    const DWORD tokens[]={0xfffe0300,0xffff}; CHECK(D3DXGetShaderVersion(tokens)==0xfffe0300 && D3DXGetShaderVersion(nullptr)==0);
    ID3DXBuffer *shader=nullptr,*errors=nullptr; const char* vs="vs.1.1\ndcl_position0 v0\nmov oPos, v0";
    // Public D3DX9_25 assembly golden: POSITION0 declaration is present even in vs_1_1.
    const DWORD golden[]={0xfffe0101,0x1f,0x80000000,0x900f0000,1,0xc00f0000,0x90e40000,0xffff};
    CHECK(D3DXAssembleShader(vs,std::strlen(vs),nullptr,nullptr,0,&shader,&errors)==D3D_OK && !errors && shader->GetBufferSize()==sizeof golden);
    CHECK(!std::memcmp(shader->GetBufferPointer(),golden,sizeof golden)); shader->Release();
    const char* ps="ps.1.1\ndef c0, 1,1,1,0.5\nmov r0,c0";
    CHECK(D3DXAssembleShader(ps,std::strlen(ps),nullptr,nullptr,0,&shader,&errors)==D3D_OK);
    CHECK(static_cast<DWORD*>(shader->GetBufferPointer())[6]==0x3f000000); shader->Release();
    return 0;
}
static int pixels() {
    const D3DFORMAT formats[]={D3DFMT_R8G8B8,D3DFMT_A8R8G8B8,D3DFMT_X8R8G8B8,D3DFMT_A8B8G8R8,D3DFMT_X8B8G8R8,D3DFMT_R5G6B5,D3DFMT_X1R5G5B5,D3DFMT_A1R5G5B5,D3DFMT_A4R4G4B4,D3DFMT_X4R4G4B4,D3DFMT_A8,D3DFMT_L8,D3DFMT_A8L8,D3DFMT_L16,D3DFMT_G16R16,D3DFMT_A16B16G16R16,D3DFMT_A2B10G10R10,D3DFMT_A2R10G10B10,D3DFMT_R16F,D3DFMT_G16R16F,D3DFMT_A16B16G16R16F,D3DFMT_R32F,D3DFMT_G32R32F,D3DFMT_A32B32G32R32F};
    for(auto f:formats) {
        unsigned char bytes[16]={}; const Color color{1,1,1,1}; encode(f,bytes,color); auto c=decode(f,bytes);
        CHECK(c.r==1 && c.a==1); if(f!=D3DFMT_R16F && f!=D3DFMT_R32F) CHECK(c.g==1);
        if(f!=D3DFMT_R16F && f!=D3DFMT_R32F && f!=D3DFMT_G16R16F && f!=D3DFMT_G32R32F && f!=D3DFMT_G16R16) CHECK(c.b==1);
        for(auto dst:formats) { unsigned char output[16]={}; CHECK(convert(bytes,f,1,1,pixelBytes(f),output,dst,1,1,pixelBytes(dst),D3DX_FILTER_NONE)); auto d=decode(dst,output); CHECK(d.r==1); }
    }
    for(auto f:{D3DFMT_R5G6B5,D3DFMT_A1R5G5B5,D3DFMT_A4R4G4B4,D3DFMT_L16}) for(unsigned i=0;i<65536;++i) {
        WORD a=i,b=0; encode(f,reinterpret_cast<unsigned char*>(&b),decode(f,reinterpret_cast<const unsigned char*>(&a))); CHECK(a==b);
    }
    const unsigned char bgra[]={0x33,0x66,0xcc,0x99}; auto c=decode(D3DFMT_A8R8G8B8,bgra);
    CHECK(near(c.r,.8) && near(c.g,.4) && near(c.b,.2) && near(c.a,.6));
    struct Golden { D3DFORMAT f; Bytes bytes; };
    const Golden golden[]={
        {D3DFMT_A8R8G8B8,{0,128,255,64}}, {D3DFMT_A8B8G8R8,{255,128,0,64}},
        {D3DFMT_R5G6B5,{0,0xfc}}, {D3DFMT_A1R5G5B5,{0,0x7e}},
        {D3DFMT_A4R4G4B4,{0x80,0x4f}}, {D3DFMT_A2B10G10R10,{0xff,3,8,0x40}},
        {D3DFMT_G16R16,{255,255,0,128}}, {D3DFMT_A8L8,{255,64}},
        {D3DFMT_A16B16G16R16F,{0,0x3c,0,0x38,0,0,0,0x34}}};
    for(const auto& g:golden) { Bytes bytes(g.bytes.size()); encode(g.f,bytes.data(),{1,.5,0,.25}); CHECK(bytes==g.bytes); }
    unsigned char out[64]={}; CHECK(convert(bgra,D3DFMT_A8R8G8B8,1,1,4,out,D3DFMT_A8R8G8B8,2,2,8,D3DX_FILTER_NONE));
    CHECK(!std::memcmp(out,bgra,4)); for(unsigned i=4;i<16;++i) CHECK(out[i]==0);
    const unsigned char gray[]={0,64,128,255};
    CHECK(convert(gray,D3DFMT_L8,2,2,2,out,D3DFMT_L8,1,1,1,D3DX_FILTER_BOX) && out[0]==112);
    CHECK(convert(gray,D3DFMT_L8,2,2,2,out,D3DFMT_L8,1,1,1,D3DX_FILTER_LINEAR) && out[0]==112);
    CHECK(convert(gray,D3DFMT_L8,2,2,2,out,D3DFMT_L8,1,1,1,D3DX_FILTER_TRIANGLE) && out[0]==112);
    CHECK(convert(gray,D3DFMT_L8,2,2,2,out,D3DFMT_L8,1,1,1,D3DX_FILTER_POINT) && out[0]==255);
    CHECK(convert(gray,D3DFMT_L8,2,2,2,out,D3DFMT_L8,4,4,4,D3DX_FILTER_POINT) && out[0]==0 && out[2]==64 && out[8]==128 && out[15]==255);
    const unsigned char line[]={0,100}; CHECK(convert(line,D3DFMT_L8,2,1,2,out,D3DFMT_L8,4,1,4,D3DX_FILTER_LINEAR));
    CHECK(out[0]==25 && out[1]==25 && out[2]==75 && out[3]==75);
    CHECK(convert(line,D3DFMT_L8,2,1,2,out,D3DFMT_L8,4,1,4,D3DX_FILTER_LINEAR|D3DX_FILTER_MIRROR_U));
    CHECK(out[0]==0 && out[1]==25 && out[2]==75 && out[3]==100);
    const unsigned char impulses[]={0,0,0,255};
    CHECK(convert(impulses,D3DFMT_L8,4,1,4,out,D3DFMT_L8,1,1,1,D3DX_FILTER_TRIANGLE) && out[0]==64);
    CHECK(convert(impulses,D3DFMT_L8,4,1,4,out,D3DFMT_L8,1,1,1,D3DX_FILTER_LINEAR) && out[0]==0);
    CHECK(convert(impulses,D3DFMT_L8,4,1,4,out,D3DFMT_L8,2,1,2,D3DX_FILTER_BOX) && out[0]==0 && out[1]==128);
    CHECK(convert(bgra,D3DFMT_A8R8G8B8,1,1,4,out,D3DFMT_A8R8G8B8,1,1,4,D3DX_FILTER_NONE,0x99cc6633));
    CHECK(out[0]==0 && out[1]==0 && out[2]==0 && out[3]==0);
    const unsigned char bc1[]={0,0xf8,0xe0,7,0xe4,0xe4,0xe4,0xe4};
    CHECK(convert(bc1,D3DFMT_DXT1,4,4,8,out,D3DFMT_A8R8G8B8,4,4,16,D3DX_FILTER_NONE));
    CHECK(out[2]==255 && out[1]==0 && out[5]==255 && out[6]==0 && out[10]==170 && out[9]==85 && out[14]==85 && out[13]==170);
    unsigned char bc3[16]={255,0,0,0,0,0,0,0,0,0xf8,0xe0,7,0,0,0,0};
    CHECK(convert(bc3,D3DFMT_DXT5,4,4,16,out,D3DFMT_A8R8G8B8,4,4,16,D3DX_FILTER_NONE) && out[3]==255 && out[2]==255);
    bc3[0]=0; CHECK(convert(bc3,D3DFMT_DXT3,4,4,16,out,D3DFMT_A8R8G8B8,4,4,16,D3DX_FILTER_NONE) && out[3]==0 && out[7]==0);
    for(auto f:{D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5}) { unsigned char packed[16]={},red[64]; for(unsigned j=0;j<16;++j) { red[j*4]=red[j*4+1]=0; red[j*4+2]=red[j*4+3]=255; }
        CHECK(convert(red,D3DFMT_A8R8G8B8,4,4,16,packed,f,4,4,pixelBytes(f),D3DX_FILTER_NONE));
        CHECK(convert(packed,f,4,4,pixelBytes(f),out,D3DFMT_A8R8G8B8,4,4,16,D3DX_FILTER_NONE)); CHECK(!std::memcmp(out,red,64)); }
    return 0;
}
static void put(Bytes& b,unsigned offset,unsigned n,unsigned size=4) { for(unsigned j=0;j<size;++j) b[offset+j]=n>>(j*8); }
static Bytes dds(unsigned w,unsigned h,unsigned mips,unsigned caps=0,unsigned depth=0) {
    Bytes b(128,0); std::memcpy(b.data(),"DDS ",4); put(b,4,124); put(b,8,0x1007); put(b,12,h); put(b,16,w); put(b,24,depth); put(b,28,mips);
    put(b,76,32); put(b,80,4); put(b,84,D3DFMT_DXT1); put(b,108,0x1000); put(b,112,caps); return b;
}
static int images() {
    auto b=dds(4,4,3); b.resize(152,42); Image im;
    CHECK(parse(b.data(),b.size(),im) && im.info.MipLevels==3 && im.data.size()==3 && im.data[2].size()==8);
    CHECK(!parse(b.data(),b.size()-1,im));
    auto cube=dds(4,4,1,0xfe00); cube.resize(176,7); CHECK(parse(cube.data(),cube.size(),im) && im.info.ResourceType==D3DRTYPE_CUBETEXTURE && im.data.size()==6);
    auto volume=dds(4,4,2,0x200000,4); volume.resize(176,8); CHECK(parse(volume.data(),volume.size(),im) && im.info.Depth==4 && im.data[0].size()==32 && im.data[1].size()==16);
    auto padded=dds(1,2,1); put(padded,8,0x100f); put(padded,20,4); put(padded,80,0x40); put(padded,84,0); put(padded,88,24); put(padded,92,0xff0000); put(padded,96,0xff00); put(padded,100,0xff);
    padded.insert(padded.end(),{1,2,3,99,4,5,6,99}); CHECK(parse(padded.data(),padded.size(),im) && im.data[0]==Bytes({1,2,3,4,5,6}));
    auto indexed=dds(2,1,1); put(indexed,80,0x20); put(indexed,84,0); put(indexed,88,8); indexed.resize(1154,0);
    indexed[128]=10; indexed[129]=20; indexed[130]=30; indexed[131]=40;
    indexed[132]=100; indexed[133]=110; indexed[134]=120; indexed[135]=130; indexed[1153]=1;
    CHECK(parse(indexed.data(),indexed.size(),im) && im.info.Format==D3DFMT_A8R8G8B8 && im.data[0]==Bytes({30,20,10,40,120,110,100,130}));
    D3DXIMAGE_INFO palInfo; CHECK(metadata(indexed.data(),124,palInfo) && palInfo.Format==D3DFMT_P8);
    // The engine owns only 124 probe bytes while reporting the full file size.
    Bytes header(b.begin(),b.begin()+124); D3DXIMAGE_INFO info; CHECK(D3DXGetImageInfoFromFileInMemory(header.data(),UINT(b.size()),&info)==D3D_OK && info.Width==4);
    Bytes tga(18,0); tga[2]=2; put(tga,12,2,2); put(tga,14,2,2); tga[16]=24;
    tga.insert(tga.end(),{0,0,255,0,255,0,255,0,0,255,255,255});
    CHECK(parse(tga.data(),tga.size(),im) && im.info.Format==D3DFMT_R8G8B8 && im.data[0][0]==255 && im.data[0][2]==0 && im.data[0][8]==255);
    Bytes saved; CHECK(serialize(im,D3DXIFF_TGA,saved)); Image back; CHECK(parse(saved.data(),saved.size(),back) && back.info.Width==2 && back.data[0][0]==255 && back.data[0][3]==255);
    tga.resize(18); tga[2]=10; tga[17]=0x30; tga.insert(tga.end(),{0x83,3,2,1}); CHECK(parse(tga.data(),tga.size(),im) && im.data[0][0]==3 && im.data[0][11]==1);
    tga[18]=0x84; CHECK(!parse(tga.data(),tga.size(),im));
    CHECK(parse(cube.data(),cube.size(),im) && serialize(im,D3DXIFF_DDS,saved) && parse(saved.data(),saved.size(),back)); CHECK(back.data==im.data && back.info.ResourceType==D3DRTYPE_CUBETEXTURE);
    CHECK(parse(volume.data(),volume.size(),im) && serialize(im,D3DXIFF_DDS,saved) && parse(saved.data(),saved.size(),back) && back.data==im.data);
    Bytes bmp(62,0); bmp[0]='B'; bmp[1]='M'; put(bmp,2,62); put(bmp,10,54); put(bmp,14,40); put(bmp,18,2); put(bmp,22,1); put(bmp,26,1,2); put(bmp,28,24,2);
    bmp[56]=255; bmp[57]=255; CHECK(parse(bmp.data(),bmp.size(),im) && im.data[0][2]==255 && im.data[0][4]==255 && im.data[0][3]==255);
    // PNG RGBA red + transparent green; scanline uses Sub prediction.
    Bytes png={137,80,78,71,13,10,26,10};
    auto chunk=[&](const char* type,const Bytes& data) { unsigned n=data.size(); for(int j=3;j>=0;--j) png.push_back(n>>(8*j)); auto start=png.size(); png.insert(png.end(),type,type+4); png.insert(png.end(),data.begin(),data.end()); unsigned crc=crc32(0,png.data()+start,n+4); for(int j=3;j>=0;--j) png.push_back(crc>>(8*j)); };
    chunk("IHDR",{0,0,0,2,0,0,0,1,8,6,0,0,0});
    const unsigned char scan[]={1,255,0,0,255,1,255,0,1}; Bytes compressed(64); uLongf n=compressed.size(); CHECK(compress(compressed.data(),&n,scan,sizeof(scan))==Z_OK); compressed.resize(n);
    chunk("IDAT",compressed); chunk("IEND",{});
    CHECK(parse(png.data(),png.size(),im) && im.data[0]==Bytes({0,0,255,255,0,255,0,0})); png[40]^=1; CHECK(!parse(png.data(),png.size(),im));
    return 0;
}
static int geometry() {
    for(unsigned kind=0;kind<3;++kind) {
        Geometry g;
        CHECK(kind==0?sphereGeometry(1,4,4,g):kind==1?boxGeometry(2,4,6,g):cylinderGeometry(1,1,2,4,2,g));
        CHECK(g.vertices.size()==(kind==0?14u:kind==1?24u:22u) && g.indices.size()==(kind==1?36u:72u));
        CHECK(g.indices[0]==0 && g.indices[1]==(kind==0?2u:1u) && g.indices[2]==(kind==0?1u:2u));
        D3DXVECTOR3 lo,hi,center; float radius;
        CHECK(D3DXComputeBoundingBox(&g.vertices[0].position,DWORD(g.vertices.size()),sizeof(MeshVertex),&lo,&hi)==D3D_OK);
        CHECK(D3DXComputeBoundingSphere(&g.vertices[0].position,DWORD(g.vertices.size()),sizeof(MeshVertex),&center,&radius)==D3D_OK);
        CHECK(near(lo.x,-1) && near(hi.x,1) && near(lo.y,kind==1?-2:-1) && near(hi.z,kind==1?3:1));
        CHECK(near(center.x,0) && near(center.y,0) && near(center.z,0) && near(radius,kind==0?1:kind==1?std::sqrt(14.):std::sqrt(2.)));
        for(const auto& v:g.vertices) CHECK(near(v.normal.x*v.normal.x+v.normal.y*v.normal.y+v.normal.z*v.normal.z,1));
        for(unsigned f=0;f<g.indices.size();f+=3) {
            for(unsigned e=0;e<3;++e) CHECK(g.indices[f+e]<g.vertices.size());
            auto a=g.vertices[g.indices[f]].position,b=g.vertices[g.indices[f+1]].position,c=g.vertices[g.indices[f+2]].position;
            double x=b.x-a.x,y=b.y-a.y,z=b.z-a.z,u=c.x-a.x,v=c.y-a.y,w=c.z-a.z;
            const double nx=y*w-z*v,ny=z*u-x*w,nz=x*v-y*u;
            // Every triangle has outward cross-product winding and nonzero area.
            CHECK(nx*(a.x+b.x+c.x)+ny*(a.y+b.y+c.y)+nz*(a.z+b.z+c.z)>0);
        }
        if(kind==0) { CHECK(near(g.vertices[1].position.x,0) && near(g.vertices[1].position.y,std::sqrt(.5)) && near(g.vertices[2].position.x,std::sqrt(.5))); CHECK(g.indices.back()==13); }
        if(kind==1) { CHECK(g.vertices[0].position.x==-1 && g.vertices[0].position.y==-2 && g.vertices[0].position.z==-3 && g.vertices[0].normal.x==-1); CHECK(g.vertices[16].position.z==3 && g.vertices[16].normal.z==1); }
        if(kind==2) { CHECK(g.vertices[0].position.z==-1 && g.vertices[0].normal.z==-1 && g.vertices.back().normal.z==1); CHECK(near(g.vertices[1].position.x,0) && near(g.vertices[1].position.y,1)); }
    }
    Geometry g;
    CHECK(sphereGeometry(1,20,20,g) && g.vertices.size()==382 && g.indices.size()==2280);
    CHECK(cylinderGeometry(0,1,1,20,20,g) && g.vertices.size()==462 && g.indices.size()==2520);
    CHECK(near(g.vertices[21].normal.z,-1/std::sqrt(2.)));
    CHECK(sphereGeometry(0,2,2,g)); for(const auto& v:g.vertices) CHECK(v.position.x==0 && v.position.y==0 && v.position.z==0);
    CHECK(!sphereGeometry(-1,4,4,g) && !sphereGeometry(1,1,4,g) && !cylinderGeometry(1,1,1,4,0,g) && !boxGeometry(-1,1,1,g));
    return 0;
}
int main(int argc,char** argv) {
    if(argc==2 && !std::strcmp(argv[1],"--corpus")) {
        unsigned n; unsigned count=0,failed=0;
        while(std::fread(&n,4,1,stdin)==1) {
            Bytes bytes(n); CHECK(std::fread(bytes.data(),1,n,stdin)==n); Image im;
            if(!parse(bytes.data(),bytes.size(),im)) { std::printf("FAIL image %u size %u\n",count,n); ++failed; }
            ++count;
        }
        std::printf("%s: %u/%u corpus images parsed\n",failed?"FAIL":"PASS",count-failed,count); return failed?1:0;
    }
    if(argc==2 && !std::strcmp(argv[1],"--assembler-frontier")) {
        std::signal(SIGABRT,[](int){_exit(93);}); ID3DXBuffer* b; D3DXAssembleShader("vs.3.0",6,nullptr,nullptr,0,&b,nullptr); return 1;
    }
    if(argc==2 && !std::strcmp(argv[1],"--writer-frontier")) {
        std::signal(SIGABRT,[](int){_exit(93);}); Image im; im.info={1,1,1,1,D3DFMT_A8R8G8B8,D3DRTYPE_TEXTURE,D3DXIFF_PNG}; im.data.push_back(Bytes(4)); Bytes out; serialize(im,D3DXIFF_PNG,out); return 1;
    }
    CHECK(math()==0); CHECK(pixels()==0); CHECK(images()==0); CHECK(geometry()==0); CHECK(menuCPUChecks()==0);
    std::puts("PASS: D3DX math, half floats, formats/BC, filters, DDS/TGA/BMP/PNG, mesh geometry and AA assembly"); return 0;
}
