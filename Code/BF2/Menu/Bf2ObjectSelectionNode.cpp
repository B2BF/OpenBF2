// HUD object-selection behavior. [INFERRED] Source placement.
#include "Bf2ObjectSelectionNode.h"
#if !DEDICATED
#include "BF2HudManager.h"                    // getMode +0x1E0, getAmmo +0x248, screenToWorld +0x2F8
#include "HudAmmo.h"                          // +0xA0, the crosshair icon size (friend)
#include "../Core/Ids.h"                      // IID_IWeaponObject (ds:91C3DC = 38080)
#include "../Game/Main/BF2ClientSeams.h"      // hudManager (ds:A10890)
#include "../Game/Objects/ICameraObject.h"    // getTransformation +0x10
#include "../Game/Objects/IObject.h"          // findOwningPlayerControlObject (440060)
#include "../Game/Objects/IPlayer.h"          // getVehicle PE +0x44, getCamera PE +0x17C
#include "../Game/Objects/IWeaponObject.h"    // setTargetDirection +0x12C
#include "../Math/Mat4.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h" // getActiveWeapon PE +0x74
#include "../Scene/Object/PlayerManager.h"    // world::playerManager (ds:A08F60)
#include "../External/BFMeme/Style.h"         // Rect
#include "../External/BFMeme/System.h"        // getPointerPosition (682BA0)
#include "../External/BFMeme/TypeDescriptor.h"
#include <new>
namespace dice { namespace meme {
namespace {
int bf2ObjectSelectionNodeInstances;   // counter [INFERRED] beside the getter
class Bf2ObjectSelectionNodeTypeDescriptor final : public TypeDescriptor
{
public:
    Bf2ObjectSelectionNodeTypeDescriptor()
        : TypeDescriptor("dice::meme::Bf2ObjectSelectionNode") {}
    TypeDescriptor* getBaseType() override { return Bf2TransformNode::getClassType(); }
    RuntimeObject* create() override // operator new(0x48), then a null name
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        void* allocation = ::operator new(sizeof(Bf2ObjectSelectionNode));
        return allocation ? new(allocation) Bf2ObjectSelectionNode(nullptr) : nullptr;
    }
    void addInstance() override { ++bf2ObjectSelectionNodeInstances; }
    void removeInstance() override { --bf2ObjectSelectionNodeInstances; } // 7CBD70, no zero guard
    int getInstanceCount() override { return bf2ObjectSelectionNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(Bf2ObjectSelectionNode); }
};
}

// The base is built with five zeros - including a null name - and the
// real name then goes through Bf2TransformNode::setName, which frees the clone
// the base constructor just made. The pointer size starts at 32x32.
Bf2ObjectSelectionNode::Bf2ObjectSelectionNode(const char* name)
    : Bf2TransformNode(nullptr, 0, 0, 0, 0),
      m_pointerWidth(32.0f), m_pointerHeight(32.0f), m_field044(0)
{
    setName(name);
}
// 7CBE00 -> 7C1BD0: unlike BfOccupiedVehicleNode, this destructor does run the
// base body, so the owned name clone is released.
Bf2ObjectSelectionNode::~Bf2ObjectSelectionNode() {}

unsigned int Bf2ObjectSelectionNode::getObjectSize()
{ return sizeof(Bf2ObjectSelectionNode); }

bool Bf2ObjectSelectionNode::setPointerSizeForHud(float width, float height) // 7CBDA0, +D8
{
    m_pointerWidth = width;
    m_pointerHeight = height;
    return true;
}

// Updates the target-selection box for a TV-guided weapon.
bool Bf2ObjectSelectionNode::updateSelf(System* system, const NodeState*, const Rect* bounds, float)
{
    if (hfe::hudManager->getMode() != 0 && hfe::hudManager->getMode() != 10)   // +0x1E0
        return true;

    const float* const pointer = system->getPointerPosition();
    float x = pointer[0];
    float y = pointer[1];
    // `mov eax,[eax+0A0h] / cdq / sub eax,edx / sar eax,1 / fild`
    const float half = static_cast<float>(hfe::hudManager->getAmmo()->m_field0a0 / 2);   // +0x248

    // The edge sums and the half-offsets stay on the x87 stack; only the clamped result is
    // stored (`fstp dword`), so the doubles below carry the unrounded values.
    const double right = static_cast<double>(bounds->width) + bounds->x;   // width + x
    if (half + static_cast<double>(x) > right)                          // `test ah,41h / jnz`: >
        x = static_cast<float>(right - half);
    else if (static_cast<double>(x) - half < bounds->x)                 // `test ah,5 / jp`: <
        x = static_cast<float>(half + static_cast<double>(bounds->x));

    const double bottom = static_cast<double>(bounds->height) + bounds->y;
    if (half + static_cast<double>(y) > bottom)
        y = static_cast<float>(bottom - half);
    else if (static_cast<double>(y) - half < bounds->y)
        y = static_cast<float>(half + static_cast<double>(bounds->y));

    const float* const current = system->getPointerPosition();
    if (current[0] != x || current[1] != y) {
        float* const target = system->getPointerPosition();
        target[0] = x;
        target[1] = y;
    }
    return true;
}

// Designates the world point under the pointer on a press inside the selection box.
// Requires an active weapon and camera. Always returns false, so the click remains unconsumed.
bool Bf2ObjectSelectionNode::onEvent1(System* system, const Rect*, const NodeState*, int first, int, int)
{
    if (hfe::hudManager->getMode() != 0)                                // +0x1E0
        return false;
    if (first != 1)
        return false;
    (void)getSize();                                                    // 659990, discarded
    (void)getPosition();                                                // 65DDD0, discarded
    const float* const pointer = system->getPointerPosition();
    const float x = pointer[0];
    const float y = pointer[1];

    hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer(); // +0x30
    if (!player)
        return false;
    hfe::world::IObject* const vehicle = player->getVehicle();          // PE +0x44
    if (!vehicle)
        return false;
    hfe::world::IPlayerControlObject* const control =
        hfe::world::findOwningPlayerControlObject(vehicle);
    if (!control)
        return false;
    hfe::world::IWeaponObject* const weapon = static_cast<hfe::world::IWeaponObject*>(
        control->getActiveWeapon(0)->queryInterface(hfe::world::IID_IWeaponObject));   // +0x74, +0x0C
    if (!weapon)
        return false;
    hfe::world::ICameraObject* const camera = player->getCamera();      // PE +0x17C

    const hfe::Vec2 screen = {x, y};
    const hfe::Vec3 target = hfe::hudManager->screenToWorld(screen);    // +0x2F8
    const hfe::Mat4& view = camera->getTransformation(0.0f);            // +0x10
    const hfe::Vec3 eye = {view.m[12], view.m[13], view.m[14]};
    weapon->setTargetDirection(eye, target);                            // +0x12C
    return false;
}

TypeDescriptor* Bf2ObjectSelectionNode::getClassType()
{
    static Bf2ObjectSelectionNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* Bf2ObjectSelectionNode::getType() { return getClassType(); }
namespace {
TypeDescriptor* const registeredBf2ObjectSelectionNodeType =
    Bf2ObjectSelectionNode::getClassType();
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2ObjectSelectionNodeTypeDescriptor) == 16, "client BF2 selection metadata");
static_assert(sizeof(Bf2ObjectSelectionNode) == 72, "native Bf2ObjectSelectionNode storage");
#endif
} }
#endif
