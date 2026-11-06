// Editable heightmap-cluster operations. Source path is attested by __FILE__.
// Arithmetic follows the ELF's single-precision expression shape; loop coordinates accumulate as floats.

#include "HeightmapClusterEditable.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "../Core/ClassManager.h"
#include "../Core/Debug.h"
#include "../Core/EventManager.h"
#include "../Core/Ids.h"
#include "../Core/Random.h"
#include "../Core/SettingsRepostitory.h"
#include "../Game/IEventListener.h"
#include "../Geom/HeightmapEventNode.h"
#include "../IO/File/File.h"
#include "../IO/File/FileManager.h"
#include "../IO/File/IFileSystem.h"
#include "../IO/Stream/IStream.h"
#include "Heightmap.h"
#include "HeightmapEditable.h"

namespace dice {
namespace hfe {

extern SettingsRepostitory* settings;     // Core/Globals.cpp

namespace world {

namespace {

// Every brush and disable(false) announce their rectangle the same way (ELF: operator new(0x30),
// HeightmapEventNode(100), setAffectedArea, eventManager->postEvent(8, node, 0) - vtable +0x10,
// PE +0x14).
void postHeightmapModified(float x0, float z0, float x1, float z1)
{
    geom::HeightmapEventNode* node = new geom::HeightmapEventNode(100);
    Vec4 area;
    area.x = x0;
    area.y = z0;
    area.z = x1;
    area.w = z1;
    node->setAffectedArea(area);
    eventManager->postEvent(ECEditor, node, 0.0f);
}

// [CHOSEN] packaging of the inlined uniform draw both random() and smoothEdges() carry: the
// engine's ((raw >> 7) | 1) + 1 draw scaled by 2^-24, mapped onto lo..hi as (hi - lo) * u + lo
// (ELF 0x838A4D5.. / 0x838AE62..: `(hi - lo) * (u * 2^-24) + lo`). PE: getRaw is the shared
// Random::getRaw at 0x580750.
float randomRange(Random& random, float lo, float hi)
{
    const float u = static_cast<float>(((random.getRaw() >> 7) | 1u) + 1u) * 0.000000059604645f;
    return (hi - lo) * u + lo;
}

} // namespace

// lifetime

// ELF 0x8386F90 (C1) / 0x8386F40 (C2), 72 B; The cluster base (which
// tail-calls its own init()), the three vptrs, then the two flags and a 1.0 water depth.
HeightmapClusterEditable::HeightmapClusterEditable()
    : m_disabled(false)
    , m_simulateWater(false)
    , m_simulatedWaterDepth(1.0f)
{
}

// ELF 0x8387020 (D1) / 0x8387060 (D0); PE 0x6AF9E0. Nothing of its own.
HeightmapClusterEditable::~HeightmapClusterEditable()
{
}

// ; PE 0x691890. The registry shape (see HeightmapCluster::create).
IBase* HeightmapClusterEditable::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    HeightmapClusterEditable* self = new HeightmapClusterEditable();
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}

// IBase - HeightmapCluster's refcount (+0xC here)

void HeightmapClusterEditable::addRef() { ++m_refCount; }
int  HeightmapClusterEditable::getRef() { return m_refCount; }

// The last reference deletes through the HeightmapCluster subobject's deleting
// destructor (`call [[this+4]+14h]` on this+4, null-tested) - the subobject is what gets deleted.
int HeightmapClusterEditable::release()
{
    const int refs = --m_refCount;
    if (refs == 0)
        delete static_cast<HeightmapCluster*>(this);
    return refs;
}

// ; PE 0x6AF930. The editable interface is +0; IBase and IHeightmapCluster are
// the cluster subobject (+4).
void* HeightmapClusterEditable::queryInterface(unsigned int iid) const
{
    HeightmapClusterEditable* self = const_cast<HeightmapClusterEditable*>(this);
    if (iid == IID_IHeightmapClusterEditable)
        return static_cast<IHeightmapClusterEditable*>(self);
    if (iid == IID_IBase)
        return static_cast<IHeightmapCluster*>(static_cast<HeightmapCluster*>(self));
    if (iid == IID_IHeightmapCluster)
        return static_cast<IHeightmapCluster*>(static_cast<HeightmapCluster*>(self));
    return 0;
}

// switches

bool  HeightmapClusterEditable::isDisabled()                       { return m_disabled; }
void  HeightmapClusterEditable::setSimulateWater(bool simulate)    { m_simulateWater = simulate; }
bool  HeightmapClusterEditable::getSimulateWater()                 { return m_simulateWater; }
void  HeightmapClusterEditable::setSimulatedWaterDepth(float depth){ m_simulatedWaterDepth = depth; }
float HeightmapClusterEditable::getSimulatedWaterDepth()           { return m_simulatedWaterDepth; }

// ; Set the switch; RE-enabling (with a primary tile)
// announces the primary's whole world rectangle as modified. The four boundary getters are
// read left to right in the ELF and right to left in the PE - a constructor-argument order, no
// side effects either way.
void HeightmapClusterEditable::disable(bool disabled)
{
    m_disabled = disabled;
    if (!disabled && m_primaryHeightmap)
    {
        postHeightmapModified(m_primaryHeightmap->getLowBoundaryX(),
                              m_primaryHeightmap->getLowBoundaryZ(),
                              m_primaryHeightmap->getHighBoundaryX(),
                              m_primaryHeightmap->getHighBoundaryZ());
    }
}

// queries - a disabled cluster answers as flat, dry, everywhere-inside ground

// ; The base's body, no disabled test.
IHeightmap* HeightmapClusterEditable::getHeightmapInSector(int sectorX, int sectorZ) const
{
    if (!isSectorValid(sectorX, sectorZ))
        return 0;
    return m_heightmaps[getGridIndex(sectorX, sectorZ)];
}


IHeightmap* HeightmapClusterEditable::getHeightmapInWorldCoords(float x, float z) const
{
    if (m_disabled)
        return 0;
    return HeightmapCluster::getHeightmapInWorldCoords(x, z);
}


float HeightmapClusterEditable::getHeightInWorldCoords(float x, float z) const
{
    if (m_disabled)
        return 0.0f;
    return HeightmapCluster::getHeightInWorldCoords(x, z);
}


float HeightmapClusterEditable::getWorldScaledHeightInWorldCoords(float x, float z) const
{
    if (m_disabled)
        return 0.0f;
    return HeightmapCluster::getWorldScaledHeightInWorldCoords(x, z);
}

// ; PE 0x6ACFB0. Disabled: an up normal, material 0, height 0.
float HeightmapClusterEditable::getHeightAndNormalInWorldCoords(float x, float z, Vec3& normal,
                                                                unsigned char& material) const
{
    if (!m_disabled)
        return HeightmapCluster::getHeightAndNormalInWorldCoords(x, z, normal, material);
    normal.x = 0.0f;
    normal.y = 1.0f;
    normal.z = 0.0f;
    material = 0;
    return 0.0f;
}

// ; The base's ray, always two-sided.
bool HeightmapClusterEditable::intersectRayInWorldCoords(const Vec3& from, const Vec3& to,
                                                         Vec3& hitPoint, float& hitFraction,
                                                         Vec3& hitNormal,
                                                         unsigned char& material) const
{
    if (m_disabled)
        return false;
    return HeightmapCluster::intersectRayInWorldCoords(from, to, hitPoint, hitFraction, hitNormal,
                                                       material, true);
}

// ; Simulated water answers the set depth over a disabled
// cluster; without it a disabled cluster has no sea (-10000); enabled, the base decides.
float HeightmapClusterEditable::getWaterLevelInWorldCoords(float x, float z) const
{
    if (m_simulateWater)
    {
        if (m_disabled)
            return m_simulatedWaterDepth;
    }
    else if (m_disabled)
    {
        return -10000.0f;
    }
    return HeightmapCluster::getWaterLevelInWorldCoords(x, z);
}


bool HeightmapClusterEditable::isInsideHeightmapCluster(float x, float z) const
{
    if (m_disabled)
        return true;
    return HeightmapCluster::isInsideHeightmapCluster(x, z);
}

// ; The smallest odd cluster that still holds every
// populated sector: 2 * max(|x|, |z|) + 1 over sectors with a tile.
int HeightmapClusterEditable::getUsedGridSize() const
{
    int used = 0;
    const int half = m_clusterSize / 2;
    for (int z = -(m_clusterSize / 2); z <= half; ++z)
    {
        for (int x = -half; x <= half; ++x)
        {
            if (getHeightmapInSector(x, z))
            {
                if (used < std::abs(x))
                    used = std::abs(x);
                if (used < std::abs(z))
                    used = std::abs(z);
            }
        }
    }
    return 2 * used + 1;
}

// point writes


void HeightmapClusterEditable::setHeightInWorldCoords(float x, float z, float height)
{
    _setHeight(x, z, height);
}

// ; PE 0x6ACF50. Both calls through the primary table.
void HeightmapClusterEditable::addHeightInWorldCoords(float x, float z, float delta)
{
    setHeightInWorldCoords(x, z, getHeightInWorldCoords(x, z) + delta);
}

// ; Only inside the cluster (`== 1` in both images), and
// only onto a tile: the tile is taken as a HeightmapEditable (a static downcast - PE [tile+E4h] is
// its IHeightmapEditable table) and does the write.
void HeightmapClusterEditable::setWorldScaledHeightInWorldCoords(float x, float z, float height,
                                                                 int flags)
{
    if (isInsideHeightmapCluster(x, z) == true)
    {
        IHeightmap* tile = getHeightmapInWorldCoords(x, z);
        if (tile)
            static_cast<HeightmapEditable*>(tile)->setWorldScaledHeightInWorldCoords(x, z, height,
                                                                                     flags);
    }
}


void HeightmapClusterEditable::addWorldScaledHeightInWorldCoords(float x, float z, float delta,
                                                                 int flags)
{
    setWorldScaledHeightInWorldCoords(x, z, getWorldScaledHeightInWorldCoords(x, z) + delta, flags);
}

// ; Write one world height into every tile whose
// footprint contains (x, z), at the nearest grid sample, directly into the tile's grid (no
// flags, no modified flag, no quadtree refresh, no event). The footprint: the tile's local
// position (hw + x - posX) / w with w = (int)((sizeX - 1) * scale.x) and hw = w / 2 (integer),
// likewise z; inside when both lie in [0, 1]. The stored value is height / scale.y truncated.
// Depths other than 8/16: line 445, fatal.
void HeightmapClusterEditable::_setHeight(float x, float z, float height)
{
    const int half = m_clusterSize / 2;
    for (int sectorZ = -(m_clusterSize / 2); sectorZ <= half; ++sectorZ)
    {
        for (int sectorX = -half; sectorX <= half; ++sectorX)
        {
            Heightmap* tile = static_cast<Heightmap*>(getHeightmapInSector(sectorX, sectorZ));
            const Vec3 scale = tile->getScale();
            int sizeX, sizeZ;
            tile->getSize(sizeX, sizeZ);
            const int width = static_cast<int>(static_cast<float>(sizeX - 1) * scale.x);
            const int depth = static_cast<int>(static_cast<float>(sizeZ - 1) * scale.z);
            const int halfWidth = width / 2;
            const int halfDepth = depth / 2;
            int posX, posZ;
            tile->getPos(posX, posZ);

            const float u = (static_cast<float>(halfWidth) + x - static_cast<float>(posX))
                            / static_cast<float>(width);
            const float v = (static_cast<float>(halfDepth) + z - static_cast<float>(posZ))
                            / static_cast<float>(depth);
            if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)
            {
                const int row = static_cast<int>(static_cast<float>(
                    std::floor(static_cast<double>(v * static_cast<float>(sizeZ - 1)) + 0.5)));
                const int column = static_cast<int>(static_cast<float>(
                    std::floor(static_cast<double>(u * static_cast<float>(sizeX - 1)) + 0.5)));
                const int index = column + sizeX * row;
                if (tile->m_bitResolution == 8)
                    static_cast<unsigned char*>(tile->m_heightData)[index] =
                        static_cast<unsigned char>(static_cast<int>(height / scale.y));
                else if (tile->m_bitResolution == 16)
                    static_cast<unsigned short*>(tile->m_heightData)[index] =
                        static_cast<unsigned short>(static_cast<int>(height / scale.y));
                else
                    Debug(DEBUGTYPE_6, "Physics/HeightmapClusterEditable.cpp", 445, __FUNCTION__)
                        << "Heightmap bit depth of " << tile->m_bitResolution << " is not supported.";
            }
        }
    }
}

// Raises or lowers samples within radius using a cosine falloff and tile-scale steps.
// Clamps the integer-centered brush box to cluster bounds, then announces that box after editing.
void HeightmapClusterEditable::modifyHeight(int x, int z, float radius, float strength, int flags)
{
    const int extent = getHeightmapSize() * getClusterSize() / 2;
    const int r = static_cast<int>(radius);
    const int x0 = (-extent < x - r) ? x - r : -extent;
    const int x1 = (x + r > extent) ? extent : x + r;
    const int z0 = (-extent < z - r) ? z - r : -extent;
    const int z1 = (z + r <= extent) ? z + r : extent;

    const float step = m_primaryHeightmap->getScale().x;
    const int countZ = static_cast<int>(static_cast<float>(z1 - z0) / step);
    const int countX = static_cast<int>(static_cast<float>(x1 - x0) / step);
    const float startX = static_cast<float>(x0);
    const float startZ = static_cast<float>(z0);

    float sampleZ = startZ;
    for (int iz = 0; iz < countZ; ++iz)
    {
        float sampleX = startX;
        const float dz = sampleZ - static_cast<float>(z);
        const float dz2 = dz * dz;
        for (int ix = countX; ix > 0; --ix)
        {
            const float dx = sampleX - static_cast<float>(x);
            const float distance = sqrtf(dx * dx + dz2);
            if (radius >= distance)
            {
                float weight = cosf(distance * (1.0f / radius) * 1.5707964f);
                if (weight <= 0.0f)
                    weight = 0.0f;
                addWorldScaledHeightInWorldCoords(sampleX, sampleZ, weight * strength, flags);
            }
            sampleX = sampleX + step;
        }
        sampleZ = sampleZ + step;
    }

    postHeightmapModified(startX, startZ, static_cast<float>(x1), static_cast<float>(z1));
}

// ; Level towards a target: `height` (or, when it is the
// -1 sentinel within FLT_EPSILON, the ground at the centre). Nothing happens unless the centre is
// on a tile. Each inside sample that is on a tile moves by weight * strength towards the target
// and stops at it.
void HeightmapClusterEditable::setHeight(int x, int z, float radius, float strength, float height,
                                         int flags)
{
    const int extent = getHeightmapSize() * getClusterSize() / 2;
    const int r = static_cast<int>(radius);
    const int x0 = (-extent >= x - r) ? -extent : x - r;
    const int x1 = (x + r > extent) ? extent : x + r;
    const int z0 = (-extent < z - r) ? z - r : -extent;
    const int z1 = (z + r <= extent) ? z + r : extent;

    const float step = m_primaryHeightmap->getScale().x;
    const int countZ = static_cast<int>(static_cast<float>(z1 - z0) / step);
    const int countX = static_cast<int>(static_cast<float>(x1 - x0) / step);
    const float centreX = static_cast<float>(x);
    const float centreZ = static_cast<float>(z);

    if (!getHeightmapInWorldCoords(centreX, centreZ))
        return;

    float target = height;
    if (fabsf(height - -1.0f) <= 0.00000011920929f)
        target = getWorldScaledHeightInWorldCoords(centreX, centreZ);

    const float startX = static_cast<float>(x0);
    const float startZ = static_cast<float>(z0);
    float sampleZ = startZ;
    for (int iz = 0; iz < countZ; ++iz)
    {
        float sampleX = startX;
        const float dz = sampleZ - centreZ;
        const float dz2 = dz * dz;
        for (int ix = countX; ix > 0; --ix)
        {
            const float dx = sampleX - centreX;
            const float distance = sqrtf(dx * dx + dz2);
            if (radius >= distance && getHeightmapInWorldCoords(sampleX, sampleZ))
            {
                float weight = cosf(distance * (1.0f / radius) * 1.5707964f);
                if (weight <= 0.0f)
                    weight = 0.0f;
                const float amount = weight * strength;
                const float current = getWorldScaledHeightInWorldCoords(sampleX, sampleZ);
                float value;
                if (current <= target)
                {
                    value = amount + current;
                    if (value > target)
                        value = target;
                }
                else
                {
                    value = current - amount;
                    if (value <= target)
                        value = target;
                }
                setWorldScaledHeightInWorldCoords(sampleX, sampleZ, value, flags);
            }
            sampleX = sampleX + step;
        }
        sampleZ = sampleZ + step;
    }

    postHeightmapModified(startX, startZ, static_cast<float>(x1), static_cast<float>(z1));
}

// ; Two passes over the box through a heap buffer of
// countZ * countX floats: first each inside sample's move towards the mean of its 3x3
// neighbourhood (one primary step apart), ((sum of nine) / 9 - centre) * strength * weight, 0
// outside; then every buffered move is added. PE multiplies by 0.11111111 and applies
// (weight * delta) * strength; the ELF's weight * ((sum / 9 - centre) * strength) is followed.
void HeightmapClusterEditable::smooth(int x, int z, float radius, float strength, int flags)
{
    const int extent = getHeightmapSize() * getClusterSize() / 2;
    const int r = static_cast<int>(radius);
    const int x0 = (-extent < x - r) ? x - r : -extent;
    const int x1 = (x + r > extent) ? extent : x + r;
    const int z0 = (-extent < z - r) ? z - r : -extent;
    const int z1 = (z + r <= extent) ? z + r : extent;

    const float step = m_primaryHeightmap->getScale().x;
    const int countZ = static_cast<int>(static_cast<float>(z1 - z0) / step);
    const int countX = static_cast<int>(static_cast<float>(x1 - x0) / step);
    float* moves = new float[countZ * countX];

    const float startX = static_cast<float>(x0);
    const float startZ = static_cast<float>(z0);
    int index = 0;
    float sampleZ = startZ;
    for (int iz = 0; iz < countZ; ++iz)
    {
        const float nextZ = sampleZ + step;
        float sampleX = startX;
        const float dz = sampleZ - static_cast<float>(z);
        const float dz2 = dz * dz;
        for (int ix = 0; ix < countX; ++ix)
        {
            const float nextX = sampleX + step;
            const float dx = sampleX - static_cast<float>(x);
            const float distance = sqrtf(dx * dx + dz2);
            if (radius >= distance)
            {
                const float centre = getWorldScaledHeightInWorldCoords(sampleX, sampleZ);
                const float prevZ = sampleZ - step;
                const float prevX = sampleX - step;
                float sum = getWorldScaledHeightInWorldCoords(prevX, prevZ) + centre;
                sum = getWorldScaledHeightInWorldCoords(prevX, sampleZ) + sum;
                sum = getWorldScaledHeightInWorldCoords(prevX, nextZ) + sum;
                sum = getWorldScaledHeightInWorldCoords(sampleX, prevZ) + sum;
                sum = getWorldScaledHeightInWorldCoords(sampleX, nextZ) + sum;
                sum = getWorldScaledHeightInWorldCoords(nextX, prevZ) + sum;
                sum = getWorldScaledHeightInWorldCoords(nextX, sampleZ) + sum;
                const float delta =
                    ((getWorldScaledHeightInWorldCoords(nextX, nextZ) + sum) / 9.0f - centre) * strength;
                float weight = cosf(distance * (1.0f / radius) * 1.5707964f);
                if (weight <= 0.0f)
                    weight = 0.0f;
                moves[index++] = weight * delta;
            }
            else
            {
                moves[index++] = 0.0f;
            }
            sampleX = nextX;
        }
        sampleZ = nextZ;
    }

    index = 0;
    sampleZ = startZ;
    for (int iz = countZ; iz > 0; --iz)
    {
        float sampleX = startX;
        for (int ix = countX; ix > 0; --ix)
        {
            addWorldScaledHeightInWorldCoords(sampleX, sampleZ, moves[index++], flags);
            sampleX = sampleX + step;
        }
        sampleZ = sampleZ + step;
    }
    delete[] moves;

    postHeightmapModified(startX, startZ, static_cast<float>(x1), static_cast<float>(z1));
}

// ; Noise: every inside sample gets a uniform draw in
// [-strength, +strength) added (no falloff), from a local generator seeded with
// time(0) & 0x7FFFFFFF - not the engine's Rand.
void HeightmapClusterEditable::random(int x, int z, float radius, float strength, int flags)
{
    const int extent = getHeightmapSize() * getClusterSize() / 2;
    const int r = static_cast<int>(radius);
    const int x0 = (-extent < x - r) ? x - r : -extent;
    const int x1 = (x + r > extent) ? extent : x + r;
    const int z0 = (-extent < z - r) ? z - r : -extent;
    const int z1 = (z + r <= extent) ? z + r : extent;

    const float step = m_primaryHeightmap->getScale().x;
    const int countZ = static_cast<int>(static_cast<float>(z1 - z0) / step);
    const int countX = static_cast<int>(static_cast<float>(x1 - x0) / step);

    Random generator;
    generator.m_seed = static_cast<unsigned int>(time(0)) & 0x7FFFFFFFu;

    const float startX = static_cast<float>(x0);
    const float startZ = static_cast<float>(z0);
    float sampleZ = startZ;
    for (int iz = 0; iz < countZ; ++iz)
    {
        float sampleX = startX;
        const float dz = sampleZ - static_cast<float>(z);
        const float dz2 = dz * dz;
        for (int ix = countX; ix > 0; --ix)
        {
            const float dx = sampleX - static_cast<float>(x);
            if (radius >= sqrtf(dx * dx + dz2))
                addWorldScaledHeightInWorldCoords(
                    sampleX, sampleZ, randomRange(generator, 0.0f, strength + strength) - strength,
                    flags);
            sampleX = sampleX + step;
        }
        sampleZ = sampleZ + step;
    }

    postHeightmapModified(startX, startZ, static_cast<float>(x1), static_cast<float>(z1));
}

// Stitches surrounding tiles to the primary heightmap and interpolates between coarse samples.
// The existing interpolation writes each span one coarse step beyond its source heights.
void HeightmapClusterEditable::stitchEdges()
{
    IHeightmap* primary = getHeightmapInSector(0, 0);
    int size;
    primary->getSize(size, size);
    const float half = static_cast<float>((size - 1) * static_cast<int>(primary->getScale().x) / 2);
    const float step = primary->getScale().x;
    const float low = -half;

    for (float z = low; z <= half; z = z + step)
        _setHeight(low, z, primary->getHeightInWorldCoords(low, z));
    for (float z = low; z <= half; z = z + step)
        _setHeight(half, z, primary->getHeightInWorldCoords(half, z));
    for (float x = low; x <= half; x = x + step)
        _setHeight(x, low, primary->getHeightInWorldCoords(x, low));
    for (float x = low; x <= half; x = x + step)
        _setHeight(x, half, primary->getHeightInWorldCoords(x, half));

    const float coarse = getHeightmapInSector(-1, -1)->getScale().x;

    // x = low, along z
    float previous = primary->getHeightInWorldCoords(low, low);
    for (float z = low + coarse; z <= half; z = z + coarse)
    {
        const float current = primary->getHeightInWorldCoords(low, z);
        for (float t = step; t < coarse; t = t + step)
            _setHeight(low, t + z, t / step * (current - previous) + previous);
        previous = current;
    }
    // x = half, along z
    previous = primary->getHeightInWorldCoords(half, low);
    for (float z = low + coarse; z <= half; z = z + coarse)
    {
        const float current = primary->getHeightInWorldCoords(half, z);
        for (float t = step; t < coarse; t = t + step)
            _setHeight(half, t + z, t / step * (current - previous) + previous);
        previous = current;
    }
    // z = low, along x
    previous = primary->getHeightInWorldCoords(low, low);
    for (float x = low + coarse; x <= half; x = x + coarse)
    {
        const float current = primary->getHeightInWorldCoords(x, low);
        for (float t = step; t < coarse; t = t + step)
            _setHeight(t + x, low, t / step * (current - previous) + previous);
        previous = current;
    }
    // z = half, along x
    previous = primary->getHeightInWorldCoords(low, half);
    for (float x = low + coarse; x <= half; x = x + coarse)
    {
        const float current = primary->getHeightInWorldCoords(x, half);
        for (float t = step; t < coarse; t = t + step)
            _setHeight(t + x, half, t / step * (current - previous) + previous);
        previous = current;
    }
}

// Blends a width-metre band between the primary tile and surrounding tiles.
// Walks paired boundary samples in 1 cm steps. Zero falloff adds engine-random height noise;
// otherwise uses the capped height-difference/falloff offset.
void HeightmapClusterEditable::smoothEdges(int width, float amplitude, float falloff)
{
    const float band = static_cast<float>(width);
    IHeightmap* primary = getHeightmapInSector(0, 0);
    int size;
    primary->getSize(size, size);
    --size;
    size *= static_cast<int>(primary->getScale().x);
    const float half = static_cast<float>(size / 2);
    const float lo = -half - band;
    const float hi = half + band;
    const float innerLow = band - half;
    const float innerHigh = half - band;

    std::vector<Vec3> outer;
    std::vector<Vec3> inner;
    Vec3 a;
    Vec3 b;

    for (float z = lo; z <= hi; z = z + 1.0f)                   // west: x = lo
    {
        a.x = lo;
        a.y = getHeightInWorldCoords(lo, z);
        a.z = z;
        const float clamped = std::min(std::max(z, innerLow), innerHigh);
        b.x = innerLow;
        b.y = getHeightInWorldCoords(innerLow, clamped);
        b.z = clamped;
        outer.push_back(a);
        inner.push_back(b);
    }
    for (float z = lo; z <= hi; z = z + 1.0f)                   // east: x = hi
    {
        a.x = hi;
        a.y = getHeightInWorldCoords(hi, z);
        a.z = z;
        const float clamped = std::min(std::max(z, innerLow), innerHigh);
        b.x = innerHigh;
        b.y = getHeightInWorldCoords(innerHigh, clamped);
        b.z = clamped;
        outer.push_back(a);
        inner.push_back(b);
    }
    for (float x = lo; x <= hi; x = x + 1.0f)                   // z = hi
    {
        a.x = x;
        a.y = getHeightInWorldCoords(x, hi);
        a.z = hi;
        const float clamped = std::min(std::max(x, innerLow), innerHigh);
        b.x = clamped;
        b.y = getHeightInWorldCoords(clamped, innerHigh);
        b.z = innerHigh;
        outer.push_back(a);
        inner.push_back(b);
    }
    for (float x = lo; x <= hi; x = x + 1.0f)                   // z = lo
    {
        a.x = x;
        a.y = getHeightInWorldCoords(x, lo);
        a.z = lo;
        const float clamped = std::min(std::max(x, innerLow), innerHigh);
        b.x = clamped;
        b.y = getHeightInWorldCoords(clamped, innerLow);
        b.z = innerLow;
        outer.push_back(a);
        inner.push_back(b);
    }

    for (unsigned int i = 0; i < outer.size(); ++i)
    {
        const Vec3& from = outer[i];
        const Vec3& to = inner[i];
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float dz = to.z - from.z;
        const float length = sqrtf(dx * dx + dy * dy + dz * dz);
        const float dirX = (from.x - to.x) / length;
        const float dirY = (from.y - to.y) / length;
        const float dirZ = (from.z - to.z) / length;

        for (float t = 0.01f; t < length - 0.01; t = t + 0.01f)
        {
            const float px = from.x - dirX * t;
            const float py = from.y - dirY * t;
            const float pz = from.z - dirZ * t;
            float offset;
            if (falloff == 0.0f)
            {
                offset = randomRange(Rand, -amplitude, amplitude);
            }
            else
            {
                offset = (fabsf(from.y - to.y) - falloff) / falloff;
                if (!(offset < 1.0f))
                    offset = 1.0f;
            }
            _setHeight(px, pz, offset + py);
        }
    }
}

// import and save

// ; Fill every surrounding tile from one square 8-bit
// .raw covering the 3 x 3 layout: side = 3 * tileSize - 2 (tiles share their border rows), the
// tile size taken from the first surrounding sector found in (z, x) order. The file must exist
// (line 300) and be exactly side^2 bytes (line 323); both are fatal, the answer false. Tile
// (sx, sz) reads rows (half + sz) * (size - 1) .. + size and columns (half + sx) * (size - 1) ..
// + size, one byte per sample, straight into its grid - the tile's depth is not consulted (the
// stock secondaries are 8-bit). No event, no modified flag.
bool HeightmapClusterEditable::importSurroundingRawHeightMap(std::string fileName)
{
    io::File file;
    file.open(fileName, (io::IFileSystem::FileMode)2, (io::IFileSystem::FileAccess)0, 0);
    if (!file.isOpen())
    {
        Debug(DEBUGTYPE_6, "Physics/HeightmapClusterEditable.cpp", 300, __FUNCTION__)
            << "TBM: File not found!";
        return false;
    }

    const int half = m_clusterSize / 2;
    IHeightmap* first = 0;
    for (int z = -(m_clusterSize / 2); z <= half && !first; ++z)
        for (int x = -half; x <= half && !first; ++x)
            if (x || z)
                first = getHeightmapInSector(x, z);

    int size;
    first->getSize(size, size);
    const int side = 3 * size - 2;
    const int expected = side * side;
    const unsigned int fileSize = static_cast<unsigned int>(file.getSize());
    if (fileSize != static_cast<unsigned int>(expected))
    {
        Debug(DEBUGTYPE_6, "Physics/HeightmapClusterEditable.cpp", 323, __FUNCTION__)
            << "TBM: file dont have the right size!, file size: " << fileSize
            << " : expected size: " << expected << " (" << side << "*" << side << ")";
        return false;
    }

    unsigned char* raw = new unsigned char[fileSize];
    file.read(raw, fileSize);

    for (int z = -half; z <= half; ++z)
    {
        for (int x = -half; x <= half; ++x)
        {
            if (!x && !z)
                continue;
            Heightmap* tile = static_cast<Heightmap*>(getHeightmapInSector(x, z));
            const int columnBase = (size - 1) * (half + x);
            const int rowBase = (half + z) * (size - 1);
            for (int row = 0; row < size; ++row)
            {
                const unsigned char* source = raw + columnBase + side * (row + rowBase);
                for (int column = 0; column < size; ++column)
                    static_cast<unsigned char*>(tile->m_heightData)[column + size * row] = source[column];
            }
        }
    }

    file.close();
    delete[] raw;
    return true;
}

// ; Write Heightdata.con and every tile's data.
//   `path` is the level path the .con will name ("Levels/<name>", a '/' appended unless it ends
//   in '/' or '\'); the files go under the setting Editor_LevelSavePath (+ '/').
//   The script - exactly the stock layout, including dice's "HeighmapCluster" spelling and the
//   blank lines - is built in an ostringstream: cluster size (getUsedGridSize), tile size, the
//   primary block (its .raw and .mat saved as it goes), then "rem --- secondary ---" and one block
//   per populated sector row by row (z, then x), each tile saved as HeightmapSecondary_<L|R><n>
//   <U|D><n>.raw (the digit is '0' + |coordinate|, one character), then the sea level. The .con
//   is opened for writing and the answer is whether the whole text went out.
bool HeightmapClusterEditable::save(const std::string& path)
{
    std::string levelPath(path);
    if (levelPath.length() != 0)
    {
        const char last = levelPath[levelPath.length() - 1];
        if (last != '/' && last != '\\')
            levelPath.append(std::string("/"));
    }
    std::string conName(levelPath);
    conName.append(std::string("Heightdata.con"));

    std::string saveRoot(settings->stringGet(std::string("Editor_LevelSavePath"), std::string(" ERROR ")));
    saveRoot.append("/");

    std::ostringstream out;
    out << "heightmapcluster.create HeighmapCluster" << std::endl;
    out << "heightmapcluster.setClusterSize " << getUsedGridSize() << std::endl;
    out << "heightmapcluster.setHeightmapSize " << static_cast<int>(m_heightmapSize) << std::endl;
    out << std::endl;

    int sizeX, sizeZ;
    m_primaryHeightmap->getSize(sizeX, sizeZ);
    out << "rem --- primary ---" << std::endl;
    out << "heightmapcluster.addHeightmap Heightmap 0 0" << std::endl;
    out << "heightmap.setSize " << sizeX << " " << sizeZ << std::endl;
    const Vec3& primaryScale = getPrimaryScale();
    out << "heightmap.setScale " << primaryScale.x << '/' << primaryScale.y << '/' << primaryScale.z
        << std::endl;
    out << "heightmap.setBitResolution " << m_primaryHeightmap->getBitResolution() << std::endl;
    out << "heightmap.setModified 1" << std::endl;
    std::string fileName = levelPath + "HeightmapPrimary.raw";
    out << "heightmap.loadHeightData " << fileName << std::endl;
    m_primaryHeightmap->saveHeightData(saveRoot + fileName);
    out << "heightmap.setMaterialScale " << m_primaryHeightmap->getMaterialScale() << std::endl;
    fileName.assign(levelPath + std::string("HeightmapPrimary.mat"));
    out << "heightmap.loadMaterialData " << fileName << std::endl;
    m_primaryHeightmap->saveMaterialData(saveRoot + fileName);

    const int used = getUsedGridSize() / 2;
    if (used > 0)
        out << std::endl << "rem --- secondary ---";
    for (int sectorZ = -used; sectorZ <= used; ++sectorZ)
    {
        out << std::endl;
        for (int sectorX = -used; sectorX <= used; ++sectorX)
        {
            if ((sectorX == 0 && sectorZ == 0) || !getHeightmapInSector(sectorX, sectorZ))
                continue;

            std::string name = levelPath + std::string("HeightmapSecondary_");
            char digit[2] = { static_cast<char>((sectorX < 0 ? -sectorX : sectorX) + '0'), 0 };
            if (sectorX < 0)
                name.append(std::string("L") + std::string(digit));
            else if (sectorX > 0)
                name.append(std::string("R") + std::string(digit));
            digit[0] = static_cast<char>((sectorZ < 0 ? -sectorZ : sectorZ) + '0');
            if (sectorZ < 0)
                name.append(std::string("U") + std::string(digit));
            else if (sectorZ > 0)
                name.append(std::string("D") + std::string(digit));

            IHeightmap* tile = getHeightmapInSector(sectorX, sectorZ);
            int tileX, tileZ;
            tile->getSize(tileX, tileZ);
            const bool modified = tile->getModified();
            out << "heightmapcluster.addHeightmap Heightmap " << sectorX << " " << sectorZ << std::endl;
            out << "heightmap.setSize " << tileX << " " << tileZ << std::endl;
            const Vec3& secondaryScale = getSecondaryScale();
            out << "heightmap.setScale " << secondaryScale.x << '/' << secondaryScale.y << '/'
                << secondaryScale.z << std::endl;
            out << "heightmap.setBitResolution "
                << getHeightmapInSector(sectorX, sectorZ)->getBitResolution() << std::endl;
            out << "heightmap.setModified " << modified << std::endl;
            out << "heightmap.loadHeightData " << name << std::string(".raw") << std::endl;
            tile->saveHeightData(saveRoot + name + std::string(".raw"));
        }
    }
    out << std::endl << "heightmapcluster.setSeaWaterLevel " << getSeaWaterLevel() << std::endl;

    io::IStream* stream = io::fileManager->open(saveRoot + conName, (io::IFileSystem::FileMode)0,
                                                (io::IFileSystem::FileAccess)2, 0);
    const std::string text = out.str();
    const bool complete = stream->write(text.c_str(), static_cast<unsigned int>(text.length()))
                          == out.str().length();
    stream->release();
    return complete;
}

} // namespace world
} // namespace hfe
} // namespace dice
