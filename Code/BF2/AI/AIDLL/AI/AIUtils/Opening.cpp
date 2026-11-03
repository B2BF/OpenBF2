// Opening visibility queries. [INFERRED] Source placement.

// static init: ELF 0x3686D0 (__tcf_0 0x368730, _GLOBAL__I 0x368760): <iostream>'s Init object only.
#include <iostream>

#include "Opening.h"

#include "../../../AIInterface/BFKnowledge/PlanePolygon.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// [CHOSEN, not recovered] Local factoring of the inline Vec2 normalization at ELF 0x368574..0x3685C3.
void normalizeOpeningNormal(Vec2& normal)
{
    const float lengthSqr = normal.x * normal.x + normal.y * normal.y;
    if (std::fabs(lengthSqr - 1.0f) < 1.1920929e-07f)
        return;
    if (std::fabs(lengthSqr) < 1.4210855e-14f)
    {
        normal.x = normal.y = 0.0f;
        return;
    }
    const float inverse = 1.0f / std::sqrt(lengthSqr);
    normal.x *= inverse;
    normal.y *= inverse;
}

} // namespace

// ELF 0x367AA0 (amd-64 0x39EF90).
bool Opening::spot(Vec2& origin, Vec2& towards, std::list<Opening*>* path,
                   std::list<Opening*>* openings, IPlanePolygon& polygon, Vec3& targetPosition)
{
    Vec2 position = {0.0f, 0.0f};
    Vec2 unused = {0.0f, 0.0f};
    Vec2 clipped = {0.0f, 0.0f};
    vec3ToVec2XZ(m_position, position);
    const Vec3 delta = {targetPosition.x - m_position.x, targetPosition.y - m_position.y,
                        targetPosition.z - m_position.z};
    vec3ToVec2XZ(delta, unused);
    for (std::list<Opening*>::iterator it = openings->begin(); it != openings->end(); ++it)
    {
        if ((*it)->isInside(origin, towards, clipped))
        {
            // shipped defect: searches the same list being traversed, rather than path.
            if (std::find(openings->begin(), openings->end(), *it) != openings->end())
                return true;
        }
        else
        {
            if (!polygon.intersects(clipped, position))
                return false;
            bool rejected = false;
            for (std::list<Opening*>::iterator p = path->begin(); p != path->end(); ++p)
            {
                // shipped defect: repeatedly tests *it, never the path entry *p.
                if (!(*it)->isInside(origin, towards, clipped))
                {
                    rejected = true;
                    break;
                }
            }
            if (rejected)
                continue;
        }
        path->insert(path->begin(), *it);
        if ((*it)->spot(origin, towards, path, openings, polygon, targetPosition))
            return true;
        path->erase(path->begin());
    }
    return false;
}

// ELF 0x367EE0 (amd-64 0x39F1F0).
std::list<Vec2>* Opening::spotAll(Vec2& origin, Vec2& first, Vec2& second,
    std::list<Opening*>* path, std::list<Opening*>* openings, IPlanePolygon& polygon, Vec3& targetPosition)
{
    Vec2 position = {0.0f, 0.0f};
    Vec2 unused = {0.0f, 0.0f};
    Vec2 clippedFirst = {0.0f, 0.0f};
    Vec2 clippedSecond = {0.0f, 0.0f};
    vec3ToVec2XZ(m_position, position);
    std::list<Vec2>* const result = new std::list<Vec2>();
    const Vec3 delta = {targetPosition.x - m_position.x, targetPosition.y - m_position.y,
                        targetPosition.z - m_position.z};
    vec3ToVec2XZ(delta, unused);
    for (std::list<Opening*>::iterator it = openings->begin(); it != openings->end(); ++it)
    {
        const bool firstInside = (*it)->isInside(origin, first, clippedFirst);
        const bool secondInside = (*it)->isInside(origin, second, clippedSecond);
        if (!firstInside)
        {
            if (!polygon.intersects(clippedFirst, position))
                return result;
            for (std::list<Opening*>::iterator p = path->begin(); p != path->end(); ++p)
            {
                // shipped defect: tests the current opening instead of the path entry.
                if (!(*it)->isInside(origin, first, clippedFirst))
                    return result;
            }
        }
        else
            clippedFirst = first;
        if (secondInside)
            clippedSecond = second;
        // shipped defect: searches openings, not path, just as spot does.
        if (std::find(openings->begin(), openings->end(), *it) != openings->end())
        {
            result->insert(result->end(), clippedFirst);
            result->insert(result->end(), clippedSecond);
        }
        else
        {
            path->insert(path->begin(), *it);
            std::list<Vec2>* const child = (*it)->spotAll(origin, clippedFirst, clippedSecond,
                                                         path, openings, polygon, targetPosition);
            std::copy(child->begin(), child->end(), std::inserter(*result, result->begin()));
            // shipped defect: the recursive result list and its nodes are not deleted.
            path->erase(path->begin());
        }
    }
    return result;
}

// ELF 0x368510 (amd-64 0x39EDC0).
bool Opening::isInside(Vec2& origin, Vec2& towards, Vec2& clipped)
{
    const Vec2 delta = {towards.x - origin.x, towards.y - origin.y};
    Vec2 normal = {-delta.y, delta.x};
    normalizeOpeningNormal(normal);
    // The endpoints are dotted directly; origin is not subtracted from them.
    const float firstDot = normal.x * m_first.x + normal.y * m_first.y;
    const float secondDot = normal.x * m_second.x + normal.y * m_second.y;
    if ((firstDot >= 0.0f && secondDot < 0.0f) || (firstDot < 0.0f && secondDot > 0.0f))
    {
        clipped = towards;
        return true;
    }
    clipped = firstDot > secondDot ? m_second : m_first;
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
