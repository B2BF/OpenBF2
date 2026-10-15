// Renderer construction helpers included after psmFillTrailIndices in the manager's namespace.
// The renderers keep their recovered byte layout; their pointer lists are std::vectors constructed
// in place (particleRendererList).

template<class T>
static T& psmCtorField(unsigned char* renderer, unsigned int offset)
{
    return *reinterpret_cast<T*>(renderer + offset);
}

static void psmCtorZeroWords(unsigned char* renderer, unsigned int offset, unsigned int count)
{
    for (unsigned int i = 0; i < count; ++i)
        psmCtorField<unsigned int>(renderer, offset + 4u * i) = 0;
}

// A renderer's retail -> native offset map (ParticleRawLayout.h).
typedef unsigned int (*PsmRendererLayout)(unsigned int retail);

template<class T>
static T& psmCtorAt(unsigned char* renderer, PsmRendererLayout layout, unsigned int retail)
{
    return psmRawField<T>(renderer, layout(retail));
}

// Zeroes `count` retail words starting at retail `offset`, each at its native place.
static void psmCtorZeroRetailWords(unsigned char* renderer, PsmRendererLayout layout,
                                   unsigned int offset, unsigned int count)
{
    for (unsigned int i = 0; i < count; ++i)
        psmCtorAt<unsigned int>(renderer, layout, offset + 4u * i) = 0;
}

// [CHOSEN, not recovered] x64: zero the relocated fields (ParticleRawLayout.h) before the retail
// stores, so a pointer slot whose retail word the constructor zeroes is zero in full. The manager's
// storage is value-initialised already; x86 has no tail.
static void psmCtorClearTail(unsigned char* renderer, unsigned int retailSize, unsigned int nativeSize)
{
    for (unsigned int i = retailSize; i < nativeSize; ++i)
        renderer[i] = 0;
}

static void psmCtorIdentity(unsigned char* matrix)
{
    psmCtorZeroWords(matrix, 0, 16);
    for (unsigned int i = 0; i < 4; ++i)
        psmCtorField<float>(matrix, 20u * i) = 1.0f;
}

// three identity matrices followed by one zero Vec4.
static void psmCtorParticleMatrices(unsigned char* matrices)
{
    for (unsigned int i = 0; i < 3; ++i)
        psmCtorIdentity(matrices + 64u * i);
    psmCtorZeroWords(matrices, 0xC0, 4);
}

// four identity matrices.
static void psmCtorMeshMatrices(unsigned char* matrices)
{
    for (unsigned int i = 0; i < 4; ++i)
        psmCtorIdentity(matrices + 64u * i);
}

// Constructs an empty pointer list in its 16-byte span.
template<class T>
static void psmCtorPointerVector(unsigned char* renderer, unsigned int offset)
{
    ::new (static_cast<void*>(renderer + offset)) std::vector<T>();
}

// The source operations are resize(30, nullptr), then resize(0), retaining capacity 30.
// 10172890 / 1016F4F0 / 10174410, with the first-growth branch of 10172660.
template<class T>
static void psmCtorPrimePointerVector(unsigned char* renderer, unsigned int offset)
{
    std::vector<T>& list = particleRendererList<T>(renderer, offset);
    list.resize(30, 0);
    list.resize(0);
}

struct PsmCtorParameter
{
    unsigned int offset;
    const char* name;
};

template<unsigned int N>
static void psmCtorParameters(unsigned char* renderer, PsmRendererLayout layout,
                              unsigned int effectOffset, const PsmCtorParameter (&parameters)[N])
{
    for (unsigned int i = 0; i < N; ++i)
    {
        // Each temporary string and repeated effect read follows the corresponding PE call.
        const std::string name(parameters[i].name);
        psmCtorAt<dx9rend::ShaderParameter*>(renderer, layout, parameters[i].offset) =
            dx9rend::effectGetShaderParameterTyped(
                psmCtorAt<dx9rend::Effect*>(renderer, layout, effectOffset), name);
    }
}

// Each of the ten 0x70-byte records is value-constructed. The second allocation is left
// uninitialized in both constructors. Both are operator-new[]'s Memory.dll allocation seam.
static void psmCtorParticleScratch(unsigned char* renderer, PsmRendererLayout layout,
                                   unsigned int recordsOffset, unsigned int scratchOffset)
{
    void* records = ::renddx9abi::engineAlloc(0x460);
    if (records)
        psmCtorZeroWords(static_cast<unsigned char*>(records), 0, 0x460 / 4);
    psmCtorAt<void*>(renderer, layout, recordsOffset) = records;
    psmCtorAt<void*>(renderer, layout, scratchOffset) = ::renddx9abi::engineAlloc(0x800);
}

// PE 10171740..10171779 uses x87 fsin/fcos and __ftol2's truncation, keeping the angle on
// the x87 stack between the calls. Preserve that precision instead of spilling through a
// modern compiler's double/SSE temporary at the signed-short quantization boundary.
static void psmCtorTrigPair(unsigned int index, short& sine, short& cosine)
{
    const float step = 0.0015339808305725455284f; // 1021EF18, float(2*pi/4096)
    const float amplitude = 32767.0f;
    short sineValue;
    short cosineValue;
#if defined(_M_IX86)
    unsigned short originalControl;
    unsigned short truncateControl;
    __asm {
        fild index
        fmul step
        fld st(0)
        fsin
        fmul amplitude
        fnstcw originalControl
        mov ax, originalControl
        or ax, 0C00h
        mov truncateControl, ax
        fldcw truncateControl
        fistp sineValue
        fldcw originalControl
        fcos
        fmul amplitude
        fldcw truncateControl
        fistp cosineValue
        fldcw originalControl
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates). The angle stays a double
    // between both calls; the C++ conversions truncate toward zero like the chop-mode fistp.
    const double angle = static_cast<double>(static_cast<int>(index)) * step;
    sineValue = static_cast<short>(static_cast<int>(std::sin(angle) * amplitude));
    cosineValue = static_cast<short>(static_cast<int>(std::cos(angle) * amplitude));
#endif
    sine = sineValue;
    cosine = cosineValue;
}

static void psmConstructParticleRenderer(unsigned char* renderer)
{
    psmCtorClearTail(renderer, 0x5E18, kPsmSpriteRendererSize);
    renderer[0] = 0;
    psmSpriteField<int>(renderer, 4) = -1;
    psmCtorPointerVector<unsigned char*>(renderer, psmSpriteRendererOffset(8));
    psmSpriteField<void*>(renderer, 0x18) = 0;
    psmCtorZeroRetailWords(renderer, psmSpriteRendererOffset, 0x20, 256u * 3u);
    psmCtorZeroRetailWords(renderer, psmSpriteRendererOffset, 0xC20, 256u * 3u);
    psmSpriteField<unsigned int>(renderer, 0x1824) = 0;
    psmSpriteField<unsigned int>(renderer, 0x1828) = 0;
    psmSpriteField<unsigned int>(renderer, 0x1C2C) = 0;
    psmSpriteField<unsigned int>(renderer, 0x1C30) = 0;
    renderer[0x1C34] = 0;
    psmCtorZeroRetailWords(renderer, psmSpriteRendererOffset, 0x1C38, 6);
    psmCtorParticleMatrices(renderer + 0x1C58);
    psmCtorIdentity(renderer + 0x1D28);
    renderer[0x1D6C] = 0;
    psmSpriteField<unsigned int>(renderer, 0x1D70) = 0;
    renderer[0x1D78] = 0;
    renderer[0x1D79] = 0;
    psmSpriteField<unsigned int>(renderer, 0x1D80) = 0;
    psmSpriteField<unsigned int>(renderer, 0x1D84) = 0;
    for (unsigned int i = 0; i < 4; ++i)
        psmSpriteField<float>(renderer, 0x1D8C + 4u * i) = 0.5f;
    renderer[0x1D9C] = 1;
    renderer[0x1D9D] = 0;
    renderer[0x1D9E] = 0;
    psmSpriteField<void*>(renderer, 0x1DC0) = 0;
    psmSpriteField<void*>(renderer, 0x1DC8) = 0;
    psmSpriteField<void*>(renderer, 0x1DCC) = 0;
    psmCtorPrimePointerVector<unsigned char*>(renderer, psmSpriteRendererOffset(8));
    psmCtorZeroRetailWords(renderer, psmSpriteRendererOffset, 0x182C, 256);

    const D3DVERTEXELEMENT9 declaration[] = {
        {0, 0x00, 2, 0, 0, 0}, {0, 0x0C, 1, 0, 5, 0},
        {0, 0x14, 2, 0, 5, 1}, {0, 0x20, 1, 0, 5, 2},
        {0, 0x28, 1, 0, 5, 3}, {0, 0x30, 6, 0, 5, 4},
        {0, 0x34, 7, 0, 5, 5}, {0, 0x3C, 6, 0, 5, 6}, D3DDECL_END()
    };
    psmSpriteField<void*>(renderer, 0x1E0C) = dx9rend::vertexDeclarationCreate(declaration);
    if (!psmSpriteField<void*>(renderer, 0x1E0C))
    {
        // PE 101713A2..10171407 emits Debug type 6. Its destructor 10074690 traps after
        // emission, but the renderer's per-type/module suppression configuration can skip it.
        // [TEMP] [CHOSEN, not recovered] The renderer Debug dispatcher is not bridged here.
        // [RETIRE-WHEN: particle-constructor-debug-dispatch] Preserve an explicit stop at that
        // error-only dependency rather than replacing the retail diagnostic with log-and-return.
        RD9_FRONTIER_AT("dice::hfe::geom::ParticleSystemRenderer::ParticleSystemRenderer "
                       "(Debug type 6, ParticleSystemRenderer.cpp:113: Failed to create declspec)",
                       renderer, 0);
        std::abort();
    }
    psmSpriteField<void*>(renderer, 0x1DD0) =
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "mods/xpack/shaders/Particles");
    const PsmCtorParameter parameters[] = {
        {0x1DD4, "ViewMat"}, {0x1DD8, "ProjMat"}, {0x1DDC, "HemiMapInfo"},
        {0x1DE0, "HemiShadowAltitude"}, {0x1DEC, "Texture0"}, {0x1DE4, "Texture1"},
        {0x1DE8, "TemplateParameters"}, {0x1DF0, "fogRange"}, {0x1DF4, "fogColor"},
        {0x1E04, "WaterFogRange"}, {0x1E00, "WaterFogColor"}, {0x1E08, "CameraToWater"},
        {0x1DF8, "EffectSunColor"}, {0x1DFC, "EffectShadowColor"}
    };
    psmCtorParameters(renderer, psmSpriteRendererOffset, 0x1DD0, parameters);
    psmCtorParticleScratch(renderer, psmSpriteRendererOffset, 0x1DC8, 0x1DC0);
    for (unsigned int i = 0; i < 4096; ++i)
        psmCtorTrigPair(i, psmSpriteField<short>(renderer, 0x1E14 + i * 4u),
                          psmSpriteField<short>(renderer, 0x1E16 + i * 4u));
}

static void psmConstructMeshParticleRenderer(unsigned char* renderer)
{
    psmCtorClearTail(renderer, 0x1D90, kPsmMeshRendererSize);
    psmMeshField<int>(renderer, 0) = -1;
    psmMeshField<void*>(renderer, 4) = 0;
    psmCtorZeroRetailWords(renderer, psmMeshRendererOffset, 8, 256u * 3u);
    psmCtorZeroRetailWords(renderer, psmMeshRendererOffset, 0xC08, 256u * 3u);
    psmMeshField<unsigned int>(renderer, 0x180C) = 0;
    psmMeshField<unsigned int>(renderer, 0x1810) = 0;
    psmMeshField<unsigned int>(renderer, 0x1C14) = 0;
    renderer[0x1C18] = 0;
    psmCtorZeroRetailWords(renderer, psmMeshRendererOffset, 0x1C1C, 6);
    psmCtorMeshMatrices(renderer + 0x1C3C);
    renderer[0x1D3C] = 0;
    psmMeshField<unsigned int>(renderer, 0x1D40) = 0;
    renderer[0x1D48] = 0;
    renderer[0x1D49] = 0;
    psmMeshField<unsigned int>(renderer, 0x1D50) = 0;
    psmMeshField<unsigned int>(renderer, 0x1D54) = 0;
    for (unsigned int i = 0; i < 4; ++i)
        psmMeshField<float>(renderer, 0x1D5C + 4u * i) = 0.5f;
    renderer[0x1D6E] = 0;
    renderer[0x1D6F] = 0;
    renderer[0x1D6C] = 1;
    renderer[0x1D6D] = 1;
    psmCtorPointerVector<MeshParticleMesh*>(renderer, psmMeshRendererOffset(0x1D80));   // the retained mesh list
    psmCtorZeroRetailWords(renderer, psmMeshRendererOffset, 0x1814, 256);
}

// The complete three-slot table at 1021F058. There is no virtual destructor. Lost and
// destroy are the same attested nullsub_15 (10037D40); reset10172B00 fills all three IBs.
class PsmTrailDeviceListener : public dx9rend::DeviceLostResetListener
{
public:
    void onDeviceLost() override {}
    void onDeviceReset() override
    {
        for (unsigned int sides = 1; sides <= 3; ++sides)
            psmFillTrailIndices(reinterpret_cast<unsigned char*>(this), sides);
    }
    void onDeviceDestroy() override {}
};
static_assert(sizeof(PsmTrailDeviceListener) == sizeof(void*), "TrailRenderer's listener is one vptr");

static void psmConstructTrailRenderer(unsigned char* renderer)
{
    psmCtorClearTail(renderer, 0x914, kPsmTrailRendererSize);
    PsmTrailDeviceListener* const listener = new (renderer) PsmTrailDeviceListener;
    psmTrailField<int>(renderer, 4) = -1;
    psmCtorPointerVector<unsigned char*>(renderer, psmTrailRendererOffset(8));
    psmTrailField<void*>(renderer, 0x18) = 0;
    psmCtorZeroRetailWords(renderer, psmTrailRendererOffset, 0x1C, 64u * 3u);
    psmTrailField<unsigned int>(renderer, 0x320) = 0;
    psmTrailField<unsigned int>(renderer, 0x724) = 0;
    renderer[0x728] = 0;
    psmCtorZeroRetailWords(renderer, psmTrailRendererOffset, 0x72C, 6);
    psmCtorMeshMatrices(renderer + 0x74C);
    psmTrailField<float>(renderer, 0x868) = 300.0f;
    renderer[0x84C] = 0;
    psmTrailField<unsigned int>(renderer, 0x850) = 0;
    renderer[0x858] = 0;
    psmTrailField<unsigned int>(renderer, 0x860) = 0;
    psmTrailField<unsigned int>(renderer, 0x864) = 0;
    for (unsigned int i = 0; i < 4; ++i)
        psmTrailField<float>(renderer, 0x874 + 4u * i) = 0.5f;
    renderer[0x884] = 1;
    psmTrailField<float>(renderer, 0x86C) =
        psmTrailField<float>(renderer, 0x868) * psmTrailField<float>(renderer, 0x868);
    renderer[0x885] = 1;
    renderer[0x886] = 0;
    renderer[0x887] = 0;
    psmTrailField<unsigned int>(renderer, 0x8AC) = 0;
    psmTrailField<void*>(renderer, 0x8B4) = 0;
    psmCtorZeroRetailWords(renderer, psmTrailRendererOffset, 0x8D8, 15);
    psmCtorPrimePointerVector<unsigned char*>(renderer, psmTrailRendererOffset(8));
    psmCtorZeroRetailWords(renderer, psmTrailRendererOffset, 0x324, 256);
    psmCtorZeroRetailWords(renderer, psmTrailRendererOffset, 0x8B8, 4);
    psmTrailField<void*>(renderer, 0x8D8) =
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "mods/xpack/shaders/Trail");
    const D3DVERTEXELEMENT9 declaration[] = {
        {0, 0x00, 2, 0, 0, 0}, {0, 0x0C, 2, 0, 3, 0},
        {0, 0x18, 2, 0, 3, 1}, {0, 0x24, 3, 0, 5, 0},
        {0, 0x34, 7, 0, 5, 1}, {0, 0x3C, 1, 0, 5, 2}, D3DDECL_END()
    };
    psmTrailField<void*>(renderer, 0x910) = dx9rend::vertexDeclarationCreate(declaration);
    const PsmCtorParameter parameters[] = {
        {0x8E4, "ViewMat"}, {0x8E8, "ProjMat"}, {0x8EC, "HemiMapInfo"},
        {0x8F0, "HemiShadowAltitude"}, {0x8F8, "Texture0"}, {0x8FC, "Texture1"},
        {0x900, "EyePos"}, {0x904, "TemplateParameters"}, {0x8F4, "FresnelOffset"},
        {0x8DC, "fogRange"}, {0x8E0, "fogColor"}, {0x908, "EffectSunColor"},
        {0x90C, "EffectShadowColor"}
    };
    psmCtorParameters(renderer, psmTrailRendererOffset, 0x8D8, parameters);
    dx9rend::registerDeviceLostResetListener(listener, 4);
}

static void psmConstructNsaParticleRenderer(unsigned char* renderer)
{
    psmCtorClearTail(renderer, 0x5A0, kPsmNsaRendererSize);
    renderer[0] = 0;
    psmNsaField<unsigned int>(renderer, 4) = 0;
    psmNsaField<int>(renderer, 8) = -1;
    psmCtorPointerVector<unsigned char*>(renderer, psmNsaRendererOffset(0xC));
    psmNsaField<void*>(renderer, 0x1C) = 0;
    psmNsaField<unsigned int>(renderer, 0x420) = 0;
    renderer[0x424] = 0;
    psmCtorZeroRetailWords(renderer, psmNsaRendererOffset, 0x428, 6);
    psmNsaField<unsigned int>(renderer, 0x444) = 0;
    psmCtorParticleMatrices(renderer + 0x448);
    renderer[0x518] = 0;
    psmNsaField<unsigned int>(renderer, 0x51C) = 0;
    renderer[0x520] = 0;
    psmNsaField<unsigned int>(renderer, 0x528) = 0;
    psmNsaField<unsigned int>(renderer, 0x52C) = 0;
    for (unsigned int i = 0; i < 4; ++i)
        psmNsaField<float>(renderer, 0x534 + 4u * i) = 0.5f;
    renderer[0x544] = 1;
    renderer[0x545] = 0;
    renderer[0x546] = 0;
    psmNsaField<void*>(renderer, 0x55C) = 0;
    psmNsaField<void*>(renderer, 0x564) = 0;
    psmNsaField<void*>(renderer, 0x568) = 0;
    psmCtorPrimePointerVector<unsigned char*>(renderer, psmNsaRendererOffset(0xC));
    psmCtorZeroRetailWords(renderer, psmNsaRendererOffset, 0x20, 256);

    const D3DVERTEXELEMENT9 declaration[] = {
        {0, 0x00, 2, 0, 0, 0}, {0, 0x0C, 1, 0, 5, 0},
        {0, 0x14, 2, 0, 5, 1}, {0, 0x20, 2, 0, 5, 2},
        {0, 0x2C, 1, 0, 5, 3}, {0, 0x34, 7, 0, 5, 4},
        {0, 0x3C, 7, 0, 5, 5}, {0, 0x44, 6, 0, 5, 6}, D3DDECL_END()
    };
    psmNsaField<void*>(renderer, 0x59C) = dx9rend::vertexDeclarationCreate(declaration);
    psmNsaField<void*>(renderer, 0x56C) = dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, "mods/xpack/shaders/NonScreenAlignedParticles");
    const PsmCtorParameter parameters[] = {
        {0x570, "ViewMat"}, {0x574, "ProjMat"}, {0x578, "HemiMapInfo"},
        {0x57C, "HemiShadowAltitude"}, {0x588, "Texture0"}, {0x580, "Texture1"},
        {0x584, "TemplateParameters"}, {0x58C, "fogRange"}, {0x590, "fogColor"},
        {0x594, "EffectSunColor"}, {0x598, "EffectShadowColor"}
    };
    psmCtorParameters(renderer, psmNsaRendererOffset, 0x56C, parameters);
    psmCtorParticleScratch(renderer, psmNsaRendererOffset, 0x564, 0x55C);
}
