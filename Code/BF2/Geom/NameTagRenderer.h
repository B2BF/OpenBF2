// Path: [INFERRED]; adjacent NametagManager.cpp is attested at RendDX9 0x1021CFB0.
// Nametag renderer declarations, recovered from RendDX9.dll 1.50 (PE only).
// The shipped member layout is 0x3860 bytes; the embedded trees are std containers here.
#ifndef GEOM_NAMETAGRENDERER_H
#define GEOM_NAMETAGRENDERER_H

#include <cstddef>
#include <map>
#include <set>

#include "../Core/IBase.h"
#include "../Math/Vec3.h"
#include "../RendDX9/RendDX9Base.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include <string>

namespace dice {
namespace hfe {

namespace world { class IObject; }

namespace dx9rend {
class Effect;
class ShaderParameter;
struct EngineTexture;
}

namespace geom {

// The manager's per-tag item (nt_core owns the class and its five vtables 0x1021CF70/80, 0x1021D080/
// 90/A0). [INFERRED] name, from INametagManager's addTouchItem family. bufferItem and the draws
// read its typed members (NametagManager.h).
class NametagItem;


// One vertex, 28 bytes - the Declaration annotation every Nametag.fx technique carries:
//     { 0, FLOAT3, position, 0 }, { 0, FLOAT2, texcoord, 0 }, { 0, SHORT4, blendindices, 0 }
// The vertex shader reads blendindices.x as the TransformationArray/AlphaArray index and .y/.z as
// the two ColorArray indices it cross-fades between (CrossFadeValue).
// The constructor is the per-vertex part of it zeroes the five floats and leaves the four
// shorts unwritten (onRenderPathActivated memsets the whole slot array before first use).

struct NametagVertex
{
    NametagVertex() : x(0.0f), y(0.0f), z(0.0f), u(0.0f), v(0.0f) {}

    float x, y, z;          // +0x00  position, clip-space offset from the tag's screen anchor
    float u, v;             // +0x0C  TEXCOORD0
    short slot;             // +0x14  blendindices.x - written by onRenderPathActivated (= slot index)
    short colorIndex1;      // +0x16  blendindices.y - written by bufferItem
    short colorIndex2;      // +0x18  blendindices.z - written by bufferItem
    short blendIndexW;      // +0x1A  blendindices.w - never written (0 after the activation memset)
};


// One tag's geometry, 224 bytes = 8 vertices = two quads indexed (i,i+1,i+2),(i+1,i+2,i+3):
//     vertices[0..3]  the rank-icon part of the row: u 0 .. iconWidth/512, colour indices "special"
//                     (3 for a colour-3 soldier on renderer 0, else 0)
//     vertices[4..7]  the name part: u iconWidth/512 .. textEnd/512, the item's own colour indices
// Corner order in each quad: top-left, top-right, bottom-left, bottom-right (bufferItem).

struct NametagSlotRecord
{
    enum { IconQuad = 0, NameQuad = 4, VertexCount = 8 };
    NametagVertex vertices[VertexCount];
};


// Icon resources and the source rectangle. setTexture stores inclusive right/bottom edges,
// so UpdateSurface copies one fewer row and column; createBar keeps the supplied bounds.
// Registration across multiple keys shares resources without taking additional references.

struct NametagIcon
{
    dx9rend::EngineTexture* texture;        // +0x00  TextureManager::getTexture result
    void*                   vertexBuffer;   // +0x04  6-vertex quad sized to the icon (see setTexture)
    int                     left;           // +0x08
    int                     top;            // +0x0C
    int                     right;          // +0x10
    int                     bottom;         // +0x14
};

// s_bars' value, 60 bytes (node 0x50: 12 + key 4 + this + 4). Both halves share ONE vertex buffer.
struct NametagBar
{
    NametagIcon full;        // +0x00  setTexture index 0 ("...BarFull.dds")  - TEXLAYER0 of the bar draw
    NametagIcon empty;       // +0x18  setTexture index 1 ("...BarEmpty.dds") - TEXLAYER1
    int         fillStart;   // +0x30  createBar's first number (stock: 23) - first fill texel
    int         fillEnd;     // +0x34  createBar's second number (stock: 52) - last fill texel
    int         width;       // +0x38  right - left, in texels: HealthValue =
                             //        ((fillEnd - fillStart) * fraction + fillStart) / width
};

// Renders one of NametagManager's three 64-slot atlases; text is buffered into 16-pixel rows.
// Six listener slots, no virtual destructor. The manager owns the array and deletes it.
// Preserve the constructor's uninitialized buffers/render path and the released-but-not-nulled
// shared resources; reactivation after all instances die retains the shipped lifetime defect.
class NameTagRenderer : public dx9rend::RenderPathListener
{
public:
    typedef std::map<int, NametagIcon> IconMap;
    typedef std::map<int, NametagBar>  BarMap;

    enum
    {
        SlotCapacity   = 64,     // m_slots / ctor loop / the 0x3800-byte activation memset
        RowHeight      = 16,     // pixels per slot row in m_nameTexture (slot << 4)
        TextureWidth   = 512,    // m_nameTexture / m_stagingTexture width  (u = px / 512)
        TextureHeight  = 1024    // m_nameTexture height                    (v = px / 1024)
    };

    // The two icon keys whose textures load into D3DPOOL_SYSTEMMEM (setTexture swaps the managed
    // flag to 2 around the load) because bufferItem / the target draw blit them with UpdateSurface.
    // Key values are the console enum "NametagItemIDs" (0x100B15BE, nt_core's range): ICRank 0,
    // ICRevive 32, ICMedic 33, ICRepair 34, ICAmmo 35, ICVehicleMedic 36, ICVehicleRepair 37,
    // ICVehicleAmmo 38, ICHealthBar 39, ICArmorBar 40, ICAmmoBar 41, ICSquadOrder 50,
    // ICCommanderOrder 60, ICTarget 70, ICObject 80, ICObjectVehicleDrop 112; NametagManager's init
    // (0x1012D610) registers the control-point icons 42, 43, 44, 46, 47 itself.
    static const int kRankIconKey   = 0;
    static const int kTargetIconKey = 70;

    NameTagRenderer();
    ~NameTagRenderer();                                            // NOT a slot

    // 0x101B0700, `retn 10h`. Stores the four arguments and registers this object as a render-path
    // listener (which, once a path has been published, calls onRenderPathActivated at once).
    // [INFERRED] parameter names; `fadeRange` is the address of an 8-byte (start, end) float pair
    // (0x10239A80 / 88 / 90) - a const Vec2& and a const float* are the same argument.
    void initialize(int type, int slotCount, int colorCount, const Vec2& fadeRange);

    // dx9rend::RenderPathListener, table 0x10221E24
    virtual void onRenderPathActivated(int path);                  // +0x00
    virtual void onRenderPathDeactivated();                        // +0x04 nullsub_15
    virtual void onRenderPathChanged(int path);                    // +0x08 nullsub_53
    virtual void onRenderPathNotification(int path);               // +0x0C nullsub_53
    virtual int  getCurrentRenderPath() const;                     // +0x10
    virtual std::string getRenderPathListenerName() const;        // +0x14

    // forwarded by NametagManager's DeviceLostResetListener subobject (0x1021D0E8), which loops
    // over the three renderers (renderer + i * 0x3860, i < 3)
    void onDeviceLost();       // drop the default-pool name texture's D3D resource
    void onDeviceReset();      // re-create it into the same wrapper (the manager
                               //              then calls its own rebufferAll, primary +0x40)
    void onDeviceDestroy();    // an empty body (`retn`)

    // buffering
    // 0x101B2320, 1,179 B, `retn 8`. Called by NametagManager's update (0x1013078F, rebuild true),
    // rebufferAll (0x1012ED65, true), 0x1012E3FB (false: colour indices only) and 0x10130101 (true).
    // rebuildText: clear the slot's row, blit the rank icon, draw the name, rebuild both quads.
    // false: only the eight vertices' colour indices are refreshed. Either way m_dirty = 1.
    void bufferItem(NametagItem* item, bool rebuildText);
    // 0x101B0640, `retn 8`: UpdateSurface(the blank staging row -> row slot*16). The second argument
    // is never read (the only caller passes 0).
    void clearSlotRow(int slot, int unused);

    // Draws only with an active or spectated player. Demo playback draws names only.
    // Uploads slots and restores StateManager state after drawing.
    void draw();
    void uploadSlots();                       // if (m_dirty): lock m_vertexBuffer (0,0,0),
                                              //   copy m_slotCount * 224 bytes of m_slots, unlock,
                                              //   m_dirty = 0
    void drawNames();                         // technique "nametag"; FadeOut (1e-3, -2),
                                              //   CrossFadeValue, ColorArray/TransformationArray/
                                              //   AlphaArray from the manager (+0x34/+0x38/+0x3C by
                                              //   m_type), TEXLAYER0 = m_nameTexture, one indexed draw
    void drawArrows();                        // 0x101B0DC0 - "nametag_arrow", s_arrowTexture,
                                              //   m_arrowVertexBuffer, AspectMul/ArrowRotation/
                                              //   ArrowTransformation/FadeOut per item
    void drawHealthBars();                    // 0x101B11F0 - "nametag_healthbar", s_bars key 39
    void drawAmmoBars();                      // 0x101B16B0 - "nametag_healthbar", s_bars key 41
    void drawIcons();                         // 0x101B1BB0 - "nametag_vehicleIcons", s_icons.find x3
    void drawArmorBarsAndVehicleIcons();      // s_bars key 40, then
                                              //   "nametag_vehicleIcons" with s_icons keys 36/37/38
    // 0x101B0580 - [INFERRED] name. ltcg convention: the object arrives in EAX, the result in ST0,
    // `retn` with no stack arguments. Walks object->+0x3C (a component) up object->+0x28 (parent)
    // until one is non-null; returns 0.0f if none, else clamp(c->vslot 0x3C() / c->vslot 0x84(), 0, 1)
    // through sub_1000ED40. Called by drawHealthBars (0x101B1420) and the armour draw (0x101B35C3).
    static float hitPointFraction(world::IObject* object);
    // (`retn 8`) is s_bars.find(key) - write it as the std::map operation.

    // the icon / bar registry: static, reached with ltcg register conventions
    // 0x101B2800, 368 B. ECX = key, EDX = count, `retn`. count == 0: release s_icons[key]'s texture
    // and vertex buffer and erase the key; count > 0: the same for key .. key + count - 1; count < 0:
    // nothing. Callers: NametagManager init helper 0x1012D610 (five times, count 0).
    static void removeTexture(int key, int count);
    // 0x101B2970, 905 B. ECX = key, EDX = count, then (const std::string& textureName, x, y, width,
    // height) on the stack, `retn 14h`. Loads the texture (systemmem for keys 0 and 70), builds a
    // 6-vertex quad of width x height pixels (UVs 0..1 for key 0, the texel rect otherwise) and
    // stores s_icons[key] - or, for count > 0, s_icons[key + i] = the i-th width-wide cell.
    // Callers: the manager's `Nametags.setTexture` path 0x1012C720 (count = the console's index
    // argument) and 0x1012D610 (count 0).
    static void setTexture(int key, int count, const std::string& textureName,
                           int x, int y, int width, int height);
    // 0x101B2D00, 502 B. ECX = key, EDX = &fullTextureName, then (const std::string& emptyTextureName,
    // left, top, right, bottom, fillStart, fillEnd), `retn 1Ch`. Caller: 0x1012C7B4 (the second
    // `Nametags.setTexture` after `Nametags.createBar`).
    static void createBar(int key, const std::string& fullTextureName,
                          const std::string& emptyTextureName, int left, int top, int right,
                          int bottom, int fillStart, int fillEnd);
    // ECX = key. s_bars.erase(key) with no resource release. NO caller in the image
    // (and IDA had not even made it a function); kept because the body is shipped.
    static void removeBar(int key);
    // The manager's init helper (0x1012D7A2) reads s_icons' size word (dword_102670AC) directly to
    // check that its five control-point registrations took (`size == before + 5`).
    static unsigned int iconCount();

    // 0x101B0570 - `m_byte384A = 1`. NO caller in the image. [UNRESOLVED] meaning.
    void setByte384A();

private:
    NameTagRenderer(const NameTagRenderer&);
    NameTagRenderer& operator=(const NameTagRenderer&);

    // data, in shipped order. The `+0xNN` comments give the shipped x86 offsets; from m_slotMap on
    // the std containers make the native offsets differ.
    int                     m_renderPath;           // +0x0004  never written (see the layout note)
    std::set<int>           m_freeSlots;            // +0x0008  vestigial (head sub_101315F0)
    std::map<int, int>      m_slotMap;              // +0x0014  vestigial, 0x18-byte nodes (head
                                                    //          sub_1008A320, the slot maps' shape)
    std::set<int>           m_set20;                // +0x0020  vestigial, 0x14-byte nodes (head
                                                    //          sub_101B4870); a four-byte key,
                                                    //          [INFERRED] int - nothing inserts one
    dx9rend::EngineTexture* m_stagingTexture;       // +0x002C
    dx9rend::EngineTexture* m_nameTexture;          // +0x0030
    void*                   m_vertexBuffer;         // +0x0034  not initialised by the ctor
    void*                   m_indexBuffer;          // +0x0038  not initialised by the ctor
    void*                   m_arrowVertexBuffer;    // +0x003C  not initialised by the ctor
    unsigned int            m_unused40[2];          // +0x0040  never referenced
    NametagSlotRecord       m_slots[SlotCapacity];  // +0x0048
    unsigned char           m_dirty;                // +0x3848
    unsigned char           m_byte3849;             // +0x3849  [UNRESOLVED], never read
    unsigned char           m_byte384A;             // +0x384A  [UNRESOLVED], set only by setByte384A
    unsigned char           m_pad384B;              // +0x384B
    int                     m_slotCount;            // +0x384C  not initialised by the ctor
    int                     m_colorCount;           // +0x3850  not initialised by the ctor
    int                     m_type;                 // +0x3854  not initialised by the ctor
    float                   m_fadeStart;            // +0x3858  ctor: 0
    float                   m_fadeEnd;              // +0x385C  ctor: 0

    // the shared static state (see the header comment for the image addresses)
    static IBase*                    s_font;                  // dword_10266D78  Font::Ported
    static dx9rend::Effect*          s_effect;                // dword_10266D7C
    static dx9rend::ShaderParameter* s_texLayer0;             // dword_10266D80
    static dx9rend::ShaderParameter* s_texLayer1;             // dword_10266D84
    static dx9rend::ShaderParameter* s_transformationArray;   // dword_10266D88
    static dx9rend::ShaderParameter* s_colorArray;            // dword_10266D8C
    static dx9rend::ShaderParameter* s_fadeOut;               // dword_10266D90
    static dx9rend::ShaderParameter* s_alphaArray;            // dword_10266D94
    static dx9rend::EngineTexture*   s_arrowTexture;          // dword_10266D98
    static int                       s_instanceCount;         // dword_10266D9C
    static BarMap                    s_bars;                  // (initialised first)
    static IconMap                   s_icons;

    friend struct NameTagRendererLayoutCheck;
};

// The vertex and record PODs keep their shipped sizes on every target (the GPU vertex declaration
// and the uploaded record block depend on them); the pointer-holding icon and bar values keep
// theirs on x86. The renderer itself holds std containers, so only the members before them keep
// their shipped offsets; NametagManager allocates and indexes the renderers as a typed array
// (shipped: 3 * 0x3860 + 4 at 0x1012F9B0, stride 0x3860).
struct NameTagRendererLayoutCheck
{
    static_assert(sizeof(NametagVertex) == 0x1C, "the Nametag.fx declaration is 28 bytes");
    static_assert(sizeof(NametagSlotRecord) == 0xE0, "0x101B4230 builds 224-byte records");
#if defined(_M_IX86)
    static_assert(sizeof(NametagIcon) == 0x18, "s_icons' node is 0x2C = 12 + 4 + 24 + 4");
    static_assert(sizeof(NametagBar) == 0x3C, "s_bars' node is 0x50 = 12 + 4 + 60 + 4; "
                                              "createBar copies 15 dwords");
    static_assert(offsetof(NameTagRenderer, m_renderPath) == 0x0004, "+0x04");
    static_assert(offsetof(NameTagRenderer, m_freeSlots) == 0x0008, "+0x08");
#endif
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // GEOM_NAMETAGRENDERER_H
