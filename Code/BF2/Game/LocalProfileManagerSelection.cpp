// [INFERRED] path/names. Full client PE4AB110/4AB1D0/4A8A30/4A82F0/4A84D0/68AD50 read.
#include "LocalProfileManager.h"
#if !DEDICATED
#include "AudioSettings.h"
#include "ControlSettings.h"
#include "GameServerSettings.h"
#include "GeneralSettings.h"
#include "GlobalSettings.h"
#include "HapticSettings.h"
#include "MapList.h"
#include "PasswordCodec.h"
#include "VideoSettings.h"
#include "GameServer/ServerSettings.h"
#include "Main/BF2ClientSeams.h"        // the settings singletons
#include "../Menu/IServerBrowser.h"     // the typed lists the reset reaches
#include "../Menu/ISwiffHost.h"        // the typed front-end host: getServerList
#include "../Core/ClassManager.h"
#include "../Core/SettingsRepostitory.h"
#include "../IO/File/FileManager.h"
#include "../IO/Input/Input.h"
#include <cstring>
#include <sstream>
#include <vector>

namespace dice { namespace hfe {
extern ClassManager* classManager;
namespace io { extern IBase* controlMapManager; } //  (Core/Globals.cpp), as ControlSettings.cpp declares it
namespace {
//  / 4AB40D: with a SwiffHost and a non-empty e-mail, the host's "favourites" and "history"
// ServerBrowser lists are fetched by name (+0x7C) and reset (+0x24, Menu/IServerBrowser.h) - the
// list pointers unguarded, as shipped.
void resetSwiffServerLists(ui::ISwiffHost* host)
{
    host->getServerList(std::string("favourites"))->reset();
    host->getServerList(std::string("history"))->reset();
}
}

// PE+30 4A7690.
bool LocalProfileManager::hasCurrentProfile(int)
{
    return m_currentProfile != 0;
}

// PE+34 4553F0 (folded with every other `return true` of this shape).
bool LocalProfileManager::slot34(int, int)
{
    return true;
}

bool LocalProfileManager::hasProfile(const std::string& profileName)
{
    std::vector<LocalProfile> profiles;
    getLocalProfiles(profiles, true); // PE4AB12C discards the bool return.
    for (unsigned int i = 0; i < profiles.size(); ++i) {
        // PE431950 uses _stricmp on c_str pointers, not string-length comparison.
        if (_stricmp(profiles[i].profileName.c_str(), profileName.c_str()) == 0)
            return true;
    }
    return false;
}

LocalProfile* LocalProfileManager::getCurrentProfile()
{
    return m_currentProfile; // PE68AD50; borrowed native pointer, no retain/copy.
}

// Sets mapList.con under the current profile directory; returns false without a profile. [INFERRED name].
bool LocalProfileManager::setMapListPath()
{
    if (!m_currentProfile)
        return false;
    std::string directory(getProfilesPath() + m_currentProfile->name);
    std::string path(directory + "/mapList.con");
    mapList->m_configFile = path;
    return true;
}

// Loads global settings followed by the selected profile's settings and controls.
// A missing Profile.con destroys the selected record and fails. Named profiles
// also refresh bookmarks, map-list state, player identity, and login count.
bool LocalProfileManager::loadCurrentProfile()
{
    if (!m_currentProfile)
        return false;
    bool isDefault;
    {
        std::string defaultName("Default");
        isDefault = _stricmp(m_currentProfile->name.c_str(), defaultName.c_str()) == 0;
    } // PE4AB20D destroys the comparison temporary before reading field54.
    if (m_field54)
        saveGlobalSettings();                                                        // +C4 4AB226
    IGlobalSettings* global = static_cast<IGlobalSettings*>(g_globalSettings);
    IAudioSettings* audio = static_cast<IAudioSettings*>(g_audioSettings);
    IVideoSettings* video = static_cast<IVideoSettings*>(g_videoSettings);
    std::string globalFile = global->getFileName();
    loadProfileFile(globalFile);                                                     // +54; result discarded
    if (isDefault) {
        audio->setDefaults();                                                        // +24
        video->setDefaults();                                                        // +20
        std::string path(m_currentProfile->name);
        path += "/";
        path += "Profile.con";
        if (!loadProfileFile(path)) {
            if (m_currentProfile) {
                delete m_currentProfile;
                m_currentProfile = nullptr;
            }
            reportProfileError(std::string("unable to open current profile"));
            return false;
        }
        path = m_currentProfile->name + "/";
        path += audio->getFileName();
        loadProfileFile(path);
        path = m_currentProfile->name + "/";
        path += video->getFileName();
        loadProfileFile(path);
    } else {
        IBase* host = classManager->getSingleton(std::string("SwiffHost"));
        if (host && m_currentProfile->email != "")
            resetSwiffServerLists(static_cast<ui::ISwiffHost*>(host));
        //  runs the vector's tidy on the member: every bookmark destroyed, the storage
        // freed, the three pointers zeroed. [INFERRED] source form.
        std::vector<LocalDemoBookmark>().swap(m_demoBookmarks);
        global->setDefaults();                                                       // +1C
        static_cast<IGeneralSettings*>(g_generalSettings)->setDefaults();            // +1C
        static_cast<IGameServerSettings*>(g_gameServerSettings)->reset();            // +1C
        audio->setDefaults();                                                        // +24
        video->setDefaults();                                                        // +20
        static_cast<IHapticSettings*>(g_hapticSettings)->setDefaults();              // +1C
        global->setDefaultUser(std::string(m_currentProfile->name));                 // +24, by value
        saveGlobalSettings();                                                        // +C4
        std::string path(m_currentProfile->name);
        path += "/";
        path += "Profile.con";
        if (!loadProfileFile(path)) {
            if (m_currentProfile) {
                delete m_currentProfile;
                m_currentProfile = nullptr;
            }
            reportProfileError(std::string("unable to open current profile"));
            return false;
        }
        serverSettings->m_configFile = getProfilesPath() + "Default/ServerSettings.con";
        path = m_currentProfile->name + "/";
        path += static_cast<IGeneralSettings*>(g_generalSettings)->getFileName();
        loadProfileFile(path);
        path = m_currentProfile->name + "/";
        path += static_cast<IGameServerSettings*>(g_gameServerSettings)->getFileName();
        loadProfileFile(path);
        path = m_currentProfile->name + "/";
        path += audio->getFileName();
        loadProfileFile(path);
        path = m_currentProfile->name + "/";
        path += video->getFileName();
        loadProfileFile(path);
        path = m_currentProfile->name + "/";
        path += static_cast<IHapticSettings*>(g_hapticSettings)->getFileName();
        loadProfileFile(path);
        // The manager is not null-tested and getActive is asked twice, as shipped.
        std::string activeName("");
        io::IControlMapManager* maps = static_cast<io::IControlMapManager*>(io::controlMapManager);
        if (maps->getActive())                                                       // +34
            activeName = maps->getActive()->getName();                               // +34, +14
        IControlSettings* control = static_cast<IControlSettings*>(g_controlSettings);
        control->resetControlMaps();                                                 // +28
        path = m_currentProfile->name + "/";
        path += control->getFileName();
        loadProfileFile(path);
        control->backupControlMaps();                                                // +1C
        if (activeName != "")
            maps->setActive(maps->getControlMap(activeName));                        // PE +1C, then +2C
        std::string bookmarkFile;
        path = m_currentProfile->name + "/";
        path += "DemoBookmarks.con";
        bookmarkFile = getProfilesPath();
        bookmarkFile += path;
        if (io::fileManager->fileExists(bookmarkFile))                               // +1C
            loadProfileFile(path);
        setMapListPath();
        mapList->clear(false);
        mapList->save();
        if (m_currentProfile->gamespyNick != "")
            settings->stringSet("GSPlayerName", m_currentProfile->gamespyNick);      // +44
        else
            settings->stringSet("GSPlayerName", m_currentProfile->nick);
        ++m_currentProfile->numTimesLoggedIn;
    }
    m_field54 = true;
    return true;
}

// Writes relative to the profile root through the required file manager. Discards write failure and returns true.
bool LocalProfileManager::writeProfileFile(const std::string& path, const std::string& contents)
{
    std::string fileName = getProfilesPath() + path;
    io::fileManager->writeFile(fileName, contents.c_str(), (unsigned int)std::strlen(contents.c_str()),
                               true);
    return true;
}

// Writes profile and global settings. Default saves only Audio and Video; named profiles save all settings,
// bookmarks, and map rotation. Returns true regardless of individual write results.
bool LocalProfileManager::saveCurrentProfile()
{
    if (!m_currentProfile)
        return false;
    bool isDefault;
    {
        std::string defaultName("Default");
        isDefault = _stricmp(m_currentProfile->name.c_str(), defaultName.c_str()) == 0;
    }
    std::stringstream script(std::ios::in | std::ios::out);
    const char* value = m_currentProfile->profileName.c_str();
    script << "LocalProfile.setName \"" << value << "\"" << std::endl;
    value = m_currentProfile->nick.c_str();
    script << "LocalProfile.setNick \"" << value << "\"" << std::endl;
    value = m_currentProfile->gamespyNick.c_str();
    script << "LocalProfile.setGamespyNick \"" << value << "\"" << std::endl;
    if (!m_currentProfile->email.empty()) {
        value = m_currentProfile->email.c_str();
        script << "LocalProfile.setEmail \"" << value << "\"" << std::endl;
    }
    if (!m_currentProfile->password.empty()) {
        PasswordCodec codec;
        if (codec.encode(m_currentProfile->password)) {
            script << "LocalProfile.setPassword \"";
            codec.write(script);
            script << "" << "\"" << std::endl; // the empty literal (PE8A3024) is streamed, as shipped
        }
    }
    const float totalPlayedTime = m_currentProfile->totalPlayedTime;
    script << "LocalProfile.setTotalPlayedTime " << totalPlayedTime << std::endl;
    const int numTimesLoggedIn = m_currentProfile->numTimesLoggedIn;
    script << "LocalProfile.setNumTimesLoggedIn " << numTimesLoggedIn << std::endl;
    // The source copies the appended temporary before adding Profile.con.
    std::string path(std::string(m_currentProfile->name) += "/");
    path += "Profile.con";
    writeProfileFile(path, script.str());

    // Global.con: the profile directory prefix is assigned and then replaced by the file name.
    IGlobalSettings* global = static_cast<IGlobalSettings*>(g_globalSettings);
    path = m_currentProfile->name + "/";
    path = global->getFileName();                                                    // +10
    writeProfileFile(path, global->getSettingsScript());                             // +14

    IAudioSettings* audio = static_cast<IAudioSettings*>(g_audioSettings);
    IVideoSettings* video = static_cast<IVideoSettings*>(g_videoSettings);
    if (isDefault) {
        path = m_currentProfile->name + "/";
        path += audio->getFileName();
        writeProfileFile(path, audio->getSettingsScript());
        path = m_currentProfile->name + "/";
        path += video->getFileName();
        writeProfileFile(path, video->getSettingsScript());
        return true;
    }

    IGeneralSettings* general = static_cast<IGeneralSettings*>(g_generalSettings);
    path = m_currentProfile->name + "/";
    path += general->getFileName();
    writeProfileFile(path, general->getSettingsScript());
    IGameServerSettings* gameServer = static_cast<IGameServerSettings*>(g_gameServerSettings);
    path = m_currentProfile->name + "/";
    path += gameServer->getFileName();
    writeProfileFile(path, gameServer->getSettingsScript());
    path = m_currentProfile->name + "/";
    path += audio->getFileName();
    writeProfileFile(path, audio->getSettingsScript());
    path = "Default/";
    path += audio->getFileName();
    writeProfileFile(path, audio->getSettingsScript());
    path = m_currentProfile->name + "/";
    path += video->getFileName();
    writeProfileFile(path, video->getSettingsScript());
    path = "Default/";
    path += video->getFileName();
    writeProfileFile(path, video->getSettingsScript());
    IHapticSettings* haptic = static_cast<IHapticSettings*>(g_hapticSettings);
    path = m_currentProfile->name + "/";
    path += haptic->getFileName();
    writeProfileFile(path, haptic->getSettingsScript());
    IControlSettings* control = static_cast<IControlSettings*>(g_controlSettings);
    path = m_currentProfile->name + "/";
    path += control->getFileName();
    writeProfileFile(path, control->getSettingsScript());

    saveGlobalSettings();                                                            // +C4 (virtual)
    if (io::controlMapManager)
        static_cast<io::IControlMapManager*>(io::controlMapManager)->applyActive();  // +40
    std::stringstream bookmarks(std::ios::in | std::ios::out);
    for (unsigned int i = 0; i < m_demoBookmarks.size(); ++i) {
        LocalDemoBookmark* bookmark = getDemoBookmark(i);                            // +AC (virtual)
        bookmarks << "LocalProfile.addDemoBookmark \"" << bookmark->field00.c_str()
                  << "\" \"" << bookmark->field1c.c_str() << "\" \"" << bookmark->field38.c_str()
                  << "\" \"" << bookmark->field54.c_str() << "\"" << std::endl;
    }
    std::string bookmarkScript = bookmarks.str();
    if (!bookmarkScript.empty()) {
        std::string bookmarkPath(m_currentProfile->name + "/");
        bookmarkPath += "DemoBookmarks.con";
        writeProfileFile(bookmarkPath, bookmarkScript);
    }
    mapList->save();                                                                 // PE448100
    return true;
}

} }
#endif
