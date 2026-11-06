// [TEMP] Strict x86 seeded-polygon fixture. The target and its direct AIPathfinding helpers are
// exact generated source extracts; the serialized navmesh, geometry and material TUs link whole.
#include "map_polygon_runtime.h"

#define main navmesh_loader_fixture_main
#include "navmesh_loader_runtime.cpp"
#undef main

#include <limits>

template<class Tag, typename Tag::type Member>
struct PrivateMember
{
    friend typename Tag::type access(Tag) { return Member; }
};

struct MapsTag
{
    using type = std::vector<AIPathfinding::MapContainer*> AIPathfinding::*;
    friend type access(MapsTag);
};

template struct PrivateMember<MapsTag, &AIPathfinding::m_maps>;

static std::vector<AIPathfinding::MapContainer*>& maps(AIPathfinding& path)
{
    return path.*access(MapsTag{});
}

namespace dice { namespace hfe { namespace ai {

// [TEMP] Chosen object lifetime only. The constructor's search/debug allocations and destructor's
// reset are outside this query fixture; C++ still constructs every native STL member.
AIPathfinding::AIPathfinding() {}
AIPathfinding::~AIPathfinding() {}
IAISettings::IAISettings() {}
IAISettings::~IAISettings() {}
void* CommonSingletonServer::m_iaiSettings = nullptr;

} } }

#include "unused.inl"
#include "settings.inl"

MaterialManager* QuerySettings::getVehicleMaterialManager(int id) const
{
    ++calls;
    equipment = id;
    if (replacement)
    {
        // This mutation distinguishes a receiver fetched before the settings call from one fetched
        // afterwards. The old pointer remains deliberately fixture-owned and alive.
        std::vector<AIPathfinding::MapContainer*> fresh(1, replacement);
        maps(*path).swap(fresh);
    }
    return result;
}

static unsigned int checks;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
    std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #expression); \
    std::exit(90); } } while (0)

static const unsigned int sentinelPolygon = 0x13579BDFu;
static const unsigned int sentinelQuadTree = 0x2468ACE0u;
static const unsigned int invalid = 0xFFFFFFFFu;
using Map = AIPathfinding::MapContainer;

static Map* load(const char* name)
{
    Map* const map = new Map(name, 7);
    CHECK(map->m_id == 7 && map->m_name == name);
    CHECK(map->m_properties != nullptr && map->m_quadTreeManager != nullptr);
    map->m_quadTreeManager->loadQuadTrees(name);
    CHECK(map->m_quadTreeManager->getNumberOfIslands() == 1);
    return map;
}

static void appendIsland(Map* destination, Map* source)
{
    destination->m_quadTreeManager->m_quadTrees.push_back(
        source->m_quadTreeManager->m_quadTrees[0]);
    source->m_quadTreeManager->m_quadTrees.clear();
}

static AIPathfinding* pathWith(Map* map)
{
    AIPathfinding* const path = new AIPathfinding;
    maps(*path).push_back(map);
    return path;
}

static QuerySettings settingsFor(AIPathfinding* path, MaterialManager* materials)
{
    QuerySettings settings;
    settings.path = path;
    settings.result = materials;
    return settings;
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    setDebugCallback(DebugTypeError, debugCallback, nullptr);

    if (argc != 3)
        unexpected("usage: seeded_polygon_runtime MODE OUT");
    const std::string mode = argv[1];
    levelDirectory = std::string(argv[2]) + "/data";
    game.m_levelPath = levelDirectory;
    CommonSingletonServer::m_iaiGame = static_cast<IAIGame*>(&game);
    CommonSingletonServer::m_iDPManager = static_cast<IDPManager*>(&drawPrimitives);

    if (mode == "bounds")
    {
        AIPathfinding* const path = new AIPathfinding;
        CommonSingletonServer::m_iaiSettings = nullptr;
        for (int map : {(std::numeric_limits<int>::min)(), -1, 0,
                        (std::numeric_limits<int>::max)()})
        {
            unsigned int polygon = sentinelPolygon;
            unsigned int quadTree = sentinelQuadTree;
            CHECK(!path->getPolygon({0, 0, 0}, map, polygon, quadTree, 17, 19, 23));
            CHECK(polygon == sentinelPolygon && quadTree == sentinelQuadTree);
        }
        maps(*path).push_back(load("walkable"));
        for (int map : {-1, 1, (std::numeric_limits<int>::max)()})
        {
            unsigned int polygon = sentinelPolygon;
            unsigned int quadTree = sentinelQuadTree;
            CHECK(!path->getPolygon({0, 0, 0}, map, polygon, quadTree, 29, 31, 37));
            CHECK(polygon == sentinelPolygon && quadTree == sentinelQuadTree);
        }
    }
    else if (mode == "hint_reuse")
    {
        Map* const map = load("walkable");
        appendIsland(map, load("walkable"));
        AIPathfinding* const path = pathWith(map);
        MaterialManager materials;
        QuerySettings settings = settingsFor(path, &materials);
        CommonSingletonServer::m_iaiSettings = &settings;
        unsigned int polygon = sentinelPolygon;
        unsigned int quadTree = sentinelQuadTree;
        CHECK(path->getPolygon({0, 0, 0}, 0, polygon, quadTree, 0, 1, -12345));
        CHECK(polygon == 0 && quadTree == 1);
        CHECK(settings.calls == 1 && settings.equipment == -12345);
    }
    else if (mode == "hint_fallback")
    {
        Map* const map = load("walkable");
        appendIsland(map, load("walkable"));
        AIPathfinding* const path = pathWith(map);
        MaterialManager materials;
        QuerySettings settings = settingsFor(path, &materials);
        CommonSingletonServer::m_iaiSettings = &settings;
        unsigned int polygon = sentinelPolygon;
        unsigned int quadTree = sentinelQuadTree;
        CHECK(path->getPolygon({0, 0, 0}, 0, polygon, quadTree, invalid, 1, 41));
        CHECK(polygon == 0 && quadTree == 0);
        polygon = sentinelPolygon;
        quadTree = sentinelQuadTree;
        CHECK(path->getPolygon({0, 0, 0}, 0, polygon, quadTree, 0, invalid, 43));
        CHECK(polygon == 0 && quadTree == 0);
        CHECK(settings.calls == 2 && settings.equipment == 43);
    }
    else if (mode == "material_choice")
    {
        AIPathfinding* const path = pathWith(load("blocked"));
        MaterialManager blocked;
        QuerySettings settings = settingsFor(path, &blocked);
        CommonSingletonServer::m_iaiSettings = &settings;
        unsigned int polygon = sentinelPolygon;
        unsigned int quadTree = sentinelQuadTree;
        CHECK(!path->getPolygon({0, 0, 0}, 0, polygon, quadTree, invalid, invalid, 51));
        CHECK(polygon == sentinelPolygon && quadTree == invalid);
        MaterialManager allowed;
        allowed.setMaterialCost(0, -0.0f);
        settings.result = &allowed;
        polygon = sentinelPolygon;
        quadTree = sentinelQuadTree;
        CHECK(path->getPolygon({0, 0, 0}, 0, polygon, quadTree, invalid, invalid, 53));
        CHECK(polygon == 0 && quadTree == 0);
        CHECK(settings.calls == 2 && settings.equipment == 53);
    }
    else if (mode == "null_material_hint")
    {
        AIPathfinding* const path = pathWith(load("walkable"));
        QuerySettings settings = settingsFor(path, nullptr);
        CommonSingletonServer::m_iaiSettings = &settings;
        unsigned int polygon = sentinelPolygon;
        unsigned int quadTree = sentinelQuadTree;
        CHECK(path->getPolygon({0, 0, 0}, 0, polygon, quadTree, 0, 0, 59));
        CHECK(polygon == 0 && quadTree == 0);
        CHECK(settings.calls == 1 && settings.equipment == 59 && settings.result == nullptr);
    }
    else if (mode == "failure_outputs")
    {
        AIPathfinding* const path = pathWith(load("walkable"));
        MaterialManager materials;
        QuerySettings settings = settingsFor(path, &materials);
        CommonSingletonServer::m_iaiSettings = &settings;
        unsigned int polygon = sentinelPolygon;
        unsigned int quadTree = sentinelQuadTree;
        CHECK(!path->getPolygon({20, 0, 20}, 0, polygon, quadTree, invalid, invalid, 61));
        CHECK(polygon == sentinelPolygon);
        CHECK(quadTree == invalid);

        AIPathfinding* const emptyPath = pathWith(new Map("empty", 9));
        settings.path = emptyPath;
        polygon = sentinelPolygon;
        quadTree = sentinelQuadTree;
        CHECK(!emptyPath->getPolygon({0, 0, 0}, 0, polygon, quadTree, invalid, invalid, 67));
        CHECK(polygon == sentinelPolygon && quadTree == invalid);
        CHECK(settings.calls == 2 && settings.equipment == 67);
    }
    else if (mode == "alias_outputs")
    {
        Map* const map = load("walkable");
        appendIsland(map, load("walkable"));
        AIPathfinding* const path = pathWith(map);
        MaterialManager materials;
        QuerySettings settings = settingsFor(path, &materials);
        CommonSingletonServer::m_iaiSettings = &settings;
        unsigned int same = sentinelPolygon;
        CHECK(path->getPolygon({0, 0, 0}, 0, same, same, 0, 1, 71));
        CHECK(same == 1); // polygon zero is written first; returned quad-tree one wins the alias.
        same = sentinelPolygon;
        CHECK(!path->getPolygon({20, 0, 20}, 0, same, same, invalid, invalid, 73));
        CHECK(same == invalid);
    }
    else if (mode == "callback_order")
    {
        Map* const blockedMap = load("blocked");
        Map* const replacement = load("walkable");
        AIPathfinding* const path = pathWith(blockedMap);
        MaterialManager materials; // material one is accepted; material zero is rejected.
        QuerySettings settings = settingsFor(path, &materials);
        settings.replacement = replacement;
        CommonSingletonServer::m_iaiSettings = &settings;
        unsigned int polygon = sentinelPolygon;
        unsigned int quadTree = sentinelQuadTree;
        CHECK(path->getPolygon({0, 0, 0}, 0, polygon, quadTree, invalid, invalid, 79));
        CHECK(polygon == 0 && quadTree == 0);
        CHECK(settings.calls == 1 && settings.equipment == 79);
        CHECK(maps(*path).size() == 1 && maps(*path)[0] == replacement);
    }
    else if (mode == "crash_null_settings")
    {
        AIPathfinding* const path = pathWith(load("walkable"));
        CommonSingletonServer::m_iaiSettings = nullptr;
        unsigned int polygon = sentinelPolygon;
        unsigned int quadTree = sentinelQuadTree;
        std::puts("ENTER crash_null_settings");
        std::fflush(stdout);
        path->getPolygon({0, 0, 0}, 0, polygon, quadTree, 0, 0, 83);
        std::puts("REACHED-AFTER crash_null_settings");
        std::fflush(stdout);
        return 0;
    }
    else
    {
        unexpected("unknown seeded polygon mode");
    }

    std::printf("PASS %s %u checks\n", mode.c_str(), checks);
    return 0;
}
