// Stream operators for the AI script vocabulary (AI/Main/AIStreamOperators.cpp, attested FILE
// symbol; header path [INFERRED]). The console uses the extractors to parse enum arguments; the
// plan dumps (streamSelf) and BAPInterpreter's diagnostics use the inserters.

#ifndef AI_AIDLL_AI_MAIN_AISTREAMOPERATORS_H
#define AI_AIDLL_AI_MAIN_AISTREAMOPERATORS_H

#include <istream>
#include <ostream>
#include <list>
#include <vector>
#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/InformationType.h"

#include "../AIBotActionPlan/BAPTypes.h"
#include "../AIStrategic/StatusMap.h"
#include "../AIStrategic/StrategyCondition.h"
#include "../AIStrategicEntity/AIStrategicObject.h"

namespace dice {
namespace hfe {
enum KitType : int;
namespace ai {
namespace mesh { enum MeshType : int; }

enum TemperatureTreeNode : int;
enum CombatStrengthType : int;
enum LandingZoneDirection : int;
enum MTStatus : int;

std::istream& operator>>(std::istream& in, std::vector<unsigned>& values);
// The output overload is spelled >> in the shipped symbol and always writes a trailing slash.
std::ostream& operator>>(std::ostream& out, const std::vector<unsigned>& values);
// A nonempty list never advances its iterator in the shipped inserter.
std::ostream& operator<<(std::ostream& out, const std::list<Vec2>& values);
std::istream& operator>>(std::istream& in, std::list<Vec2>& values);
std::ostream& operator<<(std::ostream& out, TemperatureTreeNode value);
std::ostream& operator<<(std::ostream& out, CombatStrengthType value);
std::ostream& operator<<(std::ostream& out, LandingZoneDirection value);
std::ostream& operator<<(std::ostream& out, StrategyCondition::ConditionType value);
std::istream& operator>>(std::istream& in, StrategyCondition::ConditionType& value);
std::ostream& operator<<(std::ostream& out, StrategyCondition::ConditionStrength value);
std::ostream& operator<<(std::ostream& out, StrategyCondition::CompareType value);
std::ostream& operator<<(std::ostream& out, StrategyCondition::ConditionSide value);
std::ostream& operator<<(std::ostream& out, StrategyCondition::Compare value);
std::ostream& operator<<(std::ostream& out, StrategyCondition::Conditional value);
std::ostream& operator<<(std::ostream& out, KitType value);
std::istream& operator>>(std::istream& in, KitType& value);
std::ostream& operator<<(std::ostream& out, StatusMap::Status value);
std::ostream& operator<<(std::ostream& out, mesh::MeshType value);
std::istream& operator>>(std::istream& in, mesh::MeshType& value);
std::ostream& operator<<(std::ostream& out, MTStatus value);

// Each extractor reads one token and maps its spelling. Unknown tokens normally yield the value
// one past the last name; TemperatureTreeNode yields -1, Status retains its previous value,
// BotLodLevel yields 3, CombatStrengthType yields 6, and LandingZoneDirection yields 4.
std::istream& operator>>(std::istream& in, TemperatureTreeNode& value);
std::istream& operator>>(std::istream& in, CombatStrengthType& value);
std::istream& operator>>(std::istream& in, LandingZoneDirection& value);
std::istream& operator>>(std::istream& in, StrategyCondition::ConditionStrength& value);   // 0x3C5F00
std::istream& operator>>(std::istream& in, StrategyCondition::CompareType& value);         // 0x3C60E0
std::istream& operator>>(std::istream& in, StrategyCondition::ConditionSide& value);       // 0x3C6240
std::istream& operator>>(std::istream& in, StrategyCondition::Compare& value);             // 0x3C68A0
std::istream& operator>>(std::istream& in, StrategyCondition::Conditional& value);         // 0x3C6D30
std::ostream& operator<<(std::ostream& out, AIStrategicObject::StrategicObjectType type); // 0x3C6330
std::istream& operator>>(std::istream& in, AIStrategicObject::StrategicObjectType& value); // 0x3C6460
std::istream& operator>>(std::istream& in, StatusMap::Status& value);                      // 0x3C8CC0

std::ostream& operator<<(std::ostream& out, BotLodLevel lod);          // 0x3C78B0
std::istream& operator>>(std::istream& in, BotLodLevel& lod);          // 0x3C7930
std::ostream& operator<<(std::ostream& out, BAPConditionType type);    // 0x3C7AE0
std::ostream& operator<<(std::ostream& out, BAPStatementType type);    // 0x3C7D90
std::istream& operator>>(std::istream& in, BAPStatementType& type);    // 0x3C7FA0

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AISTREAMOPERATORS_H
