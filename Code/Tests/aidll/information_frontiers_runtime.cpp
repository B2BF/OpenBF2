// [TEMP] The whole production Information.cpp and recovered IPIUnit.cpp (plus their real pools)
// against typed observers. Every IAIObject / plug-in slot the TUs should reach is traced; every
// other one aborts. Chosen code is limited to the external observers and out-of-line base members
// belonging to engine TUs outside this focused link. CoverObserver uses the real IPICover
// constructor/destructor against a chosen empty engine cover; no IPICover body is shadowed.
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/Information.h"
#include "../../BF2/AI/AIDLL/AI/Main/InformationManager.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIArmament.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIControlInfo.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPICover.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIMobile.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIPhysical.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIUnit.h"
#include "../../BF2/AI/AIDLL/AI/AIUtils/WeaponInfoManager.h"
#include "../../BF2/AI/AIDLL/AI/Main/AISettingsInterfaces.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../BF2/AI/AIDLL/AI/Main/IAIPathfinding.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/BFTimer.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/IAIObject.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "x86 only");

[[noreturn]] static void unexpected(const char* what)
{
    std::fprintf(stderr, "UNEXPECTED %s\n", what);
    std::fflush(stderr);
    std::abort();
}
static std::string calls;
static int checks;
static bool unitBorrows;
static void trace(const std::string& event) { if (!calls.empty()) calls += ','; calls += event; }
static void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL %s\ntrace: %s\n", label, calls.c_str()); std::exit(2); }
}
static void expectTrace(const char* expected, const char* label)
{
    ++checks;
    if (calls != expected) {
        std::fprintf(stderr, "FAIL %s\nexpected: %s\nactual:   %s\n", label, expected, calls.c_str());
        std::exit(2);
    }
    calls.clear();
}
// The time as its exact bits, so a pass-through is proven bit for bit.
static std::string timeText(double time)
{
    unsigned long long bits; std::memcpy(&bits, &time, sizeof(bits));
    char text[24]; std::snprintf(text, sizeof(text), "%016llx", bits);
    return text;
}

#include "information_observers.inl"

namespace dice { namespace hfe { namespace ai {
// [TEMP] out-of-line members of TUs this fixture does not link
// IPIPhysical::getType lives in IPIPhysicalReal.cpp. Its observer overrides every slot;
// this unused base entry aborts if reached. IPIMobile now links its real production TU.
int IPIPhysical::getType() { unexpected("IPIPhysical::getType (base)"); }
IPIArmament::IPIArmament(Information* information) : m_weapons() { m_information = information; }
IPIArmament::~IPIArmament() {} // chosen: production TU is outside this Information-only fixture
int IPIArmament::getType() { unexpected("IPIArmament::getType (base)"); }
void IPIArmament::setTime() { unexpected("IPIArmament::setTime() (base)"); }
void IPIArmament::setTime(double) { unexpected("IPIArmament::setTime(double) (base)"); }
int IPIArmament::getActiveWeapon() const { unexpected("IPIArmament::getActiveWeapon (base)"); }
void IPIArmament::setActiveWeapon(int, int) { unexpected("IPIArmament::setActiveWeapon (base)"); }
int IPIArmament::getWeaponIdFromActivationId(int, int) const
{ unexpected("IPIArmament::getWeaponIdFromActivationId (base)"); }
WeaponInfo* IPIArmament::getLastFiredWeapon() { unexpected("IPIArmament::getLastFiredWeapon (base)"); }
int IPIArmament::isAntiAircraft() const { unexpected("IPIArmament::isAntiAircraft (base)"); }
void IPIArmament::rearm(WeaponInfo*, float, int) { unexpected("IPIArmament::rearm (base)"); }
bool IPIArmament::getProjectilePosition(Vec3&) const
{ unexpected("IPIArmament::getProjectilePosition (base)"); }
bool IPIArmament::getProjectileSpeed(Vec3&) const
{ unexpected("IPIArmament::getProjectileSpeed (base)"); }

// [TEMP] The interface lifecycles live in engine TUs outside this focused aidll link.
IBFTimer::IBFTimer() {}
IBFTimer::~IBFTimer() {}
IAISettings::IAISettings() {}
IAISettings::~IAISettings() {}

static TimerObserver timer;
static SettingsObserver settings;
void* CommonSingletonServer::m_iBFTimer = static_cast<IBFTimer*>(&timer);
void* CommonSingletonServer::m_iaiSettings = static_cast<IAISettings*>(&settings);
// [TEMP] No path query is exercised by the lifecycle observer. Whole IPIMobile.cpp refers to
// this roster pointer in unrelated nonvirtual methods; no pathfinder object is published here.
IAIPathfinding* IAIPathfinding::instance = nullptr;

// generated observers' chosen slots
ObservedObject::ObservedObject(const char* id)
    : m_id(id), m_name(std::string(id) + ".name"), m_aiName(std::string(id) + ".ai"),
      m_typeName(std::string(id) + ".type"), m_primary(nullptr), m_secondary(nullptr),
      m_spawner(nullptr), m_isPrimary(false), m_unit(new EngineUnit(this))
{
    m_handles[0] = m_handles[1] = m_handles[2] = 0;
}
ObservedObject::~ObservedObject() { delete m_unit; }
const std::string& ObservedObject::getName() const { trace(m_id + ":name"); return m_name; }
const std::string& ObservedObject::getAIName() const { trace(m_id + ":aiName"); return m_aiName; }
const std::string& ObservedObject::getObjectTypeName() const { trace(m_id + ":typeName"); return m_typeName; }
unsigned int ObservedObject::getInformation(int side)
{
    trace(m_id + ":info" + std::to_string(side));
    if (side < 0 || side > 2) unexpected("side outside 0..2");
    return m_handles[side];
}
unsigned int ObservedObject::getObjectId() const { trace(m_id + ":id"); return 0x12345678u; }
SoldierPose ObservedObject::getSoldierPose() const { trace(m_id + ":pose"); return SPLying; }
int ObservedObject::getKitType() const { trace(m_id + ":kit"); return 5; }
bool ObservedObject::isPrimary() const { trace(m_id + ":isPrimary"); return m_isPrimary; }
IAIObject* ObservedObject::getPrimaryObject() { trace(m_id + ":primary"); return m_primary; }
IAIObject* ObservedObject::getSecondaryObject(int& iterator)
{
    trace(m_id + ":secondary" + std::to_string(iterator)); iterator += 10; return m_secondary;
}
IAIObject* ObservedObject::getFirstSecondaryObject(int& iterator)
{
    trace(m_id + ":firstSecondary" + std::to_string(iterator)); iterator = 41; return m_secondary;
}
int ObservedObject::getFirstSecondaryObjectIterator() const { trace(m_id + ":firstIterator"); return 41; }
bool ObservedObject::getObjectSpawnerHolding() const { trace(m_id + ":holding"); return true; }
IAIObject* ObservedObject::getObjectSpawner() const { trace(m_id + ":spawner"); return m_spawner; }
bool ObservedObject::isRootObject() const { trace(m_id + ":root"); return true; }
int ObservedObject::getPlayerId() const { trace(m_id + ":player"); return -7; }
IAIObjectUnit* ObservedObject::getUnit() const { trace(m_id + ":unit"); return m_unit; }
bool ObservedObject::isCover() const
{
    if (m_id != "cover_fixture") unexpected("isCover outside chosen cover constructor");
    return true;
}
IAIObjectCover* ObservedObject::getCover() const
{
    if (m_id != "cover_fixture") unexpected("getCover outside chosen cover constructor");
    static EmptyEngineCover cover;
    return &cover;
}
IPlanePolygon* EmptyEngineCover::getObjectOutline() { return nullptr; }

PhysicalObserver::~PhysicalObserver() { trace("~physical"); }
void PhysicalObserver::setTime(double time) { trace("physical:" + timeText(time)); }
float PhysicalObserver::getHealth() const { trace("physical:health"); return 1.0f; }
bool PhysicalObserver::getManDown() const { trace("physical:mandown?"); return m_manDown; }
MobileObserver::~MobileObserver() { trace("~mobile"); }
void MobileObserver::setTime(double time) { trace("mobile:" + timeText(time)); }

double TimerObserver::getGameTime() const { trace("timer:now"); return m_now; }
int SettingsObserver::getNBattleStrengths() { trace("settings:battle"); return m_battleCount; }
int SettingsObserver::getNStrategicStrengths() { trace("settings:strategic"); return m_strategicCount; }

EngineUnit::EngineUnit(IAIObject* owner) : IAIObjectUnit(owner)
{
    m_battle[0] = 100;
    m_strategic[0] = 50;
}
int EngineUnit::getMaxAmmo() const { trace("unit:maxAmmo"); return 100; }
const int* EngineUnit::getBattleStrengths() const { trace("unit:battle"); return m_battle; }
const int* EngineUnit::getStrategicStrengths() const { trace("unit:strategic"); return m_strategic; }
bool EngineUnit::mustBorrowArmament(IAIObject* vehicle) const
{
    trace("unit:borrow?");
    if (vehicle != m_object)
        unexpected("IAIObjectUnit::mustBorrowArmament owner");
    return unitBorrows;
}
} } }

// [TEMP] Production IPIUnit construction intentionally reads its cache before writing it. This
// derived observer pre-fills only that 40-byte x86 allocation so the focused Information tests are
// deterministic, then observes the virtual time/destructor calls made by Information itself.
class UnitObserver : public IPIUnit
{
public:
    explicit UnitObserver(Information* information) : IPIUnit(information) {}
    ~UnitObserver() override { trace("~unit"); }
    void setTime() override { unexpected("UnitObserver::setTime()"); }
    void setTime(double time) override { trace("unit:" + timeText(time)); }
    static void* operator new(std::size_t size)
    {
        void* const memory = ::operator new(size);
        std::memset(memory, 0, size);
        const float cachedAmmo = 1.0f;
        const double cachedTime = 100.0;
        std::memcpy(static_cast<unsigned char*>(memory) + 0x14, &cachedAmmo, sizeof(cachedAmmo));
        std::memcpy(static_cast<unsigned char*>(memory) + 0x18, &cachedTime, sizeof(cachedTime));
        return memory;
    }
    static void operator delete(void* memory) noexcept { ::operator delete(memory); }
};
static_assert(sizeof(IPIUnit) == 40, "recovered x86 IPIUnit layout");
static_assert(sizeof(UnitObserver) == sizeof(IPIUnit), "observer adds no instance fields");

static IPIUnit* makeUnit(Information* information)
{
    IPIUnit* const unit = new UnitObserver(information);
    calls.clear(); // constructor dependencies are validated elsewhere; this fixture observes Information.
    return unit;
}

// [TEMP] hand-written observers of the plug-ins with no slots of their own
class ArmamentObserver : public IPIArmament
{
public:
    explicit ArmamentObserver(const char* id) : IPIArmament(nullptr), m_id(id) {}
    ~ArmamentObserver() { trace("~" + m_id); }
    int getType() override { unexpected("IPIArmament::getType"); }
    void setTime() override { unexpected("IPIArmament::setTime()"); }
    void setTime(double time) override { trace(m_id + ":" + timeText(time)); }
    int getActiveWeapon() const override { unexpected("IPIArmament::getActiveWeapon"); }
    void setActiveWeapon(int, int) override { unexpected("IPIArmament::setActiveWeapon"); }
    unsigned int getWeapon(int) override { unexpected("IPIArmament::getWeapon"); }
    std::vector<unsigned int>& getWeapons() override { unexpected("IPIArmament::getWeapons"); }
    int getWeaponIdFromActivationId(int, int) const override { unexpected("IPIArmament::getWeaponIdFromActivationId"); }
    WeaponInfo* getLastFiredWeapon() override { unexpected("IPIArmament::getLastFiredWeapon"); }
    int isAntiAircraft() const override { unexpected("IPIArmament::isAntiAircraft"); }
    void updateWeapons() override { unexpected("IPIArmament::updateWeapons"); }
    void rearm(WeaponInfo*, float, int) override { unexpected("IPIArmament::rearm"); }
    bool getProjectilePosition(Vec3&) const override { unexpected("IPIArmament::getProjectilePosition"); }
    bool getProjectileSpeed(Vec3&) const override { unexpected("IPIArmament::getProjectileSpeed"); }
    std::string m_id;
};
// [TEMP] The Information suite observes only the plug-in lifetime and time-slot dispatch.
// This separate native owner supplies the real IPICover constructor's required cover receiver.
// Its empty outline is chosen; nonempty IPICover ownership is tested by ipicover_runtime.
static Information* coverFixtureInformation()
{
    static ObservedObject owner("cover_fixture");
    static NativeInformation information(&owner);
    return &information;
}
class CoverObserver : public IPICover
{
public:
    CoverObserver() : IPICover(coverFixtureInformation()) {}
    ~CoverObserver() { trace("~cover"); }
    int getType() override { unexpected("IPICover::getType"); }
    void setTime() override { unexpected("IPICover::setTime()"); }
    void setTime(double time) override { trace("cover:" + timeText(time)); }
};
// The real control-info plug-in (header-inline, complete), with its time slot observed.
class ControlObserver : public IPIControlInfo
{
public:
    ControlObserver() : IPIControlInfo(nullptr) {}
    ~ControlObserver() { trace("~control"); }
    void setTime(double time) override { trace("control:" + timeText(time)); }
};

struct Six
{
    ArmamentObserver* armament; ControlObserver* control; CoverObserver* cover;
    MobileObserver* mobile; PhysicalObserver* physical; IPIUnit* unit;
};
static Six install(NativeInformation& info, bool manDown)
{
    Six six = { new ArmamentObserver("arm1"), new ControlObserver, new CoverObserver,
                new MobileObserver, new PhysicalObserver(manDown), nullptr };
    bool accepted = info.setPhysical(six.physical);
    six.unit = makeUnit(&info);
    accepted = info.setUnit(six.unit) && accepted;
    accepted = info.setArmament(six.armament) && accepted;
    accepted = info.setControlInfo(six.control) && accepted;
    accepted = info.setCover(six.cover) && accepted;
    accepted = info.setMobile(six.mobile) && accepted;
    check(accepted, "every empty setter accepts");
    calls.clear();
    return six;
}

static const double kTime = 1234.56789012345678;

static void forwards()
{
    ObservedObject owner("o");
    NativeInformation info(&owner);
    check(info.getAIObject() == &owner, "inline owner accessor");
    check(&info.getObjectTypeName() == &owner.m_typeName, "type name is the owner's own string");
    expectTrace("o:typeName", "type name asks once");
    check(info.getObjectId() == 0x12345678u, "object id"); expectTrace("o:id", "id asks once");
    check(info.getPlayerId() == -7, "player id"); expectTrace("o:player", "player asks once");
    check(!info.isPrimary(), "not primary"); owner.m_isPrimary = true;
    check(info.isPrimary(), "primary read fresh"); expectTrace("o:isPrimary,o:isPrimary", "isPrimary asks each time");
    check(info.getFirstSecondaryObjectIterator() == 41, "first iterator"); expectTrace("o:firstIterator", "iterator asks once");
    check(&info.getName() == &owner.m_name && &info.getAIName() == &owner.m_aiName, "names are the owner's strings");
    expectTrace("o:name,o:aiName", "names ask once each");
    check(info.getObjectSpawnerHolding(), "spawner holding"); expectTrace("o:holding", "holding asks once");
    check(info.getKitType() == 5, "kit type"); expectTrace("o:kit", "kit asks once");
    check(info.isRootObject(), "root"); expectTrace("o:root", "root asks once");
    check(info.getSoldierPose() == SPLying, "pose"); expectTrace("o:pose", "pose asks once");
}

static void primary()
{
    ObservedObject owner("o"), primaryObject("p");
    NativeInformation info(&owner);
    owner.m_primary = &primaryObject; primaryObject.m_handles[2] = 0x20005u;
    check(info.getPrimaryObject(2) == 0x20005u, "primary's information on the side");
    expectTrace("o:primary,o:primary,p:info2", "the primary is asked for TWICE");
    owner.m_primary = nullptr;
    check(info.getPrimaryObject(1) == 0xFFFFFFFFu, "no primary answers -1");
    expectTrace("o:primary", "a null primary is asked for once");
}

static void secondary()
{
    ObservedObject owner("o"), second("s");
    NativeInformation info(&owner);
    second.m_handles[0] = 0x10003u; second.m_handles[1] = 0x30007u;
    owner.m_secondary = &second;
    int iterator = 3;
    check(info.getSecondaryObject(iterator, 1) == 0x30007u && iterator == 13, "secondary + shared iterator");
    expectTrace("o:secondary3,s:info1", "secondary then its information");
    iterator = -1;
    check(info.getFirstSecondaryObject(iterator, 0) == 0x10003u && iterator == 41, "first secondary + iterator");
    expectTrace("o:firstSecondary-1,s:info0", "first secondary then its information");
    owner.m_secondary = nullptr; iterator = 5;
    check(info.getSecondaryObject(iterator, 1) == 0xFFFFFFFFu && iterator == 15, "no secondary answers -1");
    expectTrace("o:secondary5", "null secondary asks nothing more");
    iterator = 7;
    check(info.getFirstSecondaryObject(iterator, 2) == 0xFFFFFFFFu && iterator == 41, "no first secondary answers -1");
    expectTrace("o:firstSecondary7", "null first secondary asks nothing more");
}

static void spawner()
{
    ObservedObject owner("o"), spawnerObject("sp");
    NativeInformation info(&owner);
    owner.m_spawner = &spawnerObject; spawnerObject.m_handles[2] = 0x40009u;
    check(info.getObjectSpawner(2) == 0x40009u, "spawner's information on the side");
    expectTrace("o:spawner,sp:info2", "spawner then its information");
    owner.m_spawner = nullptr;
    check(info.getObjectSpawner(1) == 0xFFFFFFFFu, "no spawner answers -1");
    expectTrace("o:spawner", "null spawner asks nothing more");
}

static void handle()
{
    InformationManager manager;
    informationManager = &manager;
    ObservedObject owner("o");
    NativeInformation a(&owner), b(&owner);
    const unsigned int ha = manager.getPool()->acquire(&a);
    const unsigned int hb = manager.getPool()->acquire(&b);
    owner.m_handles[0] = ha; owner.m_handles[1] = hb; owner.m_handles[2] = ha;
    check(a.getHandle() == ha, "side 2 resolves to this record");
    expectTrace("o:info1,o:info2", "sides 1 and 2 only - side 0 is never asked");
    check(b.getHandle() == hb, "side 1 resolves first");
    expectTrace("o:info1", "the walk stops at the first match");
    owner.m_handles[1] = 0; owner.m_handles[2] = hb ^ 0x10000u;
    check(b.getHandle() == 0xFFFFFFFFu, "zero and stale handles never match");
    expectTrace("o:info1,o:info2", "both sides asked before -1");
    owner.m_handles[0] = ha; owner.m_handles[1] = 0; owner.m_handles[2] = 0;
    check(a.getHandle() == 0xFFFFFFFFu, "a neutral-only record answers -1");
    expectTrace("o:info1,o:info2", "neutral side still never asked");
    informationManager = nullptr;
}

static void setters()
{
    ObservedObject owner("o");
    NativeInformation info(&owner);
    check(!info.hasArmament() && !info.hasMobile() && !info.getPhysical() && !info.getUnit() &&
          !info.cover() && !info.controlInfo(), "constructed with no plug-in");
    const Six six = install(info, false);
    ArmamentObserver* arm2 = new ArmamentObserver("arm2");
    ControlObserver* control2 = new ControlObserver; CoverObserver* cover2 = new CoverObserver;
    MobileObserver* mobile2 = new MobileObserver; PhysicalObserver* physical2 = new PhysicalObserver(false);
    IPIUnit* unit2 = makeUnit(&info);
    check(!info.setArmament(arm2) && !info.setControlInfo(control2) && !info.setCover(cover2) &&
          !info.setMobile(mobile2) && !info.setPhysical(physical2) && !info.setUnit(unit2),
          "every occupied setter refuses");
    check(info.armament() == six.armament && info.controlInfo() == six.control && info.cover() == six.cover &&
          info.getMobile() == six.mobile && info.getPhysical() == six.physical && info.getUnit() == six.unit,
          "FIRST WINS - the installed plug-ins stay");
    expectTrace("", "setters call nothing and free nothing");
    delete arm2; delete control2; delete cover2; delete mobile2; delete physical2; delete unit2;
    expectTrace("~arm2,~control,~cover,~mobile,~physical,~unit", "refused plug-ins are still the caller's");
}

static void removal()
{
    ObservedObject owner("o");
    NativeInformation info(&owner);
    install(info, false);
    calls.clear();
    check(!info.removePlugIn(6) && !info.removePlugIn(-1), "out-of-range types answer false");
    expectTrace("", "out-of-range types touch nothing");
    check(info.removePlugIn(0) && !info.getPhysical(), "type 0 is the physical plug-in");
    expectTrace("~physical", "type 0 destroys the physical plug-in");
    check(!info.removePlugIn(0), "an empty type answers false"); expectTrace("", "an empty type destroys nothing");
    check(info.removePlugIn(1) && !info.cover(), "type 1 is the cover plug-in"); expectTrace("~cover", "type 1");
    check(info.removePlugIn(2) && !info.hasMobile(), "type 2 is the mobile plug-in"); expectTrace("~mobile", "type 2");
    check(info.removePlugIn(3) && !info.getUnit(), "type 3 is the unit plug-in"); expectTrace("~unit", "type 3");
    check(info.removePlugIn(4) && !info.hasArmament(), "type 4 is the armament plug-in"); expectTrace("~arm1", "type 4");
    check(info.removePlugIn(5) && !info.controlInfo(), "type 5 is the control-info plug-in"); expectTrace("~control", "type 5");
    for (int type = 0; type < 6; ++type) check(!info.removePlugIn(type), "every type now empty");
    expectTrace("", "nothing left to destroy");
}

static void destroy()
{
    ObservedObject owner("o");
    NativeInformation* info = new NativeInformation(&owner);
    install(*info, false);
    calls.clear();
    delete info;
    expectTrace("~physical,~cover,~mobile,~arm1,~unit,~control",
                "the destructor frees physical, cover, mobile, armament, unit, control info");
}

static void borrow()
{
    ObservedObject owner("o");
    NativeInformation lender(&owner), borrower(&owner);
    ArmamentObserver* lent = new ArmamentObserver("lent");
    ArmamentObserver* own = new ArmamentObserver("own");
    check(lender.setArmament(lent) && borrower.setArmament(own), "both armed");
    borrower.borrowArmament(&lender);
    check(borrower.armament() == lent && lender.armament() == lent, "the pointer is SHARED, not copied");
    expectTrace("", "borrowing frees nothing - the borrower's own plug-in is dropped, not destroyed");
    borrower.resetArmament();
    check(!borrower.hasArmament() && lender.armament() == lent, "reset clears only the borrower");
    expectTrace("", "reset frees nothing");
    delete own;
    expectTrace("~own", "the dropped plug-in was still alive");
}

static void settime(bool borrows, bool manDown, const char* expected)
{
    ObservedObject owner("o");
    NativeInformation info(&owner);
    install(info, manDown);
    unitBorrows = borrows;
    calls.clear();
    info.setTimeOnPlugIns(kTime);
    std::string want = expected;
    for (std::string::size_type at; (at = want.find('T')) != std::string::npos;) want.replace(at, 1, timeText(kTime));
    expectTrace(want.c_str(), "setTimeOnPlugIns order and gates");
}

static void settimePlain()
{
    settime(false, false, "o:unit,unit:borrow?,physical:mandown?,arm1:T,control:T,cover:T,mobile:T,physical:T,unit:T");
    // No unit and no physical plug-in: the armament is timed without asking anyone.
    ObservedObject owner("o");
    NativeInformation info(&owner);
    ArmamentObserver* armament = new ArmamentObserver("arm1");
    CoverObserver* cover = new CoverObserver;
    check(info.setArmament(armament) && info.setCover(cover), "armament and cover only");
    calls.clear();
    info.setTimeOnPlugIns(kTime);
    const std::string want = "arm1:" + timeText(kTime) + ",cover:" + timeText(kTime);
    expectTrace(want.c_str(), "absent unit and physical gate nothing");
}

static void booking()
{
    ObservedObject owner("o");
    NativeInformation info(&owner);
    check(!info.isBooked() && info.getBookingPlayerId() == -1, "constructed unbooked, player -1");
    check(info.isBookedBy(-1), "an unbooked record is 'booked by' player -1 (the id alone is compared)");
    info.bookVehicle(9);
    check(info.isBooked() && info.getBookingPlayerId() == 9 && info.isBookedBy(9) && !info.isBookedBy(-1), "booked by 9");
    info.bookVehicle(4);
    check(info.isBooked() && info.getBookingPlayerId() == 4 && !info.isBookedBy(9), "a second booking overwrites");
    info.cancelBookVehicle();
    check(!info.isBooked() && info.getBookingPlayerId() == -1 && info.isBookedBy(-1), "cancel restores the constructed state");
    expectTrace("", "booking never asks the owner");
}

static void sort()
{
    ObservedObject owner("o");
    NativeInformation info(&owner);
    check(info.getInformationSort() == 3, "the base answers 3");
    check(info.time() == 12.0, "the constructor's time");
    expectTrace("", "sort asks nothing");
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    if (argc != 2) return 2;
    const std::string mode = argv[1];
    if (mode == "forwards") forwards();
    else if (mode == "primary") primary();
    else if (mode == "secondary") secondary();
    else if (mode == "spawner") spawner();
    else if (mode == "handle") handle();
    else if (mode == "setters") setters();
    else if (mode == "remove") removal();
    else if (mode == "destroy") destroy();
    else if (mode == "borrow") borrow();
    else if (mode == "settime_plain") settimePlain();
    else if (mode == "settime_borrow") settime(true, false, "o:unit,unit:borrow?,control:T,cover:T,mobile:T,physical:T,unit:T");
    else if (mode == "settime_mandown") settime(false, true, "o:unit,unit:borrow?,physical:mandown?,control:T,cover:T,mobile:T,physical:T,unit:T");
    else if (mode == "booking") booking();
    else if (mode == "sort") sort();
    else return 2;
    std::printf("PASS %s checks=%d\n", mode.c_str(), checks);
    std::fflush(stdout);
    return 0;
}
