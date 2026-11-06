// [TEMP] [CHOSEN, not recovered] Native typed observers around the whole production IPICover.cpp.
// Information/IBFTimer lifetimes are chosen here; IPICover and PlanePolygon bodies are unmodified.
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPICover.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/Information.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/BFTimer.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/IAIObject.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/PlanePolygon.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <Windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "MSVC x86 fixture only");
static_assert(sizeof(IPICover) == 12, "paired concrete cover layout");

static std::string calls;
static unsigned checks;
static bool printPhysicalTrace;
[[noreturn]] static void unexpected(const char* text)
{
    std::fprintf(stderr, "UNEXPECTED %s\n", text); std::fflush(stderr); std::abort();
}
static void trace(const std::string& text) { if (!calls.empty()) calls += ','; calls += text; }
static void check(bool value, const char* text)
{
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL %s\nTRACE %s\n", text, calls.c_str()); std::exit(2); }
}
static void expect(const char* text)
{
    check(calls == text, text); calls.clear();
}
static unsigned bits(float f) { unsigned u; std::memcpy(&u, &f, 4); return u; }
static float number(unsigned u) { float f; std::memcpy(&f, &u, 4); return f; }
static unsigned long long bits64(double f) { unsigned long long u; std::memcpy(&u, &f, 8); return u; }
static double number64(unsigned long long u) { double f; std::memcpy(&f, &u, 8); return f; }

#include "ipicover_observers.inl"

static const unsigned portalId = 0xFEDCBA98u, vertexId = 0x87654321u;
static std::vector<Vec3>* expectedVector;
static ObservedInformation* mutateInformation;
static IAIObject* replacementOwner;
static IPICover* observedPlugin;
static IPlanePolygon* expectedOldOutline;
static void (*onPolygonDelete)();
static const char* deletingLabel;
static void* deletingPointer;
static unsigned polygonDeletes;
static IBFTimer* replacementTimer;

namespace dice { namespace hfe { namespace ai {
// [TEMP] Chosen native base lifetimes. The production Information/registry TUs are outside this
// focused link. No production IPICover/PlanePolygon definition is supplied by the fixture.
Information::Information(IAIObject* object, double time)
    : m_objectTypeFlags(0), m_allowedTimeDiff(0), m_strength(0), m_basicTemp(0), m_time(time),
      m_object(object), m_isBooked(false), m_bookingPlayerId(-1), m_armament(nullptr),
      m_controlInfo(nullptr), m_cover(nullptr), m_mobile(nullptr), m_physical(nullptr), m_unit(nullptr) {}
Information::~Information() {}
int Information::getInformationSort() { unexpected("Information::getInformationSort"); }
IBFTimer::IBFTimer() {}
IBFTimer::~IBFTimer() {}
void* CommonSingletonServer::m_iBFTimer = nullptr;

bool ObservedObject::isCover() const
{
    trace(std::string(label) + ".isCover"); if (onIsCover) onIsCover(); return coverFlag;
}
IAIObjectCover* ObservedObject::getCover() const
{
    trace(std::string(label) + ".cover"); if (onGetCover) onGetCover(); return cover;
}
IAIObjectPhysical* ObservedObject::getPhysical() const
{
    trace(std::string(label) + ".physical"); return physical;
}
Mat4& ObservedInformation::getTransformation()
{
    trace("transformation");
    if (printPhysicalTrace) { std::printf("TRACE %s\n", calls.c_str()); std::fflush(stdout); }
    return transform;
}
IPlanePolygon* ObservedCover::getObjectOutline()
{
    trace(std::string(label) + ".outline"); if (onOutline) onOutline(); return outline;
}
IPlanePolygon* ObservedCover::getLocalObjectOutline() { trace("local"); return local; }
float ObservedCover::getCoverValue() const { trace("value"); return -31.25f; }
float ObservedCover::getHeight() const { trace("height"); return 17.75f; }
float ObservedCover::getTransparency() const { trace("transparency"); return -0.5f; }
unsigned int ObservedCover::getPortalCount() const { trace("count"); return 0x80000009u; }
unsigned int ObservedCover::getPortalVertexCount(unsigned int portal) const
{ check(portal == portalId, "vertex-count argument"); trace("vertexCount"); return 0xFFFFFFFFu; }
const Vec3& ObservedCover::getPortalVertex(unsigned int portal, unsigned int vertex) const
{
    check(portal == portalId && vertex == vertexId, "vertex arguments");
    trace("vertex"); return this->vertex;
}
void ObservedCover::getPortal(unsigned int portal, std::vector<Vec3>& out) const
{
    check(portal == portalId && &out == expectedVector, "portal ID and output vector identity");
    trace("portal"); out.push_back(vertex);
}
const Vec3& ObservedCover::getPortalCenter(unsigned int portal) const
{ check(portal == portalId, "center argument"); trace("center"); return center; }
const Vec3& ObservedCover::getPortalNormal(unsigned int portal) const
{ check(portal == portalId, "normal argument"); trace("normal"); return normal; }
float ObservedCover::getPortalXZExtentR(unsigned int portal) const
{ check(portal == portalId, "right argument"); trace("right"); return 4.25f; }
float ObservedCover::getPortalXZExtentL(unsigned int portal) const
{ check(portal == portalId, "left argument"); trace("left"); return -8.5f; }
int ObservedCover::getPathfindingPlaneCount() const { trace("planeCount"); return -77; }
const Vec3& ObservedCover::getPathfindingPlaneCorner(int plane, int corner) const
{
    check(plane == -9 && corner == -13, "signed plane/corner arguments");
    trace("corner"); return this->corner;
}
float ObservedPhysical::getVolume() const { trace("volume"); return -23.5f; }
BoundingBox* ObservedPhysical::getCurrentBoundingBox()
{
    trace("boundingBox");
    // The real frontier accepts this opaque typed null but never inspects it. No BoundingBox
    // object or layout is fabricated; scalar-box deletion beyond the frontier is unexercised.
    return nullptr;
}
double ObservedTimer::getGameTime() const
{ trace(label); if (callback) callback(); return now; }
} } }

class TrackedPolygon : public IPlanePolygon
{
public:
    explicit TrackedPolygon(const char* label) : label(label) {}
    ~TrackedPolygon() override
    {
        trace(std::string("destroy.") + label);
        if (observedPlugin) check(observedPlugin->getOutline() == this, "old pointer retained during deletion");
        if (onPolygonDelete) onPolygonDelete();
        ++polygonDeletes; deletingPointer = this; deletingLabel = label;
    }
    static void operator delete(void* p) noexcept
    {
        check(p == deletingPointer, "scalar virtual-delete identity");
        trace(std::string("free.") + deletingLabel); ::operator delete(p);
    }
    const char* label;
    IPlanePolygon* fromTransform(const Mat4&) const override { unexpected("outline transform"); }
    void setVertices(std::list<Vec2>&) override { unexpected("outline setVertices"); }
    const std::list<Vec2>& getVertices() const override { unexpected("outline vertices"); }
    void getScaledVertices(std::list<Vec2>&, const Vec2&) const override { unexpected("outline scale"); }
    bool intersects(const Vec2&, const Vec2&) const override { unexpected("outline intersects"); }
    bool isInside(const Vec2&, const Vec2&) const override { unexpected("outline inside"); }
};

static void replaceOwner() { mutateInformation->owner(replacementOwner); }
static void replaceTimerAndOwner()
{
    CommonSingletonServer::m_iBFTimer = replacementTimer; replaceOwner();
}
static void inspectOldPointer()
{ check(observedPlugin->getOutline() == expectedOldOutline, "replacement assignment follows acquisition"); }

struct Context
{
    ObservedObject object{"object"}, other{"other"};
    ObservedCover cover{"cover"}, otherCover{"otherCover"};
    ObservedPhysical physical;
    ObservedInformation info{&object};
    Context()
    {
        object.cover = &cover; object.physical = &physical;
        other.cover = &otherCover; other.physical = &physical;
    }
};

static void constructorCase(bool capture, bool nullOutline)
{
    Context c;
    if (!nullOutline) c.cover.outline = new TrackedPolygon("owned");
    if (capture) { mutateInformation = &c.info; replacementOwner = &c.other; c.object.onIsCover = replaceOwner; }
    IPICover* p = new IPICover(&c.info);
    check(sizeof(*p) == 12, "native IPICover size");
    check(p->getType() == 1, "cover type");
    check(p->getOutline() == c.cover.outline, "constructor takes exact owned pointer");
    expect("object.isCover,object.cover,cover.outline");
    delete static_cast<InformationPlugIn*>(p);
    expect(nullOutline ? "" : "destroy.owned,free.owned");
    check(polygonDeletes == (nullOutline ? 0u : 1u), "one owned outline destruction");
}

static void replacementCase(bool replaceWithNull, bool useTimer)
{
    Context c;
    c.cover.outline = new TrackedPolygon("old");
    c.otherCover.outline = replaceWithNull ? nullptr : new TrackedPolygon("new");
    IPICover p(&c.info); calls.clear();
    observedPlugin = &p; expectedOldOutline = p.getOutline();
    mutateInformation = &c.info; replacementOwner = &c.other;
    c.otherCover.onOutline = inspectOldPointer;
    ObservedTimer first, second;
    first.now = number64(0x8000000000000000ull); second.label = "wrongTimer";
    CommonSingletonServer::m_iBFTimer = &first;
    if (useTimer) {
        first.callback = replaceTimerAndOwner; replacementTimer = &second;
        p.setTime();
    } else {
        onPolygonDelete = replaceOwner;
        p.setTime(number64(0x7ff8123456789abcull)); // ignored, including NaN payload
    }
    expect(useTimer ? "timer,destroy.old,free.old,other.cover,otherCover.outline" :
                      "destroy.old,free.old,other.cover,otherCover.outline");
    check(p.getOutline() == c.otherCover.outline, "replacement pointer");
    check(polygonDeletes == 1, "old outline deleted once before acquisition");
    if (useTimer) check(CommonSingletonServer::m_iBFTimer == &second, "timer callback global swap survives");
    onPolygonDelete = nullptr; observedPlugin = nullptr;
    // Automatic production teardown below; its observations are checked by caller after return.
}

class DispatchCover : public IPICover
{
public:
    explicit DispatchCover(Information* info) : IPICover(info) {}
    void setTime(double value) override { trace("virtual.setTime"); received = bits64(value); ++count; }
    unsigned long long received = 0; unsigned count = 0;
};
static void timerVirtual()
{
    Context c; DispatchCover p(&c.info); calls.clear();
    ObservedTimer timer, second; second.label = "wrongTimer";
    mutateInformation = &c.info; replacementOwner = &c.other;
    replacementTimer = &second; timer.callback = replaceTimerAndOwner;
    const unsigned long long values[] = {0x8000000000000000ull, 0x7ff8123456789abcull, 0x3ff0000000000001ull};
    for (auto value : values) {
        timer.now = number64(value); CommonSingletonServer::m_iBFTimer = &timer;
        p.IPICover::setTime();
        check(p.received == value, "exact timer double reaches virtual overload");
        expect("timer,virtual.setTime");
    }
    check(p.count == 3, "one virtual dispatch per no-argument call");
}

static void forwards()
{
    Context c; TrackedPolygon local("local"); c.cover.local = &local;
    IPICover p(&c.info); calls.clear();
    check(p.getLocalOutline() == &local, "borrowed local outline identity"); expect("object.cover,local");
    check(bits(p.getVolume()) == bits(-23.5f), "volume result"); expect("object.physical,volume");
    check(bits(p.getCoverValue()) == bits(-31.25f), "cover value"); expect("object.cover,value");
    check(bits(p.getHeight()) == bits(17.75f), "height"); expect("object.cover,height");
    check(p.getPortalCount() == 0x80000009u, "full unsigned count"); expect("object.cover,count");
    check(p.getPortalVertexCount(portalId) == 0xFFFFFFFFu, "full unsigned vertex count"); expect("object.cover,vertexCount");
    check(&p.getPortalVertex(portalId, vertexId) == &c.cover.vertex, "borrowed vertex identity"); expect("object.cover,vertex");
    std::vector<Vec3> out(1, Vec3{91,92,93}); expectedVector = &out;
    p.getPortal(portalId, out);
    check(out.size() == 2 && out[0].x == 91 && out[1].x == 1, "caller vector preserved and appended"); expect("object.cover,portal");
    check(&p.getPortalCenter(portalId) == &c.cover.center, "borrowed center identity"); expect("object.cover,center");
    check(&p.getPortalNormal(portalId) == &c.cover.normal, "borrowed normal identity"); expect("object.cover,normal");
    check(bits(p.getPortalXZExtentR(portalId)) == bits(4.25f), "right extent"); expect("object.cover,right");
    check(bits(p.getPortalXZExtentL(portalId)) == bits(-8.5f), "left extent"); expect("object.cover,left");
    check(bits(p.getTransparency()) == bits(-0.5f), "transparency"); expect("object.cover,transparency");
    check(p.getPathfindingPlaneCount() == -77, "signed plane count"); expect("object.cover,planeCount");
    check(&p.getPathfindingPlaneCorner(-9,-13) == &c.cover.corner, "borrowed corner identity"); expect("object.cover,corner");
    check(polygonDeletes == 0, "local borrowed outline not deleted by forwarding");
}

static void freshOwner()
{
    Context c; IPICover p(&c.info); calls.clear(); c.info.owner(&c.other);
    mutateInformation = &c.info; replacementOwner = &c.object; c.other.onGetCover = replaceOwner;
    check(&p.getPortalCenter(portalId) == &c.otherCover.center, "cover retained across owner callback");
    expect("other.cover,center");
    check(&p.getPortalCenter(portalId) == &c.cover.center, "next query reads current owner");
    expect("object.cover,center");
}

static void direction(const char* mode)
{
    Context c; IPICover p(&c.info); calls.clear();
    unsigned samples[2][2];
    if (!std::strcmp(mode,"direction_finite")) { samples[0][0]=bits(3.25f); samples[0][1]=bits(-7.5f); samples[1][0]=bits(-2.5f); samples[1][1]=bits(11.0f); }
    else if (!std::strcmp(mode,"direction_zero")) { samples[0][0]=0; samples[0][1]=0x80000000; samples[1][0]=0x80000000; samples[1][1]=0; }
    else { samples[0][0]=0x7fc12345; samples[0][1]=0xffc54321; samples[1][0]=0xffc12345; samples[1][1]=0x7fc54321; }
    for (auto& sample : samples) {
        c.cover.normal = Vec3{number(sample[0]), number(0x7fc00001), number(sample[1])};
        const Vec3 value = p.getPortalExtentDirection(portalId);
        check(bits(value.x) == sample[1], "direction x copies normal z bits");
        check(bits(value.y) == 0, "direction y is positive zero");
        check(bits(value.z) == (sample[0] ^ 0x80000000u), "direction z flips normal x sign bit");
        expect("object.cover,normal");
    }
}

static void negative(const char* mode)
{
    Context c;
    std::printf("READY %s\n", mode); std::fflush(stdout);
    if (!std::strcmp(mode,"null_information")) { IPICover p(nullptr); check(p.getType() == 1,"unreachable"); }
    else if (!std::strcmp(mode,"null_object")) { c.info.owner(nullptr); IPICover p(&c.info); check(p.getOutline()!=nullptr,"unreachable"); }
    else if (!std::strcmp(mode,"null_cover")) { c.object.cover = nullptr; IPICover p(&c.info); check(p.getOutline()!=nullptr,"unreachable"); }
    else if (!std::strcmp(mode,"null_physical")) { c.object.coverFlag=false; c.object.physical=nullptr; IPICover p(&c.info); check(p.getOutline()!=nullptr,"unreachable"); }
    else if (!std::strcmp(mode,"null_timer")) { IPICover p(&c.info); calls.clear(); CommonSingletonServer::m_iBFTimer=nullptr; p.setTime(); }
    else if (!std::strcmp(mode,"physical_frontier")) { c.object.coverFlag=false; printPhysicalTrace=true; IPICover p(&c.info); check(p.getOutline()!=nullptr,"unreachable"); }
    unexpected("negative child returned");
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    if (argc != 2) return 89;
    const char* mode=argv[1];
    if (!std::strcmp(mode,"constructor")) constructorCase(false,false);
    else if (!std::strcmp(mode,"constructor_capture")) constructorCase(true,false);
    else if (!std::strcmp(mode,"null_outline")) constructorCase(false,true);
    else if (!std::strcmp(mode,"replacement") || !std::strcmp(mode,"replacement_null") || !std::strcmp(mode,"timer_replacement")) {
        const bool isNull = !std::strcmp(mode,"replacement_null");
        replacementCase(isNull, !std::strcmp(mode,"timer_replacement"));
        expect(isNull ? "" : "destroy.new,free.new");
        check(polygonDeletes == (isNull ? 1u : 2u), "replacement teardown ownership");
    }
    else if (!std::strcmp(mode,"timer_virtual")) timerVirtual();
    else if (!std::strcmp(mode,"forwards")) forwards();
    else if (!std::strcmp(mode,"fresh_owner")) freshOwner();
    else if (!std::strncmp(mode,"direction_",10)) direction(mode);
    else negative(mode);
    std::printf("PASS %s checks=%u\n", mode, checks);
    return 0;
}
