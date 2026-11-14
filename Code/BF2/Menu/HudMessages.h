// [INFERRED] name/path from the BF2HudManager +148 owner. BF2.exe table933748
// has five entries, beginning with IEventListener::handleEvent. See status.
#ifndef MENU_HUDMESSAGES_H
#define MENU_HUDMESSAGES_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "../Game/IEventListener.h"
#include <cstdint>
#include <vector>
namespace dice { namespace hfe { namespace world { class IObject; } } }
namespace dice { namespace hfe { class GeneralSettings; } }
namespace dice { namespace hfe { namespace hud {
class BF2HudManager;
class HudMessages : public IEventListener {
public:
    HudMessages();
    ~HudMessages();                        // 79DDA0, nonvirtual concrete ownership
    void handleEvent(EventCategory, unsigned int, EventNode*, void*) override;
    virtual void init();                   // +04 83D270, retail empty
    virtual bool registerData();
    virtual bool slot_0x0c();              // +0C 5AB2B0, retail false; role unresolved
    virtual bool update(float delta);      // +10 79E160, returns current visibility
    int getSelectedItem();                 // 79DFA0, no selection bounds guard
    int findItem(int item);                // 79DFB0, missing returns0
    int confirmSelection();                // 79E1A0, hides before reading item
    bool refreshItems();                   // 79E450, returns whether the fallback vehicle supplied items
    void selectItem(int item, bool playSound); // 79E2B0, byte bool and signed item scan
    // [INFERRED] name. BF2HudManager::applyModeTransition's
    // phase-2 case 10 is the only caller: it opens the item wheel and reverts the mode when this
    // answers false, so the return is the new m_visible.
    bool showItems();
    // Selects the next message. The ABI word is unused; int is its [CHOSEN] representation.
    void selectNext(int unusedWord);
    void selectPrevious(int unusedWord);
private:
    friend class BF2HudManager;
    friend struct HudConsoleAccess;          // hudMessages.setReverseItems (76EB14) writes +08 directly
    friend class dice::hfe::GeneralSettings; // apply (PE 4B6975) stores its itemSelectionReverseItems byte into +08
    // HudVehicleInformation::setupOccupiedVehicle (PE 78D89C) stores 2.0f into +0x18 the same
    // way BF2HudManager's own arms do - a direct write, not through a setter.
    friend class HudVehicleInformation;
    friend struct HudPlayerFilterFixture;
    friend struct HudMessagesFixture;
    static void assignIcon(const char*& destination, const char* source);
    void compactItems();                   // 79DFE0, retains source icon and both alpha fields
    void populateItems(std::vector<world::IObject*> objects); // 79E1B0, by-value vector
    struct Item {
        const char* icon;                  // borrowed sentinel or owned new[] text
        int id;
        bool active;
        float backgroundAlpha, activeAlpha; // untouched by constructor
    };
    std::uint32_t m_field004;              // untouched, interpretation unresolved
    bool m_enabled;                        // +08 true
    bool m_visible;                        // +09 false
    int m_selectedItem;                    // +0C -1
    int m_field010;                        // +10 zero
    int m_itemCount;                       // +14 zero
    float m_duration;                      // +18 10
    float m_remainingTime;                 // +1C 10
    double m_field020;                     // +20 0.3
    double m_field028;                     // +28 0
    Item m_items[10];                      // +30, each20 bytes
    static char s_emptyIcon;               // address-only zero-filled A1EF28 sentinel
};
} } }
#endif
#endif
