// [CHOSEN, not recovered] implementation split of the attested ParticleSystemManager.cpp.
// PE 100D3FB0/44A0/4500/3F30/4FD0/4820/5150/6310, read in full. Renderer-only helpers;
// the dedicated ELF exposes the caller interface, not these simulation bodies.

template<class T> T& psmRuntimeField(void* object, unsigned int offset)
{ return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset); }

// PE 0x1017A2B0. Grow the instance bounds {min x, y, z, max x, y, z} to include a point: per axis the
// maximum is tested before the minimum, with ordered compares, so NaN never moves a bound.
void psmIncludeBoundsPoint(float* bounds, float x, float y, float z)
{
    if (x > bounds[3]) bounds[3] = x;
    if (x < bounds[0]) bounds[0] = x;
    if (y > bounds[4]) bounds[4] = y;
    if (y < bounds[1]) bounds[1] = y;
    if (z > bounds[5]) bounds[5] = z;
    if (z < bounds[2]) bounds[2] = z;
}

std::set<IParticleSystemInstance*>& psmReclassification(ParticleSystemManager* self)
{ return self->m_pendingReclassification; }

// [INFERRED] Private name for the existing guarded reclassification insertion.
// PE 0x100D3650
void psmQueueReclassification(ParticleSystemManager* self,IParticleSystemInstance* instance)
{
    if (psmInstance<unsigned char>(instance,4))
        psmReclassification(self).insert(instance);
}

void psmPush(ParticleSystemInstanceVector& list, IParticleSystemInstance* instance)
{ list.push_back(instance); }

// Shared source-level spelling for the type-6 diagnostics in PE 0x100D3FB0/0x100D4500/0x100D4820.
void psmRuntimeError(unsigned int line, const char* message)
{
    ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\ParticleSystem\\ParticleSystemManager.cpp",
        static_cast<int>(line), "") << message;
}

// Add first, then classify. A trail forcibly clears both first-person flags.
void psmAddActive(ParticleSystemManager* self, IParticleSystemInstance* instance, bool firstPerson)
{
    if (psmInstance<unsigned char>(instance, 4))
        psmRuntimeError(555, "Trying to add instance twice");
    if (instance->getEffectController())
        instance->getEffectController()->addNumEmittersPlaying();
    psmInstance<unsigned char>(instance, 4) = 1;
    psmInstance<unsigned char>(instance, 6) = firstPerson;
    psmInstance<unsigned char>(instance, 7) = firstPerson;
    const bool water = instance->getWaterSide();
    const int type = instance->reserved01();
    int bucket;
    switch (type)
    {
    case 0: case 3: bucket = firstPerson ? 7 : 1 + water; break;
    case 1: bucket = firstPerson ? 9 : 3 + water; break;
    case 2: bucket = 5 + water; break;
    case 4: bucket = firstPerson ? 8 : 0; break;
    default:
        psmRuntimeError(634, "trying to push unknown instance");
        return;
    }
    ParticleSystemInstanceVector& list = self->m_renderBuckets[bucket];
    if (!firstPerson || type == 2)
        for (ParticleSystemInstanceVector::iterator cursor = list.begin(); cursor != list.end(); ++cursor)
            if (*cursor == instance)
                psmRuntimeError(type == 0 || type == 3 ? 579 : type == 1 ? 596 : type == 2 ? 609 : 621,
                                "Blah already added");
    psmPush(list, instance);
    if (type == 2)
    {
        psmInstance<unsigned char>(instance, 6) = 0;
        psmInstance<unsigned char>(instance, 7) = 0;
    }
}

// PE 100D44A0 has no deduplication or flag write.
void psmQueueInactive(ParticleSystemManager* self, IParticleSystemInstance* instance)
{ psmPush(self->m_pendingClassification, instance); }

// Pending-list end and selected bucket storage are reread after callbacks.
// PE 0x100D4500
void psmRemoveActive(ParticleSystemManager* self, bool preserveReclassification)
{
    // PE keeps ESI across pending entries. A suppressed first-person-trail error reuses it.
    ParticleSystemInstanceVector* selected = 0;
    for (IParticleSystemInstance** cursor = self->m_pendingClassification.data();
         cursor != self->m_pendingClassification.data() + self->m_pendingClassification.size();
         ++cursor)
    {
        IParticleSystemInstance* const instance = *cursor;
        if (psmInstance<unsigned char>(instance, 6))
        {
            switch (instance->reserved01())
            {
            case 0: case 3: selected = &self->m_renderBuckets[7]; break;
            case 1: selected = &self->m_renderBuckets[9]; break;
            case 4: selected = &self->m_renderBuckets[8]; break;
            case 2:
                // Shipped defect: a suppressed diagnostic keeps the prior (possibly null) bucket.
                psmRuntimeError(735, "Trail should not be 1p");
                break;
            default:
                psmRuntimeError(743, "trying to push unknown instance");
                return;
            }
        }
        else
        {
            const bool water = instance->getWaterSide();
            switch (instance->reserved01())
            {
            case 0: case 3: selected = &self->m_renderBuckets[1 + water]; break;
            case 1: selected = &self->m_renderBuckets[3 + water]; break;
            case 2: selected = &self->m_renderBuckets[5 + water]; break;
            case 4: selected = &self->m_renderBuckets[0]; break;
            default:
                psmRuntimeError(717, "trying to push unknown instance");
                return;
            }
        }
        if (!preserveReclassification)
            psmReclassification(self).erase(instance);
        ParticleSystemInstanceVector& list = *selected;
        for (unsigned int i = 0; i < list.size(); ++i)
            if (list.data()[i] == instance)
            {
                psmInstance<unsigned char>(instance, 4) = 0;
                if (instance->getEffectController())
                    instance->getEffectController()->decNumEmittersPlaying();
                list.data()[i] = list.back();
                if (list.size()) list.pop_back();
                break;
            }
    }
    self->m_pendingClassification.clear();
}

// destroy only entries still present in the master list; duplicates are harmless.
void psmDrainDestroy(ParticleSystemManager* self)
{
    for (IParticleSystemInstance** cursor = self->m_pendingDestroy.data();
         cursor != self->m_pendingDestroy.data() + self->m_pendingDestroy.size(); ++cursor)
    {
        IParticleSystemInstance* const instance = *cursor;
        for (unsigned int i = 0; i < self->m_instances.size(); ++i)
            if (self->m_instances[i] == instance)
            {
                self->m_instances[i] = self->m_instances.back();
                self->m_instances.pop_back();
                if (instance) instance->destroy();
                break;
            }
    }
    self->m_pendingDestroy.clear();
}

// PE 0x100D4FD0
void psmReclassify(ParticleSystemManager* self)
{
    std::set<IParticleSystemInstance*>& pending = psmReclassification(self);
    for (std::set<IParticleSystemInstance*>::iterator n = pending.begin(); n != pending.end(); ++n)
        psmPush(self->m_pendingClassification, *n);
    psmRemoveActive(self, true);
    for (std::set<IParticleSystemInstance*>::iterator n = pending.begin(); n != pending.end(); ++n)
    {
        IParticleSystemInstance* const instance = *n;
        psmAddActive(self, instance, psmInstance<unsigned char>(instance, 7) != 0);
    }
    pending.clear();
}

// reverse snapshot traversal, reread storage per element, skip only byte +5.
// PE 0x100D6310
void psmUpdateBucket(ParticleSystemInstanceVector& list, float dt)
{
    for (int i = static_cast<int>(list.size()) - 1; i >= 0; --i)
    {
        IParticleSystemInstance* const instance = list.data()[i];
        if (!psmInstance<unsigned char>(instance, 5)) instance->reserved41(dt);
    }
}

struct PsmViewContext
{
    dx9rend::IRenderView* view;
    Vec3 position;
    float scaleX, scaleY;
    // PE 0x100D5D20
    explicit PsmViewContext(dx9rend::IRenderView* value)
        : view(value), position{0,0,0}, scaleX(0.0f), scaleY(0.0f)
    {
        const float* const matrix = reinterpret_cast<const float*>(view->getViewMatrixPtr());
        position.x = matrix[12]; position.y = matrix[13]; position.z = matrix[14];
        const float halfFov = static_cast<float>(static_cast<double>(view->getFov()) * 0.5);
        scaleX = static_cast<float>(1.0 / ::tan(static_cast<double>(view->getFov()) * 0.5));
        scaleY = static_cast<float>(1.0 / ::tan(static_cast<double>(halfFov)));
        const float scaledAspect = static_cast<float>(static_cast<double>(view->getAspect()) * 1.05f);
        const float x = static_cast<float>(static_cast<double>(scaledAspect) / view->getFov());
        const double y = static_cast<double>(1.05f) / view->getFov();
        scaleX = static_cast<float>(static_cast<double>(x) * scaleX);
        scaleY = static_cast<float>(y * scaleY);
    }
};

// PE 0x10175C80
bool psmCoverageContainsPoint(const Vec3& low, const Vec3& high, const Vec3& point)
{
    return point.x>=low.x && point.y>=low.y && point.z>=low.z
        && point.x<=high.x && point.y<=high.y && point.z<=high.z;
}

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] Return the live x87 expression to the caller's float store.
using PsmCoverageReal=bf2x87::Real;
#else
using PsmCoverageReal=double;
#endif

// PE 0x10176A50 / PE 0x10178310 / PE 0x101799C0. These three
// descriptor helpers differ only in their descriptor LOD-index offset (64/6C/68).
// boundsOffset is the retail offset of the instance's low corner: the high corner follows at +0xC, the
// centre at +0x18 and the bounding radius at +0x24 (the trail instance at 0xF8 uses the same layout).
PsmCoverageReal psmCoverage(IParticleSystemInstance* instance, const PsmViewContext& context,
                   unsigned int lodOffset, unsigned int boundsOffset = 0xFC)
{
    const Vec3& low=psmInstance<Vec3>(instance,boundsOffset);
    const Vec3& high=psmInstance<Vec3>(instance,boundsOffset+0xC);
    const Vec3& center=psmInstance<Vec3>(instance,boundsOffset+0x18);
    const Vec3& p = context.position;
    if (psmCoverageContainsPoint(low,high,p))
        return 1.0;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    using bf2x87::wide;
#else
    const auto wide=[](double value) { return value; };
#endif
    const PsmCoverageReal width=wide(high.x)-wide(low.x),height=wide(high.y)-wide(low.y);
    const float h=static_cast<float>(height),depth=static_cast<float>(wide(high.z)-wide(low.z));
    const PsmCoverageReal radius=wide(psmInstance<float>(instance,boundsOffset+0x24))*wide(
        height > width && h > depth
            ? context.scaleY : context.scaleX);
    const float dx=static_cast<float>(wide(p.x)-wide(center.x));
    const PsmCoverageReal dy=wide(p.y)-wide(center.y),dz=wide(p.z)-wide(center.z);
    const PsmCoverageReal squared=dz*dz+dy*dy+wide(dx)*wide(dx);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const float distance=static_cast<float>(bf2x87::squareRoot(squared));
#else
    const float distance=static_cast<float>(::sqrt(squared));
#endif
    void* const descriptor = psmInstance<void*>(instance, 0x2C);
    const int lod = psmDescriptor<int>(descriptor, lodOffset);
    ParticleSystemManager* const published = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    const PsmCoverageReal projected=radius/wide(distance);
    const float nearDistance = published->m_nearDistances[lod];
    PsmCoverageReal nearCoverage=wide(1);
    if (distance > nearDistance)
    {
        PsmCoverageReal amount=(wide(distance)-wide(nearDistance))*wide(published->m_inverseNearRanges[lod]);
        if (amount > wide(1)) amount=wide(1);
        const PsmCoverageReal remaining=wide(1)-amount;
        nearCoverage=remaining*remaining;
    }
    const PsmCoverageReal result=projected+nearCoverage;
    return result>wide(1) ? wide(1) : result;
}

// PE 0x10187B00. The trail instance's copy of the coverage helper: its bounds start at +0xF8 and its
// descriptor LOD index is read at +0x68. Unreachable in 1.50: no code or data references. The name is
// [CHOSEN, not recovered].
PsmCoverageReal psmTrailCoverage(IParticleSystemInstance* instance, const PsmViewContext& context)
{
    return psmCoverage(instance, context, 0x68, 0xF8);
}

// PE 0x100D5DE0, 0x100D5E40, 0x100D5EA0: the ordinary, NSA and mesh copies (each calls its own
// psmCoverage copy and stores +0x134 / +0x134 / +0x12C).
void psmPrepareBucket(ParticleSystemInstanceVector& list, const PsmViewContext& context, int type)
{
    IParticleSystemInstance** const end = list.data() + list.size();
    for (IParticleSystemInstance** cursor = list.data(); cursor != end; ++cursor)
        if ((*cursor)->reserved01() == type)
        {
            IParticleSystemInstance* const instance = *cursor;
            if (instance->reserved10() && instance->reserved05())
                psmInstance<float>(instance, type == 1 ? 0x12C : 0x134) = static_cast<float>(
                    psmCoverage(instance, context, type == 0 ? 0x64 : type == 4 ? 0x6C : 0x68));
        }
}

// PE 0x100D3350
void psmPrepareClassifiedVectors(ParticleSystemManager* self, dx9rend::IRenderView* view)
{
    const PsmViewContext context(view);
    psmPrepareBucket(self->m_renderBuckets[2], context, 0);
    psmPrepareBucket(self->m_renderBuckets[1], context, 0);
    psmPrepareBucket(self->m_renderBuckets[7], context, 0);
    psmPrepareBucket(self->m_renderBuckets[0], context, 4);
    psmPrepareBucket(self->m_renderBuckets[8], context, 4);
    psmPrepareBucket(self->m_renderBuckets[4], context, 1);
    psmPrepareBucket(self->m_renderBuckets[3], context, 1);
    psmPrepareBucket(self->m_renderBuckets[9], context, 1);
}
