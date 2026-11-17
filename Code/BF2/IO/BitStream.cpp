// Bit-level serialization and compressed vector and quaternion codecs.

#include "BitStream.h"
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h" // [CHOSEN, not recovered] x86 conversion results on both Mac architectures.
#endif
#include <math.h>       // sqrtf, atan2f, acosf, sinf, cosf, fabsf — the vector and normal encoders
#include <cstring>      // std::memcpy — setData's payload copy
#include "../Core/Debug.h"   // resetWritePosition's attested guard line

namespace dice {
namespace hfe {
namespace io {

// Attaches a caller-owned buffer of sizeBytes and clears stream state.
// Buffer capacity is measured in bytes; read/write positions and limits are measured in bits.
void BitStream::init(unsigned char* buf, unsigned int sizeBytes)
{
    m_buffer    = buf;          // +0x00
    m_bitLimit  = 0;            // +0x08 — zero means "no limit"; writeBits skips the test when 0
    m_sizeBytes = sizeBytes;    // +0x04

    clear();
}

// Wraps sizeBytes of caller-owned memory without taking ownership.
BitStream::BitStream(unsigned char* buf, unsigned int sizeBytes)
{
    m_compressionVector[0] = 0.0f;      // +0x54..+0x5C zeroed by the ctor
    m_compressionVector[1] = 0.0f;
    m_compressionVector[2] = 0.0f;
    m_ownsBuffer = false;               // +0x0C - this stream borrows the caller's memory
    init(buf, sizeBytes);
}

// Allocates and owns a buffer of sizeBytes bytes.
BitStream::BitStream(unsigned int sizeBytes)
{
    m_compressionVector[0] = 0.0f;      // +0x54..+0x5C, zeroed first
    m_compressionVector[1] = 0.0f;
    m_compressionVector[2] = 0.0f;

    m_buffer     = new unsigned char[sizeBytes];   // +0x00  operator new[](sizeBytes)
    m_sizeBytes  = sizeBytes;                      // +0x04  init() sets both again
    m_ownsBuffer = true;                           // +0x0C  we allocated it, so we free it

    init(m_buffer, sizeBytes);
}

// Releases the buffer only when this stream owns it.
BitStream::~BitStream()
{
    if (m_ownsBuffer)
    {
        if (m_buffer)
            delete[] m_buffer;          // operator delete[] - the tail jump at 0x84309A8
    }
}

// Restores a saved position within the written portion of the stream.
// The current mode selects the cursor to move, then the saved mode is installed.
// Returns false without changing state when the position exceeds the high-water mark.
bool BitStream::setPosition(BSPosition& position)
{
    if (position.m_position > m_highWaterBits)          // +0x18
        return false;

    if (m_readWriteSwitch == 1)                         // +0x34
        m_readBitPos = position.m_position;             // +0x10
    else
        m_bitPos = position.m_position;                 // +0x14

    m_readWriteSwitch = position.m_readWriteSwitch;     // +0x34 <- mark +0x0C
    return true;
}

// Saves the current cursor for resetWritePosition, then seeks to position.
// Returns the seek result; the reset mark is saved even when that seek fails.
bool BitStream::setWritePosition(BSPosition& position)
{
    m_canResetWritePosition = true;                     // +0x50 - resetWritePosition's precondition

    if (m_readWriteSwitch == 1)                         // read mode: the live cursor is m_readBitPos
    {
        m_readWriteSwitch2 = 1;                         // +0x30 (a literal 1 in the ELF)
        m_writePosition = m_readBitPos;                 // +0x28
    }
    else                                               // write mode: the live cursor is m_bitPos
    {
        m_readWriteSwitch2 = m_readWriteSwitch;         // +0x30
        m_writePosition = m_bitPos;                     // +0x28
    }

    return setPosition(position);                       // then move to the caller's mark
}

// Returns to the cursor saved by setWritePosition. Requires an outstanding saved position.
void BitStream::resetWritePosition()
{
    if (!m_canResetWritePosition)                       // +0x50
    {
        Debug(DEBUGTYPE_6, "IO/BitStream.inl", 341)
            << "Call to resetWritePosition without prior call to setPosition";
        return;
    }
    m_canResetWritePosition = false;
#if defined(_WIN32)
    setPosition(*reinterpret_cast<BSPosition*>(&m_dword24));   // +0x24 block, BSPosition-shaped
#else
    // [CHOSEN, not recovered] POSIX: materialize the saved fields without a typed overlay.
    // The two unknown BSPosition words are not read by setPosition.
    BSPosition saved{};
    saved.m_position = m_writePosition;
    saved.m_readWriteSwitch = m_readWriteSwitch2;
    setPosition(saved);
#endif
}

// Copies n bytes into the existing buffer and makes them readable.
// Returns false when the buffer is missing or too small.
bool BitStream::setData(unsigned char* src, unsigned int n)
{
    if (!m_buffer)                    // +0x00 - nothing to copy into
        return false;
    if (n > m_sizeBytes)              // +0x04 - would overrun the buffer
        return false;

    clear();                          // reset cursors, mode and the save block
    m_bitLimit = 0;                   // +0x08 - explicit, after clear(): 0 means unlimited
    std::memcpy(m_buffer, src, n);
    setBitSize(n * 8);                // n bytes -> bits
    return true;
}

bool BitStream::setBuffer(unsigned char* buf, unsigned int sizeBytes)
{
    init(buf, sizeBytes);
    setBitSize(sizeBytes * 8);

    return true;                // `mov eax, 1` before the epilogue
}

// Stores the three components then raises the flag.

// This is the reference point for relative compression: when set, the compressed-vector primitives
// encode deltas against it. Note this is a per-stream reference, whereas the enable
// (`m_relativeCompressionEnabled`) is a class static and therefore process-wide — two different
// scopes for one feature, which is easy to conflate.
void BitStream::setCompressionVector(const Vec3& v)
{
    m_compressionVector[0] = v.x;   // +0x54
    m_compressionVector[1] = v.y;   // +0x58
    m_compressionVector[2] = v.z;   // +0x5C
    m_hasCompressionVector = true;  // +0x60
}

// The exact inverse — zeroes the vector and lowers the flag.

// Both are needed: code elsewhere tests +0x60 rather than comparing the vector against zero, so
// clearing only the components would leave relative compression armed against the origin.
void BitStream::resetCompressionVector()
{
    m_compressionVector[0] = 0.0f;
    m_compressionVector[1] = 0.0f;
    m_compressionVector[2] = 0.0f;
    m_hasCompressionVector = false;
}


// Writes the requested bits and raises the high-water mark.
// Zero bits is a failure; a zero bit limit means no additional limit.

bool BitStream::writeBits(const void* src, unsigned int nbits)
{
    // zero bits is A failure, not A successful no-OP. `test ecx,ecx / jz` falls into the same
    // `xor eax,eax` return as the error path.
    if (nbits == 0)
        return false;

    // The error flag latches. Once set, every later write fails without touching the buffer, and
    // nothing in this function clears it.
    if (m_error)
        return false;

    const unsigned int end = m_bitPos + nbits;

    // two independent capacity tests; either one overflows.
    // m_bitLimit == 0 means unlimited and is skipped, not treated as zero capacity.
    if (end > (m_sizeBytes << 3) || (m_bitLimit != 0 && end > m_bitLimit))
    {
        m_error     = true;
        m_errorCode = 1;
        return false;
    }

    const unsigned char* in = static_cast<const unsigned char*>(src);

    unsigned char*       firstPtr = m_buffer + (m_bitPos >> 3);
    unsigned char* const lastPtr  = m_buffer + ((end - 1) >> 3);
    const unsigned int   startBit = m_bitPos & 7;

    // captured before anything is written. The middle loop writes whole bytes and overruns the end
    // of the field inside the final byte; the tail restores those neighbouring bits from this copy.
    const unsigned char origLast = *lastPtr;

    const unsigned char firstMask = (unsigned char)(0xFF >> (8 - startBit));
    const unsigned char lastMask  = (unsigned char)~(0xFF >> ((8 - end) & 7));

    // First byte: preserve the low bits already present, or in the source shifted up. LSB-first.
    unsigned char prev = *in++;
    *firstPtr = (unsigned char)((firstMask & *firstPtr) | (prev << startBit));
    ++firstPtr;

    // Middle bytes: each output byte is the previous source byte's high bits followed by the current
    // source byte's low bits — the classic unaligned bit blit.
    while (firstPtr <= lastPtr)
    {
#if defined(_WIN32)
        const unsigned char cur = *in++;
#else
        // [CHOSEN, not recovered] POSIX: no speculative source byte beyond the field.
        const unsigned char cur = in < static_cast<const unsigned char*>(src) + (nbits + 7u) / 8u
            ? *in++ : 0;
#endif
        *firstPtr = (unsigned char)((cur << startBit) | (prev >> (8 - startBit)));
        ++firstPtr;
        prev = cur;
    }

    // not optional. Put back the bits of the last byte that lie beyond the field. Omitting this
    // corrupts whatever was already packed after the field — invisible until two fields share a
    // byte, which is most of them.
    *lastPtr = (unsigned char)((~lastMask & *lastPtr) | (lastMask & origLast));

    m_bitPos = end;
    if (end > m_highWaterBits)      // +0x18 — raised only, never lowered here
        m_highWaterBits = end;

    return true;
}


// Reads nbits into dst in the same bit order used by writeBits.

bool BitStream::readBits(void* dst, unsigned int nbits)
{
    // Both guards fail the same way as writeBits' zero-length guard.
    if (dst == 0)
        return false;
    if (nbits == 0)
        return false;

    const unsigned int end = m_readBitPos + nbits;

    // Reads cannot pass the written high-water mark. Error code 2 denotes a read overrun.
    if (end > m_highWaterBits)
    {
        m_error     = true;
        m_errorCode = 2;
        return false;
    }

    const unsigned char* in  = m_buffer + (m_readBitPos >> 3);
    unsigned char*       out = static_cast<unsigned char*>(dst);

    const unsigned int bitOffset = m_readBitPos & 7;
    const unsigned int upShift   = 8 - bitOffset;

    unsigned int  remaining = nbits;
    unsigned char prev      = *in++;

    // Whole output bytes.
    while (remaining > 7)
    {
#if defined(_WIN32)
        const unsigned char cur = *in++;
#else
        // [CHOSEN, not recovered] POSIX buffers need no extra byte after the last field.
        const unsigned char cur = in < m_buffer + (end + 7u) / 8u ? *in++ : 0;
#endif
        *out++ = (unsigned char)((cur << upShift) | (prev >> bitOffset));
        remaining -= 8;
        prev = cur;
    }

    // The trailing partial byte, masked to exactly `remaining` bits.
    // The writer has no matching mask — it protects the destination differently, by restoring the
    // last byte's outside bits from a pre-loop copy. Both are needed; neither is redundant.
    if (remaining > 0)
    {
#if defined(_WIN32)
        const unsigned char cur = *in;
#else
        const unsigned char cur = in < m_buffer + (end + 7u) / 8u ? *in : 0;
#endif
        const unsigned char v   = (unsigned char)((cur << upShift) | (prev >> bitOffset));
        *out = (unsigned char)(v & ((1u << remaining) - 1u));
    }

    m_readBitPos = end;     // +0x10, the read cursor — writeBits never touches it
    return true;
}

// Encodes vectors relative to the compression reference using sign-magnitude quantization.
// Selector zero stores absolute uncompressed floats.

void BitStream::writeCompressedVector(const Vec3& v, const Vec3& ref, float scale)
{
    writeCompressedVectorWithTable(v, ref, scale, m_compressionVectorBitTable);
}

// The shared body. Split out only after a normalised instruction diff proved the CompressedVector
// and HighCompressedVector forms are the same code over different tables — not because they looked
// alike. dice emitted two full copies; this factoring is a rebuild convenience, not recovered
// structure.
void BitStream::writeCompressedVectorWithTable(const Vec3& v, const Vec3& ref, float scale,
                                               const unsigned int* table)
{
    // reciprocal, computed once. `d * (1/s)` != `d / s` in the last bit, and the result is
    // truncated — so the difference reaches the wire.
    const float invScale = 1.0f / scale;

    const float dx = v.x - ref.x;
    const float dy = v.y - ref.y;
    const float dz = v.z - ref.z;

    // Select compression width from the vector length, keeping one selector for all components.
    const float scaledLen = sqrtf(dx*dx + dy*dy + dz*dz) * invScale;

    unsigned int selector = 0;
    if (m_relativeCompressionEnabled)
    {
        // smallest width first, first fit wins
        if      ((float)(1 << (table[3] - 1)) > scaledLen) selector = 3;
        else if ((float)(1 << (table[2] - 1)) > scaledLen) selector = 2;
        else if ((float)(1 << (table[1] - 1)) > scaledLen) selector = 1;
        else                                                                     selector = 0;
    }
    // else: relative compression off forces selector 0 (0x8432D17 is `xor ebx,ebx; jmp`)

    writeBits(&selector, 2);

    if (selector == 0)
    {
        // absolute, not delta. The writer pushes the original v here, never the computed delta.
        writeBits(&v.x, 32);
        writeBits(&v.y, 32);
        writeBits(&v.z, 32);
        return;
    }

    const unsigned int width = table[selector];
    const float        d[3]  = { dx, dy, dz };

    for (int i = 0; i < 3; ++i)
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        int q = bf2x87::toIntTruncate(d[i] * invScale);
#else
        int q = (int)(d[i] * invScale);          // cvttss2si — truncates toward zero
#endif
        unsigned int sign = 0;
        if (q < 0) { sign = 1; q = -q; }
        // Plain sign-magnitude: the same width either way, and negative zero is representable.
        writeBits(&sign, 1);
        writeBits(&q, width - 1);
    }
}

Vec3 BitStream::readCompressedVector(const Vec3& ref, float scale)
{
    return readCompressedVectorWithTable(ref, scale, m_compressionVectorBitTable);
}

Vec3 BitStream::readCompressedVectorWithTable(const Vec3& ref, float scale,
                                              const unsigned int* table)
{
    Vec3 out;
    out.x = 0.0f; out.y = 0.0f; out.z = 0.0f;

    unsigned int selector = 0;
    readBits(&selector, 2);

    if (selector == 0)
    {
        // The reader zeroes the output first, then reads straight into it, and skips the
        // reconstruction entirely — no scale, no reference.
        readBits(&out.x, 32);
        readBits(&out.y, 32);
        readBits(&out.z, 32);
        return out;
    }

    const unsigned int width = table[selector];

    float q[3];
    for (int i = 0; i < 3; ++i)
    {
        unsigned int sign = 0;
        readBits(&sign, 1);
        int mag = 0;
        readBits(&mag, width - 1);       // same width whether the sign bit was 0 or 1
        q[i] = (float)(sign ? -mag : mag);
    }

    // the reader uses `scale` directly — no reciprocal. The asymmetry with the writer is real and
    // must be reproduced.
    out.x = q[0] * scale + ref.x;
    out.y = q[1] * scale + ref.y;
    out.z = q[2] * scale + ref.z;
    return out;
}

// Uses the compressed-vector format with the shorter high-compression width table.

void BitStream::writeHighCompressedVector(const Vec3& v, const Vec3& ref, float scale)
{
    writeCompressedVectorWithTable(v, ref, scale, m_highCompressionVectorBitTable);
}

Vec3 BitStream::readHighCompressedVector(const Vec3& ref, float scale)
{
    return readCompressedVectorWithTable(ref, scale, m_highCompressionVectorBitTable);
}

// Writes vector deltas using the second compression format. Reader and writer must use the same reference and scale.

void BitStream::writeCompressedVector2(const Vec3& v, const Vec3& ref, float scale)
{
    const float invScale = 1.0f / scale;

    const float dx = v.x - ref.x;
    const float dy = v.y - ref.y;
    const float dz = v.z - ref.z;
    const float len = sqrtf(dx*dx + dy*dy + dz*dz);

    // Selector seven encodes a vector shorter than FLT_EPSILON with no component payload.
    if (1.1920928955078125e-07f > len)          // flt_8888D4C == FLT_EPSILON
    {
        unsigned int zeroSel = 7;
        writeBits(&zeroSel, 3);
        return;
    }

    const float scaledLen = len * invScale;

    // difference 2 — this tier does not consult m_relativeCompressionEnabled. The epsilon test
    // above occupies the place the other two tiers use for that switch, so toggling the static
    // changes CompressedVector and HighCompressedVector but leaves this one alone.
    unsigned int selector = 0;
    for (int i = 6; i >= 1; --i)                // smallest width first, first fit wins
    {
        if ((float)(1 << (m_compressionVectorBitTable2[i] - 1)) > scaledLen) { selector = (unsigned int)i; break; }
    }

    writeBits(&selector, 3);                    // difference 3 — three bits, not two

    if (selector == 0)
    {
        // Same absolute fallback as the other tiers: three raw 32-BIT floats of the original v.
        // note table2[0] is 28, not 32 — but it is only a sentinel and is never used as a width.
        // Reading it as one would emit 28-bit fields that nothing decodes.
        writeBits(&v.x, 32);
        writeBits(&v.y, 32);
        writeBits(&v.z, 32);
        return;
    }

    const unsigned int width = m_compressionVectorBitTable2[selector];
    const float        d[3]  = { dx, dy, dz };
    for (int i = 0; i < 3; ++i)
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        int q = bf2x87::toIntTruncate(d[i] * invScale);
#else
        int q = (int)(d[i] * invScale);
#endif
        unsigned int sign = 0;
        if (q < 0) { sign = 1; q = -q; }
        writeBits(&sign, 1);
        writeBits(&q, width - 1);
    }
}

Vec3 BitStream::readCompressedVector2(const Vec3& ref, float scale)
{
    Vec3 out;
    out.x = 0.0f; out.y = 0.0f; out.z = 0.0f;

    unsigned int selector = 0;
    readBits(&selector, 3);

    // The zero sentinel: nothing follows it on the wire, and the delta was below FLT_EPSILON, so the
    // result is the reference itself.
    if (selector == 7)
    {
        out = ref;
        return out;
    }

    if (selector == 0)
    {
        readBits(&out.x, 32);
        readBits(&out.y, 32);
        readBits(&out.z, 32);
        return out;
    }

    const unsigned int width = m_compressionVectorBitTable2[selector];

    float q[3];
    for (int i = 0; i < 3; ++i)
    {
        unsigned int sign = 0;
        readBits(&sign, 1);
        int mag = 0;
        readBits(&mag, width - 1);
        q[i] = (float)(sign ? -mag : mag);
    }

    out.x = q[0] * scale + ref.x;
    out.y = q[1] * scale + ref.y;
    out.z = q[2] * scale + ref.z;
    return out;
}

// Encodes a unit vector as two spherical angles using nbits per angle.

void BitStream::writeNormalVector(const Vec3& v, int nbits)
{
    const float azimuth = atan2f(v.y, v.x);

    // z is clamped into [-1,1] before acosf — a normal fractionally out of unit length would
    // otherwise return NaN. An out-of-range z is clamped, not rejected.
    float z = v.z;
    if (z >  1.0f) z =  1.0f;
    if (z < -1.0f) z = -1.0f;
    const float elevation = acosf(z);

    // An invalid normal sets error code 4 without writing any bits.
    if (fabsf(azimuth) > 3.1415927410125732f || fabsf(elevation) > 3.1415927410125732f)
    {
        m_error     = true;
        m_errorCode = 4;
        return;
    }

    writeNormalAngle(azimuth,   nbits);
    writeNormalAngle(elevation, nbits);
}

// The per-angle encoder, identical for both angles in the binary.
void BitStream::writeNormalAngle(float angle, int nbits)
{
    const float normalised = angle / 3.1415927410125732f;   // -> [-1, 1]

    // nbits == 32 is A different encoding, not a wider one: the RAW float of angle/PI goes on the
    // wire, skipping both the remap and the quantisation. It sidesteps the (1<<32)-1 overflow rather
    // than working around it.
    if (nbits == 32)
    {
        writeBits(&normalised, 32);
        return;
    }

    float unit = (normalised + 1.0f) * 0.5f;                // -> [0, 1]
    if (unit < 0.0f) unit = 0.0f;                           // clamped from below only

    // The nonnegative angle mapping uses the full width for magnitude, with truncating conversion.
    const unsigned int max = (1u << nbits) - 1u;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    unsigned int q = static_cast<unsigned int>(bf2x87::toIntTruncate(unit * (float)max));
#else
    unsigned int q = (unsigned int)(unit * (float)max);
#endif
    writeBits(&q, nbits);
}

Vec3 BitStream::readNormalVector(int nbits)
{
    // Round both angles to float before trigonometry so x86 extended precision does not alter decoded values.
    const volatile float azimuth   = readNormalAngle(nbits);
    const volatile float elevation = readNormalAngle(nbits);

    // The exact inverse of atan2f(y,x) / acosf(z): elevation is measured from +Z.
    Vec3 out;
#if defined(_M_X64)
    // [CHOSEN, not recovered] Native-port compatibility uses double trigonometry on rounded float inputs,
    // then rounds each result to match the x86 evaluation.
    out.x = static_cast<float>(cos(static_cast<double>(azimuth))) *
            static_cast<float>(sin(static_cast<double>(elevation)));
    out.y = static_cast<float>(sin(static_cast<double>(azimuth))) *
            static_cast<float>(sin(static_cast<double>(elevation)));
    out.z = static_cast<float>(cos(static_cast<double>(elevation)));
#else
    out.x = cosf(azimuth) * sinf(elevation);
    out.y = sinf(azimuth) * sinf(elevation);
    out.z = cosf(elevation);
#endif
    return out;
}

float BitStream::readNormalAngle(int nbits)
{
    float value;

    if (nbits == 32)
    {
        value = 0.0f;
        readBits(&value, 32);                 // the raw float written by the 32-bit writer path
    }
    else
    {
        const unsigned int max = (1u << nbits) - 1u;
        unsigned int q = 0;
        readBits(&q, nbits);
        // The reader divides by max where the writer multiplied by it — the same reciprocal-vs-
        // direct asymmetry as the CompressedVector pair. Reproduce both literally.
        value = ((float)q / (float)max) * 2.0f - 1.0f;
    }

    // Snap decoded values within one quantization step of zero to zero to compensate for rounding.
    const unsigned int maxForQuantum = (nbits == 32) ? 0xFFFFFFFFu : ((1u << nbits) - 1u);
    const float quantum = 2.0f / (float)maxForQuantum;
    if (quantum >= fabsf(value))
        value = 0.0f;

    return value * 3.1415927410125732f;
}

// Encodes three quaternion components and the remaining component's sign.
// Out-of-range components trigger normalization before quantization.

void BitStream::writeUnitQuaternion3Comp(const Quat& q, int nbits)
{
    Quat v = q;

    // an out-of-range component is not an error — it routes to renormalisation. No error code is
    // set anywhere in this function.
    if (fabsf(v.x) > 1.0f || fabsf(v.y) > 1.0f || fabsf(v.z) > 1.0f)
    {
        // Normalization includes all four components even though only three are transmitted.
        const float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z + v.w*v.w);
        if (len < 1.1920928955078125e-07f)          // FLT_EPSILON — a degenerate quaternion
        {
            v.x = 0.0f; v.y = 0.0f; v.z = 0.0f; v.w = 1.0f;   // becomes the identity, not zero
        }
        else
        {
            v.x /= len; v.y /= len; v.z /= len; v.w /= len;
        }
    }

    const unsigned int sign = (v.w < 0.0f) ? 1u : 0u;   // the sign of W, written first
    writeBits(&sign, 1);

    writeQuatComponent(v.x, nbits);
    writeQuatComponent(v.y, nbits);
    writeQuatComponent(v.z, nbits);
}

// The per-component scalar encoder. Identical to writeNormalAngle's body apart from taking a value
// already in [-1,1] rather than an angle — the binary shares this quantiser between the normal and
// quaternion encoders.
void BitStream::writeQuatComponent(float v, int nbits)
{
    if (nbits == 32) { writeBits(&v, 32); return; }     // raw float, same as the other primitives

    float unit = (v + 1.0f) * 0.5f;
    if (unit < 0.0f) unit = 0.0f;

    const unsigned int max = (1u << nbits) - 1u;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    unsigned int qv = static_cast<unsigned int>(bf2x87::toIntTruncate(unit * (float)max));
#else
    unsigned int qv = (unsigned int)(unit * (float)max);   // fnstcw/or 0xC00/fistp == a truncating cast
#endif
    writeBits(&qv, nbits);
}

Quat BitStream::readUnitQuaternion3Comp(int nbits)
{
    // the identity, not zero. Written before anything is read, so every early exit yields a valid
    // rotation. Zero-initialising gives a zero quaternion, which normalises to NaN.
    Quat out;
    out.x = 0.0f; out.y = 0.0f; out.z = 0.0f; out.w = 1.0f;

    unsigned int sign = 0;
    readBits(&sign, 1);                 // read first, before any component

    out.x = readQuatComponent(nbits);
    out.y = readQuatComponent(nbits);
    out.z = readQuatComponent(nbits);

    float s = out.x*out.x + out.y*out.y + out.z*out.z;

    // clamped before the sqrt. Quantisation can push the sum fractionally above 1, and sqrt of a
    // negative is NaN. `fcmovbe` pins it at 1.0, so the worst case is w == 0.
    if (1.0f <= s) s = 1.0f;

    out.w = sqrtf(1.0f - s);
    if (sign) out.w = -out.w;
    return out;
}

float BitStream::readQuatComponent(int nbits)
{
    float value;
    if (nbits == 32) { value = 0.0f; readBits(&value, 32); }
    else
    {
        const unsigned int max = (1u << nbits) - 1u;
        unsigned int qv = 0;
        readBits(&qv, nbits);
        value = ((float)qv / (float)max) * 2.0f - 1.0f;
    }

    // The same snap-to-zero dead zone as readNormalAngle — one quantum wide, decoder-only.
    const unsigned int maxForQuantum = (nbits == 32) ? 0xFFFFFFFFu : ((1u << nbits) - 1u);
    const float quantum = 2.0f / (float)maxForQuantum;
    if (quantum >= fabsf(value)) value = 0.0f;
    return value;
}

// Encodes all four quaternion components in W, X, Y, Z order.
// The writer normalizes only when an X, Y or Z component exceeds unit magnitude; the reader always normalizes.
namespace {

// not BitStream::writeQuatComponent: this clamps the TOP end as well, because writeUnitQuaternion's
// in-range path forwards W unclamped.
void writeUnitQuatComponent(BitStream& s, float v, int nbits)
{
    if (nbits == 32) { s.writeBits(&v, 32); return; }        // raw float, like every other primitive

    float unit = (v + 1.0f) * 0.5f;                          // [-1,1] -> [0,1]
    if (unit < 0.0f)      unit = 0.0f;                        // both ends clamped
    else if (unit > 1.0f) unit = 1.0f;

    const unsigned int max = (1u << nbits) - 1u;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const unsigned int qv = static_cast<unsigned int>(bf2x87::toIntTruncate(unit * (float)max));
#else
    const unsigned int qv = (unsigned int)(unit * (float)max);   // truncating cast (fnstcw/0xC00/fistp)
#endif
    s.writeBits(&qv, nbits);
}

float readUnitQuatComponent(BitStream& s, int nbits)
{
    if (nbits == 32) { float f = 0.0f; s.readBits(&f, 32); return f; }

    const unsigned int max = (1u << nbits) - 1u;
    unsigned int qv = 0;
    s.readBits(&qv, nbits);
    return ((float)qv / (float)max) * 2.0f - 1.0f;           // reciprocal decode, mirrors the encode
}

} // anonymous namespace

void BitStream::writeUnitQuaternion(const Quat& q, int nbits)
{
    // an out-of-range X, Y or Z is not an error - it routes to renormalisation. W is not tested.
    if (fabsf(q.x) > 1.0f || fabsf(q.y) > 1.0f || fabsf(q.z) > 1.0f)
    {
        Quat v = q;
        // Normalization includes all four components although only x, y and z trigger it.
        const float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z + v.w*v.w);
        if (len >= 1.1920928955078125e-07f)          // FLT_EPSILON - a valid quaternion
        {
            v.w /= len; v.x /= len; v.y /= len; v.z /= len;
        }
        else
        {
            v.x = 0.0f; v.y = 0.0f; v.z = 0.0f; v.w = 1.0f;   // degenerate -> identity, not zero
        }
        writeUnitQuatComponent(*this, v.w, nbits);   // W, X, Y, Z
        writeUnitQuatComponent(*this, v.x, nbits);
        writeUnitQuatComponent(*this, v.y, nbits);
        writeUnitQuatComponent(*this, v.z, nbits);
        return;
    }

    // In range: send the RAW components, no renormalisation.
    writeUnitQuatComponent(*this, q.w, nbits);       // W, X, Y, Z
    writeUnitQuatComponent(*this, q.x, nbits);
    writeUnitQuatComponent(*this, q.y, nbits);
    writeUnitQuatComponent(*this, q.z, nbits);
}

Quat BitStream::readUnitQuaternion(int nbits)
{
    // the identity, not zero - so a degenerate read still yields a valid rotation.
    Quat out;
    out.x = 0.0f; out.y = 0.0f; out.z = 0.0f; out.w = 1.0f;

    out.w = readUnitQuatComponent(*this, nbits);     // W, X, Y, Z - the writer's order
    out.x = readUnitQuatComponent(*this, nbits);
    out.y = readUnitQuatComponent(*this, nbits);
    out.z = readUnitQuatComponent(*this, nbits);

    // renormalise what was assembled - this subsumes the per-component dead zone the 3Comp reader has.
    const float len = sqrtf(out.w*out.w + out.x*out.x + out.y*out.y + out.z*out.z);
    if (len >= 1.1920928955078125e-07f)              // FLT_EPSILON
    {
        out.w /= len; out.x /= len; out.y /= len; out.z /= len;
    }
    else
    {
        out.x = 0.0f; out.y = 0.0f; out.z = 0.0f; out.w = 1.0f;   // degenerate -> identity
    }
    return out;
}

// Returns a quantized approximation of v at nbits without accessing the stream.
// Clamps out-of-range angles and does not apply the reader's snap-to-zero dead zone.
Vec3 BitStream::shrinkNormalVector(const Vec3& v, int nbits)
{
    const float PI = 3.1415927410125732f;
    const int   max = (nbits != 32) ? ((1 << nbits) - 1) : -1;   // -1 at 32, as the ELF (dead edge)

    // Clamp a normalised angle to [-1,1], remap to [0,1] (clamped both ends), truncate-quantise to the
    // grid, then decode straight back to an angle. No dead zone - see the header note above.
    auto shrinkAngle = [&](float normalised) -> float
    {
        if (normalised < -1.0f)      normalised = -1.0f;
        else if (normalised > 1.0f)  normalised =  1.0f;
        float unit = (normalised + 1.0f) * 0.5f;
        if (unit < 0.0f)      unit = 0.0f;
        else if (unit > 1.0f) unit = 1.0f;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const float q = static_cast<float>(bf2x87::toIntTruncate(unit * (float)max)) / (float)max;
#else
        const float q = (float)((int)(unit * (float)max)) / (float)max;   // truncate, then dequantise
#endif
        return PI * (q + q - 1.0f);
    };

    const float azimuth = shrinkAngle(atan2f(v.y, v.x) / PI);

    float z = v.z;                                    // clamp z into [-1,1] before acosf, as elsewhere
    if (z < -1.0f)      z = -1.0f;
    else if (z > 1.0f)  z =  1.0f;
    const float elevation = shrinkAngle(acosf(z) / PI);

    Vec3 out;
    out.x = cosf(azimuth) * sinf(elevation);
    out.y = sinf(azimuth) * sinf(elevation);
    out.z = cosf(elevation);
    return out;
}

// Compression tables use index zero as the uncompressed sentinel, not as a component width.

bool BitStream::m_relativeCompressionEnabled = true;

unsigned int BitStream::m_compressionVectorBitTable[4]     = { 32, 20, 16, 12 };
unsigned int BitStream::m_highCompressionVectorBitTable[4] = { 32, 12, 10,  8 };
unsigned int BitStream::m_compressionVectorBitTable2[7]    = { 28, 24, 20, 16, 12, 10, 8 };


// Sets the number of readable bits. An oversized request asserts and leaves the high-water mark unchanged.
void BitStream::setBitSize(unsigned int size)
{
    if (size > (m_sizeBytes << 3))
    {
        // assert(...) in the original — reports and returns without storing.
        return;
    }
    m_highWaterBits = size;
}


// Advances the current cursor without touching the buffer. Returns false for zero bits or an exceeded bound.
// Reads are limited by written data; writes by capacity and any bit limit. Skipped write bits are not cleared.
bool BitStream::skipBits(unsigned int bits)
{
    if (bits == 0)
        return false;

    if (m_readWriteSwitch == 1)                 // +0x34 — read mode
    {
        const unsigned int end = bits + m_readBitPos;
        if (end > m_highWaterBits)
        {
            m_error     = true;
            m_errorCode = 2;                    // read overrun
            return false;
        }
        m_readBitPos = end;
        return true;
    }

    const unsigned int end = bits + m_bitPos;
    if (end > (m_sizeBytes << 3) || (m_bitLimit != 0 && end > m_bitLimit))
    {
        m_error     = true;
        m_errorCode = 1;                        // write overrun
        return false;
    }

    const bool withinHighWater = (end <= m_highWaterBits);
    m_bitPos = end;
    if (!withinHighWater)
        m_highWaterBits = end;

    return true;
}


// Clears cursors and error state while preserving the attached buffer, capacity and bit limit.
void BitStream::clear()
{
    m_readBitPos     = 0;       // +0x10
    m_bitPos         = 0;       // +0x14
    m_highWaterBits  = 0;       // +0x18
    m_error          = false;   // +0x1C
    m_errorCode      = 0;       // +0x20

    m_compressionVector[0] = 0.0f;   // +0x54
    m_compressionVector[1] = 0.0f;   // +0x58
    m_compressionVector[2] = 0.0f;   // +0x5C
    m_hasCompressionVector = false;  // +0x60

    // Clear read/write mode and saved-position state.
    m_readWriteSwitch       = 0;     // +0x34
    m_byte38                = false; // +0x38
    m_canResetWritePosition = false; // +0x50
    m_dword24               = 0;     // +0x24
    m_writePosition         = 0;     // +0x28
    m_byte2C                = false; // +0x2C
}

} // namespace io
} // namespace hfe
} // namespace dice
