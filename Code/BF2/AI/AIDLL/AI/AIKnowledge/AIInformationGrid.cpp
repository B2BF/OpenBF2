
#include "AIInformationGrid.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2D74C0, __tcf_0 0x2D7520, _GLOBAL__I 0x2D7550).
#include <iostream>

#include "Information.h"
#include "IPIPhysical.h"
#include "InformationManager.h"
#include "../AIUtils/checkPointer.h"                    // and, through it, Core/Debug.h
#include "../../../../Math/Frustum.h"

namespace dice {
namespace hfe {
namespace ai {

// The interface's own destructor. , : a vptr store and a return, nothing else.
IAIInformationGrid::~IAIInformationGrid()
{
}

// The scalars, the map, the vector, then the cell array
// and the array is one cell longer than the grid can index. See the header.
AIInformationGrid::AIInformationGrid(int side, int dimX, int dimZ, Vec2 min, Vec2 max)
    : m_side(side)
    , m_dimX(dimX)
    , m_dimZ(dimZ)
    , m_min(min)
    , m_max(max)
    , m_cells(0)
{
    m_cells = checkPointer(new Cell[m_dimX * m_dimZ + 1]);
}

// The records the map still points at are not deleted - defect 2.
AIInformationGrid::~AIInformationGrid()
{
    delete[] m_cells;
}

// On an id it already holds this updates rather than declining, which makes it
// and `addAndUpdatePosition` nearly the same function - see the header.
void AIInformationGrid::addInfo(unsigned int id)
{
    Information* const information = informationManager->getInformation(id);
    if (!information)
        return;

    IPIPhysical* const physical = information->getPhysical();
    Vec3& position = information->getPosition();
    const float radius = physical ? physical->getRadius() : 0.0f;

    // Inserts a null first and fills it in on success: one tree walk, not two.
    const std::pair<InfoMap::iterator, bool> inserted =
        m_infos.insert(InfoMap::value_type(id, static_cast<InfoInfo*>(0)));

    if (inserted.second)
    {
        InfoInfo* const info = checkPointer(new InfoInfo);
        info->m_x = position.x;
        info->m_y = position.z;          // the Vec3's third float - this is an X/Z index
        info->m_radius = radius;
        info->m_visited = false;

        addElementToCells(id, info);
        inserted.first->second = info;
    }
    else
    {
        InfoInfo* const info = inserted.first->second;
        const InfoInfo old = *info;

        // Re-resolves the id and calls getPosition separately for each coordinate.
        // Preserve those virtual calls; the previously fetched radius is retained.
        Information* const again = informationManager->getInformation(id);
        if (again)
        {
            info->m_x = again->getPosition().x;
            info->m_y = again->getPosition().z;
            updateElementInCells(id, old, info);
        }
    }
}

// Removes information from its grid cells and deletes its record.
// A missing ID emits an error diagnostic; it is not a successful no-op.
void AIInformationGrid::removeInfo(unsigned int id)
{
    const InfoMap::iterator it = m_infos.find(id);
    if (it == m_infos.end())
    {
        Debug(DebugTypeError, __FILE__, __LINE__, "")
            << "Failed to remove info " << id << " " << m_side;
        return;
    }

    removeElementFromCells(id, it->second);
    delete it->second;
    m_infos.erase(it);
}

// Removes an entry when the predicate accepts it. Returns false only when the predicate
// refuses; an already absent id returns true.
bool AIInformationGrid::safeRemoveInfo(unsigned int id, IAIInformationGridPredicate* remove)
{
    const InfoMap::iterator it = m_infos.find(id);
    if (it == m_infos.end())
        return true;

    Information* const information = informationManager->getInformation(id);
    if (information && remove && !remove->includeInformation(information))
        return false;

    removeElementFromCells(id, it->second);
    delete it->second;
    m_infos.erase(it);
    return true;
}

// The one with a dead-band: nothing happens unless the thing moved a whole unit.
bool AIInformationGrid::updatePosition(unsigned int id)
{
    Information* const information = informationManager->getInformation(id);
    if (!information)
        return false;

    // Requires an id already present in this grid; the map lookup is not checked.
    InfoInfo* const info = m_infos.find(id)->second;

    const Vec3& position = information->getPosition();
    const float dx = info->m_x - position.x;
    const float dy = info->m_y - position.z;

    // Written in the binary's polarity on purpose: the work is guarded by `>= 1.0f`, so a NaN
    // distance does nothing. Spelling it `< 1.0f` and returning early would invert that.
    if (dx * dx + dy * dy >= 1.0f)
    {
        const InfoInfo old = *info;
        info->m_x = information->getPosition().x;
        info->m_y = information->getPosition().z;
        updateElementInCells(id, old, info);
        return true;
    }

    return false;
}

// `addInfo`'s two arms again, reached through a different map idiom: `find` first
// then `operator[]` on the miss, which walks the tree a second time to insert.
void AIInformationGrid::addAndUpdatePosition(unsigned int id)
{
    const InfoMap::iterator it = m_infos.find(id);
    if (it == m_infos.end())
    {
        Information* const information = informationManager->getInformation(id);
        if (!information)
            return;

        IPIPhysical* const physical = information->getPhysical();
        Vec3& position = information->getPosition();
        const float radius = physical ? physical->getRadius() : 0.0f;

        InfoInfo* const info = checkPointer(new InfoInfo);
        info->m_x = position.x;
        info->m_y = position.z;
        info->m_radius = radius;
        info->m_visited = false;

        addElementToCells(id, info);
        m_infos[id] = info;
    }
    else
    {
        InfoInfo* const info = it->second;
        const InfoInfo old = *info;

        Information* const information = informationManager->getInformation(id);
        if (information)
        {
            info->m_x = information->getPosition().x;
            info->m_y = information->getPosition().z;
            updateElementInCells(id, old, info);
        }
    }
}

// Maps a rectangle to clamped cell indices. Uses m_max as divisor, so the grid minimum
// must be the origin for the expected mapping. Negative coordinates clamp to zero.
void AIInformationGrid::getMinMax(int& minX, int& maxX, int& minY, int& maxY,
                                  const Vec2& min, const Vec2& max) const
{
    minX = static_cast<int>((min.x - m_min.x) * static_cast<float>(m_dimX) / m_max.x);
    maxX = static_cast<int>((max.x - m_min.x) * static_cast<float>(m_dimX) / m_max.x);
    minY = static_cast<int>((min.y - m_min.y) * static_cast<float>(m_dimZ) / m_max.y);
    maxY = static_cast<int>((max.y - m_min.y) * static_cast<float>(m_dimZ) / m_max.y);

    if (minX < 0) minX = 0;
    if (maxX < 0) maxX = 0;
    if (minY < 0) minY = 0;
    if (maxY < 0) maxY = 0;

    if (minX >= m_dimX) minX = m_dimX - 1;
    if (maxX >= m_dimX) maxX = m_dimX - 1;
    if (minY >= m_dimZ) minY = m_dimZ - 1;
    if (maxY >= m_dimZ) maxY = m_dimZ - 1;
}

// The bounding square picks the cells; the circle test then rejects the corners.
void AIInformationGrid::getInformationWithinCircle(std::vector<unsigned int>& out,
                                                   const Vec2& centre, float radius,
                                                   IAIInformationGridPredicate* keep) const
{
    Vec2 min;
    min.x = centre.x - radius;
    min.y = centre.y - radius;

    Vec2 max;
    max.x = centre.x + radius;
    max.y = centre.y + radius;

    int minX, maxX, minY, maxY;
    getMinMax(minX, maxX, minY, maxY, min, max);

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            Cell& cell = m_cells[x + m_dimX * y];
            for (Cell::iterator it = cell.begin(); it != cell.end(); ++it)
            {
                InfoInfo* const info = it->m_info;
                if (info->m_visited)
                    continue;

                info->m_visited = true;
                m_visited.push_back(info);

                Information* const information = informationManager->getInformation(it->m_id);
                if (information && (!keep || keep->includeInformation(information)))
                {
                    const float reach = radius + info->m_radius;
                    const float dx = centre.x - info->m_x;
                    const float dy = centre.y - info->m_y;
                    if (reach * reach > dx * dx + dy * dy)
                        out.push_back(it->m_id);
                }
            }
        }
    }

    for (std::vector<InfoInfo*>::iterator it = m_visited.begin(); it != m_visited.end(); ++it)
        (*it)->m_visited = false;

    m_visited.resize(0);
}

// Searches the X/Z bounds of the frustum corners, then checks the true 3-D positions
// against its six planes.
void AIInformationGrid::getInformationWithinFrustum(std::vector<unsigned int>& out,
                                                    const Frustum& frustum,
                                                    IAIInformationGridPredicate* keep) const
{
    Vec2 min;
    min.x = frustum.m_corners[0].x;
    min.y = frustum.m_corners[0].z;

    Vec2 max;
    max.x = frustum.m_corners[0].x;
    max.y = frustum.m_corners[0].z;

    for (int corner = 1; corner < 8; ++corner)
    {
        const Vec3& c = frustum.m_corners[corner];

        if (c.x < min.x)
            min.x = c.x;
        else if (c.x > max.x)
            max.x = c.x;

        if (c.z < min.y)
            min.y = c.z;
        else if (c.z > max.y)
            max.y = c.z;
    }

    int minX, maxX, minY, maxY;
    getMinMax(minX, maxX, minY, maxY, min, max);

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            Cell& cell = m_cells[x + m_dimX * y];
            for (Cell::iterator it = cell.begin(); it != cell.end(); ++it)
            {
                InfoInfo* const info = it->m_info;
                if (info->m_visited)
                    continue;

                info->m_visited = true;
                m_visited.push_back(info);

                Information* const information = informationManager->getInformation(it->m_id);
                if (information && (!keep || keep->includeInformation(information)))
                {
                    const float reach = info->m_radius;
                    const Vec3& p = information->getPosition();

                    if (frustum.m_planes[0].normal.x * p.x + frustum.m_planes[0].normal.y * p.y
                            + frustum.m_planes[0].normal.z * p.z + frustum.m_planes[0].d <= reach
                        && frustum.m_planes[1].normal.x * p.x + frustum.m_planes[1].normal.y * p.y
                            + frustum.m_planes[1].normal.z * p.z + frustum.m_planes[1].d <= reach
                        && frustum.m_planes[2].normal.x * p.x + frustum.m_planes[2].normal.y * p.y
                            + frustum.m_planes[2].normal.z * p.z + frustum.m_planes[2].d <= reach
                        && frustum.m_planes[3].normal.x * p.x + frustum.m_planes[3].normal.y * p.y
                            + frustum.m_planes[3].normal.z * p.z + frustum.m_planes[3].d <= reach
                        && frustum.m_planes[4].normal.x * p.x + frustum.m_planes[4].normal.y * p.y
                            + frustum.m_planes[4].normal.z * p.z + frustum.m_planes[4].d <= reach
                        && frustum.m_planes[5].normal.x * p.x + frustum.m_planes[5].normal.y * p.y
                            + frustum.m_planes[5].normal.z * p.z + frustum.m_planes[5].d <= reach)
                    {
                        out.push_back(it->m_id);
                    }
                }
            }
        }
    }

    for (std::vector<InfoInfo*>::iterator it = m_visited.begin(); it != m_visited.end(); ++it)
        (*it)->m_visited = false;

    m_visited.resize(0);
}

// Box against box, using the record's radius as its own box - so an element whose
// circle merely touches the query rectangle is included.
void AIInformationGrid::getInformationWithinRectangle(std::vector<unsigned int>& out,
                                                      const Vec2& min, const Vec2& max,
                                                      IAIInformationGridPredicate* keep) const
{
    int minX, maxX, minY, maxY;
    getMinMax(minX, maxX, minY, maxY, min, max);

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            Cell& cell = m_cells[x + m_dimX * y];
            for (Cell::iterator it = cell.begin(); it != cell.end(); ++it)
            {
                InfoInfo* const info = it->m_info;
                if (info->m_visited)
                    continue;

                info->m_visited = true;
                m_visited.push_back(info);

                Information* const information = informationManager->getInformation(it->m_id);
                if (information && (!keep || keep->includeInformation(information)))
                {
                    Vec2 lo;
                    lo.x = info->m_x - info->m_radius;
                    lo.y = info->m_y - info->m_radius;

                    Vec2 hi;
                    hi.x = info->m_x + info->m_radius;
                    hi.y = info->m_y + info->m_radius;

                    if (lo.x <= max.x && min.x <= hi.x && lo.y <= max.y && min.y <= hi.y)
                        out.push_back(it->m_id);
                }
            }
        }
    }

    for (std::vector<InfoInfo*>::iterator it = m_visited.begin(); it != m_visited.end(); ++it)
        (*it)->m_visited = false;

    m_visited.resize(0);
}

// Collects matching ids and removes those accepted by the removal predicate or no longer
// resolvable. With both predicates null, returns every id without resolving it; the
// one-predicate overload still resolves ids and drops failures.
void AIInformationGrid::getAllInformation(std::vector<unsigned int>& out,
                                          IAIInformationGridPredicate* keep,
                                          IAIInformationGridPredicate* cull)
{
    InfoMap::iterator it = m_infos.begin();
    while (it != m_infos.end())
    {
        if (!keep && !cull)
        {
            out.push_back(it->first);
            ++it;
            continue;
        }

        Information* const information = informationManager->getInformation(it->first);
        if (!information)
        {
            removeElementFromCells(it->first, it->second);
            delete it->second;
            m_infos.erase(it++);
            continue;
        }

        if (cull && cull->includeInformation(information))
        {
            removeElementFromCells(it->first, it->second);
            delete it->second;
            m_infos.erase(it++);
            continue;
        }

        if (!keep || keep->includeInformation(information))
            out.push_back(it->first);

        ++it;
    }
}

// Vtable slot 11. Read-only, and it validates every id it returns.
void AIInformationGrid::getAllInformation(std::vector<unsigned int>& out,
                                          IAIInformationGridPredicate* keep) const
{
    for (InfoMap::const_iterator it = m_infos.begin(); it != m_infos.end(); ++it)
    {
        Information* const information = informationManager->getInformation(it->first);
        if (!information)
            continue;

        if (keep && !keep->includeInformation(information))
            continue;

        out.push_back(it->first);
    }
}

// Two footprints, and if they cover the same cells it does nothing at all.
void AIInformationGrid::updateElementInCells(unsigned int id, const InfoInfo& old, InfoInfo* now)
{
    Vec2 oldMin;
    oldMin.x = old.m_x - old.m_radius;
    oldMin.y = old.m_y - old.m_radius;

    Vec2 oldMax;
    oldMax.x = old.m_x + old.m_radius;
    oldMax.y = old.m_y + old.m_radius;

    int oldMinX, oldMaxX, oldMinY, oldMaxY;
    getMinMax(oldMinX, oldMaxX, oldMinY, oldMaxY, oldMin, oldMax);

    Vec2 newMin;
    newMin.x = now->m_x - now->m_radius;
    newMin.y = now->m_y - now->m_radius;

    Vec2 newMax;
    newMax.x = now->m_x + now->m_radius;
    newMax.y = now->m_y + now->m_radius;

    int newMinX, newMaxX, newMinY, newMaxY;
    getMinMax(newMinX, newMaxX, newMinY, newMaxY, newMin, newMax);

    if (newMinX == oldMinX && newMaxX == oldMaxX && newMinY == oldMinY && newMaxY == oldMaxY)
        return;

    for (int y = oldMinY; y <= oldMaxY; ++y)
    {
        for (int x = oldMinX; x <= oldMaxX; ++x)
        {
            const int cell = x + m_dimX * y;
            m_cells[cell].erase(findCellElement(cell, id));
        }
    }

    GridCellElem element;
    element.m_id = id;
    element.m_info = now;

    for (int y = newMinY; y <= newMaxY; ++y)
    {
        for (int x = newMinX; x <= newMaxX; ++x)
        {
            Cell& cell = m_cells[x + m_dimX * y];
            cell.insert(cell.begin(), element);
        }
    }
}

// One element, stamped into every cell its circle's bounding square touches.
void AIInformationGrid::addElementToCells(unsigned int id, InfoInfo* info)
{
    Vec2 min;
    min.x = info->m_x - info->m_radius;
    min.y = info->m_y - info->m_radius;

    Vec2 max;
    max.x = info->m_x + info->m_radius;
    max.y = info->m_y + info->m_radius;

    int minX, maxX, minY, maxY;
    getMinMax(minX, maxX, minY, maxY, min, max);

    GridCellElem element;
    element.m_id = id;
    element.m_info = info;

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            Cell& cell = m_cells[x + m_dimX * y];
            cell.insert(cell.begin(), element);
        }
    }
}

// Removes an id from every cell in its recorded footprint. The caller must preserve the
// original footprint and cell membership; missing entries are erased without checking end().
void AIInformationGrid::removeElementFromCells(unsigned int id, InfoInfo* info)
{
    Vec2 min;
    min.x = info->m_x - info->m_radius;
    min.y = info->m_y - info->m_radius;

    Vec2 max;
    max.x = info->m_x + info->m_radius;
    max.y = info->m_y + info->m_radius;

    int minX, maxX, minY, maxY;
    getMinMax(minX, maxX, minY, maxY, min, max);

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            const int cell = x + m_dimX * y;
            m_cells[cell].erase(findCellElement(cell, id));
        }
    }
}

// A linear scan of one cell.
AIInformationGrid::Cell::iterator AIInformationGrid::findCellElement(int cell,
                                                                    unsigned int id) const
{
    Cell& list = m_cells[cell];

    Cell::iterator it = list.begin();
    while (it != list.end())
    {
        if (it->m_id == id)
            break;

        ++it;
    }

    return it;
}

} // namespace ai
} // namespace hfe
} // namespace dice
