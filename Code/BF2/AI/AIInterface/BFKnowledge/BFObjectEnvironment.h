// Queries engine objects for the AI.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_BFOBJECTENVIRONMENT_H
#define AI_AIINTERFACE_BFKNOWLEDGE_BFOBJECTENVIRONMENT_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "AIAdapterTypes.h"
#include "../../AIDLL/AI/Main/IAIPathfinding.h" // Complete triple elements in the mesh face lists.

namespace dice {
namespace hfe {

class IBase;
class CombatArea;
namespace world { class IObject; class ICollisionMesh; }

namespace ai {

// A borrowed target position; assignment keeps the destination's existing reference.
class TargetData
{
public:
    TargetData(unsigned int id, const Vec3& position, IAIObject* object);
    TargetData(const TargetData& other);
    TargetData& operator=(const TargetData& other);

    // Member spellings are [INFERRED]; ELF constructors establish their order.
    unsigned int id;        // x86 +0x00
    const Vec3& position;   // x86 +0x04
    IAIObject* object;      // x86 +0x08
};

class IAIObjectEnvironment
{
public:
    // Mesh data exchanged with the offline navigation exporter. Field names are [INFERRED].
    // ELF MeshAndBoundingBox/WaterMesh list destruction and Mesh copy construction attest
    // these source-level containers; their native STL layouts are compiler-dependent.
    struct Mesh
    {
        std::vector<Vec3> vertices;
        std::vector<std::pair<int, int>> edges;
        std::list<triple<int, int, int>> faces;
    };
    struct MeshAndBoundingBox
    {
        std::string name;
        IAIObject* object; // x86 +0x04; assigned after insertion from the object's cached AI component.
        Mesh boundingBox;
        Mesh mesh;
    };
    struct WaterMesh
    {
        std::string name;
        Mesh mesh;
    };
    struct DestroyableAddOnMesh
    {
        std::string name;
        bool initiallyWalkable;
        Mesh mesh;
    };

    IAIObjectEnvironment();
    virtual ~IAIObjectEnvironment();

    virtual void getWorldFrustumObjects(std::vector<IAIObject*>& out, const Frustum& frustum, AIObjectPredicate* filter) const = 0;
    virtual void getWorldSphereObjects(std::vector<IAIObject*>& out, const Vec3& centre, float radius, AIObjectPredicate* filter) const = 0;
    virtual void getAllWorldObjects(std::vector<IAIObject*>& out, AIObjectPredicate* filter) const = 0;
    virtual void getAllDestroyableObjects(std::vector<DestroyableObject>& out) const = 0;
    virtual void getWorldPlayerIds(std::vector<int>& out) const = 0;
    virtual void getWorldPlayerOccupiedObjects(std::vector<IAIObject*>& out, AIObjectPredicate* filter) const = 0;
    virtual void getWorldPlayerOccupiedObjectsWithIds(std::vector<std::pair<int, IAIObject*> >& out, AIObjectPredicate* filter) const = 0;
    virtual IAIObject* getLocalPlayerObject() const = 0;
    virtual bool getLocalPlayerPosition(Vec3& position) const = 0;
    virtual bool getLocalPlayerCameraTransformation(Mat4& transform) const = 0;
    virtual int getLocalPlayerId() const = 0;
    virtual const std::vector<int>& getActivePlayerIds() const = 0;
    virtual bool isValidPlayerId(int id) const = 0;
    virtual int getNextPlayerId(int id) const = 0;
    virtual bool collideLineWithWorld(const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore, bool a, float b, bool c, bool& hit) = 0;
    virtual bool collideLineWithWorld(const Vec3& from, const Vec3& to, AIObjectPredicate* filter, Vec3* hit) = 0;
    virtual bool getMaxHeight(const Vec3& from, const Vec3& to, AIObjectPredicate* filter, float& height) = 0;
    virtual bool hasIndirectLineOfFire(IWeapon* weapon, const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore) = 0;
    virtual void pickRandomSensePosition(Vec3& position, Vec3& normal, Vec3 hint, IAIObject* object) = 0;
    virtual bool pickSoldierRandomSensePosition(Vec3& position, Vec3& normal, IAIObject* object, unsigned int& part) = 0;
    virtual bool getSoldierSensePosition(Vec3& position, Vec3& normal, IAIObject* object, unsigned int part) = 0;
    virtual int getHeightAndNormalForObject(IAIObject* object, const Vec3& position, float& height, Vec3& normal, float& slope) = 0;
    virtual void getVehicleSpawnerPositions(std::vector<Vec3>& out, unsigned char team) const = 0;
    virtual IAIObject* getObjectFromName(const std::string& name) = 0;
    virtual bool getAllStaticCollisionMeshes(std::list<MeshAndBoundingBox>& meshes, std::list<WaterMesh>& water, std::map<const IAIObject*, DestroyableAddOnMesh>& addOns, std::list<MeshAndBoundingBox>& more, std::pair<Vec2, Vec2>& bounds, unsigned char team) const = 0;
    virtual bool getAllStaticCollisionMeshesWithinBoundary(const Vec2& min, const Vec2& max, std::list<MeshAndBoundingBox>& meshes, std::list<WaterMesh>& water, std::map<const IAIObject*, DestroyableAddOnMesh>& addOns, std::list<MeshAndBoundingBox>& more, std::pair<Vec2, Vec2>& bounds, unsigned char team) const = 0;
    virtual bool isAsleep(unsigned int id) const = 0;
    virtual void getTargetObjects(std::vector<TargetData>& out, int team) const = 0;
    virtual bool loadOvergrowthCollision(const std::string& path) const = 0;
    virtual bool initAiCombatAreas() const = 0;
    virtual bool isAiPositionInsideCombatArea(const Vec3& position) const = 0;
    virtual int getAiCombatAreaLayer() const = 0;
    virtual void getAICombatArea(std::list<Vec2>& out) const = 0;
    virtual void damageSeverelyDamagedVehicles() = 0;
    virtual int collideLineWithObjects(const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore, bool a, float b, bool c, bool& hit) = 0;
};

class BFObjectEnvironment : public IAIObjectEnvironment
{
public:
    BFObjectEnvironment();
    virtual ~BFObjectEnvironment();  // ELF / / ( is )

    virtual void getWorldFrustumObjects(std::vector<IAIObject*>& out, const Frustum& frustum, AIObjectPredicate* filter) const;
    virtual void getWorldSphereObjects(std::vector<IAIObject*>& out, const Vec3& centre, float radius, AIObjectPredicate* filter) const;
    virtual void getAllWorldObjects(std::vector<IAIObject*>& out, AIObjectPredicate* filter) const;
    virtual void getAllDestroyableObjects(std::vector<DestroyableObject>& out) const;
    virtual void getWorldPlayerIds(std::vector<int>& out) const;
    virtual void getWorldPlayerOccupiedObjects(std::vector<IAIObject*>& out, AIObjectPredicate* filter) const;
    virtual void getWorldPlayerOccupiedObjectsWithIds(std::vector<std::pair<int, IAIObject*> >& out, AIObjectPredicate* filter) const;
    virtual IAIObject* getLocalPlayerObject() const;
    virtual bool getLocalPlayerPosition(Vec3& position) const;
    virtual bool getLocalPlayerCameraTransformation(Mat4& transform) const;
    virtual int getLocalPlayerId() const;
    virtual const std::vector<int>& getActivePlayerIds() const;
    virtual bool isValidPlayerId(int id) const;
    virtual int getNextPlayerId(int id) const;
    virtual bool collideLineWithWorld(const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore, bool a, float b, bool c, bool& hit);
    virtual bool collideLineWithWorld(const Vec3& from, const Vec3& to, AIObjectPredicate* filter, Vec3* hit);
    virtual bool getMaxHeight(const Vec3& from, const Vec3& to, AIObjectPredicate* filter, float& height);
    virtual bool hasIndirectLineOfFire(IWeapon* weapon, const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore);
    virtual void pickRandomSensePosition(Vec3& position, Vec3& localPosition, Vec3 hint, IAIObject* object);
    virtual bool pickSoldierRandomSensePosition(Vec3& position, Vec3& localPosition, IAIObject* object, unsigned int& part);
    virtual bool getSoldierSensePosition(Vec3& position, Vec3& localPosition, IAIObject* object, unsigned int part);
    virtual int getHeightAndNormalForObject(IAIObject* object, const Vec3& position, float& height, Vec3& normal, float& slope);
    virtual void getVehicleSpawnerPositions(std::vector<Vec3>& out, unsigned char team) const;
    virtual IAIObject* getObjectFromName(const std::string& name);
    virtual bool getAllStaticCollisionMeshes(std::list<MeshAndBoundingBox>& meshes, std::list<WaterMesh>& water, std::map<const IAIObject*, DestroyableAddOnMesh>& addOns, std::list<MeshAndBoundingBox>& more, std::pair<Vec2, Vec2>& bounds, unsigned char team) const;
    virtual bool getAllStaticCollisionMeshesWithinBoundary(const Vec2& min, const Vec2& max, std::list<MeshAndBoundingBox>& meshes, std::list<WaterMesh>& water, std::map<const IAIObject*, DestroyableAddOnMesh>& addOns, std::list<MeshAndBoundingBox>& more, std::pair<Vec2, Vec2>& bounds, unsigned char team) const;
    virtual bool isAsleep(unsigned int id) const;
    virtual void getTargetObjects(std::vector<TargetData>& out, int team) const;
    virtual bool loadOvergrowthCollision(const std::string& path) const;
    virtual bool initAiCombatAreas() const;
    virtual bool isAiPositionInsideCombatArea(const Vec3& position) const;
    virtual int getAiCombatAreaLayer() const;
    virtual void getAICombatArea(std::list<Vec2>& out) const;
    virtual void damageSeverelyDamagedVehicles();
    virtual int collideLineWithObjects(const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore, bool a, float b, bool c, bool& hit);

private:
    void createMeshFromObject(world::ICollisionMesh* collision, int lod, const Mat4& transform, Mesh& mesh) const;
    void createBoundingBoxFromLocalCoordinates(const Vec3& min, const Vec3& max, const Mat4& transform, Mesh& mesh) const;
    void createBoundingBoxFromWorldCoordinates(const Vec3& min, const Vec3& max, Mesh& mesh) const;
    void createBoundingBoxFromCombatAreas(std::pair<Vec2, Vec2>& bounds) const;
    bool objectOverlapsCombatAreas(const world::IObject* object, std::pair<Vec2, Vec2>& bounds) const;
    bool hasOnlyInfantryCollMesh(world::IObject* object) const;

    void pushAIObjects(std::vector<IAIObject*>& out, std::vector<world::IObject*> objects) const;

    /* +0x00 */ // vptr
    // The element types come from the std::vector instantiations each member's users call
    // (erase / _M_fill_insert / _M_insert_aux); the destructor frees the three in reverse order.
    // [CHOSEN] member names.
    /* +0x04 */ std::vector<world::IObject*> m_senseObjects;   // pickRandomSensePosition's scratch
    /* +0x10 */ std::vector<int> m_activePlayerIds;            // getActivePlayerIds' answer
    /* +0x1C */ std::vector<CombatArea*> m_combatAreas;        // the AI combat-area set
    /* +0x28 */ unsigned char m_byte28;            // [UNRESOLVED] ctor 0
    /* +0x2C */ double m_lastSevereDamageTime;     // [CHOSEN] name; ELF +0x2C / PE +0x38, ctor 0.0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_BFOBJECTENVIRONMENT_H
