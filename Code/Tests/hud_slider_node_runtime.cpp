// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: HUD-SLIDER-FIXTURE]
// Tests slider layout and builder bindings with real nodes and data.
// The frame and event bodies themselves are covered by slider_node_behaviour.cpp.
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <windows.h>
#include "../BF2/Core/Debug.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Game/HudInformationLayer.h"
#define private public
#define protected public
#include "../BF2/Menu/Bf2HudBuilder.h"
#include "../BF2/Menu/Bf2FixedSliderNode.h"
#include "../BF2/Menu/BfFixedSliderNode.h"
#include "../BF2/External/BFMeme/SliderNode.h"
#include "../BF2/Menu/Bf2TransformNode.h"
#include "../BF2/Menu/Bf2PictureNode.h"
#include "../BF2/External/BFMeme/FloatData.h"
#undef protected
#undef private
#include "../BF2/External/BFMeme/System.h"
#include "../BF2/External/BFMeme/Node.h"
#include "../BF2/External/BFMeme/Style.h"
#include "../BF2/External/BFMeme/Quad.h"
#include "../BF2/External/BFMeme/TypeDescriptor.h"
#include "../BF2/External/BFMeme/NamedObjectRegistry.h"
#include "../BF2/Menu/Bf2HudBuilder.cpp"

namespace dice { namespace hfe {
EventManager* eventManager = nullptr;
class ServerSettings;
ServerSettings* serverSettings = nullptr;
class IGameLogic;
IGameLogic* gameLogic = nullptr;
hud::BF2HudManager* hudManager = nullptr;
} }

using namespace dice::meme;
using namespace dice::hfe;
using dice::hfe::hud::Bf2HudBuilder;

static int checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

class PlainSystem : public System
{
public:
    explicit PlainSystem(NameTreeNode& registry) : System(800, 600)
    { m_registry = &registry; }
    TypeDescriptor* getType() override { return Object::getClassType(); }
    int loadTexture(const char*, bool) override { return 200; }
};
class RecordingEvents : public EventManager {};
namespace dice { namespace hfe { IBase* g_hudInformationLayer = nullptr; } }
static HudInformationLayer* information;

// the three classes
static void sliderLayoutCases()
{
    // Every member is a float or a pointer, so retail and native agree.
    CHECK(sizeof(SliderNode) == 44);
    CHECK(sizeof(BfFixedSliderNode) == 88);
    CHECK(sizeof(Bf2FixedSliderNode) == 92);

    CHECK(std::strcmp(SliderNode::getClassType()->getName(),
                      "dice::meme::SliderNode") == 0);
    CHECK(std::strcmp(BfFixedSliderNode::getClassType()->getName(),
                      "dice::meme::BfFixedSliderNode") == 0);
    CHECK(std::strcmp(Bf2FixedSliderNode::getClassType()->getName(),
                      "dice::meme::Bf2FixedSliderNode") == 0);
    // The attested chain: Node -> SliderNode -> BfFixedSliderNode -> Bf2FixedSliderNode.
    CHECK(SliderNode::getClassType()->getBaseType() == Node::getClassType());
    CHECK(BfFixedSliderNode::getClassType()->getBaseType() == SliderNode::getClassType());
    CHECK(Bf2FixedSliderNode::getClassType()->getBaseType() == BfFixedSliderNode::getClassType());
    CHECK(SliderNode::getClassType()->getInstanceSize() == 44);
    CHECK(BfFixedSliderNode::getClassType()->getInstanceSize() == 88);
    CHECK(Bf2FixedSliderNode::getClassType()->getInstanceSize() == 92);

    {   // 8337D0: three zeroes, two 1.0s, two zeroes, two 1.0s.
        SliderNode node;
        CHECK(node.m_grabOffset == 0.0f && node.m_thumbOffset == 0.0f);
        CHECK(node.m_minimum == 0.0f && node.m_maximum == 1.0f);
        CHECK(node.m_thumbSpan == 1.0f);
        CHECK(node.m_data == nullptr && node.m_child1 == nullptr);
        CHECK(node.m_state.target == 1.0f && node.m_state.current == 1.0f);
        CHECK(node.m_next == nullptr);
        CHECK(node.getObjectSize() == 44 && node.getChildCount() == 2);
        CHECK(node.getType() == SliderNode::getClassType());
    }
    {   // 7D4210: the same shape one level down, plus the fixed step at +40.
        BfFixedSliderNode node;
        CHECK(node.m_grabOffset == 0.0f && node.m_thumbOffset == 0.0f);
        CHECK(node.m_minimum == 0.0f && node.m_maximum == 1.0f);
        CHECK(node.m_thumbSpan == 1.0f && node.m_step == 1.0f);
        CHECK(node.m_data == nullptr && node.m_child1 == nullptr);
        CHECK(node.m_state.target == 1.0f && node.m_state.current == 1.0f);
        CHECK(node.m_releaseTimer == 0.0f);
        CHECK(node.getObjectSize() == 88);
        // getChildCount is NOT overridden here: the base's shared 828A70 stands.
        CHECK(node.getChildCount() == 2);
        CHECK(node.getType() == BfFixedSliderNode::getClassType());
    }
    {   // The real factory 7CDC80: a null name and four zeroes.
        RuntimeObject* made = Bf2FixedSliderNode::getClassType()->create();
        Bf2FixedSliderNode* node = static_cast<Bf2FixedSliderNode*>(made);
        CHECK(node && node->m_name == nullptr);
        CHECK(node && node->getName() == nullptr);
        CHECK(node && node->m_minimum == 0.0f && node->m_maximum == 0.0f);
        CHECK(node && node->m_thumbSpan == 0.0f && node->m_step == 0.0f);
        CHECK(node && node->getObjectSize() == 92);
        ::operator delete(node);   // no destructor override; nothing to free
    }
    {   // 7CDC10: the four floats land on the inherited block at +34..+40.
        Bf2FixedSliderNode node("RedSliderSliderNode", 0.0f, 1.0f, 0.7f, 0.01f);
        CHECK(std::strcmp(node.m_name, "RedSliderSliderNode") == 0);
        CHECK(std::strcmp(node.getName(), "RedSliderSliderNode") == 0);
        CHECK(node.m_minimum == 0.0f && node.m_maximum == 1.0f);
        CHECK(node.m_thumbSpan == 0.7f && node.m_step == 0.01f);
        // The base's own nine words are constructed and then never read again.
        CHECK(node.SliderNode::m_maximum == 1.0f && node.SliderNode::m_thumbSpan == 1.0f);
        CHECK(node.slot_0x24());
        CHECK(node.getType() == Bf2FixedSliderNode::getClassType());
        CHECK(node.setName("Renamed") && std::strcmp(node.m_name, "Renamed") == 0);
        CHECK(node.setName(nullptr) && node.m_name == nullptr);
        ::operator delete(node.m_name);
        node.m_name = nullptr;
    }
}

static void sliderSlotCases()
{
    Bf2FixedSliderNode node("Slots", 0.0f, 1.0f, 0.5f, 0.1f);
    Bf2TransformNode thumb("Thumb", 0, 0, 1, 1);
    Bf2TransformNode track("Track", 0, 0, 1, 1);

    // Not an array and not a range check: index 1 is the thumb, every other
    // index - zero, negative, out of range - is Node's own +04 slot.
    node.setChild(1, &thumb);
    CHECK(node.m_child1 == &thumb && node.getChild(1) == &thumb);
    node.setChild(0, &track);
    CHECK(node.m_next == &track && node.getChild(0) == &track);
    CHECK(node.getChild(2) == &track);      // the getter's fallback is +04
    CHECK(node.getChild(-1) == &track);
    // The setter's fallback differs: any other index writes nothing at all.
    node.setChild(5, nullptr);
    CHECK(node.m_next == &track && node.m_child1 == &thumb);
    node.setChild(-3, nullptr);
    CHECK(node.m_next == &track && node.m_child1 == &thumb);

    // The one added slot, +C8. It writes the inherited m_data at +44 and takes
    // a null without a test.
    float bound = 0.25f;
    FloatRefData data(&bound, "sliderData");
    CHECK(node.setDataForHud(&data));
    CHECK(node.m_data == &data);
    CHECK(node.setDataForHud(nullptr));
    CHECK(node.m_data == nullptr);
    node.m_next = nullptr;
    node.m_child1 = nullptr;
    ::operator delete(node.m_name);
    node.m_name = nullptr;
}

struct Harness
{
    Harness() : system(registry)
    {
        void* allocation = ::operator new(sizeof(Bf2TransformNode));
        root = new(allocation) Bf2TransformNode("IngameHud", 0, 0, 800, 600);
        CHECK(registry.insert(nullptr, root, nullptr));
        builder.setSystem(&system);
    }
    Object* find(const char* name) { return registry.findObject(&system, name); }
    NameTreeRoot registry;
    PlainSystem system;
    Bf2TransformNode* root;
    Bf2HudBuilder builder;
};

// the builder rows
static void createCases()
{
    // The stock (remmed) line:
    //   createSliderNode IngameHud RedSlider 0 1 0.7 0.01
    Harness harness;
    CHECK(harness.builder.createSliderNode("IngameHud", "RedSlider",
                                           0.0f, 1.0f, 0.7f, 0.01f));
    Object* found = harness.find("RedSliderSliderNode");
    CHECK(found && found->getType() == Bf2FixedSliderNode::getClassType());
    Bf2FixedSliderNode* slider = static_cast<Bf2FixedSliderNode*>(found);
    Bf2TransformNode* transform = dice::hfe::hud::builderCast<Bf2TransformNode>(
        harness.find("RedSlider"));
    CHECK(transform && transform->getChild(1) == slider);
    // The four floats are the slider's range and go only to the slider; the
    // transform is created at the origin with no size at all, which is why the
    // stock script follows the row with setNodePos and setNodeSize.
    CHECK(transform && transform->getPosition().x == 0.0f);
    CHECK(transform && transform->getSize().x == 0.0f);
    CHECK(slider && slider->m_minimum == 0.0f && slider->m_maximum == 1.0f);
    CHECK(slider && slider->m_thumbSpan == 0.7f && slider->m_step == 0.01f);
    CHECK(harness.builder.m_field014 == slider);

    // The duplicate search has the family's usual child-0-chain hole: the
    // predecessor scan only sees a name that is already linked under the parent.
    CHECK(harness.builder.createSliderNode("IngameHud", "RedSlider", 0.0f, 1.0f, 1.0f, 1.0f));
    harness.root->setChild(0, transform);
    CHECK(!harness.builder.createSliderNode("IngameHud", "RedSlider", 0.0f, 1.0f, 1.0f, 1.0f));
    harness.root->setChild(0, nullptr);
}

static void dataCases()
{
    Harness harness;
    CHECK(harness.builder.createSliderNode("IngameHud", "RedSlider",
                                           0.0f, 1.0f, 0.7f, 0.01f));
    Bf2FixedSliderNode* slider = dice::hfe::hud::builderCast<Bf2FixedSliderNode>(
        harness.find("RedSliderSliderNode"));
    CHECK(slider != nullptr);

    // Row 57 binds through the information layer's getFloatByName (+0x30).
    // The stock (remmed) line names MenuBackgroundRed, which the layer
    // already registers against its own state field - so the slider ends up
    // driving the HUD's menu-background red channel directly.
    float* const layerOwned = information->getFloatByName(std::string("MenuBackgroundRed"));
    CHECK(layerOwned != nullptr);
    CHECK(harness.builder.setSliderNodeData("MenuBackgroundRed"));
    CHECK(slider && slider->m_data != nullptr);
    FloatRefData* data = static_cast<FloatRefData*>(slider->m_data);
    CHECK(data && data->m_value == layerOwned);
    // Re-registering a name the layer already owns does NOT rebind it: the
    // duplicate reports, takes a fresh id and keeps the new pointer under it,
    // but std::map::insert leaves the original name-to-id entry alone.
    static float shadow = 0.5f;
    CHECK(information->registerFloat(std::string("MenuBackgroundRed"), &shadow) >= 0);
    CHECK(information->getFloatByName(std::string("MenuBackgroundRed")) == layerOwned);
    // The wrapper carries the shipped literal name.
    CHECK(data && data->Data::m_name && std::strcmp(data->Data::m_name, "sliderData") == 0);
    CHECK(data && data->getType() == FloatRefData::getClassType());

    // An unregistered variable reports and changes nothing.
    Data* const previous = slider->m_data;
    CHECK(!harness.builder.setSliderNodeData("NeverRegistered"));
    CHECK(slider && slider->m_data == previous);
    // No m_system test in this row at all: clearing it changes nothing.
    harness.builder.m_system = nullptr;
    CHECK(harness.builder.setSliderNodeData("MenuBackgroundRed"));
    harness.builder.m_system = &harness.system;
}

static void childCases()
{
    // This row takes the node that follows the named one in the sibling
    // chain, not the named node's own content: it reads child 0, which
    // insertNode uses as the next-sibling link, where child 1 holds what the
    // create rows actually build. Replaying the stock (remmed) lines below shows
    // exactly what that does.
    Harness harness;
    CHECK(harness.builder.createPictureNode("IngameHud", "RedSliderPicture", 0, 0, 5, 20));
    CHECK(harness.builder.createSliderNode("IngameHud", "RedSlider",
                                           0.0f, 1.0f, 0.7f, 0.01f));
    Bf2TransformNode* picture = dice::hfe::hud::builderCast<Bf2TransformNode>(
        harness.find("RedSliderPicture"));
    Bf2TransformNode* transform = dice::hfe::hud::builderCast<Bf2TransformNode>(
        harness.find("RedSlider"));
    Bf2FixedSliderNode* slider = dice::hfe::hud::builderCast<Bf2FixedSliderNode>(
        harness.find("RedSliderSliderNode"));
    CHECK(picture && transform && slider);
    // insertNode chains through child 0, in script order.
    CHECK(picture && picture->getChild(0) == transform);

    CHECK(harness.builder.setSliderNodeChild("RedSliderPicture"));
    // What it actually moved: the slider's OWN parent transform, spliced out
    // of the chain and installed as the slider's child 1 - so transform->child1
    // is the slider and slider->child1 is the transform, a two-node cycle.
    CHECK(slider && slider->getChild(1) == transform);
    CHECK(transform && transform->getChild(1) == slider);
    CHECK(transform && transform->getChild(0) == nullptr);
    CHECK(picture && picture->getChild(0) == nullptr);   // picture took the moved node's link

    // Refusals, in the shipped order.
    CHECK(!harness.builder.setSliderNodeChild("NoSuchNode"));
    // The named node now has no child 0 at all.
    CHECK(!harness.builder.setSliderNodeChild("RedSliderPicture"));
    // With nothing selected the row still refuses, but only after the lookup
    // and the Node cast have both succeeded.
    Harness second;
    CHECK(second.builder.createPictureNode("IngameHud", "A", 0, 0, 1, 1));
    CHECK(second.builder.createPictureNode("IngameHud", "B", 0, 0, 1, 1));
    second.builder.m_field014 = nullptr;
    CHECK(!second.builder.setSliderNodeChild("A"));
}

static void printDiagnostic(DebugType, const std::string&, const std::string&, int line,
    const std::string&, const std::string& message, const std::string&, void*)
{
    std::fprintf(stderr, "FIXTURE_DEBUG line=%d %s\n", line, message.c_str());
    std::fflush(stderr);
}

static RecordingEvents fixtureEvents;

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    eventManager = &fixtureEvents;
    void* memory = ::operator new(sizeof(HudInformationLayer));
    information = new(memory) HudInformationLayer;
    g_hudInformationLayer = information;
    if (argc > 1 && !std::strcmp(argv[1], "create-death")) {
        setDebugCallback(DebugTypeError, printDiagnostic, nullptr);
        Harness harness;
        harness.builder.createSliderNode("IngameHud", "Warn", 0.0f, 1.0f, 1.0f, 1.0f);
        Bf2TransformNode* transform = dice::hfe::hud::builderCast<Bf2TransformNode>(
            harness.find("Warn"));
        harness.root->setChild(0, transform);
        // The name argument is "Warn"; the message must print the selected
        // node name instead, which is this row's shipped copy/paste slip.
        harness.builder.m_field018 = "TheSelectedOne";
        harness.builder.createSliderNode("IngameHud", "Warn", 0.0f, 1.0f, 1.0f, 1.0f);
        std::fprintf(stderr, "FIXTURE_CREATE_RETURNED\n");
        return 96;
    }
    if (argc > 1 && !std::strcmp(argv[1], "data-death")) {
        setDebugCallback(DebugTypeError, printDiagnostic, nullptr);
        Harness harness;
        harness.builder.m_field018 = "Absent";
        harness.builder.setSliderNodeData("x");
        std::fprintf(stderr, "FIXTURE_DATA_RETURNED\n");
        return 97;
    }
    if (argc > 1 && !std::strcmp(argv[1], "child-death")) {
        setDebugCallback(DebugTypeError, printDiagnostic, nullptr);
        Harness harness;
        harness.builder.m_field018 = "Absent";
        harness.builder.setSliderNodeChild("NoSuchNode");
        std::fprintf(stderr, "FIXTURE_CHILD_RETURNED\n");
        return 98;
    }
    turnOffAllDebug(DebugTypeError);
    sliderLayoutCases();
    sliderSlotCases();
    createCases();
    dataCases();
    childCases();
    std::printf("HUD slider node %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
