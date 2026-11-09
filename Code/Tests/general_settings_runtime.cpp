// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: GENERAL-SETTINGS-FIXTURE]
// Tests GeneralSettings lifetime, defaults, serialization, and published slots.
// HUD and class-manager pointers remain null, so HUD pushes and singleton lookup are not exercised.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include "../BF2/Game/GeneralSettings.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/IO/SettingsManager.h"
#include "../BF2/Scene/Object/PlayerManager.h"
using namespace dice::hfe;
static unsigned checks, failures;
static void check(bool value,const char* label)
{ ++checks; if(!value){++failures;std::printf("FAIL %s\n",label);} }

namespace dice { namespace hfe {
// [TEMP] The HUD manager slot Game/Main/BF2Engine.cpp defines; left null throughout.
hud::BF2HudManager* hudManager=nullptr;
// [TEMP] The five engine globals the class names, all left null. Only apply's non-null-HUD
// branch reaches any of them, and this fixture keeps hudManager null, so nothing here is
// dereferenced; defining them keeps the link honest (HudInformationAdapter::setMapStatic stays
// the ONE forced external) instead of letting /force:unresolved invent five more.
ClassManager* classManager=nullptr;
EventManager* eventManager=nullptr;
SettingsRepostitory* settings=nullptr;
namespace io { SettingsManagerBase* settingsManager=nullptr; }
namespace world { PlayerManager* playerManager=nullptr; }
struct GeneralSettingsFixture {
    static GeneralSettings& owner(IBase* base){ return *static_cast<GeneralSettings*>(base); }
    static int refs(GeneralSettings& g){ return g.m_refCount; }
    static std::string& sortKey(GeneralSettings& g){ return g.m_sortKey; }
    static int& sortOrder(GeneralSettings& g){ return g.m_sortOrder; }
    static int& numRoundsPlayed(GeneralSettings& g){ return g.m_numRoundsPlayed; }
    static std::string& serverFilter(GeneralSettings& g){ return g.m_serverFilter; }
    static float& hudTransparency(GeneralSettings& g){ return g.m_hudTransparency; }
    static int* crosshair(GeneralSettings& g){ return g.m_crosshairColor; }
    static int* buddytag(GeneralSettings& g){ return g.m_buddytagColor; }
    static int* squadtag(GeneralSettings& g){ return g.m_squadtagColor; }
    static bool& minimapRotate(GeneralSettings& g){ return g.m_minimapRotate; }
    static float& minimapTransparency(GeneralSettings& g){ return g.m_minimapTransparency; }
    static float& mapIconAlpha(GeneralSettings& g){ return g.m_mapIconAlphaTransparency; }
    static bool& viewIntroMovie(GeneralSettings& g){ return g.m_viewIntroMovie; }
    static bool& outOfVoting(GeneralSettings& g){ return g.m_outOfVoting; }
    static std::string& bftv(GeneralSettings& g){ return g.m_bftvSaveDirectory; }
    static bool& confirmQuit(GeneralSettings& g){ return g.m_confirmQuit; }
    static bool& useAdvancedServerBrowser(GeneralSettings& g){ return g.m_useAdvancedServerBrowser; }
    static bool& useBots(GeneralSettings& g){ return g.m_useBots; }
    static int& maxBots(GeneralSettings& g){ return g.m_maxBots; }
    static bool& maxBotsIncludeHumans(GeneralSettings& g){ return g.m_maxBotsIncludeHumans; }
    static float& botSkill(GeneralSettings& g){ return g.m_botSkill; }
    static int& lastAwardsCheckDate(GeneralSettings& g){ return g.m_lastAwardsCheckDate; }
    static bool& allowPunkBuster(GeneralSettings& g){ return g.m_allowPunkBuster; }
    static bool& itemSelectionReverseItems(GeneralSettings& g){ return g.m_itemSelectionReverseItems; }
    static bool& autoReload(GeneralSettings& g){ return g.m_autoReload; }
    static int& connectionType(GeneralSettings& g){ return g.m_connectionType; }
    static std::vector<std::string>& playedVOHelp(GeneralSettings& g){ return g.m_playedVOHelp; }
    static std::vector<GeneralServerEntry>& favourites(GeneralSettings& g){ return g.m_favouriteServers; }
    static std::vector<GeneralServerEntry>& history(GeneralSettings& g){ return g.m_serverHistory; }
    static int& toggleFilters(GeneralSettings& g){ return g.m_toggleFilters; }
    static bool& autoReady(GeneralSettings& g){ return g.m_autoReady; }
    static bool& lcdDisplayModes(GeneralSettings& g){ return g.m_lcdDisplayModes; }
};
} }
typedef GeneralSettingsFixture F;

// The option-value subset setDefaultValues (+20) owns.
static bool optionDefaults(GeneralSettings& g)
{
    const int* c=F::crosshair(g); const int* b=F::buddytag(g); const int* s=F::squadtag(g);
    return F::sortKey(g).empty() && F::sortOrder(g)==0 && F::serverFilter(g).empty()
        && c[0]==255 && c[1]==255 && c[2]==0 && c[3]==204 && b[0]==0 && b[1]==0 && b[2]==0
        && s[0]==0 && s[1]==255 && s[2]==0 && F::hudTransparency(g)==204.0f && !F::minimapRotate(g)
        && F::minimapTransparency(g)==204.0f && F::viewIntroMovie(g) && !F::outOfVoting(g)
        && F::bftv(g).empty() && !F::confirmQuit(g) && !F::lcdDisplayModes(g)
        && F::mapIconAlpha(g)==255.0f && F::allowPunkBuster(g) && F::itemSelectionReverseItems(g)
        && F::autoReload(g) && F::toggleFilters(g)==0;
}
// The remainder setDefaults (+1C) owns before dispatching +20.
static bool outerDefaults(GeneralSettings& g,int earliest,int latest)
{
    return F::playedVOHelp(g).empty() && F::favourites(g).empty() && F::history(g).empty()
        && F::numRoundsPlayed(g)==0 && !F::useAdvancedServerBrowser(g) && !F::useBots(g)
        && F::maxBots(g)==64 && !F::maxBotsIncludeHumans(g) && F::botSkill(g)==0.5f
        && F::lastAwardsCheckDate(g)>=earliest && F::lastAwardsCheckDate(g)<=latest
        && !F::autoReady(g) && F::connectionType(g)==5;
}
static std::string defaultScript(int date)
{
    char row[64]; std::sprintf(row,"GeneralSettings.setLastAwardsCheckDate %d\n",date);
    return std::string("GeneralSettings.setSortOrder 0\nGeneralSettings.setSortKey \"\"\nGeneralSettings.setNumRoundsPlayed 0\n"
        "GeneralSettings.setServerFilter \"\"\nGeneralSettings.setHUDTransparency 204\nGeneralSettings.setCrosshairColor 255 255 0 204\n"
        "GeneralSettings.setBuddytagColor 0 0 0\nGeneralSettings.setSquadtagColor 0 255 0\nGeneralSettings.setMinimapRotate 0\n"
        "GeneralSettings.setMinimapTransparency 204\nGeneralSettings.setViewIntroMovie 1\nGeneralSettings.setOutOfVoting 0\n"
        "GeneralSettings.setBFTVSaveDirectory \"\"\nGeneralSettings.setConfirmQuit 0\nGeneralSettings.setMapIconAlphaTransparency 255\n"
        "GeneralSettings.setUseAdvancedServerBrowser 0\nGeneralSettings.setUseBots 0\nGeneralSettings.setMaxBots 64\n"
        "GeneralSettings.setMaxBotsIncludeHumans 0\nGeneralSettings.setBotSkill 0.5\n")+row+
        "GeneralSettings.setAllowPunkBuster 1\nGeneralSettings.setItemSelectionReverseItems 1\nGeneralSettings.setToggleFilters 0\n"
        "GeneralSettings.setAutoReload 1\nGeneralSettings.setAutoReady 0\nGeneralSettings.setConnectionType 5\nGeneralSettings.setLCDDisplayModes 0\n";
}
static void scramble(GeneralSettings& g)
{
    F::sortKey(g)="ping";F::sortOrder(g)=1;F::numRoundsPlayed(g)=12;F::serverFilter(g)="filter";
    F::hudTransparency(g)=10.5f;int* c=F::crosshair(g);c[0]=1;c[1]=2;c[2]=3;c[3]=4;
    int* b=F::buddytag(g);b[0]=5;b[1]=6;b[2]=7;int* s=F::squadtag(g);s[0]=8;s[1]=9;s[2]=10;
    F::minimapRotate(g)=true;F::minimapTransparency(g)=11.f;F::mapIconAlpha(g)=12.f;F::viewIntroMovie(g)=false;
    F::outOfVoting(g)=true;F::bftv(g)="bftv";F::confirmQuit(g)=true;F::useAdvancedServerBrowser(g)=true;F::useBots(g)=true;
    F::maxBots(g)=16;F::maxBotsIncludeHumans(g)=true;F::botSkill(g)=0.75f;F::lastAwardsCheckDate(g)=7;F::allowPunkBuster(g)=false;
    F::itemSelectionReverseItems(g)=false;F::autoReload(g)=false;F::connectionType(g)=2;F::toggleFilters(g)=3;F::autoReady(g)=true;
    F::lcdDisplayModes(g)=true;
    F::playedVOHelp(g).clear();F::favourites(g).clear();F::history(g).clear();
    F::playedVOHelp(g).push_back("HUD_HELP_A");F::playedVOHelp(g).push_back("HUD_HELP_B");
    GeneralServerEntry fav;fav.name="Fav Server";fav.address="10.0.0.1:16567";fav.queryPort=29900;fav.lastPlayed=0;
    F::favourites(g).push_back(fav);
    GeneralServerEntry hist;hist.name="Past Server";hist.address="10.0.0.2:16567";hist.queryPort=29901;hist.lastPlayed=1234567;
    F::history(g).push_back(hist);
}

int main()
{
    check(sizeof(void*)==4,"x86 fixture");
    __time32_t earliest;_time32(&earliest);
    IBase* object=GeneralSettings::create(IID_IGeneralSettings,nullptr);
    __time32_t latest;_time32(&latest);
    check(object!=nullptr,"factory yields the owner for its interface id");
    IGeneralSettings* api=static_cast<IGeneralSettings*>(object);
    GeneralSettings& owner=F::owner(object);
    check(F::refs(owner)==1 && api->getRef()==1,"refcount starts at one and queryInterface does not retain");
    check(api->queryInterface(IID_IBase)==object && api->queryInterface(IID_IGeneralSettings)==object && !api->queryInterface(0xffffffff),
        "queryInterface accepts IBase and IGeneralSettings only");
    check(!GeneralSettings::create(IID_IGeneralSettings,reinterpret_cast<IBase*>(1)),"aggregation is refused before allocation");
    check(!GeneralSettings::create(0xffffffff,nullptr),"unsupported factory id releases the fresh owner");
    check(api->getFileName()=="General.con","actual filename virtual");
    check(outerDefaults(owner,static_cast<int>(earliest),static_cast<int>(latest)) && optionDefaults(owner),
        "construction runs setDefaults: every serialized field carries its retail default and the awards date is now");
    const int date=F::lastAwardsCheckDate(owner);
    check(api->getSettingsScript()==defaultScript(date),"exact default script, no list rows, retail row order");

    scramble(owner);
    const std::string scrambled=api->getSettingsScript();
    check(scrambled.find("GeneralSettings.setPlayedVOHelp \"HUD_HELP_A\"\nGeneralSettings.setPlayedVOHelp \"HUD_HELP_B\"\nGeneralSettings.setSortOrder 1\n")==0,
        "played help rows precede the sort order row");
    check(scrambled.find("GeneralSettings.addFavouriteServer \"10.0.0.1:16567\" 29900 \"Fav Server\"\n")!=std::string::npos,
        "favourite row prints address, query port, name");
    check(scrambled.find("GeneralSettings.addServerHistory \"10.0.0.2:16567\" 29901 \"Past Server\" 1234567\n")!=std::string::npos,
        "history row adds the last-played value");
    check(scrambled.find("GeneralSettings.setServerFilter \"filter\"\nGeneralSettings.addFavouriteServer")!=std::string::npos,
        "list rows follow the server filter row");
    check(scrambled.find("GeneralSettings.setCrosshairColor 1 2 3 4\nGeneralSettings.setBuddytagColor 5 6 7\nGeneralSettings.setSquadtagColor 8 9 10\n")!=std::string::npos,
        "colour rows print their components space-separated");
    check(scrambled.find("GeneralSettings.setHUDTransparency 10.5\n")!=std::string::npos && scrambled.find("GeneralSettings.setBotSkill 0.75\n")!=std::string::npos,
        "float rows use the default stream precision");
    check(scrambled.find("GeneralSettings.setToggleFilters 3\nGeneralSettings.setAutoReload 0\nGeneralSettings.setAutoReady 1\nGeneralSettings.setConnectionType 2\nGeneralSettings.setLCDDisplayModes 1\n")!=std::string::npos,
        "tail rows keep the retail order with toggle filters before auto reload");
    F::sortKey(owner)=std::string("pi\0ng",5);F::playedVOHelp(owner)[0]=std::string("a\0b",3);
    const std::string mixed=api->getSettingsScript();
    check(mixed.find("GeneralSettings.setSortKey \"pi\"\n")!=std::string::npos,"scalar string rows insert c_str and truncate at an embedded NUL");
    check(mixed.find(std::string("GeneralSettings.setPlayedVOHelp \"a\0b\"\n",36))!=std::string::npos,"list rows insert the whole std::string");

    scramble(owner);
    api->setDefaultValues();
    check(optionDefaults(owner),"setDefaultValues resets the option subset");
    check(F::playedVOHelp(owner).size()==2 && F::favourites(owner).size()==1 && F::history(owner).size()==1 && F::numRoundsPlayed(owner)==12
        && F::useAdvancedServerBrowser(owner) && F::useBots(owner) && F::maxBots(owner)==16 && F::maxBotsIncludeHumans(owner)
        && F::botSkill(owner)==0.75f && F::lastAwardsCheckDate(owner)==7 && F::autoReady(owner) && F::connectionType(owner)==2,
        "setDefaultValues leaves the lists, browser, bot, date, ready and connection values alone");
    scramble(owner);
    F::favourites(owner).reserve(8);F::history(owner).reserve(8);F::playedVOHelp(owner).reserve(8);
    __time32_t before;_time32(&before);
    api->setDefaults();
    __time32_t after;_time32(&after);
    check(outerDefaults(owner,static_cast<int>(before),static_cast<int>(after)) && optionDefaults(owner),
        "setDefaults resets everything and re-reads the 32-bit CRT time");
    check(F::favourites(owner).capacity()==0 && F::history(owner).capacity()==0 && F::playedVOHelp(owner).capacity()==0,
        "setDefaults releases the three lists' storage as the retail destroy/free/zero sequence does");

    scramble(owner);
    const std::string beforeApply=api->getSettingsScript();
    check(!api->apply() && api->getSettingsScript()==beforeApply,"apply without a HUD manager returns false and changes nothing");

    // Checks interface slots while avoiding singleton lookup. Cancel, add, and matching-remove branches are excluded.
    api->setDefaults();
    void** table=*reinterpret_cast<void***>(object);
    typedef unsigned char(__thiscall* BoolSlot)(void*);
    typedef int(__thiscall* IntSlot)(void*);
    typedef unsigned char(__thiscall* MaskSlot)(void*,int);
    F::minimapRotate(owner)=true;F::connectionType(owner)=3;F::toggleFilters(owner)=0x40;F::lcdDisplayModes(owner)=true;
    check(reinterpret_cast<BoolSlot>(table[0x68/4])(object)==1 && reinterpret_cast<IntSlot>(table[0x100/4])(object)==3
        && reinterpret_cast<MaskSlot>(table[0x10C/4])(object,0x40)==1 && reinterpret_cast<MaskSlot>(table[0x10C/4])(object,0x80)==0
        && reinterpret_cast<BoolSlot>(table[0x15C/4])(object)==1,
        "the table holds getMinimapRotate +0x68, getConnectionType +0x100, getToggleFilter +0x10C and getLCDDisplayModes +0x15C");
    api->setPlayedVOHelp("HUD_HELP_Kit");api->setPlayedVOHelp("hud_help_kit");api->setPlayedVOHelp("HUD_HELP_Flag");
    check(F::playedVOHelp(owner).size()==2 && F::playedVOHelp(owner)[0]=="HUD_HELP_Kit" && F::playedVOHelp(owner)[1]=="HUD_HELP_Flag",
        "+0x114 adds a played-help name only when it is absent, case-insensitively");
    check(api->checkPlayedVOHelp("hud_help_FLAG") && !api->checkPlayedVOHelp("HUD_HELP_Other") && !api->checkPlayedVOHelp(""),
        "+0x118 is the case-insensitive find over the same list");
    GeneralServerEntry row;row.name="Fav";row.address="10.0.0.9:16567";row.queryPort=29900;row.lastPlayed=0;
    F::favourites(owner).push_back(row);row.address="Fav.Host:16567";F::favourites(owner).push_back(row);
    check(reinterpret_cast<IntSlot>(table[0x120/4])(object)==2,"+0x120 counts the favourites");
    check(api->findFavouriteServer("Fav.Host:16567",29900)==&F::favourites(owner)[1] && !api->findFavouriteServer("fav.host:16567",29900)
        && !api->findFavouriteServer("Fav.Host:16567",29901) && api->isFavouriteServer("10.0.0.9:16567",29900)
        && !api->isFavouriteServer("10.0.0.9:16567",1),
        "+0x134 / +0x128 match a favourite by exact (case-sensitive) address and query port");
    check(!api->removeFavouriteServerEntry(2) && !api->removeFavouriteServer("10.0.0.11:16567",29900) && F::favourites(owner).size()==2,
        "+0x12C refuses an index past the count and +0x130 an unknown server, both before any lookup");
    F::favourites(owner).reserve(16);
    api->clearFavouriteServers();
    check(F::favourites(owner).empty() && F::favourites(owner).capacity()==0,"+0x158 destroys and frees the favourites' storage");
    const int plays[]={5,1,9,3};
    for(int p:plays){GeneralServerEntry h;h.name="H";h.address="10.0.1.1:16567";h.queryPort=29900+p;h.lastPlayed=p;F::history(owner).push_back(h);}
    reinterpret_cast<void(__thiscall*)(void*)>(table[0x144/4])(object);
    check(F::history(owner)[0].lastPlayed==1 && F::history(owner)[1].lastPlayed==3 && F::history(owner)[2].lastPlayed==5
        && F::history(owner)[3].lastPlayed==9 && F::history(owner)[0].queryPort==29901,
        "+0x144 sorts the history rows ascending by play count, whole rows moving");
    check(reinterpret_cast<IntSlot>(table[0x13C/4])(object)==4 && api->findServerHistory("10.0.1.1:16567",29909)==&F::history(owner)[3]
        && !api->removeServerHistoryEntry(4),
        "+0x13C counts the history, +0x150 finds a row and +0x148 refuses an index past the count");

    api->addRef();
    check(api->release()==1,"non-final release keeps the owner");
    check(api->release()==0,"final release reports zero");
    std::printf("GeneralSettings: %u/%u passed; native owner=%zu entry=%zu\n",checks-failures,checks,sizeof(GeneralSettings),sizeof(GeneralServerEntry));
    return failures?1:0;
}
