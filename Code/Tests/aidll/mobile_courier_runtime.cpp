// [TEMP] Focused strict-MSVC-x86 runtime fixture for the recovered mobile information plug-ins,
// BFInformationCourier polygon state, and InformationFactory type 3. Production TUs are linked
// whole. Generated typed observers abort on every virtual outside this fixture's explicit scope.
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/BFInformationCourier.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/InformationFactory.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/Information.h"
#include "../../BF2/AI/AIDLL/AI/Main/InformationManager.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/InformationReal.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIMobile.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIMobileKnown.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIMobileReal.h"
#include "../../BF2/AI/AIDLL/AI/AIPathFinding/AerialHeightMap/AerialHeightMap.h"
#include "../../BF2/AI/AIDLL/AI/Main/AISettingsInterfaces.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../BF2/AI/AIDLL/AI/Main/IAIPathfinding.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/BFTimer.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/IAIObject.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../BF2/Core/Ids.h"
#include "../../BF2/Math/Mat4.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;

static_assert(sizeof(void*) == 4, "x86 only");
static_assert(sizeof(IPIMobile::ValidPos) == 20, "retail ValidPos stride");
static_assert(offsetof(IPIMobile::ValidPos, polygon) == 0, "polygon is the first path output");
static_assert(offsetof(IPIMobile::ValidPos, island) == 4, "quadTree is the second path output");
static_assert(offsetof(IPIMobile::ValidPos, position) == 8, "position follows the two indices");
static_assert(sizeof(IPIMobile) == 72, "retail x86 IPIMobile layout");
static_assert(sizeof(IPIMobileReal) == 72, "retail x86 IPIMobileReal layout");
static_assert(sizeof(IPIMobileKnown) == 136, "retail MSVC x86 IPIMobileKnown layout");

[[noreturn]] static void unexpected(const char* what)
{
    std::fprintf(stderr, "UNEXPECTED %s\n", what);
    std::fflush(stderr);
    std::abort();
}

static int checks;
static std::vector<std::string> events;
static std::vector<int> informationSides;

static void trace(const std::string& event) { events.push_back(event); }
static void check(bool condition, const char* label)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL %s\n", label);
        for (const std::string& event : events)
            std::fprintf(stderr, "  %s\n", event.c_str());
        std::exit(2);
    }
}

static bool equalFloat(float a, float b, float tolerance = 0.0001f)
{
    return std::fabs(a - b) <= tolerance;
}

static unsigned int floatBits(float value)
{
    unsigned int result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

static float floatFromBits(unsigned int bits)
{
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

static double doubleFromBits(unsigned long long bits)
{
    double result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

static bool equalVec(const Vec3& a, const Vec3& b, float tolerance = 0.0001f)
{
    return equalFloat(a.x, b.x, tolerance)
        && equalFloat(a.y, b.y, tolerance)
        && equalFloat(a.z, b.z, tolerance);
}

static IPIMobile::ValidPos valid(unsigned int polygon, unsigned int quadTree,
                                 float x, float y, float z)
{
    IPIMobile::ValidPos result;
    result.polygon = polygon;
    result.island = quadTree;
    result.position.x = x;
    result.position.y = y;
    result.position.z = z;
    return result;
}

static bool equalPos(const IPIMobile::ValidPos& a, const IPIMobile::ValidPos& b)
{
    return a.polygon == b.polygon && a.island == b.island
        && equalVec(a.position, b.position);
}

static IPIMobile::ValidPos rawPos(const IPIMobile& mobile, std::size_t offset)
{
    IPIMobile::ValidPos result;
    std::memcpy(&result, reinterpret_cast<const unsigned char*>(&mobile) + offset, sizeof(result));
    return result;
}

static bool rawSelector(const IPIMobile& mobile)
{
    bool result;
    std::memcpy(&result, reinterpret_cast<const unsigned char*>(&mobile) + 0x44, sizeof(result));
    return result;
}

static void setRecords(IPIMobile& mobile, const IPIMobile::ValidPos& infantry,
                       const IPIMobile::ValidPos& first, const IPIMobile::ValidPos& second,
                       bool useFirst)
{
    mobile.setLastValidPositions(infantry, first, second);
    mobile.setLastValidVehiclePosInternal(first, second, useFirst);
}

struct PolygonReply
{
    bool result;
    unsigned int polygon;
    unsigned int quadTree;
};

struct PolygonCall
{
    Vec3 position;
    int map;
    unsigned int initialPolygon;
    unsigned int initialQuadTree;
    unsigned int hintPolygon;
    unsigned int hintQuadTree;
    int equipment;
};

struct MaterialCall
{
    int map;
    unsigned int firstIndex;
    unsigned int secondIndex;
};

static std::vector<PolygonReply> polygonReplies;
static std::vector<PolygonCall> polygonCalls;
static std::vector<unsigned char> materialReplies;
static std::vector<MaterialCall> materialCalls;
static std::size_t nextPolygonReply;
static std::size_t nextMaterialReply;
static float aerialAltitude;

static void resetObservations()
{
    events.clear();
    informationSides.clear();
    polygonReplies.clear();
    polygonCalls.clear();
    materialReplies.clear();
    materialCalls.clear();
    nextPolygonReply = 0;
    nextMaterialReply = 0;
}

#include "mobile_observers.inl"

namespace dice { namespace hfe {
class ClassManager;
ClassManager* classManager = nullptr; // [TEMP] roster guards below keep it unreachable.

namespace ai {
static int rosterToken;
void* CommonSingletonServer::m_iBFTimer = nullptr;
void* CommonSingletonServer::m_iaiInformationCourier = &rosterToken;
void* CommonSingletonServer::m_iInformationFactory = &rosterToken;
void* CommonSingletonServer::m_iaiSettings = &rosterToken;

// [TEMP] These interface lifetimes live in the engine/other AI TU, outside the focused link.
IBFTimer::IBFTimer() {}
IBFTimer::~IBFTimer() {}
IAISettings::IAISettings() {}
IAISettings::~IAISettings() {}

static int heightMapToken;
AerialHeightMap* AerialHeightMap::m_instance =
    reinterpret_cast<AerialHeightMap*>(&heightMapToken);

float AerialHeightMap::getAltitude(const Vec3& position)
{
    if (reinterpret_cast<void*>(this) != &heightMapToken)
        unexpected("AerialHeightMap instance");
    trace("height");
    (void)position;
    return aerialAltitude;
}

EngineObject::EngineObject(const char* id)
    : m_id(id), m_side(1), m_mobileType(false), m_mobile(nullptr), m_unit(nullptr)
{
    m_position.x = 10.0f;
    m_position.y = 20.0f;
    m_position.z = 30.0f;
    std::memset(&m_transform, 0, sizeof(m_transform));
    m_handles[0] = m_handles[1] = m_handles[2] = 0;
}

IAIObjectMobile* EngineObject::getMobile() const
{
    trace(m_id + ":mobile");
    return m_mobile;
}

IAIObjectUnit* EngineObject::getUnit() const
{
    trace(m_id + ":unit");
    return m_unit;
}

bool EngineObject::isOfType(InformationType type) const
{
    trace(m_id + ":type" + std::to_string(static_cast<int>(type)));
    return m_mobileType;
}

unsigned int EngineObject::getInformation(int side)
{
    trace(m_id + ":info" + std::to_string(side));
    informationSides.push_back(side);
    if (side < 0 || side > 2)
        unexpected("EngineObject information side");
    return m_handles[side];
}

const Vec3& EngineObject::getPosition() const
{
    trace(m_id + ":position");
    return m_position;
}

const Mat4& EngineObject::getTransformation() const
{
    trace(m_id + ":transform");
    return m_transform;
}

int EngineObject::getSide() const
{
    trace(m_id + ":side");
    return m_side;
}

EngineMobile::EngineMobile(IAIObject* owner, const char* id)
    : IAIObjectMobile(owner), m_id(id), m_vehicleNumber(0), m_sector(nullptr), m_lastPortal(-1),
      m_timerOnSpeed(nullptr)
{
    m_speed.x = m_speed.y = m_speed.z = 0.0f;
    m_rotationalSpeed.x = m_rotationalSpeed.y = m_rotationalSpeed.z = 0.0f;
}

const Vec3& EngineMobile::getSpeed()
{
    trace(m_id + ":speed");
    if (m_timerOnSpeed)
        CommonSingletonServer::m_iBFTimer = m_timerOnSpeed;
    return m_speed;
}
const Vec3& EngineMobile::getRotationalSpeed()
{ trace(m_id + ":rotational"); return m_rotationalSpeed; }
float EngineMobile::getSpeedMagnitude() { unexpected("EngineMobile::getSpeedMagnitude"); }
int EngineMobile::getVehicleNumber() const
{ trace(m_id + ":vehicle"); return m_vehicleNumber; }
float EngineMobile::getMaxSpeed() const { trace(m_id + ":maxSpeed"); return 101.0f; }
float EngineMobile::getTurnRadius() const { trace(m_id + ":turnRadius"); return 102.0f; }
float EngineMobile::getDrag() const { trace(m_id + ":drag"); return 103.0f; }
float EngineMobile::getCoverSearchRadius() const { trace(m_id + ":cover"); return 104.0f; }
float EngineMobile::getLodHeight() const { trace(m_id + ":lod"); return 105.0f; }
bool EngineMobile::isTurnable() const { trace(m_id + ":turnable"); return true; }
float EngineMobile::getSoundSphereRadius() const { trace(m_id + ":sound"); return 106.0f; }
float EngineMobile::getSoundSphereRadiusMin() const
{ trace(m_id + ":soundMin"); return 107.0f; }
float EngineMobile::getSoundSphereRadiusMax() const
{ trace(m_id + ":soundMax"); return 108.0f; }
float EngineMobile::getHearingProbabilityMax() const
{ trace(m_id + ":hearingMax"); return 109.0f; }
float EngineMobile::getHearingProbabilityMin() const
{ trace(m_id + ":hearingMin"); return 110.0f; }
float EngineMobile::getAvoidCollisionLookAhead() const
{ trace(m_id + ":avoid"); return 111.0f; }
IAIObject* EngineMobile::getSector() const { trace(m_id + ":sector"); return m_sector; }
int EngineMobile::getLastPassedPortal() const
{ trace(m_id + ":portal"); return m_lastPortal; }
float EngineMobile::getSprintStamina() const { trace(m_id + ":sprint"); return 112.0f; }

int EngineUnit::getEquipmentType() const
{
    trace("unit:equipment");
    return m_equipment;
}

PathObserver::PathObserver() : m_traceMap(0) {}

int PathObserver::getTraceMapIndex() const
{
    ::trace("path:traceMap");
    return m_traceMap;
}

bool PathObserver::getPolygon(const Vec3& position, int map, unsigned int& polygon,
                              unsigned int& quadTree, unsigned int hintPolygon,
                              unsigned int hintQuadTree, int equipment) const
{
    ::trace("path:polygon");
    PolygonCall call = {position, map, polygon, quadTree, hintPolygon, hintQuadTree, equipment};
    polygonCalls.push_back(call);
    if (nextPolygonReply >= polygonReplies.size())
        unexpected("PathObserver polygon reply");
    const PolygonReply reply = polygonReplies[nextPolygonReply++];
    if (reply.result)
    {
        polygon = reply.polygon;
        quadTree = reply.quadTree;
    }
    return reply.result;
}

bool PathObserver::getMaterial(int map, unsigned int firstIndex, unsigned int secondIndex,
                               unsigned char& material) const
{
    ::trace("path:material");
    materialCalls.push_back({map, firstIndex, secondIndex});
    if (nextMaterialReply >= materialReplies.size())
        unexpected("PathObserver material reply");
    material = materialReplies[nextMaterialReply++];
    return true;
}

double TimerObserver::getGameTime() const
{
    trace(m_id + ":now");
    if (m_speedOnNow)
        *m_speedOnNow = m_speedValue;
    return m_now;
}

} } } // namespace dice::hfe::ai

struct OwnedInformation
{
    unsigned int handle;
    Information* information;
    bool pooled;
};

class Environment
{
public:
    Environment()
    {
        informationManager = &manager;
        CommonSingletonServer::m_iBFTimer = &timer;
        CommonSingletonServer::m_iaiSettings = &settings;
        aerialAltitude = 0.0f;
        resetObservations();
    }

    ~Environment()
    {
        for (auto it = owned.rbegin(); it != owned.rend(); ++it)
        {
            if (it->pooled)
                manager.getPool()->release(it->handle);
            delete it->information;
        }
        owned.clear();
        CommonSingletonServer::m_iBFTimer = nullptr;
        CommonSingletonServer::m_iaiSettings = &dice::hfe::ai::rosterToken;
        informationManager = nullptr;
    }

    InformationReal* makeInformation(EngineObject& object, int side)
    {
        InformationReal* const information =
            new InformationReal(&object, 0, 1.0f, timer.m_now, 0.0f);
        const unsigned int handle = manager.getPool()->acquire(information);
        check(handle != 0, "information pool acquisition");
        object.m_handles[side] = handle;
        owned.push_back({handle, information, true});
        return information;
    }

    void release(unsigned int handle)
    {
        for (OwnedInformation& entry : owned)
        {
            if (entry.handle == handle && entry.pooled)
            {
                manager.getPool()->release(handle);
                entry.pooled = false;
                return;
            }
        }
        unexpected("Environment release handle");
    }

    IPIMobileReal* attachReal(Information* information)
    {
        IPIMobileReal* const mobile = new IPIMobileReal(information);
        check(information->setMobile(mobile), "attach Real mobile");
        return mobile;
    }

    InformationManager manager;
    TimerObserver timer;
    SettingsObserver settings;
    PathObserver path;
    std::vector<OwnedInformation> owned;
};

static void expectRawRecords(const IPIMobile& mobile, const IPIMobile::ValidPos& infantry,
                             const IPIMobile::ValidPos& first,
                             const IPIMobile::ValidPos& second, bool selector,
                             const char* label)
{
    check(equalPos(rawPos(mobile, 0x08), infantry), (std::string(label) + " infantry").c_str());
    check(equalPos(rawPos(mobile, 0x1C), first), (std::string(label) + " vehicle A").c_str());
    check(equalPos(rawPos(mobile, 0x30), second), (std::string(label) + " vehicle B").c_str());
    check(rawSelector(mobile) == selector, (std::string(label) + " selector").c_str());
}

static void pass(const char* mode)
{
    std::printf("PASS %s checks=%d\n", mode, checks);
}

static void modeLayout()
{
    Environment env;
    EngineObject object("layout");
    InformationReal information(&object, 0, 1.0f, 0.0, 0.0f);

    alignas(IPIMobileReal) unsigned char storage[sizeof(IPIMobileReal)];
    std::memset(storage, 0x5A, sizeof(storage));
    IPIMobileReal* const mobile = new (storage) IPIMobileReal(&information);
    const IPIMobile::ValidPos cleared = valid(0xFFFFFFFFu, 0xFFFFFFFFu, 0, 0, 0);
    check(equalPos(rawPos(*mobile, 0x08), cleared), "constructor clears infantry in field order");
    check(equalPos(rawPos(*mobile, 0x1C), cleared), "constructor clears vehicle A in field order");
    check(equalPos(rawPos(*mobile, 0x30), cleared), "constructor clears vehicle B in field order");
    unsigned char selectorByte = 0;
    std::memcpy(&selectorByte, storage + 0x44, 1);
    check(selectorByte == 0x5A, "constructor deliberately leaves selector byte untouched");
    mobile->~IPIMobileReal();
    pass("layout");
}

static void modeBaseMaterial()
{
    Environment env;
    EngineObject object("base");
    EngineMobile engineMobile(&object, "engine");
    object.m_mobile = &engineMobile;
    InformationReal information(&object, 0, 1.0f, 0.0, 0.0f);
    IPIMobileReal mobile(&information);
    const IPIMobile::ValidPos infantry = valid(1, 2, 3, 4, 5);
    const IPIMobile::ValidPos oldA = valid(10, 11, 12, 13, 14);
    const IPIMobile::ValidPos oldB = valid(20, 21, 22, 23, 24);
    setRecords(mobile, infantry, oldA, oldB, false);

    env.path.m_traceMap = 0;
    resetObservations();
    materialReplies.push_back(3);
    const IPIMobile::ValidPos material3 = valid(30, 31, 32, 33, 34);
    mobile.setLastValidVehiclePos(material3);
    expectRawRecords(mobile, infantry, material3, material3, true, "material 3");
    check(materialCalls.size() == 1, "material 3 issues one material query");
    check(materialCalls[0].map == 1, "material map is bool(traceMap == 0)");
    check(materialCalls[0].firstIndex == material3.island
          && materialCalls[0].secondIndex == material3.polygon,
          "material query receives quadTree then polygon");

    env.path.m_traceMap = 7;
    resetObservations();
    materialReplies.push_back(9);
    const IPIMobile::ValidPos other = valid(40, 41, 42, 43, 44);
    mobile.setLastValidVehiclePos(other);
    expectRawRecords(mobile, infantry, other, material3, true, "other material");
    check(materialCalls[0].map == 0, "nonzero trace map becomes material map zero");

    resetObservations();
    materialReplies.push_back(4);
    const IPIMobile::ValidPos material4 = valid(50, 51, 52, 53, 54);
    mobile.setLastValidVehiclePos(material4);
    expectRawRecords(mobile, infantry, other, material4, false, "material 4");
    check(equalPos(mobile.getLastValidPosition(99), material4),
          "material 4 selects vehicle B");
    engineMobile.m_vehicleNumber = env.path.m_traceMap;
    check(equalPos(mobile.getLastValidPosition(), infantry),
          "no-argument last-valid lookup forwards vehicle number and selects infantry");
    pass("base_material");
}

static void modeBasePositions()
{
    Environment env;
    EngineObject object("positions");
    InformationReal information(&object, 0, 1.0f, 0.0, 0.0f);
    IPIMobileReal mobile(&information);
    const IPIMobile::ValidPos initial = valid(1, 2, 3, 4, 5);
    mobile.setLastValidVehiclePosInternal(initial, initial, false);
    const IPIMobile::ValidPos infantry = valid(11, 12, 13, 14, 15);
    const IPIMobile::ValidPos first = valid(21, 22, 23, 24, 25);
    const IPIMobile::ValidPos second = valid(31, 32, 33, 34, 35);
    mobile.setLastValidPositions(infantry, first, second);
    expectRawRecords(mobile, infantry, first, second, false,
                     "setLastValidPositions preserves false selector");
    check(equalPos(mobile.getLastValidPosition(99), second), "false selector exposes B");
    mobile.setLastValidVehiclePosInternal(first, second, true);
    mobile.setLastValidPositions(infantry, second, first);
    expectRawRecords(mobile, infantry, second, first, true,
                     "setLastValidPositions preserves true selector");
    check(equalPos(mobile.getLastValidPosition(99), second), "true selector exposes A");
    pass("base_positions");
}

static void modeReal()
{
    Environment env;
    EngineObject object("real");
    EngineMobile engineMobile(&object, "mobile");
    object.m_mobile = &engineMobile;
    engineMobile.m_speed = {3.0f, 4.0f, 12.0f};
    engineMobile.m_rotationalSpeed = {6.0f, 7.0f, 8.0f};
    engineMobile.m_vehicleNumber = 6;
    engineMobile.m_lastPortal = 91;
    InformationReal information(&object, 0, 1.0f, 0.0, 0.0f);
    IPIMobileReal mobile(&information);

    check(&mobile.getSpeed() == &engineMobile.m_speed, "Real returns live speed reference");
    check(&mobile.getRotationalSpeed() == &engineMobile.m_rotationalSpeed,
          "Real returns live rotational-speed reference");
    check(equalFloat(mobile.getSpeedMagnitude(), 13.0f), "Real speed magnitude");
    check(mobile.getVehicleNumber() == 6, "base vehicle-number forward");
    check(equalFloat(mobile.getMaxSpeed(), 101.0f), "base max-speed forward");
    check(equalFloat(mobile.getTurnRadius(), 102.0f), "base turn-radius forward");
    check(equalFloat(mobile.getDrag(), 103.0f), "base drag forward");
    check(equalFloat(mobile.getCoverSearchRadius(), 104.0f), "base cover forward");
    check(mobile.isTurnable(), "base turnable forward");
    check(equalFloat(mobile.getSoundSphereRadius(), 106.0f), "Real sound forward");
    check(equalFloat(mobile.getSoundSphereRadiusMin(), 107.0f), "base sound-min forward");
    check(equalFloat(mobile.getSoundSphereRadiusMax(), 108.0f), "base sound-max forward");
    check(equalFloat(mobile.getHearingProbabilityMax(), 109.0f), "base hearing-max forward");
    check(equalFloat(mobile.getHearingProbabilityMin(), 110.0f), "base hearing-min forward");
    check(equalFloat(mobile.getAvoidCollisionLookAhead(), 111.0f), "base avoid forward");
    check(equalFloat(mobile.getSprintStamina(), 112.0f), "base sprint forward");
    check(mobile.getLastPassedPortal() == 91, "Real portal forward");

    env.timer.m_now = 40.0;
    resetObservations();
    mobile.setTime();
    check(events.size() == 1 && events[0] == "timer:now", "Real setTime reads timer once");

    Vec3 estimate;
    object.m_position = {10.0f, 20.0f, 30.0f};
    mobile.getPositionEstimate(estimate);
    check(equalVec(estimate, object.m_position), "Real current position estimate is exact");
    mobile.getPositionEstimate(42.0, estimate);
    const Vec3 expected = {16.0f, 28.0f, 54.0f};
    check(equalVec(estimate, expected), "Real timed estimate uses requested minus current time");

    engineMobile.m_sector = nullptr;
    check(mobile.getSector() == 0xFFFFFFFFu, "Real null sector returns -1");
    EngineObject sector("sector");
    sector.m_handles[1] = 0x12345678u;
    engineMobile.m_sector = &sector;
    check(mobile.getSector() == 0x12345678u, "Real sector resolves owning information side");
    pass("real");
}

static void modeRealRoundingOrder()
{
    Environment env;
    env.timer.m_id = "captured";
    env.timer.m_now = doubleFromBits(0x40BD5AA98D6E906Eull); // 7514.662314329396
    TimerObserver replacement("replacement");
    replacement.m_now = 0.0;

    EngineObject object("roundReal");
    EngineMobile engineMobile(&object, "roundEngine");
    object.m_mobile = &engineMobile;
    const float speed = floatFromBits(0xC7842738u);       // -67662.4375
    const float position = floatFromBits(0xCCFFD3EDu);    // -134127464
    engineMobile.m_speed = {0.0f, 0.0f, 0.0f};
    engineMobile.m_timerOnSpeed = &replacement;
    env.timer.m_speedOnNow = &engineMobile.m_speed;
    env.timer.m_speedValue = {speed, speed, speed};
    object.m_position = {position, position, position};
    InformationReal information(&object, 0, 1.0f, 0.0, 0.0f);
    IPIMobileReal mobile(&information);

    resetObservations();
    Vec3 estimate;
    mobile.getPositionEstimate(0.0, estimate);
    check(floatBits(estimate.x) == 0x4DB27EEFu,
          "Real X keeps product and addition wide until final store");
    check(floatBits(estimate.y) == 0x4DB27EF0u,
          "Real Y rounds its product before adding position");
    check(floatBits(estimate.z) == 0x4DB27EF0u,
          "Real Z rounds its product before adding position");
    check(floatBits(estimate.x) != floatBits(estimate.y),
          "Real bit probe distinguishes wide X from rounded Y/Z");
    check(floatBits(engineMobile.m_speed.x) == floatBits(speed),
          "captured timer mutates the returned speed reference before component reads");
    check(events.size() == 4 && events[0] == "roundReal:position"
          && events[1] == "roundReal:mobile" && events[2] == "roundEngine:speed"
          && events[3] == "captured:now",
          "Real order is position, speed, then captured timer receiver");
    check(CommonSingletonServer::m_iBFTimer == &replacement,
          "speed observer replaced global timer during the call");
    CommonSingletonServer::m_iBFTimer = &env.timer;
    pass("real_rounding_order");
}

static void modeKnown()
{
    Environment env;
    env.timer.m_now = 10.0;
    EngineObject object("known");
    EngineMobile engineMobile(&object, "engine");
    object.m_mobile = &engineMobile;
    object.m_side = 1;
    engineMobile.m_speed = {1.0f, 2.0f, 3.0f};
    engineMobile.m_rotationalSpeed = {4.0f, 5.0f, 6.0f};

    InformationReal* const source = env.makeInformation(object, 1);
    IPIMobileReal* const realMobile = env.attachReal(source);
    const IPIMobile::ValidPos infantry = valid(10, 11, 12, 13, 14);
    const IPIMobile::ValidPos first = valid(20, 21, 22, 23, 24);
    const IPIMobile::ValidPos second = valid(30, 31, 32, 33, 34);
    setRecords(*realMobile, infantry, first, second, true);

    resetObservations();
    IPIMobileKnown known(source);
    expectRawRecords(known, infantry, first, second, true, "Known constructor steal");
    check(equalVec(known.getSpeed(), engineMobile.m_speed), "Known constructor caches speed");
    const Vec3 zero = {0.0f, 0.0f, 0.0f};
    check(equalVec(known.getRotationalSpeed(), zero), "lost Known rotational cache starts zero");

    const IPIMobile::ValidPos ignored = valid(40, 41, 42, 43, 44);
    known.setLastValidInfantryPos(ignored);
    check(equalPos(rawPos(known, 0x08), infantry), "lost Known ignores infantry setter");

    const IPIMobile::ValidPos nextInfantry = valid(50, 51, 52, 53, 54);
    const IPIMobile::ValidPos nextFirst = valid(60, 61, 62, 63, 64);
    const IPIMobile::ValidPos nextSecond = valid(70, 71, 72, 73, 74);
    setRecords(*realMobile, nextInfantry, nextFirst, nextSecond, false);
    known.setHasContact();
    expectRawRecords(known, nextInfantry, nextFirst, nextSecond, false,
                     "setHasContact steals before enabling contact");

    engineMobile.m_speed = {7.0f, 8.0f, 9.0f};
    check(&known.getSpeed() == &engineMobile.m_speed, "contact Known uses live speed");
    check(&known.getRotationalSpeed() == &engineMobile.m_rotationalSpeed,
          "contact Known uses live rotational speed");
    known.setLastValidInfantryPos(ignored);
    check(equalPos(rawPos(known, 0x08), ignored), "contact Known accepts infantry setter");

    // A repeated contact notification does not steal again.
    setRecords(*realMobile, infantry, first, second, true);
    known.setHasContact();
    check(equalPos(rawPos(known, 0x08), ignored), "already-contact Known does not steal");

    EngineObject sector("sector");
    sector.m_handles[1] = 0x87654321u;
    engineMobile.m_sector = &sector;
    engineMobile.m_speed = {2.0f, 3.0f, 4.0f};
    env.timer.m_now = 20.0;
    setRecords(*realMobile, nextInfantry, nextFirst, nextSecond, false);
    resetObservations();
    known.setLostContact();
    check(known.getSector() == 0x87654321u, "lost contact caches sector information handle");
    expectRawRecords(known, nextInfantry, nextFirst, nextSecond, false,
                     "lost contact steals all real valid positions");
    check(events.size() >= 3 && events[0] == "known:mobile"
          && events[1] == "engine:speed" && events[2] == "timer:now",
          "lost contact resolves/caches speed before reading time");

    engineMobile.m_speed = {99.0f, 99.0f, 99.0f};
    const Vec3 cachedExpected = {2.0f, 3.0f, 4.0f};
    check(equalVec(known.getSpeed(), cachedExpected), "lost Known keeps cached linear speed");
    check(equalFloat(known.getSpeedMagnitude(), std::sqrt(29.0f)),
          "lost Known magnitude uses cached speed");

    object.m_position = {100.0f, 200.0f, 300.0f};
    Vec3 estimate;
    known.getPositionEstimate(22.0, estimate);
    const Vec3 timedExpected = {96.0f, 194.0f, 292.0f};
    check(equalVec(estimate, timedExpected),
          "Known estimate preserves timestamp-minus-requested-time direction");
    env.timer.m_now = 21.0;
    known.getPositionEstimate(estimate);
    const Vec3 currentExpected = {98.0f, 197.0f, 296.0f};
    check(equalVec(estimate, currentExpected), "lost Known no-time estimate dispatches with timer");

    known.setLastValidInfantryPos(first);
    check(equalPos(rawPos(known, 0x08), nextInfantry),
          "lost Known continues to ignore valid-position setters");
    pass("known");
}

static void modeKnownRounding()
{
    Environment env;
    env.timer.m_now = 0.0;
    EngineObject object("roundKnown");
    EngineMobile engineMobile(&object, "roundKnownEngine");
    object.m_mobile = &engineMobile;
    object.m_side = 1;
    const float speed = floatFromBits(0x4C781CE5u); // 65041300
    engineMobile.m_speed = {speed, speed, speed};
    object.m_position = {0.0f, 0.0f, 0.0f};
    InformationReal* const source = env.makeInformation(object, 1);
    IPIMobileReal* const realMobile = env.attachReal(source);
    const IPIMobile::ValidPos position = valid(1, 2, 3, 4, 5);
    setRecords(*realMobile, position, position, position, true);
    IPIMobileKnown known(source); // timestamp zero; constructor begins in lost-contact state.

    resetObservations();
    Vec3 estimate;
    known.getPositionEstimate(doubleFromBits(0x40E3AA00ED3DB156ull), estimate);
    check(floatBits(estimate.x) == 0xD4187749u,
          "Known X multiplies speed by the wide security-age scale");
    check(floatBits(estimate.y) == 0xD4187748u,
          "Known Y uses the rounded security-age scale");
    check(floatBits(estimate.z) == 0xD4187748u,
          "Known Z uses the rounded security-age scale");
    check(floatBits(estimate.x) != floatBits(estimate.y),
          "Known bit probe distinguishes wide X from rounded Y/Z");
    check(events.size() == 1 && events[0] == "roundKnown:position",
          "Known materializes its scaled vector before fetching current position");
    pass("known_rounding");
}

static void prepareUpdate(PathObserver& path, bool firstResult = false,
                          unsigned int polygon = 0, unsigned int quadTree = 0)
{
    resetObservations();
    polygonReplies.push_back({firstResult, polygon, quadTree});
    path.m_traceMap = 5;
}

static void modeSaveBranches()
{
    Environment env;
    EngineObject object("save");
    EngineMobile engineMobile(&object, "engine");
    EngineUnit unit(&object);
    object.m_mobile = &engineMobile;
    object.m_unit = &unit;
    object.m_side = 0;
    unit.m_equipment = 17;
    InformationReal* const usable = env.makeInformation(object, 1);
    IPIMobileReal* const mobile = env.attachReal(usable);
    const unsigned int usableHandle = object.m_handles[1];
    const IPIMobile::ValidPos infantry = valid(1, 2, 3, 4, 5);
    const IPIMobile::ValidPos first = valid(6, 7, 8, 9, 10);
    const IPIMobile::ValidPos second = valid(11, 12, 13, 14, 15);
    setRecords(*mobile, infantry, first, second, true);

    // Null explicitly forgets a previously saved object.
    {
        BFInformationCourier courier;
        courier.saveCurrentPolygon(&object);
        courier.saveCurrentPolygon(nullptr);
        prepareUpdate(env.path);
        courier.updateCurrentPolygon(&object);
        check(polygonCalls.size() == 1, "null save clears saved object");
    }

    // A stale information handle clears the saved object; side zero is normalized to one.
    {
        BFInformationCourier courier;
        object.m_handles[1] = 0;
        resetObservations();
        courier.saveCurrentPolygon(&object);
        check(informationSides.size() == 1 && informationSides[0] == 1,
              "save normalizes side zero to one");
        object.m_handles[1] = usableHandle;
        prepareUpdate(env.path);
        courier.updateCurrentPolygon(&object);
        check(polygonCalls.size() == 1, "stale save clears saved object");
    }

    // A live record with no mobile also clears it.
    {
        EngineObject bareObject("bare");
        bareObject.m_side = 1;
        InformationReal* const bare = env.makeInformation(bareObject, 1);
        const unsigned int bareHandle = bareObject.m_handles[1];
        (void)bare;
        BFInformationCourier courier;
        object.m_handles[1] = bareHandle;
        courier.saveCurrentPolygon(&object);
        object.m_handles[1] = usableHandle;
        prepareUpdate(env.path);
        courier.updateCurrentPolygon(&object);
        check(polygonCalls.size() == 1, "mobile-less save clears saved object");
    }

    // An invalid infantry polygon clears only the saved object; after the source is made valid the
    // update must query instead of restoring the partially copied snapshot.
    {
        BFInformationCourier courier;
        const IPIMobile::ValidPos invalid = valid(0xFFFFFFFFu, 91, 92, 93, 94);
        setRecords(*mobile, invalid, first, second, true);
        courier.saveCurrentPolygon(&object);
        setRecords(*mobile, infantry, first, second, true);
        prepareUpdate(env.path);
        courier.updateCurrentPolygon(&object);
        check(polygonCalls.size() == 1, "invalid infantry polygon clears saved object");
        check(polygonCalls[0].hintPolygon == infantry.polygon,
              "invalid save leaves live source usable for next query");
    }
    pass("save_branches");
}

static void modeSaveRestore()
{
    Environment env;
    EngineObject object("restore");
    EngineMobile engineMobile(&object, "engine");
    EngineUnit unit(&object);
    object.m_mobile = &engineMobile;
    object.m_unit = &unit;
    object.m_side = 1;
    InformationReal* const own = env.makeInformation(object, 1);
    InformationReal* const opposite = env.makeInformation(object, 2);
    IPIMobileReal* const ownMobile = env.attachReal(own);
    IPIMobileReal* const oppositeMobile = env.attachReal(opposite);
    const IPIMobile::ValidPos infantry = valid(101, 102, 103, 104, 105);
    const IPIMobile::ValidPos first = valid(201, 202, 203, 204, 205);
    const IPIMobile::ValidPos second = valid(301, 302, 303, 304, 305);
    setRecords(*ownMobile, infantry, first, second, true);

    BFInformationCourier courier;
    courier.saveCurrentPolygon(&object);
    const IPIMobile::ValidPos changed = valid(9, 8, 7, 6, 5);
    setRecords(*ownMobile, changed, changed, changed, true);
    setRecords(*oppositeMobile, changed, changed, changed, false);
    resetObservations();
    courier.updateCurrentPolygon(&object);
    expectRawRecords(*ownMobile, infantry, first, second, true, "saved restore own");
    expectRawRecords(*oppositeMobile, infantry, first, second, false, "saved restore opposite");
    check(polygonCalls.empty(), "saved restore makes no path query");
    check(informationSides.size() == 2 && informationSides[0] == 1
          && informationSides[1] == 2, "saved restore looks up own then opposite side");
    pass("save_restore");
}

static void modeUpdateBranches()
{
    Environment env;
    EngineObject object("update");
    EngineMobile engineMobile(&object, "engine");
    EngineUnit unit(&object);
    object.m_mobile = &engineMobile;
    object.m_unit = &unit;
    unit.m_equipment = 23;
    object.m_side = 0;
    InformationReal* const own = env.makeInformation(object, 1);
    IPIMobileReal* const mobile = env.attachReal(own);
    const IPIMobile::ValidPos infantry = valid(10, 11, 12, 13, 14);
    const IPIMobile::ValidPos first = valid(20, 21, 22, 23, 24);
    const IPIMobile::ValidPos second = valid(30, 31, 32, 33, 34);
    setRecords(*mobile, infantry, first, second, true);
    const unsigned int ownHandle = object.m_handles[1];
    BFInformationCourier courier;

    object.m_mobileType = true;
    aerialAltitude = 10.25f;
    resetObservations();
    courier.updateCurrentPolygon(&object);
    check(events.size() == 3 && events[0] == "update:position"
          && events[1] == "update:type4" && events[2] == "height",
          "high-air check is position, type 4, altitude");
    check(informationSides.empty() && polygonCalls.empty(),
          "altitude above ten skips side and path lookups");

    object.m_mobileType = false;
    object.m_position = {FLT_EPSILON, -FLT_EPSILON, FLT_EPSILON};
    resetObservations();
    courier.updateCurrentPolygon(&object);
    check(polygonCalls.empty(), "component magnitudes at epsilon count as zero");
    check(informationSides.size() == 2 && informationSides[0] == 1
          && informationSides[1] == 2, "side zero update looks up normalized own then side two");

    object.m_position.x = std::nextafter(FLT_EPSILON, std::numeric_limits<float>::infinity());
    prepareUpdate(env.path);
    courier.updateCurrentPolygon(&object);
    check(polygonCalls.size() == 1, "one component above epsilon proceeds to pathfinding");

    object.m_position = {10.0f, 20.0f, 30.0f};
    object.m_handles[1] = 0;
    prepareUpdate(env.path);
    courier.updateCurrentPolygon(&object);
    check(polygonCalls.empty(), "stale own information returns before pathfinding");
    check(informationSides.size() == 2 && informationSides[0] == 1
          && informationSides[1] == 2, "opposite lookup still follows stale own lookup");

    object.m_handles[1] = ownHandle;
    object.m_handles[2] = 0;
    prepareUpdate(env.path);
    courier.updateCurrentPolygon(&object);
    check(polygonCalls.size() == 1, "stale opposite information does not block own query");

    object.m_side = 2;
    object.m_handles[2] = ownHandle;
    object.m_handles[1] = 0;
    prepareUpdate(env.path);
    courier.updateCurrentPolygon(&object);
    check(informationSides.size() == 2 && informationSides[0] == 2
          && informationSides[1] == 1, "side two uses side one as opposite");
    check(polygonCalls.size() == 1, "side two valid own information reaches query");
    pass("update_branches");
}

static void modeUpdatePolygon()
{
    Environment env;
    EngineObject object("polygon");
    EngineMobile engineMobile(&object, "engine");
    EngineUnit unit(&object);
    object.m_mobile = &engineMobile;
    object.m_unit = &unit;
    object.m_side = 1;
    object.m_position = {7.0f, 8.0f, 9.0f};
    unit.m_equipment = 77;
    InformationReal* const own = env.makeInformation(object, 1);
    InformationReal* const opposite = env.makeInformation(object, 2);
    IPIMobileReal* const ownMobile = env.attachReal(own);
    IPIMobileReal* const oppositeMobile = env.attachReal(opposite);
    const IPIMobile::ValidPos infantry = valid(10, 11, 1, 2, 3);
    const IPIMobile::ValidPos first = valid(20, 21, 4, 5, 6);
    const IPIMobile::ValidPos second = valid(30, 31, 7, 8, 9);
    setRecords(*ownMobile, infantry, first, second, true);
    const IPIMobile::ValidPos oppositeOld = valid(90, 91, 92, 93, 94);
    setRecords(*oppositeMobile, oppositeOld, oppositeOld, oppositeOld, false);

    BFInformationCourier courier;
    env.path.m_traceMap = 5;
    resetObservations();
    polygonReplies.push_back({true, 101, 102});
    polygonReplies.push_back({true, 201, 202});
    materialReplies.push_back(3);
    materialReplies.push_back(4);
    courier.updateCurrentPolygon(&object);

    check(polygonCalls.size() == 2, "successful infantry query nests one vehicle query");
    const PolygonCall& infantryCall = polygonCalls[0];
    check(infantryCall.map == 5, "first polygon query uses trace map");
    check(infantryCall.initialPolygon == 0xFFFFFFFFu
          && infantryCall.initialQuadTree == 0xFFFFFFFFu,
          "first polygon output starts independently invalid");
    check(infantryCall.hintPolygon == infantry.polygon
          && infantryCall.hintQuadTree == infantry.quadTree,
          "first polygon hint order is polygon then quadTree");
    check(infantryCall.equipment == 77, "first polygon query forwards unit equipment type");
    check(equalVec(infantryCall.position, object.m_position),
          "first polygon query uses original position reference value");

    const PolygonCall& vehicleCall = polygonCalls[1];
    check(vehicleCall.map == 0, "second query map is bool(nonzero trace map == 0)");
    check(vehicleCall.initialPolygon == 0xFFFFFFFFu
          && vehicleCall.initialQuadTree == 0xFFFFFFFFu,
          "vehicle output is a separately initialized invalid record");
    check(vehicleCall.hintPolygon == first.polygon
          && vehicleCall.hintQuadTree == first.quadTree,
          "vehicle query uses selected own A hint in field order");
    check(vehicleCall.equipment == 77, "second polygon query forwards equipment type");

    const IPIMobile::ValidPos newInfantry = valid(101, 102, 7, 8, 9);
    const IPIMobile::ValidPos newVehicle = valid(201, 202, 7, 8, 9);
    expectRawRecords(*ownMobile, newInfantry, newVehicle, newVehicle, true,
                     "successful own updates");
    expectRawRecords(*oppositeMobile, newInfantry, oppositeOld, newVehicle, false,
                     "successful opposite updates");
    check(materialCalls.size() == 2, "both mobile copies route successful vehicle material");
    check(materialCalls[0].map == 0 && materialCalls[0].firstIndex == 202
          && materialCalls[0].secondIndex == 201,
          "own vehicle setter queries bool map, quadTree, polygon");
    check(materialCalls[1].map == 0 && materialCalls[1].firstIndex == 202
          && materialCalls[1].secondIndex == 201,
          "opposite vehicle setter receives the same new record");

    // The retail Boolean expression also maps trace-map zero to one.
    setRecords(*ownMobile, infantry, first, second, false);
    setRecords(*oppositeMobile, oppositeOld, oppositeOld, oppositeOld, true);
    env.path.m_traceMap = 0;
    resetObservations();
    polygonReplies.push_back({true, 301, 302});
    polygonReplies.push_back({true, 401, 402});
    materialReplies.push_back(9);
    materialReplies.push_back(9);
    courier.updateCurrentPolygon(&object);
    check(polygonCalls.size() == 2 && polygonCalls[1].map == 1,
          "second query maps trace-map zero to integer one");
    check(polygonCalls[1].hintPolygon == second.polygon
          && polygonCalls[1].hintQuadTree == second.quadTree,
          "false selector chooses own B vehicle hint");
    pass("update_polygon");
}

static void modeUpdateSecondFailure()
{
    Environment env;
    EngineObject object("failure");
    EngineMobile engineMobile(&object, "engine");
    EngineUnit unit(&object);
    object.m_mobile = &engineMobile;
    object.m_unit = &unit;
    object.m_side = 1;
    object.m_position = {1.0f, 2.0f, 3.0f};
    unit.m_equipment = 55;
    InformationReal* const own = env.makeInformation(object, 1);
    InformationReal* const opposite = env.makeInformation(object, 2);
    IPIMobileReal* const ownMobile = env.attachReal(own);
    IPIMobileReal* const oppositeMobile = env.attachReal(opposite);
    const IPIMobile::ValidPos infantry = valid(1, 2, 3, 4, 5);
    const IPIMobile::ValidPos first = valid(6, 7, 8, 9, 10);
    const IPIMobile::ValidPos second = valid(11, 12, 13, 14, 15);
    setRecords(*ownMobile, infantry, first, second, true);
    setRecords(*oppositeMobile, infantry, first, second, false);

    BFInformationCourier courier;
    env.path.m_traceMap = 4;
    resetObservations();
    polygonReplies.push_back({true, 100, 101});
    polygonReplies.push_back({false, 200, 201});
    courier.updateCurrentPolygon(&object);
    const IPIMobile::ValidPos updatedInfantry = valid(100, 101, 1, 2, 3);
    expectRawRecords(*ownMobile, updatedInfantry, first, second, true,
                     "second failure own partial update");
    expectRawRecords(*oppositeMobile, updatedInfantry, first, second, false,
                     "second failure opposite partial update");
    check(polygonCalls.size() == 2, "second failure follows first success");
    check(materialCalls.empty(), "failed second query invokes no vehicle setter");
    pass("update_second_failure");
}

static void modeFactoryMobile()
{
    Environment env;
    env.timer.m_now = 33.0;
    EngineObject sourceObject("source");
    EngineMobile engineMobile(&sourceObject, "engine");
    sourceObject.m_mobile = &engineMobile;
    sourceObject.m_side = 1;
    engineMobile.m_speed = {1.0f, 2.0f, 3.0f};
    InformationReal* const source = env.makeInformation(sourceObject, 1);
    IPIMobileReal* const sourceMobile = env.attachReal(source);
    const IPIMobile::ValidPos infantry = valid(1, 2, 3, 4, 5);
    const IPIMobile::ValidPos first = valid(6, 7, 8, 9, 10);
    const IPIMobile::ValidPos second = valid(11, 12, 13, 14, 15);
    setRecords(*sourceMobile, infantry, first, second, false);
    const unsigned int sourceHandle = sourceObject.m_handles[1];

    EngineObject realTargetObject("realTarget");
    InformationReal* const realTarget = env.makeInformation(realTargetObject, 1);
    const unsigned int realTargetHandle = realTargetObject.m_handles[1];
    EngineObject knownTargetObject("knownTarget");
    InformationReal* const knownTarget = env.makeInformation(knownTargetObject, 1);
    const unsigned int knownTargetHandle = knownTargetObject.m_handles[1];

    InformationFactory factory;
    check(factory.requestInformationPlugin(sourceHandle, realTargetHandle, 3, true),
          "factory real mobile returns true");
    check(dynamic_cast<IPIMobileReal*>(realTarget->getMobile()) != nullptr,
          "factory constructs IPIMobileReal on target");
    check(realTarget->getMobile()->getType() == 2, "factory Real has mobile type id");

    check(factory.requestInformationPlugin(sourceHandle, knownTargetHandle, 3, false),
          "factory known mobile returns true");
    IPIMobileKnown* const known = dynamic_cast<IPIMobileKnown*>(knownTarget->getMobile());
    check(known != nullptr, "factory constructs IPIMobileKnown on target");
    expectRawRecords(*known, infantry, first, second, false,
                     "factory Known constructor steals source mobile records");
    check(equalVec(known->getSpeed(), engineMobile.m_speed),
          "factory Known starts lost with constructor speed cache");
    pass("factory_mobile");
}

[[noreturn]] static void reachedAfter(const char* mode)
{
    std::fprintf(stderr, "REACHED-AFTER %s\n", mode);
    std::fflush(stderr);
    std::exit(0);
}

static void crashUpdateNullMobile()
{
    Environment env;
    EngineObject object("crashMobile");
    EngineUnit unit(&object);
    object.m_unit = &unit;
    object.m_side = 1;
    object.m_position = {1.0f, 2.0f, 3.0f};
    env.makeInformation(object, 1); // Deliberately no information-side mobile plug-in.
    BFInformationCourier courier;
    std::puts("ENTER crash_update_null_mobile");
    std::fflush(stdout);
    courier.updateCurrentPolygon(&object);
    reachedAfter("crash_update_null_mobile");
}

static void crashUpdateNullUnit()
{
    Environment env;
    EngineObject object("crashUnit");
    EngineMobile engineMobile(&object, "engine");
    object.m_mobile = &engineMobile;
    object.m_side = 1;
    object.m_position = {1.0f, 2.0f, 3.0f};
    InformationReal* const information = env.makeInformation(object, 1);
    env.attachReal(information);
    BFInformationCourier courier;
    std::puts("ENTER crash_update_null_unit");
    std::fflush(stdout);
    courier.updateCurrentPolygon(&object);
    reachedAfter("crash_update_null_unit");
}

static void crashKnownStaleReal()
{
    Environment env;
    EngineObject object("crashKnown");
    EngineMobile engineMobile(&object, "engine");
    object.m_mobile = &engineMobile;
    object.m_side = 1;
    object.m_handles[1] = 0;
    InformationReal source(&object, 0, 1.0f, 0.0, 0.0f);
    std::puts("ENTER crash_known_stale_real");
    std::fflush(stdout);
    IPIMobileKnown known(&source);
    (void)known;
    reachedAfter("crash_known_stale_real");
}

static void crashFactoryStaleTarget()
{
    Environment env;
    EngineObject sourceObject("crashFactory");
    InformationReal* const source = env.makeInformation(sourceObject, 1);
    const unsigned int sourceHandle = sourceObject.m_handles[1];
    (void)source;
    InformationFactory factory;
    std::puts("ENTER crash_factory_stale_target");
    std::fflush(stdout);
    factory.requestInformationPlugin(sourceHandle, 0, 3, true);
    reachedAfter("crash_factory_stale_target");
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    if (argc != 2)
        return 64;
    const std::string mode = argv[1];
    if (mode == "layout") modeLayout();
    else if (mode == "base_material") modeBaseMaterial();
    else if (mode == "base_positions") modeBasePositions();
    else if (mode == "real") modeReal();
    else if (mode == "real_rounding_order") modeRealRoundingOrder();
    else if (mode == "known") modeKnown();
    else if (mode == "known_rounding") modeKnownRounding();
    else if (mode == "save_branches") modeSaveBranches();
    else if (mode == "save_restore") modeSaveRestore();
    else if (mode == "update_branches") modeUpdateBranches();
    else if (mode == "update_polygon") modeUpdatePolygon();
    else if (mode == "update_second_failure") modeUpdateSecondFailure();
    else if (mode == "factory_mobile") modeFactoryMobile();
    else if (mode == "crash_update_null_mobile") crashUpdateNullMobile();
    else if (mode == "crash_update_null_unit") crashUpdateNullUnit();
    else if (mode == "crash_known_stale_real") crashKnownStaleReal();
    else if (mode == "crash_factory_stale_target") crashFactoryStaleTarget();
    else return 65;
    return 0;
}
