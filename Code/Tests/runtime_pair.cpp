// scaffolding test - not recovered game source and not part of the BF2 target.
// Exercises the runtime pair, Object component ABI, template ownership and teardown paths.

#include "../BF2/Core/Ids.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Random.h"
#include "../BF2/Animation/BoneAnimation/Skeleton.h"
#include "../BF2/Game/IGameLogic.h"
#include "../BF2/Game/Objects/Base/Bundle.h"
#include "../BF2/Game/Objects/Base/BundleTemplate.h"
#include "../BF2/Game/Objects/Base/SimpleObject.h"
#include "../BF2/Game/Objects/Base/SimpleObjectTemplate.h"
#include "../BF2/Game/Objects/Visual/LightSource.h"
#include "../BF2/Game/Objects/Visual/LightSourceTemplate.h"
#include "../BF2/Game/Objects/IObjectTemplateComponent.h"
#include "../BF2/Game/Objects/ItemContainerTemplate.h"
#include "../BF2/IO/Network/INetworkable.h"
#include "../BF2/IO/Network/NetworkableInfo.h"
#include "../BF2/IO/Stream/IStream.h"
#include "../BF2/Physics/BasicPhysicsSystem.h"
#include "../BF2/Physics/CollisionManager.h"
#include "../BF2/Physics/CollisionStats.h"
#include "../BF2/Physics/GeomUtil.h"
#include "../BF2/Physics/ICollisionMesh.h"
#include "../BF2/Physics/LatencyCompensation.h"
#include "../BF2/Physics/PhysicsNode.h"
#include "../BF2/Physics/PhysicsType.h"
#include "../BF2/Physics/PhysicsNodeManager.h"
#include "../BF2/Physics/PhysicsSpring.h"
#include "../BF2/Physics/PointResponsePhysics.h"
#include "../BF2/Physics/ResponsePhysics.h"
#include "../BF2/Physics/ResponsePhysicsManager.h"
#include "../BF2/Physics/RotationalPointPhysicsNode.h"
#include "../BF2/Physics/RotationalPointResponsePhysics.h"
#include "../BF2/Physics/SoldierPhysicsNode.h"
#include "../BF2/Physics/SoldierResponse.h"
#include "../BF2/Physics/StaticResponsePhysics.h"
#include "../BF2/Game/Objects/Game/Engine.h"
#include "../BF2/Game/Objects/Game/RopeLink.h"
#include "../BF2/Game/Objects/Game/Soldier.h"
#include "../BF2/Scene/Object/WorldObjTemplPortal/PortalManager.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/ObjectUtil.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../BF2/Sound/ISoundObjectTemplate.h"
#include "../BF2/AI/AIInterface/BFKnowledge/AITemplate.h"          // [TEMP] AI link scaffolding
#include "../BF2/AI/AIInterface/BFKnowledge/AITemplateManager.h"   //   below
#include "../BF2/AI/AIInterface/BFKnowledge/KitTemplateManager.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

using dice::hfe::Mat4;
using dice::hfe::LocalBoundingBox;
using dice::hfe::Vec3;
using dice::hfe::WeakPtr;
using dice::hfe::world::Bundle;
using dice::hfe::world::BundleTemplate;
using dice::hfe::world::CollisionMeshTemplate;
using dice::hfe::world::ICollisionManager;
using dice::hfe::world::ICollisionMesh;
using dice::hfe::world::IObject;
using dice::hfe::world::IObjectTemplate;
using dice::hfe::world::ItemContainerTemplate;
using dice::hfe::world::LightSource;
using dice::hfe::world::LightSourceTemplate;
using dice::hfe::world::Object;
using dice::hfe::world::PortalManager;
using dice::hfe::world::SimpleObject;
using dice::hfe::world::SimpleObjectTemplate;

namespace dice { namespace hfe {
extern ClassManager* classManager;
extern IGameLogic* gameLogic;
namespace io { extern IBase* networkableInfoManager; }
namespace world {
extern IBase* materialManager;
extern PhysicsNodeManager* physicsNodeManager;
extern ResponsePhysicsManager* responsePhysicsManager;
}
} }

static_assert(sizeof(void*) != 4 || sizeof(IObject) == 0x80,
              "PE x86 IObject layout drift");
static_assert(sizeof(LocalBoundingBox) == 0x3C,
              "LocalBoundingBox PE/ELF value layout drift");
static_assert(offsetof(LocalBoundingBox, m_min) == 0x00 &&
                  offsetof(LocalBoundingBox, m_max) == 0x0C &&
                  offsetof(LocalBoundingBox, m_radiusFromOrigin) == 0x18 &&
                  offsetof(LocalBoundingBox, m_radiusFromCentre) == 0x1C &&
                  offsetof(LocalBoundingBox, m_derivedDataDirty) == 0x20 &&
                  offsetof(LocalBoundingBox, m_size) == 0x24 &&
                  offsetof(LocalBoundingBox, m_centre) == 0x30,
              "LocalBoundingBox member offsets drift");
// The shipped PE used an older 0x1C-byte std::string. Modern x86 MSVC therefore lays out the
// STL-bearing Object/SimpleObject/Bundle as 0x138/0x144/0x148 instead of 0x144/0x150/0x154;
// padding the reconstruction to force those historical sizes would make the source model false.

namespace runtime_pair_scaffold {

std::vector<std::string>* enableCachedEvents = 0;
int simpleCollisionCalls = 0;
bool simpleCollisionResult = true;
IObject* simpleCollisionObject = 0;
Vec3 simpleCollisionPosition = {0.0f, 0.0f, 0.0f};
Vec3 simpleCollisionNormal = {0.0f, 0.0f, 0.0f};
Vec3 simpleCollisionVelocity = {0.0f, 0.0f, 0.0f};
int simpleCollisionMaterial = 0;
int simpleCollisionLod = 0;
dice::hfe::world::CollisionFaceInfo* simpleCollisionFirstFace = 0;
dice::hfe::world::CollisionFaceInfo* simpleCollisionSecondFace = 0;

void resetSimpleCollisionCall(bool result)
{
    simpleCollisionCalls = 0;
    simpleCollisionResult = result;
    simpleCollisionObject = 0;
    simpleCollisionMaterial = 0;
    simpleCollisionLod = 0;
    simpleCollisionFirstFace = 0;
    simpleCollisionSecondFace = 0;
}

} // namespace runtime_pair_scaffold

// Link-completion scaffolding for virtuals and dependency-bounded lifecycle seams deliberately left
// undefined in recovered production source. These let the focused executable link cleanly without
// /force; tests below distinguish the exercised production bodies from these explicit fixtures.
namespace dice {
namespace hfe {

Random g_random;

unsigned int Random::getRaw()
{
    const unsigned int factors[2] = {24112u, 26143u};
    for (int i = 0; i != 2; ++i)
    {
        const unsigned int lo = (m_seed & 0xFFFFu) * factors[i];
        const unsigned int hi = (m_seed >> 16) * factors[i];
        const int carry = static_cast<int>(lo >> 16) + static_cast<int>(hi);
        unsigned int next = (lo & 0xFFFFu)
                          + ((static_cast<unsigned int>(carry) & 0x7FFFu) << 16)
                          + static_cast<unsigned int>(carry >> 15);
        const int reduced = static_cast<int>(next - 0x7FFFFFFFu);
        m_seed = reduced < 0 ? next : static_cast<unsigned int>(reduced);
    }
    return m_seed;
}

namespace world {

ClassManager* classManager = 0;

// CollisionMeshTemplate owns these two retail debug vectors.  The runtime-pair harness does not
// link that dependency-heavy concrete template, while ResponsePhysicsManager's recovered child
// walker is required by Object.cpp and names the globals from an otherwise unexercised debug arm.
std::vector<Vec3> collisionTestedFaces;
std::vector<Vec3> collisionIntersectFaces;


void RopeLink::setAwake() {}

float EngineBase::getCurrentDifferentialRPM(float) const { return 0.0f; }
void Engine::feedbackLoop(Vec3, const Vec3&) {}
void Engine::setTrackHasContact(float) {}
float PhysicsSpring::getWheelRadius() const { return 0.1f; }

bool LatencyCompensation::m_isEnabled = false;
float LatencyCompensation::getLatency(IPlayer*) { return 0.0f; }

void Soldier::setHitByVehicleSpeed(const Vec3&) {}
dice::anim::Skeleton* Soldier::getCollisionSkeleton(int) { return 0; }
IPlayer* projectileOwner(void*) { return 0; }

}
// [TEMP]
// Link-only AI template-manager scaffolding. Bot creation is never exercised; these fallbacks return null.
namespace ai {
AITemplateManager* AITemplateManager::m_instance = 0;
KitTemplateManager* KitTemplateManager::m_instance = 0;
AITemplate* AITemplateManager::getTemplate(std::string) { return 0; }
KitTemplate* KitTemplateManager::getTemplate(const std::string&) const { return 0; }
IAIObject* AITemplate::createObject(int, world::IObject*) { return 0; }
} // namespace ai
} // namespace hfe

namespace anim {

bool Skeleton::checkRayVsSkeleton(hfe::Vec3*, hfe::Vec3*, int*, int*, float*,
                                  const hfe::Vec3&, const hfe::Vec3&,
                                  const hfe::Mat4&)
{
    return false;
}

} // namespace anim
} // namespace dice

namespace {

const unsigned int kIID_ISoundObjectTemplate = 0x00985FD2u;
const unsigned int kAlternateComponentIID = 0x7F010203u;
const unsigned int kGeneralBaseComponentID = 0x7F010204u;
const unsigned int kGeneralObjectComponentID = 0x7F010205u;

class Checks
{
public:
    Checks() : m_checks(0), m_failures(0) {}

    void expect(bool condition, const char* message)
    {
        ++m_checks;
        if (!condition)
        {
            ++m_failures;
            std::cerr << "FAIL: " << message << std::endl;
        }
    }

    int checks() const { return m_checks; }
    int failures() const { return m_failures; }

private:
    int m_checks;
    int m_failures;
};

bool near(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 0.0001f;
}

bool nearVec3(const Vec3& value, float x, float y, float z)
{
    return near(value.x, x) && near(value.y, y) && near(value.z, z);
}

void testCapsuleEdgesCollision(Checks& checks)
{
    const Vec3 v0 = { -1.0f, 0.0f, 0.0f };
    const Vec3 v1 = {  0.0f, 0.0f, 10.0f };
    const Vec3 v2 = {  1.0f, 0.0f, 0.0f };
    const Vec3 unusedFaceNormal = { 0.0f, 1.0f, 0.0f };

    Vec3 position = { 11.0f, 12.0f, 13.0f };
    Vec3 normal = { 21.0f, 22.0f, 23.0f };
    float distance = 31.0f;
    const Vec3 missStart = { 0.0f, 10.0f, 10.0f };
    const Vec3 missEnd = { 0.0f, 11.0f, 10.0f };
    checks.expect(!dice::hfe::world::checkCapsuleEdgesCollision(
                      v0, v1, v2, unusedFaceNormal, missStart, missEnd, 0.1f,
                      position, normal, distance) &&
                      nearVec3(position, 11.0f, 12.0f, 13.0f) &&
                      nearVec3(normal, 21.0f, 22.0f, 23.0f) && near(distance, 31.0f),
                  "capsule-edge miss leaves all output arguments untouched");

    const Vec3 crossingStart = { 0.0f, -1.0f, 0.25f };
    const Vec3 crossingEnd = { 0.0f, 1.0f, 0.25f };
    checks.expect(dice::hfe::world::checkCapsuleEdgesCollision(
                      v0, v1, v2, unusedFaceNormal, crossingStart, crossingEnd, 0.5f,
                      position, normal, distance) &&
                      nearVec3(position, 0.0f, 0.0f, 0.0f) &&
                      nearVec3(normal, 0.0f, 0.0f, 1.0f) && near(distance, -0.25f),
                  "capsule-edge crossing returns position before normal and penetration depth");

    const Vec3 downwardStart = { 0.0f, -0.25f, 0.0f };
    const Vec3 downwardEnd = { 0.0f, -1.0f, 0.0f };
    checks.expect(dice::hfe::world::checkCapsuleEdgesCollision(
                      v0, v1, v2, unusedFaceNormal, downwardStart, downwardEnd, 0.5f,
                      position, normal, distance) &&
                      nearVec3(position, 0.0f, 0.0f, 0.0f) &&
                      nearVec3(normal, 0.0f, 1.0f, 0.0f) && near(distance, 0.5f),
                  "capsule-edge downward normal flips upward and restores the radius to distance");
}

void testPhysicsTypeStream(Checks& checks)
{
    using dice::hfe::world::PhysicsType;
    using dice::hfe::world::PhysicsType_Mesh;
    using dice::hfe::world::PhysicsType_None;
    using dice::hfe::world::PhysicsType_Platform;
    using dice::hfe::world::PhysicsType_Point;
    using dice::hfe::world::PhysicsType_RotationalPoint;

    PhysicsType type = PhysicsType_Mesh;
    std::istringstream point("Point");
    point >> type;
    checks.expect(type == PhysicsType_Point,
                  "PhysicsType accepts the stock symbolic Point override");

    std::istringstream numeric("3");
    numeric >> type;
    checks.expect(type == PhysicsType_Mesh,
                  "PhysicsType normalizes the numeric Mesh alias");

    std::istringstream rotational("RotationalPoint");
    rotational >> type;
    checks.expect(type == PhysicsType_RotationalPoint,
                  "PhysicsType accepts RotationalPoint");

    std::istringstream platform("4");
    platform >> type;
    checks.expect(type == PhysicsType_Platform,
                  "PhysicsType normalizes the numeric Platform alias");

    std::istringstream wrongCase("point");
    wrongCase >> type;
    checks.expect(type == PhysicsType_None,
                  "PhysicsType symbolic parsing remains case-sensitive");

    std::istringstream longNumericPrefix("3junk");
    longNumericPrefix >> type;
    checks.expect(type == PhysicsType_None,
                  "PhysicsType rejects numeric prefixes beyond retail's three-byte gate");

    std::ostringstream output;
    output << static_cast<PhysicsType>(99);
    checks.expect(output.str() == "None",
                  "PhysicsType emits None for unknown values");
}

void testMatrixMultiplyPaths(Checks& checks)
{
    Mat4 nonAffine = Mat4::getIdentity();
    nonAffine.m[3] = 2.0f;
    nonAffine.m[7] = 3.0f;
    nonAffine.m[11] = 4.0f;
    nonAffine.m[15] = 5.0f;

    const Mat4 full = Mat4::getIdentity() * nonAffine;
    checks.expect(near(full.m[12], 2.0f) && near(full.m[13], 3.0f) &&
                      near(full.m[14], 4.0f) && near(full.m[15], 5.0f),
                  "Mat4 operator* preserves the full transposed non-affine result");

    Mat4 affine;
    affine.mult(Mat4::getIdentity(), nonAffine);
    checks.expect(near(affine.m[3], 0.0f) && near(affine.m[7], 0.0f) &&
                      near(affine.m[11], 0.0f) && near(affine.m[15], 1.0f),
                  "Mat4::mult remains the distinct affine fast path");
}

// Canonical sound interface collaborator: a second lookalike declaration would hide dispatch UB.
class SoundTemplateProbe : public dice::hfe::ISoundObjectTemplate
{
public:
    explicit SoundTemplateProbe(std::vector<std::string>* events)
        : m_events(events), m_refs(1), m_relativeCalls(0), m_lastPointer(0),
          m_lastRelative(Mat4::getIdentity()) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    void setIsUsed() override { std::abort(); }
    void setFilename(const std::string&) override { std::abort(); }
    std::string getFilename() const override { std::abort(); }
    void setVolumeEnvelope(const std::string&) override { std::abort(); }
    std::string getVolumeEnvelope() const override { std::abort(); }
    void setPitchEnvelope(const std::string&) override { std::abort(); }
    std::string getPitchEnvelope() const override { std::abort(); }
    void setLoopCount(int) override { std::abort(); }
    int getLoopCount() const override { std::abort(); }
    void setVolume(float) override { std::abort(); }
    float getVolume() const override { std::abort(); }
    void setPitch(float) override { std::abort(); }
    float getPitch() const override { std::abort(); }
    void setMinDistance(float) override { std::abort(); }
    float getMinDistance() const override { std::abort(); }
    void setHalfVolumeDistance(float) override { std::abort(); }
    float getHalfVolumeDistance() const override { std::abort(); }
    void setReverbLevel(float) override { std::abort(); }
    float getReverbLevel() const override { std::abort(); }
    void setSoundRadius(float) override { std::abort(); }
    float getSoundRadius() const override { std::abort(); }
    void setLowSamples(int) override { std::abort(); }
    int getLowSamples() const override { std::abort(); }
    void setMediumSamples(int) override { std::abort(); }
    int getMediumSamples() const override { std::abort(); }
    void setRelativeTransformation(const Mat4* transformation) override
    {
        if (m_events) m_events->push_back("soundRelativeTransformation");
        ++m_relativeCalls;
        m_lastPointer = transformation;
        if (transformation) m_lastRelative = *transformation;
    }
    dice::hfe::ISoundTemplate* impl() const override { std::abort(); }

    int m_relativeCalls;
    const Mat4* m_lastPointer;
    Mat4 m_lastRelative;

private:
    std::vector<std::string>* m_events;
    int m_refs;
};

// Only the driver-decal operations are exercised. Other canonical methods fail if reached.
class PlayerControlProbe : public dice::hfe::world::IPlayerControlObject
{
public:
    PlayerControlProbe()
        : m_refs(1), m_driverObject(0), m_setCalls(0), m_transformCalls(0),
          m_lastTransformation(Mat4::getIdentity()) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    using IPlayer = dice::hfe::world::IPlayer;
    using ICameraObject = dice::hfe::world::ICameraObject;
    using VehicleMod = dice::hfe::world::VehicleMod;
    IPlayer* getOccupingPlayer() const override { std::abort(); }
    IObject* getOccupingObject() const override { std::abort(); }
    bool isOccupied() const override { std::abort(); }
    bool enter(IPlayer*) override { std::abort(); }
    void exit() override { std::abort(); }
    void exitPlayer() override { std::abort(); }
    void onEnter(IObject*) override { std::abort(); }
    void onExit() override { std::abort(); }
    int getPcoId() const override { std::abort(); }
    void setPcoId(int) override { std::abort(); }
    std::map<int, ICameraObject*>* getCameras() override { std::abort(); }
    std::map<int, ICameraObject*>* getOwnCameras() override { std::abort(); }
    void nextCamera() override { std::abort(); }
    ICameraObject* getActiveCamera() override { std::abort(); }
    void setActiveCamera(int) override { std::abort(); }
    bool getExitPosition(Vec3&) override { std::abort(); }
    bool getAltExitPosition(Vec3&) override { std::abort(); }
    void addExternalCamera(ICameraObject*) override { std::abort(); }
    void removeExternalCamera(ICameraObject*) override { std::abort(); }
    bool canSpawn(const Mat4&) override { std::abort(); }
    std::map<int, IPlayerControlObject*>* getPcos() override { std::abort(); }
    const std::vector<int>* getPcoPosId() override { std::abort(); }
    void addWeapon(IObject*) override { std::abort(); }
    void* getWeapons() override { std::abort(); }
    IObject* getActiveWeapon(int) const override { std::abort(); }
    void* getWeapons(int) override { std::abort(); }
    void setActiveWeapon(IObject*, int) override { std::abort(); }
    void toggleOnWeaponLock(IObject*) override { std::abort(); }
    int getArtPos() override { std::abort(); }
    bool checkSpecialToggle(dice::hfe::io::PlayerInput) override { std::abort(); }
    bool checkSpecialToggleLastUpdate(dice::hfe::io::PlayerInputMap) override { std::abort(); }
    void setTeam(int) override { std::abort(); }
    void setDefaultTeam(int) override { std::abort(); }
    int getDefaultTeam() override { std::abort(); }
    int getTeam() override { std::abort(); }
    float getTimeToLiveUnused() override { std::abort(); }
    void setTimeToLiveUnused(float) override { std::abort(); }
    bool isAllowedToExit() override { std::abort(); }
    void addPlayerInputObject(IObject*) override { std::abort(); }
    void removePlayerInputObject(IObject*) override { std::abort(); }
    void setTimeSpawned(float) override { std::abort(); }
    float getTimeSpawned() const override { std::abort(); }
    void setObjectSpawnerId(int) override { std::abort(); }
    int getObjectSpawnerId() const override { std::abort(); }
    void setObjectSpawnerHolding(bool) override { std::abort(); }
    bool getObjectSpawnerHolding() const override { std::abort(); }
    const Vec3& getNameTagOffset() override { std::abort(); }
    const Vec3& getRidingWithOffset() override { std::abort(); }
    float getRidingWithDistMod() override { std::abort(); }
    int getVehicleCameraShake() override { std::abort(); }
    float getVehicleFov() override { std::abort(); }
    void addVehicleMod(VehicleMod) override { std::abort(); }
    void removeVehicleMod(VehicleMod) override { std::abort(); }
    int getVehicleMod(VehicleMod) override { std::abort(); }
    const std::string& getSoundFilter() const override { std::abort(); }
    int getCockpitLod() override { std::abort(); }
    int getCockpitSubGeom() override { std::abort(); }
    float getShakeFactor() const override { std::abort(); }
    bool getIsSprinting() const override { std::abort(); }
    void setIsSprinting(bool) override { std::abort(); }
    float getSprintStamina() const override { std::abort(); }
    void setSprintStamina(float) override { std::abort(); }
    void setSprintStaminaDissipationFactor(float) override { std::abort(); }
    float getSprintFactor() override { std::abort(); }
    bool getIsUnderwater() const override { std::abort(); }
    float getNormalizedUnderwaterTime() const override { std::abort(); }
    virtual void setDriverDecalObject(IObject* object)
    {
        ++m_setCalls;
        m_driverObject = object;
    }
    virtual IObject* getDriverDecalObject() const { return m_driverObject; }
    virtual void addDriverDecalTransformation(const Mat4& transformation)
    {
        ++m_transformCalls;
        m_lastTransformation = transformation;
    }
    std::vector<Mat4>* getDriverDecalTransformations() const override { std::abort(); }
    int getSpecialToggleWeaponInput() const override { std::abort(); }
    void setIsDucking(bool) override { std::abort(); }
    bool getIsDucking() override { std::abort(); }
    bool getAutoUseAbility() const override { std::abort(); }
    bool applyCameraAnimation(const Mat4&, Mat4&) override { std::abort(); }
    bool allowEnterOtherVehicle() override { std::abort(); }
    bool isChildSoldierActive() const override { std::abort(); }
    bool getNeverDrawAs1p() const override { std::abort(); }
    void removeChildVehicle(IObject*) override { std::abort(); }
    int getAmmoStatus() override { std::abort(); }
    void setAmmoStatus(int) override { std::abort(); }
    void onFirstGhost() override { std::abort(); }
    void setGiveUnmannedInput(bool) override { std::abort(); }
    void preHandlePlayerInput(dice::hfe::io::PlayerInput&, float) const override { std::abort(); }

    IObject* m_driverObject;
    int m_setCalls;
    int m_transformCalls;
    Mat4 m_lastTransformation;

private:
    int m_refs;
};

// Full primary-table fixture for ObjectTemplateManager. MSVC groups overloaded virtuals in its own
// order, so a prefix-only lookalike is not ABI-compatible with the named getTemplate(string) call
// in BundleTemplate.cpp. Mirroring the complete overload set keeps that production dispatch under
// test without constructing the dependency-heavy concrete manager.
class TemplateManagerLookupProbe : public dice::hfe::IBase,
                                   public dice::hfe::IEventListener
{
public:
    TemplateManagerLookupProbe()
        : m_refs(1), m_result(0), m_lookupCalls(0), m_classNameCalls(0),
          m_lastClassNameId(0), m_className() {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual void setEngineIsInitialized() {}
    virtual void registerTemplateClass(unsigned int, const std::string&) {}
    virtual void getTemplateClasses(
        std::list<std::pair<unsigned int, std::string> >&) const {}
    virtual const std::string& getClassNameFromTemplateClassID(unsigned int id) const
    {
        ++m_classNameCalls;
        m_lastClassNameId = id;
        return m_className;
    }
    virtual unsigned int getClassIDFromTemplateClassName(const std::string&) const { return 0; }
    virtual IObjectTemplate* getTemplate(const std::string&) const
    {
        ++m_lookupCalls;
        return m_result;
    }
    virtual IObjectTemplate* getTemplate(const std::string&, unsigned int) const { return 0; }
    virtual IObjectTemplate* getTemplate(IObject*, unsigned int) const { return 0; }
    virtual IObjectTemplate* getTemplate(const IObjectTemplate*, unsigned int) const { return 0; }
    virtual bool renameTemplate(const std::string&, const std::string&) { return false; }
    virtual void deleteAll(bool) {}
    virtual void deleteAllNotUsed() {}
    virtual bool deleteTemplate(const std::string&) { return false; }
    virtual bool deleteTemplate(IObjectTemplate*) { return false; }
    virtual IObject* getObjectOfCIDAndInputId(IObject*, unsigned int, int) const { return 0; }
    virtual IObject* getObjectOfIIDAndInputId(IObject*, unsigned int, int) const { return 0; }
    virtual IObjectTemplate* getActiveObjectTemplate() const { return 0; }
    virtual void setActiveObjectTemplate(const IObjectTemplate*) {}
    virtual IObjectTemplate* createTemplate(unsigned int, const std::string&) { return 0; }
    virtual IObjectTemplate* createTemplate(const std::string&, const std::string&) { return 0; }
    virtual IObjectTemplate* getTemplate(unsigned int) const { return 0; }
    virtual std::list<IObjectTemplate*>* getGlobalTemplateList() { return 0; }
    virtual bool setWorkingFolderAndFile(const std::string&, const std::string&) { return false; }
    virtual const std::string& getWorkingFolderAndFile(const std::string&) const
    {
        static const std::string empty;
        return empty;
    }
    virtual void getAllUsedTemplates(std::vector<const IObjectTemplate*>&) const {}
    virtual void preCacheObjects() const {}
    virtual void writeBinary(const std::string&) const {}
    virtual void readBinary(const std::string&) {}
    virtual void setDisplayError(bool) {}
    virtual unsigned int getClassID() const { return 0; }
    virtual ~TemplateManagerLookupProbe() {}
    virtual void handleEvent(dice::hfe::EventCategory, unsigned int,
                             dice::hfe::EventNode*, void*) {}

    IObjectTemplate* m_result;
    mutable int m_lookupCalls;
    mutable int m_classNameCalls;
    mutable unsigned int m_lastClassNameId;
    std::string m_className;

private:
    int m_refs;
};

// The two attachment bodies are still deliberately unresolved in production. Overriding only
// those seams keeps the inherited factory body under test while making its constructor path safe.
class FactorySimpleTemplate : public SimpleObjectTemplate
{
public:
    bool specialMaterialLatch() const { return _byte160; }
    const Vec3& portalPassingPositionForTest() const
    {
        return m_portalPassingPosition;
    }
    std::size_t portalPassingPositionSpanForTest() const
    {
        const unsigned char* first =
            reinterpret_cast<const unsigned char*>(&m_portalPassingPosition.x);
        const unsigned char* afterLast =
            reinterpret_cast<const unsigned char*>(&m_portalPassingPosition.z) + sizeof(float);
        return static_cast<std::size_t>(afterLast - first);
    }

    virtual void setPhysicsNodeComponent(IObject*) const {}
    virtual void setResponsePhysicsComponent(IObject*) const {}
};

class CacheLifecycleTemplateProbe : public FactorySimpleTemplate
{
public:
    CacheLifecycleTemplateProbe(const std::string& name, unsigned int flags,
                                std::vector<std::string>* events)
        : m_events(events), m_getFlagsCalls(0), m_updateFlagsCalls(0),
          m_objectCreatedCalls(0), m_lastSet(0), m_lastClear(0)
    {
        m_templateName = name;
        m_flags = flags;
    }

    virtual const std::string& getName() const
    {
        if (m_events) m_events->push_back(std::string("template:getName:") + m_templateName);
        return m_templateName;
    }

    virtual unsigned int getFlags() const
    {
        if (m_events) m_events->push_back("template:getFlags");
        ++m_getFlagsCalls;
        return m_flags;
    }

    virtual unsigned int updateFlags(unsigned int set, unsigned int clear)
    {
        if (m_events) m_events->push_back("template:updateFlags");
        ++m_updateFlagsCalls;
        m_lastSet = set;
        m_lastClear = clear;
        return ObjectTemplate::updateFlags(set, clear);
    }

    virtual void objectCreated(IObject*)
    {
        if (m_events) m_events->push_back("template:objectCreated");
        ++m_objectCreatedCalls;
    }

    mutable int m_getFlagsCalls;
    int m_updateFlagsCalls;
    int m_objectCreatedCalls;
    unsigned int m_lastSet;
    unsigned int m_lastClear;

private:
    std::vector<std::string>* m_events;
};

class SpringClassIdTemplate : public FactorySimpleTemplate
{
public:
    virtual unsigned int getClassID() const
    {
        return dice::hfe::world::CID_SpringTemplate;
    }
};

class HarnessBundleTemplate : public BundleTemplate
{
public:
    virtual void setPhysicsNodeComponent(IObject*) const {}
    virtual void setResponsePhysicsComponent(IObject*) const {}

    void cacheTemplate(int index, IObjectTemplate& objectTemplate)
    {
        m_templateInfos[index]->m_template = objectTemplate.getWeakPtr();
    }

    IObjectTemplate* cachedTarget(int index) const
    {
        return m_templateInfos[index]->m_template.get();
    }

    bool relativeOffsetApplied(int index) const
    {
        return m_templateInfos[index]->m_unknown50;
    }

    void enableCollisionPhysics() { m_physicsFlags |= 0x04u; }

    // BundleTemplate +0x178: the flag Bundle::handleMessage tests before message 24.
    void setLaunchOnMessageForTest(bool value) { m_explodePartAtDestroy = value; }

};

class BundleRuntimeProbe : public Bundle
{
public:
    explicit BundleRuntimeProbe(const BundleTemplate* objectTemplate)
        : Bundle(objectTemplate) {}

    void setRawGeometryForTest(void* geometry) { m_geometry = geometry; }
    void setRawPhysicsForTest(void* physicsNode) { m_physicsNode = physicsNode; }
    void setRawArmorForTest(dice::hfe::world::IObjectComponent* armor) { m_armorComponent = armor; }
    void setCacheStateForTest(unsigned int id, unsigned int cacheFlags,
                              unsigned int liveFlags)
    {
        m_objectCacheId = id;
        m_cacheFlags = cacheFlags;
        m_flags = liveFlags;
        m_cachedRootParent = 0;
    }
    unsigned int flagsForTest() const { return m_flags; }
};

class FactoryItemTemplate : public ItemContainerTemplate
{
public:
    virtual void setPhysicsNodeComponent(IObject*) const {}
    virtual void setResponsePhysicsComponent(IObject*) const {}
};

// ObjectTemplate stores the queried template-component pointer while retaining through the
// incoming IBase pointer.  These are deliberately separate objects, but both facets mutate one
// shared count: that models an adjusted interface without manufacturing two unrelated lifetimes.
struct TemplateComponentOwnershipState
{
    explicit TemplateComponentOwnershipState(std::vector<std::string>* eventLog)
        : events(eventLog), refs(1), ownerAddCalls(0), ownerReleaseCalls(0),
          interfaceAddCalls(0), interfaceReleaseCalls(0) {}

    std::vector<std::string>* events;
    int refs;
    int ownerAddCalls;
    int ownerReleaseCalls;
    int interfaceAddCalls;
    int interfaceReleaseCalls;
};

class TemplateComponentFacetProbe : public dice::hfe::world::IObjectTemplateComponent
{
public:
    TemplateComponentFacetProbe(const char* label, unsigned int classId,
                                TemplateComponentOwnershipState* state)
        : m_label(label), m_classId(classId), m_state(state), m_template(0),
          m_componentSeenDuringSetTemplate(0), m_setTemplateCalls(0) {}

    virtual void addRef()
    {
        m_state->events->push_back(std::string("add:") + m_label + ":interface");
        ++m_state->interfaceAddCalls;
        ++m_state->refs;
    }
    virtual int getRef() { return m_state->refs; }
    virtual int release()
    {
        m_state->events->push_back(std::string("release:") + m_label + ":interface");
        ++m_state->interfaceReleaseCalls;
        return --m_state->refs;
    }
    virtual void* queryInterface(unsigned int iid) const
    {
        if (iid == dice::hfe::IID_IBase ||
            iid == dice::hfe::world::IID_IObjectTemplateComponent)
            return const_cast<TemplateComponentFacetProbe*>(this);
        return 0;
    }
    virtual unsigned int getClassID() const { return m_classId; }
    virtual unsigned int getInterfaceID()
    {
        return dice::hfe::world::IID_IObjectTemplateComponent;
    }
    virtual dice::hfe::world::IObjectComponent* createComponent() { return 0; }
    virtual void objectCreated(IObject*) {}
    virtual void objectDestroyed(IObject*) {}
    virtual void setTemplate(IObjectTemplate* objectTemplate)
    {
        m_state->events->push_back(std::string("setTemplate:") + m_label);
        ++m_setTemplateCalls;
        m_template = objectTemplate;
        m_componentSeenDuringSetTemplate = objectTemplate
            ? objectTemplate->queryComponent(m_classId) : 0;
    }
    virtual void* queryImplementation(unsigned int classId)
    {
        return classId == m_classId ? this : 0;
    }

    const char* m_label;
    IObjectTemplate* m_template;
    dice::hfe::world::IObjectTemplateComponent* m_componentSeenDuringSetTemplate;
    int m_setTemplateCalls;

private:
    unsigned int m_classId;
    TemplateComponentOwnershipState* m_state;
};

class TemplateComponentOwnerProbe : public dice::hfe::IBase
{
public:
    TemplateComponentOwnerProbe(const char* label, unsigned int classId,
                                std::vector<std::string>* events)
        : m_label(label), m_state(events), m_component(label, classId, &m_state),
          m_queryCalls(0), m_lastQuery(0) {}

    virtual void addRef()
    {
        m_state.events->push_back(std::string("add:") + m_label + ":owner");
        ++m_state.ownerAddCalls;
        ++m_state.refs;
    }
    virtual int getRef() { return m_state.refs; }
    virtual int release()
    {
        m_state.events->push_back(std::string("release:") + m_label + ":owner");
        ++m_state.ownerReleaseCalls;
        return --m_state.refs;
    }
    virtual void* queryInterface(unsigned int iid) const
    {
        ++m_queryCalls;
        m_lastQuery = iid;
        m_state.events->push_back(std::string("query:") + m_label);
        if (iid == dice::hfe::world::IID_IObjectTemplateComponent)
            return const_cast<TemplateComponentFacetProbe*>(&m_component);
        if (iid == dice::hfe::IID_IBase)
            return const_cast<TemplateComponentOwnerProbe*>(this);
        return 0;
    }

    const char* m_label;
    TemplateComponentOwnershipState m_state;
    TemplateComponentFacetProbe m_component;
    mutable int m_queryCalls;
    mutable unsigned int m_lastQuery;
};

// The incoming IBase and the queried IObjectComponent deliberately live in separate objects.  That
// makes the recovered ownership distinction observable: setComponent stores the queried interface
// but retains the original incoming owner.
class ComponentInterfaceProbe : public dice::hfe::world::IObjectComponent
{
public:
    ComponentInterfaceProbe(const char* label, std::vector<std::string>* events,
                            dice::hfe::IBase* alternate)
        : m_label(label), m_events(events), m_alternate(alternate), m_object(0), m_refs(2),
          m_addCalls(0), m_releaseCalls(0), m_queryCalls(0), m_setObjectCalls(0),
          m_nullSetObjectCalls(0) {}

    virtual void addRef()
    {
        if (m_events) m_events->push_back(std::string("add:") + m_label + ":interface");
        ++m_addCalls;
        ++m_refs;
    }
    virtual int getRef() { return m_refs; }
    virtual int release()
    {
        if (m_events) m_events->push_back(std::string("release:") + m_label + ":interface");
        ++m_releaseCalls;
        return --m_refs;
    }
    virtual void* queryInterface(unsigned int iid) const
    {
        ++m_queryCalls;
        if (iid == dice::hfe::IID_IBase || iid == dice::hfe::world::IID_IObjectComponent ||
            iid == kGeneralObjectComponentID)
            return const_cast<ComponentInterfaceProbe*>(this);
        if (iid == kAlternateComponentIID)
            return m_alternate;
        return 0;
    }
    virtual unsigned int getInterfaceID() const { return kGeneralObjectComponentID; }
    virtual void init(IObject*) {}
    virtual void destroy() {}
    virtual void setObject(IObject* object)
    {
        if (m_events) m_events->push_back(std::string("setObject:") + m_label);
        ++m_setObjectCalls;
        if (!object) ++m_nullSetObjectCalls;
        m_object = object;
    }
    virtual IObject* getObject() const { return m_object; }
    virtual bool hasUpdate() const { return false; }
    virtual void handleUpdate(float) {}
    virtual bool hasFrameUpdate() const { return false; }
    virtual void handleFrameUpdate(float) {}
    virtual void handleMessage(dice::hfe::world::TemplateMessage,
                               dice::hfe::world::IPlayer*, bool) {}

    const char* m_label;
    IObject* m_object;
    int m_addCalls;
    int m_releaseCalls;
    mutable int m_queryCalls;
    int m_setObjectCalls;
    int m_nullSetObjectCalls;

private:
    std::vector<std::string>* m_events;
    dice::hfe::IBase* m_alternate;
    int m_refs;
};

class IncomingOwnerProbe : public dice::hfe::IBase
{
public:
    IncomingOwnerProbe(const char* label, std::vector<std::string>* events,
                       ComponentInterfaceProbe* objectComponent,
                       unsigned int primaryId, void* primaryInterface,
                       dice::hfe::IBase* alternate)
        : m_label(label), m_events(events), m_objectComponent(objectComponent),
          m_primaryId(primaryId), m_primaryInterface(primaryInterface),
          m_alternate(alternate), m_refs(2), m_addCalls(0), m_releaseCalls(0),
          m_queryCalls(0), m_lastQuery(0), m_logQueries(true), m_logRefEvents(true) {}

    virtual void addRef()
    {
        if (m_events && m_logRefEvents)
            m_events->push_back(std::string("add:") + m_label + ":owner");
        ++m_addCalls;
        ++m_refs;
    }
    virtual int getRef() { return m_refs; }
    virtual int release()
    {
        if (m_events && m_logRefEvents)
            m_events->push_back(std::string("release:") + m_label + ":owner");
        ++m_releaseCalls;
        return --m_refs;
    }
    virtual void* queryInterface(unsigned int iid) const
    {
        ++m_queryCalls;
        m_lastQuery = iid;
        if (m_events && m_logQueries)
            m_events->push_back(std::string("query:") + m_label + ":" +
                                std::to_string(iid));
        if (iid == dice::hfe::world::IID_IObjectComponent)
            return m_objectComponent;
        if (iid == m_primaryId)
            return m_primaryInterface ? m_primaryInterface
                                      : const_cast<IncomingOwnerProbe*>(this);
        if (iid == kAlternateComponentIID)
            return m_alternate;
        if (iid == dice::hfe::IID_IBase)
            return const_cast<IncomingOwnerProbe*>(this);
        return 0;
    }

    void resetActivity()
    {
        m_addCalls = 0;
        m_releaseCalls = 0;
        m_queryCalls = 0;
        m_lastQuery = 0;
    }
    void setEventLogging(bool queries, bool refs)
    {
        m_logQueries = queries;
        m_logRefEvents = refs;
    }

    const char* m_label;
    int m_addCalls;
    int m_releaseCalls;
    mutable int m_queryCalls;
    mutable unsigned int m_lastQuery;

private:
    std::vector<std::string>* m_events;
    ComponentInterfaceProbe* m_objectComponent;
    unsigned int m_primaryId;
    void* m_primaryInterface;
    dice::hfe::IBase* m_alternate;
    int m_refs;
    bool m_logQueries;
    bool m_logRefEvents;
};

class FastInterfaceProbe : public dice::hfe::IBase
{
public:
    FastInterfaceProbe(unsigned int primaryId, dice::hfe::IBase* alternate)
        : m_primaryId(primaryId), m_alternate(alternate), m_refs(2), m_addCalls(0),
          m_releaseCalls(0), m_queryCalls(0), m_lastQuery(0) {}

    virtual void addRef() { ++m_addCalls; ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { ++m_releaseCalls; return --m_refs; }
    virtual void* queryInterface(unsigned int iid) const
    {
        ++m_queryCalls;
        m_lastQuery = iid;
        if (iid == m_primaryId || iid == dice::hfe::IID_IBase)
            return const_cast<FastInterfaceProbe*>(this);
        if (iid == kAlternateComponentIID)
            return m_alternate;
        return 0;
    }

    unsigned int m_primaryId;
    int m_addCalls;
    int m_releaseCalls;
    mutable int m_queryCalls;
    mutable unsigned int m_lastQuery;

private:
    dice::hfe::IBase* m_alternate;
    int m_refs;
};

// IResponsePhysics remains unrecovered as a C++ interface.  This PE fixture pins only the IBase
// prefix and the setDirtyNextToCheck operation at physical +0x58 used by Object.cpp.
class ResponsePhysicsPeAbiProbe : public dice::hfe::IBase
{
public:
    explicit ResponsePhysicsPeAbiProbe(dice::hfe::IBase* alternate = 0,
                                       std::vector<std::string>* events = 0)
        : m_alternate(alternate), m_refs(2), m_addCalls(0), m_releaseCalls(0),
          m_queryCalls(0), m_dirtyCalls(0), m_updateCalls(0),
          m_cacheResetCalls(0), m_setEnabledCalls(0), m_lastEnabled(false),
          m_lastDeltaTime(0.0f), m_events(events) {}

    virtual void addRef() { ++m_addCalls; ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { ++m_releaseCalls; return --m_refs; }
    virtual void* queryInterface(unsigned int iid) const
    {
        ++m_queryCalls;
        if (iid == dice::hfe::world::IID_IResponsePhysics || iid == dice::hfe::IID_IBase)
            return const_cast<ResponsePhysicsPeAbiProbe*>(this);
        if (iid == kAlternateComponentIID)
            return m_alternate;
        return 0;
    }
    virtual ~ResponsePhysicsPeAbiProbe() {}
    virtual void setEnabledForCache(bool enabled)
    {
        if (m_events) m_events->push_back("response:setEnabled");
        ++m_setEnabledCalls;
        m_lastEnabled = enabled;
    }
#define BF2_RESPONSE_TEST_SLOT(n) virtual void slot##n() {}
    BF2_RESPONSE_TEST_SLOT(18);
#undef BF2_RESPONSE_TEST_SLOT
    virtual void resetForCache()
    {
        if (m_events) m_events->push_back("response:reset");
        ++m_cacheResetCalls;
    }
#define BF2_RESPONSE_TEST_SLOT(n) virtual void slot##n() {}
    BF2_RESPONSE_TEST_SLOT(20);
    BF2_RESPONSE_TEST_SLOT(24); BF2_RESPONSE_TEST_SLOT(28);
    BF2_RESPONSE_TEST_SLOT(2C); BF2_RESPONSE_TEST_SLOT(30);
    BF2_RESPONSE_TEST_SLOT(34); BF2_RESPONSE_TEST_SLOT(38);
    BF2_RESPONSE_TEST_SLOT(3C); BF2_RESPONSE_TEST_SLOT(40);
    BF2_RESPONSE_TEST_SLOT(44); BF2_RESPONSE_TEST_SLOT(48);
    BF2_RESPONSE_TEST_SLOT(4C); BF2_RESPONSE_TEST_SLOT(50);
    BF2_RESPONSE_TEST_SLOT(54);
#undef BF2_RESPONSE_TEST_SLOT
    virtual void setDirtyNextToCheck()
    {
        if (m_events) m_events->push_back("responseDirty");
        ++m_dirtyCalls;
    }
#define BF2_RESPONSE_UPDATE_GAP(n) virtual void updateGap##n() {}
    BF2_RESPONSE_UPDATE_GAP(5C); BF2_RESPONSE_UPDATE_GAP(60);
    BF2_RESPONSE_UPDATE_GAP(64); BF2_RESPONSE_UPDATE_GAP(68);
    BF2_RESPONSE_UPDATE_GAP(6C); BF2_RESPONSE_UPDATE_GAP(70);
    BF2_RESPONSE_UPDATE_GAP(74); BF2_RESPONSE_UPDATE_GAP(78);
    BF2_RESPONSE_UPDATE_GAP(7C); BF2_RESPONSE_UPDATE_GAP(80);
    BF2_RESPONSE_UPDATE_GAP(84); BF2_RESPONSE_UPDATE_GAP(88);
    BF2_RESPONSE_UPDATE_GAP(8C); BF2_RESPONSE_UPDATE_GAP(90);
    BF2_RESPONSE_UPDATE_GAP(94); BF2_RESPONSE_UPDATE_GAP(98);
#undef BF2_RESPONSE_UPDATE_GAP
    virtual void update(float deltaTime)
    {
        if (m_events) m_events->push_back("responseUpdate");
        ++m_updateCalls;
        m_lastDeltaTime = deltaTime;
    }

    int m_addCalls;
    int m_releaseCalls;
    mutable int m_queryCalls;
    int m_dirtyCalls;
    int m_updateCalls;
    int m_cacheResetCalls;
    int m_setEnabledCalls;
    bool m_lastEnabled;
    float m_lastDeltaTime;

private:
    dice::hfe::IBase* m_alternate;
    int m_refs;
    std::vector<std::string>* m_events;
};

// Explicit PE views of the two manager prefixes used by ObjectTemplate's inherited removal hooks.
// The ELF interfaces have two destructor entries, but MSVC emits one deleting-destructor slot:
// removePhysicsNode is therefore physical +0x1C and removeObject physical +0x18 in this harness.
class PhysicsNodeManagerPeAbiProbe
{
public:
    explicit PhysicsNodeManagerPeAbiProbe(std::vector<std::string>* events)
        : m_events(events), m_addCalls(0), m_removeCalls(0), m_lastNode(0) {}

    virtual void addRef() {}
    virtual int getRef() { return 1; }
    virtual int release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual ~PhysicsNodeManagerPeAbiProbe() {}
    virtual void slot14_getPhysicsNodeList() {}
    virtual void slot18_addPhysicsNode(dice::hfe::world::IPhysicsNode* node)
    {
        if (m_events) m_events->push_back("physicsManagerAdd");
        ++m_addCalls;
        m_lastNode = node;
    }
    virtual void removePhysicsNode(dice::hfe::world::IPhysicsNode* node)
    {
        if (m_events) m_events->push_back("physicsManagerRemove");
        ++m_removeCalls;
        m_lastNode = node;
    }

    int m_addCalls;
    int m_removeCalls;
    dice::hfe::world::IPhysicsNode* m_lastNode;

private:
    std::vector<std::string>* m_events;
};

class ResponsePhysicsManagerPeAbiProbe
{
public:
    explicit ResponsePhysicsManagerPeAbiProbe(std::vector<std::string>* events)
        : m_events(events), m_addCalls(0), m_removeCalls(0), m_lastObject(0) {}

    virtual void addRef() {}
    virtual int getRef() { return 1; }
    virtual int release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual ~ResponsePhysicsManagerPeAbiProbe() {}
    virtual void slot14_addObject(IObject* object)
    {
        if (m_events) m_events->push_back("responseManagerAdd");
        ++m_addCalls;
        m_lastObject = object;
    }
    virtual void removeObject(IObject* object)
    {
        if (m_events) m_events->push_back("responseManagerRemove");
        ++m_removeCalls;
        m_lastObject = object;
    }

    int m_addCalls;
    int m_removeCalls;
    IObject* m_lastObject;

private:
    std::vector<std::string>* m_events;
};

// PE ObjectManager view through addObjectToDestroyList (+0x118). The focused runtime bodies use
// the registry, list, name, grid and destroy-list slots directly through this static manager.
class ObjectManagerPeAbiProbe
{
public:
    explicit ObjectManagerPeAbiProbe(unsigned int firstId,
                                     std::vector<std::string>* events = 0,
                                     const char* label = "objectManager")
        : m_nextId(firstId), m_newIdCalls(0), m_refreshCalls(0),
          m_destroyObjectCalls(0), m_addDestroyCalls(0),
          m_suppressRefreshTrace(false), m_events(events), m_label(label) {}

#define BF2_OBJECT_MANAGER_TEST_SLOT(n) virtual void slot##n() {}
    BF2_OBJECT_MANAGER_TEST_SLOT(00); BF2_OBJECT_MANAGER_TEST_SLOT(01);
    BF2_OBJECT_MANAGER_TEST_SLOT(02); BF2_OBJECT_MANAGER_TEST_SLOT(03);
    BF2_OBJECT_MANAGER_TEST_SLOT(04);
#undef BF2_OBJECT_MANAGER_TEST_SLOT
    virtual void destroyObject(IObject* object)
    {
        trace("destroyObject");
        ++m_destroyObjectCalls;
        m_destroyed.push_back(object);
    }
#define BF2_OBJECT_MANAGER_TEST_SLOT(n) virtual void slot##n() {}
    BF2_OBJECT_MANAGER_TEST_SLOT(06); BF2_OBJECT_MANAGER_TEST_SLOT(07);
    BF2_OBJECT_MANAGER_TEST_SLOT(08); BF2_OBJECT_MANAGER_TEST_SLOT(09);
    BF2_OBJECT_MANAGER_TEST_SLOT(10); BF2_OBJECT_MANAGER_TEST_SLOT(11);
    BF2_OBJECT_MANAGER_TEST_SLOT(12); BF2_OBJECT_MANAGER_TEST_SLOT(13);
    BF2_OBJECT_MANAGER_TEST_SLOT(14); BF2_OBJECT_MANAGER_TEST_SLOT(15);
    BF2_OBJECT_MANAGER_TEST_SLOT(16); BF2_OBJECT_MANAGER_TEST_SLOT(17);
    BF2_OBJECT_MANAGER_TEST_SLOT(18); BF2_OBJECT_MANAGER_TEST_SLOT(19);
    BF2_OBJECT_MANAGER_TEST_SLOT(20); BF2_OBJECT_MANAGER_TEST_SLOT(21);
    BF2_OBJECT_MANAGER_TEST_SLOT(22); BF2_OBJECT_MANAGER_TEST_SLOT(23);
    BF2_OBJECT_MANAGER_TEST_SLOT(24); BF2_OBJECT_MANAGER_TEST_SLOT(25);
    BF2_OBJECT_MANAGER_TEST_SLOT(26); BF2_OBJECT_MANAGER_TEST_SLOT(27);
    BF2_OBJECT_MANAGER_TEST_SLOT(28); BF2_OBJECT_MANAGER_TEST_SLOT(29);
    BF2_OBJECT_MANAGER_TEST_SLOT(30); BF2_OBJECT_MANAGER_TEST_SLOT(31);
    BF2_OBJECT_MANAGER_TEST_SLOT(32); BF2_OBJECT_MANAGER_TEST_SLOT(33);
    BF2_OBJECT_MANAGER_TEST_SLOT(34); BF2_OBJECT_MANAGER_TEST_SLOT(35);
    BF2_OBJECT_MANAGER_TEST_SLOT(36); BF2_OBJECT_MANAGER_TEST_SLOT(37);
    BF2_OBJECT_MANAGER_TEST_SLOT(38); BF2_OBJECT_MANAGER_TEST_SLOT(39);
#undef BF2_OBJECT_MANAGER_TEST_SLOT
    virtual void refreshGridObject(IObject* object)
    {
        if (!m_suppressRefreshTrace)
            trace("refreshGrid");
        ++m_refreshCalls;
        m_refreshed.push_back(object);
    }
#define BF2_OBJECT_MANAGER_TEST_SLOT(n) virtual void slot##n() {}
    BF2_OBJECT_MANAGER_TEST_SLOT(41);
    BF2_OBJECT_MANAGER_TEST_SLOT(42); BF2_OBJECT_MANAGER_TEST_SLOT(43);
    BF2_OBJECT_MANAGER_TEST_SLOT(44); BF2_OBJECT_MANAGER_TEST_SLOT(45);
    BF2_OBJECT_MANAGER_TEST_SLOT(46); BF2_OBJECT_MANAGER_TEST_SLOT(47);
    BF2_OBJECT_MANAGER_TEST_SLOT(48); BF2_OBJECT_MANAGER_TEST_SLOT(49);
    BF2_OBJECT_MANAGER_TEST_SLOT(50); BF2_OBJECT_MANAGER_TEST_SLOT(51);
    BF2_OBJECT_MANAGER_TEST_SLOT(52); BF2_OBJECT_MANAGER_TEST_SLOT(53);
#undef BF2_OBJECT_MANAGER_TEST_SLOT
    virtual unsigned int getNewID()
    {
        trace("getNewId");
        ++m_newIdCalls;
        return ++m_nextId;
    }
    virtual void registerObject(IObject* object)
    {
        trace("register");
        m_registered.push_back(object);
    }
    virtual void unregisterObject(IObject* object)
    {
        trace("unregister");
        m_unregistered.push_back(object);
    }
    virtual void slot57_deleteAllRegisteredObjects() {}
    virtual void addObjectToNameHash(IObject* object)
    {
        trace("addName");
        m_nameAdded.push_back(object);
    }
    virtual void removeObjectFromNameHash(IObject* object)
    {
        trace("removeName");
        m_nameRemoved.push_back(object);
    }
    virtual void addObjectToUpdate(IObject* object)
    {
        trace("addUpdate");
        m_updateAdded.push_back(object);
    }
    virtual void removeObjectFromUpdate(IObject* object)
    {
        trace("removeUpdate");
        m_updateRemoved.push_back(object);
    }
    virtual void addObjectToFrameUpdate(IObject* object)
    {
        trace("addFrame");
        m_frameAdded.push_back(object);
    }
    virtual void removeObjectFromFrameUpdate(IObject* object)
    {
        trace("removeFrame");
        m_frameRemoved.push_back(object);
    }
    virtual void addObjectToGrid(IObject* object)
    {
        trace("addGrid");
        m_gridAdded.push_back(object);
    }
    virtual void slot65_updateObjectInGrid() {}
    virtual void removeObjectFromGrid(IObject* object)
    {
        trace("removeGrid");
        m_gridRemoved.push_back(object);
    }
    virtual void slot67_getCameraInfo() {}
    virtual void slot68_setOverrideLineTestLod() {}
    virtual void slot69_getOverrideLineTestLod() {}
    virtual void addObjectToDestroyList(IObject* object)
    {
        trace("addDestroy");
        ++m_addDestroyCalls;
        m_destroyAdded.push_back(object);
    }

    unsigned int m_nextId;
    int m_newIdCalls;
    int m_refreshCalls;
    int m_destroyObjectCalls;
    int m_addDestroyCalls;
    std::vector<IObject*> m_registered;
    std::vector<IObject*> m_unregistered;
    std::vector<IObject*> m_refreshed;
    std::vector<IObject*> m_destroyed;
    std::vector<IObject*> m_nameAdded;
    std::vector<IObject*> m_nameRemoved;
    std::vector<IObject*> m_updateAdded;
    std::vector<IObject*> m_updateRemoved;
    std::vector<IObject*> m_frameAdded;
    std::vector<IObject*> m_frameRemoved;
    std::vector<IObject*> m_gridAdded;
    std::vector<IObject*> m_gridRemoved;
    std::vector<IObject*> m_destroyAdded;
    bool m_suppressRefreshTrace;

private:
    void trace(const char* operation)
    {
        if (m_events)
            m_events->push_back(std::string(m_label) + ":" + operation);
    }

    std::vector<std::string>* m_events;
    const char* m_label;
};

// EventManager notification overload used by Object::destroy. MSVC groups the two overloaded
// postEvent members in reverse declaration order, placing this four-argument overload at +0x10.
class EventManagerPeAbiProbe
{
public:
    explicit EventManagerPeAbiProbe(std::vector<std::string>* events = 0)
        : m_events(events), m_postCalls(0), m_category(static_cast<dice::hfe::EventCategory>(0)),
          m_id(0), m_data(0), m_delay(-1.0f) {}

    virtual void addRef() {}
    virtual int getRef() { return 1; }
    virtual int release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual unsigned int postEvent(dice::hfe::EventCategory category,
                                   unsigned int id, void* data, float delay)
    {
        if (m_events) m_events->push_back("eventManager:post");
        ++m_postCalls;
        m_category = category;
        m_id = id;
        m_data = data;
        m_delay = delay;
        return 0;
    }
    virtual unsigned int postNode(dice::hfe::EventCategory,
                                  dice::hfe::EventNode*, float) { return 0; }

    int m_postCalls;
    dice::hfe::EventCategory m_category;
    unsigned int m_id;
    void* m_data;
    float m_delay;

private:
    std::vector<std::string>* m_events;
};

// Raw PE GameLogic table through getTeamDropVehicle (+0x50) and isServer (+0x13C).
// IGameLogic has no destructor slot, so these logical indices are the physical x86 slots.
class GameLogicPeAbiProbe
{
public:
    explicit GameLogicPeAbiProbe(std::vector<std::string>* events = 0)
        : m_events(events), m_server(false), m_serverCalls(0), m_teamCalls(0),
          m_team1("team_drop_one"), m_team2("team_drop_two") {}

#define BF2_GAME_LOGIC_TEST_SLOT(n) virtual void slot##n() {}
    BF2_GAME_LOGIC_TEST_SLOT(00); BF2_GAME_LOGIC_TEST_SLOT(01);
    BF2_GAME_LOGIC_TEST_SLOT(02); BF2_GAME_LOGIC_TEST_SLOT(03);
    BF2_GAME_LOGIC_TEST_SLOT(04); BF2_GAME_LOGIC_TEST_SLOT(05);
    BF2_GAME_LOGIC_TEST_SLOT(06); BF2_GAME_LOGIC_TEST_SLOT(07);
    BF2_GAME_LOGIC_TEST_SLOT(08); BF2_GAME_LOGIC_TEST_SLOT(09);
    BF2_GAME_LOGIC_TEST_SLOT(10); BF2_GAME_LOGIC_TEST_SLOT(11);
    BF2_GAME_LOGIC_TEST_SLOT(12); BF2_GAME_LOGIC_TEST_SLOT(13);
    BF2_GAME_LOGIC_TEST_SLOT(14); BF2_GAME_LOGIC_TEST_SLOT(15);
    BF2_GAME_LOGIC_TEST_SLOT(16); BF2_GAME_LOGIC_TEST_SLOT(17);
    BF2_GAME_LOGIC_TEST_SLOT(18); BF2_GAME_LOGIC_TEST_SLOT(19);
#undef BF2_GAME_LOGIC_TEST_SLOT
    virtual const std::string* getTeamDropVehicle(int team)
    {
        if (m_events)
            m_events->push_back(std::string("gameLogic:team") + std::to_string(team));
        ++m_teamCalls;
        m_teams.push_back(team);
        if (team == 1) return &m_team1;
        if (team == 2) return &m_team2;
        static const std::string empty;
        return &empty;
    }
#define BF2_GAME_LOGIC_TEST_SLOT(n) virtual void slot##n() {}
    BF2_GAME_LOGIC_TEST_SLOT(21); BF2_GAME_LOGIC_TEST_SLOT(22);
    BF2_GAME_LOGIC_TEST_SLOT(23); BF2_GAME_LOGIC_TEST_SLOT(24);
    BF2_GAME_LOGIC_TEST_SLOT(25); BF2_GAME_LOGIC_TEST_SLOT(26);
    BF2_GAME_LOGIC_TEST_SLOT(27); BF2_GAME_LOGIC_TEST_SLOT(28);
    BF2_GAME_LOGIC_TEST_SLOT(29); BF2_GAME_LOGIC_TEST_SLOT(30);
    BF2_GAME_LOGIC_TEST_SLOT(31); BF2_GAME_LOGIC_TEST_SLOT(32);
    BF2_GAME_LOGIC_TEST_SLOT(33); BF2_GAME_LOGIC_TEST_SLOT(34);
    BF2_GAME_LOGIC_TEST_SLOT(35); BF2_GAME_LOGIC_TEST_SLOT(36);
    BF2_GAME_LOGIC_TEST_SLOT(37); BF2_GAME_LOGIC_TEST_SLOT(38);
    BF2_GAME_LOGIC_TEST_SLOT(39); BF2_GAME_LOGIC_TEST_SLOT(40);
    BF2_GAME_LOGIC_TEST_SLOT(41); BF2_GAME_LOGIC_TEST_SLOT(42);
    BF2_GAME_LOGIC_TEST_SLOT(43); BF2_GAME_LOGIC_TEST_SLOT(44);
    BF2_GAME_LOGIC_TEST_SLOT(45); BF2_GAME_LOGIC_TEST_SLOT(46);
    BF2_GAME_LOGIC_TEST_SLOT(47); BF2_GAME_LOGIC_TEST_SLOT(48);
    BF2_GAME_LOGIC_TEST_SLOT(49); BF2_GAME_LOGIC_TEST_SLOT(50);
    BF2_GAME_LOGIC_TEST_SLOT(51); BF2_GAME_LOGIC_TEST_SLOT(52);
    BF2_GAME_LOGIC_TEST_SLOT(53); BF2_GAME_LOGIC_TEST_SLOT(54);
    BF2_GAME_LOGIC_TEST_SLOT(55); BF2_GAME_LOGIC_TEST_SLOT(56);
    BF2_GAME_LOGIC_TEST_SLOT(57); BF2_GAME_LOGIC_TEST_SLOT(58);
    BF2_GAME_LOGIC_TEST_SLOT(59); BF2_GAME_LOGIC_TEST_SLOT(60);
    BF2_GAME_LOGIC_TEST_SLOT(61); BF2_GAME_LOGIC_TEST_SLOT(62);
    BF2_GAME_LOGIC_TEST_SLOT(63); BF2_GAME_LOGIC_TEST_SLOT(64);
    BF2_GAME_LOGIC_TEST_SLOT(65); BF2_GAME_LOGIC_TEST_SLOT(66);
    BF2_GAME_LOGIC_TEST_SLOT(67); BF2_GAME_LOGIC_TEST_SLOT(68);
    BF2_GAME_LOGIC_TEST_SLOT(69); BF2_GAME_LOGIC_TEST_SLOT(70);
    BF2_GAME_LOGIC_TEST_SLOT(71); BF2_GAME_LOGIC_TEST_SLOT(72);
    BF2_GAME_LOGIC_TEST_SLOT(73); BF2_GAME_LOGIC_TEST_SLOT(74);
    BF2_GAME_LOGIC_TEST_SLOT(75); BF2_GAME_LOGIC_TEST_SLOT(76);
    BF2_GAME_LOGIC_TEST_SLOT(77); BF2_GAME_LOGIC_TEST_SLOT(78);
#undef BF2_GAME_LOGIC_TEST_SLOT
    virtual bool isServer()
    {
        if (m_events) m_events->push_back("gameLogic:isServer");
        ++m_serverCalls;
        return m_server;
    }

    std::vector<std::string>* m_events;
    bool m_server;
    int m_serverCalls;
    int m_teamCalls;
    std::vector<int> m_teams;
    std::string m_team1;
    std::string m_team2;
};

// PlayerManager::getLocalHumanPlayer is PE physical +0x30. SimpleObject::handleMessage discards
// the return value, so this fixture records only exact dispatch count.
class PlayerManagerPeAbiProbe
{
public:
    explicit PlayerManagerPeAbiProbe(std::vector<std::string>* events = 0)
        : m_calls(0), m_result(0), m_events(events) {}
#define BF2_PLAYER_MANAGER_TEST_SLOT(n) virtual void slot##n() {}
    BF2_PLAYER_MANAGER_TEST_SLOT(00); BF2_PLAYER_MANAGER_TEST_SLOT(01);
    BF2_PLAYER_MANAGER_TEST_SLOT(02); BF2_PLAYER_MANAGER_TEST_SLOT(03);
    BF2_PLAYER_MANAGER_TEST_SLOT(04); BF2_PLAYER_MANAGER_TEST_SLOT(05);
    BF2_PLAYER_MANAGER_TEST_SLOT(06); BF2_PLAYER_MANAGER_TEST_SLOT(07);
    BF2_PLAYER_MANAGER_TEST_SLOT(08); BF2_PLAYER_MANAGER_TEST_SLOT(09);
    BF2_PLAYER_MANAGER_TEST_SLOT(10); BF2_PLAYER_MANAGER_TEST_SLOT(11);
#undef BF2_PLAYER_MANAGER_TEST_SLOT
    virtual dice::hfe::world::IPlayer* getLocalHumanPlayer()
    {
        if (m_events) m_events->push_back("playerManager:getLocal");
        ++m_calls;
        return m_result;
    }

    int m_calls;
    dice::hfe::world::IPlayer* m_result;

private:
    std::vector<std::string>* m_events;
};

class ObjectComponentRuntimeProbe : public Object
{
public:
    explicit ObjectComponentRuntimeProbe(const IObjectTemplate* objectTemplate)
        : Object(objectTemplate) {}

    void* fastField(unsigned int iid) const
    {
        if (iid == dice::hfe::ai::IID_IAIObject) return m_aiObject;
        if (iid == dice::hfe::geom::IID_IGeometry) return m_geometry;
        if (iid == dice::hfe::world::IID_IArmorGLComp) return m_armorComponent;
        if (iid == dice::hfe::world::IID_ICollisionMesh) return m_collisionMesh;
        if (iid == dice::hfe::world::IID_IPhysicsNode) return m_physicsNode;
        if (iid == dice::hfe::world::IID_IResponsePhysics) return m_responsePhysics;
        if (iid == dice::hfe::io::IID_INetworkable) return m_networkable;
        if (iid == dice::hfe::world::IID_IPostProcessComp) return m_postProcessComponent;
        if (iid == dice::hfe::world::IID_IVehicleHudComp) return m_vehicleHudComponent;
        if (iid == dice::hfe::world::IID_IHelpHudComp) return m_helpHudComponent;
        if (iid == dice::hfe::world::IID_IWeaponHudComp) return m_weaponHudComponent;
        if (iid == dice::hfe::world::IID_IAbilityHudComp) return m_abilityHudComponent;
        if (iid == dice::hfe::world::IID_IAbilityGLComp) return m_abilityGLComponent;
        if (iid == dice::hfe::world::IID_IRadioGLComp) return m_radioGLComponent;
        if (iid == dice::hfe::world::IID_IWarningHudComp) return m_warningHudComponent;
        if (iid == dice::hfe::world::IID_IWindAffector) return m_windAffector;
        if (iid == dice::hfe::world::IID_IStrategicObjectComp)
            return m_strategicObjectComponent;
        return 0;
    }

    std::size_t baseComponentCount() const { return m_bases.size(); }
    std::size_t objectComponentCount() const { return m_components.size(); }
    bool hasBaseComponent(unsigned int id) const { return m_bases.find(id) != m_bases.end(); }
    bool hasObjectComponent(unsigned int id) const
    {
        return m_components.find(id) != m_components.end();
    }
    void clearBoundDirtyFlags() { m_flags &= ~0x20140000u; }
    unsigned int boundDirtyFlags() const { return m_flags & 0x20140000u; }
    IObject* cachedRootParentForTest() const { return m_cachedRootParent; }
    void setRawGeometryForTest(void* geometry) { m_geometry = geometry; }
    void setRawCollisionForTest(void* collision) { m_collisionMesh = collision; }
    void setRawPhysicsForTest(void* physics) { m_physicsNode = physics; }
    void setRawResponseForTest(void* response) { m_responsePhysics = response; }
    void setRawChildForTest(IObject* child) { m_child = child; }
    void setRawParentForTest(IObject* parent) { m_parent = parent; }
    void setRawSiblingForTest(IObject* sibling) { m_sibling = sibling; }
    void setRawNetworkableForTest(dice::hfe::io::INetworkable* networkable)
    {
        m_networkable = networkable;
    }
    unsigned int flagsForTest() const { return m_flags; }
    void setFlagsForTest(unsigned int flags) { m_flags = flags; }
    void clearFlagsForTest(unsigned int flags) { m_flags &= ~flags; }
    void markGeometryDirtyForTest() { m_flags |= 0x40000000u; }
    void clearGeometryDirtyForTest() { m_flags &= ~0x40000000u; }
    void setCachedRadiiForTest(float visual, float physic)
    {
        m_visualBoundingRadius = visual;
        m_physicBoundingRadius = physic;
    }
    float cachedVisualRadiusForTest() const { return m_visualBoundingRadius; }
    float cachedPhysicRadiusForTest() const { return m_physicBoundingRadius; }
    void setRadiusFlagsForTest(unsigned int set, unsigned int clear)
    {
        m_flags = (m_flags | set) & ~clear;
    }
};

class PhysicsSystemWindProbe : public dice::hfe::world::IPhysicsSystem
{
public:
    PhysicsSystemWindProbe() : m_refs(1), m_gravity(-14.73f), m_airHeight(1000.0f)
    {
        m_wind.x = m_wind.y = m_wind.z = 0.0f;
    }

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int iid) const
    {
        return iid == dice::hfe::world::IID_IPhysicsSystem
            ? const_cast<PhysicsSystemWindProbe*>(this) : 0;
    }
    virtual ~PhysicsSystemWindProbe() {}
    virtual void update(float) {}
    virtual void setGravity(float value) { m_gravity = value; }
    virtual float getGravity() { return m_gravity; }
    virtual void setWind(Vec3 value) { m_wind = value; }
    virtual Vec3 getWind() { return m_wind; }
    virtual void setAirDensityZeroAtHeight(float value) { m_airHeight = value; }
    virtual float getAirDensityZeroAtHeight() { return m_airHeight; }
    virtual std::string listProperties() { return std::string(); }

private:
    int m_refs;
    float m_gravity;
    Vec3 m_wind;
    float m_airHeight;
};

class PhysicsNodeDragProbe : public dice::hfe::world::PhysicsNode
{
public:
    PhysicsNodeDragProbe() : m_transformation(Mat4::getIdentity())
    {
        m_position.x = m_position.y = m_position.z = 0.0f;
    }

    virtual const Mat4& getAbsoluteTransformation() const { return m_transformation; }
    virtual const Vec3& getAbsolutePosition() const { return m_position; }

    void configure(float drag, float mass, const Vec3& positionalSpeed,
                   const Vec3& rotationalSpeed)
    {
        m_drag = drag;
        m_mass = mass;
        m_positionalSpeed = positionalSpeed;
        m_rotationalSpeed = rotationalSpeed;
        m_positionalAcceleration = Vec3::getZero();
        m_rotationalAcceleration = Vec3::getZero();
    }

    const Vec3& positionalAccelerationForTest() const { return m_positionalAcceleration; }
    const Vec3& rotationalAccelerationForTest() const { return m_rotationalAcceleration; }
    void setPositionForTest(const Vec3& position) { m_position = position; }

private:
    Mat4 m_transformation;
    Vec3 m_position;
};

class SoldierPhysicsNodeDragProbe : public dice::hfe::world::SoldierPhysicsNode
{
public:
    SoldierPhysicsNodeDragProbe() : m_transformation(Mat4::getIdentity()) {}

    virtual const Mat4& getAbsoluteTransformation() const { return m_transformation; }

    void configure(float drag, float mass, const Vec3& positionalSpeed)
    {
        m_drag = drag;
        m_mass = mass;
        m_positionalSpeed = positionalSpeed;
        m_positionalAcceleration = Vec3::getZero();
    }

    const Vec3& positionalAccelerationForTest() const { return m_positionalAcceleration; }

private:
    Mat4 m_transformation;
};

class RotationalPointPhysicsNodeProbe : public dice::hfe::world::RotationalPointPhysicsNode
{
public:
    RotationalPointPhysicsNodeProbe()
        : m_transformation(Mat4::getIdentity()), m_transformSetCalls(0) {}

    virtual const Mat4& getAbsoluteTransformation() const { return m_transformation; }
    virtual void setAbsoluteTransformation(const Mat4& value)
    {
        m_transformation = value;
        ++m_transformSetCalls;
    }

    void configure(float mass, const Vec3& speed, const Vec3& acceleration,
                   const Vec3& friction)
    {
        m_mass = mass;
        m_rotationalSpeed = speed;
        m_rotationalAcceleration = acceleration;
        m_rotationalFriction = friction;
        m_angularIntegrationVelocity = Vec3::getZero();
        m_dwordD0 = 17;
        m_transformSetCalls = 0;
        m_transformation = Mat4::getIdentity();
    }

    const Vec3& rotationalAccelerationForTest() const { return m_rotationalAcceleration; }
    const Vec3& rotationalFrictionForTest() const { return m_rotationalFriction; }
    const Vec3& angularIntegrationVelocityForTest() const
    {
        return m_angularIntegrationVelocity;
    }
    int trailingStateForTest() const { return m_dwordD0; }

    int m_transformSetCalls;

private:
    Mat4 m_transformation;
};

class RotationalPointResponseProbe : public dice::hfe::world::RotationalPointResponsePhysics
{
public:
    float contactDepthSqForTest() const { return m_contactDepthSq; }
    IObject* contactObjectForTest() const { return m_contactObject; }
    const Vec3& contactPointForTest() const { return m_contactPoint; }
    const Vec3& contactPositionForTest() const { return m_contactPosition; }
    const Vec3& contactSurfaceSpeedForTest() const { return m_contactSurfaceSpeed; }
    const Vec3& contactSpeedForTest() const { return m_contactSpeed; }
    float contactScaleForTest() const { return m_contactScale; }
    float contactFractionForTest() const { return m_contactFraction; }
    const Vec3& contactNormalForTest() const { return m_contactNormal; }
    int contactOwnMaterialForTest() const { return m_contactOwnMaterial; }
    int contactMaterialForTest() const { return m_contactMaterial; }
};

class PointResponseProbe : public dice::hfe::world::PointResponsePhysics
{
public:
    IObject* contactObject() const { return m_contactObject; }
    const Vec3& contactPoint() const { return m_contactPoint; }
    const Vec3& contactSpeed() const { return m_contactSpeed; }
    float contactFraction() const { return m_contactFraction; }
    int contactMaterial() const { return m_contactMaterial; }
    bool hasFaceInfo() const { return m_hasFaceInfo; }
};

// console-script fixtures
// Object::makeScript writes once and then hands the stream to its geometry. These three fixtures
// exist to prove that order, the discarded return values, and that the geometry facet dispatched
// is the one queryInterface returned rather than the stored m_geometry pointer.

class ScriptStreamProbe : public dice::hfe::io::INonlinearStream
{
public:
    explicit ScriptStreamProbe(std::vector<std::string>* events)
        : m_events(events), m_refs(1), m_writeCalls(0), m_shortWrite(false) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }

    virtual unsigned int read(void*, unsigned int) { return 0; }
    virtual unsigned int write(const void* buffer, unsigned int size)
    {
        if (m_events) m_events->push_back("stream:write");
        ++m_writeCalls;
        m_text.append(static_cast<const char*>(buffer), size);
        // A short count models the failure ObjectTemplate propagates and Object ignores.
        return m_shortWrite ? (size == 0u ? 0u : size - 1u) : size;
    }
    virtual int skip(unsigned int) { return 0; }
    virtual int getAvailableBytes() { return 0; }
    virtual dice::hfe::io::IStream* clone() { return 0; }
    virtual bool canRead() { return true; }
    virtual bool canWrite() { return true; }
    virtual void flushWrite() {}
    virtual int seek(dice::hfe::io::SeekOrigin, int) { return 0; }
    virtual int getPosition() { return 0; }
    virtual int getSize() { return static_cast<int>(m_text.size()); }

    const std::string& text() const { return m_text; }
    int writeCalls() const { return m_writeCalls; }
    void reportShortWrites(bool value) { m_shortWrite = value; }

private:
    std::vector<std::string>* m_events;
    std::string m_text;
    int m_refs;
    int m_writeCalls;
    bool m_shortWrite;
};

// The geometry's console facet. A separate object from the geometry itself, so a test can tell the
// queried interface pointer apart from the one Object stores.
class ScriptSaveableProbe : public dice::hfe::io::IConsoleSaveable
{
public:
    explicit ScriptSaveableProbe(std::vector<std::string>* events)
        : m_events(events), m_refs(1), m_makeScriptCalls(0), m_lastStream(0), m_result(true) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }

    virtual bool makeScript(dice::hfe::io::IStream* stream)
    {
        if (m_events) m_events->push_back("geometry:makeScript");
        ++m_makeScriptCalls;
        m_lastStream = stream;
        return m_result;
    }

    int m_makeScriptCalls;
    dice::hfe::io::IStream* m_lastStream;
    bool m_result;

private:
    std::vector<std::string>* m_events;
    int m_refs;
};

class ScriptGeometryProbe : public dice::hfe::IBase
{
public:
    ScriptGeometryProbe(ScriptSaveableProbe* saveable, std::vector<std::string>* events)
        : m_saveable(saveable), m_events(events), m_refs(1), m_releaseCalls(0),
          m_queryCalls(0), m_lastIid(0) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { ++m_releaseCalls; return --m_refs; }
    virtual void* queryInterface(unsigned int iid) const
    {
        if (m_events) m_events->push_back("geometry:query");
        ++m_queryCalls;
        m_lastIid = iid;
        if (iid == dice::hfe::io::IID_IConsoleSaveable && m_saveable)
            return static_cast<dice::hfe::io::IConsoleSaveable*>(m_saveable);
        return 0;
    }

    int m_releaseCalls;
    mutable int m_queryCalls;
    mutable unsigned int m_lastIid;

private:
    ScriptSaveableProbe* m_saveable;
    std::vector<std::string>* m_events;
    int m_refs;
};

class ScriptObjectProbe : public ObjectComponentRuntimeProbe
{
public:
    ScriptObjectProbe(const IObjectTemplate* objectTemplate, std::vector<std::string>* events)
        : ObjectComponentRuntimeProbe(objectTemplate), m_events(events), m_nameCalls(0),
          m_positionCalls(0), m_transformCalls(0) {}

    virtual const std::string& getName() const
    {
        if (m_events) m_events->push_back("object:getName");
        ++m_nameCalls;
        return Object::getName();
    }

    virtual const Vec3& getAbsolutePosition() const
    {
        ++m_positionCalls;
        return Object::getAbsolutePosition();
    }

    virtual const Mat4& getAbsoluteTransformation() const
    {
        ++m_transformCalls;
        return Object::getAbsoluteTransformation();
    }

    void resetScriptCalls() { m_nameCalls = 0; m_positionCalls = 0; m_transformCalls = 0; }

    mutable int m_nameCalls;
    mutable int m_positionCalls;
    mutable int m_transformCalls;

private:
    std::vector<std::string>* m_events;
};

class RadiusObjectProbe : public ObjectComponentRuntimeProbe
{
public:
    RadiusObjectProbe(const IObjectTemplate* objectTemplate, const char* label,
                      std::vector<std::string>* events)
        : ObjectComponentRuntimeProbe(objectTemplate), m_label(label), m_events(events),
          m_classId(dice::hfe::world::CID_Object), m_modifier(0.0f),
          m_classIdCalls(0), m_visualCalls(0), m_physicCalls(0), m_modifierCalls(0) {}

    virtual unsigned int getClassID() const
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":class");
        ++m_classIdCalls;
        return m_classId;
    }

    virtual float getVisualBoundingRadius() const
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":visual");
        ++m_visualCalls;
        return Object::getVisualBoundingRadius();
    }

    virtual float getPhysicBoundingRadius() const
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":physic");
        ++m_physicCalls;
        return Object::getPhysicBoundingRadius();
    }

    virtual float getBoundingRadiusModifier() const
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":modifier");
        ++m_modifierCalls;
        return m_modifier;
    }

    void resetRadiusCalls()
    {
        m_classIdCalls = 0;
        m_visualCalls = 0;
        m_physicCalls = 0;
        m_modifierCalls = 0;
    }

    const char* m_label;
    std::vector<std::string>* m_events;
    unsigned int m_classId;
    float m_modifier;
    mutable int m_classIdCalls;
    mutable int m_visualCalls;
    mutable int m_physicCalls;
    mutable int m_modifierCalls;
};

class BoundingGetterProbe : public ObjectComponentRuntimeProbe
{
public:
    BoundingGetterProbe(const IObjectTemplate* objectTemplate, const char* label,
                        std::vector<std::string>* events)
        : ObjectComponentRuntimeProbe(objectTemplate), m_label(label),
          m_events(events), m_fullGetterCalls(0) {}

    virtual LocalBoundingBox* getFullBoundingBox()
    {
        ++m_fullGetterCalls;
        if (m_events)
            m_events->push_back(std::string(m_label) + ":full");
        return Object::getFullBoundingBox();
    }

    int m_fullGetterCalls;

private:
    const char* m_label;
    std::vector<std::string>* m_events;
};

class RuntimeComponentProbe : public dice::hfe::world::IObjectComponent
{
public:
    RuntimeComponentProbe(const char* label, unsigned int classId,
                          std::vector<std::string>* events)
        : m_label(label), m_classId(classId), m_events(events), m_refs(2), m_object(0),
          m_destroyCalls(0), m_updateCalls(0), m_messageCalls(0),
          m_lastDeltaTime(0.0f), m_clearFlagsOnUpdate(0),
          m_lastMessage(static_cast<dice::hfe::world::TemplateMessage>(0)),
          m_lastPlayer(0), m_lastMessageFlag(false) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int iid) const
    {
        if (iid == dice::hfe::IID_IBase ||
            iid == dice::hfe::world::IID_IObjectComponent || iid == m_classId)
            return const_cast<RuntimeComponentProbe*>(this);
        return 0;
    }
    virtual unsigned int getInterfaceID() const { return m_classId; }
    virtual void init(IObject*) {}
    virtual void destroy()
    {
        if (m_events) m_events->push_back(std::string("destroy:") + m_label);
        ++m_destroyCalls;
    }
    virtual void setObject(IObject* object) { m_object = object; }
    virtual IObject* getObject() const { return m_object; }
    virtual bool hasUpdate() const { return false; }
    virtual void handleUpdate(float deltaTime)
    {
        if (m_events) m_events->push_back(std::string("update:") + m_label);
        ++m_updateCalls;
        m_lastDeltaTime = deltaTime;
        if (m_object && m_clearFlagsOnUpdate)
            m_object->updateFlags(0, m_clearFlagsOnUpdate);
    }
    virtual bool hasFrameUpdate() const { return false; }
    virtual void handleFrameUpdate(float) {}
    virtual void handleMessage(dice::hfe::world::TemplateMessage message,
                               dice::hfe::world::IPlayer* player, bool flag)
    {
        if (m_events) m_events->push_back(std::string("message:") + m_label);
        ++m_messageCalls;
        m_lastMessage = message;
        m_lastPlayer = player;
        m_lastMessageFlag = flag;
    }

    const char* m_label;
    IObject* m_object;
    int m_destroyCalls;
    int m_updateCalls;
    int m_messageCalls;
    float m_lastDeltaTime;
    unsigned int m_clearFlagsOnUpdate;
    dice::hfe::world::TemplateMessage m_lastMessage;
    dice::hfe::world::IPlayer* m_lastPlayer;
    bool m_lastMessageFlag;

private:
    unsigned int m_classId;
    std::vector<std::string>* m_events;
    int m_refs;
};

class CacheLifecycleComponentProbe : public RuntimeComponentProbe
{
public:
    CacheLifecycleComponentProbe(const char* label, unsigned int classId,
                                 bool hasUpdate, std::vector<std::string>* events)
        : RuntimeComponentProbe(label, classId, events), m_events(events),
          m_hasUpdate(hasUpdate), m_initCalls(0), m_hasUpdateCalls(0) {}

    virtual void init(IObject* object)
    {
        if (m_events) m_events->push_back(std::string("component:init:") + m_label);
        ++m_initCalls;
        m_object = object;
    }

    virtual void destroy()
    {
        if (m_events) m_events->push_back(std::string("component:destroy:") + m_label);
        ++m_destroyCalls;
    }

    virtual bool hasUpdate() const
    {
        if (m_events) m_events->push_back(std::string("component:hasUpdate:") + m_label);
        ++m_hasUpdateCalls;
        return m_hasUpdate;
    }

    int m_initCalls;
    mutable int m_hasUpdateCalls;

private:
    std::vector<std::string>* m_events;
    bool m_hasUpdate;
};

class CacheLifecycleObjectProbe : public ObjectComponentRuntimeProbe
{
public:
    CacheLifecycleObjectProbe(const IObjectTemplate* objectTemplate, const char* label,
                              std::vector<std::string>* events)
        : ObjectComponentRuntimeProbe(objectTemplate), m_label(label), m_events(events),
          m_networkInitCalls(0), m_disableDispatches(0), m_enableDispatches(0),
          m_setComponentCalls(0), m_activeLodCalls(0), m_absoluteSetCalls(0),
          m_updateGeometryCalls(0), m_getChildCalls(0), m_getCacheIdCalls(0),
          m_getNetworkableCalls(0) {}

    virtual unsigned int getObjectCacheID() const
    {
        trace("getCacheId");
        ++m_getCacheIdCalls;
        return IObject::getObjectCacheID();
    }

    virtual void disableCachedObject()
    {
        trace("disableCached");
        ++m_disableDispatches;
    }

    virtual void enableCachedObject()
    {
        trace("enableCached");
        ++m_enableDispatches;
    }

    virtual bool setComponent(unsigned int classId, dice::hfe::IBase* component)
    {
        trace("setComponent");
        ++m_setComponentCalls;
        m_removedComponentIds.push_back(classId);
        return Object::setComponent(classId, component);
    }

    virtual void updateFlags(unsigned int set, unsigned int clear)
    {
        trace("updateFlags");
        m_flagCalls.push_back(std::make_pair(set, clear));
        Object::updateFlags(set, clear);
    }

    virtual void SetActiveLod(dice::hfe::world::ObjectLodType)
    {
        trace("setActiveLod");
        ++m_activeLodCalls;
    }

    virtual dice::hfe::io::INetworkable* getNetworkable() const
    {
        trace("getNetworkable");
        ++m_getNetworkableCalls;
        return m_networkable;
    }

    virtual void setAbsoluteTransformation(const Mat4& transformation)
    {
        trace("setAbsolute");
        ++m_absoluteSetCalls;
        m_absoluteTransformation = transformation;
    }

    virtual void updateGeometry()
    {
        trace("updateGeometry");
        ++m_updateGeometryCalls;
    }

    virtual IObject* getChild() const
    {
        trace("getChild");
        ++m_getChildCalls;
        return m_child;
    }

    virtual void initNetworkableComp()
    {
        trace("initNetworkable");
        ++m_networkInitCalls;
    }

    void addComponentForTest(unsigned int classId,
                             dice::hfe::world::IObjectComponent* component)
    {
        m_components[classId] =
            dice::hfe::SmartPtr<dice::hfe::world::IObjectComponent>(component);
    }

    void addNullComponentForTest(unsigned int classId)
    {
        m_components[classId] =
            dice::hfe::SmartPtr<dice::hfe::world::IObjectComponent>();
    }

    void setCacheStateForTest(unsigned int objectCacheId, unsigned int cacheFlags,
                              unsigned int liveFlags)
    {
        m_objectCacheId = objectCacheId;
        m_cacheFlags = cacheFlags;
        m_flags = liveFlags;
        m_cachedRootParent = 0;
    }

    void setChildForCacheTest(IObject* child) { m_child = child; }
    void setSiblingForCacheTest(IObject* sibling) { m_sibling = sibling; }
    void setParentForCacheTest(IObject* parent) { m_parent = parent; }
    void setRawPhysicsForCacheTest(void* physics) { m_physicsNode = physics; }
    void setRawResponseForCacheTest(void* response) { m_responsePhysics = response; }
    void setRawCollisionForCacheTest(void* collision) { m_collisionMesh = collision; }
    void setRawNetworkableForCacheTest(dice::hfe::io::INetworkable* networkable)
    {
        m_networkable = networkable;
    }
    void setInterpolationForCacheTest(Mat4* interpolation)
    {
        m_interpolationTransformation = interpolation;
    }
    Mat4* interpolationForCacheTest() const { return m_interpolationTransformation; }
    void setDestroyedForCacheTest(bool destroyed) { m_isObjectDestroyed = destroyed; }
    std::size_t componentCountForCacheTest() const { return m_components.size(); }
    unsigned int cacheFlagsForTest() const { return m_cacheFlags; }

    const char* m_label;
    int m_networkInitCalls;
    int m_disableDispatches;
    int m_enableDispatches;
    int m_setComponentCalls;
    int m_activeLodCalls;
    int m_absoluteSetCalls;
    int m_updateGeometryCalls;
    mutable int m_getChildCalls;
    mutable int m_getCacheIdCalls;
    mutable int m_getNetworkableCalls;
    std::vector<std::pair<unsigned int, unsigned int> > m_flagCalls;
    std::vector<unsigned int> m_removedComponentIds;

private:
    void trace(const char* operation) const
    {
        if (m_events)
            m_events->push_back(std::string(m_label) + ":" + operation);
    }

    std::vector<std::string>* m_events;
};

class ArmorPeAbiProbe : public RuntimeComponentProbe
{
public:
    ArmorPeAbiProbe(float hitPoints, std::vector<std::string>* events)
        : RuntimeComponentProbe("armor", dice::hfe::world::IID_IArmorGLComp, events),
          m_events(events), m_hitPoints(hitPoints), m_setCalls(0), m_getCalls(0),
          m_sendingCalls(0), m_sending(false), m_collideCalls(0), m_lastCollideFirst(0),
          m_lastCollideMaterial(0), m_lastCollideOther(0), m_collides(false),
          m_heightCalls(0), m_lastHeight(0.0f) {}

    virtual void setHitPoints(float hitPoints)
    {
        ++m_setCalls;
        m_hitPoints = hitPoints;
    }
    virtual float getHitPoints()
    {
        if (m_events) m_events->push_back("armor:getHitPoints");
        ++m_getCalls;
        return m_hitPoints;
    }

#define BF2_ARMOR_TEST_SLOT(n) virtual void slot##n() {}
    BF2_ARMOR_TEST_SLOT(40); BF2_ARMOR_TEST_SLOT(44);
    BF2_ARMOR_TEST_SLOT(48); BF2_ARMOR_TEST_SLOT(4C);
#undef BF2_ARMOR_TEST_SLOT


    // Physical +0x50 on both ports - the gate Bundle::handleMessage consults before launching.
    virtual bool isSendingMessageAtPhysical50()
    {
        if (m_events) m_events->push_back("armor:isSendingMessage");
        ++m_sendingCalls;
        return m_sending;
    }
#define BF2_ARMOR_COLLIDE_GAP(n) virtual void collideGap##n() {}
    BF2_ARMOR_COLLIDE_GAP(54); BF2_ARMOR_COLLIDE_GAP(58); BF2_ARMOR_COLLIDE_GAP(5C);
#undef BF2_ARMOR_COLLIDE_GAP

    // Physical +0x60 (ELF +0x64) - the armor's own collision predicate.
    virtual bool handleCollisionAtPhysical60(int first, int materialId, IObject* other)
    {
        ++m_collideCalls;
        m_lastCollideFirst = first;
        m_lastCollideMaterial = materialId;
        m_lastCollideOther = other;
        return m_collides;
    }

    // Physical +0xA0 on both ports - the last-collision height.
#define BF2_ARMOR_HEIGHT_GAP(n) virtual void heightGap##n() {}
    BF2_ARMOR_HEIGHT_GAP(64); BF2_ARMOR_HEIGHT_GAP(68); BF2_ARMOR_HEIGHT_GAP(6C);
    BF2_ARMOR_HEIGHT_GAP(70); BF2_ARMOR_HEIGHT_GAP(74); BF2_ARMOR_HEIGHT_GAP(78);
    BF2_ARMOR_HEIGHT_GAP(7C); BF2_ARMOR_HEIGHT_GAP(80); BF2_ARMOR_HEIGHT_GAP(84);
    BF2_ARMOR_HEIGHT_GAP(88); BF2_ARMOR_HEIGHT_GAP(8C); BF2_ARMOR_HEIGHT_GAP(90);
    BF2_ARMOR_HEIGHT_GAP(94); BF2_ARMOR_HEIGHT_GAP(98); BF2_ARMOR_HEIGHT_GAP(9C);
#undef BF2_ARMOR_HEIGHT_GAP
    virtual void setLastCollisionHeightAtPhysicalA0(float height)
    {
        ++m_heightCalls;
        m_lastHeight = height;
    }

    float m_hitPoints;
    int m_setCalls;
    int m_getCalls;
    int m_sendingCalls;
    bool m_sending;
    int m_collideCalls;
    int m_lastCollideFirst;
    int m_lastCollideMaterial;
    IObject* m_lastCollideOther;
    bool m_collides;
    int m_heightCalls;
    float m_lastHeight;

private:
    std::vector<std::string>* m_events;
};

// Raw PE physics-node table through the isSleeping operation at physical +0x118 (slot 70).
class PhysicsNodeSleepingPeAbiProbe
{
public:
    PhysicsNodeSleepingPeAbiProbe(const char* label, bool sleeping,
                                  std::vector<std::string>* events)
        : m_label(label), m_sleeping(sleeping), m_mobile(false), m_calls(0),
          m_mobileCalls(0), m_latencyCalls(0), m_resetCalls(0),
          m_setEnabledCalls(0), m_setParentCalls(0), m_lastReset(false),
          m_lastEnabled(false), m_lastParent(0), m_swapOwner(0),
          m_swapPhysics(0), m_swapResponse(0), m_positionalSpeedCalls(0),
          m_rotationalSpeedCalls(0), m_events(events)
    {
        m_lastPositionalSpeed.x = m_lastPositionalSpeed.y = m_lastPositionalSpeed.z = 0.0f;
        m_lastRotationalSpeed.x = m_lastRotationalSpeed.y = m_lastRotationalSpeed.z = 0.0f;
    }

    int m_positionalSpeedCalls;
    int m_rotationalSpeedCalls;
    Vec3 m_lastPositionalSpeed;
    Vec3 m_lastRotationalSpeed;

#define BF2_PHYSICS_SLEEP_TEST_SLOT(n) virtual void slot##n() {}
    BF2_PHYSICS_SLEEP_TEST_SLOT(00); BF2_PHYSICS_SLEEP_TEST_SLOT(01);
    BF2_PHYSICS_SLEEP_TEST_SLOT(02); BF2_PHYSICS_SLEEP_TEST_SLOT(03);
    BF2_PHYSICS_SLEEP_TEST_SLOT(04);
#undef BF2_PHYSICS_SLEEP_TEST_SLOT
    virtual void setEnabledForCache(bool enabled)
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":setEnabled");
        ++m_setEnabledCalls;
        m_lastEnabled = enabled;
    }
#define BF2_PHYSICS_SLEEP_TEST_SLOT(n) virtual void slot##n() {}
    BF2_PHYSICS_SLEEP_TEST_SLOT(06); BF2_PHYSICS_SLEEP_TEST_SLOT(07);
    BF2_PHYSICS_SLEEP_TEST_SLOT(08); BF2_PHYSICS_SLEEP_TEST_SLOT(09);
    BF2_PHYSICS_SLEEP_TEST_SLOT(10);
#undef BF2_PHYSICS_SLEEP_TEST_SLOT
    virtual void setParentForHierarchy(void* parent)
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":setParent");
        ++m_setParentCalls;
        m_lastParent = parent;
        if (m_swapOwner)
        {
            m_swapOwner->setRawPhysicsForTest(m_swapPhysics);
            m_swapOwner->setRawResponseForTest(m_swapResponse);
        }
    }
#define BF2_PHYSICS_SLEEP_TEST_SLOT(n) virtual void slot##n() {}
    BF2_PHYSICS_SLEEP_TEST_SLOT(12); BF2_PHYSICS_SLEEP_TEST_SLOT(13);
    BF2_PHYSICS_SLEEP_TEST_SLOT(14); BF2_PHYSICS_SLEEP_TEST_SLOT(15);
    BF2_PHYSICS_SLEEP_TEST_SLOT(16); BF2_PHYSICS_SLEEP_TEST_SLOT(17);
    BF2_PHYSICS_SLEEP_TEST_SLOT(18); BF2_PHYSICS_SLEEP_TEST_SLOT(19);
    BF2_PHYSICS_SLEEP_TEST_SLOT(20); BF2_PHYSICS_SLEEP_TEST_SLOT(21);
    BF2_PHYSICS_SLEEP_TEST_SLOT(22); BF2_PHYSICS_SLEEP_TEST_SLOT(23);
    BF2_PHYSICS_SLEEP_TEST_SLOT(24);
#undef BF2_PHYSICS_SLEEP_TEST_SLOT
    // Physical +0x64 / +0x68 - the launch pair Bundle::handleMessage drives.
    virtual void setPositionalSpeedAtPhysical64(const Vec3& speed)
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":setPositionalSpeed");
        ++m_positionalSpeedCalls;
        m_lastPositionalSpeed = speed;
    }
    virtual void setRotationalSpeedAtPhysical68(const Vec3& speed)
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":setRotationalSpeed");
        ++m_rotationalSpeedCalls;
        m_lastRotationalSpeed = speed;
    }
#define BF2_PHYSICS_SLEEP_TEST_SLOT(n) virtual void slot##n() {}
    BF2_PHYSICS_SLEEP_TEST_SLOT(27);
    BF2_PHYSICS_SLEEP_TEST_SLOT(28); BF2_PHYSICS_SLEEP_TEST_SLOT(29);
    BF2_PHYSICS_SLEEP_TEST_SLOT(30); BF2_PHYSICS_SLEEP_TEST_SLOT(31);
    BF2_PHYSICS_SLEEP_TEST_SLOT(32); BF2_PHYSICS_SLEEP_TEST_SLOT(33);
    BF2_PHYSICS_SLEEP_TEST_SLOT(34); BF2_PHYSICS_SLEEP_TEST_SLOT(35);
    BF2_PHYSICS_SLEEP_TEST_SLOT(36); BF2_PHYSICS_SLEEP_TEST_SLOT(37);
    BF2_PHYSICS_SLEEP_TEST_SLOT(38); BF2_PHYSICS_SLEEP_TEST_SLOT(39);
    BF2_PHYSICS_SLEEP_TEST_SLOT(40); BF2_PHYSICS_SLEEP_TEST_SLOT(41);
    BF2_PHYSICS_SLEEP_TEST_SLOT(42); BF2_PHYSICS_SLEEP_TEST_SLOT(43);
    BF2_PHYSICS_SLEEP_TEST_SLOT(44); BF2_PHYSICS_SLEEP_TEST_SLOT(45);
    BF2_PHYSICS_SLEEP_TEST_SLOT(46); BF2_PHYSICS_SLEEP_TEST_SLOT(47);
    BF2_PHYSICS_SLEEP_TEST_SLOT(48); BF2_PHYSICS_SLEEP_TEST_SLOT(49);
    BF2_PHYSICS_SLEEP_TEST_SLOT(50); BF2_PHYSICS_SLEEP_TEST_SLOT(51);
    BF2_PHYSICS_SLEEP_TEST_SLOT(52); BF2_PHYSICS_SLEEP_TEST_SLOT(53);
    BF2_PHYSICS_SLEEP_TEST_SLOT(54); BF2_PHYSICS_SLEEP_TEST_SLOT(55);
    BF2_PHYSICS_SLEEP_TEST_SLOT(56); BF2_PHYSICS_SLEEP_TEST_SLOT(57);
    BF2_PHYSICS_SLEEP_TEST_SLOT(58); BF2_PHYSICS_SLEEP_TEST_SLOT(59);
    BF2_PHYSICS_SLEEP_TEST_SLOT(60); BF2_PHYSICS_SLEEP_TEST_SLOT(61);
    BF2_PHYSICS_SLEEP_TEST_SLOT(62); BF2_PHYSICS_SLEEP_TEST_SLOT(63);
    BF2_PHYSICS_SLEEP_TEST_SLOT(64); BF2_PHYSICS_SLEEP_TEST_SLOT(65);
    BF2_PHYSICS_SLEEP_TEST_SLOT(66); BF2_PHYSICS_SLEEP_TEST_SLOT(67);
    BF2_PHYSICS_SLEEP_TEST_SLOT(68);
#undef BF2_PHYSICS_SLEEP_TEST_SLOT
    virtual void resetForCache(bool reset)
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":reset");
        ++m_resetCalls;
        m_lastReset = reset;
    }
    virtual bool isSleeping()
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":isSleeping");
        ++m_calls;
        return m_sleeping;
    }
#define BF2_PHYSICS_CACHE_TEST_SLOT(n) virtual void cacheSlot##n() {}
    BF2_PHYSICS_CACHE_TEST_SLOT(71); BF2_PHYSICS_CACHE_TEST_SLOT(72);
    BF2_PHYSICS_CACHE_TEST_SLOT(73); BF2_PHYSICS_CACHE_TEST_SLOT(74);
    BF2_PHYSICS_CACHE_TEST_SLOT(75);
#undef BF2_PHYSICS_CACHE_TEST_SLOT
    virtual bool isMobileForCache()
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":isMobile");
        ++m_mobileCalls;
        return m_mobile;
    }
    virtual void cacheSlot77() {}
    virtual void initLatencyCompensationForCache()
    {
        if (m_events) m_events->push_back(std::string(m_label) + ":initLatency");
        ++m_latencyCalls;
    }

    const char* m_label;
    bool m_sleeping;
    bool m_mobile;
    int m_calls;
    int m_mobileCalls;
    int m_latencyCalls;
    int m_resetCalls;
    int m_setEnabledCalls;
    int m_setParentCalls;
    bool m_lastReset;
    bool m_lastEnabled;
    void* m_lastParent;
    ObjectComponentRuntimeProbe* m_swapOwner;
    void* m_swapPhysics;
    void* m_swapResponse;

private:
    std::vector<std::string>* m_events;
};

class HierarchyObjectProbe : public ObjectComponentRuntimeProbe
{
public:
    HierarchyObjectProbe(const IObjectTemplate* objectTemplate, const char* label,
                         std::vector<std::string>* events)
        : ObjectComponentRuntimeProbe(objectTemplate), m_label(label), m_events(events),
          m_setParentCalls(0), m_addSiblingCalls(0), m_removeSiblingCalls(0),
          m_relativeCalls(0), m_lastRelative(Mat4::getIdentity()),
          m_reloadParent(0), m_reloadedHead(0) {}

    virtual void setParent(IObject* parent)
    {
        trace("setParent");
        ++m_setParentCalls;
        Object::setParent(parent);
        if (!parent && m_reloadParent)
            m_reloadParent->setRawChildForTest(m_reloadedHead);
    }

    virtual void addSibling(IObject* object)
    {
        trace("addSibling");
        ++m_addSiblingCalls;
        Object::addSibling(object);
    }

    virtual void removeSibling(IObject* object)
    {
        trace("removeSibling");
        ++m_removeSiblingCalls;
        Object::removeSibling(object);
    }

    virtual void updateFlags(unsigned int set, unsigned int clear)
    {
        trace("updateFlags");
        m_flagCalls.push_back(std::make_pair(set, clear));
        m_flags = (m_flags | set) & ~clear;
    }

    virtual void setRelativeTransformation(const Mat4& transformation)
    {
        trace("setRelative");
        ++m_relativeCalls;
        m_lastRelative = transformation;
        Object::setRelativeTransformation(transformation);
    }

    void setReloadHeadOnDetach(ObjectComponentRuntimeProbe* parent, IObject* head)
    {
        m_reloadParent = parent;
        m_reloadedHead = head;
    }

    void setDestroyedForStructuralTest(bool destroyed)
    {
        m_isObjectDestroyed = destroyed;
    }

    const char* m_label;
    int m_setParentCalls;
    int m_addSiblingCalls;
    int m_removeSiblingCalls;
    int m_relativeCalls;
    Mat4 m_lastRelative;
    std::vector<std::pair<unsigned int, unsigned int> > m_flagCalls;

private:
    void trace(const char* operation)
    {
        if (m_events)
            m_events->push_back(std::string(m_label) + ":" + operation);
    }

    std::vector<std::string>* m_events;
    ObjectComponentRuntimeProbe* m_reloadParent;
    IObject* m_reloadedHead;
};

class UpdateFlagsSpyObject : public ObjectComponentRuntimeProbe
{
public:
    UpdateFlagsSpyObject(const IObjectTemplate* objectTemplate, const char* label,
                         std::vector<std::string>* events)
        : ObjectComponentRuntimeProbe(objectTemplate), m_label(label), m_events(events),
          m_calls(0), m_lastSet(0), m_lastClear(0) {}

    virtual void updateFlags(unsigned int set, unsigned int clear)
    {
        if (m_events) m_events->push_back(std::string("flags:") + m_label);
        ++m_calls;
        m_lastSet = set;
        m_lastClear = clear;
    }

    void resetCalls()
    {
        m_calls = 0;
        m_lastSet = 0;
        m_lastClear = 0;
    }

    const char* m_label;
    int m_calls;
    unsigned int m_lastSet;
    unsigned int m_lastClear;

private:
    std::vector<std::string>* m_events;
};

class DestroyTrackingObject : public ObjectComponentRuntimeProbe
{
public:
    DestroyTrackingObject(const IObjectTemplate* objectTemplate, const char* label,
                          std::vector<std::string>* events)
        : ObjectComponentRuntimeProbe(objectTemplate), m_label(label), m_events(events),
          m_destroyDispatches(0) {}

    virtual void destroy(bool force)
    {
        if (m_events)
            m_events->push_back(std::string("objectDestroy:") + m_label +
                                (force ? ":force" : ":normal"));
        ++m_destroyDispatches;
        Object::destroy(force);
    }

    const char* m_label;
    int m_destroyDispatches;

private:
    std::vector<std::string>* m_events;
};

class SimpleUpdateObjectProbe : public SimpleObject
{
public:
    explicit SimpleUpdateObjectProbe(const SimpleObjectTemplate* objectTemplate)
        : SimpleObject(objectTemplate) {}

    void setFlagsForTest(unsigned int flags) { m_flags = flags; }
    unsigned int flagsForTest() const { return m_flags; }
    void setRawPhysicsForTest(void* physicsNode) { m_physicsNode = physicsNode; }
};

class SimpleDisableObjectProbe : public SimpleObject
{
public:
    SimpleDisableObjectProbe(const SimpleObjectTemplate* objectTemplate,
                             std::vector<std::string>* events)
        : SimpleObject(objectTemplate), m_events(events), m_portalResult(false),
          m_sectorResult(false), m_setComponentResult(true), m_setComponentCalls(0),
          m_getCacheIdCalls(0), m_activeLodCalls(0), m_absoluteCalls(0),
          m_geometryCalls(0), m_lastComponentId(0), m_lastComponent(0) {}

    virtual bool setComponent(unsigned int classId, dice::hfe::IBase* component)
    {
        trace("simple:setComponent");
        ++m_setComponentCalls;
        m_lastComponentId = classId;
        m_lastComponent = component;
        if (classId == dice::hfe::ai::IID_IAIObject && !component)
            m_aiObject = 0;
        return m_setComponentResult;
    }

    virtual dice::hfe::IBase* queryComponent(unsigned int classId,
                                              unsigned int interfaceId)
    {
        trace(classId == dice::hfe::world::IID_IPortalPassing
                  ? "simple:queryPortal" : "simple:querySector");
        m_queryCalls.push_back(std::make_pair(classId, interfaceId));
        if (classId == dice::hfe::world::IID_IPortalPassing)
            return m_portalResult ? reinterpret_cast<dice::hfe::IBase*>(this) : 0;
        if (classId == dice::hfe::world::IID_ISector)
            return m_sectorResult ? reinterpret_cast<dice::hfe::IBase*>(this) : 0;
        return 0;
    }

    virtual unsigned int getObjectCacheID() const
    {
        trace("base:getCacheId");
        ++m_getCacheIdCalls;
        return IObject::getObjectCacheID();
    }

    virtual void updateFlags(unsigned int set, unsigned int clear)
    {
        trace("base:updateFlags");
        m_flagCalls.push_back(std::make_pair(set, clear));
        m_flags = (m_flags | set) & ~clear;
    }

    virtual void SetActiveLod(dice::hfe::world::ObjectLodType)
    {
        trace("base:setActiveLod");
        ++m_activeLodCalls;
    }

    virtual void setAbsoluteTransformation(const Mat4&)
    {
        trace("base:setAbsolute");
        ++m_absoluteCalls;
    }

    virtual void updateGeometry()
    {
        trace("base:updateGeometry");
        ++m_geometryCalls;
    }

    void configureCachedState(bool withAi)
    {
        m_objectCacheId = 77;
        m_flags &= ~0x20u;
        m_aiObject = withAi ? this : 0;
        m_portalResult = false;
        m_sectorResult = false;
        m_setComponentCalls = 0;
        m_getCacheIdCalls = 0;
        m_activeLodCalls = 0;
        m_absoluteCalls = 0;
        m_geometryCalls = 0;
        m_lastComponentId = 0;
        m_lastComponent = 0;
        m_queryCalls.clear();
        m_flagCalls.clear();
    }

    void prepareForCleanup()
    {
        m_aiObject = 0;
        m_objectCacheId = 0;
        m_flags &= ~0x20u;
    }

    bool hasAiForTest() const { return m_aiObject != 0; }

    bool m_portalResult;
    bool m_sectorResult;
    bool m_setComponentResult;
    int m_setComponentCalls;
    mutable int m_getCacheIdCalls;
    int m_activeLodCalls;
    int m_absoluteCalls;
    int m_geometryCalls;
    unsigned int m_lastComponentId;
    dice::hfe::IBase* m_lastComponent;
    std::vector<std::pair<unsigned int, unsigned int> > m_queryCalls;
    std::vector<std::pair<unsigned int, unsigned int> > m_flagCalls;

private:
    void trace(const char* event) const
    {
        if (m_events) m_events->push_back(event);
    }

    std::vector<std::string>* m_events;
};

class ProbeObject : public Object
{
public:
    ProbeObject(const IObjectTemplate* objectTemplate,
                std::vector<std::string>* events)
        : Object(objectTemplate),
          m_events(events),
          m_initCalls(0),
          m_inputCalls(0),
          m_childIdCalls(0),
          m_relativeCalls(0),
          m_lastRelative(Mat4::getIdentity())
    {}

    virtual void init()
    {
        if (m_events) m_events->push_back("init");
        ++m_initCalls;
        Object::init();
    }

    virtual void setChildId(char id)
    {
        if (m_events) m_events->push_back("setChildId");
        Object::setChildId(id);
    }

    virtual char getChildId() const
    {
        ++m_childIdCalls;
        return Object::getChildId();
    }

    virtual void setRelativeTransformation(const Mat4& transformation)
    {
        if (m_events) m_events->push_back("setRelativeTransformation");
        ++m_relativeCalls;
        m_lastRelative = transformation;
        Object::setRelativeTransformation(transformation);
    }

    virtual void setInputId(char inputId)
    {
        ++m_inputCalls;
        Object::setInputId(inputId);
    }

    int m_initCalls;
    int m_inputCalls;
    mutable int m_childIdCalls;
    int m_relativeCalls;
    Mat4 m_lastRelative;

private:
    std::vector<std::string>* m_events;
};

class ProbeTemplate : public SimpleObjectTemplate
{
public:
    explicit ProbeTemplate(std::vector<std::string>* events,
                           SoundTemplateProbe* sound = 0)
        : m_events(events), m_sound(sound), m_createCalls(0) {}

    virtual void* queryInterface(unsigned int iid) const
    {
        if (iid == kIID_ISoundObjectTemplate && m_sound)
            return m_sound;
        return SimpleObjectTemplate::queryInterface(iid);
    }

    virtual IObject* createObject() const
    {
        if (m_events) m_events->push_back("createObject");
        ++m_createCalls;
        ProbeObject* object = new ProbeObject(this, m_events);
        m_created.push_back(object);
        return object;
    }

    mutable std::vector<ProbeObject*> m_created;
    mutable int m_createCalls;

    float floaterMod() const { return m_floaterMod; }

private:
    std::vector<std::string>* m_events;
    SoundTemplateProbe* m_sound;
};

class ProbeHost : public Object
{
public:
    ProbeHost(const IObjectTemplate* objectTemplate,
              std::vector<std::string>* events)
        : Object(objectTemplate), m_events(events), m_childIdSeenAtAdd(-99) {}

    virtual void addChild(IObject* child)
    {
        if (m_events) m_events->push_back("addChild");
        m_childIdSeenAtAdd = child ? child->getChildId() : -99;
        Object::addChild(child);
    }

    int m_childIdSeenAtAdd;

private:
    std::vector<std::string>* m_events;
};

class DecalTemplateProbe : public ProbeTemplate
{
public:
    explicit DecalTemplateProbe(std::vector<std::string>* events)
        : ProbeTemplate(events), m_classIdCalls(0) {}

    virtual unsigned int getClassID() const
    {
        ++m_classIdCalls;
        return dice::hfe::world::CID_DecalTemplate;
    }

    mutable int m_classIdCalls;
};

class DriverHost : public ProbeHost
{
public:
    DriverHost(const IObjectTemplate* objectTemplate, PlayerControlProbe* playerControl)
        : ProbeHost(objectTemplate, 0), m_playerControl(playerControl) {}

    virtual void* queryInterface(unsigned int iid) const
    {
        if (iid == dice::hfe::world::IID_IPlayerControlObject)
            return m_playerControl;
        return ProbeHost::queryInterface(iid);
    }

private:
    PlayerControlProbe* m_playerControl;
};

// Explicit PE-x86 ABI fixture for Bundle.cpp's one recovered IGeometry call. The production
// boundary intentionally keeps IGeometry incomplete, so this probe names only physical slots:
// nineteen placeholders followed by the operation at +0x4C. It does not claim an original member
// name or a complete renderer interface.
struct GeometryPartCall
{
    Mat4 transformation;
    int part;
};

class GeometryPeAbiProbe
{
public:
    GeometryPeAbiProbe()
        : m_calls(0), m_transformCalls(0),
          m_lastTransformation(Mat4::getIdentity()), m_next(0), m_setLodCalls(0),
          m_lodCountCalls(0), m_lodCount(1), m_lastLod(-1), m_lightSetCalls(0),
          m_lightMask(7), m_radiusEvents(0), m_geometryTemplate(0),
          m_templateCalls(0), m_radiusCalls(0), m_radius(0.0f),
          m_uvTranslationCalls(0), m_lastUVIndex(-1), m_uvRotationCalls(0),
          m_lastUVRotationIndex(-1), m_lastUVRotation(Mat4::getIdentity())
    {
        m_lastUVTranslation[0] = 0.0f;
        m_lastUVTranslation[1] = 0.0f;
    }

    int m_uvTranslationCalls;
    int m_lastUVIndex;
    float m_lastUVTranslation[2];
    int m_uvRotationCalls;
    int m_lastUVRotationIndex;
    Mat4 m_lastUVRotation;

#define BF2_GEOMETRY_TEST_SLOT(n) virtual void slot##n() {}
    BF2_GEOMETRY_TEST_SLOT(00); BF2_GEOMETRY_TEST_SLOT(01);
    BF2_GEOMETRY_TEST_SLOT(02); BF2_GEOMETRY_TEST_SLOT(03);
    BF2_GEOMETRY_TEST_SLOT(04); BF2_GEOMETRY_TEST_SLOT(05);
    BF2_GEOMETRY_TEST_SLOT(06); BF2_GEOMETRY_TEST_SLOT(07);
    BF2_GEOMETRY_TEST_SLOT(08); BF2_GEOMETRY_TEST_SLOT(09);
    BF2_GEOMETRY_TEST_SLOT(10); BF2_GEOMETRY_TEST_SLOT(11);
    BF2_GEOMETRY_TEST_SLOT(12); BF2_GEOMETRY_TEST_SLOT(13);
    virtual void* getTemplateForRadius()
    {
        if (m_radiusEvents) m_radiusEvents->push_back("geometry:template");
        ++m_templateCalls;
        return m_geometryTemplate;
    }
    BF2_GEOMETRY_TEST_SLOT(15); BF2_GEOMETRY_TEST_SLOT(16);
    virtual float getRadiusForTest()
    {
        if (m_radiusEvents) m_radiusEvents->push_back("geometry:radius");
        ++m_radiusCalls;
        return m_radius;
    }
    BF2_GEOMETRY_TEST_SLOT(18);
#undef BF2_GEOMETRY_TEST_SLOT

    // Physical slot 19 == PE +0x4C. The descriptive fixture name is deliberately not presented as
    // the unresolved original IGeometry source spelling.
    virtual void slot19_setPartTransformation(const Mat4& transformation, int part)
    {
        if (runtime_pair_scaffold::enableCachedEvents)
            runtime_pair_scaffold::enableCachedEvents->push_back("geometry:setPart");
        GeometryPartCall call = {transformation, part};
        m_calls->push_back(call);
    }

    virtual void setGeometryTransformation(const Mat4& transformation)
    {
        ++m_transformCalls;
        m_lastTransformation = transformation;
    }

    // Object's light-mask forwarding lands later in the PE table: setter +0xE0 (slot 56),
    // getter +0xE4 (slot 57). Keep the intervening interface surface anonymous.
#define BF2_GEOMETRY_LIGHT_GAP(n) virtual void lightGap##n() {}
    BF2_GEOMETRY_LIGHT_GAP(21); BF2_GEOMETRY_LIGHT_GAP(22);
    BF2_GEOMETRY_LIGHT_GAP(23);
    BF2_GEOMETRY_LIGHT_GAP(24); BF2_GEOMETRY_LIGHT_GAP(25);
    BF2_GEOMETRY_LIGHT_GAP(26);
    virtual void setGeometryLod(int lod) { ++m_setLodCalls; m_lastLod = lod; }
    BF2_GEOMETRY_LIGHT_GAP(28);
    virtual int getGeometryLodCount() { ++m_lodCountCalls; return m_lodCount; }
    BF2_GEOMETRY_LIGHT_GAP(30); BF2_GEOMETRY_LIGHT_GAP(31);
    BF2_GEOMETRY_LIGHT_GAP(32); BF2_GEOMETRY_LIGHT_GAP(33);
    BF2_GEOMETRY_LIGHT_GAP(34); BF2_GEOMETRY_LIGHT_GAP(35);
#undef BF2_GEOMETRY_LIGHT_GAP
    // Physical slot 36 == PE +0x90 and slot 38 == PE +0x98 - the two operations the animated-UV
    // update drives. Descriptive fixture names again, not claimed IGeometry spellings.
    virtual void slot36_setAnimatedUVTranslation(int index, float first, float second)
    {
        ++m_uvTranslationCalls;
        m_lastUVIndex = index;
        m_lastUVTranslation[0] = first;
        m_lastUVTranslation[1] = second;
    }
#define BF2_GEOMETRY_LIGHT_GAP(n) virtual void lightGap##n() {}
    BF2_GEOMETRY_LIGHT_GAP(37);
#undef BF2_GEOMETRY_LIGHT_GAP
    virtual void slot38_setAnimatedUVRotation(int index, const Mat4& transform)
    {
        ++m_uvRotationCalls;
        m_lastUVRotationIndex = index;
        m_lastUVRotation = transform;
    }
#define BF2_GEOMETRY_LIGHT_GAP(n) virtual void lightGap##n() {}
    BF2_GEOMETRY_LIGHT_GAP(39);
    BF2_GEOMETRY_LIGHT_GAP(40); BF2_GEOMETRY_LIGHT_GAP(41);
    BF2_GEOMETRY_LIGHT_GAP(42); BF2_GEOMETRY_LIGHT_GAP(43);
    BF2_GEOMETRY_LIGHT_GAP(44); BF2_GEOMETRY_LIGHT_GAP(45);
    BF2_GEOMETRY_LIGHT_GAP(46); BF2_GEOMETRY_LIGHT_GAP(47);
    BF2_GEOMETRY_LIGHT_GAP(48); BF2_GEOMETRY_LIGHT_GAP(49);
    BF2_GEOMETRY_LIGHT_GAP(50);
    virtual GeometryPeAbiProbe* getNextGeometry() { return m_next; }
    BF2_GEOMETRY_LIGHT_GAP(52); BF2_GEOMETRY_LIGHT_GAP(53);
    BF2_GEOMETRY_LIGHT_GAP(54); BF2_GEOMETRY_LIGHT_GAP(55);
#undef BF2_GEOMETRY_LIGHT_GAP
    virtual void setLightSourceMask(int mask) { ++m_lightSetCalls; m_lightMask = mask; }
    virtual int getLightSourceMask() const { return m_lightMask; }

    std::vector<GeometryPartCall>* m_calls;
    int m_transformCalls;
    Mat4 m_lastTransformation;
    GeometryPeAbiProbe* m_next;
    int m_setLodCalls;
    int m_lodCountCalls;
    int m_lodCount;
    int m_lastLod;
    int m_lightSetCalls;
    int m_lightMask;
    std::vector<std::string>* m_radiusEvents;
    void* m_geometryTemplate;
    int m_templateCalls;
    int m_radiusCalls;
    float m_radius;
};

// Raw geometry-template probe through the PE has-radius operation at physical +0x28. The fixture
// deliberately does not assign an unrecovered interface/type name to the preceding slots.
class GeometryTemplatePeAbiProbe
{
public:
    explicit GeometryTemplatePeAbiProbe(std::vector<std::string>* events = 0)
        : m_events(events), m_hasRadiusCalls(0), m_hasRadius(false) {}

#define BF2_GEOMETRY_TEMPLATE_TEST_SLOT(n) virtual void slot##n() {}
    BF2_GEOMETRY_TEMPLATE_TEST_SLOT(00); BF2_GEOMETRY_TEMPLATE_TEST_SLOT(01);
    BF2_GEOMETRY_TEMPLATE_TEST_SLOT(02); BF2_GEOMETRY_TEMPLATE_TEST_SLOT(03);
    BF2_GEOMETRY_TEMPLATE_TEST_SLOT(04); BF2_GEOMETRY_TEMPLATE_TEST_SLOT(05);
    BF2_GEOMETRY_TEMPLATE_TEST_SLOT(06); BF2_GEOMETRY_TEMPLATE_TEST_SLOT(07);
    BF2_GEOMETRY_TEMPLATE_TEST_SLOT(08); BF2_GEOMETRY_TEMPLATE_TEST_SLOT(09);
#undef BF2_GEOMETRY_TEMPLATE_TEST_SLOT
    virtual bool hasRadiusForTest()
    {
        if (m_events) m_events->push_back("template:hasRadius");
        ++m_hasRadiusCalls;
        return m_hasRadius;
    }

    std::vector<std::string>* m_events;
    int m_hasRadiusCalls;
    bool m_hasRadius;
};

// Explicit prefix-only PE view of ICollisionMesh. The single MSVC destructor slot places the
// symbol-attested createMaterialMapping(const IObjectTemplate*) operation at physical +0x24.
class CollisionMeshPeAbiProbe : public dice::hfe::IBase
{
public:
    explicit CollisionMeshPeAbiProbe(std::vector<std::string>* events)
        : m_events(events), m_refs(5), m_mappingCalls(0), m_lastTemplate(0),
          m_collisionLodCalls(0), m_availableLod(-1), m_lastCollisionLod(-1),
          m_releaseCalls(0), m_addCalls(0), m_queryCalls(0),
          m_radiusCalls(0), m_radius(0.0f), m_boundsCalls(0), m_boundsResult(true),
          m_boundsInputsWereZero(false), m_cacheResetCalls(0),
          m_distanceCalls(0), m_distanceResult(false), m_distanceLod(-1),
          m_distanceNoFacingFilter(false), m_distanceFrontFaces(false),
          m_distanceFirstHit(false), m_distanceFaceInfo(0),
          m_distancePlaneDist(0.0f), m_distanceAlong(0.0f), m_distanceMaterial(0)
    {
        m_boundsMinimum.x = m_boundsMinimum.y = m_boundsMinimum.z = 0.0f;
        m_boundsMaximum.x = m_boundsMaximum.y = m_boundsMaximum.z = 0.0f;
        m_distanceRayStart = Vec3::getZero();
        m_distanceDirection = Vec3::getZero();
        m_distanceNormal = Vec3::getZero();
        m_distancePosition = Vec3::getZero();
    }

    virtual void addRef()
    {
        if (m_events) m_events->push_back("meshAddRef");
        ++m_addCalls;
        ++m_refs;
    }
    virtual int getRef() { return m_refs; }
    virtual int release()
    {
        if (m_events) m_events->push_back("release");
        ++m_releaseCalls;
        return --m_refs;
    }
    virtual void* queryInterface(unsigned int iid) const
    {
        if (m_events) m_events->push_back(std::string("meshQuery:") + std::to_string(iid));
        ++m_queryCalls;
        if (iid == dice::hfe::world::IID_ICollisionMesh)
            return const_cast<CollisionMeshPeAbiProbe*>(this);
        return 0;
    }
    virtual ~CollisionMeshPeAbiProbe() {}
    virtual void resetForCache()
    {
        if (m_events) m_events->push_back("mesh:reset");
        ++m_cacheResetCalls;
    }
    virtual void slot18_getWeakPtr() {}
    virtual void slot1C_getTemplate() {}
    virtual void slot20_getPathAndName() {}
    virtual void createMaterialMapping(const IObjectTemplate* objectTemplate)
    {
        if (m_events) m_events->push_back("mapping");
        ++m_mappingCalls;
        m_lastTemplate = objectTemplate;
    }
    virtual bool hasCollisionLod(int lod)
    {
        ++m_collisionLodCalls;
        m_lastCollisionLod = lod;
        return lod == m_availableLod;
    }

#define BF2_COLLISION_MESH_BOUNDS_GAP(n) virtual void boundsGap##n() {}
    BF2_COLLISION_MESH_BOUNDS_GAP(2C); BF2_COLLISION_MESH_BOUNDS_GAP(30);
    BF2_COLLISION_MESH_BOUNDS_GAP(34); BF2_COLLISION_MESH_BOUNDS_GAP(38);
    BF2_COLLISION_MESH_BOUNDS_GAP(3C); BF2_COLLISION_MESH_BOUNDS_GAP(40);
    BF2_COLLISION_MESH_BOUNDS_GAP(44); BF2_COLLISION_MESH_BOUNDS_GAP(48);
    BF2_COLLISION_MESH_BOUNDS_GAP(4C); BF2_COLLISION_MESH_BOUNDS_GAP(50);
    BF2_COLLISION_MESH_BOUNDS_GAP(54);
#undef BF2_COLLISION_MESH_BOUNDS_GAP
    virtual bool getDistanceAtPhysical58(
        int lod, bool noFacingFilter, const Mat4&, const Mat4&,
        const Vec3& rayStart, const Vec3& direction,
        Vec3& outNormal, Vec3& outPosition, float& outPlaneDist, float& outAlong,
        int* outMaterial, dice::hfe::world::CollisionFaceInfo* faceInfo,
        bool frontFaces, bool firstHit) const
    {
        ++m_distanceCalls;
        m_distanceLod = lod;
        m_distanceNoFacingFilter = noFacingFilter;
        m_distanceRayStart = rayStart;
        m_distanceDirection = direction;
        m_distanceFrontFaces = frontFaces;
        m_distanceFirstHit = firstHit;
        m_distanceFaceInfo = faceInfo;
        if (!m_distanceResult)
            return false;

        outNormal = m_distanceNormal;
        outPosition = m_distancePosition;
        outPlaneDist = m_distancePlaneDist;
        outAlong = m_distanceAlong;
        if (outMaterial)
            *outMaterial = m_distanceMaterial;
        return true;
    }

#define BF2_COLLISION_MESH_BOUNDS_GAP(n) virtual void boundsGap##n() {}
    BF2_COLLISION_MESH_BOUNDS_GAP(5C); BF2_COLLISION_MESH_BOUNDS_GAP(60);
    BF2_COLLISION_MESH_BOUNDS_GAP(64); BF2_COLLISION_MESH_BOUNDS_GAP(68);
    BF2_COLLISION_MESH_BOUNDS_GAP(6C); BF2_COLLISION_MESH_BOUNDS_GAP(70);
#undef BF2_COLLISION_MESH_BOUNDS_GAP
    virtual float getMaxBoundingRadiusForTest()
    {
        if (m_events) m_events->push_back("mesh:radius");
        ++m_radiusCalls;
        return m_radius;
    }
    virtual bool getBoundingEndpoints(Vec3& minimum, Vec3& maximum)
    {
        ++m_boundsCalls;
        m_boundsInputsWereZero =
            minimum.x == 0.0f && minimum.y == 0.0f && minimum.z == 0.0f &&
            maximum.x == 0.0f && maximum.y == 0.0f && maximum.z == 0.0f;
        minimum = m_boundsMinimum;
        maximum = m_boundsMaximum;
        return m_boundsResult;
    }

    void setBounds(float minX, float minY, float minZ,
                   float maxX, float maxY, float maxZ)
    {
        m_boundsMinimum.x = minX;
        m_boundsMinimum.y = minY;
        m_boundsMinimum.z = minZ;
        m_boundsMaximum.x = maxX;
        m_boundsMaximum.y = maxY;
        m_boundsMaximum.z = maxZ;
    }

    void setDistanceHit(const Vec3& normal, const Vec3& position,
                        float planeDistance, float alongDistance, int material)
    {
        m_distanceResult = true;
        m_distanceNormal = normal;
        m_distancePosition = position;
        m_distancePlaneDist = planeDistance;
        m_distanceAlong = alongDistance;
        m_distanceMaterial = material;
    }

    int m_mappingCalls;
    const IObjectTemplate* m_lastTemplate;
    int m_collisionLodCalls;
    int m_availableLod;
    int m_lastCollisionLod;
    int m_releaseCalls;
    int m_addCalls;
    mutable int m_queryCalls;
    int m_radiusCalls;
    float m_radius;
    int m_boundsCalls;
    bool m_boundsResult;
    bool m_boundsInputsWereZero;
    int m_cacheResetCalls;
    Vec3 m_boundsMinimum;
    Vec3 m_boundsMaximum;
    mutable int m_distanceCalls;
    bool m_distanceResult;
    mutable int m_distanceLod;
    mutable bool m_distanceNoFacingFilter;
    mutable bool m_distanceFrontFaces;
    mutable bool m_distanceFirstHit;
    mutable dice::hfe::world::CollisionFaceInfo* m_distanceFaceInfo;
    mutable Vec3 m_distanceRayStart;
    mutable Vec3 m_distanceDirection;
    Vec3 m_distanceNormal;
    Vec3 m_distancePosition;
    float m_distancePlaneDist;
    float m_distanceAlong;
    int m_distanceMaterial;

private:
    std::vector<std::string>* m_events;
    int m_refs;
};

// Explicit PE prefix through ICollisionManager::getCollisionMeshPart at +0x28. Only the operation
// reached by Bundle.cpp has behavior; preceding slots preserve the recovered physical ABI.
class CollisionManagerPeAbiProbe : public dice::hfe::IBase
{
public:
    CollisionManagerPeAbiProbe(std::vector<std::string>* events,
                               CollisionMeshPeAbiProbe* mesh)
        : m_events(events), m_mesh(mesh), m_refs(1), m_lookupCalls(0) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual ~CollisionManagerPeAbiProbe() {}
    virtual void slot14_init() {}
    virtual void slot18_setUseDeferredLoading(bool) {}
    virtual bool slot1C_getUseDeferredLoading() const { return false; }
    virtual ICollisionMesh* slot20_create(const std::string&) { return 0; }
    virtual CollisionMeshTemplate* slot24_getCollisionMeshTemplate(const std::string&)
    {
        return 0;
    }
    virtual ICollisionMesh* getCollisionMeshPart(const std::string& name, int part)
    {
        if (m_events) m_events->push_back(std::string("lookup:") + std::to_string(part));
        ++m_lookupCalls;
        m_names.push_back(name);
        m_parts.push_back(part);
        return reinterpret_cast<ICollisionMesh*>(m_mesh);
    }

    int m_lookupCalls;
    std::vector<std::string> m_names;
    std::vector<int> m_parts;

private:
    std::vector<std::string>* m_events;
    CollisionMeshPeAbiProbe* m_mesh;
    int m_refs;
};

// Explicit PE prefix through ClassManager::getSingleton at physical +0x48. SimpleObject's
// constructor reaches the collision manager through this singleton lookup; no unrecovered
// ClassManager operation is shape-declared here.
class ClassManagerPeAbiProbe
{
public:
    ClassManagerPeAbiProbe(std::vector<std::string>* events, dice::hfe::IBase* result)
        : m_lookupCalls(0), m_classNameCalls(0), m_lastClassNameId(0), m_classNameResult(true),
          m_createCalls(0), m_lastClassId(0), m_lastIid(0), m_lastOuter(0),
          m_events(events), m_result(result), m_refs(1), m_created(0) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual ~ClassManagerPeAbiProbe() {}

#define BF2_CLASS_MANAGER_TEST_SLOT(n) virtual void slot##n() {}
    BF2_CLASS_MANAGER_TEST_SLOT(14); BF2_CLASS_MANAGER_TEST_SLOT(18);
    BF2_CLASS_MANAGER_TEST_SLOT(1C); BF2_CLASS_MANAGER_TEST_SLOT(20);
    BF2_CLASS_MANAGER_TEST_SLOT(24);
#undef BF2_CLASS_MANAGER_TEST_SLOT
    // Physical +0x28 - ClassManager::getClassName, the component-name lookup makeScript uses.
    virtual bool getClassNameAtPhysical28(unsigned int classId, std::string& out)
    {
        ++m_classNameCalls;
        m_lastClassNameId = classId;
        out = m_className;
        return m_classNameResult;
    }
#define BF2_CLASS_MANAGER_TEST_SLOT(n) virtual void slot##n() {}
    BF2_CLASS_MANAGER_TEST_SLOT(2C);

    // Physical +0x30 - createInstance(classId, iid, outer), the overload setNetworkableComp uses.
    // It is ELF +0x34: MSVC reverses the three same-name overloads, and the middle of three keeps
    // the middle slot either way, which is why both ports agree on this one and not on its
    // neighbours.
    virtual dice::hfe::IBase* createInstanceAtPhysical30(unsigned int classId, unsigned int iid,
                                                        dice::hfe::IBase* outer)
    {
        if (m_events) m_events->push_back("classManager:createInstance");
        ++m_createCalls;
        m_lastClassId = classId;
        m_lastIid = iid;
        m_lastOuter = outer;
        return m_created;
    }

    BF2_CLASS_MANAGER_TEST_SLOT(34); BF2_CLASS_MANAGER_TEST_SLOT(38);
    BF2_CLASS_MANAGER_TEST_SLOT(3C); BF2_CLASS_MANAGER_TEST_SLOT(40);
    BF2_CLASS_MANAGER_TEST_SLOT(44);
#undef BF2_CLASS_MANAGER_TEST_SLOT

    virtual dice::hfe::IBase* getSingleton(const std::string& name)
    {
        if (m_events) m_events->push_back(std::string("singleton:") + name);
        ++m_lookupCalls;
        m_names.push_back(name);
        return m_result;
    }

    void setCreatedInstance(dice::hfe::IBase* created) { m_created = created; }

    int m_lookupCalls;
    std::vector<std::string> m_names;
    int m_classNameCalls;
    unsigned int m_lastClassNameId;
    std::string m_className;
    bool m_classNameResult;
    int m_createCalls;
    unsigned int m_lastClassId;
    unsigned int m_lastIid;
    dice::hfe::IBase* m_lastOuter;

private:
    std::vector<std::string>* m_events;
    dice::hfe::IBase* m_result;
    int m_refs;
    dice::hfe::IBase* m_created;
};

struct LightManagerCallSnapshot
{
    std::string operation;
    LightSource* light;
    unsigned int flags;
    bool enabled;
    bool destroyed;
};

// Explicit PE primary-table fixture through LightManager::disableLight at +0x98. The production
// class exposes ordinary typed virtual methods, so this prefix proves that LightSource reaches the
// recovered physical entries without a raw slot-indexing facade in engine code.
class LightManagerPeAbiProbe : public dice::hfe::IBase
{
public:
    explicit LightManagerPeAbiProbe(std::vector<std::string>* events)
        : m_events(events), m_refs(1), m_skyLight(0) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual ~LightManagerPeAbiProbe() {}

#define BF2_LIGHT_MANAGER_TEST_SLOT(n) virtual void slot##n() {}
    BF2_LIGHT_MANAGER_TEST_SLOT(14); BF2_LIGHT_MANAGER_TEST_SLOT(18);
    BF2_LIGHT_MANAGER_TEST_SLOT(1C); BF2_LIGHT_MANAGER_TEST_SLOT(20);
#undef BF2_LIGHT_MANAGER_TEST_SLOT

    virtual void setSkyLightAtPhysical24(LightSource* light)
    {
        m_skyLight = light;
        record("sky", light);
    }
    virtual LightSource* getSkyLightAtPhysical28() const { return m_skyLight; }

#define BF2_LIGHT_MANAGER_TEST_SLOT(n) virtual void slot##n() {}
    BF2_LIGHT_MANAGER_TEST_SLOT(2C); BF2_LIGHT_MANAGER_TEST_SLOT(30);
    BF2_LIGHT_MANAGER_TEST_SLOT(34); BF2_LIGHT_MANAGER_TEST_SLOT(38);
    BF2_LIGHT_MANAGER_TEST_SLOT(3C); BF2_LIGHT_MANAGER_TEST_SLOT(40);
    BF2_LIGHT_MANAGER_TEST_SLOT(44); BF2_LIGHT_MANAGER_TEST_SLOT(48);
    BF2_LIGHT_MANAGER_TEST_SLOT(4C); BF2_LIGHT_MANAGER_TEST_SLOT(50);
    BF2_LIGHT_MANAGER_TEST_SLOT(54); BF2_LIGHT_MANAGER_TEST_SLOT(58);
    BF2_LIGHT_MANAGER_TEST_SLOT(5C); BF2_LIGHT_MANAGER_TEST_SLOT(60);
    BF2_LIGHT_MANAGER_TEST_SLOT(64); BF2_LIGHT_MANAGER_TEST_SLOT(68);
    BF2_LIGHT_MANAGER_TEST_SLOT(6C); BF2_LIGHT_MANAGER_TEST_SLOT(70);
    BF2_LIGHT_MANAGER_TEST_SLOT(74); BF2_LIGHT_MANAGER_TEST_SLOT(78);
    BF2_LIGHT_MANAGER_TEST_SLOT(7C); BF2_LIGHT_MANAGER_TEST_SLOT(80);
    BF2_LIGHT_MANAGER_TEST_SLOT(84); BF2_LIGHT_MANAGER_TEST_SLOT(88);
#undef BF2_LIGHT_MANAGER_TEST_SLOT

    virtual void addLightAtPhysical8C(LightSource* light) { record("add", light); }
    virtual void removeLightAtPhysical90(LightSource* light) { record("remove", light); }
    virtual void enableLightAtPhysical94(LightSource* light) { record("enable", light); }
    virtual void disableLightAtPhysical98(LightSource* light) { record("disable", light); }

    std::vector<LightManagerCallSnapshot> m_calls;
    LightSource* m_skyLight;

private:
    void record(const char* operation, LightSource* light)
    {
        if (m_events)
            m_events->push_back(std::string("lightManager:") + operation);
        LightManagerCallSnapshot snapshot;
        snapshot.operation = operation;
        snapshot.light = light;
        snapshot.flags = light ? light->getFlags() : 0;
        snapshot.enabled = light ? light->getEnabled() : false;
        snapshot.destroyed = light ? light->isObjectDestroyed() : false;
        m_calls.push_back(snapshot);
    }

    std::vector<std::string>* m_events;
    int m_refs;
};

// replication-component fixtures
// These three derive the real recovered interfaces rather than raw slot prefixes, because all three
// are fully declared in the tree; that also means the networkable below carries io::INetworkable's
// actual field offsets, so the test can read what setNetworkableInfo wrote.

class NetworkableInfoProbe : public dice::hfe::io::INetworkableInfo
{
public:
    NetworkableInfoProbe(std::vector<std::string>* events, float basePriority)
        : m_events(events), m_basePriorityValue(basePriority), m_refs(1), m_basePriorityCalls(0) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }

    virtual void setName(const std::string& name) { m_name = name; }
    virtual const std::string& getName() const { return m_name; }
    virtual int getID() const { return 0; }
    virtual void setID(int) {}
    virtual void setPredictionMode(dice::hfe::io::PredictionMode) {}
    virtual dice::hfe::io::PredictionMode getPredictionMode() const
    {
        return dice::hfe::io::PredictionMode();
    }
    virtual void setBasePriority(float priority) { m_basePriorityValue = priority; }
    virtual float getBasePriority() const
    {
        if (m_events) m_events->push_back("info:getBasePriority");
        ++m_basePriorityCalls;
        return m_basePriorityValue;
    }
    virtual void setIsUnique(bool) {}
    virtual bool getIsUnique() const { return false; }
    virtual void setForceNetworkableId(bool) {}
    virtual bool getForceNetworkableId() const { return false; }
    virtual int getIsControlledBy() const { return 0; }
    virtual void setIsControlledBy(int) {}
    virtual bool getHasOrientation() const { return false; }
    virtual void setHasOrientation(bool) {}
    virtual bool getHasYaw() const { return false; }
    virtual bool getHasPitch() const { return false; }
    virtual bool getHasRoll() const { return false; }
    virtual void setHasYaw(bool) {}
    virtual void setHasPitch(bool) {}
    virtual void setHasRoll(bool) {}

    mutable int m_basePriorityCalls;

private:
    std::vector<std::string>* m_events;
    std::string m_name;
    float m_basePriorityValue;
    int m_refs;
};

class NetworkableInfoManagerProbe : public dice::hfe::io::INetworkableInfoManager
{
public:
    NetworkableInfoManagerProbe(std::vector<std::string>* events,
                                dice::hfe::io::INetworkableInfo* info)
        : m_lookupCalls(0), m_events(events), m_info(info), m_refs(1) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }

    virtual dice::hfe::io::INetworkableInfo* getInfo(const std::string& name) const
    {
        if (m_events) m_events->push_back(std::string("infoManager:getInfo:") + name);
        ++m_lookupCalls;
        m_lookupNames.push_back(name);
        return m_info;
    }
    virtual dice::hfe::io::INetworkableInfo* getInfo(int) const { return 0; }
    virtual void deleteInfo(const std::string&) {}
    virtual void deleteInfo(int) {}
    virtual void deleteInfo(const dice::hfe::io::INetworkableInfo*) {}
    virtual void deleteAllInfos() {}
    virtual const dice::hfe::io::INetworkableInfo* getActiveNetworkableInfo() const { return 0; }
    virtual void setActiveNetworkableInfo(const dice::hfe::io::INetworkableInfo*) {}
    virtual dice::hfe::io::INetworkableInfo* createNewInfo(const std::string&) { return 0; }
    virtual std::list<std::string>& getNetworkableInfoList() { return m_names; }
    virtual const std::string& getWorkingFolderAndFile(const std::string&) const
    {
        return m_workingFolder;
    }
    virtual std::vector<dice::hfe::io::INetworkableInfo*>& getAllUsedTemplates() const
    {
        return m_used;
    }

    void setInfo(dice::hfe::io::INetworkableInfo* info) { m_info = info; }

    mutable int m_lookupCalls;
    mutable std::vector<std::string> m_lookupNames;

private:
    std::vector<std::string>* m_events;
    dice::hfe::io::INetworkableInfo* m_info;
    int m_refs;
    std::list<std::string> m_names;
    std::string m_workingFolder;
    mutable std::vector<dice::hfe::io::INetworkableInfo*> m_used;
};

class NetworkableProbe : public dice::hfe::io::INetworkable
{
public:
    explicit NetworkableProbe(std::vector<std::string>* events)
        : m_initCalls(0), m_lastInitArgument(0), m_events(events), m_refs(1),
          m_releaseCalls(0), m_queryCalls(0) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release()
    {
        if (m_events) m_events->push_back("networkable:release");
        ++m_releaseCalls;
        return --m_refs;
    }
    virtual void* queryInterface(unsigned int iid) const
    {
        ++m_queryCalls;
        if (iid == dice::hfe::io::IID_INetworkable)
            return const_cast<NetworkableProbe*>(this);
        return 0;
    }

    virtual bool init(void* object)
    {
        if (m_events) m_events->push_back("networkable:init");
        ++m_initCalls;
        m_lastInitArgument = object;
        return true;
    }
    virtual unsigned int getGhostStateMask() const { return 0xFFFFFFFFu; }
    virtual void updateStateMask(unsigned int*) {}
    virtual bool getNetUpdate(dice::hfe::io::BitStream&, dice::hfe::io::BaseLineData*,
                              unsigned int, int)
    {
        return false;
    }
    virtual void setNetUpdate(dice::hfe::io::BitStream&, float, bool,
                              dice::hfe::io::BaseLineData*, int) {}
    virtual bool getBaseLine(dice::hfe::io::BaseLineData&) { return true; }

    // io::INetworkable declares these protected; the probe is what makes them observable.
    const dice::hfe::io::INetworkableInfo* installedInfo() const { return m_networkableInfo; }
    float installedBasePriority() const { return m_basePriority; }

    int m_initCalls;
    void* m_lastInitArgument;
    int m_releaseCalls;
    mutable int m_queryCalls;

private:
    std::vector<std::string>* m_events;
    int m_refs;
};

// A real SimpleObject, since initNetworkableComp is SimpleObject's override and Object's default is
// the empty body. The accessor reads io::IID_INetworkable's fast field, which is where
// Object::setComponent files a replication component.
class NetworkableSimpleObjectProbe : public SimpleObject
{
public:
    explicit NetworkableSimpleObjectProbe(const SimpleObjectTemplate* objectTemplate)
        : SimpleObject(objectTemplate) {}

    void* networkableFastField() const { return m_networkable; }
};

class NetworkableTemplateProbe : public FactorySimpleTemplate
{
public:
    NetworkableTemplateProbe(const std::string& info, std::vector<std::string>* events)
        : m_infoCalls(0), m_events(events), m_info(info) {}

    virtual const std::string& getNetworkableInfo() const
    {
        if (m_events) m_events->push_back("template:getNetworkableInfo");
        ++m_infoCalls;
        return m_info;
    }

    void setNetworkableInfoForTest(const std::string& info) { m_info = info; }

    mutable int m_infoCalls;

private:
    std::vector<std::string>* m_events;
    std::string m_info;
};

// collision fixtures
// SimpleObject::handleCollision reaches three opaque surfaces by raw slot. These prefix probes name
// only the physical offsets it uses.

class CollisionMaterialPeAbiProbe
{
public:
    explicit CollisionMaterialPeAbiProbe(bool absorbs)
        : m_absorbCalls(0), m_absorbs(absorbs) {}

#define BF2_MATERIAL_TEST_SLOT(n) virtual void slot##n() {}
    BF2_MATERIAL_TEST_SLOT(00); BF2_MATERIAL_TEST_SLOT(01);
    BF2_MATERIAL_TEST_SLOT(02); BF2_MATERIAL_TEST_SLOT(03);
    BF2_MATERIAL_TEST_SLOT(04); BF2_MATERIAL_TEST_SLOT(05);
    BF2_MATERIAL_TEST_SLOT(06); BF2_MATERIAL_TEST_SLOT(07);
    BF2_MATERIAL_TEST_SLOT(08); BF2_MATERIAL_TEST_SLOT(09);
    BF2_MATERIAL_TEST_SLOT(10); BF2_MATERIAL_TEST_SLOT(11);
    BF2_MATERIAL_TEST_SLOT(12); BF2_MATERIAL_TEST_SLOT(13);
    BF2_MATERIAL_TEST_SLOT(14); BF2_MATERIAL_TEST_SLOT(15);
    BF2_MATERIAL_TEST_SLOT(16); BF2_MATERIAL_TEST_SLOT(17);
    BF2_MATERIAL_TEST_SLOT(18); BF2_MATERIAL_TEST_SLOT(19);
    BF2_MATERIAL_TEST_SLOT(20); BF2_MATERIAL_TEST_SLOT(21);
    BF2_MATERIAL_TEST_SLOT(22); BF2_MATERIAL_TEST_SLOT(23);
    BF2_MATERIAL_TEST_SLOT(24); BF2_MATERIAL_TEST_SLOT(25);
#undef BF2_MATERIAL_TEST_SLOT
    // Physical +0x68 (ELF +0x6C) - the unnamed predicate that vetoes the collision.
    virtual bool absorbsAtPhysical68()
    {
        ++m_absorbCalls;
        return m_absorbs;
    }

    int m_absorbCalls;
    bool m_absorbs;
};

// MaterialManager's PE table puts getMaterialPtr(unsigned int) at +0x28 because MSVC reverses the
// same-name overload pair; the ELF has it at +0x24. Production calls it through the real typed
// declaration, so the compiler picks the right slot on each port - this fixture just has to match
// the PE layout the harness runs on.
class MaterialManagerPeAbiProbe
{
public:
    MaterialManagerPeAbiProbe()
        : m_lookupCalls(0), m_lastId(0), m_material(0) {}

#define BF2_MATERIAL_MANAGER_TEST_SLOT(n) virtual void slot##n() {}
    BF2_MATERIAL_MANAGER_TEST_SLOT(00); BF2_MATERIAL_MANAGER_TEST_SLOT(01);
    BF2_MATERIAL_MANAGER_TEST_SLOT(02); BF2_MATERIAL_MANAGER_TEST_SLOT(03);
    BF2_MATERIAL_MANAGER_TEST_SLOT(04); BF2_MATERIAL_MANAGER_TEST_SLOT(05);
    BF2_MATERIAL_MANAGER_TEST_SLOT(06); BF2_MATERIAL_MANAGER_TEST_SLOT(07);
    BF2_MATERIAL_MANAGER_TEST_SLOT(08); BF2_MATERIAL_MANAGER_TEST_SLOT(09);
#undef BF2_MATERIAL_MANAGER_TEST_SLOT
    virtual void* getMaterialPtrAtPhysical28(unsigned int id)
    {
        ++m_lookupCalls;
        m_lastId = id;
        return m_material;
    }

    int m_lookupCalls;
    unsigned int m_lastId;
    void* m_material;
};

struct GameLogicCollisionCall
{
    IObject* first;
    IObject* second;
    Vec3 firstVector;
    Vec3 secondVector;
    Vec3 thirdVector;
    int firstInt;
    int secondInt;
    dice::hfe::world::CollisionFaceInfo* firstFace;
    dice::hfe::world::CollisionFaceInfo* secondFace;
};

// Both collision entry points sit at the same physical offset on both ports (+0xF4 and +0x100),
// which is what IGameLogic.h's "no virtual destructor here" note predicts. Kept separate from the
// existing GameLogicPeAbiProbe, which models only the short prefix its own tests need.
class GameLogicCollisionPeAbiProbe
{
public:
    GameLogicCollisionPeAbiProbe()
        : m_isServerCalls(0), m_isServer(false), m_projectileCalls(0), m_collisionCalls(0)
    {
        const GameLogicCollisionCall empty = {0, 0, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
                                              {0.0f, 0.0f, 0.0f}, 0, 0, 0, 0};
        m_lastProjectile = empty;
        m_lastCollision = empty;
    }

#define BF2_GAME_LOGIC_TEST_SLOT(n) virtual void slot##n() {}
    BF2_GAME_LOGIC_TEST_SLOT(00); BF2_GAME_LOGIC_TEST_SLOT(01);
    BF2_GAME_LOGIC_TEST_SLOT(02); BF2_GAME_LOGIC_TEST_SLOT(03);
    BF2_GAME_LOGIC_TEST_SLOT(04); BF2_GAME_LOGIC_TEST_SLOT(05);
    BF2_GAME_LOGIC_TEST_SLOT(06); BF2_GAME_LOGIC_TEST_SLOT(07);
    BF2_GAME_LOGIC_TEST_SLOT(08); BF2_GAME_LOGIC_TEST_SLOT(09);
    BF2_GAME_LOGIC_TEST_SLOT(10); BF2_GAME_LOGIC_TEST_SLOT(11);
    BF2_GAME_LOGIC_TEST_SLOT(12); BF2_GAME_LOGIC_TEST_SLOT(13);
    BF2_GAME_LOGIC_TEST_SLOT(14); BF2_GAME_LOGIC_TEST_SLOT(15);
    BF2_GAME_LOGIC_TEST_SLOT(16); BF2_GAME_LOGIC_TEST_SLOT(17);
    BF2_GAME_LOGIC_TEST_SLOT(18); BF2_GAME_LOGIC_TEST_SLOT(19);
    BF2_GAME_LOGIC_TEST_SLOT(20); BF2_GAME_LOGIC_TEST_SLOT(21);
    BF2_GAME_LOGIC_TEST_SLOT(22); BF2_GAME_LOGIC_TEST_SLOT(23);
    BF2_GAME_LOGIC_TEST_SLOT(24); BF2_GAME_LOGIC_TEST_SLOT(25);
    BF2_GAME_LOGIC_TEST_SLOT(26); BF2_GAME_LOGIC_TEST_SLOT(27);
    BF2_GAME_LOGIC_TEST_SLOT(28); BF2_GAME_LOGIC_TEST_SLOT(29);
    BF2_GAME_LOGIC_TEST_SLOT(30); BF2_GAME_LOGIC_TEST_SLOT(31);
    BF2_GAME_LOGIC_TEST_SLOT(32); BF2_GAME_LOGIC_TEST_SLOT(33);
    BF2_GAME_LOGIC_TEST_SLOT(34); BF2_GAME_LOGIC_TEST_SLOT(35);
    BF2_GAME_LOGIC_TEST_SLOT(36); BF2_GAME_LOGIC_TEST_SLOT(37);
    BF2_GAME_LOGIC_TEST_SLOT(38); BF2_GAME_LOGIC_TEST_SLOT(39);
    BF2_GAME_LOGIC_TEST_SLOT(40); BF2_GAME_LOGIC_TEST_SLOT(41);
    BF2_GAME_LOGIC_TEST_SLOT(42); BF2_GAME_LOGIC_TEST_SLOT(43);
    BF2_GAME_LOGIC_TEST_SLOT(44); BF2_GAME_LOGIC_TEST_SLOT(45);
    BF2_GAME_LOGIC_TEST_SLOT(46); BF2_GAME_LOGIC_TEST_SLOT(47);
    BF2_GAME_LOGIC_TEST_SLOT(48); BF2_GAME_LOGIC_TEST_SLOT(49);
    BF2_GAME_LOGIC_TEST_SLOT(50); BF2_GAME_LOGIC_TEST_SLOT(51);
    BF2_GAME_LOGIC_TEST_SLOT(52); BF2_GAME_LOGIC_TEST_SLOT(53);
    BF2_GAME_LOGIC_TEST_SLOT(54); BF2_GAME_LOGIC_TEST_SLOT(55);
    BF2_GAME_LOGIC_TEST_SLOT(56); BF2_GAME_LOGIC_TEST_SLOT(57);
    BF2_GAME_LOGIC_TEST_SLOT(58); BF2_GAME_LOGIC_TEST_SLOT(59);
    BF2_GAME_LOGIC_TEST_SLOT(60);
#undef BF2_GAME_LOGIC_TEST_SLOT
    // Physical +0xF4.
    virtual void handleCollisionForProjectileAtPhysicalF4(
        IObject* first, IObject* second, const Vec3& firstVector, const Vec3& secondVector,
        const Vec3& thirdVector, int firstInt, int secondInt,
        dice::hfe::world::CollisionFaceInfo* firstFace,
        dice::hfe::world::CollisionFaceInfo* secondFace)
    {
        ++m_projectileCalls;
        const GameLogicCollisionCall call = {first, second, firstVector, secondVector, thirdVector,
                                             firstInt, secondInt, firstFace, secondFace};
        m_lastProjectile = call;
    }
    virtual void slot62() {}
    virtual void slot63() {}
    // Physical +0x100.
    virtual void handleCollisionAtPhysical100(
        IObject* first, IObject* second, const Vec3& firstVector, const Vec3& secondVector,
        const Vec3& thirdVector, int firstInt, int secondInt,
        dice::hfe::world::CollisionFaceInfo* firstFace,
        dice::hfe::world::CollisionFaceInfo* secondFace)
    {
        ++m_collisionCalls;
        const GameLogicCollisionCall call = {first, second, firstVector, secondVector, thirdVector,
                                             firstInt, secondInt, firstFace, secondFace};
        m_lastCollision = call;
    }

    // Object::init reaches physical +0x13C on this same global (Object.cpp's gameLogicIsServer), so
    // the fixture has to run that far or every init() in the suite dispatches past the vtable.
#define BF2_GAME_LOGIC_TAIL_SLOT(n) virtual void tailSlot##n() {}
    BF2_GAME_LOGIC_TAIL_SLOT(65); BF2_GAME_LOGIC_TAIL_SLOT(66);
    BF2_GAME_LOGIC_TAIL_SLOT(67); BF2_GAME_LOGIC_TAIL_SLOT(68);
    BF2_GAME_LOGIC_TAIL_SLOT(69); BF2_GAME_LOGIC_TAIL_SLOT(70);
    BF2_GAME_LOGIC_TAIL_SLOT(71); BF2_GAME_LOGIC_TAIL_SLOT(72);
    BF2_GAME_LOGIC_TAIL_SLOT(73); BF2_GAME_LOGIC_TAIL_SLOT(74);
    BF2_GAME_LOGIC_TAIL_SLOT(75); BF2_GAME_LOGIC_TAIL_SLOT(76);
    BF2_GAME_LOGIC_TAIL_SLOT(77); BF2_GAME_LOGIC_TAIL_SLOT(78);
#undef BF2_GAME_LOGIC_TAIL_SLOT
    virtual bool isServerAtPhysical13C() { ++m_isServerCalls; return m_isServer; }

    int m_isServerCalls;
    bool m_isServer;
    int m_projectileCalls;
    int m_collisionCalls;
    GameLogicCollisionCall m_lastProjectile;
    GameLogicCollisionCall m_lastCollision;
};

// The renderer-decal type remains intentionally opaque in production. This prefix fixture names
// only the ten preceding physical slots and the destructor-side operation at PE +0x28.
class RendererDecalPeAbiProbe
{
public:
    explicit RendererDecalPeAbiProbe(std::vector<std::string>* events)
        : m_events(events), m_destroyCalls(0) {}

#define BF2_RENDERER_DECAL_TEST_SLOT(n) virtual void slot##n() {}
    BF2_RENDERER_DECAL_TEST_SLOT(00); BF2_RENDERER_DECAL_TEST_SLOT(04);
    BF2_RENDERER_DECAL_TEST_SLOT(08); BF2_RENDERER_DECAL_TEST_SLOT(0C);
    BF2_RENDERER_DECAL_TEST_SLOT(10); BF2_RENDERER_DECAL_TEST_SLOT(14);
    BF2_RENDERER_DECAL_TEST_SLOT(18); BF2_RENDERER_DECAL_TEST_SLOT(1C);
    BF2_RENDERER_DECAL_TEST_SLOT(20); BF2_RENDERER_DECAL_TEST_SLOT(24);
#undef BF2_RENDERER_DECAL_TEST_SLOT

    virtual void destroyAtPhysical28()
    {
        if (m_events) m_events->push_back("decalDestroy");
        ++m_destroyCalls;
    }

    int m_destroyCalls;

private:
    std::vector<std::string>* m_events;
};

// Explicit PE prefix through the two IPortalManager unregister operations. MSVC's single deleting
// destructor slot puts portal-passing unregister at +0x18 and sector unregister at +0x20.
class PortalManagerPeAbiProbe
{
public:
    explicit PortalManagerPeAbiProbe(std::vector<std::string>* events)
        : m_events(events), m_refs(1), m_portalCalls(0), m_sectorCalls(0),
          m_portalObject(0), m_sectorObject(0), m_managerAfterPortal(0),
          m_sectorResultAfterPortal(0) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release() { return --m_refs; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual ~PortalManagerPeAbiProbe() {}
    virtual void slot14_registerPortalPassingObject() {}
    virtual void unRegisterPortalPassingObject(IObject* object)
    {
        if (m_events) m_events->push_back("unregisterPortal");
        ++m_portalCalls;
        m_portalObject = object;
        if (m_managerAfterPortal)
            dice::hfe::world::portalManager =
                reinterpret_cast<dice::hfe::IBase*>(m_managerAfterPortal);
        if (m_sectorResultAfterPortal)
            *m_sectorResultAfterPortal = true;
    }
    virtual void slot1C_registerSectorObject() {}
    virtual void unRegisterSectorObject(IObject* object)
    {
        if (m_events) m_events->push_back("unregisterSector");
        ++m_sectorCalls;
        m_sectorObject = object;
    }

    int m_portalCalls;
    int m_sectorCalls;
    IObject* m_portalObject;
    IObject* m_sectorObject;
    void* m_managerAfterPortal;
    bool* m_sectorResultAfterPortal;

private:
    std::vector<std::string>* m_events;
    int m_refs;
};

class SimpleLifecycleTemplateProbe : public SimpleObjectTemplate
{
public:
    explicit SimpleLifecycleTemplateProbe(std::vector<std::string>* events)
        : m_events(events), m_setPhysicsCalls(0), m_setResponseCalls(0),
          m_removeResponseCalls(0), m_removePhysicsCalls(0) {}

    void configureConstructorBranches(const std::string& collisionName, int collisionPart,
                                      int physicsFrequency)
    {
        m_collisionMesh = collisionName;
        m_collisionPart = collisionPart;
        m_physicsFrequency = physicsFrequency;
        m_physicsFlags |= 0x04u;
        m_animatedUVTranslation = true;
    }

    virtual void setPhysicsNodeComponent(IObject*) const
    {
        if (m_events) m_events->push_back("setPhysicsNode");
        ++m_setPhysicsCalls;
    }

    virtual void setResponsePhysicsComponent(IObject*) const
    {
        if (m_events) m_events->push_back("setResponsePhysics");
        ++m_setResponseCalls;
    }

    virtual void removeResponsePhysicsComponent(IObject*) const
    {
        if (m_events) m_events->push_back("removeResponsePhysics");
        ++m_removeResponseCalls;
    }

    virtual void removePhysicsNodeComponent(IObject*) const
    {
        if (m_events) m_events->push_back("removePhysicsNode");
        ++m_removePhysicsCalls;
    }

    mutable int m_setPhysicsCalls;
    mutable int m_setResponseCalls;
    mutable int m_removeResponseCalls;
    mutable int m_removePhysicsCalls;

private:
    std::vector<std::string>* m_events;
};

class SimpleLifecycleObjectProbe : public SimpleObject
{
public:
    explicit SimpleLifecycleObjectProbe(const SimpleObjectTemplate* objectTemplate)
        : SimpleObject(objectTemplate) {}

    bool hasInitializedAnimatedUV() const
    {
        if (!m_animatedUV || !m_animatedUV->m_firstSample) return false;
        return m_animatedUV->m_translation[0] == 0.0f &&
               m_animatedUV->m_translation[1] == 0.0f &&
               m_animatedUV->m_rotation == 0.0f &&
               m_animatedUV->m_lastPosition.x == 0.0f &&
               m_animatedUV->m_lastPosition.y == 0.0f &&
               m_animatedUV->m_lastPosition.z == 0.0f &&
               m_animatedUV->m_rotateSource == 0;
    }

    void installDecal(void* decal) { m_decalObject = decal; }
    void enableAiDestructorPath() { m_aiObject = this; }
};

class ChildPartTemplateProbe : public SimpleObjectTemplate
{
public:
    ChildPartTemplateProbe() : m_queryCalls(0) {}

    virtual void* queryImplementation(unsigned int classId)
    {
        ++m_queryCalls;
        return SimpleObjectTemplate::queryImplementation(classId);
    }

    void setParts(int geometryPart, int collisionPart)
    {
        m_geometryPart = geometryPart;
        m_collisionPart = collisionPart;
    }

    void resetQueryCalls() { m_queryCalls = 0; }

    int m_queryCalls;
};

class ChildPartObjectProbe : public Object
{
public:
    ChildPartObjectProbe(const IObjectTemplate* objectTemplate, const char* label,
                         std::vector<std::string>* events, bool setComponentResult)
        : Object(objectTemplate), m_label(label), m_events(events),
          m_setComponentResult(setComponentResult), m_absolute(Mat4::getIdentity()),
          m_absoluteCalls(0), m_setComponentCalls(0), m_lastComponentId(0),
          m_lastComponent(0) {}

    virtual const Mat4& getAbsoluteTransformation() const
    {
        ++m_absoluteCalls;
        return m_absolute;
    }

    virtual bool setComponent(unsigned int classId, dice::hfe::IBase* component)
    {
        if (m_events) m_events->push_back(std::string("set:") + m_label);
        ++m_setComponentCalls;
        m_lastComponentId = classId;
        m_lastComponent = component;
        return m_setComponentResult;
    }

    void setAbsoluteX(float x) { m_absolute.m[12] = x; }
    void setChildForTest(IObject* child) { m_child = child; }
    void setSiblingForTest(IObject* sibling) { m_sibling = sibling; }
    void setOwnGeometryForTest(void* geometry) { m_geometry = geometry; }
    void setOwnCollisionForTest(void* collision) { m_collisionMesh = collision; }
    void setCacheStateForTest(unsigned int id, unsigned int cacheFlags,
                              unsigned int liveFlags)
    {
        m_objectCacheId = id;
        m_cacheFlags = cacheFlags;
        m_flags = liveFlags;
        m_cachedRootParent = 0;
    }
    unsigned int flagsForTest() const { return m_flags; }

    const char* m_label;
    mutable int m_absoluteCalls;
    int m_setComponentCalls;
    unsigned int m_lastComponentId;
    dice::hfe::IBase* m_lastComponent;

private:
    std::vector<std::string>* m_events;
    bool m_setComponentResult;
    Mat4 m_absolute;
};

class ReleaseOrderProbe : public dice::hfe::IBase
{
public:
    ReleaseOrderProbe(const char* label, std::vector<std::string>* events)
        : m_label(label), m_events(events), m_refs(2), m_releaseCalls(0) {}

    virtual void addRef() { ++m_refs; }
    virtual int getRef() { return m_refs; }
    virtual int release()
    {
        if (m_events) m_events->push_back(std::string("release:") + m_label);
        ++m_releaseCalls;
        return --m_refs;
    }
    virtual void* queryInterface(unsigned int) const { return 0; }

    int m_releaseCalls;

private:
    const char* m_label;
    std::vector<std::string>* m_events;
    int m_refs;
};

class PortalUnregisterObjectProbe : public Object
{
public:
    PortalUnregisterObjectProbe(const IObjectTemplate* objectTemplate, const char* label,
                                std::vector<std::string>* events)
        : Object(objectTemplate), m_label(label), m_events(events), m_queryResult(0),
          m_setResult(false), m_queryCalls(0), m_setCalls(0), m_lastClassId(0),
          m_lastQueryClassId(0), m_lastQueryInterfaceId(0), m_lastComponent(0) {}

    virtual dice::hfe::IBase* queryComponent(unsigned int classId,
                                              unsigned int interfaceId)
    {
        if (m_events) m_events->push_back(std::string("query:") + m_label);
        ++m_queryCalls;
        m_lastQueryClassId = classId;
        m_lastQueryInterfaceId = interfaceId;
        return m_queryResult;
    }

    virtual bool setComponent(unsigned int classId, dice::hfe::IBase* component)
    {
        if (m_events)
            m_events->push_back(std::string(component ? "set:" : "detach:") + m_label);
        ++m_setCalls;
        m_lastClassId = classId;
        m_lastComponent = component;
        return m_setResult;
    }

    const char* m_label;
    dice::hfe::IBase* m_queryResult;
    bool m_setResult;
    int m_queryCalls;
    int m_setCalls;
    unsigned int m_lastClassId;
    unsigned int m_lastQueryClassId;
    unsigned int m_lastQueryInterfaceId;
    dice::hfe::IBase* m_lastComponent;

private:
    std::vector<std::string>* m_events;
};

typedef std::map<IObject*, PortalManager::PassingObjectData*> PortalPassingMap;

PortalPassingMap& portalPassingMapAtRecoveredX86Offset(PortalManager& manager)
{
#if defined(BF2_BUNDLE_SOUND_DISPATCH_ONLY) && !defined(_M_IX86) && !defined(__i386__)
    // This PE-layout fixture is outside the portable bundle-interface subset.
    std::abort();
#else
    static_assert(sizeof(void*) == 4,
                  "PortalManager raw map fixture is valid only for the PE-x86 harness");
    // Recovered PortalManager layout: primary vptr +0x00, refcount +0x04, map +0x08. The manager is
    // fully constructed before this typed view is formed, so all std::map invariants are live.
    return *reinterpret_cast<PortalPassingMap*>(
        reinterpret_cast<unsigned char*>(&manager) + 0x08);
#endif
}

int destroyHeapObject(IObject* object)
{
    if (!object) return -1;
    while (object->getChild())
    {
        IObject* child = object->getChild();
        child->forceDestroy();
        child->release();
    }
    object->forceDestroy();
    return object->release();
}

void finishStackObject(Object& object)
{
    while (object.getChild())
    {
        IObject* child = object.getChild();
        child->forceDestroy();
        child->release();
    }
    object.forceDestroy();
}

void finishCacheLifecycleProbe(CacheLifecycleObjectProbe& object)
{
    object.setChildForCacheTest(0);
    object.setSiblingForCacheTest(0);
    object.setParentForCacheTest(0);
    object.setRawPhysicsForCacheTest(0);
    object.setRawResponseForCacheTest(0);
    object.setRawCollisionForCacheTest(0);
    object.setRawNetworkableForCacheTest(0);
    object.setObjectCacheID(0);
    object.clearFlagsForTest(0x20u);
    object.forceDestroy();
}

void testLightSourceManagerLifecycle(Checks& checks)
{
    std::vector<std::string> events;
    LightManagerPeAbiProbe manager(&events);
    ClassManagerPeAbiProbe classManagerProbe(
        &events, static_cast<dice::hfe::IBase*>(&manager));

    dice::hfe::ClassManager* const savedClassManager = dice::hfe::classManager;
    dice::hfe::classManager =
        reinterpret_cast<dice::hfe::ClassManager*>(&classManagerProbe);

    LightSourceTemplate enabledTemplate;
    enabledTemplate.setAffectingType(2);
    enabledTemplate.setEnabled(true);
    LightSource* const light = new LightSource(&enabledTemplate);

    events.clear();
    light->init();

    checks.expect(classManagerProbe.m_lookupCalls == 1 &&
                      classManagerProbe.m_names.size() == 1 &&
                      classManagerProbe.m_names[0] == "LightManager",
                  "LightSource init resolves the LightManager singleton by name");
    checks.expect(events == std::vector<std::string>({
                      "singleton:LightManager", "lightManager:add", "lightManager:enable"}),
                  "LightSource enabled init dispatches +0x8C before +0x94");
    checks.expect(manager.m_calls.size() == 2 &&
                      manager.m_calls[0].operation == "add" &&
                      manager.m_calls[1].operation == "enable" &&
                      manager.m_calls[0].light == light &&
                      manager.m_calls[1].light == light,
                  "LightSource init passes itself through both typed manager slots");
    checks.expect(manager.m_calls.size() == 2 &&
                      (manager.m_calls[1].flags & 0x2001u) == 0x2000u &&
                      !manager.m_calls[1].enabled && light->getEnabled(),
                  "LightSource enable updates flags before +0x94 and stores m_enabled afterward");

    const std::size_t enabledInitCalls = manager.m_calls.size();
    light->setEnabled(true);
    checks.expect(manager.m_calls.size() == enabledInitCalls,
                  "LightSource same-enabled state performs no manager dispatch");

    light->setEnabled(false);
    checks.expect(manager.m_calls.size() == enabledInitCalls + 1 &&
                      manager.m_calls.back().operation == "disable" &&
                      (manager.m_calls.back().flags & 0x2001u) == 1u &&
                      manager.m_calls.back().enabled && !light->getEnabled(),
                  "LightSource disable updates flags before +0x98 and stores m_enabled afterward");
    const std::size_t disabledCalls = manager.m_calls.size();
    light->setEnabled(false);
    checks.expect(manager.m_calls.size() == disabledCalls,
                  "LightSource same-disabled state performs no manager dispatch");

    light->setEnabled(true);
    checks.expect(manager.m_calls.size() == disabledCalls + 1 &&
                      manager.m_calls.back().operation == "enable" &&
                      !manager.m_calls.back().enabled && light->getEnabled(),
                  "LightSource re-enable preserves callback-before-state-store ordering");

    std::vector<std::string> skyEvents;
    LightManagerPeAbiProbe skyManager(&skyEvents);
    ClassManagerPeAbiProbe skyClassManager(
        &skyEvents, static_cast<dice::hfe::IBase*>(&skyManager));
    dice::hfe::classManager =
        reinterpret_cast<dice::hfe::ClassManager*>(&skyClassManager);

    light->setAsSkyLight();
    checks.expect(skyClassManager.m_lookupCalls == 1 &&
                      skyClassManager.m_names.size() == 1 &&
                      skyClassManager.m_names[0] == "LightManager" &&
                      skyEvents == std::vector<std::string>({
                          "singleton:LightManager", "lightManager:sky"}),
                  "LightSource setAsSkyLight performs a fresh singleton lookup before +0x24");
    checks.expect(skyManager.m_calls.size() == 1 &&
                      skyManager.m_calls[0].operation == "sky" &&
                      skyManager.m_skyLight == light &&
                      manager.m_skyLight == 0,
                  "LightSource setAsSkyLight uses the freshly resolved manager, not its init cache");

    events.clear();
    const std::size_t beforeDestroyCalls = manager.m_calls.size();
    light->destroy(false);
    checks.expect(events == std::vector<std::string>({
                      "lightManager:disable", "lightManager:remove"}) &&
                      manager.m_calls.size() == beforeDestroyCalls + 2 &&
                      manager.m_calls[beforeDestroyCalls].operation == "disable" &&
                      manager.m_calls[beforeDestroyCalls + 1].operation == "remove",
                  "LightSource destroy dispatches +0x98 before +0x90");
    checks.expect(!manager.m_calls[beforeDestroyCalls].destroyed &&
                      !manager.m_calls[beforeDestroyCalls + 1].destroyed &&
                      light->isObjectDestroyed(),
                  "LightSource manager teardown completes before Object marks the light destroyed");

    events.clear();
    const std::size_t afterDestroyCalls = manager.m_calls.size();
    light->destroy(false);
    checks.expect(events.empty() && manager.m_calls.size() == afterDestroyCalls,
                  "LightSource repeated destroy suppresses all manager dispatches");
    checks.expect(light->release() == 0,
                  "LightSource lifecycle fixture releases cleanly after destroy");

    dice::hfe::classManager =
        reinterpret_cast<dice::hfe::ClassManager*>(&classManagerProbe);
    events.clear();
    manager.m_calls.clear();

    LightSourceTemplate disabledTemplate;
    disabledTemplate.setEnabled(false);
    LightSource* const disabledLight = new LightSource(&disabledTemplate);
    disabledLight->init();
    checks.expect(events == std::vector<std::string>({
                      "singleton:LightManager", "lightManager:add", "lightManager:disable"}) &&
                      manager.m_calls.size() == 2 &&
                      manager.m_calls[0].operation == "add" &&
                      manager.m_calls[1].operation == "disable",
                  "LightSource disabled init dispatches +0x8C before +0x98");
    checks.expect(manager.m_calls.size() == 2 &&
                      (manager.m_calls[1].flags & 0x2001u) == 1u &&
                      manager.m_calls[1].enabled && !disabledLight->getEnabled(),
                  "LightSource disabled init exposes old m_enabled during +0x98 then stores false");
    events.clear();
    const std::size_t beforeDisabledDestroy = manager.m_calls.size();
    disabledLight->destroy(false);
    checks.expect(events == std::vector<std::string>({"lightManager:remove"}) &&
                      manager.m_calls.size() == beforeDisabledDestroy + 1 &&
                      manager.m_calls.back().operation == "remove",
                  "LightSource destroy skips +0x98 when already disabled but still calls +0x90");
    checks.expect(disabledLight->release() == 0,
                  "disabled LightSource fixture releases cleanly after destroy");

    std::vector<std::string> nullEvents;
    ClassManagerPeAbiProbe nullClassManager(&nullEvents, 0);
    dice::hfe::classManager =
        reinterpret_cast<dice::hfe::ClassManager*>(&nullClassManager);

    LightSourceTemplate noManagerTemplate;
    noManagerTemplate.setEnabled(true);
    LightSource* const noManagerLight = new LightSource(&noManagerTemplate);
    noManagerLight->init();
    checks.expect(nullEvents == std::vector<std::string>({"singleton:LightManager"}) &&
                      !noManagerLight->getEnabled(),
                  "LightSource null-manager init stops after lookup and keeps its constructor state");
    noManagerLight->destroy(false);
    checks.expect(nullEvents == std::vector<std::string>({"singleton:LightManager"}) &&
                      noManagerLight->release() == 0,
                  "LightSource null-manager destroy performs no raw dispatch and releases cleanly");

    dice::hfe::classManager = savedClassManager;
}

void testObjectInitCacheLifecycle(Checks& checks)
{
    std::vector<std::string> events;
    GameLogicPeAbiProbe logic(&events);
    logic.m_server = true;
    PhysicsNodeSleepingPeAbiProbe physics("physics", false, &events);
    physics.m_mobile = true;
    CacheLifecycleTemplateProbe objectTemplate("cache_init", 0, &events);
    CacheLifecycleComponentProbe later("later", 0x7F050300u, false, &events);
    CacheLifecycleComponentProbe updating("updating", 0x7F050200u, true, &events);
    CacheLifecycleComponentProbe first("first", 0x7F050100u, false, &events);

    dice::hfe::IGameLogic* savedGameLogic = dice::hfe::gameLogic;
    dice::hfe::gameLogic = reinterpret_cast<dice::hfe::IGameLogic*>(&logic);

    {
        CacheLifecycleObjectProbe object(&objectTemplate, "init", &events);
        object.addComponentForTest(0x7F050300u, &later);
        object.addComponentForTest(0x7F050200u, &updating);
        object.addComponentForTest(0x7F050100u, &first);
        object.setRawPhysicsForCacheTest(&physics);
        object.setRawNetworkableForCacheTest(
            reinterpret_cast<dice::hfe::io::INetworkable*>(&object));

        events.clear();
        objectTemplate.m_getFlagsCalls = 0;
        objectTemplate.m_updateFlagsCalls = 0;
        object.Object::init();

        checks.expect(events == std::vector<std::string>({
                          "template:getFlags", "init:updateFlags",
                          "template:getFlags", "init:updateFlags",
                          "template:updateFlags", "init:initNetworkable",
                          "gameLogic:isServer", "init:getNetworkable",
                          "physics:isMobile", "physics:initLatency",
                          "component:init:first", "component:hasUpdate:first",
                          "component:init:updating", "component:hasUpdate:updating",
                          "component:init:later", "component:hasUpdate:later",
                          "init:updateFlags"}),
                      "Object::init preserves flag-template-network-server-component-tail order");
        checks.expect(object.m_flagCalls ==
                          std::vector<std::pair<unsigned int, unsigned int> >({
                              std::make_pair(2u, 0u), std::make_pair(0x400u, 0u),
                              std::make_pair(0x400800u, 0u)}),
                      "Object::init emits both template-derived flags and one component-update tail mask");
        checks.expect(objectTemplate.m_getFlagsCalls == 2 &&
                          objectTemplate.m_updateFlagsCalls == 1 &&
                          objectTemplate.m_lastSet == 0x20u &&
                          objectTemplate.m_lastClear == 0,
                      "Object::init evaluates both template guards then sets the template init latch");
        checks.expect(first.m_initCalls == 1 && updating.m_initCalls == 1 &&
                          later.m_initCalls == 1 && first.m_hasUpdateCalls == 1 &&
                          updating.m_hasUpdateCalls == 1 && later.m_hasUpdateCalls == 1,
                      "Object::init visits every component after an earlier hasUpdate true result");
        checks.expect(logic.m_serverCalls == 1 && physics.m_mobileCalls == 1 &&
                          physics.m_latencyCalls == 1 && object.m_getNetworkableCalls == 1,
                      "Object::init takes the server-networkable-mobile-root latency branch once");

        CacheLifecycleTemplateProbe gateTemplate("cache_gate", 6u, &events);
        CacheLifecycleObjectProbe gate(&gateTemplate, "gate", &events);
        gate.setRawPhysicsForCacheTest(&physics);
        gate.setRawNetworkableForCacheTest(
            reinterpret_cast<dice::hfe::io::INetworkable*>(&gate));

        events.clear();
        logic.m_server = false;
        logic.m_serverCalls = 0;
        physics.m_mobileCalls = physics.m_latencyCalls = 0;
        gate.m_getNetworkableCalls = 0;
        gate.Object::init();
        checks.expect(logic.m_serverCalls == 1 && gate.m_getNetworkableCalls == 0 &&
                          physics.m_mobileCalls == 0 && physics.m_latencyCalls == 0 &&
                          gate.m_flagCalls.empty(),
                      "Object::init stops the latency branch at a non-server and honors both template suppressors");

        logic.m_server = true;
        physics.m_mobile = false;
        logic.m_serverCalls = 0;
        physics.m_mobileCalls = physics.m_latencyCalls = 0;
        gate.m_getNetworkableCalls = 0;
        gate.Object::init();
        checks.expect(gate.m_getNetworkableCalls == 1 && physics.m_mobileCalls == 1 &&
                          physics.m_latencyCalls == 0,
                      "Object::init queries mobility but suppresses latency setup for a static node");

        physics.m_mobile = true;
        physics.m_mobileCalls = physics.m_latencyCalls = 0;
        gate.setParentForCacheTest(&object);
        gate.Object::init();
        checks.expect(physics.m_mobileCalls == 1 && physics.m_latencyCalls == 0,
                      "Object::init suppresses latency setup for a mobile child after the mobility query");
        gate.setParentForCacheTest(0);

        CacheLifecycleObjectProbe noTemplate(0, "noTemplate", &events);
        noTemplate.setDestroyedForCacheTest(true);
        events.clear();
        noTemplate.Object::init();
        checks.expect(!noTemplate.isObjectDestroyed() && events.empty(),
                      "Object::init clears destroyed state before the null-template early return");

        events.clear();
        finishCacheLifecycleProbe(noTemplate);
        finishCacheLifecycleProbe(gate);
        finishCacheLifecycleProbe(object);
    }

    dice::hfe::gameLogic = savedGameLogic;
}

void testObjectDisableCachedLifecycle(Checks& checks)
{
    std::vector<std::string> events;
    ObjectManagerPeAbiProbe manager(2000, &events, "cacheManager");
    manager.m_suppressRefreshTrace = true;
    CacheLifecycleTemplateProbe rootTemplate("disable_root", 0, &events);
    CacheLifecycleTemplateProbe childTemplate("disable_child", 0, &events);
    CacheLifecycleComponentProbe high("high", 0x7F060200u, false, &events);
    CacheLifecycleComponentProbe low("low", 0x7F060100u, false, &events);
    PhysicsNodeSleepingPeAbiProbe physics("physics", false, &events);
    ResponsePhysicsPeAbiProbe response(0, &events);
    CollisionMeshPeAbiProbe collision(&events);

    dice::hfe::world::ObjectManager* savedObjectManager =
        dice::hfe::world::objectManager;
    dice::hfe::world::ObjectManager* savedStaticObjectManager =
        Object::mObjectManager;
    dice::hfe::world::ObjectManager* managerFixture =
        reinterpret_cast<dice::hfe::world::ObjectManager*>(&manager);
    dice::hfe::world::objectManager = managerFixture;
    Object::mObjectManager = managerFixture;

    {
        CacheLifecycleObjectProbe root(&rootTemplate, "disableRoot", &events);
        CacheLifecycleObjectProbe same(&childTemplate, "same", &events);
        CacheLifecycleObjectProbe foreign(&childTemplate, "foreign", &events);
        root.setCacheStateForTest(31u, 0x41008000u, 0x01008000u);
        same.setCacheStateForTest(31u, 0x41008000u, 0x01008000u);
        foreign.setCacheStateForTest(99u, 0x41008000u, 0x01008000u);
        root.setChildForCacheTest(&same);
        same.setSiblingForCacheTest(&foreign);
        same.setParentForCacheTest(&root);
        foreign.setParentForCacheTest(&root);
        root.addComponentForTest(0x7F060200u, &high);
        root.addNullComponentForTest(0x7F060180u);
        root.addComponentForTest(0x7F060100u, &low);
        root.setRawPhysicsForCacheTest(&physics);
        root.setRawResponseForCacheTest(&response);
        root.setRawCollisionForCacheTest(&collision);
        root.setInterpolationForCacheTest(reinterpret_cast<Mat4*>(1));

        events.clear();
        manager.m_destroyObjectCalls = 0;
        manager.m_destroyed.clear();
        root.Object::disableCachedObject();

        checks.expect(events == std::vector<std::string>({
                          "disableRoot:getCacheId", "disableRoot:updateFlags",
                          "disableRoot:getChild", "same:getCacheId",
                          "disableRoot:getCacheId", "same:disableCached",
                          "foreign:getCacheId", "disableRoot:getCacheId",
                          "cacheManager:destroyObject", "component:destroy:low",
                          "disableRoot:setComponent", "component:destroy:high",
                          "disableRoot:setComponent", "disableRoot:setActiveLod",
                          "disableRoot:setAbsolute", "physics:reset",
                          "physics:setEnabled", "response:reset",
                          "response:setEnabled", "mesh:reset",
                          "disableRoot:updateFlags", "responseDirty",
                          "disableRoot:updateGeometry"}),
                      "Object::disableCachedObject preserves child policy, map order, raw resets, and tail order");
        checks.expect(root.m_flagCalls ==
                          std::vector<std::pair<unsigned int, unsigned int> >({
                              std::make_pair(0x20u, 0u),
                              std::make_pair(0x40000001u, 0u)}),
                      "Object::disableCachedObject applies the disable and final dirty masks exactly");
        checks.expect(same.m_disableDispatches == 1 && foreign.m_disableDispatches == 0 &&
                          manager.m_destroyObjectCalls == 1 &&
                          manager.m_destroyed.size() == 1 &&
                          manager.m_destroyed[0] == &foreign,
                      "Object::disableCachedObject recurses only for a same-cache child and destroys a foreign child through the global manager");
        checks.expect(low.m_destroyCalls == 1 && high.m_destroyCalls == 1 &&
                          root.m_removedComponentIds == std::vector<unsigned int>({
                              0x7F060100u, 0x7F060200u}) &&
                          root.componentCountForCacheTest() == 0,
                      "Object::disableCachedObject destroys and removes non-null components in key order while erasing a null entry silently");
        checks.expect(root.m_activeLodCalls == 1 && root.m_absoluteSetCalls == 1 &&
                          root.interpolationForCacheTest() == 0 &&
                          root.m_updateGeometryCalls == 1,
                      "Object::disableCachedObject clears interpolation, resets a root transform, and updates geometry last");
        checks.expect(physics.m_resetCalls == 1 && physics.m_lastReset &&
                          physics.m_setEnabledCalls == 1 && physics.m_lastEnabled &&
                          response.m_cacheResetCalls == 1 &&
                          response.m_setEnabledCalls == 1 && response.m_lastEnabled &&
                          collision.m_cacheResetCalls == 1,
                      "Object::disableCachedObject dispatches the exact PE physics/response/collision reset slots and true arguments");

        events.clear();
        root.setChildForCacheTest(0);
        same.setSiblingForCacheTest(0);
        finishCacheLifecycleProbe(foreign);
        finishCacheLifecycleProbe(same);
        finishCacheLifecycleProbe(root);
    }

    Object::mObjectManager = savedStaticObjectManager;
    dice::hfe::world::objectManager = savedObjectManager;
}

void testObjectEnableCachedLifecycle(Checks& checks)
{
    std::vector<std::string> events;
    GameLogicPeAbiProbe logic(&events);
    dice::hfe::IGameLogic* savedGameLogic = dice::hfe::gameLogic;
    dice::hfe::gameLogic = reinterpret_cast<dice::hfe::IGameLogic*>(&logic);

    CacheLifecycleTemplateProbe teamOneTemplate("Team_Drop_One", 0, &events);
    CacheLifecycleTemplateProbe supplyTemplate("supply_parachute", 0, &events);
    CacheLifecycleTemplateProbe ordinaryTemplate("ordinary", 0, &events);
    CacheLifecycleComponentProbe high("high", 0x7F070200u, false, &events);
    CacheLifecycleComponentProbe low("low", 0x7F070100u, false, &events);
    {
        CacheLifecycleObjectProbe root(&teamOneTemplate, "enableOne", &events);
        CacheLifecycleObjectProbe child(&supplyTemplate, "supplyChild", &events);
        CacheLifecycleObjectProbe sibling(&ordinaryTemplate, "ordinarySibling", &events);
        root.setCacheStateForTest(71u, 0x41008000u, 0x20u);
        root.setChildForCacheTest(&child);
        root.setSiblingForCacheTest(&sibling);
        root.addComponentForTest(0x7F070200u, &high);
        root.addNullComponentForTest(0x7F070180u);
        root.addComponentForTest(0x7F070100u, &low);

        events.clear();
        logic.m_teamCalls = 0;
        logic.m_teams.clear();
        root.Object::enableCachedObject();

        checks.expect(events == std::vector<std::string>({
                          "enableOne:getCacheId", "enableOne:updateFlags",
                          "enableOne:getChild", "template:objectCreated", "component:init:low",
                          "component:init:high", "template:getName:Team_Drop_One",
                          "gameLogic:team1", "enableOne:getChild",
                          "enableOne:getChild", "enableOne:getChild",
                          "template:getName:supply_parachute",
                          "template:getName:ordinary", "enableOne:getChild",
                          "ordinarySibling:enableCached"}),
                      "Object::enableCachedObject restores lifecycle state before the team-one supply recursion filter");
        checks.expect(root.m_flagCalls.size() == 1 &&
                          root.m_flagCalls[0] == std::make_pair(0x41008000u, 0x21u) &&
                          (root.flagsForTest() & 0x21u) == 0,
                      "Object::enableCachedObject clears 0x21 when the cached disabled bit is clear");
        checks.expect(low.m_initCalls == 1 && high.m_initCalls == 1 &&
                          root.componentCountForCacheTest() == 3,
                      "Object::enableCachedObject initializes non-null components in key order and skips a null entry without erasing it");
        checks.expect(logic.m_teams == std::vector<int>({1}) &&
                          root.m_getChildCalls == 5 && child.m_enableDispatches == 0 &&
                          sibling.m_enableDispatches == 1,
                      "Object::enableCachedObject short-circuits team two and suppresses only the exact supply child edge");

        events.clear();
        root.setChildForCacheTest(0);
        root.setSiblingForCacheTest(0);
        finishCacheLifecycleProbe(sibling);
        finishCacheLifecycleProbe(child);
        finishCacheLifecycleProbe(root);
    }

    CacheLifecycleTemplateProbe teamTwoTemplate("TEAM_DROP_TWO", 0, &events);
    CacheLifecycleTemplateProbe caseVariantTemplate("Supply_Parachute", 0, &events);
    {
        CacheLifecycleObjectProbe root(&teamTwoTemplate, "enableTwo", &events);
        CacheLifecycleObjectProbe child(&caseVariantTemplate, "caseChild", &events);
        CacheLifecycleObjectProbe sibling(&supplyTemplate, "supplySibling", &events);
        root.setCacheStateForTest(72u, 0x41008001u, 0x20u);
        root.setChildForCacheTest(&child);
        root.setSiblingForCacheTest(&sibling);

        events.clear();
        logic.m_teamCalls = 0;
        logic.m_teams.clear();
        root.Object::enableCachedObject();

        checks.expect(root.m_flagCalls.size() == 1 &&
                          root.m_flagCalls[0] == std::make_pair(0x41008001u, 0x20u) &&
                          (root.flagsForTest() & 1u) != 0,
                      "Object::enableCachedObject clears only 0x20 when the cached disabled bit is set");
        checks.expect(logic.m_teams == std::vector<int>({1, 2}) &&
                          root.m_getChildCalls == 6 && child.m_enableDispatches == 1 &&
                          sibling.m_enableDispatches == 0,
                      "Object::enableCachedObject queries team two only after mismatch and applies a case-sensitive supply edge test");
        checks.expect(events == std::vector<std::string>({
                          "enableTwo:getCacheId", "enableTwo:updateFlags",
                          "enableTwo:getChild", "template:objectCreated",
                          "template:getName:TEAM_DROP_TWO",
                          "gameLogic:team1", "gameLogic:team2",
                          "enableTwo:getChild", "enableTwo:getChild", "enableTwo:getChild",
                          "template:getName:Supply_Parachute",
                          "template:getName:supply_parachute",
                          "enableTwo:getChild", "enableTwo:getChild",
                          "caseChild:enableCached"}),
                      "Object::enableCachedObject preserves team-two, child-name, sibling-name, and child recursion order");

        events.clear();
        root.setChildForCacheTest(0);
        root.setSiblingForCacheTest(0);
        finishCacheLifecycleProbe(sibling);
        finishCacheLifecycleProbe(child);
        finishCacheLifecycleProbe(root);
    }

    CacheLifecycleTemplateProbe nonTeamTemplate("ordinary_root", 0, &events);
    {
        CacheLifecycleObjectProbe root(&nonTeamTemplate, "enableNormal", &events);
        CacheLifecycleObjectProbe child(&ordinaryTemplate, "normalChild", &events);
        CacheLifecycleObjectProbe sibling(&ordinaryTemplate, "normalSibling", &events);
        root.setCacheStateForTest(73u, 0x41008000u, 0x20u);
        root.setChildForCacheTest(&child);
        root.setSiblingForCacheTest(&sibling);

        events.clear();
        logic.m_teams.clear();
        root.Object::enableCachedObject();
        checks.expect(events == std::vector<std::string>({
                          "enableNormal:getCacheId", "enableNormal:updateFlags",
                          "enableNormal:getChild", "template:objectCreated",
                          "template:getName:ordinary_root",
                          "gameLogic:team1", "gameLogic:team2",
                          "enableNormal:getChild", "enableNormal:getChild",
                          "normalChild:enableCached", "normalSibling:enableCached"}),
                      "Object::enableCachedObject recurses child before direct sibling for a non-team root");

        events.clear();
        root.setChildForCacheTest(0);
        root.setSiblingForCacheTest(0);
        finishCacheLifecycleProbe(sibling);
        finishCacheLifecycleProbe(child);
        finishCacheLifecycleProbe(root);
    }

    dice::hfe::gameLogic = savedGameLogic;
}

void testSimpleObjectTemplatePortalPosition(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    const Vec3& position = objectTemplate.portalPassingPositionForTest();
    checks.expect(objectTemplate.portalPassingPositionSpanForTest() ==
                      3u * sizeof(float),
                  "portal-passing position occupies the complete three-float Vec3 span");
    checks.expect(position.x == 0.0f && position.y == 0.0f && position.z == 0.0f,
                  "SimpleObjectTemplate zero-initializes every portal-position component");
}

void testFactoriesAndWeakInvalidation(Checks& checks)
{
    FactorySimpleTemplate simpleTemplate;
    checks.expect(!simpleTemplate.specialMaterialLatch(),
                  "SimpleObjectTemplate latch starts clear");

    IObject* simple = simpleTemplate.createObject();
    checks.expect(simple != 0, "SimpleObjectTemplate factory returns an object");
    checks.expect(dynamic_cast<SimpleObject*>(simple) != 0,
                  "SimpleObjectTemplate factory constructs SimpleObject");
    checks.expect(dynamic_cast<Bundle*>(simple) == 0,
                  "SimpleObjectTemplate factory does not construct Bundle");
    checks.expect(simple->getTemplate() == static_cast<IObjectTemplate*>(&simpleTemplate),
                  "SimpleObject factory preserves template pointer");
    checks.expect(simpleTemplate.specialMaterialLatch(),
                  "SimpleObjectTemplate factory executes one-shot material gate");

    WeakPtr<IObject> weak = simple->getWeakPtr();
    checks.expect(weak.get() == simple, "IObject weak pointer initially resolves");
    checks.expect(destroyHeapObject(simple) == 0, "SimpleObject final release deletes object");
    checks.expect(weak.get() == 0, "IObject destruction invalidates weak pointer");

    IObject* secondSimple = simpleTemplate.createObject();
    checks.expect(simpleTemplate.specialMaterialLatch(),
                  "SimpleObjectTemplate material latch remains set on later factory call");
    checks.expect(destroyHeapObject(secondSimple) == 0,
                  "second SimpleObject factory result tears down cleanly");

    HarnessBundleTemplate bundleTemplate;
    IObject* bundle = bundleTemplate.createObject();
    checks.expect(bundle != 0, "BundleTemplate factory returns an object");
    checks.expect(dynamic_cast<Bundle*>(bundle) != 0,
                  "BundleTemplate factory constructs Bundle");
    checks.expect(bundle->getTemplate() == static_cast<IObjectTemplate*>(&bundleTemplate),
                  "Bundle factory preserves template pointer");
    checks.expect(destroyHeapObject(bundle) == 0, "Bundle factory result tears down cleanly");

    FactoryItemTemplate itemTemplate;
    IObject* itemContainer = itemTemplate.createObject();
    checks.expect(itemContainer != 0, "ItemContainerTemplate factory returns an object");
    checks.expect(dynamic_cast<Bundle*>(itemContainer) != 0,
                  "ItemContainerTemplate factory intentionally constructs Bundle");
    checks.expect(itemContainer->getTemplate() == static_cast<IObjectTemplate*>(&itemTemplate),
                  "ItemContainer factory preserves derived template pointer");
    checks.expect(destroyHeapObject(itemContainer) == 0,
                  "ItemContainer Bundle tears down cleanly");
}

void testObjectTemplateComponentOwnership(Checks& checks)
{
    const unsigned int componentId = 0x7F010206u;
    std::vector<std::string> events;
    FactorySimpleTemplate objectTemplate;
    TemplateComponentOwnerProbe first("firstTemplate", componentId, &events);
    TemplateComponentOwnerProbe second("secondTemplate", componentId, &events);

    checks.expect(static_cast<void*>(static_cast<dice::hfe::IBase*>(&first)) !=
                      static_cast<void*>(
                          static_cast<dice::hfe::world::IObjectTemplateComponent*>(
                              &first.m_component)),
                  "template component fixture exposes a distinct queried-interface address");

    objectTemplate.setComponent(componentId, static_cast<dice::hfe::IBase*>(&first));
    checks.expect(first.m_queryCalls == 1 &&
                      first.m_lastQuery ==
                          dice::hfe::world::IID_IObjectTemplateComponent,
                  "ObjectTemplate::setComponent queries IObjectTemplateComponent on the owner");
    checks.expect(first.m_component.m_setTemplateCalls == 1 &&
                      first.m_component.m_template ==
                          static_cast<IObjectTemplate*>(&objectTemplate) &&
                      first.m_component.m_componentSeenDuringSetTemplate == 0,
                  "first template component receives its owner before map insertion");
    checks.expect(objectTemplate.queryComponent(componentId) == &first.m_component,
                  "ObjectTemplate stores the queried component interface, not the owner pointer");
    checks.expect(first.m_state.ownerAddCalls == 1 &&
                      first.m_state.interfaceAddCalls == 0,
                  "ObjectTemplate retains through the original incoming owner facet");
    checks.expect(first.m_state.refs == 2,
                  "first attachment adds exactly one retained ownership reference");

    events.clear();
    objectTemplate.setComponent(componentId, static_cast<dice::hfe::IBase*>(&second));
    checks.expect(second.m_component.m_componentSeenDuringSetTemplate == &first.m_component,
                  "replacement calls setTemplate while the previous map entry is still stored");
    checks.expect(events == std::vector<std::string>{
                      "query:secondTemplate", "setTemplate:secondTemplate",
                      "release:firstTemplate:interface", "add:secondTemplate:owner"},
                  "replacement orders query, setTemplate, queried-interface release, then owner addRef");
    checks.expect(first.m_state.interfaceReleaseCalls == 1 &&
                      first.m_state.ownerReleaseCalls == 0 && first.m_state.refs == 1,
                  "replacement releases the stored queried interface against its shared count");
    checks.expect(second.m_queryCalls == 1 &&
                      second.m_lastQuery ==
                          dice::hfe::world::IID_IObjectTemplateComponent &&
                      second.m_state.ownerAddCalls == 1 &&
                      second.m_state.interfaceAddCalls == 0 && second.m_state.refs == 2,
                  "replacement leaves exactly one retained reference on the new shared owner");
    checks.expect(objectTemplate.queryComponent(componentId) == &second.m_component,
                  "replacement publishes the second queried interface");

    events.clear();
    objectTemplate.setComponent(componentId, 0);
    checks.expect(events == std::vector<std::string>{
                      "release:secondTemplate:interface"},
                  "null attachment erases the slot and releases through the stored interface");
    checks.expect(second.m_state.interfaceReleaseCalls == 1 &&
                      second.m_state.ownerReleaseCalls == 0 && second.m_state.refs == 1,
                  "null erase removes the single retained ownership reference");
    checks.expect(objectTemplate.queryComponent(componentId) == 0,
                  "null erase removes the template component map entry");
}

void testPreCacheLifecycle(Checks& checks)
{
    std::vector<std::string> events;
    ProbeTemplate objectTemplate(&events);

    checks.expect((objectTemplate.getFlags() & 0x20u) == 0,
                  "preCache template latch starts clear");
    objectTemplate.preCache();
    checks.expect(objectTemplate.m_createCalls == 1 && objectTemplate.m_created.size() == 1,
                  "preCache creates exactly one throwaway object");

    ProbeObject* object = objectTemplate.m_created.empty() ? 0 : objectTemplate.m_created[0];
    checks.expect(object && object->m_initCalls == 1,
                  "preCache initializes the throwaway object exactly once");
    checks.expect(object && object->isObjectDestroyed(),
                  "preCache force-destroys the initialized throwaway object");
    checks.expect((objectTemplate.getFlags() & 0x20u) != 0,
                  "Object::init sets the template preCache latch");

    objectTemplate.preCache();
    checks.expect(objectTemplate.m_createCalls == 1,
                  "preCache latch suppresses a second factory call");
    checks.expect(object && object->release() == 0,
                  "preCache throwaway object releases cleanly after forced destruction");
}

void testBundleChildAssembly(Checks& checks)
{
    std::vector<std::string> events;
    ProbeTemplate childTemplate(&events);
    Vec3 offset = {1.0f, 2.0f, 3.0f};
    childTemplate.setRelativePositionOffset(offset);
    // addBundleChilds selects the one-time placement branch from the child template's flag word.
    childTemplate.updateFlags(0x1000u, 0);

    HarnessBundleTemplate bundleTemplate;
    bundleTemplate.addTemplate("S_RuntimePairChild");
    bundleTemplate.cacheTemplate(0, childTemplate);
    Vec3 position = {10.0f, 20.0f, 30.0f};
    bundleTemplate.setPosition(position, 0);

    ProbeHost firstHost(&bundleTemplate, &events);
    bundleTemplate.addBundleChilds(&firstHost);

    checks.expect(childTemplate.m_createCalls == 1, "bundle creates one object per child entry");
    checks.expect(childTemplate.m_created.size() == 1, "first child is retained by probe");
    checks.expect(events.size() == 6, "bundle assembly emits six ordered operations");
    if (events.size() == 6)
    {
        checks.expect(events[0] == "createObject", "bundle assembly starts with createObject");
        checks.expect(events[1] == "setChildId", "bundle assigns ordinal before attachment");
        checks.expect(events[2] == "addChild", "bundle attaches after assigning ordinal");
        checks.expect(events[3] == "setRelativeTransformation",
                      "Object::addChild first applies its identity transform");
        checks.expect(events[4] == "setRelativeTransformation",
                      "bundle applies its authored relative transform after attachment");
        checks.expect(events[5] == "init", "bundle initializes child last");
    }
    checks.expect(firstHost.m_childIdSeenAtAdd == 0,
                  "host observes child ordinal already assigned during addChild");
    checks.expect(firstHost.getChild() == childTemplate.m_created[0],
                  "assembled child is linked into host hierarchy");

    ProbeObject* firstChild = childTemplate.m_created[0];
    checks.expect(firstChild->m_initCalls == 1, "assembled child initialized exactly once");
    checks.expect(firstChild->m_relativeCalls == 2,
                  "assembled child receives addChild identity then authored relative transform");
    checks.expect(near(firstChild->m_lastRelative.m[12], 11.0f) &&
                  near(firstChild->m_lastRelative.m[13], 22.0f) &&
                  near(firstChild->m_lastRelative.m[14], 33.0f),
                  "one-time SimpleObjectTemplate relative offset is applied");
    checks.expect(bundleTemplate.relativeOffsetApplied(0),
                  "one-time relative-offset latch is persisted in TemplateInfo");
    checks.expect(near(bundleTemplate.getTransformation(0).m[12], 11.0f) &&
                  near(bundleTemplate.getTransformation(0).m[13], 22.0f) &&
                  near(bundleTemplate.getTransformation(0).m[14], 33.0f),
                  "one-time relative offset mutates cached child transformation");
    checks.expect(bundleTemplate.cachedTarget(0) == &childTemplate,
                  "prefilled weak template cache remains the assembly source");

    events.clear();
    ProbeHost secondHost(&bundleTemplate, &events);
    bundleTemplate.addBundleChilds(&secondHost);
    checks.expect(childTemplate.m_createCalls == 2, "same bundle template can assemble again");
    checks.expect(childTemplate.m_created.size() == 2, "second assembly creates a second child");
    ProbeObject* secondChild = childTemplate.m_created[1];
    checks.expect(near(secondChild->m_lastRelative.m[12], 11.0f) &&
                  near(secondChild->m_lastRelative.m[13], 22.0f) &&
                  near(secondChild->m_lastRelative.m[14], 33.0f),
                  "relative offset is not accumulated on later assemblies");

    finishStackObject(firstHost);
    finishStackObject(secondHost);

    TemplateManagerLookupProbe managerProbe;
    dice::hfe::world::ObjectTemplateManager* savedManager =
        dice::hfe::world::objectTemplateManager;
    dice::hfe::world::objectTemplateManager =
        reinterpret_cast<dice::hfe::world::ObjectTemplateManager*>(&managerProbe);

    HarnessBundleTemplate missingTemplate;
    missingTemplate.addTemplate("RuntimePairMissingTemplate");
    ProbeHost missingHost(&missingTemplate, 0);
    missingTemplate.addBundleChilds(&missingHost);
    checks.expect(missingHost.getChild() == 0, "unresolved child template creates no object");
    checks.expect(missingTemplate.cachedTarget(0) == &missingTemplate,
                  "failed lookup stores owning-template negative sentinel");
    checks.expect(managerProbe.m_lookupCalls == 1,
                  "uncached missing template performs one manager lookup");
    missingTemplate.addBundleChilds(&missingHost);
    checks.expect(missingHost.getChild() == 0,
                  "negative template sentinel suppresses later child creation");
    checks.expect(managerProbe.m_lookupCalls == 1,
                  "live negative sentinel suppresses a repeated manager lookup");
    finishStackObject(missingHost);
    dice::hfe::world::objectTemplateManager = savedManager;

    std::vector<std::string> retryEvents;
    HarnessBundleTemplate staleTemplate;
    staleTemplate.addTemplate("RuntimePairStaleTemplate");
    {
        ProbeTemplate expiredTemplate(&retryEvents);
        staleTemplate.cacheTemplate(0, expiredTemplate);
    }
    ProbeTemplate replacementTemplate(&retryEvents);
    managerProbe.m_result = &replacementTemplate;
    dice::hfe::world::objectTemplateManager =
        reinterpret_cast<dice::hfe::world::ObjectTemplateManager*>(&managerProbe);
    ProbeHost retryHost(&staleTemplate, &retryEvents);
    staleTemplate.addBundleChilds(&retryHost);
    dice::hfe::world::objectTemplateManager = savedManager;
    checks.expect(managerProbe.m_lookupCalls == 2,
                  "expired weak cache retries through the template manager");
    checks.expect(replacementTemplate.m_createCalls == 1 && retryHost.getChild() != 0,
                  "stale-cache retry assembles the manager replacement template");
    checks.expect(staleTemplate.cachedTarget(0) == &replacementTemplate,
                  "successful stale-cache retry replaces the expired weak handle");
    managerProbe.m_result = 0;
    finishStackObject(retryHost);
}

void testBundleInputPropagation(Checks& checks)
{
    std::vector<std::string> events;
    ProbeTemplate childTemplate(&events);
    HarnessBundleTemplate bundleTemplate;

    bundleTemplate.addTemplate("RuntimePairInputChildA");
    bundleTemplate.addTemplate("RuntimePairInputChildB");
    bundleTemplate.cacheTemplate(0, childTemplate);
    bundleTemplate.cacheTemplate(1, childTemplate);
    bundleTemplate.setInputId(42, 0);

    IObject* object = bundleTemplate.createObject();
    Bundle* bundle = dynamic_cast<Bundle*>(object);
    checks.expect(bundle != 0, "input-propagation owner is a Bundle");
    bundleTemplate.addBundleChilds(object);
    checks.expect(childTemplate.m_created.size() == 2,
                  "input-propagation fixture assembles two children");

    object->setInputId(7);
    checks.expect(object->getInputId() == 7, "Bundle stores its own input id");
    if (childTemplate.m_created.size() == 2)
    {
        ProbeObject* first = childTemplate.m_created[0];
        ProbeObject* second = childTemplate.m_created[1];
        checks.expect(first->m_childIdCalls == 3 && second->m_childIdCalls == 3,
                      "Bundle preserves three virtual child-id reads per eligible child");
        checks.expect(first->getChildId() == 0 && second->getChildId() == 1,
                      "Bundle child ordinals match TemplateInfo indices");
        checks.expect(first->getInputId() == 42,
                      "explicit TemplateInfo input mapping overrides Bundle input id");
        checks.expect(second->getInputId() == 7,
                      "negative TemplateInfo input mapping inherits Bundle input id");
        checks.expect(first->m_inputCalls == 1 && second->m_inputCalls == 1,
                      "Bundle propagates input id exactly once to each child");
    }

    checks.expect(destroyHeapObject(object) == 0,
                  "assembled Bundle and children tear down cleanly");
}

void testDriverDecalRegistration(Checks& checks)
{
    std::vector<std::string> events;
    DecalTemplateProbe decalTemplate(&events);
    HarnessBundleTemplate bundleTemplate;
    bundleTemplate.enableCollisionPhysics();
    bundleTemplate.addTemplate("RuntimePairDriverDecal");
    bundleTemplate.cacheTemplate(0, decalTemplate);
    Vec3 position = {12.0f, 13.0f, 14.0f};
    bundleTemplate.setPosition(position, 0);

    PlayerControlProbe playerControl;
    DriverHost object(&bundleTemplate, &playerControl);
    bundleTemplate.addDriverDecals(&object);

    checks.expect(decalTemplate.m_classIdCalls == 2,
                  "driver-decal filtering preserves the redundant double class-id query");
    checks.expect(decalTemplate.m_createCalls == 0,
                  "driver-decal registration does not instantiate a decal runtime object");
    checks.expect(playerControl.m_setCalls == 1 &&
                      playerControl.m_driverObject == &object,
                  "driver-decal registration identifies the supplied object");
    checks.expect(playerControl.m_transformCalls == 1 &&
                      near(playerControl.m_lastTransformation.m[12], 12.0f) &&
                      near(playerControl.m_lastTransformation.m[13], 13.0f) &&
                      near(playerControl.m_lastTransformation.m[14], 14.0f),
                  "driver-decal registration appends the TemplateInfo transformation");

    finishStackObject(object);
}

void testBundleChildPartPropagation(Checks& checks)
{
    std::vector<std::string> events;
    ChildPartTemplateProbe bundleSimpleTemplate;
    ChildPartTemplateProbe rootTemplate;
    ChildPartTemplateProbe zeroTemplate;
    ChildPartTemplateProbe deepTemplate;
    ChildPartTemplateProbe blockedTemplate;
    ChildPartTemplateProbe blockedChildTemplate;
    ChildPartTemplateProbe laterTemplate;
    rootTemplate.setParts(2, 2);
    zeroTemplate.setParts(0, 0);
    deepTemplate.setParts(3, 3);
    blockedTemplate.setParts(4, 4);
    blockedChildTemplate.setParts(5, 5);
    laterTemplate.setParts(6, 6);

    ChildPartObjectProbe root(&rootTemplate, "root", &events, false);
    ChildPartObjectProbe zero(&zeroTemplate, "zero", &events, true);
    ChildPartObjectProbe deep(&deepTemplate, "deep", &events, true);
    ChildPartObjectProbe blocked(&blockedTemplate, "blocked", &events, true);
    ChildPartObjectProbe blockedChild(&blockedChildTemplate, "blockedChild", &events, true);
    ChildPartObjectProbe later(&laterTemplate, "later", &events, true);
    root.setChildForTest(&zero);
    zero.setSiblingForTest(&deep);
    root.setSiblingForTest(&blocked);
    blocked.setChildForTest(&blockedChild);
    blocked.setSiblingForTest(&later);

    root.setAbsoluteX(12.0f);
    zero.setAbsoluteX(20.0f);
    deep.setAbsoluteX(34.0f);
    blocked.setAbsoluteX(40.0f);
    blockedChild.setAbsoluteX(50.0f);
    later.setAbsoluteX(60.0f);

    int ownedGeometryMarker = 0;
    blocked.setOwnGeometryForTest(&ownedGeometryMarker);
    std::vector<GeometryPartCall> geometryCalls;
    GeometryPeAbiProbe geometry;
    geometry.m_calls = &geometryCalls;
    dice::hfe::world::setChildPartGeometries(
        &root, reinterpret_cast<dice::hfe::geom::IGeometry*>(&geometry));

    checks.expect(geometryCalls.size() == 2,
                  "geometry helper updates only positive reachable child parts");
    if (geometryCalls.size() == 2)
    {
        checks.expect(geometryCalls[0].part == 2 && geometryCalls[1].part == 3,
                      "geometry helper preserves depth-first part order");
        checks.expect(near(geometryCalls[0].transformation.m[12], 12.0f) &&
                          near(geometryCalls[1].transformation.m[12], 34.0f),
                      "geometry helper forwards each child's absolute transformation");
    }
    checks.expect(root.m_absoluteCalls == 1 && zero.m_absoluteCalls == 0 &&
                      deep.m_absoluteCalls == 1,
                  "geometry helper requests transforms only for strictly positive parts");
    checks.expect(rootTemplate.m_queryCalls == 1 && zeroTemplate.m_queryCalls == 1 &&
                      deepTemplate.m_queryCalls == 1,
                  "geometry helper queries every reachable child template implementation");
    checks.expect(blockedTemplate.m_queryCalls == 0 &&
                      blockedChildTemplate.m_queryCalls == 0 && laterTemplate.m_queryCalls == 0,
                  "owned child geometry terminates that subtree and later sibling walk");

    rootTemplate.resetQueryCalls();
    zeroTemplate.resetQueryCalls();
    deepTemplate.resetQueryCalls();
    blockedTemplate.resetQueryCalls();
    blockedChildTemplate.resetQueryCalls();
    laterTemplate.resetQueryCalls();
    int ownedCollisionMarker = 0;
    blocked.setOwnCollisionForTest(&ownedCollisionMarker);

    CollisionMeshPeAbiProbe mesh(&events);
    CollisionManagerPeAbiProbe manager(&events, &mesh);
    ICollisionManager* savedCollisionManager = dice::hfe::world::collisionManager;
    dice::hfe::world::collisionManager =
        reinterpret_cast<ICollisionManager*>(&manager);
    dice::hfe::world::setChildPartCollisionMeshes(
        static_cast<const IObjectTemplate*>(&bundleSimpleTemplate), &root,
        std::string("RuntimePairCollision"));
    dice::hfe::world::collisionManager = savedCollisionManager;

    checks.expect(manager.m_lookupCalls == 2 && manager.m_parts.size() == 2 &&
                      manager.m_parts[0] == 2 && manager.m_parts[1] == 3,
                  "collision helper requests only positive reachable parts in depth-first order");
    checks.expect(manager.m_names.size() == 2 &&
                      manager.m_names[0] == "RuntimePairCollision" &&
                      manager.m_names[1] == "RuntimePairCollision",
                  "collision helper forwards the root collision resource name unchanged");
    checks.expect(mesh.m_mappingCalls == 2 &&
                      mesh.m_lastTemplate ==
                          static_cast<const IObjectTemplate*>(&bundleSimpleTemplate),
                  "collision helper builds each mesh material map from the root simple template");
    checks.expect(root.m_setComponentCalls == 1 && deep.m_setComponentCalls == 1 &&
                      zero.m_setComponentCalls == 0,
                  "collision helper attaches exactly the meshes it successfully creates");
    checks.expect(root.m_lastComponentId == dice::hfe::world::IID_ICollisionMesh &&
                      deep.m_lastComponentId == dice::hfe::world::IID_ICollisionMesh &&
                      root.m_lastComponent == static_cast<dice::hfe::IBase*>(&mesh) &&
                      deep.m_lastComponent == static_cast<dice::hfe::IBase*>(&mesh),
                  "collision helper attaches the recovered IID and returned mesh pointer");
    checks.expect(mesh.m_releaseCalls == 2 && mesh.getRef() == 3,
                  "collision helper releases every temporary mesh even when setComponent fails");
    checks.expect(events == std::vector<std::string>({
                      "lookup:2", "mapping", "set:root", "release",
                      "lookup:3", "mapping", "set:deep", "release"}),
                  "collision helper preserves lookup-map-attach-release ordering");
    checks.expect(rootTemplate.m_queryCalls == 1 && zeroTemplate.m_queryCalls == 1 &&
                      deepTemplate.m_queryCalls == 1,
                  "collision helper queries each reachable template before testing its part");
    checks.expect(blockedTemplate.m_queryCalls == 0 &&
                      blockedChildTemplate.m_queryCalls == 0 && laterTemplate.m_queryCalls == 0,
                  "owned child collision terminates that subtree and later sibling walk");

    root.forceDestroy();
    zero.forceDestroy();
    deep.forceDestroy();
    blocked.forceDestroy();
    blockedChild.forceDestroy();
    later.forceDestroy();
}

void testBundleRuntimeWrappers(Checks& checks)
{
    HarnessBundleTemplate bundleTemplate;
    ChildPartTemplateProbe childTemplate;
    childTemplate.setParts(9, 0);
    std::vector<std::string> events;
    BundleRuntimeProbe bundle(&bundleTemplate);
    ChildPartObjectProbe child(&childTemplate, "wrapperChild", &events, true);
    child.setAbsoluteX(19.0f);
    bundle.addChild(&child);
    bundle.setCacheStateForTest(91u, 0x41008000u, 0x20u);
    child.setCacheStateForTest(91u, 0x41008000u, 0x20u);

    std::vector<GeometryPartCall> geometryCalls;
    GeometryPeAbiProbe geometry;
    geometry.m_calls = &geometryCalls;
    GameLogicPeAbiProbe cacheLogic(&events);
    dice::hfe::IGameLogic* savedCacheLogic = dice::hfe::gameLogic;
    dice::hfe::gameLogic = reinterpret_cast<dice::hfe::IGameLogic*>(&cacheLogic);
    runtime_pair_scaffold::enableCachedEvents = &events;
    bundle.setRawGeometryForTest(&geometry);
    bundle.enableCachedObject();
    runtime_pair_scaffold::enableCachedEvents = 0;
    dice::hfe::gameLogic = savedCacheLogic;

    checks.expect(geometryCalls.size() == 1 && geometryCalls[0].part == 9 &&
                      near(geometryCalls[0].transformation.m[12], 19.0f),
                  "Bundle enableCachedObject reapplies direct child-part geometry");
    checks.expect(!events.empty() && events.front() == "geometry:setPart" &&
                      (bundle.flagsForTest() & 0x20u) == 0 &&
                      (child.flagsForTest() & 0x20u) == 0,
                  "Bundle enableCachedObject reapplies geometry before the recovered SimpleObject/Object cache lifecycle");

    Vec3 position = {1.0f, 2.0f, 3.0f};
    Vec3 normal = {4.0f, 5.0f, 6.0f};
    Vec3 velocity = {7.0f, 8.0f, 9.0f};
    dice::hfe::world::CollisionFaceInfo* firstFace =
        reinterpret_cast<dice::hfe::world::CollisionFaceInfo*>(&bundle);
    dice::hfe::world::CollisionFaceInfo* secondFace =
        reinterpret_cast<dice::hfe::world::CollisionFaceInfo*>(&child);
    // The SimpleObject base is real code now, so what Bundle forwarded is observed through it: an
    // absorbing material makes the base answer false, and an armor on the bundle makes the base
    // report the collision to the game logic with the arguments Bundle handed down.
    CollisionMaterialPeAbiProbe absorbingMaterial(true);
    MaterialManagerPeAbiProbe collisionMaterials;
    collisionMaterials.m_material = &absorbingMaterial;
    ArmorPeAbiProbe collisionArmor(1.0f, 0);
    collisionArmor.m_collides = true;
    GameLogicCollisionPeAbiProbe collisionLogic;

    dice::hfe::IBase* savedCollisionMaterials = dice::hfe::world::materialManager;
    dice::hfe::IGameLogic* savedCollisionLogic = dice::hfe::gameLogic;
    dice::hfe::world::materialManager =
        reinterpret_cast<dice::hfe::IBase*>(&collisionMaterials);
    dice::hfe::gameLogic = reinterpret_cast<dice::hfe::IGameLogic*>(&collisionLogic);
    bundle.setRawArmorForTest(&collisionArmor);

    const bool collisionResult = bundle.handleCollision(
        &child, position, normal, velocity, 27, 3, firstFace, secondFace);

    bundle.setRawArmorForTest(0);
    dice::hfe::gameLogic = savedCollisionLogic;
    dice::hfe::world::materialManager = savedCollisionMaterials;

    checks.expect(!collisionResult && collisionMaterials.m_lastId == 3 &&
                      collisionArmor.m_collideCalls == 1 &&
                      collisionArmor.m_lastCollideFirst == 27 &&
                      collisionArmor.m_lastCollideMaterial == 3,
                  "Bundle handleCollision preserves the base result and first six arguments");
    checks.expect(collisionLogic.m_collisionCalls == 1 &&
                      near(collisionLogic.m_lastCollision.firstVector.x, -1.0f) &&
                      near(collisionLogic.m_lastCollision.firstVector.y, -2.0f) &&
                      near(collisionLogic.m_lastCollision.firstVector.z, -3.0f) &&
                      near(collisionLogic.m_lastCollision.secondVector.x, 4.0f) &&
                      near(collisionLogic.m_lastCollision.thirdVector.x, 7.0f) &&
                      collisionLogic.m_lastCollision.firstFace == 0 &&
                      collisionLogic.m_lastCollision.secondFace == 0,
                  "Bundle handleCollision forwards vectors and forces both face pointers null");

    bundle.setRawGeometryForTest(0);
    bundle.removeChild(&child);
    child.setObjectCacheID(0);
    bundle.setObjectCacheID(0);
    finishStackObject(child);
    finishStackObject(bundle);
}

void testObjectComponentOwnershipAndMaps(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);
    FastInterfaceProbe alternate(kAlternateComponentIID, 0);
    std::vector<std::string> events;

    ComponentInterfaceProbe firstInterface("first", &events, &alternate);
    IncomingOwnerProbe firstOwner("first", &events, &firstInterface,
                                  kGeneralObjectComponentID, 0, &alternate);
    checks.expect(object.setComponent(kGeneralObjectComponentID, &firstOwner),
                  "Object::setComponent accepts an IObjectComponent-backed owner");
    checks.expect(firstInterface.m_object == &object &&
                      firstInterface.m_setObjectCalls == 1 &&
                      firstInterface.m_nullSetObjectCalls == 0,
                  "Object::setComponent calls setObject(this) on the queried interface");
    checks.expect(firstOwner.m_addCalls == 1 && firstInterface.m_addCalls == 0,
                  "component attach retains the original IBase, not the queried interface");
    checks.expect(object.objectComponentCount() == 1 &&
                      object.baseComponentCount() == 0 &&
                      object.hasObjectComponent(kGeneralObjectComponentID),
                  "IObjectComponent-capable input is stored only in the component map");
    checks.expect(events == std::vector<std::string>({
                      std::string("query:first:") +
                          std::to_string(dice::hfe::world::IID_IObjectComponent),
                      "setObject:first", "add:first:owner"}),
                  "component attach preserves query-setObject-retain ordering");

    const int firstOwnerAddsBeforeQuery = firstOwner.m_addCalls;
    const int firstInterfaceAddsBeforeQuery = firstInterface.m_addCalls;
    checks.expect(object.queryComponent(kGeneralObjectComponentID,
                                        kAlternateComponentIID) == &alternate &&
                      firstInterface.m_queryCalls == 1 &&
                      firstOwner.m_addCalls == firstOwnerAddsBeforeQuery &&
                      firstInterface.m_addCalls == firstInterfaceAddsBeforeQuery,
                  "component-map query forwards the IID without retaining the result");

    events.clear();
    ComponentInterfaceProbe secondInterface("second", &events, &alternate);
    IncomingOwnerProbe secondOwner("second", &events, &secondInterface,
                                   kGeneralObjectComponentID, 0, &alternate);
    checks.expect(object.setComponent(kGeneralObjectComponentID, &secondOwner),
                  "Object::setComponent accepts component replacement");
    checks.expect(events == std::vector<std::string>({
                      std::string("query:second:") +
                          std::to_string(dice::hfe::world::IID_IObjectComponent),
                      "setObject:second", "release:first:interface",
                      "add:second:owner"}),
                  "component replacement sets the new owner before old release and new retain");
    checks.expect(firstInterface.m_releaseCalls == 1 &&
                      secondOwner.m_addCalls == 1 && secondInterface.m_addCalls == 0 &&
                      firstInterface.m_nullSetObjectCalls == 0,
                  "component replacement releases only the stored interface and never clears its owner");

    events.clear();
    checks.expect(object.setComponent(kGeneralObjectComponentID, 0),
                  "null component removes a component-map entry and returns true");
    checks.expect(events == std::vector<std::string>({"release:second:interface"}) &&
                      object.objectComponentCount() == 0 &&
                      secondInterface.m_nullSetObjectCalls == 0,
                  "null removal releases the stored interface without setObject(nullptr)");

    IncomingOwnerProbe firstBase("baseA", &events, 0, kGeneralBaseComponentID, 0, 0);
    IncomingOwnerProbe secondBase("baseB", &events, 0, kGeneralBaseComponentID, 0, 0);
    object.setComponent(kGeneralBaseComponentID, &firstBase);
    events.clear();
    object.setComponent(kGeneralBaseComponentID, &secondBase);
    checks.expect(events == std::vector<std::string>({
                      std::string("query:baseB:") +
                          std::to_string(dice::hfe::world::IID_IObjectComponent),
                      "release:baseA:owner", "add:baseB:owner"}),
                  "base-map replacement releases the old value before retaining the new value");

    ComponentInterfaceProbe shadowInterface("shadow", &events, &alternate);
    IncomingOwnerProbe shadowOwner("shadow", &events, &shadowInterface,
                                   kGeneralBaseComponentID, 0, &alternate);
    events.clear();
    object.setComponent(kGeneralBaseComponentID, &shadowOwner);
    checks.expect(object.hasBaseComponent(kGeneralBaseComponentID) &&
                      object.hasObjectComponent(kGeneralBaseComponentID),
                  "non-null attach leaves a same-key entry in the opposite map intact");

    secondBase.resetActivity();
    const int shadowQueriesBefore = shadowInterface.m_queryCalls;
    checks.expect(object.queryComponent(kGeneralBaseComponentID,
                                        kAlternateComponentIID) == 0 &&
                      secondBase.m_queryCalls == 1 &&
                      shadowInterface.m_queryCalls == shadowQueriesBefore,
                  "base map wins and a null base query result does not fall through to components");

    events.clear();
    checks.expect(object.setComponent(kGeneralBaseComponentID, 0),
                  "null component removes same-key entries from both maps");
    checks.expect(events == std::vector<std::string>({
                      "release:baseB:owner", "release:shadow:interface"}) &&
                      object.baseComponentCount() == 0 &&
                      object.objectComponentCount() == 0,
                  "null removal erases base map before component map");
    checks.expect(object.setComponent(kGeneralBaseComponentID, 0),
                  "removing an absent component remains an always-true operation");

    finishStackObject(object);
}

void testObjectFastComponentDispatch(Checks& checks)
{
    const unsigned int ids[] = {
        dice::hfe::world::IID_IPhysicsNode,
        dice::hfe::world::IID_IResponsePhysics,
        dice::hfe::geom::IID_IGeometry,
        dice::hfe::world::IID_ICollisionMesh,
        dice::hfe::io::IID_INetworkable,
        dice::hfe::ai::IID_IAIObject,
        dice::hfe::world::IID_IArmorGLComp,
        dice::hfe::world::IID_IPostProcessComp,
        dice::hfe::world::IID_IVehicleHudComp,
        dice::hfe::world::IID_IHelpHudComp,
        dice::hfe::world::IID_IWeaponHudComp,
        dice::hfe::world::IID_IAbilityHudComp,
        dice::hfe::world::IID_IAbilityGLComp,
        dice::hfe::world::IID_IRadioGLComp,
        dice::hfe::world::IID_IWarningHudComp,
        dice::hfe::world::IID_IWindAffector,
        dice::hfe::world::IID_IStrategicObjectComp
    };

    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);
    FastInterfaceProbe alternate(kAlternateComponentIID, 0);
    bool allMapped = true;
    bool allDirect = true;
    bool allForwarded = true;
    bool allUnretained = true;
    bool allRemoved = true;
    bool allOwned = true;

    for (unsigned int i = 0; i != sizeof(ids) / sizeof(ids[0]); ++i)
    {
        const unsigned int id = ids[i];
        if (id == dice::hfe::world::IID_IResponsePhysics)
        {
            ResponsePhysicsPeAbiProbe response(&alternate);
            IncomingOwnerProbe owner("fastResponse", 0, 0, id, &response, &alternate);
            object.setComponent(id, &owner);
            allMapped = allMapped && object.fastField(id) == &response;
            allDirect = allDirect && object.queryComponent(id, id) ==
                                         reinterpret_cast<dice::hfe::IBase*>(&response);
            const int ownerAdds = owner.m_addCalls;
            const int responseAdds = response.m_addCalls;
            allForwarded = allForwarded &&
                object.queryComponent(id, kAlternateComponentIID) == &alternate;
            allUnretained = allUnretained && owner.m_addCalls == ownerAdds &&
                            response.m_addCalls == responseAdds;
            object.setComponent(id, 0);
            allRemoved = allRemoved && object.fastField(id) == 0;
            allOwned = allOwned && owner.m_addCalls == 1 && owner.m_releaseCalls == 1;
        }
        else
        {
            FastInterfaceProbe fast(id, &alternate);
            IncomingOwnerProbe owner("fast", 0, 0, id, &fast, &alternate);
            object.setComponent(id, &owner);
            allMapped = allMapped && object.fastField(id) == &fast;
            allDirect = allDirect && object.queryComponent(id, id) == &fast;
            const int ownerAdds = owner.m_addCalls;
            const int fastAdds = fast.m_addCalls;
            allForwarded = allForwarded &&
                object.queryComponent(id, kAlternateComponentIID) == &alternate &&
                fast.m_lastQuery == kAlternateComponentIID;
            allUnretained = allUnretained && owner.m_addCalls == ownerAdds &&
                            fast.m_addCalls == fastAdds;
            object.setComponent(id, 0);
            allRemoved = allRemoved && object.fastField(id) == 0;
            allOwned = allOwned && owner.m_addCalls == 1 && owner.m_releaseCalls == 1;
        }
    }

    checks.expect(allMapped,
                  "Object::setComponent populates all seventeen recovered fast fields");
    checks.expect(allDirect,
                  "queryComponent returns every fast field directly for an identical IID");
    checks.expect(allForwarded,
                  "queryComponent forwards alternate IIDs through every fast interface");
    checks.expect(allUnretained,
                  "fast direct and forwarded queries never retain their results");
    checks.expect(allRemoved,
                  "null component clears all seventeen recovered fast fields");
    checks.expect(allOwned && object.baseComponentCount() == 0,
                  "each fast-field attach retains once and null removal releases once");

    IncomingOwnerProbe nullFast("nullFast", 0, 0, 0, 0, &alternate);
    object.setComponent(dice::hfe::geom::IID_IGeometry, &nullFast);
    checks.expect(object.fastField(dice::hfe::geom::IID_IGeometry) == 0 &&
                      object.queryComponent(dice::hfe::geom::IID_IGeometry,
                                            kAlternateComponentIID) == &alternate,
                  "a null fast field falls through to the general map lookup");
    object.setComponent(dice::hfe::geom::IID_IGeometry, 0);

    finishStackObject(object);
}

void testObjectComponentDirtySideEffects(Checks& checks)
{
    FactorySimpleTemplate parentTemplate;
    FactorySimpleTemplate childTemplate;
    ObjectComponentRuntimeProbe parent(&parentTemplate);
    ObjectComponentRuntimeProbe child(&childTemplate);
    parent.addChild(&child);

    FastInterfaceProbe geometry(dice::hfe::geom::IID_IGeometry, 0);
    IncomingOwnerProbe geometryOwner("geometry", 0, 0,
                                     dice::hfe::geom::IID_IGeometry, &geometry, 0);
    parent.clearBoundDirtyFlags();
    child.clearBoundDirtyFlags();
    child.setComponent(dice::hfe::geom::IID_IGeometry, &geometryOwner);
    checks.expect(child.boundDirtyFlags() == 0x20140000u &&
                      parent.boundDirtyFlags() == 0x20140000u,
                  "geometry attach dirties bounding caches through the ancestor chain");
    parent.clearBoundDirtyFlags();
    child.clearBoundDirtyFlags();
    child.setComponent(dice::hfe::geom::IID_IGeometry, 0);
    checks.expect(child.boundDirtyFlags() == 0x20140000u &&
                      parent.boundDirtyFlags() == 0x20140000u,
                  "geometry detach dirties bounding caches after clearing the field");

    FastInterfaceProbe collision(dice::hfe::world::IID_ICollisionMesh, 0);
    IncomingOwnerProbe collisionOwner("collision", 0, 0,
                                      dice::hfe::world::IID_ICollisionMesh,
                                      &collision, 0);
    parent.clearBoundDirtyFlags();
    child.clearBoundDirtyFlags();
    child.setComponent(dice::hfe::world::IID_ICollisionMesh, &collisionOwner);
    checks.expect(child.boundDirtyFlags() == 0x20140000u &&
                      parent.boundDirtyFlags() == 0x20140000u,
                  "collision attach dirties bounding caches through the ancestor chain");
    parent.clearBoundDirtyFlags();
    child.clearBoundDirtyFlags();
    child.setComponent(dice::hfe::world::IID_ICollisionMesh, 0);
    checks.expect(child.boundDirtyFlags() == 0x20140000u &&
                      parent.boundDirtyFlags() == 0x20140000u,
                  "collision detach dirties bounding caches after clearing the field");

    ResponsePhysicsPeAbiProbe rootResponse;
    IncomingOwnerProbe rootResponseOwner("rootResponse", 0, 0,
                                         dice::hfe::world::IID_IResponsePhysics,
                                         &rootResponse, 0);
    parent.setComponent(dice::hfe::world::IID_IResponsePhysics, &rootResponseOwner);
    checks.expect(rootResponse.m_dirtyCalls == 1,
                  "response attach stores the root response before marking it dirty");

    ResponsePhysicsPeAbiProbe childResponse;
    IncomingOwnerProbe childResponseOwner("childResponse", 0, 0,
                                          dice::hfe::world::IID_IResponsePhysics,
                                          &childResponse, 0);
    child.setComponent(dice::hfe::world::IID_IResponsePhysics, &childResponseOwner);
    checks.expect(rootResponse.m_dirtyCalls == 2 && childResponse.m_dirtyCalls == 0 &&
                      child.cachedRootParentForTest() == &parent,
                  "child response attach resolves the cached root and dirties the root response");
    child.setComponent(dice::hfe::world::IID_IResponsePhysics, 0);
    checks.expect(rootResponse.m_dirtyCalls == 3 &&
                      child.fastField(dice::hfe::world::IID_IResponsePhysics) == 0,
                  "response detach dirties through the old field before clearing it");
    child.setComponent(dice::hfe::world::IID_IResponsePhysics, 0);
    checks.expect(rootResponse.m_dirtyCalls == 3,
                  "detaching an already-null response performs no dirty callback");
    parent.setComponent(dice::hfe::world::IID_IResponsePhysics, 0);
    checks.expect(rootResponse.m_dirtyCalls == 4,
                  "root response detach marks the still-installed response before removal");

    parent.removeChild(&child);
    finishStackObject(child);
    finishStackObject(parent);
}

void testObjectLightSourceMaskForwarding(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);

    object.setLightSourceMask(0x25);
    checks.expect(object.getLightSourceMask() == 7,
                  "Object light mask uses the recovered fallback without geometry");

    std::vector<GeometryPartCall> unusedPartCalls;
    GeometryPeAbiProbe geometry;
    geometry.m_calls = &unusedPartCalls;
    object.setRawGeometryForTest(&geometry);
    object.setLightSourceMask(0x25);
    checks.expect(geometry.m_lightSetCalls == 1 && geometry.m_lightMask == 0x25,
                  "Object forwards the light mask to PE geometry slot +0xE0");
    checks.expect(object.getLightSourceMask() == 0x25,
                  "Object reads the light mask from PE geometry slot +0xE4");

    object.setRawGeometryForTest(0);
    finishStackObject(object);
}

void testObjectActiveLodForwarding(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);

    object.clearBoundDirtyFlags();
    object.SetActiveLod(static_cast<dice::hfe::world::ObjectLodType>(1));
    checks.expect(object.boundDirtyFlags() == 0x20140000u,
                  "SetActiveLod dirties bounds even when no geometry is installed");

    std::vector<GeometryPartCall> unusedPartCalls;
    GeometryPeAbiProbe geometry;
    geometry.m_calls = &unusedPartCalls;
    object.setRawGeometryForTest(&geometry);

    geometry.m_lodCount = 1;
    object.SetActiveLod(static_cast<dice::hfe::world::ObjectLodType>(1));
    checks.expect(geometry.m_lodCountCalls == 1 && geometry.m_setLodCalls == 0 &&
                      (object.flagsForTest() & 1u) != 0,
                  "single-LOD geometry sets Object flag 1 for active LOD type 1");
    object.SetActiveLod(static_cast<dice::hfe::world::ObjectLodType>(0));
    checks.expect(geometry.m_lodCountCalls == 2 && geometry.m_setLodCalls == 0 &&
                      (object.flagsForTest() & 1u) == 0,
                  "single-LOD geometry clears Object flag 1 for other LOD types");

    geometry.m_lodCount = 4;
    object.SetActiveLod(static_cast<dice::hfe::world::ObjectLodType>(1));
    checks.expect(geometry.m_lodCountCalls == 4 && geometry.m_setLodCalls == 1 &&
                      geometry.m_lastLod == 3,
                  "multi-LOD type 1 re-reads the count and selects count minus one");
    object.SetActiveLod(static_cast<dice::hfe::world::ObjectLodType>(0));
    checks.expect(geometry.m_lodCountCalls == 6 && geometry.m_setLodCalls == 2 &&
                      geometry.m_lastLod == 2,
                  "multi-LOD alternate type selects count minus two");

    object.setRawGeometryForTest(0);
    finishStackObject(object);
}

void testObjectGeometryUpdateChain(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);
    std::vector<GeometryPartCall> unusedPartCalls;
    GeometryPeAbiProbe first;
    GeometryPeAbiProbe second;
    GeometryPeAbiProbe third;
    first.m_calls = second.m_calls = third.m_calls = &unusedPartCalls;
    first.m_next = &second;
    second.m_next = &third;
    object.setRawGeometryForTest(&first);
    object.clearGeometryDirtyForTest();

    object.updateGeometry();
    checks.expect(first.m_transformCalls == 0 && second.m_transformCalls == 0 &&
                      third.m_transformCalls == 0,
                  "updateGeometry skips a clean geometry chain");

    Mat4 transformation = Mat4::getIdentity();
    transformation.m[12] = 17.0f;
    transformation.m[13] = 23.0f;
    object.setAbsoluteTransformation(transformation);
    object.updateGeometry();
    checks.expect(first.m_transformCalls == 1 && second.m_transformCalls == 1 &&
                      third.m_transformCalls == 1,
                  "updateGeometry pushes the visual transform through the complete chain");
    checks.expect(near(first.m_lastTransformation.m[12], 17.0f) &&
                      near(second.m_lastTransformation.m[13], 23.0f) &&
                      near(third.m_lastTransformation.m[12], 17.0f),
                  "every geometry node receives the current visual absolute transform");
    checks.expect((object.flagsForTest() & 0x40000000u) == 0,
                  "updateGeometry clears the geometry-dirty bit after the primary update");

    object.updateGeometry();
    checks.expect(first.m_transformCalls == 1 && second.m_transformCalls == 1 &&
                      third.m_transformCalls == 1,
                  "a second clean update does not traverse the geometry chain");

    object.setRawGeometryForTest(0);
    finishStackObject(object);
}

void testObjectCollisionLodForwarding(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);
    checks.expect(!object.hasCollisionLod(3),
                  "IObject reports no collision LOD without a collision mesh");

    std::vector<std::string> events;
    CollisionMeshPeAbiProbe mesh(&events);
    mesh.m_availableLod = 3;
    object.setComponent(dice::hfe::world::IID_ICollisionMesh, &mesh);
    checks.expect(object.hasCollisionLod(3) && !object.hasCollisionLod(2),
                  "IObject forwards collision LOD queries to PE mesh slot +0x28");
    checks.expect(mesh.m_collisionLodCalls == 2 && mesh.m_lastCollisionLod == 2,
                  "collision LOD forwarding preserves the argument and exact call count");

    object.setComponent(dice::hfe::world::IID_ICollisionMesh, 0);
    finishStackObject(object);
}

void testObjectBoundingRadii(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    std::vector<std::string> events;
    RadiusObjectProbe object(&objectTemplate, "object", &events);
    CollisionMeshPeAbiProbe mesh(&events);
    GeometryPeAbiProbe geometry;
    GeometryTemplatePeAbiProbe geometryTemplate(&events);
    geometry.m_radiusEvents = &events;
    geometry.m_geometryTemplate = &geometryTemplate;

    object.setRawCollisionForTest(&mesh);
    object.setCachedRadiiForTest(8.0f, 9.0f);
    object.setRadiusFlagsForTest(1u, 0x20140000u);
    checks.expect(near(object.Object::getVisualBoundingRadius(), 0.0f) &&
                      near(object.cachedVisualRadiusForTest(), 0.0f) &&
                      (object.flagsForTest() & 0x00040000u) != 0 &&
                      mesh.m_radiusCalls == 0 && object.m_modifierCalls == 0,
                  "disabled visual radius zeros its cache, marks dirty, and bypasses providers");
    checks.expect(near(object.Object::getPhysicBoundingRadius(), 0.0f) &&
                      near(object.cachedPhysicRadiusForTest(), 0.0f) &&
                      (object.flagsForTest() & 0x20000000u) != 0 &&
                      mesh.m_radiusCalls == 0 && object.m_modifierCalls == 0,
                  "disabled physical radius zeros its cache, marks dirty, and bypasses providers");

    object.setCachedRadiiForTest(4.0f, 5.0f);
    object.setRadiusFlagsForTest(0, 0x20140001u);
    checks.expect(near(object.Object::getVisualBoundingRadius(), 4.0f) &&
                      near(object.Object::getPhysicBoundingRadius(), 5.0f) &&
                      mesh.m_radiusCalls == 0 && object.m_modifierCalls == 0,
                  "clean radius caches bypass collision and modifier dispatch");

    events.clear();
    mesh.m_radius = 6.0f;
    object.m_modifier = 1.5f;
    object.setRadiusFlagsForTest(0x00040000u, 0x00100001u);
    checks.expect(near(object.Object::getVisualBoundingRadius(), 7.5f) &&
                      events == std::vector<std::string>({"mesh:radius", "object:modifier"}),
                  "visual recompute calls PE collision slot +0x74 before the modifier");
    checks.expect((object.flagsForTest() & 0x00040000u) == 0 &&
                      (object.flagsForTest() & 0x00100000u) != 0,
                  "changed visual radius stores its value, clears dirty, and marks bounds changed");

    object.setRawCollisionForTest(0);
    object.setRawGeometryForTest(&geometry);
    object.setCachedRadiiForTest(3.0f, object.cachedPhysicRadiusForTest());
    object.m_modifier = 2.0f;
    object.resetRadiusCalls();
    events.clear();
    geometryTemplate.m_hasRadius = false;
    object.setRadiusFlagsForTest(0x00040000u, 0x00100001u);
    checks.expect(near(object.Object::getVisualBoundingRadius(), 0.0f) &&
                      near(object.cachedVisualRadiusForTest(), 3.0f) &&
                      (object.flagsForTest() & 0x00040000u) != 0,
                  "geometry without a radius returns zero early while retaining cache and dirty bit");
    checks.expect(events == std::vector<std::string>({"geometry:template", "template:hasRadius"}) &&
                      geometry.m_radiusCalls == 0 && object.m_modifierCalls == 0,
                  "geometry radius gate uses PE slots +0x38/+0x28 and skips +0x44 and modifier");

    geometryTemplate.m_hasRadius = true;
    geometry.m_radius = 4.0f;
    object.m_modifier = 0.5f;
    events.clear();
    checks.expect(near(object.Object::getVisualBoundingRadius(), 4.5f) &&
                      events == std::vector<std::string>({
                          "geometry:template", "template:hasRadius",
                          "geometry:radius", "object:modifier"}),
                  "accepted geometry radius uses PE slot +0x44 before the modifier");

    object.setRawGeometryForTest(0);
    FactorySimpleTemplate childObjectTemplate;
    RadiusObjectProbe first(&childObjectTemplate, "first", &events);
    RadiusObjectProbe second(&childObjectTemplate, "second", &events);
    RadiusObjectProbe light(&childObjectTemplate, "light", &events);
    light.m_classId = dice::hfe::world::CID_LightSource;
    object.addChild(&first);
    object.addChild(&second);
    object.addChild(&light);
    Mat4 relative = Mat4::getIdentity();
    relative.m[12] = 3.0f;
    first.setRelativeTransformation(relative);
    relative.m[12] = 6.0f;
    second.setRelativeTransformation(relative);
    relative.m[12] = 2.0f;
    light.setRelativeTransformation(relative);
    first.setCachedRadiiForTest(2.0f, 1.0f);
    second.setCachedRadiiForTest(4.0f, 2.0f);
    light.setCachedRadiiForTest(100.0f, 20.0f);
    first.setRadiusFlagsForTest(0, 0x20040001u);
    second.setRadiusFlagsForTest(0, 0x20040001u);
    light.setRadiusFlagsForTest(0, 0x20040001u);
    object.m_modifier = 1.0f;
    object.setCachedRadiiForTest(0.0f, 0.0f);
    object.resetRadiusCalls();
    first.resetRadiusCalls();
    second.resetRadiusCalls();
    light.resetRadiusCalls();
    events.clear();
    object.setRadiusFlagsForTest(0x00040000u, 0x00100001u);
    checks.expect(near(object.Object::getVisualBoundingRadius(), 10.0f) &&
                      events == std::vector<std::string>({
                          "object:modifier", "first:class", "first:visual",
                          "second:class", "second:visual", "light:class"}),
                  "visual radius visits siblings in order and filters a LightSource before radius dispatch");
    checks.expect(first.m_visualCalls == 1 && second.m_visualCalls == 1 &&
                      light.m_visualCalls == 0,
                  "visual child merge uses distance plus positive radius and never queries light radius");

    object.resetRadiusCalls();
    first.resetRadiusCalls();
    second.resetRadiusCalls();
    light.resetRadiusCalls();
    events.clear();
    object.setRadiusFlagsForTest(0x20000000u, 0x00100001u);
    checks.expect(near(object.Object::getPhysicBoundingRadius(), 22.0f) &&
                      events == std::vector<std::string>({
                          "object:modifier", "first:physic", "second:physic", "light:physic"}),
                  "physical radius includes LightSource children without class-ID filtering");

    object.removeChild(&first);
    object.removeChild(&second);
    object.removeChild(&light);
    finishStackObject(first);
    finishStackObject(second);
    finishStackObject(light);

    object.setRawCollisionForTest(&mesh);
    object.m_modifier = 0.0f;
    object.setCachedRadiiForTest(10.0f, object.cachedPhysicRadiusForTest());
    mesh.m_radius = 10.00005f;
    object.setRadiusFlagsForTest(0x00040000u, 0x00100001u);
    checks.expect(near(object.Object::getVisualBoundingRadius(), 10.0f) &&
                      near(object.cachedVisualRadiusForTest(), 10.0f) &&
                      (object.flagsForTest() & 0x00100000u) == 0,
                  "visual epsilon retains the old cache without setting bounds-changed");
    mesh.m_radius = 10.001f;
    object.setRadiusFlagsForTest(0x00040000u, 0x00100000u);
    checks.expect(near(object.Object::getVisualBoundingRadius(), 10.001f) &&
                      near(object.cachedVisualRadiusForTest(), 10.001f) &&
                      (object.flagsForTest() & 0x00100000u) != 0,
                  "visual delta above epsilon stores the new cache and sets bounds-changed");

    object.setRawCollisionForTest(0);
    finishStackObject(object);
}

void testObjectBoundingBoxOwnAndCache(Checks& checks)
{
    LocalBoundingBox empty;
    checks.expect(sizeof(empty) == 0x3Cu && empty.m_derivedDataDirty &&
                      nearVec3(empty.m_min, 0.0f, 0.0f, 0.0f) &&
                      nearVec3(empty.m_max, 0.0f, 0.0f, 0.0f) &&
                      near(empty.m_radiusFromOrigin, 0.0f) &&
                      near(empty.m_radiusFromCentre, 0.0f) &&
                      nearVec3(empty.m_size, 0.0f, 0.0f, 0.0f) &&
                      nearVec3(empty.m_centre, 0.0f, 0.0f, 0.0f),
                  "LocalBoundingBox has the exact 0x3C zero-and-dirty initial state");

    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe noMesh(&objectTemplate);
    LocalBoundingBox* noMeshBox = noMesh.getFullBoundingBox();
    checks.expect(noMeshBox && noMeshBox->m_derivedDataDirty &&
                      nearVec3(noMeshBox->m_min, 0.0f, 0.0f, 0.0f) &&
                      nearVec3(noMeshBox->m_max, 0.0f, 0.0f, 0.0f) &&
                      noMesh.getFullBoundingBox() == noMeshBox,
                  "no-mesh full getter returns and caches a zero dirty box");
    finishStackObject(noMesh);

    std::vector<std::string> events;
    ObjectComponentRuntimeProbe rejected(&objectTemplate);
    CollisionMeshPeAbiProbe rejectedMesh(&events);
    rejectedMesh.setBounds(-9.0f, -8.0f, -7.0f, 6.0f, 5.0f, 4.0f);
    rejectedMesh.m_boundsResult = false;
    rejected.setRawCollisionForTest(&rejectedMesh);
    LocalBoundingBox* rejectedBox = rejected.getFullBoundingBox();
    checks.expect(rejectedBox && rejectedBox->m_derivedDataDirty &&
                      nearVec3(rejectedBox->m_min, 0.0f, 0.0f, 0.0f) &&
                      nearVec3(rejectedBox->m_max, 0.0f, 0.0f, 0.0f) &&
                      rejected.getFullBoundingBox() == rejectedBox &&
                      rejectedMesh.m_boundsCalls == 1,
                  "false collision bounds result leaves and caches the zero box after one call");
    rejected.setRawCollisionForTest(0);
    finishStackObject(rejected);

    ObjectComponentRuntimeProbe object(&objectTemplate);
    CollisionMeshPeAbiProbe mesh(&events);
    mesh.setBounds(-2.0f, -3.0f, -4.0f, 5.0f, 6.0f, 7.0f);
    object.setRawCollisionForTest(&mesh);

    LocalBoundingBox* full = object.getFullBoundingBox();
    checks.expect(full && nearVec3(full->m_min, -2.0f, -3.0f, -4.0f) &&
                      nearVec3(full->m_max, 5.0f, 6.0f, 7.0f) &&
                      full->m_derivedDataDirty,
                  "full bounds store raw collision endpoints and leave derived data dirty");
    checks.expect(mesh.m_boundsCalls == 1 && mesh.m_boundsInputsWereZero,
                  "full bounds use PE collision slot +0x78 with zeroed endpoint inputs");
    checks.expect(object.getFullBoundingBox() == full && mesh.m_boundsCalls == 1,
                  "full bounding box is pointer-cached after its first build");

    LocalBoundingBox* mine = object.getMineBoundingBox();
    checks.expect(mine && mine != full &&
                      nearVec3(mine->m_min, -2.0f, -3.0f, -4.0f) &&
                      nearVec3(mine->m_max, 5.0f, 6.0f, 7.0f) &&
                      mesh.m_boundsCalls == 2,
                  "root mine bounds include own collision and use a distinct cache");
    checks.expect(object.getMineBoundingBox() == mine && mesh.m_boundsCalls == 2,
                  "mine bounding box is pointer-cached after its first build");

    object.setRawCollisionForTest(0);
    finishStackObject(object);
}

void testObjectFullBoundingBoxHierarchy(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    FactorySimpleTemplate childTemplate;
    std::vector<std::string> events;
    ObjectComponentRuntimeProbe root(&objectTemplate);
    BoundingGetterProbe first(&childTemplate, "first", &events);
    BoundingGetterProbe second(&childTemplate, "second", &events);
    CollisionMeshPeAbiProbe rootMesh(&events);
    CollisionMeshPeAbiProbe firstMesh(&events);
    CollisionMeshPeAbiProbe secondMesh(&events);
    rootMesh.setBounds(-5.0f, -5.0f, -5.0f, 0.0f, 0.0f, 0.0f);
    firstMesh.setBounds(1.0f, 2.0f, 3.0f, 2.0f, 3.0f, 4.0f);
    secondMesh.setBounds(-2.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f);
    root.setRawCollisionForTest(&rootMesh);
    first.setRawCollisionForTest(&firstMesh);
    second.setRawCollisionForTest(&secondMesh);
    root.addChild(&first);
    root.addChild(&second);

    Mat4 firstRelative = Mat4::getIdentity();
    firstRelative.m[12] = 10.0f;
    first.setRelativeTransformation(firstRelative);
    Mat4 secondRelative = Mat4::getIdentity();
    secondRelative.m[13] = 20.0f;
    second.setRelativeTransformation(secondRelative);

    LocalBoundingBox* box = root.getFullBoundingBox();
    checks.expect(nearVec3(box->m_min, -5.0f, -5.0f, -5.0f) &&
                      nearVec3(box->m_max, 12.0f, 21.0f, 4.0f),
                  "full bounds merge own collision and both translated sibling branches");
    checks.expect(rootMesh.m_boundsCalls == 1 && firstMesh.m_boundsCalls == 1 &&
                      secondMesh.m_boundsCalls == 1,
                  "full bounds visit each contributing hierarchy mesh exactly once");
    checks.expect(first.m_fullGetterCalls == 2 && second.m_fullGetterCalls == 2 &&
                      events == std::vector<std::string>({
                          "first:full", "first:full",
                          "second:full", "second:full"}),
                  "full hierarchy reads each child's endpoints through two getters in sibling order");
    checks.expect(root.getFullBoundingBox() == box && rootMesh.m_boundsCalls == 1 &&
                      firstMesh.m_boundsCalls == 1 && secondMesh.m_boundsCalls == 1 &&
                      first.m_fullGetterCalls == 2 && second.m_fullGetterCalls == 2,
                  "root full-cache hit does not rebuild descendant boxes");

    root.removeChild(&first);
    root.removeChild(&second);
    root.setRawCollisionForTest(0);
    first.setRawCollisionForTest(0);
    second.setRawCollisionForTest(0);
    finishStackObject(first);
    finishStackObject(second);
    finishStackObject(root);
}

void testObjectBoundingBoxEndpointReflection(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    FactorySimpleTemplate childTemplate;
    std::vector<std::string> events;
    ObjectComponentRuntimeProbe root(&objectTemplate);
    ObjectComponentRuntimeProbe child(&childTemplate);
    CollisionMeshPeAbiProbe childMesh(&events);
    childMesh.setBounds(2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    child.setRawCollisionForTest(&childMesh);
    root.addChild(&child);

    Mat4 reflected = Mat4::getIdentity();
    reflected.m[0] = -1.0f;
    reflected.m[12] = 10.0f;
    child.setRelativeTransformation(reflected);
    LocalBoundingBox* box = root.getFullBoundingBox();
    checks.expect(nearVec3(box->m_min, 8.0f, 3.0f, 4.0f) &&
                      nearVec3(box->m_max, 5.0f, 6.0f, 7.0f),
                  "full bounds transform exactly min and max endpoints without reflection reorder");
    checks.expect(childMesh.m_boundsCalls == 1,
                  "reflected child contributes through one raw collision bounds call");

    root.removeChild(&child);
    child.setRawCollisionForTest(0);
    finishStackObject(child);
    finishStackObject(root);
}

void testObjectMineBoundingBoxEligibility(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    FactorySimpleTemplate ordinaryTemplate;
    SpringClassIdTemplate springTemplate;
    std::vector<std::string> events;
    ObjectComponentRuntimeProbe root(&objectTemplate);
    ObjectComponentRuntimeProbe ordinary(&ordinaryTemplate);
    ObjectComponentRuntimeProbe spring(&springTemplate);
    ObjectComponentRuntimeProbe springDescendant(&springTemplate);
    CollisionMeshPeAbiProbe rootMesh(&events);
    CollisionMeshPeAbiProbe ordinaryMesh(&events);
    CollisionMeshPeAbiProbe springMesh(&events);
    CollisionMeshPeAbiProbe descendantMesh(&events);
    rootMesh.setBounds(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    ordinaryMesh.setBounds(-100.0f, -100.0f, -100.0f, 100.0f, 100.0f, 100.0f);
    springMesh.setBounds(-2.0f, -2.0f, -2.0f, -1.0f, -1.0f, -1.0f);
    descendantMesh.setBounds(1.0f, 2.0f, 3.0f, 2.0f, 3.0f, 4.0f);
    root.setRawCollisionForTest(&rootMesh);
    ordinary.setRawCollisionForTest(&ordinaryMesh);
    spring.setRawCollisionForTest(&springMesh);
    springDescendant.setRawCollisionForTest(&descendantMesh);
    root.addChild(&ordinary);
    root.addChild(&spring);
    ordinary.addChild(&springDescendant);

    Mat4 ordinaryRelative = Mat4::getIdentity();
    ordinaryRelative.m[12] = 10.0f;
    ordinary.setRelativeTransformation(ordinaryRelative);
    Mat4 springRelative = Mat4::getIdentity();
    springRelative.m[13] = 20.0f;
    spring.setRelativeTransformation(springRelative);
    Mat4 descendantRelative = Mat4::getIdentity();
    descendantRelative.m[12] = 2.0f;
    springDescendant.setRelativeTransformation(descendantRelative);

    LocalBoundingBox* mine = root.getMineBoundingBox();
    checks.expect(rootMesh.m_boundsCalls == 1 && ordinaryMesh.m_boundsCalls == 0 &&
                      springMesh.m_boundsCalls == 1 && descendantMesh.m_boundsCalls == 1,
                  "mine bounds include root and attached Springs but exclude ordinary attached mesh");
    checks.expect(nearVec3(mine->m_min, -2.0f, 0.0f, -2.0f) &&
                      nearVec3(mine->m_max, 14.0f, 19.0f, 4.0f),
                  "mine bounds merge root, direct Spring and nested Spring endpoints");
    LocalBoundingBox* ordinaryMine = ordinary.getMineBoundingBox();
    checks.expect(nearVec3(ordinaryMine->m_min, 3.0f, 2.0f, 3.0f) &&
                      nearVec3(ordinaryMine->m_max, 4.0f, 3.0f, 4.0f),
                  "mine descendants propagate through an ineligible ordinary parent");
    checks.expect(root.getMineBoundingBox() == mine && rootMesh.m_boundsCalls == 1 &&
                      ordinaryMesh.m_boundsCalls == 0 && springMesh.m_boundsCalls == 1 &&
                      descendantMesh.m_boundsCalls == 1,
                  "mine-cache hit preserves eligibility call counts across the subtree");

    ordinary.removeChild(&springDescendant);
    root.removeChild(&ordinary);
    root.removeChild(&spring);
    root.setRawCollisionForTest(0);
    ordinary.setRawCollisionForTest(0);
    spring.setRawCollisionForTest(0);
    springDescendant.setRawCollisionForTest(0);
    finishStackObject(springDescendant);
    finishStackObject(ordinary);
    finishStackObject(spring);
    finishStackObject(root);
}

void testObjectDisableChildrenPropagation(Checks& checks)
{
    FactorySimpleTemplate parentTemplate;
    FactorySimpleTemplate childTemplate;
    FactorySimpleTemplate grandchildTemplate;
    ObjectComponentRuntimeProbe parent(&parentTemplate);
    ObjectComponentRuntimeProbe first(&childTemplate);
    ObjectComponentRuntimeProbe second(&childTemplate);
    ObjectComponentRuntimeProbe grandchild(&grandchildTemplate);
    parent.addChild(&first);
    parent.addChild(&second);
    first.addChild(&grandchild);

    parent.setDisableChildren(true);
    checks.expect(parent.getDisableChildren() &&
                      (first.flagsForTest() & 1u) != 0 &&
                      (second.flagsForTest() & 1u) != 0 &&
                      (grandchild.flagsForTest() & 1u) == 0,
                  "setDisableChildren stores the byte and sets flag 1 on every direct child");

    parent.setDisableChildren(false);
    checks.expect(!parent.getDisableChildren() &&
                      (first.flagsForTest() & 1u) == 0 &&
                      (second.flagsForTest() & 1u) == 0 &&
                      (grandchild.flagsForTest() & 1u) == 0,
                  "clearing disableChildren clears flag 1 on every direct child");

    first.removeChild(&grandchild);
    parent.removeChild(&first);
    parent.removeChild(&second);
    finishStackObject(grandchild);
    finishStackObject(first);
    finishStackObject(second);
    finishStackObject(parent);
}

void testUpdateFlagsRecursiveSemanticSplit(Checks& checks)
{
    FactorySimpleTemplate rootTemplate;
    FactorySimpleTemplate childTemplate;
    FactorySimpleTemplate grandchildTemplate;
    std::vector<std::string> events;
    UpdateFlagsSpyObject root(&rootTemplate, "root", &events);
    UpdateFlagsSpyObject child(&childTemplate, "child", &events);
    UpdateFlagsSpyObject grandchild(&grandchildTemplate, "grandchild", &events);
    root.addChild(&child);
    child.addChild(&grandchild);

    root.resetCalls();
    child.resetCalls();
    grandchild.resetCalls();
    events.clear();
    root.updateFlagsRecursive(0x800u, 0x400u);
    checks.expect(root.m_calls == 0 && child.m_calls == 1 && grandchild.m_calls == 1,
                  "Object::updateFlagsRecursive remains descendant-only");
    checks.expect(events == std::vector<std::string>({
                      "flags:child", "flags:grandchild"}),
                  "member recursive flags walk descendants in preorder");

    root.resetCalls();
    child.resetCalls();
    grandchild.resetCalls();
    events.clear();
    dice::hfe::world::updateFlagsRecursive(&root, 0x800u, 0x400u);
    checks.expect(root.m_calls == 1 && child.m_calls == 1 && grandchild.m_calls == 1,
                  "free updateFlagsRecursive includes its root object");
    checks.expect(events == std::vector<std::string>({
                      "flags:root", "flags:child", "flags:grandchild"}),
                  "free recursive flags walk applies self first, then descendants in preorder");
    checks.expect(root.m_lastSet == 0x800u && root.m_lastClear == 0x400u &&
                      child.m_lastSet == 0x800u && child.m_lastClear == 0x400u &&
                      grandchild.m_lastSet == 0x800u && grandchild.m_lastClear == 0x400u,
                  "free recursive flags preserve both masks at every depth");

    child.removeChild(&grandchild);
    root.removeChild(&child);
    finishStackObject(grandchild);
    finishStackObject(child);
    finishStackObject(root);
}

void testInternalAssignNewObjectId(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    FactorySimpleTemplate childTemplate;
    ObjectComponentRuntimeProbe root(&objectTemplate);
    ObjectComponentRuntimeProbe first(&childTemplate);
    ObjectComponentRuntimeProbe second(&childTemplate);
    root.addChild(&first);
    root.addChild(&second);

    WeakPtr<IObject> rootWeak = root.getWeakPtr();
    WeakPtr<IObject> firstWeak = first.getWeakPtr();
    WeakPtr<IObject> secondWeak = second.getWeakPtr();

    ObjectManagerPeAbiProbe manager(100);
    dice::hfe::world::ObjectManager* savedManager = dice::hfe::world::objectManager;
    dice::hfe::world::objectManager =
        reinterpret_cast<dice::hfe::world::ObjectManager*>(&manager);
    root.internal_assignNewObjectID();
    dice::hfe::world::objectManager = savedManager;

    checks.expect(root.getObjectId() == 101 && first.getObjectId() == 102 &&
                      second.getObjectId() == 103,
                  "internal ID reassignment visits root, child, then sibling in preorder");
    checks.expect(manager.m_newIdCalls == 3 && manager.m_unregistered.size() == 3 &&
                      manager.m_registered.size() == 3,
                  "internal ID reassignment performs all three manager operations per object");
    checks.expect(manager.m_unregistered[0] == &root && manager.m_unregistered[1] == &first &&
                      manager.m_unregistered[2] == &second &&
                      manager.m_registered[0] == &root && manager.m_registered[1] == &first &&
                      manager.m_registered[2] == &second,
                  "manager unregister/register order follows the recovered object traversal");
    checks.expect(rootWeak.get() == 0 && firstWeak.get() == 0 && secondWeak.get() == 0,
                  "internal ID reassignment invalidates every existing weak handle");

    root.removeChild(&first);
    root.removeChild(&second);
    finishStackObject(first);
    finishStackObject(second);
    finishStackObject(root);
}

void testObjectChildHierarchyLifecycle(Checks& checks)
{
    std::vector<std::string> events;
    ObjectManagerPeAbiProbe manager(1500, &events, "hierarchy");
    ResponsePhysicsManagerPeAbiProbe responseManager(&events);

    dice::hfe::world::ObjectManager* savedObjectManager =
        dice::hfe::world::objectManager;
    dice::hfe::world::ResponsePhysicsManager* savedResponseManager =
        dice::hfe::world::responsePhysicsManager;
    dice::hfe::world::objectManager =
        reinterpret_cast<dice::hfe::world::ObjectManager*>(&manager);
    dice::hfe::world::responsePhysicsManager =
        reinterpret_cast<dice::hfe::world::ResponsePhysicsManager*>(&responseManager);

    {
        FactorySimpleTemplate parentTemplate;
        FactorySimpleTemplate childTemplate;
        FactorySimpleTemplate replacementTemplate;
        HierarchyObjectProbe parent(&parentTemplate, "parent", &events);
        HierarchyObjectProbe child(&childTemplate, "child", &events);
        HierarchyObjectProbe replacement(&replacementTemplate, "replacement", &events);

        PhysicsNodeSleepingPeAbiProbe parentPhysics("parentPhysics", false, &events);
        PhysicsNodeSleepingPeAbiProbe firstPhysics("firstPhysics", false, &events);
        PhysicsNodeSleepingPeAbiProbe addTailPhysics("addTailPhysics", false, &events);
        PhysicsNodeSleepingPeAbiProbe removeTailPhysics("removeTailPhysics", false, &events);
        ResponsePhysicsPeAbiProbe rootResponse(0, &events);
        ResponsePhysicsPeAbiProbe firstResponse(0, &events);
        ResponsePhysicsPeAbiProbe addTailResponse(0, &events);
        ResponsePhysicsPeAbiProbe removeTailResponse(0, &events);

        parent.setRawPhysicsForTest(&parentPhysics);
        parent.setRawResponseForTest(&rootResponse);
        child.setRawPhysicsForTest(&firstPhysics);
        child.setRawResponseForTest(&firstResponse);
        firstPhysics.m_swapOwner = &child;
        firstPhysics.m_swapPhysics = &addTailPhysics;
        firstPhysics.m_swapResponse = &addTailResponse;
        parent.setFlagsForTest(parent.flagsForTest() | 8u);
        child.setFlagsForTest(child.flagsForTest() | 0x8400u);
        parent.clearBoundDirtyFlags();
        child.clearBoundDirtyFlags();
        events.clear();
        manager.m_refreshed.clear();
        manager.m_gridRemoved.clear();

        parent.addChild(&child);

        checks.expect(events == std::vector<std::string>({
                          "firstPhysics:setParent", "responseManagerRemove",
                          "child:setParent", "hierarchy:refreshGrid",
                          "child:updateFlags", "hierarchy:removeGrid",
                          "child:updateFlags", "child:setRelative",
                          "hierarchy:refreshGrid", "responseDirty",
                          "addTailPhysics:reset", "response:reset"}),
                      "addChild preserves physics-manager-parent-flag-grid-identity-dirty-reset order");
        checks.expect(firstPhysics.m_setParentCalls == 1 &&
                          firstPhysics.m_lastParent == &parentPhysics &&
                          firstPhysics.m_resetCalls == 0 &&
                          addTailPhysics.m_setParentCalls == 0 &&
                          addTailPhysics.m_resetCalls == 1 &&
                          addTailPhysics.m_lastReset,
                      "addChild uses PE physics +0x2C on the first pointer then reloads +0x114");
        checks.expect(firstResponse.m_cacheResetCalls == 0 &&
                          addTailResponse.m_cacheResetCalls == 1 &&
                          rootResponse.m_dirtyCalls == 1 &&
                          rootResponse.m_cacheResetCalls == 0,
                      "addChild reloads child response for PE +0x1C after dirtying the root response");
        checks.expect(responseManager.m_removeCalls == 1 &&
                          responseManager.m_addCalls == 0 &&
                          responseManager.m_lastObject == &child,
                      "addChild dispatches response-manager remove through the PE slot");
        checks.expect(parent.getChild() == &child && child.getParent() == &parent &&
                          (parent.flagsForTest() & 0x00800000u) != 0 &&
                          (child.flagsForTest() & 0x10u) != 0 &&
                          (child.flagsForTest() & 0x8000u) == 0,
                      "addChild links the first child, sets parent identity, propagates bit 8, and clears 0x8000");
        checks.expect(child.m_flagCalls ==
                          std::vector<std::pair<unsigned int, unsigned int> >({
                              std::make_pair(0x10u, 0u),
                              std::make_pair(0u, 0x8000u)}) &&
                          manager.m_gridRemoved.size() == 1 &&
                          manager.m_gridRemoved[0] == &child,
                      "addChild emits both exact flag masks around the child-grid removal");
        bool identity = child.m_relativeCalls == 1;
        for (int i = 0; i != 16; ++i)
            identity = identity && near(child.m_lastRelative.m[i], Mat4::getIdentity().m[i]);
        checks.expect(identity,
                      "addChild passes the complete identity matrix to setRelativeTransformation");
        checks.expect(parent.boundDirtyFlags() == 0x20140000u &&
                          child.boundDirtyFlags() == 0x20140000u &&
                          manager.m_refreshed.size() == 2 &&
                          manager.m_refreshed[0] == &parent &&
                          manager.m_refreshed[1] == &parent,
                      "addChild dirties child/parent via setParent then explicitly dirties the parent");

        addTailPhysics.m_swapOwner = &child;
        addTailPhysics.m_swapPhysics = &removeTailPhysics;
        addTailPhysics.m_swapResponse = &removeTailResponse;
        child.setReloadHeadOnDetach(&parent, &replacement);
        addTailPhysics.m_resetCalls = 0;
        addTailPhysics.m_setParentCalls = 0;
        addTailResponse.m_cacheResetCalls = 0;
        rootResponse.m_dirtyCalls = 0;
        responseManager.m_addCalls = 0;
        responseManager.m_removeCalls = 0;
        responseManager.m_lastObject = 0;
        parent.clearBoundDirtyFlags();
        child.clearBoundDirtyFlags();
        events.clear();
        manager.m_refreshed.clear();
        manager.m_gridAdded.clear();

        parent.removeChild(&child);

        checks.expect(events == std::vector<std::string>({
                          "responseDirty", "addTailPhysics:setParent",
                          "child:setParent", "child:updateFlags",
                          "hierarchy:refreshGrid", "hierarchy:addGrid",
                          "hierarchy:refreshGrid", "responseManagerAdd",
                          "removeTailPhysics:reset", "response:reset"}),
                      "removeChild preserves dirty-detach-parent-grid-manager-reset order");
        checks.expect(addTailPhysics.m_setParentCalls == 1 &&
                          addTailPhysics.m_lastParent == 0 &&
                          addTailPhysics.m_resetCalls == 0 &&
                          removeTailPhysics.m_setParentCalls == 0 &&
                          removeTailPhysics.m_resetCalls == 1 &&
                          removeTailPhysics.m_lastReset,
                      "removeChild calls PE physics +0x2C with null then reloads +0x114");
        checks.expect(addTailResponse.m_cacheResetCalls == 0 &&
                          removeTailResponse.m_cacheResetCalls == 1 &&
                          rootResponse.m_dirtyCalls == 1,
                      "removeChild dirties through the attached root then reloads child response");
        checks.expect(responseManager.m_addCalls == 1 &&
                          responseManager.m_removeCalls == 0 &&
                          responseManager.m_lastObject == &child,
                      "removeChild dispatches response-manager add through the PE slot");
        checks.expect(parent.getChild() == &replacement &&
                          (parent.flagsForTest() & 0x00800000u) != 0,
                      "removeChild reloads m_child after the virtual setParent callback before clearing 0x800000");
        checks.expect(child.getParent() == 0 &&
                          child.m_flagCalls.back() == std::make_pair(0x8000u, 0x10u) &&
                          manager.m_gridAdded.size() == 1 &&
                          manager.m_gridAdded[0] == &child,
                      "head removal restores root transform flags and grid membership");
        checks.expect(parent.boundDirtyFlags() == 0 &&
                          child.boundDirtyFlags() == 0x20140000u &&
                          manager.m_refreshed.size() == 2 &&
                          manager.m_refreshed[0] == &child &&
                          manager.m_refreshed[1] == &child,
                      "removeChild has no explicit parent bounds-dirty pass");

        parent.setRawChildForTest(0);
        parent.clearFlagsForTest(0x00800000u);
        child.setReloadHeadOnDetach(0, 0);
        child.setRawPhysicsForTest(0);
        child.setRawResponseForTest(0);
        parent.setRawPhysicsForTest(0);
        parent.setRawResponseForTest(0);
        finishStackObject(child);
        finishStackObject(replacement);
        finishStackObject(parent);
    }

    {
        FactorySimpleTemplate parentTemplate;
        FactorySimpleTemplate childTemplate;
        FactorySimpleTemplate orphanTemplate;
        HierarchyObjectProbe parent(&parentTemplate, "listParent", &events);
        HierarchyObjectProbe first(&childTemplate, "first", &events);
        HierarchyObjectProbe second(&childTemplate, "second", &events);
        HierarchyObjectProbe orphan(&orphanTemplate, "orphan", &events);

        events.clear();
        const int addsBeforeEarlyOut = responseManager.m_addCalls;
        parent.removeChild(0);
        parent.removeChild(&orphan);
        checks.expect(events.empty() && responseManager.m_addCalls == addsBeforeEarlyOut &&
                          parent.getChild() == 0 && orphan.getParent() == 0,
                      "removeChild null and parentless inputs return before every callback");

        parent.addChild(&first);
        parent.addChild(&second);
        checks.expect(parent.getChild() == &first && first.getSibling() == &second &&
                          second.getParent() == &parent && first.m_addSiblingCalls == 1 &&
                          (parent.flagsForTest() & 0x00800000u) != 0,
                      "addChild appends through the existing head without replacing it");

        parent.clearBoundDirtyFlags();
        events.clear();
        parent.removeChild(&second);
        checks.expect(parent.getChild() == &first && first.getSibling() == 0 &&
                          second.getParent() == 0 && first.m_removeSiblingCalls == 1 &&
                          (parent.flagsForTest() & 0x00800000u) != 0 &&
                          parent.boundDirtyFlags() == 0,
                      "nonhead removal delegates to removeSibling and preserves head flag/bounds");

        parent.addChild(&second);
        parent.clearBoundDirtyFlags();
        parent.removeChild(&first);
        checks.expect(parent.getChild() == &second && first.getParent() == 0 &&
                          (parent.flagsForTest() & 0x00800000u) != 0 &&
                          parent.boundDirtyFlags() == 0,
                      "head removal with a successor preserves 0x800000 without dirtying parent bounds");
        parent.clearBoundDirtyFlags();
        parent.removeChild(&second);
        checks.expect(parent.getChild() == 0 && second.getParent() == 0 &&
                          (parent.flagsForTest() & 0x00800000u) == 0 &&
                          parent.boundDirtyFlags() == 0,
                      "removing the last head child clears 0x800000 only after the callback");

        finishStackObject(orphan);
        finishStackObject(first);
        finishStackObject(second);
        finishStackObject(parent);
    }

    {
        FactorySimpleTemplate repeatedTemplate;
        HierarchyObjectProbe trapParent(&repeatedTemplate, "trapParent", &events);
        HierarchyObjectProbe trapChild(&repeatedTemplate, "trapChild", &events);
        trapParent.setDestroyedForStructuralTest(true);
        checks.expect(trapParent.getTemplate() == trapChild.getTemplate() &&
                          trapParent.isObjectDestroyed(),
                      "death-test fixture structurally reaches both addChild invariant guards");
        finishStackObject(trapChild);
        finishStackObject(trapParent);
    }

    dice::hfe::world::responsePhysicsManager = savedResponseManager;
    dice::hfe::world::objectManager = savedObjectManager;
}

void testObjectUpdateFlagsTransitions(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    FactorySimpleTemplate childTemplate;
    FactorySimpleTemplate grandchildTemplate;
    std::vector<std::string> events;
    ObjectManagerPeAbiProbe staticManager(2000, &events, "static");
    ObjectManagerPeAbiProbe globalManager(3000, &events, "global");
    PhysicsNodeManagerPeAbiProbe physicsManager(&events);
    ResponsePhysicsManagerPeAbiProbe responseManager(&events);

    dice::hfe::world::ObjectManager* savedStaticManager = Object::mObjectManager;
    dice::hfe::world::ObjectManager* savedGlobalManager =
        dice::hfe::world::objectManager;
    dice::hfe::world::PhysicsNodeManager* savedPhysicsManager =
        dice::hfe::world::physicsNodeManager;
    dice::hfe::world::ResponsePhysicsManager* savedResponseManager =
        dice::hfe::world::responsePhysicsManager;
    Object::mObjectManager =
        reinterpret_cast<dice::hfe::world::ObjectManager*>(&staticManager);
    dice::hfe::world::objectManager =
        reinterpret_cast<dice::hfe::world::ObjectManager*>(&globalManager);
    dice::hfe::world::physicsNodeManager =
        reinterpret_cast<dice::hfe::world::PhysicsNodeManager*>(&physicsManager);
    dice::hfe::world::responsePhysicsManager =
        reinterpret_cast<dice::hfe::world::ResponsePhysicsManager*>(&responseManager);

    ObjectComponentRuntimeProbe root(&objectTemplate);
    UpdateFlagsSpyObject child(&childTemplate, "child", &events);
    UpdateFlagsSpyObject grandchild(&grandchildTemplate, "grandchild", &events);
    root.addChild(&child);
    child.addChild(&grandchild);
    globalManager.m_refreshCalls = 0;
    globalManager.m_refreshed.clear();
    responseManager.m_addCalls = 0;
    responseManager.m_removeCalls = 0;
    responseManager.m_lastObject = 0;

    ResponsePhysicsPeAbiProbe response(0, &events);
    IncomingOwnerProbe responseOwner("response", 0, 0,
                                     dice::hfe::world::IID_IResponsePhysics,
                                     &response, 0);
    FastInterfaceProbe physicsNode(dice::hfe::world::IID_IPhysicsNode, 0);
    IncomingOwnerProbe physicsOwner("physics", 0, 0,
                                    dice::hfe::world::IID_IPhysicsNode,
                                    &physicsNode, 0);
    root.setComponent(dice::hfe::world::IID_IResponsePhysics, &responseOwner);
    root.setComponent(dice::hfe::world::IID_IPhysicsNode, &physicsOwner);

    root.setFlagsForTest(0x0100AC00u);
    const int globalRefreshBeforeDisable = globalManager.m_refreshCalls;
    const int responseDirtyBeforeDisable = response.m_dirtyCalls;
    events.clear();
    root.updateFlags(1u, 0);
    checks.expect((root.flagsForTest() & 1u) != 0 &&
                      staticManager.m_updateRemoved.size() == 1 &&
                      staticManager.m_frameRemoved.size() == 1,
                  "setting disabled removes update and frame memberships through the static manager");
    checks.expect(events == std::vector<std::string>({
                      "static:removeUpdate", "static:removeFrame",
                      "global:refreshGrid", "global:refreshGrid", "responseDirty",
                      "physicsManagerRemove", "responseManagerRemove"}),
                  "disabled transition preserves list-helper-physics-response call order");
    checks.expect(globalManager.m_refreshCalls == globalRefreshBeforeDisable + 2 &&
                      staticManager.m_refreshCalls == 0 &&
                      response.m_dirtyCalls == responseDirtyBeforeDisable + 1 &&
                      physicsManager.m_removeCalls == 1 &&
                      responseManager.m_removeCalls == 1,
                  "disabled transition separates global refresh from static list dispatch");

    events.clear();
    root.updateFlags(0, 1u);
    checks.expect((root.flagsForTest() & 1u) == 0 &&
                      staticManager.m_updateAdded.size() == 1 &&
                      staticManager.m_frameAdded.size() == 1,
                  "clearing disabled restores both surviving update memberships");
    checks.expect(events == std::vector<std::string>({
                      "static:addUpdate", "static:addFrame",
                      "global:refreshGrid", "global:refreshGrid", "responseDirty",
                      "physicsManagerAdd", "responseManagerAdd"}),
                  "enable transition preserves update-before-frame and helper call order");

    ObjectComponentRuntimeProbe transition(&objectTemplate);
    transition.setFlagsForTest(0x01008000u);
    events.clear();
    transition.updateFlags(0x6800u, 0);
    checks.expect(events == std::vector<std::string>({
                      "static:addFrame", "static:addUpdate", "static:addName"}),
                  "enabled membership transitions process frame before update then name hash");
    events.clear();
    transition.updateFlags(0, 0x6800u);
    checks.expect(events == std::vector<std::string>({
                      "static:removeFrame", "static:removeUpdate", "static:removeName"}),
                  "enabled membership removals preserve frame-update-name ordering");
    checks.expect((transition.flagsForTest() & 0x8000u) != 0,
                  "root updateFlags refuses to clear the 0x8000 transformation bit");

    ObjectComponentRuntimeProbe scheduledRoot(&objectTemplate);
    scheduledRoot.setFlagsForTest(0x01008000u);
    staticManager.m_updateAdded.clear();
    dice::hfe::world::updateFlagsRecursive(&scheduledRoot, 0x800u, 0);
    checks.expect((scheduledRoot.flagsForTest() & 0x800u) != 0 &&
                      staticManager.m_updateAdded.size() == 1 &&
                      staticManager.m_updateAdded[0] == &scheduledRoot,
                  "free recursive flags register the root itself for ObjectManager updates");

    child.resetCalls();
    grandchild.resetCalls();
    root.setFlagsForTest(0x01008000u);
    root.updateFlags(8u, 0);
    checks.expect(child.m_calls == 1 && child.m_lastSet == 0x10u &&
                      child.m_lastClear == 0 && grandchild.m_calls == 0,
                  "flag 8 propagation reaches direct children only");
    child.resetCalls();
    root.updateFlags(0x10u, 0);
    checks.expect(child.m_calls == 1 && child.m_lastSet == 0x10u &&
                      grandchild.m_calls == 0,
                  "flag 0x10 propagation also stops after direct children");
    child.resetCalls();
    root.updateFlags(0, 0x10u);
    checks.expect(child.m_calls == 0,
                  "clearing self 0x10 is suppressed for children while self flag 8 remains set");
    root.updateFlags(0, 8u);
    checks.expect(child.m_calls == 1 && child.m_lastClear == 0x10u,
                  "clearing flag 8 clears 0x10 on direct children");

    ObjectComponentRuntimeProbe responseFlag(&objectTemplate);
    ResponsePhysicsPeAbiProbe responseFlagComponent;
    IncomingOwnerProbe responseFlagOwner("responseFlag", 0, 0,
                                         dice::hfe::world::IID_IResponsePhysics,
                                         &responseFlagComponent, 0);
    responseFlag.setComponent(dice::hfe::world::IID_IResponsePhysics,
                              &responseFlagOwner);
    const int responseFlagDirty = responseFlagComponent.m_dirtyCalls;
    responseFlag.setFlagsForTest(0x01008000u);
    responseFlag.updateFlags(0x100u, 0);
    responseFlag.updateFlags(0, 0x100u);
    checks.expect(responseManager.m_addCalls == 2 && responseManager.m_removeCalls == 2 &&
                      responseFlagComponent.m_dirtyCalls == responseFlagDirty + 2,
                  "response flag transition adds/removes through its manager then dirties response");

    root.setComponent(dice::hfe::world::IID_IPhysicsNode, 0);
    root.setComponent(dice::hfe::world::IID_IResponsePhysics, 0);
    responseFlag.setComponent(dice::hfe::world::IID_IResponsePhysics, 0);
    child.removeChild(&grandchild);
    root.removeChild(&child);
    finishStackObject(grandchild);
    finishStackObject(child);
    finishStackObject(root);
    finishStackObject(transition);
    finishStackObject(scheduledRoot);
    finishStackObject(responseFlag);

    dice::hfe::world::responsePhysicsManager = savedResponseManager;
    dice::hfe::world::physicsNodeManager = savedPhysicsManager;
    Object::mObjectManager = savedStaticManager;
    dice::hfe::world::objectManager = savedGlobalManager;
}

void testSimpleObjectUpdateAndMessageDispatch(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    std::vector<std::string> events;
    SimpleUpdateObjectProbe object(&objectTemplate);
    RuntimeComponentProbe high("high", 0x7F030200u, &events);
    RuntimeComponentProbe low("low", 0x7F030100u, &events);
    object.setComponent(0x7F030200u, &high);
    object.setComponent(0x7F030100u, &low);

    ResponsePhysicsPeAbiProbe response(0, &events);
    IncomingOwnerProbe responseOwner("simpleResponse", 0, 0,
                                     dice::hfe::world::IID_IResponsePhysics,
                                     &response, 0);
    object.setComponent(dice::hfe::world::IID_IResponsePhysics, &responseOwner);
    const int responseDirtyBeforeUpdate = response.m_dirtyCalls;

    object.setFlagsForTest(0x01408000u);
    low.m_clearFlagsOnUpdate = 0x01000000u;
    events.clear();
    object.handleUpdate(0.375f, 0xDEADBEEFu);
    checks.expect(events == std::vector<std::string>({
                      "update:low", "update:high", "responseUpdate"}) &&
                      low.m_updateCalls == 1 && high.m_updateCalls == 1,
                  "SimpleObject update iterates components in ascending key order before response");
    checks.expect(near(low.m_lastDeltaTime, 0.375f) &&
                      near(high.m_lastDeltaTime, 0.375f) &&
                      response.m_updateCalls == 1 && near(response.m_lastDeltaTime, 0.375f),
                  "SimpleObject forwards exact delta time and ignores the unsigned formal");
    checks.expect((object.flagsForTest() & 0x01000000u) == 0 &&
                      response.m_dirtyCalls == responseDirtyBeforeUpdate &&
                      response.m_updateCalls == 1,
                  "SimpleObject response gate uses the entry flag snapshot despite component mutation");

    low.m_clearFlagsOnUpdate = 0;
    object.setFlagsForTest(0x01008000u);
    events.clear();
    object.handleUpdate(1.0f, 1u);
    checks.expect(events == std::vector<std::string>({"responseUpdate"}) &&
                      low.m_updateCalls == 1 && high.m_updateCalls == 1,
                  "response-only gate skips every runtime component");
    object.setFlagsForTest(0x00408000u);
    events.clear();
    object.handleUpdate(2.0f, 2u);
    checks.expect(events == std::vector<std::string>({"update:low", "update:high"}) &&
                      response.m_updateCalls == 2,
                  "component-only gate skips response physics");
    object.setFlagsForTest(0x00008000u);
    events.clear();
    object.handleUpdate(3.0f, 3u);
    checks.expect(events.empty(), "cleared SimpleObject update gates dispatch nothing");

    PlayerManagerPeAbiProbe playerManager(&events);
    dice::hfe::world::PlayerManager* savedPlayerManager =
        dice::hfe::world::playerManager;
    dice::hfe::world::playerManager =
        reinterpret_cast<dice::hfe::world::PlayerManager*>(&playerManager);
    FastInterfaceProbe radio(dice::hfe::world::IID_IRadioGLComp, 0);
    IncomingOwnerProbe radioOwner("radio", 0, 0,
                                  dice::hfe::world::IID_IRadioGLComp, &radio, 0);
    object.setComponent(dice::hfe::world::IID_IRadioGLComp, &radioOwner);

    dice::hfe::world::IPlayer* player =
        reinterpret_cast<dice::hfe::world::IPlayer*>(&object);
    events.clear();
    object.handleMessage(static_cast<dice::hfe::world::TemplateMessage>(39), player, true);
    checks.expect(events == std::vector<std::string>({
                      "playerManager:getLocal", "message:low", "message:high"}),
                  "SimpleObject message performs radio lookup before ascending component dispatch");
    checks.expect(low.m_lastMessage == static_cast<dice::hfe::world::TemplateMessage>(39) &&
                      low.m_lastPlayer == player && low.m_lastMessageFlag &&
                      high.m_messageCalls == 1,
                  "SimpleObject message preserves id, player and boolean arguments");
    events.clear();
    object.handleMessage(static_cast<dice::hfe::world::TemplateMessage>(41), player, false);
    checks.expect(events == std::vector<std::string>({"playerManager:getLocal"}) &&
                      low.m_messageCalls == 1 && high.m_messageCalls == 1,
                  "unfiltered message still performs radio lookup but skips components");
    object.setComponent(dice::hfe::world::IID_IRadioGLComp, 0);
    events.clear();
    object.handleMessage(static_cast<dice::hfe::world::TemplateMessage>(40), player, false);
    checks.expect(events == std::vector<std::string>({"message:low", "message:high"}) &&
                      low.m_messageCalls == 2 && high.m_messageCalls == 2 &&
                      !low.m_lastMessageFlag,
                  "message 40 dispatches components without local-player lookup when radio is absent");

    dice::hfe::world::playerManager = savedPlayerManager;
    object.setComponent(dice::hfe::world::IID_IResponsePhysics, 0);
    object.setComponent(0x7F030100u, 0);
    object.setComponent(0x7F030200u, 0);
    finishStackObject(object);
}

void testObjectDestroyLifecycle(Checks& checks)
{
    FactorySimpleTemplate parentTemplate;
    FactorySimpleTemplate rootTemplate;
    FactorySimpleTemplate childTemplate;
    FactorySimpleTemplate grandchildTemplate;
    std::vector<std::string> events;
    ObjectManagerPeAbiProbe staticManager(4000, &events, "static");
    ObjectManagerPeAbiProbe globalManager(5000, &events, "global");
    EventManagerPeAbiProbe eventManager(&events);

    dice::hfe::world::ObjectManager* savedStaticManager = Object::mObjectManager;
    dice::hfe::world::ObjectManager* savedGlobalManager =
        dice::hfe::world::objectManager;
    dice::hfe::EventManager* savedEventManager = dice::hfe::eventManager;
    Object::mObjectManager =
        reinterpret_cast<dice::hfe::world::ObjectManager*>(&staticManager);
    dice::hfe::world::objectManager =
        reinterpret_cast<dice::hfe::world::ObjectManager*>(&globalManager);
    dice::hfe::eventManager =
        reinterpret_cast<dice::hfe::EventManager*>(&eventManager);

    DestroyTrackingObject parent(&parentTemplate, "parent", &events);
    DestroyTrackingObject root(&rootTemplate, "root", &events);
    DestroyTrackingObject first(&childTemplate, "first", &events);
    DestroyTrackingObject grandchild(&grandchildTemplate, "grandchild", &events);
    DestroyTrackingObject second(&childTemplate, "second", &events);
    parent.addChild(&root);
    root.addChild(&first);
    first.addChild(&grandchild);
    root.addChild(&second);

    RuntimeComponentProbe high("high", 0x7F040200u, &events);
    RuntimeComponentProbe low("low", 0x7F040100u, &events);
    root.setComponent(0x7F040200u, &high);
    root.setComponent(0x7F040100u, &low);

    // Object::destroy checks only pointer presence before posting; no INetworkable method is used.
    root.setRawNetworkableForTest(
        reinterpret_cast<dice::hfe::io::INetworkable*>(&root));
    events.clear();
    root.destroy(true);
    checks.expect(events == std::vector<std::string>({
                      "objectDestroy:root:force", "destroy:low", "destroy:high",
                      "static:removeGrid", "eventManager:post",
                      "global:refreshGrid", "global:refreshGrid",
                      "objectDestroy:first:normal", "static:removeGrid",
                      "objectDestroy:grandchild:normal", "static:removeGrid",
                      "objectDestroy:second:normal", "static:removeGrid",
                      "static:addDestroy"}),
                  "forced destroy preserves component order, network event, descendant preorder, then queue");
    checks.expect(root.isObjectDestroyed() && first.isObjectDestroyed() &&
                      grandchild.isObjectDestroyed() && second.isObjectDestroyed() &&
                      root.getParent() == 0 && parent.getChild() == 0,
                  "forced destroy marks the complete subtree and detaches the root from its parent");
    checks.expect(low.m_destroyCalls == 1 && high.m_destroyCalls == 1 &&
                      staticManager.m_gridRemoved.size() == 4 &&
                      staticManager.m_destroyAdded.size() == 1 &&
                      staticManager.m_destroyAdded[0] == &root,
                  "destroy calls root components once, removes every object from grid, queues root only");
    checks.expect(eventManager.m_postCalls == 1 &&
                      static_cast<int>(eventManager.m_category) == 6 &&
                      eventManager.m_id == 17 && eventManager.m_data == &root &&
                      near(eventManager.m_delay, 0.0f),
                  "networkable destroy posts exact category 6 event 17 payload and delay");

    events.clear();
    root.destroy(false);
    root.destroy(true);
    checks.expect(events == std::vector<std::string>({
                      "objectDestroy:root:normal", "objectDestroy:root:force"}) &&
                      low.m_destroyCalls == 1 && high.m_destroyCalls == 1 &&
                      eventManager.m_postCalls == 1 && staticManager.m_addDestroyCalls == 1,
                  "destroy is idempotent after the virtual entry even when force changes");

    {
        DestroyTrackingObject nonForcedParent(
            &parentTemplate, "nonForcedParent", &events);
        DestroyTrackingObject nonForced(&rootTemplate, "nonForced", &events);
        nonForcedParent.addChild(&nonForced);
        const std::size_t gridRemovesBefore = staticManager.m_gridRemoved.size();
        const int destroyAddsBefore = staticManager.m_addDestroyCalls;

        events.clear();
        nonForced.destroy(false);
        checks.expect(events == std::vector<std::string>({
                          "objectDestroy:nonForced:normal", "static:removeGrid"}) &&
                          nonForced.isObjectDestroyed() &&
                          nonForced.getParent() == &nonForcedParent &&
                          nonForcedParent.getChild() == &nonForced,
                      "non-forced destroy marks and removes from grid without detaching");
        checks.expect(staticManager.m_gridRemoved.size() == gridRemovesBefore + 1 &&
                          staticManager.m_addDestroyCalls == destroyAddsBefore,
                      "non-forced destroy does not enqueue the object");

        events.clear();
        nonForced.destroy(true);
        checks.expect(events == std::vector<std::string>({
                          "objectDestroy:nonForced:force"}) &&
                          nonForced.getParent() == &nonForcedParent &&
                          nonForcedParent.getChild() == &nonForced &&
                          staticManager.m_gridRemoved.size() == gridRemovesBefore + 1 &&
                          staticManager.m_addDestroyCalls == destroyAddsBefore,
                      "later forced destroy is idempotent and cannot retroactively detach or queue");

        nonForcedParent.removeChild(&nonForced);
        finishStackObject(nonForcedParent);
    }

    root.setRawNetworkableForTest(0);
    root.setComponent(0x7F040100u, 0);
    root.setComponent(0x7F040200u, 0);
    if (first.getChild() == &grandchild)
        first.removeChild(&grandchild);
    if (root.getChild() == &first)
        root.removeChild(&first);
    if (root.getChild() == &second)
        root.removeChild(&second);
    // These four objects are already destroyed. Their destructors only require the invariant;
    // calling forceDestroy again would intentionally be a no-op and cannot unlink descendants.
    finishStackObject(parent);

    dice::hfe::eventManager = savedEventManager;
    Object::mObjectManager = savedStaticManager;
    dice::hfe::world::objectManager = savedGlobalManager;
}

void testSimpleObjectArmorInfo(Checks& checks)
{
    FactorySimpleTemplate parentTemplate;
    FactorySimpleTemplate childTemplate;
    FactorySimpleTemplate grandchildTemplate;
    std::vector<std::string> events;
    SimpleUpdateObjectProbe parent(&parentTemplate);
    SimpleUpdateObjectProbe child(&childTemplate);
    SimpleUpdateObjectProbe grandchild(&grandchildTemplate);
    parent.addChild(&child);
    child.addChild(&grandchild);

    PhysicsNodeSleepingPeAbiProbe childPhysics("childPhysics", true, &events);
    PhysicsNodeSleepingPeAbiProbe parentPhysics("parentPhysics", false, &events);
    child.setRawPhysicsForTest(&childPhysics);
    parent.setRawPhysicsForTest(&parentPhysics);

    events.clear();
    checks.expect(child.getArmorInfo().empty() && events.empty() &&
                      childPhysics.m_calls == 0 && parentPhysics.m_calls == 0,
                  "armor info returns empty without consulting physics when no ancestor has armor");

    ArmorPeAbiProbe parentArmor(42.125f, &events);
    IncomingOwnerProbe parentArmorOwner("parentArmor", 0, 0,
                                        dice::hfe::world::IID_IArmorGLComp,
                                        &parentArmor, 0);
    parent.setComponent(dice::hfe::world::IID_IArmorGLComp, &parentArmorOwner);
    events.clear();
    checks.expect(child.getArmorInfo() == "42.12*" &&
                      events == std::vector<std::string>({
                          "childPhysics:isSleeping", "armor:getHitPoints"}),
                  "armor info uses receiver physics before nearest ancestor hit points and adds star");
    checks.expect(childPhysics.m_calls == 1 && parentPhysics.m_calls == 0 &&
                      parentArmor.m_getCalls == 1,
                  "ancestor armor does not substitute the ancestor physics node");

    ArmorPeAbiProbe childArmor(7.0f, &events);
    IncomingOwnerProbe childArmorOwner("childArmor", 0, 0,
                                       dice::hfe::world::IID_IArmorGLComp,
                                       &childArmor, 0);
    child.setComponent(dice::hfe::world::IID_IArmorGLComp, &childArmorOwner);
    childPhysics.m_sleeping = false;
    events.clear();
    checks.expect(child.getArmorInfo() == "7.00" &&
                      events == std::vector<std::string>({
                          "childPhysics:isSleeping", "armor:getHitPoints"}) &&
                      childArmor.m_getCalls == 1 && parentArmor.m_getCalls == 1,
                  "nearest armor wins and awake output keeps exact two-decimal formatting");

    events.clear();
    checks.expect(grandchild.getArmorInfo() == "7.00" &&
                      events == std::vector<std::string>({"armor:getHitPoints"}),
                  "receiver without physics still walks to nearest armor and omits sleeping query");

    child.setComponent(dice::hfe::world::IID_IArmorGLComp, 0);
    parent.setComponent(dice::hfe::world::IID_IArmorGLComp, 0);
    child.setRawPhysicsForTest(0);
    parent.setRawPhysicsForTest(0);
    child.removeChild(&grandchild);
    parent.removeChild(&child);
    finishStackObject(grandchild);
    finishStackObject(child);
    finishStackObject(parent);
}

void testSimpleObjectDisableCachedLifecycle(Checks& checks)
{
    std::vector<std::string> events;
    PortalManagerPeAbiProbe firstPortalManager(&events);
    PortalManagerPeAbiProbe secondPortalManager(&events);
    dice::hfe::IBase* savedPortalManager = dice::hfe::world::portalManager;
    dice::hfe::world::portalManager =
        reinterpret_cast<dice::hfe::IBase*>(&firstPortalManager);

    {
        FactorySimpleTemplate objectTemplate;
        SimpleDisableObjectProbe object(&objectTemplate, &events);

        object.configureCachedState(false);
        events.clear();
        object.disableCachedObject();
        checks.expect(events == std::vector<std::string>({
                          "base:getCacheId", "base:updateFlags",
                          "base:setActiveLod", "base:setAbsolute",
                          "base:updateFlags", "base:updateGeometry"}) &&
                          object.m_setComponentCalls == 0 && object.m_queryCalls.empty(),
                      "Simple disable with null AI skips both portal queries then enters Object base");
        checks.expect(object.m_flagCalls ==
                          std::vector<std::pair<unsigned int, unsigned int> >({
                              std::make_pair(0x20u, 0u),
                              std::make_pair(0x40000001u, 0u)}) &&
                          object.m_getCacheIdCalls == 1 && object.m_activeLodCalls == 1 &&
                          object.m_absoluteCalls == 1 && object.m_geometryCalls == 1,
                      "qualified Object disable receives a valid cache and completes its exact tail");

        object.configureCachedState(true);
        object.m_setComponentResult = true;
        events.clear();
        object.disableCachedObject();
        checks.expect(events == std::vector<std::string>({
                          "simple:setComponent", "simple:queryPortal",
                          "simple:querySector", "base:getCacheId",
                          "base:updateFlags", "base:setActiveLod",
                          "base:setAbsolute", "base:updateFlags",
                          "base:updateGeometry"}),
                      "AI-present Simple disable uses virtual set/query dispatch before Object base");
        checks.expect(!object.hasAiForTest() && object.m_setComponentCalls == 1 &&
                          object.m_lastComponentId == dice::hfe::ai::IID_IAIObject &&
                          object.m_lastComponent == 0 &&
                          firstPortalManager.m_portalCalls == 0 &&
                          firstPortalManager.m_sectorCalls == 0,
                      "AI removal clears the fast field and false portal results skip callbacks");
        checks.expect(object.m_queryCalls ==
                          std::vector<std::pair<unsigned int, unsigned int> >({
                              std::make_pair(dice::hfe::world::IID_IPortalPassing,
                                             dice::hfe::world::IID_IPortalPassing),
                              std::make_pair(dice::hfe::world::IID_ISector,
                                             dice::hfe::world::IID_ISector)}),
                      "Simple disable passes doubled portal and sector IIDs exactly");

        object.configureCachedState(true);
        object.m_setComponentResult = false;
        object.m_portalResult = true;
        object.m_sectorResult = false;
        firstPortalManager.m_portalCalls = 0;
        firstPortalManager.m_sectorCalls = 0;
        firstPortalManager.m_portalObject = 0;
        firstPortalManager.m_sectorObject = 0;
        secondPortalManager.m_portalCalls = 0;
        secondPortalManager.m_sectorCalls = 0;
        secondPortalManager.m_portalObject = 0;
        secondPortalManager.m_sectorObject = 0;
        firstPortalManager.m_managerAfterPortal = &secondPortalManager;
        firstPortalManager.m_sectorResultAfterPortal = &object.m_sectorResult;
        dice::hfe::world::portalManager =
            reinterpret_cast<dice::hfe::IBase*>(&firstPortalManager);
        events.clear();
        object.disableCachedObject();

        checks.expect(events == std::vector<std::string>({
                          "simple:setComponent", "simple:queryPortal",
                          "unregisterPortal", "simple:querySector",
                          "unregisterSector", "base:getCacheId",
                          "base:updateFlags", "base:setActiveLod",
                          "base:setAbsolute", "base:updateFlags",
                          "base:updateGeometry"}),
                      "Simple disable preserves AI-portal-sector-base order when both queries hit");
        checks.expect(!object.hasAiForTest() && !object.m_setComponentResult &&
                          object.m_setComponentCalls == 1 &&
                          object.m_queryCalls.size() == 2,
                      "false AI setComponent result neither retains the field nor short-circuits portals");
        checks.expect(firstPortalManager.m_portalCalls == 1 &&
                          firstPortalManager.m_portalObject == &object &&
                          firstPortalManager.m_sectorCalls == 0 &&
                          secondPortalManager.m_portalCalls == 0 &&
                          secondPortalManager.m_sectorCalls == 1 &&
                          secondPortalManager.m_sectorObject == &object,
                      "portal callback can change sector outcome and Simple reloads portalManager afterward");

        firstPortalManager.m_managerAfterPortal = 0;
        firstPortalManager.m_sectorResultAfterPortal = 0;
        object.prepareForCleanup();
        finishStackObject(object);
    }

    dice::hfe::world::portalManager = savedPortalManager;
}

void testObjectTemplatePhysicsRemovalHooks(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);
    std::vector<std::string> events;
    PhysicsNodeManagerPeAbiProbe physicsManagerProbe(&events);
    ResponsePhysicsManagerPeAbiProbe responseManagerProbe(&events);

    dice::hfe::world::PhysicsNodeManager* savedPhysicsNodeManager =
        dice::hfe::world::physicsNodeManager;
    dice::hfe::world::ResponsePhysicsManager* savedResponsePhysicsManager =
        dice::hfe::world::responsePhysicsManager;
    dice::hfe::world::physicsNodeManager =
        reinterpret_cast<dice::hfe::world::PhysicsNodeManager*>(&physicsManagerProbe);
    dice::hfe::world::responsePhysicsManager =
        reinterpret_cast<dice::hfe::world::ResponsePhysicsManager*>(&responseManagerProbe);

    objectTemplate.removePhysicsNodeComponent(&object);
    objectTemplate.removeResponsePhysicsComponent(&object);
    checks.expect(physicsManagerProbe.m_removeCalls == 0 &&
                      responseManagerProbe.m_removeCalls == 0 && events.empty() &&
                      object.baseComponentCount() == 0 &&
                      object.objectComponentCount() == 0,
                  "physics removal hooks are no-ops when their object fields are null");

    FastInterfaceProbe physicsNode(dice::hfe::world::IID_IPhysicsNode, 0);
    IncomingOwnerProbe physicsOwner("physics", &events, 0,
                                    dice::hfe::world::IID_IPhysicsNode,
                                    &physicsNode, 0);
    object.setComponent(dice::hfe::world::IID_IPhysicsNode, &physicsOwner);
    events.clear();
    objectTemplate.removePhysicsNodeComponent(&object);
    checks.expect(physicsManagerProbe.m_removeCalls == 1 &&
                      physicsManagerProbe.m_lastNode ==
                          reinterpret_cast<dice::hfe::world::IPhysicsNode*>(&physicsNode),
                  "physics removal passes the installed node, not its owning object");
    checks.expect(object.fastField(dice::hfe::world::IID_IPhysicsNode) == 0 &&
                      object.baseComponentCount() == 0 &&
                      physicsOwner.m_releaseCalls == 1,
                  "physics removal detaches through production Object::setComponent");
    checks.expect(events == std::vector<std::string>({
                      "physicsManagerRemove", "release:physics:owner"}),
                  "physics manager dispatch precedes production component detach");

    FastInterfaceProbe managerlessPhysicsNode(dice::hfe::world::IID_IPhysicsNode, 0);
    IncomingOwnerProbe managerlessPhysicsOwner("physicsNoManager", &events, 0,
                                               dice::hfe::world::IID_IPhysicsNode,
                                               &managerlessPhysicsNode, 0);
    object.setComponent(dice::hfe::world::IID_IPhysicsNode, &managerlessPhysicsOwner);
    dice::hfe::world::physicsNodeManager = 0;
    events.clear();
    objectTemplate.removePhysicsNodeComponent(&object);
    checks.expect(object.fastField(dice::hfe::world::IID_IPhysicsNode) == 0 &&
                      managerlessPhysicsOwner.m_releaseCalls == 1 &&
                      physicsManagerProbe.m_removeCalls == 1 &&
                      events == std::vector<std::string>({
                          "release:physicsNoManager:owner"}),
                  "physics removal still detaches when the manager is null");

    ResponsePhysicsPeAbiProbe responsePhysics;
    IncomingOwnerProbe responseOwner("response", &events, 0,
                                     dice::hfe::world::IID_IResponsePhysics,
                                     &responsePhysics, 0);
    object.setComponent(dice::hfe::world::IID_IResponsePhysics, &responseOwner);
    const int responseDirtyBeforeRemoval = responsePhysics.m_dirtyCalls;
    events.clear();
    objectTemplate.removeResponsePhysicsComponent(&object);
    checks.expect(responseManagerProbe.m_removeCalls == 1 &&
                      responseManagerProbe.m_lastObject == &object,
                  "response removal passes the owning object, not the response component");
    checks.expect(object.fastField(dice::hfe::world::IID_IResponsePhysics) == 0 &&
                      object.baseComponentCount() == 0 &&
                      responseOwner.m_releaseCalls == 1 &&
                      responsePhysics.m_dirtyCalls == responseDirtyBeforeRemoval + 1,
                  "response removal detaches through production Object::setComponent");
    checks.expect(events == std::vector<std::string>({
                      "responseManagerRemove", "release:response:owner"}),
                  "response manager dispatch precedes production component detach");

    ResponsePhysicsPeAbiProbe managerlessResponsePhysics;
    IncomingOwnerProbe managerlessResponseOwner("responseNoManager", &events, 0,
                                                dice::hfe::world::IID_IResponsePhysics,
                                                &managerlessResponsePhysics, 0);
    object.setComponent(dice::hfe::world::IID_IResponsePhysics,
                        &managerlessResponseOwner);
    const int managerlessDirtyBeforeRemoval = managerlessResponsePhysics.m_dirtyCalls;
    dice::hfe::world::responsePhysicsManager = 0;
    events.clear();
    objectTemplate.removeResponsePhysicsComponent(&object);
    checks.expect(object.fastField(dice::hfe::world::IID_IResponsePhysics) == 0 &&
                      managerlessResponseOwner.m_releaseCalls == 1 &&
                      managerlessResponsePhysics.m_dirtyCalls ==
                          managerlessDirtyBeforeRemoval + 1 &&
                      responseManagerProbe.m_removeCalls == 1 &&
                      events == std::vector<std::string>({
                          "release:responseNoManager:owner"}),
                  "response removal still detaches when the manager is null");

    dice::hfe::world::physicsNodeManager = savedPhysicsNodeManager;
    dice::hfe::world::responsePhysicsManager = savedResponsePhysicsManager;
    finishStackObject(object);
}

void testPortalManagerUnregisterPaths(Checks& checks)
{
    ChildPartTemplateProbe objectTemplate;
    PortalManager manager;

    std::vector<std::string> portalEvents;
    ReleaseOrderProbe portalPassing("portal", &portalEvents);
    PortalUnregisterObjectProbe passingObject(&objectTemplate, "portal", &portalEvents);
    PortalPassingMap& passingMap = portalPassingMapAtRecoveredX86Offset(manager);
    PortalManager::PassingObjectData* passingData =
        new PortalManager::PassingObjectData;
    Vec3 lastPosition = {1.0f, 2.0f, 3.0f};
    passingData->lastPosition = lastPosition;
    passingData->portalPassing =
        reinterpret_cast<dice::hfe::world::PortalPassing*>(&portalPassing);
    passingMap.insert(std::make_pair(static_cast<IObject*>(&passingObject), passingData));

    checks.expect(passingMap.size() == 1,
                  "portal-passing fixture seeds the recovered PortalManager +0x08 map");
    manager.unRegisterPortalPassingObject(&passingObject);
    checks.expect(passingObject.m_setCalls == 1 &&
                      passingObject.m_lastClassId ==
                          dice::hfe::world::IID_IPortalPassing &&
                      passingObject.m_lastComponent == 0,
                  "portal unregister detaches IID_IPortalPassing with a null component");
    checks.expect(portalPassing.m_releaseCalls == 1 && portalPassing.getRef() == 1,
                  "portal unregister releases the detached PortalPassing exactly once");
    checks.expect(portalEvents == std::vector<std::string>({
                      "detach:portal", "release:portal"}),
                  "portal unregister detaches before releasing the PortalPassing");
    checks.expect(passingMap.empty(),
                  "portal unregister erases the object's PassingObjectData map entry");

    manager.unRegisterPortalPassingObject(&passingObject);
    checks.expect(passingObject.m_setCalls == 1 && portalPassing.m_releaseCalls == 1 &&
                      portalEvents.size() == 2,
                  "portal unregister map miss performs no detach or release");

    std::vector<std::string> sectorEvents;
    ReleaseOrderProbe sector("sector", &sectorEvents);
    PortalUnregisterObjectProbe sectorObject(&objectTemplate, "sector", &sectorEvents);
    sectorObject.m_queryResult = &sector;
    sectorObject.m_setResult = false;
    manager.unRegisterSectorObject(&sectorObject);

    checks.expect(sectorObject.m_queryCalls == 1 &&
                      sectorObject.m_lastQueryClassId == dice::hfe::world::IID_ISector &&
                      sectorObject.m_lastQueryInterfaceId == dice::hfe::world::IID_ISector,
                  "sector unregister preserves the doubled IID_ISector query");
    checks.expect(sectorObject.m_setCalls == 1 &&
                      sectorObject.m_lastClassId == dice::hfe::world::IID_ISector &&
                      sectorObject.m_lastComponent == 0,
                  "sector unregister detaches IID_ISector with a null component");
    checks.expect(sector.m_releaseCalls == 1 && sector.getRef() == 1,
                  "sector unregister releases after detach even when setComponent fails");
    checks.expect(sectorEvents == std::vector<std::string>({
                      "query:sector", "detach:sector", "release:sector"}),
                  "sector unregister preserves query-detach-release ordering");

    std::vector<std::string> missingSectorEvents;
    PortalUnregisterObjectProbe missingSectorObject(
        &objectTemplate, "missingSector", &missingSectorEvents);
    manager.unRegisterSectorObject(&missingSectorObject);
    checks.expect(missingSectorObject.m_queryCalls == 1 &&
                      missingSectorObject.m_setCalls == 0 &&
                      missingSectorEvents ==
                          std::vector<std::string>({"query:missingSector"}),
                  "sector unregister query miss performs no detach or release");

    passingObject.forceDestroy();
    sectorObject.forceDestroy();
    missingSectorObject.forceDestroy();
}

void testSimpleObjectRecoveredLifecycleBranches(Checks& checks)
{
    std::vector<std::string> events;
    SimpleLifecycleTemplateProbe objectTemplate(&events);
    objectTemplate.setName(std::string("RuntimePairSimpleLifecycle"));
    objectTemplate.configureConstructorBranches(
        std::string("RuntimePairSimpleCollision"), 7, 23);

    CollisionMeshPeAbiProbe mesh(&events);
    CollisionManagerPeAbiProbe collisionManagerProbe(&events, &mesh);
    ClassManagerPeAbiProbe classManagerProbe(
        &events, static_cast<dice::hfe::IBase*>(&collisionManagerProbe));

    dice::hfe::ClassManager* savedClassManager = dice::hfe::classManager;
    dice::hfe::classManager =
        reinterpret_cast<dice::hfe::ClassManager*>(&classManagerProbe);

    SimpleLifecycleObjectProbe* object =
        new SimpleLifecycleObjectProbe(&objectTemplate);

    dice::hfe::classManager = savedClassManager;

    checks.expect(objectTemplate.m_setPhysicsCalls == 1 &&
                      objectTemplate.m_setResponseCalls == 1,
                  "SimpleObject constructor installs both recovered physics seams in order");
    checks.expect(object->getUpdateFrequency() == 23,
                  "SimpleObject constructor copies the template physics frequency");
    checks.expect((object->getFlags() & 0x1100u) == 0x1100u,
                  "SimpleObject constructor sets collision-physics and animated-UV flags");
    checks.expect(object->hasInitializedAnimatedUV(),
                  "SimpleObject constructor allocates the zeroed enabled animated-UV state");
    checks.expect(classManagerProbe.m_lookupCalls == 1 &&
                      classManagerProbe.m_names.size() == 1 &&
                      classManagerProbe.m_names[0] == "CollisionManager",
                  "SimpleObject constructor resolves the CollisionManager singleton by name");
    checks.expect(collisionManagerProbe.m_lookupCalls == 1 &&
                      collisionManagerProbe.m_names.size() == 1 &&
                      collisionManagerProbe.m_names[0] == "RuntimePairSimpleCollision" &&
                      collisionManagerProbe.m_parts.size() == 1 &&
                      collisionManagerProbe.m_parts[0] == 7,
                  "SimpleObject constructor requests the configured collision mesh part");
    checks.expect(mesh.m_mappingCalls == 1 && mesh.m_lastTemplate == &objectTemplate,
                  "SimpleObject constructor builds the mesh material map from its template");
    checks.expect(object->queryComponent(dice::hfe::world::IID_ICollisionMesh,
                                         dice::hfe::world::IID_ICollisionMesh) ==
                      static_cast<dice::hfe::IBase*>(&mesh) &&
                      mesh.m_queryCalls == 2 && mesh.m_addCalls == 1,
                  "SimpleObject constructor attaches the mesh through production setComponent");
    checks.expect(mesh.m_releaseCalls == 1 && mesh.getRef() == 5,
                  "SimpleObject constructor releases its temporary while Object retains the mesh");
    checks.expect(events == std::vector<std::string>({
                      "setPhysicsNode", "setResponsePhysics",
                      "singleton:CollisionManager", "lookup:7", "mapping",
                      std::string("meshQuery:") +
                          std::to_string(dice::hfe::world::IID_IObjectComponent),
                      std::string("meshQuery:") +
                          std::to_string(dice::hfe::world::IID_ICollisionMesh),
                      "meshAddRef", "release"}),
                  "SimpleObject constructor preserves hook-lookup-map-attach-release ordering");

    RendererDecalPeAbiProbe decal(&events);
    PortalManagerPeAbiProbe portal(&events);
    IncomingOwnerProbe portalComponent("portal", &events, 0,
                                       dice::hfe::world::IID_IPortalPassing, 0, 0);
    IncomingOwnerProbe sectorComponent("sector", &events, 0,
                                       dice::hfe::world::IID_ISector, 0, 0);
    portalComponent.setEventLogging(true, false);
    sectorComponent.setEventLogging(true, false);
    object->setComponent(dice::hfe::world::IID_IPortalPassing, &portalComponent);
    object->setComponent(dice::hfe::world::IID_ISector, &sectorComponent);
    portalComponent.resetActivity();
    sectorComponent.resetActivity();

    events.clear();
    object->installDecal(&decal);
    object->enableAiDestructorPath();

    dice::hfe::IBase* savedPortalManager = dice::hfe::world::portalManager;
    dice::hfe::world::portalManager =
        reinterpret_cast<dice::hfe::IBase*>(&portal);

    object->forceDestroy();
    const int finalReferences = object->release();

    dice::hfe::world::portalManager = savedPortalManager;

    checks.expect(finalReferences == 0,
                  "SimpleObject recovered destructor completes a clean final release");
    checks.expect(decal.m_destroyCalls == 1,
                  "SimpleObject destructor dispatches the opaque renderer decal at PE +0x28");
    checks.expect(portalComponent.m_queryCalls == 1 &&
                      portalComponent.m_lastQuery ==
                          dice::hfe::world::IID_IPortalPassing &&
                      sectorComponent.m_queryCalls == 1 &&
                      sectorComponent.m_lastQuery == dice::hfe::world::IID_ISector,
                  "SimpleObject destructor reaches both portal components through production queryComponent");
    checks.expect(portal.m_portalCalls == 1 && portal.m_sectorCalls == 1 &&
                      portal.m_portalObject == object && portal.m_sectorObject == object,
                  "SimpleObject destructor dispatches both recovered portal-manager unregisters");
    checks.expect(objectTemplate.m_removeResponseCalls == 1 &&
                      objectTemplate.m_removePhysicsCalls == 1,
                  "SimpleObject destructor removes response physics before its physics node");
    checks.expect(events == std::vector<std::string>({
                      "decalDestroy",
                      std::string("query:portal:") +
                          std::to_string(dice::hfe::world::IID_IPortalPassing),
                      "unregisterPortal",
                      std::string("query:sector:") +
                          std::to_string(dice::hfe::world::IID_ISector),
                      "unregisterSector", "removeResponsePhysics",
                      "removePhysicsNode", "release"}) &&
                      portalComponent.m_releaseCalls == 1 &&
                      sectorComponent.m_releaseCalls == 1 &&
                      mesh.m_releaseCalls == 2 && mesh.getRef() == 4,
                  "SimpleObject destructor preserves decal-portal-sector-physics teardown order");
}

void testActualBundleInitAndSoundOrdering(Checks& checks)
{
    std::vector<std::string> events;
    SoundTemplateProbe sound(&events);
    ProbeTemplate childTemplate(&events, &sound);

    HarnessBundleTemplate bundleTemplate;
    bundleTemplate.addTemplate("RuntimePairSoundChild");
    bundleTemplate.cacheTemplate(0, childTemplate);
    Vec3 childPosition = {4.0f, 5.0f, 6.0f};
    bundleTemplate.setPosition(childPosition, 0);

    HarnessBundleTemplate parentTemplate;
    parentTemplate.addTemplate("RuntimePairBundleTransform");
    Vec3 bundlePosition = {70.0f, 80.0f, 90.0f};
    parentTemplate.setPosition(bundlePosition, 0);
    ProbeHost parent(&parentTemplate, 0);

    IObject* object = bundleTemplate.createObject();
    Bundle* bundle = dynamic_cast<Bundle*>(object);
    checks.expect(bundle != 0, "Bundle::init fixture uses a real Bundle factory result");
    checks.expect((bundleTemplate.getFlags() & 0x20u) == 0,
                  "Bundle template init latch starts clear");

    object->setChildId(0);
    parent.addChild(object);
    object->init();

    checks.expect((bundleTemplate.getFlags() & 0x20u) != 0,
                  "Bundle::init executes the SimpleObject/Object init chain");
    checks.expect(childTemplate.m_createCalls == 1,
                  "Bundle::init assembles its template child");
    checks.expect(bundle->getChild() == childTemplate.m_created[0],
                  "Bundle::init links the assembled child to the real Bundle");
    checks.expect(childTemplate.m_created[0]->m_initCalls == 1,
                  "Bundle::init initializes the assembled child exactly once");
    checks.expect(near(childTemplate.floaterMod(), 0.0f),
                  "Bundle::init runs the recovered child-floater shutdown walk");

    checks.expect(sound.m_relativeCalls == 1,
                  "sound child receives one template-relative transform update");
    checks.expect(sound.m_lastPointer == &bundleTemplate.getTransformation(0),
                  "sound child receives the TemplateInfo transform by address");
    checks.expect(near(sound.m_lastRelative.m[12], 4.0f) &&
                  near(sound.m_lastRelative.m[13], 5.0f) &&
                  near(sound.m_lastRelative.m[14], 6.0f),
                  "sound child sees the configured transform value");
    checks.expect(events.size() == 6,
                  "actual Bundle::init emits the complete sound/child event sequence");
    if (events.size() == 6)
    {
        checks.expect(events[0] == "soundRelativeTransformation",
                      "sound transform is set before child creation");
        checks.expect(events[1] == "createObject",
                      "child creation follows the sound-template update");
        checks.expect(events[2] == "setChildId",
                      "actual Bundle::init assigns the child ordinal next");
        checks.expect(events[3] == "setRelativeTransformation",
                      "actual Bundle::init attaches with Object identity first");
        checks.expect(events[4] == "setRelativeTransformation",
                      "actual Bundle::init applies the runtime relative transform");
        checks.expect(events[5] == "init",
                      "actual Bundle::init initializes the child last");
    }

    checks.expect(bundle->getBundleTransformation() ==
                      &parentTemplate.getTransformation(0),
                  "Bundle::init binds its parent TemplateInfo transformation");
    checks.expect(near(bundle->getBundleTransformation()->m[12], 70.0f) &&
                  near(bundle->getBundleTransformation()->m[13], 80.0f) &&
                  near(bundle->getBundleTransformation()->m[14], 90.0f),
                  "bound parent transformation preserves its configured position");

    checks.expect(destroyHeapObject(object) == 0,
                  "initialized Bundle and its assembled child tear down cleanly");
    checks.expect(parent.getChild() == 0,
                  "forced Bundle teardown detaches it from its parent");
    finishStackObject(parent);
}

// Object::makeScript - ELF 0x83093B0 / PE 0x6DCEE0. Exercises the emitted text, the twice-called
// name getter, the reversed-and-negated rotation triple with its epsilon suppression, the single
// write, and the post-write geometry hand-off that discards every result it could observe.
void testObjectMakeScript(Checks& checks)
{
    std::vector<std::string> events;
    CacheLifecycleTemplateProbe objectTemplate("runtime_pair_script", 0, &events);

    // unnamed, unrotated, geometry-free
    ScriptObjectProbe plain(&objectTemplate, &events);
    const Vec3 plainPosition = {1.5f, -2.25f, 3.0f};
    plain.setAbsolutePosition(plainPosition);

    ScriptStreamProbe plainStream(&events);
    events.clear();
    plain.resetScriptCalls();
    const bool plainResult = plain.makeScript(&plainStream);

    std::ostringstream plainExpected;
    plainExpected << "Object.create runtime_pair_script " << std::endl
                  << "Object.absolutePosition " << plainPosition.x << '/' << plainPosition.y << '/'
                  << plainPosition.z << std::endl;

    checks.expect(plainResult, "Object::makeScript returns true for a plain object");
    checks.expect(plainStream.text() == plainExpected.str(),
                  "Object::makeScript emits the create and absolutePosition lines only");
    checks.expect(plainStream.text().find("runtime_pair_script \n") != std::string::npos,
                  "Object::makeScript keeps the create line's trailing space before the newline");
    checks.expect(plainStream.writeCalls() == 1,
                  "Object::makeScript writes the whole script in one call");
    checks.expect(plainStream.text().find('\0') == std::string::npos &&
                      plainStream.text().size() == plainExpected.str().size(),
                  "Object::makeScript writes the text length and excludes the NUL");
    checks.expect(plain.m_nameCalls == 1,
                  "an empty object name costs exactly one getName dispatch");
    checks.expect(plain.m_positionCalls == 1 && plain.m_transformCalls == 1,
                  "Object::makeScript queries position and transformation once each");
    checks.expect(events == std::vector<std::string>({
                      "template:getName:runtime_pair_script", "object:getName", "stream:write"}),
                  "Object::makeScript reads the template name first and writes last");

    // named, rotated, with a console-saveable geometry
    ScriptObjectProbe rich(&objectTemplate, &events);
    rich.setName(std::string("runtime_pair_object"));

    Mat4 rotated = Mat4::getIdentity();
    dice::hfe::setRotationZXY(rotated, 10.0f, 20.0f, 30.0f);
    rotated.m[12] = 4.0f;
    rotated.m[13] = 5.0f;
    rotated.m[14] = 6.0f;
    rich.setAbsoluteTransformation(rotated);

    float zDegrees = 0.0f;
    float xDegrees = 0.0f;
    float yDegrees = 0.0f;
    dice::hfe::getRotationZXY(rich.getAbsoluteTransformation(), zDegrees, xDegrees, yDegrees);

    ScriptSaveableProbe saveable(&events);
    ScriptGeometryProbe geometry(&saveable, &events);
    rich.setRawGeometryForTest(static_cast<dice::hfe::IBase*>(&geometry));

    ScriptStreamProbe richStream(&events);
    richStream.reportShortWrites(true);
    saveable.m_result = false;
    events.clear();
    rich.resetScriptCalls();
    const bool richResult = rich.makeScript(&richStream);

    std::ostringstream richExpected;
    richExpected << "Object.create runtime_pair_script " << std::endl
                 << "Object.name runtime_pair_object" << std::endl
                 << "Object.absolutePosition " << 4.0f << '/' << 5.0f << '/' << 6.0f << std::endl
                 << "Object.rotation " << -yDegrees << '/' << -xDegrees << '/' << -zDegrees
                 << std::endl;

    checks.expect(richStream.text() == richExpected.str(),
                  "Object::makeScript emits name, position and rotation in the recovered order");
    // Reversal and negation are proved against the alternatives rather than against a fixed string:
    // the ZXY getter order and the emitted order differ, and both would "look right" on their own.
    std::ostringstream forwardOrder;
    std::ostringstream unnegated;
    forwardOrder << "Object.rotation " << -zDegrees << '/' << -xDegrees << '/' << -yDegrees;
    unnegated << "Object.rotation " << yDegrees << '/' << xDegrees << '/' << zDegrees;
    checks.expect(near(zDegrees, 10.0f) && near(xDegrees, 20.0f) && near(yDegrees, 30.0f) &&
                      richStream.text().find(forwardOrder.str()) == std::string::npos &&
                      richStream.text().find(unnegated.str()) == std::string::npos,
                  "Object::makeScript reverses and negates the ZXY triple into -Y/-X/-Z");
    checks.expect(rich.m_nameCalls == 2,
                  "a non-empty object name costs two getName dispatches");
    checks.expect(richResult,
                  "Object::makeScript ignores a short write and a false geometry result");
    checks.expect(geometry.m_queryCalls == 1 &&
                      geometry.m_lastIid == dice::hfe::io::IID_IConsoleSaveable,
                  "Object::makeScript queries geometry once for IID_IConsoleSaveable");
    checks.expect(saveable.m_makeScriptCalls == 1 && saveable.m_lastStream == &richStream,
                  "Object::makeScript forwards the same stream to the queried interface");
    checks.expect(geometry.m_releaseCalls == 0,
                  "Object::makeScript never releases the geometry it queried");
    checks.expect(events == std::vector<std::string>({
                      "template:getName:runtime_pair_script", "object:getName", "object:getName",
                      "stream:write", "geometry:query", "geometry:makeScript"}),
                  "geometry is queried and dispatched only after the object's own write");

    // geometry that does not answer the interface
    ScriptGeometryProbe mute(0, &events);
    rich.setRawGeometryForTest(static_cast<dice::hfe::IBase*>(&mute));
    ScriptStreamProbe muteStream(&events);
    const bool muteResult = rich.makeScript(&muteStream);

    checks.expect(muteResult && mute.m_queryCalls == 1 && saveable.m_makeScriptCalls == 1,
                  "a geometry without the console interface is queried, skipped, and still true");
    checks.expect(muteStream.text() == richExpected.str(),
                  "the emitted text does not depend on the geometry hand-off");

    rich.setRawGeometryForTest(0);
    finishStackObject(plain);
    finishStackObject(rich);
}

// SimpleObject::initNetworkableComp (ELF 0x81EEE60 / PE 0x6152A0) over the shared
// world::setNetworkableComp (ELF 0x8325A70 / PE 0x6DEF30). Covers both gate layers, the four
// true-without-a-component exits, the hard failure on a missing info manager, the half-built
// unwind, and the info/base-priority pair that setNetworkableInfo writes.
void testSimpleObjectNetworkableComp(Checks& checks)
{
    std::vector<std::string> events;

    NetworkableInfoProbe info(&events, 7.25f);
    NetworkableInfoManagerProbe infoManager(&events, &info);
    NetworkableProbe networkable(&events);
    ClassManagerPeAbiProbe classManagerProbe(&events, 0);
    classManagerProbe.setCreatedInstance(static_cast<dice::hfe::IBase*>(&networkable));

    dice::hfe::ClassManager* savedClassManager = dice::hfe::classManager;
    dice::hfe::IBase* savedInfoManager = dice::hfe::io::networkableInfoManager;
    dice::hfe::classManager = reinterpret_cast<dice::hfe::ClassManager*>(&classManagerProbe);
    dice::hfe::io::networkableInfoManager =
        static_cast<dice::hfe::IBase*>(
            static_cast<dice::hfe::io::INetworkableInfoManager*>(&infoManager));

    // an empty networkable-info name never reaches the helper
    NetworkableTemplateProbe silentTemplate(std::string(), &events);
    SimpleObject silent(&silentTemplate);
    events.clear();
    silent.initNetworkableComp();

    checks.expect(silentTemplate.m_infoCalls == 1 && classManagerProbe.m_createCalls == 0,
                  "an empty networkable-info name stops in the override, before the helper");

    // the pre-cache gate wins over a perfectly good name
    NetworkableTemplateProbe objectTemplate(std::string("SimpleObjectNetworkable"), &events);
    SimpleObject precaching(&objectTemplate);
    dice::hfe::world::ObjectTemplate::setIsPreCaching(true);
    events.clear();
    objectTemplate.m_infoCalls = 0;
    precaching.initNetworkableComp();
    dice::hfe::world::ObjectTemplate::setIsPreCaching(false);

    checks.expect(objectTemplate.m_infoCalls == 1 && classManagerProbe.m_createCalls == 0,
                  "pre-caching suppresses the component before any instance is created");

    // the full path
    NetworkableSimpleObjectProbe host(&objectTemplate);
    events.clear();
    objectTemplate.m_infoCalls = 0;
    host.initNetworkableComp();

    checks.expect(classManagerProbe.m_createCalls == 1 &&
                      classManagerProbe.m_lastClassId ==
                          dice::hfe::CID_SimpleObjectNetworkable &&
                      classManagerProbe.m_lastIid == dice::hfe::io::IID_INetworkable &&
                      classManagerProbe.m_lastOuter == 0,
                  "SimpleObject asks for its own networkable class through IID_INetworkable");
    checks.expect(networkable.m_initCalls == 1 &&
                      networkable.m_lastInitArgument ==
                          static_cast<void*>(static_cast<IObject*>(&host)),
                  "the created networkable is initialized with the owning object");
    checks.expect(objectTemplate.m_infoCalls == 3,
                  "the networkable-info name is re-read for the lookup, never cached");
    checks.expect(infoManager.m_lookupCalls == 1 &&
                      infoManager.m_lookupNames.size() == 1 &&
                      infoManager.m_lookupNames[0] == "SimpleObjectNetworkable",
                  "the info lookup uses the template's networkable-info name");
    checks.expect(networkable.installedInfo() ==
                      static_cast<const dice::hfe::io::INetworkableInfo*>(&info) &&
                      near(networkable.installedBasePriority(), 7.25f) &&
                      info.m_basePriorityCalls == 1,
                  "setNetworkableInfo stores the policy and seeds the base priority from it");
    checks.expect(host.networkableFastField() == static_cast<void*>(&networkable),
                  "the networkable is attached to the object as its replication component");
    checks.expect(networkable.m_releaseCalls == 1,
                  "the helper drops its own reference once setComponent has taken one");
    checks.expect(events == std::vector<std::string>({
                      "template:getNetworkableInfo", "template:getNetworkableInfo",
                      "classManager:createInstance", "networkable:init",
                      "template:getNetworkableInfo",
                      "infoManager:getInfo:SimpleObjectNetworkable", "info:getBasePriority",
                      "networkable:release"}),
                  "create, init, look up, then attach - the recovered order");

    // a missing NetworkableInfoManager is a hard failure
    ObjectComponentRuntimeProbe orphan(&objectTemplate);
    dice::hfe::io::networkableInfoManager = 0;
    events.clear();
    const bool missingManager =
        dice::hfe::world::setNetworkableComp(&orphan,
                                             dice::hfe::CID_SimpleObjectNetworkable);
    dice::hfe::io::networkableInfoManager =
        static_cast<dice::hfe::IBase*>(
            static_cast<dice::hfe::io::INetworkableInfoManager*>(&infoManager));

    checks.expect(!missingManager && classManagerProbe.m_createCalls == 1,
                  "a missing info manager fails without creating anything");
    checks.expect(orphan.fastField(dice::hfe::io::IID_INetworkable) == 0,
                  "the failed object keeps no replication component");

    // a half-built component is unwound, not attached
    NetworkableProbe orphanNetworkable(&events);
    classManagerProbe.setCreatedInstance(static_cast<dice::hfe::IBase*>(&orphanNetworkable));
    infoManager.setInfo(0);
    ObjectComponentRuntimeProbe unwound(&objectTemplate);
    events.clear();
    const bool missingInfo =
        dice::hfe::world::setNetworkableComp(&unwound,
                                             dice::hfe::CID_SimpleObjectNetworkable);
    infoManager.setInfo(&info);

    checks.expect(!missingInfo && orphanNetworkable.m_initCalls == 1 &&
                      orphanNetworkable.m_releaseCalls == 1,
                  "an unknown info name releases the networkable it already created");
    checks.expect(orphanNetworkable.installedInfo() == 0 &&
                      unwound.fastField(dice::hfe::io::IID_INetworkable) == 0,
                  "the unwound networkable has its policy cleared and is never attached");
    checks.expect(near(orphanNetworkable.installedBasePriority(), 1.0f),
                  "clearing the policy leaves the constructor's base priority untouched");

    // a template-less object is a success that attaches nothing
    ObjectComponentRuntimeProbe templateless(0);
    const bool noTemplate =
        dice::hfe::world::setNetworkableComp(&templateless,
                                             dice::hfe::CID_SimpleObjectNetworkable);
    checks.expect(noTemplate && templateless.fastField(dice::hfe::io::IID_INetworkable) == 0,
                  "no template reports success while attaching nothing");

    dice::hfe::io::networkableInfoManager = savedInfoManager;
    dice::hfe::classManager = savedClassManager;

    finishStackObject(silent);
    finishStackObject(precaching);
    finishStackObject(host);
    finishStackObject(orphan);
    finishStackObject(unwound);
    finishStackObject(templateless);
}

// Records Bundle's broadcast and supplies a fixed parent radius after detachment.
// Checks the launch formula and single radius query independently of cache invalidation.
class BundleLaunchParentProbe : public ObjectComponentRuntimeProbe
{
public:
    BundleLaunchParentProbe(const IObjectTemplate* objectTemplate, float radius)
        : ObjectComponentRuntimeProbe(objectTemplate), m_radiusCalls(0), m_radius(radius) {}

    virtual float getPhysicBoundingRadius() const
    {
        ++m_radiusCalls;
        return m_radius;
    }

    mutable int m_radiusCalls;

private:
    float m_radius;
};

// The launching bundle reports a fixed absolute position for the same reason its parent reports a
// fixed radius: the detach that precedes the direction calculation invalidates Object's transform
// caches, and the point of the test is the formula, not the cache.
class BundleLaunchProbe : public Bundle
{
public:
    BundleLaunchProbe(const BundleTemplate* objectTemplate, const Vec3& position)
        : Bundle(objectTemplate), m_positionCalls(0), m_position(position) {}

    virtual const Vec3& getAbsolutePosition() const
    {
        ++m_positionCalls;
        return m_position;
    }

    void setRawPhysicsForTest(void* physicsNode) { m_physicsNode = physicsNode; }
    // The armor is installed as a raw fast field rather than through setComponent: the gate reads
    // m_armorComponent directly in both ports, and this keeps component ownership out of the test.
    void setRawArmorForTest(dice::hfe::world::IObjectComponent* armor)
    {
        m_armorComponent = armor;
    }
    unsigned int flagsForTest() const { return m_flags; }

    mutable int m_positionCalls;

private:
    Vec3 m_position;
};

class BundleMessageChildProbe : public SimpleObject
{
public:
    BundleMessageChildProbe(const SimpleObjectTemplate* objectTemplate,
                            std::vector<std::string>* events)
        : m_messageCalls(0),
          m_lastMessage(static_cast<dice::hfe::world::TemplateMessage>(0)),
          m_lastPlayer(0), m_lastMessageFlag(false),
          SimpleObject(objectTemplate), m_events(events) {}

    virtual void handleMessage(dice::hfe::world::TemplateMessage message,
                               dice::hfe::world::IPlayer* player, bool flag)
    {
        if (m_events) m_events->push_back("child:handleMessage");
        ++m_messageCalls;
        m_lastMessage = message;
        m_lastPlayer = player;
        m_lastMessageFlag = flag;
    }

    int m_messageCalls;
    dice::hfe::world::TemplateMessage m_lastMessage;
    dice::hfe::world::IPlayer* m_lastPlayer;
    bool m_lastMessageFlag;

private:
    std::vector<std::string>* m_events;
};

// Bundle::handleMessage - ELF 0x81D7570 / PE 0x6143A0. The unconditional child broadcast, the
// four-deep launch gate, and the detach/direction/speed sequence that throws a bundle member off
// its parent.
void testBundleHandleMessage(Checks& checks)
{
    std::vector<std::string> events;
    HarnessBundleTemplate bundleTemplate;
    FactorySimpleTemplate parentTemplate;
    FactorySimpleTemplate childTemplate;

    BundleLaunchParentProbe parent(&parentTemplate, 4.0f);
    const Vec3 bundlePosition = {3.0f, 0.0f, 0.0f};
    BundleLaunchProbe bundle(&bundleTemplate, bundlePosition);
    BundleMessageChildProbe firstChild(&childTemplate, &events);
    BundleMessageChildProbe secondChild(&childTemplate, &events);
    parent.addChild(&bundle);
    bundle.addChild(&firstChild);
    bundle.addChild(&secondChild);

    PhysicsNodeSleepingPeAbiProbe physics("bundlePhysics", false, &events);
    bundle.setRawPhysicsForTest(&physics);

    ArmorPeAbiProbe armor(10.0f, &events);
    bundle.setRawArmorForTest(&armor);

    // the broadcast happens whatever the message is
    events.clear();
    bundle.handleMessage(static_cast<dice::hfe::world::TemplateMessage>(7), 0, true);

    checks.expect(firstChild.m_messageCalls == 1 && secondChild.m_messageCalls == 1 &&
                      firstChild.m_lastMessage ==
                          static_cast<dice::hfe::world::TemplateMessage>(7) &&
                      firstChild.m_lastMessageFlag,
                  "Bundle::handleMessage broadcasts the unchanged tuple to every direct child");
    checks.expect(armor.m_sendingCalls == 0 && physics.m_positionalSpeedCalls == 0,
                  "an ordinary message never reaches the launch gate");

    // message 24 without the template flag is still not a launch
    armor.m_sending = true;
    events.clear();
    bundle.handleMessage(static_cast<dice::hfe::world::TemplateMessage>(24), 0, false);

    checks.expect(armor.m_sendingCalls == 0 && bundle.getParent() ==
                      static_cast<IObject*>(&parent),
                  "the template flag is tested before the armor is ever consulted");

    // the flag and the message, but the armor is not sending
    bundleTemplate.setLaunchOnMessageForTest(true);
    armor.m_sending = false;
    events.clear();
    bundle.handleMessage(static_cast<dice::hfe::world::TemplateMessage>(24), 0, false);

    checks.expect(armor.m_sendingCalls == 1 && physics.m_positionalSpeedCalls == 0 &&
                      bundle.getParent() == static_cast<IObject*>(&parent),
                  "a silent armor consumes the gate without detaching anything");

    // the full launch
    armor.m_sending = true;

    events.clear();
    firstChild.m_messageCalls = 0;
    bundle.handleMessage(static_cast<dice::hfe::world::TemplateMessage>(24), 0, false);

    checks.expect(bundle.getParent() == 0 && parent.getChild() == 0,
                  "the launching member detaches itself from its parent");
    checks.expect((bundle.flagsForTest() & 0x800u) != 0,
                  "the launching member sets flag 0x800 before detaching");
    checks.expect(physics.m_mobileCalls == 1 && physics.m_positionalSpeedCalls == 1 &&
                      physics.m_rotationalSpeedCalls == 1,
                  "the launch asks the node whether it is mobile, then sets both speeds");
    // Root is the parent's root, taken after the detach: parent sits at the origin with a physical
    // radius of 4, so the direction is (3, 8, 0) normalised and scaled by 10.
    checks.expect(near(physics.m_lastPositionalSpeed.x, 30.0f / std::sqrt(73.0f)) &&
                      near(physics.m_lastPositionalSpeed.y, 80.0f / std::sqrt(73.0f)) &&
                      near(physics.m_lastPositionalSpeed.z, 0.0f),
                  "the launch direction is (position + twice the root radius up) - root position");
    checks.expect(near(physics.m_lastRotationalSpeed.x, 0.0f) &&
                      near(physics.m_lastRotationalSpeed.y, 0.0f) &&
                      near(physics.m_lastRotationalSpeed.z, 0.0f),
                  "the launch stops the member spinning");
    checks.expect(firstChild.m_messageCalls == 1 && secondChild.m_messageCalls == 4 && parent.m_radiusCalls == 1,
                  "the children were told before the launch decision was taken");

    // a bundle with no parent stops after the flag
    events.clear();
    physics.m_positionalSpeedCalls = 0;
    physics.m_mobileCalls = 0;
    bundle.handleMessage(static_cast<dice::hfe::world::TemplateMessage>(24), 0, false);

    checks.expect(physics.m_mobileCalls == 0 && physics.m_positionalSpeedCalls == 0,
                  "a parentless bundle sets the flag and does nothing else");

    // Teardown, in the order the fixtures allow. The launch already detached the bundle from its
    // parent, so only the two children are still linked; unlinking them by hand keeps
    // finishStackObject from releasing stack objects to a zero refcount. The raw physics and armor
    // fields must be cleared too - both probes are declared after the bundle and therefore die
    // first, and ~SimpleObject dispatches through the physics node.
    bundle.removeChild(&firstChild);
    bundle.removeChild(&secondChild);
    finishStackObject(firstChild);
    finishStackObject(secondChild);
    bundle.setRawPhysicsForTest(0);
    bundle.setRawArmorForTest(0);
    finishStackObject(bundle);
    finishStackObject(parent);
}

// SimpleObjectTemplate keeps the whole animatedUV* family protected, so the fixture reaches them
// from inside the class rather than the test opening them up.
class AnimatedUVTemplateProbe : public FactorySimpleTemplate
{
public:
    explicit AnimatedUVTemplateProbe(const std::string& name) { m_templateName = name; }

    void configureRotateAs(const std::string& sourceName, bool reverse)
    {
        m_rotateAsAnimatedUV = true;
        m_rotateAsAnimatedUVObject = sourceName;
        m_rotateAsAnimatedUVReverse = reverse;
    }

    void configureTranslation(float sizeX, float sizeY, float speedX, float speedY,
                              float maxX, float maxY, int index, bool reverse)
    {
        m_animatedUVTranslation = true;
        m_animatedUVTranslationSize.x = sizeX;
        m_animatedUVTranslationSize.y = sizeY;
        m_animatedUVTranslationSpeed.x = speedX;
        m_animatedUVTranslationSpeed.y = speedY;
        m_animatedUVTranslationMax.x = maxX;
        m_animatedUVTranslationMax.y = maxY;
        m_animatedUVTranslationIndex = index;
        m_animatedUVTranslationReverse = reverse;
    }

    void configureRotation(float radius, int index, float scaleX, float scaleY, bool reverse)
    {
        m_animatedUVRotation = true;
        m_animatedUVRotationRadius = radius;
        m_animatedUVRotationIndex = index;
        m_animatedUVRotationScale.x = scaleX;
        m_animatedUVRotationScale.y = scaleY;
        m_animatedUVRotationReverse = reverse;
    }
};

// A SimpleObject whose animated-UV block the test can read and seed, plus a fixed absolute position
// so the travel scalar is exactly what the test set up.
class AnimatedUVObjectProbe : public SimpleObject
{
public:
    AnimatedUVObjectProbe(const SimpleObjectTemplate* objectTemplate, const Vec3& position)
        : SimpleObject(objectTemplate), m_position(position) {}

    void setTestPosition(const Vec3& position) { m_position = position; }
    virtual const Vec3& getAbsolutePosition() const { return m_position; }

    AnimatedUV* animatedUVForTest() const { return m_animatedUV; }
    void setRawGeometryForTest(void* geometry) { m_geometry = geometry; }

private:
    Vec3 m_position;
};

// setRotateXDeg (ELF 0x83B6120), world::findObjectWithName (ELF 0x831C9D0) and
// SimpleObject::handleVisualUpdate (ELF 0x81EF1F0 / PE 0x614880).
void testSimpleObjectVisualUpdate(Checks& checks)
{
    // setRotateXDeg
    Mat4 rotation = Mat4::getIdentity();
    rotation.m[12] = 5.0f;
    dice::hfe::setRotateXDeg(rotation, 0.0f);
    checks.expect(near(rotation.m[12], 0.0f) && near(rotation.m[0], 1.0f) &&
                      near(rotation.m[5], 1.0f) && near(rotation.m[15], 1.0f),
                  "setRotateXDeg writes the whole identity and discards the translation");

    dice::hfe::setRotateXDeg(rotation, 90.0f);
    checks.expect(near(rotation.m[5], 0.0f) && near(rotation.m[6], 1.0f) &&
                      near(rotation.m[9], -1.0f) && near(rotation.m[10], 0.0f),
                  "setRotateXDeg puts +sin at m[6] and -sin at m[9], transposing setRotationZXY");

    // findObjectWithName
    std::vector<std::string> events;
    CacheLifecycleTemplateProbe rootTemplate("root", 0, 0);
    CacheLifecycleTemplateProbe branchTemplate("branch", 0, 0);
    CacheLifecycleTemplateProbe targetTemplate("target", 0, 0);

    ObjectComponentRuntimeProbe searchRoot(&rootTemplate);
    ObjectComponentRuntimeProbe firstBranch(&branchTemplate);
    ObjectComponentRuntimeProbe secondBranch(&branchTemplate);
    ObjectComponentRuntimeProbe deepTarget(&targetTemplate);
    ObjectComponentRuntimeProbe siblingTarget(&targetTemplate);
    searchRoot.addChild(&firstBranch);
    searchRoot.addChild(&secondBranch);
    firstBranch.addChild(&deepTarget);
    secondBranch.addChild(&siblingTarget);

    // firstBranch's own child and its sibling's child both match. Sibling-before-child means the
    // sibling's subtree wins, which a conventional depth-first search would get backwards.
    IObject* found = dice::hfe::world::findObjectWithName(&firstBranch, std::string("target"));
    checks.expect(found == static_cast<IObject*>(&siblingTarget),
                  "findObjectWithName searches the sibling subtree before its own child");
    checks.expect(dice::hfe::world::findObjectWithName(&searchRoot, std::string("root")) ==
                      static_cast<IObject*>(&searchRoot),
                  "findObjectWithName matches the object it was handed first");
    checks.expect(dice::hfe::world::findObjectWithName(&searchRoot, std::string("absent")) == 0,
                  "findObjectWithName returns null when no template name matches");

    // rotate as animated UV
    AnimatedUVTemplateProbe sourceTemplate("uvSource");
    sourceTemplate.configureRotation(1.0f, 0, 1.0f, 1.0f, false);

    AnimatedUVTemplateProbe followerTemplate("uvFollower");
    followerTemplate.configureRotateAs("uvSource", false);

    const Vec3 origin = {0.0f, 0.0f, 0.0f};
    ObjectComponentRuntimeProbe hierarchyRoot(&rootTemplate);
    AnimatedUVObjectProbe source(&sourceTemplate, origin);
    AnimatedUVObjectProbe follower(&followerTemplate, origin);
    hierarchyRoot.addChild(&source);
    hierarchyRoot.addChild(&follower);

    source.animatedUVForTest()->m_rotation = 6.2831855f / 4.0f;   // a quarter turn
    Mat4 followerPlacement = Mat4::getIdentity();
    followerPlacement.m[12] = 2.0f;
    followerPlacement.m[13] = 3.0f;
    followerPlacement.m[14] = 4.0f;
    follower.setRelativeTransformation(followerPlacement);

    follower.handleVisualUpdate(0.0f, 0.0f);

    const Mat4& applied = follower.getRelativeTransformation();
    checks.expect(follower.animatedUVForTest()->m_rotateSource ==
                      static_cast<IObject*>(&source),
                  "rotate-as-animated-UV resolves and caches its source by template name");
    checks.expect(near(applied.m[6], 1.0f) && near(applied.m[9], -1.0f),
                  "the follower takes a pure X rotation of the source's UV phase in degrees");
    checks.expect(near(applied.m[12], 2.0f) && near(applied.m[13], 3.0f) &&
                      near(applied.m[14], 4.0f),
                  "the follower's own relative position survives the replacement");

    // the UV animation proper
    // Channel 2 is disabled by a zero size even though its speed is nonzero.
    AnimatedUVTemplateProbe uvTemplate("uvMember");
    uvTemplate.configureTranslation(2.0f, 0.0f, 0.25f, 0.5f, 100.0f, 0.0f, 3, false);

    GeometryPeAbiProbe rootGeometry;
    std::vector<GeometryPartCall> geometryCalls;
    rootGeometry.m_calls = &geometryCalls;

    ObjectComponentRuntimeProbe uvRoot(&rootTemplate);
    uvRoot.setRawGeometryForTest(&rootGeometry);
    const Vec3 start = {0.0f, 0.0f, 10.0f};
    AnimatedUVObjectProbe member(&uvTemplate, start);
    uvRoot.addChild(&member);

    member.handleVisualUpdate(0.0f, 0.0f);

    checks.expect(rootGeometry.m_uvTranslationCalls == 1 && rootGeometry.m_lastUVIndex == 3,
                  "the animated-UV update drives the ROOT's geometry, not the member's own");
    checks.expect(!member.animatedUVForTest()->m_firstSample &&
                      near(member.animatedUVForTest()->m_translation[0], 0.25f),
                  "the first update contributes speed only - its travel delta is discarded");
    checks.expect(near(member.animatedUVForTest()->m_translation[1], 0.0f),
                  "a zero size leaves that channel's accumulator untouched");
    checks.expect(near(member.animatedUVForTest()->m_lastPosition.z, 10.0f),
                  "the update stores the position it ends on");

    // Move four units along +Z. The member's absolute transform is the identity, so the travel
    // scalar is -length(delta) * dot(zAxis, normalise(delta)) = -4, and channel 1 divides by 2.
    const Vec3 moved = {0.0f, 0.0f, 14.0f};
    member.setTestPosition(moved);
    member.handleVisualUpdate(0.0f, 0.0f);

    // 0.25 from the first update, then travel -4 divided by size 2, then the speed again: -1.5.
    // The wrap then lifts it into [0, max] by adding 100 - the accumulator is never left negative.
    checks.expect(near(member.animatedUVForTest()->m_translation[0], 98.5f),
                  "travel is the negated distance on the object's Z axis, then wrapped into [0,max]");
    checks.expect(rootGeometry.m_uvTranslationCalls == 2 &&
                      near(rootGeometry.m_lastUVTranslation[0], 98.5f) &&
                      near(rootGeometry.m_lastUVTranslation[1], 0.0f),
                  "both accumulators are handed to the geometry every update");

    // rotation channel
    AnimatedUVTemplateProbe spinTemplate("uvSpinner");
    spinTemplate.configureRotation(1.0f, 9, 2.0f, 4.0f, false);

    AnimatedUVObjectProbe spinner(&spinTemplate, start);
    uvRoot.addChild(&spinner);
    spinner.animatedUVForTest()->m_rotation = 6.2831855f / 4.0f;
    spinner.handleVisualUpdate(0.0f, 0.0f);

    checks.expect(rootGeometry.m_uvRotationCalls == 1 && rootGeometry.m_lastUVRotationIndex == 9,
                  "the rotation channel reaches the geometry with its own index");
    checks.expect(near(rootGeometry.m_lastUVRotation.m[0], 0.0f) &&
                      near(rootGeometry.m_lastUVRotation.m[1], -4.0f) &&
                      near(rootGeometry.m_lastUVRotation.m[4], 2.0f) &&
                      near(rootGeometry.m_lastUVRotation.m[5], 0.0f),
                  "the UV rotation matrix scales column 0 by scale.x and column 1 by scale.y");

    uvRoot.removeChild(&member);
    uvRoot.removeChild(&spinner);
    finishStackObject(member);
    finishStackObject(spinner);
    uvRoot.setRawGeometryForTest(0);
    finishStackObject(uvRoot);

    hierarchyRoot.removeChild(&source);
    hierarchyRoot.removeChild(&follower);
    finishStackObject(source);
    finishStackObject(follower);
    finishStackObject(hierarchyRoot);

    searchRoot.removeChild(&firstBranch);
    searchRoot.removeChild(&secondBranch);
    firstBranch.removeChild(&deepTarget);
    secondBranch.removeChild(&siblingTarget);
    finishStackObject(deepTarget);
    finishStackObject(siblingTarget);
    finishStackObject(firstBranch);
    finishStackObject(secondBranch);
    finishStackObject(searchRoot);
}

// A SimpleObject that can claim to be a projectile, so the two game-logic dispatches can be told
// apart without inventing a projectile class.
class ProjectileClaimingObjectProbe : public SimpleObject
{
public:
    ProjectileClaimingObjectProbe(const SimpleObjectTemplate* objectTemplate, bool projectile)
        : SimpleObject(objectTemplate), m_projectile(projectile) {}

    virtual void* queryInterface(unsigned int iid) const
    {
        if (m_projectile && iid == dice::hfe::world::IID_IProjectileObject)
            return const_cast<ProjectileClaimingObjectProbe*>(this);
        return SimpleObject::queryInterface(iid);
    }

    void setRawArmorForTest(dice::hfe::world::IObjectComponent* armor)
    {
        m_armorComponent = armor;
    }

private:
    bool m_projectile;
};

// SimpleObject::handleCollision - ELF 0x81EEEC0 / PE 0x6152E0. The Bundle wrapper test already
// covers the material veto and the swapped non-projectile dispatch; this covers the projectile
// route, the armor-less route, and the fact that the boolean is not an early exit.
void testSimpleObjectHandleCollision(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    FactorySimpleTemplate otherTemplate;

    CollisionMaterialPeAbiProbe absorbing(true);
    MaterialManagerPeAbiProbe materials;
    GameLogicCollisionPeAbiProbe logic;
    ArmorPeAbiProbe armor(1.0f, 0);
    armor.m_collides = true;

    dice::hfe::IBase* savedMaterials = dice::hfe::world::materialManager;
    dice::hfe::IGameLogic* savedLogic = dice::hfe::gameLogic;
    dice::hfe::world::materialManager = reinterpret_cast<dice::hfe::IBase*>(&materials);
    dice::hfe::gameLogic = reinterpret_cast<dice::hfe::IGameLogic*>(&logic);

    const Vec3 first = {1.0f, 2.0f, 3.0f};
    const Vec3 second = {4.0f, 5.0f, 6.0f};
    const Vec3 third = {7.0f, 8.0f, 9.0f};
    dice::hfe::world::CollisionFaceInfo* firstFace =
        reinterpret_cast<dice::hfe::world::CollisionFaceInfo*>(&armor);
    dice::hfe::world::CollisionFaceInfo* secondFace =
        reinterpret_cast<dice::hfe::world::CollisionFaceInfo*>(&logic);

    // a projectile with armor: everything is passed through untouched
    ProjectileClaimingObjectProbe projectile(&objectTemplate, true);
    SimpleObject other(&otherTemplate);
    projectile.setRawArmorForTest(&armor);

    const bool projectileResult = projectile.handleCollision(&other, first, second, third, 11, 22,
                                                             firstFace, secondFace);

    checks.expect(projectileResult && materials.m_lookupCalls == 1 && materials.m_lastId == 22,
                  "the SECOND int is the material id the manager is asked for");
    checks.expect(logic.m_projectileCalls == 1 && logic.m_collisionCalls == 0 &&
                      logic.m_lastProjectile.first == static_cast<IObject*>(&projectile) &&
                      logic.m_lastProjectile.second == static_cast<IObject*>(&other) &&
                      near(logic.m_lastProjectile.firstVector.x, 1.0f) &&
                      logic.m_lastProjectile.firstInt == 11 &&
                      logic.m_lastProjectile.secondInt == 22 &&
                      logic.m_lastProjectile.firstFace == firstFace &&
                      logic.m_lastProjectile.secondFace == secondFace,
                  "the projectile route hands the game logic every argument unchanged");
    checks.expect(armor.m_heightCalls == 1,
                  "armor is told the colliding object's height after the dispatch");

    // an absorbing material vetoes the result but does not stop the rest
    materials.m_material = &absorbing;
    armor.m_heightCalls = 0;
    logic.m_projectileCalls = 0;

    const bool vetoed = projectile.handleCollision(&other, first, second, third, 11, 22,
                                                   firstFace, secondFace);

    checks.expect(!vetoed && absorbing.m_absorbCalls == 1,
                  "an absorbing material turns the result false");
    checks.expect(logic.m_projectileCalls == 1 && armor.m_heightCalls == 1,
                  "the false result is not an early exit - the armor path still runs");

    // no armor anywhere on the chain
    projectile.setRawArmorForTest(0);
    materials.m_material = 0;
    armor.m_heightCalls = 0;
    logic.m_projectileCalls = 0;
    armor.m_collideCalls = 0;

    const bool unarmored = projectile.handleCollision(&other, first, second, third, 11, 22,
                                                      firstFace, secondFace);

    checks.expect(unarmored && logic.m_projectileCalls == 1 && armor.m_collideCalls == 0 &&
                      armor.m_heightCalls == 0,
                  "without armor only the projectile dispatch happens");

    // a non-projectile without armor reports nothing at all
    ProjectileClaimingObjectProbe ordinary(&objectTemplate, false);
    logic.m_projectileCalls = 0;
    logic.m_collisionCalls = 0;

    const bool silent = ordinary.handleCollision(&other, first, second, third, 11, 22,
                                                 firstFace, secondFace);

    checks.expect(silent && logic.m_projectileCalls == 0 && logic.m_collisionCalls == 0,
                  "an armour-less non-projectile collision is never reported to the game logic");

    dice::hfe::gameLogic = savedLogic;
    dice::hfe::world::materialManager = savedMaterials;

    finishStackObject(projectile);
    finishStackObject(ordinary);
    finishStackObject(other);
}

// ObjectTemplate is abstract, so the fixture derives the nearest concrete template and calls the
// base emitter qualified - which is exactly how every subclass emitter reaches it.
class ScriptTemplateProbe : public FactorySimpleTemplate
{
public:
    ScriptTemplateProbe(const std::string& name, unsigned int flags)
    {
        m_templateName = name;
        m_flags = flags;
    }

    void setNetworkableInfoForTest(const std::string& info) { m_networkableInfo = info; }
};

// ObjectTemplate::makeScript - ELF 0x8321410 / PE 0x6D9FB0, the base of the template .con-writer
// family.
void testObjectTemplateMakeScript(Checks& checks)
{
    std::vector<std::string> events;
    TemplateManagerLookupProbe templateNames;
    templateNames.m_className = "SimpleObjectTemplate";
    ClassManagerPeAbiProbe classManagerProbe(0, 0);
    classManagerProbe.m_className = "world.simple.MyComponentTemplate";

    dice::hfe::world::ObjectTemplateManager* savedManager =
        dice::hfe::world::objectTemplateManager;
    dice::hfe::ClassManager* savedClassManager = dice::hfe::classManager;
    dice::hfe::world::objectTemplateManager =
        reinterpret_cast<dice::hfe::world::ObjectTemplateManager*>(&templateNames);
    dice::hfe::classManager = reinterpret_cast<dice::hfe::ClassManager*>(&classManagerProbe);

    // an ordinary template: positive flags mean activeSafe, not create
    ScriptTemplateProbe ordinary("MyTemplate", 0);
    ScriptStreamProbe ordinaryStream(&events);
    const bool ordinaryResult = ordinary.dice::hfe::world::ObjectTemplate::makeScript(&ordinaryStream);

    std::ostringstream ordinaryExpected;
    ordinaryExpected << "ObjectTemplate.activeSafe SimpleObjectTemplate MyTemplate" << std::endl;

    checks.expect(ordinaryResult && ordinaryStream.text() == ordinaryExpected.str(),
                  "a template with positive flags emits activeSafe, not create");
    checks.expect(templateNames.m_classNameCalls == 1,
                  "the class name comes from the template manager's id lookup");

    // create needs bit 7 (0x80, createdInEditor) set with bits 0 and 10 clear
    ScriptTemplateProbe editorMade("EditorTemplate", 0x80u | 0x1000u);
    editorMade.setNetworkableInfoForTest("SoldierNetworkable");
    ScriptStreamProbe editorStream(&events);
    const bool editorResult = editorMade.dice::hfe::world::ObjectTemplate::makeScript(&editorStream);

    std::ostringstream editorExpected;
    editorExpected << "ObjectTemplate.create SimpleObjectTemplate EditorTemplate" << std::endl
                   << "ObjectTemplate.setNetworkableInfo SoldierNetworkable" << std::endl
                   << "ObjectTemplate.createdInEditor 1" << std::endl
                   << "ObjectTemplate.hasRelativeOffset 1" << std::endl;

    checks.expect(editorResult && editorStream.text() == editorExpected.str(),
                  "create needs bit 7 (0x80) set with bits 0 and 10 clear, and the flag lines follow");
    checks.expect(editorStream.text().find("createdInEditor 1") != std::string::npos &&
                      editorStream.text().find("createdInEditor 0") == std::string::npos,
                  "the boolean lines are hardcoded to 1 and only appear when their bit is set");

    // bit 31 alone is NOT "created in editor": both ports test the sign of AL, bit 7
    ScriptTemplateProbe highBit("HighBitTemplate", 0x80000000u);
    ScriptStreamProbe highBitStream(&events);
    const bool highBitResult = highBit.dice::hfe::world::ObjectTemplate::makeScript(&highBitStream);

    std::ostringstream highBitExpected;
    highBitExpected << "ObjectTemplate.activeSafe SimpleObjectTemplate HighBitTemplate" << std::endl;

    checks.expect(highBitResult && highBitStream.text() == highBitExpected.str(),
                  "bit 31 alone still emits activeSafe and no createdInEditor line");

    // a short primary write stops before any component
    ScriptTemplateProbe withComponent("ComponentTemplate", 0);

    ScriptStreamProbe shortStream(&events);
    shortStream.reportShortWrites(true);
    const bool shortResult = withComponent.dice::hfe::world::ObjectTemplate::makeScript(&shortStream);

    checks.expect(!shortResult && classManagerProbe.m_classNameCalls == 0,
                  "a short primary write returns false without traversing any component");

    // the derived emitter chains onto this one
    ScriptTemplateProbe chained("ChainedTemplate", 0);
    ScriptStreamProbe chainedStream(&events);
    const bool chainedResult = chained.makeScript(&chainedStream);

    std::ostringstream chainedBase;
    chainedBase << "ObjectTemplate.activeSafe SimpleObjectTemplate ChainedTemplate" << std::endl;

    checks.expect(chainedResult &&
                      chainedStream.text().compare(0, chainedBase.str().size(),
                                                   chainedBase.str()) == 0,
                  "SimpleObjectTemplate::makeScript emits its base's text first");
    checks.expect(chainedStream.text().find("ObjectTemplate.hasMobilePhysics ") !=
                      std::string::npos,
                  "the derived emitter's one unconditional line always follows");

    // the bundle emitter brackets its members with one rule, twice
    HarnessBundleTemplate bundleScript;
    ScriptStreamProbe emptyBundleStream(&events);
    const bool emptyBundleResult = bundleScript.makeScript(&emptyBundleStream);
    const std::string emptyText = emptyBundleStream.text();

    bundleScript.addTemplate("Member");
    ScriptStreamProbe memberBundleStream(&events);
    const bool memberBundleResult = bundleScript.makeScript(&memberBundleStream);
    const std::string memberText = memberBundleStream.text();

    std::ostringstream rule;
    rule << "rem -------------------------------------" << std::endl;

    checks.expect(emptyBundleResult && emptyText.find(rule.str()) == std::string::npos,
                  "a memberless bundle emits no separator at all");
    checks.expect(memberBundleResult &&
                      memberText.find(rule.str()) != std::string::npos &&
                      memberText.find(rule.str(), memberText.find(rule.str()) + 1) !=
                          std::string::npos,
                  "a bundle with members brackets them with the same rule twice");

    dice::hfe::classManager = savedClassManager;
    dice::hfe::world::objectTemplateManager = savedManager;
}

// PhysicsNode's two advanced-drag bodies are the rigid-body vehicle path. This pins their common
// projection/clipping behavior and the shipped Z rotational coefficient quirk.
void testPhysicsNodeAdvancedDrag(Checks& checks)
{
    PhysicsSystemWindProbe physicsSystem;
    dice::hfe::IBase* const savedPhysicsSystem = dice::hfe::world::basicPhysicsSystem;
    dice::hfe::world::basicPhysicsSystem = &physicsSystem;

    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);
    PhysicsNodeDragProbe node;
    node.setObject(&object);

    Vec3 wind = { 1.0f, 0.0f, 0.0f };
    physicsSystem.setWind(wind);
    const Vec3 positionalSpeed = { 4.0f, -4.0f, 0.0f };
    const Vec3 noRotation = { 0.0f, 0.0f, 0.0f };
    node.configure(2.0f, 4.0f, positionalSpeed, noRotation);
    node.updatePositionalDragAdvanced(3.0f, 2.0f, 1.0f, 1.0f);
    checks.expect(nearVec3(node.positionalAccelerationForTest(), -7.5f, 20.0f, 0.0f),
                  "advanced positional drag projects wind-relative speed onto all object axes");

    wind.x = wind.y = wind.z = 0.0f;
    physicsSystem.setWind(wind);
    const Vec3 noPosition = { 0.0f, 0.0f, 0.0f };
    const Vec3 rotationalSpeed = { 1.0f, 2.0f, 3.0f };
    node.configure(2.0f, 4.0f, noPosition, rotationalSpeed);
    node.updateRotationalDragAdvanced(3.0f, 5.0f, 7.0f, 1.0f);
    const float base = std::sqrt(14.0f) * 0.5f;
    checks.expect(nearVec3(node.rotationalAccelerationForTest(),
                           -base * 8.0f, -base * 20.0f, -base * 30.0f),
                  "advanced rotational drag preserves retail's shared Y/Z area coefficient");

    // Soldier drag folds the negative sign into its projected force before clipping and then adds
    // it directly. A large force therefore remains -100 here; the rigid-body path would cap the
    // one-tick change to -30 before applying it.
    SoldierPhysicsNodeDragProbe soldierNode;
    soldierNode.setObject(&object);
    const Vec3 soldierSpeed = { 1.0f, 0.0f, 0.0f };
    soldierNode.configure(100.0f, 1.0f, soldierSpeed);
    soldierNode.updatePositionalDragAdvanced(1.0f, 1.0f, 1.0f, 1.0f);
    checks.expect(nearVec3(soldierNode.positionalAccelerationForTest(), -100.0f, 0.0f, 0.0f),
                  "soldier advanced drag preserves its negative-before-clipping behavior");

    dice::hfe::world::basicPhysicsSystem = savedPhysicsSystem;
    finishStackObject(object);
}

// The five recovered PhysicsNode vtable bodies: linear impulses, their torque arms, root-directed
// acceleration, torque-only acceleration, and acceleration at a world-space tangent point.
void testPhysicsNodeForceMethods(Checks& checks)
{
    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);
    PhysicsNodeDragProbe node;
    node.setObject(&object);

    const Vec3 zero = { 0.0f, 0.0f, 0.0f };
    const Vec3 lever = { 0.0f, 2.0f, 0.0f };
    const Vec3 impulse = { 3.0f, 0.0f, 0.0f };
    node.configure(0.0f, 1.0f, zero, zero);
    node.addSpeedAtRelativePosition(lever, impulse);
    checks.expect(nearVec3(node.getPositionalSpeed(), 3.0f, 0.0f, 0.0f) &&
                      nearVec3(node.getRotationalSpeed(), 0.0f, 0.0f, -6.0f),
                  "relative speed impulse adds linear speed and lever-arm torque");

    const Vec3 origin = { 5.0f, 1.0f, 0.0f };
    const Vec3 absolutePoint = { 5.0f, 3.0f, 0.0f };
    node.configure(0.0f, 1.0f, zero, zero);
    node.setPositionForTest(origin);
    node.addSpeedAtAbsolutePosition(absolutePoint, impulse);
    checks.expect(nearVec3(node.getPositionalSpeed(), 3.0f, 0.0f, 0.0f) &&
                      nearVec3(node.getRotationalSpeed(), 0.0f, 0.0f, -6.0f),
                  "absolute speed impulse subtracts the body origin before torque");

    node.configure(0.0f, 1.0f, zero, zero);
    const Vec3 rootAcceleration = { 1.0f, 2.0f, 3.0f };
    node.addPositionalAccelerationToRootParent(rootAcceleration);
    checks.expect(nearVec3(node.getPositionalAcceleration(), 1.0f, 2.0f, 3.0f),
                  "root-directed acceleration accumulates on an independent body");

    node.configure(0.0f, 1.0f, zero, zero);
    node.setPositionForTest(origin);
    const Vec3 centreOffset = { 1.0f, 0.0f, 0.0f };
    node.setCenterOfMassOffset(centreOffset);
    const Vec3 offsetPoint = { 6.0f, 3.0f, 0.0f };
    node.addRotationalAccelerationAtAbsolutePosition(offsetPoint, impulse);
    checks.expect(nearVec3(node.getPositionalAcceleration(), 0.0f, 0.0f, 0.0f) &&
                      nearVec3(node.getRotationalAcceleration(), 0.0f, 0.0f, -6.0f),
                  "torque-only acceleration subtracts origin and centre-of-mass offset");

    const Vec3 angularSpeed = { 0.0f, 0.0f, 3.0f };
    node.configure(0.0f, 1.0f, zero, angularSpeed);
    node.setPositionForTest(zero);
    node.setCenterOfMassOffset(zero);
    const Vec3 linearAcceleration = { 1.0f, 2.0f, 3.0f };
    const Vec3 angularAcceleration = { 0.0f, 0.0f, 2.0f };
    node.setPositionalAcceleration(linearAcceleration);
    node.setRotationalAcceleration(angularAcceleration);
    object.setRawPhysicsForTest(&node);
    const Vec3 tangentPoint = { 2.0f, 0.0f, 0.0f };
    const Vec3& tangentAcceleration = node.getTangentAcceleration(tangentPoint);
    checks.expect(nearVec3(tangentAcceleration, -17.0f, 6.0f, 3.0f),
                  "tangent acceleration combines linear, alpha-cross-radius and centripetal terms");
    object.setRawPhysicsForTest(0);

    finishStackObject(object);
}

// The two point-family bodies closed after the advanced-drag path: Soldier's unused simple helper
// and the type-2 node's live rotational integrator.
void testPointFamilyRecoveredPhysics(Checks& checks)
{
    PhysicsSystemWindProbe physicsSystem;
    dice::hfe::IBase* const savedPhysicsSystem = dice::hfe::world::basicPhysicsSystem;
    dice::hfe::world::basicPhysicsSystem = &physicsSystem;

    FactorySimpleTemplate objectTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);

    SoldierPhysicsNodeDragProbe soldierNode;
    soldierNode.setObject(&object);
    const Vec3 soldierSpeed = { 2.0f, -1.0f, 0.5f };
    soldierNode.configure(2.0f, 1.0f, soldierSpeed);
    soldierNode.updatePositionalDragSimple(0.5f, 1.0f);
    checks.expect(nearVec3(soldierNode.positionalAccelerationForTest(),
                           -3.1415927f, 1.57079635f, -0.78539818f),
                  "soldier simple drag writes its component-wise force directly");

    RotationalPointPhysicsNodeProbe rotationalNode;
    rotationalNode.setObject(&object);
    const Vec3 noSpeed = { 0.0f, 0.0f, 0.0f };
    const Vec3 acceleration = { 2.0f, 4.0f, 6.0f };
    const Vec3 friction = { 1.0f, -1.0f, 0.0f };
    rotationalNode.configure(150.0f, noSpeed, acceleration, friction);
    rotationalNode.updateRotationalPhysics(1.0f);
    checks.expect(nearVec3(rotationalNode.getRotationalSpeed(), 3.0f, 3.0f, 6.0f) &&
                      nearVec3(rotationalNode.angularIntegrationVelocityForTest(),
                               1.5f, 1.5f, 3.0f),
                  "rotational point integration uses sphere inertia and trapezoidal speed");
    checks.expect(nearVec3(rotationalNode.rotationalAccelerationForTest(), 0.0f, 0.0f, 0.0f) &&
                      nearVec3(rotationalNode.rotationalFrictionForTest(), 0.0f, 0.0f, 0.0f) &&
                      rotationalNode.trailingStateForTest() == 0 &&
                      rotationalNode.m_transformSetCalls == 1,
                  "rotational point integration rotates once and clears per-tick state");

    dice::hfe::world::basicPhysicsSystem = savedPhysicsSystem;
    finishStackObject(object);
}

// RotationalPointResponsePhysics::checkObjectVsObject - ELF 0x8373F70 / PE 0x6A01B0. Exercise
// the ordinary (non-GenericProjectile) direct-mesh arm through the real production body and the
// PE collision-mesh slot used by the x86 harness.
void testRotationalPointObjectCollision(Checks& checks)
{
    PhysicsSystemWindProbe physicsSystem;
    dice::hfe::IBase* const savedPhysicsSystem = dice::hfe::world::basicPhysicsSystem;
    dice::hfe::world::basicPhysicsSystem = &physicsSystem;

    const int savedObjectRayChecks = dice::hfe::world::collisionStatsData->unknown20;
    dice::hfe::world::collisionStatsData->unknown20 = 41;

    FactorySimpleTemplate objectTemplate;
    FactorySimpleTemplate otherTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate);
    ObjectComponentRuntimeProbe other(&otherTemplate);

    const Vec3 objectPosition = { 10.0f, 5.0f, 1.0f };
    const Vec3 otherPosition = { 3.0f, 4.0f, 5.0f };
    object.setAbsolutePosition(objectPosition);
    other.setAbsolutePosition(otherPosition);

    PhysicsNodeDragProbe objectNode;
    PhysicsNodeDragProbe otherNode;
    objectNode.setObject(&object);
    otherNode.setObject(&other);
    const Vec3 objectSpeed = { 2.0f, -2.0f, 3.0f };
    const Vec3 noSpeed = { 0.0f, 0.0f, 0.0f };
    objectNode.configure(0.0f, 1.0f, objectSpeed, noSpeed);
    otherNode.configure(0.0f, 1.0f, noSpeed, noSpeed);
    const Vec3 objectIntegrationVelocity = { 4.0f, -2.0f, 0.5f };
    const Vec3 otherIntegrationVelocity = { 1.0f, -1.0f, -0.5f };
    objectNode.setLinearIntegrationVelocity(objectIntegrationVelocity);
    otherNode.setLinearIntegrationVelocity(otherIntegrationVelocity);
    object.setRawPhysicsForTest(&objectNode);
    other.setRawPhysicsForTest(&otherNode);

    CollisionMeshPeAbiProbe mesh(0);
    mesh.m_availableLod = 0;
    const Vec3 hitNormal = { 0.0f, 1.0f, 0.0f };
    const Vec3 hitPosition = { 9.25f, 4.75f, 1.25f };
    mesh.setDistanceHit(hitNormal, hitPosition, -0.25f, -0.5f, 7);
    other.setRawCollisionForTest(&mesh);

    RotationalPointResponseProbe response;
    response.setObject(&object);
    object.setRawResponseForTest(&response);
    response.checkObjectVsObject(&object, &other, 0.5f, false);

    checks.expect(mesh.m_collisionLodCalls == 1 && mesh.m_lastCollisionLod == 0 &&
                      mesh.m_distanceCalls == 1 && mesh.m_distanceLod == 0 &&
                      !mesh.m_distanceNoFacingFilter && mesh.m_distanceFrontFaces &&
                      !mesh.m_distanceFirstHit && mesh.m_distanceFaceInfo == 0,
                  "rotational point direct-mesh collision uses the recovered LOD and query flags");
    checks.expect(nearVec3(mesh.m_distanceRayStart, 8.5f, 5.5f, 0.5f) &&
                      nearVec3(mesh.m_distanceDirection, 1.5f, -0.5f, 0.5f),
                  "rotational point direct-mesh collision derives the shipped sweep start and direction");
    checks.expect(nearVec3(object.getAbsolutePosition(), 9.25f, 4.75f, 1.25f),
                  "rotational point direct-mesh hit moves the object to the returned hit position");
    checks.expect(nearVec3(objectNode.getPositionalSpeed(), 2.0f, -1.509f, 3.0f),
                  "rotational point upward normal clamps downward speed by gravity divided by thirty");
    checks.expect(dice::hfe::world::collisionStatsData->unknown20 == 42,
                  "rotational point object collision increments collision stats +0x20 once");
    checks.expect(near(response.contactDepthSqForTest(), 0.0f) &&
                      response.contactObjectForTest() == static_cast<IObject*>(&other) &&
                      nearVec3(response.contactPointForTest(), 9.25f, 4.75f, 1.25f) &&
                      nearVec3(response.contactPositionForTest(), 9.25f, 4.75f, 1.25f) &&
                      nearVec3(response.contactSurfaceSpeedForTest(), 3.0f, 4.0f, 5.0f) &&
                      nearVec3(response.contactSpeedForTest(), 2.0f, -1.509f, 3.0f) &&
                      near(response.contactScaleForTest(), 1.0f) &&
                      near(response.contactFractionForTest(), -0.25f) &&
                      nearVec3(response.contactNormalForTest(), 0.0f, 1.0f, 0.0f) &&
                      response.contactOwnMaterialForTest() == 0 &&
                      response.contactMaterialForTest() == 7,
                  "rotational point direct-mesh collision stores the complete shipped contact record");

    object.setRawResponseForTest(0);
    object.setRawPhysicsForTest(0);
    other.setRawPhysicsForTest(0);
    other.setRawCollisionForTest(0);
    dice::hfe::world::collisionStatsData->unknown20 = savedObjectRayChecks;
    dice::hfe::world::basicPhysicsSystem = savedPhysicsSystem;
    finishStackObject(other);
    finishStackObject(object);
}

// Point sweep regression: the missing body used to be shadowed by this harness's no-op.
// Exercise its response-presence gate and ordinary mesh hit with distinct distance outputs.
void testPointObjectCollision(Checks& checks)
{
    FactorySimpleTemplate objectTemplate, otherTemplate;
    ObjectComponentRuntimeProbe object(&objectTemplate), other(&otherTemplate);
    object.setAbsolutePosition(Vec3{10.0f, 5.0f, 1.0f});
    PhysicsNodeDragProbe ownNode, otherNode;
    ownNode.setObject(&object);
    otherNode.setObject(&other);
    ownNode.setPositionForTest(object.getAbsolutePosition());
    const Vec3 zero = {0.0f, 0.0f, 0.0f};
    ownNode.configure(0.0f, 1.0f, Vec3{2.0f, -2.0f, 3.0f}, zero);
    otherNode.configure(0.0f, 1.0f, Vec3{1.0f, 0.0f, -1.0f}, zero);
    ownNode.setLinearIntegrationVelocity(Vec3{4.0f, -2.0f, 0.5f});
    object.setRawPhysicsForTest(&ownNode);
    other.setRawPhysicsForTest(&otherNode);
    CollisionMeshPeAbiProbe mesh(0);
    mesh.m_availableLod = 0;
    mesh.setDistanceHit(Vec3{0.0f, 1.0f, 0.0f}, Vec3{9.0f, 5.5f, 0.75f}, 0.25f, 0.75f, 7);
    other.setRawCollisionForTest(&mesh);
    PointResponseProbe response;
    response.setObject(&object);
    const int savedChecks = dice::hfe::world::collisionStatsData->unknown08;
    response.checkObjectVsObject(&object, &other, 0.5f, false);
    checks.expect(mesh.m_distanceCalls == 0, "point sweep requires another response component");
    other.setRawResponseForTest(&response);
    response.checkObjectVsObject(&object, &other, 0.5f, false);
    checks.expect(mesh.m_distanceCalls == 1 && mesh.m_distanceLod == 0 &&
                      !mesh.m_distanceNoFacingFilter && mesh.m_distanceFrontFaces &&
                      !mesh.m_distanceFirstHit && mesh.m_distanceFaceInfo != 0,
                  "point sweep uses LOD zero and supplies face information");
    checks.expect(nearVec3(mesh.m_distanceRayStart, 8.0f, 6.0f, 0.75f) &&
                      nearVec3(mesh.m_distanceDirection, 2.0f, -1.0f, 0.25f),
                  "point sweep uses the point integration velocity without subtracting target motion");
    checks.expect(response.contactObject() == &other &&
                      nearVec3(response.contactPoint(), 9.0f, 5.5f, 0.75f) &&
                      near(response.contactFraction(), 0.25f) && response.contactMaterial() == 7 &&
                      response.hasFaceInfo(), "point sweep stores the hit and first distance output");
    checks.expect(nearVec3(response.contactSpeed(), 1.0f, -2.0f, 4.0f),
                  "point contact subtracts the target tangent velocity");
    checks.expect(dice::hfe::world::collisionStatsData->unknown08 == savedChecks + 1,
                  "point sweep increments the object query counter once");
    dice::hfe::world::collisionStatsData->unknown08 = savedChecks;
    other.setRawResponseForTest(0);
    object.setRawPhysicsForTest(0);
    other.setRawPhysicsForTest(0);
    other.setRawCollisionForTest(0);
    finishStackObject(other);
    finishStackObject(object);
}

} // unnamed namespace

#if !defined(BF2_BUNDLE_SOUND_DISPATCH_ONLY)
int main()
{
    ObjectManagerPeAbiProbe defaultObjectManager(1000);
    PhysicsNodeManagerPeAbiProbe defaultPhysicsManager(0);
    ResponsePhysicsManagerPeAbiProbe defaultResponseManager(0);
    EventManagerPeAbiProbe defaultEventManager;
    PlayerManagerPeAbiProbe defaultPlayerManager;

    dice::hfe::world::ObjectManager* savedObjectManager =
        dice::hfe::world::objectManager;
    dice::hfe::world::ObjectManager* savedStaticObjectManager =
        Object::mObjectManager;
    dice::hfe::world::PhysicsNodeManager* savedPhysicsNodeManager =
        dice::hfe::world::physicsNodeManager;
    dice::hfe::world::ResponsePhysicsManager* savedResponsePhysicsManager =
        dice::hfe::world::responsePhysicsManager;
    dice::hfe::EventManager* savedEventManager = dice::hfe::eventManager;
    dice::hfe::world::PlayerManager* savedPlayerManager =
        dice::hfe::world::playerManager;

    // SimpleObject::handleCollision dereferences both globals without a null guard, exactly as the
    // shipped code does, so every test that reaches it needs them present.
    MaterialManagerPeAbiProbe defaultMaterialManager;
    GameLogicCollisionPeAbiProbe defaultGameLogic;
    dice::hfe::IBase* savedMaterialManager = dice::hfe::world::materialManager;
    dice::hfe::IGameLogic* savedGameLogic = dice::hfe::gameLogic;
    dice::hfe::world::materialManager =
        reinterpret_cast<dice::hfe::IBase*>(&defaultMaterialManager);
    dice::hfe::gameLogic = reinterpret_cast<dice::hfe::IGameLogic*>(&defaultGameLogic);

    dice::hfe::world::ObjectManager* objectManagerFixture =
        reinterpret_cast<dice::hfe::world::ObjectManager*>(&defaultObjectManager);
    dice::hfe::world::objectManager = objectManagerFixture;
    Object::mObjectManager = objectManagerFixture;
    dice::hfe::world::physicsNodeManager =
        reinterpret_cast<dice::hfe::world::PhysicsNodeManager*>(&defaultPhysicsManager);
    dice::hfe::world::responsePhysicsManager =
        reinterpret_cast<dice::hfe::world::ResponsePhysicsManager*>(&defaultResponseManager);
    dice::hfe::eventManager =
        reinterpret_cast<dice::hfe::EventManager*>(&defaultEventManager);
    dice::hfe::world::playerManager =
        reinterpret_cast<dice::hfe::world::PlayerManager*>(&defaultPlayerManager);

    Checks checks;
    testCapsuleEdgesCollision(checks);
    testPhysicsTypeStream(checks);
    testMatrixMultiplyPaths(checks);
    testSimpleObjectTemplatePortalPosition(checks);
    testFactoriesAndWeakInvalidation(checks);
    testObjectTemplateComponentOwnership(checks);
    testPreCacheLifecycle(checks);
    testBundleChildAssembly(checks);
    testBundleInputPropagation(checks);
    testDriverDecalRegistration(checks);
    testBundleChildPartPropagation(checks);
    testBundleRuntimeWrappers(checks);
    testObjectComponentOwnershipAndMaps(checks);
    testObjectFastComponentDispatch(checks);
    testObjectComponentDirtySideEffects(checks);
    testObjectLightSourceMaskForwarding(checks);
    testObjectActiveLodForwarding(checks);
    testObjectGeometryUpdateChain(checks);
    testObjectCollisionLodForwarding(checks);
    testObjectBoundingRadii(checks);
    testObjectBoundingBoxOwnAndCache(checks);
    testObjectFullBoundingBoxHierarchy(checks);
    testObjectBoundingBoxEndpointReflection(checks);
    testObjectMineBoundingBoxEligibility(checks);
    testObjectDisableChildrenPropagation(checks);
    testUpdateFlagsRecursiveSemanticSplit(checks);
    testInternalAssignNewObjectId(checks);
    testLightSourceManagerLifecycle(checks);
    testObjectChildHierarchyLifecycle(checks);
    testObjectInitCacheLifecycle(checks);
    testObjectDisableCachedLifecycle(checks);
    testObjectEnableCachedLifecycle(checks);
    testObjectUpdateFlagsTransitions(checks);
    testSimpleObjectUpdateAndMessageDispatch(checks);
    testObjectMakeScript(checks);
    testSimpleObjectNetworkableComp(checks);
    testBundleHandleMessage(checks);
    testSimpleObjectVisualUpdate(checks);
    testSimpleObjectHandleCollision(checks);
    testObjectTemplateMakeScript(checks);
    testPhysicsNodeAdvancedDrag(checks);
    testPhysicsNodeForceMethods(checks);
    testPointFamilyRecoveredPhysics(checks);
    testRotationalPointObjectCollision(checks);
    testPointObjectCollision(checks);
    testObjectDestroyLifecycle(checks);
    testSimpleObjectArmorInfo(checks);
    testSimpleObjectDisableCachedLifecycle(checks);
    testObjectTemplatePhysicsRemovalHooks(checks);
    testPortalManagerUnregisterPaths(checks);
    testSimpleObjectRecoveredLifecycleBranches(checks);
    testActualBundleInitAndSoundOrdering(checks);

    std::cout << "runtime pair: " << checks.checks() << " checks, "
              << checks.failures() << " failures" << std::endl;
    const int result = checks.failures() == 0 ? 0 : 1;

    dice::hfe::gameLogic = savedGameLogic;
    dice::hfe::world::materialManager = savedMaterialManager;
    dice::hfe::world::playerManager = savedPlayerManager;
    dice::hfe::eventManager = savedEventManager;
    dice::hfe::world::responsePhysicsManager = savedResponsePhysicsManager;
    dice::hfe::world::physicsNodeManager = savedPhysicsNodeManager;
    Object::mObjectManager = savedStaticObjectManager;
    dice::hfe::world::objectManager = savedObjectManager;
    return result;
}
#endif
