// [TEMP] Behaviour fixture for the recovered slider bodies: SliderNode (BF2.exe 0x833660..0x833DD2)
// and BfFixedSliderNode (0x7D3D50..0x7D4B75, inherited by the HUD's Bf2FixedSliderNode). Retail HUD
// content builds no slider (HudElementsPlayer.con keeps its four inside beginrem/endrem), so no
// session reaches these bodies; this drives each of them (docs/tracks/game/frontier-closure.md).
// Portable: CTest bf2_slider_node_check on macOS, slider_node_behaviour.cmd on Windows.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define private public
#define protected public
#include "../BF2/External/BFMeme/SliderNode.h"
#include "../BF2/Menu/BfFixedSliderNode.h"
#include "../BF2/External/BFMeme/System.h"
#include "../BF2/External/BFMeme/Data.h"
#include "../BF2/External/BFMeme/Stream.h"
#undef protected
#undef private
#include "../BF2/External/BFMeme/Style.h"
#include "../BF2/IO/Input/Input.h"

namespace dice { namespace hfe { namespace io {
IBase* inputDeviceManager = nullptr;
} } }

using namespace dice::meme;
namespace io = dice::hfe::io;

static int checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
static bool approx(double a, double b) { return std::fabs(a - b) < 1e-4; }

namespace {
class FixtureSystem : public System
{
public:
    FixtureSystem() : System(800.0f, 600.0f) {}
    TypeDescriptor* getType() override { return Object::getClassType(); }
    void setPoint(float x, float y) { float* p = getPointerPosition(); p[0] = x; p[1] = y; }
};

class FixtureData : public Data
{
public:
    explicit FixtureData(float v) : Data("FixtureData"), value(v), sets(0) {}
    float getFloat(System*) override { return value; }
    bool setFloat(System*, float v) override { value = v; ++sets; return true; }
    float value;
    int sets;
};

// Records what a child receives: the rect, the state pointer and the context.
class ChildNode : public Node
{
public:
    ChildNode() : paints(0), updates(0), answer(true), lastState(nullptr), lastContext(nullptr) {}
    void paint(System*, const NodeState* state, const Rect* bounds, const Rect*,
               RenderContext* context) override
    { ++paints; lastRect = *bounds; lastState = state; lastContext = context;
      if (context) lastCopy = *context; }
    bool update(System*, const NodeState* state, const Rect* bounds, float) override
    { ++updates; lastRect = *bounds; lastState = state; return answer; }
    int paints, updates;
    bool answer;
    Rect lastRect;
    const NodeState* lastState;
    RenderContext* lastContext;
    RenderContext lastCopy;
};

class RecordingStream : public Stream
{
public:
    bool streamFloat(float*, const char* name) override { labels.push_back(name); return true; }
    bool streamData(System*, Data*&, const char* name, TypeDescriptor*) override
    { labels.push_back(name); return true; }
    bool streamNode0x84(System*, Node*&, const char* name, TypeDescriptor*) override
    { labels.push_back(name); return true; }
    bool streamNode0x88(System*, Node*&, const char* name, TypeDescriptor*) override
    { labels.push_back(name); return true; }
    std::vector<std::string> labels;
};

class FixtureMouse : public io::IAdvancedInputDevice
{
public:
    FixtureMouse() : down(true), downCalls(0), lastButton(io::IDButton_31) {}
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return nullptr; }
    io::InputDeviceTypes getType() const override { return io::IDTMouse; }
    bool setCooperativeMode(io::InputDeviceMode) override { return true; }
    io::InputDeviceMode getCooperativeMode() const override { return io::IDM_NONEXCLUSIVE; }
    bool activate() override { return true; }
    bool deActivate() override { return true; }
    void update(float) override {}
    bool isActive() const override { return true; }
    bool getNextInput(io::InputDeviceAxes&, io::InputDeviceButtons&,
                      io::InputDeviceKeys&, bool&) override { return false; }
    int getNumAxes() const override { return 0; }
    float readAxis(io::InputDeviceAxes, bool) const override { return 0.0f; }
    void peakXYAxis(float& x, float& y) override { x = 0.0f; y = 0.0f; }
    void setInvertAxis(io::InputDeviceAxes, bool) override {}
    bool getInvertAxis(io::InputDeviceAxes) const override { return false; }
    void setAxisScale(io::InputDeviceAxes, float) override {}
    float getAxisScale(io::InputDeviceAxes) const override { return 0.0f; }
    void setAxisAdd(io::InputDeviceAxes, float) override {}
    float getAxisAdd(io::InputDeviceAxes) const override { return 0.0f; }
    int getNumButtons() const override { return 1; }
    bool buttonDown(io::InputDeviceButtons button) const override
    { ++downCalls; lastButton = button; return down; }
    bool buttonPressed(io::InputDeviceButtons) const override { return false; }
    bool buttonReleased(io::InputDeviceButtons) const override { return false; }
    float buttonPressedSampleLatency(io::InputDeviceButtons) const override { return 0.0f; }
    bool down;
    mutable int downCalls;
    mutable io::InputDeviceButtons lastButton;
};

class FixtureInputManager : public io::IInputDeviceManager
{
public:
    FixtureInputManager() : answer(nullptr), lastFlags(io::IDFNone) {}
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return nullptr; }
    bool initDevices(unsigned int*) override { return true; }
    io::IInputDevice* getInputDevice(io::InputDeviceFlags flags) const override
    { lastFlags = flags; return answer; }
    unsigned int getInputDevices() const override { return 0; }
    bool activateDevices(unsigned int) override { return true; }
    bool deActivateDevices(unsigned int) override { return true; }
    void update(float) override {}
    unsigned int getActiveInputDevices() const override { return 0; }
    void* getAllDevices() override { return nullptr; }
    io::IInputDevice* answer;
    mutable io::InputDeviceFlags lastFlags;
};

const Rect kTrack = {0.0f, 0.0f, 150.0f, 20.0f, 0.0f};
const NodeState kIdle = {1.0f, 1.0f};
const NodeState kHeld = {-1.0f, -1.0f};

// Both classes share the same field names one level apart; one template drives either.
template <class Slider>
void setUp(Slider& node, FixtureData* data, float minimum, float maximum, float visible)
{
    node.m_data = data; node.m_minimum = minimum; node.m_maximum = maximum;
    node.m_thumbSpan = visible;
}

template <class Slider>
void commonCases(const char* name)
{
    std::printf("-- %s\n", name);
    // streamProperties: Node's "Next node", then the class's own rows in the shipped order.
    {
        Slider node; FixtureSystem system; RecordingStream stream;
        CHECK(node.streamProperties(&system, &stream));
        CHECK(stream.labels.size() >= 6);
        CHECK(stream.labels[0] == "Next node");
        CHECK(stream.labels[1] == "Cursor node");
        CHECK(stream.labels[2] == "Data");
        CHECK(stream.labels[3] == "Minimum value");
        CHECK(stream.labels[4] == "Maximum value");
        CHECK(stream.labels[5] == "Number visible");
    }
    // updateSelf without a held press: the thumb is placed from the Data. Visible 0.7 of [0,1] on a
    // 150 track: thumb 105, scale (150-105)/1, value 0.5 -> offset 22.5. No Data: nothing.
    {
        Slider node; FixtureSystem system; FixtureData data(0.5f);
        setUp(node, &data, 0.0f, 1.0f, 0.7f);
        CHECK(node.updateSelf(&system, &kIdle, &kTrack, 0.0f));
        CHECK(approx(node.m_thumbOffset, 22.5));
        CHECK(approx(node.thumbExtent(&kTrack.width), 105.0));
        Slider bare; CHECK(bare.updateSelf(&system, &kIdle, &kTrack, 0.0f));
    }
    // paint: the thumb rect (x + offset, width = visible share), the slider's own state; a plain
    // context passes through, an effect context gets a child context (parent, no effect, rate,
    // decoded state, 1.0).
    {
        Slider node; FixtureSystem system; ChildNode child; FixtureData data(0.0f);
        setUp(node, &data, 0.0f, 1.0f, 0.7f);
        node.m_child1 = &child; node.m_thumbOffset = 10.0f;
        RenderContext plain = {nullptr, nullptr, 0, 0, 0, 0, 0};
        node.paint(&system, &kIdle, &kTrack, nullptr, &plain);
        CHECK(child.paints == 1 && child.lastContext == &plain && child.lastState == &node.m_state);
        CHECK(approx(child.lastRect.x, 10.0) && approx(child.lastRect.width, 105.0));
        Effect* effect = reinterpret_cast<Effect*>(&data);   // only tested for non-null
        RenderContext withEffect = {nullptr, effect, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        node.m_state.target = 1.0f; node.m_state.current = -1.5f;   // decodes to 0.5, rate +3
        node.paint(&system, &kIdle, &kTrack, nullptr, &withEffect);
        CHECK(child.paints == 2 && child.lastContext != &withEffect);
        CHECK(child.lastCopy.parent == &withEffect && child.lastCopy.effect == nullptr);
        CHECK(child.lastCopy.field08 == 2.0f && child.lastCopy.field0c == 3.0f);
        CHECK(approx(child.lastCopy.field10, 3.0) && approx(child.lastCopy.field14, 0.5));
        CHECK(child.lastCopy.field18 == 1.0f);
    }
    // update: the thumb updates with the thumb rect; its refusal stops the walk.
    {
        Slider node; FixtureSystem system; ChildNode child; FixtureData data(0.0f);
        setUp(node, &data, 0.0f, 1.0f, 0.7f);
        node.m_child1 = &child;
        CHECK(node.update(&system, &kIdle, &kTrack, 0.016f));
        CHECK(child.updates == 1 && approx(child.lastRect.width, 105.0));
        child.answer = false;
        CHECK(!node.update(&system, &kIdle, &kTrack, 0.016f));
    }
    // onEvent1: a non-primary press or one already held is refused; a primary press beside the
    // thumb locks the pointer and pages by whole thumb widths. Pointer 140 on a thumb at 0 (page
    // 105): grab 140 -> thumb 105 clamps to 150-105 = 45, grab 140-45 = 95, which is <= 105.
    {
        Slider node; FixtureSystem system; FixtureData data(0.0f);
        setUp(node, &data, 0.0f, 1.0f, 0.7f);
        system.setPoint(140.0f, 5.0f);
        CHECK(!node.onEvent1(&system, &kTrack, &kIdle, 2, 1, 1));
        CHECK(!node.onEvent1(&system, &kTrack, &kIdle, 1, 1, 2));
        CHECK(system.getLockCount() == 0);
        CHECK(node.onEvent1(&system, &kTrack, &kIdle, 1, 1, 1));
        CHECK(system.getLockCount() == 1);
        CHECK(approx(node.m_grabOffset, 95.0));
        system.lockPointer();   // now held: a second press with a pressed state is refused
        CHECK(!node.onEvent1(&system, &kTrack, &kHeld, 1, 1, 1));
        // A press left of the thumb pages down and stops at 0.
        Slider low; setUp(low, &data, 0.0f, 1.0f, 0.2f);   // page 30
        low.m_thumbOffset = 40.0f;
        FixtureSystem other; other.setPoint(5.0f, 5.0f);
        CHECK(low.onEvent1(&other, &kTrack, &kIdle, 1, 1, 1));
        CHECK(low.m_grabOffset >= 0.0f);
    }
    // Drag: with the pointer locked and a pressed parent state, the thumb follows the pointer less
    // the grab offset, clamped to [0, width - thumb]; the value written back is offset/scale + min.
    {
        Slider node; FixtureSystem system; FixtureData data(0.0f);
        setUp(node, &data, 0.0f, 1.0f, 0.7f);
        system.lockPointer();
        node.m_grabOffset = 10.0f;
        system.setPoint(60.0f, 5.0f);       // offset 50 > limit 45
        CHECK(node.updateSelf(&system, &kHeld, &kTrack, 0.0f));
        CHECK(approx(node.m_thumbOffset, 45.0) && approx(data.value, 1.0));
        system.setPoint(-50.0f, 5.0f);      // clamped to the track, offset -10 -> 0
        node.updateSelf(&system, &kHeld, &kTrack, 0.0f);
        CHECK(approx(node.m_thumbOffset, 0.0) && approx(data.value, 0.0));
    }
}

void sliderOnlyCases()
{
    std::printf("-- SliderNode release\n");
    // The base release unlocks without checking the count (no zero guard).
    SliderNode node; FixtureSystem system; FixtureData data(0.5f);
    setUp(node, &data, 0.0f, 1.0f, 0.7f);
    CHECK(!node.onEvent2(&system, &kTrack, &kIdle, 1, 1));
    CHECK(system.getLockCount() == -1 && approx(node.m_thumbOffset, 22.5));
    CHECK(!node.onEvent3(&system, &kTrack, &kIdle, 2, 1));
    CHECK(system.getLockCount() == -1);
}

void fixedOnlyCases()
{
    std::printf("-- BfFixedSliderNode step, release, missed release\n");
    // The sixth stream row is the step, "Interval".
    {
        BfFixedSliderNode node; FixtureSystem system; RecordingStream stream;
        CHECK(node.streamProperties(&system, &stream));
        CHECK(stream.labels.size() == 7 && stream.labels[6] == "Interval");
    }
    // The fixed step: a dragged value 20/45 = 0.444 snaps up to 0.5 with a 0.25 step; a value
    // over the maximum is clamped; a value <= 0 snaps to 0.
    {
        BfFixedSliderNode node; FixtureSystem system; FixtureData data(0.0f);
        setUp(node, &data, 0.0f, 1.0f, 0.7f); node.m_step = 0.25f;
        system.lockPointer();
        system.setPoint(20.0f, 5.0f);
        CHECK(node.updateSelf(&system, &kHeld, &kTrack, 0.0f));
        CHECK(approx(data.value, 0.5));
        data.value = 3.0f;
        CHECK(node.updateSelf(&system, &kIdle, &kTrack, 0.0f));   // placed, then clamped
        CHECK(approx(data.value, 1.0));
        system.setPoint(-5.0f, 5.0f);
        node.updateSelf(&system, &kHeld, &kTrack, 0.0f);
        CHECK(approx(data.value, 0.0));
    }
    // Release: only while the pointer is locked.
    {
        BfFixedSliderNode node; FixtureSystem system; FixtureData data(0.5f);
        setUp(node, &data, 0.0f, 1.0f, 0.7f);
        CHECK(!node.onEvent2(&system, &kTrack, &kIdle, 1, 1));
        CHECK(system.getLockCount() == 0);
        system.lockPointer();
        CHECK(!node.onEvent3(&system, &kTrack, &kIdle, 1, 1));
        CHECK(system.getLockCount() == 0 && approx(node.m_thumbOffset, 22.5));
    }
    // Missed release: while held, the timer passes 0.25 s; with the left button (IDButton_0 of
    // IDFMouse) up and the pointer locked the release is synthesised, and the timer restarts.
    {
        BfFixedSliderNode node; FixtureSystem system; FixtureData data(0.5f);
        setUp(node, &data, 0.0f, 1.0f, 0.7f); node.m_step = 0.01f;
        FixtureInputManager manager; FixtureMouse mouse; manager.answer = &mouse;
        io::inputDeviceManager = &manager;
        system.lockPointer();
        system.setPoint(40.0f, 5.0f);
        node.updateSelf(&system, &kHeld, &kTrack, 0.2f);
        CHECK(approx(node.m_releaseTimer, 0.2) && mouse.downCalls == 0);
        node.updateSelf(&system, &kHeld, &kTrack, 0.1f);           // 0.3 > 0.25, button down
        CHECK(mouse.downCalls == 1 && mouse.lastButton == io::IDButton_0);
        CHECK(manager.lastFlags == io::IDFMouse);
        CHECK(node.m_releaseTimer == 0.0f && system.getLockCount() == 1);
        mouse.down = false;
        node.updateSelf(&system, &kHeld, &kTrack, 0.3f);           // button up: synthesised
        CHECK(system.getLockCount() == 0 && node.m_releaseTimer == 0.0f);
        io::inputDeviceManager = nullptr;
    }
}
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // a crash must not swallow the progress lines
    commonCases<SliderNode>("SliderNode");
    commonCases<BfFixedSliderNode>("BfFixedSliderNode");
    sliderOnlyCases();
    fixedOnlyCases();
    std::printf("slider node behaviour %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
