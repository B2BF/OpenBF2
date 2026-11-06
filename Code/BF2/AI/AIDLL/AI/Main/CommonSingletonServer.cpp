// Refreshes cached interfaces from the singleton registry.
// A refresh after destruction clears pointers to interfaces that have unregistered.
// [INFERRED] Source placement.

#include "CommonSingletonServer.h"

#include <string>

#include "../../../../Core/ClassManager.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// ELF 0x2EC020 / 0x2EC040; engine ELF 0x841BE50 / 0x841BE60.
ISingletonServerUpdater::ISingletonServerUpdater()
{
}

// Shared out-of-line destructor used by engine and DLL updater classes. Keep it before
// update in the interface: update occupies ELF +0x08 and PE +0x04.
// ELF 0x2EC060 / 0x2EC080 / 0x2EC0A0
ISingletonServerUpdater::~ISingletonServerUpdater()
{
}

// definitions. `= 0` is the ELF's own initial state (.bss), not a convention
void* CommonSingletonServer::m_iAimerHelper                        = 0;
void* CommonSingletonServer::m_iBFTimer                            = 0;
void* CommonSingletonServer::m_iaiInformationCourier               = 0;
void* CommonSingletonServer::m_iaiInformationEnvironmentModifiers  = 0;
void* CommonSingletonServer::m_iaiPathfindingCourier               = 0;
void* CommonSingletonServer::m_iaiTerrain                          = 0;
void* CommonSingletonServer::m_iaiGame                             = 0;
void* CommonSingletonServer::m_iaiGameEffector                     = 0;
void* CommonSingletonServer::m_iaiObjectEnvironment                = 0;
void* CommonSingletonServer::m_iaiPhysics                          = 0;
void* CommonSingletonServer::m_iDPManager                          = 0;
void* CommonSingletonServer::m_iDPStaticCreator                    = 0;
void* CommonSingletonServer::m_iDPAIObjectCreator                  = 0;
void* CommonSingletonServer::m_iaiSettings                         = 0;
void* CommonSingletonServer::m_iaiSettingsModifier                 = 0;
void* CommonSingletonServer::m_iAICollisionHandler                 = 0;
void* CommonSingletonServer::m_iBFCollisionHandler                 = 0;
void* CommonSingletonServer::m_iKitTemplateManager                 = 0;
void* CommonSingletonServer::m_iaiPlayerFactory                    = 0;
void* CommonSingletonServer::m_iInformationFactory                 = 0;
void* CommonSingletonServer::m_iMapRenderer                        = 0;
void* CommonSingletonServer::m_iDebugRenderer                      = 0;
void* CommonSingletonServer::m_iBFModuleCourier                    = 0;
void* CommonSingletonServer::m_iBFCommander                        = 0;
void* CommonSingletonServer::m_iBFSquad                            = 0;
void* CommonSingletonServer::m_iBFRadioComm                        = 0;
void* CommonSingletonServer::m_iBFGraphCreator                     = 0;
void* CommonSingletonServer::m_iAIMineManager                      = 0;

// - a bare `ret`. See the header: the base publishes the entry point and does
// nothing with it. AISingletonServer::updateStaticMethods is the one that is actually called.
// ELF 0x2EB570
void CommonSingletonServer::updateStaticMethods()
{
}

// Refreshes all 28 singleton pointers in order. Missing names store null without logging
// or reporting failure; callers must validate the interfaces they require.
// ELF 0x2EB580
void CommonSingletonServer::updateStaticMethodsInternal()
{
    m_iAimerHelper                       = classManager->getSimpleSingleton("AimerHelper");
    m_iBFTimer                           = classManager->getSimpleSingleton("IBFTimer");
    m_iaiInformationCourier              = classManager->getSimpleSingleton("IAIInformationCourier");
    m_iaiInformationEnvironmentModifiers = classManager->getSimpleSingleton("IAIInformationEnvironmentModifiers");
    m_iaiPathfindingCourier              = classManager->getSimpleSingleton("IAIPathfindingCourier");
    m_iaiTerrain                         = classManager->getSimpleSingleton("IAITerrain");
    m_iaiGame                            = classManager->getSimpleSingleton("IAIGame");
    m_iaiGameEffector                    = classManager->getSimpleSingleton("IAIGameEffector");
    m_iaiObjectEnvironment               = classManager->getSimpleSingleton("IAIObjectEnvironment");
    m_iaiPhysics                         = classManager->getSimpleSingleton("IAIPhysics");
    m_iDPManager                         = classManager->getSimpleSingleton("IDPManager");
    m_iDPStaticCreator                   = classManager->getSimpleSingleton("IDPStaticCreator");
    m_iDPAIObjectCreator                 = classManager->getSimpleSingleton("IDPAIObjectCreator");
    m_iaiSettings                        = classManager->getSimpleSingleton("IAISettings");
    m_iaiSettingsModifier                = classManager->getSimpleSingleton("IAISettingsModifier");
    m_iAICollisionHandler                = classManager->getSimpleSingleton("IAICollisionHandler");
    m_iBFCollisionHandler                = classManager->getSimpleSingleton("IBFCollisionHandler");
    m_iKitTemplateManager                = classManager->getSimpleSingleton("IKitTemplateManager");
    m_iaiPlayerFactory                   = classManager->getSimpleSingleton("IAIPlayerFactory");
    m_iInformationFactory                = classManager->getSimpleSingleton("IInformationFactory");
    m_iMapRenderer                       = classManager->getSimpleSingleton("IMapRenderer");
    m_iDebugRenderer                     = classManager->getSimpleSingleton("IDebugRenderer");
    m_iBFModuleCourier                   = classManager->getSimpleSingleton("IBFModuleCourier");
    m_iBFCommander                       = classManager->getSimpleSingleton("IBFCommander");
    m_iBFSquad                           = classManager->getSimpleSingleton("IBFSquad");
    m_iBFRadioComm                       = classManager->getSimpleSingleton("IBFRadioComm");
    m_iBFGraphCreator                    = classManager->getSimpleSingleton("IBFGraphCreator");
    m_iAIMineManager                     = classManager->getSimpleSingleton("IAIMineManager");
}

// The same twenty-eight, same order, all nulled.
// ELF 0x2EB410
void CommonSingletonServer::clearAllStaticMethods()
{
    m_iAimerHelper                       = 0;
    m_iBFTimer                           = 0;
    m_iaiInformationCourier              = 0;
    m_iaiInformationEnvironmentModifiers = 0;
    m_iaiPathfindingCourier              = 0;
    m_iaiTerrain                         = 0;
    m_iaiGame                            = 0;
    m_iaiGameEffector                    = 0;
    m_iaiObjectEnvironment               = 0;
    m_iaiPhysics                         = 0;
    m_iDPManager                         = 0;
    m_iDPStaticCreator                   = 0;
    m_iDPAIObjectCreator                 = 0;
    m_iaiSettings                        = 0;
    m_iaiSettingsModifier                = 0;
    m_iAICollisionHandler                = 0;
    m_iBFCollisionHandler                = 0;
    m_iKitTemplateManager                = 0;
    m_iaiPlayerFactory                   = 0;
    m_iInformationFactory                = 0;
    m_iMapRenderer                       = 0;
    m_iDebugRenderer                     = 0;
    m_iBFModuleCourier                   = 0;
    m_iBFCommander                       = 0;
    m_iBFSquad                           = 0;
    m_iBFRadioComm                       = 0;
    m_iBFGraphCreator                    = 0;
    m_iAIMineManager                     = 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
