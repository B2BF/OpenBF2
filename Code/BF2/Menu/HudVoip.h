// [INFERRED] Voice-chat HUD row controller.
#ifndef MENU_HUDVOIP_H
#define MENU_HUDVOIP_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include <cstdint>
namespace dice { namespace meme { class System; class BfListBoxData; } }
namespace dice { namespace hfe { namespace world { class IPlayer; } } }
namespace dice { namespace hfe { namespace hud {

class IHudVoip {
public:
    virtual void setSystem(meme::System*) = 0; // +00 794530, folded with Bf2HudBuilder::setSystem
    virtual bool registerData() = 0;
    virtual bool registerFunctions() = 0;      // +08 5A5E20, the shared `return true`
};

class HudVoip : public IHudVoip {
public:
    explicit HudVoip(meme::System*);
    ~HudVoip();                                // 7A4DD0; nonvirtual, the HUD destroys this owner
    void setSystem(meme::System*) override;
    bool registerData() override;
    bool registerFunctions() override;
    // Called from BF2HudManager::updateModeWidgets mode 0 with
    // (squadId > 0 || isCommander) && m_enabled.
    void update(bool visible);

private:
    friend class BF2HudManager;
    friend struct HudVoipFixture;
    // Paints one player's row into the list.
    void paintRow(meme::BfListBoxData* list, world::IPlayer* player);

    meme::System* m_system;        // +04, borrowed ctor argument
    std::uint32_t m_field008;      // +08, ctor 0 [UNRESOLVED]
    int m_visibleRows;             // +0C, ctor 9; copied into the list's +2C
    std::uint32_t m_field010;      // +10, ctor 0 [UNRESOLVED]
    bool m_visible;                // +14, the last update() argument
    // +15. The only writers are BF2HudManager::applyModeTransition (750510,
    // still a [FRONTIER]): cleared at 750587, set at 750879. Until that lands
    // update() returns immediately and the list stays empty - retail's own
    // gating, not a VoIP gap.
    bool m_enabled;                // +15
    int m_rowHeight;               // +18, ctor 0 [INFERRED]; fild'd into every cell
    // These two are the second and third columns. BfListBoxData's own
    // constructor already adds a width-0 column at index 0, which is where the
    // row painter puts the commander/squad status icon - hidden by its width.
    // Index 1 gets the speaker icon, index 2 the player's name.
    int m_speakerColumnWidth;      // +1C, ctor 15
    int m_nameColumnWidth;         // +20, ctor 30
    float m_field024;              // +24, ctor -1.0f; the cells' sixth float
    // Meme texture handles (pointer-sized; int on x86).
    std::intptr_t m_emptyTexture;     // +28, Ingame/GeneralIcons/empty.tga
    std::intptr_t m_speakerTexture;   // +2C, Ingame/Respawn/icon_speaker.tga
    std::intptr_t m_commanderTexture; // +30, Ingame/Kits/Icons/kit_Commander.tga
    // +34..+58. Per-squad minimap icons, loaded lazily by the row painter.
    std::intptr_t m_squadTextures[10]; // ctor 0
    meme::BfListBoxData* m_data;   // +5C, created by registerData, never deleted
};
#if defined(_M_IX86)
static_assert(sizeof(HudVoip) == 0x60, "retail and native HudVoip storage");
#endif
} } }
#endif
#endif
