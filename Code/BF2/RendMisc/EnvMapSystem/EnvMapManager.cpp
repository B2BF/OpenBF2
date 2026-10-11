// Path attested: RendDX9.dll 1.50 string10222428, RendMisc/EnvMapSystem/EnvMapManager.cpp.
// Complete15-slot table102224FC plus the load/frame/lifetime collaborators used by BundledMesh.
// PE-only.
#ifdef BF2_IMAGE_RENDDX9
#include "EnvMapManager.h"
#include "EnvMapObject.h"
#include "../IRenderer.h"
#include "../IRenderView.h"
#include "../../RendDX9/Texture.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../Math/Mat4.h"
#include "../../../RendDX9Abi/EngineFile.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include <cmath>
#include <cstdio>
#include <map>
#include <new>
#include <sstream>
#include <string>

namespace dice { namespace hfe {
namespace dx9rend {
extern void* g_rendererBase;
// BF2Render.cpp - Renderer's cube-face render (0x10028B00), generateEnvMaps' only callee there.
bool rendererRenderEnvMapFace(unsigned int size,EngineTexture* cube,int face,const Vec3& position);
}
namespace Rend {
namespace {
void freeVector(vc71::vector_layout& value)
{
    if(value.first)::renddx9abi::engineFree(value.first);
    value.first=value.last=value.end=0;
}
// Container operations stay source-level; VC7.1 bridge allocation/growth is tracked by RW-74.
void clearStringMap(vc71::map_layout& map)
{ while(vc71::map_node* n=vc71::mapFirst(map))vc71::mapErase(map,n); }
void freePlainTree(vc71::map_node* n,vc71::map_node* head)
{
    if(n==head)return;
    freePlainTree(n->right,head);
    freePlainTree(n->left,head);
    ::renddx9abi::engineFree(n);
}
void clearPlainMap(vc71::map_layout& map)
{
    freePlainTree(map.head->parent,map.head);
    map.head->left=map.head->right=map.head->parent=map.head;
    map.size=0;
}
void releaseTexture(dx9rend::EngineTexture* texture)
{
    if(texture && --texture->refCount==0)
    { dx9rend::textureDtor(texture); ::renddx9abi::engineFree(texture); }
}
}

EnvMapEntry::EnvMapEntry() : flag(false),texture(0)
{ position.x=position.y=position.z=0; } // padding untouched
EnvMapManager::EnvMapManager() : m_refCount(1)
{
    m_objects.first=m_objects.last=m_objects.end=0;
    m_templates.construct(48);
    m_pendingRemoval.first=m_pendingRemoval.last=m_pendingRemoval.end=0;
    m_map34.construct(24); m_map40.construct(24); m_maps.construct(48);
    m_currentTexture=0; m_templateCount=0; m_enabled=true; m_perPixelEnabled=true;
}
EnvMapManager::~EnvMapManager()
{
    // 101C0770 preserves cleanup order; the object's no-dtor vtable stays installed.
    flushRemovals(); clearObjects(); clearMaps();
    clearStringMap(m_maps); ::renddx9abi::engineFree(m_maps.head); m_maps.head=0; m_maps.size=0;
    clearPlainMap(m_map40); ::renddx9abi::engineFree(m_map40.head); m_map40.head=0; m_map40.size=0;
    clearPlainMap(m_map34); ::renddx9abi::engineFree(m_map34.head); m_map34.head=0; m_map34.size=0;
    freeVector(m_pendingRemoval);
    clearStringMap(m_templates); ::renddx9abi::engineFree(m_templates.head); m_templates.head=0; m_templates.size=0;
    freeVector(m_objects);
}
void EnvMapManager::addRef() { ++m_refCount; }
int EnvMapManager::getRef() { return m_refCount; }
int EnvMapManager::release()
{
    const int result=--m_refCount;
    if(!result){ this->~EnvMapManager(); ::renddx9abi::engineFree(this); }
    return result;
}
void* EnvMapManager::queryInterface(unsigned int iid) const
{ return iid==1u || iid==820202u ? const_cast<EnvMapManager*>(this):0; }
void EnvMapManager::reset()
{ m_currentTexture=0; m_templateCount=0; m_perPixelEnabled=true; }
EnvMapObject* EnvMapManager::createObject(EnvMapObjectTemplate* objectTemplate)
{
    void* storage=::renddx9abi::engineAlloc(0x1C);
    EnvMapObject* object=storage ? new(storage) EnvMapObject(objectTemplate):0;
    vc71::vectorPushBackIn(m_objects,4,&object);
    return object;
}
void EnvMapManager::queueRemoval(EnvMapObject* object)
{ vc71::vectorPushBackIn(m_pendingRemoval,4,&object); }
EnvMapObjectTemplate* EnvMapManager::getTemplate(const vc71::string& name)
{
    if(vc71::map_node* n=vc71::mapFindInsensitive(m_templates,name))
        return static_cast<EnvMapObjectTemplate*>(vc71::mapNodeValue(n));
    if(m_templateCount>=256)
        ::renddx9abi::logf("RendMisc/EnvMapSystem/EnvMapManager.cpp:120 To many EnvMapManager templates");
    void* storage=::renddx9abi::engineAlloc(0x28);
    EnvMapObjectTemplate* value=storage ? new(storage) EnvMapObjectTemplate(name):0;
    vc71::mapNodeValueRef(vc71::mapInsertInsensitive(m_templates,48,name))=value;
    ++m_templateCount;
    return value;
}
void EnvMapManager::removeTemplate(EnvMapObjectTemplate*) {} // exactly RET4
void EnvMapManager::setEnabled(bool value) { m_enabled=value; }
bool EnvMapManager::getEnabled() const { return m_enabled; }
void EnvMapManager::setPerPixelEnabled(bool value) { m_perPixelEnabled=value; }
bool EnvMapManager::getPerPixelEnabled() const { return m_perPixelEnabled; }
void EnvMapManager::clear()
{
    m_currentTexture=0; m_templateCount=0;
    flushRemovals(); clearObjects(); clearMaps();
}
void EnvMapManager::flushRemovals()
{
    for(EnvMapObject** item=reinterpret_cast<EnvMapObject**>(m_pendingRemoval.first);
        item!=reinterpret_cast<EnvMapObject**>(m_pendingRemoval.last);++item)
    {
        EnvMapObject** begin=reinterpret_cast<EnvMapObject**>(m_objects.first);
        EnvMapObject** end=reinterpret_cast<EnvMapObject**>(m_objects.last);
        EnvMapObject** found=begin;
        while(found!=end && *found!=*item)++found;
        if(found!=end)
        {
            *found=*(end-1);
            if(begin && end!=begin)m_objects.last-=4;
            if(*item)delete *item;
        }
    }
    m_pendingRemoval.last=m_pendingRemoval.first;
}
void EnvMapManager::clearObjects()
{
    for(vc71::map_node* n=vc71::mapFirst(m_templates);n;n=vc71::mapNext(m_templates,n))
        if(void* value=vc71::mapNodeValue(n))delete static_cast<EnvMapObjectTemplate*>(value);
    for(void** item=reinterpret_cast<void**>(m_objects.first);item!=reinterpret_cast<void**>(m_objects.last);++item)
        if(*item)delete static_cast<EnvMapObject*>(*item);
    m_pendingRemoval.last=m_pendingRemoval.first;
    clearStringMap(m_templates);
    clearPlainMap(m_map34);
    freeVector(m_objects); m_templateCount=0;
}
void EnvMapManager::clearMaps()
{
    for(vc71::map_node* n=vc71::mapFirst(m_maps);n;n=vc71::mapNext(m_maps,n))
    {
        EnvMapEntry* value=static_cast<EnvMapEntry*>(vc71::mapNodeValue(n));
        releaseTexture(value->texture);
        // Separate cache eviction after our reference decrement, even if it deleted the wrapper.
        dx9rend::textureManagerEvictFromCache(value->texture);
        ::renddx9abi::engineFree(value);
    }
    clearStringMap(m_maps);
}
void EnvMapManager::removeEntry(EnvMapEntry* value)
{
    // owns neither the entry nor cache eviction; only texture reference and map node.
    if(!m_maps.size)return;
    for(vc71::map_node* n=vc71::mapFirst(m_maps);n;n=vc71::mapNext(m_maps,n))
        if(vc71::mapNodeValue(n)==value)
        { releaseTexture(value->texture); vc71::mapErase(m_maps,n); return; }
}
bool EnvMapManager::loadInfo(const vc71::string& path)
{
    ::renddx9abi::EngineStream file;
    if(!file.open(path))
    {
        ::renddx9abi::logf("RendMisc/EnvMapSystem/EnvMapManager.cpp:308 Couldn't open EMI file \"%s\"",path.c_str());
        return false;
    }
    // 100751A0 ->max of signed rfind results; normalise final separator to '/'.
    const std::string filename(path.c_str(),path.size());
    int separator=static_cast<int>(filename.rfind('\\'));
    const int slash=static_cast<int>(filename.rfind('/'));
    if(slash>separator)separator=slash;
    const std::string directory=separator==-1 ? std::string():filename.substr(0,separator)+"/";
    const int size=file.getSize();
    if(size>0)
    {
        char* bytes=static_cast<char*>(::renddx9abi::engineAlloc(size+2));
        if(file.read(bytes,size)!=static_cast<unsigned int>(size))
        {
            // Shipped refusal leaks the allocated read buffer; reconstruction preserves that.
            ::renddx9abi::logf("RendMisc/EnvMapSystem/EnvMapManager.cpp:321 Couldn't read EMI file \"%s\"",path.c_str());
            return false;
        }
        bytes[size]='\n'; bytes[size+1]=0;
        std::istringstream input{std::string(bytes)};
        char buffer[1024];
        input.getline(buffer,1023);
        // The retail loop tests eofbit only, including its long-line failure behaviour.
        while(!input.eof())
        {
            std::string line(buffer);
            if(!line.empty() && line.back()=='\r')line.erase(line.size()-1);
            if(!line.empty())
            {
                std::istringstream record(line);
                std::string unused1,unused2,name;
                float x=0,y=0,z=0;
                record>>name;
                if(name!="#")
                {
                    char delimiter;
                    bool flag;
                    record>>x; record.get(delimiter); record>>y; record.get(delimiter);
                    record>>z; record.get(delimiter); record>>flag;
                    void* storage=::renddx9abi::engineAlloc(0x14);
                    EnvMapEntry* entry=storage ? new(storage) EnvMapEntry:0;
                    entry->position.x=x; entry->position.y=y; entry->position.z=z; entry->flag=flag;
                    const std::string texturePath=directory+name+".dds";
                    entry->texture=static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(vc71::string(texturePath.c_str())));
                    // Duplicate keys replace the pointer without freeing the old record (retail).
                    vc71::mapNodeValueRef(vc71::mapInsert(m_maps,48,name.c_str()))=entry;
                }
            }
            input.getline(buffer,1023);
        }
        ::renddx9abi::engineFree(bytes);
    }
    file.close();
    return true;
}
void EnvMapManager::onPostLoad()
{
    if(m_maps.size || !m_enabled)return;
    vc71::string level,directory;
    dx9rend::readStringSettingDefault("GSLoadLevel","lightsPlease",level);
    dx9rend::readStringSettingDefault("GSLevelDirectory","",directory);
    const vc71::string path((std::string(directory.c_str())+level.c_str()+"/Envmaps/EnvironmentMapInfo.emi").c_str());
    if(::renddx9abi::engineFileExists(path))loadInfo(path);
}


// Saves level environment-map metadata and cube textures beneath Editor_LevelSavePath.
// Missing objects, entries, textures or output stream fail. The minor version is written
// as a raw character, and a late failure can leave the already-opened EMI file empty.

namespace {
// byte_1023B39C: file-backed 1, read by save alone and written by nothing. [INFERRED] name.
const unsigned char kEnvironmentMapInfoMinorVersion=1;
}
bool EnvMapManager::save()
{
    if(!m_objects.first)return false;
    if(!((m_objects.last-m_objects.first)>>2))return false;
    vc71::string level,directory;
    dx9rend::readStringSettingDefault("GSLoadLevel","lightsPlease",level);
    dx9rend::readStringSettingDefault("GSLevelDirectory","",directory);
    vc71::string levelPath(directory); levelPath+=level;
    vc71::string envmapDirectory(levelPath); envmapDirectory+="/Envmaps/";
    vc71::string infoPath(envmapDirectory); infoPath+="EnvironmentMapInfo.emi";
    vc71::string savePath;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath"," ERROR ",savePath);

    vc71::string fullPath(savePath); fullPath+="/"; fullPath+=infoPath;
    renddx9abi::HostStream* const stream=::dice::hfe::fileManager->open(
        ::renddx9abi::makeHostString(fullPath),static_cast<renddx9abi::HostFileMode>(0),
        static_cast<renddx9abi::HostFileAccess>(2),0);
    if(!stream)return false;

    std::ostringstream out;
    out<<"# EnvironmentMapInformation file "<<std::string(infoPath.c_str(),infoPath.size())<<" "<<std::endl;
    out<<"# Version 0."<<kEnvironmentMapInfoMinorVersion<<" "<<"May 26 2009"<<" "<<std::endl;
    out<<"# "<<std::endl;
    int index=0;
    for(EnvMapObject** item=reinterpret_cast<EnvMapObject**>(m_objects.first);
        item!=reinterpret_cast<EnvMapObject**>(m_objects.last);++item)
    {
        EnvMapObject* const object=*item;
        EnvMapEntry* const entry=object->m_entry;
        if(!entry || !entry->texture)
        {
            stream->release();                                 // File's destructor (sub_10075DE0)
            return false;
        }
        char name[80];
        sprintf(name,"EnvMap%i",index);
        out<<name<<" "<<entry->position.x<<","<<entry->position.y<<","<<entry->position.z<<","
           <<object->m_entry->flag<<" "<<std::endl;
        vc71::string texturePath(savePath); texturePath+="/"; texturePath+=envmapDirectory; texturePath+=name;
        dx9rend::textureManagerSaveTexture(object->m_entry->texture,texturePath);
        ++index;
    }
    const std::string text=out.str();
    stream->write(text.c_str(),static_cast<unsigned int>(text.size()));    // +0x14
    stream->release();                                                     // File::close (sub_10075E00)
    return true;
}


// Captures six faces per environment-map entry, filters mipmaps and rebuilds position keys.
// Requires every object to have an entry. Filter failure logs and continues; no objects returns false.

bool EnvMapManager::generateEnvMaps()
{
    if(!m_objects.first)return false;
    if(!((m_objects.last-m_objects.first)>>2))return false;
    for(EnvMapObject** item=reinterpret_cast<EnvMapObject**>(m_objects.first);
        item!=reinterpret_cast<EnvMapObject**>(m_objects.last);++item)
    {
        EnvMapObject* const object=*item;
        int savedManaged=dx9rend::textureManagerSwapManaged(0);
        dx9rend::EngineTexture* const target=static_cast<dx9rend::EngineTexture*>(
            dx9rend::textureManagerCreateCube(128u,D3DFMT_X8R8G8B8,0u,D3DUSAGE_RENDERTARGET,0));
        dx9rend::textureManagerSwapManaged(savedManaged);
        savedManaged=dx9rend::textureManagerSwapManaged(1);
        object->m_entry->texture=static_cast<dx9rend::EngineTexture*>(
            dx9rend::textureManagerCreateCube(128u,D3DFMT_DXT1,0u,0u,0));
        dx9rend::textureManagerSwapManaged(savedManaged);
        for(unsigned int face=0;face<6;++face)
        {
            dx9rend::rendererRenderEnvMapFace(128u,target,static_cast<int>(face),object->m_entry->position);
            IDirect3DSurface9* const destination=
                dx9rend::textureGetLevelSurface(object->m_entry->texture,0u,face);
            IDirect3DSurface9* const source=dx9rend::textureGetLevelSurface(target,0u,face);
            D3DXLoadSurfaceFromSurface(destination,0,0,source,0,0,D3DX_DEFAULT,0); // IAT 0x101DB4C4
            destination->Release();
            source->Release();
        }
        if(target && --target->refCount==0)
        { dx9rend::textureDtor(target); ::renddx9abi::engineFree(target); }
        const HRESULT hr=D3DXFilterTexture(object->m_entry->texture->baseTexture,0,0,D3DX_DEFAULT); // IAT 0x101DB4A8
        if(hr)
            ::renddx9abi::logf("RendMisc/EnvMapSystem/EnvMapManager.cpp:452 Could not generate mip maps on "
                               "envmap cube texture (hr=0x%08X)",static_cast<unsigned int>(hr));
    }
    clearStringMap(m_maps);
    for(EnvMapObject** item=reinterpret_cast<EnvMapObject**>(m_objects.first);
        item!=reinterpret_cast<EnvMapObject**>(m_objects.last);++item)
    {
        EnvMapObject* const object=*item;
        const Vec3& position=object->m_entry->position;
        const int z=static_cast<int>(position.z);
        const int y=static_cast<int>(position.y);
        const int x=static_cast<int>(position.x);
        char key[64];
        sprintf(key,"=%i=%i=%i",x,y,z);
        vc71::mapNodeValueRef(vc71::mapInsert(m_maps,48,key))=object->m_entry;
    }
    return true;
}
void EnvMapManager::update(float)
{
    flushRemovals();
    if(!m_maps.size)return;
    if(m_maps.size==1)
    { m_currentTexture=static_cast<EnvMapEntry*>(vc71::mapNodeValue(m_maps.head->left))->texture; return; }
    dx9rend::IRenderView* view=static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderView();
    const Mat4* transform=view->getViewMatrixPtr();
    // Keep the shipped multimap operation and duplicate-distance order; no nearest-only shortcut.
    std::multimap<float,EnvMapEntry*> distances;
    for(vc71::map_node* n=vc71::mapFirst(m_maps);n;n=vc71::mapNext(m_maps,n))
    {
        EnvMapEntry* value=static_cast<EnvMapEntry*>(vc71::mapNodeValue(n));
        const float x=transform->m[12]-value->position.x;
        const double y=static_cast<double>(transform->m[13])-value->position.y;
        const double z=static_cast<double>(transform->m[14])-value->position.z;
        const float distance=static_cast<float>(std::sqrt(z*z+y*y+static_cast<double>(x)*x));
        distances.insert(std::make_pair(distance,value));
    }
    EnvMapEntry* closest=distances.begin()->second;
    m_currentTexture=closest->texture;
}
IBase* __fastcall createEnvMapManager(unsigned int iid,IBase* outer)
{
    if(outer)return 0;
    void* storage=::renddx9abi::engineAlloc(0x6C);
    EnvMapManager* instance=storage ? new(storage) EnvMapManager:0;
    IBase* result=static_cast<IBase*>(instance->queryInterface(iid));
    if(!result)instance->release();
    return result;
}
}
namespace geom {
dx9rend::EngineTexture* bundledMeshEnvironmentTexture()
{ return static_cast<Rend::EnvMapManager*>(Rend::g_EnvMapManager)->m_currentTexture; }
}
} }
#endif
