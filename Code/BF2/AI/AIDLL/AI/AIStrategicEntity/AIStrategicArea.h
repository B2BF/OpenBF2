// Strategic area with explicit bounds or a control-point anchor.
// Control-point areas defer initialization until world objects exist.
// The base index is used to select the area's debug primitives.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICAREA_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICAREA_H

#include <list>
#include <map>
#include <string>
#include <vector>

#include "AIStrategicObject.h"
#include "AreaDistanceInfo.h"

namespace dice {
namespace hfe {
namespace ai {

class AILandingZone;
class SAWayPoint;

class AIStrategicArea : public AIStrategicObject
{
public:
    // Per-map order-position offset and polygon state. Construction leaves the polygon field unset.
    class MapData
    {
    public:
        MapData()
        {
            // the island ID is left indeterminate on purpose. The zeroing loop steps four words
            // and writes three of them. Do not "fix" it.
            m_offset.x = 0.0f;
            m_offset.y = 0.0f;
            m_offset.z = 0.0f;
        }

        /*  +0 */ Vec3 m_offset;        // absolute until `initOrderPoses`, a delta afterwards
        /* +12 */ unsigned int m_island;
    };

    // [UNRESOLVED] List payloads whose original element types have not been identified.
    struct AreaWord   { int m_word; };

    // ELF 0x335480 (C2; C1 0x335850): an area with explicit bounds. The first float is the
    // base's stored height (m_f148), the second its radius. [INFERRED] parameter names.
    AIStrategicArea(const std::string& name, Vec2 topLeft, Vec2 centre, float height, float radius);
    AIStrategicArea(const std::string& name, unsigned int controlPointId, float radius);

    // [INFERRED] Initializes area state. Manager-created areas call this during initAreas;
    // control-point areas defer it until postLoadInit.
    void init();

    // Control-point areas defer shared construction until post-load position resolution.
    void internalConstructorCall(const Vec2& topLeft, const Vec2& centre);
    void postLoadInit();
    void initOrderPoses();

    // Writes the control-point position and radius; returns false for an unassigned id.
    bool getControlPointPosAndRadius(Vec3& position, float& radius);

    // ELF 0x33BD30: bit test of the landing-zone equipment mask (+340).
    bool isLandingZoneUser(int equipment);
    // ELF 0x33BD50: with the side's distances dirty (+343 + side), rebuild them per vehicle
    // group (or for group 0 when there are none).
    void validateDistances(int side);
    void validateDistances(int side, int group);                 // ELF 0x33BE30
    void insertUnitDistances(int side, int group);               // ELF 0x33BF90
    void getDistances(int side, std::list<std::pair<float, AIStrategicObject*> >& distances,
                      std::map<AIStrategicObject*, std::list<AIStrategicObject*> >& routes,
                      std::map<AIStrategicObject*, float>& costs, int groupMask);   // ELF 0x33AAC0

    // ELF 0x33F180 / 0x33F130: inside the box from m_centre around the side-1 position (+0x84);
    // the Vec3 form tests its XZ projection.
    bool isInside(const Vec2& point) const;
    bool isInside(const Vec3& point) const;
    // ELF 0x3396E0: the address of the side-1 position (+0x84). Read by
    // CommunicationHub::enemySpottedByAI.
    const Vec2& getCenterPosition() const;
    // ELF 0x339890 (added by ai-bot for closeAndShouldAvoidVehicles): a control point area
    // (hasObjectTypeFlag(16)) whose order position for (map, side) lies beyond the control point's
    // radius (IAIGame::getControlPointInfo(m_controlPointId)).
    bool isOrderPosOutsideCpRadius(int map, int side);
    // ELF 0x338FF0 (added by ai-bot for SquadCoordinator::squadManeuverStaticFlank): the flanking
    // pair (+392 records: two Vec3) whose first position is closest to `position`; false when none.
    bool getClosestFlankingPositions(Vec3 position, Vec3& first, Vec3& second);

    // ELF 0x33EF80: answers the second position; the first and the waypoint list are unused.
    Vec3 getBotWaypoint(Vec3 from, Vec3 to, const std::vector<SAWayPoint*>& waypoints, int map);
    // ELF 0x339AB0.
    void getControlpointPos(AIStrategicObject* object, int side, int unit, int equipment,
                            const Vec3& from, Vec3& position);

    // ELF 0x33C430 / 0x33C940: orderBot's two arms (air units / everything else). Neither sets
    // eax (both end in the goal list's deallocate), so they and orderBot answer nothing.
    void orderAirBot(int side, const std::list<AIStrategicObject*>& objects, int unit, ISAI* sai);
    void orderNormalBot(int side, const std::list<AIStrategicObject*>& objects, int unit, ISAI* sai);
    // ELF 0x33A2F0. Declared by ai-bot for SquadMember::moveToGoalPos (0x1B4240).
    bool getRouteList(int side, AIStrategicArea* from, std::list<AIStrategicObject*>& route);

    // Virtual declaration order: thirty-four ELF slots.
    virtual ~AIStrategicArea();                                    // slots 0/1,

    virtual int getType() const;                                   // slot 2,
    // slots 3 and 4 are AIStrategicObject::getPosition / ::setPosition - not overridden

    virtual Vec3 getOrderPos(int a, int b) const;                  // slot 5,
    virtual float updateTemperature(const Strategy* strategy, int side);  // slot 6, ELF 0x33A0F0
    virtual void update(int side, ISAI* sai, bool& changed);        // slot 7
    virtual int  getChangedAttackDefence(int side);                // slot 8,
    virtual float getControlPointTakeOverCounter();               // slot 9
    virtual bool isEnemyTakingOverBase(int side);                  // slot 10,
    virtual float calculateDefenseNeed(int side);                   // slot 11,
    virtual float calculateAttackValue(int side);                   // slot 12,
    virtual float calculateAttackCost(int side);                    // slot 13,
    virtual bool isNeighbour(AIStrategicObject* object) const;     // slot 14,
    virtual bool hasNeighbourWithStatus(StatusMap::Status status,
                                        int side) const;           // slot 15,
    virtual bool isNeighbouringObject(AIStrategicObject* object) const;   // slot 16,
    virtual bool hasNeighbouringObjectWithStatus(StatusMap::Status status,
                                                 int side) const;  // slot 17,
    virtual int  getObjectTypeFlags() const;                       // slot 18,
    // Tests a StrategicObjectType bit position, not a mask.
    virtual bool hasObjectTypeFlag(StrategicObjectType type) const;       // slot 19,
    virtual bool hasObjectTypeFlags(int flags) const;              // slot 20,
    virtual int  getTacticalStatus(int side) const;                // slot 21,
    virtual bool isAreaTakeable();                                 // slot 22,
    virtual bool collectSquadResources(int side, ISAI* sai);       // slot 23, ELF 0x33A380
    virtual int  collectAdditionalSquadResources(int amount, int side, ISAI* sai);
                                                                   // slot 24,
    virtual bool releaseSurplus(int side);                         // slot 25, ELF 0x33A6A0
    virtual float gravity(const Vec3& position, int side);         // slot 26,
    virtual float gravity(const Vec2& position, int side);         // slot 27,
    virtual bool retainBot(int side, int a, ISAI* sai);            // slot 28,
    virtual float getXExtent(int side) const;                      // slot 29,
    virtual float getZExtent(int side) const;                      // slot 30,
    virtual void setAsDirty(int side);                             // slot 31,
    virtual void orderBot(int side, const std::list<AIStrategicObject*>& objects,
                          int unit, ISAI* sai);                    // slot 32, ELF 0x33C340

    // Additional virtual overload declared after the inherited slots.
    virtual float gravity(float distance, int side);               // slot 33,

    // Non-virtual area operations called by AIStrategicObjectManager and the console bindings.
    void addFlankPosition(Vec3 first, Vec3 second);                // ELF 0x338EC0
    std::list<std::pair<Vec3, Vec3> > getFlankPositions();         // ELF 0x338F50
    void addNeighbour(AIStrategicObject* object);                  // ELF 0x3391A0
    void removeNeighbour(AIStrategicObject* object);               // ELF 0x339230
    void addNeighbouringObject(AIStrategicObject* object);         // ELF 0x3393E0
    void removeAllNeighbours();                                    // ELF 0x339500
    // The base's virtual setPosition(int, const Vec2&) stays visible beside this overload.
    using AIStrategicObject::setPosition;
    void setPosition(const Vec2& position);                        // ELF 0x339680
    void setCenterPosition(const Vec2& position);                  // ELF 0x3396B0
    void setTopLeftCorner(const Vec2& corner);                     // ELF 0x3396F0
    bool setOrderPos(int map, const Vec3& position);               // ELF 0x339710
    // Both answer the new flag word: AIStrategicObjectManager's forwarders (ELF 0x346850,
    // 0x3467B0) return the call's eax as their own int result.
    int setObjectTypeFlags(int flags);                             // ELF 0x33A2B0
    int addObjectTypeFlag(StrategicObjectType type);               // ELF 0x33A2D0
    std::string attachLandingZone(const std::string& name);        // ELF 0x33B580
    AILandingZone* getLandingZone(int index);                      // ELF 0x33B6E0
    AILandingZone* getLandingZone(int id, int& index);             // ELF 0x33B700
    std::vector<AILandingZone*>& getLandingZones();                // ELF 0x33B780
    const std::vector<AILandingZone*>& getLandingZones() const;    // ELF 0x33B790
    std::string listLandingZones();                                // ELF 0x33B7A0
    AILandingZone* getClosestLandingZone(const Vec2& position);    // ELF 0x33BC40
    AILandingZone* getClosestLandingZone(const Vec3& position);    // ELF 0x33BCC0
    void addToLandingZoneUsers(int equipment);                     // ELF 0x33BD10
    void cleanUnitDistances(int side, int group);                  // ELF 0x33C260
    void getKnownEmptyVehicles(std::vector<unsigned int>& vehicles, int side) const;  // ELF 0x33F1C0

    // layout, ELF offsets, read store by store out of the constructor
    // The base occupies 0..267. MSVC's own offsets will differ - a libstdc++ `std::list` is four
    // bytes and a `std::string` is four. These are documentation, not a layout claim for this build.
    /* +268 */ std::list<AreaWord> m_list268;      // [UNRESOLVED] - landing zones? node 12 B
    // The top-left corner: the bounded constructor and setTopLeftCorner store it; [INFERRED]
    // name kept from the control-point path, which derives it from the control-point radius.
    /* +272 */ Vec2 m_centre;                      // getXExtent/getZExtent subtract it
    /* +280 */ MapData* m_mapData;                 // new MapData[IAIPathfinding::getNumberOfMaps]
    // Separate vectors for neighboring areas and other neighboring strategic objects.
    /* +284 */ std::vector<AIStrategicObject*> m_neighbours;
    /* +296 */ std::vector<AIStrategicObject*> m_neighbouringObjects;
    /* +308 */ std::string m_name;                 // the area name the script gave
    /* +312 */ int m_objectTypeFlags;              // 0; `getObjectTypeFlags` returns exactly this
    /* +316 */ unsigned int m_controlPointId;      // the second script argument, e.g. 502
    // [UNRESOLVED] Both fields are initialized to 25; their meanings are unknown.
    /* +320 */ int m_i320;                         // = 25; tactical status, side 1 (getTacticalStatus)
    /* +324 */ int m_i324;                         // = 25; tactical status, side 2
    // Landing zones associated with this area.
    /* +328 */ std::vector<AILandingZone*> m_landingZones;
    /* +340 */ int m_i340;                         // 0; landing-zone equipment bit mask
// Per-vehicle-group arrays; each allocation has at least one element.
// [INFERRED] Container element types remain partly uncertain. Side-indexed flags use side - 1.
    /* +344 */ bool m_distancesDirty[2];           // both = true [INFERRED] name
    /* +346 */ char m_pad346[2];
    /* +348 */ bool* m_flagsA;                     // new bool[groups], all true [UNRESOLVED]
    /* +352 */ bool* m_flagsB;                     // new bool[groups], all true [UNRESOLVED]
    /* +356 */ bool m_sideEnabled[2];             // both true; [INFERRED] name, update side gates
    /* +358 */ bool m_showNeighbours;              // false; console showNeighbours
    /* +359 */ char m_pad359[1];
    /* +360 */ std::list<std::pair<float, AIStrategicObject*> >* m_listsA;   // [group]
    /* +364 */ std::list<std::pair<float, AIStrategicObject*> >* m_listsB;   // [group]
    /* +368 */ std::map<AIStrategicObject*, std::list<AIStrategicObject*> >* m_mapsA; // list-valued (validateDistances teardown); [group]
    /* +372 */ std::map<AIStrategicObject*, std::list<AIStrategicObject*> >* m_mapsB; // list-valued (validateDistances teardown); [group]
    /* +376 */ std::map<AIStrategicObject*, float>* m_mapsC;                 // [group]
    /* +380 */ std::map<AIStrategicObject*, float>* m_mapsD;                 // [group]
    // Per-side distance records, indexed by side - 1 like m_distancesDirty.
    /* +384 */ std::list<AreaDistanceInfo> m_distances[2];
    // Flanking position pairs (addFlankPosition / getClosestFlankingPositions); node 0x20.
    /* +392 */ std::list<std::pair<Vec3, Vec3> > m_flankPositions;
    // Stored doubled extent, initialized to twice m_size[1].
    /* +396 */ float m_f396;                       // = 2 * m_size[1] [UNRESOLVED] meaning
    // -1 from both constructors. Read as a layer by AIStrategicObjectManager: neighbours must share
    // it (addNeighbour) and streamWaypoints filters on it. [INFERRED] meaning; no writer known.
    /* +400 */ int m_i400;
    /* +404 */ bool m_visible;                     // true; console setVisible/getVisible
    /* +405 */ bool m_statusChanged;               // false; update stores its changed output here
    // Additional state initialized by internalConstructorCall.
    /* +406 */ bool m_enemyTakeover[2];            // both false; [INFERRED] name, update's side flags
    /* +408 */ float m_f408;                       // = 1.0f [UNRESOLVED]
    /* +412 */ float m_f412;                       // = 1.0f [UNRESOLVED]
    // Both constructors store 0.0f to all three words. [UNRESOLVED] meaning.
    /* +416 */ Vec3 m_v416;
    // Selects control-point initialization; the bounded constructor sets it false and uses
    // control point id -1.
    /* +428 */ bool m_fromControlPoint;            // true
    // Raised by `initOrderPoses` and by nothing else; both constructors clear it. [INFERRED]
    /* +429 */ bool m_orderPosesInitialised;       // false
    /* +430 */ char m_pad430[2];                   // tail padding to the observed 432
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICAREA_H
