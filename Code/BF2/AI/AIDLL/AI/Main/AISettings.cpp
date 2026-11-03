// AI configuration, bot names and console bindings.

#include "AISettings.h"


#include <cstring>
#include <ios>
#include "../../../../Core/Debug.h"
#include "../../../../Core/ClassManager.h"
#include "CommonSingletonServer.h"
#include "AIMain.h"                                // IAIMain::spawnBots - setAutoSpawnBots' trigger
#include "AILODManager.h"                          // setViewDistance's second broadcast
#include "AIRandom.h"                            // addBotName's weight draw
#include "AIRadio.h"                             // the destructor's first delete
#include "IBotManager.h"                           // setViewDistance walks the bot pool
#include "../AIBot/BotMain.h"                      // Bot::isDeactivated / Bot::getPlayer
#include "../AIPathFinding/Material/Material.h"     // materialId console spelling and extractor
#include "../AIPathFinding/Material/MaterialManager.h"
#include "../AIUtils/checkPointer.h"
#include "../AIPlanInterpreter/BAPInterpreter.h"   // the per-vehicle plan interpreter pool
#include "../AIBotBehaviour/BotBehaviour.h"           // the destructor's virtual deleting destructors
#include "../AIBotBehaviourPattern/PlanGenerator.h"   // (complete types, or delete skips them)
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"  // IAIPlayer::setViewDistance, slot +0x68
#include "../AIUtils/DebugRendering/AIRenderUtil.h"

namespace {
// [CHOSEN, not recovered] These three original TUs share this source file; retain their
// three independent stream-initialization lifetimes with explicit objects.
// static init: ELF 0x3C1570 (__tcf_0 0x3C15D0, _GLOBAL__I 0x3C1600).
std::ios_base::Init aiSettingsStreams;
// static init: ELF 0x3E7400 (__tcf_0 0x3E7460, _GLOBAL__I 0x3E7490).
std::ios_base::Init iaiSettingsStreams;
// static init: ELF 0x3E77B0 (__tcf_0 0x3E7810, _GLOBAL__I 0x3E7840).
std::ios_base::Init iaiSettingsModifierStreams;
}

namespace dice {
namespace hfe {
extern ClassManager* classManager;

namespace ai {

// Returns whether AI configuration overrides the game menu settings.
// ELF 0x3C1530.
bool AISettings::overrideMenuSettings() const
{
    return m_overrideMenuSettings;
}

// The same byte, under a second name. See above.
// ELF 0x3C1540.
bool AISettings::getOverrideMenuSettings() const
{
    return m_overrideMenuSettings;
}


// Recovered accessors follow the shipped declaration order.
// ELF 0x3BEF90.
int AISettings::getNumberOfBehaviours() const
{
    return m_numberOfBehaviours;
}

// ELF 0x3BEFA0.
int AISettings::getNumberOfVehicles() const
{
    return static_cast<int>(m_behaviours.size());
}

// ELF 0x3C0EF0.
float* AISettings::getVehicleMods(int vehicle) const
{
    return m_vehicleMods[vehicle];
}

// ELF 0x3BF640.
BotBehaviour* AISettings::getBehaviour(int vehicle, int index) const
{
    return m_behaviours[vehicle][index];
}

// ELF 0x3BF670.
BotBehaviour* AISettings::getDefaultBehaviour(int vehicle) const
{
    const int index = m_defaultBehaviour[vehicle];
    if (index != -1)
        return m_behaviours[vehicle][index];
    return 0;
}

// ELF 0x3BF6B0.
PlanInterpreter* AISettings::getPlanInterpreter(int vehicle) const
{
    return m_planInterpreters[vehicle];
}

// ELF 0x3BF6D0.
PlanGenerator* AISettings::getPlanGenerator(int vehicle, int index) const
{
    return m_planGenerators[vehicle][index];
}

// ELF 0x3C0230.
float AISettings::getStatsViewDistance() const
{
    return m_statsViewDistance;
}

// ELF 0x3C0250.
bool AISettings::getDrawBotStats() const
{
    return m_drawBotStats;
}

// ELF 0x3C01F0.
int AISettings::getNBattleStrengths()
{
    return 6;
}

// ELF 0x3C0200.
int AISettings::getNStrategicStrengths()
{
    return 2;
}

// ELF 0x3C0220.
int AISettings::getInformationGridDimension() const
{
    return m_informationGridDimension;
}

// ELF 0x3C0A00.
float AISettings::getSAIUpdateFrequency() const
{
    return m_saiUpdateFrequency;
}

// [INFERRED] reference spelling, proven borrowed string object.
// ELF 0x3BF320.
const std::string& AISettings::getVehicle(int vehicle) const
{
    if (vehicle < 0 || vehicle >= static_cast<int>(m_vehicleNames.size()))
    {
        Debug(DebugTypeError, __FILE__, __LINE__, "") << "Error: Illegal vehicle id: " << vehicle;

        // Shipped fallthrough if Debug ever returns; DebugTypeError aborts in this tree.
        return m_sensingAgentPatternName;
    }

    return m_vehicleNames[vehicle];
}

// ELF 0x3BF4A0.
int AISettings::getVehicle(const std::string& name) const
{
    // (!) guarded, and the sibling below is not. Same map type, same find, opposite discipline.
    const std::map<std::string, int>::const_iterator i = m_vehicleIndex.find(name);
    if (i == m_vehicleIndex.end())
        return -1;
    return i->second;
}

// Every lookup/index is unchecked in both images.
// ELF 0x3BF500.
const std::string& AISettings::getVehicleClusterMapping(const std::string& name) const
{
    const int vehicle = m_vehicleIndex.find(name)->second;
    return m_vehicleNames[m_clusterCostTypes[vehicle]];
}

// ELF 0x3BF570.
int AISettings::getClusterCostType(int index) const
{
    return m_clusterCostTypes[index];
}

// ELF 0x3BF590.
const std::vector<int>& AISettings::getClusterCostTypes() const
{
    return m_clusterCostTypes;
}

// ELF 0x3C0950.
int AISettings::getNumberOfVehicleGroups() const
{
    return static_cast<int>(m_vehicleGroupNames.size());
}

// ELF 0x3C0970.
int AISettings::getVehicleGroup(int index) const
{
    return m_vehicleGroups[index];
}

// ELF 0x3C0990.
int AISettings::getVehicleGroup(const std::string& name) const
{
    // Requires an existing group name; the lookup result is dereferenced without checking end().
    return m_vehicleGroupIndex.find(name)->second;
}

// ELF 0x3C09E0.
const std::string& AISettings::getVehicleGroupName(int group) const
{
    return m_vehicleGroupNames[group];
}

// ELF 0x3C0A60.
float* AISettings::getStandardPersonality(int side) const
{
    // (!) 1-based. The binary computes `this + 176 + 4*side`, i.e. this array at +180 subscripted
    // [side - 1]. See the layout note.
    return m_standardPersonality[side - 1];
}

// ELF 0x3C0A80.
BotMoralAdmin* AISettings::getStandardMoralAdmin(int side) const
{
    return m_standardMoralAdmin[side - 1];
}

// ELF 0x3C0AD0.
bool AISettings::hasActiveRenderer() const
{
    // This unsupported operation emits an error diagnostic.
    Debug(DebugTypeError, __FILE__, __LINE__, "") << "Does not work.";
    return false;
}

// ELF 0x3C0C00.
bool AISettings::getDraw() const
{
    return m_draw;
}

// - a bare member return, the shape every accessor in this run has.
// ELF 0x3C4140.
const std::map<int, ISensingAgentPattern*>& AISettings::getSensingAgentPatterns() const
{
    return m_sensingAgentPatterns;
}

// ELF 0x3C0E20.
ISensingAgentPattern* AISettings::getSensingAgentPattern(int id) const
{
    const std::map<int, ISensingAgentPattern*>::const_iterator i = m_sensingAgentPatterns.find(id);
    if (i == m_sensingAgentPatterns.end())
        return 0;
    return i->second;
}

// ELF 0x3C0E70.
ISensingAgentPattern* AISettings::getSensingAgentNullPattern() const
{
    return m_sensingAgentNullPattern;
}

// ELF 0x3C0FD0.
int AISettings::getManDownEquipmentType()
{
    // Preserve virtual dispatch so derived getVehicle implementations are honored.
    return getVehicle(m_manDownEquipment);
}

// ELF 0x3C1090.
bool AISettings::isValidMaterial(int vehicle, unsigned char material) const
{
    return m_vehicleMaterialManagers[vehicle]->isValidMaterial(material);
}

// ELF 0x3C1110.
float AISettings::getVehicleMaterialCost(const std::string& name, unsigned char material) const
{
    return m_vehicleMaterialManagers[getVehicle(name)]->getMaterialCost(material);
}

// ELF 0x3C10D0.
float AISettings::getVehicleMaterialCost(int vehicle, unsigned char material) const
{
    return m_vehicleMaterialManagers[vehicle]->getMaterialCost(material);
}

// ELF 0x3C1170.
MaterialManager* AISettings::getVehicleMaterialManager(int vehicle) const
{
    return m_vehicleMaterialManagers[vehicle];
}

// Returns the next bot name and rotates the roster for the following request.
// ELF 0x3C1480.
const std::string& AISettings::getBotName() const
{
    m_botNames.push_back(m_botNames.front());
    m_botNames.pop_front();

    return m_botNames.back().second;
}

// The shared bot value narrows, the settings value does not.
// ELF 0x3BEF50.
void AISettings::setMaxNumberOfBehaviours(int count)
{
    BotMain::setNBehaviours(static_cast<unsigned char>(count));
    m_numberOfBehaviours = count;
}

// ELF 0x3C0F10: grows the table to the vehicle (new slots null), then stores the set.
void AISettings::setVehicleMods(int vehicle, float* mods)
{
    if (static_cast<int>(m_vehicleMods.size()) <= vehicle)
        m_vehicleMods.resize(vehicle + 1, 0);
    m_vehicleMods[vehicle] = mods;
}

// ELF 0x3BF730.
void AISettings::addBehaviour(int vehicle, int index, BotBehaviour* behaviour)
{
    prepareBehaviourPool(vehicle, index);
    m_behaviours[vehicle][index] = behaviour;
}

// ELF 0x3BF7A0: the pool is prepared for (vehicle, index) although only the default slot is set.
void AISettings::setDefaultBehaviour(int vehicle, int index)
{
    prepareBehaviourPool(vehicle, index);
    m_defaultBehaviour[vehicle] = index;
}

// ELF 0x3BF870: BAPInterpreter::addEntry(entry) on the vehicle's interpreter.
void AISettings::addInterpreterEntry(int vehicle, BAPEntry* entry)
{
    preparePlanInterpreterPool(vehicle);
    static_cast<BAPInterpreter*>(m_planInterpreters[vehicle])->addEntry(entry);
}

// ELF 0x3BF8D0.
void AISettings::addInterpreterEntry(int vehicle, BotLodLevel lod, BAPEntry* entry)
{
    preparePlanInterpreterPool(vehicle);
    static_cast<BAPInterpreter*>(m_planInterpreters[vehicle])->addEntry(lod, entry);
}

// ELF 0x3BF930.
void AISettings::removeInterpreterEntry(int vehicle, BotLodLevel lod, BAPEntry* entry)
{
    preparePlanInterpreterPool(vehicle);
    static_cast<BAPInterpreter*>(m_planInterpreters[vehicle])->removeEntry(lod, entry);
}

// ELF 0x3BF990.
void AISettings::activateDefaultInterpreter(int vehicle, BAPStatementType type, BotLodLevel lod)
{
    preparePlanInterpreterPool(vehicle);
    static_cast<BAPInterpreter*>(m_planInterpreters[vehicle])->activateEntry(type, lod);
}

// ELF 0x3BF800.
void AISettings::addGenerator(int vehicle, int index, PlanGenerator* generator)
{
    preparePlanGeneratorPool(vehicle, index);
    m_planGenerators[vehicle][index] = generator;
}

// ELF 0x3BFA20: one behaviour list per vehicle up to `vehicle`; a list too short for `index` is
// sized to the number of behaviours (not to index + 1); the default table grows with -1 entries.
// Every size test is signed (`cmpl ...; jg`).
void AISettings::prepareBehaviourPool(int vehicle, int index)
{
    if (static_cast<int>(m_behaviours.size()) <= vehicle)
        m_behaviours.resize(vehicle + 1, std::vector<BotBehaviour*>());
    std::vector<BotBehaviour*>& behaviours = m_behaviours[vehicle];
    if (static_cast<int>(behaviours.size()) <= index)
        behaviours.resize(m_numberOfBehaviours, 0);
    prepareVehicleGroup(vehicle);
    if (static_cast<int>(m_defaultBehaviour.size()) <= vehicle)
        m_defaultBehaviour.resize(vehicle + 1, -1);
}

// ELF 0x3BFC50: as prepareBehaviourPool, with the vehicle group prepared before the inner list.
void AISettings::preparePlanGeneratorPool(int vehicle, int index)
{
    if (static_cast<int>(m_planGenerators.size()) <= vehicle)
        m_planGenerators.resize(vehicle + 1, std::vector<PlanGenerator*>());
    prepareVehicleGroup(vehicle);
    std::vector<PlanGenerator*>& generators = m_planGenerators[vehicle];
    if (static_cast<int>(generators.size()) <= index)
        generators.resize(m_numberOfBehaviours, 0);
}

// ELF 0x3BFDE0: one `new BAPInterpreter(45)` (operator new 0x10; 45 statement types) per missing
// vehicle, each checked (checkPointer, the Debug at line 46) and pushed back.
void AISettings::preparePlanInterpreterPool(int vehicle)
{
    if (static_cast<int>(m_planInterpreters.size()) <= vehicle)
    {
        for (unsigned int i = m_planInterpreters.size(); i < static_cast<unsigned int>(vehicle + 1); ++i)
            m_planInterpreters.push_back(checkPointer(new BAPInterpreter(45)));
    }
    prepareVehicleGroup(vehicle);
}

// ELF 0x3C0040: the vehicle-group table grows to the vehicle with group 0.
void AISettings::prepareVehicleGroup(int vehicle)
{
    if (vehicle >= static_cast<int>(m_vehicleGroups.size()))
        m_vehicleGroups.resize(vehicle + 1, 0);
}

// ELF 0x3BF9F0: the class static BotMain reads (BotMain::setBasicWeights).
void AISettings::setBasicBotWeights(float* weights)
{
    BotMain::setBasicWeights(weights);
}

// ELF 0x3C0240.
void AISettings::setStatsViewDistance(float distance)
{
    m_statsViewDistance = distance;
}

// ELF 0x3C0260.
void AISettings::setDrawBotStats(bool draw)
{
    m_drawBotStats = draw;
}

// ELF 0x3C0210.
void AISettings::setInformationGridDimension(int dimension)
{
    m_informationGridDimension = dimension;
}

// ELF 0x3C0280.
void AISettings::setNSides(int sides)
{
    m_nSides = sides;
}

// ELF 0x3C0A10.
void AISettings::setSAIUpdateFrequency(float frequency)
{
    m_saiUpdateFrequency = frequency;
}

// ELF 0x3C0A20.
void AISettings::setStandardPersonality(float* weights, int side)
{
    m_standardPersonality[side - 1] = weights;
}

// ELF 0x3C0A40.
void AISettings::setStandardMoralAdmin(BotMoralAdmin* admin, int side)
{
    m_standardMoralAdmin[side - 1] = admin;
}

// ELF 0x3C0AB0.
void AISettings::setRespawnAllowed(bool allowed)
{
    m_respawnAllowed = allowed;
}

// Duplicate insertion failure is deliberately ignored; the three
// parallel vectors still grow, while the name map retains the first id.
// ELF 0x3BEFC0.
void AISettings::setVehicle(const std::string& name)
{
    const int id = static_cast<int>(m_vehicleIndex.size());
    m_vehicleIndex.insert(std::make_pair(name, id));
    m_vehicleNames.push_back(name);
    m_vehicleMaterialManagers.push_back(checkPointer(new MaterialManager));
    m_clusterCostTypes.push_back(-1);
}

// Both map finds are intentionally unguarded.
// ELF 0x3BF5A0.
void AISettings::setVehicleClusterMapping(const std::string& vehicle,
                                          const std::string& clusterVehicle)
{
    const int vehicleId = m_vehicleIndex.find(vehicle)->second;
    const int clusterVehicleId = m_vehicleIndex.find(clusterVehicle)->second;
    m_clusterCostTypes[vehicleId] = clusterVehicleId;
}

// Retail uses resize rather than push_back and ignores duplicates.
// ELF 0x3C0640.
void AISettings::createVehicleGroup(const std::string& name)
{
    const int id = static_cast<int>(m_vehicleGroupNames.size());
    m_vehicleGroupNames.resize(m_vehicleGroupNames.size() + 1, name);
    m_vehicleGroupIndex.insert(std::make_pair(name, id));
}

// ELF 0x3C0790.
void AISettings::addVehicleToVehicleGroup(int vehicle, const std::string& group)
{
    // [PLATFORM] This single expression is load-bearing: PE captures the destination before the
    // virtual lookup; ELF reloads the vector after it. C++ operand evaluation order permits both.
    m_vehicleGroups[vehicle] = getVehicleGroup(group);
}

// ELF 0x3C07D0.
void AISettings::addVehicleToVehicleGroup(const std::string& vehicle,
                                          const std::string& group)
{
    const int vehicleId = getVehicle(vehicle);

    // Shipped defect in both images: the known-vehicle arm reports "unknown"; the negative id arm
    // performs the unchecked assignment. Preserve the inverted test and the unsafe missing-name arm.
    if (vehicleId >= 0)
    {
        Debug(DebugTypeError, __FILE__, __LINE__, "")
            << "Vehicle of name " << vehicle << " is unknown.";
        return;
    }

    m_vehicleGroups[vehicleId] = getVehicleGroup(group);
}

// Stores the auto-spawn flag. Enabling also requests immediate spawning when IAIMain
// exists; disabling performs no spawn action.
// ELF 0x3C0EA0.
void AISettings::setAutoSpawnBots(bool enable)
{
    m_autoSpawnBots = enable;

    if (enable && IAIMain::m_instance)
        IAIMain::m_instance->spawnBots();
}

// ELF 0x3C0C10. Disabling drawing clears render objects without a singleton guard.
void AISettings::setDraw(bool draw)
{
    m_draw = draw;
    if (!draw)
        AIRenderUtil::m_instance->unrenderAll();
}

// ELF 0x3C0C50: registers the name under the next index (insert_unique: a known name keeps its
// index).
void AISettings::setSensingAgentPatternName(const std::string& name)
{
    const int index = static_cast<int>(m_sensingAgentPatternIds.size());
    m_sensingAgentPatternIds.insert(std::pair<const std::string, int>(name, index));
}

// ELF 0x3C0D80.
void AISettings::setSensingAgentNullPattern(ISensingAgentPattern* pattern)
{
    m_sensingAgentNullPattern = pattern;
}

// ELF 0x3C0DA0: insert_unique(vehicle, pattern) into the +0x6C map.
void AISettings::addSensingAgentPattern(int vehicle, ISensingAgentPattern* pattern)
{
    m_sensingAgentPatterns.insert(std::pair<const int, ISensingAgentPattern*>(vehicle, pattern));
}

// ELF 0x3C0FF0.
void AISettings::setManDownEquipment(const std::string& equipment)
{
    m_manDownEquipment = equipment;
}

// ELF 0x3C1020.
void AISettings::setVehicleMaterialCost(const std::string& vehicle,
                                        unsigned char material,
                                        float cost)
{
    m_vehicleMaterialManagers[getVehicle(vehicle)]->setMaterialCost(material, cost);
}

// Adds a name to the bot-name roster used by getBotName.
// ELF 0x3C1190.
void AISettings::addBotName(const std::string& first, const std::string& last)
{
    const unsigned int count = static_cast<unsigned int>(m_botNames.size());

    float weight;

    if (count <= 5)
    {
        // The narrow range - see the header note. The zero terms are the binary's.
        weight = 0.0f + AIRandom::m_instance->nextFloat() * (0.40000001f - 0.0f);
    }
    else
    {
        weight = AIRandom::m_instance->nextFloat();
    }

    std::list<std::pair<float, std::string> >::iterator position = m_botNames.begin();

    // the dereference comes first. See the header note.
    while (weight > position->first && position != m_botNames.end())
        ++position;

    m_botNames.insert(position, std::pair<float, std::string>(weight, first + " " + last));
}

// ELF 0x3C1520.
void AISettings::setBotNameUnused(const std::string&)
{
    // (*) five bytes in the binary - a bare `ret`. dice declared it, the .ai console binds it, and
    // it does nothing at all. A recovered empty body, not a stub.
}


// ELF 0x3BD020 / 0x3BD5B0. Initializes AI settings and their default values.
AISettings::AISettings()
    : m_maxNBots(-1)
    , m_numberOfBehaviours(0)
    , m_viewDistance(-1.0f)
    , m_statsViewDistance(30.0f)
    , m_botSkill(0.75f)
    , m_saiUpdateFrequency(35.0f)
    , m_informationGridDimension(32)
    , m_nSides(0)
    , m_respawnAllowed(true)
    , m_draw(false)
    , m_autoSpawnBots(false)
    , m_maxBotsIncludeHumans(true)
    , m_ignoreCommanderDistance(30.0f)
    , m_overrideMenuSettings(false)
    , m_drawBotStats(true)
{
    // Defect 1: m_sensingAgentNullPattern is deliberately not initialised here. See above.

    // Both arrays are indexed [side - 1]; the constructor nulls all four words.
    m_standardMoralAdmin[0]  = 0;
    m_standardMoralAdmin[1]  = 0;
    m_standardPersonality[0] = 0;
    m_standardPersonality[1] = 0;

    // The one piece of AI tuning data this constructor gives up outright.
    m_armourClassValues[0] =  1.0f;
    m_armourClassValues[1] =  3.0f;
    m_armourClassValues[2] =  8.0f;
    m_armourClassValues[3] = 15.0f;
    m_armourClassValues[4] = 10.0f;
    m_armourClassValues[5] = 12.0f;

    // Defect 3: two no-ops, in this order, on two vectors that are empty. Reproduced because the
    // calls are in the binary.
    m_vehicleGroupNames.resize(m_vehicleGroupNames.size());
    m_vehicleGroups.resize(m_vehicleGroups.size());
}


// Registers the first settings/modifier instance using roster-static guards.
// Those guards can remain stale until updateStaticMethods refreshes the roster.
// ELF 0x3E7100 / 0x3E71A0.
IAISettings::IAISettings()
{
    if (!CommonSingletonServer::m_iaiSettings)
        classManager->registerSimpleSingleton("IAISettings", this);
}

// ELF 0x3E7240 / 0x3E72D0 / 0x3E7360.
IAISettings::~IAISettings()
{
    if (CommonSingletonServer::m_iaiSettings == this)
        classManager->unregisterSimpleSingleton("IAISettings");
}

// ELF 0x3E74B0 / 0x3E7550.
IAISettingsModifier::IAISettingsModifier()
{
    if (!CommonSingletonServer::m_iaiSettingsModifier)
        classManager->registerSimpleSingleton("IAISettingsModifier", this);
}

// ELF 0x3E75F0 / 0x3E7680 / 0x3E7710.
IAISettingsModifier::~IAISettingsModifier()
{
    if (CommonSingletonServer::m_iaiSettingsModifier == this)
        classManager->unregisterSimpleSingleton("IAISettingsModifier");
}

// Remaining recovered field accessors.

// ELF 0x3BEF20.
int AISettings::getMaxNBots() const
{
    return m_maxNBots;
}

// ELF 0x3BEF30.
bool AISettings::getmaxBotsIncludeHumans() const
{
    return m_maxBotsIncludeHumans;
}

// ELF 0x3BEF40.
float AISettings::getIgnoreCommanderDistance() const
{
    return m_ignoreCommanderDistance;
}

// ELF 0x3C00F0.
float AISettings::getViewDistance() const
{
    return m_viewDistance;
}

// ELF 0x3C0620.
float AISettings::getBotSkill() const
{
    return m_botSkill;
}

// ELF 0x3C0290.
int AISettings::getNSides() const
{
    return m_nSides;
}

// ELF 0x3C0AA0.
bool AISettings::isRespawnAllowed() const
{
    return m_respawnAllowed;
}

// ELF 0x3C0E90.
bool AISettings::getAutoSpawnBots() const
{
    return m_autoSpawnBots;
}

// ELF 0x3C0BD0.
const float* AISettings::getArmourClassValues() const
{
    return m_armourClassValues;
}

// ELF 0x3C0BE0.
float AISettings::getArmourClassValue(int index) const
{
    return m_armourClassValues[index];
}

// ELF 0x3BF700.
void AISettings::setMaxNBots(int n)
{
    m_maxNBots = n;
}

// ELF 0x3BF710.
void AISettings::maxBotsIncludeHumans(bool include)
{
    m_maxBotsIncludeHumans = include;
}

// ELF 0x3BF720.
void AISettings::setIgnoreCommanderDistance(float distance)
{
    m_ignoreCommanderDistance = distance;
}

// ELF 0x3C0630.
void AISettings::setBotSkill(float skill)
{
    m_botSkill = skill;
}

// Sets the AI view distance and propagates it to every live bot player.
// ELF 0x3C0100.
void AISettings::setViewDistance(float distance)
{
    m_viewDistance = distance;

    IBotManager* const botManager = IBotManager::instance;
    if (botManager)
    {
        for (unsigned int i = 0; i < static_cast<unsigned int>(botManager->getBotCount()); ++i)
        {
            Bot* const bot = botManager->getBot(i);

            if (!bot->isDeactivated())
                bot->getPlayer()->setViewDistance(m_viewDistance);
        }
    }

    AILODManager* const lod = AILODManager::m_instance;
    if (lod)
    {
        if (lod->m_basicLODDistance > distance)
        {
            lod->setBasicLODDistance(distance);
        }
        else if (distance > lod->m_basicLODDistance
              && lod->m_maxLODDistance > lod->m_basicLODDistance)
        {
            lod->setBasicLODDistance(lod->m_maxLODDistance > distance
                                         ? distance
                                         : lod->m_maxLODDistance);
        }
    }
}

// ELF 0x3C1550.
void AISettings::setOverrideMenuSettings(bool override_)
{
    m_overrideMenuSettings = override_;
}

// ELF 0x3C02A0. Resets only the shipped subset of settings and registries.
bool AISettings::reset()
{
    if (AIRadio::m_instance)
        AIRadio::m_instance->reset();
    // shipped defect: deleted entries stay in their vectors and can be deleted again.
    for (unsigned int row = 0; row < m_behaviours.size(); ++row)
        for (unsigned int i = 0; i < m_behaviours[row].size(); ++i)
            if (m_behaviours[row][i])
                delete m_behaviours[row][i];
    for (unsigned int row = 0; row < m_planGenerators.size(); ++row)
        for (unsigned int i = 0; i < m_planGenerators[row].size(); ++i)
            if (m_planGenerators[row][i])
                delete m_planGenerators[row][i];
    for (unsigned int i = 0; i < m_planInterpreters.size(); ++i)
        if (m_planInterpreters[i])
            delete m_planInterpreters[i];
    m_vehicleIndex.clear();
    m_vehicleNames.resize(0);
    m_sensingAgentPatternIds.clear();
    m_sensingAgentPatterns.clear();
    m_maxNBots = -1;
    // [UNRESOLVED] Types/names of the two words at +0x10/+0x14; both are zeroed here.
    std::memset(m_gap16, 0, sizeof(m_gap16));
    m_numberOfBehaviours = 0;
    m_informationGridDimension = 32;
    m_viewDistance = 600.0f;
    m_nSides = 0;
    m_standardPersonality[0] = 0;
    m_statsViewDistance = 512.0f;
    m_standardPersonality[1] = 0;
    m_autoSpawnBots = false;
    m_botSkill = 0.75f;
    m_respawnAllowed = true;
    m_draw = false;
    m_saiUpdateFrequency = 2.0f;
    m_standardMoralAdmin[0] = 0;
    m_standardMoralAdmin[1] = 0;
    return true;
}


// ELF 0x3BE130 (1772 B; the other variants are 0x3BDA40 and 0x3BE820). The shipped body first deletes
// AIRadio::m_instance (null-tested, nonvirtual D1 + operator delete). Nothing in libaidll.so
// constructs an AIRadio (see AIRadio.h), so the pointer is null and the step does nothing; it is
// kept because the instruction is there. Then every behaviour and every plan generator (per
// vehicle row) and every plan interpreter go through their virtual deleting destructors (+4),
// and every vehicle material manager through its nonvirtual one. The pointers are not cleared.
// The containers, the strings and both interface bases follow as members.
AISettings::~AISettings()
{
    delete AIRadio::m_instance;
    for (unsigned int row = 0; row < m_behaviours.size(); ++row)
        for (unsigned int i = 0; i < m_behaviours[row].size(); ++i)
            if (m_behaviours[row][i])
                delete m_behaviours[row][i];
    for (unsigned int row = 0; row < m_planGenerators.size(); ++row)
        for (unsigned int i = 0; i < m_planGenerators[row].size(); ++i)
            if (m_planGenerators[row][i])
                delete m_planGenerators[row][i];
    for (unsigned int i = 0; i < m_planInterpreters.size(); ++i)
        if (m_planInterpreters[i])
            delete m_planInterpreters[i];
    for (unsigned int i = 0; i < m_vehicleMaterialManagers.size(); ++i)
        if (m_vehicleMaterialManagers[i])
            delete m_vehicleMaterialManagers[i];
}

} // namespace ai
} // namespace hfe
} // namespace dice
