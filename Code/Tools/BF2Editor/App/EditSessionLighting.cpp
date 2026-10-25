// Lighting and minimap bakes invoke renderer commands and write files immediately, outside undo (RW-273).
// Terrain lightmaps are traced into the editable terrain's staging folder, then copied into the level.
// Minimap capture disables shadow updates to avoid window-sized depth clipping.
// Environment saves stage files before copying them into the level; the 1.50 EnvMapTemplate factory
// returns null, so it currently produces no environment objects. Remove only the bake's temporary lightingTex.dds.
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/EditorLevel.h"
#include "App/LevelDocument.h"
#include "Framework/EditorConsole.h"
#include "Framework/EditorLog.h"
#include "Framework/NewLevel.h"
#include "App/StagingFolder.h"
#include "Framework/LightmapAtlas.h"

#include "Core/SettingsRepostitory.h"
#include "Core/SmartItf.h"
#include "Geom/XpLightManager.h"
#include "Scene/Object/ObjectManager.h"
#include "IO/Console/Console.h"

#include <windows.h>

#include <cctype>
#include <cstdio>
#include <map>
#include <vector>

namespace bf2editor
{

namespace
{

// One engine console line with full access; its answer, trimmed (EditSession.cpp's engineLine).
std::string engineLine(const std::string& line)
{
    std::string answer;
    dice::hfe::io::mainConsole->executeLine(line + "\n", answer, false, false, false, false, true);
    const size_t begin = answer.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    return answer.substr(begin, answer.find_last_not_of(" \t\r\n") - begin + 1);
}

bool fileThere(const std::string& path)
{
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Every file under root/relative, as paths relative to root.
void listFiles(const std::string& root, const std::string& relative, std::vector<std::string>& out)
{
    WIN32_FIND_DATAA found;
    const std::string pattern = root + "/" + (relative.empty() ? std::string() : relative + "/") + "*";
    HANDLE h = FindFirstFileA(pattern.c_str(), &found);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const std::string name = found.cFileName;
        if (name == "." || name == "..")
            continue;
        const std::string path = relative.empty() ? name : relative + "/" + name;
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            listFiles(root, path, out);
        else
            out.push_back(path);
    } while (FindNextFileA(h, &found));
    FindClose(h);
}

void makeFolders(const std::string& path)
{
    for (size_t i = 0; i < path.size(); ++i)
        if (path[i] == '/' || path[i] == '\\')
            CreateDirectoryA(path.substr(0, i).c_str(), 0);
}

} // namespace

bool EditSession::lightingLevel(std::string& levelDir, std::string& levelPath, std::string& error) const
{
    if (!m_document || !m_document->isOpen() || !dice::hfe::io::mainConsole)
    {
        error = "no level is open";
        return false;
    }
    // "mods/bf2/Levels/<name>/" and the level path the engine's files name ("Levels/<name>").
    levelDir = m_document->levelDirectory();
    const size_t levels = levelDir.find("Levels/");
    if (levels == std::string::npos)
    {
        error = "cannot find Levels/ in " + levelDir;
        return false;
    }
    levelPath = levelDir.substr(levels);
    if (!levelPath.empty() && levelPath[levelPath.size() - 1] == '/')
        levelPath.erase(levelPath.size() - 1);
    if (!levelDir.empty() && levelDir[levelDir.size() - 1] != '/')
        levelDir += '/';
    return true;
}

bool EditSession::bakeGroundHemi(int resolution, const dice::hfe::Vec3& position, float size, std::string& result)
{
    std::string levelDir, levelPath;
    if (!lightingLevel(levelDir, levelPath, result))
        return false;
    if (resolution < 16 || resolution > 4096 || size <= 0.0f)
    {
        result = "a hemi map is 16 to 4096 pixels square and covers a positive size";
        return false;
    }
    const std::string file = levelDir + "groundhemi.dds";
    const bool hadLightingTexture = fileThere("lightingTex.dds");
    char line[512];
    std::snprintf(line, sizeof(line), "hemiMapManager.makeHemiMap %d %s %g/%g/%g %g", resolution, file.c_str(), position.x,
                  position.y, position.z, size);
    const std::string answer = engineLine(line);
    if (!hadLightingTexture && fileThere("lightingTex.dds"))
        DeleteFileA("lightingTex.dds");
    if (answer != "1")
    {
        result = "the hemi bake failed (hemiMapManager.makeHemiMap answered '" + answer + "')";
        return false;
    }
    const std::string shown = engineLine("lightManager.groundHemi " + levelPath + "/groundhemi");
    std::snprintf(line, sizeof(line), " (%dx%d, from %g/%g/%g over %g m)", resolution, resolution, position.x, position.y,
                  position.z, size);
    result = "baked " + file + line + (shown.empty() ? "; the view shows it" : "; lightManager.groundHemi answered '" + shown + "'")
           + " (written now: not an undoable edit)";
    logLine(LogLevel::Info, "lighting: %s", result.c_str());
    return true;
}

bool EditSession::captureMinimap(int resolution, std::string& result)
{
    std::string levelDir, levelPath;
    if (!lightingLevel(levelDir, levelPath, result))
        return false;
    float bounds[4];
    if (!m_level.worldBounds(bounds))
    {
        result = "the level has no terrain extent to capture";
        return false;
    }
    if (resolution < 16 || resolution > 4096)
    {
        result = "a minimap is 16 to 4096 pixels square";
        return false;
    }
    const float size = bounds[2] - bounds[0];
    const std::string file = levelDir + "minimap";
    char line[512];
    std::snprintf(line, sizeof(line), "renderer.makeTopWorldScreen %d %s 0/500/0 %g", resolution, file.c_str(), size);
    // The shadow-map pass hands the renderer's own (window-sized) depth-stencil back in the middle of the
    // frame (RW-273), so a capture taller than the window keeps only the window's rows. With the light
    // manager's shadow updates off for the capture - as retail turned the sky dome off around its own -
    // that pass does not run and the whole square is drawn; they are put back after.
    dice::hfe::geom::LightManager* const lights =
        dice::hfe::classManager ? dice::hfe::geom::asLightManager(dice::hfe::classManager->getSingleton(std::string("LightManager"))) : 0;
    const bool shadows = lights && lights->getShadowUpdatesEnabled();
    if (shadows)
        lights->setShadowUpdatesEnabled(false);
    const std::string answer = engineLine(line);
    if (shadows)
        lights->setShadowUpdatesEnabled(true);
    if (!answer.empty() || !fileThere(file + ".dds"))
    {
        result = "the capture wrote no " + file + ".dds (renderer.makeTopWorldScreen answered '" + answer + "')";
        return false;
    }
    ++m_minimapVersion;
    std::snprintf(line, sizeof(line), " (the top-down view over %g m; the sky dome is not turned off: the rebuilt "
                  "RendDX9 binds no renderer.drawSkyDome)", size);
    result = "captured " + file + ".dds" + line + " (written now: not an undoable edit)";
    logLine(LogLevel::Info, "lighting: %s", result.c_str());
    return true;
}

bool EditSession::generateLightmaps(int quality, const std::string& parts, const std::string& options,
                                    std::string& result)
{
    std::string levelDir, levelPath;
    if (!lightingLevel(levelDir, levelPath, result))
        return false;
    // The retail dialog's resolutions (Low, Medium, Final and the fourth), as its driver set the modifier.
    static const float kModifiers[4] = { 0.25f, 0.5f, 1.0f, 2.0f };
    if (quality < 0 || quality > 3)
    {
        result = "the quality is 0..3 (resolution 0.25, 0.5, 1 or 2)";
        return false;
    }
    // The parts, joined by '+': the terrain's (primary, secondary, both) and the objects' (objects, or selected).
    bool primary = false, surrounding = false, objects = false, selected = false;
    for (size_t start = 0; start <= parts.size();)
    {
        size_t end = parts.find('+', start);
        if (end == std::string::npos)
            end = parts.size();
        const std::string part = parts.substr(start, end - start);
        if (part == "primary" || part == "both")
            primary = true;
        if (part == "secondary" || part == "both")
            surrounding = true;
        if (part == "objects")
            objects = true;
        else if (part == "selected")
            selected = true;
        else if (part != "primary" && part != "secondary" && part != "both")
        {
            result = "the parts are primary, secondary, both, objects or selected, joined by + (objects and selected not "
                     "together)";
            return false;
        }
        start = end + 1;
    }
    if (objects && selected)
    {
        result = "the parts are primary, secondary, both, objects or selected, joined by + (objects and selected not "
                 "together)";
        return false;
    }
    const bool terrain = primary || surrounding;
    bool nearest = false, resume = false;
    for (size_t start = 0; options != "-" && start <= options.size();)
    {
        size_t end = options.find('+', start);
        if (end == std::string::npos)
            end = options.size();
        const std::string option = options.substr(start, end - start);
        if (option == "nearest")
            nearest = true;
        else if (option == "resume")
            resume = true;
        else
        {
            result = "the options are - or nearest, resume, nearest+resume";
            return false;
        }
        start = end + 1;
    }
    if (terrain && !m_editableTerrain.active())
    {
        result = "terrain lightmaps are traced on the editable terrain (start the editor with +editableTerrain 1)";
        return false;
    }
    if (selected && m_selection.empty())
    {
        result = "no object is selected (the selected part traces the selected objects)";
        return false;
    }
    // The generator set up as the retail editor's driver did it (EditorEngine's CompileManager::generateLightmaps):
    // created, the resume log (removed first unless resuming), the resolution and onlyNearest, then the terrain
    // lighting preset - the level's Editor/TerrainLight.con run with BF2Editor (its SkyManager's terrain mode). 1.50
    // dropped the generator's includeWaterDepth, which the driver set from renderer.IsNightMap.
    std::vector<std::string> lines;
    lines.push_back("lightmapGeneration.init");
    const char kLog[] = "Logs/LightmapGeneration.log";
    CreateDirectoryA("Logs", 0);
    if (!resume)
        DeleteFileA(kLog);
    lines.push_back(std::string("lightmapGeneration.useLog \"") + kLog + "\"");
    char line[128];
    std::snprintf(line, sizeof(line), "lightmapGeneration.resolutionModifier %f", kModifiers[quality]);
    lines.push_back(line);
    std::snprintf(line, sizeof(line), "lightmapGeneration.onlyNearest %d", nearest ? 1 : 0);
    lines.push_back(line);
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const ConsoleLineResult r = runConsoleLine(lines[i]);
        if (r.failed)
        {
            result = "lightmaps NOT generated: " + lines[i] + " answered '" + r.output + "'";
            return false;
        }
    }
    std::string summary;
    if (terrain && !traceTerrainLightmaps(quality, primary, surrounding, nearest, resume, levelDir, levelPath, summary))
    {
        result = summary;
        return false;
    }
    if (objects || selected)
    {
        std::string objectSummary;
        if (!traceObjectLightmaps(quality, selected, objectSummary))
        {
            result = (summary.empty() ? std::string() : summary + "; ") + objectSummary;
            return false;
        }
        summary += (summary.empty() ? std::string() : "; ") + objectSummary;
    }
    result = summary + " (written now: not an undoable edit)";
    logLine(LogLevel::Info, "lighting: %s", result.c_str());
    return true;
}

bool EditSession::traceTerrainLightmaps(int quality, bool primary, bool surrounding, bool nearest, bool resume,
                                        const std::string& levelDir, const std::string& levelPath, std::string& result)
{
    static const float kModifiers[4] = { 0.25f, 0.5f, 1.0f, 2.0f };
    std::vector<std::string> lines;
    std::string preset;
    if (fileThere(levelDir + "Editor/TerrainLight.con"))
    {
        lines.push_back("run " + levelPath + "/Editor/TerrainLight.con BF2Editor");
        preset = "the level's Editor/TerrainLight.con";
    }
    else
    {
        // [CHOSEN] A level without the file (no stock level ships its Editor folder) is lit with the preset the retail
        // editor writes for a new level.
        const std::vector<std::string> defaults = terrainLightDefaults();
        lines.insert(lines.end(), defaults.begin(), defaults.end());
        preset = "the retail editor's preset for a new level (the level has no Editor/TerrainLight.con)";
    }
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const ConsoleLineResult r = runConsoleLine(lines[i]);
        if (r.failed)
        {
            result = "terrain lightmaps NOT generated: " + lines[i] + " answered '" + r.output + "'";
            return false;
        }
    }
    std::vector<std::pair<std::string, std::string> > files;
    std::string error;
    const DWORD start = GetTickCount();
    if (!m_editableTerrain.traceLightmaps(primary, surrounding, files, error))
    {
        result = "terrain lightmaps NOT generated: " + error;
        return false;
    }
    const unsigned long seconds = static_cast<unsigned long>((GetTickCount() - start) / 1000);
    int written = 0;
    if (!copyStagedFiles(files, written, error))
    {
        result = "the terrain lightmaps were traced, but not copied into the level: " + error;
        return false;
    }
    char text[256];
    std::snprintf(text, sizeof(text), "traced the terrain's lightmaps (%s patches, resolution %g%s%s) in %lu s: %u "
                  "file%s, %d written into the level", primary && surrounding ? "primary and ring" : primary ? "primary"
                  : "ring", static_cast<double>(kModifiers[quality]), nearest ? ", only the nearest block" : "",
                  resume ? ", resumed" : "", seconds, static_cast<unsigned>(files.size()), files.size() == 1 ? "" : "s",
                  written);
    result = std::string(text) + "; lit with " + preset;
    return true;
}

bool EditSession::traceObjectLightmaps(int quality, bool selectedOnly, std::string& result)
{
    std::string levelDir, levelPath;
    if (!lightingLevel(levelDir, levelPath, result))
        return false;
    // As the retail driver: the High resolution traces objects at 1.0; then the level's object preset (its SkyManager's
    // object mode ran Editor/ObjectLight.con with BF2Editor).
    std::vector<std::string> lines;
    if (quality == 3)
        lines.push_back("lightmapGeneration.resolutionModifier 1.000000");
    std::string preset;
    if (fileThere(levelDir + "Editor/ObjectLight.con"))
    {
        lines.push_back("run " + levelPath + "/Editor/ObjectLight.con BF2Editor");
        preset = "the level's Editor/ObjectLight.con";
    }
    else
    {
        // [CHOSEN] A level without the file (no stock level ships its Editor folder, and the retail editor's new level
        // has none) is lit with the retail editor's terrain preset for a new level.
        const std::vector<std::string> defaults = terrainLightDefaults();
        lines.insert(lines.end(), defaults.begin(), defaults.end());
        preset = "the retail editor's preset for a new level (the level has no Editor/ObjectLight.con)";
    }
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const ConsoleLineResult r = runConsoleLine(lines[i]);
        if (r.failed)
        {
            result = "object lightmaps NOT generated: " + lines[i] + " answered '" + r.output + "'";
            return false;
        }
    }
    // The samplers write <Editor_LevelSavePath>/<level path>/Lightmaps/Objects/<template>=<geom>=<position>.dds; the atlas
    // reads that folder through the file manager and writes LightmapAtlas.tai and its pages beside the lightmaps. The
    // folder starts with every object lightmap the level has - its atlas unpacked (a stock level has nothing else), then
    // its own single lightmaps over them - so the atlas keeps the objects this trace does not reach.
    std::string root, error;
    if (!makeStagingFolder("lightmaps", root, error))
    {
        result = "object lightmaps NOT generated: " + error;
        return false;
    }
    const std::string folder = levelPath + "/Lightmaps/Objects";
    const std::string staged = root + "/" + folder + "/";
    CreateDirectoryA((root + "/Levels").c_str(), 0);
    CreateDirectoryA((root + "/" + levelPath).c_str(), 0);
    CreateDirectoryA((root + "/" + levelPath + "/Lightmaps").c_str(), 0);
    CreateDirectoryA((root + "/" + folder).c_str(), 0);
    int unpacked = 0, kept = 0;
    std::string index;
    if (ObjectLibrary::readTextFile(levelDir + "Lightmaps/Objects/LightmapAtlas.tai", index, error))
    {
        std::vector<AtlasEntry> entries;
        if (!parseAtlasIndex(index, entries, error))
        {
            result = "object lightmaps NOT generated: the level's LightmapAtlas.tai: " + error;
            return false;
        }
        std::map<std::string, std::string> pages;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const std::string pageName = atlasFileName(entries[i].atlasFile);
            std::map<std::string, std::string>::iterator page = pages.find(pageName);
            if (page == pages.end())
            {
                std::string bytes;
                ObjectLibrary::readTextFile(levelDir + "Lightmaps/Objects/" + pageName, bytes, error);
                page = pages.insert(std::make_pair(pageName, bytes)).first;
            }
            std::string texture;
            if (cutAtlasTexture(page->second, entries[i], texture, error)
                && ObjectLibrary::writeTextFile(staged + atlasFileName(entries[i].texture), texture, error))
                ++unpacked;
        }
    }
    std::vector<std::string> own;
    listFolderFiles(levelDir + "Editor/Lightmaps/Objects", std::string(), own);
    for (size_t i = 0; i < own.size(); ++i)
    {
        std::string data;
        if (own[i].find('/') == std::string::npos
            && ObjectLibrary::readTextFile(levelDir + "Editor/Lightmaps/Objects/" + own[i], data, error)
            && ObjectLibrary::writeTextFile(staged + own[i], data, error))
            ++kept;
    }
    FILETIME before;
    GetSystemTimeAsFileTime(&before);
    const DWORD start = GetTickCount();
    int traced = 0;
    std::string atlas;
    {
        StagingRedirect redirect(root);
        if (selectedOnly)
        {
            dice::hfe::world::IObject* const active =
                dice::hfe::world::objectManager ? dice::hfe::world::objectManager->getActiveObject() : 0;
            const std::vector<unsigned> ids = m_selection.ids();
            for (size_t i = 0; i < ids.size(); ++i)
            {
                std::string why;
                if (!activateObject(ids[i], why))
                    continue;
                char line[96];
                std::snprintf(line, sizeof(line), "lightmapGeneration.traceActiveObjectProgress %u %u",
                              static_cast<unsigned>(i), static_cast<unsigned>(ids.size()));
                const ConsoleLineResult r = runConsoleLine(line);
                if (!r.failed && r.output == "1")
                    ++traced;
            }
            if (dice::hfe::world::objectManager)
                dice::hfe::world::objectManager->setActiveObject(active);
        }
        else
        {
            const ConsoleLineResult r = runConsoleLine("lightmapGeneration.traceAllObjects");
            if (r.failed || r.output != "1")
            {
                result = "object lightmaps NOT generated: lightmapGeneration.traceAllObjects answered '" + r.output + "'";
                return false;
            }
        }
        // The atlas the game reads, packed from every lightmap in the folder.
        const ConsoleLineResult r = runConsoleLine("StaticMeshRenderer.loadAllLightMapFilesDLL " + folder + "/ .dds");
        if (r.failed || r.output != "1")
            atlas = "the atlas NOT packed ('" + r.output + "')";
    }
    const unsigned long seconds = static_cast<unsigned long>((GetTickCount() - start) / 1000);
    // The level gets the atlas (Lightmaps/Objects) and the lightmaps this trace wrote (Editor/Lightmaps/Objects: the
    // packer leaves the Editor folder out of the game's archives).
    std::vector<std::string> names;
    listFolderFiles(root + "/" + folder, std::string(), names);
    std::vector<std::pair<std::string, std::string> > files;
    int lightmaps = 0, pages = 0;
    for (size_t i = 0; i < names.size(); ++i)
    {
        const std::string path = staged + names[i];
        std::string low = names[i];
        for (size_t k = 0; k < low.size(); ++k)
            low[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(low[k])));
        if (low.compare(0, 13, "lightmapatlas") == 0)
        {
            files.push_back(std::make_pair(path, "Lightmaps/Objects/" + names[i]));
            pages += low.size() > 4 && low.compare(low.size() - 4, 4, ".dds") == 0 ? 1 : 0;
            continue;
        }
        WIN32_FILE_ATTRIBUTE_DATA info;
        if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &info)
            && CompareFileTime(&info.ftLastWriteTime, &before) >= 0)
        {
            files.push_back(std::make_pair(path, "Editor/Lightmaps/Objects/" + names[i]));
            ++lightmaps;
        }
    }
    if (lightmaps == 0)
    {
        result = "object lightmaps NOT generated: the trace wrote none (each mesh's .samples file: the editor mounts "
                 "bf2editor/LightmapSamples*.zip at Objects, or a mod's loose Objects folder holds them)";
        return false;
    }
    if (atlas.empty() && pages == 0)
        atlas = "the atlas NOT packed (no page written)";
    if (atlas.empty())
    {
        char text[96];
        std::snprintf(text, sizeof(text), "the atlas packed (%d pages)", pages);
        atlas = text;
    }
    int written = 0;
    if (!copyStagedFiles(files, written, error))
    {
        result = "the object lightmaps were traced, but not copied into the level: " + error;
        return false;
    }
    // A smaller atlas leaves the old one's last pages in the folder, named by no entry now; the folder clean (L2)
    // moves them aside.
    size_t stale = 0;
    std::vector<AtlasEntry> packed;
    std::string packedIndex;
    if (ObjectLibrary::readTextFile(levelDir + "Lightmaps/Objects/LightmapAtlas.tai", packedIndex, error)
        && parseAtlasIndex(packedIndex, packed, error))
    {
        std::vector<std::string> listed, present;
        listFolderFiles(levelDir + "Lightmaps/Objects", std::string(), listed);
        for (size_t i = 0; i < listed.size(); ++i)
            if (listed[i].find('/') == std::string::npos)   // not those moved into Unused
                present.push_back(listed[i]);
        stale = unnamedAtlasPages(packed, present).size();
    }
    char text[400];
    std::snprintf(text, sizeof(text), "traced %s in %lu s: %d lightmaps written (%d unpacked from the level's atlas, %d of "
                  "its own kept), %s; %d files written into the level",
                  selectedOnly ? "the selected objects' lightmaps" : "the objects' lightmaps", seconds, lightmaps,
                  unpacked, kept, atlas.c_str(), written);
    result = std::string(text) + "; lit with " + preset;
    if (stale)
    {
        std::snprintf(text, sizeof(text), "; %u pages of the old atlas are named by no entry now (EditorLighting.cleanLightmaps "
                      "1 moves them into Lightmaps/Objects/Unused)", static_cast<unsigned>(stale));
        result += text;
    }
    if (selectedOnly && traced < static_cast<int>(m_selection.size()))
    {
        char more[96];
        std::snprintf(more, sizeof(more), "; %d of the %u selected objects traced", traced,
                      static_cast<unsigned>(m_selection.size()));
        result += more;
    }
    return true;
}

bool EditSession::generateEnvMaps(std::string& result)
{
    std::string levelDir, levelPath;
    if (!lightingLevel(levelDir, levelPath, result))
        return false;
    if (!dice::hfe::settings)
    {
        result = "no settings repository";
        return false;
    }
    const std::string generated = engineLine("envmapManager.generateEnvMaps");
    if (!generated.empty())
    {
        result = "envmapManager.generateEnvMaps answered '" + generated + "'";
        return false;
    }
    // EnvMapManager::save writes <Editor_LevelSavePath>/<level path>/Envmaps/...: a staging folder for the call.
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "bf2editor_envmaps_%lu", static_cast<unsigned long>(GetCurrentProcessId()));
    std::string staging = std::string(temp) + stamp;
    for (size_t i = 0; i < staging.size(); ++i)
        if (staging[i] == '\\')
            staging[i] = '/';
    makeFolders(staging + "/" + levelPath + "/Envmaps/");
    const std::string key = "Editor_LevelSavePath";
    const std::string previous = dice::hfe::settings->stringGet(key, std::string());
    dice::hfe::settings->stringSet(key, staging);
    const std::string saved = engineLine("envmapManager.save");
    dice::hfe::settings->stringSet(key, previous);
    std::vector<std::string> written;
    listFiles(staging, "", written);
    // "Levels/<name>/Envmaps/x" -> "Envmaps/x" in the level, copied in as one backed-up save.
    std::vector<std::pair<std::string, std::string> > files;
    for (size_t i = 0; i < written.size(); ++i)
        files.push_back(std::make_pair(staging + "/" + written[i], written[i].size() > levelPath.size() + 1
                                                                       ? written[i].substr(levelPath.size() + 1) : written[i]));
    int copied = 0;
    std::string failed;
    if (!files.empty() && !copyStagedFiles(files, copied, failed))
        copied = 0;
    for (size_t i = 0; i < files.size(); ++i)
        DeleteFileA(files[i].first.c_str());
    if (written.empty())
    {
        // The save is a void row: an empty answer is its only one. In 1.50 EnvMapTemplate::createObject returns
        // no object (its shipped body is `return 0`), so no environment-map object ever reaches the manager
        // and EnvMapManager::save has none to write - retail's editor generated them on its own 2006 engine.
        result = "envmapManager.save wrote nothing: in 1.50 an environment-map template makes no object "
                 "(EnvMapTemplate::createObject returns null, as shipped), so the manager holds none to save"
               + (saved.empty() ? std::string() : " (it answered '" + saved + "')");
        return false;
    }
    if (copied == 0)
    {
        result = "envmapManager.save wrote files that could not be copied into " + levelDir + "Envmaps: " + failed;
        return false;
    }
    char count[96];
    std::snprintf(count, sizeof(count), "%d file%s", copied, copied == 1 ? "" : "s");
    result = std::string("generated the environment maps and wrote ") + count + " into " + levelDir + "Envmaps"
           + (failed.empty() ? std::string() : "; not copied: " + failed) + " (written now: not an undoable edit)";
    logLine(LogLevel::Info, "lighting: %s", result.c_str());
    return failed.empty();
}

} // namespace bf2editor
