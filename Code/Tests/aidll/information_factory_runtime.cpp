// [TEMP] InformationFactory, BFInformationCourier, InformationReal, IPIPhysicalReal,
// IPIArmamentReal, WeaponInfoReal and IPIUnit, linked whole with the real handle pools and SCurve.
// Chosen parts, each named: typed observers for the engine object and plug-ins, a deterministic
// allocation seed for IPIUnit's shipped uninitialised-cache read, and roster statics pre-set so no
// interface constructor reaches the absent ClassManager.
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/InformationFactory.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/BFInformationCourier.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/Information.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/InformationReal.h"
#include "../../BF2/AI/AIDLL/AI/Main/InformationManager.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIArmament.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIArmamentReal.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPICover.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIPhysicalReal.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIUnit.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/WeaponInfo.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/WeaponInfoReal.h"
#include "../../BF2/AI/AIDLL/AI/AIUtils/WeaponInfoManager.h"
#include "../../BF2/AI/AIDLL/AI/AIUtils/CustomCurve.h"
#include "../../BF2/AI/AIDLL/AI/AIPathFinding/AerialHeightMap/AerialHeightMap.h"
#include "../../BF2/AI/AIDLL/AI/Main/AISettingsInterfaces.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/BFTimer.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/IAIObject.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/PlanePolygon.h"
#include "../../BF2/Core/Ids.h"
#include "../../BF2/Math/Mat4.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <string>
#include <vector>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "x86 only");
static_assert(sizeof(IPICover) == 12, "recovered MSVC x86 IPICover layout");
static_assert(sizeof(IPIUnit) == 40, "the allocation seed is for the recovered x86 layout");

// [TEMP] The shipped constructor reads +0x14/+0x18 before writing them. Direct construction uses
// prefilled placement storage; factory case 5 must allocate internally, so this one-shot hook seeds
// exactly its 40-byte allocation. Float-array allocation/deallocation counts observe ownership.
static bool seedNextUnitAllocation;
static bool trackUnitArrays;
static unsigned int unitArrayAllocations;
static unsigned int unitArrayDeletions;
static void* unitArrays[32];
static std::size_t unitArraySizes[32];
static unsigned int unitArrayCount;

void* operator new(std::size_t size)
{
    void* const memory = std::malloc(size);
    if (!memory)
        throw std::bad_alloc();
    if (seedNextUnitAllocation && size == sizeof(IPIUnit))
    {
        std::memset(memory, 0, size);
        const float cachedAmmo = 0.375f;
        const double cachedTime = 100.0;
        const unsigned int unresolvedPointer = 0xA5B6C7D8u;
        std::memcpy(static_cast<unsigned char*>(memory) + 0x14, &cachedAmmo, sizeof(cachedAmmo));
        std::memcpy(static_cast<unsigned char*>(memory) + 0x18, &cachedTime, sizeof(cachedTime));
        std::memcpy(static_cast<unsigned char*>(memory) + 0x24, &unresolvedPointer, sizeof(unresolvedPointer));
        seedNextUnitAllocation = false;
    }
    return memory;
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

void* operator new[](std::size_t size)
{
    void* const memory = std::malloc(size);
    if (!memory)
        throw std::bad_alloc();
    if (trackUnitArrays)
    {
        if (unitArrayCount == sizeof(unitArrays) / sizeof(unitArrays[0]))
            std::abort();
        unitArrays[unitArrayCount] = memory;
        unitArraySizes[unitArrayCount++] = size;
        ++unitArrayAllocations;
    }
    return memory;
}
void operator delete[](void* memory) noexcept
{
    for (unsigned int i = 0; i < unitArrayCount; ++i)
    {
        if (unitArrays[i] == memory)
        {
            ++unitArrayDeletions;
            unitArrays[i] = nullptr;
            break;
        }
    }
    std::free(memory);
}
void operator delete[](void* memory, std::size_t) noexcept { operator delete[](memory); }

[[noreturn]] static void unexpected(const char* what)
{
    std::fprintf(stderr, "UNEXPECTED %s\n", what);
    std::fflush(stderr);
    std::abort();
}
static std::string calls;
static int checks;
static void trace(const std::string& event) { if (!calls.empty()) calls += ','; calls += event; }
static void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL %s\ntrace: %s\n", label, calls.c_str()); std::exit(2); }
}
static void expectTrace(const std::string& expected, const char* label)
{
    ++checks;
    if (calls != expected) {
        std::fprintf(stderr, "FAIL %s\nexpected: %s\nactual:   %s\n", label, expected.c_str(), calls.c_str());
        std::exit(2);
    }
    calls.clear();
}
static unsigned int bits(float value) { unsigned int b; std::memcpy(&b, &value, 4); return b; }
static std::string timeText(double time)
{
    unsigned long long b; std::memcpy(&b, &time, 8);
    char text[24]; std::snprintf(text, sizeof(text), "%016llx", b);
    return text;
}
static std::string num(float value) { char text[32]; std::snprintf(text, sizeof(text), "%g", value); return text; }

#include "factory_observers.inl"

namespace dice { namespace hfe {
class ClassManager;
ClassManager* classManager = nullptr;   // [TEMP] never reached: the roster statics are pre-set below
namespace ai {
// [TEMP] the roster statics these TUs read. Non-null, so the interface constructors skip the
// registration and the destructors skip the unregistration (neither is this fixture's subject).
static int rosterToken;
void* CommonSingletonServer::m_iBFTimer = nullptr;
void* CommonSingletonServer::m_iaiInformationCourier = &rosterToken;
void* CommonSingletonServer::m_iInformationFactory = &rosterToken;
void* CommonSingletonServer::m_iaiSettings = &rosterToken;

// [TEMP] IBFTimer's out-of-line lifecycle lives in the engine image.
IBFTimer::IBFTimer() {}
IBFTimer::~IBFTimer() {}

// [TEMP] IAISettings' out-of-line lifecycle lives in AISettings.cpp, outside this focused link.
IAISettings::IAISettings() {}
IAISettings::~IAISettings() {}

// [TEMP] The aerial height map: AerialHeightMap.cpp (terrain, level files) is not linked. The
// instance is a token the observer checks; the altitude is scripted per x coordinate.
static int heightMapToken;
static std::map<float, float> heights;
AerialHeightMap* AerialHeightMap::m_instance = reinterpret_cast<AerialHeightMap*>(&heightMapToken);
float AerialHeightMap::getAltitude(const Vec3& position)
{
    if (reinterpret_cast<void*>(this) != &heightMapToken) unexpected("height map instance");
    trace("height:" + num(position.x) + "/" + num(position.y) + "/" + num(position.z));
    const auto found = heights.find(position.x);
    return found == heights.end() ? 1000.0f : found->second;
}

// generated observers' chosen slots
static unsigned int outlineConstructions;
static unsigned int outlineDestructions;

class OutlineObserver : public IPlanePolygon
{
public:
    explicit OutlineObserver(const std::string& id) : m_id(id) { ++outlineConstructions; }
    ~OutlineObserver() { ++outlineDestructions; trace("~" + m_id); }

    IPlanePolygon* fromTransform(const Mat4&) const override { unexpected("IPlanePolygon::fromTransform"); }
    void setVertices(std::list<Vec2>&) override { unexpected("IPlanePolygon::setVertices"); }
    const std::list<Vec2>& getVertices() const override { unexpected("IPlanePolygon::getVertices"); }
    void getScaledVertices(std::list<Vec2>&, const Vec2&) const override
    { unexpected("IPlanePolygon::getScaledVertices"); }
    bool intersects(const Vec2&, const Vec2&) const override { unexpected("IPlanePolygon::intersects"); }
    bool isInside(const Vec2&, const Vec2&) const override { unexpected("IPlanePolygon::isInside"); }

private:
    std::string m_id;
};

EngineObject::EngineObject(const char* id)
    : m_id(id), m_side(1), m_primary(false), m_physical(nullptr), m_armament(nullptr),
      m_cover(nullptr), m_unit(nullptr)
{
    m_position.x = 100.0f; m_position.y = 50.0f; m_position.z = 200.0f;
    for (int i = 0; i < 16; ++i) m_transformation.m[i] = static_cast<float>(i);
    m_handles[0] = m_handles[1] = m_handles[2] = 0;
}
const Vec3& EngineObject::getPosition() const { trace(m_id + ":position"); return m_position; }
const Mat4& EngineObject::getTransformation() const { trace(m_id + ":transformation"); return m_transformation; }
int EngineObject::getSide() const { trace(m_id + ":side"); return m_side; }
IAIObjectPhysical* EngineObject::getPhysical() const { trace(m_id + ":physical"); return m_physical; }
IAIObjectArmament* EngineObject::getArmament() const { trace(m_id + ":armament"); return m_armament; }
IAIObjectCover* EngineObject::getCover() const { trace(m_id + ":cover"); return m_cover; }
IAIObjectUnit* EngineObject::getUnit() const { trace(m_id + ":unit"); return m_unit; }
bool EngineObject::isCover() const { trace(m_id + ":cover?"); return m_cover != nullptr; }
bool EngineObject::isPrimary() const { trace(m_id + ":isPrimary"); return m_primary; }
IAIObject* EngineObject::getFirstSecondaryObject(int& iterator)
{
    trace(m_id + ":firstSecondary"); iterator = 0;
    return m_secondaries.empty() ? nullptr : m_secondaries[0];
}
IAIObject* EngineObject::getSecondaryObject(int& iterator)
{
    trace(m_id + ":secondary"); ++iterator;
    return iterator < static_cast<int>(m_secondaries.size()) ? m_secondaries[iterator] : nullptr;
}
unsigned int EngineObject::getInformation(int side)
{
    trace(m_id + ":info" + std::to_string(side));
    if (side < 0 || side > 2) unexpected("side outside 0..2");
    return m_handles[side];
}

EngineCover::EngineCover(IAIObject* owner, const char* id)
    : IAIObjectCover(owner), m_id(id), m_lastOutline(nullptr), m_created(0)
{
}

IPlanePolygon* EngineCover::getObjectOutline()
{
    trace(m_id + ":outline");
    ++m_created;
    m_lastOutline = new OutlineObserver(m_id + std::to_string(m_created));
    return m_lastOutline;
}

static int boxToken, localBoxToken;
float EnginePhysical::getHealth() { trace(m_id + ":health"); return m_health; }
float EnginePhysical::getMaxHealth() const { trace(m_id + ":maxHealth"); return m_maxHealth; }
float EnginePhysical::getMass() const { trace(m_id + ":mass"); return 1500.0f; }
float EnginePhysical::getVolume() const { trace(m_id + ":volume"); return 12.5f; }
static const Vec3* armourDirection;
float EnginePhysical::getArmour(const Vec3& direction) const
{
    trace(m_id + ":armour"); check(&direction == armourDirection, "the direction is passed by reference"); return 0.75f;
}
float EnginePhysical::getBestArmour() const { trace(m_id + ":bestArmour"); return 0.9f; }
const LocalBoundingBox* EnginePhysical::getLocalBoundingBox() const
{
    trace(m_id + ":localBox"); return reinterpret_cast<const LocalBoundingBox*>(&localBoxToken);
}
BoundingBox* EnginePhysical::getCurrentBoundingBox() { trace(m_id + ":currentBox"); return reinterpret_cast<BoundingBox*>(&boxToken); }
float EnginePhysical::getRadius() const { trace(m_id + ":radius"); return m_id == "ps" ? 7.0f : 9.0f; }
Vec3 EnginePhysical::getSphereLocalOffset() const
{
    trace(m_id + ":sphereOffset"); Vec3 v; v.x = 1.5f; v.y = -2.5f; v.z = 3.5f; return v;
}
float EnginePhysical::getSmallestRadius() const { trace(m_id + ":smallestRadius"); return 0.5f; }
float EnginePhysical::getSubSpheres2d(std::vector<Vec2>& out) const
{
    trace(m_id + ":subSpheres"); Vec2 v; v.x = 4.0f; v.y = 5.0f; out.push_back(v); return 77.0f;
}
int EnginePhysical::getStrType() const { trace(m_id + ":strType"); return 3; }
static Vec3* terrainProbe;
bool EnginePhysical::isTouchingTerrain(Vec3& out) const
{
    trace(m_id + ":touching"); check(&out == terrainProbe, "the probe is passed by reference"); return true;
}
bool EnginePhysical::isInAir() const { trace(m_id + ":inAir"); return false; }
bool EnginePhysical::isInWater() const { trace(m_id + ":inWater"); return true; }
int EnginePhysical::getMaterial() const { trace(m_id + ":material"); return 42; }
float EnginePhysical::getManDownTimeLeft() const { trace(m_id + ":manDownLeft"); return 6.0f; }
float EnginePhysical::getMaxManDownTime() const { trace(m_id + ":manDownMax"); return 15.0f; }
bool EnginePhysical::getManDown() const { trace(m_id + ":manDown"); return true; }

double TimerObserver::getGameTime() const { trace("timer:now"); return m_now; }

WeaponObserver::WeaponObserver(const char* id)
    : m_id(id), m_name(std::string(id) + ".name"), m_lockedTarget(nullptr),
      m_refillResult(false), m_targetResult(true), m_lastRefill(0.0f), m_setFiringTime(0.0),
      m_targetId(0), m_ammo(41)
{
    m_position.x = 1.0f; m_position.y = 2.0f; m_position.z = 3.0f;
    for (int i = 0; i < 16; ++i)
    {
        m_absolute.m[i] = float(i + 10);
        m_projectileOrigin.m[i] = float(i + 30);
    }
    m_strengths[0] = 71; m_strengths[1] = 73; m_strengths[2] = 79;
}
const Mat4& WeaponObserver::getAbsoluteTransformation() const { trace(m_id + ":absolute"); return m_absolute; }
const Vec3& WeaponObserver::getPosition() const { trace(m_id + ":position"); return m_position; }
const Mat4& WeaponObserver::getProjectileOrigin() { trace(m_id + ":origin"); return m_projectileOrigin; }
int WeaponObserver::getWeaponActivate() const { trace(m_id + ":activate"); return 17; }
int WeaponObserver::getWeaponFire() const { trace(m_id + ":fire"); return 19; }
int WeaponObserver::getWeaponZoom() const { trace(m_id + ":zoom"); return 23; }
int WeaponObserver::getWeaponAltFire() const { trace(m_id + ":altfire"); return 29; }
bool WeaponObserver::isZoomed() const { trace(m_id + ":zoomed"); return true; }
int WeaponObserver::getWeaponState() const { trace(m_id + ":state"); return 31; }
float WeaponObserver::getDrag() const { trace(m_id + ":drag"); return 1.25f; }
float WeaponObserver::getGravityModifier() const { trace(m_id + ":gravity"); return 2.25f; }
float WeaponObserver::getExitVelocity() const { trace(m_id + ":velocity"); return 3.25f; }
float WeaponObserver::getMinRange() const { trace(m_id + ":minRange"); return 4.25f; }
float WeaponObserver::getMaxRange() const { trace(m_id + ":maxRange"); return 5.25f; }
float WeaponObserver::getOptimalRangePercentage() const { trace(m_id + ":optimal"); return 6.25f; }
SoldierPose WeaponObserver::getFiringPose() const { trace(m_id + ":pose"); return SPCrouching; }
float WeaponObserver::getAllowedDeviation() const { trace(m_id + ":deviation"); return 7.25f; }
bool WeaponObserver::getFiresThroughTransparent() const { trace(m_id + ":transparent"); return true; }
const int* WeaponObserver::getStrengths() const { trace(m_id + ":strengths"); return m_strengths; }
int WeaponObserver::getStrength(int index) const
{ trace(m_id + ":strength" + std::to_string(index)); return m_strengths[index]; }
float WeaponObserver::getExplosionRadius() const { trace(m_id + ":explosion"); return 8.25f; }
bool WeaponObserver::getIsThrown() const { trace(m_id + ":thrown"); return true; }
bool WeaponObserver::useAimerOnly() const { trace(m_id + ":aimer"); return false; }
int WeaponObserver::getFireRate() const { trace(m_id + ":rate"); return 37; }
int WeaponObserver::getAmmo() const { trace(m_id + ":ammo"); return m_ammo; }
int WeaponObserver::getCurrentAmmo() const { trace(m_id + ":currentAmmo"); return 43; }
int WeaponObserver::getAmmoInMag(int mag) const
{ trace(m_id + ":mag" + std::to_string(mag)); return 100 + mag; }
int WeaponObserver::getMaxAmmo() const { trace(m_id + ":maxAmmo"); return 47; }
int WeaponObserver::getMaxAmmoInMag() const { trace(m_id + ":maxMag"); return 53; }
int WeaponObserver::getNumberOfMags() const { trace(m_id + ":mags"); return 59; }
bool WeaponObserver::hasAutoReload() const { trace(m_id + ":autoReload"); return true; }
float WeaponObserver::getReloadTime() const { trace(m_id + ":reload"); return 9.25f; }
bool WeaponObserver::refillAmmo(float fraction)
{ trace(m_id + ":refill" + num(fraction)); m_lastRefill = fraction; return m_refillResult; }
int WeaponObserver::getWeaponType() const { trace(m_id + ":type"); return 61; }
int WeaponObserver::getWeaponGuidingSystem() const { trace(m_id + ":guidance"); return 67; }
void WeaponObserver::setTimeOfFiring(double time)
{ trace(m_id + ":setFiring" + timeText(time)); m_setFiringTime = time; }
double WeaponObserver::getTimeOfFiring() const { trace(m_id + ":firing"); return 11.5; }
bool WeaponObserver::getBurst() const { trace(m_id + ":burst"); return true; }
bool WeaponObserver::getIndirect() const { trace(m_id + ":indirect"); return false; }
float WeaponObserver::getHeat() const { trace(m_id + ":heat"); return 12.25f; }
bool WeaponObserver::hasHeat() const { trace(m_id + ":hasHeat"); return true; }
bool WeaponObserver::usesHeat() const { trace(m_id + ":usesHeat"); return false; }
bool WeaponObserver::isReadyToFire() const { trace(m_id + ":ready"); return true; }
bool WeaponObserver::setFakeFire(bool fake)
{ trace(m_id + (fake ? ":fake1" : ":fake0")); return false; }
bool WeaponObserver::getFakeFire() const { trace(m_id + ":getFake"); return true; }
float WeaponObserver::getSoundSphereRadius() const { trace(m_id + ":sound"); return 13.25f; }
int WeaponObserver::getProjectileMaterial() const { trace(m_id + ":material"); return 71; }
float WeaponObserver::getReplenishRadius() const { trace(m_id + ":replenish"); return 14.25f; }
void WeaponObserver::setBotSkill(float skill, double since, float scale)
{ trace(m_id + ":skill" + num(skill) + "/" + timeText(since) + "/" + num(scale)); }
world::IObject* WeaponObserver::getProjectile() const
{ trace(m_id + ":projectile"); return reinterpret_cast<world::IObject*>(0x1234); }
bool WeaponObserver::setTarget(unsigned int targetId)
{ trace(m_id + ":target" + std::to_string(targetId)); m_targetId = targetId; return m_targetResult; }
bool WeaponObserver::hasTargetLocked() const { trace(m_id + ":locked?"); return true; }
IAIObject* WeaponObserver::getLockedTarget() const { trace(m_id + ":locked"); return m_lockedTarget; }
const std::string& WeaponObserver::getName() const { trace(m_id + ":name"); return m_name; }

static std::vector<unsigned int> handlesExpectedReleased;

EngineArmament::EngineArmament(IAIObject* owner)
    : IAIObjectArmament(owner), m_lastFired(nullptr), m_rearmResult(false),
      m_lastRearmAmount(0.0f), m_lastRearmWeapon(nullptr), m_lastRearmSide(0)
{
}
int EngineArmament::isAntiAircraft() const { trace("arm:aa"); return 83; }
int EngineArmament::getActiveWeapon() const { trace("arm:active"); return 3; }
void EngineArmament::setActiveWeapon(unsigned int index, int side)
{ trace("arm:setActive" + std::to_string(index) + "/" + std::to_string(side)); }
IWeapon* EngineArmament::getWeapon(unsigned int index) const
{ trace("arm:weapon" + std::to_string(index)); return m_weapons[index]; }
void EngineArmament::setLastFiredWeapon() { trace("arm:setLastFired"); }
IWeapon* EngineArmament::getLastFiredWeapon() const { trace("arm:lastFired"); return m_lastFired; }
const std::vector<IWeapon*>& EngineArmament::getWeapons() const
{
    for (unsigned int oldHandle : handlesExpectedReleased)
        check(!WeaponInfoManager::m_instance->getInformation(oldHandle),
              "all old weapon handles are released before the engine vector is obtained");
    trace("arm:weapons"); return m_weapons;
}
int EngineArmament::getWeaponIdFromActivationId(int activation, int fire) const
{ trace("arm:map" + std::to_string(activation) + "/" + std::to_string(fire)); return 1; }
bool EngineArmament::rearm(IWeapon* weapon, float amount, int side)
{
    trace("arm:rearm" + num(amount) + "/" + std::to_string(side));
    m_lastRearmWeapon = weapon; m_lastRearmAmount = amount; m_lastRearmSide = side;
    return m_rearmResult;
}
bool EngineArmament::getProjectilePosition(Vec3& out) const
{ trace("arm:projectilePos"); out.x = 101; out.y = 102; out.z = 103; return true; }
bool EngineArmament::getProjectileSpeed(Vec3& out) const
{ trace("arm:projectileSpeed"); out.x = 201; out.y = 202; out.z = 203; return false; }

EngineUnit::EngineUnit(IAIObject* owner)
    : IAIObjectUnit(owner), m_id("unit"), m_maxAmmo(82), m_occupier(nullptr),
      m_occupiedObject(nullptr), m_occupied(true), m_teamLock(2), m_missileLock(29),
      m_borrow(true), m_possessionTime(12.5)
{
    const int battle[] = {10, 20, 30, 40, 50, 60};
    const int strategic[] = {7, 11};
    std::memcpy(m_battle, battle, sizeof(m_battle));
    std::memcpy(m_strategic, strategic, sizeof(m_strategic));
    m_entryPosition.x = 9.0f; m_entryPosition.y = 8.0f; m_entryPosition.z = 7.0f;
}
int EngineUnit::getAmmo() const { trace("unit:ammo"); return 43; }
int EngineUnit::getMaxAmmo() const { trace("unit:maxAmmo"); return m_maxAmmo; }
const int* EngineUnit::getBattleStrengths() const { trace("unit:battle"); return m_battle; }
const int* EngineUnit::getStrategicStrengths() const { trace("unit:strategic"); return m_strategic; }
int EngineUnit::getBattleStrength(int index) const
{ trace("unit:battle" + std::to_string(index)); return m_battle[index]; }
int EngineUnit::getStrategicStrength(int index) const
{ trace("unit:strategic" + std::to_string(index)); return m_strategic[index]; }
void EngineUnit::setBattleStrength(int index, int value)
{ trace("unit:setBattle" + std::to_string(index) + "/" + std::to_string(value)); m_battle[index] = value; }
void EngineUnit::setStrategicStrength(int index, int value)
{ trace("unit:setStrategic" + std::to_string(index) + "/" + std::to_string(value)); m_strategic[index] = value; }
int EngineUnit::getEquipmentType() const { trace("unit:equipment"); return 91; }
bool EngineUnit::useNoPathfindingToGetToObject() const { trace("unit:noPath"); return true; }
bool EngineUnit::hasExposedSoldier() const { trace("unit:exposed"); return false; }
bool EngineUnit::getAlwaysTargetSoldier() const { trace("unit:alwaysTarget"); return true; }
bool EngineUnit::isOccupied() const { trace("unit:occupied?"); return m_occupied; }
float EngineUnit::getCullDistance() const { trace("unit:cull"); return 44.5f; }
float EngineUnit::getCullDistanceSqr() const { trace("unit:cullSq"); return 1980.25f; }
float EngineUnit::getEntryRadius() const { trace("unit:entryRadius"); return 6.25f; }
IAIObject* EngineUnit::getOccupier() const { trace("unit:occupier"); return m_occupier; }
IAIObject* EngineUnit::getOccupiedObject() const { trace("unit:occupiedObject"); return m_occupiedObject; }
double EngineUnit::getLastPlayerPossesionTime() const { trace("unit:possession"); return m_possessionTime; }
void EngineUnit::setLastPlayerPossesionTime(double time)
{ trace("unit:setPossession" + timeText(time)); m_possessionTime = time; }
int EngineUnit::getTeamLock() const { trace("unit:teamLock"); return m_teamLock; }
bool EngineUnit::mustBorrowArmament(IAIObject* vehicle) const
{
    trace("unit:borrow");
    if (vehicle != m_object) unexpected("IAIObjectUnit::mustBorrowArmament owner");
    return m_borrow;
}
int EngineUnit::getOccupyingPlayerId() const { trace("unit:player"); return 1234; }
io::PlayerInputMap EngineUnit::getSelectKey() const { trace("unit:select"); return io::PIPositionSelect3; }
void EngineUnit::setMissileLockStatus(int status)
{ trace("unit:setMissile" + std::to_string(status)); m_missileLock = status; }
int EngineUnit::getMissileLockStatus() const { trace("unit:missile"); return m_missileLock; }
const Vec3& EngineUnit::getEntryPositionOfNonMobile() const { trace("unit:entry"); return m_entryPosition; }

int SettingsObserver::getNBattleStrengths() { trace("settings:battle"); return m_battleCount; }
int SettingsObserver::getNStrategicStrengths() { trace("settings:strategic"); return m_strategicCount; }

TracedInformation::~TracedInformation() { trace("~" + m_id); }
} } }

// hand-written observers
class WeaponRecord : public WeaponInfoReal
{
public:
    explicit WeaponRecord(const char* id) : WeaponInfoReal(nullptr), m_id(id) {}
    ~WeaponRecord() { trace("~" + m_id); }
    std::string m_id;
};
class ArmamentPlug : public IPIArmament
{
public:
    ArmamentPlug() : IPIArmament(nullptr) {}
    ~ArmamentPlug() { trace("~armament"); }
    std::vector<unsigned int> m_unitWeapons;
    int getType() override { unexpected("IPIArmament::getType"); }
    void setTime() override { unexpected("IPIArmament::setTime()"); }
    void setTime(double) override { unexpected("IPIArmament::setTime(double)"); }
    int getActiveWeapon() const override { unexpected("IPIArmament::getActiveWeapon"); }
    void setActiveWeapon(int, int) override { unexpected("IPIArmament::setActiveWeapon"); }
    unsigned int getWeapon(int) override { unexpected("IPIArmament::getWeapon"); }
    std::vector<unsigned int>& getWeapons() override { trace("unitArm:weapons"); return m_unitWeapons; }
    int getWeaponIdFromActivationId(int, int) const override { unexpected("IPIArmament::getWeaponIdFromActivationId"); }
    WeaponInfo* getLastFiredWeapon() override { unexpected("IPIArmament::getLastFiredWeapon"); }
    int isAntiAircraft() const override { unexpected("IPIArmament::isAntiAircraft"); }
    void updateWeapons() override { unexpected("IPIArmament::updateWeapons"); }
    void rearm(WeaponInfo*, float, int) override { unexpected("IPIArmament::rearm"); }
    bool getProjectilePosition(Vec3&) const override { unexpected("IPIArmament::getProjectilePosition"); }
    bool getProjectileSpeed(Vec3&) const override { unexpected("IPIArmament::getProjectileSpeed"); }
};

class WeaponInfoProbe : public WeaponInfoReal
{
public:
    explicit WeaponInfoProbe(IWeapon* weapon) : WeaponInfoReal(weapon) {}
    IWeapon* rawWeapon() const { return m_weapon; }
    float rearmAmount() const { return m_rearmAmount; }
};

class FactoryObserver : public InformationFactory
{
public:
    unsigned int requestWeaponInfo(unsigned int handle, IWeapon* weapon, bool real) const override
    {
        for (unsigned int oldHandle : handlesExpectedReleased)
            check(!WeaponInfoManager::m_instance->getInformation(oldHandle),
                  "all old weapon handles are released before rebuilding begins");
        trace(std::string("factory:weapon/") + (real ? "real" : "known"));
        return InformationFactory::requestWeaponInfo(handle, weapon, real);
    }
};

class CoverRecord : public InformationReal
{
public:
    explicit CoverRecord(IAIObject* object) : InformationReal(object, 0u, 0.0f, 0.0, 0.0f) {}
    IPICover* cover() const { return m_cover; }
};

// environment
static TimerObserver timer;
static SettingsObserver settings;
struct Environment
{
    Environment()
    {
        informationManager = new InformationManager;
        new WeaponInfoManager;                           // first wins: m_instance
        CommonSingletonServer::m_iBFTimer = static_cast<IBFTimer*>(&timer);
        CommonSingletonServer::m_iaiSettings = static_cast<IAISettings*>(&settings);
    }
};
static InformationManager::Pool* pool() { return informationManager->getPool(); }
static Information* record(unsigned int handle) { return informationManager->getInformation(handle); }
static unsigned int releasedHandle;
static const double kTime = 7.0000000000000018;

static void resetUnitArrayTracking()
{
    trackUnitArrays = true;
    unitArrayAllocations = 0;
    unitArrayDeletions = 0;
    unitArrayCount = 0;
    std::memset(unitArrays, 0, sizeof(unitArrays));
    std::memset(unitArraySizes, 0, sizeof(unitArraySizes));
}

static void seedUnitStorage(void* memory)
{
    std::memset(memory, 0, sizeof(IPIUnit));
    const float cachedAmmo = 0.375f;
    const double cachedTime = 100.0;
    const unsigned int unresolvedPointer = 0xA5B6C7D8u;
    std::memcpy(static_cast<unsigned char*>(memory) + 0x14, &cachedAmmo, sizeof(cachedAmmo));
    std::memcpy(static_cast<unsigned char*>(memory) + 0x18, &cachedTime, sizeof(cachedTime));
    std::memcpy(static_cast<unsigned char*>(memory) + 0x24, &unresolvedPointer, sizeof(unresolvedPointer));
}

static void primeFactoryUnitAllocation()
{
    seedNextUnitAllocation = true;
}

// courier
static void courierPool()
{
    Environment env; BFInformationCourier courier; EngineObject object("o");
    TracedInformation* base = new TracedInformation(nullptr, "a");
    const unsigned int handle = courier.registrateInformation(base);
    check((handle & 0xFFFFu) == 1u, "the first handle is index 1");
    check(courier.getInformation(handle) == base, "getInformation resolves");
    check(!courier.getInformation(0) && !courier.getInformation(handle ^ 0x10000u) && !courier.getInformation(0xFFFFFFFFu),
          "zero, stale and -1 resolve to null");
    check(courier.isRealInformation(handle), "a base-sort (3) record answers TRUE");
    InformationReal* real = new InformationReal(&object, 0x80u, 2.5f, 100.0, 0.25f);
    const unsigned int realHandle = courier.registrateInformation(real);
    check(!courier.isRealInformation(realHandle), "an InformationReal (sort 0) answers FALSE - the name reads backwards");
    check(!courier.isRealInformation(0) && !courier.isRealInformation(handle ^ 0x10000u), "unresolved answers false");
    expectTrace("", "the pool calls nothing");
}

static void courierFree()
{
    Environment env; BFInformationCourier courier;
    TracedInformation* doomed = new TracedInformation(nullptr, "a");
    releasedHandle = courier.registrateInformation(doomed);
    struct Probe : TracedInformation {
        Probe() : TracedInformation(nullptr, "probe") {}
    };
    courier.freeInformation(releasedHandle);
    expectTrace("~a", "the record is destroyed");
    check(!courier.getInformation(releasedHandle), "the handle is stale afterwards");
    courier.freeInformation(releasedHandle);
    courier.freeInformation(0);
    expectTrace("", "a stale or zero handle destroys nothing");
    // The slot is released before the record is destroyed: seen from inside the destructor.
    struct Watch : TracedInformation {
        Watch() : TracedInformation(nullptr, "w") {}
        ~Watch() { check(!informationManager->getInformation(releasedHandle), "released before destroyed"); }
    };
    Watch* watch = new Watch;
    releasedHandle = courier.registrateInformation(watch);
    courier.freeInformation(releasedHandle);
    expectTrace("~w", "the watched record is destroyed after its release");
}

static void courierSet()
{
    Environment env; BFInformationCourier courier;
    const unsigned int handle = courier.registrateInformation(new TracedInformation(nullptr, "a"));
    TracedInformation* b = new TracedInformation(nullptr, "b");
    courier.setInformation(handle, b);
    expectTrace("~a", "the old record is destroyed first");
    check(courier.getInformation(handle) == b, "then the new one stored");
    TracedInformation* c = new TracedInformation(nullptr, "c");
    courier.setInformation(handle ^ 0x10000u, c);
    expectTrace("", "a stale handle destroys nothing");
    check(courier.getInformation(handle) == b, "and stores nothing - the new record belongs to nobody");
    delete c;
    expectTrace("~c", "the orphan was still alive");
    courier.setInformation(handle, nullptr);
    expectTrace("~b", "null replaces and destroys");
    check(!courier.getInformation(handle) && (handle & 0xFFFFu) == 1u, "the slot holds null, still valid");
}

static void courierArmament()
{
    Environment env; BFInformationCourier courier;
    TracedInformation* lender = new TracedInformation(nullptr, "L");
    TracedInformation* borrower = new TracedInformation(nullptr, "B");
    const unsigned int hl = courier.registrateInformation(lender);
    const unsigned int hb = courier.registrateInformation(borrower);
    ArmamentPlug* armament = new ArmamentPlug;
    check(lender->setArmament(armament), "the lender is armed");
    courier.borrowArmament(hl, 0);
    courier.borrowArmament(0, hb);
    check(!borrower->armament(), "both handles must resolve");
    courier.borrowArmament(hl, hb);
    check(borrower->armament() == armament && lender->armament() == armament, "the SECOND handle borrows the FIRST's");
    courier.resetArmament(hb);
    check(!borrower->armament() && lender->armament() == armament, "reset clears only the borrower");
    courier.resetArmament(0);
    expectTrace("", "sharing and resetting destroy nothing");
}

static void courierWeapons()
{
    Environment env; BFInformationCourier courier;
    WeaponRecord* first = new WeaponRecord("w1");
    const unsigned int handle = courier.registrateWeaponInfo(first);
    check((handle & 0xFFFFu) == 1u && courier.getWeaponInfo(handle) == first, "registered and resolved");
    WeaponRecord* second = new WeaponRecord("w2");
    courier.setWeaponInfo(handle, second);
    expectTrace("~w1", "set destroys the old record through WeaponInfo's first slot");
    check(courier.getWeaponInfo(handle) == second, "then stores the new one");
    courier.freeWeaponInfo(handle);
    expectTrace("~w2", "free destroys the record");
    check(!courier.getWeaponInfo(handle), "and the handle is stale");
    courier.freeWeaponInfo(handle);
    courier.setWeaponInfo(handle, nullptr);
    expectTrace("", "stale handles destroy nothing");
}

static void courierPolygon()
{
    Environment env; BFInformationCourier courier; EngineObject object("o");
    courier.saveCurrentPolygon(nullptr);
    expectTrace("", "a null object forgets without asking anything");
    object.m_side = 0;
    courier.saveCurrentPolygon(&object);
    expectTrace("o:side,o:info1", "side 0 is read as side 1; no record forgets");
    object.m_side = 2;
    object.m_handles[2] = courier.registrateInformation(new TracedInformation(&object, "t"));
    courier.saveCurrentPolygon(&object);
    expectTrace("o:side,o:info2", "a record without a mobile plug-in forgets");
}

// factory
static void factoryIface()
{
    Environment env;
    void* raw = operator new(sizeof(InformationFactory));
    std::memset(raw, 0, sizeof(InformationFactory));   // the constructor never writes the count
    InformationFactory* factory = new (raw) InformationFactory;
    IInformationFactory* face = factory;
    check(factory->getRef() == 0, "the count is whatever the allocation held (zeroed here)");
    factory->addRef(); factory->addRef();
    check(factory->getRef() == 2, "addRef increments");
    check(factory->release() == 1 && factory->getRef() == 1, "release returns the decremented count");
    check(face->queryInterface(IID_IBase) == factory && face->queryInterface(IID_IInformationFactory) == factory,
          "IBase and IInformationFactory answer this");
    check(!face->queryInterface(0) && !face->queryInterface(IID_IInformationFactory + 1), "other ids answer null");
    check(IID_IInformationFactory == 9990007u, "the libaidll IID");
    check(factory->release() == 0, "release to zero returns 0 - and deletes");
    expectTrace("", "the reference count calls nothing");
}

static void factoryRegister()
{
    Environment env; InformationFactory factory;
    TracedInformation* a = new TracedInformation(nullptr, "a");
    const unsigned int handle = factory.registerInformation(0xFFFFFFFFu, a);
    check((handle & 0xFFFFu) != 0 && record(handle) == a, "-1 acquires a new handle");
    TracedInformation* b = new TracedInformation(nullptr, "b");
    check(factory.registerInformation(handle, b) == handle && record(handle) == b, "a valid handle is overwritten in place");
    expectTrace("", "WITHOUT destroying the record it held");
    delete a; expectTrace("~a", "the displaced record was still alive");
    TracedInformation* c = new TracedInformation(nullptr, "c");
    check(factory.registerInformation(handle ^ 0x10000u, c) == (handle ^ 0x10000u) && record(handle) == b,
          "a stale handle is returned unchanged and nothing is stored");
    check(factory.registerInformation(0, c) == 0, "handle 0 likewise");
    delete c; expectTrace("~c", "the unstored record was the caller's");
    check(factory.checkIfInfoExists(handle) && !factory.checkIfInfoExists(0) && !factory.checkIfInfoExists(handle ^ 0x10000u),
          "checkIfInfoExists");
    // A full pool: the registration answers 0 and the record belongs to nobody.
    static int filler;
    unsigned int taken = 0;
    while (pool()->acquire(reinterpret_cast<Information*>(&filler))) ++taken;
    check(taken == 65535u - 1u, "the pool held 65535 slots");
    TracedInformation* d = new TracedInformation(nullptr, "d");
    check(factory.registerInformation(0xFFFFFFFFu, d) == 0, "a full pool answers 0");
    delete d; expectTrace("~d", "the refused record was not stored");
    // The weapon twins.
    WeaponRecord* w = new WeaponRecord("w");
    const unsigned int weapon = factory.registerWeapon(0xFFFFFFFFu, w);
    check((weapon & 0xFFFFu) == 1u && WeaponInfoManager::m_instance->getInformation(weapon) == w, "-1 registers a weapon");
    check(factory.checkIfWeaponInfoExists(weapon) && !factory.checkIfWeaponInfoExists(0), "checkIfWeaponInfoExists");
    WeaponRecord* w2 = new WeaponRecord("w2");
    check(factory.registerWeapon(weapon, w2) == weapon && WeaponInfoManager::m_instance->getInformation(weapon) == w2,
          "a valid weapon handle is overwritten");
    expectTrace("", "without destroying the old weapon record");
    delete w; expectTrace("~w", "the displaced weapon record was still alive");
}

static void factoryReal()
{
    Environment env; InformationFactory factory; EngineObject object("o");
    const unsigned int handle = factory.requestInformationReal(0xFFFFFFFFu, &object, 0x80u, 2.5f, 100.0, 0.25f);
    Information* info = record(handle);
    check(info && info->getInformationSort() == 0, "a real record (sort 0) is registered");
    check(info->getObjectTypeFlags() == 0x80u && bits(info->getStrength()) == bits(2.5f), "types and basic temperature");
    check(info->getAIObject() == &object && !info->getPhysical() && !info->getUnit() && !info->hasMobile(), "owner, no plug-ins");
    check(!info->isBooked() && info->getBookingPlayerId() == -1, "not booked");
    const unsigned int again = factory.requestInformationReal(handle, &object, 1u, 1.0f, 5.0, 0.5f);
    check(again == handle && record(handle) != info && record(handle)->getObjectTypeFlags() == 1u,
          "an existing handle is overwritten (the old record is left alive)");
    expectTrace("", "building a record asks the object nothing");
}

static void factoryPlugins()
{
    Environment env; InformationFactory factory; EngineObject object("o"); EnginePhysical physical("p");
    object.m_physical = &physical;
    const unsigned int handle = factory.requestInformationReal(0xFFFFFFFFu, &object, 0u, 0.0f, 0.0, 0.0f);
    Information* info = record(handle);
    check(factory.requestInformationPlugin(handle, 4, true) && info->getPhysical(), "a real physical half");
    IPIPhysical* first = info->getPhysical();
    check(first->getType() == 0, "IPIPhysical::getType answers 0");
    check(factory.requestInformationPlugin(handle, 4, true) && info->getPhysical() == first, "a second is refused, still true");
    check(factory.requestInformationPlugin(handle, 1, true) && factory.requestInformationPlugin(handle, 1, false),
          "control info, real or known");
    check(info->removePlugIn(5) && !info->removePlugIn(5), "exactly one control-info half installed (type 5)");
    check(!factory.requestInformationPlugin(handle, 6, true) && !factory.requestInformationPlugin(handle, -1, false),
          "unknown types answer false");
    expectTrace("", "building plug-ins asks the object nothing");
}

static void factorySource()
{
    Environment env; InformationFactory factory;
    EngineObject source("s"), target("t"); EnginePhysical sourcePhysical("ps"), targetPhysical("pt");
    source.m_physical = &sourcePhysical; target.m_physical = &targetPhysical;
    const unsigned int hs = factory.requestInformationReal(0xFFFFFFFFu, &source, 0u, 0.0f, 0.0, 0.0f);
    const unsigned int ht = factory.requestInformationReal(0xFFFFFFFFu, &target, 0u, 0.0f, 0.0, 0.0f);
    check(factory.requestInformationPlugin(hs, ht, 4, true), "source / target form");
    check(record(ht)->getPhysical() && !record(hs)->getPhysical(), "the TARGET record receives the half");
    check(record(ht)->getPhysical()->getRadius() == 7.0f, "which reads the SOURCE record's object");
    expectTrace("s:physical,ps:radius", "the source's engine plug-in answers");
}

static void factoryUpdate()
{
    Environment env; InformationFactory factory; EngineObject object("o");
    const unsigned int handle = factory.requestInformationReal(0xFFFFFFFFu, &object, 0u, 0.0f, 0.0, 0.0f);
    check(!factory.updateInformationPlugin(handle, 1) && !factory.updateInformationPlugin(handle, 5), "only type 0 updates");
    check(!factory.updateInformationPlugin(0, 0) && !factory.updateInformationPlugin(handle ^ 0x10000u, 0), "a stale handle answers false");
    expectTrace("", "the refusals ask nothing");
}

// InformationReal
static void realGetters()
{
    Environment env; EngineObject object("o");
    InformationReal real(&object, 0u, 0.0f, 0.0, 0.0f);
    timer.m_now = 12345.678;
    check(real.getTimeStamp() == 12345.678, "the time stamp is always now"); expectTrace("timer:now", "the timer is asked");
    check(real.getSecurity() == 1.0f && real.getSecurity(3.0) == 1.0f && real.hasContact(), "full security, always in contact");
    real.addContact(); real.removeContact(); real.removeAllContacts();
    expectTrace("", "contacts are empty bodies");
    check(&real.getPosition() == &object.m_position, "the object's own position"); expectTrace("o:position", "position");
    check(&real.getTransformation() == &object.m_transformation, "the object's own transformation");
    expectTrace("o:transformation", "transformation");
    const Vec3 facing = real.getFaceing();
    check(facing.x == 8.0f && facing.y == 9.0f && facing.z == 10.0f, "facing = m[8..10]");
    expectTrace("o:transformation", "facing reads the transformation");
    object.m_side = 2;
    check(real.getSide() == 2, "side"); expectTrace("o:side", "side is the object's");
    heights[100.0f] = 33.0f;
    check(real.getAltitude() == 33.0f, "altitude at the object");
    expectTrace("o:position,height:100/50/200", "the height map at the position");
    check(real.setTime(kTime, 99.0f), "setTime(double, float) answers true");
    expectTrace("o:isPrimary", "a non-primary without unit only asks isPrimary");
}

static std::string heightTrace(std::initializer_list<int> xs)
{
    std::string text = "o:position";
    for (int x : xs) text += ",height:" + std::to_string(x) + "/50/200";
    return text;
}

static void realAltitude()
{
    Environment env; EngineObject object("o");
    InformationReal real(&object, 0u, 0.0f, 0.0, 0.0f);
    Vec3 direction; direction.x = 10.0f; direction.y = 0.0f; direction.z = 0.0f;
    const float table[] = { 40, -999, 35, 35, 50, 60, 70, 12, 30, 12, 20 };   // x = 100 .. 110
    for (int i = 0; i <= 10; ++i) heights[100.0f + i] = table[i];
    check(real.getAltitude(direction) == 12.0f, "the lowest height along the direction");
    expectTrace(heightTrace({100, 102, 103, 104, 105, 106, 107, 108, 109, 110}),
                "start, then tenths 2..10 - the FIRST step is never sampled");
    Vec3 zero; zero.x = zero.y = zero.z = 0.0f;
    check(real.getAltitude(zero) == 40.0f, "a zero direction samples only the start");
    expectTrace(heightTrace({100}), "zero direction");
}

static void realAltitudeNan()
{
    Environment env; EngineObject object("o");
    InformationReal real(&object, 0u, 0.0f, 0.0, 0.0f);
    Vec3 direction; direction.x = 10.0f; direction.y = 0.0f; direction.z = 0.0f;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (int i = 0; i <= 10; ++i) heights[100.0f + i] = 60.0f - i;   // 60, 59 .. 50
    heights[105.0f] = nan;
    check(real.getAltitude(direction) == 50.0f, "a NaN sample keeps the lowest so far (the PE's order)");
    calls.clear();
    heights[100.0f] = nan;
    check(std::isnan(real.getAltitude(direction)), "a NaN START poisons the result");
    calls.clear();
}

static void realSetTime()
{
    Environment env; InformationFactory factory;
    EngineObject object("o"); EnginePhysical physical("p"); EngineUnit engineUnit(&object);
    object.m_physical = &physical; object.m_unit = &engineUnit;
    const unsigned int handle = factory.requestInformationReal(0xFFFFFFFFu, &object, 0u, 0.0f, 0.0, 0.0f);
    Information* info = record(handle);
    check(factory.requestInformationPlugin(handle, 4, true), "physical half");
    timer.m_now = 50.0;
    primeFactoryUnitAllocation();
    IPIUnit* unit = new IPIUnit(info);
    check(!seedNextUnitAllocation, "the real unit allocation consumed the deterministic cache seed");
    check(info->setUnit(unit), "unit half");
    calls.clear();
    check(info->setTime(kTime), "setTime answers true");
    expectTrace("o:isPrimary,o:physical,p:maxHealth,p:health,o:physical,p:health,p:maxHealth,"
                "o:physical,p:maxHealth,p:health,o:physical,p:health,p:maxHealth,o:unit,"
                "settings:battle,unit:battle,settings:strategic,unit:strategic",
                "physical refresh, strength from the health FRACTION, then the plug-ins' time");
    // A primary passes the time to every secondary's record on its own side first.
    EngineObject first("s1"), second("s2");
    first.m_handles[1] = factory.requestInformationReal(0xFFFFFFFFu, &first, 0u, 0.0f, 0.0, 0.0f);
    second.m_handles[1] = factory.requestInformationReal(0xFFFFFFFFu, &second, 0u, 0.0f, 0.0, 0.0f);
    EngineObject owner("q"); owner.m_primary = true; owner.m_side = 1;
    owner.m_secondaries.push_back(&first); owner.m_secondaries.push_back(&second);
    const unsigned int primaryHandle = factory.requestInformationReal(0xFFFFFFFFu, &owner, 0u, 0.0f, 0.0, 0.0f);
    calls.clear();
    check(record(primaryHandle)->setTime(kTime), "the primary answers true");
    expectTrace("q:isPrimary,q:side,q:firstSecondary,s1:info1,s1:isPrimary,q:secondary,s2:info1,s2:isPrimary,q:secondary",
                "the secondaries are walked through the pool, each told the time");
}

// IPIPhysicalReal
static void physicalForwards()
{
    Environment env; InformationFactory factory;
    EngineObject object("o"); EnginePhysical physical("p"); object.m_physical = &physical;
    const unsigned int handle = factory.requestInformationReal(0xFFFFFFFFu, &object, 0u, 0.0f, 0.0, 0.0f);
    IPIPhysicalReal real(record(handle));
    check(real.getType() == 0, "type 0");
    check(bits(real.getHealth()) == bits(0.25f), "health over maximum");
    expectTrace("o:physical,p:health,p:maxHealth", "health is read first");
    check(real.getMaxHealth() == 200.0f, "maximum"); expectTrace("o:physical,p:maxHealth", "maximum");
    real.setHealth(3.0f); expectTrace("", "setHealth is empty");
    timer.m_now = 55.0;
    const float mod = real.getMod();
    const float want = static_cast<float>(static_cast<double>(SCurve::calculate(50.0f)) * 0.5 + 0.5f);
    check(bits(mod) == bits(want), "0.5 + 0.5 * SCurve(ABSOLUTE health)");
    expectTrace("timer:now,o:physical,p:maxHealth,p:health", "getMod refreshes through setTime() first");
    physical.m_maxHealth = 0.0f;
    const float zeroMod = real.getMod();
    check(bits(zeroMod) == bits(static_cast<float>(static_cast<double>(SCurve::calculate(0.0f)) * 0.5 + 0.5f)),
          "a maximum <= 0 reads the health as 0");
    expectTrace("timer:now,o:physical,p:maxHealth", "without asking the health");
    physical.m_maxHealth = 200.0f;
    check(real.getBoundingBox() == reinterpret_cast<BoundingBox*>(&boxToken), "current bounding box");
    expectTrace("o:physical,p:currentBox", "getBoundingBox -> getCurrentBoundingBox");
    check(real.getLocalBoundingBox() == reinterpret_cast<const LocalBoundingBox*>(&localBoxToken), "local box");
    expectTrace("o:physical,p:localBox", "local box");
    check(real.getRadius() == 9.0f, "radius"); expectTrace("o:physical,p:radius", "radius");
    const Vec3 offset = real.getSphereLocalOffset();
    check(offset.x == 1.5f && offset.y == -2.5f && offset.z == 3.5f, "sphere offset by value");
    expectTrace("o:physical,p:sphereOffset", "sphere offset");
    check(real.getSmallestRadius() == 0.5f, "smallest radius"); expectTrace("o:physical,p:smallestRadius", "smallest radius");
    std::vector<Vec2> spheres;
    real.getSubSpheres2d(spheres);
    check(spheres.size() == 1 && spheres[0].x == 4.0f, "sub-spheres fill the caller's vector");
    expectTrace("o:physical,p:subSpheres", "sub-spheres");
    Vec3 direction; direction.x = 1; direction.y = 0; direction.z = 0; armourDirection = &direction;
    check(real.getArmour(direction) == 0.75f, "armour"); expectTrace("o:physical,p:armour", "armour");
    check(real.getBestArmour() == 0.9f, "best armour"); expectTrace("o:physical,p:bestArmour", "best armour");
    check(real.getStrengthType() == 3, "strength type"); expectTrace("o:physical,p:strType", "getStrengthType -> getStrType");
    check(real.getVolume() == 12.5f, "volume"); expectTrace("o:physical,p:volume", "volume");
    check(real.getMass() == 1500.0f, "mass"); expectTrace("o:physical,p:mass", "mass");
    check(real.getMaterial() == 42, "material"); expectTrace("o:physical,p:material", "material");
    check(real.getManDownTimeLeft() == 6.0f, "man-down left"); expectTrace("o:physical,p:manDownLeft", "man-down left");
    check(real.getMaxManDownTime() == 15.0f, "man-down max"); expectTrace("o:physical,p:manDownMax", "man-down max");
    check(real.getManDown(), "man down"); expectTrace("o:physical,p:manDown", "man down");
    Vec3 probe; probe.x = probe.y = probe.z = 0; terrainProbe = &probe;
    check(real.isTouchingTerrain(probe), "touching terrain"); expectTrace("o:physical,p:touching", "touching");
    check(!real.isInAir(), "in air"); expectTrace("o:physical,p:inAir", "in air");
    check(real.isInWater(), "in water"); expectTrace("o:physical,p:inWater", "in water");
}

// WeaponInfo / WeaponInfoReal
static void weaponInfo()
{
    WeaponObserver weapon("w"); EngineObject locked("locked"); weapon.m_lockedTarget = &locked;
    WeaponInfoProbe info(&weapon);
    check(sizeof(WeaponInfo) == 12 && sizeof(WeaponInfoReal) == 12 && sizeof(WeaponInfoProbe) == 12,
          "the base and Real record carry exactly vptr, IWeapon pointer and rearm float");
    check(info.rawWeapon() == &weapon && bits(info.rearmAmount()) == bits(0.0f),
          "the constructor keeps the non-owning weapon and zeros the rearm accumulator");
    check(info.update(), "both base and Real update answer true");
    check(info.getFireRate() == 37 && info.getReloadTime() == 9.25f, "rate and reload forwards");
    check(&info.getAbsoluteTransformation() == &weapon.m_absolute && &info.getPosition() == &weapon.m_position &&
          &info.getProjectileOrigin() == &weapon.m_projectileOrigin, "transformation forwards preserve references");
    check(info.hasAutoReload() && info.hasHeat(), "reload and heat presence forwards");
    check(info.getWeaponType() == 61 && info.getWeaponGuidanceType() == 67, "weapon type forwards");
    check(info.getBurst() && !info.getIndirect() && info.getProjectileMaterial() == 71, "fire shape forwards");
    check(info.getMaxAmmoInMag() == 53 && info.getDrag() == 1.25f && info.getGravityModifier() == 2.25f &&
          info.getExitVelocity() == 3.25f, "ballistic forwards");
    check(info.getMinRange() == 4.25f && info.getMaxRange() == 5.25f &&
          info.getOptimalRangePercentage() == 6.25f, "range forwards");
    check(info.getFiresThroughTransparent() && info.getFiringPose() == SPCrouching &&
          info.getAllowedDeviation() == 7.25f && info.getExplosionRadius() == 8.25f,
          "aim and explosion forwards");
    check(info.getStrengths() == weapon.m_strengths && info.getStrength(1) == 73, "strength forwards");
    check(info.getWeaponActivate() == 17 && info.getWeaponFire() == 19 && info.getWeaponZoom() == 23 &&
          info.getWeaponAltFire() == 29 && info.isZoomed() && info.getWeaponState() == 31,
          "input/state forwards");
    check(info.getMaxAmmo() == 47 && info.getIsThrown() && !info.useAimerOnly(), "ammo and aimer forwards");
    check(info.getSoundSphereRadius() == 13.25f && !info.setFakeFire(true) &&
          info.getReplenishRadius() == 14.25f, "sound, fake-fire and replenish forwards");
    info.setBotSkill(0.25f, kTime, 0.75f);
    check(info.setTarget(0x12345678u) && weapon.m_targetId == 0x12345678u,
          "target id and bool return forward");
    check(info.hasTargetLocked() && info.getLockedTarget() == &locked && &info.getName() == &weapon.m_name,
          "lock and name forwards");
    check(info.getShotsInMag(7) == 107 && info.getAmmo() == 41 && info.getCurrentAmmo() == 43 &&
          info.getNumberOfMags() == 59, "Real ammo virtuals forward");
    check(info.isReadyToFire() && info.getTimeOfFiring() == 11.5 && info.getHeat() == 12.25f &&
          !info.usesHeat(), "Real firing virtuals forward");
    check(info.getSecurity() == 1.0f && info.getSecurity(kTime) == 1.0f,
          "Real security is exactly one without consulting the weapon");
    expectTrace("w:rate,w:reload,w:absolute,w:position,w:origin,w:autoReload,w:hasHeat,w:type,w:guidance,"
                "w:burst,w:indirect,w:material,w:maxMag,w:drag,w:gravity,w:velocity,w:minRange,w:maxRange,"
                "w:optimal,w:transparent,w:pose,w:deviation,w:explosion,w:strengths,w:strength1,w:activate,"
                "w:fire,w:zoom,w:altfire,w:zoomed,w:state,w:maxAmmo,w:thrown,w:aimer,w:sound,w:fake1,"
                "w:replenish,w:skill0.25/401c000000000002/0.75,w:target305419896,w:locked?,w:locked,w:name,"
                "w:mag7,w:ammo,w:currentAmmo,w:mags,w:ready,w:firing,w:heat,w:usesHeat",
                "every wrapper reaches the matching typed IWeapon slot once and in call order");
}

static void factoryWeapon()
{
    Environment env; FactoryObserver factory; WeaponObserver first("w1"), second("w2");
    void* const savedFactory = CommonSingletonServer::m_iInformationFactory;
    CommonSingletonServer::m_iInformationFactory = static_cast<IInformationFactory*>(&factory);
    handlesExpectedReleased.clear();
    const unsigned int handle = factory.requestWeaponInfo(0xFFFFFFFFu, &first, true);
    WeaponInfo* const firstInfo = WeaponInfoManager::m_instance->getInformation(handle);
    check((handle & 0xFFFFu) == 1u && firstInfo && firstInfo->getAmmo() == 41,
          "the Real factory arm registers a live truth-view record");
    expectTrace("factory:weapon/real,w1:ammo", "real weapon creation and use");
    const unsigned int same = factory.requestWeaponInfo(handle, &second, true);
    WeaponInfo* const secondInfo = WeaponInfoManager::m_instance->getInformation(handle);
    check(same == handle && secondInfo != firstInfo && secondInfo->getAmmo() == 41,
          "an explicit valid handle is overwritten with a fresh Real record");
    expectTrace("factory:weapon/real,w2:ammo", "overwrite uses the second weapon");
    CommonSingletonServer::m_iInformationFactory = savedFactory;
}

static void armamentReal()
{
    Environment env; FactoryObserver factory; EngineObject object("o"); EngineArmament engine(&object);
    WeaponObserver first("w1"), second("w2"); object.m_armament = &engine;
    engine.m_weapons.push_back(nullptr); engine.m_weapons.push_back(nullptr);
    const unsigned int informationHandle =
        factory.requestInformationReal(0xFFFFFFFFu, &object, 0u, 0.0f, 0.0, 0.0f);
    Information* const information = record(informationHandle);
    void* const savedFactory = CommonSingletonServer::m_iInformationFactory;
    CommonSingletonServer::m_iInformationFactory = static_cast<IInformationFactory*>(&factory);
    handlesExpectedReleased.clear(); calls.clear();

    check(factory.requestInformationPlugin(informationHandle, 0, true),
          "the real armament factory arm answers true");
    IPIArmament* const armament = information->getArmament();
    check(armament && armament->getType() == 4, "the target publishes an IPIArmamentReal of type 4");
    check(armament->getWeapons().size() == 2 && armament->getWeapon(0) == 0xFFFFFFFFu &&
          armament->getWeapon(1) == 0xFFFFFFFFu, "null-only engine weapons publish invalid handles");
    expectTrace("o:armament,arm:weapons", "construction obtains the engine vector after no old frees");

    timer.m_now = 4321.25; armament->setTime(); armament->setTime(kTime);
    check(armament->getActiveWeapon() == 3, "active weapon forward");
    armament->setActiveWeapon(7, 2);
    check(armament->getWeaponIdFromActivationId(11, 13) == 1 && armament->isAntiAircraft() == 83,
          "activation mapping and anti-aircraft forwards");
    Vec3 position, speed;
    check(armament->getProjectilePosition(position) && !armament->getProjectileSpeed(speed) &&
          position.x == 101 && position.y == 102 && position.z == 103 &&
          speed.x == 201 && speed.y == 202 && speed.z == 203, "projectile forwards preserve outputs and bools");
    expectTrace("timer:now,o:armament,arm:active,o:armament,arm:setActive7/2,o:armament,arm:map11/13,"
                "o:armament,arm:aa,o:armament,arm:projectilePos,o:armament,arm:projectileSpeed",
                "base forwards use the source Information's engine armament");

    engine.m_weapons.clear(); engine.m_weapons.push_back(&first); engine.m_weapons.push_back(&second);
    engine.m_weapons.push_back(nullptr);
    check(factory.updateInformationPlugin(informationHandle, 0), "factory update dispatches the armament slot");
    std::vector<unsigned int>& initial = armament->getWeapons();
    check(initial.size() == 3 && initial[0] != 0xFFFFFFFFu && initial[1] != 0xFFFFFFFFu &&
          initial[2] == 0xFFFFFFFFu, "non-null weapons become registered Real handles; null stays invalid");
    WeaponInfo* const firstInfo = WeaponInfoManager::m_instance->getInformation(initial[0]);
    WeaponInfo* const secondInfo = WeaponInfoManager::m_instance->getInformation(initial[1]);
    check(firstInfo && secondInfo && &firstInfo->getName() == &first.m_name &&
          &secondInfo->getName() == &second.m_name, "the handles retain their matching engine weapons");
    expectTrace("o:armament,arm:weapons,factory:weapon/real,factory:weapon/real,w1:name,w2:name",
                "update obtains the vector once, then builds non-null slots in ascending order");

    engine.m_lastFired = &second;
    check(armament->getLastFiredWeapon() == secondInfo, "last-fired activation/fire mapping resolves its handle");
    expectTrace("o:armament,arm:lastFired,w2:fire,w2:activate,arm:map17/19",
                "last-fired reads fire before activation and maps activation first");

    engine.m_rearmResult = false; armament->rearm(firstInfo, 0.25f, 4);
    check(engine.m_lastRearmWeapon == &first && bits(engine.m_lastRearmAmount) == bits(0.25f) &&
          engine.m_lastRearmSide == 4, "a failed rearm retains the first accumulated amount");
    engine.m_rearmResult = true; armament->rearm(firstInfo, 0.5f, 5);
    check(bits(engine.m_lastRearmAmount) == bits(0.75f), "the retained amount is included in the next rearm");
    engine.m_rearmResult = false; armament->rearm(firstInfo, 0.125f, 6);
    check(bits(engine.m_lastRearmAmount) == bits(0.125f), "a successful rearm reset the accumulator to zero");
    expectTrace("o:armament,arm:rearm0.25/4,o:armament,arm:rearm0.75/5,o:armament,arm:rearm0.125/6",
                "rearm uses the engine weapon and preserves/reset the accumulator by bool result");

    const unsigned int oldFirst = initial[0], oldSecond = initial[1];
    handlesExpectedReleased = {oldFirst, oldSecond};
    engine.m_weapons.clear(); engine.m_weapons.push_back(&second); engine.m_weapons.push_back(nullptr);
    engine.m_weapons.push_back(&first);
    check(factory.updateInformationPlugin(informationHandle, 0), "a second full update succeeds");
    std::vector<unsigned int>& rebuilt = armament->getWeapons();
    check(!WeaponInfoManager::m_instance->getInformation(oldFirst) &&
          !WeaponInfoManager::m_instance->getInformation(oldSecond), "every prior handle is stale after refresh");
    check(rebuilt.size() == 3 && rebuilt[0] != 0xFFFFFFFFu && rebuilt[1] == 0xFFFFFFFFu &&
          rebuilt[2] != 0xFFFFFFFFu, "refresh rebuilds slots in the new engine-vector shape");
    const unsigned int retainedAfterDestruction = rebuilt[0];
    expectTrace("o:armament,arm:weapons,factory:weapon/real,factory:weapon/real",
                "all frees precede the one engine-vector fetch and ascending rebuild");
    handlesExpectedReleased.clear();
    check(information->removePlugIn(4), "the Information owns and deletes its armament plug-in");
    check(WeaponInfoManager::m_instance->getInformation(retainedAfterDestruction) != nullptr,
          "armament destruction deliberately does not release stored weapon handles");
    expectTrace("", "armament destruction has no engine-side effect");
    CommonSingletonServer::m_iInformationFactory = savedFactory;
}

static float expectedUnitModifier(float ammo, float health)
{
    const float fromAmmo =
        static_cast<float>(static_cast<double>(SCurve::calculate(ammo)) * 0.5 + 0.25);
    return static_cast<float>(static_cast<double>(SCurve::calculate(health)) * 0.25 + fromAmmo);
}

static void unitLifecycle()
{
    Environment env; EngineObject object("o"); EnginePhysical physical("p"); EngineUnit engineUnit(&object);
    object.m_physical = &physical; object.m_unit = &engineUnit;
    InformationReal information(&object, 0u, 0.0f, 0.0, 0.0f);
    check(information.setPhysical(new IPIPhysicalReal(&information)), "unit source has a physical half");

    alignas(IPIUnit) unsigned char storage[sizeof(IPIUnit)];
    seedUnitStorage(storage);
    timer.m_now = 50.0;
    resetUnitArrayTracking();
    IPIUnit* const unit = new (storage) IPIUnit(&information);
    check(unit->getType() == 3 && sizeof(IPIUnit) == 40, "the real ctor publishes type 3 and the x86 layout extent");
    unsigned int unresolvedPointer;
    std::memcpy(&unresolvedPointer, storage + 0x24, sizeof(unresolvedPointer));
    check(storage[0x20] == 0 && unresolvedPointer == 0xA5B6C7D8u,
          "the ctor clears only the ownership flag and leaves the final pointer bytes untouched");
    check(unitArrayAllocations == 2 && unitArrayDeletions == 0 &&
          unitArraySizes[0] == 3 * sizeof(float) && unitArraySizes[1] == 2 * sizeof(float),
          "the ctor allocates the settings-sized battle then strategic arrays");
    const float initialModifier = expectedUnitModifier(0.375f, 0.25f);
    check(bits(unit->getBattleStrengths()[0]) == bits(10.0f * initialModifier) &&
          bits(unit->getBattleStrengths()[2]) == bits(30.0f * initialModifier) &&
          bits(unit->getStrategicStrengths()[0]) == bits(7.0f * initialModifier) &&
          bits(unit->getStrategicStrengths()[1]) == bits(11.0f * initialModifier),
          "the ctor scales both strength arrays from the seeded cached ammo and physical health");
    expectTrace("timer:now,settings:battle,settings:strategic,timer:now,o:physical,p:health,p:maxHealth,"
                "o:unit,settings:battle,unit:battle,settings:strategic,unit:strategic",
                "ctor reads the cache first, allocates battle then strategic, then runs setTime");

    check(bits(unit->getAmmo(99.0)) == bits(0.375f), "an older time returns the seeded cached ammo");
    check(bits(unit->getAmmo(101.0)) == bits(1.0f), "a cache miss without armament answers one");
    check(bits(unit->getAmmo(101.0)) == bits(0.375f),
          "the no-armament return does not replace the cached ammo although it advances the cache time");
    expectTrace("", "cached and no-armament ammo paths ask no typed observer");

    WeaponObserver weapon("uw"), staleWeapon("stale");
    const unsigned int weaponHandle =
        WeaponInfoManager::m_instance->registerInformation(new WeaponInfoReal(&weapon));
    const unsigned int staleHandle =
        WeaponInfoManager::m_instance->registerInformation(new WeaponInfoReal(&staleWeapon));
    WeaponInfoManager::m_instance->freeInformation(staleHandle);
    ArmamentPlug* const armament = new ArmamentPlug;
    armament->m_unitWeapons.push_back(weaponHandle);
    armament->m_unitWeapons.push_back(0xFFFFFFFFu);
    armament->m_unitWeapons.push_back(staleHandle);
    check(information.setArmament(armament), "the ammo probe installs its chosen armament");
    check(bits(unit->getAmmo(102.0)) == bits(0.5f), "valid ammo is divided by the engine unit maximum");
    expectTrace("unitArm:weapons,unitArm:weapons,uw:ammo,unitArm:weapons,unitArm:weapons,"
                "unitArm:weapons,o:unit,unit:maxAmmo",
                "ammo repeats getWeapons for every end test and skips invalid and stale handles");

    unit->setTime(103.0);
    const float refreshedModifier = expectedUnitModifier(0.5f, 0.25f);
    check(bits(unit->getBattleStrengths()[1]) == bits(20.0f * refreshedModifier) &&
          bits(unit->getStrategicStrengths()[1]) == bits(11.0f * refreshedModifier),
          "setTime refreshes ammo and scales battle then strategic strengths");
    expectTrace("unitArm:weapons,unitArm:weapons,uw:ammo,unitArm:weapons,unitArm:weapons,"
                "unitArm:weapons,o:unit,unit:maxAmmo,o:physical,p:health,p:maxHealth,o:unit,"
                "settings:battle,unit:battle,settings:strategic,unit:strategic",
                "setTime performs ammo, modifier and strength work in shipped order");

    weapon.m_ammo = -1;
    check(bits(unit->getAmmo(104.0)) == bits(1.0f), "negative weapon ammo answers one immediately");
    check(bits(unit->getAmmo(104.0)) == bits(0.5f),
          "the negative-ammo return retains the prior cached value while advancing its time");
    expectTrace("unitArm:weapons,unitArm:weapons,uw:ammo",
                "negative ammo returns before another vector end check or maximum lookup");

    weapon.m_ammo = 41; engineUnit.m_maxAmmo = 0;
    check(bits(unit->getAmmo(105.0)) == bits(1.0f) && bits(unit->getAmmo(105.0)) == bits(1.0f),
          "a nonpositive maximum stores one in the cache for the equal-time hit");
    expectTrace("unitArm:weapons,unitArm:weapons,uw:ammo,unitArm:weapons,unitArm:weapons,"
                "unitArm:weapons,o:unit,unit:maxAmmo", "zero maximum is checked after summing weapons");
    engineUnit.m_maxAmmo = 82;

    const float high = 2.0f;
    std::memcpy(storage + 0x10, &high, sizeof(high));
    unit->calculateStrengths();
    check(bits(unit->getBattleStrengths()[2]) == bits(30.0f) &&
          bits(unit->getStrategicStrengths()[1]) == bits(11.0f),
          "calculateStrengths clamps a high modifier to one before both loops");
    const float low = -1.0f;
    std::memcpy(storage + 0x10, &low, sizeof(low));
    unit->calculateStrengths();
    check(bits(unit->getBattleStrengths()[0]) == bits(0.0f) &&
          bits(unit->getStrategicStrengths()[0]) == bits(0.0f),
          "calculateStrengths clamps a low modifier to zero before both loops");
    expectTrace("o:unit,settings:battle,unit:battle,settings:strategic,unit:strategic,"
                "o:unit,settings:battle,unit:battle,settings:strategic,unit:strategic",
                "strength clamping precedes each battle/strategic pair");

    check(unit->calculateStrMod(0.5f), "a new modifier answers true");
    check(!unit->calculateStrMod(0.5f), "the same modifier answers false");
    check(unit->calculateStrMod(0.75f), "a different health changes it");
    check(unit->calculateStrMod(std::numeric_limits<float>::quiet_NaN()), "a NaN health reads as SCurve 0 - changed");
    check(!unit->calculateStrMod(-1.0f), "a negative health is the same SCurve 0 - unchanged");
    check(!unit->calculateStrMod(std::numeric_limits<float>::infinity()), "infinity likewise");
    expectTrace("", "calculateStrMod asks nothing");

    unit->~IPIUnit();
    check(unitArrayAllocations == 2 && unitArrayDeletions == 2,
          "the real destructor releases both arrays in the constructed object");
    trackUnitArrays = false;
}

static void unitForwards()
{
    Environment env; EngineObject object("o"), occupier("r"), occupied("d");
    EnginePhysical physical("p"); EngineUnit engineUnit(&object);
    object.m_physical = &physical; object.m_unit = &engineUnit;
    occupier.m_handles[2] = 0x10203040u; occupied.m_handles[1] = 0x50607080u;
    engineUnit.m_occupier = &occupier; engineUnit.m_occupiedObject = &occupied;
    InformationReal information(&object, 0u, 0.0f, 0.0, 0.0f);
    check(information.setPhysical(new IPIPhysicalReal(&information)), "forward source has a physical half");
    alignas(IPIUnit) unsigned char storage[sizeof(IPIUnit)]; seedUnitStorage(storage);
    timer.m_now = 50.0; IPIUnit* const unit = new (storage) IPIUnit(&information); calls.clear();

    check(unit->getOccupier(2) == 0x10203040u && unit->getOccupiedObject(1) == 0x50607080u,
          "occupier forwards map returned objects through the requested side");
    engineUnit.m_occupier = nullptr;
    check(unit->getOccupier(0) == 0xFFFFFFFFu, "a null occupier maps to the invalid handle");
    check(unit->getOccupyingPlayerId(99) == 1234, "the side argument is ignored for occupying player id");
    expectTrace("o:unit,unit:occupier,r:info2,o:unit,unit:occupiedObject,d:info1,"
                "o:unit,unit:occupier,o:unit,unit:player", "occupancy mapping and null behavior");

    check(unit->getNBattleStrengths() == 3 && unit->getNStrategicStrengths() == 2,
          "strength counts come from settings");
    check(unit->getEquipmentType() == 91 && unit->isOccupied(), "equipment and occupancy forward");
    check(unit->useNoPathfindingToGetToObject() && !unit->hasExposedSoldier() &&
          unit->getAlwaysTargetSoldier(), "pathfinding and soldier flags forward");
    check(unit->getCullDistance() == 44.5f && unit->getCullDistanceSqr() == 1980.25f &&
          unit->getEntryRadius() == 6.25f, "distance forwards preserve their values");
    check(unit->getSelectKey() == io::PIPositionSelect3 && unit->getLastPlayerPossesionTime() == 12.5,
          "select key and possession time forward");
    expectTrace("settings:battle,settings:strategic,o:unit,unit:equipment,o:unit,unit:occupied?,"
                "o:unit,unit:noPath,o:unit,unit:exposed,o:unit,unit:alwaysTarget,o:unit,unit:cull,"
                "o:unit,unit:cullSq,o:unit,unit:entryRadius,o:unit,unit:select,o:unit,unit:possession",
                "scalar unit forwards use the typed engine unit once each");

    check(unit->getTeamLock() == 2 && unit->isTeamLocked(2) && !unit->isTeamLocked(1),
          "nonzero team lock accepts only the matching side");
    engineUnit.m_teamLock = 0;
    check(unit->isTeamLocked(99), "zero team lock accepts every side");
    check(unit->getMissileLockStatus() == 29, "missile lock getter forwards");
    unit->setMissileLockStatus(31);
    check(unit->getMissileLockStatus() == 31, "missile lock setter forwards before the next getter");
    check(unit->mustBorrowArmament(), "mustBorrow passes the owning object back to the engine unit");
    const Vec3& entry = unit->getEntryPositionOfNonMobile();
    check(&entry == &engineUnit.m_entryPosition && entry.x == 9.0f && entry.y == 8.0f && entry.z == 7.0f,
          "entry position preserves the engine reference");
    expectTrace("o:unit,unit:teamLock,o:unit,unit:teamLock,o:unit,unit:teamLock,o:unit,unit:teamLock,"
                "o:unit,unit:missile,o:unit,unit:setMissile31,o:unit,unit:missile,o:unit,unit:borrow,"
                "o:unit,unit:entry", "team, missile, borrow and entry-position forwards");
    unit->~IPIUnit();
}

static void unitFactory()
{
    Environment env; InformationFactory factory;
    EngineObject sourceObject("s"), trueObject("t"), falseObject("f");
    EnginePhysical physical("sp"); EngineUnit engineUnit(&sourceObject);
    sourceObject.m_physical = &physical; sourceObject.m_unit = &engineUnit;
    const unsigned int sourceHandle =
        factory.requestInformationReal(0xFFFFFFFFu, &sourceObject, 0u, 0.0f, 0.0, 0.0f);
    const unsigned int trueHandle =
        factory.requestInformationReal(0xFFFFFFFFu, &trueObject, 0u, 0.0f, 0.0, 0.0f);
    const unsigned int falseHandle =
        factory.requestInformationReal(0xFFFFFFFFu, &falseObject, 0u, 0.0f, 0.0, 0.0f);
    Information* const source = record(sourceHandle);
    Information* const trueTarget = record(trueHandle);
    Information* const falseTarget = record(falseHandle);
    check(factory.requestInformationPlugin(sourceHandle, 4, true) && source->getPhysical(),
          "the source has the physical half required by IPIUnit construction");
    timer.m_now = 50.0; calls.clear(); resetUnitArrayTracking();

    primeFactoryUnitAllocation();
    check(factory.requestInformationPlugin(sourceHandle, trueHandle, 5, true) && !seedNextUnitAllocation,
          "factory case 5 constructs and publishes the real=true unit");
    IPIUnit* const first = trueTarget->getUnit();
    check(first && first->getType() == 3 && unitArrayAllocations == 2,
          "the first target owns a complete type-3 unit and two arrays");
    expectTrace("timer:now,settings:battle,settings:strategic,timer:now,s:physical,sp:health,sp:maxHealth,"
                "s:unit,settings:battle,unit:battle,settings:strategic,unit:strategic",
                "factory true construction uses the source information throughout");

    primeFactoryUnitAllocation();
    check(factory.requestInformationPlugin(sourceHandle, falseHandle, 5, false) && !seedNextUnitAllocation,
          "factory case 5 ignores real=false and still publishes IPIUnit");
    IPIUnit* const second = falseTarget->getUnit();
    check(second && second != first && second->getType() == 3 && unitArrayAllocations == 4,
          "the false arm has the same concrete unit behavior and fresh arrays");
    expectTrace("timer:now,settings:battle,settings:strategic,timer:now,s:physical,sp:health,sp:maxHealth,"
                "s:unit,settings:battle,unit:battle,settings:strategic,unit:strategic",
                "factory false construction has the same source order");

    primeFactoryUnitAllocation();
    check(factory.requestInformationPlugin(sourceHandle, trueHandle, 5, false) && !seedNextUnitAllocation,
          "a duplicate case-5 request still answers true");
    check(trueTarget->getUnit() == first && unitArrayAllocations == 6 && unitArrayDeletions == 0,
          "first wins while the refused complete unit and its two arrays remain leaked");
    expectTrace("timer:now,settings:battle,settings:strategic,timer:now,s:physical,sp:health,sp:maxHealth,"
                "s:unit,settings:battle,unit:battle,settings:strategic,unit:strategic",
                "the duplicate is fully constructed before the setter refuses it");

    check(trueTarget->removePlugIn(3) && falseTarget->removePlugIn(3),
          "both published units are removable through the unit type");
    check(!trueTarget->getUnit() && !falseTarget->getUnit() && unitArrayDeletions == 4,
          "removing the installed units destroys four arrays; the duplicate's two remain leaked");
    expectTrace("", "unit array destruction has no typed engine effect");
    trackUnitArrays = false;
}

static void coverFactory()
{
    Environment env; InformationFactory factory;
    EngineObject sameObject("same"), sourceObject("source"), targetObject("target"), duplicateObject("duplicate");
    EngineCover sameCover(&sameObject, "sameCover"), sourceCover(&sourceObject, "sourceCover"),
                duplicateCover(&duplicateObject, "duplicateCover");
    sameObject.m_cover = &sameCover;
    sourceObject.m_cover = &sourceCover;
    duplicateObject.m_cover = &duplicateCover;

    CoverRecord* const same = new CoverRecord(&sameObject);
    CoverRecord* const source = new CoverRecord(&sourceObject);
    CoverRecord* const target = new CoverRecord(&targetObject);
    CoverRecord* const duplicate = new CoverRecord(&duplicateObject);
    const unsigned int sameHandle = factory.registerInformation(0xFFFFFFFFu, same);
    const unsigned int sourceHandle = factory.registerInformation(0xFFFFFFFFu, source);
    const unsigned int targetHandle = factory.registerInformation(0xFFFFFFFFu, target);
    const unsigned int duplicateHandle = factory.registerInformation(0xFFFFFFFFu, duplicate);
    outlineConstructions = outlineDestructions = 0;
    calls.clear();

    check(factory.requestInformationPlugin(sameHandle, 2, true),
          "the three-argument true request accepts case 2");
    IPICover* const samePlugIn = same->cover();
    check(samePlugIn && samePlugIn->getType() == 1 && samePlugIn->getOutline() == sameCover.m_lastOutline,
          "the same record receives a type-1 cover with the engine-created outline");
    check(outlineConstructions == 1 && outlineDestructions == 0,
          "the first cover owns one live outline");
    expectTrace("same:cover?,same:cover,sameCover:outline",
                "the same-handle true arm constructs only from its source object");

    check(factory.requestInformationPlugin(sourceHandle, targetHandle, 2, false),
          "the split false request accepts the same concrete case 2");
    IPICover* const splitPlugIn = target->cover();
    check(!source->cover() && splitPlugIn && splitPlugIn != samePlugIn &&
          splitPlugIn->getType() == 1 && splitPlugIn->getOutline() == sourceCover.m_lastOutline,
          "the target owns a fresh cover whose constructor used the source record");
    check(outlineConstructions == 2 && outlineDestructions == 0,
          "real=false constructs the same owned-outline shape");
    // The two pool lookups are inline, const reads and have no typed observer seam. Their mutual
    // order is source-evidence-only; the first observable call below still pins source construction
    // before publication to the target.
    expectTrace("source:cover?,source:cover,sourceCover:outline",
                "the split arm touches the source object and not the target object");

    check(factory.requestInformationPlugin(duplicateHandle, targetHandle, 2, true),
          "a duplicate case-2 request still answers true");
    check(target->cover() == splitPlugIn && duplicateCover.m_lastOutline &&
          duplicateCover.m_lastOutline != splitPlugIn->getOutline(),
          "first wins and keeps the originally published plug-in");
    check(outlineConstructions == 3 && outlineDestructions == 0,
          "the refused plug-in and its outline remain leaked");
    expectTrace("duplicate:cover?,duplicate:cover,duplicateCover:outline",
                "the refused duplicate is fully constructed from its source before setter refusal");

    check(same->removePlugIn(1) && target->removePlugIn(1),
          "both published covers remain owned by their target records");
    check(!same->cover() && !target->cover() && outlineDestructions == 2,
          "removing published covers destroys two outlines while the refused third stays leaked");
    expectTrace("~sameCover1,~sourceCover1", "installed outline destruction preserves removal order");
}

// the named stops: each must report exactly and abort
static void stop(const std::string& mode)
{
    Environment env; InformationFactory factory; BFInformationCourier courier; EngineObject object("o");
    const unsigned int handle = factory.requestInformationReal(0xFFFFFFFFu, &object, 0u, 0.0f, 0.0, 0.0f);
    if (mode == "stop_known") factory.requestInformationKnown(0xFFFFFFFFu, &object, 0u, 0.0, 0.1f, 0.2f, 0.3f, 1);
    else if (mode == "stop_weapon_known") factory.requestWeaponInfo(0xFFFFFFFFu, nullptr, false);
    else if (mode == "stop_armament_known") factory.requestInformationPlugin(handle, 0, false);
    else if (mode == "stop_cover_physical")
    {
        EnginePhysical physical("p");
        object.m_physical = &physical;
        factory.requestInformationPlugin(handle, 2, true);
    }
    else if (mode == "stop_physical_known") factory.requestInformationPlugin(handle, 4, false);
    else std::exit(2);
    unexpected("a named stop returned");
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    if (argc != 2) return 2;
    SCurve::init();   // as AIMain does before any curve is read
    const std::string mode = argv[1];
    if (mode.compare(0, 5, "stop_") == 0) stop(mode);
    else if (mode == "courier_pool") courierPool();
    else if (mode == "courier_free") courierFree();
    else if (mode == "courier_set") courierSet();
    else if (mode == "courier_armament") courierArmament();
    else if (mode == "courier_weapons") courierWeapons();
    else if (mode == "courier_polygon") courierPolygon();
    else if (mode == "factory_iface") factoryIface();
    else if (mode == "factory_register") factoryRegister();
    else if (mode == "factory_real") factoryReal();
    else if (mode == "factory_plugins") factoryPlugins();
    else if (mode == "factory_source") factorySource();
    else if (mode == "factory_update") factoryUpdate();
    else if (mode == "real_getters") realGetters();
    else if (mode == "real_altitude") realAltitude();
    else if (mode == "real_altitude_nan") realAltitudeNan();
    else if (mode == "real_settime") realSetTime();
    else if (mode == "physical_forwards") physicalForwards();
    else if (mode == "weapon_info") weaponInfo();
    else if (mode == "factory_weapon") factoryWeapon();
    else if (mode == "armament_real") armamentReal();
    else if (mode == "unit_lifecycle") unitLifecycle();
    else if (mode == "unit_forwards") unitForwards();
    else if (mode == "unit_factory") unitFactory();
    else if (mode == "cover_factory") coverFactory();
    else return 2;
    std::printf("PASS %s checks=%d\n", mode.c_str(), checks);
    std::fflush(stdout);
    std::_Exit(0);   // skip static teardown: the pools and records are deliberately left alive
}
