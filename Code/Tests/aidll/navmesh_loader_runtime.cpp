// [TEMP]
// Runs real navmesh loaders against stock and synthetic .qtr/.clb pairs.
// Test collaborators supply byte-buffer files, level paths, ID counts, roster state, and fatal Debug reporting.
#include "../../BF2/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTreeManager.h"
#include "../../BF2/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTree.h"
#include "../../BF2/AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.h"
#include "../../BF2/AI/AIDLL/AI/DrawPrimitives/DPManager.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/BFGame.h"
#include "../../BF2/Core/Debug.h"
#include "../../BF2/IO/File/File.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <string>
#include <vector>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "x86 only");

[[noreturn]] static void unexpected(const char* what)
{
    std::fprintf(stderr, "UNEXPECTED %s\n", what);
    std::fflush(stderr);
    std::abort();
}

static void out(const std::string& line)
{
    std::fputs(line.c_str(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

static std::string num(unsigned long long value) { return std::to_string(value); }

// [TEMP] io::File over a byte buffer
// File's whole object is one INonlinearStream*; every member that touches it is defined here, so
// the fixture keeps its own record behind the pointer and never calls through it.
namespace {
struct Stream
{
    std::string name;                  // the path with the level directory shown as <level>
    std::vector<unsigned char> bytes;
    size_t position = 0;
    std::string reads;                 // every raw read's requested size, in order
};
std::string levelDirectory;
Stream* lastQuadTreeStream = nullptr;

std::string shown(const std::string& path)
{
    if (path.compare(0, levelDirectory.size(), levelDirectory) == 0)
        return "<level>" + path.substr(levelDirectory.size());
    return path;
}
} // namespace

namespace dice { namespace hfe { namespace io {

File::File() : m_file(nullptr) {}

File::~File()
{
    if (m_file)
    {
        out("release " + reinterpret_cast<Stream*>(m_file)->name);
        m_file = nullptr;
    }
}

bool File::open(const std::string& path, IFileSystem::FileMode mode, IFileSystem::FileAccess access,
                unsigned int flags)
{
    const std::string arguments = " mode=" + std::to_string(static_cast<int>(mode)) + " access=" +
                                  std::to_string(static_cast<int>(access)) + " flags=" + num(flags);
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
    {
        out("open-fail " + shown(path) + arguments);
        return false;
    }
    Stream* stream = new Stream;
    stream->name = shown(path);
    unsigned char chunk[65536];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof(chunk), file)) > 0)
        stream->bytes.insert(stream->bytes.end(), chunk, chunk + got);
    std::fclose(file);
    if (m_file)
        unexpected("open over an open File");
    m_file = reinterpret_cast<INonlinearStream*>(stream);
    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".qtr") == 0)
        lastQuadTreeStream = stream;
    out("open " + stream->name + arguments + " size=" + num(stream->bytes.size()));
    return true;
}

void File::close()
{
    if (!m_file)
        unexpected("close without a stream");
    Stream* stream = reinterpret_cast<Stream*>(m_file);
    out("close " + stream->name + " position=" + num(stream->position) + " size=" +
        num(stream->bytes.size()) + " reads=" + stream->reads);
    m_file = nullptr;
}

unsigned int File::read(void* buffer, unsigned int size)
{
    if (!m_file)
        unexpected("read without a stream");
    Stream* stream = reinterpret_cast<Stream*>(m_file);
    const size_t left = stream->bytes.size() - stream->position;
    const size_t got = size < left ? size : left;
    if (got)
        std::memcpy(buffer, &stream->bytes[stream->position], got);
    stream->position += got;
    if (!stream->reads.empty())
        stream->reads += ',';
    stream->reads += num(size);
    if (got != size)
        stream->reads += "(short:" + num(got) + ")";
    return static_cast<unsigned int>(got);
}

bool File::read(unsigned int& v) { return read(&v, sizeof v) == sizeof v; }
bool File::read(int& v) { return read(&v, sizeof v) == sizeof v; }
bool File::read(float& v) { return read(&v, sizeof v) == sizeof v; }

}}} // namespace dice::hfe::io

// [TEMP] the roster statics the two TUs read, and the two interfaces behind them
namespace dice { namespace hfe { namespace ai {
void* CommonSingletonServer::m_iaiGame = nullptr;
void* CommonSingletonServer::m_iDPManager = nullptr;

// The interface lifecycles register with the (absent) ClassManager in the engine; not this
// fixture's subject.
IAIGame::IAIGame() {}
IAIGame::~IAIGame() {}
IDPManager::IDPManager() {}
IDPManager::~IDPManager() {}
}}} // namespace dice::hfe::ai

namespace {
class LevelGame : public IAIGame
{
public:
    std::string m_levelPath;
    int getMaxNumberOfPlayers() const override { unexpected("getMaxNumberOfPlayers"); }
    int getAISettingsFromGame(int&, bool&, float&) const override { unexpected("getAISettingsFromGame"); }
    bool isServer() const override { unexpected("isServer"); }
    int getTickets(int) const override { unexpected("getTickets"); }
    int getClosestHostileMine(IAIPlayer*, Vec3, bool&, Vec3&, Vec3&, unsigned int&,
                              std::vector<unsigned int>&) const override { unexpected("getClosestHostileMine"); }
    int getNumberOfControlPoints(int) const override { unexpected("getNumberOfControlPoints"); }
    int getOwnerOfControlPoint(unsigned int) const override { unexpected("getOwnerOfControlPoint"); }
    float getControlPointTakeOverCounter(unsigned int) const override { unexpected("getControlPointTakeOverCounter"); }
    bool getControlPointInfo(unsigned int, Vec3&, float&, bool&) const override { unexpected("getControlPointInfo"); }
    int findControlPoint(const Vec2&, const Vec2&) const override { unexpected("findControlPoint"); }
    const std::list<int>& getSpawnPointList(int) const override { unexpected("getSpawnPointList"); }
    const Vec3& getSpawnPointPosition(int) const override { unexpected("getSpawnPointPosition"); }
    int getControlPointId(int) const override { unexpected("getControlPointId"); }
    int getSpawnPointPositions(std::vector<Vec3>&, unsigned char) const override { unexpected("getSpawnPointPositions"); }
    void getControlPointPositions(std::vector<Vec3>&, unsigned char) const override { unexpected("getControlPointPositions"); }
    bool isEligibleSpawnpoint(int, int) const override { unexpected("isEligibleSpawnpoint"); }
    bool isEnterOnSpawnSpawnpoint(int) const override { unexpected("isEnterOnSpawnSpawnpoint"); }
    int getEnterOnSpawnSpawnpointObject(int, int) const override { unexpected("getEnterOnSpawnSpawnpointObject"); }
    std::string getLevelPath() const override { out("getLevelPath"); return m_levelPath; }
    int getMaxNumberOfSquads() const override { unexpected("getMaxNumberOfSquads"); }
    int getMaxNumberOfSquadMembers() const override { unexpected("getMaxNumberOfSquadMembers"); }
    float getRecomendedViewDistance() const override { unexpected("getRecomendedViewDistance"); }
    bool isPlaying() const override { unexpected("isPlaying"); }
    void setIsPrecaching(bool) override { unexpected("setIsPrecaching"); }
    bool getIsPrecaching() const override { unexpected("getIsPrecaching"); }
    void enemySpottedByAI(world::IPlayer*, world::IObject*) const override { unexpected("enemySpottedByAI"); }
};

class CountingDPManager : public IDPManager
{
public:
    int m_deleteMesh = 0;
    int deleteCircle(DrawPrimitive*&) override { unexpected("deleteCircle"); }
    int getCircleAttributes(DrawPrimitive*, float&, unsigned int&, Vec3&, float&, Vec3&, bool&, int&) override { unexpected("getCircleAttributes"); }
    int setCircleAttributes(DrawPrimitive*, float, unsigned int, const Vec3&, float, const Vec3&, bool, int) override { unexpected("setCircleAttributes"); }
    int deleteLine(DrawPrimitive*&) override { unexpected("deleteLine"); }
    int getLineAttributes(DrawPrimitive*, float&, unsigned int&, std::vector<Vec3>&, float&) override { unexpected("getLineAttributes"); }
    int setLineAttributes(DrawPrimitive*, float, unsigned int, const std::vector<Vec3>&, float) override { unexpected("setLineAttributes"); }
    int delete2dText(DrawPrimitive*&) override { unexpected("delete2dText"); }
    int get2dTextAttributes(DrawPrimitive*, float&, unsigned int&, Vec2&, std::vector<std::string>&, int&, AITextAlignmentX&, AITextAlignmentY&) override { unexpected("get2dTextAttributes"); }
    int set2dTextAttributes(DrawPrimitive*, float, unsigned int, const Vec2&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) override { unexpected("set2dTextAttributes"); }
    int delete3dText(DrawPrimitive*&) override { unexpected("delete3dText"); }
    int get3dTextAttributes(DrawPrimitive*, float&, unsigned int&, Vec3&, std::vector<std::string>&, int&, AITextAlignmentX&, AITextAlignmentY&) override { unexpected("get3dTextAttributes"); }
    int set3dTextAttributes(DrawPrimitive*, float, unsigned int, const Vec3&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) override { unexpected("set3dTextAttributes"); }
    int deleteTriangle(DrawPrimitive*&) override { unexpected("deleteTriangle"); }
    int getTriangleAttributes(DrawPrimitive*, float&, std::vector<Vec3>&, std::vector<unsigned int>&, bool&) override { unexpected("getTriangleAttributes"); }
    int setTriangleAttributes(DrawPrimitive*, float&, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool&) override { unexpected("setTriangleAttributes"); }
    int deletePolygon(DrawPrimitive*&) override { unexpected("deletePolygon"); }
    int getPolygonAttributes(DrawPrimitive*, float&, bool&) override { unexpected("getPolygonAttributes"); }
    int setPolygonAttributes(DrawPrimitive*, float&, bool&) override { unexpected("setPolygonAttributes"); }
    // The one slot the loader reaches: QuadTree::undraw on every clean(). No mesh is ever drawn, so
    // the handle it passes must be null, and DPManager's answer for that is "not found".
    bool deleteMesh(DrawPrimitive*& primitive) override
    {
        if (primitive)
            unexpected("deleteMesh with a handle");
        ++m_deleteMesh;
        return false;
    }
    void setTextGamma(float) override { unexpected("setTextGamma"); }
    float getTextGamma() const override { unexpected("getTextGamma"); }
    void drawAll() override { unexpected("drawAll"); }
    void nodrawUpdate() override { unexpected("nodrawUpdate"); }
    int deleteAll() override { unexpected("deleteAll"); }
};

LevelGame game;
CountingDPManager drawPrimitives;

// Reports what the fatal Debug(Error) said, and where the .qtr stream stood, before ~Debug aborts.
void debugCallback(DebugType type, const std::string& module, const std::string&, int line,
                   const std::string&, const std::string& text, const std::string& fullFileName, void*)
{
    out("DEBUG type=" + std::to_string(static_cast<int>(type)) + " file=" + fullFileName + " line=" +
        std::to_string(line) + " module=" + module);
    out("TEXT " + text);
    if (lastQuadTreeStream)
        out("qtr position=" + num(lastQuadTreeStream->position) + " size=" +
            num(lastQuadTreeStream->bytes.size()));
}

void report(QuadTreeManager& manager, const char* stage)
{
    std::string line = std::string("state ") + stage + " islands=" + num(manager.getNumberOfIslands());
    for (unsigned int i = 0; i < manager.getNumberOfIslands(); ++i)
    {
        line += manager.getIsland(i) ? " [tree," : " [null,";
        line += manager.getClusterGraph(i) ? "graph]" : "null]";
    }
    line += " deleteMesh=" + num(drawPrimitives.m_deleteMesh);
    out(line);
}
} // namespace

int main(int argc, char** argv)
{
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    setDebugCallback(DebugTypeError, debugCallback, nullptr);

    if (argc != 4)
        unexpected("usage: navmesh_loader_runtime load|reload|destroyone LEVELDIR MAP");
    const std::string mode = argv[1];
    levelDirectory = argv[2];
    game.m_levelPath = argv[2];
    CommonSingletonServer::m_iaiGame = static_cast<IAIGame*>(&game);
    CommonSingletonServer::m_iDPManager = static_cast<IDPManager*>(&drawPrimitives);

    // Never deleted: ~QuadTreeManager is still a named frontier (its debug-mesh unloads are
    // unwritten), and it is not this fixture's subject.
    QuadTreeManager& manager = *new QuadTreeManager;

    manager.loadQuadTrees(argv[3]);
    report(manager, "load");

    if (mode == "load")
    {
        manager.destroyQuadTrees();
        report(manager, "destroyQuadTrees");
    }
    else if (mode == "reload")
    {
        manager.loadQuadTrees(argv[3]);
        report(manager, "reload");
    }
    else if (mode == "destroyone")
    {
        manager.destroyQuadTree(manager.getIsland(0));
        report(manager, "destroyQuadTree");
        manager.destroyQuadTrees();
        report(manager, "destroyQuadTrees");
    }
    else
    {
        unexpected("mode");
    }
    out("PASS " + mode);
    return 0;
}
