// Code/Tools/BF2Editor/App/EditableTerrain.cpp - see EditableTerrain.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditableTerrain.h"

#include "App/TerrainLayerRecords.h"
#include "Framework/DdsImage.h"
#include "Framework/EditorConsole.h"
#include "Framework/EditorLog.h"
#include "Framework/TerrainLowDetail.h"
#include "Framework/TerrainRawCompile.h"
#include "Framework/TerrainRawLayers.h"
#include "Framework/TextureEncode.h"

#include "Core/ClassManager.h"
#include "Core/SettingsRepostitory.h"
#include "Core/SmartItf.h"
#include "Geom/ITerrainEditable.h"
#include "Physics/Heightmap.h"
#include "Physics/HeightmapCluster.h"
#include "IO/File/File.h"
#include "IO/File/FileManager.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <list>
#include <map>
#include <sstream>

namespace bf2editor
{

namespace
{

using dice::hfe::geom::ITerrainEditable;

std::string trimmed(const std::string& text)
{
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

std::string lowered(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

// A file as the engine's file manager reads it (the level's archives or its loose folder).
bool readEngineFile(const std::string& path, std::string& data)
{
    using namespace dice::hfe;
    if (!io::fileManager)
        return false;
    io::File file;
    if (!file.open(path, io::IFileSystem::FILEMODE_READ, static_cast<io::IFileSystem::FileAccess>(0), 0))
        return false;
    const int size = file.getSize();
    if (size < 0)
        return false;
    data.resize(static_cast<size_t>(size));
    return size == 0 || file.read(&data[0], static_cast<unsigned>(size)) == static_cast<unsigned>(size);
}

void makeFolders(const std::string& path)
{
    for (size_t i = 0; i < path.size(); ++i)
        if ((path[i] == '/' || path[i] == '\\') && i > 2)
            CreateDirectoryA(path.substr(0, i).c_str(), 0);
}

bool writeWhole(const std::string& path, const std::string& data)
{
    makeFolders(path);
    const HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    const BOOL ok = data.empty() || WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, 0);
    CloseHandle(file);
    return ok && written == data.size();
}

// The lines of Terrain.con's editor branch: after `if v_arg1 == BF2Editor`, up to its `else` or `endIf`.
std::vector<std::string> editorBranch(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    bool inside = false;
    int depth = 0;
    while (std::getline(in, line))
    {
        const std::string t = trimmed(line);
        std::string flat;
        for (size_t i = 0; i < t.size(); ++i)
            if (t[i] != ' ' && t[i] != '\t')
                flat += static_cast<char>(std::tolower(static_cast<unsigned char>(t[i])));
        if (!inside)
        {
            if (flat == "ifv_arg1==bf2editor")
                inside = true;
            continue;
        }
        if (flat.compare(0, 2, "if") == 0 && flat.find("==") != std::string::npos)
            ++depth;
        else if (flat == "else" || flat == "endif")
        {
            if (depth == 0)
                break;
            if (flat == "endif")
                --depth;
        }
        if (!t.empty() && lowered(t).compare(0, 3, "rem") != 0)
            lines.push_back(t);
    }
    return lines;
}

bool parseVec3(const std::string& text, dice::hfe::Vec3& v)
{
    float a = 0.0f, b = 0.0f, c = 0.0f;
    char tail = 0;
    if (std::sscanf(text.c_str(), "%f/%f/%f%c", &a, &b, &c, &tail) != 3)
        return false;
    v.x = a;
    v.y = b;
    v.z = c;
    return true;
}

std::string unquoted(const std::string& text)
{
    const std::string t = trimmed(text);
    if (t.size() >= 2 && t[0] == '"' && t[t.size() - 1] == '"')
        return t.substr(1, t.size() - 2);
    return t;
}

// The folder a base name lives in: "Levels/X/Colormaps/tx" -> "Levels/X/Colormaps/".
std::string folderOf(const std::string& baseName)
{
    const size_t slash = baseName.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : baseName.substr(0, slash + 1);
}

// The editor's staging folder under the game root (the working directory), one "terrain_<process id>" a session.
const char kStagingFolder[] = "bf2editor_temp";

void removeTree(const std::string& folder)
{
    WIN32_FIND_DATAA found;
    const HANDLE search = FindFirstFileA((folder + "/*").c_str(), &found);
    if (search != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string name = found.cFileName;
            if (name == "." || name == "..")
                continue;
            const std::string path = folder + "/" + name;
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                removeTree(path);
            else
                DeleteFileA(path.c_str());
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    RemoveDirectoryA(folder.c_str());
}

// The staging folders of editor sessions that have ended (their process is gone): only folders this code made.
void removeStaleStaging()
{
    WIN32_FIND_DATAA found;
    const HANDLE search = FindFirstFileA((std::string(kStagingFolder) + "/terrain_*").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;
        const unsigned long pid = std::strtoul(found.cFileName + 8, 0, 10);
        const HANDLE process = pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid) : 0;
        DWORD code = 0;
        const bool alive = process && GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
        if (process)
            CloseHandle(process);
        if (!alive && pid != GetCurrentProcessId())
            removeTree(std::string(kStagingFolder) + "/" + found.cFileName);
    } while (FindNextFileA(search, &found));
    FindClose(search);
}

// Every file directly in an engine folder (the level's archive or its loose folder), copied under `staging`.
int stageFolder(const std::string& folder, const std::string& staging, std::string& failed)
{
    using namespace dice::hfe;
    if (!io::fileManager || folder.empty())
        return 0;
    std::list<io::FileInfo> found;
    io::fileManager->findFiles(folder, false, found);
    int copied = 0;
    for (std::list<io::FileInfo>::const_iterator it = found.begin(); it != found.end(); ++it)
    {
        if (it->isDirectory || it->name.empty())
            continue;
        std::string name = it->name;
        const size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos)
            name = name.substr(slash + 1);
        std::string data;
        if (!readEngineFile(folder + name, data) || !writeWhole(staging + "/" + folder + name, data))
        {
            failed += (failed.empty() ? "" : ", ") + folder + name;
            continue;
        }
        ++copied;
    }
    return copied;
}

// The ring slot of a sector, as SurroundingPatchEditable's editor placement puts the slots: 0..2 the +z row from -x,
// 3 the +x side, 4..6 the -z row from +x, 7 the -x side. -1 for the primary tile and outside the 3x3.
int ringSlot(int x, int z)
{
    static const int kSlots[3][3] = { { 6, 5, 4 }, { 7, -1, 3 }, { 0, 1, 2 } };   // [z + 1][x + 1]
    return x < -1 || x > 1 || z < -1 || z > 1 ? -1 : kSlots[z + 1][x + 1];
}

// Whether the cluster holds a heightmap in sector x, z that is marked modified (Heightdata.con's setModified, or an
// edit since). The retail terrain save compiles such a sector into the ring even when it is not shown; a new level's
// surrounding heightmaps are not marked, so it has no ring.
bool sectorModified(int x, int z)
{
    using namespace dice::hfe;
    world::IHeightmapCluster* const cluster =
        classManager ? static_cast<world::IHeightmapCluster*>(classManager->getSingleton("HeightmapCluster")) : 0;
    const world::IHeightmap* const tile = cluster ? cluster->getHeightmapInSector(x, z) : 0;
    return tile && tile->getModified();
}

// The editable terrain's detail-map size: Terrain's constructor default, which nothing an editor branch sets changes.
const int kEditorDetailmapSize = 512;
// The game low-detail maps' size: the renderer's default low-detail settings, "-dxt1c -prescale 256 256 -Box"
// (TerrainEditable's constructor).
const int kLowDetailmapSize = 256;
// How closely recovered low-detail types must explain a level's low-detail maps: the mean difference of red and blue,
// of 255 [CHOSEN]. Stock Karkand's types explain its maps to 2.6, its compression's error.
const float kLowDetailFit = 10.0f;

bool readWholeFile(const std::string& path, std::string& data)
{
    const HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    const DWORD size = GetFileSize(file, 0);
    data.resize(size);
    DWORD read = 0;
    const BOOL ok = size == 0 || ReadFile(file, &data[0], size, &read, 0);
    CloseHandle(file);
    return ok && read == size;
}

// The .dds files of a folder by name, with their last-write times.
std::map<std::string, unsigned long long> ddsWriteTimes(const std::string& folder)
{
    std::map<std::string, unsigned long long> times;
    WIN32_FIND_DATAA found;
    const HANDLE search = FindFirstFileA((folder + "*.dds").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return times;
    do
    {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            times[found.cFileName] = (static_cast<unsigned long long>(found.ftLastWriteTime.dwHighDateTime) << 32)
                                   | found.ftLastWriteTime.dwLowDateTime;
    } while (FindNextFileA(search, &found));
    FindClose(search);
    return times;
}

bool fileExists(const std::string& path)
{
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// The patches read their colormaps and detail maps from the editor variants of the folders (the base names with
// "/Editor" inserted, QuadPatchEditable init): a patch takes paint only with an editor colormap, and its detail
// weights come from <editor detail base><XXxZZ>_1 / _2 (empty maps are made otherwise - a stock level then shows no
// detail). The level's own editor files are staged when it has them (a retail editor save keeps them in
// Levels/<level>/Editor/...); every other patch starts from a copy of its game file. `folder` is "/colormaps" or
// "/detailmaps" or "/lowdetailmaps"; `only`, when given, keeps the names holding it. Answers the editor files in place.
int stageEditorCopies(const std::string& base, const char* folder, const std::string& staging, std::string& failed,
                      const char* only = 0)
{
    const size_t at = lowered(base).find(folder);
    if (at == std::string::npos)
        return 0;
    const std::string& colormapBase = base;
    const std::string editorBase = colormapBase.substr(0, at) + "/Editor" + colormapBase.substr(at);
    const std::string gameFolder = folderOf(colormapBase);
    const std::string editorFolder = folderOf(editorBase);
    const std::string prefix = lowered(colormapBase.substr(gameFolder.size()));
    stageFolder(editorFolder, staging, failed);
    int present = 0;
    WIN32_FIND_DATAA found;
    const HANDLE search = FindFirstFileA((staging + "/" + gameFolder + "*.dds").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return 0;
    do
    {
        const std::string name = found.cFileName;
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || lowered(name).compare(0, prefix.size(), prefix) != 0
            || (only && lowered(name).find(only) == std::string::npos))
            continue;
        const std::string target = staging + "/" + editorFolder + name;
        if (!fileExists(target))
        {
            makeFolders(target);
            bool made = false;
            if (std::string(folder) == "/detailmaps")
            {
                // The editable patches read their detail maps as A8R8G8B8 of the terrain's detail-map size (512, the
                // renderer's default; QuadPatchEditable::importLayer ignores the pitch), while the game's are
                // compressed and smaller: converted and scaled here.
                std::string bytes, error;
                DdsImage image;
                if (readWholeFile(staging + "/" + gameFolder + name, bytes) && image.read(bytes, error))
                {
                    const DdsImage editor = image.width == kEditorDetailmapSize && image.height == kEditorDetailmapSize
                                          ? image : image.scaled(kEditorDetailmapSize, kEditorDetailmapSize);
                    made = writeWhole(target, editor.write());
                }
            }
            else
            {
                made = CopyFileA((staging + "/" + gameFolder + name).c_str(), target.c_str(), TRUE) != 0;
            }
            if (!made)
            {
                failed += (failed.empty() ? "" : ", ") + editorFolder + name;
                continue;
            }
        }
        ++present;
    } while (FindNextFileA(search, &found));
    FindClose(search);
    return present;
}

const char kSavePathKey[] = "Editor_LevelSavePath";

// Puts the staging folder in front of the engine's file paths for the lifetime of the object: relative writes land
// in it, reads find its files before the mod's, and the editor save-path setting names it (the atlas builder's
// folder). Both are restored on destruction.
class StagingRedirect
{
public:
    explicit StagingRedirect(const std::string& root)
        : m_path(root + "/")
        , m_previous(dice::hfe::settings->stringGet(kSavePathKey, std::string()))
    {
        dice::hfe::settings->stringSet(kSavePathKey, root);
        dice::hfe::io::fileManager->addPath(m_path);
    }
    ~StagingRedirect()
    {
        dice::hfe::io::fileManager->removePath(m_path);
        dice::hfe::settings->stringSet(kSavePathKey, m_previous);
    }

private:
    StagingRedirect(const StagingRedirect&);
    StagingRedirect& operator=(const StagingRedirect&);
    std::string m_path;
    std::string m_previous;
};

// The level's overgrowth folder as Overgrowth.path gives it, with '/' separators ("levels/<level>/overgrowth/").
std::string overgrowthFolder()
{
    std::string path = trimmed(runConsoleLine("Overgrowth.path").output);
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

// Copies the atlas sources listed by `folder`OvergrowthAtlas.tai from the engine's files to `root`/<source>, since
// the atlas builder reads plain files only. Returns the number staged; `missing` counts the sources the engine could
// not read.
int stageAtlasSources(const std::string& folder, const std::string& root, int& missing)
{
    missing = 0;
    std::string tai;
    if (!readEngineFile(folder + "OvergrowthAtlas.tai", tai))
        return 0;
    int staged = 0;
    std::istringstream in(tai);
    std::string line;
    while (std::getline(in, line))
    {
        const std::string entry = trimmed(line);
        if (entry.empty() || entry[0] == '#')
            continue;
        const std::string source = entry.substr(0, entry.find_first_of(" \t"));
        if (lowered(source).find(".dds") == std::string::npos)
            continue;
        const std::string target = root + "/" + source;
        std::string data;
        if (fileExists(target) || (readEngineFile(source, data) && writeWhole(target, data)))
            ++staged;
        else
            ++missing;
    }
    return staged;
}

// The lines of an Overgrowth.con that decide the atlas: the types and their geometries.
std::vector<std::string> overgrowthTypeLines(const std::string& text)
{
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
    {
        const std::string entry = trimmed(line);
        const std::string low = lowered(entry);
        if (low.compare(0, 18, "overgrowth.addtype") == 0 || low.compare(0, 23, "overgrowthtype.geometry") == 0)
            out.push_back(entry);
    }
    return out;
}

} // namespace

bool EditableTerrain::open(const std::string& level, std::string& report)
{
    using namespace dice::hfe;
    report.clear();
    if (m_editable)
    {
        report = "the terrain is editable already (TerrainEditable is made once a session)";
        return false;
    }
    if (!classManager || !settings)
    {
        report = "no class manager or settings repository";
        return false;
    }
    const std::string levelPath = "Levels/" + level + "/";
    std::string text;
    if (!readEngineFile(levelPath + "Terrain.con", text))
    {
        report = "the level has no Terrain.con the engine can read";
        return false;
    }
    const std::vector<std::string> lines = editorBranch(text);
    bool asksForEditable = false;
    for (size_t i = 0; i < lines.size(); ++i)
        asksForEditable = asksForEditable || lowered(lines[i]) == "terrain.create terraineditable";
    if (!asksForEditable)
    {
        report = levelPath + "Terrain.con has no editor branch that makes a TerrainEditable";
        return false;
    }

    // The staging copy of the terrain texture folders the base names name (init reads them there and writes a
    // missing detail map there): <game folder>/bf2editor_temp/terrain_<process id>, absolute - a texture save goes
    // through the file manager's write path, which puts a relative name under the mod (FileManager::getWritePath).
    // No dot anywhere in it: texture names come without an extension and the texture manager strips one at the
    // last dot (TextureManager::getTexture, pathStripExtension), so a dot in a folder name ("Name.DOMAIN") cuts the
    // name, nothing loads, and the shipped loader then writes through the missing fallback's null texture.
    removeStaleStaging();
    char directory[MAX_PATH] = "";
    GetCurrentDirectoryA(MAX_PATH, directory);
    std::string root = directory;
    for (size_t i = 0; i < root.size(); ++i)
        if (root[i] == '\\')
            root[i] = '/';
    if (root.find('.') != std::string::npos)
    {
        report = "the game folder's path (" + root + ") holds a dot: the renderer cuts texture names at a dot, so the "
                 "editable terrain's textures could not be read from a staging folder there";
        return false;
    }
    char stamp[64];
    std::snprintf(stamp, sizeof(stamp), "/%s/terrain_%lu", kStagingFolder, static_cast<unsigned long>(GetCurrentProcessId()));
    m_staging = root + stamp;
    std::vector<std::string> folders;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const std::string l = lowered(lines[i]);
        if (l.find("basename") == std::string::npos)
            continue;
        const size_t space = lines[i].find_first_of(" \t");
        if (space == std::string::npos)
            continue;
        const std::string folder = folderOf(unquoted(lines[i].substr(space + 1)));
        bool known = false;
        for (size_t k = 0; k < folders.size(); ++k)
            known = known || lowered(folders[k]) == lowered(folder);
        if (!known && !folder.empty())
            folders.push_back(folder);
    }
    int staged = 0;
    std::string failed;
    for (size_t i = 0; i < folders.size(); ++i)
        staged += stageFolder(folders[i], m_staging, failed);
    int editorDetailmaps = 0;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const size_t space = lines[i].find_first_of(" \t");
        const std::string command = space == std::string::npos ? std::string() : lowered(lines[i].substr(0, space));
        if (command == "terrain.colormapbasename")
            m_editorColormaps = stageEditorCopies(unquoted(lines[i].substr(space + 1)), "/colormaps", m_staging, failed);
        else if (command == "terrain.detailmapbasename")
            editorDetailmaps = stageEditorCopies(unquoted(lines[i].substr(space + 1)), "/detailmaps", m_staging, failed);
        else if (command == "terrain.lowdetailmapbasename")
            stageEditorCopies(unquoted(lines[i].substr(space + 1)), "/lowdetailmaps", m_staging, failed, "_s");
    }
    if (!failed.empty())
        logLine(LogLevel::Warning, "terrain: not staged: %s", failed.c_str());

    // The compiled terrain stays alive: the undergrowth system holds it from the load, and `terrain.create` releases
    // the singleton it replaces.
    IBase* const compiled = classManager->getSingleton("Terrain");
    if (compiled)
        compiled->addRef();
    const ConsoleLineResult created = runConsoleLine("terrain.create TerrainEditable");
    IBase* const made = classManager->getSingleton("Terrain");
    ITerrainEditable* const editable = made ? static_cast<ITerrainEditable*>(made->queryInterface(geom::kIID_ITerrainEditable)) : 0;
    if (!editable)
    {
        // Put the compiled terrain back as the game branch makes it.
        runConsoleLine("terrain.create Terrain");
        runConsoleLine("terrain.load " + levelPath + "terraindata.raw");
        report = "terrain.create TerrainEditable made no editable terrain"
               + (created.output.empty() ? std::string() : " (it answered '" + created.output + "')")
               + "; the compiled terrain was loaded again";
        return false;
    }

    // The settings, in the file's order; init with the staging folder as the save path.
    m_notApplied.clear();
    m_baseNames.clear();
    m_lightmapBase.clear();
    const auto rememberBase = [this](const std::string& name) -> std::string {
        m_baseNames.push_back(name);
        return name;
    };
    bool initialised = false;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const std::string& line = lines[i];
        const size_t space = line.find_first_of(" \t");
        const std::string command = lowered(space == std::string::npos ? line : line.substr(0, space));
        const std::string value = space == std::string::npos ? std::string() : trimmed(line.substr(space + 1));
        Vec3 v;
        if (command == "terrain.create")
            continue;
        else if (command == "terrain.patchsize")
            editable->setPatchSize(std::atoi(value.c_str()));
        else if (command == "terrain.subdividepatches")
            editable->setSubdividePatches(std::atoi(value.c_str()) != 0);
        else if (command == "terrain.primaryworldscale" && parseVec3(value, v))
            editable->setPrimaryWorldScale(v);
        else if (command == "terrain.secondaryworldscale" && parseVec3(value, v))
            editable->setSecondaryWorldScale(v);
        else if (command == "terrain.patchcolormapsize")
            editable->setPatchColormapSize(std::atoi(value.c_str()));
        else if (command == "terrain.lowdetailmapsize")
            editable->setLowDetailmapSize(std::atoi(value.c_str()));
        else if (command == "terrain.detailmapbasename")
            editable->setDetailmapBaseName(rememberBase(unquoted(value)));
        else if (command == "terrain.lowdetailmapbasename")
            editable->setLowDetailmapBaseName(rememberBase(unquoted(value)));
        else if (command == "terrain.colormapbasename")
            editable->setColormapBaseName(rememberBase(unquoted(value)));
        else if (command == "terrain.lightmapbasename")
        {
            m_lightmapBase = unquoted(value);
            editable->setLightmapBaseName(rememberBase(m_lightmapBase));
        }
        else if (command == "terrain.terrainwatercolor" && parseVec3(value, v))
            editable->setTerrainWaterColor(v);
        else if (command == "terrain.init")
        {
            const std::string key = "Editor_LevelSavePath";
            const std::string previous = settings->stringGet(key, std::string());
            settings->stringSet(key, m_staging);
            const ConsoleLineResult r = runConsoleLine("terrain.init");
            settings->stringSet(key, previous);
            if (!r.output.empty())
                logLine(LogLevel::Warning, "terrain: terrain.init answered '%s'", r.output.c_str());
            initialised = true;
        }
        else
            m_notApplied.push_back(line);
    }
    m_editable = editable;
    m_level = level;
    if (!initialised)
    {
        report = "Terrain.con's editor branch has no terrain.init: the editable terrain is made but not initialised";
        return false;
    }
    editable->setShaderTechnique(0);
    // Sky.con's terrain lines (terrain.sunColor, terrain.GIColor) lit the compiled terrain at load; the editable one that
    // replaced it takes them again (in retail's editor load it existed before Sky.con ran).
    int skyLines = 0;
    {
        std::string sky;
        if (readEngineFile(levelPath + "Sky.con", sky))
        {
            std::istringstream in(sky);
            std::string line;
            while (std::getline(in, line))
            {
                const std::string t = trimmed(line);
                if (lowered(t).compare(0, 8, "terrain.") != 0)
                    continue;
                const ConsoleLineResult r = runConsoleLine(t);
                if (!r.output.empty() || r.failed)
                    logLine(LogLevel::Warning, "terrain: Sky.con's '%s' answered '%s'", t.c_str(), r.output.c_str());
                ++skyLines;
            }
        }
    }
    logLine(LogLevel::Info, "terrain: %d Sky.con terrain line%s applied to the editable terrain", skyLines,
            skyLines == 1 ? "" : "s");
    // The surrounding sectors the game draws: those marked modified, which the terrain save compiles into the ring
    // (retail showed a sector with its activation).
    int ring = 0;
    for (int z = -1; z <= 1; ++z)
        for (int x = -1; x <= 1; ++x)
        {
            std::string ignored;
            if ((x || z) && sectorModified(x, z) && setSectorShown(x, z, true, ignored))
                ++ring;
        }
    logLine(LogLevel::Info, "terrain: %d surrounding sector%s shown", ring, ring == 1 ? "" : "s");
    loadGround(levelPath, lines);
    char summary[256];
    std::snprintf(summary, sizeof(summary), "TerrainEditable from %sTerrain.con's editor branch: %u texture files staged, %u "
                  "editor colormaps, %u editor detail maps, %u line%s not applied", levelPath.c_str(),
                  static_cast<unsigned>(staged), static_cast<unsigned>(m_editorColormaps),
                  static_cast<unsigned>(editorDetailmaps), static_cast<unsigned>(m_notApplied.size()),
                  m_notApplied.size() == 1 ? "" : "s");
    report = summary;
    for (size_t i = 0; i < m_notApplied.size(); ++i)
        report += (i ? "; " : " (") + m_notApplied[i] + (i + 1 == m_notApplied.size() ? ")" : "");
    return true;
}

bool EditableTerrain::setTechnique(int technique, std::string& error)
{
    if (!m_editable)
    {
        error = "the terrain is not editable (start with +editableTerrain 1)";
        return false;
    }
    // 8 (the material map) rebuilds the heightmap's materials from the layers' (the grid a stock level's layers lack):
    // refused. Above 8 the shipped code has no bound.
    if (technique < 0 || technique > 7)
    {
        error = "the terrain technique is 0 (colormap and lightmap), 1 (grid), 2 (topographic grid), 3 (detail layers), "
                "4 (undergrowth overlay), 5 (overgrowth), 6 (overgrowth shadow) or 7 (base pass)";
        return false;
    }
    // 5 and 6 export the level's overgrowth maps into the overlays; the shipped switch dereferences the overgrowth
    // without a test, and a missing map turns the look back to 0.
    if (technique == 5 || technique == 6)
    {
        std::string why;
        if (!overgrowthAvailable(why))
        {
            error = why;
            return false;
        }
    }
    m_editable->setShaderTechnique(technique);
    if (m_editable->getShaderTechnique() != technique)
    {
        error = "the overgrowth's maps are not loaded (the renderer turned the look back to 0)";
        return false;
    }
    return true;
}

bool EditableTerrain::paint(const TerrainBrushSettings& brush, float x, float z, float radius, bool secondary,
                            std::string& error)
{
    if (!m_editable)
    {
        error = "painting needs the editable terrain (start with +editableTerrain 1)";
        return false;
    }
    if (brush.layer < 0 || brush.layer > 5)
    {
        error = "no layer selected (the Terrain panel's layer, or EditorTerrain.brush layer 0..5)";
        return false;
    }
    const int amount = static_cast<int>(static_cast<float>(brush.strength) * 2.55f);
    const bool detail = brush.paintMode != TerrainPaintMode::Color;
    const bool colour = brush.paintMode != TerrainPaintMode::Detail;
    // The renderer paints nothing from a layer without a colour texture (and erases colour only through it).
    const std::map<int, GroundLayer>::const_iterator layerSettings = m_ground.layers.find(brush.layer);
    if (colour && (secondary || brush.colorFromTexture)
        && (layerSettings == m_ground.layers.end() || layerSettings->second.colorTexture.empty()))
    {
        error = "layer " + std::to_string(brush.layer) + " has no colour texture (Ground properties): painting colour "
                "from it, and erasing colour, need one";
        return false;
    }
    if (detail)
        m_editable->paintDetail(x, z, radius, secondary ? -amount : amount, brush.layer);
    if (colour)
    {
        if (secondary || brush.colorFromTexture)
            m_editable->paintColorFromTexture(x, z, radius, secondary ? -amount : amount, brush.layer);
        else
            m_editable->paintColor(x, z, radius, amount, brush.color);
    }
    // The patches the stroke reaches, by grid place - the renderer paints a quarter of the radius; one heightmap cell
    // more is kept in hand: the compile re-encodes their game textures.
    const float cell = m_editable->getPrimaryWorldScale().x;
    const float patchWorld = static_cast<float>(m_editable->getPatchSize()) * cell;
    const float reach = radius * 0.25f + cell;
    if (patchWorld > 0.0f)
    {
        const int x0 = static_cast<int>(std::floor((x - reach) / patchWorld));
        const int x1 = static_cast<int>(std::floor((x + reach) / patchWorld));
        const int z0 = static_cast<int>(std::floor((z - reach) / patchWorld));
        const int z1 = static_cast<int>(std::floor((z + reach) / patchWorld));
        for (int gz = z0; gz <= z1; ++gz)
        {
            for (int gx = x0; gx <= x1; ++gx)
            {
                if (detail)
                {
                    m_detailPainted.insert(std::make_pair(gx, gz));
                    m_detailUnmerged.insert(std::make_pair(gx, gz));
                }
                if (colour)
                    m_colorPainted.insert(std::make_pair(gx, gz));
            }
        }
    }
    m_painted = true;
    return true;
}

void EditableTerrain::markCompiled()
{
    m_detailPainted.clear();
    m_colorPainted.clear();
    if (m_lowDetailTypesKnown)
        for (int layer = 0; layer < 6; ++layer)
            m_compiledLowDetailTypes[layer] = lowDetailType(layer);
}

int EditableTerrain::lowDetailType(int layer) const
{
    const std::map<int, GroundLayer>::const_iterator it = m_ground.layers.find(layer);
    return it == m_ground.layers.end() ? 0 : it->second.lowDetailType;
}

bool EditableTerrain::mergeEdits(TerrainPaintMode mode, std::string& result)
{
    if (!m_editable)
    {
        result = "the terrain is not editable (start with +editableTerrain 1)";
        return false;
    }
    if (mode != TerrainPaintMode::Color)
    {
        m_editable->commitDetailLayers();
        m_detailUnmerged.clear();
    }
    if (mode != TerrainPaintMode::Detail)
        m_editable->commitColorMapEdits();
    result = std::string("merged the ") + (mode == TerrainPaintMode::Both ? "detail and colour" : mode == TerrainPaintMode::Detail
             ? "detail" : "colour") + " edits";
    return true;
}

bool EditableTerrain::clearEdits(TerrainPaintMode mode, std::string& result)
{
    if (!m_editable)
    {
        result = "the terrain is not editable (start with +editableTerrain 1)";
        return false;
    }
    if (mode != TerrainPaintMode::Color)
    {
        m_editable->revertDetailLayers();
        if (!m_detailUnmerged.empty())
        {
            m_detailPainted.insert(m_detailUnmerged.begin(), m_detailUnmerged.end());
            m_painted = true;
        }
        m_detailUnmerged.clear();
    }
    if (mode != TerrainPaintMode::Detail)
        m_editable->clearColorMapEdits();
    result = std::string("cleared the unmerged ") + (mode == TerrainPaintMode::Both ? "detail and colour"
             : mode == TerrainPaintMode::Detail ? "detail" : "colour") + " edits";
    return true;
}

bool EditableTerrain::saveMaps(std::vector<std::pair<std::string, std::string> >& files, std::string& error)
{
    using namespace dice::hfe;
    files.clear();
    if (!m_editable)
    {
        error = "the terrain is not editable";
        return false;
    }
    // The renderer finds each editor folder by inserting "/Editor" before the case-sensitive folder name; any other
    // spelling makes the editor-map saves overwrite the game maps.
    static const char* const kFolders[] = { "/Colormaps", "/Detailmaps", "/Lightmap", "/LowDetailmaps" };
    const std::string levelPath = "Levels/" + m_level + "/";
    std::vector<std::string> editorFolders;
    for (size_t i = 0; i < m_baseNames.size(); ++i)
    {
        size_t at = std::string::npos;
        for (size_t k = 0; k < sizeof(kFolders) / sizeof(kFolders[0]) && at == std::string::npos; ++k)
            at = m_baseNames[i].find(kFolders[k]);
        if (at == std::string::npos)
        {
            error = "the base name " + m_baseNames[i] + " is not spelled as the renderer expects (Colormaps, Detailmaps, "
                    "Lightmaps, LowDetailmaps): its editor maps would overwrite the game's";
            return false;
        }
        const std::string folder = folderOf(m_baseNames[i].substr(0, at) + "/Editor" + m_baseNames[i].substr(at));
        if (lowered(folder).compare(0, levelPath.size(), lowered(levelPath)) != 0)
        {
            error = "the base name " + m_baseNames[i] + " lies outside " + levelPath + ": its maps are not this level's";
            return false;
        }
        bool known = false;
        for (size_t k = 0; k < editorFolders.size(); ++k)
            known = known || editorFolders[k] == folder;
        if (!known)
            editorFolders.push_back(folder);
        makeFolders(m_staging + "/" + folder + "x");
    }

    // The primary heightmap's material file, as Heightdata.con loads it.
    std::string materialFile;
    {
        std::string text;
        readEngineFile(levelPath + "Heightdata.con", text);
        std::istringstream in(text);
        std::string line;
        bool primary = false;
        while (std::getline(in, line))
        {
            const std::string l = lowered(trimmed(line));
            if (l.compare(0, 29, "heightmapcluster.addheightmap") == 0)
                primary = l.find(" 0 0") != std::string::npos;
            else if (primary && materialFile.empty() && l.compare(0, 26, "heightmap.loadmaterialdata") == 0)
                materialFile = unquoted(trimmed(trimmed(line).substr(26)));
        }
    }

    const std::string key = "Editor_LevelSavePath";
    const std::string previous = settings->stringGet(key, std::string());
    settings->stringSet(key, m_staging);
    m_editable->save(levelPath.substr(0, levelPath.size() - 1), 2);
    m_editable->saveEditedColorMaps();
    settings->stringSet(key, previous);

    // The material grid back as the level's file has it.
    std::string materials;
    world::IHeightmapCluster* const cluster =
        classManager ? static_cast<world::IHeightmapCluster*>(classManager->getSingleton("HeightmapCluster")) : 0;
    world::IHeightmap* const heightmap = cluster ? cluster->getPrimaryHeightmap() : 0;
    if (heightmap && !materialFile.empty() && readEngineFile(materialFile, materials))
    {
        int sizeX = 0, sizeZ = 0;
        heightmap->getSize(sizeX, sizeZ);
        const int scale = heightmap->getMaterialScale();
        const size_t bytes = static_cast<size_t>(scale) * scale * sizeX * sizeZ;
        if (scale < 1 || materials.size() < bytes)
        {
            error = "the painted maps were written, but " + materialFile + " is too short to put the heightmap's materials "
                    "back: do not save the heightmap";
            return false;
        }
        for (int gz = 0; gz < sizeZ; ++gz)
            for (int gx = 0; gx < sizeX; ++gx)
                heightmap->setMaterialInGrid(gx - sizeX / 2, gz - sizeZ / 2,
                    static_cast<unsigned char>(materials[static_cast<size_t>(scale) * gz * scale * sizeX
                                                         + static_cast<size_t>(scale) * gx]));
    }

    // Every staged file under the level's Editor folders. A colormap the renderer loaded with a skipped mip level
    // (+skipMips) is saved at that lower resolution: refused rather than written over the level's.
    const int colormapSize = m_editable->getPatchColormapSize();
    for (size_t i = 0; i < editorFolders.size(); ++i)
    {
        WIN32_FIND_DATAA found;
        const HANDLE search = FindFirstFileA((m_staging + "/" + editorFolders[i] + "*").c_str(), &found);
        if (search == INVALID_HANDLE_VALUE)
            continue;
        const bool colormaps = lowered(editorFolders[i]).find("/editor/colormaps/") != std::string::npos;
        do
        {
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            const std::string staged = m_staging + "/" + editorFolders[i] + found.cFileName;
            std::string header;
            if (colormaps && readWholeFile(staged, header) && header.size() >= 20
                && *reinterpret_cast<const unsigned*>(header.data() + 16) < static_cast<unsigned>(colormapSize))
            {
                FindClose(search);
                files.clear();
                error = std::string(found.cFileName) + " was loaded below the terrain's colormap size (" +
                        std::to_string(colormapSize) + "; started with +skipMips?): saving it would lower its resolution";
                return false;
            }
            files.push_back(std::make_pair(staged, editorFolders[i].substr(levelPath.size()) + found.cFileName));
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    return true;
}

bool EditableTerrain::compileTerrain(std::vector<std::pair<std::string, std::string> >& files, std::string& result)
{
    using namespace dice::hfe;
    files.clear();
    if (!m_editable)
    {
        result = "the terrain is not editable (start with +editableTerrain 1)";
        return false;
    }
    world::IHeightmapCluster* const cluster =
        classManager ? static_cast<world::IHeightmapCluster*>(classManager->getSingleton("HeightmapCluster")) : 0;
    world::IHeightmap* const primary = cluster ? cluster->getPrimaryHeightmap() : 0;
    if (!primary)
    {
        result = "the level has no primary heightmap";
        return false;
    }
    const std::string levelPath = "Levels/" + m_level + "/";
    std::string bytes, error;
    TerrainRawFile templ;
    if (!readEngineFile(levelPath + "terraindata.raw", bytes) || !templ.parse(bytes, error))
    {
        result = "the level's terraindata.raw cannot be read" + (error.empty() ? std::string() : ": " + error);
        return false;
    }

    // The weights: the staged editor detail maps <detail base with /Editor><XXxZZ>_1 and _2; layer 3k + c is byte c of
    // map k's texels, as QuadPatchEditable::importLayer reads them. A patch without a map has no weight in its layers.
    std::string detailBase;
    for (size_t i = 0; i < m_baseNames.size() && detailBase.empty(); ++i)
    {
        const size_t at = m_baseNames[i].find("/Detailmaps");
        if (at != std::string::npos)
            detailBase = m_baseNames[i].substr(0, at) + "/Editor" + m_baseNames[i].substr(at);
    }
    if (detailBase.empty())
    {
        result = "the terrain has no detail map base name";
        return false;
    }
    const int half = static_cast<int>(templ.patchCount) / 2;
    std::vector<std::vector<unsigned char> > weights(templ.patches.size() * 6);
    int maps = 0;
    for (size_t p = 0; p < templ.patches.size(); ++p)
    {
        for (int k = 0; k < 2; ++k)
        {
            char name[32];
            std::snprintf(name, sizeof(name), "%02ix%02i_%i.dds", templ.patches[p].gridX + half, templ.patches[p].gridZ + half,
                          k + 1);
            std::string data;
            if (!readWholeFile(m_staging + "/" + detailBase + name, data))
                continue;
            DdsImage image;
            if (!image.read(data, error) || image.width != kEditorDetailmapSize || image.height != kEditorDetailmapSize)
            {
                result = detailBase + name + " is not a " + std::to_string(kEditorDetailmapSize) + "-square detail map" +
                         (error.empty() ? std::string() : ": " + error);
                return false;
            }
            ++maps;
            for (int c = 0; c < 3; ++c)
            {
                std::vector<unsigned char>& layer = weights[p * 6 + 3 * k + c];
                layer.resize(image.pixels.size());
                bool any = false;
                for (size_t t = 0; t < layer.size(); ++t)
                {
                    layer[t] = static_cast<unsigned char>(image.pixels[t] >> (8 * c));
                    any = any || layer[t] != 0;
                }
                if (!any)
                    layer.clear();
            }
        }
    }

    TerrainCompileSource source;
    source.height = [cluster](float x, float z) { return cluster->getHeightInWorldCoords(x, z); };
    source.seaWaterLevel = cluster->getSeaWaterLevel();
    int sizeX = 0, sizeZ = 0;
    primary->getSize(sizeX, sizeZ);
    source.heightmapSize = sizeX;
    source.weightSize = kEditorDetailmapSize;
    source.weights = [&weights](int patch, int layer) -> const unsigned char* {
        const std::vector<unsigned char>& w = weights[static_cast<size_t>(patch) * 6 + layer];
        return w.empty() ? 0 : &w[0];
    };
    // The ring: the surrounding sectors whose heightmap is marked modified, by ring slot (sector x, z). The retail save
    // also wrote the sectors whose surrounding patch existed; here showing a sector is the view only, and activating
    // it (EditorTerrain.sector) marks it.
    static const int kSectors[8][2] = { { -1, 1 }, { 0, 1 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, -1 }, { -1, -1 }, { -1, 0 } };
    for (int slot = 0; slot < 8; ++slot)
        if (sectorModified(kSectors[slot][0], kSectors[slot][1]))
            source.ringSlots.push_back(slot);
    // The ground properties' layers (the name as the compiled file holds it: no extension) and far terrain.
    source.replaceDetailTextures = true;
    source.detailTextures = templ.detailTextures;
    source.detailTextures.resize(6);
    for (std::map<int, GroundLayer>::const_iterator it = m_ground.layers.begin(); it != m_ground.layers.end(); ++it)
    {
        if (it->first < 0 || it->first > 5)
            continue;
        TerrainRawDetailTexture& texture = source.detailTextures[it->first];
        std::string name = it->second.detailTexture;
        if (name.size() > 4 && lowered(name.substr(name.size() - 4)) == ".dds")
            name.erase(name.size() - 4);
        texture.name = name;
        texture.planeMap = static_cast<unsigned char>(it->second.planeMap);
        texture.sideTiling[0] = it->second.sideTiling[0];
        texture.sideTiling[1] = it->second.sideTiling[1];
        texture.topTiling = it->second.topTiling;
        texture.yOffset = it->second.yOffset;
        texture.envMap = it->second.envMap ? 1 : 0;
    }
    source.replaceFar = true;
    source.farSideTiling[0] = m_ground.misc.farSideTiling[0];
    source.farSideTiling[1] = m_ground.misc.farSideTiling[1];
    source.farTopTilingHi = m_ground.misc.farTopTilingHi;
    source.farTopTilingLow = m_ground.misc.farTopTilingLow;
    source.farYOffset = m_ground.misc.farYOffset;

    TerrainRawFile compiled;
    if (!compileTerrainRaw(templ, source, compiled, error))
    {
        result = error;
        return false;
    }
    const std::string rawPath = m_staging + "/" + levelPath + "terraindata.raw";
    makeFolders(rawPath);
    if (!writeWhole(rawPath, compiled.render()))
    {
        result = "cannot write " + rawPath;
        return false;
    }
    files.push_back(std::make_pair(rawPath, std::string("terraindata.raw")));

    // The game textures of the painted patches; the others keep theirs. A detail map is written when one of its three
    // layers has triangles, R5G6B5 when the patch draws a layer above 2 and DXT1 otherwise, from the editor detail map
    // halved by the box filter, without mips (the retail writer's nvdxt settings). A colormap is the editor colormap as
    // DXT1 with the box-filtered mip chain [CHOSEN, not recovered: the retail editor rendered the patch top-down, its
    // roads drawn in, and compiled that with a Kaiser filter].
    std::string colormapBase, lowDetailBase;
    for (size_t i = 0; i < m_baseNames.size(); ++i)
    {
        if (colormapBase.empty() && m_baseNames[i].find("/Colormaps") != std::string::npos)
            colormapBase = m_baseNames[i];
        if (lowDetailBase.empty() && lowered(m_baseNames[i]).find("/lowdetailmaps") != std::string::npos)
            lowDetailBase = m_baseNames[i];
    }
    const std::string gameDetailBase = detailBase.substr(0, detailBase.find("/Editor")) +
                                       detailBase.substr(detailBase.find("/Editor") + 7);
    // The low-detail maps: QuadPatchEditable::rebuildLowDetailMap over the editor detail maps, halved by the box filter
    // to the renderer's default -prescale 256 256 and written as DXT1 with the mip chain (the retail writer's nvdxt
    // settings). Every patch's when a layer's low-detail type changed since the level's maps were built, else those of
    // the detail-painted patches; none while the types are unknown, so the level's stay.
    int types[6];
    bool typesChanged = false;
    for (int layer = 0; layer < 6; ++layer)
    {
        types[layer] = lowDetailType(layer);
        typesChanged = typesChanged || types[layer] != m_compiledLowDetailTypes[layer];
    }
    const bool lowDetail = m_lowDetailTypesKnown && !lowDetailBase.empty();
    const bool lowDetailAll = lowDetail && typesChanged;
    // The game textures are copied into the level by their path under it: a base name elsewhere is refused.
    const auto inLevel = [&levelPath](const std::string& name) {
        return name.empty() || lowered(name).compare(0, levelPath.size(), lowered(levelPath)) == 0;
    };
    const bool insideLevel = inLevel(gameDetailBase) && inLevel(colormapBase) && inLevel(lowDetailBase);
    if (!insideLevel && (!m_detailPainted.empty() || !m_colorPainted.empty() || lowDetailAll))
    {
        result = "the terrain's texture base names lie outside " + levelPath + ": its game textures are not this level's";
        return false;
    }
    int detailMaps = 0, colormaps = 0, lowDetailMaps = 0;
    for (size_t p = 0; p < compiled.patches.size(); ++p)
    {
        const std::pair<int, int> place(compiled.patches[p].gridX, compiled.patches[p].gridZ);
        char name[16];
        std::snprintf(name, sizeof(name), "%02ix%02i", place.first + half, place.second + half);
        if (m_detailPainted.count(place))
        {
            int highest = -1;
            bool needed[2] = { false, false };
            for (size_t l = 0; l < compiled.patches[p].layers.size(); ++l)
            {
                const unsigned index = compiled.patches[p].layers[l].detailIndex;
                if (index > 5)
                    continue;
                highest = static_cast<int>(index) > highest ? static_cast<int>(index) : highest;
                needed[index / 3] = true;
            }
            for (int k = 0; k < 2; ++k)
            {
                if (!needed[k])
                    continue;
                const std::string suffix = std::string(name) + "_" + std::to_string(k + 1) + ".dds";
                std::string data;
                DdsImage editor;
                if (!readWholeFile(m_staging + "/" + detailBase + suffix, data) || !editor.read(data, error))
                {
                    result = "the editor detail map " + detailBase + suffix + " cannot be read";
                    return false;
                }
                const DdsImage half256 = halveBox(editor);
                const std::string target = m_staging + "/" + gameDetailBase + suffix;
                makeFolders(target);
                if (!writeWhole(target, highest >= 3 ? encodeR5G6B5(half256) : encodeDxt1(half256, false)))
                {
                    result = "cannot write " + target;
                    return false;
                }
                files.push_back(std::make_pair(target, (gameDetailBase + suffix).substr(levelPath.size())));
                ++detailMaps;
            }
        }
        if (m_colorPainted.count(place) && !colormapBase.empty())
        {
            const size_t at = colormapBase.find("/Colormaps");
            const std::string editorColormap = colormapBase.substr(0, at) + "/Editor" + colormapBase.substr(at) + name + ".dds";
            std::string data;
            DdsImage editor;
            if (!readWholeFile(m_staging + "/" + editorColormap, data) || !editor.read(data, error))
            {
                result = "the editor colormap " + editorColormap + " cannot be read";
                return false;
            }
            const std::string target = m_staging + "/" + colormapBase + name + ".dds";
            makeFolders(target);
            if (!writeWhole(target, encodeDxt1(editor, true)))
            {
                result = "cannot write " + target;
                return false;
            }
            files.push_back(std::make_pair(target, (colormapBase + name + ".dds").substr(levelPath.size())));
            ++colormaps;
        }
        if (lowDetail && (lowDetailAll || m_detailPainted.count(place)))
        {
            DdsImage editor[2];
            const DdsImage* maps[2] = { 0, 0 };
            for (int k = 0; k < 2; ++k)
            {
                const std::string suffix = std::string(name) + "_" + std::to_string(k + 1) + ".dds";
                std::string data;
                if (!readWholeFile(m_staging + "/" + detailBase + suffix, data))
                    continue;                                   // a patch without the map: its layers weigh nothing
                if (!editor[k].read(data, error) || editor[k].width != kEditorDetailmapSize
                    || editor[k].height != kEditorDetailmapSize)
                {
                    result = "the editor detail map " + detailBase + suffix + " cannot be read";
                    return false;
                }
                maps[k] = &editor[k];
            }
            DdsImage low = buildLowDetailMap(maps, types, kEditorDetailmapSize);
            while (low.width > kLowDetailmapSize)
                low = halveBox(low);
            const std::string target = m_staging + "/" + lowDetailBase + name + ".dds";
            makeFolders(target);
            if (!writeWhole(target, encodeDxt1(low, true)))
            {
                result = "cannot write " + target;
                return false;
            }
            files.push_back(std::make_pair(target, (lowDetailBase + name + ".dds").substr(levelPath.size())));
            ++lowDetailMaps;
        }
    }

    size_t layers = 0;
    for (size_t p = 0; p < compiled.patches.size(); ++p)
        layers += compiled.patches[p].layers.size();
    const char* const lowDetailNote = lowDetailAll ? " (every patch's: a layer's low-detail type changed)"
                                    : !lowDetail && !m_detailPainted.empty()
                                    ? " (the level's kept: the layers' low-detail types are unknown)" : "";
    char text[400];
    std::snprintf(text, sizeof(text), "terrain compiled into the staging folder: %u patches (%u detail maps read), %u layer "
                  "records, %u surrounding sectors, %u culler nodes; %d game detail map%s and %d colormap%s of painted "
                  "patches, %d low-detail map%s%s", static_cast<unsigned>(compiled.patches.size()),
                  static_cast<unsigned>(maps), static_cast<unsigned>(layers), static_cast<unsigned>(compiled.ring.size()),
                  static_cast<unsigned>(compiled.culler.size()), detailMaps, detailMaps == 1 ? "" : "s", colormaps,
                  colormaps == 1 ? "" : "s", lowDetailMaps, lowDetailMaps == 1 ? "" : "s", lowDetailNote);
    result = text;
    return true;
}

bool EditableTerrain::overgrowthAvailable(std::string& why) const
{
    if (!m_editable)
    {
        why = "the terrain is not editable (start with +editableTerrain 1)";
        return false;
    }
    // The renderer's Overgrowth rows answer only while the level has an overgrowth.
    const ConsoleLineResult r = runConsoleLine("Overgrowth.path");
    if (r.failed || r.output.empty() || r.output.find("Unknown") != std::string::npos)
    {
        why = "this level has no overgrowth";
        return false;
    }
    return true;
}

std::vector<std::pair<std::string, int> > EditableTerrain::overgrowthMaterials() const
{
    // "tree1 [1]\pinebushes [2]"
    std::vector<std::pair<std::string, int> > out;
    std::string why;
    if (!overgrowthAvailable(why))
        return out;
    const std::string list = runConsoleLine("Overgrowth.getMaterialList").output;
    size_t start = 0;
    while (start < list.size())
    {
        size_t end = list.find('\\', start);
        if (end == std::string::npos)
            end = list.size();
        const std::string item = list.substr(start, end - start);
        const size_t open = item.rfind('[');
        if (open != std::string::npos && open > 0)
            out.push_back(std::make_pair(trimmed(item.substr(0, open)), std::atoi(item.c_str() + open + 1)));
        start = end + 1;
    }
    return out;
}

bool EditableTerrain::paintOvergrowth(const TerrainBrushSettings& brush, float x, float z, float radius, bool secondary,
                                      std::string& error)
{
    if (!overgrowthAvailable(error))
        return false;
    char line[160];
    if (brush.overgrowthShadow)
        std::snprintf(line, sizeof(line), "Overgrowth.paintShadowmap %f %f %f %d", x, z, radius,
                      secondary ? 0 : brush.overgrowthShade);
    else
        std::snprintf(line, sizeof(line), "Overgrowth.paintMaterial %f %f %f %d", x, z, radius,
                      secondary ? 0 : brush.overgrowthMaterial);
    if (m_lastOvergrowthCommand == line)
        return true;
    const ConsoleLineResult r = runConsoleLine(line);
    if (r.failed || !r.output.empty())
    {
        error = std::string(line) + " answered '" + r.output + "'";
        return false;
    }
    m_lastOvergrowthCommand = line;
    m_overgrowthPainted = true;
    return true;
}

bool EditableTerrain::generateOvergrowth(std::string& result)
{
    if (!overgrowthAvailable(result))
        return false;
    const std::string folder = overgrowthFolder();
    if (folder.empty() || folder[folder.size() - 1] != '/')
    {
        result = "the overgrowth's path '" + folder + "' is not a folder";
        return false;
    }
    const std::string root = m_staging + "/overgrowth";
    makeFolders(root + "/" + folder + "x");
    int missing = 0;
    const int sources = stageAtlasSources(folder, root, missing);
    {
        StagingRedirect redirect(root);
        runConsoleLine("Overgrowth.editorEnable 1");
        const ConsoleLineResult r = runConsoleLine("Overgrowth.generateAll");
        if (r.failed || !r.output.empty())
        {
            result = "Overgrowth.generateAll answered '" + r.output + "'";
            return false;
        }
    }
    if (!fileExists(root + "/" + folder + "Overgrowth.raw"))
    {
        result = "the regenerated overgrowth was not written to " + root + "/" + folder;
        return false;
    }
    char text[256];
    std::snprintf(text, sizeof(text), "overgrowth regenerated from its maps (%d atlas source%s staged%s); the level's "
                  "files change when it is saved", sources, sources == 1 ? "" : "s",
                  missing ? (", " + std::to_string(missing) + " unreadable").c_str() : "");
    result = text;
    return true;
}

bool EditableTerrain::saveOvergrowth(std::vector<std::pair<std::string, std::string> >& files, std::string& result)
{
    files.clear();
    if (!generateOvergrowth(result))
        return false;
    const std::string folder = overgrowthFolder();
    const std::string root = m_staging + "/overgrowth";
    {
        StagingRedirect redirect(root);
        const ConsoleLineResult r = runConsoleLine("Overgrowth.save");
        if (r.failed || !r.output.empty())
        {
            result = "Overgrowth.save answered '" + r.output + "'";
            return false;
        }
    }
    const std::string levelPrefix = "levels/" + lowered(m_level) + "/";
    if (lowered(folder).compare(0, levelPrefix.size(), levelPrefix) != 0)
    {
        result = "the overgrowth's path " + folder + " lies outside the level";
        return false;
    }
    const std::string inLevel = folder.substr(levelPrefix.size());
    static const char* const kData[] = { "Overgrowth.con", "Overgrowth.raw", "OvergrowthShadowmap.raw",
                                         "OvergrowthCollision.con" };
    for (size_t i = 0; i < sizeof(kData) / sizeof(kData[0]); ++i)
    {
        const std::string staged = root + "/" + folder + kData[i];
        if (!fileExists(staged))
        {
            result = std::string("the overgrowth save did not write ") + kData[i];
            files.clear();
            return false;
        }
        files.push_back(std::make_pair(staged, inLevel + kData[i]));
    }
    // The atlas only follows a change of types: a rebuilt atlas of the same textures is left out of the level.
    std::string stagedCon, levelCon;
    readWholeFile(root + "/" + folder + "Overgrowth.con", stagedCon);
    readEngineFile(folder + "Overgrowth.con", levelCon);
    const bool typesChanged = overgrowthTypeLines(stagedCon) != overgrowthTypeLines(levelCon);
    if (typesChanged)
    {
        // The builder writes its output path into the .tai's first (comment) line; the stock form names the mod's
        // folder instead, so no local staging path reaches the level.
        std::string tai;
        const std::string taiPath = root + "/" + folder + "OvergrowthAtlas.tai";
        if (readWholeFile(taiPath, tai) && !tai.empty() && tai[0] == '#')
        {
            const std::string mod = dice::hfe::settings->stringGet("GSModDirectory", std::string());
            std::string stockFolder = folder;
            std::replace(stockFolder.begin(), stockFolder.end(), '/', '\\');
            size_t end = tai.find('\n');
            if (end != std::string::npos && end > 0 && tai[end - 1] == '\r')
                --end;
            tai = "# " + mod + stockFolder + "OvergrowthAtlas.tai" + (end == std::string::npos ? std::string() : tai.substr(end));
            writeWhole(taiPath, tai);
        }
        WIN32_FIND_DATAA found;
        const HANDLE search = FindFirstFileA((root + "/" + folder + "OvergrowthAtlas*").c_str(), &found);
        if (search != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    files.push_back(std::make_pair(root + "/" + folder + found.cFileName, inLevel + found.cFileName));
            } while (FindNextFileA(search, &found));
            FindClose(search);
        }
    }
    result = typesChanged ? "the atlas rebuilt for the changed types" : "the atlas kept: the types are unchanged";
    return true;
}

bool EditableTerrain::traceLightmaps(bool primary, bool surrounding,
                                     std::vector<std::pair<std::string, std::string> >& files, std::string& result)
{
    using namespace dice::hfe;
    files.clear();
    if (!m_editable)
    {
        result = "the terrain is not editable (start with +editableTerrain 1)";
        return false;
    }
    const std::string levelPrefix = "levels/" + lowered(m_level) + "/";
    if (m_lightmapBase.empty() || lowered(m_lightmapBase).compare(0, levelPrefix.size(), levelPrefix) != 0)
    {
        result = "Terrain.con's lightmap base name does not lie in the level (" + m_lightmapBase + ")";
        return false;
    }
    // The generator writes the patches' lightmaps under the lightmap base and the ring patches' under its editor
    // variant ("/Editor" before "/Lightmap", as the renderer inserts it).
    std::vector<std::string> folders(1, folderOf(m_lightmapBase));
    const size_t at = m_lightmapBase.find("/Lightmap");
    if (at != std::string::npos)
        folders.push_back(folderOf(m_lightmapBase.substr(0, at) + "/Editor" + m_lightmapBase.substr(at)));
    std::vector<std::map<std::string, unsigned long long> > before;
    for (size_t i = 0; i < folders.size(); ++i)
        before.push_back(ddsWriteTimes(m_staging + "/" + folders[i]));
    const ConsoleLineResult primaryWas = runConsoleLine("terrain.drawPrimaryLightmaps");
    const ConsoleLineResult surroundingWas = runConsoleLine("terrain.drawSurroundingLightmaps");
    if (primaryWas.failed || surroundingWas.failed)
    {
        result = "the renderer binds no terrain.drawPrimaryLightmaps / drawSurroundingLightmaps (the editor's renderer "
                 "flavour does): " + primaryWas.output + surroundingWas.output;
        return false;
    }
    runConsoleLine(std::string("terrain.drawPrimaryLightmaps ") + (primary ? "1" : "0"));
    runConsoleLine(std::string("terrain.drawSurroundingLightmaps ") + (surrounding ? "1" : "0"));
    const std::string previous = settings->stringGet(kSavePathKey, std::string());
    settings->stringSet(kSavePathKey, m_staging);
    const ConsoleLineResult traced = runConsoleLine("lightmapGeneration.traceTerrain");
    // The patches read their lightmaps again from the save path, the staging folder.
    m_editable->rebuildTerrain();
    settings->stringSet(kSavePathKey, previous);
    runConsoleLine("terrain.drawPrimaryLightmaps " + trimmed(primaryWas.output));
    runConsoleLine("terrain.drawSurroundingLightmaps " + trimmed(surroundingWas.output));
    if (traced.failed || trimmed(traced.output) == "0")
    {
        result = "lightmapGeneration.traceTerrain answered '" + traced.output + "'";
        return false;
    }
    for (size_t i = 0; i < folders.size(); ++i)
    {
        const std::map<std::string, unsigned long long> after = ddsWriteTimes(m_staging + "/" + folders[i]);
        for (std::map<std::string, unsigned long long>::const_iterator it = after.begin(); it != after.end(); ++it)
        {
            const std::map<std::string, unsigned long long>::const_iterator old = before[i].find(it->first);
            if (old == before[i].end() || old->second != it->second)
                files.push_back(std::make_pair(m_staging + "/" + folders[i] + it->first,
                                               folders[i].substr(levelPrefix.size()) + it->first));
        }
    }
    if (files.empty())
    {
        result = "lightmapGeneration.traceTerrain wrote no lightmap";
        return false;
    }
    return true;
}

bool EditableTerrain::setSectorShown(int x, int z, bool shown, std::string& result)
{
    using namespace dice::hfe;
    if (!m_editable)
    {
        result = "the terrain is not editable (start with +editableTerrain 1)";
        return false;
    }
    const int slot = ringSlot(x, z);
    if (slot < 0)
    {
        result = "a surrounding sector is x, z in -1..1 (0, 0 is the primary tile)";
        return false;
    }
    world::IHeightmapCluster* const cluster =
        classManager ? static_cast<world::IHeightmapCluster*>(classManager->getSingleton("HeightmapCluster")) : 0;
    if (!cluster || !cluster->getHeightmapInSector(x, z))
    {
        result = "no tile in sector " + std::to_string(x) + "," + std::to_string(z);
        return false;
    }
    char text[96];
    std::snprintf(text, sizeof(text), "sector %d,%d (ring slot %d) %s", x, z, slot, shown ? "shown" : "hidden");
    result = text;
    if (m_sectorShown[slot] == shown)
        return true;
    // The patch reads its textures from <Editor_LevelSavePath><editor base>_s<slot>: the staging folder for the call.
    const std::string key = "Editor_LevelSavePath";
    const std::string previous = settings->stringGet(key, std::string());
    settings->stringSet(key, m_staging);
    if (shown)
        m_editable->createSurroundingPatch(slot);
    else
        m_editable->removeSurroundingPatch(slot);
    settings->stringSet(key, previous);
    m_sectorShown[slot] = shown;
    return true;
}

bool EditableTerrain::sectorShown(int x, int z) const
{
    const int slot = ringSlot(x, z);
    return slot >= 0 && m_sectorShown[slot];
}

void EditableTerrain::loadGround(const std::string& levelPath, const std::vector<std::string>& lines)
{
    m_ground = GroundProperties();
    std::string text, error;
    std::string raw, rawError;
    TerrainRawLayers compiled;
    const bool haveCompiled = readEngineFile(levelPath + "terraindata.raw", raw) && compiled.read(raw, rawError);
    const std::string file = levelPath + "Editor/GroundProperties.con";
    m_lowDetailTypesKnown = false;
    if (readEngineFile(file, text) && m_ground.parse(text, error))
    {
        m_groundSource = file;
        m_lowDetailTypesKnown = true;
    }
    else
    {
        if (!text.empty())
            logLine(LogLevel::Warning, "terrain: %s: %s - the compiled terrain's layers are used instead", file.c_str(),
                    error.c_str());
        m_ground = GroundProperties();
        // The terrain's own settings as the editor branch gives them (what the retail editor wrote back), the LOD
        // distance from the renderer.
        for (size_t i = 0; i < lines.size(); ++i)
        {
            const size_t space = lines[i].find_first_of(" \t");
            if (space == std::string::npos)
                continue;
            const std::string command = lowered(lines[i].substr(0, space));
            const std::string value = trimmed(lines[i].substr(space + 1));
            std::string ignored;
            if (command == "terrain.farsidetiling" || command == "terrain.fartoptilinghi"
                || command == "terrain.fartoptilinglow" || command == "terrain.faryoffset")
                m_ground.setMisc(command.substr(8), value, ignored);
        }
        m_ground.misc.lodSwitchDistance = m_editable->getLodSwitchDistance();
        if (haveCompiled)
        {
            for (size_t i = 0; i < compiled.layers.size() && i < 6; ++i)
            {
                const TerrainRawLayer& c = compiled.layers[i];
                GroundLayer layer;
                if (!c.name.empty())
                    layer.detailTexture = c.name + ".dds";
                layer.planeMap = c.planeMap;
                layer.sideTiling[0] = c.sideTiling[0];
                layer.sideTiling[1] = c.sideTiling[1];
                layer.topTiling = c.topTiling;
                layer.yOffset = c.yOffset;
                layer.envMap = c.envMap;
                m_ground.layers[static_cast<int>(i)] = layer;
            }
            m_groundSource = levelPath + "terraindata.raw (the compiled terrain's layers)";
        }
        else
        {
            m_groundSource = "none (no Editor/GroundProperties.con, and terraindata.raw: " + rawError + ")";
        }
    }
    bool anyType = false;
    for (std::map<int, GroundLayer>::const_iterator it = m_ground.layers.begin(); it != m_ground.layers.end(); ++it)
        anyType = anyType || it->second.lowDetailType != 0;
    if (!anyType && haveCompiled && recoverLowDetailTypes(compiled.patchCount))
        m_lowDetailTypesKnown = true;
    for (int layer = 0; layer < 6; ++layer)
        m_compiledLowDetailTypes[layer] = lowDetailType(layer);
    m_groundSaved = m_ground;
    std::string notApplied;
    for (std::map<int, GroundLayer>::const_iterator it = m_ground.layers.begin(); it != m_ground.layers.end(); ++it)
    {
        const std::string missing = applyGroundLayer(m_editable, it->first, it->second);
        if (!missing.empty())
            notApplied += (notApplied.empty() ? "" : "; ") + std::string("layer ") + std::to_string(it->first) + ": " + missing;
    }
    m_editable->setLodSwitchDistance(m_ground.misc.lodSwitchDistance);
    logLine(LogLevel::Info, "terrain: ground properties from %s: %u layer%s%s%s", m_groundSource.c_str(),
            static_cast<unsigned>(m_ground.layers.size()), m_ground.layers.size() == 1 ? "" : "s",
            notApplied.empty() ? "" : ", not applied: ", notApplied.c_str());
}

// [added] The compiled terrain holds no low-detail types, and no stock level ships its ground properties: the types are
// recovered from the level's game detail and low-detail maps (Framework/TerrainLowDetail), and taken when they explain
// the low-detail maps to within kLowDetailFit.
bool EditableTerrain::recoverLowDetailTypes(unsigned patchCount)
{
    std::string detailBase, lowDetailBase;
    for (size_t i = 0; i < m_baseNames.size(); ++i)
    {
        const std::string name = lowered(m_baseNames[i]);
        if (lowDetailBase.empty() && name.find("/lowdetailmaps") != std::string::npos)
            lowDetailBase = m_baseNames[i];
        else if (detailBase.empty() && name.find("/detailmaps") != std::string::npos)
            detailBase = m_baseNames[i];
    }
    if (detailBase.empty() || lowDetailBase.empty() || patchCount == 0 || patchCount > 64)
    {
        logLine(LogLevel::Warning, "terrain: the layers' low-detail types are unknown (no detail or low-detail base name, "
                "or %u patches per side): saving keeps the level's low-detail maps", patchCount);
        return false;
    }
    std::deque<DdsImage> images;
    std::vector<LowDetailSample> samples;
    for (unsigned z = 0; z < patchCount; ++z)
    {
        for (unsigned x = 0; x < patchCount; ++x)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "%02ux%02u", x, z);
            std::string data, error;
            DdsImage image;
            if (!readEngineFile(lowDetailBase + name + ".dds", data) || !image.read(data, error))
                continue;
            LowDetailSample sample;
            images.push_back(image);
            sample.lowDetail = &images.back();
            for (int k = 0; k < 2; ++k)
            {
                if (readEngineFile(detailBase + name + "_" + std::to_string(k + 1) + ".dds", data) && image.read(data, error))
                {
                    images.push_back(image);
                    sample.detail[k] = &images.back();
                }
            }
            samples.push_back(sample);
        }
    }
    int types[6] = { 0, 0, 0, 0, 0, 0 };
    float error = 0.0f;
    if (!inferLowDetailTypes(samples, types, error))
    {
        logLine(LogLevel::Warning, "terrain: the layers' low-detail types are unknown (no patch has both a low-detail map "
                "and a detail map): saving keeps the level's low-detail maps");
        return false;
    }
    char list[32];
    std::snprintf(list, sizeof(list), "%d %d %d %d %d %d", types[0], types[1], types[2], types[3], types[4], types[5]);
    if (error > kLowDetailFit)
    {
        logLine(LogLevel::Warning, "terrain: the layers' low-detail types are unknown (the closest, %s, miss the level's "
                "low-detail maps by %.1f of 255): saving keeps the level's low-detail maps", list, error);
        return false;
    }
    for (std::map<int, GroundLayer>::iterator it = m_ground.layers.begin(); it != m_ground.layers.end(); ++it)
        if (it->first >= 0 && it->first < 6)
            it->second.lowDetailType = types[it->first];
    logLine(LogLevel::Info, "terrain: the layers' low-detail types recovered from the level's compiled maps (%u patches): "
            "%s, within %.1f of 255", static_cast<unsigned>(samples.size()), list, error);
    return true;
}

bool EditableTerrain::setGround(int layer, const std::string& name, const std::string& value, std::string& result)
{
    if (!m_editable)
    {
        result = "the terrain is not editable (start with +editableTerrain 1)";
        return false;
    }
    std::string error;
    if (layer < 0)
    {
        if (!m_ground.setMisc(name, value, error))
        {
            result = error;
            return false;
        }
        m_editable->setLodSwitchDistance(m_ground.misc.lodSwitchDistance);
        std::string now;
        m_ground.getMisc(name, now);
        result = name + " " + now + (lowered(name) == "lodswitchdistance" ? std::string()
                 : std::string(" (kept in GroundProperties.con: the renderer's far tiling has no host interface)"));
        return true;
    }
    if (!m_ground.setLayer(layer, name, value, error))
    {
        result = error;
        return false;
    }
    if (lowered(name) == "lowdetailtype")
        m_lowDetailTypesKnown = true;
    const std::string missing = applyGroundLayer(m_editable, layer, m_ground.layers[layer]);
    std::string now;
    m_ground.getLayer(layer, name, now);
    result = "layer " + std::to_string(layer) + " " + name + " " + now;
    if (!missing.empty())
        result += " (kept in GroundProperties.con; not applied to the renderer yet: " + missing + ")";
    return true;
}

bool EditableTerrain::removeGroundLayer(int layer, std::string& result)
{
    if (!m_ground.layers.erase(layer))
    {
        result = "layer " + std::to_string(layer) + " is not in the ground properties";
        return false;
    }
    result = "layer " + std::to_string(layer) + " taken out of the ground properties";
    return true;
}

int EditableTerrain::technique() const
{
    return m_editable ? m_editable->getShaderTechnique() : -1;
}

std::string EditableTerrain::describe() const
{
    if (!m_editable)
        return "the compiled terrain (start with +editableTerrain 1 for the editable one)";
    char text[200];
    const dice::hfe::Vec3& scale = m_editable->getPrimaryWorldScale();
    std::snprintf(text, sizeof(text), "TerrainEditable: patch size %d, colormap size %d, primary scale %g/%g/%g, technique %d",
                  m_editable->getPatchSize(), m_editable->getPatchColormapSize(), scale.x, scale.y, scale.z,
                  m_editable->getShaderTechnique());
    return text;
}

} // namespace bf2editor
