// Header placement [INFERRED]: beside its .cpp, whose path AIDLL_r.dll confirms (AIUtils/SCurve.cpp).
// Split out of CustomCurve.h; CustomCurve.h includes it so existing users still see SCurve.

#ifndef AI_AIDLL_AI_AIUTILS_SCURVE_H
#define AI_AIDLL_AI_AIUTILS_SCURVE_H

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

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIUTILS_SCURVE_H
