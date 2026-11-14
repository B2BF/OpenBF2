// [INFERRED] private split of the attested ParticleSystemManager.cpp renderer implementation.
// BF2 1.50 RendDX9 PE instruction streams read in full; no dedicated ELF renderer counterpart.
// Included inside the manager's anonymous namespace after psmDrawField.

// PE 1016F580 uses the current x87 rounding mode, not C++ truncation.
static int psmParticleRound(float value)
{
#if defined(_WIN32)
#if defined(_M_IX86)
    int rounded;
    __asm {
        fld value
        frndint
        fistp rounded
    }
    return rounded;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates). cvtss2si rounds in the
    // current (nearest-even) mode and yields 0x80000000 for NaN or out of range, like fistp.
    return _mm_cvtss_si32(_mm_set_ss(value));
#endif
#else
// [CHOSEN, not recovered] Preserve the S2.1 POSIX numeric arm (docs/macos-port.md).
    int rounded;
    // [CHOSEN, not recovered] Preserve x87 current-mode FRNDINT and integer indefinite.
    rounded = bf2x87::integer<int>(bf2x87::wide(value), false);
    return rounded;
#endif
}

// PE 1000ED40, also used by the clamped acos helper 100A6050.
static float psmParticleClamp(float value, float low, float high)
{
    if (value >= high) value = high;
    // fcmovb also selects low for an unordered comparison.
    if (!(value >= low)) value = low;
    return value;
}

// PE 0x100A6050
static double psmParticleAcos(float value)
{
    return std::acos(psmParticleClamp(value, -1.0f, 1.0f));
}

// Four stable byte-radix passes on the nonnegative squared-distance bits.
// The bucket chains use the particle's first word and are emptied on each pass.
static void psmSortParticles(unsigned char* renderer, unsigned char** table, unsigned int count)
{
    unsigned char** buckets = &psmSpriteField<unsigned char*>(renderer, 0x182C);
    for (unsigned int byte = 0; byte < 4; ++byte)
    {
        for (int i = static_cast<int>(count) - 1; i >= 0; --i)
        {
            unsigned char* particle = table[i];
            unsigned int key = particle[0x14 + byte];
            psmRecordLink(particle, 0) = buckets[key];
            buckets[key] = particle;
        }
        unsigned int output = 0;
        for (unsigned int key = 0; key < 256; ++key)
        {
            unsigned char* particle = buckets[key];
            buckets[key] = 0;
            while (particle)
            {
                table[output++] = particle;
                particle = static_cast<unsigned char*>(psmRecordLink(particle, 0));
            }
        }
    }
}

// rotation/flipbook state is advanced once per dirty particle, even when
// multiple render views collect it. The instance argument is present but unused in retail.
// PE 0x101721F0
static void psmUpdateParticleDrawState(unsigned char* renderer, unsigned char* particle,
                                       unsigned char* descriptor, unsigned char*)
{
    unsigned int& flags = psmDrawField<unsigned int>(particle, 0x54);
    if (!(flags & 0x80)) return;
    flags &= ~0x80u;
#if defined(__APPLE__)
    // [CHOSEN, not recovered] Keep x87 intermediates until the attested stores.
    using bf2x87::wide;
    const bf2x87::Real age = wide(psmDrawField<float>(particle, 0x24));
    const float rate = bf2x87::store(age * age * wide(psmDescriptor0<float>(descriptor, 0x100))
        + age * wide(psmDescriptor0<float>(descriptor, 0x104))
        + age * age * age * wide(psmDescriptor0<float>(descriptor, 0xFC))
        + wide(psmDescriptor0<float>(descriptor, 0x108)));
    const float dt = bf2x87::store(wide(psmParticleClamp(rate, 0.0f, 1.0f))
        * wide(psmSpriteField<float>(renderer, 0x1C3C)));
#else
    const double age = psmDrawField<float>(particle, 0x24);
    const float rate = static_cast<float>(age * age * psmDescriptor0<float>(descriptor, 0x100)
        + age * psmDescriptor0<float>(descriptor, 0x104)
        + age * age * age * psmDescriptor0<float>(descriptor, 0xFC)
        + psmDescriptor0<float>(descriptor, 0x108));
    const float dt = psmParticleClamp(rate, 0.0f, 1.0f)
        * psmSpriteField<float>(renderer, 0x1C3C);
#endif
    float& rotation = psmDrawField<float>(particle, 0x80);
    if (!descriptor[0x21C])
        rotation = static_cast<float>(std::fmod(static_cast<double>(dt)
            * psmDrawField<float>(particle, 0x7C) + rotation, 6.283185482025146));
    else
    {
        const float* position = reinterpret_cast<float*>(particle + 0x18);
        const float* velocity = reinterpret_cast<float*>(particle + 0x60);
        const Mat4& matrix = psmSpriteField<Mat4>(renderer, 0x1C98);
#if defined(__APPLE__)
        // [CHOSEN, not recovered] The previous Y/Z, current reciprocal and
        // projected Y remain live; previous X and its reciprocal are spilled.
        const float previousX = bf2x87::store(wide(position[0]) - wide(velocity[0]) * wide(0.001f));
        const bf2x87::Real previousY = wide(position[1]) - wide(velocity[1]) * wide(0.001f);
        const float deltaZ = bf2x87::store(wide(velocity[2]) * wide(0.001f));
        const bf2x87::Real previousZ = wide(position[2]) - wide(deltaZ);
        float projected[2][3];
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 3; ++j)
                projected[i][j] = bf2x87::store(
                    (i ? previousZ : wide(position[2])) * wide(matrix.m[8 + j])
                    + (i ? previousY : wide(position[1])) * wide(matrix.m[4 + j])
                    + wide(i ? previousX : position[0]) * wide(matrix.m[j]) + wide(matrix.m[12 + j]));
        float currentX = 0.0f;
        bf2x87::Real currentY = wide(0.0f), oldX = wide(0.0f), oldY = wide(0.0f);
        if (std::fabs(projected[0][2]) > 0.00000023841858f)
        {
            const bf2x87::Real reciprocal = wide(1.0f) / wide(projected[0][2]);
            currentX = bf2x87::store(wide(projected[0][0]) * reciprocal);
            currentY = reciprocal * wide(projected[0][1]);
        }
        if (std::fabs(projected[1][2]) > 0.00000023841858f)
        {
            const float reciprocal = bf2x87::store(wide(1.0f) / wide(projected[1][2]));
            oldX = wide(projected[1][0]) * wide(reciprocal);
            oldY = wide(projected[1][1]) * wide(reciprocal);
        }
        const float x = bf2x87::store(wide(currentX) - oldX);
        const bf2x87::Real y = currentY - oldY;
        const bf2x87::Real length = bf2x87::squareRoot(wide(x) * wide(x) + y * y);
        if (length > wide(0.00000023841858f))
        {
            const double angle = psmParticleAcos(bf2x87::store(y / length));
            rotation = static_cast<float>(x > 0 ? angle : 6.2831855f - angle);
        }
        else rotation = 0.0f;
#else
        float previous[3];
        for (int i = 0; i < 3; ++i) previous[i] = position[i] - velocity[i] * 0.001f;
        float projected[2][3];
        for (int i = 0; i < 2; ++i)
        {
            const float* p = i ? previous : position;
            for (int j = 0; j < 3; ++j)
                projected[i][j] = static_cast<float>(static_cast<double>(p[2]) * matrix.m[8 + j]
                    + static_cast<double>(p[1]) * matrix.m[4 + j]
                    + static_cast<double>(p[0]) * matrix.m[j] + matrix.m[12 + j]);
            if (std::fabs(projected[i][2]) <= 0.00000023841858f)
                projected[i][0] = projected[i][1] = 0.0f;
            else
            {
                const float reciprocal = 1.0f / projected[i][2];
                projected[i][0] *= reciprocal;
                projected[i][1] *= reciprocal;
            }
        }
        const float x = projected[0][0] - projected[1][0];
        const double y = static_cast<double>(projected[0][1]) - projected[1][1];
        const double length = std::sqrt(static_cast<double>(x) * x + y * y);
        if (std::fabs(length) > 0.00000023841858)
        {
            const double angle = psmParticleAcos(static_cast<float>(y / length));
            rotation = static_cast<float>(x > 0 ? angle : 6.2831855f - angle);
        }
        else rotation = 0.0f;
#endif
    }
    const unsigned int trigIndex = static_cast<unsigned int>(psmParticleRound(rotation * 651.89862f)) & 0xFFF;
    psmDrawField<unsigned int>(particle, 0x88) = trigIndex;
    psmDrawField<unsigned int>(particle, 0x3C) = psmSpriteField<unsigned int>(renderer, 0x1E14 + trigIndex * 4);
    if (descriptor[0xD4] && psmDescriptor0<unsigned int>(descriptor, 0xE0))
    {
        float& frame = psmDrawField<float>(particle, 0x78);
        const double nextFrame = static_cast<double>(dt) * psmDescriptor0<float>(descriptor, 0xD0) + frame;
        frame = static_cast<float>(nextFrame);
        // FST stores the float but floor receives the still-live x87 result as a double.
        const int whole = psmParticleRound(static_cast<float>(std::floor(nextFrame)));
        psmDrawField<float>(particle, 0x30) = static_cast<float>(static_cast<double>(frame) - whole);
        const unsigned int frameCount = psmDescriptor0<unsigned int>(descriptor, 0xE0);
        const unsigned int current = static_cast<unsigned int>(whole) % frameCount;
        if (current != (flags & 0xF))
        {
            flags ^= (current ^ flags) & 0xF;
            const unsigned int next = (current + 1) % frameCount;
            const unsigned int columns = psmDescriptor0<unsigned int>(descriptor, 0xE4);
#if defined(__APPLE__)
            // [CHOSEN, not recovered] PE 10172588 spills only the second row product.
            const unsigned int xs[2] = { current % columns, next % columns };
            const bf2x87::Real ys[2] = {
                wide(current - xs[0]) * wide(psmDescriptor0<float>(descriptor, 0xE8)),
                wide(bf2x87::store(wide(next - xs[1]) * wide(psmDescriptor0<float>(descriptor, 0xE8)))) };
#else
            const unsigned int cells[2] = { current, next };
#endif
            for (unsigned int i = 0; i < 2; ++i)
            {
#if defined(__APPLE__)
                const bf2x87::Real u = (wide(psmDescriptor0<short>(descriptor, 0x21E)) * wide(0.000030518509f)
                    + wide(xs[i]) * wide(psmDescriptor0<float>(descriptor, 0x13C))) * wide(32767.0f);
                const bf2x87::Real v = (wide(psmDescriptor0<short>(descriptor, 0x220)) * wide(0.000030518509f)
                    + ys[i] * wide(psmDescriptor0<float>(descriptor, 0x140))) * wide(32767.0f);
                psmDrawField<short>(particle, 0x40 + i * 4) = bf2x87::lowShort(bf2x87::toInt64Truncate(u));
                psmDrawField<short>(particle, 0x42 + i * 4) = bf2x87::lowShort(bf2x87::toInt64Truncate(v));
#else
                const unsigned int x = cells[i] % columns;
                const double y = i ? static_cast<float>(static_cast<double>(cells[i] - x) * psmDescriptor0<float>(descriptor, 0xE8))
                    : static_cast<double>(cells[i] - x) * psmDescriptor0<float>(descriptor, 0xE8);
                psmDrawField<short>(particle, 0x40 + i * 4) = static_cast<short>(static_cast<__int64>(
                    (static_cast<double>(psmDescriptor0<short>(descriptor, 0x21E)) * 0.000030518509f
                    + static_cast<double>(x) * psmDescriptor0<float>(descriptor, 0x13C)) * 32767.0f));
                psmDrawField<short>(particle, 0x42 + i * 4) = static_cast<short>(static_cast<__int64>(
                    (static_cast<double>(psmDescriptor0<short>(descriptor, 0x220)) * 0.000030518509f
                    + y * psmDescriptor0<float>(descriptor, 0x140)) * 32767.0f));
#endif
            }
        }
    }
}

// Both leading view calls and the view call per accepted particle are retained.
// PE 0x10171A00 (the sorted renderer's collect callback, descriptor +0x2A0).
static void psmCollectSortedParticles(unsigned char* renderer, dx9rend::IRenderView* view,
                                      unsigned char* instance, unsigned char* descriptor)
{
    view->getViewMatrixPtr();
    view->getViewMatrixPtr();
    const float nearPlane = view->getNearPlane();
    const unsigned int count = psmInstanceA<unsigned int>(instance, 0x28);
    if (psmDescriptor0<int>(descriptor, 8) == -1)
        psmDescriptor0<unsigned int>(descriptor, 8) = psmSpriteField<unsigned int>(renderer, 0x1D70)++;
    ParticleSystemManager* manager = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    const unsigned int lod = psmDescriptor0<unsigned int>(descriptor, 0x64);
    const float farDistance = manager->m_farDistances[lod];
    const float inverseFarRange = manager->m_inverseFarRanges[lod];
    for (unsigned int i = 0; i < count; ++i)
    {
        unsigned char* p = psmInstanceA<unsigned char**>(instance, 0x24)[i];
        psmDrawField<unsigned short>(p, 4) = psmDescriptor0<unsigned short>(descriptor, 4);
        // `fst` keeps the float copy, but the near-plane test compares the unrounded sum
        // (0x10171ABC..0x10171AC7); the far test and fromNear reload the float.
        const double depthSum = static_cast<double>(psmDrawField<float>(p, 0x1C)) * psmSpriteField<float>(renderer, 0x1C44)
            + static_cast<double>(psmDrawField<float>(p, 0x20)) * psmSpriteField<float>(renderer, 0x1C48)
            + static_cast<double>(psmDrawField<float>(p, 0x18)) * psmSpriteField<float>(renderer, 0x1C40)
            + psmSpriteField<float>(renderer, 0x1C4C);
        const float depth = static_cast<float>(depthSum);
        if (depthSum < nearPlane) continue;
        float& alpha = psmDrawField<float>(p, 0x2C);
        if (!(depth > farDistance)) alpha = psmDrawField<float>(p, 0x58);
        else
        {
            double fade = (static_cast<double>(depth) - farDistance) * inverseFarRange;
            if (fade > 1) fade = 1;
            alpha = static_cast<float>((1 - fade) * (1 - fade) * psmDrawField<float>(p, 0x58));
        }
        if (alpha < psmDescriptor0<float>(descriptor, 0x9C)) continue;
        const float fromNear = depth - nearPlane;
        if (descriptor[0x29C] && fromNear < psmDescriptor0<float>(descriptor, 0x288)
            && psmDrawField<float>(p, 0x24) > psmDescriptor0<float>(descriptor, 0x290))
        {
            double ageFade = (static_cast<double>(psmDrawField<float>(p, 0x24)) - psmDescriptor0<float>(descriptor, 0x290))
                * psmDescriptor0<float>(descriptor, 0x294);
            ageFade = ageFade > 1 ? 0 : 1 - ageFade;
            const double distanceFade = (static_cast<double>(psmDescriptor0<float>(descriptor, 0x288)) - fromNear)
                * psmDescriptor0<float>(descriptor, 0x28C);
            // d * a + (1 - d): the shipped association (0x10171BB1..0x10171BBD).
            alpha = static_cast<float>(distanceFade > 1 ? ageFade : distanceFade * ageFade + (1 - distanceFade));
            if (alpha < psmDescriptor0<float>(descriptor, 0x298)) continue;
        }
        const Mat4* matrix = view->getViewMatrixPtr();
        const float x = matrix->m[12] - psmDrawField<float>(p, 0x18);
        const double y = static_cast<double>(matrix->m[13]) - psmDrawField<float>(p, 0x1C);
        const double z = static_cast<double>(matrix->m[14]) - psmDrawField<float>(p, 0x20);
        psmDrawField<float>(p, 0x14) = static_cast<float>(z * z + y * y + static_cast<double>(x) * x);
        psmUpdateParticleDrawState(renderer, p, descriptor, instance);
        psmSpriteField<unsigned char**>(renderer, 0x18)[psmSpriteField<unsigned int>(renderer, 0x1C38)++] = p;
    }
    particleRendererList<unsigned char*>(renderer, psmSpriteRendererOffset(8)).push_back(instance);
}

// PE 10170CE0 retains the pre-fade alpha test, omits the sorted-path near fade and
// collects per-instance groups (maximum 256 in each draw-order group).
static void psmCollectAdditiveParticles(unsigned char* renderer, dx9rend::IRenderView* view,
                                        unsigned char* instance, unsigned char* descriptor)
{
    const float* eye = view->getViewMatrixPtr()->m + 12;
    const float nearPlane = view->getNearPlane();
    view->getViewMatrixPtr();
    const unsigned int count = psmInstanceA<unsigned int>(instance, 0x28);
    if (psmDescriptor0<int>(descriptor, 8) == -1)
        psmDescriptor0<unsigned int>(descriptor, 8) = psmSpriteField<unsigned int>(renderer, 0x1D74)++;
    view->getViewport();
    ParticleSystemManager* manager = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    const unsigned int lod = psmDescriptor0<unsigned int>(descriptor, 0x64);
    const float farDistance = manager->m_farDistances[lod];
    const float inverseFarRange = manager->m_inverseFarRanges[lod];
    unsigned int accepted = 0;
    unsigned int& total = psmSpriteField<unsigned int>(renderer, 0x1820);
    unsigned char** table = psmSpriteField<unsigned char**>(renderer, 0x1C);
    for (unsigned int i = 0; i < count; ++i)
    {
        unsigned char* p = psmInstanceA<unsigned char**>(instance, 0x24)[i];
        float& alpha = psmDrawField<float>(p, 0x2C);
        if (alpha < psmDescriptor0<float>(descriptor, 0x9C)) continue;
        const float depth = static_cast<float>(static_cast<double>(psmDrawField<float>(p, 0x20)) * psmSpriteField<float>(renderer, 0x1C48)
            + static_cast<double>(psmDrawField<float>(p, 0x1C)) * psmSpriteField<float>(renderer, 0x1C44)
            + static_cast<double>(psmDrawField<float>(p, 0x18)) * psmSpriteField<float>(renderer, 0x1C40)
            + psmSpriteField<float>(renderer, 0x1C4C));
        if (depth < nearPlane) continue;
        if (!(depth > farDistance)) alpha = psmDrawField<float>(p, 0x58);
        else
        {
            double fade = (static_cast<double>(depth) - farDistance) * inverseFarRange;
            if (fade > 1) fade = 1;
            alpha = static_cast<float>((1 - fade) * (1 - fade) * psmDrawField<float>(p, 0x58));
        }
        const float x = eye[0] - psmDrawField<float>(p, 0x18);
        const double y = static_cast<double>(eye[1]) - psmDrawField<float>(p, 0x1C);
        const double z = static_cast<double>(eye[2]) - psmDrawField<float>(p, 0x20);
        psmDrawField<float>(p, 0x14) = static_cast<float>(z * z + static_cast<double>(x) * x + y * y);
        psmDrawField<unsigned short>(p, 4) = psmDescriptor0<unsigned short>(descriptor, 4);
        psmUpdateParticleDrawState(renderer, p, descriptor, instance);
        table[total + accepted++] = p;
    }
    if (!accepted) return;
    const bool ordered = psmDescriptor0<unsigned int>(descriptor, 0x7C) != 0;
    unsigned int& groupCount = psmSpriteField<unsigned int>(renderer, ordered ? 0x1828 : 0x1824);
    if (groupCount >= 256) return;
    PsmDrawGroup* const group = psmSpriteGroups(renderer, ordered ? 0xC20 : 0x20) + groupCount;
    group->instance = instance;
    group->last = static_cast<int>(total + accepted - 1);
    group->count = accepted;
    ++groupCount;
    psmSortParticles(renderer, table + total, accepted);
    total += accepted;
}
