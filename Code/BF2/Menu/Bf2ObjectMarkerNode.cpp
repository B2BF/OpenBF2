// World-object HUD marker selection and rendering. [INFERRED] Source placement.
#include "Bf2ObjectMarkerNode.h"
#if !DEDICATED
#include "Bf2TransformNode.h"
#include "BF2HudManager.h"                    // worldToScreen +0x2FC
#include "Ref2System.h"                       // getRenderView (7AD390)
#include "../Core/Ids.h"                      // IID_IWeaponObject (ds:91C3DC = 38080)
#include "../Core/StringUtil.h"
#include "../External/BFMeme/Effect.h"        // drawContextTexture (82D790)
#include "../External/BFMeme/Quad.h"          // Quad (7732D0), rotateQuad (800BE0)
#include "../External/BFMeme/TypeDescriptor.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/IGameLogic.h"               // gameLogic +0x1B0 getTargetObjectMap
#include "../Game/Main/BF2ClientSeams.h"      // hudManager (ds:A10890)
#include "../Game/Objects/Game/TargetObject.h" // getTargetActive (5E9630), getTargetType (5E9650)
#include "../Game/Objects/IObject.h"          // getRootParent (71AE20), findOwningPlayerControlObject (440060)
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/IWeaponHudComp.h"   // setLockState +0x40
#include "../Game/Objects/IWeaponObject.h"    // +0x120 / +0x124 / +0x134
#include "../Game/Objects/ObjectLayoutAccess.h" // objectId (+0x10), weaponHudComponent (+0x5C)
#include "../Physics/HeightmapCluster.h"      // IHeightmapCluster - intersectRayInWorldCoords
#include "../RendMisc/IRenderView.h"          // getOrthoWidth +0x54 / getOrthoHeight +0x5C
#include "../Scene/Object/ObjectManager.h"    // world::objectManager (ds:A08F58) - intersectLine
#include "../Scene/Object/ObjectPredicator.h" // ProjectileVsTargetPredicator (vtable 8AB3DC)
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h" // getPcos +0x60, getActiveWeapon +0x74
#include "../Scene/Object/PlayerManager.h"    // world::playerManager (ds:A08F60)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>
namespace dice { namespace hfe { namespace world {
// Engine-owned heightmap cluster for water height and ray queries.
extern IBase* heightmapCluster;       // Core/Globals.cpp, ELF ds:0x8ABAF24
} } }
namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor class name. Counter A23388 has only the add/sub/get
// references below; construction and destruction do not adjust it.
int bf2ObjectMarkerNodeInstances;
class Bf2ObjectMarkerNodeTypeDescriptor final : public TypeDescriptor
{
public:
    Bf2ObjectMarkerNodeTypeDescriptor()
        : TypeDescriptor("dice::meme::Bf2ObjectMarkerNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override // operator new(0x1CC), then (null, null, 5)
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Allocation and construction are real; publication retains Node33.
        void* allocation = ::operator new(sizeof(Bf2ObjectMarkerNode));
        return allocation ? new(allocation) Bf2ObjectMarkerNode(nullptr, nullptr, 5) : nullptr;
    }
    void addInstance() override { ++bf2ObjectMarkerNodeInstances; }
    void removeInstance() override { --bf2ObjectMarkerNodeInstances; } // 7CC1D0, no zero guard
    int getInstanceCount() override { return bf2ObjectMarkerNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(Bf2ObjectMarkerNode); }
};

char* cloneMarkerName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes); // no allocation-null guard in the original
    return copy;
}
// 737EE0 takes the address of the pointer: it frees the clone and nulls it.
void freeMarkerName(char*& name)
{
    ::operator delete(name);
    name = nullptr;
}
// Eight zeroed bytes in .data whose address every name slot starts at.
char emptyMarkerNameStorage[8];

typedef std::map<unsigned int, hfe::world::IObject*> TargetObjectMap;

// 464660, the shared HUD distance helper (a `__fastcall` pair: from in ECX, to in EDX). The X
// difference is rounded to float; Y and Z stay on the x87 stack; summed Z, X, Y. The same body
// HudInformationLayer.cpp (hudControlPointDistance) and Bf2MenuMapNode.cpp (controlPointDistance)
// carry as TU-local copies - this is a third, for the same reason.
double markerDistance(const hfe::Vec3& from, const hfe::Vec3& to)
{
    const float x = static_cast<float>(static_cast<double>(to.x) - from.x);
    const double y = static_cast<double>(to.y) - from.y;
    const double z = static_cast<double>(to.z) - from.z;
    return std::sqrt(z * z + static_cast<double>(x) * x + y * y);
}

// 7CCBC0's distance, inlined there three times: vehicle minus object, the X difference rounded
// to float (`fstp [ebp-18h]`), Y and Z kept on the x87 stack, summed Z, Y, X.
double indexDistance(const hfe::Vec3& vehicle, const hfe::Vec3& object)
{
    const float x = static_cast<float>(static_cast<double>(vehicle.x) - object.x);
    const double y = static_cast<double>(vehicle.y) - object.y;
    const double z = static_cast<double>(vehicle.z) - object.z;
    return std::sqrt(z * z + y * y + static_cast<double>(x) * x);
}
}
const char* const defaultMarkerName = emptyMarkerNameStorage;

// Initializes marker drawing state and registers with system during construction.
// The range-line texture starts at zero; other fields follow MarkerQuad initialization.
Bf2ObjectMarkerNode::Bf2ObjectMarkerNode(System* system, const char* name, int objects)
    : m_objects(objects), m_range(0.0f), m_lockTextOffset(),
      m_field048(0), m_field04c(-1), m_field050(-1), m_lockOnType(2), m_weapon(0),
      m_field05c(true), m_lockedShow(false), m_lockTextEnabled(false),
      m_field05f(false), m_field060(true), m_field064(0), m_name(nullptr)
{
    (void)system; // the constructor stores it nowhere; only the caller keeps it
    m_textures[0] = m_textures[1] = m_textures[2] = m_textures[3] = 0;
    for (int i = 0; i < 4; ++i) { m_textureSizes[i].x = 32.0f; m_textureSizes[i].y = 32.0f; }
    m_lockTextOffset.x = -3.0f;
    m_lockTextOffset.y = 0.0f;
    m_rangeLine.texture = 0;                // PE+9C
    // +98 (the range line's rotation) and +A0 (its +28 word) are never written by the
    // shipped constructor; they are left exactly as the allocation delivered them.
    freeMarkerName(m_name);                 // clears the initially null clone
    m_name = cloneMarkerName(name);
    static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer)
        ->registerBool(hfe::formatString("%sLockedShow", m_name), &m_lockedShow);
    for (int i = 0; i < MarkerSlotCount; ++i) {
        m_show[i] = false;
        m_names[i] = defaultMarkerName;
    }
    // m_positions is not initialised by the shipped constructor either.
}

Bf2ObjectMarkerNode::~Bf2ObjectMarkerNode()
{
    freeMarkerName(m_name);
    // Every slot the information layer replaced is owned; the sentinel is not.
    for (int i = 0; i < MarkerSlotCount; ++i)
        if (m_names[i] != defaultMarkerName)
            ::operator delete(const_cast<char*>(m_names[i]));
    // The four containers release their own nodes; retail clears them in the
    // order +1C0, +1B4, +A4, +68, which C++ reverses. Nothing observes the order.
}

unsigned int Bf2ObjectMarkerNode::getObjectSize() { return sizeof(Bf2ObjectMarkerNode); }
const char* Bf2ObjectMarkerNode::getName() { return m_name; }
bool Bf2ObjectMarkerNode::setName(const char* name)
{
    freeMarkerName(m_name);
    m_name = cloneMarkerName(name); // free-before-copy aliasing defect retained
    return true;
}
bool Bf2ObjectMarkerNode::slot_0x24() { return true; }

// the added slots
bool Bf2ObjectMarkerNode::clearShowFlagsForHud()               // 7CBFE0, +C0
{
    m_field04c = -1;
    m_field050 = -1;
    m_field064 = 0;
    m_lockedShow = false;
    for (int i = 0; i < MarkerSlotCount; ++i) m_show[i] = false;
    return true;                       // the three arguments are ignored
}
bool Bf2ObjectMarkerNode::setObjectCountForHud(int objects)    // 7CDFC0, +C8
{ m_objects = objects; return true; }
bool Bf2ObjectMarkerNode::setLockTextOffsetForHud(int x, int y) // 7CD1F0, +CC
{
    m_lockTextOffset.x = static_cast<float>(x);
    m_lockTextOffset.y = static_cast<float>(y);
    return true;
}
bool Bf2ObjectMarkerNode::setLockTextForHud(bool enabled, std::string prefix)
{
    m_lockTextEnabled = enabled;
    if (enabled) {
        // Four information-layer registrations per slot, in the shipped order.
        for (int i = 0; i < MarkerSlotCount; ++i) {
            hfe::HudInformationLayer* const layer =
                static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
            layer->registerString(hfe::formatString("%s%iString", prefix.c_str(), i), &m_names[i]);
            layer->registerBool(hfe::formatString("%s%iShow", prefix.c_str(), i), &m_show[i]);
            layer->registerInt(hfe::formatString("%s%iPosX", prefix.c_str(), i), &m_positions[i][0]);
            layer->registerInt(hfe::formatString("%s%iPosY", prefix.c_str(), i), &m_positions[i][1]);
        }
    }
    return true;                       // a hard true, even when nothing ran
}
bool Bf2ObjectMarkerNode::addLockTextNodeForHud(Bf2TransformNode* node) // 7CD000, +D4
{
    if (!node) return false;
    m_lockTextNodes.push_back(node);   // retail inlines list::insert(end(), node)
    return true;
}
bool Bf2ObjectMarkerNode::setTextureForHud(int index, TextureHandle texture) // 7CD1C0, +D8
{
    // Out-of-range is silently ignored and still answers true; a negative
    // index is not tested at all, so it writes before the array.
    if (index < 4) m_textures[index] = texture;
    return true;
}
bool Bf2ObjectMarkerNode::setTextureSizeForHud(int index, float width, float height) // 7CD210, +DC
{
    if (index < 4) { m_textureSizes[index].x = width; m_textureSizes[index].y = height; }
    return true;
}
bool Bf2ObjectMarkerNode::setLockOnTypeForHud(int type)        // 7CDDD0, +E0
{ m_lockOnType = type; return true; }
bool Bf2ObjectMarkerNode::setWeaponForHud(int weapon)          // 7CD1E0, +E4
{ m_weapon = weapon; return true; }

// Collects visible target markers, updates their labels and draws them.
// Marker quads use bounds for clipping; the separate clip argument is unused.
void Bf2ObjectMarkerNode::paintSelf(System* system, const NodeState*, const Rect* bounds,
                                    const Rect*, RenderContext* context)
{
    clearFrameMarkers();
    collectMarkers(m_objects, system, bounds);
    updateMarkerLabels(bounds);
    drawMarkers(system, bounds, context);
}

// Three inlined VC7.1 clear()s - the +1B4 list, the +A4 list, then the +68 map
// (`_Erase(_Root())` 46B7F0 and the head relinks).
void Bf2ObjectMarkerNode::clearFrameMarkers()
{
    m_markerQuads.clear();
    m_markerTargets.clear();
    m_weaponTargets.clear();
}

// `insert(pair(target +0x10, target))` through 4F0730, the unsigned-key
// insert_unique: a second target with the same id is refused, not replaced.
void Bf2ObjectMarkerNode::addWeaponTarget(hfe::world::IObject* target)
{
    m_weaponTargets.insert(std::make_pair(
        hfe::world::object_detail::LayoutAccess::objectId(target), target));
}

// Collects eligible weapon targets in nearest-first order, up to the configured limit.
// Current and locked targets bypass the ordinary visibility and eligibility filters.
void Bf2ObjectMarkerNode::collectMarkers(int, System* system, const Rect* bounds)
{
    typedef hfe::world::object_detail::LayoutAccess Layout;
    TargetObjectMap* const registry =
        static_cast<TargetObjectMap*>(hfe::gameLogic->getTargetObjectMap());   // +0x1B0
    hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer();
    m_field05f = false;
    m_lockedShow = false;
    m_field050 = -1;
    m_field04c = -1;
    m_range = 0.0f;
    if (!player)
        return;
    if (!player->getVehicle())                                  // PE +0x44
        return;
    hfe::world::IObject* const vehicle = player->getVehicle();  // asked again
    if (!vehicle)
        return;

    hfe::world::IObject* const weaponObject =
        hfe::world::findOwningPlayerControlObject(vehicle)->getActiveWeapon(m_weapon); // +0x74
    if (weaponObject) {
        hfe::world::IWeaponObject* const weapon = static_cast<hfe::world::IWeaponObject*>(
            weaponObject->queryInterface(hfe::world::IID_IWeaponObject));
        if (weapon) {
            m_range = weapon->getMaxTargetDistance();           // +0x134
            hfe::world::IObject* const target = weapon->getTargetObject();   // +0x120
            if (target) {
                if (m_lockOnType == -1)
                    addWeaponTarget(target);
                if (weapon->hasTargetLock()) {                  // +0x124
                    m_lockedShow = true;
                    m_field05f = true;
                    m_field050 = static_cast<int>(Layout::objectId(target));
                } else {
                    m_field05f = true;
                    m_field04c = static_cast<int>(Layout::objectId(target));
                }
            }
        }
    }
    if (m_range <= 0.0f)                                        // `test ah,41h / jp`: NaN skips
        m_range = player->getViewDistance();                    // PE +0x84
    const hfe::Vec3 vehiclePosition = vehicle->getAbsolutePosition();     // PE +0x68

    TargetObjectMap& targets = m_lockOnType > -1 ? *registry : m_weaponTargets;
    for (TargetObjectMap::iterator it = targets.begin(); it != targets.end(); ++it) {
        hfe::world::IObject* const object = it->second;
        const unsigned int objectId = Layout::objectId(object); // +0x10, not the map key
        hfe::world::TargetObject* const targetObject = static_cast<hfe::world::TargetObject*>(object);
        if (!targetObject->getTargetActive())
            continue;

        bool accepted = false;
        if (vehicle != object) {
            const int lockOnType = m_lockOnType;
            if (targetObject->getTargetType() == lockOnType || lockOnType == -1) {
                if (isWithinRadius(vehiclePosition, object->getAbsolutePosition(), m_range)) {
                    hfe::world::IObject* const vehicleRoot =
                        hfe::world::getRootParent(player->getVehicle());           // asked first
                    if (hfe::world::getRootParent(object) != vehicleRoot
                        && !isObstructedByTerrain(vehicle, object)
                        && !isObstructedByObject(vehicle, object))
                        accepted = true;
                }
            }
        }
        if (!accepted
            && objectId != static_cast<unsigned int>(m_field050)
            && objectId != static_cast<unsigned int>(m_field04c))
            continue;

        const int index = markerIndexForDistance(object);
        if (static_cast<unsigned int>(index) > static_cast<unsigned int>(m_objects))
            continue;
        addMarker(objectId, object, index, system, bounds);
    }
}

// Returns whether terrain blocks the target.
bool Bf2ObjectMarkerNode::isObstructedByTerrain(hfe::world::IObject* vehicle,
                                                hfe::world::IObject* object)
{
    if (!object)
        return false;
    if (!vehicle)
        return false;
    hfe::Vec3 hit = {0.0f, 0.0f, 0.0f};
    hfe::Vec3 normal = {0.0f, 0.0f, 0.0f};
    float fraction = 0.0f;
    unsigned char material;
    const hfe::Vec3& to = object->getAbsolutePosition();
    const hfe::Vec3& from = vehicle->getAbsolutePosition();
    if (!static_cast<hfe::world::IHeightmapCluster*>(hfe::world::heightmapCluster)
             ->intersectRayInWorldCoords(from, to, hit, fraction, normal, material, true))
        return false;
    const hfe::Vec3& position = object->getAbsolutePosition();
    const double x = static_cast<double>(hit.x) - position.x;
    const double y = static_cast<double>(hit.y) - position.y;
    const double z = static_cast<double>(hit.z) - position.z;
    return !(std::sqrt(z * z + y * y + x * x) < 0.1);           // `test ah,5 / jp`: NaN obstructs
}

// Returns whether another object blocks the target.
bool Bf2ObjectMarkerNode::isObstructedByObject(hfe::world::IObject* vehicle,
                                               hfe::world::IObject* object)
{
    if (!object)
        return false;
    if (!vehicle)
        return false;
    hfe::Vec3 hitPoint = {0.0f, 0.0f, 0.0f};
    hfe::Vec3 hitNormal = {0.0f, 0.0f, 0.0f};
    int hitPart = 0;
    hfe::world::IObject* hitObject = 0;
    const hfe::Vec3 from = vehicle->getAbsolutePosition();
    const hfe::Vec3& to = object->getAbsolutePosition();
    const hfe::Vec3 direction = {to.x - from.x, to.y - from.y, to.z - from.z};
    hfe::world::IObject* const objectRoot = hfe::world::getRootParent(object);
    hfe::world::IObject* const vehicleRoot = hfe::world::getRootParent(vehicle);
    hfe::world::ProjectileVsTargetPredicator predicator(0x100u, 0u, vehicleRoot, objectRoot);
    return hfe::world::objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                                    from, direction, &predicator,
                                                    true, false, false, true, 0u);
}

// one body with HudInformationLayer::isWithinRadius: BF2.exe's HudInformationLayer
// table holds 7CC570 at +0x70, and this node calls the same address directly with ECX = itself.
// `this` is never read. Identical folded bodies; this is a second spelling of the tree's copy.
bool Bf2ObjectMarkerNode::isWithinRadius(hfe::Vec3 a, hfe::Vec3 b, float radius)
{
    const double x = static_cast<double>(b.x) - a.x;
    const double y = static_cast<double>(b.y) - a.y;
    const double z = static_cast<double>(b.z) - a.z;
    return std::sqrt(z * z + x * x + y * y) <= radius;
}

// Returns the insertion index for a marker ordered by distance from the local vehicle.
// Requires a local player and vehicle. The single-marker case handles equal distances differently from the general loop.
int Bf2ObjectMarkerNode::markerIndexForDistance(hfe::world::IObject* object)
{
    const hfe::Vec3 vehiclePosition =
        hfe::world::playerManager->getLocalHumanPlayer()->getVehicle()->getAbsolutePosition();
    const float distance =
        static_cast<float>(indexDistance(vehiclePosition, object->getAbsolutePosition()));
    int index = 0;
    if (m_markerTargets.size() == 1) {
        if (indexDistance(vehiclePosition,
                          m_markerTargets.front().object->getAbsolutePosition()) < distance)
            return 1;
        return 0;
    }
    for (std::list<MarkerTarget>::iterator it = m_markerTargets.begin();
         it != m_markerTargets.end(); ++it) {
        if (indexDistance(vehiclePosition, it->object->getAbsolutePosition()) > distance)
            break;
        ++index;
    }
    return index;
}

// Inserts a projected target at index in both marker lists.
// Requires a valid insertion index and texture index. Keep m_objects in the supported positive range
// so marker labels cannot exceed their twenty slots; a missing texture adds nothing.
void Bf2ObjectMarkerNode::addMarker(unsigned int objectId, hfe::world::IObject* object, int index,
                                    System* system, const Rect* bounds)
{
    (void)system;
    const hfe::Vec3 screen = hfe::hudManager->worldToScreen(object->getAbsolutePosition());
    if (!(screen.z > -1.0f))                                    // `test ah,41h / jnz`
        return;
    if (!isInsideBounds(screen, bounds))
        return;
    const int textureIndex = selectMarkerTexture(objectId, object, screen, bounds);
    MarkerQuad quad;
    if (textureIndex < 0)
        return;
    quad.width = m_textureSizes[textureIndex].x;
    quad.height = m_textureSizes[textureIndex].y;
    const TextureHandle texture = m_textures[textureIndex];
    quad.texture = texture;
    if (!texture)
        return;
    quad.field08[0] = 0.0f;
    quad.field08[1] = 1.0f;
    quad.field08[2] = 0.0f;
    quad.field08[3] = 1.0f;
    quad.x = screen.x;
    quad.y = screen.y;
    quad.rotation = 0.0f;

    std::list<MarkerQuad>::iterator quadWhere = m_markerQuads.begin();
    std::list<MarkerTarget>::iterator targetWhere = m_markerTargets.begin();
    for (int remaining = index; remaining > 0; --remaining) {   // `jle` skips; `sub/jnz` loops
        ++quadWhere;
        ++targetWhere;
    }
    const MarkerTarget target = {objectId, object};
    m_markerQuads.insert(quadWhere, quad);
    m_markerTargets.insert(targetWhere, target);

    const unsigned int cap = static_cast<unsigned int>(m_objects);
    if (m_markerQuads.size() > cap && cap > 0u) {
        // VC7.1 erase(--end()) refuses the head, so each pop is a no-op on an empty list.
        if (!m_markerQuads.empty())
            m_markerQuads.pop_back();
        if (!m_markerTargets.empty())
            m_markerTargets.pop_back();
    }
}

// strictly inside on all four sides; the z coordinate is carried and ignored.
bool Bf2ObjectMarkerNode::isInsideBounds(hfe::Vec3 screen, const Rect* bounds)
{
    return screen.x > bounds->x
        && static_cast<double>(bounds->width) + bounds->x > screen.x
        && screen.y > bounds->y
        && static_cast<double>(bounds->height) + bounds->y > screen.y;
}

// Selects the texture for the target's lock state.
int Bf2ObjectMarkerNode::selectMarkerTexture(unsigned int objectId, hfe::world::IObject* object,
                                             hfe::Vec3 screen, const Rect* bounds)
{
    if (object) {
        if (hfe::world::IPlayerControlObject* const control =
                hfe::world::findOwningPlayerControlObject(object)) {
            const std::map<int, hfe::world::IPlayerControlObject*> seats(*control->getPcos());
            for (std::map<int, hfe::world::IPlayerControlObject*>::const_iterator it = seats.begin();
                 it != seats.end(); ++it) {
                hfe::world::IPlayer* const occupant = it->second->getOccupingPlayer();
                if (!occupant)
                    continue;
                hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
                if (!local)
                    continue;
                const int localTeam = local->getTeam();         // PE +0xE4
                if (occupant->getTeam() == localTeam)
                    return 0;                                   // 7CD57E, the copy destroyed
            }
        }
    }
    if (objectId > 0u
        && (objectId == static_cast<unsigned int>(m_field04c)
            || objectId == static_cast<unsigned int>(m_field050)))
        updateRangeLine(screen, bounds);
    if (m_lockedShow && objectId == static_cast<unsigned int>(m_field050))
        return 2;
    return 1;
}

// Positions and rotates the range line from bounds center to screen, using the texture's width.
void Bf2ObjectMarkerNode::updateRangeLine(hfe::Vec3 screen, const Rect* bounds)
{
    const float centerY = static_cast<float>(bounds->height * 0.5 + bounds->y);
    const float centerX = static_cast<float>(bounds->width * 0.5 + bounds->x);
    const float dy = screen.y - centerY;
    const float dx = screen.x - centerX;
    double angle = -1.5707963705062866 - std::atan2(static_cast<double>(dy),
                                                    static_cast<double>(dx));   // fpatan, fsubr
    if (3.1415927410125732 + angle < 0.0)                       // `test ah,5 / jp`: NaN skips
        angle += 6.2831854820251465;
    m_rangeLine.height = static_cast<float>(
        std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy));   // PE+94
    m_rangeLine.width = m_textureSizes[3].x;                    // PE+90 <- PE+38
    m_rangeLine.x = static_cast<float>((static_cast<double>(screen.x) + centerX) * 0.5);  // PE+78
    m_rangeLine.y = static_cast<float>((static_cast<double>(screen.y) + centerY) * 0.5);  // PE+7C
    m_rangeLine.texture = m_textures[3];                        // PE+9C <- PE+1C
    m_rangeLine.rotation = static_cast<float>(angle);           // PE+98
}

// Updates marker distance labels and hides the ordinary label for a locked target.
// Requires at most twenty markers. Each update clones label text without releasing the previous clone.
void Bf2ObjectMarkerNode::updateMarkerLabels(const Rect*)
{
    hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer();
    hfe::Vec3 vehiclePosition = {0.0f, 0.0f, 0.0f};
    if (player && player->getVehicle())
        vehiclePosition = player->getVehicle()->getAbsolutePosition();
    int slot = 0;
    if (!m_markerTargets.empty()) {
        for (std::list<MarkerTarget>::iterator it = m_markerTargets.begin();
             it != m_markerTargets.end(); ++it, ++slot) {
            if (slot >= static_cast<int>(m_markerQuads.size()))
                continue;
            if (!m_field060 || !player) {
                m_show[slot] = false;
                continue;
            }
            m_show[slot] = !(it->objectId == static_cast<unsigned int>(m_field050) && m_lockedShow);
            if (!m_show[slot])
                continue;
            const double distance =
                markerDistance(vehiclePosition, it->object->getAbsolutePosition());
            m_names[slot] = cloneMarkerName(
                hfe::formatString("%i", static_cast<int>(std::ceil(distance))).c_str());
        }
        if (slot >= MarkerSlotCount)
            return;
    }
    std::memset(&m_show[slot], 0, MarkerSlotCount - slot);
}

// Draws marker quads, lock labels and the range line, then publishes the weapon's lock state.
void Bf2ObjectMarkerNode::drawMarkers(System* system, const Rect* bounds, RenderContext* context)
{
    std::list<Bf2TransformNode*>::iterator label = m_lockTextNodes.begin();
    const bool labelsFit = m_markerQuads.size() <= m_lockTextNodes.size();
    for (std::list<MarkerQuad>::iterator it = m_markerQuads.begin(); it != m_markerQuads.end(); ++it) {
        const float markerY = it->y;                            // `fst [ebp-10h]`
        const float markerX = it->x;                            // `fst [ebp-40h]`
        drawMarker(it->x, it->y, it->rotation, it->width, it->height, it->texture,
                   system, bounds, context);
        if (m_lockTextEnabled && labelsFit) {
            Bf2TransformNode* const node = *label;
            hfe::dx9rend::IRenderView* const view = static_cast<hfe::dx9rend::IRenderView*>(
                static_cast<hfe::menu::Ref2System*>(system)->getRenderView());
            const float labelY = static_cast<float>(
                static_cast<double>(view->getOrthoHeight()) * 0.5 + m_lockTextOffset.y + markerY);
            const double labelX = std::ceil(
                static_cast<double>(view->getOrthoWidth()) * 0.5 + m_lockTextOffset.x + markerX);
            node->setPositionForHud(static_cast<float>(labelX),
                                    static_cast<float>(std::ceil(static_cast<double>(labelY))));
            ++label;
        }
    }

    int lockState = 0;
    if ((m_lockedShow || m_field04c > 0) && m_field05f) {       // `jle`: signed
        lockState = m_lockedShow ? 2 : 1;
        if (m_rangeLine.texture) {
            drawMarker(m_rangeLine.x, m_rangeLine.y, m_rangeLine.rotation, m_rangeLine.width,
                       m_rangeLine.height, m_rangeLine.texture, system, bounds, context);
            m_rangeLine.texture = 0;                            // re-armed by 7CC3D0 next frame
        }
    }

    hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer();
    if (!player)
        return;
    if (hfe::world::IObject* const vehicle = player->getVehicle()) {
        if (hfe::world::IPlayerControlObject* const control =
                hfe::world::findOwningPlayerControlObject(vehicle)) {
            if (hfe::world::IObject* const weapon = control->getActiveWeapon(m_weapon)) { // +0x74
                if (hfe::world::IObjectComponent* const component =
                        hfe::world::object_detail::LayoutAccess::weaponHudComponent(weapon)) {
                    static_cast<hfe::world::IWeaponHudComp*>(component)
                        ->setLockState(static_cast<hfe::world::WarningState>(lockState));  // +0x40
                }
            }
        }
    }
    m_field064 = static_cast<std::uint32_t>(lockState);
}

// One quad of `texture`: a rect of ceiled size around (x, y) with each corner
// ceiled, the Quad clipped to `clip` (7732D0), turned by `rotation` about the rect's centre
// (800BE0 - always, even for 0) and drawn through paintSelf's `context` argument, passed
// through (82D790).
// `this` is never read. The clip is the caller's `bounds` - paintSelf's own clip is unused.
// A texture of 0 draws nothing.
void Bf2ObjectMarkerNode::drawMarker(float x, float y, float rotation, float width, float height,
                                     TextureHandle texture, System* system, const Rect* clip,
                                     RenderContext* context)
{
    if (!texture)
        return;
    const float ceiledWidth = static_cast<float>(std::ceil(static_cast<double>(width)));
    const float top = static_cast<float>(
        std::ceil(static_cast<double>(y) - static_cast<double>(height) * 0.5));
    const float left = static_cast<float>(
        std::ceil(static_cast<double>(x) - static_cast<double>(width) * 0.5));
    const float ceiledHeight = static_cast<float>(std::ceil(static_cast<double>(height)));
    const Rect placed = {left, top, ceiledWidth, ceiledHeight, 0.0f};
    Quad quad(&placed, clip);
    const float center[2] = {
        static_cast<float>(static_cast<double>(ceiledWidth) * 0.5 + left),
        static_cast<float>(static_cast<double>(ceiledHeight) * 0.5 + top)
    };
    rotateQuad(&quad, center, rotation);
    drawContextTexture(context, system, texture, &quad);
}

TypeDescriptor* Bf2ObjectMarkerNode::getClassType()
{
    static Bf2ObjectMarkerNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* Bf2ObjectMarkerNode::getType() { return getClassType(); }
namespace {
TypeDescriptor* const registeredBf2ObjectMarkerNodeType = Bf2ObjectMarkerNode::getClassType();
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2ObjectMarkerNodeTypeDescriptor) == 16, "client BF2 marker metadata");
static_assert(sizeof(Bf2ObjectMarkerNode) == 444, "native Bf2ObjectMarkerNode storage");
#endif
} }
#endif
