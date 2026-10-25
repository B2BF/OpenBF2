// Compact HUD map with its own icon styling and event handling.
#include "Bf2MiniMenuMapNode.h"
#if !DEDICATED
#include "BF2HudManager.h"
#include "HudColors.h"
#include "HudCommander.h"                                   // +14C: isSquadSelected +30, getExpandedSquad +38
#include "HudSquad.h"                                       // +14C: the squad screen's map selection
#include "../Core/SettingsRepostitory.h"                    // paintSelf: GLSWorldSizeZ / GLSWorldSizeX
#include "../Core/System.h"                                 // WorldPref::getWorldTime (4C4400)
#include "../Game/IGame.h"
#include "../Game/Objects/ICameraObject.h"                  // collectLocalPlayerIcon: +0x10
#include "../Game/Objects/IPlayer.h"
#include "../Game/GameLogic/SquadManager/SquadOrder.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Core/StringUtil.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/Effect.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
namespace dice { namespace meme {
namespace {
// The blank name the constructor hands loadTexture twenty-two times. Not a
// defect: Ref2System::loadTexture("") finds or creates one texture named "" and
// returns the same handle each time, so those slots deliberately share a blank.
const char* const blankTextureName = "";
const char* const microMapActionTexture = "Ingame/Minimap/Icons/placedAction_micromap.tga";

// The five masks the constructor applies to m_filterSets[5][0], in the shipped
// order. Each is a dword or byte and on the bitset's underlying storage; the
// high halves below leave the other dword alone.
const unsigned long long maskHighA = 0x000007DF00000000ull | 0x00000000FFFFFFFFull;
const unsigned long long maskLowA  = 0xFFFFFFFF00000000ull | 0x000000007FFEFFF7ull;
const unsigned long long maskHighB = 0xFFFFFFFE00000000ull | 0x00000000FFFFFFFFull;
const unsigned long long maskLowB  = 0xFFFFFFFF00000000ull | 0x00000000FF797FFFull;
// [+4BD] &= 0xF9 clears bits 41 and 42 - the byte at bit offset 40.
const unsigned long long maskByte  = ~0x0000060000000000ull;

// Applies the upper bound before the lower bound.
inline float clampFloat(float value, float low, float high)
{
    float result = value;
    if (value >= high) result = high;          // fcmovnb: CF == 0, ordered and not below
    if (!(result >= low)) result = low;        // fcmovb: CF == 1, below or unordered
    return result;
}
}

// Everything below the base call is this class's own initialisation; it
// overwrites a slice of what Bf2MenuMapNode just set up.
Bf2MiniMenuMapNode::Bf2MiniMenuMapNode(System* system, const char* name,
                                       int x, int y, int width, int height)
    : Bf2MenuMapNode(system, name, x, y, width, height)
{
    getPosition().x = 0.0f;                         // 65DDD0, LEA ECX+C
    getPosition().y = 0.0f;
    getSize().x = static_cast<float>(width);        // 659990, LEA ECX+14
    getSize().y = static_cast<float>(height);

    m_showKitsOnMap = false;
    m_field3c0.reset();
    m_field3c0.set(7);                              // or 0x80 on the low dword

    m_filterSets[5][0] &= std::bitset<43>(maskHighA);
    m_filterSets[5][0] &= std::bitset<43>(maskLowA);
    m_filterSets[5][0] &= std::bitset<43>(maskHighB);
    m_filterSets[5][0] &= std::bitset<43>(maskLowB);
    m_filterSets[5][0] &= std::bitset<43>(maskByte);
    for (int i = 1; i != 6; ++i) m_filterSets[5][i] = m_filterSets[5][0];
    for (int i = 0; i != 6; ++i) m_filterSets[6][i].reset();

    m_field8a8[0] = m_system->loadTexture("Ingame/Minimap/map_Line.tga", true);

    // The four members below are the ones that travel through the obfuscation
    // pair; the pair is an identity, so each simply receives the handle. The
    // shipped order is 3, 4, 6, 5 - not sequential.
    const int actionTexture = m_system->loadTexture(microMapActionTexture, true);
    m_field8a8[3] = actionTexture;
    m_field8a8[4] = actionTexture;
    m_field8a8[6] = actionTexture;
    m_field8a8[5] = actionTexture;
    m_field8a8[7] = m_defaultMinimapTexture;
    m_field8a8[8] = m_defaultMinimapTexture;
    m_field8a8[9] = m_defaultMinimapTexture;
    m_field8a8[13] = m_defaultMinimapTexture;       // overwritten four lines later
    m_field8a8[10] = m_defaultMinimapTexture;
    m_field8a8[11] = m_defaultMinimapTexture;

    const int secondAction = m_system->loadTexture(microMapActionTexture, true);
    m_field8a8[13] = secondAction;
    m_field8a8[14] = secondAction;
    m_field8a8[16] = secondAction;
    m_field8a8[15] = secondAction;
    m_field8a8[17] = m_defaultMinimapTexture;
    m_field8a8[18] = m_defaultMinimapTexture;
    m_field8a8[19] = m_defaultMinimapTexture;
    m_controlPointTextures[0] = m_defaultMinimapTexture; // overwritten immediately below
    m_field8a8[20] = m_defaultMinimapTexture;
    m_field8a8[21] = m_defaultMinimapTexture;

    m_controlPointTextures[0] = m_system->loadTexture(blankTextureName, true);
    m_controlPointTextures[1] = m_system->loadTexture(blankTextureName, true);
    m_controlPointTextures[2] = m_system->loadTexture(blankTextureName, true);
    m_controlPointBaseTextures[0] = m_system->loadTexture(blankTextureName, true);
    m_controlPointBaseTextures[1] = m_system->loadTexture(blankTextureName, true);
    m_controlPointBaseTextures[2] = m_system->loadTexture(blankTextureName, true);
    m_teamFlagTextures[0] = m_system->loadTexture(blankTextureName, true);
    m_teamFlagTextures[1] = m_system->loadTexture(blankTextureName, true);
    m_teamFlagTextures[2] = m_system->loadTexture(blankTextureName, true);
    m_field93c[4] = m_system->loadTexture(blankTextureName, true);
    m_field93c[5] = m_system->loadTexture(blankTextureName, true);
    m_field93c[6] = m_system->loadTexture(blankTextureName, true);
    m_field93c[9] = m_system->loadTexture(blankTextureName, true);
    m_field93c[10] = m_system->loadTexture(blankTextureName, true);
    m_field93c[18] = m_system->loadTexture("Ingame/Commander/micromap_zoomSquare.tga", true);
    m_field93c[19] = m_system->loadTexture(blankTextureName, true);
    m_field93c[20] = m_system->loadTexture(blankTextureName, true);
    m_field93c[17] = m_system->loadTexture(blankTextureName, true);
    m_field9a8[8] = m_system->loadTexture(blankTextureName, true);
    m_field9a8[9] = m_system->loadTexture(blankTextureName, true);
    m_field9a8[10] = m_system->loadTexture(blankTextureName, true);
    m_fielda94 = m_system->loadTexture(blankTextureName, true);
    m_field93c[22] = m_system->loadTexture(blankTextureName, true);

    m_currentZoomExponent = 0.0f;
    resetViewBiasForHud();                          // the +158 self-call at 7CFE74
    m_field68d = false;
    m_fielda7b = false;                             // the base leaves this true
    m_field7f9 = false;                             // zoom icons off; the base leaves them on
    m_field694 = 1.0f;                              // the base leaves 0.5
    m_field68c = true;
    m_expandedMap = true;                           // the base leaves this false
    m_field708[2] = 0.5f;                           // the base leaves 1.0
    m_field678 = 2;                                 // the base leaves 8
    // +C80 (m_lastUpdateTime) is not written here - see the header and paintSelf.
}

// Both vptrs are restored and the base does all the work.
Bf2MiniMenuMapNode::~Bf2MiniMenuMapNode() {}

TypeDescriptor* Bf2MiniMenuMapNode::getClassType()       // 7CF750, singletonA23414
{
    static ClassTypeDescriptor<Bf2MiniMenuMapNode, Bf2MenuMapNode>
        type("dice::meme::Bf2MiniMenuMapNode");
    return &type;
}
TypeDescriptor* Bf2MiniMenuMapNode::getType() { return getClassType(); }
unsigned int Bf2MiniMenuMapNode::getObjectSize() { return sizeof(Bf2MiniMenuMapNode); }

namespace {
TypeDescriptor* const registeredBf2MiniMenuMapNodeType = Bf2MiniMenuMapNode::getClassType();
}

// 7CEDB0, the +158 slot. Two constants into the pair the base zeroes; the
// constructor is its only caller in this image.
void Bf2MiniMenuMapNode::resetViewBiasForHud()
{
    m_field7e8.x = 0.5f;
    m_field7e8.y = -0.5f;
}

// 7CE3D0 (117 B), the +DC override - slot 55, one of the twenty this class
// replaces. It is not the map's 777DC0 and shares nothing with it: where the map
// runs a 29-way switch, this body accepts exactly two modes and treats every other
// one as "off". 15 and 21 are the first two entries of
// BF2HudManager::isCommanderMode's list (74FE40) - this node takes that pair, not
// the whole commander set, which fits a node the builder names "CommanderMiniMap".
void Bf2MiniMenuMapNode::setHudMode(int mode)
{
    if (mode != 15 && mode != 21) {
        m_field06c = false;                          // +6C, 7CE3E3
        return;
    }
    m_field690 = false;                              // +690
    m_commanderMode = true;                          // +691
    m_squadMode = false;                             // +692
    applyFilterSet(m_filterSets[5]);                 // 772060, the +4B8 row
    m_field674 = 2;                                  // +674; the constructor leaves 1
    setField70c(0.0f);
    setField718(1.0f, true);                         // 772880, so +700 follows +718
    setField704(1.0f);
    m_field06c = true;                               // +6C, 7CE43C
}

// 7CE470 (8 B) and 7CE480 (11 B), the +118 and +11C overrides. Both are a single
// tail-call into this class's own added slot +158, and 7CE480 throws the Vec2
// away: a mini-map does not track a view centre, it just re-seats the bias pair.
void Bf2MiniMenuMapNode::recenterOnPlayer() { resetViewBiasForHud(); }       // jmp +158
void Bf2MiniMenuMapNode::setViewCenter(hfe::Vec2) { resetViewBiasForHud(); } // call +158

// The mini-map replaces the base timed-marker lookup quartet. The shared
// 4553E0 body returns zero for +E4/+FC; +E8 and +EC were read at 7CF690/7CF660.
int Bf2MiniMenuMapNode::timedMarkerTexture(unsigned int) { return 0; }
hfe::Vec2 Bf2MiniMenuMapNode::timedMarkerSize(unsigned int)
{
    return hfe::Vec2{4.0f, 4.0f};
}
bool Bf2MiniMenuMapNode::populateNetworkIcon(int, MapIconRecord* icon, bool)
{
    if (!icon) return false;
    icon->texture = 0;
    icon->size = hfe::Vec2{4.0f, 4.0f};
    return true;
}
int Bf2MiniMenuMapNode::getSmallSquadIcon(int) { return 0; }
// +110, the same shared 4553E0 (`xor eax, eax; retn 4`) - vtable 938858 slot +110.
int Bf2MiniMenuMapNode::getSupportRequestTexture(int) { return 0; }

// Adds order markers to the compact map.
bool Bf2MiniMenuMapNode::addOrderIcons(hfe::SquadOrder* order, int, int leaderNetworkId,
                                       int kind, const Rect* bounds)
{
    MapLineRecord line;
    line.a = hfe::Vec2{0.0f, 0.0f};
    line.b = hfe::Vec2{0.0f, 0.0f};
    line.networkIdA = 0;
    line.networkIdB = 0;
    line.field10 = false;
    line.valid = true;
    line.squad = 0;
    MapIconRecord marker;
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();
    marker.size = hfe::Vec2{15.0f, 31.0f};
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
            return true;
        line.networkIdA = leaderNetworkId;
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
        marker.rotation = 0.0f;
        line.line.size.x = static_cast<float>(m_field678);
        line.line.rotation += m_field76c;
        styleOrderIcons(kind, &line, &marker, order->flag2C);                         // +130
        if (line.valid && order->flag2C)
            addMapIcon(8, line.line);
        if (order->flag2C || kind == 2)
            addMapIcon(type, marker);
    }
    return true;
}

// Styles order and request icons for the minimap. Requests have no line; orders keep their assigned color.
void Bf2MiniMenuMapNode::styleOrderIcons(int kind, MapLineRecord* line, MapIconRecord* marker,
                                         bool)
{
    if (!line)
        return;
    if (!marker)
        return;
    const float alpha = m_field6fc;                                                   // +6FC
    const Color& typeColor = hfe::hud::mapOrderColors[line->kind];                    // ds:A23760
    marker->texture = m_defaultMinimapTexture;                                        // +8A4
    marker->color = Color{typeColor.r, typeColor.g, typeColor.b, alpha};
    marker->size = hfe::Vec2{4.0f, 4.0f};
    marker->rotation = 0.785398185f;
    line->valid = true;
    line->line.texture = m_defaultMinimapTexture;
    line->line.color = Color{typeColor.r, typeColor.g, typeColor.b, alpha};
    if (kind == 1) {
        marker->texture = m_field8a8[13 + line->kind];                                // +8DC
    } else if (kind == 0) {
        marker->texture = m_field8a8[3 + line->kind];                                 // +8B4
    } else if (kind == 2) {
        marker->texture = m_field8a8[3 + line->kind];
        marker->size = hfe::Vec2{8.0f, 8.0f};
        marker->color.a = static_cast<float>(static_cast<int>(m_fielda18));           // movzx, fild
        line->valid = false;
    }
}

// 7CF5A0, virtual +13C. The by-value map name and the four style flags are
// intentionally ignored; retail still destroys the string on every exit.
void Bf2MiniMenuMapNode::populateControlPointMapIcon(
    MapIconRecord* icon, int team, bool, std::string, bool, bool, bool)
{
    if (!icon) return;

    icon->texture = m_defaultMinimapTexture;
    icon->size = hfe::Vec2{5.0f, 5.0f};

    hfe::world::IPlayer* const local =
        hfe::world::playerManager->getLocalHumanPlayer();
    if (!local) return;

    const int side = (team == local->getTeam()) + 1;
    icon->color = team <= 0
        ? hfe::hud::miniControlPointColors[side]
        : hfe::hud::mapObjectColors[side];
}

// 7CF020, virtual +124. It intentionally differs from the base painter: sizes
// are floored first, positions are not half-pixel snapped, and no mask texture
// is installed.
void Bf2MiniMenuMapNode::paintMapIcon(hfe::Vec2 position, float rotation,
                                      hfe::Vec2 size, unsigned int texture, System* system,
                                      const Rect* adjusted, const Rect* bounds,
                                      const Color* color, RenderContext* context)
{
    // [CHOSEN, not recovered]
    if (texture == 0) return;
    position.x -= static_cast<float>(std::floor(position.x + 0.5f));
    position.y -= static_cast<float>(std::floor(position.y + 0.5f));
    size.x = static_cast<float>(std::floor(size.x));
    size.y = static_cast<float>(std::floor(size.y));
    const Rect destination = {
        adjusted->x + adjusted->width * 0.5f
            - size.x * 0.5f + position.x * adjusted->width,
        adjusted->y + adjusted->height * 0.5f
            - size.y * 0.5f + position.y * adjusted->height,
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
    drawContextTexture(context, system, texture, &quad);
}

// Advances the compact map by delta seconds.
void Bf2MiniMenuMapNode::update(float delta)
{
    m_fielda30 += delta;                                                    // +A30
    updateBlinkPulse(delta);
    if (m_fielda30 > m_fielda24) {                                          // +A24, 0.5
        m_fielda18 = false;                                                 // +A18
        if (m_fielda30 > m_fielda28)                                        // +A28, 1.0
            m_fielda30 = 0.0f;                                              // an integer zero store
    } else {
        m_fielda18 = true;
    }
    updateTimedMarkers(delta);
    m_lastUpdateTime = hfe::WorldPref::getWorldTime();                      // 4C4400 -> +C80
}

// Draws the compact map and its visible icon families.
void Bf2MiniMenuMapNode::paintSelf(System* system, const NodeState*, const Rect* bounds,
                                   const Rect*, RenderContext* context)
{
    // m_lastUpdateTime is never initialised (see the header): the first paint's delta is the
    // world time minus the heap's leftover bits. As shipped.
    update(static_cast<float>(hfe::WorldPref::getWorldTime() - m_lastUpdateTime));   // +E0 7CE340

    int defaultWorldZ = 0;
    int defaultWorldX = 0;
    {
        const std::string worldZName("GLSWorldSizeZ");
        const std::string worldXName("GLSWorldSizeX");
        const int worldZ = hfe::settings->intGet(worldZName, defaultWorldZ); // ds:987338 +0x10
        const int worldX = hfe::settings->intGet(worldXName, defaultWorldX);
        m_field088 = 1.0f / static_cast<float>(worldZ);                     // +88
        m_worldToMapScale = 1.0f / static_cast<float>(worldX);              // +84
    }

    hfe::world::playerManager->getLocalHumanPlayer();                       // +0x30, discarded

    if (isMapFilterEnabled(0))                                              // +104
        drawMapTexture(m_minimapTexture, bounds, bounds, system, context);
    if (isMapFilterEnabled(3)) {
        updateSupplyLine(system);
        drawSupplyLine(bounds, bounds, system, context);
    }
    clearMapIcons();

    if (isMapFilterEnabled(17) || isMapFilterEnabled(18) || isMapFilterEnabled(15))
        collectIconFamily783F90();
    // The argument is materialised as `push 1` / `push 0` off a second +104 call.
    if (isMapFilterEnabled(10))
        collectControlPointIcons(isMapFilterEnabled(3));
    if (isMapFilterEnabled(20)) collectSquadOrderIcons(bounds);
    if (isMapFilterEnabled(21)) collectCommanderOrderIcons(bounds);
    if (isMapFilterEnabled(19)) collectRequestIcons(bounds);
    if (isMapFilterEnabled(22) || isMapFilterEnabled(23) || isMapFilterEnabled(37))
        collectIconFamily783440();
    collectLocalPlayerIcon();                                               // +12C 7CEDD0
    if (isMapFilterEnabled(16)) collectIconFamily77E500();
    // `fcomp qword ptr ds:8E7748` - the float +708 against the double 0.01, `test ah,41h / jne`.
    if (isMapFilterEnabled(31) && m_field708[0] > 0.01)
        collectSpawnGroupIcons();
    if (isMapFilterEnabled(30)) collectIconFamily7820E0();

    paintMapIcons(system, bounds, bounds, context);

    m_field07c = false;                                                     // +7C
    m_field07d = false;                                                     // +7D
    m_field07e = false;                                                     // +7E
}

// Adds the local-player arrow to the compact map.
void Bf2MiniMenuMapNode::collectLocalPlayerIcon()
{
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();  // +0x30
    MapIconRecord icon;                                                     // 773590, inlined
    icon.rotation = 0.0f;                                                   // stored after texture
    icon.clampToVisibleMap = true;                                          // +28
    if (!local)
        return;

    hfe::world::ICameraObject* camera = local->getCamera();                 // client +0x17C
    if (local->getIsCommander())                                            // client +0x110
        camera = hfe::hudManager->getCameraObject(local->getTeam());        // +0x70, client +0xE4
    if (!camera)
        return;

    bool headingOut = true;                                                 // [ebp-2]
    bool rotateOut = true;                                                  // [ebp-1]
    populateMiniPlayerIcon(local, &icon, &headingOut, &rotateOut, false, true);
    colorMiniMapIcon(local, &icon, true);                                   // +14C 7CE490

    const hfe::Mat4& view = camera->getTransformation(0.0f);                // ICameraObject +0x10
    projectWorldPosition(hfe::Vec3{view.m[12], view.m[13], view.m[14]}, &icon);

    icon.color = Color{1.0f, 1.0f, 1.0f, m_field6fc};                       // +6FC
    icon.rotation = 0.0f;
    if (rotateOut)
        icon.rotation = m_field76c - std::atan2(view.m[8], view.m[10]);     // +76C, fpatan

    hfe::Vec2 scale{1.0f, 1.0f};
    if (hfe::hudManager->getMap()) {                                        // +0x250
        scale = hfe::hudManager->getMap()->getZoomScale(false);             // asked again, 773900
        scale.x = clampFloat(scale.x * 0.5f, 1.0f, 100.0f);                 // ds:8EE510, 402EE0
        scale.y = clampFloat(scale.y * 0.5f, 1.0f, 100.0f);
        Bf2MenuMapNode* const map = hfe::hudManager->getMap();              // a third time
        const hfe::Vec2& center = map->m_centers[map->m_zoomLevel];         // +7C8 + level*8
        icon.position.x += center.x * m_worldToMapScale;                    // +84
        icon.position.y += m_worldToMapScale * center.y;
    }
    icon.size.x /= scale.x;
    icon.size.y /= scale.y;
    addMapIcon(6, icon);
}

// Chooses the color for a compact-map icon.
void Bf2MiniMenuMapNode::colorMiniMapIcon(hfe::world::IPlayer* player, MapIconRecord* icon,
                                          bool highlightSelection)
{
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();  // +0x30
    const int mode = hfe::hudManager->getMode();                            // +0x1E0
    if (!icon)
        return;
    if (!player) {
        if (local)
            icon->color = hfe::hud::mapNeutralColors[2];                    // ds:A1CB38
        return;
    }
    if (!local)
        return;

    const int side = 2 - (local->getTeam() != player->getTeam());           // client +0xE4, local first
    if (player == local) {
        icon->color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        return;
    }
    if (std::find(m_fielda34.begin(), m_fielda34.end(), player->getId())    // client +0x50, 777A30
            != m_fielda34.end()) {
        icon->color = hfe::hud::mapSelectedColors[side];                    // ds:A1CD48
        return;
    }

    if ((mode == 18 || mode == 17 || mode == 19 || mode == 20)
            && hfe::hudManager->getSquad()) {                               // +0x254
        // Retail reads the squad screen's word before asking the player for its id / squad.
        if (highlightSelection
                && (player->getId() == hfe::hudManager->getSquad()->getSelectedPlayerForMap()      // +90
                    || player->getSquadId()
                        == hfe::hudManager->getSquad()->getSelectedSquadForMap())) {               // +8C
            icon->color = hfe::hud::mapWhiteColors[side];                   // ds:A1CE98
            return;
        }
        if (player->getSquadId() && player->getSquadId() == local->getSquadId()   // client +0x108
                && !player->getIsCommander()                                // client +0x110
                && icon->texture != m_fielda94) {                           // +A94
            icon->color = hfe::hud::squadChatColors[side];                  // ds:A1C738
            return;
        }
        if (player->getIsCommander()
                && player->getDefaultVehicle() == player->getVehicle()      // client +0xA0, then +0x44
                && icon->texture != m_fielda94) {
            icon->color = Color{1.0f, 1.0f, 1.0f, 1.0f};
            return;
        }
        icon->color = hfe::hud::mapObjectColors[side];                      // ds:A1CCC8
        return;
    }

    if (hfe::hudManager->isCommanderShown() && hfe::hudManager->getCommander()) {  // +0x1EC, +0x258
        if (highlightSelection) {
            // Retail order: getCommander, then the player's id, then +0x38.
            if (player->getId() == hfe::hudManager->getCommander()->getExpandedSquad()) {  // +0x38
                icon->color = hfe::hud::mapWhiteColors[side];
                return;
            }
            // No non-zero-squad test in front of it (the map has one).
            if (hfe::hudManager->getCommander()->isSquadSelected(player->getSquadId())) {  // +0x30
                icon->color = hfe::hud::mapWhiteColors[side];
                return;
            }
        }
        if (player->getSquadId())
            icon->color = hfe::hud::squadChatColors[side];
        else
            icon->color = hfe::hud::mapObjectColors[side];
        return;
    }

    if (player->getSquadId() && player->getSquadId() == local->getSquadId()
            && !player->getIsCommander()
            && icon->texture != m_fielda94) {
        icon->color = hfe::hud::squadChatColors[side];
        return;
    }
    if (player->getIsCommander()
            && player->getDefaultVehicle() == player->getVehicle()
            && icon->texture != m_fielda94) {
        icon->color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        return;
    }
    icon->color = hfe::hud::mapObjectColors[side];
}

// Fills a player icon for the compact map.
void Bf2MiniMenuMapNode::populateMiniPlayerIcon(hfe::world::IPlayer* player, MapIconRecord* icon,
                                                bool* headingOut, bool* rotateOut,
                                                bool, bool specialRoleIcons)
{
    hfe::world::IPlayer* const local = hfe::world::playerManager->getLocalHumanPlayer();  // +0x30
    if (icon) {
        if (player == local) {
            icon->size = hfe::Vec2{120.0f, 120.0f};                         // +18 then +1C
            icon->texture = m_field93c[18];                                 // +984
        } else {
            const float dot = player->getIsSquadLeader() && specialRoleIcons ? 4.0f : 2.0f;
            icon->size.y = dot;                                             // +1C first
            icon->size.x = dot;
            icon->texture = 0;
        }
        if (rotateOut)
            *rotateOut = false;
    }
    if (headingOut)
        *headingOut = false;
}

// Moves the commander view to the clicked map position.
bool Bf2MiniMenuMapNode::onEvent1(System* system, const Rect* rect, const NodeState*,
                                  int first, int, int)
{
    if (!m_field06c)                                                        // +6C
        return false;

    const float zoom = static_cast<float>(
        std::pow(static_cast<double>(2.3f), m_currentZoomExponent));        // ds:930150, +698
    const float* const pointer = system->getPointerPosition();
    float relX = pointer[0];
    float relY = pointer[1];
    const float centerX = (system->getWidth() + rect->width) * 0.5f + rect->x;
    relX = relX - (centerX - system->getWidth() * 0.5f);                    // asked again
    const float centerY = (system->getHeight() + rect->height) * 0.5f + rect->y;
    relY = relY - (centerY - system->getHeight() * 0.5f);
    const float scaleX = 1.0f / (zoom * rect->width);
    const float scaleY = 1.0f / (zoom * rect->height);

    if (!(std::fabs(relX) < rect->width * 0.5f))
        return false;
    if (!(std::fabs(relY) < rect->height * 0.5f))
        return false;

    const float mapX = m_field7e8.x - 0.5f + scaleX * relX;                 // +7E8
    const float mapY = scaleY * relY + m_field7e8.y + 0.5f;                 // +7EC
    hfe::hudManager->getMode();                                             // +0x1E0, discarded

    if (first == 1) {
        m_commanderPosition.x = mapX;                                       // +7E0
        m_commanderPosition.y = mapY;                                       // +7E4
        hfe::hudManager->setMode(15, true);                                 // +0x1DC
        hfe::hudManager->setCommanderCameraPosition(hfe::Vec2{
            1.0f / m_worldToMapScale * m_commanderPosition.x,               // +84
            -(1.0f / m_field088 * m_commanderPosition.y)});                 // +88; +0x2EC
        hfe::hudManager->disableVOHelpMessage(26);                          // +0xFC
        return false;
    }
    if (first == 21)
        hfe::hudManager->setMode(15, true);
    return false;
}

// Handles compact-map HUD events in place of the base handler.
void Bf2MiniMenuMapNode::handleHudEvent(unsigned int id, hfe::EventNode*, void* data)
{
    // 7CF799, before the switch and for every id, including the ones the switch
    // ignores: the local human player is fetched and the answer is thrown away -
    // `mov eax, [ebp+arg_0]` overwrites it two instructions later and no arm reads
    // it. Preserved as a call, not folded away: it is a virtual dispatch with the
    // side effects of one.
    // ds:A08F60 carries no null test. A mini-map that receives any ECHud event
    // before the PlayerManager exists faults here, exactly as shipped.
    hfe::world::playerManager->getLocalHumanPlayer();          // PlayerManager +0x30

    switch (id) {
    case 8:
        // Load the current level's minimap art, falling back to the default texture when absent. Requires game.
        setMinimapTexture(hfe::formatString("Levels/%s/Hud/Minimap/ingameMap.tga",
                                            hfe::game->getCurrentMapName().c_str()));
        return;

    case 2:
        // The HUD mode change. The mode is read from the HUD singleton
        // (ds:A10890 +1E0, 751A00 - a plain `return m_field250`) and handed to this
        // node's own +DC slot. That slot is virtual and slot 55 is one of this
        // class's twenty overrides, so it reaches 7CE3D0 above - not the map's
        // 777DC0, which is still a frontier. hudManager is dereferenced with no
        // null test, as shipped.
        setHudMode(hfe::hudManager->getMode());                // +1E0, then +DC 7CE3D0
        return;

    case 25: {
        // Ignore missing payloads before updating squad-request state.
        if (!data) return;
        const int* const words = static_cast<const int*>(data);
        setSquadRequestType(words[0], words[2]);                 // words[1] is not read
        return;
    }

    case 60: {
        // Unlike id 25, `data` itself is not tested: it is dereferenced
        // and only the first word - the marker id - is checked, so a null payload
        // faults here. Preserved.
        const int markerId = *static_cast<const int*>(data);
        if (!markerId) return;                                 // 7CF7EA, into the no-op
        MapIconRecord icon;
        icon.size = hfe::Vec2{4.0f, 4.0f};
        addTimedMarker(markerId, hfe::Vec2{0.0f, 0.0f}, icon,
                       m_fielda2c, m_fielda24, m_fielda28, 0.0f, 1);
        return;
    }

    default:
        return;                                                // 7CF8AD, the shared no-op
    }
}

} }
#endif
