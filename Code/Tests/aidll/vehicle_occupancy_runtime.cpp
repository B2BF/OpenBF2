// [TEMP] Whole production VehicleOccupancy and InformationManager TUs, real SpHandlePool.
// Native Information/IPIUnit lifetimes and owner/occupancy collaborators are chosen only
// for this caller fixture. They do not establish the unrecovered engine forwarding ABI.
#include "../../BF2/AI/AIDLL/AI/AIUtils/VehicleOccupancy.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/Information.h"
#include "../../BF2/AI/AIDLL/AI/Main/InformationManager.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIUnit.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <type_traits>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static int checks;
static std::string trace;
static IPIUnit* constructingUnit;
static std::map<const Information*, IPIUnit**> unitSlots;
struct InformationScript
{
    std::string name;
    std::function<bool()> primaryTest;
    std::function<unsigned(int)> primary;
    std::function<unsigned(int&, int)> first, next;
};
struct UnitScript { std::string name; std::function<bool()> occupied; };
static std::map<const Information*, InformationScript> informationScripts;
static std::map<IPIUnit*, UnitScript> unitScripts;

__declspec(noreturn) static void unexpected(const char* method)
{
    std::fprintf(stderr, "UNEXPECTED %s\n", method);
    std::abort();
}
static void check(bool value, const char* label)
{
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL %s trace=%s\n", label, trace.c_str()); std::exit(2); }
}
static void exact(const char* expected) { check(trace == expected, "exact call order/receivers/arguments"); }
static std::string number(int value) { return std::to_string(value); }

namespace dice { namespace hfe { namespace ai {
// [TEMP] Chosen genuine native storage, no raw objects or header rewriting. A constructor
// may access its actual private fields; the captured typed field pointer permits mutations.
Information::Information(IAIObject* object, double time)
    : m_objectTypeFlags(0), m_strength(0), m_time(time), m_object(object), m_isBooked(false),
      m_bookingPlayerId(-1), m_armament(nullptr), m_controlInfo(nullptr), m_cover(nullptr),
      m_mobile(nullptr), m_physical(nullptr), m_unit(constructingUnit)
{ unitSlots[this] = &m_unit; }
Information::~Information() {} // chosen non-owning fixture lifetime
int Information::getInformationSort() { unexpected("Information::getInformationSort"); }
bool Information::isPrimary() const
{
    auto& script = informationScripts.at(this);
    trace += "is:" + script.name + ";";
    if (!script.primaryTest) unexpected("isPrimary collaborator");
    return script.primaryTest();
}
unsigned int Information::getPrimaryObject(int side)
{
    auto& script = informationScripts.at(this);
    trace += "primary:" + script.name + ":" + number(side) + ";";
    if (!script.primary) unexpected("getPrimaryObject collaborator");
    return script.primary(side);
}
unsigned int Information::getFirstSecondaryObject(int& iterator, int side)
{
    auto& script = informationScripts.at(this);
    trace += "first:" + script.name + ":" + number(side) + ";";
    if (!script.first) unexpected("getFirstSecondaryObject collaborator");
    return script.first(iterator, side); // never read the uninitialized incoming iterator
}
unsigned int Information::getSecondaryObject(int& iterator, int side)
{
    auto& script = informationScripts.at(this);
    trace += "next:" + script.name + ":" + number(iterator) + ":" + number(side) + ";";
    if (!script.next) unexpected("getSecondaryObject collaborator");
    return script.next(iterator, side);
}

IPIUnit::IPIUnit(Information* information)
    : m_battleStrengths(nullptr), m_strategicStrengths(nullptr), m_strengthMod(0), m_ammo(0),
      m_ammoTime(0), m_ownsPointer24(false), m_pointer24(nullptr)
{ m_information = information; }
IPIUnit::~IPIUnit() {} // chosen non-owning fixture lifetime
int IPIUnit::getType() { unexpected("IPIUnit::getType"); }
void IPIUnit::setTime() { unexpected("IPIUnit::setTime()"); }
void IPIUnit::setTime(double) { unexpected("IPIUnit::setTime(double)"); }
bool IPIUnit::isOccupied()
{
    auto& script = unitScripts.at(this);
    trace += "unit:" + script.name + ";";
    if (!script.occupied) unexpected("isOccupied collaborator");
    return script.occupied();
}
} } }

class NativeInformation : public Information
{
    static IAIObject* setup(IPIUnit* unit) { constructingUnit = unit; return nullptr; }
public:
    NativeInformation(const char* name, IPIUnit* unit = nullptr)
        : Information(setup(unit), 0.0)
    {
        auto& script = informationScripts[this];
        script.name = name;
        script.primaryTest = [] { return true; };
    }
    bool setTime(double) override { unexpected("setTime(double)"); }
    bool setTime(double, float) override { unexpected("setTime(double,float)"); }
    double getTimeStamp() const override { unexpected("getTimeStamp"); }
    void addContact() override { unexpected("addContact"); }
    void removeContact() override { unexpected("removeContact"); }
    void removeAllContacts() override { unexpected("removeAllContacts"); }
    bool hasContact() const override { unexpected("hasContact"); }
    float getSecurity(double) override { unexpected("getSecurity(double)"); }
    float getSecurity() override { unexpected("getSecurity"); }
    Vec3& getPosition() override { unexpected("getPosition"); }
    float getAltitude() override { unexpected("getAltitude"); }
    float getAltitude(const Vec3&) override { unexpected("getAltitude(Vec3)"); }
    Mat4& getTransformation() override { unexpected("getTransformation"); }
    Vec3 getFaceing() override { unexpected("getFaceing"); }
    int getSide() const override { unexpected("getSide"); }
};

int main(int argc, char** argv)
{
    static_assert(sizeof(void*) == 4, "MSVC x86 fixture only");
    static_assert(std::is_same<decltype(&vehicleOccupied), bool (*)(unsigned int, int)>::value, "typed free helper");
    if (argc != 2) return 3;
    const char* mode = argv[1];
    InformationManager managerA, managerB;
    informationManager = &managerA;
    IPIUnit freeUnit(nullptr), occupiedUnit(nullptr);
    unitScripts[&freeUnit] = { "free", [] { return false; } };
    unitScripts[&occupiedUnit] = { "occupied", [] { return true; } };
    NativeInformation root("root", &freeUnit), child("child"), other("other", &occupiedUnit);
    auto& rootScript = informationScripts[&root];
    const unsigned rootHandle = managerA.getPool()->acquire(&root);
    const unsigned childHandle = managerA.getPool()->acquire(&child);
    const unsigned otherHandle = managerA.getPool()->acquire(&other);

    if (!std::strcmp(mode, "low_words")) {
        informationManager = nullptr;
        check(!vehicleOccupied(0, 1), "zero handle without manager");
        check(!vehicleOccupied(0x10000u, -7), "nonzero generation zero index");
        check(!vehicleOccupied(0xffff0000u, 0), "maximum generation zero index");
        exact("");
    } else if (!std::strcmp(mode, "invalid_initial")) {
        check(!vehicleOccupied(rootHandle ^ 0x10000u, 1), "generation mismatch");
        check(!vehicleOccupied(0xffffffffu, 1), "maximum index/generation mismatch");
        check(!vehicleOccupied(4, 1), "null value in valid-generation record");
        managerA.getPool()->release(rootHandle);
        check(!vehicleOccupied(rootHandle, 1), "released stale handle");
        exact("");
    } else if (!std::strcmp(mode, "null_primary_unit")) {
        check(!vehicleOccupied(childHandle, 0), "no primary unit ends scan");
        exact("is:child;");
    } else if (!std::strcmp(mode, "occupied_primary")) {
        check(vehicleOccupied(otherHandle, 2), "primary occupation succeeds");
        exact("is:other;unit:occupied;");
    } else if (!std::strcmp(mode, "empty_secondary")) {
        rootScript.first = [](int& iterator, int) { iterator = 17; return 0u; };
        check(!vehicleOccupied(rootHandle, 7), "empty secondary scan");
        exact("is:root;unit:free;first:root:7;");
    } else if (!std::strcmp(mode, "first_zero_null_manager")) {
        rootScript.first = [](int& iterator, int) { iterator = 17; informationManager = nullptr; return 0xffff0000u; };
        check(!vehicleOccupied(rootHandle, 0), "first-secondary pool low-word gate");
        check(informationManager == nullptr, "first callback mutation occurred");
        exact("is:root;unit:free;first:root:0;");
    } else if (!std::strcmp(mode, "next_zero_null_manager")) {
        rootScript.first = [=](int& iterator, int) { iterator = 17; return childHandle; };
        rootScript.next = [](int& iterator, int) { iterator = -5; informationManager = nullptr; return 0x10000u; };
        check(!vehicleOccupied(rootHandle, -1), "subsequent low-word gate");
        check(informationManager == nullptr, "next callback mutation occurred");
        exact("is:root;unit:free;first:root:-1;next:root:17:-1;");
    } else if (!std::strcmp(mode, "resolved_primary")) {
        rootScript.primaryTest = [] { return false; };
        rootScript.primary = [=](int) { return otherHandle; };
        check(vehicleOccupied(rootHandle, -99), "secondary root resolves primary");
        exact("is:root;primary:root:-99;unit:occupied;");
    } else if (!std::strcmp(mode, "fresh_primary_manager")) {
        const unsigned fresh = managerB.getPool()->acquire(&other);
        rootScript.primaryTest = [] { return false; };
        rootScript.primary = [&](int) { informationManager = &managerB; return fresh; };
        check(vehicleOccupied(rootHandle, 3), "primary callback selects fresh manager");
        check(informationManager == &managerB, "primary callback swapped manager");
        exact("is:root;primary:root:3;unit:occupied;");
    } else if (!std::strcmp(mode, "fresh_first_manager")) {
        const unsigned fresh = managerB.getPool()->acquire(&other);
        rootScript.first = [&](int& iterator, int) { iterator = 17; informationManager = &managerB; return fresh; };
        check(vehicleOccupied(rootHandle, 9), "first callback selects fresh manager");
        check(informationManager == &managerB, "first callback swapped manager");
        exact("is:root;unit:free;first:root:9;unit:occupied;");
    } else if (!std::strcmp(mode, "fresh_next_manager")) {
        const unsigned fresh = managerB.getPool()->acquire(&other);
        rootScript.first = [=](int& iterator, int) { iterator = 17; return childHandle; };
        rootScript.next = [&](int& iterator, int) { iterator = -23; informationManager = &managerB; return fresh; };
        check(vehicleOccupied(rootHandle, 12), "next callback selects fresh manager");
        check(informationManager == &managerB, "next callback swapped manager");
        exact("is:root;unit:free;first:root:12;next:root:17:12;unit:occupied;");
    } else if (!std::strcmp(mode, "secondary_walk")) {
        NativeInformation freeChild("freeChild", &freeUnit);
        const unsigned freeHandle = managerA.getPool()->acquire(&freeChild);
        rootScript.first = [=](int& iterator, int) { iterator = 17; return childHandle; };
        int calls = 0;
        rootScript.next = [&](int& iterator, int) {
            ++calls;
            if (calls == 1) { iterator = -23; return freeHandle; }
            iterator = 1234567; return otherHandle;
        };
        check(vehicleOccupied(rootHandle, (-2147483647 - 1)), "null and free secondary units skipped");
        check(calls == 2, "two next callbacks on retained primary");
        exact("is:root;unit:free;first:root:-2147483648;next:root:17:-2147483648;unit:free;next:root:-23:-2147483648;unit:occupied;");
    } else if (!std::strcmp(mode, "secondary_generation_end")) {
        managerA.getPool()->release(otherHandle);
        rootScript.first = [=](int& iterator, int) { iterator = -31; return childHandle; };
        rootScript.next = [=](int& iterator, int) { iterator = 7; return otherHandle; };
        check(!vehicleOccupied(rootHandle, 2147483647), "stale secondary terminates scan");
        exact("is:root;unit:free;first:root:2147483647;next:root:-31:2147483647;");
    } else if (!std::strcmp(mode, "primary_live_unit")) {
        rootScript.primaryTest = [&] { *unitSlots.at(&root) = &occupiedUnit; return true; };
        check(vehicleOccupied(rootHandle, 1), "unit loaded after primary callback");
        check(root.getUnit() == &occupiedUnit, "native field changed");
        exact("is:root;unit:occupied;");
    } else if (!std::strcmp(mode, "retained_primary")) {
        unitScripts[&freeUnit].occupied = [&] { managerA.getPool()->set(rootHandle, &other); return false; };
        rootScript.first = [](int& iterator, int) { iterator = 17; return 0u; };
        check(!vehicleOccupied(rootHandle, 4), "primary retained after callback replaces pool entry");
        check(managerA.getInformation(rootHandle) == &other, "pool replacement occurred");
        exact("is:root;unit:free;first:root:4;");
    } else return 3;
    informationManager = nullptr;
    std::printf("PASS %s %d checks\n", mode, checks);
    return 0;
}
