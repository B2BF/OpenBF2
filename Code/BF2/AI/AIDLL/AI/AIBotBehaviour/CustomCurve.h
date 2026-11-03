// Samples a supplied curve function into a table for later evaluation.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_CUSTOMCURVE_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_CUSTOMCURVE_H

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// Static curve table with no per-instance state. Construction initializes the shared
// table and destruction releases it.
class SCurve
{
public:
    SCurve();                          // - calls init, nothing else
    ~SCurve();
    static void  init();
    static float calculate(float x);   // - the only one with a NaN guard
    static float* curveValues;
};

class JCurve
{
public:
    JCurve();
    ~JCurve();
    static void  init();
    static float calculate(float x);   // - extrapolates above 1
    static float* curveValues;
};

class DecleiningSlopeCurve             // dice's spelling
{
public:
    DecleiningSlopeCurve();
    ~DecleiningSlopeCurve();
    static void  init();
    static float calculate(float x);
    static float* curveValues;
};

// A function sampled into a table over [min, max] (AI/AIDLL/AI/AIUtils/CustomCurve.cpp in the image).
// Recovered by ai-w1-d-behaviours for BBFire (its constructor builds one); libaidll.so addresses.
class CustomCurve
{
public:
    typedef float (*Function)(float);

    CustomCurve(Function function, float min, float max, float step);   // 0x36D630
    ~CustomCurve();                                                     // 0x36D7D0

    // Table lookup of x in [min, max] (asserts outside), linearly interpolated when asked.
    float calculate(float x, bool interpolate) const;                   // 0x36D800
    float testAccuracy(float density, bool interpolate) const;
    bool testBounds(float density, float minimum, float maximum) const;
    void accuracyTest(float density) const;
    void getStatistics(float density, float minimum, float maximum, int flags) const;
    float calculateEstimation(float value, bool& nonnegative) const;

    // Weak in BBFire.cpp (0x20BAB0): powf(2x - 0.95, 21) * 1.012 + 1.
    static float modifiedSoftStepFunc(float x);

private:
    CustomCurve(const CustomCurve&);              // 0x36D680
    CustomCurve operator=(const CustomCurve&);    // 0x36D720: returns a resampled copy by value

    void init();                                                        // 0x36DFB0

    /* +0x00 */ Function m_function;
    /* +0x04 */ float*   m_table;   // m_size + 1 samples
    /* +0x08 */ int      m_size;
    /* +0x0C */ float    m_min;
    /* +0x10 */ float    m_max;
    /* +0x14 */ float    m_step;
};

inline float CustomCurve::modifiedSoftStepFunc(float x)
{
    return std::pow(x + x - 0.95f, 21.0f) * 1.012f + 1.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_CUSTOMCURVE_H
