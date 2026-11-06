// [INFERRED] Source placement: the TU's static initializer is _GLOBAL__I__ZN4dice3hfe2ai14AreaAssignmentC2Ev.

#include "AreaAssignment.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x34E7F0, __tcf_0 0x34E850, _GLOBAL__I 0x34E880).
#include <iostream>

#include "../AIStrategic/SAI.h"                   // EnemyStatistics

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x34D5A0 (C2; C1 0x34D630).
AreaAssignment::AreaAssignment()
    : m_effectiveTarget(0), m_effectiveAssigned(0), m_assigned(0), m_target(0), m_testAssigned(0)
    , m_testEffectiveAssigned(0), m_testQuotient(0), m_testDifference(0), m_testAssignedCount(0)
    , m_assignedCount(0), m_targetCount(0), m_difference(0), m_quotient(0)
    , m_sqrDistDifference(-1.0f), m_sqrDistTestDifference(-1.0f), m_sqrDistQuotient(-1.0f)
    , m_sqrDistTestQuotient(-1.0f), m_size(0), m_calculated(false), m_b49(true)
{
}

// ELF 0x34D6C0 (C2; C1 0x34D770).
AreaAssignment::AreaAssignment(int size)
    : m_effectiveTarget(0), m_effectiveAssigned(0), m_assigned(0), m_target(0), m_testAssigned(0)
    , m_testEffectiveAssigned(0), m_testQuotient(0), m_testDifference(0), m_testAssignedCount(0)
    , m_assignedCount(0), m_targetCount(0), m_difference(0), m_quotient(0)
    , m_sqrDistDifference(-1.0f), m_sqrDistTestDifference(-1.0f), m_sqrDistQuotient(-1.0f)
    , m_sqrDistTestQuotient(-1.0f), m_size(0), m_calculated(false), m_b49(true)
{
    setSize(size);
}

// ELF 0x34D820 (C2; C1 0x34D990): fresh arrays of the other's size, then flags, the four
// results and every array element.
AreaAssignment::AreaAssignment(const AreaAssignment& other)
    : m_effectiveTarget(0), m_effectiveAssigned(0), m_assigned(0), m_target(0), m_testAssigned(0)
    , m_testEffectiveAssigned(0), m_testQuotient(0), m_testDifference(0), m_testAssignedCount(0)
    , m_assignedCount(0), m_targetCount(0), m_difference(0), m_quotient(0), m_size(0)
{
    setSize(other.m_size);
    m_calculated = other.m_calculated;
    m_b49 = other.m_b49;
    m_sqrDistDifference = other.m_sqrDistDifference;
    m_sqrDistTestDifference = other.m_sqrDistTestDifference;
    m_sqrDistQuotient = other.m_sqrDistQuotient;
    m_sqrDistTestQuotient = other.m_sqrDistTestQuotient;
    for (int i = 0; i < m_size; ++i)
    {
        m_assigned[i] = other.m_assigned[i];
        m_target[i] = other.m_target[i];
        m_difference[i] = other.m_difference[i];
        m_quotient[i] = other.m_quotient[i];
        m_assignedCount[i] = other.m_assignedCount[i];
        m_testAssigned[i] = other.m_testAssigned[i];
        m_testEffectiveAssigned[i] = other.m_testEffectiveAssigned[i];
        m_testQuotient[i] = other.m_testQuotient[i];
        m_testDifference[i] = other.m_testDifference[i];
        m_testAssignedCount[i] = other.m_testAssignedCount[i];
        m_targetCount[i] = other.m_targetCount[i];
        m_effectiveTarget[i] = other.m_effectiveTarget[i];
        m_effectiveAssigned[i] = other.m_effectiveAssigned[i];
    }
}

// ELF 0x34DB00 (D2; D1 0x34DC60).
AreaAssignment::~AreaAssignment()
{
    delete[] m_assigned;
    delete[] m_target;
    delete[] m_difference;
    delete[] m_quotient;
    delete[] m_assignedCount;
    delete[] m_targetCount;
    delete[] m_effectiveTarget;
    delete[] m_effectiveAssigned;
    delete[] m_testAssigned;
    delete[] m_testEffectiveAssigned;
    delete[] m_testQuotient;
    delete[] m_testDifference;
    delete[] m_testAssignedCount;
}

// ELF 0x34DDC0.
void AreaAssignment::setSize(int size)
{
    if (!m_size)
    {
        m_size = size;
        m_assigned = new float[m_size];
        m_target = new float[m_size];
        m_targetCount = new int[m_size];
        m_difference = new float[m_size];
        m_quotient = new float[m_size];
        m_assignedCount = new int[m_size];
        m_effectiveAssigned = new float[m_size];
        m_effectiveTarget = new float[m_size];
        m_testAssigned = new float[m_size];
        m_testEffectiveAssigned = new float[m_size];
        m_testQuotient = new float[m_size];
        m_testDifference = new float[m_size];
        m_testAssignedCount = new int[m_size];
    }
    for (int i = 0; i < m_size; ++i)
    {
        m_assigned[i] = 0.0f;
        m_target[i] = 0.0f;
        m_difference[i] = 0.0f;
        m_quotient[i] = 0.0f;
        m_assignedCount[i] = 0;
        m_testAssigned[i] = 0.0f;
        m_testEffectiveAssigned[i] = 0.0f;
        m_testQuotient[i] = 0.0f;
        m_testDifference[i] = 0.0f;
        m_testAssignedCount[i] = 0;
        m_targetCount[i] = 0;
        m_effectiveTarget[i] = 0.0f;
        m_effectiveAssigned[i] = 0.0f;
    }
    m_calculated = false;
}

// ELF 0x34DF50: the target and its truncated counts; the assignment is cleared.
float* AreaAssignment::setVecToMatch(float* strength, float* count)
{
    for (int i = 0; i < m_size; ++i)
    {
        m_target[i] = strength[i];
        m_targetCount[i] = static_cast<int>(count[i]);
        m_assigned[i] = 0.0f;
        m_assignedCount[i] = 0;
    }
    m_calculated = false;
    return m_target;
}

// ELF 0x34DFC0: as above from the statistics' strength and count arrays.
float* AreaAssignment::setVecToMatch(EnemyStatistics* statistics)
{
    float* const strength = statistics->m_strength;
    float* const count = statistics->m_count;
    for (int i = 0; i < m_size; ++i)
    {
        m_target[i] = strength[i];
        m_targetCount[i] = static_cast<int>(count[i]);
        m_assigned[i] = 0.0f;
        m_assignedCount[i] = 0;
    }
    m_calculated = false;
    return m_target;
}

// ELF 0x34E050.
int AreaAssignment::getNumberOfAssignedUnits()
{
    int units = 0;
    for (int i = 0; i < m_size; ++i)
        units += m_assignedCount[i];
    return units;
}

// ELF 0x34E080. As shipped, the target strength is divided by the assigned count and the
// assigned strength by the target count; a zero numerator or count gives 0.
void AreaAssignment::calculateEffectiveStrengths()
{
    for (int i = 0; i < m_size; ++i)
    {
        if (m_target[i] != 0.0f && m_assignedCount[i] != 0)
            m_effectiveTarget[i] = m_target[i] / static_cast<float>(m_assignedCount[i]);
        else
            m_effectiveTarget[i] = 0.0f;
        if (m_assigned[i] != 0.0f && m_targetCount[i] != 0)
            m_effectiveAssigned[i] = m_assigned[i] / static_cast<float>(m_targetCount[i]);
        else
            m_effectiveAssigned[i] = 0.0f;
    }
    m_calculated = true;
}

// ELF 0x34E130: difference = effective target - effective assigned.
void AreaAssignment::calculateDifference()
{
    add(-1.0f, m_difference, m_effectiveTarget, m_effectiveAssigned);
}

// ELF 0x34E180.
float* AreaAssignment::getDifference()
{
    return m_difference;
}

// ELF 0x34E190.
float AreaAssignment::calculateSqrDistDifference()
{
    return m_sqrDistDifference = sqrDist(m_difference);
}

// ELF 0x34E1D0.
float AreaAssignment::getSqrDistDifference()
{
    return m_sqrDistDifference;
}

// ELF 0x34E1E0.
float* AreaAssignment::getQuotient()
{
    return m_quotient;
}

// ELF 0x34E1F0.
void AreaAssignment::calculateQuotient()
{
    quotient(m_quotient, m_effectiveAssigned);
}

// ELF 0x34E230.
float AreaAssignment::getSummedQuotient()
{
    return sum(m_quotient);
}

// ELF 0x34E260.
float AreaAssignment::calculateSqrDistQuotient()
{
    return m_sqrDistQuotient = sqrDist(m_quotient);
}

// ELF 0x34E2A0.
float AreaAssignment::getSqrDistQuotient()
{
    return m_sqrDistQuotient;
}

// ELF 0x34E2B0.
void AreaAssignment::calculate()
{
    calculateEffectiveStrengths();
    calculateQuotient();
    calculateDifference();
    calculateSqrDistQuotient();
    calculateSqrDistDifference();
}

// ELF 0x34E300: test strength = strength + test strength; one more test unit at `index`.
float* AreaAssignment::addTestStrength(float* strength, int index)
{
    add(1.0f, m_testAssigned, strength, m_testAssigned);
    ++m_testAssignedCount[index];
    return m_testAssigned;
}

// ELF 0x34E360.
void AreaAssignment::calculateTestDifference()
{
    add(-1.0f, m_testDifference, m_effectiveTarget, m_testEffectiveAssigned);
}

// ELF 0x34E3B0.
float* AreaAssignment::getTestDifference()
{
    return m_testDifference;
}

// ELF 0x34E3C0. Shipped defect, preserved: the result is not stored in m_sqrDistTestDifference.
float AreaAssignment::calculateSqrDistTestDifference()
{
    return sqrDist(m_testDifference);
}

// ELF 0x34E3F0.
float AreaAssignment::getSqrDistTestDifference()
{
    return m_sqrDistTestDifference;
}

// ELF 0x34E400.
float AreaAssignment::summedTestStrength()
{
    return sum(m_testAssigned);
}

// ELF 0x34E430.
void AreaAssignment::calculateTestQuotient()
{
    quotient(m_testQuotient, m_testEffectiveAssigned);
}

// ELF 0x34E470.
float* AreaAssignment::getTestQuotient()
{
    return m_testQuotient;
}

// ELF 0x34E480.
float AreaAssignment::getSummedTestQuotient()
{
    return sum(m_testQuotient);
}

// ELF 0x34E4B0.
float AreaAssignment::calculateSqrDistTestQuotient()
{
    return m_sqrDistTestQuotient = sqrDist(m_testQuotient);
}

// ELF 0x34E4F0.
float AreaAssignment::getSqrDistTestQuotient()
{
    return m_sqrDistTestQuotient;
}

// ELF 0x34E500.
float* AreaAssignment::addStrength(float* strength, int index)
{
    add(1.0f, m_assigned, strength);
    ++m_assignedCount[index];
    m_calculated = false;
    return m_assigned;
}

// ELF 0x34E560.
void AreaAssignment::makeTestActual()
{
    for (int i = 0; i < m_size; ++i)
    {
        m_assigned[i] = m_testAssigned[i];
        m_effectiveAssigned[i] = m_testEffectiveAssigned[i];
        m_quotient[i] = m_testQuotient[i];
        m_difference[i] = m_testDifference[i];
        m_assignedCount[i] = m_testAssignedCount[i];
    }
}

// ELF 0x34E5C0.
void AreaAssignment::resetTestCase()
{
    for (int i = 0; i < m_size; ++i)
    {
        m_testAssigned[i] = m_assigned[i];
        m_testEffectiveAssigned[i] = m_effectiveAssigned[i];
        m_testQuotient[i] = m_quotient[i];
        m_testDifference[i] = m_difference[i];
        m_testAssignedCount[i] = m_assignedCount[i];
    }
}

// ELF 0x34E620: any negative difference entry.
bool AreaAssignment::hasNegativeStrength()
{
    for (int i = 0; i < m_size; ++i)
    {
        if (m_difference[i] < 0.0f)
            return true;
    }
    return false;
}

// ELF 0x34E670: out = in / effective target, 0 where either is 0.
void AreaAssignment::quotient(float* out, float* in)
{
    for (int i = 0; i < m_size; ++i)
    {
        if (in[i] == 0.0f || m_effectiveTarget[i] == 0.0f)
            out[i] = 0.0f;
        else
            out[i] = in[i] / m_effectiveTarget[i];
    }
}

// ELF 0x34E6E0: inOut += factor * in.
void AreaAssignment::add(float factor, float* inOut, float* in)
{
    for (int i = 0; i < m_size; ++i)
        inOut[i] = inOut[i] + factor * in[i];
}

// ELF 0x34E720: out = in + factor * scaled.
void AreaAssignment::add(float factor, float* out, float* in, float* scaled)
{
    for (int i = 0; i < m_size; ++i)
        out[i] = in[i] + factor * scaled[i];
}

// ELF 0x34E760.
float AreaAssignment::sum(float* values)
{
    float total = 0.0f;
    for (int i = 0; i < m_size; ++i)
        total += values[i];
    return total;
}

// ELF 0x34E7A0.
float AreaAssignment::sqrDist(float* values)
{
    float total = 0.0f;
    for (int i = 0; i < m_size; ++i)
    {
        if (values[i] > 0.0f)
            total += values[i] * values[i];
    }
    return total;
}

} // namespace ai
} // namespace hfe
} // namespace dice
