// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: PROFILE-CONSOLE-FIXTURE]
// Real module, native owner/list, real console registry and every metadata slot.
// Allocation observation and opaque non-dereferenced map values are fixture-only. Since frontier
// batch 8 the rows execute; their behaviour is checked on macOS (bf2_game_frontier_check).
#include <windows.h>
#include <crtdbg.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include "../BF2/Menu/ProfileConsoleBindings.h"
#include "../BF2/Menu/ClientConsoleModules.h"
#include "../BF2/Core/ConsoleObjectBase.h"

namespace dice { namespace hfe { namespace hud { class BF2HudManager; }
hud::BF2HudManager* hudManager = nullptr; // HUD provider linked, not invoked
class IGameLogic; IGameLogic* gameLogic = nullptr;                            // applySettings, not invoked
namespace world { class PlayerManager; PlayerManager* playerManager = nullptr; }
} }
using namespace dice::hfe;
using namespace dice::hfe::hud;
static unsigned checks, failures;
static bool observeDestruction, captureNextAllocation;
static unsigned destructionCalls, destructionOrderFailures, borrowedDeletes;
static void* capturedAllocation;
static void* borrowedValue;
void* operator new(std::size_t size)
{
    void* value = std::malloc(size ? size : 1);
    if (!value) throw std::bad_alloc();
    if (captureNextAllocation) { capturedAllocation = value; captureNextAllocation = false; }
    return value;
}
void operator delete(void* value) noexcept
{
    if (observeDestruction && value) {
        ++destructionCalls;
        if (profileConsoleOwner || !mainConsoleObjects->getConsoleObjects().empty()) ++destructionOrderFailures;
    }
    if (value && value == borrowedValue) ++borrowedDeletes;
    std::free(value);
}
void operator delete(void* value, std::size_t) noexcept { ::operator delete(value); }
static void check(bool pass, const char* label)
{ ++checks; if (!pass) { ++failures; std::printf("FAIL %s\n", label); } }

namespace dice { namespace hfe { namespace hud {
struct ProfileConsoleFixture {
    static bool empty(const ProfileConsoleOwner& owner)
    { return owner.m_emptyName.empty() && owner.m_profiles.empty() && !owner.m_selectedProfile && !owner.m_activeProfile; }
    static void seed(ProfileConsoleOwner& owner, PlayerProfile* value)
    {
        owner.m_emptyName.assign(90, 'n');
        owner.m_profiles.emplace(std::string(100, 'k'), value);
        owner.m_profiles.emplace("Case", value);
        owner.m_profiles.emplace("case", value);
    }
    static std::size_t count(const ProfileConsoleOwner& owner) { return owner.m_profiles.size(); }
    static void select(ProfileConsoleOwner& owner, PlayerProfile* value) { owner.m_selectedProfile = value; }
    static void activate(ProfileConsoleOwner& owner, PlayerProfile* value) { owner.m_activeProfile = value; }
};
} } }

struct Expected { const char* name; int type, min, max; const char* result; };
static const Expected expected[] = {
    {"createPlayerProfile",0,1,1,"bool"}, {"activePlayerProfile",1,0,1,"std::string"},
    {"activatePlayerProfile",0,1,1,"bool"}, {"rank",1,0,1,"int"},
    {"addCharacterProfile",0,1,1,"bool"}, {"activeCharacterProfile",1,0,1,"std::string"},
    {"activateCharacterProfile",0,1,1,"bool"}, {"addModSetting",0,1,1,"bool"},
    {"activeMod",1,0,1,"std::string"}, {"playerName",1,0,1,"std::string"},
    {"visualUnlockLevel",1,0,1,"int"}, {"defaultKit",1,0,1,"int"},
    {"tagIcon",1,0,1,"std::string"}, {"tagText",1,0,1,"std::string"},
    {"setIndexForKit",0,3,3,"void"}, {"getIndexForKit",0,2,2,"void"}, {"applySettings",0,0,0,"void"}
};

int main()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::vector<ConsoleObject*> bindings(profileConsoleBindingList().begin(), profileConsoleBindingList().end());
    check(sizeof(void*) == 4,"x86 pointers");
    check(sizeof(ProfileConsoleOwner) == sizeof(std::string)+2*sizeof(void*)+sizeof(std::map<std::string,PlayerProfile*>),"native owner shape");
    check(sizeof(PlayerProfile) == sizeof(std::string)+16+sizeof(std::map<std::string,CharacterProfile*>),"native player storage shape");
    check(!profileConsoleOwner,"initial owner is null");
    check(bindings.size() == 17,"all17 bindings");
    check(mainConsoleObjects->getConsoleObjects().empty(),"initial registry empty");
    for (std::size_t i=0;i<bindings.size();++i) {
        ConsoleObject* b=bindings[i]; const Expected& e=expected[i];
        check(!std::strcmp(b->getObjectName(),"profile"),"exact lowercase object name");
        check(!std::strcmp(b->getFunctionName(),e.name),"exact ordered function name");
        check(b->getType()==e.type && b->isSetGet()==(e.type==1),"property direction");
        check(b->getAccess()==2 && b->getCategory()==0,"defaults");
        check(b->getMinNoArgs()==e.min && b->getMaxNoArgs()==e.max,"arity");
        check(!std::strcmp(b->getReturnType(),e.result),"advertised return incl getIndexForKit void");
        check(b->isDynamic()==(i<14) && !b->hasReturnValue(),"dynamic and initial result flag");
        check(b->getObject()==nullptr && !b->isActive(),"actual base-null object and absent-owner availability");
        for(int j=0;j<e.max;++j) {
            const char* type=(i==3 || i==10 || i==11 || j>0) ? "int" : "std::string";
            check(!std::strcmp(b->getArgType(j),type),"exact argument type");
        }
        check(!b->hasRange() && !b->hasBundledType(),"actual default range and bundle flags");
        auto range=b->getRange(); check(!range.first && !range.second,"actual zero pointer pair");
        auto text=b->getRangeAsString(); check(text.first.empty() && text.second.empty(),"actual empty range strings");
        check(b->getDeltaAsString()=="No Delta" && b->getTypeDescription().empty(),"PE delta and type text");
    }
    ProfileConsoleModule module;
    check(module.close() && !profileConsoleOwner,"close before init");
    check(module.init(),"actual module init returns true");
    ProfileConsoleOwner* first=profileConsoleOwner;
    check(first && ProfileConsoleFixture::empty(*first),"native owner initialized and published");
    check(mainConsoleObjects->getConsoleObjects().size()==17,"actual registry has17 entries");
    auto it=mainConsoleObjects->getConsoleObjects().begin();
    for(std::size_t i=0;i<bindings.size();++i,++it) {
        check(*it==bindings[i],"registry keeps binding identity and order");
        check(bindings[i]->isActive()==(i<3),"only owner commands initially active");
        check(mainConsoleObjects->getConsoleObject("PROFILE",expected[i].name)==(i<3?bindings[i]:nullptr),"real case-insensitive lookup uses availability");
    }
    ProfileConsoleOwner* second=new ProfileConsoleOwner;
    check(profileConsoleOwner==first && ProfileConsoleFixture::empty(*second),"second owner does not replace first");
    delete second; check(profileConsoleOwner==first,"second destruction preserves published owner");
    int opaqueValue=7;
    borrowedValue=&opaqueValue;
    PlayerProfile* token=reinterpret_cast<PlayerProfile*>(&opaqueValue); // never dereferenced
    ProfileConsoleFixture::select(*first,token);
    for (int i=3;i<6;++i) check(bindings[i]->isActive(),"selected profile activates direct profile bindings");
    check(!bindings[6]->isActive(),"active profile remains separate");
    ProfileConsoleFixture::activate(*first,token);
    check(bindings[6]->isActive(),"active profile enables activation command");
    ProfileConsoleFixture::select(*first,nullptr); ProfileConsoleFixture::activate(*first,nullptr);
    ProfileConsoleFixture::seed(*first,token);
    check(ProfileConsoleFixture::count(*first)==3,"native map comparator preserves distinct case keys");
    // Registry teardown allocates temporary keys; observe only from the first owner's
    // destructor onward using its native long-string buffer deallocations below.
    mainConsoleObjects->unregisterConsoleObjects(profileConsoleBindingList());
    observeDestruction=true; delete first; observeDestruction=false;
    check(!profileConsoleOwner && destructionCalls>=5 && !destructionOrderFailures,"global cleared before all native key/map/string frees");
    check(!borrowedDeletes && opaqueValue==7,"mapped profile values are not deleted"); borrowedValue=nullptr;
    check(module.init() && module.close() && !profileConsoleOwner && mainConsoleObjects->getConsoleObjects().empty(),"real module init/close cycle");
    check(module.init(),"repeated init first owner"); first=profileConsoleOwner;
    captureNextAllocation=true; check(module.init(),"repeated init second allocation");
    second=static_cast<ProfileConsoleOwner*>(capturedAllocation);
    check(profileConsoleOwner==first && second!=first && ProfileConsoleFixture::empty(*second),"retail repeat-init leaves second owner unpublished");
    check(mainConsoleObjects->getConsoleObjects().size()==34,"repeat-init duplicates registration");
    check(module.close() && !profileConsoleOwner && mainConsoleObjects->getConsoleObjects().size()==17,"retail unregister removes only one list copy and published owner");
    check(module.close() && mainConsoleObjects->getConsoleObjects().empty(),"second close removes remaining list copies with null owner");
    delete second; // fixture cleans the deliberately unowned retail repeat-init allocation
    // The invokers replaced the four diagnostic stops (frontier batch 8); their behaviour is covered by
    // bf2_game_frontier_check profileConsole and the loopback script profile-console.
    std::string none;
    check(static_cast<ConsoleObjectBaseImpl*>(bindings[0])->execute(&none,0).empty(),"wrong arity returns no result");
    std::printf("Profile console: %u/%u checks passed; native owner %zu, player storage %zu\n",checks-failures,checks,sizeof(ProfileConsoleOwner),sizeof(PlayerProfile));
    return failures?1:0;
}
