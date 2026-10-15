// HUD manager interface and native state. [INFERRED] Header placement.
// Reference offsets describe VC7.1 and must not be used as native STL offsets.
#ifndef MENU_BF2HUDMANAGER_H
#define MENU_BF2HUDMANAGER_H

#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "../IO/IInputFilter.h"
#include "../Game/IEventListener.h"
#include "../Game/IGame.h" // world::VehicleType: console-attested +13C parameter
#include "../Game/GameEvents/RequestEvent.h" // nested RequestTypes at HUD+2AC
#include "../Game/GameLogic/SquadManager/SquadOrder.h" // embedded native value at retail+270
#include "../Math/Vec3.h"
#include <cstdint>
#include <list>
#include <string>
#include <vector>

namespace dice { namespace meme { class Data; class BfEditData; class BfListBoxData; class BfOccupiedVehicleData; class Bf2MenuMapNode; class Bf2PictureNode; } }
namespace dice { namespace hfe {
struct SquadOrder;
class ISound;
namespace menu { class Ref2System; }
namespace world { class IPlayer; class ICameraObject; class ISoldier; class Kit; }
namespace hud {

// 74DC00, [INFERRED] name. Narrow -> wide with one allocation and one mbstowcs bounded by the
// narrow string's native length plus terminator; empty text on a conversion failure. Defined in
// BF2HudManagerHudEvents.cpp. Shared: BF2HudManager's commander-chat arm and
// HudScoreboard::updateFinalList are its two callers, in two different TUs.
std::wstring convertHudEventText(const std::string& text);
class HudBlinkManager;
class HudInformationAdapter;
class Bf2HudBuilder;
class HudLoadingScreen;
class HudAwardCatalogue; // [INFERRED] name; 7BF910 native award catalogue
using HudInitOwner = HudAwardCatalogue;
class HudRadio;
class HudChat;
// Concrete owner names [INFERRED] from constructor/consumer evidence. These
// declarations do not manufacture their still-unrecovered virtual surfaces.
class HudQuestionManager; // 788540, nonpolymorphic
class HudScoreboard;
class HudVoip;
class HudVoteMap;
class HudKitSelection;   // 7A1DE0, nonpolymorphic
class HudAmmo;
class HudVehicleInformation;
using HudMap = meme::Bf2MenuMapNode; // descriptor-attested builder map owner7764B0
class HudSquad;
class HudCommander;
class HudMessages;

// [INFERRED] record name; complete7B0AF0 walks12-byte values and decrements+8.
struct HudPendingRequest { int squad, type; float remaining; };

class BF2HudManager : public io::IInputFilter, public IEventListener
{
public:
    BF2HudManager();                                  // 0x7564A0, recovered
    ~BF2HudManager() override;                        // +0x00
    bool filterGameInput(io::GameInput&) override;     // +0x04
    bool filterPlayerInput(io::PlayerInput&) override; // +0x08
    virtual bool init();                             // +0x0C, 0x756CE0; widget/owner dependencies remain
    // Names are [INFERRED] except refresh/setPaint/setUpdate, attested by the
    // console descriptors. PE callers establish the value/reference conventions.
    virtual bool onLevelLoaded();                     // +0x10, 0x751B70; configureMap dependency open
    virtual bool onLevelUnloaded();                   // +0x14
    virtual bool refresh();                          // +0x18, 0x5A5E20, recovered retail true
    virtual void paint();                            // +0x1C
    virtual void setPaint(bool enabled);              // +0x20
    virtual void setUpdate(bool enabled);             // +0x24
    virtual bool update(float dt, world::IPlayer*, Vec3 lookDirection); // +0x28
    virtual menu::Ref2System* getSystem();             // +0x2C
    virtual bool hasActiveMenu();                     // +0x30, 0x7513A0; higher mode slots open
    virtual void getString(meme::Data*, std::string& out); // +0x34
    virtual std::wstring getLocalizedString(std::string key); // +0x38
    // Distinct inferred names preserve slot order: MSVC groups/reverses overloads.
    virtual std::wstring replaceWideToken(std::wstring text, std::string token,
                                         std::wstring replacement); // +0x3C
    virtual std::wstring replaceWideTokenFromString(std::wstring text, std::string token,
                                                   std::string replacement); // +0x40
    virtual std::wstring replaceWideTokenFromInt(std::wstring text, std::string token,
                                                int replacement); // +0x44
    virtual std::wstring replaceStringTokenFromInt(std::string text, std::string token,
                                                  int replacement); // +0x48
    virtual std::wstring replaceStringToken(std::string text, std::string token,
                                            std::string replacement); // +0x4C
    virtual std::string toNarrowString(const std::wstring& text); // +0x50
    virtual std::wstring toWideString(const std::string& text);   // +0x54
    virtual std::wstring replaceWideInputKeys(std::wstring text); // +0x58
    virtual std::string replaceInputKeys(std::string text);       // +0x5C
    // Icon getters answer meme texture handles (meme::TextureHandle, int on x86).
    virtual std::intptr_t getPlayerVehicleIcon(world::IPlayer*, bool typeIcon);
    virtual std::intptr_t getPlayerKitIcon(world::IPlayer*, bool mapIcon, bool spotted);
    virtual std::intptr_t getPlayerRankIcon(world::IPlayer*); // +68 0x74F8D0; root texture dependency
    virtual std::intptr_t getVehicleTypeIcon(int type); // +6C 0x74F980; root texture dependency
    virtual world::ICameraObject* getCameraObject(int team);
    virtual int getPlayerVehicleType(world::IPlayer*);
    virtual void showRankUpIcon(std::string path, float seconds);
    virtual void showMedalIcon(std::string path, float seconds); // +7C 0x74F6B0
    virtual void showWideHelp(std::wstring text, float seconds);
    virtual void showHelp(std::string text, float seconds);
    // [INFERRED] Names and reference spelling; PE passes four float addresses.
    // Distinct names preserve MSVC virtual order. Both selectors return void.
    virtual void selectBoolColor(bool value, bool comparison,
        float& r, float& g, float& b, float& a,
        float unequalR, float unequalG, float unequalB, float unequalA,
        float equalR, float equalG, float equalB, float equalA);
    virtual void selectFloatColor(float value, float comparison,
        float& r, float& g, float& b, float& a,
        float otherR, float otherG, float otherB, float otherA,
        float lessR, float lessG, float lessB, float lessA); // +8C 0x74F4F0
    virtual world::Kit* getPlayerKit(world::IPlayer*);
    virtual world::ISoldier* getPlayerSoldier(world::IPlayer*);
    virtual bool isPlayerOnFoot(world::IPlayer*);
    virtual int getLocalPlayerTeam();                 // +9C 0x74F250
    virtual void ignorePlayer(int id);                // +A0 0x7B09D0
    virtual void unignorePlayer(int id);              // +A4 0x7B06A0
    virtual bool isPlayerIgnored(int id);             // +A8 0x7B05A0
    virtual std::string listIgnoredPlayers();         // +AC 0x7B05D0, by-value result
    virtual void toggleSayAll(bool alternateInput);   // +B0 0x7B3360
    virtual void toggleSayTeam(bool alternateInput);  // +B4 0x7B33F0
    virtual void toggleSaySquad(bool alternateInput); // +B8 0x7B3480
    virtual bool closeChat();                        // +BC 0x7B3E20
    virtual void outputChatMessage(std::string text); // +C0 0x7B0390, by-value argument
    virtual bool isChatActive();                     // +C4 0x7518D0
    virtual bool isSayAllActive();                   // +C8 0x751900, inferred name
    virtual bool isSayTeamActive();                  // +CC 0x751910, inferred name
    virtual bool isSaySquadActive();                 // +D0 0x751920, inferred name
    // [INFERRED] Method names unless also exposed by console commands.
    virtual bool processChatInput(io::GameInput&, int ignoredCharacter, meme::BfEditData*); // +D4 7B3F40
    virtual void sendSayAllText(std::string text);     // +D8 7B3D00
    virtual bool sendSayAll();                       // +DC 7B3530
    virtual void sendSayTeamText(std::string text);    // +E0 7B3D60
    virtual bool sendSayTeam();                      // +E4 7B37C0
    virtual void sendSaySquadText(std::string text);   // +E8 7B3DC0
    virtual bool sendSaySquad();                     // +EC 7B3A20
    virtual bool getChatFlag089();                   // +F0 751930; ctor leaves this byte untouched
    virtual void tickChatHook(float dt);             // +F4 5AB2A0: complete retail empty body; update756328 passes dt
    virtual void enableVOHelpMessage(int id);         // +F8 751320
    virtual void disableVOHelpMessage(int id);        // +FC 751360
    virtual void showDisconnectMessage(std::wstring text); // +100 7AF310, once until cleared
    virtual void clearDisconnectMessage();
    virtual void showConnectionMessage(std::wstring text);
    virtual void clearConnectionMessage();           // +10C 7AF3F0
    virtual void clearDeathMessage();
    virtual void setMenuLoading(bool loading);
    virtual bool isMenuLoading();
    virtual void addGlobalChat(std::wstring text, world::IPlayer*, int team); // +11C 7AF4A0
    virtual void addTeamChat(std::wstring text, world::IPlayer*, int team);
    virtual void addSquadChat(std::wstring text, world::IPlayer*, int team);
    virtual void addRadioChat(std::wstring text, world::IPlayer*, int team, int alternateCategory);
    virtual void addCommanderChat(std::wstring text, world::IPlayer*, int team); // +12C 7AF720
    virtual void addServerChat(std::wstring text, int team);
    virtual void addServerTeamChat(std::wstring text, int team);
    virtual void sendKitMessage();
    virtual void sendSpottedMessage(bool announce, world::VehicleType type); // +13C 7B0EC0, descriptor76F1F2
    virtual void refreshChatCategory(int category);
    virtual void setSpottedObjectId(unsigned short id);
    virtual void setSpottedPosition(Vec3 position);   // +148 751F50; caller77B98F copies three floats
    virtual bool findSpottedTarget(world::IObject*& object, Vec3& position, float distance, bool vehiclePosition); // +14C 755870
    virtual bool isWithinAimThreshold(world::IObject*, float threshold);
    virtual void setAimThreshold(float threshold);   // +154 751980, console75DD4A
    virtual void callForMedic();
    virtual void callForRevive();                    // +15C 7AFE00
    virtual void callForAmmo();
    virtual void callForRepair();
    virtual bool isPointerEnabled();
    virtual void enablePointer(std::intptr_t texture, int mode); // +16C 7510C0; picture/descriptor dependencies remain
    virtual void disablePointer();
    virtual bool setPointerSize(int width, int height);
    virtual bool setMouseTextureEmpty(std::string);
    virtual bool setMouseTextureFull(std::string);   // +17C 750F80
    virtual bool setMouseTextureArtillery(std::string);
    virtual bool setMouseTextureUAV(std::string);
    virtual bool setMouseTextureSupply(std::string);
    virtual bool setMouseTextureCancel(std::string); // +18C 751080
    virtual bool setCommMouseSensitivity(int value);
    virtual bool setPointerMouseSensitivity(int value);
    virtual bool setCommPos(int x, int y);
    virtual bool setCommSize(int width, int height); // +19C 751EB0
    virtual bool setCommMousePos(int x, int y);       // +1A0 751ED0
    virtual bool setSpottedMenuPos(int x, int y);     // +1A4 751EF0
    virtual bool setSpottedMenuSize(int width, int height); // +1A8 751F10
    virtual bool setSpottedMousePos(int x, int y);    // +1AC 751F30
    virtual void setMaximumNrOfCPs(int value);        // +1B0 7519A0
    virtual int getMaximumNrOfCPs();                 // +1B4 5FFEE0
    virtual void handleGameInput(float dt, io::GameInput&); // +1B8 752B00; engine4049E6 returns true separately
    virtual void setDone(bool done);                 // +1BC 74F180
    virtual bool getPaint();                         // +1C0 7519B0
    virtual bool getUpdate();                        // +1C4 7519C0
    virtual meme::BfEditData* getEditData(int selector); // +1C8 7AF130
    virtual meme::BfListBoxData* getListData(int selector); // +1CC 7AF170; native widget owners
    // Concrete payloads are StringData; Data* return spelling [INFERRED] from
    // the common Data interface consumed by callers, with the same base address.
    virtual meme::Data* getStringData(int selector);  // +1D0 7AF2D0
    virtual meme::BfOccupiedVehicleData* getOccupiedVehicleData(int selector); // +1D4 74F160
    virtual void toggleMode(int first, int second);   // +1D8 74FF40
    virtual void setMode(int mode, bool savePrevious); // +1DC 74FD50, adapter786260
    virtual int getMode();                           // +1E0 751A00
    virtual int getPreviousMode();                   // +1E4 751A10
    virtual bool unknown1E8();                       // +1E8 74FDA0; discards getMode, then true
    virtual bool isCommanderShown();                 // +1EC 74FDD0
    virtual bool wasCommanderMode();                 // +1F0 74FE00
    virtual bool isCommanderMode(int mode);           // +1F4 74FE40
    virtual bool isScoreboardMode();                  // +1F8 74FDB0
    virtual bool isOrderMode();                       // +1FC 74FED0
    virtual bool activateRadio(HudRadio*);
    virtual void openRadio(bool alternate);
    virtual void openSquadRadio(bool alternate);
    virtual bool updateRadioMode();                  // +20C 750020
    virtual void resetRadio(HudRadio*);
    virtual void setSpottedMenuActive();
    virtual HudQuestionManager* getQuestionManager();
    virtual Bf2HudBuilder* getBuilder();              // +21C 504BC0
    virtual HudScoreboard* getScoreboard();
    virtual HudVoip* getVoip();
    virtual HudVoteMap* getVoteMap();
    virtual HudRadio* getSquadRadio();                // +22C 751A40
    virtual HudRadio* getVehicleRadio();
    virtual HudRadio* getInfantryRadio();             // +234 field108
    virtual HudRadio* getSquadLeaderRadio();          // +238 field114
    virtual HudRadio* getCommanderRadio();            // +23C field118
    virtual HudKitSelection* getKitSelection();       // +240 field11C
    virtual HudInformationAdapter* getInformationAdapter();
    // Final suffix: console types, event enums, aggregate argument widths and
    // constructor-backed owners verified. Input reference/const spelling and
    // names without descriptors are [INFERRED]. Unwritten bodies
    // below remain declaration-only; this complete table is still unpublished.
    virtual HudAmmo* getAmmo();                   // +248 751A70 ->12C, ctor7A5B10
    virtual HudVehicleInformation* getVehicleInformation(); // +24C 751A80 ->130, ctor78C560
    virtual HudMap* getMap();                     // +250 504B90 ->F8, builder's real map owner
    virtual HudSquad* getSquad();                 // +254 504F30 ->138, ctor791260
    virtual HudCommander* getCommander();         // +258 751A90 ->13C, ctor794700
    virtual HudMessages* getMessages();           // +25C 504C60 ->148, ctor79E0E0
    virtual bool getFlag229();                    // +260 751B00 byte229
    virtual bool getFlag22A();                    // +264 751B10 byte22A
    virtual void setFlagE7(bool);                 // +268 751AB0; HUD calls759C6C/78F6AE/7B120E discard result
    virtual bool isSquadRenameActive();           // +26C 751AC0 byteE5, input filter tests AL
    virtual void setSquadRenameActive(bool);      // +270 751AD0; 7B12E9 forwards boolean arg
    virtual bool isSquadCreateActive();           // +274 5C1620 byteE4, input filter tests AL
    virtual void setSquadCreateActive(bool);      // +278 751AE0; 7B11C7/7B1202 push0/1 and discard
    virtual std::string getSquadText();           // +27C 4B1960, string copy fromC0, hiddenresult ret4
    virtual void setFlagE6(bool);                 // +280 751AF0;78F675 forwardsboolean/discards
    virtual void setSquadEditText(std::string);   // +284 7B13A0, owned by-value string; cursor strlen
    virtual std::string getSquadEditText();       // +288 7B1410, HUDgetString(DC,C0), thenCOPY C0
    virtual void submitSquadCreate();            // +28C 7B15B0: createSquad(C0), closeChat(), C0=""
    virtual void prepareSquadCreate(bool);       // +290 7B1150, bytearg input 759B89/78F65C
    virtual void createSquad(std::string);       // +294 7B15F0, ownedbyvalue; empty name uses default
    virtual void submitSquadRename();            // +298 7B1340, C0 string to squadmanager
    virtual void prepareSquadRename(bool);       // +29C 7B1240: test BL,BL at7B1259 (not fullword)
    virtual void leaveSquad();                   // +2A0 7B1520, console76D3BF returnvoid
    virtual int findEmptySquad(int team);        // +2A4 7B1750, indices1..9, returnsfirstempty or-1
    virtual void sendRequestForOrder();          // +2A8 7AFE90, console76CB0D returnvoid
    virtual void sendRequestEvent(int team, int squad, RequestEvent::RequestTypes);
                                                 // +2AC 7AFF10; see decisive evidence below
    virtual void commanderApply();              // +2B0 7B1C50, console76CB7D returnvoid
    virtual void commanderResign();              // +2B4 7B1ED0, console76CBED returnvoid
    virtual void commanderMutiny();              // +2B8 7B1F50, console76CC5D returnvoid
    virtual void artillery();                    // +2BC 7B22C0, console76CFE5 returnvoid
    virtual bool launchArtillery(bool silent);   // +2C0 7B2490, explicit AL true/false
    virtual bool satellite(bool silent);         // +2C4 7B2BC0, console76D055 bool; wrapper PUSH0!
    virtual bool supplies(bool silent);          // +2C8 7B2860, console76D155 bool; wrapper PUSH0!
    virtual bool uav(bool silent);               // +2CC 7B29D0, console76D0D5 bool; wrapper PUSH0!
    virtual bool vehicleDrop(bool silent);       // +2D0 7B2660, console76D1D5 bool; wrapper PUSH0!
    virtual bool isVehicleDropPositionValid(const Vec3& position); // +2D4 7B3230
    virtual void toggleArtilleryState();          // +2D8 7B1F60, console76CE25 void
    virtual void toggleUAVState();                // +2DC 7B2060, console76CE95 void
    virtual void toggleSupplyState();             // +2E0 7B2180, console76CF05 void
    virtual void toggleVehicleDropState();        // +2E4 7B2220, console76CF75 void
    virtual void accept(bool accepted);          // +2E8 7B2360, console76D255 void/bool
    virtual bool setCommanderCameraPosition(Vec2 position); // +2EC 7B2D20, by-value eight bytes
    virtual bool zoomCommander();                // +2F0 7B23B0, console76D341 bool
    virtual Vec2 getCommanderPosition(bool relative); // +2F4 7B2E00, sret8 + bytebool
    virtual Vec3 screenToWorld(Vec2 position);    // +2F8 7B2EB0, sret12 + by-value8
    virtual Vec3 worldToScreen(Vec3 position);    // +2FC 7B3080, sret12 + by-value12
    virtual void setSatelliteEnabled(bool);       // +300 7B1BC0, byte2C0 store
    virtual bool getSatelliteEnabled();           // +304 7B1BD0, byte2C0 load
    virtual void setUAVEnabled(bool);             // +308 7B1BE0, byte2C2 store
    virtual bool getUAVEnabled();                 // +30C 7B1BF0, byte2C2 load
    virtual void setArtilleryEnabled(bool);        // +310 7B1C00, byte2C1 store
    virtual bool getArtilleryEnabled();            // +314 7B1C10, byte2C1 load
    virtual void setSatelliteAvailability(float); // +318 4BE270, field2C4 bits
    virtual float getSatelliteAvailability();     // +31C 4BE260, FLD field2C4
    virtual void setArtilleryAvailability(float); // +320 4BE290, field2C8 bits
    virtual float getArtilleryAvailability();     // +324 7B1C20, FLD field2C8
    virtual void setUAVAvailability(float);       // +328 4BE2B0, field2CC bits
    virtual float getUAVAvailability();           // +32C 7B1C30, FLD field2CC
    virtual void setVehicleDropAvailability(float); // +330 4BE2D0 field2D0;782990 event74/75
    virtual float getVehicleDropAvailability();  // +334 7B1C40, FLD field2D0
    virtual void updateOrderPositionColor(const Vec3& position); // +338 74FFC0; modifies350/354/358/35C
    virtual void sendOrder(int type, bool);       // +33C 7B1790, console76C8A9/76CCD9 void,int,bool
    virtual void sendRequest(int type, bool);     // +340 7B19D0, console76C9C1 void,int,bool
    virtual void setSquadOrder(const SquadOrder&); // +344 7554F0, exact48-byte typed member copy
    virtual void toggleSquadCommander();          // +348 7B1440, caller788E1B discards
    virtual bool isPlayerAlive(world::IPlayer*);  // +34C 751520, player->soldier->component+5C ==1
    virtual void onManDown();                    // +350 751460, no returnvalue materialized
    virtual void onSpawn();                      // +354 751560, no returnvalue materialized
    virtual float getCommanderCameraHeight();    // +358 7513E0, cached2BC, OverheadCameraTemplate+238
    virtual void queueConsoleCommand(const std::string&); // +35C 755E20, native list<string>::push_back
    virtual void executeQueuedConsoleCommands(); // +360 755E60, live iteration then clear
    virtual void playRoundResultSound();         // +364 751650, localplayer->team and winner, soundmanager
    virtual void showRoundResult();              // +368 751720, localized win/draw display
    virtual void addTextureAtlas(const std::string&); // +36C 755F00, console75E3D5 void,string
    virtual void addFavouriteServer(bool);       // +370 751B50, console75E955 void,bool
    virtual const std::list<std::string>& getAtlasList(); // +374 756970, borrowed address33C
    void handleEvent(EventCategory, unsigned int, EventNode*, void*) override;
    // [INFERRED] Enters squad-create mode, focuses the name edit box and clears queued console keys.
    void selectSquadCreateName();

private:
    friend struct BF2HudOrdersFixture;
    bool registerHudEvents();                        // 0x74E090; [INFERRED] name
    bool registerHudFunctions();                     // 74DC90; explicit AL=1
    bool registerHudData();                          // 74DD10; explicit AL=1
    void cacheMouseInput(bool button);               // 0x74F0C0; [INFERRED] name
    void respondToRequest(bool accepted, int squad, int orderType, int requestType); // 7AFFE0, inferred name/int spelling
    void sendSquadReply(int squad, std::string message, bool announce); // 7AFAE0, inferred name; owned string
    void configureMap(const std::string& mapName);    // 74FB70, map texture setup
    void resetHudState();
    // Category dispatchers are recovered; their named leaf dependencies remain open.
    void handleHudEvent(unsigned int, EventNode*, void*);
    void applyModeTransition(); // 750510 [FRONTIER], inferred name
    void announceSquadOrder(SquadOrder*, int squad); // 7AF960, BF2HudManagerOrders.cpp
    void announceSquadRequest(int squad, int type); // 7B0D40, BF2HudManagerOrders.cpp
    // The three helpers announceSquadRequest calls - [INFERRED] names, all __thiscall members
    // (ecx = this) and none a vtable slot. BF2HudManagerOrders.cpp.
    bool isRequestAvailable(int type);               // 7AFB50, `retn 4`
    std::wstring getRequestName(int type);           // 7AFBE0, `retn 8` (hidden return slot)
    void deferSquadRequest(int squad, int type);     // 7B0C90, `retn 8` - m_field32c.push_back
    void handleGameEvent(unsigned int, EventNode*, void*); // 753B10, recovered
    void handleGameLogicEvent(unsigned int, EventNode*, void*);
    void updateMapPosition(world::IPlayer*, Vec3);    // 751C10, map position/heading consumer still open
    void updateModeWidgets(float);
    void updateNotifications(float);
    void updateDemoHelp(float);
    bool updatePendingRequests(float);
    bool isVehicleDropPositionBlocked(const Vec3&);   // 755740, opposite result to public7B3230

    // [UNRESOLVED] Member identities/types except the STL containers, measured
    // float/byte stores, and the blink owner. Raw words preserve unknown scalar
    // storage; they are not fake interfaces and must be typed before later use.
    std::uint32_t m_field008, m_field00c;               // untouched by ctor
    std::wstring m_field010;
    std::uint32_t m_field02c[3];                       // untouched
    std::wstring m_field038;
    std::uint32_t m_field054[3];                       // untouched
    std::wstring m_field060;
    std::uint32_t m_field07c;                          // untouched
    unsigned char m_field080;
    bool m_field081;                                  // chat focus gate; ctor leaves untouched
    unsigned char m_field082[2];                      // untouched
    int m_field084;
    bool m_field088;
    bool m_field089;                                  // untouched; getter751930
    std::string m_field08c;
    meme::Data* m_field0a8[2];                        // untouched; StringData owners in74DD10
    bool m_field0b0, m_field0b1, m_field0b2;
    meme::BfEditData* m_field0b4[2];                   // untouched; constructors74DE49/74DE6D
    bool m_field0bc, m_field0bd, m_field0be, m_field0bf;
    std::string m_field0c0;
    meme::Data* m_field0dc;                           // untouched; StringData
    meme::BfEditData* m_field0e0;                      // untouched; ctor74DE91
    bool m_field0e4, m_field0e5, m_field0e6, m_field0e7;
    std::list<int> m_field0e8;                         // ignored player ids, 0x7B09D0/0x7B05D0
    menu::Ref2System* ref2System;
    HudMap* hudMap;
    Bf2HudBuilder* m_field0fc; // init 756CE0 -> 794E30; direct destruction 756A60
    HudQuestionManager* m_field100;
    HudInformationAdapter* m_field104;
    HudRadio* m_field108;
    HudRadio* m_field10c;
    HudRadio* m_field110;
    HudRadio* m_field114;
    HudRadio* m_field118;
    HudKitSelection* m_field11c;
    HudScoreboard* m_field120;
    HudVoip* m_field124;
    HudVoteMap* m_field128;
    HudAmmo* hudAmmo;
    HudVehicleInformation* hudVehicleInformation;
    HudLoadingScreen* m_field134; // init 757128 -> 7AA5A0; nonvirtual owner
    HudSquad* m_field138;
    HudCommander* m_field13c;
    HudChat* m_field140;
    HudChat* hudChat;
    HudMessages* m_field148;
    HudInitOwner* m_field14c;
    int m_field150;
    float m_field154, m_field158;
    bool m_field15c;
    std::uint32_t m_field160;                          // untouched
    // Pointer texture handles; uintptr_t is uint32_t on x86.
    std::uintptr_t m_field164, m_field168, m_field16c, m_field170, m_field174;
    std::uintptr_t m_field178;                         // untouched by the constructor
    int m_field17c, m_field180;
    std::uintptr_t m_field184, m_field188;             // faded/death icon handles
    std::intptr_t m_field18c[22];                     // rank texture handles, 0x74F8D0
    unsigned short m_field1e4;                        // networkable id, setter751B20
    Vec3 m_field1e8;                                  // spotted position, setter751F50
    float m_field1f4;
    float m_field1f8, m_field1fc;
    float m_field200, m_field204, m_field208, m_field20c;
    float m_field210, m_field214;
    float m_field218, m_field21c, m_field220, m_field224;
    bool m_field228, m_field229, m_field22a;
    float m_field22c;                                 // in-game help seconds
    bool m_field230;
    float m_field234, m_field238;                      // rank / medal seconds
    float m_field23c;                                 // 750277 FLD, replay-help timer
    bool m_field240, m_field241, m_field242;
    int m_field244;
    bool m_field248, m_field249, m_field24a, m_field24b, m_field24c, m_field24d;
    int m_field250, m_field254;                        // registered as HudState; previous mode
    meme::Bf2PictureNode* m_field258;                 // graphic owned through Pointer; 7510C0
    std::uint32_t m_field25c, m_field260;
    float m_field264, m_field268, m_field26c;
    SquadOrder m_order;                              // +270..29F; only points zeroed by ctor
    float m_field2a0, m_field2a4, m_field2a8, m_field2ac;
    bool m_field2b0;                                   // initial blink output
    float m_field2b4, m_field2b8, m_field2bc;            // maximum / source / unresolved
    bool m_field2c0, m_field2c1, m_field2c2;
    float m_field2c4, m_field2c8, m_field2cc;
    float m_field2d0, m_field2d4, m_field2d8;
    HudBlinkManager* m_field2dc;
    std::list<std::string> m_field2e0;
    int m_field2ec;
    ISound* m_field2f0;                               // saved engine trigger750C70; addRef/release+0/+4
    bool m_field2f4, m_field2f5;
    float m_field2f8;                                 // 753278 FLD; 756340 fstp (held-input elapsed time)
    bool m_field2fc;
    float m_field300;                                 // 753A56 fstp, saved player-input axis
    std::string m_field304;
    bool m_field320;
    int m_field324, m_field328;
    std::vector<HudPendingRequest> m_field32c;         // 7B0AF0/7E3B80:12-byte values, native vector
    std::list<std::string> m_field33c;
    std::uint32_t m_field348, m_field34c;
    float m_field350, m_field354, m_field358;
    bool m_field35c;
};

} } }
#endif
#endif
