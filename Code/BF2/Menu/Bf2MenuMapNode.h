// HUD map node with icon collection, filtering and pointer interaction.
#ifndef MENU_BF2MENUMAPNODE_H
#define MENU_BF2MENUMAPNODE_H
#include "Bf2TransformNode.h"
#include "../Game/IEventListener.h"
#include "../Game/GameLogic/SquadManager/ISquadInfo.h"
#include "../External/BFMeme/Quad.h"
#include <bitset>
#include <list>
#include <map>
#include <string>
#include <vector>
#if !DEDICATED
namespace dice { namespace hfe {
class HudInformationLayer;
struct SquadOrder;
namespace hud { class BF2HudManager; class HudKitSelection; class HudCommander; class HudVehicleInformation; class Bf2HudBuilder; }
namespace world { class IControlPoint; class IObject; class IPlayer; }
namespace geom { class IMemeTexture; }
}
// [CHOSEN] Free-function form of the vehicle-zoom helper; friendship provides access to map state.
namespace meme { class Bf2MenuMapNode; }
namespace hfe { namespace hud {
void dispatchHudMapApplyVehicleZoom(::dice::meme::Bf2MenuMapNode*);
} }
namespace meme {
// Native map drawing records. Names are [INFERRED]; unresolved fields retain their layout placeholders.
struct MapIconRecord
{
    hfe::Vec2 position;       // +00
    Color color;              // +08
    hfe::Vec2 size;           // +18
    float rotation;           // +20; retail default construction leaves this untouched
    TextureHandle texture;    // +24
    bool clampToVisibleMap;   // +28; 776AF0 applies the 775800 viewport inset clamp when true
    std::uint8_t padding[3];  // +29; retail copies no padding bytes

    // 773590 and 7741E0.  The omitted scalar initializers are intentional:
    // retail leaves rotation/+28/padding untouched in the default constructor,
    // and its copy constructor copies the typed fields plus exactly byte +28.
    MapIconRecord()
        : position{0.0f, 0.0f}, color{1.0f, 1.0f, 1.0f, 1.0f},
          size{0.0f, 0.0f}, texture(0) {}
    MapIconRecord(const MapIconRecord& other)
        : position(other.position), color(other.color), size(other.size),
          rotation(other.rotation), texture(other.texture),
          clampToVisibleMap(other.clampToVisibleMap) {}
};
#if defined(_M_IX86)
static_assert(sizeof(MapIconRecord) == 0x2C, "retail map icon record");
#endif
// Order-line record ending in its drawable icon. [INFERRED] Field names.
struct MapLineRecord
{
    int kind;                    // +00 the order's orderId; 4 for a follow-on segment
    int networkIdA;              // +04 start networkable (the squad leader's vehicle), -1 = none
    int networkIdB;              // +08 end networkable (the order's target), -1 = none
    int squad;                   // +0C squad number + 1 on a commander's map, else -1 (map) / 0 (mini)
    bool field10;                // +10 [UNRESOLVED] both builders set it; nothing here reads it
    bool valid;                  // +11 774240 clears it for a degenerate end - and +130 then sets it
                                 //     again for orders (kinds 0/1), clearing it only for requests
    std::uint8_t padding[2];
    hfe::Vec2 a;                 // +14 start, map space
    hfe::Vec2 b;                 // +1C end, map space
    MapIconRecord line;          // +24 the connecting line's icon (map_Line)
};
#if defined(_M_IX86)
static_assert(sizeof(MapLineRecord) == 0x50, "retail order-line record");
#endif
struct MapTimedMarker
{
    TextureHandle texture;      // +00
    int networkId;              // +04
    hfe::Vec2 size;             // +08
    int kind;                   // +10
    float remaining;            // +14
    float field18;              // +18
    float initialDuration;      // +1C
    float field20;              // +20
    float field24;              // +24
    bool negativeDuration;      // +28
    std::uint8_t padding[3];    // +29
    hfe::Vec2 position;         // +2C

    MapTimedMarker() : size{0.0f, 0.0f}, position{0.0f, 0.0f} {}
};
#if defined(_M_IX86)
static_assert(sizeof(MapTimedMarker) == 0x34, "retail timed map marker");
#endif
struct MapField09ec { std::uint32_t words[2]; };     // 8-byte list value;7771D0 allocates the16-byte node
struct MapVehicleDrop {
    bool active;
    TextureHandle texture;
    int id;
    hfe::Vec3 position;     // 77ACE0 writes bool,texture,id,x,y,z;24 bytes
};

class Bf2MenuMapNode : public Bf2TransformNode, public hfe::IEventListener {
public:
    // 780180 takes System*, the name and four unused stack words (the base is
    // constructed with a null name and zero geometry; the name is set afterwards
    // through setName). Derived micromap7CF910 proves the final two are signed
    // width/height; the first two remain untyped and are ignored here as shipped.
    Bf2MenuMapNode(System* system, const char* name, int unused0, int unused1, int unused2, int unused3);
    Bf2MenuMapNode(const Bf2MenuMapNode&) = delete;      // [CHOSEN] no copy
    ~Bf2MenuMapNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool updateSelf(System*, const NodeState*, const Rect*, float) override;
    void paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override;
    bool onEvent1(System*, const Rect*, const NodeState*, int, int, int) override;
    void handleEvent(hfe::EventCategory,unsigned int,hfe::EventNode*,void*) override;
    static TypeDescriptor* getClassType();

    void resetSatelliteReload();
    void resetUAVReload();
    int cycleZoom();
    void prepareOrder(int type);
    void clearOrder();                                  // 772C00, clears pointer without deleting
    // Sets expanded-map mode and adjusts the view center and zoom.
    void setExpanded(bool expanded);
    bool isExpandedMap() { return m_expandedMap; }        // 7728F0, complete MOV AL,[ECX+68E];RET
    int getCloseToControlPointTeam() const { return m_fielda70; } // [INFERRED] name; 772D70 MOV EAX,[ECX+A70];RET
private:
    friend class hfe::HudInformationLayer;
    friend class hfe::hud::BF2HudManager;
    friend class hfe::hud::HudKitSelection;
    friend class hfe::hud::HudCommander;
    friend class hfe::hud::HudVehicleInformation; // 78B1E0 reads772870 / overhead zoom flag
    friend struct Bf2MenuMapNodeFixture;
    friend void ::dice::hfe::hud::dispatchHudMapApplyVehicleZoom(::dice::meme::Bf2MenuMapNode*);
    // The [CHOSEN] +F8 forwarder at the foot of this header reaches the private virtual.
    friend void dispatchMapSetSelectionWord(Bf2MenuMapNode*, int, unsigned int);
    friend class Bf2MiniMenuMapNode;
    void setMinimapTexture(std::string);
    // ECHud 10's whole payload: three neighbours cleared, then the flag.
    // [INFERRED] name, from the information-layer name the arm looks up
    // ("MapStatic") and from HudInformationAdapter::setMapStatic (785BA0), which
    // is what posts the event.
    void setMapStatic(bool isStatic);

public:
    // The nine `Availability::BuilderMap` console rows (33..41). Their invokers
    // reach hudManager->getBuilder()->m_field008 and act on the map directly -
    // no Bf2HudBuilder method is involved at all. Two of the nine survive as
    // out-of-line bodies (75F319 and 75F1BF); MSVC inlined the other seven into
    // their invokers, so declaring all nine here is the natural reading of the
    // shipped source rather than a chosen shape. [INFERRED] names.
    bool setMiniPosForHud(float x, float y);              // 75F319, row 34
    bool setMiniSizeForHud(float x, float y);             // 75F1BF, row 33
    bool setMaxiPosForHud(float x, float y);              // inlined at 75F555, row 36
    bool setMaxiSizeForHud(float x, float y);             // inlined at 75F423, row 35
    bool setCommanderPosForHud(float x, float y);         // inlined at 75F7B9, row 38
    bool setCommanderSizeForHud(float x, float y);        // inlined at 75F687, row 37
    bool setZoomIconsForHud(bool enabled);                // inlined at 75F8CB, row 39
    void setControlPointFontForHud(std::string font);      // 75F9D0, row 40
    void setControlPointFontColorForHud(float red, float green,
                                        float blue, float alpha); // inlined at 75FC39, row 41
    // Called only by setMiniPosForHud. While the commander view is not
    // shown it just records the offset; once it is, it re-centres the view.
    bool mapRecenterForCommander(float x, float y);

    // Console-facing kit and vehicle filter controls. The zoom binding cycles zoom levels.
    void setPaintKitForHud(int kit);                      // 7764F0, row 149
    void setPaintAllKitsForHud();                         // 7778E0, row 150
    void setPaintVehicleForHud(int type);                 // 776530, row 151
    void setPaintAllVehiclesForHud();                     // 777930, row 152
    void toggleShowKitsForHud();                          // inlined at 76BE1D, row 156
private:
    void setTeamFlagTexture(std::string, int team);
    void setControlPointTexture(std::string, int team);
    void setControlPointBaseTexture(std::string, int team);
    void releaseMinimapTexture();
    void setAirstrikeMarker(hfe::Vec3, float duration);
    void setUAVMarker(hfe::Vec3, float duration);
    // 775340 (86 B), ECHud 83's whole payload - the supply-drop marker, the airstrike setter's
    // exact twin over the +B10..+B28 block. [INFERRED] name. (D1).
    void setSuppliesMarker(hfe::Vec3, float duration);
    // Three one-line setters over the +6FC..+71B float block, each with exactly
    // one caller in the image - Bf2MiniMenuMapNode's +DC override. MSVC left all
    // three out of line, so they are real source functions. [INFERRED] names: the
    // members' roles are still [UNRESOLVED], so the offsets name them.
    void setField70c(float value);                        // 7728B0, m_field708[1]
    void setField718(float value, bool alsoField700);
    void setField704(float value);
    void clearVehicleDrops();
    // 77ACE0 (117 B), ECHud 74's map half: one MapVehicleDrop appended to +B2C, its icon
    // re-loaded through the System on every call. [INFERRED] name. (D1).
    void addVehicleDrop(int id, hfe::Vec3 position);
    void removeTimedMarker(int networkId);
    // Returns the pointer position in map coordinates.
    hfe::Vec2 getPointerMapPosition(System*);
    // Maps pointer coordinates and selects a spawn dot. [INFERRED] Helper names.
    // Coordinate conversion crosses the supplied scale components.
    hfe::Vec2 mapPointerToMapCoordinates(hfe::Vec2 pointer, hfe::Vec2 scale);
    bool pickSpawnGroup(float scale, hfe::Vec2 position, float maxDistance);
    // [INFERRED] Picks the nearest friendly squad request and updates the commander position.
    bool pickSquadRequest(float scale, hfe::Vec2 position, float maxDistance);
    // the nearest live, standing enemy in a vehicle; answers that vehicle's
    //   networkable id (a short - only AX is written, see the body). The commander's release (3.6).
    unsigned short pickEnemyVehicle(float scale, hfe::Vec2 position, float maxDistance);
    // the nearest player in a vehicle; answers the player id or -1. `this` is not
    //   read (ecx is loaded at the call and never used). The press (3.8b).
    int pickPlayer(float scale, float x, float y, float maxDistance);
    void setPlayerPosition(hfe::Vec2, float heading);
    // the heading half of setPlayerPosition, its only caller. [INFERRED]
    // name. Stores the target heading and latches the rotation direction so the needle takes the
    // short way across the +/-pi wrap. See the body.
    void setPlayerHeading(float heading);
    // Three per-frame workers of update(+E0), all [INFERRED] names,
    void updateBlinkPulse(float delta);
    void updateTimedMarkers(float delta);
    void updateSatelliteScan(float delta);
    void updateUav(float delta);
    void retireStaleVehicleDrops();
    // [CHOSEN] factoring of the three-way zoom-row select update() spells out at each of its three
    // sites. Keeping it a function preserves the three isCommanderShown() calls exactly.
    float zoomExponentTarget();                           // inside 77C330
    int applyZoom(int level, bool remember);              // 776670; three virtual-dispatch frontiers below
    hfe::Vec2 getZoomScale(bool selectedLevel);           // 773900 [INFERRED] name/Vec2 return spelling
    hfe::Vec2 getMapCenterOffset();                       // 773A20 [INFERRED] name/Vec2 return spelling
    void updateCommanderZoom();
    bool snapOrderToControlPoint(float scale, hfe::Vec2 position, float distance);
    bool updateOrder(hfe::SquadOrder*& order, int type, float scale,
                     hfe::Vec2 position, unsigned short target);
    // copies six bitsets into the live filter states. [INFERRED] name.
    void applyFilterSet(const std::bitset<43>* set);
    // Handles HUD events through virtual dispatch so minimap behavior can override the map implementation.
    virtual void handleHudEvent(unsigned int id, hfe::EventNode* node, void* data);
    // +DC, the HUD mode change. virtual for the same reason handleHudEvent is:
    // 782A2E and 7CF7C2 make the identical indirect call through this slot, and
    // slot 55 is one of the twenty Bf2MiniMenuMapNode replaces - the map runs a
    // 29-way switch (777DC0) and the mini-map a two-mode test (7CE3D0), sharing
    // nothing. [INFERRED] name, from its only argument.
    virtual void setHudMode(int mode);                    // +DC 777DC0
    // Selects filters for the player's role or restores the default icon mask. [INFERRED] Helper names.
    void applyPlayerRoleFilters(hfe::world::IPlayer* player);
    void resetMapFilterBits();
    // Recenters the map on the local player; specialized maps may override this behavior.
    virtual void recenterOnPlayer();
    virtual void setViewCenter(hfe::Vec2 position);       // +11C 7737C0
    virtual void updateViewBounds();                      // +120 7769C0 [FRONTIER]
    virtual void setZoom(int level, bool remember);       // +D8  777980
    // The map painter reaches these actual suffix slots. Names and the ignored
    // `void` returns are [INFERRED]; argument shapes and virtual identities are
    // instruction-backed. They are declared as ordinary methods so derived map
    // nodes dispatch through C++, never through raw table offsets.
    virtual bool isMapFilterEnabled(unsigned int type);
    virtual bool shouldScaleMapIcon(unsigned int type);
    virtual bool shouldUseSecondaryIconTexture();         // +10C 772C60
    // Stores a request type for a squad. A negative squad clears all ten entries.
    // Nonnegative squad indices must be below ten; the implementation does not check the upper bound.
    virtual void setSquadRequestType(int squad, int type);         // +F8 772CA0
    // +F4, 77A8D0 (29 B): the reader. A negative squad reads [0]; a squad past 10 is clamped to 10 -
    // which is still one past the array (m_field068, +68), as shipped. The mini-map inherits it.
    virtual int getSquadRequestType(int squad);                    // +F4 77A8D0
    // +110, 7730B0 (61 B): the icon a support request (ECHud 46..50) draws with - 47 ammo, 48
    // repair, 49/50 revive, 46 and anything else medic (the four constructor textures at +9A8).
    virtual TextureHandle getSupportRequestTexture(int eventId);
    // Returns the cached squad-number icon, loading it on demand. Requires a valid squad index. [INFERRED] Name.
    virtual TextureHandle getSquadNumberIcon(int squad);
    // +114, 782220 (602 B): one order's line and marker. Always answers true.
    virtual bool addOrderIcons(hfe::SquadOrder* order, int squad, int leaderNetworkId,
                               int kind, const Rect* bounds);
    // +130, 77D560 (608 B): textures, sizes and colours by kind (0 squad order, 1 commander
    // order, 2 request); a request also gets a pulse ring and, for a commander, the squad number.
    virtual void styleOrderIcons(int kind, MapLineRecord* line, MapIconRecord* marker,
                                 bool active);
    // Updates map presentation and expires timed markers over delta seconds.
    virtual void update(float delta);                     // +E0  77C330, 4,379 B
    virtual TextureHandle timedMarkerTexture(unsigned int id); // +E4 7779B0
    virtual hfe::Vec2 timedMarkerSize(unsigned int id);    // +E8 7779E0
    virtual bool populateNetworkIcon(int id, MapIconRecord* icon,
                                     bool asPlayer);       // +EC 775720
    virtual void colorMapIcon(hfe::world::IPlayer* player,
                              MapIconRecord* icon, int type,
                              bool compareTeam);           // +F0 7788B0
    virtual TextureHandle getSmallSquadIcon(int squadId);  // +FC 772ED0
    virtual void paintMapIcon(hfe::Vec2 position, float rotation,
                              hfe::Vec2 size, std::uintptr_t texture, System* system,
                              const Rect* adjusted, const Rect* bounds,
                              const Color* color, RenderContext* context);
    virtual void paintMapIcons(System* system, const Rect* adjusted,
                               const Rect* bounds, RenderContext* context);
    virtual void collectLocalPlayerIcon();                // +12C 77F4D0
    virtual void populateVehicleMapIcon(hfe::world::IObject* object,
                                        MapIconRecord* icon,
                                        bool* unused,
                                        bool spotted);
    virtual void populatePlayerMapIcon(hfe::world::IPlayer* player,
                                       MapIconRecord* icon,
                                       bool* headingOut,
                                       bool* rotateOut,
                                       bool showKit,
                                       bool specialRoleIcons,
                                       bool spotted);
    virtual void populateControlPointMapIcon(MapIconRecord* icon,
                                              int team,
                                              bool useTeamTexture,
                                              std::string mapName,
                                              bool notCapturable,
                                              bool cachePresent,
                                              bool localCaptureActive); // +13C 7755A0
    virtual TextureHandle getLeaderMapIcon(int squadId, int sizeIndex);
    // 772810, the arm +D8 takes when the map is not expanded. [INFERRED] name.
    void setCommanderZoom(int level, bool remember);
    // Retail indexes `[this + 0x69C + type*4]` for a vehicle type in [0, 40) -
    // forty words - while the per-vehicle saved-zoom table at +69C is only ten.
    // A type above nine walks straight through m_savedZoom, m_zoomLevel,
    // m_zoomExponents and the float block after them. The bound is retail's only
    // guard and it is wider than the table; preserved exactly, not tightened.
    // The native member run from m_field69c to m_controlPointSnapDistance is the
    // same 160 bytes as the retail one, so the walk lands on the same fields.
    int& savedZoomSlot(int type);
    // 7768E0, updateViewBounds's only helper and its only caller in the image.
    // [INFERRED] name. The two components are not an (x, y) pair: `.x` comes
    // out positive and `.y` negative, and updateViewBounds uses them
    // asymmetrically - see the body.
    hfe::Vec2 mapViewExtent();
    // Three [CHOSEN] source-level extractions of blocks MSVC tail-merged inside
    // the adapter-alpha pair modes 0 and 2 share (777EB0 / 7780BE), the
    // overhead-camera reset five modes end with (777F2A / 77803F / 778365) and the
    // tail modes 17 and 18 share (7782F5). They are not separate retail functions -
    // they are how the duplicated source reads.
    void applyMenuMapAlphas();
    void clearOverheadCamera();
    void applySquadViewTail(int mode);
    void handleCoreEvent(unsigned int id, hfe::EventNode* node, void* data);

    // paintSelf's direct member helpers. Names are [INFERRED], while the PE
    // addresses, call order and complete argument shapes are recovered.
    void updateFilterActiveFlags();
    void updatePaintCenter();
    void drawMapTexture(TextureHandle texture, const Rect* adjusted, const Rect* bounds,
                        System* system, RenderContext* context);
    void drawMapTint(TextureHandle texture, const Color* color, const Rect* adjusted,
                     const Rect* bounds, System* system, RenderContext* context);
    // Updates and draws supply lines. [INFERRED] Helper names.
    void updateSupplyLine(System* system);
    void drawSupplyLine(const Rect* adjusted, const Rect* bounds,
                        System* system, RenderContext* context);
    void loadSupplyLineTextures(System* system);
    void advanceSupplyLineGroup();
    bool updateCombatAreaTexture(bool measureOnly);
    void drawCombatAreaTexture(TextureHandle texture, const Rect* adjusted,
                               const Rect* bounds, System* system,
                               RenderContext* context);
    void drawMapBorder(const Rect* adjusted, const Rect* bounds,
                       System* system, RenderContext* context);
    void clearMapIcons();
    void collectSupplyLineIcons(const Rect* bounds);
    float getZoomExponent(int level);
    void addMapIcon(unsigned int type, MapIconRecord& icon);
    // [CHOSEN] source-level extraction of 7751D0 -> 774240 for its sole
    // shipped call shape: both network ids are -1, so the supplied world
    // positions are converted directly and the unreachable id-resolution
    // branch is not fabricated here.
    void makeWorldSupplyLineRecord(MapIconRecord& output, const Rect* bounds,
                                   const hfe::Vec3& source,
                                   const hfe::Vec3& target);
    void projectWorldPosition(hfe::Vec3 position, MapIconRecord* output);
    void projectWorldPosition(hfe::Vec3 position, hfe::Vec2* output);
    void screenCoordinatesToMap(const hfe::Vec2& input,
                                hfe::Vec2& output);
    void clampProjectedPosition(hfe::Vec2 unused,
                                MapIconRecord* icon);
    void projectObject(hfe::world::IObject* object,
                       MapIconRecord* icon, bool heading);
    unsigned int remapMapIconType(unsigned int type, TextureHandle texture,
                                  bool timed, float* rotation);
    void addTimedMarker(int networkId, hfe::Vec2 position,
                        MapIconRecord icon, float duration,
                        float field20, float field24,
                        float field18, int kind);
    bool resolveTimedMarker(int networkId, hfe::world::IPlayer* player,
                            MapIconRecord* icon, bool* rotate,
                            bool* secondary, int* type);
    void updateSatelliteTarget(hfe::world::IObject* object,
                               int networkId, TextureHandle texture);
    void updateUavTarget(hfe::world::IObject* object,
                         int networkId, TextureHandle texture);
    // [INFERRED] name; complete PE body is map<unsigned,IObject*>::erase(key).
    void removeUavObject(unsigned int networkId);
    void collectIconFamily783F90();
    void collectControlPointIcons(bool supplyLine);
    // Collects role-visible squad order icons within bounds.
    void collectSquadOrderIcons(const Rect* bounds);
    void collectCommanderOrderIcons(const Rect* bounds);
    void collectRequestIcons(const Rect* bounds);
    // The order helpers. [INFERRED] names.
    int getSquadLeaderNetworkId(int squad);               // the leader's vehicle netId, or -1
    hfe::Vec2 networkIdToMap(int networkId, float scale); // 773AF0 - (x, -z) * scale, or (-1, -1)
    void completeLineRecord(float scale, MapLineRecord* line);
    void collectIconFamily783440();
    // Collects friendly squad-leader request icons using the squad-request texture.
    void collectSquadRequestIcons();
    void addSquadRequestIcon(unsigned int type, hfe::world::IPlayer* player,
                             const MapIconRecord* icon);
    void collectIconFamily77E500();
    // renamed from collectIconFamily77F6E0 once the body was read: it is the spawn-point
    // icon family (map filter 31), driven from dice::hfe::spawnManager.
    void collectSpawnGroupIcons();
    void collectIconFamily7820E0();
    void collectIconFamily77FC30();
    void collectAirstrikeIcon();                         // 77F8E0 [FRONTIER]
    void collectSuppliesIcon();                          // 77FA00 [FRONTIER]
    void collectVehicleDropIcon(const MapVehicleDrop*);  // 77FB20 [FRONTIER]
    void drawSatellite(System*, const Rect*, RenderContext*);
    void drawMapShadow(TextureHandle texture, const Rect*, System*, RenderContext*);
    void drawMapFrame(TextureHandle texture, const Rect*, System*, RenderContext*);
    void drawControlPointLabels(System*, const NodeState*, const Rect*,
                                const Rect*, RenderContext*);
    hfe::geom::IMemeTexture* combatAreaTexture() const;

    // Per-squad request types. Unidentified scalar groups retain native scalar storage, while containers use native STL.
    int m_squadRequestTypes[10];                          // +40..67, constructor 0
    std::uint32_t m_field068;                             // +68, constructor0
    bool m_field06c;                                     // +6C, constructor false
    std::uint8_t m_field06d[3];                           // +6D..6F untouched
    std::uint32_t m_field070[3];                          // +70..7B untouched
    bool m_field07c, m_field07d, m_field07e, m_field07f;  // +7C..7F, constructor false
    bool m_playerTeamPresent[2];                         // +80/+81, reset then set for alive teams 1/2 by 783440
    std::uint8_t m_field082[2];                          // +82/+83 untouched
    float m_worldToMapScale;                             // +84 inverse world X,773760/777D20
    float m_field088;                                    // +88 inverse world Z
    // Per-team flags indicate that neither spawn groups nor live players remain.
    std::uint32_t m_field08c[4];                          // +8C..9B untouched
    bool m_field09c, m_field09d;                          // +9C/+9D, set per team by 77F6E0
    std::uint8_t m_field09e[2];                           // +9E/+9F untouched
    std::uint32_t m_field0a0[2];                          // +A0..A7 untouched
    std::vector<MapIconRecord> m_icons[43];               // +A8..357, retail vectors16 bytes
    System* m_system;                                    // +358 borrowed
    std::uint32_t m_field35c;                             // +35C untouched
    hfe::SquadOrder* m_order;                            // +360,776DE0 allocates48-byte order
    bool m_field364;                                     // +364, constructor false
    std::uint8_t m_field365[3];
    int m_orderPointCount;
    bool m_field36c;                                     // +36C, constructor true
    bool m_mapFilterActive[32];                           // +36D..38C, "%sMapFilter%iActive"0..31 [INFERRED] name
    bool m_showKitsOnMap;                                 // +38D, "%sShowKitsOnMap"
    std::uint8_t m_field38e[2];                           // untouched
    std::bitset<43> m_filterStates[6];                    // +390..3BF
    // [CHOSEN] Packs the original kit and vehicle bitsets into one native bitset.
    // Kit bits occupy 0..7 and vehicle bits 32..38; accessors must preserve those separate bounds.
    std::bitset<39> m_field3c0;                           // +3C0/+3C4, constructor 7F:000000FF
    std::bitset<43> m_filterSets[7][6];                    // +3C8..517
    hfe::Vec2 m_iconScales[43];                           // +518..66F
    float m_field670;                                    // +670, constructor0.7 (icon scale seed)
    int m_field674;                                      // +674, constructor1
    int m_field678;                                      // +678, constructor8
    float m_field67c, m_field680;                         // +67C/+680, constructor0.55/0.1
    bool m_field684;                                     // +684, constructor false
    std::uint8_t m_field685[7];                           // +685..68B untouched
    bool m_field68c, m_field68d, m_expandedMap;            // +68C..68E,7728F0 exposes68E; 68D untouched
    std::uint8_t m_field68f;                              // untouched
    bool m_field690;                                     // +690, constructor false
    bool m_commanderMode, m_squadMode;                    // +691/+692, constructor false
    std::uint8_t m_field693;                              // untouched
    float m_field694, m_currentZoomExponent;
    int m_field69c[10];                                  // +69C..6C3: 1,1,0,1,0,1,1,1,1,1
    int m_savedZoom, m_savedCommanderZoom, m_savedSquadZoom; // +6C4/+6C8/+6CC =0
    int m_zoomLevel;                                     // +6D0 =1, signed index0..2
    int m_zoomDisplay;                                   // +6D4 =0, "%sZoomDisplay"
    float m_zoomExponents[3][3];                         // +6D8..6FB: mini/expanded/commander
    float m_field6fc;                                    // =1
    float m_field700, m_field704;                         // =1,1
    float m_field708[4];                                 // +708..717: 1,1,1,0.8
    float m_field718;                                    // =1
    // The float at +724 advances the settling wobble; adjacent words remain untouched.
    std::uint32_t m_field71c[2];                          // +71C/+720 untouched
    float m_wobbleTime;                                   // +724, the settle-wobble clock
    std::uint32_t m_field728;                             // +728 untouched
    float m_field72c, m_field730, m_field734;             // 0.02, 0.0025, 0.01
    float m_controlPointSnapDistance;                    // +738 squared-distance threshold, 0.0004
    // +73C is a byte, not a word: setMapStatic (7728C0) writes only its low
    // byte, so its three neighbours keep the retail (uninitialized) content - the
    // same rule the constructor's byte stores already follow here. Nothing writes
    // any of the four before ECHud 10 arrives.
    bool m_mapStatic;                                     // +73C, 7728C0
    std::uint8_t m_field73d[3];                           // +73D..73F untouched
    // +740/+744 are the mini-map's view offset: mapRecenterForCommander (773630)
    // stores the requested position there whenever the commander view is not up.
    hfe::Vec2 m_miniViewOffset;                           // +740/+744
    hfe::Vec2 m_field748;                                 // +748/+74C source view center
    hfe::Vec2 m_field750;                                 // +750/+754 interpolated paint center
    float m_field758;                                    // =1
    // Current animated map heading in radians.
    float m_field75c;                                     // +75C, constructor0 - current heading
    float m_delayedMapAngle;                             // +760, "%sDelayedMapAngle"; [INFERRED] name from the variable
    // Rendered heading followed by the settling-wobble offset.
    float m_field764[2];                                  // +764/+768, constructor0
    float m_field76c;                                    // +76C rendered map angle
    // Target map heading; the direction field selects the interpolation direction.
    float m_field770;                                     // +770, constructor0 - target heading
    bool m_field774, m_field775, m_field776, m_field777;  // false,true,true,false
    hfe::Vec2 m_field778;                                // +778, constructor copies m_field798
    hfe::Vec2 m_field780;                                // +780, constructor copies m_field788
    hfe::Vec2 m_field788;                                // (197,-300); `setMiniPos`
    hfe::Vec2 m_field790;                                // (-200,-256)
    hfe::Vec2 m_field798;                                // (194,194); `setMiniSize`
    hfe::Vec2 m_field7a0;                                // (194,-297)
    hfe::Vec2 m_field7a8;                                // (241,20); `setCommanderPos`
    hfe::Vec2 m_field7b0;                                // (558,558); `setCommanderSize`
    // not a texture size: `setMaxiSize` writes it. The old name predates the
    // console rows and is kept only so existing callers still compile.
    hfe::Vec2 m_textureSize;                              // +7B8/+7BC, (194,-297); `setMaxiSize`
    hfe::Vec2 m_field7c0;                                 // +7C0/+7C4, (-200,-256); `setMaxiPos`
    hfe::Vec2 m_centers[3];                               // +7C8..7DF, constructor0
    hfe::Vec2 m_commanderPosition;                        // +7E0/+7E4, (-4000,-4000)
    hfe::Vec2 m_field7e8;                                 // +7E8/+7EC interpolation target
    hfe::Vec2 m_viewCenter;                               // +7F0/+7F4,7737C0; constructor0
    bool m_overheadZoomActive;                           // +7F8, constructor false
    bool m_field7f9;                                     // +7F9, true; `setZoomIcons`
    std::uint8_t m_field7fa[2];                           // untouched
    TextureHandle m_field7fc[10];                         // +7FC..823, small-squad texture cache
    // Cached squad-number texture handles.
    TextureHandle m_squadNumberIcons[10];                 // +824..84B, constructor 0
    TextureHandle m_field84c[20];                         // +84C..89B, sized leader texture cache
    TextureHandle m_field89c;                            // texture mini_Squad_request
    TextureHandle m_field8a0;                            // texture GeneralIcons/empty
    TextureHandle m_defaultMinimapTexture;               // +8A4, texture GeneralIcons/full
    TextureHandle m_field8a8[23];                        // +8A8..903, textures (constructor order)
    TextureHandle m_controlPointTextures[3];             // +904..90C
    TextureHandle m_controlPointBaseTextures[3];         // +910..918
    TextureHandle m_teamFlagTextures[3];                 // +91C..924
    TextureHandle m_field928[4];                         // +928..937, textures
    TextureHandle m_minimapTexture;                      // +938
    TextureHandle m_field93c[26];                        // +93C..9A3, textures and zeros; [25] spawn group
    int m_spawnGroup;                                    // +9A4, signed, direct kit writes
    TextureHandle m_field9a8[13];                        // +9A8..9DB, textures and zeros
    bool m_field9dc;                                     // +9DC, constructor false
    std::uint8_t m_field9dd[3];
    std::uint32_t m_field9e0[3];                          // +9E0..9EB; +9E8 constructor0
    std::list<MapField09ec> m_field9ec;                    // +9EC; opaque element, no construction
    std::vector<MapTimedMarker> m_timedMarkers;           // +9F8..A07
    // Pulse phase and timing parameters.
    float m_fielda08;                                     // +A08 =0, the pulse phase
    float m_fielda0c, m_fielda10;                         // 4.0, 0.18
    float m_fielda14;                                     // +A14 =0, the pulse output
    bool m_fielda18;                                     // constructor false
    std::uint8_t m_fielda19[3];
    std::uint32_t m_fielda1c, m_fielda20;                  // untouched
    float m_fielda24, m_fielda28, m_fielda2c;             // 0.5, 1.0, 12.0
    float m_fielda30;                                     // +A30 =0, a delta-accumulating timer
    std::list<int> m_fielda34;                            // +A34,777A30 compares signed IDs
    std::list<std::string> m_fielda40;                    // +A40,4072F0 destroys string values
    bool m_fielda4c;                                     // constructor false
    std::uint8_t m_fielda4d[3];
    float m_fielda50, m_fielda54, m_fielda58, m_fielda5c; // -0.49, 0.49, -0.49, 0.49
    float m_minCenterY, m_maxCenterY, m_minCenterX, m_maxCenterX; // +A60..A6F, copies of the four above
    int m_fielda70;                                      // =-1, getCloseToControlPointTeam
    bool m_fielda74, m_fielda75, m_fielda76, m_fielda77;  // constructor false
    std::uint8_t m_fielda78[3];                           // untouched
    bool m_fielda7b;                                     // constructor true
    bool m_satelliteReloading, m_satelliteActive;         // +A7C/+A7D
    std::uint8_t m_fielda7e[2];
    float m_fielda80, m_fielda84, m_fielda88;             // +A80/+A84/+A88 satellite scan window
    float m_fielda8c;                                      // +A8C timed satellite fade
    TextureHandle m_fielda90;                            // texture map_SatelliteScanLine
    TextureHandle m_fielda94;                            // texture mini_SatelliteObject
    float m_satelliteScanX, m_satelliteScanY;              // +A98/+A9C
    int m_satelliteLineSize;                             // +AA0 =150
    float m_satelliteReloadRemaining;                    // +AA4
    float m_satelliteReloadFraction, m_fieldaac;          // +AA8 =1.0 ("%sSatelliteTimeUntilReloaded"), +AAC
    hfe::Vec3 m_uavPosition;                             // +AB0..AB8
    float m_fieldabc, m_fieldac0, m_fieldac4;              // 0, 4.0, 0
    float m_uavReloadRemaining;                          // +AC8
    bool m_uavActive;                                    // +ACC
    int m_uavRadius;                                     // +AD0, 100
    TextureHandle m_uavTexture, m_uavIcon;               // +AD4/+AD8: map_UAV, icon_UAV
    float m_uavReloadFraction;                           // +ADC =1.0 ("%sUAVTimeUntilReloaded")
    bool m_uavReloading;                                 // +AE0
    float m_uavActiveTime;                               // +AE4
    bool m_uavState;                                     // +AE8
    float m_fieldaec, m_fieldaf0;                         // 0.5, 0
    hfe::Vec3 m_airstrikePosition;                        // +AF4..AFC
    bool m_airstrikeActive;                              // +B00
    TextureHandle m_airstrikeTexture, m_artilleryIcon;    // +B04/+B08
    float m_fieldb0c, m_fieldb10;                         // +B0C/+B10
    hfe::Vec3 m_suppliesPosition;                        // +B14..B1C
    bool m_suppliesActive;                               // +B20
    TextureHandle m_suppliesTexture;                     // +B24 icon_supplies
    float m_fieldb28;                                     // +B28 =0, supplies duration
    std::list<MapVehicleDrop> m_vehicleDrops;             // +B2C,77ACE0
    std::map<unsigned int, hfe::world::IObject*> m_uavObjects; // +B38, 77A030
    int m_fieldb44, m_fieldb48;                           // =6,6
    Color m_fieldb4c;
    Rect m_fieldb5c;                                      // +B5C..B6F cached adjusted bounds
    bool m_fieldb70;                                     // constructor false; ECCore13 clears it
    std::uint8_t m_fieldb71[3];
    hfe::Vec2 m_mapOffset, m_mapExtent;                    // +B74/+B78,+B7C/+B80: (0,0),(512,512)
    float m_fieldb84;                                    // =20
    int m_selectedRequestSquad;                          // +B88,773B60 scans squads0..9; =-1
    int m_selectedRequestType;                           // +B8C,773B60 scans requests0..3; =0
    hfe::OrderType m_selectedRequestOrderType;             // +B90,773B60 stores2;7AFFE0 setIsAccepted OrderType
    // State used to draw supply-line connections.
    bool m_fieldb94, m_fieldb95;                          // constructor false
    std::uint8_t m_fieldb96[2];
    std::uint32_t m_fieldb98;                             // untouched by the constructor; see above
    std::uint32_t m_fieldb9c, m_fieldba0;                  // constructor0
    // Supply-line texture handles, loaded by stage and initially zero.
    TextureHandle m_supplyLineTextures[20];               // +BA4..BF3, constructor0
    std::list<int> m_controlPointIds;                     // +BF4,77C0A0
    std::list<int> m_fieldc00;                            // +C00,77E720 compares control-point template ID
    std::uint32_t m_fieldc0c[18];                         // +C0C..C53, constructor0
    std::map<unsigned int, hfe::world::IControlPoint*> m_controlPoints; // +C54,77E720/779B70
    // m_fieldc64 stores the font handle loaded through this map's System.
    std::uint32_t m_fieldc60;                             // +C60 untouched
    std::uintptr_t m_fieldc64;                            // +C64 0; uint32_t on x86
    Color m_fieldc68;                                    // (1,1,1,1); `setCPFontColor`
    float m_fieldc78, m_fieldc7c;                         // constructor0
};

// 772230 has no this argument. Name/reference spelling inferred from its
// output writes and caller-owned Vec2 pointer; it reloads settings between axes.
void mapCoordinatesToWorld(hfe::Vec3& output, const hfe::Vec2& position);
// 7720E0 inverse: no ECX read, input Vec3 then output Vec2 stack pointers.
void worldCoordinatesToMap(const hfe::Vec3& position, hfe::Vec2& output);
// map coordinates to the world ground point below them (see the definition).
hfe::Vec3 mapCoordinatesToGround(const hfe::Vec2& coordinates, bool normalized);
// [CHOSEN] Compatibility forwarder to setSquadRequestType.
// Remove it after callers use the typed member directly.
void dispatchMapSetSelectionWord(Bf2MenuMapNode*, int, unsigned int); // +F8 ->772CA0
} }
#endif
#endif
