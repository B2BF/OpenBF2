// AI module registration and lifecycle entry points.
// [INFERRED] Source placement.

#include "AIModule.h"

#include <ios>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include "../../../../Core/ClassManager.h"
#include "../../../../Core/ConsoleObjects.h"
#include "../../../../Core/DllModule.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/ConsoleObjectBase.h"
#include "../../../../Core/ConsoleObjectMacro.h"   // BF2_CC_HEAD_NAMED, BF2_CC_ARG, return/range hooks
#include "../AIUtils/checkPointer.h"
#include "BF2AI.h"
#include "../AIPathFinding/Material/Material.h"
#include "../AIStrategic/Strategy.h"
#include "../AIStrategic/StrategyCondition.h"
#include "../AIStrategicEntity/AILandingZone.h"
#include "../AIStrategicEntity/AILandingZoneManager.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIStrategicEntity/CoverPosition/CoverPositionManager.h"
#include "../AIStrategicEntity/GuardPosition.h"
#include "../AIStrategicEntity/PatrolPath.h"
#include "../AIStrategicMap/AerialHeightMap.h"
#include "../DrawPrimitives/DPManager.h"
#include "AIConsole.h"
#include "AILODManager.h"
#include "AIMain.h"
#include "AIRandom.h"
#include "AISettings.h"
#include "AISettingsInterfaces.h"
#include "AIStreamOperators.h"
#include "BotBehaviourManager.h"
#include "BotPlacementManager.h"
#include "CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "IAIPathfinding.h"
#include "IBotManager.h"
#include "StrategyManager.h"
#include "UserOrderManager.h"

namespace dice {
namespace hfe {

// Core/Globals.cpp declares these in the engine; in a module image they are this file's own copies
// filled by initDll. No header declares them, so every consumer spells the extern itself.
extern ClassManager*   classManager;
extern ConsoleObjects* mainConsoleObjects;

// static init: ELF 0x486170 / 0x4861D0. The module pointer is initialized before the console list.
IModule* dllmodules[] =
{
    new ai::AIModule,
    0
};

// [INFERRED] The two unsigned spellings the bindings' type strings use ("hfe::uint", "u32"); both
// are extracted by istream::operator>>(unsigned int&) in the image.
typedef unsigned int uint;
typedef unsigned int u32;

namespace ai {

// The receivers of the generated console bindings (ELF 0x3EE940..0x3EEB40): each returns the
// active object of its manager, or null while the manager does not exist.

// ELF 0x3EE940
PatrolPath* getActivePatrolPathIfManager()
{
    PatrolPath* path = 0;
    if (UserOrderManager::m_instance)
        path = UserOrderManager::m_instance->getActivePatrolPath();
    return path;
}

// ELF 0x3EE980
GuardPosition* getActiveGuardPositionIfManager()
{
    GuardPosition* position = 0;
    if (UserOrderManager::m_instance)
        position = UserOrderManager::m_instance->getActiveGuardPosition();
    return position;
}

// ELF 0x3EE9C0: IAIPathfindingSetup +0x50.
IAIPathfindingSetup::IMapContainer* getActiveMapContainer()
{
    if (!IAIPathfindingSetup::instance)
        return 0;
    return IAIPathfindingSetup::instance->getActiveMapContainer();
}

// ELF 0x3EEA00: getInstance is called twice, once for the test and once for the call.
Strategy* getActiveStrategy()
{
    Strategy* strategy = 0;
    if (StrategyManager::getInstance())
        strategy = StrategyManager::getInstance()->getActiveStrategy();
    return strategy;
}

// ELF 0x3EEA40. The executors call AIMain members on the result non-virtually, so the helper's
// type is AIMain* [INFERRED]; IAIMain is AIMain's only base, the conversion moves no pointer.
AIMain* getActiveAIMain()
{
    AIMain* aiMain = 0;
    if (AIConsole::m_instance)
        aiMain = static_cast<AIMain*>(AIConsole::m_instance->getActiveAIMain());
    return aiMain;
}

// ELF 0x3EEA80
StrategyCondition* getActiveCondition()
{
    StrategyCondition* condition = 0;
    if (StrategyManager::getInstance())
        condition = StrategyManager::getInstance()->getActiveCondition();
    return condition;
}

// ELF 0x3EEAC0
AIStrategicArea* getActiveStrategicArea()
{
    AIStrategicArea* area = 0;
    if (AIStrategicObjectManager::getInstance())
        area = AIStrategicObjectManager::getInstance()->getActiveArea();
    return area;
}

// ELF 0x3EEB00
AILandingZone* getActiveLandingZone()
{
    AILandingZone* zone = 0;
    if (AILandingZoneManager::m_instance)
        zone = AILandingZoneManager::m_instance->getActiveZone();
    return zone;
}

// ELF 0x3EEB40: the settings singleton, unchecked.
IAISettings* getAISettings()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
}

namespace {

// static init: ELF 0x3EEE90, _GLOBAL__I 0x40FA70; stream cleanup __tcf_111 0x40A860.
// [CHOSEN, not recovered] Explicit object retains the two merged TUs' initialization order.
std::ios_base::Init consoleStreamInit;

// The one list every generated binding of this file appends itself to; init registers it.
// static init: ELF 0x3EEE90; list cleanup __tcf_112 0x40A890 releases nodes, not their pointers.
ConsoleObjects::ConsoleObjectList aiList;

#include "AIModuleConsole.inc"

} // anonymous namespace

// Two stores and nothing else - the vptr, and a refcount of one.
// ELF 0x4861F0
AIModule::AIModule()
    : m_refCount(1)
{
}

// The IBase quartet. Textbook, and reproduced because "textbook" is itself a finding - the AI module
// does not do anything unusual with lifetime.

// ELF 0x40FA90
void AIModule::addRef()
{
    ++m_refCount;
}

// ELF 0x40FAA0
int AIModule::getRef()
{
    return m_refCount;
}

// Decrements the reference count and frees this without a destructor at zero.
// Requires a heap-allocated module with no member cleanup.
// ELF 0x40FAB0
int AIModule::release()
{
    const int n = --m_refCount;
    if (n == 0)
        operator delete(this);
    return n;
}

// Returns the interface pointer for IBase, IClassServer, or IModule, testing in that order.
// ELF 0x40FAF0
void* AIModule::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IClassServer)
        return const_cast<AIModule*>(this);

    if (iid == IID_IModule)
        return const_cast<AIModule*>(this);

    return 0;
}

// IClassServer - both empty. See the note in AIModule.h; this is the fact that the AI registers no
// class ids and is therefore reachable only as a simple singleton.

// - a bare `ret`.
// ELF 0x3EEDD0
void AIModule::getClassList(std::vector<ClassRegInfo>& /*out*/)
{
}

// - a bare `ret`.
// ELF 0x3EEDC0
void AIModule::getSingletonList(std::vector<SingletonRegInfo>& /*out*/)
{
}

// Creates the AI singleton and publishes its console bindings.
// ELF 0x3EEB60
bool AIModule::init()
{
    mainConsoleObjects->registerConsoleObjects(aiList);

    BF2AI* ai = new BF2AI;
    // Preserves the explicit allocation check even with throwing operator new.
    checkPointer(ai);

    // Returns true if the allocation diagnostic returns.
    return true;
}

// Unregisters console objects and looks up the AI through getSingleton. The original
// uses the wrong registry for this simple singleton, leaving the object allocated.
// ELF 0x3EECB0
bool AIModule::close()
{
    mainConsoleObjects->unregisterConsoleObjects(aiList);

    // getSingleton (+0x4C) searches m_normalSingletons. "AI" was registered into
    // m_simpleSingletons. This returns null on every run.
    IBase* ai = classManager->getSingleton("AI");

    // The unregister does hit the right map, so the name is correctly released either way.
    classManager->unregisterSimpleSingleton("AI");

    if (ai)
    {
        // shipped defect: the normal-registry result is treated as IAI despite its IBase type.
        delete reinterpret_cast<IAI*>(ai);
    }

    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
