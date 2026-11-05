// [TEMP] [CHOSEN, not recovered] Focused execution fixture for the exact production numeric
// material-query bodies extracted by material_query_runtime.py. These small chosen receiver
// classes exercise source-level control flow only: they are not production headers, layouts,
// constructors or teardown. No server or map is launched.

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#include <crtdbg.h>
#endif

namespace dice {
namespace hfe {
namespace ai {

struct Vec3
{
    float x;
    float y;
    float z;
};

// [TEMP] Chosen record for the accessor's indexing. The size check below checks this fixture
// record only; it is not a production-header or retail ABI assertion.
struct Polygon
{
    unsigned char vertexCount;
    unsigned char m_material;
    unsigned short cluster;
    unsigned int vertexOffset;
    Vec3 m_normal;
};

class QuadTree
{
public:
    unsigned char getMaterialOfPolygon(unsigned int polygon) const;
    Polygon* m_polygons;
};

class ClusterGraph;

class QuadTreeManager
{
public:
    QuadTree* getIsland(unsigned int island) const;
    std::vector<std::pair<QuadTree*, ClusterGraph*> > m_quadTrees;
};

class AIPathfinding
{
public:
    class MapContainer
    {
    public:
        bool getMaterial(unsigned int quadTree, unsigned int polygon,
                         unsigned char& material) const;
        QuadTreeManager* m_quadTreeManager;
    };
};

#include "material_query_actual.inl"

} // namespace ai
} // namespace hfe
} // namespace dice

using namespace dice::hfe::ai;

static_assert(sizeof(void*) == 4, "This fixture requires an x86 build");
static_assert(UINT_MAX == 0xFFFFFFFFu, "The sentinel contract requires 32-bit unsigned int");

static unsigned int g_checks = 0;

#define CHECK(expression)                                                                    \
    do {                                                                                     \
        ++g_checks;                                                                          \
        if (!(expression)) {                                                                 \
            std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #expression);       \
            std::fflush(stderr);                                                             \
            std::exit(90);                                                                   \
        }                                                                                    \
    } while (0)

static int runNormal()
{
    CHECK(sizeof(dice::hfe::ai::Polygon) == 20);

    dice::hfe::ai::Polygon polygons[3] = {};
    polygons[0].m_material = 0;
    polygons[1].m_material = 0x7D;
    polygons[2].m_material = 0xE6;

    QuadTree tree;
    tree.m_polygons = polygons;
    CHECK(tree.getMaterialOfPolygon(0) == 0);
    CHECK(tree.getMaterialOfPolygon(1) == 0x7D);
    CHECK(tree.getMaterialOfPolygon(2) == 0xE6);

    QuadTreeManager manager;
    manager.m_quadTrees.push_back(std::make_pair(&tree, static_cast<ClusterGraph*>(0)));
    dice::hfe::ai::Polygon secondPolygon = {};
    secondPolygon.m_material = 0xFF;
    QuadTree secondTree;
    secondTree.m_polygons = &secondPolygon;
    manager.m_quadTrees.push_back(std::make_pair(&secondTree, static_cast<ClusterGraph*>(0)));
    AIPathfinding::MapContainer map;
    map.m_quadTreeManager = &manager;

    unsigned char material = 0xA5;
    CHECK(map.getMaterial(0, 1, material));
    CHECK(material == 0x7D);

    material = 0xA5;
    CHECK(map.getMaterial(0, 0, material));
    CHECK(material == 0);

    material = 0xA5;
    CHECK(map.getMaterial(0, 2, material));
    CHECK(material == 0xE6);

    material = 0xA5;
    CHECK(map.getMaterial(1, 0, material));
    CHECK(material == 0xFF);

    secondPolygon.m_material = 0x42;
    CHECK(map.getMaterial(1, 0, material));
    CHECK(material == 0x42);

    // A poisoned manager proves both sentinel arms return before the island access.
    AIPathfinding::MapContainer sentinelMap;
    sentinelMap.m_quadTreeManager =
        reinterpret_cast<QuadTreeManager*>(static_cast<std::uintptr_t>(1));

    material = 0xA5;
    CHECK(!sentinelMap.getMaterial(UINT_MAX, 0, material));
    CHECK(material == 0);

    material = 0xA5;
    CHECK(!sentinelMap.getMaterial(0, UINT_MAX, material));
    CHECK(material == 0);

    material = 0xA5;
    CHECK(!sentinelMap.getMaterial(UINT_MAX, UINT_MAX, material));
    CHECK(material == 0);

    std::printf("PASS material_query_runtime %u checks\n", g_checks);
    return 0;
}

static int runBadIsland()
{
    dice::hfe::ai::Polygon polygon = {};
    QuadTree tree;
    tree.m_polygons = &polygon;
    QuadTreeManager manager;
    manager.m_quadTrees.push_back(std::make_pair(&tree, static_cast<ClusterGraph*>(0)));
    AIPathfinding::MapContainer map;
    map.m_quadTreeManager = &manager;
    unsigned char material = 0xA5;
    // A large non-sentinel index makes the unchecked pair stride leave the vector allocation.
    volatile unsigned int badIsland = 0x10000000u;
    const bool accepted = map.getMaterial(badIsland, 0, material);
    std::fprintf(stderr, "FAIL bad_island unexpectedly returned accepted=%u material=%u\n",
                 static_cast<unsigned int>(accepted), static_cast<unsigned int>(material));
    return 90;
}

static int runNullPolygonStorage()
{
    volatile std::uintptr_t zero = 0;
    QuadTree tree;
    tree.m_polygons = reinterpret_cast<dice::hfe::ai::Polygon*>(zero);
    QuadTreeManager manager;
    manager.m_quadTrees.push_back(std::make_pair(&tree, static_cast<ClusterGraph*>(0)));
    AIPathfinding::MapContainer map;
    map.m_quadTreeManager = &manager;
    unsigned char material = 0xA5;
    // Consume both outputs if it returns so an unused pure call cannot be optimized away.
    const bool accepted = map.getMaterial(0, 0, material); // Direct byte read at address 1.
    std::fprintf(stderr, "FAIL null_polygon_storage unexpectedly returned accepted=%u material=%u\n",
                 static_cast<unsigned int>(accepted), static_cast<unsigned int>(material));
    return 90;
}

int main(int argc, char** argv)
{
#if defined(_WIN32)
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
#endif
    if (argc != 2)
        return 89;
    if (std::strcmp(argv[1], "normal") == 0)
        return runNormal();
    if (std::strcmp(argv[1], "bad_island") == 0)
        return runBadIsland();
    if (std::strcmp(argv[1], "null_polygon_storage") == 0)
        return runNullPolygonStorage();
    return 89;
}
