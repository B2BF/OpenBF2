// [TEMP]
// Strict x86 tests of environment-map capture and EMI writing with file, texture, view, and D3DX observers.
// No GPU, game process, or production fallback is used.
#include "../BF2/RendMisc/EnvMapSystem/EnvMapManager.cpp"
#include "../BF2/RendMisc/EnvMapSystem/EnvMapObject.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
using namespace dice::hfe;
using namespace dice::hfe::Rend;
namespace {
int checks=0,destroyed=0,evictions=0,logs=0;
#define CHECK(c) do { ++checks;if(!(c)){std::fprintf(stderr,"line%d: %s\n",__LINE__,#c);std::exit(1);} }while(0)
std::map<std::string,std::string> files;
std::map<std::string,dx9rend::EngineTexture*> textures;
std::vector<std::string> loaded;
std::string openPath;
unsigned int cursor=0;
unsigned char viewBytes[sizeof(Mat4)]={};
struct Fake {void** vptr;} view,renderer;
void* viewSlots[26]={},*rendererSlots[19]={};
void* __fastcall getView(void*,void*) {return &view;}
const Mat4* __fastcall getTransform(void*,void*) {return reinterpret_cast<const Mat4*>(viewBytes);}
EnvMapEntry* entry(EnvMapManager& m,const char* key)
{ const auto found=m.m_maps.find(key);return found==m.m_maps.end()?0:found->second; }
// Capture and writer observers.
struct FaceCall {unsigned int size;dx9rend::EngineTexture* cube;int face;Vec3 position;};
std::vector<FaceCall> faceCalls;
std::vector<dx9rend::EngineTexture*> cubes,savedTextureObjects;
std::vector<unsigned int> cubeFormats,cubeUsages;
std::vector<int> cubeManaged;
std::vector<std::pair<dx9rend::EngineTexture*,unsigned int> > surfaceRequests;
std::vector<IDirect3DBaseTexture9*> filtered;
std::vector<std::string> settingReads,savedTextures;
int managedFlag=1,surfaceLoads=0,surfaceReleases=0,fileOpens=0,streamReleases=0,refuseOpen=0;
int openedMode=-1,openedAccess=-1;
HRESULT filterResult=0;
std::string openedPath,written;
Fake surfaceFake,fileManagerFake,streamFake;
void* surfaceSlots[3]={},*fileManagerSlots[48]={},*streamSlots[16]={};
unsigned long __stdcall surfaceRelease(void*) {++surfaceReleases;return 0;}
renddx9abi::HostStream* __fastcall fileOpen(void*,void*,const std::string& path,int mode,int access,unsigned int)
{
    ++fileOpens;openedPath=path;openedMode=mode;openedAccess=access;
    return refuseOpen ? 0:reinterpret_cast<renddx9abi::HostStream*>(&streamFake);
}
unsigned int __fastcall streamWrite(void*,void*,const void* data,unsigned int size)
{ written.append(static_cast<const char*>(data),size);return size; }
int __fastcall streamRelease(void*,void*) {++streamReleases;return 0;}
}
namespace renddx9abi {
void* engineAlloc(unsigned int n){return std::malloc(n);}void engineFree(void* p){std::free(p);}
void logf(const char*,...){++logs;}
bool EngineStream::open(const std::string& path)
{ openPath=path.c_str();cursor=0;m_stream=files.count(openPath)?reinterpret_cast<HostNonlinearStream*>(1):0;return m_stream!=0; }
void EngineStream::close(){m_stream=0;}
unsigned int EngineStream::read(void* out,unsigned int n)
{const std::string& data=files[openPath];CHECK(cursor+n<=data.size());std::memcpy(out,data.data()+cursor,n);cursor+=n;return n;}
int EngineStream::getSize(){return static_cast<int>(files[openPath].size());}
bool engineFileExists(const std::string& path){return files.count(path.c_str())!=0;}
}
namespace dice { namespace hfe {
namespace Rend {IBase* g_EnvMapManager=0;}
::renddx9abi::HostFileManager* fileManager=0; // HM1: main points it at the file spy
namespace dx9rend {
void* g_rendererBase=&renderer;
void* textureManagerGetTexture(const std::string& name)
{
    std::string key=name.c_str();loaded.push_back(key);
    EngineTexture*& t=textures[key];
    if(!t){t=static_cast<EngineTexture*>(std::calloc(1,sizeof(EngineTexture)));t->refCount=1;}
    ++t->refCount;return t;
}
void textureDtor(EngineTexture*){++destroyed;}
void textureManagerEvictFromCache(void* value)
{
    ++evictions;
    for(auto i=textures.begin();i!=textures.end();++i)if(i->second==value)
    {EngineTexture* t=i->second;CHECK(t->refCount==1);textureDtor(t);std::free(t);textures.erase(i);return;}
    CHECK(false);
}
void readStringSettingDefault(const char* name,const char* fallback,std::string& out)
{
    settingReads.push_back(name);
    if(!std::strcmp(name,"GSLoadLevel")){CHECK(!std::strcmp(fallback,"lightsPlease"));out="fixture";}
    else if(!std::strcmp(name,"Editor_LevelSavePath")){CHECK(!std::strcmp(fallback," ERROR "));out="save";}
    else{CHECK(!std::strcmp(name,"GSLevelDirectory") && !*fallback);out="Levels/";}
}
// Capture and writer dependencies.
int textureManagerSwapManaged(int value) {const int old=managedFlag;managedFlag=value;return old;}
void* textureManagerCreateCube(unsigned int side,unsigned int format,unsigned int mips,unsigned int usage,void* existing)
{
    CHECK(side==128 && !mips && !existing);
    EngineTexture* t=static_cast<EngineTexture*>(std::calloc(1,sizeof(EngineTexture)));
    t->refCount=1;t->baseTexture=reinterpret_cast<IDirect3DBaseTexture9*>(0x1000+16*cubes.size());
    cubes.push_back(t);cubeFormats.push_back(format);cubeUsages.push_back(usage);cubeManaged.push_back(managedFlag);
    return t;
}
IDirect3DSurface9* textureGetLevelSurface(EngineTexture* texture,unsigned int mip,unsigned int face)
{
    CHECK(!mip);surfaceRequests.push_back(std::make_pair(texture,face));
    return reinterpret_cast<IDirect3DSurface9*>(&surfaceFake);
}
bool textureManagerSaveTexture(EngineTexture* texture,const std::string& name)
{ savedTextureObjects.push_back(texture);savedTextures.push_back(name.c_str());return true; }
bool rendererRenderEnvMapFace(unsigned int size,EngineTexture* cube,int face,const Vec3& position)
{ FaceCall call={size,cube,face,position};faceCalls.push_back(call);return false; }
} } }
// D3DX observers for environment-map capture.
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromSurface(
    IDirect3DSurface9* destination,const PALETTEENTRY* destinationPalette,const RECT* destinationRect,
    IDirect3DSurface9* source,const PALETTEENTRY* sourcePalette,const RECT* sourceRect,
    DWORD filter,D3DCOLOR colourKey)
{
    CHECK(destination && source && !destinationPalette && !destinationRect && !sourcePalette && !sourceRect);
    CHECK(filter==0xFFFFFFFFu && !colourKey);
    ++surfaceLoads;return 0;
}
extern "C" HRESULT WINAPI D3DXFilterTexture(
    IDirect3DBaseTexture9* texture,const PALETTEENTRY* palette,UINT sourceLevel,DWORD filter)
{ CHECK(!palette && !sourceLevel && filter==0xFFFFFFFFu);filtered.push_back(texture);return filterResult; }
int main()
{
    view.vptr=viewSlots;renderer.vptr=rendererSlots;
    viewSlots[0x64/4]=reinterpret_cast<void*>(getTransform);
    rendererSlots[0x48/4]=reinterpret_cast<void*>(getView);
    surfaceFake.vptr=surfaceSlots;surfaceSlots[2]=reinterpret_cast<void*>(surfaceRelease); // COM Release
    streamFake.vptr=streamSlots;streamSlots[0x08/4]=reinterpret_cast<void*>(streamRelease);
    streamSlots[0x14/4]=reinterpret_cast<void*>(streamWrite);
    fileManagerFake.vptr=fileManagerSlots;fileManagerSlots[0x18/4]=reinterpret_cast<void*>(fileOpen);
    dice::hfe::fileManager=reinterpret_cast<renddx9abi::HostFileManager*>(&fileManagerFake);
    EnvMapManager m;g_EnvMapManager=&m;
    CHECK(m.getRef()==1 && m.getEnabled() && m.getPerPixelEnabled());
    CHECK(m.queryInterface(1)==&m && m.queryInterface(820202)==&m && !m.queryInterface(2));
    CHECK(!m.m_currentTexture && m.m_maps.empty());
    CHECK(!m.loadInfo(std::string("missing.emi")) && logs==1);
    files["empty.emi"]="";CHECK(m.loadInfo(std::string("empty.emi")) && m.m_maps.empty());
    files["Levels/fixture/Envmaps/EnvironmentMapInfo.emi"]="# ignored\r\n\nzeta 10/0/0/1\r\nalpha -10/0/0/0\n";
    m.setEnabled(false);m.onPostLoad();CHECK(m.m_maps.empty());
    m.setEnabled(true);m.onPostLoad();CHECK(m.m_maps.size()==2 && loaded.size()==2);
    CHECK(loaded[0]=="Levels/fixture/Envmaps/zeta.dds" && loaded[1]=="Levels/fixture/Envmaps/alpha.dds");
    CHECK(entry(m,"zeta")->position.x==10 && entry(m,"zeta")->flag);
    CHECK(entry(m,"alpha")->position.x==-10 && !entry(m,"alpha")->flag);
    m.onPostLoad();CHECK(loaded.size()==2);
    m.update(1);CHECK(m.m_currentTexture==entry(m,"alpha")->texture); // equal distances: map key traversal order
    reinterpret_cast<float*>(viewBytes)[12]=9;m.update(2);CHECK(m.m_currentTexture==entry(m,"zeta")->texture);
    CHECK(geom::bundledMeshEnvironmentTexture()==m.m_currentTexture);
    m.setPerPixelEnabled(false);CHECK(!m.getPerPixelEnabled());m.reset();
    CHECK(!m.m_currentTexture && m.getPerPixelEnabled() && m.m_maps.size()==2);
    m.clear();CHECK(m.m_maps.empty() && !m.m_currentTexture && destroyed==2 && evictions==2 && textures.empty());
    files["one.emi"]="only 1/2/3/1\n";CHECK(m.loadInfo(std::string("one.emi")));
    m.update(3);CHECK(m.m_currentTexture==entry(m,"only")->texture && loaded.back()=="only.dds");
    EnvMapObjectTemplate* t=m.getTemplate(std::string("Vehicle"));
    CHECK(t && m.getTemplate(std::string("vehicle"))==t && m.m_templateCount==1);
    CHECK(t->getSize()==1 && t->getIsStatic());
    EnvMapObject* object=m.createObject(t);CHECK(object && m.m_objects.size()==1);
    m.queueRemoval(object);m.update(4);CHECK(m.m_objects.empty() && m.m_pendingRemoval.empty());
    m.clear();CHECK(m.m_templates.empty() && !m.m_templateCount && textures.empty());
    // The capture cubes are not cache textures; this fixture leaves the manager undestroyed.
    {
        EnvMapManager* const c=new EnvMapManager;g_EnvMapManager=c;
        CHECK(!c->generateEnvMaps() && !c->save() && !fileOpens && faceCalls.empty() && cubes.empty());
        EnvMapObjectTemplate* const probe=c->getTemplate(std::string("Probe"));
        EnvMapObject* const first=c->createObject(probe);
        EnvMapObject* const second=c->createObject(probe);
        Mat4 transform;std::memset(&transform,0,sizeof(transform));
        transform.m[12]=10.75f;transform.m[13]=-2.25f;transform.m[14]=3.5f;
        CHECK(first->update(transform,probe,0));
        probe->setIsStatic(false);
        transform.m[12]=-4;transform.m[13]=5;transform.m[14]=-6.5f;
        CHECK(second->update(transform,probe,0));
        const int destroyedBefore=destroyed;
        CHECK(c->generateEnvMaps());
        CHECK(cubes.size()==4 && managedFlag==1 && faceCalls.size()==12 && surfaceRequests.size()==24);
        CHECK(surfaceLoads==12 && surfaceReleases==24 && filtered.size()==2 && destroyed==destroyedBefore+2);
        for(unsigned int o=0;o<2;++o)
        {
            EnvMapEntry* const e=(o ? second:first)->m_entry;
            CHECK(cubeFormats[2*o]==D3DFMT_X8R8G8B8 && cubeUsages[2*o]==D3DUSAGE_RENDERTARGET && !cubeManaged[2*o]);
            CHECK(cubeFormats[2*o+1]==D3DFMT_DXT1 && !cubeUsages[2*o+1] && cubeManaged[2*o+1]==1);
            CHECK(e->texture==cubes[2*o+1] && filtered[o]==cubes[2*o+1]->baseTexture);
            for(unsigned int f=0;f<6;++f)
            {
                const FaceCall& call=faceCalls[6*o+f];
                CHECK(call.size==128 && call.cube==cubes[2*o] && call.face==static_cast<int>(f));
                CHECK(call.position.x==e->position.x && call.position.y==e->position.y && call.position.z==e->position.z);
                CHECK(surfaceRequests[12*o+2*f]==std::make_pair(cubes[2*o+1],f));   // the DXT1 face, then
                CHECK(surfaceRequests[12*o+2*f+1]==std::make_pair(cubes[2*o],f));   // the target's
            }
        }
        CHECK(c->m_maps.size()==2 && entry(*c,"=10=-3=3")==first->m_entry && entry(*c,"=-4=5=-7")==second->m_entry);
        const int logsBefore=logs;filterResult=E_FAIL;
        CHECK(c->generateEnvMaps() && logs==logsBefore+2 && c->m_maps.size()==2); // reported, goes on, rebuilt
        filterResult=0;
        const size_t reads=settingReads.size();
        CHECK(c->save() && settingReads.size()==reads+3 && settingReads[reads]=="GSLoadLevel");
        CHECK(settingReads[reads+1]=="GSLevelDirectory" && settingReads[reads+2]=="Editor_LevelSavePath");
        CHECK(fileOpens==1 && openedPath=="save/Levels/fixture/Envmaps/EnvironmentMapInfo.emi");
        CHECK(openedMode==0 && openedAccess==2 && streamReleases==1);
        CHECK(written==std::string("# EnvironmentMapInformation file Levels/fixture/Envmaps/EnvironmentMapInfo.emi \n"
                                   "# Version 0.\x01 May 26 2009 \n# \nEnvMap0 10,-3,3,1 \nEnvMap1 -4,5,-7,0 \n"));
        CHECK(savedTextures.size()==2 && savedTextures[0]=="save/Levels/fixture/Envmaps/EnvMap0");
        CHECK(savedTextures[1]=="save/Levels/fixture/Envmaps/EnvMap1");
        CHECK(savedTextureObjects[0]==first->m_entry->texture && savedTextureObjects[1]==second->m_entry->texture);
        refuseOpen=1;CHECK(!c->save() && fileOpens==2 && streamReleases==1 && savedTextures.size()==2);
        refuseOpen=0;
        const std::string text=written;
        c->createObject(probe);                    // no entry: the first such object ends the save
        CHECK(!c->save() && fileOpens==3 && streamReleases==2 && written==text && savedTextures.size()==4);
    }
    g_EnvMapManager=0;
    CHECK(createEnvMapManager(1,reinterpret_cast<IBase*>(1))==0);
    IBase* heap=createEnvMapManager(820202,0);CHECK(heap && heap->release()==0);
    CHECK(!createEnvMapManager(0xBAD,0));
    std::printf("EnvMapManager: %d checks passed\n",checks);
}
