// Client console bindings used to read and write profile settings.
// [CHOSEN] Translation-unit placement; no retail __FILE__ attests this filename.
// Bindings retain their receiver signatures and client-only compilation boundary.

#include <string>

#include "../Core/BuildConfig.h"

#if !DEDICATED

#include "../Core/ConsoleObjectMacro.h"
#include "AudioSettings.h"
#include "GameServerSettings.h"
#include "GeneralSettings.h"
#include "HapticSettings.h"
#include "VideoSettings.h"
#include "Main/BF2ClientSeams.h"        // g_audioSettings ... g_gameServerSettings (ds:0x987354..0x987394)

namespace dice {
namespace hfe {

namespace {

// Registration tag for settings console methods; ModuleGame must register this list.
const char* const kSettingsConsoleTag = "game.settings";

// The five receivers, re-read on every use as each executor re-reads its global. The singleton
// slots hold the object's IBase*, which for these single-inheritance owners IS the interface
// pointer and the final owner; a null slot casts to null (the active tests below rely on it).
AudioSettings* audioSettings()                                          // ds:0x987384
{
    return static_cast<AudioSettings*>(static_cast<IAudioSettings*>(g_audioSettings));
}
IVideoSettings* videoSettings()                                         // ds:0x987388
{
    return static_cast<IVideoSettings*>(g_videoSettings);
}
GeneralSettings* generalSettings()                                      // ds:0x987390
{
    return static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(g_generalSettings));
}
IHapticSettings* hapticSettings()                                       // ds:0x987394
{
    return static_cast<IHapticSettings*>(g_hapticSettings);
}
IGameServerSettings* gameServerSettings()                               // ds:0x987354
{
    return static_cast<IGameServerSettings*>(g_gameServerSettings);
}

// Converts the console float to bool; zero is false and NaN is true.
void gameServerSetNoVehicles(float value)
{
    gameServerSettings()->setNoVehicles(value != 0.0f);                 // +0x1B4
}

} // namespace

// Audio properties edit the pending settings record when its singleton is available.
BF2_CONSOLE_OBJECT("AudioSettings", 2, 0)

// voipEnabled: ctor 412C7F, exec 412D3D (+0x38 / +0x3C), conv 412CF0 (>> bool), ret 7611C7
BF2_CONSOLE_PROPERTY(voipEnabled, bool, audioSettings(), setVoipEnabled, getVoipEnabled, kSettingsConsoleTag)
// voipPlaybackVolume: ctor 412D8E, exec 412E4C (+0x40 / +0x44), conv 412DFF (>> float), ret 51343E
BF2_CONSOLE_PROPERTY(voipPlaybackVolume, float, audioSettings(), setVoipPlaybackVolume, getVoipPlaybackVolume, kSettingsConsoleTag)
// voipCaptureVolume: ctor 412E95, exec 412F53 (+0x48 / +0x4C), conv 412F06, ret 51343E
BF2_CONSOLE_PROPERTY(voipCaptureVolume, float, audioSettings(), setVoipCaptureVolume, getVoipCaptureVolume, kSettingsConsoleTag)
// voipCaptureThreshold: ctor 412F9C, exec 413066 (+0x50 / +0x54), conv 413019, ret 51343E
BF2_CONSOLE_PROPERTY(voipCaptureThreshold, float, audioSettings(), setVoipCaptureThreshold, getVoipCaptureThreshold, kSettingsConsoleTag)
// voipBoostEnabled: ctor 4130AF, exec 41316D (+0x58 / +0x5C), conv 413120, ret 7611C7
BF2_CONSOLE_PROPERTY(voipBoostEnabled, bool, audioSettings(), setVoipBoostEnabled, getVoipBoostEnabled, kSettingsConsoleTag)
// voipUsePushToTalk: ctor 4131BE, exec 41327C (+0x60 / +0x64), conv 41322F, ret 7611C7
BF2_CONSOLE_PROPERTY(voipUsePushToTalk, bool, audioSettings(), setVoipUsePushToTalk, getVoipUsePushToTalk, kSettingsConsoleTag)
// provider: ctor 4132CD (constructs the +0xA4 string first), exec 41334A (+0x68 sret copy into +0xA4 /
// +0x6C by value), conv 4133D1 (dead string branch), ret 7709E2
BF2_CONSOLE_PROPERTY(provider, std::string, audioSettings(), setProvider, getProvider, kSettingsConsoleTag)
// soundQuality: ctor 413446, exec 4134C3 (+0x70 / +0x74), conv 41354A, ret 7709E2
BF2_CONSOLE_PROPERTY(soundQuality, std::string, audioSettings(), setSoundQuality, getSoundQuality, kSettingsConsoleTag)
// effectsVolume: ctor 4135BF, exec 41367D (+0x78 / +0x7C), conv 413630, ret 51343E
BF2_CONSOLE_PROPERTY(effectsVolume, float, audioSettings(), setEffectsVolume, getEffectsVolume, kSettingsConsoleTag)
// musicVolume: ctor 4136C6, exec 413784 (+0x80 / +0x84), conv 413737, ret 51343E
BF2_CONSOLE_PROPERTY(musicVolume, float, audioSettings(), setMusicVolume, getMusicVolume, kSettingsConsoleTag)
// helpVoiceVolume: ctor 4137D3, exec 413891 (+0x88 / +0x8C), conv 413844, ret 51343E
BF2_CONSOLE_PROPERTY(helpVoiceVolume, float, audioSettings(), setHelpVoiceVolume, getHelpVoiceVolume, kSettingsConsoleTag)
// englishOnlyVoices: ctor 4138E0, exec 41399E (+0x90 / +0x94), conv 413951, ret 7611C7
BF2_CONSOLE_PROPERTY(englishOnlyVoices, bool, audioSettings(), setEnglishOnlyVoices, getEnglishOnlyVoices, kSettingsConsoleTag)
// enableEAX: ctor 4139F5, exec 413AB3 (+0x98 / +0x9C), conv 413A66, ret 7611C7
BF2_CONSOLE_PROPERTY(enableEAX, bool, audioSettings(), setEnableEAX, getEnableEAX, kSettingsConsoleTag)
// alwaysAllowSystemOpenAL: ctor 413B0A, exec 413BC8 (+0xA8 / +0xAC), conv 413B7B, ret 7611C7
BF2_CONSOLE_PROPERTY(alwaysAllowSystemOpenAL, bool, audioSettings(), setAlwaysAllowSystemOpenAL, getAlwaysAllowSystemOpenAL, kSettingsConsoleTag)
// forceNumberOfVoices: ctor 413C1F, exec 413CDD (+0xB0 / +0xB4), conv 413C90 (>> int), ret 439A64
BF2_CONSOLE_PROPERTY(forceNumberOfVoices, int, audioSettings(), setForceNumberOfVoices, getForceNumberOfVoices, kSettingsConsoleTag)

// Video setters edit the pending record; string arguments are passed by value.
BF2_CONSOLE_OBJECT("VideoSettings", 2, 0)

// setTerrainQuality: ctor 413D29, exec 413DE7 -> +0x44, conv 413D9A (>> int)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setTerrainQuality, videoSettings()->setTerrainQuality, ::dice::hfe::g_videoSettings != 0, int, kSettingsConsoleTag)
// setGeometryQuality: ctor 413E08, exec 413EC6 -> +0x4C, conv 413E79
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setGeometryQuality, videoSettings()->setGeometryQuality, ::dice::hfe::g_videoSettings != 0, int, kSettingsConsoleTag)
// setLightingQuality: ctor 413EE7, exec 413FA5 -> +0x54, conv 413F58
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setLightingQuality, videoSettings()->setLightingQuality, ::dice::hfe::g_videoSettings != 0, int, kSettingsConsoleTag)
// setDynamicLightingQuality: ctor 413FC6, exec 414084 -> +0x5C, conv 414037
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setDynamicLightingQuality, videoSettings()->setDynamicLightingQuality, ::dice::hfe::g_videoSettings != 0, int, kSettingsConsoleTag)
// setDynamicShadowsQuality: ctor 4140A5, exec 414163 -> +0x64, conv 414116
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setDynamicShadowsQuality, videoSettings()->setDynamicShadowsQuality, ::dice::hfe::g_videoSettings != 0, int, kSettingsConsoleTag)
// setEffectsQuality: ctor 414184, exec 414242 -> +0x6C, conv 4141F5
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setEffectsQuality, videoSettings()->setEffectsQuality, ::dice::hfe::g_videoSettings != 0, int, kSettingsConsoleTag)
// setTextureQuality: ctor 414263, exec 414321 -> +0x74, conv 4142D4
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setTextureQuality, videoSettings()->setTextureQuality, ::dice::hfe::g_videoSettings != 0, int, kSettingsConsoleTag)
// setTextureFilteringQuality: ctor 414342, exec 414400 -> +0x7C, conv 4143B3
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setTextureFilteringQuality, videoSettings()->setTextureFilteringQuality, ::dice::hfe::g_videoSettings != 0, int, kSettingsConsoleTag)
// setResolution: ctor 414421, exec 41449E -> +0x84 (by value), conv 4144F0 (dead string branch)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setResolution, videoSettings()->setResolution, ::dice::hfe::g_videoSettings != 0, std::string, kSettingsConsoleTag)
// setAntialiasing: ctor 414565, exec 4145D6 -> +0x8C (by value), conv 414628
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAntialiasing, videoSettings()->setAntialiasing, ::dice::hfe::g_videoSettings != 0, std::string, kSettingsConsoleTag)
// setViewDistanceScale: ctor 41469D, exec 41475B -> +0x94, conv 41470E (>> float)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setViewDistanceScale, videoSettings()->setViewDistanceScale, ::dice::hfe::g_videoSettings != 0, float, kSettingsConsoleTag)
// setVideoOptionScheme: ctor 414782, exec 414840 -> +0xDC, conv 4147F3
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVideoOptionScheme, videoSettings()->setVideoOptionScheme, ::dice::hfe::g_videoSettings != 0, int, kSettingsConsoleTag)

// General settings console methods, registered in constructor order.
BF2_CONSOLE_OBJECT("GeneralSettings", 2, 0)

// Adds a new favourite, optionally queries it through the movie host, and removes it from history.
// Existing favourites are ignored.
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(addFavouriteServer, generalSettings()->addFavouriteServer, ::dice::hfe::g_generalSettings != 0, std::string, int, std::string, kSettingsConsoleTag)
// Restores a saved history row using its persisted play count, then queries the browser history list.
BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(addServerHistory, generalSettings()->addServerHistory, ::dice::hfe::g_generalSettings != 0, std::string, int, std::string, int, kSettingsConsoleTag)
// setPlayedVOHelp "<name>" - ctor 414CFC, exec 414D6D -> +0x114(const std::string&) (the ONE string row
// here passing a reference to its temporary, not a by-value copy), conv 414DAF.
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPlayedVOHelp, generalSettings()->setPlayedVOHelp, ::dice::hfe::g_generalSettings != 0, std::string, kSettingsConsoleTag)
// setSortKey: ctor 414E24, exec 414E95 -> +0x2C (by value), conv 414EE4
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setSortKey, generalSettings()->setSortKey, ::dice::hfe::g_generalSettings != 0, std::string, kSettingsConsoleTag)
// setSortOrder: ctor 414F59, exec 415017 -> +0x34, conv 414FCA (>> int)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setSortOrder, generalSettings()->setSortOrder, ::dice::hfe::g_generalSettings != 0, int, kSettingsConsoleTag)
// setNumRoundsPlayed: ctor 415038, exec 4150F6 -> +0x3C, conv 4150A9
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setNumRoundsPlayed, generalSettings()->setNumRoundsPlayed, ::dice::hfe::g_generalSettings != 0, int, kSettingsConsoleTag)
// setServerFilter: ctor 415117, exec 415188 -> +0x44 (by value), conv 4151D7
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setServerFilter, generalSettings()->setServerFilter, ::dice::hfe::g_generalSettings != 0, std::string, kSettingsConsoleTag)
// setHUDTransparency: ctor 41524C, exec 41530A -> +0x4C, conv 4152BD (>> float)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setHUDTransparency, generalSettings()->setHUDTransparency, ::dice::hfe::g_generalSettings != 0, float, kSettingsConsoleTag)
// setCrosshairColor r g b a - ctor 41532E (min 4 / max 4, four ints), exec 4154E9 -> +0x54(r, g, b, a),
// conv 4153F2
BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(setCrosshairColor, generalSettings()->setCrosshairColor, ::dice::hfe::g_generalSettings != 0, int, int, int, int, kSettingsConsoleTag)
// setBuddytagColor r g b - ctor 415522 (min 3 / max 3), exec 415689 -> +0x5C(r, g, b), conv 4155CA
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setBuddytagColor, generalSettings()->setBuddytagColor, ::dice::hfe::g_generalSettings != 0, int, int, int, kSettingsConsoleTag)
// setSquadtagColor r g b - ctor 4156BA, exec 415821 -> +0x64(r, g, b), conv 415762
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setSquadtagColor, generalSettings()->setSquadtagColor, ::dice::hfe::g_generalSettings != 0, int, int, int, kSettingsConsoleTag)
// setMinimapRotate: ctor 415852, exec 415910 -> +0x6C, conv 4158C3 (>> bool)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setMinimapRotate, generalSettings()->setMinimapRotate, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setMinimapTransparency: ctor 41593B, exec 415A05 -> +0x74, conv 4159B8
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setMinimapTransparency, generalSettings()->setMinimapTransparency, ::dice::hfe::g_generalSettings != 0, float, kSettingsConsoleTag)
// setViewIntroMovie: ctor 415A29, exec 415AE7 -> +0x7C, conv 415A9A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setViewIntroMovie, generalSettings()->setViewIntroMovie, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setOutOfVoting: ctor 415B12, exec 415BD0 -> +0x84, conv 415B83
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setOutOfVoting, generalSettings()->setOutOfVoting, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setBFTVSaveDirectory: ctor 415BFE, exec 415C6F -> +0x8C (by value), conv 415CC1
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setBFTVSaveDirectory, generalSettings()->setBFTVSaveDirectory, ::dice::hfe::g_generalSettings != 0, std::string, kSettingsConsoleTag)
// setConfirmQuit: ctor 415D36, exec 415DF4 -> +0x94, conv 415DA7
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setConfirmQuit, generalSettings()->setConfirmQuit, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setMapIconAlphaTransparency: ctor 415E22, exec 415EE0 -> +0x9C, conv 415E93
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setMapIconAlphaTransparency, generalSettings()->setMapIconAlphaTransparency, ::dice::hfe::g_generalSettings != 0, float, kSettingsConsoleTag)
// setUseAdvancedServerBrowser: ctor 415F07, exec 415FC5 -> +0xA4, conv 415F78
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setUseAdvancedServerBrowser, generalSettings()->setUseAdvancedServerBrowser, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setUseBots: ctor 415FF3, exec 4160B1 -> +0xAC, conv 416064
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setUseBots, generalSettings()->setUseBots, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setMaxBots: ctor 4160DF, exec 41619D -> +0xB4, conv 416150
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setMaxBots, generalSettings()->setMaxBots, ::dice::hfe::g_generalSettings != 0, int, kSettingsConsoleTag)
// setMaxBotsIncludeHumans: ctor 4161C1, exec 41627F -> +0xBC, conv 416232
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setMaxBotsIncludeHumans, generalSettings()->setMaxBotsIncludeHumans, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setBotSkill: ctor 4162AD, exec 41636B -> +0xC4, conv 41631E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setBotSkill, generalSettings()->setBotSkill, ::dice::hfe::g_generalSettings != 0, float, kSettingsConsoleTag)
// setAllowPunkBuster: ctor 416392, exec 416450 -> +0xD4, conv 416403
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAllowPunkBuster, generalSettings()->setAllowPunkBuster, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setItemSelectionReverseItems: ctor 41647E, exec 41653C -> +0xDC, conv 4164EF
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setItemSelectionReverseItems, generalSettings()->setItemSelectionReverseItems, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setToggleFilters: ctor 41656A, exec 416628 -> +0x108 (the whole mask), conv 4165DB
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setToggleFilters, generalSettings()->setToggleFilters, ::dice::hfe::g_generalSettings != 0, int, kSettingsConsoleTag)
// setAutoReload: ctor 41664C, exec 41670A -> +0xE4, conv 4166BD
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAutoReload, generalSettings()->setAutoReload, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setAutoReady: ctor 416738, exec 4167F6 -> +0xFC, conv 4167A9
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAutoReady, generalSettings()->setAutoReady, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
// setConnectionType: ctor 416824, exec 4168E2 -> +0x104, conv 416895
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setConnectionType, generalSettings()->setConnectionType, ::dice::hfe::g_generalSettings != 0, int, kSettingsConsoleTag)
// setLCDDisplayModes: ctor 416906 (name 0x883E84 stored at 0x416913, return type "void" 0x881214 at
// +0x4C by 0x416943), exec 4169C4 -> +0x160, conv 416977
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setLCDDisplayModes, generalSettings()->setLCDDisplayModes, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)

// [CHOSEN, not recovered] Owner's tutorial option, saved with the general settings.
// Only these new commands are available at end-user access; retail bindings keep access 2.
BF2_CONSOLE_OBJECT("GeneralSettings", 1, 0)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setShowTutorialMessages, generalSettings()->setShowTutorialMessages, ::dice::hfe::g_generalSettings != 0, bool, kSettingsConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getShowTutorialMessages, generalSettings()->getShowTutorialMessages, bool, ::dice::hfe::g_generalSettings != 0, kSettingsConsoleTag)

// Haptic settings use console access level 1. The load binding reloads the current
// profile file and returns its Boolean result.
BF2_CONSOLE_OBJECT("HapticSettings", 1, 0)

// load: ctor 4169F2, exec 416A45 -> +0x28, conv 57DD1F; tree method reloadCurrentProfileFile
BF2_CONSOLE_METHOD_0_RET_ACTIVE(load, hapticSettings()->reloadCurrentProfileFile, bool, ::dice::hfe::g_hapticSettings != 0, kSettingsConsoleTag)
// setForceCapX: ctor 416A6D, exec 416B2B -> +0x30, conv 416ADE
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setForceCapX, hapticSettings()->setForceCapX, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setForceCapY: ctor 416B4F, exec 416C0D -> +0x38, conv 416BC0
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setForceCapY, hapticSettings()->setForceCapY, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setForceCapZ: ctor 416C31, exec 416CEF -> +0x40, conv 416CA2
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setForceCapZ, hapticSettings()->setForceCapZ, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setControlBoxLength: ctor 416D13, exec 416E7A -> +0x48, conv 416DBB
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxLength, hapticSettings()->setControlBoxLength, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxStiffness: ctor 416EB2, exec 417019 -> +0x50, conv 416F5A
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxStiffness, hapticSettings()->setControlBoxStiffness, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxLengthSOLDIER: ctor 417051, exec 4171B8 -> +0x58, conv 4170F9
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxLengthSOLDIER, hapticSettings()->setControlBoxLengthSOLDIER, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxStiffnessSOLDIER: ctor 4171F0, exec 417357 -> +0x60, conv 417298
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxStiffnessSOLDIER, hapticSettings()->setControlBoxStiffnessSOLDIER, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxLengthLAND: ctor 41738F, exec 4174F6 -> +0x88, conv 417437
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxLengthLAND, hapticSettings()->setControlBoxLengthLAND, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxStiffnessLAND: ctor 417531, exec 417698 -> +0x90, conv 4175D9
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxStiffnessLAND, hapticSettings()->setControlBoxStiffnessLAND, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxLengthAIR: ctor 4176D3, exec 41783A -> +0x68, conv 41777B
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxLengthAIR, hapticSettings()->setControlBoxLengthAIR, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxStiffnessAIR: ctor 417872, exec 4179D9 -> +0x70, conv 41791A
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxStiffnessAIR, hapticSettings()->setControlBoxStiffnessAIR, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxLengthHELI: ctor 417A11, exec 417B78 -> +0x78, conv 417AB9
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxLengthHELI, hapticSettings()->setControlBoxLengthHELI, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxStiffnessHELI: ctor 417BB0, exec 417D17 -> +0x80, conv 417C58
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxStiffnessHELI, hapticSettings()->setControlBoxStiffnessHELI, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxLengthSEA: ctor 417D52, exec 417EB9 -> +0x98, conv 417DFA
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxLengthSEA, hapticSettings()->setControlBoxLengthSEA, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setControlBoxStiffnessSEA: ctor 417EF4, exec 41805B -> +0xA0, conv 417F9C
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setControlBoxStiffnessSEA, hapticSettings()->setControlBoxStiffnessSEA, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivity: ctor 418096, exec 4181FD -> +0xA8, conv 41813E
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivity, hapticSettings()->setAimSensitivity, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivityLength: ctor 418238, exec 41839F -> +0xB0, conv 4182E0
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivityLength, hapticSettings()->setAimSensitivityLength, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setTurnSensitivity: ctor 4183DA, exec 41854D -> +0xB8, conv 41848E
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setTurnSensitivity, hapticSettings()->setTurnSensitivity, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivitySOLDIER: ctor 418588, exec 4186EF -> +0xC0, conv 418630
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivitySOLDIER, hapticSettings()->setAimSensitivitySOLDIER, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivityLengthSOLDIER: ctor 41872A, exec 418891 -> +0xC8, conv 4187D2
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivityLengthSOLDIER, hapticSettings()->setAimSensitivityLengthSOLDIER, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setTurnSensitivitySOLDIER: ctor 4188CC, exec 418A33 -> +0xD0, conv 418974
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setTurnSensitivitySOLDIER, hapticSettings()->setTurnSensitivitySOLDIER, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivityLAND: ctor 418A6E, exec 418BD5 -> +0x108, conv 418B16
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivityLAND, hapticSettings()->setAimSensitivityLAND, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivityLengthLAND: ctor 418C10, exec 418D77 -> +0x110, conv 418CB8
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivityLengthLAND, hapticSettings()->setAimSensitivityLengthLAND, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setTurnSensitivityLAND: ctor 418DB2, exec 418F19 -> +0x118, conv 418E5A
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setTurnSensitivityLAND, hapticSettings()->setTurnSensitivityLAND, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivityAIR: ctor 418F54, exec 4190BB -> +0xD8, conv 418FFC
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivityAIR, hapticSettings()->setAimSensitivityAIR, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivityLengthAIR: ctor 4190F6, exec 41925D -> +0xE0, conv 41919E
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivityLengthAIR, hapticSettings()->setAimSensitivityLengthAIR, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setTurnSensitivityAIR: ctor 419298, exec 4193FF -> +0xE8, conv 419340
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setTurnSensitivityAIR, hapticSettings()->setTurnSensitivityAIR, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivityHELI: ctor 41943A, exec 4195A1 -> +0xF0, conv 4194E2
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivityHELI, hapticSettings()->setAimSensitivityHELI, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivityLengthHELI: ctor 4195DC, exec 419743 -> +0xF8, conv 419684
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivityLengthHELI, hapticSettings()->setAimSensitivityLengthHELI, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setTurnSensitivityHELI: ctor 41977E, exec 4198E5 -> +0x100, conv 419826
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setTurnSensitivityHELI, hapticSettings()->setTurnSensitivityHELI, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivitySEA: ctor 419920, exec 419A87 -> +0x120, conv 4199C8
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivitySEA, hapticSettings()->setAimSensitivitySEA, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setAimSensitivityLengthSEA: ctor 419AC2, exec 419C29 -> +0x128, conv 419B6A
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setAimSensitivityLengthSEA, hapticSettings()->setAimSensitivityLengthSEA, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setTurnSensitivitySEA: ctor 419C64, exec 419DCB -> +0x130, conv 419D0C
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setTurnSensitivitySEA, hapticSettings()->setTurnSensitivitySEA, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScale: ctor 419E06, exec 419F6D -> +0x138, conv 419EAE
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setPhysicsSOLDIERScale, hapticSettings()->setPhysicsSOLDIERScale, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScaleXLow: ctor 419FA8, exec 41A066 -> +0x140, conv 41A019
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERScaleXLow, hapticSettings()->setPhysicsSOLDIERScaleXLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScaleYLow: ctor 41A08D, exec 41A14B -> +0x148, conv 41A0FE
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERScaleYLow, hapticSettings()->setPhysicsSOLDIERScaleYLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScaleZLow: ctor 41A172, exec 41A230 -> +0x150, conv 41A1E3
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERScaleZLow, hapticSettings()->setPhysicsSOLDIERScaleZLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERTransitionValueMed: ctor 41A257, exec 41A315 -> +0x190, conv 41A2C8
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERTransitionValueMed, hapticSettings()->setPhysicsSOLDIERTransitionValueMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScaleXMed: ctor 41A33C, exec 41A3FA -> +0x158, conv 41A3AD
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERScaleXMed, hapticSettings()->setPhysicsSOLDIERScaleXMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScaleYMed: ctor 41A421, exec 41A4DF -> +0x160, conv 41A492
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERScaleYMed, hapticSettings()->setPhysicsSOLDIERScaleYMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScaleZMed: ctor 41A506, exec 41A5C4 -> +0x168, conv 41A577
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERScaleZMed, hapticSettings()->setPhysicsSOLDIERScaleZMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERTransitionValueHigh: ctor 41A5EB, exec 41A6A9 -> +0x198, conv 41A65C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERTransitionValueHigh, hapticSettings()->setPhysicsSOLDIERTransitionValueHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScaleXHigh: ctor 41A6D0, exec 41A78E -> +0x170, conv 41A741
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERScaleXHigh, hapticSettings()->setPhysicsSOLDIERScaleXHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScaleYHigh: ctor 41A7B5, exec 41A873 -> +0x178, conv 41A826
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERScaleYHigh, hapticSettings()->setPhysicsSOLDIERScaleYHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERScaleZHigh: ctor 41A89A, exec 41A958 -> +0x180, conv 41A90B
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERScaleZHigh, hapticSettings()->setPhysicsSOLDIERScaleZHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSOLDIERSmoothTimeXLow: ctor 41A97F, exec 41AA3D -> +0x1B8, conv 41A9F0
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERSmoothTimeXLow, hapticSettings()->setPhysicsSOLDIERSmoothTimeXLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSOLDIERSmoothTimeYLow: ctor 41AA61, exec 41AB1F -> +0x1C0, conv 41AAD2
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERSmoothTimeYLow, hapticSettings()->setPhysicsSOLDIERSmoothTimeYLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSOLDIERSmoothTimeZLow: ctor 41AB43, exec 41AC01 -> +0x1C8, conv 41ABB4
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERSmoothTimeZLow, hapticSettings()->setPhysicsSOLDIERSmoothTimeZLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSOLDIERSmoothTimeXMed: ctor 41AC25, exec 41ACE3 -> +0x1D0, conv 41AC96
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERSmoothTimeXMed, hapticSettings()->setPhysicsSOLDIERSmoothTimeXMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSOLDIERSmoothTimeYMed: ctor 41AD07, exec 41ADC5 -> +0x1D8, conv 41AD78
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERSmoothTimeYMed, hapticSettings()->setPhysicsSOLDIERSmoothTimeYMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSOLDIERSmoothTimeZMed: ctor 41ADE9, exec 41AEA7 -> +0x1E0, conv 41AE5A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERSmoothTimeZMed, hapticSettings()->setPhysicsSOLDIERSmoothTimeZMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSOLDIERSmoothTimeXHigh: ctor 41AECB, exec 41AF89 -> +0x1E8, conv 41AF3C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERSmoothTimeXHigh, hapticSettings()->setPhysicsSOLDIERSmoothTimeXHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSOLDIERSmoothTimeYHigh: ctor 41AFAD, exec 41B06B -> +0x1F0, conv 41B01E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERSmoothTimeYHigh, hapticSettings()->setPhysicsSOLDIERSmoothTimeYHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSOLDIERSmoothTimeZHigh: ctor 41B08F, exec 41B14D -> +0x1F8, conv 41B100
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSOLDIERSmoothTimeZHigh, hapticSettings()->setPhysicsSOLDIERSmoothTimeZHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsLANDScale: ctor 41B171, exec 41B2D8 -> +0x200, conv 41B219
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setPhysicsLANDScale, hapticSettings()->setPhysicsLANDScale, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setPhysicsLANDScaleXLow: ctor 41B313, exec 41B3D1 -> +0x208, conv 41B384
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDScaleXLow, hapticSettings()->setPhysicsLANDScaleXLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDScaleYLow: ctor 41B3F8, exec 41B4B6 -> +0x210, conv 41B469
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDScaleYLow, hapticSettings()->setPhysicsLANDScaleYLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDScaleZLow: ctor 41B4DD, exec 41B59B -> +0x218, conv 41B54E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDScaleZLow, hapticSettings()->setPhysicsLANDScaleZLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDTransitionValueMed: ctor 41B5C2, exec 41B680 -> +0x258, conv 41B633
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDTransitionValueMed, hapticSettings()->setPhysicsLANDTransitionValueMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDScaleXMed: ctor 41B6A7, exec 41B765 -> +0x220, conv 41B718
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDScaleXMed, hapticSettings()->setPhysicsLANDScaleXMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDScaleYMed: ctor 41B78C, exec 41B84A -> +0x228, conv 41B7FD
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDScaleYMed, hapticSettings()->setPhysicsLANDScaleYMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDScaleZMed: ctor 41B871, exec 41B92F -> +0x230, conv 41B8E2
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDScaleZMed, hapticSettings()->setPhysicsLANDScaleZMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDTransitionValueHigh: ctor 41B956, exec 41BA14 -> +0x260, conv 41B9C7
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDTransitionValueHigh, hapticSettings()->setPhysicsLANDTransitionValueHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDScaleXHigh: ctor 41BA3B, exec 41BAF9 -> +0x238, conv 41BAAC
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDScaleXHigh, hapticSettings()->setPhysicsLANDScaleXHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDScaleYHigh: ctor 41BB20, exec 41BBDE -> +0x240, conv 41BB91
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDScaleYHigh, hapticSettings()->setPhysicsLANDScaleYHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDScaleZHigh: ctor 41BC05, exec 41BCD6 -> +0x248, conv 41BC89
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDScaleZHigh, hapticSettings()->setPhysicsLANDScaleZHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsLANDSmoothTimeXLow: ctor 41BCFD, exec 41BDBB -> +0x280, conv 41BD6E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDSmoothTimeXLow, hapticSettings()->setPhysicsLANDSmoothTimeXLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsLANDSmoothTimeYLow: ctor 41BDDF, exec 41BE9D -> +0x288, conv 41BE50
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDSmoothTimeYLow, hapticSettings()->setPhysicsLANDSmoothTimeYLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsLANDSmoothTimeZLow: ctor 41BEC1, exec 41BF7F -> +0x290, conv 41BF32
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDSmoothTimeZLow, hapticSettings()->setPhysicsLANDSmoothTimeZLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsLANDSmoothTimeXMed: ctor 41BFA3, exec 41C061 -> +0x298, conv 41C014
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDSmoothTimeXMed, hapticSettings()->setPhysicsLANDSmoothTimeXMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsLANDSmoothTimeYMed: ctor 41C085, exec 41C143 -> +0x2A0, conv 41C0F6
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDSmoothTimeYMed, hapticSettings()->setPhysicsLANDSmoothTimeYMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsLANDSmoothTimeZMed: ctor 41C167, exec 41C225 -> +0x2A8, conv 41C1D8
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDSmoothTimeZMed, hapticSettings()->setPhysicsLANDSmoothTimeZMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsLANDSmoothTimeXHigh: ctor 41C249, exec 41C307 -> +0x2B0, conv 41C2BA
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDSmoothTimeXHigh, hapticSettings()->setPhysicsLANDSmoothTimeXHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsLANDSmoothTimeYHigh: ctor 41C32B, exec 41C3E9 -> +0x2B8, conv 41C39C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDSmoothTimeYHigh, hapticSettings()->setPhysicsLANDSmoothTimeYHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsLANDSmoothTimeZHigh: ctor 41C40D, exec 41C4CB -> +0x2C0, conv 41C47E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsLANDSmoothTimeZHigh, hapticSettings()->setPhysicsLANDSmoothTimeZHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsAIRScale: ctor 41C4EF, exec 41C656 -> +0x2C8, conv 41C597
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setPhysicsAIRScale, hapticSettings()->setPhysicsAIRScale, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setPhysicsAIRScaleXLow: ctor 41C691, exec 41C74F -> +0x2D0, conv 41C702
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRScaleXLow, hapticSettings()->setPhysicsAIRScaleXLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRScaleYLow: ctor 41C776, exec 41C834 -> +0x2D8, conv 41C7E7
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRScaleYLow, hapticSettings()->setPhysicsAIRScaleYLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRScaleZLow: ctor 41C85B, exec 41C919 -> +0x2E0, conv 41C8CC
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRScaleZLow, hapticSettings()->setPhysicsAIRScaleZLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRTransitionValueMed: ctor 41C940, exec 41C9FE -> +0x320, conv 41C9B1
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRTransitionValueMed, hapticSettings()->setPhysicsAIRTransitionValueMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRScaleXMed: ctor 41CA25, exec 41CAE3 -> +0x2E8, conv 41CA96
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRScaleXMed, hapticSettings()->setPhysicsAIRScaleXMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRScaleYMed: ctor 41CB0A, exec 41CBC8 -> +0x2F0, conv 41CB7B
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRScaleYMed, hapticSettings()->setPhysicsAIRScaleYMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRScaleZMed: ctor 41CBEF, exec 41CCAD -> +0x2F8, conv 41CC60
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRScaleZMed, hapticSettings()->setPhysicsAIRScaleZMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRTransitionValueHigh: ctor 41CCD4, exec 41CD92 -> +0x328, conv 41CD45
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRTransitionValueHigh, hapticSettings()->setPhysicsAIRTransitionValueHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRScaleXHigh: ctor 41CDB9, exec 41CE77 -> +0x300, conv 41CE2A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRScaleXHigh, hapticSettings()->setPhysicsAIRScaleXHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRScaleYHigh: ctor 41CE9E, exec 41CF5C -> +0x308, conv 41CF0F
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRScaleYHigh, hapticSettings()->setPhysicsAIRScaleYHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRScaleZHigh: ctor 41CF83, exec 41D041 -> +0x310, conv 41CFF4
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRScaleZHigh, hapticSettings()->setPhysicsAIRScaleZHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsAIRSmoothTimeXLow: ctor 41D068, exec 41D126 -> +0x348, conv 41D0D9
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRSmoothTimeXLow, hapticSettings()->setPhysicsAIRSmoothTimeXLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsAIRSmoothTimeYLow: ctor 41D14A, exec 41D208 -> +0x350, conv 41D1BB
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRSmoothTimeYLow, hapticSettings()->setPhysicsAIRSmoothTimeYLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsAIRSmoothTimeZLow: ctor 41D22C, exec 41D2EA -> +0x358, conv 41D29D
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRSmoothTimeZLow, hapticSettings()->setPhysicsAIRSmoothTimeZLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsAIRSmoothTimeXMed: ctor 41D30E, exec 41D3CC -> +0x360, conv 41D37F
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRSmoothTimeXMed, hapticSettings()->setPhysicsAIRSmoothTimeXMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsAIRSmoothTimeYMed: ctor 41D3F0, exec 41D4AE -> +0x368, conv 41D461
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRSmoothTimeYMed, hapticSettings()->setPhysicsAIRSmoothTimeYMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsAIRSmoothTimeZMed: ctor 41D4D2, exec 41D590 -> +0x370, conv 41D543
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRSmoothTimeZMed, hapticSettings()->setPhysicsAIRSmoothTimeZMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsAIRSmoothTimeXHigh: ctor 41D5B4, exec 41D672 -> +0x378, conv 41D625
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRSmoothTimeXHigh, hapticSettings()->setPhysicsAIRSmoothTimeXHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsAIRSmoothTimeYHigh: ctor 41D696, exec 41D754 -> +0x380, conv 41D707
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRSmoothTimeYHigh, hapticSettings()->setPhysicsAIRSmoothTimeYHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsAIRSmoothTimeZHigh: ctor 41D778, exec 41D836 -> +0x388, conv 41D7E9
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsAIRSmoothTimeZHigh, hapticSettings()->setPhysicsAIRSmoothTimeZHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsHELIScale: ctor 41D85A, exec 41D9C1 -> +0x390, conv 41D902
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setPhysicsHELIScale, hapticSettings()->setPhysicsHELIScale, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setPhysicsHELIScaleXLow: ctor 41D9FC, exec 41DABA -> +0x398, conv 41DA6D
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELIScaleXLow, hapticSettings()->setPhysicsHELIScaleXLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELIScaleYLow: ctor 41DAE1, exec 41DB9F -> +0x3A0, conv 41DB52
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELIScaleYLow, hapticSettings()->setPhysicsHELIScaleYLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELIScaleZLow: ctor 41DBC6, exec 41DC84 -> +0x3A8, conv 41DC37
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELIScaleZLow, hapticSettings()->setPhysicsHELIScaleZLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELITransitionValueMed: ctor 41DCAB, exec 41DD69 -> +0x3E8, conv 41DD1C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELITransitionValueMed, hapticSettings()->setPhysicsHELITransitionValueMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELIScaleXMed: ctor 41DD90, exec 41DE4E -> +0x3B0, conv 41DE01
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELIScaleXMed, hapticSettings()->setPhysicsHELIScaleXMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELIScaleYMed: ctor 41DE75, exec 41DF33 -> +0x3B8, conv 41DEE6
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELIScaleYMed, hapticSettings()->setPhysicsHELIScaleYMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELIScaleZMed: ctor 41DF5A, exec 41E018 -> +0x3C0, conv 41DFCB
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELIScaleZMed, hapticSettings()->setPhysicsHELIScaleZMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELITransitionValueHigh: ctor 41E03F, exec 41E0FD -> +0x3F0, conv 41E0B0
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELITransitionValueHigh, hapticSettings()->setPhysicsHELITransitionValueHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELIScaleXHigh: ctor 41E124, exec 41E1E2 -> +0x3C8, conv 41E195
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELIScaleXHigh, hapticSettings()->setPhysicsHELIScaleXHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELIScaleYHigh: ctor 41E209, exec 41E2C7 -> +0x3D0, conv 41E27A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELIScaleYHigh, hapticSettings()->setPhysicsHELIScaleYHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELIScaleZHigh: ctor 41E2EE, exec 41E3AC -> +0x3D8, conv 41E35F
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELIScaleZHigh, hapticSettings()->setPhysicsHELIScaleZHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsHELISmoothTimeXLow: ctor 41E3D3, exec 41E491 -> +0x410, conv 41E444
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELISmoothTimeXLow, hapticSettings()->setPhysicsHELISmoothTimeXLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsHELISmoothTimeYLow: ctor 41E4B5, exec 41E573 -> +0x418, conv 41E526
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELISmoothTimeYLow, hapticSettings()->setPhysicsHELISmoothTimeYLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsHELISmoothTimeZLow: ctor 41E597, exec 41E655 -> +0x420, conv 41E608
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELISmoothTimeZLow, hapticSettings()->setPhysicsHELISmoothTimeZLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsHELISmoothTimeXMed: ctor 41E679, exec 41E737 -> +0x428, conv 41E6EA
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELISmoothTimeXMed, hapticSettings()->setPhysicsHELISmoothTimeXMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsHELISmoothTimeYMed: ctor 41E75B, exec 41E819 -> +0x430, conv 41E7CC
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELISmoothTimeYMed, hapticSettings()->setPhysicsHELISmoothTimeYMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsHELISmoothTimeZMed: ctor 41E83D, exec 41E8FB -> +0x438, conv 41E8AE
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELISmoothTimeZMed, hapticSettings()->setPhysicsHELISmoothTimeZMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsHELISmoothTimeXHigh: ctor 41E91F, exec 41E9DD -> +0x440, conv 41E990
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELISmoothTimeXHigh, hapticSettings()->setPhysicsHELISmoothTimeXHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsHELISmoothTimeYHigh: ctor 41EA01, exec 41EABF -> +0x448, conv 41EA72
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELISmoothTimeYHigh, hapticSettings()->setPhysicsHELISmoothTimeYHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsHELISmoothTimeZHigh: ctor 41EAE3, exec 41EBA1 -> +0x450, conv 41EB54
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsHELISmoothTimeZHigh, hapticSettings()->setPhysicsHELISmoothTimeZHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSEAScale: ctor 41EBC5, exec 41ED2C -> +0x458, conv 41EC6D
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setPhysicsSEAScale, hapticSettings()->setPhysicsSEAScale, ::dice::hfe::g_hapticSettings != 0, float, float, float, kSettingsConsoleTag)
// setPhysicsSEAScaleXLow: ctor 41ED67, exec 41EE25 -> +0x460, conv 41EDD8
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEAScaleXLow, hapticSettings()->setPhysicsSEAScaleXLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEAScaleYLow: ctor 41EE4C, exec 41EF0A -> +0x468, conv 41EEBD
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEAScaleYLow, hapticSettings()->setPhysicsSEAScaleYLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEAScaleZLow: ctor 41EF31, exec 41EFEF -> +0x470, conv 41EFA2
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEAScaleZLow, hapticSettings()->setPhysicsSEAScaleZLow, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEATransitionValueMed: ctor 41F016, exec 41F0D4 -> +0x4B0, conv 41F087
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEATransitionValueMed, hapticSettings()->setPhysicsSEATransitionValueMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEAScaleXMed: ctor 41F0FB, exec 41F1B9 -> +0x478, conv 41F16C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEAScaleXMed, hapticSettings()->setPhysicsSEAScaleXMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEAScaleYMed: ctor 41F1E0, exec 41F29E -> +0x480, conv 41F251
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEAScaleYMed, hapticSettings()->setPhysicsSEAScaleYMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEAScaleZMed: ctor 41F2C5, exec 41F383 -> +0x488, conv 41F336
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEAScaleZMed, hapticSettings()->setPhysicsSEAScaleZMed, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEATransitionValueHigh: ctor 41F3AA, exec 41F468 -> +0x4B8, conv 41F41B
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEATransitionValueHigh, hapticSettings()->setPhysicsSEATransitionValueHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEAScaleXHigh: ctor 41F48F, exec 41F54D -> +0x490, conv 41F500
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEAScaleXHigh, hapticSettings()->setPhysicsSEAScaleXHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEAScaleYHigh: ctor 41F574, exec 41F632 -> +0x498, conv 41F5E5
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEAScaleYHigh, hapticSettings()->setPhysicsSEAScaleYHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEAScaleZHigh: ctor 41F659, exec 41F717 -> +0x4A0, conv 41F6CA
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEAScaleZHigh, hapticSettings()->setPhysicsSEAScaleZHigh, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setPhysicsSEASmoothTimeXLow: ctor 41F73E, exec 41F7FC -> +0x4D8, conv 41F7AF
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEASmoothTimeXLow, hapticSettings()->setPhysicsSEASmoothTimeXLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSEASmoothTimeYLow: ctor 41F820, exec 41F8DE -> +0x4E0, conv 41F891
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEASmoothTimeYLow, hapticSettings()->setPhysicsSEASmoothTimeYLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSEASmoothTimeZLow: ctor 41F902, exec 41F9C0 -> +0x4E8, conv 41F973
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEASmoothTimeZLow, hapticSettings()->setPhysicsSEASmoothTimeZLow, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSEASmoothTimeXMed: ctor 41F9E4, exec 41FAA2 -> +0x4F0, conv 41FA55
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEASmoothTimeXMed, hapticSettings()->setPhysicsSEASmoothTimeXMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSEASmoothTimeYMed: ctor 41FAC6, exec 41FB84 -> +0x4F8, conv 41FB37
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEASmoothTimeYMed, hapticSettings()->setPhysicsSEASmoothTimeYMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSEASmoothTimeZMed: ctor 41FBA8, exec 41FC66 -> +0x500, conv 41FC19
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEASmoothTimeZMed, hapticSettings()->setPhysicsSEASmoothTimeZMed, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSEASmoothTimeXHigh: ctor 41FC8A, exec 41FD48 -> +0x508, conv 41FCFB
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEASmoothTimeXHigh, hapticSettings()->setPhysicsSEASmoothTimeXHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSEASmoothTimeYHigh: ctor 41FD6C, exec 41FE2A -> +0x510, conv 41FDDD
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEASmoothTimeYHigh, hapticSettings()->setPhysicsSEASmoothTimeYHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setPhysicsSEASmoothTimeZHigh: ctor 41FE4E, exec 41FF0C -> +0x518, conv 41FEBF
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPhysicsSEASmoothTimeZHigh, hapticSettings()->setPhysicsSEASmoothTimeZHigh, ::dice::hfe::g_hapticSettings != 0, int, kSettingsConsoleTag)
// setDamageScale: ctor 41FF30, exec 41FFEE -> +0x520, conv 41FFA1
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setDamageScale, hapticSettings()->setDamageScale, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScalePISTOL: ctor 420015, exec 4200D3 -> +0x5E8, conv 420086
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScalePISTOL, hapticSettings()->setRecoilPunchScalePISTOL, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimePISTOL: ctor 4200FA, exec 4201B8 -> +0x5F0, conv 42016B
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimePISTOL, hapticSettings()->setRecoilPunchTimePISTOL, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScalePISTOL: ctor 4201DF, exec 42029D -> +0x5F8, conv 420250
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScalePISTOL, hapticSettings()->setRecoilYawScalePISTOL, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScalePISTOL: ctor 4202C4, exec 420382 -> +0x600, conv 420335
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScalePISTOL, hapticSettings()->setRecoilPitchScalePISTOL, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleRIFLE: ctor 4203A9, exec 420467 -> +0x548, conv 42041A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleRIFLE, hapticSettings()->setRecoilPunchScaleRIFLE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeRIFLE: ctor 42048E, exec 42054C -> +0x550, conv 4204FF
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeRIFLE, hapticSettings()->setRecoilPunchTimeRIFLE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleRIFLE: ctor 420573, exec 420631 -> +0x558, conv 4205E4
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleRIFLE, hapticSettings()->setRecoilYawScaleRIFLE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleRIFLE: ctor 420658, exec 420716 -> +0x560, conv 4206C9
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleRIFLE, hapticSettings()->setRecoilPitchScaleRIFLE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleCARBINE: ctor 42073D, exec 4207FB -> +0x588, conv 4207AE
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleCARBINE, hapticSettings()->setRecoilPunchScaleCARBINE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeCARBINE: ctor 420822, exec 4208E0 -> +0x590, conv 420893
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeCARBINE, hapticSettings()->setRecoilPunchTimeCARBINE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleCARBINE: ctor 420907, exec 4209C5 -> +0x598, conv 420978
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleCARBINE, hapticSettings()->setRecoilYawScaleCARBINE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleCARBINE: ctor 4209EC, exec 420AAA -> +0x5A0, conv 420A5D
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleCARBINE, hapticSettings()->setRecoilPitchScaleCARBINE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleSHOTGUN: ctor 420AD1, exec 420B8F -> +0x648, conv 420B42
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleSHOTGUN, hapticSettings()->setRecoilPunchScaleSHOTGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeSHOTGUN: ctor 420BB6, exec 420C74 -> +0x650, conv 420C27
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeSHOTGUN, hapticSettings()->setRecoilPunchTimeSHOTGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleSHOTGUN: ctor 420C9B, exec 420D59 -> +0x658, conv 420D0C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleSHOTGUN, hapticSettings()->setRecoilYawScaleSHOTGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleSHOTGUN: ctor 420D80, exec 420E3E -> +0x660, conv 420DF1
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleSHOTGUN, hapticSettings()->setRecoilPitchScaleSHOTGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleSNIPER: ctor 420E65, exec 420F23 -> +0x5C8, conv 420ED6
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleSNIPER, hapticSettings()->setRecoilPunchScaleSNIPER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeSNIPER: ctor 420F4A, exec 421008 -> +0x5D0, conv 420FBB
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeSNIPER, hapticSettings()->setRecoilPunchTimeSNIPER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleSNIPER: ctor 42102F, exec 4210ED -> +0x5D8, conv 4210A0
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleSNIPER, hapticSettings()->setRecoilYawScaleSNIPER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleSNIPER: ctor 421114, exec 4211D2 -> +0x5E0, conv 421185
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleSNIPER, hapticSettings()->setRecoilPitchScaleSNIPER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleRIFLELAUNCHER: ctor 4211F9, exec 4212B7 -> +0x568, conv 42126A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleRIFLELAUNCHER, hapticSettings()->setRecoilPunchScaleRIFLELAUNCHER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeRIFLELAUNCHER: ctor 4212DE, exec 42139C -> +0x570, conv 42134F
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeRIFLELAUNCHER, hapticSettings()->setRecoilPunchTimeRIFLELAUNCHER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleRIFLELAUNCHER: ctor 4213C3, exec 421481 -> +0x578, conv 421434
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleRIFLELAUNCHER, hapticSettings()->setRecoilYawScaleRIFLELAUNCHER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleRIFLELAUNCHER: ctor 4214A8, exec 421566 -> +0x580, conv 421519
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleRIFLELAUNCHER, hapticSettings()->setRecoilPitchScaleRIFLELAUNCHER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleLAUNCHER: ctor 42158D, exec 42164B -> +0x608, conv 4215FE
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleLAUNCHER, hapticSettings()->setRecoilPunchScaleLAUNCHER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeLAUNCHER: ctor 421672, exec 421730 -> +0x610, conv 4216E3
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeLAUNCHER, hapticSettings()->setRecoilPunchTimeLAUNCHER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleLAUNCHER: ctor 421757, exec 421815 -> +0x618, conv 4217C8
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleLAUNCHER, hapticSettings()->setRecoilYawScaleLAUNCHER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleLAUNCHER: ctor 42183C, exec 4218FA -> +0x620, conv 4218AD
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleLAUNCHER, hapticSettings()->setRecoilPitchScaleLAUNCHER, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleLMG: ctor 421921, exec 4219DF -> +0x5A8, conv 421992
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleLMG, hapticSettings()->setRecoilPunchScaleLMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeLMG: ctor 421A06, exec 421AC4 -> +0x5B0, conv 421A77
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeLMG, hapticSettings()->setRecoilPunchTimeLMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleLMG: ctor 421AEB, exec 421BA9 -> +0x5B8, conv 421B5C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleLMG, hapticSettings()->setRecoilYawScaleLMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleLMG: ctor 421BD0, exec 421C8E -> +0x5C0, conv 421C41
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleLMG, hapticSettings()->setRecoilPitchScaleLMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleSMG: ctor 421CB5, exec 421D73 -> +0x628, conv 421D26
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleSMG, hapticSettings()->setRecoilPunchScaleSMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeSMG: ctor 421D9A, exec 421E58 -> +0x630, conv 421E0B
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeSMG, hapticSettings()->setRecoilPunchTimeSMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleSMG: ctor 421E7F, exec 421F3D -> +0x638, conv 421EF0
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleSMG, hapticSettings()->setRecoilYawScaleSMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleSMG: ctor 421F64, exec 422022 -> +0x640, conv 421FD5
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleSMG, hapticSettings()->setRecoilPitchScaleSMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleKNIFE: ctor 422049, exec 422107 -> +0x668, conv 4220BA
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleKNIFE, hapticSettings()->setRecoilPunchScaleKNIFE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeKNIFE: ctor 42212E, exec 4221EC -> +0x670, conv 42219F
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeKNIFE, hapticSettings()->setRecoilPunchTimeKNIFE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleKNIFE: ctor 422213, exec 4222D1 -> +0x678, conv 422284
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleKNIFE, hapticSettings()->setRecoilYawScaleKNIFE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleKNIFE: ctor 4222F8, exec 4223B6 -> +0x680, conv 422369
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleKNIFE, hapticSettings()->setRecoilPitchScaleKNIFE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleDEFIB: ctor 4223DD, exec 42249B -> +0x688, conv 42244E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleDEFIB, hapticSettings()->setRecoilPunchScaleDEFIB, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeDEFIB: ctor 4224C2, exec 422580 -> +0x690, conv 422533
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeDEFIB, hapticSettings()->setRecoilPunchTimeDEFIB, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleDEFIB: ctor 4225A7, exec 422665 -> +0x698, conv 422618
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleDEFIB, hapticSettings()->setRecoilYawScaleDEFIB, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleDEFIB: ctor 42268C, exec 42274A -> +0x6A0, conv 4226FD
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleDEFIB, hapticSettings()->setRecoilPitchScaleDEFIB, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleDROP: ctor 422771, exec 42282F -> +0x6A8, conv 4227E2
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleDROP, hapticSettings()->setRecoilPunchScaleDROP, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeDROP: ctor 422856, exec 422914 -> +0x6B0, conv 4228C7
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeDROP, hapticSettings()->setRecoilPunchTimeDROP, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleDROP: ctor 42293B, exec 4229F9 -> +0x6B8, conv 4229AC
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleDROP, hapticSettings()->setRecoilYawScaleDROP, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleDROP: ctor 422A20, exec 422ADE -> +0x6C0, conv 422A91
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleDROP, hapticSettings()->setRecoilPitchScaleDROP, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleTHROWN: ctor 422B05, exec 422BC3 -> +0x6C8, conv 422B76
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleTHROWN, hapticSettings()->setRecoilPunchScaleTHROWN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeTHROWN: ctor 422BEA, exec 422CA8 -> +0x6D0, conv 422C5B
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeTHROWN, hapticSettings()->setRecoilPunchTimeTHROWN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleTHROWN: ctor 422CCF, exec 422D8D -> +0x6D8, conv 422D40
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleTHROWN, hapticSettings()->setRecoilYawScaleTHROWN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleTHROWN: ctor 422DB4, exec 422E72 -> +0x6E0, conv 422E25
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleTHROWN, hapticSettings()->setRecoilPitchScaleTHROWN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleV_LMG: ctor 422E99, exec 422F57 -> +0x6E8, conv 422F0A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleV_LMG, hapticSettings()->setRecoilPunchScaleV_LMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeV_LMG: ctor 422F7E, exec 42303C -> +0x6F0, conv 422FEF
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeV_LMG, hapticSettings()->setRecoilPunchTimeV_LMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleV_LMG: ctor 423063, exec 423121 -> +0x6F8, conv 4230D4
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleV_LMG, hapticSettings()->setRecoilYawScaleV_LMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleV_LMG: ctor 423148, exec 423206 -> +0x700, conv 4231B9
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleV_LMG, hapticSettings()->setRecoilPitchScaleV_LMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleV_HMG: ctor 42322D, exec 4232EB -> +0x708, conv 42329E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleV_HMG, hapticSettings()->setRecoilPunchScaleV_HMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeV_HMG: ctor 423312, exec 4233D0 -> +0x710, conv 423383
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeV_HMG, hapticSettings()->setRecoilPunchTimeV_HMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleV_HMG: ctor 4233F7, exec 4234B5 -> +0x718, conv 423468
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleV_HMG, hapticSettings()->setRecoilYawScaleV_HMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleV_HMG: ctor 4234DC, exec 42359A -> +0x720, conv 42354D
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleV_HMG, hapticSettings()->setRecoilPitchScaleV_HMG, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleV_HELIGUN: ctor 4235C1, exec 42367F -> +0x728, conv 423632
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleV_HELIGUN, hapticSettings()->setRecoilPunchScaleV_HELIGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeV_HELIGUN: ctor 4236A6, exec 423764 -> +0x730, conv 423717
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeV_HELIGUN, hapticSettings()->setRecoilPunchTimeV_HELIGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleV_HELIGUN: ctor 42378B, exec 423849 -> +0x738, conv 4237FC
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleV_HELIGUN, hapticSettings()->setRecoilYawScaleV_HELIGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleV_HELIGUN: ctor 423870, exec 42392E -> +0x740, conv 4238E1
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleV_HELIGUN, hapticSettings()->setRecoilPitchScaleV_HELIGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleV_AIRGUN: ctor 423955, exec 423A13 -> +0x748, conv 4239C6
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleV_AIRGUN, hapticSettings()->setRecoilPunchScaleV_AIRGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeV_AIRGUN: ctor 423A3A, exec 423AF8 -> +0x750, conv 423AAB
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeV_AIRGUN, hapticSettings()->setRecoilPunchTimeV_AIRGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleV_AIRGUN: ctor 423B1F, exec 423BDD -> +0x758, conv 423B90
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleV_AIRGUN, hapticSettings()->setRecoilYawScaleV_AIRGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleV_AIRGUN: ctor 423C04, exec 423CC2 -> +0x760, conv 423C75
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleV_AIRGUN, hapticSettings()->setRecoilPitchScaleV_AIRGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleV_MISSILE: ctor 423CE9, exec 423DA7 -> +0x768, conv 423D5A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleV_MISSILE, hapticSettings()->setRecoilPunchScaleV_MISSILE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeV_MISSILE: ctor 423DCE, exec 423E8C -> +0x770, conv 423E3F
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeV_MISSILE, hapticSettings()->setRecoilPunchTimeV_MISSILE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleV_MISSILE: ctor 423EB3, exec 423F71 -> +0x778, conv 423F24
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleV_MISSILE, hapticSettings()->setRecoilYawScaleV_MISSILE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleV_MISSILE: ctor 423F98, exec 424056 -> +0x780, conv 424009
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleV_MISSILE, hapticSettings()->setRecoilPitchScaleV_MISSILE, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleV_ARMORCANNON: ctor 42407D, exec 42413B -> +0x788, conv 4240EE
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleV_ARMORCANNON, hapticSettings()->setRecoilPunchScaleV_ARMORCANNON, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeV_ARMORCANNON: ctor 424162, exec 424220 -> +0x790, conv 4241D3
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeV_ARMORCANNON, hapticSettings()->setRecoilPunchTimeV_ARMORCANNON, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleV_ARMORCANNON: ctor 424247, exec 424305 -> +0x798, conv 4242B8
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleV_ARMORCANNON, hapticSettings()->setRecoilYawScaleV_ARMORCANNON, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleV_ARMORCANNON: ctor 42432C, exec 4243EA -> +0x7A0, conv 42439D
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleV_ARMORCANNON, hapticSettings()->setRecoilPitchScaleV_ARMORCANNON, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchScaleV_COAXIALGUN: ctor 424411, exec 4244CF -> +0x7A8, conv 424482
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchScaleV_COAXIALGUN, hapticSettings()->setRecoilPunchScaleV_COAXIALGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPunchTimeV_COAXIALGUN: ctor 4244F6, exec 4245B4 -> +0x7B0, conv 424567
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPunchTimeV_COAXIALGUN, hapticSettings()->setRecoilPunchTimeV_COAXIALGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilYawScaleV_COAXIALGUN: ctor 4245FB, exec 4246B9 -> +0x7B8, conv 42466C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilYawScaleV_COAXIALGUN, hapticSettings()->setRecoilYawScaleV_COAXIALGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setRecoilPitchScaleV_COAXIALGUN: ctor 4246E0, exec 42479E -> +0x7C0, conv 424751
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRecoilPitchScaleV_COAXIALGUN, hapticSettings()->setRecoilPitchScaleV_COAXIALGUN, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setGeneralShakeScale: ctor 4247C5, exec 424883 -> +0x7D8, conv 424836
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setGeneralShakeScale, hapticSettings()->setGeneralShakeScale, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setExplosionShakeScale: ctor 4248AA, exec 424968 -> +0x7E0, conv 42491B
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setExplosionShakeScale, hapticSettings()->setExplosionShakeScale, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// setSpeedShakeScale: ctor 42498F, exec 424A4D -> +0x7E8, conv 424A00
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setSpeedShakeScale, hapticSettings()->setSpeedShakeScale, ::dice::hfe::g_hapticSettings != 0, float, kSettingsConsoleTag)
// addWeaponClassMap: ctor 424A74, exec 424B05 -> +0x7F0, conv 424B4D
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(addWeaponClassMap, hapticSettings()->addWeaponClassMap, ::dice::hfe::g_hapticSettings != 0, std::string, int, kSettingsConsoleTag)

// Client mirror of server settings. Preserve the console spelling setAllowNATNegotation;
// setCommunityLogoURL addresses the field currently named setField1D8. setNoVehicles
// advertises a float argument but forwards a bool.
BF2_CONSOLE_OBJECT("GameServerSettings", 2, 0)

// setServerName: ctor 425472, exec 4254EF -> +0x24, conv 42553E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setServerName, gameServerSettings()->setServerName, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setPassword: ctor 4255B3, exec 425624 -> +0x2C, conv 425673
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPassword, gameServerSettings()->setPassword, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setInternet: ctor 4256E8, exec 4257A6 -> +0x34, conv 425759
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setInternet, gameServerSettings()->setInternet, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setMaxPlayers: ctor 4257D1, exec 42588F -> +0x44, conv 425842
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setMaxPlayers, gameServerSettings()->setMaxPlayers, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setGameMode: ctor 4258B0, exec 425921 -> +0x4C, conv 425970
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setGameMode, gameServerSettings()->setGameMode, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setStartDelay: ctor 4259E5, exec 425AA3 -> +0x54, conv 425A56
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setStartDelay, gameServerSettings()->setStartDelay, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setEndDelay: ctor 425AC4, exec 425B82 -> +0x5C, conv 425B35
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setEndDelay, gameServerSettings()->setEndDelay, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setSpawnTime: ctor 425BA3, exec 425C61 -> +0x64, conv 425C14
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setSpawnTime, gameServerSettings()->setSpawnTime, ::dice::hfe::g_gameServerSettings != 0, float, kSettingsConsoleTag)
// setManDownTime: ctor 425C85, exec 425D43 -> +0x6C, conv 425CF6
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setManDownTime, gameServerSettings()->setManDownTime, ::dice::hfe::g_gameServerSettings != 0, float, kSettingsConsoleTag)
// setTicketRatio: ctor 425D67, exec 425E25 -> +0x74, conv 425DD8
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setTicketRatio, gameServerSettings()->setTicketRatio, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setRoundsPerMap: ctor 425E46, exec 425F04 -> +0x7C, conv 425EB7
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setRoundsPerMap, gameServerSettings()->setRoundsPerMap, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setTimeLimit: ctor 425F25, exec 425FE3 -> +0x84, conv 425F96
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setTimeLimit, gameServerSettings()->setTimeLimit, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setScoreLimit: ctor 426007, exec 4260C5 -> +0x8C, conv 426078
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setScoreLimit, gameServerSettings()->setScoreLimit, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setSoldierFF: ctor 4260E9, exec 4261A7 -> +0x94, conv 42615A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setSoldierFF, gameServerSettings()->setSoldierFF, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setVehicleFF: ctor 4261CB, exec 426289 -> +0x9C, conv 42623C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVehicleFF, gameServerSettings()->setVehicleFF, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setSoldierSplashFF: ctor 4262AD, exec 42636B -> +0xA4, conv 42631E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setSoldierSplashFF, gameServerSettings()->setSoldierSplashFF, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setVehicleSplashFF: ctor 42638F, exec 42644D -> +0xAC, conv 426400
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVehicleSplashFF, gameServerSettings()->setVehicleSplashFF, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setPunishTeamKills: ctor 426471, exec 42652F -> +0xB4, conv 4264E2
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPunishTeamKills, gameServerSettings()->setPunishTeamKills, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setVoteTime: ctor 42655D, exec 42661B -> +0xBC, conv 4265CE
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVoteTime, gameServerSettings()->setVoteTime, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setMinPlayersForVoting: ctor 42663F, exec 4266FD -> +0xC4, conv 4266B0
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setMinPlayersForVoting, gameServerSettings()->setMinPlayersForVoting, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setTeamVoteOnly: ctor 426721, exec 4267DF -> +0xCC, conv 426792
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setTeamVoteOnly, gameServerSettings()->setTeamVoteOnly, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setVoipEnabled: ctor 42680D, exec 4268CB -> +0xD4, conv 42687E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVoipEnabled, gameServerSettings()->setVoipEnabled, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setVoipQuality: ctor 4268F9, exec 4269B7 -> +0xDC, conv 42696A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVoipQuality, gameServerSettings()->setVoipQuality, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setVoipServerRemote: ctor 4269DB, exec 426A99 -> +0xE4, conv 426A4C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVoipServerRemote, gameServerSettings()->setVoipServerRemote, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setVoipServerRemoteIP: ctor 426AC7, exec 426B38 -> +0xEC, conv 426B8A
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVoipServerRemoteIP, gameServerSettings()->setVoipServerRemoteIP, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setVoipServerPort: ctor 426BFF, exec 426CBD -> +0xF4, conv 426C70
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVoipServerPort, gameServerSettings()->setVoipServerPort, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setVoipBFClientPort: ctor 426CE1, exec 426D9F -> +0xFC, conv 426D52
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVoipBFClientPort, gameServerSettings()->setVoipBFClientPort, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setVoipBFServerPort: ctor 426DC3, exec 426E81 -> +0x104, conv 426E34
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVoipBFServerPort, gameServerSettings()->setVoipBFServerPort, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setVoipSharedPassword: ctor 426EA5, exec 426F16 -> +0x10C, conv 426F68
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVoipSharedPassword, gameServerSettings()->setVoipSharedPassword, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setGameSpyPort: ctor 426FDD, exec 42709B -> +0x114, conv 42704E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setGameSpyPort, gameServerSettings()->setGameSpyPort, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setAllowNATNegotation: ctor 4270BF, exec 42717D -> +0x11C, conv 427130; tree method setAllowNATNegotiation
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAllowNATNegotation, gameServerSettings()->setAllowNATNegotiation, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setInterfaceIP: ctor 4271AB, exec 42721C -> +0x124, conv 42726E
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setInterfaceIP, gameServerSettings()->setInterfaceIP, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setAutoRecord: ctor 4272E3, exec 4273A1 -> +0x12C, conv 427354
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAutoRecord, gameServerSettings()->setAutoRecord, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setTimeBeforeRestartMap: ctor 4273CF, exec 42748D -> +0x154, conv 427440
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setTimeBeforeRestartMap, gameServerSettings()->setTimeBeforeRestartMap, ::dice::hfe::g_gameServerSettings != 0, float, kSettingsConsoleTag)
// setDemoIndexURL: ctor 4274B4, exec 427525 -> +0x134, conv 427577
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setDemoIndexURL, gameServerSettings()->setDemoIndexURL, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setDemoDownloadURL: ctor 4275EC, exec 42765D -> +0x13C, conv 4276AF
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setDemoDownloadURL, gameServerSettings()->setDemoDownloadURL, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setDemoHook: ctor 427724, exec 427795 -> +0x144, conv 4277E7
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setDemoHook, gameServerSettings()->setDemoHook, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setAdminScript: ctor 42785C, exec 4278CD -> +0x14C, conv 42791F
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAdminScript, gameServerSettings()->setAdminScript, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setSponsorText: ctor 427994, exec 427A05 -> +0x15C, conv 427A57
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setSponsorText, gameServerSettings()->setSponsorText, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setSponsorLogoURL: ctor 427ACC, exec 427B3D -> +0x164, conv 427B8F
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setSponsorLogoURL, gameServerSettings()->setSponsorLogoURL, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setCommunityLogoURL: ctor 427C04, exec 427C75 -> +0x18C, conv 427CC7; tree method setField1D8
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setCommunityLogoURL, gameServerSettings()->setField1D8, ::dice::hfe::g_gameServerSettings != 0, std::string, kSettingsConsoleTag)
// setSvPunkBuster: ctor 427D3C, exec 427DFA -> +0x184, conv 427DAD
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setSvPunkBuster, gameServerSettings()->setSvPunkBuster, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setAutoBalanceTeam: ctor 427E28, exec 427EE6 -> +0x16C, conv 427E99
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAutoBalanceTeam, gameServerSettings()->setAutoBalanceTeam, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setAutoRecordRounds: ctor 427F14, exec 427FD2 -> +0x174, conv 427F85
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAutoRecordRounds, gameServerSettings()->setAutoRecordRounds, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setTeamRatio: ctor 427FF6, exec 4280B4 -> +0x17C, conv 428067
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setTeamRatio, gameServerSettings()->setTeamRatio, ::dice::hfe::g_gameServerSettings != 0, float, kSettingsConsoleTag)
// setFriendlyFireWithMines: ctor 4280DB, exec 428199 -> +0x194, conv 42814C
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setFriendlyFireWithMines, gameServerSettings()->setFriendlyFireWithMines, ::dice::hfe::g_gameServerSettings != 0, bool, kSettingsConsoleTag)
// setCoopBotRatio: ctor 4281C7, exec 428285 -> +0x19C, conv 428238
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setCoopBotRatio, gameServerSettings()->setCoopBotRatio, ::dice::hfe::g_gameServerSettings != 0, float, kSettingsConsoleTag)
// setCoopBotCount: ctor 4282AC, exec 42836A -> +0x1A4, conv 42831D
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setCoopBotCount, gameServerSettings()->setCoopBotCount, ::dice::hfe::g_gameServerSettings != 0, int, kSettingsConsoleTag)
// setCoopBotDifficulty: ctor 42838E, exec 42844C -> +0x1AC, conv 4283FF
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setCoopBotDifficulty, gameServerSettings()->setCoopBotDifficulty, ::dice::hfe::g_gameServerSettings != 0, float, kSettingsConsoleTag)
// setNoVehicles: ctor 428473, exec 428531 -> +0x1B4, conv 4284E4
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setNoVehicles, gameServerSetNoVehicles, ::dice::hfe::g_gameServerSettings != 0, float, kSettingsConsoleTag)

// Handed to the console registry by ModuleGame::init / close (Core/ModuleRegistry.cpp), beside the
// other slices of the 0x98913C list, under `#if !DEDICATED`.
ConsoleObjectList& settingsConsoleObjects()
{
    return consoleBindingList(kSettingsConsoleTag);
}

} // namespace hfe
} // namespace dice

#endif // !dedicated
