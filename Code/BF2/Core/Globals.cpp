// [CHOSEN, not recovered] Consolidated singleton definitions; move each to its owning translation unit as available.
// Null slots are populated by module registration. Do not include Ids.h here: its internal-linkage
// constants conflict with the explicit external definitions below.

namespace dice {
namespace hfe {

class IBase;
class System;
class ClassManager;
class LoadStat;
class MapList;
class ReservedSlots;
class ServerSettings;
class EventManager;
class IGame;
class IGameLogic;
class BF2Engine;
class SettingsRepostitory;                    // dice's spelling - frozen
class IProfilerClient;
class DebugDraw;

namespace ai      { class AIInterfaceModule; }
namespace console { class Manager; }
namespace io      { class OldConsole; class FileManager; class SettingsManagerBase;
                    class NetworkManager; class FileChangeManager; }
namespace world   { class ObjectManager; class ObjectTemplateManager; class PlayerManager;
                    class PhysicsNodeManager; class ResponsePhysicsManager;
                    class ICollisionManager;
                    class CPersistentExplosionEffectManager; }   // ds:0x8AB08E0

// Pointers to file-scope singleton owners must retain their static initializers.
// Registration of an existing singleton records its address; it does not construct the pointee.
// In particular, console and debug-service globals must reference their actual owners before use.
// The default debugOn array enables all nine diagnostic types.

// Module-registered globals. Debug.cpp owns the typed debug globals; do not duplicate their definitions.
IBase*         debugCounterManager    = 0;      // ds:0x8AC86EC
// The spawn manager belongs to dice::hfe and is populated during module initialization.
IBase*         spawnManager           = 0;   // ds:0x8AD3A00
EventManager*  eventManager           = 0;
EventManager*  frameEventManager      = 0;
IBase*         profiler               = 0;
System*        g_system               = 0;      // ds:0x8AC0D20 - see Core/System.h.
                                                // Singleton slots with concrete types.
ClassManager*  classManager           = 0;
// Dynamically constructed singletons stay in their owning translation units.
// Do not replace loadStat, serverSettings, mapList or reservedSlots initialization with null globals here.

// Global game/settings types must match their public declarations; MSVC includes type in the symbol name.
// ServerInfo owns its separate global definition.
IGame*               game             = 0;      // ds: read by every ConsoleClass::isObjectActive
SettingsRepostitory* settings         = 0;      // ds:0x8ABBCBC - dice's spelling, frozen

// The console singleton
namespace io {
}

// Typed singletons added
// The remainder of the audit's 32 unresolved data symbols: every one already declared with a real
// (or deliberately shim) type somewhere in the tree, none previously defined anywhere. Same rule
// as the block above: null at load, populated by module registration.
BF2Engine*             bf2Engine              = 0;   // ds:0x8AACBD8
IGameLogic*            gameLogic              = 0;   // ds:0x8ACBFA0, written only by setGameLogic()
IBase*                 squadManager           = 0;   // ds:0x8ACBFA4 -
                                                     // Created by GameServer::loadPath; an existing manager receives the dynamic spawn groups.
IProfilerClient*       profilerClient         = 0;   // ds:0x8ABBCA8, name attested
DebugDraw*             debugDraw              = 0;   // ds:0x8ABBC78, name attested
// Sound/BfSound.cpp owns the typed sound globals; their earlier names were [INFERRED].
ai::AIInterfaceModule* aiInterfaceModule      = 0;   // ds:0x8AACC0C - variable lives at hfe scope

// console::manager (ELF 0x8AC3CA0 / PE 0x8974F4) is defined and eagerly
// constructed in IO/Console/Manager.cpp; it is not a null singleton slot.

namespace io {
FileManager*           fileManager            = 0;   // _ZN4dice3hfe2io11fileManagerE
NetworkManager*        networkManager         = 0;   // ds:0x8AC3D1C
FileChangeManager*     fileChangeManager      = 0;
}

namespace world {
ObjectManager*          objectManager          = 0;
ObjectTemplateManager*  objectTemplateManager  = 0;  // ds:0x8B1B924; Module::getSingletonList
// PlayerManager constructs this shared explosion-effect manager and deletes/nulls it during teardown.
CPersistentExplosionEffectManager* explosionEffectManager = 0;   // ds:0x8AB08E0

PlayerManager*          playerManager          = 0;  // ds:0x8B1B928,
PhysicsNodeManager*     physicsNodeManager     = 0;  // ds:0x8B27844 — name attested as
ResponsePhysicsManager* responsePhysicsManager = 0;  // ds:0x8B27848 — likewise attested, as
                                                     // _ZN4dice3hfe5world22responsePhysicsManagerE.
}

// Registry-owned singleton slots. Each registration row stores the address of its slot.
// Keep each definition's type synchronized with ModuleRegistry.cpp and all extern declarations;
// use the recovered class type when available, rather than guessing an interface.
IBase*                 g_modInfo              = 0;   // ds:0x8B16FC8  _ZN4dice3hfe9g_modInfoE

namespace io {
IBase*                 inputDeviceManager     = 0;   // ds:0x8AC8C88  _ZN4dice3hfe2io18inputDeviceManagerE
IBase*                 controlMapManager      = 0;   // ds:0x8AC8C8C  _ZN4dice3hfe2io17controlMapManagerE
IBase*                 resourceManager        = 0;   // ds:0x8AC8C90  _ZN4dice3hfe2io15resourceManagerE
IBase*                 cacheManager           = 0;   // ds:0x8AC8C94  _ZN4dice3hfe2io12cacheManagerE
IBase*                 asyncMultiStreamer     = 0;   // ds:0x8AC8C98  _ZN4dice3hfe2io18asyncMultiStreamerE
IBase*                 networkableInfoManager = 0;   // ds:0x8AC8CA0  _ZN4dice3hfe2io22networkableInfoManagerE
IBase*                 inputManager           = 0;   // ds:0x8AC8CA4  _ZN4dice3hfe2io12inputManagerE
IBase*                 lexicon                = 0;   // ds:0x8AC8CA8  _ZN4dice3hfe2io7lexiconE
IBase*                 activeMethodConsole    = 0;   // ds:0x8AC3C88  _ZN4dice3hfe2io19activeMethodConsoleE
}

namespace ai {
IBase*                 g_aiConnection         = 0;   // ds:0x8B29AE0  _ZN4dice3hfe2ai14g_aiConnectionE
// [CHOSEN] AI-instance singleton name. Its bots-game query controls whether loading includes .ai files.
IBase*                 g_aiInstance           = 0;   // ds:0x8AC37F8  [CHOSEN name]
// ds:0x8AC35E4 is CommonSingletonServer::m_iBFCollisionHandler (AIDLL/AI/Main), not a separate
// global: the engine's fire, timeout, collision, explosion and damage reports read it there.
}

namespace world {
// Level-scoped managers publish and clear these pointers in their constructors and destructors.
class TriggerableManager; class TriggerManager; class ArtilleryManager; class UAVVehicleManager;
TriggerableManager*    triggerableManager     = 0;   // ds:0x8AADC8C
TriggerManager*        triggerManager         = 0;   // ds:0x8AADC98
ArtilleryManager*      artilleryManager       = 0;   // ds:0x8AACE20
UAVVehicleManager*     uavVehicleManager      = 0;   // ds:0x8AADD40

// [CHOSEN] global name for the supply-drop singleton. Its constructor-side initializer publishes it;
// runtime users require a valid instance and do not null-check.
class SupplyDropManager;
SupplyDropManager*     supplyDropManager      = 0;   // ds:0x8AADAAC (unnamed in the ELF)

// [CHOSEN] Commander-tools singleton name. The first manager publishes itself during server-logic construction.
class CommanderToolsManager;
CommanderToolsManager* commanderToolsManager  = 0;   // ds:0x8AAC270 (unnamed in the ELF)

// Ambient-effect-area manager. The first instance publishes itself; only its owning destructor clears the pointer.
// [CHOSEN] Namespace alias for the class singleton.
class AmbientEffectAreaManager;
AmbientEffectAreaManager* ambientEffectAreaManager = 0;
IBase*                 windManager            = 0;   // ds:0x8B1B92C  _ZN4dice3hfe5world11windManagerE
IBase*                 portalManager          = 0;   // ds:0x8B1ED80  _ZN4dice3hfe5world13portalManagerE
ICollisionManager*     collisionManager       = 0;   // ds:0x8B27840  _ZN4dice3hfe5world16collisionManagerE
IBase*                 basicPhysicsSystem     = 0;   // ds:0x8B2784C  _ZN4dice3hfe5world18basicPhysicsSystemE
IBase*                 heightmapCluster       = 0;   // ds:0x8ABAF24  _ZN4dice3hfe5world16heightmapClusterE
IBase*                 materialManager        = 0;   // ds:0x8AD2820  _ZN4dice3hfe5world15materialManagerE
IBase*                 scoreManager           = 0;   // ds:0x8AD1740  _ZN4dice3hfe5world12scoreManagerE
}

// External definitions for IDs declared extern by public headers.
// Their values match Ids.h, whose namespace-scope const definitions have internal linkage.
extern const unsigned int IID_IGameServer = 120001;

// External constants used by engine startup. Keep constness consistent in every declaration.
extern const unsigned int IID_IGame           = 120000;
extern const unsigned int IID_IGameClient     = 120002;
extern const unsigned int CID_GameServer      = 121002;
extern const unsigned int CID_GameClient      = 121003;
extern const unsigned int CID_ClientGameLogic = 121006;
extern const unsigned int CID_ServerGameLogic = 121007;

namespace world {
extern const unsigned int CID_ObjectTemplate              = 37999;
extern const unsigned int IID_IObjectTemplate             = 50020;
extern const unsigned int IID_IObjectTemplateComponent    = 50379;
extern const unsigned int IID_IObjectComponent            = 50043;   // == 0xC37B, the dword at 0x896ABC4
extern const unsigned int IID_IPhysicsNode                = 50210;
extern const unsigned int IID_IResponsePhysics            = 50220;
} // namespace world

} // namespace hfe
} // namespace dice
