// HUD map rendering, interaction and event handling.
#include "Bf2MenuMapNode.h"
#include <list>
#if !DEDICATED
#include "../Game/IGameLogic.h"
#include "../Game/IGame.h"
#include "../Game/GameLogic/CombatArea.h"
#include "../Game/GameLogic/CombatAreaManager.h"
#include "../Core/SettingsRepostitory.h"
#include "BF2HudManager.h"
#include "Bf2ColorEffect.h"
#include "Bf2EffectNode.h"
#include "Bf2Style.h"
#include "Bf2TextNode.h"
#include "HudColors.h"
#include "HudCommander.h"
#include "HudSquad.h"
#include "HudKitSelection.h"   // pickSpawnGroup hands the id to it
#include "HudInformationAdapter.h"
#include "Ref2System.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Game/Objects/ISoldier.h"
#include "../Game/Objects/IStrategicObjectComp.h"
#include "../Game/Objects/StrategicObjectComp.h"
#include "../Game/Objects/Game/RemoteControlledObject.h"
#include "../Game/Objects/IWeaponObject.h"
#include "../Game/Objects/IVehicleHudComp.h"
#include "../Game/Objects/Base/SimpleObject.h"
#include "../Game/Objects/Game/Kit.h"
#include "../Game/Objects/GameLogicComp/IAbilityGLComp.h"
#include "../Game/Objects/GameLogicComp/IArmorGLComp.h"
#include "../Game/Objects/ObjectLayoutAccess.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../Scene/Object/ObjectUtil.h"
#include "../IO/Network/INetworkable.h"
#include "../IO/Network/NetworkManager.h"
#include "../Sound/BfSound.h"
#include "../Math/Mat4.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Game/Objects/IControlPoint.h"
#include "../Game/Objects/IControlPointTemplate.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Game/GameLogic/SquadManager/Squad.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Core/Ids.h"
#include "../Core/ClassManager.h"
#include "../Physics/HeightmapCluster.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/ObjectPredicator.h"
#include "../Core/EventManager.h"
#include "../Core/StringUtil.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/Effect.h"
#include "../External/BFMeme/IntData.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include "../GeomMisc/MenuRend/IMemeTexture.h"
#include "../Game/GameLogic/SpawnSystem/SpawnManager.h"  // getGroupsForTeam/ForPlayer, getSpawnGroup
#include "../Game/GameLogic/SpawnSystem/SpawnGroup.h"    // ISpawnGroup accessors
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>   // the Minimap paint rows' out_of_range (retail bitset _Xran, 78F590)
namespace dice { namespace hfe { extern IGameLogic* gameLogic; extern IBase* squadManager; extern hud::BF2HudManager* hudManager; extern ClassManager* classManager; }
// update()'s gas/blinding-light arm reads this; its header is not included here, so it is
// declared the same way the line above declares the other engine globals. ds:0x9FDC3C.
namespace hfe { namespace world { extern bool blindingLightDamageShow; } }   // already inside `namespace dice`
// collectSpawnGroupIcons reaches this; Core/Globals.cpp defines it as dice::hfe::spawnManager
// (ds:0x8AD3A00 / PE 0x9E5680), an IBase* published through the module registry.
namespace hfe { extern IBase* spawnManager; }
// mapPointerToMapCoordinates' overhead arm reaches this: ds:0xA07E28 in BF2.exe, ELF ds:0x8ABAF24,
// defined in Core/Globals.cpp - declared here the way Bf2ObjectMarkerNode.cpp declares it.
namespace hfe { namespace world { extern IBase* heightmapCluster; } }
namespace meme {
void Bf2MenuMapNode::setMinimapTexture(std::string name)
{
    m_minimapTexture = m_system->loadTexture(name.c_str(), false);
    if (!m_minimapTexture) m_minimapTexture = m_defaultMinimapTexture;
}
// Sets static-map mode and clears the related transition state.
void Bf2MenuMapNode::setMapStatic(bool isStatic)
{
    m_field76c = 0.0f;                              // +76C
    m_field75c = 0;                                  // +75C
    m_delayedMapAngle = 0.0f;                        // +760, stored as an integer zero
    m_mapStatic = isStatic;                          // +73C, a byte
}

// Three one-line setters over the +6FC..+71B float block. Their only caller in
// the whole image is Bf2MiniMenuMapNode's +DC override (7CE3D0).
void Bf2MenuMapNode::setField70c(float value) { m_field708[1] = value; }
void Bf2MenuMapNode::setField704(float value) { m_field704 = value; }
void Bf2MenuMapNode::setField718(float value, bool alsoField700)
{
    m_field718 = value;                              // +718, unconditional
    if (alsoField700) m_field700 = value;            // +700, only when the flag is set
}

void Bf2MenuMapNode::setTeamFlagTexture(std::string name, int team)
{
    m_teamFlagTextures[team] = m_system->loadTexture(name.c_str(), true);
}
void Bf2MenuMapNode::setControlPointTexture(std::string name, int team)
{
    m_controlPointTextures[team] = m_system->loadTexture(name.c_str(), true);
}
void Bf2MenuMapNode::setControlPointBaseTexture(std::string name, int team)
{
    m_controlPointBaseTextures[team] = m_system->loadTexture(name.c_str(), true);
}
void Bf2MenuMapNode::releaseMinimapTexture()
{
    if (m_system) {
        m_system->releaseTexture(m_minimapTexture);
        m_minimapTexture = 0;
    }
}
void Bf2MenuMapNode::setAirstrikeMarker(hfe::Vec3 position, float duration)
{
    m_airstrikeActive = duration > 0.0f;
    if (m_airstrikeActive) {
        m_fieldaf0 = 0;
        m_airstrikePosition = position;
        m_fieldb0c = duration;
    }
}
void Bf2MenuMapNode::setUAVMarker(hfe::Vec3 position, float duration)
{
    hfe::world::IPlayer* player = hfe::world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    // Only ordered equality with zero passes the commander's reload check.
    if (player->getIsCommander() && !(m_uavReloadRemaining == 0.0f)) return;
    m_uavActive = duration > 0.0f;
    if (!m_uavActive) return;
    m_fieldaac = 0;
    m_uavPosition = position;
    m_fieldabc = 0;
    m_fieldac0 = 4;
    m_uavReloadRemaining = hfe::gameLogic ? hfe::gameLogic->getUAVReloadTime() : 0;
    m_uavActiveTime = duration;
    // fistp with truncation produces integer-indefinite for an invalid range.
    const double radius = hfe::gameLogic ? hfe::gameLogic->getUAVRadius() : 0;
    m_uavRadius = radius >= -2147483648.0 && radius < 2147483648.0
        ? static_cast<int>(radius) : (std::numeric_limits<int>::min)();
    m_uavReloading = true;
    const unsigned int sentinel = 0xffffffffu;
    if (m_uavObjects.find(sentinel) == m_uavObjects.end())
        m_uavObjects.insert(std::make_pair(sentinel,
                                           static_cast<hfe::world::IObject*>(nullptr)));
}
// Enables a supplies marker for a positive duration, otherwise disables it.
// An active marker stores position and duration and resets its transition state.
void Bf2MenuMapNode::setSuppliesMarker(hfe::Vec3 position, float duration)
{
    m_suppliesActive = duration > 0.0f;
    if (m_suppliesActive) {
        m_fieldb10 = 0;
        m_suppliesPosition = position;
        m_fieldb28 = duration;
    }
}
void Bf2MenuMapNode::clearVehicleDrops()
{
    m_vehicleDrops.clear();
}
// 77ACE0 (117 B), ECHud 74's map half. (D1). The record is built on the
// stack - active = true, the icon, the id, the position - and appended (779150 _Buynode, 777330
// _Incsize: list::push_back). The icon is loaded through the System on every call rather
// than cached, as shipped; the System's own registry is what keeps that cheap.
void Bf2MenuMapNode::addVehicleDrop(int id, hfe::Vec3 position)
{
    MapVehicleDrop drop;
    drop.active = true;
    drop.texture = m_system->loadTexture("Ingame/Minimap/Icons/icon_vehicledrop.tga", true);  // +8C
    drop.id = id;
    drop.position = position;
    m_vehicleDrops.push_back(drop);
}
// +F8, 772CA0 (59 B). (D1); renamed in D2 from setSelectionWord - the
// proof is at the declaration. A negative squad clears all ten (+40..+64); anything else stores
// the type. no upper bound, as shipped.
void Bf2MenuMapNode::setSquadRequestType(int squad, int type)
{
    if (squad < 0) {
        for (int i = 0; i != 10; ++i) m_squadRequestTypes[i] = 0;
        return;
    }
    m_squadRequestTypes[squad] = type;
}
// +F4, 77A8D0 (29 B). (D2). The reader: negative -> [0], above 10 -> [10].
// [10] is one past the array (the +68 word), as shipped - the clamp is 10, not 9.
int Bf2MenuMapNode::getSquadRequestType(int squad)
{
    int index = squad;
    if (squad < 0)
        return m_squadRequestTypes[0];
    if (squad > 10)
        index = 10;
    return m_squadRequestTypes[index];
}
// [CHOSEN] forwarder - see Bf2MenuMapNode.h. BF2HudManagerHudEvents.cpp's caller reaches +F8
// through this name; it is a virtual dispatch, exactly as the shipped `call [edx+0F8h]` is.
void dispatchMapSetSelectionWord(Bf2MenuMapNode* node, int index, unsigned int word)
{
    node->setSquadRequestType(index, static_cast<int>(word));
}
// +110, 7730B0 (61 B). (D1). [INFERRED] name. A five-way switch on
// eventId - 46: the constructor's four support textures at +9A8..+9B4 (see the constructor's
// mini_Ammo / mini_Repair / mini_Revive / mini_Medic loads). 46 and every other id fall to medic.
int Bf2MenuMapNode::getSupportRequestTexture(int eventId)
{
    switch (eventId) {
    case 47: return m_field9a8[0];                       // +9A8 mini_Ammo
    case 48: return m_field9a8[1];                       // +9AC mini_Repair
    case 49: case 50: return m_field9a8[2];              // +9B0 mini_Revive
    default: return m_field9a8[3];                       // +9B4 mini_Medic (46 and the default)
    }
}
void Bf2MenuMapNode::resetSatelliteReload()
{
    m_satelliteActive = false;
    m_satelliteScanX = 0;
    m_satelliteScanY = 0;
    if (hfe::gameLogic) {
        m_satelliteReloadRemaining = hfe::gameLogic->getSatelliteScanReloadTime();
        m_satelliteReloading = true;
    } else {
        m_satelliteReloading = true;
        m_satelliteReloadRemaining = 0;
    }
}
void Bf2MenuMapNode::resetUAVReload()
{
    m_uavReloadRemaining = hfe::gameLogic ? hfe::gameLogic->getUAVReloadTime() : 0;
    m_uavActive = false;
    m_uavReloading = true;
}
int Bf2MenuMapNode::cycleZoom() { return applyZoom(-1, true); }
int Bf2MenuMapNode::applyZoom(int level, bool remember)
{
    hfe::hudManager->getMode(); // The discarded virtual read is shipped.
    m_field718 = m_field700;
    if (level >= 0) {
        m_zoomLevel = level;
        updateViewBounds();                                     // +120
        if (level == 0) setViewCenter(getMapCenterOffset());      // +11C
        else recenterOnPlayer();                                  // +118
    } else {
        const int next = static_cast<int>(static_cast<unsigned int>(m_zoomLevel) + 1u);
        if (next > 2) {
            setViewCenter(getMapCenterOffset());
            m_zoomLevel = 0;
        } else m_zoomLevel = next;
        if (hfe::hudManager->isCommanderShown() && m_commanderMode) {
            updateViewBounds();
            recenterOnPlayer();
        } else {
            updateViewBounds();
            setViewCenter(m_commanderPosition);
        }
        const int current = m_zoomLevel;
        hfe::hudManager->getInformationAdapter()->setMapZoom(current);
    }
    if (hfe::hudManager->isCommanderShown() && m_commanderMode) {
        hfe::hudManager->disableVOHelpMessage(28);
        if (m_zoomLevel > 0) hfe::hudManager->enableVOHelpMessage(39);
        if (m_zoomLevel == 2) {
            m_field704 = 1;
            m_field718 = 0;
            hfe::world::IPlayer* player = hfe::world::playerManager->getLocalHumanPlayer();
            if (!m_overheadZoomActive) {
                if (hfe::squadManager && player) {
                    const int id = player->getId();
                    static_cast<hfe::ISquadManager*>(hfe::squadManager)->setCommanderOverheadCamZoomMode(id, true);
                }
                m_overheadZoomActive = true;
            }
            updateCommanderZoom();
        } else {
            hfe::world::IPlayer* player = hfe::world::playerManager->getLocalHumanPlayer();
            if (m_overheadZoomActive) m_overheadZoomActive = false;
            if (player) player->setCommanderCamOverhead(false);
            m_field704 = 1;
        }
    }
    if (remember) {
        if (m_commanderMode) m_savedCommanderZoom = m_zoomLevel;
        else if (m_squadMode) m_savedSquadZoom = m_zoomLevel;
        else m_savedZoom = m_zoomLevel;
    }
    int result = m_zoomLevel;
    if (result < 0) result = 0;
    else if (result > 2) result = 2;
    m_zoomDisplay = result;
    return result;
}
hfe::Vec2 Bf2MenuMapNode::getZoomScale(bool selectedLevel)
{
    float verticalExponent, horizontalExponent;
    if (selectedLevel) {
        int level = m_zoomLevel;
        verticalExponent = hfe::hudManager->isCommanderShown()
            ? m_zoomExponents[2][level] : m_zoomExponents[m_expandedMap ? 1 : 0][level];
        level = m_zoomLevel; // Retain the second index read before the second HUD callback.
        horizontalExponent = hfe::hudManager->isCommanderShown()
            ? m_zoomExponents[2][level] : m_zoomExponents[m_expandedMap ? 1 : 0][level];
    } else verticalExponent = horizontalExponent = m_currentZoomExponent;
    const double base = static_cast<double>(2.3f); // PE dbl930150 is widened float2.3.
    float horizontal = static_cast<float>(std::pow(base, horizontalExponent));
    double vertical = std::pow(base, verticalExponent);
    const float roundedVertical = static_cast<float>(vertical);
    if (hfe::gameLogic) {
        const float height = getSize().y;
        const double width = getSize().x;
        const double factor = 1.0 / (static_cast<double>(m_mapExtent.x) / m_textureSize.x * width);
        horizontal = static_cast<float>(factor * width * horizontal);
        vertical = factor * height * roundedVertical;
    }
    return hfe::Vec2{horizontal, static_cast<float>(vertical)};
}
hfe::Vec2 Bf2MenuMapNode::getMapCenterOffset()
{
    float x = 0;
    double y = 0;
    if (hfe::gameLogic) {
        const float height = getSize().y;
        const double width = getSize().x;
        const double inverseWidth = 1.0 / m_textureSize.x;
        const float offsetX = static_cast<float>(inverseWidth * m_mapOffset.x * width);
        const double inverseHeight = 1.0 / m_textureSize.y;
        const float offsetY = static_cast<float>(inverseHeight * m_mapOffset.y * height);
        const float extentX = static_cast<float>(inverseWidth * m_mapExtent.x * width);
        const float extentY = static_cast<float>(inverseHeight * m_mapExtent.y * height);
        x = static_cast<float>((extentX * 0.5 + offsetX - width * 0.5) / width);
        y = (extentY * 0.5 + offsetY - height * 0.5) / height;
    }
    return hfe::Vec2{x, static_cast<float>(y)};
}
void Bf2MenuMapNode::updateCommanderZoom()
{
    hfe::world::IPlayer* player = hfe::world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    hfe::world::ICameraObject* camera = hfe::hudManager->getCameraObject(player->getTeam());
    if (!camera) return;
    static_cast<hfe::menu::Ref2System*>(m_system)->getRenderView();
    const float tangent = static_cast<float>(std::tan(camera->getActiveFOV() * 0.5));
    const float height = hfe::hudManager->getCommanderCameraHeight();
    const float width = static_cast<float>(static_cast<double>(height) * tangent * 2.0);
    m_zoomExponents[2][2] = static_cast<float>(std::log(1.0 / m_worldToMapScale / width) / std::log(static_cast<double>(2.3f)));
    const hfe::Vec2& firstPosition = getPosition();
    const float x = static_cast<float>(static_cast<double>(firstPosition.x) / getSize().x * width);
    const hfe::Vec2& secondPosition = getPosition();
    const float y = static_cast<float>(static_cast<double>(secondPosition.y) / getSize().y * width * -1.0);
    m_centers[2].x = x * 0.5f;
    m_centers[2].y = y * 0.5f;
}
void Bf2MenuMapNode::prepareOrder(int type)
{
    if (type >= 4 && type <= 9 && m_zoomLevel < 2) {
        const float scaleY = getZoomScale(false).y;
        const float distance = static_cast<float>(static_cast<double>(std::numeric_limits<float>::max()) / (scaleY * 10.0));
        snapOrderToControlPoint(m_worldToMapScale, m_commanderPosition, distance);
    }
    updateOrder(m_order, type, m_worldToMapScale, m_commanderPosition, 0);
}
bool Bf2MenuMapNode::snapOrderToControlPoint(float scale, hfe::Vec2 position, float distance)
{
    bool found = false;
    if (!hfe::world::playerManager->getLocalHumanPlayer()) return false;
    hfe::Vec2 selected{0, 0};
    if (!hfe::gameLogic) return false;
    using ControlPoints = std::map<unsigned int, hfe::world::IObject*>;
    ControlPoints& points = *hfe::gameLogic->getControlPointMap();
    for (auto it = points.begin(); it != points.end(); ++it) {
        hfe::world::IObject* object = it->second;
        if (!object) continue;
        auto* point = static_cast<hfe::world::IControlPoint*>(object->queryInterface(hfe::world::IID_IControlPoint));
        if (!point || !point->getShowOnMinimap()) continue;
        const hfe::Vec3& world = object->getAbsolutePosition();
        const float x = static_cast<float>(static_cast<double>(world.x) * scale);
        const float y = static_cast<float>(-static_cast<double>(world.z) * scale);
        const double dx = static_cast<double>(position.x) - x;
        const double dy = static_cast<double>(position.y) - y;
        const float squared = static_cast<float>(dy * dy + dx * dx);
        const float zoomY = getZoomScale(false).y;
        if (static_cast<double>(m_controlPointSnapDistance) / zoomY > squared && squared < distance) {
            distance = squared;
            found = true;
            const double offset = static_cast<double>(scale) * -1.0;
            const float roundedOffset = static_cast<float>(offset);
            selected.x = static_cast<float>(offset + x);
            selected.y = static_cast<float>(static_cast<double>(roundedOffset) + y);
        }
    }
    if (found) m_commanderPosition = selected;
    return found;
}
bool Bf2MenuMapNode::updateOrder(hfe::SquadOrder*& order, int type, float,
                               hfe::Vec2 position, unsigned short target)
{
    if (m_orderPointCount > 0) {
        order = nullptr; // Retail abandons the previous pointer without deleting it.
        m_orderPointCount = 0;
    }
    if (!order) {
        void* storage = ::operator new(sizeof(hfe::SquadOrder));
        hfe::SquadOrder* allocated = storage ? new(storage) hfe::SquadOrder : nullptr;
        if (allocated) for (auto& point : allocated->points) point = hfe::Vec3{0, 0, 0};
        order = allocated;
        for (auto& point : order->points) point = hfe::Vec3{-4000, -4000, -4000};
    }
    order->orderId = type;
    order->targetNetId = target;
    const hfe::Vec3 point = mapCoordinatesToGround(position, true);
    order->points[m_orderPointCount] = point;
    hfe::hudManager->setSquadOrder(*order);
    ++m_orderPointCount;
    return true;
}
void mapCoordinatesToWorld(hfe::Vec3& output, const hfe::Vec2& position)
{
    int fallbackX = 0;
    {
        const std::string key("GLSWorldSizeX");
        const int& extent = hfe::settings->intGet(key, fallbackX);
        output.x = static_cast<float>(static_cast<double>(extent) * position.x);
    }
    output.y = 0;
    int fallbackZ = 0;
    {
        const std::string key("GLSWorldSizeZ");
        const int& extent = hfe::settings->intGet(key, fallbackZ);
        output.z = static_cast<float>(static_cast<double>(extent) * position.y * -1.0);
    }
}
void worldCoordinatesToMap(const hfe::Vec3& position, hfe::Vec2& output)
{
    int fallbackX = 0;
    {
        const std::string key("GLSWorldSizeX");
        const int& extent = hfe::settings->intGet(key, fallbackX);
        output.x = static_cast<float>(position.x / static_cast<double>(extent));
    }
    int fallbackZ = 0;
    {
        const std::string key("GLSWorldSizeZ");
        const int& extent = hfe::settings->intGet(key, fallbackZ);
        output.y = static_cast<float>(static_cast<double>(position.z) * -1.0 / extent);
    }
}
hfe::Vec3 mapCoordinatesToGround(const hfe::Vec2& coordinates, bool normalized)
{
    hfe::Vec3 from{0, 0, 0};
    float groundHeight = 0;
    if (normalized) {
        int width, depth;
        {
            int fallback = 0;
            const std::string key("GLSWorldSizeX");
            width = hfe::settings->intGet(key, fallback);
        }
        {
            int fallback = 0;
            const std::string key("GLSWorldSizeZ");
            depth = hfe::settings->intGet(key, fallback);
        }
        from.x = static_cast<float>(static_cast<double>(width) * coordinates.x);
        from.z = static_cast<float>(static_cast<double>(depth) * coordinates.y * -1.0);
    } else {
        from.x = coordinates.x;
        from.z = coordinates.y;
    }
    hfe::world::IHeightmapCluster* terrain;
    {
        const std::string key("HeightmapCluster");
        terrain = static_cast<hfe::world::IHeightmapCluster*>(hfe::classManager->getSingleton(key));
    }
    if (terrain) groundHeight = terrain->getHeightInWorldCoords(from);
    from.y = groundHeight + 100;
    hfe::Vec3 hitPoint{0, 0, 0};
    hfe::Vec3 hitNormal{0, 0, 0};
    hfe::world::ObjectFlagPredicatorEx predicator(0x1000100u, 8u);
    const hfe::Vec3 direction{0, -100, 0};
    hfe::world::IObject* hitObject; // Retail leaves these two outputs uninitialized.
    int hitPart;
    if (hfe::world::objectManager->intersectLine(hitObject, hitPoint, hitNormal,
            hitPart, from, direction, &predicator, true, false, false, true, 0)
        && groundHeight < hitPoint.y)
        groundHeight = hitPoint.y;
    return hfe::Vec3{from.x, groundHeight, from.z};
}
void Bf2MenuMapNode::clearOrder()
{ m_order = nullptr; }

// Map display and input handling for the HUD.
namespace {
// [INFERRED] descriptor class name. Counter A1D490 (773160/773170/773180) has only the
// add/sub/get references; construction and destruction do not adjust it.
int bf2MenuMapNodeInstances;
class Bf2MenuMapNodeTypeDescriptor final : public TypeDescriptor
{
public:
    Bf2MenuMapNodeTypeDescriptor() : TypeDescriptor("dice::meme::Bf2MenuMapNode") {}
    TypeDescriptor* getBaseType() override { return Bf2TransformNode::getClassType(); }
    RuntimeObject* create() override
    {
        // [FRONTIER] [CHOSEN, not recovered] Saved-node construction is unimplemented; the builder constructs nodes directly.
        std::fprintf(stderr, "[FRONTIER] dice::meme::Bf2MenuMapNodeTypeDescriptor::create reached\n");
        std::fflush(stderr);
        std::abort();
    }
    void addInstance() override { ++bf2MenuMapNodeInstances; }
    void removeInstance() override { --bf2MenuMapNodeInstances; } // 773170, no zero guard
    int getInstanceCount() override { return bf2MenuMapNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(Bf2MenuMapNode); }
};

// 42 filter sets, replayed from the constructor's word/byte stores (7805F3..780A2D) on a
// byte image of the retail object and read back as 43-bit values; [2] is also what
// 772060 copies into the live states at 781E74. Evidence: scratchpad filters.py.
const unsigned long long mapFilterSets[7][6] = {
    { 0x000000308001045BULL, 0x000001308001845BULL, 0x000001308001845BULL, 0x000001308001845BULL, 0x000001308001845BULL, 0x000001308001845BULL },
    { 0x0000003000010458ULL, 0x0000003000010458ULL, 0x0000003000010458ULL, 0x0000003000010458ULL, 0x0000003000010458ULL, 0x0000003000010458ULL },
    { 0x000002BF7F437FFBULL, 0x000003BF7F43FFFBULL, 0x000003BF7F43FFFBULL, 0x000001BF7F43FFFBULL, 0x000003BF7F43FFFBULL, 0x000003BF7F43FFFBULL },
    { 0x000002BF7F537FFBULL, 0x000003BF7F53FFFBULL, 0x000003BF7F53FFFBULL, 0x000001BF7F53FFFBULL, 0x000003BF7F53FFFBULL, 0x000003BF7F53FFFBULL },
    { 0x000002FF7F7B7FFBULL, 0x000003FF7F7BFFFBULL, 0x000003FF7F7BFFFBULL, 0x000001FF7F7BFFFBULL, 0x000003FF7F7BFFFBULL, 0x000003FF7F7BFFFBULL },
    { 0x000002FF7F7B7FFBULL, 0x000003FF7F7B7FFBULL, 0x000003FF7F7B7FFBULL, 0x000001FF7F7B7FFBULL, 0x000003FF7F7B7FFBULL, 0x000001C341380101ULL },
    { 0x0000000000000000ULL, 0x000007FFFFFFFFFFULL, 0x000007FFFFFFFFFFULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x000007FFFFFFFFFFULL },
};

// Returns the shared zero marker position, initialized on first use.
const hfe::Vec3& zeroMarkerPosition()
{
    static const hfe::Vec3 position = {0.0f, 0.0f, 0.0f};
    return position;
}

// ds:A1D0A8 is the active-combat-area mask cache. It lives in the PE's zeroed
// writable section; ECCore 13 also resets it explicitly.
std::uint32_t mapStatic_A1D0A8 = 0;

// 77D7C0. [INFERRED] helper name; the function is internal to the map TU and
// carries no Bf2MenuMapNode state. Every dependency is a typed engine interface.
void rasterizeCombatArea(hfe::geom::IMemeTexture* texture,
                         const std::vector<hfe::Vec2>& points,
                         unsigned char value,
                         hfe::Vec2& cropMin,
                         hfe::Vec2& cropExtent,
                         bool measureOnly)
{
    hfe::world::IHeightmapCluster* cluster;
    {
        const std::string name("HeightmapCluster");
        cluster = static_cast<hfe::world::IHeightmapCluster*>(
            hfe::classManager->getSingleton(name));
    }
    hfe::world::IHeightmap* const heightmap = cluster->getPrimaryHeightmap();
    const float lowX = heightmap->getLowBoundaryX();
    const float highX = heightmap->getHighBoundaryX();
    const float lowZ = heightmap->getLowBoundaryZ();
    const float highZ = heightmap->getHighBoundaryZ();

    std::vector<hfe::geom::IMemeTexture::CanvasPoint> canvas;
    for (std::vector<hfe::Vec2>::const_iterator it = points.begin();
         it != points.end(); ++it) {
        const float x = (it->x - lowX) / (highX - lowX);
        const float y = 1.0f - (it->y - lowZ) / (highZ - lowZ);
        if (measureOnly) {
            const float pixelX = x * 512.0f;
            const float pixelY = y * 512.0f;
            if (pixelX < cropMin.x) cropMin.x = pixelX;
            if (pixelY < cropMin.y) cropMin.y = pixelY;
            if (pixelX > cropExtent.x) cropExtent.x = pixelX;
            if (pixelY > cropExtent.y) cropExtent.y = pixelY;
        } else {
            float mappedX = (512.0f / cropExtent.x) *
                            (x - cropMin.x * 0.001953125f);
            float mappedY = (512.0f / cropExtent.y) *
                            (y - cropMin.y * 0.001953125f);
            if (mappedX < 0.99f) {
                if (mappedX <= 0.01f) mappedX = 0.01f;
            } else {
                mappedX = 0.99f;
            }
            if (mappedY < 0.99f) {
                if (mappedY <= 0.01f) mappedY = 0.01f;
            } else {
                mappedY = 0.99f;
            }
            canvas.push_back(hfe::geom::IMemeTexture::CanvasPoint(mappedX, mappedY));
        }
    }
    if (!measureOnly) {
        // hand over A buffer the callee may free. plotAndFill releases `first` itself, so it
        // must not be `canvas`'s own storage - that would be freed twice. The span layout is the
        // shipped sixteen-byte one; see IMemeTexture.h for why a std::vector cannot be used here.
        hfe::geom::IMemeTexture::CanvasPointSpan span = { 0, 0, 0, 0 };
        if (!canvas.empty()) {
            const std::size_t bytes =
                canvas.size() * sizeof(hfe::geom::IMemeTexture::CanvasPoint);
            span.first = static_cast<float*>(std::malloc(bytes));   // engineFree's heap (native)
            if (span.first) {
                std::memcpy(span.first, &canvas[0], bytes);
                span.last = reinterpret_cast<float*>(
                    reinterpret_cast<char*>(span.first) + bytes);
                span.end = span.last;
            }
        }
        texture->plotAndFill(span, value);
    }
}
}

// Order follows the shipped stores; containers construct natively where the
// original ran their constructors. Textures load through the borrowed System's virtual
// +8C with menuPath=true, exactly once per shipped call (the same path appears twice for
// +974/+978 and +8DC is loaded twice: both preserved).
Bf2MenuMapNode::Bf2MenuMapNode(System* system, const char* name, int, int, int, int)
    : Bf2TransformNode(nullptr, 0, 0, 0, 0)          // five zero words
{
    // 7801AB..780206: the scalar/bitset zeroing before the borrowed System store
    m_worldToMapScale = 0.0f;                        // +84
    m_field088 = 0.0f;                               // +88
    m_system = system;                               // +358
    m_uavPosition = m_airstrikePosition = m_suppliesPosition = zeroMarkerPosition(); // +AB0/+AF4/+B14 zero words
    m_fieldb4c.r = m_fieldb4c.g = m_fieldb4c.b = m_fieldb4c.a = 1.0f; // +B4C..B58 (overwritten below, as shipped)
    m_fieldb5c.x = m_fieldb5c.y = m_fieldb5c.width = m_fieldb5c.height = m_fieldb5c.z = 0.0f;
    m_mapOffset.x = m_mapOffset.y = 0.0f;            // +B74/+B78
    m_mapExtent.x = m_mapExtent.y = 0.0f;            // +B7C/+B80 (512 arrives at 781F6D)
    m_fieldc64 = 0;                                  // +C64 (the map at +C54 constructs natively)
    m_fieldc68.r = m_fieldc68.g = m_fieldc68.b = m_fieldc68.a = 1.0f; // +C68..C74
    m_fieldc78 = m_fieldc7c = 0.0f;                  // +C78/+C7C
    m_field068 = 0;                                  // +68
    setName(name);
    m_order = nullptr;                               // +360
    m_field364 = false;                              // +364
    m_orderPointCount = 0;                           // +368
    m_field670 = 0.7f;                               // +670
    // 7805E7..780A2D: the 42 filter sets (see mapFilterSets)
    for (int set = 0; set != 7; ++set)
        for (int mode = 0; mode != 6; ++mode)
            m_filterSets[set][mode] = std::bitset<43>(mapFilterSets[set][mode]);
    // 780684..7806D1: every icon scale is (0.7, 0.7); [8] and [4] become (0.7, -1)
    for (int i = 0; i != 43; ++i) { m_iconScales[i].x = m_field670; m_iconScales[i].y = m_field670; }
    m_iconScales[8].x = m_field670; m_iconScales[8].y = -1.0f;    // +558/+55C
    m_iconScales[4].x = m_field670; m_iconScales[4].y = -1.0f;    // +538/+53C
    // 780A33..780C59: the information-layer registrations, in shipped order
    hfe::HudInformationLayer* information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerFloat(hfe::formatString("%sDelayedMapAngle", m_name), &m_delayedMapAngle); // +760
    information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerFloat(hfe::formatString("%sSatelliteTimeUntilReloaded", m_name), &m_satelliteReloadFraction); // +AA8
    information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerBool(hfe::formatString("%sSatelliteActive", m_name), &m_satelliteActive);       // +A7D
    information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerBool(hfe::formatString("%sSatelliteReloading", m_name), &m_satelliteReloading); // +A7C
    information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerFloat(hfe::formatString("%sUAVTimeUntilReloaded", m_name), &m_uavReloadFraction); // +ADC
    information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerBool(hfe::formatString("%sUAVState", m_name), &m_uavState);                     // +AE8
    information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerBool(hfe::formatString("%sUAVActive", m_name), &m_uavActive);                   // +ACC
    information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerBool(hfe::formatString("%sUAVReloading", m_name), &m_uavReloading);             // +AE0
    information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerBool(hfe::formatString("%sShowKitsOnMap", m_name), &m_showKitsOnMap);           // +38D
    information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
    information->registerInt(hfe::formatString("%sZoomDisplay", m_name), &m_zoomDisplay);                // +6D4
    for (int i = 0; i < 32; ++i) {                   // 780C59..780D32, decoded loop 0..31 step 1
        information = static_cast<hfe::HudInformationLayer*>(hfe::g_hudInformationLayer);
        information->registerBool(hfe::formatString("%sMapFilter%iActive", m_name, i), &m_mapFilterActive[i]); // +36D+i
    }
    for (int i = 0; i < 43; ++i)                     // 780D37..780D8C, decoded loop 0..42 step 1
        m_icons[i].reserve(32);                      // 77A220 with decoded 32
    // 780D8E..: textures and the remaining scalars, in shipped order
    m_minimapTexture = 0;                            // +938
    m_field93c[0] = 0;                               // +93C
    m_field93c[3] = m_field93c[2] = m_system->loadTexture("Ingame/Minimap/map_CombatArea32.dds", true); // +948/+944
    m_field93c[1] = 0;                               // +940
    std::memset(m_supplyLineTextures, 0, sizeof(m_supplyLineTextures));  // +BA4..BF3, 0x50 bytes
    m_field928[2] = m_system->loadTexture("Ingame/Minimap/map_Shadow.tga", true);   // +930
    m_field928[3] = m_system->loadTexture("Ingame/Minimap/map_Frame.tga", true);    // +934
    m_field928[1] = m_system->loadTexture("Ingame/Minimap/map_Mask.tga", true);     // +92C
    std::memset(m_field7fc, 0, sizeof(m_field7fc));  // +7FC..823, decoded 0x28
    std::memset(m_squadNumberIcons, 0, sizeof(m_squadNumberIcons));  // +824..84B, decoded 0x28
    std::memset(m_field84c, 0, sizeof(m_field84c));  // +84C..89B, decoded 0x50
    for (int i = 0; i != 10; ++i) m_squadRequestTypes[i] = 0; // +40..+64
    m_field93c[14] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_commander1.tga", true); // +974
    m_field93c[15] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_commander1.tga", true); // +978, same path twice
    m_field93c[16] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_commander2.tga", true); // +97C
    m_field8a8[0] = m_system->loadTexture("Ingame/Minimap/map_Line.tga", true);                        // +8A8
    m_field8a8[1] = m_system->loadTexture("Ingame/Flags/Icons/Minimap/SupplyLine/sl2_line.tga", true); // +8AC
    m_field8a8[2] = m_system->loadTexture("Ingame/Flags/Icons/Minimap/SupplyLine/sl2_arrow.tga", true); // +8B0
    m_field928[0] = m_system->loadTexture("Ingame/Minimap/Icons/request_circle.tga", true);            // +928
    m_field8a8[3] = m_system->loadTexture("Ingame/Minimap/Icons/icon_artillery.tga", true);            // +8B4
    m_field8a8[4] = m_system->loadTexture("Ingame/Minimap/Icons/icon_UAV.tga", true);                  // +8B8
    m_field8a8[6] = m_system->loadTexture("Ingame/Minimap/Icons/icon_supplies.tga", true);             // +8C0
    m_field8a8[5] = m_system->loadTexture("Ingame/Minimap/Icons/icon_vehicledrop.tga", true);          // +8BC
    m_field8a8[7] = m_system->loadTexture("Ingame/Minimap/Icons/orderMove.tga", true);                 // +8C4
    m_field8a8[8] = m_system->loadTexture("Ingame/Minimap/Icons/orderAttack.tga", true);               // +8C8
    m_field8a8[9] = m_system->loadTexture("Ingame/Minimap/Icons/orderDefend.tga", true);               // +8CC
    m_field8a8[13] = m_system->loadTexture("Ingame/Minimap/Icons/orderAttack.tga", true);              // +8DC (first load)
    m_field8a8[10] = m_system->loadTexture("Ingame/Minimap/Icons/orderRepair.tga", true);              // +8D0
    m_field8a8[11] = m_system->loadTexture("Ingame/Minimap/Icons/orderDestroy.tga", true);             // +8D4
    m_field8a8[12] = m_system->loadTexture("Ingame/Minimap/Icons/orderMine.tga", true);                // +8D8
    m_field8a8[13] = m_system->loadTexture("Ingame/Minimap/Icons/icon_artillery.tga", true);           // +8DC (overwritten, as shipped)
    m_field8a8[14] = m_system->loadTexture("Ingame/Minimap/Icons/icon_UAV.tga", true);                 // +8E0
    m_field8a8[16] = m_system->loadTexture("Ingame/Minimap/Icons/icon_supplies.tga", true);            // +8E8
    m_field8a8[15] = m_system->loadTexture("Ingame/Minimap/Icons/icon_vehicledrop.tga", true);         // +8E4
    m_field8a8[17] = m_system->loadTexture("Ingame/Minimap/Icons/orderMoveCommander.tga", true);       // +8EC
    m_field8a8[18] = m_system->loadTexture("Ingame/Minimap/Icons/orderAttackCommander.tga", true);     // +8F0
    m_field8a8[19] = m_system->loadTexture("Ingame/Minimap/Icons/orderDefendCommander.tga", true);     // +8F4
    m_controlPointTextures[0] = m_system->loadTexture("Ingame/Minimap/Icons/orderAttackCommander.tga", true); // +904 (zeroed below)
    m_field8a8[20] = m_system->loadTexture("Ingame/Minimap/Icons/orderRepairCommander.tga", true);     // +8F8
    m_field8a8[21] = m_system->loadTexture("Ingame/Minimap/Icons/orderDestroyCommander.tga", true);    // +8FC
    m_field8a8[22] = m_system->loadTexture("Ingame/Minimap/Icons/orderMineCommander.tga", true);       // +900
    m_field89c = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_Squad_request.tga", true);    // +89C
    for (int i = 0; i != 3; ++i) m_controlPointTextures[i] = m_controlPointBaseTextures[i] = m_teamFlagTextures[i] = 0; // +904..924
    m_field93c[4] = 0;                                                                                   // +94C
    m_field93c[9] = m_system->loadTexture("Ingame/Minimap/Icons/spawn_SelectedInactive.tga", true);      // +960
    m_field93c[10] = m_system->loadTexture("Ingame/Minimap/Icons/spawn_Selected.tga", true);             // +964
    m_field93c[11] = m_system->loadTexture("Ingame/Minimap/Icons/spawn_SquadSelectedInactive.tga", true); // +968
    m_field93c[12] = m_system->loadTexture("Ingame/Minimap/Icons/spawn_SquadSelected.tga", true);        // +96C
    m_field93c[5] = m_system->loadTexture("Ingame/Minimap/Icons/spawn_UnSelectedInactive.tga", true);    // +950
    m_field93c[6] = m_system->loadTexture("Ingame/Minimap/Icons/spawn_UnSelected.tga", true);            // +954
    m_field93c[7] = m_system->loadTexture("Ingame/Minimap/Icons/spawn_SquadUnSelectedInactive.tga", true); // +958
    m_field93c[8] = m_system->loadTexture("Ingame/Minimap/Icons/spawn_SquadUnSelected.tga", true);       // +95C
    m_field93c[18] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_LocalArrow2.tga", true);    // +984
    m_field93c[19] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_Soldier.tga", true);        // +988
    m_field93c[20] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_Health.tga", true);         // +98C
    m_field93c[17] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_headingArrow.tga", true);   // +980
    m_field9a8[3] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_Medic.tga", true);           // +9B4
    m_field9a8[4] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_MedicArrow.tga", true);      // +9B8
    m_field9a8[0] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_Ammo.tga", true);            // +9A8
    m_field9a8[1] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_Repair.tga", true);          // +9AC
    m_field9a8[2] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_Revive.tga", true);          // +9B0
    m_field9a8[8] = m_system->loadTexture("Ingame/Minimap/Icons/laserpaintTarget.tga", true);            // +9C8
    m_field9a8[9] = m_system->loadTexture("Ingame/Minimap/Icons/laserpaintTargetActive.tga", true);      // +9CC
    m_field9a8[10] = m_system->loadTexture("Ingame/Minimap/Icons/unitSpotted.tga", true);                // +9D0
    m_fielda94 = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_SatelliteObject.tga", true);    // +A94
    m_field93c[22] = m_system->loadTexture("Ingame/Player/Icons/Minimap/mini_SatelliteObject.tga", true); // +994
    m_defaultMinimapTexture = m_system->loadTexture("Ingame/GeneralIcons/full.tga", true);               // +8A4
    m_field8a0 = m_system->loadTexture("Ingame/GeneralIcons/empty.tga", true);                           // +8A0
    m_field694 = 0.5f;                               // +694
    m_expandedMap = false; m_field690 = false;       // +68E/+690 (78181C/781822)
    m_commanderMode = false; m_squadMode = false;
    m_field776 = true;                               // +776
    m_selectedRequestSquad = -1;                     // +B88
    m_selectedRequestType = 0;                       // +B8C
    m_selectedRequestOrderType = static_cast<hfe::OrderType>(2); // +B90
    m_satelliteActive = false; m_satelliteReloading = false; // +A7D/+A7C
    m_fielda84 = m_fielda88 = m_fielda8c = 0;        // +A84..A8C
    m_fielda90 = m_system->loadTexture("Ingame/Minimap/map_SatelliteScanLine.tga", true); // +A90
    m_satelliteScanX = m_satelliteScanY = 0.0f;      // +A98/+A9C
    m_satelliteLineSize = 150;                       // +AA0
    m_satelliteReloadRemaining = 0.0f;               // +AA4
    m_satelliteReloadFraction = 1.0f;                // +AA8
    m_fielda80 = 0;                                  // +A80
    m_fieldaac = 0.0f;                               // +AAC
    m_uavPosition = zeroMarkerPosition();            // +AB0..AB8 (6D8F10)
    m_fieldabc = 0.0f; m_fieldac0 = 4.0f; m_fieldac4 = 0.0f; // +ABC..AC4
    m_uavActive = false;                             // +ACC
    m_uavReloadRemaining = 0.0f;                     // +AC8
    m_uavReloadFraction = 1.0f;                      // +ADC
    m_uavReloading = false;                          // +AE0
    m_uavActiveTime = 0.0f;                          // +AE4
    m_uavRadius = 100;                               // +AD0
    m_uavTexture = m_system->loadTexture("Ingame/Minimap/map_UAV.tga", true);          // +AD4
    m_uavIcon = m_system->loadTexture("Ingame/Minimap/Icons/icon_UAV.tga", true);       // +AD8
    m_uavState = false;                              // +AE8
    m_fieldaec = 0.5f; m_fieldaf0 = 0.0f;            // +AEC/+AF0
    m_airstrikePosition = zeroMarkerPosition();      // +AF4..AFC
    m_airstrikeActive = false;                       // +B00
    m_airstrikeTexture = m_system->loadTexture("Ingame/Minimap/Icons/placedAirstrike.tga", true); // +B04
    m_artilleryIcon = m_system->loadTexture("Ingame/Minimap/Icons/icon_artillery.tga", true);     // +B08
    m_fieldb0c = m_fieldb10 = 0.0f;                  // +B0C/+B10
    m_suppliesPosition = zeroMarkerPosition();       // +B14..B1C
    m_suppliesActive = false;                        // +B20
    m_fieldb28 = 0;                                  // +B28
    m_suppliesTexture = m_system->loadTexture("Ingame/Minimap/Icons/icon_supplies.tga", true);   // +B24
    m_field07f = m_field07c = m_field07d = m_field07e = false; // +7F,+7C,+7D,+7E
    m_field678 = 8;                                  // +678
    m_field71c[2] = 0;                               // +724
    m_field76c = 0.0f; m_field770 = 0;               // +76C/+770
    m_field75c = 0; m_delayedMapAngle = 0.0f; m_field764[0] = m_field764[1] = 0; // +75C..768
    m_field758 = 1.0f;                               // +758
    m_field774 = false; m_field775 = true; m_field777 = false; // +774/+775/+777
    m_currentZoomExponent = 0.0f;                    // +698
    m_savedZoom = m_savedCommanderZoom = m_savedSquadZoom = 0; // +6C4..6CC
    m_field69c[0] = 1; m_field69c[1] = 1; m_field69c[2] = 0; m_field69c[3] = 1; m_field69c[4] = 0;
    m_field69c[5] = 1; m_field69c[6] = 1; m_field69c[7] = 1; m_field69c[8] = 1; m_field69c[9] = 1; // +69C..6C0
    m_zoomLevel = 1;                                 // +6D0
    m_zoomDisplay = 0;                               // +6D4
    m_zoomExponents[0][0] = 0.5f; m_zoomExponents[0][1] = 1.5f; m_zoomExponents[0][2] = 2.5f; // +6D8..6E0
    m_zoomExponents[1][0] = 0.0f; m_zoomExponents[1][1] = 1.0f; m_zoomExponents[1][2] = 2.0f; // +6E4..6EC
    m_zoomExponents[2][0] = 0.0f; m_zoomExponents[2][1] = 1.0f; m_zoomExponents[2][2] = 2.0f; // +6F0..6F8
    for (int i = 0; i != 3; ++i) { m_centers[i].x = 0.0f; m_centers[i].y = 0.0f; } // +7C8..7DC
    m_field708[0] = 1.0f; m_field708[1] = 1.0f;      // +708/+70C
    m_field6fc = 1.0f; m_field704 = 1.0f;            // +6FC/+704
    m_field708[2] = 1.0f; m_field708[3] = 0.8f;      // +710/+714
    m_field700 = 1.0f; m_field718 = 1.0f;            // +700/+718
    m_miniViewOffset.x = 0.0f;                       // +740/+744
    m_field748.x = m_field748.y = 0.0f;              // +748/+74C
    m_field750.x = m_field750.y = 0.0f;              // +750/+754
    m_miniViewOffset.y = 0.0f;
    m_field72c = 0.02f;                              // +72C
    m_overheadZoomActive = false;                    // +7F8
    m_field730 = 0.0025f; m_field734 = 0.01f;        // +730/+734
    m_controlPointSnapDistance = 0.0004f;            // +738
    m_fieldb4c.r = 0.39215687f; m_fieldb4c.g = 0.6666667f; m_fieldb4c.b = 0.9411765f; m_fieldb4c.a = 1.0f; // +B4C..B58
    m_fieldb44 = m_fieldb48 = 6;                     // +B44/+B48
    m_commanderPosition.x = m_commanderPosition.y = -4000.0f; // +7E0/+7E4
    m_field7e8.x = m_field7e8.y = 0.0f;              // +7E8/+7EC
    m_viewCenter.x = m_viewCenter.y = 0.0f;          // +7F0/+7F4
    m_field7c0.x = -200.0f; m_field7c0.y = -256.0f;  // +7C0/+7C4
    m_textureSize.x = 194.0f; m_textureSize.y = -297.0f; // +7B8/+7BC (the shipped stores, in this order)
    m_field7a8.x = 241.0f; m_field7a8.y = 20.0f;     // +7A8/+7AC
    m_field7b0.x = 558.0f; m_field7b0.y = 558.0f;    // +7B0/+7B4
    m_field790 = m_field7c0;                         // +790/+794
    m_field7a0 = m_textureSize;                      // +7A0/+7A4
    m_field788.x = 197.0f; m_field788.y = -300.0f;   // +788/+78C
    m_field798.x = 194.0f; m_field798.y = 194.0f;    // +798/+79C
    m_field780 = m_field788;                         // +780/+784
    m_field778 = m_field798;                         // +778/+77C
    getPosition() = m_field780;                      // the inherited position takes +780/+784
    getSize() = m_field778;                          // the inherited size takes +778/+77C
    m_field7f9 = true;                               // +7F9
    m_field68c = false;                              // +68C
    m_spawnGroup = 0;                                // +9A4
    m_field684 = false;                              // +684
    m_field9a8[11] = m_field9a8[12] = 0;             // +9D4/+9D8
    m_field9dc = false;                              // +9DC
    m_field93c[25] = 0;                              // +9A0
    m_fielda30 = 0;                                  // +A30
    m_fielda24 = 0.5f; m_fielda28 = 1.0f; m_fielda2c = 12.0f; // +A24..A2C
    m_fielda18 = false;                              // +A18
    m_fielda14 = 0;                                  // +A14
    m_fielda08 = 0;                                  // +A08
    m_fielda0c = 4.0f; m_fielda10 = 0.18f;           // +A0C/+A10
    m_field9e0[2] = 0;                               // +9E8
    m_fielda50 = -0.49f; m_fielda54 = 0.49f; m_fielda58 = -0.49f; m_fielda5c = 0.49f; // +A50..A5C
    m_minCenterY = m_fielda50; m_maxCenterY = m_fielda54; m_minCenterX = m_fielda58; m_maxCenterX = m_fielda5c; // +A60..A6C
    m_field67c = 0.55f; m_field680 = 0.1f;           // +67C/+680
    m_fielda74 = false;                              // +A74
    m_fielda70 = -1;                                 // +A70
    m_fielda75 = m_fielda76 = m_fielda77 = false;    // +A75..A77
    m_fielda4c = false;                              // +A4C
    m_field36c = true;                               // +36C
    m_field06c = false;                              // +6C
    m_showKitsOnMap = false;                         // +38D
    applyFilterSet(m_filterSets[2]);
    m_field3c0 = std::bitset<39>(0x0000007F000000FFULL); // +3C0/+3C4: -1 & 0xFF, -1 & 0x7F
    m_field674 = 1;                                  // +674
    m_fielda7b = true;                               // +A7B
    m_fieldb94 = m_fieldb95 = false;                 // +B94/+B95
    m_fieldb9c = m_fieldba0 = 0;                     // +B9C/+BA0
    m_controlPoints.clear();                         // 781EBF..781EE7: erase all, then the head resets
    for (int i = 0; i != 18; ++i) m_fieldc0c[i] = 0; // +C0C..C50 (the 2x3x3 store loop)
    m_fieldc68.r = m_fieldc68.g = m_fieldc68.b = m_fieldc68.a = 1.0f; // +C68..C74
    m_fieldc64 = 0;                                  // +C64
    m_fieldb70 = false;                              // +B70
    m_mapOffset.x = m_mapOffset.y = 0.0f;            // +B74/+B78
    m_mapExtent.x = m_mapExtent.y = 512.0f;          // +B7C/+B80
    m_fieldb84 = 20.0f;                              // +B84
    std::vector<MapTimedMarker>().swap(m_timedMarkers); // release the (empty) storage, zero the words
    hfe::IEventListener* listener = this;            // the +3C face
    hfe::eventManager->registerEventHandler(static_cast<hfe::EventCategory>(2), listener, 0); // ECHud
    hfe::eventManager->registerEventHandler(static_cast<hfe::EventCategory>(5), listener, 0); // ECCore
    m_fieldc78 = m_fieldc7c = 0.0f;                  // +C78/+C7C
    m_vehicleDrops.clear();
}

// both handlers unregistered first, the name freed and nulled, then the
// containers and the base. Native member destruction reproduces the container part.
Bf2MenuMapNode::~Bf2MenuMapNode()
{
    hfe::IEventListener* listener = this;
    hfe::eventManager->unregisterEventHandler(static_cast<hfe::EventCategory>(2), listener);
    hfe::eventManager->unregisterEventHandler(static_cast<hfe::EventCategory>(5), listener);
    ::operator delete(m_name);                       // free and null
    m_name = nullptr;
}

unsigned int Bf2MenuMapNode::getObjectSize() { return sizeof(Bf2MenuMapNode); }
TypeDescriptor* Bf2MenuMapNode::getClassType()
{
    static Bf2MenuMapNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* Bf2MenuMapNode::getType() { return getClassType(); }
namespace {
TypeDescriptor* const registeredBf2MenuMapNodeType = Bf2MenuMapNode::getClassType();
}

void Bf2MenuMapNode::applyFilterSet(const std::bitset<43>* set) // twelve word copies
{
    for (int i = 0; i != 6; ++i) m_filterStates[i] = set[i];
}

// the +3C face. Only ECHud (2) and ECCore (5) are routed; every other category
// returns without touching the object.
void Bf2MenuMapNode::handleEvent(hfe::EventCategory category, unsigned int id, hfe::EventNode* node, void* data)
{
    if (category == static_cast<hfe::EventCategory>(2)) handleHudEvent(id, node, data);   // +144
    else if (category == static_cast<hfe::EventCategory>(5)) handleCoreEvent(id, node, data); // +148
}

// 775890 (52 B): only ECCore id 13 does anything - the offset flag/vector and the static
// mask are cleared and the extent returns to 512x512.
void Bf2MenuMapNode::handleCoreEvent(unsigned int id, hfe::EventNode*, void*)
{
    if (id != 13) return;
    m_fieldb70 = false;
    m_mapOffset.x = m_mapOffset.y = 0.0f;
    mapStatic_A1D0A8 = 0;
    m_mapExtent.x = m_mapExtent.y = 512.0f;
}

// Updates map state in response to HUD events.
void Bf2MenuMapNode::handleHudEvent(unsigned int id, hfe::EventNode*, void* data)
{
    // ds:A08F60 carries no null test, as shipped - the mini-map's arm does the same.
    // (Named localPlayer so the arms that fetch it again, as the shipped arms do, keep `local`.)
    hfe::world::IPlayer* const localPlayer = hfe::world::playerManager->getLocalHumanPlayer();

    switch (id) {
    case 8:  resetSatelliteReload(); resetUAVReload(); return;
    case 82: resetUAVReload(); return;
    case 75: hfe::hudManager->setVehicleDropAvailability(*static_cast<float*>(data)); return;
    case 42: {
        if (!data) return;
        const float* words = static_cast<const float*>(data);
        const bool active = static_cast<const unsigned char*>(data)[12] != 0;
        setAirstrikeMarker(hfe::Vec3{words[0], words[1], words[2]}, active ? 25.0f : 0.0f);
        return;
    }
    case 9:
        // The zoom the HUD remembers, straight back into the map.
        // Neither hudManager nor the adapter is null-tested, as shipped.
        setZoom(hfe::hudManager->getInformationAdapter()                // +244
                    ->getInt(std::string("MapZoom")), true);            // +18, then +D8
        return;

    case 76:
        // This is the event setHudMode posts at the end of modes 0, 2, 17
        // and 18 - the map hands itself "restore the zoom saved for whichever view
        // is now up". A collapsed map ignores it entirely.
        if (!m_expandedMap) return;                                     // +68E
        if (m_field690) { setZoom(0, false); return; }                  // +690
        if (m_commanderMode) setZoom(m_savedCommanderZoom, true);       // +691 -> +6C8
        else if (m_squadMode) setZoom(m_savedSquadZoom, true);          // +692 -> +6CC
        else setZoom(m_savedZoom, true);                                //         +6C4
        return;

    case 14: case 20: case 21: case 22: case 24: case 37: {
        // A player's role changed; if it was OUR player, re-layer the role
        // filters. `data` is not tested - it is dereferenced - and the target is
        // looked up before the identity check, so a stray id still costs a lookup.
        const int playerId = *static_cast<const int*>(data);
        hfe::world::IPlayer* const target =
            hfe::world::playerManager->getPlayerFromId(playerId);       // +20
        hfe::world::IPlayer* const local =
            hfe::world::playerManager->getLocalHumanPlayer();           // +30
        if (!local) return;
        if (playerId != local->getId()) return;                         // PE +50 = ELF +54
        applyPlayerRoleFilters(target);
        return;
    }

    case 33: {
        // The same lookup as above plus the vehicle-drop availability.
        // Three differences from that arm, all shipped: the id is RE-read from the
        // payload for the comparison rather than reused; a missing local player
        // skips only the filter step; and the +330 call happens either way.
        const int* const words = static_cast<const int*>(data);         // not tested
        hfe::world::IPlayer* const target =
            hfe::world::playerManager->getPlayerFromId(words[0]);
        hfe::world::IPlayer* const local =
            hfe::world::playerManager->getLocalHumanPlayer();
        if (local && words[0] == local->getId()) applyPlayerRoleFilters(target);
        hfe::hudManager->setVehicleDropAvailability(
            *reinterpret_cast<const float*>(&words[1]));                // +330
        return;
    }

    case 40: {
        // gameLogic's +288 is getUAVActiveTime, not the one-slot-shifted
        // neighbour: IGameLogic declares its destructor after every numbered slot,
        // so this interface carries no [PLATFORM] shift in this range. Arm 41
        // corroborates it independently - the satellite arm reaches +28C,
        // getSatelliteScanActiveTime.
        if (!hfe::gameLogic) return;
        if (!data) return;
        const float duration = hfe::gameLogic->getUAVActiveTime();      // +288
        const float* const words = static_cast<const float*>(data);
        setUAVMarker(hfe::Vec3{words[0], words[1], words[2]}, duration);
        return;
    }

    case 10:
        // The adapter's own setMapStatic (785BA0) posts this event
        // synchronously; the map reads the flag straight back out of the
        // information layer by name rather than taking it from the payload, and
        // `data` is ignored entirely.
        // neither hudManager NOR the adapter is null-tested, and the shipped
        // BF2HudManager constructor leaves +104 (the adapter) null - so this arm
        // faults on a manager whose adapter was never installed. Preserved.
        setMapStatic(hfe::hudManager->getInformationAdapter()
                         ->getBool(std::string("MapStatic")));         // +10, then 7728C0
        return;
    case 2:
        // The mode change, reached on the menu-disable event: the mode is
        // read from the HUD singleton (+1E0, 751A00) and handed to this node's own
        // +DC slot, which is a virtual dispatch - 7CF7C2 makes the identical call
        // and lands on Bf2MiniMenuMapNode's override instead.
        // hudManager is dereferenced with no null test, as shipped.
        setHudMode(hfe::hudManager->getMode());                   // +1E0, then +DC
        return;
    case 25:
        // ECHudHESquadLeaderRequest. A null payload is ignored; otherwise this->+F8
        // (772CA0) with the payload's first and third words - the squad and its request type.
        // words[1] is not read: RequestEvent never writes it (the hole its header records).
        // The mini-map's arm (7CF895) is the same.
        if (!data) return;
        setSquadRequestType(static_cast<const int*>(data)[0],
                            static_cast<const int*>(data)[2]);                  // +F8 772CA0
        return;

    case 41: {
        // Start the satellite scan only when reload time is exactly zero, initializing sweep, reload and sound.
        if (!hfe::gameLogic) return;
        const float visibleTime = hfe::gameLogic->getSatelliteScanVisibleTime();   // +29C
        const float activeTime  = hfe::gameLogic->getSatelliteScanActiveTime();    // +28C
        if (m_satelliteReloadRemaining == 0.0f) {                                  // +AA4
            m_fielda84 = activeTime;                                               // +A84
            m_satelliteActive = true;                                              // +A7D
            m_fielda8c = visibleTime;                                              // +A8C
            m_satelliteScanX = 0.0f;                                               // +A98
            m_satelliteScanY = 0.0f;                                               // +A9C
            m_satelliteReloadRemaining = hfe::gameLogic
                ? hfe::gameLogic->getSatelliteScanReloadTime() : 0.0f;             // +298
            m_fielda88 = 0.0f;                                                     // +A88
            m_satelliteReloading = true;                                           // +A7C
            hfe::g_bfSound->playSound(std::string("HUD_Satellite_scan"));          // +2C
        }
        // Runs whether or not the scan restarted - an already-running scan re-sends it.
        if (m_satelliteActive)
            hfe::hudManager->disableVOHelpMessage(29);                             // +FC
        return;
    }

    case 43: {
        // [INFERRED] Spot payload: two u16 network IDs at +0/+2, team byte at +4,
        // map position at +8/+C, and fallback vehicle type at +10.
        if (!data) return;
        const unsigned char* const bytes = static_cast<const unsigned char*>(data);
        const hfe::Vec2 position{*reinterpret_cast<const float*>(bytes + 8),
                                 *reinterpret_cast<const float*>(bytes + 0xC)};
        MapIconRecord icon;
        hfe::world::IObject* const spotted = hfe::io::networkManager->getObject(
            *reinterpret_cast<const unsigned short*>(bytes + 2));
        if (!spotted) {
            // Not ghosted here: a marker at the payload's map position, drawn with
            // the HUD's icon for the payload's vehicle type, for the spotting team only.
            if (!localPlayer) return;
            if (bytes[4] != localPlayer->getTeam()) return;                        // PE +E4
            icon.texture = hfe::hudManager->getVehicleTypeIcon(
                *reinterpret_cast<const int*>(bytes + 0x10));                      // +6C
            addTimedMarker(0, position, icon, m_fielda2c, m_fielda24, m_fielda28, 0.0f, 3);
            return;
        }
        hfe::world::IPlayerControlObject* const pco =
            static_cast<hfe::world::IPlayerControlObject*>(
                spotted->queryInterface(hfe::world::IID_IPlayerControlObject));
        if (!pco) return;
        hfe::world::IPlayer* player = pco->getOccupingPlayer();                    // +10
        if (!player) {
            // Search a copied seat map for the first occupied seat.
            std::map<int, hfe::world::IPlayerControlObject*> seats = *pco->getPcos();   // +60
            for (std::map<int, hfe::world::IPlayerControlObject*>::iterator it = seats.begin();
                 it != seats.end(); ++it) {
                player = it->second->getOccupingPlayer();                          // +10
                if (player) break;
            }
            if (!player) return;
        }
        if (player->getIsManDown()) {                                              // PE +FC
            // A man-down target: no marker - the "taunt_1" sound at the spotter.
            hfe::world::IObject* const spotter = hfe::io::networkManager->getObject(
                *reinterpret_cast<const unsigned short*>(bytes));
            if (!spotter) return;
            hfe::ISound* const sound = hfe::g_bfSound->getSound(std::string("taunt_1")); // PE +40
            if (!sound) return;
            // Play the sound at the spotter's absolute position.
            sound->playAt(spotter->getAbsolutePosition());
            return;
        }
        if (!localPlayer) return;
        if (player->getTeam() == localPlayer->getTeam()) return;                   // PE +E4
        if (bytes[4] != localPlayer->getTeam()) return;                            // asked again
        icon.texture = 0;
        addTimedMarker(*reinterpret_cast<const unsigned short*>(bytes + 2), position, icon,
                       m_fielda2c, m_fielda24, m_fielda28, 0.0f, 0);
        return;
    }

    case 46: case 47: case 48: case 49: case 50: {
        // Updates medic, ammunition and repair requests.
        if (!data) return;
        hfe::world::IObject* const object = hfe::io::networkManager->getObject(
            *static_cast<const unsigned short*>(data));
        if (!object) return;
        hfe::world::ISoldier* const soldier = static_cast<hfe::world::ISoldier*>(
            object->queryInterface(hfe::world::IID_ISoldier));
        if (!soldier) return;
        hfe::world::IPlayer* const requester = soldier->getOccupingPlayer();       // +34
        if (!requester) return;
        if (!localPlayer) return;
        if (requester->getTeam() == localPlayer->getTeam()
                && requester->getId() != localPlayer->getId()) {                   // +E4 / +50
            if (!m_field07f) {
                hfe::world::Kit* const kit = hfe::hudManager->getPlayerKit(localPlayer);   // +90
                if (!kit) return;
                hfe::world::IAbilityGLComp* const ability =
                    static_cast<hfe::world::IAbilityGLComp*>(
                        hfe::world::object_detail::LayoutAccess::abilityGLComponent(kit));   // +64
                if (!ability) return;
                if ((id == 46 || id == 49 || id == 50) && ability->getGLInfo()->hasHealingAbility) {
                    // answerable
                } else if (id == 47) {
                    if (!ability->getGLInfo()->hasAmmoAbility) return;
                } else if (id != 48 || !ability->getGLInfo()->hasRepairingAbility) {
                    return;
                }
            }
            removeTimedMarker(*static_cast<const unsigned short*>(data));
            MapIconRecord icon;
            icon.texture = getSupportRequestTexture(static_cast<int>(id));         // +110
            addTimedMarker(*static_cast<const unsigned short*>(data), hfe::Vec2{0.0f, 0.0f},
                           icon, m_fielda2c, m_fielda24, m_fielda28, 0.0f, 4);
        } else if (requester->getId() == localPlayer->getId()) {
            m_field07c = (id == 46 || id == 49 || id == 50);                      // +7C
            m_field07d = (id == 47);                                               // +7D
            m_field07e = (id == 48);                                               // +7E
        }
        return;
    }

    case 60: {
        // Add a timed squad-chat marker for the speaker, using vehicle or player imagery when available.
        // Requires a payload; an unresolved speaker still receives a textureless marker.
        const int speakerId = *static_cast<const int*>(data);
        MapIconRecord icon;                                                        // prologue
        hfe::world::IObject* const object = hfe::io::networkManager->getObject(
            static_cast<unsigned short>(speakerId));
        if (object) {
            hfe::world::ISoldier* const soldier = static_cast<hfe::world::ISoldier*>(
                object->queryInterface(hfe::world::IID_ISoldier));
            if (soldier) {
                hfe::world::IPlayer* const speaker = soldier->getOccupingPlayer(); // +34
                if (speaker) {
                    bool rotate = false;                   // retail: stack bytes the callees fill
                    bool heading = false;
                    populateVehicleMapIcon(speaker->getVehicle(), &icon, &rotate, false);   // +134
                    if (icon.texture == 0)
                        populatePlayerMapIcon(speaker, &icon, &heading, &rotate,
                                              m_showKitsOnMap, m_field3c0[7], false);       // +138
                }
            }
        }
        addTimedMarker(speakerId, hfe::Vec2{0.0f, 0.0f}, icon,
                       m_fielda2c, m_fielda24, m_fielda28, 0.0f, 1);
        return;
    }

    case 74: {
        // A vehicle drop: the HUD takes the availability (+330, arm 75's setter) and
        // the map adds the drop marker. `data` is not tested.
        const unsigned char* const bytes = static_cast<const unsigned char*>(data);
        hfe::hudManager->setVehicleDropAvailability(
            *reinterpret_cast<const float*>(bytes + 4));                           // +330
        addVehicleDrop(*reinterpret_cast<const int*>(bytes),
                       hfe::Vec3{*reinterpret_cast<const float*>(bytes + 8),
                                 *reinterpret_cast<const float*>(bytes + 0xC),
                                 *reinterpret_cast<const float*>(bytes + 0x10)});
        return;
    }

    case 77:
        // Leave overhead zoom through the squad manager for commanders, or clear the local player's camera flag.
        if (m_commanderMode) {                                                     // +691
            if (m_zoomLevel == 2) return;                                          // +6D0
            hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer();
            if (!m_overheadZoomActive) return;                                     // +7F8
            if (hfe::squadManager && player)
                static_cast<hfe::ISquadManager*>(hfe::squadManager)
                    ->setCommanderOverheadCamZoomMode(player->getId(), false);     // +80
            m_overheadZoomActive = false;
            return;
        } else {
            hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer();
            if (m_overheadZoomActive) m_overheadZoomActive = false;
            if (!player) return;
            player->setCommanderCamOverhead(false);                                // PE +C4
            return;
        }

    case 83:
        // The supply-drop marker at the payload's world position, for 25 seconds.
        if (!data) return;
        setSuppliesMarker(hfe::Vec3{static_cast<const float*>(data)[0],
                                    static_cast<const float*>(data)[1],
                                    static_cast<const float*>(data)[2]}, 25.0f);
        return;

    default:
        return;                                                   // 782A12, the shared no-op case
    }
}

// HUD mode and player-role filters.

// Applies the local player's role filter, with leader and commander rows overriding ordinary squad membership.
void Bf2MenuMapNode::applyPlayerRoleFilters(hfe::world::IPlayer* player)
{
    if (!player) return;
    if (player->getSquadId() > 0) applyFilterSet(m_filterSets[3]);   // +458; PE +108
    if (player->getIsSquadLeader()) {                                // PE +138
        applyFilterSet(m_filterSets[4]);                             // +488
    } else if (player->getIsCommander()) {                           // PE +110
        if (hfe::hudManager->getMode() == 1) return;                 // unguarded
        if (hfe::hudManager && hfe::hudManager->isCommanderShown())   // guarded here, not above
            applyFilterSet(m_filterSets[5]);                         // +4B8
        else
            applyFilterSet(m_filterSets[4]);                         // +488
    } else {
        return;
    }
    m_field674 = 2;
}

// 7778B0 (40 B, no stack frame). Kits off the map and the icon bitset back to the
// exact value the constructor gave it - all ones, masked to 0x7F000000FF.
// [INFERRED] name.
void Bf2MenuMapNode::resetMapFilterBits()
{
    m_showKitsOnMap = false;                                         // +38D
    m_field3c0.set();                                                // both words to -1...
    m_field3c0 &= std::bitset<39>(0x0000007F000000FFull);             // ...then the two and masks
}

namespace {
// Stops the satellite-scan sound through the shared sound facade.
void stopSatelliteScanSound()
{
    hfe::g_bfSound->stopSound(std::string("HUD_Satellite_scan"));
}
}

void Bf2MenuMapNode::setHudMode(int mode)
{
    // The shared prologue: every mode, including the ones the switch
    // ignores, gets the row-2 filters, m_field674 = 1, the UAV state cleared and
    // the local player's role filters on top.
    // ds:A08F60 is dereferenced with no null test, as shipped.
    hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer();
    applyFilterSet(m_filterSets[2]);
    m_field674 = 1;
    m_uavState = false;                                  // +AE8
    applyPlayerRoleFilters(player);

    switch (mode) {
    case 0:
        m_field690 = false; m_commanderMode = false; m_squadMode = false;
        m_field06c = false;
        setExpanded(false);
        m_field708[1] = 0.0f;                            // +70C
        resetMapFilterBits();
        m_field780 = m_field788;
        m_field778 = m_field798;
        m_satelliteActive = false;                       // +A7D
        m_fielda8c = 0;
        m_satelliteScanX = 0.0f; m_satelliteScanY = 0.0f;
        stopSatelliteScanSound();
        applyMenuMapAlphas();
        clearOverheadCamera();
        hfe::eventManager->postEvent(static_cast<hfe::EventCategory>(2), 76u, nullptr, 0.0f);
        break;

    case 1:
        resetMapFilterBits();
        m_field780 = m_field7c0;
        m_field778 = m_textureSize;
        m_field690 = true; m_commanderMode = false; m_squadMode = false;
        setExpanded(true);
        applyFilterSet(m_filterSets[0]);                 // +3C8
        m_field06c = true;
        m_field674 = 1;
        m_field708[1] = 1.0f;
        m_field704 = 0.5f;
        m_field718 = 1.0f; m_field700 = 1.0f;
        m_satelliteActive = false;
        m_fielda8c = 0;
        m_satelliteScanX = 0.0f; m_satelliteScanY = 0.0f;
        stopSatelliteScanSound();
        // mode 1 is the only one of 0/1/2/17/18 that does not post ECHud 76.
        clearOverheadCamera();
        break;

    case 2:
        resetMapFilterBits();
        m_field780 = m_field7c0;
        m_field778 = m_textureSize;
        m_field690 = false; m_commanderMode = false; m_squadMode = false;
        m_field06c = false;
        setExpanded(true);
        m_field708[1] = 0.0f;
        applyMenuMapAlphas();
        m_satelliteActive = false;
        m_fielda8c = 0;
        m_satelliteScanX = 0.0f; m_satelliteScanY = 0.0f;
        stopSatelliteScanSound();
        hfe::eventManager->postEvent(static_cast<hfe::EventCategory>(2), 76u, nullptr, 0.0f);
        clearOverheadCamera();
        break;

    case 15:
        if (m_commanderMode) { m_field06c = true; break; }   // already commander: 77838D
        m_field690 = false; m_commanderMode = true; m_squadMode = false;
        m_field718 = 1.0f; m_field700 = 1.0f;
        setExpanded(true);
        m_field780 = m_field7a8;
        m_field778 = m_field7b0;
        m_field674 = 2;
        m_field708[1] = 0.0f;
        m_field06c = true;
        break;

    case 17:
        if (hfe::hudManager->getPreviousMode() == 20) { m_field06c = true; break; } // +1E4
        resetMapFilterBits();
        m_field780 = m_field7c0;
        m_field778 = m_textureSize;
        m_field690 = false; m_commanderMode = false; m_squadMode = true;
        setExpanded(true);
        m_field674 = 2;                                  // mode 18 does not do this
        applySquadViewTail(17);                           // the shared 7782F5 tail
        break;

    case 18:
        if (hfe::hudManager->getPreviousMode() == 19) { m_field06c = true; break; }
        resetMapFilterBits();
        m_field780 = m_field7c0;
        m_field778 = m_textureSize;
        m_field690 = false; m_commanderMode = false; m_squadMode = true;
        setExpanded(true);
        applySquadViewTail(18);
        break;

    case 22: case 24: case 25:                           // straight to 77838D
        m_field06c = true;
        break;

    case 23:
        m_uavState = true;                               // +AE8
        m_field06c = true;
        break;

    case 9: case 12: case 13: case 14: case 16:
    case 19: case 20: case 21: case 26: case 27: case 28:
        m_field06c = false;
        break;

    default:                                             // modes 3..8, 10, 11 and out of range
        break;
    }

    // 7783A3, the tail every path falls through to. It subtracts filter row 1 from
    // the live states - but only when row 1 is already wholly contained in state 0.
    if (m_field36c) return;                              // +36C
    if (m_filterStates[0] != (m_filterSets[1][0] | m_filterStates[0])) return; // 777190 then 7758D0
    for (int i = 0; i != 6; ++i)
        m_filterStates[i] &= ~m_filterSets[1][i];         // 777110 then 777150
}

// The adapter-alpha block modes 0 and 2 share (777EB0 / 7780BE). The manager and
// the adapter are both tested here - unlike the unguarded reads elsewhere in this
// body - and the same accessor is called twice per value, exactly as shipped.
void Bf2MenuMapNode::applyMenuMapAlphas()
{
    if (!hfe::hudManager || !hfe::hudManager->getInformationAdapter()) return;   // +244
    m_field704 = hfe::hudManager->getInformationAdapter()
                     ->getFloat(std::string("MenuMapIconAlpha"));                 // +24
    const float alpha = hfe::hudManager->getInformationAdapter()
                            ->getFloat(std::string("MenuMapAlpha"));
    m_field718 = alpha;
    m_field700 = alpha;                                  // FST then fstp: one value, two members
}

// the overhead-camera reset modes 0, 1, 2, 17 and 18 end
// with. The redundant `if (flag) flag = false;` is shipped; so is the second,
// separate getLocalHumanPlayer call - the prologue's answer is not reused.
void Bf2MenuMapNode::clearOverheadCamera()
{
    hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer();
    if (m_overheadZoomActive) m_overheadZoomActive = false;           // +7F8
    if (player) player->setCommanderCamOverhead(false);               // PE +C4 = ELF +C8
}

// 7782F5, the tail modes 17 and 18 share. 0.9f on both map alphas, the satellite
// state cleared, the scan sound stopped and ECHud 76 posted.
void Bf2MenuMapNode::applySquadViewTail(int mode)
{
    m_satelliteScanY = 0.0f; m_satelliteScanX = 0.0f;
    m_fielda8c = 0;
    m_satelliteActive = false;
    m_field700 = 0.9f; m_field718 = 0.9f;
    m_field704 = 1.0f;
    m_field708[1] = 0.0f;
    stopSatelliteScanSound();                       // A23DC0 +34, in both the 17 and the 18 arm
    hfe::eventManager->postEvent(static_cast<hfe::EventCategory>(2), 76u, nullptr, 0.0f);
    clearOverheadCamera();
    m_field06c = true;
}

// 7737C0 (169 B), the +11C slot. The pair is biased by half a unit - x UP, y down -
// and then clamped into the four bounds updateViewBounds maintains.
void Bf2MenuMapNode::setViewCenter(hfe::Vec2 position)
{
    m_viewCenter.x = position.x + 0.5f;
    m_viewCenter.y = position.y - 0.5f;                  // the same constant, subtracted
    if (m_viewCenter.x > m_maxCenterX) m_viewCenter.x = m_maxCenterX;        // +A6C
    else if (m_viewCenter.x < m_minCenterX) m_viewCenter.x = m_minCenterX;   // +A68
    if (m_viewCenter.y > m_maxCenterY) m_viewCenter.y = m_maxCenterY;        // +A64
    else if (m_viewCenter.y < m_minCenterY) m_viewCenter.y = m_minCenterY;   // +A60
}

// 777980 (24 B), the +D8 slot: two tail-jumps and nothing else. The expanded arm
// tail-jumps to applyZoom, so retail forwards its int return through this slot; no
// caller in the image reads it, and the other arm produces nothing, so the slot is
// spelled void here.
void Bf2MenuMapNode::setZoom(int level, bool remember)
{
    if (m_expandedMap) applyZoom(level, remember);       // +68E, 776670
    else setCommanderZoom(level, remember);
}

// 772810 (92 B). It re-derives the local player and the vehicle type itself
// rather than taking them from setExpanded, which has both in hand one frame up.
void Bf2MenuMapNode::setCommanderZoom(int level, bool remember)
{
    updateViewBounds();                                  // +120
    recenterOnPlayer();                                  // +118
    m_zoomLevel = level;                                 // +6D0
    if (!remember) return;
    const int type = hfe::hudManager->getPlayerVehicleType(
        hfe::world::playerManager->getLocalHumanPlayer());                   // both unguarded
    if (type >= 0 && type < 40) savedZoomSlot(type) = level;
}

// Returns an unchecked reference into the ten-entry zoom table; callers may pass larger indices.
int& Bf2MenuMapNode::savedZoomSlot(int type)
{
    return reinterpret_cast<int*>(m_field69c)[type];
}

// Centers the map on the local player's vehicle.
void Bf2MenuMapNode::recenterOnPlayer()
{
    hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    if (!player->getVehicle()) return;                   // PE +44 = ELF +48
    const hfe::Mat4& transform = player->getVehicle()->getAbsoluteTransformation();
    setViewCenter(hfe::Vec2{ transform.m[12] * m_worldToMapScale,      // +84
                            -transform.m[14] * m_worldToMapScale });   // +11C
}

// Returns the extent of the current map view.
hfe::Vec2 Bf2MenuMapNode::mapViewExtent()
{
    const hfe::Vec2 zoom = getZoomScale(true);           // 773900, the selected level
    const float sizeY = getSize().y;                     // asked twice, as shipped
    const float sizeX = getSize().x;

    const float spanX = m_mapExtent.x / m_textureSize.x * zoom.x * sizeX;   // +B7C / +7B8
    // zoom.X again, not zoom.y: the Y span is scaled by the X zoom.
    const float spanY = m_mapExtent.y / m_textureSize.y * zoom.x * sizeY;   // +B80 / +7BC

    const float unitX = spanX / (zoom.x * sizeX);
    const float unitY = spanY / (zoom.y * sizeY);        // zoom.y here, though spanY used zoom.x
    const float halfX = sizeX / spanX * 0.5f;
    const float halfY = sizeY / spanY * -0.5f;

    hfe::Vec2 extent;
    extent.x = 0.5f - (halfX - 1.0f + 0.5f) * unitX;     // the -1.0 then +0.5 is two roundings
    extent.y = -0.5f - (halfY + 1.0f - 0.5f) * unitY;
    return extent;
}

// 7769C0 (141 B), the +120 slot. The four bounds setViewCenter clamps into.
void Bf2MenuMapNode::updateViewBounds()
{
    if (!m_expandedMap) {                                // +68E
        m_minCenterY = -1.0f;                            // +A60
        m_maxCenterY = 0.0f;                             // +A64
        m_minCenterX = 0.0f;                             // +A68
        m_maxCenterX = 1.0f;                             // +A6C
        return;
    }
    const hfe::Vec2 centre = getMapCenterOffset();
    const hfe::Vec2 extent = mapViewExtent();
    // Read exactly as shipped, and it is lopsided: three of the four bounds are
    // built from extent.y and only m_maxCenterX uses extent.x. That falls out of
    // extent.x being the positive half and extent.y the negative one.
    m_minCenterY = centre.y + extent.y;
    m_maxCenterY = centre.y - (extent.y + 1.0f);
    m_maxCenterX = centre.x + extent.x;
    m_minCenterX = centre.x + extent.y + 1.0f;
}

// Expands around the map center or restores the local vehicle's saved zoom when collapsed.
// Collapsing requires the HUD manager and a local player.
void Bf2MenuMapNode::setExpanded(bool expanded)
{
    if (expanded) {
        setViewCenter(getMapCenterOffset());             // +11C, 773A20
        m_expandedMap = expanded;                        // +68E, stored after the call
        // Neither flag set: the zoom is skipped entirely and only +774 is cleared.
        if (!m_commanderMode && !m_field690) { m_field774 = false; return; }
        setZoom(0, false);                               // +D8
    } else {
        m_expandedMap = false;
        setViewCenter(hfe::Vec2{0.0f, 0.0f});
        // The same 0..40 walk 772810 does on the way in - see savedZoomSlot.
        const int type = hfe::hudManager->getPlayerVehicleType(
            hfe::world::playerManager->getLocalHumanPlayer());        // +74, both unguarded
        if (type >= 0 && type < 40) setZoom(savedZoomSlot(type), true);
        else                        setZoom(0, true);
    }
    m_field774 = false;                                  // +774
}

bool Bf2MenuMapNode::updateSelf(System* system, const NodeState*, const Rect* bounds, float)
{
    // 772580 (134 B). The map only claims the pointer while its +6C flag is
    // set and the pointer is strictly inside its current bounds. The strict
    // positive gate also preserves retail's unordered/NaN refusal.
    if (!m_field06c) return true;

    const float* const pointer = system->getPointerPosition();
    const float x = pointer[0];
    const float y = pointer[1];
    const float left = bounds->x;
    const float top = bounds->y;
    if (x > left && x < left + bounds->width
            && y > top && y < top + bounds->height)
        system->setPointerOwner(this);
    return true;
}

// 776FD0, virtual +104. The public bitset operation preserves retail's range
// diagnostic for a type >= 43 rather than silently indexing past the set.
bool Bf2MenuMapNode::isMapFilterEnabled(unsigned int type)
{
    int zoom = m_zoomLevel;
    if (zoom < 0) zoom = 0;
    else if (zoom > 2) zoom = 2;
    return m_filterStates[(m_expandedMap ? 3 : 0) + zoom].test(type);
}

// 777030, virtual +108. This is a separate filter row from +104: row six
// controls icon-size interpolation for the current compact/expanded zoom.
bool Bf2MenuMapNode::shouldScaleMapIcon(unsigned int type)
{
    if (!m_field7f9) return false;
    int zoom = m_zoomLevel;
    if (zoom < 0) zoom = 0;
    else if (zoom > 2) zoom = 2;
    return m_filterSets[6][(m_expandedMap ? 3 : 0) + zoom].test(type);
}

// 772C60, virtual +10C.
bool Bf2MenuMapNode::shouldUseSecondaryIconTexture()
{
    return (!m_field68d && !m_field68c && !m_expandedMap)
        || m_field694 < m_field67c;
}

// No level clamp and no HUD-manager guard exist in retail.
float Bf2MenuMapNode::getZoomExponent(int level)
{
    if (hfe::hudManager->isCommanderShown())
        return m_zoomExponents[2][level];
    return m_zoomExponents[m_expandedMap ? 1 : 0][level];
}

// Both axes deliberately repeat getZoomExponent (and therefore the
// HUD callback). The negated comparisons preserve the shipped x87 unordered
// behaviour: a NaN exponent or scaled size takes the write arm.
void Bf2MenuMapNode::addMapIcon(unsigned int type, MapIconRecord& icon)
{
    if (!isMapFilterEnabled(type)) return;
    if (shouldScaleMapIcon(type)) {
        if (m_iconScales[type].x >= 0.0f
                && (!(getZoomExponent(m_zoomLevel) > m_currentZoomExponent)
                    || !(m_currentZoomExponent * m_iconScales[type].x
                            * icon.size.x < icon.size.x)))
            icon.size.x = m_currentZoomExponent * m_iconScales[type].x
                * icon.size.x;

        if (m_iconScales[type].y >= 0.0f
                && (!(getZoomExponent(m_zoomLevel) > m_currentZoomExponent)
                    || !(m_currentZoomExponent * m_iconScales[type].y
                            * icon.size.y < icon.size.y)))
            icon.size.y = m_currentZoomExponent * m_iconScales[type].y
                * icon.size.y;
    }
    m_icons[type].push_back(icon);
}

// The five-float Rect at +B5C is the adjusted map viewport.
void Bf2MenuMapNode::screenCoordinatesToMap(const hfe::Vec2& input,
                                            hfe::Vec2& output)
{
    output.x = (input.x - (m_fieldb5c.width * 0.5f + m_fieldb5c.x))
        / m_fieldb5c.width + 1.0f;
    output.y = (input.y - (m_fieldb5c.height * 0.5f + m_fieldb5c.y))
        / m_fieldb5c.height - 1.0f;
}

// The by-value Vec2 is present in the shipped call shape but unused.
void Bf2MenuMapNode::clampProjectedPosition(hfe::Vec2, MapIconRecord* icon)
{
    if (!icon) return;
    const float inset = m_field72c / getZoomScale(false).y;
    const auto retailClamp = [](float value, float minimum, float maximum) {
        if (value >= maximum) value = maximum;
        if (value < minimum) value = minimum;
        return value;
    };
    icon->position.y = retailClamp(icon->position.y,
                                   inset - 1.5f, -0.5f - inset);
    icon->position.x = retailClamp(icon->position.x,
                                   inset + 0.5f, 1.5f - inset);
}

// The final byte test intentionally reads MapIconRecord's +28 byte;
// its default constructor leaves that byte untouched, matching retail.
void Bf2MenuMapNode::projectWorldPosition(hfe::Vec3 position,
                                          MapIconRecord* output)
{
    if (!output) return;
    if (!m_overheadZoomActive) {
        output->position.x = ((1.0f / m_worldToMapScale) * 0.5f + position.x)
            * m_worldToMapScale + 0.5f;
        output->position.y = -(((1.0f / m_field088) * 0.5f + position.z)
            * m_field088 + 0.5f);
    } else {
        if (position.y == 0.0f)
            position = mapCoordinatesToGround(hfe::Vec2{position.x, position.z}, false);
        const hfe::Vec3 screen = hfe::hudManager->worldToScreen(position);
        screenCoordinatesToMap(hfe::Vec2{screen.x, screen.y}, output->position);
    }
    if (output->clampToVisibleMap)
        clampProjectedPosition(m_field748, output);
}

// This overload performs the same projection as 776AF0 but writes
// only the coordinates and therefore has no MapIconRecord clamp flag to test.
void Bf2MenuMapNode::projectWorldPosition(hfe::Vec3 position,
                                          hfe::Vec2* output)
{
    if (!output) return;
    if (!m_overheadZoomActive) {
        output->x = ((1.0f / m_worldToMapScale) * 0.5f + position.x)
            * m_worldToMapScale + 0.5f;
        output->y = -(((1.0f / m_field088) * 0.5f + position.z)
            * m_field088 + 0.5f);
    } else {
        if (position.y == 0.0f)
            position = mapCoordinatesToGround(hfe::Vec2{position.x, position.z}, false);
        const hfe::Vec3 screen = hfe::hudManager->worldToScreen(position);
        screenCoordinatesToMap(hfe::Vec2{screen.x, screen.y}, *output);
    }
}

// [CHOSEN] extraction of the -1/-1 world-position arm of 7751D0 -> 774240.
// It is the only shape 77EF50 calls. The full helper's network-id arm remains
// outside this method rather than being guessed through an untyped manager.
void Bf2MenuMapNode::makeWorldSupplyLineRecord(MapIconRecord& output,
                                               const Rect* bounds,
                                               const hfe::Vec3& source,
                                               const hfe::Vec3& target)
{
    hfe::Vec2 start{0.0f, 0.0f};
    hfe::Vec2 end{0.0f, 0.0f};
    worldCoordinatesToMap(source, start);
    worldCoordinatesToMap(target, end);

    hfe::Vec2 midpoint{0.0f, 0.0f};
    float length = 0.0f;
    float rotation = 0.0f;
    const float epsilon = (std::numeric_limits<float>::epsilon)();
    const bool valid = !(std::fabs(start.y) + std::fabs(start.x) <= epsilon)
        && !(std::fabs(end.y) + std::fabs(end.x) <= epsilon);
    if (valid) {
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        double angle = static_cast<double>(-1.5707964f)
            - std::atan2(static_cast<double>(dy), static_cast<double>(dx));
        if (angle + static_cast<double>(3.1415927f) < 0.0)
            angle += static_cast<double>(6.2831855f);
        rotation = static_cast<float>(angle);
        length = static_cast<float>(std::sqrt(
            static_cast<double>(dx) * dx + static_cast<double>(dy) * dy))
            * getZoomScale(false).y;
        midpoint.x = (end.x + start.x) * 0.5f;
        midpoint.y = (end.y + start.y) * 0.5f;
    }

    output.position = midpoint;
    output.color = Color{1.0f, 1.0f, 1.0f, 1.0f};
    output.size.x = static_cast<float>(m_field678);
    output.size.y = length * bounds->width;
    output.rotation = rotation + m_field76c;
    output.texture = 0;
}

// Bytes 8 and 17, plus every byte after 18, are deliberately untouched.
void Bf2MenuMapNode::updateFilterActiveFlags()
{
    for (unsigned int bit = 0; bit != 8; ++bit)
        m_mapFilterActive[bit] = m_field3c0.test(bit);

    int lowCount = 0;
    for (unsigned int bit = 0; bit != 32; ++bit)
        if (m_field3c0.test(bit)) ++lowCount;
    m_mapFilterActive[9] = lowCount == 8;

    for (unsigned int bit = 0; bit != 7; ++bit)
        m_mapFilterActive[10 + bit] = m_field3c0.test(32 + bit);

    int highCount = 0;
    for (unsigned int bit = 32; bit != 39; ++bit)
        if (m_field3c0.test(bit)) ++highCount;
    m_mapFilterActive[18] = highCount == 7;
}

// No HUD-manager guard exists in the shipped body.
void Bf2MenuMapNode::updatePaintCenter()
{
    if (hfe::hudManager->getMode() == 2) {
        m_field750 = m_field748;
    } else {
        const float inverse = 1.0f - m_field694;
        m_field750.x = inverse * m_field748.x + m_field694 * m_field7e8.x;
        m_field750.y = inverse * m_field748.y + m_field694 * m_field7e8.y;
    }
}

hfe::geom::IMemeTexture* Bf2MenuMapNode::combatAreaTexture() const
{
    // System::loadTexture's historical interface spells every renderer handle
    // `int`; this particular handle is proven to be an IMemeTexture object.
    return reinterpret_cast<hfe::geom::IMemeTexture*>(
        static_cast<std::uintptr_t>(static_cast<unsigned int>(m_field93c[2])));
}

// 774A60. [INFERRED] void return; the caller discards the incidental draw result.
void Bf2MenuMapNode::drawMapTexture(int texture, const Rect* adjusted,
                                    const Rect* bounds, System* system,
                                    RenderContext* context)
{
    const Color color = {1.0f, 1.0f, 1.0f, m_field708[2]};
    Quad quad(adjusted, bounds, &color);
    const float center[2] = {
        bounds->x + bounds->width * 0.5f,
        bounds->y + bounds->height * 0.5f
    };
    rotateQuad(&quad, center, m_field76c);
    if (shouldUseSecondaryIconTexture()) quad.texture2 = m_field928[1];
    drawContextTexture(context, system, texture, &quad);
}

// The adjusted Rect argument is genuinely unused: retail constructs
// the tint Quad with `bounds` as both its geometry and clipping rectangle.
void Bf2MenuMapNode::drawMapTint(int texture, const Color* color, const Rect*,
                                 const Rect* bounds, System* system,
                                 RenderContext* context)
{
    Quad quad(bounds, bounds, color);
    const float center[2] = {
        bounds->x + bounds->width * 0.5f,
        bounds->y + bounds->height * 0.5f
    };
    rotateQuad(&quad, center, m_field76c);
    quad.state08 = m_fieldb44;
    quad.state0c = m_fieldb48;
    if (shouldUseSecondaryIconTexture()) quad.texture2 = m_field928[1];
    drawContextTexture(context, system, texture, &quad);
}

// The two-call protocol is intentional: measurement does not publish
// the new mask; the draw pass recomputes it, publishes it, then rasterizes.
bool Bf2MenuMapNode::updateCombatAreaTexture(bool measureOnly)
{
    hfe::CombatAreaManager* const manager = hfe::CombatAreaManager::getInstance();
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
    if (!manager) return false;

    std::uint32_t activeMask = 0;
    unsigned int areaIndex = 0;
    for (std::vector<hfe::CombatArea*>::const_iterator it = manager->m_areas.begin();
         it != manager->m_areas.end(); ++it, ++areaIndex) {
        if ((*it)->checkIfActiveToPlayer(local))
            activeMask |= 1u << (areaIndex & 31u); // x86 masks the retail shift count
    }
    if (activeMask == mapStatic_A1D0A8) return false;

    hfe::geom::IMemeTexture* texture = nullptr;
    if (measureOnly) {
        m_mapOffset = m_textureSize;
        m_mapExtent.x = m_mapExtent.y = 0.0f;
    } else {
        texture = combatAreaTexture();
        mapStatic_A1D0A8 = activeMask;
        texture->beginShape(0);
        texture->applyCanvas(0x500);
    }

    for (std::vector<hfe::CombatArea*>::const_iterator it = manager->m_areas.begin();
         it != manager->m_areas.end(); ++it) {
        hfe::CombatArea* const area = *it;
        if (area->m_vehicleCategory == 0 || area->m_vehicleCategory == 4)
            continue;
        if (!area->checkIfActiveToPlayer(local) || area->m_points.empty())
            continue;
        rasterizeCombatArea(texture, area->m_points, 128,
                            m_mapOffset, m_mapExtent, measureOnly);
        if (!measureOnly) {
            texture->applyCanvas(0x402);
            texture->beginShape(0);
        }
    }

    for (std::vector<hfe::CombatArea*>::const_iterator it = manager->m_areas.begin();
         it != manager->m_areas.end(); ++it) {
        hfe::CombatArea* const area = *it;
        if (area->m_vehicleCategory != 0 && area->m_vehicleCategory != 4)
            continue;
        if (area->m_points.empty()) continue;
        rasterizeCombatArea(texture, area->m_points, 255,
                            m_mapOffset, m_mapExtent, measureOnly);
        if (!measureOnly) {
            texture->applyCanvas(0x402);
            texture->beginShape(0);
        }
    }

    if (!measureOnly) {
        texture->applyCanvas(0x703);
        m_fieldb70 = true;
        return true;
    }

    m_mapExtent.x -= m_mapOffset.x;
    m_mapExtent.y -= m_mapOffset.y;
    float padding = m_fieldb84;
    const float farX = m_mapExtent.x + m_mapOffset.x;
    if (farX + padding > m_textureSize.x)
        padding = (farX - m_textureSize.x) * 0.5f;
    const float farY = m_mapExtent.y + m_mapOffset.y;
    if (farY + padding > m_textureSize.y)
        padding = (farY - m_textureSize.y) * 0.5f;

    const float doubledPadding = padding + padding;
    m_mapExtent.x += doubledPadding;
    m_mapExtent.y += doubledPadding;
    m_mapOffset.x -= padding;
    m_mapOffset.y -= padding;
    if (m_mapOffset.x < 0.0f) m_mapOffset.x = 0.0f;
    if (m_mapOffset.y < 0.0f) m_mapOffset.y = 0.0f;

    if (m_mapExtent.x <= m_mapExtent.y) {
        float difference = m_mapExtent.y - m_mapExtent.x;
        const float square = m_mapExtent.y;
        m_mapExtent.x = square;
        if (m_mapOffset.x - difference * 0.5f + square > m_textureSize.x)
            difference = (square + m_mapOffset.x - m_textureSize.x) * 2.0f;
        m_mapOffset.x -= difference * 0.5f;
    } else {
        float difference = m_mapExtent.x - m_mapExtent.y;
        const float square = m_mapExtent.x;
        m_mapExtent.y = square;
        if (m_mapOffset.y - difference * 0.5f + square > m_textureSize.y)
            difference = (square + m_mapOffset.y - m_textureSize.y) * 2.0f;
        m_mapOffset.y -= difference * 0.5f;
    }
    return true;
}

// 774EB0. [INFERRED] void return.
void Bf2MenuMapNode::drawCombatAreaTexture(int texture, const Rect* adjusted,
                                           const Rect* bounds, System* system,
                                           RenderContext* context)
{
    const Color color = {1.0f, 1.0f, 1.0f, m_field708[3] * m_field708[2]};
    const hfe::Vec2 centerOffset = getMapCenterOffset();
    const float ratioY = getSize().y / m_textureSize.y;
    const float ratioX = getSize().x / m_textureSize.x;
    const hfe::Vec2 firstZoom = getZoomScale(false);
    const hfe::Vec2 secondZoom = getZoomScale(false);
    const float width = m_mapExtent.x * secondZoom.x * ratioX;
    const float height = m_mapExtent.y * firstZoom.y * ratioY;
    const Rect destination = {
        adjusted->x + (adjusted->width - width) * 0.5f
            + centerOffset.x * adjusted->width,
        adjusted->y + (adjusted->height - height) * 0.5f
            + centerOffset.y * adjusted->height,
        width, height, 0.0f
    };
    Quad quad(&destination, bounds, &color);
    const float center[2] = {
        bounds->x + bounds->width * 0.5f,
        bounds->y + bounds->height * 0.5f
    };
    rotateQuad(&quad, center, m_field76c);
    if (shouldUseSecondaryIconTexture()) quad.texture2 = m_field928[1];
    drawContextTexture(context, system, texture, &quad);
}

// Four submissions in top/right/bottom/left order, all reusing one
// default Quad exactly as the PE does.
void Bf2MenuMapNode::drawMapBorder(const Rect* adjusted, const Rect* bounds,
                                   System* system, RenderContext* context)
{
    if ((!m_field68d && !m_field68c && !m_expandedMap)
            || m_field694 < m_field67c
            || hfe::hudManager->isCommanderShown())
        return;

    const Color color = {0.482f, 0.474f, 0.388f, m_field708[2]};
    Quad quad;
    const auto setBounds = [&](float left, float top, float right, float bottom) {
        quad.clip = adjusted;
        quad.corners[0].x = left;  quad.corners[0].y = top;    quad.corners[0].z = 0.0f;
        quad.corners[1].x = right; quad.corners[1].y = top;    quad.corners[1].z = 0.0f;
        quad.corners[2].x = right; quad.corners[2].y = bottom; quad.corners[2].z = 0.0f;
        quad.corners[3].x = left;  quad.corners[3].y = bottom; quad.corners[3].z = 0.0f;
        colorQuad(&quad, &color);
    };
    const float left = bounds->x;
    const float top = bounds->y;
    const float right = left + bounds->width;
    const float bottom = top + bounds->height;
    setBounds(left, top, right, top + 4.0f);
    drawContextTexture(context, system, 0, &quad);
    setBounds(right - 4.0f, top + 4.0f, right, bottom);
    drawContextTexture(context, system, 0, &quad);
    setBounds(left + 4.0f, bottom - 4.0f, right - 4.0f, bottom);
    drawContextTexture(context, system, 0, &quad);
    setBounds(left, top + 4.0f, left + 4.0f, bottom);
    drawContextTexture(context, system, 0, &quad);
}

// Capacity survives, just as in retail's resize(0, filler) loop.
void Bf2MenuMapNode::clearMapIcons()
{
    for (int category = 0; category != 43; ++category) m_icons[category].clear();
}

// 7746E0, virtual +124. [INFERRED] void return/name.
void Bf2MenuMapNode::paintMapIcon(hfe::Vec2 position, float rotation,
                                  hfe::Vec2 size, unsigned int texture, System* system,
                                  const Rect* adjusted, const Rect* bounds,
                                  const Color* color, RenderContext* context)
{
    // [CHOSEN, not recovered]
    if (texture == 0) return;
    position.x -= static_cast<float>(std::floor(position.x + 0.5f));
    position.y -= static_cast<float>(std::floor(position.y + 0.5f));
    const float left = adjusted->x + adjusted->width * 0.5f
        - size.x * 0.5f + position.x * adjusted->width;
    const float top = adjusted->y + adjusted->height * 0.5f
        - size.y * 0.5f + position.y * adjusted->height;
    const Rect destination = {
        static_cast<float>(std::floor(left)) - 0.5f,
        static_cast<float>(std::floor(top)) - 0.5f,
        static_cast<float>(std::ceil(size.x)),
        static_cast<float>(std::ceil(size.y)),
        0.0f
    };
    Quad quad(&destination, bounds);
    const float iconCenter[2] = {
        destination.x + destination.width * 0.5f,
        destination.y + destination.height * 0.5f
    };
    rotateQuad(&quad, iconCenter, rotation - m_field76c);
    const float boundsCenter[2] = {
        bounds->x + bounds->width * 0.5f,
        bounds->y + bounds->height * 0.5f
    };
    rotateQuad(&quad, boundsCenter, m_field76c);
    colorQuad(&quad, color);
    if (shouldUseSecondaryIconTexture()) quad.texture2 = m_field928[1];
    drawContextTexture(context, system, texture, &quad);
}

// 778520, virtual +128. The local typed copy is observable in the PE and
// intentionally invokes MapIconRecord's recovered copy constructor.
void Bf2MenuMapNode::paintMapIcons(System* system, const Rect* adjusted,
                                   const Rect* bounds, RenderContext* context)
{
    for (int category = 0; category != 43; ++category) {
        for (std::vector<MapIconRecord>::const_iterator it = m_icons[category].begin();
             it != m_icons[category].end(); ++it) {
            const MapIconRecord icon(*it);
            paintMapIcon(icon.position, icon.rotation, icon.size, icon.texture,
                         system, adjusted, bounds, &icon.color, context);
        }
    }
}

// 784A80, all 1,947 bytes. Every short-circuit and repeated virtual query is
// retained; in particular, filter results are never cached across phases.
void Bf2MenuMapNode::paintSelf(System* system, const NodeState* state,
                               const Rect* bounds, const Rect* clip,
                               RenderContext* context)
{
    int defaultWorldZ = 0;
    int defaultWorldX = 0;
    {
        const std::string worldZName("GLSWorldSizeZ");
        const std::string worldXName("GLSWorldSizeX");
        const int worldZ = hfe::settings->intGet(worldZName, defaultWorldZ);
        const int worldX = hfe::settings->intGet(worldXName, defaultWorldX);
        m_field088 = 1.0f / static_cast<float>(worldZ);
        m_worldToMapScale = 1.0f / static_cast<float>(worldX);
    }

    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
    if (!local) return;
    if (local->getVehicle())
        local->getVehicle()->queryInterface(hfe::world::IID_IPlayerControlObject);

    updateFilterActiveFlags();
    updatePaintCenter();
    m_fieldb5c = *bounds;
    const hfe::Vec2 zoom = getZoomScale(false);
    m_fieldb5c.height *= zoom.x; // crossed axes are shipped
    m_fieldb5c.width *= zoom.y;
    m_fieldb5c.x -= (m_fieldb5c.width - bounds->width) * 0.5f;
    m_fieldb5c.y -= (m_fieldb5c.height - bounds->height) * 0.5f;
    m_fieldb5c.x -= (m_field750.x - 0.5f) * m_fieldb5c.width;
    m_fieldb5c.y -= (m_field750.y + 0.5f) * m_fieldb5c.height;

    const float ratioX = bounds->width / m_textureSize.x;
    const float ratioY = bounds->height / m_textureSize.y;
    const float leftCrop = zoom.x * m_mapOffset.x * ratioX;
    const float topCrop = ratioY * m_mapOffset.y * zoom.y;
    const float rightCrop = zoom.x * m_mapExtent.x * ratioX;
    const float bottomCrop = ratioY * m_mapExtent.y * zoom.y;
    if (m_fieldb5c.x + leftCrop > bounds->x)
        m_fieldb5c.x = bounds->x - leftCrop;
    if (m_fieldb5c.width < bounds->width)
        m_fieldb5c.width = bounds->width;
    if (m_fieldb5c.y + topCrop > bounds->y)
        m_fieldb5c.y = bounds->y - topCrop;
    if (m_fieldb5c.height < bounds->height)
        m_fieldb5c.height = bounds->height;
    if (m_fieldb5c.x + leftCrop + rightCrop < bounds->x + bounds->width)
        m_fieldb5c.x = bounds->x + bounds->width - (leftCrop + rightCrop);
    if (m_fieldb5c.y + topCrop + bottomCrop < bounds->y + bounds->height)
        m_fieldb5c.y = bounds->y + bounds->height - (topCrop + bottomCrop);

    // PE 784D0A calls IGame +F0 (game mode); +EC is the map name.
    const std::string gameMode(hfe::game->getCurrentMapGameMode());
    if (m_field708[2] > 0.01f && isMapFilterEnabled(0))
        drawMapTexture(m_minimapTexture, &m_fieldb5c, bounds, system, context);
    if (isMapFilterEnabled(2))
        drawMapTint(0, &m_fieldb4c, &m_fieldb5c, bounds, system, context);
    if (m_field708[2] > 0.01f && isMapFilterEnabled(3)
            && gameMode.find("sl2", 0, 3) != std::string::npos) {
        updateSupplyLine(system);
        drawSupplyLine(&m_fieldb5c, bounds, system, context);
    }
    if (updateCombatAreaTexture(true)) {
        updateCombatAreaTexture(false);
        setExpanded(m_expandedMap);
    }
    if (m_field708[2] > 0.01f && isMapFilterEnabled(1)
            && m_fieldb70 && m_field93c[2])
        drawCombatAreaTexture(m_field93c[2], &m_fieldb5c, bounds, system, context);

    drawMapBorder(&m_fieldb5c, bounds, system, context);
    clearMapIcons();

    if (m_field708[2] > 0.01f && isMapFilterEnabled(4)
            && gameMode.find("sl", 0, 2) != std::string::npos)
        collectSupplyLineIcons(bounds);
    if (isMapFilterEnabled(17) || isMapFilterEnabled(18)
            || isMapFilterEnabled(15) || isMapFilterEnabled(36)
            || isMapFilterEnabled(39))
        collectIconFamily783F90();
    if (isMapFilterEnabled(10)) {
        const bool supplyLine = gameMode.find("sl", 0, 2) != std::string::npos
            && isMapFilterEnabled(3);
        collectControlPointIcons(supplyLine);
    }
    if (isMapFilterEnabled(20)) collectSquadOrderIcons(bounds);
    if (isMapFilterEnabled(21)) collectCommanderOrderIcons(bounds);
    if (isMapFilterEnabled(19)) collectRequestIcons(bounds);
    if (isMapFilterEnabled(22) || isMapFilterEnabled(23)
            || isMapFilterEnabled(37) || isMapFilterEnabled(39))
        collectIconFamily783440();
    if (local->getIsAlive() && isMapFilterEnabled(6)) collectLocalPlayerIcon();
    if (isMapFilterEnabled(38)) collectSquadRequestIcons();
    if (isMapFilterEnabled(16)) collectIconFamily77E500();
    if (isMapFilterEnabled(31) && m_field708[0] > 0.01) collectSpawnGroupIcons();
    if (isMapFilterEnabled(30)) collectIconFamily7820E0();
    if (isMapFilterEnabled(32)) collectIconFamily77FC30();
    if (isMapFilterEnabled(33) && m_airstrikeActive) collectAirstrikeIcon();
    if (isMapFilterEnabled(34) && m_suppliesActive) collectSuppliesIcon();
    if (isMapFilterEnabled(35)) {
        for (std::list<MapVehicleDrop>::const_iterator it = m_vehicleDrops.begin();
             it != m_vehicleDrops.end(); ++it)
            if (it->active) collectVehicleDropIcon(&*it);
    }

    paintMapIcons(system, &m_fieldb5c, bounds, context);
    if (m_satelliteActive) drawSatellite(system, bounds, context);
    if (isMapFilterEnabled(41)
            && ((!m_field68d && !m_field68c && !m_expandedMap)
                || m_field694 < m_field67c))
        drawMapShadow(m_field928[2], bounds, system, context);
    if (isMapFilterEnabled(42)
            && ((!m_field68d && !m_field68c && m_field774 && !m_expandedMap)
                || m_field694 < m_field67c))
        drawMapFrame(m_field928[3], bounds, system, context);
    if (isMapFilterEnabled(40))
        drawControlPointLabels(system, state, bounds, clip, context);

    m_field07c = false;
    m_field07d = false;
    m_field07e = false;
}

// Loads staged supply-line minimap textures, stopping at the first failure.
// Remaining slots stay zero; the map name is queried for each stage.
void Bf2MenuMapNode::loadSupplyLineTextures(System* system)
{
    if (!hfe::gameLogic)
        return;
    const int last = hfe::gameLogic->getNumberOfControlPointGroups() * 2 - 2;       // +0x184
    for (int stage = 1; stage <= last; ++stage) {
        const std::string name = hfe::formatString(
            "Levels/%s/HUD/Minimap/supplyline_%02d.tga",                             // ds:92FF5C
            hfe::game->getCurrentMapName().c_str(), stage);                          // client +0xEC
        const int texture = system->loadTexture(name.c_str(), false);               // +0x8C
        if (!texture)
            return;
        m_supplyLineTextures[stage] = texture;                                     // +BA8 onwards
    }
}

// Advances to the next supply-line group.
void Bf2MenuMapNode::advanceSupplyLineGroup()
{
    if (!hfe::gameLogic)
        return;
    const std::list<int> previous(m_controlPointIds);                               // never read
    m_controlPointIds.clear();
    ++m_fieldba0;
    if (static_cast<int>(m_fieldba0) <= 0
            || static_cast<int>(m_fieldba0) > hfe::gameLogic->getNumberOfControlPointGroups())
        return;
    typedef std::map<unsigned int, hfe::world::IObject*> ControlPointMap;
    ControlPointMap& controlPoints = *hfe::gameLogic->getControlPointMap();          // +0x174
    for (ControlPointMap::iterator it = controlPoints.begin(); it != controlPoints.end(); ++it) {
        hfe::world::IObject* const object = it->second;
        if (!object)
            continue;
        hfe::world::IControlPoint* const point = static_cast<hfe::world::IControlPoint*>(
            object->queryInterface(hfe::world::IID_IControlPoint));
        if (!point || !point->getShowOnMinimap() || !point->getAllowCaptureByTeam(1))   // +38 / +2C
            continue;
        hfe::world::IControlPointTemplate* const pointTemplate =
            static_cast<hfe::world::IControlPointTemplate*>(object->getTemplate()->queryInterface(
                hfe::world::IID_IControlPointTemplate));
        if (!pointTemplate || pointTemplate->getGroupId() != static_cast<int>(m_fieldba0))   // +BC
            continue;
        m_fieldba0 = static_cast<std::uint32_t>(pointTemplate->getGroupId());       // asked again
        const int id = pointTemplate->getControlPointId();                          // +2C
        m_controlPointIds.push_back(id);
    }
}

// Updates the supply line for the selected group.
void Bf2MenuMapNode::updateSupplyLine(System* system)
{
    if (!hfe::gameLogic)
        return;
    typedef std::map<unsigned int, hfe::world::IObject*> ControlPointMap;
    ControlPointMap& controlPoints = *hfe::gameLogic->getControlPointMap();          // +0x174
    if (!m_fieldb94 && controlPoints.size() != 0) {
        m_fieldb98 = 1;
        m_fieldba0 = 1;
        advanceSupplyLineGroup();
        loadSupplyLineTextures(system);
        m_fieldb9c = 1;
        if (m_supplyLineTextures[1])                                                // +BA8
            m_field93c[1] = m_supplyLineTextures[1];                                // +940
        m_fieldb94 = true;
    }
    if (m_controlPointIds.size() == 0)                                              // +BFC
        return;
    std::list<int> held;
    held.clear();              // 460320 (list::clear) on the list just built, 77F31E. As shipped.
    bool neutral = false;
    for (ControlPointMap::iterator it = controlPoints.begin(); it != controlPoints.end(); ++it) {
        hfe::world::IObject* const object = it->second;
        if (!object)
            continue;
        hfe::world::IControlPoint* const point = static_cast<hfe::world::IControlPoint*>(
            object->queryInterface(hfe::world::IID_IControlPoint));
        if (!point || !point->getShowOnMinimap() || !point->getAllowCaptureByTeam(1))
            continue;
        hfe::world::IControlPointTemplate* const pointTemplate =
            static_cast<hfe::world::IControlPointTemplate*>(object->getTemplate()->queryInterface(
                hfe::world::IID_IControlPointTemplate));
        if (!pointTemplate || pointTemplate->getGroupId() != static_cast<int>(m_fieldba0))
            continue;
        if (point->getTeam() == 1 || point->getTeam() == 0) {                       // +1C
            if (point->getTeam() == 0)
                neutral = true;
        } else {
            const int id = pointTemplate->getControlPointId();
            held.push_back(id);
        }
    }
    if (held.size() == 0 && !neutral) {
        ++m_fieldb9c;
        if (m_supplyLineTextures[m_fieldb9c])
            m_field93c[1] = m_supplyLineTextures[m_fieldb9c];
        m_fieldb95 = false;
        advanceSupplyLineGroup();
        return;
    }
    if (held.size() < m_controlPointIds.size()) {
        if (!m_fieldb95) {
            ++m_fieldb9c;
            if (m_supplyLineTextures[m_fieldb9c]) {
                m_field93c[1] = m_supplyLineTextures[m_fieldb9c];
                m_fieldb95 = true;
            }
        }
        return;
    }
    if (held.size() == m_controlPointIds.size() && m_fieldb95) {
        --m_fieldb9c;
        if (m_supplyLineTextures[m_fieldb9c])
            m_field93c[1] = m_supplyLineTextures[m_fieldb9c];
        m_fieldb95 = false;
    }
}

// The current stage texture (+940) over the whole map, turned with it, faded in
// only once the map is more than 0.7 opaque.
// The local player's team is asked and discarded - with no null test on the player.
void Bf2MenuMapNode::drawSupplyLine(const Rect* adjusted, const Rect* bounds, System* system,
                                    RenderContext* context)
{
    hfe::world::playerManager->getLocalHumanPlayer()->getTeam();                    // +0x30, client +0xE4
    const float alpha = m_field708[2] - 0.7f;                                       // +710, ds:91979C
    if (!(alpha > 0.0f))                                                            // ds:93C850
        return;
    const Color color = {1.0f, 1.0f, 1.0f, alpha};
    Quad quad(adjusted, bounds, &color);
    const float center[2] = {
        bounds->x + bounds->width * 0.5f,
        bounds->y + bounds->height * 0.5f
    };
    rotateQuad(&quad, center, m_field76c);
    if (shouldUseSecondaryIconTexture())                                            // +10C
        quad.texture2 = m_field928[1];                                              // +92C
    drawContextTexture(context, system, m_field93c[1], &quad);
}

// 775110. [INFERRED] void return. The shadow is a full-bounds black quad whose opacity follows the
// map layer alpha.  Retail does not rotate it or attach the secondary texture.
void Bf2MenuMapNode::drawMapShadow(int texture, const Rect* bounds,
                                   System* system, RenderContext* context)
{
    const Color color = {0.0f, 0.0f, 0.0f, m_field708[2] * 0.6f};
    Quad quad(bounds, bounds, &color);
    drawContextTexture(context, system, texture, &quad);
}

// 775170. [INFERRED] void return. Like the shadow pass this uses the same Rect for geometry and clip,
// but the frame color is a constant opaque black.
void Bf2MenuMapNode::drawMapFrame(int texture, const Rect* bounds,
                                  System* system, RenderContext* context)
{
    const Color color = {0.0f, 0.0f, 0.0f, 1.0f};
    Quad quad(bounds, bounds, &color);
    drawContextTexture(context, system, texture, &quad);
}

// Collects icons for eligible populated control-point groups within bounds.
void Bf2MenuMapNode::collectSupplyLineIcons(const Rect* bounds)
{
    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();
    if (!local || !hfe::gameLogic) return;

    const int localTeam = local->getTeam();
    typedef std::map<unsigned int, hfe::world::IObject*> ControlPointMap;
    ControlPointMap& controlPoints = *hfe::gameLogic->getControlPointMap();

    for (ControlPointMap::iterator sourceIt = controlPoints.begin();
         sourceIt != controlPoints.end(); ++sourceIt) {
        hfe::world::IObject* const source = sourceIt->second;
        if (!source) continue;
        hfe::world::IControlPoint* const sourcePoint =
            static_cast<hfe::world::IControlPoint*>(
                source->queryInterface(hfe::world::IID_IControlPoint));
        if (!sourcePoint || !sourcePoint->getShowOnMinimap()) continue;

        hfe::world::IControlPointTemplate* const sourceTemplate =
            static_cast<hfe::world::IControlPointTemplate*>(
                source->getTemplate()->queryInterface(
                    hfe::world::IID_IControlPointTemplate));
        // No source-template null guard exists in either shipped image.
        const std::list<int> groups =
            sourceTemplate->getSupplyGroupNeeded(localTeam);

        for (std::list<int>::const_iterator group = groups.begin();
             group != groups.end(); ++group) {
            std::vector<hfe::world::IObject*> targets;
            if (!hfe::gameLogic->getControlPointsInGroup(targets, *group))
                continue;

            for (std::vector<hfe::world::IObject*>::const_iterator targetIt =
                     targets.begin(); targetIt != targets.end(); ++targetIt) {
                hfe::world::IObject* const target = *targetIt;
                if (!target) continue;
                hfe::world::IControlPoint* const targetPoint =
                    static_cast<hfe::world::IControlPoint*>(
                        target->queryInterface(hfe::world::IID_IControlPoint));
                if (!targetPoint || !targetPoint->getShowOnMinimap()) continue;

                // MSVC evaluates the shipped call's by-value positions from
                // right to left: target first, then source.
                const hfe::Vec3 targetPosition = target->getAbsolutePosition();
                const hfe::Vec3 sourcePosition = source->getAbsolutePosition();
                MapIconRecord line;
                makeWorldSupplyLineRecord(line, bounds,
                                          sourcePosition, targetPosition);

                bool addDirectionArrow = false;
                if (sourcePoint->getTeam() != targetPoint->getTeam()
                        || sourcePoint->getTeam() == 0) {
                    if (sourcePoint->getTeam() != local->getTeam()
                            && sourcePoint->getAllowCaptureByTeam(local->getTeam())
                            && local->getTeam() == targetPoint->getTeam())
                        addDirectionArrow = true;
                }

                line.color.a *= m_field6fc;
                line.size.x = 4.0f;
                line.texture = m_field8a8[1];
                addMapIcon(4, line);

                if (addDirectionArrow) {
                    MapIconRecord arrow;
                    arrow.position = hfe::Vec2{0.0f, 0.0f};
                    arrow.color = line.color;
                    arrow.size = hfe::Vec2{16.0f, 16.0f};
                    arrow.texture = m_field8a8[2];
                    const hfe::Mat4& transform =
                        source->getAbsoluteTransformation();
                    arrow.rotation = 0.0f;
                    projectWorldPosition(hfe::Vec3{
                        transform.m[12], transform.m[13], transform.m[14]},
                        &arrow);
                    arrow.rotation = line.rotation;
                    addMapIcon(11, arrow);
                }
            }
        }
    }
}

namespace {
// Equality belongs to the accepted side of the radius test.
bool __stdcall positionsWithinRadius(const hfe::Vec3* left,
                                     const hfe::Vec3* right,
                                     float radius)
{
    const float z = left->z - right->z;
    const float x = left->x - right->x;
    const float y = left->y - right->y;
    return radius * radius >= z * z + x * x + y * y;
}

// The X difference is rounded to float while Y and Z stay widened for
// the square root, matching the shared HUD helper's x87 evaluation order.
double controlPointDistance(const hfe::Vec3& left, const hfe::Vec3& right)
{
    const float x = static_cast<float>(static_cast<double>(right.x) - left.x);
    const double y = static_cast<double>(right.y) - left.y;
    const double z = static_cast<double>(right.z) - left.z;
    return std::sqrt(z * z + static_cast<double>(x) * x + y * y);
}

// 778680. [INFERRED] name; the __stdcall/free-function ABI, categories and
// repeated player queries are all instruction-backed.
int __stdcall classifyObjectForMap(hfe::world::IObject* object,
                                   hfe::world::IPlayer** ownerOut)
{
    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();
    hfe::world::IPlayer* owner = nullptr;
    int type = 5;

    if (object && local) {
        hfe::world::ISoldier* const soldier =
            static_cast<hfe::world::ISoldier*>(
                object->queryInterface(hfe::world::IID_ISoldier));
        if (soldier) {
            owner = soldier->getOccupingPlayer();
            if (owner) {
                if (owner == local) type = 37;
                else if (owner->getTeam() == local->getTeam()) type = 22;
                else if (owner->getTeam() != local->getTeam()) type = 23;
            }
        } else {
            hfe::world::IPlayerControlObject* const pco =
                hfe::world::findOwningPlayerControlObject(object);
            if (pco) {
                std::map<int, hfe::world::IPlayerControlObject*>* const seats =
                    pco->getPcos();
                for (std::map<int, hfe::world::IPlayerControlObject*>::iterator it =
                         seats->begin(); it != seats->end(); ++it) {
                    hfe::world::IPlayerControlObject* const seat = it->second;
                    if (!seat) continue;
                    hfe::world::IPlayer* const occupant = seat->getOccupingPlayer();
                    if (!occupant) continue;
                    if (occupant->getDefaultVehicle() == occupant->getVehicle())
                        continue;
                    owner = occupant;
                    if (occupant == local) break;
                }

                if (!owner) {
                    const int firstPcoTeam = pco->getTeam();
                    if (firstPcoTeam == 0) {
                        type = 15;
                    } else {
                        const int localTeam = local->getTeam();
                        const int secondPcoTeam = pco->getTeam();
                        type = secondPcoTeam == localTeam ? 17 : 18;
                    }
                } else if (owner == local) {
                    type = 36;
                } else if (owner->getTeam() == local->getTeam()) {
                    type = 17;
                } else if (owner->getTeam() != local->getTeam()) {
                    type = 18;
                }
            }
        }
    }

    if (ownerOut) *ownerOut = owner;
    return type;
}
}

// 7779B0 / 7779E0, virtual +E4/+E8.
int Bf2MenuMapNode::timedMarkerTexture(unsigned int id)
{
    for (std::vector<MapTimedMarker>::const_iterator it = m_timedMarkers.begin();
         it != m_timedMarkers.end(); ++it)
        if (it->networkId == static_cast<int>(id)) return it->texture;
    return 0;
}

hfe::Vec2 Bf2MenuMapNode::timedMarkerSize(unsigned int id)
{
    for (std::vector<MapTimedMarker>::const_iterator it = m_timedMarkers.begin();
         it != m_timedMarkers.end(); ++it)
        if (it->networkId == static_cast<int>(id)) return it->size;
    return hfe::Vec2{0.0f, 0.0f};
}

// 775640, virtual +134. The unused pointer and the missing icon guard are
// both shipped behaviour.
void Bf2MenuMapNode::populateVehicleMapIcon(hfe::world::IObject* object,
                                             MapIconRecord* icon,
                                             bool*, bool spotted)
{
    if (!object) return;
    hfe::world::IVehicleHudComp* component = nullptr;
    if (!icon
            || !(component = static_cast<hfe::world::IVehicleHudComp*>(
                hfe::world::object_detail::LayoutAccess::vehicleHudComponent(object)))
            || !m_system) {
        icon->texture = 0;
        return;
    }
    if (spotted && component->getHudIcons()->spottedIconPtr) {
        icon->texture = component->getHudIcons()->spottedIconPtr;
        const float sizeY = static_cast<float>(
            component->getHudInfo()->spottedIconSize);
        const hfe::world::VehicleHudInfo* const second = component->getHudInfo();
        icon->size.x = static_cast<float>(second->spottedIconSize);
        icon->size.y = sizeY;
    } else {
        icon->texture = component->getHudIcons()->miniMapIconPtr;
        const float sizeY = static_cast<float>(
            component->getHudInfo()->miniMapIconSize);
        const hfe::world::VehicleHudInfo* const second = component->getHudInfo();
        icon->size.x = static_cast<float>(second->miniMapIconSize);
        icon->size.y = sizeY;
    }
}

// 775450, virtual +138.
void Bf2MenuMapNode::populatePlayerMapIcon(hfe::world::IPlayer* player,
                                            MapIconRecord* icon,
                                            bool* headingOut,
                                            bool* rotateOut,
                                            bool showKit,
                                            bool specialRoleIcons,
                                            bool spotted)
{
    if (!icon || !player) return;
    if (showKit) {
        if (headingOut) *headingOut = false;
        if (rotateOut) *rotateOut = false;
        icon->size = hfe::Vec2{15.0f, 15.0f};
        icon->texture = hfe::hudManager->getPlayerKitIcon(player, true, spotted);
    } else {
        if (headingOut) *headingOut = false;
        icon->size = hfe::Vec2{16.0f, 16.0f};
        icon->texture = m_field93c[19];                 // +988 mini_Soldier
    }

    if (!specialRoleIcons) return;
    if (player->getIsSquadLeader()) {
        if (headingOut) *headingOut = true;
        if (rotateOut) *rotateOut = true;
        icon->size = hfe::Vec2{16.0f, 16.0f};
        icon->texture = getSmallSquadIcon(player->getSquadId());
    } else if (player->getIsCommander()) {
        if (headingOut) *headingOut = true;
        if (rotateOut) *rotateOut = true;
        icon->size = hfe::Vec2{32.0f, 32.0f};
        hfe::world::IPlayer* const local =
            hfe::world::playerManager->getLocalHumanPlayer();
        int side = 1;
        if (local) {
            const int localTeam = local->getTeam();
            const int playerTeam = player->getTeam();
            side = playerTeam != localTeam ? 1 : 2;
        }
        icon->texture = m_field93c[14 + side];          // +978/+97C
    }
}

// 7755A0, virtual +13C. The map name and the final two state flags are present
// in both retail overrides but unused by this base implementation.
void Bf2MenuMapNode::populateControlPointMapIcon(
    MapIconRecord* icon, int team, bool useTeamTexture, std::string,
    bool notCapturable, bool, bool)
{
    if (!icon) return;

    icon->color = Color{1.0f, 1.0f, 1.0f, m_field6fc};
    icon->size = hfe::Vec2{32.0f, 32.0f};
    icon->texture = useTeamTexture && !notCapturable
        ? m_controlPointTextures[team]
        : m_controlPointBaseTextures[team];
}

// 775720, virtual +EC.
bool Bf2MenuMapNode::populateNetworkIcon(int id, MapIconRecord* icon,
                                         bool asPlayer)
{
    if (!icon) return false;
    if (id > 0) {
        icon->size = timedMarkerSize(static_cast<unsigned int>(id));
        icon->texture = timedMarkerTexture(static_cast<unsigned int>(id));
    }
    if (!icon->texture) {
        hfe::world::IObject* object = hfe::io::networkManager->getObject(
            static_cast<unsigned short>(id));
        if (!object)
            object = hfe::world::objectManager->getObjectFromGrid(
                static_cast<unsigned int>(id));
        if (asPlayer) {
            populatePlayerMapIcon(hfe::world::playerManager->getPlayerFromId(id),
                                  icon, nullptr, nullptr, true, false, true);
        } else if (object) {
            populateVehicleMapIcon(object, icon, nullptr, true);
        }
    }
    if (icon->texture) return true;
    icon->size = hfe::Vec2{16.0f, 16.0f};
    return false;
}

// 772ED0, virtual +FC.
int Bf2MenuMapNode::getSmallSquadIcon(int squadId)
{
    int& texture = m_field7fc[squadId];
    if (!texture) {
        const std::string path = hfe::formatString(
            "Ingame/Player/Icons/Minimap/mini_Squad_%d_small.tga", squadId);
        texture = m_system->loadTexture(path.c_str(), true);
    }
    return texture;
}

// 772FB0, virtual +140. Retail deliberately indexes size + 2*squad, so the
// cache rows overlap; preserve that quirk.
int Bf2MenuMapNode::getLeaderMapIcon(int squadId, int sizeIndex)
{
    int& texture = m_field84c[sizeIndex + 2 * squadId];
    if (!texture) {
        std::string size("Medium");
        if (sizeIndex == 0) size = "Small";
        else if (sizeIndex == 2) size = "Large";
        const std::string path = squadId <= 0
            ? hfe::formatString(
                "Ingame/Vehicles/Icons/Minimap/mini_Commander%s.tga",
                size.c_str())
            : hfe::formatString(
                "Ingame/Vehicles/Icons/Minimap/mini_Squad%s_%d.tga",
                size.c_str(), squadId);
        texture = m_system->loadTexture(path.c_str(), true);
    }
    return texture;
}


void Bf2MenuMapNode::projectObject(hfe::world::IObject* object,
                                    MapIconRecord* icon, bool heading)
{
    if (!object || !icon) return;
    const hfe::Mat4& transform = object->getAbsoluteTransformation();
    icon->rotation = heading
        ? m_field76c - static_cast<float>(std::atan2(transform.m[8], transform.m[10]))
        : 0.0f;
    projectWorldPosition(hfe::Vec3{
        transform.m[12], transform.m[13], transform.m[14]}, icon);
}


unsigned int Bf2MenuMapNode::remapMapIconType(unsigned int type, int texture,
                                               bool timed, float* rotation)
{
    if (type == 37 || type == 36 || type == 25 || type == 24) return type;
    if (texture == m_field9a8[1]) {
        if (rotation) *rotation = 0.0f;
        return 27;
    }
    if (texture == m_field9a8[0]) {
        if (rotation) *rotation = 0.0f;
        return 28;
    }
    if (texture == m_field9a8[2] || texture == m_field9a8[3]) {
        if (rotation) *rotation = 0.0f;
        return 26;
    }
    return timed ? 39u : type;
}

// The heap temporary is intentionally leaked after push_back, exactly
// as in retail.
void Bf2MenuMapNode::addTimedMarker(int networkId,
                                    hfe::Vec2 position,
                                    MapIconRecord icon,
                                    float duration,
                                    float field20,
                                    float field24,
                                    float field18,
                                    int kind)
{
    if (kind != 3) {
        for (std::vector<MapTimedMarker>::iterator it = m_timedMarkers.begin();
             it != m_timedMarkers.end(); ++it)
            if (it->networkId == networkId) return;
    }

    MapTimedMarker* const marker = new MapTimedMarker;
    marker->texture = icon.texture;
    marker->networkId = networkId;
    if (std::sqrt(icon.size.x * icon.size.x + icon.size.y * icon.size.y) == 0.0f)
        marker->size = hfe::Vec2{16.0f, 16.0f};
    else
        marker->size = icon.size;
    marker->kind = kind;
    marker->remaining = duration;
    marker->field18 = field18;
    marker->initialDuration = duration;
    marker->field20 = field20;
    marker->field24 = field24;
    marker->negativeDuration = duration < 0.0f;
    marker->position = position;
    m_timedMarkers.push_back(*marker);
}


bool Bf2MenuMapNode::resolveTimedMarker(int networkId,
                                        hfe::world::IPlayer* player,
                                        MapIconRecord* icon,
                                        bool* rotate,
                                        bool* secondary,
                                        int* type)
{
    std::vector<MapTimedMarker>::iterator marker = m_timedMarkers.begin();
    while (marker != m_timedMarkers.end()
           && (marker->networkId != networkId || marker->kind == 3))
        ++marker;
    if (marker == m_timedMarkers.end()) return false;
    if (marker->remaining <= 0.0f && marker->field18 <= 0.0f
            && !marker->negativeDuration) {
        m_timedMarkers.erase(marker);
        return true;
    }
    if (!icon) return true;

    hfe::world::IObject* object = hfe::io::networkManager->getObject(
        static_cast<unsigned short>(networkId));
    if (!object)
        object = hfe::world::objectManager->getObjectFromGrid(
            static_cast<unsigned int>(networkId));
    bool usePlayer = false;
    if (object) {
        hfe::world::IVehicleHudComp* const component =
            static_cast<hfe::world::IVehicleHudComp*>(
                hfe::world::object_detail::LayoutAccess::vehicleHudComponent(object));
        usePlayer = component && component->getHudInfo()->usePlayerIcon && player;
    }
    if (usePlayer || (!populateNetworkIcon(networkId, icon, false) && player)) {
        if (!populateNetworkIcon(player->getId(), icon, true))
            icon->texture = m_field9a8[10];            // +9D0 unitSpotted
        if (rotate) *rotate = false;
    }
    colorMapIcon(player, icon, 15, true);

    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();
    if (marker->kind == 1) {
        if (local) {
            if (marker->field18 > 0.0f || m_fielda18)
                icon->color = hfe::hud::mapTimedColors[2];
            icon->color.a = m_field6fc;
            if (type) *type = 25;
        }
        return true;
    }
    if (marker->kind == 2) {
        if (!local) return true;
        if (secondary) *secondary = false;
        if (rotate) *rotate = true;
        if (type) *type = 24;
        icon->color = hfe::hud::mapAlertColors[local->getTeam()];
        icon->color.a = marker->remaining / marker->initialDuration * m_field6fc;
        return true;
    }
    if (marker->kind == 4) {
        if (rotate) *rotate = false;
        return true;
    }
    if (marker->field18 <= 0.0f)
        icon->color.a = m_fielda18 ? 0.0f : m_field6fc;
    if (secondary && type && (*type == 23 || *type == 18))
        *secondary = false;
    return true;
}

// 7788B0, virtual +F0. This is deliberately verbose: the shipped body repeats
// player/manager queries and has distinct HUD-mode and ordinary-map branches.
void Bf2MenuMapNode::colorMapIcon(hfe::world::IPlayer* player,
                                   MapIconRecord* icon, int type,
                                   bool compareTeam)
{
    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();
    const int mode = hfe::hudManager->getMode();
    if (!icon) return;

    if (icon->texture == m_field9a8[1] || icon->texture == m_field9a8[0]
            || icon->texture == m_field9a8[2]
            || icon->texture == m_field9a8[3]) {
        icon->color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        return;
    }

    if (!player) {
        if (local)
            icon->color = (type == 22 || type == 17)
                ? hfe::hud::mapObjectColors[2]
                : hfe::hud::mapNeutralColors[2];
        return;
    }
    if (!local) return;

    hfe::world::IObject* const vehicle = player->getVehicle();
    bool vehicleTypeEnabled = true;
    bool sameVehicleRoot = false;
    int vehicleType = -1;
    if (player->getDefaultVehicle()) {
        if (!vehicle || vehicle == player->getDefaultVehicle()) {
            vehicleTypeEnabled = false;
        } else {
            hfe::world::IVehicleHudComp* const component =
                static_cast<hfe::world::IVehicleHudComp*>(
                    hfe::world::object_detail::LayoutAccess::vehicleHudComponent(vehicle));
            if (component) vehicleType = component->getHudInfo()->vehicleType;
        }
        hfe::world::ISoldier* const soldier = hfe::hudManager->getPlayerSoldier(player);
        const int kitType = soldier ? soldier->getKitType() : -1;
        if (static_cast<unsigned int>(kitType) < 8u)
            (void)m_field3c0.test(static_cast<std::size_t>(kitType));
        if (static_cast<unsigned int>(vehicleType) < 7u)
            vehicleTypeEnabled = m_field3c0.test(32u + static_cast<unsigned int>(vehicleType));
        if (local->getVehicle() && vehicle
                && hfe::world::getRootParent(local->getVehicle())
                    == hfe::world::getRootParent(vehicle))
            sameVehicleRoot = true;
    }

    const int side = 2 - (local->getTeam() != player->getTeam());
    const bool sameTeam = compareTeam && player->getTeam() == local->getTeam();
    if (std::find(m_fielda34.begin(), m_fielda34.end(), player->getId())
            != m_fielda34.end()) {
        icon->color = hfe::hud::mapSelectedColors[side];
        return;
    }

    const bool roleMode = mode == 18 || mode == 17 || mode == 19
        || mode == 26 || mode == 20;
    if (roleMode && hfe::hudManager->getSquad()) {
        if (sameTeam
                && (player->getId()
                        == hfe::hudManager->getSquad()->getSelectedPlayerForMap()
                    || player->getSquadId()
                        == hfe::hudManager->getSquad()->getSelectedSquadForMap())) {
            icon->color = hfe::hud::mapWhiteColors[side];
            return;
        }
    } else if (hfe::hudManager->isCommanderShown()
               && hfe::hudManager->getCommander()) {
        if (sameTeam) {
            if (player->getId()
                    == hfe::hudManager->getCommander()->getExpandedSquad()) {
                icon->color = hfe::hud::mapWhiteColors[side];
                return;
            }
            if (player->getSquadId()
                    && hfe::hudManager->getCommander()->isSquadSelected(
                        player->getSquadId())) {
                icon->color = hfe::hud::mapWhiteColors[side];
                return;
            }
        }
        if (player->getSquadId() && icon->texture != m_fielda94
                && player->getTeam() == local->getTeam()) {
            icon->color = hfe::hud::squadChatColors[side];
            return;
        }
        if (player->getIsCommander()
                && (player->getDefaultVehicle() == player->getVehicle()
                    || (!vehicleTypeEnabled && m_field3c0.test(7)))
                && icon->texture != m_fielda94) {
            icon->color = Color{1.0f, 1.0f, 1.0f, 1.0f};
            return;
        }
        icon->color = hfe::hud::mapObjectColors[side];
        return;
    } else {
        if ((sameVehicleRoot && local->getSquadId())
                || (player->getSquadId()
                    && player->getSquadId() == local->getSquadId()
                    && player->getTeam() == local->getTeam()
                    && !player->getIsCommander()
                    && icon->texture != m_fielda94)) {
            icon->color = hfe::hud::squadChatColors[side];
            return;
        }
        if (player->getIsCommander()
                && (player->getDefaultVehicle() == player->getVehicle()
                    || hfe::world::getRootParent(player->getVehicle())
                        ->getTemplate()->getClassID() == 40006)
                && (icon->texture == m_field93c[15]
                    || icon->texture == m_field93c[16])) {
            icon->color = Color{1.0f, 1.0f, 1.0f, 1.0f};
            return;
        }
        icon->color = hfe::hud::mapObjectColors[side];
        return;
    }

    // Common role-mode tail.
    if (player->getSquadId()
            && player->getSquadId() == local->getSquadId()
            && !player->getIsCommander()
            && icon->texture != m_fielda94) {
        icon->color = hfe::hud::squadChatColors[side];
        return;
    }
    if (player->getIsCommander()
            && (player->getDefaultVehicle() == player->getVehicle()
                || (!vehicleTypeEnabled && m_field3c0.test(7)))
            && icon->texture != m_fielda94) {
        icon->color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        return;
    }
    icon->color = hfe::hud::mapObjectColors[side];
}


void Bf2MenuMapNode::updateSatelliteTarget(hfe::world::IObject* object,
                                            int networkId, int texture)
{
    if (!object) return;
    const float objectX = object->getAbsoluteTransformation().m[12];
    const int defaultWorldX = 0;
    const float worldX = static_cast<float>(hfe::settings->intGet(
        std::string("GLSWorldSizeX"), defaultWorldX));
    const float scan = (m_satelliteScanX / m_fieldb5c.width - 0.5f) * worldX;
    float step = 10.0f;
    if (m_satelliteScanY != 0.0f && scan != m_satelliteScanY)
        step = scan - m_satelliteScanY;
    const float distance = objectX - scan;
    m_satelliteScanY = scan;
    if (std::fabs(distance) < std::fabs(step) && distance < 0.0f) {
        MapIconRecord icon;
        icon.position = hfe::Vec2{0.0f, 0.0f};
        icon.color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        icon.size = hfe::Vec2{0.0f, 0.0f};
        icon.texture = texture;
        addTimedMarker(networkId, hfe::Vec2{0.0f, 0.0f}, icon,
                       2.0f, 0.0f, 4.0f,
                       m_fielda8c, 2);
        hfe::g_bfSound->playSound(std::string("HUD_Satellite_target"));
    }
}

// Source position and radius deliberately live across iterations:
// malformed/null UAV entries reuse the last values observed by retail.
void Bf2MenuMapNode::updateUavTarget(hfe::world::IObject* object,
                                      int networkId, int texture)
{
    if (!object) return;

    const hfe::Vec3 targetPosition = object->getAbsolutePosition();
    hfe::Vec3 sourcePosition{0.0f, 0.0f, 0.0f};
    hfe::Vec2 targetMap{0.0f, 0.0f};
    hfe::Vec2 sourceMap{0.0f, 0.0f};
    int radius = 0;
    projectWorldPosition(targetPosition, &targetMap);

    for (std::map<unsigned int, hfe::world::IObject*>::iterator it =
             m_uavObjects.begin(); it != m_uavObjects.end(); ++it) {
        if (it->first == 0xffffffffu) {
            sourcePosition = m_uavPosition;
            radius = m_uavRadius;
        } else if (it->second) {
            sourcePosition = it->second->getAbsolutePosition();
            hfe::world::IAbilityGLComp* const ability =
                static_cast<hfe::world::IAbilityGLComp*>(
                    hfe::world::object_detail::LayoutAccess::abilityGLComponent(
                        it->second));
            if (ability)
                radius = ability->getGLInfo()->radarRadius;
        }

        projectWorldPosition(sourcePosition, &sourceMap);
        const float dx = sourcePosition.x - targetPosition.x;
        const float dz = sourcePosition.z - targetPosition.z;
        const float distance = static_cast<float>(std::sqrt(dx * dx + dz * dz));
        float angle = static_cast<float>(std::atan2(
            targetMap.x - sourceMap.x, targetMap.y - sourceMap.y));
        if (angle < 0.0f) angle += 6.2831855f;

        if (static_cast<float>(radius) >= distance
                && std::fabs(m_fieldabc - m_fieldac4 - angle) < m_fieldac4) {
            MapIconRecord icon;
            icon.texture = texture;
            addTimedMarker(networkId, hfe::Vec2{0.0f, 0.0f}, icon,
                           m_fieldaec, 0.0f, 0.0f, 0.3f, 2);
            hfe::g_bfSound->playSound(std::string("HUD_Satellite_target"));
        }
    }
}

void Bf2MenuMapNode::collectIconFamily783F90()
{
    // Body below is PE 783F90; its helper calls above are all typed C++ calls.
    if (!hfe::gameLogic) return;
    std::map<unsigned int, hfe::world::IObject*>* const objects =
        hfe::gameLogic->getPcoMap();
    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();

    for (std::map<unsigned int, hfe::world::IObject*>::iterator objectIt =
             objects->begin(); objectIt != objects->end(); ++objectIt) {
        hfe::world::IObject* const object = objectIt->second;
        if (object) {
            hfe::world::IArmorGLComp* const armor =
                static_cast<hfe::world::IArmorGLComp*>(
                    hfe::world::object_detail::LayoutAccess::armorComponent(object));
            if (armor && armor->getStatus() == 1) continue;
        }

        bool closeOrMarked = true;
        if (local && object) {
            if (local->getIsAlive() && local->getVehicle()) {
                const hfe::Vec3& objectPosition = object->getAbsolutePosition();
                const hfe::Vec3& vehiclePosition =
                    local->getVehicle()->getAbsolutePosition();
                closeOrMarked = positionsWithinRadius(
                    &objectPosition, &vehiclePosition, 100.0f);
            }
            if (!closeOrMarked) {
                hfe::io::INetworkable* const networkable = object->getNetworkable();
                if (networkable)
                    closeOrMarked = resolveTimedMarker(networkable->m_networkableId,
                                                       nullptr, nullptr, nullptr,
                                                       nullptr, nullptr);
            }
        }

        if (!local || !object || object->getParent()
                || (object->getFlags() & 1u) || object->isObjectDestroyed()
                || !m_field674)
            continue;

        MapIconRecord icon;
        icon.position = hfe::Vec2{0.0f, 0.0f};
        icon.size = hfe::Vec2{0.0f, 0.0f};
        icon.color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        icon.rotation = 0.0f;
        icon.texture = 0;
        icon.clampToVisibleMap = true;

        MapIconRecord secondaryIcon;
        secondaryIcon.color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        secondaryIcon.size = hfe::Vec2{16.0f, 64.0f};
        secondaryIcon.rotation = 0.0f;
        secondaryIcon.texture = 0;
        secondaryIcon.clampToVisibleMap = false;

        MapIconRecord headingIcon;
        headingIcon.color = Color{0.0f, 0.0f, 0.0f, 1.0f};
        headingIcon.size = hfe::Vec2{32.0f, 32.0f};
        headingIcon.rotation = 0.0f;
        headingIcon.texture = m_field93c[17];
        headingIcon.clampToVisibleMap = false;

        const float iconAlpha = m_field6fc;
        bool timed = false;
        bool unsupported = false;
        bool rotate = true;
        bool secondary = true;
        bool heading = false;
        bool forcePlayerIcon = false;
        int vehicleType = -1;
        hfe::world::IVehicleHudComp* const vehicleHud =
            static_cast<hfe::world::IVehicleHudComp*>(
                hfe::world::object_detail::LayoutAccess::vehicleHudComponent(object));
        if (vehicleHud) vehicleType = vehicleHud->getHudInfo()->vehicleType;

        hfe::world::IPlayer* owner = nullptr;
        int type = classifyObjectForMap(object, &owner);
        (void)isMapFilterEnabled(static_cast<unsigned int>(type));
        const unsigned char replicationTeam =
            static_cast<hfe::world::SimpleObject*>(object)->getReplicationState();
        if (replicationTeam) {
            const bool friendlyEnabled = replicationTeam == local->getTeam()
                && isMapFilterEnabled(22);
            if (!friendlyEnabled && replicationTeam != local->getTeam())
                (void)isMapFilterEnabled(23);
        }

        bool enabled = false;
        switch (type) {
        case 15:
        case 17:
            enabled = isMapFilterEnabled(static_cast<unsigned int>(type));
            break;
        case 18:
            if (!closeOrMarked && m_field674 == 1)
                enabled = isMapFilterEnabled(18);
            break;
        case 36:
            icon.clampToVisibleMap = true;
            secondaryIcon.clampToVisibleMap = true;
            headingIcon.clampToVisibleMap = true;
            enabled = isMapFilterEnabled(36);
            break;
        default:
            unsupported = true;
            enabled = false;
            break;
        }

        if (enabled) {
            if (owner && owner->getDefaultVehicle() && type != 36
                    && (m_field07c || m_field07d || m_field07e)) {
                hfe::world::IObject* const defaultVehicle = owner->getDefaultVehicle();
                hfe::io::INetworkable* const networkable =
                    defaultVehicle->getNetworkable();
                hfe::world::Kit* const kit = hfe::hudManager->getPlayerKit(owner);
                hfe::world::IAbilityGLComp* ability = nullptr;
                if (kit)
                    ability = static_cast<hfe::world::IAbilityGLComp*>(
                        hfe::world::object_detail::LayoutAccess::abilityGLComponent(kit));
                if (networkable && ability) {
                    if (m_field07c && ability->getGLInfo()->hasHealingAbility) {
                        MapIconRecord markerIcon;
                        markerIcon.texture = m_field9a8[3];
                        addTimedMarker(networkable->m_networkableId,
                                       hfe::Vec2{0.0f, 0.0f}, markerIcon,
                                       0.0f, 0.0f, 0.0f, m_fielda2c, 4);
                    }
                    if (m_field07e && ability->getGLInfo()->hasRepairingAbility) {
                        MapIconRecord markerIcon;
                        markerIcon.texture = m_field9a8[1];
                        addTimedMarker(networkable->m_networkableId,
                                       hfe::Vec2{0.0f, 0.0f}, markerIcon,
                                       0.0f, 0.0f, 0.0f, m_fielda2c, 4);
                    }
                    if (m_field07d && ability->getGLInfo()->hasAmmoAbility) {
                        MapIconRecord markerIcon;
                        markerIcon.texture = m_field9a8[0];
                        addTimedMarker(networkable->m_networkableId,
                                       hfe::Vec2{0.0f, 0.0f}, markerIcon,
                                       0.0f, 0.0f, 0.0f, m_fielda2c, 4);
                    }
                }
            } else if (owner && vehicleType >= 0) {
                hfe::world::ISoldier* const soldier =
                    hfe::hudManager->getPlayerSoldier(owner);
                const int kitType = soldier ? soldier->getKitType() : -1;
                if (static_cast<unsigned int>(kitType) < 8u
                        && static_cast<unsigned int>(vehicleType) < 7u) {
                    if (!m_field3c0.test(
                                32u + static_cast<unsigned int>(vehicleType))
                            && !m_field3c0.test(
                                static_cast<unsigned int>(kitType))
                            && !owner->getIsSquadLeader()
                            && !owner->getIsCommander()
                            && m_field3c0.test(7)) {
                        enabled = false;
                    } else if (!m_field3c0.test(
                                   32u + static_cast<unsigned int>(vehicleType))) {
                        if (m_field3c0.test(static_cast<unsigned int>(kitType))
                                || (m_field3c0.test(7)
                                && (owner->getIsSquadLeader()
                                    || owner->getIsCommander()))) {
                            forcePlayerIcon = true;
                            enabled = true;
                        } else {
                            forcePlayerIcon = false;
                            enabled = false;
                        }
                    }
                }
            } else if (!owner && vehicleType >= 0) {
                enabled = m_field3c0.test(
                    32u + static_cast<unsigned int>(vehicleType));
            }
        }

        if (owner && owner->getDefaultVehicle() && type != 36 && enabled
                && !owner->getIsMuted() && owner->getIsTalking()) {
            hfe::world::IObject* const defaultVehicle = owner->getDefaultVehicle();
            hfe::io::INetworkable* const networkable =
                defaultVehicle->getNetworkable();
            if (networkable) {
                populateVehicleMapIcon(object, &icon, &rotate, false);
                addTimedMarker(networkable->m_networkableId,
                               hfe::Vec2{0.0f, 0.0f}, icon,
                               m_fielda2c, m_fielda24, m_fielda28,
                               0.0f, 1);
            }
        }

        if (!unsupported) {
            hfe::io::INetworkable* const networkable = object->getNetworkable();
            if (type == 18 && networkable) {
                hfe::world::IPlayerControlObject* const pco =
                    static_cast<hfe::world::IPlayerControlObject*>(
                        object->queryInterface(hfe::world::IID_IPlayerControlObject));
                if (m_satelliteActive && pco && pco->isOccupied())
                    updateSatelliteTarget(object, networkable->m_networkableId,
                                          m_fielda94);
                if (!m_uavObjects.empty() && owner && owner->getIsAlive()
                        && !owner->getIsManDown())
                    updateUavTarget(object, networkable->m_networkableId, 0);
            }
            if (networkable)
                timed = resolveTimedMarker(networkable->m_networkableId, owner,
                                           &icon, &rotate, &secondary, &type);
            if (!timed && enabled && owner && owner->getDefaultVehicle()) {
                hfe::io::INetworkable* const defaultNetworkable =
                    owner->getDefaultVehicle()->getNetworkable();
                if (defaultNetworkable)
                    timed = resolveTimedMarker(defaultNetworkable->m_networkableId,
                                               owner, &icon, &rotate,
                                               &secondary, &type);
            }
        }

        if (!enabled && !m_field684 && !timed) continue;
        projectObject(object, &icon, true);
        if (!timed) {
            if (forcePlayerIcon
                    || (vehicleHud && vehicleHud->getHudInfo()->usePlayerIcon
                        && type != 15)) {
                if (owner) {
                    populatePlayerMapIcon(owner, &icon, &heading, &rotate,
                                          m_showKitsOnMap, m_field3c0.test(7),
                                          false);
                    projectObject(owner->getDefaultVehicle(), &icon, rotate);
                    if (rotate) {
                        const hfe::Mat4& camera =
                            owner->getCamera()->getTransformation(0.0f);
                        icon.rotation = m_field76c - static_cast<float>(
                            std::atan2(camera.m[8], camera.m[10]));
                    }
                }
                secondary = false;
            } else {
                populateVehicleMapIcon(object, &icon, &rotate, false);
                secondary = m_field3c0.test(7);
            }
            colorMapIcon(owner, &icon, type, true);
            icon.color.a = iconAlpha;
        }

        type = static_cast<int>(remapMapIconType(
            static_cast<unsigned int>(type), icon.texture, timed,
            &icon.rotation));
        if (owner && secondary) {
            if ((owner->getIsCommander() || owner->getIsSquadLeader())
                    && vehicleHud && !forcePlayerIcon) {
                secondaryIcon.texture = getLeaderMapIcon(
                    owner->getSquadId(),
                    vehicleHud->getHudInfo()->miniMapIconLeaderSizeValue);
            } else {
                secondary = false;
            }
        }

        if (m_fielda7b) {
            if (heading) {
                headingIcon.position = icon.position;
                headingIcon.rotation = icon.rotation;
                headingIcon.color.a = icon.color.a;
                addMapIcon(static_cast<unsigned int>(type), headingIcon);
            }
            if (secondary) {
                secondaryIcon.position = icon.position;
                secondaryIcon.rotation = icon.rotation;
                if (owner && owner->getIsSquadLeader())
                    secondaryIcon.color = icon.color;
                secondaryIcon.color.a = icon.color.a;
                addMapIcon(static_cast<unsigned int>(type), secondaryIcon);
            }
        }
        addMapIcon(static_cast<unsigned int>(type), icon);
    }
}
// Both passes use the typed GameLogic control-point map and typed
// control-point/template interfaces. The fixed cache key, repeated getters,
// missing guards and the unreachable erase arm are all shipped behaviour.
// Its three IGame +F0 calls pass the game mode, as in paintSelf (784A80).
void Bf2MenuMapNode::collectControlPointIcons(bool supplyLine)
{
    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();
    if (!hfe::gameLogic || !local) return;

    typedef std::map<unsigned int, hfe::world::IObject*> ControlPointMap;
    ControlPointMap& points = *hfe::gameLogic->getControlPointMap();

    m_field06d[0] = 0;
    int closestTeam = -1;
    float closestDistance = 1000.0f;
    m_field070[1] = 0;
    m_field070[2] = 0;

    std::list<int> prepassIds;
    if (supplyLine) {
        for (ControlPointMap::iterator it = points.begin(); it != points.end(); ++it) {
            hfe::world::IObject* const object = it->second;
            if (!object) continue;

            hfe::world::IControlPoint* const point =
                static_cast<hfe::world::IControlPoint*>(
                    object->queryInterface(hfe::world::IID_IControlPoint));
            if (!point || !point->getShowOnMinimap()
                    || !point->getAllowCaptureByTeam(1))
                continue;

            hfe::world::IControlPointTemplate* const pointTemplate =
                static_cast<hfe::world::IControlPointTemplate*>(
                    object->getTemplate()->queryInterface(
                        hfe::world::IID_IControlPointTemplate));
            if (pointTemplate
                    && pointTemplate->getGroupId() == m_fieldba0
                    && point->getTeam() != 1
                    && point->getTeam() != 0)
                prepassIds.push_back(pointTemplate->getControlPointId());
        }
    }

    const unsigned int cacheKey = 0xCF9D54FBu;
    for (ControlPointMap::iterator it = points.begin(); it != points.end(); ++it) {
        hfe::world::IObject* const object = it->second;
        if (!object) continue;

        MapIconRecord icon;
        icon.size = hfe::Vec2{33.0f, 33.0f};
        icon.rotation = 0.0f;
        icon.clampToVisibleMap = false;
        unsigned int iconType = 5;
        bool rotateIcon = true;

        hfe::world::IControlPoint* const point =
            static_cast<hfe::world::IControlPoint*>(
                object->queryInterface(hfe::world::IID_IControlPoint));
        if (!point || !point->getShowOnMinimap()) {
            if (!m_field684) continue;
        } else {
            iconType = 10;
            hfe::world::IControlPointTemplate* const pointTemplate =
                static_cast<hfe::world::IControlPointTemplate*>(
                    object->getTemplate()->queryInterface(
                        hfe::world::IID_IControlPointTemplate));
            const int team = point->getTeam();

            std::map<unsigned int, hfe::world::IControlPoint*>::iterator cached =
                m_controlPoints.find(cacheKey);
            const bool localCaptureActive =
                team == local->getTeam()
                && point->getTimeUntilTakeOver() > 0.0f;
            bool cachePresent = cached != m_controlPoints.end();
            std::list<int> neededGroups;

            if (!localCaptureActive && !cachePresent
                    && team == local->getTeam()) {
                neededGroups = pointTemplate->getSupplyGroupNeeded(team);
                int missing = static_cast<int>(neededGroups.size());
                if (missing > 0) {
                    for (std::list<int>::const_iterator group = neededGroups.begin();
                         group != neededGroups.end(); ++group) {
                        if (m_controlPoints.find(*group) != m_controlPoints.end())
                            --missing;
                    }
                    if (missing <= 0) cachePresent = true;
                }
            }

            if (cachePresent || localCaptureActive) {
                if (cached == m_controlPoints.end())
                    m_controlPoints.insert(std::make_pair(cacheKey, point));
            } else if (cached != m_controlPoints.end()) {
                // The iterator test that produced cachePresent makes this arm
                // unreachable for an ordinary std::map; retail emits it anyway.
                m_controlPoints.erase(cached);
            }

            if (!supplyLine) {
                populateControlPointMapIcon(
                    &icon, team, !pointTemplate->getUnableToChangeTeam(),
                    hfe::game->getCurrentMapGameMode(), false, false, false);
            } else {
                bool missingSupplySide = false;
                {
                    const std::list<int> teamOne =
                        pointTemplate->getSupplyGroupNeeded(1);
                    if (teamOne.empty()) {
                        missingSupplySide = true;
                    } else {
                        const std::list<int> teamTwo =
                            pointTemplate->getSupplyGroupNeeded(2);
                        missingSupplySide = teamTwo.empty();
                    }
                }

                if (missingSupplySide) {
                    const int captureTeam = 2 - (team != 1);
                    populateControlPointMapIcon(
                        &icon, team, false, hfe::game->getCurrentMapGameMode(),
                        !point->getAllowCaptureByTeam(captureTeam),
                        cachePresent, localCaptureActive);
                } else {
                    bool listed = false;
                    for (std::list<int>::const_iterator id = m_fieldc00.begin();
                         id != m_fieldc00.end(); ++id) {
                        if (pointTemplate->getControlPointId() == *id)
                            listed = true;
                    }
                    if (prepassIds.size() < m_controlPointIds.size()) {
                        for (std::list<int>::const_iterator id = m_controlPointIds.begin();
                             id != m_controlPointIds.end(); ++id) {
                            if (pointTemplate->getControlPointId() == *id)
                                listed = true;
                        }
                    }

                    if (listed) {
                        populateControlPointMapIcon(
                            &icon, team, true, std::string("gpm_cq"),
                            false, false, false);
                    } else {
                        const int captureTeam = team == 0
                            ? local->getTeam()
                            : 2 - (team != 1);
                        populateControlPointMapIcon(
                            &icon, team, true, hfe::game->getCurrentMapGameMode(),
                            !point->getAllowCaptureByTeam(captureTeam),
                            cachePresent, localCaptureActive);
                    }
                }
            }

            rotateIcon = false;
            if (team == 1)
                m_field070[2] += point->getAreaValueTeam1();
            else if (team == 2)
                m_field070[1] += point->getAreaValueTeam2();

            if (local->getVehicle()) {
                const hfe::Mat4& pointTransform =
                    object->getAbsoluteTransformation();
                const hfe::Mat4& vehicleTransform =
                    local->getVehicle()->getAbsoluteTransformation();
                std::vector<hfe::world::IObject*> nearby;
                const hfe::Vec3 radiusOffset = pointTemplate->getRadiusOffset();
                const hfe::Vec3 centre = {
                    pointTransform.m[12] + radiusOffset.x,
                    pointTransform.m[13] + radiusOffset.y,
                    pointTransform.m[14] + radiusOffset.z
                };
                hfe::world::objectManager->getObjectsFromGrid(
                    centre, pointTemplate->getRadius(), nearby, nullptr);

                bool insideControlPoint = false;
                hfe::world::IObject* const vehicle = local->getVehicle();
                if (std::find(nearby.begin(), nearby.end(), vehicle) != nearby.end())
                    insideControlPoint = local->getIsInsideCP();

                const hfe::Vec3 pointPosition = {
                    pointTransform.m[12], pointTransform.m[13], pointTransform.m[14]
                };
                const hfe::Vec3 vehiclePosition = {
                    vehicleTransform.m[12], vehicleTransform.m[13], vehicleTransform.m[14]
                };
                if (positionsWithinRadius(&pointPosition, &vehiclePosition,
                                          closestDistance)
                        && !pointTemplate->getUnableToChangeTeam()
                        && local->getIsAlive()
                        && insideControlPoint) {
                    closestTeam = team;
                    closestDistance = static_cast<float>(
                        controlPointDistance(vehiclePosition, pointPosition));
                }
            }
        }

        const hfe::Mat4& transform = object->getAbsoluteTransformation();
        icon.rotation = rotateIcon
            ? m_field76c - static_cast<float>(
                std::atan2(transform.m[8], transform.m[10]))
            : 0.0f;
        projectWorldPosition(
            hfe::Vec3{transform.m[12], transform.m[13], transform.m[14]},
            &icon);
        addMapIcon(iconType, icon);
    }

    if (closestTeam != m_fielda70) {
        m_fielda70 = closestTeam;
        hfe::eventManager->postEvent(hfe::ECHud, 1u, this, 0.0f);
    }
}
// An order produces a destination marker and, while valid, a line from the squad leader's vehicle.

// The squad leader's vehicle networkable id, or -1. A squad index in [0, 10) asks the
// squad manager for that squad's leader (local team); anything else takes the local player when
// alive, otherwise the local squad's leader. Only a live leader counts.
int Bf2MenuMapNode::getSquadLeaderNetworkId(int squad)
{
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
    int playerId = -1;
    if (!local)
        return -1;
    hfe::ISquadManager* const squads = static_cast<hfe::ISquadManager*>(hfe::squadManager);
    if (squad > -1 && squad < 10) {
        hfe::world::IPlayer* const leader = hfe::world::playerManager->getPlayerFromId(
            squads->getSquadLeader(local->getTeam(), squad));                       // +3C
        if (leader && leader->getIsAlive())
            playerId = leader->getId();
    } else if (local->getIsAlive()) {
        playerId = local->getId();
    } else {
        const int squadId = local->getSquadId();                                     // asked first
        hfe::world::IPlayer* const leader = hfe::world::playerManager->getPlayerFromId(
            squads->getSquadLeader(local->getTeam(), squadId));
        if (leader && leader->getIsAlive())
            playerId = leader->getId();
    }
    hfe::world::IPlayer* const player = hfe::world::playerManager->getPlayerFromId(playerId);
    if (!player)
        return -1;
    hfe::world::IObject* const vehicle = player->getVehicle();                       // PE +44
    if (!vehicle)
        return -1;
    hfe::io::INetworkable* const networkable = vehicle->getNetworkable();            // PE +D4
    if (!networkable)
        return -1;
    return networkable->m_networkableId;
}

// A networkable's map position: world (x, -z) times the scale; (-1, -1) when the id
// resolves to nothing. `this` is not read.
hfe::Vec2 Bf2MenuMapNode::networkIdToMap(int networkId, float scale)
{
    hfe::Vec2 out{-1.0f, -1.0f};
    hfe::world::IObject* const object =
        hfe::io::networkManager->getObject(static_cast<unsigned short>(networkId));
    if (!object)
        return out;
    const hfe::Mat4& transform = object->getAbsoluteTransformation();                 // PE +80
    out.x = transform.m[12] * scale;
    out.y = -transform.m[14] * scale;
    return out;
}

// Complete a line record: resolve either end from its networkable when one is given,
// invalidate a degenerate end, then place the line - midpoint, length (times the zoom scale's y)
// and angle. The same arithmetic makeWorldSupplyLineRecord carries for its -1/-1 case.
// An explicit start ends up invalid only when no start networkable was given; the end is
// tested either way. Both tests are the shipped `<= epsilon` - a NaN end stays valid.
void Bf2MenuMapNode::completeLineRecord(float scale, MapLineRecord* line)
{
    if (line->networkIdA > -1)
        line->a = networkIdToMap(line->networkIdA, scale);
    if (line->networkIdB > -1)
        line->b = networkIdToMap(line->networkIdB, scale);
    const hfe::Vec2 a = line->a;
    const hfe::Vec2 b = line->b;
    const float epsilon = (std::numeric_limits<float>::epsilon)();                   // ds:92FE78
    if ((line->networkIdA == -1 && std::fabs(line->a.y) + std::fabs(line->a.x) <= epsilon)
            || std::fabs(line->b.y) + std::fabs(line->b.x) <= epsilon)
        line->valid = false;
    if (!line->valid)
        return;
    const float dy = b.y - a.y;
    const float dx = b.x - a.x;
    double angle = static_cast<double>(-1.5707964f)
        - std::atan2(static_cast<double>(dy), static_cast<double>(dx));
    if (angle + static_cast<double>(3.1415927f) < 0.0)
        angle += static_cast<double>(6.2831855f);
    line->line.rotation = static_cast<float>(angle);                                  // +44
    const hfe::Vec2 zoom = getZoomScale(false);
    line->line.size.y = static_cast<float>(std::sqrt(
        static_cast<double>(dx) * dx + static_cast<double>(dy) * dy) * zoom.y);       // +40
    line->line.position.x = (b.x + a.x) * 0.5f;                                       // +24
    line->line.position.y = (b.y + a.y) * 0.5f;                                       // +28
}

// +100, 772F40. Loaded on first use through the System, then cached. No bound on `squad`.
int Bf2MenuMapNode::getSquadNumberIcon(int squad)
{
    if (m_squadNumberIcons[squad] == 0)
        m_squadNumberIcons[squad] = m_system->loadTexture(
            hfe::formatString("Ingame/Player/Icons/Minimap/mini_squadNumber_%d.tga", squad).c_str(),
            true);                                                                    // +8C
    return m_squadNumberIcons[squad];
}

// Adds the order line and destination marker to the map.
bool Bf2MenuMapNode::addOrderIcons(hfe::SquadOrder* order, int squad, int leaderNetworkId,
                                   int kind, const Rect* bounds)
{
    MapLineRecord line;
    line.networkIdA = 0;
    line.networkIdB = 0;
    line.squad = 0;
    line.field10 = false;
    line.valid = true;
    line.a = hfe::Vec2{0.0f, 0.0f};
    line.b = hfe::Vec2{0.0f, 0.0f};
    MapIconRecord marker;
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
    marker.size = hfe::Vec2{15.0f, 31.0f};
    marker.clampToVisibleMap = false;
    unsigned int type = 21;                              // commander order
    if (kind == 0) type = 20;                            // squad order
    else if (kind == 2) type = 19;                       // request
    if (!local)
        return true;
    if (!order)
        return true;
    for (int segment = 0; segment < 1; ++segment) {
        const hfe::Vec3& point = order->points[segment];
        if (point.x == -4000.0f && order->targetNetId == 0)
            break;
        line.networkIdA = leaderNetworkId;
        line.squad = local->getIsCommander() ? squad + 1 : -1;                        // PE +110
        if (segment != 0)
            worldCoordinatesToMap(order->points[segment - 1], line.a);
        line.networkIdB = order->targetNetId ? static_cast<int>(order->targetNetId) : -1;
        worldCoordinatesToMap(point, line.b);
        line.kind = segment ? 4 : order->orderId;
        line.field10 = true;
        line.valid = true;
        completeLineRecord(m_worldToMapScale, &line);
        line.line.size.y *= bounds->width;
        marker.position = line.b;
        line.line.size.x = static_cast<float>(m_field678);
        marker.rotation = 0.0f;
        line.line.rotation += m_field76c;
        styleOrderIcons(kind, &line, &marker, order->flag2C);                         // +130
        if (line.valid)
            addMapIcon(8, line.line);
        addMapIcon(type, marker);
    }
    return true;
}

// +130, 77D560. `kind` values outside 0..2 leave the marker's texture as it came. Nothing
// bounds the order id either: it indexes the colour table and the texture table directly, so a
// commander order of id 10 reads one past m_field8a8 - m_controlPointTextures[0], +904 - as shipped.
void Bf2MenuMapNode::styleOrderIcons(int kind, MapLineRecord* line, MapIconRecord* marker,
                                     bool active)
{
    if (!line)
        return;
    if (!marker)
        return;
    const float alpha = m_field6fc;                                                   // +6FC
    marker->color = Color{1.0f, 1.0f, 1.0f, alpha};
    const Color& typeColor = hfe::hud::mapOrderColors[line->kind];                    // ds:A1D3E0
    const float red = typeColor.r;
    const float green = typeColor.g;
    const float blue = typeColor.b;
    line->valid = true;
    if (kind == 1) {
        marker->size = hfe::Vec2{15.0f, 47.0f};
        if (line->kind == 3)
            marker->size = hfe::Vec2{15.0f, 15.0f};
        marker->texture = m_field8a8[13 + line->kind];                                // +8DC
    } else if (kind == 0) {
        marker->texture = m_field8a8[3 + line->kind];                                 // +8B4
    } else if (kind == 2) {
        line->valid = false;
        marker->size = hfe::Vec2{15.0f, 15.0f};
        marker->texture = m_field8a8[3 + line->kind];
        marker->color = Color{red, green, blue, alpha};
        // The request's pulse ring - the marker collectors' shape. Its rotation is never
        // written (773590 leaves it untouched), as shipped.
        MapIconRecord ring;
        const float pulse = m_fielda14 * 58.0f;
        ring.position = marker->position;
        ring.color = marker->color;
        const float base = (1.0f - m_fielda10 * 0.5f) * 58.0f;
        ring.texture = m_field928[0];                                                 // request_circle
        ring.clampToVisibleMap = false;
        ring.size = hfe::Vec2{base + pulse, base + pulse};
        addMapIcon(19, ring);
        if (line->squad > 0) {
            MapIconRecord number(*marker);
            number.texture = getSquadNumberIcon(line->squad);                         // +100
            number.size = hfe::Vec2{36.0f, 36.0f};
            addMapIcon(19, number);
        }
    }
    line->line.texture = m_field8a8[0];                                               // +8A8 map_Line
    if (!active) {
        line->line.color = Color{0.7f, 0.7f, 0.7f, 1.0f};                             // ds:97DE60
        line->line.color.a *= 0.8f;                                                   // ds:8E82B4
    } else {
        line->line.color = Color{red, green, blue, alpha};
    }
}

// filter 20. A commander: every squad selected in the commander screen that has
// players; anyone else: their own squad. getCommander() is asked twice per squad.
void Bf2MenuMapNode::collectSquadOrderIcons(const Rect* bounds)
{
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
    if (!local)
        return;
    if (local->getIsCommander()) {                                                    // PE +110
        for (int squad = 0; squad < 10; ++squad) {
            if (!hfe::hudManager->getCommander())                                     // +258
                continue;
            if (!hfe::hudManager->getCommander()->isSquadSelected(squad))             // +30
                continue;
            hfe::ISquadInfo* const info = static_cast<hfe::ISquadManager*>(hfe::squadManager)
                ->getSquadInfo(local->getTeam(), squad);                              // +28
            if (!info)
                continue;
            if (hfe::world::playerManager->getPlayersInSquad(local->getTeam(), squad).empty())
                continue;                                                             // PE +54
            hfe::SquadOrder* const order = info->getSquadOrders();                    // +10
            if (!order)
                continue;
            const int leader = getSquadLeaderNetworkId(squad);
            addOrderIcons(order, squad, leader, 0, bounds);                           // +114
        }
        return;
    }
    const int ownSquad = local->getSquadId();                                         // asked first
    hfe::ISquadInfo* const info = static_cast<hfe::ISquadManager*>(hfe::squadManager)
        ->getSquadInfo(local->getTeam(), ownSquad);
    if (!info)
        return;
    hfe::SquadOrder* const order = info->getSquadOrders();
    if (!order)
        return;
    const int leader = getSquadLeaderNetworkId(-1);
    addOrderIcons(order, local->getSquadId(), leader, 0, bounds);
}

// filter 21. collectSquadOrderIcons' exact twin over getCommanderOrders (+14), kind 1.
void Bf2MenuMapNode::collectCommanderOrderIcons(const Rect* bounds)
{
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
    if (!local)
        return;
    if (local->getIsCommander()) {
        for (int squad = 0; squad < 10; ++squad) {
            if (!hfe::hudManager->getCommander())
                continue;
            if (!hfe::hudManager->getCommander()->isSquadSelected(squad))
                continue;
            hfe::ISquadInfo* const info = static_cast<hfe::ISquadManager*>(hfe::squadManager)
                ->getSquadInfo(local->getTeam(), squad);
            if (!info)
                continue;
            if (hfe::world::playerManager->getPlayersInSquad(local->getTeam(), squad).empty())
                continue;
            hfe::SquadOrder* const order = info->getCommanderOrders();                // +14
            if (!order)
                continue;
            const int leader = getSquadLeaderNetworkId(squad);
            addOrderIcons(order, squad, leader, 1, bounds);
        }
        return;
    }
    const int ownSquad = local->getSquadId();
    hfe::ISquadInfo* const info = static_cast<hfe::ISquadManager*>(hfe::squadManager)
        ->getSquadInfo(local->getTeam(), ownSquad);
    if (!info)
        return;
    hfe::SquadOrder* const order = info->getCommanderOrders();
    if (!order)
        return;
    const int leader = getSquadLeaderNetworkId(-1);
    addOrderIcons(order, local->getSquadId(), leader, 1, bounds);
}

// filter 19. The four request slots (getRequest 0..3, +18), kind 2, for a commander (every
// squad with players - no commander-screen selection test here) or a squad leader (own squad).
// the commander'S loop passes local->getSquadId() and the local leader's vehicle, not the loop's
// squad: MSVC inlined 772AC0 there with -1. Every request lands on the commander's own line start.
void Bf2MenuMapNode::collectRequestIcons(const Rect* bounds)
{
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
    if (!local)
        return;
    hfe::ISquadManager* const squads = static_cast<hfe::ISquadManager*>(hfe::squadManager);
    if (local->getIsCommander()) {
        for (int squad = 0; squad < 10; ++squad) {
            hfe::ISquadInfo* const info = squads->getSquadInfo(local->getTeam(), squad);
            if (!info)
                continue;
            if (hfe::world::playerManager->getPlayersInSquad(local->getTeam(), squad).empty())
                continue;
            for (int request = 0; request <= 3; ++request) {
                hfe::SquadOrder* const order =
                    info->getRequest(static_cast<hfe::SquadOrderType>(request));      // +18
                if (!order)
                    continue;
                const int leader = getSquadLeaderNetworkId(-1);                       // inlined, -1
                addOrderIcons(order, local->getSquadId(), leader, 2, bounds);
            }
        }
        return;
    }
    if (!local->getIsSquadLeader())                                                   // PE +138
        return;
    const int ownSquad = local->getSquadId();
    hfe::ISquadInfo* const info = squads->getSquadInfo(local->getTeam(), ownSquad);
    if (!info)
        return;
    for (int request = 0; request <= 3; ++request) {
        hfe::SquadOrder* const order = info->getRequest(static_cast<hfe::SquadOrderType>(request));
        if (!order)
            continue;
        const int leader = getSquadLeaderNetworkId(-1);
        addOrderIcons(order, local->getSquadId(), leader, 2, bounds);
    }
}

// Collects player and soldier icons, refreshes team-presence flags and adds nearby heal/ammo overlays.
void Bf2MenuMapNode::collectIconFamily783440()
{
    const std::list<hfe::world::IPlayer*>& players =
        hfe::world::playerManager->getPlayers();
    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();

    m_playerTeamPresent[0] = false;
    m_playerTeamPresent[1] = false;

    for (std::list<hfe::world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it) {
        hfe::world::IPlayer* const player = *it;
        if (!player || !player->getVehicle() || !player->getDefaultVehicle())
            continue;

        int colorSide = 2;
        if (local) {
            const int localTeam = local->getTeam();
            colorSide = 2 - (player->getTeam() != localTeam);
        }

        MapIconRecord icon;
        icon.position = hfe::Vec2{0.0f, 0.0f};
        icon.color = hfe::hud::mapNeutralColors[colorSide];
        icon.size = hfe::Vec2{16.0f, 16.0f};
        icon.rotation = 0.0f;
        icon.texture = 0;
        icon.clampToVisibleMap = true;

        MapIconRecord headingIcon;
        headingIcon.position = hfe::Vec2{0.0f, 0.0f};
        headingIcon.color = Color{0.0f, 0.0f, 0.0f, 1.0f};
        headingIcon.size = hfe::Vec2{32.0f, 32.0f};
        headingIcon.rotation = 0.0f;
        headingIcon.texture = m_field93c[17];
        headingIcon.clampToVisibleMap = false;

        MapIconRecord statusIcon;
        statusIcon.position = hfe::Vec2{0.0f, 0.0f};
        statusIcon.color = Color{0.0f, 0.0f, 0.0f, 1.0f};
        statusIcon.size = hfe::Vec2{16.0f, 16.0f};
        statusIcon.rotation = 0.0f;
        statusIcon.texture = m_field93c[20];
        statusIcon.clampToVisibleMap = false;

        int type = classifyObjectForMap(player->getVehicle(), nullptr);
        const float iconAlpha = m_field6fc;
        bool rotate = true;
        bool heading = true;
        bool enabled = true;
        bool timed = false;
        bool localCanHeal = false;
        bool localCanResupply = false;
        bool lowStatus = false;
        float statusRatio = 0.0f;

        if (!player->getIsAlive())
            continue;
        if (player->getTeam() == 1)
            m_playerTeamPresent[0] = true;
        else if (player->getTeam() == 2)
            m_playerTeamPresent[1] = true;

        bool sameTeamOrNear = true;
        if (local) {
            const int localTeam = local->getTeam();
            if (player->getTeam() == localTeam) {
                sameTeamOrNear = true;
            } else if (local->getVehicle() && player->getVehicle()) {
                hfe::world::IObject* const playerVehicle = player->getVehicle();
                hfe::world::IObject* const localVehicle = local->getVehicle();
                const hfe::Vec3& playerPosition =
                    playerVehicle->getAbsolutePosition();
                const hfe::Vec3& localPosition =
                    localVehicle->getAbsolutePosition();
                sameTeamOrNear = positionsWithinRadius(
                    &localPosition, &playerPosition, 100.0f);
            }

            hfe::world::Kit* const kit = hfe::hudManager->getPlayerKit(local);
            if (kit) {
                hfe::world::IAbilityGLComp* const ability =
                    static_cast<hfe::world::IAbilityGLComp*>(
                        hfe::world::object_detail::LayoutAccess::abilityGLComponent(kit));
                if (ability) {
                    localCanHeal = ability->getGLInfo()->hasHealingAbility;
                    localCanResupply = ability->getGLInfo()->hasAmmoAbility;
                }
            }
        }

        if (!m_field674 || (m_field674 == 1 && !sameTeamOrNear))
            enabled = false;

        bool roleEnabled = false;
        hfe::world::ISoldier* const soldier =
            hfe::hudManager->getPlayerSoldier(player);
        if (soldier) {
            const unsigned int kitType =
                static_cast<unsigned int>(soldier->getKitType());
            if (kitType < 8u && m_field3c0.test(kitType))
                roleEnabled = true;
        }
        if (!roleEnabled
                && (player->getIsSquadLeader() || player->getIsCommander())
                && m_field3c0.test(7))
            roleEnabled = true;

        if (!roleEnabled) {
            enabled = false;
        } else if (type == 22 || type == 23) {
            if (enabled)
                enabled = isMapFilterEnabled(static_cast<unsigned int>(type));
        } else if (type == 37) {
            if (hfe::hudManager->isPlayerAlive(player))
                icon.texture = m_field9a8[2];
            icon.clampToVisibleMap = true;
            headingIcon.clampToVisibleMap = true;
            statusIcon.clampToVisibleMap = true;
            if (enabled)
                enabled = isMapFilterEnabled(37);
        } else {
            enabled = false;
        }

        hfe::world::IObject* const vehicle = player->getVehicle();
        hfe::io::INetworkable* const networkable =
            vehicle ? vehicle->getNetworkable() : nullptr;
        if (vehicle && networkable) {
            if (enabled && type != 37
                    && (m_field07e || m_field07c || m_field07d)) {
                hfe::world::Kit* const kit =
                    hfe::hudManager->getPlayerKit(player);
                if (kit) {
                    hfe::world::IAbilityGLComp* const ability =
                        static_cast<hfe::world::IAbilityGLComp*>(
                            hfe::world::object_detail::LayoutAccess::abilityGLComponent(kit));
                    if (ability) {
                        if (m_field07c
                                && ability->getGLInfo()->hasHealingAbility) {
                            MapIconRecord marker;
                            marker.texture = m_field9a8[3];
                            addTimedMarker(networkable->m_networkableId,
                                           hfe::Vec2{0.0f, 0.0f}, marker,
                                           0.0f, 0.0f, 0.0f, m_fielda2c, 4);
                        }
                        if (m_field07e
                                && ability->getGLInfo()->hasRepairingAbility) {
                            MapIconRecord marker;
                            marker.texture = m_field9a8[1];
                            addTimedMarker(networkable->m_networkableId,
                                           hfe::Vec2{0.0f, 0.0f}, marker,
                                           0.0f, 0.0f, 0.0f, m_fielda2c, 4);
                        }
                        if (m_field07d
                                && ability->getGLInfo()->hasAmmoAbility) {
                            MapIconRecord marker;
                            marker.texture = m_field9a8[0];
                            addTimedMarker(networkable->m_networkableId,
                                           hfe::Vec2{0.0f, 0.0f}, marker,
                                           0.0f, 0.0f, 0.0f, m_fielda2c, 4);
                        }
                    }
                }
            }

            hfe::world::IObject* const defaultVehicle =
                player->getDefaultVehicle();
            if (player->getVehicle() == defaultVehicle) {
                if (type == 23) {
                    if (m_satelliteActive)
                        updateSatelliteTarget(vehicle,
                                              networkable->m_networkableId,
                                              m_fielda94);
                    if (!m_uavObjects.empty() && player->getIsAlive()
                            && !player->getIsManDown())
                        updateUavTarget(vehicle, networkable->m_networkableId,
                                        m_field93c[19]);
                }

                if (type != 37 && enabled && !player->getIsMuted()
                        && player->getIsTalking()) {
                    populatePlayerMapIcon(player, &icon, &heading, &rotate,
                                          m_showKitsOnMap,
                                          m_field3c0.test(7), false);
                    addTimedMarker(networkable->m_networkableId,
                                   hfe::Vec2{0.0f, 0.0f}, icon,
                                   m_fielda2c, m_fielda24, m_fielda28,
                                   0.0f, 1);
                }

                populatePlayerMapIcon(player, &icon, &heading, &rotate,
                                      m_showKitsOnMap,
                                      m_field3c0.test(7), false);
                timed = resolveTimedMarker(networkable->m_networkableId,
                                           player, &icon, &rotate,
                                           &heading, &type);
            }
        }

        if (!enabled && !m_field684 && !timed)
            continue;

        if (!timed) {
            populatePlayerMapIcon(player, &icon, &heading, &rotate,
                                  m_showKitsOnMap,
                                  m_field3c0.test(7), false);
        }

        if (icon.texture == m_field9a8[3]
                || icon.texture == m_field9a8[1]
                || icon.texture == m_field9a8[0]) {
            headingIcon.texture = m_field9a8[4];
            headingIcon.size = hfe::Vec2{16.0f, 16.0f};
            heading = true;
        }

        int drawType = type;
        if (type != 37 && type != 36 && type != 25 && type != 24) {
            if (icon.texture == m_field9a8[1])
                drawType = 27;
            else if (icon.texture == m_field9a8[0])
                drawType = 28;
            else if (icon.texture == m_field9a8[2]
                     || icon.texture == m_field9a8[3])
                drawType = 26;
            else if (timed)
                drawType = 39;
        }

        if (localCanHeal || localCanResupply) {
            if (local->getVehicle()) {
                hfe::world::IObject* const targetVehicle =
                    player->getDefaultVehicle();
                if (targetVehicle) {
                    hfe::world::IObject* const localVehicle =
                        local->getVehicle();
                    const hfe::Vec3& localPosition =
                        localVehicle->getAbsolutePosition();
                    const hfe::Vec3& targetPosition =
                        targetVehicle->getAbsolutePosition();
                    if (positionsWithinRadius(&targetPosition, &localPosition,
                                              150.0f)) {
                        hfe::world::IArmorGLComp* armor = nullptr;
                        if (localCanHeal)
                            armor = static_cast<hfe::world::IArmorGLComp*>(
                                hfe::world::object_detail::LayoutAccess::armorComponent(
                                    targetVehicle));
                        if (localCanHeal && armor) {
                            const float hitPoints = armor->getHitPoints();
                            const float maxHitPoints = armor->getMaxHitPoints();
                            statusRatio = hitPoints / maxHitPoints;
                            lowStatus = statusRatio < 0.5f;
                        } else if (localCanResupply) {
                            hfe::world::IPlayerControlObject* const pco =
                                hfe::world::findOwningPlayerControlObject(
                                    targetVehicle);
                            if (pco && pco->queryInterface(
                                           hfe::world::IID_ISoldier)) {
                                hfe::world::IObject* const activeWeapon =
                                    pco->getActiveWeapon(0);
                                hfe::world::IWeaponObject* const weapon =
                                    activeWeapon
                                    ? static_cast<hfe::world::IWeaponObject*>(
                                        activeWeapon->queryInterface(
                                            hfe::world::IID_IWeaponObject))
                                    : nullptr;
                                if (weapon && weapon->getMagSize() >= 0
                                        && weapon->getMaxNrOfMags() >= 0) {
                                    const int currentMags =
                                        weapon->getCurrentNrOfMags();
                                    const int maxMags = weapon->getMaxNrOfMags();
                                    statusRatio =
                                        (static_cast<float>(currentMags) - 1.0f)
                                        / static_cast<float>(maxMags);
                                    lowStatus = statusRatio < 0.5f;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (!timed) {
            colorMapIcon(player, &icon, 15, true);
            icon.color.a = iconAlpha;
        }

        hfe::world::IObject* const renderVehicle = player->getVehicle();
        const hfe::Mat4& transform =
            renderVehicle->getAbsoluteTransformation();
        float rotation = 0.0f;
        if (rotate) {
            if (drawType == 37 && player->getCamera()) {
                const hfe::Mat4& camera =
                    player->getCamera()->getTransformation(0.0f);
                rotation = m_field76c - static_cast<float>(
                    std::atan2(camera.m[8], camera.m[10]));
            } else {
                rotation = m_field76c - static_cast<float>(
                    std::atan2(transform.m[8], transform.m[10]));
            }
        }
        if (heading)
            headingIcon.rotation = rotation;
        else
            icon.rotation = rotation;

        projectWorldPosition(
            hfe::Vec3{transform.m[12], transform.m[13], transform.m[14]},
            &icon);

        if (m_fielda7b) {
            if (lowStatus) {
                float clampedRatio = statusRatio;
                if (clampedRatio > 1.0f)
                    clampedRatio = 1.0f;
                else if (clampedRatio < 0.0f)
                    clampedRatio = 0.0f;
                statusIcon.position = icon.position;
                statusIcon.color = Color{
                    1.0f, clampedRatio, 0.0f, icon.color.a};
                addMapIcon(static_cast<unsigned int>(drawType), statusIcon);
            }
            if (heading) {
                headingIcon.position = icon.position;
                headingIcon.color.a = icon.color.a;
                addMapIcon(static_cast<unsigned int>(drawType), headingIcon);
            }
        }
        addMapIcon(static_cast<unsigned int>(drawType), icon);
    }
}
// Adds the local-player arrow when the vehicle or kit filter allows it.
// Requires a local player. Position follows the vehicle; heading follows the commander vehicle or camera.
// Kit and vehicle flags share native storage but represent separate filter tables.
void Bf2MenuMapNode::collectLocalPlayerIcon()
{
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();

    MapIconRecord icon;
    icon.position = hfe::Vec2{0.0f, 0.0f};
    icon.color = Color{1.0f, 1.0f, 1.0f, 1.0f};
    icon.size = hfe::Vec2{50.0f, 70.0f};
    icon.rotation = 0.0f;
    icon.texture = m_field93c[18];                                   // +984
    icon.clampToVisibleMap = true;

    hfe::world::IObject* const vehicle = local->getVehicle();        // PE IPlayer +0x44
    bool byVehicleType = false;
    if (vehicle && vehicle != local->getDefaultVehicle())            // PE +0xA0
    {
        hfe::world::IObjectComponent* const hud =                // asked again, then +0x54
            hfe::world::object_detail::LayoutAccess::vehicleHudComponent(local->getVehicle());
        if (hud)
        {
            const int type = static_cast<hfe::world::IVehicleHudComp*>(hud)
                                 ->getHudInfo()->vehicleType;        // +0x64, info +0xC0
            if (type >= 0 && static_cast<unsigned int>(type) < 7)
                byVehicleType = m_field3c0[static_cast<std::size_t>(type) + 32];
        }
    }

    if (!byVehicleType)
    {
        hfe::world::ISoldier* const soldier = hfe::hudManager->getPlayerSoldier(local); // +0x94
        if (!soldier) return;
        const int kit = soldier->getKitType();                       // ISoldier +0x58
        if (static_cast<unsigned int>(kit) >= 8) return;
        if (!m_field3c0[static_cast<std::size_t>(kit)])
        {
            if (!m_field3c0[7]) return;
            if (!local->getIsSquadLeader() && !local->getIsCommander()) return; // +0x138 / +0x110
        }
    }

    if (!local->getCamera()) return;                                 // PE +0x17C
    if (!vehicle) return;

    const hfe::Mat4& vehicleTransform = vehicle->getAbsoluteTransformation();   // PE +0x80
    const hfe::Mat4& heading = m_commanderMode                                  // +691
        ? vehicleTransform
        : local->getCamera()->getTransformation(0.0f);               // asked again, then +0x10

    projectWorldPosition(hfe::Vec3{vehicleTransform.m[12], vehicleTransform.m[13],
                                   vehicleTransform.m[14]}, &icon);

    icon.color = Color{1.0f, 1.0f, 1.0f, m_field6fc};                // +6FC
    icon.rotation = m_field76c - std::atan2(heading.m[8], heading.m[10]);       // +76C
    addMapIcon(6, icon);
}
// Adds icons for pending squad requests.
void Bf2MenuMapNode::collectSquadRequestIcons()
{
    MapIconRecord icon;                                                     // 773590, inlined
    const std::list<hfe::world::IPlayer*>& players =
        hfe::world::playerManager->getPlayers();                             // PE +40
    hfe::world::playerManager->getLocalHumanPlayer();                        // PE +30, discarded
    for (std::list<hfe::world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it) {
        hfe::world::IPlayer* const player = *it;
        if (!player)
            continue;
        icon.color.a = m_field6fc;                                           // +6FC
        hfe::world::IObject* const vehicle = player->getVehicle();           // PE +44
        if (vehicle) {
            const hfe::Mat4& transform = vehicle->getAbsoluteTransformation(); // PE +80
            icon.rotation = 0.0f;
            projectWorldPosition(hfe::Vec3{transform.m[12], transform.m[13], transform.m[14]},
                                 &icon);
        }
        const int type = classifyObjectForMap(player->getVehicle(), nullptr); // 778680, asked again
        if (type == 17 || type == 22)
            addSquadRequestIcon(38, player, &icon);
    }
}

// 77E420. [INFERRED] name. The marker: a 30x30 "mini_Squad_request" at the record's position, in
// the request palette's first colour for the squad's type and the record's alpha - only for a
// squad leader whose squad's request type (+F4) is exactly 1, and only while +A18 is set.
// +F4 is asked twice, for the test and again for the palette row. The retail record is raw
// stack: its clamp byte is never written, which the default constructor reproduces.
void Bf2MenuMapNode::addSquadRequestIcon(unsigned int type, hfe::world::IPlayer* player,
                                         const MapIconRecord* icon)
{
    if (!player)
        return;
    if (!player->getIsSquadLeader())                                        // PE +138
        return;
    if (getSquadRequestType(player->getSquadId()) != 1)                     // PE +108, then +F4
        return;
    if (!m_fielda18)                                                        // +A18
        return;
    MapIconRecord marker;
    marker.position = icon->position;
    marker.texture = m_field89c;                                            // +89C mini_Squad_request
    const Color& row =
        hfe::hud::mapSquadRequestColors[getSquadRequestType(player->getSquadId())][0];
    marker.color = Color{row.r, row.g, row.b, icon->color.a};              // ds:A1CB68, 32-byte rows
    marker.size = hfe::Vec2{30.0f, 30.0f};
    marker.rotation = 0.0f;
    addMapIcon(type, marker);
}

namespace {

// [INFERRED] Predicate accepting objects with a remote-controlled-object template.
class StrategicRemoteControlledPredicator : public hfe::world::ObjectPredicator
{
public:
    StrategicRemoteControlledPredicator() : m_unused04(0), m_flags(0) {}

    bool predicate(hfe::world::IObject* object) override
    {
        if (!object || !object->getTemplate())
            return false;
        return const_cast<hfe::world::IObjectTemplate*>(object->getTemplate())
                   ->queryImplementation(
                       hfe::world::CID_RemoteControlledObjectTemplate) != 0;
    }

    unsigned int getFlags() const override { return m_flags; }
    bool performCulling(hfe::world::IObject*) override { return true; }

private:
    unsigned int m_unused04;
    unsigned int m_flags;
};

} // namespace

// The strategic-object landmark pass. Every map value is the real
// StrategicObjectHolder assembled by GameLogic; the first piece supplies the
// component/icons and orientation while the holder supplies the averaged world
// position. A remote-controlled descendant suppresses enemy landmarks and also
// disables rotation for a friendly one.
void Bf2MenuMapNode::collectIconFamily77E500()
{
    if (!hfe::gameLogic)
        return;

    typedef std::map<std::string, hfe::world::StrategicObjectHolder*>
        StrategicObjectMap;
    StrategicObjectMap* const strategicObjects =
        hfe::gameLogic->getStrategicObjectMap();
    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();

    for (StrategicObjectMap::iterator it = strategicObjects->begin();
         it != strategicObjects->end(); ++it) {
        hfe::world::StrategicObjectHolder* const holder = it->second;
        hfe::world::IObject* const object = holder->getObjectByIndex(0);
        if (!local || !object)
            continue;

        hfe::world::IStrategicObjectComp* const strategicComponent =
            static_cast<hfe::world::IStrategicObjectComp*>(
                object->getStrategicObjectComponent());
        if (!strategicComponent)
            continue;

        std::vector<hfe::world::IObject*> remoteControlledObjects;
        StrategicRemoteControlledPredicator remoteControlledPredicator;
        hfe::world::getChilds(object, remoteControlledObjects,
                              &remoteControlledPredicator, nullptr);

        bool rotate = true;
        if (!remoteControlledObjects.empty()
                && remoteControlledObjects.front()) {
            rotate = false;
            const int localTeam = local->getTeam();
            const int remoteTeam =
                static_cast<hfe::world::RemoteControlledObject*>(
                    remoteControlledObjects.front())->getTeam();
            if (remoteTeam != localTeam)
                continue;
        }

        const int texture = holder->getNrDestroyedObjects() > 0
            ? strategicComponent->getDestroyedIconPtr()
            : strategicComponent->getIntactIconPtr();
        if (texture <= 0)
            continue;

        MapIconRecord icon;
        icon.position = hfe::Vec2{0.0f, 0.0f};
        icon.color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        icon.size = hfe::Vec2{19.0f, 19.0f};
        icon.rotation = 0.0f;
        icon.texture = texture;
        icon.clampToVisibleMap = false;
        icon.color.a = m_field6fc;

        projectWorldPosition(holder->getPosition(), &icon);
        if (rotate) {
            const hfe::Mat4& transform = object->getAbsoluteTransformation();
            icon.rotation = m_field76c - static_cast<float>(
                std::atan2(transform.m[8], transform.m[10])) + 1.5707964f;
        }
        addMapIcon(16u, icon);
    }
}
// Adds spawn-point icons with selection, squad and active-state styling.
// Posts help once per matching active group. Team-presence flags are set here but are not cleared.
void Bf2MenuMapNode::collectSpawnGroupIcons()
{
    hfe::SpawnManager* const manager =
        static_cast<hfe::SpawnManager*>(hfe::spawnManager);         // ds:0x9E5680
    if (!manager)
        return;

    // both team lists are fetched before either is tested, as shipped
    const std::list<int>& team1 = manager->getGroupsForTeam(1);     // ELF +0x2C
    const std::list<int>& team2 = manager->getGroupsForTeam(2);
    if (!m_playerTeamPresent[0] && team1.empty()) m_field09c = true;
    if (!m_playerTeamPresent[1] && team2.empty()) m_field09d = true;

    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
    if (!local)
        return;

    const std::list<int>& groups = manager->getGroupsForPlayer(local);   // ELF +0x30
    for (std::list<int>::const_iterator it = groups.begin(); it != groups.end(); ++it)
    {
        hfe::ISpawnGroup* const group = manager->getSpawnGroup(*it);     // ELF +0x28
        if (!group)
            continue;

        MapIconRecord icon;
        icon.position.x = 0.0f;
        icon.position.y = 0.0f;
        icon.color.r = 1.0f;                       // ds:0x97E1A0, color97e1a0
        icon.color.g = 1.0f;
        icon.color.b = 1.0f;
        icon.color.a = 1.0f;
        icon.size.x = 16.0f;
        icon.size.y = 16.0f;
        icon.rotation = 0.0f;
        icon.clampToVisibleMap = false;

        const bool selected = (*it == m_spawnGroup);                     // +9A4
        icon.texture = selected
            ? m_field93c[9 + (group->getActive(false) ? 1 : 0)]          // +960
            : m_field93c[5 + (group->getActive(false) ? 1 : 0)];         // +950

        // getSquad() twice, as shipped
        if (group->getSquad() == local->getSquadId() && group->getSquad() > 0)
        {
            if (group->getActive(false))
                hfe::hudManager->enableVOHelpMessage(0x38);              // +F8

            icon.texture = selected
                ? m_field93c[11 + (group->getActive(false) ? 1 : 0)]     // +968
                : m_field93c[7 + (group->getActive(false) ? 1 : 0)];     // +958
        }

        icon.color.a = m_field708[0];               // the map fade update() animates
        projectWorldPosition(group->getPos(), &icon);
        addMapIcon(31, icon);
    }
}
void Bf2MenuMapNode::collectIconFamily7820E0()
{
    // Kind-three timed markers are the free-position map family. The
    // local-player query is deliberately outside the loop and unguarded, just
    // as shipped; it selects the neutral/enemy palette entry used as the seed
    // before populateNetworkIcon gets a chance to replace the record.
    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();

    for (std::vector<MapTimedMarker>::const_iterator marker =
             m_timedMarkers.begin();
         marker != m_timedMarkers.end(); ++marker) {
        if (marker->kind != 3 || !m_fielda18)
            continue;

        MapIconRecord icon;
        icon.position = hfe::Vec2{0.0f, 0.0f};
        icon.color = hfe::hud::mapObjectColors[local ? 1 : 0];
        icon.size = hfe::Vec2{0.0f, 0.0f};
        icon.rotation = 0.0f;
        icon.texture = 0;
        icon.clampToVisibleMap = false;

        if (!populateNetworkIcon(marker->networkId, &icon, false)) {
            if (marker->texture) {
                icon.size = marker->size;
                icon.texture = marker->texture;
            } else {
                icon.texture = m_field9a8[10];
            }
        }

        icon.color.a = m_field6fc;
        projectWorldPosition(
            hfe::Vec3{marker->position.x, 0.0f, marker->position.y}, &icon);
        addMapIcon(30u, icon);
    }
}

// PE emits this as the shared member boundary used by the UAV update
// and the status/team removals below; its complete body is map::erase(key).
void Bf2MenuMapNode::removeUavObject(unsigned int networkId)
{
    m_uavObjects.erase(networkId);
}

void Bf2MenuMapNode::collectIconFamily77FC30()
{
    // The map is deliberately re-resolved through ObjectManager by
    // network id; its stored IObject* is not read by this painter.
    typedef std::map<unsigned int, hfe::world::IObject*> UavObjectMap;

    UavObjectMap::iterator it = m_uavObjects.begin();
    MapIconRecord icon;
    MapIconRecord centreIcon;
    float alpha = m_field6fc;

    // The returned address is deliberately unused, but the lazy local-static
    // initialization at 6D8F10 still occurs here in retail.
    (void)zeroMarkerPosition();

    while (it != m_uavObjects.end()) {
        // Retail repeats its size test at the top of every trip through the
        // loop, independently of the iterator/end comparison.
        if (m_uavObjects.empty()) return;

        hfe::Vec3 position;
        int radius = 0;
        float rotation = 0.0f;
        int texture = 0;
        bool emit = false;
        bool emitCentre = false;

        if (it->first == 0xffffffffu) {
            if (m_uavActive) {
                position = m_uavPosition;
                radius = m_uavRadius;
                rotation = m_fieldabc;
                texture = m_uavTexture;
                emit = true;
                emitCentre = true;

                centreIcon.size = hfe::Vec2{15.0f, 15.0f};
                centreIcon.rotation = 0.0f;
                centreIcon.texture = m_uavIcon;
                centreIcon.clampToVisibleMap = false;

                const float ratio = m_fieldaac / m_uavActiveTime;
                if (ratio > 0.9f)
                    alpha = (1.0f - (ratio - 0.9f) * 10.0f) * m_field6fc;
                else if (ratio < 0.1f)
                    alpha = (1.0f - (0.1f - ratio) * 10.0f) * m_field6fc;
                else
                    alpha = m_field6fc;
            } else {
                m_uavObjects.erase(0xffffffffu);
            }
        } else {
            hfe::world::IObject* const object =
                hfe::world::objectManager->getObjectFromGrid(it->first);
            if (object) {
                hfe::world::IAbilityGLComp* const ability =
                    static_cast<hfe::world::IAbilityGLComp*>(
                        hfe::world::object_detail::LayoutAccess::abilityGLComponent(
                            object));
                if (ability) {
                    position = object->getAbsolutePosition();
                    rotation = m_fieldabc;
                    texture = m_uavTexture;
                    radius = ability->getGLInfo()->radarRadius;
                    emit = true;

                    hfe::world::IArmorGLComp* const armor =
                        static_cast<hfe::world::IArmorGLComp*>(
                            hfe::world::object_detail::LayoutAccess::armorComponent(
                                object));
                    if (armor && armor->getStatus() == 1) {
                        removeUavObject(it->first);
                        emit = false;
                    }

                    hfe::world::IPlayerControlObject* const control =
                        static_cast<hfe::world::IPlayerControlObject*>(
                            object->queryInterface(
                                hfe::world::IID_IPlayerControlObject));
                    if (control) {
                        std::map<int, hfe::world::IPlayerControlObject*>::iterator
                            child = control->getPcos()->begin();
                        hfe::world::IPlayer* occupant = nullptr;
                        while (child != control->getPcos()->end()) {
                            occupant = child->second->getOccupingPlayer();
                            if (occupant) break;
                            ++child;
                        }

                        if (occupant) {
                            hfe::world::IPlayer* const local =
                                hfe::world::playerManager->getLocalHumanPlayer();
                            if (local) {
                                const int localTeam = local->getTeam();
                                const int occupantTeam = occupant->getTeam();
                                if (localTeam != occupantTeam) {
                                    // Shipped defect: if the armor arm erased the
                                    // node above, retail rereads the key through
                                    // that invalidated iterator before requesting
                                    // the second erase.  Preserve the stale read.
                                    removeUavObject(it->first);
                                    emit = false;
                                }
                            }
                        }
                    }
                }
            } else {
                // The key is reread after getObjectFromGrid returns, matching
                // the shipped iterator lifetime rather than caching it.
                m_uavObjects.erase(it->first);
            }
        }

        if (!emit) {
            // Every refusal/removal arm assigns begin() and then flows through
            // the for-loop increment, so a non-empty map resumes at its second
            // entry.  Avoid incrementing end() on modern STL implementations.
            it = m_uavObjects.begin();
            if (it != m_uavObjects.end()) ++it;
            continue;
        }

        icon.color = Color{1.0f, 1.0f, 1.0f, alpha};
        const float baseSize = static_cast<float>(radius) * 2.0f
            * m_worldToMapScale;
        const float zoomY = getZoomScale(false).y;
        icon.size.x = baseSize * zoomY;
        icon.size.y = baseSize * zoomY;
        const float nodeWidth = getSize().x;
        icon.size.x *= nodeWidth;
        icon.size.y *= nodeWidth;
        icon.rotation = rotation;
        icon.texture = texture;
        icon.clampToVisibleMap = false;
        projectWorldPosition(position, &icon);
        addMapIcon(32u, icon);

        if (emitCentre) {
            centreIcon.position = icon.position;
            centreIcon.color = icon.color;
            addMapIcon(32u, centreIcon);
        }

        ++it;
    }
}
// Adds airstrike, supply and vehicle-drop markers.
void Bf2MenuMapNode::collectAirstrikeIcon()
{
    const float pulse = m_fielda14 * 58.0f;                          // +A14
    const float base = (1.0f - m_fielda10 * 0.5f) * 58.0f;           // +A10
    MapIconRecord ring;
    ring.position = hfe::Vec2{0.0f, 0.0f};
    ring.color = hfe::hud::mapAirstrikeColors[2];                    // ds:A1C9B8, red
    ring.size = hfe::Vec2{base + pulse, base + pulse};
    ring.rotation = 0.0f;
    ring.texture = m_airstrikeTexture;                               // +B04
    ring.clampToVisibleMap = false;
    MapIconRecord marker;
    marker.color = hfe::hud::mapAirstrikeColors[2];
    marker.size = hfe::Vec2{15.0f, 15.0f};
    marker.rotation = 0.0f;
    marker.texture = m_artilleryIcon;                                // +B08
    marker.clampToVisibleMap = false;
    projectWorldPosition(m_airstrikePosition, &ring);                // 776AF0, +AF4..+AFC
    marker.position = ring.position;
    addMapIcon(33, ring);
    addMapIcon(33, marker);
}
void Bf2MenuMapNode::collectSuppliesIcon()
{
    const float pulse = m_fielda14 * 58.0f;
    const float base = (1.0f - m_fielda10 * 0.5f) * 58.0f;
    const Color white = {1.0f, 1.0f, 1.0f, 1.0f};                    // ds:0x97E1A0, color97e1a0
    MapIconRecord ring;
    ring.position = hfe::Vec2{0.0f, 0.0f};
    ring.color = white;
    ring.size = hfe::Vec2{base + pulse, base + pulse};
    ring.rotation = 0.0f;
    ring.texture = m_field928[0];                                    // +928 request_circle
    ring.clampToVisibleMap = false;
    MapIconRecord marker;
    marker.color = white;
    marker.size = hfe::Vec2{15.0f, 15.0f};
    marker.rotation = 0.0f;
    marker.texture = m_suppliesTexture;                              // +B24 icon_supplies
    marker.clampToVisibleMap = false;
    projectWorldPosition(m_suppliesPosition, &ring);                 // 776AF0, +B14..+B1C
    marker.position = ring.position;
    addMapIcon(34, ring);
    addMapIcon(34, marker);
}
void Bf2MenuMapNode::collectVehicleDropIcon(const MapVehicleDrop* drop)
{
    const float pulse = m_fielda14 * 58.0f;
    const float base = (1.0f - m_fielda10 * 0.5f) * 58.0f;
    const Color white = {1.0f, 1.0f, 1.0f, 1.0f};                    // ds:0x97E1A0, color97e1a0
    MapIconRecord ring;
    ring.position = hfe::Vec2{0.0f, 0.0f};
    ring.color = white;
    ring.size = hfe::Vec2{base + pulse, base + pulse};
    ring.rotation = 0.0f;
    ring.texture = m_field928[0];                                    // +928 request_circle
    ring.clampToVisibleMap = false;
    MapIconRecord marker;
    marker.color = white;
    marker.size = hfe::Vec2{15.0f, 15.0f};
    marker.rotation = 0.0f;
    marker.texture = drop->texture;                                  // the drop's own icon
    marker.clampToVisibleMap = false;
    projectWorldPosition(drop->position, &ring);
    marker.position = ring.position;
    addMapIcon(35, ring);
    addMapIcon(35, marker);
}

// 7753A0. [INFERRED] void return. Retail first gates on the scan-line texture,
// then performs the otherwise unguarded local-player query.  The scan strip is
// clipped by the map bounds but extends 30 pixels above and below them; its
// color is the mutable first map-alert palette entry.
void Bf2MenuMapNode::drawSatellite(System* system, const Rect* bounds,
                                   RenderContext* context)
{
    if (!m_fielda90) return;
    if (!hfe::world::playerManager->getLocalHumanPlayer()) return;

    Rect scan;
    scan.x = m_fieldb5c.x + m_satelliteScanX
           - static_cast<float>(m_satelliteLineSize);
    scan.y = bounds->y - 30.0f;
    scan.width = static_cast<float>(m_satelliteLineSize);
    scan.height = bounds->height + 60.0f;
    scan.z = 0.0f;

    Quad quad(&scan, bounds);
    colorQuad(&quad, &hfe::hud::mapAlertColors[0]);
    drawContextTexture(context, system, m_fielda90, &quad);
}
// The fourth argument is unused: retail clips every temporary text transform against the
// map bounds passed as the third argument. The initial GameLogic/map query deliberately precedes
// the zero-font return, and the later manager/data uses retain their shipped missing null guards.
void Bf2MenuMapNode::drawControlPointLabels(System* system, const NodeState* state,
                                            const Rect* bounds, const Rect*,
                                            RenderContext* context)
{
    typedef std::map<unsigned int, hfe::world::IObject*> ControlPointMap;
    ControlPointMap& points = *hfe::gameLogic->getControlPointMap();
    if (!m_fieldc64) return;

    Bf2ColorEffect colorEffect(m_fieldc68.r, m_fieldc68.g,
                               m_fieldc68.b, m_fieldc68.a);
    Bf2EffectNode effectNode(nullptr, &colorEffect);
    if (hfe::hudManager && hfe::hudManager->getInformationAdapter()) {
        Effect* const effect = effectNode.getEffectForHud();
        if (effect) {
            const float alpha = hfe::hudManager->getInformationAdapter()
                                    ->getFloat(std::string("MenuMapIconAlpha"));
            effect->setColor(m_fieldc68.r, m_fieldc68.g, m_fieldc68.b, alpha);
        }
    }

    Bf2TextNode textNode(nullptr);
    Bf2TransformNode transform(nullptr, 0, 0, 0, 0);
    Bf2CenterAlignedStyle style(nullptr, static_cast<int>(m_fieldc64));

    void* stringAllocation = ::operator new(sizeof(WstringData));
    WstringData* stringData = nullptr;
    if (stringAllocation) {
        const std::string empty("");
        const std::wstring wideEmpty = hfe::toWideString(empty);
        stringData = new(stringAllocation) WstringData(wideEmpty.c_str(), "");
    }
    void* typeAllocation = ::operator new(sizeof(IntData));
    IntData* const typeData = typeAllocation
        ? new(typeAllocation) IntData(0, nullptr) : nullptr;

    if (shouldUseSecondaryIconTexture()) style.setTexture(m_field928[1]);
    transform.setChild(1, &effectNode);
    effectNode.setChild(0, &textNode);
    textNode.setStyleForHud(&style);
    textNode.setStringDataForHud(stringData);
    textNode.setTypeDataForHud(typeData);

    std::vector<hfe::Vec2> occupiedPositions;
    for (ControlPointMap::iterator it = points.begin(); it != points.end(); ++it) {
        hfe::world::IObject* const object = it->second;
        if (!object) continue;

        hfe::world::IControlPoint* const point =
            static_cast<hfe::world::IControlPoint*>(
                object->queryInterface(hfe::world::IID_IControlPoint));
        if (!point || !point->getShowOnMinimap()) continue;

        hfe::world::IControlPointTemplate* const pointTemplate =
            static_cast<hfe::world::IControlPointTemplate*>(
                object->getTemplate()->queryInterface(
                    hfe::world::IID_IControlPointTemplate));
        if (!pointTemplate) continue;

        {
            const std::wstring localized = hfe::hudManager->getLocalizedString(
                std::string(pointTemplate->getControlPointName()));
            stringData->setWideString(m_system, localized.c_str());
        }
        (void)point->getTeam();

        hfe::Vec2 mapPosition{0.0f, 0.0f};
        hfe::Vec2 position{0.0f, 0.0f};
        projectWorldPosition(object->getAbsolutePosition(), &mapPosition);

        // 7721D0, the exact inverse of screenCoordinatesToMap (772170).
        position.x = (mapPosition.x - 1.0f) * m_fieldb5c.width
                   + m_fieldb5c.width * 0.5f + m_fieldb5c.x;
        position.y = (mapPosition.y + 1.0f) * m_fieldb5c.height
                   + m_fieldb5c.height * 0.5f + m_fieldb5c.y;

        const float centerX = bounds->x + bounds->width * 0.5f;
        const float centerY = bounds->y + bounds->height * 0.5f;
        const float x = position.x - centerX;
        const float y = position.y - centerY;
        const float rotatedX = std::sin(m_field76c) * y
                             + std::cos(m_field76c) * x;
        const float rotatedY = std::cos(m_field76c) * y
                             - std::sin(m_field76c) * x;
        position.x = centerX + rotatedX + 350.0f;
        position.y = centerY + rotatedY + 305.0f;

        const hfe::Vec2 offset = pointTemplate->getMinimapNameOffset();
        position.x += offset.x;
        position.y += offset.y;

        // Each adjustment is visible to the checks against later entries.
        for (std::vector<hfe::Vec2>::const_iterator occupied = occupiedPositions.begin();
             occupied != occupiedPositions.end(); ++occupied) {
            if (std::fabs(position.x - occupied->x) < 100.0f) {
                const float deltaY = position.y - occupied->y;
                if (std::fabs(deltaY) < 10.0f)
                    position.y = deltaY >= 1.0f
                        ? occupied->y + 10.0f : occupied->y - 10.0f;
            }
        }
        occupiedPositions.push_back(position);

        transform.setPositionForHud(position.x, position.y);
        transform.setSizeForHud(100.0f, 10.0f);
        const Rect labelBounds = {-400.0f, -300.0f, 800.0f, 600.0f, 0.0f};
        transform.paint(system, state, &labelBounds, bounds, context);
    }

    // Both allocations are dereferenced without guards in retail, including on the empty-map path.
    typeData->destroy();
    stringData->destroy();
}

// Map pointer selection helpers.
namespace {
// 4639B0 (135 B), the engine's in-place Vec3 normalise, as 774380's overhead arm calls it
// (`lea ecx,[ebp-38h]`). HudInformationLayer.cpp and BF2HudManager.cpp carry the same body, both
// file-local. [INFERRED] name.
// An already-unit vector (|lenSq - 1| < FLT_EPSILON, ds:89E4E8) is left alone; a degenerate one
// (|lenSq| < FLT_EPSILON) is zeroed and answers -20001; NaN normalises to NaN. The squared length
// is rounded to float (4639CC) before either test. The one caller here discards the answer.
int normalizeInPlace(hfe::Vec3& value)
{
    const float lengthSquared = static_cast<float>(
        static_cast<double>(value.x) * value.x +
        static_cast<double>(value.y) * value.y +
        static_cast<double>(value.z) * value.z);
    const double epsilon = 1.1920928955078125e-7;
    if (!(std::fabs(static_cast<double>(lengthSquared) - 1.0) < epsilon)) {
        if (std::fabs(static_cast<double>(lengthSquared)) < epsilon) {
            value.x = value.y = value.z = 0.0f;
            return -20001;
        }
        const double inverseLength = 1.0 / std::sqrt(static_cast<double>(lengthSquared));
        value.x = static_cast<float>(inverseLength * value.x);
        value.y = static_cast<float>(inverseLength * value.y);
        value.z = static_cast<float>(inverseLength * value.z);
    }
    return 0;
}
}

// Converts pointer coordinates to map coordinates using the supplied scales.
// The caller supplies reciprocal height then width; axis calculations preserve this ordering.
hfe::Vec2 Bf2MenuMapNode::mapPointerToMapCoordinates(hfe::Vec2 pointer, hfe::Vec2 scale)
{
    hfe::Vec2 out;
    out.x = 0.0f;                                   // 77439x, both cleared before the branch
    out.y = 0.0f;
    if (m_overheadZoomActive) {
        // Projects the commander's pointer from the overhead camera into world space.
        hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer(); // +0x30
        if (!local)
            return out;
        hfe::Vec3 hit = {0.0f, 0.0f, 0.0f};                              // [ebp-44h]
        hfe::Vec3 normal = {0.0f, 0.0f, 0.0f};                           // [ebp-5Ch]
        hfe::world::IObject* hitObject = 0;                              // [ebp-8]
        int hitPart = 0;                                                 // [ebp-4]
        const hfe::Mat4& eyeTransform =
            local->getCamera()->getTransformation(0.0f);                 // +0x17C, then +0x10(0)
        const hfe::Vec3 eye = {eyeTransform.m[12], eyeTransform.m[13],
                               eyeTransform.m[14]};                      // +0x30 / +0x34 / +0x38
        const hfe::Vec3 target = hfe::hudManager->screenToWorld(pointer); // +0x2F8, sret
        hfe::Vec3 ray = {target.x - eye.x, target.y - eye.y, target.z - eye.z};
        normalizeInPlace(ray);                                           // 4639B0, answer dropped
        const float height = hfe::hudManager->getCommanderCameraHeight(); // +0x358
        const double reachX = static_cast<double>(ray.x) * height;       // stays on the x87 stack
        const float reachY = ray.y * height;                             // 7744B9 fstp
        const float reachZ = ray.z * height;                             // 7744CA fstp
        const float farZ = static_cast<float>(reachZ * 1.5);             // 7744E7 fstp; 8E8CCC 1.5f
        const hfe::Vec3 rayEnd = {
            static_cast<float>(reachX * 1.5 + eye.x),
            static_cast<float>(static_cast<double>(reachY) * 1.5 + eye.y),
            static_cast<float>(static_cast<double>(farZ) + eye.z) };
        float fraction;                                                  // [ebp-0Ch], never read
        unsigned char material;                                          // [ebp+1Bh], never set
        if (!static_cast<hfe::world::IHeightmapCluster*>(hfe::world::heightmapCluster)
                 ->intersectRayInWorldCoords(eye, rayEnd, hit, fraction, normal, material,
                                             true))                      // +0x8C, push 1
            return out;
        const double dx = static_cast<double>(eye.x) - hit.x;
        const double dy = static_cast<double>(eye.y) - hit.y;
        const double dz = static_cast<double>(eye.z) - hit.z;
        const float length = static_cast<float>(std::sqrt(dz * dz + dy * dy + dx * dx));
        hfe::world::ObjectFlagPredicator predicator(0x100u);             // 71B930, [ebp-1Ch]
        const hfe::Vec3 direction = {ray.x * length, ray.y * length, ray.z * length};
        hfe::world::objectManager->intersectLine(hitObject, hit, normal, hitPart, eye, direction,
                                                 &predicator, false, false, false, true,
                                                 0u);                    // +0x68, answer dropped
        worldCoordinatesToMap(hit, out);
        return out;
    }
    out.x = scale.x * pointer.x + (m_field7e8.x - 0.5f);
    out.y = scale.y * pointer.y + m_field7e8.y + 0.5f;
    return out;
}

// Returns the pointer position in map coordinates.
hfe::Vec2 Bf2MenuMapNode::getPointerMapPosition(System* system)
{
    const hfe::Vec2& size = getSize();                                   // 659990, asked first
    const hfe::Vec2& nodePosition = getPosition();
    const float* const pointer = system->getPointerPosition();
    float relative[2];
    relative[0] = pointer[0] - ((size.x * 0.5f + nodePosition.x) - system->getWidth() * 0.5f);
    relative[1] = pointer[1] - ((size.y * 0.5f + nodePosition.y) - system->getHeight() * 0.5f);
    const float scaleX = 1.0f / m_fieldb5c.width;                        // +B64
    const float scaleY = 1.0f / m_fieldb5c.height;                       // +B68
    const float* const source = m_overheadZoomActive ? system->getPointerPosition()   // +7F8
                                                     : relative;
    return mapPointerToMapCoordinates(hfe::Vec2{source[0], source[1]},
                                      hfe::Vec2{scaleY, scaleX});        // 774380, crossed
}

// Map pickers choose the nearest eligible icon strictly inside both distance limits.

// The nearest request of the local team (squads 0..9, requests 0..3 through
// ISquadInfo +0x18). The winner's squad / request / order type 2 go to +B88 / +B8C / +B90, and its
// position is mapped into m_commanderPosition, for the request menu (mode 27) the caller opens.
// local->getTeam() is asked once per squad, not cached. getZoomScale is called once per
// candidate, for the radius test alone (m_field734 / zoom.y), exactly as pickSpawnGroup does.
bool Bf2MenuMapNode::pickSquadRequest(float scale, hfe::Vec2 position, float maxDistance)
{
    bool found = false;
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();   // +0x30
    if (!local)
        return found;
    for (int squad = 0; squad < 10; ++squad) {
        hfe::ISquadInfo* const info = static_cast<hfe::ISquadManager*>(hfe::squadManager)
            ->getSquadInfo(local->getTeam(), squad);                                       // +0x28
        if (!info)
            continue;
        for (int request = 0; request <= 3; ++request) {
            hfe::SquadOrder* const order =
                info->getRequest(static_cast<hfe::SquadOrderType>(request));               // +0x18
            if (!order)
                continue;
            const hfe::Vec3& world = order->points[0];                                     // +0x08
            const float dx = position.x - world.x * scale;
            const float dy = position.y + world.z * scale;
            const float distance = dy * dy + dx * dx;
            if (!(m_field734 / getZoomScale(false).y > distance))
                continue;
            if (!(distance < maxDistance))
                continue;
            maxDistance = distance;
            m_selectedRequestSquad = squad;                                                // +B88
            m_selectedRequestType = request;                                               // +B8C
            m_selectedRequestOrderType = static_cast<hfe::OrderType>(2);                   // +B90
            worldCoordinatesToMap(world, m_commanderPosition);
            found = true;
        }
    }
    return found;
}

// Returns the network id of the nearest eligible live enemy vehicle.
unsigned short Bf2MenuMapNode::pickEnemyVehicle(float scale, hfe::Vec2 position,
                                                float maxDistance)
{
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();   // +0x30
    if (!local || !hfe::gameLogic)
        return 0;
    // one expression: the manager's vtable is loaded before getTeam is asked (77B441), then the
    // manager pointer is re-read for the call (77B44C).
    const std::list<hfe::world::IPlayer*> players = hfe::world::playerManager->getPlayers(
        local->getTeam() == 1 ? 2 : 1);                                                    // PE +0x3C, client +0xE4
    unsigned short picked = 0;
    for (std::list<hfe::world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it) {
        hfe::world::IPlayer* const player = *it;
        if (!player || !player->getVehicle() || !player->getIsAlive()                      // +0x44 / +0x68
                || player->getIsManDown())                                                 // client +0xFC
            continue;
        hfe::world::IObject* const vehicle = player->getVehicle();                         // asked again
        const hfe::Mat4& transform = vehicle->getAbsoluteTransformation();                 // IObject PE +0x80
        const float dx = position.x - transform.m[12] * scale;
        const float dy = position.y + transform.m[14] * scale;
        const float distance = dy * dy + dx * dx;
        if (!(distance < 0.0016f))                                                         // ds:930320
            continue;
        if (!(distance < maxDistance))
            continue;
        hfe::io::INetworkable* const networkable = vehicle->getNetworkable();              // IObject PE +0xD4
        if (!networkable)
            continue;
        picked = networkable->m_networkableId;
        maxDistance = distance;
    }
    return picked;
}

// Returns the nearest player ID among occupied vehicles on any team, or -1 when none is close enough.
int Bf2MenuMapNode::pickPlayer(float scale, float x, float y, float maxDistance)
{
    const std::list<hfe::world::IPlayer*>& players = hfe::world::playerManager->getPlayers();  // PE +0x40
    int picked = -1;
    for (std::list<hfe::world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it) {
        hfe::world::IPlayer* const player = *it;
        if (!player || !player->getVehicle())                                              // +0x44
            continue;
        const int id = player->getId();                                                    // +0x50
        const hfe::Mat4& transform = player->getVehicle()->getAbsoluteTransformation();    // PE +0x80
        const float dx = x - transform.m[12] * scale;
        const float dy = y + transform.m[14] * scale;
        const float distance = dy * dy + dx * dx;
        if (!(distance < 0.0009f))                                                         // ds:930178
            continue;
        if (!(distance < maxDistance))
            continue;
        maxDistance = distance;
        picked = id;
    }
    return picked;
}

// Selects a spawn group within the maximum pointer distance.
bool Bf2MenuMapNode::pickSpawnGroup(float scale, hfe::Vec2 position, float maxDistance)
{
    if (m_fieldc78 > 132.0f && m_fieldc7c > 251.0f)    // 777BC9..777BED, ds:930180 / ds:93017C
        return true;

    hfe::SpawnManager* const manager =
        static_cast<hfe::SpawnManager*>(hfe::spawnManager);          // ds:0x9E5680
    if (!manager)
        return true;
    hfe::world::IPlayer* const player = hfe::world::playerManager->getLocalHumanPlayer();
    if (!player)
        return true;

    // PE +0x2C is this tree's +0x30: the client has one destructor slot,
    // so subsequent slots are four bytes lower than in the two-slot table.
    const std::list<int>& groups = manager->getGroupsForPlayer(player);
    m_spawnGroup = 0;                                                // 777C20, before the walk

    for (std::list<int>::const_iterator it = groups.begin(); it != groups.end(); ++it) {
        const int id = *it;
        hfe::ISpawnGroup* const group = manager->getSpawnGroup(id);   // PE +0x24
        if (!group)
            continue;
        if (!group->getActive(false))                                 // PE +0x28
            continue;
        const hfe::Vec3& groupPosition = group->getPos();             // PE +0x30

        // the Z term is added, not subtracted: the shipped code negates the component with
        // `fchs` and then subtracts, which is `position.y + p.z * scale`.
        const float dx = position.x - groupPosition.x * scale;
        const float dy = position.y + groupPosition.z * scale;
        const float distance = dy * dy + dx * dx;

        if (!(m_field730 / getZoomScale(false).y > distance))
            continue;
        if (!(distance < maxDistance))
            continue;
        maxDistance = distance;
        m_spawnGroup = id;
    }

    // +0x9A0 is the last word of the texture block the header calls m_field93c, and this is the
    // only thing in the image that writes it. Its role is [UNRESOLVED]; the store is shipped.
    m_field93c[25] = m_spawnGroup;
    hfe::hudManager->getKitSelection()->setSpawnGroup(m_spawnGroup);  // +0x240, then 7A1EB0
    return true;
}

// see the note at the top of this block for the four
// arms that stop.
bool Bf2MenuMapNode::onEvent1(System* system, const Rect*, const NodeState*,
                              int first, int, int)
{
    // 3.1 the not-expanded early block, 77B56D ----------------------------------------------
    // getMode() is asked five separate times here. Every one is a shipped virtual call.
    if (!m_field06c && first != 21) {
        if (first != 1)
            return false;
        if (hfe::hudManager->isCommanderShown() && !hfe::hudManager->isScoreboardMode()
                && hfe::hudManager->getMode() != 16) {
            hfe::hudManager->setMode(15, true);
            return false;
        }
        if (hfe::hudManager->getMode() == 20 || hfe::hudManager->getMode() == 27
                || hfe::hudManager->getMode() == 19) {
            hfe::hudManager->setMode(hfe::hudManager->getPreviousMode(), true);
            return false;
        }
        if (system->getPointerOwner() != this)
            return false;
        if (hfe::hudManager->getMode() == 16)
            hfe::hudManager->setMode(hfe::hudManager->getPreviousMode(), true);
        return false;
    }

    // 3.2 the geometry, 77B633 --------------------------------------------------------------
    const hfe::Vec2& size = getSize();                                // 659990, asked first
    const hfe::Vec2& nodePosition = getPosition();
    const float* const pointer = system->getPointerPosition();
    const float halfWidth = size.x * 0.5f;
    const float halfHeight = size.y * 0.5f;
    // The node's centre on screen, each kept as a float local (77B682 [ebp+8], 77B6AD [ebp-4]):
    // 3.3 adds the pointer offset back onto them.
    const float anchorX = nodePosition.x + halfWidth;
    const float anchorY = nodePosition.y + halfHeight;
    m_fieldc78 = pointer[0] - (anchorX - system->getWidth() * 0.5f);
    m_fieldc7c = pointer[1] - (anchorY - system->getHeight() * 0.5f);

    // Applies the selection to settings only on pointer release.
    if (first == 21) {
        hfe::hud::HudInformationAdapter* adapter = hfe::hudManager->getInformationAdapter();  // +0x244
        adapter->setInt(std::string("OrderMenuPosX"),                                        // +0x14
                        static_cast<int>(std::floor(static_cast<double>(anchorX + m_fieldc78))));
        adapter = hfe::hudManager->getInformationAdapter();
        adapter->setInt(std::string("OrderMenuPosY"),
                        static_cast<int>(std::floor(static_cast<double>(anchorY + m_fieldc7c))));
    }

    // 3.4 the hit rectangle, 77B792 ---------------------------------------------------------
    const float scaleX = 1.0f / m_fieldb5c.width;
    const float scaleY = 1.0f / m_fieldb5c.height;

    // Both shipped tests are `test ah,5 / jp`, i.e. not (a < b ordered) jumps out - so a NaN
    // coordinate is outside the map, which is what this spelling preserves.
    if (std::fabs(m_fieldc78) < halfWidth && std::fabs(m_fieldc7c) < halfHeight) {
        // 3.5 the map coordinates, 77B7D6 ---------------------------------------------------
        const float* const source = m_overheadZoomActive ? system->getPointerPosition()
                                                         : &m_fieldc78;
        // the pair is built {scaleY, scaleX}. See mapPointerToMapCoordinates.
        const hfe::Vec2 coordinates = mapPointerToMapCoordinates(
            hfe::Vec2{source[0], source[1]}, hfe::Vec2{scaleY, scaleX});
        m_commanderPosition.y = coordinates.y;                        // the Y store comes first
        m_commanderPosition.x = coordinates.x;
        const int mode = hfe::hudManager->getMode();                  // held for the rest

        if (first == 21) {
            if (hfe::hudManager->isCommanderShown() && !hfe::hudManager->isScoreboardMode()) {
                // Applies the commander's pointer-release action.
                const float requestDistance =
                    3.4028235e38f / (getZoomScale(false).y * 10.0f);
                if (pickSquadRequest(m_worldToMapScale,
                                     hfe::Vec2{m_commanderPosition.x, m_commanderPosition.y},
                                     requestDistance)) {
                    hfe::hudManager->setMode(27, true);
                    return false;
                }
                if (hfe::hudManager->getMode() == 22 || hfe::hudManager->getMode() == 23
                        || hfe::hudManager->getMode() == 25 || hfe::hudManager->getMode() == 24) {
                    hfe::hudManager->setMode(15, true);
                    return false;
                }
                const float vehicleDistance =
                    3.4028235e38f / (getZoomScale(false).y * 10.0f);
                const unsigned short spotted = pickEnemyVehicle(m_worldToMapScale,
                    hfe::Vec2{m_commanderPosition.x, m_commanderPosition.y},
                    vehicleDistance);
                if (spotted) {
                    hfe::hudManager->setSpottedObjectId(spotted);                 // +0x144
                    hfe::hudManager->setMode(21, true);
                    return false;
                }
                hfe::Vec3 world = {0.0f, 0.0f, 0.0f};
                mapCoordinatesToWorld(world, m_commanderPosition);
                hfe::hudManager->setSpottedPosition(world);                       // +0x148, by value
                hfe::hudManager->setSpottedObjectId(0);                           // +0x144
                hfe::hudManager->updateOrderPositionColor(world);                 // +0x338
                hfe::hudManager->setMode(21, true);
                return false;
            }
            // 3.7 the release with no commander view, 77BA18 --------------------------------
            if (mode == 17 || mode == 20) {
                hfe::Vec3 world = {0.0f, 0.0f, 0.0f};
                mapCoordinatesToWorld(world, m_commanderPosition);
                hfe::hudManager->setSpottedPosition(world);           // +0x148, by value
                hfe::hudManager->updateOrderPositionColor(world);     // +0x338
                if (mode == 17)
                    hfe::hudManager->setMode(20, true);               // mode 20 does not set it
                return false;
            }
            if (mode == 18) {
                hfe::hudManager->setMode(19, true);
                return false;
            }
            if (mode == 1 || mode == 21 || mode == 27)
                return false;
            hfe::hudManager->toggleSquadCommander();                  // +0x348
            return false;
        }

        // 3.8 the press, 77BB4E -----------------------------------------------------------
        if (first != 1)
            return false;

        // Spawn selection takes priority over other press modes and returns whether the event was consumed.
        if (m_field690)
            return pickSpawnGroup(m_worldToMapScale,
                                  hfe::Vec2{m_commanderPosition.x, m_commanderPosition.y},
                                  3.4028235e38f);                     // ds:9152C8 = FLT_MAX

        // Each of the four commander actions re-reads getMode(); the shipped code does not cache
        // it here even though `mode` is already in hand one line above.
        if (hfe::hudManager->getMode() == 22) {
            hfe::hudManager->artillery();                             // +0x2BC
            hfe::hudManager->setMode(15, true);
            return false;
        }
        if (hfe::hudManager->getMode() == 23) {
            hfe::hudManager->uav(false);                              // +0x2CC
            hfe::hudManager->setMode(15, true);
            return false;
        }
        if (hfe::hudManager->getMode() == 24) {
            hfe::hudManager->supplies(false);                         // +0x2C8
            hfe::hudManager->setMode(15, true);
            return false;
        }
        if (hfe::hudManager->getMode() == 25) {
            hfe::hudManager->vehicleDrop(false);                      // +0x2D0
            hfe::hudManager->setMode(15, true);
            return false;
        }

        if (hfe::hudManager->isCommanderShown()) {
            hfe::hudManager->setMode(15, true);
            hfe::hudManager->setCommanderCameraPosition(
                hfe::hudManager->getCommanderPosition(true));         // +0x2F4 then +0x2EC
        } else if (mode == 18 || mode == 17) {
            setViewCenter(hfe::Vec2{m_commanderPosition.x, m_commanderPosition.y});  // +0x11C
        }

        // 3.8b the player hit test, 77BCA5 (D2) ------------------------
        const float playerDistance = 3.4028235e38f / (getZoomScale(false).y * 10.0f);
        const int playerId = pickPlayer(m_worldToMapScale, m_commanderPosition.x,
                                        m_commanderPosition.y, playerDistance);

        // Handles the player's map selection.
        hfe::world::IPlayer* const hit = hfe::world::playerManager->getPlayerFromId(playerId);  // +0x20
        hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();   // +0x30
        if (local && hit) {
            const int localTeam = local->getTeam();                       // client +0xE4, asked first
            if (hit->getTeam() != localTeam)
                return false;
            if (hit->getIsSquadLeader()) {                                // client +0x138
                if (mode == 15) {
                    if (!hfe::hudManager->getCommander())                 // +0x258
                        return true;
                    if (hfe::hudManager->getFlag229()) {                  // +0x260
                        hfe::hud::HudCommander* const commander = hfe::hudManager->getCommander();
                        commander->toggleSquad(hit->getSquadId());        // +0x1C
                        return true;
                    }
                    if (hfe::hudManager->getFlag22A()) {                  // +0x264
                        hfe::hud::HudCommander* const commander = hfe::hudManager->getCommander();
                        commander->selectSquadRange(hit->getSquadId(), -1);   // +0x2C
                        return true;
                    }
                    hfe::hud::HudCommander* const commander = hfe::hudManager->getCommander();
                    commander->selectSingleSquad(hit->getSquadId());      // +0x18
                    return true;
                }
                if (mode == 18 || mode == 17) {
                    if (!hfe::hudManager->getSquad())                     // +0x254
                        return true;
                    const int squad = hit->getSquadId();                  // before the second ask
                    hfe::hudManager->getSquad()->toggleSquadSelection(squad);
                    return true;
                }
                return true;
            }
            if (mode == 15) {
                if (!hfe::hudManager->getCommander())
                    return true;
                hfe::hud::HudCommander* const commander = hfe::hudManager->getCommander();
                commander->toggleExpandedSquad(hit->getId());             // +0x20
                return true;
            }
            if (mode == 18 || mode == 17) {
                if (hfe::hudManager->getSquad()) {
                    const int id = hit->getId();                          // before the second ask
                    hfe::hudManager->getSquad()->togglePlayerSelection(id);
                }
                return true;
            }
            return true;
        }
        // The miss, 77BECB.
        if ((mode == 18 || mode == 17) && hfe::hudManager->getSquad()) {
            hfe::hudManager->getSquad()->selectSquad(-1);
            hfe::hudManager->getSquad()->selectPlayer(-1);
            return false;
        }
        if (mode == 15 && hfe::hudManager->getCommander()) {
            hfe::hudManager->getCommander()->m_expandedSquad = -1;        // 77BF4D, +A8 directly
            return false;
        }
        return false;
    }

    // 3.10 the pointer is off the map, 77BF62 -----------------------------------------------
    if (hfe::hudManager->getMode() == 21 || hfe::hudManager->getMode() == 20
            || hfe::hudManager->getMode() == 27 || hfe::hudManager->getMode() == 19) {
        if (first == 1 || first == 21)
            hfe::hudManager->setMode(hfe::hudManager->getPreviousMode(), true);
    }
    return false;
}
static_assert(sizeof(Bf2MenuMapNodeTypeDescriptor) == 16, "client BF2 map metadata");

// Stores the minimap position, requests recentering and returns true.
bool Bf2MenuMapNode::setMiniPosForHud(float x, float y)
{
    m_field788.x = x;
    m_field788.y = y;
    mapRecenterForCommander(x, y);
    return true;
}

// 75F1BF, row 33. Stores the size and then drives the node's own +C8 slot with
// the same pair - which is Bf2TransformNode's size accessor, inherited unchanged.
bool Bf2MenuMapNode::setMiniSizeForHud(float x, float y)
{
    m_field798.x = x;
    m_field798.y = y;
    setSizeForHud(x, y);                 // +C8 = 7C1C00, the inherited accessor
    return true;
}

bool Bf2MenuMapNode::setMaxiPosForHud(float x, float y)        // row 36, +7C0
{ m_field7c0.x = x; m_field7c0.y = y; return true; }
bool Bf2MenuMapNode::setMaxiSizeForHud(float x, float y)       // row 35, +7B8
{ m_textureSize.x = x; m_textureSize.y = y; return true; }
bool Bf2MenuMapNode::setCommanderPosForHud(float x, float y)   // row 38, +7A8
{ m_field7a8.x = x; m_field7a8.y = y; return true; }
bool Bf2MenuMapNode::setCommanderSizeForHud(float x, float y)  // row 37, +7B0
{ m_field7b0.x = x; m_field7b0.y = y; return true; }
bool Bf2MenuMapNode::setZoomIconsForHud(bool enabled)          // row 39, +7F9
{ m_field7f9 = enabled; return true; }

// 75F9D0, row 40. A map with no System silently keeps the handle it had; the
// row reports nothing either way, and its console return type is void.
void Bf2MenuMapNode::setControlPointFontForHud(std::string font)
{
    if (m_system) m_fieldc64 = static_cast<std::uint32_t>(m_system->loadFont(font.c_str()));
}

// Inlined at 75FC39, row 41: four floats copied straight into +C68.
void Bf2MenuMapNode::setControlPointFontColorForHud(float red, float green,
                                                    float blue, float alpha)
{
    m_fieldc68.r = red;
    m_fieldc68.g = green;
    m_fieldc68.b = blue;
    m_fieldc68.a = alpha;
}

// Console bindings for minimap visibility and filters.

// 7764F0 (64 B), row 149. Flip one kit's paint filter, then retire VO help 41.
// ds:A10890 is used for the VO call with no null test.
void Bf2MenuMapNode::setPaintKitForHud(int kit)
{
    if (static_cast<unsigned int>(kit) >= 8u)                        // 7764FC cmp ecx,8 / jb
        throw std::out_of_range("invalid bitset<N> position");       // 776503 -> 78F590 (_Xran)
    m_field3c0.flip(static_cast<std::size_t>(kit));                  // 77651C xor [eax], edx
    hfe::hudManager->disableVOHelpMessage(41);                       // push 29h; +FC 751360
}

// 7778E0 (73 B), row 150. All eight kits on - unless all eight already are, then all OFF.
// bitset<8>::count() over the one +3C0 word (nibble table 97E3F0) against 8, then reset()
// (777909: the word to 0) or set() (777914: the word to -1, then _Trim's and 0FFh).
// No VO call here, unlike the single-kit row.
void Bf2MenuMapNode::setPaintAllKitsForHud()
{
    const std::bitset<39> kitWord(0xFFFFFFFFull);                    // the +3C0 word, bits 0..31
    if ((m_field3c0 & kitWord).count() == 8)
        m_field3c0 &= ~kitWord;
    else
        m_field3c0 = (m_field3c0 & ~kitWord) | std::bitset<39>(0xFFull);
}

// 776530 (64 B), row 151. setPaintKitForHud's twin over the +3C4 bitset<7>: bound 7.
void Bf2MenuMapNode::setPaintVehicleForHud(int type)
{
    if (static_cast<unsigned int>(type) >= 7u)                       // 77653C cmp ecx,7 / jb
        throw std::out_of_range("invalid bitset<N> position");       // 776543 -> 78F590 (_Xran)
    m_field3c0.flip(32u + static_cast<std::size_t>(type));           // 77655C xor [eax], edx
    hfe::hudManager->disableVOHelpMessage(41);                       // push 29h; +FC 751360
}

// 777930 (70 B), row 152. setPaintAllKitsForHud's twin: count the +3C4 word (nibble table
// 97E404) against 7, then reset() (777959) or set() (777964: -1, then and 7Fh).
void Bf2MenuMapNode::setPaintAllVehiclesForHud()
{
    const std::bitset<39> vehicleWord(0xFFFFFFFF00000000ull);        // the +3C4 word, bits 32..38
    if ((m_field3c0 & vehicleWord).count() == 7)
        m_field3c0 &= ~vehicleWord;
    else
        m_field3c0 = (m_field3c0 & ~vehicleWord) | std::bitset<39>(0x7F00000000ull);
}

// Inlined at 76BE1D, row 156: `cmp byte ptr [map+38Dh], 0 / sete cl / mov [map+38Dh], cl`.
void Bf2MenuMapNode::toggleShowKitsForHud()                          // +38D
{ m_showKitsOnMap = !m_showKitsOnMap; }

// While the commander view is not up this only records the offset. Once
// it is, the requested position is added to the current zoom level's centre
// scaled by the world-to-map factor, and the result re-centres the view.
// The two axes are biased in opposite directions: x loses 0.5 and y gains it
// (the second constant at 8AAD08 is -0.5, and the body subtracts it).
bool Bf2MenuMapNode::mapRecenterForCommander(float x, float y)
{
    if (!hfe::hudManager->isCommanderShown()) {          // vslot +1EC
        m_miniViewOffset.x = x;
        m_miniViewOffset.y = y;
        return true;
    }
    const hfe::Vec2& centre = m_centers[m_zoomLevel];    // +7C8 + zoomLevel*8
    hfe::Vec2 view;
    view.x = (centre.x * m_worldToMapScale + x) - 0.5f;
    view.y = (centre.y * m_worldToMapScale + y) + 0.5f;
    setViewCenter(view);                                // +11C ->7737C0
    return true;
}

namespace {

// Returns the shortest signed angular difference in radians.
float angleDifference(float a, float b)
{
    float d;
    if (a < 0.0f && b > 0.0f)
        d = b - a;
    else
        d = a - b;

    const float magnitude = std::fabs(d);
    if (magnitude > 3.1415927f)                         // ds:0x92FE88
        return 6.2831855f - magnitude;                  // ds:0x8AC48C, sign dropped
    return d;
}


// the shared float clamp - upper bound tested first, then the lower. Spelled out here for
// the third time (Game/GameServerSettings.cpp and Menu/HudVehicleFrontiers.cpp have the same
// file-local copy) because retail inlines it at every site and there is no header to share.
inline float clampFloat(float value, float low, float high)
{
    float result = value;
    if (value >= high) result = high;
    if (result < low) return low;
    return result;
}

// Wraps angle symmetrically into [-pi, pi). [INFERRED] Helper name.
float wrapAngleToPi(float angle)
{
    const float turns = angle * 0.15915494f;            // ds:0x8AC43C = 1/(2*pi)
    return (turns - std::floor(turns + 0.5f)) * 6.2831855f;   // ds:0x8EE510, ds:0x8AC48C
}

// Clamps the exponential interpolation factor to its supported range.
float clampedExp(float x)
{
    return std::exp(clampFloat(x, -10.0f, 10.0f));      // 402EE0, then the inline x87 exp
}


// Returns zero within tolerance, otherwise the direction to rotate from the first heading to the second.
int turnDirection(float from, float to, float tolerance)
{
    if (std::fabs(to - from) < tolerance)               return 0;
    if (std::fabs(to - (from + 6.2831855f)) < tolerance) return 0;
    if (std::fabs(to - (from - 6.2831855f)) < tolerance) return 0;

    const float d         = from - to;
    const float magnitude = std::fabs(d);

    // more than half a turn apart: the short way is across the +/-pi seam, so the plain sign
    // is wrong and the straddle decides instead.
    if (magnitude > 3.1415927f)                         // ds:0x92FE88
    {
        if (to < 0.0f && from > 0.0f) return -1;
        if (to > 0.0f && from < 0.0f) return 1;
    }
    return static_cast<int>(std::ceil(d / magnitude));
}


// Returns whether any seat in the object is occupied.
bool anySeatOccupied(hfe::world::IObject* object)
{
    if (!object)
        return false;
    hfe::world::IPlayerControlObject* const pco = static_cast<hfe::world::IPlayerControlObject*>(
        object->queryInterface(hfe::world::IID_IPlayerControlObject));
    if (!pco)
        return false;
    std::map<int, hfe::world::IPlayerControlObject*>* const seats = pco->getPcos();   // +0x60
    if (!seats)
        return false;
    for (std::map<int, hfe::world::IPlayerControlObject*>::const_iterator it = seats->begin();
         it != seats->end(); ++it)
    {
        if (it->second && it->second->isOccupied())     // ipco +0x18
            return true;
    }
    return false;
}


// Moves a value toward its target using the supplied interpolation factor.
void easeToward(float& value, float target, float threshold, float delta, float rate,
                bool forceSnap)
{
    if (std::fabs(value - target) < threshold || forceSnap)
    {
        value = target;
        return;
    }
    value += (target - value) * (1.0f - clampedExp(delta * rate));
}

} // anonymous namespace

// Updates the player heading used by the map.
void Bf2MenuMapNode::setPlayerHeading(float heading)
{
    if (std::fabs(angleDifference(heading, m_field75c)) > 0.2f)   // ds:0x8E5D84
    {
        m_field775    = false;
        m_field777    = false;
        m_field71c[2] = 0;                              // +724

        // the plain answer: turn whichever way the number moved
        m_field758 = (heading - m_field75c) > 0.0f ? 1.0f : -1.0f;

        // the wrap correction, only when the two are more than pi apart
        if (std::fabs(m_field75c - heading) > 3.1415927f)          // ds:0x92FE88
        {
            if (m_field75c > 0.0f && heading < 0.0f)
                m_field758 = 1.0f;
            else if (m_field75c < 0.0f && heading > 0.0f)
                m_field758 = -1.0f;
        }
    }
    else if (!m_field775)
    {
        m_field777 = true;
    }

    m_field770 = heading;                               // every path
}

// Updates the player's map position and forward direction.
void Bf2MenuMapNode::setPlayerPosition(hfe::Vec2 position, float heading)
{
    mapRecenterForCommander(position.x, position.y);
    setPlayerHeading(heading);
}

// Advances the pulse used by blinking map markers.
void Bf2MenuMapNode::updateBlinkPulse(float delta)
{
    m_fielda14 = static_cast<float>(
        std::sin(std::fmod(m_fielda0c * m_fielda08, 6.2831853071795865)) * m_fielda10);

    m_fielda08 += delta;
    if (m_fielda08 > 100.0f && m_fielda14 < 0.01f)
        m_fielda08 = 0.0f;
}

// Ages timed markers and removes those that have expired.
void Bf2MenuMapNode::updateTimedMarkers(float delta)
{
    for (std::vector<MapTimedMarker>::iterator it = m_timedMarkers.begin();
         it != m_timedMarkers.end(); )
    {
        if (it->field18 > 0.0f)
            it->field18 -= delta;
        else if (!it->negativeDuration)
            it->remaining -= delta;

        if (it->remaining <= 0.0f && it->field18 <= 0.0f && !it->negativeDuration)
            it = m_timedMarkers.erase(it);
        else
            ++it;
    }
}

// Removes a timed marker.
void Bf2MenuMapNode::removeTimedMarker(int networkId)
{
    for (std::vector<MapTimedMarker>::iterator it = m_timedMarkers.begin();
         it != m_timedMarkers.end(); ++it)
    {
        if (it->networkId == networkId)
        {
            m_timedMarkers.erase(it);
            return;
        }
    }
}

// Updates the satellite scan and its visible map markers.
void Bf2MenuMapNode::updateSatelliteScan(float delta)
{
    if (!hfe::gameLogic)
        return;

    if (m_fielda88 <= m_fielda84)
    {
        // the sweep is running ----------------------------------------------------------
        m_fielda88 += delta;
        const float visible = hfe::gameLogic->getSatelliteScanReloadTime();   // PE +0x298 = ELF +0x298
        m_satelliteReloadFraction = 1.0f - (m_fielda88 / (visible + m_fielda84));

        if ((m_fielda84 - m_fielda88) < m_fielda80)
            hfe::g_bfSound->stopSound(std::string("HUD_Satellite_scan"));   // ds:A23DC0 vt +0x34

        if (!m_satelliteActive)
            return;

        m_satelliteScanX =
            (static_cast<float>(m_satelliteLineSize) + m_fieldb5c.width) / m_fielda84 * m_fielda88;
        return;
    }

    // reloading ------------------------------------------------------------------------
    if (m_satelliteReloadRemaining > 0.0f)
    {
        m_satelliteReloadRemaining -= delta;
    }
    else
    {
        hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
        if (local && local->getSatelliteIsReady())      // +0x124
        {
            m_satelliteReloading       = false;
            m_satelliteReloadRemaining = 0.0f;
        }
    }

    if (hfe::gameLogic)                                 // re-tested, as shipped
    {
        const float visible = hfe::gameLogic->getSatelliteScanReloadTime();
        if (m_satelliteReloadRemaining >= 0.0f && m_satelliteReloadRemaining <= visible)
            m_satelliteReloadFraction = m_satelliteReloadRemaining / (visible + m_fielda84);
    }

    if (m_satelliteActive)
        m_satelliteActive = false;
}

// Updates the UAV's scan and reload state.
void Bf2MenuMapNode::updateUav(float delta)
{
    if (!hfe::gameLogic)
        return;

    m_fieldac4  = delta * m_fieldac0;               // the per-frame sweep step
    m_fieldabc += m_fieldac4;
    if (m_fieldabc > 6.2831855f)                    // ds:0x8AC48C
        m_fieldabc = 0.0f;

    if (m_fieldaac <= m_uavActiveTime)
    {
        // the UAV is up ------------------------------------------------------------------
        m_fieldaac += delta;
        m_uavReloadFraction =
            1.0f - (m_fieldaac / (m_uavReloadRemaining + m_uavActiveTime));
    }
    else
    {
        // reloading ----------------------------------------------------------------------
        if (m_uavReloadRemaining > 0.0f)
        {
            m_uavReloadRemaining -= delta;
        }
        else
        {
            hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
            if (local && local->getUAVIsReady())    // ELF +0x11C
            {
                m_uavReloading       = false;
                m_uavReloadRemaining = 0.0f;
            }
        }

        if (m_uavReloadRemaining >= 0.0f &&
            m_uavReloadRemaining <= hfe::gameLogic->getUAVReloadTime())   // PE +0x290
        {
            m_uavReloadFraction =
                m_uavReloadRemaining / (hfe::gameLogic->getUAVReloadTime() + m_uavActiveTime);
        }

        if (m_uavActive)
        {
            hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
            if (local && !local->getIsAIPlayer())   // ELF +0x64
            {
                // Publish the UAV-offline notice on the team radio channel.
                const std::string notice("uav_offline");
                hfe::game->localRadioMessage(hfe::TCTeam, notice);         // push 1; +0x58
            }
            m_uavActive = false;
        }
    }

    if (!m_uavActive)
        removeUavObject(0xFFFFFFFFu);              // 77ACA0, the -1 sentinel
}

// Removes vehicle-drop markers that no longer refer to a valid drop.
void Bf2MenuMapNode::retireStaleVehicleDrops()
{
    std::list<MapVehicleDrop>::iterator stale = m_vehicleDrops.end();

    for (std::list<MapVehicleDrop>::iterator it = m_vehicleDrops.begin();
         it != m_vehicleDrops.end(); ++it)
    {
        if (!it->active)
            continue;

        hfe::world::IObject* const object = hfe::io::networkManager->getObject(
            static_cast<unsigned short>(it->id));               // a word, as shipped
        if (!object)
            continue;

        // retail reads m_armorComponent (+0x3C) directly; it is protected here, so this goes
        // through IObject's own inline getArmorComponent(), which is the same generated code.
        hfe::world::IObjectComponent* const armor = object->getArmorComponent();
        const bool dead = !armor
                          || static_cast<hfe::world::IArmorGLComp*>(armor)->isDestroyed()  // +0x54
                          || anySeatOccupied(object);
        if (dead)
        {
            it->active = false;
            stale      = it;                                    // the last one wins
        }
    }

    if (stale != m_vehicleDrops.end())
        m_vehicleDrops.erase(stale);
}

// Returns the target zoom exponent for minimap, expanded or commander mode. [CHOSEN] Helper factoring.
float Bf2MenuMapNode::zoomExponentTarget()
{
    const int level = m_zoomLevel;                      // +6D0
    if (hfe::hudManager->isCommanderShown())            // +1EC
        return m_zoomExponents[2][level];
    return m_zoomExponents[m_expandedMap ? 1 : 0][level];
}

// Advances map animation, scans and timed markers by delta seconds.
void Bf2MenuMapNode::update(float delta)
{
    const float entryZoom   = m_currentZoomExponent;    // saved before anything moves it
    bool        sizeReached = m_field776;               // a working copy; written back at the end

    if (hfe::hudManager->getMode() == 15)               // +1E0
    {
        m_field780 = m_field7a8;                        // commander position
        m_field778 = m_field7b0;                        // commander size
    }

    m_fielda30 += delta;
    updateSatelliteScan(delta);
    updateUav(delta);

    // the two commander-marker lifetimes ------------------------------------------------
    if (m_fieldaf0 <= m_fieldb0c)
    {
        if (m_airstrikeActive)
            m_fieldaf0 += delta;
    }
    else if (m_airstrikeActive)
    {
        m_airstrikeActive = false;
    }

    if (m_fieldb10 <= m_fieldb28)
    {
        if (m_suppliesActive)
            m_fieldb10 += delta;
    }
    else if (m_suppliesActive)
    {
        m_suppliesActive = false;
    }

    retireStaleVehicleDrops();
    updateBlinkPulse(delta);

    // the icon blink gate: on for the first half second, off to one, then round again ----
    if (m_fielda30 > m_fielda24)                        // 0.5
    {
        m_fielda18 = false;
        if (m_fielda30 > m_fielda28)                    // 1.0
            m_fielda30 = 0.0f;
    }
    else
    {
        m_fielda18 = true;
    }

    updateTimedMarkers(delta);

    // Expanded maps use a fixed heading.
    if (m_expandedMap)
        m_field770 = 0.0f;

    float normalisedTarget = 0.0f;
    if (m_field68d)
    {
        if (m_mapStatic)
        {
            m_field774 = true;
        }
        else
        {
            normalisedTarget = wrapAngleToPi(m_field770);
            m_field75c       = wrapAngleToPi(m_field75c);

            // the map heading eases toward the target ------------------------------------
            int dir = turnDirection(normalisedTarget, m_field75c, 0.001f);
            if (dir == 0)
            {
                m_field75c = normalisedTarget;
            }
            else
            {
                const float gap = angleDifference(normalisedTarget, m_field75c);
                m_field75c += dir * std::fabs(gap) * (1.0f - clampedExp(delta * -9.0f));
            }

            // Apply the settling wobble, decaying its amplitude over 0.75 seconds.
            if (!m_field777)
            {
                m_field764[0] = wrapAngleToPi(m_field75c);
            }
            else
            {
                const float amplitude = 0.3f - m_wobbleTime * 0.4f;
                const float wobble =
                    static_cast<float>(std::sin(m_wobbleTime * 4.5f)) * amplitude * m_field758;
                m_field764[1] = wobble;
                m_field764[0] = wrapAngleToPi(m_field75c + wobble);

                if (amplitude > 0.0f)
                {
                    m_wobbleTime += delta;
                }
                else
                {
                    m_field775    = true;
                    m_field777    = false;
                    m_field764[1] = 0.0f;
                    m_wobbleTime  = 0.0f;
                }
            }

            // the delayed angle trails the rendered one ----------------------------------
            m_delayedMapAngle = wrapAngleToPi(m_delayedMapAngle);
            dir = turnDirection(m_field764[0], m_delayedMapAngle, 0.001f);
            if (dir == 0)
            {
                m_delayedMapAngle = m_field764[0];
            }
            else
            {
                const float gap = angleDifference(m_field764[0], m_delayedMapAngle);
                m_delayedMapAngle += dir * std::fabs(gap) * (1.0f - clampedExp(delta * -9.0f));
            }

            // the painted map angle, walked out in fixed steps ---------------------------
            // UP to 100 passes in one frame. Each pass moves a fixed amount, so the loop is
            // how a large heading change is consumed. The step differs by branch: 0.05 once the
            // size has settled, 0.007 while it has not.
            if (sizeReached && m_field774)
            {
                for (int i = 0; i < 100; ++i)
                {
                    dir = turnDirection(normalisedTarget, m_field76c, 0.2f);
                    if (dir == 0)
                    {
                        m_field76c = normalisedTarget;
                        break;
                    }
                    m_field76c += dir * 0.05f * (1.0f - clampedExp(delta * -9.0f));
                }
            }
            else
            {
                for (int i = 0; i < 100; ++i)
                {
                    dir = turnDirection(normalisedTarget, m_field76c, 0.2f);
                    if (dir == 0)
                    {
                        m_field76c = normalisedTarget;
                        m_field774 = true;
                        break;
                    }
                    m_field774 = false;             // re-cleared every pass, as shipped
                    m_field76c += dir * 0.007f * (1.0f - clampedExp(delta * -9.0f));
                }
            }
        }
    }

    // how far the node's size has travelled between mini and maxi ------------------------
    float ratio;
    if (m_expandedMap)
        ratio = getSize().y / m_field778.y;
    else
        ratio = 1.0f - (m_field778.y / getSize().y);
    m_field694 = ratio;

    // Skip geometry updates once zoom has settled on an unexpanded map.
    if (!(m_field68d && !m_field774 && (ratio > 0.0f || m_expandedMap)))
    {
        const bool snap = m_overheadZoomActive;         // +7F8, tested at every geometry site

        easeToward(m_field7e8.x, m_viewCenter.x,      0.0001f, delta, -6.0f, snap);
        easeToward(m_field7e8.y, m_viewCenter.y,      0.0001f, delta, -6.0f, snap);
        easeToward(m_field748.x, m_miniViewOffset.x,  0.001f,  delta, -6.0f, snap);
        easeToward(m_field748.y, m_miniViewOffset.y,  0.001f,  delta, -6.0f, snap);

        // the zoom exponent does not test m_overheadZoomActive, and it re-selects its target
        // on every arm - three isCommanderShown() calls, preserved through zoomExponentTarget().
        if (std::fabs(zoomExponentTarget() - m_currentZoomExponent) < 0.0001f)
            m_currentZoomExponent = zoomExponentTarget();
        else
            m_currentZoomExponent +=
                (zoomExponentTarget() - m_currentZoomExponent) * (1.0f - clampedExp(delta * -6.0f));

        easeToward(m_fielda50, m_minCenterY, 0.001f, delta, -6.0f, snap);
        easeToward(m_fielda54, m_maxCenterY, 0.001f, delta, -6.0f, snap);
        easeToward(m_fielda58, m_minCenterX, 0.001f, delta, -6.0f, snap);
        easeToward(m_fielda5c, m_maxCenterX, 0.001f, delta, -6.0f, snap);

        // Gas or the blinding-light debug flag immediately suppresses the map fade.
        hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
        if (local && !local->getIsManDown() && local->getIsAlive()
            && (hfe::world::blindingLightDamageShow || local->getIsGassed()))
        {
            m_field708[2] = m_field718 - 0.2f;
            if (!(m_field708[2] > 0.0f))
                m_field708[2] = 0.0f;
            m_field6fc    = 0.0f;
            m_field708[0] = 0.0f;           // retail reaches this by jumping into the block below
        }
        else
        {
            // the fade eases toward m_field718 and is clamped at whichever end it approaches
            if (std::fabs(m_field708[2] - m_field718) < 0.01f)
            {
                m_field708[2] = m_field718;
            }
            else if (m_field708[2] < m_field718)
            {
                m_field708[2] += (1.0f - clampedExp(delta * -9.0f)) * 0.2f;
                if (m_field708[2] > m_field718)
                    m_field708[2] = m_field718;
            }
            else
            {
                m_field708[2] -= (1.0f - clampedExp(delta * -9.0f)) * 0.1f;
                if (!(m_field708[2] > 0.0f))
                    m_field708[2] = 0.0f;
            }

            // these two use rate -10 and clamp at both ends, unlike every other site
            if (std::fabs(m_field6fc - m_field704) < 0.01f)
            {
                m_field6fc = m_field704;
            }
            else
            {
                m_field6fc += (m_field704 - m_field6fc) * (1.0f - clampedExp(delta * -10.0f));
                if (m_field6fc > m_field704)
                    m_field6fc = m_field704;
                else if (!(m_field6fc > 0.0f))
                    m_field6fc = 0.0f;
            }

            if (std::fabs(m_field708[0] - m_field708[1]) < 0.01f)
            {
                m_field708[0] = m_field708[1];
            }
            else
            {
                m_field708[0] +=
                    (m_field708[1] - m_field708[0]) * (1.0f - clampedExp(delta * -10.0f));
                if (m_field708[0] > m_field708[1])
                    m_field708[0] = m_field708[1];
                else if (!(m_field708[0] > 0.0f))
                    m_field708[0] = 0.0f;
            }
        }

        // the node's own transform, eased toward the target pose -----------------------
        // the offsets are half an 800x600 screen. getPosition()/getSize() are re-called at
        // every step rather than cached, as shipped.
        easeToward(getPosition().y, m_field780.y + 300.0f, 0.1f, delta, -6.0f, false);
        easeToward(getPosition().x, m_field780.x + 400.0f, 0.1f, delta, -6.0f, false);
        easeToward(getSize().y,     m_field778.y,          0.1f, delta, -6.0f, false);
        easeToward(getSize().x,     m_field778.x,          0.1f, delta, -6.0f, false);

        // has the node arrived at one of its two sizes? --------------------------------
        // each test RE-calls getSize(); the 3.0 slack is the arrival tolerance.
        if (hfe::hudManager->getMode() == 15)
        {
            if (m_field7b0.x - 3.0f < getSize().x && m_field7b0.y - 3.0f < getSize().y)
            {
                sizeReached  = true;
                m_field68d   = false;
                m_field68c   = true;
            }
            else
            {
                sizeReached = false;
                m_field68c  = false;
                m_field68d  = false;
            }
        }
        else if (m_textureSize.x - 3.0f < getSize().x && m_textureSize.y - 3.0f < getSize().y)
        {
            sizeReached = true;
            m_field68c  = true;
            m_field68d  = false;
        }
        else if (m_field798.x + 3.0f > getSize().x && m_field798.y + 3.0f > getSize().y)
        {
            sizeReached = true;
            m_field68d  = true;
            m_field68c  = false;
        }
        else
        {
            sizeReached = false;
            m_field68c  = false;
            m_field68d  = false;
        }
    }

    // The completion test compares the zoom exponent with the level index itself.
    if (m_currentZoomExponent == static_cast<float>(m_zoomLevel)
        && m_currentZoomExponent != entryZoom)
    {
        hfe::eventManager->postEvent(hfe::ECHud, 0x4D, 0, 0.0f);   // PE +0x10 overload
    }

    // Send size-change events on either transition while the commander screen is hidden.
    if ((!m_field776 && sizeReached && !hfe::hudManager->isCommanderShown())
        || (m_field776 && !sizeReached && !hfe::hudManager->isCommanderShown()))
    {
        hfe::eventManager->postEvent(hfe::ECHud, 0x4C, 0, 0.0f);
    }

    m_field776 = sizeReached;
}

} }
#endif
