// One condition of a strategy prerequisite: compares a value of the 44-entry strategic state
// arrays (SAI::updateStates) with a target and answers a score in [-1, 1] (crisp) or a distance
// (fuzzy). Subclasses choose the operands: SCConstant, SCHomogeneous, SCHeterogeneous, SCArea.
// Evidence: libaidll.so (ia-32) 0x332790..0x333090 and 0x334150..0x334570; vtable 0x4BF470
// {D1, D0, getType, compare = __cxa_pure_virtual}. Enumerator names are the strings the
// shipped stream operators print (0x3C5D20..0x3C6D30); member names are [INFERRED].

#ifndef AI_AIDLL_AI_AISTRATEGIC_STRATEGYCONDITION_H
#define AI_AIDLL_AI_AISTRATEGIC_STRATEGYCONDITION_H

#include <string>

namespace dice {
namespace hfe {
namespace ai {

class StrategyCondition
{
public:
    // getType: SCConstant 0, SCHomogeneous 1, SCHeterogeneous 2, SCArea 3, the base 4. The
    // printer names only 0..2.
    // Fixed int representation also admits the parser's unnamed fallback value 4.
    enum ConditionType : int
    {
        Constant      = 0,
        Homogeneous   = 1,
        Heterogeneous = 2
    };

    enum ConditionStrength
    {
        Required         = 0,
        RequiredPositive = 1,
        RequiredNegative = 2,
        Advisory         = 3,
        AdvisoryPositive = 4,
        AdvisoryNegative = 5
    };

    enum CompareType
    {
        Crisp    = 0,
        Fuzzy    = 1,
        FuzzySqr = 2
    };

    enum ConditionSide
    {
        Friendly = 0,
        Enemy    = 1,
        Both     = 2
    };

    // Equal* compare the difference a - b, Quotient* the quotient a / b, Difference* repeat the
    // Equal* arms (internalCompareCrisp/Fuzzy jump tables 0x4A2A00 / 0x4A2A24).
    enum Compare
    {
        Equal             = 0,
        EqualGreater      = 1,
        EqualSmaller      = 2,
        Quotient          = 3,
        QuotientGreater   = 4,
        QuotientSmaller   = 5,
        Difference        = 6,
        DifferenceGreater = 7,
        DifferenceSmaller = 8
    };

    // Index into the 44-float state arrays. Spelled as the printer spells it ("Infantery").
    enum Conditional
    {
        Carrier = 0, BattleShip = 1, LCVP = 2, Naval = 3, Bomber = 4, Fighter = 5, AntiAir = 6,
        Air = 7, Infantery = 8, Artillery = 9, AntiTank = 10, HeavyAttack = 11, FastAttack = 12,
        Ticket = 13, ControlPoint = 14, Time = 15, StartTime = 16, Attacks = 17, Defences = 18,
        NumberOfFriendlyAreas = 19, NumberOfNeutralAreas = 20, NumberOfHostileAreas = 21,
        Flank = 22, Base = 23, Close = 24, Centre = 25, Remote = 26, Route = 27, Bridge = 28,
        North = 29, West = 30, South = 31, East = 32, FrontFlank = 33, FrontBase = 34,
        FrontClose = 35, FrontCentre = 36, FrontRemote = 37, FrontNeutral = 38, Front = 39,
        Safe = 40, EnemyObject = 41, UnReachable = 42, UnitConstant = 43
    };

    // ELF 0x334150: vptr, abort flag false, strength 0 (Required), name copied last.
    StrategyCondition(const std::string& name, float target, Compare compare, ConditionSide side,
                      CompareType compareType, Conditional conditional);
    virtual ~StrategyCondition() {}                                     // ELF 0x332790 (weak)
    virtual int getType() const { return 4; }                          // ELF 0x332860 (weak)
    virtual float compare(float* friendly, float* enemy, int side) const = 0;

    float setTargetValue(float value);            // ELF 0x334230; fsts leaves the value in st0
    float getTargetValue() const;
    void setConditionStrength(ConditionStrength strength);
    ConditionStrength getConditionStrength() const;
    bool setIsAbortCondition(bool abort);         // ELF 0x334540; answers the stored flag
    bool getIsAbortCondition() const;
    bool isAbortCondition() const;

    float internalCompare(float a, float b) const;
    float internalCompareCrisp(float a, float b) const;
    float internalCompareFuzzy(float a, float b) const;

    /* +0x00 */ // vptr
    /* +0x04 */ bool m_isAbortCondition;
    /* +0x08 */ float m_targetValue;
    /* +0x0C */ Compare m_compare;
    /* +0x10 */ CompareType m_compareType;
    /* +0x14 */ ConditionSide m_side;
    /* +0x18 */ Conditional m_conditional;
    /* +0x1C */ ConditionStrength m_strength;
    /* +0x20 */ std::string m_name;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGIC_STRATEGYCONDITION_H
