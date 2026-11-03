// AI configuration exposed through separate query and modifier interfaces.
// Use typed interface conversions to account for the secondary base subobject.

#ifndef AI_AIDLL_AI_MAIN_AISETTINGS_H        // [INFERRED] guard style
#define AI_AIDLL_AI_MAIN_AISETTINGS_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <istream>

#include "../AIStrategic/StatusMap.h"
#include "../AIStrategic/StrategyCondition.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "AISettingsInterfaces.h"      // the two bases, all 87 slots - RW-9's retirement
#include "AIStreamOperators.h"         // the console argument extractors

namespace dice {
namespace hfe {
namespace ai {

class BAPEntry;
class BotBehaviour;
class BotMoralAdmin;
class ISensingAgentPattern;
class PlanGenerator;
class PlanInterpreter;

// The two abstract halves are declared in AISettingsInterfaces.h with all 87 virtual positions.
// AISettings is the concrete implementation in both recovered images.
class IAISettings;
class IAISettingsModifier;

// sizeof == 284 (0x11C), from `operator new(0x11Cu)` at BF2AI::init. The last field is at +281.
// The multiple-inheritance declaration below is part of the recovered registry and vtable contract.
class AISettings : public IAISettings, public IAISettingsModifier
{
public:
    AISettings();
    virtual ~AISettings();

    // IAISettings, slots 0..47 in vtable order
    virtual int getMaxNBots() const;
    virtual bool getmaxBotsIncludeHumans() const;
    virtual float getIgnoreCommanderDistance() const;
    virtual int getNumberOfBehaviours() const;
    virtual int getNumberOfVehicles() const;
    virtual float* getVehicleMods(int) const;
    virtual BotBehaviour* getBehaviour(int, int) const;
    virtual BotBehaviour* getDefaultBehaviour(int) const;
    virtual PlanInterpreter* getPlanInterpreter(int) const;
    virtual PlanGenerator* getPlanGenerator(int, int) const;
    virtual float getViewDistance() const;
    virtual float getStatsViewDistance() const;
    virtual bool getDrawBotStats() const;
    virtual int getNBattleStrengths();
    virtual int getNStrategicStrengths();
    virtual int getInformationGridDimension() const;
    virtual int getNSides() const;
    virtual float getBotSkill() const;
    virtual float getSAIUpdateFrequency() const;
    virtual bool isRespawnAllowed() const;
    virtual const std::string& getVehicle(int) const;
    virtual int getVehicle(const std::string&) const;
    virtual const std::string& getVehicleClusterMapping(const std::string&) const;
    virtual int getClusterCostType(int) const;
    virtual const std::vector<int>& getClusterCostTypes() const;
    virtual int getNumberOfVehicleGroups() const;
    virtual int getVehicleGroup(int) const;
    virtual int getVehicleGroup(const std::string&) const;
    virtual const std::string& getVehicleGroupName(int) const;
    virtual float* getStandardPersonality(int) const;
    virtual BotMoralAdmin* getStandardMoralAdmin(int) const;
    virtual bool hasActiveRenderer() const;
    virtual bool getDraw() const;
    virtual const std::map<int, ISensingAgentPattern*>& getSensingAgentPatterns() const;
    virtual ISensingAgentPattern* getSensingAgentPattern(int) const;
    virtual ISensingAgentPattern* getSensingAgentNullPattern() const;
    virtual bool getAutoSpawnBots() const;
    virtual const float* getArmourClassValues() const;
    virtual float getArmourClassValue(int) const;
    virtual bool reset();
    virtual int getManDownEquipmentType();
    virtual bool isValidMaterial(int, unsigned char) const;
    virtual float getVehicleMaterialCost(const std::string&, unsigned char) const;
    virtual float getVehicleMaterialCost(int, unsigned char) const;
    virtual MaterialManager* getVehicleMaterialManager(int) const;
    // Returns a reference to the selected bot-name string in the list.
    virtual const std::string& getBotName() const;
    virtual bool overrideMenuSettings() const;
    virtual bool getOverrideMenuSettings() const;

    // The IAISettingsModifier overriders, in AISettings's own primary-vtable order (ELF vtable for
    // AISettings +0xC8..+0x15C; the order differs from the interface's: createVehicleGroup precedes
    // setVehicle, and setAutoSpawnBots follows addSensingAgentPattern). The interface's own slot
    // order is IAISettingsModifier's declaration order. reset is shared with the read side above
    // and is therefore declared only once.
    virtual void setMaxNBots(int);
    virtual void maxBotsIncludeHumans(bool);
    virtual void setIgnoreCommanderDistance(float);
    virtual void setMaxNumberOfBehaviours(int);
    virtual void setVehicleMods(int, float *);
    virtual void addBehaviour(int, int, BotBehaviour *);
    virtual void setDefaultBehaviour(int, int);
    virtual void addInterpreterEntry(int, BAPEntry *);
    virtual void addInterpreterEntry(int, BotLodLevel, BAPEntry *);
    virtual void removeInterpreterEntry(int, BotLodLevel, BAPEntry *);
    virtual void activateDefaultInterpreter(int, BAPStatementType, BotLodLevel);
    virtual void addGenerator(int, int, PlanGenerator *);
    virtual void setBasicBotWeights(float *);
    virtual void setViewDistance(float);
    virtual void setStatsViewDistance(float);
    virtual void setDrawBotStats(bool);
    virtual void setInformationGridDimension(int);
    virtual void setNSides(int);
    virtual void setBotSkill(float);
    virtual void setSAIUpdateFrequency(float);
    virtual void createVehicleGroup(const std::string&);
    virtual void setVehicle(const std::string&);
    virtual void setVehicleClusterMapping(const std::string&, const std::string&);
    virtual void addVehicleToVehicleGroup(int, const std::string&);
    virtual void addVehicleToVehicleGroup(const std::string&, const std::string&);
    virtual void setStandardPersonality(float *, int);
    virtual void setStandardMoralAdmin(BotMoralAdmin *, int);
    virtual void setRespawnAllowed(bool);
    virtual void setDraw(bool);
    virtual void setSensingAgentPatternName(const std::string&);
    virtual void setSensingAgentNullPattern(ISensingAgentPattern *);
    virtual void addSensingAgentPattern(int, ISensingAgentPattern *);
    virtual void setAutoSpawnBots(bool);
    virtual void setManDownEquipment(const std::string&);
    virtual void setVehicleMaterialCost(const std::string&, unsigned char, float);
    virtual void addBotName(const std::string&, const std::string&);
    virtual void setBotNameUnused(const std::string&);
    virtual void setOverrideMenuSettings(bool);

private:
    // Non-virtual pool helpers (direct PLT calls), ELF 0x3BFA20 / 0x3BFC50 / 0x3BFDE0 / 0x3C0040.
    void prepareBehaviourPool(int vehicle, int index);
    void preparePlanGeneratorPool(int vehicle, int index);
    void preparePlanInterpreterPool(int vehicle);
    void prepareVehicleGroup(int vehicle);

    // Field offsets describe retail x86 layout; native STL sizes may differ.
    /*   8 */ int    m_maxNBots;                  // = -1 getMaxNBots / setMaxNBots
    /*  12 */ int    m_numberOfBehaviours;        // = 0 getNumberOfBehaviours
    /*  16 */ char   m_gap16[8];                  // [UNRESOLVED] two words reset zeroes; ctor leaves them alone
    /*  24 */ float  m_viewDistance;              // = -1.0f getViewDistance
    /*  28 */ float  m_statsViewDistance;         // = 30.0f get/setStatsViewDistance
    /*  32 */ float  m_botSkill;                  // = 0.75f get/setBotSkill
    /*  36 */ float  m_saiUpdateFrequency;        // = 35.0f get/setSAIUpdateFrequency
    /*  40 */ int    m_informationGridDimension;  // = 32 get/setInformationGridDimension
    /*  44 */ int    m_nSides;                    // = 0 get/setNSides
    /*  48 */ bool   m_respawnAllowed;            // = true isRespawnAllowed / setRespawnAllowed
    /*  49 */ bool   m_draw;                      // = false getDraw / setDraw
    /*  50 */ bool   m_autoSpawnBots;             // = false getAutoSpawnBots / setAutoSpawnBots
    /*  51 */ bool   m_maxBotsIncludeHumans;      // = true getmaxBotsIncludeHumans (dice's casing)
    /*  52 */ float  m_ignoreCommanderDistance;   // = 30.0f get/setIgnoreCommanderDistance

    // getManDownEquipmentType is `getVehicle(m_manDownEquipment)` - a virtual call on itself
    // through slot 21. setManDownEquipment writes it.
    /*  56 */ std::string m_manDownEquipment;

    // the size of this vector is `getNumberOfVehicles`. The AI counts vehicles by counting
    // behaviour lists, one per vehicle; `getBehaviour(vehicle, i)` is `m_behaviours[vehicle][i]`.
    /*  60 */ std::vector<std::vector<BotBehaviour*> > m_behaviours;
    /*  72 */ std::vector<int> m_defaultBehaviour;          // index into m_behaviours[v]; -1 = none
    /*  84 */ std::vector<std::vector<PlanGenerator*> > m_planGenerators;
    /*  96 */ std::vector<PlanInterpreter*> m_planInterpreters;   // each a BAPInterpreter (vector<PlanInterpreter*>::_M_insert_aux)
    /* 108 */ std::map<int, ISensingAgentPattern*> m_sensingAgentPatterns;
    /* 120 */ std::vector<MaterialManager*> m_vehicleMaterialManagers;

    // Uninitialized until setSensingAgentNullPattern is called; callers must set it before use.
    /* 132 */ ISensingAgentPattern* m_sensingAgentNullPattern;

    /* 136 */ std::vector<std::string> m_vehicleGroupNames; // size == getNumberOfVehicleGroups
    /* 148 */ std::map<std::string, int> m_vehicleGroupIndex;   // createVehicleGroup inserts here
    /* 160 */ std::vector<int> m_vehicleGroups;

    // Two per-side arrays indexed by side - 1; callers must supply side 1 or 2.
    /* 172 */ BotMoralAdmin* m_standardMoralAdmin[2];
    /* 180 */ float*         m_standardPersonality[2];

    /* 188 */ std::vector<float*> m_vehicleMods;
    /* 200 */ std::map<std::string, int> m_vehicleIndex;    // setVehicle inserts; getVehicle(name) reads
    /* 212 */ std::vector<std::string> m_vehicleNames;      // setVehicle push_backs the same name here
    /* 224 */ std::vector<int> m_clusterCostTypes;
    /* 236 */ std::string m_sensingAgentPatternName;        // [INFERRED] setSensingAgentPatternName

    // Six zero-based armor-class weights, initialized to 1, 3, 8, 15, 10, and 12.
    /* 240 */ float m_armourClassValues[6];

    // Pattern name -> registration index (setSensingAgentPatternName, ELF 0x3C0C50: insert_unique of
    // the map's size before the insert). Name [INFERRED].
    /* 264 */ std::map<std::string, int> m_sensingAgentPatternIds;
    // Mutable because getBotName rotates the list while satisfying the const interface.
    /* 276 */ mutable std::list<std::pair<float, std::string> > m_botNames;   // addBotName / getBotName
    /* 280 */ bool m_overrideMenuSettings;         // = false overrideMenuSettings + getOverrideMenuSettings
    /* 281 */ bool m_drawBotStats;                 // = true getDrawBotStats / setDrawBotStats
};

// The console argument extractors moved to AIStreamOperators.h (included above).

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AISETTINGS_H
