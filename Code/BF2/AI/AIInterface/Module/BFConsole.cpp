
#include "BFConsole.h"

#include <sstream>

#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../../../Core/Debug.h"
#include "../../../Core/Ids.h"
#include "../AIInstance.h"
#include "../SingletonServer.h"
#include "../Main/AIEventListener.h"
#include "../BFKnowledge/AITemplateManager.h"
#include "../BFKnowledge/BFCollisionHandler.h"
#include "../BFKnowledge/BFTerrain.h"
#include <cmath>        // aiMainInit's two log2 computations - see the block on it

// The sixteen adapters createSingletons builds and clearSingletons destroys. Chunk 01's second
// pass; every one carries its own vtable shape, its registry name and its roster static.
#include "../BFKnowledge/AIPlayerFactory.h"
#include "../BFKnowledge/AITemplate.h"              // the aiTemplate bindings' raw field stores
#include "../BFKnowledge/AITemplateArmament.h"      // and the aiTemplatePlugIn ones'
#include "../BFKnowledge/AITemplateControlInfo.h"
#include "../BFKnowledge/AITemplateCover.h"
#include "../BFKnowledge/AITemplateManager.h"
#include "../BFKnowledge/AITemplateMobile.h"
#include "../BFKnowledge/AITemplatePhysical.h"
#include "../BFKnowledge/AITemplatePlugIn.h"
#include "../BFKnowledge/AITemplatePlugInManager.h"
#include "../BFKnowledge/AITemplateUnit.h"
#include "../BFKnowledge/InformationType.h"         // aiTemplate.addType's argument
#include "../BFKnowledge/SoldierPose.h"             // weaponTemplate.firingPose's argument
#include "../BFKnowledge/WeaponTemplate.h"          // the weaponTemplate bindings' raw field stores
#include "../BFKnowledge/BFCollisionHandler.h"
#include "../BFKnowledge/BFCommander.h"
#include "../BFKnowledge/BFGame.h"
#include "../BFKnowledge/BFGameEffector.h"
#include "../BFKnowledge/BFGraphCreator.h"
#include "../BFKnowledge/BFObjectEnvironment.h"
#include "../BFKnowledge/BFPhysics.h"
#include "../BFKnowledge/BFRadioComm.h"
#include "../BFKnowledge/BFSquad.h"
#include "../BFKnowledge/BFTerrain.h"
#include "../BFKnowledge/BFTimer.h"
#include "../BFKnowledge/DebugRenderer.h"
#include "../BFKnowledge/KitTemplateManager.h"
#include "../BFKnowledge/KitTemplate.h"
#include "../BFKnowledge/WeaponTemplateManager.h"
#include "../../../Core/ConsoleObjectMacro.h"
#include "../../../Core/StringUtil.h"
#include "../../../Game/IGameLogic.h"                  // addAIRadioMessage: gameLogic
#include "../../../Game/IGame.h"
#include "../../../Game/Objects/IPlayer.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../../Game/GameLogic/ScoreSystem/ScoreManager.h"
#include "../../../Physics/BasicPhysicsSystem.h"
#include "../../../Game/GameLogic/GameLogic.h"         // getVoiceMessages
#include "../../../Game/GameLogic/VoiceMessages.h"     // namesToIds

#include <cstring>
#include <string>

namespace dice {
namespace hfe {

extern ClassManager* classManager;
namespace world { extern IBase* scoreManager; }

namespace ai {

enum CombatStrengthType : int;

// Reads a case-sensitive named enum token. Unknown tokens overwrite the result with
// 13 for KitType or 6 for CombatStrengthType; numeric inputs are not accepted.
// [INFERRED] Placement beside the console consumers.
std::istream& operator>>(std::istream& stream, KitType& type)
{
    std::string value;
    stream >> value;
    if (value == "KTSpecops" || value == "Specops" || value == "0") type = static_cast<KitType>(0);
    else if (value == "KTSniper" || value == "Sniper" || value == "1") type = static_cast<KitType>(1);
    else if (value == "KTAssault" || value == "Assault" || value == "2") type = static_cast<KitType>(2);
    else if (value == "KTSupport" || value == "Support" || value == "3") type = static_cast<KitType>(3);
    else if (value == "KTEngineer" || value == "Engineer" || value == "4") type = static_cast<KitType>(4);
    else if (value == "KTMedic" || value == "Medic" || value == "5") type = static_cast<KitType>(5);
    else if (value == "KTAT" || value == "AT" || value == "6") type = static_cast<KitType>(6);
    else if (value == "KTExtended1" || value == "Extended1" || value == "7") type = static_cast<KitType>(7);
    else if (value == "KTExtended2" || value == "Extended2" || value == "8") type = static_cast<KitType>(8);
    else if (value == "KTExtended3" || value == "Extended3" || value == "9") type = static_cast<KitType>(9);
    else if (value == "KTExtended4" || value == "Extended4" || value == "10") type = static_cast<KitType>(10);
    else if (value == "KTExtended5" || value == "Extended5" || value == "11") type = static_cast<KitType>(11);
    else if (value == "KTExtended6" || value == "Extended6" || value == "12") type = static_cast<KitType>(12);
    else type = static_cast<KitType>(13);
    return stream;
}

// The miss arm still evaluates the two final
// exact-name comparisons, but every miss has the same NumberOfTypes value.
std::istream& operator>>(std::istream& stream, CombatStrengthType& type)
{
    std::string value;
    stream >> value;
    if (value == "0" || value == "CSTInfantry" || value == "CSTInfantery"
        || compareIgnoreCase(value, std::string("Infantry")) == 0
        || compareIgnoreCase(value, std::string("Infantery")) == 0)
        type = static_cast<CombatStrengthType>(0);
    else if (value == "1" || value == "CSTLightArmour" || compareIgnoreCase(value, std::string("LightArmour")) == 0)
        type = static_cast<CombatStrengthType>(1);
    else if (value == "2" || value == "CSTHeavyArmour" || compareIgnoreCase(value, std::string("HeavyArmour")) == 0)
        type = static_cast<CombatStrengthType>(2);
    else if (value == "3" || value == "CSTNavalArmour" || compareIgnoreCase(value, std::string("NavalArmour")) == 0)
        type = static_cast<CombatStrengthType>(3);
    else if (value == "4" || value == "CSTHelicopter" || compareIgnoreCase(value, std::string("Helicopter")) == 0)
        type = static_cast<CombatStrengthType>(4);
    else if (value == "5" || value == "CSTAirPlane" || compareIgnoreCase(value, std::string("AirPlane")) == 0)
        type = static_cast<CombatStrengthType>(5);
    else if (value == "CSTNumberOfTypes" || value == "All")
        type = static_cast<CombatStrengthType>(6);
    else type = static_cast<CombatStrengthType>(6);
    return stream;
}

// Returns the active AI template, or null when its manager is absent.
AITemplate* getActiveAITemplate()
{
    return AITemplateManager::m_instance ? AITemplateManager::m_instance->getActiveTemplate() : 0;
}

// the weaponTemplate bindings' gate and target, the same shape.
WeaponTemplate* getActiveWeaponTemplate()
{
    return WeaponTemplateManager::m_instance ? WeaponTemplateManager::m_instance->getActiveTemplate() : 0;
}

// PE inlined into and the setter executors.
KitTemplate* getActiveKitTemplate()
{
    return KitTemplateManager::m_instance ? KitTemplateManager::m_instance->getActiveTemplate() : 0;
}

namespace {
// [CHOSEN] native list key. Retail pushes every generated binding of this TU onto one list
// this tree's recovered subset - aiTemplate, aiTemplatePlugIn, kitTemplate and aiComm so far
// shares this one.
const char* const kKitTemplateConsoleTag = "hfe.ai.kitTemplate";

// The generated enum conversions pass their static storage directly to the
// custom parser. Unlike numeric stream extraction
// these parsers overwrite it with a sentinel even when token extraction fails.
// Keep the shared macro's legacy numeric commit rule for the other arguments.
template<class T>
void convertKitConsoleArgument(std::istream& stream, T& storage)
{
    T value = storage;
    stream >> value;
    if (!stream.fail())
        storage = value;
}
void convertKitConsoleArgument(std::istream& stream, KitType& storage)
{
    stream >> storage;
}
void convertKitConsoleArgument(std::istream& stream, CombatStrengthType& storage)
{
    stream >> storage;
}
// ELF ConsoleClass101::setArgFromString : `ai::operator>>` straight into the static
// which always writes (-1 for an unknown name).
void convertKitConsoleArgument(std::istream& stream, InformationType& storage)
{
    stream >> storage;
}
// ELF ConsoleClass93::setArgFromString : `std::operator>>` straight into the guarded
// static string - GCC 3's extractor erases it first, so an empty token leaves it empty, not
// unchanged. Dead in practice (execute hands std::string arguments over raw), matched anyway.
void convertKitConsoleArgument(std::istream& stream, std::string& storage)
{
    stream >> storage;
}
// ELF ConsoleClass202..207/213..216::setArgFromString: `io::operator>>` straight into the static
// the name table's reader, which stores PINone for any unknown spelling.
void convertKitConsoleArgument(std::istream& stream, io::PlayerInputMap& storage)
{
    stream >> storage;
}
// ELF ConsoleClass209/210::setArgFromString: float, char, float, char, float read inline into the
// static - the same sequence Math/Vec3.cpp's reader packages.
void convertKitConsoleArgument(std::istream& stream, Vec3& storage)
{
    stream >> storage;
}
// ELF ConsoleClass241::setArgFromString: `ai::operator>>(SoldierPose&)` straight into the static
// which always writes (SPNotAvailable for an unknown name).
void convertKitConsoleArgument(std::istream& stream, SoldierPose& storage)
{
    stream >> storage;
}

// aiTemplate's targets (ELF ConsoleClass93..102 executors, )
// create/active/addPlugIn call the manager with no null test inside the executor (their gate is
// the manager itself); the other seven write the active template raw, fetched once per call.
std::string consoleAITemplateCreate(std::string name)
{
    return AITemplateManager::m_instance->create(name);
}
void consoleAITemplateSetActive(std::string name)
{
    AITemplateManager::m_instance->setActive(name);
}
void consoleAITemplateSetTypes(int value)            { getActiveAITemplate()->m_types = value; }
void consoleAITemplateSetDegeneration(float value)   { getActiveAITemplate()->m_degeneration = value; }
void consoleAITemplateSetAllowedTimeDiff(float value){ getActiveAITemplate()->m_allowedTimeDiff = value; }
void consoleAITemplateSetBasicTemp(float value)      { getActiveAITemplate()->m_basicTemp = value; }
void consoleAITemplateSetCommonKnowledge(bool value) { getActiveAITemplate()->m_commonKnowledge = value; }
void consoleAITemplateSetSecondary(bool value)       { getActiveAITemplate()->m_secondary = value; }
// the template is fetched before the sign test, and a negative type - the
// parser's "unknown name" - is skipped silently. No bound check above: a type >= 32 would shift out
// of range; the parser cannot produce one.
void consoleAITemplateAddType(InformationType type)
{
    AITemplate* const aiTemplate = getActiveAITemplate();
    if (type >= 0)
        aiTemplate->m_types |= 1 << type;
}
std::string consoleAITemplateAddPlugIn(std::string name)
{
    return AITemplateManager::m_instance->addPlugIn(name);
}

// getActiveTemplatePlugInIfMatch<T>::get(id) - , six 61-byte instances
// one per plug-in class (Cover 2, Mobile 3, Physical 4, Armament 0, Unit 5, ControlInfo 1): the
// active plug-in, if its virtual getId is the one asked for, else null - also null with no
// manager or no active plug-in. Every `aiTemplatePlugIn.<field>` binding gates on it and then asks
// it again for the object it writes, so each property line costs two lookups, as shipped. A class
// template with one static member, named and nested exactly as the mangled names spell it.
template<class T>
struct getActiveTemplatePlugInIfMatch
{
    static T* get(int id)
    {
        if (AITemplatePlugInManager::m_instance)
        {
            AITemplatePlugIn* const plugIn = AITemplatePlugInManager::m_instance->getActiveTemplatePlugIn();
            if (plugIn && plugIn->getId() == id)
                return static_cast<T*>(plugIn);
        }
        return 0;
    }
};

// aiTemplatePlugIn's targets (ELF ConsoleClass139..216 executors)
// The manager pair has no null test inside the executor (its gate is the manager); every field
// target re-fetches its plug-in through the matcher, unguarded - the gate already proved it.
std::string consolePlugInCreate(std::string type, std::string name)
{
    return AITemplatePlugInManager::m_instance->create(type, name);
}
void consolePlugInSetActive(std::string name)
{
    AITemplatePlugInManager::m_instance->setActive(name);
}

// Mobile (3): ConsoleClass148/152/155/156 call the class's own setters; the rest store raw.
void consolePlugInSetPathfindingMap(std::string name)
{
    getActiveTemplatePlugInIfMatch<AITemplateMobile>::get(3)->setPathfindingMap(name);
}
void consolePlugInSetCoverSearchRadius(float radius)
{
    getActiveTemplatePlugInIfMatch<AITemplateMobile>::get(3)->setCoverSearchRadius(radius);
}
void consolePlugInSetSoundSphereRadius(float first, float second)
{
    getActiveTemplatePlugInIfMatch<AITemplateMobile>::get(3)->setSoundSphereRadius(first, second);
}
void consolePlugInSetHearingProbability(float first, float second)
{
    getActiveTemplatePlugInIfMatch<AITemplateMobile>::get(3)->setHearingProbability(first, second);
}

// Physical (4): ConsoleClass159 is a zero-argument method returning the int, 160 its setter.
int consolePlugInGetStrType()
{
    return getActiveTemplatePlugInIfMatch<AITemplatePhysical>::get(4)->m_strType;
}
void consolePlugInSetStrType(CombatStrengthType type)
{
    getActiveTemplatePlugInIfMatch<AITemplatePhysical>::get(4)->m_strType = static_cast<int>(type);
}

// Armament (0): ConsoleClass164, an int store.
void consolePlugInSetIsAntiAircraft(int value)
{
    getActiveTemplatePlugInIfMatch<AITemplateArmament>::get(0)->m_isAntiAircraft = value;
}

// Unit (5). ConsoleClass167 stores the distance and its square; 169/170 index their arrays with no
// bound check; 168 goes through the class's IAISettings round trip.
void consolePlugInSetCullDistance(float distance)
{
    AITemplateUnit* const unit = getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5);
    unit->m_cullDistance = distance;
    unit->m_cullDistanceSquared = distance * distance;
}
void consolePlugInSetEquipmentTypeName(std::string name)
{
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5)->setEquipmentTypeName(name);
}
void consolePlugInSetBattleStrength(int index, int strength)
{
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5)->m_battleStrengths[index] = strength;
}
void consolePlugInSetStrategicStrength(int index, int strength)
{
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5)->m_strategicStrengths[index] = strength;
}
void consolePlugInSetUseNoPathfindingToGetToObject(bool value)
{
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5)->m_useNoPathfindingToGetToObject = value;
}
void consolePlugInSetHasExposedSoldier(bool value)
{
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5)->m_hasExposedSoldier = value;
}
void consolePlugInSetAlwaysTargetSoldier(bool value)
{
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5)->m_alwaysTargetSoldier = value;
}

// ControlInfo (1). ConsoleClass208 converts degrees on the way in; 209/210 call the clamping setters.
AITemplateControlInfo* activeControlInfo()
{
    return getActiveTemplatePlugInIfMatch<AITemplateControlInfo>::get(1);
}
void consolePlugInSetCameraRelativeDofRotationOffsetDeg(float degrees)
{
    activeControlInfo()->m_cameraRelativeDofRotationOffset = degrees * 0.017453292f;
}
void consolePlugInSetCameraRelativeMinRotationDeg(hfe::Vec3 degrees)
{
    activeControlInfo()->setCameraRelativeMinRotationDeg(degrees);
}
void consolePlugInSetCameraRelativeMaxRotationDeg(hfe::Vec3 degrees)
{
    activeControlInfo()->setCameraRelativeMaxRotationDeg(degrees);
}

// weaponTemplate's targets (ELF ConsoleClass223..245 executors)
std::string consoleWeaponTemplateCreate(std::string name)
{
    return WeaponTemplateManager::m_instance->create(name);
}
void consoleWeaponTemplateSetActive(std::string name)
{
    WeaponTemplateManager::m_instance->setActive(name);
}
// ConsoleClass226: the time and its reciprocal - 0, not infinity, for a zero time. The template is
// fetched once; the reciprocal is computed after the time is stored.
void consoleWeaponTemplateSetDeviationCorrectionTime(float seconds)
{
    WeaponTemplate* const weapon = getActiveWeaponTemplate();
    float rate = 0.0f;
    weapon->m_deviationCorrectionTime = seconds;
    if (seconds != 0.0f)
        rate = 1.0f / seconds;
    weapon->m_deviationCorrectionRate = rate;
}
void consoleWeaponTemplateSetIsThrown(bool thrown)
{
    getActiveWeaponTemplate()->setIsThrown(thrown);
}
// Preserves the registered getStrength command's empty result: it queries the active
// weapon template but reads no strength and supplies no return value.
void consoleWeaponTemplateGetStrength(CombatStrengthType)
{
    getActiveWeaponTemplate();
}
// ConsoleClass239: indexes the strengths array with no bound check.
void consoleWeaponTemplateSetStrength(CombatStrengthType type, int strength)
{
    getActiveWeaponTemplate()->m_strengths[type] = strength;
}

std::string consoleKitTemplateCreate(std::string name, KitType type)
{
    return KitTemplateManager::m_instance->create(name, type);
}
void consoleKitTemplateSetStrategicStrength(int index, int strength)
{
    getActiveKitTemplate()->setStrategicStrength(index, strength);
}
void consoleKitTemplateSetBattleStrength(CombatStrengthType type, int strength)
{
    getActiveKitTemplate()->setBattleStrength(static_cast<int>(type), strength);
}
} // anonymous namespace

// ELF ConsoleClass251/254/256: /;
// PE executors /. Defaults
// access 2, category 0. Only create is dynamic. The module's
// existing list registration owns all three; the other four kit commands remain
// unregistered pending their bindings. [RETIRE-WHEN: aidll-03b-templates]
#pragma push_macro("BF2_CC_CONVERT")
#undef BF2_CC_CONVERT
#define BF2_CC_CONVERT(T, oneBasedIndex)                                              \
    if (index == (oneBasedIndex))                                                    \
    {                                                                               \
        static T storage;                                                           \
        std::istringstream stream(value);                                           \
        convertKitConsoleArgument(stream, storage);                                 \
        m_args[(oneBasedIndex) - 1] = reinterpret_cast<std::string*>(&storage);       \
    }

// Console bindings for named AI templates.
BF2_CONSOLE_OBJECT("AITemplate", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(create, consoleAITemplateCreate, std::string,
    AITemplateManager::m_instance != 0, std::string, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(active, std::string, AITemplateManager::m_instance != 0,
    AITemplateManager::m_instance->getActive(), consoleAITemplateSetActive, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(types, int, getActiveAITemplate() != 0,
    getActiveAITemplate()->m_types, consoleAITemplateSetTypes, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(degeneration, float, getActiveAITemplate() != 0,
    getActiveAITemplate()->m_degeneration, consoleAITemplateSetDegeneration, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(allowedTimeDiff, float, getActiveAITemplate() != 0,
    getActiveAITemplate()->m_allowedTimeDiff, consoleAITemplateSetAllowedTimeDiff, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(basicTemp, float, getActiveAITemplate() != 0,
    getActiveAITemplate()->m_basicTemp, consoleAITemplateSetBasicTemp, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(commonKnowledge, bool, getActiveAITemplate() != 0,
    getActiveAITemplate()->m_commonKnowledge, consoleAITemplateSetCommonKnowledge, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(secondary, bool, getActiveAITemplate() != 0,
    getActiveAITemplate()->m_secondary, consoleAITemplateSetSecondary, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(addType, consoleAITemplateAddType, getActiveAITemplate() != 0,
    ai::InformationType, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(addPlugIn, consoleAITemplateAddPlugIn, std::string,
    AITemplateManager::m_instance != 0, std::string, kKitTemplateConsoleTag)

// Console properties require an active plug-in of the matching type.

// A raw plug-in field as a read/write property: the set arm stores through a fresh matcher lookup
// the get arm reads through another - the common shape of ConsoleClass143..212's executors.
#define BF2_AI_PLUGIN_FIELD(fnName, T, PlugIn, id, member)                                        \
    namespace { void BF2_CC_CAT(consolePlugInSet_, fnName)(T value)                              \
        { getActiveTemplatePlugInIfMatch<PlugIn>::get(id)->member = value; } }                   \
    BF2_CONSOLE_PROPERTY_FN(fnName, T, getActiveTemplatePlugInIfMatch<PlugIn>::get(id) != 0,     \
        getActiveTemplatePlugInIfMatch<PlugIn>::get(id)->member,                                 \
        BF2_CC_CAT(consolePlugInSet_, fnName), kKitTemplateConsoleTag)

// A ControlInfo input: the field, then its bit in the group mask (ConsoleClass202..207, 213..216).
// `1 << input` is a 32-bit INT, converted into the u64 mask - so it sign-extends (PIAltFire, 31
// sets the whole upper half) and inputs >= 32 alias 0..31 under x86's masked shift count.
#define BF2_AI_CONTROL_INPUT(fnName, member, mask)                                                \
    namespace { void BF2_CC_CAT(consolePlugInSet_, fnName)(io::PlayerInputMap input)             \
        { AITemplateControlInfo* const info = activeControlInfo();                               \
          info->member = input;                                                                  \
          const int bit = 1 << input;                                                            \
          info->mask |= static_cast<unsigned long long>(bit); } }                                \
    BF2_CONSOLE_PROPERTY_FN(fnName, io::PlayerInputMap,                                          \
        getActiveTemplatePlugInIfMatch<AITemplateControlInfo>::get(1) != 0,                      \
        activeControlInfo()->member, BF2_CC_CAT(consolePlugInSet_, fnName), kKitTemplateConsoleTag)

// A read-only property (m_type 2, argc exactly 0): ConsoleClass177..179. No family in
// Core/ConsoleObjectMacro.h has this shape yet, so it is spelled here, from the same head/tail.
#define BF2_AI_CONSOLE_READONLY_FN(fnName, T, typeName, activeExpr, getExpr, listTag)             \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 2, 0, 0, typeName, true)                                                   \
    BF2_CC_TAIL(0, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 0) return;                                                               \
        m_hasReturnValue = true;                                                                   \
        m_returnValue = (getExpr);                                                                 \
    }                                                                                              \
    virtual void setArgFromString(int, const std::string&) { }                                     \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    T m_returnValue;                                                                               \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

BF2_CONSOLE_OBJECT("AITemplatePlugIn", 2, 0)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(create, consolePlugInCreate, std::string,
    AITemplatePlugInManager::m_instance != 0, std::string, std::string, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(active, std::string, AITemplatePlugInManager::m_instance != 0,
    AITemplatePlugInManager::m_instance->getActive(), consolePlugInSetActive, kKitTemplateConsoleTag)
// Cover (2)
BF2_AI_PLUGIN_FIELD(coverValue, float, AITemplateCover, 2, m_coverValue)
BF2_AI_PLUGIN_FIELD(transparency, float, AITemplateCover, 2, m_transparency)
// Mobile (3)
BF2_AI_PLUGIN_FIELD(vehicleNumber, int, AITemplateMobile, 3, m_vehicleNumber)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setPathfindingMap, consolePlugInSetPathfindingMap,
    getActiveTemplatePlugInIfMatch<AITemplateMobile>::get(3) != 0, std::string, kKitTemplateConsoleTag)
BF2_AI_PLUGIN_FIELD(maxSpeed, float, AITemplateMobile, 3, m_maxSpeed)
BF2_AI_PLUGIN_FIELD(lodHeight, float, AITemplateMobile, 3, m_lodHeight)
BF2_AI_PLUGIN_FIELD(turnRadius, float, AITemplateMobile, 3, m_turnRadius)
BF2_CONSOLE_PROPERTY_FN(coverSearchRadius, float, getActiveTemplatePlugInIfMatch<AITemplateMobile>::get(3) != 0,
    getActiveTemplatePlugInIfMatch<AITemplateMobile>::get(3)->m_coverSearchRadius,
    consolePlugInSetCoverSearchRadius, kKitTemplateConsoleTag)
BF2_AI_PLUGIN_FIELD(avoidCollisionLookAhead, float, AITemplateMobile, 3, m_avoidCollisionLookAhead)
BF2_AI_PLUGIN_FIELD(isTurnable, bool, AITemplateMobile, 3, m_isTurnable)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setSoundSphereRadius, consolePlugInSetSoundSphereRadius,
    getActiveTemplatePlugInIfMatch<AITemplateMobile>::get(3) != 0, float, float, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setHearingProbability, consolePlugInSetHearingProbability,
    getActiveTemplatePlugInIfMatch<AITemplateMobile>::get(3) != 0, float, float, kKitTemplateConsoleTag)
// Physical (4)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getStrType, consolePlugInGetStrType, int,
    getActiveTemplatePlugInIfMatch<AITemplatePhysical>::get(4) != 0, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setStrType, consolePlugInSetStrType,
    getActiveTemplatePlugInIfMatch<AITemplatePhysical>::get(4) != 0, CombatStrengthType, kKitTemplateConsoleTag)
// Armament (0)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setIsAntiAircraft, consolePlugInSetIsAntiAircraft,
    getActiveTemplatePlugInIfMatch<AITemplateArmament>::get(0) != 0, int, kKitTemplateConsoleTag)
// Unit (5)
BF2_CONSOLE_PROPERTY_FN(cullDistance, float, getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5) != 0,
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5)->m_cullDistance,
    consolePlugInSetCullDistance, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(equipmentTypeName, std::string, getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5) != 0,
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5)->getEquipmentTypeName(),
    consolePlugInSetEquipmentTypeName, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setBattleStrength, consolePlugInSetBattleStrength,
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5) != 0, int, int, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setStrategicStrength, consolePlugInSetStrategicStrength,
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5) != 0, int, int, kKitTemplateConsoleTag)
BF2_AI_PLUGIN_FIELD(selectKey, io::PlayerInputMap, AITemplateUnit, 5, m_selectKey)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setUseNoPathfindingToGetToObject, consolePlugInSetUseNoPathfindingToGetToObject,
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5) != 0, bool, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setHasExposedSoldier, consolePlugInSetHasExposedSoldier,
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5) != 0, bool, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAlwaysTargetSoldier, consolePlugInSetAlwaysTargetSoldier,
    getActiveTemplatePlugInIfMatch<AITemplateUnit>::get(5) != 0, bool, kKitTemplateConsoleTag)
// ControlInfo (1)
BF2_AI_CONSOLE_READONLY_FN(driveControls, unsigned long long, "u64",
    getActiveTemplatePlugInIfMatch<AITemplateControlInfo>::get(1) != 0,
    activeControlInfo()->getDriveControls(), kKitTemplateConsoleTag)
BF2_AI_CONSOLE_READONLY_FN(aimControls, unsigned long long, "u64",
    getActiveTemplatePlugInIfMatch<AITemplateControlInfo>::get(1) != 0,
    activeControlInfo()->getAimControls(), kKitTemplateConsoleTag)
BF2_AI_CONSOLE_READONLY_FN(lookControls, unsigned long long, "u64",
    getActiveTemplatePlugInIfMatch<AITemplateControlInfo>::get(1) != 0,
    activeControlInfo()->getLookControls(), kKitTemplateConsoleTag)
BF2_AI_PLUGIN_FIELD(throttleSensitivity, float, AITemplateControlInfo, 1, m_throttleSensitivity)
BF2_AI_PLUGIN_FIELD(pitchSensitivity, float, AITemplateControlInfo, 1, m_pitchSensitivity)
BF2_AI_PLUGIN_FIELD(rollSensitivity, float, AITemplateControlInfo, 1, m_rollSensitivity)
BF2_AI_PLUGIN_FIELD(yawSensitivity, float, AITemplateControlInfo, 1, m_yawSensitivity)
BF2_AI_PLUGIN_FIELD(proportionalConstant, float, AITemplateControlInfo, 1, m_proportionalConstant)
BF2_AI_PLUGIN_FIELD(derivativeConstant, float, AITemplateControlInfo, 1, m_derivativeConstant)
BF2_AI_PLUGIN_FIELD(angleSpeedConstant, float, AITemplateControlInfo, 1, m_angleSpeedConstant)
BF2_AI_PLUGIN_FIELD(throttleLookAhead, float, AITemplateControlInfo, 1, m_throttleLookAhead)
BF2_AI_PLUGIN_FIELD(pitchLookAhead, float, AITemplateControlInfo, 1, m_pitchLookAhead)
BF2_AI_PLUGIN_FIELD(rollLookAhead, float, AITemplateControlInfo, 1, m_rollLookAhead)
BF2_AI_PLUGIN_FIELD(yawLookAhead, float, AITemplateControlInfo, 1, m_yawLookAhead)
BF2_AI_PLUGIN_FIELD(throttleScale, float, AITemplateControlInfo, 1, m_throttleScale)
BF2_AI_PLUGIN_FIELD(pitchScale, float, AITemplateControlInfo, 1, m_pitchScale)
BF2_AI_PLUGIN_FIELD(rollScale, float, AITemplateControlInfo, 1, m_rollScale)
BF2_AI_PLUGIN_FIELD(yawScale, float, AITemplateControlInfo, 1, m_yawScale)
BF2_AI_PLUGIN_FIELD(lookVerticalSensitivity, float, AITemplateControlInfo, 1, m_lookVerticalSensitivity)
BF2_AI_PLUGIN_FIELD(lookHorizontalSensitivity, float, AITemplateControlInfo, 1, m_lookHorizontalSensitivity)
BF2_AI_PLUGIN_FIELD(lookVerticalLookAhead, float, AITemplateControlInfo, 1, m_lookVerticalLookAhead)
BF2_AI_PLUGIN_FIELD(lookHorizontalLookAhead, float, AITemplateControlInfo, 1, m_lookHorizontalLookAhead)
BF2_AI_PLUGIN_FIELD(lookVerticalScale, float, AITemplateControlInfo, 1, m_lookVerticalScale)
BF2_AI_PLUGIN_FIELD(lookHorizontalScale, float, AITemplateControlInfo, 1, m_lookHorizontalScale)
BF2_AI_PLUGIN_FIELD(maxSpeedBumpinessFallOff, float, AITemplateControlInfo, 1, m_maxSpeedBumpinessFallOff)
BF2_AI_CONTROL_INPUT(driveThrottleControl, m_driveThrottleControl, m_driveControls)
BF2_AI_CONTROL_INPUT(driveTurnControl, m_driveTurnControl, m_driveControls)
BF2_AI_CONTROL_INPUT(aimVerticalControl, m_aimVerticalControl, m_aimControls)
BF2_AI_CONTROL_INPUT(aimHorizontalControl, m_aimHorizontalControl, m_aimControls)
BF2_AI_CONTROL_INPUT(lookVerticalControl, m_lookVerticalControl, m_lookControls)
BF2_AI_CONTROL_INPUT(lookHorizontalControl, m_lookHorizontalControl, m_lookControls)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setCameraRelativeDofRotationOffsetDeg, consolePlugInSetCameraRelativeDofRotationOffsetDeg,
    getActiveTemplatePlugInIfMatch<AITemplateControlInfo>::get(1) != 0, float, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setCameraRelativeMinRotationDeg, consolePlugInSetCameraRelativeMinRotationDeg,
    getActiveTemplatePlugInIfMatch<AITemplateControlInfo>::get(1) != 0, hfe::Vec3, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setCameraRelativeMaxRotationDeg, consolePlugInSetCameraRelativeMaxRotationDeg,
    getActiveTemplatePlugInIfMatch<AITemplateControlInfo>::get(1) != 0, hfe::Vec3, kKitTemplateConsoleTag)
BF2_AI_PLUGIN_FIELD(maxClimbAngle, float, AITemplateControlInfo, 1, m_maxClimbAngle)
BF2_AI_PLUGIN_FIELD(maxRollAngle, float, AITemplateControlInfo, 1, m_maxRollAngle)
BF2_AI_CONTROL_INPUT(driveRollControl, m_driveRollControl, m_driveControls)
BF2_AI_CONTROL_INPUT(drivePitchControl, m_drivePitchControl, m_driveControls)
BF2_AI_CONTROL_INPUT(aimThrottleControl, m_aimThrottleControl, m_aimControls)
BF2_AI_CONTROL_INPUT(aimRollControl, m_aimRollControl, m_aimControls)
#undef BF2_AI_CONSOLE_READONLY_FN
#undef BF2_AI_CONTROL_INPUT
#undef BF2_AI_PLUGIN_FIELD

// Registers weaponTemplate commands. Creation and selection require the manager; other
// commands require an active template. Preserve the published argument type spellings
// and the non-dynamic getStrength/setStrength registrations.

// A raw weapon field as a read/write property (ConsoleClass225..245's common shape).
#define BF2_AI_WEAPON_FIELD(fnName, T, member)                                                     \
    namespace { void BF2_CC_CAT(consoleWeaponSet_, fnName)(T value)                              \
        { getActiveWeaponTemplate()->member = value; } }                                         \
    BF2_CONSOLE_PROPERTY_FN(fnName, T, getActiveWeaponTemplate() != 0,                           \
        getActiveWeaponTemplate()->member, BF2_CC_CAT(consoleWeaponSet_, fnName), kKitTemplateConsoleTag)

// ConsoleClass238's shape: a one-argument method whose metadata says "int" but whose executor never
// produces a value (hasReturnValue false, formatter "") - a void body under an int declaration.
#define BF2_AI_CONSOLE_DEAD_GETTER_1(fnName, target, retTypeName, activeExpr, A0, listTag)         \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 1, 1, retTypeName, false)                                               \
            m_argTypes[0] = #A0;                                                                   \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 1) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0));                                                                 \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

BF2_CONSOLE_OBJECT("weaponTemplate", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(create, consoleWeaponTemplateCreate, std::string,
    WeaponTemplateManager::m_instance != 0, std::string, kKitTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(active, std::string, WeaponTemplateManager::m_instance != 0,
    WeaponTemplateManager::m_instance->getActive(), consoleWeaponTemplateSetActive, kKitTemplateConsoleTag)
BF2_AI_WEAPON_FIELD(deviation, float, m_deviation)
BF2_CONSOLE_PROPERTY_FN(deviationCorrectionTime, float, getActiveWeaponTemplate() != 0,
    getActiveWeaponTemplate()->m_deviationCorrectionTime, consoleWeaponTemplateSetDeviationCorrectionTime,
    kKitTemplateConsoleTag)
BF2_AI_WEAPON_FIELD(indirect, bool, m_indirect)
BF2_AI_WEAPON_FIELD(useAimerOnly, bool, m_useAimerOnly)
BF2_AI_WEAPON_FIELD(drag, float, m_drag)
BF2_AI_WEAPON_FIELD(minRange, float, m_minRange)
BF2_AI_WEAPON_FIELD(maxRange, float, m_maxRange)
BF2_AI_WEAPON_FIELD(weaponActivate, hfe::io::PlayerInputMap, m_weaponActivate)
BF2_AI_WEAPON_FIELD(weaponFire, hfe::io::PlayerInputMap, m_weaponFire)
BF2_CONSOLE_PROPERTY_FN(isThrown, bool, getActiveWeaponTemplate() != 0,
    getActiveWeaponTemplate()->getIsThrown(), consoleWeaponTemplateSetIsThrown, kKitTemplateConsoleTag)
BF2_AI_WEAPON_FIELD(soundSphereRadius, float, m_soundSphereRadius)
BF2_AI_WEAPON_FIELD(exitVelocity, float, m_exitVelocity)
BF2_AI_CONSOLE_DEAD_GETTER_1(getStrength, consoleWeaponTemplateGetStrength, "int",
    getActiveWeaponTemplate() != 0, CombatStrengthType, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setStrength, consoleWeaponTemplateSetStrength,
    getActiveWeaponTemplate() != 0, CombatStrengthType, int, kKitTemplateConsoleTag)
BF2_AI_WEAPON_FIELD(fireRate, int, m_fireRate)
BF2_AI_WEAPON_FIELD(firingPose, SoldierPose, m_firingPose)
BF2_AI_WEAPON_FIELD(optimalRangePercentage, float, m_optimalRangePercentage)
BF2_AI_WEAPON_FIELD(explosionRadius, float, m_explosionRadius)
BF2_AI_WEAPON_FIELD(allowedDeviation, float, m_allowedDeviation)
BF2_AI_WEAPON_FIELD(firesThroughTransparent, bool, m_firesThroughTransparent)
#undef BF2_AI_CONSOLE_DEAD_GETTER_1
#undef BF2_AI_WEAPON_FIELD

BF2_CONSOLE_OBJECT("kitTemplate", 2, 0)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(create, consoleKitTemplateCreate, std::string,
    KitTemplateManager::m_instance != 0, std::string, KitType, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setStrategicStrength, consoleKitTemplateSetStrategicStrength,
    getActiveKitTemplate() != 0, int, int, kKitTemplateConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setBattleStrength, consoleKitTemplateSetBattleStrength,
    getActiveKitTemplate() != 0, CombatStrengthType, int, kKitTemplateConsoleTag)
#pragma pop_macro("BF2_CC_CONVERT")

// aiComm (ConsoleClass262, bf2 0x887E470): access 2, category 0; active while the console exists.
// The executor copies the name and forwards the three floats to the console instance.
namespace {
void consoleAddAIRadioMessage(std::string name, float modifier, float maxSendPeriod, float receiveTimeToLive)
{
    BFConsole::m_instance->addAIRadioMessage(name, modifier, maxSendPeriod, receiveTimeToLive);
}
} // namespace
BF2_CONSOLE_OBJECT("aiComm", 2, 0)
BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(addAIRadioMessage, consoleAddAIRadioMessage, BFConsole::m_instance != 0,
    std::string, float, float, float, kKitTemplateConsoleTag)

ConsoleObjectList& kitTemplateConsoleObjects()
{
    return consoleBindingList(kKitTemplateConsoleTag);
}

// Zero-initialised; the constructor is the only thing that ever sets it and the
// destructor the only thing that ever clears it.
BFConsole* BFConsole::m_instance = 0;

// Initializes console state and registers the AI adapter types. Registration keeps the first singleton.
// ELF 0x887ED00 / 0x887F790.
BFConsole::BFConsole()
    : m_skipBulkClear(false)
    , m_singletonsCreated(false)
    , m_aiEventListener(0)
{
    // The first console instance remains the singleton.
    if (!m_instance)
        m_instance = this;

    m_radioMessageTypes.insert(std::make_pair(std::string("req_new_order"),           static_cast<AIRadioMessageType>(14)));
    m_radioMessageTypes.insert(std::make_pair(std::string("defend"),                  static_cast<AIRadioMessageType>(15)));
    m_radioMessageTypes.insert(std::make_pair(std::string("attack_position"),         static_cast<AIRadioMessageType>(16)));
    m_radioMessageTypes.insert(std::make_pair(std::string("move"),                    static_cast<AIRadioMessageType>(17)));
    m_radioMessageTypes.insert(std::make_pair(std::string("bail_out"),                static_cast<AIRadioMessageType>(0)));
    m_radioMessageTypes.insert(std::make_pair(std::string("get_in"),                  static_cast<AIRadioMessageType>(1)));
    m_radioMessageTypes.insert(std::make_pair(std::string("req_pickup"),              static_cast<AIRadioMessageType>(2)));
    m_radioMessageTypes.insert(std::make_pair(std::string("medic"),                   static_cast<AIRadioMessageType>(3)));
    m_radioMessageTypes.insert(std::make_pair(std::string("need_repair"),             static_cast<AIRadioMessageType>(4)));
    m_radioMessageTypes.insert(std::make_pair(std::string("roger_that"),              static_cast<AIRadioMessageType>(6)));
    m_radioMessageTypes.insert(std::make_pair(std::string("negative"),                static_cast<AIRadioMessageType>(7)));
    m_radioMessageTypes.insert(std::make_pair(std::string("PLAYER_CONFIRM_thankyou"), static_cast<AIRadioMessageType>(8)));
    m_radioMessageTypes.insert(std::make_pair(std::string("PLAYER_CONFIRM_sorry"),    static_cast<AIRadioMessageType>(9)));
    m_radioMessageTypes.insert(std::make_pair(std::string("need_ammo"),               static_cast<AIRadioMessageType>(10)));
    m_radioMessageTypes.insert(std::make_pair(std::string("PLAYER_TACTICS_gogogo"),   static_cast<AIRadioMessageType>(11)));
    m_radioMessageTypes.insert(std::make_pair(std::string("PLAYER_TACTICS_followme"), static_cast<AIRadioMessageType>(12)));
    m_radioMessageTypes.insert(std::make_pair(std::string("laserpaint"),              static_cast<AIRadioMessageType>(13)));
}

// Unregisters this console instance when it is the active singleton.
// ELF 0x8880190 / 0x88802C0.
BFConsole::~BFConsole()
{
    if (m_instance == this)
        m_instance = 0;
}

// Returns null in this build.
// ELF 0x8881B50.
void* BFConsole::getSinglePlayerInstance()
{
    return 0;
}

// Constructs the engine-side AI adapters and refreshes cached singleton interfaces.
// ELF 0x88803F0.
void BFConsole::createSingletons()
{
    if (!m_singletonsCreated)
    {
        BFTimer* bfTimer = new BFTimer;                                     // operator new(0x14)
        checkPointer(bfTimer);
        BFGameEffector* gameEffector = new BFGameEffector;                  // 0x10
        checkPointer(gameEffector);
        BFObjectEnvironment* objectEnvironment = new BFObjectEnvironment;   // 0x34
        checkPointer(objectEnvironment);
        BFPhysics* physics = new BFPhysics;                                 // 4
        checkPointer(physics);
        DebugRenderer* debugRenderer = new DebugRenderer;                   // 4
        checkPointer(debugRenderer);
        AIPlayerFactory* playerFactory = new AIPlayerFactory;               // 0x10
        checkPointer(playerFactory);
        BFCommander* commander = new BFCommander;                           // 4
        checkPointer(commander);
        BFSquad* squad = new BFSquad;                                       // 4
        checkPointer(squad);
        BFRadioComm* radioComm = new BFRadioComm;                           // 4
        checkPointer(radioComm);
        BFGame* game = new BFGame;                                          // 8
        checkPointer(game);
        AITemplateManager* templateManager = new AITemplateManager;         // 0x24
        checkPointer(templateManager);
        AITemplatePlugInManager* plugInManager = new AITemplatePlugInManager;   // 0x20
        checkPointer(plugInManager);
        // The literal 6 is the binary's, at this the only call site.
        WeaponTemplateManager* weaponTemplates = new WeaponTemplateManager(6);   // 0x24
        checkPointer(weaponTemplates);
        KitTemplateManager* kitTemplates = new KitTemplateManager;          // 0x2C
        checkPointer(kitTemplates);
        BFGraphCreator* graphCreator = new BFGraphCreator;                  // 4
        checkPointer(graphCreator);

        m_singletonsCreated = true;
    }

    if (!m_aiEventListener)
    {
        AIEventListener* listener = new AIEventListener;                    // 8
        checkPointer(listener);
        m_aiEventListener = listener;
        m_aiEventListener->init();
    }

    AIInstanceServer::updateInstance();
    BFSingletonServer::updateStaticMethods();
    RenderSingletonServer::updateStaticMethods();
}

// Destroys the engine-side AI adapters and refreshes cached interfaces.
// m_skipBulkClear suppresses one clearing pass.
// ELF 0x8881300.
void BFConsole::clearSingletons()
{
    if (m_skipBulkClear)
    {
        m_skipBulkClear = false;
    }
    else
    {
        // Ten deletes, in the binary's order, each null-guarded. `delete` through the interface
        // pointer is the ELF's vtable +0x04 - Itanium's deleting destructor - and it is the right
        // slot on either port precisely because it is typed rather than numbered.
        if (CommonSingletonServer::m_iaiGame)
            delete static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
        if (CommonSingletonServer::m_iaiGameEffector)
            delete static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector);
        if (CommonSingletonServer::m_iaiObjectEnvironment)
            delete static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
        if (CommonSingletonServer::m_iaiPhysics)
            delete static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics);
        // The engine debug renderer is owned through BFSingletonServer's pointer.
        if (BFSingletonServer::m_iDebugRenderer)
            delete static_cast<IDebugRenderer*>(BFSingletonServer::m_iDebugRenderer);
        if (CommonSingletonServer::m_iaiPlayerFactory)
            delete static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);
        if (CommonSingletonServer::m_iBFTimer)
            delete static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
        if (CommonSingletonServer::m_iBFCommander)
            delete static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander);
        if (CommonSingletonServer::m_iBFSquad)
            delete static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
        if (CommonSingletonServer::m_iBFGraphCreator)
            delete static_cast<IBFGraphCreator*>(CommonSingletonServer::m_iBFGraphCreator);

        m_singletonsCreated = false;
    }

    // always, latch or no latch - and neither of these two was built by createSingletons.
    if (CommonSingletonServer::m_iaiTerrain)
        delete static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    if (CommonSingletonServer::m_iBFCollisionHandler)
        delete static_cast<IBFCollisionHandler*>(CommonSingletonServer::m_iBFCollisionHandler);

    // (!) vtable +0x08 here, not +0x04 - AIEventListener's vtable starts with handleEvent, so its
    // deleting destructor is the third slot. Typed `delete` lands on it without a slot number.
    if (m_aiEventListener)
        delete m_aiEventListener;
    m_aiEventListener = 0;

    BFSingletonServer::updateStaticMethods();
    RenderSingletonServer::updateStaticMethods();
    AIInstanceServer::updateInstance();
}

// Initializes the AI adapters needed for a game.
// ELF 0x8881430.
void BFConsole::aiMainInit()
{
    // the two world adapters - see the block above.
    if (!CommonSingletonServer::m_iaiTerrain)
        checkPointer(new BFTerrain());

    if (!CommonSingletonServer::m_iBFCollisionHandler)
        checkPointer(new BFCollisionHandler());

    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    const float worldSizeX = static_cast<float>(terrain->getWorldSizeX());
    const float worldSizeZ = static_cast<float>(terrain->getWorldSizeZ());

    // computed and discarded - see the block above. Kept because the source keeps them.
    const double log2SizeX = std::log(static_cast<double>(worldSizeX)) / std::log(2.0);
    const double log2SizeZ = std::log(static_cast<double>(worldSizeZ)) / std::log(2.0);
    (void)log2SizeX;
    (void)log2SizeZ;

    AIInstanceServer::updateInstance();

    // No null guard. See the block above.
    AITemplateManager::m_instance->createWreckTemplates();
}

// bf2 0x88816E0 (1135 B; recovered on the client track, docs/macos-c-singleplayer.md). The
// collision handler drops every tracked projectile (+0x08); the three template managers are
// deleted (nonvirtual destructors) and the kit manager through its deleting destructor (+0x10);
// all four are built again (their constructors install the singletons), each allocation checked
// like createSingletons (the Debug arms log and carry on); finally the AI game is told it is
// precaching (IAIGame +0x64, true).
// ELF 0x88816E0.
void BFConsole::reset()
{
    if (CommonSingletonServer::m_iBFCollisionHandler)
        static_cast<IBFCollisionHandler*>(CommonSingletonServer::m_iBFCollisionHandler)->removeAllProjectiles();
    if (AITemplateManager::m_instance)
        delete AITemplateManager::m_instance;
    if (AITemplatePlugInManager::m_instance)
        delete AITemplatePlugInManager::m_instance;
    if (WeaponTemplateManager::m_instance)
        delete WeaponTemplateManager::m_instance;
    if (KitTemplateManager::m_instance)
        delete static_cast<IKitTemplateManager*>(KitTemplateManager::m_instance);

    AITemplateManager* templateManager = new AITemplateManager;             // 0x24
    checkPointer(templateManager);
    AITemplatePlugInManager* plugInManager = new AITemplatePlugInManager;   // 0x20
    checkPointer(plugInManager);
    WeaponTemplateManager* weaponTemplates = new WeaponTemplateManager(6);  // 0x24
    checkPointer(weaponTemplates);
    KitTemplateManager* kitTemplates = new KitTemplateManager;              // 0x2C
    checkPointer(kitTemplates);

    if (CommonSingletonServer::m_iaiGame)
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->setIsPrecaching(true);
}

// bf2 0x8881BE0 (Hex-Rays; called only by the aiComm.addAIRadioMessage executor, ConsoleClass262).
// The voice message id comes from GameLogic's VoiceMessages (no voice, no filter); the radio
// message type from the name table the constructor filled, read without an end() test (an
// unknown name reads the header node, as shipped). Both maps keep an existing entry
// (insert_unique): settings[(unsigned char)id] = {type, modifier, max send period, receive time to
// live}, voices[type] = (unsigned char)id. AIRadioMessages.ai calls it once per message.
// ELF 0x8881B60.
void BFConsole::addAIRadioMessage(const std::string& name, float modifier, float maxSendPeriod,
                                  float receiveTimeToLive)
{
    int messageId;
    const std::string filter;
    const std::string voice;
    static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(&messageId, 0, 0, name, voice, filter);
    const AIRadioMessageType type = m_radioMessageTypes.find(name)->second;
    AIRadioMessageSettings settings;
    const int typeWord = static_cast<int>(type);
    std::memcpy(settings.m_opaque0, &typeWord, sizeof(typeWord));
    settings.m_modifier = modifier;
    settings.m_maxSendPeriod = maxSendPeriod;
    settings.m_receiveTimeToLive = receiveTimeToLive;
    m_messageSettings.insert(std::make_pair(static_cast<unsigned char>(messageId), settings));
    m_messageVoices.insert(std::make_pair(type, static_cast<unsigned char>(messageId)));
}

// Resolves radio-message identifiers through the module courier.
// ELF 0x8881D90.
bool BFConsole::getAIMessageSettings(int voiceId, AIRadioMessageSettings& settings)
{
    // The key is the low byte - see the block above.
    const unsigned char key = static_cast<unsigned char>(voiceId);

    std::map<unsigned char, AIRadioMessageSettings>::const_iterator it = m_messageSettings.find(key);
    if (it == m_messageSettings.end())
        return false;

    settings = it->second;
    return true;
}

// ELF 0x8881E00.
bool BFConsole::getVoiceFromAiMessageType(AIRadioMessageType type, int& voice)
{
    std::map<AIRadioMessageType, unsigned char>::const_iterator it = m_messageVoices.find(type);
    if (it == m_messageVoices.end())
        return false;

    voice = it->second;
    return true;
}

// ELF 0x8881E50. ConsoleClass268's returned-string assignment also attests the result type.
std::string BFConsole::code(const std::string& command)
{
    std::stringstream result;
    world::IScoreManager* const scores = static_cast<world::IScoreManager*>(world::scoreManager);
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    Debug(DebugTypeError, "AI/AIInterface/Module/BFConsole.cpp", 322, "") << "Does not work";
    if (!player)
    {
        result << "Error, could not find local player.";
        return result.str();
    }

    const unsigned int team = static_cast<unsigned int>(player->getTeam());
    const int tickets = scores->getTeamScore(team)->m_tickets;
    float fraction = 0.0f;
    bool reduceTickets = true;
    if (equalsIgnoreCase(std::string(command.c_str()), std::string("Tobias.Karlsson")))
    {
        if (game)
            game->queryInterface(IID_IGameServer);
        else
            result << "Error, no game server active.";
        // shipped defect: the failed server lookup does not prevent the ticket change.
        fraction = 0.25f;
    }
    else if (equalsIgnoreCase(std::string(command.c_str()), std::string("Jonathan.Gustavsson")))
    {
        if (gameLogic->getGameStatus() == 1)
            result << "Error, no active BotManager found.";
        else
            result << "Error, game must be running for this command.";
        return result.str();
    }
    else if (equalsIgnoreCase(std::string(command.c_str()), std::string("Thomas.Skoldenborg")) ||
             equalsIgnoreCase(std::string(command.c_str()), std::string("Thomas.Sk\xF6ldenborg")))
    {
        fraction = 0.33f;
    }
    else if (equalsIgnoreCase(std::string(command.c_str()), std::string("TheAllSeeingEyeOfTheAIProgrammer")))
    {
        fraction = 0.5f;
    }
    else if (equalsIgnoreCase(std::string(command.c_str()), std::string("BotsCanCheatToo")))
    {
        result << "The bots are CHEATING now, be aware.";
        fraction = 0.75f;
    }
    else if (equalsIgnoreCase(std::string(command.c_str()), std::string("ChallengeMyComputer")))
    {
        result << "There goes the FRAME RATE.";
        fraction = 0.75f;
    }
    else if (equalsIgnoreCase(std::string(command.c_str()), std::string("EarthWalk")))
    {
        static_cast<world::IPhysicsSystem*>(world::basicPhysicsSystem)->setGravity(-10.0f);
        result << "GRAVITY set to EARTH. Home sweet home.";
        reduceTickets = false;
    }
    else if (equalsIgnoreCase(std::string(command.c_str()), std::string("MoonWalk")))
    {
        static_cast<world::IPhysicsSystem*>(world::basicPhysicsSystem)->setGravity(-1.67f);
        result << "GRAVITY set to MOON. Let's dance!";
        fraction = 0.75f;
    }
    else if (equalsIgnoreCase(std::string(command.c_str()), std::string("SpaceWalk")))
    {
        static_cast<world::IPhysicsSystem*>(world::basicPhysicsSystem)->setGravity(-0.1f);
        result << "GRAVITY set to SPACE. Better hold on tight!";
        fraction = 0.5f;
    }
    else if (equalsIgnoreCase(std::string(command.c_str()), std::string("FUBAR")))
    {
        result << "QWERTY!";
        reduceTickets = false;
    }
    else
    {
        result << "Trying to cheat? \nShame on you! \n Feel my Wrath.";
        const int remaining = tickets < 2 ? tickets : 1;
        scores->getTeamScore(team)->setTickets(remaining);
        reduceTickets = false;
    }

    if (reduceTickets)
    {
        world::TeamScore* const score = scores->getTeamScore(team);
        const float remaining = static_cast<float>(tickets) * fraction;
        score->setTickets(static_cast<int>(remaining));
    }
    // shipped defect: a busy loop only polls tickets and increments a local counter.
    for (int i = 0; i < tickets - scores->getTeamScore(team)->m_tickets; ++i)
    {
    }
    return result.str();
}

// Registers the module courier interface for access through the singleton registry.
IBFModuleCourier::IBFModuleCourier()
{
    if (!CommonSingletonServer::m_iBFModuleCourier)
        classManager->registerSimpleSingleton(std::string("IBFModuleCourier"), this);
}

IBFModuleCourier::~IBFModuleCourier()
{
    if (CommonSingletonServer::m_iBFModuleCourier == this)
        classManager->unregisterSimpleSingleton(std::string("IBFModuleCourier"));
}

// Adds nothing; the six forwards are 17 or 40
// bytes each, which is a vtable load and a jump.
BFModuleCourier::BFModuleCourier()
{
}

BFModuleCourier::~BFModuleCourier()
{
}

void BFModuleCourier::initSingletons()
{
    BFConsole::m_instance->createSingletons();
}

void BFModuleCourier::clearSingletons()
{
    BFConsole::m_instance->clearSingletons();
}

void BFModuleCourier::aiMainInit()
{
    BFConsole::m_instance->aiMainInit();
}

void BFModuleCourier::reset()
{
    BFConsole::m_instance->reset();
}

bool BFModuleCourier::getAIMessageSettings(int message, AIRadioMessageSettings& settings)
{
    return BFConsole::m_instance->getAIMessageSettings(message, settings);
}

bool BFModuleCourier::getVoiceFromAiMessageType(AIRadioMessageType type, int& voice)
{
    return BFConsole::m_instance->getVoiceFromAiMessageType(type, voice);
}

// The courier really is one pointer in both images - 33-byte constructor, no members. Unlike
// BFConsole's size, this one carries no STL and is therefore identical across toolchains, so the
// assertion is still meaningful and is kept.
typedef char BFModuleCourier_is_one_pointer[
    (sizeof(BFModuleCourier) == sizeof(void*)) ? 1 : -1];

} // namespace ai
} // namespace hfe
} // namespace dice
