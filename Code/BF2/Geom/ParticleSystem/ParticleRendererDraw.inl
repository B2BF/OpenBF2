// Particle collection, shader binding and GPU submission; included inside the manager namespace.
// [RETIRE-WHEN: particle-mesh-debug-bounds] Replace the optional bounds boundary after
// the mesh overlay is implemented and validated. Include standard headers before this file.

template<typename T>
static T& psmDrawField(unsigned char* renderer, unsigned int offset)
{
    return *reinterpret_cast<T*>(renderer + offset);
}

// vector<T*>::resize(0), used by 10172890, 1016F4F0 and 10174410, on the renderer pointer list at
// native `offset`. These pointer elements own nothing: erase retains the allocation and capacity.
template<class T>
static void psmDrawClearPointerVector(unsigned char* renderer, unsigned int offset)
{
    particleRendererList<T>(renderer, offset).clear();
}

template<class T>
static unsigned int psmDrawPointerVectorSize(unsigned char* renderer, unsigned int offset)
{
    return static_cast<unsigned int>(particleRendererList<T>(renderer, offset).size());
}

#include "ParticleRendererActive.inl"
#include "ParticleRendererGpu.inl"
#include "ParticleRendererNsa.inl"
#include "ParticleRendererTrail.inl"
#include "ParticleRendererMesh.inl"

// The resets precede both the empty-input and disabled exits;
// the current index at +4 is deliberately retained. The vector is at renderer +8.
static void psmPrepareParticleRenderer(unsigned char* renderer, dx9rend::IRenderView* view,
                                       ParticleSystemInstanceVector* instances, float deltaTime, bool firstPerson)
{
    const unsigned int count = static_cast<unsigned int>(instances->size());
    psmSpriteField<unsigned int>(renderer, 0x1D7C) = 0;
    psmSpriteField<unsigned char>(renderer, 0x1D6C) = 0;
    psmDrawClearPointerVector<unsigned char*>(renderer, psmSpriteRendererOffset(8));
    psmSpriteField<unsigned int>(renderer, 0x1820) = 0;
    psmSpriteField<unsigned int>(renderer, 0x1824) = 0;
    psmSpriteField<unsigned int>(renderer, 0x1828) = 0;
    if (!count || !psmSpriteField<unsigned char>(renderer, 0x1D9C))
        return;

    psmSpriteField<float>(renderer, 0x1C3C) = deltaTime;
    psmSpriteField<Mat4>(renderer, 0x1C58) = *view->getInverseViewMatrixPtr();
    Mat4& viewProjection = psmSpriteField<Mat4>(renderer, 0x1C98);
    viewProjection = *view->getViewMatrixPtr();
    const Mat4 originalView = viewProjection;
    fullInvert(originalView, viewProjection);
    const Mat4* projection = view->getProjectionMatrixPtr();
    const Mat4 inverseView = viewProjection;
    viewProjection.fullMult(inverseView, *projection);
    // CRT initializer 101D5060: 1 / sqrt(double(2)), stored once as a float.
    psmSpriteField<float>(renderer, 0x1D18) = static_cast<float>(1.0 / std::sqrt(2.0));
    psmSpriteField<float>(renderer, 0x1D1C) = 255.0f;
    psmSpriteField<unsigned int>(renderer, 0x1D20) = 0;
    psmSpriteField<unsigned int>(renderer, 0x1D24) = 0;
    const Mat4* inverse = view->getInverseViewMatrixPtr();
    for (unsigned int i = 0; i < 4; ++i)
        psmSpriteField<float>(renderer, 0x1C40 + i * 4) = inverse->m[2 + i * 4];
    for (unsigned int i = 0; i < count; ++i)
    {
        if ((*instances)[i]->reserved01() != 0) continue;
        unsigned char* instance = reinterpret_cast<unsigned char*>((*instances)[i]);
        if ((instance[8] != 0) != firstPerson) continue;
        if (!instance[0x14C] && !psmInstanceA<unsigned int>(instance, 0x28)) continue;
        const float* bounds = reinterpret_cast<float*>(instance + 0xFC);
        Vec3 center;
        center.x = static_cast<float>((static_cast<double>(bounds[0]) + bounds[3]) * 0.5);
        center.y = static_cast<float>((static_cast<double>(bounds[4]) + bounds[1]) * 0.5);
        const float sumZ = bounds[5] + bounds[2];
        center.z = sumZ * 0.5f;
        const float halfX = (bounds[3] - bounds[0]) * 0.5f;
        const double halfY = (static_cast<double>(bounds[4]) - bounds[1]) * 0.5;
        const float deltaZ = bounds[5] - bounds[2];
        const double halfZ = static_cast<double>(deltaZ) * 0.5;
        const float radius = static_cast<float>(std::sqrt(halfZ * halfZ + static_cast<double>(halfX) * halfX + halfY * halfY));
        if (!view->getFrustumPtr()->inside(center, radius)) continue;
        void* owner = psmInstanceA<void*>(instance, 0x10);
        if (owner && psmOwnerRootDisabled(owner)) continue;             // IObject m_flags bit 0 (RW-266)
        psmInstanceA<unsigned int>(instance, 0x124) =
            static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getFrameOrdinal();
        instance[0x128] = 1;
        if (psmInstanceA<unsigned int>(instance, 0x28))
        {
            unsigned char* descriptor = psmInstanceA<unsigned char*>(instance, 0x2C);
            typedef void (__thiscall *Collector)(void*, dx9rend::IRenderView*,
                IParticleSystemInstance*, IParticleSystemTemplate*);
            psmDescriptor0<Collector>(descriptor, 0x2A0)(renderer, view,
                reinterpret_cast<IParticleSystemInstance*>(instance),
                reinterpret_cast<IParticleSystemTemplate*>(descriptor));
        }
    }
    if (psmSpriteField<unsigned int>(renderer, 0x1C38))
    {
        psmSortParticles(renderer, psmSpriteField<unsigned char**>(renderer, 0x18),
            psmSpriteField<unsigned int>(renderer, 0x1C38));
        psmSpriteField<int>(renderer, 4) = psmSpriteField<unsigned int>(renderer, 0x1C38) - 1;
    }
}

// Empty or disabled input returns before any renderer write.
// In particular it does not reset the index +8 or retained eligibility byte +520.
static void psmPrepareNsaParticleRenderer(unsigned char* renderer, dx9rend::IRenderView* view,
                                          ParticleSystemInstanceVector* instances,
                                          float deltaTime, bool firstPerson)
{
    const unsigned int count = static_cast<unsigned int>(instances->size());
    if (!count || !psmNsaField<unsigned char>(renderer, 0x544))
        return;

    psmNsaField<unsigned int>(renderer, 0x524) = 0;
    psmNsaField<unsigned char>(renderer, 0x518) = 0;
    psmDrawClearPointerVector<unsigned char*>(renderer, psmNsaRendererOffset(0xC));
    psmNsaField<float>(renderer, 0x42C) = deltaTime;
    psmNsaField<Mat4>(renderer, 0x448) = *view->getInverseViewMatrixPtr();
    Mat4& viewProjection = psmNsaField<Mat4>(renderer, 0x488);
    viewProjection = *view->getViewMatrixPtr();
    const Mat4 originalView = viewProjection;
    fullInvert(originalView, viewProjection);
    const Mat4* projection = view->getProjectionMatrixPtr();
    const Mat4 inverseView = viewProjection;
    viewProjection.fullMult(inverseView, *projection);
    psmNsaField<float>(renderer, 0x508) = static_cast<float>(1.0 / std::sqrt(2.0));
    psmNsaField<float>(renderer, 0x50C) = 255.0f;
    psmNsaField<unsigned int>(renderer, 0x510) = 0;
    psmNsaField<unsigned int>(renderer, 0x514) = 0;
    const Mat4* inverse = view->getInverseViewMatrixPtr();
    for (unsigned int i = 0; i < 4; ++i)
        psmNsaField<float>(renderer, 0x430 + 4 * i) = inverse->m[2 + 4 * i];
    for (unsigned int i = 0; i < count; ++i)
    {
        if ((*instances)[i]->reserved01() != 4) continue;
        unsigned char* instance = reinterpret_cast<unsigned char*>((*instances)[i]);
        if (!instance[0x14C] && !psmInstanceA<unsigned int>(instance, 0x28)) continue;
        if ((instance[8] != 0) != firstPerson) continue;
        const float* bounds = reinterpret_cast<float*>(instance + 0xFC);
        Vec3 center;
        center.x = static_cast<float>((static_cast<double>(bounds[0]) + bounds[3]) * 0.5);
        center.y = static_cast<float>((static_cast<double>(bounds[4]) + bounds[1]) * 0.5);
        const float sumZ = bounds[5] + bounds[2];
        center.z = sumZ * 0.5f;
        const float halfX = (bounds[3] - bounds[0]) * 0.5f;
        const double halfY = (static_cast<double>(bounds[4]) - bounds[1]) * 0.5;
        const float deltaZ = bounds[5] - bounds[2];
        const double halfZ = static_cast<double>(deltaZ) * 0.5;
        const float radius = static_cast<float>(std::sqrt(halfZ * halfZ
            + static_cast<double>(halfX) * halfX + halfY * halfY));
        if (!view->getFrustumPtr()->inside(center, radius)) continue;
        void* owner = psmInstanceA<void*>(instance, 0x10);
        if (owner && psmOwnerRootDisabled(owner)) continue;             // IObject m_flags bit 0 (RW-266)
        psmInstanceA<unsigned int>(instance, 0x124) =
            static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getFrameOrdinal();
        instance[0x128] = 1;
        if (psmInstanceA<unsigned int>(instance, 0x28))
        {
            psmCollectNsaParticles(renderer, view, instance, psmInstanceA<unsigned char*>(instance, 0x2C));
            if (renderer[0x545])
            {
                // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: particle-nsa-debug-bounds]
                // PE 1016E986 requests/publishes a DebugBoxMesh through owner10247C3C.
                std::fprintf(stderr, "[FRONTIER] dice::hfe::geom::NSAParticleSystemRenderer::prepare "
                    "(debug bounding mesh, PE 1016E986) reached\n");
                std::fflush(stderr);
                std::abort();
            }
        }
    }
    if (psmNsaField<unsigned int>(renderer, 0x428))
    {
        psmSortNsaParticles(renderer, psmNsaField<unsigned char**>(renderer, 0x1C),
            psmNsaField<unsigned int>(renderer, 0x428));
        psmNsaField<int>(renderer, 8) = psmNsaField<unsigned int>(renderer, 0x428) - 1;
    }
}

// The five counters and the matrix-cache byte are reset even
// for empty or disabled input. The retained pointer vector at +1D80 is not cleared.
static void psmPrepareMeshParticleRenderer(unsigned char* renderer, dx9rend::IRenderView* view,
                                           ParticleSystemInstanceVector* instances, float deltaTime)
{
    psmMeshField<unsigned char>(renderer, 0x1D3C) = 0;
    const unsigned int count = static_cast<unsigned int>(instances->size());
    psmMeshField<unsigned int>(renderer, 0x1C14) = 0;
    psmMeshField<unsigned int>(renderer, 0x1808) = 0;
    psmMeshField<unsigned int>(renderer, 0x180C) = 0;
    psmMeshField<unsigned int>(renderer, 0x1810) = 0;
    psmMeshField<unsigned int>(renderer, 0x1D4C) = 0;
    if (!count || !psmMeshField<unsigned char>(renderer, 0x1D6C))
        return;

    psmMeshField<float>(renderer, 0x1C20) = deltaTime;
    // the inverse-view (camera) matrix's third column is the depth row the collector
    // dots every particle with. Primary slot +0x68.
    const Mat4* inverse = view->getInverseViewMatrixPtr();
    for (unsigned int i = 0; i < 4; ++i)
        psmMeshField<float>(renderer, 0x1C24 + 4 * i) = inverse->m[2 + 4 * i];
    for (unsigned int i = 0; i < count; ++i)
    {
        if ((*instances)[i]->reserved01() != 1) continue;
        unsigned char* instance = reinterpret_cast<unsigned char*>((*instances)[i]);
        if (!instance[0x144] && !psmInstanceB<unsigned int>(instance, 0x28)) continue;
        // Center inline, radius through 1016EB70, exactly as the sibling renderers.
        const float* bounds = reinterpret_cast<float*>(instance + 0xFC);
        Vec3 center;
        center.x = static_cast<float>((static_cast<double>(bounds[0]) + bounds[3]) * 0.5);
        center.y = static_cast<float>((static_cast<double>(bounds[4]) + bounds[1]) * 0.5);
        const float sumZ = bounds[5] + bounds[2];
        center.z = sumZ * 0.5f;
        const float halfX = (bounds[3] - bounds[0]) * 0.5f;
        const double halfY = (static_cast<double>(bounds[4]) - bounds[1]) * 0.5;
        const float deltaZ = bounds[5] - bounds[2];
        const double halfZ = static_cast<double>(deltaZ) * 0.5;
        const float radius = static_cast<float>(std::sqrt(halfZ * halfZ
            + static_cast<double>(halfX) * halfX + halfY * halfY));
        if (!view->getFrustumPtr()->inside(center, radius)) continue;
        void* owner = psmInstanceB<void*>(instance, 0x10);
        if (owner && psmOwnerRootDisabled(owner)) continue;             // IObject m_flags bit 0 (RW-266)
        psmInstanceB<unsigned int>(instance, 0x124) =
            static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getFrameOrdinal();
        instance[0x21E] = 1;
        if (!psmInstanceB<unsigned int>(instance, 0x28)) continue;
        // 10174F2F: 255 admitted groups (both draw orders together) end the whole walk; the
        // remaining instances are neither marked nor collected. The note is one-shot per process.
        if (psmMeshField<unsigned int>(renderer, 0x180C) + psmMeshField<unsigned int>(renderer, 0x1810) >= 0xFF)
        {
            psmNoteMeshGroupOverflow(renderer);
            break;
        }
        psmCollectMeshParticles(renderer, view, instance, psmInstanceB<unsigned char*>(instance, 0x2C));
        if (renderer[0x1D6E])
        {
            // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: particle-mesh-debug-bounds]
            // PE 10174F58..1017500E requests a DebugBoxMesh through owner 10247C3C (+0x18 by
            // reflection type, named "bounding" via +0x68), scales the shared identity (100791E0)
            // by the aabb extents, translates it to the aabb center (1016EBC0) and publishes it
            // (+0x14). Renderer byte +0x1D6E, constructor default false.
            std::fprintf(stderr, "[FRONTIER] dice::hfe::geom::MeshParticlesRenderer::prepare "
                "(debug bounding mesh, PE 10174F58) reached\n");
            std::fflush(stderr);
            std::abort();
        }
    }
    // the over-budget byte compares +0x1C14, which this function zeroed above and only
    // the draw increments, so it is always false here. Kept, like Trail's +0x728.
    const unsigned int threshold = static_cast<unsigned int>(static_cast<int>(
        static_cast<double>(psmMeshField<unsigned int>(renderer, 0x1D78)) * 0.85000002f));
    renderer[0x1C18] = psmMeshField<unsigned int>(renderer, 0x1C14) > threshold;
}

// The table loads precede the index == -1 tests in the PE,
// including the loops. There is no table-null or index-range guard. The unsigned comparison
// is entry +14 >= target. Both sorted groups contribute to the same +1D79 byte.
static bool psmSelectParticleRenderer(unsigned char* renderer, unsigned int target)
{
    unsigned char** const table = psmSpriteField<unsigned char**>(renderer, 0x18);
    psmSpriteField<unsigned int>(renderer, 0x1D7C) = target;
    const int index = psmSpriteField<int>(renderer, 4);
    psmSpriteField<unsigned char>(renderer, 0x1D78) = 0;
    psmSpriteField<unsigned char>(renderer, 0x1D79) = 0;
    unsigned char* const entry = table[index];
    if (index != -1 && entry && psmDrawField<unsigned int>(entry, 0x14) >= target)
        psmSpriteField<unsigned char>(renderer, 0x1D78) = 1;

    for (unsigned int group = 0; group < 2; ++group)
    {
        const unsigned int countOffset = group ? 0x1828 : 0x1824;
        if (psmSpriteField<unsigned int>(renderer, countOffset))
        {
            unsigned char** const sortedTable = psmSpriteField<unsigned char**>(renderer, 0x1C);
            const unsigned int firstIndexOffset = group ? 0xC24 : 0x24;
            for (unsigned int i = 0; i < psmSpriteField<unsigned int>(renderer, countOffset); ++i)
            {
                const int sortedIndex = psmSpriteField<int>(renderer, firstIndexOffset + 12 * i);
                unsigned char* const sortedEntry = sortedTable[sortedIndex];
                if (sortedIndex != -1 && sortedEntry
                    && psmDrawField<unsigned int>(sortedEntry, 0x14) >= target)
                {
                    psmSpriteField<unsigned char>(renderer, 0x1D79) = 1;
                    break;
                }
            }
        }
    }
    return psmSpriteField<unsigned char>(renderer, 0x1D79) != 0
        || psmSpriteField<unsigned char>(renderer, 0x1D78) != 0;
}

// Unlike the ordinary renderer this never clears +520.
// The entry's threshold at +94 is unsigned, and the indexed load precedes the -1 test.
static bool psmSelectNsaParticleRenderer(unsigned char* renderer, unsigned int target)
{
    unsigned char** const table = psmNsaField<unsigned char**>(renderer, 0x1C);
    psmNsaField<unsigned int>(renderer, 0x524) = target;
    const int index = psmNsaField<int>(renderer, 8);
    unsigned char* const entry = table[index];
    if (index != -1 && entry && psmDrawField<unsigned int>(entry, 0x94) >= target)
        psmNsaField<unsigned char>(renderer, 0x520) = 1;
    return psmNsaField<unsigned char>(renderer, 0x520) != 0;
}

// The two sorted groups have independent flags. Every invocation
// clears both flags, stores target, then scans each group until its first qualifying record.
static bool psmSelectMeshParticleRenderer(unsigned char* renderer, unsigned int target)
{
    psmMeshField<unsigned int>(renderer, 0x1D4C) = target;
    psmMeshField<unsigned char>(renderer, 0x1D48) = 0;
    psmMeshField<unsigned char>(renderer, 0x1D49) = 0;
    for (unsigned int group = 0; group < 2; ++group)
    {
        const unsigned int countOffset = group ? 0x1810 : 0x180C;
        if (psmMeshField<unsigned int>(renderer, countOffset))
        {
            unsigned char** const table = psmMeshField<unsigned char**>(renderer, 4);
            const unsigned int firstIndexOffset = group ? 0xC0C : 0xC;
            for (unsigned int i = 0; i < psmMeshField<unsigned int>(renderer, countOffset); ++i)
            {
                const int index = psmMeshField<int>(renderer, firstIndexOffset + 12 * i);
                unsigned char* const entry = table[index];
                if (index != -1 && entry && psmDrawField<unsigned int>(entry, 0xBC) >= target)
                {
                    psmMeshField<unsigned char>(renderer, group ? 0x1D49 : 0x1D48) = 1;
                    break;
                }
            }
        }
    }
    return psmMeshField<unsigned char>(renderer, 0x1D49) != 0
        || psmMeshField<unsigned char>(renderer, 0x1D48) != 0;
}

// An empty count does not clear previously-set eligibility.
static bool psmSelectTrailRenderer(unsigned char* renderer, unsigned int target)
{
    psmTrailField<unsigned int>(renderer, 0x85C) = target;
    if (psmTrailField<unsigned int>(renderer, 0x320))
        psmTrailField<unsigned char>(renderer, 0x858) = 1;
    return psmTrailField<unsigned char>(renderer, 0x858) != 0;
}

// both exits explicitly return false in AL. The active branch
// first constructs/enters the ParticleSystem profiling scope, then touches StateManager.
static bool psmDrawParticleRenderer(unsigned char* renderer, dx9rend::IRenderView* view)
{
    if (!psmSpriteField<unsigned char>(renderer, 0x1D9C))
        return false;
    if (!psmSpriteField<unsigned char>(renderer, 0x1D79)
        && !psmSpriteField<unsigned char>(renderer, 0x1D78))
        return false;
    // [CHOSEN, not recovered] Like the existing mesh/sky draw paths, renderer profiler
    // registration/timing is omitted; all render state and submission ordering is retained.
    dx9rend::stateManagerPush();
    if (!renderer[0x1D6C])
    {
        psmSpriteField<Mat4>(renderer, 0x1CD8) = *view->getProjectionMatrixPtr();
        const Mat4* projection = view->getProjectionMatrixPtr();
        const Mat4* inverse = view->getInverseViewMatrixPtr();
        psmSpriteField<Mat4>(renderer, 0x1D28).fullMult(*inverse, *projection);
        psmSpriteField<float>(renderer, 0x1D68) = view->getViewport()[2];
        renderer[0x1D6C] = 1;
    }
    bool drawn = false;
    psmBindParticleParameters(renderer);
    if (renderer[0x1D79] && psmSpriteField<unsigned int>(renderer, 0x1828))
        drawn = psmDrawAdditiveParticles(renderer, psmSpriteField<unsigned int>(renderer, 0x1D7C),
            psmSpriteField<unsigned int>(renderer, 0x1828), psmSpriteGroups(renderer, 0xC20));
    if (renderer[0x1D78])
    {
        const bool sortedDrawn = psmDrawSortedParticles(renderer, psmSpriteField<float>(renderer, 0x1D7C));
        drawn = sortedDrawn || drawn;
        renderer[0x1D78] = 0;
    }
    if (renderer[0x1D79] && psmSpriteField<unsigned int>(renderer, 0x1824))
        drawn = psmDrawAdditiveParticles(renderer, psmSpriteField<unsigned int>(renderer, 0x1D7C),
            psmSpriteField<unsigned int>(renderer, 0x1824), psmSpriteGroups(renderer, 0x20)); // retail overwrites result
    renderer[0x1D79] = 0;
    dx9rend::stateManagerPop();
    return drawn;
}

// Do not clear +520 on the disabled exit: retail clears it
// only after the active 1016D9B0 draw returns.
static bool psmDrawNsaParticleRenderer(unsigned char* renderer, dx9rend::IRenderView* view)
{
    if (!psmNsaField<unsigned char>(renderer, 0x544)
        || !psmNsaField<unsigned char>(renderer, 0x520))
        return false;
    // [CHOSEN, not recovered] Profiler registration/timing is omitted, as in the ordinary draw.
    dx9rend::stateManagerPush();
    if (!renderer[0x518])
    {
        psmNsaField<Mat4>(renderer, 0x4C8) = *view->getProjectionMatrixPtr();
        renderer[0x518] = 1;
    }
    bool drawn = false;
    if (renderer[0x520])
    {
        drawn = psmDrawNsaParticles(renderer, psmNsaField<unsigned int>(renderer, 0x524));
        renderer[0x520] = 0;
    }
    dx9rend::stateManagerPop();
    return drawn;
}

// Either the selected group flag OR a retained +1D80 list
// requires the active branch. The renderer-enabled check comes after that decision.
static bool psmDrawMeshParticleRenderer(unsigned char* renderer, dx9rend::IRenderView* view, bool pass)
{
    if (!psmMeshField<unsigned char>(renderer, pass ? 0x1D48 : 0x1D49)
        && !psmDrawPointerVectorSize<MeshParticleMesh*>(renderer, psmMeshRendererOffset(0x1D80)))
        return false;
    if (!psmMeshField<unsigned char>(renderer, 0x1D6C))
        return false;
    // [CHOSEN, not recovered] The "MeshParticles" profiler scope (static 10261718) is omitted,
    // exactly as in the other recovered draw wrappers.
    dx9rend::stateManagerPush();
    if (!renderer[0x1D3C])
    {
        psmMeshField<Mat4>(renderer, 0x1CFC) = *view->getProjectionMatrixPtr();
        const Mat4* projection = view->getProjectionMatrixPtr();
        const Mat4* inverse = view->getInverseViewMatrixPtr();
        psmMeshField<Mat4>(renderer, 0x1CBC).fullMult(*inverse, *projection);
        renderer[0x1D3C] = 1;
    }
    // `mov bl, 1` precedes the group loops and each result is OR-ed in: the active branch
    // always returns true.
    bool drawn = true;
    psmBindMeshParticleParameters(renderer);
    if (!pass)
    {
        if (renderer[0x1D49])
        {
            for (unsigned int i = 0; i < psmMeshField<unsigned int>(renderer, 0x1810); ++i)
                drawn = psmDrawMeshParticleGroup(renderer, view,
                    psmMeshField<unsigned int>(renderer, 0x1D4C), i, psmMeshGroups(renderer, 0xC08)) || drawn;
            renderer[0x1D49] = 0;
        }
    }
    else
    {
        // The pass=true arm does NOT retest +0x1D48 here (the prologue may have been satisfied
        // by the retained list alone); it walks every group-0 record and then clears the flag.
        for (unsigned int i = 0; i < psmMeshField<unsigned int>(renderer, 0x180C); ++i)
            drawn = psmDrawMeshParticleGroup(renderer, view,
                psmMeshField<unsigned int>(renderer, 0x1D4C), i, psmMeshGroups(renderer, 8)) || drawn;
        renderer[0x1D48] = 0;
    }
    dx9rend::stateManagerPop();
    return drawn;
}

// The extra draw uses the retained list independently of
// either sorted-group count/flag. Empty and disabled exits materialize false in AL.
static bool psmDrawMeshParticleExtras(unsigned char* renderer, bool pass)
{
    if (!psmDrawPointerVectorSize<MeshParticleMesh*>(renderer, psmMeshRendererOffset(0x1D80))
        || !psmMeshField<unsigned char>(renderer, 0x1D6C))
        return false;
    // [CHOSEN, not recovered] The second "MeshParticles" profiler scope (static 102614A8) is
    // omitted like the others.
    dx9rend::stateManagerPush();
    psmBindMeshParticleParameters(renderer);
    psmDrawMeshParticleExtraList(renderer, pass);
    dx9rend::stateManagerPop();
    return true;
}
