// Query and modifier interfaces for AI settings. Preserve their virtual declaration order.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_AISETTINGSINTERFACES_H
#define AI_AIDLL_AI_MAIN_AISETTINGSINTERFACES_H

#include <map>
#include <string>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

class BAPEntry;
class MaterialManager;
class BotBehaviour;
class BotMoralAdmin;
class ISensingAgentPattern;
class PlanGenerator;
class PlanInterpreter;

// [UNRESOLVED] enumerators. Both are one dword on the stack at every call site.
enum BotLodLevel : int;
enum BAPStatementType : int;

// IAISettings - 50 vtable slots: 48 accessors then the destructor pair.
// First-wins on CommonSingletonServer::m_iaiSettings.
class IAISettings
{
public:
    IAISettings();
    virtual int getMaxNBots() const = 0;
    virtual bool getmaxBotsIncludeHumans() const = 0;
    virtual float getIgnoreCommanderDistance() const = 0;
    virtual int getNumberOfBehaviours() const = 0;
    virtual int getNumberOfVehicles() const = 0;
    virtual float* getVehicleMods(int) const = 0;
    virtual BotBehaviour* getBehaviour(int, int) const = 0;
    virtual BotBehaviour* getDefaultBehaviour(int) const = 0;
    virtual PlanInterpreter* getPlanInterpreter(int) const = 0;
    virtual PlanGenerator* getPlanGenerator(int, int) const = 0;
    virtual float getViewDistance() const = 0;
    virtual float getStatsViewDistance() const = 0;
    virtual bool getDrawBotStats() const = 0;
    virtual int getNBattleStrengths() = 0;
    virtual int getNStrategicStrengths() = 0;
    virtual int getInformationGridDimension() const = 0;
    virtual int getNSides() const = 0;
    virtual float getBotSkill() const = 0;
    virtual float getSAIUpdateFrequency() const = 0;
    virtual bool isRespawnAllowed() const = 0;
    // Paired ELF/PE callers consume the borrowed vector element as a std::string. The previous
    // void* declaration preserved the register ABI but hid the recovered source contract. The
    // reference spelling is [INFERRED]; borrowed object identity and const use are directly evidenced.
    virtual const std::string& getVehicle(int) const = 0;
    virtual int getVehicle(const std::string&) const = 0;
    virtual const std::string& getVehicleClusterMapping(const std::string&) const = 0;
    virtual int getClusterCostType(int) const = 0;
    virtual const std::vector<int>& getClusterCostTypes() const = 0;
    virtual int getNumberOfVehicleGroups() const = 0;
    virtual int getVehicleGroup(int) const = 0;
    virtual int getVehicleGroup(const std::string&) const = 0;
    virtual const std::string& getVehicleGroupName(int) const = 0;
    virtual float* getStandardPersonality(int) const = 0;
    virtual BotMoralAdmin* getStandardMoralAdmin(int) const = 0;
    virtual bool hasActiveRenderer() const = 0;
    virtual bool getDraw() const = 0;
    virtual const std::map<int, ISensingAgentPattern*>& getSensingAgentPatterns() const = 0;
    virtual ISensingAgentPattern* getSensingAgentPattern(int) const = 0;
    virtual ISensingAgentPattern* getSensingAgentNullPattern() const = 0;
    virtual bool getAutoSpawnBots() const = 0;
    virtual const float* getArmourClassValues() const = 0;
    virtual float getArmourClassValue(int) const = 0;
    virtual bool reset() = 0;
    virtual int getManDownEquipmentType() = 0;
    virtual bool isValidMaterial(int, unsigned char) const = 0;
    virtual float getVehicleMaterialCost(const std::string&, unsigned char) const = 0;
    virtual float getVehicleMaterialCost(int, unsigned char) const = 0;
    virtual MaterialManager* getVehicleMaterialManager(int) const = 0;
    // Returns a reference to the selected bot-name string.
    virtual const std::string& getBotName() const = 0;
    virtual bool overrideMenuSettings() const = 0;
    virtual bool getOverrideMenuSettings() const = 0;
    virtual ~IAISettings();                   // slot 48, see the note above
};

// IAISettingsModifier - 41 vtable slots: 39 mutators then the destructor pair, reached at +4.
// First-wins on CommonSingletonServer::m_iaiSettingsModifier.
class IAISettingsModifier
{
public:
    IAISettingsModifier();
    virtual void setMaxNBots(int) = 0;
    virtual void maxBotsIncludeHumans(bool) = 0;
    virtual void setIgnoreCommanderDistance(float) = 0;
    virtual void setMaxNumberOfBehaviours(int) = 0;
    virtual void setVehicleMods(int, float *) = 0;
    virtual void addBehaviour(int, int, BotBehaviour *) = 0;
    virtual void setDefaultBehaviour(int, int) = 0;
    virtual void addInterpreterEntry(int, BAPEntry *) = 0;
    virtual void addInterpreterEntry(int, BotLodLevel, BAPEntry *) = 0;
    virtual void removeInterpreterEntry(int, BotLodLevel, BAPEntry *) = 0;
    virtual void activateDefaultInterpreter(int, BAPStatementType, BotLodLevel) = 0;
    virtual void addGenerator(int, int, PlanGenerator *) = 0;
    virtual void setBasicBotWeights(float *) = 0;
    virtual void setViewDistance(float) = 0;
    virtual void setStatsViewDistance(float) = 0;
    virtual void setDrawBotStats(bool) = 0;
    virtual void setInformationGridDimension(int) = 0;
    virtual void setNSides(int) = 0;
    virtual void setBotSkill(float) = 0;
    virtual void setSAIUpdateFrequency(float) = 0;
    virtual void setStandardPersonality(float *, int) = 0;
    virtual void setStandardMoralAdmin(BotMoralAdmin *, int) = 0;
    virtual void setRespawnAllowed(bool) = 0;
    virtual void setVehicle(const std::string&) = 0;
    virtual void setVehicleClusterMapping(const std::string&, const std::string&) = 0;
    virtual void createVehicleGroup(const std::string&) = 0;
    virtual void addVehicleToVehicleGroup(int, const std::string&) = 0;
    virtual void addVehicleToVehicleGroup(const std::string&, const std::string&) = 0;
    virtual void setAutoSpawnBots(bool) = 0;
    virtual void setDraw(bool) = 0;
    virtual void setSensingAgentPatternName(const std::string&) = 0;
    virtual void setSensingAgentNullPattern(ISensingAgentPattern *) = 0;
    virtual void addSensingAgentPattern(int, ISensingAgentPattern *) = 0;
    virtual bool reset() = 0;
    virtual void setManDownEquipment(const std::string&) = 0;
    virtual void setVehicleMaterialCost(const std::string&, unsigned char, float) = 0;
    virtual void addBotName(const std::string&, const std::string&) = 0;
    virtual void setBotNameUnused(const std::string&) = 0;
    virtual void setOverrideMenuSettings(bool) = 0;
    virtual ~IAISettingsModifier();           // slot 39, see the note above
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AISETTINGSINTERFACES_H
