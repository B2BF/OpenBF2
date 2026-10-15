// Owns floating player, order and object tags and batches them through three renderers.
// [INFERRED] header placement. Keep the primary, event-listener and device-listener base order.
// Host-crossing strings use HostString; init and all addTouchItem overloads return bool.
// The renderer interface includes an additional slot and preserves MSVC overload grouping.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_NAMETAGMANAGER_H
#define BF2_GEOM_NAMETAGMANAGER_H

#include <cstddef>
#include <map>
#include <set>
#include <vector>

#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include <string>
#include "../Core/IBase.h"
#include "../Core/WeakPtr.h"
#include "../Math/Vec3.h"
#include "../Math/Frustum.h"
#include "../Game/IEventListener.h"
#include "../Game/GameLogic/SquadManager/SquadOrder.h"
#include "../RendDX9/RendDX9Base.h"
#include "../Scene/Object/ObjectPredicator.h"

namespace dice {
namespace hfe {

namespace world {
class IPlayer;
class IObject;
class IObjectComponent;
class ControlPoint;
class IPlayerControlObject;
class IWeaponObject;             // queryWeaponObject's answer (IID 38080)
}

namespace geom {

class NameTagRenderer;           // nt_renderer's class - Geom/NameTagRenderer.h, 0x3860 bytes each in the image


// Console enum values select the first icon in each renderer icon group.

enum NametagItemIDs
{
    ICRank              = 0x00,   // + (rank + 1), 22 frames (nametags.con: setTexture 22 .../ranks)
    ICRevive            = 0x20,
    ICMedic             = 0x21,
    ICRepair            = 0x22,
    ICAmmo              = 0x23,
    ICVehicleMedic      = 0x24,
    ICVehicleRepair     = 0x25,
    ICVehicleAmmo       = 0x26,
    ICHealthBar         = 0x27,
    ICArmorBar          = 0x28,
    ICAmmoBar           = 0x29,
    ICSquadOrder        = 0x32,   // squad orders use 0x2E + orderId (0x1012C600), i.e. ids 4.. land here
    ICCommanderOrder    = 0x3C,   // commander orders 0x38 + orderId
    ICTarget            = 0x46,
    ICObject            = 0x50,   // + 3dMapIcon, 32 frames (nametags.con: setTexture 32 .../Objects)
    ICObjectVehicleDrop = 0x70
};

// The three renderers - also the `type` of every item and the index of every per-type table.
enum TagType
{
    TTPlayer = 0,     // renderer 0: initialize(0, 64, 4, &s_cullDistances[0])
    TTOrder  = 1,     // renderer 1: initialize(1, 64, 1, &s_cullDistances[1]) - but only TWO slots
    TTObject = 2      // renderer 2: initialize(2, 64, 5, &s_cullDistances[2])
};

// The item key spaces (the map key is `base + id`). Read from every add/remove body.
const int kNametagKeyPlayer = 0x10000000;   // + IPlayer::getId()              (PE +0x50)
const int kNametagKeyOrder  = 0x20000000;   // + the order type (0 squad, 1 commander)
const int kNametagKeyObject = 0x40000000;   // + IObject object id             (IObject +0x10)

// Flags controlling tag visibility, text, rank, team filtering and directional arrows.
// Names are [INFERRED]; unnamed NIF_BitN values remain [UNRESOLVED].
// Bit 15 forces friendly colouring when a local player exists, but its writer is [UNRESOLVED].
enum NametagItemFlag
{
    NIF_Visible            = 0x00000001,   // "draw this tag" - the add/refresh bodies set/clear it
    NIF_ShowRank           = 0x00000002,   // bufferItem: draw m_rankIcon
    NIF_Bit2               = 0x00000004,   // [nt_update] objects: show the object's own name
    NIF_ShowText           = 0x00000008,   // bufferItem: draw m_name even in arrow mode
    NIF_Bit4               = 0x00000010,   // [nt_update] players: same team as the local player
    NIF_SquadMember        = 0x00000020,   // refreshPlayerItem: same squad as the local player
    NIF_FollowTarget       = 0x00000040,   // OrderNametagItem: track m_target's position
    NIF_Bit7               = 0x00000080,
    NIF_UseHolderPosition  = 0x00000100,   // ObjectNametagItem: strategic holder position
    NIF_KindIcon           = 0x00000200,   // refreshPlayerItem: the icon is the soldier/vehicle one
    NIF_Bit10              = 0x00000400,   // projectile/deployable tags; [nt_update] team-gated fade
    NIF_Bit11              = 0x00000800,
    NIF_Bit12              = 0x00001000,
    NIF_Bit13              = 0x00002000,
    NIF_Bit14              = 0x00004000,
    NIF_Bit15              = 0x00008000,   // updateItemSelection reads it
    // Bars and icons are shown only for the vehicle's primary occupant.
    NIF_PrimaryOccupant    = 0x00010000,
    NIF_CallActive         = 0x00020000,   // a call-for-help icon is showing
    // named (was NIF_Bit18): set only by addTouchItem(ControlPoint*)'s
    // 0x4001D; its two readers are the control-point arms above. [INFERRED] name.
    NIF_ControlPoint       = 0x00040000
};

// Item +0xA0: three doubles the update closure's two fade helpers drive (0x1012CC50 / 0x1012CCF0
// read `this->m_time - fade.m_time` and write `fade.m_value`). Field roles [INFERRED].
struct NametagFade
{
    // Accumulated invisible time before fade-in; fade-out resets it.
    double m_sightTime;   // +0x00
    double m_value;       // +0x08  the faded alpha, clamped to [0, 1]; getAlphaArray(type)[slot] = it
    double m_time;        // +0x10  the manager time of the last fade step (every fade body stores
                          //        m_time here last - 0x1012E7F6 / 0x1012ED05)
};


// Base nametag record, 0xC0 bytes in the image (0x10131360 lays it out). Renderer buffering,
// drawing and the update helpers read its typed members.

class NametagItem
{
public:
    NametagItem(int type, int slot);

    // the vtable, in slot order
    // +0x00: compute and return the tag's world position (the derived bodies store it at +0xC4 /
    // +0xF4 and return that address; the player item returns Vec3::getZero() when its player is
    // gone). [INFERRED] name.
    virtual const Vec3& updatePosition() = 0;
    // +0x04: the item's map key (kNametagKey* + id) - insertItem files the item under it.
    virtual int getKey() = 0;
    // +0x08: the scalar deleting destructor (0x1012C2D0; body 0x1012C2B0 destroys the two strings).
    virtual ~NametagItem();

    // Returns true for an object tag whose live template is a UAV vehicle.
    bool isUAV();

    // data - the `+0xNN` comments give the shipped x86 offsets. The members up to m_name still sit
    // at them; the std::string members make every later native offset differ.
    // +0x04 is NOT a member: it is MSVC's padding after the vfptr of a class whose alignment is 8
    // (the doubles below). That is why no recovered body ever writes it - and this compiler
    // reproduces it, which the static_asserts below prove.
    int          m_slot;             // +0x08  renderer slot (allocateSlot), -1 = none
    int          m_type;             // +0x0C  TagType - selects renderer / slot tables
    std::string m_name;             // +0x10  the text the renderer draws (0x101B2320 fonts +0x10)
    std::string m_text2C;           // +0x2C  a second string [UNRESOLVED role - update closure]
    Vec3         m_offset;           // +0x48  added to the owner position (player: 0; CP: y = pole
                                     //        height + 1; objects: y = radius or 1.2)
    int          m_distance;         // +0x54  handleEvent tests `(float)m_distance < 150`
                                     //        (s_maxCallDistance) - the update closure writes it
    double       m_viewDistance;     // +0x58  per-item cull distance (300 / 1200 / 40 / 100 ...)
    int          m_rankIcon;         // +0x60  -1; players: rank + 1 (0x1012DE50); bufferItem's icon
    int          m_colorIndex;       // +0x64  -1; index into getColors(type): 0 white, 1 enemy
                                     //        red, 2 friendly blue, 3 squad green, 4 yellow
    int          m_icon;             // +0x68  -1; the main icon (NametagItemIDs group + offset)
    int          m_callIcon;         // +0x6C  -1; 0x20..0x23 while a call-for-help shows
    int          m_flashColorIndex;  // +0x70  -1; the second (cross-fade) colour index, -1 = m_colorIndex
    float        m_timer;            // +0x74  0; call icon / flash time left (12, 30, 100 s)
    // Second icon indicating a control point more than two meters above/below the local vehicle.
    int          m_elevationIcon;    // +0x78  -1; stored at 0x1012CDF8 / 0x1012CE13
    float        m_iconRotation;     // +0x7C  0; a float: the update closure
                                     //        stores it with `fstp dword [ebx+7Ch]` (0x1012E6B2 /
                                     //        0x1012E6B9) and drawIcons (0x101B1FDD) passes it to
                                     //        D3DXMatrixRotationZ when flag 0x200 is set. [INFERRED name]
    int          m_team;             // +0x80  0; the owner's team
    // Timestamp of the last line-of-sight check; checks are spaced by at least 0.5 seconds.
    double       m_lastSightCheckTime;   // +0x88  0; insertItem: (seeded Random step) * 2^-25;
                                         //        stored at 0x1012E88D
    Vec3         m_worldPosition;    // +0x90  current world-space tag position (0x1012CD8D..9B)
    // Control-point height above the local vehicle, rounded by truncating delta + 0.5.
    // Written with the elevation icon selection; not otherwise read.
    int          m_heightDelta;      // +0x9C  0; stored at 0x1012CDF0
    NametagFade  m_fade;             // +0xA0..+0xB7  0, 0, 0
    unsigned int m_flags;            // +0xB8  7 - NametagItemFlag
    bool         m_arrowMode;        // +0xBC  0; set = drawn as the off-screen arrow: the renderer's
                                     //        drawArrows (0x101B0DC0) skips items where it is 0, and
                                     //        bufferItem skips m_name while it is set (unless bit 3).
                                     //        [INFERRED name - nt_renderer's reading; nt_update writes it]
    bool         m_dirty;            // +0xBD  1; re-buffer requested (update/enableBuffering set it)
    // Current frustum-test result, required for bar rendering.
    bool         m_inView;           // +0xBE  0; stored at 0x101305FE
    // renamed from m_unknownBF: isEnemyInSight's cached answer - in the
    // view cone (s_enemyDotLimit) AND a clear line of sight, re-tested at most every 0.5 s
    // (0x1012E8B1 / 0x1012E8BC). [INFERRED] name.
    bool         m_inSight;          // +0xBF  0

private:
    NametagItem(const NametagItem&);
    NametagItem& operator=(const NametagItem&);
};

// Type 0. vtable 0x1021D080, 0xD0 bytes (operator new(0xD0) in 0x10130820).
class PlayerNametagItem : public NametagItem
{
public:
    PlayerNametagItem(world::IPlayer* player, int slot);
    virtual const Vec3& updatePosition();
    virtual int getKey();
    virtual ~PlayerNametagItem();                             // (folded with the object item's)

    // Returns the live player referenced by the item.
    world::IPlayer* getPlayer();

    WeakPtr<world::IPlayer> m_player;                         // +0xC0  player->getWeakPtr()
    Vec3                    m_position;                       // +0xC4  updatePosition's result
};

// Type 2. vtable 0x1021D090, 0xD0 bytes - control points, UAVs, strategic objects, deployables,
// supply objects and vehicle drops all share it (0x10130880 builds it inline; the out-of-line copy
// 0x1012DA50 has no caller).
class ObjectNametagItem : public NametagItem
{
public:
    ObjectNametagItem(world::IObject* object, int slot);      // 0x1012DA50, 63 B, retn 8 (unreferenced;
                                                              //   0x10130880 inlines the same)
    virtual const Vec3& updatePosition();
    virtual int getKey();
    virtual ~ObjectNametagItem();

    // isUAV (0x1012D7D0) moved to NametagItem - see there.

    WeakPtr<world::IObject> m_object;                         // +0xC0  object->getWeakPtr()
    Vec3                    m_position;                       // +0xC4
    // +0xC4..+0xCF also zeroed by the inline constructor (0x101308D4..0x101308E0)
};

// Type 1. vtable 0x1021D09C (three slots: 0x1012DDE0, 0x1012C370, 0x10131830), 0x108 bytes
// (operator new(0x108) in 0x10130910 / 0x10130970).
class OrderNametagItem : public NametagItem
{
public:
    OrderNametagItem(const SquadOrder* order, int orderType, const Vec3& position, int slot);
    OrderNametagItem(const SquadOrder* order, int orderType, world::IObject* target, int slot);
    // The two re-issue bodies - see the note at their callers in NametagManager.cpp.
    void setOrder(const SquadOrder* order, const Vec3& position);
    void setOrder(const SquadOrder* order, world::IObject* target);
    virtual const Vec3& updatePosition();
    virtual int getKey();
    virtual ~OrderNametagItem();

    SquadOrder              m_order;                          // +0xC0  0x30 bytes (copy 0x10131440)
    int                     m_orderType;                      // +0xF0  0 squad, 1 commander
    Vec3                    m_targetPosition;                 // +0xF4
    WeakPtr<world::IObject> m_target;                         // +0x100
};


// Ray filter excluding a root and optionally soldiers, with caller-supplied object flags.

class NametagRayPredicator : public world::ObjectPredicator
{
public:
    NametagRayPredicator(world::IObject* ignoredRoot, unsigned int flags, bool ignoreSoldiers)
        : m_ignoredRoot(ignoredRoot), m_flags(flags), m_ignoreSoldiers(ignoreSoldiers) {}

    virtual bool         predicate(world::IObject* object);
    virtual unsigned int getFlags() const { return m_flags; }
    virtual bool         performCulling(world::IObject*) { return true; }
    virtual bool         predicateSurface(int) { return true; }

    world::IObject* m_ignoredRoot;      // +0x04
    unsigned int    m_flags;            // +0x08
    bool            m_ignoreSoldiers;   // +0x0C
};


// The manager's containers. As members they are constructed and destroyed in member order - which
// is exactly the order 0x1012F810 builds them and 0x1012F5A0 tears them down (see the destructor's
// note).

// key -> item (head sub_10131690, destroy sub_10131A40). The image's copy of a map<int, 4-byte>,
// 0x101327D0, is called by the update closure (0x1012FC70, 0x10130290) and, through 0x10132860, by
// the renderer (0x101B33A0); 0x10132580 destroys such a copy and 0x10132280 is its
// erase(first, last). Which map those callers copy is theirs to establish - the node shape is the
// same for this map and NametagSlotMap.
typedef std::map<int, NametagItem*> NametagItemMap;

// key -> slot (head sub_1008A320, destroy sub_1008A450).
typedef std::map<int, int> NametagSlotMap;

// The slots not in use (head sub_101315F0, destroy sub_10131B00). Nothing in the image copies one.
typedef std::set<int> NametagFreeSlotSet;

// The per-TagType pointers to the static float tables. Only ever resized once, to three nulls
// (sub_10132390(3, 0)).
typedef std::vector<float*> NametagTableVector;


// NametagManager - 0x5D8 bytes.

class NametagManager : public IBase, public IEventListener, public dx9rend::DeviceLostResetListener
{
public:
    NametagManager();
    ~NametagManager();

    // IBase, primary +0x00..+0x0C
    virtual void  addRef();                                            // +0x00
    virtual int   getRef();                                            // +0x04
    virtual int   release();                                           // +0x08
    virtual void* queryInterface(unsigned int iid) const;              // +0x0C

    // INametagManager, +0x10..+0x68
    // Returns whether all five control-point icons registered (0x1012D610's count check).
    virtual bool  init(const renddx9abi::HostString& team1,
                       const renddx9abi::HostString& team2);           // +0x10
    virtual void  update();                                            // +0x14 (nt_update)
    virtual void  draw();                                              // +0x18

    // Keep declaration order: MSVC reverses same-name overloads in the vtable,
    // placing (IObject*, int) at +0x1C and (IPlayer*) at +0x30.
    virtual bool  addTouchItem(world::IPlayer* player);                              // +0x30
    virtual bool  addTouchItem(world::ControlPoint* controlPoint);                   // +0x2C
    virtual bool  addTouchItem(world::IObject* object);                              // +0x28
    virtual bool  addTouchItem(SquadOrder* order, int orderType, const Vec3& position); // +0x24
    virtual bool  addTouchItem(SquadOrder* order, int orderType, world::IObject* target); // +0x20
    virtual bool  addTouchItem(world::IObject* object, int team);                    // +0x1C

    virtual float* getColors(int type);                                // +0x34
    virtual float* getTransformations(int type);                       // +0x38
    virtual float* getAlphaArray(int type);                            // +0x3C
    virtual void  rebufferAll();                                       // +0x40
    virtual void  enableBuffering();                                   // +0x44
    virtual void  clearAll();                                          // +0x48
    virtual void  removeItem(int type, void* item);                    // +0x50
    virtual void  removeItem(int type, void* item, int orderType);     // +0x4C
    virtual const void* getItems() const;                              // +0x54
    virtual bool  get3dMapOn() const;                                  // +0x58
    virtual void  set3dMapOn(bool on);                                 // +0x5C
    virtual void  setSpectatedPlayer(world::IPlayer* player);          // +0x60
    virtual world::IPlayer* getSpectatedPlayer() const;                // +0x64
    virtual bool  isDemoPlayback() const;                              // +0x68

    // RendDX9's own slot
    // +0x6C hide and delete every item, then clearAll(). init() runs it first and the
    // destructor runs it last. [INFERRED] name - no caller outside this class.
    virtual void  removeAllItems();

    // IEventListener, secondary table 0x1021D0F4 (`this` = +0x04)
    virtual void  handleEvent(EventCategory category, unsigned int id,
                              EventNode* node, void* data);

    // dx9rend::DeviceLostResetListener, secondary table 0x1021D0E8 (`this` = +0x08)
    virtual void  onDeviceLost();
    virtual void  onDeviceReset();                                     // then rebufferAll
    virtual void  onDeviceDestroy();

    // the `nametags` console surface (non-virtual; the executors load ecx = the singleton)
    void  createBar(int itemId, int barStart, int barEnd);
    void  createIcon(int itemId);
    void  setTexture(int count, std::string path, int x, int y, int width, int height);
    const Vec2& getCullDistance(int type);
    void  setCullDistance(int type, const Vec2& distance);
    float  getEnemyDotLimit();               void setEnemyDotLimit(float v);
    float  getMaxCallDistance();             void setMaxCallDistance(float v);
    float  getVehicleNametagOffset();        void setVehicleNametagOffset(float v);
    float  getVehicleNametagOffsetFactor();  void setVehicleNametagOffsetFactor(float v);
    float  getMaxFriendlyDistance();         void setMaxFriendlyDistance(float v);
    double getEnemyTagDelayTime();           void setEnemyTagDelayTime(double v);
    double getDeployableDistance();          void setDeployableDistance(double v);
    double getEnemyTagFadeInTime();          void setEnemyTagFadeInTime(double v);
    double getEnemyTagFadeOutTime();         void setEnemyTagFadeOutTime(double v);
    float  getDeathFadeOutTime();            void setDeathFadeOutTime(float v);
    float  getManDownFadeOutTime();          void setManDownFadeOutTime(float v);
    // no caller and no console binding in the image. [INFERRED] name.
    bool   getUseVisualRadiusForVehicles();  void setUseVisualRadiusForVehicles(bool v);

    // helpers shared with the update closure (nt_core writes these)
    // hide an item's call state (flags &= 0xFFFF87D7, m_callIcon = m_flashColorIndex =
    // -1, m_timer = 0, orders |= 0x40B9, m_dirty = 1). Null-safe.
    void  resetItem(NametagItem* item);
    // kNametagKeyPlayer + player->getId().
    int   playerKey(world::IPlayer* player);
    // the player's default vehicle as a soldier -> getKitType(), else 13.
    int   playerKitType(world::IPlayer* player);
    // rank icon of a player item: 1 + rank when a local player exists and is not the
    // item's player, else -1. The item's player is NOT null-tested.
    int   playerRankIcon(PlayerNametagItem* item);
    // 0x1012C410, 55 B, retn 4 - the same test for a bare IPlayer*. NO caller in the image (no direct
    // call, no address reference); 0x1012DE50 carries its own copy. [INFERRED] name.
    int   playerRankIconOf(world::IPlayer* player);
    // 0x1012C450, 189 B - an ltcg fastcall (ecx = object, dl = childrenOnly), `ret`. Its only callers
    // are its own two recursive calls (0x1012C484 / 0x1012C497): the largest visual bounding radius
    // over the object's getChild()/+0x2C sibling hierarchy, and the object's own unless childrenOnly.
    // [INFERRED] name.
    static float hierarchyVisualRadius(world::IObject* object, bool childrenOnly);
    // 0x1012C330, 21 B - an ltcg fastcall (ecx = object), `ret`: the first non-null IObject +0x3C
    // (the armor component) walking up the +0x28 parent chain; 0 at the root. Callers: the update
    // closure's updateHudTarget (0x1012D404 / 0x1012D57F) and the renderer's armor-bar draw
    // (0x101B35B4). Public because the renderer (nt_draw) calls it. [INFERRED] name.
    static world::IObjectComponent* findArmorComponent(world::IObject* object);
    // recompute a player item's icon / colour / visibility. Returns "changed".
    bool  refreshPlayerItem(PlayerNametagItem* item);
    // the per-type slot count (64 / 2 / 64 / -1). No caller in the image - the ctor
    // and clearAll inline it.
    int   maxSlotCount(int type);
    // tick an item's m_timer down by the frame delta, resetItem at zero. Unreferenced
    // in the image (IDA left it undefined).
    void  tickItemTimer(NametagItem* item);
    // Dispatch position and fade updates according to the item type.
    void  updateItemPosition(NametagItem* item);
    void  updateItemFade(NametagItem* item);

    // add/remove plumbing (nt_core)
    int          allocateSlot(int type, int key);
    NametagItem* insertItem(NametagItem* item);
    void         removeKey(int type, int key);
    NametagItem* createPlayerItem(world::IPlayer* player);
    NametagItem* createObjectItem(world::IObject* object);
    NametagItem* createOrderItem(SquadOrder* order, int orderType, const Vec3& position);
    NametagItem* createOrderItem(SquadOrder* order, int orderType, world::IObject* target);
    int          orderIcon(int orderType, int orderId);
    void         resetAllCalls();
    bool         registerControlPointIcons(const renddx9abi::HostString& team1,
                                           const renddx9abi::HostString& team2);

    // Per-frame visibility, distance, and fade helpers.
    bool  isPlayerSoldierDown(world::IPlayer* player);       // 0x1012C380, 74 B, retn 4: the current vehicle
                                                             //   (+0x44, not the default one) is a soldier
                                                             //   (CID 38035) whose armour getStatus() == 1
    // Was fadeTowards(fade, from, to): the doubles are a delay (compared with m_sightTime) and a
    // fade-in duration (the elapsed time is divided by it). [INFERRED] names.
    void  fadeIn(NametagFade* fade, double delay, double duration);    // 0x1012CC50, 150 B, retn 14h
    void  fadeOut(NametagFade* fade, double duration);        // 0x1012CCF0, 92 B, retn 0Ch
    void  updateObjectItem(NametagItem* item);                // 0x1012CD50, 369 B, retn 4 (ecx unused); void
    Vec4  projectToScreen(Vec3 worldPosition, NametagItem* item);   // 0x1012CED0, 688 B, retn 14h - hidden sret,
                                                              //   Vec3 BY value; reads "GSNameTagCircleRadius"
    bool  isInViewCone(world::IObject* object, float dotLimit);   // 0x1012D180, 230 B, retn 8 (ecx unused)
    world::IObject* findCrosshairTarget(float maxDistance);  // 0x1012D270, 268 B, retn 4 (ecx unused)
    // Was (world::IObject* target, NametagItem* item): 0x1012E3D5 pushes the item's player then the
    // local player - (local, player). The HUD kit is the local player's; the checks run on `player`.
    void  updateHudTarget(world::IPlayer* local, world::IPlayer* player);   // 0x1012D380, 604 B, retn 8
    void  updateItemSelection(NametagItem* item);             // 0x1012E350, 235 B, retn 4; void
    // Was updatePlayerItemVisibility: it computes the player tag's distance, its icon rotation and
    // its world position (+0x90) - no visibility decision. [INFERRED] name.
    void  updatePlayerItemPosition(NametagItem* item);        // 0x1012E4F0, 501 B, retn 4; void
    void  updateObjectItemFade(NametagItem* item);            // 0x1012E6F0, 306 B, retn 4; void
    bool  isEnemyInSight(NametagItem* item);                  // 0x1012E830, 160 B, retn 4 (s_enemyDotLimit)
    void  updatePlayerItemFade(NametagItem* item);            // 0x1012E8F0, 1,092 B, retn 4; void
    void  updateLocalVehicleSeats(NametagItem* item);         // 0x1012FD20, 107 B, retn 4 (s_lastSeatCount)
    // Was updateItemTransform: it builds m_name / m_text2C and rebuffers the item's row (bufferItem
    // true, then m_dirty = 0) - no transformation is touched. [INFERRED] name.
    void  rebuildItemText(NametagItem* item);                 // 0x1012FD90, 902 B, retn 4; void
    // Was updateSpectatedItem(player) "an LTCG custom convention": it is a plain __thiscall (edi is
    // only saved and reused as the map head) that answers "is `player` the first occupied seat of
    // its vehicle root" - the NIF_PrimaryOccupant test. Nothing about spectating. [INFERRED] name.
    bool  isPrimaryOccupant(world::IPlayer* player);          // 0x10130290, 201 B, retn 4
    // Free (fastcall/stdcall) helpers of the closure:
    // Returns 0, or -20001 (0xFFFFB1DF) after zeroing a degenerate vector; the one caller discards it.
    static int    normalize4(Vec4& v);                        // 0x1012C1F0, 157 B, `ret`, ecx = the Vec4
    static world::IWeaponObject* queryWeaponObject(IBase* component);   // 0x1012C290, 20 B, fastcall:
                                                              //   c ? c->queryInterface(38080) : 0
    // Snaps position.x/.y (clip space) to the render view's pixel centres; z and w untouched.
    static void   projectOntoView(Vec4& position, dx9rend::IRenderView* view);
                                                              //   fastcall(ecx = &position, edx = view)
    // Was wrapAngle(double): it reads the Ra constants block's FogRange (+0xA8..+0xB4) and returns
    // saturate(max(d*y + w, w) - (d*x + z)^3) - the CPU twin of RaCommon.fx's calcFog. [INFERRED] name.
    static double calcFog(double distance);                   // 0x1012E440, 169 B, retn 8
    static int    countSeats(world::IPlayerControlObject* pco);  // 0x1012FC70, 162 B, ecx = the PCO: the
                                                              //   occupied seats of a copy of getPcos()
    static double clampValue(double value, double low, double high);   // 0x10131320, 50 B, retn 18h
    // NEW - 0x1012CB40, 269 B, `ret`, ecx = the target object: true when the
    // camera-to-target ray hits nothing (`sete al`). [INFERRED] name. See NametagItem.
    static bool   isLineOfSightClear(world::IObject* target);
    // Returns the soldier's default weapon, adapting native and compatibility layouts.
    static IBase* soldierDefaultWeapon(world::IPlayerControlObject* control);

    // the data, in shipped order
    // The `+0xNN` comments give the shipped x86 offsets. The std containers make every native
    // offset from m_colors on differ; the static_asserts after the class hold the ones that agree.
    int                     m_refCount;                 // +0x00C  1
    double                  m_time;                     // +0x010  0.0; update: += frame delta
    NametagTableVector      m_transformations;          // +0x018  float* per TagType (first +0x1C)
    NametagTableVector      m_colors;                   // +0x028  float* per TagType (first +0x2C)
    float                   m_alphaArrays[3][64];       // +0x038  getAlphaArray(t) = &[t][0]
    NametagItemMap          m_items;                    // +0x338  key -> item (head +0x33C)
    NameTagRenderer*        m_renderers;                // +0x344  new NameTagRenderer[3]
    bool                    m_bufferingEnabled;         // +0x348  0; enableBuffering sets it; update
                                                        //         returns at once while it is clear
    bool                    m_3dMapOn;                  // +0x349  1
    unsigned char           m_pad34A[2];
    NametagFreeSlotSet      m_freeSlots[3];             // +0x34C  per TagType: slots not in use
    NametagSlotMap          m_slotMaps[3];              // +0x370  per TagType: key -> slot
    int                     m_textureMode;              // +0x394  0 icon, 1 bar (createIcon/Bar)
    int                     m_currentItemId;            // +0x398  -1; the NametagItemIDs being set up
    int                     m_pendingBarTexture;        // +0x39C  -1; 0 once the bar's first texture
                                                        //         is stored, back to -1 when used
    int                     m_barStart;                 // +0x3A0  0; createBar's second argument
    int                     m_barEnd;                   // +0x3A4  0; createBar's third argument
    std::string            m_pendingBarPath;           // +0x3A8  the bar's first texture path
    int                     m_pendingBarRect[4];        // +0x3C4  0; its x, y, width, height
    int                     m_pad3D4;                   // +0x3D4  never touched
    double                  m_lastTargetCheckTime;      // +0x3D8  0.0; update: every 0.5 s
    WeakPtr<world::IPlayer> m_spectatedPlayer;          // +0x3E0  setSpectatedPlayer
    world::IObject*         m_targetRoot;               // +0x3E4  0; update: the crosshair target's root
    Frustum                 m_frustum;                  // +0x3E8  update: the view frustum, far 2000
    bool                    m_isDemoPlayback;           // +0x5D0  0; update: game->isPlayingDemo()
    unsigned char           m_pad5D1[7];

    // the TU's statics (NametagManager.cpp owns the definitions)
    // Shipped as file-scope .data/.bss in this TU; the update closure and the renderer's draw read
    // several, so they are published here. Address, default and every reader are recorded.
    static float  s_maxCallDistance;            // 0x10239A0C 150    handleEvent, console, NameTagRenderer 0x101B1BB0
    static float  s_callIconTime;               // 0x10239A10 12     call icon time / pulse-rate seed [INFERRED name]
    static float  s_enemyDotLimit;
    static double s_enemyTagDelayTime;
    static double s_enemyTagFadeInTime;
    static double s_enemyTagFadeOutTime;
    static double s_deployableDistance;         // 0x10239A30 40.0   addTouchItem(IObject*)
    static float  s_supplyObjectDistance;       // 0x10239A38 100    addTouchItem(IObject*) [INFERRED name]
    static float  s_maxFriendlyDistance;
    static float  s_vehicleNametagOffset;       // 0x10239A40 0.45   PlayerNametagItem::updatePosition
    static float  s_vehicleNametagOffsetFactor; // 0x10239A44 0.35   PlayerNametagItem::updatePosition
    static float  s_manDownFadeOutTime;
    static float  s_deathFadeOutTime;
    static float  s_normalizeEpsilon;           // 0x10239A50 0x28800000 (1.42e-14) - 0x1012C1F0 [INFERRED]
    static Vec2   s_cullDistances[3];           // (100,300) (100,1200) (0,1200)
    static int    s_unknown10239A98;            // 0x10239A98 2      0x1012CD50 [UNRESOLVED name]
                                                // the elevation-icon threshold
                                                //   in metres (m_elevationIcon above); its only
                                                //   reference in the image is that read (0x1012CDE8) -
                                                //   no writer, no console binding

    static float  s_pulse;                      // 0x1025C210 bss    update writes; NameTagRenderer
                                                //                   0x101B0750/0x101B11F0/0x101B1BB0 read
    static double s_medicCallTimer;             // 0x1025C218 bss    the local player's own call, s
    static double s_ammoCallTimer;              // 0x1025C220 bss
    static double s_repairCallTimer;            // 0x1025C228 bss
    static bool   s_useVisualRadiusForVehicles; // 0x1025C230 bss    PlayerNametagItem::updatePosition
    static float  s_transformations1[2][4];     // 0x1025C240        renderer 1's TransformationArray
    static float  s_colors0[5][4];              // 0x1025C260        renderers 0 and 2 ColorArray
    static float  s_colors1[1][4];              // 0x1025C2B0        renderer 1 ColorArray
    static float  s_transformations0[64][4];    // 0x1025C5B8        renderer 0
    static float  s_pulseRate;                  // 0x1025C9B8        dynamic init 0x101D3E20
    static float  s_transformations2[64][4];    // 0x1025C9C0        renderer 2
    static int    s_lastSeatCount;              // 0x1025CEA0 bss    0x1012FD20 [INFERRED name]

private:
    NametagManager(const NametagManager&);
    NametagManager& operator=(const NametagManager&);
};

// The shipped offsets that still hold: the predicator, the frustum, and the members before the
// first std container. The items and the manager hold std containers, so their sizes and later
// offsets are native (the `+0xNN` comments keep the shipped ones).
#if defined(_M_IX86)
static_assert(sizeof(NametagRayPredicator) == 0x10, "0x1012CC27 builds 16 bytes");
static_assert(sizeof(Frustum) == 0x1E8, "the embedded frustum runs +0x3E8..+0x5CF");

static_assert(offsetof(NametagItem, m_slot) == 0x08, "item slot at +0x08");
static_assert(offsetof(NametagItem, m_type) == 0x0C, "item type at +0x0C");
static_assert(offsetof(NametagItem, m_name) == 0x10, "item name at +0x10");
static_assert(offsetof(NametagFade, m_sightTime) == 0x00 && offsetof(NametagFade, m_value) == 0x08
              && offsetof(NametagFade, m_time) == 0x10, "the fade triple at item +0xA0/+0xA8/+0xB0");

static_assert(offsetof(NametagManager, m_refCount) == 0x00C, "refcount at +0x0C");
static_assert(offsetof(NametagManager, m_time) == 0x010, "time at +0x10");
static_assert(offsetof(NametagManager, m_transformations) == 0x018, "transformation vector at +0x18");
#endif

// ModuleGeom.cpp's g_NametagManager (the "NametagManager" singleton slot). NametagManager derives
// from IBase first, so the IBase subobject is at offset zero and the downcast adjusts nothing.
extern IBase* g_NametagManager;
inline NametagManager* nametagManager()
{
    return static_cast<NametagManager*>(g_NametagManager);
}

// The renderer's frame delta (g_Renderer +0x5E8, double) - update() accumulates it and the
// timer tick subtracts it. The same raw read BundledMeshDraw.cpp:571 makes.
double nametagFrameDelta();

// Creates the nametag manager interface.
IBase* __fastcall createNametagManager(unsigned int iid, IBase* outer);

// NametagManagerConsole.cpp (native flavour only - the tree's console-object rule).
bool registerNametagManagerConsoleObjects();
bool unregisterNametagManagerConsoleObjects();

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_NAMETAGMANAGER_H
#endif // BF2_IMAGE_RENDDX9
