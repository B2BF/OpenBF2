// HUD hover-node layout and updates. [INFERRED] Source placement.
#include "Bf2HoverNode.h"
#if !DEDICATED
#include "../Core/StringUtil.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ObjectLayoutAccess.h" // LayoutAccess::physicsNode - IObject +0x44
#include "../Physics/IPhysicsNode.h"            // getPositionalSpeed (PE +0x5C = ELF +0x60)
#include "../Scene/Object/PlayerManager.h"      // world::playerManager (ds:A08F60)
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
int bf2HoverNodeInstances;            // counter [INFERRED] beside the getter
class Bf2HoverNodeTypeDescriptor final : public TypeDescriptor
{
public:
    Bf2HoverNodeTypeDescriptor() : TypeDescriptor("dice::meme::Bf2HoverNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override // operator new(0x54), then two nulls
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        void* allocation = ::operator new(sizeof(Bf2HoverNode));
        return allocation ? new(allocation) Bf2HoverNode(nullptr, nullptr) : nullptr;
    }
    void addInstance() override { ++bf2HoverNodeInstances; }
    void removeInstance() override { --bf2HoverNodeInstances; } // 7CB200, no zero guard
    int getInstanceCount() override { return bf2HoverNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(Bf2HoverNode); }
};

char* cloneHoverName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes); // no allocation-null guard in the original
    return copy;
}
void freeHoverName(char*& name)
{
    ::operator delete(name);
    name = nullptr;
}
}

// The node publishes three information-layer values named after itself,
// and the constructor is where it registers them: two ints and a bool.
// The System argument is accepted and never stored - the registrations go
// through the g_hudInformationLayer global instead.
// +24 is never written by the shipped constructor.
Bf2HoverNode::Bf2HoverNode(System* system, const char* name)
    : m_name(nullptr), m_shownTime(0.0f), m_maxValue(0.5f), m_length(0.0f), m_width(0.0f),
      m_anchorX(0.0f), m_anchorZ(0.0f), m_xPos(0), m_yPos(0), m_middleX(0), m_middleY(0),
      m_boundsHeightInt(0), m_boundsWidthInt(0), m_show(false),
      m_boundsWidth(0.0f), m_boundsHeight(0.0f), m_boundsX(0.0f), m_boundsY(0.0f)
{
    (void)system;
    freeHoverName(m_name);              // clears the initially null clone, as shipped
    m_name = cloneHoverName(name);
    hfe::HudInformationLayer* const layer =
        static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    layer->registerInt(hfe::formatString("%sXPos", m_name), &m_xPos);
    layer->registerInt(hfe::formatString("%sYPos", m_name), &m_yPos);
    layer->registerBool(hfe::formatString("%sShow", m_name), &m_show);
}

// unlike BfOccupiedVehicleNode this destructor has its own body, and it
// does release the clone before running the base teardown.
Bf2HoverNode::~Bf2HoverNode() { freeHoverName(m_name); }

unsigned int Bf2HoverNode::getObjectSize() { return sizeof(Bf2HoverNode); }
const char* Bf2HoverNode::getName() { return m_name; }
bool Bf2HoverNode::setName(const char* name)
{
    freeHoverName(m_name);
    m_name = cloneHoverName(name);   // free-before-copy aliasing defect retained
    return true;
}
bool Bf2HoverNode::slot_0x24() { return true; }

bool Bf2HoverNode::setMaxValueForHud(float value)          // 7CE240, +C8
{ m_maxValue = value; return true; }

bool Bf2HoverNode::setWidthLengthForHud(float width, float length) // 7CB180, +CC
{
    // crossed, as shipped: the first argument lands at +18 and the second
    // at +14 - the reverse of the member order.
    m_width = width;
    m_length = length;
    return true;
}

bool Bf2HoverNode::setMiddlePositionForHud(int x, int y)
{ m_middleX = x; m_middleY = y; return true; }

// Sets HUD bounds and retains the integer width and height used by scaling.
bool Bf2HoverNode::setBoundsForHud(int x, int y, int width, int height) // 7CB1C0, +D4
{
    m_boundsX = static_cast<float>(x);        // fild, +4C
    m_boundsY = static_cast<float>(y);        // +50
    m_boundsWidth = static_cast<float>(width);   // +44
    m_boundsHeight = static_cast<float>(height); // +48
    m_boundsWidthInt = width;                 // +3C, raw
    m_boundsHeightInt = height;               // +38, raw - below the width
    return true;
}

// Updates the hover indicator while the vehicle moves slowly on both horizontal axes.
// The indicator stays visible until it leaves its bounds; losing the vehicle leaves its last values intact.
// The Z calculation adds a nonnegative anchor rather than subtracting it. Preserve this behavior.
bool Bf2HoverNode::updateSelf(System*, const NodeState*, const Rect*, float delta)
{
    hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer(); // +0x30
    if (!player)
        return true;
    hfe::world::IObject* const vehicle = player->getVehicle();          // PE +0x44
    if (!vehicle)
        return true;
    void* const node = hfe::world::object_detail::LayoutAccess::physicsNode(vehicle);   // +0x44
    if (!node)
        return true;
    // Both components are copied out at once: x with `fst`, z as a raw dword.
    const hfe::Vec3& speed =
        static_cast<hfe::world::IPhysicsNode*>(node)->getPositionalSpeed();  // PE +0x5C
    const float speedX = speed.x;
    const float speedZ = speed.z;

    // 7CB3B6..7CB3EB. `test ah,41h / jp` is <=, `test ah,1 / jnz` is >=; NaN fails every test.
    // The order is +x, +z, then -max negated once, then -x, -z.
    if (speedX <= m_maxValue && speedZ <= m_maxValue) {
        const float negativeMax = -m_maxValue;
        if (speedX >= negativeMax && speedZ >= negativeMax && !m_show) {
            m_show = true;
            m_anchorX = vehicle->getAbsolutePosition().x;               // PE +0x68, [eax]
            m_anchorZ = vehicle->getAbsolutePosition().z;               // asked again, [eax+8]
        }
    }

    if (m_show) {
        m_shownTime = delta + m_shownTime;
        const float yScale = static_cast<float>(m_boundsHeightInt) / m_length;   // fstp: a float
        const float positionZ = vehicle->getAbsolutePosition().z;
        const double deltaZ = m_anchorZ < 0.0f                          // see the note above
            ? static_cast<double>(positionZ) - m_anchorZ
            : static_cast<double>(positionZ) + m_anchorZ;
        m_yPos = static_cast<int>(static_cast<double>(yScale) * deltaZ) + m_middleY;  // +2C
        const float xScale = static_cast<float>(m_boundsWidthInt) / m_width;
        const float positionX = vehicle->getAbsolutePosition().x;
        m_xPos = m_middleX
               - static_cast<int>((static_cast<double>(positionX) - m_anchorX) * xScale);  // +28
    }

    // Runs whether or not the indicator is shown.
    const int left = static_cast<int>(m_boundsX);                       // __ftol2
    bool outside = m_xPos < left || m_xPos > static_cast<int>(m_boundsWidth) + left;
    if (!outside) {
        const int top = static_cast<int>(m_boundsY);                    // __ftol2
        outside = m_yPos < top || m_yPos > static_cast<int>(m_boundsHeight) + top;
    }
    if (outside) {
        m_show = false;
        m_shownTime = 0.0f;
    }
    return true;
}

TypeDescriptor* Bf2HoverNode::getClassType()
{
    static Bf2HoverNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* Bf2HoverNode::getType() { return getClassType(); }
namespace {
TypeDescriptor* const registeredBf2HoverNodeType = Bf2HoverNode::getClassType();
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2HoverNodeTypeDescriptor) == 16, "client BF2 hover metadata");
static_assert(sizeof(Bf2HoverNode) == 84, "native Bf2HoverNode storage");
#endif
} }
#endif
