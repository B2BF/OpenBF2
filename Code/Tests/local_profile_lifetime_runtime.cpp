// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: LOCAL-PROFILE-LIFETIME-FIXTURE]
// Actual owner/factory/record/destructor with typed ClassManager/EventManager observers.
// No production constructor, release, shutdown, interface or registry body is shadowed.
#include <windows.h>
#include <crtdbg.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include "../BF2/Game/LocalProfileManager.h"
#include "../BF2/Game/ProfileManager.h"
#include "../BF2/Game/AudioSettings.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "SwiffHostStandIn.h"
#include "../BF2/Game/GeneralSettings.h"
#include "GeneralSettingsStandIn.h"
#include "AudioSettingsStandIn.h"
#include "../BF2/Game/GameServerSettings.h"
#include "../BF2/Game/VideoSettings.h"
#include "../BF2/Game/ControlSettings.h"
#include "../BF2/Game/HapticSettings.h"
#include "../BF2/Game/GlobalSettings.h"
#include "../BF2/Game/DemoRecorder/DemoLibrary.h"
#include "../BF2/Game/GameServer/ServerSettings.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Game/MapList.h"
#include "../BF2/IO/Input/Input.h"
#include "../BF2/Game/PasswordCodec.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/IO/File/FileManager.h"
#include "../BF2/IO/Console/Console.h"

static unsigned seamCalls;
static int seamType, seamFlag;
static std::string seamText;
static dice::hfe::IBase* seamHost;
namespace dice { namespace hfe {
ClassManager* classManager = nullptr;
EventManager* eventManager = nullptr;
IBase* g_audioSettings = nullptr;
// [TEMP] The other default-profile singleton slots BF2Engine.cpp defines. The linked
// seam frontiers stop at the first of them before any null pointer is used.
IBase* g_gameServerSettings = nullptr; IBase* g_generalSettings = nullptr; IBase* g_videoSettings = nullptr;
IBase* g_controlSettings = nullptr; IBase* g_hapticSettings = nullptr;
IBase* g_globalSettings = nullptr;
IGame* game = nullptr;
// The real settings repository (constructed in main) receives the named load's GSPlayerName.
SettingsRepostitory* settings = nullptr;
// [TEMP] The named load's one ServerSettings write is `m_configFile`. Linking the real unit would
// drag the AI instance, game logic and server-info globals into this focused executable, so the
// object is a zeroed block of the class's own size in which main constructs only that member;
// the compiler's own layout places the write, no offset is assumed.
static unsigned char serverSettingsStorage[sizeof(ServerSettings)];
ServerSettings* serverSettings = reinterpret_cast<ServerSettings*>(serverSettingsStorage);
// [TEMP] The typed SwiffHost stand-in: postMessage records the {type, text, code} record
// reportProfileError posts (+0x1E4) and validateProfileName records its arguments and answers a
// canned refusal (+0x26C). resetStatsCache observes shutdown's recovered +0x2EC call.
// Every other slot aborts - the named load's list reset (+0x7C, then the
// ServerBrowser seam) stays a diagnostic stop and is not driven here.
struct LifetimeSwiffHost : SwiffHostStandIn {
    std::string validatedName; unsigned validations=0; bool validationAnswer=false;
    unsigned statsResets=0; std::function<void()> onStatsReset;
    bool postMessage(const SwiffHostMessage& m) override
    { ++seamCalls; seamHost=this; seamType=m.type; seamText=m.text; seamFlag=m.code; return true; }
    bool validateProfileName(const std::string& name,std::string& error) override
    { ++validations; validatedName=name; if(validationAnswer) return true; error="hostrefused"; return false; }
    void resetStatsCache() override
    { ++statsResets; if(onStatsReset)onStatsReset(); }
};
static LifetimeSwiffHost swiffHostObserver;
namespace io {
FileManager* fileManager = nullptr;
IBase* controlMapManager = nullptr; // a typed observer in main: the named save applies it, the named load asks for its active map
#ifndef BF2_PROFILE_DISCOVERY_FIXTURE
// Link-only globals for unexercised external dependencies in the strict gate.
// Cached-profile tests never dereference this null console/filesystem.
OldConsole* mainConsole = nullptr;
std::string Console::ignoredString_;
#endif
}
} }
using namespace dice::hfe;
static unsigned checks, failures, freedOwners;
static void* watchedOwner;
static std::vector<void*> watchedStrings;
static std::vector<unsigned> freedStrings;
static bool watchStrings;
static bool captureFactoryAllocation;
static LocalProfileManager* factoryOwner;
void* operator new(std::size_t size)
{
    void* p=std::malloc(size?size:1); if(!p) throw std::bad_alloc();
    if(captureFactoryAllocation && size==sizeof(LocalProfileManager)) {
        factoryOwner=static_cast<LocalProfileManager*>(p); watchedOwner=p; captureFactoryAllocation=false;
    }
    return p;
}
void operator delete(void* p) noexcept
{
    if(p && p==watchedOwner) ++freedOwners;
    if(watchStrings && p) for(unsigned i=0;i<watchedStrings.size();++i)
        if(p==watchedStrings[i]) { freedStrings.push_back(i); break; }
    std::free(p);
}
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
static void check(bool pass,const char* label)
{ ++checks; if(!pass) { ++failures; std::printf("FAIL %s\n",label); } }

namespace dice { namespace hfe {
struct LocalProfileFixture {
    static bool fresh(const LocalProfileManager& m)
    { return m.m_refCount==1 && !m.m_currentProfile && m.m_demoBookmarks.empty() && !m.m_hasDefaultProfile
        && m.m_profilesDirty && !m.m_filterValidProfiles && m.m_profiles.empty() && m.m_field34==-1
        && m.m_field38.empty() && !m.m_field54; }
    static bool resetBeforeUnregister(const LocalProfileManager& m)
    { return !m.m_currentProfile && m.m_field34==-1 && m.m_field38.empty(); }
    static bool dirty(const LocalProfileManager& m) { return m.m_profilesDirty; }
    static bool seeded(const LocalProfileManager& m)
    { return m.m_currentProfile && m.m_currentProfile->name==std::string(70,'a')
        && m.m_field34==37 && m.m_field38==std::string(70,'c') && !m.m_profilesDirty
        && m.m_profiles.size()==1 && m.m_demoBookmarks.size()==1; }
    static void setDirty(LocalProfileManager& m,bool value) { m.m_profilesDirty=value; }
    static void setGamespyNick(LocalProfileManager& m,const std::string& value)
    { m.m_currentProfile->gamespyNick=value; }
    static LocalProfile* current(LocalProfileManager& m) { return m.m_currentProfile; }
    static const std::vector<LocalProfile>& profiles(const LocalProfileManager& m) { return m.m_profiles; }
    static bool filter(const LocalProfileManager& m) { return m.m_filterValidProfiles; }
    static bool defaultFlag(const LocalProfileManager& m) { return m.m_hasDefaultProfile; }
    static void setDefaultFlag(LocalProfileManager& m,bool value) { m.m_hasDefaultProfile=value; }
    static void setSaveGlobalFlag(LocalProfileManager& m,bool value) { m.m_field54=value; }
    static bool saveGlobalFlag(const LocalProfileManager& m) { return m.m_field54; }
    static int loginIndex(const LocalProfileManager& m) { return m.m_field34; }
    static const std::string& loginPassword(const LocalProfileManager& m) { return m.m_field38; }
    static std::size_t bookmarkCapacity(const LocalProfileManager& m) { return m.m_demoBookmarks.capacity(); }
    static void cache(LocalProfileManager& m,const std::vector<LocalProfile>& values,bool filter)
    { m.m_profiles=values; m.m_profilesDirty=false; m.m_filterValidProfiles=filter; }
    static void seed(LocalProfileManager& m)
    {
        m.m_currentProfile=new LocalProfile;
        m.m_currentProfile->name.assign(70,'a');
        m.m_currentProfile->password.assign(70,'b');
        m.m_field34=37; m.m_field38.assign(70,'c'); m.m_profilesDirty=false;
        m.m_profiles.resize(1); m.m_profiles[0].name.assign(70,'d');
        m.m_profiles[0].password.assign(70,'e');
        m.m_demoBookmarks.resize(1); m.m_demoBookmarks[0].field00.assign(70,'f');
        m.m_demoBookmarks[0].field54.assign(70,'g');
        watchedStrings={const_cast<char*>(m.m_currentProfile->password.data()),
            const_cast<char*>(m.m_currentProfile->name.data()), const_cast<char*>(m.m_field38.data()),
            const_cast<char*>(m.m_profiles[0].password.data()),const_cast<char*>(m.m_profiles[0].name.data()),
            const_cast<char*>(m.m_demoBookmarks[0].field54.data()),const_cast<char*>(m.m_demoBookmarks[0].field00.data())};
    }
};
} }

static IBase* demoLibraryObserver=nullptr; // the DemoLibrary observer main installs
// [TEMP] Actual typed online interface; only shutdown's logout callback is observed.
// Every other method aborts instead of providing invented account behavior.
struct LifetimeOnlineProfile : IProfileManager {
    unsigned logouts=0; std::function<void()> onLogout;
    unsigned logins=0; std::string lastNick,lastPassword; bool loginAnswer=false;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    bool createOnlineAccount(const std::string&,const std::string&,const std::string&,const std::string&) override { std::abort(); }
    bool login(const std::string& nick,const std::string& password) override
    { ++logins; lastNick=nick; lastPassword=password; return loginAnswer; }
    bool loginAccount(const std::string&,const std::string&,const std::string&) override { std::abort(); }
    bool loginAlternate(const std::string&,const std::string&) override { std::abort(); }
    bool createProfile(const std::string&,const std::string&) override { std::abort(); }
    bool selectProfile(const std::string&) override { std::abort(); }
    void logout() override { ++logouts; if(onLogout)onLogout(); }
    OnlineProfile* getCurrentProfile() override { std::abort(); }
    bool getProfileId(int&) override { std::abort(); }
    bool getUniqueNick(std::string&) override { std::abort(); }
    bool deleteProfile() override { std::abort(); }
    bool getProfileInformation(int,ProfileInformation&) override { std::abort(); }
    bool setProfileInformation(const ProfileInformation&) override { std::abort(); }
    bool isLoggedIn() override { std::abort(); }
    bool update() override { std::abort(); }
    bool requestNickSuggestions(const std::string&) override { std::abort(); }
    bool getNickSuggestions(std::vector<std::string>&) override { std::abort(); }
    bool validateProfileName(const std::string&,std::string&) override { std::abort(); }
    bool hasPendingTransition(std::string&) override { std::abort(); }
    void setPendingTransition(bool,const std::string&) override { std::abort(); }
};
struct Registry : ClassManager {
    unsigned queries=0, swiffQueries=0; IProfileManager* online=nullptr; bool swiff=false;
    ui::ISwiffHost* shutdownHost=nullptr;
    std::vector<std::string>* shutdownTrace=nullptr; std::string traceName;
    IBase* getSingleton(const std::string& name) override
    {
        ++queries;
        if(shutdownTrace)shutdownTrace->push_back(traceName+":"+name);
        if(name=="SwiffHost") { ++swiffQueries; return swiff?static_cast<IBase*>(shutdownHost?shutdownHost:&swiffHostObserver):nullptr; }
        if(name=="DemoLibrary") return demoLibraryObserver; // login refreshes it typed
        check(name=="ProfileManager","shutdown looks up actual online singleton name");
        return online;
    }
};
struct Events : EventManager {
    unsigned removals=0; LocalProfileManager* current=nullptr;
    std::vector<std::string>* shutdownTrace=nullptr;
    bool unregisterEventHandler(EventCategory category,IEventListener* listener) override
    {
        if(shutdownTrace)shutdownTrace->push_back("unregister");
        ++removals; check(category==ECGame,"shutdown category is ECGame");
        LocalProfileManager* owner=current?current:factoryOwner;
        check(owner && listener==static_cast<IEventListener*>(owner),"actual adjusted secondary listener pointer");
        check(LocalProfileFixture::resetBeforeUnregister(*owner),"current record/string/index reset before callback");
        LocalProfileFixture::setDirty(*owner,false);
        return EventManager::unregisterEventHandler(category,listener);
    }
};

static void onlineShutdownTests(Events& events,Registry& registry)
{
    // Full PE4A81F0: online lookup -> logout -> reload ClassManager -> fresh host
    // lookup -> conditional resetStatsCache -> local reset -> unregister -> dirty.
    // Changing the registry during logout distinguishes a fresh lookup from reuse.
    for(unsigned scenario=0;scenario!=3;++scenario) {
        LocalProfileManager manager; events.current=&manager;
        LocalProfileFixture::seed(manager);
        LifetimeOnlineProfile online;
        LifetimeSwiffHost freshHost;
        Registry freshRegistry;
        std::vector<std::string> trace;
        registry.traceName="initial"; registry.shutdownTrace=&trace;
        registry.swiff=true; registry.online=scenario?&online:nullptr;
        freshRegistry.traceName="fresh"; freshRegistry.shutdownTrace=&trace;
        freshRegistry.swiff=scenario==1; freshRegistry.shutdownHost=&freshHost;
        events.shutdownTrace=&trace;
        const unsigned originalResets=swiffHostObserver.statsResets;
        bool repeated=false;
        const auto checkBeforeReset=[&]() {
            check(repeated?LocalProfileFixture::resetBeforeUnregister(manager):LocalProfileFixture::seeded(manager),
                "external shutdown callbacks precede local record/index/string changes");
        };
        online.onLogout=[&]() {
            trace.push_back("logout"); checkBeforeReset();
            classManager=&freshRegistry;
        };
        freshHost.onStatsReset=[&]() {
            trace.push_back("resetStatsCache"); checkBeforeReset();
        };
        ILocalProfileManager* prefix=&manager;
        prefix->shutdown();
        const std::vector<std::string> expected=scenario==0
            ?std::vector<std::string>{"initial:ProfileManager","unregister"}
            :scenario==1?std::vector<std::string>{"initial:ProfileManager","logout","fresh:SwiffHost","resetStatsCache","unregister"}
            :std::vector<std::string>{"initial:ProfileManager","logout","fresh:SwiffHost","unregister"};
        check(trace==expected,"shutdown follows online/null-host/fresh-host ordering");
        check(online.logouts==(scenario?1u:0u) && freshHost.statsResets==(scenario==1?1u:0u)
            && swiffHostObserver.statsResets==originalResets,"shutdown resets only the newly resolved nonnull host");
        check(LocalProfileFixture::resetBeforeUnregister(manager) && LocalProfileFixture::dirty(manager),
            "shutdown completes local teardown and writes dirty after unregister observer");
        if(scenario==1) {
            repeated=true; trace.clear(); classManager=&registry;
            prefix->shutdown();
            check(trace==expected && online.logouts==2 && freshHost.statsResets==2,
                "repeated shutdown repeats online logout and host reset even without local record");
            check(LocalProfileFixture::dirty(manager),"repeated shutdown restores dirty after unregister");
        }
        // Destructor still exercises actual shutdown, with ordinary absent-online observers.
        registry.online=nullptr; registry.swiff=false; registry.shutdownTrace=nullptr;
        events.shutdownTrace=nullptr; classManager=&registry;
    }
    events.current=nullptr;
}

// [TEMP]
// Typed AudioSettings observer for LocalProfile calls; unselected slots abort.
static std::vector<std::string> applyLog;
struct ProfileAudio : AudioSettingsStandIn {
    unsigned calls=0;
    bool result=false,lastFlag=true;
    std::function<void()> onApply;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    std::string getFileName() override { return "Audio.con"; }
    std::string getSettingsScript() override { return "AUDIO"; }
    bool apply(bool flag) override {
        ++calls;lastFlag=flag;applyLog.push_back("audio");
        check(!flag,"LocalProfile always passes false to AudioSettings apply");
        if(onApply)onApply();return result;
    }
    bool hasChanges() override { std::abort(); }
    unsigned reverts=0, defaultsCalls=0;
    void revertChanges() override { ++reverts; }
    void setDefaults() override { ++defaultsCalls; }
};
// [TEMP]
// Typed GeneralSettings observer for default-profile reset; unselected slots abort.
struct ProfileGeneral : GeneralSettingsStandIn {
    unsigned defaults=0; bool announce=false;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    std::string getFileName() override { return "General.con"; }
    std::string getSettingsScript() override { return "GENERAL"; }
    bool apply() override { applyLog.push_back("general"); return true; }
    void setDefaults() override
    {
        ++defaults;
        if(announce){ std::fprintf(stderr,"[TEMP] general setDefaults callback after the audio apply\n"); std::fflush(stderr); }
    }
    void setDefaultValues() override { std::abort(); }
};
// [TEMP] Typed observer of the GameServerSettings singleton createDefaultProfile resets and
// whose VoIP quality it sets to 5; the fifty-one accessor pairs abort except that setter.
#define GSS_PAIRS(X) \
    X(std::string,ServerName) X(std::string,Password) X(bool,Internet) X(int,BandwidthChoke) X(int,MaxPlayers) \
    X(std::string,GameMode) X(int,StartDelay) X(int,EndDelay) X(float,SpawnTime) X(float,ManDownTime) \
    X(int,TicketRatio) X(int,RoundsPerMap) X(int,TimeLimit) X(int,ScoreLimit) X(int,SoldierFF) X(int,VehicleFF) \
    X(int,SoldierSplashFF) X(int,VehicleSplashFF) X(bool,PunishTeamKills) X(int,VoteTime) X(int,MinPlayersForVoting) \
    X(bool,TeamVoteOnly) X(bool,VoipEnabled) X(int,VoipQuality) X(bool,VoipServerRemote) X(std::string,VoipServerRemoteIP) \
    X(int,VoipServerPort) X(int,VoipBFClientPort) X(int,VoipBFServerPort) X(std::string,VoipSharedPassword) X(int,GameSpyPort) \
    X(bool,AllowNATNegotiation) X(std::string,InterfaceIP) X(bool,AutoRecord) X(std::string,DemoIndexURL) X(std::string,DemoDownloadURL) \
    X(std::string,DemoHook) X(std::string,AdminScript) X(float,TimeBeforeRestartMap) X(std::string,SponsorText) X(std::string,SponsorLogoURL) \
    X(bool,AutoBalanceTeam) X(int,AutoRecordRounds) X(float,TeamRatio) X(bool,SvPunkBuster) X(std::string,Field1D8) \
    X(bool,FriendlyFireWithMines) X(float,CoopBotRatio) X(int,CoopBotCount) X(float,CoopBotDifficulty) X(bool,NoVehicles)
struct ProfileGameServer : IGameServerSettings {
    unsigned resets=0, voipSets=0; int voipQuality=-1; bool announce=false;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    std::string getFileName() override { return "ServerSettings.con"; }
    std::string getSettingsScript() override { return "SERVER"; }
    bool apply() override { applyLog.push_back("gameServer"); return true; }
    void reset() override
    {
        ++resets;
        if(announce){ std::fprintf(stderr,"[TEMP] game server settings reset callback after the general defaults\n"); std::fflush(stderr); }
    }
    void onSet(const char* name,int value)
    {
        if(std::strcmp(name,"VoipQuality")) std::abort();
        ++voipSets; voipQuality=value;
        if(announce){ std::fprintf(stderr,"[TEMP] game server settings voip quality %d after its reset\n",value); std::fflush(stderr); }
    }
    void onSet(const char*,bool) { std::abort(); }
    void onSet(const char*,float) { std::abort(); }
    void onSet(const char*,const std::string&) { std::abort(); }
#define GSS_OBSERVE(T,N) T get##N() override { std::abort(); } void set##N(T value) override { onSet(#N,value); }
    GSS_PAIRS(GSS_OBSERVE)
#undef GSS_OBSERVE
};
// [TEMP] Typed observer of the VideoSettings singleton createDefaultProfile resets through +20;
// every other slot of the 65-slot table aborts. No production VideoSettings body is linked here.
struct ProfileVideo : IVideoSettings {
    unsigned defaults=0; bool announce=false;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    std::string getFileName() override { return "Video.con"; }
    std::string getSettingsScript() override { return "VIDEO"; }
    bool hasChanges() override { std::abort(); }
    bool apply() override { applyLog.push_back("video"); return true; }
    void setDefaults() override
    {
        ++defaults;
        if(announce){ std::fprintf(stderr,"[TEMP] video settings setDefaults callback after the audio defaults\n"); std::fflush(stderr); }
    }
    void revertChanges() override { std::abort(); }
    void applyToOptions(IVideoOptions*) override { std::abort(); }
    int detectVideoOptionScheme() override { std::abort(); }
    void applyVideoOptionScheme(int) override { std::abort(); }
    void setCustomVideoOptionScheme() override { std::abort(); }
    bool hasChangesInGame() override { std::abort(); }
    bool needsRestart() override { std::abort(); }
#define VS_INT_PAIR(N) int get##N() override { std::abort(); } void set##N(int) override { std::abort(); }
#define VS_RANGE(N) int get##N##Min() override { std::abort(); } int get##N##Max() override { std::abort(); }
    VS_INT_PAIR(TerrainQuality) VS_INT_PAIR(GeometryQuality) VS_INT_PAIR(LightingQuality) VS_INT_PAIR(DynamicLightingQuality)
    VS_INT_PAIR(DynamicShadowsQuality) VS_INT_PAIR(EffectsQuality) VS_INT_PAIR(TextureQuality) VS_INT_PAIR(TextureFilteringQuality)
    std::string getResolution() override { std::abort(); } void setResolution(std::string) override { std::abort(); }
    std::string getAntialiasing() override { std::abort(); } void setAntialiasing(std::string) override { std::abort(); }
    float getViewDistanceScale() override { std::abort(); } void setViewDistanceScale(float) override { std::abort(); }
    VS_RANGE(TerrainQuality) VS_RANGE(GeometryQuality) VS_RANGE(LightingQuality) VS_RANGE(DynamicLightingQuality)
    VS_RANGE(DynamicShadowsQuality) VS_RANGE(EffectsQuality) VS_RANGE(TextureQuality) VS_RANGE(TextureFilteringQuality)
    VS_INT_PAIR(VideoOptionScheme)
#undef VS_INT_PAIR
#undef VS_RANGE
    bool optionRequiresRestart(int) override { std::abort(); }
    int getOptionMaxQuality(int) override { std::abort(); }
    std::vector<std::string> getResolutionList() override { std::abort(); }
    std::vector<std::string> getAntialiasingList() override { std::abort(); }
    std::vector<std::string> getQualityLevelNames() override { std::abort(); }
    std::vector<std::string> getVideoOptionSchemeNames() override { std::abort(); }
    void readOptions(IVideoOptions*) override { std::abort(); }
    bool optionsSlot0x38() override { std::abort(); }
};
// [TEMP] Typed observer of the ControlSettings singleton createDefaultProfile backs up through
// +1C; every other slot of the 38-slot table aborts. No production ControlSettings body is linked.
struct ProfileControl : IControlSettings {
    unsigned backups=0,resets=0,defaults=0; bool announce=false;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    std::string getFileName() override { return "Controls.con"; }
    std::string getSettingsScript() override { return "CONTROL"; }
    bool restoreControlMaps() override { std::abort(); }
    void backupControlMaps() override
    {
        ++backups;
        if(announce){ std::fprintf(stderr,"[TEMP] control settings backup callback after the video defaults\n"); std::fflush(stderr); }
    }
    void backupControlMapsByName() override { std::abort(); }
    bool setDefaults() override { ++defaults; return true; }   // +24, createProfile 4AAACA
    void resetControlMaps() override { ++resets; }
    void updateInputDevices() override { std::abort(); }
    bool mapNextInput(int,bool,int,bool,bool) override { std::abort(); }
    bool selectControlMap(const std::string&) override { std::abort(); }
    bool getBindingDescription(io::IControlMap*,int,bool,bool,bool,std::string&,std::wstring&) override { std::abort(); }
    std::string getActiveControlMapName() override { std::abort(); }
#define CS_FLOAT_PAIR(G,S) float G() override { std::abort(); } void S(float) override { std::abort(); }
    CS_FLOAT_PAIR(getMouseSensitivity,setMouseSensitivity) CS_FLOAT_PAIR(getSlot48,setSlot4C) CS_FLOAT_PAIR(getSlot50,setSlot54)
    CS_FLOAT_PAIR(getYawFactor,setYawFactor) CS_FLOAT_PAIR(getPitchFactor,setPitchFactor)
    bool getInvertMouse() override { std::abort(); } void setInvertMouse(bool) override { std::abort(); }
    CS_FLOAT_PAIR(getKeyboardSensitivity,setKeyboardSensitivity)
#undef CS_FLOAT_PAIR
    bool apply() override { std::abort(); }
    void deleteControlMapBackups() override { std::abort(); }
    void activateMouse() override { std::abort(); }
    void deactivateMouse() override { std::abort(); }
    bool isMappingInput() override { std::abort(); }
    void setMappingInput(bool) override { std::abort(); }
    std::string getMappedInputName() override { std::abort(); }
};
// [TEMP] Typed observer of the HapticSettings singleton createDefaultProfile resets through +1C;
// every other slot of the 510-slot table aborts. No production HapticSettings body is linked here.
struct ProfileHaptic : IHapticSettings {
    unsigned defaults=0; bool announce=false;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    std::string getFileName() override { return "Haptic.con"; }
    std::string getSettingsScript() override { return "HAPTIC"; }
    bool apply() override { applyLog.push_back("haptic"); return true; }
    void setDefaults() override
    {
        ++defaults;
        if(announce){ std::fprintf(stderr,"[TEMP] haptic settings setDefaults callback after the control backup\n"); std::fflush(stderr); }
    }
    void setDefaultValues() override { std::abort(); }
    void loadCurrentProfileFile() override { std::abort(); }
    bool reloadCurrentProfileFile() override { std::abort(); }
    float getForceCapX() override { std::abort(); }
    void setForceCapX(float) override { std::abort(); }
    float getForceCapY() override { std::abort(); }
    void setForceCapY(float) override { std::abort(); }
    float getForceCapZ() override { std::abort(); }
    void setForceCapZ(float) override { std::abort(); }
    Vec3 getControlBoxLength() override { std::abort(); }
    void setControlBoxLength(float, float, float) override { std::abort(); }
    Vec3 getControlBoxStiffness() override { std::abort(); }
    void setControlBoxStiffness(float, float, float) override { std::abort(); }
    Vec3 getControlBoxLengthSOLDIER() override { std::abort(); }
    void setControlBoxLengthSOLDIER(float, float, float) override { std::abort(); }
    Vec3 getControlBoxStiffnessSOLDIER() override { std::abort(); }
    void setControlBoxStiffnessSOLDIER(float, float, float) override { std::abort(); }
    Vec3 getControlBoxLengthAIR() override { std::abort(); }
    void setControlBoxLengthAIR(float, float, float) override { std::abort(); }
    Vec3 getControlBoxStiffnessAIR() override { std::abort(); }
    void setControlBoxStiffnessAIR(float, float, float) override { std::abort(); }
    Vec3 getControlBoxLengthHELI() override { std::abort(); }
    void setControlBoxLengthHELI(float, float, float) override { std::abort(); }
    Vec3 getControlBoxStiffnessHELI() override { std::abort(); }
    void setControlBoxStiffnessHELI(float, float, float) override { std::abort(); }
    Vec3 getControlBoxLengthLAND() override { std::abort(); }
    void setControlBoxLengthLAND(float, float, float) override { std::abort(); }
    Vec3 getControlBoxStiffnessLAND() override { std::abort(); }
    void setControlBoxStiffnessLAND(float, float, float) override { std::abort(); }
    Vec3 getControlBoxLengthSEA() override { std::abort(); }
    void setControlBoxLengthSEA(float, float, float) override { std::abort(); }
    Vec3 getControlBoxStiffnessSEA() override { std::abort(); }
    void setControlBoxStiffnessSEA(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivity() override { std::abort(); }
    void setAimSensitivity(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivityLength() override { std::abort(); }
    void setAimSensitivityLength(float, float, float) override { std::abort(); }
    Vec3 getTurnSensitivity() override { std::abort(); }
    void setTurnSensitivity(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivitySOLDIER() override { std::abort(); }
    void setAimSensitivitySOLDIER(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivityLengthSOLDIER() override { std::abort(); }
    void setAimSensitivityLengthSOLDIER(float, float, float) override { std::abort(); }
    Vec3 getTurnSensitivitySOLDIER() override { std::abort(); }
    void setTurnSensitivitySOLDIER(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivityAIR() override { std::abort(); }
    void setAimSensitivityAIR(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivityLengthAIR() override { std::abort(); }
    void setAimSensitivityLengthAIR(float, float, float) override { std::abort(); }
    Vec3 getTurnSensitivityAIR() override { std::abort(); }
    void setTurnSensitivityAIR(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivityHELI() override { std::abort(); }
    void setAimSensitivityHELI(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivityLengthHELI() override { std::abort(); }
    void setAimSensitivityLengthHELI(float, float, float) override { std::abort(); }
    Vec3 getTurnSensitivityHELI() override { std::abort(); }
    void setTurnSensitivityHELI(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivityLAND() override { std::abort(); }
    void setAimSensitivityLAND(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivityLengthLAND() override { std::abort(); }
    void setAimSensitivityLengthLAND(float, float, float) override { std::abort(); }
    Vec3 getTurnSensitivityLAND() override { std::abort(); }
    void setTurnSensitivityLAND(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivitySEA() override { std::abort(); }
    void setAimSensitivitySEA(float, float, float) override { std::abort(); }
    Vec3 getAimSensitivityLengthSEA() override { std::abort(); }
    void setAimSensitivityLengthSEA(float, float, float) override { std::abort(); }
    Vec3 getTurnSensitivitySEA() override { std::abort(); }
    void setTurnSensitivitySEA(float, float, float) override { std::abort(); }
    Vec3 getPhysicsSOLDIERScale() override { std::abort(); }
    void setPhysicsSOLDIERScale(float, float, float) override { std::abort(); }
    float getPhysicsSOLDIERScaleXLow() override { std::abort(); }
    void setPhysicsSOLDIERScaleXLow(float) override { std::abort(); }
    float getPhysicsSOLDIERScaleYLow() override { std::abort(); }
    void setPhysicsSOLDIERScaleYLow(float) override { std::abort(); }
    float getPhysicsSOLDIERScaleZLow() override { std::abort(); }
    void setPhysicsSOLDIERScaleZLow(float) override { std::abort(); }
    float getPhysicsSOLDIERScaleXMed() override { std::abort(); }
    void setPhysicsSOLDIERScaleXMed(float) override { std::abort(); }
    float getPhysicsSOLDIERScaleYMed() override { std::abort(); }
    void setPhysicsSOLDIERScaleYMed(float) override { std::abort(); }
    float getPhysicsSOLDIERScaleZMed() override { std::abort(); }
    void setPhysicsSOLDIERScaleZMed(float) override { std::abort(); }
    float getPhysicsSOLDIERScaleXHigh() override { std::abort(); }
    void setPhysicsSOLDIERScaleXHigh(float) override { std::abort(); }
    float getPhysicsSOLDIERScaleYHigh() override { std::abort(); }
    void setPhysicsSOLDIERScaleYHigh(float) override { std::abort(); }
    float getPhysicsSOLDIERScaleZHigh() override { std::abort(); }
    void setPhysicsSOLDIERScaleZHigh(float) override { std::abort(); }
    float getPhysicsSOLDIERTransitionValue(int) override { std::abort(); }
    void setPhysicsSOLDIERTransitionValue(int, float) override { std::abort(); }
    float getPhysicsSOLDIERTransitionValueMed() override { std::abort(); }
    void setPhysicsSOLDIERTransitionValueMed(float) override { std::abort(); }
    float getPhysicsSOLDIERTransitionValueHigh() override { std::abort(); }
    void setPhysicsSOLDIERTransitionValueHigh(float) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeX(int) override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeX(int, int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeY(int) override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeY(int, int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeZ(int) override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeZ(int, int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeXLow() override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeXLow(int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeYLow() override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeYLow(int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeZLow() override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeZLow(int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeXMed() override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeXMed(int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeYMed() override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeYMed(int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeZMed() override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeZMed(int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeXHigh() override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeXHigh(int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeYHigh() override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeYHigh(int) override { std::abort(); }
    int getPhysicsSOLDIERSmoothTimeZHigh() override { std::abort(); }
    void setPhysicsSOLDIERSmoothTimeZHigh(int) override { std::abort(); }
    Vec3 getPhysicsLANDScale() override { std::abort(); }
    void setPhysicsLANDScale(float, float, float) override { std::abort(); }
    float getPhysicsLANDScaleXLow() override { std::abort(); }
    void setPhysicsLANDScaleXLow(float) override { std::abort(); }
    float getPhysicsLANDScaleYLow() override { std::abort(); }
    void setPhysicsLANDScaleYLow(float) override { std::abort(); }
    float getPhysicsLANDScaleZLow() override { std::abort(); }
    void setPhysicsLANDScaleZLow(float) override { std::abort(); }
    float getPhysicsLANDScaleXMed() override { std::abort(); }
    void setPhysicsLANDScaleXMed(float) override { std::abort(); }
    float getPhysicsLANDScaleYMed() override { std::abort(); }
    void setPhysicsLANDScaleYMed(float) override { std::abort(); }
    float getPhysicsLANDScaleZMed() override { std::abort(); }
    void setPhysicsLANDScaleZMed(float) override { std::abort(); }
    float getPhysicsLANDScaleXHigh() override { std::abort(); }
    void setPhysicsLANDScaleXHigh(float) override { std::abort(); }
    float getPhysicsLANDScaleYHigh() override { std::abort(); }
    void setPhysicsLANDScaleYHigh(float) override { std::abort(); }
    float getPhysicsLANDScaleZHigh() override { std::abort(); }
    void setPhysicsLANDScaleZHigh(float) override { std::abort(); }
    float getPhysicsLANDTransitionValue(int) override { std::abort(); }
    void setPhysicsLANDTransitionValue(int, float) override { std::abort(); }
    float getPhysicsLANDTransitionValueMed() override { std::abort(); }
    void setPhysicsLANDTransitionValueMed(float) override { std::abort(); }
    float getPhysicsLANDTransitionValueHigh() override { std::abort(); }
    void setPhysicsLANDTransitionValueHigh(float) override { std::abort(); }
    int getPhysicsLANDSmoothTimeX(int) override { std::abort(); }
    void setPhysicsLANDSmoothTimeX(int, int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeY(int) override { std::abort(); }
    void setPhysicsLANDSmoothTimeY(int, int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeZ(int) override { std::abort(); }
    void setPhysicsLANDSmoothTimeZ(int, int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeXLow() override { std::abort(); }
    void setPhysicsLANDSmoothTimeXLow(int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeYLow() override { std::abort(); }
    void setPhysicsLANDSmoothTimeYLow(int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeZLow() override { std::abort(); }
    void setPhysicsLANDSmoothTimeZLow(int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeXMed() override { std::abort(); }
    void setPhysicsLANDSmoothTimeXMed(int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeYMed() override { std::abort(); }
    void setPhysicsLANDSmoothTimeYMed(int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeZMed() override { std::abort(); }
    void setPhysicsLANDSmoothTimeZMed(int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeXHigh() override { std::abort(); }
    void setPhysicsLANDSmoothTimeXHigh(int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeYHigh() override { std::abort(); }
    void setPhysicsLANDSmoothTimeYHigh(int) override { std::abort(); }
    int getPhysicsLANDSmoothTimeZHigh() override { std::abort(); }
    void setPhysicsLANDSmoothTimeZHigh(int) override { std::abort(); }
    Vec3 getPhysicsAIRScale() override { std::abort(); }
    void setPhysicsAIRScale(float, float, float) override { std::abort(); }
    float getPhysicsAIRScaleXLow() override { std::abort(); }
    void setPhysicsAIRScaleXLow(float) override { std::abort(); }
    float getPhysicsAIRScaleYLow() override { std::abort(); }
    void setPhysicsAIRScaleYLow(float) override { std::abort(); }
    float getPhysicsAIRScaleZLow() override { std::abort(); }
    void setPhysicsAIRScaleZLow(float) override { std::abort(); }
    float getPhysicsAIRScaleXMed() override { std::abort(); }
    void setPhysicsAIRScaleXMed(float) override { std::abort(); }
    float getPhysicsAIRScaleYMed() override { std::abort(); }
    void setPhysicsAIRScaleYMed(float) override { std::abort(); }
    float getPhysicsAIRScaleZMed() override { std::abort(); }
    void setPhysicsAIRScaleZMed(float) override { std::abort(); }
    float getPhysicsAIRScaleXHigh() override { std::abort(); }
    void setPhysicsAIRScaleXHigh(float) override { std::abort(); }
    float getPhysicsAIRScaleYHigh() override { std::abort(); }
    void setPhysicsAIRScaleYHigh(float) override { std::abort(); }
    float getPhysicsAIRScaleZHigh() override { std::abort(); }
    void setPhysicsAIRScaleZHigh(float) override { std::abort(); }
    float getPhysicsAIRTransitionValue(int) override { std::abort(); }
    void setPhysicsAIRTransitionValue(int, float) override { std::abort(); }
    float getPhysicsAIRTransitionValueMed() override { std::abort(); }
    void setPhysicsAIRTransitionValueMed(float) override { std::abort(); }
    float getPhysicsAIRTransitionValueHigh() override { std::abort(); }
    void setPhysicsAIRTransitionValueHigh(float) override { std::abort(); }
    int getPhysicsAIRSmoothTimeX(int) override { std::abort(); }
    void setPhysicsAIRSmoothTimeX(int, int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeY(int) override { std::abort(); }
    void setPhysicsAIRSmoothTimeY(int, int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeZ(int) override { std::abort(); }
    void setPhysicsAIRSmoothTimeZ(int, int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeXLow() override { std::abort(); }
    void setPhysicsAIRSmoothTimeXLow(int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeYLow() override { std::abort(); }
    void setPhysicsAIRSmoothTimeYLow(int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeZLow() override { std::abort(); }
    void setPhysicsAIRSmoothTimeZLow(int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeXMed() override { std::abort(); }
    void setPhysicsAIRSmoothTimeXMed(int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeYMed() override { std::abort(); }
    void setPhysicsAIRSmoothTimeYMed(int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeZMed() override { std::abort(); }
    void setPhysicsAIRSmoothTimeZMed(int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeXHigh() override { std::abort(); }
    void setPhysicsAIRSmoothTimeXHigh(int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeYHigh() override { std::abort(); }
    void setPhysicsAIRSmoothTimeYHigh(int) override { std::abort(); }
    int getPhysicsAIRSmoothTimeZHigh() override { std::abort(); }
    void setPhysicsAIRSmoothTimeZHigh(int) override { std::abort(); }
    Vec3 getPhysicsHELIScale() override { std::abort(); }
    void setPhysicsHELIScale(float, float, float) override { std::abort(); }
    float getPhysicsHELIScaleXLow() override { std::abort(); }
    void setPhysicsHELIScaleXLow(float) override { std::abort(); }
    float getPhysicsHELIScaleYLow() override { std::abort(); }
    void setPhysicsHELIScaleYLow(float) override { std::abort(); }
    float getPhysicsHELIScaleZLow() override { std::abort(); }
    void setPhysicsHELIScaleZLow(float) override { std::abort(); }
    float getPhysicsHELIScaleXMed() override { std::abort(); }
    void setPhysicsHELIScaleXMed(float) override { std::abort(); }
    float getPhysicsHELIScaleYMed() override { std::abort(); }
    void setPhysicsHELIScaleYMed(float) override { std::abort(); }
    float getPhysicsHELIScaleZMed() override { std::abort(); }
    void setPhysicsHELIScaleZMed(float) override { std::abort(); }
    float getPhysicsHELIScaleXHigh() override { std::abort(); }
    void setPhysicsHELIScaleXHigh(float) override { std::abort(); }
    float getPhysicsHELIScaleYHigh() override { std::abort(); }
    void setPhysicsHELIScaleYHigh(float) override { std::abort(); }
    float getPhysicsHELIScaleZHigh() override { std::abort(); }
    void setPhysicsHELIScaleZHigh(float) override { std::abort(); }
    float getPhysicsHELITransitionValue(int) override { std::abort(); }
    void setPhysicsHELITransitionValue(int, float) override { std::abort(); }
    float getPhysicsHELITransitionValueMed() override { std::abort(); }
    void setPhysicsHELITransitionValueMed(float) override { std::abort(); }
    float getPhysicsHELITransitionValueHigh() override { std::abort(); }
    void setPhysicsHELITransitionValueHigh(float) override { std::abort(); }
    int getPhysicsHELISmoothTimeX(int) override { std::abort(); }
    void setPhysicsHELISmoothTimeX(int, int) override { std::abort(); }
    int getPhysicsHELISmoothTimeY(int) override { std::abort(); }
    void setPhysicsHELISmoothTimeY(int, int) override { std::abort(); }
    int getPhysicsHELISmoothTimeZ(int) override { std::abort(); }
    void setPhysicsHELISmoothTimeZ(int, int) override { std::abort(); }
    int getPhysicsHELISmoothTimeXLow() override { std::abort(); }
    void setPhysicsHELISmoothTimeXLow(int) override { std::abort(); }
    int getPhysicsHELISmoothTimeYLow() override { std::abort(); }
    void setPhysicsHELISmoothTimeYLow(int) override { std::abort(); }
    int getPhysicsHELISmoothTimeZLow() override { std::abort(); }
    void setPhysicsHELISmoothTimeZLow(int) override { std::abort(); }
    int getPhysicsHELISmoothTimeXMed() override { std::abort(); }
    void setPhysicsHELISmoothTimeXMed(int) override { std::abort(); }
    int getPhysicsHELISmoothTimeYMed() override { std::abort(); }
    void setPhysicsHELISmoothTimeYMed(int) override { std::abort(); }
    int getPhysicsHELISmoothTimeZMed() override { std::abort(); }
    void setPhysicsHELISmoothTimeZMed(int) override { std::abort(); }
    int getPhysicsHELISmoothTimeXHigh() override { std::abort(); }
    void setPhysicsHELISmoothTimeXHigh(int) override { std::abort(); }
    int getPhysicsHELISmoothTimeYHigh() override { std::abort(); }
    void setPhysicsHELISmoothTimeYHigh(int) override { std::abort(); }
    int getPhysicsHELISmoothTimeZHigh() override { std::abort(); }
    void setPhysicsHELISmoothTimeZHigh(int) override { std::abort(); }
    Vec3 getPhysicsSEAScale() override { std::abort(); }
    void setPhysicsSEAScale(float, float, float) override { std::abort(); }
    float getPhysicsSEAScaleXLow() override { std::abort(); }
    void setPhysicsSEAScaleXLow(float) override { std::abort(); }
    float getPhysicsSEAScaleYLow() override { std::abort(); }
    void setPhysicsSEAScaleYLow(float) override { std::abort(); }
    float getPhysicsSEAScaleZLow() override { std::abort(); }
    void setPhysicsSEAScaleZLow(float) override { std::abort(); }
    float getPhysicsSEAScaleXMed() override { std::abort(); }
    void setPhysicsSEAScaleXMed(float) override { std::abort(); }
    float getPhysicsSEAScaleYMed() override { std::abort(); }
    void setPhysicsSEAScaleYMed(float) override { std::abort(); }
    float getPhysicsSEAScaleZMed() override { std::abort(); }
    void setPhysicsSEAScaleZMed(float) override { std::abort(); }
    float getPhysicsSEAScaleXHigh() override { std::abort(); }
    void setPhysicsSEAScaleXHigh(float) override { std::abort(); }
    float getPhysicsSEAScaleYHigh() override { std::abort(); }
    void setPhysicsSEAScaleYHigh(float) override { std::abort(); }
    float getPhysicsSEAScaleZHigh() override { std::abort(); }
    void setPhysicsSEAScaleZHigh(float) override { std::abort(); }
    float getPhysicsSEATransitionValue(int) override { std::abort(); }
    void setPhysicsSEATransitionValue(int, float) override { std::abort(); }
    float getPhysicsSEATransitionValueMed() override { std::abort(); }
    void setPhysicsSEATransitionValueMed(float) override { std::abort(); }
    float getPhysicsSEATransitionValueHigh() override { std::abort(); }
    void setPhysicsSEATransitionValueHigh(float) override { std::abort(); }
    int getPhysicsSEASmoothTimeX(int) override { std::abort(); }
    void setPhysicsSEASmoothTimeX(int, int) override { std::abort(); }
    int getPhysicsSEASmoothTimeY(int) override { std::abort(); }
    void setPhysicsSEASmoothTimeY(int, int) override { std::abort(); }
    int getPhysicsSEASmoothTimeZ(int) override { std::abort(); }
    void setPhysicsSEASmoothTimeZ(int, int) override { std::abort(); }
    int getPhysicsSEASmoothTimeXLow() override { std::abort(); }
    void setPhysicsSEASmoothTimeXLow(int) override { std::abort(); }
    int getPhysicsSEASmoothTimeYLow() override { std::abort(); }
    void setPhysicsSEASmoothTimeYLow(int) override { std::abort(); }
    int getPhysicsSEASmoothTimeZLow() override { std::abort(); }
    void setPhysicsSEASmoothTimeZLow(int) override { std::abort(); }
    int getPhysicsSEASmoothTimeXMed() override { std::abort(); }
    void setPhysicsSEASmoothTimeXMed(int) override { std::abort(); }
    int getPhysicsSEASmoothTimeYMed() override { std::abort(); }
    void setPhysicsSEASmoothTimeYMed(int) override { std::abort(); }
    int getPhysicsSEASmoothTimeZMed() override { std::abort(); }
    void setPhysicsSEASmoothTimeZMed(int) override { std::abort(); }
    int getPhysicsSEASmoothTimeXHigh() override { std::abort(); }
    void setPhysicsSEASmoothTimeXHigh(int) override { std::abort(); }
    int getPhysicsSEASmoothTimeYHigh() override { std::abort(); }
    void setPhysicsSEASmoothTimeYHigh(int) override { std::abort(); }
    int getPhysicsSEASmoothTimeZHigh() override { std::abort(); }
    void setPhysicsSEASmoothTimeZHigh(int) override { std::abort(); }
    float getDamageScale() override { std::abort(); }
    void setDamageScale(float) override { std::abort(); }
    float getRecoilPunchScale(int) override { std::abort(); }
    void setRecoilPunchScale(int, float) override { std::abort(); }
    float getRecoilPunchTime(int) override { std::abort(); }
    void setRecoilPunchTime(int, float) override { std::abort(); }
    float getRecoilYawScale(int) override { std::abort(); }
    void setRecoilYawScale(int, float) override { std::abort(); }
    float getRecoilPitchScale(int) override { std::abort(); }
    void setRecoilPitchScale(int, float) override { std::abort(); }
    float getRecoilPunchScaleRIFLE() override { std::abort(); }
    void setRecoilPunchScaleRIFLE(float) override { std::abort(); }
    float getRecoilPunchTimeRIFLE() override { std::abort(); }
    void setRecoilPunchTimeRIFLE(float) override { std::abort(); }
    float getRecoilYawScaleRIFLE() override { std::abort(); }
    void setRecoilYawScaleRIFLE(float) override { std::abort(); }
    float getRecoilPitchScaleRIFLE() override { std::abort(); }
    void setRecoilPitchScaleRIFLE(float) override { std::abort(); }
    float getRecoilPunchScaleRIFLELAUNCHER() override { std::abort(); }
    void setRecoilPunchScaleRIFLELAUNCHER(float) override { std::abort(); }
    float getRecoilPunchTimeRIFLELAUNCHER() override { std::abort(); }
    void setRecoilPunchTimeRIFLELAUNCHER(float) override { std::abort(); }
    float getRecoilYawScaleRIFLELAUNCHER() override { std::abort(); }
    void setRecoilYawScaleRIFLELAUNCHER(float) override { std::abort(); }
    float getRecoilPitchScaleRIFLELAUNCHER() override { std::abort(); }
    void setRecoilPitchScaleRIFLELAUNCHER(float) override { std::abort(); }
    float getRecoilPunchScaleCARBINE() override { std::abort(); }
    void setRecoilPunchScaleCARBINE(float) override { std::abort(); }
    float getRecoilPunchTimeCARBINE() override { std::abort(); }
    void setRecoilPunchTimeCARBINE(float) override { std::abort(); }
    float getRecoilYawScaleCARBINE() override { std::abort(); }
    void setRecoilYawScaleCARBINE(float) override { std::abort(); }
    float getRecoilPitchScaleCARBINE() override { std::abort(); }
    void setRecoilPitchScaleCARBINE(float) override { std::abort(); }
    float getRecoilPunchScaleLMG() override { std::abort(); }
    void setRecoilPunchScaleLMG(float) override { std::abort(); }
    float getRecoilPunchTimeLMG() override { std::abort(); }
    void setRecoilPunchTimeLMG(float) override { std::abort(); }
    float getRecoilYawScaleLMG() override { std::abort(); }
    void setRecoilYawScaleLMG(float) override { std::abort(); }
    float getRecoilPitchScaleLMG() override { std::abort(); }
    void setRecoilPitchScaleLMG(float) override { std::abort(); }
    float getRecoilPunchScaleSNIPER() override { std::abort(); }
    void setRecoilPunchScaleSNIPER(float) override { std::abort(); }
    float getRecoilPunchTimeSNIPER() override { std::abort(); }
    void setRecoilPunchTimeSNIPER(float) override { std::abort(); }
    float getRecoilYawScaleSNIPER() override { std::abort(); }
    void setRecoilYawScaleSNIPER(float) override { std::abort(); }
    float getRecoilPitchScaleSNIPER() override { std::abort(); }
    void setRecoilPitchScaleSNIPER(float) override { std::abort(); }
    float getRecoilPunchScalePISTOL() override { std::abort(); }
    void setRecoilPunchScalePISTOL(float) override { std::abort(); }
    float getRecoilPunchTimePISTOL() override { std::abort(); }
    void setRecoilPunchTimePISTOL(float) override { std::abort(); }
    float getRecoilYawScalePISTOL() override { std::abort(); }
    void setRecoilYawScalePISTOL(float) override { std::abort(); }
    float getRecoilPitchScalePISTOL() override { std::abort(); }
    void setRecoilPitchScalePISTOL(float) override { std::abort(); }
    float getRecoilPunchScaleLAUNCHER() override { std::abort(); }
    void setRecoilPunchScaleLAUNCHER(float) override { std::abort(); }
    float getRecoilPunchTimeLAUNCHER() override { std::abort(); }
    void setRecoilPunchTimeLAUNCHER(float) override { std::abort(); }
    float getRecoilYawScaleLAUNCHER() override { std::abort(); }
    void setRecoilYawScaleLAUNCHER(float) override { std::abort(); }
    float getRecoilPitchScaleLAUNCHER() override { std::abort(); }
    void setRecoilPitchScaleLAUNCHER(float) override { std::abort(); }
    float getRecoilPunchScaleSMG() override { std::abort(); }
    void setRecoilPunchScaleSMG(float) override { std::abort(); }
    float getRecoilPunchTimeSMG() override { std::abort(); }
    void setRecoilPunchTimeSMG(float) override { std::abort(); }
    float getRecoilYawScaleSMG() override { std::abort(); }
    void setRecoilYawScaleSMG(float) override { std::abort(); }
    float getRecoilPitchScaleSMG() override { std::abort(); }
    void setRecoilPitchScaleSMG(float) override { std::abort(); }
    float getRecoilPunchScaleSHOTGUN() override { std::abort(); }
    void setRecoilPunchScaleSHOTGUN(float) override { std::abort(); }
    float getRecoilPunchTimeSHOTGUN() override { std::abort(); }
    void setRecoilPunchTimeSHOTGUN(float) override { std::abort(); }
    float getRecoilYawScaleSHOTGUN() override { std::abort(); }
    void setRecoilYawScaleSHOTGUN(float) override { std::abort(); }
    float getRecoilPitchScaleSHOTGUN() override { std::abort(); }
    void setRecoilPitchScaleSHOTGUN(float) override { std::abort(); }
    float getRecoilPunchScaleKNIFE() override { std::abort(); }
    void setRecoilPunchScaleKNIFE(float) override { std::abort(); }
    float getRecoilPunchTimeKNIFE() override { std::abort(); }
    void setRecoilPunchTimeKNIFE(float) override { std::abort(); }
    float getRecoilYawScaleKNIFE() override { std::abort(); }
    void setRecoilYawScaleKNIFE(float) override { std::abort(); }
    float getRecoilPitchScaleKNIFE() override { std::abort(); }
    void setRecoilPitchScaleKNIFE(float) override { std::abort(); }
    float getRecoilPunchScaleDEFIB() override { std::abort(); }
    void setRecoilPunchScaleDEFIB(float) override { std::abort(); }
    float getRecoilPunchTimeDEFIB() override { std::abort(); }
    void setRecoilPunchTimeDEFIB(float) override { std::abort(); }
    float getRecoilYawScaleDEFIB() override { std::abort(); }
    void setRecoilYawScaleDEFIB(float) override { std::abort(); }
    float getRecoilPitchScaleDEFIB() override { std::abort(); }
    void setRecoilPitchScaleDEFIB(float) override { std::abort(); }
    float getRecoilPunchScaleDROP() override { std::abort(); }
    void setRecoilPunchScaleDROP(float) override { std::abort(); }
    float getRecoilPunchTimeDROP() override { std::abort(); }
    void setRecoilPunchTimeDROP(float) override { std::abort(); }
    float getRecoilYawScaleDROP() override { std::abort(); }
    void setRecoilYawScaleDROP(float) override { std::abort(); }
    float getRecoilPitchScaleDROP() override { std::abort(); }
    void setRecoilPitchScaleDROP(float) override { std::abort(); }
    float getRecoilPunchScaleTHROWN() override { std::abort(); }
    void setRecoilPunchScaleTHROWN(float) override { std::abort(); }
    float getRecoilPunchTimeTHROWN() override { std::abort(); }
    void setRecoilPunchTimeTHROWN(float) override { std::abort(); }
    float getRecoilYawScaleTHROWN() override { std::abort(); }
    void setRecoilYawScaleTHROWN(float) override { std::abort(); }
    float getRecoilPitchScaleTHROWN() override { std::abort(); }
    void setRecoilPitchScaleTHROWN(float) override { std::abort(); }
    float getRecoilPunchScaleV_LMG() override { std::abort(); }
    void setRecoilPunchScaleV_LMG(float) override { std::abort(); }
    float getRecoilPunchTimeV_LMG() override { std::abort(); }
    void setRecoilPunchTimeV_LMG(float) override { std::abort(); }
    float getRecoilYawScaleV_LMG() override { std::abort(); }
    void setRecoilYawScaleV_LMG(float) override { std::abort(); }
    float getRecoilPitchScaleV_LMG() override { std::abort(); }
    void setRecoilPitchScaleV_LMG(float) override { std::abort(); }
    float getRecoilPunchScaleV_HMG() override { std::abort(); }
    void setRecoilPunchScaleV_HMG(float) override { std::abort(); }
    float getRecoilPunchTimeV_HMG() override { std::abort(); }
    void setRecoilPunchTimeV_HMG(float) override { std::abort(); }
    float getRecoilYawScaleV_HMG() override { std::abort(); }
    void setRecoilYawScaleV_HMG(float) override { std::abort(); }
    float getRecoilPitchScaleV_HMG() override { std::abort(); }
    void setRecoilPitchScaleV_HMG(float) override { std::abort(); }
    float getRecoilPunchScaleV_HELIGUN() override { std::abort(); }
    void setRecoilPunchScaleV_HELIGUN(float) override { std::abort(); }
    float getRecoilPunchTimeV_HELIGUN() override { std::abort(); }
    void setRecoilPunchTimeV_HELIGUN(float) override { std::abort(); }
    float getRecoilYawScaleV_HELIGUN() override { std::abort(); }
    void setRecoilYawScaleV_HELIGUN(float) override { std::abort(); }
    float getRecoilPitchScaleV_HELIGUN() override { std::abort(); }
    void setRecoilPitchScaleV_HELIGUN(float) override { std::abort(); }
    float getRecoilPunchScaleV_AIRGUN() override { std::abort(); }
    void setRecoilPunchScaleV_AIRGUN(float) override { std::abort(); }
    float getRecoilPunchTimeV_AIRGUN() override { std::abort(); }
    void setRecoilPunchTimeV_AIRGUN(float) override { std::abort(); }
    float getRecoilYawScaleV_AIRGUN() override { std::abort(); }
    void setRecoilYawScaleV_AIRGUN(float) override { std::abort(); }
    float getRecoilPitchScaleV_AIRGUN() override { std::abort(); }
    void setRecoilPitchScaleV_AIRGUN(float) override { std::abort(); }
    float getRecoilPunchScaleV_MISSILE() override { std::abort(); }
    void setRecoilPunchScaleV_MISSILE(float) override { std::abort(); }
    float getRecoilPunchTimeV_MISSILE() override { std::abort(); }
    void setRecoilPunchTimeV_MISSILE(float) override { std::abort(); }
    float getRecoilYawScaleV_MISSILE() override { std::abort(); }
    void setRecoilYawScaleV_MISSILE(float) override { std::abort(); }
    float getRecoilPitchScaleV_MISSILE() override { std::abort(); }
    void setRecoilPitchScaleV_MISSILE(float) override { std::abort(); }
    float getRecoilPunchScaleV_ARMORCANNON() override { std::abort(); }
    void setRecoilPunchScaleV_ARMORCANNON(float) override { std::abort(); }
    float getRecoilPunchTimeV_ARMORCANNON() override { std::abort(); }
    void setRecoilPunchTimeV_ARMORCANNON(float) override { std::abort(); }
    float getRecoilYawScaleV_ARMORCANNON() override { std::abort(); }
    void setRecoilYawScaleV_ARMORCANNON(float) override { std::abort(); }
    float getRecoilPitchScaleV_ARMORCANNON() override { std::abort(); }
    void setRecoilPitchScaleV_ARMORCANNON(float) override { std::abort(); }
    float getRecoilPunchScaleV_COAXIALGUN() override { std::abort(); }
    void setRecoilPunchScaleV_COAXIALGUN(float) override { std::abort(); }
    float getRecoilPunchTimeV_COAXIALGUN() override { std::abort(); }
    void setRecoilPunchTimeV_COAXIALGUN(float) override { std::abort(); }
    float getRecoilYawScaleV_COAXIALGUN() override { std::abort(); }
    void setRecoilYawScaleV_COAXIALGUN(float) override { std::abort(); }
    float getRecoilPitchScaleV_COAXIALGUN() override { std::abort(); }
    void setRecoilPitchScaleV_COAXIALGUN(float) override { std::abort(); }
    float getUnresolved48C() override { std::abort(); }
    void setUnresolved48C(float) override { std::abort(); }
    float getUnresolved490() override { std::abort(); }
    void setUnresolved490(float) override { std::abort(); }
    float getGeneralShakeScale() override { std::abort(); }
    void setGeneralShakeScale(float) override { std::abort(); }
    float getExplosionShakeScale() override { std::abort(); }
    void setExplosionShakeScale(float) override { std::abort(); }
    float getSpeedShakeScale() override { std::abort(); }
    void setSpeedShakeScale(float) override { std::abort(); }
    int getWeaponClass(const std::string&) override { std::abort(); }
    void addWeaponClassMap(const std::string&, int) override { std::abort(); }
};

// [TEMP] Typed observer of the GlobalSettings singleton the profile save, load and apply paths
// reach (file name, script, apply, setDefaults, setDefaultUser); no production GlobalSettings body
// is linked here.
struct ProfileGlobal : IGlobalSettings {
    unsigned defaults=0, defaultUserSets=0, applies=0; std::string defaultUser;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    std::string getFileName() override { return "Global.con"; }
    std::string getSettingsScript() override { return "GLOBAL"; }
    bool apply() override { ++applies; applyLog.push_back("global"); return true; }
    void setDefaults() override { ++defaults; }
    std::string getDefaultUser() override { std::abort(); }
    void setDefaultUser(std::string value) override { ++defaultUserSets; defaultUser=value; }
    std::string getNamePrefix() override { std::abort(); }
    void setNamePrefix(std::string) override { std::abort(); }
};

// [TEMP] Typed observer of the DemoLibrary singleton login refreshes; every other slot aborts.
struct ProfileDemoLibrary : IDemoLibrary {
    unsigned refreshes=0;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    bool refresh() override { ++refreshes; return true; }
    bool download(const std::string&,const std::string&,const std::string&) override { std::abort(); }
    unsigned int getNumEntries() override { std::abort(); }
    DemoLibraryEntry* getEntry(unsigned int) override { std::abort(); }
    bool removeEntry(unsigned int) override { std::abort(); }
    void update() override { std::abort(); }
    bool isDownloading() override { std::abort(); }
    const std::string& getDownloadName() override { std::abort(); }
    const std::string& getDownloadUrl() override { std::abort(); }
    const std::string& getDownloadDescription() override { std::abort(); }
    float getDownloadProgress() override { std::abort(); }
    bool cancelDownload() override { std::abort(); }
    bool getFlagA4() override { std::abort(); }
    void setFlagA4(bool) override { std::abort(); }
    void setPendingBookmark(int) override { std::abort(); }
    bool hasDownloadFailed() override { std::abort(); }
};
static ProfileDemoLibrary* demoLibraryOwner=nullptr;

// [TEMP] Typed observer of io::controlMapManager: the named save applies the active map and the
// named load asks for it (none here, so no map is re-selected); every other slot aborts.
struct ProfileControlMaps : io::IControlMapManager {
    unsigned applies=0, activeQueries=0;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    void update(float) override { std::abort(); }
    io::IControlMap* create(const std::string&) override { std::abort(); }
    io::IControlMap* getControlMap(const std::string&) const override { std::abort(); }
    io::IControlMap* getControlMap(int) const override { std::abort(); }
    void deleteControlMap(const std::string&) override { std::abort(); }
    void deleteControlMap(int) override { std::abort(); }
    void deleteControlMap(const io::IControlMap*) override { std::abort(); }
    void setActive(const std::string&) override { std::abort(); }
    void setActive(const io::IControlMap*) override { std::abort(); }
    io::IControlMap* getActive() const override { ++const_cast<ProfileControlMaps*>(this)->activeQueries; return nullptr; }
    bool mapNextInput(io::IControlMap*,int,bool,bool,bool,bool,std::string&) override { std::abort(); }
    float getRepeatRate(int,bool) override { std::abort(); }
    void applyActive() override { ++applies; }
};

#ifdef BF2_PROFILE_DISCOVERY_FIXTURE
// [TEMP] Dependency observers inherit the real headers and constructors. Every
// LocalProfile body remains production code; only external filesystem/console
// results are controlled. The FileManager observer lives until process exit,
// because its unrelated destructor remains unresolved in this focused link.
static std::vector<std::string> ioCalls;
struct ProfileStream : io::INonlinearStream {
    std::string contents;
    unsigned size=0, delivered=0, reads=0, releases=0;
    std::function<void()> onRelease;
    void addRef() override { std::abort(); }
    int getRef() override { return 1; }
    int release() override { ++releases;ioCalls.push_back("release");if(onRelease)onRelease();return 0; }
    void* queryInterface(unsigned int) const override { std::abort(); }
    unsigned read(void* target,unsigned count) override {
        ++reads;ioCalls.push_back("read");
        check(count==size,"loader reads the full reported size once");
        const char* before=static_cast<const char*>(target);
        bool zero=true;for(unsigned i=0;i<=size;++i)zero=zero && before[i]==0;
        check(zero,"loader zero-fills size plus trailing terminator before read");
        unsigned n=delivered<count?delivered:count;if(n>contents.size())n=static_cast<unsigned>(contents.size());
        if(n)std::memcpy(target,contents.data(),n);return n;
    }
    unsigned write(const void*,unsigned) override { std::abort(); }
    int skip(unsigned) override { std::abort(); }
    int getAvailableBytes() override { std::abort(); }
    io::IStream* clone() override { std::abort(); }
    bool canRead() override { return true; }
    bool canWrite() override { return false; }
    int seek(io::SeekOrigin,int) override { std::abort(); }
    int getPosition() override { std::abort(); }
    int getSize() override { ioCalls.push_back("size");return static_cast<int>(size); }
};
struct Files : io::FileManager {
    std::list<io::FileInfo> listing;
    unsigned searches=0,opens=0;
    int findResult=0;
    ProfileStream* stream=nullptr;
    std::string lastOpen;
    std::function<void()> onFind;
    bool exists=false;
    unsigned existenceChecks=0;
    std::string lastExists;
    std::vector<std::string> existsPaths;
    int existsFalseAt=-1;                   // [TEMP] the ordinal existence test that answers false (-1: none)
    std::function<void()> onExists;
    std::function<void()> onOpen;
    unsigned writes=0;
    std::vector<std::string> writtenPaths,writtenContents,openedPaths;
    std::vector<bool> appendFlags;
    // [TEMP] Every profile write is recorded (path, text, the retail flag) and succeeds.
    bool writeFile(const std::string& name,const void* buffer,unsigned int size,bool append) override {
        ++writes;ioCalls.push_back("write");writtenPaths.push_back(name);appendFlags.push_back(append);
        writtenContents.push_back(std::string(static_cast<const char*>(buffer),size));
        return true;
    }
    void reset() { writes=0;opens=0;writeOpens=0;writtenPaths.clear();writtenContents.clear();appendFlags.clear();openedPaths.clear();lastOpen.clear();ioCalls.clear(); }
    bool fileExists(const std::string& path) override {
        ++existenceChecks;ioCalls.push_back("exists");lastExists=path;existsPaths.push_back(path);
        if(onExists)onExists();
        if(existsFalseAt>=0 && static_cast<int>(existsPaths.size())==existsFalseAt+1) return false;
        return exists;
    }
    int findFiles(const std::string& path,bool recursive,std::list<io::FileInfo>& out) override {
        ++searches;ioCalls.push_back("find");
        check(path=="@HOME@/Profiles/" && !recursive,"discovery uses actual root and nonrecursive search");
        if(onFind)onFind();out=listing;return findResult;
    }
    unsigned writeOpens=0;
    io::IStream* open(const std::string& path,io::FileMode mode,io::FileAccess access,unsigned flags) override {
        if(mode!=2) { ++writeOpens;openedPaths.push_back(path);return nullptr; } // the map list's write open is refused
        ++opens;ioCalls.push_back("open");lastOpen=path;openedPaths.push_back(path);
        check(mode==2 && access==0 && flags==0,"profile loader uses retail read mode/access/flags");
        if(onOpen)onOpen();
        return stream;
    }
    void directory(const std::string& name) {
        io::FileInfo info{};info.name=name;info.isDirectory=true;listing.push_back(info);
    }
};
struct ProfileConsole : io::OldConsole {
    mutable unsigned reads=0;
    unsigned executions=0;
    int access=7;
    std::string input;
    std::vector<int> accessWrites;
    std::function<void()> onRead;
    std::function<void()> onExecute;
    int getAccess() const override { ++reads;ioCalls.push_back("getAccess");if(onRead)onRead();return access; }
    void setAccess(int value) override { ioCalls.push_back("setAccess");accessWrites.push_back(value);access=value; }
    void executeLine(const std::string& text,std::string& out,bool a,bool b,bool c,bool d,bool e) override {
        ++executions;ioCalls.push_back("execute");input=text;
        check(&out==&io::Console::ignoredString_,"loader passes the actual shared console output string");
        check(!a && !b && !c && !d && !e,"loader preserves all five false execute flags");
        check(access==2,"loader executes at retail access level two");
        out="fixture console error";if(onExecute)onExecute();
    }
};

static void discoveryTests(Events& events,Files& files)
{
    LocalProfileManager manager;events.current=&manager;
    ILocalProfileManager* api=&manager;
    check(!api->getCurrentProfile(),"actual current-profile pointer getter returns null initially");
    check(!api->loadCurrentProfile() && !api->saveCurrentProfile(),"current-profile load/save preserve absent-record refusal");
    std::string path=api->getProfilesPath();path="changed";
    check(api->getProfilesPath()=="@HOME@/Profiles/","profile root is an independent returned native string");
    std::vector<LocalProfile> output(1);output[0].name="discard output";
    files.listing.clear();files.findResult=-19;
    unsigned before=files.searches;
    check(api->getLocalProfiles(output,true) && output.empty() && files.searches==before+1,
        "empty discovery replaces output and ignores filesystem result");
    check(!LocalProfileFixture::dirty(manager) && LocalProfileFixture::filter(manager),"refresh stores dirty/filter state");
    output.resize(1);before=files.searches;
    check(api->getLocalProfiles(output,true) && output.empty() && files.searches==before,
        "empty cached enumeration avoids another filesystem call");
    io::FileInfo regular{};regular.name="ordinary.txt";regular.isDirectory=false;
    files.listing.push_back(regular);files.findResult=91;
    check(api->getLocalProfiles(output,false) && output.empty() && files.searches==before+1,
        "changed validity filter refreshes and skips non-directories");
    std::vector<LocalProfile> stored(3);
    stored[0].name="0001";stored[0].profileName="First";stored[0].password=std::string("p\0w",3);
    stored[1].name="0002";stored[1].profileName="SECOND";stored[1].numTimesLoggedIn=7;
    stored[2].name="0003";stored[2].profileName="first";
    LocalProfileFixture::cache(manager,stored,true);before=files.searches;
    check(api->getLocalProfiles(output,true) && files.searches==before && output.size()==3,
        "cached enumeration copies every native record without I/O");
    check(output[0].password==std::string("p\0w",3) && output[1].numTimesLoggedIn==7,"cached record copy retains full strings and scalars");
    output[0].name="caller edit";
    check(LocalProfileFixture::profiles(manager)[0].name=="0001","enumeration output does not alias the cache");
    check(api->hasProfile("fIrSt") && !api->hasProfile("absent"),"actual profile lookup compares profile names without case");
    std::string found="unchanged";
    check(api->findProfileDirectory("first",found) && found=="0001","directory lookup chooses first matching profile name");
    check(!api->findProfileDirectory("absent",found) && found=="0001","directory lookup miss preserves caller output");
    check(files.searches==before,"selection and directory helpers reuse a matching valid-profile cache");
    LocalProfileFixture::setDefaultFlag(manager,true);
    check(api->hasDefaultProfile() && files.searches==before,"hasDefaultProfile reads its actual flag after cached enumeration");
    LocalProfileFixture::setDirty(manager,true);
    files.onFind=[&]() {
        check(!LocalProfileFixture::dirty(manager) && LocalProfileFixture::filter(manager),"dirty/filter writes precede filesystem callback");
        check(LocalProfileFixture::profiles(manager).empty() && LocalProfileFixture::profiles(manager).capacity()==0,
            "refresh destroys and frees old cached storage before search");
    };
    check(api->getLocalProfiles(output,true) && output.empty() && files.searches==before+1,"dirty cache refreshes despite matching filter");
    check(LocalProfileFixture::defaultFlag(manager),"refresh preserves the previously detected default flag");
    files.onFind=nullptr;files.listing.clear();

    io::OldConsole* savedConsole=io::mainConsole;
    ProfileConsole first,second,third;
    io::mainConsole=&first;files.stream=nullptr;ioCalls.clear();
    check(!api->loadProfileFile("0001/Profile.con") && files.lastOpen=="@HOME@/Profiles/0001/Profile.con",
        "missing profile file returns false after exact root-relative open");
    check(ioCalls==std::vector<std::string>({"open"}) && first.reads==0 && !first.executions,
        "failed open performs no stream or console work");
    ProfileStream stream;stream.contents=std::string("ab\0ignored",10);stream.size=10;stream.delivered=10;
    files.stream=&stream;ioCalls.clear();
    check(api->loadProfileFile("0001/Profile.con"),"successful open returns true even when console reports an error");
    check(first.input=="ab" && stream.reads==1 && stream.releases==1,"loader truncates embedded NUL and releases stream once");
    check(first.accessWrites==std::vector<int>({2,7}) && first.access==7,"loader restores original console access after execution");
    check(ioCalls==std::vector<std::string>({"open","size","read","release","getAccess","setAccess","execute","setAccess"}),
        "loader preserves read/release/console callback ordering");
    stream.contents="short";stream.size=8;stream.delivered=2;ioCalls.clear();
    check(api->loadProfileFile("short.con") && first.input=="sh","short read uses zero-filled tail and does not fail");
    stream.contents.clear();stream.size=0;stream.delivered=0;
    check(api->loadProfileFile("empty.con") && first.input.empty(),"empty stream still executes an empty command and succeeds");
    stream.contents="global reload";stream.size=13;stream.delivered=13;
    first.access=11;first.onRead=[&](){io::mainConsole=&second;};
    second.onExecute=[&](){io::mainConsole=&third;};io::mainConsole=&first;
    check(api->loadProfileFile("reload.con") && second.input=="global reload","loader reloads console global between access read and execution");
    check(second.accessWrites==std::vector<int>({2}) && third.accessWrites==std::vector<int>({11}),
        "access restoration reloads console global again after execute callback");
    first.onRead=nullptr;second.onExecute=nullptr;io::mainConsole=savedConsole;files.stream=nullptr;
}

static void validationTests(Events& events,Files& files)
{
    LocalProfileManager manager;events.current=&manager;
    ILocalProfileManager* api=&manager;
    files.listing.clear();files.exists=false;files.stream=nullptr;files.onFind=nullptr;
    unsigned before=files.existenceChecks;
    check(api->validateProfileFiles("Default") && files.existenceChecks==before,"exact Default bypasses existence checks");
    check(!api->validateProfileFiles("default") && files.existenceChecks==before+1
        && files.lastExists=="@HOME@/Profiles/default/Profile.con","validation is case-sensitive and checks exact profile filename");
    std::string embedded("Default\0suffix",14);
    check(!api->validateProfileFiles(embedded) && files.lastExists==std::string("@HOME@/Profiles/")+embedded+"/Profile.con",
        "Default comparison and file path preserve embedded NUL length");
    check(!api->hasDefaultVoiceSettings() && files.lastExists=="@HOME@/Profiles/BattleCommoDefaultUserSettings.con",
        "default voice-settings query checks its actual root-relative file");
    files.exists=true;
    check(api->hasDefaultVoiceSettings(),"default voice-settings query returns actual existence result");
    // Checks Profile.con, then General, GameServer, and Audio paths, stopping at the first missing file.
    files.existsPaths.clear();
    check(api->validateProfileFiles("0001") && files.existsPaths.size()==4
        && files.existsPaths[0]=="@HOME@/Profiles/0001/Profile.con"
        && files.existsPaths[1]=="@HOME@/Profiles/0001/General.con"
        && files.existsPaths[2]=="@HOME@/Profiles/0001/ServerSettings.con"
        && files.existsPaths[3]=="@HOME@/Profiles/0001/Audio.con",
        "a named profile validates Profile.con, then the General, GameServer and Audio files in that order");
    for(int miss=1;miss<4;++miss) {
        files.existsPaths.clear();files.existsFalseAt=miss;
        check(!api->validateProfileFiles("0001") && static_cast<int>(files.existsPaths.size())==miss+1,
            "a missing settings file fails validation at its own test and asks for nothing after it");
    }
    files.existsFalseAt=-1;files.existsPaths.clear();
    files.exists=false;

    LocalProfileFixture::seed(manager);LocalProfile* original=api->getCurrentProfile();
    std::vector<LocalProfile> output;
    files.directory("missing");files.directory("default");
    io::FileInfo ordinary{};ordinary.name="ordinary";files.listing.push_back(ordinary);
    files.onExists=[&](){check(api->getCurrentProfile()==original,"validation runs before replacing current profile");};
    unsigned opens=files.opens;
    LocalProfileFixture::setDirty(manager,true);
    check(api->getLocalProfiles(output,true) && output.empty() && files.opens==opens,
        "valid-only enumeration drops missing-file directories without loading them");
    check(!LocalProfileFixture::defaultFlag(manager) && api->getCurrentProfile()==original,
        "invalid lowercase default does not set default flag and preserves current profile");
    check(api->getLocalProfiles(output,false) && output.size()==2 && output[0].name=="missing" && output[1].name=="default",
        "unfiltered enumeration retains invalid directories in filesystem order");
    check(output[0].profileName.empty() && output[0].password.empty() && output[0].numTimesLoggedIn==0
        && output[0].totalPlayedTime==0.0f && files.opens==opens,"invalid records retain constructor defaults without console load");
    check(api->getCurrentProfile()==original && !LocalProfileFixture::defaultFlag(manager),"invalid-record append restores original current profile");
    files.onExists=nullptr;

    files.listing.clear();files.directory("Default");LocalProfileFixture::setDirty(manager,true);
    files.onOpen=[&](){check(api->getCurrentProfile()!=original && api->getCurrentProfile()->name=="Default",
        "default load publishes the temporary native record before open");};
    check(api->getLocalProfiles(output,true) && output.empty() && files.lastOpen=="@HOME@/Profiles/Default/Profile.con",
        "validated Default is opened and excluded from cached profiles");
    check(LocalProfileFixture::defaultFlag(manager) && api->getCurrentProfile()==original,
        "failed Default open still sets default flag and restores current profile");

    ProfileConsole console;io::OldConsole* saved=io::mainConsole;io::mainConsole=&console;
    ProfileStream stream;stream.contents="set fixture fields";stream.size=18;stream.delivered=18;files.stream=&stream;
    console.onExecute=[&](){
        check(api->getCurrentProfile()!=original && api->getCurrentProfile()->name=="Default","console sees the temporary profile owner");
        api->setName("Loaded Default");api->setNick("fixture nick");api->setPassword("temporary password",false);
        check(api->getName()=="Loaded Default" && api->getPassword()=="temporary password","console callback reaches real typed field methods");
    };
    LocalProfileFixture::setDefaultFlag(manager,false);LocalProfileFixture::setDirty(manager,true);
    check(api->getLocalProfiles(output,false) && output.empty() && console.executions==1 && stream.releases==1,
        "unfiltered enumeration also validates and loads exact Default without caching it");
    check(LocalProfileFixture::defaultFlag(manager) && api->getCurrentProfile()==original && original->profileName.empty()
        && original->password==std::string(70,'b'),"temporary console writes do not modify original current profile");
    files.onOpen=nullptr;files.stream=nullptr;files.listing.clear();io::mainConsole=saved;
}
#endif

static void cachedTests(Events& events)
{
    LocalProfileManager manager;events.current=&manager;
    ILocalProfileManager* api=&manager;
    check(!api->getCurrentProfile(),"actual current-profile getter returns null");
    check(!api->loadCurrentProfile() && !api->saveCurrentProfile(),"absent current profile refuses load/save before dependencies");
    check(api->getProfilesPath()=="@HOME@/Profiles/","actual profile-root value getter");
    std::vector<LocalProfile> records(2),out(4);
    records[0].name="0001";records[0].profileName=std::string("first\0tail",10);
    records[0].password=std::string("p\0w",3);records[1].name="0002";records[1].profileName="Second";
    LocalProfileFixture::cache(manager,records,true);
    check(api->getLocalProfiles(out,true) && out.size()==2 && out[0].password==records[0].password,
        "real cached enumeration assigns full native records with null filesystem");
    out[0].name="edited";
    check(LocalProfileFixture::profiles(manager)[0].name=="0001","cached enumeration returns independent copies");
    check(api->hasProfile("FIRST") && !api->hasProfile("third"),"hasProfile uses case-insensitive C-string comparison");
    std::string directory="unchanged";
    check(api->findProfileDirectory("first",directory) && directory=="0001","directory lookup preserves embedded-NUL comparison behavior");
    check(!api->findProfileDirectory("third",directory) && directory=="0001","directory lookup miss preserves output");
    LocalProfileFixture::setDefaultFlag(manager,true);
    check(api->hasDefaultProfile(),"hasDefaultProfile returns the real flag after successful cached enumeration");
    LocalProfileFixture::cache(manager,{},true);
    const std::string empty;
    check(!api->login(-1,empty,empty) && !api->login(0,empty,empty) && !api->login(4,empty,empty),
        "login rejects negative and too-large indices after actual cached enumeration");
    check(!api->deleteProfile(0) && !api->deleteProfile(4),
        "delete rejects nonnegative out-of-range indices after actual cached enumeration");
    check(!api->getCurrentProfile(),"invalid index branches never publish a current record");
}

static void fieldTests(Events& events,Registry& registry,ProfileAudio& audio)
{
    LocalProfileManager manager;events.current=&manager;
    ILocalProfileManager* api=&manager;
    void** table=*reinterpret_cast<void***>(api);
    bool complete=true;
    for(unsigned i=0;i<51;++i) complete=complete && table[i] && table[i]!=GetModuleHandleA(nullptr);
    check(complete,"all 51 primary entries through +C8 have callable native addresses");
    unsigned audioBefore=audio.calls;
    check(!api->applySettings(false) && !api->applySettings(true) && audio.calls==audioBefore,
        "apply refuses absent current profile for both branches without audio callback");
    std::string value("full\0string",11),error="unchanged";
    api->setName(value);api->setGamespyNick(value);api->setEmail(value);
    api->setPassword(value,false);api->setPassword(value,true);api->setNick(value);
    api->setNumTimesLoggedIn(-31);api->setTotalPlayedTime(17.5f);
    check(!api->getCurrentProfile(),"all setters preserve absent current profile including encoded password");
    check(api->getName().empty() && api->getGamespyNick().empty() && api->getEmail().empty()
        && api->getPassword().empty() && api->getNick().empty(),"absent-profile string getters return native empty values");
    check(api->getNumTimesLoggedIn()==0 && api->getTotalPlayedTime()==0.0f,"absent-profile scalar getters return retail zero values");
    unsigned before=registry.swiffQueries;
    check(!api->validateProfileName(value,error) && error=="unchanged" && registry.swiffQueries==before+1,
        "missing SwiffHost refuses name validation without touching output");
    registry.swiff=true;swiffHostObserver.validations=0;
    check(!api->validateProfileName("Alpha",error) && error=="hostrefused" && swiffHostObserver.validations==1 && swiffHostObserver.validatedName=="Alpha",
        "with a SwiffHost the name and the error string go through its +26C validator");
    registry.swiff=false;error="unchanged";
    check(api->validateProfileFiles("Default"),"exact Default directory validates before null filesystem use");
    check(api->getNumDemoBookmarks()==0 && !api->removeDemoBookmark(0) && !api->removeDemoBookmark(~0u),
        "empty bookmarks reject zero and maximum unsigned index");
    api->addDemoBookmark(value,"second",std::string(90,'x'),"fourth");
    check(api->getNumDemoBookmarks()==1,"bookmark append works without any current profile");
    LocalDemoBookmark* first=api->getDemoBookmark(0);
    check(first->field00==value && first->field1c=="second" && first->field38==std::string(90,'x') && first->field54=="fourth",
        "borrowed bookmark contains all four independently copied strings");
    first->field1c="borrowed edit";
    check(api->getDemoBookmark(0)->field1c=="borrowed edit","bookmark getter returns live record storage");
    while(api->getNumDemoBookmarks()<LocalProfileFixture::bookmarkCapacity(manager)) api->addDemoBookmark("padding","","","");
    first=api->getDemoBookmark(0);
    unsigned oldCount=api->getNumDemoBookmarks();
    api->addDemoBookmark(first->field00,first->field1c,first->field38,first->field54);
    LocalDemoBookmark* copied=api->getDemoBookmark(oldCount);
    check(api->getNumDemoBookmarks()==oldCount+1 && copied->field00==value && copied->field1c=="borrowed edit"
        && copied->field38==std::string(90,'x') && copied->field54=="fourth","append preserves aliased source fields across vector growth");
    check(!api->removeDemoBookmark(api->getNumDemoBookmarks()) && !api->removeDemoBookmark(~0u)
        && api->getNumDemoBookmarks()==oldCount+1,"unsigned out-of-range removal preserves bookmarks");
    check(api->removeDemoBookmark(0) && api->getNumDemoBookmarks()==oldCount
        && api->getDemoBookmark(oldCount-1)->field00==value,"erase shifts surviving records and decrements count");
    while(api->getNumDemoBookmarks()) api->removeDemoBookmark(api->getNumDemoBookmarks()-1);
    api->addDemoBookmark("A","a1","a2","a3");api->addDemoBookmark("B","b1","b2","b3");api->addDemoBookmark("C","c1","c2","c3");
    check(api->removeDemoBookmark(1) && api->getNumDemoBookmarks()==2 && api->getDemoBookmark(0)->field00=="A"
        && api->getDemoBookmark(1)->field00=="C","middle erase preserves ordered neighbors");

    LocalProfileFixture::seed(manager);
    LocalProfile* current=api->getCurrentProfile();const std::string directory=current->name;
    audioBefore=audio.calls;audio.result=false;
    audio.onApply=[&](){check(api->getCurrentProfile()==current,"audio callback sees existing current profile");};
    check(api->applySettings(true) && audio.calls==audioBefore+1 && !audio.lastFlag,
        "audio-only apply returns true after exactly one false-result apply(false) callback");
    audio.result=true;
    check(api->applySettings(true) && audio.calls==audioBefore+2 && !audio.lastFlag,
        "audio-only apply also ignores a true callback result and still passes false");
    check(api->getCurrentProfile()==current && !LocalProfileFixture::dirty(manager),"audio-only apply preserves profile publication and dirty state");
    audio.onApply=nullptr;audio.result=false;
    api->setName(value);api->setGamespyNick("gamespy");api->setEmail(std::string(90,'e'));
    api->setPassword(value,false);api->setNick("nick");api->setNumTimesLoggedIn(-31);
    check(api->getName()==value && api->getGamespyNick()=="gamespy" && api->getEmail()==std::string(90,'e')
        && api->getPassword()==value && api->getNick()=="nick" && api->getNumTimesLoggedIn()==-31,"typed field setters/getters preserve strings and signed counter");
    check(current->name==directory && !LocalProfileFixture::dirty(manager),"field writes preserve directory name and dirty state");
    std::string copy=api->getName();copy[0]='X';
    check(api->getName()==value,"native string getter returns an independent value");
    api->setName(current->profileName);api->setPassword(current->password,false);
    check(api->getName()==value && api->getPassword()==value,"self-aliased field assignment preserves full string");
    api->setTotalPlayedTime(-17.25f);
    check(api->getTotalPlayedTime()==-17.25f,"total-played-time uses a float value return");
    unsigned int payload=0xffc12345u,stored=0;float payloadFloat;
    std::memcpy(&payloadFloat,&payload,sizeof(payload));api->setTotalPlayedTime(payloadFloat);
    std::memcpy(&stored,&current->totalPlayedTime,sizeof(stored));
    check(stored==payload,"played-time setter preserves raw NaN payload bits");
}

// The password codec (Game/PasswordCodec.h) and setPassword's encoded arm, both real. The
// protected bytes come from the current Windows user's dpapi, so only their shape and the round
// trip are asserted.
static void codecTests(Events& events)
{
    PasswordCodec codec;
    check(!codec.encode(""),"codec refuses an empty string before allocating");
    check(codec.encode("secret"),"codec protects a plain string");
    std::ostringstream hexStream;codec.write(hexStream);
    std::string hex=hexStream.str();
    bool shape=!hex.empty() && hex.size()%2==0;
    for(char c:hex) if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) shape=false;
    check(shape,"codec writes two lowercase hex digits per protected byte");
    check(codec.encode("again"),"re-encoding releases the previous blob and protects anew");
    std::string decoded=hex;
    check(PasswordCodec::decode(decoded) && decoded=="secret","codec round trip restores the plain string");
    std::string garbage="encoded";
    check(!PasswordCodec::decode(garbage) && garbage=="encoded","a failed decode leaves the text as given");
    std::string empty;
    check(!PasswordCodec::decode(empty),"decode refuses an empty string");
    LocalProfileManager manager;events.current=&manager;
    ILocalProfileManager* api=&manager;
    api->setPassword(hex,true);
    check(!api->getCurrentProfile(),"encoded setPassword without a current profile stores nothing");
    LocalProfileFixture::seed(manager);
    api->setPassword("",true);
    check(api->getPassword().empty(),"an encoded empty string is stored without decoding");
    api->setPassword("encoded",true);
    check(api->getPassword()=="encoded","undecodable encoded text is stored as given");
    api->setPassword(hex,true);
    check(api->getPassword()=="secret","encoded hex is decoded into the stored password");
    api->setPassword("plain",false);
    check(api->getPassword()=="plain","raw assignment still bypasses the codec");
}

// applySettings (PE4A77E0): the full apply calls the six settings owners in retail order and never
// ControlSettings; the audio-only arm calls audio alone.
static void applyTests(Events& events)
{
    LocalProfileManager manager;events.current=&manager;
    ILocalProfileManager* api=&manager;
    applyLog.clear();
    check(!api->applySettings(false) && applyLog.empty(),"apply refuses an absent current profile before any owner");
    LocalProfileFixture::seed(manager);
    check(api->applySettings(false),"full apply returns true");
    check(applyLog==std::vector<std::string>({"global","general","gameServer","audio","video","haptic"}),
        "full apply calls global, general, game server, audio, video and haptic in retail order and not control");
    applyLog.clear();
    check(api->applySettings(true) && applyLog==std::vector<std::string>({"audio"}),"audio-only apply calls audio alone");
}

// quickLogin (PE4AAD00): the refusal while a profile is current, and the scans that find no row.
// The matches that reach login need the file observer and live in saveLoadTests.
static void quickLoginTests(Events& events,Registry& registry)
{
    {
        LocalProfileManager manager;events.current=&manager;
        ILocalProfileManager* api=&manager;
        LocalProfileFixture::seed(manager);
        const unsigned seams=seamCalls;
        check(!api->quickLogin("Named","pw") && seamCalls==seams,"a current profile refuses quickLogin and reports to no host");
        registry.swiff=true;
        check(!api->quickLogin("Named","pw") && seamCalls==seams+1 && seamType==3 && seamFlag==3
            && seamText=="Unable to quickLogin while allready logged in","with a SwiffHost the refusal posts the shipped message, misspelling kept");
        registry.swiff=false;
    }
    LocalProfileManager fresh;events.current=&fresh;
    ILocalProfileManager* api=&fresh;
    LocalProfileFixture::cache(fresh,{},true);
    unsigned queries=registry.queries;
    check(!api->quickLogin("Anyone","pw") && registry.queries==queries+1 && !api->getCurrentProfile(),
        "with a password and no rows the ProfileManager is looked up and nothing logs in");
    std::vector<LocalProfile> rows(2);
    rows[0].name="0001";rows[0].profileName="Alpha";rows[0].gamespyNick="AlphaGS";rows[0].password="";
    rows[1].name="0002";rows[1].profileName="Beta";rows[1].gamespyNick="BetaGS";rows[1].password="secret";
    LocalProfileFixture::cache(fresh,rows,true);
    queries=registry.queries;
    check(!api->quickLogin("Alpha","pw") && registry.queries==queries+1,"with a password the scan matches gamespy nicks, not profile names");
    check(!api->quickLogin("Beta","") && registry.queries==queries+1,"without a password a row with a stored password is skipped and no singleton is asked");
    check(!api->quickLogin("AlphaGS","") && !api->getCurrentProfile(),"without a password the scan matches profile names, not gamespy nicks");
}

#ifdef BF2_PROFILE_DISCOVERY_FIXTURE
static bool passwordRowIsHex(const std::string& contents)
{
    const char* key="LocalProfile.setPassword \"";
    std::size_t at=contents.find(key);
    if(at==std::string::npos) return false;
    std::size_t begin=at+std::strlen(key), end=contents.find('"',begin);
    if(end==std::string::npos || end==begin || (end-begin)%2) return false;
    for(std::size_t i=begin;i<end;++i){ char c=contents[i]; if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false; }
    return true;
}
// The complete save and load paths against the file observer: the Default creation and its four
// writes, init's default creation followed by the Default load's two opens, the named save's twelve
// writes, the named load's resets and Global.con write before its failed Profile.con open, the
// pending-save flag, saveGlobalSettings alone, and init with an existing Default directory.
static void saveLoadTests(Events& events,Files& files,ProfileGlobal& global,ProfileAudio& audio,ProfileGeneral& general,
    ProfileGameServer& gameServer,ProfileVideo& video,ProfileControl& control,ProfileHaptic& haptic,ProfileControlMaps& controlMaps)
{
    const std::string root="@HOME@/Profiles/";
    const std::string named=root+std::string(70,'a')+"/";
    {
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        files.reset();
        check(api->createDefaultProfile(),"createDefaultProfile completes through the Default save");
        check(files.writtenPaths==std::vector<std::string>({root+"Default/Profile.con",root+"Global.con",root+"Default/Audio.con",root+"Default/Video.con"}),
            "the Default save writes Profile.con, Global.con at the root, Audio.con and Video.con in order");
        check(files.writtenContents.size()==4 && files.writtenContents[0]=="LocalProfile.setName \"Default\"\nLocalProfile.setNick \"Default\"\n"
            "LocalProfile.setGamespyNick \"\"\nLocalProfile.setTotalPlayedTime 0\nLocalProfile.setNumTimesLoggedIn 0\n","the Default Profile.con text");
        check(files.writtenContents.size()==4 && files.writtenContents[1]=="GLOBAL" && files.writtenContents[2]=="AUDIO" && files.writtenContents[3]=="VIDEO",
            "each settings file carries its owner's script");
        bool flagged=!files.appendFlags.empty();for(bool a:files.appendFlags) flagged=flagged&&a;
        check(flagged,"every profile write passes the retail true flag");
        check(!api->getCurrentProfile() && files.opens==0,"createDefaultProfile destroys its temporary record and opens nothing");
    }
    {
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        LocalProfileFixture::cache(manager,{},true);files.reset();
        const unsigned applies=audio.calls,audioDefaults=audio.defaultsCalls,videoDefaults=video.defaults,seams=seamCalls;
        check(api->init(),"init completes: default creation, the failed Default load, no apply");
        check(files.writtenPaths.size()==4,"init's default creation writes the four files");
        check(files.openedPaths==std::vector<std::string>({root+"Global.con",root+"Default/Profile.con"}),
            "the Default load opens Global.con then the profile script and stops at the failed open");
        check(audio.defaultsCalls==audioDefaults+2 && video.defaults==videoDefaults+2,"the Default load resets audio and video before opening the profile script");
        check(audio.calls==applies+1,"the failed load destroys the record, so init's audio-only apply refuses");
        check(!api->getCurrentProfile() && seamCalls==seams,"init leaves no current profile and reports the open failure to no host");
    }
    {
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        LocalProfileFixture::seed(manager);LocalProfile* record=LocalProfileFixture::current(manager);
        record->profileName="Named";record->nick="Nick";record->gamespyNick="Gs";record->email="e@x";
        record->totalPlayedTime=1.5f;record->numTimesLoggedIn=3;
        files.reset();
        check(api->saveCurrentProfile(),"the named save completes");
        check(files.writtenPaths==std::vector<std::string>({named+"Profile.con",root+"Global.con",named+"General.con",named+"ServerSettings.con",
            named+"Audio.con",root+"Default/Audio.con",named+"Video.con",root+"Default/Video.con",named+"Haptic.con",named+"Controls.con",
            root+"Global.con",named+"DemoBookmarks.con"}),"the named save writes twelve files in retail order, Global.con twice (the save and saveGlobalSettings)");
        check(files.writtenContents.size()==12 && files.writtenContents[0].find("LocalProfile.setName \"Named\"\nLocalProfile.setNick \"Nick\"\n"
            "LocalProfile.setGamespyNick \"Gs\"\nLocalProfile.setEmail \"e@x\"\nLocalProfile.setPassword \"")==0 && passwordRowIsHex(files.writtenContents[0])
            && files.writtenContents[0].find("\"\nLocalProfile.setTotalPlayedTime 1.5\nLocalProfile.setNumTimesLoggedIn 3\n")!=std::string::npos,
            "the named Profile.con carries every field with the protected password as even lowercase hex");
        check(files.writtenContents.size()==12 && files.writtenContents[2]=="GENERAL" && files.writtenContents[3]=="SERVER" && files.writtenContents[8]=="HAPTIC"
            && files.writtenContents[9]=="CONTROL" && files.writtenContents[10]=="GLOBAL","the named settings files carry their owners' scripts");
        check(files.writtenContents.size()==12 && files.writtenContents[11]==std::string("LocalProfile.addDemoBookmark \"")+std::string(70,'f')+"\" \"\" \"\" \""+std::string(70,'g')+"\"\n",
            "DemoBookmarks.con has one row per bookmark with its four quoted fields");
        check(files.opens==0,"the map list has no config path yet, so its save opens nothing");
        check(controlMaps.applies==1,"the named save applies the active control map through the manager");
    }
    {
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        LocalProfileFixture::seed(manager);files.reset();
        global.defaults=0;global.defaultUserSets=0;
        const unsigned generalDefaults=general.defaults,resets=gameServer.resets,hapticDefaults=haptic.defaults,
            audioDefaults=audio.defaultsCalls,videoDefaults=video.defaults,seams=seamCalls;
        check(!api->loadCurrentProfile(),"the named load fails at the missing Profile.con");
        check(files.openedPaths==std::vector<std::string>({root+"Global.con",named+"Profile.con"}),"the named load opens Global.con then the profile script");
        check(global.defaults==1 && general.defaults==generalDefaults+1 && gameServer.resets==resets+1 && audio.defaultsCalls==audioDefaults+1
            && video.defaults==videoDefaults+1 && haptic.defaults==hapticDefaults+1,"the named load resets every settings owner once");
        check(global.defaultUserSets==1 && global.defaultUser==std::string(70,'a'),"the named load makes the profile directory the default user");
        check(files.writtenPaths==std::vector<std::string>({root+"Global.con"}) && files.writtenContents[0]=="GLOBAL",
            "the named load saves the global settings before opening the profile script");
        check(!api->getCurrentProfile() && seamCalls==seams,"the failed open destroys the current record and reports to no host");
        check(LocalProfileFixture::bookmarkCapacity(manager)==0,"the named load frees the bookmark vector");
    }
    {
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        LocalProfileFixture::seed(manager);LocalProfileFixture::setSaveGlobalFlag(manager,true);files.reset();
        check(!api->loadCurrentProfile() && files.writtenPaths==std::vector<std::string>({root+"Global.con",root+"Global.con"}),
            "the pending flag saves the global settings first and the named load saves them again");
    }
    {
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        files.reset();
        check(api->saveGlobalSettings() && files.writtenPaths==std::vector<std::string>({root+"Global.con"}) && files.writtenContents[0]=="GLOBAL",
            "saveGlobalSettings writes Global.con at the profiles root");
    }
    {
        // The named load against an empty script stream and a console observer: every settings
        // file is opened and executed, the server settings and map list are pointed at the
        // profile, GSPlayerName is published and the login is counted.
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        LocalProfileFixture::seed(manager);LocalProfileFixture::current(manager)->gamespyNick="Gs";
        ProfileStream stream;files.stream=&stream;ProfileConsole console;io::OldConsole* saved=io::mainConsole;io::mainConsole=&console;
        files.reset();controlMaps.applies=0;controlMaps.activeQueries=0;serverSettings->m_configFile.clear();
        const unsigned resets=control.resets,backups=control.backups;
        check(api->loadCurrentProfile(),"the named load succeeds with a script stream");
        check(files.openedPaths==std::vector<std::string>({root+"Global.con",named+"Profile.con",named+"General.con",named+"ServerSettings.con",
            named+"Audio.con",named+"Video.con",named+"Haptic.con",named+"Controls.con",named+"mapList.con"}),
            "the named load opens Global.con, the profile script, the six settings files and then the map list's write");
        check(console.executions==8 && stream.releases==8,"every opened script is executed at access two and its stream released");
        check(files.writtenPaths==std::vector<std::string>({root+"Global.con"}),"the named load writes only the global settings");
        check(serverSettings->m_configFile==root+"Default/ServerSettings.con","the server settings are pointed at Default/ServerSettings.con");
        check(controlMaps.activeQueries==1 && control.resets==resets+1 && control.backups==backups+1,
            "no active map: one query, then the control maps are reset, loaded and backed up");
        check(mapList->m_configFile==named+"mapList.con" && files.writeOpens==1,"the map list is pointed at the profile's mapList.con and saved");
        check(settings->stringGet("GSPlayerName","")=="Gs","GSPlayerName is the gamespy nick when it is set");
        check(api->getCurrentProfile() && api->getCurrentProfile()->numTimesLoggedIn==1 && LocalProfileFixture::saveGlobalFlag(manager),
            "the login is counted and the field-54 flag raised");
        check(LocalProfileFixture::bookmarkCapacity(manager)==0,"the bookmark vector is freed before the settings files load");
        files.stream=nullptr;io::mainConsole=saved;mapList->m_configFile.clear();
    }
    {
        // The complete offline login: the cached row's directory becomes the current profile, the
        // named load runs against the script stream, the demo library is refreshed, the index and
        // password are stored, the cache is marked dirty and the full apply's result is returned.
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        std::vector<LocalProfile> rows(1);rows[0].name="0001";LocalProfileFixture::cache(manager,rows,true);
        ProfileStream stream;files.stream=&stream;ProfileConsole console;io::OldConsole* saved=io::mainConsole;io::mainConsole=&console;
        files.reset();demoLibraryOwner->refreshes=0;applyLog.clear();
        check(api->login(0,"unused","pw"),"the offline login returns the full apply's result");
        check(api->getCurrentProfile() && api->getCurrentProfile()->name=="0001" && console.executions==8,"login loads the chosen profile through every settings file");
        check(demoLibraryOwner->refreshes==1,"login refreshes the demo library once, after the load");
        check(LocalProfileFixture::loginIndex(manager)==0 && LocalProfileFixture::loginPassword(manager)=="pw" && LocalProfileFixture::dirty(manager),
            "login stores the index and password and marks the profile cache dirty");
        check(applyLog==std::vector<std::string>({"global","general","gameServer","audio","video","haptic"}),"login ends with the full apply in retail order");
        check(!api->login(0,"unused","pw"),"a second login refuses while a profile is current");
        files.stream=nullptr;io::mainConsole=saved;mapList->m_configFile.clear();
    }
    {
        // quickLogin's two matching arms, each ending in the complete offline login.
        std::vector<LocalProfile> rows(2);
        rows[0].name="0001";rows[0].profileName="Alpha";rows[0].gamespyNick="";rows[0].password="";
        rows[1].name="0002";rows[1].profileName="Beta";rows[1].gamespyNick="BetaGS";rows[1].email="beta@example";rows[1].password="secret";
        {
            LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
            LocalProfileFixture::cache(manager,rows,true);
            ProfileStream stream;files.stream=&stream;ProfileConsole console;io::OldConsole* saved=io::mainConsole;io::mainConsole=&console;
            files.reset();demoLibraryOwner->refreshes=0;
            check(api->quickLogin("Alpha","") && api->getCurrentProfile() && api->getCurrentProfile()->name=="0001"
                && LocalProfileFixture::loginIndex(manager)==0 && LocalProfileFixture::loginPassword(manager).empty() && demoLibraryOwner->refreshes==1,
                "without a password the first row whose profile name matches and whose password is empty logs in with empty strings");
            check(!api->quickLogin("Alpha",""),"a second quickLogin refuses while the profile is current");
            files.stream=nullptr;io::mainConsole=saved;mapList->m_configFile.clear();
        }
        {
            LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
            LocalProfileFixture::cache(manager,rows,true);
            ProfileStream stream;files.stream=&stream;ProfileConsole console;io::OldConsole* saved=io::mainConsole;io::mainConsole=&console;
            files.reset();demoLibraryOwner->refreshes=0;
            check(api->quickLogin("BetaGS","pw") && api->getCurrentProfile() && api->getCurrentProfile()->name=="0002"
                && LocalProfileFixture::loginIndex(manager)==1 && LocalProfileFixture::loginPassword(manager)=="pw" && demoLibraryOwner->refreshes==1,
                "with a password the first row whose gamespy nick matches logs in with that password");
            files.stream=nullptr;io::mainConsole=saved;mapList->m_configFile.clear();
        }
    }
    {
        // init with a stream: the Default load succeeds and init's audio-only apply runs.
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        LocalProfileFixture::cache(manager,{},true);ProfileStream stream;files.stream=&stream;
        ProfileConsole console;io::OldConsole* saved=io::mainConsole;io::mainConsole=&console;files.reset();
        const unsigned applies=audio.calls;
        check(api->init() && files.openedPaths==std::vector<std::string>({root+"Global.con",root+"Default/Profile.con",root+"Default/Audio.con",root+"Default/Video.con"}),
            "init's Default load opens Global.con, the profile script, Audio.con and Video.con");
        check(audio.calls==applies+2 && console.executions==4,"init applies audio at creation and after the successful load");
        files.stream=nullptr;io::mainConsole=saved;
    }
    {
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        files.listing.clear();files.directory("Default");files.reset();
        check(api->init(),"init with a Default directory completes");
        check(files.openedPaths.size()>=2 && files.openedPaths[files.openedPaths.size()-2]==root+"Global.con"
            && files.openedPaths.back()==root+"Default/Profile.con","the Default load's two opens end the sequence");
        check(!api->getCurrentProfile(),"init leaves no current profile");
        files.listing.clear();
    }
    events.current=nullptr;
}
#endif

// [TEMP] createProfile's two refusals, PE4AA98F and 4AA9B2. An absent SwiffHost makes the +BC
// validator answer false without a host call, and a host that rejects the name has its error
// posted through the +1E4 seam; neither path allocates a record, touches a settings owner or
// reaches the file manager, so both belong in the strict gate. The accepting path needs the
// filesystem and runs in the discovery gate (createProfileDiscoveryTests).
static void createProfileTests(Events& events,Registry& registry,ProfileGeneral& general,
    ProfileControl& control,ProfileGlobal& global)
{
    LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
    const unsigned generalDefaults=general.defaults,controlResets=control.resets;
    const unsigned controlDefaults=control.defaults,defaultUsers=global.defaultUserSets;
    registry.swiff=false;swiffHostObserver.validations=0;
    unsigned swiffQueries=registry.swiffQueries,seams=seamCalls;
    check(!api->createProfile("Alpha","Nick"),"an absent SwiffHost refuses the name, so creation fails");
    check(registry.swiffQueries==swiffQueries+2 && swiffHostObserver.validations==0,
        "the validator and the error report each ask the registry once, and no host is reached");
    check(seamCalls==seams,"an absent SwiffHost also swallows the error report");
    registry.swiff=true;swiffQueries=registry.swiffQueries;seams=seamCalls;
    check(!api->createProfile("Alpha","Nick"),"a validator refusal fails the creation");
    check(swiffHostObserver.validations==1 && swiffHostObserver.validatedName=="Alpha",
        "the profile NAME, not the nick, goes to the host's +26C validator");
    check(seamCalls==seams+1 && seamType==3 && seamFlag==3 && seamText=="hostrefused",
        "the error the validator filled is posted as the {3, message, 3} record");
    registry.swiff=false;
    check(!api->getCurrentProfile() && general.defaults==generalDefaults && control.resets==controlResets
        && control.defaults==controlDefaults && global.defaultUserSets==defaultUsers,
        "neither refusal allocates a record or touches a settings owner");
}

#ifdef BF2_PROFILE_DISCOVERY_FIXTURE
// [TEMP] PE4AA980 end to end, discovery gate: an accepted name takes the first free "%.4d"
// directory, resets every settings owner, keeps its new record as the current profile, re-points
// the map list and the server settings, publishes GSPlayerName and saves the whole profile.
static void createProfileDiscoveryTests(Events& events,Files& files,Registry& registry,ProfileGlobal& global,
    ProfileGeneral& general,ProfileGameServer& gameServer,ProfileAudio& audio,ProfileVideo& video,
    ProfileControl& control,ProfileHaptic& haptic)
{
    const std::string root="@HOME@/Profiles/";
    LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
    // 0001 and 0003 are taken, so the scan restarts twice and settles on 0002.
    std::vector<LocalProfile> rows(2);rows[0].name="0001";rows[1].name="0003";
    LocalProfileFixture::cache(manager,rows,false);
    files.reset();applyLog.clear();
    serverSettings->m_configFile.clear();mapList->m_configFile.clear();
    settings->stringSet("GSPlayerName","stale");
    registry.swiff=true;swiffHostObserver.validationAnswer=true;swiffHostObserver.validations=0;
    const unsigned generalDefaults=general.defaults,gameServerResets=gameServer.resets;
    const unsigned voipSets=gameServer.voipSets,audioDefaults=audio.defaultsCalls;
    const unsigned videoDefaults=video.defaults,hapticDefaults=haptic.defaults;
    const unsigned controlResets=control.resets,controlDefaults=control.defaults;
    const unsigned defaultUsers=global.defaultUserSets,seams=seamCalls;
    check(api->createProfile("Named","Nick"),"createProfile completes and answers true");
    check(swiffHostObserver.validations==1 && swiffHostObserver.validatedName=="Named" && seamCalls==seams,
        "the accepted name is validated once and nothing is reported");
    LocalProfile* record=api->getCurrentProfile();
    check(record && record->name=="0002",
        "the first free four-digit directory is chosen, the scan restarting at the first record");
    check(record && record->profileName=="Named" && record->nick=="Nick" && record->gamespyNick.empty()
        && record->password.empty() && record->email.empty(),
        "the record carries the directory, the profile name, the nick and three empty strings");
    check(general.defaults==generalDefaults+1 && gameServer.resets==gameServerResets+1
        && gameServer.voipSets==voipSets+1 && gameServer.voipQuality==5 && audio.defaultsCalls==audioDefaults+1
        && video.defaults==videoDefaults+1 && haptic.defaults==hapticDefaults+1,
        "the five settings owners are reset once each and the voip quality is 5");
    check(control.resets==controlResets+1 && control.defaults==controlDefaults+1,
        "the control maps are reset and then defaulted, in that order");
    check(global.defaultUser=="0002" && global.defaultUserSets==defaultUsers+1,
        "the new directory becomes the default user");
    check(mapList->m_configFile==root+"0002/mapList.con","the map list is pointed at the new profile");
    check(serverSettings->m_configFile==root+"Default/ServerSettings.con",
        "the server settings are pointed at Default/ServerSettings.con");
    check(settings->stringGet("GSPlayerName","")=="Nick",
        "GSPlayerName is the nick when the new profile has no gamespy nick");
    check(LocalProfileFixture::dirty(manager),"the profile cache is marked dirty");
    check(files.writtenPaths.size()>=2 && files.writtenPaths[0]==root+"0002/Profile.con",
        "the new profile is saved, starting with its own Profile.con");
    check(api->getCurrentProfile()==record,"unlike createDefaultProfile, the record is KEPT current");
    check(!api->createProfile("Named","Nick"),"a second creation refuses while that profile is current");
    swiffHostObserver.validationAnswer=false;registry.swiff=false;
    mapList->m_configFile.clear();
}
#endif
#ifdef BF2_PROFILE_DISCOVERY_FIXTURE
// [TEMP]
// Exercises both results of the typed profile-login call.
static void onlineLoginArmTests(Events& events,Files& files,Registry& registry)
{
    for (bool answer : {false, true}) {
        LocalProfileManager manager;events.current=&manager;ILocalProfileManager* api=&manager;
        LifetimeOnlineProfile online;registry.online=&online;online.loginAnswer=answer;
        ProfileStream stream;files.stream=&stream;ProfileConsole console;
        io::OldConsole* saved=io::mainConsole;io::mainConsole=&console;
        console.onExecute=[&](){ api->setGamespyNick("Gs"); };
        std::vector<LocalProfile> rows(1);rows[0].name="0001";LocalProfileFixture::cache(manager,rows,true);
        files.reset();
        const bool result=api->login(0,"unused","pw");
        check(online.logins==1 && online.lastNick=="Gs" && online.lastPassword=="pw",
            "the loaded gamespy nick and the login password reach the typed ProfileManager +14");
        if (answer) {
            check(result && api->getCurrentProfile(),"an accepted presence login keeps the record and completes");
        } else {
            check(!result && !api->getCurrentProfile() && online.logouts==1,
                "a refused presence login shuts the manager down and fails");
        }
        files.stream=nullptr;io::mainConsole=saved;registry.online=nullptr;
    }
}
#endif

static void death(const char* mode,const char* expected,const char* prelude=nullptr)
{
    char executable[MAX_PATH]; GetModuleFileNameA(nullptr,executable,MAX_PATH);
    std::string command=std::string("\"")+executable+"\" "+mode;
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE reader=nullptr,writer=nullptr;
    if(!CreatePipe(&reader,&writer,&security,0)) { check(false,"diagnostic pipe"); return; }
    SetHandleInformation(reader,HANDLE_FLAG_INHERIT,0);
    STARTUPINFOA startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdOutput=startup.hStdError=writer; startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    bool started=CreateProcessA(nullptr,&command[0],nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE;
    CloseHandle(writer); check(started,"start diagnostic child");
    if(!started) { CloseHandle(reader); return; }
    DWORD wait=WaitForSingleObject(process.hProcess,10000); check(wait==WAIT_OBJECT_0,"diagnostic child stops promptly");
    if(wait!=WAIT_OBJECT_0) TerminateProcess(process.hProcess,99);
    DWORD result; GetExitCodeProcess(process.hProcess,&result);
    std::string output; char data[512]; DWORD count;
    while(ReadFile(reader,data,sizeof(data),&count,nullptr)&&count) output.append(data,count);
    CloseHandle(reader); CloseHandle(process.hThread); CloseHandle(process.hProcess);
    check(result!=0 && result!=STILL_ACTIVE,"diagnostic cannot succeed");
    const std::size_t expectedOffset=prelude?std::strlen(prelude):0;
    if(prelude)check(output.find(prelude)==0,"expected callback completes before the next production frontier");
    if(output.find(expected)!=expectedOffset) std::printf("Diagnostic mismatch for %s: expected %s; actual %s\n",mode,expected,output.c_str());
    check(output.find(expected)==expectedOffset,"qualified diagnostic is first production output after expected callbacks");
    check(output.find("unexpected return")==std::string::npos,"diagnostic never returns");
}

int main(int argc,char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    Registry registry; Events events; classManager=&registry; eventManager=&events;
    ProfileAudio audio;g_audioSettings=&audio;
    ProfileGeneral general;g_generalSettings=&general;
    ProfileGameServer gameServer;g_gameServerSettings=&gameServer;
    ProfileVideo video;g_videoSettings=&video;
    ProfileControl control;g_controlSettings=&control;
    ProfileHaptic haptic;g_hapticSettings=&haptic;
    ProfileGlobal global;g_globalSettings=&global;
    SettingsRepostitory repository;settings=&repository;
    new (&serverSettings->m_configFile) std::string();
    ProfileControlMaps controlMaps;io::controlMapManager=&controlMaps;
    ProfileDemoLibrary demoLibrary;demoLibraryOwner=&demoLibrary;demoLibraryObserver=&demoLibrary;
#ifdef BF2_PROFILE_DISCOVERY_FIXTURE
    Files& files=*new Files;io::fileManager=&files;
#endif
    if(argc==2) {
        LocalProfileManager manager; events.current=&manager; std::string value;
        ILocalProfileManager* prefix=&manager;
        if(!std::strcmp(argv[1],"delete-operation")) {
            std::vector<LocalProfile> rows(1);rows[0].name="0001";LocalProfileFixture::cache(manager,rows,true);prefix->deleteProfile(0);
        }
        if(!std::strcmp(argv[1],"online-account")) {
            LocalProfileFixture::seed(manager); prefix->createOnlineAccount(value,value,value,value);
        }
        if(!std::strcmp(argv[1],"slot30")) prefix->frontierPrimary30();
        if(!std::strcmp(argv[1],"slot34")) prefix->frontierPrimary34();
        if(!std::strcmp(argv[1],"online-login")) prefix->loginOnline(value,value,false);
        if(!std::strcmp(argv[1],"online-alternate")) prefix->loginOnlineAlternate(value,value,true);
        if(!std::strcmp(argv[1],"voice-load")) prefix->loadVoiceSettings();
        if(!std::strcmp(argv[1],"voice-save")) prefix->saveVoiceSettings();
        if(!std::strcmp(argv[1],"voice-read")) { float a=1,b=2,c=3;bool d=false;prefix->readVoiceSettings(a,b,c,d); }
#ifdef BF2_PROFILE_DISCOVERY_FIXTURE
#endif
        if(!std::strcmp(argv[1],"slot70")) localProfileManager_vslot0x70(&manager,value);
        if(!std::strcmp(argv[1],"slotD0")) localProfileManager_vslot0xD0(&manager,value,1);
        std::puts("unexpected return"); return 0;
    }
    check(sizeof(void*)==4,"x86 pointers");
    check(sizeof(LocalProfile)==6*sizeof(std::string)+8,"native six-string profile shape");
    check(sizeof(LocalDemoBookmark)==4*sizeof(std::string),"native four-string bookmark shape");
    LocalProfile record;
    check(record.name.empty() && record.profileName.empty() && record.nick.empty() && record.gamespyNick.empty()
        && record.email.empty() && record.password.empty() && record.totalPlayedTime==0.0f && record.numTimesLoggedIn==0,"actual record constructor defaults");
    for(unsigned iid:{IID_IBase,IID_ILocalProfileManager}) {
        IBase* object=LocalProfileManager::create(iid,nullptr);
        auto* manager=static_cast<LocalProfileManager*>(object); events.current=manager;
        watchedOwner=manager; freedOwners=0;
        check(LocalProfileFixture::fresh(*manager),"all real manager constructor defaults");
        check(!manager->createOnlineAccount("nick","email","password","country") && LocalProfileFixture::fresh(*manager),"online creation refuses absent current profile without state changes");
        check(reinterpret_cast<char*>(static_cast<IEventListener*>(manager))-reinterpret_cast<char*>(manager)==4,"secondary interface offset4");
        check(object->getRef()==1,"factory gives initial reference without addRef");
        check(object->queryInterface(IID_IBase)==manager && object->queryInterface(IID_ILocalProfileManager)==manager,"two query interfaces retain primary identity");
        check(!object->queryInterface(0) && !object->queryInterface(40000) && object->getRef()==1,"unknown IID and no query retention");
        IEventListener* listener=manager;
        const unsigned before=events.removals;
        listener->handleEvent(ECGame,99,reinterpret_cast<EventNode*>(1),reinterpret_cast<void*>(2));
        check(events.removals==before && LocalProfileFixture::fresh(*manager),"actual empty event handler leaves state untouched");
        object->addRef(); check(object->getRef()==2 && object->release()==1 && !freedOwners,"retained reference prevents delete");
        LocalProfileFixture::seed(*manager);
        check(manager->getCurrentProfile()==LocalProfileFixture::current(*manager) && object->getRef()==1,
            "actual current-profile getter returns borrowed native record without retaining");
        unsigned audioBefore=audio.calls;
        check(!manager->createDefaultProfile() && audio.calls==audioBefore,"retail createDefault rejects existing current profile without audio call");
        ILocalProfileManager* prefix=manager;
        const unsigned beforeGuardQueries=registry.queries;
        std::string argument(80,'p');
        check(!prefix->login(-1,argument,argument),"login rejects current profile before enumeration, despite invalid index");
        check(!prefix->createProfile(argument,argument),"create rejects current profile before validation");
        check(registry.queries==beforeGuardQueries && LocalProfileFixture::seeded(*manager),"login/create refusal preserves state without callbacks");
        const unsigned beforeSwiffQueries=registry.swiffQueries;
        check(!prefix->deleteProfile(-1),"delete with current profile reports refusal before checking index");
        check(registry.swiffQueries==beforeSwiffQueries+1,"delete refusal asks actual SwiffHost singleton once");
        const unsigned beforeSeamCalls=seamCalls;   // this block runs once per interface id
        check(seamCalls==beforeSeamCalls,"absent SwiffHost posts no message record");
        registry.swiff=true;
        check(!prefix->deleteProfile(-1) && seamCalls==beforeSeamCalls+1 && seamHost==&swiffHostObserver && seamType==3 && seamFlag==3
            && seamText=="Unable to delete local profile while you are connected","present SwiffHost receives the {3, message, 3} record");
        registry.swiff=false;
        check(LocalProfileFixture::seeded(*manager) && argument==std::string(80,'p'),"all refusal paths preserve current profile, cache and input strings");
        LocalProfileFixture::setGamespyNick(*manager,"existing");
        const unsigned beforeOnlineQueries=registry.queries;
        check(!prefix->createOnlineAccount(argument,argument,argument,argument),"online creation refuses already associated current profile");
        check(registry.queries==beforeOnlineQueries && LocalProfileFixture::seeded(*manager),"online refusal performs no registry calls and preserves state");
        LocalProfileFixture::setGamespyNick(*manager,"");
        freedStrings.clear(); freedStrings.reserve(20); watchStrings=true;
        check(object->release()==0,"final release returns zero"); watchStrings=false;
        check(freedOwners==1 && events.removals==before+1,"final release destroys exactly once and unregisters once");
        check(freedStrings==std::vector<unsigned>({0,1,2,3,4,5,6}),"current record then member string/profile vector/bookmark vector destruction order");
        watchedOwner=nullptr; events.current=nullptr;
    }
    const unsigned q=registry.queries;
    check(!LocalProfileManager::create(IID_ILocalProfileManager,reinterpret_cast<IBase*>(1)) && registry.queries==q,"aggregation rejected before construction");
    captureFactoryAllocation=true; freedOwners=0;
    check(!LocalProfileManager::create(0xffffffff,nullptr),"factory rejects unsupported interface");
    check(freedOwners==1 && !captureFactoryAllocation,"unsupported interface releases real constructed owner");
    watchedOwner=nullptr; factoryOwner=nullptr;
    LocalProfileManager* manager=new LocalProfileManager; events.current=manager;
    ILocalProfileManager* prefix=manager;
    prefix->shutdown(); check(LocalProfileFixture::dirty(*manager),"typed shutdown slot: dirty store follows unregister callback");
    delete manager; events.current=nullptr;
    onlineShutdownTests(events,registry);
    cachedTests(events);events.current=nullptr;
    fieldTests(events,registry,audio);events.current=nullptr;
    codecTests(events);events.current=nullptr;
    applyTests(events);events.current=nullptr;
    quickLoginTests(events,registry);events.current=nullptr;
    createProfileTests(events,registry,general,control,global);events.current=nullptr;
#ifdef BF2_PROFILE_DISCOVERY_FIXTURE
    discoveryTests(events,files);events.current=nullptr;
    validationTests(events,files);events.current=nullptr;
    saveLoadTests(events,files,global,audio,general,gameServer,video,control,haptic,controlMaps);events.current=nullptr;
    createProfileDiscoveryTests(events,files,registry,global,general,gameServer,audio,video,control,haptic);events.current=nullptr;
    onlineLoginArmTests(events,files,registry);events.current=nullptr;
#endif
    // The default-profile creation, both saves, both loads and the settings apply are complete
    // production paths now; they are exercised in-process (applyTests, saveLoadTests) instead of
    // as diagnostic children.
    death("delete-operation","[FRONTIER] dice::hfe::LocalProfileManager::deleteProfile:");
    death("online-account","[FRONTIER] dice::hfe::LocalProfileManager::createOnlineAccount:");
    death("slot30","[FRONTIER] dice::hfe::LocalProfileManager::frontierPrimary30:");
    death("slot34","[FRONTIER] dice::hfe::LocalProfileManager::frontierPrimary34:");
    for(const auto& pair:{std::make_pair("online-login","loginOnline"),std::make_pair("online-alternate","loginOnlineAlternate"),
        std::make_pair("voice-load","loadVoiceSettings"),std::make_pair("voice-save","saveVoiceSettings"),
        std::make_pair("voice-read","readVoiceSettings")}) {
        std::string expected=std::string("[FRONTIER] dice::hfe::LocalProfileManager::")+pair.second+":";
        death(pair.first,expected.c_str());
    }
    for(const auto& pair:{std::make_pair("slot70","localProfileManager_vslot0x70"),std::make_pair("slotD0","localProfileManager_vslot0xD0")}) {
        std::string expected=std::string("[FRONTIER] dice::hfe::")+pair.second+":";
        death(pair.first,expected.c_str());
    }
    std::printf("LocalProfile lifetime: %u/%u passed; native manager=%zu record=%zu bookmark=%zu\n",checks-failures,checks,sizeof(LocalProfileManager),sizeof(LocalProfile),sizeof(LocalDemoBookmark));
    return failures?1:0;
}
