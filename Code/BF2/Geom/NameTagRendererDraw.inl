// Path: [INFERRED], [CHOSEN] split of NameTagRenderer.cpp; never compile separately.
// Per-frame nametag drawing, recovered from RendDX9.dll 1.50 (0x101B0550..0x101B3ECA).
// Included after NameTagRenderer.cpp's namespace closure, inside BF2_IMAGE_RENDDX9.
// Float operations retain retail order; SSE2 double and transcendental results can differ
// from the retail x87 device context.

#ifndef GEOM_NAMETAGRENDERERDRAW_INL
#define GEOM_NAMETAGRENDERERDRAW_INL

#ifdef BF2_IMAGE_RENDDX9

#include "NameTagRenderer.h"

#include "StaticMesh.h"                  // staticMeshRootParent - sub_10103CE0, RendDX9's getRootParent
#include "../Core/Ids.h"
#include "../Core/WeakPtr.h"
#include "../Math/Mat4.h"
#include "../Math/Vec3.h"                // Vec4
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderer.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"

// The host interfaces, imported the way BundledMeshDraw.cpp / XpLightManager.cpp import them: under
// the retail (compat) host ABI, HfeAbi.h already supplies ClassManager's VC7.1 registration carriers,
// so the engine's own ClassManager header is kept out while these interfaces come in. Only virtual
// dispatch and scalar/pointer fields cross the boundary below; the one host container (getPcos) is
// copied at ntCopySeats.
#include "../Game/SingletonServer.h"     // INametagManager - needs the integrator's +0x60..+0x68
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ISoldier.h"
#include "../Game/Objects/ObjectLayoutAccess.h"
#include "../Game/Objects/GameLogicComp/IAbilityGLComp.h"
#include "../Game/Objects/GameLogicComp/IArmorGLComp.h"
#include "../Game/Objects/Game/Kit.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

#include <cmath>
#include <cstring>
#include <map>
#include <utility>

// D3DXMatrixRotationZ is the d3dx9_25 import behind the thunk 0x101C4ACE (called by drawArrows and
// drawIcons); its D3DXMATRIX is sixteen floats (a Mat4 here).

namespace dice {
namespace hfe {

extern IBase* playerManager;                                     // PluginEntry.cpp

namespace dx9rend {

extern void* g_rendererBase;                                     // dword_1023B970
// BufferManager.cpp / StateManager.cpp - declared per TU, as the other geometry classes do.
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  indexBufferBind(void* wrapper);
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);
// Named after the D3D call, NOT in the shipped push order - see StateManager.cpp.
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);

} // namespace dx9rend

namespace geom {

// ModuleGeom.cpp - dword_1024B91C, the singleton slot ClassManager fills with the
// queryInterface(0xB801DCFE) result, i.e. the manager's INametagManager (primary) pointer. The image
// re-reads it before every call; so does ntManager().
extern IBase* g_NametagManager;

// [CHOSEN, not recovered] integration helpers for tag drawing.
// Draw paths borrow the manager's item map, pulse value and maximum call distance through the
// INametagManager slots the image calls.

namespace {


// The item fields this closure reads are NametagItem's typed members (NametagManager.h); the
// shipped instructions per field:
//   m_slot             +0x08  every draw: `mov ecx,[esi+8]; shl ecx,4` (the Vec4 index)
//   m_type             +0x0C  `cmp [esi+0Ch], m_type` (arrows, icons) / `cmp [esi+0Ch],0`
//   m_distance         +0x54  [INFERRED] rounded metres to the local player's vehicle (update
//                             0x1012CE93..0x1012CEB3 fistp); drawIcons 0x101B2059 `fild [esi+54h]`
//   m_viewDistance     +0x58  [INFERRED] the item's own fade END; 0.0 = use the renderer's pair.
//                             `fld qword [esi+58h]` in every FadeOut
//   m_colorIndex       +0x64  `mov cx, word [esi+64h]` (arrows), `movzx edx, word [esi+64h]` (bars),
//                             `mov ebx,[esi+64h]` (icons)
//   m_icon             +0x68  drawIcons: the s_icons key of the icon
//   m_callIcon         +0x6C  drawIcons 0x101B1CE0 (TEXLAYER1 icon, -1 = the icon itself); vehicle
//                             icons 0x101B39DB (a dead lookup)
//   m_flashColorIndex  +0x70  `mov edx,[esi+70h]; cmp edx,-1` - -1 = m_colorIndex
//   m_elevationIcon    +0x78  [INFERRED] update writes 48 (above) / 49 / -1 (0x1012CDF8, 0x1012CE13);
//                             drawIcons 0x101B1D24 draws it as a second quad
//   m_iconRotation     +0x7C  radians; drawIcons 0x101B1FDD, when flags bit 9
//   m_team             +0x80  `cmp [esi+80h], eax` against the viewer's getTeam()
//   m_flags            +0xB8  bits read here: 3, 4, 5, 9, 11, 12, 14, 16 (the byte test
//                             `test byte [esi+0BAh],1`), 17 - see the bodies
//   m_arrowMode        +0xBC  nonzero: drawn as the off-screen arrow
//   m_inView           +0xBE  [INFERRED] update's render-view frustum test of the item's +0x90
//                             position
//   (m_dirty, +0xBD, is not read by this closure.)

// flags bit n, as the image tests it (`shr reg, n; test reg8, 1`, or a byte test for bit 16).
inline bool ntFlag(const NametagItem& item, unsigned int bit)
{
    return ((item.m_flags >> bit) & 1u) != 0;
}

// Seam 5: NametagItem::getPlayer() const, 0x10131540 - `mov eax,[ecx+0C0h]; test eax,eax; je;
// mov eax,[eax+4]` = the +0xC0 WeakPtr's get(), PlayerNametagItem::m_player. The image calls it
// from the three bar draws and inlines it in the vehicle-icon loop; both are this one source-level
// call. Every caller has already tested the item's type for 0 (a player item). (Renderer-2 items
// keep an object there: update 0x101305A8 reads its +0x14 template.)
inline world::IPlayer* ntItemPlayer(NametagItem* item)
{
    return static_cast<PlayerNametagItem*>(item)->m_player.get();
}


// Seam 4: the manager's item map, std::map<int, NametagItem*> (NametagItemMap). The draws walk it
// in key order (the shipped _Tree iterator ++: inlined at 0x101B1174 (drawArrows) and in drawIcons,
// the out-of-line 0x100A54F0 elsewhere).
typedef NametagItemMap NtItemMap;


// The host singletons, re-read at every use as the image does.

inline INametagManager* ntManager()
{
    return static_cast<INametagManager*>(g_NametagManager);
}

inline dx9rend::IRenderer* ntRenderer()
{
    return static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase);
}

inline world::IPlayer* ntLocalHumanPlayer()
{
    return reinterpret_cast<world::PlayerManager*>(playerManager)->getLocalHumanPlayer();   // +0x30
}

// 0x101B0550, 27 B - [INFERRED] the viewing player: the local human player, else the spectated one.
//     mov ecx,[10244AE0h]; call [eax+30h]; test eax,eax; jnz ret   ; getLocalHumanPlayer
//     mov ecx,[1024B91Ch]; jmp [edx+64h]                           ; getSpectatedPlayer (tail)
// The shipped copy at that address has NO caller: draw() (0x101B3E53), the three bar/icon draws
// (0x101B1209, 0x101B16C9, 0x101B33AC) and the vehicle-icon team test (0x101B38AE) all inline it.
world::IPlayer* ntViewingPlayer()
{
    world::IPlayer* const local = ntLocalHumanPlayer();
    if (local)
        return local;
    return ntManager()->getSpectatedPlayer();                        // +0x64
}

// getTransformations(int) - +0x38 - answers the renderer's 64 x Vec4 array (item->slot << 4 indexes
// it). The tree's INametagManager spells the return void* [UNRESOLVED]; the cast lives here only.
inline const Vec4* ntTransformations(int type)
{
    return static_cast<const Vec4*>(static_cast<const void*>(ntManager()->getTransformations(type)));
}

// getItems() - +0x54 - the manager's item map by reference (`call [edx+54h]` then [eax+4]/[eax+8]:
// the shipped head and size words). The interface spells the answer const void*; the cast lives here.
inline const NtItemMap& ntItems()
{
    return *static_cast<const NtItemMap*>(ntManager()->getItems());
}


// Clamps high first, then low. Unordered comparisons map NaN to low.

float ntClamp(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// sub_10103CB0 - itself first, then the +0x28 parent chain, asking each for
// IID_IPlayerControlObject (read from ds:10208CF0 = 50373); the first answer, not the object.
//     test esi,esi; je 0 / loop: call [eax+0Ch](50373); jnz ret; mov esi,[esi+28h]; test; jnz loop
world::IPlayerControlObject* ntOwningControlObject(world::IObject* object)
{
    while (object)
    {
        void* const found = object->queryInterface(world::IID_IPlayerControlObject);
        if (found)
            return static_cast<world::IPlayerControlObject*>(found);
        object = object->getParent();                                // the raw +0x28 read
    }
    return 0;
}

// The kit's cached ability component: `mov ecx,[eax+64h]` straight off ISoldier::getKit()'s result
// (Kit's IObject base is its primary base, so the image applies no adjustment) - IObject's
// m_abilityGLComponent, IID_IAbilityGLComp.
inline world::IAbilityGLComp* ntKitAbility(world::Kit* kit)
{
    return static_cast<world::IAbilityGLComp*>(
        world::object_detail::LayoutAccess::abilityGLComponent(kit));
}


// Sets shader fading across start/end, using the item's range when nonzero (including NaN).
// Item ranges start at 33% of their end distance. The two helper variants preserve
// distinct floating-point expressions for their bias calculations.

void ntSetFadeOutA(dx9rend::Effect* effect, double itemFadeDistance, float start, float end)
{
    float fadeOut[2];
    if (itemFadeDistance != 0.0)
    {
        const double itemStart = itemFadeDistance * 0.33;
        const double scale = 1.0f / (itemFadeDistance - itemStart);
        fadeOut[0] = static_cast<float>(scale);
        fadeOut[1] = static_cast<float>(-(scale * itemStart));
        dx9rend::effectGetShaderParameterBorrowedTyped(effect, std::string("FadeOut"))
            ->setValue(fadeOut, 0);
    }
    else
    {
        const float scale = 1.0f / (end - start);
        fadeOut[0] = scale;
        fadeOut[1] = -(scale * start);
        dx9rend::effectGetShaderParameterBorrowedTyped(effect, std::string("FadeOut"))
            ->setValue(fadeOut, 0);
    }
}

void ntSetFadeOutB(dx9rend::Effect* effect, double itemFadeDistance, float start, float end)
{
    float fadeOut[2];
    if (itemFadeDistance != 0.0)
    {
        const double itemStart = itemFadeDistance * 0.33;
        const double range = itemFadeDistance - itemStart;
        fadeOut[0] = static_cast<float>(1.0f / range);
        fadeOut[1] = static_cast<float>(-1.0f / range * itemStart);
        dx9rend::effectGetShaderParameterBorrowedTyped(effect, std::string("FadeOut"))
            ->setValue(fadeOut, 0);
    }
    else
    {
        const float range = end - start;
        fadeOut[0] = 1.0f / range;
        fadeOut[1] = -1.0f / range * start;
        dx9rend::effectGetShaderParameterBorrowedTyped(effect, std::string("FadeOut"))
            ->setValue(fadeOut, 0);
    }
}


// The by-value copy of `*pco->getPcos()` the vehicle-icon loop walks (0x101B3967: 0x10132860, the
// map copy constructor, into a local at [ebp-9Ch]; destroyed by 0x10132580 at 0x101B3E18).
// THE MAP belongs TO THE host: its std::map<int, IPlayerControlObject*> (in the image 0x18-byte
// nodes, the same shape as the item map). The copy is plugin-local and is walked in key order.

typedef std::map<int, world::IPlayerControlObject*> NtSeatMap;

NtSeatMap ntCopySeats(world::IPlayerControlObject* pco)
{
    return NtSeatMap(*pco->getPcos());                                   // +0x60
}

// The s_icons / s_bars keys this closure looks up: values of the console enum "NametagItemIDs"
// (0x100B15BE, nt_core's range). Stock Menu_client.zip:Nametag/nametags.con registers all six.
const int kICVehicleMedic  = 36;
const int kICVehicleRepair = 37;
const int kICVehicleAmmo   = 38;
const int kICHealthBar     = 39;
const int kICArmorBar      = 40;
const int kICAmmoBar       = 41;

// Kit types, [INFERRED] from the vehicle icon each selects (36 medic, 37 repair, 38 ammo) and the
// armour-bar gate: 3 support, 4 engineer, 5 medic. The tree spells kit types as raw numbers too.
const int kKitSupport  = 3;
const int kKitEngineer = 4;
const int kKitMedic    = 5;

const float kHalfPi          = 1.5707964f;    // flt_10221D64 = 0x3FC90FDB (D3DX_PI / 2)
const float kAmmoStatusScale = 0.06666667f;   // flt_10221DCC = 0x3D888889 (1/15: getAmmoStatus 0..15)

} // anonymous namespace


// 0x101B3E50, 122 B - the per-renderer draw NametagManager::draw calls for renderers 2, 1, 0.

// The StateManager bracket (sub_100261A0 / sub_100261B0, the latter a tail jump) wraps everything,
// including a demo playback's names-only frame. The viewing-player gate is the inlined 0x101B0550.

void NameTagRenderer::draw()
{
    if (!ntViewingPlayer())
        return;

    dx9rend::stateManagerPush();
    uploadSlots();
    drawNames();
    if (!ntManager()->isDemoPlayback())                              // +0x68, `test al,al`
    {
        drawArrows();
        drawArmorBarsAndVehicleIcons();
        drawHealthBars();
        drawAmmoBars();
        drawIcons();
    }
    dx9rend::stateManagerPop();
}


// Returns the nearest ancestor armor's health fraction, clamped to [0,1], or zero without
// armor. Zero maximum health is not guarded; a NaN fraction clamps to zero.

float NameTagRenderer::hitPointFraction(world::IObject* object)
{
    world::IArmorGLComp* const armor =
        static_cast<world::IArmorGLComp*>(NametagManager::findArmorComponent(object));
    if (!armor)
        return 0.0f;
    const float hitPoints = armor->getHitPoints();                   // IArmorGLComp +0x3C
    return ntClamp(hitPoints / armor->getMaxHitPoints(), 0.0f, 1.0f);   // +0x84
}


// 0x101B05E0, 88 B - when bufferItem has dirtied the records, copy m_slotCount * 224 bytes of them
// into the slot vertex buffer. The byte count is `(n * 8 - n) << 5` (rep movsd + movsb).
// The unlock and the m_dirty clear run whether or not the lock succeeded.

void NameTagRenderer::uploadSlots()
{
    if (!m_dirty)
        return;
    void* const vertices = dx9rend::vertexBufferLock(m_vertexBuffer, 0, 0, 0);
    if (vertices)
        std::memcpy(vertices, m_slots,
                    static_cast<unsigned int>(m_slotCount * static_cast<int>(sizeof(NametagSlotRecord))));
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
    m_dirty = 0;
}


// Draws all name slots in one indexed pass using the manager's colors, transforms, and alpha.
// Names fade between view-space w = 2000 and 3000, independently of renderer fade settings.

void NameTagRenderer::drawNames()
{
    dx9rend::effectSetTechniqueByName(s_effect, std::string("nametag"));   // result dropped
    dx9rend::effectBegin(s_effect);
    dx9rend::effectBeginPass(s_effect, 0);

    float fadeOut[2];
    fadeOut[0] = 0.001f;
    fadeOut[1] = -2.0f;
    dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("FadeOut"))
        ->setValue(fadeOut, 0);
    dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("CrossFadeValue"))
        ->setValue(&NametagManager::s_pulse, 0);

    s_colorArray->setValue(ntManager()->getColors(m_type),
                           static_cast<unsigned int>(m_colorCount));             // +0x34
    s_transformationArray->setValue(ntManager()->getTransformations(m_type),
                                    static_cast<unsigned int>(m_slotCount));     // +0x38
    s_texLayer0->setValue(m_nameTexture, 0);
    s_alphaArray->setValue(ntManager()->getAlphaArray(m_type),
                           static_cast<unsigned int>(m_slotCount));              // +0x3C

    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::indexBufferBind(m_indexBuffer);
    dx9rend::rendererDrawIndexedPrimitive(static_cast<unsigned int>(m_slotCount * 4),
                                          D3DPT_TRIANGLELIST, 0, 0u,
                                          static_cast<unsigned int>(m_slotCount * 8), 0u);
    dx9rend::effectEnd(s_effect);
}


// Draws rotated offscreen arrows from item transformations.
// Aspect scaling uses arrow width on both axes; repeated view/transform queries are intentional.

void NameTagRenderer::drawArrows()
{
    dx9rend::effectSetTechniqueByName(s_effect, std::string("nametag_arrow"));
    if (ntItems().size() == 0)                                       // `cmp [eax+8],0; jbe`
        return;

    dx9rend::effectBegin(s_effect);
    dx9rend::effectBeginPass(s_effect, 0);
    s_texLayer0->setValue(s_arrowTexture, 0);

    dx9rend::IRenderView* const heightView = ntRenderer()->getRenderViewDup();   // +0x50
    dx9rend::IRenderView* const widthView  = ntRenderer()->getRenderViewDup();   // +0x50 again
    const float* const heightViewport = heightView->getViewport();              // +0x14
    const float* const widthViewport  = widthView->getViewport();
    float aspectMul[4];
    aspectMul[1] = static_cast<float>(dx9rend::textureDims(s_arrowTexture)[0]) / heightViewport[3];
    aspectMul[0] = static_cast<float>(dx9rend::textureDims(s_arrowTexture)[0]) / widthViewport[2];
    aspectMul[2] = 1.0f;
    aspectMul[3] = 1.0f;
    dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("AspectMul"))
        ->setValue(aspectMul, 0);

    const NtItemMap& items = ntItems();                              // the second getItems call
    for (NtItemMap::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        NametagItem& item = *it->second;
        if (item.m_type != m_type || !item.m_arrowMode)
            continue;

        const Vec4* const forY = ntTransformations(m_type);
        const Vec4* const forX = ntTransformations(m_type);
        Mat4 rotation;
        D3DXMatrixRotationZ(reinterpret_cast<D3DXMATRIX*>(&rotation),
                            std::atan2(forX[item.m_slot].x, forY[item.m_slot].y) + kHalfPi);
        float arrowRotation[4];
        arrowRotation[0] = rotation.m[0];                            // _11
        arrowRotation[1] = rotation.m[1];                            // _12
        arrowRotation[2] = rotation.m[4];                            // _21
        arrowRotation[3] = rotation.m[5];                            // _22
        const Vec4 arrowTransformation = ntTransformations(m_type)[item.m_slot];

        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ArrowRotation"))
            ->setValue(arrowRotation, 0);
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ArrowTransformation"))
            ->setValue(&arrowTransformation, 0);
        ntSetFadeOutA(s_effect, item.m_viewDistance, m_fadeStart, m_fadeEnd);

        NametagVertex* const vertices =
            static_cast<NametagVertex*>(dx9rend::vertexBufferLock(m_arrowVertexBuffer, 0, 0, 0));
        if (vertices)
        {
            // The item is re-read for every vertex, as the image does.
            for (int i = 0; i < 6; ++i)
            {
                vertices[i].slot        = static_cast<short>(item.m_slot);
                vertices[i].colorIndex1 = static_cast<short>(item.m_colorIndex);
                vertices[i].colorIndex2 = static_cast<short>(item.m_flashColorIndex != -1
                                                             ? item.m_flashColorIndex
                                                             : item.m_colorIndex);
            }
        }
        dx9rend::vertexBufferUnlock(m_arrowVertexBuffer);
        dx9rend::vertexBufferBindStream(m_arrowVertexBuffer, 0);
        dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLELIST, 0);
    }
    dx9rend::effectEnd(s_effect);
}


// Draws low-health teammate bars for a medic viewer, excluding occupants of other vehicles.
// Requires the ICHealthBar entry when the bar map is nonempty.

void NameTagRenderer::drawHealthBars()
{
    if (m_type != 0)
        return;

    world::IPlayer* const viewer = ntViewingPlayer();
    if (!viewer)
        return;
    world::IObject* const viewerSoldierObject = viewer->getDefaultVehicle();     // PE +0xA0
    if (!viewerSoldierObject)
        return;
    world::ISoldier* const viewerSoldier = static_cast<world::ISoldier*>(
        viewerSoldierObject->queryInterface(world::IID_ISoldier));               // ds:10208D3C
    if (!viewerSoldier)
        return;
    world::Kit* const viewerKit = viewerSoldier->getKit();                      // ISoldier +0x54
    if (!viewerKit)
        return;
    world::IAbilityGLComp* const ability = ntKitAbility(viewerKit);
    if (!ability)
        return;
    if (!ability->getGLInfo()->hasHealingAbility)                               // +0x38, byte 0
        return;

    const NtItemMap& items = ntItems();
    dx9rend::effectSetTechniqueByName(s_effect, std::string("nametag_healthbar"));
    if (s_bars.size() == 0)                                          // dword_102670B8, `jbe`
        return;
    const BarMap::iterator bar = s_bars.find(kICHealthBar);          // unchecked

    dx9rend::effectBegin(s_effect);
    dx9rend::effectBeginPass(s_effect, 0);
    s_texLayer0->setValue(bar->second.full.texture, 0);
    s_texLayer1->setValue(bar->second.empty.texture, 0);
    dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex1"))
        ->setInt(0);
    dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex2"))
        ->setInt(0);

    for (NtItemMap::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        if (m_type != 0)
            continue;
        NametagItem* const touchItem = it->second;
        NametagItem& item = *touchItem;
        if (item.m_type != 0)
            continue;
        if (item.m_team != viewer->getTeam())                        // PE +0xE4
            continue;
        if (!item.m_inView)
            continue;
        if (item.m_arrowMode)
            continue;

        world::IPlayer* const player = ntItemPlayer(touchItem);
        if (!player)
            continue;
        world::IObject* const vehicle = player->getVehicle();        // PE +0x44
        world::IObject* const soldierObject = player->getDefaultVehicle();   // PE +0xA0
        if (!vehicle || !soldierObject || vehicle != soldierObject)
            continue;

        const float health = hitPointFraction(vehicle);
        if (!(health < 0.5f))                                        // `fcomp; test ah,5; jp` - NaN skips
            continue;

        Vec4 translation = ntTransformations(m_type)[item.m_slot];
        const float invHalfHeight =
            1.0f / (ntRenderer()->getRenderView()->getViewport()[3] * 0.5f);     // +0x48, +0x14
        const int fillStart = bar->second.fillStart;
        const int fillRange = bar->second.fillEnd - fillStart;
        translation.y = translation.y - invHalfHeight * 16.0f;
        float healthValue = (fillRange * health + fillStart) / bar->second.width;
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("HealthValue"))
            ->setValue(&healthValue, 0);
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("HealthBarTrans"))
            ->setValue(&translation, 0);
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("CrossFadeValue"))
            ->setValue(&NametagManager::s_pulse, 0);
        ntSetFadeOutB(s_effect, item.m_viewDistance, m_fadeStart, m_fadeEnd);

        NametagVertex* const vertices = static_cast<NametagVertex*>(
            dx9rend::vertexBufferLock(bar->second.full.vertexBuffer, 0, 0, 0));
        if (vertices)
        {
            for (int i = 0; i < 6; ++i)
            {
                vertices[i].slot        = static_cast<short>(item.m_slot);
                vertices[i].colorIndex1 = static_cast<short>(item.m_colorIndex);
                vertices[i].colorIndex2 = static_cast<short>(item.m_colorIndex);
            }
        }
        dx9rend::vertexBufferUnlock(bar->second.full.vertexBuffer);
        dx9rend::vertexBufferBindStream(bar->second.full.vertexBuffer, 0);
        dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLELIST, 0);
    }
    dx9rend::effectEnd(s_effect);
}


// Draws low-ammo teammate bars for a support viewer, including eligible vehicle occupants.
// Requires ICAmmoBar when the bar map is nonempty; ammo status is normalized from 0..15.

void NameTagRenderer::drawAmmoBars()
{
    if (m_type != 0)
        return;

    world::IPlayer* const viewer = ntViewingPlayer();
    if (!viewer)
        return;
    world::IObject* const viewerSoldierObject = viewer->getDefaultVehicle();     // PE +0xA0
    if (!viewerSoldierObject)
        return;
    world::ISoldier* const viewerSoldier = static_cast<world::ISoldier*>(
        viewerSoldierObject->queryInterface(world::IID_ISoldier));
    if (!viewerSoldier)
        return;
    world::Kit* const viewerKit = viewerSoldier->getKit();
    if (!viewerKit)
        return;
    world::IAbilityGLComp* const ability = ntKitAbility(viewerKit);
    if (!ability)
        return;
    if (!ability->getGLInfo()->hasAmmoAbility)                                  // byte 2
        return;

    const NtItemMap& items = ntItems();
    dx9rend::effectSetTechniqueByName(s_effect, std::string("nametag_healthbar"));
    if (s_bars.size() == 0)
        return;
    const BarMap::iterator bar = s_bars.find(kICAmmoBar);            // unchecked

    dx9rend::effectBegin(s_effect);
    dx9rend::effectBeginPass(s_effect, 0);
    s_texLayer0->setValue(bar->second.full.texture, 0);
    s_texLayer1->setValue(bar->second.empty.texture, 0);
    float noCrossFade = 0.0f;                                        // [ebp-3Ch]
    dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("CrossFadeValue"))
        ->setValue(&noCrossFade, 0);
    dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex1"))
        ->setInt(0);
    dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex2"))
        ->setInt(0);

    for (NtItemMap::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        if (m_type != 0)
            continue;
        NametagItem* const touchItem = it->second;
        NametagItem& item = *touchItem;
        if (item.m_type != 0)
            continue;
        if (item.m_team != viewer->getTeam())
            continue;
        if (item.m_arrowMode)
            continue;
        if (!item.m_inView)
            continue;
        if (!ntFlag(item, 16))                                       // `test byte [esi+0BAh],1`
            continue;

        world::IPlayer* const player = ntItemPlayer(touchItem);
        if (!player)
            continue;
        world::IObject* const vehicle = player->getVehicle();
        world::IObject* const soldierObject = player->getDefaultVehicle();
        if (!vehicle || !soldierObject)
            continue;
        world::IPlayerControlObject* const control = ntOwningControlObject(vehicle);
        if (!control)
            continue;

        const float ammo = static_cast<float>(control->getAmmoStatus()) * kAmmoStatusScale;   // +0x14C
        if (!(ammo < 0.5f))
            continue;

        Vec4 translation = ntTransformations(m_type)[item.m_slot];
        const float invHalfHeight =
            1.0f / (ntRenderer()->getRenderView()->getViewport()[3] * 0.5f);
        translation.y = translation.y - invHalfHeight * 10.0f;       // flt_101E2B00
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("HealthBarTrans"))
            ->setValue(&translation, 0);

        const int fillStart = bar->second.fillStart;
        float ammoValue = (ntClamp(ammo, 0.0f, 1.0f) * (bar->second.fillEnd - fillStart) + fillStart)
                          / bar->second.width;
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("HealthValue"))
            ->setValue(&ammoValue, 0);
        ntSetFadeOutB(s_effect, item.m_viewDistance, m_fadeStart, m_fadeEnd);

        NametagVertex* const vertices = static_cast<NametagVertex*>(
            dx9rend::vertexBufferLock(bar->second.full.vertexBuffer, 0, 0, 0));
        if (vertices)
        {
            for (int i = 0; i < 6; ++i)
            {
                vertices[i].slot        = static_cast<short>(item.m_slot);
                vertices[i].colorIndex1 = static_cast<short>(item.m_colorIndex);
                vertices[i].colorIndex2 = static_cast<short>(item.m_flashColorIndex != -1
                                                             ? item.m_flashColorIndex
                                                             : item.m_colorIndex);
            }
        }
        dx9rend::vertexBufferUnlock(bar->second.full.vertexBuffer);
        dx9rend::vertexBufferBindStream(bar->second.full.vertexBuffer, 0);
        dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLELIST, 0);
    }
    dx9rend::effectEnd(s_effect);
}


// Draws eligible item icons and optional elevation quads using the manager's tag state.
// Uses registered icon textures, visibility/team flags and distance-scaled pulse fading.
// The elevation quad intentionally shares the main icon's texture after parameter overwrite.

void NameTagRenderer::drawIcons()
{
    const NtItemMap& items = ntItems();
    if (items.size() == 0)
        return;

    dx9rend::effectSetTechniqueByName(s_effect, std::string("nametag_vehicleIcons"));
    dx9rend::effectBegin(s_effect);
    dx9rend::effectBeginPass(s_effect, 0);

    for (NtItemMap::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        NametagItem& item = *it->second;
        if (item.m_type != m_type)
            continue;
        if (!ntFlag(item, 4))
            continue;
        if (!ntManager()->get3dMapOn() && !ntFlag(item, 17)
            && !(ntFlag(item, 14) && item.m_arrowMode))
            continue;
        if (!item.m_inView && !(ntFlag(item, 5) && ntFlag(item, 14)))
            continue;
        if (!ntFlag(item, 16))
            continue;

        const IconMap::iterator icon = s_icons.find(item.m_icon);
        IconMap::iterator flashIcon = icon;
        if (item.m_callIcon != -1)
            flashIcon = s_icons.find(item.m_callIcon);
        if (icon == s_icons.end())
            continue;

        const NametagIcon* flash = 0;                                // [ebp-10h]
        const NametagIcon* second = 0;                               // [ebp-0Ch]
        if (flashIcon != s_icons.end())
            flash = &flashIcon->second;
        if (item.m_elevationIcon != -1)
        {
            const IconMap::iterator elevation = s_icons.find(item.m_elevationIcon);
            if (elevation != s_icons.end())
            {
                second = &elevation->second;
                s_texLayer0->setValue(second->texture, 0);           // overwritten just below
            }
        }
        s_texLayer0->setValue(icon->second.texture, 0);
        s_texLayer1->setValue(flash ? flash->texture : 0, 0);

        Vec4 translation = ntTransformations(m_type)[item.m_slot];   // copied inline here
        const float invHalfHeight =
            1.0f / (ntRenderer()->getRenderView()->getViewport()[3] * 0.5f);
        const unsigned int iconWidth  = dx9rend::textureDims(icon->second.texture)[0];
        const unsigned int iconHeight = dx9rend::textureDims(icon->second.texture)[1];
        if (!item.m_arrowMode || ntFlag(item, 3))
            translation.y = static_cast<float>(iconHeight) * invHalfHeight + translation.y;
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("HealthBarTrans"))
            ->setValue(&translation, 0);

        const float iconInvWidth  = 1.0f / static_cast<float>(iconWidth);
        const float iconInvHeight = 1.0f / static_cast<float>(iconHeight);
        float iconTexOffset[4];
        iconTexOffset[2] = 0.0f;
        iconTexOffset[3] = 0.0f;
        iconTexOffset[0] = icon->second.left * iconInvWidth;
        iconTexOffset[1] = icon->second.top * iconInvHeight;
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("IconTexOffset"))
            ->setValue(iconTexOffset, 0);

        ntSetFadeOutA(s_effect, item.m_viewDistance, m_fadeStart, m_fadeEnd);

        static Mat4 s_iconRotation;                                  // ds:102670C0 (guard ds:10267100)
        D3DXMatrixRotationZ(reinterpret_cast<D3DXMATRIX*>(&s_iconRotation),
                            ntFlag(item, 9) ? item.m_iconRotation : 0.0f);
        float iconRotation[4];
        iconRotation[0] = s_iconRotation.m[0];                       // _11
        iconRotation[1] = s_iconRotation.m[1];                       // _12
        iconRotation[2] = s_iconRotation.m[4];                       // _21
        iconRotation[3] = s_iconRotation.m[5];                       // _22
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("IconRotation"))
            ->setValue(iconRotation, 0);

        float crossFade = (1.0f - ntClamp(static_cast<float>(item.m_distance) /
                                              NametagManager::s_maxCallDistance, 0.0f, 1.0f) * 0.5f)
                          * NametagManager::s_pulse;
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("CrossFadeValue"))
            ->setValue(&crossFade, 0);

        if (flash)
        {
            const float flashInvWidth =
                1.0f / static_cast<float>(dx9rend::textureDims(flash->texture)[0]);
            const float flashInvHeight =
                1.0f / static_cast<float>(dx9rend::textureDims(flash->texture)[1]);
            float flashScaleOffset[4];
            flashScaleOffset[0] = flashInvWidth / iconInvWidth;
            flashScaleOffset[1] = flashInvHeight / iconInvHeight;
            flashScaleOffset[2] = flash->left * flashInvWidth;
            flashScaleOffset[3] = flash->top * flashInvHeight;
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect,
                                                           std::string("IconFlashTexScaleOffset"))
                ->setValue(flashScaleOffset, 0);
        }

        const int colorIndex1 = ntFlag(item, 12) ? 0 : item.m_colorIndex;
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex1"))
            ->setInt(colorIndex1);
        if (ntFlag(item, 11) || item.m_flashColorIndex == -1)
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex2"))
                ->setInt(colorIndex1);
        else if (item.m_callIcon == -1)
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex2"))
                ->setInt(item.m_flashColorIndex);
        else
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex2"))
                ->setInt(0);

        dx9rend::vertexBufferBindStream(icon->second.vertexBuffer, 0);
        dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLELIST, 0);
        if (second)
        {
            dx9rend::vertexBufferBindStream(second->vertexBuffer, 0);
            dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLELIST, 0);
        }
    }
    dx9rend::effectEnd(s_effect);
}

// draw engineer armour bars, then 3D-map supply icons, over one item list.
// Keep the per-seat template tests, discarded icon lookup and repair x += height * 0.0f;
// the latter preserves NaN behaviour. getDefaultVehicle() is not null-tested in retail.
// [UNRESOLVED] the folded function-static guard has no observable storage.
void NameTagRenderer::drawArmorBarsAndVehicleIcons()
{
    world::IPlayer* const viewer = ntViewingPlayer();
    if (!viewer)
        return;

    const NtItemMap& items = ntItems();
    dx9rend::effectSetTechniqueByName(s_effect, std::string("nametag_healthbar"));

    // part 1: armour bars
    if (s_bars.size() != 0 && viewer->getKit() == kKitEngineer)                 // PE +0x16C
    {
        const BarMap::iterator bar = s_bars.find(kICArmorBar);       // unchecked
        dx9rend::effectBegin(s_effect);
        dx9rend::effectBeginPass(s_effect, 0);
        s_texLayer0->setValue(bar->second.full.texture, 0);
        s_texLayer1->setValue(bar->second.empty.texture, 0);
        float noCrossFade = 0.0f;                                    // [ebp-18h]
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("CrossFadeValue"))
            ->setValue(&noCrossFade, 0);
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex1"))
            ->setInt(0);
        dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex2"))
            ->setInt(0);

        for (NtItemMap::const_iterator it = items.begin(); it != items.end(); ++it)
        {
            if (m_type != 0)
                continue;
            NametagItem* const touchItem = it->second;
            NametagItem& item = *touchItem;
            if (item.m_type != 0)
                continue;
            if (item.m_team != viewer->getTeam())
                continue;
            if (!item.m_inView)
                continue;
            if (!ntFlag(item, 16))
                continue;

            world::IPlayer* const player = ntItemPlayer(touchItem);
            if (!player)
                continue;
            world::IObject* const vehicle = player->getVehicle();
            if (!vehicle)
                continue;
            if (vehicle->getTemplate()->getClassID() != world::CID_PlayerControlObjectTemplate)
                continue;                                            // template +0x14, vslot +0x10
            if (!NametagManager::findArmorComponent(vehicle))
                continue;
            const float armour = hitPointFraction(vehicle);          // walks it again
            if (!(armour < 0.5f))
                continue;

            Vec4 translation = ntTransformations(m_type)[item.m_slot];
            const float invHalfHeight =
                1.0f / (ntRenderer()->getRenderView()->getViewport()[3] * 0.5f);
            const int fillStart = bar->second.fillStart;
            const int fillRange = bar->second.fillEnd - fillStart;
            translation.y = translation.y - invHalfHeight * 16.0f;
            float armourValue = (fillRange * armour + fillStart) / bar->second.width;
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("HealthValue"))
                ->setValue(&armourValue, 0);
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("HealthBarTrans"))
                ->setValue(&translation, 0);
            ntSetFadeOutA(s_effect, item.m_viewDistance, m_fadeStart, m_fadeEnd);
            dx9rend::vertexBufferBindStream(bar->second.full.vertexBuffer, 0);
            dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLELIST, 0);
        }
        dx9rend::effectEnd(s_effect);
    }

    // part 2: the medic / repair / ammo icons over supply vehicles
    if (s_icons.size() == 0 || !ntManager()->get3dMapOn())           // dword_102670AC, +0x58
        return;

    const IconMap::iterator medicIcon  = s_icons.find(kICVehicleMedic);    // [ebp-50h]
    const IconMap::iterator repairIcon = s_icons.find(kICVehicleRepair);   // [ebp-64h]
    const IconMap::iterator ammoIcon   = s_icons.find(kICVehicleAmmo);     // [ebp-60h]
    dx9rend::effectSetTechniqueByName(s_effect, std::string("nametag_vehicleIcons"));
    dx9rend::effectBegin(s_effect);
    dx9rend::effectBeginPass(s_effect, 0);

    for (NtItemMap::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        if (m_type != 0)
            continue;
        NametagItem* const touchItem = it->second;
        NametagItem& item = *touchItem;
        if (item.m_type != 0)
            continue;
        if (!item.m_inView)
            continue;
        world::IPlayer* const teamViewer = ntViewingPlayer();        // inlined per item
        if (teamViewer && item.m_team != teamViewer->getTeam())
            continue;

        world::IPlayer* const player = ntItemPlayer(touchItem);      // inlined here in the image
        if (!player)
            continue;
        world::IObject* const vehicle = player->getVehicle();
        if (!vehicle)
            continue;
        if (!ntFlag(item, 16) && vehicle->getParent() != 0)          // the raw +0x28 read
            continue;
        world::IObject* const defaultVehicle = player->getDefaultVehicle();
        if (!defaultVehicle)
            continue;
        world::IObject* const root =
            static_cast<world::IObject*>(staticMeshRootParent(defaultVehicle));
        if (!root)
            continue;
        world::IPlayerControlObject* const rootControl = static_cast<world::IPlayerControlObject*>(
            root->queryInterface(world::IID_IPlayerControlObject));
        if (!rootControl)
            continue;

        const NtSeatMap seats = ntCopySeats(rootControl);
        for (NtSeatMap::const_iterator seat = seats.begin(); seat != seats.end(); ++seat)
        {
            world::IPlayerControlObject* const seatControl = seat->second;
            if (!seatControl->isOccupied())                          // +0x18
                continue;
            world::IPlayer* const occupant = seatControl->getOccupingPlayer();   // +0x10
            if (!occupant)
                continue;
            world::ISoldier* const occupantSoldier = static_cast<world::ISoldier*>(
                occupant->getDefaultVehicle()->queryInterface(world::IID_ISoldier));   // unguarded
            if (!occupantSoldier)
                continue;
            if (vehicle->getTemplate()->getClassID() == world::CID_SoldierTemplate)
                continue;

            bool medic = false;                                      // bl
            bool repair = false;                                     // [ebp-9]
            bool ammo = false;                                       // [ebp-1]
            IconMap::iterator icon;
            if (item.m_callIcon != -1)
                icon = s_icons.find(item.m_callIcon);                // dead - overwritten below
            switch (occupantSoldier->getKitType())                   // ISoldier +0x58
            {
            case kKitSupport:
                icon = ammoIcon;
                ammo = true;
                break;
            case kKitEngineer:
                icon = repairIcon;
                repair = true;
                break;
            case kKitMedic:
                icon = medicIcon;
                medic = true;
                break;
            default:
                icon = s_icons.find(-1);
                break;
            }
            if (icon == s_icons.end())                               // dword_102670A8, the head
                continue;

            s_texLayer0->setValue(icon->second.texture, 0);
            s_texLayer1->setValue(0, 0);

            Vec4 translation = ntTransformations(m_type)[item.m_slot];
            const float invHalfHeight =
                1.0f / (ntRenderer()->getRenderView()->getViewport()[3] * 0.5f);
            const unsigned int iconWidth = dx9rend::textureDims(icon->second.texture)[0];
            translation.y = translation.y - invHalfHeight * 32.0f;  // flt_10221E4C
            if (medic)
                translation.x = translation.x - invHalfHeight * 16.0f;
            else if (repair)
                translation.x = invHalfHeight * 0.0f + translation.x;   // adds zero
            else if (ammo)
                translation.x = invHalfHeight * 16.0f + translation.x;
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("HealthBarTrans"))
                ->setValue(&translation, 0);

            float iconRotation[4];                                   // see the guard note above
            iconRotation[0] = 1.0f;
            iconRotation[1] = 0.0f;
            iconRotation[2] = 0.0f;
            iconRotation[3] = 1.0f;
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("IconRotation"))
                ->setValue(iconRotation, 0);
            float noCrossFade = 0.0f;                                // [ebp-5Ch]
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("CrossFadeValue"))
                ->setValue(&noCrossFade, 0);
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex1"))
                ->setInt(0);
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("ColorIndex2"))
                ->setInt(0);
            ntSetFadeOutA(s_effect, item.m_viewDistance, m_fadeStart, m_fadeEnd);

            const unsigned int iconHeight = dx9rend::textureDims(icon->second.texture)[1];
            float iconTexOffset[4];
            iconTexOffset[2] = 0.0f;
            iconTexOffset[3] = 0.0f;
            iconTexOffset[0] = 1.0f / static_cast<float>(iconWidth) * icon->second.left;
            iconTexOffset[1] = 1.0f / static_cast<float>(iconHeight) * icon->second.top;
            dx9rend::effectGetShaderParameterBorrowedTyped(s_effect, std::string("IconTexOffset"))
                ->setValue(iconTexOffset, 0);

            dx9rend::vertexBufferBindStream(icon->second.vertexBuffer, 0);
            dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLELIST, 0);
        }
    }
    dx9rend::effectEnd(s_effect);
}

// s_bars.find (0x101B4D40) and s_icons.find (0x101B4CD0) are the std::map finds above; no body here.

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9

#endif // GEOM_NAMETAGRENDERERDRAW_INL
