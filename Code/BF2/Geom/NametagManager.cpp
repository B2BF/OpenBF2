// Floating-tag lifecycle, event handling and item updates.
// Profiling brackets are omitted. Reissued orders retain the original unchanged-item result.
// OrderNametagItem::setOrder keeps a named stop for the null-target arm;
// callers must supply a target.

#ifdef BF2_IMAGE_RENDDX9

#include "NametagManager.h"
#include "NameTagRenderer.h"                  // nt_renderer - the per-TagType batch renderer
#include "StaticMesh.h"                       // staticMeshRootParent - sub_10103CE0

#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../Core/EventManager.h"
#include "../Core/Ids.h"
#include "../Core/Random.h"

// The engine interfaces below are imported the way XpLightManager.cpp / BundledMeshDraw.cpp do it:
// only their virtual interfaces cross the plugin boundary; in the retail-compatible flavour HfeAbi
// already supplies ClassManager's VC7.1 carriers, so Core/ClassManager.h is kept out.
#include "../Game/IGame.h"
#include "../Game/Objects/ObjectLayoutAccess.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ISoldier.h"
#include "../Game/Objects/IControlPoint.h"
#include "../Game/Objects/IControlPointTemplate.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Game/Objects/IPlayerControlObjectTemplate.h"
#include "../Game/Objects/IStrategicObjectComp.h"
#include "../Game/Objects/IVehicleHudComp.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/PlayerScoreData.h"
// nt_update: the update closure's host interfaces.
#include "../Game/Objects/ICameraObject.h"
#include "../Game/Objects/IWeaponObject.h"
#include "../Game/Objects/GameLogicComp/IAbilityGLComp.h"
#include "../Game/Objects/GameLogicComp/IArmorGLComp.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
// Native flavour only: the host is built from these very headers, so the compiler's own layout of
// these classes IS the host's layout. The retail-compatible flavour reads the retail offsets
// instead (see the host seams below) and must not see these classes' native-STL layouts.
#include "../Game/Objects/Game/ControlPoint.h"
#include "../Game/Objects/SupplyObjectTemplate.h"
#include "../Game/Objects/Projectiles/GenericProjectileTemplate.h"
#include "../Game/Objects/Projectiles/GenericProjectile.h"
#include "../Game/Objects/Game/Soldier.h"            // nt_update: m_weapon2 (a friend line)
// RW-266: the host seams below, typed in this flavour.
#include "../Core/SettingsRepostitory.h"             // settingsFloat: floatGet (+0x20)
#include "../Game/Objects/Game/Kit.h"                // hudPlayerKit: the Kit -> IObject upcast
#include "../Game/Objects/StrategicObjectComp.h"     // addTouchItem: m_holder (a friend line)
#include "../Menu/BF2HudManager.h"                   // getPlayerKit (+0x90) / enableVOHelpMessage (+0xF8)
#include "../Menu/ISwiffHost.h"                      // getLocalizedString (+0xA8)

#include "../Math/Mat4.h"                            // nt_update
#include "../RendMisc/IRenderer.h"                   // nt_update: Renderer +0xB0
#include "../RendMisc/IRenderView.h"                 // nt_update: +0x14/+0x64/+0x68/+0x6C/+0x74

#include <cmath>                                     // nt_update
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>                                       // nt_update
#include <new>
#include <string>
#include <vector>

namespace dice {
namespace hfe {

extern IBase* playerManager;                  // PluginEntry.cpp
extern IBase* objectManager;                  // PluginEntry.cpp (nt_update)

namespace dx9rend {
extern IBase* g_Renderer;                     // BF2Render.cpp
extern void* g_rendererBase;                  // RendDX9Init.cpp (nt_update)
extern void* g_raShaderConstants;             // RenderQueues.cpp (nt_update)
}

namespace geom {


// THE TU'S statics - definitions. See the header for readers and addresses.

float  NametagManager::s_maxCallDistance            = 150.0f;
float  NametagManager::s_callIconTime               = 12.0f;
float  NametagManager::s_enemyDotLimit              = 0.99999f;
double NametagManager::s_enemyTagDelayTime          = 1.0;
double NametagManager::s_enemyTagFadeInTime         = 1.0;
double NametagManager::s_enemyTagFadeOutTime        = 1.0;
double NametagManager::s_deployableDistance         = 40.0;
float  NametagManager::s_supplyObjectDistance       = 100.0f;
float  NametagManager::s_maxFriendlyDistance        = 120.0f;
float  NametagManager::s_vehicleNametagOffset       = 0.45f;
float  NametagManager::s_vehicleNametagOffsetFactor = 0.35f;
float  NametagManager::s_manDownFadeOutTime         = 14.0f;
float  NametagManager::s_deathFadeOutTime           = 0.6f;
float  NametagManager::s_normalizeEpsilon           = 1.42108547152020037174e-14f;
Vec2   NametagManager::s_cullDistances[3]           = { { 100.0f, 300.0f },
                                                        { 100.0f, 1200.0f },
                                                        { 0.0f,   1200.0f } };
int    NametagManager::s_unknown10239A98            = 2;
float  NametagManager::s_pulse                      = 0.0f;
double NametagManager::s_medicCallTimer             = 0.0;
double NametagManager::s_ammoCallTimer              = 0.0;
double NametagManager::s_repairCallTimer            = 0.0;
bool   NametagManager::s_useVisualRadiusForVehicles = false;
float  NametagManager::s_transformations1[2][4];
float  NametagManager::s_colors0[5][4];
float  NametagManager::s_colors1[1][4];
float  NametagManager::s_transformations0[64][4];
// the TU's dynamic initialiser: `fld A10; fdiv A10; fmul 2pi; fstp C9B8`. The same
// global read twice, so the rate is always 2pi (as a float) whatever the call icon time is.
float  NametagManager::s_pulseRate = NametagManager::s_callIconTime / NametagManager::s_callIconTime
                                     * 6.28318548f;
float  NametagManager::s_transformations2[64][4];
int    NametagManager::s_lastSeatCount              = 0;
// (0x1025C2C0..0x1025C5B4 hold the squad-name strings "Unassigned", "Alpha" ... that a shared game
// header defines in every including TU - sub_101D39A0. Nothing in this TU reads them.)

namespace {

// icon indices the bodies build that the console enum does not name
const int kIconControlPoint     = 0x2A;   // 42 + team (0 neutral, 1, 2): "miniMap_CP"
const int kIconControlPointBase = 0x2D;   // 45 + team: "miniMap_CPBase" (45 itself is never registered)
const int kIconSquadOrderBase   = 0x2E;   // squad orders, + orderId
const int kIconCommanderOrderBase = 0x38; // commander orders, + orderId
const int kIconUnknownOrder     = 0x32;   // 0x1012C600's fallback
const int kIconSoldier          = 0x5A;   // the on-foot / "player icon" vehicle entry
const int kIconSquadLeaderBase  = 0x69;   // squad leader: + squad id; the commander uses 0x69 itself

// the kit types the call-for-help logic keys on ([INFERRED] from the icon each one unlocks)
const int kKitSupport  = 3;               // sees ICAmmo calls
const int kKitEngineer = 4;               // sees ICRepair calls
const int kKitMedic    = 5;               // sees ICMedic and ICRevive calls
const int kKitNone     = 13;              // 0x1012C3D0 / ISoldier::getKitType's "no kit"

// the ECHud / ECGame ids handleEvent answers (names: BF2HudManager.cpp:836.., GameClient.cpp)
const unsigned int kGameEventPlayerSpawned   = 6;    // ECGame 6 - posted with the IPlayer*
const unsigned int kHudEventCallForMedic     = 46;   // ECHudHECallForMedic
const unsigned int kHudEventCallForAmmo      = 47;   // ECHudHECallForAmmo
const unsigned int kHudEventCallForRepair    = 48;   // ECHudHECallForRepair
const unsigned int kHudEventCallForRevive    = 49;   // ECHudHECallForRevive
const unsigned int kHudEventAutoCallForRevive = 50;  // ECHudHEAutoCallForRevive
const unsigned int kHudEvent60               = 60;   // [UNRESOLVED name] - no registration names it

inline float floatFromBits(unsigned int bits)
{
    float value;
    ::memcpy(&value, &bits, sizeof(value));
    return value;
}


// Host adapters use typed virtual calls; string and object-layout accesses select the host ABI.

inline world::IPlayer* localHumanPlayer()
{
    return reinterpret_cast<world::PlayerManager*>(playerManager)->getLocalHumanPlayer();   // +0x30
}

inline unsigned int objectId(world::IObject* object)
{
    return world::object_detail::LayoutAccess::objectId(object);                           // +0x10
}

inline world::IObjectTemplate* templateOf(world::IObject* object)
{
    // IObject +0x14, read raw by every shipped body here. The template is a host object whose
    // non-const slots (queryImplementation) are called through it, hence the const_cast.
    return const_cast<world::IObjectTemplate*>(
        world::object_detail::LayoutAccess::objectTemplate(object));
}

inline world::IStrategicObjectComp* strategicComponent(world::IObject* object)
{
    // IObject +0x74 - the cached IID_IStrategicObjectComp interface pointer (Object::setComponent).
    return static_cast<world::IStrategicObjectComp*>(
        world::object_detail::LayoutAccess::strategicObjectComponent(object));
}


inline void* rootParent(void* object)
{
    return staticMeshRootParent(object);
}

// Geometry +0x44 - the bounding radius (StaticMesh.h / SkinnedMesh.h name the slot). The geometry
// objects are RendDX9's own, so the slot is this image's in both flavours; XpLightManager.cpp's
// geometrySlot precedent. 0x1012D9BE.
inline float geometryBoundingRadius(void* geometry)
{
    typedef float (__thiscall* RadiusFn)(void*);
    return reinterpret_cast<RadiusFn>((*reinterpret_cast<void***>(geometry))[0x44 / 4])(geometry);
}

// host strings
typedef std::wstring HostWString;
inline const std::string& hostStdString(const renddx9abi::HostString& value) { return value; }
inline const renddx9abi::HostString& fromHostStdString(const std::string& value) { return value; }

// std::operator==(const std::string&, const char*) (MSVCP71 import, 0x10130ECD) - compare() == 0.
inline bool hostStringEquals(const renddx9abi::HostString& value, const char* text)
{
    const unsigned int length = static_cast<unsigned int>(::strlen(text));
    return static_cast<unsigned int>(value.size()) == length
        && ::memcmp(value.c_str(), text, length) == 0;
}

// ISwiffHost +0xA8 - bool getLocalizedString(std::wstring& out, const std::string& key)
// (Menu/ISwiffHost.h:249). Reached by raw slot with the flavour's own string types, exactly as
// BF2Render.cpp reaches ISwiffHost +0x120: the typed interface spells the native std types, which
// are not the retail host's. 0x10130FA5; the result is discarded there.
inline bool swiffHostGetLocalizedString(IBase* swiffHost, HostWString& out,
                                        const renddx9abi::HostString& key)
{
    // RW-266: in this flavour the flavour's string types ARE the host's std types, so the typed
    // interface applies - ISwiffHost slot 42 is +0xA8 (/d1reportSingleClassLayout).
    return static_cast<ui::ISwiffHost*>(swiffHost)->getLocalizedString(out, key);
}

// Converts wide text using the current locale. The first size-error test intentionally
// follows the original off-by-one expression; conversion failures are reported by the second call.
renddx9abi::HostString narrowString(const HostWString& wide)
{
    const size_t length = ::wcstombs(0, wide.c_str(), 0) + 1;
    if (length == static_cast<size_t>(-1))
        return renddx9abi::HostString("no_conversion_exists");

    std::vector<char> buffer(length, '\0');                  // sub_10075C80(n, 0) - engine heap
    char* const destination = buffer.empty() ? 0 : &buffer[0];
    const size_t converted = ::wcstombs(destination, wide.c_str(), length);
    if (converted == static_cast<size_t>(-1))
        return renddx9abi::HostString("no_conversion_exists");
    if (converted == 0)
        return renddx9abi::HostString("");
    return renddx9abi::HostString(destination, static_cast<unsigned int>(converted));
}

// sub_10074AB0 - the image's printf-into-a-std::string helper, reproduced TU-locally as
// RaShader.cpp's raFormatString and QuadPatch.cpp's formatString already are (their copies have
// internal linkage). 0xFFF limit; an overflow is replaced by the fixed warning.
char g_nametagFormatBuffer[0x1000];

std::string nametagFormatString(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = ::_vsnprintf(g_nametagFormatBuffer, 0xFFF, format, args);
    va_end(args);

    if (written != -1)
        g_nametagFormatBuffer[written] = '\0';
    else
        ::sprintf(g_nametagFormatBuffer, "%s",
                  "WARNING: The original message was too long for formatString(). \n");
    return std::string(g_nametagFormatBuffer);
}

// lcgrand_step (0x1009EF30) - RendDX9's copy of dice::hfe::Random::getRaw (ELF 0x8166CE0): two
// Schrage steps, 24112 then 26143. The particle code carries the same arithmetic
// (ParticleInstanceType0Emission.inl's psmEmissionRandomStep); Core/Random.cpp is not in the
// RendDX9 manifest, so it is reproduced here rather than linked.
unsigned int randomStep(unsigned int value, unsigned int multiplier)
{
    const unsigned int low = (value & 0xFFFFu) * multiplier;
    const unsigned int high = static_cast<unsigned int>(static_cast<int>(value) >> 16) * multiplier;
    const int middle = static_cast<int>((low >> 16) + high);
    const unsigned int next = (low & 0xFFFFu) + ((static_cast<unsigned int>(middle) & 0x7FFFu) << 16)
        + static_cast<unsigned int>(middle >> 15);
    const int reduced = static_cast<int>(next - 0x7FFFFFFFu);
    return reduced < 0 ? next : static_cast<unsigned int>(reduced);
}

unsigned int randomRaw(Random& random)
{
    random.m_seed = randomStep(randomStep(random.m_seed, 24112u), 26143u);
    return random.m_seed;
}

// Native and plugin host layouts.

// Protected fields reached the way C++ allows from outside: a pointer-to-member formed inside a
// derived class. Nothing is instantiated.
struct ControlPointFieldAccess : world::ControlPoint
{
    static float flagPoleHeight(world::ControlPoint* cp)
    {
        return cp->*(&ControlPointFieldAccess::m_flagPoleHeight);
    }
};

struct SupplyTemplateFieldAccess : world::SupplyObjectTemplate
{
    static bool readVisibleOn3dMap(world::SupplyObjectTemplate* t)
    {
        return t->*(&SupplyTemplateFieldAccess::visibleOn3dMap);
    }
    static int readMapIcon3d(world::SupplyObjectTemplate* t)
    {
        return t->*(&SupplyTemplateFieldAccess::mapIcon3d);
    }
};

inline world::IObject* controlPointObject(world::ControlPoint* cp) { return cp; }
inline int   controlPointTeam(world::ControlPoint* cp) { return cp->getTeam(); }   // IControlPoint +0x1C
inline float controlPointFlagPoleHeight(world::ControlPoint* cp)
{
    return ControlPointFieldAccess::flagPoleHeight(cp);
}
inline bool supplyVisibleOn3dMap(void* supplyTemplate)
{
    return SupplyTemplateFieldAccess::readVisibleOn3dMap(
        static_cast<world::SupplyObjectTemplate*>(supplyTemplate));
}
inline int supplyMapIcon(void* supplyTemplate)
{
    return SupplyTemplateFieldAccess::readMapIcon3d(
        static_cast<world::SupplyObjectTemplate*>(supplyTemplate));
}
inline bool projectileVisibleOn3dMap(void* projectileTemplate)
{
    return static_cast<world::GenericProjectileTemplate*>(projectileTemplate)->m_byte1FA != 0;
}
inline int projectileMapIcon(void* projectileTemplate)
{
    return static_cast<world::GenericProjectileTemplate*>(projectileTemplate)->m_field1FC;
}
// The IProjectileObject interface is deliberately undeclared in this tree (IProjectileObject.h), so
// the native host's subobject table does NOT carry retail's +0x60/+0xC4. GenericProjectile is the
// interface's only implementor; its own table does, typed.
inline int projectileTeam(world::IObject* object, void*)
{
    return static_cast<world::GenericProjectile*>(object)->getTeam();
}
inline world::IObject* projectileTarget(world::IObject* object, void*)
{
    return static_cast<world::GenericProjectile*>(object)->getTargetObject();
}
inline bool vehicleHudUsesPlayerIcon(world::IObjectComponent* hudComponent)
{
    return static_cast<world::IVehicleHudComp*>(hudComponent)->getHudInfo()->usePlayerIcon;
}



// [CHOSEN, not recovered] Preserve the reissue defect's unchanged item state without
// reproducing its caller-stack corruption. Do not redirect the write onto the real item.

inline void reissueOrderDefect()
{
}

} // anonymous namespace

double nametagFrameDelta()
{
    // g_Renderer +0x5E8 - the delta Renderer::update stores (BF2Render.cpp: self + 1512).
    return dx9rend::rendererTailDouble(0x5E8);
}


// NametagItem and its three leaves


// 0x10131360, 212 B, retn 8. Every store is the shipped one, in declaration order; +0x04 and +0x84
// are alignment padding and are never written (their bytes are whatever operator new returned).
NametagItem::NametagItem(int type, int slot)
    : m_slot(slot)
    , m_type(type)
    , m_name()
    , m_text2C()
    , m_distance(0)
    , m_viewDistance(0.0)
    , m_rankIcon(-1)
    , m_colorIndex(-1)
    , m_icon(-1)
    , m_callIcon(-1)
    , m_flashColorIndex(-1)
    , m_timer(0.0f)
    , m_elevationIcon(-1)
    , m_iconRotation(0.0f)          // a zero dword either way
    , m_team(0)
    , m_lastSightCheckTime(0.0)
    , m_flags(7u)
    , m_arrowMode(false)
    , m_dirty(true)
    , m_inView(false)
    , m_inSight(false)
{
    m_offset.x = 0.0f;
    m_offset.y = 0.0f;
    m_offset.z = 0.0f;
    m_worldPosition.x = 0.0f;       // +0x90..+0x9C: four zero dwords, as shipped
    m_worldPosition.y = 0.0f;
    m_worldPosition.z = 0.0f;
    m_heightDelta = 0;
    m_fade.m_sightTime = 0.0;
    m_fade.m_value = 0.0;
    m_fade.m_time = 0.0;
}

// (body) / 0x1012C2D0 (scalar deleting): the two strings, +0x2C then +0x10.
NametagItem::~NametagItem()
{
}

// 0x1012D810, 75 B, retn 8.
PlayerNametagItem::PlayerNametagItem(world::IPlayer* player, int slot)
    : NametagItem(TTPlayer, slot)
    , m_player(player->getWeakPtr())          // IPlayer PE +0x18, constructed in place at +0xC0
{
    m_position.x = 0.0f;
    m_position.y = 0.0f;
    m_position.z = 0.0f;
}

// the weak pointer at +0xC0 (sub_101314A0), then the base.
PlayerNametagItem::~PlayerNametagItem()
{
}


int PlayerNametagItem::getKey()
{
    world::IPlayer* const player = m_player.get();
    return player ? player->getId() + kNametagKeyPlayer : -1;            // PE +0x50
}


// Updates and returns the soldier/vehicle-relative tag position; no player returns zero.
// Repeated vehicle reads may observe different vehicles. The x component's original
// x87 extended intermediate can differ by one ULP from SSE2.

const Vec3& PlayerNametagItem::updatePosition()
{
    world::IPlayer* const player = m_player.get();
    if (!player)
        return Vec3::getZero();

    Vec3 offset;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = 0.0f;

    world::IObject* const vehicle = player->getVehicle();               // PE +0x44
    if (vehicle)
    {
        world::ISoldier* const soldier =
            static_cast<world::ISoldier*>(vehicle->queryInterface(world::IID_ISoldier));
        if (soldier)
        {
            offset = soldier->getNameTagOffset();                        // ISoldier +0xA4
            switch (soldier->getPose())                                  // ISoldier +0x38
            {
            case world::SoldierPose_Crouch:                              // flt_1021D08C
                offset.x *= 0.6f; offset.y *= 0.6f; offset.z *= 0.6f;
                break;
            case world::SoldierPose_Prone:                               // flt_101DDE64
                offset.x *= 0.0f; offset.y *= 0.0f; offset.z *= 0.0f;
                break;
            case world::SoldierPose_Swim:                                // flt_1021D020 == 0x3F333333
                offset.x *= 0.7f; offset.y *= 0.7f; offset.z *= 0.7f;
                break;
            default:
                break;
            }
        }
        else
        {
            float radius;
            if (NametagManager::s_useVisualRadiusForVehicles)
            {
                radius = vehicle->getVisualBoundingRadius();             // PE +0x90
            }
            else
            {
                world::IObject* const root = static_cast<world::IObject*>(rootParent(vehicle));
                void* const geometry =
                    root ? world::object_detail::LayoutAccess::geometry(root) : 0;   // +0x38
                radius = geometry ? geometryBoundingRadius(geometry) : 0.0f;
            }
            offset.x = 0.0f;
            offset.z = 0.0f;
            offset.y = radius * NametagManager::s_vehicleNametagOffsetFactor
                     + NametagManager::s_vehicleNametagOffset;
        }
    }

    const Vec3* base;
    if (player->getVehicle())
        base = &player->getVehicle()->getAbsolutePosition();            // PE +0x68
    else
        base = &Vec3::getZero();

    const float partialY = offset.y + m_offset.y;
    const float partialZ = offset.z + m_offset.z;
    m_position.x = (offset.x + m_offset.x) + base->x;
    m_position.y = partialY + base->y;
    m_position.z = partialZ + base->z;
    return m_position;
}

// 0x1012DA50, 63 B, retn 8 - the out-of-line copy (no caller; 0x10130880 inlines the same): the
// base with type 2, the vtable, the weak pointer from IObject PE +0x18 (constructed in place at
// +0xC0), then +0xC4..+0xCC zeroed.
ObjectNametagItem::ObjectNametagItem(world::IObject* object, int slot)
    : NametagItem(TTObject, slot)
    , m_object(object->getWeakPtr())
{
    m_position.x = 0.0f;
    m_position.y = 0.0f;
    m_position.z = 0.0f;
}

// (the same folded body as the player item's).
ObjectNametagItem::~ObjectNametagItem()
{
}

// 0x1012DAA0, 29 B. 0 (not -1) when the object is gone - unlike the player item.
int ObjectNametagItem::getKey()
{
    world::IObject* const object = m_object.get();
    return object ? static_cast<int>(objectId(object)) + kNametagKeyObject : 0;
}

// 0x1012DAC0, 189 B. With NIF_UseHolderPosition the strategic holder's position is used and the
// tag is shown only while the object is part of a destroyed holder; otherwise the object's own
// position. The offset is added and the result stored at +0xC4.
const Vec3& ObjectNametagItem::updatePosition()
{
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;

    world::IObject* const object = m_object.get();
    if (object)
    {
        if (m_flags & NIF_UseHolderPosition)
        {
            world::IStrategicObjectComp* const component = strategicComponent(object);
            if (component)
            {
                position = component->getHolderPosition();              // +0x70
                if (component->isPartOfDestroyedHolder())              // +0x6C
                    m_flags |= NIF_Visible;
                else
                    m_flags &= ~NIF_Visible;
            }
        }
        else
        {
            position = object->getAbsolutePosition();                   // PE +0x68
        }
    }

    m_position.x = position.x + m_offset.x;
    m_position.y = position.y + m_offset.y;
    m_position.z = position.z + m_offset.z;
    return m_position;
}

// 0x1012DB80, 93 B, retn 10h.
OrderNametagItem::OrderNametagItem(const SquadOrder* order, int orderType, const Vec3& position,
                                   int slot)
    : NametagItem(TTOrder, slot)
    , m_order(*order)                          // SquadOrder's implicit copy
    , m_orderType(orderType)
    , m_targetPosition(position)
    , m_target()
{
}

// 0x1012DBE0, 89 B, retn 10h. `target` is not null-tested here (the only caller tests it).
OrderNametagItem::OrderNametagItem(const SquadOrder* order, int orderType, world::IObject* target,
                                   int slot)
    : NametagItem(TTOrder, slot)
    , m_order(*order)
    , m_orderType(orderType)
    , m_targetPosition()
    , m_target(target->getWeakPtr())           // IObject PE +0x18, in place at +0x100
{
}

// the weak pointer at +0x100, then the base.
OrderNametagItem::~OrderNametagItem()
{
}


int OrderNametagItem::getKey()
{
    return m_orderType + kNametagKeyOrder;
}


const Vec3& OrderNametagItem::updatePosition()
{
    if (m_flags & NIF_FollowTarget)
    {
        world::IObject* const target = m_target.get();
        if (target)
        {
            const Vec3& position = target->getAbsolutePosition();      // PE +0x68
            m_targetPosition.x = m_offset.x + position.x;
            m_targetPosition.y = m_offset.y + position.y;
            m_targetPosition.z = m_offset.z + position.z;
        }
    }
    return m_targetPosition;
}

// 0x1012DC40, 148 B, retn 8. Reached in retail only through the defect above (never on a real
// item), so never called here; transcribed because it is this TU's body.
void OrderNametagItem::setOrder(const SquadOrder* order, const Vec3& position)
{
    m_order = *order;
    m_targetPosition = position;
}

// 0x1012DCE0, 252 B, retn 8.
void OrderNametagItem::setOrder(const SquadOrder* order, world::IObject* target)
{
    m_order = *order;
    if (target)
    {
        m_target = target->getWeakPtr();
        return;
    }
    // the null-target arm retains the null temporary with `xor eax,eax; add [eax],1`
    // - a guaranteed write to address 0. Unreachable from every shipped caller (addTouchItem tests
    // the target first). Kept as a stop rather than written as UB or "fixed" into a clear.
    RD9_FRONTIER_AT("dice::hfe::geom::OrderNametagItem::setOrder (0x1012DDB2 - the shipped null-"
                    "target arm writes to address 0)", this, 0);
    std::abort();
}

// 0x101C8080, 83 B, retn 4.
bool NametagRayPredicator::predicate(world::IObject* object)
{
    if (rootParent(object) == m_ignoredRoot)
        return false;
    if (m_ignoreSoldiers && object)
    {
        const world::IObjectTemplate* const objectTemplate = templateOf(object);
        if (objectTemplate && objectTemplate->getClassID() == world::CID_SoldierTemplate)   // +0x10
            return false;
    }
    // A null object without the soldier test reaches this read, as shipped.
    const unsigned int flags = world::object_detail::LayoutAccess::flags(object);    // +0x04
    return (flags & m_flags) == m_flags;
}



// NametagManager - construction and destruction


// 0x1012F810, 849 B. Members in declaration order (the shipped store order: vptrs, refcount, time,
// the two vectors, the item map head, the two bytes, three set heads, three map heads, the texture
// state, the string, the rect, the double, the weak pointer, the target, the frustum, the demo
// byte), then the body.
NametagManager::NametagManager()
    : m_refCount(1)
    , m_time(0.0)
    , m_bufferingEnabled(false)
    , m_3dMapOn(true)
    , m_textureMode(0)
    , m_currentItemId(-1)
    , m_pendingBarTexture(-1)
    , m_barStart(0)
    , m_barEnd(0)
    , m_pendingBarPath()
    , m_pendingBarRect()
    , m_lastTargetCheckTime(0.0)
    , m_spectatedPlayer()
    , m_targetRoot(0)
    , m_frustum()
    , m_isDemoPlayback(false)
{
    // the IEventListener subobject (this + 4), categories 2 then 1,
    // priority 0. g_EventManager is dereferenced with no null test, as shipped.
    eventManager->registerEventHandler(ECHud, static_cast<IEventListener*>(this), 0);
    eventManager->registerEventHandler(ECGame, static_cast<IEventListener*>(this), 0);

    m_transformations.resize(3, 0);                                      // sub_10132390(3, 0)
    m_colors.resize(3, 0);                                               //   (vector::resize)

    // operator new(0xA924): a count prefix of 3 and three NameTagRenderer ctors (0x101B2F00). A
    // failed allocation leaves a NULL array and the next line dereferences it - shipped; the
    // nothrow form reproduces the null result the retail allocator can return.
    m_renderers = new (std::nothrow) NameTagRenderer[3];

    m_renderers[0].initialize(TTPlayer, 64, 4, s_cullDistances[TTPlayer]);
    m_transformations[TTPlayer] = &s_transformations0[0][0];
    m_colors[TTPlayer] = &s_colors0[0][0];
    // the five colours. The pointer is re-read from the vector for every
    // row in the shipped body; the same five rows land in s_colors0 either way.
    float* colors = m_colors[TTPlayer];
    colors[0]  = 1.0f;                        colors[1]  = 1.0f;
    colors[2]  = 1.0f;                        colors[3]  = 1.0f;       // 0: white
    colors[4]  = floatFromBits(0x3F74F4F5u);  colors[5]  = 0.0f;       // 1: enemy red (244/255)
    colors[6]  = 0.0f;                        colors[7]  = 1.0f;
    colors[8]  = 0.0f;                        colors[9]  = floatFromBits(0x3EECECEDu);   // 2: friendly
    colors[10] = floatFromBits(0x3F78F8F9u);  colors[11] = 1.0f;       //    blue (0, 118, 248)/255
    colors[12] = floatFromBits(0x3ECCCCCDu);  colors[13] = 1.0f;       // 3: squad green (0.4, 1, 0)
    colors[14] = 0.0f;                        colors[15] = 1.0f;
    colors[16] = 1.0f;                        colors[17] = 1.0f;       // 4: yellow
    colors[18] = 0.0f;                        colors[19] = 1.0f;

    m_renderers[1].initialize(TTOrder, 64, 1, s_cullDistances[TTOrder]);
    m_transformations[TTOrder] = &s_transformations1[0][0];
    m_colors[TTOrder] = &s_colors1[0][0];
    colors = m_colors[TTOrder];
    colors[0] = 1.0f; colors[1] = 1.0f; colors[2] = 1.0f; colors[3] = 1.0f;

    m_renderers[2].initialize(TTObject, 64, 5, s_cullDistances[TTObject]);
    m_transformations[TTObject] = &s_transformations2[0][0];
    m_colors[TTObject] = &s_colors0[0][0];     // shared with the player renderer's colours

    // 0x1012FB10..0x1012FB3E: `lea edi, [esi+38h]` is inside the loop, so all three passes clear
    // from the first alpha array (64, 2 and 64 dwords). Arrays 1 and 2 keep whatever operator new
    // returned. Shipped.
    for (int type = 0; type < 3; ++type)
        ::memset(m_alphaArrays[0], 0, static_cast<size_t>(maxSlotCount(type)) * sizeof(float));

    // RendDX9_registerDeviceLostResetListener(this + 8, 4) on the renderer base (dword_1023B970).
    dx9rend::registerDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this), 4);

    clearAll();                                // a direct call
}

// 0x1012F5A0, 435 B - NOT a vtable slot; release() calls it directly. The body, then the members
// in reverse order: the weak pointer, the bar path, the three slot maps (+0x388 first), the three
// free-slot sets, the item map, the colour and transformation vectors - which is the shipped order
// exactly.
NametagManager::~NametagManager()
{
    // Category 1 is unregistered twice and category 2 once (0x1012F5CE / 0x1012F5DC /
    // 0x1012F5EA). Shipped; the second removal finds nothing.
    eventManager->unregisterEventHandler(ECGame, static_cast<IEventListener*>(this));
    eventManager->unregisterEventHandler(ECHud, static_cast<IEventListener*>(this));
    eventManager->unregisterEventHandler(ECGame, static_cast<IEventListener*>(this));
    dx9rend::unregisterDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this));

    delete[] m_renderers;                      // 0x101B2FE0 x3 in reverse, then sub_1005F370

    removeAllItems();                          // a direct call
}


// IBase

void NametagManager::addRef()                  // the shared "++[this+0Ch]; return it"
{
    ++m_refCount;
}

int NametagManager::getRef()
{
    return m_refCount;
}

// The remaining count is cached before the destructor, as shipped.
int NametagManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~NametagManager();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// iid 1 (dword_102088DC) or 0xB801DCFE (dword_1021AE7C) answer the raw `this`.
void* NametagManager::queryInterface(unsigned int iid) const
{
    if (iid == 1u)
        return const_cast<NametagManager*>(this);
    return iid == IID_INametagManager ? const_cast<NametagManager*>(this) : 0;
}


// init / draw


// 0x1012DEA0, 31 B, retn 8: slot +0x6C (a virtual call), then 0x1012D610's result.
bool NametagManager::init(const renddx9abi::HostString& team1, const renddx9abi::HostString& team2)
{
    removeAllItems();
    return registerControlPointIcons(team1, team2);
}

// 0x1012D610, 437 B, retn 8. The five per-team control-point icons for this map: release and set
// each id's texture; true when the renderer's icon table grew by exactly five entries.
bool NametagManager::registerControlPointIcons(const renddx9abi::HostString& team1,
                                               const renddx9abi::HostString& team2)
{
    const unsigned int iconsBefore = NameTagRenderer::iconCount();       // dword_102670AC

    std::string path("Menu/Nametag/Neutral/miniMap_CP");
    NameTagRenderer::removeTexture(kIconControlPoint + 0, 0);             // (42, 0)
    NameTagRenderer::setTexture(kIconControlPoint + 0, 0, path, 0, 0, 16, 16);

    path = nametagFormatString("Menu/Nametag/%s/miniMap_CP", team1.c_str());
    NameTagRenderer::removeTexture(kIconControlPoint + 1, 0);             // 43
    NameTagRenderer::setTexture(kIconControlPoint + 1, 0, path, 0, 0, 16, 16);

    path = nametagFormatString("Menu/Nametag/%s/miniMap_CPBase", team1.c_str());
    NameTagRenderer::removeTexture(kIconControlPointBase + 1, 0);         // 46
    NameTagRenderer::setTexture(kIconControlPointBase + 1, 0, path, 0, 0, 16, 16);

    path = nametagFormatString("Menu/Nametag/%s/miniMap_CP", team2.c_str());
    NameTagRenderer::removeTexture(kIconControlPoint + 2, 0);             // 44
    NameTagRenderer::setTexture(kIconControlPoint + 2, 0, path, 0, 0, 16, 16);

    path = nametagFormatString("Menu/Nametag/%s/miniMap_CPBase", team2.c_str());
    NameTagRenderer::removeTexture(kIconControlPointBase + 2, 0);         // 47
    NameTagRenderer::setTexture(kIconControlPointBase + 2, 0, path, 0, 0, 16, 16);

    return NameTagRenderer::iconCount() == iconsBefore + 5;
}


void NametagManager::draw()
{
    // the profiler bracket ("Nametags") - not reproduced, see
    // the file header.
    update();                                  // slot +0x14 - a virtual call
    for (int type = TTObject; type >= TTPlayer; --type)   // 0x70C0, 0x3860, 0 - renderers 2, 1, 0
        m_renderers[type].draw();
}

// Adds a player nametag.
bool NametagManager::addTouchItem(world::IPlayer* player)
{
    NametagItem* item;
    const NametagItemMap::iterator found = m_items.find(player->getId() + kNametagKeyPlayer);   // inline key
    if (found == m_items.end())
    {
        item = createPlayerItem(player);
        if (!item)
            return false;
    }
    else
    {
        item = found->second;
        item->m_dirty = true;
    }

    localHumanPlayer();                        // asked and discarded, as shipped
    item->m_flags = (item->m_flags & ~0x28u) | 0x401u;
    item->m_colorIndex = 2;
    item->m_team = player->getTeam();                                    // PE +0xE4
    item->m_flags &= ~NIF_Bit4;
    item->m_viewDistance = s_cullDistances[TTPlayer].y;                  // flt_10239A84
    resetItem(item);
    refreshPlayerItem(static_cast<PlayerNametagItem*>(item));            // result unused
    return true;
}

// 0x10130A90, 243 B, retn 4.
bool NametagManager::addTouchItem(world::ControlPoint* controlPoint)
{
    world::IObject* const object = controlPointObject(controlPoint);
    NametagItem* item;
    const NametagItemMap::iterator found = m_items.find(static_cast<int>(objectId(object)) + kNametagKeyObject);
    if (found == m_items.end())
    {
        item = createObjectItem(object);
        if (!item)
            return false;
    }
    else
    {
        item = found->second;
        item->m_dirty = true;
    }

    // An uncapturable ("main base") control point uses the CPBase icons.
    int base = kIconControlPoint;
    world::IControlPointTemplate* const cpTemplate = static_cast<world::IControlPointTemplate*>(
        templateOf(object)->queryInterface(world::IID_IControlPointTemplate));
    if (cpTemplate && cpTemplate->getUnableToChangeTeam())               // +0x64
        base = kIconControlPointBase;

    item->m_icon = controlPointTeam(controlPoint) + base;                // IControlPoint +0x1C
    item->m_team = controlPointTeam(controlPoint);                       // asked twice, as shipped
    item->m_colorIndex = 0;
    item->m_offset.x = 0.0f;
    item->m_offset.z = 0.0f;
    item->m_offset.y = controlPointFlagPoleHeight(controlPoint) + 1.0f;  // flt_101DDDE4
    item->m_flags = (item->m_flags & ~0x420u) | 0x4001Du;
    item->m_viewDistance = s_cullDistances[TTObject].y;                  // flt_10239A94
    return true;
}


// 0x10130B90, 951 B, retn 4 - a soldier, a strategic object, a supply object, a projectile, a
// vehicle drop, or anything else. Every exit, store and repeated call is the shipped one.

bool NametagManager::addTouchItem(world::IObject* object)
{
    NametagItem* item;
    const NametagItemMap::iterator found = m_items.find(static_cast<int>(objectId(object)) + kNametagKeyObject);
    if (found != m_items.end())
    {
        item = found->second;
        item->m_dirty = true;
    }
    else
    {
        item = createObjectItem(object);
        if (!item)
            return false;
    }

    item->m_flags = (item->m_flags & ~0x424u) | 0x18u;
    item->m_viewDistance = s_cullDistances[TTObject].y;
    item->m_colorIndex = 0;

    // a soldier: coloured by its player's relation
    world::ISoldier* const soldier = static_cast<world::ISoldier*>(object->queryInterface(world::IID_ISoldier));
    if (soldier)
    {
        world::IPlayer* const player = soldier->getOccupingPlayer();     // +0x34
        if (!player)
            return true;
        if (player->getSquadId() != -1)                                  // PE +0x108
            item->m_colorIndex = 3;
        else
            item->m_colorIndex = (player == localHumanPlayer()) + 1;
        item->m_team = player->getTeam();
        return true;
    }

    // a strategic object
    world::IStrategicObjectComp* const component = strategicComponent(object);
    if (component)
    {
        item->m_icon = component->get3dMapIcon() + ICObject;            // +0x64
        item->m_offset.y = object->getVisualBoundingRadius();            // PE +0x90
        item->m_offset.x = 0.0f;
        item->m_offset.z = 0.0f;
        if (static_cast<world::StrategicObjectComp*>(component)->m_holder)  // +0x14 (RW-266)
            item->m_flags |= NIF_UseHolderPosition;                      // byte +0xB9 |= 1
        item->m_flags &= ~NIF_Visible;
        item->m_colorIndex = 4;
        return true;
    }

    world::IObjectTemplate* const objectTemplate = templateOf(object);

    // a supply object
    void* const supply = objectTemplate->queryImplementation(world::CID_SupplyObjectTemplate);   // PE +0x4C
    if (supply)
    {
        if (!supplyVisibleOn3dMap(supply))
        {
            removeItem(TTObject, object);      // slot +0x50, a virtual call (0x10130EB6)
            return false;
        }
        item->m_icon = supplyMapIcon(supply) + ICObject;
        item->m_offset.x = 0.0f;
        item->m_offset.z = 0.0f;
        item->m_offset.y = floatFromBits(0x3F99999Au);                   // 1.2f
        item->m_viewDistance = s_supplyObjectDistance;
        item->m_colorIndex = 0;
        item->m_flags = (item->m_flags & ~0x404u) | 0x4039u;
        return true;
    }

    // not a projectile: a vehicle drop, or a plain object
    void* const projectile = object->queryInterface(world::IID_IProjectileObject);
    if (!projectile)
    {
        const renddx9abi::HostString& name = fromHostStdString(objectTemplate->getName());   // PE +0x28
        if (hostStringEquals(name, "dropVehicle"))
        {
            item->m_icon = ICObjectVehicleDrop;
            item->m_offset.x = 0.0f;
            item->m_offset.z = 0.0f;
            item->m_offset.y = floatFromBits(0x3F99999Au);
            item->m_viewDistance = s_supplyObjectDistance;
            item->m_flags = (item->m_flags & ~0x404u) | 0x4039u;
            item->m_colorIndex = 0;
            return true;
        }
        item->m_icon = ICObject;
        item->m_offset.y = object->getVisualBoundingRadius();
        item->m_offset.x = 0.0f;
        item->m_offset.z = 0.0f;
        item->m_flags |= NIF_Visible;
        return true;
    }

    // a projectile
    void* const projectileTemplate =
        objectTemplate->queryImplementation(world::CID_GenericProjectileTemplate);
    if (!projectileTemplate)
        return true;
    if (!projectileVisibleOn3dMap(projectileTemplate))
    {
        removeItem(TTObject, object);
        return false;
    }

    item->m_team = projectileTeam(object, projectile);                    // IProjectileObject +0x60
    const int icon = projectileMapIcon(projectileTemplate) + ICObject;
    float radius;
    if (icon >= 0x54 && icon <= 0x58)
    {
        item->m_flags &= ~NIF_Bit10;                                      // byte +0xB9 &= ~4
        item->m_icon = icon;
        item->m_colorIndex = 0;
        radius = object->getVisualBoundingRadius();
    }
    else if (static_cast<world::IObjectTemplate*>(projectileTemplate)->getComponentFromClassID(
                 world::CID_SeekClosestTargetCompTemplate))                       // PE +0x5C
    {
        // A seeking projectile shows only while it is locked on the local player's vehicle.
        item->m_flags &= ~NIF_Visible;
        if (!projectileTarget(object, projectile))                        // +0xC4
            return true;
        world::IPlayer* const local = localHumanPlayer();
        if (!local)
            return true;
        if (!local->getVehicle())
            return true;
        void* const localRoot = rootParent(local->getVehicle());          // asked again
        if (rootParent(projectileTarget(object, projectile)) != localRoot)   // and again
            return true;
        item->m_flags = (item->m_flags & ~0x404u) | 0x4039u;
        item->m_icon = icon;
        item->m_colorIndex = 1;
        item->m_flashColorIndex = 4;
        item->m_timer = 100.0f;
        return true;
    }
    else
    {
        localHumanPlayer();                    // asked and discarded, as shipped
        item->m_flags |= NIF_Bit10;                                       // byte +0xB9 |= 4
        item->m_icon = icon;
        item->m_colorIndex = 0;
        radius = object->getVisualBoundingRadius();
    }
    item->m_offset.x = 0.0f;
    item->m_offset.y = radius;
    item->m_offset.z = 0.0f;
    item->m_flags |= NIF_Visible;
    item->m_viewDistance = s_deployableDistance;                          // dbl_10239A30
    return true;
}

// 0x101310A0, 204 B, retn 0Ch.
bool NametagManager::addTouchItem(SquadOrder* order, int orderType, const Vec3& position)
{
    const NametagItemMap::iterator found = m_items.find(orderType + kNametagKeyOrder);
    NametagItem* item = 0;
    if (found != m_items.end())
    {
        item = found->second;
        item->m_dirty = true;
    }
    if (!item)
        item = createOrderItem(order, orderType, position);
    else
        reissueOrderDefect();                  // see the note above
    if (!item)
        return false;

    item->m_rankIcon = -1;
    item->m_colorIndex = 0;
    item->m_timer = s_callIconTime;
    item->m_flashColorIndex = 4;
    item->m_callIcon = -1;
    item->m_icon = orderIcon(orderType, order->orderId);
    item->m_viewDistance = s_cullDistances[TTOrder].y;                   // flt_10239A8C
    item->m_flags |= 0x40B9u;
    return true;
}

// 0x10131170, 226 B, retn 0Ch.
bool NametagManager::addTouchItem(SquadOrder* order, int orderType, world::IObject* target)
{
    if (!target)
        return false;

    const NametagItemMap::iterator found = m_items.find(orderType + kNametagKeyOrder);
    NametagItem* item = 0;
    if (found != m_items.end())
    {
        item = found->second;
        item->m_dirty = true;
    }
    if (!item)
        item = createOrderItem(order, orderType, target);
    else
        reissueOrderDefect();                  // see the note above
    if (!item)
        return false;

    item->m_rankIcon = -1;
    item->m_colorIndex = 0;
    item->m_timer = s_callIconTime;
    item->m_flashColorIndex = 4;
    item->m_callIcon = -1;
    item->m_icon = orderIcon(orderType, order->orderId);
    item->m_viewDistance = s_cullDistances[TTOrder].y;
    item->m_flags = (item->m_flags & ~NIF_ShowText) | 0x4031u;
    return true;
}


// 0x10130F50, 327 B, retn 8 - the UAV: rename it to the localised "KILLMESSAGE_VEHICLE_uav" text,
// then tag it with the owning team's relation colour.
// The "SwiffHost" singleton is not null-tested; the localisation result is discarded.

bool NametagManager::addTouchItem(world::IObject* object, int team)
{
    IBase* const swiffHost = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    HostWString localized;
    const renddx9abi::HostString key("KILLMESSAGE_VEHICLE_uav");
    swiffHostGetLocalizedString(swiffHost, localized, key);             // ISwiffHost +0xA8
    const renddx9abi::HostString name = narrowString(localized);
    object->setName(hostStdString(name));                                // IObject PE +0x64

    NametagItem* item;
    const NametagItemMap::iterator found = m_items.find(static_cast<int>(objectId(object)) + kNametagKeyObject);
    if (found == m_items.end())
    {
        item = createObjectItem(object);
        if (!item)
            return false;
    }
    else
    {
        item = found->second;
        item->m_dirty = true;
    }

    item->m_flags |= NIF_Visible | NIF_Bit2;                             // |= 5
    world::IPlayer* const local = localHumanPlayer();
    if (local)
        item->m_colorIndex = (team == local->getTeam()) + 1;             // PE +0xE4
    else
        item->m_colorIndex = team;
    item->m_team = team;
    item->m_viewDistance = s_cullDistances[TTObject].y;
    return true;
}


// the per-type tables

// Unchecked indexing, as shipped (operator[], not the throwing at()).
float* NametagManager::getColors(int type)
{
    return m_colors[type];
}

float* NametagManager::getTransformations(int type)
{
    return m_transformations[type];
}

float* NametagManager::getAlphaArray(int type)        // this + 0x38 + type * 0x100
{
    return m_alphaArrays[type];
}


// buffering / clearing


// 0x1012ED40, 117 B: every item re-buffered into its renderer (0x101B2320, full = true).
void NametagManager::rebufferAll()
{
    for (NametagItemMap::iterator it = m_items.begin(); it != m_items.end(); ++it)
        m_renderers[it->second->m_type].bufferItem(it->second, true);
}

// 0x1012EDC0, 106 B - the renderer's ECCore CEPostLoad arm calls it: mark every item dirty and
// let update() run.
void NametagManager::enableBuffering()
{
    for (NametagItemMap::iterator it = m_items.begin(); it != m_items.end(); ++it)
        it->second->m_dirty = true;
    m_bufferingEnabled = true;
}

// 0x1012F420, 372 B: every slot free again, every key forgotten. The counts are 0x1012C840's,
// inlined (64, 2, 64).
void NametagManager::clearAll()
{
    for (int type = TTPlayer; type <= TTObject; ++type)
    {
        for (int slot = 0; slot < maxSlotCount(type); ++slot)
            m_freeSlots[type].insert(slot);
        m_slotMaps[type].clear();
    }
}

// 0x1012EF10, 119 B (slot +0x6C). Park each item's transformation off-screen ({0, 0, -1, 0}),
// delete it (slot +0x08 with 1) and erase it, until the map is empty; then clearAll (slot +0x48).
// The item's type and slot are read before the item is null-tested.
void NametagManager::removeAllItems()
{
    for (NametagItemMap::iterator it = m_items.begin(); it != m_items.end(); it = m_items.begin())
    {
        NametagItem* const item = it->second;
        float* const transformation = getTransformations(item->m_type) + item->m_slot * 4;   // virtual
        transformation[0] = 0.0f;
        transformation[1] = 0.0f;
        transformation[2] = -1.0f;
        transformation[3] = 0.0f;
        if (item)
            delete item;
        m_items.erase(it);
    }
    clearAll();                                // slot +0x48 - a virtual call
}

// 0x10130190, 119 B, retn 0Ch (slot +0x4C).
void NametagManager::removeItem(int type, void* item, int orderType)
{
    switch (type)
    {
    case TTPlayer:
        removeKey(type, static_cast<world::IPlayer*>(item)->getId() + kNametagKeyPlayer);
        return;
    case TTOrder:
        removeKey(type, orderType + kNametagKeyOrder);
        return;
    case TTObject:
        removeKey(type, static_cast<int>(objectId(static_cast<world::IObject*>(item))) + kNametagKeyObject);
        return;
    default:
        removeKey(type, 0);
        return;
    }
}

// 0x10130210, 116 B, retn 8 (slot +0x50). The order arm has no order type to go on and removes
// key the squad order - whatever was meant.
void NametagManager::removeItem(int type, void* item)
{
    switch (type)
    {
    case TTPlayer:
        removeKey(type, static_cast<world::IPlayer*>(item)->getId() + kNametagKeyPlayer);
        return;
    case TTOrder:
        removeKey(type, kNametagKeyOrder);
        return;
    case TTObject:
        removeKey(type, static_cast<int>(objectId(static_cast<world::IObject*>(item))) + kNametagKeyObject);
        return;
    default:
        removeKey(type, 0);
        return;
    }
}

// Frees the key's slot and removes its item. The type argument selects the transformation table.
void NametagManager::removeKey(int type, int key)
{
    const NametagItemMap::iterator found = m_items.find(key);
    if (found == m_items.end())
        return;

    NametagSlotMap& slots = m_slotMaps[type];
    if (slots.count(key) != 0)                                           // equal_range + _Distance
    {
        slots.erase(key);
        m_freeSlots[type].insert(found->second->m_slot);
    }

    NametagItem*& entry = m_items[key];
    float* const transformation = getTransformations(type) + entry->m_slot * 4;   // virtual +0x38
    transformation[0] = 0.0f;
    transformation[1] = 0.0f;
    transformation[2] = -1.0f;
    transformation[3] = 0.0f;
    NametagItem* const item = m_items[key];                              // operator[] again
    if (item)
        delete item;                                                     // slot +0x08(1)
    m_items.erase(key);
}


// small accessors

const void* NametagManager::getItems() const          // this + 0x338
{
    return &m_items;
}

bool NametagManager::get3dMapOn() const
{
    return m_3dMapOn;
}

void NametagManager::set3dMapOn(bool on)
{
    m_3dMapOn = on;
}

bool NametagManager::isDemoPlayback() const
{
    return m_isDemoPlayback;
}

world::IPlayer* NametagManager::getSpectatedPlayer() const
{
    return m_spectatedPlayer.get();
}

// 0x10132660, 134 B + 0x101326F0 (WeakPtr::operator=, 51 B). Either the player's weak pointer
// (IPlayer PE +0x18) or a null one is assigned; the temporary is destroyed after.
void NametagManager::setSpectatedPlayer(world::IPlayer* player)
{
    m_spectatedPlayer = player ? player->getWeakPtr() : WeakPtr<world::IPlayer>();
}


// the event handler - IEventListener, `this` at +0x04 in the shipped body


// 0x1012EFD0, 1,020 B, retn 10h.
void NametagManager::handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data)
{
    (void)node;
    // looked up for every event, before the category is known.
    IGame* const gameSingleton = static_cast<IGame*>(
        classManager->getSingleton(renddx9abi::HostString("Game")));     // g_hfeClassManager +0x48

    if (category == ECGame)
    {
        if (id == kGameEventPlayerSpawned)
        {
            world::IPlayer* const spawned = static_cast<world::IPlayer*>(data);
            if (spawned == localHumanPlayer())
            {
                resetAllCalls();
                return;
            }
            // The spawned player is dereferenced with no null test; the key is built inline.
            const NametagItemMap::iterator found = m_items.find(spawned->getId() + kNametagKeyPlayer);
            if (found == m_items.end())
                return;
            resetItem(found->second);
            return;
        }
        // any other ECGame id jumps into the ECHud id tests below. Shipped.
    }
    else if (category != ECHud)
    {
        return;
    }

    if (id < kHudEventCallForMedic)
        return;

    if (id <= kHudEventAutoCallForRevive)
    {
        if (!data || !gameSingleton)
            return;
        world::IObject* const object =
            gameSingleton->getObjectFromNetworkableId(*static_cast<const unsigned short*>(data));   // IGame PE +0x78
        if (!object)
            return;
        world::ISoldier* const soldier =
            static_cast<world::ISoldier*>(object->queryInterface(world::IID_ISoldier));
        if (!soldier)
            return;
        world::IPlayer* const caller = soldier->getOccupingPlayer();     // +0x34
        if (!caller)
            return;
        const NametagItemMap::iterator found = m_items.find(playerKey(caller));
        if (found == m_items.end())
            return;
        NametagItem* const item = found->second;
        world::IPlayer* const local = localHumanPlayer();
        if (!local)
            return;
        // fild [item+54h] against s_maxCallDistance - an exact integer/float comparison.
        if (!(static_cast<double>(item->m_distance) < static_cast<double>(s_maxCallDistance)))
            return;
        const int kit = playerKitType(local);

        switch (id)
        {
        case kHudEventCallForMedic:
            if (kit == kKitMedic)
            {
                if (item->m_timer <= 0.0f)
                    item->m_timer = s_callIconTime;
                item->m_flags |= 0x26808u;
                item->m_callIcon = ICMedic;
                item->m_flashColorIndex = 4;
                item->m_dirty = true;
            }
            if (caller == local)
                s_medicCallTimer = s_callIconTime;
            return;

        case kHudEventCallForAmmo:
            if (kit == kKitSupport)
            {
                if (item->m_timer <= 0.0f)
                    item->m_timer = s_callIconTime;
                item->m_flags |= 0x26808u;
                item->m_callIcon = ICAmmo;
                item->m_flashColorIndex = 4;
                item->m_dirty = true;
            }
            if (caller == local)
                s_ammoCallTimer = s_callIconTime;
            return;

        case kHudEventCallForRepair:
            if (kit == kKitEngineer)
            {
                if (item->m_timer <= 0.0f)
                    item->m_timer = s_callIconTime;
                item->m_flags |= 0x26808u;
                item->m_callIcon = ICRepair;
                item->m_flashColorIndex = 4;
                item->m_dirty = true;
            }
            if (caller == local)
                s_repairCallTimer = s_callIconTime;
            return;

        case kHudEventCallForRevive:
        case kHudEventAutoCallForRevive:
            if (kit == kKitMedic)
            {
                item->m_flags |= 0x26808u;
                item->m_timer = 30.0f;                                   // unconditional here
                item->m_callIcon = ICRevive;
                item->m_flashColorIndex = 4;
                item->m_dirty = true;
            }
            if (caller == local)
                s_medicCallTimer = s_callIconTime;   // the revive call reuses the medic timer
            return;

        default:
            // unreachable (the switch covers 46..50); dice's own diagnostic, logged
            // the way this image logs its asserts (ObjectDrawer.cpp:598).
            ::renddx9abi::logf("NametagManager.cpp:2422 unknown");
            return;
        }
    }

    if (id == kHudEvent60)
    {
        // Neither `data` nor the game is tested on this path (unlike 46..50), and the soldier
        // interface is used without a null test (0x1012F05F..0x1012F066). Shipped.
        const unsigned short networkId = *static_cast<const unsigned short*>(data);
        world::IObject* const object = gameSingleton->getObjectFromNetworkableId(networkId);
        if (!object)
            return;
        world::ISoldier* const soldier =
            static_cast<world::ISoldier*>(object->queryInterface(world::IID_ISoldier));
        world::IPlayer* const player = soldier->getOccupingPlayer();
        if (!player)
            return;
        const NametagItemMap::iterator found = m_items.find(player->getId() + kNametagKeyPlayer);
        if (found == m_items.end())
            return;
        NametagItem* const item = found->second;
        if (item->m_timer <= 0.0f)
            item->m_timer = s_callIconTime;
        item->m_flashColorIndex = (item->m_colorIndex != 4) ? 4 : 0;
        item->m_dirty = true;
    }
}

// 0x1012EE30, 217 B - the local player spawned: forget its own calls and every tag's call state.
void NametagManager::resetAllCalls()
{
    s_repairCallTimer = 0.0;                   // dbl_1025C228 first, as shipped
    s_medicCallTimer = 0.0;
    s_ammoCallTimer = 0.0;
    for (NametagItemMap::iterator it = m_items.begin(); it != m_items.end(); ++it)
        resetItem(it->second);                 // 0x1012CA10, inlined in the shipped loop
}

// 0x1012CA10, 74 B, retn 4.
void NametagManager::resetItem(NametagItem* item)
{
    if (!item)
        return;
    item->m_flags &= 0xFFFF87D7u;
    item->m_callIcon = -1;
    item->m_flashColorIndex = -1;
    item->m_timer = 0.0f;
    if (item->m_type == TTOrder)
        item->m_flags |= 0x40B9u;
    item->m_dirty = true;
}

// 0x1012C310, 20 B, retn 4.
int NametagManager::playerKey(world::IPlayer* player)
{
    return player->getId() + kNametagKeyPlayer;
}

// 0x1012C3D0, 56 B, retn 4.
int NametagManager::playerKitType(world::IPlayer* player)
{
    world::IObject* const defaultVehicle = player->getDefaultVehicle();   // PE +0xA0
    if (defaultVehicle)
    {
        world::ISoldier* const soldier =
            static_cast<world::ISoldier*>(defaultVehicle->queryInterface(world::IID_ISoldier));
        if (soldier)
            return soldier->getKitType();                                // +0x58
    }
    return kKitNone;
}

// 0x1012DE50, 77 B, retn 4.
int NametagManager::playerRankIcon(PlayerNametagItem* item)
{
    world::IPlayer* const player = item->m_player.get();
    world::IPlayer* const local = localHumanPlayer();
    if (local && local != player)
        return player->getScoreData()->m_rank + 1;                       // PE +0x170, +0x20
    return -1;
}

// 0x1012C410, 55 B, retn 4 - unreferenced (see the header); the item form above is the live one.
int NametagManager::playerRankIconOf(world::IPlayer* player)
{
    world::IPlayer* const local = localHumanPlayer();                    // [0x10244AE0] +0x30
    if (local && local != player)
        return player->getScoreData()->m_rank + 1;                       // PE +0x170, +0x20
    return -1;
}

// 0x1012C330, 21 B. The armor component is read once per level.
world::IObjectComponent* NametagManager::findArmorComponent(world::IObject* object)
{
    for (; object; object = world::object_detail::LayoutAccess::parent(object))       // +0x28
    {
        world::IObjectComponent* const armor =
            world::object_detail::LayoutAccess::armorComponent(object);                  // +0x3C
        if (armor)
            return armor;
    }
    return 0;
}

// 0x1012C450, 189 B - unreferenced outside itself.
// A child that sets a new maximum is evaluated twice (0x1012C484 compares, 0x1012C497 stores):
//   the whole sub-hierarchy is walked again, as shipped.
// The object's own radius is asked twice as well (0x1012C4B5 compares, 0x1012C4D7 returns).
// The x87 tests decide NaN: a NaN child result replaces the maximum (`test ah,5; jnp` skips only a
// clean "less"), a NaN own radius never wins (`test ah,41h; jne`). The comparisons run on x87
// extended values in retail (the callee's ST0); float precision here.
float NametagManager::hierarchyVisualRadius(world::IObject* object, bool childrenOnly)
{
    if (!object)
        return 0.0f;                                                     // flt_101DDE64

    float largest = 0.0f;
    for (world::IObject* child = object->getChild();                     // PE +0xEC
         child;
         child = world::object_detail::LayoutAccess::sibling(child))     // +0x2C
    {
        if (!(hierarchyVisualRadius(child, false) < largest))
            largest = hierarchyVisualRadius(child, false);
    }
    if (childrenOnly)
        return largest;

    const float own = world::object_detail::LayoutAccess::geometry(object)   // +0x38
                    ? object->getVisualBoundingRadius()                  // PE +0x90
                    : 0.0f;
    if (own > largest)
        return world::object_detail::LayoutAccess::geometry(object)
             ? object->getVisualBoundingRadius()
             : 0.0f;
    return largest;
}


// 0x1012DEC0, 1,060 B, retn 4. A player tag's icon (call-for-help / squad leader / commander /
// soldier / vehicle kind), its relation colour and whether it is drawn. Returns whether the
// colour changed.

bool NametagManager::refreshPlayerItem(PlayerNametagItem* item)
{
    world::IPlayer* const player = item->m_player.get();
    world::IPlayer* const local = localHumanPlayer();
    bool changed = false;
    if (!player)
        return false;

    item->m_rankIcon = playerRankIcon(item);

    // 0x1012DF19..0x1012DF44 - 0x1012C3D0 inlined for the item's player.
    int kit = kKitNone;
    world::IObject* const defaultVehicle = player->getDefaultVehicle();
    if (defaultVehicle)
    {
        world::ISoldier* const soldier =
            static_cast<world::ISoldier*>(defaultVehicle->queryInterface(world::IID_ISoldier));
        if (soldier)
            kit = soldier->getKitType();
    }

    item->m_flags &= 0xFFFFA7DFu;

    // the icon
    // While the local player's own call is pending, the specialists who can answer it show the call.
    int callIcon = -1;
    if (s_medicCallTimer > 0.0 && kit == kKitMedic)
        callIcon = ICMedic;
    else if (s_repairCallTimer > 0.0 && kit == kKitEngineer)
        callIcon = ICRepair;
    else if (s_ammoCallTimer > 0.0 && kit == kKitSupport)
        callIcon = ICAmmo;

    if (callIcon != -1)
    {
        const unsigned int flags = item->m_flags;          // captured before the icon test, as shipped
        if (item->m_icon != callIcon)
        {
            item->m_icon = callIcon;
            item->m_flashColorIndex = -1;
            item->m_dirty = true;
        }
        item->m_flags = (flags & ~NIF_KindIcon) | 0x25820u;
    }
    else if (player->getIsSquadLeader())                                 // PE +0x138
    {
        item->m_icon = player->getSquadId() + kIconSquadLeaderBase;      // PE +0x108
        item->m_flags = (item->m_flags & ~NIF_KindIcon) | NIF_Bit14;
    }
    else if (player->getIsCommander())                                   // PE +0x110
    {
        item->m_flags &= ~NIF_KindIcon;
        item->m_icon = kIconSquadLeaderBase;
    }
    else
    {
        if (item->m_timer <= 0.0f)
            item->m_flags &= ~NIF_CallActive;                            // byte +0xBA &= ~2

        world::IObject* const vehicle = player->getVehicle();           // PE +0x44
        if (vehicle)
        {
            if (vehicle->queryInterface(world::IID_ISoldier))
            {
                if (item->m_icon != kIconSoldier)
                {
                    item->m_flags |= NIF_KindIcon;
                    item->m_icon = kIconSoldier;
                }
            }
            else
            {
                bool usePlayerIcon = false;
                world::IObjectComponent* const hud =
                    world::object_detail::LayoutAccess::vehicleHudComponent(vehicle);   // +0x54
                if (hud)
                    usePlayerIcon = vehicleHudUsesPlayerIcon(hud);       // getHudInfo() +0x64, +0x94

                int icon = kIconSoldier;
                // Neither the root nor its template is null-tested.
                world::IObject* const root = static_cast<world::IObject*>(rootParent(vehicle));
                world::IPlayerControlObjectTemplate* const pcoTemplate =
                    static_cast<world::IPlayerControlObjectTemplate*>(
                        templateOf(root)->queryInterface(world::IID_IPlayerControlObjectTemplate));
                if (pcoTemplate)
                {
                    // byte_1012E318 + jpt_1012E0D6, all 26 vehicle types.
                    switch (pcoTemplate->getVehicleType())               // +0x34
                    {
                    case 0: case 1:                 icon = 0x5F; break;
                    case 2:                         icon = 0x51; break;
                    case 3: case 15: case 17: case 18: icon = 0x5D; break;
                    case 4:                         icon = 0x5C; break;
                    case 5: case 6: case 7: case 16: icon = 0x61; break;
                    case 8:                         icon = 0x64; break;
                    case 9:                         icon = 0x65; break;
                    case 10: case 11: case 12: case 13: case 21: icon = 0x60; break;
                    case 14: case 24: case 25:      icon = kIconSoldier; break;
                    case 19:                        icon = 0x66; break;
                    case 20:                        icon = 0x5E; break;
                    case 22:                        icon = 0x62; break;
                    case 23:                        icon = 0x63; break;
                    default:
                        // jpt_1012E13C - by category, unless the vehicle's HUD uses the player icon.
                        if (!usePlayerIcon)
                        {
                            switch (pcoTemplate->getVehicleCategory())   // +0x24
                            {
                            case 0: icon = 0x5B; break;
                            case 1: icon = 0x60; break;
                            case 2: icon = 0x61; break;
                            case 3: icon = 0x62; break;
                            default: break;
                            }
                        }
                        break;
                    }
                }
                if (item->m_icon != icon)
                {
                    item->m_flags |= NIF_KindIcon;
                    item->m_icon = icon;
                }
            }
        }
    }

    // the relation colour (0x1012E168)
    int relation;
    bool haveRelation = false;
    if (local)
    {
        const int localTeam = local->getTeam();                          // evaluated first
        const int playerTeam = player->getTeam();
        if (localTeam == playerTeam)
        {
            if (local->getIsCommander() && player->getIsSquadLeader())
            {
                relation = 3;
                haveRelation = true;
            }
            else if (player->getIsCommander())
            {
                if (item->m_colorIndex != 2)
                {
                    item->m_colorIndex = 2;
                    changed = true;
                }
                item->m_flags &= ~NIF_SquadMember;
                goto visibility;
            }
            else
            {
                bool sameSquad = false;
                if (player->getSquadId() != 0)
                {
                    const int localSquad = local->getSquadId();
                    const int playerSquad = player->getSquadId();       // asked again
                    sameSquad = (playerSquad == localSquad);
                }
                if (sameSquad)
                {
                    if (item->m_colorIndex != 3)
                    {
                        changed = true;
                        item->m_colorIndex = 3;
                    }
                    if (local == player)
                        item->m_flags &= ~NIF_SquadMember;
                    else
                        item->m_flags |= NIF_SquadMember;
                    goto visibility;
                }
                relation = 2;
                haveRelation = true;
            }
        }
    }
    if (!haveRelation)
        relation = isDemoPlayback() ? player->getTeam() : 1;            // slot +0x68, a virtual call
    if (item->m_colorIndex != relation)
    {
        item->m_colorIndex = relation;
        changed = true;
    }

visibility:
    // drawn unless the local player shares the vehicle in first person (0x1012E257)
    if (local)
    {
        void* const playerRoot = rootParent(player->getVehicle());
        if (rootParent(local->getVehicle()) == playerRoot)
            item->m_flags &= ~NIF_SquadMember;
        if (local->hasFirstPersonView())                                 // PE +0x184
        {
            void* const playerRootAgain = rootParent(player->getVehicle());
            if (rootParent(local->getVehicle()) == playerRootAgain)
            {
                item->m_flags &= ~NIF_Visible;
                return changed;
            }
        }
    }
    item->m_flags |= NIF_Visible;
    return changed;
}

// no caller in the image; the constructor and clearAll inline the same switch.
int NametagManager::maxSlotCount(int type)
{
    switch (type)
    {
    case TTPlayer: return 64;
    case TTOrder:  return 2;
    case TTObject: return 64;
    default:       return -1;
    }
}

// unreferenced in the image. Counts the item's timer down by the frame delta and
// resets its call state at zero. The comparison is on the unrounded difference (fst keeps st0).
void NametagManager::tickItemTimer(NametagItem* item)
{
    if (item->m_timer > 0.0f)
    {
        const double remaining = item->m_timer - nametagFrameDelta();
        item->m_timer = static_cast<float>(remaining);
        if (remaining <= 0.0)
            resetItem(item);
    }
}

// unreferenced dispatchers (the update inlines the same type test).
void NametagManager::updateItemPosition(NametagItem* item)
{
    if (item->m_type == TTPlayer)
        updatePlayerItemPosition(item);
    else
        updateObjectItem(item);
}

void NametagManager::updateItemFade(NametagItem* item)
{
    if (item->m_type == TTPlayer)
        updatePlayerItemFade(item);
    else
        updateObjectItemFade(item);
}


// slots and items


// 0x1012F760, 176 B, retn 8. A key keeps its slot; otherwise the lowest free slot is taken; -1
// when the type's slots are exhausted.
int NametagManager::allocateSlot(int type, int key)
{
    NametagSlotMap& slots = m_slotMaps[type];
    NametagFreeSlotSet& freeSlots = m_freeSlots[type];

    if (slots.count(key) != 0)                                           // sub_101319E0 + _Distance
        return slots[key];

    if (freeSlots.size() == 0)
        return -1;

    const int slot = *freeSlots.begin();
    slots.insert(NametagSlotMap::value_type(key, slot));
    freeSlots.erase(freeSlots.begin());
    return slot;
}

// 0x10130120, 104 B, retn 4. An item without a slot (-1) is neither filed nor freed - it leaks,
// and the caller is told "no item". The Random is a fresh local seeded 0x759DC150 on every call,
// so every item gets the same "random" phase. Both shipped.
NametagItem* NametagManager::insertItem(NametagItem* item)
{
    if (!item || item->m_slot == -1)
        return 0;

    Random random;                                                       // m_seed = 0x759DC150
    const int value = (static_cast<int>(randomRaw(random) | 0x80u) >> 7) + 1;   // lcgrand_step
    item->m_lastSightCheckTime = static_cast<double>(value) * 2.98023223876953125e-08; // flt_1020CFD0 = 2^-25

    m_items[item->getKey()] = item;                                      // slot +0x04, sub_10132750
    return item;
}

// 0x10130820, 92 B, retn 4.
NametagItem* NametagManager::createPlayerItem(world::IPlayer* player)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(PlayerNametagItem));   // new(0xD0)
    NametagItem* item = 0;
    if (storage)
        item = new (storage) PlayerNametagItem(
            player, allocateSlot(TTPlayer, player->getId() + kNametagKeyPlayer));
    return insertItem(item);
}

// 0x10130880, 129 B, retn 4.
NametagItem* NametagManager::createObjectItem(world::IObject* object)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(ObjectNametagItem));   // new(0xD0)
    NametagItem* item = 0;
    if (storage)
        item = new (storage) ObjectNametagItem(
            object, allocateSlot(TTObject, static_cast<int>(objectId(object)) + kNametagKeyObject));
    return insertItem(item);
}

// 0x10130910, 94 B, retn 0Ch.
NametagItem* NametagManager::createOrderItem(SquadOrder* order, int orderType, const Vec3& position)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(OrderNametagItem));    // new(0x108)
    NametagItem* item = 0;
    if (storage)
        item = new (storage) OrderNametagItem(
            order, orderType, position, allocateSlot(TTOrder, orderType + kNametagKeyOrder));
    return insertItem(item);
}

// 0x10130970, 94 B, retn 0Ch.
NametagItem* NametagManager::createOrderItem(SquadOrder* order, int orderType, world::IObject* target)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(OrderNametagItem));
    NametagItem* item = 0;
    if (storage)
        item = new (storage) OrderNametagItem(
            order, orderType, target, allocateSlot(TTOrder, orderType + kNametagKeyOrder));
    return insertItem(item);
}

// 0x1012C600, 195 B, retn 8.
int NametagManager::orderIcon(int orderType, int orderId)
{
    if (orderType == 0)
        return orderId + kIconSquadOrderBase;
    if (orderType == 1)
        return orderId + kIconCommanderOrderBase;
    // sub_10074300(6, __FILE__, 2146, "") << "Unknown order - type: " << type << " subtype: " << id
    ::renddx9abi::logf("NametagManager.cpp:2146 Unknown order - type: %d subtype: %d",
                       orderType, orderId);
    return kIconUnknownOrder;
}


// the device listener - `this` at +0x08 in the shipped bodies

void NametagManager::onDeviceLost()            // renderers 0, 1, 2
{
    for (int type = TTPlayer; type <= TTObject; ++type)
        m_renderers[type].onDeviceLost();
}

void NametagManager::onDeviceReset()
{
    for (int type = TTPlayer; type <= TTObject; ++type)
        m_renderers[type].onDeviceReset();
    rebufferAll();                             // `jmp [eax+40h]` - a virtual tail call
}

void NametagManager::onDeviceDestroy()
{
    for (int type = TTPlayer; type <= TTObject; ++type)
        m_renderers[type].onDeviceDestroy();   // nullsub_67 (0x101B06F0)
}


// the `nametags` console surface - member accessors whose `this` the executors load but never use


// 0x1012C6D0, retn 0Ch - `createBar <NametagItemIDs> <start> <end>`.
void NametagManager::createBar(int itemId, int barStart, int barEnd)
{
    m_currentItemId = itemId;
    m_textureMode = 1;
    m_barStart = barStart;
    m_barEnd = barEnd;
}

// 0x1012C700, retn 4 - `createIcon <NametagItemIDs>`.
void NametagManager::createIcon(int itemId)
{
    m_currentItemId = itemId;
    m_textureMode = 0;
}


// 0x1012C720, 249 B, retn 30h - `setTexture <count> <path> <x> <y> <w> <h>` (the path BY value).
// An icon registers `count` frames at once. A bar needs two calls: index 0 stores the full
// texture and its rect, index 1 registers both textures.
// The second call's rect is ignored - the bar is registered with the first texture's rect.
// Any other index in bar mode just replaces the stored texture.

void NametagManager::setTexture(int count, std::string path, int x, int y, int width, int height)
{
    if (m_textureMode == 0)
    {
        NameTagRenderer::setTexture(m_currentItemId, count, path, x, y, width, height);
        return;
    }
    if (m_textureMode == 1)
    {
        if (m_pendingBarTexture == 0 && count == 1)
        {
            NameTagRenderer::createBar(m_currentItemId, m_pendingBarPath, path,
                                            m_pendingBarRect[0], m_pendingBarRect[1],
                                            m_pendingBarRect[2], m_pendingBarRect[3],
                                            m_barStart, m_barEnd);
            m_pendingBarTexture = -1;
            return;
        }
        m_pendingBarTexture = count;
        m_pendingBarRect[1] = y;
        m_pendingBarRect[0] = x;
        m_pendingBarRect[2] = width;
        m_pendingBarRect[3] = height;
        m_pendingBarPath = path;
    }
}

// 0x1012C870, retn 4 - an out-of-range type answers the player entry.
const Vec2& NametagManager::getCullDistance(int type)
{
    if (type >= 0 && type < 3)
        return s_cullDistances[type];
    return s_cullDistances[0];
}

// 0x1012D5E0, retn 8 - an out-of-range type is ignored.
void NametagManager::setCullDistance(int type, const Vec2& distance)
{
    if (type >= 0 && type < 3)
        s_cullDistances[type] = distance;
}

float  NametagManager::getEnemyDotLimit()                  { return s_enemyDotLimit; }
void   NametagManager::setEnemyDotLimit(float v)           { s_enemyDotLimit = v; }
float  NametagManager::getMaxCallDistance()                { return s_maxCallDistance; }
void   NametagManager::setMaxCallDistance(float v)         { s_maxCallDistance = v; }
float  NametagManager::getVehicleNametagOffset()           { return s_vehicleNametagOffset; }
void   NametagManager::setVehicleNametagOffset(float v)    { s_vehicleNametagOffset = v; }
float  NametagManager::getVehicleNametagOffsetFactor()     { return s_vehicleNametagOffsetFactor; }
void   NametagManager::setVehicleNametagOffsetFactor(float v) { s_vehicleNametagOffsetFactor = v; }
float  NametagManager::getMaxFriendlyDistance()            { return s_maxFriendlyDistance; }
void   NametagManager::setMaxFriendlyDistance(float v)     { s_maxFriendlyDistance = v; }
double NametagManager::getEnemyTagDelayTime()              { return s_enemyTagDelayTime; }
void   NametagManager::setEnemyTagDelayTime(double v)      { s_enemyTagDelayTime = v; }
double NametagManager::getDeployableDistance()             { return s_deployableDistance; }
void   NametagManager::setDeployableDistance(double v)     { s_deployableDistance = v; }
double NametagManager::getEnemyTagFadeInTime()             { return s_enemyTagFadeInTime; }
void   NametagManager::setEnemyTagFadeInTime(double v)     { s_enemyTagFadeInTime = v; }
double NametagManager::getEnemyTagFadeOutTime()            { return s_enemyTagFadeOutTime; }
void   NametagManager::setEnemyTagFadeOutTime(double v)    { s_enemyTagFadeOutTime = v; }
float  NametagManager::getDeathFadeOutTime()               { return s_deathFadeOutTime; }
void   NametagManager::setDeathFadeOutTime(float v)        { s_deathFadeOutTime = v; }
float  NametagManager::getManDownFadeOutTime()             { return s_manDownFadeOutTime; }
void   NametagManager::setManDownFadeOutTime(float v)      { s_manDownFadeOutTime = v; }
bool   NametagManager::getUseVisualRadiusForVehicles()     { return s_useVisualRadiusForVehicles; }
void   NametagManager::setUseVisualRadiusForVehicles(bool v) { s_useVisualRadiusForVehicles = v; }


// Creates the nametag manager interface. Failed allocation is not checked before dispatch.

IBase* __fastcall createNametagManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(NametagManager));   // new(0x5D8)
    NametagManager* const self = storage ? new (storage) NametagManager() : 0;

    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

// The per-frame update closure (0x10130360 and its callees) - inside the namespaces, so that it
// uses this TU's anonymous-namespace host seams.
#include "NametagManagerUpdate.inl"

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
