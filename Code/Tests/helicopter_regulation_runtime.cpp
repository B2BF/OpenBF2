// [TEMP] [CHOSEN, not recovered] Full-engine-object console/regulation regression.
// The class registry exposes real factories. No controller, template, physics, or console body
// is replaced. This process sets controlled physics state; it never starts a server or flight.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/ConsoleObjectMacro.h"
#include "../BF2/Core/ConsoleObjects.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/Modules.h"
#include "../BF2/Game/Objects/PlayerControlObjectTemplate.h"
#include "../BF2/Game/Objects/Game/PlayerControlObject.h"
#include "../BF2/Game/Objects/ObjectLayoutAccess.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/IO/Input/PlayerInput.h"
#include "../BF2/Physics/BasicPhysicsSystem.h"
#include "../BF2/Physics/PhysicsNode.h"
#include "../BF2/Physics/PhysicsNodeManager.h"
#include "../BF2/Physics/ResponsePhysicsManager.h"
#include "../BF2/Sound/BfSound.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

namespace dice { namespace hfe { extern ClassManager* classManager;
namespace world {
extern ResponsePhysicsManager* responsePhysicsManager;
extern PhysicsNodeManager* physicsNodeManager;
} } }
using namespace dice::hfe;
using namespace dice::hfe::world;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
    std::fflush(stdout);
}
bool near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
class FixtureClasses : public ClassManager
{
public:
    std::vector<ClassRegInfo> entries;
    FixtureClasses()
    {
        GameLogicObjTemplBFModule().getClassList(entries);
        GameLogicObjTemplBaseModule().getClassList(entries);
        ObjTemplBaseModule().getClassList(entries);
        BfSoundModule().getClassList(entries);
    }
    IBase* createInstance(unsigned int cid, unsigned int iid, IBase* outer) override
    {
        for (const auto& entry : entries)
            if (entry.classId == cid) return entry.create(iid, outer);
        std::fprintf(stderr, "Unexpected fixture factory request: %u\n", cid);
        std::abort();
    }
    bool getClassName(unsigned int cid, std::string& name) override
    {
        for (const auto& entry : entries)
            if (entry.classId == cid) { name = entry.name; return true; }
        std::abort();
    }
};
ConsoleObject* binding(const char* name)
{
    ConsoleObject* found = 0;
    for (ConsoleObject* entry : consoleBindingList("world.objTemplGame"))
        if (std::strcmp(entry->getFunctionName(), name) == 0)
        {
            check(!found, std::string(name) + " has only one registration");
            found = entry;
        }
    return found;
}
std::string command(const std::string& text)
{
    std::string result;
    io::mainConsole->handleCommand(text, result, static_cast<AccessType>(7));
    std::printf("COMMAND %s => %s\n", text.c_str(), result.c_str());
    return result;
}
void set(const char* name, const char* value)
{
    check(command(std::string("ObjectTemplate.") + name + " " + value).empty(),
          std::string(name) + " command accepted");
}
void query(const char* name, const char* expected)
{
    check(command(std::string("ObjectTemplate.") + name) == expected,
          std::string(name) + " query round trip");
}
io::PlayerInput sample(float throttle, float yaw = 0)
{
    io::PlayerInput value = {};
    value.mappingFlags = ~0ULL;
    value.inputs[io::PIThrottle] = throttle;
    value.inputs[io::PIYaw] = yaw;
    return value;
}
Mat4 bank(float degrees)
{
    Mat4 result = Mat4::getIdentity();
    const float radians = degrees * 6.2831855f / 360.0f;
    result.m[0] = result.m[5] = std::cos(radians);
    result.m[1] = std::sin(radians);
    result.m[4] = -result.m[1];
    return result;
}
}

int main()
{
    eventManager = new EventManager();
    responsePhysicsManager = new ResponsePhysicsManager();
    physicsNodeManager = new PhysicsNodeManager();
    classManager = new FixtureClasses();
    g_bfSound = new BfSoundNull();
    io::mainConsole = new io::OldConsole();
    objectTemplateManager = new ObjectTemplateManager();
    auto* physics = new BasicPhysicsSystem();
    physics->setGravity(-10.0f);
    basicPhysicsSystem = physics;
    mainConsoleObjects->registerConsoleObjects(consoleBindingList("world.objTemplGame"));
    const char* names[] = {"regulateYawInput", "regulatePitchInput", "regulateRollInput",
        "regulateVerticalPosInput", "regulateYaw", "regulatePitch", "regulateRoll",
        "regulateVerticalPos", "maxVertRegAngle", "noVertRegAngle"};
    std::vector<ConsoleObject*> properties;
    for (int i = 0; i != 10; ++i)
    {
        ConsoleObject* p = binding(names[i]);
        properties.push_back(p);
        check(p != 0, std::string(names[i]) + " is registered");
        if (!p) continue; // Keep baseline running to demonstrate actual controller behavior too.
        const char* type = i < 4 ? "io::PlayerInputMap" : i < 8 ? "Vec2" : "float";
        check(p->getType() == 1 && p->getMinNoArgs() == 0 && p->getMaxNoArgs() == 1
              && p->isDynamic() && p->hasRange() == (i < 4), "stock property metadata");
        check(std::strcmp(p->getReturnType(), type) == 0
              && std::strcmp(p->getArgType(0), type) == 0, "stock parser/result type");
        check(!p->isActive(), "no active template refuses property");
        if (i < 4)
        {
            const auto range = p->getRange();
            check(*static_cast<const int*>(range.first) == 0
                  && *static_cast<const int*>(range.second) == 64, "input range includes PINone");
        }
    }
    auto* templ = static_cast<PlayerControlObjectTemplate*>(objectTemplateManager->createTemplate(
        CID_PlayerControlObjectTemplate, "helicopter_regulation_fixture"));
    check(templ != 0, "real registered template factory");
    if (!templ) return 1;
    auto* vehicle = new PlayerControlObject(templ);
    auto* node = new PhysicsNode();
    node->setObject(vehicle);
    object_detail::LayoutAccess::physicsNode(vehicle) = static_cast<IPhysicsNode*>(node);
    node->setPositionalSpeed(Vec3{0, -0.6f, 0});
    node->setPositionalAcceleration(Vec3{0, -10, 0});
    node->setRotationalSpeed(Vec3{0, 0.5f, 0});
    io::PlayerInput disabled = sample(0);
    vehicle->regulateInput(disabled, 0.1f);
    check(disabled.inputs[io::PIThrottle] == 0 && disabled.inputs[io::PIYaw] == 0,
          "constructor PINone mappings leave input unchanged");

    // The four lines are authored identically by stock AH1Z and UH60 templates.
    set("regulateYawInput", "PIYaw");
    set("regulateYaw", "-0.2/0.2");
    set("regulateVerticalPosInput", "PIThrottle");
    set("regulateVerticalPos", "-0.5/2");
    query("regulateYawInput", "PIYaw");
    query("regulateYaw", "-0.2/0.2");
    query("regulateVerticalPosInput", "PIThrottle");
    query("regulateVerticalPos", "-0.5/2");
    query("maxVertRegAngle", "10");
    query("noVertRegAngle", "20");
    io::PlayerInput idle = sample(0), descend = sample(-1), ascend = sample(1);
    vehicle->regulateInput(idle, 0.1f);
    vehicle->regulateInput(descend, 0.1f);
    vehicle->regulateInput(ascend, 0.1f);
    std::printf("CONTROL falling state: idle throttle=%g, S=%g, W=%g, yaw=%g\n",
        idle.inputs[io::PIThrottle], descend.inputs[io::PIThrottle],
        ascend.inputs[io::PIThrottle], idle.inputs[io::PIYaw]);
    check(near(idle.inputs[io::PIThrottle], 0.3f), "neutral falling input receives lift correction");
    check(descend.inputs[io::PIThrottle] == -1 && ascend.inputs[io::PIThrottle] == 1,
          "explicit descent/ascent inputs remain distinct from neutral correction");
    check(near(idle.inputs[io::PIYaw], -0.3f), "stock yaw gains damp angular motion");

    node->setPositionalSpeed(Vec3{0, -9, 0});
    idle = sample(0);
    vehicle->regulateInput(idle, 0.1f);
    check(idle.inputs[io::PIThrottle] == 1, "large correction saturates at unit input");
    node->setPositionalSpeed(Vec3{0, 0.6f, 0});
    idle = sample(0);
    vehicle->regulateInput(idle, 0.1f);
    check(near(idle.inputs[io::PIThrottle], -0.3f), "upward motion gets opposite neutral correction");
    idle = sample(0);
    idle.setInputIsMapped(io::PIThrottle, false);
    vehicle->regulateInput(idle, 0.1f);
    check(idle.inputs[io::PIThrottle] == 0, "unmapped control ignores regulator writes");

    set("regulatePitchInput", "PIPitch");
    set("regulateRollInput", "PIRoll");
    set("regulatePitch", "-0.1/0.2");
    set("regulateRoll", "-0.3/0.2");
    node->setRotationalSpeed(Vec3{0.5f, 0.5f, 0.5f});
    idle = sample(0);
    vehicle->regulateInput(idle, 0.1f);
    check(near(idle.inputs[io::PIPitch], -0.15f) && near(idle.inputs[io::PIRoll], -0.45f),
          "pitch and roll properties reach the existing controller");
    // Stock helicopters override the constructor's 10/20-degree defaults with 35/55.
    set("maxVertRegAngle", "35");
    set("noVertRegAngle", "55");
    query("maxVertRegAngle", "35");
    query("noVertRegAngle", "55");
    node->setPositionalSpeed(Vec3{0, -0.6f, 0});
    vehicle->setAbsoluteTransformation(bank(30));
    idle = sample(0);
    vehicle->regulateInput(idle, 0.1f);
    check(near(idle.inputs[io::PIThrottle], 0.3f), "stock threshold retains full correction below 35 degrees");
    vehicle->setAbsoluteTransformation(bank(45));
    idle = sample(0);
    vehicle->regulateInput(idle, 0.1f);
    const float faded = 0.3f * (bank(45).m[5] - bank(55).m[5])
                        / (bank(35).m[5] - bank(55).m[5]);
    check(near(idle.inputs[io::PIThrottle], faded), "correction fades between stock attitude thresholds");
    vehicle->setAbsoluteTransformation(bank(60));
    idle = sample(0);
    vehicle->regulateInput(idle, 0.1f);
    check(idle.inputs[io::PIThrottle] == 0, "correction disabled beyond the stock no-regulation angle");
    vehicle->setAbsoluteTransformation(Mat4::getIdentity());
    set("regulateVerticalPosInput", "PINone");
    query("regulateVerticalPosInput", "PINone");
    idle = sample(0);
    vehicle->regulateInput(idle, 0.1f);
    check(idle.inputs[io::PIThrottle] == 0, "PINone disables vertical regulation again");
    check(command("ObjectTemplate.regulateYaw 1/2 3/4") == "Properties only takes one argument!",
          "property rejects extra arguments");

    objectTemplateManager->createTemplate(CID_SimpleObjectTemplate, "wrong_regulation_template");
    for (int i = 0; i != 10; ++i)
    {
        check(properties[i] && !properties[i]->isActive(), "unrelated template refuses property");
        check(command(std::string("ObjectTemplate.") + names[i] + " 0")
              == "Unknown object or method!", "wrong-template command rejected");
    }
    objectTemplateManager->setActiveObjectTemplate(templ);
    query("regulateYaw", "-0.2/0.2");
    std::printf("helicopter regulation fixture: %d checks, %d failures\n", checks, failures);
    // Test-owned objects/services end with this process; this fixture does not prove lifecycle or flight.
    return failures ? 1 : 0;
}
