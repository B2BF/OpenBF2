// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: VIDEO-SETTINGS-FIXTURE]
// Tests real VideoSettings lifetime, defaults, serialization, accessors, and renderer branches.
// The renderer exposes a typed IVideoOptions observer through its primary table; owned settings state remains real.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../BF2/Game/VideoSettings.h"
#include "../BF2/Game/Main/IVideoOptions.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
using namespace dice::hfe;
static unsigned checks, failures;
static void check(bool value,const char* label)
{ ++checks; if(!value){++failures;std::printf("FAIL %s\n",label);} }

namespace dice { namespace hfe {
// [TEMP] Globals this isolated executable does not link from their owning units. `game` is only
// ever compared with null by the exercised bodies, so a non-null dummy models "a game is loaded".
SettingsRepostitory* settings=nullptr;
ClassManager* classManager=nullptr;
EventManager* eventManager=nullptr;
IGame* game=nullptr;
BF2Render* g_bf2Render=nullptr;
struct VideoSettingsFixture {
    static VideoSettings& owner(IBase* base){ return *static_cast<VideoSettings*>(base); }
    static int refs(VideoSettings& v){ return v.m_refCount; }
    static bool restart(VideoSettings& v){ return v.m_restartRequired; }
    static const VideoSettingsData& current(VideoSettings& v){ return v.m_current; }
    static const VideoSettingsData& pending(VideoSettings& v){ return v.m_pending; }
};
} }

static std::vector<std::string> callLog;
static std::string entry(const char* name,int index){ char b[64]; std::snprintf(b,sizeof b,"%s %d",name,index); return b; }
static std::string entry(const char* name,int index,int value){ char b[64]; std::snprintf(b,sizeof b,"%s %d %d",name,index,value); return b; }
static std::string joined(){ std::string s; for(const std::string& e:callLog){ s+=e; s+=';'; } return s; }

// [TEMP] The renderer's VideoOptions object as VideoSettings sees it: every slot records itself.
struct OptionsObserver : IVideoOptions {
    int quality[8], minimum[8], level[8], ceiling[8]; bool flags[8];
    std::vector<std::string> lists[2]; std::string selected[2];
    float scale=0.75f; int detected=2, presetApplied=-1; unsigned applies=0; bool perGpu=false;
    OptionsObserver()
    {
        for(int i=0;i<8;++i){ quality[i]=i; minimum[i]=(i==3||i==4)?0:1; level[i]=3; ceiling[i]=(i==7)?2:3; flags[i]=(i==4); }
        lists[0]={"Off","2Samples","4Samples"}; lists[1]={"640x480@60Hz","800x600@60Hz","1024x768@85Hz"};
        selected[0]="2Samples"; selected[1]="1024x768@85Hz";
    }
    bool setQuality(int index,int value) override { callLog.push_back(entry("setQuality",index,value)); quality[index]=value; return true; }
    int getQuality(int index) override { callLog.push_back(entry("getQuality",index)); return quality[index]; }
    int getMinQuality(int index) override { callLog.push_back(entry("getMinQuality",index)); return minimum[index]; }
    int getMaxQualityLevel(int index) override { callLog.push_back(entry("getMaxQualityLevel",index)); return level[index]; }
    int getMaxQuality(int index) override { callLog.push_back(entry("getMaxQuality",index)); return ceiling[index]; }
    bool getRestartFlag(int index) override { callLog.push_back(entry("getRestartFlag",index)); return flags[index]; }
    const std::vector<std::string>& getDynamicOptionList(int index) override { callLog.push_back(entry("getDynamicOptionList",index)); return lists[index]; }
    bool setDynamicOption(int index,const std::string& value) override { callLog.push_back(entry("setDynamicOption",index)+"="+value); selected[index]=value; return true; }
    const std::string& getDynamicOption(int index) override { callLog.push_back(entry("getDynamicOption",index)); return selected[index]; }
    void setViewDistanceScale(float value) override { callLog.push_back("setViewDistanceScale"); scale=value; }
    float getViewDistanceScale() override { callLog.push_back("getViewDistanceScale"); return scale; }
    void apply() override { callLog.push_back("apply"); ++applies; }
    int applyQualityPreset(int scheme) override { callLog.push_back(entry("applyQualityPreset",scheme)); presetApplied=scheme; for(int i=0;i<8;++i) quality[i]=scheme; return 0; }
    int detectQualityLevel() override { callLog.push_back("detectQualityLevel"); return detected; }
    bool getPerGpuFlag() override { callLog.push_back("getPerGpuFlag"); return perGpu; }
};

// [TEMP] The renderer handle: a primary table whose +0x74 slot answers with the observer (or null).
static OptionsObserver* rendererOptions=nullptr;
static unsigned slot74Calls=0;
static IVideoOptions* __fastcall rendererSlot74(IBase*,void*) { ++slot74Calls; return rendererOptions; }
// [CHOSEN] +0xDC getDesktopMode: a 1280x1024@85 desktop, so only entries up to that size fit.
static bool __fastcall rendererSlotDC(IBase*,void*,unsigned& w,unsigned& h,unsigned& hz) { w=1280; h=1024; hz=85; return true; }
static void* rendererTable[64];
struct FakeRenderer { void** table; };
static FakeRenderer fakeRenderer{rendererTable};
// BF2Render's x86 layout (BF2ClientSeams.h asserts 0x20): vptr, engine, window, dll, renderer, ...
struct FakeBF2Render { void* vptr; void* engine; void* window; void* dll; IBase* renderer; void* postProduction; void* carrier; int index; };
static_assert(sizeof(FakeBF2Render)==sizeof(BF2Render),"the fixture mirrors BF2Render's x86 layout");
static FakeBF2Render fakeRender{};

static const char* const defaultScript=
    "VideoSettings.setTerrainQuality 2\nVideoSettings.setGeometryQuality 2\nVideoSettings.setLightingQuality 2\n"
    "VideoSettings.setDynamicLightingQuality 2\nVideoSettings.setDynamicShadowsQuality 2\nVideoSettings.setEffectsQuality 2\n"
    "VideoSettings.setTextureQuality 2\nVideoSettings.setTextureFilteringQuality 2\nVideoSettings.setResolution 800x600@60Hz\n"
    "VideoSettings.setAntialiasing Off\nVideoSettings.setViewDistanceScale 1\nVideoSettings.setVideoOptionScheme 1\n";

static bool isDefault(const VideoSettingsData& d)
{
    return d.terrainQuality==2 && d.geometryQuality==2 && d.lightingQuality==2 && d.dynamicLightingQuality==2
        && d.dynamicShadowsQuality==2 && d.effectsQuality==2 && d.textureQuality==2 && d.textureFilteringQuality==2
        && d.resolution=="800x600@60Hz" && d.antialiasing=="Off" && d.viewDistanceScale==1.0f && d.videoOptionScheme==1;
}
static bool rangesAre(IVideoSettings* api,const int* minimum,const int* maximum)
{
    const int mins[8]={api->getTerrainQualityMin(),api->getGeometryQualityMin(),api->getLightingQualityMin(),api->getDynamicLightingQualityMin(),
        api->getDynamicShadowsQualityMin(),api->getEffectsQualityMin(),api->getTextureQualityMin(),api->getTextureFilteringQualityMin()};
    const int maxs[8]={api->getTerrainQualityMax(),api->getGeometryQualityMax(),api->getLightingQualityMax(),api->getDynamicLightingQualityMax(),
        api->getDynamicShadowsQualityMax(),api->getEffectsQualityMax(),api->getTextureQualityMax(),api->getTextureFilteringQualityMax()};
    for(int i=0;i<8;++i) if(mins[i]!=minimum[i] || maxs[i]!=maximum[i]) return false;
    return true;
}
static std::string rangeReads(){ std::string s; for(int i=0;i<8;++i){ s+=entry("getMinQuality",i)+";"+entry("getMaxQualityLevel",i)+";"; } return s; }
static std::string readback(){ std::string s; for(int i=0;i<8;++i) s+=entry("getQuality",i)+";"; return s+"getDynamicOption 1;getDynamicOption 0;getViewDistanceScale;"; }
static std::string pushes(const int* rows,const char* resolution,const char* antialiasing)
{
    std::string s; for(int i=0;i<8;++i) s+=entry("setQuality",i,rows[i])+";";
    s+=std::string("setDynamicOption 1=")+resolution+";";
    if(antialiasing) s+=std::string("setDynamicOption 0=")+antialiasing+";";
    return s+"setViewDistanceScale;";
}

int main()
{
    check(sizeof(void*)==4,"x86 fixture");
    SettingsRepostitory repository; settings=&repository;
    rendererTable[0x74/4]=reinterpret_cast<void*>(&rendererSlot74);
    rendererTable[0xDC/4]=reinterpret_cast<void*>(&rendererSlotDC);
    IGame* const loadedGame=reinterpret_cast<IGame*>(&repository);   // any non-null value: only tested against null

    // lifetime and identity
    IBase* object=VideoSettings::create(IID_IVideoSettings,nullptr);
    check(object!=nullptr,"factory yields the owner for its interface id");
    IVideoSettings* api=static_cast<IVideoSettings*>(object);
    VideoSettings& owner=VideoSettingsFixture::owner(object);
    const VideoSettingsData& current=VideoSettingsFixture::current(owner);
    const VideoSettingsData& pending=VideoSettingsFixture::pending(owner);
    check(VideoSettingsFixture::refs(owner)==1 && api->getRef()==1,"refcount starts at one and queryInterface does not retain");
    check(api->queryInterface(IID_IBase)==object && api->queryInterface(IID_IVideoSettings)==object && !api->queryInterface(0xffffffff),
        "queryInterface accepts IBase and IVideoSettings only");
    check(!VideoSettings::create(IID_IVideoSettings,reinterpret_cast<IBase*>(1)),"aggregation is refused before allocation");
    check(!VideoSettings::create(0xffffffff,nullptr),"unsupported factory id releases the fresh owner");
    check(api->getFileName()=="Video.con","actual filename virtual");

    // constructed state
    static const int zeros[8]={0,0,0,0,0,0,0,0};
    check(isDefault(current) && isDefault(pending),"both records carry the record constructor's defaults");
    check(!api->hasChanges() && !VideoSettingsFixture::restart(owner),"no change and no restart after construction");
    check(rangesAre(api,zeros,zeros),"the sixteen range values start at zero (chosen; retail leaves them unwritten)");
    check(api->getSettingsScript()==defaultScript,"exact twelve-row script: unquoted strings, float scale");

    // accessors edit pending only
    api->setTerrainQuality(3); api->setGeometryQuality(4); api->setLightingQuality(5); api->setDynamicLightingQuality(0);
    api->setDynamicShadowsQuality(1); api->setEffectsQuality(6); api->setTextureQuality(7); api->setTextureFilteringQuality(8);
    api->setResolution(std::string("1024x768@85Hz")); api->setAntialiasing(std::string("4Samples")); api->setViewDistanceScale(0.5f); api->setVideoOptionScheme(2);
    check(api->getTerrainQuality()==3 && api->getGeometryQuality()==4 && api->getLightingQuality()==5 && api->getDynamicLightingQuality()==0
        && api->getDynamicShadowsQuality()==1 && api->getEffectsQuality()==6 && api->getTextureQuality()==7 && api->getTextureFilteringQuality()==8,
        "the eight quality pairs round-trip");
    check(api->getResolution()=="1024x768@85Hz" && api->getAntialiasing()=="4Samples" && api->getViewDistanceScale()==0.5f && api->getVideoOptionScheme()==2,
        "the string, float and scheme pairs round-trip");
    check(api->getSettingsScript()==defaultScript,"the script writes the current record, not the edited one");
    check(api->hasChanges() && isDefault(current),"edits leave current alone and register as changes");
    api->revertChanges();
    check(isDefault(pending) && !api->hasChanges(),"revertChanges copies current over pending");
    api->setCustomVideoOptionScheme();
    check(api->getVideoOptionScheme()==3,"the custom scheme is index 3");
    api->revertChanges();

    // no renderer object at all
    check(api->detectVideoOptionScheme()==1,"without g_bf2Render the detected scheme is 1");
    api->applyVideoOptionScheme(1); api->readOptions(nullptr);
    check(isDefault(pending),"without g_bf2Render the scheme push and the read-back are inert");
    check(!api->optionRequiresRestart(4) && api->getOptionMaxQuality(4)==0 && api->optionsSlot0x38(),"without g_bf2Render: no restart flag, ceiling 0, slot 0x38 true");
    api->setTextureQuality(3);
    api->setDefaults();
    check(isDefault(pending) && rangesAre(api,zeros,zeros),"setDefaults without g_bf2Render resets pending and leaves the ranges");
    api->setAntialiasing(std::string("4Samples"));
    check(api->needsRestart(),"an antialiasing change needs a restart before any renderer flag is consulted");
    api->revertChanges(); api->setTerrainQuality(3);
    check(!api->needsRestart(),"a quality change without a renderer flag needs no restart");
    check(!api->hasChangesInGame(),"no loaded game: hasChangesInGame is false despite changes");
    game=loadedGame; check(api->hasChangesInGame(),"loaded game with changes: hasChangesInGame is true"); game=nullptr;
    api->revertChanges();
    check(!api->needsRestart(),"no changes: needsRestart is false");

    // A renderer whose +0x74 hands back nothing: apply fails after latching the restart need and
    // asks the renderer twice (its own fetch and applyToRenderer's).
    g_bf2Render=reinterpret_cast<BF2Render*>(&fakeRender); fakeRender.renderer=reinterpret_cast<IBase*>(&fakeRenderer);
    rendererOptions=nullptr; slot74Calls=0;
    api->setAntialiasing(std::string("4Samples"));
    check(!api->apply(),"renderer without options: apply returns false");
    check(slot74Calls==2,"apply and applyToRenderer each fetch the options once");
    check(VideoSettingsFixture::restart(owner),"the restart need is latched before the options are fetched");
    check(isDefault(current) && pending.antialiasing=="4Samples","a failed apply changes neither record");
    api->revertChanges();

    // setDefaults with a renderer: ranges, detect ([CHOSEN] answers High whatever the renderer
    // detected), the preset with the highest antialiasing, read-back, then the resolution list, the
    // largest entry fitting the renderer's desktop mode, the antialiasing again and a read-back.
    OptionsObserver observer; rendererOptions=&observer; callLog.clear(); slot74Calls=0;
    observer.detected=0;
    api->setDefaults();
    const std::string maxAa="getDynamicOptionList 0;setDynamicOption 0=4Samples;";
    const std::string head=rangeReads()+"detectQualityLevel;applyQualityPreset 2;"+maxAa+readback()+"getDynamicOptionList 1;";
    const std::string log=joined();
    const std::string tail=log.compare(0,head.size(),head)==0 ? log.substr(head.size()) : std::string("<mismatch>");
    const bool picked=tail=="setDynamicOption 1=1024x768@85Hz;"+maxAa+readback();
    check(picked,"setDefaults: range reads, detect, High preset, read-back, resolution list, desktop pick");
    check(slot74Calls==6u,"setDefaults, detect, preset, read-back, list and pick read-back each fetch the options");
    check(rangesAre(api,observer.minimum,observer.level),"the range getters expose rows +08/+0C per index");
    check(pending.terrainQuality==2 && pending.textureFilteringQuality==2 && pending.antialiasing=="4Samples"
        && pending.resolution=="1024x768@85Hz" && pending.viewDistanceScale==0.75f && pending.videoOptionScheme==2,
        "pending mirrors the renderer after the preset");
    check(observer.presetApplied==2,"[CHOSEN] detect answers High (2) over the renderer's 0; the caller's 1 is not read");
    check(api->hasChanges(),"the renderer's values differ from current");

    // apply with a renderer
    static const int twos[8]={2,2,2,2,2,2,2,2};
    callLog.clear(); slot74Calls=0;
    check(api->apply(),"apply with options succeeds");
    check(joined()==pushes(twos,"1024x768@85Hz","4Samples")+"apply;"+readback(),"apply: rows 0..7, resolution, antialiasing, scale, renderer apply, read-back");
    check(slot74Calls==2,"apply fetches the options once and readOptions fetches them again");
    check(!current.differsFrom(pending) && !api->hasChanges() && observer.applies==1,"current takes pending after the read-back");
    check(current.resolution=="1024x768@85Hz" && api->getSettingsScript().find("VideoSettings.setResolution 1024x768@85Hz\n")!=std::string::npos,"the script now shows the applied record");
    // Loaded game: nothing is pushed, the renderer still applies, and the read-back overwrites the edit.
    game=loadedGame; api->setEffectsQuality(3); callLog.clear();
    check(api->apply(),"in-game apply succeeds");
    check(joined()==std::string("getRestartFlag 5;apply;")+readback(),"in-game apply: only the restart flag read, the renderer apply and the read-back");
    check(current.effectsQuality==2 && !api->hasChanges(),"the in-game edit is lost to the read-back (retail)");
    game=nullptr;
    // A named level in the settings repository skips only the antialiasing push.
    repository.stringSet(std::string("GSLoadLevel"),std::string("strike_at_karkand"));
    api->setAntialiasing(std::string("4Samples")); callLog.clear();
    check(api->apply(),"apply with a named level succeeds");
    check(joined()==pushes(twos,"1024x768@85Hz",nullptr)+"apply;"+readback(),"a named GSLoadLevel skips the antialiasing push only");
    check(observer.selected[0]=="2Samples" && current.antialiasing=="2Samples","the skipped antialiasing edit is lost to the read-back");
    repository.stringErase(std::string("GSLoadLevel"));

    // restart latching with a fresh owner
    IBase* second=VideoSettings::create(IID_IVideoSettings,nullptr);
    IVideoSettings* api2=static_cast<IVideoSettings*>(second);
    VideoSettings& owner2=VideoSettingsFixture::owner(second);
    observer.selected[1]="800x600@60Hz"; observer.selected[0]="Off"; observer.scale=1.0f;
    api2->setTextureQuality(3); callLog.clear();
    check(!api2->needsRestart() && joined()=="getRestartFlag 6;","only the changed row's flag is read");
    check(api2->apply() && !VideoSettingsFixture::restart(owner2),"an unflagged row change applies without a restart need");
    api2->setDynamicShadowsQuality(3);
    check(api2->needsRestart(),"a flagged row change (index 4) needs a restart");
    check(api2->apply() && VideoSettingsFixture::restart(owner2),"apply latches the restart need");
    api2->setDynamicShadowsQuality(2); api2->setDynamicLightingQuality(1); callLog.clear();
    check(api2->needsRestart() && joined()=="getRestartFlag 3;getRestartFlag 4;","flags are read in row order until one is set");

    // the renderer-backed getters and lists
    check(api->detectVideoOptionScheme()==2 && api->getOptionMaxQuality(7)==2 && api->getOptionMaxQuality(0)==3,"detect and the capped ceiling reach the renderer");
    check(api->optionRequiresRestart(4) && !api->optionRequiresRestart(3) && !api->optionsSlot0x38(),"the flag and slot 0x38 reach the renderer");
    check(api->getResolutionList()==observer.lists[1] && api->getAntialiasingList()==observer.lists[0],"the dynamic-option lists are copied out (1 resolution, 0 antialiasing)");
    check(api->getQualityLevelNames()==std::vector<std::string>{"Off","Low","Medium","High","UltraHigh"},"five quality level names");
    check(api->getVideoOptionSchemeNames()==std::vector<std::string>{"Low","Medium","High","Custom"},"four scheme names");

    // release
    api->addRef();
    check(api->release()==1,"non-final release keeps the owner");
    check(api->release()==0,"final release reports zero");
    check(api2->release()==0,"second owner released");
    std::printf("VideoSettings: %u/%u passed; native owner=%zu record=%zu\n",checks-failures,checks,sizeof(VideoSettings),sizeof(VideoSettingsData));
    return failures?1:0;
}
