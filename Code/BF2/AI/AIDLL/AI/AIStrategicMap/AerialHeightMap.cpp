// Cached ground heights for aerial navigation. Evidence: libaidll.so (ia-32) 0x3133B0..0x313D09.

#include "AerialHeightMap.h"

#include <algorithm>
// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x313C40, __tcf_0 0x313CA0, _GLOBAL__I 0x313CD0).
#include <iostream>

#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIAdapterTypes.h"     // AIObjectPredicate
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h" // IAIObjectEnvironment::getMaxHeight
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h"             // IAIGame::getLevelPath
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"          // IAITerrain, the world bounds
#include "../../../../IO/File/File.h"

namespace dice {
namespace hfe {
namespace ai {

AerialHeightMap* AerialHeightMap::m_instance = 0;

// Filters vehicles out of the height probe: accepts every object that is not mobile. Inline in
// the image (weak ELF 0x313CF0, emitted with this TU), so declared here; its vtable has the one
// predicate slot and no destructor. Placement is [INFERRED].
class AIObjectPredicateNoVehicles : public AIObjectPredicate
{
public:
    virtual bool predicate(IAIObject* object)
    {
        return !object->isMobile();
    }
};

namespace {

// The cache's name, spelled dice's way in both directions. "AerialHeighMap" is missing a 't' and
// that is not a transcription slip - the reader and the writer share the literal, so a rebuilt
// server that corrects the spelling stops finding the file a shipped level ships with.
const char* const kHeightMapSuffix = "/AIPathFinding/AerialHeighMap.ahm";

} // namespace

// ELF 0x3133B0 (C2; C1 0x3133F0). Starts with cell size 16, zero origin, and no height buffer.
// Construction unconditionally replaces the singleton.
AerialHeightMap::AerialHeightMap()
    : m_cellSize(16.0f)
    , m_heights(0)
{
    m_origin.x = 0.0f;
    m_origin.y = 0.0f;

    // m_width and m_height deliberately not initialised - the binary steps over both. See the
    // header; `m_width` is genuinely read before anything is guaranteed to have written it.
    m_instance = this;
}

// ELF 0x313430 (D2; D1 0x313470). It nulls the static unconditionally, without testing `== this`, so destroying a displaced
// instance clears the pointer to the live one. Reproduced.
AerialHeightMap::~AerialHeightMap()
{
    delete[] m_heights;
    m_instance = 0;
}

// Samples height above ground. Grid lookup requires a position inside the configured bounds.
// An unloaded map returns the missing-height sentinel 9999.
// ELF 0x313960.
float AerialHeightMap::getAltitude(const Vec3& position)
{
    return position.y - getGroundHeight(position);
}

// ELF 0x3139A0.
float AerialHeightMap::getGroundHeight(const Vec3& position)
{
    return getGroundHeight(position.x, position.z);
}

// ELF 0x3139E0.
float AerialHeightMap::getGroundHeight(float x, float z)
{
    if (!m_heights)
        return 9999.0f;

    const int column = static_cast<int>((x - m_origin.x) / m_cellSize);
    const int row    = static_cast<int>((z - m_origin.y) / m_cellSize);

    return static_cast<float>(m_heights[column + m_width * row]);
}

// ELF 0x313650. Loads the level's cached height grid. Missing cache data leaves the map empty.
void AerialHeightMap::readHeightMapFromFile()
{
    const std::string levelPath =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getLevelPath();

    io::File file;

    std::string path(levelPath);
    path.append(kHeightMapSuffix);

    if (!file.open(path, io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0))
        return;

    file.read(m_cellSize);
    file.read(m_width);
    file.read(m_height);
    file.read(m_origin);

    if (!m_heights)
        m_heights = new unsigned short[m_width * m_height];

    file.read(m_heights, 2 * m_width * m_height);
    file.close();
}

// ELF 0x3134B0. The exact mirror of the reader, field for field and in the same order. It shares the reader's
// silence on failure, and it writes the array unconditionally - a save on an object whose
// `m_heights` is still null passes a null pointer and a zero size to `File::write`.
void AerialHeightMap::saveHeightMapToFile()
{
    const std::string levelPath =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getLevelPath();

    io::File file;

    std::string path(levelPath);
    path.append(kHeightMapSuffix);

    if (file.open(path, static_cast<io::FileMode>(0), io::FILEACCESS_1, 0))
    {
        file.write(m_cellSize);
        file.write(m_width);
        file.write(m_height);
        file.write(m_origin);
        file.write(m_heights, 2 * m_width * m_height);
        file.close();
    }
}

// ELF 0x313810. Builds the aerial height grid from terrain bounds and saves it to the level cache.
void AerialHeightMap::createHeightMap(int samplesPerAxis)
{
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);

    m_width  = static_cast<int>(static_cast<float>(terrain->getWorldSizeX()) / m_cellSize);
    m_height = static_cast<int>(static_cast<float>(terrain->getWorldSizeZ()) / m_cellSize);

    m_origin.x = static_cast<float>(terrain->getWorldMinX());
    m_origin.y = static_cast<float>(terrain->getWorldMinZ());

    if (m_heights)
        delete[] m_heights;

    m_heights = new unsigned short[m_width * m_height];

    for (int z = 0; z < m_height; ++z)
        for (int x = 0; x < m_width; ++x)
            m_heights[x + m_width * z] = computeHeightAt(x, z, samplesPerAxis);

    saveHeightMapToFile();
}

// ELF 0x313A60. The highest of samplesPerAxis^2 probes spread over the cell (at the centres of a
// regular sub-grid): each probe takes IAIObjectEnvironment::getMaxHeight on a vertical line from
// y = 9999 down to y = -9999 (the lower end one metre further along x), ignoring vehicles, and
// then the higher of that and the water level. The probe result is not reset between probes, so a
// query that does not write it keeps the previous probe's height; the first starts at -9999. The
// result is truncated to 16 bits.
unsigned short AerialHeightMap::computeHeightAt(int cellX, int cellZ, int samplesPerAxis)
{
    const float cellOriginX = static_cast<float>(cellX) * m_cellSize + m_origin.x;
    const float cellOriginZ = static_cast<float>(cellZ) * m_cellSize + m_origin.y;
    const float top = 9999.0f;
    AIObjectPredicateNoVehicles noVehicles;
    float height = -9999.0f;
    float highest = -9999.0f;
    const float step = m_cellSize / static_cast<float>(samplesPerAxis);
    for (int row = 0; row < samplesPerAxis; ++row)
    {
        const float baseY = 0.0f;
        const float offsetZ = (static_cast<float>(row) + 0.5f) * step;
        for (int column = 0; column < samplesPerAxis; ++column)
        {
            Vec3 from;
            from.x = (static_cast<float>(column) + 0.5f) * step + cellOriginX;
            from.y = baseY + top;
            from.z = offsetZ + cellOriginZ;
            Vec3 to;
            to.x = from.x + 1.0f;
            to.y = -9999.0f;
            to.z = from.z;
            static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
                ->getMaxHeight(from, to, &noVehicles, height);
            const float probe = height;
            const float water = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                ->getWaterLevel(from.x, from.z);
            height = std::max(probe, water);
            highest = std::max(highest, height);
        }
    }
    return static_cast<unsigned short>(static_cast<int>(highest));
}

} // namespace ai
} // namespace hfe
} // namespace dice
