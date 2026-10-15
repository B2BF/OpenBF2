// [TEMP] [CHOSEN, not recovered] Includes the actual ResourceManager source. Mocked boundaries
// observe lifecycle registration, typed ABI dispatch, branch ordering and enumerated names.
// This is not a real CEPreLoad/CEPostLoad map-load or D3D acceptance run.
#include "../BF2/RendDX9/ResourceManager.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>

using namespace dice::hfe;
using namespace dice::hfe::dx9rend;
namespace {
int checks=0, failures=0, frees=0, managedReads=0, managedMode=1;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; std::printf("FAIL %d: %s\n",__LINE__,#e); } } while(0)
std::vector<std::string> events, acquisitions;
std::map<std::string,std::vector<std::pair<std::string,bool>>> directory;
IEventListener* registered=0;
bool managedSetting=true, clearReadyInSetting=false, registrationResult=true;
int surfaceReleases=0;
void* const textureSentinel=reinterpret_cast<void*>(0x12121212u);
void* const particlesSentinel=reinterpret_cast<void*>(0x13131313u);
void* surfaceObject=0;

bool __fastcall registerHandler(void*,void*,EventCategory category,IEventListener* listener,int priority)
{
    CHECK(category==ECCore && priority==9000);
    CHECK(reinterpret_cast<unsigned char*>(listener)[4]==0);
    CHECK(reinterpret_cast<unsigned int*>(listener)[2]==0 && reinterpret_cast<unsigned int*>(listener)[3]==0);
    CHECK(reinterpret_cast<unsigned int*>(listener)[4]==0);
    registered=listener; events.push_back("register"); return registrationResult;
}
bool __fastcall unregisterHandler(void*,void*,EventCategory category,IEventListener* listener)
{ CHECK(category==ECCore && listener==registered); registered=0; events.push_back("unregister"); return true; }
unsigned int __fastcall postEvent(void*,void*,EventCategory category,unsigned int id,void* text,float delay)
{
    CHECK(category==ECHud && id==7 && delay==0.0f);
    const std::string value(static_cast<char*>(text));
    CHECK(value=="Loading Geometries.." || value=="Loading Textures..");
    events.push_back(value); return 0;
}
void __fastcall resetTemplates(void*,void*)
{ CHECK(resourceManagerIsReady()); events.push_back("resetTemplates"); }
void __fastcall loadTemplates(void*,void*)
{ CHECK(resourceManagerIsReady()); events.push_back("loadTemplates"); }
void __fastcall loadShadows(void*,void*)
{ CHECK(!resourceManagerIsReady()); events.push_back("shadows"); }
double __fastcall clockRead(void*,void*)
{ CHECK(resourceManagerIsReady()); events.push_back("clock"); return 25.0; }
const bool& __fastcall settingsRead(void*,void*,const renddx9abi::HostString& name,const bool& fallback)
{
    CHECK(std::strcmp(name.c_str(),"GSManagedTextures")==0 && fallback);
    ++managedReads; events.push_back("setting");
    if(clearReadyInSetting) resourceManagerSetReady(0);
    return managedSetting;
}
int __fastcall findFiles(void*,void*,const std::string& path,bool recursive,std::list<io::FileInfo>& out)
{
    CHECK(!recursive); events.push_back("find:"+path);
    for(const auto& item:directory[path]) {
        io::FileInfo file={}; file.name=item.first; file.isDirectory=item.second; out.push_back(file);
    }
    return static_cast<int>(out.size());
}
ULONG __stdcall releaseSurface(void* receiver)
{ CHECK(receiver==&surfaceObject); CHECK(!resourceManagerIsReady()); ++surfaceReleases; events.push_back("releaseSurface"); return 0; }
HRESULT __stdcall createRenderTarget(void*,UINT w,UINT h,D3DFORMAT format,D3DMULTISAMPLE_TYPE multi,
                                    DWORD quality,BOOL lockable,IDirect3DSurface9** result,HANDLE* shared)
{
    CHECK(resourceManagerIsReady());
    CHECK(w==2048 && h==2048 && format==D3DFMT_A8R8G8B8);
    CHECK(multi==D3DMULTISAMPLE_NONE && quality==0 && lockable==FALSE && shared==0);
    CHECK(reinterpret_cast<unsigned int*>(g_resourceManager)[2]==0x1000000u);
    *result=reinterpret_cast<IDirect3DSurface9*>(&surfaceObject);
    events.push_back("reserve"); return 0;
}

void clearObserved() { events.clear(); acquisitions.clear(); }
void expectPost(bool managed)
{
    const char* prefix[]={"overPre","Loading Geometries..","collect","particlePost","clock",
        "loadTemplates","clock","Loading Textures..","clock","sweep","clock","setting"};
    std::vector<std::string> expected(prefix,prefix+sizeof(prefix)/sizeof(prefix[0]));
    if(managed) expected.push_back("suffix");
    else { expected.push_back("releaseSurface"); expected.push_back("managed:1"); }
    expected.push_back("overPost"); expected.push_back("shadows"); expected.push_back("collect");
    CHECK(events==expected);
}
}

namespace renddx9abi {
void* engineAlloc(unsigned int size) { CHECK(size==20); return std::malloc(size); }
void engineFree(void* p) { ++frees; std::free(p); }
void logf(const char*,...) {}
unsigned int addr(const void* p) { return reinterpret_cast<unsigned int>(p); }
}
namespace dice { namespace hfe {
EventManager* eventManager=0;
::renddx9abi::HostFileManager* fileManager=0;
System* g_system=0;
decltype(settings) settings=0;
namespace geom {
IBase* g_GeometryTemplateManager=0;
IBase* g_ParticleSystemManager=static_cast<IBase*>(particlesSentinel);
LightManager* g_LightManager=0;
void particleSystemManagerPreLoadLevel(IParticleSystemManager* manager)
{ CHECK(manager==particlesSentinel); events.push_back("particlePre"); }
void particleSystemManagerPostLoadLevel(IParticleSystemManager* manager)
{ CHECK(manager==particlesSentinel && resourceManagerIsReady()); events.push_back("particlePost"); }
}
namespace dx9rend {
void* g_rendererBase=0;
void bufferManagerCollectUnused() { events.push_back("collect"); }
void textureManagerSweep() { CHECK(resourceManagerIsReady()); events.push_back("sweep"); }
void textureManagerRefreshFileSuffix() { CHECK(!resourceManagerIsReady()); events.push_back("suffix"); }
void overGrowthPreLoadResources() { events.push_back("overPre"); }
void overGrowthPostLoadResources() { CHECK(!resourceManagerIsReady()); events.push_back("overPost"); }
int textureManagerSwapManaged(int value)
{ const int old=managedMode; managedMode=value; events.push_back("managed:"+std::to_string(value)); return old; }
void* textureManagerGetTexture(const std::string& name)
{
    CHECK(resourceManagerDefaultPriority()==7);
    acquisitions.push_back(name.c_str()); events.push_back("texture:"+std::string(name.c_str()));
    return textureSentinel;
}
void textureReleaseRef(EngineTexture*& texture)
{ CHECK(texture==textureSentinel); events.push_back("releaseTexture"); texture=0; }
// Narrow pathname helper model. It preserves the already-recovered last-dot and '..' rule;
// this test assesses ResourceManager's use of its result, not the helper's implementation.
void pathGetExtension(std::string& out,const std::string& in)
{
    const std::string value(in.c_str()); const size_t dot=value.rfind('.');
    out=(dot==std::string::npos || (dot>1 && value[dot-1]=='.')) ? "" : value.substr(dot+1).c_str();
}
// This fixture's atlas names are ASCII. Extended-Latin path normalization belongs to the
// existing TextureManager helper's own contract, outside this ResourceManager test.
void pathToLower(std::string& value)
{
    std::string lower(value.c_str());
    for(char& c:lower) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    value=lower.c_str();
}
}
} }

int main()
{
    void* eventTable[0x34/4]={}; eventTable[0x10/4]=reinterpret_cast<void*>(&postEvent);
    eventTable[0x2C/4]=reinterpret_cast<void*>(&registerHandler); eventTable[0x30/4]=reinterpret_cast<void*>(&unregisterHandler);
    void* eventObject=eventTable; eventManager=reinterpret_cast<EventManager*>(&eventObject);
    void* geometryTable[0x44/4]={}; geometryTable[0x3C/4]=reinterpret_cast<void*>(&loadTemplates);
    geometryTable[0x40/4]=reinterpret_cast<void*>(&resetTemplates);
    void* geometryObject=geometryTable; geom::g_GeometryTemplateManager=reinterpret_cast<IBase*>(&geometryObject);
    void* lightTable[0x2F0/4]={}; lightTable[0x2EC/4]=reinterpret_cast<void*>(&loadShadows);
    void* lightObject=lightTable; geom::g_LightManager=reinterpret_cast<geom::LightManager*>(&lightObject);
    void* clockTable[0x1C/4]={}; clockTable[0x18/4]=reinterpret_cast<void*>(&clockRead);
    void* clockObject=clockTable; g_system=reinterpret_cast<System*>(&clockObject);
    void* settingsTable[0x54/4]={}; settingsTable[0x50/4]=reinterpret_cast<void*>(&settingsRead);
    void* settingsObject=settingsTable; settings=reinterpret_cast<decltype(settings)>(&settingsObject);
    void* fileTable[0x38/4]={}; fileTable[0x34/4]=reinterpret_cast<void*>(&findFiles);
    void* fileObject=fileTable; fileManager=reinterpret_cast<renddx9abi::HostFileManager*>(&fileObject);
    void* surfaceTable[3]={}; surfaceTable[2]=reinterpret_cast<void*>(&releaseSurface); surfaceObject=surfaceTable;
    void* deviceTable[0x74/4]={}; deviceTable[0x70/4]=reinterpret_cast<void*>(&createRenderTarget);
    void* deviceObject=deviceTable;
    RendDX9Base base={}; base.device=reinterpret_cast<IDirect3DDevice9*>(&deviceObject); g_rendererBase=&base;
    CHECK(createResourceManager());
    ResourceManager* manager=static_cast<ResourceManager*>(g_resourceManager);
    CHECK(registered==manager && resourceManagerIsReady());
    CHECK(resourceManagerSwapDefaultPriority(7)==0);
    clearObserved(); registered->handleEvent(ECHud,2,0,0); registered->handleEvent(ECCore,99,0,0);
    CHECK(events.empty());

    directory["menu/atlas/"]={{"ordinary.dds",false},{"ATLAS_B.dds",false},{"pack/AtlasA.dds",false},
        {"ignored.DDS",false},{"atlas..dds",false},{"readme.txt",false},{"sub",true}};
    directory["menu/atlas/sub/"]={{"plain.dds",false},{"nested/deep.dds",false},{"folder.dds",true}};
    registered->handleEvent(ECCore,2,0,0);
    const std::vector<std::string> names={"menu/atlas/ATLAS_B","menu/atlas/AtlasA",
        "menu/atlas/sub/plain","menu/atlas/sub/deep","menu/atlas/sub/folder"};
    CHECK(acquisitions==names);
    CHECK(events[0]=="resetTemplates" && events[1]=="setting" && events[2]=="managed:1");
    CHECK(events.back()=="particlePre" && resourceManagerDefaultPriority()==7);
    CHECK(events[events.size()-2]=="find:menu/atlas/sub/folder.dds/");
    clearObserved(); manager->loadAtlases("menu/atlas/",false);
    const std::vector<std::string> rootNames={"menu/atlas/ATLAS_B","menu/atlas/AtlasA"};
    CHECK(acquisitions==rootNames);
    CHECK(events.size()==5 && events[0]=="find:menu/atlas/");
    clearObserved(); registered->handleEvent(ECCore,3,0,0); expectPost(true);
    CHECK(!resourceManagerIsReady());

    clearObserved(); clearReadyInSetting=true; registered->handleEvent(ECCore,2,0,0);
    const std::vector<std::string> skip={"resetTemplates","setting","managed:1","particlePre"};
    CHECK(events==skip && acquisitions.empty()); clearReadyInSetting=false;
    managedSetting=false; directory.clear(); clearObserved(); registered->handleEvent(ECCore,2,0,0);
    const std::vector<std::string> unmanaged={"resetTemplates","setting","reserve","managed:0","find:menu/atlas/","particlePre"};
    CHECK(events==unmanaged);
    clearObserved(); registered->handleEvent(ECCore,3,0,0); expectPost(false);
    CHECK(surfaceReleases==1 && reinterpret_cast<unsigned int*>(manager)[2]==0 && reinterpret_cast<unsigned int*>(manager)[3]==0);

    // The setting is queried again at post-load, not cached from pre-load.
    clearObserved(); registered->handleEvent(ECCore,2,0,0); managedSetting=true;
    clearObserved(); registered->handleEvent(ECCore,3,0,0); expectPost(true);
    CHECK(surfaceReleases==1 && managedReads==7);
    delete manager; g_resourceManager=0;
    CHECK(registered==0 && frees==1 && events.back()=="unregister");
    registrationResult=false; clearObserved();
    CHECK(createResourceManager());
    CHECK(registered==g_resourceManager && resourceManagerIsReady());
    delete static_cast<ResourceManager*>(g_resourceManager); g_resourceManager=0;
    CHECK(frees==2 && registered==0);
    std::printf("ResourceManager lifecycle: %d/%d checks passed\n",checks-failures,checks);
    return failures ? 1 : 0;
}
