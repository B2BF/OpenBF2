// Code/Tools/BF2Editor/App/EditorLevel.cpp - see EditorLevel.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditorLevel.h"

#include "App/EditorGame.h"
#include "Framework/EditorLog.h"

#include "Core/ClassManager.h"
#include "Core/EventManager.h"
#include "Core/Ids.h"
#include "Core/LoadStat.h"
#include "Core/SettingsRepostitory.h"
#include "Core/SmartItf.h"
#include "Game/IGame.h"
#include "Game/IGameLogic.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/IObjectTemplate.h"
#include "IO/Console/Console.h"
#include "IO/File/File.h"
#include "IO/File/FileManager.h"
#include "Scene/Object/ObjectManager.h"
#include "Physics/Heightmap.h"
#include "Physics/HeightmapCluster.h"

#include <windows.h>

#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace bf2editor
{

using namespace dice::hfe;

namespace
{
// ECCore event ids the game's load posters use (GameServer::loadPath, GameClient::load).
const unsigned int kPreLoad = 2;            // CEPreLoad: GameLogicInit.con, RendDX9's shader archive
const unsigned int kPostLoad = 3;           // CEPostLoad: nametags, animations, sounds, env maps
const unsigned int kResourcesLoaded = 7;    // CEResourcesLoaded: load stats end, water meshes

bool readEngineText(const std::string& path, std::string& text)
{
    if (!io::fileManager)
        return false;
    io::File file;
    if (!file.open(path, io::IFileSystem::FILEMODE_READ, static_cast<io::IFileSystem::FileAccess>(0), 0))
        return false;
    const int size = file.getSize();
    if (size < 0)
        return false;
    text.assign(static_cast<size_t>(size), '\0');
    return size == 0 || file.read(&text[0], static_cast<unsigned>(size)) == static_cast<unsigned>(size);
}

std::string trimmedLower(const std::string& line)
{
    size_t begin = 0, end = line.size();
    while (begin < end && (line[begin] == ' ' || line[begin] == '\t'))
        ++begin;
    while (end > begin && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r'))
        --end;
    std::string out = line.substr(begin, end - begin);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

// The lines inside every top-level `if v_arg1 == <argument>` ... `endIf` block of a .con text, in order.
// A block with a conditional of its own inside is left out and counted in `refused`.
std::vector<std::string> blockLines(const std::string& text, const std::string& argument, int& refused)
{
    std::vector<std::string> lines, block;
    std::string compact = "ifv_arg1==" + argument;
    for (size_t i = 0; i < compact.size(); ++i)
        compact[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(compact[i])));
    int depth = 0;                  // conditionals open around the current line
    bool inside = false;            // inside a wanted block (at depth 1)
    bool nested = false;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        const std::string line = text.substr(start, end - start);
        start = end + 1;
        const std::string low = trimmedLower(line);
        std::string squeezed;
        for (size_t k = 0; k < low.size(); ++k)
            if (low[k] != ' ' && low[k] != '\t')
                squeezed += low[k];
        if (low.compare(0, 3, "if ") == 0)
        {
            ++depth;
            if (depth == 1 && squeezed == compact)
            {
                inside = true;
                nested = false;
                block.clear();
                continue;
            }
            if (inside)
                nested = true;
        }
        else if (low == "endif")
        {
            if (depth == 1 && inside)
            {
                if (nested)
                    ++refused;
                else
                    lines.insert(lines.end(), block.begin(), block.end());
                inside = false;
            }
            if (depth > 0)
                --depth;
            continue;
        }
        else if (low == "else" && depth == 1 && inside)
        {
            nested = true;              // an else of the block itself: not a plain block either
        }
        if (inside && !low.empty() && low.compare(0, 3, "rem") != 0)
            block.push_back(line);
        if (end == text.size())
            break;
    }
    return lines;
}

// What running a block did: the objects it asked for and made, the templates it could not make
// (Object.create answers "NoObject"; a made object answers "id<n>"), and the other lines that answered
// with text - lines after a failed create are not counted, they had no object to act on.
struct BlockRun
{
    int                      asked;
    int                      made;
    std::vector<std::string> unknown;
    int                      complaints;
    std::string              firstComplaint;
    BlockRun() : asked(0), made(0), complaints(0) {}
};

BlockRun runEngineLines(const std::vector<std::string>& lines)
{
    BlockRun run;
    bool objectFailed = false;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::string answer;
        io::mainConsole->executeLine(lines[i] + "\n", answer, false, false, false, false, true);
        const std::string low = trimmedLower(lines[i]);
        const size_t first = answer.find_first_not_of(" \t\r\n");
        const std::string said = first == std::string::npos ? std::string() : answer.substr(first);
        if (low.compare(0, 14, "object.create ") == 0)
        {
            ++run.asked;
            objectFailed = said.compare(0, 2, "id") != 0;
            if (objectFailed)
                run.unknown.push_back(trimmedLower(low.substr(14)));
            else
                ++run.made;
        }
        else if (!said.empty() && !(objectFailed && low.compare(0, 7, "object.") == 0))
        {
            if (run.complaints++ == 0)
                run.firstComplaint = lines[i] + " -> " + said;
        }
    }
    return run;
}

// One object of an overgrowth collision script as Overgrowth.save writes it: `Object.create <template>`, then
// `Object.absoluteTransformation [..][..][..][x/y/z/1]`, whose last row is the position.
struct CollisionRecord
{
    std::string templateName;   // lower case
    float       position[3];
    bool        placed;
};

std::vector<CollisionRecord> parseCollisionScript(const std::string& text)
{
    std::vector<CollisionRecord> records;
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        const std::string line = trimmedLower(text.substr(start, end - start));
        start = end + 1;
        if (line.compare(0, 14, "object.create ") == 0)
        {
            CollisionRecord record;
            record.templateName = trimmedLower(line.substr(14));
            record.position[0] = record.position[1] = record.position[2] = 0.0f;
            record.placed = false;
            records.push_back(record);
        }
        else if (line.compare(0, 30, "object.absolutetransformation ") == 0 && !records.empty())
        {
            const size_t row = line.rfind('[');
            CollisionRecord& record = records.back();
            record.placed = row != std::string::npos
                && std::sscanf(line.c_str() + row, "[%f/%f/%f/", &record.position[0], &record.position[1],
                               &record.position[2]) == 3;
        }
    }
    return records;
}

// Removes the objects a level's Overgrowth/OvergrowthCollision.con made: each record takes the nearest loaded
// object marked isOvergrowth with its template within 0.01 m, the document's binding tolerance. `records`
// receives the script's record count; returns the number of objects removed.
int removeOvergrowthCollisionObjects(const std::string& levelPath, int& records)
{
    records = 0;
    std::string text;
    if (!world::objectManager || !readEngineText(levelPath + "Overgrowth/OvergrowthCollision.con", text))
        return 0;
    const std::vector<CollisionRecord> script = parseCollisionScript(text);
    records = static_cast<int>(script.size());

    const std::map<unsigned int, world::IObject*>& roots = world::objectManager->getAllObjectsFromGrid();
    std::vector<world::IObject*> candidates;
    std::map<std::string, std::vector<size_t> > byTemplate;
    for (std::map<unsigned int, world::IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
    {
        world::IObject* const object = it->second;
        if (!object || !object->getTemplate() || !object->getIsOvergrowth())
            continue;
        byTemplate[trimmedLower(object->getTemplate()->getName())].push_back(candidates.size());
        candidates.push_back(object);
    }

    // Collected first: removal changes the map the candidates came from.
    std::vector<bool> taken(candidates.size(), false);
    std::vector<world::IObject*> doomed;
    for (size_t r = 0; r < script.size(); ++r)
    {
        std::map<std::string, std::vector<size_t> >::const_iterator group = byTemplate.find(script[r].templateName);
        if (!script[r].placed || group == byTemplate.end())
            continue;
        size_t best = candidates.size();
        float bestDistance = 0.01f * 0.01f;
        for (size_t c = 0; c < group->second.size(); ++c)
        {
            const size_t index = group->second[c];
            if (taken[index])
                continue;
            const Mat4& m = candidates[index]->getAbsoluteTransformation();
            const float dx = m.m[12] - script[r].position[0];
            const float dy = m.m[13] - script[r].position[1];
            const float dz = m.m[14] - script[r].position[2];
            const float distance = dx * dx + dy * dy + dz * dz;
            if (distance <= bestDistance)
            {
                bestDistance = distance;
                best = index;
            }
        }
        if (best < candidates.size())
        {
            taken[best] = true;
            doomed.push_back(candidates[best]);
        }
    }
    // The engine's own removal, as the editor's delete does it: forceDestroy, then the destroy list drained.
    for (size_t i = 0; i < doomed.size(); ++i)
        doomed[i]->forceDestroy();
    if (!doomed.empty())
        world::objectManager->destroyObjects();
    return static_cast<int>(doomed.size());
}
}

EditorLevel::EditorLevel()
    : m_game(0)
    , m_open(false)
    , m_maxPlayers(0)
    , m_layerSize(0)
{
}

// Leaving the editor does not close the level: while the renderer is up the editor exits the way
// BF2.exe does (EditorApp::run), which never unloads a level either, and GameLogic::unload's client
// path has not run live yet (RW-240). Closing is explicit (close()).
EditorLevel::~EditorLevel()
{
}

// The editor's IGame, published as the game's own are (BF2Engine::startGame: registerSingleton
// {"Game", &game, IID_IGame, cid, 2}); flags 4 because the object exists already and stays the
// editor's (Core/ClassManager.h). Then the game logic: a ClientGameLogic (shared code casts
// `gameLogic` to GameLogic, so it must be a real one) and its init(), which marks the template
// manager initialised so a level's templates are the ones unload() removes.
bool EditorLevel::ensureGameObjects()
{
    if (!m_game)
    {
        if (game)
        {
            logLine(LogLevel::Error, "level: the engine already has a game object; the editor will not replace it");
            return false;
        }
        m_game = new EditorGame();
        game = m_game;
        SingletonRegInfo info = { std::string("Game"), reinterpret_cast<IBase**>(&game), IID_IGame, 0, 4 };
        classManager->registerSingleton(info);
        game->init();
    }
    if (!gameLogic)
        return replaceGameLogic();
    return true;
}

bool EditorLevel::replaceGameLogic()
{
    // setGameLogic releases the previous one first (Core/ModuleRegistry.cpp).
    if (!setGameLogic(CID_ClientGameLogic) || !gameLogic)
    {
        logLine(LogLevel::Error, "level: no ClientGameLogic could be created");
        return false;
    }
    gameLogic->init();
    return true;
}

bool EditorLevel::open(const std::string& name, const std::string& gameMode, int maxPlayers)
{
    if (m_open)
        close();
    if (name.empty() || name.find_first_of("/\\:.") != std::string::npos)
    {
        logLine(LogLevel::Error, "level: '%s' is not a level folder name", name.c_str());
        return false;
    }
    const std::string levelDirectory = settings->stringGet("GSLevelDirectory", "Levels/");
    const std::string path = levelDirectory + name + "/";
    if (!io::fileManager->fileExists(path + "Init.con"))
    {
        logLine(LogLevel::Error, "level: %sInit.con not found in the mod", path.c_str());
        return false;
    }
    // GameLogic::load probes GameModes/<mode>/{16,32,64,128}/GamePlayObjects.con and treats "none" as
    // fatal (a severity-6 Debug: exit). The editor asks the same question first and refuses instead.
    static const int kSizes[] = { 16, 32, 64, 128 };
    std::string layers;
    for (size_t i = 0; i < sizeof(kSizes) / sizeof(kSizes[0]); ++i)
    {
        char size[8];
        std::snprintf(size, sizeof(size), "%d", kSizes[i]);
        if (io::fileManager->fileExists(path + "GameModes/" + gameMode + "/" + size + "/GamePlayObjects.con"))
            layers += (layers.empty() ? "" : ", ") + std::string(size);
    }
    if (layers.empty())
    {
        logLine(LogLevel::Error, "level: %s has no %s gameplay layer (GameModes/%s/<size>/GamePlayObjects.con)",
                name.c_str(), gameMode.c_str(), gameMode.c_str());
        return false;
    }
    logLine(LogLevel::Info, "level: %s layers for %s: %s", gameMode.c_str(), name.c_str(), layers.c_str());
    if (!ensureGameObjects())
        return false;

    // What both posters establish before GameLogic::load: the settings RendDX9 and the level code
    // read (GSLoadLevel: terrain water, static-mesh lightmaps; GSGameMode / GSMaxNrOfPlayers), and
    // the map info GameLogic::load reads back through the game object (the gameplay-object layer
    // GameModes/<mode>/<size>/). GSUseObjectCache off: no 64-per-kit object warm-up in an editor.
    settings->stringSet("GSLoadLevel", name);
    settings->stringSet("GSGameMode", gameMode);
    settings->intSet("GSMaxNrOfPlayers", maxPlayers);
    settings->boolSet("GSUseObjectCache", false);
    m_game->setLevel(levelDirectory, name, gameMode, maxPlayers);

    logLine(LogLevel::Info, "level: opening %s (%s, %d players)", path.c_str(), gameMode.c_str(), maxPlayers);
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    eventManager->postEvent(ECCore, kPreLoad, const_cast<char*>(name.c_str()), 0.0f);
    if (!gameLogic->load(path))
    {
        // GameServer::loadPath's failure arm: unload what came in, end the load stats. A load that
        // aborted (ECCore 14) leaves the GameLogic refusing every later load, so it is replaced.
        logLine(LogLevel::Error, "level: GameLogic::load refused %s", path.c_str());
        gameLogic->unload();
        loadStat->unregisterLoadCallback();
        loadStat->resetLoadStats();
        replaceGameLogic();
        return false;
    }
    // The retail editor loads a level through Init.con's editor branch, which runs Overgrowth.con and then
    // `Overgrowth.editorEnable 1`, and never OvergrowthCollision.con. In editor mode the renderer's resource
    // pre-load leaves the static isOvergrowth objects (Karkand's palms) to draw as themselves and the
    // overgrowth generates its plants from its maps. This load ran the game branch, so editor mode is set
    // before the post-load event initialises the overgrowth, and the objects the collision script made
    // are removed: the generated plants stand where they stood.
    const BlockRun editorMode = runEngineLines(std::vector<std::string>(1, "Overgrowth.editorEnable 1"));
    eventManager->postEvent(ECCore, kPostLoad, static_cast<void*>(0), 0.0f);
    eventManager->postEvent(ECCore, kResourcesLoaded, static_cast<void*>(0), 0.0f);
    int collisionRecords = 0;
    const int collisionRemoved = removeOvergrowthCollisionObjects(path, collisionRecords);
    if (editorMode.complaints)
        logLine(LogLevel::Warning, "level: %s", editorMode.firstComplaint.c_str());
    else if (collisionRecords)
        logLine(LogLevel::Info, "level: overgrowth in editor mode, as the retail editor loads it; %d of %d "
                "OvergrowthCollision.con objects removed (the retail editor never runs that script)",
                collisionRemoved, collisionRecords);

    QueryPerformanceCounter(&end);
    m_open = true;
    m_name = name;
    m_gameMode = gameMode;
    m_maxPlayers = maxPlayers;
    m_layerSize = m_game->getCurrentMapSize();
    logLine(LogLevel::Info, "level: %s open (%.1f s), %s layer %d", name.c_str(),
            static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart, gameMode.c_str(), m_layerSize);
    return true;
}

bool EditorLevel::groundHeight(float x, float z, float& height) const
{
    if (!m_open || !classManager)
        return false;
    world::IHeightmapCluster* const cluster =
        static_cast<world::IHeightmapCluster*>(classManager->getSingleton("HeightmapCluster"));
    world::IHeightmap* const heightmap = cluster ? cluster->getPrimaryHeightmap() : 0;
    if (!heightmap)
        return false;
    // Heightmap::getHeightFromGrid indexes the height data with no bounds check (as shipped), and the
    // sample reads the cell under (x, z) and its +1 neighbours (getWorldScaledHeightInWorldCoords ->
    // getHeightInLocalCoords, Physics/Heightmap.cpp). A point off the map is refused here: a pick ray
    // marching past the edge read outside the grid and faulted.
    int sizeX = 0, sizeZ = 0, posX = 0, posZ = 0;
    heightmap->getSize(sizeX, sizeZ);
    heightmap->getPos(posX, posZ);
    const Vec3& scale = heightmap->getScale();
    if (sizeX < 2 || sizeZ < 2 || scale.x <= 0.0f || scale.z <= 0.0f)
        return false;
    const float gridX = (x - static_cast<float>(posX)) / scale.x;
    const float gridZ = (z - static_cast<float>(posZ)) / scale.z;
    const float halfX = static_cast<float>(sizeX / 2);
    const float halfZ = static_cast<float>(sizeZ / 2);
    if (!(gridX >= -halfX && gridX < static_cast<float>(sizeX - 1) - halfX
          && gridZ >= -halfZ && gridZ < static_cast<float>(sizeZ - 1) - halfZ))
        return false;
    height = heightmap->getWorldScaledHeightInWorldCoords(x, z);
    return true;
}

bool EditorLevel::worldBounds(float bounds[4]) const
{
    if (!m_open || !classManager)
        return false;
    world::IHeightmapCluster* const cluster =
        static_cast<world::IHeightmapCluster*>(classManager->getSingleton("HeightmapCluster"));
    world::IHeightmap* const heightmap = cluster ? cluster->getPrimaryHeightmap() : 0;
    if (!heightmap)
        return false;
    // The same grid groundHeight samples: columns -size/2 .. size-1-size/2 about the heightmap's position.
    int sizeX = 0, sizeZ = 0, posX = 0, posZ = 0;
    heightmap->getSize(sizeX, sizeZ);
    heightmap->getPos(posX, posZ);
    const Vec3& scale = heightmap->getScale();
    if (sizeX < 2 || sizeZ < 2 || scale.x <= 0.0f || scale.z <= 0.0f)
        return false;
    bounds[0] = static_cast<float>(posX) - static_cast<float>(sizeX / 2) * scale.x;
    bounds[1] = static_cast<float>(posZ) - static_cast<float>(sizeZ / 2) * scale.z;
    bounds[2] = static_cast<float>(posX) + static_cast<float>(sizeX - 1 - sizeX / 2) * scale.x;
    bounds[3] = static_cast<float>(posZ) + static_cast<float>(sizeZ - 1 - sizeZ / 2) * scale.z;
    return true;
}

bool EditorLevel::loadEditorContent(int& made)
{
    made = 0;
    if (!m_open || !io::mainConsole || !world::objectManager)
        return false;
    const size_t before = world::objectManager->getAllObjectsFromGrid().size();
    const std::string levelPath = settings->stringGet("GSLevelDirectory", "Levels/") + m_name + "/";
    char layer[128];
    std::snprintf(layer, sizeof(layer), "GameModes/%s/%d/GamePlayObjects.con", m_gameMode.c_str(), m_layerSize);
    struct Source { std::string file; const char* argument; } sources[] = {
        { levelPath + layer, "host" },
        { levelPath + "AmbientObjects.con", "BF2Editor" },
    };
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i)
    {
        std::string text;
        if (!readEngineText(sources[i].file, text))
        {
            logLine(LogLevel::Warning, "level: %s could not be read; its editor-only objects are not shown",
                    sources[i].file.c_str());
            continue;
        }
        int refused = 0;
        const std::vector<std::string> lines = blockLines(text, sources[i].argument, refused);
        const BlockRun run = runEngineLines(lines);
        logLine(LogLevel::Info, "level: %s: its `if v_arg1 == %s` blocks made %d of %d objects for the editor%s",
                sources[i].file.c_str(), sources[i].argument, run.made, run.asked,
                refused ? " (a block with conditionals inside was left out)" : "");
        // A Sound template makes no world object in this engine: SoundObjectTemplate::createObject marks
        // the template used and returns none, as the 1.50 game does (Sound/SoundObjectTemplate.cpp). The
        // editor-only Object.create lines of the ambient sounds meant something only to the retail
        // editor's own engine, so those sounds need markers of the editor's own.
        if (!run.unknown.empty())
            logLine(LogLevel::Info, "level: %u of those make no world object in this engine (%s%s: a Sound template "
                    "creates none); their records are kept as written", static_cast<unsigned>(run.unknown.size()),
                    run.unknown[0].c_str(), run.unknown.size() > 1 ? ", ..." : "");
        if (run.complaints)
            logLine(LogLevel::Warning, "level: %d other lines answered, the first: %s", run.complaints,
                    run.firstComplaint.c_str());
    }
    made = static_cast<int>(world::objectManager->getAllObjectsFromGrid().size()) - static_cast<int>(before);
    return true;
}

int EditorLevel::runObjectLines(const std::vector<std::string>& lines, std::string& firstComplaint)
{
    firstComplaint.clear();
    if (!m_open || !io::mainConsole)
        return 0;
    const BlockRun run = runEngineLines(lines);
    firstComplaint = run.firstComplaint;
    return run.made;
}

void EditorLevel::close()
{
    if (!m_open)
        return;
    logLine(LogLevel::Info, "level: closing %s", m_name.c_str());
    if (gameLogic)
        gameLogic->unload();                    // posts CEUnloadLevel, unmounts the level archives
    m_open = false;
    m_name.clear();
    m_layerSize = 0;
}

void EditorLevel::release()
{
    close();
    if (m_game)
    {
        classManager->unregisterSingleton("Game");
        if (game == m_game)
            game = 0;
        delete m_game;
        m_game = 0;
    }
    // The ClientGameLogic stays with the engine: BF2Engine's own teardown releases `gameLogic`
    // (shutdownGame), and the editor leaves the process the way BF2.exe does while the renderer is up.
}

} // namespace bf2editor
