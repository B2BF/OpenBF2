// [INFERRED] names/path. Client Profile console: the profile store and its 17 bindings.
#ifndef MENU_PROFILECONSOLEBINDINGS_H
#define MENU_PROFILECONSOLEBINDINGS_H
#include "../Core/ConsoleObjects.h"
#include <map>
#include <string>
#if !DEDICATED
namespace dice { namespace hfe { namespace hud {

struct ProfileConsoleFixture;

// [INFERRED] names. The console-side profile store, BF2.exe 0x7BF950..0x7C1340: an owner of player
// profiles, each with character profiles, each with per-mod settings. Nothing in retail frees them
// (the owner's destructor 7BFE80/7BFC10 erases keys and nodes only), so they live until exit.

// Per-mod settings, 0x84 bytes, ctor 7C1200.
class ModSetting {
public:
    explicit ModSetting(const std::string& name);                // 7C1200
    void applySettings();                                        // 7C09E0
    std::string m_name;                                          // +00
    std::string m_playerName;                                    // +1C, empty
    int m_visualUnlockLevel;                                     // +38, 0
    int m_defaultKit;                                            // +3C, see the ctor
    std::string m_tagIcon;                                       // +40, ""
    std::string m_tagText;                                       // +5C, ""
    std::map<std::string, std::map<int, int> > m_kitIndices;     // +78
};

// 0x30 bytes, ctor 7C1270. Starts with one "BF2" mod setting, which is active.
class CharacterProfile {
public:
    CharacterProfile(const std::string& name, int id);           // 7C1270
    const std::string& getName() const { return m_name; }        // 5B0770
    bool addModSetting(const std::string& name);                 // 7C1340
    // 689490: the active setting read as its name (+00); no null test.
    const std::string& getActiveMod() const { return m_activeMod->m_name; }
    void setActiveMod(const std::string& name);                  // 7C0B00
    const std::string& getPlayerName() const { return m_activeMod->m_playerName; }      // 7C05D0
    void setPlayerName(const std::string& value) { m_activeMod->m_playerName = value; } // 7C05E0
    int getVisualUnlockLevel() const { return m_activeMod->m_visualUnlockLevel; }       // 7C05B0
    void setVisualUnlockLevel(int value) { m_activeMod->m_visualUnlockLevel = value; }  // 7C05C0
    int getDefaultKit() const { return m_activeMod->m_defaultKit; }                     // 7C0590
    void setDefaultKit(int value) { m_activeMod->m_defaultKit = value; }                // 7C05A0
    const std::string& getTagIcon() const { return m_activeMod->m_tagIcon; }            // 7C05F0
    void setTagIcon(const std::string& value) { m_activeMod->m_tagIcon = value; }       // 7C0600
    const std::string& getTagText() const { return m_activeMod->m_tagText; }            // 7C0610
    void setTagText(const std::string& value) { m_activeMod->m_tagText = value; }       // 7C0620
    void setIndexForKit(const std::string& kit, int index, int key); // 7C1080
    int getIndexForKit(const std::string& kit, int key);         // 7C0B30
    void applySettings() { m_activeMod->applySettings(); }       // 7C0B90
private:
    friend struct ProfileConsoleFixture;
    std::string m_name;                                          // +00
    int m_id;                                                    // +1C, the map size after insertion
    ModSetting* m_activeMod;                                     // +20
    std::map<std::string, ModSetting*> m_mods;                   // +24
};

// 0x38 bytes, ctor 7C0530.
class PlayerProfile {
public:
    PlayerProfile(const std::string& name, int id);              // 7C0530
    const std::string& getName() const { return m_name; }        // 5B0770
    int getRank() const { return m_rank; }                       // 689490
    void setRank(int rank) { m_rank = rank; }                    // 6D8770
    bool addCharacterProfile(const std::string& name);           // 7C0440
    // 7C0000: the selected character's name; no null test.
    const std::string& getActiveCharacterProfile() const { return m_selectedCharacter->getName(); }
    bool setActiveCharacterProfile(const std::string& name);     // 7C02C0
    bool activateCharacterProfile(const std::string& name);      // 7C0300
    void activateFirstCharacterProfile();                        // 7C0510
    CharacterProfile* getSelectedCharacter() const { return m_selectedCharacter; } // 433140
    CharacterProfile* getActiveCharacter() const { return m_activeCharacter; }     // 65DE70
private:
    friend struct ProfileConsoleFixture;
    std::string m_name;                                          // +00
    int m_id;                                                    // +1C, the map size after insertion
    int m_rank;                                                  // +20, -1
    CharacterProfile* m_selectedCharacter;                       // +24
    CharacterProfile* m_activeCharacter;                         // +28
    std::map<std::string, CharacterProfile*> m_characters;       // +2C
};

class ProfileConsoleOwner {
public:
    ProfileConsoleOwner();
    ~ProfileConsoleOwner();
    PlayerProfile* getSelectedProfile() const { return m_selectedProfile; }   // 689460
    PlayerProfile* getActiveProfile() const { return m_activeProfile; }       // 689490
    bool createPlayerProfile(const std::string& name);           // 7BFDB0
    const std::string& getActivePlayerProfile() const;           // 7BF950
    void setActivePlayerProfile(const std::string& name);        // 7BFC60
    bool activatePlayerProfile(const std::string& name);         // 7BFCA0
private:
    friend struct ProfileConsoleFixture;
    std::string m_emptyName;                     // retail+00
    PlayerProfile* m_selectedProfile;            // retail+1C
    PlayerProfile* m_activeProfile;              // retail+20
    std::map<std::string, PlayerProfile*> m_profiles; // retail+24; pointer values not deleted
};

extern ProfileConsoleOwner* profileConsoleOwner; // A22158, initially null
ConsoleObjects::ConsoleObjectList& profileConsoleBindingList();

} } }
#endif
#endif
