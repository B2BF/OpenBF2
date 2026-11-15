// [TEMP] [CHOSEN, not recovered] Camera::handleUpdate and its shake/recoil helpers (Game/Objects/Base/Camera.cpp,
// batch 21) against fake roots, physics nodes, templates, players and the object grid (CTest
// bf2_camera_shake_check). The real Camera.cpp is linked; the rest of the engine is replaced by
// tools/macos/link_stubs.py's generated stubs (which abort by name if reached) and by the fakes below,
// generated from their headers. The Camera is never constructed: a zeroed block gets Camera's own
// vtable, which is all the tested functions use. Expected values come from the shipped constants.
#include "../BF2/Game/Objects/Base/Camera.h"
#include "../BF2/Game/Objects/CameraTemplate.h"
#include "../BF2/Game/Objects/IObject.h"
#include "../BF2/Game/Objects/ObjectLayoutAccess.h"
#include "../BF2/Game/Objects/IObjectTemplate.h"
#include "../BF2/Game/Objects/IPlayer.h"
#include "../BF2/Physics/IPhysicsNode.h"
#include "../BF2/Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/ObjectPredicator.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Core/Ids.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace dice::hfe;
using namespace dice::hfe::world;

static unsigned checks, failures;
static void check(bool value, const std::string& label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label.c_str()); }
}
static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
[[noreturn]] static void unexpected(const char* name)
{
    std::fprintf(stderr, "[TEMP] camera_shake check: unexpected call %s\n", name);
    std::fflush(stderr);
    std::abort();
}
static std::vector<std::string> calls;
static Vec3 v3(float x, float y, float z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; }

// ---- the engine pieces the tested functions reach -------------------------------------------------
static Vec3 s_cameraPosition = v3(0.0f, 0.0f, 0.0f);
static io::INetworkable* s_networkable = 0;
static IPlayer* s_localPlayer = 0;
namespace dice { namespace hfe { namespace world {
PlayerManager* playerManager = 0;
ObjectManager* objectManager = 0;
IObject::IObject() {}
IObject::~IObject() {}
IPhysicsNode::IPhysicsNode() {}
IPhysicsNode::~IPhysicsNode() {}
IObjectTemplate::IObjectTemplate() {}
IObjectTemplate::~IObjectTemplate() {}
const Vec3& Object::getAbsolutePosition() const { return s_cameraPosition; }   // only the camera is an Object here
io::INetworkable* IObject::getNetworkable() const { return s_networkable; }
IObject* getRootParent(const IObject* object)
{
    IObject* current = const_cast<IObject*>(object);
    while (current && object_detail::LayoutAccess::parent(current))
        current = object_detail::LayoutAccess::parent(current);
    return current;
}
} } }
namespace dice { namespace hfe { namespace world {
void RotationalBundle::handleUpdate(float deltaTime, unsigned int flags)
{
    char b[64];
    std::snprintf(b, sizeof(b), "base %.3f %u", deltaTime, flags);
    calls.push_back(b);
}
} } }

// ---- fakes, generated from their headers -------------------------------------------------------
namespace dice { namespace hfe { namespace world {
struct FakePhysics final : IPhysicsNode {
    bool mobile = true;
    float mass = 0.0f;
    Vec3 speed = {0.0f, 0.0f, 0.0f};
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("IPhysicsNode::queryInterface"); }
    bool getIsMobile() const override { return mobile; }
    float getMass() const override { return mass; }
    const Vec3& getPositionalSpeed() const override { return speed; }
    void setEnabled(bool) override { unexpected("IPhysicsNode::setEnabled"); }
    bool getEnabled() const override { unexpected("IPhysicsNode::getEnabled"); }
    void setIsDamaged(bool) override { unexpected("IPhysicsNode::setIsDamaged"); }
    bool getIsDamaged() const override { unexpected("IPhysicsNode::getIsDamaged"); }
    void setObject(IObject*) override { unexpected("IPhysicsNode::setObject"); }
    IObject* getObject() const override { unexpected("IPhysicsNode::getObject"); }
    void setParent(IPhysicsNode*) override { unexpected("IPhysicsNode::setParent"); }
    IPhysicsNode* getParent() const override { unexpected("IPhysicsNode::getParent"); }
    float getBoundingRadius() const override { unexpected("IPhysicsNode::getBoundingRadius"); }
    const Mat4& getAbsoluteTransformation() const override { unexpected("IPhysicsNode::getAbsoluteTransformation"); }
    void setAbsoluteTransformation(const Mat4&) override { unexpected("IPhysicsNode::setAbsoluteTransformation"); }
    const Mat4& getRelativeTransformation() const override { unexpected("IPhysicsNode::getRelativeTransformation"); }
    const Vec3& getAbsolutePosition() const override { unexpected("IPhysicsNode::getAbsolutePosition"); }
    void setAbsolutePosition(const Vec3&) override { unexpected("IPhysicsNode::setAbsolutePosition"); }
    const Mat4& getPrevTransformation() const override { unexpected("IPhysicsNode::getPrevTransformation"); }
    void setPrevTransformation(const Mat4&) override { unexpected("IPhysicsNode::setPrevTransformation"); }
    void setOldPosition(const Vec3&) override { unexpected("IPhysicsNode::setOldPosition"); }
    const Vec3& getRelativePosition() const override { unexpected("IPhysicsNode::getRelativePosition"); }
    const Vec3& getRotationalSpeed() const override { unexpected("IPhysicsNode::getRotationalSpeed"); }
    void setPositionalSpeed(const Vec3&) override { unexpected("IPhysicsNode::setPositionalSpeed"); }
    void setRotationalSpeed(const Vec3&) override { unexpected("IPhysicsNode::setRotationalSpeed"); }
    void addPositionalSpeed(const Vec3&) override { unexpected("IPhysicsNode::addPositionalSpeed"); }
    void addRotationalSpeed(const Vec3&) override { unexpected("IPhysicsNode::addRotationalSpeed"); }
    void setPositionalAcceleration(const Vec3&) override { unexpected("IPhysicsNode::setPositionalAcceleration"); }
    void setRotationalAcceleration(const Vec3&) override { unexpected("IPhysicsNode::setRotationalAcceleration"); }
    void setPositionalFriction(const Vec3&) override { unexpected("IPhysicsNode::setPositionalFriction"); }
    void setRotationalFriction(const Vec3&) override { unexpected("IPhysicsNode::setRotationalFriction"); }
    void addSpeedAtAbsolutePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addSpeedAtAbsolutePosition"); }
    void addSpeedAtRelativePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addSpeedAtRelativePosition"); }
    void addAccelerationAtAbsolutePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addAccelerationAtAbsolutePosition"); }
    void addAccelerationAtRelativePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addAccelerationAtRelativePosition"); }
    void addFrictionAtAbsolutePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addFrictionAtAbsolutePosition"); }
    void addPositionalAccelerationToRootParent(const Vec3&) override { unexpected("IPhysicsNode::addPositionalAccelerationToRootParent"); }
    void addRotationalAccelerationAtAbsolutePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addRotationalAccelerationAtAbsolutePosition"); }
    const Vec3& getTangentSpeed(const Vec3&) override { unexpected("IPhysicsNode::getTangentSpeed"); }
    const Vec3& getTangentAcceleration(const Vec3&) override { unexpected("IPhysicsNode::getTangentAcceleration"); }
    const Vec3& getPositionalAcceleration() const override { unexpected("IPhysicsNode::getPositionalAcceleration"); }
    const Vec3& getRotationalAcceleration() const override { unexpected("IPhysicsNode::getRotationalAcceleration"); }
    const Vec3& getPositionalFriction() const override { unexpected("IPhysicsNode::getPositionalFriction"); }
    const Vec3& getRotationalFriction() const override { unexpected("IPhysicsNode::getRotationalFriction"); }
    void updatePhysics(float dt) override { unexpected("IPhysicsNode::updatePhysics"); }
    void setDrag(float) override { unexpected("IPhysicsNode::setDrag"); }
    float getDrag() const override { unexpected("IPhysicsNode::getDrag"); }
    void setDragOffset(const Vec3&) override { unexpected("IPhysicsNode::setDragOffset"); }
    const Vec3& getDragOffset() const override { unexpected("IPhysicsNode::getDragOffset"); }
    void setDragModifier(const Vec3&) override { unexpected("IPhysicsNode::setDragModifier"); }
    const Vec3& getDragModifier() const override { unexpected("IPhysicsNode::getDragModifier"); }
    void setMass(float) override { unexpected("IPhysicsNode::setMass"); }
    void setCenterOfMassOffset(const Vec3&) override { unexpected("IPhysicsNode::setCenterOfMassOffset"); }
    const Vec3& getCenterOfMassOffset() const override { unexpected("IPhysicsNode::getCenterOfMassOffset"); }
    void setCenterOfCollisionOffset(const Vec3&) override { unexpected("IPhysicsNode::setCenterOfCollisionOffset"); }
    const Vec3& getCenterOfCollisionOffset() const override { unexpected("IPhysicsNode::getCenterOfCollisionOffset"); }
    void setInertiaModifier(const Vec3&) override { unexpected("IPhysicsNode::setInertiaModifier"); }
    const Vec3& getInertiaModifier() const override { unexpected("IPhysicsNode::getInertiaModifier"); }
    void setGravityModifier(float) override { unexpected("IPhysicsNode::setGravityModifier"); }
    float getGravityModifier() const override { unexpected("IPhysicsNode::getGravityModifier"); }
    void setHasSeparatePhysicsUpdate(bool) override { unexpected("IPhysicsNode::setHasSeparatePhysicsUpdate"); }
    bool getHasSeparatePhysicsUpdate() const override { unexpected("IPhysicsNode::getHasSeparatePhysicsUpdate"); }
    void setUnderWater(float) override { unexpected("IPhysicsNode::setUnderWater"); }
    float getUnderWater() const override { unexpected("IPhysicsNode::getUnderWater"); }
    void setUnderwaterYComponent(float) override { unexpected("IPhysicsNode::setUnderwaterYComponent"); }
    bool getIsTreadingWater() const override { unexpected("IPhysicsNode::getIsTreadingWater"); }
    void reset(bool) override { unexpected("IPhysicsNode::reset"); }
    bool getIsSleeping() const override { unexpected("IPhysicsNode::getIsSleeping"); }
    void setIsSleeping() override { unexpected("IPhysicsNode::setIsSleeping"); }
    void setSleepiness(int) override { unexpected("IPhysicsNode::setSleepiness"); }
    int getSleepiness() const override { unexpected("IPhysicsNode::getSleepiness"); }
    void setIsSleepier() override { unexpected("IPhysicsNode::setIsSleepier"); }
    void setIsAwake() override { unexpected("IPhysicsNode::setIsAwake"); }
    bool getIsPoint() const override { unexpected("IPhysicsNode::getIsPoint"); }
    void initLatencyCompensation() override { unexpected("IPhysicsNode::initLatencyCompensation"); }
    bool hasLatencyCompensation() override { unexpected("IPhysicsNode::hasLatencyCompensation"); }
    void updateLatencyState() override { unexpected("IPhysicsNode::updateLatencyState"); }
    const Mat4& getLatencyCompensatedTransformation(float, int*) override { unexpected("IPhysicsNode::getLatencyCompensatedTransformation"); }
    const Vec3& getAbsolutePositionalSpeed() override { unexpected("IPhysicsNode::getAbsolutePositionalSpeed"); }
    IPhysicsNode* getRootNode() override { unexpected("IPhysicsNode::getRootNode"); }
    void setHandleCollisionSpeed(float) override { unexpected("IPhysicsNode::setHandleCollisionSpeed"); }
    float getHandleCollisionSpeed() const override { unexpected("IPhysicsNode::getHandleCollisionSpeed"); }
    const Vec3& getInverseInertia() override { unexpected("IPhysicsNode::getInverseInertia"); }
    void addRelativeImpulse(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addRelativeImpulse"); }
    void setFloaterMod(float) override { unexpected("IPhysicsNode::setFloaterMod"); }
    float getFloaterMod() const override { unexpected("IPhysicsNode::getFloaterMod"); }
    const Vec3& getLinearIntegrationVelocity() const override { unexpected("IPhysicsNode::getLinearIntegrationVelocity"); }
    void setLinearIntegrationVelocity(const Vec3&) override { unexpected("IPhysicsNode::setLinearIntegrationVelocity"); }
    const Vec3& getAngularIntegrationVelocity() const override { unexpected("IPhysicsNode::getAngularIntegrationVelocity"); }
    void setAngularIntegrationVelocity(const Vec3&) override { unexpected("IPhysicsNode::setAngularIntegrationVelocity"); }
    void setTreeCollisionRadius(float) override { unexpected("IPhysicsNode::setTreeCollisionRadius"); }
    float getTreeCollisionRadius() const override { unexpected("IPhysicsNode::getTreeCollisionRadius"); }
    void postFrameUpdate() override { unexpected("IPhysicsNode::postFrameUpdate"); }
    void setLocalLinearSpeed(const Vec3&) override { unexpected("IPhysicsNode::setLocalLinearSpeed"); }
    const Vec3& getLocalLinearSpeed() const override { unexpected("IPhysicsNode::getLocalLinearSpeed"); }
    void setLocalAngularSpeed(const Vec3&) override { unexpected("IPhysicsNode::setLocalAngularSpeed"); }
    const Vec3& getLocalAngularSpeed() const override { unexpected("IPhysicsNode::getLocalAngularSpeed"); }
    void setOtherCollisionLod(int) override { unexpected("IPhysicsNode::setOtherCollisionLod"); }
    int getOtherCollisionLod() const override { unexpected("IPhysicsNode::getOtherCollisionLod"); }
    bool getShouldGravityCompensateOnContact() const override { unexpected("IPhysicsNode::getShouldGravityCompensateOnContact"); }
    void setHasContact(bool) override { unexpected("IPhysicsNode::setHasContact"); }
    bool getHasContact() const override { unexpected("IPhysicsNode::getHasContact"); }
    void setIsInAir(bool) override { unexpected("IPhysicsNode::setIsInAir"); }
    bool getIsInAir() const override { unexpected("IPhysicsNode::getIsInAir"); }
    void setSleepTimeout(int) override { unexpected("IPhysicsNode::setSleepTimeout"); }
    void setIsPlatformPhysics(bool) override { unexpected("IPhysicsNode::setIsPlatformPhysics"); }
    bool getIsPlatformPhysics() const override { unexpected("IPhysicsNode::getIsPlatformPhysics"); }
};
struct FakeTemplate final : IObjectTemplate {
    unsigned int classId = 0;
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("IObjectTemplate::queryInterface"); }
    unsigned int getClassID() const override { return classId; }
    void templateInitialize() override { unexpected("IObjectTemplate::templateInitialize"); }
    bool writeBinary(io::IStream*) const override { unexpected("IObjectTemplate::writeBinary"); }
    bool readBinary(io::IStream*) override { unexpected("IObjectTemplate::readBinary"); }
    void setName(const std::string&) override { unexpected("IObjectTemplate::setName"); }
    const std::string& getName() const override { unexpected("IObjectTemplate::getName"); }
    const std::string& getCreator() const override { unexpected("IObjectTemplate::getCreator"); }
    void setCreator(const std::string&) override { unexpected("IObjectTemplate::setCreator"); }
    const std::string& getModifiedByUser() const override { unexpected("IObjectTemplate::getModifiedByUser"); }
    void setModifiedByUser(const std::string&) override { unexpected("IObjectTemplate::setModifiedByUser"); }
    void setId(unsigned int) override { unexpected("IObjectTemplate::setId"); }
    unsigned int getId() const override { unexpected("IObjectTemplate::getId"); }
    unsigned int getFlags() const override { unexpected("IObjectTemplate::getFlags"); }
    unsigned int updateFlags(unsigned int set, unsigned int clear) override { unexpected("IObjectTemplate::updateFlags"); }
    void* queryImplementation(unsigned int classId) override { unexpected("IObjectTemplate::queryImplementation"); }
    IObject* createObject() const override { unexpected("IObjectTemplate::createObject"); }
    void setComponent(unsigned int classId, IBase* component) override { unexpected("IObjectTemplate::setComponent"); }
    IObjectTemplateComponent* queryComponent(unsigned int) override { unexpected("IObjectTemplate::queryComponent"); }
    IObjectTemplateComponent* getComponentFromClassID(unsigned int) const override { unexpected("IObjectTemplate::getComponentFromClassID"); }
    void objectCreated(IObject*) override { unexpected("IObjectTemplate::objectCreated"); }
    void objectDestroyed(IObject*) override { unexpected("IObjectTemplate::objectDestroyed"); }
    void preCache() const override { unexpected("IObjectTemplate::preCache"); }
    void setCullRadiusScale(float) override { unexpected("IObjectTemplate::setCullRadiusScale"); }
    float getCullRadiusScale() const override { unexpected("IObjectTemplate::getCullRadiusScale"); }
    void setPhysicsComponent(IObject*) const override { unexpected("IObjectTemplate::setPhysicsComponent"); }
    void removePhysicsComponent(IObject*) const override { unexpected("IObjectTemplate::removePhysicsComponent"); }
    void setPhysicsNodeComponent(IObject*) const override { unexpected("IObjectTemplate::setPhysicsNodeComponent"); }
    void removePhysicsNodeComponent(IObject*) const override { unexpected("IObjectTemplate::removePhysicsNodeComponent"); }
    void setResponsePhysicsComponent(IObject*) const override { unexpected("IObjectTemplate::setResponsePhysicsComponent"); }
    void removeResponsePhysicsComponent(IObject*) const override { unexpected("IObjectTemplate::removeResponsePhysicsComponent"); }
    void setNetworkableInfo(const std::string&) override { unexpected("IObjectTemplate::setNetworkableInfo"); }
    const std::string& getNetworkableInfo() const override { unexpected("IObjectTemplate::getNetworkableInfo"); }
    void setCollisionMesh(const std::string&) override { unexpected("IObjectTemplate::setCollisionMesh"); }
    const std::string& getCollisionMesh() const override { unexpected("IObjectTemplate::getCollisionMesh"); }
    const Vec3& getAnchor() const override { unexpected("IObjectTemplate::getAnchor"); }
    void setAnchor(const Vec3&) override { unexpected("IObjectTemplate::setAnchor"); }
    const Vec3& getAnchorOffset() const override { unexpected("IObjectTemplate::getAnchorOffset"); }
    void setAnchorOffset(const Vec3&) override { unexpected("IObjectTemplate::setAnchorOffset"); }
    Vec3 getAnchorPos() const override { unexpected("IObjectTemplate::getAnchorPos"); }
    void setIsOvergrowth(bool) override { unexpected("IObjectTemplate::setIsOvergrowth"); }
    bool getIsOvergrowth() const override { unexpected("IObjectTemplate::getIsOvergrowth"); }
    std::vector<std::pair<std::string, unsigned int> >* getMaterialVector() override { unexpected("IObjectTemplate::getMaterialVector"); }
    ComponentMap* getComponentMap() override { unexpected("IObjectTemplate::getComponentMap"); }
    void setMaterial(int) override { unexpected("IObjectTemplate::setMaterial"); }
    int getMaterial() const override { unexpected("IObjectTemplate::getMaterial"); }
    void setSpecialMaterials(unsigned int) override { unexpected("IObjectTemplate::setSpecialMaterials"); }
    unsigned int getSpecialMaterials() const override { unexpected("IObjectTemplate::getSpecialMaterials"); }
};
struct FakePlayer final : IPlayer {
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("IPlayer::queryInterface"); }
    bool getFreeCameraEnabled() const override { return false; }
    unsigned int getClassID() const override { unexpected("IPlayer::getClassID"); }
    WeakPtr<IPlayer> getWeakPtr() override { unexpected("IPlayer::getWeakPtr"); }
    void setName(const std::string& name) override { unexpected("IPlayer::setName"); }
    const std::string& getName() const override { unexpected("IPlayer::getName"); }
    void setFlags(const Flag32Bits<IPlayer>& flags) override { unexpected("IPlayer::setFlags"); }
    bool testFlags(const Flag32Bits<IPlayer>& flags) const override { unexpected("IPlayer::testFlags"); }
    void handleUpdate(float dt, unsigned int) override { unexpected("IPlayer::handleUpdate"); }
    void handleFrameUpdate(float dt) override { unexpected("IPlayer::handleFrameUpdate"); }
    UpdateFreqencyType getUpdateFrequencyType() const override { unexpected("IPlayer::getUpdateFrequencyType"); }
    void setUpdateFrequencyType(UpdateFreqencyType type) override { unexpected("IPlayer::setUpdateFrequencyType"); }
    void handleInput(io::PlayerInput& input, float dt, unsigned int) override { unexpected("IPlayer::handleInput"); }
    void getAutoInput(io::PlayerInput& out, bool) override { unexpected("IPlayer::getAutoInput"); }
    IObject* getVehicle() const override { unexpected("IPlayer::getVehicle"); }
    bool setVehicle(IObject* vehicle, int seat) override { unexpected("IPlayer::setVehicle"); }
    int getInputId() const override { unexpected("IPlayer::getInputId"); }
    int getId() const override { unexpected("IPlayer::getId"); }
    void setIsRemote(bool remote) override { unexpected("IPlayer::setIsRemote"); }
    bool getIsRemote() const override { unexpected("IPlayer::getIsRemote"); }
    void setIsAIPlayer(bool ai) override { unexpected("IPlayer::setIsAIPlayer"); }
    bool getIsAIPlayer() const override { unexpected("IPlayer::getIsAIPlayer"); }
    void setIsAlive(bool alive) override { unexpected("IPlayer::setIsAlive"); }
    bool getIsAlive() const override { unexpected("IPlayer::getIsAlive"); }
    void setSuicide(bool) override { unexpected("IPlayer::setSuicide"); }
    bool getSuicide() const override { unexpected("IPlayer::getSuicide"); }
    bool getIsConnected() const override { unexpected("IPlayer::getIsConnected"); }
    void setIsConnected(bool) override { unexpected("IPlayer::setIsConnected"); }
    float getCrossHairFadeValue() const override { unexpected("IPlayer::getCrossHairFadeValue"); }
    void setViewDistance(float) override { unexpected("IPlayer::setViewDistance"); }
    float getViewDistance() const override { unexpected("IPlayer::getViewDistance"); }
    void setFov(float fov, float time) override { unexpected("IPlayer::setFov"); }
    float getFov() const override { unexpected("IPlayer::getFov"); }
    float getEndFov() const override { unexpected("IPlayer::getEndFov"); }
    ActionBuffer* getActionBuffer() override { unexpected("IPlayer::getActionBuffer"); }
    void* getAIPlayerInput() const override { unexpected("IPlayer::getAIPlayerInput"); }
    void setDefaultVehicle(IObject*) override { unexpected("IPlayer::setDefaultVehicle"); }
    IObject* getDefaultVehicle() override { unexpected("IPlayer::getDefaultVehicle"); }
    IObject* getFreeCamera() const override { unexpected("IPlayer::getFreeCamera"); }
    void setFreeCameraEnabled(bool) override { unexpected("IPlayer::setFreeCameraEnabled"); }
    void toggleFreeCameraEnabled() override { unexpected("IPlayer::toggleFreeCameraEnabled"); }
    void setOverheadCameraEnabled(bool) override { unexpected("IPlayer::setOverheadCameraEnabled"); }
    bool getOverheadCameraEnabled() const override { unexpected("IPlayer::getOverheadCameraEnabled"); }
    void setOverheadCameraTopDownView(bool) override { unexpected("IPlayer::setOverheadCameraTopDownView"); }
    bool getOverheadCameraTopDownView() const override { unexpected("IPlayer::getOverheadCameraTopDownView"); }
    void setCommanderCamOverhead(bool) override { unexpected("IPlayer::setCommanderCamOverhead"); }
    bool getCommanderCamOverhead() const override { unexpected("IPlayer::getCommanderCamOverhead"); }
    int getTickCount() const override { unexpected("IPlayer::getTickCount"); }
    void addTick() override { unexpected("IPlayer::addTick"); }
    void subTick() override { unexpected("IPlayer::subTick"); }
    void resetTickCounter() override { unexpected("IPlayer::resetTickCounter"); }
    float getTimeOutSideWorld() const override { unexpected("IPlayer::getTimeOutSideWorld"); }
    void setTeam(int team, bool) override { unexpected("IPlayer::setTeam"); }
    int getTeam() const override { unexpected("IPlayer::getTeam"); }
    unsigned int getSpawnAtTick() const override { unexpected("IPlayer::getSpawnAtTick"); }
    void setSpawnAtTick(unsigned int tick) override { unexpected("IPlayer::setSpawnAtTick"); }
    float getTimeToSpawn() const override { unexpected("IPlayer::getTimeToSpawn"); }
    float getManDownTimeLeft() const override { unexpected("IPlayer::getManDownTimeLeft"); }
    void setIsManDown(bool) override { unexpected("IPlayer::setIsManDown"); }
    bool getIsManDown() override { unexpected("IPlayer::getIsManDown"); }
    unsigned int getManDownAtTick() override { unexpected("IPlayer::getManDownAtTick"); }
    void setSquadId(unsigned int id) override { unexpected("IPlayer::setSquadId"); }
    int getSquadId() const override { unexpected("IPlayer::getSquadId"); }
    void setIsCommander(bool) override { unexpected("IPlayer::setIsCommander"); }
    bool getIsCommander() const override { unexpected("IPlayer::getIsCommander"); }
    void setUAVIsReady(bool) override { unexpected("IPlayer::setUAVIsReady"); }
    bool getUAVIsReady() const override { unexpected("IPlayer::getUAVIsReady"); }
    void setSatelliteIsReady(bool) override { unexpected("IPlayer::setSatelliteIsReady"); }
    bool getSatelliteIsReady() const override { unexpected("IPlayer::getSatelliteIsReady"); }
    void setLastTimeCommander(double t) override { unexpected("IPlayer::setLastTimeCommander"); }
    double getLastTimeCommander() const override { unexpected("IPlayer::getLastTimeCommander"); }
    void setCommanderReapplyWaitTime(double t) override { unexpected("IPlayer::setCommanderReapplyWaitTime"); }
    double getCommanderReapplyWaitTime() const override { unexpected("IPlayer::getCommanderReapplyWaitTime"); }
    void setIsSquadLeader(bool) override { unexpected("IPlayer::setIsSquadLeader"); }
    bool getIsSquadLeader() const override { unexpected("IPlayer::getIsSquadLeader"); }
    bool getIsGassed() override { unexpected("IPlayer::getIsGassed"); }
    void setIsInsideCP(bool) override { unexpected("IPlayer::setIsInsideCP"); }
    bool getIsInsideCP() override { unexpected("IPlayer::getIsInsideCP"); }
    void setHasSimulatedFrame(bool) override { unexpected("IPlayer::setHasSimulatedFrame"); }
    bool getHasSimulatedFrame() const override { unexpected("IPlayer::getHasSimulatedFrame"); }
    void setSpawnGroup(int group) override { unexpected("IPlayer::setSpawnGroup"); }
    int getSpawnGroup() const override { unexpected("IPlayer::getSpawnGroup"); }
    void setInterpolationTime(unsigned int ms) override { unexpected("IPlayer::setInterpolationTime"); }
    unsigned int getInterpolationTime() const override { unexpected("IPlayer::getInterpolationTime"); }
    void setPing(unsigned int ping) override { unexpected("IPlayer::setPing"); }
    unsigned int getPing() const override { unexpected("IPlayer::getPing"); }
    void setKit(int kit) override { unexpected("IPlayer::setKit"); }
    int getKit() const override { unexpected("IPlayer::getKit"); }
    PlayerScoreData* getScoreData() override { unexpected("IPlayer::getScoreData"); }
    void setFlagHolder(bool) override { unexpected("IPlayer::setFlagHolder"); }
    bool getFlagHolder() const override { unexpected("IPlayer::getFlagHolder"); }
    ICameraObject* getCamera() const override { unexpected("IPlayer::getCamera"); }
    void setCamera(ICameraObject* camera) override { unexpected("IPlayer::setCamera"); }
    bool hasFirstPersonView() const override { unexpected("IPlayer::hasFirstPersonView"); }
    double getTimeConnected() const override { unexpected("IPlayer::getTimeConnected"); }
    io::INetworkable* getNetworkable() override { unexpected("IPlayer::getNetworkable"); }
    bool getStateSync(io::BitStream* stream, IObject* object) override { unexpected("IPlayer::getStateSync"); }
    bool setStateSync(io::BitStream* stream, IObject* object, double) override { unexpected("IPlayer::setStateSync"); }
    void reset() override { unexpected("IPlayer::reset"); }
    void setFiringState(bool) override { unexpected("IPlayer::setFiringState"); }
    bool getFiringState() const override { unexpected("IPlayer::getFiringState"); }
    void setAltFiringState(bool) override { unexpected("IPlayer::setAltFiringState"); }
    bool getAltFiringState() const override { unexpected("IPlayer::getAltFiringState"); }
    void setFlareFiringState(bool) override { unexpected("IPlayer::setFlareFiringState"); }
    bool getFlareFiringState() const override { unexpected("IPlayer::getFlareFiringState"); }
    void setSprintState(bool) override { unexpected("IPlayer::setSprintState"); }
    bool getSprintState() const override { unexpected("IPlayer::getSprintState"); }
    void setSprintStateLastTick(bool) override { unexpected("IPlayer::setSprintStateLastTick"); }
    bool getSprintStateLastTick() const override { unexpected("IPlayer::getSprintStateLastTick"); }
    void setReloadState() override { unexpected("IPlayer::setReloadState"); }
    bool getAndResetReloadState() override { unexpected("IPlayer::getAndResetReloadState"); }
    void updateFiringState(unsigned int, IObject*) override { unexpected("IPlayer::updateFiringState"); }
    void updateGhostFiringState() override { unexpected("IPlayer::updateGhostFiringState"); }
    void setHitIndicator(bool) override { unexpected("IPlayer::setHitIndicator"); }
    bool getHitIndicator() const override { unexpected("IPlayer::getHitIndicator"); }
    float updateHitFromDirection(const Vec3& direction) override { unexpected("IPlayer::updateHitFromDirection"); }
    float getHitFromDirection() const override { unexpected("IPlayer::getHitFromDirection"); }
    void updateCameraShake(float amount) override { unexpected("IPlayer::updateCameraShake"); }
    float getCameraShake() const override { unexpected("IPlayer::getCameraShake"); }
    unsigned int getSquadInviteMask() const override { unexpected("IPlayer::getSquadInviteMask"); }
    void setSquadInviteMask(unsigned int mask) override { unexpected("IPlayer::setSquadInviteMask"); }
    bool getIsInvitedToSquad(int squad) const override { unexpected("IPlayer::getIsInvitedToSquad"); }
    void setKitTimeStamp(double t) override { unexpected("IPlayer::setKitTimeStamp"); }
    double getKitTimeStamp() const override { unexpected("IPlayer::getKitTimeStamp"); }
    void setCurrentFiringButton(io::PlayerInputMap button) override { unexpected("IPlayer::setCurrentFiringButton"); }
    io::PlayerInputMap getCurrentFiringButton() const override { unexpected("IPlayer::getCurrentFiringButton"); }
    void setPersonalDecalTemplate(const IObjectTemplate* decal) override { unexpected("IPlayer::setPersonalDecalTemplate"); }
    const IObjectTemplate* getPersonalDecalTemplate() const override { unexpected("IPlayer::getPersonalDecalTemplate"); }
    void enableVOHelp(bool) override { unexpected("IPlayer::enableVOHelp"); }
    void enableVOHelpMessage(int) override { unexpected("IPlayer::enableVOHelpMessage"); }
    void disableVOHelpMessage(int) override { unexpected("IPlayer::disableVOHelpMessage"); }
    bool getVOHelpMessagePlayed(int) override { unexpected("IPlayer::getVOHelpMessagePlayed"); }
    void doAddTouchItem(IPlayer* other) override { unexpected("IPlayer::doAddTouchItem"); }
    void setHealRepairedAtTick(unsigned int tick, bool giving, bool repairing) override { unexpected("IPlayer::setHealRepairedAtTick"); }
    unsigned int getHealRepairedAtTick() const override { unexpected("IPlayer::getHealRepairedAtTick"); }
    void setGaveAmmoAtTick(unsigned int tick, bool giving) override { unexpected("IPlayer::setGaveAmmoAtTick"); }
    unsigned int getGaveAmmoAtTick() const override { unexpected("IPlayer::getGaveAmmoAtTick"); }
    void setFailedExitAtTick(unsigned int tick) override { unexpected("IPlayer::setFailedExitAtTick"); }
    unsigned int getFailedExitAtTick() const override { unexpected("IPlayer::getFailedExitAtTick"); }
    bool getIsGivingAbility() const override { unexpected("IPlayer::getIsGivingAbility"); }
    bool getIsRepairing() const override { unexpected("IPlayer::getIsRepairing"); }
    bool getIsGivingReplenish() const override { unexpected("IPlayer::getIsGivingReplenish"); }
    int getChosenUnlockLevel(int kit, int slot) const override { unexpected("IPlayer::getChosenUnlockLevel"); }
    void setChosenUnlockLevel(int kit, int slot, int level) override { unexpected("IPlayer::setChosenUnlockLevel"); }
    bool isUnlocked(int kit, int slot, int level) override { unexpected("IPlayer::isUnlocked"); }
    int getUnlockLevel(int kit, int slot) override { unexpected("IPlayer::getUnlockLevel"); }
    void setUnlocks(std::map<int, int>* unlocks) override { unexpected("IPlayer::setUnlocks"); }
    std::map<int, int>* getUnlocks() override { unexpected("IPlayer::getUnlocks"); }
    void setPeerAddress(const std::string& address) override { unexpected("IPlayer::setPeerAddress"); }
    const std::string& getPeerAddress() const override { unexpected("IPlayer::getPeerAddress"); }
    bool getVoipEnabled() override { unexpected("IPlayer::getVoipEnabled"); }
    unsigned short getVoipSessionId() override { unexpected("IPlayer::getVoipSessionId"); }
    void setVoipSessionId(unsigned short id) override { unexpected("IPlayer::setVoipSessionId"); }
    bool getIsMuted() override { unexpected("IPlayer::getIsMuted"); }
    bool getIsTalking() override { unexpected("IPlayer::getIsTalking"); }
    double getLastTalkTime() override { unexpected("IPlayer::getLastTalkTime"); }
    double getStopTalkingThreshold() override { unexpected("IPlayer::getStopTalkingThreshold"); }
    void setIsMuted(bool) override { unexpected("IPlayer::setIsMuted"); }
    void setIsTalking(bool) override { unexpected("IPlayer::setIsTalking"); }
    void setVoipEnabled(bool) override { unexpected("IPlayer::setVoipEnabled"); }
    void setVotingStatus(PlayerVotingStatus status) override { unexpected("IPlayer::setVotingStatus"); }
    PlayerVotingStatus getVotingStatus() override { unexpected("IPlayer::getVotingStatus"); }
    void setParticipateInVoting(bool) override { unexpected("IPlayer::setParticipateInVoting"); }
    bool getParticipateInVoting() override { unexpected("IPlayer::getParticipateInVoting"); }
    void setTargetOfVote(bool) override { unexpected("IPlayer::setTargetOfVote"); }
    bool getTargetOfVote() override { unexpected("IPlayer::getTargetOfVote"); }
    int getProfileId() override { unexpected("IPlayer::getProfileId"); }
    void setProfileId(int id) override { unexpected("IPlayer::setProfileId"); }
    void setBlurryVision(float, float, float) override { unexpected("IPlayer::setBlurryVision"); }
    float getBlurValue() const override { unexpected("IPlayer::getBlurValue"); }
    bool getIsNearGasCloud() const override { unexpected("IPlayer::getIsNearGasCloud"); }
    bool getIsGasMaskOn() const override { unexpected("IPlayer::getIsGasMaskOn"); }
    void setIsGasMaskOn(bool, bool) override { unexpected("IPlayer::setIsGasMaskOn"); }
    void setGasMaskHudItem(bool) override { unexpected("IPlayer::setGasMaskHudItem"); }
    float getPlayerGasMaskChangeTimer() const override { unexpected("IPlayer::getPlayerGasMaskChangeTimer"); }
    float getPlayerNightVisionChangeTimer() const override { unexpected("IPlayer::getPlayerNightVisionChangeTimer"); }
    bool getIsUnderwater() const override { unexpected("IPlayer::getIsUnderwater"); }
    float getNormalizedNightVisionPower() override { unexpected("IPlayer::getNormalizedNightVisionPower"); }
    void setNightVision(bool, bool) override { unexpected("IPlayer::setNightVision"); }
    void rechargeNightVision() override { unexpected("IPlayer::rechargeNightVision"); }
    void setVisualNightVision(bool) override { unexpected("IPlayer::setVisualNightVision"); }
    bool getNightVision() const override { unexpected("IPlayer::getNightVision"); }
    void disableFlashBangEffect() override { unexpected("IPlayer::disableFlashBangEffect"); }
    void disableBlurryVision() override { unexpected("IPlayer::disableBlurryVision"); }
    io::PlayerInput getPlayerInput() override { unexpected("IPlayer::getPlayerInput"); }
    void resetOnDeath() override { unexpected("IPlayer::resetOnDeath"); }
    bool isPlayerInNightMap() const override { unexpected("IPlayer::isPlayerInNightMap"); }
    void setTracePlayerId(int id) override { unexpected("IPlayer::setTracePlayerId"); }
    int getTracePlayerId() override { unexpected("IPlayer::getTracePlayerId"); }
    void allocateFreeCamera() override { unexpected("IPlayer::allocateFreeCamera"); }
    bool getIsThisASpam(bool) override { unexpected("IPlayer::getIsThisASpam"); }
    void setRadioSpamFlagCount(int) override { unexpected("IPlayer::setRadioSpamFlagCount"); }
    int getRadioSpamFlagCount() override { unexpected("IPlayer::getRadioSpamFlagCount"); }
    void setRadioSpamBlockedFlagCount(int) override { unexpected("IPlayer::setRadioSpamBlockedFlagCount"); }
    int getRadioSpamBlockedFlagCount() override { unexpected("IPlayer::getRadioSpamBlockedFlagCount"); }
    void setRadioSpamFilterFlagTimer(float) override { unexpected("IPlayer::setRadioSpamFilterFlagTimer"); }
    float getRadioSpamFilterFlagTimer() override { unexpected("IPlayer::getRadioSpamFilterFlagTimer"); }
    void resetSpamFilter() override { unexpected("IPlayer::resetSpamFilter"); }
    void getCloseMines(std::vector<IObject*>& out) const override { unexpected("IPlayer::getCloseMines"); }
    bool getAutoReload() const override { unexpected("IPlayer::getAutoReload"); }
    void setAutoReload(bool) override { unexpected("IPlayer::setAutoReload"); }
    void updateClientSettings() override { unexpected("IPlayer::updateClientSettings"); }
};
struct FakePco final : IPlayerControlObject {
    IPlayer* occupant = 0;
    bool occupied = false;
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("IPlayerControlObject::queryInterface"); }
    IPlayer* getOccupingPlayer() const override { return occupant; }
    bool isOccupied() const override { return occupied; }
    IObject* getOccupingObject() const override { unexpected("IPlayerControlObject::getOccupingObject"); }
    bool enter(IPlayer* player) override { unexpected("IPlayerControlObject::enter"); }
    void exit() override { unexpected("IPlayerControlObject::exit"); }
    void exitPlayer() override { unexpected("IPlayerControlObject::exitPlayer"); }
    void onEnter(IObject*) override { unexpected("IPlayerControlObject::onEnter"); }
    void onExit() override { unexpected("IPlayerControlObject::onExit"); }
    int getPcoId() const override { unexpected("IPlayerControlObject::getPcoId"); }
    void setPcoId(int id) override { unexpected("IPlayerControlObject::setPcoId"); }
    std::map<int, ICameraObject*>* getCameras() override { unexpected("IPlayerControlObject::getCameras"); }
    std::map<int, ICameraObject*>* getOwnCameras() override { unexpected("IPlayerControlObject::getOwnCameras"); }
    void nextCamera() override { unexpected("IPlayerControlObject::nextCamera"); }
    ICameraObject* getActiveCamera() override { unexpected("IPlayerControlObject::getActiveCamera"); }
    void setActiveCamera(int) override { unexpected("IPlayerControlObject::setActiveCamera"); }
    bool getExitPosition(Vec3& out) override { unexpected("IPlayerControlObject::getExitPosition"); }
    bool getAltExitPosition(Vec3& out) override { unexpected("IPlayerControlObject::getAltExitPosition"); }
    void addExternalCamera(ICameraObject* camera) override { unexpected("IPlayerControlObject::addExternalCamera"); }
    void removeExternalCamera(ICameraObject* camera) override { unexpected("IPlayerControlObject::removeExternalCamera"); }
    bool canSpawn(const Mat4& transform) override { unexpected("IPlayerControlObject::canSpawn"); }
    std::map<int, IPlayerControlObject*>* getPcos() override { unexpected("IPlayerControlObject::getPcos"); }
    const std::vector<int>* getPcoPosId() override { unexpected("IPlayerControlObject::getPcoPosId"); }
    void addWeapon(IObject* weapon) override { unexpected("IPlayerControlObject::addWeapon"); }
    void* getWeapons() override { unexpected("IPlayerControlObject::getWeapons"); }
    IObject* getActiveWeapon(int) const override { unexpected("IPlayerControlObject::getActiveWeapon"); }
    void* getWeapons(int) override { unexpected("IPlayerControlObject::getWeapons"); }
    void setActiveWeapon(IObject* weapon, int) override { unexpected("IPlayerControlObject::setActiveWeapon"); }
    void toggleOnWeaponLock(IObject*) override { unexpected("IPlayerControlObject::toggleOnWeaponLock"); }
    int getArtPos() override { unexpected("IPlayerControlObject::getArtPos"); }
    bool checkSpecialToggle(io::PlayerInput input) override { unexpected("IPlayerControlObject::checkSpecialToggle"); }
    bool checkSpecialToggleLastUpdate(io::PlayerInputMap map) override { unexpected("IPlayerControlObject::checkSpecialToggleLastUpdate"); }
    void setTeam(int team) override { unexpected("IPlayerControlObject::setTeam"); }
    void setDefaultTeam(int team) override { unexpected("IPlayerControlObject::setDefaultTeam"); }
    int getDefaultTeam() override { unexpected("IPlayerControlObject::getDefaultTeam"); }
    int getTeam() override { unexpected("IPlayerControlObject::getTeam"); }
    float getTimeToLiveUnused() override { unexpected("IPlayerControlObject::getTimeToLiveUnused"); }
    void setTimeToLiveUnused(float t) override { unexpected("IPlayerControlObject::setTimeToLiveUnused"); }
    bool isAllowedToExit() override { unexpected("IPlayerControlObject::isAllowedToExit"); }
    void addPlayerInputObject(IObject*) override { unexpected("IPlayerControlObject::addPlayerInputObject"); }
    void removePlayerInputObject(IObject*) override { unexpected("IPlayerControlObject::removePlayerInputObject"); }
    void setTimeSpawned(float t) override { unexpected("IPlayerControlObject::setTimeSpawned"); }
    float getTimeSpawned() const override { unexpected("IPlayerControlObject::getTimeSpawned"); }
    void setObjectSpawnerId(int id) override { unexpected("IPlayerControlObject::setObjectSpawnerId"); }
    int getObjectSpawnerId() const override { unexpected("IPlayerControlObject::getObjectSpawnerId"); }
    void setObjectSpawnerHolding(bool holding) override { unexpected("IPlayerControlObject::setObjectSpawnerHolding"); }
    bool getObjectSpawnerHolding() const override { unexpected("IPlayerControlObject::getObjectSpawnerHolding"); }
    const Vec3& getNameTagOffset() override { unexpected("IPlayerControlObject::getNameTagOffset"); }
    const Vec3& getRidingWithOffset() override { unexpected("IPlayerControlObject::getRidingWithOffset"); }
    float getRidingWithDistMod() override { unexpected("IPlayerControlObject::getRidingWithDistMod"); }
    int getVehicleCameraShake() override { unexpected("IPlayerControlObject::getVehicleCameraShake"); }
    float getVehicleFov() override { unexpected("IPlayerControlObject::getVehicleFov"); }
    void addVehicleMod(VehicleMod mod) override { unexpected("IPlayerControlObject::addVehicleMod"); }
    void removeVehicleMod(VehicleMod mod) override { unexpected("IPlayerControlObject::removeVehicleMod"); }
    int getVehicleMod(VehicleMod mod) override { unexpected("IPlayerControlObject::getVehicleMod"); }
    const std::string& getSoundFilter() const override { unexpected("IPlayerControlObject::getSoundFilter"); }
    int getCockpitLod() override { unexpected("IPlayerControlObject::getCockpitLod"); }
    int getCockpitSubGeom() override { unexpected("IPlayerControlObject::getCockpitSubGeom"); }
    float getShakeFactor() const override { unexpected("IPlayerControlObject::getShakeFactor"); }
    bool getIsSprinting() const override { unexpected("IPlayerControlObject::getIsSprinting"); }
    void setIsSprinting(bool) override { unexpected("IPlayerControlObject::setIsSprinting"); }
    float getSprintStamina() const override { unexpected("IPlayerControlObject::getSprintStamina"); }
    void setSprintStamina(float) override { unexpected("IPlayerControlObject::setSprintStamina"); }
    void setSprintStaminaDissipationFactor(float) override { unexpected("IPlayerControlObject::setSprintStaminaDissipationFactor"); }
    float getSprintFactor() override { unexpected("IPlayerControlObject::getSprintFactor"); }
    bool getIsUnderwater() const override { unexpected("IPlayerControlObject::getIsUnderwater"); }
    float getNormalizedUnderwaterTime() const override { unexpected("IPlayerControlObject::getNormalizedUnderwaterTime"); }
    void setDriverDecalObject(IObject*) override { unexpected("IPlayerControlObject::setDriverDecalObject"); }
    IObject* getDriverDecalObject() const override { unexpected("IPlayerControlObject::getDriverDecalObject"); }
    void addDriverDecalTransformation(const Mat4&) override { unexpected("IPlayerControlObject::addDriverDecalTransformation"); }
    std::vector<Mat4>* getDriverDecalTransformations() const override { unexpected("IPlayerControlObject::getDriverDecalTransformations"); }
    int getSpecialToggleWeaponInput() const override { unexpected("IPlayerControlObject::getSpecialToggleWeaponInput"); }
    void setIsDucking(bool) override { unexpected("IPlayerControlObject::setIsDucking"); }
    bool getIsDucking() override { unexpected("IPlayerControlObject::getIsDucking"); }
    bool getAutoUseAbility() const override { unexpected("IPlayerControlObject::getAutoUseAbility"); }
    bool applyCameraAnimation(const Mat4& in, Mat4& out) override { unexpected("IPlayerControlObject::applyCameraAnimation"); }
    bool allowEnterOtherVehicle() override { unexpected("IPlayerControlObject::allowEnterOtherVehicle"); }
    bool isChildSoldierActive() const override { unexpected("IPlayerControlObject::isChildSoldierActive"); }
    bool getNeverDrawAs1p() const override { unexpected("IPlayerControlObject::getNeverDrawAs1p"); }
    void removeChildVehicle(IObject*) override { unexpected("IPlayerControlObject::removeChildVehicle"); }
    int getAmmoStatus() override { unexpected("IPlayerControlObject::getAmmoStatus"); }
    void setAmmoStatus(int) override { unexpected("IPlayerControlObject::setAmmoStatus"); }
    void onFirstGhost() override { unexpected("IPlayerControlObject::onFirstGhost"); }
    void setGiveUnmannedInput(bool) override { unexpected("IPlayerControlObject::setGiveUnmannedInput"); }
    void preHandlePlayerInput(io::PlayerInput& input, float dt) const override { unexpected("IPlayerControlObject::preHandlePlayerInput"); }
};
struct FakeObject final : IObject {
    Vec3 position;
    IPlayerControlObject* pco = 0;
    FakeObject(IObject* parent, const IObjectTemplate* objectTemplate, IPhysicsNode* physics, Vec3 at,
               unsigned int flags)
        : position(at)
    {
        m_parent = parent;
        m_template = objectTemplate;
        m_physicsNode = physics;
        m_flags = flags;
    }
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int iid) const override
    {
        return iid == IID_IPlayerControlObject ? static_cast<void*>(pco) : 0;
    }
    const Vec3& getAbsolutePosition() const override { return position; }
    unsigned int getClassID() const override { unexpected("IObject::getClassID"); }
    WeakPtr<IObject> getWeakPtr() override { unexpected("IObject::getWeakPtr"); }
    void init() override { unexpected("IObject::init"); }
    void disableCachedObject() override { unexpected("IObject::disableCachedObject"); }
    void enableCachedObject() override { unexpected("IObject::enableCachedObject"); }
    unsigned int getObjectCacheID() const override { unexpected("IObject::getObjectCacheID"); }
    void setObjectCacheID(unsigned int) override { unexpected("IObject::setObjectCacheID"); }
    void destroy(bool force) override { unexpected("IObject::destroy"); }
    void forceDestroy() override { unexpected("IObject::forceDestroy"); }
    bool isObjectDestroyed() override { unexpected("IObject::isObjectDestroyed"); }
    void setObjectInDestroyList(bool) override { unexpected("IObject::setObjectInDestroyList"); }
    bool getObjectInDestroyList() const override { unexpected("IObject::getObjectInDestroyList"); }
    void setGridCullerObject(CullerObject*) override { unexpected("IObject::setGridCullerObject"); }
    CullerObject* getGridCullerObject() const override { unexpected("IObject::getGridCullerObject"); }
    void setLatencyCullerObject(CullerObject*) override { unexpected("IObject::setLatencyCullerObject"); }
    CullerObject* getLatencyCullerObject() const override { unexpected("IObject::getLatencyCullerObject"); }
    bool setComponent(unsigned int classId, IBase*) override { unexpected("IObject::setComponent"); }
    IBase* queryComponent(unsigned int classId, unsigned int iid) override { unexpected("IObject::queryComponent"); }
    void updateFlags(unsigned int set, unsigned int clear) override { unexpected("IObject::updateFlags"); }
    const std::string& getName() const override { unexpected("IObject::getName"); }
    void setName(const std::string&) override { unexpected("IObject::setName"); }
    void setAbsolutePosition(const Vec3&) override { unexpected("IObject::setAbsolutePosition"); }
    const Vec3& getAbsolutePositionSecondary() const override { unexpected("IObject::getAbsolutePositionSecondary"); }
    void setAbsolutePositionSecondary(const Vec3&) override { unexpected("IObject::setAbsolutePositionSecondary"); }
    const Vec3& getRotationSecondary() override { unexpected("IObject::getRotationSecondary"); }
    void setRotationSecondary(const Vec3&) override { unexpected("IObject::setRotationSecondary"); }
    const Mat4& getAbsoluteTransformation() const override { unexpected("IObject::getAbsoluteTransformation"); }
    const Mat4& getInvAbsoluteTransformation() const override { unexpected("IObject::getInvAbsoluteTransformation"); }
    void setAbsoluteTransformation(const Mat4&) override { unexpected("IObject::setAbsoluteTransformation"); }
    const Mat4& getVisualAbsoluteTransformation() const override { unexpected("IObject::getVisualAbsoluteTransformation"); }
    float getVisualBoundingRadius() const override { unexpected("IObject::getVisualBoundingRadius"); }
    float getPhysicBoundingRadius() const override { unexpected("IObject::getPhysicBoundingRadius"); }
    float getBoundingRadiusModifier() const override { unexpected("IObject::getBoundingRadiusModifier"); }
    LocalBoundingBox* getFullBoundingBox() override { unexpected("IObject::getFullBoundingBox"); }
    LocalBoundingBox* getMineBoundingBox() override { unexpected("IObject::getMineBoundingBox"); }
    void handleVisualUpdate(float, float) override { unexpected("IObject::handleVisualUpdate"); }
    void handleFrameUpdate(float) override { unexpected("IObject::handleFrameUpdate"); }
    void handleUpdate(float, unsigned int) override { unexpected("IObject::handleUpdate"); }
    bool handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&, int, int, CollisionFaceInfo*, CollisionFaceInfo*) override { unexpected("IObject::handleCollision"); }
    void updateGeometry() override { unexpected("IObject::updateGeometry"); }
    int getUpdateFrequency() const override { unexpected("IObject::getUpdateFrequency"); }
    UpdateFreqencyType getUpdateFrequencyType() const override { unexpected("IObject::getUpdateFrequencyType"); }
    void setUpdateFrequencyType(UpdateFreqencyType) override { unexpected("IObject::setUpdateFrequencyType"); }
    int getGeometryPart() const override { unexpected("IObject::getGeometryPart"); }
    bool hasCollisionLod(int) const override { unexpected("IObject::hasCollisionLod"); }
    int getMaterial() const override { unexpected("IObject::getMaterial"); }
    void SetActiveLod(ObjectLodType) override { unexpected("IObject::SetActiveLod"); }
    io::INetworkable* getNetworkable() const override { unexpected("IObject::getNetworkable"); }
    void* getDecalObject() const override { unexpected("IObject::getDecalObject"); }
    const Vec3& getRelativePosition() const override { unexpected("IObject::getRelativePosition"); }
    void setRelativePosition(const Vec3&) override { unexpected("IObject::setRelativePosition"); }
    const Mat4& getRelativeTransformation() const override { unexpected("IObject::getRelativeTransformation"); }
    void setRelativeTransformation(const Mat4&) override { unexpected("IObject::setRelativeTransformation"); }
    IObject* getChild() const override { unexpected("IObject::getChild"); }
    void addChild(IObject*) override { unexpected("IObject::addChild"); }
    void removeChild(IObject*) override { unexpected("IObject::removeChild"); }
    void resetCachedRootParents() override { unexpected("IObject::resetCachedRootParents"); }
    void handlePlayerInput(IPlayer*, const io::PlayerInput&, float, unsigned int) override { unexpected("IObject::handlePlayerInput"); }
    void handleMessage(TemplateMessage, IPlayer*, bool) override { unexpected("IObject::handleMessage"); }
    char getInputId() const override { unexpected("IObject::getInputId"); }
    void setInputId(char) override { unexpected("IObject::setInputId"); }
    char getChildId() const override { unexpected("IObject::getChildId"); }
    void setChildId(char) override { unexpected("IObject::setChildId"); }
    unsigned char getAlpha() override { unexpected("IObject::getAlpha"); }
    unsigned char getLayer() override { unexpected("IObject::getLayer"); }
    void setLayer(unsigned char) override { unexpected("IObject::setLayer"); }
    unsigned char getGroup() override { unexpected("IObject::getGroup"); }
    void setGroup(unsigned char) override { unexpected("IObject::setGroup"); }
    void setIsOvergrowth(bool) override { unexpected("IObject::setIsOvergrowth"); }
    bool getIsOvergrowth() override { unexpected("IObject::getIsOvergrowth"); }
    void setNotInAI(bool) override { unexpected("IObject::setNotInAI"); }
    bool getNotInAI() const override { unexpected("IObject::getNotInAI"); }
    void setLightSourceMask(int) override { unexpected("IObject::setLightSourceMask"); }
    int getLightSourceMask() const override { unexpected("IObject::getLightSourceMask"); }
    bool getDisableChildren() override { unexpected("IObject::getDisableChildren"); }
    void setDisableChildren(bool) override { unexpected("IObject::setDisableChildren"); }
    std::string getArmorInfo() override { unexpected("IObject::getArmorInfo"); }
    bool isCreatedByLocalPlayer() const override { unexpected("IObject::isCreatedByLocalPlayer"); }
    void internal_assignNewObjectID() override { unexpected("IObject::internal_assignNewObjectID"); }
    void destroyObject() override { unexpected("IObject::destroyObject"); }
    void addSibling(IObject*) override { unexpected("IObject::addSibling"); }
    void removeSibling(IObject*) override { unexpected("IObject::removeSibling"); }
    void setParent(IObject*) override { unexpected("IObject::setParent"); }
};

// PlayerManager and ObjectManager: every virtual defined here, failing loudly except the two used.
PlayerManager::PlayerManager() {}
PlayerManager::~PlayerManager() { unexpected("PlayerManager::~PlayerManager"); }
IPlayer* PlayerManager::getLocalHumanPlayer() { return s_localPlayer; }
void PlayerManager::addRef() { unexpected("PlayerManager::addRef"); }
int PlayerManager::getRef() { unexpected("PlayerManager::getRef"); }
int PlayerManager::release() { unexpected("PlayerManager::release"); }
void* PlayerManager::queryInterface(unsigned int iid) const { unexpected("PlayerManager::queryInterface"); }
void PlayerManager::addPlayer(IPlayer* player) { unexpected("PlayerManager::addPlayer"); }
void PlayerManager::removePlayer(IPlayer* player) { unexpected("PlayerManager::removePlayer"); }
void PlayerManager::removePlayerId(int id) { unexpected("PlayerManager::removePlayerId"); }
IPlayer* PlayerManager::getPlayerFromName(const std::string& name) { unexpected("PlayerManager::getPlayerFromName"); }
IPlayer* PlayerManager::getPlayerFromId(int id) { unexpected("PlayerManager::getPlayerFromId"); }
IPlayer* PlayerManager::getNextPlayerFromId(int id) { unexpected("PlayerManager::getNextPlayerFromId"); }
IPlayer* PlayerManager::getPlayerFromProfileId(int profileId) { unexpected("PlayerManager::getPlayerFromProfileId"); }
IPlayer* PlayerManager::getPlayerFromIdString(const std::string& id) { unexpected("PlayerManager::getPlayerFromIdString"); }
void PlayerManager::setLocalHumanPlayer(IPlayer* player) { unexpected("PlayerManager::setLocalHumanPlayer"); }
void PlayerManager::setLocalHumanPlayer(int id) { unexpected("PlayerManager::setLocalHumanPlayer"); }
const std::list<IPlayer*>& PlayerManager::getPlayers() { unexpected("PlayerManager::getPlayers"); }
const std::list<IPlayer*>& PlayerManager::getPlayersSortedByScore() { unexpected("PlayerManager::getPlayersSortedByScore"); }
const std::list<IPlayer*>& PlayerManager::getPlayersSortedByRank() { unexpected("PlayerManager::getPlayersSortedByRank"); }
const std::list<IPlayer*>& PlayerManager::getPlayers(int team) { unexpected("PlayerManager::getPlayers"); }
const std::list<IPlayer*>& PlayerManager::getPlayersSortedByScore(int team) { unexpected("PlayerManager::getPlayersSortedByScore"); }
const std::list<IPlayer*>& PlayerManager::getPlayersSortedByRank(int team) { unexpected("PlayerManager::getPlayersSortedByRank"); }
const std::list<IPlayer*>& PlayerManager::getPlayersInSquad(int team, int squad) { unexpected("PlayerManager::getPlayersInSquad"); }
const std::list<IPlayer*>& PlayerManager::getPlayersInSquadSortedByScore(int team, int squad) { unexpected("PlayerManager::getPlayersInSquadSortedByScore"); }
const std::list<IPlayer*>& PlayerManager::getPlayersInSquadSortedByRank(int team, int squad) { unexpected("PlayerManager::getPlayersInSquadSortedByRank"); }
const std::list<IPlayer*>& PlayerManager::getTeamSquadLeaders(int team) { unexpected("PlayerManager::getTeamSquadLeaders"); }
void PlayerManager::updatePlayers(float deltaTime) { unexpected("PlayerManager::updatePlayers"); }
void PlayerManager::registerPlayerClass(unsigned int classId, const std::string& className) { unexpected("PlayerManager::registerPlayerClass"); }
void PlayerManager::getPlayerClasses(std::list<std::pair<unsigned int, std::string> >& out) const { unexpected("PlayerManager::getPlayerClasses"); }
const std::string& PlayerManager::getClassNameFromPlayerClassID(unsigned int classId) const { unexpected("PlayerManager::getClassNameFromPlayerClassID"); }
IPlayer* PlayerManager::createPlayer(unsigned int classId, const std::string& name, int id) { unexpected("PlayerManager::createPlayer"); }
IPlayer* PlayerManager::createPlayer(const std::string& className, const std::string& name, int id) { unexpected("PlayerManager::createPlayer"); }
bool PlayerManager::saveAllPlayers(const std::string& fileName) { unexpected("PlayerManager::saveAllPlayers"); }
IPlayer* PlayerManager::getActivePlayer() const { unexpected("PlayerManager::getActivePlayer"); }
void PlayerManager::setActivePlayer(IPlayer* player) { unexpected("PlayerManager::setActivePlayer"); }
int PlayerManager::getNumberOfPlayers() const { unexpected("PlayerManager::getNumberOfPlayers"); }
int PlayerManager::getNumberOfHumanPlayers() const { unexpected("PlayerManager::getNumberOfHumanPlayers"); }
int PlayerManager::getNumberOfAliveHumanPlayers() const { unexpected("PlayerManager::getNumberOfAliveHumanPlayers"); }
int PlayerManager::getNrOfPlayerInTeam(int team) { unexpected("PlayerManager::getNrOfPlayerInTeam"); }
int PlayerManager::getNrOfPlayerInSquads(int team) { unexpected("PlayerManager::getNrOfPlayerInSquads"); }
int PlayerManager::getNrOfPlayersInSquad(int team, int squad) { unexpected("PlayerManager::getNrOfPlayersInSquad"); }
const std::list<IPlayer*>& PlayerManager::getInvitedPlayers(int team, int squad) { unexpected("PlayerManager::getInvitedPlayers"); }
int PlayerManager::getNrOfActiveSquads(int team) { unexpected("PlayerManager::getNrOfActiveSquads"); }
int PlayerManager::getTeamToAddPlayer(float team1Weight, float team2Weight) { unexpected("PlayerManager::getTeamToAddPlayer"); }
bool PlayerManager::deactivatePlayer(int id) { unexpected("PlayerManager::deactivatePlayer"); }
bool PlayerManager::reactivatePlayer(int id) { unexpected("PlayerManager::reactivatePlayer"); }
bool PlayerManager::renamePlayer(IPlayer* player, std::string name) { unexpected("PlayerManager::renamePlayer"); }

static std::vector<IObject*> s_gridObjects;
ObjectManager::ObjectManager() {}
ObjectManager::~ObjectManager() { unexpected("ObjectManager::~ObjectManager"); }
void ObjectManager::getObjectsFromGrid(const Vec3& centre, float radius, std::vector<IObject*>& out,
                                       ObjectPredicator* pred)
{
    char b[96];
    std::snprintf(b, sizeof(b), "grid %.1f %.1f %.1f r=%.1f", centre.x, centre.y, centre.z, radius);
    calls.push_back(b);
    out.clear();
    for (IObject* object : s_gridObjects)
        if (pred->predicate(object))
            out.push_back(object);
}
void ObjectManager::getObjectsFromGrid(std::vector<IObject*>&, ObjectPredicator*) { unexpected("ObjectManager::getObjectsFromGrid(out)"); }
void ObjectManager::getObjectsFromGrid(const Frustum&, std::vector<IObject*>&, ObjectPredicator*, bool, float, float) { unexpected("ObjectManager::getObjectsFromGrid(frustum)"); }
void ObjectManager::getObjectsFromGrid(const Vec3&, const Vec3&, std::vector<IObject*>&, ObjectPredicator*, bool, int, bool) { unexpected("ObjectManager::getObjectsFromGrid(ray)"); }
void ObjectManager::addRef() { unexpected("ObjectManager::addRef"); }
int ObjectManager::getRef() { unexpected("ObjectManager::getRef"); }
int ObjectManager::release() { unexpected("ObjectManager::release"); }
void* ObjectManager::queryInterface(unsigned int iid) const { unexpected("ObjectManager::queryInterface"); }
IObject* ObjectManager::createObject(const IObjectTemplate* templ, unsigned int id, const Mat4* transform) { unexpected("ObjectManager::createObject"); }
void ObjectManager::destroyObject(IObject* object) { unexpected("ObjectManager::destroyObject"); }
void ObjectManager::setDrawStats(int level) { unexpected("ObjectManager::setDrawStats"); }
int ObjectManager::getDrawStats() const { unexpected("ObjectManager::getDrawStats"); }
void ObjectManager::initGrid(const Vec2& min, const Vec2& max, int cells, const Vec3& origin) { unexpected("ObjectManager::initGrid"); }
IObject* ObjectManager::getObjectFromNameHash(const std::string& name) { unexpected("ObjectManager::getObjectFromNameHash"); }
void ObjectManager::updateObjects(float deltaTime, UpdateFreqencyType type, unsigned int frame) { unexpected("ObjectManager::updateObjects"); }
void ObjectManager::handleFrameUpdates(float deltaTime) { unexpected("ObjectManager::handleFrameUpdates"); }
IObject* ObjectManager::getObjectFromGrid(unsigned int id) { unexpected("ObjectManager::getObjectFromGrid"); }
std::map<unsigned int, IObject*>& ObjectManager::getAllObjectsFromGrid() { unexpected("ObjectManager::getAllObjectsFromGrid"); }
const std::map<unsigned int, IObject*>& ObjectManager::getAllRegisteredGamePlayObjects() const { unexpected("ObjectManager::getAllRegisteredGamePlayObjects"); }
void ObjectManager::getNoCollisionObjectsFromGrid(const Vec3& centre, float radius, std::vector<IObject*>& out, ObjectPredicator* pred) { unexpected("ObjectManager::getNoCollisionObjectsFromGrid"); }
void ObjectManager::appendGetObjectsFromGrid(const Vec3& centre, float radius, std::vector<IObject*>& out, ObjectPredicator* pred) { unexpected("ObjectManager::appendGetObjectsFromGrid"); }
void ObjectManager::clearGetObjectsFromGrid() { unexpected("ObjectManager::clearGetObjectsFromGrid"); }
void ObjectManager::getObjectsFromGridNoCullConst(const Frustum& frustum, std::vector<IObject*>& out, ObjectPredicator* pred, bool b, float f1, float f2) { unexpected("ObjectManager::getObjectsFromGridNoCullConst"); }
void ObjectManager::getNoCollisionObjectsFromGrid(const Frustum& frustum, std::vector<IObject*>& out, ObjectPredicator* pred, bool b, float f1, float f2) { unexpected("ObjectManager::getNoCollisionObjectsFromGrid"); }
void ObjectManager::getObjectsFromGridWithLatency(const Vec3& from, const Vec3& direction, std::vector<IObject*>& out, ObjectPredicator* pred) { unexpected("ObjectManager::getObjectsFromGridWithLatency"); }
void ObjectManager::getObjectsFromGridWithLatency(const Vec3& centre, float radius, std::vector<IObject*>& out, ObjectPredicator* pred) { unexpected("ObjectManager::getObjectsFromGridWithLatency"); }
bool ObjectManager::intersectLine(IObject*& hitObject, Vec3& hitPoint, Vec3& hitNormal, int& hitPart, const Vec3& from, const Vec3& direction, ObjectPredicator* pred, bool b1, bool b2, bool b3, bool b4, unsigned int u) { unexpected("ObjectManager::intersectLine"); }
bool ObjectManager::intersectPill(std::vector<Vec3>& hitPoints, std::vector<Vec3>& hitNormals, const Vec3& from, const Vec3& direction, float radius, ObjectPredicator* pred, bool b) { unexpected("ObjectManager::intersectPill"); }
bool ObjectManager::intersectLineWithSpheres(IObject*& hitObject, Vec3& hitPoint, Vec3& hitNormal, int& hitPart, const Vec3& from, const Vec3& direction, ObjectPredicator* pred, ObjectPredicator* pred2, float probabilityScale) { unexpected("ObjectManager::intersectLineWithSpheres"); }
bool ObjectManager::raycast(const Vec3& from, const Vec3& direction, ObjectPredicator* pred, ObjectPredicator* pred2, unsigned int* outPart) { unexpected("ObjectManager::raycast"); }
void ObjectManager::getStaticObjectsFromGridRend(const Frustum& frustum, std::vector<IObject*>& out, LocalBoundingBox* box, ObjectPredicator* pred, unsigned int u, bool b) { unexpected("ObjectManager::getStaticObjectsFromGridRend"); }
void ObjectManager::getDynamicObjectsFromGridRend(const Frustum& frustum, std::vector<IObject*>& out, LocalBoundingBox* box, ObjectPredicator* pred, unsigned int u, bool b) { unexpected("ObjectManager::getDynamicObjectsFromGridRend"); }
void ObjectManager::getStaticObjectsFromGridRend(const Vec3& centre, float radius, std::vector<IObject*>& out, LocalBoundingBox* box, ObjectPredicator* pred, unsigned int u, bool b) { unexpected("ObjectManager::getStaticObjectsFromGridRend"); }
void ObjectManager::getDynamicObjectsFromGridRend(const Vec3& centre, float radius, std::vector<IObject*>& out, LocalBoundingBox* box, ObjectPredicator* pred, unsigned int u, bool b) { unexpected("ObjectManager::getDynamicObjectsFromGridRend"); }
void ObjectManager::addChildrenRend(const std::vector<IObject*>& in, std::vector<IObject*>& out, LocalBoundingBox* box, ObjectPredicator* pred) { unexpected("ObjectManager::addChildrenRend"); }
void ObjectManager::addChildrenRend(IObject* object, std::vector<IObject*>& out, LocalBoundingBox* box, ObjectPredicator* pred) { unexpected("ObjectManager::addChildrenRend"); }
IObject* ObjectManager::getActiveObject() const { unexpected("ObjectManager::getActiveObject"); }
void ObjectManager::setActiveObject(IObject* object) { unexpected("ObjectManager::setActiveObject"); }
void ObjectManager::destroyObjects() { unexpected("ObjectManager::destroyObjects"); }
void ObjectManager::refreshTerrainSampling() { unexpected("ObjectManager::refreshTerrainSampling"); }
void ObjectManager::refreshGridObject(IObject* object) { unexpected("ObjectManager::refreshGridObject"); }
void ObjectManager::updateObjectsInGrid() { unexpected("ObjectManager::updateObjectsInGrid"); }
std::vector<IObject*>& ObjectManager::getObjectTempVector() { unexpected("ObjectManager::getObjectTempVector"); }
void ObjectManager::getObjectsInBoxFromGrid(const Vec3& min, const Vec3& max, std::vector<IObject*>& out, ObjectPredicator* pred) { unexpected("ObjectManager::getObjectsInBoxFromGrid"); }
IObject* ObjectManager::getRegisteredObject(int id) { unexpected("ObjectManager::getRegisteredObject"); }
const std::map<unsigned int, IObject*>& ObjectManager::getAllRegisteredObjects() const { unexpected("ObjectManager::getAllRegisteredObjects"); }
void ObjectManager::setMinCullDistance(float d) { unexpected("ObjectManager::setMinCullDistance"); }
float ObjectManager::getMinCullDistance() { unexpected("ObjectManager::getMinCullDistance"); }
void ObjectManager::setDistanceCullConst(float c) { unexpected("ObjectManager::setDistanceCullConst"); }
float ObjectManager::getDistanceCullConst() { unexpected("ObjectManager::getDistanceCullConst"); }
void ObjectManager::setDistanceCullConstPCO(float c) { unexpected("ObjectManager::setDistanceCullConstPCO"); }
float ObjectManager::getDistanceCullConstPCO() { unexpected("ObjectManager::getDistanceCullConstPCO"); }
void ObjectManager::postMessage(IObject* to, TemplateMessage message, float delay) { unexpected("ObjectManager::postMessage"); }
void ObjectManager::removeAllMessagesTo(IObject* to) { unexpected("ObjectManager::removeAllMessagesTo"); }
unsigned int ObjectManager::getNewID() { unexpected("ObjectManager::getNewID"); }
void ObjectManager::registerObject(IObject* object) { unexpected("ObjectManager::registerObject"); }
void ObjectManager::unregisterObject(IObject* object) { unexpected("ObjectManager::unregisterObject"); }
void ObjectManager::deleteAllRegisteredObjects() { unexpected("ObjectManager::deleteAllRegisteredObjects"); }
void ObjectManager::addObjectToNameHash(IObject* object) { unexpected("ObjectManager::addObjectToNameHash"); }
void ObjectManager::removeObjectFromNameHash(IObject* object) { unexpected("ObjectManager::removeObjectFromNameHash"); }
void ObjectManager::addObjectToUpdate(IObject* object) { unexpected("ObjectManager::addObjectToUpdate"); }
void ObjectManager::removeObjectFromUpdate(IObject* object) { unexpected("ObjectManager::removeObjectFromUpdate"); }
void ObjectManager::addObjectToFrameUpdate(IObject* object) { unexpected("ObjectManager::addObjectToFrameUpdate"); }
void ObjectManager::removeObjectFromFrameUpdate(IObject* object) { unexpected("ObjectManager::removeObjectFromFrameUpdate"); }
void ObjectManager::addObjectToGrid(IObject* object) { unexpected("ObjectManager::addObjectToGrid"); }
void ObjectManager::updateObjectInGrid(IObject* object) { unexpected("ObjectManager::updateObjectInGrid"); }
void ObjectManager::removeObjectFromGrid(IObject* object) { unexpected("ObjectManager::removeObjectFromGrid"); }
void ObjectManager::getCameraInfo(Mat4* transform, float* f1, float* f2) { unexpected("ObjectManager::getCameraInfo"); }
void ObjectManager::setOverrideLineTestLod(int lod) { unexpected("ObjectManager::setOverrideLineTestLod"); }
int ObjectManager::getOverrideLineTestLod() { unexpected("ObjectManager::getOverrideLineTestLod"); }
void ObjectManager::addObjectToDestroyList(IObject* object) { unexpected("ObjectManager::addObjectToDestroyList"); }
ObjectGrid* ObjectManager::getObjectGrid() { unexpected("ObjectManager::getObjectGrid"); }

} } }

// Field access for the never-constructed camera and its template.
namespace {
struct CameraFields : Camera {
    static void setParent(Camera* c, IObject* parent) { static_cast<CameraFields*>(c)->m_parent = parent; }
    static void setTemplate(Camera* c, const IObjectTemplate* t) { static_cast<CameraFields*>(c)->m_template = t; }
};
struct TemplateFields : CameraTemplate {
    static void setSpeeds(CameraTemplate* t, float start, float maximum)
    {
        static_cast<TemplateFields*>(t)->m_cameraShakeStartSpeed = start;
        static_cast<TemplateFields*>(t)->m_cameraShakeMaxSpeed = maximum;
    }
};
extern "C" void* cameraVtable[] __asm__("__ZTVN4dice3hfe5world6CameraE");
const float kTwoPi = 6.2831855f;
} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    alignas(16) static unsigned char cameraStorage[sizeof(Camera)];
    alignas(16) static unsigned char templateStorage[sizeof(CameraTemplate)];
    std::memset(cameraStorage, 0, sizeof(cameraStorage));
    std::memset(templateStorage, 0, sizeof(templateStorage));
    *reinterpret_cast<void**>(cameraStorage) = &cameraVtable[2];
    Camera* cam = reinterpret_cast<Camera*>(cameraStorage);
    CameraTemplate* camTemplate = reinterpret_cast<CameraTemplate*>(templateStorage);
    CameraFields::setTemplate(cam, camTemplate);
    playerManager = new PlayerManager();
    objectManager = new ObjectManager();

    // The rig: the camera sits on a vehicle root with a physics node; the player occupies it.
    FakePlayer player;
    s_localPlayer = &player;
    FakePhysics rootPhysics;
    FakeTemplate vehicleTemplate;
    vehicleTemplate.classId = 1;
    FakePco rootPco;
    rootPco.occupant = &player;
    rootPco.occupied = true;
    FakeObject vehicle(0, &vehicleTemplate, &rootPhysics, v3(0, 0, 0), 0x1000008);
    vehicle.pco = &rootPco;
    CameraFields::setParent(cam, &vehicle);

    // ---- updateCameraRecoil ----
    cam->m_cameraRecoilX = 0.3f;
    cam->m_cameraRecoilY = 0.7f;
    cam->updateCameraRecoil(0.02f);
    check(cam->m_cameraRecoilX == 0.0f && cam->m_cameraRecoilY == 0.0f, "recoil idle: both terms zeroed");
    cam->m_updateCameraRecoil = true;
    cam->m_recoilImpulseSpeed = 12.0f;
    cam->m_recoilImpulseSize = 2.0f;
    cam->updateCameraRecoil(0.02f);
    check(near(cam->m_cameraRecoilX, 0.24f) && near(cam->m_cameraRecoilY, std::sin(kTwoPi * 0.24f) * 0.6f * 2.0f)
          && cam->m_updateCameraRecoil, "recoil: phase += dt*speed*1.0, y = sin(2pi phase) * 0.6 * size");
    for (int i = 0; i < 3; ++i) cam->updateCameraRecoil(0.02f);
    check(near(cam->m_cameraRecoilX, 0.96f) && cam->m_updateCameraRecoil, "recoil: still running below 1");
    cam->updateCameraRecoil(0.02f);
    check(!cam->m_updateCameraRecoil && cam->m_cameraRecoilX == 0.0f && cam->m_cameraRecoilY == 0.0f,
          "recoil: past one period it stops and clears");

    // ---- updateCameraShake: the speed shake ----
    rootPhysics.speed = v3(0.0f, 0.0f, 25.0f);
    cam->m_speedShakeRoll = 9.0f;
    TemplateFields::setSpeeds(camTemplate, 0.0f, 30.0f);
    cam->updateCameraShake(0.1f);
    check(cam->m_speedShakeRoll == 9.0f && cam->m_speedShakeTime == 0.0f, "speed shake: start speed 0 disables it");
    TemplateFields::setSpeeds(camTemplate, 20.0f, 30.0f);
    rootPhysics.speed = v3(0.0f, 3.0f, 9.0f);
    cam->updateCameraShake(0.1f);
    check(cam->m_speedShakeRoll == 0.0f && cam->m_speedShakeTime == 0.0f, "speed shake: below the start speed it is zero");
    rootPhysics.speed = v3(0.0f, 15.0f, 20.0f);   // 25 m/s: half way
    cam->updateCameraShake(0.1f);
    check(near(cam->m_speedShakeTime, 0.1f)
          && near(cam->m_speedShakeRoll, std::sin(std::fmod(0.1f * 40.0f, kTwoPi)) * 0.5f * 0.5f),
          "speed shake: sin(fmod(t*40, 2pi)) * scale * 0.5 at half the range");
    rootPhysics.speed = v3(0.0f, 0.0f, 45.0f);
    cam->updateCameraShake(0.1f);
    check(near(cam->m_speedShakeTime, 0.2f)
          && near(cam->m_speedShakeRoll, std::sin(std::fmod(0.2f * 40.0f, kTwoPi)) * 1.0f * 0.5f),
          "speed shake: the scale clamps to 1 above the max speed");
    rootPhysics.speed = v3(0.0f, 21.0f, 28.0f);   // 35 m/s: scale 1.5 before the clamp
    cam->updateCameraShake(0.05f);
    check(near(cam->m_speedShakeTime, 0.25f)
          && near(cam->m_speedShakeRoll, std::sin(std::fmod(0.25f * 40.0f, kTwoPi)) * 1.0f * 0.5f),
          "speed shake: a scale of 1.5 clamps to 1");
    TemplateFields::setSpeeds(camTemplate, 20.0f, 10.0f);   // no range: full scale
    cam->updateCameraShake(0.1f);
    check(near(cam->m_speedShakeRoll, std::sin(std::fmod(0.35f * 40.0f, kTwoPi)) * 0.5f), "speed shake: max <= start is full scale");
    FakeObject bare(0, &vehicleTemplate, 0, v3(0, 0, 0), 0);
    CameraFields::setParent(cam, &bare);
    cam->m_speedShakeRoll = 9.0f;
    TemplateFields::setSpeeds(camTemplate, 20.0f, 30.0f);
    cam->updateCameraShake(0.1f);
    check(cam->m_speedShakeRoll == 9.0f, "speed shake: no physics node on the root, nothing changes");
    CameraFields::setParent(cam, &vehicle);

    // ---- updateCameraShake: the explosion shake ----
    cam->m_explosionShakeTimeRemaining = 0.6f;
    cam->m_explosionShakeMagnitude = 2.0f;
    cam->updateCameraShake(0.1f);
    {
        const float progress = 1.0f - 0.5f / 0.6f;
        check(near(cam->m_explosionShakeTimeRemaining, 0.5f)
              && near(cam->m_explosionShakeRoll, std::sin(progress * kTwoPi * 7.0f) * (1.0f - progress) * 2.0f * 2.0f),
              "explosion: time -= dt, roll = sin(p*2pi*7) * (1-p) * 2 * magnitude");
    }
    cam->m_explosionShakeTimeRemaining = 0.0f;
    cam->m_explosionShakeRoll = 5.0f;
    cam->updateCameraShake(0.1f);
    check(cam->m_explosionShakeTimeRemaining == 0.0f && cam->m_explosionShakeRoll == 0.0f, "explosion: over, roll zeroed");

    // ---- updateCameraShake: the close-vehicle shakes apply on foot only ----
    cam->m_soldierShakeX = 0.5f;
    cam->m_soldierShakeY = 0.8f;
    cam->m_soldierShakeRoll = 3.0f;
    cam->updateCameraShake(0.1f);
    check(cam->m_soldierShakeRoll == 0.0f && cam->m_soldierShakePitch == 0.0f && cam->m_closeVehicleShakePhaseX == 0.0f,
          "close-vehicle shake: in a vehicle it is cleared");
    FakeTemplate soldierTemplate;
    soldierTemplate.classId = CID_SoldierTemplate;
    FakePhysics soldierPhysics;   // standing still: no speed shake
    FakeObject soldier(0, &soldierTemplate, &soldierPhysics, v3(0, 0, 0), 0);
    soldier.pco = &rootPco;
    CameraFields::setParent(cam, &soldier);
    cam->updateCameraShake(0.06f);
    check(near(cam->m_closeVehicleShakePhaseX, 0.6f) && near(cam->m_soldierShakeRoll, std::sin(kTwoPi * 0.6f) * 0.5f * 0.2f)
          && near(cam->m_soldierShakePitch, std::sin(kTwoPi * 0.6f) * 0.8f * 0.2f),
          "close-vehicle shake on foot: phase += dt*10, sin(2pi phase) * strength * 0.2");
    cam->updateCameraShake(0.06f);
    check(near(cam->m_closeVehicleShakePhaseX, 0.2f) && near(cam->m_closeVehicleShakePhaseY, 0.2f),
          "close-vehicle shake: the phase wraps at 1");

    // ---- updateCloseVehicleShake: the two heavy-engine scans ----
    FakePlayer other;
    FakePco occupiedPco, emptyPco;
    occupiedPco.occupant = &other;
    occupiedPco.occupied = true;
    FakePhysics heavy, medium, parked, light;
    heavy.mass = 20000.0f;
    medium.mass = 5000.0f;
    parked.mass = 50000.0f;
    parked.mobile = false;
    light.mass = 1500.0f;
    FakeObject tank(0, &vehicleTemplate, &heavy, v3(3.5f, 0, 0), 0x1000008);           // both scans, 0.5
    tank.pco = &occupiedPco;
    FakeObject jeep(0, &vehicleTemplate, &medium, v3(0, 1.4f, 0), 0x1000008);          // force scan only, 0.8
    jeep.pco = &occupiedPco;
    FakeObject statue(0, &vehicleTemplate, &parked, v3(0, 0, 1.0f), 0x1000008);        // not mobile
    statue.pco = &occupiedPco;
    FakeObject emptyTank(0, &vehicleTemplate, &heavy, v3(1.0f, 0, 0), 0x1000008);      // not occupied
    emptyTank.pco = &emptyPco;
    FakeObject quad(0, &vehicleTemplate, &light, v3(0.5f, 0, 0), 0x1000008);           // below 2000
    quad.pco = &occupiedPco;
    FakeObject unflagged(0, &vehicleTemplate, &heavy, v3(0.5f, 0, 0), 0x8);           // flag 0x1000000 missing
    unflagged.pco = &occupiedPco;
    FakeObject farTank(0, &vehicleTemplate, &heavy, v3(9.0f, 0, 0), 0x1000008);        // beyond 7 m: negative
    farTank.pco = &occupiedPco;
    FakeObject noPco(0, &vehicleTemplate, &heavy, v3(0.5f, 0, 0), 0x1000008);          // no player-control object
    FakePhysics ownPhysics;
    ownPhysics.mass = 50000.0f;
    FakeObject ownTank(0, &vehicleTemplate, &ownPhysics, v3(0, 0, 0), 0x1000008);      // the camera's own root
    ownTank.pco = &occupiedPco;
    s_gridObjects = { &tank, &jeep, &statue, &emptyTank, &quad, &unflagged, &farTank, &noPco, &ownTank };
    CameraFields::setParent(cam, &ownTank);
    s_cameraPosition = v3(0, 0, 0);
    cam->m_viewMode = 3;
    cam->m_soldierShakeX = cam->m_soldierShakeY = 0.4f;
    calls.clear();
    cam->updateCloseVehicleShake();
    check(cam->m_soldierShakeX == 0.0f && cam->m_soldierShakeY == 0.0f && calls.empty(),
          "close-vehicle scan: outside view modes 1/2 both are cleared and nothing is queried");
    cam->m_viewMode = 1;
    calls.clear();
    cam->updateCloseVehicleShake();
    check(near(cam->m_soldierShakeX, 0.5f), "close-vehicle scan X: the mobile, occupied, >= 10000 tank at 3.5 m");
    check(near(cam->m_soldierShakeY, 0.8f), "close-vehicle scan Y: the >= 2000 jeep at 1.4 m wins");
    check(calls.size() == 2 && calls[0] == "grid 0.0 0.0 0.0 r=7.0" && calls[1] == calls[0],
          "close-vehicle scan: two grid queries of 7 m around the camera");
    cam->m_viewMode = 2;
    tank.position = v3(6.0f, 0, 0);
    jeep.position = v3(0, 6.3f, 0);
    cam->updateCloseVehicleShake();
    check(near(cam->m_soldierShakeX, 1.0f / 7.0f) && near(cam->m_soldierShakeY, 1.0f / 7.0f),
          "close-vehicle scan: strength (7 - distance) / 7, mode 2 too");

    // ---- handleUpdate ----
    CameraFields::setParent(cam, &soldier);
    TemplateFields::setSpeeds(camTemplate, 0.0f, 0.0f);
    cam->m_frameTime = 1.0f;
    cam->m_closeVehicleCheckTime = 0.5f;
    cam->m_savedViewMode = 8;
    cam->m_updateCameraRecoil = true;
    cam->m_recoilImpulseSpeed = 12.0f;
    cam->m_recoilImpulseSize = 2.0f;
    cam->m_cameraRecoilX = 0.0f;
    // Non-zero shake terms (retail also hands them to the archived Falcon's ShakeForce block).
    cam->m_speedShakeRoll = 0.0321f;                 // kept: the start speed is 0
    cam->m_explosionShakeTimeRemaining = 0.3f;
    cam->m_explosionShakeMagnitude = 0.5f;
    cam->m_closeVehicleShakePhaseX = 0.1f;
    cam->m_closeVehicleShakePhaseY = 0.55f;
    calls.clear();
    cam->handleUpdate(0.02f, 0);
    check(near(cam->m_frameTime, 1.02f), "handleUpdate: the frame clock advances by dt");
    check(near(cam->m_cameraRecoilX, 0.24f), "handleUpdate: the local occupant's recoil runs");
    check(near(cam->m_closeVehicleCheckTime, 0.52f) && calls.size() == 1 && calls[0] == "base 0.020 0",
          "handleUpdate: below the 1 s interval the scan clock advances, then RotationalBundle");
    cam->m_closeVehicleCheckTime = 1.01f;
    calls.clear();
    cam->handleUpdate(0.02f, 2);
    check(cam->m_closeVehicleCheckTime == 0.0f && calls.size() == 3 && calls[2] == "base 0.020 2",
          "handleUpdate: past 1 / g_checkCloseVehicleFreq it scans (two queries) and resets the clock");
    rootPco.occupant = &other;   // someone else occupies it
    const float recoilBefore = cam->m_cameraRecoilX;
    calls.clear();
    cam->handleUpdate(0.02f, 0);
    check(near(cam->m_frameTime, 1.06f) && cam->m_cameraRecoilX == recoilBefore && calls.size() == 1,
          "handleUpdate: not the local player's camera: only the clock and the base update");
    rootPco.occupant = &player;
    s_networkable = 0;
    calls.clear();
    cam->handleUpdate(0.02f, 4);
    check(near(cam->m_frameTime, 1.06f) && calls.empty(), "handleUpdate flags & 4 without a networkable: nothing");
    s_networkable = reinterpret_cast<io::INetworkable*>(&player);   // any non-null pointer
    cam->handleUpdate(0.02f, 4);
    check(near(cam->m_frameTime, 1.06f) && calls.size() == 1 && calls[0] == "base 0.020 4",
          "handleUpdate flags & 4 with a networkable: straight to RotationalBundle");

    std::printf("camera shake %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
