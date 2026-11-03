// The AI script vocabulary: stream operators for the AI enums. AI/Main/AIStreamOperators.cpp is an
// attested FILE symbol of libaidll.so (its static initializer is named after the TU's first function,
// operator>>(istream&, vector<unsigned>&) 0x3C4150); the text runs to 0x3C9170.

// static init: ELF 0x3C9170 (__tcf_0 0x3C91D0, _GLOBAL__I 0x3C9200): <iostream>'s Init object only.
#include <iostream>

#include "AIStreamOperators.h"
#include "../../../../Core/StringUtil.h"
#include "../../../../IO/Input/Input.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// The strategy vocabulary extractors (moved from AISettings.cpp; this TU is their image placement). Each table is the ELF's compare chain
// in code order; the chains compare the whole token with std::string::compare.
namespace {

struct AIEnumSpelling
{
    const char* name;
    int value;
};

// Answers the first matching spelling's value, or `fallback`; the caller decides whether to assign it.
int readAIEnumSpelling(std::istream& in, const AIEnumSpelling* table, int count, int fallback)
{
    std::string token;
    in >> token;
    for (int i = 0; i < count; ++i)
    {
        if (token.compare(table[i].name) == 0)
            return table[i].value;
    }
    return fallback;
}

} // anonymous namespace

static const AIEnumSpelling kTemperatureTreeNodeSpellings[] = {
    { "TTNTop", 0 },
    { "TTNDynamic", 1 },
    { "TTNStatic", 2 },
    { "TTNFriendly", 3 },
    { "TTNHostile", 4 },
    { "TTNTactical", 5 },
    { "TTNStrategic", 6 },
    { "TTNFriendGroundAndNaval", 7 },
    { "TTNHostileGroundAndNaval", 8 },
    { "TTNFriendArtillery", 9 },
    { "TTNHostileArtillery", 10 },
    { "TTNFriendGround", 11 },
    { "TTNFriendNaval", 12 },
    { "TTNFriendAir", 13 },
    { "TTNHostileGround", 14 },
    { "TTNHostileAir", 15 },
    { "TTNHostileNaval", 16 },
    { "TTNFriendGroundFixed", 17 },
    { "TTNFriendGroundMobile", 18 },
    { "TTNFriendNavalFixed", 19 },
    { "TTNFriendNavalMobile", 20 },
    { "TTNFriendAirFixed", 21 },
    { "TTNFriendAirMobile", 22 },
    { "TTNHostileGroundFixed", 23 },
    { "TTNHostileGroundMobile", 24 },
    { "TTNHostileNavalFixed", 25 },
    { "TTNHostileNavalMobile", 26 },
    { "TTNHostileAirFixed", 27 },
    { "TTNHostileAirMobile", 28 },
    { "TTNDefensivePos", 29 },
    { "TTNTopography", 30 },
    { "TTNEditors", 31 },
    { "TTNFixedWeapons", 32 },
    { "TTNAirfield", 33 },
    { "TTNProduction", 34 },
    { "TTNRadar", 35 },
    { "TTNTransportation", 36 },
    { "TTNControlPoint", 37 },
    { "TTNSupplies", 38 },
    { "TTNSuppliesRepair", 39 },
    { "TTNSuppliesAmmunition", 40 },
};

// ELF 0x3C5400
std::istream& operator>>(std::istream& in, TemperatureTreeNode& value)
{
    value = static_cast<TemperatureTreeNode>(readAIEnumSpelling(in, kTemperatureTreeNodeSpellings,
        sizeof(kTemperatureTreeNodeSpellings) / sizeof(kTemperatureTreeNodeSpellings[0]), -1));
    return in;
}

static const AIEnumSpelling kCombatStrengthTypeSpellings[] = {
    { "0", 0 }, { "CSTInfantry", 0 }, { "CSTInfantery", 0 }, { "Infantry", 0 }, { "Infantery", 0 },
    { "1", 1 }, { "CSTLightArmour", 1 }, { "LightArmour", 1 },
    { "2", 2 }, { "CSTHeavyArmour", 2 }, { "HeavyArmour", 2 },
    { "3", 3 }, { "CSTNavalArmour", 3 }, { "NavalArmour", 3 },
    { "4", 4 }, { "CSTHelicopter", 4 }, { "Helicopter", 4 },
    { "5", 5 }, { "CSTAirPlane", 5 }, { "AirPlane", 5 },
    { "CSTNumberOfTypes", 6 }, { "All", 6 },
};

// ELF 0x3C5A60. The final "All" comparison is made even though either outcome stores 6.
std::istream& operator>>(std::istream& in, CombatStrengthType& value)
{
    value = static_cast<CombatStrengthType>(readAIEnumSpelling(in, kCombatStrengthTypeSpellings,
        sizeof(kCombatStrengthTypeSpellings) / sizeof(kCombatStrengthTypeSpellings[0]), 6));
    return in;
}

static const AIEnumSpelling kLandingZoneDirectionSpellings[] = {
    { "LZXMin", 0 }, { "XMin", 0 }, { "0", 0 },
    { "LZZMin", 1 }, { "ZMin", 1 }, { "1", 1 },
    { "LZXMax", 2 }, { "XMax", 2 }, { "2", 2 },
    { "LZZMax", 3 }, { "ZMax", 3 }, { "3", 3 },
    { "LZUndefined", 4 }, { "Undefined", 4 }, { "-1", 4 }, { "4", 4 },
};

// ELF 0x3C8A20. The final "4" comparison is made even though either outcome stores 4.
std::istream& operator>>(std::istream& in, LandingZoneDirection& value)
{
    value = static_cast<LandingZoneDirection>(readAIEnumSpelling(in, kLandingZoneDirectionSpellings,
        sizeof(kLandingZoneDirectionSpellings) / sizeof(kLandingZoneDirectionSpellings[0]), 4));
    return in;
}

// ELF 0x3C5F00.
static const AIEnumSpelling kConditionStrengthSpellings[] = {
    { "Required", 0 },
    { "RequiredPositive", 1 },
    { "RequiredNegative", 2 },
    { "Advisory", 3 },
    { "AdvisoryPositive", 4 },
    { "AdvisoryNegative", 5 },
};

std::istream& operator>>(std::istream& in, StrategyCondition::ConditionStrength& value)
{
    value = static_cast<StrategyCondition::ConditionStrength>(readAIEnumSpelling(in, kConditionStrengthSpellings, sizeof(kConditionStrengthSpellings) / sizeof(kConditionStrengthSpellings[0]), 6));
    return in;
}

// ELF 0x3C60E0.
static const AIEnumSpelling kCompareTypeSpellings[] = {
    { "Crisp", 0 },
    { "Fuzzy", 1 },
    { "FuzzySqr", 2 },
};

std::istream& operator>>(std::istream& in, StrategyCondition::CompareType& value)
{
    value = static_cast<StrategyCondition::CompareType>(readAIEnumSpelling(in, kCompareTypeSpellings, sizeof(kCompareTypeSpellings) / sizeof(kCompareTypeSpellings[0]), 3));
    return in;
}

// ELF 0x3C6240.
static const AIEnumSpelling kConditionSideSpellings[] = {
    { "Friendly", 0 },
    { "Enemy", 1 },
    { "Both", 2 },
};

std::istream& operator>>(std::istream& in, StrategyCondition::ConditionSide& value)
{
    value = static_cast<StrategyCondition::ConditionSide>(readAIEnumSpelling(in, kConditionSideSpellings, sizeof(kConditionSideSpellings) / sizeof(kConditionSideSpellings[0]), 3));
    return in;
}

// ELF 0x3C68A0.
static const AIEnumSpelling kCompareSpellings[] = {
    { "Equal", 0 },
    { "=", 0 },
    { "EqualGreater", 1 },
    { ">=", 1 },
    { "EqualSmaller", 2 },
    { "<=", 2 },
    { "Quotient", 3 },
    { "/", 3 },
    { "QuotientGreater", 4 },
    { ">/", 4 },
    { "QuotientSmaller", 5 },
    { "</", 5 },
    { "Difference", 6 },
    { "-", 6 },
    { "DifferenceGreater", 7 },
    { ">-", 7 },
    { "DifferenceSmaller", 8 },
    { "<-", 8 },
};

std::istream& operator>>(std::istream& in, StrategyCondition::Compare& value)
{
    value = static_cast<StrategyCondition::Compare>(readAIEnumSpelling(in, kCompareSpellings, sizeof(kCompareSpellings) / sizeof(kCompareSpellings[0]), 9));
    return in;
}

// ELF 0x3C6D30.
static const AIEnumSpelling kConditionalSpellings[] = {
    { "Ticket", 13 },
    { "ControlPoint", 14 },
    { "Time", 15 },
    { "StartTime", 16 },
    { "Attacks", 17 },
    { "Defences", 18 },
    { "Carrier", 0 },
    { "HeavyNaval", 1 },
    { "Naval", 3 },
    { "Bomber", 4 },
    { "Fighter", 5 },
    { "AntiAir", 6 },
    { "Air", 7 },
    { "LCVP", 2 },
    { "Infantry", 8 },
    { "Infantery", 8 },
    { "Artillery", 9 },
    { "AntiTank", 10 },
    { "HeavyAttack", 11 },
    { "FastAttack", 12 },
    { "NumberOfFriendlyAreas", 19 },
    { "NumberOfNeutralAreas", 20 },
    { "NumberOfHostileAreas", 21 },
    { "Flank", 22 },
    { "Base", 23 },
    { "Close", 24 },
    { "Centre", 25 },
    { "Remote", 26 },
    { "North", 29 },
    { "East", 32 },
    { "South", 31 },
    { "West", 30 },
    { "Route", 27 },
    { "Bridge", 28 },
    { "FrontFlank", 33 },
    { "FrontBase", 34 },
    { "FrontClose", 35 },
    { "FrontCentre", 36 },
    { "FrontRemote", 37 },
    { "FrontNeutral", 38 },
    { "Front", 39 },
    { "Safe", 40 },
    { "EnemyObject", 41 },
    { "UnReachable", 42 },
    { "UnitConstant", 43 },
};

std::istream& operator>>(std::istream& in, StrategyCondition::Conditional& value)
{
    value = static_cast<StrategyCondition::Conditional>(readAIEnumSpelling(in, kConditionalSpellings, sizeof(kConditionalSpellings) / sizeof(kConditionalSpellings[0]), 44));
    return in;
}

// ELF 0x3C6330: a 27-entry jump table (cmpl $0x1a); 0, 21, 25 and values above 26 print nothing.
std::ostream& operator<<(std::ostream& out, AIStrategicObject::StrategicObjectType type)
{
    switch (type)
    {
    case AIStrategicObject::Flank:          out << "Flank"; break;
    case AIStrategicObject::Centre:         out << "Centre"; break;
    case AIStrategicObject::Base:           out << "Base"; break;
    case AIStrategicObject::Close:          out << "Close"; break;
    case AIStrategicObject::Remote:         out << "Remote"; break;
    case AIStrategicObject::AirSpawner:     out << "AirSpawner"; break;
    case AIStrategicObject::LandSpawner:    out << "LandSpawner"; break;
    case AIStrategicObject::NavalSpawner:   out << "NavalSpawner"; break;
    case AIStrategicObject::SoldierSpawner: out << "SoldierSpawner"; break;
    case AIStrategicObject::StrongPoint:    out << "StrongPoint"; break;
    case AIStrategicObject::ChokePoint:     out << "ChokePoint"; break;
    case AIStrategicObject::Bridge:         out << "Bridge"; break;
    case AIStrategicObject::AirField:       out << "AirField"; break;
    case AIStrategicObject::SupplyPoint:    out << "SupplyPoint"; break;
    case AIStrategicObject::Route:          out << "Route"; break;
    case AIStrategicObject::ControlPoint:   out << "ControlPoint"; break;
    case AIStrategicObject::North:          out << "North"; break;
    case AIStrategicObject::West:           out << "West"; break;
    case AIStrategicObject::South:          out << "South"; break;
    case AIStrategicObject::East:           out << "East"; break;
    case AIStrategicObject::Front:          out << "Front"; break;
    case AIStrategicObject::Safe:           out << "Safe"; break;
    case AIStrategicObject::Enemy:          out << "Enemy"; break;
    case AIStrategicObject::NeutralType:    out << "Neutral"; break;
    default: break;
    }
    return out;
}

// ELF 0x3C6460.
static const AIEnumSpelling kStrategicObjectTypeSpellings[] = {
    { "Flank", 1 },
    { "Centre", 2 },
    { "Base", 3 },
    { "Close", 4 },
    { "Remote", 5 },
    { "AirSpawner", 6 },
    { "LandSpawner", 7 },
    { "NavalSpawner", 8 },
    { "SoldierSpawner", 9 },
    { "StrongPoint", 10 },
    { "ChokePoint", 11 },
    { "Bridge", 12 },
    { "AirField", 13 },
    { "SupplyPoint", 14 },
    { "Route", 15 },
    { "ControlPoint", 16 },
    { "Front", 22 },
    { "Safe", 23 },
    { "Enemy", 24 },
    { "Neutral", 26 },
    { "North", 17 },
    { "West", 18 },
    { "South", 19 },
    { "East", 20 },
};

std::istream& operator>>(std::istream& in, AIStrategicObject::StrategicObjectType& value)
{
    value = static_cast<AIStrategicObject::StrategicObjectType>(readAIEnumSpelling(in, kStrategicObjectTypeSpellings, sizeof(kStrategicObjectTypeSpellings) / sizeof(kStrategicObjectTypeSpellings[0]), 27));
    return in;
}

// ELF 0x3C8CC0.
static const AIEnumSpelling kStatusSpellings[] = {
    { "Owned", 0 },
    { "Hostile", 1 },
    { "Neutral", 2 },
    { "Unknown", 3 },
};

std::istream& operator>>(std::istream& in, StatusMap::Status& value)
{
    const int read = readAIEnumSpelling(in, kStatusSpellings, sizeof(kStatusSpellings) / sizeof(kStatusSpellings[0]), -1);
    if (read != -1)
        value = static_cast<StatusMap::Status>(read);
    return in;
}

// ELF 0x3C78B0: the four spellings; any other value prints nothing.
std::ostream& operator<<(std::ostream& out, BotLodLevel lod)
{
    switch (lod)
    {
    case 0: out << "BLLFull"; break;
    case 1: out << "BLLMedium"; break;
    case 2: out << "BLLLight"; break;
    case 3: out << "BLLNone"; break;
    default: break;
    }
    return out;
}

// ELF 0x3C7930: "BLLFull"/"Full"/"0" 0, "BLLMedium"/"Medium"/"1" 1, "BLLLight"/"Light"/"2" 2,
// anything else 3 (BLLNone). The image still compares "BLLNone" and then "None", but both
// outcomes store 3 (the last compare's result is not tested).
std::istream& operator>>(std::istream& in, BotLodLevel& lod)
{
    std::string token;
    in >> token;
    if (token.compare("BLLFull") == 0 || token.compare("Full") == 0 || token.compare("0") == 0)
        lod = static_cast<BotLodLevel>(0);
    else if (token.compare("BLLMedium") == 0 || token.compare("Medium") == 0 || token.compare("1") == 0)
        lod = static_cast<BotLodLevel>(1);
    else if (token.compare("BLLLight") == 0 || token.compare("Light") == 0 || token.compare("2") == 0)
        lod = static_cast<BotLodLevel>(2);
    else
        lod = static_cast<BotLodLevel>(3);   // BotMain.h's enum has no name for 3 (BLLNone)
    return in;
}

// ELF 0x3C7AE0: a 59-entry jump table (cmpl $0x3a); 48 and values above 58 print nothing.
std::ostream& operator<<(std::ostream& out, BAPConditionType type)
{
    static const char* const kNames[59] = {
        "LookDeviation", "Precision", "Precision3d", "BombPrecision3d", "&&", "EndOfPath", "false",
        "Generic1Arg", "Generic2Arg", "InsideBattleZone", "Iteration", "LineOfFire",
        "PointDistance", "PointInFront", "!", "DistanceToProj", "StrategicObjectStatus",
        "ObjectBehind", "ObjectDistance", "ObjectDistanceToPoint", "ObjectEnemyOccupied",
        "ObjectExists", "ObjectHealth", "ObjectInFront", "ObjectInLandingZone",
        "ObjectInsideFrustum", "ObjectLineOfFire", "ObjectManDown", "ObjectMoved",
        "ObjectOccupied", "ObjectSpeed", "ObjectTouchingGround", "ObjectValidAiming",
        "ObjectValidPosition", "ObjectVisible", "||", "PointAt", "Position", "SharedCondition",
        "Time", "true", "WeaponMagAmmo", "WeaponTotalAmmo", "TotalWeaponAmmo",
        "WeaponReadyToFire", "WeaponTargetReachable", "WeaponHeat", "WeaponHasLockOnTarget",
        0, "IsTriggered", "Allow fire", "Sprinting", "Zoomed", "Detonator active",
        "Weapon active", "<", "Ok to throw grenade", "Splash damage", "Triggerable Active"
    };
    const unsigned int index = static_cast<unsigned int>(type);   // `ja` default: unsigned bound
    if (index < 59 && kNames[index])
        out << kNames[index];
    return out;
}

// ELF 0x3C7D90: a 45-entry jump table (cmpl $0x2c); 30 and values above 44 print nothing.
// The names are the BAPST* spellings without the prefix, except "if" and "while" (lower case).
std::ostream& operator<<(std::ostream& out, BAPStatementType type)
{
    static const char* const kNames[45] = {
        "Parallel", "Serial", "PlanEnd", "if", "Wait", "while", "AimAt", "MoveTo",
        "MoveToDirection", "MoveToObject", "MoveAwayFromObject", "Trigger", "TriggerContinously",
        "Idle", "ResetControls", "InfoWrapper", "AimingWrapper", "FinishedWrapper",
        "WeaponWrapper", "LookAtWrapper", "Sense", "OverrideSensingAgent", "PlaceC4", "LookAt",
        "Turn", "Roll3d", "SoldierPose", "UpdateVehicle", "ChangeWeapon", "ICChangeVehicle", 0,
        "ICTurnLock", "Evaluate", "NotifyBot", "PhysicsEnable", "SetSensingAgentFocus",
        "MergeSoldierInputs", "SendRadioMessage", "Debug", "ForgetTarget",
        "UseFlareOnMissileLock", "RunOverObject", "HonkHorn", "EvaluateCondition", "Revive"
    };
    const unsigned int index = static_cast<unsigned int>(type);
    if (index < 45 && kNames[index])
        out << kNames[index];
    return out;
}

// ELF 0x3C7FA0: the BAPST* spelling or the bare name, in the image's compare order (note "AImAt",
// "TurnLock" and "ChangeVehicle" for the IC statements); no match gives 45.
static const AIEnumSpelling kStatementTypeSpellings[] = {
    { "BAPSTParallel", 0 }, { "Parallel", 0 },
    { "BAPSTSerial", 1 }, { "Serial", 1 },
    { "BAPSTPlanEnd", 2 }, { "PlanEnd", 2 },
    { "BAPSTIf", 3 }, { "If", 3 },
    { "BAPSTWait", 4 }, { "Wait", 4 },
    { "BAPSTWhile", 5 }, { "While", 5 },
    { "BAPSTAimAt", 6 }, { "AImAt", 6 },
    { "BAPSTMoveTo", 7 }, { "MoveTo", 7 },
    { "BAPSTTurn", 24 }, { "Turn", 24 },
    { "BAPSTMoveToDirection", 8 }, { "MoveToDirection", 8 },
    { "BAPSTMoveToObject", 9 }, { "MoveToObject", 9 },
    { "BAPSTMoveAwayFromObject", 10 }, { "MoveAwayFromObject", 10 },
    { "BAPSTTrigger", 11 }, { "Trigger", 11 },
    { "BAPSTTriggerContinously", 12 }, { "TriggerContinously", 12 },
    { "BAPSTIdle", 13 }, { "Idle", 13 },
    { "BAPSTResetControls", 14 }, { "ResetControls", 14 },
    { "BAPSTInfoWrapper", 15 }, { "InfoWrapper", 15 },
    { "BAPSTSense", 20 }, { "Sense", 20 },
    { "BAPSTLookAt", 23 }, { "LookAt", 23 },
    { "BAPSTOverrideSensingAgent", 21 }, { "OverrideSensingAgent", 21 },
    { "BAPSTPlaceC4", 22 }, { "PlaceC4", 22 },
    { "BAPSTRoll3d", 25 }, { "Roll3d", 25 },
    { "BAPSTSoldierPose", 26 }, { "SoldierPose", 26 },
    { "BAPSTUpdateVehicle", 27 }, { "UpdateVehicle", 27 },
    { "BAPSTChangeWeapon", 28 }, { "ChangeWeapon", 28 },
    { "BAPSTICTurnLock", 31 }, { "TurnLock", 31 },
    { "BAPSTICChangeVehicle", 29 }, { "ChangeVehicle", 29 },
    { "BAPSTEvaluate", 32 }, { "Evaluate", 32 },
    { "BAPSTNotifyBot", 33 }, { "NotifyBot", 33 },
    { "BAPSTAimingWrapper", 16 }, { "AimingWrapper", 16 },
    { "BAPSTWeaponWrapper", 18 }, { "WeaponWrapper", 18 },
    { "BAPSTLookAtWrapper", 19 }, { "LookAtWrapper", 19 },
    { "BAPSTFinishedWrapper", 17 }, { "FinishedWrapper", 17 },
    { "BAPSTPhysicsEnable", 34 }, { "PhysicsEnable", 34 },
    { "BAPSTSetSensingAgentFocus", 35 }, { "SetSensingAgentFocus", 35 },
    { "BAPSTMergeSoldierInputs", 36 }, { "MergeSoldierInputs", 36 },
    { "BAPSTSendRadioMessage", 37 }, { "SendRadioMessage", 37 },
    { "BAPSTDebug", 38 }, { "Debug", 38 },
    { "BAPSTForgetTarget", 39 }, { "ForgetTarget", 39 },
    { "BAPSTUseFlareOnMissileLock", 40 }, { "UseFlareOnMissileLock", 40 },
    { "BAPSTRunOverObject", 41 }, { "RunOverObject", 41 },
    { "BAPSTHonkHorn", 42 }, { "HonkHorn", 42 },
    { "BAPSTEvaluateCondition", 43 }, { "EvaluateCondition", 43 },
    { "BAPSTRevive", 44 }, { "Revive", 44 },
};

std::istream& operator>>(std::istream& in, BAPStatementType& type)
{
    type = static_cast<BAPStatementType>(readAIEnumSpelling(in, kStatementTypeSpellings, sizeof(kStatementTypeSpellings) / sizeof(kStatementTypeSpellings[0]), 45));
    return in;
}

// ELF 0x3C4150
std::istream& operator>>(std::istream& in, std::vector<unsigned>& values)
{
    values.erase(values.begin(), values.end());
    unsigned value;
    for (;;)
    {
        in >> value;
        values.push_back(value);
        if (in.peek() == std::char_traits<char>::eof() || in.peek() == ' ')
            break;
        char separator;
        in >> separator;
    }
    return in;
}

// ELF 0x3C4220
std::ostream& operator>>(std::ostream& out, const std::vector<unsigned>& values)
{
    for (std::vector<unsigned>::const_iterator it = values.begin(); it != values.end(); ++it)
    {
        out << static_cast<unsigned long>(*it);
        // shipped defect: tests the current iterator, so even the final element gets a slash.
        if (it != values.end())
            out << "/";
    }
    return out;
}

// ELF 0x3C42A0
std::ostream& operator<<(std::ostream& out, const std::list<Vec2>& values)
{
    std::list<Vec2>::const_iterator it = values.begin();
    // shipped defect: the iterator is never incremented.
    while (it != values.end())
    {
        out << it->x << '/';
        out << it->y;
        if (it != values.end())
            out << ",";
    }
    return out;
}

// ELF 0x3C4350
std::istream& operator>>(std::istream& in, std::list<Vec2>& values)
{
    values.clear();
    float value;
    for (;;)
    {
        Vec2 point = { 0.0f, 0.0f };
        char separator;
        in >> value >> separator;
        point.x = value;
        in >> value;
        point.y = value;
        values.insert(values.end(), point);
        if (in.peek() == std::char_traits<char>::eof() || in.peek() == ' ')
            break;
        in >> separator;
    }
    return in;
}

static const char* const kInformationNames[] = {
    "ITProduction",
    "ITAmmunition",
    "ITRepair",
    "ITUnit",
    "ITAir",
    "ITHelicopter",
    "ITGround",
    "ITNaval",
    "ITControlpoint",
    "ITRadar",
    "ITAirfield",
    "ITTransportation",
    "ITArtillery",
    "ITFixed",
    "ITCover",
    "ITMobile",
    "ITUnmanned",
    "ITLowPriority",
    "ITStructure",
    "ITBiological",
    "ITVegetation",
    "ITSoldier",
    "ITObstructedView",
    "ITNoTemperature",
    "ITHasWreck",
    "ITObstacle",
    "ITHasCarHorn",
};
// ELF 0x3C4440
std::ostream& operator<<(std::ostream& out, InformationType value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 27)
        out << kInformationNames[index];
    return out;
}

// ELF 0x3C4590
std::istream& operator>>(std::istream& in, InformationType& value)
{
    static const AIEnumSpelling spellings[] = {
        { "ITProduction", 0 },
        { "ITAmmunition", 1 },
        { "ITRepair", 2 },
        { "ITUnit", 3 },
        { "ITAir", 4 },
        { "ITGround", 6 },
        { "ITNaval", 7 },
        { "ITControlpoint", 8 },
        { "ITRadar", 9 },
        { "ITAirfield", 10 },
        { "ITTransportation", 11 },
        { "ITArtillery", 12 },
        { "ITFixed", 13 },
        { "ITCover", 14 },
        { "ITMobile", 15 },
        { "ITHasWreck", 24 },
        { "ITObstacle", 25 },
        { "ITHasCarHorn", 26 },
        { "ITUnmanned", 16 },
        { "ITLowPriority", 17 },
        { "ITStructure", 18 },
        { "ITBiological", 19 },
        { "ITVegetation", 20 },
        { "ITSoldier", 21 },
        { "ITObstructedView", 22 },
        { "ITNoTemperature", 23 },
        { "ITHelicopter", 5 },
    };
    value = static_cast<InformationType>(readAIEnumSpelling(in, spellings, 27, -1));
    return in;
}

static const char* const kPlayerInputNames[] = {
    "PIYaw",
    "PIPitch",
    "PIRoll",
    "PIThrottle",
    "PIMouseLookX",
    "PIMouseLookY",
    "PICameraX",
    "PICameraY",
    "PIFire",
    "PIAction",
    "PIUse",
    "PIMouseLook",
    "PIAltSprint",
    "PISprint",
    "PIWeaponSelect1",
    "PIWeaponSelect2",
    "PIWeaponSelect3",
    "PIWeaponSelect4",
    "PIWeaponSelect5",
    "PIWeaponSelect6",
    "PIWeaponSelect7",
    "PIWeaponSelect8",
    "PIWeaponSelect9",
    "PIPositionSelect1",
    "PIPositionSelect2",
    "PIPositionSelect3",
    "PIPositionSelect4",
    "PIPositionSelect5",
    "PIPositionSelect6",
    "PIPositionSelect7",
    "PIPositionSelect8",
    "PIAltFire",
    "PIReload",
    nullptr,
    "PIDrop",
    "PIToggleCameraMode",
    "PIToggleCamera",
    "PILie",
    "PICrouch",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    "PIFlareFire",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    "PISayAll",
    "PISayTeam",
    "PINextItem",
    "PIPrevItem",
    "PICommunication",
    nullptr,
    "PINone",
};
// These are the AI namespace overloads, distinct from IO/Input's io namespace operators.
// ELF 0x3C4990
std::ostream& operator<<(std::ostream& out, io::PlayerInputMap value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 65 && kPlayerInputNames[index])
        out << kPlayerInputNames[index];
    return out;
}

// ELF 0x3C4BB0
std::istream& operator>>(std::istream& in, io::PlayerInputMap& value)
{
    static const AIEnumSpelling spellings[] = {
        { "PIYaw", 0 },
        { "PIPitch", 1 },
        { "PIRoll", 2 },
        { "PIThrottle", 3 },
        { "PIMouseLookX", 4 },
        { "PIMouseLookY", 5 },
        { "PICameraX", 6 },
        { "PICameraY", 7 },
        { "PIFire", 8 },
        { "PIAction", 9 },
        { "PIUse", 10 },
        { "PIMouseLook", 11 },
        { "PIAltSprint", 12 },
        { "PISprint", 13 },
        { "PISayAll", 58 },
        { "PISayTeam", 59 },
        { "PINextItem", 60 },
        { "PIPrevItem", 61 },
        { "PICommunication", 62 },
        { "PIWeaponSelect1", 14 },
        { "PIWeaponSelect2", 15 },
        { "PIWeaponSelect3", 16 },
        { "PIWeaponSelect4", 17 },
        { "PIWeaponSelect5", 18 },
        { "PIWeaponSelect6", 19 },
        { "PIWeaponSelect7", 20 },
        { "PIWeaponSelect8", 21 },
        { "PIWeaponSelect9", 22 },
        { "PIPositionSelect1", 23 },
        { "PIPositionSelect2", 24 },
        { "PIPositionSelect3", 25 },
        { "PIPositionSelect4", 26 },
        { "PIPositionSelect5", 27 },
        { "PIPositionSelect6", 28 },
        { "PIPositionSelect7", 29 },
        { "PIPositionSelect8", 30 },
        { "PIAltFire", 31 },
        { "PIReload", 32 },
        { "PIDrop", 34 },
        { "PIFlareFire", 47 },
        { "PIToggleCameraMode", 35 },
        { "PIToggleCamera", 36 },
        { "PILie", 37 },
        { "PICrouch", 38 },
        { "PINone", 64 },
    };
    value = static_cast<io::PlayerInputMap>(readAIEnumSpelling(in, spellings,
        sizeof(spellings) / sizeof(spellings[0]), -1));
    return in;
}

// ELF 0x3C5210
std::ostream& operator<<(std::ostream& out, TemperatureTreeNode value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 41)
        out << kTemperatureTreeNodeSpellings[index].name;
    return out;
}

static const char* const kCombatStrengthNames[] = {
    "CSTInfantry",
    "CSTLightArmour",
    "CSTHeavyArmour",
    "CSTNavalArmour",
    "CSTHelicopter",
    "CSTAirPlane",
    "CSTNumberOfTypes",
};
// ELF 0x3C59D0
std::ostream& operator<<(std::ostream& out, CombatStrengthType value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 7)
        out << kCombatStrengthNames[index];
    return out;
}

static const char* const kConditionTypeNames[] = {
    "Constant",
    "Homogeneous",
    "Heterogeneous",
};
// ELF 0x3C5D20
std::ostream& operator<<(std::ostream& out, StrategyCondition::ConditionType value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 3)
        out << kConditionTypeNames[index];
    return out;
}

// ELF 0x3C5D90
std::istream& operator>>(std::istream& in, StrategyCondition::ConditionType& value)
{
    static const AIEnumSpelling spellings[] = {
        { "Constant", 0 }, { "Homogeneous", 1 }, { "Heterogeneous", 2 }
    };
    value = static_cast<StrategyCondition::ConditionType>(readAIEnumSpelling(in, spellings, 3, 4));
    return in;
}

// ELF 0x3C5E80
std::ostream& operator<<(std::ostream& out, StrategyCondition::ConditionStrength value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 6)
        out << kConditionStrengthSpellings[index].name;
    return out;
}

// ELF 0x3C6070
std::ostream& operator<<(std::ostream& out, StrategyCondition::CompareType value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 3)
        out << kCompareTypeSpellings[index].name;
    return out;
}

// ELF 0x3C61D0
std::ostream& operator<<(std::ostream& out, StrategyCondition::ConditionSide value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 3)
        out << kConditionSideSpellings[index].name;
    return out;
}

// ELF 0x3C6800
std::ostream& operator<<(std::ostream& out, StrategyCondition::Compare value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 9)
        out << kCompareSpellings[index * 2].name;
    return out;
}

static const char* const kConditionalNames[] = {
    "Carrier",
    "BattleShip",
    "LCVP",
    "Naval",
    "Bomber",
    "Fighter",
    "AntiAir",
    "Air",
    "Infantery",
    "Artillery",
    "AntiTank",
    "HeavyAttack",
    "FastAttack",
    "Ticket",
    "ControlPoint",
    "Time",
    "StartTime",
    "Attacks",
    "Defences",
    "NumberOfFriendlyAreas",
    "NumberOfNeutralAreas",
    "NumberOfHostileAreas",
    "Flank",
    "Base",
    "Close",
    "Centre",
    "Remote",
    "Route",
    "Bridge",
    "North",
    "West",
    "South",
    "East",
    "FrontFlank",
    "FrontBase",
    "FrontClose",
    "FrontCentre",
    "FrontRemote",
    "FrontNeutral",
    "Front",
    "Safe",
    "EnemyObject",
    "UnReachable",
    "UnitConstant",
};
// ELF 0x3C6B20
std::ostream& operator<<(std::ostream& out, StrategyCondition::Conditional value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 44)
        out << kConditionalNames[index];
    return out;
}

static const char* const kKitNames[] = {
    "KTSpecops",
    "KTSniper",
    "KTAssault",
    "KTSupport",
    "KTEngineer",
    "KTMedic",
    "KTAT",
    "KTExtended1",
    "KTExtended2",
    "KTExtended3",
    "KTExtended4",
    "KTExtended5",
    "KTExtended6",
};
// ELF 0x3C7370
std::ostream& operator<<(std::ostream& out, KitType value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 13)
        out << kKitNames[index];
    return out;
}

// ELF 0x3C7430
std::istream& operator>>(std::istream& in, KitType& value)
{
    static const AIEnumSpelling spellings[] = {
        { "KTSpecops", 0 }, { "Specops", 0 }, { "0", 0 },
        { "KTSniper", 1 }, { "Sniper", 1 }, { "1", 1 },
        { "KTAssault", 2 }, { "Assault", 2 }, { "2", 2 },
        { "KTSupport", 3 }, { "Support", 3 }, { "3", 3 },
        { "KTEngineer", 4 }, { "Engineer", 4 }, { "4", 4 },
        { "KTMedic", 5 }, { "Medic", 5 }, { "5", 5 },
        { "KTAT", 6 }, { "AT", 6 }, { "6", 6 },
        { "KTExtended1", 7 }, { "Extended1", 7 }, { "7", 7 },
        { "KTExtended2", 8 }, { "Extended2", 8 }, { "8", 8 },
        { "KTExtended3", 9 }, { "Extended3", 9 }, { "9", 9 },
        { "KTExtended4", 10 }, { "Extended4", 10 }, { "10", 10 },
        { "KTExtended5", 11 }, { "Extended5", 11 }, { "11", 11 },
        { "KTExtended6", 12 }, { "Extended6", 12 }, { "12", 12 },
    };
    value = static_cast<KitType>(readAIEnumSpelling(in, spellings, 39, 13));
    return in;
}

static const char* const kLandingZoneNames[] = {
    "LZXMin",
    "LZZMin",
    "LZXMax",
    "LZZMax",
    "LZUndefined",
};
// ELF 0x3C89A0
std::ostream& operator<<(std::ostream& out, LandingZoneDirection value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 5)
        out << kLandingZoneNames[index];
    return out;
}

static const char* const kStatusNames[] = {
    "Owned",
    "Hostile",
    "Neutral",
};
// ELF 0x3C8C50
std::ostream& operator<<(std::ostream& out, StatusMap::Status value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 3)
        out << kStatusNames[index];
    else
        out << "Unknown";
    return out;
}

static const char* const kMeshNames[] = {
    "Bullet",
    "Vehicle",
    "Soldier",
};
// ELF 0x3C8DC0
std::ostream& operator<<(std::ostream& out, mesh::MeshType value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 3)
        out << kMeshNames[index];
    else
        out << "Error";
    return out;
}

// ELF 0x3C8E30
std::istream& operator>>(std::istream& in, mesh::MeshType& value)
{
    std::string token;
    in >> token;
    if (equalsIgnoreCase(token, std::string("Bullet")))
        value = static_cast<mesh::MeshType>(0);
    else if (equalsIgnoreCase(token, std::string("Vehicle")))
        value = static_cast<mesh::MeshType>(1);
    else if (equalsIgnoreCase(token, std::string("Soldier")))
        value = static_cast<mesh::MeshType>(2);
    else
    {
        equalsIgnoreCase(token, std::string("Error"));
        value = static_cast<mesh::MeshType>(4);
    }
    return in;
}

static const char* const kMTStatusNames[] = {
    "Forward",
    "Behind",
    "Right Forwards",
    "Left Forwards",
    "Avoid obstacle forwards left",
    "Avoid obstacle forwards right",
    "Avoid Obstacle Forward",
    "Ignore obstacle",
    "Back up",
    "Right Backwards",
    "Avoid Obstacle Backwards",
    "Left Backwards",
    "Back up until room",
    "Avoid obstacle backwards right",
    "Avoid obstacle backwards left",
};
// ELF 0x3C90A0
std::ostream& operator<<(std::ostream& out, MTStatus value)
{
    const unsigned index = static_cast<unsigned>(value);
    if (index < 15)
        out << kMTStatusNames[index];
    else
        out << "Error";
    return out;
}


} // namespace ai
} // namespace hfe
} // namespace dice
