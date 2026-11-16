// [CHOSEN, not recovered] Shape topology and normals, independent of GPU storage.
#if (defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))) && defined(BF2_IMAGE_RENDDX9)
#include "D3DXHelpers.h"
#include <cmath>
#include <utility>
namespace bf2d3dx {
static constexpr float pi=3.14159265358979323846f;
bool sphereGeometry(float radius,unsigned slices,unsigned stacks,Geometry& out) {
    if(!(radius>=0) || !std::isfinite(radius) || slices<2 || stacks<2 || std::uint64_t(slices)*stacks>10000000) return false;
    Geometry g; auto& v=g.vertices; auto& ix=g.indices;
    v.push_back({{0,0,radius},{0,0,1}});
    for(unsigned k=1;k<stacks;++k) { const float t=pi*k/stacks;
        for(unsigned j=0;j<slices;++j) { const float a=pi/2-2*pi*j/slices; D3DXVECTOR3 n(std::sin(t)*std::cos(a),std::sin(t)*std::sin(a),std::cos(t)); v.push_back({{radius*n.x,radius*n.y,radius*n.z},n}); } }
    v.push_back({{0,0,-radius},{0,0,-1}});
    for(unsigned j=0;j<slices;++j) ix.insert(ix.end(),{0,1+(j+1)%slices,1+j});
    for(unsigned k=0;k+2<stacks;++k) for(unsigned j=0;j<slices;++j) {
        DWORD a=1+k*slices+j,b=1+k*slices+(j+1)%slices,c=a+slices,e=b+slices; ix.insert(ix.end(),{a,b,c,b,e,c}); }
    const DWORD last=DWORD(v.size()-1),base=1+(stacks-2)*slices;
    for(unsigned j=0;j<slices;++j) ix.insert(ix.end(),{base+j,base+(j+1)%slices,last});
    out=std::move(g); return true;
}
bool boxGeometry(float w,float h,float depth,Geometry& out) {
    if(!(w>=0) || !(h>=0) || !(depth>=0) || !std::isfinite(w+h+depth)) return false;
    const D3DXVECTOR3 normals[]={{-1,0,0},{0,1,0},{1,0,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const D3DXVECTOR3 u[]={{0,0,1},{0,0,1},{0,0,1},{0,0,-1},{1,0,0},{0,1,0}};
    const D3DXVECTOR3 b[]={{0,1,0},{1,0,0},{0,-1,0},{1,0,0},{0,1,0},{1,0,0}};
    Geometry g; auto& v=g.vertices; auto& ix=g.indices;
    for(unsigned f=0;f<6;++f) {
        for(unsigned j=0;j<4;++j) { float s=j==0 || j==3?-1:1,t=j<2?-1:1; auto n=normals[f]; v.push_back({{(n.x+s*u[f].x+t*b[f].x)*w/2,(n.y+s*u[f].y+t*b[f].y)*h/2,(n.z+s*u[f].z+t*b[f].z)*depth/2},n}); }
        DWORD a=f*4; ix.insert(ix.end(),{a,a+1,a+2,a+2,a+3,a});
    }
    out=std::move(g); return true;
}
bool cylinderGeometry(float r1,float r2,float length,unsigned slices,unsigned stacks,Geometry& out) {
    if(!(r1>=0) || !(r2>=0) || !(length>=0) || !std::isfinite(r1+r2+length) || slices<2 || !stacks || std::uint64_t(slices)*(std::uint64_t(stacks)+3)>10000000) return false;
    Geometry g; auto& v=g.vertices; auto& ix=g.indices;
    auto ring=[&](float r,float z,bool cap,float nz) { for(unsigned j=0;j<slices;++j) { float a=pi/2-2*pi*j/slices,x=std::cos(a),y=std::sin(a);
        float nx=cap?0:x,ny=cap?0:y,n=std::sqrt(nx*nx+ny*ny+nz*nz); v.push_back({{r*x,r*y,z},{n?nx/n:0,n?ny/n:0,n?nz/n:0}}); } };
    v.push_back({{0,0,-length/2},{0,0,-1}}); ring(r1,-length/2,true,-1);
    for(unsigned j=0;j<slices;++j) ix.insert(ix.end(),{0,1+j,1+(j+1)%slices});
    float nz=length?(r1-r2)/length:0;
    for(unsigned k=0;k<=stacks;++k) ring(k==stacks?r2:r1+(r2-r1)*k/stacks,k==stacks?length/2:-length/2+length*k/stacks,false,nz);
    for(unsigned k=0;k<stacks;++k) for(unsigned j=0;j<slices;++j) { DWORD a=1+slices+k*slices+j,b=1+slices+k*slices+(j+1)%slices,c=a+slices,e=b+slices; ix.insert(ix.end(),{a,c,b,b,c,e}); }
    const DWORD cap=DWORD(v.size()),center=cap+slices; ring(r2,length/2,true,1); v.push_back({{0,0,length/2},{0,0,1}});
    for(unsigned j=0;j<slices;++j) ix.insert(ix.end(),{cap+j,center,cap+(j+1)%slices});
    out=std::move(g); return true;
}
}
#endif
