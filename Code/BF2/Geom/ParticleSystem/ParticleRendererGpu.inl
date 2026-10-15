// [INFERRED] private split of the ordinary sprite renderer's active GPU submission.
// Complete BF2 1.50 RendDX9 instruction streams: 10171F60, 1016F740, 1016FAE0,
// Dedicated ELF has no rendering counterpart.

// The shader parameters each renderer keeps, by retail offset.
static dx9rend::ParameterOwner* psmParticleParameter(unsigned char* renderer, unsigned int offset)
{
    return psmSpriteField<dx9rend::ParameterOwner*>(renderer, offset);
}
static dx9rend::ParameterOwner* psmNsaParameter(unsigned char* renderer, unsigned int offset)
{
    return psmNsaField<dx9rend::ParameterOwner*>(renderer, offset);
}
static dx9rend::ParameterOwner* psmTrailParameter(unsigned char* renderer, unsigned int offset)
{
    return psmTrailField<dx9rend::ParameterOwner*>(renderer, offset);
}

// Error-only dependency, matching the existing constructor diagnostic boundary.
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: particle-draw-debug-dispatch]
// The retail renderer Debug type-6 dispatcher is not yet available here. Never silently
// continue a failed GPU lock or the retail inconsistent-sort-count assertion.
static void psmParticleDrawDebug(unsigned int line, const char* message)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::geom::ParticleSystemRenderer::draw "
        "(Debug type 6, ParticleSystemRenderer.cpp:%u: %s) reached\n", line, message);
    std::fflush(stderr);
    std::abort();
}

static void* psmParticleDescriptorTexture(unsigned char* descriptor)
{
    unsigned char* region = psmDescriptor0<unsigned char*>(descriptor, 0x260);
    return region ? static_cast<void*>(reinterpret_cast<dx9rend::TextureAtlasEntry*>(region)->page) : psmDescriptor0<void*>(descriptor, 0x25C);
}

// PE 10172130 copies seven Vec4 values; these are plain descriptor shader parameters.
static void psmParticleCopyParameters(unsigned char* destination, unsigned char* source)
{
    for (unsigned int i = 0; i < 28; ++i)
        psmDrawField<float>(destination, i * 4) = psmDrawField<float>(source, i * 4);
}

static void psmParticleBlend(bool additive)
{
    ParticleSystemManager* manager = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    if (additive || !manager->m_shaderQuality)
    {
        dx9rend::stateManagerSetRenderState(19, 2);
        dx9rend::stateManagerSetRenderState(20, 2);
        dx9rend::stateManagerSetRenderState(28, 0);
    }
    else
    {
        dx9rend::stateManagerSetRenderState(19, 5);
        dx9rend::stateManagerSetRenderState(20, 6);
        dx9rend::stateManagerSetRenderState(28, 1);
    }
}

static void psmBindParticleParameters(unsigned char* renderer)
{
    psmParticleParameter(renderer, 0x1DD4)->setValue(renderer + 0x1C58, 0);
    psmParticleParameter(renderer, 0x1DD8)->setValue(renderer + 0x1CD8, 0);
    HemiMap* map = g_HemiMapManager->getBaseHemiMap();
    if (map)
    {
        psmParticleParameter(renderer, 0x1DDC)->setValue(&g_HemiMapManager->getHemiMapInfo(map), 0);
        psmParticleParameter(renderer, 0x1DE0)->setFloat(map->falloff);
        psmParticleParameter(renderer, 0x1DE4)->setValue(map->texture, 0);
    }
    else
    {
        // PE 100F1490 asks HemiMapManager again, despite the preceding null result.
        HemiMap* fallback = g_HemiMapManager->getBaseHemiMap();
        psmParticleParameter(renderer, 0x1DE4)->setValue(fallback ? fallback->texture : 0, 0);
    }
    psmParticleParameter(renderer, 0x1DF0)->setValue(dx9rend::raShaderConstantsAt(0xA8), 0);
    psmParticleParameter(renderer, 0x1DF4)->setValue(dx9rend::raShaderConstantsAt(0xBC), 0);
    psmParticleParameter(renderer, 0x1E00)->setValue(dx9rend::raShaderConstantsAt(0x10C), 0);
    psmParticleParameter(renderer, 0x1E04)->setValue(dx9rend::raShaderConstantsAt(0xF8), 0);
    psmParticleParameter(renderer, 0x1E08)->setValue(dx9rend::raShaderConstantsAt(0x1D8), 0);
    psmParticleParameter(renderer, 0x1DF8)->setValue(&g_LightManager->getEffectSunColor(), 0);
    psmParticleParameter(renderer, 0x1DFC)->setValue(&g_LightManager->getEffectShadowColor(), 0);
    ParticleSystemManager* manager = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    if (!manager->m_shaderQuality)
    {
        const float scale = 1.0f / manager->m_value8CA4;
        const Vec3 color = { scale, scale, scale };
        psmParticleParameter(renderer, 0x1DF8)->setValue(&color, 0);
    }
    dx9rend::vertexDeclarationBind(psmSpriteField<void*>(renderer, 0x1E0C));
    dx9rend::vertexBufferBindStream(psmSpriteField<void*>(renderer, 0x1DCC), 0);
}

// PE 1016FAE0 batches at most ten descriptor IDs. The sorted variant deliberately
// passes count=0 to setValue; the additive variant below passes the actual count.
static unsigned int psmBatchSortedParticles(unsigned char* renderer, unsigned int begin, unsigned int end)
{
    unsigned char* mapping = psmSpriteField<unsigned char*>(renderer, 0x1DC0);
    ::memset(mapping, 0xFF, 0x800);
    unsigned int& count = psmSpriteField<unsigned int>(renderer, 0x1DC4);
    count = 0;
    unsigned int i = begin;
    for (; i < end; ++i)
    {
        unsigned char* particle = psmSpriteField<unsigned char**>(renderer, 0x18)[psmSpriteField<int>(renderer, 4) - i];
        unsigned short id = psmDrawField<unsigned short>(particle, 4);
        if (mapping[id] != 0xFF) continue;
        if (count >= 10) break;
        mapping[id] = static_cast<unsigned char>(count);
        // PE 100D4CE0 is map::operator[], including insertion on a missing ID.
        unsigned char* descriptor = reinterpret_cast<unsigned char*>(g_templatesById[id]);
        psmParticleCopyParameters(psmSpriteField<unsigned char*>(renderer, 0x1DC8) + count * 112, descriptor + 0x13C);
        ++count;
    }
    psmParticleParameter(renderer, 0x1DE8)->setValue(psmSpriteField<void*>(renderer, 0x1DC8), 0);
    return i;
}

static unsigned int psmBatchAdditiveParticles(unsigned char* renderer, unsigned int begin,
    unsigned int end, PsmDrawGroup* groups, unsigned int& particles)
{
    particles = 0;
    unsigned char* mapping = psmSpriteField<unsigned char*>(renderer, 0x1DC0);
    ::memset(mapping, 0xFF, 0x800);
    unsigned int& count = psmSpriteField<unsigned int>(renderer, 0x1DC4);
    count = 0;
    unsigned char* initial = groups[begin].instance;
    void* texture = psmParticleDescriptorTexture(psmInstanceA<unsigned char*>(initial, 0x2C));
    unsigned int i = begin;
    for (; i < end; ++i)
    {
        PsmDrawGroup* const group = groups + i;
        if (!group->count) continue;
        unsigned char* instance = group->instance;
        unsigned char* descriptor = psmInstanceA<unsigned char*>(instance, 0x2C);
        const unsigned short id = psmDescriptor0<unsigned short>(descriptor, 4);
        if (id >= 0x800) psmParticleDrawDebug(404, "Too Many templates");
        if (psmParticleDescriptorTexture(descriptor) != texture) break;
        texture = psmParticleDescriptorTexture(descriptor);
        if (mapping[id] == 0xFF)
        {
            if (count >= 10) break;
            mapping[id] = static_cast<unsigned char>(count);
            psmParticleCopyParameters(psmSpriteField<unsigned char*>(renderer, 0x1DC8) + count * 112, descriptor + 0x13C);
            ++count;
        }
        particles += group->count;
    }
    if (count)
        psmParticleParameter(renderer, 0x1DE8)->setValue(psmSpriteField<void*>(renderer, 0x1DC8), count);
    return i;
}

// The identical six 64-byte vertex records in 1016FD50/101705F0. Packed signed UVs,
// rotation pairs and asymmetric top/bottom size values are copied without reinterpretation.
static void psmWriteParticleVertices(unsigned char* renderer, unsigned char* particle, unsigned char* output)
{
    float minimumSize = psmDrawField<float>(particle, 8);
    if (psmDrawField<float>(particle, 0xC) < minimumSize) minimumSize = psmDrawField<float>(particle, 0xC);
    const unsigned short id = psmDrawField<unsigned short>(particle, 4);
    const float descriptorIndex = static_cast<float>(static_cast<signed char>(psmSpriteField<unsigned char*>(renderer, 0x1DC0)[id]));
    const int corners[6][2] = { {-1,1}, {1,1}, {-1,-1}, {-1,-1}, {1,1}, {1,-1} };
    for (unsigned int i = 0; i < 6; ++i)
    {
        unsigned char* vertex = output + i * 64;
        ::memcpy(vertex, particle + 0x18, 16);
        psmDrawField<float>(vertex, 0x10) = descriptorIndex;
        ::memcpy(vertex + 0x14, particle + 0x28, 12);
        psmDrawField<float>(vertex, 0x20) = static_cast<float>(corners[i][0]) - psmDrawField<float>(particle, 0x34);
        psmDrawField<float>(vertex, 0x24) = static_cast<float>(corners[i][1]) - psmDrawField<float>(particle, 0x38);
        psmDrawField<float>(vertex, 0x28) = corners[i][1] > 0 ? psmDrawField<float>(particle, 0xC) : minimumSize;
        psmDrawField<unsigned int>(vertex, 0x2C) = psmDrawField<unsigned int>(particle, 0x10);
        ::memcpy(vertex + 0x30, particle + 0x3C, 12);
        psmDrawField<short>(vertex, 0x3C) = static_cast<short>(corners[i][0] > 0 ? 0x7FFF : 0x8000);
        psmDrawField<short>(vertex, 0x3E) = static_cast<short>(corners[i][1] > 0 ? 0x7FFF : 0x8000);
    }
}

static void psmParticleTechnique(unsigned char* renderer, bool additive)
{
    ParticleSystemManager* manager = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    static const char* const sortedNames[] = { "ParticleShowFill", "ParticleLow", "ParticleMedium", "ParticleHigh" };
    static const char* const additiveNames[] = { "ParticleShowFill", "AdditiveLow", "AdditiveHigh", "AdditiveHigh" };
    if (manager->m_shaderQuality <= 3)
    {
        const std::string technique((additive ? additiveNames : sortedNames)[manager->m_shaderQuality]);
        dx9rend::effectSetTechniqueByName(psmSpriteField<void*>(renderer, 0x1DD0), technique);
    }
}

static unsigned char* psmLockParticleVertices(unsigned char* renderer, unsigned int count, bool first)
{
    unsigned int& offset = psmSpriteField<unsigned int>(renderer, 0x5E14);
    unsigned int flags = first && !offset ? 10240 : 6144;
    if (count + offset >= psmSpriteField<unsigned int>(renderer, 0x1DB8))
    {
        offset = 0;
        flags = 10240;
    }
    return static_cast<unsigned char*>(dx9rend::vertexBufferLock(psmSpriteField<void*>(renderer, 0x1DCC), offset, count, flags));
}

static void psmSubmitParticleVertices(unsigned char* renderer, unsigned int vertices)
{
    dx9rend::rendererDrawPrimitive(vertices / 3, 4, psmSpriteField<unsigned int>(renderer, 0x5E14));
    psmSpriteField<unsigned int>(renderer, 0x5E14) += vertices;
}

// `counter` indexes the manager's draw counters (retail +0x8C44 + 4 * counter).
static void psmCountParticleDraw(unsigned int counter)
{
    // PE reloads the published manager after external buffer/shader calls.
    ++reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager)->m_drawCounters[counter];
}

// Texture/state switches flush the current vertices. A batch-boundary
// blend mismatch writes the OLD blend into the particle; this shipped quirk is intentional.
static bool psmDrawSortedParticles(unsigned char* renderer, float target)
{
    unsigned int& remaining = psmSpriteField<unsigned int>(renderer, 0x1C38);
    if (!remaining) return false;
    const int startIndex = psmSpriteField<int>(renderer, 4);
    unsigned char** table = psmSpriteField<unsigned char**>(renderer, 0x18);
    unsigned char* firstParticle = table[startIndex];
    unsigned short blend = psmDrawField<unsigned short>(firstParticle, 0x90);
    bool additive = blend != 0;
    psmParticleBlend(additive);
    void* texture = psmRecordPointer<void*>(firstParticle, 0);
    psmParticleParameter(renderer, 0x1DEC)->setValue(texture, 0);
    const unsigned int count = remaining;
    unsigned int batchEnd = psmBatchSortedParticles(renderer, 0, count);
    psmParticleTechnique(renderer, false);
    void* effect = psmSpriteField<void*>(renderer, 0x1DD0);
    void* buffer = psmSpriteField<void*>(renderer, 0x1DCC);
    dx9rend::effectBegin(effect);
    dx9rend::effectBeginPass(effect, 0);
    unsigned int vertices = 0;
    unsigned char* output = psmLockParticleVertices(renderer, batchEnd * 6, true);
    if (!output) psmParticleDrawDebug(668, "Failed to lock");
    ParticleSystemManager* manager = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    const bool forceSorted = manager->m_forceSortedParticleType != 0;          // +0x8BA8
    unsigned int done = 0;
    for (; done < count; ++done)
    {
        unsigned char* particle = table[startIndex - done];
        if (psmDrawField<float>(particle, 0x14) < target) break;
        if (done == batchEnd)
        {
            dx9rend::vertexBufferUnlock(buffer);
            psmSubmitParticleVertices(renderer, vertices);
            vertices = 0;
            batchEnd = psmBatchSortedParticles(renderer, done, count);
            if (psmRecordPointer<void*>(particle, 0) != texture)
            {
                psmParticleParameter(renderer, 0x1DEC)->setValue(psmRecordPointer<void*>(particle, 0), 0);
                texture = psmRecordPointer<void*>(particle, 0);
            }
            if (psmDrawField<unsigned short>(particle, 0x90) != blend)
            {
                psmDrawField<unsigned short>(particle, 0x90) = blend;
                psmParticleBlend(additive);
            }
            output = psmLockParticleVertices(renderer, (batchEnd - done) * 6, false);
            if (!output) psmParticleDrawDebug(724, "Failed to lock vbuffer");
            psmCountParticleDraw(0);   // +0x8C44
        }
        const bool textureChanged = psmRecordPointer<void*>(particle, 0) != texture;
        const bool blendChanged = forceSorted && psmDrawField<unsigned short>(particle, 0x90) != blend;
        if (textureChanged || blendChanged)
        {
            dx9rend::vertexBufferUnlock(buffer);
            psmSubmitParticleVertices(renderer, vertices);
            vertices = 0;
            output = psmLockParticleVertices(renderer, (batchEnd - done) * 6, false);
            if (!output) psmParticleDrawDebug(759, "Failed to lock vbuffer");
            if (textureChanged)
            {
                psmParticleParameter(renderer, 0x1DEC)->setValue(psmRecordPointer<void*>(particle, 0), 0);
                texture = psmRecordPointer<void*>(particle, 0);
            }
            if (blendChanged)
            {
                blend = psmDrawField<unsigned short>(particle, 0x90);
                additive = blend != 0;
                psmParticleBlend(additive);
            }
            psmCountParticleDraw(0);   // +0x8C44
        }
        psmWriteParticleVertices(renderer, particle, output + vertices * 64);
        vertices += 6;
        ++psmSpriteField<unsigned int>(renderer, 0x1C2C);
        --remaining;
    }
    dx9rend::vertexBufferUnlock(buffer);
    psmCountParticleDraw(0);   // +0x8C44
    psmSubmitParticleVertices(renderer, vertices);
    psmSpriteField<int>(renderer, 4) = startIndex - done;
    dx9rend::effectEnd(effect);
    if (remaining) psmParticleDrawDebug(909, "m_particleSortCount != 0");
    return true;
}

static bool psmDrawAdditiveParticles(unsigned char* renderer, unsigned int target,
    unsigned int count, PsmDrawGroup* groups)
{
    unsigned int particles;
    unsigned int batchEnd = psmBatchAdditiveParticles(renderer, 0, count, groups, particles);
    if (!particles) return false;
    unsigned char* instance = groups[0].instance;
    void* texture = psmParticleDescriptorTexture(psmInstanceA<unsigned char*>(instance, 0x2C));
    psmParticleParameter(renderer, 0x1DEC)->setValue(texture, 0);
    psmParticleBlend(true);
    psmParticleTechnique(renderer, true);
    void* effect = psmSpriteField<void*>(renderer, 0x1DD0);
    void* buffer = psmSpriteField<void*>(renderer, 0x1DCC);
    dx9rend::effectBegin(effect);
    dx9rend::effectBeginPass(effect, 0);
    unsigned int vertices = 0;
    unsigned char* output = psmLockParticleVertices(renderer, particles * 6, true);
    if (!output) psmParticleDrawDebug(973, "Failed to lock surface");
    for (unsigned int i = 0; i < count; ++i)
    {
        PsmDrawGroup* const group = groups + i;
        if (i == batchEnd)
        {
            dx9rend::vertexBufferUnlock(buffer);
            psmCountParticleDraw(2);   // +0x8C4C
            psmSubmitParticleVertices(renderer, vertices);
            vertices = 0;
            batchEnd = psmBatchAdditiveParticles(renderer, i, count, groups, particles);
            instance = group->instance;
            unsigned char* descriptor = psmInstanceA<unsigned char*>(instance, 0x2C);
            if (texture != psmParticleDescriptorTexture(descriptor))
            {
                texture = psmParticleDescriptorTexture(descriptor);
                psmParticleParameter(renderer, 0x1DEC)->setValue(texture, 0);
            }
            output = psmLockParticleVertices(renderer, particles * 6, false);
            if (!output) return false; // Retail has no Debug/effectEnd on this later failure.
        }
        const int index = group->last;
        const unsigned int available = group->count;
        unsigned int done = 0;
        for (; done < available; ++done)
        {
            unsigned char* particle = psmSpriteField<unsigned char**>(renderer, 0x1C)[index - done];
            if (psmDrawField<unsigned int>(particle, 0x14) < target) break;
            psmWriteParticleVertices(renderer, particle, output + vertices * 64);
            vertices += 6;
            --group->count;
            ++psmSpriteField<unsigned int>(renderer, 0x1C30);
        }
        group->last = index - static_cast<int>(done);
    }
    dx9rend::vertexBufferUnlock(buffer);
    if (vertices)
    {
        psmCountParticleDraw(2);   // +0x8C4C
        psmSubmitParticleVertices(renderer, vertices);
    }
    dx9rend::effectEnd(effect);
    return true;
}
