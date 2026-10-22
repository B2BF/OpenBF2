// Code/Tools/BF2Editor/App/EditSessionLightSetup.cpp - the terrain and object light setups. Editor-only code (D3).

// Two lightmap light setups per level, `Editor/ObjectLight.con` and `Editor/TerrainLight.con`, one of them current: the
// object setup when the level opens. As the retail SkyManager's SetObjectMode / SetTerrainMode / Save*Mode did,
// switching saves the setup being left and loads the other, and saving writes the current one; neither marks the level
// modified. [CHOSEN] A switch saves the setup left only when it changed (retail also skipped the session's first
// switch, and a second level loaded in terrain mode had the new level's TerrainLight.con overwritten by the old
// settings - neither is kept). The setups take effect once lightmap generation exists (parity §9, ): 1.50 RendDX9
// binds none of the lightmapGeneration settings, so the editor keeps them itself.
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/LevelDocument.h"
#include "Framework/EditorLog.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SourceControl.h"

#include <windows.h>

namespace bf2editor
{

namespace
{

const char* const kSetupFiles[2] = { "ObjectLight.con", "TerrainLight.con" };
const char* const kModes[2] = { "object", "terrain" };

} // namespace

bool EditSession::lightSetupCommand(const std::string& action, const std::string& argument, std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level document: the light setups live in the level's Editor folder";
        return false;
    }
    const std::string folder = m_document->levelDirectory() + "Editor/";
    if (!m_lightSetupLoaded)
    {
        // Read once a level: a missing file keeps the defaults.
        for (int m = 0; m < 2; ++m)
        {
            std::string text, error;
            m_lightSetups[m] = LightSetup();
            if (ObjectLibrary::readTextFile(folder + kSetupFiles[m], text, error))
                m_lightSetups[m].parse(text);
            m_lightSetupModified[m] = false;
        }
        m_lightSetupMode = 0;
        m_lightSetupLoaded = true;
    }
    const auto save = [&](int m, std::string& error) {
        const std::string path = folder + kSetupFiles[m];
        if (!writeFilesNow("light setup", { std::make_pair(path, m_lightSetups[m].render()) }, error))
            return false;
        m_lightSetupModified[m] = false;
        return true;
    };
    if (action == "lightSetup")
    {
        result = std::string(kModes[m_lightSetupMode]) + " light setup (" + kSetupFiles[m_lightSetupMode] + "):";
        const std::vector<std::string>& names = LightSetup::settingNames();
        for (size_t i = 0; i < names.size(); ++i)
            result += (i ? ", " : " ") + names[i] + " " + m_lightSetups[m_lightSetupMode].values[i];
        if (m_lightSetupModified[m_lightSetupMode])
            result += "; modified";
        return true;
    }
    if (action == "lightSetupMode")
    {
        const int mode = argument == "object" ? 0 : argument == "terrain" ? 1 : -1;
        if (mode < 0)
        {
            result = "EditorLighting.lightSetupMode <object|terrain>";
            return false;
        }
        if (mode == m_lightSetupMode)
        {
            result = std::string("the ") + kModes[mode] + " light setup is the current one already";
            return true;
        }
        std::string error;
        const bool saved = m_lightSetupModified[m_lightSetupMode];
        if (saved && !save(m_lightSetupMode, error))
        {
            result = error;
            return false;
        }
        result = (saved ? std::string("saved ") + kSetupFiles[m_lightSetupMode] + "; " : std::string()) + "the " + kModes[mode]
               + " light setup is the current one";
        m_lightSetupMode = mode;
        return true;
    }
    if (action == "lightSetupSet")
    {
        const size_t space = argument.find(' ');
        std::string error;
        if (space == std::string::npos || !m_lightSetups[m_lightSetupMode].set(argument.substr(0, space), argument.substr(space + 1), error))
        {
            result = space == std::string::npos ? std::string("EditorLighting.lightSetupSet <setting> <value>") : error;
            return false;
        }
        m_lightSetupModified[m_lightSetupMode] = true;
        result = "set " + argument + " in the " + kModes[m_lightSetupMode] + " light setup";
        return true;
    }
    if (action == "lightSetupSave")
    {
        std::string error;
        if (!save(m_lightSetupMode, error))
        {
            result = error;
            return false;
        }
        result = std::string("saved ") + kSetupFiles[m_lightSetupMode];
        logLine(LogLevel::Info, "lighting: %s", result.c_str());
        return true;
    }
    result = "no light setup action '" + action + "'";
    return false;
}

} // namespace bf2editor
