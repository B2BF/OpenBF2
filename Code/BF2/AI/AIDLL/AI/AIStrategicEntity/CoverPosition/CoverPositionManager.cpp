// CoverPositionManager. Evidence: libaidll.so 1.5.3153 (ia-32), see CoverPositionManager.h.

#include "CoverPositionManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x359190, __tcf_0 0x3591F0, _GLOBAL__I 0x359220).
#include <iostream>

#include <cfloat>
#include <cmath>

#include "CPNode.h"
#include "CoverPosition.h"
#include "../../AIUtils/checkPointer.h"
#include "../../Main/AIRandom.h"

namespace dice {
namespace hfe {
namespace ai {

CoverPositionManager* CoverPositionManager::m_instance = 0;

// ELF 0x356680 (C2; C1 0x356F00): the engine, then five search resources (open tree 15, both
// hashes 20) whose ids go to the free list; the first manager becomes the singleton.
CoverPositionManager::CoverPositionManager()
{
    m_engine = checkPointer(new CPPathfindingEngine());

    for (int i = 4; i >= 0; --i)
    {
        CPPathfindingEngine* const pool = m_engine;

        CPAStarResource* const resource = new CPAStarResource(checkPointer(new CPNodeManager()));
        checkPointer(resource);

        pool->m_searches.push_back(resource);

        resource->m_iterations = 0;
        resource->m_openSize = 15;
        resource->m_hashSize = 20;

        resource->m_open   = checkPointer(new BinarySearchTree<CPNode, CPNode>(15));
        resource->m_openOverflow = checkPointer(new Hash<CPNode, CPNodeManager, CPNode>(20));
        resource->m_closed  = checkPointer(new Hash<CPNode, CPNodeManager, CPNode>(20));

        resource->m_openOverflow->m_nodeManager = resource->m_nodeManager;
        resource->m_closed->m_nodeManager  = resource->m_nodeManager;

        pool->m_available.push_back((unsigned int)(pool->m_searches.size() - 1));
    }

    if (!m_instance)
        m_instance = this;
}

// ELF 0x357680 (D1, 337 B; recovered on the client track, docs/macos-c-singleplayer.md): the
// singleton is zeroed when it is this one, the CPNode pathfinding engine is deleted, every cover
// position is deleted (CoverPosition D1 0x352EA0 destroys members only) and the vector emptied;
// the distance table and the position vector then go as members.
CoverPositionManager::~CoverPositionManager()
{
    if (m_instance == this)
        m_instance = 0;
    if (m_engine)
        delete m_engine;
    for (unsigned int i = 0; i < m_coverPositions.size(); ++i)
        delete m_coverPositions[i];
    m_coverPositions.erase(m_coverPositions.begin(), m_coverPositions.end());
}

// ELF 0x357CF0: `return m_coverPositions.back();`, with no empty check. On an empty
// vector it reads one element in front of the buffer. Shipped; preserved.
CoverPosition* CoverPositionManager::getLastCoverPosition() const
{
    return m_coverPositions.back();
}

// ELF 0x3577E0. The id is the index; the new table row has one zero per position including the
// new one, then the distances to the earlier positions are filled in.
void CoverPositionManager::createCoverPosition(const Vec2& a, const Vec2& b)
{
    Vec2 low = a;
    Vec2 high = b;
    if (high.x < low.x)
    {
        high.x = a.x;
        low.x = b.x;
    }
    if (high.y < low.y)
    {
        high.y = a.y;
        low.y = b.y;
    }
    CoverPosition* const position = checkPointer(new CoverPosition(low, high));
    const int id = static_cast<int>(m_coverPositions.size());
    position->m_id = id;
    m_coverPositions.push_back(position);
    m_distances.push_back(std::vector<float>(id + 1, 0.0f));
    for (int i = 0; i < id; ++i)
        m_distances[id][i] = position->getShortestDistance(m_coverPositions[i]);
}

// ELF 0x357D10. One random draw per axis.
void CoverPositionManager::RandomizeCoverPos(Vec2 low, Vec2 high, Vec2& out)
{
    out.x = (high.x - low.x - 2.0f) * AIRandom::m_instance->nextFloat() + low.x + 1.0f;
    out.y = (high.y - low.y - 2.0f) * AIRandom::m_instance->nextFloat() + low.y + 1.0f;
}

// ELF 0x357E70. A first draw picks the edge direction (at least 0.5: along y); the second the
// offset. The step is taken away from the far side when it would leave the box, and the other
// axis moves 1 inward the same way. Shipped asymmetry, preserved: along x the back step is
// `d - 1`, along y it is `d`.
void CoverPositionManager::RandomizeCoverPosAlongEdge(Vec2 low, Vec2 high, Vec2& out)
{
    if (AIRandom::m_instance->nextFloat() >= 0.5f)
    {
        const float d = (high.y - low.y) * AIRandom::m_instance->nextFloat() * 0.5f;
        float y = out.y + d;
        if (high.y < y)
            y = out.y - d;
        out.y = y;
        float x = out.x + 1.0f;
        if (high.x < x)
            x = out.x - 1.0f;
        out.x = x;
    }
    else
    {
        const float d = (high.x - low.x) * AIRandom::m_instance->nextFloat() * 0.5f;
        if (out.x + d <= high.x)
            out.x = out.x + (d + 1.0f);
        else
            out.x = out.x - (d - 1.0f);
        float y = out.y + 1.0f;
        if (high.y < y)
            y = out.y - 1.0f;
        out.y = y;
    }
}

// ELF 0x3580E0: the first cover position whose box (+0 min, +8 max, bounds inclusive) holds the
// point, or null. The image answers the pointer (`mov (%edx),%edx; mov %edx,-0x40`), not a bool.
CoverPosition* CoverPositionManager::isInCoverPosition(Vec2 point)
{
    for (unsigned int i = 0; i < m_coverPositions.size(); ++i)
    {
        const Vec2 low = m_coverPositions[i]->m_min;
        const Vec2 high = m_coverPositions[i]->m_max;
        if (point.x >= low.x && point.y >= low.y && high.x >= point.x && high.y >= point.y)
            return m_coverPositions[i];
    }
    return 0;
}

// ELF 0x3581B0: a cover spot for attacking `to` from `from`. Only when the two are more than 5
// apart (squared distance > 25): the answer starts as `from`; the reference point is
// from + (to - from) * fraction / 4. Each position whose centre is closer to it than the best so
// far, within maxDistance and within half the squared from-to distance, is found (answer true):
// when `from` is inside its box the answer is `from` (best 0); otherwise the centre, randomized in
// the box (RandomizeCoverPos) - or, when `to` is itself in cover and the position sees that cover
// best from a corner (findHighestVisibilityCorner), a point along that edge.
bool CoverPositionManager::getClosestAttackCoverPosition(const Vec2& from, const Vec2& to, float maxDistance,
                                                         float fraction, Vec2& out)
{
    bool found = false;
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float distance = dy * dy + dx * dx;
    if (!(distance > 25.0f))
        return found;
    float best = 3.40282347e+38f;
    out.y = from.y;
    out.x = from.x;
    Vec2 reference;
    reference.x = (to.x - from.x) * fraction * 0.25f + from.x;
    reference.y = (to.y - from.y) * fraction * 0.25f + from.y;
    CoverPosition* const targetCover = isInCoverPosition(to);
    for (unsigned int i = 0; i < m_coverPositions.size(); ++i)
    {
        CoverPosition* const cover = m_coverPositions[i];
        const Vec2 low = cover->m_min;
        const Vec2 high = cover->m_max;
        Vec2 centre;
        centre.x = low.x + (high.x - low.x) * 0.5f;
        centre.y = low.y + (high.y - low.y) * 0.5f;
        const float cx = centre.x - reference.x;
        const float cy = centre.y - reference.y;
        const float d = cx * cx + cy * cy;
        if (!(d < best) || !(d < maxDistance * maxDistance) || !(d < distance * 0.5f))
            continue;
        found = true;
        if (!(from.x < low.x || from.y < low.y || high.x < from.x || high.y < from.y))
        {
            out = from;
            best = 0.0f;
            continue;
        }
        best = d;
        if (targetCover)
        {
            if (cover->findHighestVisibilityCorner(targetCover, out))
            {
                RandomizeCoverPosAlongEdge(low, high, out);
                continue;
            }
        }
        out = centre;
        RandomizeCoverPos(low, high, out);
    }
    return found;
}

// ELF 0x358610. A centre qualifies when it is closer to `from` than the best so far (and more than
// 0.001 away), closer to `to` than `from` is, closer to `from` than `to` is, and - when `from` is
// in cover - seen from that cover (CoverPosition::canSee, asked before the distance tests). The
// answer is the distance to the chosen centre, or from `from` to `to`.
float CoverPositionManager::getClosestNavCoverPosition(Vec2 from, Vec2 to, Vec2& out,
                                                       CoverPosition*& cover)
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float total = dy * dy + dx * dx;
    float best = FLT_MAX;
    out.x = to.x;
    out.y = to.y;
    bool found = false;
    CoverPosition* const current = isInCoverPosition(from);
    cover = 0;
    for (unsigned int i = 0; i < m_coverPositions.size(); ++i)
    {
        CoverPosition* const position = m_coverPositions[i];
        const Vec2 low = position->m_min;
        const Vec2 high = position->m_max;
        Vec2 centre;
        centre.x = low.x + (high.x - low.x) * 0.5f;
        centre.y = low.y + (high.y - low.y) * 0.5f;
        bool visible = true;
        const float d = (centre.x - from.x) * (centre.x - from.x) + (centre.y - from.y) * (centre.y - from.y);
        const float rest = (to.y - centre.y) * (to.y - centre.y) + (to.x - centre.x) * (to.x - centre.x);
        if (current != 0)
            visible = current->canSee(position);
        if (d < best && d > 0.001f && rest < total && d < total && visible)
        {
            cover = position;
            out = centre;
            found = true;
            best = d;
        }
    }
    if (!found)
        best = (to.y - from.y) * (to.y - from.y) + (to.x - from.x) * (to.x - from.x);
    return sqrtf(best);
}

// ELF 0x3588A0.
void CoverPositionManager::finaliseCoverPositions()
{
    for (std::vector<CoverPosition*>::iterator it = m_coverPositions.begin();
         it != m_coverPositions.end(); ++it)
    {
        (*it)->updateAttacks(true);
        (*it)->updateMoveNeighbours();
    }
}

// ELF 0x358900: the engine's update, inlined.
void CoverPositionManager::updatePathfinding(double budget)
{
    m_engine->update(budget);
}

// ELF 0x358A40. Inlined, in order: take the first free resource for owner 1 (as
// AIPathfinding::acquireResource, without the locking test; none free gives index UINT_MAX, which
// retail then uses), size the node flags, take a start and a goal node from the pool, reset the
// search (counters, open tree, both hashes, node flags), then either finish at once when both name
// the same position (the goal node goes back to the pool) or record the goal, return its node to
// the pool and open the start. One engine update of 10000 then runs; an owned resource is
// released as AIPathfinding::releaseResource does. The start node is not returned.
bool CoverPositionManager::search(int start, int goal)
{
    unsigned int resource;
    std::list<unsigned int>& available = m_engine->m_available;
    if (available.empty())
        resource = 0xFFFFFFFFu;
    else
    {
        std::list<unsigned int>::iterator first = available.begin();
        resource = *first;
        CPAStarResource* const candidate = m_engine->m_searches[resource];
        if (candidate->m_owner < 0)
        {
            candidate->m_owner = 1;
            available.erase(first);
            m_engine->m_inFlight.insert(m_engine->m_inFlight.end(), resource);
        }
        else
            resource = 0xFFFFFFFFu;
    }

    CPNodeManager* const manager = m_engine->m_searches[resource]->m_nodeManager;
    manager->init();
    CPNode* const startNode = manager->getNewNode(0, start);
    CPNode* const goalNode = manager->getNewNode(0, goal);

    CPAStarResource* search = m_engine->m_searches[resource];
    search->m_iterations = 0;
    search->m_status = astarStatus::kIdle;
    search->m_worstOpen = 0;
    search->m_open->clear();
    search->m_openOverflow->clear();
    search->m_closed->clear();
    search->m_nodeManager->reset();

    search = m_engine->m_searches[resource];
    if (startNode->m_key == goalNode->m_key)
    {
        goalNode->m_next = search->m_nodeManager->m_nodes;
        search->m_nodeManager->m_nodes = goalNode;
        search->m_status = astarStatus::kFound;
        search->m_goal = startNode;
    }
    else
    {
        search->m_nodeManager->m_goal = goalNode->m_key->m_id;
        goalNode->m_next = search->m_nodeManager->m_nodes;
        search->m_nodeManager->m_nodes = goalNode;
        search->pushOpenNode(startNode);
        search->m_status = astarStatus::kSearching;
    }

    m_engine->update(10000.0);

    search = m_engine->m_searches[resource];
    const int status = search->m_status;
    if (search->m_owner == 1)
    {
        search->m_owner = -1;
        search = m_engine->m_searches[resource];
        search->m_maxIterations = 0;
        search->m_maxSearchDepth = -1;
        search->m_nodeManager->returnReset();
        search->m_status = astarStatus::kIdle;
        search->m_owner = -1;
        m_engine->m_available.insert(m_engine->m_available.end(), resource);
        m_engine->m_inFlight.remove(resource);
    }
    return status == astarStatus::kFound;
}

} // namespace ai
} // namespace hfe
} // namespace dice
