// [CHOSEN, not recovered] implementation split of the attested ParticleSystemManager.cpp.
// PE 100D3FB0/44A0/4500/3F30/4FD0/4820/5150/6310, read in full. Renderer-only helpers;
// the dedicated ELF exposes the caller interface, not these simulation bodies.

template<class T> T& psmRuntimeField(void* object, unsigned int offset)
{ return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset); }

std::set<IParticleSystemInstance*>& psmReclassification(ParticleSystemManager* self)
{ return self->m_pendingReclassification; }

void psmPush(ParticleSystemInstanceVector& list, IParticleSystemInstance* instance)
{ list.push_back(instance); }

// The renderer Debug stream is still a named boundary on the shipped invalid-state assertions.
// Valid scheduling does not pass through these sites. [FRONTIER]
void psmRuntimeError(ParticleSystemManager* self, const char* message)
{
    RD9_FRONTIER_AT(message, self, 0);
    std::abort();
}

// Add first, then classify. A trail forcibly clears both first-person flags.
void psmAddActive(ParticleSystemManager* self, IParticleSystemInstance* instance, bool firstPerson)
{
    if (psmInstance<unsigned char>(instance, 4))
        psmRuntimeError(self, "dice::hfe::geom::ParticleSystemManager::addActiveInstance "
                             "(Debug: Trying to add instance twice)");
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
        psmRuntimeError(self, "dice::hfe::geom::ParticleSystemManager::addActiveInstance "
                             "(Debug: Invalid template type)");
        return;
    }
    ParticleSystemInstanceVector& list = self->m_renderBuckets[bucket];
    if (!firstPerson || type == 2)
        for (ParticleSystemInstanceVector::iterator cursor = list.begin(); cursor != list.end(); ++cursor)
            if (*cursor == instance)
                psmRuntimeError(self, "dice::hfe::geom::ParticleSystemManager::addActiveInstance "
                                     "(Debug: Blah already added)");
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
void psmRemoveActive(ParticleSystemManager* self, bool preserveReclassification)
{
    for (IParticleSystemInstance** cursor = self->m_pendingClassification.data();
         cursor != self->m_pendingClassification.data() + self->m_pendingClassification.size();
         ++cursor)
    {
        IParticleSystemInstance* const instance = *cursor;
        int bucket;
        if (psmInstance<unsigned char>(instance, 6))
        {
            switch (instance->reserved01())
            {
            case 0: case 3: bucket = 7; break;
            case 1: bucket = 9; break;
            case 4: bucket = 8; break;
            case 2:
                psmRuntimeError(self, "dice::hfe::geom::ParticleSystemManager::removeActiveInstances "
                                     "(Debug: Trail should not be 1p)");
                return;
            default:
                psmRuntimeError(self, "dice::hfe::geom::ParticleSystemManager::removeActiveInstances "
                                     "(Debug: Invalid template type)");
                return;
            }
        }
        else
        {
            const bool water = instance->getWaterSide();
            switch (instance->reserved01())
            {
            case 0: case 3: bucket = 1 + water; break;
            case 1: bucket = 3 + water; break;
            case 2: bucket = 5 + water; break;
            case 4: bucket = 0; break;
            default:
                psmRuntimeError(self, "dice::hfe::geom::ParticleSystemManager::removeActiveInstances "
                                     "(Debug: Invalid template type)");
                return;
            }
        }
        if (!preserveReclassification)
            psmReclassification(self).erase(instance);
        ParticleSystemInstanceVector& list = self->m_renderBuckets[bucket];
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
    explicit PsmViewContext(dx9rend::IRenderView* value) : view(value)
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

// PE 10176A50/10178310/101799C0, with the identical aabb test at 10175C80. These three
// descriptor helpers differ only in their descriptor LOD-index offset (64/6C/68).
double psmCoverage(IParticleSystemInstance* instance, const PsmViewContext& context,
                   unsigned int lodOffset)
{
    const float* const data = reinterpret_cast<const float*>(instance);
    const Vec3& p = context.position;
    if (p.x >= data[63] && p.y >= data[64] && p.z >= data[65]
        && p.x <= data[66] && p.y <= data[67] && p.z <= data[68])
        return 1.0;
    const double height = static_cast<double>(data[67]) - data[64];
    const float h = static_cast<float>(height), depth = data[68] - data[65];
    const double radius = data[72] * static_cast<double>(
        height > static_cast<double>(data[66]) - data[63] && h > depth
            ? context.scaleY : context.scaleX);
    const float dx = p.x - data[69];
    const double dy = static_cast<double>(p.y) - data[70], dz = static_cast<double>(p.z) - data[71];
    const float distance = static_cast<float>(::sqrt(dz * dz + dy * dy + static_cast<double>(dx) * dx));
    void* const descriptor = psmInstance<void*>(instance, 0x2C);
    const int lod = psmDescriptor<int>(descriptor, lodOffset);
    ParticleSystemManager* const published = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    const float nearDistance = published->m_nearDistances[lod];
    double nearCoverage = 1.0;
    if (distance > nearDistance)
    {
        double amount = (static_cast<double>(distance) - nearDistance) * published->m_inverseNearRanges[lod];
        if (amount > 1.0) amount = 1.0;
        nearCoverage = (1.0 - amount) * (1.0 - amount);
    }
    const double result = radius / distance + nearCoverage;
    return result > 1.0 ? 1.0 : result;
}

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
