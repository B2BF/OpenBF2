// Owns strategic areas, waypoints and bridges, and tracks the active area for console commands.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICOBJECTMANAGER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICOBJECTMANAGER_H

#include <list>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Vec2 and Vec3 cannot be forward-declared here: in this tree they are typedefs to
// `dice::hfe::Math::Vec2/Vec3`, not classes in `dice::hfe`, so a local `struct Vec3;` is a
// redefinition with a different basic type (C2371). The header has to come in.
#include "../../../../Math/Vec3.h"
#include "AIStrategicObject.h"                       // StrategicObjectType

namespace dice {
namespace hfe {
namespace ai {

class AIStrategicArea;
class AIStrategicObject;
class IAIObject;
class SAWayPoint;

class AIStrategicObjectManager
{
public:
    // Waypoint collections for a pair of connected strategic areas.
    class WPContainer
    {
    public:
        WPContainer();                                   // ELF 0x343340 (C2; C1 0x343380)
        WPContainer(const WPContainer& other);           // ELF 0x3433C0 (C2; C1 0x3434F0)
        ~WPContainer();                                  // ELF 0x343660 (D2; D1 0x3436D0)

        std::vector<SAWayPoint*>& getWayPoints(int type);
        const std::vector<SAWayPoint*>& getWayPoints(int type) const;

    private:
        /*  +0 */ std::vector<SAWayPoint*> m_wayPointsA;   // [UNRESOLVED] which list this is
        /* +12 */ std::vector<SAWayPoint*> m_wayPointsB;
    };

    // Bridge segment record linking a position and the affected strategic objects.
    class BridgeSegment
    {
    public:
        BridgeSegment()
        {
            m_position.x = 0.0f;
            m_position.y = 0.0f;
            m_position.z = 0.0f;
        }

        /*  +0 */ bool isDestroyed;       // 0 at creation; `bridgeSegmentDestroyed` sets it to 1  // name: AIDLL_r assert "bridge->isDestroyed" (0x10086E40; +0)
        /*  +4 */ int m_i4;              // [UNRESOLVED] - written only by `bridgeSegmentDestroyed`
        /*  +8 */ IAIObject* m_object;   // the search key for all three of the methods above
        /* +12 */ Vec3 m_position;       // [INFERRED] - three zeroed floats, never read here
    };

    AIStrategicObjectManager();
    ~AIStrategicObjectManager();                    // , not virtual

    virtual bool reset();                           // - slot 0
    virtual bool resetAreas();                      // - slot 1

    static AIStrategicObjectManager* getInstance();

    // strategic objects
    const std::vector<AIStrategicObject*>& getStrategicObjects() const;
    int getNumberOfStrategicObjects() const;
    const std::vector<AIStrategicObject*>& getStrategicObjectsOfCategory(int) const;
    AIStrategicObject* getStrategicObject(int) const;
    AIStrategicObject* getStrategicObject(const std::string&) const;
    AIStrategicObject* getStrategicObjectOfCategory(int, int) const;
    void addStrategicObject(AIStrategicObject*);
    // ELF 0x344D00: the removed object, or 0 for an index out of range.
    AIStrategicObject* removeStrategicObject(int index);
    AIStrategicObject* getClosestObject(const Vec3&, int) const;
    AIStrategicObject* getClosestObject(const Vec3&, int, bool) const;
    AIStrategicObject* getClosestObject(const Vec3&, int, int, int) const;
    // ELF 0x345020. [INFERRED] parameter names, from their uses.
    AIStrategicObject* getClosestObjectinCategory(const Vec3& position, int side, int category,
                                                  int equipment, int vehicle) const;

    // strategic areas
    AIStrategicArea* getStrategicArea(int) const;
    AIStrategicArea* getStrategicAreaOfCategory(int, int) const;
    const std::map<std::string, AIStrategicArea*>& getStrategicAreas() const;
    std::string getStrategicAreaNames();
    AIStrategicArea* getActiveArea();
    bool setActive(std::string name);       // ELF 0x346D40; answers whether the name was found
    std::string getActive() const;
    // Initializes strategic areas; has no return value.
    void initAreas();
    void postLoadInitAreas();
    Vec2 getPosition() const;
    bool isInside(const std::string&, const Vec2&);
    // ELF 0x3456D0: answers false (and clears the active area) for a duplicate name or a failed
    // allocation, true after creating and selecting the area.
    bool create(std::string name, Vec2 topLeft, Vec2 centre, float height, float radius,
                const std::string& category = "");   // ConsoleClass417 passes "" for five arguments
    // Creates an area from a control point. Returns false for duplicate names or allocation
    // failure and true after successful creation.
    // ConsoleClass419 passes "" when the category is omitted.
    bool createFromControlPoint(const std::string&, unsigned, float, const std::string& = "");
    bool removeActive();                    // ELF 0x3461A0; false without an active area
    void setFlankPosition(Vec3, Vec3);
    std::string list() const;

    // neighbours
    // ELF 0x3463B0 / 0x346460: false for an unknown name or areas of different layers (+0x190).
    bool addNeighbour(std::string);
    bool addTwoWayNeighbour(std::string, std::string);
    void removeAllNeighbours();
    std::string getNeighbours();

    // categories
    int getNumberOfCategories();
    const std::string& getCategoryName(int) const;
    int getCategoryFromName(const std::string&) const;
    void createNewCategory(const std::string&);

    // object type flags
    // The active area's new flag word, or -1 without an active area (or for type 27).
    int addObjectTypeFlag(AIStrategicObject::StrategicObjectType);     // ELF 0x3467B0
    int removeObjectTypeFlag(AIStrategicObject::StrategicObjectType);  // ELF 0x346800
    int setObjectTypeFlags(int);                                       // ELF 0x346850
    int getObjectTypeFlags();
    void removeAllObjectTypeFlags();
    std::string getObjectTypeFlagMapping() const;

    // order positions
    bool setOrderPosition(std::string, const Vec3&);
    Vec3 getOrderPosition(std::string);
    std::string listOrderPositions();
    void checkAllOrderPositions() const;    // ELF 0x34A150: no result (plain retl)

    // waypoints
    void addWayPoint(const Vec3&, const std::string&, const std::string&, const std::string&);
    bool removeWayPoint(const Vec3&, const std::string&, const std::string&, const std::string&);
    void clearWayPoints(const std::string&, const std::string&, const std::string&);
    const std::vector<SAWayPoint*>& getWaypoints(int, int, int) const;
    const std::vector<SAWayPoint*>& getWaypoints(int, const AIStrategicArea*, const AIStrategicArea*) const;
    void drawWayPoints() const;
    void undrawWayPoints() const;
    void setWayPointsToDraw(const std::string&, const std::string&);
    void clearWaypointToDraw();
    void checkAllWayPoints() const;         // ELF 0x34A350: no result (plain retl)
    // ELF 0x349610: an `aiStrategicArea.addWayPoint` line per waypoint, skipping containers whose
    // first area's layer is listed in `excludedLayers` ("|"-separated).
    void streamWaypoints(std::ostringstream& out, const std::string& excludedLayers) const;

    // bridges
    void createBridgeSegment(IAIObject*);
    void removeBridgeSegment(IAIObject*);
    void deleteBridges();
    void resetBridges();
    void bridgeSegmentDestroyed(IAIObject*, int);
    void bridgeSegmentRepaired(IAIObject*);
    // Returns the nearest destroyed segment's object (ELF 0x349ED0 returns the pointer; the mangled
    // name carries no return type). `position` receives the segment record's position, `distance`
    // the squared distance.
    IAIObject* getClosestDestroyedBridgeSegment(Vec3 from, bool repairRequested, int side, Vec3& position,
                                                float& distance);

    static AIStrategicObjectManager* m_instance;

private:
    // layout, , read from the constructor and confirmed member by member against the
    // destructor's reverse-order teardown
    // MSVC's `sizeof` will differ - libstdc++ 3.x puts a `_Rb_tree` in twelve bytes (a header-node
    // pointer, a node count and four bytes of empty comparator) and a `std::list` in four.
    /*   +4 */ std::vector<AIStrategicObject*> m_strategicObjects;
    /*  +16 */ std::vector<std::vector<AIStrategicObject*> > m_objectsPerCategory;   // name: AIDLL_r assert 0x10086290 (PE +0x14)
public:
    // [CHOSEN, not recovered] Visibility for the generated inline debug control.
    /*  +28 */ bool m_debugEnabled;                 // false; console enableDebug
private:
    /*  +32 */ std::map<std::string, AIStrategicArea*> m_strategicAreaMap;   // name: AIDLL_r assert 0x1008B6F0 (PE +0x28)
    // The reverse index. Its value type is `std::string` - read straight off the destructor's
    // `_Rb_tree<AIStrategicArea*, pair<AIStrategicArea* const, std::string>, ...>` teardown.
    /*  +44 */ std::map<AIStrategicArea*, std::string> m_areaNames;
    // An iterator, not a pointer - one word in this libstdc++, initialised to `m_strategicAreaMap.end`
    // by the constructor and compared against `end` by every reader (`getActiveArea`
    // `getObjectTypeFlags`, `setOrderPosition`, `getPosition`).
    /*  +56 */ std::map<std::string, AIStrategicArea*>::iterator m_activeArea;
    /*  +60 */ std::map<std::string, int> m_namesToCategory;   // name: AIDLL_r assert 0x100892D0 (PE +0x44)
    /*  +72 */ std::vector<std::string> m_categoryToNames;   // name: AIDLL_r assert 0x100867B0 (PE +0x50)
public:
    // [CHOSEN, not recovered] Visibility for the generated inline drawing control.
    /*  +84 */ bool m_drawVehicleSearchRadii;       // false; console drawVehicleSearchRadi
private:
    /*  +88 */ std::map<std::pair<int, int>, WPContainer> m_waypointMap;   // name: AIDLL_r assert 0x10089680 (PE +0x64)
    // The two halves of a `m_waypointMap` key, both -1 for "nothing selected".
    // `clearWaypointToDraw` resets only the first of the two; `drawWayPoints` tests both.
    /* +100 */ int m_wayPointsToDrawFirst;
    /* +104 */ int m_wayPointsToDrawSecond;
    /* +108 */ std::list<BridgeSegment*> m_bridgeSegments;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICOBJECTMANAGER_H
