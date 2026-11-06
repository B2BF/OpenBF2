// [TEMP] Real inline accessors with chosen native Information lifetime. No retail layout claim.
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/Information.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static unsigned int* flagSlot;
static float* strengthSlot;
static void** mobileSlot;
static unsigned char* opaque08;
static unsigned char* opaque14;
static unsigned int offsets[7];
static int checks;

__declspec(noreturn) static void unexpected(const char* method)
{
    std::fprintf(stderr, "UNEXPECTED %s\n", method);
    std::abort();
}
static void check(bool value, const char* label)
{
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FAIL %s\n", label);
        std::exit(2);
    }
}

// [TEMP] Layout-only baseline of the pre-change native declaration. Never instantiated,
// published or used for object access; comparison protects the existing downstream offsets.
struct LegacyInformationLayout
{
    virtual ~LegacyInformationLayout();
    unsigned char unwritten[20];
    double time;
    IAIObject* object;
    bool bool24;
    int int28;
    void* plugIn4;
    void* plugIn5;
    void* plugIn1;
    void* plugIn2;
    IPIPhysical* physical;
    IPIUnit* unit;
};

namespace dice { namespace hfe { namespace ai {
Information::Information(IAIObject* object, double time)
    : m_objectTypeFlags(0), m_strength(0.0f), m_time(time), m_object(object),
      m_isBooked(false), m_bookingPlayerId(-1), m_armament(nullptr), m_controlInfo(nullptr),
      m_cover(nullptr), m_mobile(nullptr), m_physical(nullptr), m_unit(nullptr)
{
    static_assert(sizeof(void*) == 4, "MSVC x86 only");
    static_assert(sizeof(Information) == sizeof(LegacyInformationLayout), "native size preserved");
    static_assert(alignof(Information) == alignof(LegacyInformationLayout), "native alignment preserved");
    static_assert(offsetof(Information, m_objectTypeFlags) == offsetof(LegacyInformationLayout, unwritten), "flag split");
    static_assert(offsetof(Information, m_strength) == offsetof(LegacyInformationLayout, unwritten) + 12, "float split");
    static_assert(offsetof(Information, m_time) == offsetof(LegacyInformationLayout, time), "time offset preserved");
    static_assert(offsetof(Information, m_object) == offsetof(LegacyInformationLayout, object), "object offset preserved");
    static_assert(offsetof(Information, m_isBooked) == offsetof(LegacyInformationLayout, bool24), "bool offset preserved");
    static_assert(offsetof(Information, m_bookingPlayerId) == offsetof(LegacyInformationLayout, int28), "integer offset preserved");
    static_assert(offsetof(Information, m_armament) == offsetof(LegacyInformationLayout, plugIn4), "plugin4 offset preserved");
    static_assert(offsetof(Information, m_controlInfo) == offsetof(LegacyInformationLayout, plugIn5), "plugin5 offset preserved");
    static_assert(offsetof(Information, m_cover) == offsetof(LegacyInformationLayout, plugIn1), "plugin1 offset preserved");
    static_assert(offsetof(Information, m_mobile) == offsetof(LegacyInformationLayout, plugIn2), "mobile offset preserved");
    static_assert(offsetof(Information, m_physical) == offsetof(LegacyInformationLayout, physical), "physical offset preserved");
    static_assert(offsetof(Information, m_unit) == offsetof(LegacyInformationLayout, unit), "unit offset preserved");
    flagSlot = &m_objectTypeFlags;
    strengthSlot = &m_strength;
    // Poison and verify the twelve bytes in fields these accessors must not read.
    static_assert(offsetof(Information, m_unwritten0C) == offsetof(Information, m_allowedTimeDiff) + 4, "+0x08..+0x0F contiguous");
    static_assert(offsetof(Information, m_basicTemp) == offsetof(Information, m_strength) + 4, "+0x14 follows the strength");
    mobileSlot = reinterpret_cast<void**>(&m_mobile);
    opaque08 = reinterpret_cast<unsigned char*>(&m_allowedTimeDiff);
    opaque14 = reinterpret_cast<unsigned char*>(&m_basicTemp);
    for (unsigned i = 0; i < 8; ++i) opaque08[i] = static_cast<unsigned char>(0xa0 + i);
    for (unsigned i = 0; i < 4; ++i) opaque14[i] = static_cast<unsigned char>(0xb0 + i);
    offsets[0] = sizeof(Information);
    offsets[1] = offsetof(Information, m_objectTypeFlags);
    offsets[2] = offsetof(Information, m_strength);
    offsets[3] = offsetof(Information, m_time);
    offsets[4] = offsetof(Information, m_object);
    offsets[5] = offsetof(Information, m_physical);
    offsets[6] = offsetof(Information, m_unit);
}
Information::~Information() {} // chosen lifetime: no owned production plugins installed
int Information::getInformationSort() { unexpected("Information::getInformationSort"); }
} } }

class NativeInformation : public Information
{
public:
    NativeInformation() : Information(nullptr, 23.0) {}
    bool setTime(double) override { unexpected("setTime(double)"); }
    bool setTime(double, float) override { unexpected("setTime(double,float)"); }
    double getTimeStamp() const override { unexpected("getTimeStamp"); }
    void addContact() override { unexpected("addContact"); }
    void removeContact() override { unexpected("removeContact"); }
    void removeAllContacts() override { unexpected("removeAllContacts"); }
    bool hasContact() const override { unexpected("hasContact"); }
    float getSecurity(double) override { unexpected("getSecurity(double)"); }
    float getSecurity() override { unexpected("getSecurity"); }
    Vec3& getPosition() override { unexpected("getPosition"); }
    float getAltitude() override { unexpected("getAltitude"); }
    float getAltitude(const Vec3&) override { unexpected("getAltitude(Vec3)"); }
    Mat4& getTransformation() override { unexpected("getTransformation"); }
    Vec3 getFaceing() override { unexpected("getFaceing"); }
    int getSide() const override { unexpected("getSide"); }
};

static unsigned int bits(float value)
{
    unsigned int result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
static float fromBits(unsigned int value)
{
    float result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

int main(int argc, char** argv)
{
    static_assert(std::is_same<decltype(&Information::getObjectTypeFlags), unsigned int (Information::*)() const>::value, "flags contract");
    static_assert(std::is_same<decltype(&Information::getStrength), float (Information::*)() const>::value, "float contract");
    static_assert(std::is_same<decltype(&Information::hasMobile), bool (Information::*)() const>::value, "presence contract");
    if (argc != 2) return 3;
    NativeInformation info;
    const Information& view = info;
    const char* mode = argv[1];
    if (!std::strcmp(mode, "native_layout")) {
        check(view.getUnit() == nullptr, "existing unit field unaffected");
        check(view.getPhysical() == nullptr, "existing physical field unaffected");
        std::printf("NATIVE_LAYOUT size=%u flags=%u strength=%u time=%u object=%u physical=%u unit=%u\n",
                    offsets[0], offsets[1], offsets[2], offsets[3], offsets[4], offsets[5], offsets[6]);
    } else if (!std::strcmp(mode, "flags_strength")) {
        const unsigned int patterns[] = { 0, 1, 0x80u, 0x200000u, 0x80000000u, 0xffffffffu };
        for (auto value : patterns) {
            *flagSlot = value;
            check(view.getObjectTypeFlags() == value, "full unsigned flag bits and fresh reads");
        }
        const unsigned int values[] = { 0, 0x80000000u, 0x3f800000u, 0xc1200000u, 0x7f800000u, 0xff800000u, 0x7fc12345u };
        for (auto value : values) {
            *strengthSlot = fromBits(value);
            check(bits(view.getStrength()) == value, "float field preserves tested bits and fresh reads");
        }
        check(view.getObjectTypeFlags() == 0xffffffffu, "strength writes do not overlap flags");
    } else if (!std::strcmp(mode, "mobile_presence")) {
        int opaqueMarker = 7; // pointer presence only: never treated as a plugin object
        check(!view.hasMobile(), "absent mobile");
        *mobileSlot = &opaqueMarker;
        check(view.hasMobile(), "present opaque pointer");
        check(opaqueMarker == 7, "presence does not dereference pointer");
        *mobileSlot = nullptr;
        check(!view.hasMobile(), "fresh absent mobile");
    } else return 3;
    for (unsigned i = 0; i < 8; ++i) check(opaque08[i] == 0xa0 + i, "first opaque bytes retained");
    for (unsigned i = 0; i < 4; ++i) check(opaque14[i] == 0xb0 + i, "last opaque bytes retained");
    std::printf("PASS %s checks=%d\n", mode, checks);
    return 0;
}
