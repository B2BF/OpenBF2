// Per-bot navigation state and owned paths. The bot owner is borrowed.

#ifndef AI_AIDLL_AI_AIBOT_BOTNAVIGATION_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_BOTNAVIGATION_H

#include <set>
#include <vector>
#include <vector>

#include "../Main/AIPathfindingSearchTypes.h"   // QTPath, ClusterNodePath - both already written
#include "../../../../Math/Vec3.h"        // Vec2 and Vec3 both live here, and both are typedefs
                                          // to dice::hfe::Math::* - never forward-declare either

namespace dice {
namespace hfe {
namespace ai {

class BotMain;
class Information;
class IAIPathfinding;
class QuadTree;

class BotNavigation
{
public:
    explicit BotNavigation(BotMain* owner);
    ~BotNavigation();                           // [FRONTIER]

    // [FRONTIER] Navigation methods below remain incomplete unless implemented explicitly.
    bool update(bool& a, bool* b);
    void updateOccationally();                  // dice's spelling - "Occationally"
    // ELF 0x192200: the point to walk to; `arrived` at the last point, `direct` when the bot
    // should not follow points (no path, retracing, or blocked by a hostile vehicle's material).
    bool updatePath(Vec2& point, bool& arrived, bool& direct, float distance);
    void updateNavPoints();
    void updateObstacles();
    void drawPath();
    void updateLastPoint(Information* info);
    void updateSpaceLeftAndRight();
    void updateClosestDistance();

    // ELF 0x1901B0 / 0x1902B0: start a path search to `to`; false when none can be started.
    bool initPathfinding(const Vec3& to, float arriveDistance, float b);
    bool initPathfinding(unsigned int island, unsigned int goalPolygon, const Vec3& to, float arriveDistance, float b);
    bool doPolygonSearch(bool& a, bool* b);
    bool doClusterSearch(bool& a, bool* b);
    void calculatePoints();
    void mergeNavigationPoints();
    void getNavigationPoints(std::vector<Vec3>& points) const;
    void optimizePoints();
    void testDirectPath();                      // ELF 0x190DA0 (void: no eax on its exits)
    void retrace();

    bool stillOnPath();
    bool hasIllegalPath();
    // [INFERRED] accessor name for the +192 byte, which retail reads inline (BotManager::
    // actionComAgent). initPathfinding and updateOccationally set it; clearProgress
    // checkProgress, doPolygonSearch and setIsStuck clear it; updatePath tests it.
    bool isPathSearchPending() const { return m_flag192; }
    // [INFERRED] accessor names for the +193 and + (PE +0xC9 / +0x148), both read inline
    // by BotManager::actionPathfinding (, ).
    // +193: set to 1 only on initPathfinding's search-started path , cleared by the
    // constructor, initPathfinding's other exits and reset. +316: set by initPathfinding
    // cleared by update and reset.
    bool isSearchActive() const { return m_flag193; }
    bool isSearchNew() const { return m_flag316; }
    bool isActive();
    // returns true, unconditionally, in both images.
    bool isRunning();
    int getMapIndex() const { return m_mapIndex; }
    // [INFERRED] readers of fields the BAP actions read inline (ai-w1-c-actions request):
    // +0x2C (BAPAMoveTo*), +0xBC, +0xC0, +0xCC (BAPAMoveToObjectFinding::initObjectFinding).
    const Vec3& getGoal() const { return m_goal; }
    float getTargetDistance() const { return m_sentinel188; }
    bool getIsStuck() const { return m_flag192; }
    unsigned int getLastValidIsland() const { return static_cast<unsigned int>(m_vehicleIsland); }
    // [INFERRED] reader of +0x58 (InfanteryControl / BBAvoid read begin/end inline).
    const std::vector<unsigned int>& getObstacles() const { return m_obstacles; }
    float getSpaceLeft() const { return m_spaceLeft; }
    float getSpaceRight() const { return m_spaceRight; }
    bool checkProgress();
    void clearProgress();
    bool checkArrivedAtPoint(float distance);
    void setIsStuck(bool stuck);

    bool pointWithinObstacleRadius(const Vec3& point);
    void getFirstPointBeyondDist(const Vec2& from, float distance, Vec2& out);
    bool getSecondPoint(Vec2& out);             // ELF 0x1940C0
    bool getLargestAngleDiff(float& out);       // ELF 0x194120 (always true)
    void getTraceData(int a, unsigned int& b, unsigned int& c, Vec3& d);

    // ELF 0x195310: the warned position and the time left of 2.8 s; true while some is left.
    bool getGrenadeWarning(Vec3& position, double& timeLeft);
    // [INFERRED] reader of +0x114 (BBRandom / BBAvoid::calculateUrgency read it inline).
    bool getGrenadeFlag() const { return m_grenadeFlag; }
    void setGrenadeWarning(const Vec3& position, double time, bool b);   // ELF 0x195280

    void reset();                          // - (one arm frontier)
    void resetAll();

    // attested as a nested type by the container instantiations. sizeof 40. testDirectPath
    // (ELF 0x190DA0) writes all three Vec3 = the goal and +0x24 = trace's polygon; the walkers
    // (updatePath, getSecondPoint, ...) read +0x18 and +0x24. The first two Vec3 are
    // [UNRESOLVED] names.
    struct NavigationPoint
    {
        NavigationPoint()
            : m_polygon(-1)
        {
            edgePoint1.x = 0.0f; edgePoint1.y = 0.0f; edgePoint1.z = 0.0f;
            m_pointC.x = 0.0f; m_pointC.y = 0.0f; m_pointC.z = 0.0f;
            m_position.x = 0.0f; m_position.y = 0.0f; m_position.z = 0.0f;
        }

        /* +0x00 */ Vec3 edgePoint1;   // name: AIDLL_r assert "m_navigationPoints[i].edgePoint1" 0x10149B20 (entry +0)
        /* +0x0C */ Vec3 m_pointC;
        /* +0x18 */ Vec3 m_position;
        /* +0x24 */ int  m_polygon;     // = -1
    };

private:
    // ELF 0x22A429 reads the infantry polygon inline; reset writes it at 0x190D14.
    friend class BBPAvoidCollision2d;
    /*   0 */ BotMain* m_bot;              // the ctor argument, and the first store
    // initPathfinding's two floats (ELF 0x1902B0 stores +4 = a, +8 = b, +0xC = b * b);
    // checkArrivedAtPoint arrives within max(+4, 1) for a vehicle. [INFERRED] names.
    /*   4 */ float m_arriveDistance;      // = 2.0f
    /*   8 */ float m_float8;              // = 2.0f [UNRESOLVED]
    /*  12 */ float m_float8Squared;       // initPathfinding: b * b
    // initPathfinding / retrace: the bot's last valid position (IPIMobile::ValidPos fields).
    /*  16 */ int   m_startIsland;              // = -1; ValidPos::island [INFERRED] name: AIDLL_r names +112 m_island (0x10149B20), so this one was renamed
    /*  20 */ int   m_startPolygon;        // = -1
    /*  24 */ Vec3  m_startPosition;       // all 0
    /*  36 */ int   m_goalPolygon;         // = -1
    // IAIPathfinding::getMaterial's answer for the goal polygon (initPathfinding) or trace's.
    /*  40 */ unsigned char m_goalMaterial;   // = 0
    /*  41 */ char  m_pad41[3];
    /*  44 */ Vec3  m_goal;                // all 0; initPathfinding's target
    // path state: 0 reset, 2 path ready (testDirectPath), 4 (update stops). [INFERRED]
    /*  56 */ int   m_int56;               // not a gap - `reset` zeroes it
    // IAIPathfinding::instance, set by initPathfinding; reset releases the two reservations
    // through it (releaseResource +0x9C, releaseClusterResource +0xA4).
    /*  60 */ IAIPathfinding* m_pathfinding;   // = 0
    // IAIPathfinding::getQuadTree(map, island) (+0x64), set by initPathfinding.
    /*  64 */ QuadTree* m_quadTree;        // = 0
    // update (ELF 0x191320) sets it with the point reset and runs the polygon search while it
    // is set. [CHOSEN] name.
    /*  68 */ bool  m_polygonSearchPending;
    /*  69 */ char  m_pad69[3];
    /*  72 */ QTPath* m_path;       // new(0x18) - the fine path  // name: AIDLL_r assert 0x10149B20 (PE +0x48)
    // IAIPathfinding::acquireResource's polygon-search slot; -1 = none.
    /*  76 */ unsigned int m_int76;        // = -1
    // AIPathfinding::trace (ELF 0x3A81C0) reads +0x50 as the map index passed to getTraceData
    // when it does not use the trace map. [INFERRED] name.
    /*  80 */ int   m_mapIndex;            // never written by the constructor
    // IPIUnit::getEquipmentType of the bot's equipment, set by initPathfinding. [INFERRED] name.
    /*  84 */ int   m_equipmentType;       // never written by the constructor
    // updateObstacles (ELF 0x194B60) clears and fills it through getObjectsWithinRadius, and
    // stores its size at +252.
    /*  88 */ std::vector<unsigned int> m_obstacles;
    // the walkable result - see the header note. The constructor's dead resize targets this.
    /* 100 */ std::vector<NavigationPoint> m_navigationPoints;   // name: AIDLL_r assert 0x10149B20 (PE +0x68, 0x28-byte entries)
    // the island the path was searched on (initPathfinding: the start island).
    /* 112 */ int   m_island;          // never written by the constructor  // name: AIDLL_r assert "m_island == m_path->getIsland()" 0x10149B20 (PE +0x78)
    // index of the point being walked to.
    /* 116 */ unsigned int m_currentPoint;   // = 0
    // update: nonzero selects the cluster search. [CHOSEN] name.
    /* 120 */ bool  m_useClusterSearch;
    /* 121 */ char  m_pad121[3];
    /* 124 */ ClusterNodePath* m_clusterPath;   // new(0x1C) - the coarse path
    // the cluster-search slot (acquireClusterResource); -1 = none.
    /* 128 */ unsigned int m_int128;       // = -1
    // `allocate(20)` with 0 / 0 / self / self is an `_Rb_tree` header at 16 + sizeof(value_type)
    // so the value type is four bytes - a set of pointers or handles. Which is [UNRESOLVED];
    // `void*` holds the width and claims nothing.
    /* 132 */ std::set<unsigned short> m_set132;   // doPolygonSearch: the clusters the local search may use (setAllowedClusters)
    /* 144 */ double m_noProgressTimer;          // = 0.0  // name: AIDLL_r assert in checkProgress 0x10145DD0 (PE +0x98)
    /* 152 */ double m_noThrottleTimer;          // = 0.0  // name: AIDLL_r assert in checkProgress 0x10145DD0 (PE +0xA0)
    /* 160 */ double m_double160;          // = 0.0
    /* 168 */ double m_double168;          // = 0.0
    /* 176 */ float m_zero176[3];         // +176..+184; checkProgress: the soldier position at the last action-press check
    // is 9,999,999.0f - a "no distance yet" sentinel, and a third kind after the
    // DBL_MAX timestamps and -9999.0 magnitudes BotMain uses. Do not normalise them together.
    // distance to the target (C's reader); reset 9999999.0f, updatePath's arrival 1e8f.
    /* 188 */ float m_sentinel188;         // = 9999999.0f
    // setIsStuck writes it (never while parachuting); initPathfinding sets it when no path can
    // be started; updatePath does nothing while it is set.
    /* 192 */ bool  m_flag192;             // = false
    /* 193 */ bool  m_flag193;             // = false
    /* 194 */ bool  m_flag194;             // not padding - `reset` clears it
    /* 195 */ bool  m_flag195;             // = false
    /* 196 */ char  m_gap196[4];           // [UNRESOLVED] - never written
    // the last valid positions (reset / initPathfinding): the vehicle's (getLastValidPosition of
    // the vehicle number) and the infantry one (IPIMobile +8 / +0xC).
    /* 200 */ int   m_currentPolygon;      // = -1  // name: AIDLL_r asserts 0x10146930, 0x1014AD10 (PE +0xD0)
    /* 204 */ int   m_vehicleIsland;       // = -1
    /* 208 */ int   m_infantryPolygon;     // = -1
    /* 212 */ int   m_infantryIsland;      // = -1
    // getTraceData (ELF 0x194F10): the last traced valid position and its map index. The
    // constructor zeroes the position (three word stores); the index is never initialised.
    /* 216 */ Vec3  m_tracePosition;
    /* 228 */ int   m_traceMapIndex;
    // IPIPhysical::getSmallestRadius of the equipment (initPathfinding). [INFERRED] name.
    /* 232 */ float m_radius;
    // initPathfinding: the equipment's type flags carry 0x200000 (byte +6 bit 0x20). The soldier
    // arms test it (checkArrivedAtPoint's soldierMinDist, updatePath's 0.5 s retrace delay);
    // updateOccationally (ELF 0x192830) calls updateSpaceLeftAndRight while it is set.
    /* 236 */ bool  m_updateSpaceLeftAndRight;
    /* 237 */ char  m_pad237[3];
    // updatePath: useSkipTrace && soldier before a retrace; stillOnPath counts it down.
    /* 240 */ int   m_skipTraceCount;              // = 0  // name: AIDLL_r assert 0x101471C0 (PE +0xF8)
    // updatePath's game time (isActive: three seconds).
    /* 244 */ double m_double244;          // not a gap - `reset` zeroes it
    // resetAll clears this field.
    /* 252 */ int   m_int252;              // resetAll: 0
    // setGrenadeWarning (ELF 0x195280) writes all three; getGrenadeWarning answers 2.8 s from
    // +268. [INFERRED] names.
    /* 256 */ Vec3  m_grenadePosition;     // all 0
    /* 268 */ double m_grenadeTime;        // = 0.0; resetAll -9999.0
    /* 276 */ bool  m_grenadeFlag;         // = false; setGrenadeWarning's bool
    /* 277 */ char  m_pad277[3];
    /* 280 */ float m_zero280[3];         // +280..+288; checkProgress: the position at the last progress
    // BotMain::postPlanExecution (ELF 0x189220) strafes towards avoid side 1 while +292 > 0.5
    // and towards side 2 while +296 > 0.5; updateSpaceLeftAndRight writes them. [INFERRED] names.
    /* 292 */ float m_spaceLeft;
    /* 296 */ float m_spaceRight;
    /* 300 */ double m_lastAngleDiffTest;          // Initially -1.0; independent of the distance sentinel.  // name: AIDLL_r assert 0x10147490 (PE +0x138)
    // getLargestAngleDiff's cached answer (recomputed every 0.5 s, +300 the time).
    /* 308 */ float m_largestAngleDiff;    // = 0
    // updateClosestDistance: the walked-points length from the bot (updatePath re-checks the
    // cluster path at 30 or more). [INFERRED] name.
    /* 312 */ float m_pathLength;          // = 0
    // `reset`'s first statement clears the byte here.
    /* 316 */ bool  m_flag316;             // reset: false
    /* 317 */ char  m_pad317[3];
    // doPolygonSearch's local search segment: from the last point (or the start) to the next
    // cluster portal two steps ahead (or the goal). [INFERRED] names.
    /* 320 */ Vec3  m_searchStart;         // ctor: 0
    /* 332 */ Vec3  m_searchGoal;          // ctor: 0
    /* 344 */ unsigned int m_searchStartPolygon;   // never written by the ctor
    /* 348 */ unsigned int m_searchGoalPolygon;
    /* 352 */ bool  m_searchGoalIsFinal;   // the segment ends at the goal
    /* 353 */ char  m_pad353[3];
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_BOTNAVIGATION_H
