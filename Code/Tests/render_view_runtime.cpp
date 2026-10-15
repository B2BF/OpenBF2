// [TEMP][CHOSEN, not recovered] Focused x86 fixture for the actual RenderView.
// No replacement vtable, byte-overlay class, /FORCE link or D3D device. Unrecovered
// derived matrices and clone are never called.
#include "../BF2/RendMisc/IRenderView.h"
#include "../BF2/Math/Frustum.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <limits>
#include <cfloat>

namespace dice { namespace hfe { namespace Rend {
IBase* createRenderViewDirect();
IBase* __fastcall createRenderView(unsigned int, IBase*);
}}}
using namespace dice::hfe;
using dx9rend::IRenderView;
static int checks, failures;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; std::printf("FAIL line %d: %s\n", __LINE__, #e); } } while (0)
static bool near(float a, float b, float eps=0.00002f) { return std::fabs(a-b) <= eps; }
static unsigned bits(const float& f) { unsigned r; std::memcpy(&r,&f,4); return r; }
static bool poison(const void* p, unsigned n) {
    const unsigned char* b=static_cast<const unsigned char*>(p);
    for (unsigned i=0;i<n;++i) if (b[i]!=0xA5) return false;
    return true;
}
static bool zero(const Vec3& v) { return v.x==0 && v.y==0 && v.z==0; }
static Mat4 identity() { Mat4 m={}; m.m[0]=m.m[5]=m.m[10]=m.m[15]=1; return m; }
static void frustumConstructor() {
    alignas(Frustum) unsigned char storage[sizeof(Frustum)];
    std::memset(storage,0xA5,sizeof(storage));
    Frustum* f=new(storage) Frustum;
    for(int i=0;i<6;++i) {
        CHECK(zero(f->m_planes[i].normal));
        CHECK(poison(&f->m_planes[i].d,sizeof(float)));
    }
    for(int i=0;i<8;++i) {
        CHECK(zero(f->m_corners[i]));
        CHECK(zero(f->m_prevCorners[i]));
    }
    CHECK(poison(f->m_planeOutcodes,sizeof(f->m_planeOutcodes)));
    CHECK(poison(&f->m_boundingRadius,4) && poison(&f->m_boundingRadiusSq,4));
    CHECK(poison(&f->m_fov,4) && poison(&f->m_aspect,4));
    CHECK(poison(&f->m_dword1D4,4) && poison(&f->m_dword1D8,4));
    CHECK(poison(&f->m_nearDist,4) && poison(&f->m_farDist,4));
    const Mat4 id=identity();
    CHECK(std::memcmp(&id,&f->m_transformation,sizeof(id))==0);
    CHECK(zero(f->m_center) && f->m_type==2 && f->m_cornersDirty);
    CHECK(zero(f->m_boundingBox.m_min) && zero(f->m_boundingBox.m_max));
    CHECK(f->m_boundingBox.m_radiusFromOrigin==0 && f->m_boundingBox.m_radiusFromCentre==0);
    CHECK(f->m_boundingBox.m_derivedDataDirty && zero(f->m_boundingBox.m_size) && zero(f->m_boundingBox.m_centre));
    f->~Frustum();
}
static void orthoPrecision() {
    unsigned original, ignored;
    _controlfp_s(&original, 0, 0);
    _controlfp_s(&ignored, _PC_64, _MCW_PC);
    Frustum frustum;
    Mat4 matrix=identity();
    const float term=1.0f+std::ldexp(1.0f,-23);
    matrix.m[10]=term;matrix.m[14]=-(1.0f+std::ldexp(1.0f,-22));
    frustum.setOrtho(2,2,term,term,matrix);
    CHECK(frustum.m_center.z==std::ldexp(1.0f,-46));
    matrix=identity();
    frustum.setOrtho(2,2,FLT_MAX,FLT_MAX,matrix);
    CHECK(frustum.m_center.z==FLT_MAX);
    _controlfp_s(&ignored, original, _MCW_PC);
}
static void viewRuntime() {
    IRenderView* v=static_cast<IRenderView*>(Rend::createRenderViewDirect());
    CHECK(v->getRef()==1);
    CHECK(v->queryInterface(1)==v && v->queryInterface(820201)==v);
    CHECK(v->queryInterface(820301)==0 && v->getRef()==1);
    v->addRef(); CHECK(v->getRef()==2); CHECK(v->release()==1);
    CHECK(v->getProjectionMode()==0);
    CHECK(near(v->getFov(),1.0471976f));
    CHECK(v->getZoom()==1 && v->getInverseZoom()==1 && v->getReferenceFov()==0);
    CHECK(v->getNearPlane()==0.01f && v->getFarPlane()==1000 && v->getAspect()==0.75f);
    CHECK(v->getOrthoWidth()==1 && v->getOrthoHeight()==1);
    CHECK(!v->getFlag42E() && !v->getFlag42F());
    v->setFlag42E(true); CHECK(v->getFlag42E() && !v->getFlag42F());
    v->setFlag42F(true); CHECK(v->getFlag42E() && v->getFlag42F());
    v->setFlag42E(false); v->setFlag42F(false);
    CHECK(!v->getFlag42E() && !v->getFlag42F());
    float* viewport=v->getViewport();
    CHECK(viewport[0]==0 && viewport[1]==0 && viewport[2]==0 && viewport[3]==0);
    v->setViewport(-11,13,1280,720);
    CHECK(v->getViewport()==viewport && viewport[0]==-11 && viewport[1]==13 && viewport[2]==1280 && viewport[3]==720);
    Mat4 id=identity();
    CHECK(std::memcmp(v->getViewMatrixPtr(),&id,sizeof(id))==0);
    v->setFov(1.0f);
    CHECK(v->getReferenceFov()==1 && v->getZoom()==1 && v->getInverseZoom()==1);
    v->setFov(0.5f);
    CHECK(near(v->getZoom(),2.13949394f) && near(v->getInverseZoom(),0.46740025f));
    IRenderView* second=static_cast<IRenderView*>(Rend::createRenderViewDirect());
    second->setFov(0.5f);
    CHECK(second->getReferenceFov()==1 && second->getZoom()==v->getZoom());
    CHECK(second->release()==0);

    // Consumer-visible cache behavior distinguishes the unconditional near/far/
    // aspect/FOV setters from viewport and equality-guarded mode/ortho setters.
    v->setFov(1); v->setNearPlane(2); v->setFarPlane(8); v->setAspect(0.75f);
    Mat4* p=v->getProjectionMatrixPtr();
    CHECK(near(p->m[0],1.3728658f) && near(p->m[5],1.8304877f));
    CHECK(p->m[10]==1.25f && p->m[11]==1 && p->m[14]==-2.5f && p->m[15]==0);
    // Homogeneous depth is 0 at near=2 and 1 at near+far=10.
    CHECK((2*p->m[10]+p->m[14])/2==0 && (10*p->m[10]+p->m[14])/10==1);
    p->m[0]=123;
    v->setViewport(1,2,3,4); CHECK(v->getProjectionMatrixPtr()->m[0]==123);
    v->setProjectionMode(0); CHECK(v->getProjectionMatrixPtr()->m[0]==123);
    v->setOrthoWidth(1); v->setOrthoHeight(1); CHECK(v->getProjectionMatrixPtr()->m[0]==123);
    v->setViewMatrix(id.m); CHECK(v->getProjectionMatrixPtr()->m[0]==123);
    v->setNearPlane(2); CHECK(near(v->getProjectionMatrixPtr()->m[0],1.3728658f));
    p->m[0]=123; v->setFarPlane(8); CHECK(p==v->getProjectionMatrixPtr() && p->m[0]!=123);
    p->m[0]=123; v->setAspect(0.75f); CHECK(v->getProjectionMatrixPtr()->m[0]!=123);
    p->m[0]=123; v->setFov(1); CHECK(v->getProjectionMatrixPtr()->m[0]!=123);
    p->m[0]=123; v->setOrthoWidth(20); CHECK(v->getProjectionMatrixPtr()->m[0]!=123);
    p->m[0]=123; v->setOrthoHeight(10); CHECK(v->getProjectionMatrixPtr()->m[0]!=123);
    v->setProjectionMode(1);
    CHECK(v->getProjectionMode()==1 && v->getOrthoWidth()==20 && v->getOrthoHeight()==10);
    p=v->getProjectionMatrixPtr();
    CHECK(near(p->m[0],0.1f) && near(p->m[5],0.2f) && p->m[10]==0.125f && p->m[14]==-0.25f && p->m[15]==1);
    Frustum* ortho=v->getFrustumPtr();
    CHECK(ortho->m_type==1 && ortho->m_dword1D4==20 && ortho->m_dword1D8==10);
    CHECK(ortho->m_center.z==6 && ortho->m_nearDist==2 && ortho->m_farDist==10);
    CHECK(ortho->m_corners[0].x==-10 && ortho->m_corners[0].y==5 && ortho->m_corners[0].z==2);
    CHECK(ortho->m_corners[7].x==10 && ortho->m_corners[7].y==-5 && ortho->m_corners[7].z==10);
    Frustum copied(*ortho); // actual value copy, matching client752D90
    copied.setFarDist(18);
    CHECK(copied.m_farDist==18 && copied.m_center.z==10 && copied.m_corners[7].z==18);
    CHECK(ortho->m_farDist==10 && ortho->m_corners[7].z==10);
    copied.m_type=2; copied.m_center.x=123; copied.setFarDist(24);
    CHECK(copied.m_farDist==24 && copied.m_center.x==123 && copied.m_type==2);
    v->setProjectionMode(0);
    const float nan=std::numeric_limits<float>::quiet_NaN();
    v->setOrthoWidth(nan); p=v->getProjectionMatrixPtr(); p->m[0]=123;
    v->setOrthoWidth(nan); CHECK(std::isnan(v->getOrthoWidth()) && v->getProjectionMatrixPtr()->m[0]!=123);
    v->setOrthoHeight(nan); p->m[0]=123; v->getProjectionMatrixPtr(); p->m[0]=123;
    v->setOrthoHeight(nan); CHECK(std::isnan(v->getOrthoHeight()) && v->getProjectionMatrixPtr()->m[0]!=123);
    v->setOrthoWidth(20); v->setOrthoHeight(10);

    v->setAspect(0.5f); v->setAdjustedFov(1); CHECK(near(v->getFov(),0.8888889f));
    v->setAspect(0.5625f); v->setAdjustedFov(1); CHECK(near(v->getFov(),1));
    v->setAspect(0.6f); v->setAdjustedFov(1); CHECK(v->getFov()==1);
    v->setAspect(0.8f); v->setAdjustedFov(1); CHECK(near(v->getFov(),1.0666667f));
    v->setAspect(0.9f); v->setAdjustedFov(1); CHECK(near(v->getFov(),1.2f));
    v->setAspect(nan); v->setAdjustedFov(1); CHECK(v->getFov()==1 && std::isnan(v->getAspect()));

    // Real native Frustum is returned; the matrix includes a translated camera.
    v->setAspect(0.75f); v->setNearPlane(2); v->setFarPlane(8);
    Mat4 camera=id; camera.m[12]=10; camera.m[13]=20; camera.m[14]=30;
    v->setViewMatrix(camera.m);
    CHECK(std::memcmp(v->getViewMatrixPtr(),&camera,sizeof(camera))==0);
    Frustum* f=v->getFrustumPtr();
    CHECK(std::memcmp(&f->m_transformation,&camera,sizeof(camera))==0);
    CHECK(f->m_fov==1 && f->m_aspect==0.75f && f->m_nearDist==2 && f->m_farDist==10 && f->m_type==0);
    CHECK(f->m_center.x==10 && f->m_center.y==20 && f->m_center.z==36);
    CHECK(!f->m_cornersDirty && f->m_boundingRadius>0);
    Frustum perspectiveCopy(*f); perspectiveCopy.setFarDist(18);
    CHECK(perspectiveCopy.m_center.z==40 && near(perspectiveCopy.m_corners[7].z,48,0.0001f));
    CHECK(f->m_center.z==36 && f->m_farDist==10);
    for(int i=0;i<8;++i) CHECK(std::memcmp(&f->m_corners[i],&f->m_prevCorners[i],sizeof(Vec3))==0);
    for(int i=0;i<4;++i) CHECK(near(f->m_corners[i].z,32,0.0001f) && near(f->m_corners[i+4].z,40,0.0001f));
    for(int i=0;i<6;++i) {
        const Vec3& n=f->m_planes[i].normal;
        const unsigned expected=(bits(n.x)>>31)|((bits(n.y)>>30)&2)|((bits(n.z)>>29)&4);
        CHECK(static_cast<unsigned>(f->m_planeOutcodes[i])==expected);
    }
    f->m_center.x=123; CHECK(v->getFrustumPtr()==f && f->m_center.x==123);
    v->setViewport(0,0,800,600); CHECK(v->getFrustumPtr()->m_center.x==123);
    v->setViewMatrix(camera.m); CHECK(v->getFrustumPtr()->m_center.x==10);

    // Degenerate signed input still transports raw normal sign bits into masks.
    camera.m[0]=nan; camera.m[4]=nan; camera.m[8]=nan;
    v->setViewMatrix(camera.m); f=v->getFrustumPtr();
    for(int i=0;i<6;++i) {
        const Vec3& n=f->m_planes[i].normal;
        CHECK(std::isnan(n.x));
        CHECK((static_cast<unsigned>(f->m_planeOutcodes[i])&1)==(bits(n.x)>>31));
    }
    CHECK(Rend::createRenderView(1,v)==0); // factory refuses aggregation
    CHECK(Rend::createRenderView(0xDEADBEEF,0)==0); // real failed-query release
    CHECK(v->release()==0);
}
int main() {
    static_assert(sizeof(void*)==4,"x86 only");
    static_assert(sizeof(IRenderView)==4 && sizeof(Mat4)==64 && sizeof(Frustum)==488,"native shared geometry");
    frustumConstructor(); viewRuntime(); orthoPrecision();
    std::printf("RenderView: %d/%d checks passed (%s ABI)\n",checks-failures,checks,
        "native"
    );
    return failures?1:0;
}
