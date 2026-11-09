// [INFERRED] path. Client-only class: the dedicated ELF merely looks the "GeneralSettings"
// singleton up and tolerates its absence, so every body below was read from BF2.exe.
#include "GeneralSettings.h"
#include "LocalProfileManager.h"          // cancel: ILocalProfileManager +0x44 / +0x54
#include "../Core/SmartItf.h"
#include "../Menu/ISwiffHost.h"
#include "../Menu/IServerBrowser.h"
#if !DEDICATED
#include "Main/BF2ClientSeams.h"          // hudManager (ds:0xA10890)
#include "../Scene/Object/PlayerManager.h"  // world::playerManager (ds:0xA08F60), +0x30 getLocalHumanPlayer
#include "Objects/IPlayer.h"                // IPlayer::updateClientSettings, the PE +0x334 call
#include "../IO/SettingsManager.h"          // io::settingsManager (ds:0xA03910) and its registered-name set
#include "../Core/SettingsRepostitory.h"    // settings->boolGet / boolSet
#include "../Core/EventManager.h"           // the ECCore change event
#include "../Core/StringUtil.h"             // compareIgnoreCase (PE 431950) - addServerHistory's row match
#include "../Core/Ids.h"
#include "../Menu/BF2HudManager.h"
#include "../Menu/HudInformationAdapter.h"
#include "../Menu/HudMessages.h"
#include <algorithm>                      // sortServerHistory
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <new>
#include <sstream>

namespace dice { namespace hfe {
// Demo launches force auto-ready; ordinary launches use the saved setting.
bool GeneralSettings::getAutoReady() const
{
    if (!settings->stringGet(std::string("GSDemo"), std::string("")).empty())
        return true;
    return m_autoReady;
}

// Duplicate address/port pairs have no side effects. Query the browser before insertion, then remove matching history.
void GeneralSettings::addFavouriteServer(const std::string& address, int queryPort,
                                         const std::string& name)
{
    if (isFavouriteServer(address, queryPort)) return;                  // +0x128
    GeneralServerEntry entry;
    entry.name = name;
    entry.address = address;
    entry.queryPort = queryPort;
    // Retail leaves entry.lastPlayed untouched; it is written only for history rows.
    ui::ISwiffHost* host;
    {
        const std::string singletonName("SwiffHost");
        host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(singletonName));
    }
    if (host) {
        const std::string listName("favourites");
        host->getServerList(listName)->queryAddress(address, queryPort, false);
    }
    m_favouriteServers.push_back(entry); // PE4B7A20, native vector operation
    if (findServerHistory(address, queryPort))                          // +0x150
        removeServerHistory(address, queryPort);                        // +0x14C
}

// PE4B7760 (virtual+130): only the first exact address/queryPort match is removed.
// Empty and nonmatching lists return false without looking up SwiffHost.
bool GeneralSettings::removeFavouriteServer(const std::string& address, int queryPort)
{
    for (unsigned int i = 0; i < m_favouriteServers.size(); ++i) {
        const GeneralServerEntry& entry = m_favouriteServers[i];
        if (entry.address != address || entry.queryPort != queryPort) continue;
        m_favouriteServers.erase(m_favouriteServers.begin() + i); // PE4B6FA0
        ui::ISwiffHost* host;
        {
            const std::string singletonName("SwiffHost");
            host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(singletonName));
        }
        if (host) {
            const std::string listName("favourites");
            host->getServerList(listName)->removeAddress(address, queryPort);
        }
        return true;
    }
    return false;
}

// Adds a non-favorite server to history. lastPlayed == -1 increments a play count
// using a case-insensitive address match; other values restore a saved count.
// Queries the browser and trims the oldest row when the list reaches fifty entries.
void GeneralSettings::addServerHistory(const std::string& address, int queryPort,
                                       const std::string& name, int lastPlayed)
{
    if (findFavouriteServer(address, queryPort))                        // +0x134
        return;

    GeneralServerEntry entry;
    entry.name      = name;
    entry.address   = address;
    entry.queryPort = queryPort;

    if (lastPlayed == -1)
    {
        entry.lastPlayed = 1;
        for (unsigned int i = 0; i < m_serverHistory.size(); ++i)
        {
            GeneralServerEntry& row = m_serverHistory[i];
            if (row.queryPort == queryPort && compareIgnoreCase(row.address, address) == 0)
            {
                ++row.lastPlayed;
                return;
            }
        }
        ui::ISwiffHost* host;
        {
            const std::string singletonName("SwiffHost");
            host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(singletonName));
        }
        if (host)
        {
            const std::string listName("history");
            host->getServerList(listName)->queryAddress(address, queryPort, false);   // +0x7C, +0x48
        }
        m_serverHistory.push_back(entry);
    }
    else
    {
        entry.lastPlayed = lastPlayed;
        m_serverHistory.push_back(entry);
        ui::ISwiffHost* host;
        {
            const std::string singletonName("SwiffHost");
            host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(singletonName));
        }
        if (host)
        {
            const std::string listName("history");
            host->getServerList(listName)->queryAddress(address, queryPort, false);
        }
    }

    if (m_serverHistory.size() >= 50)
        removeServerHistoryEntry(0);                                    // +0x148
}

// Adds a played voice-help name if no case-insensitive match exists.
// [CHOSEN, not recovered] Non-virtual placement for a withheld virtual slot.
void GeneralSettings::setPlayedVOHelp(const std::string& name)
{
    for (unsigned int i = 0; i < m_playedVOHelp.size(); ++i)
    {
        if (compareIgnoreCase(m_playedVOHelp[i], name) == 0)
            return;
    }
    m_playedVOHelp.push_back(name);
}

// Removes a valid history row by address and port; returns whether removal occurred.
// Preserves the original aliasing: the address reference points into the row being erased.
bool GeneralSettings::removeServerHistoryEntry(unsigned int index)
{
    if (index >= static_cast<unsigned int>(getServerHistoryCount()))
        return false;
    removeServerHistory(m_serverHistory[index].address, m_serverHistory[index].queryPort);
    return true;
}

//  (virtual +0x12C), the favourites twin of +0x148 - the count
// through +0x120, `jb` (unsigned), then the row's own address (+0x1C, by reference into the vector)
// and port (+0x38) to +0x130, and true. The same aliasing as +0x148.
bool GeneralSettings::removeFavouriteServerEntry(unsigned int index)
{
    if (index >= static_cast<unsigned int>(getNumFavouriteServers()))
        return false;
    removeFavouriteServer(m_favouriteServers[index].address, m_favouriteServers[index].queryPort);
    return true;
}

// PE4B6490 (virtual+150): case-sensitive string equality, first matching row,
// repeated current vector bounds; null when absent. Returned pointer is borrowed.
GeneralServerEntry* GeneralSettings::findServerHistory(const std::string& address, int queryPort)
{
    for (unsigned int i = 0; i < m_serverHistory.size(); ++i)
        if (m_serverHistory[i].address == address && m_serverHistory[i].queryPort == queryPort)
            return &m_serverHistory[i];
    return nullptr;
}

//  (virtual +0x134), the favourites twin of +0x150, byte for
// byte - the size recomputed from first/last on every pass (`sar 6`, unsigned `jnb`), std::operator==
// on the row's address (+0x1C, case-sensitive), then its port (+0x38); the row's address, or null.
GeneralServerEntry* GeneralSettings::findFavouriteServer(const std::string& address, int queryPort)
{
    for (unsigned int i = 0; i < m_favouriteServers.size(); ++i)
        if (m_favouriteServers[i].address == address && m_favouriteServers[i].queryPort == queryPort)
            return &m_favouriteServers[i];
    return nullptr;
}

// PE4B7850 (virtual+14C): the history analogue of removeFavouriteServer, including
// removal before the optional browser call and the missing returned-list null guard.
bool GeneralSettings::removeServerHistory(const std::string& address, int queryPort)
{
    for (unsigned int i = 0; i < m_serverHistory.size(); ++i) {
        const GeneralServerEntry& entry = m_serverHistory[i];
        if (entry.address != address || entry.queryPort != queryPort) continue;
        m_serverHistory.erase(m_serverHistory.begin() + i);
        ui::ISwiffHost* host;
        {
            const std::string singletonName("SwiffHost");
            host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(singletonName));
        }
        if (host) {
            const std::string listName("history");
            host->getServerList(listName)->removeAddress(address, queryPort);
        }
        return true;
    }
    return false;
}


// PE0x4B7320 (vtable +0xB4) and PE0x4B1CA0 (vtable +0xF4): two one-instruction setters the coop
// bot count is pushed into. +0xA4 is the persisted maxBots setting the script writes; +0xBC is the
// runtime-only coop bot count (float +0xB8 is its ratio twin) that neither the constructor, the
// defaults nor the script touches - SwiffPlayer's get/setCoopBotCount rows name it.
void GeneralSettings::setMaxBots(int value) { m_maxBots = value; }
void GeneralSettings::setCoopBotCount(int value) { m_coopBotCount = value; }

// PE4B51C0 (virtual +0x128): the count through +0x120 before the loop and after every failed match
// (unsigned `jbe` / `jb`), the row through +0x124, std::operator== on its address, then its port.
bool GeneralSettings::isFavouriteServer(const std::string& address, int queryPort)
{
    unsigned int index = 0;
    if (static_cast<unsigned int>(getNumFavouriteServers()) == 0) return false;     // +0x120
    do {
        const GeneralServerEntry* entry = getFavouriteServer(index);                  // +0x124
        if (entry->address == address && entry->queryPort == queryPort)
            return true;
    } while (++index < static_cast<unsigned int>(getNumFavouriteServers()));
    return false;
}

// Checks whether a voice-over help entry has been played, comparing names case-insensitively.
bool GeneralSettings::checkPlayedVOHelp(const std::string& name) const
{
    for (unsigned int i = 0; i < m_playedVOHelp.size(); ++i)
    {
        if (compareIgnoreCase(m_playedVOHelp[i], name) == 0)
            return true;
    }
    return false;
}

namespace {
// The empty predicate 4B7E60 pushes: its operator() is inlined into VC7.1's _Sort helpers
// (4B6BA0's insertion pass, 4B6610's partition) as `left.lastPlayed < right.lastPlayed` (+0x3C).
// [INFERRED] name.
struct ServerHistoryPlayedLess
{
    bool operator()(const GeneralServerEntry& left, const GeneralServerEntry& right) const
    { return left.lastPlayed < right.lastPlayed; }
};
}

// Sorts server history by ascending play count.
// [PLATFORM] Equal-count ordering may differ between standard-library implementations because sort is unstable.
void GeneralSettings::sortServerHistory()
{
    std::sort(m_serverHistory.begin(), m_serverHistory.end(), ServerHistoryPlayedLess());
}

//  (virtual +0x154), the history's storage destroyed and freed
// (the VC7.1 _Tidy sequence, spelled with the swap idiom as setDefaults does), then - only when the
// "SwiffHost" singleton exists - its +0x7C "history" list's +0x24 reset(). The list pointer is used
// without a null test, as shipped (removeServerHistory's browser call has the same gap).
void GeneralSettings::clearServerHistory()
{
    std::vector<GeneralServerEntry>().swap(m_serverHistory);
    ui::ISwiffHost* host;
    {
        const std::string singletonName("SwiffHost");
        host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(singletonName));
    }
    if (host)
    {
        const std::string listName("history");
        host->getServerList(listName)->reset();                         // +0x7C, +0x24
    }
}

//  (virtual +0x158), the favourites' storage destroyed and freed,
// and nothing else - unlike +0x154 no browser list is touched.
void GeneralSettings::clearFavouriteServers()
{
    std::vector<GeneralServerEntry>().swap(m_favouriteServers);
}

namespace {
// Applies a registered Boolean setting and posts a change event only when its value
// differs. Returns false for an unregistered name. [INFERRED] Helper name and placement.
bool applyRegisteredBool(io::SettingsManagerBase* manager, const std::string& name, const bool& value)
{
    if (manager->m_registeredVars.find(name) == manager->m_registeredVars.end())
        return false;
    const bool fallback = false;
    const bool changed = settings->boolGet(name, fallback) != value;
    settings->boolSet(name, value);
    if (changed && eventManager)
        eventManager->postEvent(ECCore, 1, const_cast<std::string*>(&name), 0.0f);
    return true;
}

// : the 1/255 every HUD alpha and colour push multiplies by.
const float kByteToUnit = 0.0039215689f;
}

// (129 B): refcount 1, the three strings and three lists start empty, the LCD flag and
// the toggle-filter mask are seeded (both again below), then 4B7460 is called directly - not
// through the table - and itself dispatches +20 virtually.
GeneralSettings::GeneralSettings() : m_refCount(1)
{
    m_lcdDisplayModes = false;
    // [CHOSEN, not recovered] Retail seeds 63616 (PunkBuster, all map sizes, same version and
    // same mod); new profiles start with no server browser filters. See docs/server-browser-filters.md.
    m_toggleFilters = 0;
    setDefaults();
}

//: history, favourites, played-help list, then the bftv, filter and sort-key strings -
// the reverse of the member order, which the compiler reproduces.
GeneralSettings::~GeneralSettings() = default;

//: the same shape as the other settings factories. Aggregation is refused, 252 retail
// bytes are allocated, the object is constructed only when the allocation succeeded, and the
// result is queried unconditionally (the shipped allocation-null defect is kept).
IBase* GeneralSettings::create(unsigned int iid, IBase* outer)
{
    if (outer) return nullptr;
    void* allocation = ::operator new(sizeof(GeneralSettings));
    GeneralSettings* owner = allocation ? new (allocation) GeneralSettings : nullptr;
    IBase* result = static_cast<IBase*>(owner->queryInterface(iid));
    if (!result) owner->release();
    return result;
}
void GeneralSettings::addRef() { ++m_refCount; }
int GeneralSettings::getRef() { return m_refCount; }
//: decrement, destroy through the (withheld) deleting destructor at zero.
int GeneralSettings::release()
{
    const int references = --m_refCount;
    if (!references) delete this;
    return references;
}
//: IBase, then IGeneralSettings (dword_892CA0 = 121049).
void* GeneralSettings::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IGeneralSettings)
        return const_cast<GeneralSettings*>(this);
    return nullptr;
}

std::string GeneralSettings::getFileName() { return std::string("General.con"); }

// (2,038 B): thirty-one console rows in this order. List rows insert std::strings;
// the three scalar strings insert c_str() (no escaping, embedded-NUL truncation); bools print
// as 0/1; floats through the default ostream precision.
std::string GeneralSettings::getSettingsScript()
{
    std::stringstream stream;
    for (unsigned int i = 0; i < m_playedVOHelp.size(); ++i)
        stream << "GeneralSettings.setPlayedVOHelp \"" << m_playedVOHelp[i] << "\"" << std::endl;
    stream << "GeneralSettings.setSortOrder " << m_sortOrder << std::endl;
    stream << "GeneralSettings.setSortKey \"" << m_sortKey.c_str() << "\"" << std::endl;
    stream << "GeneralSettings.setNumRoundsPlayed " << m_numRoundsPlayed << std::endl;
    stream << "GeneralSettings.setServerFilter \"" << m_serverFilter.c_str() << "\"" << std::endl;
    for (unsigned int i = 0; i < m_favouriteServers.size(); ++i) {
        const GeneralServerEntry& entry = m_favouriteServers[i];
        stream << "GeneralSettings.addFavouriteServer \"" << entry.address << "\" " << entry.queryPort
               << " \"" << entry.name << "\"" << std::endl;
    }
    for (unsigned int i = 0; i < m_serverHistory.size(); ++i) {
        const GeneralServerEntry& entry = m_serverHistory[i];
        stream << "GeneralSettings.addServerHistory \"" << entry.address << "\" " << entry.queryPort
               << " \"" << entry.name << "\" " << entry.lastPlayed << std::endl;
    }
    stream << "GeneralSettings.setHUDTransparency " << m_hudTransparency << std::endl;
    stream << "GeneralSettings.setCrosshairColor " << m_crosshairColor[0] << " " << m_crosshairColor[1]
           << " " << m_crosshairColor[2] << " " << m_crosshairColor[3] << std::endl;
    stream << "GeneralSettings.setBuddytagColor " << m_buddytagColor[0] << " " << m_buddytagColor[1]
           << " " << m_buddytagColor[2] << std::endl;
    stream << "GeneralSettings.setSquadtagColor " << m_squadtagColor[0] << " " << m_squadtagColor[1]
           << " " << m_squadtagColor[2] << std::endl;
    stream << "GeneralSettings.setMinimapRotate " << m_minimapRotate << std::endl;
    stream << "GeneralSettings.setMinimapTransparency " << m_minimapTransparency << std::endl;
    stream << "GeneralSettings.setViewIntroMovie " << m_viewIntroMovie << std::endl;
    stream << "GeneralSettings.setOutOfVoting " << m_outOfVoting << std::endl;
    stream << "GeneralSettings.setBFTVSaveDirectory \"" << m_bftvSaveDirectory.c_str() << "\"" << std::endl;
    stream << "GeneralSettings.setConfirmQuit " << m_confirmQuit << std::endl;
    stream << "GeneralSettings.setMapIconAlphaTransparency " << m_mapIconAlphaTransparency << std::endl;
    stream << "GeneralSettings.setUseAdvancedServerBrowser " << m_useAdvancedServerBrowser << std::endl;
    stream << "GeneralSettings.setUseBots " << m_useBots << std::endl;
    stream << "GeneralSettings.setMaxBots " << m_maxBots << std::endl;
    stream << "GeneralSettings.setMaxBotsIncludeHumans " << m_maxBotsIncludeHumans << std::endl;
    stream << "GeneralSettings.setBotSkill " << m_botSkill << std::endl;
    stream << "GeneralSettings.setLastAwardsCheckDate " << m_lastAwardsCheckDate << std::endl;
    stream << "GeneralSettings.setAllowPunkBuster " << m_allowPunkBuster << std::endl;
    stream << "GeneralSettings.setItemSelectionReverseItems " << m_itemSelectionReverseItems << std::endl;
    stream << "GeneralSettings.setToggleFilters " << m_toggleFilters << std::endl;
    stream << "GeneralSettings.setAutoReload " << m_autoReload << std::endl;
    stream << "GeneralSettings.setAutoReady " << m_autoReady << std::endl;
    stream << "GeneralSettings.setConnectionType " << m_connectionType << std::endl;
    stream << "GeneralSettings.setLCDDisplayModes " << m_lcdDisplayModes << std::endl;
    return stream.str();
}

// (534 B): nothing happens without the HUD manager. Otherwise the information adapter
// (+244, re-fetched for every call) receives the static-map flag and seven 1/255-scaled floats,
// the messages owner's +08 byte takes the item-selection flag, and the two unrecovered owners
// follow. The colour components are ints converted before the multiply (fild).
bool GeneralSettings::apply()
{
    if (!hudManager) return false;
    hudManager->getInformationAdapter()->setMapStatic(!m_minimapRotate);
    hudManager->getInformationAdapter()->setFloat(std::string("MenuBackgroundAlpha"), m_hudTransparency * kByteToUnit);
    hudManager->getInformationAdapter()->setFloat(std::string("MenuMapAlpha"), m_minimapTransparency * kByteToUnit);
    hudManager->getInformationAdapter()->setFloat(std::string("MenuMapIconAlpha"), m_mapIconAlphaTransparency * kByteToUnit);
    hudManager->getMessages()->m_enabled = m_itemSelectionReverseItems;
    hudManager->getInformationAdapter()->setFloat(std::string("CrossHairColorRed"), static_cast<float>(m_crosshairColor[0]) * kByteToUnit);
    hudManager->getInformationAdapter()->setFloat(std::string("CrossHairColorGreen"), static_cast<float>(m_crosshairColor[1]) * kByteToUnit);
    hudManager->getInformationAdapter()->setFloat(std::string("CrossHairColorBlue"), static_cast<float>(m_crosshairColor[2]) * kByteToUnit);
    hudManager->getInformationAdapter()->setFloat(std::string("CrossHairColorAlpha"), static_cast<float>(m_crosshairColor[3]) * kByteToUnit);
    // Fetch the local player again after the null check; the second result is used
    // without another guard. Typed dispatch preserves the correct interface adjustment.
    if (world::playerManager && world::playerManager->getLocalHumanPlayer())
        world::playerManager->getLocalHumanPlayer()->updateClientSettings();
    // : the PunkBuster preference reaches the repository under its registered name.
    // ds:A03910 is io::settingsManager, whose +0x90 registered-name set the helper probes; it is
    // dereferenced with no null test, as shipped.
    applyRegisteredBool(io::settingsManager, std::string("GSClPunkBuster"), m_allowPunkBuster);
    return true;
}

// (219 B): the three lists are destroyed, their storage freed and their words zeroed -
// the VC7.1 _Tidy sequence, whose source spelling is [UNRESOLVED]; the swap idiom releases the
// storage the same way. Then the browser, bot, awards-date, ready, connection and LCD values,
// and finally the virtual +20.
void GeneralSettings::setDefaults()
{
    std::vector<std::string>().swap(m_playedVOHelp);
    std::vector<GeneralServerEntry>().swap(m_favouriteServers);
    std::vector<GeneralServerEntry>().swap(m_serverHistory);
    m_numRoundsPlayed = 0;
    m_useAdvancedServerBrowser = false;
    m_useBots = false;
    m_maxBots = 64;
    m_maxBotsIncludeHumans = false;
    m_botSkill = 0.5f;
    __time32_t now;
    _time32(&now);                                // : the 32-bit CRT time
    m_lastAwardsCheckDate = static_cast<int>(now);
    m_autoReady = false;
    m_connectionType = 5;
    m_lcdDisplayModes = false;
    setDefaultValues();
}

// (198 B): the option values, in the shipped store order.
void GeneralSettings::setDefaultValues()
{
    m_sortKey = "";
    m_sortOrder = 0;
    m_serverFilter = "";
    m_crosshairColor[0] = 255;
    m_crosshairColor[1] = 255;
    m_crosshairColor[2] = 0;
    m_crosshairColor[3] = 204;
    m_buddytagColor[0] = 0;
    m_buddytagColor[1] = 0;
    m_buddytagColor[2] = 0;
    m_squadtagColor[0] = 0;
    m_squadtagColor[1] = 255;
    m_squadtagColor[2] = 0;
    m_hudTransparency = 204.0f;
    m_minimapRotate = false;
    m_minimapTransparency = 204.0f;
    m_viewIntroMovie = true;
    m_outOfVoting = false;
    m_bftvSaveDirectory = "";
    m_confirmQuit = false;
    m_lcdDisplayModes = false;
    m_mapIconAlphaTransparency = 255.0f;
    m_allowPunkBuster = true;
    m_itemSelectionReverseItems = true;
    m_autoReload = true;
    // [CHOSEN, not recovered] Retail seeds 63616; new profiles start with no server browser
    // filters. See docs/server-browser-filters.md.
    m_toggleFilters = 0;
}

// Reloads General.con to discard unsaved options while retaining server-browser and
// bot choices. Does nothing without a current profile. Lists are cleared before
// loading, so a missing file leaves them empty. [INFERRED] Method name.
void GeneralSettings::cancel()
{
    ILocalProfileManager* lpm;
    {
        const std::string singletonName("LocalProfileManager");
        lpm = static_cast<ILocalProfileManager*>(classManager->getSingleton(singletonName));
    }
    if (!lpm || !lpm->getCurrentProfile())                              // +0x44
        return;

    const std::string sortKey(m_sortKey);
    const int sortOrder = m_sortOrder;
    const std::string serverFilter(m_serverFilter);
    const bool useAdvancedServerBrowser = m_useAdvancedServerBrowser;
    const int maxBots = m_maxBots;
    const bool useBots = m_useBots;
    const bool allowPunkBuster = m_allowPunkBuster;
    const bool maxBotsIncludeHumans = m_maxBotsIncludeHumans;
    const float botSkill = m_botSkill;

    std::vector<std::string>().swap(m_playedVOHelp);
    std::vector<GeneralServerEntry>().swap(m_favouriteServers);
    std::vector<GeneralServerEntry>().swap(m_serverHistory);

    std::string path = std::string(lpm->getCurrentProfile()->name) + "/";   // +0x44 again
    path += getFileName();                                              // +0x10
    (void) lpm->loadProfileFile(path);                                  // +0x54

    m_allowPunkBuster = allowPunkBuster;
    m_sortKey = sortKey;
    m_sortOrder = sortOrder;
    m_serverFilter = serverFilter;
    m_useAdvancedServerBrowser = useAdvancedServerBrowser;
    m_useBots = useBots;
    m_maxBots = maxBots;
    m_maxBotsIncludeHumans = maxBotsIncludeHumans;
    m_botSkill = botSkill;
}

} }
#endif
