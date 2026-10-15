// attested: Geom/BundledMeshTemplate.cpp, RendDX9.dll1.50 string1021A3C8.
// PE-only renderer recovery. Addresses identify the instruction-read witnesses.
// The template's containers are std containers (issue #7); retail layouts are in the header comments.
#ifdef BF2_IMAGE_RENDDX9
#define NOMINMAX
#include "BundledMeshTemplate.h"
#include "BundledMesh.h"
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
void* createVertexBuffer(void*,unsigned int,unsigned int,const std::string&,unsigned int);
void* vertexBufferLock(void*,unsigned int,unsigned int,unsigned int);
bool vertexBufferUnlock(void*);
// BufferManager.cpp - the wrapper fields (+0x08 D3D object, +0x20 desc.Size, +0x28 stride).
unsigned int vertexBufferStride(void* wrapper);
unsigned int vertexBufferByteSize(void* wrapper);
IDirect3DVertexBuffer9* vertexBufferD3D(void* wrapper);
IDirect3DIndexBuffer9* indexBufferD3D(void* wrapper);
IDirect3DVertexDeclaration9* vertexDeclarationD3D(void* wrapper);
unsigned int rendererFrameOrdinal();
}
namespace geom {
extern unsigned char g_bundledMeshTechniquesPending;
extern dx9rend::Effect* g_bundledShadowEffect;
bool meshBuildTechniqueMasks(const std::string&,const std::string&,const char* const*,
    std::map<unsigned int,dx9rend::Ra::RaShader*>&,std::vector<unsigned int>&);

namespace {
void zeroBounds(MeshBounds& b)
{
    b.min=Vec3{0,0,0}; b.max=Vec3{0,0,0}; b.originRadius=b.radius=0;
    b.extent=Vec3{0,0,0}; b.center=Vec3{0,0,0}; b.dirty=1;
}
const Vec3& unitScale() { static const Vec3 v={1,1,1}; return v; }
unsigned int packedKey(int g,int l,unsigned char m)
{ return (static_cast<unsigned int>(static_cast<unsigned char>(g))<<16)
       | (static_cast<unsigned int>(static_cast<unsigned char>(l))<<8) | m; }
float asFloat(unsigned int v) { float f; std::memcpy(&f,&v,4); return f; }
unsigned int asBits(float v) { unsigned int i; std::memcpy(&i,&v,4); return i; }
BundledMeshTemplate::MaterialList& materials(BundledMeshTemplate* t,int g,int l)
{ return t->m_materials[g][l]; }

std::map<unsigned int,dx9rend::Ra::RaShader*> g_techniques;
std::vector<unsigned int> g_qualityMasks; //static teardown frees it at exit
const char* const kBits[]={"nothing","hasuvanimation","hasnormalmap","hasgimap",
    "hasenvmap","hascockpit","usehemimap","hasshadow","hascolormapgloss",
    "hasstaticgloss","hasdot3alphatest","pointlight","hasshadowocclusion",
    "hashemiocclusion",0};
// One texture holder's value in the material's Ra parameter block (material +0x10).
void assignTexture(RaMaterialParameters::Texture& holder,dx9rend::EngineTexture* texture)
{
    dx9rend::EngineTexture*& slot=reinterpret_cast<dx9rend::EngineTexture*&>(holder.value);
    if(slot!=texture) { dx9rend::textureReleaseRef(slot); slot=texture; if(texture) dx9rend::textureAddRef(texture); }
}
}

//The constructor deliberately leaves allocator/comparator words,
//padding, the file-header fields and +1F4 untouched.
// The material tree, the instance set (head node 100EB000), the three value maps (head nodes
// 0x100EB100) and the geom vector are default-constructed empty.
BundledMeshTemplate::BundledMeshTemplate() : MeshTemplate(),m_refCount(1)
{
    m_dword10C=0;
    m_maxPartCount=0; m_hasOpaqueMaterials=0; m_byte125=0;
    m_hasEnvironmentMap=0; m_compressVertexData=1; m_byte192=0; m_maxTextureRepeat=16;
    m_byte198=0; m_byte199=0; zeroBounds(m_bounds);
    m_compressScale=unitScale(); m_decompressScale=unitScale(); m_uvDecompress=1;
    m_liveInstanceCount=0; bundledMeshStateManagerRetain(); m_keepData=0;
    for(unsigned int i=0;i<26;++i) m_partBounds[i]=0;
}
//states belong to the template, material destruction does not release them.
//The storage is freed afterwards without a C++ destructor, so the std members are destroyed
//explicitly, in the shipped order.
void BundledMeshTemplate::destroyMembers()
{
    for(unsigned int g=0;g<m_lodCounts.size();++g)
        for(int l=0;l<m_lodCounts[g];++l)
        {
            MaterialList& v=materials(this,g,l);
            for(unsigned int i=0;i<v.size();++i)
            {
                BundledMeshMaterial* m=v[i];
                if(m->m_state) m->m_state->release();
                if(m) m->destroy(1);
            }
        }
    for(unsigned int i=0;i<26;++i) if(m_partBounds[i]) ::renddx9abi::engineFree(m_partBounds[i]);
    bundledMeshStateManagerRelease();
    m_geoms.~GeomVector(); m_map100.~ValueMap(); m_map0F4.~ValueMap();
    m_materialValues0E8.~ValueMap(); m_instances.~InstanceSet();
    m_materials.~MaterialTree(); MeshTemplate::destroyMembers();
}
void* BundledMeshTemplate::destroy(unsigned char flags)
{ destroyMembers(); if(flags&1) ::renddx9abi::engineFree(this); return this; }
void BundledMeshTemplate::addRef() { ++m_refCount; }
int BundledMeshTemplate::getRef() { return m_refCount; }
int BundledMeshTemplate::release()
{ int n=--m_refCount; if(!n) destroy(1); return n; }
unsigned int BundledMeshTemplate::getClassId() const { return 0x870CA651; }
void BundledMeshTemplate::setName(const std::string& n) { m_name=n; }
IBase* __fastcall createBundledMeshTemplate(unsigned int iid,IBase* outer)
{
    if(outer) return 0;
    void* p=::renddx9abi::engineAlloc(sizeof(BundledMeshTemplate)); //retail 0x1FC
    BundledMeshTemplate* t=p?new(p) BundledMeshTemplate:0;
    IBase* result=static_cast<IBase*>(t->queryInterface(iid));
    if(!result) t->release();
    return result;
}
void* BundledMeshTemplate::createInstance()
{
    void* storage=::renddx9abi::engineAlloc(sizeof(BundledMesh)); //retail 0x2F8
    void* instance=storage?bundledMeshConstruct(storage,this):0;
    if(m_loaded || (m_instances.insert(static_cast<BundledMesh*>(instance)),m_loaded)
       || dx9rend::resourceManagerIsReady() || load())
    { ++m_liveInstanceCount; m_keepData=1; return instance; }
    m_instances.erase(static_cast<BundledMesh*>(instance));
    if(instance) bundledMeshDestroyFlags(instance,1);
    return 0;
}
bool BundledMeshTemplate::load()
{
    if(m_loaded) return false;
    std::string a(m_path); a+="Meshes/"; a+=m_name; a+=".bundledMesh";
    std::string b(m_path); b+="Meshes/"; b+=m_name; b+=".bundledMesh";
    renddx9abi::HostFileManager* files=::dice::hfe::fileManager;
    if(!files->fileExists(renddx9abi::makeHostString(a)))
    { renddx9abi::logf("BundledMeshTemplate.cpp:269: Cannot load BundledMesh %s Please make sure it exists!",a.c_str()); return false; }
    const std::string& path=files->fileExists(renddx9abi::makeHostString(b))?b:a;
    renddx9abi::HostStream* stream=files->open(renddx9abi::makeHostString(path),
        static_cast<renddx9abi::HostFileMode>(2),static_cast<renddx9abi::HostFileAccess>(0),0);
    if(!stream) return false;
    bool ok=loadFromStream(static_cast<renddx9abi::HostNonlinearStream*>(stream));
    stream->release(); if(!ok) return false;
    m_loaded=1;
    for(InstanceSet::const_iterator n=m_instances.begin();n!=m_instances.end();++n)
        bundledMeshInitTemplate(*n);
    return true;
}

void BundledMeshTemplate::setLodDistance(int g,int l,float distance)
{
    if(g<0||l<0||distance<0) { renddx9abi::logf("BundledMeshTemplate.cpp:312: Negative subgeometry, lod or distance"); return; }
    if(!m_geoms.empty())
    {
        if(static_cast<unsigned int>(g)>=m_geoms.size())
        { renddx9abi::logf("BundledMeshTemplate.cpp:320: Invalid Subgeometry"); return; }
        std::vector<float>& distances=m_geoms[g].distances;
        int skip=m_geomInts[g];
        if(l>=skip+static_cast<int>(distances.size()))
        { renddx9abi::logf("BundledMeshTemplate.cpp:328: Invalid lodSwitch"); return; }
        if(l>=skip) distances[l-skip]=distance;
    }
    else
    {
        if(m_lodDistances.size()<=static_cast<unsigned int>(g))
            m_lodDistances.resize(g+1);
        std::vector<float>& v=m_lodDistances[g];
        if(v.size()<=static_cast<unsigned int>(l)) v.resize(l+1,0.0f);
        v[l]=distance;
    }
}
float BundledMeshTemplate::getLodDistance(int g,int l)
{ return !m_geoms.empty()?m_geoms[g].distances[l]:-1.0f; }
void BundledMeshTemplate::slot_0x40(int g,int l,int m,int bits)
{ m_materialValues0E8[packedKey(g,l,static_cast<unsigned char>(m))]=bits; }
float BundledMeshTemplate::slot_0x44(int g,int l,int m)
{
    ValueMap::const_iterator n=m_materialValues0E8.find(packedKey(g,l,static_cast<unsigned char>(m)));
    return n==m_materialValues0E8.end()?0.25f:asFloat(n->second);
}
void BundledMeshTemplate::setSpecularStaticGloss(int g,int l,unsigned char m,float v)
{ m_map0F4[packedKey(g,l,m)]=asBits(v); }
float BundledMeshTemplate::getSpecularStaticGloss(int g,int l,unsigned char m)
{
    ValueMap::const_iterator n=m_map0F4.find(packedKey(g,l,m));
    return n==m_map0F4.end()?0.15000001f:asFloat(n->second);
}
void BundledMeshTemplate::setMaterialValue100(int g,int l,unsigned char m,int v)
{ m_map100[packedKey(g,l,m)]=v; }
int BundledMeshTemplate::getMaterialValue100(int g,int l,unsigned char m)
{
    ValueMap::const_iterator n=m_map100.find(packedKey(g,l,m));
    return n==m_map100.end()?64:static_cast<int>(n->second);
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
    MaterialList& v=materials(this,g,l);
    if(m!=-1) return v[m]->getPrimitiveCount();
    int result=0; for(unsigned int i=0;i<v.size();++i) result+=v[i]->getPrimitiveCount();
    return result;
}
int BundledMeshTemplate::getVertexCount(int g,int l,int m)
{
    int result=0;
    if(g==-1&&l==-1)
    {
        for(unsigned int i=0;i<m_lodCounts.size();++i)
            for(int j=0;j<m_lodCounts[i];++j) result+=getVertexCount(i,j,-1);
        return result;
    }
    MaterialList& v=materials(this,g,l);
    if(m!=-1) return v[m]->getVertexCount();
    for(unsigned int i=0;i<v.size();++i) result+=v[i]->getVertexCount();
    return result;
}
int BundledMeshTemplate::getMaterialCount(unsigned int l)
{
    int result=0; for(unsigned int g=0;g<m_geoms.size();++g)
        if(l<m_geoms[g].lods.size()) result+=static_cast<int>(materials(this,g,l).size());
    return result;
}
int BundledMeshTemplate::getLodPrimitiveCount(unsigned int l)
{
    int result=0; for(unsigned int g=0;g<m_geoms.size();++g)
        if(l<m_geoms[g].lods.size())
        {
            MaterialList& v=materials(this,g,l);
            for(unsigned int i=0;i<v.size();++i) result+=v[i]->getPrimitiveCount();
        }
    return result;
}
unsigned int BundledMeshTemplate::getMemoryUsage(int mask)
{
    if(mask==-1)
    { unsigned int result=getMemoryUsage(1); result+=getMemoryUsage(2); result+=getMemoryUsage(4); return result; }
    if((mask&8)&&m_indexBuffer) return m_indexCount/3;
    if((mask&1)&&m_vertexBuffer) return dx9rend::vertexBufferByteSize(m_vertexBuffer);   // wrapper +0x20
    if((mask&2)&&m_indexBuffer) return 2*m_indexCount;
    if(mask&4)
    {
        std::set<dx9rend::EngineTexture*> textures;
        for(unsigned int g=0;g<m_geoms.size();++g)
            for(unsigned int l=0;l<m_geoms[g].lods.size();++l)
            {
                MaterialList& v=materials(this,g,l);
                for(unsigned int m=0;m<v.size();++m)
                {
                    const BundledMeshState::TextureList& t=v[m]->m_state->m_textures;
                    for(unsigned int i=0;i<t.size();++i)
                        if(t[i]) textures.insert(t[i]);
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
    { IDirect3DResource9* r=dx9rend::vertexBufferD3D(m_vertexBuffer); if(r) r->PreLoad(); }   // wrapper +0x08
    if(m_indexBuffer)
    { IDirect3DResource9* r=dx9rend::indexBufferD3D(m_indexBuffer); if(r) r->PreLoad(); }     // wrapper +0x08
    if(m_materials.size()>=2)
    {
        MaterialList& v=materials(this,1,0);
        for(unsigned int m=0;m<v.size();++m)
        {
            const BundledMeshState::TextureList& t=v[m]->m_state->m_textures;
            for(unsigned int i=0;i<t.size();++i)
            {
                dx9rend::EngineTexture* p=t[i];
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
    const unsigned int geoms=static_cast<unsigned int>(m_lodCounts.size()); m_geoms.resize(geoms);
    for(unsigned int g=0;g<geoms;++g)
    {
        int maximum=0;
        if(geoms==3) { if(g==1) maximum=m_maxSkip3pLods; else if(g==2) maximum=m_maxSkipWreckLods; }
        else if(geoms==2&&g==1) maximum=m_maxSkip3pLods;
        int skip=std::min(maximum,static_cast<int>(dx9rend::rendererTailInt(0x7E4)));   // Renderer +0x7E4
        skip=std::min(skip,m_lodCounts[g]-1);
        m_geomInts[g]=skip;
        const int kept=(m_lodCounts[g]-=skip);
        BundledMeshGeom& geom=m_geoms[g];
        BundledMeshLod empty; empty.partCount=0; zeroBounds(empty.bounds); empty.renderKind=0; empty.hasAlphaTest=0;
        geom.lods.resize(static_cast<unsigned int>(kept),empty);
        geom.distances.resize(static_cast<unsigned int>(kept-1),0.0f);
        int remaining=skip;
        for(int l=0;l<kept;++l)
        {
            BundledMeshLod& lod=geom.lods[l];
            if(m_head||m_version>=10) meshBoundsReadMinMax(lod.bounds,stream);
            else meshBoundsReadLegacy(lod.bounds,stream);
            lod.bounds.dirty=1; meshBoundsExpand(m_bounds,lod.bounds.min); meshBoundsExpand(m_bounds,lod.bounds.max);
            stream->read(&lod.partCount,4); m_maxPartCount=std::max(m_maxPartCount,lod.partCount);
            if(remaining>0) { --remaining; --l; }
        }
        float running=static_cast<float>(skip+1)*50.0f;
        for(int l=0;l<kept-1;++l)
        {
            if(m_lodDistances.size()>g
               &&m_lodDistances[g].size()>static_cast<unsigned int>(l))
            {
                const float* pending=m_lodDistances[g].data();
                geom.distances[l]=pending[l+skip]; running+=pending[l];
            }
            else { geom.distances[l]=running; running+=50.0f; }
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
        dx9rend::vertexDeclarationD3D(m_vertexDeclaration)->GetDeclaration(elements,&n);   // wrapper +0x08
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
    std::string name("BundledMeshTemplate : "); name+=m_name;
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
    for(unsigned int g=0;g<m_geoms.size();++g)
    {
        const std::vector<float>& v=m_geoms[g].distances;
        for(unsigned int l=0;l<v.size();++l)
            if(v[l]!=static_cast<float>(50*(l+1)))
                out<<"GeometryTemplate.setSubGeometryLodDistance "<<g<<" "<<l<<" "<<v[l]<<std::endl;
    }
    const ValueMap* maps[]={&m_materialValues0E8,&m_map0F4};
    const char* commands[]={"GeometryTemplate.setMaterialReflectionScale ","GeometryTemplate.setSpecularStaticGloss "};
    for(unsigned int i=0;i<2;++i)
    {
        for(ValueMap::const_iterator n=maps[i]->begin();n!=maps[i]->end();++n)
            out<<commands[i]<<static_cast<unsigned int>(static_cast<int>(n->first)>>16)<<" "
               <<((n->first>>8)&255)<<" "<<(n->first&255)<<" "<<asFloat(n->second)<<std::endl;
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
    const unsigned int geoms=static_cast<unsigned int>(m_lodCounts.size()); m_materials.resize(geoms);
    D3DVERTEXELEMENT9* elements=static_cast<D3DVERTEXELEMENT9*>(renddx9abi::engineAlloc(128));
    std::memset(elements,0,128); UINT n=16;
    dx9rend::vertexDeclarationD3D(m_vertexDeclaration)->GetDeclaration(elements,&n);   // wrapper +0x08
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
        const int lodCount=m_lodCounts[g]; int skip=m_geomInts[g];
        m_materials[g].resize(static_cast<unsigned int>(lodCount));
        for(int l=0;l<lodCount;++l)
        {
            MaterialList& v=materials(this,g,l); unsigned int materialCount;
            stream->read(&materialCount,4); v.resize(materialCount,static_cast<BundledMeshMaterial*>(0));
            BundledMeshLod& lod=m_geoms[g].lods[l];
            lod.renderKind=3; lod.hasAlphaTest=0;
            for(unsigned int i=0;i<materialCount;++i)
            {
                void* storage=renddx9abi::engineAlloc(sizeof(BundledMeshMaterial)); //retail 0x1A0
                BundledMeshMaterial* m=storage?new(storage) BundledMeshMaterial(this,hasUv1,hasTangent):0;
                v[i]=m;
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
    for(unsigned int g=0;g<m_lodCounts.size();++g)
        for(int l=0;l<m_lodCounts[g];++l)
        {
            MaterialList& v=materials(this,g,l);
            for(unsigned int i=0;i<v.size();++i)
            {
                BundledMeshMaterial* m=v[i];
                m->m_vertexBuffer=dx9rend::vertexBufferD3D(m_vertexBuffer);   // wrapper +0x08
                m->m_indexBuffer=dx9rend::indexBufferD3D(m_indexBuffer);      // wrapper +0x08
                m->m_vertexStride=dx9rend::vertexBufferStride(m_vertexBuffer);   // wrapper +0x28
                m->m_primitiveType=m_primitiveType;
                // The block's holders; the `+0xNN` comments are the retail material offsets.
                RaMaterialParameters& p=bundledMeshMaterialParameters(m);
                if(m->m_kind==2) { p.alphaTest.value=1; p.alphaTestRef.value=m->m_state->m_alphaTest?15:127; }   // +0x2C, +0xB4
                if(m->m_kind==1)
                { p.alphaTest.value=1; p.alphaTestRef.value=0; p.alphaBlendEnable.value=1; p.cullMode.value=1; p.depthWrite.value=0; }   // +0x2C +0xB4 +0x1C +0xAC +0x24
                // Unguarded, as shipped.
                const BundledMeshState::TextureList& textures=m->m_state->m_textures;
                assignTexture(p.diffuseMap,textures.data()[0]);                                   // +0xCC
                if(m->m_state->m_normalMap) assignTexture(p.normalMap,textures.data()[1]);        // +0xEC
                if(m->m_state->m_extraMap) assignTexture(p.giMap,textures.data()[textures.size()-1]);   // +0xFC
                p.posUnpack.value[0]=m_decompressScale.x; p.posUnpack.value[1]=m_decompressScale.y;   // +0x138, +0x13C
                p.posUnpack.value[2]=m_decompressScale.z; p.texUnpack.value=m_uvDecompress;           // +0x140, +0x14C
                if(getCompressVertexData())
                {
                    if(slot_0x84()) { p.normalUnpack.value[0]=dx9rend::g_materialCapC; p.normalUnpack.value[1]=dx9rend::g_materialCapD; }   // +0x154, +0x158
                    else { p.normalUnpack.value[0]=dx9rend::g_materialCapScale; p.normalUnpack.value[1]=dx9rend::g_materialCapBias; }
                }
                else { p.normalUnpack.value[0]=1; p.normalUnpack.value[1]=0; }
            }
        }
}

void bundledMeshInitTechniques()
{
    if(!g_bundledMeshTechniquesPending) return;
    if(!meshBuildTechniqueMasks(std::string("shaders/RaShaderBM.mfx"),std::string("BM"),kBits,g_techniques,g_qualityMasks)) return;
    dx9rend::Ra::RaShaderTable* table=static_cast<dx9rend::Ra::RaShaderTable*>(dx9rend::g_renderQueues);
    g_techniques.insert(std::make_pair(0xF8000000u,dx9rend::Ra::getShader(table,std::string("Default"),false)));
    g_techniques.insert(std::make_pair(0xF6000000u,dx9rend::Ra::getShader(table,std::string("BMZOnly"),false)));
    g_bundledMeshTechniquesPending=0;
}
dx9rend::Ra::RaShader* bundledMeshShaderForBits(unsigned int bits)
{
    auto i=g_techniques.find(bits); if(i!=g_techniques.end()) return i->second;
    std::string names;
    for(unsigned int b=0;b<32;++b) if(bits&(1u<<b)) names+=kBits[b]; //no range guard in retail
    renddx9abi::logf("BundledMeshTemplate.cpp:1115: Couldn't find shader bits: %u%s",bits,names.c_str());
    i=g_techniques.find(0xF8000000u); if(i!=g_techniques.end()) return i->second;
    renddx9abi::logf("BundledMeshTemplate.cpp:1123: Couldn't find shader default"); return 0;
}
unsigned int bundledMeshQualityMask(unsigned int index) { return g_qualityMasks.data()[index]; }
unsigned int bundledMeshQualityMask() { return bundledMeshQualityMask(dx9rend::rendererRaPath()); }   // Renderer +0x6CC

} } }
#endif
