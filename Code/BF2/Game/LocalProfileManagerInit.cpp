// Profile startup and defaults. Two abort-only placeholders preserve later virtual positions.
// Unsupported suffix operations stop at known callers.
#include "LocalProfileManager.h"
#if !DEDICATED
#include "AudioSettings.h"
#include "GameServerSettings.h"
#include "GeneralSettings.h"
#include "VideoSettings.h"
#include "ControlSettings.h"
#include "HapticSettings.h"
#include "Main/BF2ClientSeams.h"
#include <new>

#include <cstdio>

namespace dice { namespace hfe {

namespace {
// [CHOSEN, not recovered] Set when createDefaultProfile wrote the Default profile in this run.
bool s_defaultProfileCreatedThisRun = false;
}

bool LocalProfileManager::hasDefaultProfile()
{
    // PE4AA339 discards the enumeration result and samples flag20 before the
    // output vector is destroyed. Returning the member preserves that order.
    std::vector<LocalProfile> profiles;
    getLocalProfiles(profiles, true);
    return m_hasDefaultProfile;
}

// Resets all six settings singletons in order, rereading each global and discarding results.
bool LocalProfileManager::createDefaultProfile()
{
    // PE4A80C3..4A80CC: the already-current-profile arm really returns false.
    if (m_currentProfile)
        return false;
    static_cast<IAudioSettings*>(g_audioSettings)->apply(false);                  //  +18, bool discarded
    static_cast<IGeneralSettings*>(g_generalSettings)->setDefaults();             //  +1C
    static_cast<IGameServerSettings*>(g_gameServerSettings)->reset();             //  +1C
    static_cast<IGameServerSettings*>(g_gameServerSettings)->setVoipQuality(5);   //  +DC
    static_cast<IAudioSettings*>(g_audioSettings)->setDefaults(); //  +24
    static_cast<IVideoSettings*>(g_videoSettings)->setDefaults();                 //  +20
    static_cast<IControlSettings*>(g_controlSettings)->backupControlMaps();       //  +1C
    static_cast<IHapticSettings*>(g_hapticSettings)->setDefaults();               //  +1C
    void* allocation = ::operator new(sizeof(LocalProfile));
    m_currentProfile = allocation ? new (allocation) LocalProfile : nullptr;
    // The original has no allocation-failure guard before these string writes,
    // which keep the shipped order: name, profile name and nick become "Default",
    // then the gamespy nick, password and email are emptied.
    m_currentProfile->name = "Default";
    m_currentProfile->profileName = "Default";
    m_currentProfile->nick = "Default";
    m_currentProfile->gamespyNick = "";
    m_currentProfile->password = "";
    m_currentProfile->email = "";
    if (hasDefaultVoiceSettings()) // +94, PE4A81B4
        loadVoiceSettings();       // +98, PE4A81C2; result discarded
    saveCurrentProfile();          // +40, PE4A81CC; result discarded
    if (m_currentProfile) {
        delete m_currentProfile;   // deleting destructor4A8060
        m_currentProfile = nullptr;
    }
    s_defaultProfileCreatedThisRun = true; // [CHOSEN, not recovered] see applyFirstStartVideoDefaults
    return true;
}

bool LocalProfileManager::init()
{
    // Keep the two distinct virtual calls and short-circuit order, PE4A8983..98.
    if (!hasDefaultProfile() && !createDefaultProfile())
        return false;
    void* allocation = ::operator new(sizeof(LocalProfile));
    m_currentProfile = allocation ? new (allocation) LocalProfile : nullptr;
    // The original has no allocation-failure guard before this string write.
    m_currentProfile->name = "Default";
    loadCurrentProfile(); // PE4A89CA discards the bool result.
    applySettings(true); // PE4A89D3 also discards this result.
    delete m_currentProfile;
    m_currentProfile = nullptr;
    return true;
}

// [CHOSEN, not recovered] Owner decision 2026-11-14. createDefaultProfile (4A80C0) runs from init
// (4A8980) before the renderer exists, so VideoSettings::setDefaults keeps the constructor record
// (Medium, 800x600) and a fresh install shows its login screen that way. BF2Engine::initEngine
// calls this once BF2Render::init succeeded (PE 0x409399): only when the Default profile was created
// in this run, setDefaults runs again with the renderer (High, desktop-fitting resolution, highest
// antialiasing), the session applies it (a display-mode change before the first menu frame) and
// Default/Video.con is rewritten. Existing Default profiles are never touched.
void LocalProfileManager::applyFirstStartVideoDefaults()
{
    if (!s_defaultProfileCreatedThisRun)
        return;
    s_defaultProfileCreatedThisRun = false;
    if (m_currentProfile || !g_videoSettings)
        return;
    IVideoSettings* video = static_cast<IVideoSettings*>(g_videoSettings);
    video->setDefaults();
    video->apply();
    std::string path("Default/");
    path += video->getFileName();
    writeProfileFile(path, video->getSettingsScript()); // the applied (current) record
    std::fprintf(stderr, "[CHOSEN, not recovered] LocalProfileManager::applyFirstStartVideoDefaults: "
                         "first-start Default profile at scheme %d, resolution %s, antialiasing %s\n",
                 video->getVideoOptionScheme(), video->getResolution().c_str(),
                 video->getAntialiasing().c_str());
}

} }
#endif
