// [TEMP] Focused renderer fixture. Synthetic interfaces model only draw's inputs;
// production BundledMeshDraw.cpp is compiled directly, without /FORCE or a game.
#include "../BF2/Geom/BundledMeshDraw.cpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <utility>

using namespace dice::hfe;
using namespace dice::hfe::geom;
namespace {
int checks=0;
#define CHECK(c) do { ++checks;if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);} }while(0)
bool closeFloat(float a,float b){return std::fabs(a-b)<0.00001f;}
struct Fake
{
    void** vptr;unsigned char bytes[0x900];void* table[256];
    Fake(){std::memset(this,0,sizeof(*this));vptr=table;}
    template<class T> void bind(unsigned int at,T fn){table[at/4]=reinterpret_cast<void*>(fn);}
};
Fake base,renderer,lm,pm,manager,view,video,templStorage;
Fake staticLight,dynamicLight,staticLightTemplate,dynamicLightTemplate;
Fake testPlayer,testVehicle,testCamera,testArmor,testSystem;
unsigned char stats[0x250]={},timerData[0x30]={};
Mat4 identity={{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}};
Mat4 camera=identity,projection=identity,rootTransform=identity,zero={};
Vec3 sky={0.1f,0.2f,0.3f},sunColor={0.8f,0.6f,0.4f},sunSpec={0.2f,0.4f,0.6f};
Vec2 fadeRange={2,6};
unsigned int frame=10,mask=1,animationValue=0,gatherCount=0,requiredMask=0;
int heights=0,lods=0,playerQueries=0;bool enabled=true,expansion=false,traceHit=false;
float sampleValue=0.25f,ioValue=1.0f;
void* traceObject=0;
bool gatherPointLights=false;
bool usePlayer=false,useSky=false,useShadow=false;
Vec3 pointPosition={7,8,9},pointColor={0.8f,0.4f,0.2f};
Vec3 cockpitColor={0.2f,0.4f,0.6f},damageColor={1,0,0},cockpitAmbient={0.1f,0.2f,0.3f},cockpitDirection={0,-1,0};
std::vector<unsigned int> shaders;
struct Route{unsigned int index;dx9rend::RenderQueueEntry* entry;};
std::vector<Route> routes;
std::string comEvents;
struct CountedTexture : IUnknown
{
    ULONG refs=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**)override{CHECK(false);return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{comEvents+='A';return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{comEvents+='R';return --refs;}
};
dx9rend::RenderQueueEntry entries[64];dx9rend::Ra::RaLightRecord records[64];unsigned int nextEntry;
bool __fastcall isEnabled(void*,void*){return enabled;}
unsigned int __fastcall currentFrame(void*,void*){return frame;}
void* __fastcall getView(void*,void*){return &view;}
void* __fastcall getStats(void*,void*){return stats;}
void* __fastcall getVideo(void*,void*){return &video;}
const Mat4& __fastcall getCamera(void*,void*){return camera;}
const Mat4& __fastcall getProjection(void*,void*){return projection;}
bool __fastcall isExpansion(void*,void*){return expansion;}
world::IPlayer* __fastcall noPlayer(void*,void*){++playerQueries;return usePlayer?reinterpret_cast<world::IPlayer*>(&testPlayer):0;}
world::IObject* __fastcall vehicle(void*,void*){return reinterpret_cast<world::IObject*>(&testVehicle);}
world::ICameraObject* __fastcall gate(void*,void*){return reinterpret_cast<world::ICameraObject*>(&testCamera);}
bool __fastcall disabled(void*,void*){return false;}
float __fastcall frequency(void*,void*){return 2.0f;}
float __fastcall threshold(void*,void*){return 0.75f;}
float __fastcall health(void*,void*){return 25.0f;}
float __fastcall maximumHealth(void*,void*){return 100.0f;}
double __fastcall exactTime(void*,void*){return 2.25;}
const Vec3& __fastcall cockpitRgb(void*,void*){return cockpitColor;}
const Vec3& __fastcall damageRgb(void*,void*){return damageColor;}
const Vec3& __fastcall ambientRgb(void*,void*){return cockpitAmbient;}
const Vec3& __fastcall cockpitDir(void*,void*){return cockpitDirection;}
IBase* __fastcall armor(void*,void*,unsigned int cid,unsigned int iid)
{CHECK(cid==50345 && iid==50345);return reinterpret_cast<IBase*>(&testArmor);}
unsigned int __fastcall capabilities(void*,void*,int which){CHECK(which==4);return 0xFFFFFFFFu;}
float __fastcall sample(void*,void*,float,float){return sampleValue;}
float __fastcall sampleIO(void*,void*,float,float){return ioValue;}
const Vec2& __fastcall shadowRange(void*,void*){return fadeRange;}
float __fastcall one(void*,void*){return 1.0f;}
int __fastcall maxLights(void*,void*){return 4;}
void __fastcall gather(void*,void*,void*,std::vector<world::LightSource*>&,unsigned int maximum,
                       unsigned int required,unsigned int forbidden,bool occlusion);
void __fastcall gather(void*,void*,void*,std::vector<world::LightSource*>& out,unsigned int maximum,
                       unsigned int required,unsigned int forbidden,bool occlusion)
{
    ++gatherCount;requiredMask=required;CHECK(maximum==4 && forbidden==0 && occlusion);
    if(gatherPointLights)
    {
        out.push_back(reinterpret_cast<world::LightSource*>(&staticLight));
        out.push_back(reinterpret_cast<world::LightSource*>(&dynamicLight));
    }
}
float __fastcall pointRange(void*,void*){return 2.0f;}
float __fastcall rangeScale(void*,void*){return 3.0f;}
const Vec3& __fastcall pointPos(void*,void*){return pointPosition;}
const Vec3& __fastcall pointRgb(void*,void*){return pointColor;}
bool __fastcall dynamicTemplate(void* self,void*){return self==&dynamicLightTemplate;}
const Vec3& __fastcall skyColor(void*,void*){return sky;}
Vec3* __fastcall getSunColor(void*,void*,Vec3* out){*out=sunColor;return out;}
Vec3* __fastcall getSunSpec(void*,void*,Vec3* out){*out=sunSpec;return out;}
world::LightSource* __fastcall noSkyLight(void*,void*){return useSky?reinterpret_cast<world::LightSource*>(&staticLight):0;}
int __fastcall materialInteger(void*,void*,int,int,unsigned char){return 64;}
float __fastcall materialGloss(void*,void*,int,int,unsigned char){return 0.15f;}
float __fastcall materialEnv(void*,void*,int,int,int){return 0.25f;}
void initialize()
{
    manager.bind(0x14,isEnabled);base.bind(0x54,currentFrame);base.bind(0x48,getView);
    base.bind(0x50,getView);base.bind(0xD4,getView);base.bind(0x6C,getStats);base.bind(0x74,getVideo);
    base.bind(0xAC,isExpansion);
    renderer.bind(0xAC,isExpansion);renderer.bind(0x48,getView);
    field<void*>(&renderer,0x5DC)=timerData;field<double>(timerData,0x20)=10.0;
    field<float>(&renderer,0x59C)=0.5f;field<double>(&renderer,0x5E8)=0.5;
    view.bind(0x64,getCamera);view.bind(0x84,getProjection);video.bind(4,capabilities);
    pm.bind(0x30,noPlayer);lm.bind(0x2C4,sample);lm.bind(0x2C8,sampleIO);
    lm.bind(0x2C0,shadowRange);lm.bind(0x2A8,one);lm.bind(0x60,maxLights);lm.bind(0x20,gather);
    lm.bind(0x30,skyColor);lm.bind(0x1AC,getSunColor);lm.bind(0x1B4,getSunSpec);lm.bind(0x28,noSkyLight);
    lm.bind(0x2E0,rangeScale);
    staticLight.bind(0x190,pointRange);dynamicLight.bind(0x190,pointRange);
    staticLight.bind(0x198,pointRgb);dynamicLight.bind(0x198,pointRgb);
    staticLight.bind(0x68,pointPos);dynamicLight.bind(0x68,pointPos);
    staticLightTemplate.bind(0xF0,dynamicTemplate);dynamicLightTemplate.bind(0xF0,dynamicTemplate);
    field<void*>(&staticLight,0x14)=&staticLightTemplate;
    field<void*>(&dynamicLight,0x14)=&dynamicLightTemplate;
    testPlayer.bind(0x44,vehicle);testPlayer.bind(0x17C,gate);
    testCamera.bind(0x5C,disabled);testCamera.bind(0x60,disabled);
    testCamera.bind(0x64,cockpitDir);testCamera.bind(0x68,cockpitRgb);testCamera.bind(0x6C,damageRgb);
    testCamera.bind(0x70,frequency);testCamera.bind(0x74,threshold);testCamera.bind(0x78,ambientRgb);
    testVehicle.bind(0x58,armor);testArmor.bind(0x3C,health);testArmor.bind(0x84,maximumHealth);
    testSystem.bind(0x14,exactTime);
    templStorage.bind(0xFC,materialInteger);templStorage.bind(0xF4,materialGloss);templStorage.bind(0x44,materialEnv);
}
void clearSubmission(){nextEntry=0;routes.clear();shaders.clear();gatherCount=playerQueries=0;}
}
namespace renddx9abi {
void* engineAlloc(unsigned int n){return std::malloc(n);}
void engineFree(void* p){std::free(p);}
}
namespace dice { namespace hfe {
IBase* playerManager=reinterpret_cast<IBase*>(&pm);System* g_system=reinterpret_cast<System*>(&testSystem);
const Mat4& Mat4::getIdentity(){return identity;}
namespace Math {const Vec3& Vec3::getZero(){static const Vec3 v={};return v;}}
namespace dx9rend {
void* g_rendererBase=&base;IBase* g_Renderer=reinterpret_cast<IBase*>(&renderer);void* g_renderQueues=0;
void textureDtor(EngineTexture*){CHECK(false);}
RenderQueueEntry* renderEntryPoolAcquire()
{
    CHECK(nextEntry<64);unsigned int n=nextEntry++;
    // A fresh value-initialized entry each acquire: zeroed words, an empty lighting block.
    entries[n].~RenderQueueEntry();new(&entries[n]) RenderQueueEntry();
    std::memset(&records[n],0,sizeof(records[n]));
    entries[n].lightArray.lights.value=&records[n];return &entries[n];
}
void renderQueueEnqueue(RenderQueueEntry* e,bool t,bool f,bool d)
{routes.push_back({unsigned(t)+2u*unsigned(f)+4u*unsigned(d),e});}
void renderQueueAddToSlot(unsigned int s,RenderQueueEntry* e){routes.push_back({s,e});}
namespace Ra {
RaShader* getShader(RaShaderTable*,const std::string& name,bool skip)
{CHECK(std::strcmp(name.c_str(),"BMadditive")==0 && !skip);return reinterpret_cast<RaShader*>(0x1234);}
// [TEMP] Storage-only lighting block for the pooled entries and the copy probes: RaShader.cpp (the
// recovered constructor, registry and binder tables) is not linked. Every holder starts zeroed and
// every vector empty, the state the earlier zero-filled entries modeled.
RaLightingParameters::RaLightingParameters()
    : vptr(0),registry(0),unnamed008(),terrainLightMap(),lightMap(),lightMapOffset(),matBones(),
      overGrowthAmbient(),shadowViewProj(),shadowViewProjArray(),clampViewProjArray(),
      shadowPosArray(),shadowSizeArray(),shadowCount(),tail0FC(),tail10C(0){}
RaLightingParameters::~RaLightingParameters(){}
}
}
namespace geom {
LightManager* g_LightManager=reinterpret_cast<LightManager*>(&lm);
// The typed LightManager read the draw uses for its shadow fade (retail LightManager +0x838).
double lightManagerShadowFadeDuration(const LightManager* manager)
{CHECK(manager==g_LightManager);return 1.0;}
BundledMeshStateManager* g_bundledMeshStateManager=reinterpret_cast<BundledMeshStateManager*>(&manager);
const Mat4* BundledMesh::getGeometryTransformation()const{return m_partTransforms[0];}
void BundledMesh::updateHeightOverTerrain(){++heights;}
void BundledMesh::updateLodSelection(){++lods;}
unsigned int BundledMesh::animationIndex(const Vec3&){return animationValue;}
unsigned int BundledMesh::materialFeatures(BundledMeshMaterial*,bool,unsigned int){return mask;}
bool BundledMesh::castsDynamicShadow(){return useShadow;}
unsigned int BundledMesh::getShadowCasterFrame(){return frame;}
void* BundledMesh::refreshRootObject(){return m_ownerObject;}
int BundledMesh::getLodIndex()const{return m_lodIndex;}
int BundledMesh::getGeomIndex()const{return m_geomIndex;}
const Mat4& bundledMeshZeroMatrix(){return zero;}
void* staticMeshRootParent(void* object){return object;}
unsigned int bundledMeshQualityMask(unsigned int i){CHECK(i==0);return 0xFFFFFFFFu;}
dx9rend::Ra::RaShader* bundledMeshShaderForBits(unsigned int bits)
{shaders.push_back(bits);return reinterpret_cast<dx9rend::Ra::RaShader*>(bits);}
dx9rend::EngineTexture* bundledMeshEnvironmentTexture(){CHECK(false);return 0;}
bool bundledMeshTraceAdditive(BundledMesh*,const Vec3& origin,const Vec3& direction,void*& hit)
{CHECK(closeFloat(origin.x,camera.m[12]) && closeFloat(direction.x,rootTransform.m[12]-camera.m[12]));hit=traceObject;return traceHit;}
}
} }
int main()
{
    initialize();
    alignas(8) unsigned char meshMemory[sizeof(BundledMesh)]={};
    BundledMesh& mesh=*reinterpret_cast<BundledMesh*>(meshMemory);
    BundledMeshTemplate& templ=*reinterpret_cast<BundledMeshTemplate*>(&templStorage);
    alignas(4) unsigned char matMemory[sizeof(BundledMeshMaterial)]={};
    BundledMeshMaterial& material=*reinterpret_cast<BundledMeshMaterial*>(matMemory);
    alignas(4) unsigned char stateMemory[sizeof(BundledMeshState)]={};
    BundledMeshState& state=*reinterpret_cast<BundledMeshState*>(stateMemory);
    BundledMeshLod lod={};lod.partCount=2;const Mat4* parts[]={&rootTransform,&zero};
    BundledMeshMaterial* materials[]={&material};
    std::vector<BundledMeshMaterial*> materialVector(materials,materials+1);
    mesh.m_template=&templ;mesh.m_partTransforms=parts;mesh.m_parameters.transparency.value=1;
    mesh.m_lodRecord=&lod;mesh.m_lodMaterials=&materialVector;mesh.m_fov=-1;
    mesh.m_shadowSunHeight=100;mesh.m_shadowFadeTime=9;mesh.m_heightOverTerrain=12;
    templ.m_loaded=1;templ.m_maxPartCount=2;templ.m_bounds.min={-1,-2,-1};templ.m_bounds.max={1,2,1};
    templ.m_primitiveType=4;material.m_state=&state;material.m_kind=2;material.m_primitiveCount=7;
    rootTransform.m[12]=3;rootTransform.m[14]=4;
    enabled=false;mesh.draw(0,0,0);CHECK(lods==0 && heights==0);
    enabled=true;templ.m_loaded=0;mesh.draw(0,0,0);CHECK(lods==0);
    templ.m_loaded=1;mesh.m_parameters.transparency.value=0;mesh.draw(0,0,0);CHECK(lods==0);
    mesh.m_parameters.transparency.value=1;clearSubmission();mesh.draw(0,0,0);
    CHECK(lods==1 && heights==1 && mesh.m_drawFrame==frame);
    CHECK(parts[1]==parts[0] && closeFloat(mesh.m_distanceToCamera,5));
    CHECK(gatherCount==1 && requiredMask==0x8B09 && playerQueries==2);
    CHECK(routes.size()==1 && routes[0].index==0);
    CHECK(closeFloat(records[0].color[0],sunColor.x)); // clamped high height -> factor 1
    CHECK(field<int>(&material,0xB4)==127 && field<unsigned char>(&material,0x2C)==1);
    CHECK(closeFloat(field<float>(&material,0x94),64) && closeFloat(field<float>(&material,0x114),0.5f));
    CHECK(mesh.m_parameters.geomBones.a==reinterpret_cast<unsigned int>(parts) && mesh.m_parameters.geomBones.b==2);
    CHECK(closeFloat(mesh.m_parameters.heightOverTerrain.value,12));
    CHECK(std::memcmp(mesh.m_parameters.hemiMapSkyColor.value,&sky,12)==0);

    clearSubmission();mesh.m_shadowSunHeight=-100;mesh.draw(0,0,0);
    CHECK(closeFloat(records[0].color[0],sunColor.x*sampleValue) && heights==1);
    clearSubmission();expansion=true;mesh.m_underRoof=1;ioValue=0.5f;mesh.m_shadowFadeTime=9;
    mesh.draw(0,0,0);
    CHECK(closeFloat(mesh.m_shadowFade,0.5f) && requiredMask==0x8B01);
    CHECK(mesh.m_parameters.hemiMapSkyColor.value[0]==0 && mesh.m_parameters.hemiMapSkyColor.value[2]==0);
    CHECK(closeFloat(records[0].color[0],sunColor.x));
    expansion=false;mesh.m_underRoof=0;ioValue=1;
    clearSubmission();state.m_alphaTest=1;mask=0x401;mesh.m_fov=1.1f;
    g_bundledMeshStateManager->m_enabled9=1;mesh.draw(0,0,0);
    CHECK(playerQueries==1 && routes.size()==2 && routes[0].index==11 && routes[1].index==2);
    CHECK(field<int>(&material,0xB4)==15 && shaders[1]==0xF6000000u);
    clearSubmission();mask=1;mesh.draw(0,0,0);CHECK(field<unsigned char>(&material,0x2C)==0);
    clearSubmission();mesh.m_parameters.transparency.value=0.4f;mesh.draw(0,0,0);
    CHECK(gatherCount==0 && routes.size()==1 && routes[0].index==3);
    clearSubmission();gatherPointLights=true;mesh.m_parameters.transparency.value=1;
    mesh.m_shadowFade=0.5f;mesh.m_shadowFadeTime=10;state.m_alphaTest=0;
    mesh.draw(0,0,0);
    CHECK(routes.size()==4 && routes[2].index==6 && routes[3].index==6);
    CHECK(closeFloat(records[2].color[0],pointColor.x*0.5f) && closeFloat(records[3].color[0],pointColor.x));
    CHECK(closeFloat(records[2].invRangeSq,0.25f) && closeFloat(records[3].invRangeSq,1.0f/36.0f));
    CHECK(records[2].position[0]==7 && records[2].position[1]==8 && records[2].position[2]==9);
    CHECK(shaders.back()==0x801 && entries[2].shader==reinterpret_cast<dx9rend::Ra::RaShader*>(0x801));
    gatherPointLights=false;
    clearSubmission();usePlayer=useSky=true;mesh.m_ownerObject=&testVehicle;state.m_cockpit=1;mask=0x21;
    mesh.draw(0,0,0);
    CHECK(closeFloat(records[0].color[0],0.6f) && closeFloat(records[0].color[1],0.2f) && closeFloat(records[0].color[2],0.3f));
    CHECK(closeFloat(records[0].scaled24,0.198f) && records[0].unread28[0]==0);
    CHECK(records[0].direction[1]==-1 && shaders[0]==0x21 && routes.size()==2);
    usePlayer=useSky=false;mesh.m_ownerObject=0;state.m_cockpit=0;mask=1;
    alignas(4) unsigned char shadow[0x400]={};
    field<Mat4>(shadow,0x2F8)=identity;field<Mat4>(shadow,0x338)=identity;field<Mat4>(shadow,0x3B8)=identity;
    field<Mat4>(shadow,0x2F8).m[12]=17;field<Mat4>(shadow,0x338).m[12]=18;field<Mat4>(shadow,0x3B8).m[12]=19;
    clearSubmission();useShadow=true;mesh.m_shadowProjection=shadow;mesh.draw(0,0,0);
    CHECK(mesh.m_parameters.shadowProjMat.value[12]==17 && mesh.m_parameters.shadowOccProjMat.value[12]==18 && mesh.m_parameters.shadowTrapMat.value[12]==19);
    useShadow=false;mesh.m_shadowProjection=0;

    clearSubmission();material.m_kind=1;animationValue=2;mesh.m_parameters.transparency.value=1;
    state.m_animated=1;templ.m_dword10C=1;mesh.draw(0,0,0);
    CHECK(entries[0].lighting.tail10C==42 && field<unsigned char>(&material,0x24)==0);
    CHECK(mesh.m_parameters.userData.value.size()==2*64);
    CHECK(std::memcmp(mesh.m_parameters.userData.value.data(),&identity,64)==0);
    CHECK(std::memcmp(mesh.m_parameters.userData.value.data()+64,&identity,64)==0);
    Mat4 uv[2]={identity,identity};uv[1].m[12]=9;mesh.m_uvMatrices=uv;
    clearSubmission();templ.m_primitiveType=5;mesh.draw(0,0,0);
    CHECK(entries[0].lighting.tail10C==18 && reinterpret_cast<Mat4*>(mesh.m_parameters.userData.value.data())[1].m[12]==9);

    clearSubmission();state.m_alphaOne=1;traceHit=false;mesh.draw(0,0,0);
    CHECK(routes.size()==1 && routes[0].index==10 && mesh.m_parameters.transparency.value==1);
    CHECK(mesh.m_parameters.geomBones.a==reinterpret_cast<unsigned int>(&mesh.m_inverseTransform));
    CHECK(mesh.m_parameters.geomBones.b==1 && mesh.m_dword2A4==frame);
    CHECK(closeFloat(mesh.m_inverseTransform->m[12],3) && closeFloat(mesh.m_inverseTransform->m[14],4));
    clearSubmission();traceHit=true;traceObject=reinterpret_cast<void*>(2);++frame;mesh.draw(0,0,0);
    CHECK(closeFloat(mesh.m_parameters.transparency.value,0.75f) && routes.size()==1);
    clearSubmission();frame+=3;mesh.draw(0,0,0);
    CHECK(mesh.m_parameters.transparency.value==0 && routes.empty());

    // Parameter copy retains holder vptrs, owns copied vectors, frees on empty assignment.
    dx9rend::Ra::RaLightingParameters src,dst;
    src.registry=reinterpret_cast<void*>(17);dst.unnamed008.vptr=reinterpret_cast<void*>(19);
    src.unnamed008.value[2]=7;src.tail10C=3;
    src.shadowViewProjArray.value.resize(64);
    reinterpret_cast<Mat4*>(src.shadowViewProjArray.value.data())->m[8]=27;
    copyLighting(dst,src);
    CHECK(dst.registry==src.registry && dst.unnamed008.vptr==reinterpret_cast<void*>(19));
    CHECK(dst.unnamed008.value[2]==7 && dst.tail10C==3);
    CHECK(dst.shadowViewProjArray.value.data()!=src.shadowViewProjArray.value.data());
    CHECK(reinterpret_cast<Mat4*>(dst.shadowViewProjArray.value.data())->m[8]==27);
    CountedTexture oldTexture,newTexture;
    dst.terrainLightMap.value=&oldTexture;src.terrainLightMap.value=&newTexture;
    copyLighting(dst,src);
    CHECK(comEvents=="RA" && oldTexture.refs==0 && newTexture.refs==2);
    copyLighting(dst,src);copyLighting(dst,dst);
    CHECK(comEvents=="RA" && newTexture.refs==2);
    src.terrainLightMap.value=0;copyLighting(dst,src);
    CHECK(comEvents=="RAR" && newTexture.refs==1);
    // An empty source releases the destination's storage, as the shipped vector assignment does.
    std::vector<unsigned char>().swap(src.shadowViewProjArray.value);copyLighting(dst,src);
    CHECK(dst.shadowViewProjArray.value.data()==0 && dst.shadowViewProjArray.value.capacity()==0);
    // The mesh lives in zeroed raw storage, so release the vectors its draws grew by hand.
    std::vector<unsigned char>().swap(mesh.m_parameters.userData.value);
    std::vector<world::LightSource*>().swap(mesh.m_lights);
    std::vector<dx9rend::Ra::RaLightRecord>().swap(mesh.m_lightRecords);
    renddx9abi::engineFree(mesh.m_inverseTransform);
    std::printf("BundledMesh draw: %d checks passed (strict x86 fixture).\n",checks);
}

#include "renderer_accessor_fixture_support.h"   // RendDX9Base.h Renderer accessors (x86 identity)
