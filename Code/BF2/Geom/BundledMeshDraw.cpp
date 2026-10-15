// Implementation belongs to the attested Geom/BundledMesh.cpp TU (1021B300).
// [INFERRED] source split keeps its main draw beside the recovered instance owner.
// RendDX9.dll 1.50: 10104F70..101063A9, all instructions read. No dedicated ELF body.
#ifdef BF2_IMAGE_RENDDX9
#include "BundledMesh.h"
#include "BundledMeshMaterial.h"
#include "BundledMeshStateManager.h"
#include "BundledMeshTemplate.h"
#include "StaticMesh.h"
#include "XpLightManager.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../Core/System.h"
// Match XpLightManager's engine-interface import: retail HfeAbi already supplies
// ClassManager's VC7.1 registration carriers. Only the virtual interfaces below
// cross this boundary; native STL fields are never read in retail mode.
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/GameLogicComp/IArmorGLComp.h"
#include "../Game/Objects/Visual/LightSource.h"
#include "../Game/Objects/Visual/LightSourceTemplate.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../RendDX9/Texture.h"
#include <cmath>
#include <cstring>
#include <new>
#include <stdexcept>

namespace dice { namespace hfe {
extern IBase* playerManager;
namespace dx9rend {
extern void* g_rendererBase;
extern IBase* g_Renderer;
extern void* g_renderQueues;
}
namespace geom {
extern LightManager* g_LightManager;
// The renderer-owned environment texture; 10105987 reads the owner's +64.
dx9rend::EngineTexture* bundledMeshEnvironmentTexture();

namespace {
using dx9rend::RenderQueueEntry;
using dx9rend::Ra::RaLightRecord;
using dx9rend::Ra::RaLightingParameters;

template<class T> T& field(void* p,unsigned int offset)
{ return *reinterpret_cast<T*>(static_cast<unsigned char*>(p)+offset); }
// The material's Ra parameter block (material +0x10); the `+0xNN` comments below are the retail
// material offsets the shipped draw writes.
RaMaterialParameters& materialParams(BundledMeshMaterial* m) { return bundledMeshMaterialParameters(m); }
template<class R> R slot(void* p,unsigned int offset)
{
    typedef R(__thiscall* Fn)(void*);
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(p))[offset/4])(p);
}
unsigned int frameOrdinal() { return slot<unsigned int>(dx9rend::g_rendererBase,0x54); }
world::IPlayer* localPlayer()
{ return reinterpret_cast<world::PlayerManager*>(playerManager)->getLocalHumanPlayer(); }

// query this object first, then each parent. Native Object storage is a
// source layout; the retail DLL adapter uses its attested +28 parent link.
world::IPlayerControlObject* controlObject(void* object)
{
    world::IObject* current=static_cast<world::IObject*>(object);
    while(current)
    {
        void* const found=current->queryInterface(50373u);
        if(found) return static_cast<world::IPlayerControlObject*>(found);
        current=current->getParent();
    }
    return 0;
}
const world::LightSourceTemplate* lightTemplate(world::LightSource* light)
{
    return static_cast<const world::LightSourceTemplate*>(light->getTemplate());
}
float lightRange(world::LightSource* light)
{
    return light->getAttenuationRange2();
}
const Vec3& lightColor(world::LightSource* light)
{
    return light->getColor();
}
const Vec3& lightDirection(world::LightSource* light)
{
    return light->getDirection();
}

// 100777A0, including the unit-length early return and zero-length refusal.
void normalize(Vec3& v)
{
    const float lengthSquared=v.x*v.x+v.y*v.y+v.z*v.z;
    const double epsilon=1.1920928955078125e-7;
    if(std::fabs(lengthSquared-1.0)>=epsilon)
    {
        if(std::fabs(static_cast<double>(lengthSquared))<epsilon)
            v.x=v.y=v.z=0.0f;
        else
        {
            const double scale=1.0/std::sqrt(static_cast<double>(lengthSquared));
            v.x=static_cast<float>(scale*v.x);
            v.y=static_cast<float>(scale*v.y);
            v.z=static_cast<float>(scale*v.z);
        }
    }
}
Vec3 subtract(const Vec3& a,const Vec3& b)
{ const Vec3 result={a.x-b.x,a.y-b.y,a.z-b.z};return result; }
const Vec3& position(const Mat4& m) { return *reinterpret_cast<const Vec3*>(m.m+12); }
void copy3(float* out,const Vec3& value) { std::memcpy(out,&value,sizeof(value)); }

// typed parameter assignment preserves each holder's own vptr. The
// two texture values are COM interfaces (the PE pushes this, then calls stdcall
// +8/+4); these are not EngineTexture wrappers or IBase refcount slots.
void copyLighting(RaLightingParameters& dest,const RaLightingParameters& src)
{
    dest.registry=src.registry;
    std::memcpy(dest.unnamed008.value,src.unnamed008.value,16);
    dx9rend::Ra::RaParamRef* drefs[]={&dest.terrainLightMap,&dest.lightMap};
    const dx9rend::Ra::RaParamRef* srefs[]={&src.terrainLightMap,&src.lightMap};
    for(unsigned int i=0;i<2;++i)
    {
        if(drefs[i]!=srefs[i] && drefs[i]->value!=srefs[i]->value)
        {
            if(drefs[i]->value)static_cast<IUnknown*>(drefs[i]->value)->Release();
            drefs[i]->value=srefs[i]->value;
            if(drefs[i]->value)static_cast<IUnknown*>(drefs[i]->value)->AddRef();
        }
    }
    std::memcpy(dest.lightMapOffset.value,src.lightMapOffset.value,16);
    dest.matBones.a=src.matBones.a;dest.matBones.b=src.matBones.b;
    std::memcpy(dest.overGrowthAmbient.value,src.overGrowthAmbient.value,16);
    std::memcpy(&dest.shadowViewProj.value,&src.shadowViewProj.value,64);
    // 10106E40/10106F90, vector assignment: an empty source releases the destination's storage.
    dx9rend::Ra::raAssignParamBytes(dest.shadowViewProjArray.value,src.shadowViewProjArray.value);
    dx9rend::Ra::raAssignParamBytes(dest.clampViewProjArray.value,src.clampViewProjArray.value);
    dx9rend::Ra::raAssignParamBytes(dest.shadowPosArray.value,src.shadowPosArray.value);
    dx9rend::Ra::raAssignParamBytes(dest.shadowSizeArray.value,src.shadowSizeArray.value);
    dest.shadowCount.value=src.shadowCount.value;
    dx9rend::Ra::raAssignParamBytes(dest.tail0FC,src.tail0FC);
    dest.tail10C=src.tail10C;
}

// 100CE120 -> 101C6750/101C67F0 -> 101C6C70/101C6E10. The eight corners are
// emitted in x/y/z bit order; maximum/minimum comparisons retain NaN behavior.
float worldHeight(const MeshBounds& bounds,const Mat4& world)
{
    Vec3 corners[8]={};
    for(unsigned int i=0;i<8;++i)
    {
        const float x=(i&4)?bounds.max.x:bounds.min.x;
        const float y=(i&2)?bounds.max.y:bounds.min.y;
        const float z=(i&1)?bounds.max.z:bounds.min.z;
        corners[i].x=i==0?z*world.m[8]+x*world.m[0]+y*world.m[4]+world.m[12]
                              :world.m[4]*y+z*world.m[8]+x*world.m[0]+world.m[12];
        corners[i].y=i==0?x*world.m[1]+y*world.m[5]+z*world.m[9]+world.m[13]
                              :x*world.m[1]+z*world.m[9]+y*world.m[5]+world.m[13];
        corners[i].z=z*world.m[10]+x*world.m[2]+y*world.m[6]+world.m[14];
    }
    Vec3 maximum=corners[0],minimum=corners[0];
    for(unsigned int i=1;i<8;++i)
    {
        if(maximum.x<corners[i].x)maximum.x=corners[i].x;
        if(maximum.y<corners[i].y)maximum.y=corners[i].y;
        if(maximum.z<corners[i].z)maximum.z=corners[i].z;
    }
    for(unsigned int i=1;i<8;++i)
    {
        if(minimum.x>corners[i].x)minimum.x=corners[i].x;
        if(minimum.y>corners[i].y)minimum.y=corners[i].y;
        if(minimum.z>corners[i].z)minimum.z=corners[i].z;
    }
    return maximum.y-minimum.y;
}
void configureAlpha(BundledMeshMaterial* material,unsigned int mask)
{
    RaMaterialParameters& p=materialParams(material);
    if(material->m_kind==2)
    {
        p.alphaTest.value=1;                                     // +0x2C
        if(material->m_state->m_alphaTest)
        {
            if(mask&0x400)p.alphaTestRef.value=15;               // +0xB4
            else p.alphaTest.value=0;
        }
        else p.alphaTestRef.value=127;
    }
    else if(material->m_kind==1)
    {
        p.alphaTest.value=1;
        p.alphaTestRef.value=0;
        p.alphaBlendEnable.value=1;                              // +0x1C
        p.cullMode.value=1;                                      // +0xAC
        p.depthWrite.value=0;                                    // +0x24
    }
}
void setEntry(RenderQueueEntry* entry,BundledMeshMaterial* material,void* object,
              dx9rend::Ra::RaShader* shader)
{
    entry->material=material;entry->materialParameters=material->m_parameters;
    entry->objectParameters=object;entry->shader=shader;
}
} // anonymous namespace

void BundledMesh::draw(void*,void*,void*)
{
    if(!g_bundledMeshStateManager->getEnabled())return;
    if(!m_template->m_loaded)return;
    if(!m_partTransforms)return;
    if(m_parameters.transparency.value<1.0e-5f)return;
    const unsigned int frame=frameOrdinal();
    if(frame-m_heightFrame>=2)
    { m_heightFrame=frame;updateHeightOverTerrain(); }
    void* const view=slot<void*>(dx9rend::g_rendererBase,0x48);
    const Mat4* const transform=getGeometryTransformation();
    const Mat4& camera=slot<const Mat4&>(view,0x64);
    const Vec3 difference=subtract(position(*transform),position(camera));
    m_distanceToCamera=static_cast<float>(std::sqrt(
        difference.z*difference.z+difference.y*difference.y+difference.x*difference.x));
    std::memcpy(&m_parameters.tail220,&m_distanceToCamera,4);
    ++field<unsigned int>(slot<void*>(dx9rend::g_rendererBase,0x6C),0x240);
    updateLodSelection();
    if(m_template->m_maxPartCount>1 && m_partTransforms[1]==&bundledMeshZeroMatrix())
        for(unsigned int i=1;i<m_template->m_maxPartCount;++i)m_partTransforms[i]=m_partTransforms[0];

    float hemiFactor=1.0f,ambientScale=1.0f,fadeTarget=1.0f;
    bool inside=false;
    if(slot<bool>(dx9rend::g_rendererBase,0xAC))
    {
        const Vec3& p=position(*m_partTransforms[0]);
        fadeTarget=g_LightManager->sampleSimpleShadowMapIO(p.x,p.z);
        if(m_underRoof)ambientScale=0.0f;
    }
    else
    {
        const Vec3& p=position(*m_partTransforms[0]);
        const float sample=g_LightManager->sampleSimpleShadowMap(p.x,p.z);
        const float io=g_LightManager->sampleSimpleShadowMapIO(p.x,p.z);
        inside=io<0.5f;
        float factor=1.0f;
        if(m_underRoof)
        { if(io<0.5f)factor=sample; }
        else if(!(io<0.5f))
        {
            const Vec2& range=g_LightManager->getShadowSunHeightOverFade();
            float u=(m_shadowSunHeight-range.x)/(range.y-range.x);
            // 101051BA consumes the clamp's ST0 result at 101051BF..CA.
            if(u>=1.0f)u=1.0f;
            if(u<0.0f)u=0.0f;
            factor=u+(1.0f-u)*sample;
        }
        hemiFactor=static_cast<float>(std::pow(static_cast<double>(factor),
                     static_cast<double>(g_LightManager->getShadowSunScalePower())));
    }
    void* const timer=dx9rend::rendererFrameTimer();                          // +0x5DC
    // The timer is a raw 48-byte block of integers and doubles (BF2Render.cpp), so its total time
    // sits at +0x20 on every target.
    const double now=field<double>(timer,0x20);
    const double elapsed=now-m_shadowFadeTime;
    m_shadowFadeTime=now;
    double fraction=elapsed/lightManagerShadowFadeDuration(g_LightManager);   // +0x838
    if(fraction>1.0)fraction=1.0;
    m_shadowFade=static_cast<float>(m_shadowFade+fraction*(fadeTarget-m_shadowFade));
    m_lights.clear();

    bool acceptLights=true,staticSun=false;
    world::ICameraObject* cameraGate=0;
    world::IPlayer* player=localPlayer();
    if(player)
    {
        world::IObject* const vehicle=player->getVehicle();
        if(vehicle)
        {
            void* const root=refreshRootObject();
            if(staticMeshRootParent(vehicle)==root)
            {
                cameraGate=player->getCamera();
                if(cameraGate)
                {
                    acceptLights=cameraGate->getAllowDynamicLights();
                    staticSun=!cameraGate->getAllowDynamicSun();
                }
            }
        }
    }
    if(m_parameters.transparency.value>=1.0f)
    {
        unsigned int required=0x8B01;
        if(m_shadowFade==0.0f || !slot<bool>(dx9rend::g_Renderer,0xAC))required=0x8B09;
        const int maximum=g_LightManager->getMaxLightsPerBundledMesh();
        g_LightManager->gatherLightsForGeometry(this,m_lights,maximum,required,0,true);
    }
    const unsigned int lightCount=static_cast<unsigned int>(m_lights.size());
    {
        // 10102850->10102690: new records are zero up to +38 with 1.0f at +3C and +40; +38 is
        // uninitialized in the shipped default and overwritten below.
        RaLightRecord fill;
        std::memset(&fill,0,0x38);
        fill.float3C=fill.float40=1.0f;
        m_lightRecords.resize(lightCount,fill);
    }
    for(unsigned int i=0;i<lightCount;++i)
    {
        world::LightSource* const light=m_lights[i];
        float range=lightRange(light);
        const world::LightSourceTemplate* templ=lightTemplate(light);
        if(templ && templ->getIsDynamic())range*=g_LightManager->getDynamicLightRangeScale();
        RaLightRecord& record=m_lightRecords[i];
        copy3(record.position,light->getAbsolutePosition());
        record.invRangeSq=1.0f/(range*range);
        Vec3 color=lightColor(light);
        templ=lightTemplate(light);
        if(!templ->getIsDynamic())
        { color.x*=m_shadowFade;color.y*=m_shadowFade;color.z*=m_shadowFade; }
        copy3(record.color,color);
    }
    float damagePulse=0.0f;
    const float epsilon=1.1920928955078125e-7f;
    if(cameraGate && cameraGate->getInsideDamageFrequency()>epsilon)
    {
        world::IObject* const root=static_cast<world::IObject*>(refreshRootObject());
        if(root)
        {
            world::IArmorGLComp* const armor=static_cast<world::IArmorGLComp*>(
                root->queryComponent(50345u,50345u));
            if(armor)
            {
                const float health=armor->getHitPoints();
                const float ratio=health/armor->getMaxHitPoints();
                if(cameraGate->getInsideDamageThreshold()>=ratio)
                {
                    const double time=g_system->getExactTime();
                    const double period=2.0/cameraGate->getInsideDamageFrequency();
                    const float phase=static_cast<float>(std::fmod(time,period));
                    damagePulse=cameraGate->getInsideDamageFrequency()*phase;
                    if(damagePulse>1.0f)damagePulse=2.0f-damagePulse;
                }
            }
        }
    }
    unsigned int fovBits;std::memcpy(&fovBits,&m_fov,4);
    bool firstPerson=fovBits!=0xBF800000u;
    if(!firstPerson)
    {
        player=localPlayer();
        if(player && player->hasFirstPersonView())
        {
            world::IPlayerControlObject* const local=controlObject(player->getVehicle());
            if(local && !local->getNeverDrawAs1p() && !local->isChildSoldierActive())
            {
                world::IPlayerControlObject* const mine=controlObject(m_ownerObject);
                if(m_ownerObject && mine && !mine->getNeverDrawAs1p())
                {
                    void* const localRoot=staticMeshRootParent(player->getVehicle());
                    firstPerson=localRoot==staticMeshRootParent(m_ownerObject);
                }
                if(player->getOverheadCameraTopDownView())firstPerson=false;
            }
        }
    }
    const unsigned int materialCount=static_cast<unsigned int>(m_lodMaterials->size());
    const Mat4& animationCamera=slot<const Mat4&>(slot<void*>(dx9rend::g_rendererBase,0x50),0x64);
    const unsigned int animation=animationIndex(position(animationCamera));
    const float heightScale=2.0f/worldHeight(m_template->m_bounds,*m_partTransforms[0]);
    m_parameters.heightOverTerrain.value=m_heightOverTerrain;
    void* const video=slot<void*>(dx9rend::g_rendererBase,0x74);
    typedef unsigned int(__thiscall* Caps)(void*,int);
    const unsigned int caps=reinterpret_cast<Caps>((*reinterpret_cast<void***>(video))[1])(video,4);

    for(unsigned int index=0;index<materialCount;++index)
    {
        BundledMeshMaterial* const material=(*m_lodMaterials)[index];
        BundledMeshState* const state=material->m_state;
        unsigned int mask=materialFeatures(material,false,caps);
        if(inside && !(mask&0x1000))mask=(mask&~0x100u)|0x200u;
        unsigned int quality=dx9rend::rendererRaPath();                         // +0x6CC
        if(dx9rend::rendererTailByte(0x708) && m_shaderLod>quality)quality=m_shaderLod;
        const unsigned int qualityMask=bundledMeshQualityMask(quality);
        mask&=qualityMask;if(!(mask&0x5F))mask|=1;
        RenderQueueEntry* const entry=dx9rend::renderEntryPoolAcquire();
        RaLightRecord* const sun=static_cast<RaLightRecord*>(entry->lightArray.lights.value);
        if(staticSun && state->m_cockpit)
        {
            const Vec3& ambient=cameraGate->getStaticSunAmbientColor();
            const Vec3& damage=cameraGate->getStaticSunDamageColor();
            const Vec3& color=cameraGate->getStaticSunColor();
            const Vec3 blended={color.x+(damage.x-color.x)*damagePulse,
                                color.y+(damage.y-color.y)*damagePulse,
                                color.z+(damage.z-color.z)*damagePulse};
            copy3(sun->color,blended);
            sun->scaled24=ambient.z*0.33f+ambient.y*0.33f+ambient.x*0.33f;
            copy3(sun->unread28,Vec3::getZero());
        }
        else
        {
            mask&=~0x20u;
            copy3(sun->color,g_LightManager->getSunColor());
            sun->scaled24=0.7f;
            copy3(sun->unread28,g_LightManager->getSunSpecColor());
            if(!(mask&0x1000))
            {
                for(unsigned int i=0;i<3;++i)sun->color[i]*=hemiFactor;
                for(unsigned int i=0;i<4;++i)sun->unread28[i]*=hemiFactor;
            }
        }
        configureAlpha(material,mask);
        RaMaterialParameters& params=materialParams(material);
        raMaterialShaderSlot(params)=bundledMeshShaderForBits(mask);                    // +0x15C
        params.specularPower.value=static_cast<float>(m_template->getMaterialValue100(  // +0x94
            m_geomIndex,m_lodIndex,static_cast<unsigned char>(index)));
        if(state->m_envMap)
        {
            dx9rend::EngineTexture* const texture=bundledMeshEnvironmentTexture();
            dx9rend::EngineTexture*& old=                                               // +0x130
                reinterpret_cast<dx9rend::EngineTexture*&>(params.cubeMap.value);
            if(old!=texture)
            {
                if(old && --old->refCount==0)
                { dx9rend::textureDtor(old);::renddx9abi::engineFree(old); }
                old=texture;
                if(texture)slot<void>(texture,0x08);
            }
            const int lod=getLodIndex();
            const int geom=getGeomIndex();
            params.reflectivity.value=m_template->slot_0x44(geom,lod,index);           // +0xA4
        }
        if(state->m_feature39)
            params.staticGloss.value=m_template->getSpecularStaticGloss(               // +0x104
                m_geomIndex,m_lodIndex,static_cast<unsigned char>(index));
        unsigned int currentFovBits;std::memcpy(&currentFovBits,&m_fov,4);
        void* const projectionView=slot<void*>(dx9rend::g_rendererBase,
                                                currentFovBits==0xBF800000u?0x50:0xD4);
        // A pointer in the holder's first word: RaParamPair must be pointer-sized on x64.
        m_parameters.geomBones.a=reinterpret_cast<uintptr_t>(m_partTransforms);
        m_parameters.geomBones.b=m_lodRecord->partCount;
        std::memcpy(&m_parameters.viewProjection.value,
                    &slot<const Mat4&>(projectionView,0x84),sizeof(Mat4));
        if(state->m_animated)
        {
            const unsigned int count=m_template->m_dword10C+1;
            // 10106DC0: resize the byte vector; new bytes are zero-filled.
            m_parameters.userData.value.resize(count<<6);
            for(int i=0;i<static_cast<int>(count);++i)
                std::memcpy(m_parameters.userData.value.data()+64*i,
                    m_uvMatrices?&m_uvMatrices[i]:&Mat4::getIdentity(),64);
        }
        if(castsDynamicShadow())
        {
            const unsigned int shadowFrame=frameOrdinal();
            if(getShadowCasterFrame()==shadowFrame && m_shadowProjection && !(staticSun && state->m_cockpit))
            {
                const LightShadowRecord* const record=static_cast<const LightShadowRecord*>(m_shadowProjection);
                std::memcpy(&m_parameters.shadowProjMat.value,&record->matrices[4],64);     // +0x2F8
                std::memcpy(&m_parameters.shadowTrapMat.value,&record->matrices[7],64);     // +0x3B8
                std::memcpy(&m_parameters.shadowOccProjMat.value,&record->matrices[5],64);  // +0x338
            }
        }
        if(slot<bool>(dx9rend::g_Renderer,0xAC))
        {
            const Vec3& sky=g_LightManager->getSkyColor();
            const Vec3 color={sky.x*ambientScale,sky.y*ambientScale,sky.z*ambientScale};
            copy3(m_parameters.hemiMapSkyColor.value,color);
        }
        else copy3(m_parameters.hemiMapSkyColor.value,g_LightManager->getSkyColor());
        configureAlpha(material,mask);                    // repeated in the shipped draw
        params.invHemiHeightScale.value=heightScale;                                   // +0x114
        setEntry(entry,material,&m_parameters,
                 static_cast<dx9rend::Ra::RaShader*>(raMaterialShaderSlot(params)));   // +0x15C
        if(material->m_kind==1)
        {
            if(m_template->m_primitiveType==4)entry->lighting.tail10C=3*animation*material->m_primitiveCount;
            else if(m_template->m_primitiveType==5)entry->lighting.tail10C=animation*(material->m_primitiveCount+2);
        }
        world::LightSource* const skyLight=g_LightManager->getSkyLight();
        if(skyLight)
        {
            if(staticSun && state->m_cockpit)copy3(sun->direction,cameraGate->getStaticSunDirection());
            else
            {
                Vec3 direction=lightDirection(skyLight);
                if(dx9rend::rendererRenderSwitch(0x69C))
                {
                    const Mat4& eye=slot<const Mat4&>(slot<void*>(dx9rend::g_Renderer,0x48),0x64);
                    direction=subtract(position(*getGeometryTransformation()),position(eye));
                    normalize(direction);
                }
                copy3(sun->direction,direction);
            }
        }
        if(state->m_alphaOne)
        {
            if(!m_inverseTransform)
            {
                m_inverseTransform=static_cast<Mat4*>(::renddx9abi::engineAlloc(64));
                if(m_inverseTransform)*m_inverseTransform=Mat4::getIdentity();
            }
            const Vec3 origin=position(slot<const Mat4&>(view,0x64));
            const Vec3 direction=subtract(position(*getGeometryTransformation()),origin);
            void* hitObject=0;
            const bool hit=bundledMeshTraceAdditive(this,origin,direction,hitObject);
            float& fade=*reinterpret_cast<float*>(&m_dword2A0);         // +2A0
            if(!hit || hitObject==refreshRootObject())fade=1.0f;
            else if(fade>epsilon)
            {
                fade=static_cast<float>(fade-dx9rend::rendererTailFloat(0x59C)
                                              *dx9rend::rendererTailDouble(0x5E8));
                if(fade<0.0f || m_dword2A4+1<frameOrdinal())fade=0.0f;
            }
            m_parameters.transparency.value=fade;
            if(fade>epsilon && m_partTransforms)
            {
                const Mat4& root=*m_partTransforms[0];
                const Mat4& eye=slot<const Mat4&>(view,0x64);
                Vec3& forward=*reinterpret_cast<Vec3*>(m_inverseTransform->m+8);
                forward.x=-eye.m[8];forward.y=-eye.m[9];forward.z=-eye.m[10];
                normalize(forward);
                Vec3 up=*reinterpret_cast<const Vec3*>(root.m+4);
                normalize(up);
                if(up.z*forward.z+up.y*forward.y+up.x*forward.x>0.99f)
                { up=*reinterpret_cast<const Vec3*>(root.m);normalize(up); }
                Vec3& right=*reinterpret_cast<Vec3*>(m_inverseTransform->m);
                right.x=up.y*forward.z-up.z*forward.y;
                right.y=up.z*forward.x-up.x*forward.z;
                right.z=up.x*forward.y-up.y*forward.x;
                Vec3& vertical=*reinterpret_cast<Vec3*>(m_inverseTransform->m+4);
                vertical.x=right.z*forward.y-right.y*forward.z;
                vertical.y=right.x*forward.z-forward.x*right.z;
                vertical.z=forward.x*right.y-right.x*forward.y;
                copy3(m_inverseTransform->m+12,position(root));
                m_parameters.geomBones.a=reinterpret_cast<uintptr_t>(&m_inverseTransform);
                m_parameters.geomBones.b=1;
                entry->shader=dx9rend::Ra::getShader(
                    static_cast<dx9rend::Ra::RaShaderTable*>(dx9rend::g_renderQueues),std::string("BMadditive"),false);
                dx9rend::renderQueueAddToSlot(10,entry);
                m_dword2A4=frameOrdinal();
            }
        }
        else
        {
            const bool transparent=material->m_kind==1 || m_parameters.transparency.value<1.0f;
            if(g_bundledMeshStateManager->m_enabled9 && firstPerson && !transparent)
            {
                RenderQueueEntry* const depth=dx9rend::renderEntryPoolAcquire();
                setEntry(depth,material,&m_parameters,bundledMeshShaderForBits(0xF6000000u));
                copyLighting(depth->lighting,entry->lighting);
                dx9rend::renderQueueAddToSlot(11,depth);
            }
            dx9rend::renderQueueEnqueue(entry,transparent,firstPerson,false);
            if(acceptLights && !state->m_cockpit)
            {
                unsigned int delta=0;
                if(state->m_animated)delta|=2;if(state->m_normalMap)delta|=4;
                if(state->m_extraMap)delta|=8;if(state->m_envMap)delta|=16;
                if(state->m_cockpit)delta|=32;if(delta==0 || delta==32)delta|=1;
                if(state->m_colorMapGloss)delta|=0x100;if(state->m_feature39)delta|=0x200;
                if(state->m_alphaTest)delta|=0x400;delta|=0x800;
                delta&=qualityMask;if(!(delta&0x5F))delta|=1;
                dx9rend::Ra::RaShader* const shader=bundledMeshShaderForBits(delta);
                for(unsigned int i=0;i<m_lightRecords.size();++i)
                {
                    RenderQueueEntry* const lit=dx9rend::renderEntryPoolAcquire();
                    setEntry(lit,material,&m_parameters,shader);
                    *static_cast<RaLightRecord*>(lit->lightArray.lights.value)=m_lightRecords[i];
                    copyLighting(lit->lighting,entry->lighting);
                    dx9rend::renderQueueEnqueue(lit,transparent,firstPerson,true);
                }
            }
        }
    }
    m_drawFrame=frameOrdinal();
}
} } }
#endif
