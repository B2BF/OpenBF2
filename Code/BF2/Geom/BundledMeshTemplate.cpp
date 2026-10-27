// attested: Geom/BundledMeshTemplate.cpp, RendDX9.dll1.50 string1021A3C8.
// PE-only renderer recovery. Addresses identify the instruction-read witnesses.
// VC7.1 container ABI adapters retain the existing RW-74 balancing policy.
#ifdef BF2_IMAGE_RENDDX9
#define NOMINMAX
#include "BundledMeshTemplate.h"
#include "BundledMeshMaterial.h"
#include "BundledMeshStateManager.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/EngineFile.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/Texture.h"
#include "../RendDX9/Ra/RaShader.h"
#include "../IO/Stream/IStream.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <new>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace dice { namespace hfe {
namespace dx9rend {
extern IBase* g_Renderer;
extern void* g_deviceCaps;
extern void* g_renderQueues;
extern void* g_bufferManager;
extern float g_materialCapScale, g_materialCapBias, g_materialCapC, g_materialCapD;
void* createVertexBuffer(void*,unsigned int,unsigned int,const vc71::string&,unsigned int);
void* vertexBufferLock(void*,unsigned int,unsigned int,unsigned int);
bool vertexBufferUnlock(void*);
unsigned int rendererFrameOrdinal();
}
namespace geom {
extern unsigned char g_bundledMeshTechniquesPending;
extern dx9rend::Effect* g_bundledShadowEffect;
bool meshBuildTechniqueMasks(const vc71::string&,const vc71::string&,const char* const*,
    std::map<unsigned int,dx9rend::Ra::RaShader*>&,vc71::vector_layout&);

namespace {
template<class T> unsigned int count(const vc71::vector_layout& v)
{ return v.first ? static_cast<unsigned int>((v.last-v.first)/sizeof(T)) : 0; }
template<class T> T* data(vc71::vector_layout& v) { return reinterpret_cast<T*>(v.first); }
template<class T> const T* data(const vc71::vector_layout& v) { return reinterpret_cast<const T*>(v.first); }
void initialize(vc71::vector_layout& v) { v.first=v.last=v.end=0; }
void dispose(vc71::vector_layout& v)
{ if(v.first) ::renddx9abi::engineFree(v.first); v.first=v.last=v.end=0; }
// Source-level vector resize. Records here are trivially relocatable; nested vector
// contents are explicitly destroyed by their owner, matching the PE's instantiations.
template<class T> void resize(vc71::vector_layout& v,unsigned int n,const T& value)
{
    const unsigned int old=count<T>(v);
    if(n<=old) { v.last=v.first+n*sizeof(T); return; }
    const unsigned int maximum=0xFFFFFFFFu/sizeof(T);
    if(n>maximum) throw std::length_error("vector<T> too long");
    const unsigned int capacity=v.first?static_cast<unsigned int>((v.end-v.first)/sizeof(T)):0;
    if(n>capacity)
    {
        unsigned int next=maximum-(capacity>>1)>=capacity?capacity+(capacity>>1):0;
        if(next<n) next=n;
        char* p=static_cast<char*>(::renddx9abi::engineAlloc(next*sizeof(T)));
        if(old) std::memcpy(p,v.first,old*sizeof(T));
        if(v.first) ::renddx9abi::engineFree(v.first);
        v.first=p; v.end=p+next*sizeof(T);
    }
    for(unsigned int i=old;i<n;++i) new(v.first+i*sizeof(T)) T(value);
    v.last=v.first+n*sizeof(T);
}
void zeroBounds(MeshBounds& b)
{
    b.min=Vec3{0,0,0}; b.max=Vec3{0,0,0}; b.originRadius=b.radius=0;
    b.extent=Vec3{0,0,0}; b.center=Vec3{0,0,0}; b.dirty=1;
}
const Vec3& unitScale() { static const Vec3 v={1,1,1}; return v; }
vc71::vector_layout emptyVector() { vc71::vector_layout v; initialize(v); return v; }
unsigned int packedKey(int g,int l,unsigned char m)
{ return (static_cast<unsigned int>(static_cast<unsigned char>(g))<<16)
       | (static_cast<unsigned int>(static_cast<unsigned char>(l))<<8) | m; }
float asFloat(unsigned int v) { float f; std::memcpy(&f,&v,4); return f; }
unsigned int asBits(float v) { unsigned int i; std::memcpy(&i,&v,4); return i; }
vc71::vector_layout& materials(BundledMeshTemplate* t,int g,int l)
{ return data<vc71::vector_layout>(data<vc71::vector_layout>(t->m_materials)[g])[l]; }
int rendererInt(unsigned int offset)
{ return *reinterpret_cast<const int*>(reinterpret_cast<const char*>(dx9rend::g_Renderer)+offset); }
void constructSet(vc71::set_layout& s) //100EB000, comparator/key/padding untouched
{
    vc71::set_node* p=static_cast<vc71::set_node*>(::renddx9abi::engineAlloc(20));
    if(p) p->left=0;
    if(reinterpret_cast<unsigned int>(p)!=0xFFFFFFFCu) p->parent=0;
    if(reinterpret_cast<unsigned int>(p)!=0xFFFFFFF8u) p->right=0;
    p->color=1; p->isNil=0;
    s.head=p; s.size=0; p->isNil=1; p->parent=p; p->left=p; p->right=p;
}
std::map<unsigned int,dx9rend::Ra::RaShader*> g_techniques;
struct QualityMasks
{
    vc71::vector_layout values;
    QualityMasks() { initialize(values); }
    ~QualityMasks() { dispose(values); } //free first, clear all three pointers
} g_qualityMaskOwner;
vc71::vector_layout& g_qualityMasks=g_qualityMaskOwner.values;
const char* const kBits[]={"nothing","hasuvanimation","hasnormalmap","hasgimap",
    "hasenvmap","hascockpit","usehemimap","hasshadow","hascolormapgloss",
    "hasstaticgloss","hasdot3alphatest","pointlight","hasshadowocclusion",
    "hashemiocclusion",0};
template<class T> T& materialField(BundledMeshMaterial* m,unsigned int offset)
{ return *reinterpret_cast<T*>(reinterpret_cast<char*>(m)+offset); }
void assignTexture(BundledMeshMaterial* m,unsigned int offset,dx9rend::EngineTexture* texture)
{
    dx9rend::EngineTexture*& slot=materialField<dx9rend::EngineTexture*>(m,offset);
    if(slot!=texture) { dx9rend::textureReleaseRef(slot); slot=texture; if(texture) dx9rend::textureAddRef(texture); }
}
}

//The constructor deliberately leaves allocator/comparator words,
//padding, the file-header fields and +1F4 untouched.
BundledMeshTemplate::BundledMeshTemplate() : MeshTemplate(),m_refCount(1)
{
    initialize(m_materials); constructSet(m_instances);
    bundledMeshMapConstruct(m_materialValues0E8); bundledMeshMapConstruct(m_map0F4);
    bundledMeshMapConstruct(m_map100); m_dword10C=0; initialize(m_geoms);
    m_maxPartCount=0; m_hasOpaqueMaterials=0; m_byte125=0;
    m_hasEnvironmentMap=0; m_compressVertexData=1; m_byte192=0; m_maxTextureRepeat=16;
    m_byte198=0; m_byte199=0; zeroBounds(m_bounds);
    m_compressScale=unitScale(); m_decompressScale=unitScale(); m_uvDecompress=1;
    m_liveInstanceCount=0; bundledMeshStateManagerRetain(); m_keepData=0;
    for(unsigned int i=0;i<26;++i) m_partBounds[i]=0;
}
//states belong to the template, material destruction does not release them.
void BundledMeshTemplate::destroyMembers()
{
    for(unsigned int g=0;g<count<int>(m_lodCounts);++g)
        for(int l=0;l<data<int>(m_lodCounts)[g];++l)
        {
            vc71::vector_layout& v=materials(this,g,l);
            for(unsigned int i=0;i<count<BundledMeshMaterial*>(v);++i)
            {
                BundledMeshMaterial* m=data<BundledMeshMaterial*>(v)[i];
                if(m->m_state) m->m_state->release();
                if(m) m->destroy(1);
            }
        }
    for(unsigned int i=0;i<26;++i) if(m_partBounds[i]) ::renddx9abi::engineFree(m_partBounds[i]);
    bundledMeshStateManagerRelease();
    for(unsigned int i=0;i<count<BundledMeshGeom>(m_geoms);++i)
    { dispose(data<BundledMeshGeom>(m_geoms)[i].lods); dispose(data<BundledMeshGeom>(m_geoms)[i].distances); }
    dispose(m_geoms); bundledMeshMapDestroy(m_map100); bundledMeshMapDestroy(m_map0F4);
    bundledMeshMapDestroy(m_materialValues0E8); m_instances.destroy();
    for(unsigned int g=0;g<count<vc71::vector_layout>(m_materials);++g)
    {
        vc71::vector_layout& v=data<vc71::vector_layout>(m_materials)[g];
        for(unsigned int l=0;l<count<vc71::vector_layout>(v);++l) dispose(data<vc71::vector_layout>(v)[l]);
        dispose(v);
    }
    dispose(m_materials); MeshTemplate::destroyMembers();
}
void* BundledMeshTemplate::destroy(unsigned char flags)
{ destroyMembers(); if(flags&1) ::renddx9abi::engineFree(this); return this; }
void BundledMeshTemplate::addRef() { ++m_refCount; }
int BundledMeshTemplate::getRef() { return m_refCount; }
int BundledMeshTemplate::release()
{ int n=--m_refCount; if(!n) destroy(1); return n; }
unsigned int BundledMeshTemplate::getClassId() const { return 0x870CA651; }
void BundledMeshTemplate::setName(const vc71::string& n) { m_name=n; }
IBase* __fastcall createBundledMeshTemplate(unsigned int iid,IBase* outer)
{
    if(outer) return 0;
    void* p=::renddx9abi::engineAlloc(0x1FC);
    BundledMeshTemplate* t=p?new(p) BundledMeshTemplate:0;
    IBase* result=static_cast<IBase*>(t->queryInterface(iid));
    if(!result) t->release();
    return result;
}
void* BundledMeshTemplate::createInstance()
{
    void* storage=::renddx9abi::engineAlloc(0x2F8);
    void* instance=storage?bundledMeshConstruct(storage,this):0;
    if(m_loaded || (m_instances.insert(reinterpret_cast<unsigned int>(instance)),m_loaded)
       || dx9rend::resourceManagerIsReady() || load())
    { ++m_liveInstanceCount; m_keepData=1; return instance; }
    vc71::setEraseValue(m_instances,instance);
    if(instance) bundledMeshDestroyFlags(instance,1);
    return 0;
}
bool BundledMeshTemplate::load()
{
    if(m_loaded) return false;
    vc71::string a(m_path); a+="Meshes/"; a+=m_name; a+=".bundledMesh";
    vc71::string b(m_path); b+="Meshes/"; b+=m_name; b+=".bundledMesh";
    renddx9abi::HostFileManager* files=::dice::hfe::fileManager;
    if(!files->fileExists(renddx9abi::makeHostString(a)))
    { renddx9abi::logf("BundledMeshTemplate.cpp:269: Cannot load BundledMesh %s Please make sure it exists!",a.c_str()); return false; }
    const vc71::string& path=files->fileExists(renddx9abi::makeHostString(b))?b:a;
    renddx9abi::HostStream* stream=files->open(renddx9abi::makeHostString(path),
        static_cast<renddx9abi::HostFileMode>(2),static_cast<renddx9abi::HostFileAccess>(0),0);
    if(!stream) return false;
    bool ok=loadFromStream(static_cast<renddx9abi::HostNonlinearStream*>(stream));
    stream->release(); if(!ok) return false;
    m_loaded=1;
    for(vc71::set_node* n=m_instances.head->left;n!=m_instances.head;n=vc71::set_layout::next(n))
        bundledMeshInitTemplate(reinterpret_cast<void*>(n->key));
    return true;
}

void BundledMeshTemplate::setLodDistance(int g,int l,float distance)
{
    if(g<0||l<0||distance<0) { renddx9abi::logf("BundledMeshTemplate.cpp:312: Negative subgeometry, lod or distance"); return; }
    if(count<BundledMeshGeom>(m_geoms))
    {
        if(static_cast<unsigned int>(g)>=count<BundledMeshGeom>(m_geoms))
        { renddx9abi::logf("BundledMeshTemplate.cpp:320: Invalid Subgeometry"); return; }
        vc71::vector_layout& distances=data<BundledMeshGeom>(m_geoms)[g].distances;
        int skip=data<int>(m_geomInts)[g];
        if(l>=skip+static_cast<int>(count<float>(distances)))
        { renddx9abi::logf("BundledMeshTemplate.cpp:328: Invalid lodSwitch"); return; }
        if(l>=skip) data<float>(distances)[l-skip]=distance;
    }
    else
    {
        if(count<vc71::vector_layout>(m_lodDistances)<=static_cast<unsigned int>(g))
            resize(m_lodDistances,g+1,emptyVector());
        vc71::vector_layout& v=data<vc71::vector_layout>(m_lodDistances)[g];
        if(count<float>(v)<=static_cast<unsigned int>(l)) resize(v,l+1,0.0f);
        data<float>(v)[l]=distance;
    }
}
float BundledMeshTemplate::getLodDistance(int g,int l)
{ return count<BundledMeshGeom>(m_geoms)?data<float>(data<BundledMeshGeom>(m_geoms)[g].distances)[l]:-1.0f; }
void BundledMeshTemplate::slot_0x40(int g,int l,int m,int bits)
{ bundledMeshMapAt(m_materialValues0E8,packedKey(g,l,static_cast<unsigned char>(m)))=bits; }
float BundledMeshTemplate::slot_0x44(int g,int l,int m)
{
    BundledMeshMapNode* n=bundledMeshMapFind(m_materialValues0E8,packedKey(g,l,static_cast<unsigned char>(m)));
    return n==reinterpret_cast<BundledMeshMapNode*>(m_materialValues0E8.head)?0.25f:asFloat(n->value);
}
void BundledMeshTemplate::setSpecularStaticGloss(int g,int l,unsigned char m,float v)
{ bundledMeshMapAt(m_map0F4,packedKey(g,l,m))=asBits(v); }
float BundledMeshTemplate::getSpecularStaticGloss(int g,int l,unsigned char m)
{
    BundledMeshMapNode* n=bundledMeshMapFind(m_map0F4,packedKey(g,l,m));
    return n==reinterpret_cast<BundledMeshMapNode*>(m_map0F4.head)?0.15000001f:asFloat(n->value);
}
void BundledMeshTemplate::setMaterialValue100(int g,int l,unsigned char m,int v)
{ bundledMeshMapAt(m_map100,packedKey(g,l,m))=v; }
int BundledMeshTemplate::getMaterialValue100(int g,int l,unsigned char m)
{
    BundledMeshMapNode* n=bundledMeshMapFind(m_map100,packedKey(g,l,m));
    return n==reinterpret_cast<BundledMeshMapNode*>(m_map100.head)?64:static_cast<int>(n->value);
}
void BundledMeshTemplate::slot_0x48(int v) { m_byte125=static_cast<unsigned char>(v); }
bool BundledMeshTemplate::slot_0x4C() { return m_byte125!=0; }
void BundledMeshTemplate::setCompressVertexData(bool v) { m_compressVertexData=v; }
bool BundledMeshTemplate::getCompressVertexData() const { return m_compressVertexData!=0; }
void BundledMeshTemplate::slot_0x80(int v) { m_byte192=static_cast<unsigned char>(v); }
bool BundledMeshTemplate::slot_0x84() { return m_byte192!=0; }
void BundledMeshTemplate::setMaxTextureRepeat(int v) { m_maxTextureRepeat=v; }
int BundledMeshTemplate::getMaxTextureRepeat() const { return m_maxTextureRepeat; }
void BundledMeshTemplate::slot_0x90(int v) { m_byte198=static_cast<unsigned char>(v); }
bool BundledMeshTemplate::slot_0x94() { return m_byte198!=0; }
void BundledMeshTemplate::slot_0x98(int v) { m_byte199=static_cast<unsigned char>(v); }
bool BundledMeshTemplate::slot_0x9C() { return m_byte199!=0; }
int BundledMeshTemplate::getPrimitiveCount(int g,int l,int m)
{
    vc71::vector_layout& v=materials(this,g,l);
    if(m!=-1) return data<BundledMeshMaterial*>(v)[m]->getPrimitiveCount();
    int result=0; for(unsigned int i=0;i<count<BundledMeshMaterial*>(v);++i) result+=data<BundledMeshMaterial*>(v)[i]->getPrimitiveCount();
    return result;
}
int BundledMeshTemplate::getVertexCount(int g,int l,int m)
{
    int result=0;
    if(g==-1&&l==-1)
    {
        for(unsigned int i=0;i<count<int>(m_lodCounts);++i)
            for(int j=0;j<data<int>(m_lodCounts)[i];++j) result+=getVertexCount(i,j,-1);
        return result;
    }
    vc71::vector_layout& v=materials(this,g,l);
    if(m!=-1) return data<BundledMeshMaterial*>(v)[m]->getVertexCount();
    for(unsigned int i=0;i<count<BundledMeshMaterial*>(v);++i) result+=data<BundledMeshMaterial*>(v)[i]->getVertexCount();
    return result;
}
int BundledMeshTemplate::getMaterialCount(unsigned int l)
{
    int result=0; for(unsigned int g=0;g<count<BundledMeshGeom>(m_geoms);++g)
        if(l<count<BundledMeshLod>(data<BundledMeshGeom>(m_geoms)[g].lods)) result+=count<BundledMeshMaterial*>(materials(this,g,l));
    return result;
}
int BundledMeshTemplate::getLodPrimitiveCount(unsigned int l)
{
    int result=0; for(unsigned int g=0;g<count<BundledMeshGeom>(m_geoms);++g)
        if(l<count<BundledMeshLod>(data<BundledMeshGeom>(m_geoms)[g].lods))
        {
            vc71::vector_layout& v=materials(this,g,l);
            for(unsigned int i=0;i<count<BundledMeshMaterial*>(v);++i) result+=data<BundledMeshMaterial*>(v)[i]->getPrimitiveCount();
        }
    return result;
}
unsigned int BundledMeshTemplate::getMemoryUsage(int mask)
{
    if(mask==-1)
    { unsigned int result=getMemoryUsage(1); result+=getMemoryUsage(2); result+=getMemoryUsage(4); return result; }
    if((mask&8)&&m_indexBuffer) return m_indexCount/3;
    if((mask&1)&&m_vertexBuffer) return *reinterpret_cast<unsigned int*>(static_cast<char*>(m_vertexBuffer)+32);
    if((mask&2)&&m_indexBuffer) return 2*m_indexCount;
    if(mask&4)
    {
        std::set<dx9rend::EngineTexture*> textures;
        for(unsigned int g=0;g<count<BundledMeshGeom>(m_geoms);++g)
            for(unsigned int l=0;l<count<BundledMeshLod>(data<BundledMeshGeom>(m_geoms)[g].lods);++l)
            {
                vc71::vector_layout& v=materials(this,g,l);
                for(unsigned int m=0;m<count<BundledMeshMaterial*>(v);++m)
                {
                    vc71::vector_layout& t=data<BundledMeshMaterial*>(v)[m]->m_state->m_textures;
                    for(unsigned int i=0;i<count<dx9rend::EngineTexture*>(t);++i)
                        if(data<dx9rend::EngineTexture*>(t)[i]) textures.insert(data<dx9rend::EngineTexture*>(t)[i]);
                }
            }
        unsigned int result=0; for(auto t:textures) result+=t->totalBytes; return result;
    }
    return 0;
}
Vec3* BundledMeshTemplate::decompressPosition(Vec3* out,const Vec3* in)
{
    const float y=in->y*m_compressScale.y,z=in->z*m_compressScale.z;
    const float qx=static_cast<float>(static_cast<int>(std::floor(static_cast<double>(in->x)*m_compressScale.x+0.5)));
    const float qy=static_cast<float>(static_cast<int>(std::floor(static_cast<double>(y)+0.5)));
    const float qz=static_cast<float>(static_cast<int>(std::floor(static_cast<double>(z)+0.5)));
    out->x=qx*m_decompressScale.x; out->y=qy*m_decompressScale.y; out->z=qz*m_decompressScale.z;
    return out;
}
void BundledMeshTemplate::releaseDeviceResources() //actually preloads
{
    static std::vector<dx9rend::EngineTexture*> textures;
    textures.clear();
    if(m_vertexBuffer)
    { IDirect3DResource9* r=*reinterpret_cast<IDirect3DResource9**>(static_cast<char*>(m_vertexBuffer)+8); if(r) r->PreLoad(); }
    if(m_indexBuffer)
    { IDirect3DResource9* r=*reinterpret_cast<IDirect3DResource9**>(static_cast<char*>(m_indexBuffer)+8); if(r) r->PreLoad(); }
    if(count<vc71::vector_layout>(m_materials)>=2)
    {
        vc71::vector_layout& v=materials(this,1,0);
        for(unsigned int m=0;m<count<BundledMeshMaterial*>(v);++m)
        {
            vc71::vector_layout& t=data<BundledMeshMaterial*>(v)[m]->m_state->m_textures;
            for(unsigned int i=0;i<count<dx9rend::EngineTexture*>(t);++i)
            {
                dx9rend::EngineTexture* p=data<dx9rend::EngineTexture*>(t)[i];
                if(p&&std::find(textures.begin(),textures.end(),p)==textures.end()) textures.push_back(p);
            }
        }
        for(auto p:textures) if(p->baseTexture) p->baseTexture->PreLoad();
    }
    m_dword08=dx9rend::rendererFrameOrdinal();
}

//Skipped records still contribute bounds and maximum part count.
void BundledMeshTemplate::readLods(renddx9abi::HostNonlinearStream* stream)
{
    BundledMeshGeom emptyGeom; initialize(emptyGeom.lods); initialize(emptyGeom.distances);
    const unsigned int geoms=count<int>(m_lodCounts); resize(m_geoms,geoms,emptyGeom);
    for(unsigned int g=0;g<geoms;++g)
    {
        int maximum=0;
        if(geoms==3) { if(g==1) maximum=m_maxSkip3pLods; else if(g==2) maximum=m_maxSkipWreckLods; }
        else if(geoms==2&&g==1) maximum=m_maxSkip3pLods;
        int skip=std::min(maximum,rendererInt(2020));
        skip=std::min(skip,data<int>(m_lodCounts)[g]-1);
        data<int>(m_geomInts)[g]=skip;
        const int kept=(data<int>(m_lodCounts)[g]-=skip);
        BundledMeshGeom& geom=data<BundledMeshGeom>(m_geoms)[g];
        BundledMeshLod empty; empty.partCount=0; zeroBounds(empty.bounds); empty.renderKind=0; empty.hasAlphaTest=0;
        resize(geom.lods,kept,empty); resize(geom.distances,kept-1,0.0f);
        int remaining=skip;
        for(int l=0;l<kept;++l)
        {
            BundledMeshLod& lod=data<BundledMeshLod>(geom.lods)[l];
            if(m_head||m_version>=10) meshBoundsReadMinMax(lod.bounds,stream);
            else meshBoundsReadLegacy(lod.bounds,stream);
            lod.bounds.dirty=1; meshBoundsExpand(m_bounds,lod.bounds.min); meshBoundsExpand(m_bounds,lod.bounds.max);
            stream->read(&lod.partCount,4); m_maxPartCount=std::max(m_maxPartCount,lod.partCount);
            if(remaining>0) { --remaining; --l; }
        }
        float running=static_cast<float>(skip+1)*50.0f;
        for(int l=0;l<kept-1;++l)
        {
            if(count<vc71::vector_layout>(m_lodDistances)>g
               &&count<float>(data<vc71::vector_layout>(m_lodDistances)[g])>static_cast<unsigned int>(l))
            {
                float* pending=data<float>(data<vc71::vector_layout>(m_lodDistances)[g]);
                data<float>(geom.distances)[l]=pending[l+skip]; running+=pending[l];
            }
            else { data<float>(geom.distances)[l]=running; running+=50.0f; }
        }
    }
}

//Part number is the low bone-index byte; retail does not bound it.
void BundledMeshTemplate::readVertices(renddx9abi::HostNonlinearStream* stream)
{
    m_vertexData=renddx9abi::engineAlloc(m_vertexCount*m_vertexStride);
    stream->read(m_vertexData,m_vertexCount*m_vertexStride);
    if(dx9rend::readBoolSetting("GSCreateSubPartBoundingBoxes",false))
        for(unsigned int i=0;i<m_vertexCount;++i)
        {
            char* p=static_cast<char*>(m_vertexData)+i*m_vertexStride;
            const Vec3& point=*reinterpret_cast<const Vec3*>(p);
            MeshBounds*& bound=m_partBounds[static_cast<unsigned char>(p[24])];
            if(bound)
            {
                // This inlined arm does not dirty the already-dirty bounds.
                if(point.x<=bound->max.x) { if(point.x<bound->min.x) bound->min.x=point.x; } else bound->max.x=point.x;
                if(point.y<=bound->max.y) { if(point.y<bound->min.y) bound->min.y=point.y; } else bound->max.y=point.y;
                if(point.z<=bound->max.z) { if(point.z<bound->min.z) bound->min.z=point.z; } else bound->max.z=point.z;
            }
            else
            {
                bound=static_cast<MeshBounds*>(renddx9abi::engineAlloc(60));
                if(bound) { bound->min=point; bound->max=point; bound->extent=Vec3{0,0,0}; bound->center=Vec3{0,0,0}; bound->dirty=1; }
            }
        }
}
void BundledMeshTemplate::mergeMaterials() {} //shipped ret

//The declaration rewrite uses packed8 normals even when +192 selects
//packed16 output; this shipped inconsistency is retained. Tangents are written
//unconditionally (the presence flag only controls the stride subtraction).
void BundledMeshTemplate::buildVertexBuffers()
{
    const bool compressed=getCompressVertexData(); bool hasUv1=false,hasTangent=false;
    if(compressed)
    {
        UINT n=16;
        D3DVERTEXELEMENT9* elements=static_cast<D3DVERTEXELEMENT9*>(renddx9abi::engineAlloc(128));
        std::memset(elements,0,128);
        (*reinterpret_cast<IDirect3DVertexDeclaration9**>(static_cast<char*>(m_vertexDeclaration)+8))->GetDeclaration(elements,&n);
        const unsigned char normalType=(*reinterpret_cast<unsigned int*>(static_cast<char*>(dx9rend::g_deviceCaps)+0xEC)&0x80)?14:4;
        unsigned int removed=0;
        for(unsigned int i=0;i<n&&elements[i].Stream!=255;++i)
        {
            D3DVERTEXELEMENT9& e=elements[i]; e.Offset=static_cast<WORD>(e.Offset-removed);
            switch(e.Usage)
            {
                case D3DDECLUSAGE_POSITION: e.Type=7; removed+=4; break;
                case D3DDECLUSAGE_NORMAL: e.Type=normalType; removed+=8; break;
                case D3DDECLUSAGE_TEXCOORD: if(e.UsageIndex==1) hasUv1=true; e.Type=6; removed+=4; break;
                case D3DDECLUSAGE_TANGENT: hasTangent=true; e.Type=normalType; removed+=8; break;
            }
        }
        void* declaration=dx9rend::vertexDeclarationCreate(elements);
        dx9rend::vertexDeclarationRelease(m_vertexDeclaration); m_vertexDeclaration=declaration;
        renddx9abi::engineFree(elements);
    }
    const float uvScale=static_cast<float>(32766.0/static_cast<double>(getMaxTextureRepeat()));
    if(compressed)
    {
        float x=std::fabs(m_bounds.max.x),y=std::fabs(m_bounds.max.y),z=std::fabs(m_bounds.max.z);
        const float minX=std::fabs(m_bounds.min.x),minY=std::fabs(m_bounds.min.y),minZ=std::fabs(m_bounds.min.z);
        if(x<minX) x=minX; if(!(y>=minY)) y=minY; if(!(z>=minZ)) z=minZ;
        const float epsilon=1.1920928955078125e-7f;
        if(!(x>=epsilon)) x=epsilon; if(!(y>=epsilon)) y=epsilon; if(!(z>=epsilon)) z=epsilon;
        const Vec3& one=unitScale();
        const float inverseY=one.y/y,inverseZ=one.z/z;
        m_compressScale.x=static_cast<float>((static_cast<double>(one.x)/x)*32766.0);
        m_compressScale.y=inverseY*32766.0f; m_compressScale.z=inverseZ*32766.0f;
        const float inverseQuantization=3.051944077014923e-5f;
        m_decompressScale=Vec3{x*inverseQuantization,y*inverseQuantization,z*inverseQuantization};
        m_uvDecompress=1.0f/uvScale;
    }
    else { m_compressScale=unitScale(); m_decompressScale=unitScale(); m_uvDecompress=1; }
    const unsigned int oldStride=m_vertexStride;
    if(compressed)
    {
        const unsigned int normalRemoved=4*(m_byte192==0)+4;
        m_vertexStride-=normalRemoved+8; if(hasUv1) m_vertexStride-=4; if(hasTangent) m_vertexStride-=normalRemoved;
    }
    vc71::string name("BundledMeshTemplate : "); name+=m_name;
    m_vertexBuffer=dx9rend::createVertexBuffer(dx9rend::g_bufferManager,m_vertexCount,m_vertexStride,name,8);
    unsigned short* target=static_cast<unsigned short*>(dx9rend::vertexBufferLock(m_vertexBuffer,0,m_vertexCount,0));
    if(target)
    {
        if(compressed)
        {
            const unsigned int tangentOffset=hasUv1?44:36;
            for(unsigned int i=0;i<m_vertexCount;++i)
            {
                const char* source=static_cast<const char*>(m_vertexData)+i*oldStride;
                const float* f=reinterpret_cast<const float*>(source);
                const double px=static_cast<double>(m_compressScale.x)*f[0];
                const float py=f[1]*m_compressScale.y,pz=f[2]*m_compressScale.z;
                const float u0=uvScale*f[7],v0=uvScale*f[8],u1=uvScale*f[9],v1=uvScale*f[10];
                *target++=static_cast<unsigned short>(static_cast<__int64>(std::floor(px+0.5)));
                *target++=static_cast<unsigned short>(static_cast<__int64>(std::floor(static_cast<double>(py)+0.5)));
                *target++=static_cast<unsigned short>(static_cast<__int64>(std::floor(static_cast<double>(pz)+0.5))); *target++=1;
                if(m_byte192)
                {
                    for(unsigned int c=3;c<6;++c) *target++=static_cast<unsigned short>(static_cast<__int64>(static_cast<double>(f[c])*32767.0));
                    *target++=1;
                }
                else { *reinterpret_cast<unsigned int*>(target)=dx9rend::Ra::packVertexNormal(f+3); target+=2; }
                *reinterpret_cast<unsigned int*>(target)=*reinterpret_cast<const unsigned int*>(source+24); target+=2;
                *target++=static_cast<unsigned short>(static_cast<__int64>(u0)); *target++=static_cast<unsigned short>(static_cast<__int64>(v0));
                if(hasUv1) { *target++=static_cast<unsigned short>(static_cast<__int64>(u1)); *target++=static_cast<unsigned short>(static_cast<__int64>(v1)); }
                const float* tangent=reinterpret_cast<const float*>(source+tangentOffset);
                if(m_byte192)
                {
                    for(unsigned int c=0;c<3;++c) *target++=static_cast<unsigned short>(static_cast<__int64>(static_cast<double>(tangent[c])*32767.0));
                    *target++=1;
                }
                else { *reinterpret_cast<unsigned int*>(target)=dx9rend::Ra::packVertexNormal(tangent); target+=2; }
            }
        }
        else std::memcpy(target,m_vertexData,m_vertexCount*m_vertexStride);
        dx9rend::vertexBufferUnlock(m_vertexBuffer);
    }
    renddx9abi::engineFree(m_vertexData); //retail leaves the staging pointer dangling
}

bool BundledMeshTemplate::makeScript(::dice::hfe::io::IStream* stream) //100E8590 secondary-this
{
    MeshTemplate::makeScript(stream); //base write result deliberately discarded
    std::ostringstream out;
    for(unsigned int g=0;g<count<BundledMeshGeom>(m_geoms);++g)
    {
        vc71::vector_layout& v=data<BundledMeshGeom>(m_geoms)[g].distances;
        for(unsigned int l=0;l<count<float>(v);++l)
            if(data<float>(v)[l]!=static_cast<float>(50*(l+1)))
                out<<"GeometryTemplate.setSubGeometryLodDistance "<<g<<" "<<l<<" "<<data<float>(v)[l]<<std::endl;
    }
    vc71::map_layout* maps[]={&m_materialValues0E8,&m_map0F4};
    const char* commands[]={"GeometryTemplate.setMaterialReflectionScale ","GeometryTemplate.setSpecularStaticGloss "};
    for(unsigned int i=0;i<2;++i)
    {
        BundledMeshMapNode* head=reinterpret_cast<BundledMeshMapNode*>(maps[i]->head);
        for(BundledMeshMapNode* n=head->left;n!=head;n=bundledMeshMapNext(n))
            out<<commands[i]<<static_cast<unsigned int>(static_cast<int>(n->key)>>16)<<" "
               <<((n->key>>8)&255)<<" "<<(n->key&255)<<" "<<asFloat(n->value)<<std::endl;
    }
    out<<"GeometryTemplate.compressVertexData "<<(m_compressVertexData!=0)<<std::endl;
    out<<"GeometryTemplate.maxTextureRepeat "<<m_maxTextureRepeat<<std::endl;
    if(m_byte198) out<<"GeometryTemplate.useRadiusForShadowDepth "<<(m_byte198!=0)<<std::endl;
    if(m_byte199) out<<"GeometryTemplate.useRadiusForShadowSpatial "<<(m_byte199!=0)<<std::endl;
    const std::string text=out.str();
    return stream->write(text.data(),static_cast<unsigned int>(text.size()))==text.size();
}

//Skipped material allocations are overwritten by the next lod, just as
//shipped; only their state reference is released. No read-result guards exist.
void BundledMeshTemplate::readMaterials(renddx9abi::HostNonlinearStream* stream)
{
    const int oldMip=dx9rend::textureManagerSwapDefaultMipClass(11);
    bundledMeshInitTechniques();
    const unsigned int geoms=count<int>(m_lodCounts); resize(m_materials,geoms,emptyVector());
    D3DVERTEXELEMENT9* elements=static_cast<D3DVERTEXELEMENT9*>(renddx9abi::engineAlloc(128));
    std::memset(elements,0,128); UINT n=16;
    (*reinterpret_cast<IDirect3DVertexDeclaration9**>(static_cast<char*>(m_vertexDeclaration)+8))->GetDeclaration(elements,&n);
    bool hasUv1=false,hasTangent=false;
    for(unsigned int i=0;i<n;++i)
    {
        D3DVERTEXELEMENT9* e=elements+i;
        if(e->Usage==D3DDECLUSAGE_TEXCOORD&&e->UsageIndex==1) hasUv1=true;
        if(e->Usage==D3DDECLUSAGE_TANGENT) hasTangent=true;
        if(e->Stream==255) break;
    }
    renddx9abi::engineFree(elements);
    unsigned int removedVertices=0,removedIndices=0;
    for(unsigned int g=0;g<geoms;++g)
    {
        const int lodCount=data<int>(m_lodCounts)[g]; int skip=data<int>(m_geomInts)[g];
        resize(data<vc71::vector_layout>(m_materials)[g],lodCount,emptyVector());
        for(int l=0;l<lodCount;++l)
        {
            vc71::vector_layout& v=materials(this,g,l); unsigned int materialCount;
            stream->read(&materialCount,4); resize(v,materialCount,static_cast<BundledMeshMaterial*>(0));
            BundledMeshLod& lod=data<BundledMeshLod>(data<BundledMeshGeom>(m_geoms)[g].lods)[l];
            lod.renderKind=3; lod.hasAlphaTest=0;
            for(unsigned int i=0;i<materialCount;++i)
            {
                void* storage=renddx9abi::engineAlloc(0x1A0);
                BundledMeshMaterial* m=storage?new(storage) BundledMeshMaterial(this,hasUv1,hasTangent):0;
                data<BundledMeshMaterial*>(v)[i]=m;
                if(m_version<5) m->m_kind=i==1?1:0; else stream->read(&m->m_kind,4);
                if(m->m_kind==1) lod.renderKind=2;
                else { m_hasOpaqueMaterials=1; if(m->m_kind==2) lod.hasAlphaTest=1; }
                unsigned int length; stream->read(&length,4); stream->skip(length);
                m->m_state=g_bundledMeshStateManager->read(g_bundledShadowEffect,m_path,stream,false,skip>0);
                if(m->m_state->m_alphaOne) m_hasEnvironmentMap=1; //field name inferred; exact state+3B
                unsigned int indices;
                stream->read(&m->m_vertexStart,4); stream->read(&m->m_indexStart,4); stream->read(&indices,4);
                if(m_primitiveType==4) m->m_primitiveCount=indices/3;
                else if(m_primitiveType==5) m->m_primitiveCount=indices-2;
                if(m->m_kind==1) indices*=m_u2;
                stream->read(&m->m_vertexCount,4); stream->skip(4); stream->skip(4);
                if(skip>0)
                { if(m->m_state) m->m_state->release(); removedVertices+=m->m_vertexCount; removedIndices+=indices; }
                else
                {
                    if(removedIndices)
                    { unsigned int old=m->m_indexStart; m->m_indexStart-=removedIndices; std::memmove(m_indexData+m->m_indexStart,m_indexData+old,indices*2); }
                    if(removedVertices)
                    { unsigned int old=m->m_vertexStart; m->m_vertexStart-=removedVertices; std::memmove(static_cast<char*>(m_vertexData)+m->m_vertexStart*m_vertexStride,static_cast<char*>(m_vertexData)+old*m_vertexStride,m->m_vertexCount*m_vertexStride); }
                }
            }
            if(skip>0) { --skip; --l; }
        }
    }
    m_indexCount-=removedIndices; m_vertexCount-=removedVertices;
    dx9rend::textureManagerSwapDefaultMipClass(oldMip);
}

//Parameters are the shared 0x150-byte Ra material block at material+10.
void BundledMeshTemplate::finalizeMaterials()
{
    for(unsigned int g=0;g<count<int>(m_lodCounts);++g)
        for(int l=0;l<data<int>(m_lodCounts)[g];++l)
        {
            vc71::vector_layout& v=materials(this,g,l);
            for(unsigned int i=0;i<count<BundledMeshMaterial*>(v);++i)
            {
                BundledMeshMaterial* m=data<BundledMeshMaterial*>(v)[i];
                m->m_vertexBuffer=*reinterpret_cast<void**>(static_cast<char*>(m_vertexBuffer)+8);
                m->m_indexBuffer=*reinterpret_cast<void**>(static_cast<char*>(m_indexBuffer)+8);
                m->m_vertexStride=*reinterpret_cast<unsigned int*>(static_cast<char*>(m_vertexBuffer)+40);
                m->m_primitiveType=m_primitiveType;
                if(m->m_kind==2) { materialField<unsigned char>(m,0x2C)=1; materialField<int>(m,0xB4)=m->m_state->m_alphaTest?15:127; }
                if(m->m_kind==1)
                { materialField<unsigned char>(m,0x2C)=1; materialField<int>(m,0xB4)=0; materialField<unsigned char>(m,0x1C)=1; materialField<int>(m,0xAC)=1; materialField<unsigned char>(m,0x24)=0; }
                vc71::vector_layout& textures=m->m_state->m_textures;
                assignTexture(m,0xCC,data<dx9rend::EngineTexture*>(textures)[0]);
                if(m->m_state->m_normalMap) assignTexture(m,0xEC,data<dx9rend::EngineTexture*>(textures)[1]);
                if(m->m_state->m_extraMap) assignTexture(m,0xFC,data<dx9rend::EngineTexture*>(textures)[count<dx9rend::EngineTexture*>(textures)-1]);
                materialField<float>(m,0x138)=m_decompressScale.x; materialField<float>(m,0x13C)=m_decompressScale.y;
                materialField<float>(m,0x140)=m_decompressScale.z; materialField<float>(m,0x14C)=m_uvDecompress;
                if(getCompressVertexData())
                {
                    if(slot_0x84()) { materialField<float>(m,0x154)=dx9rend::g_materialCapC; materialField<float>(m,0x158)=dx9rend::g_materialCapD; }
                    else { materialField<float>(m,0x154)=dx9rend::g_materialCapScale; materialField<float>(m,0x158)=dx9rend::g_materialCapBias; }
                }
                else { materialField<float>(m,0x154)=1; materialField<float>(m,0x158)=0; }
            }
        }
}

void bundledMeshInitTechniques()
{
    if(!g_bundledMeshTechniquesPending) return;
    if(!meshBuildTechniqueMasks(vc71::string("shaders/RaShaderBM.mfx"),vc71::string("BM"),kBits,g_techniques,g_qualityMasks)) return;
    dx9rend::Ra::RaShaderTable* table=static_cast<dx9rend::Ra::RaShaderTable*>(dx9rend::g_renderQueues);
    g_techniques.insert(std::make_pair(0xF8000000u,dx9rend::Ra::getShader(table,vc71::string("Default"),false)));
    g_techniques.insert(std::make_pair(0xF6000000u,dx9rend::Ra::getShader(table,vc71::string("BMZOnly"),false)));
    g_bundledMeshTechniquesPending=0;
}
dx9rend::Ra::RaShader* bundledMeshShaderForBits(unsigned int bits)
{
    auto i=g_techniques.find(bits); if(i!=g_techniques.end()) return i->second;
    vc71::string names;
    for(unsigned int b=0;b<32;++b) if(bits&(1u<<b)) names+=kBits[b]; //no range guard in retail
    renddx9abi::logf("BundledMeshTemplate.cpp:1115: Couldn't find shader bits: %u%s",bits,names.c_str());
    i=g_techniques.find(0xF8000000u); if(i!=g_techniques.end()) return i->second;
    renddx9abi::logf("BundledMeshTemplate.cpp:1123: Couldn't find shader default"); return 0;
}
unsigned int bundledMeshQualityMask(unsigned int index) { return data<unsigned int>(g_qualityMasks)[index]; }
unsigned int bundledMeshQualityMask()
{
    return bundledMeshQualityMask(static_cast<unsigned int>(
        dx9rend::rendererGetShaderPath(dx9rend::g_Renderer)));
}

} } }
#endif
