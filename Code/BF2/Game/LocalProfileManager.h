// [INFERRED] path. Client class name is attested by its ModuleGame registry row.
// The primary prefix includes two explicitly approved abort-only placeholders;
// keep their abort behavior until the real signatures and bodies are established
// (local-profile-approved-opaque-slots).
#ifndef GAME_LOCALPROFILEMANAGER_H
#define GAME_LOCALPROFILEMANAGER_H
#include "../Core/IBase.h"
#include "IEventListener.h"
#include <string>
#include <vector>
// The interface is declared in every image (the dedicated AI reaches it through BFGame's menu
// arm, which no dedicated module can trigger); the implementation is client-only.
namespace dice { namespace hfe {

struct LocalProfile {
    LocalProfile(); // PE4A7520
    ~LocalProfile() = default; // six reverse-order string destructors:4A8060
    std::string name;        // retail+00 directory name; role spellings below inferred from4A8A30 literals
    std::string profileName; // retail+1C setName
    std::string nick;        // retail+38 setNick
    std::string gamespyNick; // retail+54 setGamespyNick
    std::string email;       // retail+70 setEmail
    std::string password;    // retail+8C setPassword
    float totalPlayedTime; // retail+A8, serialized4A8A30
    int numTimesLoggedIn;  // retail+AC, serialized4A8A30
};

struct LocalDemoBookmark {
    // Four native strings, retail112 bytes: serialization4A8A30 and destruction4A96A0.
    std::string field00, field1c, field38, field54;
};

class ILocalProfileManager : public IBase {
public:
    virtual bool hasDefaultProfile() = 0;
    virtual bool createDefaultProfile() = 0;
    virtual bool init() = 0;
    virtual bool login(int profileIndex, const std::string& unusedName,
                       const std::string& password) = 0; // PE+1C 4AA370; middle argument unused
    virtual void shutdown() = 0;             // PE+20 4A81F0; caller-discard void spelling
    virtual bool createProfile(const std::string& profileName, const std::string& nick) = 0;
    virtual bool deleteProfile(int profileIndex) = 0;
    virtual bool createOnlineAccount(const std::string& nick, const std::string& email,
                                     const std::string& password, const std::string& country) = 0; // PE+2C 4A7600
    // PE+30 4A7690: `cmp [ecx+0C],0; setne al; ret 4` - whether a profile is current; the one stack
    // argument is ignored (its type is unknown, a dword). Name [INFERRED].
    virtual bool hasCurrentProfile(int ignored) = 0;
    // PE+34 4553F0: the image's shared `mov al,1; ret 8` - true, both dword arguments ignored. Name
    // [INFERRED]; no caller in the client image.
    virtual bool slot34(int ignored, int ignored2) = 0;
    virtual bool hasProfile(const std::string& profileName) = 0;
    virtual bool loadCurrentProfile() = 0; // PE+3C 4AB1D0
    virtual bool saveCurrentProfile() = 0;
    virtual LocalProfile* getCurrentProfile() = 0;
    virtual bool getLocalProfiles(std::vector<LocalProfile>& output, bool validOnly) = 0;
    virtual std::string getProfilesPath() = 0; // PE+4C 4A76A0
    virtual bool findProfileDirectory(const std::string& profileName, std::string& directory) = 0;
    virtual bool loadProfileFile(const std::string& relativePath) = 0;
    virtual bool applySettings(bool audioOnly) = 0;
    virtual bool loginOnline(const std::string& name, const std::string& password, bool remember) = 0; // PE+5C 4AA500
    virtual bool loginOnlineAlternate(const std::string& name, const std::string& password, bool remember) = 0;
    virtual void setName(const std::string& value) = 0;
    virtual void setGamespyNick(const std::string& value) = 0;
    virtual void setEmail(const std::string& value) = 0; // PE+6C 4A78A0
    virtual void setPassword(const std::string& value, bool encoded) = 0;
    virtual void setNick(const std::string& value) = 0;
    virtual void setNumTimesLoggedIn(int value) = 0;
    virtual std::string getName() = 0; // PE+7C 4A7940
    virtual std::string getGamespyNick() = 0;
    virtual std::string getEmail() = 0;
    virtual std::string getPassword() = 0;
    virtual std::string getNick() = 0; // PE+8C 4A7A60
    virtual int getNumTimesLoggedIn() = 0;
    virtual bool hasDefaultVoiceSettings() = 0;
    virtual bool loadVoiceSettings() = 0;
    virtual bool saveVoiceSettings() = 0; // PE+9C 4A8390
    virtual bool readVoiceSettings(float& playback, float& capture, float& threshold, bool& boost) = 0; // PE+A0 4A7BC0
    virtual void addDemoBookmark(const std::string& field00, const std::string& field1c, const std::string& field38, const std::string& field54) = 0; // PE+A4 4AC030
    virtual unsigned int getNumDemoBookmarks() = 0; // PE+A8 4A84A0
    virtual LocalDemoBookmark* getDemoBookmark(unsigned int index) = 0; // PE+AC 4A9680
    virtual bool removeDemoBookmark(unsigned int index) = 0; // PE+B0 4ABA00
    virtual float getTotalPlayedTime() = 0; // PE+B4 4A7D10
    virtual void setTotalPlayedTime(float value) = 0; // PE+B8 4A7D30
    virtual bool validateProfileName(const std::string& name, std::string& error) = 0; // PE+BC 4A7D50
    virtual bool quickLogin(const std::string& name, const std::string& password) = 0; // PE+C0 4AAD00
    virtual bool saveGlobalSettings() = 0; // PE+C4 4A9440
    virtual bool validateProfileFiles(const std::string& directory) = 0; // PE+C8 4A8580
    // PE+CC 619360 `mov eax,[ecx+34h]; ret` and PE+D0 4A7DB0 (sret copy of this+38h, RET 4):
    // the profile index and password the last login stored. [INFERRED] names. The only known
    // caller is BF2Engine's activateMod pair (0x403E40 / 0x404010).
    virtual int getLoginProfileIndex() = 0;      // PE+CC 619360 (ICF-shared one-load getter)
    virtual std::string getLoginPassword() = 0;  // PE+D0 4A7DB0
    // [INFERRED] +A8/+AC/+B0 use unsigned indices/count and a borrowed pointer.
    // Full PE bodies/callers prove the x86 widths, arity and return conventions;
    // int-vs-unsigned and pointer-vs-reference source spelling are not attested.
    // PE+D4 4ABD10 is the scalar deleting destructor; no destructor slot is declared (see below).
};

#if !DEDICATED
struct LocalProfileFixture;
class LocalProfileManager final : public ILocalProfileManager, public IEventListener {
public:
    LocalProfileManager();                       // retail allocation88
    // [CHOSEN, not recovered] [RETIRE-WHEN: local-profile-prefix-publication]
    // The final owner is deleted directly by release.
    // No destructor slot is inserted where the untyped PE suffix belongs (+D4).
    ~LocalProfileManager();                      // actual body4AA280
    static IBase* create(unsigned int iid, IBase* outer); // factory4104FC
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    bool hasDefaultProfile() override;            // root-owned4AA310
    bool createDefaultProfile() override;         // root-owned4A80C0
    bool init() override;                         // root-owned4A8980
    bool login(int profileIndex, const std::string& unusedName, const std::string& password) override;
    void shutdown() override;                     // conditional external frontier
    bool createProfile(const std::string& profileName, const std::string& nick) override;
    bool deleteProfile(int profileIndex) override;
    bool createOnlineAccount(const std::string& nick, const std::string& email,
                             const std::string& password, const std::string& country) override;
    bool hasCurrentProfile(int ignored) override;
    bool slot34(int ignored, int ignored2) override;
    bool hasProfile(const std::string& profileName) override;
    bool loadCurrentProfile() override;
    bool saveCurrentProfile() override;
    LocalProfile* getCurrentProfile() override;
    bool getLocalProfiles(std::vector<LocalProfile>& output, bool validOnly) override;
    std::string getProfilesPath() override;
    bool findProfileDirectory(const std::string& profileName, std::string& directory) override;
    bool loadProfileFile(const std::string& relativePath) override;
    bool applySettings(bool audioOnly) override;
    bool loginOnline(const std::string& name, const std::string& password, bool remember) override;
    bool loginOnlineAlternate(const std::string& name, const std::string& password, bool remember) override;
    void setName(const std::string& value) override;
    void setGamespyNick(const std::string& value) override;
    void setEmail(const std::string& value) override;
    void setPassword(const std::string& value, bool encoded) override;
    void setNick(const std::string& value) override;
    void setNumTimesLoggedIn(int value) override;
    std::string getName() override;
    std::string getGamespyNick() override;
    std::string getEmail() override;
    std::string getPassword() override;
    std::string getNick() override;
    int getNumTimesLoggedIn() override;
    bool hasDefaultVoiceSettings() override;
    bool loadVoiceSettings() override;
    bool saveVoiceSettings() override;
    bool readVoiceSettings(float& playback, float& capture, float& threshold, bool& boost) override;
    void addDemoBookmark(const std::string& field00, const std::string& field1c, const std::string& field38, const std::string& field54) override;
    unsigned int getNumDemoBookmarks() override;
    LocalDemoBookmark* getDemoBookmark(unsigned int index) override;
    bool removeDemoBookmark(unsigned int index) override;
    float getTotalPlayedTime() override;
    void setTotalPlayedTime(float value) override;
    bool validateProfileName(const std::string& name, std::string& error) override;
    bool quickLogin(const std::string& name, const std::string& password) override;
    bool saveGlobalSettings() override;
    bool validateProfileFiles(const std::string& directory) override;
    int getLoginProfileIndex() override;
    std::string getLoginPassword() override;
    void handleEvent(EventCategory, unsigned int, EventNode*, void*) override; // actual5B74B0 RET16
    // [CHOSEN, not recovered] Non-virtual, no retail counterpart: once the renderer exists, a Default
    // profile created in this run takes the renderer-aware defaults (see LocalProfileManagerInit.cpp).
    void applyFirstStartVideoDefaults();
private:
    friend struct LocalProfileFixture;
    void reportProfileError(const std::string& message); // [INFERRED] name/caller-discard void; PE4B11A0
    bool writeProfileFile(const std::string& path, const std::string& contents); // [INFERRED] name; PE4A82F0
    bool setMapListPath(); // [INFERRED] name; PE4A84D0: mapList->m_configFile = <root><name>/mapList.con
    int m_refCount;                            // retail+08
    LocalProfile* m_currentProfile;             // retail+0C
    std::vector<LocalDemoBookmark> m_demoBookmarks; // retail+10, native STL
    bool m_hasDefaultProfile;                  // retail+20
    bool m_profilesDirty;                      // retail+21
    bool m_filterValidProfiles;                // retail+22
    std::vector<LocalProfile> m_profiles;        // retail+24
    int m_field34;                             // retail+34 login's profile index (+CC)
    std::string m_field38;                     // retail+38 login's password (+D0)
    bool m_field54;
};

#endif // !DEDICATED
} }
#endif
