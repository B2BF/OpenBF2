// [INFERRED] path and shared class name beside the client HUD owners.
// Complete BF2.exe A1781C list: 228 constructors75C964..77007E and their
// static initialization order862D41..864291.
#include "HudConsoleBindings.h"
#if !DEDICATED
#include "BF2HudManager.h"
#include "Bf2HudBuilder.h"
#include "Bf2MenuMapNode.h"        // the nine BuilderMap rows act on the map itself
#include "HudCommander.h"          // the six CommanderMenu rows
#include "HudRadio.h"             // the five family R rows add comrose entries
#include "HudScoreboard.h"        // the two Scoreboard rows (setToggleShow / singleClick)
#include "HudSquad.h"             // the ten SquadMenu rows (189..198)
#include "HudVoteMap.h"           // the two LevelsList rows (225 / 226)
#include "HudKitSelection.h"      // the five SpawnManager rows (216..220)
#include "HudInformationAdapter.h" // hudItems.setBool (row 161)
#include "../Game/Main/Game.h"        // operator>>(istream&, TargetChannel&) - PE 442150
#include "../Core/ConsoleObjectBase.h"
#include "../Core/ConsoleObjectMacro.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <sstream>

namespace dice { namespace hfe {
extern hud::BF2HudManager* hudManager;
// Vehicle-type console reader. Keep this declaration consistent with the shared enum's int representation.
namespace world { std::istream& operator>>(std::istream&, VehicleType&); }
namespace hud {
namespace {
std::list<ConsoleObject*> bindings; // native list; initialized before all entries
enum class Availability {
    Manager, Map, Information, Messages, Builder, BuilderMap, Commander, Squad,
    SquadLeaderRadio, SquadRadio, CommanderRadio, InfantryRadio, VehicleRadio,
    Questions, KitSelection, Scoreboard, VoteMap
};
}

// [CHOSEN, not recovered] Consolidates generated classes' identical metadata
// and availability operations. The native base is real; the additional enum
// replaces source-generator specialization, never a retail object overlay.
// All unsupported invocations explicitly stop before conversion or execution.
class HudConsoleBinding : public ConsoleObjectBaseImpl {
public:
    HudConsoleBinding(const char* name, int type, int minArgs, int maxArgs,
        const char* result, bool dynamic, Availability availability,
        std::initializer_list<const char*> arguments)
        : m_availability(availability)
    {
        m_objectName = consoleDefaultName();
        m_functionName = name;
        m_type = type;
        m_isDynamic = dynamic;
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_minNoArgs = minArgs;
        m_maxNoArgs = maxArgs;
        m_returnType = result;
        m_hasReturnValue = false; // every PE constructor stores zero, even bool returns
        bindings.push_back(this); // 75C949 precedes argument-type/conversion stores
        int index = 0;
        for (const char* argument : arguments) m_argTypes[index++] = argument;
        for (int i = 0; i < maxArgs; ++i)
            m_argNeedsConversion[i] = std::string(m_argTypes[i]) == "std::string" ? 0 : -1;
    }

    // [CHOSEN, not recovered] Shared accessor for the builder's borrowed map node.
    static meme::Bf2MenuMapNode* builderMap()   // the +8 read at 75F586 and friends
    { return hudManager->getBuilder()->m_field008; }

    // Narrow accessors for commander reload settings used by generated invokers.
    static float commanderVehicleDropReloadTime()
    { return hudManager->getCommander()->m_vehicleDropDuration; }
    static void setCommanderVehicleDropReloadTime(float value)
    { hudManager->getCommander()->m_vehicleDropDuration = value; }

    bool isObjectActive() const override
    {
        // Only Manager checks the singleton itself. Other strategies require it, and BuilderMap also requires the builder.
        switch (m_availability) {
        case Availability::Manager: return hudManager != nullptr;
        case Availability::Map: return hudManager->getMap() != nullptr;
        case Availability::Information: return hudManager->getInformationAdapter() != nullptr;
        case Availability::Messages: return hudManager->getMessages() != nullptr;
        case Availability::Builder: return hudManager->getBuilder() != nullptr;
        case Availability::BuilderMap: return hudManager->getBuilder()->m_field008 != nullptr;
        case Availability::Commander: return hudManager->getCommander() != nullptr;
        case Availability::Squad: return hudManager->getSquad() != nullptr;
        case Availability::SquadLeaderRadio: return hudManager->getSquadLeaderRadio() != nullptr;
        case Availability::SquadRadio: return hudManager->getSquadRadio() != nullptr;
        case Availability::CommanderRadio: return hudManager->getCommanderRadio() != nullptr;
        case Availability::InfantryRadio: return hudManager->getInfantryRadio() != nullptr;
        case Availability::VehicleRadio: return hudManager->getVehicleRadio() != nullptr;
        case Availability::Questions: return hudManager->getQuestionManager() != nullptr;
        case Availability::KitSelection: return hudManager->getKitSelection() != nullptr;
        case Availability::Scoreboard: return hudManager->getScoreboard() != nullptr;
        case Availability::VoteMap: return hudManager->getVoteMap() != nullptr;
        }
        __assume(0); // all constructor-supplied enum values covered
    }

    std::string getObjectDeltaAsString() const override { return "No Delta"; }

    // [TEMP] [CHOSEN, not recovered] Unimplemented bindings stop before conversion or execution.
    // [RETIRE-WHEN: hud-console-invokers] Replace after every remaining invoker is implemented and validated.
    std::string execute(std::string*, int) override
    {
        std::fprintf(stderr, "[FRONTIER] dice::hfe::hud::HudConsoleBinding::execute: %s.%s\n", m_objectName, m_functionName);
        std::fflush(stderr);
        std::abort();
    }
    void executeObjectMethod() override
    {
        std::fprintf(stderr, "[FRONTIER] dice::hfe::hud::HudConsoleBinding::executeObjectMethod: %s.%s\n", m_objectName, m_functionName);
        std::fflush(stderr);
        std::abort();
    }
    void setArgFromString(int, const std::string&) override
    {
        std::fprintf(stderr, "[FRONTIER] dice::hfe::hud::HudConsoleBinding::setArgFromString: %s.%s\n", m_objectName, m_functionName);
        std::fflush(stderr);
        std::abort();
    }
    std::string getReturnValueAsString() const override
    {
        std::fprintf(stderr, "[FRONTIER] dice::hfe::hud::HudConsoleBinding::getReturnValueAsString: %s.%s\n", m_objectName, m_functionName);
        std::fflush(stderr);
        std::abort();
    }
private:
    Availability m_availability;
};

namespace {
// [CHOSEN, not recovered] Shared string-command template. Each specialization owns its conversion and result storage.
template<bool (BF2HudManager::*Method)(std::string)>
class HudStringBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* args, int count) override
    {
        return ConsoleObjectBaseImpl::execute(args, count);
    }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return; // Wrong arity preserves the previous result/flag.
        const std::string argument(*m_args[0]);
        m_hasReturnValue = true; // After local copy, before the second by-value copy/call.
        m_result = (hudManager->*Method)(argument); // No added singleton/null-argument guard.
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream result;
        result << m_result;
        return result.str();
    }
    void setArgFromString(int index, const std::string& value) override
    {
        // PE75CBFF/75CD47/75CE8F/75CFD7/75D11F/75D267. Normal execute keeps
        // strings raw; explicit conversion uses one token in per-command storage.
        if (index != 1) return;
        static std::string converted;
        std::istringstream input(value);
        input >> converted;
        m_args[0] = &converted;
    }
private:
    bool m_result; // Retail static object zero initialization, no constructor write.
};

// [CHOSEN, not recovered] Two-string builder binding with separate lazy conversion storage and result state.
template<bool (Bf2HudBuilder::*Method)(std::string, std::string)>
class HudBuilderStringPairBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const std::string first(*m_args[0]);
        const std::string second(*m_args[1]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static std::string first; // PE A110C0, initialized lazily
            std::istringstream input(value);
            input >> first;
            m_args[0] = &first;
        }
        if (index == 2) {
            static std::string second; // PE A110A4, independent lifetime
            std::istringstream input(value);
            input >> second;
            m_args[1] = &second;
        }
    }

    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }

private:
    bool m_result; // PE +A4, no constructor store
};

// [CHOSEN, not recovered] Builder binding with two strings and four integer arguments.
// Result selects the command's Boolean or integer return-storage type.
template<bool (Bf2HudBuilder::*Method)(std::string, std::string, int, int, int, int),
         class Result = bool>
class HudBuilderCreateNodeBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }

    void executeObjectMethod() override
    {
        if (m_argCount != 6) return;
        const std::string first(*m_args[0]);
        const std::string second(*m_args[1]);
        const int x = *reinterpret_cast<int*>(m_args[2]);
        const int y = *reinterpret_cast<int*>(m_args[3]);
        const int width = *reinterpret_cast<int*>(m_args[4]);
        const int height = *reinterpret_cast<int*>(m_args[5]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second, x, y, width, height);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1 || index == 2) {
            static std::string text[2]; // two independent lazy string objects
            std::istringstream input(value);
            input >> text[index - 1];
            m_args[index - 1] = &text[index - 1];
        } else if (index >= 3 && index <= 6) {
            static int number[4];
            std::istringstream input(value);
            input >> number[index - 3];
            m_args[index - 1] = reinterpret_cast<std::string*>(&number[index - 3]);
        }
    }

    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }

private:
    Result m_result; // PE +A4, no constructor store
};

// [CHOSEN] Console adapter for a node-creation method with two strings and four floats.
// Preserves the method's argument conversion order.
template<bool (Bf2HudBuilder::*Method)(std::string, std::string, float, float, float, float)>
class HudBuilderCreateSliderBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }

    void executeObjectMethod() override
    {
        if (m_argCount != 6) return;
        const std::string first(*m_args[0]);
        const std::string second(*m_args[1]);
        const float minimum = *reinterpret_cast<float*>(m_args[2]);
        const float maximum = *reinterpret_cast<float*>(m_args[3]);
        const float thumbSpan = *reinterpret_cast<float*>(m_args[4]);
        const float step = *reinterpret_cast<float*>(m_args[5]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second, minimum, maximum,
                                                       thumbSpan, step);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1 || index == 2) {
            static std::string text[2]; // two independent lazy string objects
            std::istringstream input(value);
            input >> text[index - 1];
            m_args[index - 1] = &text[index - 1];
        } else if (index >= 3 && index <= 6) {
            static float number[4];
            std::istringstream input(value);
            input >> number[index - 3];
            m_args[index - 1] = reinterpret_cast<std::string*>(&number[index - 3]);
        }
    }

    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }

private:
    bool m_result; // PE +A4, no constructor store
};

// [CHOSEN, not recovered] Builder bindings for extended node creation; optional Boolean fields retain separate conversion storage.
template<bool (Bf2HudBuilder::*Method)(std::string, std::string, int, int, int, int, int)>
class HudBuilderCreateBarBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 7) return;
        const std::string first(*m_args[0]);
        const std::string second(*m_args[1]);
        int numbers[5];
        for (int i = 0; i != 5; ++i) numbers[i] = *reinterpret_cast<int*>(m_args[i + 2]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second, numbers[0],
            numbers[1], numbers[2], numbers[3], numbers[4]);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1 || index == 2) {
            static std::string text[2];
            std::istringstream input(value);
            input >> text[index - 1];
            m_args[index - 1] = &text[index - 1];
        } else if (index >= 3 && index <= 7) {
            static int number[5];
            std::istringstream input(value);
            input >> number[index - 3];
            m_args[index - 1] = reinterpret_cast<std::string*>(&number[index - 3]);
        }
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

template<bool (Bf2HudBuilder::*Method)(std::string, std::string, int, int, int, int, int, bool, bool)>
class HudBuilderCreateCompassBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 9) return;
        const std::string first(*m_args[0]);
        const std::string second(*m_args[1]);
        int numbers[5];
        for (int i = 0; i != 5; ++i) numbers[i] = *reinterpret_cast<int*>(m_args[i + 2]);
        const bool snap = *reinterpret_cast<bool*>(m_args[7]);
        const bool wrap = *reinterpret_cast<bool*>(m_args[8]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second, numbers[0],
            numbers[1], numbers[2], numbers[3], numbers[4], snap, wrap);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1 || index == 2) {
            static std::string text[2];
            std::istringstream input(value);
            input >> text[index - 1];
            m_args[index - 1] = &text[index - 1];
        } else if (index >= 3 && index <= 7) {
            static int number[5];
            std::istringstream input(value);
            input >> number[index - 3];
            m_args[index - 1] = reinterpret_cast<std::string*>(&number[index - 3]);
        } else if (index == 8 || index == 9) {
            static bool flag[2];
            std::istringstream input(value);
            input >> flag[index - 8];
            m_args[index - 1] = reinterpret_cast<std::string*>(&flag[index - 8]);
        }
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE766AD6/766D0F executors; tables92A8C8/92A960. An integer index then a string.
template<bool (Bf2HudBuilder::*Method)(int, std::string)>
class HudBuilderIntStringBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const int index = *reinterpret_cast<int*>(m_args[0]);
        const std::string text(*m_args[1]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(index, text);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static int number;
            std::istringstream input(value);
            input >> number;
            m_args[0] = reinterpret_cast<std::string*>(&number);
        } else if (index == 2) {
            static std::string text;
            std::istringstream input(value);
            input >> text;
            m_args[1] = &text;
        }
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE767F50's executor; table92AF88. A boolean then a string.
template<bool (Bf2HudBuilder::*Method)(bool, std::string)>
class HudBuilderBoolStringBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const bool flag = *reinterpret_cast<bool*>(m_args[0]);
        const std::string text(*m_args[1]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(flag, text);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static bool flag;
            std::istringstream input(value);
            input >> flag;
            m_args[0] = reinterpret_cast<std::string*>(&flag);
        } else if (index == 2) {
            static std::string text;
            std::istringstream input(value);
            input >> text;
            m_args[1] = &text;
        }
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE767126's executor; table92AB30. One boolean.
template<bool (Bf2HudBuilder::*Method)(bool)>
class HudBuilderBoolBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const bool flag = *reinterpret_cast<bool*>(m_args[0]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(flag);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static bool converted;
        std::istringstream input(value);
        input >> converted;
        m_args[0] = reinterpret_cast<std::string*>(&converted);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE76983D's executor; table92B820. A string then an integer.
template<bool (Bf2HudBuilder::*Method)(std::string, int)>
class HudBuilderStringIntBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const std::string text(*m_args[0]);
        const int number = *reinterpret_cast<int*>(m_args[1]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(text, number);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static std::string text;
            std::istringstream input(value);
            input >> text;
            m_args[0] = &text;
        } else if (index == 2) {
            static int number;
            std::istringstream input(value);
            input >> number;
            m_args[1] = reinterpret_cast<std::string*>(&number);
        }
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE769B2A's executor; table92B950. Two floats, loaded in argument order.
template<bool (Bf2HudBuilder::*Method)(float, float)>
class HudBuilderFloatPairBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const float first = *reinterpret_cast<float*>(m_args[0]);
        const float second = *reinterpret_cast<float*>(m_args[1]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1 && index != 2) return;
        static float converted[2];
        std::istringstream input(value);
        input >> converted[index - 1];
        m_args[index - 1] = reinterpret_cast<std::string*>(&converted[index - 1]);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE766177 executor; table92A5B0. One integer, no conversion beyond the token.
template<bool (Bf2HudBuilder::*Method)(int)>
class HudBuilderIntBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const int argument = *reinterpret_cast<int*>(m_args[0]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(argument);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static int converted;
        std::istringstream input(value);
        input >> converted;
        m_args[0] = reinterpret_cast<std::string*>(&converted);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE7662C3 executor; table92A650. Two integers, read in argument order.
template<bool (Bf2HudBuilder::*Method)(int, int)>
class HudBuilderIntPairBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const int first = *reinterpret_cast<int*>(m_args[0]);
        const int second = *reinterpret_cast<int*>(m_args[1]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1 && index != 2) return;
        static int converted[2];
        std::istringstream input(value);
        input >> converted[index - 1];
        m_args[index - 1] = reinterpret_cast<std::string*>(&converted[index - 1]);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// Reads two floats separated by one character, accepting forms such as -122/-273.
inline void readConsoleVec2(std::istringstream& input, Math::Vec2& value)
{
    char separator = 0;
    input >> value.x >> separator >> value.y;
}

// PE75F555/75F423/75F687/75F7B9 executors and 75F4EA-shaped converters; one Vec2
// argument, a bool result. Each specialization keeps its own lazily zeroed static.
template<bool (meme::Bf2MenuMapNode::*Method)(float, float)>
class HudMapVec2Binding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const Math::Vec2 value = *reinterpret_cast<Math::Vec2*>(m_args[0]);
        m_hasReturnValue = true;
        m_result = (builderMap()->*Method)(value.x, value.y);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static Math::Vec2 converted;      // lazily zeroed, as at 75F50C
        std::istringstream input(value);
        readConsoleVec2(input, converted);
        m_args[0] = reinterpret_cast<std::string*>(&converted);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE75F8CB executor; one bool, a bool result.
template<bool (meme::Bf2MenuMapNode::*Method)(bool)>
class HudMapBoolBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const bool value = *reinterpret_cast<bool*>(m_args[0]);
        m_hasReturnValue = true;
        m_result = (builderMap()->*Method)(value);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static bool converted;
        std::istringstream input(value);
        input >> converted;
        m_args[0] = reinterpret_cast<std::string*>(&converted);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE75F979 executor; one string and no return value - the console row's declared
// type is void, and the executor clears the has-value flag instead of setting it.
template<void (meme::Bf2MenuMapNode::*Method)(std::string)>
class HudMapStringBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const std::string argument(*m_args[0]);
        m_hasReturnValue = false;
        (builderMap()->*Method)(argument);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static std::string converted;
        std::istringstream input(value);
        input >> converted;
        m_args[0] = &converted;
    }
    std::string getReturnValueAsString() const override { return std::string(); }
};

// PE75FC39 executor; four floats copied as one block, and no return value.
template<void (meme::Bf2MenuMapNode::*Method)(float, float, float, float)>
class HudMapColorBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 4) return;
        float channels[4];
        for (int i = 0; i != 4; ++i) channels[i] = *reinterpret_cast<float*>(m_args[i]);
        m_hasReturnValue = false;
        (builderMap()->*Method)(channels[0], channels[1],
                                                        channels[2], channels[3]);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index < 1 || index > 4) return;
        static float converted[4];
        std::istringstream input(value);
        input >> converted[index - 1];
        m_args[index - 1] = reinterpret_cast<std::string*>(&converted[index - 1]);
    }
    std::string getReturnValueAsString() const override { return std::string(); }
};

// PE75FD9C executor; table928510. Family N's create row: two strings, five
// integers and a Boolean. The seven-integer shape above is one argument short of
// it, which is why this is the only new template the family needs.
template<bool (Bf2HudBuilder::*Method)(std::string, std::string, int, int, int, int, int, bool)>
class HudBuilderCreateListBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 8) return;
        const std::string parent(*m_args[0]);
        const std::string name(*m_args[1]);
        int numbers[5];
        for (int i = 0; i != 5; ++i) numbers[i] = *reinterpret_cast<int*>(m_args[2 + i]);
        const bool flag = *reinterpret_cast<bool*>(m_args[7]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(parent, name, numbers[0], numbers[1],
                                                       numbers[2], numbers[3], numbers[4], flag);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index >= 3 && index <= 7) {
            static int numbers[5];
            std::istringstream input(value);
            input >> numbers[index - 3];
            m_args[index - 1] = reinterpret_cast<std::string*>(&numbers[index - 3]);
        } else if (index == 8) {
            static bool flag;
            std::istringstream input(value);
            input >> flag;
            m_args[7] = reinterpret_cast<std::string*>(&flag);
        }
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE7636B8 executor; table9296B8. A string then a Boolean - the mirror of the
// (bool, std::string) shape above, and family O's only new template.
template<bool (Bf2HudBuilder::*Method)(std::string, bool)>
class HudBuilderStringBoolBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const std::string text(*m_args[0]);
        const bool flag = *reinterpret_cast<bool*>(m_args[1]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(text, flag);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 2) return;
        static bool converted;
        std::istringstream input(value);
        input >> converted;
        m_args[1] = reinterpret_cast<std::string*>(&converted);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// Console adapters for radio menu operations.

// Forwards three string arguments without numeric conversion.
template<HudRadio* (BF2HudManager::*Getter)()>
class HudRadioStringMapBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 3) return;
        const std::string key(*m_args[0]);
        const std::string localizedId(*m_args[1]);
        const std::string command(*m_args[2]);
        m_hasReturnValue = false;
        (hudManager->*Getter)()->addEntry(key, localizedId, command);
    }
    std::string getReturnValueAsString() const override { return std::string(); }
};

// The commo rose (Q / T menus). The selectOrder rows below handle clicks; each click runs the entry's
// setStringMap command (family R above), dispatched by HudRadio::selectOption - see there.

// Selects an order from the radio menu.
template<HudRadio* (BF2HudManager::*Getter)()>
class HudRadioSelectBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const std::string option(*m_args[0]);
        m_hasReturnValue = true;
        m_result = (hudManager->*Getter)()->selectOption(option);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static std::string converted;       // one lazily constructed string per row
        std::istringstream input(value);
        input >> converted;
        m_args[0] = &converted;
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result; // PE +A4, no constructor store
};

// No-argument void rows on the HUD manager: 166 SquadLeader.sendRequestForOrder (executor 76CB5F,
// +2A8 = 7AFE90), 211 Radio.setSpottedMenuActive (76F390, +214 = 7AFDC0), 212 Radio.callForMedic
// (76F400, +158 = 7AFDD0), 213 Radio.callForAmmo (76F470, +160 = 7AFE30) and 214 Radio.callForRepair
// (76F4E0, +164 = 7AFE60); tables 92CFC0/92E920/92E9C0/92EA58/92EAE8, converter 57DD1F (a bare
// RET 8), void formatter 41BC76, availability 76CCAF (`hudManager != 0`, the only gate that tests
// the singleton). Shipped: exact arity 0, the has-value byte cleared, ds:A10890 reloaded, one
// virtual call. Wrong arity changes nothing.
template<void (BF2HudManager::*Method)()>
class HudNoArgVoidBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = false;
        (hudManager->*Method)();
    }
    void setArgFromString(int, const std::string&) override {} // PE 57DD1F, RET 8
    std::string getReturnValueAsString() const override { return std::string(); }
};

// Sends the selected order or request.
template<void (BF2HudManager::*Method)(int, bool), int Row>
class HudIntBoolVoidBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const bool flag = *reinterpret_cast<bool*>(m_args[1]);
        const int type = *reinterpret_cast<int*>(m_args[0]);
        m_hasReturnValue = false;
        (hudManager->*Method)(type, flag);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static int number;
            std::istringstream input(value);
            input >> number;
            m_args[0] = reinterpret_cast<std::string*>(&number);
        }
        if (index == 2) {
            static bool flag;
            std::istringstream input(value);
            input >> flag;
            m_args[1] = reinterpret_cast<std::string*>(&flag);
        }
    }
    std::string getReturnValueAsString() const override { return std::string(); }
};

// Sends a spotted-enemy message.
template<void (BF2HudManager::*Method)(bool, world::VehicleType)>
class HudBoolVehicleTypeVoidBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const bool announce = *reinterpret_cast<bool*>(m_args[0]);
        const world::VehicleType type = *reinterpret_cast<world::VehicleType*>(m_args[1]);
        m_hasReturnValue = false;
        (hudManager->*Method)(announce, type);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static bool announce;
            std::istringstream input(value);
            input >> announce;
            m_args[0] = reinterpret_cast<std::string*>(&announce);
        }
        if (index == 2) {
            static world::VehicleType type;
            std::istringstream input(value);
            input >> type;                      // world::operator>>, PE 5081A0
            m_args[1] = reinterpret_cast<std::string*>(&type);
        }
    }
    std::string getReturnValueAsString() const override { return std::string(); }
};

// CommanderMenu rows 183..186: PE76D496/76D50D/76D584/76D5FB. The
// generated invokers reject nonzero arity without changing prior state, clear
// the return flag before reacquiring the owner, and format void as empty.
template<void (HudCommander::*Method)()>
class HudCommanderNoArgBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = false;
        (hudManager->getCommander()->*Method)();
    }
    void setArgFromString(int, const std::string&) override {}
    std::string getReturnValueAsString() const override { return std::string(); }
};

// Commander radio-message binding using the shared TargetChannel parser.
template<void (HudCommander::*Method)(std::string, TargetChannel)>
class HudCommanderRadioBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const std::string message(*m_args[0]);
        const TargetChannel channel = *reinterpret_cast<TargetChannel*>(m_args[1]);
        m_hasReturnValue = false;
        (hudManager->getCommander()->*Method)(message, channel);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static std::string message;
            std::istringstream input(value);
            input >> message;
            m_args[0] = &message;
        }
        if (index == 2) {
            static TargetChannel channel;
            std::istringstream input(value);
            input >> channel;
            m_args[1] = reinterpret_cast<std::string*>(&channel);
        }
    }
    std::string getReturnValueAsString() const override { return std::string(); }
};

// CommanderMenu.vehicleDropReloadTime: PE76D876 executor,76D829 converter,
// 51343E formatter and table92DC68. This property changes only duration +68;
// it deliberately does not restart or otherwise alter an active cooldown.
class HudCommanderVehicleDropReloadTimeBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount == 0) {
            m_hasReturnValue = true;
            m_result = commanderVehicleDropReloadTime();
        } else if (m_argCount == 1) {
            const float value = *reinterpret_cast<float*>(m_args[0]);
            m_hasReturnValue = false;
            setCommanderVehicleDropReloadTime(value);
        }
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static float converted;
        std::istringstream input(value);
        input >> converted;
        m_args[0] = reinterpret_cast<std::string*>(&converted);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    float m_result;
};

// PE76451E executor and76445F converter; table929A60. Three integers, each with
// its own lazily zeroed static (A11350/A1134C/A11348), read in argument order.
template<bool (Bf2HudBuilder::*Method)(int, int, int)>
class HudBuilderIntTripleBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 3) return;
        const int first = *reinterpret_cast<int*>(m_args[0]);
        const int second = *reinterpret_cast<int*>(m_args[1]);
        const int third = *reinterpret_cast<int*>(m_args[2]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second, third);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index < 1 || index > 3) return;
        static int converted[3];
        std::istringstream input(value);
        input >> converted[index - 1];
        m_args[index - 1] = reinterpret_cast<std::string*>(&converted[index - 1]);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE7664BD executor; table92A6F0. Four integers, read in argument order.
template<bool (Bf2HudBuilder::*Method)(int, int, int, int)>
class HudBuilderIntQuadBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 4) return;
        const int first = *reinterpret_cast<int*>(m_args[0]);
        const int second = *reinterpret_cast<int*>(m_args[1]);
        const int third = *reinterpret_cast<int*>(m_args[2]);
        const int fourth = *reinterpret_cast<int*>(m_args[3]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second, third, fourth);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index < 1 || index > 4) return;
        static int converted[4];
        std::istringstream input(value);
        input >> converted[index - 1];
        m_args[index - 1] = reinterpret_cast<std::string*>(&converted[index - 1]);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE7666C7 executor; table92A790. Four floats: the PE loads all four onto the
// x87 stack before raising the flag, then spills them into the call frame.
template<bool (Bf2HudBuilder::*Method)(float, float, float, float)>
class HudBuilderFloatQuadBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 4) return;
        const float first = *reinterpret_cast<float*>(m_args[0]);
        const float second = *reinterpret_cast<float*>(m_args[1]);
        const float third = *reinterpret_cast<float*>(m_args[2]);
        const float fourth = *reinterpret_cast<float*>(m_args[3]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second, third, fourth);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index < 1 || index > 4) return;
        static float converted[4];
        std::istringstream input(value);
        input >> converted[index - 1];
        m_args[index - 1] = reinterpret_cast<std::string*>(&converted[index - 1]);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// [CHOSEN, not recovered] Builder binding for two strings and one integer.
// Incorrect arity leaves the previous result and return flag unchanged.
template<bool (Bf2HudBuilder::*Method)(std::string, std::string, int)>
class HudBuilderStringPairIntBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }

    void executeObjectMethod() override
    {
        if (m_argCount != 3) return;
        const std::string first(*m_args[0]);
        const std::string second(*m_args[1]);
        const int third = *reinterpret_cast<int*>(m_args[2]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second, third);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static std::string first; // PE A11998, initialized lazily
            std::istringstream input(value);
            input >> first;
            m_args[0] = &first;
        }
        if (index == 2) {
            static std::string second; // PE A1197C, independent lifetime
            std::istringstream input(value);
            input >> second;
            m_args[1] = &second;
        }
        if (index == 3) {
            static int third; // PE A11978, loader-zeroed, no guard
            std::istringstream input(value);
            input >> third;
            m_args[2] = reinterpret_cast<std::string*>(&third);
        }
    }

    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }

private:
    bool m_result; // PE +A4, no constructor store
};

// PE76A0B1 executor and76A02A converter; table92BBC8. A float (static A11930,
// loaded with `fld`) then an int (static A1192C). [CHOSEN, not recovered] spelling.
template<bool (Bf2HudBuilder::*Method)(float, int)>
class HudBuilderFloatIntBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const float first = *reinterpret_cast<float*>(m_args[0]);
        const int second = *reinterpret_cast<int*>(m_args[1]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static float number;
            std::istringstream input(value);
            input >> number;
            m_args[0] = reinterpret_cast<std::string*>(&number);
        } else if (index == 2) {
            static int count;
            std::istringstream input(value);
            input >> count;
            m_args[1] = reinterpret_cast<std::string*>(&count);
        }
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE76A862 executor and76A90E converter; table92BEE8. Three strings, each with
// its own lazily constructed static (A11A10/A119F4/A119D8, guarded by bits1,2
// and4 of A11A2C). The executor copies all three out of m_args before the call
// and copies each again for the by-value parameter, exactly as written here.
template<bool (Bf2HudBuilder::*Method)(std::string, std::string, std::string)>
class HudBuilderStringTripleBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 3) return;
        const std::string first(*m_args[0]);
        const std::string second(*m_args[1]);
        const std::string third(*m_args[2]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(first, second, third);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index < 1 || index > 3) return;
        static std::string converted[3];
        std::istringstream input(value);
        input >> converted[index - 1];
        m_args[index - 1] = &converted[index - 1];
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE769E3E/769E9E, table92BA90. [CHOSEN, not recovered] Native template
// spelling; string copies, lazy storage and result lifetime match the invoker.
template<bool (Bf2HudBuilder::*Method)(std::string)>
class HudBuilderStringBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const std::string argument(*m_args[0]);
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)(argument);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static std::string converted;
        std::istringstream input(value);
        input >> converted;
        m_args[0] = &converted;
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result; // Static object zero-initialization, no constructor store.
};

// PE76B162/76B25D executors and76B115/76B210 converters. Each specialization
// retains a separate lazy float. [CHOSEN, not recovered] Template name only.
template<bool (Bf2HudBuilder::*Method)(float)>
class HudBuilderFloatBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        float* input = reinterpret_cast<float*>(m_args[0]);
        bool* returnFlag = &m_hasReturnValue;
        float argument;
        // Preserve FLD before the flag and fstp before getBuilder, including
        // the x87 behavior for a signaling NaN or nondefault exception mask.
#if defined(_M_IX86)
        __asm {
            mov eax, input
            fld dword ptr [eax]
            mov eax, returnFlag
            mov byte ptr [eax], 1
            fstp argument
        }
#else
        // [PLATFORM] x64: SSE2 conversions. The double round trip quiets a signalling NaN as
        // fld/fstp does; its invalid-operation flag is raised in MXCSR instead of the x87 word.
        const double loaded = *input;
        *returnFlag = true;
        argument = static_cast<float>(loaded);
#endif
        m_result = (hudManager->getBuilder()->*Method)(argument);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static float converted;
        std::istringstream input(value);
        input >> converted;
        m_args[0] = reinterpret_cast<std::string*>(&converted);
    }
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// PE769F66, table92BB28. [CHOSEN, not recovered] Native template name.
template<bool (Bf2HudBuilder::*Method)()>
class HudBuilderNoArgBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = true;
        m_result = (hudManager->getBuilder()->*Method)();
    }
    void setArgFromString(int, const std::string&) override {} // PE57DD1F RET8
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result;
};

// [CHOSEN, not recovered] Template names replace generated class identities.
// Each member-pointer specialization retains its own static conversion storage.

template<bool (BF2HudManager::*Method)(int)>
class HudIntBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const int argument = *reinterpret_cast<int*>(m_args[0]);
        m_hasReturnValue = true;
        m_result = (hudManager->*Method)(argument);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static int converted;
            std::istringstream input(value);
            input >> converted;
            m_args[0] = reinterpret_cast<std::string*>(&converted);
        }
    }

    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }

private:
    bool m_result; // PE +A4; no constructor store, static object zero-initialization
};

template<bool (BF2HudManager::*Method)(int, int)>
class HudIntPairBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        int* const firstStorage = reinterpret_cast<int*>(m_args[0]);
        int* const secondStorage = reinterpret_cast<int*>(m_args[1]);
        const int first = *firstStorage;
        const int second = *secondStorage;
        m_hasReturnValue = true;
        m_result = (hudManager->*Method)(first, second);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static int first;
            std::istringstream input(value);
            input >> first;
            m_args[0] = reinterpret_cast<std::string*>(&first);
        }
        if (index == 2) {
            static int second;
            std::istringstream input(value);
            input >> second;
            m_args[1] = reinterpret_cast<std::string*>(&second);
        }
    }

    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }

private:
    bool m_result; // PE +A4; no constructor store, static object zero-initialization
};

// Converts the console argument to a Boolean before calling the HUD method.
template<void (BF2HudManager::*Method)(bool)>
class HudBoolVoidBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const bool argument = *reinterpret_cast<bool*>(m_args[0]);
        m_hasReturnValue = false;
        (hudManager->*Method)(argument);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static bool converted;
            std::istringstream input(value);
            input >> converted;
            m_args[0] = reinterpret_cast<std::string*>(&converted);
        }
    }

    std::string getReturnValueAsString() const override
    {
        return std::string();
    }
};

template<void (BF2HudManager::*Method)(int)>
class HudIntVoidBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const int argument = *reinterpret_cast<int*>(m_args[0]);
        m_hasReturnValue = false;
        (hudManager->*Method)(argument);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static int converted;
            std::istringstream input(value);
            input >> converted;
            m_args[0] = reinterpret_cast<std::string*>(&converted);
        }
    }

    std::string getReturnValueAsString() const override
    {
        return std::string();
    }
};

// [CHOSEN, not recovered] Scoreboard command binding with one integer argument and no return value.
// Requires the scoreboard established by its availability check.
template<void (HudScoreboard::*Method)(int)>
class HudScoreboardIntBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const int argument = *reinterpret_cast<int*>(m_args[0]);
        m_hasReturnValue = false;
        (hudManager->getScoreboard()->*Method)(argument);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static int converted;                   // A11EAC / A11EB0, one per row
            std::istringstream input(value);
            input >> converted;
            m_args[0] = reinterpret_cast<std::string*>(&converted);
        }
    }

    std::string getReturnValueAsString() const override
    {
        return std::string();
    }
};

// Console adapters for spawn-screen controls.

// Executors 76F6C4 / 76F7AD / 76FA15, converters 76F677 / 76F760 / 76F9C8 (int statics
// A11E5C / A11E60 / A11E68, one per row); tables 92EC10 / 92ECA8 / 92EE70.
template<void (HudKitSelection::*Method)(int)>
class HudKitSelectionIntBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const int argument = *reinterpret_cast<int*>(m_args[0]);
        m_hasReturnValue = false;
        (hudManager->getKitSelection()->*Method)(argument);    // +240, then the direct call
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static int converted;                   // A11E5C / A11E60 / A11E68, one per row
            std::istringstream input(value);
            input >> converted;
            m_args[0] = reinterpret_cast<std::string*>(&converted);
        }
    }

    std::string getReturnValueAsString() const override
    {
        return std::string();
    }
};

// Executor 76F90D, converter 76F8C0 (bool static A11E64, `operator>>(bool&)`); table 92EDD8.
// The byte is read and pushed before the has-value clear, as in HudBoolVoidBinding.
template<void (HudKitSelection::*Method)(bool)>
class HudKitSelectionBoolBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const bool argument = *reinterpret_cast<bool*>(m_args[0]);
        m_hasReturnValue = false;
        (hudManager->getKitSelection()->*Method)(argument);    // +240, then 7A1F90
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static bool converted;
            std::istringstream input(value);
            input >> converted;
            m_args[0] = reinterpret_cast<std::string*>(&converted);
        }
    }

    std::string getReturnValueAsString() const override
    {
        return std::string();
    }
};

// Executor 76F82A, converter 57DD1F (the shared `ret 8`); table 92ED40. Arity must be zero.
template<void (HudKitSelection::*Method)()>
class HudKitSelectionNoArgBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = false;
        (hudManager->getKitSelection()->*Method)();            // +240, then 7A1E30
    }

    void setArgFromString(int, const std::string&) override {}

    std::string getReturnValueAsString() const override
    {
        return std::string();
    }
};

// Sets a Boolean HUD item value.
template<bool (HudInformationAdapter::*Method)(std::string, bool)>
class HudInformationStringBoolBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        const std::string name(*m_args[0]);
        const bool value = *reinterpret_cast<bool*>(m_args[1]);
        m_hasReturnValue = false;
        HudInformationAdapter* const adapter = hudManager->getInformationAdapter(); // +244
        (adapter->*Method)(name, value);                        // +0C, result discarded
    }

    void setArgFromString(int index, const std::string& value) override
    {
        // Two independent tests, not else-if: 76C4E8 re-tests the index after arm 1.
        if (index == 1) {
            static std::string name;               // A11B34, lazy (A11B50 bit 0), atexit 87BA36
            std::istringstream input(value);
            input >> name;
            m_args[0] = &name;
        }
        if (index == 2) {
            static bool flag;
            std::istringstream input(value);
            input >> flag;
            m_args[1] = reinterpret_cast<std::string*>(&flag);
        }
    }

    std::string getReturnValueAsString() const override
    {
        return std::string();
    }
};

// [CHOSEN, not recovered] Copies one string argument, clears return-value state and calls the target by const reference.
template<void (BF2HudManager::*Method)(const std::string&)>
class HudStringRefVoidBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const std::string argument(*m_args[0]);
        m_hasReturnValue = false;
        (hudManager->*Method)(argument);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index != 1) return;
        static std::string converted;              // A10D88, lazy (A10DA4 bit 0), atexit 87B0CD
        std::istringstream input(value);
        input >> converted;
        m_args[0] = &converted;
    }

    std::string getReturnValueAsString() const override
    {
        return std::string();
    }
};


// [CHOSEN] Native console adapters for squad menu operations.
template<class Result, Result (HudSquad::*Method)()>
class HudSquadNoArgBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = false;
        (hudManager->getSquad()->*Method)();
    }
    void setArgFromString(int, const std::string&) override {}
    std::string getReturnValueAsString() const override { return std::string(); }
};

// Rows 192 / 195. Converters 76DAAB / 76DC8C parse one bool with istream::operator>>(bool&) (MSVCP71
// import at 87F35C) into the lazily used statics A11BF8 / A11BF9 - "1"/"0", no boolalpha - the
// HudBoolVoidBinding shape. Executors 76DAF8 / 76DCD9 copy the byte before clearing +A0.
template<class Result, Result (HudSquad::*Method)(bool)>
class HudSquadBoolBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const bool argument = *reinterpret_cast<bool*>(m_args[0]);
        m_hasReturnValue = false;
        (hudManager->getSquad()->*Method)(argument);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static bool converted;                  // A11BF8 / A11BF9, one per row
            std::istringstream input(value);
            input >> converted;
            m_args[0] = reinterpret_cast<std::string*>(&converted);
        }
    }
    std::string getReturnValueAsString() const override { return std::string(); }
};

// Vote-map integer command binding. [CHOSEN, not recovered] Native template name.
template<void (HudVoteMap::*Method)(int)>
class HudVoteMapIntBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;

    std::string execute(std::string* arguments, int count) override
    {
        return ConsoleObjectBaseImpl::execute(arguments, count);
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const int argument = *reinterpret_cast<int*>(m_args[0]);
        m_hasReturnValue = false;
        (hudManager->getVoteMap()->*Method)(argument);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static int converted;                   // A11EB4 / A11EB8, one per row
            std::istringstream input(value);
            input >> converted;
            m_args[0] = reinterpret_cast<std::string*>(&converted);
        }
    }

    std::string getReturnValueAsString() const override
    {
        return std::string();
    }
};

// [CHOSEN, not recovered] No-argument commander bindings call their targets with silent=false and publish the Boolean result.

template<bool (BF2HudManager::*Method)(bool)>
class HudNoArgSilentFalseBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = true;
        m_result = (hudManager->*Method)(false);              // push 0
    }
    void setArgFromString(int, const std::string&) override {} // PE57DD1F RET 8
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result; // PE +A4, no constructor store
};

// Commander row 181 zoomCommander: executor 76D394 (table 92D870) - the shape above with no
// owner argument; slot +2F0 (7B23B0).
template<bool (BF2HudManager::*Method)()>
class HudNoArgBoolBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = true;
        m_result = (hudManager->*Method)();
    }
    void setArgFromString(int, const std::string&) override {} // PE57DD1F RET 8
    std::string getReturnValueAsString() const override
    {
        if (!m_hasReturnValue) return std::string();
        std::ostringstream output;
        output << m_result;
        return output.str();
    }
private:
    bool m_result; // PE +A4, no constructor store
};

// Minimap rows 149 setPaintKit and 151 setPaintVehicle: executors 76B97E/76BADE, converters
// 76B931/76BA91 (int statics A11AFC/A11B00), void formatter 41BC76; tables 92C5E0/92C708. The
// argument is read and the has-value byte cleared before hudManager->getMap() (+250) is asked -
// the getter the Map gate 75E56E already tested, so the answer is not re-tested - and the map
// member (7764F0 / 776530) is called.
template<void (meme::Bf2MenuMapNode::*Method)(int)>
class HudMapIntVoidBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        const int argument = *reinterpret_cast<int*>(m_args[0]);
        m_hasReturnValue = false;
        (hudManager->getMap()->*Method)(argument);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1) {
            static int converted;                                 // A11AFC / A11B00, one per row
            std::istringstream input(value);
            input >> converted;
            m_args[0] = reinterpret_cast<std::string*>(&converted);
        }
    }
    std::string getReturnValueAsString() const override { return std::string(); }
};

// Minimap rows 150 setPaintAllKits, 152 setPaintAllVehicles, 154 setZoom and 156
// toggleShowKits: executors 76B9FB/76BB5B/76BCBB/76BE1D (tables 92C670/92C7A0/92C8D0/92C9F8),
// empty converter 57DD1F, void formatter 41BC76.
// Row 154 `setZoom` is not the map's +D8 setZoom (777980): its executor calls 7779A0,
//   cycleZoom, and drops the int it answers - hence the Result parameter.
// Row 156's body was inlined into 76BE1D (`cmp byte [map+38Dh],0 / sete / store`); it is
//   Bf2MenuMapNode::toggleShowKitsForHud here.
template<class Result, Result (meme::Bf2MenuMapNode::*Method)()>
class HudMapNoArgBinding final : public HudConsoleBinding {
public:
    using HudConsoleBinding::HudConsoleBinding;
    std::string execute(std::string* arguments, int count) override
    { return ConsoleObjectBaseImpl::execute(arguments, count); }
    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = false;
        (hudManager->getMap()->*Method)();
    }
    void setArgFromString(int, const std::string&) override {} // PE57DD1F RET 8
    std::string getReturnValueAsString() const override { return std::string(); }
};

// Every row retains original case, spelling, access/category defaults, arity,
// property direction, advertised return/argument types, and dynamic flag.
// Untouched base argument storage remains statically zero-initialized.
static SetConsoleClassDefaults defaults0("HudManager", 2, 0);
static HudConsoleBinding binding0("refresh", 0, 0, 0, "bool", true, Availability::Manager, {}); // 75C964 table926BB0
static HudIntPairBinding<&BF2HudManager::setPointerSize> binding1("setPointerSize", 0, 2, 2, "bool", true, Availability::Manager, {"int", "int"}); // 75C9DF table926C40
static HudStringBinding<&BF2HudManager::setMouseTextureEmpty> binding2("setMouseTextureEmpty", 0, 1, 1, "bool", true, Availability::Manager, {"std::string"}); // 75CB2C table926CD8
static HudStringBinding<&BF2HudManager::setMouseTextureFull> binding3("setMouseTextureFull", 0, 1, 1, "bool", true, Availability::Manager, {"std::string"}); // 75CC74 table926D78
static HudStringBinding<&BF2HudManager::setMouseTextureArtillery> binding4("setMouseTextureArtillery", 0, 1, 1, "bool", true, Availability::Manager, {"std::string"}); // 75CDBC table926E10
static HudStringBinding<&BF2HudManager::setMouseTextureUAV> binding5("setMouseTextureUAV", 0, 1, 1, "bool", true, Availability::Manager, {"std::string"}); // 75CF04 table926EB0
static HudStringBinding<&BF2HudManager::setMouseTextureSupply> binding6("setMouseTextureSupply", 0, 1, 1, "bool", true, Availability::Manager, {"std::string"}); // 75D04C table926F48
static HudStringBinding<&BF2HudManager::setMouseTextureCancel> binding7("setMouseTextureCancel", 0, 1, 1, "bool", true, Availability::Manager, {"std::string"}); // 75D194 table926FE8
static HudIntBinding<&BF2HudManager::setCommMouseSensitivity> binding8("setCommMouseSensitivity", 0, 1, 1, "bool", true, Availability::Manager, {"int"}); // 75D2DC table927088
static HudIntBinding<&BF2HudManager::setPointerMouseSensitivity> binding9("setPointerMouseSensitivity", 0, 1, 1, "bool", true, Availability::Manager, {"int"}); // 75D3CD table927128
static HudIntPairBinding<&BF2HudManager::setCommSize> binding10("setCommSize", 0, 2, 2, "bool", true, Availability::Manager, {"int", "int"}); // 75D4BE table9271C8
static HudIntPairBinding<&BF2HudManager::setCommPos> binding11("setCommPos", 0, 2, 2, "bool", true, Availability::Manager, {"int", "int"}); // 75D60B table927258
static HudIntPairBinding<&BF2HudManager::setCommMousePos> binding12("setCommMousePos", 0, 2, 2, "bool", true, Availability::Manager, {"int", "int"}); // 75D758 table9272E8
static HudIntPairBinding<&BF2HudManager::setSpottedMenuSize> binding13("setSpottedMenuSize", 0, 2, 2, "bool", true, Availability::Manager, {"int", "int"}); // 75D8A5 table927380
static HudIntPairBinding<&BF2HudManager::setSpottedMenuPos> binding14("setSpottedMenuPos", 0, 2, 2, "bool", true, Availability::Manager, {"int", "int"}); // 75D9F2 table927418
static HudIntPairBinding<&BF2HudManager::setSpottedMousePos> binding15("setSpottedMousePos", 0, 2, 2, "bool", true, Availability::Manager, {"int", "int"}); // 75DB3F table9274B0
static HudConsoleBinding binding16("setSpottedAngle", 0, 1, 1, "void", false, Availability::Manager, {"float"}); // 75DC8C table927548
static HudBoolVoidBinding<&BF2HudManager::toggleSayAll> binding17("enableSayAllChatBox", 0, 1, 1, "void", false, Availability::Manager, {"bool"}); // 75DD71 table9275E0; executor 75DE2F -> +B0 7B3360, converter 75DDE2 (bool A10D7C)
static HudBoolVoidBinding<&BF2HudManager::toggleSayTeam> binding18("enableSayTeamChatBox", 0, 1, 1, "void", false, Availability::Manager, {"bool"}); // 75DE5D table927678; executor 75DF1B -> +B4 7B33F0, converter 75DECE (bool A10D7D)
static HudBoolVoidBinding<&BF2HudManager::toggleSaySquad> binding19("enableSaySquadChatBox", 0, 1, 1, "void", false, Availability::Manager, {"bool"}); // 75DF49 table927718; executor 75E007 -> +B8 7B3480, converter 75DFBA (bool A10D7E)
static HudIntVoidBinding<&BF2HudManager::setMaximumNrOfCPs> binding20("setMaximumNrOfCPs", 0, 1, 1, "void", false, Availability::Manager, {"int"}); // 75E035 table9277B8
static HudConsoleBinding binding21("setPaint", 0, 1, 1, "void", false, Availability::Manager, {"bool"}); // 75E117 table927850
static HudConsoleBinding binding22("setUpdate", 0, 1, 1, "void", false, Availability::Manager, {"bool"}); // 75E200 table9278E0
static HudBoolVoidBinding<&BF2HudManager::setDone> binding23("setDone", 0, 1, 1, "void", false, Availability::Manager, {"bool"}); // 75E2E9 table927970; thunks 75E35A/75E3A7 -> +1BC
static HudStringRefVoidBinding<&BF2HudManager::addTextureAtlas> binding24("addTextureAtlas", 0, 1, 1, "void", false, Availability::Manager, {"std::string"}); // 75E3D5 table927A00; executor 75E446 -> +36C 755F00, converter 75E488 (string A10D88)
static HudConsoleBinding binding25("setDisplayControlpoints", 0, 1, 1, "void", false, Availability::Map, {"bool"}); // 75E4FD table927A98
static HudConsoleBinding binding26("setDisplayTickets", 0, 1, 1, "void", false, Availability::Information, {"bool"}); // 75E5FB table927B38
static HudConsoleBinding binding27("setMapStatic", 0, 1, 1, "void", false, Availability::Information, {"bool"}); // 75E744 table927BD0
static HudConsoleBinding binding28("setTargetTop", 0, 1, 1, "void", false, Availability::Map, {"float"}); // 75E837 table927C68
static HudBoolVoidBinding<&BF2HudManager::addFavouriteServer> binding29("addFavouriteServer", 0, 1, 1, "void", false, Availability::Manager, {"bool"}); // 75E955 table927D00; executor 75EA13 -> +370 751B50, converter 75E9C6 (bool A10DB0)
static SetConsoleClassDefaults defaults1("ItemSelection", 2, 0);
static HudConsoleBinding binding30("setReverseItems", 0, 1, 1, "void", false, Availability::Messages, {"bool"}); // 75EA41 table927D88
static SetConsoleClassDefaults defaults2("HudBuilder", 2, 0);
static HudBuilderStringPairBinding<&Bf2HudBuilder::createMapNode> binding31("createMapNode", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "std::string"}); // 75EB3C table927E20; invoker75EBC9 ->796530
static HudBuilderCreateNodeBinding<&Bf2HudBuilder::createMiniMapNode> binding32("createMiniMapNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 75ED22 table927EB8
static HudMapVec2Binding<&meme::Bf2MenuMapNode::setMiniSizeForHud> binding33("setMiniSize", 0, 1, 1, "bool", true, Availability::BuilderMap, {"Vec2"}); // 75F07D table927F50
static HudMapVec2Binding<&meme::Bf2MenuMapNode::setMiniPosForHud> binding34("setMiniPos", 0, 1, 1, "bool", true, Availability::BuilderMap, {"Vec2"}); // 75F1F0 table927FE0
static HudMapVec2Binding<&meme::Bf2MenuMapNode::setMaxiSizeForHud> binding35("setMaxiSize", 0, 1, 1, "bool", true, Availability::BuilderMap, {"Vec2"}); // 75F345 table928070
static HudMapVec2Binding<&meme::Bf2MenuMapNode::setMaxiPosForHud> binding36("setMaxiPos", 0, 1, 1, "bool", true, Availability::BuilderMap, {"Vec2"}); // 75F477 table928100
static HudMapVec2Binding<&meme::Bf2MenuMapNode::setCommanderSizeForHud> binding37("setCommanderSize", 0, 1, 1, "bool", true, Availability::BuilderMap, {"Vec2"}); // 75F5A9 table928190
static HudMapVec2Binding<&meme::Bf2MenuMapNode::setCommanderPosForHud> binding38("setCommanderPos", 0, 1, 1, "bool", true, Availability::BuilderMap, {"Vec2"}); // 75F6DB table928228
static HudMapBoolBinding<&meme::Bf2MenuMapNode::setZoomIconsForHud> binding39("setZoomIcons", 0, 1, 1, "bool", true, Availability::BuilderMap, {"bool"}); // 75F80D table9282C0
static HudMapStringBinding<&meme::Bf2MenuMapNode::setControlPointFontForHud> binding40("setCPFont", 0, 1, 1, "void", false, Availability::BuilderMap, {"std::string"}); // 75F908 table928358
static HudMapColorBinding<&meme::Bf2MenuMapNode::setControlPointFontColorForHud> binding41("setCPFontColor", 0, 4, 4, "void", false, Availability::BuilderMap, {"float", "float", "float", "float"}); // 75FA7E table9283E8
static HudConsoleBinding binding42("setModifyer", 0, 1, 1, "void", false, Availability::BuilderMap, {"float"}); // 75FCAA table928480
static HudBuilderCreateListBinding<&Bf2HudBuilder::createListNode> binding43("createListNode", 0, 8, 8, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int", "int", "bool"}); // 75FD9C table928510
static HudBuilderIntBinding<&Bf2HudBuilder::setListNodeData> binding44("setListNodeData", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 7601BF table9285A8
static HudBuilderIntStringBinding<&Bf2HudBuilder::setListNodeConCmd> binding45("setListNodeConCmd", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 7602B7 table928640
static HudBuilderFloatQuadBinding<&Bf2HudBuilder::setListNodeBackgroundColor> binding46("setListNodeBackgroundColor", 0, 4, 4, "bool", true, Availability::Builder, {"float", "float", "float", "float"}); // 76045E table9286D8
static HudBuilderFloatQuadBinding<&Bf2HudBuilder::setListNodeSelectColor> binding47("setListNodeSelectColor", 0, 4, 4, "bool", true, Availability::Builder, {"float", "float", "float", "float"}); // 760672 table928778
static HudBuilderFloatQuadBinding<&Bf2HudBuilder::setListNodeBorderColor> binding48("setListNodeBorderColor", 0, 4, 4, "bool", true, Availability::Builder, {"float", "float", "float", "float"}); // 760886 table928818
static HudBuilderIntQuadBinding<&Bf2HudBuilder::setListNodeBorder> binding49("setListNodeBorder", 0, 4, 4, "bool", true, Availability::Builder, {"int", "int", "int", "int"}); // 760A9A table9288B8
static HudBuilderIntPairBinding<&Bf2HudBuilder::setListNodeScrollbar> binding50("setListNodeScrollbar", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 760CA4 table928950
static HudBuilderIntBinding<&Bf2HudBuilder::setListNodeRowSpacing> binding51("setListNodeRowSpacing", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 760DF6 table9289F0
static HudBuilderFloatQuadBinding<&Bf2HudBuilder::setListNodeScrollbarColor> binding52("setListNodeScrollbarColor", 0, 4, 4, "bool", true, Availability::Builder, {"float", "float", "float", "float"}); // 760EEE table928A90
static HudBuilderFloatQuadBinding<&Bf2HudBuilder::setListNodeScrollbarBackgroundColor> binding53("setListNodeScrollbarBackgroundColor", 0, 4, 4, "bool", true, Availability::Builder, {"float", "float", "float", "float"}); // 761102 table928B30
static HudBuilderStringIntBinding<&Bf2HudBuilder::setListNodeFont> binding54("setListNodeFont", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "int"}); // 761370 table928BD8
static HudBuilderBoolBinding<&Bf2HudBuilder::setListNodeOutline> binding55("setListNodeOutline", 0, 1, 1, "bool", true, Availability::Builder, {"bool"}); // 761515 table928C70
static HudBuilderCreateSliderBinding<&Bf2HudBuilder::createSliderNode> binding56("createSliderNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "float", "float", "float", "float"}); // 761616 table928D08
static HudBuilderStringBinding<&Bf2HudBuilder::setSliderNodeData> binding57("setSliderNodeData", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 76197B table928DA0
static HudBuilderStringBinding<&Bf2HudBuilder::setSliderNodeChild> binding58("setSliderNodeChild", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 761AC3 table928E38
static HudBuilderCreateNodeBinding<&Bf2HudBuilder::createTransformNode, int> binding59("createTransformNode", 0, 6, 6, "int", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 761C0B table928ED0
static HudBuilderStringPairBinding<&Bf2HudBuilder::createSplitNode> binding60("createSplitNode", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "std::string"}); // 761F65 table928F68
static HudBuilderCreateBarBinding<&Bf2HudBuilder::createTransformListNode> binding61("createTransformListNode", 0, 7, 7, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int", "int"}); // 76214B table929000
static HudBuilderStringBinding<&Bf2HudBuilder::addTransformListNode> binding62("addTransformListNode", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 762517 table9290A0
static HudBuilderIntStringBinding<&Bf2HudBuilder::setTranformListNodePosVariable> binding63("setTranformListNodePosVariable", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 76265F table929140
static HudBuilderIntPairBinding<&Bf2HudBuilder::setTranformListNodeOffset> binding64("setTranformListNodeOffset", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 762806 table9291E8
static HudConsoleBinding binding65("createTileNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 762958 table929288
static HudConsoleBinding binding66("setTileNodeOptions", 0, 3, 3, "bool", true, Availability::Builder, {"int", "int", "int"}); // 762CB3 table929320
static HudBuilderCreateNodeBinding<&Bf2HudBuilder::createEditNode> binding67("createEditNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 762E61 table9293B8
static HudBuilderIntBinding<&Bf2HudBuilder::setEditNodeData> binding68("setEditNodeData", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 7631BC table929450
static HudBuilderIntBinding<&Bf2HudBuilder::setEditNodeString> binding69("setEditNodeString", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 7632B4 table9294E8
static HudBuilderIntBinding<&Bf2HudBuilder::setEditNodeMaxLength> binding70("setEditNodeMaxLength", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 7633AC table929580
static HudBuilderFloatQuadBinding<&Bf2HudBuilder::setEditNodeColor> binding71("setEditNodeColor", 0, 4, 4, "bool", true, Availability::Builder, {"float", "float", "float", "float"}); // 7634A4 table929620
static HudBuilderStringBoolBinding<&Bf2HudBuilder::setEditNodeFont> binding72("setEditNodeFont", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "bool"}); // 7636B8 table9296B8
static HudBuilderCreateBarBinding<&Bf2HudBuilder::createOccupiedNode> binding73("createOccupiedNode", 0, 7, 7, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int", "int"}); // 76385E table929750
static HudBuilderIntBinding<&Bf2HudBuilder::setOccupiedNodeData> binding74("setOccupiedNodeData", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 763C15 table9297E8
static HudBuilderIntStringBinding<&Bf2HudBuilder::setOccupiedNodePosVariable> binding75("setOccupiedNodePosVariable", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 763D0D table929880
static HudBuilderCreateNodeBinding<&Bf2HudBuilder::createObjectMarkerNode> binding76("createObjectMarkerNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 763EB4 table929920
static HudBuilderIntStringBinding<&Bf2HudBuilder::setObjectMarkerNodeTexture> binding77("setObjectMarkerNodeTexture", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 76420F table9299C0
static HudBuilderIntTripleBinding<&Bf2HudBuilder::setObjectMarkerNodeTextureSize> binding78("setObjectMarkerNodeTextureSize", 0, 3, 3, "bool", true, Availability::Builder, {"int", "int", "int"}); // 7643B6 table929A60
static HudBuilderIntBinding<&Bf2HudBuilder::setObjectMarkerNodeObjects> binding79("setObjectMarkerNodeObjects", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 764564 table929B08
static HudBuilderBoolStringBinding<&Bf2HudBuilder::setObjectMarkerNodeLockText> binding80("setObjectMarkerNodeLockText", 0, 2, 2, "bool", true, Availability::Builder, {"bool", "std::string"}); // 76465C table929BA8
static HudBuilderIntPairBinding<&Bf2HudBuilder::setObjectMarkerNodeLockTextOffset> binding81("setObjectMarkerNodeLockTextOffset", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 764804 table929C48
static HudBuilderStringBinding<&Bf2HudBuilder::addObjectMarkerNodeLockTextNode> binding82("addObjectMarkerNodeLockTextNode", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 764956 table929CF0
static HudBuilderIntBinding<&Bf2HudBuilder::setObjectMarkerNodeLockOnType> binding83("setObjectMarkerNodeLockOnType", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 764A9E table929D98
static HudBuilderIntBinding<&Bf2HudBuilder::setObjectMarkerNodeWeapon> binding84("setObjectMarkerNodeWeapon", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 764B96 table929E40
static HudBuilderCreateNodeBinding<&Bf2HudBuilder::createHoverNode> binding85("createHoverNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 764C8E table929EE0
static HudBuilderIntPairBinding<&Bf2HudBuilder::setHoverInMiddlePos> binding86("setHoverInMiddlePos", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 764FE9 table929F78
static HudBuilderFloatBinding<&Bf2HudBuilder::setHoverMaxValue> binding87("setHoverMaxValue", 0, 1, 1, "bool", true, Availability::Builder, {"float"}); // 76513B table92A010
static HudBuilderFloatPairBinding<&Bf2HudBuilder::setHoverWidthLength> binding88("setHoverWidthLength", 0, 2, 2, "bool", true, Availability::Builder, {"float", "float"}); // 765236 table92A0A8
static HudBuilderCreateNodeBinding<&Bf2HudBuilder::createObjectSelectionNode> binding89("createObjectSelectionNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 76538F table92A140
static HudBuilderIntPairBinding<&Bf2HudBuilder::setObjectSelectionNodePointerSize> binding90("setObjectSelectionNodePointerSize", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 7656EA table92A1E0
static HudBuilderCreateNodeBinding<&Bf2HudBuilder::createPictureNode> binding91("createPictureNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 76583C table92A288
static HudBuilderStringBinding<&Bf2HudBuilder::setPictureNodeTexture> binding92("setPictureNodeTexture", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 765B97 table92A320
static HudConsoleBinding binding93("setPictureNodeAlphaMask", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 765CDF table92A3C0
static HudBuilderStringBinding<&Bf2HudBuilder::setPictureNodeVariableTexture> binding94("setPictureNodeVariableTexture", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 765E27 table92A460
static HudBuilderStringBinding<&Bf2HudBuilder::setPictureNodeRotateVariable> binding95("setPictureNodeRotateVariable", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 765F6F table92A508
static HudBuilderIntBinding<&Bf2HudBuilder::setPictureNodeRotation> binding96("setPictureNodeRotation", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 7660B7 table92A5B0
static HudBuilderIntPairBinding<&Bf2HudBuilder::setPictureNodeCenterPoint> binding97("setPictureNodeCenterPoint", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 7661AF table92A650
static HudBuilderIntQuadBinding<&Bf2HudBuilder::setPictureNodeBorder> binding98("setPictureNodeBorder", 0, 4, 4, "bool", true, Availability::Builder, {"int", "int", "int", "int"}); // 766301 table92A6F0
static HudBuilderFloatQuadBinding<&Bf2HudBuilder::setPictureNodeBorderColor> binding99("setPictureNodeBorderColor", 0, 4, 4, "bool", true, Availability::Builder, {"float", "float", "float", "float"}); // 76650B table92A790
static HudBuilderCreateBarBinding<&Bf2HudBuilder::createBarNode> binding100("createBarNode", 0, 7, 7, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int", "int"}); // 76671F table92A830
static HudBuilderIntStringBinding<&Bf2HudBuilder::setBarNodeTexture> binding101("setBarNodeTexture", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 766AD6 table92A8C8
static HudBuilderIntStringBinding<&Bf2HudBuilder::setBarNodeVariableTexture> binding102("setBarNodeVariableTexture", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 766C7D table92A960
static HudBuilderIntQuadBinding<&Bf2HudBuilder::setBarNodeBorder> binding103("setBarNodeBorder", 0, 4, 4, "bool", true, Availability::Builder, {"int", "int", "int", "int"}); // 766E24 table92AA00
static HudBuilderIntBinding<&Bf2HudBuilder::setBarNodeSnap> binding104("setBarNodeSnap", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 76702E table92AA98
static HudBuilderBoolBinding<&Bf2HudBuilder::setBarNodeSnapDir> binding105("setBarNodeSnapDir", 0, 1, 1, "bool", true, Availability::Builder, {"bool"}); // 767126 table92AB30
static HudBuilderStringBinding<&Bf2HudBuilder::setBarNodeValueVariable> binding106("setBarNodeValueVariable", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 767227 table92ABC8
static HudBuilderCreateCompassBinding<&Bf2HudBuilder::createCompassNode> binding107("createCompassNode", 0, 9, 9, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int", "int", "bool", "bool"}); // 76736F table92AC68
static HudBuilderIntStringBinding<&Bf2HudBuilder::setCompassNodeTexture> binding108("setCompassNodeTexture", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 7677EE table92AD00
static HudConsoleBinding binding109("setCompassNodeVariableTexture", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 767995 table92ADA0
static HudBuilderIntQuadBinding<&Bf2HudBuilder::setCompassNodeBorder> binding110("setCompassNodeBorder", 0, 4, 4, "bool", true, Availability::Builder, {"int", "int", "int", "int"}); // 767B3C table92AE48
static HudBuilderIntQuadBinding<&Bf2HudBuilder::setCompassNodeSnapOffset> binding111("setCompassNodeSnapOffset", 0, 4, 4, "bool", true, Availability::Builder, {"int", "int", "int", "int"}); // 767D46 table92AEE8
static HudBuilderBoolStringBinding<&Bf2HudBuilder::setCompassNodeSnapTexture> binding112("setCompassNodeSnapTexture", 0, 2, 2, "bool", true, Availability::Builder, {"bool", "std::string"}); // 767F50 table92AF88
static HudBuilderStringBinding<&Bf2HudBuilder::setCompassNodeValueVariable> binding113("setCompassNodeValueVariable", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 7680F8 table92B028
static HudBuilderIntBinding<&Bf2HudBuilder::setCompassNodeOffset> binding114("setCompassNodeOffset", 0, 1, 1, "bool", true, Availability::Builder, {"int"}); // 768240 table92B0C8
static HudBuilderIntPairBinding<&Bf2HudBuilder::setCompassNodeTextureSize> binding115("setCompassNodeTextureSize", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 768338 table92B168
static HudBuilderCreateNodeBinding<&Bf2HudBuilder::createButtonNode> binding116("createButtonNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 76848A table92B208
static HudBuilderIntStringBinding<&Bf2HudBuilder::setButtonNodeTexture> binding117("setButtonNodeTexture", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 7687E5 table92B2A0
static HudBuilderIntQuadBinding<&Bf2HudBuilder::setButtonNodeMouseArea> binding118("setButtonNodeMouseArea", 0, 4, 4, "bool", true, Availability::Builder, {"int", "int", "int", "int"}); // 76898C table92B340
static HudConsoleBinding binding119("setButtonNodeMouseOverColor", 0, 4, 4, "bool", true, Availability::Builder, {"float", "float", "float", "float"}); // 768B96 table92B3E0
static HudBuilderStringIntBinding<&Bf2HudBuilder::setButtonNodeConCmd> binding120("setButtonNodeConCmd", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "int"}); // 768DAA table92B480
static HudBuilderStringIntBinding<&Bf2HudBuilder::setButtonNodeAltConCmd> binding121("setButtonNodeAltConCmd", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "int"}); // 768F4F table92B518
static HudConsoleBinding binding122("setButtonNodeFunction", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "int"}); // 7690F4 table92B5B8
static HudBuilderBoolBinding<&Bf2HudBuilder::setButtonNodeDebug> binding123("setButtonNodeDebug", 0, 1, 1, "bool", true, Availability::Builder, {"bool"}); // 769299 table92B658
static HudBuilderCreateNodeBinding<&Bf2HudBuilder::createTextNode> binding124("createTextNode", 0, 6, 6, "bool", true, Availability::Builder, {"std::string", "std::string", "int", "int", "int", "int"}); // 76939A table92B6F0
static HudBuilderStringBinding<&Bf2HudBuilder::setTextNodeString> binding125("setTextNodeString", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 7696F5 table92B788
static HudBuilderStringIntBinding<&Bf2HudBuilder::setTextNodeStyle> binding126("setTextNodeStyle", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "int"}); // 76983D table92B820
static HudConsoleBinding binding127("setTextNodeOutLine", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 7699E2 table92B8B8
static HudBuilderFloatPairBinding<&Bf2HudBuilder::setTextNodeOutLineOffset> binding128("setTextNodeOutLineOffset", 0, 2, 2, "bool", true, Availability::Builder, {"float", "float"}); // 769B2A table92B950
static HudBuilderStringBinding<&Bf2HudBuilder::setTextNodeStringVariable> binding129("setTextNodeStringVariable", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 769C83 table92B9F0
static HudBuilderStringBinding<&Bf2HudBuilder::setNodeShowVariable> binding130("setNodeShowVariable", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 769DCB table92BA90
static HudBuilderNoArgBinding<&Bf2HudBuilder::addNodeAlphaShowEffect> binding131("addNodeAlphaShowEffect", 0, 0, 0, "bool", true, Availability::Builder, {}); // 769F13 table92BB28
static HudBuilderFloatIntBinding<&Bf2HudBuilder::addNodeMoveShowEffect> binding132("addNodeMoveShowEffect", 0, 2, 2, "bool", true, Availability::Builder, {"float", "int"}); // 769F98 table92BBC8
static HudBuilderIntPairBinding<&Bf2HudBuilder::addNodeBlendEffect> binding133("addNodeBlendEffect", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 76A0F2 table92BC68
static HudConsoleBinding binding134("addNodeVariableMoveShowEffect", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "std::string"}); // 76A244 table92BD00
static HudBuilderStringPairIntBinding<&Bf2HudBuilder::setNodeLogicShowVariable> binding135("setNodeLogicShowVariable", 0, 3, 3, "bool", true, Availability::Builder, {"std::string", "std::string", "int"}); // 76A42A table92BDA8
static HudBuilderStringBinding<&Bf2HudBuilder::setNodeAlphaVariable> binding136("setNodeAlphaVariable", 0, 1, 1, "bool", true, Availability::Builder, {"std::string"}); // 76A671 table92BE48
static HudBuilderStringTripleBinding<&Bf2HudBuilder::setNodeRGBVariables> binding137("setNodeRGBVariables", 0, 3, 3, "bool", true, Availability::Builder, {"std::string", "std::string", "std::string"}); // 76A7B9 table92BEE8
static HudBuilderFloatQuadBinding<&Bf2HudBuilder::setNodeColor> binding138("setNodeColor", 0, 4, 4, "bool", true, Availability::Builder, {"float", "float", "float", "float"}); // 76AA43 table92BF80
static HudBuilderIntPairBinding<&Bf2HudBuilder::setNodePos> binding139("setNodePos", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 76AC57 table92C018
static HudBuilderIntPairBinding<&Bf2HudBuilder::setNodeOffset> binding140("setNodeOffset", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 76ADA9 table92C0A8
static HudBuilderIntStringBinding<&Bf2HudBuilder::setNodePosVariable> binding141("setNodePosVariable", 0, 2, 2, "bool", true, Availability::Builder, {"int", "std::string"}); // 76AEFB table92C140
static HudBuilderFloatBinding<&Bf2HudBuilder::setNodeInTime> binding142("setNodeInTime", 0, 1, 1, "bool", true, Availability::Builder, {"float"}); // 76B0A2 table92C1D8
static HudBuilderFloatBinding<&Bf2HudBuilder::setNodeOutTime> binding143("setNodeOutTime", 0, 1, 1, "bool", true, Availability::Builder, {"float"}); // 76B19D table92C270
static HudBuilderIntPairBinding<&Bf2HudBuilder::setNodeSize> binding144("setNodeSize", 0, 2, 2, "bool", true, Availability::Builder, {"int", "int"}); // 76B298 table92C308
static HudConsoleBinding binding145("deleteNode", 0, 0, 0, "bool", true, Availability::Builder, {}); // 76B3EA table92C398
static HudConsoleBinding binding146("searchNodes", 0, 2, 2, "meme::Node*", true, Availability::Builder, {"std::string", "std::string"}); // 76B46F table92C438
static HudBuilderStringPairBinding<&Bf2HudBuilder::setActiveObject> binding147("setActiveObject", 0, 2, 2, "bool", true, Availability::Builder, {"std::string", "std::string"}); // 76B655 table92C4C8
static HudBuilderNoArgBinding<&Bf2HudBuilder::newLayer> binding148("newLayer", 0, 0, 0, "bool", true, Availability::Builder, {}); // 76B83B table92C550
static SetConsoleClassDefaults defaults3("Minimap", 2, 0);
static HudMapIntVoidBinding<&meme::Bf2MenuMapNode::setPaintKitForHud> binding149("setPaintKit", 0, 1, 1, "void", false, Availability::Map, {"int"}); // 76B8C0 table92C5E0, executor 76B97E -> 7764F0
static HudMapNoArgBinding<void, &meme::Bf2MenuMapNode::setPaintAllKitsForHud> binding150("setPaintAllKits", 0, 0, 0, "void", false, Availability::Map, {}); // 76B9A9 table92C670, executor 76B9FB -> 7778E0
static HudMapIntVoidBinding<&meme::Bf2MenuMapNode::setPaintVehicleForHud> binding151("setPaintVehicle", 0, 1, 1, "void", false, Availability::Map, {"int"}); // 76BA20 table92C708, executor 76BADE -> 776530
static HudMapNoArgBinding<void, &meme::Bf2MenuMapNode::setPaintAllVehiclesForHud> binding152("setPaintAllVehicles", 0, 0, 0, "void", false, Availability::Map, {}); // 76BB09 table92C7A0, executor 76BB5B -> 777930
static HudConsoleBinding binding153("setCurrOrderList", 0, 1, 1, "void", false, Availability::Map, {"int"}); // 76BB80 table92C838
static HudMapNoArgBinding<int, &meme::Bf2MenuMapNode::cycleZoom> binding154("setZoom", 0, 0, 0, "void", false, Availability::Map, {}); // 76BC69 table92C8D0, executor 76BCBB -> 7779A0 cycleZoom, not +D8
static HudConsoleBinding binding155("setFullScreenZoom", 0, 1, 1, "void", false, Availability::Map, {"int"}); // 76BCE0 table92C960
static HudMapNoArgBinding<void, &meme::Bf2MenuMapNode::toggleShowKitsForHud> binding156("toggleShowKits", 0, 0, 0, "void", false, Availability::Map, {}); // 76BDCB table92C9F8, executor 76BE1D, body inlined (+38D)
static HudConsoleBinding binding157("setCommanderOffset", 0, 1, 1, "void", false, Availability::Map, {"Vec2"}); // 76BE48 table92CA90
static HudConsoleBinding binding158("setSourceBlend", 0, 1, 1, "void", false, Availability::Map, {"int"}); // 76BF7D table92CB28
static HudConsoleBinding binding159("setDestinationBlend", 0, 1, 1, "void", false, Availability::Map, {"int"}); // 76C066 table92CBC0
static HudConsoleBinding binding160("setFilterColor", 0, 4, 4, "void", false, Availability::Map, {"float", "float", "float", "float"}); // 76C14F table92CC58
static SetConsoleClassDefaults defaults4("HudItems", 2, 0);
static HudInformationStringBoolBinding<&HudInformationAdapter::setBool> binding161("setBool", 0, 2, 2, "void", false, Availability::Information, {"std::string", "bool"}); // 76C386 table92CCF0; executor 76C417 -> +244 then adapter +0C 785D00, converter 76C47A (string A11B34, bool A11B30)
static HudConsoleBinding binding162("setString", 0, 2, 2, "void", false, Availability::Information, {"std::string", "std::string"}); // 76C527 table92CD80
static HudConsoleBinding binding163("setFloat", 0, 2, 2, "void", false, Availability::Information, {"std::string", "float"}); // 76C704 table92CE10
static SetConsoleClassDefaults defaults5("SquadLeader", 2, 0);
static HudIntBoolVoidBinding<&BF2HudManager::sendOrder, 164> binding164("sendOrder", 0, 2, 2, "void", false, Availability::Manager, {"int", "bool"}); // 76C8A9 table92CEA0, executor 76CDF1 (folded with row 170), converter 76C93A -> +33C
static HudIntBoolVoidBinding<&BF2HudManager::sendRequest, 165> binding165("sendRequest", 0, 2, 2, "void", false, Availability::Manager, {"int", "bool"}); // 76C9C1 table92CF30, executor 76CAD9, converter 76CA52 -> +340
static HudNoArgVoidBinding<&BF2HudManager::sendRequestForOrder> binding166("sendRequestForOrder", 0, 0, 0, "void", false, Availability::Manager, {}); // 76CB0D table92CFC0, executor 76CB5F -> +2A8
static SetConsoleClassDefaults defaults6("Commander", 2, 0);
static HudNoArgVoidBinding<&BF2HudManager::commanderApply> binding167("commanderApply", 0, 0, 0, "void", false, Availability::Manager, {}); // 76CB7D table92D058, executor 76CBCF -> +2B0 7B1C50
static HudNoArgVoidBinding<&BF2HudManager::commanderResign> binding168("commanderResign", 0, 0, 0, "void", false, Availability::Manager, {}); // 76CBED table92D0F0, executor 76CC3F -> +2B4 7B1ED0
static HudNoArgVoidBinding<&BF2HudManager::commanderMutiny> binding169("commanderMutiny", 0, 0, 0, "void", false, Availability::Manager, {}); // 76CC5D table92D188, executor 76CCBB -> +2B8 7B1F50
static HudIntBoolVoidBinding<&BF2HudManager::sendOrder, 170> binding170("sendOrder", 0, 2, 2, "void", false, Availability::Manager, {"int", "bool"}); // 76CCD9 table92D220, executor 76CDF1 / converter 76CD6A -> +33C 7B1790
static HudNoArgVoidBinding<&BF2HudManager::toggleArtilleryState> binding171("toggleArtilleryState", 0, 0, 0, "void", false, Availability::Manager, {}); // 76CE25 table92D2A8, executor 76CE77 -> +2D8 7B1F60
static HudNoArgVoidBinding<&BF2HudManager::toggleUAVState> binding172("toggleUAVState", 0, 0, 0, "void", false, Availability::Manager, {}); // 76CE95 table92D348, executor 76CEE7 -> +2DC 7B2060
static HudNoArgVoidBinding<&BF2HudManager::toggleSupplyState> binding173("toggleSupplyState", 0, 0, 0, "void", false, Availability::Manager, {}); // 76CF05 table92D3E0, executor 76CF57 -> +2E0 7B2180
static HudNoArgVoidBinding<&BF2HudManager::toggleVehicleDropState> binding174("toggleVehicleDropState", 0, 0, 0, "void", false, Availability::Manager, {}); // 76CF75 table92D478, executor 76CFC7 -> +2E4 7B2220
static HudNoArgVoidBinding<&BF2HudManager::artillery> binding175("artillery", 0, 0, 0, "void", false, Availability::Manager, {}); // 76CFE5 table92D518, executor 76D037 -> +2BC 7B22C0
static HudNoArgSilentFalseBinding<&BF2HudManager::satellite> binding176("satellite", 0, 0, 0, "bool", true, Availability::Manager, {}); // 76D055 table92D5A8, executor 76D0A8 -> +2C4 7B2BC0(false)
static HudNoArgSilentFalseBinding<&BF2HudManager::uav> binding177("uav", 0, 0, 0, "bool", true, Availability::Manager, {}); // 76D0D5 table92D638, executor 76D128 -> +2CC 7B29D0(false)
static HudNoArgSilentFalseBinding<&BF2HudManager::supplies> binding178("supplies", 0, 0, 0, "bool", true, Availability::Manager, {}); // 76D155 table92D6C0, executor 76D1A8 -> +2C8 7B2860(false)
static HudNoArgSilentFalseBinding<&BF2HudManager::vehicleDrop> binding179("vehicleDrop", 0, 0, 0, "bool", true, Availability::Manager, {}); // 76D1D5 table92D750, executor 76D228 -> +2D0 7B2660(false)
static HudBoolVoidBinding<&BF2HudManager::accept> binding180("accept", 0, 1, 1, "void", false, Availability::Manager, {"bool"}); // 76D255 table92D7E0, executor 76D313 / converter 76D2C6 (bool A11BCC) -> +2E8 7B2360
static HudNoArgBoolBinding<&BF2HudManager::zoomCommander> binding181("zoomCommander", 0, 0, 0, "bool", true, Availability::Manager, {}); // 76D341 table92D870, executor 76D394 -> +2F0 7B23B0
static SetConsoleClassDefaults defaults7("Squad", 2, 0);
static HudConsoleBinding binding182("leaveSquad", 0, 0, 0, "void", false, Availability::Manager, {}); // 76D3BF table92D908
static SetConsoleClassDefaults defaults8("CommanderMenu", 2, 0);
static HudCommanderNoArgBinding<&HudCommander::singleClick> binding183("singleClick", 0, 0, 0, "void", false, Availability::Commander, {}); // 76D42F table92D990
static HudCommanderNoArgBinding<&HudCommander::rightClick> binding184("rightClick", 0, 0, 0, "void", false, Availability::Commander, {}); // 76D4BB table92DA20
static HudCommanderNoArgBinding<&HudCommander::doubleClick> binding185("doubleClick", 0, 0, 0, "void", false, Availability::Commander, {}); // 76D532 table92DAB0
static HudCommanderNoArgBinding<&HudCommander::deselect> binding186("deselect", 0, 0, 0, "void", false, Availability::Commander, {}); // 76D5A9 table92DB40
static HudCommanderRadioBinding<&HudCommander::sendRadioMessage> binding187("sendRadioMessage", 0, 2, 2, "void", false, Availability::Commander, {"std::string", "TargetChannel"}); // 76D620 table92DBD0
static HudCommanderVehicleDropReloadTimeBinding binding188("vehicleDropReloadTime", 1, 0, 1, "float", true, Availability::Commander, {"float"}); // 76D7B8 table92DC68
static SetConsoleClassDefaults defaults9("SquadMenu", 2, 0);
// SquadMenu command bindings.
static HudSquadNoArgBinding<void, &HudSquad::doubleClickNew> binding189("doubleClickNew", 0, 0, 0, "void", false, Availability::Squad, {}); // 76D8D5 table92DD08, executor 76D927 -> 83D270 (RET)
static HudSquadNoArgBinding<void, &HudSquad::singleClickNew> binding190("singleClickNew", 0, 0, 0, "void", false, Availability::Squad, {}); // 76D94C table92DDA0, executor 76D99E -> 791CE0
static HudSquadNoArgBinding<void, &HudSquad::finishSquadCreate> binding191("createSquad", 0, 0, 0, "void", false, Availability::Squad, {}); // 76D9C3 table92DE38, executor 76DA15 -> 78F9B0
static HudSquadBoolBinding<bool, &HudSquad::setCreateMode> binding192("setSquadCreateSelect", 0, 1, 1, "void", false, Availability::Squad, {"bool"}); // 76DA3A table92DEC8, executor 76DAF8, converter 76DAAB -> 78F5E0
static HudSquadNoArgBinding<void, &HudSquad::squadCreateLockToggle> binding193("squadCreateLockToggle", 0, 0, 0, "void", false, Availability::Squad, {}); // 76DB2D table92DF68, executor 76DB7F -> 78F220
static HudSquadNoArgBinding<bool, &HudSquad::setSquadNameSelect> binding194("setSquadNameSelect", 0, 0, 0, "void", false, Availability::Squad, {}); // 76DBA4 table92E008, executor 76DBF6 -> 78F190
static HudSquadBoolBinding<void, &HudSquad::setShowInviteList> binding195("setShowInviteList", 0, 1, 1, "void", false, Availability::Squad, {"bool"}); // 76DC1B table92E0A0, executor 76DCD9, converter 76DC8C -> 7918E0
static HudSquadNoArgBinding<void, &HudSquad::applyInviteList> binding196("applyInviteList", 0, 0, 0, "void", false, Availability::Squad, {}); // 76DD0E table92E138, executor 76DD60 -> 791FD0
static HudSquadNoArgBinding<void, &HudSquad::doubleClickInvite> binding197("doubleClickInvite", 0, 0, 0, "void", false, Availability::Squad, {}); // 76DD85 table92DD08 = ROW 189's (folded), executor 76D927 -> 83D270 (RET)
static HudSquadNoArgBinding<void, &HudSquad::singleClickInvite> binding198("singleClickInvite", 0, 0, 0, "void", false, Availability::Squad, {}); // 76DDD7 table92E1E0, executor 76DE3E -> 791F50
static SetConsoleClassDefaults defaults10("SquadLeaderInterface", 2, 0);
static HudRadioSelectBinding<&BF2HudManager::getSquadLeaderRadio> binding199("selectOrder", 0, 1, 1, "bool", true, Availability::SquadLeaderRadio, {"std::string"}); // 76DE63 table92E278, executor 76DED6 (+238 -> HudRadio +08)
static HudRadioStringMapBinding<&BF2HudManager::getSquadLeaderRadio> binding200("setStringMap", 0, 3, 3, "void", false, Availability::SquadLeaderRadio, {"std::string", "std::string", "std::string"}); // 76DFA0 table92E308
static SetConsoleClassDefaults defaults11("SquadInterface", 2, 0);
static HudRadioSelectBinding<&BF2HudManager::getSquadRadio> binding201("selectOrder", 0, 1, 1, "bool", true, Availability::SquadRadio, {"std::string"}); // 76E236 table92E3A0, executor 76E2A9 (+22C -> HudRadio +08)
static HudRadioStringMapBinding<&BF2HudManager::getSquadRadio> binding202("setStringMap", 0, 3, 3, "void", false, Availability::SquadRadio, {"std::string", "std::string", "std::string"}); // 76E373 table92E428
static SetConsoleClassDefaults defaults12("CommanderInterface", 2, 0);
static HudRadioSelectBinding<&BF2HudManager::getCommanderRadio> binding203("selectOrder", 0, 1, 1, "bool", true, Availability::CommanderRadio, {"std::string"}); // 76E609 table92E4B0, executor 76E691 (+23C -> HudRadio +08)
static HudRadioStringMapBinding<&BF2HudManager::getCommanderRadio> binding204("setStringMap", 0, 3, 3, "void", false, Availability::CommanderRadio, {"std::string", "std::string", "std::string"}); // 76E75B table92E538
static SetConsoleClassDefaults defaults13("RadioInterface", 2, 0);
static HudRadioSelectBinding<&BF2HudManager::getInfantryRadio> binding205("selectOrder", 0, 1, 1, "bool", true, Availability::InfantryRadio, {"std::string"}); // 76E9DC table92E5C0, executor 76EA64 (+234 -> HudRadio +08)
static HudRadioStringMapBinding<&BF2HudManager::getInfantryRadio> binding206("setStringMap", 0, 3, 3, "void", false, Availability::InfantryRadio, {"std::string", "std::string", "std::string"}); // 76EB2E table92E648
static SetConsoleClassDefaults defaults14("RadioVehicleInterface", 2, 0);
static HudRadioSelectBinding<&BF2HudManager::getVehicleRadio> binding207("selectOrder", 0, 1, 1, "bool", true, Availability::VehicleRadio, {"std::string"}); // 76EDAF table92E6D0, executor 76EE37 (+230 -> HudRadio +08)
static HudRadioStringMapBinding<&BF2HudManager::getVehicleRadio> binding208("setStringMap", 0, 3, 3, "void", false, Availability::VehicleRadio, {"std::string", "std::string", "std::string"}); // 76EF01 table92E758
static SetConsoleClassDefaults defaults15("Radio", 2, 0);
static HudConsoleBinding binding209("sendKitMessage", 0, 0, 0, "void", false, Availability::Manager, {}); // 76F182 table92E7E0
static HudBoolVehicleTypeVoidBinding<&BF2HudManager::sendSpottedMessage> binding210("sendSpottedMessage", 0, 2, 2, "void", false, Availability::Manager, {"bool", "world::VehicleType"}); // 76F1F2 table92E888, executor 76F30A, converter 76F283 -> +13C
static HudNoArgVoidBinding<&BF2HudManager::setSpottedMenuActive> binding211("setSpottedMenuActive", 0, 0, 0, "void", false, Availability::Manager, {}); // 76F33E table92E920, executor 76F390 -> +214
static HudNoArgVoidBinding<&BF2HudManager::callForMedic> binding212("callForMedic", 0, 0, 0, "void", false, Availability::Manager, {}); // 76F3AE table92E9C0, executor 76F400 -> +158
static HudNoArgVoidBinding<&BF2HudManager::callForAmmo> binding213("callForAmmo", 0, 0, 0, "void", false, Availability::Manager, {}); // 76F41E table92EA58, executor 76F470 -> +160
static HudNoArgVoidBinding<&BF2HudManager::callForRepair> binding214("callForRepair", 0, 0, 0, "void", false, Availability::Manager, {}); // 76F48E table92EAE8, executor 76F4E0 -> +164
static HudConsoleBinding binding215("answer", 0, 1, 1, "void", false, Availability::Questions, {"bool"}); // 76F4FE table92EB80
static SetConsoleClassDefaults defaults16("SpawnManager", 2, 0);
static HudKitSelectionIntBinding<&HudKitSelection::selectTeam> binding216("setPlayerTeam", 0, 1, 1, "void", false, Availability::KitSelection, {"int"}); // 76F606 table92EC10; executor 76F6C4 -> +240 then 7A2180, converter 76F677 (int A11E5C)
static HudKitSelectionIntBinding<&HudKitSelection::selectKit> binding217("setPlayerKit", 0, 1, 1, "void", false, Availability::KitSelection, {"int"}); // 76F6EF table92ECA8; executor 76F7AD -> +240 then 7A1EF0, converter 76F760 (int A11E60)
static HudKitSelectionNoArgBinding<&HudKitSelection::suicide> binding218("commitSuicide", 0, 0, 0, "void", false, Availability::KitSelection, {}); // 76F7D8 table92ED40; executor 76F82A -> +240 then 7A1E30, converter 57DD1F
static HudKitSelectionBoolBinding<&HudKitSelection::showSpawnMenu> binding219("toggleMembers", 0, 1, 1, "void", false, Availability::KitSelection, {"bool"}); // 76F84F table92EDD8; executor 76F90D -> +240 then 7A1F90, converter 76F8C0 (bool A11E64)
static HudKitSelectionIntBinding<&HudKitSelection::cycleUnlock> binding220("selectNextUnlock", 0, 1, 1, "void", false, Availability::KitSelection, {"int"}); // 76F942 table92EE70; executor 76FA15 -> +240 then 7A20B0, converter 76F9C8 (int A11E68)
static SetConsoleClassDefaults defaults17("Chat", 2, 0);
static HudConsoleBinding binding221("sayTeam", 0, 1, 1, "void", false, Availability::Manager, {"std::string"}); // 76FA40 table92EF08
static HudConsoleBinding binding222("sayAll", 0, 1, 1, "void", false, Availability::Manager, {"std::string"}); // 76FB78 table92EF90
static SetConsoleClassDefaults defaults18("Scoreboard", 2, 0);
// The setToggleShow command calls setPage. [INFERRED] Method name.
static HudScoreboardIntBinding<&HudScoreboard::setPage> binding223("setToggleShow", 0, 1, 1, "void", false, Availability::Scoreboard, {"int"}); // 76FCB0 table92F018, executor 76FD6E
static HudScoreboardIntBinding<&HudScoreboard::singleClick> binding224("singleClick", 0, 1, 1, "void", false, Availability::Scoreboard, {"int"}); // 76FD99 table92F0B0, executor 76FE6C
static SetConsoleClassDefaults defaultsVoip("VoipList", 2, 0); // 864213, no entries
static SetConsoleClassDefaults defaults19("LevelsList", 2, 0);
// Map-vote list command bindings.
static HudVoteMapIntBinding<&HudVoteMap::setVoteMapShow> binding225("setVoteMapShow", 0, 1, 1, "void", false, Availability::VoteMap, {"int"}); // 76FE97 table92F138, executor 76FF55 -> 785300
static HudVoteMapIntBinding<&HudVoteMap::singleClick> binding226("singleClick", 0, 1, 1, "void", false, Availability::VoteMap, {"int"}); // 76FF80 table92F1D0, executor 770053 -> 785700
static SetConsoleClassDefaults defaults20("IngameHelp", 2, 0);
static HudConsoleBinding binding227("showIngameHelp", 0, 2, 2, "void", false, Availability::Manager, {"std::string", "float"}); // 77007E table92F258
}
std::list<ConsoleObject*>& hudConsoleObjects() { return bindings; }
} } }
#endif
