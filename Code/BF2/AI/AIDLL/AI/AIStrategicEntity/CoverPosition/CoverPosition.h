// A rectangular cover position and its visibility links to the other positions.
// Evidence: libaidll.so 1.5.3153 (ia-32), attested file
// AI/AIDLL/AI/AIStrategicEntity/CoverPosition/CoverPosition.cpp (the Debug __FILE__ strings and the
// anonymous namespace of MobilePredicate), ELF 0x352AE0..0x354CB0 plus its map / vector
// instantiations up to 0x356670. Size 0x38 (operator new(0x38) in
// CoverPositionManager::createCoverPosition).

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_COVERPOSITION_H     // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_COVERPOSITION_H

#include <map>
#include <set>
#include <vector>

#include "../../../../../Math/Vec3.h"           // Vec2, Vec3

namespace dice {
namespace hfe {
namespace ai {

// How well one cover position sees another: an overall value, one per corner of the looking
// position, and one per line from each of its corners to each corner of the other. 28 bytes (the
// map node is 0x30). Inline throughout; the default constructor is the inlined
// `-1, resize(4, -1), resize(4, corners)` sequence of CoverPosition::updateAttacks.
class ConnectionType
{
public:
    // [INFERRED] enumerator names; the values are the stored ones. Lines are kVisible or
    // kBlocked; a corner is kVisible when all four of its lines are clear, kBlocked when none is,
    // else kPartial.
    enum NeighbourConnection
    {
        kUnknown = -1,
        kVisible = 0,
        kPartial = 1,
        kBlocked = 2
    };

    // The looking position's corners, in the order updateAttacks visits them: (min.x, min.y),
    // (min.x, max.y), (max.x, max.y), (max.x, min.y). [INFERRED] enumerator names.
    enum Corner
    {
        kCorner0 = 0,
        kCorner1 = 1,
        kCorner2 = 2,
        kCorner3 = 3
    };

    ConnectionType()
        : m_type(kUnknown)
    {
        m_corners.resize(4, kUnknown);
        m_lines.resize(4, m_corners);
    }

    // [INFERRED] names.
    /*  +0 */ NeighbourConnection m_type;
    /*  +4 */ std::vector<NeighbourConnection> m_corners;
    /* +16 */ std::vector<std::vector<NeighbourConnection> > m_lines;
};

class CoverPosition
{
public:
    // The two corners are stored as given (CoverPositionManager orders them first).
    CoverPosition(const Vec2& min, const Vec2& max);
    ~CoverPosition();

    // Whether this position's link to `other` is neither blocked nor unknown. Looks the link up
    // without an end check.
    bool canSee(CoverPosition* other);
    // The corner of this position that sees `other` best (worst), true when a link to `other`
    // exists; `corner` starts as the last corner and is untouched without a link.
    bool findHighestVisibilityCorner(CoverPosition* other, Vec2& corner);
    bool findLowestVisibilityCorner(CoverPosition* other, Vec2& corner);
    bool isReachableNeighbour(int index) const;
    bool isAttackableNeighbour(int index) const;
    bool isDefendableNeighbour(int index) const;
    // The gap between the two boxes (0 where they overlap on both axes).
    float getShortestDistance(CoverPosition* other) const;
    // updateAttacks(other, force) against every other cover position.
    void updateAttacks(bool force);
    // Line-of-sight from this position's corners (1.5 above the terrain) to `other`'s (0.3 above),
    // stored in this position's attack link to `other` and in `other`'s defend link to this one.
    // Without `force` an existing link is recomputed and a new one is left at its defaults.
    void updateAttacks(CoverPosition* other, bool force);
    // One looking corner against the other position's four corners; answers the clear-line count.
    // [INFERRED] int return (the counter is a 32-bit word returned in eax; callers ignore it).
    int updateCorner(const Vec3& eye, const Vec3& corner0, const Vec3& corner1, const Vec3& corner2,
                     const Vec3& corner3, ConnectionType& attack, ConnectionType& defend,
                     const ConnectionType::Corner& corner);
    // Empty in retail: the move-neighbour set is never filled.
    void updateMoveNeighbours();

    // Layout from the constructor (0x352C90); names [INFERRED].
    /*  +0 */ Vec2 m_min;
    /*  +8 */ Vec2 m_max;
    /* +16 */ int m_id;      // the index in CoverPositionManager; written by createCoverPosition
    /* +20 */ std::map<int, ConnectionType> m_attackable;   // isAttackableNeighbour; keyed by the other's id
    /* +32 */ std::map<int, ConnectionType> m_defendable;   // isDefendableNeighbour
    /* +44 */ std::set<int> m_moveNeighbours;               // isReachableNeighbour; CPNode's expansion
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_COVERPOSITION_H
