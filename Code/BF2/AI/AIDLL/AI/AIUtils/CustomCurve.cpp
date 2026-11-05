// Source placement: retail libaidll.so 1.50 embeds "AI/AIDLL/AI/AIUtils/CustomCurve.cpp" (CustomCurve's asserts).

#include "CustomCurve.h"

#include <cassert>     // CustomCurve: the image asserts

#include <cmath>
#include <iostream>
// static init: ELF 0x36E0E0, 0x36E140, 0x36E170: CustomCurve.cpp's iostream Init only.

namespace dice {
namespace hfe {
namespace ai {

// SCurve lives in SCurve.cpp (AIDLL_r __FILE__).
float* JCurve::curveValues               = 0;
float* DecleiningSlopeCurve::curveValues = 0;

// Builds the shared table on construction and frees it on destruction.
// Destruction retains the null guard and does not clear the shared pointer.
JCurve::JCurve()
{
    init();
}

JCurve::~JCurve()
{
    if (curveValues)
        delete[] curveValues;
}

DecleiningSlopeCurve::DecleiningSlopeCurve()
{
    init();
}

DecleiningSlopeCurve::~DecleiningSlopeCurve()
{
    if (curveValues)
        delete[] curveValues;
}

// Extrapolates above one and has no NaN guard. At x == 1, interpolation reads one
// element beyond the 101-entry table even though its coefficient is zero.
float JCurve::calculate(float x)
{
    if (x > 1.0f)
        return x / 100.0f + 1.0f;
    if (x < 0.0f)
        return 0.0f;

    const float scaled = x * 100.0f;
    const int   index  = static_cast<int>(scaled);
    const float frac   = scaled - static_cast<float>(index);

    return frac * curveValues[index + 1] + (1.0f - frac) * curveValues[index];
}

// Has no NaN guard and reads past the table at x == 1. Preserves the interpolation
// term order, which is reversed relative to the other curves.
float DecleiningSlopeCurve::calculate(float x)
{
    if (x > 1.0f)
        return 1.0f;
    if (x < 0.0f)
        return 0.0f;

    const float scaled = x * 100.0f;
    const int   index  = static_cast<int>(scaled);
    const float frac   = scaled - static_cast<float>(index);

    return (1.0f - frac) * curveValues[index] + frac * curveValues[index + 1];
}

// 101 constants, transcribed.
void JCurve::init()
{
    curveValues = new float[101];   // operator new[](0x194) = 101 * 4

    curveValues[  0] = 0.0f;
    curveValues[  1] = 9.99999996e-13f;
    curveValues[  2] = 6.39999997e-11f;
    curveValues[  3] = 7.29000027e-10f;
    curveValues[  4] = 4.09599998e-09f;
    curveValues[  5] = 1.5625e-08f;
    curveValues[  6] = 4.66560017e-08f;
    curveValues[  7] = 1.17649002e-07f;
    curveValues[  8] = 2.62143999e-07f;
    curveValues[  9] = 5.31441003e-07f;

    curveValues[ 10] = 9.99999997e-07f;
    curveValues[ 11] = 1.77156005e-06f;
    curveValues[ 12] = 2.98598002e-06f;
    curveValues[ 13] = 4.8268098e-06f;
    curveValues[ 14] = 7.5295402e-06f;
    curveValues[ 15] = 1.13905999e-05f;
    curveValues[ 16] = 1.67771996e-05f;
    curveValues[ 17] = 2.41376001e-05f;
    curveValues[ 18] = 3.40121987e-05f;
    curveValues[ 19] = 4.70459017e-05f;

    curveValues[ 20] = 6.39999998e-05f;
    curveValues[ 21] = 8.57660998e-05f;
    curveValues[ 22] = 0.000113380003f;
    curveValues[ 23] = 0.000148035993f;
    curveValues[ 24] = 0.000191102998f;
    curveValues[ 25] = 0.000244141003f;
    curveValues[ 26] = 0.000308916002f;
    curveValues[ 27] = 0.000387420005f;
    curveValues[ 28] = 0.000481889991f;
    curveValues[ 29] = 0.000594822981f;

    curveValues[ 30] = 0.000729000021f;
    curveValues[ 31] = 0.000887504022f;
    curveValues[ 32] = 0.00107373996f;
    curveValues[ 33] = 0.00129147002f;
    curveValues[ 34] = 0.0015448f;
    curveValues[ 35] = 0.00183826999f;
    curveValues[ 36] = 0.00217678002f;
    curveValues[ 37] = 0.0025657299f;
    curveValues[ 38] = 0.00301094004f;
    curveValues[ 39] = 0.00351873995f;

    curveValues[ 40] = 0.00409599999f;
    curveValues[ 41] = 0.00475009996f;
    curveValues[ 42] = 0.00548902992f;
    curveValues[ 43] = 0.00632135989f;
    curveValues[ 44] = 0.00725630997f;
    curveValues[ 45] = 0.00830376986f;
    curveValues[ 46] = 0.00947429985f;
    curveValues[ 47] = 0.0107792001f;
    curveValues[ 48] = 0.0122306002f;
    curveValues[ 49] = 0.0138413003f;

    curveValues[ 50] = 0.015625f;
    curveValues[ 51] = 0.0175963007f;
    curveValues[ 52] = 0.0197705999f;
    curveValues[ 53] = 0.0221644007f;
    curveValues[ 54] = 0.0247949008f;
    curveValues[ 55] = 0.0276806001f;
    curveValues[ 56] = 0.0308410004f;
    curveValues[ 57] = 0.0342964008f;
    curveValues[ 58] = 0.0380687006f;
    curveValues[ 59] = 0.0421805009f;

    curveValues[ 60] = 0.0466560014f;
    curveValues[ 61] = 0.0515203997f;
    curveValues[ 62] = 0.0568002015f;
    curveValues[ 63] = 0.0625234991f;
    curveValues[ 64] = 0.0687194988f;
    curveValues[ 65] = 0.075418897f;
    curveValues[ 66] = 0.0826539993f;
    curveValues[ 67] = 0.0904584005f;
    curveValues[ 68] = 0.0988674983f;
    curveValues[ 69] = 0.107918002f;

    curveValues[ 70] = 0.117648996f;
    curveValues[ 71] = 0.128099993f;
    curveValues[ 72] = 0.139313996f;
    curveValues[ 73] = 0.151334003f;
    curveValues[ 74] = 0.164205998f;
    curveValues[ 75] = 0.177979007f;
    curveValues[ 76] = 0.192699999f;
    curveValues[ 77] = 0.208422005f;
    curveValues[ 78] = 0.225199997f;
    curveValues[ 79] = 0.243086994f;

    curveValues[ 80] = 0.262143999f;
    curveValues[ 81] = 0.282429993f;
    curveValues[ 82] = 0.304006994f;
    curveValues[ 83] = 0.32694f;
    curveValues[ 84] = 0.351298004f;
    curveValues[ 85] = 0.377149999f;
    curveValues[ 86] = 0.404567003f;
    curveValues[ 87] = 0.433625996f;
    curveValues[ 88] = 0.464403987f;
    curveValues[ 89] = 0.496980995f;

    curveValues[ 90] = 0.531440973f;
    curveValues[ 91] = 0.567869008f;
    curveValues[ 92] = 0.606355011f;
    curveValues[ 93] = 0.646990001f;
    curveValues[ 94] = 0.68987f;
    curveValues[ 95] = 0.735091984f;
    curveValues[ 96] = 0.782757998f;
    curveValues[ 97] = 0.83297199f;
    curveValues[ 98] = 0.885842025f;
    curveValues[ 99] = 0.941479981f;

    curveValues[100] = 1.0f;

}

// 101 constants, transcribed.
void DecleiningSlopeCurve::init()
{
    curveValues = new float[101];   // operator new[](0x194) = 101 * 4

    curveValues[  0] = 0.0f;
    curveValues[  1] = 0.0292421058f;
    curveValues[  2] = 0.0584842116f;
    curveValues[  3] = 0.0877263099f;
    curveValues[  4] = 0.116968423f;
    curveValues[  5] = 0.146210521f;
    curveValues[  6] = 0.17545262f;
    curveValues[  7] = 0.204694748f;
    curveValues[  8] = 0.233936846f;
    curveValues[  9] = 0.263178945f;

    curveValues[ 10] = 0.292421043f;
    curveValues[ 11] = 0.321663141f;
    curveValues[ 12] = 0.35090524f;
    curveValues[ 13] = 0.380147398f;
    curveValues[ 14] = 0.409389496f;
    curveValues[ 15] = 0.438631594f;
    curveValues[ 16] = 0.467873693f;
    curveValues[ 17] = 0.497115791f;
    curveValues[ 18] = 0.526357889f;
    curveValues[ 19] = 0.551246345f;

    curveValues[ 20] = 0.578947365f;
    curveValues[ 21] = 0.604010522f;
    curveValues[ 22] = 0.626794755f;
    curveValues[ 23] = 0.647596836f;
    curveValues[ 24] = 0.666666329f;
    curveValues[ 25] = 0.684210539f;
    curveValues[ 26] = 0.7004053f;
    curveValues[ 27] = 0.715399981f;
    curveValues[ 28] = 0.729323208f;
    curveValues[ 29] = 0.742286325f;

    curveValues[ 30] = 0.754386306f;
    curveValues[ 31] = 0.765704215f;
    curveValues[ 32] = 0.776315808f;
    curveValues[ 33] = 0.786284208f;
    curveValues[ 34] = 0.795665324f;
    curveValues[ 35] = 0.804511547f;
    curveValues[ 36] = 0.812865257f;
    curveValues[ 37] = 0.820768476f;
    curveValues[ 38] = 0.828254759f;
    curveValues[ 39] = 0.835357904f;

    curveValues[ 40] = 0.842105269f;
    curveValues[ 41] = 0.848524213f;
    curveValues[ 42] = 0.854636908f;
    curveValues[ 43] = 0.860465288f;
    curveValues[ 44] = 0.866028428f;
    curveValues[ 45] = 0.871345282f;
    curveValues[ 46] = 0.876430511f;
    curveValues[ 47] = 0.881298959f;
    curveValues[ 48] = 0.885965288f;
    curveValues[ 49] = 0.890439987f;

    curveValues[ 50] = 0.894736886f;
    curveValues[ 51] = 0.898865283f;
    curveValues[ 52] = 0.9028337f;
    curveValues[ 53] = 0.906653643f;
    curveValues[ 54] = 0.910331607f;
    curveValues[ 55] = 0.913875818f;
    curveValues[ 56] = 0.917293727f;
    curveValues[ 57] = 0.92059058f;
    curveValues[ 58] = 0.923774779f;
    curveValues[ 59] = 0.926850557f;

    curveValues[ 60] = 0.929824233f;
    curveValues[ 61] = 0.932701111f;
    curveValues[ 62] = 0.935484231f;
    curveValues[ 63] = 0.938178957f;
    curveValues[ 64] = 0.940789521f;
    curveValues[ 65] = 0.943319976f;
    curveValues[ 66] = 0.945773721f;
    curveValues[ 67] = 0.948153675f;
    curveValues[ 68] = 0.950464189f;
    curveValues[ 69] = 0.95270735f;

    curveValues[ 70] = 0.95488739f;
    curveValues[ 71] = 0.957005262f;
    curveValues[ 72] = 0.959064245f;
    curveValues[ 73] = 0.961067379f;
    curveValues[ 74] = 0.963015795f;
    curveValues[ 75] = 0.964912653f;
    curveValues[ 76] = 0.966758907f;
    curveValues[ 77] = 0.968557894f;
    curveValues[ 78] = 0.970310569f;
    curveValues[ 79] = 0.972018957f;

    curveValues[ 80] = 0.973684251f;
    curveValues[ 81] = 0.975308418f;
    curveValues[ 82] = 0.976893723f;
    curveValues[ 83] = 0.978439987f;
    curveValues[ 84] = 0.979949474f;
    curveValues[ 85] = 0.981424272f;
    curveValues[ 86] = 0.982864201f;
    curveValues[ 87] = 0.984270513f;
    curveValues[ 88] = 0.985646307f;
    curveValues[ 89] = 0.986989498f;

    curveValues[ 90] = 0.988304257f;
    curveValues[ 91] = 0.989589512f;
    curveValues[ 92] = 0.990846336f;
    curveValues[ 93] = 0.992076874f;
    curveValues[ 94] = 0.993281066f;
    curveValues[ 95] = 0.994460046f;
    curveValues[ 96] = 0.995613754f;
    curveValues[ 97] = 0.996744215f;
    curveValues[ 98] = 0.99785161f;
    curveValues[ 99] = 0.998936832f;

    curveValues[100] = 1.0f;

}

// ELF 0x36D630
CustomCurve::CustomCurve(Function function, float min, float max, float step)
    : m_function(function)
    , m_table(0)
    , m_min(min)
    , m_max(max)
    , m_step(step)
{
    init();
}

// ELF 0x36D7D0
CustomCurve::~CustomCurve()
{
    if (m_table)
        delete[] m_table;
}

// ELF 0x36D800. The image asserts (expression "0") when x is outside [min, max] and when the
// interpolated index has no successor.
float CustomCurve::calculate(float x, bool interpolate) const
{
    if (m_min > x || x > m_max)
        assert(0);
    // The image keeps the step, offset and fraction on the x87 stack; doubles stand in for that.
    const double step = (static_cast<double>(m_max) - m_min) / static_cast<double>(static_cast<float>(m_size));
    const double offset = static_cast<double>(x) - m_min;
    const int index = static_cast<int>(static_cast<float>(offset / step));
    if (!interpolate)
        return m_table[index];
    const double fraction = (offset - static_cast<double>(static_cast<float>(index)) * step) / step;
    if (index >= m_size)
        assert(0);
    return static_cast<float>(m_table[index] + (static_cast<double>(m_table[index + 1]) - m_table[index]) * fraction);
}

// ELF 0x36DFB0. Samples m_size + 1 points, then writes the two ends again from min and max exactly.
void CustomCurve::init()
{
    if (0.0f >= m_step)
        assert(0);
    if (m_table)
        delete[] m_table;
    m_size = static_cast<int>(static_cast<float>((static_cast<double>(m_max) - m_min) / m_step));
    m_table = new float[m_size + 1];
    const double delta = (static_cast<double>(m_max) - m_min) / static_cast<double>(static_cast<float>(m_size));
    for (int i = 0; i <= m_size; ++i)
        m_table[i] = m_function(m_min + static_cast<float>(i * delta));
    m_table[0] = m_function(m_min);
    m_table[m_size] = m_function(m_max);
}

// ELF 0x36D680 (C2), 0x36D6D0 (C1): resamples the function instead of copying table entries.
CustomCurve::CustomCurve(const CustomCurve& other)
    : m_function(other.m_function), m_table(0), m_min(other.m_min), m_max(other.m_max), m_step(other.m_step)
{
    init();
}

// ELF 0x36D720. Shipped quirk: returns a new, resampled curve by value, even on self-assignment.
CustomCurve CustomCurve::operator=(const CustomCurve& other)
{
    if (this != &other)
    {
        if (m_table) delete[] m_table;
        m_table = 0;
        m_function = other.m_function;
        m_min = other.m_min;
        m_max = other.m_max;
        m_step = other.m_step;
        init();
    }
    return *this;
}

// ELF 0x36D8F0: zero-valued function samples contribute no error but still count in the divisor.
float CustomCurve::testAccuracy(float density, bool interpolate) const
{
    if (0.0f >= density) assert(0);
    float error = 0.0f;
    const float width = m_max - m_min;
    const int count = static_cast<int>(static_cast<float>(m_size) * density);
    const float divisor = static_cast<float>(count);
    const float step = width / divisor;
    for (int i = 0; i < count; ++i)
    {
        const float position = m_min + static_cast<float>(i) * step;
        const float actual = m_function(position);
        const float estimated = calculate(position, interpolate);
        if (actual != 0.0f) error += std::fabs(estimated - actual);
    }
    return error / divisor;
}

// ELF 0x36DA40. Shipped defect: the second pass also asks for interpolation.
bool CustomCurve::testBounds(float density, float minimum, float maximum) const
{
    if (0.0f >= density) assert(0);
    bool valid = true;
    for (float position = m_min; position < m_max;
         position += (m_max - m_min) / (static_cast<float>(m_size) * density))
    {
        const float value = calculate(position, true);
        if (value < minimum || value > maximum)
        {
            std::cout << "Interpolated value[" << position << "] = " << value
                      << " is out of boundary (" << minimum << "," << maximum << ")\n";
            valid = false;
        }
    }
    for (float position = m_min; position < m_max;
         position += (m_max - m_min) / (static_cast<float>(m_size) * density))
    {
        const float value = calculate(position, true);
        if (value < minimum || value > maximum)
        {
            std::cout << "Non-interpolated value[" << position << "] = " << value
                      << " is out of boundary (" << minimum << "," << maximum << ")\n";
            valid = false;
        }
    }
    if (valid)
        std::cout << "All values are within bounds (" << minimum << "," << maximum << ")\n";
    else
        std::cout << "BOUNDS CHECK : Failed\n";
    return valid;
}

// ELF 0x36DD60.
void CustomCurve::accuracyTest(float density) const
{
    std::cout << "\nACCURACY TESTS:\n";
    const float interpolated = testAccuracy(density, true);
    const float direct = testAccuracy(density, false);
    std::cout << "Curve is " << (1.0f - direct) * 100.0f << "% accurate without interpolation\n";
    std::cout << "Curve is " << (1.0f - interpolated) * 100.0f << "% accurate with interpolation\n";
}

// ELF 0x36DE60: the fourth argument is unused; the byte estimate omits the final table element.
void CustomCurve::getStatistics(float density, float minimum, float maximum, int /*flags*/) const
{
    std::cout << "\nCUSTOMCURVE STATISTICS\n----------------------\n";
    std::cout << "Curve has " << m_size << " elements and uses a total of "
              << sizeof(CustomCurve) + m_size * sizeof(float) << " bytes.\n\n";
    testBounds(density, minimum, maximum);
    accuracyTest(density);
}

// ELF 0x36DF50: preserves negative zero and unordered values.
float CustomCurve::calculateEstimation(float value, bool& nonnegative) const
{
    if (value < 0.0f) { nonnegative = false; return -value; }
    nonnegative = true;
    return value;
}

} // namespace ai
} // namespace hfe
} // namespace dice
