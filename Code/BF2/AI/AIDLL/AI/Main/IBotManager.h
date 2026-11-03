// Bot-manager query and configuration interfaces. Preserve virtual declaration order.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_IBOTMANAGER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_MAIN_IBOTMANAGER_H

#include <iosfwd>
#include <string>

namespace dice {
namespace hfe {
namespace ai {

struct AIRadioMessageSettings;
class Bot;
class IAIObject;

class IBotManager
{
public:
    IBotManager();                  // , - `instance = this`
    virtual ~IBotManager();         // , - `instance = 0`

    virtual int getBotCount() const = 0;                                                            // +0x008
    virtual int getNumberOfActiveBots() const = 0;                                                  // +0x00C
    virtual int getNumberOfDeactivedBots() const = 0;                                               // +0x010
    virtual Bot* getBot(unsigned int index) const = 0;                                              // +0x014
    virtual Bot* getBotFromId(unsigned int id) const = 0;                                           // +0x018
    virtual Bot* getBotFromPlayerId(int playerId) const = 0;                                        // +0x01C
    virtual Bot* getBotFromName(const std::string& name) const = 0;                                 // +0x020
    virtual Bot* getBot(IAIObject* object) const = 0;                                               // +0x024
    virtual Bot* getLastCreatedBot() const = 0;                                                     // +0x028
    virtual void updateBotVehicle(Bot* bot, IAIObject* vehicle) = 0;                                // +0x02C
    virtual void setBotChangedVehicle(int id, IAIObject* vehicle) = 0;                              // +0x030
    virtual void setBotIsDead(int id) = 0;                                                          // +0x034
    virtual void setBotIsAlive(int id) = 0;                                                         // +0x038
    virtual void setBotIsManDown(int id, IAIObject* object) = 0;                                    // +0x03C
    virtual void setBotIsRevived(int id, IAIObject* object) = 0;                                    // +0x040
    virtual void setBotReceivedRadioMessage(const AIRadioMessageSettings& settings, int a, int b) = 0; // +0x044
    virtual void setBotRecievedAttackCommanderOrder(int id) = 0;                                    // +0x048
    virtual void setBotRecievedDefendCommanderOrder(int id) = 0;                                    // +0x04C
    virtual void setBotRecievedMoveCommanderOrder(int id) = 0;                                      // +0x050
    virtual void setBotRecievedAttackSquadOrder(int id) = 0;                                        // +0x054
    virtual void setBotRecievedDefendSquadOrder(int id) = 0;                                        // +0x058
    virtual void setBotRecievedMoveSquadOrder(int id) = 0;                                          // +0x05C
    virtual void setBotTeamKilled(int a, int b) = 0;                                                // +0x060
    virtual void registerDamageOnBot(float damage, int a, int b) = 0;                               // +0x064
    virtual float getVehicleBailValue() = 0;                                                        // +0x068
    virtual void removeMineTarget(unsigned int handle) = 0;                                         // +0x06C
    virtual bool reset() = 0;                                                                       // +0x070
    virtual void setRenderBotSensing(bool render) = 0;                                              // +0x074
    virtual bool getRenderBotSensing() const = 0;                                                   // +0x078
    virtual void writeBotPlans() = 0;                                                               // +0x07C
    virtual void increaseTabLevel() = 0;                                                            // +0x080
    virtual void decreaseTabLevel() = 0;                                                            // +0x084
    virtual void tabulate(std::ostream& out) = 0;                                                   // +0x088
    virtual void setLodLevelTicks(unsigned full, unsigned medium, unsigned light) = 0;              // +0x08C
    virtual void setLodLevelPriority(unsigned full, unsigned medium, unsigned light) = 0;           // +0x090
    virtual void setPlannedDecisionMakingThreshold(const double& full, const double& medium, const double& light) = 0; // +0x094
    virtual void setUnplannedDecisionMakingThreshold(const double& full, const double& medium, const double& light) = 0; // +0x098
    virtual void setDecisionMakingInterleave(unsigned a, unsigned b) = 0;                           // +0x09C
    virtual void setSensingQuotient(unsigned a, unsigned b) = 0;                                    // +0x0A0
    virtual void setSystemQuotient(unsigned a, unsigned b, unsigned c) = 0;                         // +0x0A4
    virtual unsigned getLodLevelFullTicks() const = 0;                                              // +0x0A8
    virtual unsigned getLodLevelMediumTicks() const = 0;                                            // +0x0AC
    virtual unsigned getLodLevelLightTicks() const = 0;                                             // +0x0B0
    virtual unsigned getLodLevelFullPriority() const = 0;                                           // +0x0B4
    virtual unsigned getLodLevelMediumPriority() const = 0;                                         // +0x0B8
    virtual unsigned getLodLevelLightPriority() const = 0;                                          // +0x0BC
    virtual double getPlannedDecisionMakingThresholdFull() const = 0;                               // +0x0C0
    virtual double getPlannedDecisionMakingThresholdMedium() const = 0;                             // +0x0C4
    virtual double getPlannedDecisionMakingThresholdLight() const = 0;                              // +0x0C8
    virtual double getUnplannedDecisionMakingThresholdFull() const = 0;                             // +0x0CC
    virtual double getUnplannedDecisionMakingThresholdMedium() const = 0;                           // +0x0D0
    virtual double getUnplannedDecisionMakingThresholdLight() const = 0;                            // +0x0D4
    virtual unsigned getPlannedDecisionMakingInterleave() const = 0;                                // +0x0D8
    virtual unsigned getUnplannedDecisionMakingInterleave() const = 0;                              // +0x0DC
    virtual double getSensingQuotient() const = 0;                                                  // +0x0E0
    virtual double getMemoryUpdateQuotient() const = 0;                                             // +0x0E4
    virtual double getSystemDecisionMakingQuotient() const = 0;                                     // +0x0E8
    virtual double getSystemPathfindingQuotient() const = 0;                                        // +0x0EC
    virtual double getSystemSensingQuotient() const = 0;                                            // +0x0F0
    virtual void resetStatistics() = 0;                                                             // +0x0F4
    virtual void resetStatistics(unsigned int index) const = 0;                                     // +0x0F8
    virtual unsigned getPlanExecutionUpdateCount(unsigned int index) const = 0;                     // +0x0FC
    virtual unsigned getDecisionMakingUpdateCount(unsigned int index) const = 0;                    // +0x100
    virtual unsigned getSensingUpdateCount(unsigned int index) const = 0;                           // +0x104
    virtual unsigned getDecisionMakingBehaviourUpdateCount(unsigned int index) const = 0;           // +0x108
    virtual double getPlanExecutionRunTime(unsigned int index) const = 0;                           // +0x10C
    virtual double getDecisionMakingRunTime(unsigned int index) const = 0;                          // +0x110
    virtual double getSensingRunTime(unsigned int index) const = 0;                                 // +0x114
    virtual bool isPathfinding(unsigned int index) const = 0;                                       // +0x118
    virtual bool getLastPathfindingSucceeded(unsigned int index) const = 0;                         // +0x11C
    virtual bool getPathfindingStall(unsigned int index) const = 0;                                 // +0x120
    virtual double getLastPathfindingRunTime(unsigned int index) const = 0;                         // +0x124
    virtual void enableBotScriptDebugger(bool enable) = 0;                                          // +0x128

    static IBotManager* instance;
};

// The tuning face. Every slot here is also a slot of `IBotManager`; the trailing comment on each
// line gives the primary's offset so the two tables can be diffed.
class IBotManagerModifier
{
public:
    IBotManagerModifier();
    virtual ~IBotManagerModifier();

    virtual void setLodLevelTicks(unsigned full, unsigned medium, unsigned light) = 0;              // +0x008 (IBotManager +0x08C)
    virtual void setLodLevelPriority(unsigned full, unsigned medium, unsigned light) = 0;           // +0x00C (IBotManager +0x090)
    virtual void setPlannedDecisionMakingThreshold(const double& full, const double& medium, const double& light) = 0; // +0x010 (IBotManager +0x094)
    virtual void setUnplannedDecisionMakingThreshold(const double& full, const double& medium, const double& light) = 0; // +0x014 (IBotManager +0x098)
    virtual void setDecisionMakingInterleave(unsigned a, unsigned b) = 0;                           // +0x018 (IBotManager +0x09C)
    virtual void setSensingQuotient(unsigned a, unsigned b) = 0;                                    // +0x01C (IBotManager +0x0A0)
    virtual void setSystemQuotient(unsigned a, unsigned b, unsigned c) = 0;                         // +0x020 (IBotManager +0x0A4)
    virtual unsigned getLodLevelFullTicks() const = 0;                                              // +0x024 (IBotManager +0x0A8)
    virtual unsigned getLodLevelMediumTicks() const = 0;                                            // +0x028 (IBotManager +0x0AC)
    virtual unsigned getLodLevelLightTicks() const = 0;                                             // +0x02C (IBotManager +0x0B0)
    virtual unsigned getLodLevelFullPriority() const = 0;                                           // +0x030 (IBotManager +0x0B4)
    virtual unsigned getLodLevelMediumPriority() const = 0;                                         // +0x034 (IBotManager +0x0B8)
    virtual unsigned getLodLevelLightPriority() const = 0;                                          // +0x038 (IBotManager +0x0BC)
    virtual double getPlannedDecisionMakingThresholdFull() const = 0;                               // +0x03C (IBotManager +0x0C0)
    virtual double getPlannedDecisionMakingThresholdMedium() const = 0;                             // +0x040 (IBotManager +0x0C4)
    virtual double getPlannedDecisionMakingThresholdLight() const = 0;                              // +0x044 (IBotManager +0x0C8)
    virtual double getUnplannedDecisionMakingThresholdFull() const = 0;                             // +0x048 (IBotManager +0x0CC)
    virtual double getUnplannedDecisionMakingThresholdMedium() const = 0;                           // +0x04C (IBotManager +0x0D0)
    virtual double getUnplannedDecisionMakingThresholdLight() const = 0;                            // +0x050 (IBotManager +0x0D4)
    virtual unsigned getPlannedDecisionMakingInterleave() const = 0;                                // +0x054 (IBotManager +0x0D8)
    virtual unsigned getUnplannedDecisionMakingInterleave() const = 0;                              // +0x058 (IBotManager +0x0DC)
    virtual double getSensingQuotient() const = 0;                                                  // +0x05C (IBotManager +0x0E0)
    virtual double getMemoryUpdateQuotient() const = 0;                                             // +0x060 (IBotManager +0x0E4)
    virtual double getSystemDecisionMakingQuotient() const = 0;                                     // +0x064 (IBotManager +0x0E8)
    virtual double getSystemPathfindingQuotient() const = 0;                                        // +0x068 (IBotManager +0x0EC)
    virtual double getSystemSensingQuotient() const = 0;                                            // +0x06C (IBotManager +0x0F0)
    virtual int getBotCount() const = 0;                                                            // +0x070 (IBotManager +0x008)
    virtual void resetStatistics() = 0;                                                             // +0x074 (IBotManager +0x0F4)
    virtual void resetStatistics(unsigned int index) const = 0;                                     // +0x078 (IBotManager +0x0F8)
    virtual unsigned getPlanExecutionUpdateCount(unsigned int index) const = 0;                     // +0x07C (IBotManager +0x0FC)
    virtual unsigned getDecisionMakingUpdateCount(unsigned int index) const = 0;                    // +0x080 (IBotManager +0x100)
    virtual unsigned getSensingUpdateCount(unsigned int index) const = 0;                           // +0x084 (IBotManager +0x104)
    virtual double getPlanExecutionRunTime(unsigned int index) const = 0;                           // +0x088 (IBotManager +0x10C)
    virtual double getDecisionMakingRunTime(unsigned int index) const = 0;                          // +0x08C (IBotManager +0x110)
    virtual double getSensingRunTime(unsigned int index) const = 0;                                 // +0x090 (IBotManager +0x114)
    virtual bool isPathfinding(unsigned int index) const = 0;                                       // +0x094 (IBotManager +0x118)
    virtual bool getLastPathfindingSucceeded(unsigned int index) const = 0;                         // +0x098 (IBotManager +0x11C)
    virtual bool getPathfindingStall(unsigned int index) const = 0;                                 // +0x09C (IBotManager +0x120)
    virtual double getLastPathfindingRunTime(unsigned int index) const = 0;                         // +0x0A0 (IBotManager +0x124)
    virtual bool reset() = 0;                                                                       // +0x0A4 (IBotManager +0x070)
    virtual void enableBotScriptDebugger(bool enable) = 0;                                          // +0x0A8 (IBotManager +0x128)

    static IBotManagerModifier* instance;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_IBOTMANAGER_H
