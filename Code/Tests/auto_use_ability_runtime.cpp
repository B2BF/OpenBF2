// [TEMP] [CHOSEN, not recovered] Isolated console/property regression.
// Links the real dedicated engine objects but never enters BF2::main or starts a server.
// The class-manager adapter exposes actual module factories; it does not replace templates,
// console parsing, binding bodies, or PlayerControlObject::getAutoUseAbility.
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
#include "../BF2/IO/Console/Console.h"
#include "../BF2/Physics/ResponsePhysicsManager.h"
#include "../BF2/Sound/BfSound.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

namespace dice { namespace hfe { extern ClassManager* classManager;
namespace world { extern ResponsePhysicsManager* responsePhysicsManager; } } }
using namespace dice::hfe;
using namespace dice::hfe::world;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    std::fflush(stdout);
}
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
        std::fprintf(stderr, "Unexpected fixture class-name request: %u\n", cid);
        std::abort();
    }
};
ConsoleObject* binding()
{
    ConsoleObject* found = 0;
    for (ConsoleObject* entry : consoleBindingList("world.objTemplGame"))
        if (std::strcmp(entry->getFunctionName(), "autoUseAbility") == 0)
        {
            check(!found, "autoUseAbility has only one module registration");
            found = entry;
        }
    return found;
}
std::string command(const std::string& text)
{
    std::string result;
    io::mainConsole->handleCommand(text, result, static_cast<AccessType>(7));
    return result;
}
}

int main()
{
    ConsoleObject* property = binding();
    check(property != 0, "actual game-template module publishes autoUseAbility");
    if (!property) return 1; // Saved pre-fix module must fail here, before constructing a runtime object.
    check(property->getType() == 1 && property->getMinNoArgs() == 0
          && property->getMaxNoArgs() == 1 && property->isDynamic() && !property->hasRange(),
          "property accepts zero/one arguments dynamically without a range");
    check(std::strcmp(property->getReturnType(), "int") == 0
          && std::strcmp(property->getArgType(0), "int") == 0,
          "property uses the stock integer parser and result type");

    eventManager = new EventManager();
    responsePhysicsManager = new ResponsePhysicsManager();
    classManager = new FixtureClasses();
    g_bfSound = new BfSoundNull();
    io::mainConsole = new io::OldConsole();
    objectTemplateManager = new ObjectTemplateManager();
    mainConsoleObjects->registerConsoleObjects(consoleBindingList("world.objTemplGame"));
    check(!property->isActive()
          && !mainConsoleObjects->getConsoleObject("ObjectTemplate", "autoUseAbility"),
          "property is unavailable without an active template");

    IObjectTemplate* created = objectTemplateManager->createTemplate(
        CID_PlayerControlObjectTemplate, "ability_vehicle_fixture");
    check(created && created->queryImplementation(CID_PlayerControlObjectTemplate),
          "actual registered factory constructs a player-control template");
    if (!created) return 1;
    auto* vehicleTemplate = static_cast<PlayerControlObjectTemplate*>(created);
    check(property->isActive()
          && mainConsoleObjects->getConsoleObject("ObjectTemplate", "autoUseAbility") == property,
          "registry resolves the active template property");
    check(command("ObjectTemplate.autoUseAbility") == "0", "real template defaults to disabled");

    auto* vehicle = new PlayerControlObject(vehicleTemplate);
    check(!vehicle->getAutoUseAbility(), "actual runtime getter initially returns disabled");
    const char* values[] = {"0", "1", "2", "-1", "0"};
    const bool expected[] = {false, true, true, true, false};
    for (int i = 0; i != 5; ++i)
    {
        const std::string output = command(std::string("ObjectTemplate.autoUseAbility ") + values[i]);
        check(output.empty(), (std::string("console accepts integer ") + values[i]).c_str());
        check(command("ObjectTemplate.autoUseAbility") == (expected[i] ? "1" : "0"),
              "query normalizes the stored boolean to integer zero/one");
        check(vehicle->getAutoUseAbility() == expected[i],
              "existing runtime object observes the live template setting");
    }
    check(command("ObjectTemplate.autoUseAbility 1 0") == "Properties only takes one argument!"
          && !vehicle->getAutoUseAbility(), "excess arguments are rejected without mutation");

    IObjectTemplate* unrelated = objectTemplateManager->createTemplate(
        CID_SimpleObjectTemplate, "ability_wrong_template_fixture");
    check(unrelated && !property->isActive()
          && !mainConsoleObjects->getConsoleObject("ObjectTemplate", "autoUseAbility"),
          "property rejects an unrelated active template");
    check(command("ObjectTemplate.autoUseAbility 1") == "Unknown object or method!"
          && !vehicle->getAutoUseAbility(), "wrong-template command cannot mutate the vehicle");
    objectTemplateManager->setActiveObjectTemplate(vehicleTemplate);
    check(command("ObjectTemplate.autoUseAbility") == "0", "reactivation retains the prior setting");
    std::printf("autoUseAbility fixture: %d checks, %d failures\n", checks, failures);
    // Test-owned service and object lifetime ends with this process. No lifecycle/gameplay claim.
    return failures ? 1 : 0;
}
