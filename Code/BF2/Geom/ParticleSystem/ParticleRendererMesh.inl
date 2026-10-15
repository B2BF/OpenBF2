// Draws mesh particles through retained MeshParticleMesh instances.
// Included inside the manager's anonymous namespace after the shared draw helpers;
// MeshParticleMesh headers must be included outside that namespace.
inline void meshParticleMeshSetAgeAndAlpha(unsigned char* mesh, float age, float alpha,
                                           unsigned int slot)
{
    reinterpret_cast<MeshParticleMesh*>(mesh)->setAgeAndAlpha(age, alpha, static_cast<int>(slot));
}
inline void meshParticleMeshDrawBatch(unsigned char* mesh, int technique, bool transparentPass)
{
    reinterpret_cast<MeshParticleMesh*>(mesh)->draw(technique, transparentPass);
}
// Mesh +0x44 is the selected (lod, geom) record; its first word is the number of bone slots.
inline unsigned int meshParticleMeshLodPartCount(unsigned char* mesh)
{
    return reinterpret_cast<MeshParticleMesh*>(mesh)->m_lodRecord->partCount;
}

template<typename Fn>
static Fn psmMeshGeometrySlot(void* mesh, unsigned int offset)
{
    return reinterpret_cast<Fn>((*static_cast<void***>(mesh))[offset / 4]);
}

// +0x38, PE 10145640 `mov eax,[ecx+84h]; retn` - the geometry template (IGeometry::getTemplate,
// the same slot StaticMesh names). Only 10175230 calls it, and discards the result.
static void* psmMeshGeometryGetTemplate(void* mesh)
{
    typedef void* (__thiscall *Fn)(void*);
    return psmMeshGeometrySlot<Fn>(mesh, 0x38)(mesh);
}

// +0x4C, PE 101AFD80 `this[+38h][part] = transform`, `retn 8` - BundledMesh's
// setPartTransformation(const Mat4*, int) shape. The pointer is stored, not copied.
static void psmMeshGeometrySetPartTransformation(void* mesh, const Mat4* transform, int part)
{
    typedef void (__thiscall *Fn)(void*, const Mat4*, int);
    psmMeshGeometrySlot<Fn>(mesh, 0x4C)(mesh, transform, part);
}

// +0x50, PE 101AFC20 `this->vslot4C(transform, 0)`, `retn 4` - setGeometryTransformation.
static void psmMeshGeometrySetTransformation(void* mesh, const Mat4* transform)
{
    typedef void (__thiscall *Fn)(void*, const Mat4*);
    psmMeshGeometrySlot<Fn>(mesh, 0x50)(mesh, transform);
}

// The renderer calls ShaderParameter through its vtable (`call [reg+18h]`, +24h, +28h). Upcast so
// the call stays a genuine virtual dispatch rather than a devirtualised call on the final class.
static dx9rend::ParameterOwner* psmMeshParameter(dx9rend::ShaderParameter* parameter)
{
    return parameter;
}


// Math helpers private to the mesh particle transform. None of these PE bodies exists in the tree.


// cross(a, b). Register-passed (ecx = result, edx = a, [esp] = b; ltcg). Each
// component is x87: two exact products, one double subtraction, one float rounding.
static Vec3 psmMeshCross(const Vec3& a, const Vec3& b)
{
    Vec3 result;
    result.x = static_cast<float>(static_cast<double>(b.z) * a.y - static_cast<double>(b.y) * a.z);
    result.y = static_cast<float>(static_cast<double>(b.x) * a.z - static_cast<double>(b.z) * a.x);
    result.z = static_cast<float>(static_cast<double>(b.y) * a.x - static_cast<double>(b.x) * a.y);
    return result;
}

// rows 0..2 = the three axes, row 3 = translation, fourth column (0,0,0,1).
static void psmMeshSetRows(Mat4& matrix, const Vec3& row0, const Vec3& row1, const Vec3& row2,
                           const Vec3& row3)
{
    matrix.m[0] = row0.x;  matrix.m[1] = row0.y;  matrix.m[2] = row0.z;  matrix.m[3] = 0.0f;
    matrix.m[4] = row1.x;  matrix.m[5] = row1.y;  matrix.m[6] = row1.z;  matrix.m[7] = 0.0f;
    matrix.m[8] = row2.x;  matrix.m[9] = row2.y;  matrix.m[10] = row2.z; matrix.m[11] = 0.0f;
    matrix.m[12] = row3.x; matrix.m[13] = row3.y; matrix.m[14] = row3.z; matrix.m[15] = 1.0f;
}

// the four-row constructor - all sixteen floats zeroed, then 101744A0.
static void psmMeshConstructFromRows(Mat4& matrix, const Vec3& row0, const Vec3& row1,
                                     const Vec3& row2, const Vec3& row3)
{
    for (unsigned int i = 0; i < 16; ++i)
        matrix.m[i] = 0.0f;
    psmMeshSetRows(matrix, row0, row1, row2, row3);
}

// scale column j of the 3x3 part by s[j]; write order m0,m4,m8,m1,m5,m9,m2,m6,m10.
static void psmMeshScaleColumns(Mat4& matrix, const Vec3& scale)
{
    matrix.m[0] = scale.x * matrix.m[0];
    matrix.m[4] = matrix.m[4] * scale.x;
    matrix.m[8] = scale.x * matrix.m[8];
    matrix.m[1] = scale.y * matrix.m[1];
    matrix.m[5] = scale.y * matrix.m[5];
    matrix.m[9] = scale.y * matrix.m[9];
    matrix.m[2] = scale.z * matrix.m[2];
    matrix.m[6] = matrix.m[6] * scale.z;
    matrix.m[10] = matrix.m[10] * scale.z;
}

// PE 100794D0: `*this *= rhs` - copy-construct (10030DD0) then the affine product 10077C50, which
// this tree already recovers as Mat4::mult (the particle instances use it for 10077C50 too).
static void psmMeshMultiplyInPlace(Mat4& matrix, const Mat4& rhs)
{
    const Mat4 copy = matrix;
    matrix.mult(copy, rhs);
}

// Four stable byte-radix passes over the +0xBC squared-distance bits, buckets at
// renderer +0x1814, chains through the particle's first word; identical in shape to 1016F7F0.
static void psmSortMeshParticles(unsigned char* renderer, unsigned char** table, unsigned int count)
{
    unsigned char** buckets = &psmMeshField<unsigned char*>(renderer, 0x1814);
    for (unsigned int byte = 0; byte < 4; ++byte)
    {
        for (int i = static_cast<int>(count) - 1; i >= 0; --i)
        {
            unsigned char* particle = table[i];
            const unsigned int key = particle[0xBC + byte];
            psmRecordLink(particle, 1) = buckets[key];
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
                particle = static_cast<unsigned char*>(psmRecordLink(particle, 1));
            }
        }
    }
}

// PE 10175860 (1002 B, `retn 10h`). Builds the particle's world matrix at +0x04 once per dirty
// particle. The instance and view arguments are present but never read (shipped).
// Descriptor (type-1 template) fields: +0xFC rotation graph, +0x13C transparency graph,
// +0x14C size graph (each a*t^3 + b*t^2 + c*t + d over the normalised age), +0x1E8 particle max
// size, +0x1EC directional scale, +0x1F0 align-rotation-to-speed.
static void psmUpdateMeshParticleDrawState(unsigned char* renderer, unsigned char* particle,
    unsigned char* descriptor, unsigned char*, dx9rend::IRenderView*)
{
    unsigned int& flags = psmDrawField<unsigned int>(particle, 0x60);
    if (!(flags & 0x80)) return;   // `test al,al; jns`: bit 7 of the low byte is the dirty flag
    flags &= ~0x80u;
    const double age = psmDrawField<float>(particle, 0x5C);
    const float rate = static_cast<float>(age * age * psmDescriptor1<float>(descriptor, 0x100)
        + age * psmDescriptor1<float>(descriptor, 0x104)
        + age * age * age * psmDescriptor1<float>(descriptor, 0xFC)
        + psmDescriptor1<float>(descriptor, 0x108));
    const float step = psmParticleClamp(rate, 0.0f, 1.0f) * psmMeshField<float>(renderer, 0x1C20);
    Mat4& matrix = psmDrawField<Mat4>(particle, 4);
    const Vec3& position = psmDrawField<Vec3>(particle, 0x50);

    // 101758C5/101758E1: `fucompp; test ah,44h; jp` - a NaN directional scale takes the velocity arm.
    if (!descriptor[0x1F0] && psmDescriptor1<float>(descriptor, 0x1EC) == 0.0f)
    {
        Vec3& rotation = psmDrawField<Vec3>(particle, 0xA8);
        const Vec3& rotationSpeed = psmDrawField<Vec3>(particle, 0x9C);
        // the Z step alone is rounded to float (fstp) before it is accumulated; the X
        // and Y steps stay on the x87 stack. Kept.
        const float stepZ = static_cast<float>(static_cast<double>(step) * rotationSpeed.z);
        rotation.x = static_cast<float>(static_cast<double>(step) * rotationSpeed.x + rotation.x);
        rotation.y = static_cast<float>(static_cast<double>(step) * rotationSpeed.y + rotation.y);
        rotation.z = static_cast<float>(static_cast<double>(stepZ) + rotation.z);
        // PE 10078730 is RendDX9's copy of dice::hfe::setRotationZXY (Math/Mat4.cpp): three
        // fsincos rotations, Z/X/Y by the non-zero-angle mask, only the nine 3x3 terms written.
        // So m[3]/m[7]/m[11]/m[15] keep whatever the pool record last held. Kept.
        setRotationZXY(matrix, -rotation.z, -rotation.y, -rotation.x);
        matrix.m[12] = position.x;
        matrix.m[13] = position.y;
        matrix.m[14] = position.z;
    }
    else
    {
        const Vec3& velocity = psmDrawField<Vec3>(particle, 0x84);
        Vec3 up = { 0.0f, 1.0f, 0.0f };
        Vec3 right;
        Vec3 direction;
        const double length = std::sqrt(static_cast<double>(velocity.x) * velocity.x
            + static_cast<double>(velocity.y) * velocity.y
            + static_cast<double>(velocity.z) * velocity.z);
        // 101759D0 `fst [ebp+arg_0]` overwrites the step slot: this arm never uses the step.
        const float lengthRounded = static_cast<float>(length);
        // The unrounded length is tested (FLT_EPSILON, 1021F0A8); NaN takes the normalise arm.
        if (std::fabs(length) < 1.1920929e-7f)
        {
            // identity written element by element, then the translation.
            matrix = Mat4::getIdentity();
            matrix.m[13] = position.y;
            matrix.m[12] = position.x;
            matrix.m[14] = position.z;
        }
        else
        {
            // The reciprocal of the rounded length stays unrounded on the x87 stack.
            const double inverse = 1.0 / static_cast<double>(lengthRounded);
            direction.x = static_cast<float>(inverse * velocity.x);
            direction.y = static_cast<float>(static_cast<double>(velocity.y) * inverse);
            direction.z = static_cast<float>(static_cast<double>(velocity.z) * inverse);
            // 1021F0E4 = 0.0001f; `test ah,5; jp` sends NaN to the general arm.
            if (std::fabs(direction.x) < 0.0001f && std::fabs(direction.z) < 0.0001f)
            {
                // Vertical motion: a fixed frame. `test ah,41h; jnz` puts dir.y <= 0 AND NaN on
                // the downward arm.
                const bool upward = direction.y > 0.0f;
                up.y = 0.0f; up.x = 0.0f;
                right.z = 0.0f; right.y = 0.0f; right.x = 1.0f;
                direction.z = 0.0f; direction.x = 0.0f;
                direction.y = upward ? 1.0f : -1.0f;
                up.z = upward ? -1.0f : 1.0f;
            }
            else
            {
                right = psmMeshCross(up, direction);
                psmNormalize3(&right.x);
                up = psmMeshCross(direction, right);
            }
            // `test ah,41h; jnz`: only a strictly positive directional scale stretches (NaN no).
            if (psmDescriptor1<float>(descriptor, 0x1EC) > 0.0f)
            {
                matrix = Mat4::getIdentity();
                const Vec3 stretch = { 1.0f, 1.0f, lengthRounded * psmDescriptor1<float>(descriptor, 0x1EC) };
                psmMeshScaleColumns(matrix, stretch);
                Mat4 basis;
                psmMeshConstructFromRows(basis, right, up, direction, position);
                psmMeshMultiplyInPlace(matrix, basis);
            }
            else
                psmMeshSetRows(matrix, right, up, direction, position);
        }
    }

    // the size graph scales the whole 3x3 part uniformly, then the transparency graph
    // multiplies the alpha the collector faded.
    const double tailAge = psmDrawField<float>(particle, 0x5C);
    const float sizeRate = static_cast<float>(tailAge * tailAge * psmDescriptor1<float>(descriptor, 0x150)
        + tailAge * psmDescriptor1<float>(descriptor, 0x154)
        + tailAge * tailAge * tailAge * psmDescriptor1<float>(descriptor, 0x14C)
        + psmDescriptor1<float>(descriptor, 0x158));
    const float size = static_cast<float>(
        static_cast<double>(psmParticleClamp(sizeRate, 0.0f, 1.0f)) * psmDescriptor1<float>(descriptor, 0x1E8)
        + psmDrawField<float>(particle, 0x68));
    const Vec3 uniform = { size, size, size };
    psmMeshScaleColumns(matrix, uniform);
    const float alphaRate = static_cast<float>(tailAge * tailAge * psmDescriptor1<float>(descriptor, 0x140)
        + tailAge * psmDescriptor1<float>(descriptor, 0x144)
        + tailAge * tailAge * tailAge * psmDescriptor1<float>(descriptor, 0x13C)
        + psmDescriptor1<float>(descriptor, 0x148));
    psmDrawField<float>(particle, 0x6C) =
        psmParticleClamp(alphaRate, 0.0f, 1.0f) * psmDrawField<float>(particle, 0x6C);
}

// PE 10174B00 (667 B, `retn 0Ch`). Fades, culls and collects one type-1 instance into the
// renderer table (+4), then admits it as a {instance, lastIndex, count} group.
static void psmCollectMeshParticles(unsigned char* renderer, dx9rend::IRenderView* view,
    unsigned char* instance, unsigned char* descriptor)
{
    view->getViewMatrixPtr();                   // discarded
    const float nearPlane = view->getNearPlane();
    view->getViewMatrixPtr();                   // discarded (getViewport below likewise)
    const unsigned int count = psmInstanceB<unsigned int>(instance, 0x28);
    if (psmDescriptor1<int>(descriptor, 8) == -1)
    {
        // renderer +0x1C38 is tested and both arms store the same counter. The retail
        // constructor initialises neither +0x1C38 nor the counter +0x1D44 (the rebuild's
        // zero-filled manager makes both 0). Kept as the dead test it is.
        if (psmMeshField<unsigned int>(renderer, 0x1C38))
            psmDescriptor1<unsigned int>(descriptor, 8) = psmMeshField<unsigned int>(renderer, 0x1D44);
        else
            psmDescriptor1<unsigned int>(descriptor, 8) = psmMeshField<unsigned int>(renderer, 0x1D44);
        ++psmMeshField<unsigned int>(renderer, 0x1D44);
    }
    view->getViewport();                        // discarded
    ParticleSystemManager* manager = reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    const unsigned int lod = psmDescriptor1<unsigned int>(descriptor, 0x68);
    const float farDistance = manager->m_farDistances[lod];
    const float inverseFarRange = manager->m_inverseFarRanges[lod];
    unsigned int accepted = 0;
    for (unsigned int i = 0; i < count; ++i)
    {
        unsigned char* particle = psmInstanceB<unsigned char**>(instance, 0x14)[i];
        const double depth = static_cast<double>(psmDrawField<float>(particle, 0x54)) * psmMeshField<float>(renderer, 0x1C28)
            + static_cast<double>(psmDrawField<float>(particle, 0x58)) * psmMeshField<float>(renderer, 0x1C2C)
            + static_cast<double>(psmDrawField<float>(particle, 0x50)) * psmMeshField<float>(renderer, 0x1C24)
            + psmMeshField<float>(renderer, 0x1C30);
        const float roundedDepth = static_cast<float>(depth);
        // The near test compares the unrounded depth (`fst` keeps st0), the far fade below uses
        // the rounded copy. `test ah,5; jnp` lets NaN through.
        if (depth < nearPlane) continue;
        float& alpha = psmDrawField<float>(particle, 0x6C);
        if (!(roundedDepth > farDistance)) alpha = psmDrawField<float>(particle, 0x70);
        else
        {
            double fade = (static_cast<double>(roundedDepth) - farDistance) * inverseFarRange;
            if (fade > 1) fade = 1;
            alpha = static_cast<float>((1 - fade) * (1 - fade) * psmDrawField<float>(particle, 0x70));
        }
        if (alpha < psmDescriptor1<float>(descriptor, 0xC4)) continue;   // alpha cull
        const Mat4* camera = view->getViewMatrixPtr();
        const float x = static_cast<float>(static_cast<double>(camera->m[12]) - psmDrawField<float>(particle, 0x50));
        const double y = static_cast<double>(camera->m[13]) - psmDrawField<float>(particle, 0x54);
        const double z = static_cast<double>(camera->m[14]) - psmDrawField<float>(particle, 0x58);
        psmDrawField<float>(particle, 0xBC) = static_cast<float>(z * z + y * y + static_cast<double>(x) * x);
        psmUpdateMeshParticleDrawState(renderer, particle, descriptor, instance, view);
        // The template id is written after the draw-state update, unlike the sprite collectors.
        psmDrawField<unsigned short>(particle, 0x64) = psmDescriptor1<unsigned short>(descriptor, 4);
        psmMeshField<unsigned char**>(renderer, 4)[psmMeshField<unsigned int>(renderer, 0x1808) + accepted] = particle;
        ++accepted;
    }
    if (!accepted) return;
    // Draw order 0 -> group 0 at +8 (drawn by the pass=true draw); anything else -> group 1 at
    // +0xC08 (drawn by the pass=false draw). A full group (256) returns without claiming the
    // table slots it just wrote; those particles' dirty bits are already consumed. Kept.
    const bool second = psmDescriptor1<unsigned int>(descriptor, 0x94) != 0;
    unsigned int& groupCount = psmMeshField<unsigned int>(renderer, second ? 0x1810 : 0x180C);
    if (groupCount >= 256) return;
    PsmDrawGroup* const group = psmMeshGroups(renderer, second ? 0xC08 : 8) + groupCount;
    group->instance = instance;
    group->last = static_cast<int>(psmMeshField<unsigned int>(renderer, 0x1808) + accepted - 1);
    group->count = accepted;
    ++groupCount;
    psmSortMeshParticles(renderer,
        psmMeshField<unsigned char**>(renderer, 4) + psmMeshField<unsigned int>(renderer, 0x1808), accepted);
    psmMeshField<unsigned int>(renderer, 0x1808) += accepted;
}

// PE 10175025..10175086, the group-overflow arm of 10174DA0. byte_1023A29C is a process-wide
// once-flag initialised to 1 and referenced nowhere else. The walk calls getTemplate (+0x8C) on
// every admitted group's instance and discards each result - the remains of a compiled-out
// diagnostic. The caller then abandons the rest of the instance list (shipped).
static void psmNoteMeshGroupOverflow(unsigned char* renderer)
{
    static bool s_reportGroupOverflow = true;   // [INFERRED] name, PE .data 1023A29C = 1
    if (!s_reportGroupOverflow) return;
    s_reportGroupOverflow = false;
    for (unsigned int i = 0; i < psmMeshField<unsigned int>(renderer, 0x180C); ++i)
        psmMeshField<IParticleSystemInstance*>(renderer, 8 + 12 * i)->getTemplate();
    for (unsigned int i = 0; i < psmMeshField<unsigned int>(renderer, 0x1810); ++i)
        psmMeshField<IParticleSystemInstance*>(renderer, 0xC08 + 12 * i)->getTemplate();
}

// PE 10175660 (402 B). Per-frame MeshParticleMesh effect state. Unlike the sprite renderers there
// is no View/Proj pair, no water fog, no shader-quality sun override, no technique and no stream
// binding here: drawBatch owns those. HemiMapInfo/HemiShadowAltitude/Texture1 are looked up BY
// name through the uncounted sub_1001E3A0, each temporary string living until after its setter.
static void psmBindMeshParticleParameters(unsigned char* renderer)
{
    if (g_meshParticleMeshEffectState.worldViewProjection)
        psmMeshParameter(g_meshParticleMeshEffectState.worldViewProjection)->setValue(renderer + 0x1CBC, 0);
    HemiMap* map = g_HemiMapManager->getBaseHemiMap();
    if (map)
    {
        {
            const std::string name("HemiMapInfo");
            dx9rend::ParameterOwner* parameter = psmMeshParameter(
                dx9rend::effectGetShaderParameterBorrowedTyped(g_meshParticleMeshEffectState.effect, name));
            const HemiMapInfo& info = g_HemiMapManager->getHemiMapInfo(map);
            parameter->setValue(&info, 0);
        }
        {
            const std::string name("HemiShadowAltitude");
            psmMeshParameter(dx9rend::effectGetShaderParameterBorrowedTyped(g_meshParticleMeshEffectState.effect, name))
                ->setFloat(map->falloff);
        }
        {
            const std::string name("Texture1");
            psmMeshParameter(dx9rend::effectGetShaderParameterBorrowedTyped(g_meshParticleMeshEffectState.effect, name))
                ->setValue(map->texture, 0);
        }
    }
    else
    {
        const std::string name("Texture1");
        dx9rend::ParameterOwner* parameter = psmMeshParameter(
            dx9rend::effectGetShaderParameterBorrowedTyped(g_meshParticleMeshEffectState.effect, name));
        // PE 100F1490 rereads the owner despite the null result above (as in the siblings).
        HemiMap* fallback = g_HemiMapManager->getBaseHemiMap();
        parameter->setValue(fallback ? fallback->texture : 0, 0);
    }
    // These four statics are dereferenced without a null test (only WorldViewProjection above
    // and the three colour words in the draws are tested). Shipped.
    psmMeshParameter(g_meshParticleMeshEffectState.fogRange)->setValue(dx9rend::raShaderConstantsAt(0xA8), 0);
    psmMeshParameter(g_meshParticleMeshEffectState.fogColor)->setValue(dx9rend::raShaderConstantsAt(0xBC), 0);
    {
        const Vec3& sun = g_LightManager->getEffectSunColor();
        psmMeshParameter(g_meshParticleMeshEffectState.effectSunColor)->setValue(&sun, 0);
    }
    {
        const Vec3& shadow = g_LightManager->getEffectShadowColor();
        psmMeshParameter(g_meshParticleMeshEffectState.effectShadowColor)->setValue(&shadow, 0);
    }
}

// PE 10174640 (577 B, `retn 10h`). Draws one collected group through the descriptor's
// MeshParticleMesh instance, filling its bone slots from the top down and flushing a batch
// (drawBatch) every time slot 0 is reached. Returns false only for an empty group.
static bool psmDrawMeshParticleGroup(unsigned char* renderer, dx9rend::IRenderView* view,
    unsigned int target, unsigned int groupIndex, PsmDrawGroup* groups)
{
    PsmDrawGroup* const group = groups + groupIndex;
    if (!group->count) return false;
    const int lastIndex = group->last;
    view->getViewport();                        // discarded
    IParticleSystemInstance* const instance = reinterpret_cast<IParticleSystemInstance*>(group->instance);
    const unsigned int count = group->count;
    unsigned int consumed = 0;
    IParticleSystemTemplate* const systemTemplate = instance->getTemplate();
    unsigned char* const descriptor = reinterpret_cast<unsigned char*>(systemTemplate);
    // Technique for drawBatch: blend mode 1 -> "Additive"; else write-Z -> "DiffuseWithZWrite";
    // else "Diffuse".
    int technique = 0;
    if (psmDescriptor1<unsigned int>(descriptor, 0x90) == 1) technique = 1;
    else if (descriptor[0x1F2]) technique = 2;
    // COLOR1 reads descriptor +0x1AC..+0x1B8 as a Vec4 whose w is the derived light-affection
    // factor at +0x1B8; COLOR2 is the {rgb,1} Vec4 at +0x1BC; colorblendgraph the graph at +0x12C.
    if (g_meshParticleMeshEffectState.color1)
        psmMeshParameter(g_meshParticleMeshEffectState.color1)->setValue(descriptor + 0x1AC, 0);
    if (g_meshParticleMeshEffectState.color2)
        psmMeshParameter(g_meshParticleMeshEffectState.color2)->setValue(descriptor + 0x1BC, 0);
    if (g_meshParticleMeshEffectState.colorBlendGraph)
        psmMeshParameter(g_meshParticleMeshEffectState.colorBlendGraph)->setValue(descriptor + 0x12C, 0);
    // +0x1B4 getLightMapIntensityOffset: the type-1 descriptor's slot 109 returns -1.0.
    const float lightmapIntensityOffset =
        static_cast<IParticleSpriteSystemTemplate*>(systemTemplate)->getLightMapIntensityOffset();
    {
        // No null test on the looked-up parameter (the colour words above are tested).
        const std::string name("LightmapIntensityOffset");
        psmMeshParameter(dx9rend::effectGetShaderParameterBorrowedTyped(g_meshParticleMeshEffectState.effect, name))
            ->setValue(&lightmapIntensityOffset, 0);
    }
    // Descriptor +0x8C is the MeshParticleMesh instance the type-1 template's setGeometry
    // (slot 114, PE 101796D0) created. Every use below re-reads it, as the PE does. A null
    // geometry leaves the group untouched and still reports true (shipped).
    if (psmDescriptor1<unsigned char*>(descriptor, 0x8C))
    {
        // Mesh +0x44 points at the selected (lod, geom) record whose first word is the number of
        // bone slots (the instance's matrix array at +0x38 has 26).
        unsigned int slot = meshParticleMeshLodPartCount(psmDescriptor1<unsigned char*>(descriptor, 0x8C));
        if (count)
        {
            for (;;)
            {
                unsigned char* particle = psmMeshField<unsigned char**>(renderer, 4)[lastIndex - static_cast<int>(consumed)];
                if (psmDrawField<unsigned int>(particle, 0xBC) < target) break;   // unsigned `jb`
                --slot;
                psmMeshGeometrySetPartTransformation(psmDescriptor1<unsigned char*>(descriptor, 0x8C),
                    &psmDrawField<Mat4>(particle, 4), static_cast<int>(slot));
                meshParticleMeshSetAgeAndAlpha(psmDescriptor1<unsigned char*>(descriptor, 0x8C),
                    psmDrawField<float>(particle, 0x5C), psmDrawField<float>(particle, 0x6C), slot);
                if (!slot)
                {
                    // A full batch writes slot 0 twice: +0x4C above, then +0x50 (-> +0x4C(m, 0)).
                    psmMeshGeometrySetTransformation(psmDescriptor1<unsigned char*>(descriptor, 0x8C),
                        &psmDrawField<Mat4>(particle, 4));
                    meshParticleMeshDrawBatch(psmDescriptor1<unsigned char*>(descriptor, 0x8C), technique, false);
                    slot = meshParticleMeshLodPartCount(psmDescriptor1<unsigned char*>(descriptor, 0x8C));
                }
                --group->count;
                ++psmMeshField<unsigned int>(renderer, 0x1C14);
                if (++consumed >= count) break;
            }
        }
        if (slot < meshParticleMeshLodPartCount(psmDescriptor1<unsigned char*>(descriptor, 0x8C)))
        {
            // A partial batch parks every unfilled slot on the shared zero matrix (PE 10079290),
            // collapsing those copies of the mesh to a point, and draws the whole batch anyway.
            while (slot > 1)
            {
                --slot;
                psmMeshGeometrySetPartTransformation(psmDescriptor1<unsigned char*>(descriptor, 0x8C),
                    &Mat4::getZero(), static_cast<int>(slot));
            }
            if (slot == 1)
                psmMeshGeometrySetTransformation(psmDescriptor1<unsigned char*>(descriptor, 0x8C), &Mat4::getZero());
            meshParticleMeshDrawBatch(psmDescriptor1<unsigned char*>(descriptor, 0x8C), technique, false);
        }
    }
    group->last = lastIndex - static_cast<int>(consumed);
    return true;
}

// PE 10175230 (428 B, `retn 4`). Draws the MeshParticleMesh instances object draws retained at
// +0x1D80 (pushed by the geometry's +0x18 draw slot, PE 101B0370 -> 101B04C0). Element bytes:
// +0x7C first-person flag, +0x7D use the second first-person view, +0x80 alpha.
static void psmDrawMeshParticleExtraList(unsigned char* renderer, bool pass)
{
    std::vector<MeshParticleMesh*>& list = meshParticlesRendererMeshes(renderer);
    MeshParticleMesh** entry = list.data();
    if (entry != list.data() + list.size())
    {
        do
        {
            MeshParticleMesh* const meshObject = *entry;
            unsigned char* const mesh = reinterpret_cast<unsigned char*>(meshObject);
            psmMeshGeometryGetTemplate(mesh);   // slot +0x38, result discarded
            // The view is fetched for every element, including the ones skipped below.
            dx9rend::IRenderView* const view = pass && meshObject->m_alternateView     // +0x7D
                ? static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderView2()
                : static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderViewDup();
            if (pass ? meshObject->m_firstPerson != 0 : meshObject->m_firstPerson == 0) // +0x7C
            {
                if (g_meshParticleMeshEffectState.worldViewProjection)
                {
                    const Mat4* viewProjection = view->getViewProjectionMatrixPtr();
                    psmMeshParameter(g_meshParticleMeshEffectState.worldViewProjection)->setValue(viewProjection, 0);
                }
                const Vec4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
                const float light = g_LightManager->getDefaultEffectLightAffectionFactor();
                const Vec4 blend = { 0.0f, 0.0f, 0.0f, light };
                if (g_meshParticleMeshEffectState.color1)
                    psmMeshParameter(g_meshParticleMeshEffectState.color1)->setValue(&white, 0);
                if (g_meshParticleMeshEffectState.color2)
                    psmMeshParameter(g_meshParticleMeshEffectState.color2)->setValue(&white, 0);
                if (g_meshParticleMeshEffectState.colorBlendGraph)
                    psmMeshParameter(g_meshParticleMeshEffectState.colorBlendGraph)->setValue(&blend, 0);
                const float one = 1.0f;
                {
                    const std::string name("LightmapIntensityOffset");
                    psmMeshParameter(dx9rend::effectGetShaderParameterBorrowedTyped(g_meshParticleMeshEffectState.effect, name))
                        ->setValue(&one, 0);
                }
                meshParticleMeshSetAgeAndAlpha(mesh, 1.0f, meshObject->m_alpha, 0);   // +0x80
                meshParticleMeshSetAgeAndAlpha(mesh, 1.0f, meshObject->m_alpha, 1);
                meshParticleMeshDrawBatch(mesh, 1, false);   // always "Additive"
            }
            ++entry;
        } while (entry != list.data() + list.size());
    }
    // Only the first-person call (pass == true) empties the list: PE 100D74B0 resize(0, 0),
    // which keeps the allocation (same effect as psmDrawClearPointerVector).
    if (pass)
        psmDrawClearPointerVector<MeshParticleMesh*>(renderer, psmMeshRendererOffset(0x1D80));
}
