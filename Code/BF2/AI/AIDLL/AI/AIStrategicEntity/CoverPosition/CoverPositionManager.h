// Owns the cover positions, their distance table and the cover-position A* engine.
// Evidence: libaidll.so 1.5.3153 (ia-32), ELF 0x356680..0x359220 plus the engine's CPNode template
// instantiations up to 0x35BA70. [INFERRED] Placement
// AIStrategicEntity/CoverPosition/CoverPositionManager.cpp (the static-init symbol names
// CoverPositionManager::m_instance).

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_COVERPOSITIONMANAGER_H   // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_COVERPOSITIONMANAGER_H

#include <set>
#include <vector>

#include "../../../../../Math/Vec3.h"           // Vec2 and Vec3 are typedefs, not classes
#include "../../Main/AIPathfindingSearchTypes.h"  // PathfindingEngine, AStarResource, BinarySearchTree, Hash
#include "CPNodeManager.h"

namespace dice {
namespace hfe {
namespace ai {

class CoverPosition;
class CPNode;

// The search key is the move-neighbour iterator CPNode::expandFirst / expand advance
// (std::_Rb_tree_iterator<int, int const&, int const*> in the mangled names: libstdc++'s
// set<int> iterator); the path type is vector<int>.
typedef PathfindingEngine<CPNode, CPNodeManager, CPNode, std::set<int>::const_iterator, std::vector<int> >
        CPPathfindingEngine;
typedef AStarResource<CPNode, CPNodeManager, CPNode, std::set<int>::const_iterator, std::vector<int> >
        CPAStarResource;

class CoverPositionManager
{
public:
    CoverPositionManager();
    ~CoverPositionManager();                // not virtual

    // A new position over the box spanned by the two points (corners ordered per axis), its
    // distances to every existing position appended as a new table row. Returns nothing.
    void createCoverPosition(const Vec2& a, const Vec2& b);
    CoverPosition* getLastCoverPosition() const;
    // Moves `out` to a random point at least 1 inside the box.
    void RandomizeCoverPos(Vec2 low, Vec2 high, Vec2& out);
    // Moves `out` (a corner) along one of the box's edges by a random fraction of half its length.
    void RandomizeCoverPosAlongEdge(Vec2 low, Vec2 high, Vec2& out);
    CoverPosition* isInCoverPosition(Vec2 point);   // the containing position, not a bool
    bool getClosestAttackCoverPosition(const Vec2& from, const Vec2& to, float maxDistance,
                                       float fraction, Vec2& out);
    // The closest cover centre between `from` and `to` that the cover `from` is in can see;
    // answers the distance to it (or to `to` when none qualifies).
    float getClosestNavCoverPosition(Vec2 from, Vec2 to, Vec2& out, CoverPosition*& cover);
    void finaliseCoverPositions();
    void updatePathfinding(double budget);
    // A synchronous search between two cover positions; true when it reached the goal.
    bool search(int start, int goal);

    // [INFERRED] Inline accessor for the lower-triangular distance table, as the CPNode search
    // reads it (m_distances[larger][smaller]).
    float getDistance(int a, int b) const
    {
        return a > b ? m_distances[a][b] : m_distances[b][a];
    }

    // dice's own capitalisation on the two `Randomize*` methods - initial capital, and the
    // American spelling next to `finaliseCoverPositions`'s British one, in the same class.

    static CoverPositionManager* m_instance;

    // Layout from the constructor and destructor; names [INFERRED]. Public: CPNode,
    // CPNodeManager and CoverPosition read the positions through m_instance.
    /*  +0 */ std::vector<CoverPosition*> m_coverPositions;
    // Row i holds the distances from position i to positions 0..i (createCoverPosition).
    /* +12 */ std::vector<std::vector<float> > m_distances;
    /* +24 */ CPPathfindingEngine* m_engine;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_COVERPOSITIONMANAGER_H
