// General profile settings and server-history storage.
#ifndef GAME_GENERALSETTINGS_H
#define GAME_GENERALSETTINGS_H
#include <string>
#include <vector>
#include "../Core/IBase.h"
// The interface is declared in every image (the dedicated AI reaches it through BFGame's menu
// arm, which no dedicated module can trigger); the implementation is client-only.
namespace dice { namespace hfe {

// Favourite/history entry. [INFERRED] Field names follow serialized settings.
// For history rows, lastPlayed is a play count used for ascending sorting.
struct GeneralServerEntry {
    std::string name;      // retail+00, the quoted third field
    std::string address;   // retail+1C, the quoted first field
    int queryPort;         // retail+38
    int lastPlayed;        // retail+3C, written by history rows only
};

// The records the three colour getters return BY value (hidden return pointer, `retn 4`): +0x50
// copies the four ints at +0x4C, +0x58 / +0x60 the three at +0x5C / +0x68. [INFERRED] names.
struct GeneralSettingsRgba { int red, green, blue, alpha; };
struct GeneralSettingsRgb  { int red, green, blue; };

// General settings interface used by SwiffPlayer through virtual dispatch. Preserve
// slot order; method names and constness are [INFERRED] from bindings and field use.
// The implementation's deleting-destructor slot is intentionally not exposed here.
class IGeneralSettings : public IBase {
public:
    virtual std::string getFileName() = 0;
    virtual std::string getSettingsScript() = 0;
    virtual bool apply() = 0;                                       //+18 4B6890; no argument, unlike AudioSettings
    virtual void setDefaults() = 0;                                 //+1C 4B7460; createDefaultProfile 4A80E2
    virtual void setDefaultValues() = 0;                            //+20 4B6AB0; setDefaults dispatches here virtually
    virtual void cancel() = 0;                                      //+24 4B7540: re-read the profile's General.con, nine values kept
    virtual std::string getSortKey() const = 0;
    virtual void setSortKey(std::string value) = 0;                 //+2C 5D2000 (ICF), by value
    virtual int  getSortOrder() const = 0;                          //+30 433140 (ICF)
    virtual void setSortOrder(int value) = 0;
    virtual int  getNumRoundsPlayed() const = 0;                    //+38 65DE70 (ICF)
    virtual void setNumRoundsPlayed(int value) = 0;                 //+3C 4B7210
    virtual std::string getServerFilter() const = 0;                //+40 65D420 (ICF)
    virtual void setServerFilter(std::string value) = 0;            //+44 656AD0 (ICF), by value
    virtual float getHUDTransparency() const = 0;                   //+48 65A0A0 (ICF)
    virtual void setHUDTransparency(float value) = 0;               //+4C 4B7220
    virtual GeneralSettingsRgba getCrosshairColor() const = 0;
    virtual void setCrosshairColor(int red, int green, int blue, int alpha) = 0;
    virtual GeneralSettingsRgb getBuddytagColor() const = 0;
    virtual void setBuddytagColor(int red, int green, int blue) = 0; //+5C 4B7420
    virtual GeneralSettingsRgb getSquadtagColor() const = 0;
    virtual void setSquadtagColor(int red, int green, int blue) = 0;
    virtual bool getMinimapRotate() const = 0;                      //+68 7DE6A0 (ICF)
    virtual void setMinimapRotate(bool value) = 0;                  //+6C 4B7260
    virtual float getMinimapTransparency() const = 0;
    virtual void setMinimapTransparency(float value) = 0;           //+74 5C9510 (ICF)
    virtual bool getViewIntroMovie() const = 0;
    virtual void setViewIntroMovie(bool value) = 0;                 //+7C 4B7290
    virtual bool getOutOfVoting() const = 0;
    virtual void setOutOfVoting(bool value) = 0;
    virtual std::string getBFTVSaveDirectory() const = 0;           //+88 65B1C0 (ICF)
    virtual void setBFTVSaveDirectory(std::string value) = 0;       //+8C 65CF10 (ICF), by value
    virtual bool getConfirmQuit() const = 0;
    virtual void setConfirmQuit(bool value) = 0;
    virtual float getMapIconAlphaTransparency() const = 0;          //+98 7F7300 (ICF)
    virtual void setMapIconAlphaTransparency(float value) = 0;      //+9C 5C9520 (ICF)
    virtual bool getUseAdvancedServerBrowser() const = 0;           //+A0 4B72E0
    virtual void setUseAdvancedServerBrowser(bool value) = 0;       //+A4 4B72F0
    virtual bool getUseBots() const = 0;                            //+A8 4B7300
    virtual void setUseBots(bool value) = 0;                        //+AC 4B7310
    virtual int  getMaxBots() const = 0;                            //+B0 6D6A70 (ICF)
    virtual void setMaxBots(int value) = 0;                         //+B4 4B7320
    virtual bool getMaxBotsIncludeHumans() const = 0;               //+B8 4B7330
    virtual void setMaxBotsIncludeHumans(bool value) = 0;           //+BC 4B7340
    virtual float getBotSkill() const = 0;                          //+C0 4B7350
    virtual void setBotSkill(float value) = 0;                      //+C4 4B1C90
    virtual int  getLastAwardsCheckDate() const = 0;                //+C8 6BBD40 (ICF)
    virtual void setLastAwardsCheckDate(int value) = 0;             //+CC 4B1910 (ICF with AudioSettings +0x44)
    virtual bool getAllowPunkBuster() const = 0;                    //+D0 612E20 (ICF)
    virtual void setAllowPunkBuster(bool value) = 0;                //+D4 4B7360
    virtual bool getItemSelectionReverseItems() const = 0;          //+D8 4B7370
    virtual void setItemSelectionReverseItems(bool value) = 0;      //+DC 4B7380
    virtual bool getAutoReload() const = 0;                         //+E0 4B7390
    virtual void setAutoReload(bool value) = 0;                     //+E4 4B73A0
    virtual float getCoopBotRatio() const = 0;                      //+E8 4B73B0
    virtual void setCoopBotRatio(float value) = 0;                  //+EC 4B1930
    virtual int  getCoopBotCount() const = 0;                       //+F0 5049A0 (ICF)
    virtual void setCoopBotCount(int value) = 0;                    //+F4 4B1CA0
    virtual bool getAutoReady() const = 0;                          //+F8 4B5140: GSDemo forces true
    virtual void setAutoReady(bool value) = 0;                      //+FC 4B3B90
    virtual int  getConnectionType() const = 0;
    virtual void setConnectionType(int value) = 0;
    virtual void setToggleFilters(int value) = 0;                   //+108 4B3B70: the whole mask
    virtual bool getToggleFilter(int mask) const = 0;               //+10C 4B5230
    virtual void setToggleFilter(int mask, bool on) = 0;
    virtual void setPlayedVOHelp(const std::string& name) = 0;      //+114 4B76F0: case-insensitive add-if-absent
    virtual bool checkPlayedVOHelp(const std::string& name) const = 0; //+118 4B62C0: case-insensitive find
    virtual void addFavouriteServer(const std::string& address, int queryPort,
                                    const std::string& name) = 0;   //+11C 4B7B50
    virtual int  getNumFavouriteServers() const = 0;
    virtual GeneralServerEntry* getFavouriteServer(unsigned int index) = 0; //+124 4B6330: unchecked
    virtual bool isFavouriteServer(const std::string& address, int queryPort) = 0; //+128 4B51C0: through +120/+124
    virtual bool removeFavouriteServerEntry(unsigned int index) = 0; //+12C 4B63C0: through +120 and +130
    virtual bool removeFavouriteServer(const std::string& address, int queryPort) = 0;
    virtual GeneralServerEntry* findFavouriteServer(const std::string& address, int queryPort) = 0;
    virtual void addServerHistory(const std::string& address, int queryPort,
                                  const std::string& name, int lastPlayed) = 0;
    virtual int  getServerHistoryCount() const = 0;                 //+13C 4B53F0
    virtual GeneralServerEntry* getServerHistory(unsigned int index) = 0; //+140 4B6350: unchecked
    virtual void sortServerHistory() = 0;                           //+144 4B7E60: std::sort by lastPlayed
    virtual bool removeServerHistoryEntry(unsigned int index) = 0;  //+148 4B6370: through +13C and +14C
    virtual bool removeServerHistory(const std::string& address, int queryPort) = 0; //+14C 4B7850
    virtual GeneralServerEntry* findServerHistory(const std::string& address, int queryPort) = 0;
    virtual void clearServerHistory() = 0;                          //+154 4B7940: + the browser's "history" list
    virtual void clearFavouriteServers() = 0;
    virtual bool getLCDDisplayModes() const = 0;                    //+15C 4B5280
    virtual void setLCDDisplayModes(bool value) = 0;
    // +164 is the deleting destructor (4B7B20), the one slot still withheld: see ~GeneralSettings.
};

#if !DEDICATED
struct GeneralSettingsFixture;
class GeneralSettings final : public IGeneralSettings {
public:
    GeneralSettings();                            // retail 252 bytes
    // [CHOSEN, not recovered] [RETIRE-WHEN: general-settings-prefix-publication]
    // Direct final-owner deletion; the deleting destructor lives at the withheld +164 (4B7B20).
    ~GeneralSettings();                           // body
    static IBase* create(unsigned int iid, IBase* outer);
    void addRef() override;                       // shared
    int getRef() override;                        // shared
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    std::string getFileName() override;
    std::string getSettingsScript() override;
    bool apply() override;
    void setDefaults() override;
    void setDefaultValues() override;
    void cancel() override;                                                       //+24, out of line

    // Options exposed through GeneralSettings console bindings and saved in General.con.
    // String setters take values by copy; float setters preserve the raw value bits.
    std::string getSortKey() const override        { return m_sortKey; }           //+28
    void  setSortKey(std::string value) override   { m_sortKey = value; }          //+2C
    int   getSortOrder() const override            { return m_sortOrder; }         //+30
    void  setSortOrder(int value) override         { m_sortOrder = value; }        //+34
    int   getNumRoundsPlayed() const override      { return m_numRoundsPlayed; }   //+38
    void  setNumRoundsPlayed(int value) override   { m_numRoundsPlayed = value; }  //+3C
    std::string getServerFilter() const override   { return m_serverFilter; }      //+40
    void  setServerFilter(std::string value) override { m_serverFilter = value; }  //+44
    float getHUDTransparency() const override      { return m_hudTransparency; }   //+48
    void  setHUDTransparency(float value) override { m_hudTransparency = value; }  //+4C
    //  copies the four ints at +0x4C..+0x58 into the caller's record; 4B73F0 (retn 10h) stores
    // its four arguments there in argument order.
    GeneralSettingsRgba getCrosshairColor() const override                         //+50
    { GeneralSettingsRgba c = { m_crosshairColor[0], m_crosshairColor[1], m_crosshairColor[2], m_crosshairColor[3] }; return c; }
    void  setCrosshairColor(int red, int green, int blue, int alpha) override      //+54
    { m_crosshairColor[0] = red; m_crosshairColor[1] = green; m_crosshairColor[2] = blue; m_crosshairColor[3] = alpha; }
    //  / 4BD5E0 copy three ints out; 4B7420 / 4B7440 (retn 0Ch) store three.
    GeneralSettingsRgb getBuddytagColor() const override                           //+58
    { GeneralSettingsRgb c = { m_buddytagColor[0], m_buddytagColor[1], m_buddytagColor[2] }; return c; }
    void  setBuddytagColor(int red, int green, int blue) override                  //+5C
    { m_buddytagColor[0] = red; m_buddytagColor[1] = green; m_buddytagColor[2] = blue; }
    GeneralSettingsRgb getSquadtagColor() const override                           //+60
    { GeneralSettingsRgb c = { m_squadtagColor[0], m_squadtagColor[1], m_squadtagColor[2] }; return c; }
    void  setSquadtagColor(int red, int green, int blue) override                  //+64
    { m_squadtagColor[0] = red; m_squadtagColor[1] = green; m_squadtagColor[2] = blue; }
    bool  getMinimapRotate() const override        { return m_minimapRotate; }     //+68
    void  setMinimapRotate(bool value) override    { m_minimapRotate = value; }    //+6C
    float getMinimapTransparency() const override  { return m_minimapTransparency; } //+70
    void  setMinimapTransparency(float value) override { m_minimapTransparency = value; } //+74
    // Voting participation is the inverse of m_outOfVoting.
    bool  getViewIntroMovie() const override       { return m_viewIntroMovie; }    //+78
    void  setViewIntroMovie(bool value) override   { m_viewIntroMovie = value; }   //+7C
    bool  getOutOfVoting() const override          { return m_outOfVoting; }       //+80
    void  setOutOfVoting(bool value) override      { m_outOfVoting = value; }      //+84
    std::string getBFTVSaveDirectory() const override { return m_bftvSaveDirectory; } //+88
    void  setBFTVSaveDirectory(std::string value) override { m_bftvSaveDirectory = value; } //+8C
    bool  getConfirmQuit() const override          { return m_confirmQuit; }       //+90
    void  setConfirmQuit(bool value) override      { m_confirmQuit = value; }      //+94
    float getMapIconAlphaTransparency() const override { return m_mapIconAlphaTransparency; } //+98
    void  setMapIconAlphaTransparency(float value) override { m_mapIconAlphaTransparency = value; } //+9C
    bool  getUseAdvancedServerBrowser() const override { return m_useAdvancedServerBrowser; } //+A0
    void  setUseAdvancedServerBrowser(bool value) override { m_useAdvancedServerBrowser = value; } //+A4
    // SwiffPlayer's getListEntries("installedMaps") reads +0xA8 to keep only AI-capable map types.
    bool  getUseBots() const override              { return m_useBots; }           //+A8
    void  setUseBots(bool value) override          { m_useBots = value; }          //+AC
    int   getMaxBots() const override              { return m_maxBots; }           //+B0
    // Coop limits update max bots before the current bot count.
    void  setMaxBots(int value) override;                                          //+B4, out of line
    bool  getMaxBotsIncludeHumans() const override { return m_maxBotsIncludeHumans; } //+B8
    void  setMaxBotsIncludeHumans(bool value) override { m_maxBotsIncludeHumans = value; } //+BC
    float getBotSkill() const override             { return m_botSkill; }          //+C0
    void  setBotSkill(float value) override        { m_botSkill = value; }         //+C4
    // The awards date has no console binding, so profile loading reseeds it from the current time.
    // The movie's updateLastAwardsCheckDate action calls the setter directly.
    int   getLastAwardsCheckDate() const override  { return m_lastAwardsCheckDate; } //+C8
    void  setLastAwardsCheckDate(int value) override { m_lastAwardsCheckDate = value; } //+CC
    bool  getAllowPunkBuster() const override      { return m_allowPunkBuster; }   //+D0
    void  setAllowPunkBuster(bool value) override  { m_allowPunkBuster = value; }  //+D4
    bool  getItemSelectionReverseItems() const override { return m_itemSelectionReverseItems; } //+D8
    void  setItemSelectionReverseItems(bool value) override { m_itemSelectionReverseItems = value; } //+DC
    bool  getAutoReload() const override           { return m_autoReload; }        //+E0
    void  setAutoReload(bool value) override       { m_autoReload = value; }       //+E4
    // Runtime-only coop ratio and count; construction, defaults, and profile scripts leave them uninitialized.
    float getCoopBotRatio() const override         { return m_coopBotRatio; }      //+E8
    void  setCoopBotRatio(float value) override    { m_coopBotRatio = value; }     //+EC
    int   getCoopBotCount() const override         { return m_coopBotCount; }      //+F0
    void  setCoopBotCount(int value) override;                                     //+F4, out of line
    // Auto-ready is forced during demo playback; otherwise it uses the stored setting.
    bool  getAutoReady() const override;                                           //+F8, out of line
    void  setAutoReady(bool value) override        { m_autoReady = value; }        //+FC
    //  is `mov eax, [ecx+0xC0]; ret` - the whole function. Retail's other caller is
    // GameClient::handleEvent's ECNetwork 3 arm, which reaches the object by name and hands the value
    // to setConnectionType.
    int   getConnectionType() const override       { return m_connectionType; }    //+100
    void  setConnectionType(int value) override    { m_connectionType = value; }   //+104
    // : the whole mask is replaced (`mov [ecx+0F4h], eax`) - unlike setToggleFilter, which
    // ORs or clears single bits. SwiffPlayer's dice.bf2.Multiplay.getToggleFilter / setToggleFilter
    // rows are the callers of +0x10C / +0x110 - one per server-browser filter checkbox.
    void  setToggleFilters(int value) override     { m_toggleFilters = value; }    //+108
    bool  getToggleFilter(int mask) const override { return (mask & m_toggleFilters) != 0; } //+10C
    void  setToggleFilter(int mask, bool on) override                              //+110
    { if (on) m_toggleFilters |= mask; else m_toggleFilters &= ~mask; }
    // The played voice-over-help list: General.con's one row per entry reads back as a set.
    void  setPlayedVOHelp(const std::string& name) override;                       //+114, out of line
    bool  checkPlayedVOHelp(const std::string& name) const override;               //+118, out of line

    // the favourite servers (+0xD4) and the server history (+0xE4)
    void  addFavouriteServer(const std::string& address, int queryPort,
                             const std::string& name) override;                   //+11C, out of line
    int   getNumFavouriteServers() const override { return static_cast<int>(m_favouriteServers.size()); } //+120
    //  / 4B6350 are `first + (index << 6)` with NO bounds test - spelled on the raw storage so a
    // checked-iterator build does not add one either.
    GeneralServerEntry* getFavouriteServer(unsigned int index) override { return m_favouriteServers.data() + index; } //+124
    bool  isFavouriteServer(const std::string& address, int queryPort) override;  //+128, out of line
    bool  removeFavouriteServerEntry(unsigned int index) override;                 //+12C, out of line
    bool  removeFavouriteServer(const std::string& address, int queryPort) override; //+130, out of line
    GeneralServerEntry* findFavouriteServer(const std::string& address, int queryPort) override; //+134
    void  addServerHistory(const std::string& address, int queryPort,
                           const std::string& name, int lastPlayed) override;     //+138, out of line
    int   getServerHistoryCount() const override { return static_cast<int>(m_serverHistory.size()); } //+13C
    GeneralServerEntry* getServerHistory(unsigned int index) override { return m_serverHistory.data() + index; } //+140
    void  sortServerHistory() override;                                            //+144, out of line
    bool  removeServerHistoryEntry(unsigned int index) override;                   //+148, out of line
    bool  removeServerHistory(const std::string& address, int queryPort) override; //+14C, out of line
    GeneralServerEntry* findServerHistory(const std::string& address, int queryPort) override; //+150
    void  clearServerHistory() override;                                           //+154, out of line
    void  clearFavouriteServers() override;                                        //+158, out of line
    // LCD display mode setting, also exposed through the movie's VideoSettings namespace.
    bool  getLCDDisplayModes() const override      { return m_lcdDisplayModes; }   //+15C
    void  setLCDDisplayModes(bool value) override  { m_lcdDisplayModes = value; }  //+160

    // [CHOSEN, not recovered] Owner's tutorial option.
    // Native-only accessors leave the recovered virtual interface unchanged.
    bool getShowTutorialMessages() const { return m_showTutorialMessages; }
    void setShowTutorialMessages(bool value) { m_showTutorialMessages = value; }
private:
    friend struct GeneralSettingsFixture;
    int m_refCount;                               // retail+04
    std::string m_sortKey;                        //+08
    int m_sortOrder;                              //+24
    int m_numRoundsPlayed;                        //+28
    std::string m_serverFilter;                   //+2C
    float m_hudTransparency;                      //+48
    int m_crosshairColor[4];                      //+4C..+58 red, green, blue, alpha
    int m_buddytagColor[3];                       //+5C..+64
    int m_squadtagColor[3];                       //+68..+70
    bool m_minimapRotate;                         //+74
    float m_minimapTransparency;                  //+78
    float m_mapIconAlphaTransparency;             //+7C
    bool m_viewIntroMovie;                        //+80
    bool m_outOfVoting;                           //+81
    std::string m_bftvSaveDirectory;              //+84
    bool m_confirmQuit;                           //+A0
    bool m_useAdvancedServerBrowser;              //+A1
    bool m_useBots;                               //+A2
    int m_maxBots;                                //+A4
    bool m_maxBotsIncludeHumans;                  //+A8
    float m_botSkill;                             //+AC
    int m_lastAwardsCheckDate;                    //+B0, the 32-bit CRT time
    bool m_allowPunkBuster;                       //+B4
    bool m_itemSelectionReverseItems;             //+B5
    bool m_autoReload;                            //+B6
    float m_coopBotRatio;                         //+B8, runtime only (see getCoopBotRatio)
    int m_coopBotCount;                           //+BC, runtime only
    int m_connectionType;                         //+C0
    std::vector<std::string> m_playedVOHelp;      //+C4, case-insensitive add at +114
    std::vector<GeneralServerEntry> m_favouriteServers; //+D4
    std::vector<GeneralServerEntry> m_serverHistory;    //+E4
    int m_toggleFilters;                          //+F4, a bit mask: +10C tests, +110 sets/clears
    bool m_autoReady;                             //+F8
    bool m_lcdDisplayModes;                       //+F9
    // [CHOSEN, not recovered] Persisted general option; true keeps retail pop-ups.
    bool m_showTutorialMessages;
};

#endif // !DEDICATED
} }
#endif
