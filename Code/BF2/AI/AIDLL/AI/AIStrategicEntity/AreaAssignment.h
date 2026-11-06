// Per-equipment strength bookkeeping for matching assigned units against a target distribution
// (libaidll.so AreaAssignment.cpp, 0x34D5A0..0x34E8A0). Nothing in the shipped image constructs
// one: every caller is another AreaAssignment member.
// [INFERRED] Source placement and member names; the offsets are the ELF layout.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_AREAASSIGNMENT_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_AREAASSIGNMENT_H

namespace dice {
namespace hfe {
namespace ai {

class EnemyStatistics;

class AreaAssignment
{
public:
    AreaAssignment();                                    // ELF 0x34D5A0 (C2; C1 0x34D630)
    explicit AreaAssignment(int size);                   // ELF 0x34D6C0 (C2; C1 0x34D770)
    AreaAssignment(const AreaAssignment& other);         // ELF 0x34D820 (C2; C1 0x34D990)
    ~AreaAssignment();                                   // ELF 0x34DB00 (D2; D1 0x34DC60)

    // Allocates the thirteen arrays on first use (a later call keeps the old size and only
    // clears them).
    void setSize(int size);
    // Both answer the target-strength array (the image reloads +0xC into eax). [INFERRED] type.
    float* setVecToMatch(float* strength, float* count);
    float* setVecToMatch(EnemyStatistics* statistics);
    int getNumberOfAssignedUnits();

    void calculateEffectiveStrengths();
    void calculateDifference();
    float* getDifference();
    float calculateSqrDistDifference();
    float getSqrDistDifference();
    float* getQuotient();
    void calculateQuotient();
    float getSummedQuotient();
    float calculateSqrDistQuotient();
    float getSqrDistQuotient();
    void calculate();

    // Both answer the strength array they add into (explicit reload in the image).
    float* addTestStrength(float* strength, int index);
    void calculateTestDifference();
    float* getTestDifference();
    float calculateSqrDistTestDifference();
    float getSqrDistTestDifference();
    float summedTestStrength();
    void calculateTestQuotient();
    float* getTestQuotient();
    float getSummedTestQuotient();
    float calculateSqrDistTestQuotient();
    float getSqrDistTestQuotient();
    float* addStrength(float* strength, int index);
    void makeTestActual();
    void resetTestCase();
    bool hasNegativeStrength();

    // Element-wise helpers over m_size entries. The void returns are [INFERRED]: their
    // callers forward eax without using it.
    void quotient(float* out, float* in);
    void add(float factor, float* inOut, float* in);
    void add(float factor, float* out, float* in, float* scaled);
    float sum(float* values);
    // The sum of the squares of the positive entries.
    float sqrDist(float* values);

private:
    /* +0x00 */ float* m_effectiveTarget;     // m_target / m_assignedCount
    /* +0x04 */ float* m_effectiveAssigned;   // m_assigned / m_targetCount
    /* +0x08 */ float* m_assigned;
    /* +0x0C */ float* m_target;
    /* +0x10 */ float* m_testAssigned;
    /* +0x14 */ float* m_testEffectiveAssigned;
    /* +0x18 */ float* m_testQuotient;
    /* +0x1C */ float* m_testDifference;
    /* +0x20 */ int* m_testAssignedCount;
    /* +0x24 */ int* m_assignedCount;
    /* +0x28 */ int* m_targetCount;
    /* +0x2C */ float* m_difference;
    /* +0x30 */ float* m_quotient;
    /* +0x34 */ float m_sqrDistDifference;
    // -1 from the constructors; calculateSqrDistTestDifference does not store its result here.
    /* +0x38 */ float m_sqrDistTestDifference;
    /* +0x3C */ float m_sqrDistQuotient;
    /* +0x40 */ float m_sqrDistTestQuotient;
    /* +0x44 */ int m_size;
    /* +0x48 */ bool m_calculated;            // set by calculateEffectiveStrengths
    /* +0x49 */ bool m_b49;                   // true from every constructor [UNRESOLVED]
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_AREAASSIGNMENT_H
