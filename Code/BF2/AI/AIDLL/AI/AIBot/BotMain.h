// Bot implementation and its shared virtual interface.
// Preserve virtual declaration order across the AI module boundary.

#ifndef AI_AIDLL_AI_AIBOT_BOTMAIN_H          // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_BOTMAIN_H

#include "../../../../Math/Vec3.h"

#include <list>
#include <map>
#include <utility>
#include <string>
#include <vector>

namespace dice {
namespace hfe {

// Include Vec3's definition; it is a Math alias and cannot be forward-declared as a class.
namespace world { class IObject; }
namespace io { class PlayerInput; }

namespace ai {

class AIStrategicObject;
class BAPContext;
class BotBehaviour;
class IAIObject;
class BotMemory;
class BotNavigation;
class BotOrder;
class BotScript;
class BotSquad;
class CommunicationAgent;
class FireCorrectionData;
class FireObject;
class IAIObject;
class IAIPlayer;
class IBotDeathCallBack;
class IBotSpawnCallBack;
class Information;
class PlanInterpreter;
class SensedObject;
class SensingAgent;
class SquadMember;
class UrgencyGenerator;
class WaypointList;

#if defined(BF2_PLATFORM_POSIX)
// [CHOSEN, not recovered] Match AISettingsInterfaces.h's forward declaration on Clang/GCC.
enum BotLodLevel : int { BotLodFull = 0, BotLodMedium = 1, BotLodLight = 2 };
#else
enum BotLodLevel { BotLodFull = 0, BotLodMedium = 1, BotLodLight = 2 };  // [CHOSEN] - see BotManager
#endif

// Base interface used by the bot manager to schedule and query bots.
// [RETIRE-WHEN: botmain-bases] Remove when the corresponding implementation is complete and validated.
class Bot
{
public:
    // The C4 record BotMain keeps at +0x1A4 and getC4 (ELF 0x188880, `ret $4`) returns by value:
    // placeC4 (0x1888F0) and removeC4 (0x188980) write it. BBC4::calculateUrgency reads it.
    // [INFERRED] type and field names; placement type stays an int (the enum's labels are not
    // recovered; Bot::C4PlacementType is attested by the placeC4 mangling).
    struct C4State
    {
        /* +0x00 */ bool placed;
        /* +0x04 */ int placementType;
        /* +0x08 */ Vec3 position;
        /* +0x14 */ AIStrategicObject* object;
        /* +0x18 */ double placedTime;    // game time at placeC4
        /* +0x20 */ double lifeTime;      // 240.0 (0x49CB3C) at placeC4; updateC4 removes after it
    };

    virtual ~Bot();     // - one instruction each

    virtual void setLodLevel(BotLodLevel level) = 0;                                              // +0x008
    virtual BotLodLevel getLodLevel() const = 0;                                                  // +0x00C
    virtual void planExecution(const double& slice) = 0;                                          // +0x010 partial [FRONTIER]
    virtual int decisionMaking(const double& slice) = 0;                                          // +0x014 partial [FRONTIER]
    virtual void cheatExecution() = 0;                                                            // +0x018
    virtual void see(const double& dt) = 0;                                                       // +0x01C
    virtual void updateSensing(const double& dt) = 0;                                             // +0x020
    virtual void updatePathfinding(bool& a, bool* b) = 0;                                         // +0x024
    virtual void reset() = 0;                                                                     // +0x028
    virtual bool updateBotVehicle(bool* out) = 0;                                                 // +0x02C
    virtual bool updateBot() = 0;                                                                 // +0x030 (ELF bool)
    virtual BotSquad* getBotSquad() const = 0;                                                    // +0x034
    virtual float getObedience() = 0;                                                             // +0x038
    virtual float* getCurrentOrders() = 0;                                                       // +0x03C order weights
    virtual WaypointList* getWaypoints() = 0;                                                     // +0x040
    virtual void setWaypoints(WaypointList* w) = 0;                                               // +0x044
    virtual bool recieveOrder(BotOrder* order) = 0;                                               // +0x048
    virtual bool recieveOrder(BotOrder* order, std::list<unsigned int>& targets) = 0;             // +0x04C (ELF answers 0/1)
    virtual std::list<unsigned int>* getOrderList(unsigned char behaviour) = 0;                   // +0x050
    virtual bool hasPlan() = 0;                                                                   // +0x054
    virtual bool hasDefaultPlan() const = 0;                                                      // +0x058
    virtual void invalidatePlan() = 0;                                                            // +0x05C
    virtual BotScript* getPlan() = 0;                                                             // +0x060
    virtual void setPlan(BotScript* plan) = 0;                                                    // +0x064
    virtual bool getPlanLock() const = 0;                                                         // +0x068
    virtual void setPlanLock(bool b) = 0;                                                         // +0x06C
    virtual bool isTakingOrders() = 0;                                                            // +0x070
    virtual bool isOrderedBehaviour(unsigned char behaviour) = 0;                                 // +0x074
    virtual bool isOrderedObject(unsigned int handle, BotBehaviour* b) = 0;                       // +0x078
    virtual void setFiringTarget(unsigned int handle) = 0;                                        // +0x07C [FRONTIER]
    virtual unsigned int getFiringTarget() const = 0;                                             // +0x080
    virtual double getFiringTargetTime() const = 0;                                               // +0x084
    virtual SensingAgent* getSensingAgent() = 0;                                                  // +0x088
    virtual void insertBehaviourInformation(float f, const Vec3& v, unsigned char c) = 0;         // +0x08C [FRONTIER]
    virtual const std::map<unsigned int, SensedObject*>& getSensedObjects() const = 0;                          // +0x090
    virtual SensedObject* getSensedObjectFromHandle(unsigned int handle) const = 0;                       // +0x094
    virtual void setUnresolvedCollisionObject(unsigned int handle) = 0;                           // +0x098
    virtual unsigned int getUnresolvedCollisionHandle() = 0;                                      // +0x09C
    virtual std::list<std::pair<FireObject*, unsigned int> >* getIncommingFire() = 0;                                                         // +0x0A0
    virtual float getIncommingFireTotalStrength() = 0;                                            // +0x0A4
    virtual float getIncommingFireTotalStrengthAgainstSelf() = 0;                                 // +0x0A8
    virtual float getAttackerStrength() const = 0;                                                // +0x0AC
    virtual CommunicationAgent* getCommunicationAgent() = 0;                                      // +0x0B0
    virtual int getPlayerId() const = 0;                                                          // +0x0B4
    virtual int getInputId() = 0;                                                                 // +0x0B8
    virtual IAIPlayer* getPlayer() const = 0;                                                     // +0x0BC
    virtual void updatePlayerAction(bool stalled) = 0;                                            // +0x0C0
    virtual bool isDead() const = 0;                                                              // +0x0C4
    virtual bool setSide(int side) = 0;                                                           // +0x0C8
    virtual void setName(const std::string& name) = 0;                                            // +0x0CC
    virtual bool isDeactivated() const = 0;                                                       // +0x0D0
    virtual void setAsDeactivated(bool b) = 0;                                                    // +0x0D4
    virtual unsigned int getCurrentEquipment() const = 0;                                         // +0x0D8
    virtual void setCurrentEquipment(unsigned int handle) = 0;                                    // +0x0DC
    virtual int getSide() const = 0;                                                              // +0x0E0
    virtual int getBotType() const;                                                               // +0x0E4
    virtual unsigned int getBotId() const = 0;                                                    // +0x0E8
    virtual BotMemory* getMemory() = 0;                                                           // +0x0EC
    virtual const std::string& getName() const = 0;                                               // +0x0F0
    virtual void setTimeOfLastGrenadeThrown(double t) = 0;                                        // +0x0F4
    virtual double getTimeOfGrenadeThrown(int i) const = 0;                                       // +0x0F8
    virtual BotNavigation* getBotNavigation() = 0;                                                // +0x0FC
    virtual int getSpawnPoint() const = 0;                                                        // ELF +0x100 / PE +0xFC
    virtual int getSpawnKit() const = 0;                                                          // +0x104
    virtual void getSpawnTarget(int& a, int& b) const = 0;                                        // +0x108
    virtual void setSpawnTarget(int spawnGroup, int kit) = 0;                                    // ELF +0x10C / PE +0x10C (overload order)
    virtual void setSpawnTarget(int strategicArea, int spawnGroup, int kit) = 0;                 // +0x110
    virtual int getSpawnToStrategicAreaId() = 0;                                                  // +0x114
    virtual bool isJustSpawned() = 0;                                                             // +0x118
    virtual IBotDeathCallBack* getDeathCallBack() const = 0;                                      // +0x11C
    virtual void setDeathCallBack(IBotDeathCallBack* cb) = 0;                                     // +0x120
    virtual IBotSpawnCallBack* getSpawnCallBack() const = 0;                                      // +0x124
    virtual void setSpawnCallBack(IBotSpawnCallBack* cb) = 0;                                     // +0x128
    virtual void setIsFiring(bool b) = 0;                                                         // +0x12C
    virtual bool isFiring() = 0;                                                                  // +0x130
    virtual void setAllowFire(bool b) = 0;                                                        // +0x134
    virtual bool isFireAllowed() = 0;                                                             // +0x138
    virtual void setSquadMember(SquadMember* m) = 0;                                              // +0x13C
    virtual SquadMember* getSquadMember() = 0;                                                    // +0x140
    virtual bool isBotStuck() = 0;                                                                // +0x144
    virtual void setBotStuck(bool b) = 0;                                                         // +0x148
    virtual void placeC4(int placementType, Vec3 v, AIStrategicObject* o) = 0;                    // +0x14C
    virtual bool isC4Placed() = 0;                                                                // +0x150
    virtual void removeC4() = 0;                                                                  // +0x154
    virtual C4State getC4() = 0;                                                                  // +0x158
    virtual void updateC4() = 0;                                                                  // +0x15C
    virtual void enableBotScriptDebugger(bool /*enable*/) = 0;                                    // +0x160
    virtual void* getDebugger() = 0;                                                              // +0x164
    virtual BAPContext* getScriptContext();                                                       // +0x168
    virtual void event_projectile(FireObject* f, unsigned int h) = 0;                             // +0x16C
    virtual void event_shotHit(int i, world::IObject* o, const Vec3& v) = 0;                      // +0x170
    virtual void event_shotMissed(int i, const Vec3& v) = 0;                                      // +0x174
    virtual void event_heardObject(unsigned int h) = 0;                                           // +0x178
    virtual bool isInTransitionPhase() const = 0;                                                 // +0x17C
    virtual void setIsInTransitionPhase(bool b) = 0;                                              // +0x180
    virtual void addToMoveInclination(float f) = 0;                                               // +0x184
    virtual void addToFireInclination(float f) = 0;                                               // +0x188
    virtual float getMoveInclination() = 0;                                                       // +0x18C
    virtual float getFireInclination() = 0;                                                       // +0x190
    virtual void requestSoldierPose(int pose, bool b) = 0;                                        // +0x194
    virtual int getRequestedSolderPose() = 0;                                                     // +0x198
    virtual void setPoseStatus(int pose, unsigned int h, bool a, bool b) = 0;                     // +0x19C
    virtual void event_airborn() = 0;                                                             // +0x1A0
    virtual bool isAirBorn() const = 0;                                                           // +0x1A4
    virtual void setLastChangeTime(double t) = 0;                                                 // +0x1A8
    virtual void setManDown(bool b) = 0;                                                          // +0x1AC
    virtual bool getManDown() const = 0;                                                          // +0x1B0
    virtual void rearm(Information* i) = 0;                                                       // +0x1B4
    virtual void setChangeTarget(unsigned int h) = 0;                                             // +0x1B8
    virtual unsigned int getChangeTarget() const = 0;                                             // +0x1BC
    virtual void setRepairTarget(unsigned int h) = 0;                                             // +0x1C0
    virtual unsigned int getRepairTarget() const = 0;                                             // +0x1C4
    virtual void setWantedRepairTarget(unsigned int h) = 0;                                       // +0x1C8
    virtual unsigned int getWantedRepairTarget() const = 0;                                       // +0x1CC
    virtual void setBridgeRepairTarget(unsigned int h) = 0;                                       // +0x1D0
    virtual unsigned int getBridgeRepairTarget() const = 0;                                       // +0x1D4
    virtual void setReviveTarget(unsigned int h) = 0;                                             // +0x1D8
    virtual unsigned int getReviveTarget() const = 0;                                             // +0x1DC

    // [INFERRED] friendship, like BotMain's below: BotManager::actionExecutePlan reads +4 inline
    // through a `Bot*` to press a climbing bot's inputs.
    friend class BotManager;
    // [INFERRED] likewise: CommunicationAgent::shutOffHorn (ELF 0x1C53F0) reads +4 inline.
    friend class CommunicationAgent;
    // [INFERRED] likewise: SquadMember::update (ELF 0x1B1ED0) presses the weapon input via +4.
    friend class SquadMember;
    // [INFERRED] inline reader of +4 (no symbol): the BAP actions and entries read the bound input directly.
    io::PlayerInput* getInput() const { return m_playerInput; }

protected:
    // Base construction leaves both pointers uninitialized. Player input is borrowed and
    // bound later; BotMain allocates the script context stored here.
    /*  +4 */ io::PlayerInput* m_playerInput;
    /*  +8 */ BAPContext* m_bapContext;     // BotMain's constructor does `new BAPContext` into it
};

// sizeof == 588 (0x24C), from `operator new(0x24Cu)` in BotManager's constructor. The layout below
// ends with a Vec3 at +576, which lands exactly on 588 - an independent confirmation of the size.
class BotMain : public Bot
{
public:
    // Accepts the player, bot settings, shared memory, interpreter, and urgency generators.
    // The merger is passed through its UrgencyGenerator base interface.
    BotMain(IAIPlayer* player,
            unsigned int id,
            float* weights,
            float f,
            int i,
            BotOrder* order,
            BotMemory* memory,
            PlanInterpreter* interpreter,
            int n,
            UrgencyGenerator* merger,
            UrgencyGenerator** generators);
    ~BotMain();

// Bot virtual interface. Preserve declaration order across the DLL boundary.
// [RETIRE-WHEN: botmain-bases] Remove when the corresponding implementation is complete and validated.
    virtual void setLodLevel(BotLodLevel level);                                                  // +0x008
    virtual BotLodLevel getLodLevel() const;                                                      // +0x00C
    virtual void planExecution(const double& slice);                                              // +0x010 partial [FRONTIER]
    // [INFERRED] Returns an integer work count accumulated by BotManager.
    virtual int decisionMaking(const double& slice);                                              // +0x014 partial [FRONTIER]
    virtual void cheatExecution();                                                                // +0x018
    virtual void see(const double& dt);                                                           // +0x01C
    virtual void updateSensing(const double& dt);                                                 // +0x020
    virtual void updatePathfinding(bool& a, bool* b);                                             // +0x024
    virtual void reset();                                                                         // +0x028
    virtual bool updateBotVehicle(bool* out);                                                     // +0x02C
    virtual bool updateBot();                                                                     // +0x030 ELF 0x18BF50
    virtual BotSquad* getBotSquad() const;                                                        // +0x034
    virtual float getObedience();                                                                 // +0x038
    virtual float* getCurrentOrders();                                                           // +0x03C order weights
    virtual WaypointList* getWaypoints();                                                         // +0x040
    virtual void setWaypoints(WaypointList* w);                                                   // +0x044
    virtual bool recieveOrder(BotOrder* order);                                                   // +0x048
    virtual bool recieveOrder(BotOrder* order, std::list<unsigned int>& targets);                 // +0x04C (ELF answers 0/1)
    virtual std::list<unsigned int>* getOrderList(unsigned char behaviour);                       // +0x050
    virtual bool hasPlan();                                                                       // +0x054
    virtual bool hasDefaultPlan() const;                                                          // +0x058
    virtual void invalidatePlan();                                                                // +0x05C
    virtual BotScript* getPlan();                                                                 // +0x060
    virtual void setPlan(BotScript* plan);                                                        // +0x064
    virtual bool getPlanLock() const;                                                             // +0x068
    virtual void setPlanLock(bool b);                                                             // +0x06C
    virtual bool isTakingOrders();                                                                // +0x070
    virtual bool isOrderedBehaviour(unsigned char behaviour);                                     // +0x074
    virtual bool isOrderedObject(unsigned int handle, BotBehaviour* b);                           // +0x078
    virtual void setFiringTarget(unsigned int handle);                                            // +0x07C [FRONTIER]
    virtual unsigned int getFiringTarget() const;                                                 // +0x080
    virtual double getFiringTargetTime() const;                                                   // +0x084
    virtual SensingAgent* getSensingAgent();                                                      // +0x088
    virtual void insertBehaviourInformation(float f, const Vec3& v, unsigned char c);             // +0x08C
    virtual const std::map<unsigned int, SensedObject*>& getSensedObjects() const;                              // +0x090
    virtual SensedObject* getSensedObjectFromHandle(unsigned int handle) const;                           // +0x094
    virtual void setUnresolvedCollisionObject(unsigned int handle);                               // +0x098
    virtual unsigned int getUnresolvedCollisionHandle();                                          // +0x09C
    virtual std::list<std::pair<FireObject*, unsigned int> >* getIncommingFire();                                                             // +0x0A0
    virtual float getIncommingFireTotalStrength();                                                // +0x0A4
    virtual float getIncommingFireTotalStrengthAgainstSelf();                                     // +0x0A8
    virtual float getAttackerStrength() const;                                                    // +0x0AC
    virtual CommunicationAgent* getCommunicationAgent();                                          // +0x0B0
    virtual int getPlayerId() const;                                                              // +0x0B4
    virtual int getInputId();                                                                     // +0x0B8
    virtual IAIPlayer* getPlayer() const;                                                         // +0x0BC
    virtual void updatePlayerAction(bool stalled);                                                // +0x0C0
    virtual bool isDead() const;                                                                  // +0x0C4
    virtual bool setSide(int side);                                                               // +0x0C8
    virtual void setName(const std::string& name);                                                // +0x0CC
    virtual bool isDeactivated() const;                                                           // +0x0D0
    virtual void setAsDeactivated(bool b);                                                        // +0x0D4
    virtual unsigned int getCurrentEquipment() const;                                             // +0x0D8
    virtual void setCurrentEquipment(unsigned int handle);                                        // +0x0DC
    virtual int getSide() const;                                                                  // +0x0E0
    virtual int getBotType() const;                                                               // +0x0E4
    virtual unsigned int getBotId() const;                                                        // +0x0E8
    virtual BotMemory* getMemory();                                                               // +0x0EC
    virtual const std::string& getName() const;                                                   // +0x0F0
    virtual void setTimeOfLastGrenadeThrown(double t);                                            // +0x0F4
    virtual double getTimeOfGrenadeThrown(int i) const;                                           // +0x0F8
    virtual BotNavigation* getBotNavigation();                                                    // +0x0FC
    virtual int getSpawnPoint() const;
    virtual int getSpawnKit() const;                                                              // +0x104
    virtual void getSpawnTarget(int& a, int& b) const;                                            // +0x108
    virtual void setSpawnTarget(int spawnGroup, int kit);
    virtual void setSpawnTarget(int strategicArea, int spawnGroup, int kit);                     // +0x110
    virtual int getSpawnToStrategicAreaId();                                                      // +0x114
    virtual bool isJustSpawned();                                                                 // +0x118
    virtual IBotDeathCallBack* getDeathCallBack() const;                                          // +0x11C
    virtual void setDeathCallBack(IBotDeathCallBack* cb);                                         // +0x120
    virtual IBotSpawnCallBack* getSpawnCallBack() const;                                          // +0x124
    virtual void setSpawnCallBack(IBotSpawnCallBack* cb);                                         // +0x128
    virtual void setIsFiring(bool b);                                                             // +0x12C
    virtual bool isFiring();                                                                      // +0x130
    virtual void setAllowFire(bool b);                                                            // +0x134
    virtual bool isFireAllowed();                                                                 // +0x138
    virtual void setSquadMember(SquadMember* m);                                                  // +0x13C
    virtual SquadMember* getSquadMember();                                                        // +0x140
    virtual bool isBotStuck();                                                                    // +0x144
    virtual void setBotStuck(bool b);                                                             // +0x148
    virtual void placeC4(int placementType, Vec3 v, AIStrategicObject* o);                        // +0x14C
    virtual bool isC4Placed();                                                                    // +0x150
    virtual void removeC4();                                                                      // +0x154
    virtual C4State getC4();                                                                      // +0x158
    virtual void updateC4();                                                                      // +0x15C
    virtual void enableBotScriptDebugger(bool /*enable*/);                                        // +0x160
    virtual void* getDebugger();                                                                  // +0x164
    // Inherits Bot::getScriptContext; do not add a derived override for that slot.
    virtual void event_projectile(FireObject* f, unsigned int h);                                 // +0x16C
    virtual void event_shotHit(int i, world::IObject* o, const Vec3& v);                          // +0x170
    virtual void event_shotMissed(int i, const Vec3& v);                                          // +0x174
    virtual void event_heardObject(unsigned int h);                                               // +0x178
    virtual bool isInTransitionPhase() const;                                                     // +0x17C
    virtual void setIsInTransitionPhase(bool b);                                                  // +0x180
    virtual void addToMoveInclination(float f);                                                   // +0x184
    virtual void addToFireInclination(float f);                                                   // +0x188
    virtual float getMoveInclination();                                                           // +0x18C
    virtual float getFireInclination();                                                           // +0x190
    virtual void requestSoldierPose(int pose, bool b);                                            // +0x194
    virtual int getRequestedSolderPose();                                                         // +0x198
    virtual void setPoseStatus(int pose, unsigned int h, bool a, bool b);                         // +0x19C
    virtual void event_airborn();                                                                 // +0x1A0
    virtual bool isAirBorn() const;                                                               // +0x1A4
    virtual void setLastChangeTime(double t);                                                     // +0x1A8
    virtual void setManDown(bool b);                                                              // +0x1AC
    virtual bool getManDown() const;                                                              // +0x1B0
    virtual void rearm(Information* i);                                                           // +0x1B4
    virtual void setChangeTarget(unsigned int h);                                                 // +0x1B8
    virtual unsigned int getChangeTarget() const;                                                 // +0x1BC
    virtual void setRepairTarget(unsigned int h);                                                 // +0x1C0
    virtual unsigned int getRepairTarget() const;                                                 // +0x1C4
    virtual void setWantedRepairTarget(unsigned int h);                                           // +0x1C8
    virtual unsigned int getWantedRepairTarget() const;                                           // +0x1CC
    virtual void setBridgeRepairTarget(unsigned int h);                                           // +0x1D0
    virtual unsigned int getBridgeRepairTarget() const;                                           // +0x1D4
    virtual void setReviveTarget(unsigned int h);                                                 // +0x1D8
    virtual unsigned int getReviveTarget() const;                                                 // +0x1DC
    virtual void setIsRepaired(bool b);                                                           // +0x1E0
    virtual bool getIsRepaired() const;                                                           // +0x1E4
    virtual void setIsEngineerTarget(bool b);                                                     // +0x1E8
    virtual bool getIsEngineerTarget() const;                                                     // +0x1EC
    virtual void setShouldWaitForRepairs(bool b);                                                 // +0x1F0
    virtual bool getShouldWaitForRepairs() const;                                                 // +0x1F4
    virtual void setMineTarget(unsigned int h);                                                   // +0x1F8
    virtual unsigned int getMineTarget() const;                                                   // +0x1FC
    virtual void setNewAssignedAreas(int a, int b);                                               // +0x200
    virtual bool hasSameAssignedAreas(int a);                                                     // +0x204
    virtual void copyAssignedAreas();                                                             // +0x208
    virtual void setCurrentWaypoint(Vec3 v, int i);                                               // +0x20C
    virtual Vec3 getCurrentWaypoint();                                                            // +0x210 by value (ELF sret)
    virtual void resetAssignedAreas();                                                            // +0x214
    virtual void setTimeOfLastFailedStaticSkyAttack(double t);                                    // +0x218
    virtual double getTimeOfLastFailedStaticSkyAttack() const;                                    // +0x21C
    virtual void setTimeOfLastFailedStaticAttack(double t);                                       // +0x220
    virtual double getTimeOfLastFailedStaticAttack() const;                                       // +0x224
    virtual void setBotWantsToSimulateArtillery(bool b);                                          // +0x228
    virtual bool getBotWantsToSimulateArtillery() const;                                          // +0x22C
    virtual void setBotIsSimulatingArtillery(bool b);                                             // +0x230
    virtual bool getBotIsSimulatingArtillery() const;                                             // +0x234
    virtual bool setAvoidSide(int side);                                                          // +0x238 (ELF answers 0/1)
    virtual int getAvoidSide() const;                                                             // +0x23C
    virtual int getMoveToStatus() const;                                                          // +0x240
    virtual void setMoveToStatus(int status);                                                     // +0x244
    virtual void setCollidedObject(unsigned int h);                                               // +0x248
    virtual unsigned int getCollidedObject() const;                                               // +0x24C
    virtual void setCollidedObjectPosition(const Vec3& v);                                        // +0x250
    virtual const Vec3& getCollidedObjectPosition() const;                                        // +0x254
    virtual void setIgnoreObstacles(bool b);                                                      // +0x258
    virtual bool getIgnoreObstacles() const;                                                      // +0x25C
    virtual void setSlowDownToTarget(unsigned int h);                                             // +0x260
    virtual unsigned int getSlowDownToTarget() const;                                             // +0x264
    virtual double getFriendsInLineOfFireTime() const;                                            // +0x268
    virtual void setFriendsInLineOfFireTime(double t);                                            // +0x26C
    virtual void setLastCheckMoveToFinishedTime(double t);                                        // +0x270
    virtual double getLastCheckMoveToFinishedTime() const;                                        // +0x274
    virtual void setIsGoingForFlag(bool b);                                                       // +0x278
    virtual bool getIsGoingForFlag() const;                                                       // +0x27C
    virtual bool botInPoseTransition() const;                                                     // +0x280
    virtual void setUseStaticAttack(bool b);                                                      // +0x284
    virtual bool getUseStaticAttack() const;                                                      // +0x288
    virtual void setTimeOfLastFailedSplashDamageAttack(double t);                                 // +0x28C
    virtual double getTimeOfLastFailedSplashDamageAttack() const;                                 // +0x290
    virtual void setTakeOffPosition(const Vec3& v);                                               // +0x294
    virtual const Vec3& getTakeOffPosition() const;                                               // +0x298
    virtual void setQuadSeen();                                                                   // +0x29C
    virtual bool isAttacking(unsigned int handle, double& out) const;                             // +0x2A0
    virtual void event_shotTimeOut(int i);                                                        // +0x2A4
    virtual const Vec3& event_firing(bool& b, int i, const Vec3& v);                              // +0x2A8 returns the record's +56 Vec3

    // not in the vtable. Ordinary members; the vtable dump above is the authority on which
    // every name here is absent from all 171 slots.
    void  prePlanExecution();
    void  postPlanExecution();
    void  addBehaviour(BotBehaviour* b, int i);
    void  removeBehaviour(BotBehaviour* b);
    void  uninitAllBehaviours();
    BotBehaviour* getActiveBehaviour();
    UrgencyGenerator** getBehaviourUrgencyGenerators();   // ELF 0x18AAD0: +152
    UrgencyGenerator*  getSelector();                     // ELF 0x18AAC0: +148
    void  calculateMods();
    unsigned char getMode();
    void setMode(unsigned char mode);
    float getLastScout();
    float* getBasicBehaviourMods();
    float* getCurrentBehaviourMods();
    // these two touch the class static, not A member. See the header.
    static void          setNBehaviours(unsigned char n);   // , no this argument
    static unsigned char getNBehaviours();
    static void   setBasicWeights(float* w);                // , no this argument
    static float* getBasicWeights();
    void  setTakingOrders(bool b);
    PlanInterpreter* getPlanInterpreter();
    void  setPlanInterpreter(PlanInterpreter* p);
    void  setHadPrecisionTimeStamp(double time);            // ELF 0x18C7D0: +276
    double getHadPrecisionTimeStamp() const;
    // [INFERRED] inline reader (no symbol): +180 is read inline by BAPAAimAt::correctAim 0x1CDA3D and
    // EntryMouseTurretAimAt 0x2B9A5C. Added by ai-w1-c-actions (requested from ai-bot).
    FireCorrectionData* getFireCorrection() const { return m_fireCorrection; }
    double getTimeOfLastFire() const;
    void   stepFireInclination();                           // thunk
    void*  getAttackers() const;
    // +0x298 and returns it), so this pair is the other Vec3 - the one the constructor never
    // initialises. The old note that one of the two pairs was +576 is settled.
    const Vec3& getSpawnCoord() const;
    void  setSpawnCoord(const Vec3& v);
    const Vec3& getLookDir();
    void setLookDir(const Vec3& direction);
    float setObedience(float f);
    float getAttention();
    float setAttention(float f);
    float getInvAttention();
    float setInvAttention(float f); // ELF 0x18AB80: stores and returns the supplied value
    float getMarksmanship();                                // no setter
    int getHarmlessThrsh();                                 // ELF 0x18ABA0: +0x28 (cvtsi2ss at the caller); no setter, dice's spelling
    void  setPlayer(IAIPlayer* p);
    void  setPlayerInput();
    void  resetAllControls(bool a, bool b, bool c);
    // Ask the next action to re-check the bot's vehicle (+0x58 m_checkVehicle).
    void  setUpdateCurrentEquipment();                      // ELF 0x18A9F0
    bool  getUpdateCurrentEquipment();                      // ELF 0x18AA00
    void  resetChangeTarget();
    void  pollRequestedSoldierPose();
    bool  checkPoseOk(bool& out, int pose);
    void  clearTestResults();
    void  stepMoveInclination();
    void clearMoveInclination();
    void clearFireInclination();
    bool validateDataStructures();
    // A class static, shared by every bot. See the header - the constructor reads it three times.
    static unsigned char mu8NBehaviours;
    static float* maBasicWeights;                           // ELF .bss

private:
    // [INFERRED] friendship: addUnspawnedBot directly replaces +28 in both images.
    friend class BotManager;
    // [INFERRED] friendship: SensingAgent::quadInFocus (ELF 0x1BDA60) reads +0x9C (m_player) inline.
    friend class SensingAgent;
    // [INFERRED] friendship: the WPBeachLanding constructors (ELF 0x1B8370, 0x1B8470) store their
    // landing zone into +0xE8 (m_ptr232) through the Bot pointer.
    friend class WPBeachLanding;
    // ELF 0x1EEB90 reads the landing-zone pointer written by WPBeachLanding.
    friend class BAPConObjectInLandingZone;
    // [CHOSEN] Names for fields whose original identifiers are unavailable. Offsets describe the retail x86 layout.
    /*  12 */ void*        m_debugger;              // = 0 getDebugger
    /*  16 */ BotLodLevel  m_lodLevel;              // = 0 get/setLodLevel
    /*  20 */ bool         m_planInvalid;           // = true hasPlan / invalidatePlan
    /*  21 */ bool         m_hasDefaultPlan;        // = false hasDefaultPlan
    /*  24 */ unsigned int m_botId;                 // ctor arg `id`; getBotId reads it
    /*  28 */ float*       m_basicWeights;          // ctor arg `weights`
    /*  32 */ float        m_float32;               // ctor arg `f` [UNRESOLVED]
    /*  36 */ float        m_obedience;             // = 1.0f getObedience
    /*  40 */ int          m_int40;                 // ctor arg `i` [UNRESOLVED]
    /*  44 */ int          m_side;                  // ctor arg `n` getSide
    /*  48 */ float        m_float48;               // = 0.01f [UNRESOLVED] a small threshold
    /*  52 */ float        m_inverseAttention;      // = 0; get/setInvAttention
    /*  56 */ BotSquad*    m_squad;                 // new(0xC)
    /*  60 */ int          m_spawnToStrategicAreaId;// = -1 getSpawnToStrategicAreaId
    /*  64 */ unsigned char m_mode;                // = 0xFF
    /*  65 */ char         m_pad65[3];
    /*  68 */ float*       m_currentOrders;         // borrowed BotOrder weights, not the order object
    /*  72 */ float*       m_ptr72;                 // = 0; per-behaviour modifiers in calculateMods
    /*  76 */ BotMemory*   m_memory;                // Borrowed; construction also initializes its first word and +16 double.
    /*  80 */ bool         m_takingOrders;          // isTakingOrders
    /*  81 */ char         m_pad81[3];
    /*  84 */ float        m_lastScout;             // = 0
    // decisionMaking (ELF 0x1893D0): +88 the equipment must be re-checked against the player's
    // vehicle, +89 the last vehicle check failed ("Bot in bad vehicle!"), +90 updateBotVehicle
    // asks for a fresh plan. The ctor zeroes +89/+90 only. [CHOSEN] names.
    /*  88 */ bool         m_checkVehicle;
    /*  89 */ bool         m_badVehicle;
    /*  90 */ bool         m_vehicleChanged;
    /*  91 */ bool         m_inTransitionPhase;     // isInTransitionPhase
    /*  92 */ char         m_pad92;
    /*  93 */ bool         m_deactivated;           // isDeactivated
    /*  94 */ char         m_pad94[2];
    // Parallel per-behaviour arrays: scalar urgency values and behaviour pointers.
    /*  96 */ float*       m_behaviourUrgencies;    // new[] sized by mu8NBehaviours - one per behaviour
    // Behaviour indices queued by urgency group (BotBehaviour +5); addBehaviour grows it and
    // decisionMaking rotates it. ELF `std::vector<std::list<unsigned short> >` (12 bytes).
    /* 100 */ std::vector<std::list<unsigned short> > m_behaviourQueues;   // ctor: empty
    /* 112 */ BotBehaviour** m_behaviours;         // new[] sized by mu8NBehaviours, zero-filled.
    // Owns the active BotScript. reset destroys it virtually and clears the pointer.
    /* 116 */ BotScript*   m_activePlan;            // Initially null; get/setPlan access it.
    /* 120 */ bool         m_planLock;              // = false get/setPlanLock
    /* 121 */ char         m_pad121[3];
    /* 124 */ WaypointList* m_waypoints;            // = 0 getWaypoints
    // Handle lists. The first element type is attested; the second is inferred.
    /* 128 */ std::list<unsigned int> m_list128;
    /* 132 */ std::list<unsigned int> m_list132;    // [INFERRED] element type, by symmetry with
                                                    // Signed behaviour selector; reset uses -1 for no selection.
    /* 136 */ signed char  m_orderedBehaviour;      // reset: -1 isOrderedBehaviour
    /* 137 */ char         m_pad137[3];
    /* 140 */ unsigned int m_currentEquipment;      // = -1 get/setCurrentEquipment
    /* 144 */ PlanInterpreter*   m_planInterpreter; // ctor arg
    /* 148 */ UrgencyGenerator*  m_urgencyMerger;   // ctor arg `merger`
    /* 152 */ UrgencyGenerator** m_urgencyGenerators;// ctor arg `generators`
    /* 156 */ IAIPlayer*   m_player;                // ctor arg; getPlayer reads it
    /* 160 */ Vec3         m_lookDir;               // zeroed before and after setPlayerInput
    // reset clears this double before the other fields.
    /* 172 */ double       m_double172;             // reset: 0.0
    /* 180 */ FireCorrectionData* m_fireCorrection; // new(0x80)
    /* 184 */ std::list<void*> m_list184;    // allocate(12) - see +128
    /* 188 */ BotNavigation*      m_navigation;     // new(0x164)
    /* 192 */ SensingAgent*       m_sensingAgent;   // new(0x9C), ctor takes mu8NBehaviours
    /* 196 */ CommunicationAgent* m_communication;  // new(0x70)
    /* 200 */ int          m_int200;                // reset: -1 (not a gap)
    // Independent task targets, initially -1. reset preserves change and revive targets.
    /* 204 */ unsigned int m_changeTarget;          // = -1
    /* 208 */ unsigned int m_repairTarget;          // = -1
    /* 212 */ unsigned int m_bridgeRepairTarget;    // = -1
    /* 216 */ unsigned int m_wantedRepairTarget;    // = -1
    /* 220 */ unsigned int m_reviveTarget;          // = -1
    // Three flags, not a gap - `reset` zeroes +224, +225 and +226.
    /* 224 */ bool         m_isRepaired;            // reset: false
    /* 225 */ bool         m_isEngineerTarget;      // reset: false
    /* 226 */ bool         m_shouldWaitForRepairs;  // reset: false
    /* 227 */ char         m_pad227;
    /* 228 */ unsigned int m_mineTarget;            // = -1
    /* 232 */ void*        m_ptr232;                // = 0; WPBeachLanding stores its AILandingZone*
    /* 236 */ float        m_moveInclination;       // = 0
    /* 240 */ float        m_fireInclination;       // = 0
    /* 244 */ float        m_moveInclinationDelta;  // = 1.0f addToMoveInclination accumulates here
    /* 248 */ float        m_fireInclinationDelta;  // = 1.0f
    /* 252 */ void*        m_ptr252;                // = 0
    /* 256 */ void*        m_ptr256;                // = 0
    /* 260 */ void*        m_ptr260;                // = 0
    /* 264 */ unsigned int m_firingTarget;          // = -1
    /* 268 */ double       m_firingTargetTime;      // getFiringTargetTime
    /* 276 */ double       m_time276;               // = -DBL_MAX - "never happened"; setHadPrecisionTimeStamp
    /* 284 */ bool         m_botStuck;              // = false
    // pollRequestedSoldierPose (ELF 0x18CEE0): a pose input is held this tick; requestSoldierPose
    // refuses an unforced change while it is set. [CHOSEN] name.
    /* 285 */ bool         m_poseInputActive;       // = false
    /* 286 */ char         m_pad286[2];
    // Construction uses -DBL_MAX as the timestamp sentinel; reset uses -9999.0.
    // When pollRequestedSoldierPose last pressed lie/crouch; botInPoseTransition is 0.8 s after it.
    /* 288 */ double       m_poseChangeTime;               // ctor: -DBL_MAX, reset: -9999.0
    /* 296 */ int          m_requestedSoldierPose;  // = 1 dice spells the getter "Solder"
    // setPoseStatus / checkPoseOk / clearTestResults: [0],[1] crouch, [2],[3] lie.
    /* 300 */ bool         m_poseTestResults[4];            // +300..+303, all false
    // the firing target the pose tests were made against (setPoseStatus); -1 = none.
    /* 304 */ int          m_poseTestTarget;                // = -1
    /* 308 */ unsigned int m_unresolvedCollision;   // = -1
    /* 312 */ bool         m_airBorn;               // = false isAirBorn
    // isJustSpawned (ELF 0x18BB80) sets this byte while the player is dead and clears it on the
    // first alive call. [CHOSEN] name.
    /* 313 */ bool         m_spawnPending;
    /* 314 */ char         m_pad314[2];
    // get/setSpawnCoord (ELF 0x18BBD0 / 0x18BBE0) return and copy this Vec3.
    /* 316 */ float        m_spawnCoord[3];         // +316..+324, all = 0
    /* 328 */ IBotDeathCallBack*  m_deathCallBack;  // = 0
    /* 332 */ IBotSpawnCallBack*  m_spawnCallBack;  // = 0
    /* 336 */ char         m_gap336[4];             // [UNRESOLVED]
    // Sensed objects returned by reference; construction reserves capacity for 256 players.
    /* 340 */ std::vector<IAIObject*> m_sensedObjects;
    /* 352 */ double       m_lastChangeTime;        // setLastChangeTime
    /* 360 */ bool         m_manDown;               // get/setManDown
    /* 361 */ char         m_pad361[3];
    // A double, found by `reset`. The old 12-byte gap swallowed it.
    // rearm (ELF 0x18D7D0) accumulates the time increase here and rearms past 6 s.
    /* 364 */ double       m_rearmTimer;             // reset: 0.0
    /* 372 */ bool         m_isFiring;              // isFiring / setIsFiring
    /* 373 */ char         m_pad373[3];
    /* 376 */ SquadMember* m_squadMember;           // = 0
    /* 380 */ bool         m_fireAllowed;           // = true
    /* 381 */ char         m_pad381[3];
    // four, and the second pair is A copy of the first - see copyAssignedAreas.
    /* 384 */ int          m_assignedAreas[4];      // +384..+396, all = -1
    /* 400 */ float        m_currentWaypoint[3];    // = 0 (three zero stores); a Vec3 - set/getCurrentWaypoint
    /* 412 */ int          m_int412;                // = -1; setCurrentWaypoint's int
    // [INFERRED] Consecutive plan-executor skips, cleared when execution resumes.
    // A signed comparison against 15 decides when a stalled bot stops coasting.
    /* 416 */ int          m_stalledPlanTicks;      // = 0

    // +420..+459: placed (= false, the ctor's last field store), type, position (+428..+436, the
    // ctor zeroes it), object, placed time, life time.
    /* 420 */ C4State      m_c4;
    // Three grenade timestamps. getTimeOfGrenadeThrown requires an index in [0, 3);
    // it performs no bounds check and larger indices read adjacent attack timestamps.
    /* 460 */ double       m_grenadeThrownTime[3];  // +460, +468, +476
    /* 484 */ double       m_timeOfLastFailedStaticAttack;     // = 0.0
    /* 492 */ double       m_timeOfLastFailedStaticSkyAttack;  // = 0.0
    /* 500 */ bool         m_wantsToSimulateArtillery;   // = false
    /* 501 */ bool         m_isSimulatingArtillery;      // = false
    // requestSoldierPose(pose, true) sets it; pollRequestedSoldierPose clears it every call.
    /* 502 */ bool         m_forcePose;                    // = false [UNRESOLVED]
    /* 503 */ char         m_pad503;
    /* 504 */ int          m_moveToStatus;          // = 0
    /* 508 */ int          m_avoidSide;             // = 0
    /* 512 */ bool         m_avoidSideChanged;      // +512 byte = 0; setAvoidSide: switched between two non-zero sides [INFERRED]
    /* 513 */ char         m_pad513[3];
    /* 516 */ double       m_double516;             // = 0.0; setMoveToStatus: the game time of the last status change
    /* 524 */ unsigned int m_collidedObject;        // = -1
    /* 528 */ float        m_collidedObjectPosition[3];  // a Vec3 - getCollidedObjectPosition
    /* 540 */ bool         m_ignoreObstacles;       // = false
    /* 541 */ char         m_pad541[3];
    /* 544 */ unsigned int m_slowDownToTarget;      // = -1
    /* 548 */ double       m_friendsInLineOfFireTime;   // = 0.0
    /* 556 */ double       m_lastCheckMoveToFinishedTime;  // = -9999.0, a magnitude sentinel
    /* 564 */ bool         m_isGoingForFlag;        // = false
    /* 565 */ bool         m_useStaticAttack;       // = false
    /* 566 */ char         m_pad566[2];
    /* 568 */ double       m_timeOfLastFailedSplashDamageAttack;  // = -9999.0
    /* 576 */ float        m_takeOffPosition[3];    // Vec3, = Vec3::getZero
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_BOTMAIN_H
