// Procedural noise generation. Arithmetic order is significant for floating-point results;
// Native transcendental calls retain the port's libm policy.


#ifdef BF2_IMAGE_RENDDX9

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h" // [CHOSEN, not recovered] x87 conversion results, including invalid inputs.
#endif
#include "PerlinNoise.h"
#include "../Geom/LcgRandom.h"                 // lcgrandStep - sub_1009EF30

#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"          // renddx9abi::logf - dice's debug stream
#include "../../RendDX9Abi/ModuleCommon.h"
#include "Debug.h"
#if !defined(_MSC_VER)
#include "../Platform/X87Compat.h"
#endif

#include <cmath>
#include <cstring>
#include <ctime>
#include <new>

namespace dice {
namespace hfe {
namespace geom {

// dword_1020AF00 - 81035, read as data by getClassList (0x1007DEC7).
extern const unsigned int CID_PerlinNoise = 81035u;

namespace {

// dword_1020AE74 (80035, IPerlinNoise) and dword_10208DC0 (0x3709A4FF, io::IConsoleSaveable) - the
// two ids queryInterface tests after IBase's 1; both read as data.
const unsigned int kIID_IPerlinNoise      = 80035u;
const unsigned int kIID_IConsoleSaveable  = 0x3709A4FFu;

// flt_1020CF68 / flt_1020CFD0 / flt_1020CFCC (2^-24, 2^-25, 2^-26) - the scales that turn the 24-bit
// draw into [0, 1), [0, 1/2) and [0, 1/4) of the range; flt_1020CF58 = 3.1415927f.
const float kTwoPowMinus24 = 5.9604644775390625e-08f;
const float kTwoPowMinus25 = 2.98023223876953125e-08f;
const float kTwoPowMinus26 = 1.490116119384765625e-08f;
const float kPi            = 3.1415927f;

// The 24-bit draw every sampler makes: `or al, 80h; sar eax, 7; add eax, 1` - [1, 2^24], exact in a
// float.
float draw24(int value)
{
    return static_cast<float>(((value | 0x80) >> 7) + 1);
}

} // anonymous namespace


// Construction, reference counting, identity.


// The stores in shipped order after the three vptr writes: refCount 1, then the lcgrand
// state from the seed word before the seed is written - the allocator's leftover +0x10 masked to 31
// bits (shipped; read through memcpy so the stale bytes are copied, not interpreted early) - then the
// seed from time(0), and the six defaults. PE 0x1009F530.
PerlinNoise::PerlinNoise()
{
    m_refCount = 1;
    int stale;
    std::memcpy(&stale, &m_seed, sizeof(stale));
    m_state = stale & 0x7FFFFFFF;
    m_seed = static_cast<int>(std::time(0));
    m_dimension = 0;
    m_smoothing = 0;
    m_interpolation = 2;
    m_octaves = 1;
    m_persistence = 0.25f;
    m_range.x = -1.0f;
    m_range.y = 1.0f;
}

void PerlinNoise::addRef()
{
    ++m_refCount;                                                            // 0x1001A010 (shared)
}

int PerlinNoise::getRef()
{
    return m_refCount;                                                       // 0x10051870 (shared)
}

// 0x1009F6A0: `--ref`; at zero the (trivial) destructor restores both vptrs and the block goes back to
// the engine heap. The new count is the answer.
int PerlinNoise::release()
{
    const int count = --m_refCount;
    if (!count)
    {
        this->~PerlinNoise();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// 0x1009F630: 1 and 80035 answer `this`; 0x3709A4FF answers the IConsoleSaveable subobject when
// `this` is non-null; anything else 0.
void* PerlinNoise::queryInterface(unsigned int iid) const
{
    PerlinNoise* const self = const_cast<PerlinNoise*>(this);
    if (iid == RD9_IID_IBASE || iid == kIID_IPerlinNoise)
        return static_cast<IPerlinNoise*>(self);
    if (iid == kIID_IConsoleSaveable)
        return static_cast< ::dice::hfe::io::IConsoleSaveable*>(self);       // null-tested, `add eax, 4`
    return 0;
}

// 0x1009F460 (`xor al, al; retn 4`): the noise writes no console script.
bool PerlinNoise::makeScript(::dice::hfe::io::IStream* /*stream*/)
{
    return false;
}


// The accessors, 0x1009F000..0x1009F0C0 and 0x1009F470 - one store or one load each. The setters
// are PE 0x1009F000 (seed +0x10), 0x1009F020 (dimension +0x14), 0x1009F040 (smoothing +0x18),
// 0x1009F060 (interpolation +0x1C), 0x1009F080 (octaves +0x20) and 0x1009F0A0 (persistence +0x24),
// each `ret 4`.

// PE 0x1009F000
void  PerlinNoise::setSeed(int seed)                  { m_seed = seed; }
int   PerlinNoise::getSeed()                          { return m_seed; }
// PE 0x1009F020
void  PerlinNoise::setDimension(int dimension)        { m_dimension = dimension; }
int   PerlinNoise::getDimension()                     { return m_dimension; }
// PE 0x1009F040
void  PerlinNoise::setSmoothing(int mode)             { m_smoothing = mode; }
int   PerlinNoise::getSmoothing()                     { return m_smoothing; }
// PE 0x1009F060
void  PerlinNoise::setInterpolation(int mode)         { m_interpolation = mode; }
int   PerlinNoise::getInterpolation()                 { return m_interpolation; }
// PE 0x1009F080
void  PerlinNoise::setOctaves(int octaves)            { m_octaves = octaves; }
int   PerlinNoise::getOctaves()                       { return m_octaves; }
// PE 0x1009F0A0
void  PerlinNoise::setPersistence(float persistence)  { m_persistence = persistence; }
float PerlinNoise::getPersistence()                   { return m_persistence; }
const Vec2& PerlinNoise::getOutputRange()             { return m_range; }

// PE 0x1009F470: y through the FPU, x as a dword - a plain Vec2 copy.
void PerlinNoise::setOutputRange(const Vec2& range)
{
    m_range.y = range.y;
    m_range.x = range.x;
}


// The generator and the three samplers.

namespace {

static void unknownNoiseParameter(int line)
{
    ::renddx9abi::Debug(static_cast<DebugType>(6),
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\PerlinNoise.cpp",
        line, "") << "Unknown param";
}

#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Native internal calls carry ST0 in Real rather than
// narrowing at the C++ float return boundary. Public signatures stay unchanged.
static int noiseOffset(int n, int delta)
{
    const unsigned int bits = static_cast<unsigned int>(n) + static_cast<unsigned int>(delta);
    int result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

// PE 0x1009F0D0
static bf2x87::Real baseNoiseValue(PerlinNoise* self, int n)
{
    const unsigned int seed = static_cast<unsigned int>(self->m_seed) + static_cast<unsigned int>(n);
    if (self->m_smoothing == 0)
    {
        self->m_state = seed & 0x7FFFFFFFu;
        const float hi = self->m_range.y, lo = self->m_range.x;
        const float draw = draw24(self->nextRandom());
        return bf2x87::add(bf2x87::mul(bf2x87::mul(bf2x87::wide(draw),
            bf2x87::sub(bf2x87::wide(hi), bf2x87::wide(lo))), bf2x87::wide(kTwoPowMinus24)),
            bf2x87::wide(lo));
    }
    if (self->m_smoothing != 1)
    {
        unknownNoiseParameter(151);
        return bf2x87::wide(0.0f);
    }

    self->m_state = seed & 0x7FFFFFFFu;
    const float lo = self->m_range.x, hi = self->m_range.y;
    const float first = draw24(self->nextRandom());
    self->m_state = (seed - 1u) & 0x7FFFFFFFu;
    const float previousLo = self->m_range.x, previousHi = self->m_range.y;
    const float center = bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::mul(bf2x87::wide(first),
        bf2x87::sub(bf2x87::wide(hi), bf2x87::wide(lo))), bf2x87::wide(kTwoPowMinus25)),
        bf2x87::mul(bf2x87::wide(lo), bf2x87::wide(0.5f))));
    const float second = draw24(self->nextRandom());
    self->m_state = (seed + 1u) & 0x7FFFFFFFu;
    const float nextHi = self->m_range.y, nextLo = self->m_range.x;
    const float partial = bf2x87::store(bf2x87::add(bf2x87::add(bf2x87::mul(bf2x87::mul(
        bf2x87::wide(second), bf2x87::sub(bf2x87::wide(previousHi), bf2x87::wide(previousLo))),
        bf2x87::wide(kTwoPowMinus26)), bf2x87::mul(bf2x87::wide(previousLo), bf2x87::wide(0.25f))),
        bf2x87::wide(center)));
    const float third = draw24(self->nextRandom());
    return bf2x87::add(bf2x87::add(bf2x87::mul(bf2x87::mul(bf2x87::wide(third),
        bf2x87::sub(bf2x87::wide(nextHi), bf2x87::wide(nextLo))), bf2x87::wide(kTwoPowMinus26)),
        bf2x87::mul(bf2x87::wide(nextLo), bf2x87::wide(0.25f))), bf2x87::wide(partial));
}

// PE 0x1009F2A0
static bf2x87::Real interpolatedNoiseValue(PerlinNoise* self, int n, float t)
{
    switch (self->m_interpolation)
    {
    case 1:
    {
        bf2x87::Real angle = bf2x87::mul(bf2x87::wide(t), bf2x87::wide(kPi));
#if defined(__i386__) || defined(__x86_64__)
        // [CHOSEN, not recovered] Native Intel can execute the original FCOS directly.
        __asm__ volatile("fcos" : "+t"(angle));
#else
        // [CHOSEN, not recovered] Other processors retain the port's native libm policy.
        angle = bf2x87::cosValue(angle);
#endif
        t = bf2x87::store(bf2x87::mul(bf2x87::sub(bf2x87::wide(1.0f), angle), bf2x87::wide(0.5f)));
    }
        // The linear and cosine modes share the weighted-sample operation.
        // fall through
    case 0:
    {
        const float a = bf2x87::store(bf2x87::mul(baseNoiseValue(self, noiseOffset(n, 1)), bf2x87::wide(t)));
        const bf2x87::Real b = baseNoiseValue(self, n);
        return bf2x87::add(bf2x87::mul(bf2x87::sub(bf2x87::wide(1.0f), bf2x87::wide(t)), b), bf2x87::wide(a));
    }
    case 2:
    {
        const float v0 = bf2x87::store(baseNoiseValue(self, noiseOffset(n, -1)));
        const float d01 = bf2x87::store(bf2x87::sub(bf2x87::wide(v0), baseNoiseValue(self, n)));
        const float v3 = bf2x87::store(baseNoiseValue(self, noiseOffset(n, 2)));
        const float p = bf2x87::store(bf2x87::sub(bf2x87::sub(bf2x87::wide(v3),
            baseNoiseValue(self, noiseOffset(n, 1))), bf2x87::wide(d01)));
        const float v2 = bf2x87::store(baseNoiseValue(self, noiseOffset(n, 1)));
        const float linear = bf2x87::store(bf2x87::mul(bf2x87::sub(bf2x87::wide(v2),
            baseNoiseValue(self, noiseOffset(n, -1))), bf2x87::wide(t)));
        bf2x87::Real result = bf2x87::add(baseNoiseValue(self, n), bf2x87::wide(linear));
        // [CHOSEN, not recovered] Native libm supplies the existing _CIpow substitution.
        const bf2x87::Real square = bf2x87::wide(std::pow(static_cast<double>(t), 2.0));
        (void)bf2x87::store(square); // The original FST raises conversion exceptions too.
        result = bf2x87::add(result, bf2x87::mul(bf2x87::sub(bf2x87::wide(d01), bf2x87::wide(p)), square));
        const bf2x87::Real cube = bf2x87::wide(std::pow(static_cast<double>(t), 3.0));
        (void)bf2x87::store(cube);
        return bf2x87::add(result, bf2x87::mul(cube, bf2x87::wide(p)));
    }
    default:
        unknownNoiseParameter(175);
        return bf2x87::wide(0.0f);
    }
}
#endif

} // anonymous namespace


// The same retail function as Geom/LcgRandom.h's lcgrandStep (lcgrand_step), not a separate one:
// the image calls it with ecx = this + 0x0C. The new state is the answer.
int PerlinNoise::nextRandom()
{
    return static_cast<int>(lcgrandStep(reinterpret_cast<unsigned int*>(&m_state)));
}

// Reseeds each coordinate so output depends only on seed, coordinate, range, and smoothing mode.
// Mode 0 draws once; mode 1 weights neighboring samples 1/4, 1/2, 1/4. Unknown modes report an error and return zero.
float PerlinNoise::baseNoise(int n)
{
#if !defined(_MSC_VER)
    return bf2x87::store(baseNoiseValue(this, n));
#else
    const int seed = m_seed + n;
    switch (m_smoothing)
    {
    case 0:
    {
        m_state = seed & 0x7FFFFFFF;
        const float lo = m_range.x;
        const float hi = m_range.y;
        const float r = draw24(nextRandom());
        return r * (hi - lo) * kTwoPowMinus24 + lo;
    }
    case 1:
    {
        m_state = seed & 0x7FFFFFFF;
        const float lo = m_range.x;
        const float hi = m_range.y;
        const float r1 = draw24(nextRandom());
        m_state = (seed - 1) & 0x7FFFFFFF;
        const float a = r1 * (hi - lo) * kTwoPowMinus25 + lo * 0.5f;
        const float r2 = draw24(nextRandom());
        m_state = (seed + 1) & 0x7FFFFFFF;
        const float b = r2 * (m_range.y - m_range.x) * kTwoPowMinus26 + m_range.x * 0.25f;
        const float sum = b + a;
        const float r3 = draw24(nextRandom());
        const float c = r3 * (m_range.y - m_range.x) * kTwoPowMinus26 + m_range.x * 0.25f;
        return c + sum;
    }
    default:
        // sub_10074300(6, __FILE__, 151, "") << "Unknown param"; sub_10074690 - dice's debug stream.
        unknownNoiseParameter(151);
        return 0.0f;
    }
#endif
}

// Interpolates neighboring samples linearly, with cosine smoothing, or cubically.
// Preserve repeated cubic samples and their order because each call changes generator state.
float PerlinNoise::interpolatedNoise(int n, float t)
{
#if !defined(_MSC_VER)
    return bf2x87::store(interpolatedNoiseValue(this, n, t));
#else
    switch (m_interpolation)
    {
    case 0:
    {
        const float a = baseNoise(n + 1) * t;
        return (1.0f - t) * baseNoise(n) + a;
    }
    case 1:
    {
        // `fmul pi` rounds; fcos does not; `fsubr 1.0` rounds once.
        const float angle = t * kPi;
        const float f = static_cast<float>(1.0 - std::cos(static_cast<double>(angle))) * 0.5f;
        const float a = baseNoise(n + 1) * f;
        return (1.0f - f) * baseNoise(n) + a;
    }
    case 2:
    {
        const float v0 = baseNoise(n - 1);
        const float d01 = v0 - baseNoise(n);                                 // P - Q's shared term
        const float v3 = baseNoise(n + 2);
        const float p = (v3 - baseNoise(n + 1)) - d01;                       // the cubic coefficient
        const float v2 = baseNoise(n + 1);
        const float r = (v2 - baseNoise(n - 1)) * t;                         // the linear term
        float result = baseNoise(n) + r;
        // _CIpow(t, 2.0) and (t, 3.0) (dbl_101DDDE8, dbl_1020D038): double results, each rounded by
        // the multiply that consumes it.
        result = result + static_cast<float>(std::pow(static_cast<double>(t), 2.0) *
                                              static_cast<double>(d01 - p));
        result = result + static_cast<float>(std::pow(static_cast<double>(t), 3.0) *
                                              static_cast<double>(p));
        return result;
    }
    default:
        unknownNoiseParameter(175);
        return 0.0f;
    }
#endif
}

// Sums octave samples weighted by persistence. Nonpositive octave counts return zero.
// PE 0x1009F490
float PerlinNoise::octaveSum(float x)
{
    float total = 0.0f;
    for (int i = 0; i < m_octaves; ++i)
    {
        const float persistence = m_persistence;
#if !defined(_MSC_VER)
        // [CHOSEN, not recovered] The scaled coordinate stays in ST0 through FISTP
        // and fractional subtraction; neither the interpolation nor its product is spilled.
        const unsigned int frequencyBits = 1u << (i & 31);
        int frequency;
        std::memcpy(&frequency, &frequencyBits, sizeof(frequency));
        const bf2x87::Real scaled = bf2x87::mul(bf2x87::wide(frequency), bf2x87::wide(x));
        const int whole = bf2x87::toIntTruncate(scaled);
        const float fraction = bf2x87::store(bf2x87::sub(scaled, bf2x87::wide(whole)));
        const bf2x87::Real value = interpolatedNoiseValue(this, whole, fraction);
        // [CHOSEN, not recovered] Retain the native libm substitution for _CIpow.
        const bf2x87::Real amplitude = bf2x87::wide(std::pow(static_cast<double>(persistence), static_cast<double>(i)));
        (void)bf2x87::store(amplitude);
        total = bf2x87::store(bf2x87::add(bf2x87::mul(amplitude, value), bf2x87::wide(total)));
#else
        // `shl edx, cl` - the hardware masks the count to five bits.
        const float scaled = static_cast<float>(static_cast<int>(1u << (i & 31))) * x;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const int whole = bf2x87::toIntTruncate(scaled);                          // fistp, truncating
#else
        const int whole = static_cast<int>(scaled);                          // fistp, truncating
#endif
        const float fraction = scaled - static_cast<float>(whole);
        const float value = interpolatedNoise(whole, fraction);
        total = static_cast<float>(static_cast<double>(value) *
                                   std::pow(static_cast<double>(persistence),
                                            static_cast<double>(i))) + total;
#endif
    }
    return total;
}

// 0x1009F5A0 (`retn 10h`) - only the one-dimensional path exists: any non-zero +0x14 logs "Not
// implemented" (line 106) and answers 0. y, z and w are never read.
float PerlinNoise::sample(float x, float /*y*/, float /*z*/, float /*w*/)
{
    if (m_dimension != 0)
    {
        ::renddx9abi::logf("PerlinNoise.cpp:106: Not implemented");
        return 0.0f;
    }
    return octaveSum(x);
}


// RefFactory 0x1007CEF0: refuse aggregation, 0x30 bytes, construct, QI with no null test on the
// object (shipped), release on refusal.

IBase* __fastcall createPerlinNoise(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(PerlinNoise));    // 0x30 on x86
    PerlinNoise* self = 0;
    if (storage)
        self = new (storage) PerlinNoise();

    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));    // [eax+0Ch]
    if (!result)
        self->release();                                                     // [edx+8]
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
