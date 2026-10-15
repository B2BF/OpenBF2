// HUD node that displays weapon targets and their lock state.
#ifndef MENU_BF2OBJECTMARKERNODE_H
#define MENU_BF2OBJECTMARKERNODE_H
#include "../External/BFMeme/Node.h"
#include "../Math/Vec3.h"
#if !DEDICATED
#include <cstdint>
#include <list>
#include <map>
#include <string>
namespace dice { namespace hfe { namespace world { class IObject; } } }
namespace dice { namespace meme {
class Bf2TransformNode;

// Marker target and geometry payloads. [INFERRED] Record and field names.
struct MarkerTarget                     // 8 bytes: one per marker, parallel to MarkerQuad
{
    unsigned int objectId;              // +00 the object's IObject +0x10 id
    hfe::world::IObject* object;        // +04
};
// One textured, rotated quad (7CC490 draws it). The same 44-byte shape as the node's PE+78
// block, which holds the range line (7CC3D0).
struct MarkerQuad
{
    // Inlined in both 7CD230 (the PE+78 block) and 7CD5B0: position and size zero, the four
    // +08 words 1.0f. Rotation, texture and the +28 word are not written by it.
    MarkerQuad() : x(0.0f), y(0.0f), width(0.0f), height(0.0f)
    {
        field08[0] = field08[1] = field08[2] = field08[3] = 1.0f;
    }
    float x, y;                         // +00/+04 centre, screen-centre-relative (worldToScreen)
    float field08[4];                   // +08..+14 [UNRESOLVED] never read in this image:
                                        //   {1,1,1,1} as constructed, {0,1,0,1} per marker
    float width, height;                // +18/+1C
    float rotation;                     // +20 radians; 0 for every marker
    TextureHandle texture;              // +24 handle; 0 means nothing is drawn
    std::uint32_t field28;              // +28 never written and never read
};
#if defined(_M_IX86)
static_assert(sizeof(MarkerTarget) == 8, "retail MarkerTarget list payload");
static_assert(sizeof(MarkerQuad) == 44, "retail MarkerQuad list payload");
#endif

class Bf2ObjectMarkerNode : public Node {
public:
    // Defaults [INFERRED] from factory7CD7F0, which supplies two nulls and5.
    Bf2ObjectMarkerNode(System* system = nullptr, const char* name = nullptr,
                        int objects = 5);
    ~Bf2ObjectMarkerNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;                           // 5A5E20, true
    void paintSelf(System*, const NodeState*, const Rect* bounds,
                   const Rect* clip, RenderContext*) override;
    static TypeDescriptor* getClassType();               // 7CC620, metadata9380BC

    // [CHOSEN, not recovered] Native event-routing accessors.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace them when the corresponding Node virtual methods are typed and validated.
    bool clearShowFlagsForHud();                          // 7CBFE0, +C0
    bool setObjectCountForHud(int objects);               // 7CDFC0, +C8
    bool setLockTextOffsetForHud(int x, int y);           // 7CD1F0, +CC
    bool setLockTextForHud(bool enabled, std::string prefix);
    bool addLockTextNodeForHud(Bf2TransformNode* node);    // 7CD000, +D4
    bool setTextureForHud(int index, TextureHandle texture); // 7CD1C0, +D8
    bool setTextureSizeForHud(int index, float width, float height); // 7CD210, +DC
    bool setLockOnTypeForHud(int type);                   // 7CDDD0, +E0
    bool setWeaponForHud(int weapon);                     // 7CD1E0, +E4

    // The twenty information-layer slots the +D0 accessor registers. Exposed for
    // the fixture; retail reaches them only through HudInformationLayer.
    enum { MarkerSlotCount = 20 };

protected:
    friend struct Bf2ObjectMarkerNodeFixture;
    int m_objects;                       // PE+08, ctor argument, default5: the marker cap
    // [INFERRED] Marker range, refreshed from weapon targeting range or player view distance.
    float m_range;                       // PE+0C, zeroed
    TextureHandle m_textures[4];         // PE+10, index<4 only; [3] is the range line's
    hfe::Vec2 m_textureSizes[4];         // PE+20, every entry starts (32,32)
    hfe::Vec2 m_lockTextOffset;          // PE+40, starts (-3, 0)
    std::uint32_t m_field048;            // PE+48 [UNRESOLVED], zeroed
    // PE+4C / PE+50: the IObject +0x10 id of the weapon's current target while it is not
    // locked (+4C) and once it is locked (+50); -1 when there is none. Reset every frame.
    int m_field04c, m_field050;          // PE+4C/+50, both start -1
    int m_lockOnType;                    // PE+54, starts2; -1 = only the weapon's own target
    int m_weapon;                        // PE+58, starts0: the getActiveWeapon index
    bool m_field05c;                     // PE+5C, starts true [UNRESOLVED], unread by paint
    bool m_lockedShow;                   // PE+5D, registered as "<name>LockedShow"
    bool m_lockTextEnabled;              // PE+5E
    bool m_field05f;                     // PE+5F, starts false: the weapon has a target this frame
    bool m_field060;                     // PE+60, starts true: the distance labels' master switch
    // Last computed lock state: zero none, one target, two locked.
    std::uint32_t m_field064;            // PE+64, zeroed
    // PE+68: the weapon's own current target, keyed by its IObject id (7CCFD0 insert_unique;
    // the key compare is `setb`, i.e. unsigned - the same map type as GameLogic's registry).
    std::map<unsigned int, hfe::world::IObject*> m_weaponTargets; // PE+68 (12 B, node0x18); native+64 (8 B)
    char* m_name;                        // PE+74, owned narrow clone
    // Range-line quad. Its rotation and trailing word remain uninitialized by construction.
    MarkerQuad m_rangeLine;              // PE+78
    std::list<MarkerTarget> m_markerTargets; // PE+A4 (12 B, node0x10)
    bool m_show[MarkerSlotCount];        // PE+B0, "<prefix><i>Show"
    const char* m_names[MarkerSlotCount];// PE+C4, "<prefix><i>String"
    int m_positions[MarkerSlotCount][2]; // PE+114, "<prefix><i>PosX"/"PosY"
    std::list<MarkerQuad> m_markerQuads; // PE+1B4 (12 B, node0x34), nearest first
    std::list<Bf2TransformNode*> m_lockTextNodes; // PE+1C0 (12 B, node0x0C)

private:
    // Marker collection and drawing helpers. [INFERRED] Names.
    void clearFrameMarkers();
    void addWeaponTarget(hfe::world::IObject* target);
    void collectMarkers(int objects, System* system, const Rect* bounds);
    bool isObstructedByTerrain(hfe::world::IObject* vehicle, hfe::world::IObject* object);
    bool isObstructedByObject(hfe::world::IObject* vehicle, hfe::world::IObject* object);
    bool isWithinRadius(hfe::Vec3 from, hfe::Vec3 to, float radius);
    int markerIndexForDistance(hfe::world::IObject* object);
    void addMarker(unsigned int objectId, hfe::world::IObject* object, int index,
                   System* system, const Rect* bounds);
    bool isInsideBounds(hfe::Vec3 screen, const Rect* bounds);
    int selectMarkerTexture(unsigned int objectId, hfe::world::IObject* object,
                            hfe::Vec3 screen, const Rect* bounds);
    void updateRangeLine(hfe::Vec3 screen, const Rect* bounds);
    void updateMarkerLabels(const Rect* bounds);
    void drawMarkers(System* system, const Rect* bounds, RenderContext* context);
    void drawMarker(float x, float y, float rotation, float width, float height, TextureHandle texture,
                    System* system, const Rect* clip, RenderContext* context);
};
// Retail is 0x1CC; the four containers lose four bytes each on this toolchain.
#if defined(_M_IX86)
static_assert(sizeof(Bf2ObjectMarkerNode) == 444, "native Bf2ObjectMarkerNode storage");
#endif

// an eight-byte zeroed .data block whose address is the default value
// of every m_names entry. The destructor compares against it before releasing a
// slot, so it is the class's "not mine" sentinel, not the shared rdata literal
// at 87FB54. Name [INFERRED].
extern const char* const defaultMarkerName;
} }
#endif
#endif
