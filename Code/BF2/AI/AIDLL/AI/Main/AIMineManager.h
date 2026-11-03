// Tracks mines by side and updates navigation hazards when mines settle or disappear.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_AIMINEMANAGER_H
#define AI_AIDLL_AI_MAIN_AIMINEMANAGER_H

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// Mine-manager interface. Registration guards the roster static, which may remain
// stale until updateStaticMethods refreshes it.
class IAIMineManager
{
public:
    // Nested mine type: 0 is anti-personnel and 1 anti-tank.
    // [UNRESOLVED] Original enumerator names remain unknown.
    enum MineType : int;

    IAIMineManager();
    virtual ~IAIMineManager();

    virtual void init(MineType, unsigned int) = 0;
    virtual void reset() = 0;
    virtual void update() = 0;
    // Mine notifications do not return a result.
    virtual void mineRemoved(unsigned int) = 0;
    virtual void minePlaced(MineType, unsigned int, const Mat4&, float) = 0;
    virtual const std::set<unsigned int>& getMinedPolygons(MineType, unsigned int) const = 0;
    // Returns a borrowed position and writes its navigation indices; unknown mines yield two -1 indices.
    virtual const Vec3& getPathfindingPositionOfMine(unsigned int, MineType, unsigned int&, unsigned int&) const = 0;
};

// Mine tracking and polygon counters. Offsets describe the original 88-byte ELF object;
// STL representation and double alignment make the native size compiler-dependent.
class AIMineManager : public IAIMineManager
{
public:
    // Two unsigned polygon reference counts. Removal erases the entry only when both reach
    // zero. Callers must balance increments and decrements; underflow is unchecked.
    struct PolygonData
    {
        PolygonData();
        PolygonData(const PolygonData&);

        /* +0 */ unsigned int m_mines;          // [CHOSEN] after removeMineFromPolygon
        /* +4 */ unsigned int m_otherMines;     // [CHOSEN] after removeOtherMineFromPolygon
    };

// Stored mine state used by the manager's pending and active collections.
    struct MineInfo
    {
        MineInfo();
        MineInfo(const MineInfo& other);

        // The anti-tank form: a radius, i.e. a circular danger area. ,
        MineInfo(unsigned int, MineType, const Mat4&, float);
        // The anti-personnel form: two corners, i.e. a triangular one. ,
        MineInfo(unsigned int, MineType, const Mat4&, const Vec3&, const Vec3&);

        // Mine type also selects the pathfinding map: 0 for infantry, 1 for vehicles.
        // The polygon index is organized by type and then quad-tree island.
        /* +0   */ int m_id;                        // = -1 [INFERRED] first ctor argument
        /* +4   */ int m_mineType;                  // = 0 see above
        /* +8   */ unsigned int m_quadTree;         // = -1 filled by getPolygon's 4th argument
        /* +12  */ unsigned int m_polygon;          // = -1 the seed polygon
        /* +16  */ std::list<unsigned int> m_polygons;  // every polygon the mine covers
        // the radius, proven twice: `getPolygonsInRadius` takes it as the radius argument, and
        // `findSeedPolygonForATMine` samples an annulus between 2.0 and this.
        /* +20  */ float m_radius;                  // = 1.0f
        /* +24  */ Mat4 m_transform;                // = identity; m[8..10] is the facing, m[12..14] the position
        // the other two corners of the danger triangle, from `getPolygonsInTriangle`'s last two
        // arguments, and the 5-argument constructor's two Vec3s. Used only by AP mines.
        /* +88  */ Vec3 m_corner1;                  // = 0
        /* +100 */ Vec3 m_corner2;                  // = 0
    };

    AIMineManager();
    virtual ~AIMineManager();

    // IAIMineManager
    virtual void init(MineType, unsigned int);
    virtual void reset();
    virtual void update();
    virtual void mineRemoved(unsigned int);
    virtual void minePlaced(MineType, unsigned int, const Mat4&, float);
    virtual const std::set<unsigned int>& getMinedPolygons(MineType, unsigned int) const;
    virtual const Vec3& getPathfindingPositionOfMine(unsigned int, MineType, unsigned int&, unsigned int&) const;

private:
    // The secondary counter tracks projections of mines onto another navigation map.
    void mineAtRest(const MineInfo& info);

    // The iterator is passed by reference; preserve re-reads after helper calls because
    // the referenced iterator may alias caller state.
    void removeMine(std::map<unsigned int, std::list<MineInfo> >::iterator& entry);
    unsigned int findSeedPolygonForAPMine(MineInfo& info);
    unsigned int findSeedPolygonForATMine(MineInfo& info);
    void addNewMineToPolygon(MineType, unsigned int, unsigned int);
    void addNewOtherMineToPolygon(MineType, unsigned int, unsigned int);
    void removeMineFromPolygon(MineType, unsigned int, unsigned int);
    void removeOtherMineFromPolygon(MineType, unsigned int, unsigned int);

public:

private:
    // Next update deadline. update runs when now exceeds it, then schedules now + 0.5;
    // the initial -1e9 value makes the first tick run.
    /*  4 */ double m_nextUpdateTime;

    // Indexed by mine type, then quad-tree island. init() sizes the island dimension.
    /* 12 */ std::vector<std::vector<std::set<unsigned int> > > m_minedPolygons;
    /* 24 */ std::vector<std::vector<std::map<unsigned int, PolygonData> > > m_polygonData;

    /* 36 */ std::map<unsigned int, std::list<MineInfo> > m_mines;      // at rest; node =
    /* 48 */ std::map<unsigned int, MineInfo> m_settlingMines;          // still moving; node = 132

    /* 60 */ int  m_int60;              // = -1 [UNRESOLVED]; reset sets it to -1 as well
    /* 64 */ char m_gap64[8];           // the constructor never writes these eight bytes
    /* 72 */ Vec3 m_missingMinePosition; // Zero fallback returned by getPathfindingPositionOfMine; name [INFERRED].
    /* 84 */ bool m_flag84;             // = false [UNRESOLVED]; reset clears it as well
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIMINEMANAGER_H
