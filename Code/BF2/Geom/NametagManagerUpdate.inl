// Path: [CHOSEN] split of NametagManager.cpp, attested at RendDX9 0x1021CFB0.
// Updates tag visibility, fade, relation, projection and text before drawing.
// Included inside NametagManager.cpp's geom namespace; never compile separately.
// PE update: 0x10130360. Arithmetic order is preserved; SSE2 double chains can
// differ from the retail device's 24-bit x87 precision.

namespace {


// Reads a float setting and copies it before the temporary key is destroyed.
// Requires a valid repository and returned value; compatibility builds use the VC7.1 slot.
float settingsFloat(const char* name, float defaultValue)
{
    const renddx9abi::HostString key(name);
    return settings->floatGet(key, defaultValue);
}

// Returns the player's kit as its primary IObject face across the host ABI boundary.
world::IObject* hudPlayerKit(IBase* hud, world::IPlayer* player)
{
    return reinterpret_cast<hud::BF2HudManager*>(hud)->getPlayerKit(player);
}

void hudEnableVOHelpMessage(IBase* hud, int message)
{
    reinterpret_cast<hud::BF2HudManager*>(hud)->enableVOHelpMessage(message);
}

// sub_10103CB0 - the object, then its +0x28 parents, asking each for IID_IPlayerControlObject
// (ds:10208CF0 = 50373); the first answer. (SkinnedMesh.cpp, BundledMeshDraw.cpp, XpLightManager.cpp
// and NameTagRendererDraw.inl each carry a TU-local copy of this 20-byte body; the tree has no shared
// declaration to call.)
world::IPlayerControlObject* owningControlObject(world::IObject* object)
{
    for (; object; object = world::object_detail::LayoutAccess::parent(object))
    {
        void* const found = object->queryInterface(world::IID_IPlayerControlObject);
        if (found)
            return static_cast<world::IPlayerControlObject*>(found);
    }
    return 0;
}

// sub_100777A0 - normalise in place unless |v|^2 is already within FLT_EPSILON (flt_10208958) of 1;
// a vector with |v|^2 below FLT_EPSILON is zeroed and -20001 answered. A NaN length fails both tests
// and normalises to NaN. (SkyDome.cpp's vec3Normalize and SkinnedMesh.cpp keep TU-local copies.)
int nametagNormalizeVec3(Vec3& v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920929e-07f)
        return 0;
    if (std::fabs(lengthSquared) < 1.1920929e-07f)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return -20001;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    v.x = scale * v.x;
    v.y = scale * v.y;
    v.z = scale * v.z;
    return 0;
}

// sub_1000ED40, __stdcall, `retn 0Ch` (value, low, high): `fcomi high; fcmovnb` then
// `fcomi low; fcmovb`. The high bound is applied first; a NaN is unordered, sets CF, and so comes
// back as `low` (NameTagRendererDraw.inl's ntClamp reads it the same way). The answer is re-loaded
// from its float spill, so it is a float.
float nametagClampFloat(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// `mov eax, a; sub eax, b; cdq; xor eax, edx; sub eax, edx` - the distance hysteresis' |a - b|, in
// the wrapping 32-bit arithmetic the shipped code uses (INT_MIN stays INT_MIN; the callers then
// compare it signed with `jg`).
int wrappedAbsDifference(int a, int b)
{
    const unsigned int difference = static_cast<unsigned int>(a) - static_cast<unsigned int>(b);
    const unsigned int sign = (difference & 0x80000000u) ? 0xFFFFFFFFu : 0u;
    return static_cast<int>((difference ^ sign) - sign);
}

// The distance hysteresis both position bodies share (0x1012CE49..0x1012CE91 and
// 0x1012E5D6..0x1012E61E): an exact change under 25 m, more than 2 m under 250 m, more than 5 m
// under 400 m, more than 9 m anywhere. Signed compares throughout (`jge` / `jg`).
bool distanceChanged(int distance, int previous)
{
    if (distance < 25 && distance != previous)
        return true;
    if (distance < 250 && wrappedAbsDifference(distance, previous) > 2)
        return true;
    if (distance < 400 && wrappedAbsDifference(distance, previous) > 5)
        return true;
    return wrappedAbsDifference(distance, previous) > 9;
}

// The transformation every "not drawn" arm stores: {0, 0, -1, 0} - the same four words removeKey and
// removeAllItems write when a slot is freed (0x1012FBD5.., 0x1012EF4A..). Written x, y, z, w.
void parkTransformation(float* transformations, int slot)
{
    float* const transformation = transformations + slot * 4;
    transformation[0] = 0.0f;
    transformation[1] = 0.0f;
    transformation[2] = -1.0f;
    transformation[3] = 0.0f;
}


// Rejects objects under the ignored root and requires every requested flag bit.
// Objects whose roots differ must be nonnull. [CHOSEN] Translation-unit-local adapter.

class NametagRootIgnorePredicator : public world::ObjectPredicator
{
public:
    NametagRootIgnorePredicator(unsigned int flags, world::IObject* object)
        : m_flags(flags)
        , m_root(static_cast<world::IObject*>(rootParent(object)))
    {
    }

    virtual bool predicate(world::IObject* object)
    {
        if (rootParent(object) == m_root)
            return false;
        return (world::object_detail::LayoutAccess::flags(object) & m_flags) == m_flags;
    }
    virtual unsigned int getFlags() const { return m_flags; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int) { return true; }

    unsigned int    m_flags;   // +0x04
    world::IObject* m_root;    // +0x08
};


// Iterates a by-value copy of the host's seat map (the std::map getPcos() answers) in key order.

class NametagSeatCopy
{
public:
    explicit NametagSeatCopy(world::IPlayerControlObject* pco)
        : m_seats(*pco->getPcos())                                             // +0x60
        , m_it(m_seats.begin())
    {
    }
    bool atEnd() const { return m_it == m_seats.end(); }
    world::IPlayerControlObject* seat() const { return m_it->second; }
    void next() { ++m_it; }

private:
    NametagSeatCopy(const NametagSeatCopy&);
    NametagSeatCopy& operator=(const NametagSeatCopy&);

    typedef std::map<int, world::IPlayerControlObject*> SeatMap;
    SeatMap                 m_seats;
    SeatMap::const_iterator m_it;
};

} // anonymous namespace


// Advances tag selection, positions, fade and text while buffering is enabled.
// Requires the Game singleton; crosshair refresh and item updates share the same due timestamp.

void NametagManager::update()
{
    if (!m_bufferingEnabled)
        return;

    IGame* const gameSingleton = static_cast<IGame*>(
        classManager->getSingleton(renddx9abi::HostString("Game")));        // ClassManager +0x48
    m_isDemoPlayback = gameSingleton->isPlayingDemo();                       // IGame PE +0xDC

    // time, the cross-fade pulse, the local player's own call timers
    const double frameDelta = nametagFrameDelta();                           // [ebp-10h]
    m_time = frameDelta + m_time;
    const float phase = static_cast<float>(m_time * s_pulseRate);            // [ebp-8] - dead
    s_pulse = static_cast<float>(0.5f - std::cos(std::fmod(static_cast<double>(phase),
                                                           6.2831854820251465)) * 0.5f);   // dbl_1020AAE8

    double remaining = s_medicCallTimer - frameDelta;
    if (0.0 > remaining)
        remaining = 0.0;
    s_medicCallTimer = remaining;
    remaining = s_ammoCallTimer - frameDelta;
    if (0.0 > remaining)
        remaining = 0.0;
    s_ammoCallTimer = remaining;
    remaining = s_repairCallTimer - frameDelta;
    if (0.0 > remaining)
        remaining = 0.0;
    s_repairCallTimer = remaining;

    // the crosshair target, every half second
    if (m_time - m_lastTargetCheckTime > 0.5)
        m_targetRoot = static_cast<world::IObject*>(
            rootParent(findCrosshairTarget(s_cullDistances[TTPlayer].y)));   // flt_10239A84 (300)

    // the manager's own frustum: the render view's, re-projected to (0, 2000]
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();    // Renderer +0x48
    m_frustum = *view->getFrustumPtr();                                      // +0x74
    m_frustum.setPerspective(m_frustum.getFov(), m_frustum.m_aspect, 0.0f, 2000.0f,
                             m_frustum.m_transformation);

    for (NametagItemMap::iterator it = m_items.begin(); it != m_items.end(); ++it)
    {
        NametagItem* const item = it->second;

        tickItemTimer(item);                                                 // 0x1012CB00, inlined
        updateItemPosition(item);                                            // 0x1012E8D0, inlined

        if (item->m_type != TTPlayer
            || isPrimaryOccupant(static_cast<PlayerNametagItem*>(item)->m_player.get()))
            item->m_flags |= NIF_PrimaryOccupant;                            // `or byte [esi+0BAh],1`
        else
            item->m_flags &= ~NIF_PrimaryOccupant;

        // in view?
        const Frustum* frustum;
        if (item->m_type == TTPlayer)
        {
            if (!(item->m_flags & NIF_PrimaryOccupant))
            {
                parkTransformation(m_transformations[item->m_type], item->m_slot);
                continue;
            }
            frustum = dx9rend::rendererGetRenderView()->getFrustumPtr();    // +0x48, +0x74
        }
        else if (item->isUAV())                                              // 0x1012D7D0, inlined
        {
            frustum = dx9rend::rendererGetRenderView()->getFrustumPtr();
        }
        else
        {
            frustum = &m_frustum;
        }
        item->m_inView = frustum->inside(item->m_worldPosition);

        // out of view: only a squad member, or a specialist the local player has called for
        if (!(item->m_flags & NIF_SquadMember) && !item->m_inView)
        {
            bool callPending = false;
            if (item->m_type == TTPlayer)
            {
                // The item's player is handed on without a null test (0x1012C3D0 dereferences it).
                switch (playerKitType(static_cast<PlayerNametagItem*>(item)->m_player.get()))
                {
                case kKitMedic:    callPending = s_medicCallTimer > 0.0;  break;
                case kKitSupport:  callPending = s_ammoCallTimer > 0.0;   break;
                case kKitEngineer: callPending = s_repairCallTimer > 0.0; break;
                default:                                                  break;
                }
            }
            if (!callPending)
            {
                parkTransformation(m_transformations[item->m_type], item->m_slot);
                continue;
            }
        }

        updateItemFade(item);                                                // 0x1012EF90, inlined
        updateItemSelection(item);

        // the slot's transformation
        const double fade = item->m_fade.m_value;                            // [ebp-10h]
        const bool arrowModeBefore = item->m_arrowMode;                      // [ebp-1]
        if (fade > 0.01 && (item->m_flags & NIF_Visible))                    // dbl_1021D000
        {
            const Vec4 screen = projectToScreen(item->m_worldPosition, item);   // by value
            float* const transformation = m_transformations[item->m_type] + item->m_slot * 4;
            transformation[0] = screen.x;                                    // stored y, x, z, w
            transformation[1] = screen.y;
            transformation[2] = screen.z;
            transformation[3] = screen.w;
        }
        else
        {
            parkTransformation(m_transformations[item->m_type], item->m_slot);
        }

        updateLocalVehicleSeats(item);

        if (item->m_dirty && fade > 0.01)
            rebuildItemText(item);
        else if (item->m_arrowMode != arrowModeBefore)
            m_renderers[item->m_type].bufferItem(item, true);
    }

    if (m_time - m_lastTargetCheckTime > 0.5)
        m_lastTargetCheckTime = m_time;
}


// 0x1012C1F0, 157 B, `ret`, ecx = a Vec4 on the caller's stack (projectToScreen's arrow).
// ((x^2 + y^2) + z^2) + w^2 spilled as a float; nothing happens within FLT_EPSILON of 1; a length
// below s_normalizeEpsilon (2^-46 - NOT the FLT_EPSILON sub_100777A0 uses) zeroes all four and
// answers -20001; otherwise each component is multiplied by 1/sqrt. NaN reaches the multiply.

int NametagManager::normalize4(Vec4& v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920929e-07f)                    // flt_1021CEC0
        return 0;
    if (std::fabs(lengthSquared) < s_normalizeEpsilon)                       // flt_10239A50
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        v.w = 0.0f;
        return -20001;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    v.x = scale * v.x;
    v.y = scale * v.y;
    v.z = scale * v.z;
    v.w = scale * v.w;
    return 0;
}

// 0x1012C290, 20 B, fastcall (ecx = the component), `ret`: its only caller is updateHudTarget.
world::IWeaponObject* NametagManager::queryWeaponObject(IBase* component)
{
    if (!component)
        return 0;
    return static_cast<world::IWeaponObject*>(
        component->queryInterface(world::IID_IWeaponObject));                // ds:10208CBC = 38080
}


// 0x1012C380, 74 B, `retn 4` (ecx unused). The current vehicle (+0x44), a soldier by its template's
// class id, whose armour component (0x1012C330's walk, inlined) reports status 1.
// The player and the vehicle's template are not null-tested.

bool NametagManager::isPlayerSoldierDown(world::IPlayer* player)
{
    world::IObject* const vehicle = player->getVehicle();                   // PE +0x44
    if (!vehicle)
        return false;
    if (templateOf(vehicle)->getClassID() != world::CID_SoldierTemplate)    // +0x14, vslot +0x10
        return false;
    world::IArmorGLComp* const armor =
        static_cast<world::IArmorGLComp*>(findArmorComponent(vehicle));     // inlined walk
    if (!armor)
        return false;
    return armor->getStatus() == 1;                                          // +0x5C
}


// 0x1012C510, 156 B, fastcall (ecx = &position, edx = the render view), `ret`. Snaps a clip-space
// x/y to the centre of the pixel it falls in:
//     x = (floor((x + 1) / (1 / (width / 2))) - 0.5) * (1 / (width / 2)) - 1      (and y by height)
// getViewport (+0x14) is asked twice, once per axis. floor is MSVCR71's double floor
// (ds:101DB2D8); the half-pixel subtraction and everything after it are float.

void NametagManager::projectOntoView(Vec4& position, dx9rend::IRenderView* view)
{
    const float invHalfWidth  = 1.0f / (view->getViewport()[2] * 0.5f);     // [ebp-4]
    const float invHalfHeight = 1.0f / (view->getViewport()[3] * 0.5f);     // [ebp-8]
    const float pixelX = static_cast<float>(
        std::floor(static_cast<double>((position.x + 1.0f) / invHalfWidth)) - 0.5f);   // [ebp-0Ch]
    const float pixelY = static_cast<float>(
        std::floor(static_cast<double>((position.y + 1.0f) / invHalfHeight)) - 0.5f);
    position.x = pixelX * invHalfWidth - 1.0f;
    position.y = pixelY * invHalfHeight - 1.0f;
}


// 0x1012CB40, 269 B, `ret`, ecx = the target object (isEnemyInSight hands it the item player's
// vehicle). A ray from the local player's camera eye to the target's position through
// ObjectManager +0x68 intersectLine, with nt_core's NametagRayPredicator { the local player's
// vehicle (not its root), 0x100, ignore soldiers }. True when the ray hits nothing (`sete al`).
// The camera is used with no null test (0x1012CBA5), unlike findCrosshairTarget / isInViewCone.
// The eye point is copied out of the camera matrix before the target is asked for its position.

bool NametagManager::isLineOfSightClear(world::IObject* target)
{
    world::IPlayer* const local = localHumanPlayer();
    if (!local)
        return false;

    world::IObject* hitObject = 0;                                           // [ebp-8]
    Vec3 hitPoint;                                                           // [ebp-44h]
    hitPoint.x = 0.0f;
    hitPoint.y = 0.0f;
    hitPoint.z = 0.0f;
    Vec3 hitNormal;                                                          // [ebp-38h]
    hitNormal.x = 0.0f;
    hitNormal.y = 0.0f;
    hitNormal.z = 0.0f;
    int hitPart = 0;                                                         // [ebp-4]

    const Mat4& eye = local->getCamera()->getTransformation(0.0f);          // PE +0x17C, +0x10
    Vec3 from;                                                               // [ebp-14h]
    from.x = eye.m[12];
    from.y = eye.m[13];
    from.z = eye.m[14];
    const Vec3 to = target->getAbsolutePosition();                           // PE +0x68, copied
    NametagRayPredicator predicator(local->getVehicle(), 0x100u, true);
    Vec3 direction;                                                          // [ebp-2Ch]
    direction.x = to.x - from.x;
    direction.y = to.y - from.y;
    direction.z = to.z - from.z;

    const bool hit = reinterpret_cast<world::ObjectManager*>(objectManager)->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, from, direction, &predicator,
        true, true, true, true, 0u);                                         // +0x68
    return !hit;
}


// 0x1012CC50, 150 B, `retn 14h` (fade, delay, duration). A tag stays invisible while its
// m_sightTime accumulates up to `delay`; after that - or once it is already visible - it grows by
// elapsed / duration; then [0, 1].
// A m_sightTime that fails `<= delay` (greater, or NaN) jumps straight to the growth
//   (0x1012CC76), skipping the second test - a NaN sight time still fades in.
// The clamp re-stores an unchanged value; a NaN value stays NaN.

void NametagManager::fadeIn(NametagFade* fade, double delay, double duration)
{
    const double elapsed = m_time - fade->m_time;
    bool grow;
    if (!(fade->m_value <= 0.01))                                            // dbl_1021D000
    {
        grow = fade->m_sightTime > delay || fade->m_value > 0.01;
    }
    else if (!(fade->m_sightTime <= delay))
    {
        grow = true;
    }
    else
    {
        fade->m_sightTime = elapsed + fade->m_sightTime;
        grow = fade->m_sightTime > delay || fade->m_value > 0.01;
    }
    if (grow)
        fade->m_value = elapsed / duration + fade->m_value;

    if (0.0 > fade->m_value)
        fade->m_value = 0.0;
    else if (1.0 < fade->m_value)
        fade->m_value = 1.0;
}

// 0x1012CCF0, 92 B, `retn 0Ch` (fade, duration): down by elapsed / duration, the sight time reset,
// then [0, 1] as above.
void NametagManager::fadeOut(NametagFade* fade, double duration)
{
    fade->m_value = fade->m_value - (m_time - fade->m_time) / duration;
    fade->m_sightTime = 0.0;
    if (0.0 > fade->m_value)
        fade->m_value = 0.0;
    else if (1.0 < fade->m_value)
        fade->m_value = 1.0;
}


// Converts through the compatibility 64-bit truncation rule and returns the low dword.
// NaN/out-of-range values yield zero; values beyond signed 32-bit range wrap.
inline int nametagFtol2(double value)
{
    if (!(value > -9223372036854775808.0 && value < 9223372036854775808.0))   // NaN or out of range
        return 0;
    return static_cast<int>(static_cast<unsigned int>(
        static_cast<unsigned long long>(static_cast<long long>(value))));
}

void NametagManager::updateObjectItem(NametagItem* item)
{
    world::IPlayer* const local = localHumanPlayer();
    if (!local)
        return;
    world::IObject* const vehicle = local->getVehicle();                    // PE +0x44
    if (!vehicle)
        return;

    item->m_worldPosition = item->updatePosition();                          // slot +0x00
    const Vec3 position = item->m_worldPosition;                             // [ebp-10h..-8]
    const Vec3 vehiclePosition = vehicle->getAbsolutePosition();             // PE +0x68

    if (item->m_flags & NIF_ControlPoint)
    {
        const int heightDelta = nametagFtol2(position.y - vehiclePosition.y + 0.5f);      // _ftol2
        item->m_heightDelta = heightDelta;
        if (heightDelta > s_unknown10239A98)
            item->m_elevationIcon = 0x30;
        else
            item->m_elevationIcon = heightDelta >= -s_unknown10239A98 ? -1 : 0x31;
    }

    const float dx = vehiclePosition.x - position.x;
    const float dz = vehiclePosition.z - position.z;
    const int distance = static_cast<int>(std::sqrt(dz * dz + dx * dx));     // fistp, truncating
    if (distanceChanged(distance, item->m_distance))
    {
        item->m_dirty = true;
        item->m_distance = static_cast<int>(static_cast<float>(distance) + 0.5f);
    }
}


// Projects a tag to pixel-snapped screen coordinates and updates arrow mode.
// Offscreen squad tags use the configured circle; unordered coordinates also select arrows.

Vec4 NametagManager::projectToScreen(Vec3 position, NametagItem* item)
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();    // Renderer +0x48
    const float circleRadius = settingsFloat("GSNameTagCircleRadius", 0.65f);

    Vec4 result;
    const Mat4* const camera = view->getViewMatrixPtr();                     // +0x64, row 3 = eye
    const float cx = camera->m[12] - position.x;
    const float cy = camera->m[13] - position.y;
    const float cz = camera->m[14] - position.z;
    result.w = std::sqrt(cz * cz + cy * cy + cx * cx);                      // [ebp-10h]

    const Mat4* const toView = view->getInverseViewMatrixPtr();              // +0x68
    const float viewZ = position.y * toView->m[6] + position.z * toView->m[10]
                      + position.x * toView->m[2] + toView->m[14];
    const float viewY = position.y * toView->m[5] + position.z * toView->m[9]
                      + position.x * toView->m[1] + toView->m[13];
    const float viewX = position.y * toView->m[4] + position.z * toView->m[8]
                      + position.x * toView->m[0] + toView->m[12];
    float pointX = viewX;                                                    // [ebp+0Ch]
    float pointY = viewY;                                                    // [ebp+10h]

    bool arrow = (item->m_flags & NIF_SquadMember) && !(viewZ > 1.0f);
    if (!arrow)
    {
        const Mat4* const projection = view->getProjectionMatrixPtr();      // +0x6C
        result.z = 1.0f;                                                     // [ebp-14h]
        const float clipZ = viewY * projection->m[6] + viewZ * projection->m[10]
                          + viewX * projection->m[2] + projection->m[14];
        const float clipY = viewY * projection->m[5] + viewZ * projection->m[9]
                          + viewX * projection->m[1] + projection->m[13];
        const float clipX = viewY * projection->m[4] + viewZ * projection->m[8]
                          + viewX * projection->m[0] + projection->m[12];
        pointX = clipX;
        pointY = clipY;
        const float inverseZ = 1.0f / clipZ;
        result.x = clipX * inverseZ;                                         // [ebp-1Ch]
        result.y = clipY * inverseZ;                                         // [ebp-18h]
        if (item->m_flags & NIF_SquadMember)
        {
            const float negativeRadius = -circleRadius;                      // [ebp+18h]
            arrow = !(result.x >= negativeRadius) || !(result.x <= circleRadius)
                 || !(result.y >= negativeRadius) || !(result.y <= circleRadius);
        }
        if (!arrow)
        {
            projectOntoView(result, view);
            item->m_arrowMode = false;
            return result;
        }
    }

    const Mat4* const projection = view->getProjectionMatrixPtr();          // +0x6C, again
    result.z = 0.0f;
    result.w = 0.0f;
    result.y = pointY * projection->m[5] + pointX * projection->m[1]
             + projection->m[9] * 10.0f + projection->m[13];                 // flt_101E2B00
    result.x = pointY * projection->m[4] + pointX * projection->m[0]
             + projection->m[8] * 10.0f + projection->m[12];
    normalize4(result);                                                      // discarded
    result.x = result.x * circleRadius;
    result.w = 1.0f;
    result.y = result.y * circleRadius;
    result.z = result.z * circleRadius;
    projectOntoView(result, view);
    item->m_arrowMode = true;
    return result;
}


// 0x1012D180, 230 B, `retn 8`, ecx unused. The cosine between the local camera's forward (matrix
// row 2) and the normalised camera-to-object vector, against `dotLimit`.
// No camera: the dot product is the 0.0f the body parks in its first argument's slot, so the
//   answer is 0 > dotLimit.
// getTransformation(0) is asked twice - once for the forward row, once for the eye - and the
//   object's position is copied in between.
// The dot product sums z, y, x.

bool NametagManager::isInViewCone(world::IObject* object, float dotLimit)
{
    world::IPlayer* const local = localHumanPlayer();
    if (!object || !local)
        return false;

    float dot = 0.0f;                                                        // [ebp+8]
    world::ICameraObject* const camera = local->getCamera();                // PE +0x17C
    if (camera)
    {
        const Mat4& facing = camera->getTransformation(0.0f);               // +0x10
        Vec3 forward;                                                        // [ebp-24h]
        forward.x = facing.m[8];
        forward.y = facing.m[9];
        forward.z = facing.m[10];
        const Vec3 position = object->getAbsolutePosition();                 // PE +0x68, copied
        const Mat4& eye = camera->getTransformation(0.0f);                  // asked again
        Vec3 toObject;                                                       // [ebp-0Ch]
        toObject.x = position.x - eye.m[12];
        toObject.y = position.y - eye.m[13];
        toObject.z = position.z - eye.m[14];
        nametagNormalizeVec3(toObject);
        dot = toObject.z * forward.z + toObject.y * forward.y + toObject.x * forward.x;
    }
    return dot > dotLimit;
}


// 0x1012D270, 268 B, `retn 4`, ecx unused. The object under the local player's crosshair within
// `maxDistance` (update passes the player cull distance, 300): intersectLine from the camera eye
// along maxDistance * the camera's forward row, ignoring everything under the local vehicle's root
// and anything without object flag 0x100 (the RendDX9 RootObjectIgnorePredicator copy above).
// getCamera (+0x17C) is asked three times - the null test, then once for each matrix - and
//   getTransformation(0) twice; the direction is computed before the second matrix is fetched.
// The eye point is passed BY address into the camera's own matrix (row 3), not copied.

world::IObject* NametagManager::findCrosshairTarget(float maxDistance)
{
    world::IPlayer* const local = localHumanPlayer();
    if (!local)
        return 0;
    if (!local->getCamera())
        return 0;

    world::IObject* hitObject = 0;                                           // [ebp-4]
    Vec3 hitPoint;                                                           // [ebp-2Ch]
    hitPoint.x = 0.0f;
    hitPoint.y = 0.0f;
    hitPoint.z = 0.0f;
    Vec3 hitNormal;                                                          // [ebp-20h]
    hitNormal.x = 0.0f;
    hitNormal.y = 0.0f;
    hitNormal.z = 0.0f;
    int hitPart = 0;                                                         // [ebp-8]

    NametagRootIgnorePredicator predicator(0x100u, local->getVehicle());
    world::ICameraObject* const facingCamera = local->getCamera();
    world::ICameraObject* const eyeCamera = local->getCamera();
    const Mat4& facing = facingCamera->getTransformation(0.0f);
    Vec3 direction;                                                          // [ebp-14h]
    direction.x = maxDistance * facing.m[8];
    direction.y = maxDistance * facing.m[9];
    direction.z = maxDistance * facing.m[10];
    const Mat4& eye = eyeCamera->getTransformation(0.0f);

    const bool hit = reinterpret_cast<world::ObjectManager*>(objectManager)->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, *reinterpret_cast<const Vec3*>(&eye.m[12]),
        direction, &predicator, true, true, true, true, 0u);                 // +0x68
    return hit ? hitObject : 0;                                              // neg / sbb / and
}


// Offers the first matching kit hint for a target in the local camera's 0.85 cone:
// healing below 70% health, ammo below 70%, or repair below 70% armor, in that order.
// local and player must be nonnull. Repeated ability/magazine reads remain separate.

void NametagManager::updateHudTarget(world::IPlayer* local, world::IPlayer* player)
{
    IBase* const hud = classManager->getSingleton(renddx9abi::HostString("HudManager"));
    if (!hud)
        return;
    world::IObject* const kit = hudPlayerKit(hud, local);                    // BF2HudManager +0x90
    if (!kit)
        return;
    world::IAbilityGLComp* const ability = static_cast<world::IAbilityGLComp*>(
        world::object_detail::LayoutAccess::abilityGLComponent(kit));        // Kit +0x64
    if (!ability)
        return;

    if (ability->getGLInfo()->hasHealingAbility)                             // +0x38, byte 0
    {
        world::IObject* const soldier = player->getDefaultVehicle();        // PE +0xA0
        if (!soldier)
            return;
        world::IArmorGLComp* const armor =
            static_cast<world::IArmorGLComp*>(findArmorComponent(soldier));
        if (!armor)
            return;
        const float hitPoints = armor->getHitPoints();                       // +0x3C
        if (!(hitPoints / armor->getMaxHitPoints() < 0.7f))                  // +0x84; flt_1021D020
            return;
        if (!isInViewCone(player->getVehicle(), 0.85f))
            return;
        hudEnableVOHelpMessage(hud, 45);                                     // BF2HudManager +0xF8
        return;
    }

    if (ability->getGLInfo()->hasAmmoAbility)                                // byte 2
    {
        world::IObject* const vehicle = player->getVehicle();               // PE +0x44
        world::IObject* const soldier = player->getDefaultVehicle();        // PE +0xA0
        if (!vehicle || vehicle != soldier)
            return;
        world::IPlayerControlObject* const control = owningControlObject(soldier);
        if (!control)
            return;
        world::IWeaponObject* const weapon = queryWeaponObject(soldierDefaultWeapon(control));
        if (!weapon)
            return;
        if (weapon->getMagSize() < 0)                                        // +0xA8, `jl`
            return;
        if (weapon->getMaxNrOfMags() < 0)                                    // +0x98
            return;
        const float inMagazine = static_cast<float>(weapon->getMag0Size());  // +0xAC, fild / fstp
        if (!(inMagazine / static_cast<float>(weapon->getMagSize()) < 0.7f)) // +0xA8 again
            return;
        if (!isInViewCone(player->getVehicle(), 0.85f))
            return;
        hudEnableVOHelpMessage(hud, 46);
        return;
    }

    if (ability->getGLInfo()->hasRepairingAbility)                           // byte 1
    {
        world::IObject* const vehicle = player->getVehicle();
        world::IObject* const soldier = player->getDefaultVehicle();
        if (!vehicle || vehicle == soldier)
            return;
        world::IArmorGLComp* const armor =
            static_cast<world::IArmorGLComp*>(findArmorComponent(vehicle));
        if (!armor)
            return;
        const float hitPoints = armor->getHitPoints();
        if (!(hitPoints / armor->getMaxHitPoints() < 0.7f))
            return;
        if (!isInViewCone(player->getVehicle(), 0.85f))
            return;
        hudEnableVOHelpMessage(hud, 47);
    }
}


// Returns the soldier's default weapon without checking the control object's dynamic type.
// The compatibility ABI reads face +0x74; native builds access Soldier::m_weapon2.

IBase* NametagManager::soldierDefaultWeapon(world::IPlayerControlObject* control)
{
    return static_cast<world::Soldier*>(control)->m_weapon2;
}


// 0x1012D7D0, 51 B, `ret`, ecx = the item: a TTObject item whose live object's template is a UAV.
// Every link is null-tested; update (0x101305A3) and the object fade (0x1012E703) inline the same
// test on items of every type.

bool NametagItem::isUAV()
{
    if (m_type != TTObject)
        return false;
    world::IObject* const object = static_cast<ObjectNametagItem*>(this)->m_object.get();
    if (!object)
        return false;
    world::IObjectTemplate* const objectTemplate = templateOf(object);      // +0x14
    if (!objectTemplate)
        return false;
    return objectTemplate->getClassID() == world::CID_UAVVehicleTemplate;   // ds:10208CB4 = 40019
}


// Updates relation color, rank, team state, and paced HUD hints.
// Requires an item's player when processing team state. Color changes rebuffer indices only;
// NIF_Bit15 forces color 2 whenever a local player exists.

void NametagManager::updateItemSelection(NametagItem* item)
{
    world::IPlayer* const local = localHumanPlayer();
    if (local && (item->m_flags & NIF_Bit15))
        item->m_colorIndex = 2;
    if (item->m_type != TTPlayer)
        return;

    PlayerNametagItem* const playerItem = static_cast<PlayerNametagItem*>(item);
    const int rankIcon = playerRankIcon(playerItem);
    if (rankIcon != item->m_rankIcon)
    {
        item->m_rankIcon = rankIcon;
        item->m_dirty = true;
    }

    world::IPlayer* const player = playerItem->m_player.get();               // [ebp+8]
    if (m_time - m_lastTargetCheckTime > 0.5)
        updateHudTarget(local, player);

    if (refreshPlayerItem(playerItem))
        m_renderers[item->m_type].bufferItem(item, false);

    if (local)
    {
        const int playerTeam = player->getTeam();                            // PE +0xE4
        if (local->getTeam() == playerTeam)
            item->m_flags |= NIF_Bit4;
        else
            item->m_flags &= ~NIF_Bit4;
    }
}


// Returns CPU fog visibility in [0,1] using the active Ra fog constants.
// Requires the constants block. A NaN far term selects its baseline; a NaN result propagates.

double NametagManager::calcFog(double distance)
{
    const float* const fogRange = reinterpret_cast<const float*>(
        dx9rend::raShaderConstantsAt(0xA8));                                 // FogRange, 4 floats
    const float nearScaled = static_cast<float>(fogRange[0] * distance);    // [ebp+8] as a float
    const float nearTerm = nearScaled + fogRange[2];                         // [ebp-10h]
    const float farTerm = static_cast<float>(distance * fogRange[1] + fogRange[3]);   // [ebp-0Ch]
    const float farClamped = farTerm > fogRange[3] ? farTerm : fogRange[3];   // [ebp+0Ch]
    const double visibility = farClamped - std::pow(static_cast<double>(nearTerm), 3.0);  // _CIpow
    if (0.0 > visibility)
        return 0.0;
    if (1.0 < visibility)
        return 1.0;
    return visibility;
}


// Updates a player's tag position, truncated horizontal camera distance and relative heading.
// Uses the spectated player as viewer when present; absent player/viewer leaves the item alone.

void NametagManager::updatePlayerItemPosition(NametagItem* item)
{
    world::IPlayer* const player = static_cast<PlayerNametagItem*>(item)->m_player.get();
    world::IPlayer* const viewer = m_spectatedPlayer.get() ? m_spectatedPlayer.get()
                                                           : localHumanPlayer();
    if (!player || !viewer)
        return;

    if (player != viewer)
    {
        world::IObject* const vehicle = player->getVehicle();               // PE +0x44
        Mat4 cameraMatrix = Mat4::getIdentity();
        if (viewer->getCamera())                                             // PE +0x17C
            cameraMatrix = viewer->getCamera()->getTransformation(0.0f);
        if (vehicle)
        {
            const Vec3& vehiclePosition = vehicle->getAbsolutePosition();    // PE +0x68
            const float dx = cameraMatrix.m[12] - vehiclePosition.x;
            const float dz = cameraMatrix.m[14] - vehiclePosition.z;
            const int distance = static_cast<int>(std::sqrt(dz * dz + dx * dx));   // fistp
            if (distanceChanged(distance, item->m_distance))
            {
                item->m_distance = distance;
                item->m_dirty = true;
            }
        }

        Mat4 vehicleMatrix = Mat4::getIdentity();
        float vehicleForwardY;
        if (vehicle)
        {
            vehicleMatrix = vehicle->getAbsoluteTransformation();            // PE +0x80
            vehicleForwardY = 0.0f;                                          // flt_101DDE64
        }
        else
        {
            vehicleForwardY = vehicleMatrix.m[9];                            // [ebp-6Ch]
        }
        const float yTerm = vehicleForwardY * 0.0f;                          // [ebp+8]
        const float forwardDot = vehicleMatrix.m[10] * cameraMatrix.m[10]
                               + vehicleMatrix.m[8] * cameraMatrix.m[8] + yTerm;
        const double angle = std::acos(static_cast<double>(
            nametagClampFloat(forwardDot, -1.0f, 1.0f)));                    // 0x1000ED40, _CIacos
        if (cameraMatrix.m[0] * vehicleMatrix.m[8] + cameraMatrix.m[2] * vehicleMatrix.m[10]
            + yTerm > 0.0f)
            item->m_iconRotation =
                static_cast<float>(static_cast<double>(6.28318548f) - angle); // flt_101E2B9C
        else
            item->m_iconRotation = static_cast<float>(angle);
    }

    item->m_worldPosition = item->updatePosition();                          // slot +0x00
}


// Updates order/object alpha from distance when the 3D map or UAV visibility permits it.
// Team-only tags require a local teammate. Hidden tags multiply alpha by elapsed frame
// time and clear sight time; nonpositive view distance uses a range of 9999.

void NametagManager::updateObjectItemFade(NametagItem* item)
{
    bool shown = m_3dMapOn || item->isUAV();                                 // byte +0x349; 0x1012D7D0
    if (shown)
    {
        world::IPlayer* const local = localHumanPlayer();
        if ((item->m_flags & NIF_Bit10) && (!local || local->getTeam() != item->m_team))
            shown = false;
    }

    if (shown)
    {
        const double range = item->m_viewDistance > 0.0 ? item->m_viewDistance : 9999.0;  // dbl_1021D0A8
        const double value = 1.0 - static_cast<double>(item->m_distance) / range;         // fidivr
        if (0.0 > value)
            item->m_fade.m_value = 0.0;
        else if (1.0 < value)
            item->m_fade.m_value = 1.0;
        else
            item->m_fade.m_value = value;
    }
    else
    {
        item->m_fade.m_value = (m_time - item->m_fade.m_time) * item->m_fade.m_value;
        item->m_fade.m_sightTime = 0.0;
    }

    item->m_fade.m_time = m_time;
    const float alpha = static_cast<float>(item->m_fade.m_value);            // before the call
    getAlphaArray(item->m_type)[item->m_slot] = alpha;                       // slot +0x3C, virtual
}


// Returns cached enemy visibility, retesting cone and line of sight at most every 0.5 s.
// A root matching the crosshair target is immediately visible, including two null roots.
// The item's player must exist; repeated vehicle calls preserve their original order.

bool NametagManager::isEnemyInSight(NametagItem* item)
{
    world::IPlayer* const player = static_cast<PlayerNametagItem*>(item)->m_player.get();
    world::IObject* const vehicle = player->getVehicle();                   // PE +0x44
    if (m_targetRoot == rootParent(vehicle))
        return true;

    if (m_time - item->m_lastSightCheckTime > 0.5)                          // dbl_1020D7D0
    {
        item->m_lastSightCheckTime = m_time;
        if (isInViewCone(player->getVehicle(), s_enemyDotLimit))
        {
            item->m_inSight = isLineOfSightClear(vehicle);
            return item->m_inSight;
        }
        item->m_inSight = false;
    }
    return item->m_inSight;
}


// Updates player-tag fading for team, camera mode, death state, distance and line of sight.
// With no player/viewer, leaves fade time and alpha unchanged. item must be a player tag.
// Enemy range testing precedes the sight query, whose retest timing has side effects.
// Publishes the resulting fade value through the alpha-array interface.

void NametagManager::updatePlayerItemFade(NametagItem* item)
{
    world::IPlayer* const player = static_cast<PlayerNametagItem*>(item)->m_player.get();   // [ebp-8]
    world::IPlayer* const viewer = m_spectatedPlayer.get() ? m_spectatedPlayer.get()
                                                           : localHumanPlayer();
    if (!player || !viewer)
        return;

    bool hasBody = false;                                                    // [ebp+0Bh]
    world::IObject* const vehicle = player->getVehicle();                   // PE +0x44
    if (vehicle)
    {
        world::IObjectTemplate* const vehicleTemplate = templateOf(vehicle);
        if (vehicleTemplate)
        {
            hasBody = vehicleTemplate->getClassID() != world::CID_FreeCameraTemplate;
            const int viewerTeam = viewer->getTeam();                        // PE +0xE4
            if (player->getTeam() != viewerTeam && isPlayerSoldierDown(player))
                hasBody = false;
        }
    }
    const bool manDown = isPlayerSoldierDown(player);                        // [ebp-1]

    if (viewer->getOverheadCameraEnabled())                                  // PE +0xB8
    {
        if ((item->m_flags & NIF_Bit10) && viewer->getTeam() != item->m_team)
            fadeOut(&item->m_fade, s_enemyTagFadeOutTime);                   // dbl_10239A28
        else
            fadeIn(&item->m_fade, 0.0, 0.7);                                 // dbl_1021D0D8
    }
    else if (!hasBody)
    {
        fadeOut(&item->m_fade, s_deathFadeOutTime);                          // flt_10239A4C
    }
    else if (playerKitType(viewer) == kKitMedic && manDown)
    {
        fadeOut(&item->m_fade, s_manDownFadeOutTime);                        // flt_10239A48
    }
    else if (manDown || (viewer == player && viewer->hasFirstPersonView()))  // PE +0x184
    {
        fadeOut(&item->m_fade, s_deathFadeOutTime);
    }
    else if ((item->m_flags & NIF_Bit10) && viewer->getTeam() != item->m_team && !isDemoPlayback())
    {
        // enemy
        const float cullDistance = s_cullDistances[TTPlayer].y;              // flt_10239A84
        item->m_viewDistance = cullDistance;                                 // `fst qword`
        if (static_cast<double>(item->m_distance) < static_cast<double>(cullDistance)
            && isEnemyInSight(item))
        {
            const double share = static_cast<double>(item->m_distance) / item->m_viewDistance;
            fadeIn(&item->m_fade, s_enemyTagDelayTime * share,               // dbl_10239A18
                   s_enemyTagFadeInTime * share);                            // dbl_10239A20
        }
        else
        {
            fadeOut(&item->m_fade, s_enemyTagFadeOutTime);
        }

        const double scaledDistance =
            static_cast<double>(item->m_distance) * 1.0526315789473684;      // dbl_1021D0D0 (20/19)
        const double falloff = clampValue(
            1.0 - (scaledDistance - item->m_viewDistance * 0.66666666666666663)   // dbl_1021D0C8
                / (item->m_viewDistance * 0.33333333333333331),                   // dbl_1021D0C0
            0.0, 1.0);
        item->m_fade.m_value =
            clampValue(item->m_fade.m_value, 0.0, calcFog(scaledDistance) * falloff);
    }
    else
    {
        // friend
        item->m_viewDistance = s_maxFriendlyDistance;                        // flt_10239A3C
        const double maxFriendlyDistance = s_maxFriendlyDistance;           // read again
        if (isEnemyInSight(item))
        {
            item->m_viewDistance = 500.0;                                    // dbl_1021D0B8
            fadeIn(&item->m_fade, 0.0, 0.7);
            const double inSightRange = item->m_viewDistance;
            item->m_fade.m_value = clampValue(
                1.0 - (static_cast<double>(item->m_distance) - inSightRange * 0.66666666666666663)
                    / (inSightRange * 0.33333333333333331),
                0.0, 1.0) * item->m_fade.m_value;
        }
        else
        {
            fadeOut(&item->m_fade, 1.5);                                     // dbl_1021D0B0
            const double distance = static_cast<double>(item->m_distance);
            if (distance < maxFriendlyDistance)
            {
                const double floorValue = clampValue(
                    1.0 - (distance - maxFriendlyDistance * 0.66666666666666663)
                        / (maxFriendlyDistance * 0.33333333333333331),
                    0.0, 1.0);
                item->m_fade.m_value =
                    (floorValue < item->m_fade.m_value) ? item->m_fade.m_value : floorValue;
            }
        }
    }

    item->m_fade.m_time = m_time;
    const float alpha = static_cast<float>(item->m_fade.m_value);            // before the call
    getAlphaArray(item->m_type)[item->m_slot] = alpha;                       // slot +0x3C, virtual
}


// 0x1012FC70, 162 B, `ret`, ecx = the IPlayerControlObject face: how many seats of a copy of its
// getPcos() map are occupied (+0x18 isOccupied), in key order.
// Unlike isPrimaryOccupant, a null seat is NOT skipped - it is dispatched on, as shipped.
// `pco` itself is not null-tested. Its only callers (0x1012FD6A, 0x1012FFBE) hand it
//   owningControlObject(rootParent(vehicle)) unguarded; update reaches both only for a player tag
//   that kept NIF_PrimaryOccupant this frame - which needs a vehicle whose root answers
//   IPlayerControlObject - so a null does not arrive on the shipped path.

int NametagManager::countSeats(world::IPlayerControlObject* pco)
{
    int occupied = 0;
    for (NametagSeatCopy seats(pco); !seats.atEnd(); seats.next())
        if (seats.seat()->isOccupied())
            ++occupied;
    return occupied;
}


// 0x1012FD20, 107 B, `retn 4`. On the local player's own tag: the number of other occupants of its
// vehicle; a change marks the tag dirty (rebuildItemText then shows "  [+n]") and is remembered in
// the file-static s_lastSeatCount (shared by nothing else).
// Neither the vehicle, its root nor the owning control object is null-tested before countSeats
//   dereferences the last. Kept unguarded: the only caller (update, 0x1013074A) runs this after
//   parking every player tag without NIF_PrimaryOccupant, so the local player's tag gets here only
//   with a vehicle whose root answers IPlayerControlObject.

void NametagManager::updateLocalVehicleSeats(NametagItem* item)
{
    if (item->m_type != TTPlayer)
        return;
    world::IPlayer* const player = static_cast<PlayerNametagItem*>(item)->m_player.get();
    world::IPlayer* const local = localHumanPlayer();
    if (local != player || !local)
        return;
    const int others = countSeats(owningControlObject(static_cast<world::IObject*>(
                           rootParent(local->getVehicle())))) - 1;
    if (others != s_lastSeatCount)                                           // dword_1025CEA0
    {
        item->m_dirty = true;
        s_lastSeatCount = others;
    }
}


// Rebuilds type-specific tag text, buffers its row and clears dirty state.
// Requires valid player/control objects and a supported item type.

void NametagManager::rebuildItemText(NametagItem* item)
{
    switch (item->m_type)
    {
    case TTPlayer:
    {
        world::IPlayer* const player = static_cast<PlayerNametagItem*>(item)->m_player.get();
        std::string passengers;                                             // [ebp-1Ch]
        const int others = countSeats(owningControlObject(static_cast<world::IObject*>(
                               rootParent(player->getVehicle())))) - 1;
        if (others != 0)
            passengers = nametagFormatString("  [+%d]", others);

        if (item->m_arrowMode && (item->m_flags & NIF_ShowText))
        {
            item->m_name = nametagFormatString("[%dm]", item->m_distance);
        }
        else if ((item->m_flags & NIF_ShowText)
                 && static_cast<double>(item->m_distance) <= s_cullDistances[TTPlayer].y)
        {
            const renddx9abi::HostString& name = fromHostStdString(player->getName());   // PE +0x20
            std::string label(nametagFormatString("[%dm]  %s", item->m_distance, name.c_str()));
            label += passengers;
            item->m_name = label;
        }
        else
        {
            const renddx9abi::HostString& name = fromHostStdString(player->getName());
            std::string label(renddx9abi::fromHostString(name));
            label += passengers;
            item->m_name = label;
        }
        break;
    }

    case TTOrder:
        if (isDemoPlayback())                                                // slot +0x68
            item->m_name = "";                                               // "" at 0x101DC333
        else
            item->m_name = nametagFormatString("[%dm]", item->m_distance);
        break;

    case TTObject:
    {
        if (isDemoPlayback())
        {
            item->m_name = "";
            item->m_text2C = "";
            break;
        }
        world::IObject* const object = static_cast<ObjectNametagItem*>(item)->m_object.get();
        if (item->isUAV())
        {
            item->m_name = renddx9abi::fromHostString(
                fromHostStdString(object->getName()));                       // PE +0x60
        }
        else if (object && !item->m_arrowMode && (item->m_flags & NIF_Bit2))
        {
            item->m_name = nametagFormatString("%s   [%im]",
                fromHostStdString(object->getName()).c_str(), item->m_distance);
        }
        else
        {
            item->m_name = nametagFormatString("[%im]", item->m_distance);
        }
        if (item->m_flags & NIF_ControlPoint)
        {
            static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->slot_0xB0();   // dropped
            item->m_text2C = "";
        }
        break;
    }

    default:
        // sub_10074300(stream, 6, __FILE__, 1566, "") << "Unknown type: " << m_type, then
        // sub_10074690 - logged the way this image logs its asserts (nt_core's orderIcon).
        ::renddx9abi::logf("NametagManager.cpp:1566 Unknown type: %d", item->m_type);
        break;
    }

    m_renderers[item->m_type].bufferItem(item, true);
    item->m_dirty = false;
}


// 0x10130290, 201 B, `retn 4`. Is `player` the first occupied seat (in key order) of a copy of
// its vehicle root's control-object map? A null seat is skipped; the first occupied one decides.
// Only the root is asked for IID_IPlayerControlObject (no parent walk, unlike sub_10103CB0).
// The copy is destroyed on both exits (0x10132580 on the early one, erase + free on the other).

bool NametagManager::isPrimaryOccupant(world::IPlayer* player)
{
    if (!player)
        return false;
    world::IObject* const vehicle = player->getVehicle();                   // PE +0x44
    if (!vehicle)
        return false;
    world::IObject* const root = static_cast<world::IObject*>(rootParent(vehicle));
    if (!root)
        return false;
    world::IPlayerControlObject* const control = static_cast<world::IPlayerControlObject*>(
        root->queryInterface(world::IID_IPlayerControlObject));             // ds:10208CF0
    if (!control)
        return false;

    for (NametagSeatCopy seats(control); !seats.atEnd(); seats.next())
    {
        world::IPlayerControlObject* const seat = seats.seat();
        if (seat && seat->isOccupied())                                      // +0x18
            return seat->getOccupingPlayer() == player;                      // +0x10
    }
    return false;
}

// 0x10131320, 50 B, `retn 18h` (value, low, high): `low > value` first, then `high < value`; a NaN
// value passes both and comes back unchanged.
double NametagManager::clampValue(double value, double low, double high)
{
    if (low > value)
        return low;
    if (high < value)
        return high;
    return value;
}
