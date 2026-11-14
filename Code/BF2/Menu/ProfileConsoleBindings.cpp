// [INFERRED] names/path. The profile store (BF2.exe 0x7BF950..0x7C1340) and the 17 profile console
// bindings (constructors 0x7702E5..0x771819, invokers at their tables' +0x5C).
#include "ProfileConsoleBindings.h"
#if !DEDICATED
#include "../Core/ConsoleObjectBase.h"
#include "../Core/ConsoleObjectMacro.h"
#include "../Game/IGameLogic.h"                 // gameLogic (ds:99EF80) +0x48 getTeamName
#include "../Game/Objects/IPlayer.h"
#include "../Scene/Object/PlayerManager.h"      // world::playerManager (ds:A08F60) +0x30
#include <cstdlib>
#include <sstream>
#include <type_traits>

namespace dice { namespace hfe { namespace hud {

ProfileConsoleOwner* profileConsoleOwner = nullptr;

ProfileConsoleOwner::ProfileConsoleOwner()
    : m_emptyName(""), m_selectedProfile(nullptr), m_activeProfile(nullptr)
{
    // a second owner does not replace the published first one.
    if (!profileConsoleOwner) profileConsoleOwner = this;
}

ProfileConsoleOwner::~ProfileConsoleOwner()
{
    // clear the singleton before native map/string destruction. The tree
    // erase7BFE80/7BFC10 destroys keys/nodes only, never mapped PlayerProfile*s.
    if (profileConsoleOwner == this) profileConsoleOwner = nullptr;
}


namespace {
const std::string defaultModName("BF2");   // A2215C, static initializer 86AE00
}

// ---- the owner ----

// 7BFDB0. The profile's id is the map size after the insertion; a duplicate name clears the selection.
bool ProfileConsoleOwner::createPlayerProfile(const std::string& name)
{
    std::pair<std::map<std::string, PlayerProfile*>::iterator, bool> inserted =
        m_profiles.insert(std::make_pair(name, static_cast<PlayerProfile*>(nullptr)));
    if (!inserted.second) {
        m_selectedProfile = nullptr;
        return false;
    }
    PlayerProfile* profile = new PlayerProfile(name, static_cast<int>(m_profiles.size()));
    inserted.first->second = profile;
    m_selectedProfile = profile;
    return true;
}

// 7BF950: the selected profile's name, or the owner's empty string.
const std::string& ProfileConsoleOwner::getActivePlayerProfile() const
{
    return m_selectedProfile ? m_selectedProfile->getName() : m_emptyName;
}

void ProfileConsoleOwner::setActivePlayerProfile(const std::string& name)    // 7BFC60
{
    std::map<std::string, PlayerProfile*>::iterator found = m_profiles.find(name);
    m_selectedProfile = found != m_profiles.end() ? found->second : nullptr;
}

bool ProfileConsoleOwner::activatePlayerProfile(const std::string& name)     // 7BFCA0
{
    std::map<std::string, PlayerProfile*>::iterator found = m_profiles.find(name);
    if (found == m_profiles.end()) {
        m_activeProfile = nullptr;
        return false;
    }
    m_activeProfile = found->second;
    m_activeProfile->activateFirstCharacterProfile();
    return true;
}

// ---- PlayerProfile ----

PlayerProfile::PlayerProfile(const std::string& name, int id)                // 7C0530
    : m_name(name), m_id(id), m_rank(-1), m_selectedCharacter(nullptr), m_activeCharacter(nullptr)
{
}

// 7C0440, the owner's insertion pattern one level down.
bool PlayerProfile::addCharacterProfile(const std::string& name)
{
    std::pair<std::map<std::string, CharacterProfile*>::iterator, bool> inserted =
        m_characters.insert(std::make_pair(name, static_cast<CharacterProfile*>(nullptr)));
    if (!inserted.second) {
        m_selectedCharacter = nullptr;
        return false;
    }
    CharacterProfile* character = new CharacterProfile(name, static_cast<int>(m_characters.size()));
    inserted.first->second = character;
    m_selectedCharacter = character;
    return true;
}

bool PlayerProfile::setActiveCharacterProfile(const std::string& name)       // 7C02C0
{
    std::map<std::string, CharacterProfile*>::iterator found = m_characters.find(name);
    if (found == m_characters.end()) {
        m_selectedCharacter = nullptr;
        return false;
    }
    m_selectedCharacter = found->second;
    return true;
}

// 7C0300: activation switches the character to the "BF2" settings (the literal 935D84, not the
// A2215C string) and applies them.
bool PlayerProfile::activateCharacterProfile(const std::string& name)
{
    std::map<std::string, CharacterProfile*>::iterator found = m_characters.find(name);
    if (found == m_characters.end()) {
        m_activeCharacter = nullptr;
        return false;
    }
    m_activeCharacter = found->second;
    m_activeCharacter->setActiveMod(std::string("BF2"));
    m_activeCharacter->applySettings();
    return true;
}

// 7C0510: activates the first character by name.
// [CHOSEN, not recovered] With no characters retail reads the map head's unconstructed value; an
// empty name is passed here instead, which finds nothing and clears the active character.
void PlayerProfile::activateFirstCharacterProfile()
{
    activateCharacterProfile(m_characters.empty() ? std::string() : m_characters.begin()->second->getName());
}

// ---- CharacterProfile ----

// 7C1270: one setting under the A2215C name, made active.
CharacterProfile::CharacterProfile(const std::string& name, int id)
    : m_name(name), m_id(id), m_activeMod(nullptr)
{
    ModSetting* setting = new ModSetting(defaultModName);
    m_mods.insert(std::make_pair(defaultModName, setting));
    m_activeMod = setting;
}

// 7C1340: a new setting becomes active; a duplicate name leaves everything as it was.
bool CharacterProfile::addModSetting(const std::string& name)
{
    std::pair<std::map<std::string, ModSetting*>::iterator, bool> inserted =
        m_mods.insert(std::make_pair(name, static_cast<ModSetting*>(nullptr)));
    if (!inserted.second) return false;
    ModSetting* setting = new ModSetting(name);
    inserted.first->second = setting;
    m_activeMod = setting;
    return true;
}

void CharacterProfile::setActiveMod(const std::string& name)                 // 7C0B00
{
    std::map<std::string, ModSetting*>::iterator found = m_mods.find(name);
    if (found != m_mods.end()) m_activeMod = found->second;                  // unknown: unchanged
}

// 7C1080: the active setting's per-kit map takes key -> index (an existing key is overwritten; an
// unknown kit gets a new map holding that one entry).
void CharacterProfile::setIndexForKit(const std::string& kit, int index, int key)
{
    m_activeMod->m_kitIndices[kit][key] = index;
}

int CharacterProfile::getIndexForKit(const std::string& kit, int key)        // 7C0B30
{
    std::map<std::string, std::map<int, int> >::iterator found = m_activeMod->m_kitIndices.find(kit);
    if (found == m_activeMod->m_kitIndices.end()) return 0;
    std::map<int, int>::iterator entry = found->second.find(key);
    return entry != found->second.end() ? entry->second : 0;
}

// ---- ModSetting ----

// 7C1200. +3C is not written: bf_malloc does not clear, so retail starts it with whatever the heap
// held. [CHOSEN, not recovered] It starts at 0 here.
ModSetting::ModSetting(const std::string& name)
    : m_name(name), m_visualUnlockLevel(0), m_defaultKit(0), m_tagIcon(""), m_tagText("")
{
}

// 7C09E0. Applies to the local human player: its name, then its kit. The team read and the walks
// over both teams' kit maps have empty bodies in retail; they are kept as shipped.
void ModSetting::applySettings()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    player->setName(m_playerName);                                           // +1C
    (void)player->getTeam();                                                 // +E4, result unused
    for (int team = 1; team <= 2; ++team) {
        std::map<std::string, std::map<int, int> >::iterator kits =
            m_kitIndices.find(gameLogic->getTeamName(team));                 // 99EF80 +48
        if (kits == m_kitIndices.end()) continue;
        for (std::map<int, int>::iterator it = kits->second.begin(); it != kits->second.end(); ++it) {
        }
    }
    player->setKit(m_defaultKit);                                            // +168
}

namespace {
ConsoleObjects::ConsoleObjectList profileBindings; // static initializer864291
SetConsoleClassDefaults profileDefaults("Profile", 2, 0);

enum class Activation { Owner, SelectedProfile, ActiveProfile, SelectedCharacter, ActiveCharacter };

PlayerProfile* selectedProfile()
{
    return profileConsoleOwner ? profileConsoleOwner->getSelectedProfile() : nullptr;
}
PlayerProfile* activeProfile()
{
    return profileConsoleOwner ? profileConsoleOwner->getActiveProfile() : nullptr;
}
CharacterProfile* selectedCharacter()
{
    PlayerProfile* profile = selectedProfile();
    return profile ? profile->getSelectedCharacter() : nullptr;
}
CharacterProfile* activeCharacter()
{
    PlayerProfile* profile = activeProfile();
    return profile ? profile->getActiveCharacter() : nullptr;
}

// [CHOSEN, not recovered] Shared metadata for the 17 profile console bindings; every row's
// invoker, converter and formatter is in the typed subclasses below.
class ProfileConsoleBinding : public ConsoleObjectBaseImpl {
public:
    ProfileConsoleBinding(const char* name, int type, int minArgs, int maxArgs,
        const char* returnType, Activation activation, bool dynamic,
        const char* arg0 = nullptr, const char* arg1 = nullptr, const char* arg2 = nullptr)
        : m_activation(activation)
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = name;
        m_type = type;
        m_minNoArgs = minArgs;
        m_maxNoArgs = maxArgs;
        m_returnType = returnType;
        m_hasReturnValue = false;
        m_isDynamic = dynamic;
        // The PE constructors append before initializing argument metadata.
        profileBindings.push_back(this);
        const char* arguments[] = {arg0, arg1, arg2};
        for (int i = 0; i < maxArgs; ++i) {
            m_argTypes[i] = arguments[i];
            m_argNeedsConversion[i] = std::string(arguments[i]) == "std::string" ? 0 : -1;
        }
        // Unused argument slots and execution state remain untouched.
    }
    bool isObjectActive() const override
    {
        switch (m_activation) {
        case Activation::Owner: return profileConsoleOwner != nullptr;
        case Activation::SelectedProfile: return selectedProfile() != nullptr;
        case Activation::ActiveProfile: return activeProfile() != nullptr;
        case Activation::SelectedCharacter: return selectedCharacter() != nullptr;
        case Activation::ActiveCharacter: return activeCharacter() != nullptr;
        }
        std::abort(); // impossible private enum value, not a retail availability branch
    }
    std::string getObjectDeltaAsString() const override { return "No Delta"; }
    // +54: every table holds the shared 4C4510.
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
private:
    Activation m_activation; // chosen metadata, not a claimed retail generated-class suffix
};

// One conversion slot per row and argument, as each retail class owns its static: strings keep one
// token, ints are read with operator>> (770724 and its twins).
template<class Arg, int Row, int Index>
std::string* convertArgument(const std::string& value)
{
    static Arg converted;
    std::istringstream input(value);
    input >> converted;
    return reinterpret_cast<std::string*>(&converted);
}
template<class Arg> Arg argumentValue(std::string* slot) { return *reinterpret_cast<Arg*>(slot); }
template<> std::string argumentValue<std::string>(std::string* slot) { return *slot; }

// Shared result formatters: 7611C7 (bool), 439A64 (int), 7709E2 (std::string); nothing until a
// getter or command has stored a result.
template<class Result>
std::string formatResult(bool hasResult, const Result& result)
{
    if (!hasResult) return std::string();
    std::ostringstream output;
    output << result;
    return output.str();
}

// The targets each invoker reaches through: the owner itself (no null test), or the 77025C / 77026E /
// 770280 / 770293 helpers above.
ProfileConsoleOwner* ownerTarget() { return profileConsoleOwner; }

// createPlayerProfile 770358, activatePlayerProfile 7705F3, addCharacterProfile 77082C,
// activateCharacterProfile 770B32, addModSetting 770C66: one string, the argument copied before the
// flag is raised, a bool result.
template<class Target, Target* (*Get)(), bool (Target::*Method)(const std::string&), int Row>
class ProfileCommandBinding final : public ProfileConsoleBinding {
public:
    using ProfileConsoleBinding::ProfileConsoleBinding;
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const std::string argument(*m_args[0]);
        m_hasReturnValue = true;
        m_result = (Get()->*Method)(argument);
    }
    std::string getReturnValueAsString() const override { return formatResult(m_hasReturnValue, m_result); }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) m_args[0] = convertArgument<std::string, Row, 1>(value);
    }
private:
    bool m_result; // +A4
};

// The get/set rows: no argument reads into +A4 and raises the flag, one argument clears the flag and
// sets. String rows copy the argument first; int rows read the converted slot.
template<class Value>
using ProfileSetArg = typename std::conditional<std::is_same<Value, int>::value, int, const std::string&>::type;
template<class Value>
using ProfileGetResult = typename std::conditional<std::is_same<Value, int>::value, int, const std::string&>::type;

// The setter's own result (7C02C0's bool) is dropped.
template<class Target, class Value, class SetResult, Target* (*Get)(),
         ProfileGetResult<Value> (Target::*Getter)() const, SetResult (Target::*Setter)(ProfileSetArg<Value>),
         int Row>
class ProfilePropertyBinding final : public ProfileConsoleBinding {
public:
    using ProfileConsoleBinding::ProfileConsoleBinding;
    void executeObjectMethod() override
    {
        if (m_argCount == 0) {
            m_hasReturnValue = true;
            m_result = (Get()->*Getter)();
        } else if (m_argCount == 1) {
            const Value value = argumentValue<Value>(m_args[0]);
            m_hasReturnValue = false;
            (Get()->*Setter)(value);
        }
    }
    std::string getReturnValueAsString() const override { return formatResult(m_hasReturnValue, m_result); }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) m_args[0] = convertArgument<Value, Row, 1>(value);
    }
private:
    Value m_result; // +A4
};

// setIndexForKit 771558 (string, int, int) and getIndexForKit 771726 (string, int): void rows whose
// call result is dropped; the flag is cleared before the call. Formatter 41BC76.
class ProfileSetIndexForKitBinding final : public ProfileConsoleBinding {
public:
    using ProfileConsoleBinding::ProfileConsoleBinding;
    void executeObjectMethod() override
    {
        if (m_argCount != 3) return;
        const std::string kit(*m_args[0]);
        const int index = argumentValue<int>(m_args[1]);
        const int key = argumentValue<int>(m_args[2]);
        m_hasReturnValue = false;
        selectedCharacter()->setIndexForKit(kit, index, key);
    }
    std::string getReturnValueAsString() const override { return std::string(); }
    void setArgFromString(int index, const std::string& value) override       // 7715A4
    {
        if (index == 1) m_args[0] = convertArgument<std::string, 14, 1>(value);
        if (index == 2) m_args[1] = convertArgument<int, 14, 2>(value);
        if (index == 3) m_args[2] = convertArgument<int, 14, 3>(value);
    }
};

class ProfileGetIndexForKitBinding final : public ProfileConsoleBinding {
public:
    using ProfileConsoleBinding::ProfileConsoleBinding;
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const std::string kit(*m_args[0]);
        const int key = argumentValue<int>(m_args[1]);
        m_hasReturnValue = false;
        selectedCharacter()->getIndexForKit(kit, key);                       // result dropped, as shipped
    }
    std::string getReturnValueAsString() const override { return std::string(); }
    void setArgFromString(int index, const std::string& value) override       // 77176C
    {
        if (index == 1) m_args[0] = convertArgument<std::string, 15, 1>(value);
        if (index == 2) m_args[1] = convertArgument<int, 15, 2>(value);
    }
};

// applySettings 771877: no arguments, the active character's settings. Converter 57DD1F, formatter 41BC76.
class ProfileApplySettingsBinding final : public ProfileConsoleBinding {
public:
    using ProfileConsoleBinding::ProfileConsoleBinding;
    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = false;
        activeCharacter()->applySettings();
    }
    std::string getReturnValueAsString() const override { return std::string(); }
    void setArgFromString(int, const std::string&) override {}
};

using S = std::string;
using O = ProfileConsoleOwner;
using P = PlayerProfile;
using C = CharacterProfile;

// CRT pointers 96D74C..96D78C prove this exact order. Constructors 7702E5..771819.
ProfileCommandBinding<O, ownerTarget, &O::createPlayerProfile, 0>
    createPlayerProfileBinding("createPlayerProfile", 0, 1, 1, "bool", Activation::Owner, true, "std::string");
ProfilePropertyBinding<O, S, void, ownerTarget, &O::getActivePlayerProfile, &O::setActivePlayerProfile, 1>
    activePlayerProfileBinding("activePlayerProfile", 1, 0, 1, "std::string", Activation::Owner, true, "std::string");
ProfileCommandBinding<O, ownerTarget, &O::activatePlayerProfile, 2>
    activatePlayerProfileBinding("activatePlayerProfile", 0, 1, 1, "bool", Activation::Owner, true, "std::string");
ProfilePropertyBinding<P, int, void, selectedProfile, &P::getRank, &P::setRank, 3>
    rankBinding("rank", 1, 0, 1, "int", Activation::SelectedProfile, true, "int");
ProfileCommandBinding<P, selectedProfile, &P::addCharacterProfile, 4>
    addCharacterProfileBinding("addCharacterProfile", 0, 1, 1, "bool", Activation::SelectedProfile, true, "std::string");
ProfilePropertyBinding<P, S, bool, selectedProfile, &P::getActiveCharacterProfile, &P::setActiveCharacterProfile, 5>
    activeCharacterProfileBinding("activeCharacterProfile", 1, 0, 1, "std::string", Activation::SelectedProfile, true, "std::string");
ProfileCommandBinding<P, activeProfile, &P::activateCharacterProfile, 6>
    activateCharacterProfileBinding("activateCharacterProfile", 0, 1, 1, "bool", Activation::ActiveProfile, true, "std::string");
ProfileCommandBinding<C, selectedCharacter, &C::addModSetting, 7>
    addModSettingBinding("addModSetting", 0, 1, 1, "bool", Activation::SelectedCharacter, true, "std::string");
ProfilePropertyBinding<C, S, void, selectedCharacter, &C::getActiveMod, &C::setActiveMod, 8>
    activeModBinding("activeMod", 1, 0, 1, "std::string", Activation::SelectedCharacter, true, "std::string");
ProfilePropertyBinding<C, S, void, selectedCharacter, &C::getPlayerName, &C::setPlayerName, 9>
    playerNameBinding("playerName", 1, 0, 1, "std::string", Activation::SelectedCharacter, true, "std::string");
ProfilePropertyBinding<C, int, void, selectedCharacter, &C::getVisualUnlockLevel, &C::setVisualUnlockLevel, 10>
    visualUnlockLevelBinding("visualUnlockLevel", 1, 0, 1, "int", Activation::SelectedCharacter, true, "int");
ProfilePropertyBinding<C, int, void, selectedCharacter, &C::getDefaultKit, &C::setDefaultKit, 11>
    defaultKitBinding("defaultKit", 1, 0, 1, "int", Activation::SelectedCharacter, true, "int");
ProfilePropertyBinding<C, S, void, selectedCharacter, &C::getTagIcon, &C::setTagIcon, 12>
    tagIconBinding("tagIcon", 1, 0, 1, "std::string", Activation::SelectedCharacter, true, "std::string");
ProfilePropertyBinding<C, S, void, selectedCharacter, &C::getTagText, &C::setTagText, 13>
    tagTextBinding("tagText", 1, 0, 1, "std::string", Activation::SelectedCharacter, true, "std::string");
ProfileSetIndexForKitBinding
    setIndexForKitBinding("setIndexForKit", 0, 3, 3, "void", Activation::SelectedCharacter, false, "std::string", "int", "int");
ProfileGetIndexForKitBinding
    getIndexForKitBinding("getIndexForKit", 0, 2, 2, "void", Activation::SelectedCharacter, false, "std::string", "int");
ProfileApplySettingsBinding
    applySettingsBinding("applySettings", 0, 0, 0, "void", Activation::ActiveCharacter, false);
} // namespace

ConsoleObjects::ConsoleObjectList& profileConsoleBindingList() { return profileBindings; }

} } }
#endif
