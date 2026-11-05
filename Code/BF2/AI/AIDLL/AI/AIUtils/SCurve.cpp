// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\AIUtils\SCurve.cpp" (SCurve::calculate
// 0x101AA8E0 asserts "dep: SCurve::calculate() saw NaN value -- fix me"; retail 1.50 has no assert
// and returns 0). Split out of CustomCurve.cpp; the functions are unchanged. In the ELF SCurve's
// functions follow JCurve's (0x36EEE0..0x36F500) without a static-init block of their own.

#include "SCurve.h"

#include <cmath>       // isfinite - SCurve's guard

namespace dice {
namespace hfe {
namespace ai {

float* SCurve::curveValues               = 0;

// Builds the shared table on construction and frees it on destruction.
// Destruction retains the null guard and does not clear the shared pointer.
SCurve::SCurve()
{
    init();
}

SCurve::~SCurve()
{
    if (curveValues)
        delete[] curveValues;
}

// Returns zero for non-finite input and clamps x >= 1 to one before interpolation.
// [PLATFORM] Uses the standard isfinite name for the original finite predicate.
float SCurve::calculate(float x)
{
    if (!std::isfinite(x))
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;
    if (x < 0.0f)
        return 0.0f;

    const float scaled = x * 100.0f;
    const int   index  = static_cast<int>(scaled);
    const float frac   = scaled - static_cast<float>(index);

    return frac * curveValues[index + 1] + (1.0f - frac) * curveValues[index];
}

// 101 constants, transcribed.
void SCurve::init()
{
    curveValues = new float[101];   // operator new[](0x194) = 101 * 4

    curveValues[  0] = 0.0f;
    curveValues[  1] = 0.00224938989f;
    curveValues[  2] = 0.00338781998f;
    curveValues[  3] = 0.00463052979f;
    curveValues[  4] = 0.00598675013f;
    curveValues[  5] = 0.00746650994f;
    curveValues[  6] = 0.00908064004f;
    curveValues[  7] = 0.0108407997f;
    curveValues[  8] = 0.0127597004f;
    curveValues[  9] = 0.0148508996f;

    curveValues[ 10] = 0.0171288997f;
    curveValues[ 11] = 0.0196096003f;
    curveValues[ 12] = 0.0223098006f;
    curveValues[ 13] = 0.0252474006f;
    curveValues[ 14] = 0.0284416005f;
    curveValues[ 15] = 0.0319131017f;
    curveValues[ 16] = 0.0356835015f;
    curveValues[ 17] = 0.0397756994f;
    curveValues[ 18] = 0.0442142002f;
    curveValues[ 19] = 0.0490242988f;

    curveValues[ 20] = 0.0542327985f;
    curveValues[ 21] = 0.0598676018f;
    curveValues[ 22] = 0.0659573004f;
    curveValues[ 23] = 0.0725317001f;
    curveValues[ 24] = 0.0796212032f;
    curveValues[ 25] = 0.0872564986f;
    curveValues[ 26] = 0.0954684988f;
    curveValues[ 27] = 0.104287997f;
    curveValues[ 28] = 0.113744996f;
    curveValues[ 29] = 0.123869002f;

    curveValues[ 30] = 0.134688005f;
    curveValues[ 31] = 0.146227002f;
    curveValues[ 32] = 0.158509001f;
    curveValues[ 33] = 0.171553999f;
    curveValues[ 34] = 0.185376003f;
    curveValues[ 35] = 0.199987993f;
    curveValues[ 36] = 0.215393007f;
    curveValues[ 37] = 0.231592f;
    curveValues[ 38] = 0.248576f;
    curveValues[ 39] = 0.266330004f;

    curveValues[ 40] = 0.284830987f;
    curveValues[ 41] = 0.304048002f;
    curveValues[ 42] = 0.323940992f;
    curveValues[ 43] = 0.344460994f;
    curveValues[ 44] = 0.365550995f;
    curveValues[ 45] = 0.387147993f;
    curveValues[ 46] = 0.409179002f;
    curveValues[ 47] = 0.431564987f;
    curveValues[ 48] = 0.45422399f;
    curveValues[ 49] = 0.477064997f;

    curveValues[ 50] = 0.5f;
    curveValues[ 51] = 0.52293402f;
    curveValues[ 52] = 0.54577601f;
    curveValues[ 53] = 0.568435013f;
    curveValues[ 54] = 0.590821028f;
    curveValues[ 55] = 0.612851977f;
    curveValues[ 56] = 0.634449005f;
    curveValues[ 57] = 0.655538976f;
    curveValues[ 58] = 0.676059008f;
    curveValues[ 59] = 0.695951998f;

    curveValues[ 60] = 0.715167999f;
    curveValues[ 61] = 0.733669996f;
    curveValues[ 62] = 0.751424015f;
    curveValues[ 63] = 0.768408f;
    curveValues[ 64] = 0.784606993f;
    curveValues[ 65] = 0.800011992f;
    curveValues[ 66] = 0.814624012f;
    curveValues[ 67] = 0.828445971f;
    curveValues[ 68] = 0.841490984f;
    curveValues[ 69] = 0.853772998f;

    curveValues[ 70] = 0.86531198f;
    curveValues[ 71] = 0.876130998f;
    curveValues[ 72] = 0.886255026f;
    curveValues[ 73] = 0.895712018f;
    curveValues[ 74] = 0.904532015f;
    curveValues[ 75] = 0.912743986f;
    curveValues[ 76] = 0.920378983f;
    curveValues[ 77] = 0.927468002f;
    curveValues[ 78] = 0.93404299f;
    curveValues[ 79] = 0.940132022f;

    curveValues[ 80] = 0.945766985f;
    curveValues[ 81] = 0.950976014f;
    curveValues[ 82] = 0.95578599f;
    curveValues[ 83] = 0.960223973f;
    curveValues[ 84] = 0.964317024f;
    curveValues[ 85] = 0.968087018f;
    curveValues[ 86] = 0.971557975f;
    curveValues[ 87] = 0.974753022f;
    curveValues[ 88] = 0.977689981f;
    curveValues[ 89] = 0.980390012f;

    curveValues[ 90] = 0.982870996f;
    curveValues[ 91] = 0.985149026f;
    curveValues[ 92] = 0.987240016f;
    curveValues[ 93] = 0.989158988f;
    curveValues[ 94] = 0.990918994f;
    curveValues[ 95] = 0.992533982f;
    curveValues[ 96] = 0.994013011f;
    curveValues[ 97] = 0.995369017f;
    curveValues[ 98] = 0.996612012f;
    curveValues[ 99] = 0.997750998f;

    curveValues[100] = 1.0f;

}

} // namespace ai
} // namespace hfe
} // namespace dice
