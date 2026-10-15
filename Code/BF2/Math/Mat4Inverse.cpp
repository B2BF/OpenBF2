// Full 4x4 inverse for client and supported aidll builds.
// The dedicated implementation has different rounding and remains declaration-only.
#include "Mat4.h"
#if !DEDICATED || (defined(BF2_IMAGE_AIDLL) && defined(_MSC_VER) && defined(_M_IX86))
namespace dice { namespace hfe {
namespace {
#if defined(_M_IX86)
// Explicit x87 arithmetic keeps the caller's precision/rounding control and
// the shipped float stores. A product of two input floats fits in a double;
// the second group's pair10 deliberately has no intermediate float store.
float roundedPairProduct(float left, float right)
{
    float value;
    __asm {
        fld left
        fmul right
        fstp value
    }
    return value;
}
double pairProduct(float left, float right)
{
    double value;
    __asm {
        fld left
        fmul right
        fstp value
    }
    return value;
}
void cofactor(float& output, double a0, double b0, double a1, double b1,
              double a2, double b2, double a3, double b3, double a4, double b4,
              double a5, double b5)
{
    __asm {
        mov edx, output
        fld a0
        fmul b0
        fld a1
        fmul b1
        faddp st(1), st
        fld a2
        fmul b2
        faddp st(1), st
        fstp dword ptr [edx]
        fld a3
        fmul b3
        fld a4
        fmul b4
        faddp st(1), st
        fld a5
        fmul b5
        faddp st(1), st
        fsubr dword ptr [edx]
        fstp dword ptr [edx]
    }
}
void divideByDeterminant(const float* input, float* output)
{
    const float one = 1.0f;
    // Retain the reciprocal on the x87 stack throughout all16 scaled stores.
    // No zero/NaN/infinity test exists in either shipped image.
    __asm {
        mov ecx, input
        mov edx, output
        fld dword ptr [ecx+16]
        fmul dword ptr [edx+4]
        fld dword ptr [ecx]
        fmul dword ptr [edx]
        faddp st(1), st
        fld dword ptr [ecx+32]
        fmul dword ptr [edx+8]
        faddp st(1), st
        fld dword ptr [ecx+48]
        fmul dword ptr [edx+12]
        faddp st(1), st
        fdivr one
        mov ecx, 16
    scaleElement:
        fld st
        fmul dword ptr [edx]
        fstp dword ptr [edx]
        add edx, 4
        dec ecx
        jnz scaleElement
        fstp st
    }
}
#else
// [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates, under the caller's precision
// control). Each helper keeps the x87 body's operation order and its float/double store widths
// (issue #7).
float roundedPairProduct(float left, float right)
{
    // fld; fmul; fstp dword - one rounding of the exact product to float.
    return static_cast<float>(static_cast<double>(left) * static_cast<double>(right));
}
double pairProduct(float left, float right)
{
    // fld; fmul; fstp qword - the exact product, no float rounding.
    return static_cast<double>(left) * static_cast<double>(right);
}
void cofactor(float& output, double a0, double b0, double a1, double b1,
              double a2, double b2, double a3, double b3, double a4, double b4,
              double a5, double b5)
{
    // (a0*b0 + a1*b1) + a2*b2 is stored as a float first; the second sum is then subtracted from
    // that stored float (fsubr dword) and the difference stored as a float again.
    output = static_cast<float>(a0 * b0 + a1 * b1 + a2 * b2);
    const double second = a3 * b3 + a4 * b4 + a5 * b5;
    output = static_cast<float>(static_cast<double>(output) - second);
}
void divideByDeterminant(const float* input, float* output)
{
    // ((in4*out1 + in0*out0) + in8*out2) + in12*out3, then 1/det kept unrounded across all sixteen
    // scaled float stores. No zero/NaN/infinity test exists in either shipped image.
    const double determinant =
        static_cast<double>(input[4]) * static_cast<double>(output[1])
        + static_cast<double>(input[0]) * static_cast<double>(output[0])
        + static_cast<double>(input[8]) * static_cast<double>(output[2])
        + static_cast<double>(input[12]) * static_cast<double>(output[3]);
    const double reciprocal = static_cast<double>(1.0f) / determinant;
    for (int i = 0; i < 16; ++i)
        output[i] = static_cast<float>(reciprocal * static_cast<double>(output[i]));
}
#endif
}
void fullInvert(const Mat4& source, Mat4& destination)
{
    // Every input is captured before the first output store, including aliases.
    float input[16];
    for (int i = 0; i < 16; ++i) input[i] = source.m[i];
    double pair[12];
    float* output = destination.m;
    pair[0] = roundedPairProduct(input[15], input[10]);
    pair[1] = roundedPairProduct(input[11], input[14]);
    pair[2] = roundedPairProduct(input[6], input[15]);
    pair[3] = roundedPairProduct(input[7], input[14]);
    pair[4] = roundedPairProduct(input[6], input[11]);
    pair[5] = roundedPairProduct(input[7], input[10]);
    pair[6] = roundedPairProduct(input[2], input[15]);
    pair[7] = roundedPairProduct(input[3], input[14]);
    pair[8] = roundedPairProduct(input[2], input[11]);
    pair[9] = roundedPairProduct(input[3], input[10]);
    pair[10] = roundedPairProduct(input[2], input[7]);
    pair[11] = roundedPairProduct(input[3], input[6]);
    cofactor(output[0], input[13], pair[4], input[9], pair[3], input[5], pair[0], input[13], pair[5], input[9], pair[2], input[5], pair[1]);
    cofactor(output[1], input[13], pair[9], input[9], pair[6], input[1], pair[1], input[13], pair[8], input[9], pair[7], input[1], pair[0]);
    cofactor(output[2], input[13], pair[10], input[5], pair[7], input[1], pair[2], input[13], pair[11], input[5], pair[6], input[1], pair[3]);
    cofactor(output[3], pair[11], input[9], input[5], pair[8], input[1], pair[5], pair[10], input[9], input[5], pair[9], input[1], pair[4]);
    cofactor(output[4], input[12], pair[5], input[8], pair[2], input[4], pair[1], input[12], pair[4], input[8], pair[3], input[4], pair[0]);
    cofactor(output[5], input[12], pair[8], input[0], pair[0], input[8], pair[7], input[12], pair[9], input[0], pair[1], input[8], pair[6]);
    cofactor(output[6], input[12], pair[11], input[0], pair[3], input[4], pair[6], input[12], pair[10], input[0], pair[2], input[4], pair[7]);
    cofactor(output[7], input[0], pair[4], input[8], pair[10], input[4], pair[9], input[0], pair[5], input[8], pair[11], input[4], pair[8]);
    pair[0] = roundedPairProduct(input[8], input[13]);
    pair[1] = roundedPairProduct(input[12], input[9]);
    pair[2] = roundedPairProduct(input[4], input[13]);
    pair[3] = roundedPairProduct(input[12], input[5]);
    pair[4] = roundedPairProduct(input[4], input[9]);
    pair[5] = roundedPairProduct(input[8], input[5]);
    pair[6] = roundedPairProduct(input[0], input[13]);
    pair[7] = roundedPairProduct(input[12], input[1]);
    pair[8] = roundedPairProduct(input[0], input[9]);
    pair[9] = roundedPairProduct(input[8], input[1]);
    pair[10] = pairProduct(input[5], input[0]);
    pair[11] = roundedPairProduct(input[4], input[1]);
    cofactor(output[8], pair[3], input[11], input[7], pair[0], pair[4], input[15], input[7], pair[1], pair[2], input[11], pair[5], input[15]);
    cofactor(output[9], input[3], pair[1], pair[6], input[11], pair[9], input[15], pair[7], input[11], input[3], pair[0], pair[8], input[15]);
    cofactor(output[10], pair[7], input[7], input[3], pair[2], pair[10], input[15], input[3], pair[3], pair[6], input[7], pair[11], input[15]);
    cofactor(output[11], input[3], pair[5], pair[8], input[7], pair[11], input[11], input[3], pair[4], pair[9], input[7], pair[10], input[11]);
    cofactor(output[12], pair[5], input[14], input[6], pair[1], pair[2], input[10], pair[4], input[14], input[6], pair[0], pair[3], input[10]);
    cofactor(output[13], pair[8], input[14], input[2], pair[0], pair[7], input[10], pair[9], input[14], input[2], pair[1], pair[6], input[10]);
    cofactor(output[14], input[2], pair[3], pair[6], input[6], pair[11], input[14], pair[7], input[6], input[2], pair[2], pair[10], input[14]);
    cofactor(output[15], input[2], pair[4], pair[9], input[6], pair[10], input[10], input[2], pair[5], pair[8], input[6], pair[11], input[10]);
    divideByDeterminant(input, output);
}
} }
#endif
