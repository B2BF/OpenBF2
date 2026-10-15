// Compact map node for the in-game HUD.
#ifndef MENU_BF2MINIMENUMAPNODE_H
#define MENU_BF2MINIMENUMAPNODE_H
#include "Bf2MenuMapNode.h"
#if !DEDICATED
namespace dice { namespace meme {

class Bf2MiniMenuMapNode : public Bf2MenuMapNode {
public:
    // Defaults from the real factory 7CFED0, which pushes six zeros. A
    // default-constructed node dereferences its null System on the first
    // loadTexture - exactly as the shipped factory would; it is not guarded.
    Bf2MiniMenuMapNode(System* system = nullptr, const char* name = nullptr,
                       int x = 0, int y = 0, int width = 0, int height = 0);
    ~Bf2MiniMenuMapNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    void paintSelf(System*, const NodeState*, const Rect*, const Rect*,
                   RenderContext*) override;
    bool onEvent1(System*, const Rect*, const NodeState*, int, int, int) override;
    // Handles the minimap's HUD events through the map event interface.
    void handleHudEvent(unsigned int id, hfe::EventNode* node, void* data) override;
    // +DC, slot 55 - one of the twenty overrides, and not one of the three that
    // point at the shared "return zero" 4553E0. The map's body is a 29-way switch
    // (777DC0); this one tests two modes. Recovered in full.
    void setHudMode(int mode) override;                      // 7CE3D0, +DC
    // +118 and +11C: two more of the twenty overrides, and both collapse to the
    // same added slot - this class's own +158. setViewCenter discards the
    // position it is handed.
    void recenterOnPlayer() override;
    void setViewCenter(hfe::Vec2 position) override;         // 7CE480, +11C
    static TypeDescriptor* getClassType();                   // 7CF750, metadata9389C4

    // [CHOSEN, not recovered] Resets minimap view bias through native dispatch.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    void resetViewBiasForHud();

protected:
    friend struct Bf2MiniMenuMapNodeFixture;
    // Updates blink state and timed markers, then records world time.
    void update(float delta) override;                                 // +E0 7CE340
    TextureHandle timedMarkerTexture(unsigned int id) override;                // +E4 4553E0
    hfe::Vec2 timedMarkerSize(unsigned int id) override;               // +E8 7CF690
    bool populateNetworkIcon(int id, MapIconRecord* icon,
                             bool asPlayer) override;                  // +EC 7CF660
    TextureHandle getSmallSquadIcon(int squadId) override;                     // +FC 4553E0
    // +110, the third slot this class points at the shared "return 0" 4553E0: the
    // map's support-request icon getter answers no texture on a mini-map.
    TextureHandle getSupportRequestTexture(int eventId) override;
    // Adds commander minimap order icons without squad numbers. Order lines require flag2C; requests always draw their marker.
    bool addOrderIcons(hfe::SquadOrder* order, int squad, int leaderNetworkId,
                       int kind, const Rect* bounds) override;
    void styleOrderIcons(int kind, MapLineRecord* line, MapIconRecord* marker,
                         bool active) override;
    void populateControlPointMapIcon(MapIconRecord* icon,
                                     int team,
                                     bool useTeamTexture,
                                     std::string mapName,
                                     bool notCapturable,
                                     bool cachePresent,
                                     bool localCaptureActive) override; // +13C 7CF5A0
    void paintMapIcon(hfe::Vec2 position, float rotation, hfe::Vec2 size,
                      std::uintptr_t texture, System* system, const Rect* adjusted,
                      const Rect* bounds, const Color* color,
                      RenderContext* context) override;
    void collectLocalPlayerIcon() override;                              // +12C 7CEDD0, 583 B

    // [CHOSEN, not recovered] Native event-routing accessors.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace them when the corresponding Node virtual methods are typed and validated.
    void colorMiniMapIcon(hfe::world::IPlayer* player, MapIconRecord* icon,
                          bool highlightSelection);                      // +14C 7CE490
    // +160, 7CF1C0 (121 B): the mini-map's player-icon shape - populatePlayerMapIcon (+138) cut
    // down to six arguments: the local player gets the 120x120 zoom square, everyone else a 2 or
    // 4 pixel dot. Both out-flags always come back false. `showKit` is never read.
    void populateMiniPlayerIcon(hfe::world::IPlayer* player, MapIconRecord* icon,
                                bool* headingOut, bool* rotateOut,
                                bool showKit, bool specialRoleIcons);

    // World time of the last update. The constructor leaves it unset, so update must precede meaningful paint timing.
    double m_lastUpdateTime;                                             // retail +C80
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2MiniMenuMapNode) == 3008, "native Bf2MiniMenuMapNode storage");
#endif
} }
#endif
#endif
