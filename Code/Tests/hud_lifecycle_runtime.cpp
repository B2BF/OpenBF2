// [TEMP] Actual HUD update/destructor and native owner lifetimes. Typed
// collaborators observe callbacks; map/picture construction is not fabricated.
// [RETIRE-WHEN: HUD-LIFECYCLE-FIXTURE] Full HUD/widget startup and shutdown
// cover these paths without external observers or /FORCE-only unused entries.
#define main unusedChatLifecycleRegression
#include "hud_chat_send_runtime.cpp"
#undef main
#include "../BF2/Menu/Ref2System.h"
#include "../BF2/Menu/HudInformationAdapter.h"
#include "../BF2/Menu/HudQuestionManager.h"
#include "../BF2/IO/Input/Input.h"
#include "../BF2/Sound/NullSound.h"
#include <cmath>
using dice::hfe::io::IInputFilter;
namespace dice { namespace hfe {
class IMenuLogic; IMenuLogic* menuLogic=nullptr;
IBase* spawnManager=nullptr;
namespace io { IBase* inputManager=nullptr; }
} }
struct UpdatePlayer : Player {
    bool hasVehicle=true;
    world::IObject* getVehicle() const override {
        trace.push_back("vehicle");
        // Presence-only return; update never dispatches through this token.
        return hasVehicle ? reinterpret_cast<world::IObject*>(1) : nullptr;
    }
    bool getIsCommander() const override {return false;}
};
struct UpdateLogic : Logic {
    int status=1, queries=0;
    int getGameStatus() override {++queries;trace.push_back("status");return status;}
};
struct UpdateGame : Game {
    bool demo=false;
    std::vector<std::string> replies;
    bool isPlayingDemo() override {trace.push_back("demo");return demo;}
    bool radioMessage(TargetChannel channel,const std::string& message,world::IPlayer*,unsigned leader,bool announce,bool flag) override {
        check(channel==14&&leader==17&&!announce&&!flag,"expired request radio arguments");
        replies.push_back(message);return false;
    }
};
struct UpdateHud : BF2HudManager {
    int mode=2,previous=7,ticks=0,rounds=0,modeQueries=0;
    std::vector<std::string> help;
    int getMode() override {++modeQueries;return mode;}
    int getPreviousMode() override {return previous;}
    void setMode(int value,bool save) override {check(!save,"message fallback does not save mode");mode=value;}
    void tickChatHook(float) override {++ticks;trace.push_back("chatTick");}
    void showRoundResult() override {++rounds;}
    std::wstring getLocalizedString(std::string key) override {help.push_back(key);return L"help";}
    std::wstring replaceWideInputKeys(std::wstring value) override {return value+L" keys";}
    void showWideHelp(std::wstring text,float seconds) override {check(text==L"help keys"&&seconds==5,"demo localized help and duration");}
};
struct UpdateUI : menu::Ref2System {
    int updates=0, closes=0;
    bool* destroyed=nullptr;
    std::function<void()> onClose;
    UpdateUI():Ref2System(800,600) {}
    ~UpdateUI() override {if(destroyed)*destroyed=true;trace.push_back("uiDelete");}
    bool update(float) override {++updates;trace.push_back("uiUpdate");return false;}
    bool close() override {++closes;trace.push_back("uiClose");if(onClose)onClose();return false;}
};
struct UpdateAdapter : HudInformationAdapter {
    bool visible=true;
    std::vector<std::string> hidden;
    std::function<void(const std::string&)> onGet;
    bool getBool(std::string key) override {trace.push_back(key);if(onGet)onGet(key);return visible;}
    bool setBool(std::string key,bool value) override {check(!value,"timers hide flag");hidden.push_back(key);return false;}
};
struct UpdateSquads : ActionSquadBoundary {
    bool setSquadName(int,int,int,const std::string&,bool) override {actionUnsupported("rename squad");}
    void inviteToSquad(int,int,int,bool,bool) override {actionUnsupported("invite squad");}
    void removeFromSquad(int,int,int,bool) override {actionUnsupported("remove squad");}
    int getCommander(int) override {actionUnsupported("get commander");}
    std::vector<int> squads,types;
    bool setIsAccepted(int team,int squad,bool accepted,OrderType order,SquadOrderType type,bool flag) override {
        check(team==2&&!accepted&&order==2&&!flag,"expired request acceptance arguments");
        squads.push_back(squad);types.push_back(type);return false;
    }
    int getSquadLeader(int team,int) override {check(team==2,"reply resolves local team");return 17;}
};
struct LifetimeEvents : EventManager {
    IEventListener* expected=nullptr;
    std::vector<int> categories;
    bool unregisterEventHandler(EventCategory category,IEventListener* listener) override {
        if(listener==expected){categories.push_back(category);trace.push_back("unregister");}
        return EventManager::unregisterEventHandler(category,listener);
    }
};
struct LifetimeInput : io::IInputManager {
    IInputFilter* removed=nullptr;
    void addRef() override {actionUnsupported("input addRef");}
    int getRef() override {actionUnsupported("input getRef");}
    int release() override {actionUnsupported("input release");}
    void* queryInterface(unsigned) const override {actionUnsupported("input query");}
    bool init(unsigned) override {actionUnsupported("input init");}
    bool shutdown() override {actionUnsupported("input shutdown");}
    bool setHasFocus(bool,unsigned) override {actionUnsupported("input focus");}
    void update(int,float) override {actionUnsupported("input update");}
    bool registerInputFilter(IInputFilter*) override {actionUnsupported("input register");}
    bool unregisterInputFilter(IInputFilter* p) override {removed=p;trace.push_back("inputRemove");return false;}
    bool setGameControlMap(std::string) override {actionUnsupported("input game map");}
    bool setPlayerControlMap(std::string) override {actionUnsupported("input player map");}
    std::string getPlayerControlMap() override {actionUnsupported("input get map");}
    io::PlayerInput* getNewPlayerInputSample() override {actionUnsupported("input sample");}
    io::GameInput* getNewGameInputSample() override {actionUnsupported("input sample");}
    bool advanceNextUnread() override {actionUnsupported("input advance");}
    void catchUp() override {actionUnsupported("input catchup");}
    void setPlayerInputDelay(int) override {actionUnsupported("input delay");}
    int getNumEntries() override {actionUnsupported("input entries");}
};
struct LifetimeSound : NullSound {
    int releases=0;
    int release() override {++releases;trace.push_back("soundRelease");return 1;}
};
int main()
{
    static_assert(sizeof(void*)==4,"x86");
    static_assert(sizeof(HudPendingRequest)==12,"native pending request");
    static_assert(sizeof(BF2HudManager)==824,"native HUD size preserved");
    LifetimeEvents events;eventManager=&events;
    LifetimeInput input;io::inputManager=&input;
    Players players;world::playerManager=&players;
    UpdatePlayer player;players.local=&player;
    UpdateLogic logic;UpdateGame g;game=&g;
    UpdateSquads squads;squadManager=&squads;
    auto* h=new UpdateHud;hudManager=h;
    UpdateUI ui;UpdateAdapter adapter,second;
    HudQuestionManager questions;
    const Vec3 direction={0,0,1};
    // Menu pointer is read only for presence; it is never dereferenced.
    menuLogic=reinterpret_cast<IMenuLogic*>(1);
    check(!h->update(1,nullptr,direction),"menu early return");
    menuLogic=nullptr;h->m_field15c=false;h->m_field248=false;gameLogic=nullptr;
    check(!h->update(1,nullptr,direction),"mouse inactive, no game logic");
    h->m_field15c=true;
    check(!h->update(1,nullptr,direction),"mouse active, no game logic");
    gameLogic=&logic;logic.status=3;trace.clear();
    check(!h->update(1,&player,direction)&&trace==std::vector<std::string>{"status","demo"},"status3 skips player access");
    logic.status=1;player.hasVehicle=false;
    check(!h->update(1,&player,direction),"player without vehicle takes fallback");
    player.hasVehicle=true;h->m_field0bf=true;h->m_field2f5=true;h->m_field2f8=4;
    check(h->update(0.25f,&player,direction)&&h->m_field2f8==4.25f&&!h->ticks,"active path suppresses mode callbacks but advances held input");
    h->m_field0f4=&ui;h->m_field104=&adapter;
    h->m_field234=1;h->m_field238=0;h->m_field22c=std::numeric_limits<float>::quiet_NaN();
    trace.clear();h->update(1,&player,direction);
    check(h->m_field234==0&&h->m_field238==-1&&std::isnan(h->m_field22c),"icon timers preserve strict zero and NaN");
    check(adapter.hidden==std::vector<std::string>{"ShowMedalIcon"},"only negative timer hides icon");
    check(trace==std::vector<std::string>{"status","vehicle","alive","uiUpdate","ShowRankUpIcon","ShowMedalIcon","IngameHelpShow","status"},"UI and icon callbacks retain retail order");
    adapter.onGet=[&](const std::string&){h->m_field104=&second;};
    h->m_field234=-1;h->update(1,&player,direction);
    check(!second.hidden.empty()&&second.hidden.front()=="ShowRankUpIcon","timer setter reloads adapter after getter callback");
    adapter.onGet={};h->m_field0f4=nullptr;h->m_field104=nullptr;
    h->m_field0bf=false;h->m_field100=&questions;h->m_field2b8=0;h->m_field2b0=true;
    h->update(0,&player,direction);
    check(h->ticks==1&&!h->m_field2b0,"notification zero timer clears blink before chat hook");
    h->mode=10;h->update(0,&player,direction);check(h->mode==7,"empty messages restores previous mode");
    h->mode=11;h->update(0,&player,direction);check(h->rounds==1,"round mode invokes round result");
    h->mode=0;players.local=nullptr;h->update(0,&player,direction);
    check(h->ticks==4,"missing local player only exits mode helper");players.local=&player;
    h->mode=2;h->m_field32c={{1,0,0},{2,1,0.5f},{3,2,0},{4,3,0},{5,9,0},{6,0,std::numeric_limits<float>::quiet_NaN()}};
    h->update(0,&player,direction);
    check(squads.squads==std::vector<int>{1,3,4,5}&&g.replies==std::vector<std::string>{"artillery_in_use","vehicledrop_in_use","supplies_in_use"},"request expiry erases adjacent entries and preserves message mapping");
    check(h->m_field32c.size()==2&&h->m_field32c[0].squad==2&&std::isnan(h->m_field32c[1].remaining),"pending positive and NaN survive");
    players.local=nullptr;h->update(1,&player,direction);
    check(h->m_field32c[0].remaining==0.5f,"no local player leaves pending timers untouched");players.local=&player;
    gameLogic=nullptr;g.demo=true;h->m_field23c=9;h->m_field240=false;
    h->update(1,nullptr,direction);h->update(10,nullptr,direction);h->update(100,nullptr,direction);
    check(h->help==std::vector<std::string>{"HUD_HELP_BATTLERECORDER_controlsQ","HUD_HELP_BATTLERECORDER_controlsT"}&&h->m_field23c==-1,"demo help has two timed stages then stays disabled");
    h->m_field23c=std::numeric_limits<float>::quiet_NaN();h->update(100,nullptr,direction);
    check(h->help.size()==2,"NaN demo timer is inactive");
    h->m_field100=nullptr;h->m_field32c.clear();
    auto* original=new UpdateUI;auto* replacement=new UpdateUI;bool originalGone=false,replacementGone=false;
    original->destroyed=&originalGone;replacement->destroyed=&replacementGone;
    original->onClose=[&]{h->m_field0f4=replacement;};h->m_field0f4=original;
    LifetimeSound sound;h->m_field2f0=&sound;events.expected=h;trace.clear();
    IInputFilter* expectedInput=h;delete h;
    check(replacementGone&&!originalGone&&original->closes==1,"destructor reloads UI owner after close callback");
    check(input.removed==expectedInput&&events.categories==std::vector<int>{2,1,0,10},"destructor unregisters native input/event faces in order");
    check(trace==std::vector<std::string>{"uiClose","uiDelete","inputRemove","unregister","unregister","unregister","unregister","soundRelease"}&&sound.releases==1,"saved sound release follows UI and event teardown");
    delete original;hudManager=nullptr;
    std::printf("HUD lifecycle/update: %u/%u checks passed\n",checks-failures,checks);
    return failures?1:0;
}
