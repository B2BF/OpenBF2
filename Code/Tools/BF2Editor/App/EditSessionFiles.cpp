// Code/Tools/BF2Editor/App/EditSessionFiles.cpp

// EditSession's whole-level file operations: packing the saved level for the game, copying it under a new name,
// making a new level, and the mod manager. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/EditorLevel.h"
#include "App/LevelDocument.h"
#include "Framework/EditorLog.h"
#include "Framework/LevelCopier.h"
#include "Framework/LevelPacker.h"
#include "Framework/ModScaffold.h"
#include "Framework/NewLevel.h"
#include "Framework/TerrainRawFile.h"

#include <windows.h>

#include <cstdio>

namespace bf2editor
{

namespace
{

bool readFileBytes(const std::string& path, std::string& bytes)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    bytes.clear();
    char buffer[65536];
    size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        bytes.append(buffer, n);
    const bool ok = !std::ferror(file);
    std::fclose(file);
    return ok;
}

} // namespace

bool EditSession::packLevel(const std::string& modFolder, std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level document";
        return false;
    }
    if (hasUnsavedChanges())
    {
        result = "the level has unsaved changes: save it first (a pack takes the level's files as saved)";
        return false;
    }
    if (modFolder.empty())
    {
        result = "EditorLevel.pack takes the mod folder to pack into (<game>/mods/<mod>)";
        return false;
    }
    // "mods/bf2/Levels/<name>/" -> the absolute source, and <mod folder>/Levels/<name> as the destination.
    std::string relative = m_document->levelDirectory();
    while (!relative.empty() && (relative[relative.size() - 1] == '/' || relative[relative.size() - 1] == '\\'))
        relative.erase(relative.size() - 1);
    const size_t slash = relative.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? relative : relative.substr(slash + 1);
    char source[MAX_PATH], destination[MAX_PATH];
    if (!GetFullPathNameA(relative.c_str(), MAX_PATH, source, 0)
        || !GetFullPathNameA((modFolder + "/Levels/" + name).c_str(), MAX_PATH, destination, 0))
    {
        result = "bad folder " + modFolder;
        return false;
    }
    LevelPackReport report;
    std::string error;
    if (!bf2editor::packLevel(source, destination, report, error))     // the free function, not this member
    {
        result = error;
        return false;
    }
    char text[256];
    std::snprintf(text, sizeof(text), "packed %s: client.zip %d files (%llu bytes), server.zip %d files (%llu bytes), "
                  "%d Info files, %u not packed, into ", name.c_str(), report.clientFiles, report.clientBytes,
                  report.serverFiles, report.serverBytes, report.infoFiles, static_cast<unsigned>(report.skipped.size()));
    result = text + std::string(destination);
    logLine(LogLevel::Info, "pack: %s", result.c_str());
    for (size_t i = 0; i < report.skipped.size(); ++i)
        logLine(LogLevel::Info, "pack: not packed: %s", report.skipped[i].c_str());
    return true;
}

bool EditSession::saveLevelAs(const std::string& name, bool open, std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level document";
        return false;
    }
    if (hasUnsavedChanges())
    {
        result = "the level has unsaved changes: save it first (Save level as copies the level as saved)";
        return false;
    }
    if (!validLevelName(name))
    {
        result = "'" + name + "' is not a level name (letters, digits and underscores)";
        return false;
    }
    // "mods/bf2/Levels/<old>/" -> the source folder, its parent and the old name.
    std::string relative = m_document->levelDirectory();
    while (!relative.empty() && (relative[relative.size() - 1] == '/' || relative[relative.size() - 1] == '\\'))
        relative.erase(relative.size() - 1);
    const size_t slash = relative.find_last_of("/\\");
    const std::string old = slash == std::string::npos ? relative : relative.substr(slash + 1);
    const std::string parent = slash == std::string::npos ? std::string(".") : relative.substr(0, slash);
    if (_stricmp(old.c_str(), name.c_str()) == 0)
    {
        result = "the copy needs a name of its own";
        return false;
    }
    char source[MAX_PATH], target[MAX_PATH];
    if (!GetFullPathNameA(relative.c_str(), MAX_PATH, source, 0)
        || !GetFullPathNameA((parent + "/" + name).c_str(), MAX_PATH, target, 0))
    {
        result = "bad level folder";
        return false;
    }
    LevelCopyReport report;
    std::string error;
    if (!copyLevel(source, target, old, name, report, error))
    {
        result = error;
        return false;
    }
    char text[256];
    std::snprintf(text, sizeof(text), "copied %s as %s: %d files, %d path references in %d files pointed at the copy, "
                  "%d files renamed", old.c_str(), name.c_str(), report.files, report.references, report.rewritten,
                  report.renamed);
    result = text;
    logLine(LogLevel::Info, "save as: %s", result.c_str());
    if (open)
    {
        if (!m_canRelaunch)
        {
            result += "; opening it starts the editor again, which a hidden or batch run does not do (+loadLevel " + name
                      + ")";
            return true;
        }
        m_relaunchMode = m_level.gameMode();
        m_relaunchSize = m_level.layerSize();
        m_relaunchLevel = name;
        result += "; the editor starts again on it";
    }
    return true;
}

bool EditSession::createLevel(const std::string& name, const std::string& terrain, bool open, std::string& result)
{
    NewLevelSettings settings;
    settings.name = name;
    if (_stricmp(terrain.c_str(), "default") != 0)
    {
        char tail = 0;
        if (std::sscanf(terrain.c_str(), "%d/%d/%d/%d%c", &settings.size, &settings.scale, &settings.defaultHeight,
                        &settings.lowResSize, &tail) != 4)
        {
            result = "the terrain is size/scale/height/lowResSize (for example 1024/2/25/256) or default";
            return false;
        }
    }
    std::string error;
    if (!checkNewLevelSettings(settings, error))
    {
        result = error;
        return false;
    }
    if (!m_document || !m_document->isOpen())
    {
        result = "no level document: a new level is made beside the open one";
        return false;
    }
    // "mods/<mod>/Levels/<open>/" -> the open level's folder and the Levels folder holding it.
    std::string relative = m_document->levelDirectory();
    while (!relative.empty() && (relative[relative.size() - 1] == '/' || relative[relative.size() - 1] == '\\'))
        relative.erase(relative.size() - 1);
    const size_t slash = relative.find_last_of("/\\");
    const std::string parent = slash == std::string::npos ? std::string(".") : relative.substr(0, slash);
    char target[MAX_PATH];
    if (!GetFullPathNameA((parent + "/" + name).c_str(), MAX_PATH, target, 0))
    {
        result = "bad level folder";
        return false;
    }
    // The retail editor's default content, as its new level copies it.
    NewLevelContent content;
    const std::string defaults = "bf2editor/Content/Terrain/";
    const struct { const char* file; std::string* bytes; } kContent[] = {
        { "defaultcolor.dds", &content.colormap }, { "defaultdetail_1.dds", &content.detailmap1 },
        { "defaultdetail_2.dds", &content.detailmap2 }, { "defaultingamemap.dds", &content.ingameMap },
        { "defaultcommandermap.dds", &content.commanderMap },
        { "defaultcommandermicromap.dds", &content.commanderMicromap },
        { "DefaultShadowmap/SimpleShadowmap.raw", &content.shadowmap } };
    for (size_t i = 0; i < sizeof(kContent) / sizeof(kContent[0]); ++i)
    {
        if (!readFileBytes(defaults + kContent[i].file, *kContent[i].bytes))
        {
            result = "the retail editor's default terrain content is missing: " + defaults + kContent[i].file;
            return false;
        }
    }
    // The terrain LOD block, the same in every terraindata.raw of the 128-cell patch grid: the open level's.
    std::string bytes;
    TerrainRawFile lodSource;
    if (!readFileBytes(relative + "/terraindata.raw", bytes) || !lodSource.parse(bytes, error))
    {
        result = "the open level has no loose terraindata.raw to take the terrain LOD block from";
        return false;
    }
    std::vector<NewLevelFile> files;
    if (!buildNewLevel(settings, content, lodSource, files, error) || !writeNewLevel(target, files, error))
    {
        result = error;
        return false;
    }
    char text[320];
    std::snprintf(text, sizeof(text), "made %s/%s: %d x %d cells at %d m (a %d m level), flat at %d m, the surrounding "
                  "terrain %d x %d at %d m; %u files", parent.c_str(), name.c_str(), settings.size, settings.size,
                  settings.scale, settings.worldSize(), settings.defaultHeight, settings.lowResSize, settings.lowResSize,
                  settings.lowResScale(), static_cast<unsigned>(files.size()));
    result = text;
    logLine(LogLevel::Info, "new level: %s", result.c_str());
    if (open)
    {
        if (!m_canRelaunch)
        {
            result += "; opening it starts the editor again, which a hidden or batch run does not do (+loadLevel " + name
                      + ")";
            return true;
        }
        // On the new level's one gameplay layer.
        m_relaunchMode = "gpm_cq";
        m_relaunchSize = 16;
        m_relaunchLevel = name;
        m_relaunchMod.clear();
        result += "; the editor starts again on it (gpm_cq 16)";
    }
    else
        result += "; its one gameplay layer is gpm_cq 16 (+loadLevel " + name + " +gameMode gpm_cq opens it)";
    return true;
}

// The mod manager

bool EditSession::listModsCommand(std::string& result)
{
    const std::vector<std::string> mods = listMods("mods");
    result.clear();
    for (size_t i = 0; i < mods.size(); ++i)
        result += (i ? ", " : "") + mods[i];
    if (result.empty())
        result = "no mods (folders with a Mod.desc) under mods/";
    return true;
}

bool EditSession::createModCommand(const std::string& name, std::string& result)
{
    // Layered over bf2, as the retail editor's own version ("Battlefield2" -> bf2) layered its new mods.
    std::vector<std::string> bases(1, "bf2");
    if (!createMod("mods", name, bases, result))
        return false;
    result = "made mods/" + name + " (archive scripts over bf2, GameLogicInit.con, Init.con, " + name
           + ".mew, Ingame.mep, Mod.desc); EditorMod.open " + name + " starts the editor on it";
    logLine(LogLevel::Info, "mod: %s", result.c_str());
    return true;
}

bool EditSession::openMod(const std::string& name, std::string& result)
{
    const std::vector<std::string> mods = listMods("mods");
    bool known = false;
    for (size_t i = 0; i < mods.size(); ++i)
        known = known || _stricmp(mods[i].c_str(), name.c_str()) == 0;
    if (!known)
    {
        result = "no mod named " + name + " under mods/ (EditorMod.list)";
        return false;
    }
    if (!m_canRelaunch)
    {
        result = "this run cannot start the editor again (a hidden or batch run): refused";
        return false;
    }
    m_relaunchMode = m_level.gameMode();
    m_relaunchSize = m_level.layerSize();
    m_relaunchLevel.clear();
    m_relaunchMod = name;
    result = "the editor starts again on the mod " + name + " once it closes";
    return true;
}

} // namespace bf2editor
