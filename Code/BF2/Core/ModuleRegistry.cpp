// Module-owned class and singleton registrations. [INFERRED] Consolidated source placement.
// The root module keeps its own tables in Core/Module.cpp.

#include <list>
#include <map>          // listAt's address-keyed list table - see the note there
#include <sstream>
#include <cstdlib>
#include <cstdio>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace dice { namespace hfe {
class ConsoleObject;                                        // Core/ConsoleObject.h
std::list<ConsoleObject*>& gameConsoleObjects();            // Game/Main/GameConsole.cpp
std::list<ConsoleObject*>& serverSettingsConsoleObjects();  // Game/GameServer/ServerSettingsConsole.cpp
std::list<ConsoleObject*>& mapListConsoleObjects();         // Game/MapListConsole.cpp
std::list<ConsoleObject*>& gameLogicConsoleObjects();       // Game/GameLogic/GameLogicConsole.cpp
std::list<ConsoleObject*>& combatAreaConsoleObjects();      // Game/GameLogic/CombatAreaConsole.cpp
#if !DEDICATED
std::list<ConsoleObject*>& menuTeamConsoleObjects();        // client PE ModuleGame list 0x98913C
std::list<ConsoleObject*>& swiffHostConsoleObjects();       // client gameModes.con binding
std::list<ConsoleObject*>& localProfileConsoleObjects();    // Game/LocalProfileConsole.cpp, the Profile.con object
std::list<ConsoleObject*>& bf2RenderConsoleObjects();       // Game/Main/BF2RenderConsole.cpp, renderer.*
std::list<ConsoleObject*>& settingsConsoleObjects();        // Game/SettingsConsole.cpp, the five profile-file objects
#endif
std::list<ConsoleObject*>& banListConsoleObjects();         // Game/GameServer/BanListConsole.cpp
std::list<ConsoleObject*>& spawnSystemConsoleObjects();     // Game/GameLogic/SpawnSystem/SpawnSystemConsole.cpp
namespace io {
std::list<ConsoleObject*>& fileManagerConsoleObjects();     // IO/File/FileManagerConsole.cpp
std::list<ConsoleObject*>& controlMapConsoleObjects();      // IO/Input/ControlMapConsole.cpp
std::list<ConsoleObject*>& networkConsoleObjects();         // IO/Network/NetworkConsole.cpp
}
namespace world {
std::list<ConsoleObject*>& objectManagerConsoleObjects();   // Scene/Object/ObjectManagerConsole.cpp
std::list<ConsoleObject*>& objTemplBaseConsoleObjects();    // Game/Objects/Base/ModuleWorldObjTemplBase.cpp
std::list<ConsoleObject*>& objTemplGameConsoleObjects();    // Game/Objects/Game/ModuleWorldObjTemplGame.cpp
std::list<ConsoleObject*>& objTemplFxConsoleObjects();      // Game/Objects/Fx/ModuleWorldObjTemplFx.cpp
std::list<ConsoleObject*>& objTemplVehicleConsoleObjects(); // Game/Objects/Vehicle/ModuleWorldObjTemplVehicle.cpp
void addConstantHelper(const std::string& name, unsigned int value);   // world:: twin of io::'s below - ELF 0x8576C50, defined in ModuleWorldObjTemplGame.cpp (its TU key)
std::list<ConsoleObject*>& heightmapConsoleObjects();       // Physics/HeightmapConsole.cpp
std::list<ConsoleObject*>& collisionManagerConsoleObjects();// Physics/CollisionManager.cpp
std::list<ConsoleObject*>& physicsConsoleObjects();         // Physics/BasicPhysicsSystem.cpp
std::list<ConsoleObject*>& gameLogicCompConsoleObjects();   // Game/Objects/GameLogicComp/ModuleGameLogicComp.cpp (RW-31)
std::list<ConsoleObject*>& materialSystemConsoleObjects();  // Game/GameLogic/MaterialSystem/ModuleMaterialSystem.cpp (RW-31)
std::list<ConsoleObject*>& scoreSystemConsoleObjects();     // Game/GameLogic/ScoreSystem/ModuleScoreSystem.cpp
}
namespace tool {
std::list<ConsoleObject*>& toolObjectConsoleObjects();      // Scene/Object/ToolObject/ModuleToolObject.cpp
} } }      // the ostringstream inlined into addConstantHelper/addConstantFloatHelper
#include <string>

#include "Modules.h"
#include "Ids.h"
#include "ConsoleObjects.h"
#include "../IO/Console/Manager.h" // Native type-name manager, recovered
#include "../Scene/Object/PlayerManager.h"   // the real PlayerManager - replaces PlayerManagerLike
#include "../Math/Math.h"    // initMath / closeMath - MathModule::init brackets them
#include "ClassManager.h"    // classManager->createInstance - set{Spawn,Material}Manager use it
#include "../Game/Persistence/PlayerAwards.h"
#include "../Game/Persistence/PlayerStats.h"
#include "../Game/Persistence/PlayerUnlocks.h"
#include "../Game/Persistence/LeaderBoard.h"
#include "../Game/Persistence/SearchForPlayer.h"
#include "../Game/Persistence/PlayerRankInfo.h"
#include "../Game/Persistence/PlayerUnlockInfo.h"
#include "../Game/Persistence/StatsManager.h"
#include "../Game/Persistence/PlayerStatsManager.h"
#include "../Game/GameLogic/ClientGameLogic.h"                    // ClientGameLogic::create
#include "../Game/GameLogic/SquadManager/ClientSquadManager.h"    // ClientSquadManager::create
#if !DEDICATED
#include "../Game/Main/Game.h"            // isDedicated() - the client singleton rows
#include "../Game/Main/BF2ClientSeams.h"  // their slots
#include "../Game/LocalProfileManager.h" // client factory4104FC, native startup owner
#include "../Game/ProfileManager.h" // client factory410382, online-profile lifetime/status owner
#include "../Game/AudioSettings.h"       // client factory4105B9, native settings owner
#include "../Game/GeneralSettings.h"     // client factory4106BE, native settings owner
#include "../Game/GameServerSettings.h"  // client factory41067C, the ServerSettings mirror
#include "../Game/HudInformationLayer.h" // client factory40FECC, native HUD data owner
#include "../Game/VideoSettings.h"       // client factory4105FB, the renderer options owner
#include "../Game/ControlSettings.h"     // client factory41063D, the control-map backup owner
#include "../Game/HapticSettings.h"      // client factory410700, the force-feedback tuning owner
#include "../Game/GlobalSettings.h"      // client factory41057A, the default-user/clan-tag owner
#include "../Game/DemoRecorder/DemoLibrary.h" // client factory410742, the demo library
#include "../Menu/ServerLogoManager.h"   // client factory410784, the server-browser logo cache
#include "../Menu/SwiffHost.h"           // client factory410244, the front-end host
#include "../Menu/BetweenRoundsLogic.h"   // ModuleGame::handleEvent builds it on ECMenu 3 / 9
#include "LoadStat.h"                      // loadStat->executeLoadCallback - the ECMenu 6 arm
#include "../Game/Main/BF2Engine.h"        // menuLogic, the live IMenuLogic global (ds:0x987358)
#include "../Menu/MainMenuLogic.h"       // client factory410286, the Flash menu bridge
#include "../Menu/ServerBrowser.h"       // client factory4101C3, a named GameSpy server list
#include "../Menu/MenuTeamManager.h"     // client factory41053B, team/kit menu data
#include "../Menu/ServerBrowserManager.h" // client factory410205, the lists' owner
#include "../IO/Input/DX8InputDeviceManager.h" // client InputDX8Module factory rows
#include "../IO/Input/InputManager.h"
#endif
#if !DEDICATED
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>                      // LoadLibraryA / GetProcAddress / FreeLibrary - BfSoundModule
#include "../Game/Utility/Vars.h"         // Vars::getInt("opt-nosound") - BfSoundModule::init
#endif
#include "System.h"          // g_system - io::ModuleIO::init calls resetExactTime
#include "../Game/Objects/HudComp/ModuleHudComp.h" // shared strategic bindings, client HUD metadata
#include "../Physics/CollisionManager.h" // real class; avoid a conflicting factory shim
#include "EventManager.h"       // EventManager::create, referenced by dice::hfe::Module's table
#include "Debug.h"              // Registration-failure diagnostics.
#include "../Animation/BoneAnimation/RagDollTemplate.h"
// Animation-system declarations must be included at file scope.
#include "../Animation/BoneAnimation/AnimationSystem.h"
#include "../Animation/BoneAnimation/AnimationManager.h"  // anim::animationManager
#include "../Animation/BoneAnimation/ValueHolder.h"   // anim::valueHolderManager
#include "../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../Scene/Object/ObjectManager.h"       // real ObjectManager::create factory
#include "../Scene/Object/BinTreeCuller.h"       // real spatial culler factories
#include "../Scene/Object/QuadTreeCuller.h"
#include "../Sound/BfSound.h"        // Sound interfaces and engine globals.
#include "../Sound/SoundObjectTemplate.h"  // the hfe-namespace "Sound" template class (9986001)
#include "../IO/File/ZipArchive.h"   // ZipArchiveHandler::create
#include "../IO/File/UnixFileSystem.h"   // UnixFileSystem::create - the POSIX build's file-system row
#include "../Game/IGameLogic.h"      // gameLogic + setGameLogic - defined below
#include "../Game/GameServer/GameServer.h" // real GameServer::create factory
#if !DEDICATED
#include "../Game/GameClient/GameClient.h" // GameClient::create - the client's Game
#endif
#include "../Game/GameLogic/ServerGameLogic.h" // real ServerGameLogic::create factory
#include "../Game/GameLogic/SquadManager/SquadManager.h" // real SquadManager::create factory
#include "SettingsRepostitory.h"           // AIInterfaceModule's aidll setting
#include "../AI/AIInterface/AIInstance.h"
#include "../AI/AIInterface/SingletonServer.h"
#include "../AI/AIInterface/Main/AIConnection.h"
#include "../AI/AIInterface/Module/BFConsole.h"
#include "../AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include "../AI/AIDLL/AI/AIUtils/checkPointer.h"

namespace dice {

// Animation types belong to dice::anim. Skeleton's narrow local declaration covers memory lifecycle only.
namespace anim {
class Skeleton { public: static void initMemory(); static void closeMemory(); };
}

namespace hfe {

// Module dependencies use shared typed declarations; earlier shim signatures were [INFERRED].
class IBase;
// Returns the shared console registry.
inline ConsoleObjects* consoleObjects() { return mainConsoleObjects; }

// Console-constant registration helpers used by ModuleIO.
namespace io {
void addConstantHelper(const std::string& name, unsigned int value);
void addConstantToAllConsoles(const std::string& name, const std::string& value);
void addConstantFloatHelper(const std::string& name, float value);
}


namespace world {
// Use the typed ObjectTemplateManager and PlayerManager interfaces for class registration.
// registerTemplateClass and registerPlayerClass are different slots on different receivers.
extern PlayerManager*         playerManager;            // Ds:0x8B1B928, real type since
extern ObjectTemplateManager* objectTemplateManager;    // ds:0x8B1B924 - real type, header included
}

// Use the same typed global declaration as the owning translation unit.
extern ClassManager* classManager;

// ds:0x8AD3A00, _ZN4dice3hfe12spawnManagerE - dice::hfe scope, not world. See Core/Globals.cpp.
extern IBase* spawnManager;

// Registry targets use typed ClassFactory specializations or class-specific static factories.

// These factory declarations avoid requiring every concrete class definition here.
// Use Core/ClassFactory.h's implementation with complete classes: refuse aggregation, query the requested
// interface and release on mismatch rather than returning an unadjusted object pointer.
template <typename T>
struct ClassFactory { static IBase* create(unsigned int classId, IBase* outer); };

namespace ai    { template <typename T> struct ClassFactory { static IBase* create(unsigned int, IBase*); }; }
namespace world { template <typename T> struct ClassFactory { static IBase* create(unsigned int, IBase*); }; }

#define BF2_FACTORY(name) class name { public: static IBase* create(unsigned int, IBase*); }

// 124 ClassFactory<T> targets  -  names only
class SoundObjectTemplate;
namespace ai { class AIConnection; }
namespace world {
class AbilityGLCompTemplate; class AbilityHudCompTemplate; class AirDraftEffectBundleTemplate;
class AmbientEffectAreaTemplate; class AnimatedBundleTemplate; class AntennaObjectTemplate;
class AreaObjectTemplate; class ArmorGLCompTemplate; class BasicPhysicsSystem; class BundleTemplate;
class CameraTemplate; class ControlPointTemplate; class DecalTemplate; class DefaultAmmoCompTemplate;
class ICollisionManager;
class DefaultAnimationCompTemplate; class DefaultCollisionCompTemplate;
class DefaultDetonationCompTemplate; class DefaultFollowCompTemplate;
class DefaultPenetrateCompTemplate; class DefaultProjectileSoundCompTemplate;
class DefaultRicochetCompTemplate; class DefaultSoundCompTemplate; class DefaultZoomCompTemplate;
class DestroyableObjectTemplate; class DestroyableWindowTemplate;
class DestroyableWindowsBundleTemplate; class DropVehicleTemplate; class DynamicBundleTemplate;
class EffectBundleTemplate; class EmitterTemplate; class EngineTemplate; class EntryPointTemplate;
class EnvMapTemplate; class ExplosivesFireCompTemplate; class FixedTargetCompTemplate;
class FloatingBundleTemplate; class ForceObjectTemplate; class FreeCameraTemplate;
class GenericFireArmTemplate; class GenericProjectileTemplate;
class GrapplingHookRopeContainerTemplate; class GrapplingHookRopeTemplate;
class GroundEffectBundleTemplate; class HelpHudCompTemplate; class HookLinkTemplate;
class IkTemplateComponent; class ItemContainerTemplate; class ItemTemplate; class KitPartTemplate;
class KitTemplate; class LPFireCompTemplate; class LPTargetCompTemplate;
class LadderContainerTemplate; class LadderTemplate; class LandingGearTemplate;
class LightSourceTemplate; class MaskObjectTemplate; class MeshParticleSystemTemplate;
class MultiFireArmTargetCompTemplate; class MultiFireCompTemplate;
class NonScreenAlignedParticleSystemTemplate; class ObjectSpawnerTemplate; class ObjectTemplateManager;
class ObstacleTemplate; class OverheadCameraTemplate; class ParachuteTemplate;
class ParticleSystemEmitterTemplate; class ParticleTemplate; class PhysicsNode;
class PhysicsNodeManager; class PlayerControlObjectTemplate; class PortalManager;
class PostProcessCompTemplate; class PreDestinedFollowCompTemplate; class RadioGLCompTemplate;
class RemoteControlledObjectTemplate; class ReplenishCollisionCompTemplate;
class ReplenishDetonationCompTemplate; class ReplenishingAmmoCompTemplate; class ResponsePhysics;
class ResponsePhysicsManager; class ResurrectCollisionCompTemplate; class RopeLinkTemplate;
class RotationalBundleTemplate; class RotorTemplate; class SeekClosestTargetCompTemplate;
class SimpleDeviationCompTemplate; class SimpleObjectTemplate; class SimpleTargetCompTemplate;
class SingleFireCompTemplate; class SoldierBasedRecoilCompTemplate; class SoldierDeviationCompTemplate;
class SoldierTemplate; class SpawnObjectFireCompTemplate; class SpringTemplate;
class SpriteParticleSystemTemplate; class SpriteParticleTemplate; class StickyCollisionCompTemplate;
class StrategicObjectCompTemplate; class SupplyDepotTemplate; class SupplyObjectTemplate;
class TargetDeviationCompTemplate; class TargetObjectTemplate; class ThrownAnimationCompTemplate;
class ThrownFireCompTemplate; class ToggleCameraCompTemplate; class TrailSystemTemplate;
class TriggerTemplate; class TriggerableTargetTemplate; class TriggerableTemplate;
class TurnableRemoteControlledObjectTemplate; class UAVVehicleTemplate; class VehicleHudCompTemplate;
class WarningHudCompTemplate; class WeaponBasedRecoilCompTemplate; class WeaponHudCompTemplate;
class WheelEffectBundleTemplate; class WindAffectorTemplate; class WingTemplate;
class ZiplineContainerTemplate; class ZiplineRopeTemplate; class ZiplineTemplate;
} // namespace world

// Factory declarations for classes with static create entry points.
BF2_FACTORY(ControlPointNetworkable); BF2_FACTORY(DebugCounterManager);
BF2_FACTORY(DestroyableObjectNetworkable); BF2_FACTORY(DestroyableWindowsBundleNetworkable);
BF2_FACTORY(EngineNetworkable);    BF2_FACTORY(FreeCameraNetworkable);
BF2_FACTORY(GenericProjectileNetworkable);
BF2_FACTORY(ItemNetworkable);      BF2_FACTORY(KitNetworkable);
BF2_FACTORY(MemoryPool);
BF2_FACTORY(ModInfo);              BF2_FACTORY(OverheadCameraNetworkable);
BF2_FACTORY(PlayerControlObjectNetworkable);
BF2_FACTORY(Profiler);
BF2_FACTORY(RotationalBundleNetworkable);
BF2_FACTORY(SimpleObjectNetworkable);
BF2_FACTORY(SoldierNetworkable);   BF2_FACTORY(SpawnManager);
BF2_FACTORY(SpawnPointTemplate);   BF2_FACTORY(SpringNetworkable);
BF2_FACTORY(TargetObjectNetworkable); BF2_FACTORY(TriggerableTargetNetworkable);

namespace io {
BF2_FACTORY(BufferedStream);       BF2_FACTORY(ControlMap);
BF2_FACTORY(ControlMapManager);    BF2_FACTORY(ExcelLexicon);
BF2_FACTORY(FileManager);
BF2_FACTORY(MemStream);            BF2_FACTORY(NetworkableInfoManager);
BF2_FACTORY(NullCacheManager);     BF2_FACTORY(NullInputDeviceManager);
BF2_FACTORY(NullResourceManager);
#if defined(_WIN32)
BF2_FACTORY(Win32FileSystem);      // the Windows half - see the registry row below. Only the Win32
                                   // row names it, so the stand-in is guarded exactly like the row.
#endif
} // namespace io

namespace world {
BF2_FACTORY(Heightmap);            BF2_FACTORY(HeightmapCluster);
BF2_FACTORY(HeightmapClusterEditable); BF2_FACTORY(HeightmapEditable);
BF2_FACTORY(MaterialManager);      BF2_FACTORY(Object);
BF2_FACTORY(ObjectPriorityCalculator);
BF2_FACTORY(ScoreManager);         BF2_FACTORY(WindManager);
} // namespace world


// Registry rows point to live typed global slots, never retail image addresses.
// Keep each declaration consistent with its owning header to avoid duplicate MSVC data symbols.
extern IBase* g_modInfo;                   // ds:0x8B16FC8  _ZN4dice3hfe9g_modInfoE
// (g_bfSound, ds:0x8AC0D80, is declared with its ISoundEngine shim at the top of this file.)

namespace io {
//  IO/Console/Console.h owns this one; the class is forward-declared because that header is not
// included here (this file only needs the global's address).
class OldConsole;

// Already defined elsewhere  -  RE-used at their existing types, not redeclared
extern FileManager*       fileManager;         // ds:0x8AC8C80  Core/Globals.cpp, IO/File/FileManager.h
extern FileChangeManager* fileChangeManager;   // ds:0x8AC8C84  Core/Globals.cpp, IO/File/FileChangeManager.h
extern OldConsole*        mainConsole;         // ds:0x898CBE4  Core/Globals.cpp, IO/Console/Console.h

extern IBase* inputDeviceManager;          // ds:0x8AC8C88  _ZN4dice3hfe2io18inputDeviceManagerE
extern IBase* controlMapManager;           // ds:0x8AC8C8C  _ZN4dice3hfe2io17controlMapManagerE
extern IBase* resourceManager;             // ds:0x8AC8C90  _ZN4dice3hfe2io15resourceManagerE
extern IBase* cacheManager;                // ds:0x8AC8C94  _ZN4dice3hfe2io12cacheManagerE
extern IBase* asyncMultiStreamer;          // ds:0x8AC8C98  _ZN4dice3hfe2io18asyncMultiStreamerE
extern IBase* networkableInfoManager;      // ds:0x8AC8CA0  _ZN4dice3hfe2io22networkableInfoManagerE
extern IBase* inputManager;                // ds:0x8AC8CA4  _ZN4dice3hfe2io12inputManagerE
extern IBase* lexicon;                     // ds:0x8AC8CA8  _ZN4dice3hfe2io7lexiconE
extern IBase* activeMethodConsole;         // ds:0x8AC3C88  _ZN4dice3hfe2io19activeMethodConsoleE
// SocketManager is not registered as a module singleton.
} // namespace io

namespace ai {
extern IBase* g_aiConnection;              // ds:0x8B29AE0  _ZN4dice3hfe2ai14g_aiConnectionE
}

namespace world {
// Already defined elsewhere  -  RE-used at their existing types, not redeclared
// (objectTemplateManager and playerManager are declared further up, with the note on the shim that
//  was resolved for the first of them.)
extern ObjectManager*          objectManager;          // ds:0x8B1B920  Core/Globals.cpp, Scene/Object/ObjectManager.h
extern PhysicsNodeManager*     physicsNodeManager;     // ds:0x8B27844  Core/Globals.cpp
extern ResponsePhysicsManager* responsePhysicsManager; // ds:0x8B27848  Core/Globals.cpp

extern IBase* windManager;                 // ds:0x8B1B92C  _ZN4dice3hfe5world11windManagerE
extern IBase* portalManager;               // ds:0x8B1ED80  _ZN4dice3hfe5world13portalManagerE
extern ICollisionManager* collisionManager; // ds:0x8B27840  _ZN4dice3hfe5world16collisionManagerE
extern IBase* basicPhysicsSystem;          // ds:0x8B2784C  _ZN4dice3hfe5world18basicPhysicsSystemE
extern IBase* heightmapCluster;            // ds:0x8ABAF24  _ZN4dice3hfe5world16heightmapClusterE
extern IBase* materialManager;             // ds:0x8AD2820  _ZN4dice3hfe5world15materialManagerE
extern IBase* scoreManager;                // ds:0x8AD1740  _ZN4dice3hfe5world12scoreManagerE
} // namespace world


// Per-module reference counting and dispatch for IBase, IClassServer and IModule.
// release deletes at zero and returns the saved new count.
#define BF2_MODULE_IBASE(cls)                                                       \
    void  cls::addRef()  { ++m_refCount; }                                          \
    int   cls::getRef()  { return m_refCount; }                                     \
    int   cls::release() { int n = --m_refCount; if (n == 0) delete this; return n; }\
    void* cls::queryInterface(unsigned int iid) const                               \
    {                                                                               \
        if (iid == IID_IBase || iid == IID_IClassServer) return (void*)this;        \
        return (iid == IID_IModule) ? (void*)this : 0;                              \
    }

// dice::hfe

// ModuleBF2
BF2_MODULE_IBASE(ModuleBF2)
void ModuleBF2::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_GameServer, "dice.bf.gameServer", &GameServer::create});
#if !DEDICATED
    // BF2.exe 43587B - the row right after gameServer's in the client's table (sub_435835), absent
    // from both dedicated images. CID 121003 is read from ds:892BF0 there; the factory is 4345D0.
    out.push_back(ClassRegInfo{CID_GameClient, "dice.bf.gameClient", &GameClient::create});
#endif
    //  world::CID_PlayerManager, qualified. ModuleBF2 is a dice::hfe module but this entry
    // registers a world:: class, and Core/Ids.h puts the id in dice::hfe::world alongside it. The
    // extractor emitted the bare name because it reads ids by address, not by scope.
    out.push_back(ClassRegInfo{world::CID_PlayerManager, "dice.hfe.world.PlayerManager.Standard", &world::PlayerManager::create});
}
void ModuleBF2::getSingletonList(std::vector<SingletonRegInfo>&)
{
    // empty  -  5 bytes in the binary, no entries.
}

// ModuleGame
BF2_MODULE_IBASE(ModuleGame)
void ModuleGame::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_StatsManager, "dice.hfe.StatsManager", &StatsManager::create});
    out.push_back(ClassRegInfo{CID_PlayerStatsManager, "dice.hfe.PlayerStatsManager", &PlayerStatsManager::create});
    out.push_back(ClassRegInfo{CID_PlayerAwards, "dice.hfe.PlayerAwards", &PlayerAwards::create});
    out.push_back(ClassRegInfo{CID_PlayerStats, "dice.hfe.PlayerStats", &PlayerStats::create});
    out.push_back(ClassRegInfo{CID_PlayerUnlocks, "dice.hfe.PlayerUnlocks", &PlayerUnlocks::create});
    out.push_back(ClassRegInfo{CID_SearchForPlayer, "dice.hfe.SearchForPlayer", &SearchForPlayer::create});
    out.push_back(ClassRegInfo{CID_LeaderBoard, "dice.hfe.LeaderBoard", &LeaderBoard::create});
    out.push_back(ClassRegInfo{CID_PlayerUnlockInfo, "dice.hfe.PlayerUnlockInfo", &PlayerUnlockInfo::create});
    out.push_back(ClassRegInfo{CID_PlayerRankInfo, "dice.hfe.PlayerRankInfo", &PlayerRankInfo::create});
#if !DEDICATED
    // Client rows inside !isDedicated(); unrecovered intervening factories stay withheld.
    if (!isDedicated()) {
        // PE430A53..430A7B, then430A9F..430AC7: settings, then HUD information.
        out.push_back(ClassRegInfo{CID_GameServerSettings, "dice.hfe.GameServerSettings", &GameServerSettings::create});
        out.push_back(ClassRegInfo{CID_HudInformationLayer, "dice.bf2.HudInformationLayer", &HudInformationLayer::create});
        // PE430B1D..430B45 and430B5C..430B84; DistributedLightRenderer lies between HUD and host.
        out.push_back(ClassRegInfo{CID_SwiffHost, "dice.hfe.SwiffHost", &SwiffHost::create});
        out.push_back(ClassRegInfo{CID_MainMenuLogic, "dice.hfe.MainMenuLogic", &MainMenuLogic::create});
        // Loading-screen logic created by the InitLoadingScreen menu event.
        out.push_back(ClassRegInfo{CID_BetweenRoundsLogic, "dice.hfe.BetweenRoundsLogic", &BetweenRoundsLogic::create});
        // EndOfRoundLogic and LoadGameLogic remain unregistered until their classes are implemented.
        out.push_back(ClassRegInfo{CID_LocalProfileManager, "dice.hfe.LocalProfileManager", &LocalProfileManager::create});
        // PE430C97..430CCF: the online owner immediately follows the local manager.
        out.push_back(ClassRegInfo{CID_ProfileManager, "dice.hfe.ProfileManager", &ProfileManager::create});
        // PE430E11..430E39: later than LocalProfileManager, before GlobalSettings.
        out.push_back(ClassRegInfo{CID_ServerBrowser, "dice.hfe.ServerBrowser", &ServerBrowser::create});
        // PE430E50: the existing MenuTeamManager singleton requires this factory.
        out.push_back(ClassRegInfo{CID_MenuTeamManager, "dice.hfe.MenuTeamManager", &MenuTeamManager::create});
        // PE430E8F..430EB7: after MenuTeamManager's row.
        out.push_back(ClassRegInfo{CID_GlobalSettings, "dice.hfe.GlobalSettings", &GlobalSettings::create});
        // PE430ECE..430EF6: after GlobalSettings, before VideoSettings.
        // Adjacent missing factories remain withheld; no table position is invented.
        out.push_back(ClassRegInfo{CID_AudioSettings, "dice.hfe.AudioSettings", &AudioSettings::create});
        // PE430F0D..430F35: directly after AudioSettings.
        out.push_back(ClassRegInfo{CID_VideoSettings, "dice.hfe.VideoSettings", &VideoSettings::create});
        // PE430F4C..430F74: directly after VideoSettings.
        out.push_back(ClassRegInfo{CID_ControlSettings, "dice.hfe.ControlSettings", &ControlSettings::create});
        // PE430F8B..430FB3: directly after ControlSettings.
        out.push_back(ClassRegInfo{CID_GeneralSettings, "dice.hfe.GeneralSettings", &GeneralSettings::create});
        // PE430FCA..430FF2: directly after GeneralSettings.
        out.push_back(ClassRegInfo{CID_HapticSettings, "dice.hfe.HapticSettings", &HapticSettings::create});
        // PE431009..431031: directly after HapticSettings.
        out.push_back(ClassRegInfo{CID_ServerBrowserManager, "dice.hfe.ServerBrowserManager", &ServerBrowserManager::create});
        // PE431048..431070: after ServerBrowserManager's row and before ServerLogoManager's.
        out.push_back(ClassRegInfo{CID_DemoLibrary, "dice.hfe.DemoLibrary", &DemoLibrary::create});
        // PE431087..4310AF: the last row of the client's table.
        out.push_back(ClassRegInfo{CID_ServerLogoManager, "dice.hfe.ServerLogoManager", &ServerLogoManager::create});
    }
#endif
    out.push_back(ClassRegInfo{CID_ModInfo, "dice.hfe.ModInfo", &ModInfo::create});
}
void ModuleGame::getSingletonList(std::vector<SingletonRegInfo>& out)
{
#if !DEDICATED
    // Client-only singleton registrations. GameServerSettings is not registered in the dedicated image;
    // [UNRESOLVED] Its original dedicated-module placement.
    if (!isDedicated())
    {
        out.push_back(SingletonRegInfo{"GameServerSettings",       &g_gameServerSettings,       IID_IGameServerSettings,    CID_GameServerSettings,       2});
        out.push_back(SingletonRegInfo{"HudInformationLayer",      &g_hudInformationLayer,      1,                          CID_HudInformationLayer,      1});
        out.push_back(SingletonRegInfo{"DistributedLightRenderer", &g_distributedLightRenderer, 1,                          CID_DistributedLightRenderer, 1});
        out.push_back(SingletonRegInfo{"LocalProfileManager",      &g_localProfileManager,      IID_ILocalProfileManager,   CID_LocalProfileManager,      2});
        out.push_back(SingletonRegInfo{"ProfileManager",           &g_profileManager,           IID_IProfileManager,        CID_ProfileManager,           2});
        out.push_back(SingletonRegInfo{"RankManager",              &g_rankManager,              IID_IRankManager,           CID_RankManager,              2});
        out.push_back(SingletonRegInfo{"ClanManager",              &g_clanManager,              IID_IClanManager,           CID_ClanManager,              1});
        out.push_back(SingletonRegInfo{"NewsManager",              &g_newsManager,              IID_INewsManager,           CID_NewsManager,              1});
        out.push_back(SingletonRegInfo{"AutoPatch",                &g_autoPatch,                IID_IAutoPatch,             CID_AutoPatch,                1});
        out.push_back(SingletonRegInfo{"Persistence",              &g_persistence,              IID_IPersistence,           CID_Persistence,              1});
        out.push_back(SingletonRegInfo{"MenuTeamManager",          &g_menuTeamManager,          IID_IMenuTeamManager,       CID_MenuTeamManager,          2});
        out.push_back(SingletonRegInfo{"GlobalSettings",           &g_globalSettings,           IID_IGlobalSettings,        CID_GlobalSettings,           2});
        out.push_back(SingletonRegInfo{"VideoSettings",            &g_videoSettings,            IID_IVideoSettings,         CID_VideoSettings,            2});
        out.push_back(SingletonRegInfo{"AudioSettings",            &g_audioSettings,            IID_IAudioSettings,         CID_AudioSettings,            2});
        out.push_back(SingletonRegInfo{"ControlSettings",          &g_controlSettings,          IID_IControlSettings,       CID_ControlSettings,          2});
        out.push_back(SingletonRegInfo{"GeneralSettings",          &g_generalSettings,          IID_IGeneralSettings,       CID_GeneralSettings,          2});
        out.push_back(SingletonRegInfo{"HapticSettings",           &g_hapticSettings,           IID_IHapticSettings,        CID_HapticSettings,           2});
        out.push_back(SingletonRegInfo{"ServerBrowserManager",     &g_serverBrowserManager,     IID_IServerBrowserManager,  CID_ServerBrowserManager,     2});
        out.push_back(SingletonRegInfo{"SwiffHost",                &g_swiffHost,                IID_ISwiffHost,             CID_SwiffHost,                2});
        out.push_back(SingletonRegInfo{"DemoLibrary",              &g_demoLibrary,              IID_IDemoLibrary,           CID_DemoLibrary,              2});
        out.push_back(SingletonRegInfo{"ServerLogoManager",        &g_serverLogoManager,        IID_IServerLogoManager,     CID_ServerLogoManager,        2});
    }
#endif
    out.push_back(SingletonRegInfo{"ModInfo", &g_modInfo, IID_IModInfo, CID_ModInfo, 2});
}

// ModuleGameLogic
BF2_MODULE_IBASE(ModuleGameLogic)
void ModuleGameLogic::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_SquadManager, "dice.bf.SquadManager", &SquadManager::create});
    out.push_back(ClassRegInfo{CID_ClientSquadManager, "dice.bf.ClientSquadManager", &ClientSquadManager::create});
    out.push_back(ClassRegInfo{CID_ClientGameLogic, "dice.bf.clientGameLogic", &ClientGameLogic::create});
    out.push_back(ClassRegInfo{CID_ServerGameLogic, "dice.bf.serverGameLogic", &ServerGameLogic::create});
}
void ModuleGameLogic::getSingletonList(std::vector<SingletonRegInfo>&) {}

// ModuleNetworkables  -  the 16 replicated types
BF2_MODULE_IBASE(ModuleNetworkables)
void ModuleNetworkables::getClassList(std::vector<ClassRegInfo>& out)
{
    // Networkable factories must follow the client/server registration order.
    out.push_back(ClassRegInfo{CID_SimpleObjectNetworkable, "dice.bf2.Networkable.SimpleObject", &SimpleObjectNetworkable::create});
    out.push_back(ClassRegInfo{CID_RotationalBundleNetworkable, "dice.bf2.Networkable.RotationalBundle", &RotationalBundleNetworkable::create});
    // Free cameras require this component during player allocation.
    out.push_back(ClassRegInfo{CID_FreeCameraNetworkable, "dice.bf2.Networkable.FreeCamera", &FreeCameraNetworkable::create});
    out.push_back(ClassRegInfo{CID_OverheadCameraNetworkable, "dice.bf2.Networkable.OverheadCamera", &OverheadCameraNetworkable::create});
    out.push_back(ClassRegInfo{CID_SpringNetworkable, "dice.bf2.Networkable.Spring", &SpringNetworkable::create});
    out.push_back(ClassRegInfo{CID_ItemNetworkable, "dice.bf2.Networkable.Item", &ItemNetworkable::create});
    // Destroyable objects use always-ghosted static descriptors and consume shared-sweep network IDs.
    out.push_back(ClassRegInfo{CID_DestroyableObjectNetworkable, "dice.bf2.Networkable.DestroyableObject", &DestroyableObjectNetworkable::create});
    out.push_back(ClassRegInfo{CID_DestroyableWindowsBundleNetworkable, "dice.bf2.Networkable.DestroyableWindowsBundle", &DestroyableWindowsBundleNetworkable::create});
    // Soldier warm-up allocation consumes network IDs through this component.
    out.push_back(ClassRegInfo{CID_SoldierNetworkable, "dice.bf2.Networkable.Soldier", &SoldierNetworkable::create});
    // Preserve the double dot in this registered lookup key; the single-dot spelling will not match.
    out.push_back(ClassRegInfo{CID_PlayerControlObjectNetworkable, "dice..bf2.Networkable.PlayerControlObject", &PlayerControlObjectNetworkable::create});
    out.push_back(ClassRegInfo{CID_KitNetworkable, "dice.bf2.Networkable.Kit", &KitNetworkable::create});
    // Projectile warm-up allocation consumes network IDs through this component.
    // Client prediction and interpolation retain their named [FRONTIER] boundaries.
    out.push_back(ClassRegInfo{CID_GenericProjectileNetworkable, "dice.bf2.Networkable.GenericProjectile", &GenericProjectileNetworkable::create});
    out.push_back(ClassRegInfo{CID_ControlPointNetworkable, "dice.bf2.Networkable.ControlPoint", &ControlPointNetworkable::create});
    out.push_back(ClassRegInfo{CID_EngineNetworkable, "dice.bf2.Networkable.Engine", &EngineNetworkable::create});
    out.push_back(ClassRegInfo{CID_TargetObjectNetworkable, "dice.bf2.Networkable.TargetObject", &TargetObjectNetworkable::create});
    out.push_back(ClassRegInfo{CID_TriggerableTargetNetworkable, "dice.bf2.Networkable.TriggerableTarget", &TriggerableTargetNetworkable::create});
}
void ModuleNetworkables::getSingletonList(std::vector<SingletonRegInfo>&) {}

// MathModule  -  registers nothing at all
BF2_MODULE_IBASE(MathModule)
void MathModule::getClassList(std::vector<ClassRegInfo>&) {}
void MathModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

// BfSoundModule
BF2_MODULE_IBASE(BfSoundModule)
void BfSoundModule::getClassList(std::vector<ClassRegInfo>& out)
{
    //  9986001 is an inline immediate  -  no CID_ global names it.
    out.push_back(ClassRegInfo{9986001u, "dice.hfe.world.ObjectTemplate.Sound", &ClassFactory<SoundObjectTemplate>::create});
}
void BfSoundModule::getSingletonList(std::vector<SingletonRegInfo>& out)
{
    // The registry stores the address of the typed sound pointer through its generic slot representation.
    out.push_back(SingletonRegInfo{"Sound", reinterpret_cast<IBase**>(&g_bfSound), 0, 0, 4});
}

// SpawnSystemModule
BF2_MODULE_IBASE(SpawnSystemModule)
void SpawnSystemModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_SpawnPointTemplate, "dice.bf.SpawnPoint", &SpawnPointTemplate::create});
    out.push_back(ClassRegInfo{CID_SpawnManager, "dice.bf.SpawnManager", &SpawnManager::create});
}
void SpawnSystemModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

// io
namespace io {

// Io::ModuleIO  -  note the four .Null implementations
BF2_MODULE_IBASE(ModuleIO)
void ModuleIO::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_ControlMap, "dice.hfe.io.ControlMap.Standard", &io::ControlMap::create});
    out.push_back(ClassRegInfo{CID_NullInputDeviceManager, "dice.hfe.io.InputDeviceManager.Null", &io::NullInputDeviceManager::create});
    out.push_back(ClassRegInfo{CID_NullResourceManager, "dice.hfe.io.ResourceManager.Null", &io::NullResourceManager::create});
    out.push_back(ClassRegInfo{CID_NullCacheManager, "dice.hfe.io.CacheManager.Null", &io::NullCacheManager::create});
    out.push_back(ClassRegInfo{CID_ControlMapManager, "dice.hfe.io.ControlMapManager.Standard", &io::ControlMapManager::create});
    out.push_back(ClassRegInfo{CID_ExcelLexicon, "dice.hfe.ExcelLexicon", &io::ExcelLexicon::create});
}
void ModuleIO::getSingletonList(std::vector<SingletonRegInfo>& out)
{
    //  FileManager and FileChangeManager take the loadStat cast: both globals already exist with
    // real class types (IO/File/*.h, defined in Core/Globals.cpp) and must not be redeclared as
    // IBase* here  -  that is precisely how one variable ends up as two MSVC symbols.
    out.push_back(SingletonRegInfo{"FileManager", reinterpret_cast<IBase**>(&fileManager), IID_IFileManager, CID_FileManager, 1});
    out.push_back(SingletonRegInfo{"FileChangeManager", reinterpret_cast<IBase**>(&fileChangeManager), IID_IFileChangeManager, CID_FileChangeManager, 1});
    out.push_back(SingletonRegInfo{"ResourceManager", &resourceManager, IID_IResourceManager, CID_NullResourceManager, 1});
    out.push_back(SingletonRegInfo{"CacheManager", &cacheManager, IID_ICacheManager, CID_NullCacheManager, 1});
    out.push_back(SingletonRegInfo{"AsyncMultiStreamer", &asyncMultiStreamer, IID_IAsyncMultiStreamer, CID_AsyncMultiStreamer, 1});
    out.push_back(SingletonRegInfo{"InputDeviceManager", &inputDeviceManager, IID_IInputDeviceManager, CID_NullInputDeviceManager, 1});
    out.push_back(SingletonRegInfo{"InputManager", &inputManager, IID_IInputManager, CID_InputManager, 1});
    out.push_back(SingletonRegInfo{"ControlMapManager", &controlMapManager, IID_IControlMapManager, CID_ControlMapManager, 1});
    // Register the existing main-console instance by its global pointer slot.
    out.push_back(SingletonRegInfo{"MainConsole", reinterpret_cast<IBase**>(&mainConsole), 0, 0, 4});
    out.push_back(SingletonRegInfo{"ActiveMethodConsole", &activeMethodConsole, 0, 0, 4});
    out.push_back(SingletonRegInfo{"Lexicon", &lexicon, IID_ILexicon, CID_ExcelLexicon, 2});
}

// Io::FileModule
BF2_MODULE_IBASE(FileModule)
void FileModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_FileManager, "dice.hfe.io.FileManager.Standard", &io::FileManager::create});
    out.push_back(ClassRegInfo{CID_FileChangeManager, "dice.hfe.io.FileChangeManager.Standard", &io::FileChangeManager::create});
    // Register the Unix filesystem only in the POSIX image that implements it.
#if !defined(_WIN32)
    out.push_back(ClassRegInfo{CID_UnixFileSystem, "dice.hfe.io.FileSystem.Unix", &io::UnixFileSystem::create});
#endif

    // [CHOSEN] Win32 registration matches the platform-specific filesystem selected during setup.
    // The CID must be registered before the setup factory lookup.
#if defined(_WIN32)
    out.push_back(ClassRegInfo{CID_Win32FileSystem, "dice.hfe.io.FileSystem.Win32", &io::Win32FileSystem::create});
#endif
    out.push_back(ClassRegInfo{CID_ZipArchiveHandler, "dice.hfe.io.ArchiveHandler.ZIP", &io::ZipArchiveHandler::create});
    out.push_back(ClassRegInfo{CID_MemStream, "dice.hfe.io.Stream.Memory", &io::MemStream::create});
    out.push_back(ClassRegInfo{CID_BufferedStream, "dice.hfe.io.Stream.Buffered", &io::BufferedStream::create});
}
void FileModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

// Io::NetworkModule
BF2_MODULE_IBASE(NetworkModule)
void NetworkModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_NetworkableInfoManager, "dice.hfe.io.NetworkableInfoManager.Standard", &io::NetworkableInfoManager::create});
}
void NetworkModule::getSingletonList(std::vector<SingletonRegInfo>& out)
{
    out.push_back(SingletonRegInfo{"NetworkableInfoManager", &networkableInfoManager, IID_INetworkableInfoManager, CID_NetworkableInfoManager, 1});
}

// Io::InputDX8Module
BF2_MODULE_IBASE(InputDX8Module)
#if !DEDICATED
void InputDX8Module::getClassList(std::vector<ClassRegInfo>& out) // PE6AC3E6, complete138 bytes
{
    // [PLATFORM] Client table89AE6C supplies these two rows in this order.
    // Use native ClassRegInfo/factory ABI, never the retail36-byte STL record.
    out.push_back(ClassRegInfo{CID_DX8InputDeviceManager, "dice.hfe.io.InputDeviceManager.DX8", &DX8InputDeviceManager::create});
    out.push_back(ClassRegInfo{CID_InputManager, "dice.hfe.io.InputManager", &InputManager::create});
}
#else
void InputDX8Module::getClassList(std::vector<ClassRegInfo>&) {}
#endif
void InputDX8Module::getSingletonList(std::vector<SingletonRegInfo>&) {} // PE57F33A / ELF84FDA20: empty

} // namespace io

// console / tool / animationTools / ai
namespace console {
BF2_MODULE_IBASE(ModuleConsole)
void ModuleConsole::getClassList(std::vector<ClassRegInfo>&) {}
void ModuleConsole::getSingletonList(std::vector<SingletonRegInfo>&) {}
}

namespace tool {   // attested path: Scene/Object/ToolObject/ModuleToolObject.cpp
BF2_MODULE_IBASE(ModuleToolObject)
void ModuleToolObject::getClassList(std::vector<ClassRegInfo>&) {}
void ModuleToolObject::getSingletonList(std::vector<SingletonRegInfo>&) {}
}

namespace animationTools {   // attested path: Animation/BoneAnimation/ModuleBoneAnimation.cpp
BF2_MODULE_IBASE(AnimModule)
void AnimModule::getClassList(std::vector<ClassRegInfo>&) {}
void AnimModule::getSingletonList(std::vector<SingletonRegInfo>&) {}
}

namespace ai {
BF2_MODULE_IBASE(AIInterfaceModule)
void AIInterfaceModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_AIConnection, "dice.bf2.ai.AIConnection", &ai::ClassFactory<ai::AIConnection>::create});
}
void AIInterfaceModule::getSingletonList(std::vector<SingletonRegInfo>& out)
{
    out.push_back(SingletonRegInfo{"AIConnection", &g_aiConnection, IID_IAIConnection, CID_AIConnection, 2});
}
}

// world
namespace world {

// World::Module  -  singletons only
BF2_MODULE_IBASE(Module)
void Module::getClassList(std::vector<ClassRegInfo>&) {}
void Module::getSingletonList(std::vector<SingletonRegInfo>& out)
{
    // Register typed manager pointer slots through the generic registry representation.
    out.push_back(SingletonRegInfo{"ObjectManager", reinterpret_cast<IBase**>(&objectManager), IID_IObjectManager, CID_ObjectManager, 1});
    out.push_back(SingletonRegInfo{"ObjectTemplateManager", reinterpret_cast<IBase**>(&objectTemplateManager), IID_IObjectTemplateManager, CID_ObjectTemplateManager, 1});
    out.push_back(SingletonRegInfo{"PlayerManager", reinterpret_cast<IBase**>(&playerManager), IID_IPlayerManager, CID_PlayerManager, 1});
    out.push_back(SingletonRegInfo{"WindManager", &windManager, IID_IWindManager, CID_WindManager, 1});
}

// World::ObjectModule
BF2_MODULE_IBASE(ObjectModule)
void ObjectModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_ObjectManager, "dice.hfe.world.ObjectManager.Standard", &world::ObjectManager::create});
    out.push_back(ClassRegInfo{CID_WindManager, "dice.hfe.world.WindManager.Standard", &world::WindManager::create});
    out.push_back(ClassRegInfo{CID_Object, "dice.hfe.world.Object.Standard", &world::Object::create});
#if DEDICATED
    // Dedicated body preserves retail's absent-render-singleton fault in update().
    out.push_back(ClassRegInfo{CID_ObjectPriorityCalculator, "dice.hfe.world.ObjectPriorityCalculator.Standard", &world::ObjectPriorityCalculator::create});
#else
    // RW-6: the client render-singleton update arm remains unrecovered.
#endif
    out.push_back(ClassRegInfo{CID_QuadTreeCuller, "dice.hfe.world.Culler.QuadTreeCuller", &world::QuadTreeCuller::create});
    out.push_back(ClassRegInfo{CID_BinTreeCuller, "dice.hfe.world.Culler.BinTreeCuller", &world::BinTreeCuller::create});
}
void ObjectModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

// World::ObjTemplBaseModule
BF2_MODULE_IBASE(ObjTemplBaseModule)
void ObjTemplBaseModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_ObjectTemplateManager, "dice.hfe.world.ObjectTemplateManager.Standard", &world::ClassFactory<world::ObjectTemplateManager>::create});
}
void ObjTemplBaseModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

// World::ObjTemplPortalModule
BF2_MODULE_IBASE(ObjTemplPortalModule)
void ObjTemplPortalModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_PortalManager, "dice.hfe.world.PortalManager.Standard", &world::ClassFactory<world::PortalManager>::create});
}
void ObjTemplPortalModule::getSingletonList(std::vector<SingletonRegInfo>& out)
{
    out.push_back(SingletonRegInfo{"PortalManager", &portalManager,
                                   IID_IPortalManager, CID_PortalManager, 1});
}

// World::PhysicsModule
BF2_MODULE_IBASE(PhysicsModule)
void PhysicsModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_BasicPhysicsSystem, "dice.hfe.world.PhysicsSystem.Basic", &world::ClassFactory<world::BasicPhysicsSystem>::create});
    out.push_back(ClassRegInfo{CID_PhysicsNode, "dice.hfe.world.PhysicsNode.Basic", &world::ClassFactory<world::PhysicsNode>::create});
    out.push_back(ClassRegInfo{CID_PhysicsNodeManager, "dice.hfe.world.PhysicsNodeManager.Basic", &world::ClassFactory<world::PhysicsNodeManager>::create});
    out.push_back(ClassRegInfo{CID_ResponsePhysics, "dice.hfe.world.ResponsePhysics.Basic", &world::ClassFactory<world::ResponsePhysics>::create});
    out.push_back(ClassRegInfo{CID_ResponsePhysicsManager, "dice.hfe.world.ResponsePhysicsManager.Basic", &world::ClassFactory<world::ResponsePhysicsManager>::create});
    out.push_back(ClassRegInfo{CID_Heightmap, "dice.hfe.world.Heightmap", &world::Heightmap::create});
    // Editable heightmap implementations are selected by the editor image.
    out.push_back(ClassRegInfo{CID_HeightmapEditable, "dice.hfe.world.HeightmapEditable", &world::HeightmapEditable::create});
    out.push_back(ClassRegInfo{CID_HeightmapCluster, "dice.hfe.world.HeightmapCluster", &world::HeightmapCluster::create});
    out.push_back(ClassRegInfo{CID_HeightmapClusterEditable, "dice.hfe.world.HeightmapClusterEditable", &world::HeightmapClusterEditable::create});
    out.push_back(ClassRegInfo{CID_CollisionManager, "dice.hfe.world.CollisionManager", &world::CollisionManager::create});
}
void PhysicsModule::getSingletonList(std::vector<SingletonRegInfo>& out)
{
    out.push_back(SingletonRegInfo{"BasicPhysicsSystem", &basicPhysicsSystem, IID_IPhysicsSystem, CID_BasicPhysicsSystem, 1});
    // Register the addresses of the typed physics-manager globals; their symbol names are attested.
    out.push_back(SingletonRegInfo{"PhysicsNodeManager", reinterpret_cast<IBase**>(&physicsNodeManager), IID_IPhysicsNodeManager, CID_PhysicsNodeManager, 1});
    out.push_back(SingletonRegInfo{"ResponsePhysicsManager", reinterpret_cast<IBase**>(&responsePhysicsManager), IID_IResponsePhysicsManager, CID_ResponsePhysicsManager, 1});
    out.push_back(SingletonRegInfo{"HeightmapCluster", &heightmapCluster, IID_IHeightmapCluster, CID_HeightmapCluster, 1});
    out.push_back(SingletonRegInfo{"CollisionManager", reinterpret_cast<IBase**>(&collisionManager), IID_ICollisionManager, CID_CollisionManager, 1});
}

//world::BFMaterialSystemModule  -  attested: Game/GameLogic/MaterialSystem/ModuleMaterialSystem.cpp
BF2_MODULE_IBASE(BFMaterialSystemModule)
void BFMaterialSystemModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{50351u, "dice.hfe.world.MaterialManager", &world::MaterialManager::create});
}
void BFMaterialSystemModule::getSingletonList(std::vector<SingletonRegInfo>& out)
{
    out.push_back(SingletonRegInfo{"MaterialManager", &materialManager, 0, 0, 1});
}

// World::ScoreSystemModule  -  attested: Game/GameLogic/ScoreSystem/ModuleScoreSystem.cpp
BF2_MODULE_IBASE(ScoreSystemModule)
void ScoreSystemModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{50361u, "dice.hfe.world.ScoreManager", &world::ScoreManager::create});
}
void ScoreSystemModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

// World::HudCompModule  -  attested: Game/Objects/HudComp/ModuleHudComp.cpp
BF2_MODULE_IBASE(HudCompModule)
void HudCompModule::getClassList(std::vector<ClassRegInfo>& out)
{
    // Complete PE 58C743 and ELF 8748E50 reads: preserve all seven rows/order.
    out.push_back(ClassRegInfo{CID_PostProcessCompTemplate, "dice.hfe.world.TemplateComponent.PostProcess", &world::ClassFactory<world::PostProcessCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_VehicleHudCompTemplate, "dice.hfe.world.TemplateComponent.VehicleHud", &world::ClassFactory<world::VehicleHudCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_WeaponHudCompTemplate, "dice.hfe.world.TemplateComponent.WeaponHud", &world::ClassFactory<world::WeaponHudCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_AbilityHudCompTemplate, "dice.hfe.world.TemplateComponent.AbilityHud", &world::ClassFactory<world::AbilityHudCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_WarningHudCompTemplate, "dice.hfe.world.TemplateComponent.WarningHud", &world::ClassFactory<world::WarningHudCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_HelpHudCompTemplate, "dice.hfe.world.TemplateComponent.HelpHud", &world::ClassFactory<world::HelpHudCompTemplate>::create});
    // Complete template/runtime recovered from ELF; client PE independently agrees.
    out.push_back(ClassRegInfo{CID_StrategicObjectCompTemplate, "dice.hfe.world.TemplateComponent.StrategicObject", &world::ClassFactory<world::StrategicObjectCompTemplate>::create});
}
void HudCompModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

// World::GameLogicCompModule  -  attested: Game/Objects/GameLogicComp/ModuleGameLogicComp.cpp
BF2_MODULE_IBASE(GameLogicCompModule)
void GameLogicCompModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_ArmorGLCompTemplate, "dice.hfe.world.TemplateComponent.Armor", &world::ClassFactory<world::ArmorGLCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_AbilityGLCompTemplate, "dice.hfe.world.TemplateComponent.Ability", &world::ClassFactory<world::AbilityGLCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_RadioGLCompTemplate, "dice.hfe.world.TemplateComponent.Radio", &world::ClassFactory<world::RadioGLCompTemplate>::create});
}
void GameLogicCompModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

//world::GameLogicObjTemplBaseModule  -  attested: Game/Objects/Base/ModuleWorldObjTemplBase.cpp --
BF2_MODULE_IBASE(GameLogicObjTemplBaseModule)
void GameLogicObjTemplBaseModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_BundleTemplate, "dice.hfe.world.ObjectTemplate.Bundle", &world::ClassFactory<world::BundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_DynamicBundleTemplate, "dice.hfe.world.ObjectTemplate.DynamicBundle", &world::ClassFactory<world::DynamicBundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_CameraTemplate, "dice.hfe.world.ObjectTemplate.Camera", &world::ClassFactory<world::CameraTemplate>::create});
    out.push_back(ClassRegInfo{CID_EntryPointTemplate, "dice.hfe.world.ObjectTemplate.EntryPoint", &world::ClassFactory<world::EntryPointTemplate>::create});
    out.push_back(ClassRegInfo{CID_RotationalBundleTemplate, "dice.hfe.world.ObjectTemplate.RotationalBundle", &world::ClassFactory<world::RotationalBundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_SimpleObjectTemplate, "dice.hfe.world.ObjectTemplate.SimpleObject", &world::ClassFactory<world::SimpleObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_FreeCameraTemplate, "dice.hfe.world.ObjectTemplate.FreeCamera", &world::ClassFactory<world::FreeCameraTemplate>::create});
    out.push_back(ClassRegInfo{CID_OverheadCameraTemplate, "dice.hfe.world.ObjectTemplate.OverheadCamera", &world::ClassFactory<world::OverheadCameraTemplate>::create});
    out.push_back(ClassRegInfo{CID_LightSourceTemplate, "dice.hfe.world.ObjectTemplate.LightSource", &world::ClassFactory<world::LightSourceTemplate>::create});
    //  19,381,293  -  four orders of magnitude outside every neighbouring id. Read from the
    // instruction stream. An id-range sanity check would reject it as corrupt; it is not.
    out.push_back(ClassRegInfo{CID_WindAffectorTemplate, "dice.hfe.world.TemplateComponent.WindAffector", &world::ClassFactory<world::WindAffectorTemplate>::create});
    out.push_back(ClassRegInfo{CID_IkTemplateComponent, "dice.hfe.world.TemplateComponent.Ik", &world::ClassFactory<world::IkTemplateComponent>::create}); // RW-6b retired: retail createComponent returns null
}
void GameLogicObjTemplBaseModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

// World::GameLogicObjTemplFxModule  -  attested: Game/Objects/Fx/ModuleWorldObjTemplFx.cpp
BF2_MODULE_IBASE(GameLogicObjTemplFxModule)
void GameLogicObjTemplFxModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_EffectBundleTemplate, "dice.hfe.world.ObjectTemplate.EffectBundle", &world::ClassFactory<world::EffectBundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_WheelEffectBundleTemplate, "dice.hfe.world.ObjectTemplate.WheelEffectBundle", &world::ClassFactory<world::WheelEffectBundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_AirDraftEffectBundleTemplate, "dice.hfe.world.ObjectTemplate.AirDraftEffectBundle", &world::ClassFactory<world::AirDraftEffectBundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_GroundEffectBundleTemplate, "dice.hfe.world.ObjectTemplate.GroundEffectBundle", &world::ClassFactory<world::GroundEffectBundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_DecalTemplate, "dice.hfe.world.ObjectTemplate.Decal", &world::ClassFactory<world::DecalTemplate>::create});
    out.push_back(ClassRegInfo{CID_EmitterTemplate, "dice.hfe.world.ObjectTemplate.Emitter", &world::ClassFactory<world::EmitterTemplate>::create});
    out.push_back(ClassRegInfo{CID_ParticleTemplate, "dice.hfe.world.ObjectTemplate.Particle", &world::ClassFactory<world::ParticleTemplate>::create});
#if DEDICATED
    out.push_back(ClassRegInfo{CID_SpriteParticleTemplate, "dice.hfe.world.ObjectTemplate.SpriteParticle", &world::ClassFactory<world::SpriteParticleTemplate>::create});
#endif // Client particle presentation remains outside the dedicated recovery.
    // Particle templates must retain registration order so client and server assign matching template IDs.
    out.push_back(ClassRegInfo{CID_SpriteParticleSystemTemplate, "dice.hfe.world.ObjectTemplate.SpriteParticleSystem", &world::ClassFactory<world::SpriteParticleSystemTemplate>::create});
    out.push_back(ClassRegInfo{CID_MeshParticleSystemTemplate, "dice.hfe.world.ObjectTemplate.MeshParticleSystem", &world::ClassFactory<world::MeshParticleSystemTemplate>::create});
    out.push_back(ClassRegInfo{CID_TrailSystemTemplate, "dice.hfe.world.ObjectTemplate.TrailSystem", &world::ClassFactory<world::TrailSystemTemplate>::create});
    out.push_back(ClassRegInfo{CID_ParticleSystemEmitterTemplate, "dice.hfe.world.ObjectTemplate.ParticleSystemEmitter", &world::ClassFactory<world::ParticleSystemEmitterTemplate>::create});
    out.push_back(ClassRegInfo{CID_NonScreenAlignedParticleSystemTemplate, "dice.hfe.world.ObjectTemplate.NonScreenAlignedParticleSystem", &world::ClassFactory<world::NonScreenAlignedParticleSystemTemplate>::create});
    out.push_back(ClassRegInfo{CID_EnvMapTemplate, "dice.hfe.world.ObjectTemplate.EnvMap", &world::ClassFactory<world::EnvMapTemplate>::create});
}
void GameLogicObjTemplFxModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

//world::GameLogicObjTemplVehicleModule  -  attested: Game/Objects/Vehicle/ModuleWorldObjTemplVehicle.cpp
BF2_MODULE_IBASE(GameLogicObjTemplVehicleModule)
void GameLogicObjTemplVehicleModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_EngineTemplate, "dice.hfe.world.ObjectTemplate.Engine", &world::ClassFactory<world::EngineTemplate>::create});
    out.push_back(ClassRegInfo{CID_FloatingBundleTemplate, "dice.hfe.world.ObjectTemplate.FloatingBundle", &world::ClassFactory<world::FloatingBundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_SpringTemplate, "dice.hfe.world.ObjectTemplate.Spring", &world::ClassFactory<world::SpringTemplate>::create});
    out.push_back(ClassRegInfo{CID_WingTemplate, "dice.hfe.world.ObjectTemplate.Wing", &world::ClassFactory<world::WingTemplate>::create});
    out.push_back(ClassRegInfo{CID_LandingGearTemplate, "dice.hfe.world.ObjectTemplate.LandingGear", &world::ClassFactory<world::LandingGearTemplate>::create});
    out.push_back(ClassRegInfo{CID_RotorTemplate, "dice.hfe.world.ObjectTemplate.Rotor", &world::ClassFactory<world::RotorTemplate>::create});
    out.push_back(ClassRegInfo{CID_ForceObjectTemplate, "dice.hfe.world.ObjectTemplate.ForceObject", &world::ClassFactory<world::ForceObjectTemplate>::create});
}
void GameLogicObjTemplVehicleModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

// World::GameLogicObjTemplBFModule  -  73 classes, 17,432 bytes, the largest in the engine
BF2_MODULE_IBASE(GameLogicObjTemplBFModule)
void GameLogicObjTemplBFModule::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_ItemTemplate, "dice.hfe.world.ObjectTemplate.Item", &world::ClassFactory<world::ItemTemplate>::create});
    out.push_back(ClassRegInfo{CID_ItemContainerTemplate, "dice.hfe.world.ObjectTemplate.ItemContainer", &world::ClassFactory<world::ItemContainerTemplate>::create});
    // RW-98 retired: the real bundle/panes now occupy their stock template IDs.
    out.push_back(ClassRegInfo{CID_DestroyableWindowsBundleTemplate, "dice.hfe.world.ObjectTemplate.DestroyableWindowsBundle", &world::ClassFactory<world::DestroyableWindowsBundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_DestroyableWindowTemplate, "dice.hfe.world.ObjectTemplate.DestroyableWindow", &world::ClassFactory<world::DestroyableWindowTemplate>::create});
    out.push_back(ClassRegInfo{CID_DestroyableObjectTemplate, "dice.hfe.world.ObjectTemplate.DestroyableObject", &world::ClassFactory<world::DestroyableObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_KitTemplate, "dice.hfe.world.ObjectTemplate.Kit", &world::ClassFactory<world::KitTemplate>::create});
    // RW-6 retired for MaskObject: recovered template/runtime and authored mask properties.
    // Xpack defines 26 of these; omitting them shifts later CreateObject template IDs.
    out.push_back(ClassRegInfo{CID_MaskObjectTemplate, "dice.hfe.world.ObjectTemplate.MaskObject", &world::ClassFactory<world::MaskObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_AntennaObjectTemplate, "dice.hfe.world.ObjectTemplate.AntennaObject", &world::ClassFactory<world::AntennaObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_AnimatedBundleTemplate, "dice.hfe.world.ObjectTemplate.AnimatedBundle", &world::ClassFactory<world::AnimatedBundleTemplate>::create});
    out.push_back(ClassRegInfo{CID_SoldierTemplate, "dice.hfe.world.ObjectTemplate.Soldier", &world::ClassFactory<world::SoldierTemplate>::create});
    out.push_back(ClassRegInfo{CID_GenericFireArmTemplate, "dice.hfe.world.ObjectTemplate.GenericFireArm", &world::ClassFactory<world::GenericFireArmTemplate>::create});
    out.push_back(ClassRegInfo{CID_ObjectSpawnerTemplate, "dice.hfe.world.ObjectTemplate.ObjectSpawner", &world::ClassFactory<world::ObjectSpawnerTemplate>::create});
    out.push_back(ClassRegInfo{CID_PlayerControlObjectTemplate, "dice.hfe.world.ObjectTemplate.PlayerControlObject", &world::ClassFactory<world::PlayerControlObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_ParachuteTemplate, "dice.hfe.world.ObjectTemplate.Parachute", &world::ClassFactory<world::ParachuteTemplate>::create});
#if DEDICATED
    out.push_back(ClassRegInfo{CID_KitPartTemplate, "dice.hfe.world.ObjectTemplate.KitPart", &world::ClassFactory<world::KitPartTemplate>::create});
    out.push_back(ClassRegInfo{CID_SupplyDepotTemplate, "dice.hfe.world.ObjectTemplate.SupplyDepot", &world::ClassFactory<world::SupplyDepotTemplate>::create});
#endif // Client presentation paths have not been attested by this server recovery.
    out.push_back(ClassRegInfo{CID_SupplyObjectTemplate, "dice.hfe.world.ObjectTemplate.SupplyObject", &world::ClassFactory<world::SupplyObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_ControlPointTemplate, "dice.hfe.world.ObjectTemplate.ControlPoint", &world::ClassFactory<world::ControlPointTemplate>::create});
    out.push_back(ClassRegInfo{CID_ObstacleTemplate, "dice.hfe.world.ObjectTemplate.Obstacle", &world::ClassFactory<world::ObstacleTemplate>::create});
    out.push_back(ClassRegInfo{CID_AreaObjectTemplate, "dice.hfe.world.ObjectTemplate.AreaObject", &world::ClassFactory<world::AreaObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_TargetObjectTemplate, "dice.hfe.world.ObjectTemplate.TargetObject", &world::ClassFactory<world::TargetObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_LadderTemplate, "dice.hfe.world.ObjectTemplate.Ladder", &world::ClassFactory<world::LadderTemplate>::create});
    out.push_back(ClassRegInfo{CID_LadderContainerTemplate, "dice.hfe.world.ObjectTemplate.LadderContainer", &world::ClassFactory<world::LadderContainerTemplate>::create});
    out.push_back(ClassRegInfo{CID_ZiplineRopeTemplate, "dice.hfe.world.ObjectTemplate.ZiplineRope", &world::ClassFactory<world::ZiplineRopeTemplate>::create});
    out.push_back(ClassRegInfo{CID_ZiplineTemplate, "dice.hfe.world.ObjectTemplate.Zipline", &world::ClassFactory<world::ZiplineTemplate>::create});
    out.push_back(ClassRegInfo{CID_ZiplineContainerTemplate, "dice.hfe.world.ObjectTemplate.ZiplineContainer", &world::ClassFactory<world::ZiplineContainerTemplate>::create});
    out.push_back(ClassRegInfo{CID_GrapplingHookRopeTemplate, "dice.hfe.world.ObjectTemplate.GrapplingHookRope", &world::ClassFactory<world::GrapplingHookRopeTemplate>::create});
    out.push_back(ClassRegInfo{CID_RopeLinkTemplate, "dice.hfe.world.ObjectTemplate.RopeLink", &world::ClassFactory<world::RopeLinkTemplate>::create});
    out.push_back(ClassRegInfo{CID_HookLinkTemplate, "dice.hfe.world.ObjectTemplate.HookLink", &world::ClassFactory<world::HookLinkTemplate>::create});
    out.push_back(ClassRegInfo{CID_GrapplingHookRopeContainerTemplate, "dice.hfe.world.ObjectTemplate.GrapplingHookRopeContainer", &world::ClassFactory<world::GrapplingHookRopeContainerTemplate>::create});
    out.push_back(ClassRegInfo{CID_RemoteControlledObjectTemplate, "dice.hfe.world.ObjectTemplate.RemoteControlledObject", &world::ClassFactory<world::RemoteControlledObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_TurnableRemoteControlledObjectTemplate, "dice.hfe.world.ObjectTemplate.TurnableRemoteControlledObject", &world::ClassFactory<world::TurnableRemoteControlledObjectTemplate>::create});
    out.push_back(ClassRegInfo{CID_TriggerTemplate, "dice.hfe.world.ObjectTemplate.Trigger", &world::ClassFactory<world::TriggerTemplate>::create});
    out.push_back(ClassRegInfo{CID_AmbientEffectAreaTemplate, "dice.hfe.world.ObjectTemplate.AmbientEffectArea", &world::ClassFactory<world::AmbientEffectAreaTemplate>::create});
    out.push_back(ClassRegInfo{CID_TriggerableTemplate, "dice.hfe.world.ObjectTemplate.Triggerable", &world::ClassFactory<world::TriggerableTemplate>::create});
    out.push_back(ClassRegInfo{CID_TriggerableTargetTemplate, "dice.hfe.world.ObjectTemplate.TriggerableTarget", &world::ClassFactory<world::TriggerableTargetTemplate>::create});
    out.push_back(ClassRegInfo{CID_UAVVehicleTemplate, "dice.hfe.world.ObjectTemplate.UAVVehicle", &world::ClassFactory<world::UAVVehicleTemplate>::create});
    out.push_back(ClassRegInfo{CID_DropVehicleTemplate, "dice.hfe.world.ObjectTemplate.DropVehicle", &world::ClassFactory<world::DropVehicleTemplate>::create});

    // Weapon components
    out.push_back(ClassRegInfo{CID_SingleFireCompTemplate, "dice.hfe.world.TemplateComponent.SingleFireComp", &world::ClassFactory<world::SingleFireCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_MultiFireCompTemplate, "dice.hfe.world.TemplateComponent.MultiFireComp", &world::ClassFactory<world::MultiFireCompTemplate>::create});
    // Weapon component templates; HUD and projectile components are registered separately.
    out.push_back(ClassRegInfo{CID_DefaultAmmoCompTemplate, "dice.hfe.world.TemplateComponent.DefaultAmmoComp", &world::ClassFactory<world::DefaultAmmoCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_ReplenishingAmmoCompTemplate, "dice.hfe.world.TemplateComponent.ReplenishingAmmoComp", &world::ClassFactory<world::ReplenishingAmmoCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_DefaultZoomCompTemplate, "dice.hfe.world.TemplateComponent.DefaultZoomComp", &world::ClassFactory<world::DefaultZoomCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_ToggleCameraCompTemplate, "dice.hfe.world.TemplateComponent.ToggleCameraComp", &world::ClassFactory<world::ToggleCameraCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_DefaultSoundCompTemplate, "dice.hfe.world.TemplateComponent.DefaultSoundComp", &world::ClassFactory<world::DefaultSoundCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_SoldierBasedRecoilCompTemplate, "dice.hfe.world.TemplateComponent.SoldierBasedRecoilComp", &world::ClassFactory<world::SoldierBasedRecoilCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_WeaponBasedRecoilCompTemplate, "dice.hfe.world.TemplateComponent.WeaponBasedRecoilComp", &world::ClassFactory<world::WeaponBasedRecoilCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_SoldierDeviationCompTemplate, "dice.hfe.world.TemplateComponent.SoldierDeviationComp", &world::ClassFactory<world::SoldierDeviationCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_SimpleDeviationCompTemplate, "dice.hfe.world.TemplateComponent.SimpleDeviationComp", &world::ClassFactory<world::SimpleDeviationCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_TargetDeviationCompTemplate, "dice.hfe.world.TemplateComponent.TargetDeviationComp", &world::ClassFactory<world::TargetDeviationCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_DefaultAnimationCompTemplate, "dice.hfe.world.TemplateComponent.DefaultAnimationComp", &world::ClassFactory<world::DefaultAnimationCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_ThrownAnimationCompTemplate, "dice.hfe.world.TemplateComponent.ThrownAnimationComp", &world::ClassFactory<world::ThrownAnimationCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_MultiFireArmTargetCompTemplate, "dice.hfe.world.TemplateComponent.MultiFireArmTargetComp", &world::ClassFactory<world::MultiFireArmTargetCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_LPTargetCompTemplate, "dice.hfe.world.TemplateComponent.LPTargetComp", &world::ClassFactory<world::LPTargetCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_SimpleTargetCompTemplate, "dice.hfe.world.TemplateComponent.SimpleTargetComp", &world::ClassFactory<world::SimpleTargetCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_LPFireCompTemplate, "dice.hfe.world.TemplateComponent.LPFireComp", &world::ClassFactory<world::LPFireCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_SpawnObjectFireCompTemplate, "dice.hfe.world.TemplateComponent.SpawnObjectFireComp", &world::ClassFactory<world::SpawnObjectFireCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_ThrownFireCompTemplate, "dice.hfe.world.TemplateComponent.ThrownFireComp", &world::ClassFactory<world::ThrownFireCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_ExplosivesFireCompTemplate, "dice.hfe.world.TemplateComponent.ExplosivesFireComp", &world::ClassFactory<world::ExplosivesFireCompTemplate>::create});

    // Projectiles and their components
    out.push_back(ClassRegInfo{CID_GenericProjectileTemplate, "dice.hfe.world.ObjectTemplate.GenericProjectile", &world::ClassFactory<world::GenericProjectileTemplate>::create});
    // RW-6b is retired for this projectile family: publication requires every dispatched damage,
    // ricochet, penetration and hit-indicator method, not merely a clean unresolved-symbol delta.
    // Console properties are registered by ModuleProjectileComp.
    out.push_back(ClassRegInfo{CID_DefaultCollisionCompTemplate, "dice.hfe.world.TemplateComponent.DefaultCollisionComp", &world::ClassFactory<world::DefaultCollisionCompTemplate>::create});
    // The three specialised collision leaves, their full console vocabulary and their typed
    // replenish/resurrect/stick runtime chains are recovered from both shipped x86 images.
    out.push_back(ClassRegInfo{CID_ReplenishCollisionCompTemplate, "dice.hfe.world.TemplateComponent.ReplenishCollisionComp", &world::ClassFactory<world::ReplenishCollisionCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_ResurrectCollisionCompTemplate, "dice.hfe.world.TemplateComponent.ResurrectCollisionComp", &world::ClassFactory<world::ResurrectCollisionCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_StickyCollisionCompTemplate, "dice.hfe.world.TemplateComponent.StickyCollisionComp", &world::ClassFactory<world::StickyCollisionCompTemplate>::create});
    // Both images publish detonation templates. RW-6b's client publication hold is retired under RW-253:
    // client explosion/effect bodies are implemented, and replenish still refuses the client side.
    out.push_back(ClassRegInfo{CID_DefaultDetonationCompTemplate, "dice.hfe.world.TemplateComponent.DefaultDetonationComp", &world::ClassFactory<world::DefaultDetonationCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_ReplenishDetonationCompTemplate, "dice.hfe.world.TemplateComponent.ReplenishDetonationComp", &world::ClassFactory<world::ReplenishDetonationCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_DefaultProjectileSoundCompTemplate, "dice.hfe.world.TemplateComponent.DefaultProjSoundComp", &world::ClassFactory<world::DefaultProjectileSoundCompTemplate>::create});
    // Complete runtime/template/interface family and all three shipped console properties recovered
    // from both ports. The named id is CID_DefaultPenetrateCompTemplate == 38206 (not 38207).
    out.push_back(ClassRegInfo{CID_DefaultPenetrateCompTemplate, "dice.hfe.world.TemplateComponent.DefaultPenetrateComp", &world::ClassFactory<world::DefaultPenetrateCompTemplate>::create});
    // Complete typed bounce, dynamic-target seek and guided-follow leaves recovered from PE/ELF.
    out.push_back(ClassRegInfo{CID_DefaultRicochetCompTemplate, "dice.hfe.world.TemplateComponent.DefaultRicochetComp", &world::ClassFactory<world::DefaultRicochetCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_SeekClosestTargetCompTemplate, "dice.hfe.world.TemplateComponent.SeekClosestTargetComp", &world::ClassFactory<world::SeekClosestTargetCompTemplate>::create});
    // Complete runtime/template family and both stock seek properties recovered from PE/ELF.
    out.push_back(ClassRegInfo{CID_FixedTargetCompTemplate, "dice.hfe.world.TemplateComponent.FixedTargetComp", &world::ClassFactory<world::FixedTargetCompTemplate>::create});
    out.push_back(ClassRegInfo{CID_DefaultFollowCompTemplate, "dice.hfe.world.TemplateComponent.DefaultFollowComp", &world::ClassFactory<world::DefaultFollowCompTemplate>::create});
    // Complete state-free template and debug-only ballistic follow runtime recovered from PE/ELF.
    out.push_back(ClassRegInfo{CID_PreDestinedFollowCompTemplate, "dice.hfe.world.TemplateComponent.PreDestinedFollowComp", &world::ClassFactory<world::PreDestinedFollowCompTemplate>::create});
}
void GameLogicObjTemplBFModule::getSingletonList(std::vector<SingletonRegInfo>&) {}

} // namespace world

// Module initialization/closure preserves each module's console-list and subsystem behavior.
// BFMaterialSystemModule, ScoreSystemModule and SpawnSystemModule register their lists but do not
// unregister them on close; preserve that asymmetry.

namespace {

// [INFERRED] shape; the addresses are read. A std::list<ConsoleObject*> in the original.
typedef std::list<ConsoleObject*> ConsoleObjectList;

// [CHOSEN, not recovered] Maps original list identities to native list objects.
// Repeated keys must return the same list for matching module init/close and shared registrations.
// Function-static storage avoids initialization-order dependencies. Unpopulated lists remain empty;
// their successful registration does not establish that the corresponding commands are available.
inline ConsoleObjectList& listAt(unsigned int addr)
{
    static std::map<unsigned int, ConsoleObjectList> lists;
    return lists[addr];
}

} // anonymous namespace

// The 37-byte shape  -  28 of the 58.
#define BF2_MODULE_CONSOLE_INIT(cls, addr)                                          \
    bool cls::init()  { consoleObjects()->registerConsoleObjects(listAt(addr));   \
                        return true; }
#define BF2_MODULE_CONSOLE_CLOSE(cls, addr)                                         \
    bool cls::close() { consoleObjects()->unregisterConsoleObjects(listAt(addr)); \
                        return true; }
// Module lifecycle methods with no initialization or shutdown work.
#define BF2_MODULE_NOOP_INIT(cls)   bool cls::init()  { return true; }
#define BF2_MODULE_NOOP_CLOSE(cls)  bool cls::close() { return true; }

// Registers engine console commands, including the native bindings associated with the original list.
bool ModuleBF2::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8AD45A8));
    consoleObjects()->registerConsoleObjects(gameConsoleObjects());
    return true;
}
bool ModuleBF2::close()
{
    consoleObjects()->unregisterConsoleObjects(listAt(0x8AD45A8));
    consoleObjects()->unregisterConsoleObjects(gameConsoleObjects());
    return true;
}

// The one module that hooks the event system for itself. It registers `this + 4`  -  its
// IEventListener subobject  -  which is what proves the multiple inheritance recorded in Modules.h.
bool ModuleGame::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8B1704C));
    // 0x8B1704C is the hudInformationLayer TU's list (map/sv/python/renderer/SquadManager
    // defaults); this tree's recovered sv bindings live in Game/GameServer/ServerSettingsConsole.cpp.
    consoleObjects()->registerConsoleObjects(serverSettingsConsoleObjects());
    // Map-list bindings execute maplist.con commands.
    consoleObjects()->registerConsoleObjects(mapListConsoleObjects());
    // the `GameLogic` object - and it is load-bearing, not cosmetic: `gameLogic.setKit` is
    // driven fourteen times per level from server/Init.con, and without this registration those
    // lines are a silent no-op (an unmatched console object is not an error) and the kit maps
    // stay empty. See Game/GameLogic/GameLogicConsole.cpp.
    consoleObjects()->registerConsoleObjects(gameLogicConsoleObjects());
    // Combat-area bindings apply the authored battlefield boundaries from level scripts.
    consoleObjects()->registerConsoleObjects(combatAreaConsoleObjects());
#if !DEDICATED
    // PE4107C3 registers list 0x98913C before the event handlers. These four
    // static command constructors append to that same ModuleGame list.
    consoleObjects()->registerConsoleObjects(menuTeamConsoleObjects());
    consoleObjects()->registerConsoleObjects(swiffHostConsoleObjects());
    // Renderer screen-mode console bindings.
    consoleObjects()->registerConsoleObjects(bf2RenderConsoleObjects());
    // Profile-file bindings populate account names and settings. [INFERRED] Module placement.
    consoleObjects()->registerConsoleObjects(localProfileConsoleObjects());
    // Bindings used by Audio, Video, General, Haptic and ServerSettings profile files.
    consoleObjects()->registerConsoleObjects(settingsConsoleObjects());
#endif
    eventManager->registerEventHandler((EventCategory)10, this, 0);
    eventManager->registerEventHandler((EventCategory)0,  this, 0);
    eventManager->registerEventHandler((EventCategory)5,  this, 0);
    m_appActive = true;                                     // [this+0x0C] - see Modules.h
    return true;
}

// Handles client loading and progress events. The dedicated image has no menu behavior.
// Client progress updates use a literal 1.0 delta and require the application to be active.
void ModuleGame::handleEvent(EventCategory category, unsigned int id, EventNode*, void* data)
{
#if !DEDICATED
#  if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] Editor builds ignore menu events to avoid constructing game UI and input devices.
    (void)category;
    (void)id;
    (void)data;
    return;
#  endif
    if (isDedicated())
        return;

    if (category == ECCore)
    {
        if (id == 15)                                       // CEActivateApp
            m_appActive = *static_cast<bool*>(data);
        return;
    }

    if (category == static_cast<EventCategory>(10))         // ECMenu
    {
        switch (id)
        {
        case 3:                                             // ECMenuMenuEvent_InitLoadingScreen
            if (menuLogic)
            {
                menuLogic->release();
                menuLogic = 0;
            }
            menuLogic = static_cast<IMenuLogic*>(
                classManager->createInstance(CID_BetweenRoundsLogic, IID_IMenuLogic, 0));
            menuLogic->init();                              // +0x14, unguarded as shipped
            {
                float progress = 0.0f;
                eventManager->postEvent(static_cast<EventCategory>(10), 4, &progress, 0.0f);
            }
            return;

        case 4:                                             // ECMenuMenuEvent_UpdateLoadingScreen
            if (menuLogic && m_appActive)
            {
                menuLogic->update(1.0);                     // +0x28, the literal 1.0
                if (menuLogic->beginFrame())                // +0x2C
                {
                    menuLogic->display();                   // +0x30
                    menuLogic->endFrame();                  // +0x34
                }
            }
            return;

        case 5:                                             // ECMenuMenuEvent_RemoveLoadingScreen
        case 10:                                            // ECMenuMenuEvent_DisableBetweenRounds
            if (menuLogic)
            {
                menuLogic->release();
                menuLogic = 0;
            }
            return;

        case 6:                                             // ECMenuMenuEvent_SetLoadingScreenDebug
            loadStat->executeLoadCallback();
            return;

        case 9:                                             // ECMenuMenuEvent_EnableBetweenRounds
            if (menuLogic)
            {
                menuLogic->release();
                menuLogic = 0;
            }
            menuLogic = static_cast<IMenuLogic*>(
                classManager->createInstance(CID_BetweenRoundsLogic, IID_IMenuLogic, 0));
            menuLogic->init();
            return;

        default:
            return;
        }
    }

    if (category == ECGameLogic)
    {
        // GLGameStatus uses an integer payload; value 2 identifies the handled state.
        if (id == 1 && *static_cast<int*>(data) == 2)
        {
            if (g_localProfileManager)
            {
                const float played =
                    static_cast<ILocalProfileManager*>(g_localProfileManager)->getTotalPlayedTime();   // +0xB4
                static_cast<ILocalProfileManager*>(g_localProfileManager)->setTotalPlayedTime(
                    static_cast<float>(WorldPref::getWorldTime() + played));                           // +0xB8
            }
        }
    }
#else
    (void)category;
    (void)id;
    (void)data;
#endif
}
bool ModuleGame::close()
{
    //  reverse order  -  5, 0, 10 against init's 10, 0, 5. Transcribed as read.
    eventManager->unregisterEventHandler((EventCategory)5,  this);
    eventManager->unregisterEventHandler((EventCategory)0,  this);
    eventManager->unregisterEventHandler((EventCategory)10, this);
    consoleObjects()->unregisterConsoleObjects(listAt(0x8B1704C));
    consoleObjects()->unregisterConsoleObjects(serverSettingsConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(mapListConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(gameLogicConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(combatAreaConsoleObjects());
#if !DEDICATED
    consoleObjects()->unregisterConsoleObjects(menuTeamConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(localProfileConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(swiffHostConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(bf2RenderConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(settingsConsoleObjects());
#endif
    return true;
}

bool ModuleGameLogic::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8ACC02C));
    // Register admin level, ban, kick and player-list commands in their original order.
    consoleObjects()->registerConsoleObjects(banListConsoleObjects());
    // Register the player class used by game logic.
    world::playerManager->registerPlayerClass(world::CID_Player, std::string("Player"));
    return true;
}
bool ModuleGameLogic::close()
{
    consoleObjects()->unregisterConsoleObjects(listAt(0x8ACC02C));
    consoleObjects()->unregisterConsoleObjects(banListConsoleObjects());
    return true;
}

BF2_MODULE_NOOP_INIT (ModuleNetworkables)
BF2_MODULE_NOOP_CLOSE(ModuleNetworkables)

bool MathModule::init()
{
    initMath();
    // ELF 0x88184F0 / dedicated PE 0x689F3D. The const-reference literals
    // are suffix-merged; the full bytes, not IDA's short labels, establish them.
    console::manager->registerNameForType("Vec2",        "vec2");
    console::manager->registerNameForType("Vec2&",       "vec2");
    console::manager->registerNameForType("const Vec2&", "vec2");
    console::manager->registerNameForType("Vec3",        "vec3");
    console::manager->registerNameForType("Vec3&",       "vec3");
    console::manager->registerNameForType("const Vec3&", "vec3");
    console::manager->registerNameForType("Vec4",        "vec4");
    console::manager->registerNameForType("Vec4&",       "vec4");
    console::manager->registerNameForType("const Vec4&", "vec4");
    return true;
}
bool MathModule::close() { closeMath(); return true; }

#if !DEDICATED
static HMODULE s_audioLibrary = 0;                          // BF2.exe ds:0xA24CB0 - the BF2Audio.dll handle
#endif
bool BfSoundModule::init()
{
    // Refuses duplicate initialization and registers sound bindings and the Sound template.
    // Dedicated and noSound runs use the null facade; clients load BF2Audio and fall back to the null engine on failure.
    if (g_bfSound)                                          // ds:0x8AC0D80 - refuses to run twice,
        return false;                                       //   and says false when it does
    consoleObjects()->registerConsoleObjects(listAt(0x8AC0DBC));
    consoleObjects()->registerConsoleObjects(bfSoundConsoleObjects());   // the TU's recovered rows
    world::objectTemplateManager->registerTemplateClass(9986001u, std::string("Sound"));
    g_soundEngine = g_nullSoundEngine;                      // ds:0x8AC0D84 = the core null engine
#if DEDICATED
    g_bfSound = new BfSoundNull;                            // {vptr, refcount=1} - inlined ctor in
    return true;                                            //   the ELF; our ctor matches it
#else
    if (isDedicated() || *Vars::getInt("opt-nosound", 0) == 1)
    {
        g_bfSound = new BfSoundNull;                        // the 8-byte null facade
        return true;
    }
    BfSound* facade = new BfSound;
    g_bfSound = facade;
    s_audioLibrary = LoadLibraryA("BF2Audio.dll");
    if (s_audioLibrary)
    {
        typedef ISoundEngine* (*GetSoundEngineFn)();
        GetSoundEngineFn getSoundEngine =
            reinterpret_cast<GetSoundEngineFn>(GetProcAddress(s_audioLibrary, "getSoundEngine"));
        if (getSoundEngine)
            g_soundEngine = getSoundEngine();
        else
            Debug(DebugTypeError, "Sound/BfSound.cpp", 1849, "")
                << "Invalid " << "BF2Audio.dll" << ". Function getSoundEngine not found.";
        facade->init();
    }
    else
    {
        Debug(DebugTypeError, "Sound/BfSound.cpp", 1854, "") << "BF2Audio.dll" << " not found";
        facade->init();
    }
    return true;
#endif
}
bool BfSoundModule::close()
{
    // Deletes the sound facade regardless of its reference count, clears the global, then unregisters bindings.
    // Client shutdown also shuts down the DLL engine and preserves the existing repeated FreeLibrary call.
    if (g_bfSound)                                          // ds:0x8AC0D80
        delete g_bfSound;                                   // virtual delete = the +0x14 dispatch
    g_bfSound = 0;
#if !DEDICATED
    if (s_audioLibrary)
    {
        typedef void (*ShutdownSoundEngineFn)();
        ShutdownSoundEngineFn shutdownSoundEngine =
            reinterpret_cast<ShutdownSoundEngineFn>(GetProcAddress(s_audioLibrary, "shutdownSoundEngine"));
        if (shutdownSoundEngine)
            shutdownSoundEngine();
        FreeLibrary(s_audioLibrary);
        if (s_audioLibrary)
            FreeLibrary(s_audioLibrary);
    }
#endif
    consoleObjects()->unregisterConsoleObjects(listAt(0x8AC0DBC));
    consoleObjects()->unregisterConsoleObjects(bfSoundConsoleObjects());
    g_soundEngine = 0;                                      // ds:0x8AC0D84
#if !DEDICATED
    s_audioLibrary = 0;
#endif
    return true;
}

bool SpawnSystemModule::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8AD3A08));
    // Registers spawn-manager and spawn-point-template commands.
    consoleObjects()->registerConsoleObjects(spawnSystemConsoleObjects());
    // Registers the SpawnPoint template name used by .con files.
    world::objectTemplateManager->registerTemplateClass(CID_SpawnPointTemplate,
                                                        std::string("SpawnPoint"));
    return true;
}
//  Registers at init and never unregisters. In the binary.
BF2_MODULE_NOOP_CLOSE(SpawnSystemModule)

// io
namespace io {

// Console constants preserve their integer, string and float registration paths.
// Numeric helpers use default ostringstream formatting, including six significant digits for floats.
void addConstantHelper(const std::string& name, unsigned int value)
{
    std::ostringstream text;
    text << value;
    addConstantToAllConsoles(name, text.str());
}

void addConstantFloatHelper(const std::string& name, float value)
{
    std::ostringstream text;
    text << value;
    addConstantToAllConsoles(name, text.str());
}

bool ModuleIO::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8AC8CB0));

    // [CHOSEN] ModuleIO registers file-manager commands until their original owning module is identified.
    consoleObjects()->registerConsoleObjects(io::fileManagerConsoleObjects());
    // Registers control-map commands alongside the file-manager commands from the same source unit.
    consoleObjects()->registerConsoleObjects(io::controlMapConsoleObjects());

    consoleObjects()->registerTypeConversion("string", "std::string");
    g_system->resetExactTime();

    // 9 string-valued constants
    static const char* const kStr[][2] = {
        { "c_Debug", "0" }, { "c_Release", "1" }, { "c_Xbox", "0" }, { "c_Win32", "1" },
        { "c_DTDebug", "0" }, { "c_DTInfo", "1" }, { "c_DTWarning", "2" },
        { "c_DTAssert", "3" }, { "c_DTError", "4" }
    };
    for (unsigned i = 0; i < sizeof(kStr) / sizeof(kStr[0]); ++i)
        addConstantToAllConsoles(std::string(kStr[i][0]), std::string(kStr[i][1]));

    // Console constants retain their original spellings and values.
    // c_NIGhostAlways is negative, which selects static rather than scopable network descriptors.
    addConstantFloatHelper(std::string("c_CMMinFallTime"), 0.001f);
    addConstantFloatHelper(std::string("c_CMNonRepetive"), 10000.0f);
    addConstantFloatHelper(std::string("c_CMPushAndHold"), 0.0f);
    addConstantFloatHelper(std::string("c_CMDoubleTap"),   1000.0f);
    addConstantFloatHelper(std::string("c_NIGhostAlways"), -1.0f);

    // 132 integer constants
    //  c_totalAccess is 7, not 3  -  the access levels are a bitmask (1|2|4), not a rank.
    //  c_PINumInput and c_PINone are both 64 (the None == count idiom).
    //  c_GI* and c_PI* overlap and mean different things: 37 is c_GIMenuSelect0 and also c_PILie.
    struct IntConst { const char* name; int value; };
    static const IntConst kInt[] = {
        { "c_noAccess", 0 }, { "c_endUserAccess", 1 }, { "c_fullAccess", 2 }, { "c_totalAccess", 7 },

        { "c_GIMenu", 0 }, { "c_GIToggleConsole", 1 }, { "c_GIEscape", 2 }, { "c_GIUp", 3 },
        { "c_GIDown", 4 }, { "c_GILeft", 5 }, { "c_GIRight", 6 }, { "c_GIPageUp", 7 },
        { "c_GIPageDown", 8 }, { "c_GIRightShift", 9 }, { "c_GILeftShift", 10 },
        { "c_GILeftCtrl", 11 }, { "c_GIRightCtrl", 12 }, { "c_GILeftAlt", 13 },
        { "c_GIRightAlt", 14 }, { "c_GIOk", 15 }, { "c_GIAltOk", 16 }, { "c_GICancel", 17 },
        { "c_GIScreenShot", 18 }, { "c_GITogglePause", 19 }, { "c_GIMouseLookX", 20 },
        { "c_GIMouseLookY", 21 }, { "c_GISayAll", 22 }, { "c_GISayTeam", 23 },
        { "c_GISaySquad", 24 }, { "c_GIMouseWheelUp", 25 }, { "c_GIMouseWheelDown", 26 },
        { "c_GIQuit", 27 }, { "c_GITab", 28 }, { "c_GIEnter", 29 }, { "c_GIDelete", 30 },
        { "c_GIBack", 31 }, { "c_GITacticalComm", 33 }, { "c_GIRadioComm", 34 },
        { "c_GIMapSize", 35 }, { "c_GIMapZoom", 36 },
        { "c_GIMenuSelect0", 37 }, { "c_GIMenuSelect1", 38 }, { "c_GIMenuSelect2", 39 },
        { "c_GIMenuSelect3", 40 }, { "c_GIMenuSelect4", 41 }, { "c_GIMenuSelect5", 42 },
        { "c_GIMenuSelect6", 43 }, { "c_GIMenuSelect7", 44 }, { "c_GIMenuSelect8", 45 },
        { "c_GIMenuSelect9", 46 }, { "c_GIYes", 47 }, { "c_GINo", 48 },
        { "c_GIAutoAccept", 49 }, { "c_GICreateSquad", 50 }, { "c_GIJoinSquad", 51 },
        { "c_GISelectSquad", 52 }, { "c_GIToggleFreeCamera", 53 }, { "c_GIHotRankUp", 54 },
        { "c_GIShowScoreboard", 55 }, { "c_GIVoipUseLeaderChannel", 56 }, { "c_GI3dMap", 57 },
        { "c_GISelectAll", 58 }, { "c_GIDeselectAll", 59 }, { "c_GILeader", 60 },
        { "c_GIVoipPushToTalk", 61 },
        //  32 is never registered, and None is 74 rather than 62  -  the enum has a gap and a tail
        // that this function does not name.
        { "c_GINone", 74 },

        { "c_PIYaw", 0 }, { "c_PIPitch", 1 }, { "c_PIRoll", 2 }, { "c_PIThrottle", 3 },
        { "c_PIMouseLookX", 4 }, { "c_PIMouseLookY", 5 }, { "c_PICameraX", 6 },
        { "c_PICameraY", 7 }, { "c_PIFire", 8 }, { "c_PIAction", 9 }, { "c_PIUse", 10 },
        { "c_PIMouseLook", 11 }, { "c_PIAltSprint", 12 }, { "c_PISprint", 13 },
        { "c_PIWeaponSelect1", 14 }, { "c_PIWeaponSelect2", 15 }, { "c_PIWeaponSelect3", 16 },
        { "c_PIWeaponSelect4", 17 }, { "c_PIWeaponSelect5", 18 }, { "c_PIWeaponSelect6", 19 },
        { "c_PIWeaponSelect7", 20 }, { "c_PIWeaponSelect8", 21 }, { "c_PIWeaponSelect9", 22 },
        { "c_PIPositionSelect1", 23 }, { "c_PIPositionSelect2", 24 }, { "c_PIPositionSelect3", 25 },
        { "c_PIPositionSelect4", 26 }, { "c_PIPositionSelect5", 27 }, { "c_PIPositionSelect6", 28 },
        { "c_PIPositionSelect7", 29 }, { "c_PIPositionSelect8", 30 },
        { "c_PIAltFire", 31 }, { "c_PIReload", 32 }, { "c_PISelectFunc", 33 }, { "c_PIDrop", 34 },
        { "c_PIToggleCameraMode", 35 }, { "c_PIToggleCamera", 36 },
        { "c_PILie", 37 }, { "c_PICrouch", 38 },
        { "c_PICameraMode1", 39 }, { "c_PICameraMode2", 40 }, { "c_PICameraMode3", 41 },
        { "c_PICameraMode4", 42 }, { "c_PISelectPrimWeapon", 43 }, { "c_PISelectSecWeapon", 44 },
        { "c_PIToggleWeapon", 45 }, { "c_PIToggleFireRate", 46 }, { "c_PIFlareFire", 47 },
        { "c_PIRadio1", 48 }, { "c_PIRadio2", 49 }, { "c_PIRadio3", 50 }, { "c_PIRadio4", 51 },
        { "c_PIRadio5", 52 }, { "c_PIRadio6", 53 }, { "c_PIRadio7", 54 }, { "c_PIRadio8", 55 },
        { "c_PIScreenShot", 56 }, { "c_PIToolTip", 57 }, { "c_PISayAll", 58 },
        { "c_PISayTeam", 59 }, { "c_PINextItem", 60 }, { "c_PIPrevItem", 61 },
        { "c_PICommunication", 62 }, { "c_PIShowScoreBoard", 63 },
        { "c_PINumInput", 64 }, { "c_PINone", 64 }
    };
    for (unsigned i = 0; i < sizeof(kInt) / sizeof(kInt[0]); ++i)
        addConstantHelper(std::string(kInt[i].name), kInt[i].value);

    return true;
}
BF2_MODULE_CONSOLE_CLOSE(ModuleIO, 0x8AC8CB0)

BF2_MODULE_NOOP_INIT (FileModule)
BF2_MODULE_NOOP_CLOSE(FileModule)

// Registers Network and NetworkableInfo console bindings.
bool NetworkModule::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8AC8704));
    consoleObjects()->registerConsoleObjects(networkConsoleObjects());
    return true;
}
bool NetworkModule::close()
{
    consoleObjects()->unregisterConsoleObjects(listAt(0x8AC8704));
    consoleObjects()->unregisterConsoleObjects(networkConsoleObjects());
    return true;
}

// PE57E764 supplies both slots; ELF84FDA00/84FDA10 also return true without work.
BF2_MODULE_NOOP_INIT (InputDX8Module)
BF2_MODULE_NOOP_CLOSE(InputDX8Module)

} // namespace io

// console / tool / animationTools / ai
namespace console {

// Teaches the console type-name table that three spellings of std::string all display as "string".

//  the third literal is "const std::string&", not the "const " that IDA labels at that address.
// The shorter symbol is a suffix-merged pointer into the middle of the longer string; reading the
// bytes settles it. This is the same suffix-merge trap recorded elsewhere in this tree, and taking
// the label at face value would have produced a plausible, wrong registration.
bool ModuleConsole::init()
{
    console::manager->registerNameForType("std::string",        "string");
    console::manager->registerNameForType("std::string&",       "string");
    console::manager->registerNameForType("const std::string&", "string");
    return true;
}
BF2_MODULE_NOOP_CLOSE(ModuleConsole)

} // namespace console

namespace tool {
// Registers Object, ObjectTemplate and Editor tool commands used by level scripts.
bool ModuleToolObject::init()
{
    consoleObjects()->registerConsoleObjects(toolObjectConsoleObjects());
    return true;
}
bool ModuleToolObject::close()
{
    consoleObjects()->unregisterConsoleObjects(toolObjectConsoleObjects());
    return true;
}
}

namespace animationTools {

// Defined in Animation/BoneAnimation/ModuleBoneAnimation.cpp - this module's console binding
// list, the stand-in for the original's ds:0x8B22A24.
ConsoleObjectList& boneAnimationConsoleObjects();

// Creates the animation managers and registers this module's adjusted event-listener interface.
// These managers are constructed here rather than through singleton registry rows.
// The module and its template manager register separate handlers on ECCore; preserve their order.
bool AnimModule::init()
{
    // Register animation-system console bindings before initializing animation storage.
    consoleObjects()->registerConsoleObjects(animationTools::boneAnimationConsoleObjects());

    anim::Skeleton::initMemory();

    // Create value holders before any animation script can load.
    anim::valueHolderManager = new anim::ValueHolderManager();         // ds:0x8ABA1B0, 20 B
    anim::animationManager = new anim::AnimationManager();             // ds:0x8AB0F60, 24 B
    anim::animationSystemTemplateManager = new anim::AnimationSystemTemplateManager();

    anim::RagDollTemplate::init();

    // Registers the unload listener that resets RagDollTemplate. GameLogic must destroy objects before
    // posting unload; template shutdown refuses surviving ragdolls.
    if (!eventManager->registerEventHandler(static_cast<EventCategory>(5), this, 0))
    {
        Debug(DEBUGTYPE_6, "Animation/BoneAnimation/ModuleBoneAnimation.cpp", 255, "")
            << "Failed to register AnimModuleEventListener as event handler for ECCore.";
    }
    return true;
}

// Unregisters console bindings and the event handler before shutting down template subsystems.
// Managers use virtual deletion except ValueHolderManager's explicit teardown. Globals are not nulled.
bool AnimModule::close()
{
    consoleObjects()->unregisterConsoleObjects(animationTools::boneAnimationConsoleObjects());
    eventManager->unregisterEventHandler((EventCategory)5, this);

    anim::RagDollTemplate::shutdown();
    anim::Skeleton::closeMemory();

    // Delete the owned animation managers through their virtual destructors.
    if (anim::animationManager)
        delete anim::animationManager;
    if (anim::animationSystemTemplateManager)
        delete anim::animationSystemTemplateManager;                     // vtable +0x04
    // the ODD one out, and faithfully so: ValueHolderManager has no vtable, so the ELF calls
    // ~ValueHolderManager directly and then operator delete. `delete` compiles to exactly that.
    if (anim::valueHolderManager)
        delete anim::valueHolderManager;
    return true;
}

// ELF 0x8818AC0 / dedicated PE 0x70D654. ECCore 13 recreates the ragdoll template;
// other categories/ids have no effect. Preserve shutdown-before-init and both missing guards.
void AnimModule::handleEvent(EventCategory category, unsigned int id, EventNode*, void*)
{
    if (category == static_cast<EventCategory>(5) && id == 13)
    {
        anim::RagDollTemplate::shutdown();
        anim::RagDollTemplate::init();
    }
}

} // namespace animationTools

namespace ai {

// The engine/aidll bridge. Empty aidll is the shipped graceful no-bot path; every allocation is
// inside BFConsole's first-init guard and publishes itself through ClassManager rather than through
// ModuleRegistry factories.
bool AIInterfaceModule::initAIConnection()
{
    std::string dll = settings->stringGet(std::string("AIDLL"), std::string(""));
    if (dll.empty())
        return false;

    IAIConnection* connection = static_cast<IAIConnection*>(
        classManager->getSingleton(std::string("AIConnection")));
    if (!connection)
    {
#if defined(_MSC_VER)
        __debugbreak();
#else
        std::abort();
#endif
    }

    if (!connection->init(dll))
        return false;

    if (!BFConsole::m_instance)
    {
        BFConsole* console = new BFConsole;
        checkPointer(console);
        BFModuleCourier* courier = new BFModuleCourier;
        checkPointer(courier);
        BFSingletonServerUpdater* updater = new BFSingletonServerUpdater;
        checkPointer(updater);
    }

    AIInstanceServer::updateInstance();
    BFSingletonServer::updateStaticMethods();
    return true;
}

bool AIInterfaceModule::closeAIConnection()
{
    IAIConnection* connection = static_cast<IAIConnection*>(
        classManager->getSingleton(std::string("AIConnection")));
    if (connection)
    {
        classManager->unregisterSingleton(std::string("AIConnection"));

        if (BFConsole::m_instance)
            BFConsole::m_instance->clearSingletons();
        delete BFConsole::m_instance;

        if (CommonSingletonServer::m_iBFModuleCourier)
            delete static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier);

        ISingletonServerUpdater* updater = static_cast<ISingletonServerUpdater*>(
            classManager->getSimpleSingleton(std::string("BFSingletonServerUpdater")));
        if (updater)
            delete updater;

        BFSingletonServer::clearAllStaticMethods();
        AIInstanceServer::updateInstance();
    }
    return true;
}

bool AIInterfaceModule::reset()
{
    if (g_aiInstance)
    {
        IAIConnection* connection = static_cast<IAIConnection*>(
            classManager->getSingleton(std::string("AIConnection")));
        BFConsole::m_instance->setSkipBulkClear(true);
        connection->clear();
        BFConsole::m_instance->reset();
        AIInstanceServer::updateInstance();
        BFSingletonServer::updateStaticMethods();
    }
    return true;
}

//  the only init in the engine that can fail. Every other module returns true unconditionally.
// This one returns false when the AI connection does not come up, and BF2EngineSetup::initModules
// then closes and unregisters it while still returning true overall.

// The console objects register only on the success path, and only once  -  the flag at +0x0C guards
// it, and close() re-arms the flag so a later init registers again.
bool AIInterfaceModule::init()
{
    if (!initAIConnection())
        return false;

    if (m_needsConsoleRegister)                             // [this+0x0C]
    {
        consoleObjects()->registerConsoleObjects(listAt(0x8B29AE8));
        consoleObjects()->registerConsoleObjects(kitTemplateConsoleObjects());
        m_needsConsoleRegister = false;
    }
    return true;
}
bool AIInterfaceModule::close()
{
    closeAIConnection();
    consoleObjects()->unregisterConsoleObjects(listAt(0x8B29AE8));
    consoleObjects()->unregisterConsoleObjects(kitTemplateConsoleObjects());
    m_needsConsoleRegister = true;
    return true;
}

} // namespace ai

// world
namespace world {

// 1,926 bytes: twelve registerTypeConversion calls. These are what let a .con script write `object`
// where the engine wants an IObject*, and they are why console properties can pass object and
// template handles around as text.
bool Module::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8B1B944));
    // 0x8B1B944 is the objectManager TU's list; this tree's recovered bindings for that TU
    // (objectTemplate.create) live in Scene/Object/ObjectManagerConsole.cpp.
    consoleObjects()->registerConsoleObjects(objectManagerConsoleObjects());

    static const char* const kConversions[][2] = {
        { "object",         "IObject*"                              },
        { "object",         "IObject_ptrproxy"                      },
        { "object",         "world::IObject*"                       },
        { "object",         "world::IObject_ptrproxy"               },
        { "objectTemplate", "IObjectTemplate_ptrproxy"              },
        { "objectTemplate", "world::IObjectTemplate*"               },
        { "objectTemplate", "world::IObjectTemplate_ptrproxy"       },
        { "objectTemplate", "const IObjectTemplate*"                },
        { "objectTemplate", "const IObjectTemplate_ptrproxy"        },
        { "objectTemplate", "const world::IObjectTemplate*"         },
        { "objectTemplate", "const world::IObjectTemplate_ptrproxy" }
    };
    //  eleven pairs here against twelve calls in the binary. The extracted literal sequence has one
    // "objectTemplate" without a partner, so one pair could not be reconstructed unambiguously.
    // Left short rather than padded with a plausible twelfth  -  the missing one is almost certainly
    // "objectTemplate"/"IObjectTemplate*", but almost certainly is not read.
    for (unsigned i = 0; i < sizeof(kConversions) / sizeof(kConversions[0]); ++i)
        consoleObjects()->registerTypeConversion(kConversions[i][0], kConversions[i][1]);
    return true;
}
bool Module::close()
{
    consoleObjects()->unregisterConsoleObjects(listAt(0x8B1B944));
    consoleObjects()->unregisterConsoleObjects(objectManagerConsoleObjects());
    return true;
}

BF2_MODULE_CONSOLE_INIT (ObjectModule, 0x8B1ECA8)
BF2_MODULE_CONSOLE_CLOSE(ObjectModule, 0x8B1ECA8)

BF2_MODULE_CONSOLE_INIT (ObjTemplPortalModule, 0x8B1ED88)
BF2_MODULE_CONSOLE_CLOSE(ObjTemplPortalModule, 0x8B1ED88)

BF2_MODULE_CONSOLE_INIT (ObjTemplBaseModule, 0x8B1ED68)
BF2_MODULE_CONSOLE_CLOSE(ObjTemplBaseModule, 0x8B1ED68)

// Registers terrain and collision-template console commands.
bool PhysicsModule::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8B278D8));
    consoleObjects()->registerConsoleObjects(heightmapConsoleObjects());
    consoleObjects()->registerConsoleObjects(collisionManagerConsoleObjects());
    consoleObjects()->registerConsoleObjects(physicsConsoleObjects());
    return true;
}
bool PhysicsModule::close()
{
    consoleObjects()->unregisterConsoleObjects(listAt(0x8B278D8));
    consoleObjects()->unregisterConsoleObjects(heightmapConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(collisionManagerConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(physicsConsoleObjects());
    return true;
}

// Registers Material and MaterialManager console commands.
bool BFMaterialSystemModule::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8AD2828));
    consoleObjects()->registerConsoleObjects(materialSystemConsoleObjects());
    return true;
}
BF2_MODULE_NOOP_CLOSE   (BFMaterialSystemModule)          //  registers, never unregisters   (PE 0x4DA630: `mov al, 1`)

// Registers score-system console commands.
bool ScoreSystemModule::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8AD1748));
    consoleObjects()->registerConsoleObjects(scoreSystemConsoleObjects());
    return true;
}
BF2_MODULE_NOOP_CLOSE   (ScoreSystemModule)               //  registers, never unregisters

// Registers HUD component commands. Dedicated builds include only the shared strategic properties.
bool HudCompModule::init()
{
    consoleObjects()->registerConsoleObjects(hudCompConsoleObjects());
    return true;
}
bool HudCompModule::close()
{
    consoleObjects()->unregisterConsoleObjects(hudCompConsoleObjects());
    return true;
}

// Registers armor-component console commands.
bool GameLogicCompModule::init()
{
    consoleObjects()->registerConsoleObjects(listAt(0x8B0A564));
    consoleObjects()->registerConsoleObjects(gameLogicCompConsoleObjects());
    return true;
}
bool GameLogicCompModule::close()
{
    consoleObjects()->unregisterConsoleObjects(listAt(0x8B0A564));
    consoleObjects()->unregisterConsoleObjects(gameLogicCompConsoleObjects());
    return true;
}

// Template modules register console bindings before publishing their observed CID/type-name pairs.
struct TemplateClassReg { unsigned int cid; const char* name; };

#define BF2_REGISTER_TEMPLATE_NAMES(list, rows)                                          \
    consoleObjects()->registerConsoleObjects(listAt(list));                            \
    for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)                        \
        objectTemplateManager->registerTemplateClass(rows[i].cid,                        \
                                                     std::string(rows[i].name));         \
    return true;

bool GameLogicObjTemplBaseModule::init()
{
    // 0x8AF17A4 is this module TU's binding list; the tree's recovered bindings for it live in
    // the attested Game/Objects/Base/ModuleWorldObjTemplBase.cpp.
    consoleObjects()->registerConsoleObjects(objTemplBaseConsoleObjects());

    // nine pairs, in registration order.
    static const TemplateClassReg n[] = {
        { CID_BundleTemplate,           "Bundle" },
        { CID_DynamicBundleTemplate,    "DynamicBundle" },
        { CID_CameraTemplate,           "Camera" },
        { CID_EntryPointTemplate,       "EntryPoint" },
        { CID_RotationalBundleTemplate, "RotationalBundle" },
        { CID_SimpleObjectTemplate,     "SimpleObject" },
        { CID_FreeCameraTemplate,       "FreeCamera" },
        { CID_OverheadCameraTemplate,   "OverheadCamera" },
        { CID_LightSourceTemplate,      "LightSource" },
    };
    BF2_REGISTER_TEMPLATE_NAMES(0x8AF17A4, n)
}
BF2_MODULE_CONSOLE_CLOSE(GameLogicObjTemplBaseModule, 0x8AF17A4)

bool GameLogicObjTemplFxModule::init()
{
    consoleObjects()->registerConsoleObjects(objTemplFxConsoleObjects());
    static const TemplateClassReg n[] = {
        { CID_EffectBundleTemplate,          "EffectBundle" },
        { CID_WheelEffectBundleTemplate,     "WheelEffectBundle" },
        { CID_AirDraftEffectBundleTemplate,  "AirDraftEffectBundle" },
        { CID_GroundEffectBundleTemplate,    "GroundEffectBundle" },
        { CID_EmitterTemplate,               "Emitter" },
        { CID_ParticleTemplate,              "Particle" },
        { CID_SpriteParticleTemplate,        "SpriteParticle" },
        { CID_SpriteParticleSystemTemplate,  "SpriteParticleSystem" },
        { CID_MeshParticleSystemTemplate,    "MeshParticleSystem" },
        { CID_TrailSystemTemplate,           "TrailSystem" },
        { CID_ParticleSystemEmitterTemplate, "ParticleSystemEmitter" },
        { CID_NonScreenAlignedParticleSystemTemplate, "NonScreenAlignedParticleSystem" },
        { CID_DecalTemplate,                 "Decal" },
        { CID_EnvMapTemplate,                "EnvMap" },
    };
    BF2_REGISTER_TEMPLATE_NAMES(0x8AFACA4, n)
}
bool GameLogicObjTemplFxModule::close()
{
    consoleObjects()->unregisterConsoleObjects(objTemplFxConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(listAt(0x8AFACA4));
    return true;
}

bool GameLogicObjTemplVehicleModule::init()
{
    // Register vehicle-template bindings before engine-type constants and template names.
    consoleObjects()->registerConsoleObjects(objTemplVehicleConsoleObjects());

    // Engine-type constants are substituted before typed console extraction.
    // NewCar2 carries the distinct friction-model bit; NewTank shares NewCar's base bit.
    struct EngineTypeConstant { const char* name; unsigned int value; };
    static const EngineTypeConstant kEngineTypes[] = {
        { "c_ETPlane",      0x01 },                  // 1
        { "c_ETCar",        0x02 },                  // 2
        { "c_ETTank",       0x06 },                  // 6
        { "c_ETShip",       0x09 },                  // 9
        { "c_ETRocket",     0x11 },                  // 17
        { "c_ETTorpedo",    0x19 },                  // 25
        { "c_ETHelicopter", 0x21 },                  // 33
        { "c_ETNewCar",     0x42 },                  // 66  - bit 6 = the geared CarEngine product
        { "c_ETNewCar2",    0xC2 },                  // 194 - bits 1|6|7 (the jeeps/newCar2 model)
        { "c_ETNewTank",    0x46 },                  // 70
    };
    for (unsigned i = 0; i < sizeof(kEngineTypes) / sizeof(kEngineTypes[0]); ++i)
        addConstantHelper(std::string(kEngineTypes[i].name), kEngineTypes[i].value);

    // note the first row: this module RE-registers "Bundle" with the same CID the Base module
    // already registered - registerTemplateClass's case-insensitive-erase-then-insert makes that a
    // harmless self-replacement, but it is the last-wins policy being exercised by dice's own code.
    static const TemplateClassReg n[] = {
        { CID_BundleTemplate,         "Bundle" },
        { CID_EngineTemplate,         "Engine" },
        { CID_FloatingBundleTemplate, "FloatingBundle" },
        { CID_SpringTemplate,         "Spring" },
        { CID_WingTemplate,           "Wing" },
        { CID_LandingGearTemplate,    "LandingGear" },
        { CID_RotorTemplate,          "Rotor" },
        { CID_ForceObjectTemplate,    "ForceObject" },
    };
    BF2_REGISTER_TEMPLATE_NAMES(0x8B11904, n)
}
BF2_MODULE_CONSOLE_CLOSE(GameLogicObjTemplVehicleModule, 0x8B11904)

bool GameLogicObjTemplBFModule::init()
{
    // 4,625 bytes  -  44 type names plus six constants.

    // * five of these names are not in this module'S getClassList: Flag, FlagBase,
    // DestroyTargetObjectiveTemplate, ANDCompositeObjectiveTemplate, TimerObjectiveTemplate. *

    // So init registers .con type names for types the module does not itself construct. The name
    // table and the factory table are not the same set, and any tool that assumes they are will be
    // wrong about exactly these five. Do not treat the factory list
    // as the complete .con vocabulary  -  it is not.
    static const TemplateClassReg n[] = {
        { CID_AntennaObjectTemplate,            "AntennaObject" },
        { CID_AnimatedBundleTemplate,           "AnimatedBundle" },
        { CID_ItemTemplate,                     "Item" },
        { CID_ItemContainerTemplate,            "ItemContainer" },
        { CID_DestroyableObjectTemplate,        "DestroyableObject" },
        { CID_DestroyableWindowsBundleTemplate, "DestroyableWindowsBundle" },
        { CID_DestroyableWindowTemplate,        "DestroyableWindow" },
        { CID_KitTemplate,                      "Kit" },
        { CID_MaskObjectTemplate,               "MaskObject" },
        { CID_SoldierTemplate,                  "Soldier" },
        { CID_GenericFireArmTemplate,           "GenericFireArm" },
        { CID_GenericProjectileTemplate,        "GenericProjectile" },
        { CID_FlagTemplate,                     "Flag" },
        { CID_FlagBaseTemplate,                 "FlagBase" },
        { CID_ObjectSpawnerTemplate,            "ObjectSpawner" },
        { CID_PlayerControlObjectTemplate,      "PlayerControlObject" },
        { CID_ParachuteTemplate,                "Parachute" },
        { CID_KitPartTemplate,                  "KitPart" },
        { CID_SupplyDepotTemplate,              "SupplyDepot" },
        { CID_SupplyObjectTemplate,             "SupplyObject" },
        { CID_ControlPointTemplate,             "ControlPoint" },
        { CID_ObstacleTemplate,                 "Obstacle" },
        { CID_AreaObjectTemplate,               "AreaObject" },
        // The three objectives keep their "Template" suffix in the console name - as shipped.
        { CID_DestroyTargetObjectiveTemplate,   "DestroyTargetObjectiveTemplate" },
        { CID_ANDCompositeObjectiveTemplate,    "ANDCompositeObjectiveTemplate" },
        { CID_TimerObjectiveTemplate,           "TimerObjectiveTemplate" },
        { CID_TargetObjectTemplate,             "TargetObject" },
        { CID_LadderTemplate,                   "Ladder" },
        { CID_LadderContainerTemplate,          "LadderContainer" },
        { CID_ZiplineRopeTemplate,              "ZiplineRope" },
        { CID_ZiplineTemplate,                  "Zipline" },
        { CID_ZiplineContainerTemplate,         "ZiplineContainer" },
        { CID_GrapplingHookRopeTemplate,        "GrapplingHookRope" },
        { CID_RopeLinkTemplate,                 "RopeLink" },
        { CID_HookLinkTemplate,                 "HookLink" },
        { CID_GrapplingHookRopeContainerTemplate, "GrapplingHookRopeContainer" },
        { CID_RemoteControlledObjectTemplate,   "RemoteControlledObject" },
        { CID_TurnableRemoteControlledObjectTemplate, "TurnableRemoteControlledObject" },
        { CID_TriggerTemplate,                  "Trigger" },
        { CID_AmbientEffectAreaTemplate,        "AmbientEffectArea" },
        { CID_TriggerableTemplate,              "Triggerable" },
        { CID_TriggerableTargetTemplate,        "TriggerableTarget" },
        { CID_UAVVehicleTemplate,               "UAVVehicle" },
        { CID_DropVehicleTemplate,              "DropVehicle" },
    };

    // [UNRESOLVED] Numeric values and registration call for these Boolean and collision-group constants.
    static const char* const kConstants[] = { "c_True", "c_False", "c_CGLandscape",
        "c_CGStaticObjects", "c_CGLadders", "c_CGProjectiles" };
    (void)kConstants;

    // Game-object and weapon template bindings share the same registration list.
    consoleObjects()->registerConsoleObjects(objTemplGameConsoleObjects());

    BF2_REGISTER_TEMPLATE_NAMES(0x8AD54F8, n)
}
bool GameLogicObjTemplBFModule::close()
{
    consoleObjects()->unregisterConsoleObjects(objTemplGameConsoleObjects());
    consoleObjects()->unregisterConsoleObjects(listAt(0x8AD54F8));
    return true;
}

// Releases the current material manager, then creates classId with the material-manager IID.
// The former [UNRESOLVED] constants are CID 50351 and IID 50350; failed creation leaves null.
void setMaterialManager(unsigned int classId)
{
    if (materialManager)
    {
        materialManager->release();          // IBase vtable +0x08
        materialManager = 0;                 // nulled before the create, not after
    }

    materialManager = classManager->createInstance(classId, 0xC4AE, 0);   // vtable +0x34
}

// Replaces the score manager: releases the old instance and clears the global before creation.
// Failed creation leaves null. The requested interface ID is 50360.
void setScoreManager(unsigned int classId)
{
    if (scoreManager)
    {
        scoreManager->release();             // IBase vtable +0x08
        scoreManager = 0;
    }

    scoreManager = classManager->createInstance(classId, 0xC4B8, 0);   // vtable +0x34
}

} // namespace world

// Replaces the spawn manager using the requested class ID. Preserve both exported getter spellings.
// [UNRESOLVED] Purpose of the duplicate trailing-underscore getter.
void setSpawnManager(unsigned int classId)
{
    if (spawnManager)
    {
        spawnManager->release();             // IBase vtable +0x08
        spawnManager = 0;
    }

    spawnManager = classManager->createInstance(classId, IID_ISpawnManager, 0);   // vtable +0x34
}

IBase* getSpawnManager()  { return spawnManager; }
IBase* getSpawnManager_() { return spawnManager; }

// Replaces the game-logic singleton with classId and returns its interface.
// Release precedes creation, so failure leaves null. File placement is [CHOSEN].
IGameLogic* setGameLogic(unsigned int classId)
{
    if (gameLogic)
    {
        gameLogic->release();
        gameLogic = 0;
    }

    // reinterpret-level cast: IGameLogic carries its own quartet rather than deriving IBase, so
    // the createInstance result (the factory's queryInterface(IID_IGameLogic) pointer, which is
    // the IGameLogic subobject) converts by address, not by hierarchy - same shape as the
    // IModInfo cast in BF2Engine::initEngine.
    gameLogic = reinterpret_cast<IGameLogic*>(
        classManager->createInstance(classId, IID_IGameLogic, 0));      // vtable +0x34
    return gameLogic;
}

} // namespace hfe
} // namespace dice
