// [TEMP] [CHOSEN, not recovered] Isolated x86 occlusion regression scaffolding.
// Includes the real drawer; narrow object/geometry collaborators abort on unsupported calls.
// Covers construction, both public render passes, exact boundary decisions and destruction.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <initializer_list>
#include "../BF2/Geom/ObjectDrawer.cpp"

namespace {
using namespace dice::hfe;
using namespace dice::hfe::geom;
int checks = 0, failures = 0;
std::vector<void*> liveAllocations, freedAllocations;
void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
__declspec(noreturn) void unexpected(const char* name)
{
    std::fprintf(stderr, "FAIL: unsupported fixture call: %s\n", name);
    std::fflush(stderr);
    std::abort();
}
void __fastcall unexpectedSlot(void*, void*) { unexpected("vtable slot"); }
Mat4 identity(float x = 0, float y = 0, float z = 0)
{
    Mat4 result = {};
    result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1;
    result.m[12] = x; result.m[13] = y; result.m[14] = z;
    return result;
}
template<size_t N> void poison(void* (&table)[N])
{
    for (size_t i = 0; i != N; ++i) table[i] = reinterpret_cast<void*>(&unexpectedSlot);
}
template<class T> std::vector<T> view(const T* first, unsigned int count)
{ return std::vector<T>(first, first + count); }
struct EventsFixture
{
    void** vptr;
    void* table[16];
    std::vector<IEventListener*> listeners;
    unsigned int registrations, unregistrations;
    EventsFixture() : vptr(table), registrations(0), unregistrations(0)
    {
        poison(table);
        table[0x2C / 4] = reinterpret_cast<void*>(&add);
        table[0x30 / 4] = reinterpret_cast<void*>(&remove);
    }
    static bool __fastcall add(EventsFixture* self, void*, EventCategory category,
                               IEventListener* listener, int priority)
    {
        check(category == 0 && priority == 0, "drawer registers ECCore listener at priority zero");
        self->listeners.push_back(listener); ++self->registrations; return true;
    }
    static bool __fastcall remove(EventsFixture* self, void*, EventCategory category,
                                  IEventListener* listener)
    {
        auto found = std::find(self->listeners.begin(), self->listeners.end(), listener);
        check(category == 0 && found != self->listeners.end(), "destructor unregisters exactly the registered listener");
        if (found != self->listeners.end()) self->listeners.erase(found);
        ++self->unregistrations; return true;
    }
};
struct GeometryFixture
{
    void** vptr;
    void* table[60];
    LocalBoundingBox bounds;
    Mat4 matrix;
    unsigned int boundsReads, transformReads, key, groupReads;
    int groupCount;
    const void* groups[4];
    GeometryFixture() : vptr(table), matrix(identity()), boundsReads(0), transformReads(0),
        key(0), groupReads(0), groupCount(0)
    {
        poison(table);
        table[0x38 / 4] = reinterpret_cast<void*>(&readKey);
        table[0x40 / 4] = reinterpret_cast<void*>(&readBounds);
        table[0x48 / 4] = reinterpret_cast<void*>(&readTransform);
        table[0xE8 / 4] = reinterpret_cast<void*>(&readGroupCount);
        table[0xEC / 4] = reinterpret_cast<void*>(&readGroup);
        setBounds(-0.1f, 0.1f);
    }
    void setBounds(float low, float high)
    {
        bounds.m_min = Vec3{ low, low, low }; bounds.m_max = Vec3{ high, high, high };
        bounds.calcRadius();
    }
    static unsigned int __fastcall readKey(GeometryFixture* self, void*) { return self->key; }
    static const void* __fastcall readBounds(GeometryFixture* self, void*)
    { ++self->boundsReads; return &self->bounds; }
    static const Mat4* __fastcall readTransform(GeometryFixture* self, void*)
    { ++self->transformReads; return &self->matrix; }
    static int __fastcall readGroupCount(GeometryFixture* self, void*) { return self->groupCount; }
    static const void* __fastcall readGroup(GeometryFixture* self, void*, unsigned int index)
    {
        if (index >= static_cast<unsigned int>(self->groupCount)) unexpected("group index");
        ++self->groupReads; return self->groups[index];
    }
};

struct ObjectFixture
{
    // Test-only storage uses the real header size and friend field access. The three
    // logical source slots lose the extra ELF destructor entry in MSVC x86.
    alignas(world::IObject) unsigned char storage[sizeof(world::IObject)];
    void* table[90];
    Vec3 position;
    Mat4 matrix;
    float radius;
    unsigned int positionReads, radiusReads, transformReads, draws;
    ObjectFixture(float x, float y, float z, float r, GeometryFixture* geometry = 0)
        : position{ x, y, z }, matrix(identity(x, y, z)), radius(r),
          positionReads(0), radiusReads(0), transformReads(0), draws(0)
    {
        std::memset(storage, 0, sizeof(storage)); poison(table);
        *reinterpret_cast<void***>(storage) = table;
        table[26] = reinterpret_cast<void*>(&readPosition);  // getAbsolutePosition
        table[32] = reinterpret_cast<void*>(&readTransform); // getAbsoluteTransformation
        table[36] = reinterpret_cast<void*>(&readRadius);    // getVisualBoundingRadius
        table[45] = reinterpret_cast<void*>(&updateGeometry);
        table[54] = reinterpret_cast<void*>(&readDecal);
        world::object_detail::LayoutAccess::geometry(get()) = geometry;
        world::object_detail::LayoutAccess::flags(get()) = world::IObject::OFIsVisible;
    }
    world::IObject* get() { return reinterpret_cast<world::IObject*>(storage); }
    static const Vec3& __fastcall readPosition(ObjectFixture* self, void*)
    { ++self->positionReads; return self->position; }
    static const Mat4& __fastcall readTransform(ObjectFixture* self, void*)
    { ++self->transformReads; return self->matrix; }
    static float __fastcall readRadius(ObjectFixture* self, void*)
    { ++self->radiusReads; return self->radius; }
    static void __fastcall updateGeometry(ObjectFixture* self, void*) { ++self->draws; }
    static void* __fastcall readDecal(ObjectFixture*, void*) { return 0; }
};

typedef ObjectDrawer DrawerFixture;
struct BorrowedVolumes
{
    ObjectDrawer& drawer;
    BorrowedVolumes(ObjectDrawer& d, std::initializer_list<std::vector<Plane3> > volumes) : drawer(d)
    { drawer.m_vector50.assign(volumes.begin(), volumes.end()); }
    ~BorrowedVolumes() { std::vector<std::vector<Plane3> >().swap(drawer.m_vector50); }
};
struct ViewFixture
{
    void** vptr;
    void* table[39];
    Mat4 matrix;
    unsigned int reads;
    ViewFixture() : vptr(table), matrix(identity()), reads(0)
    {
        poison(table); table[0x64 / 4] = reinterpret_cast<void*>(&readMatrix);
        table[0x74 / 4] = reinterpret_cast<void*>(&readFrustum);
    }
    static Mat4* __fastcall readMatrix(ViewFixture* self, void*)
    { ++self->reads; return &self->matrix; }
    static Frustum* __fastcall readFrustum(ViewFixture* self, void*)
    { return reinterpret_cast<Frustum*>(self); }
};
struct GridFixture
{
    void** vptr;
    void* table[20];
    ObjectDrawerSet collision, noncollision;
    const void* frustum;
    GridFixture(const ViewFixture& view) : vptr(table), frustum(&view)
    {
        poison(table); table[0x40 / 4] = reinterpret_cast<void*>(&queryCollision);
        table[0x4C / 4] = reinterpret_cast<void*>(&queryNoncollision);
    }
    static void query(GridFixture* self, const void* frustum, void* output, void* predicate,
                      int mode, float zero, float time, bool collision)
    {
        check(frustum == self->frustum && mode == 1 && zero == 0 && time == 0.25f,
            "public pass preserves grid query arguments");
        check(static_cast<ObjectDrawerFlagPredicator*>(predicate)->getFlags() == world::IObject::OFIsVisible,
            "public pass uses visible-object predicate");
        *static_cast<ObjectDrawerSet*>(output) = collision ? self->collision : self->noncollision;
    }
    static void __fastcall queryCollision(GridFixture* self, void*, const void* frustum, void* output,
        void* predicate, int mode, float zero, float time)
    { query(self, frustum, output, predicate, mode, zero, time, true); }
    static void __fastcall queryNoncollision(GridFixture* self, void*, const void* frustum, void* output,
        void* predicate, int mode, float zero, float time)
    { query(self, frustum, output, predicate, mode, zero, time, false); }
};
// One world-space quad occluder in StaticMesh's record shape: two opposed faces over four points,
// each face referencing all four edges (edge index in the low byte, created-here flag in the high).
// The record points at members, so a fixture is used where it is constructed and never copied.
struct OccluderFixture
{
    std::vector<StaticMeshOccluderEdge> edges;
    std::vector<unsigned short> references[2];       // the faces' original reference lists
    std::vector<unsigned short> referenceVectors[2];  // the lists the faces point at
    StaticMeshOccluderRecord record;
    OccluderFixture(float x = 0, float z = 5, bool reverse = false)
    {
        record.points.push_back(Vec3{ x - 1, -1, z }); record.points.push_back(Vec3{ x + 1, -1, z });
        record.points.push_back(Vec3{ x + 1, 1, z }); record.points.push_back(Vec3{ x - 1, 1, z });
        for (unsigned char i = 0; i != 4; ++i)
        {
            const unsigned char next = (i + 1) % 4;
            const StaticMeshOccluderEdge edge = { { reverse ? next : i, reverse ? i : next }, { 0, 0 } };
            edges.push_back(edge);
            references[0].push_back(static_cast<unsigned short>(i | (reverse ? 0 : 1) << 8));
            references[1].push_back(static_cast<unsigned short>(i | (reverse ? 1 : 0) << 8));
        }
        referenceVectors[0] = references[0];
        referenceVectors[1] = references[1];
        const StaticMeshOccluderFace front = { Plane3{ Vec3{ 0, 0, -1 }, z }, &referenceVectors[0] };
        const StaticMeshOccluderFace back = { Plane3{ Vec3{ 0, 0, 1 }, -z }, &referenceVectors[1] };
        record.faces.push_back(front); record.faces.push_back(back);
        record.edges = &edges;
    }
};
unsigned int volumeCount(const ObjectDrawer& drawer)
{ return static_cast<unsigned int>(drawer.m_vector50.size()); }
std::vector<Plane3>& volume(ObjectDrawer& drawer, unsigned int index)
{ return drawer.m_vector50[index]; }
float signedDistance(const Plane3& plane, const Vec3& point)
{ return plane.normal.x * point.x + plane.normal.y * point.y + plane.normal.z * point.z + plane.d; }
void releaseBuiltVolumes(ObjectDrawer& drawer)
{
    objectDrawerClearOccluders(&drawer);
    std::vector<std::vector<Plane3> >().swap(drawer.m_vector50);
}
bool culled(ObjectDrawer& drawer, ObjectFixture& object)
{
    ObjectDrawerSet set(1, object.get()); objectDrawerCullObjects(&drawer, set);
    return set.empty();
}
void cullCases()
{
    DrawerFixture drawer;
    Plane3 plane = { Vec3{ 1, 0, 0 }, 0 }; // Negative x half-space is hidden.
    GeometryFixture geometry;
    ObjectFixture inside(-2, 0, 0, 0.5f, &geometry);
    check(!culled(drawer, inside), "no volumes retain every object");
    BorrowedVolumes volumes(drawer, { view(&plane, 1) });
    check(culled(drawer, inside), "precise mode removes wholly hidden sphere");
    check(inside.positionReads == 2 && inside.radiusReads == 2, "typed object position/radius slots used once per cull");
    check(inside.transformReads == 0 && geometry.boundsReads == 0, "fully hidden sphere skips geometry bounds");
    ObjectFixture visible(2, 0, 0, 0.5f, &geometry);
    check(!culled(drawer, visible), "positive centre remains visible");
    ObjectFixture tangent(-1, 0, 0, 1, &geometry);
    check(culled(drawer, tangent), "sphere tangent from inside is hidden");
    check(tangent.transformReads == 0, "sphere tangency needs no box refinement");
    ObjectFixture overlap(-0.05f, 0, 0, 1, &geometry);
    check(!culled(drawer, overlap), "partially exposed box remains visible");
    check(overlap.transformReads == 1 && geometry.boundsReads == 1, "overlapping sphere refines with actual box and object transform");
    ObjectFixture refined(-0.5f, 0, 0, 1, &geometry);
    check(culled(drawer, refined), "hidden small box is culled despite overlapping sphere");
    ObjectFixture boxTangent(-0.1f, 0, 0, 1, &geometry);
    check(culled(drawer, boxTangent), "box tangent from inside is hidden");
    ObjectFixture noGeometry(-0.5f, 0, 0, 1);
    check(!culled(drawer, noGeometry), "overlapping sphere without geometry is retained");
    ObjectFixture hiddenNoGeometry(-2, 0, 0, 1);
    unsigned int count = drawer.m_dword7C;
    check(culled(drawer, hiddenNoGeometry), "fully hidden sphere without geometry is removed");
    check(drawer.m_dword7C == count, "hidden object without geometry does not increment geometry count");
    geometry.bounds.m_centre = Vec3{ 50, 0, 0 };
    geometry.matrix = identity(50, 0, 0);
    check(culled(drawer, refined), "object origin and object matrix drive cull, not bounds centre or geometry matrix");
    check(geometry.transformReads == 0, "cull never substitutes geometry transformation");
    refined.matrix = identity(1, 0, 0);
    check(!culled(drawer, refined), "absolute object transform exposes box despite hidden sphere centre");
    drawer.m_byte68 = 0;
    unsigned int boundsBefore = geometry.boundsReads;
    check(!culled(drawer, boxTangent), "sphere mode conservatively keeps intersecting sphere");
    check(culled(drawer, tangent), "sphere mode hides inside tangency");
    check(geometry.boundsReads == boundsBefore, "sphere mode never refines geometry bounds");
}
void orderingCases()
{
    DrawerFixture drawer; drawer.m_byte68 = 0;
    Plane3 left = { Vec3{ 1, 0, 0 }, 2 };
    Plane3 right = { Vec3{ -1, 0, 0 }, 2 };
    BorrowedVolumes volumes(drawer, { view(&left, 1), view(&right, 1) });
    GeometryFixture geometry;
    ObjectFixture a(-4, 0, 0, 1, &geometry), b(-5, 0, 0, 1),
        c(0, 0, 0, 1, &geometry), d(5, 0, 0, 1, &geometry), e(0, 1, 0, 1);
    ObjectDrawerSet set = { a.get(), b.get(), c.get(), d.get(), e.get() };
    objectDrawerCullObjects(&drawer, set);
    check(set.size() == 2, "multiple volumes remove adjacent entries without skipping");
    check(set.size() == 2 && set[0] == c.get() && set[1] == e.get(), "survivor order remains stable");
    check(drawer.m_dword7C == 2, "only erased objects with geometry counted");
    for (ObjectFixture* object : { &a, &b, &c, &d, &e })
        check(object->positionReads == 1 && object->radiusReads == 1, "each candidate inspected exactly once");
}
void comparisonPrecisionCases()
{
    DrawerFixture drawer;
    Plane3 plane = { Vec3{ 0.1f, 0, 0 }, -1 };
    BorrowedVolumes volumes(drawer, { view(&plane, 1) });
    ObjectFixture boundary(10, 0, 0, 0);
    check(!culled(drawer, boundary), "precise comparison preserves positive unrounded plane distance");
    drawer.m_byte68 = 0;
    check(!culled(drawer, boundary), "sphere comparison preserves positive unrounded plane distance");
}
void builderCases()
{
    DrawerFixture drawer; ViewFixture camera; drawer.setRenderView(&camera);
    GeometryFixture geometry; OccluderFixture occluder;
    geometry.groupCount = 1; geometry.groups[0] = &occluder.record;
    ObjectFixture owner(0, 0, 5, 2, &geometry);
    ObjectDrawerSet set(1, owner.get());
    objectDrawerBuildOccluders(&drawer, set);
    check(camera.reads == 2, "builder reads camera and forward through actual view slot");
    check(volumeCount(drawer) == 1, "facing near occluder produces one volume");
    check(geometry.groupReads == 1 && owner.positionReads == 1, "builder uses record slots and object distance");
    check(volume(drawer, 0).size() == 5, "quad yields source face plus four silhouette planes");
    Plane3* planes = volume(drawer, 0).data();
    check(planes[0].normal.z == -1 && planes[0].d == 5, "builder copies selected world face unchanged");
    for (int i = 1; i != 5; ++i)
    {
        const Plane3& plane = planes[i];
        check(std::fabs(plane.normal.x * plane.normal.x + plane.normal.y * plane.normal.y
            + plane.normal.z * plane.normal.z - 1) < 0.00001f, "silhouette normal is unit length");
        check(std::fabs(signedDistance(plane, Vec3{ 0, 0, 0 })) < 0.00001f, "silhouette passes through camera");
        check(signedDistance(plane, Vec3{ 0, 0, 10 }) < 0, "edge winding faces outward from hidden region");
    }
    GeometryFixture targetGeometry;
    ObjectFixture hidden(0, 0, 10, 0.5f, &targetGeometry), front(0, 0, 4, 0.5f, &targetGeometry),
        outside(3, 0, 10, 0.5f, &targetGeometry), exposed(1.99f, 0, 10, 0.5f, &targetGeometry);
    check(culled(drawer, hidden), "built volume hides object behind quad");
    check(!culled(drawer, front), "built volume retains object in front of quad");
    check(!culled(drawer, outside), "built volume retains object outside silhouette");
    check(!culled(drawer, exposed), "built volume retains partly exposed geometry");
    // The second face keeps two references; its storage still holds all four.
    occluder.referenceVectors[1].resize(2);
    occluder.edges[2].marks[1] = occluder.edges[3].marks[1] = 1;
    objectDrawerBuildOccluders(&drawer, set);
    check(volume(drawer, 0).size() == 5
        && occluder.edges[2].marks[1] == 0 && occluder.edges[3].marks[1] == 0,
        "every face uses first-face reference count with current-face reference storage");
    occluder.referenceVectors[1] = occluder.references[1];
    OccluderFixture second(3); geometry.groupCount = 2; geometry.groups[1] = &second.record;
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 2, "all geometry occluder records produce separate volumes");
    ObjectFixture secondHidden(6, 0, 10, 0.25f, &targetGeometry);
    check(culled(drawer, secondHidden), "second constructed volume hides an object outside first volume");
    geometry.groupCount = 1;
    OccluderFixture reversed(0, 5, true); geometry.groups[0] = &reversed.record;
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 1 && volume(drawer, 0).size() == 5,
        "reverse edge order and opposite selector preserve plane count");
    check(culled(drawer, hidden) && !culled(drawer, exposed), "opposite edge selector preserves visibility");
    const std::vector<Plane3>* const storage = drawer.m_vector50.data();
    drawer.m_float64 = 4.999f;
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 0, "distance beyond threshold rejects occluder");
    check(drawer.m_vector50.data() == storage && drawer.m_vector50.capacity() != 0,
          "frame clear retains outer vector capacity");
    check(!culled(drawer, hidden), "previous frame occluders no longer hide anything");
    drawer.m_float64 = 5;
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 1, "distance exactly at threshold accepts occluder");
    camera.matrix.m[10] = -1;
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 0, "looking away rejects forward-facing source plane");
    camera.matrix.m[10] = 1;
    float threshold = 1; std::memcpy(&drawer.m_dword6C, &threshold, sizeof(threshold));
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 0, "facing threshold equality is rejected");
    drawer.m_dword6C = 0;
    camera.matrix.m[14] = 5;
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 0, "camera on source plane is not behind an occluder");
    camera.matrix = identity(10, 0, 0); owner.position.x = 10;
    OccluderFixture translated(10); geometry.groups[0] = &translated.record;
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 1, "translated world records are relative to translated camera");
    hidden.position.x = 10; hidden.matrix = identity(10, 0, 10);
    outside.position.x = 13; outside.matrix = identity(13, 0, 10);
    check(culled(drawer, hidden) && !culled(drawer, outside), "translated silhouette keeps inside/outside distinction");
    geometry.groupCount = 0; unsigned int reads = owner.positionReads;
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 0 && reads == owner.positionReads, "no-record geometry clears frame and skips position lookup");
    ObjectFixture noGeometry(0, 0, 5, 1); set[0] = noGeometry.get();
    objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 0 && noGeometry.positionReads == 0, "missing geometry skips all builder object calls");
    set.clear(); objectDrawerBuildOccluders(&drawer, set);
    check(volumeCount(drawer) == 0, "empty frame remains clear");
    releaseBuiltVolumes(drawer);
}
struct OccluderOwner : IBase
{
    ObjectDrawer* drawer;
    EventsFixture* events;
    int refs, releases;
    OccluderOwner(EventsFixture& e) : drawer(0), events(&e), refs(1), releases(0) {}
    void addRef() override { ++refs; }
    int getRef() override { return refs; }
    int release() override
    {
        ++releases;
        check(events->listeners.empty(), "event listener detached before retained occluder release");
        check(drawer->m_vector50.capacity() == 0 && drawer->m_visibleSet.empty() && drawer->m_drawSet.empty(),
            "plane and object vectors cleared before retained occluder release");
        check(drawer->m_tree1C.size() == 3, "retained occluder released before trees");
        return --refs;
    }
    void* queryInterface(unsigned int) const override { unexpected("occluder queryInterface"); }
};
void teardownCases(EventsFixture& events)
{
    const size_t allocationStart = liveAllocations.size();
    OccluderOwner owner(events);
    {
        DrawerFixture drawer; owner.drawer = &drawer; drawer.setOccluder(&owner);
        check(owner.refs == 2, "setOccluder retains its owner");
        ViewFixture camera; drawer.setRenderView(&camera);
        GeometryFixture geometry; OccluderFixture occluder;
        geometry.groupCount = 1; geometry.groups[0] = &occluder.record;
        ObjectFixture object(0, 0, 5, 1, &geometry);
        drawer.m_visibleSet.push_back(object.get()); drawer.m_drawSet.push_back(object.get());
        objectDrawerBuildOccluders(&drawer, drawer.m_visibleSet);
        check(volumeCount(drawer) == 1, "destructor owns a populated nested plane vector");
        // Three [UNRESOLVED]-valued entries the destructor must still hold at the occluder release.
        const ObjectDrawerTreeValue value = { { 0, 0 } };
        drawer.m_tree1C[1] = value; drawer.m_tree1C[2] = value; drawer.m_tree1C[3] = value;
    }
    check(owner.refs == 1 && owner.releases == 1, "destructor releases retained occluder exactly once");
    check(liveAllocations.size() == allocationStart, "destructor leaves no engine-heap allocation behind");
}
void publicPassCases()
{
    DrawerFixture drawer; ViewFixture camera; drawer.setRenderView(&camera);
    GridFixture grid(camera); objectManager = reinterpret_cast<IBase*>(&grid);
    OccluderFixture occluder; GeometryFixture wallGeometry, hiddenGeometry, shownGeometry;
    wallGeometry.groupCount = 1; wallGeometry.groups[0] = &occluder.record;
    hiddenGeometry.key = 1; shownGeometry.key = 2;
    ObjectFixture wall(0, 0, 5, 2, &wallGeometry), hidden(0, 0, 10, 0.2f, &hiddenGeometry),
        shown(3, 0, 10, 0.2f, &shownGeometry), hiddenOther(0, 0, 12, 0.2f, &hiddenGeometry),
        shownOther(4, 0, 10, 0.2f, &shownGeometry);
    grid.collision = { shown.get(), hidden.get(), wall.get() };
    grid.noncollision = { shownOther.get(), hiddenOther.get() };
    drawer.m_dword80 = drawer.m_dword84 = 9;
    drawer.updateVisible(0.25f);
    check(drawer.m_visibleSet.size() == 2 && drawer.m_visibleSet[0] == wall.get()
        && drawer.m_visibleSet[1] == shown.get(), "actual updateVisible builds, sorts and culls collision candidates");
    check(drawer.m_culledCount == 1 && drawer.m_dword7C == 1, "updateVisible publishes removed-object counters");
    check(drawer.m_dword80 == 0 && drawer.m_dword84 == 0 && drawer.m_updateVisibleHasRun == 1,
        "updateVisible resets auxiliary counters and completes frame gate");
    drawer.draw(0.25f);
    check(drawer.m_drawSet.size() == 1 && drawer.m_drawSet[0] == shownOther.get(),
        "actual draw culls noncollision candidates against same frame volumes");
    check(drawer.m_dword78 == 1 && drawer.m_dword7C == 2, "draw publishes noncollision removal counters");
    check(!drawer.isCullingDebugEnabled(), "real culling leaves debug overlay disabled");
    const unsigned int savedCount = drawer.m_dword78;
    for (unsigned int count : { 0u, 1u, 255u, 256u, 257u })
    {
        drawer.m_dword78 = count;
        drawer.m_byte70 = 0;
        check(!drawer.isCullingDebugEnabled(), "culling count never enables the debug overlay");
        drawer.m_byte70 = 1;
        check(drawer.isCullingDebugEnabled(), "explicit debug flag enables overlay independently of count");
        drawer.m_byte70 = 0;
        check(!drawer.isCullingDebugEnabled(), "clearing debug flag disables overlay independently of count");
    }
    drawer.m_dword78 = savedCount;
    check(hidden.draws == 0 && hiddenOther.draws == 0 && wall.draws == 1
        && shown.draws == 1 && shownOther.draws == 1, "only survivors reach actual visual submission path");
    grid.collision.clear(); drawer.updateVisible(0.25f);
    check(drawer.m_visibleSet.empty() && volumeCount(drawer) == 0 && drawer.m_culledCount == 0,
        "empty next frame clears previously constructed occluders");
    drawer.m_byte60 = 0; grid.collision = { hidden.get(), shown.get() };
    drawer.updateVisible(0.25f);
    check(drawer.m_visibleSet.size() == 2 && drawer.m_culledCount == 0,
        "disabled public occlusion gate retains all queried objects");
    objectManager = 0;
}
}

// Narrow process-local providers. No reconstructed implementation is replaced.
namespace renddx9abi {
void* engineAlloc(unsigned int bytes)
{ void* ptr = std::malloc(bytes); if (ptr) liveAllocations.push_back(ptr); return ptr; }
void engineFree(void* ptr)
{
    if (!ptr) return;
    auto found = std::find(liveAllocations.begin(), liveAllocations.end(), ptr);
    if (found == liveAllocations.end()) unexpected("free of unowned/already-freed engine allocation");
    liveAllocations.erase(found); freedAllocations.push_back(ptr); std::free(ptr);
}
void frontier(const char* name, const void*, unsigned int) { unexpected(name); }
void logf(const char* format, ...)
{ va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args); }
}
namespace dice { namespace hfe {
IBase* objectManager = 0;
EventManager* eventManager = 0;
namespace geom {
void meshBoundsFinalize(MeshBounds&) { unexpected("meshBoundsFinalize: fixture bounds must be clean"); }
}
} }

int main()
{
    EventsFixture events; eventManager = reinterpret_cast<EventManager*>(&events);
    cullCases(); orderingCases(); comparisonPrecisionCases(); builderCases(); teardownCases(events);
    publicPassCases();
    check(events.listeners.empty() && events.registrations == events.unregistrations, "all drawer listeners detached");
    check(liveAllocations.empty(), "all engine allocations released");
    eventManager = 0;
    std::printf("ObjectDrawer occlusion: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
