// CoverPosition. Evidence: libaidll.so 1.5.3153 (ia-32), see CoverPosition.h.

#include "CoverPosition.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x354C20, __tcf_0 0x354C80, _GLOBAL__I 0x354CB0).
#include <iostream>

#include <cmath>

#include "CoverPositionManager.h"
#include "../../Main/CommonSingletonServer.h"
#include "../../../../AIInterface/BFKnowledge/AIAdapterTypes.h"        // AIObjectPredicate
#include "../../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"   // IAIObjectEnvironment
#include "../../../../AIInterface/BFKnowledge/BFTerrain.h"             // IAITerrain
#include "../../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// Lets only mobile objects block a line (vtable ELF 0x4BF780, typeinfo names this TU's anonymous
// namespace).
class MobilePredicate : public AIObjectPredicate
{
public:
    // ELF 0x356580 (weak): IAIObject +0x70.
    virtual bool predicate(IAIObject* object) { return object->isMobile(); }
};

} // namespace

// ELF 0x352AE0 (C2; C1 0x352C90): the corners; the id is left for the manager.
CoverPosition::CoverPosition(const Vec2& min, const Vec2& max)
    : m_min(min)
    , m_max(max)
{
}

// ELF 0x352EA0 (D1; D2 0x352D80): the set and the two maps go as members.
CoverPosition::~CoverPosition()
{
}

// ELF 0x352FC0. Shipped: the found iterator is dereferenced without an end check.
bool CoverPosition::canSee(CoverPosition* other)
{
    std::map<int, ConnectionType>::iterator it = m_attackable.find(other->m_id);
    return it->second.m_type != ConnectionType::kBlocked &&
           it->second.m_type != ConnectionType::kUnknown;
}

// ELF 0x353020. The candidates are this position's corners in Corner order; the first strictly
// better corner value wins, starting from kBlocked.
bool CoverPosition::findHighestVisibilityCorner(CoverPosition* other, Vec2& corner)
{
    bool found = false;
    std::map<int, ConnectionType>::iterator it = m_attackable.find(other->m_id);
    if (it != m_attackable.end())
    {
        found = true;
        int best = ConnectionType::kBlocked;
        std::vector<Vec2> corners;
        Vec2 point = m_min;
        corners.push_back(point);
        point.y = m_max.y;
        corners.push_back(point);
        point.x = m_max.x;
        corners.push_back(point);
        point.y = m_min.y;
        corners.push_back(point);
        corner = point;
        for (unsigned int i = 0; i < it->second.m_corners.size(); ++i)
        {
            if (it->second.m_corners[i] < best)
            {
                best = it->second.m_corners[i];
                corner = corners[i];
            }
        }
    }
    return found;
}

// ELF 0x353250. As findHighestVisibilityCorner, starting from kVisible and taking the first
// strictly worse corner.
bool CoverPosition::findLowestVisibilityCorner(CoverPosition* other, Vec2& corner)
{
    bool found = false;
    std::map<int, ConnectionType>::iterator it = m_attackable.find(other->m_id);
    if (it != m_attackable.end())
    {
        found = true;
        int worst = ConnectionType::kVisible;
        std::vector<Vec2> corners;
        Vec2 point = m_min;
        corners.push_back(point);
        point.y = m_max.y;
        corners.push_back(point);
        point.x = m_max.x;
        corners.push_back(point);
        point.y = m_min.y;
        corners.push_back(point);
        corner = point;
        for (unsigned int i = 0; i < it->second.m_corners.size(); ++i)
        {
            if (it->second.m_corners[i] > worst)
            {
                worst = it->second.m_corners[i];
                corner = corners[i];
            }
        }
    }
    return found;
}

// ELF 0x353480.
bool CoverPosition::isReachableNeighbour(int index) const
{
    return m_moveNeighbours.find(index) != m_moveNeighbours.end();
}

// ELF 0x3534D0.
bool CoverPosition::isAttackableNeighbour(int index) const
{
    return m_attackable.find(index) != m_attackable.end();
}

// ELF 0x353520.
bool CoverPosition::isDefendableNeighbour(int index) const
{
    return m_defendable.find(index) != m_defendable.end();
}

// ELF 0x353570. Per axis the facing edges when the boxes are apart, else the larger minimum on
// both sides.
float CoverPosition::getShortestDistance(CoverPosition* other) const
{
    float x0;
    float x1;
    if (m_min.x > other->m_max.x)
    {
        x0 = m_min.x;
        x1 = other->m_max.x;
    }
    else if (other->m_min.x > m_max.x)
    {
        x0 = m_max.x;
        x1 = other->m_min.x;
    }
    else
    {
        x0 = other->m_min.x < m_min.x ? m_min.x : other->m_min.x;
        x1 = x0;
    }

    float y0;
    float y1;
    if (m_min.y > other->m_max.y)
    {
        y0 = m_min.y;
        y1 = other->m_max.y;
    }
    else if (other->m_min.y > m_max.y)
    {
        y0 = m_max.y;
        y1 = other->m_min.y;
    }
    else
    {
        y0 = other->m_min.y < m_min.y ? m_min.y : other->m_min.y;
        y1 = y0;
    }

    return sqrtf((y1 - y0) * (y1 - y0) + (x1 - x0) * (x1 - x0));
}

// ELF 0x353660. The end is re-read after every call.
void CoverPosition::updateAttacks(bool force)
{
    std::vector<CoverPosition*>& positions = CoverPositionManager::m_instance->m_coverPositions;
    for (std::vector<CoverPosition*>::iterator it = positions.begin(); it != positions.end(); ++it)
    {
        if (*it != this)
            updateAttacks(*it, force);
    }
}

// ELF 0x3536C0 (4.8 KB, most of it the inlined ConnectionType copies). The target corners go
// (min, min), (max, min), (max, max), (min, max); the looking corners in Corner order. The
// overall attack value starts as the first corner's; afterwards it stays only while the corners
// agree and becomes kPartial otherwise. The defend value follows the corners with
// visible -> visible, partial -> partial unless already visible, blocked -> unchanged. Shipped,
// preserved: a link created by this call is left at its defaults unless `force` is set.
void CoverPosition::updateAttacks(CoverPosition* other, bool force)
{
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    Vec3 eye = { 0.0f, 0.0f, 0.0f };

    Vec3 target0;
    target0.x = other->m_min.x;
    target0.y = terrain->getHeight(other->m_min.x, other->m_min.y) + 0.3f;
    target0.z = other->m_min.y;
    Vec3 target1;
    target1.x = other->m_max.x;
    target1.y = terrain->getHeight(other->m_max.x, other->m_min.y) + 0.3f;
    target1.z = other->m_min.y;
    Vec3 target2;
    target2.x = other->m_max.x;
    target2.y = terrain->getHeight(other->m_max.x, other->m_max.y) + 0.3f;
    target2.z = other->m_max.y;
    Vec3 target3;
    target3.x = other->m_min.x;
    target3.y = terrain->getHeight(other->m_min.x, other->m_max.y) + 0.3f;
    target3.z = other->m_max.y;

    std::pair<std::map<int, ConnectionType>::iterator, bool> attackLink =
        m_attackable.insert(std::make_pair(other->m_id, ConnectionType()));
    if (!force && attackLink.second)
        return;

    std::map<int, ConnectionType>::iterator defendLink =
        other->m_defendable.insert(std::make_pair(m_id, ConnectionType())).first;
    ConnectionType& attack = attackLink.first->second;
    ConnectionType& defend = defendLink->second;

    eye.x = m_min.x;
    eye.z = m_min.y;
    eye.y = terrain->getHeight(m_min.x, m_min.y) + 1.5f;
    updateCorner(eye, target0, target1, target2, target3, attack, defend, ConnectionType::kCorner0);
    eye.x = m_min.x;
    eye.z = m_max.y;
    eye.y = terrain->getHeight(m_min.x, m_max.y) + 1.5f;
    updateCorner(eye, target0, target1, target2, target3, attack, defend, ConnectionType::kCorner1);
    eye.x = m_max.x;
    eye.z = m_max.y;
    eye.y = terrain->getHeight(m_max.x, m_max.y) + 1.5f;
    updateCorner(eye, target0, target1, target2, target3, attack, defend, ConnectionType::kCorner2);
    eye.x = m_max.x;
    eye.z = m_min.y;
    eye.y = terrain->getHeight(m_max.x, m_min.y) + 1.5f;
    updateCorner(eye, target0, target1, target2, target3, attack, defend, ConnectionType::kCorner3);

    for (int i = 0; i < 4; ++i)
    {
        if (attack.m_type == ConnectionType::kUnknown)
        {
            switch (attack.m_corners[i])
            {
            case ConnectionType::kVisible:
                attack.m_type = ConnectionType::kVisible;
                defend.m_type = ConnectionType::kVisible;
                break;
            case ConnectionType::kPartial:
                attack.m_type = ConnectionType::kPartial;
                defend.m_type = ConnectionType::kPartial;
                break;
            case ConnectionType::kBlocked:
                attack.m_type = ConnectionType::kBlocked;
                defend.m_type = ConnectionType::kBlocked;
                break;
            default:
                Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/CoverPosition/CoverPosition.cpp", 393, "")
                    << "Unknown connectiontype.";
                break;
            }
        }
        else
        {
            attack.m_type = attack.m_corners[i] == attack.m_type ? attack.m_corners[i]
                                                                 : ConnectionType::kPartial;
            switch (attack.m_corners[i])
            {
            case ConnectionType::kVisible:
                defend.m_type = ConnectionType::kVisible;
                break;
            case ConnectionType::kPartial:
                defend.m_type = defend.m_type != ConnectionType::kVisible ? ConnectionType::kPartial
                                                                          : ConnectionType::kVisible;
                break;
            case ConnectionType::kBlocked:
                break;
            default:
                Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/CoverPosition/CoverPosition.cpp", 424, "")
                    << "Unknown connectiontype.";
                break;
            }
        }
    }
}

// ELF 0x3549A0. Each line is tested with IAIObjectEnvironment::collideLineWithWorld (+0x44, only
// mobile objects block) and stored in both links; the target corners land in line slots 0, 3, 2
// and 1.
int CoverPosition::updateCorner(const Vec3& eye, const Vec3& corner0, const Vec3& corner1,
                                const Vec3& corner2, const Vec3& corner3, ConnectionType& attack,
                                ConnectionType& defend, const ConnectionType::Corner& corner)
{
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    MobilePredicate predicate;
    int clear = 0;

    bool visible = !environment->collideLineWithWorld(eye, corner0, &predicate, 0);
    ConnectionType::NeighbourConnection line = visible ? ConnectionType::kVisible : ConnectionType::kBlocked;
    attack.m_lines[corner][0] = line;
    defend.m_lines[corner][0] = line;
    if (visible)
        ++clear;

    visible = !environment->collideLineWithWorld(eye, corner1, &predicate, 0);
    line = visible ? ConnectionType::kVisible : ConnectionType::kBlocked;
    attack.m_lines[corner][3] = line;
    defend.m_lines[corner][3] = line;
    if (visible)
        ++clear;

    visible = !environment->collideLineWithWorld(eye, corner2, &predicate, 0);
    line = visible ? ConnectionType::kVisible : ConnectionType::kBlocked;
    attack.m_lines[corner][2] = line;
    defend.m_lines[corner][2] = line;
    if (visible)
        ++clear;

    visible = !environment->collideLineWithWorld(eye, corner3, &predicate, 0);
    line = visible ? ConnectionType::kVisible : ConnectionType::kBlocked;
    attack.m_lines[corner][1] = line;
    defend.m_lines[corner][1] = line;
    if (visible)
        ++clear;

    if (clear == 0)
        attack.m_corners[corner] = ConnectionType::kBlocked;
    else if (clear == 4)
        attack.m_corners[corner] = ConnectionType::kVisible;
    else
        attack.m_corners[corner] = ConnectionType::kPartial;
    defend.m_corners[corner] = attack.m_corners[corner];
    return clear;
}

// ELF 0x354C10: empty.
void CoverPosition::updateMoveNeighbours()
{
}

} // namespace ai
} // namespace hfe
} // namespace dice
