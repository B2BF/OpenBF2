// [INFERRED] name/path, owned at BF2HudManager+13C. PE9324E0 has15 entries.
// Native STL/bitset storage; no retail STL overlay.
#ifndef MENU_HUDCOMMANDER_H
#define MENU_HUDCOMMANDER_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "../Game/IEventListener.h"
#include "../Game/IGame.h"
#include <bitset>
#include <cstdint>
#include <list>
#include <string>
namespace dice { namespace meme { class System; class BfListBoxData; class Bf2MenuMapNode; } }
namespace dice { namespace hfe { namespace world { class IPlayer; } } }
namespace dice { namespace hfe { namespace hud {
class BF2HudManager;
class HudConsoleBinding;
class HudCommander : public IEventListener {
public:
    explicit HudCommander(meme::System*);
    ~HudCommander(); // 794500, nonvirtual; does not own the list-data pointer
    void handleEvent(EventCategory, unsigned int, EventNode*, void*) override;
    virtual void setSystem(meme::System*);
    virtual bool registerData();
    virtual bool registerFunctions(); // +0C 5A5E20, retailtrue
    virtual void update(bool refreshRows);
    virtual void refreshAbilities();
    // Nonvirtual CommanderMenu console targets. Their names are attested by
    // the six generated bindings at 76D42F..76D7B8.
    void singleClick();
    void rightClick();
    void doubleClick(); // 83D270, shipped RET-only body
    void deselect();
    void sendRadioMessage(std::string message, TargetChannel channel);
    virtual void selectSingleSquad(unsigned int squad);
    virtual void toggleSquad(unsigned int squad); // +1C 793D50
    virtual void toggleExpandedSquad(int squad);
    virtual void selectAllSquads(bool selected);
    virtual void setSquadSelected(int squad, bool selected);
    virtual void selectSquadRange(int first, int last); // +2C 793950
    virtual bool isSquadSelected(int squad);
    virtual int getLastSelectedSquad();
    virtual int getExpandedSquad();
private:
    friend class BF2HudManager;
    friend class HudConsoleBinding;
    friend struct HudCommanderFixture;
    // Map clicks can clear the expanded squad directly.
    friend class meme::Bf2MenuMapNode;
    void handleHudEvent(unsigned int, EventNode*, void*);
    // Builds one 44-column row per squad, numbered one through nine. [INFERRED] Helper names.
    void rebuildSquadRows(int team);
    void appendSquadRow(const std::list<world::IPlayer*>& players, int team, int squad);
    void appendMemberCells(world::IPlayer* player, int slot, int squad); // 793200, cols 5*slot+1..+5
    void appendLeaderVoipCells(int squad);  // 793F10, cols 32..34
    void appendRequestCells(int squad);     // 792C30, cols 35..37
    void appendSquadNumberCells(int squad); // 792E90, cols 39..41, then the row highlight
    void appendSquadFrameCells(int squad);  // 793090, col 42 (hidden id), col 0 frame
    void refreshArtillery();
    void refreshSupplies();
    void refreshUAV(); // 7929A0, needs HudMap772E90
    void refreshSatellite(); // 792910, needs HudMap772E40
    void refreshVehicleDrop(); // 792B50, native team drop-vehicle string
    void setArtilleryReloading(bool);
    void setSuppliesReloading(bool);
    void setVehicleDropReloading(bool);
    int getSelectedRowFlag(); // 793840, atoi hidden column43
    int getSelectedSquad(); // 7938C0, hidden columns43/42
    void selectRadioSquad(int squad);

    meme::System* m_system; // +04 borrowed
    bool m_artilleryState, m_noArtillery, m_noSatellite, m_noVehicleDrop, m_noUAV; // +08..0C
    float m_uavArmor, m_satelliteArmor, m_artilleryArmor, m_previousArmor; // +10..1C
    bool m_warningActive, m_warningPending; // +20/+21
    double m_warningTime, m_warningInterval; // +28/+30
    float m_artilleryDuration, m_artilleryFraction, m_artilleryRemaining; // +38..40
    bool m_artilleryReloading; // +44
    float m_artilleryDeadline; // +48
    bool m_supplyState; // +4C
    float m_supplyRemaining, m_supplyFraction, m_supplyDuration; // +50..58
    bool m_supplyReloading; // +5C
    float m_supplyDeadline; // +60
    bool m_vehicleDropReloading; // +64
    float m_vehicleDropDuration, m_vehicleDropFraction; // +68/+6C
    bool m_vehicleDropState; // +70
    float m_vehicleDropDeadline, m_vehicleDropRemaining, m_vehiclesAvailable; // +74..7C
    float m_blinkInterval, m_lastBlink; // +80/+84
    bool m_blink, m_radioShown; // +88/+89
    // Squad radio selection, talking-leader count and list dimensions; each member slot spans m_slotColumns columns.
    int m_radioSquad, m_talkingLeaders, m_rowCount, m_visibleRows, m_slotColumns; // +8C..9C
    bool m_sortByScore; // +A0
    std::bitset<16> m_selectedSquads; // +A4; range16 and all-set low word proven
    // +A8 holds a player id despite its name. 793200 compares it with IPlayer::getId() (7932C3).
    //   The map's click handler passes getId() to toggleExpandedSquad (77BE79), and 7788B0 / 7CE490
    //   compare getExpandedSquad() with getId(). Not renamed here: the name reaches the virtual
    //   surface and the map files.
    int m_expandedSquad, m_lastSelectedSquad, m_team; // +A8..B0
    // +B4 / +B8: setRowHighlight's last two floats (792E90). They are -1.0 and 23.0, the height of
    // squadFrame.tga.
    // +BC: the y of every 15-pixel icon cell, 4.0 = (23 - 15) / 2.
    // +C0: the y of the two text cells, 0.
    // All four are float: each is pushed raw or read with `fadd`, never `fild`. Nothing else uses
    // them.
    float m_field0b4, m_field0b8, m_field0bc, m_field0c0; // +B4..C0
    int m_columnPositions[11]; // +C4..EC, initialized after event registration
    std::intptr_t m_textures[14]; // +F0..124, meme texture handles; initialized after event registration
    float m_colors[8]; // +128..144
    meme::BfListBoxData* m_data; // +148 untouched by constructor
    std::list<int> m_mutedPlayers; // +14C; event59 stores signed player ids
};
} } }
#endif
#endif
