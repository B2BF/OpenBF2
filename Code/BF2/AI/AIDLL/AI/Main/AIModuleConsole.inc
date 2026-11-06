// GENERATED FILE - do not edit by hand. Regenerate with
//     python3 tools/ai-reconstruct/gen_console_bindings.py
// from libaidll.so (Linux 1.5.3153, ia-32, with symbols). Every address is an ELF address.
//
// The AI image's console bindings: 298 generated classes ConsoleClass<line>, all in the
// anonymous namespace of AIModule.cpp (the original __LINE__ is the class number). Emitted here:
// 298; the 0 left out are listed in place with the reason (their commands stay
// unregistered, as before this file existed). Shared shapes recovered from the image:
//   - constructor (inlined into AIModule.cpp's static initializer, ELF 0x3EEE90): object name,
//     access and category from the SetConsoleClassDefaults in force; function name, m_type,
//     min/max argument count, return-type and argument-type strings (the stringized C++ types),
//     the +0xA1 flag (1 exactly when getReturnValueAsString formats a value), then the
//     needs-conversion flags (`argType != "std::string"`) and `aiList.push_back(this)`.
//   - isObjectActive: `receiver != 0`, the receiver being a global or an AIModule.cpp getter.
//   - executeObjectMethod: one arm per accepted argument count; m_hasReturnValue is stored before
//     the receiver is evaluated; the arm calls receiver->method(arguments) (virtual calls through
//     the receiver's static type) and stores the result when the flag is set. Arms that pass fewer
//     arguments than the target takes rely on the target's C++ default arguments.
//     The verified inline bodies preserve argument copies before that flag, unchecked indices,
//     repeated stores and discarded results; see each class's inline-body note and JSON hash.
//   - setArgFromString(index): `static T arg; std::istringstream in(value); in >> arg;
//     m_args[index - 1] = &arg;` with no fail check (no temporary, unlike Core's BF2_CC_CONVERT).
//   - getReturnValueAsString / range hooks: Core/ConsoleObjectMacro.h's BF2_CC_RETURN_AS_STRING,
//     BF2_CC_NO_RETURN_AS_STRING and BF2_CC_COMMON_HOOKS (one shape each in the image).
// [INFERRED] The original macro names; the class bodies are spelled out because the shared
// BF2_CONSOLE_* families hard-code a different +0xA1 value and the temp-and-commit conversion.

#pragma push_macro("BF2_CC_NAME")

// Converts argument `oneBasedIndex` into its function-local static (one per argument).
#define AIMODULE_CC_EXTRACT(T, oneBasedIndex)                                                     \
        if (index == (oneBasedIndex))                                                            \
        {                                                                                        \
            static T arg;                                                                        \
            std::istringstream in(value);                                                        \
            in >> arg;                                                                           \
            m_args[(oneBasedIndex) - 1] = reinterpret_cast<std::string*>(&arg);                  \
        }

// The same, where the image calls a named extractor that overload resolution would not pick.
#define AIMODULE_CC_EXTRACT_WITH(T, oneBasedIndex, extractor)                                     \
        if (index == (oneBasedIndex))                                                            \
        {                                                                                        \
            static T arg;                                                                        \
            std::istringstream in(value);                                                        \
            extractor(in, arg);                                                                  \
            m_args[(oneBasedIndex) - 1] = reinterpret_cast<std::string*>(&arg);                  \
        }

// Closes the constructor BF2_CC_HEAD_NAMED opened: conversion flags, then this file's list.
#define AIMODULE_CC_TAIL(nArgs)                                                                   \
            for (int i = 0; i < (nArgs); ++i)                                                    \
                m_argNeedsConversion[i] =                                                        \
                    (std::string(m_argTypes[i]) == "std::string") ? 0 : -1;                      \
            aiList.push_back(this);                                                              \
        }


SetConsoleClassDefaults IAIfullAccessSetConsoleClassDefaults("IAI", 2, 0);

SetConsoleClassDefaults AIfullAccessSetConsoleClassDefaults("AI", 2, 0);

// ConsoleClass161 AI.createHeightMap: method, 0..1 arguments, no result.
// ELF isObjectActive 0x40FF70, executeObjectMethod 0x4102D0,
// setArgFromString 0x410090, getReturnValueAsString 0x40FF30, D1 0x40FF10, D0 0x40FED0.
// argc 0: dice::hfe::ai::AerialHeightMap::createHeightMap(int) (call at 0x41031B; ledger done)
// argc 1: dice::hfe::ai::AerialHeightMap::createHeightMap(int) (call at 0x41031B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass161
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createHeightMap", 0, 0, 1, "void", false)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AerialHeightMap::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            AerialHeightMap::m_instance->createHeightMap();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AerialHeightMap::m_instance->createHeightMap(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject161;

// ConsoleClass162 AI.readHeightMapFromFile: method, 0..0 arguments, no result.
// ELF isObjectActive 0x4103E0, executeObjectMethod 0x410510,
// setArgFromString 0x410500, getReturnValueAsString 0x4103A0, D1 0x410380, D0 0x410340.
// argc 0: dice::hfe::ai::AerialHeightMap::readHeightMapFromFile() (call at 0x410549; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass162
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("readHeightMapFromFile", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AerialHeightMap::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            AerialHeightMap::m_instance->readHeightMapFromFile();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject162;

// ConsoleClass165 AI.init: method, 1..1 arguments, no result.
// ELF isObjectActive 0x4105F0, executeObjectMethod 0x410950,
// setArgFromString 0x410710, getReturnValueAsString 0x4105B0, D1 0x410590, D0 0x410550.
// argc 1: dice::hfe::ai::AIConsole::init(int) (call at 0x410991; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass165
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("init", 0, 1, 1, "void", false)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->init(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject165;

// ConsoleClass168 AI.debug: method, 0..0 arguments, no result.
// ELF isObjectActive 0x410A40, executeObjectMethod 0x410B70,
// setArgFromString 0x410B60, getReturnValueAsString 0x410A00, D1 0x4109E0, D0 0x4109A0.
// argc 0: dice::hfe::ai::AIConsole::debug() (call at 0x410BA9; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass168
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("debug", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->debug();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject168;

// ConsoleClass171 AI.writeBotPlans: method, 0..0 arguments, no result.
// ELF isObjectActive 0x410C50, executeObjectMethod 0x410D80,
// setArgFromString 0x410D70, getReturnValueAsString 0x410C10, D1 0x410BF0, D0 0x410BB0.
// argc 0: dice::hfe::ai::BotManager::writeBotPlans() (slot +0x7C at 0x410DBB; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass171
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("writeBotPlans", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManager::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            IBotManager::instance->writeBotPlans();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject171;

// ConsoleClass174 AI.aiSystemQuotient: property, 0..1 arguments, returns double.
// ELF isObjectActive 0x411060, executeObjectMethod 0x4113D0,
// setArgFromString 0x411190, getReturnValueAsString 0x410E20, D1 0x410E00, D0 0x410DC0.
// argc 0: dice::hfe::ai::AIMain::getAISystemQuotient() const (slot +0x5C at 0x411438; ledger done)
// argc 1: dice::hfe::ai::AIMain::setAISystemQuotient(double const&) (slot +0x58 at 0x411420; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass174
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("aiSystemQuotient", 1, 0, 1, "double", true)
            m_argTypes[0] = "double";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getAISystemQuotient();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setAISystemQuotient(BF2_CC_ARG(double, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(double, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject174;

// ConsoleClass175 AI.setAISystemComponentsQuotient: method, 2..2 arguments, no result.
// ELF isObjectActive 0x4114F0, executeObjectMethod 0x411AA0,
// setArgFromString 0x411620, getReturnValueAsString 0x4114B0, D1 0x411490, D0 0x411450.
// argc 2: dice::hfe::ai::AIMain::setAISystemComponentsQuotient(unsigned int, unsigned int) (call at 0x411AF3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass175
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setAISystemComponentsQuotient", 0, 2, 2, "void", false)
            m_argTypes[0] = "hfe::uint";
            m_argTypes[1] = "hfe::uint";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setAISystemComponentsQuotient(BF2_CC_ARG(hfe::uint, 0), BF2_CC_ARG(hfe::uint, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(hfe::uint, 1)
        AIMODULE_CC_EXTRACT(hfe::uint, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject175;

// ConsoleClass176 AI.getBotManagerQuotient: method, 0..0 arguments, double result discarded.
// ELF isObjectActive 0x411BA0, executeObjectMethod 0x411CE0,
// setArgFromString 0x411CD0, getReturnValueAsString 0x411B60, D1 0x411B40, D0 0x411B00.
// argc 0: verified inline body, discarded quotient getter; evaluates only the receiver
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass176
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getBotManagerQuotient", 0, 0, 0, "double", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            (void)getActiveAIMain();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject176;

// ConsoleClass177 AI.getSAIQuotient: method, 0..0 arguments, double result discarded.
// ELF isObjectActive 0x411DC0, executeObjectMethod 0x411F00,
// setArgFromString 0x411EF0, getReturnValueAsString 0x411D80, D1 0x411D60, D0 0x411D20.
// argc 0: verified inline body, discarded quotient getter; evaluates only the receiver
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass177
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getSAIQuotient", 0, 0, 0, "double", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            (void)getActiveAIMain();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject177;

// ConsoleClass178 AI.botStatisticUpdateSpeed: property, 0..1 arguments, returns double.
// ELF isObjectActive 0x4121E0, executeObjectMethod 0x412550,
// setArgFromString 0x412310, getReturnValueAsString 0x411FA0, D1 0x411F80, D0 0x411F40.
// argc 0: verified inline body, statistic speed; ia-32 negates exactly when value < 0 (retains -0 and unordered values)
// argc 1: verified inline body, statistic speed; ia-32 negates exactly when value < 0 (retains -0 and unordered values)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass178
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("botStatisticUpdateSpeed", 1, 0, 1, "double", true)
            m_argTypes[0] = "double";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->m_botStatisticUpdateSpeed;
        }
        else if (m_argCount == 1)
        {
            const double value = BF2_CC_ARG(double, 0);
            m_hasReturnValue = false;
            AIMain* const object = getActiveAIMain();
            object->m_botStatisticUpdateSpeed = value < 0.0 ? -value : value;
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(double, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject178;

// ConsoleClass181 AI.getBotCount: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x412870, executeObjectMethod 0x4129A0,
// setArgFromString 0x412990, getReturnValueAsString 0x412630, D1 0x412610, D0 0x4125D0.
// argc 0: dice::hfe::ai::BotManager::getBotCount() const (slot +0x8 at 0x4129D5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass181
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getBotCount", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManager::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManager::instance->getBotCount();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject181;

// ConsoleClass182 AI.getNumberOfActiveBots: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x412CA0, executeObjectMethod 0x412DD0,
// setArgFromString 0x412DC0, getReturnValueAsString 0x412A60, D1 0x412A40, D0 0x412A00.
// argc 0: dice::hfe::ai::BotManager::getNumberOfActiveBots() const (slot +0xC at 0x412E05; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass182
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getNumberOfActiveBots", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManager::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManager::instance->getNumberOfActiveBots();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject182;

// ConsoleClass183 AI.getNumberOfDeactivedBots: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x4130D0, executeObjectMethod 0x413200,
// setArgFromString 0x4131F0, getReturnValueAsString 0x412E90, D1 0x412E70, D0 0x412E30.
// argc 0: dice::hfe::ai::BotManager::getNumberOfDeactivedBots() const (slot +0x10 at 0x413235; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass183
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getNumberOfDeactivedBots", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManager::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManager::instance->getNumberOfDeactivedBots();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject183;

// ConsoleClass186 AI.addBot: method, 4..4 arguments, returns int.
// ELF isObjectActive 0x413530, executeObjectMethod 0x413FC0,
// setArgFromString 0x413650, getReturnValueAsString 0x4132C0, D1 0x4132A0, D0 0x413260.
// argc 4: dice::hfe::ai::AIConsole::addBot(std::string, int, std::string, std::string) (call at 0x4140AF; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass186
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addBot", 0, 4, 4, "int", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "int";
            m_argTypes[2] = "std::string";
            m_argTypes[3] = "std::string";
    AIMODULE_CC_TAIL(4)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 4)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->addBot(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(int, 1), BF2_CC_ARG(std::string, 2), BF2_CC_ARG(std::string, 3));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(int, 2)
        AIMODULE_CC_EXTRACT(std::string, 3)
        AIMODULE_CC_EXTRACT(std::string, 4)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject186;

// ConsoleClass187 AI.addUnspawnedBot: method, 3..3 arguments, returns int.
// ELF isObjectActive 0x4144B0, executeObjectMethod 0x414CE0,
// setArgFromString 0x4145D0, getReturnValueAsString 0x414240, D1 0x414220, D0 0x4141E0.
// argc 3: dice::hfe::ai::AIConsole::addUnspawnedBot(std::string, int, std::string) (call at 0x414D80; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass187
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addUnspawnedBot", 0, 3, 3, "int", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "int";
            m_argTypes[2] = "std::string";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->addUnspawnedBot(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(int, 1), BF2_CC_ARG(std::string, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(int, 2)
        AIMODULE_CC_EXTRACT(std::string, 3)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject187;

// ConsoleClass188 AI.deactivateBot: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x4150E0, executeObjectMethod 0x415450,
// setArgFromString 0x415210, getReturnValueAsString 0x414EA0, D1 0x414E80, D0 0x414E40.
// argc 1: dice::hfe::ai::AIMain::deactivateBot(int) (slot +0x1C at 0x41548D; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass188
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("deactivateBot", 0, 1, 1, "bool", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->deactivateBot(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject188;

// ConsoleClass189 AI.updateBot: method, 1..1 arguments, no result.
// ELF isObjectActive 0x415560, executeObjectMethod 0x4158C0,
// setArgFromString 0x415680, getReturnValueAsString 0x415520, D1 0x415500, D0 0x4154C0.
// argc 1: dice::hfe::ai::AIConsole::updateBot(int) (call at 0x415901; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass189
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("updateBot", 0, 1, 1, "void", false)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->updateBot(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject189;

// ConsoleClass190 AI.killBotId: method, 1..1 arguments, returns std::string.
// ELF isObjectActive 0x415C30, executeObjectMethod 0x415F90,
// setArgFromString 0x415D50, getReturnValueAsString 0x4159F0, D1 0x415980, D0 0x415910.
// argc 1: dice::hfe::ai::AIConsole::killBotId(int) (call at 0x415FDB; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass190
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("killBotId", 0, 1, 1, "std::string", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->killBotId(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject190;

// ConsoleClass191 AI.killBotName: method, 1..1 arguments, returns std::string.
// ELF isObjectActive 0x416350, executeObjectMethod 0x4166F0,
// setArgFromString 0x416470, getReturnValueAsString 0x416110, D1 0x4160A0, D0 0x416030.
// argc 1: dice::hfe::ai::AIConsole::killBotName(std::string const&) (call at 0x416754; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass191
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("killBotName", 0, 1, 1, "std::string", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->killBotName(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject191;

// ConsoleClass192 AI.killAllBots: method, 0..0 arguments, no result.
// ELF isObjectActive 0x416870, executeObjectMethod 0x4169A0,
// setArgFromString 0x416990, getReturnValueAsString 0x416830, D1 0x416810, D0 0x4167D0.
// argc 0: dice::hfe::ai::AIConsole::killAllBots() (call at 0x4169D9; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass192
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("killAllBots", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->killAllBots();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject192;

// ConsoleClass195 AI.botGiveOrder: method, 3..3 arguments, no result.
// ELF isObjectActive 0x416A80, executeObjectMethod 0x4172B0,
// setArgFromString 0x416BA0, getReturnValueAsString 0x416A40, D1 0x416A20, D0 0x4169E0.
// argc 3: dice::hfe::ai::AIConsole::botGiveOrder(int, std::string, std::string) (call at 0x41735E; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass195
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("botGiveOrder", 0, 3, 3, "void", false)
            m_argTypes[0] = "int";
            m_argTypes[1] = "std::string";
            m_argTypes[2] = "std::string";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->botGiveOrder(BF2_CC_ARG(int, 0), BF2_CC_ARG(std::string, 1), BF2_CC_ARG(std::string, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
        AIMODULE_CC_EXTRACT(std::string, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject195;

// ConsoleClass196 AI.botSetWaypoints_Patrol: method, 5..6 arguments, no result.
// ELF isObjectActive 0x417490, executeObjectMethod 0x418570,
// setArgFromString 0x4175B0, getReturnValueAsString 0x417450, D1 0x417430, D0 0x4173F0.
// argc 5: dice::hfe::ai::AIConsole::botSetWaypoints_Patrol(int, float, int, dice::hfe::Vec3, dice::hfe::Vec3, dice::hfe::Vec3) (call at 0x418671; ledger stop)
// argc 6: dice::hfe::ai::AIConsole::botSetWaypoints_Patrol(int, float, int, dice::hfe::Vec3, dice::hfe::Vec3, dice::hfe::Vec3) (call at 0x418671; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass196
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("botSetWaypoints_Patrol", 0, 5, 6, "void", false)
            m_argTypes[0] = "int";
            m_argTypes[1] = "float";
            m_argTypes[2] = "int";
            m_argTypes[3] = "hfe::Vec3";
            m_argTypes[4] = "hfe::Vec3";
            m_argTypes[5] = "hfe::Vec3";
    AIMODULE_CC_TAIL(6)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 5)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->botSetWaypoints_Patrol(BF2_CC_ARG(int, 0), BF2_CC_ARG(float, 1), BF2_CC_ARG(int, 2), BF2_CC_ARG(hfe::Vec3, 3), BF2_CC_ARG(hfe::Vec3, 4));
        }
        else if (m_argCount == 6)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->botSetWaypoints_Patrol(BF2_CC_ARG(int, 0), BF2_CC_ARG(float, 1), BF2_CC_ARG(int, 2), BF2_CC_ARG(hfe::Vec3, 3), BF2_CC_ARG(hfe::Vec3, 4), BF2_CC_ARG(hfe::Vec3, 5));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
        AIMODULE_CC_EXTRACT(float, 2)
        AIMODULE_CC_EXTRACT(int, 3)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 4)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 5)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 6)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject196;

// ConsoleClass197 AI.botSetWaypoints_Patrol3d: method, 5..6 arguments, no result.
// ELF isObjectActive 0x4187D0, executeObjectMethod 0x4198B0,
// setArgFromString 0x4188F0, getReturnValueAsString 0x418790, D1 0x418770, D0 0x418730.
// argc 5: dice::hfe::ai::AIConsole::botSetWaypoints_Patrol3d(int, float, int, dice::hfe::Vec3, dice::hfe::Vec3, dice::hfe::Vec3) (call at 0x4199B1; ledger stop)
// argc 6: dice::hfe::ai::AIConsole::botSetWaypoints_Patrol3d(int, float, int, dice::hfe::Vec3, dice::hfe::Vec3, dice::hfe::Vec3) (call at 0x4199B1; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass197
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("botSetWaypoints_Patrol3d", 0, 5, 6, "void", false)
            m_argTypes[0] = "int";
            m_argTypes[1] = "float";
            m_argTypes[2] = "int";
            m_argTypes[3] = "hfe::Vec3";
            m_argTypes[4] = "hfe::Vec3";
            m_argTypes[5] = "hfe::Vec3";
    AIMODULE_CC_TAIL(6)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 5)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->botSetWaypoints_Patrol3d(BF2_CC_ARG(int, 0), BF2_CC_ARG(float, 1), BF2_CC_ARG(int, 2), BF2_CC_ARG(hfe::Vec3, 3), BF2_CC_ARG(hfe::Vec3, 4));
        }
        else if (m_argCount == 6)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->botSetWaypoints_Patrol3d(BF2_CC_ARG(int, 0), BF2_CC_ARG(float, 1), BF2_CC_ARG(int, 2), BF2_CC_ARG(hfe::Vec3, 3), BF2_CC_ARG(hfe::Vec3, 4), BF2_CC_ARG(hfe::Vec3, 5));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
        AIMODULE_CC_EXTRACT(float, 2)
        AIMODULE_CC_EXTRACT(int, 3)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 4)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 5)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 6)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject197;

// ConsoleClass198 AI.botMoveTo: method, 3..3 arguments, no result.
// ELF isObjectActive 0x419B10, executeObjectMethod 0x41A370,
// setArgFromString 0x419C30, getReturnValueAsString 0x419AD0, D1 0x419AB0, D0 0x419A70.
// argc 3: dice::hfe::ai::AIConsole::botMoveTo(float, dice::hfe::Vec3, int) (call at 0x41A3F0; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass198
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("botMoveTo", 0, 3, 3, "void", false)
            m_argTypes[0] = "float";
            m_argTypes[1] = "hfe::Vec3";
            m_argTypes[2] = "int";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->botMoveTo(BF2_CC_ARG(float, 0), BF2_CC_ARG(hfe::Vec3, 1), BF2_CC_ARG(int, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 2)
        AIMODULE_CC_EXTRACT(int, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject198;

// ConsoleClass199 AI.botFlyTo: method, 5..5 arguments, no result.
// ELF isObjectActive 0x41A4A0, executeObjectMethod 0x41B150,
// setArgFromString 0x41A5C0, getReturnValueAsString 0x41A460, D1 0x41A440, D0 0x41A400.
// argc 5: dice::hfe::ai::AIConsole::botFlyTo(int, float, float, float, dice::hfe::Vec3) (call at 0x41B1E2; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass199
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("botFlyTo", 0, 5, 5, "void", false)
            m_argTypes[0] = "int";
            m_argTypes[1] = "float";
            m_argTypes[2] = "float";
            m_argTypes[3] = "float";
            m_argTypes[4] = "hfe::Vec3";
    AIMODULE_CC_TAIL(5)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 5)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->botFlyTo(BF2_CC_ARG(int, 0), BF2_CC_ARG(float, 1), BF2_CC_ARG(float, 2), BF2_CC_ARG(float, 3), BF2_CC_ARG(hfe::Vec3, 4));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
        AIMODULE_CC_EXTRACT(float, 2)
        AIMODULE_CC_EXTRACT(float, 3)
        AIMODULE_CC_EXTRACT(float, 4)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 5)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject199;

// ConsoleClass202 AI.createSAI: method, 0..0 arguments, no result.
// ELF isObjectActive 0x41B290, executeObjectMethod 0x41B3D0,
// setArgFromString 0x41B3C0, getReturnValueAsString 0x41B250, D1 0x41B230, D0 0x41B1F0.
// argc 0: dice::hfe::ai::AIMain::createSAI() (call at 0x41B406; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass202
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createSAI", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->createSAI();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject202;

// ConsoleClass203 AI.saiEnable: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x41B6B0, executeObjectMethod 0x41BA20,
// setArgFromString 0x41B7E0, getReturnValueAsString 0x41B470, D1 0x41B450, D0 0x41B410.
// argc 0: dice::hfe::ai::AIMain::getSAIEnable() (slot +0x38 at 0x41BA91; ledger done)
// argc 1: dice::hfe::ai::AIMain::setSAIEnable(bool) (call at 0x41BA70; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass203
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("saiEnable", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getSAIEnable();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setSAIEnable(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject203;

// ConsoleClass204 AI.saiMapXDimension: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x41BD80, executeObjectMethod 0x41C0F0,
// setArgFromString 0x41BEB0, getReturnValueAsString 0x41BB10, D1 0x41BAF0, D0 0x41BAB0.
// argc 0: dice::hfe::ai::AIMain::getSAIMapXDimension() const (call at 0x41C156; ledger done)
// argc 1: dice::hfe::ai::AIMain::setSAIMapXDimension(int) (call at 0x41C13E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass204
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("saiMapXDimension", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getSAIMapXDimension();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setSAIMapXDimension(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject204;

// ConsoleClass205 AI.saiMapYDimension: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x41C440, executeObjectMethod 0x41C7B0,
// setArgFromString 0x41C570, getReturnValueAsString 0x41C1D0, D1 0x41C1B0, D0 0x41C170.
// argc 0: dice::hfe::ai::AIMain::getSAIMapYDimension() const (call at 0x41C816; ledger done)
// argc 1: dice::hfe::ai::AIMain::setSAIMapYDimension(int) (call at 0x41C7FE; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass205
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("saiMapYDimension", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getSAIMapYDimension();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setSAIMapYDimension(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject205;

// ConsoleClass206 AI.addSAIStrategy: method, 2..2 arguments, no result.
// ELF isObjectActive 0x41C8D0, executeObjectMethod 0x41CEC0,
// setArgFromString 0x41C9F0, getReturnValueAsString 0x41C890, D1 0x41C870, D0 0x41C830.
// argc 2: dice::hfe::ai::AIConsole::addSAIStrategy(int, std::string) (call at 0x41CF40; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass206
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addSAIStrategy", 0, 2, 2, "void", false)
            m_argTypes[0] = "int";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->addSAIStrategy(BF2_CC_ARG(int, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject206;

// ConsoleClass209 AI.renderSide: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x41D260, executeObjectMethod 0x41D5D0,
// setArgFromString 0x41D390, getReturnValueAsString 0x41CFF0, D1 0x41CFD0, D0 0x41CF90.
// argc 0: dice::hfe::ai::AIMain::getRenderSide() const (call at 0x41D636; ledger done)
// argc 1: dice::hfe::ai::AIMain::setRenderSide(int) (call at 0x41D61E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass209
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("renderSide", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getRenderSide();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setRenderSide(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject209;

// ConsoleClass211 AI.validateDataStructuresEnabled: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x41D8F0, executeObjectMethod 0x41DC60,
// setArgFromString 0x41DA20, getReturnValueAsString 0x41D6B0, D1 0x41D690, D0 0x41D650.
// argc 0: dice::hfe::ai::AIMain::getValidateDataStructuresEnabled() const (call at 0x41DCCF; ledger done)
// argc 1: dice::hfe::ai::AIMain::setValidateDataStructuresEnabled(bool) (call at 0x41DCB0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass211
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("validateDataStructuresEnabled", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getValidateDataStructuresEnabled();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setValidateDataStructuresEnabled(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject211;

// ConsoleClass212 AI.showBotUrgencies: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x41DF90, executeObjectMethod 0x41E300,
// setArgFromString 0x41E0C0, getReturnValueAsString 0x41DD50, D1 0x41DD30, D0 0x41DCF0.
// argc 0: dice::hfe::ai::AIMain::getShowBotUrgencies() const (call at 0x41E36F; ledger done)
// argc 1: dice::hfe::ai::AIMain::setShowBotUrgencies(bool) (call at 0x41E350; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass212
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showBotUrgencies", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getShowBotUrgencies();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setShowBotUrgencies(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject212;

// ConsoleClass213 AI.showBotSpawnCoord: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x41E630, executeObjectMethod 0x41E9A0,
// setArgFromString 0x41E760, getReturnValueAsString 0x41E3F0, D1 0x41E3D0, D0 0x41E390.
// argc 0: dice::hfe::ai::AIMain::getShowBotSpawnCoord() const (call at 0x41EA0F; ledger done)
// argc 1: dice::hfe::ai::AIMain::setShowBotSpawnCoord(bool) (call at 0x41E9F0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass213
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showBotSpawnCoord", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getShowBotSpawnCoord();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setShowBotSpawnCoord(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject213;

// ConsoleClass214 AI.showStrategies: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x41ECD0, executeObjectMethod 0x41F040,
// setArgFromString 0x41EE00, getReturnValueAsString 0x41EA90, D1 0x41EA70, D0 0x41EA30.
// argc 0: dice::hfe::ai::AIMain::getShowStrategies() const (call at 0x41F0AF; ledger done)
// argc 1: dice::hfe::ai::AIMain::setShowStrategies(bool) (call at 0x41F090; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass214
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showStrategies", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getShowStrategies();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setShowStrategies(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject214;

// ConsoleClass215 AI.showSAIStats: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x41F370, executeObjectMethod 0x41F6E0,
// setArgFromString 0x41F4A0, getReturnValueAsString 0x41F130, D1 0x41F110, D0 0x41F0D0.
// argc 0: dice::hfe::ai::AIMain::getShowSAIStats() const (call at 0x41F74F; ledger done)
// argc 1: dice::hfe::ai::AIMain::setShowSAIStats(bool) (call at 0x41F730; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass215
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showSAIStats", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getShowSAIStats();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setShowSAIStats(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject215;

// ConsoleClass216 AI.showBotStats: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x41FA10, executeObjectMethod 0x41FD80,
// setArgFromString 0x41FB40, getReturnValueAsString 0x41F7D0, D1 0x41F7B0, D0 0x41F770.
// argc 0: dice::hfe::ai::AIMain::getShowBotStats() const (call at 0x41FDEF; ledger done)
// argc 1: dice::hfe::ai::AIMain::setShowBotStats(bool) (call at 0x41FDD0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass216
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showBotStats", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getShowBotStats();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setShowBotStats(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject216;

// ConsoleClass217 AI.showBotSense: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x4200B0, executeObjectMethod 0x420410,
// setArgFromString 0x4201D0, getReturnValueAsString 0x41FE70, D1 0x41FE50, D0 0x41FE10.
// argc 0: dice::hfe::ai::BotManager::getRenderBotSensing() const (slot +0x78 at 0x420484; ledger done)
// argc 1: dice::hfe::ai::BotManager::setRenderBotSensing(bool) (slot +0x74 at 0x420465; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass217
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showBotSense", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IBotManager::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManager::instance->getRenderBotSensing();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            IBotManager::instance->setRenderBotSensing(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject217;

// ConsoleClass218 AI.showStrategicAreas: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x420740, executeObjectMethod 0x420AB0,
// setArgFromString 0x420870, getReturnValueAsString 0x420500, D1 0x4204E0, D0 0x4204A0.
// argc 0: dice::hfe::ai::AIMain::getShowStrategicAreas() const (call at 0x420B1F; ledger done)
// argc 1: dice::hfe::ai::AIMain::setShowStrategicAreas(bool) (call at 0x420B00; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass218
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showStrategicAreas", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveAIMain()->getShowStrategicAreas();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->setShowStrategicAreas(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject218;

// ConsoleClass219 AI.showAll: method, 1..1 arguments, no result.
// ELF isObjectActive 0x420BE0, executeObjectMethod 0x420F50,
// setArgFromString 0x420D10, getReturnValueAsString 0x420BA0, D1 0x420B80, D0 0x420B40.
// argc 1: dice::hfe::ai::AIMain::showAll(bool) (call at 0x420F96; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass219
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showAll", 0, 1, 1, "void", false)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->showAll(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject219;

// ConsoleClass220 AI.showStrategic: method, 1..1 arguments, no result.
// ELF isObjectActive 0x421040, executeObjectMethod 0x4213B0,
// setArgFromString 0x421170, getReturnValueAsString 0x421000, D1 0x420FE0, D0 0x420FA0.
// argc 1: dice::hfe::ai::AIMain::showStrategic(bool) (call at 0x4213F6; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass220
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showStrategic", 0, 1, 1, "void", false)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->showStrategic(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject220;

// ConsoleClass221 AI.showBot: method, 1..1 arguments, no result.
// ELF isObjectActive 0x4214A0, executeObjectMethod 0x421810,
// setArgFromString 0x4215D0, getReturnValueAsString 0x421460, D1 0x421440, D0 0x421400.
// argc 1: dice::hfe::ai::AIMain::showBot(bool) (call at 0x421856; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass221
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showBot", 0, 1, 1, "void", false)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveAIMain() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveAIMain()->showBot(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject221;

// ConsoleClass223 AI.getRendCameraPosition: method, 0..0 arguments, returns hfe::Vec3.
// ELF isObjectActive 0x421B60, executeObjectMethod 0x421C90,
// setArgFromString 0x421C80, getReturnValueAsString 0x4218C0, D1 0x4218A0, D0 0x421860.
// argc 0: dice::hfe::ai::AIConsole::getRendCameraPosition() (call at 0x421CD3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass223
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getRendCameraPosition", 0, 0, 0, "hfe::Vec3", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->getRendCameraPosition();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::Vec3 m_returnValue;
};
BF2_CC_NAME consoleObject223;

// ConsoleClass224 AI.getRendCameraRotation: method, 0..0 arguments, returns hfe::Vec3.
// ELF isObjectActive 0x422010, executeObjectMethod 0x422140,
// setArgFromString 0x422130, getReturnValueAsString 0x421D70, D1 0x421D50, D0 0x421D10.
// argc 0: dice::hfe::ai::AIConsole::getRendCameraRotation() (call at 0x422183; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass224
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getRendCameraRotation", 0, 0, 0, "hfe::Vec3", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->getRendCameraRotation();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::Vec3 m_returnValue;
};
BF2_CC_NAME consoleObject224;

// ConsoleClass228 AI.getCurrentPitchAngle: method, 0..0 arguments, returns float.
// ELF isObjectActive 0x422460, executeObjectMethod 0x422590,
// setArgFromString 0x422580, getReturnValueAsString 0x422220, D1 0x422200, D0 0x4221C0.
// argc 0: dice::hfe::ai::AIConsole::getCurrentPitchAngle() const (call at 0x4225C3; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass228
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getCurrentPitchAngle", 0, 0, 0, "float", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->getCurrentPitchAngle();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject228;

// ConsoleClass229 AI.getCurrentYawAngle: method, 0..0 arguments, returns float.
// ELF isObjectActive 0x422890, executeObjectMethod 0x4229C0,
// setArgFromString 0x4229B0, getReturnValueAsString 0x422650, D1 0x422630, D0 0x4225F0.
// argc 0: dice::hfe::ai::AIConsole::getCurrentYawAngle() const (call at 0x4229F3; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass229
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getCurrentYawAngle", 0, 0, 0, "float", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->getCurrentYawAngle();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject229;

// ConsoleClass248 AI.addPlacement: method, 6..6 arguments, returns bool.
// ELF isObjectActive 0x422CC0, executeObjectMethod 0x423D10,
// setArgFromString 0x422DE0, getReturnValueAsString 0x422A80, D1 0x422A60, D0 0x422A20.
// argc 6: dice::hfe::ai::BotPlacementManager::addPlacement(std::string const&, dice::hfe::Vec3 const&, dice::hfe::Vec3 const&, std::string const&, int, std::string const&) (call at 0x423DFC; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass248
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addPlacement", 0, 6, 6, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "hfe::Vec3";
            m_argTypes[2] = "hfe::Vec3";
            m_argTypes[3] = "std::string";
            m_argTypes[4] = "int";
            m_argTypes[5] = "std::string";
    AIMODULE_CC_TAIL(6)
    virtual bool isObjectActive() const { return BotPlacementManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 6)
        {
            m_hasReturnValue = true;
            m_returnValue = BotPlacementManager::m_instance->addPlacement(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(hfe::Vec3, 1), BF2_CC_ARG(hfe::Vec3, 2), BF2_CC_ARG(std::string, 3), BF2_CC_ARG(int, 4), BF2_CC_ARG(std::string, 5));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 2)
        AIMODULE_CC_EXTRACT(hfe::Vec3, 3)
        AIMODULE_CC_EXTRACT(std::string, 4)
        AIMODULE_CC_EXTRACT(int, 5)
        AIMODULE_CC_EXTRACT(std::string, 6)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject248;

// ConsoleClass249 AI.spawnBot: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x424140, executeObjectMethod 0x4244A0,
// setArgFromString 0x424260, getReturnValueAsString 0x423F00, D1 0x423EE0, D0 0x423EA0.
// argc 1: dice::hfe::ai::BotPlacementManager::spawnBot(int) (call at 0x4244DB; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass249
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("spawnBot", 0, 1, 1, "bool", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return BotPlacementManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = BotPlacementManager::m_instance->spawnBot(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject249;

// ConsoleClass252 AI.pause: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x4247A0, executeObjectMethod 0x424B00,
// setArgFromString 0x4248C0, getReturnValueAsString 0x424560, D1 0x424540, D0 0x424500.
// argc 0: dice::hfe::ai::BFTimer::getPause() (slot +0x1C at 0x424B74; bf2 ELF 0x841C080)
// argc 1: dice::hfe::ai::BFTimer::setPause(bool) (slot +0x18 at 0x424B55; bf2 ELF 0x841C060)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass252
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("pause", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getPause();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->setPause(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject252;

SetConsoleClassDefaults aiSettingsfullAccessSetConsoleClassDefaults("aiSettings", 2, 0);

// ConsoleClass259 aiSettings.setMaxNBots: method, 1..1 arguments, no result.
// ELF isObjectActive 0x424C30, executeObjectMethod 0x424F90,
// setArgFromString 0x424D50, getReturnValueAsString 0x424BF0, D1 0x424BD0, D0 0x424B90.
// argc 1: dice::hfe::ai::AISettings::setMaxNBots(int) (slot +0x0 at 0x424FD3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass259
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setMaxNBots", 0, 1, 1, "void", false)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setMaxNBots(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject259;

// ConsoleClass260 aiSettings.maxBotsIncludeHumans: method, 1..1 arguments, no result.
// ELF isObjectActive 0x425080, executeObjectMethod 0x4253E0,
// setArgFromString 0x4251A0, getReturnValueAsString 0x425040, D1 0x425020, D0 0x424FE0.
// argc 1: dice::hfe::ai::AISettings::maxBotsIncludeHumans(bool) (slot +0x4 at 0x42542B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass260
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("maxBotsIncludeHumans", 0, 1, 1, "void", false)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->maxBotsIncludeHumans(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject260;

// ConsoleClass261 aiSettings.setIgnoreCommanderDistance: method, 1..1 arguments, no result.
// ELF isObjectActive 0x4254D0, executeObjectMethod 0x425830,
// setArgFromString 0x4255F0, getReturnValueAsString 0x425490, D1 0x425470, D0 0x425430.
// argc 1: dice::hfe::ai::AISettings::setIgnoreCommanderDistance(float) (slot +0x8 at 0x425873; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass261
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setIgnoreCommanderDistance", 0, 1, 1, "void", false)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setIgnoreCommanderDistance(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject261;

// ConsoleClass262 aiSettings.setMaxNumberOfBehaviours: method, 1..1 arguments, no result.
// ELF isObjectActive 0x425920, executeObjectMethod 0x425C80,
// setArgFromString 0x425A40, getReturnValueAsString 0x4258E0, D1 0x4258C0, D0 0x425880.
// argc 1: dice::hfe::ai::AISettings::setMaxNumberOfBehaviours(int) (slot +0xC at 0x425CC3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass262
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setMaxNumberOfBehaviours", 0, 1, 1, "void", false)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setMaxNumberOfBehaviours(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject262;

// ConsoleClass263 aiSettings.setViewDistance: method, 1..1 arguments, no result.
// ELF isObjectActive 0x425D70, executeObjectMethod 0x4260D0,
// setArgFromString 0x425E90, getReturnValueAsString 0x425D30, D1 0x425D10, D0 0x425CD0.
// argc 1: dice::hfe::ai::AISettings::setViewDistance(float) (slot +0x34 at 0x426113; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass263
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setViewDistance", 0, 1, 1, "void", false)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setViewDistance(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject263;

// ConsoleClass264 aiSettings.setStatsViewDistance: method, 1..1 arguments, no result.
// ELF isObjectActive 0x4261C0, executeObjectMethod 0x426520,
// setArgFromString 0x4262E0, getReturnValueAsString 0x426180, D1 0x426160, D0 0x426120.
// argc 1: dice::hfe::ai::AISettings::setStatsViewDistance(float) (slot +0x38 at 0x426563; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass264
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setStatsViewDistance", 0, 1, 1, "void", false)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setStatsViewDistance(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject264;

// ConsoleClass265 aiSettings.setDrawBotStats: method, 1..1 arguments, no result.
// ELF isObjectActive 0x426610, executeObjectMethod 0x426970,
// setArgFromString 0x426730, getReturnValueAsString 0x4265D0, D1 0x4265B0, D0 0x426570.
// argc 1: dice::hfe::ai::AISettings::setDrawBotStats(bool) (slot +0x3C at 0x4269BB; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass265
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setDrawBotStats", 0, 1, 1, "void", false)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setDrawBotStats(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject265;

// ConsoleClass266 aiSettings.setInformationGridDimension: method, 1..1 arguments, no result.
// ELF isObjectActive 0x426A60, executeObjectMethod 0x426DC0,
// setArgFromString 0x426B80, getReturnValueAsString 0x426A20, D1 0x426A00, D0 0x4269C0.
// argc 1: dice::hfe::ai::AISettings::setInformationGridDimension(int) (slot +0x40 at 0x426E03; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass266
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setInformationGridDimension", 0, 1, 1, "void", false)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setInformationGridDimension(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject266;

// ConsoleClass267 aiSettings.setVehicle: method, 1..1 arguments, no result.
// ELF isObjectActive 0x426EB0, executeObjectMethod 0x427250,
// setArgFromString 0x426FD0, getReturnValueAsString 0x426E70, D1 0x426E50, D0 0x426E10.
// argc 1: dice::hfe::ai::AISettings::setVehicle(std::string const&) (slot +0x5C at 0x4272AF; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass267
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setVehicle", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setVehicle(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject267;

// ConsoleClass268 aiSettings.setVehicleClusterMapping: method, 2..2 arguments, no result.
// ELF isObjectActive 0x427380, executeObjectMethod 0x427990,
// setArgFromString 0x4274A0, getReturnValueAsString 0x427340, D1 0x427320, D0 0x4272E0.
// argc 2: dice::hfe::ai::AISettings::setVehicleClusterMapping(std::string const&, std::string const&) (slot +0x60 at 0x427A10; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass268
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setVehicleClusterMapping", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setVehicleClusterMapping(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject268;

// ConsoleClass269 aiSettings.setVehicleMaterialCost: method, 3..3 arguments, no result.
// ELF isObjectActive 0x427B00, executeObjectMethod 0x4282E0,
// setArgFromString 0x427C20, getReturnValueAsString 0x427AC0, D1 0x427AA0, D0 0x427A60.
// argc 3: dice::hfe::ai::AISettings::setVehicleMaterialCost(std::string const&, unsigned char, float) (slot +0x8C at 0x428356; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass269
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setVehicleMaterialCost", 0, 3, 3, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "materialId";
            m_argTypes[2] = "float";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setVehicleMaterialCost(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(materialId, 1), BF2_CC_ARG(float, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT_WITH(materialId, 2, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT(float, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject269;

// ConsoleClass270 aiSettings.setManDownEquipment: method, 1..1 arguments, no result.
// ELF isObjectActive 0x428430, executeObjectMethod 0x4287D0,
// setArgFromString 0x428550, getReturnValueAsString 0x4283F0, D1 0x4283D0, D0 0x428390.
// argc 1: dice::hfe::ai::AISettings::setManDownEquipment(std::string const&) (slot +0x88 at 0x42882F; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass270
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setManDownEquipment", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setManDownEquipment(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject270;

// ConsoleClass271 aiSettings.getInformationGridDimension: method, 0..0 arguments, returns int.
// ELF isObjectActive 0x428B30, executeObjectMethod 0x428C60,
// setArgFromString 0x428C50, getReturnValueAsString 0x4288C0, D1 0x4288A0, D0 0x428860.
// argc 0: dice::hfe::ai::AISettings::getInformationGridDimension() const (slot +0x3C at 0x428C95; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass271
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getInformationGridDimension", 0, 0, 0, "int", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getInformationGridDimension();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject271;

// ConsoleClass272 aiSettings.getViewDistance: method, 0..0 arguments, returns float.
// ELF isObjectActive 0x428F60, executeObjectMethod 0x429090,
// setArgFromString 0x429080, getReturnValueAsString 0x428D20, D1 0x428D00, D0 0x428CC0.
// argc 0: dice::hfe::ai::AISettings::getViewDistance() const (slot +0x28 at 0x4290C5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass272
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getViewDistance", 0, 0, 0, "float", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getViewDistance();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject272;

// ConsoleClass273 aiSettings.getStatsViewDistance: method, 0..0 arguments, returns float.
// ELF isObjectActive 0x429390, executeObjectMethod 0x4294C0,
// setArgFromString 0x4294B0, getReturnValueAsString 0x429150, D1 0x429130, D0 0x4290F0.
// argc 0: dice::hfe::ai::AISettings::getStatsViewDistance() const (slot +0x2C at 0x4294F5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass273
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getStatsViewDistance", 0, 0, 0, "float", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getStatsViewDistance();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject273;

// ConsoleClass274 aiSettings.getVehicle: method, 1..1 arguments, returns std::string.
// ELF isObjectActive 0x429840, executeObjectMethod 0x429BA0,
// setArgFromString 0x429960, getReturnValueAsString 0x429600, D1 0x429590, D0 0x429520.
// argc 1: dice::hfe::ai::AISettings::getVehicle(int) const (slot +0x50 at 0x429BE3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass274
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getVehicle", 0, 1, 1, "std::string", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject274;

// ConsoleClass275 aiSettings.getVehicleClusterMapping: method, 1..1 arguments, returns std::string.
// ELF isObjectActive 0x429F30, executeObjectMethod 0x42A2D0,
// setArgFromString 0x42A050, getReturnValueAsString 0x429CF0, D1 0x429C80, D0 0x429C10.
// argc 1: dice::hfe::ai::AISettings::getVehicleClusterMapping(std::string const&) const (slot +0x58 at 0x42A326; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass275
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getVehicleClusterMapping", 0, 1, 1, "std::string", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicleClusterMapping(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject275;

// ConsoleClass276 aiSettings.setNSides: method, 1..1 arguments, no result.
// ELF isObjectActive 0x42A410, executeObjectMethod 0x42A770,
// setArgFromString 0x42A530, getReturnValueAsString 0x42A3D0, D1 0x42A3B0, D0 0x42A370.
// argc 1: dice::hfe::ai::AISettings::setNSides(int) (slot +0x44 at 0x42A7B3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass276
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setNSides", 0, 1, 1, "void", false)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setNSides(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject276;

// ConsoleClass277 aiSettings.getNSides: method, 0..0 arguments, returns int.
// ELF isObjectActive 0x42AA90, executeObjectMethod 0x42ABC0,
// setArgFromString 0x42ABB0, getReturnValueAsString 0x42A820, D1 0x42A800, D0 0x42A7C0.
// argc 0: dice::hfe::ai::AISettings::getNSides() const (slot +0x40 at 0x42ABF5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass277
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getNSides", 0, 0, 0, "int", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNSides();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject277;

// ConsoleClass278 aiSettings.setBotSkill: method, 1..1 arguments, no result.
// ELF isObjectActive 0x42ACC0, executeObjectMethod 0x42B020,
// setArgFromString 0x42ADE0, getReturnValueAsString 0x42AC80, D1 0x42AC60, D0 0x42AC20.
// argc 1: dice::hfe::ai::AISettings::setBotSkill(float) (slot +0x48 at 0x42B063; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass278
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setBotSkill", 0, 1, 1, "void", false)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setBotSkill(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject278;

// ConsoleClass279 aiSettings.getBotSkill: method, 0..0 arguments, returns float.
// ELF isObjectActive 0x42B310, executeObjectMethod 0x42B440,
// setArgFromString 0x42B430, getReturnValueAsString 0x42B0D0, D1 0x42B0B0, D0 0x42B070.
// argc 0: dice::hfe::ai::AISettings::getBotSkill() const (slot +0x44 at 0x42B475; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass279
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getBotSkill", 0, 0, 0, "float", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getBotSkill();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject279;

// ConsoleClass280 aiSettings.getNumberOfVehicleGroups: method, 0..0 arguments, returns int.
// ELF isObjectActive 0x42B770, executeObjectMethod 0x42B8A0,
// setArgFromString 0x42B890, getReturnValueAsString 0x42B500, D1 0x42B4E0, D0 0x42B4A0.
// argc 0: dice::hfe::ai::AISettings::getNumberOfVehicleGroups() const (slot +0x64 at 0x42B8D5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass280
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getNumberOfVehicleGroups", 0, 0, 0, "int", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNumberOfVehicleGroups();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject280;

// ConsoleClass281 aiSettings.getVehicleGroup: method, 1..1 arguments, returns int.
// ELF isObjectActive 0x42BBD0, executeObjectMethod 0x42BF30,
// setArgFromString 0x42BCF0, getReturnValueAsString 0x42B960, D1 0x42B940, D0 0x42B900.
// argc 1: dice::hfe::ai::AISettings::getVehicleGroup(int) const (slot +0x68 at 0x42BF6D; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass281
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getVehicleGroup", 0, 1, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicleGroup(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject281;

// ConsoleClass282 aiSettings.getVehicleGroupName: method, 1..1 arguments, returns std::string.
// ELF isObjectActive 0x42C2B0, executeObjectMethod 0x42C610,
// setArgFromString 0x42C3D0, getReturnValueAsString 0x42C070, D1 0x42C000, D0 0x42BF90.
// argc 1: dice::hfe::ai::AISettings::getVehicleGroupName(int) const (slot +0x70 at 0x42C653; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass282
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getVehicleGroupName", 0, 1, 1, "std::string", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicleGroupName(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject282;

// ConsoleClass283 aiSettings.createVehicleGroup: method, 1..1 arguments, no result.
// ELF isObjectActive 0x42C720, executeObjectMethod 0x42CAC0,
// setArgFromString 0x42C840, getReturnValueAsString 0x42C6E0, D1 0x42C6C0, D0 0x42C680.
// argc 1: dice::hfe::ai::AISettings::createVehicleGroup(std::string const&) (slot +0x64 at 0x42CB1F; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass283
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createVehicleGroup", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->createVehicleGroup(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject283;

// ConsoleClass284 aiSettings.addVehicleToVehicleGroup: method, 2..2 arguments, no result.
// ELF isObjectActive 0x42CBF0, executeObjectMethod 0x42D200,
// setArgFromString 0x42CD10, getReturnValueAsString 0x42CBB0, D1 0x42CB90, D0 0x42CB50.
// argc 2: dice::hfe::ai::AISettings::addVehicleToVehicleGroup(std::string const&, std::string const&) (slot +0x6C at 0x42D280; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass284
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addVehicleToVehicleGroup", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->addVehicleToVehicleGroup(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject284;

// ConsoleClass285 aiSettings.getSAIUpdateFrequency: method, 0..0 arguments, returns float.
// ELF isObjectActive 0x42D570, executeObjectMethod 0x42D6A0,
// setArgFromString 0x42D690, getReturnValueAsString 0x42D330, D1 0x42D310, D0 0x42D2D0.
// argc 0: dice::hfe::ai::AISettings::getSAIUpdateFrequency() const (slot +0x48 at 0x42D6D5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass285
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getSAIUpdateFrequency", 0, 0, 0, "float", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getSAIUpdateFrequency();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject285;

// ConsoleClass286 aiSettings.setSAIUpdateFrequency: method, 1..1 arguments, no result.
// ELF isObjectActive 0x42D7A0, executeObjectMethod 0x42DB00,
// setArgFromString 0x42D8C0, getReturnValueAsString 0x42D760, D1 0x42D740, D0 0x42D700.
// argc 1: dice::hfe::ai::AISettings::setSAIUpdateFrequency(float) (slot +0x4C at 0x42DB43; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass286
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setSAIUpdateFrequency", 0, 1, 1, "void", false)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setSAIUpdateFrequency(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject286;

// ConsoleClass287 aiSettings.setRespawnAllowed: method, 1..1 arguments, no result.
// ELF isObjectActive 0x42DBF0, executeObjectMethod 0x42DF50,
// setArgFromString 0x42DD10, getReturnValueAsString 0x42DBB0, D1 0x42DB90, D0 0x42DB50.
// argc 1: dice::hfe::ai::AISettings::setRespawnAllowed(bool) (slot +0x58 at 0x42DF9B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass287
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setRespawnAllowed", 0, 1, 1, "void", false)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setRespawnAllowed(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject287;

// ConsoleClass288 aiSettings.addBotName: method, 2..2 arguments, no result.
// ELF isObjectActive 0x42E040, executeObjectMethod 0x42E650,
// setArgFromString 0x42E160, getReturnValueAsString 0x42E000, D1 0x42DFE0, D0 0x42DFA0.
// argc 2: dice::hfe::ai::AISettings::addBotName(std::string const&, std::string const&) (slot +0x90 at 0x42E6D0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass288
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addBotName", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->addBotName(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject288;

// ConsoleClass289 aiSettings.isRespawnAllowed: method, 0..0 arguments, returns bool.
// ELF isObjectActive 0x42E9C0, executeObjectMethod 0x42EAF0,
// setArgFromString 0x42EAE0, getReturnValueAsString 0x42E780, D1 0x42E760, D0 0x42E720.
// argc 0: dice::hfe::ai::AISettings::isRespawnAllowed() const (slot +0x4C at 0x42EB25; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass289
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("isRespawnAllowed", 0, 0, 0, "bool", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->isRespawnAllowed();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject289;

// ConsoleClass290 aiSettings.overrideMenuSettings: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x42EDF0, executeObjectMethod 0x42F160,
// setArgFromString 0x42EF20, getReturnValueAsString 0x42EBB0, D1 0x42EB90, D0 0x42EB50.
// argc 0: dice::hfe::ai::AISettings::getOverrideMenuSettings() const (slot +0xBC at 0x42F1D1; ledger done)
// argc 1: dice::hfe::ai::AISettings::setOverrideMenuSettings(bool) (slot +0x15C at 0x42F1B2; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass290
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("overrideMenuSettings", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getAISettings() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getAISettings()->getOverrideMenuSettings();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<AISettings*>(getAISettings())->setOverrideMenuSettings(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject290;

// ConsoleClass293 aiSettings.lodEnable: method, 0..1 arguments, returns bool.
// ELF isObjectActive 0x42F490, executeObjectMethod 0x42F7F0,
// setArgFromString 0x42F5B0, getReturnValueAsString 0x42F250, D1 0x42F230, D0 0x42F1F0.
// argc 0: dice::hfe::ai::AILODManager::lodEnable() (call at 0x42F862; ledger done)
// argc 1: dice::hfe::ai::AILODManager::lodEnable(bool) (call at 0x42F835; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass293
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("lodEnable", 0, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AILODManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AILODManager::m_instance->lodEnable();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AILODManager::m_instance->lodEnable(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject293;

// ConsoleClass294 aiSettings.setBasicLODDistance: method, 1..1 arguments, no result.
// ELF isObjectActive 0x42F910, executeObjectMethod 0x42FC70,
// setArgFromString 0x42FA30, getReturnValueAsString 0x42F8D0, D1 0x42F8B0, D0 0x42F870.
// argc 1: dice::hfe::ai::AILODManager::setBasicLODDistance(float) (call at 0x42FCB1; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass294
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setBasicLODDistance", 0, 1, 1, "void", false)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AILODManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AILODManager::m_instance->setBasicLODDistance(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject294;

// ConsoleClass296 aiSettings.setVehicleMods: method, 2..2 arguments, no result.
// ELF isObjectActive 0x42FD60, executeObjectMethod 0x430370,
// setArgFromString 0x42FE80, getReturnValueAsString 0x42FD20, D1 0x42FD00, D0 0x42FCC0.
// argc 2: dice::hfe::ai::AIConsole::setVehicleMods(std::string const&, std::string const&) (call at 0x4303EE; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass296
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setVehicleMods", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setVehicleMods(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject296;

// ConsoleClass297 aiSettings.setStandardPersonality: method, 2..2 arguments, no result.
// ELF isObjectActive 0x4304E0, executeObjectMethod 0x430AB0,
// setArgFromString 0x430600, getReturnValueAsString 0x4304A0, D1 0x430480, D0 0x430440.
// argc 2: dice::hfe::ai::AIConsole::setStandardPersonality(std::string const&, int) (call at 0x430B16; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass297
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setStandardPersonality", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "int";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setStandardPersonality(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(int, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(int, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject297;

// ConsoleClass298 aiSettings.setVehicleBehaviour: method, 7..7 arguments, no result.
// ELF isObjectActive 0x430BE0, executeObjectMethod 0x431D90,
// setArgFromString 0x430D00, getReturnValueAsString 0x430BA0, D1 0x430B80, D0 0x430B40.
// argc 7: dice::hfe::ai::AIConsole::setVehicleBehaviour(std::string, std::string, std::string, std::string, int, std::string, std::string) (call at 0x431F5A; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass298
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setVehicleBehaviour", 0, 7, 7, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
            m_argTypes[2] = "std::string";
            m_argTypes[3] = "std::string";
            m_argTypes[4] = "int";
            m_argTypes[5] = "std::string";
            m_argTypes[6] = "std::string";
    AIMODULE_CC_TAIL(7)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 7)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setVehicleBehaviour(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1), BF2_CC_ARG(std::string, 2), BF2_CC_ARG(std::string, 3), BF2_CC_ARG(int, 4), BF2_CC_ARG(std::string, 5), BF2_CC_ARG(std::string, 6));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
        AIMODULE_CC_EXTRACT(std::string, 3)
        AIMODULE_CC_EXTRACT(std::string, 4)
        AIMODULE_CC_EXTRACT(int, 5)
        AIMODULE_CC_EXTRACT(std::string, 6)
        AIMODULE_CC_EXTRACT(std::string, 7)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject298;

// ConsoleClass299 aiSettings.setVehicleDefaultBehaviour: method, 2..2 arguments, no result.
// ELF isObjectActive 0x4321D0, executeObjectMethod 0x4327E0,
// setArgFromString 0x4322F0, getReturnValueAsString 0x432190, D1 0x432170, D0 0x432130.
// argc 2: dice::hfe::ai::AIConsole::setVehicleDefaultBehaviour(std::string&, std::string&) (call at 0x43285E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass299
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setVehicleDefaultBehaviour", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setVehicleDefaultBehaviour(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject299;

// ConsoleClass300 aiSettings.addInterpreterEntry: method, 2..3 arguments, no result.
// ELF isObjectActive 0x432950, executeObjectMethod 0x433180,
// setArgFromString 0x432A70, getReturnValueAsString 0x432910, D1 0x4328F0, D0 0x4328B0.
// argc 2: dice::hfe::ai::AIConsole::addInterpreterEntry(std::string, std::string) (call at 0x43333D; ledger done)
// argc 3: dice::hfe::ai::AIConsole::addInterpreterEntry(std::string, std::string, dice::hfe::ai::BotLodLevel) (call at 0x43323F; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass300
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addInterpreterEntry", 0, 2, 3, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
            m_argTypes[2] = "BotLodLevel";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->addInterpreterEntry(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
        else if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->addInterpreterEntry(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1), BF2_CC_ARG(BotLodLevel, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
        AIMODULE_CC_EXTRACT_WITH(BotLodLevel, 3, dice::hfe::ai::operator>>)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject300;

// ConsoleClass301 aiSettings.removeInterpreterEntry: method, 3..3 arguments, no result.
// ELF isObjectActive 0x433460, executeObjectMethod 0x433C90,
// setArgFromString 0x433580, getReturnValueAsString 0x433420, D1 0x433400, D0 0x4333C0.
// argc 3: dice::hfe::ai::AIConsole::removeInterpreterEntry(std::string, std::string, dice::hfe::ai::BotLodLevel) (call at 0x433D3E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass301
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("removeInterpreterEntry", 0, 3, 3, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
            m_argTypes[2] = "BotLodLevel";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->removeInterpreterEntry(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1), BF2_CC_ARG(BotLodLevel, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
        AIMODULE_CC_EXTRACT_WITH(BotLodLevel, 3, dice::hfe::ai::operator>>)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject301;

// ConsoleClass302 aiSettings.activateDefaultInterpreter: method, 3..3 arguments, no result.
// ELF isObjectActive 0x433E70, executeObjectMethod 0x434650,
// setArgFromString 0x433F90, getReturnValueAsString 0x433E30, D1 0x433E10, D0 0x433DD0.
// argc 3: dice::hfe::ai::AIConsole::activateDefaultInterpreter(std::string, dice::hfe::ai::BAPStatementType, dice::hfe::ai::BotLodLevel) (call at 0x4346DF; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass302
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("activateDefaultInterpreter", 0, 3, 3, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "BAPStatementType";
            m_argTypes[2] = "BotLodLevel";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->activateDefaultInterpreter(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(BAPStatementType, 1), BF2_CC_ARG(BotLodLevel, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT_WITH(BAPStatementType, 2, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(BotLodLevel, 3, dice::hfe::ai::operator>>)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject302;

// ConsoleClass303 aiSettings.setBasicBotWeights: method, 1..1 arguments, no result.
// ELF isObjectActive 0x4347D0, executeObjectMethod 0x434B70,
// setArgFromString 0x4348F0, getReturnValueAsString 0x434790, D1 0x434770, D0 0x434730.
// argc 1: dice::hfe::ai::AIConsole::setBasicBotWeights(std::string) (call at 0x434BE1; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass303
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setBasicBotWeights", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setBasicBotWeights(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject303;

// ConsoleClass304 aiSettings.setBehaviour: method, 2..2 arguments, no result.
// ELF isObjectActive 0x434CD0, executeObjectMethod 0x4352C0,
// setArgFromString 0x434DF0, getReturnValueAsString 0x434C90, D1 0x434C70, D0 0x434C30.
// argc 2: dice::hfe::ai::AIConsole::setBehaviour(int, std::string) (call at 0x435340; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass304
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setBehaviour", 0, 2, 2, "void", false)
            m_argTypes[0] = "int";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setBehaviour(BF2_CC_ARG(int, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject304;

// ConsoleClass305 aiSettings.getBehaviour: method, 1..1 arguments, returns std::string.
// ELF isObjectActive 0x4356B0, executeObjectMethod 0x435A10,
// setArgFromString 0x4357D0, getReturnValueAsString 0x435470, D1 0x435400, D0 0x435390.
// argc 1: dice::hfe::ai::AIConsole::getBehaviour(int) (call at 0x435A5B; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass305
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getBehaviour", 0, 1, 1, "std::string", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->getBehaviour(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject305;

// ConsoleClass306 aiSettings.createBehaviourModifiers: method, 1..1 arguments, no result.
// ELF isObjectActive 0x435B50, executeObjectMethod 0x435EF0,
// setArgFromString 0x435C70, getReturnValueAsString 0x435B10, D1 0x435AF0, D0 0x435AB0.
// argc 1: dice::hfe::ai::AIConsole::createBehaviourModifiers(std::string) (call at 0x435F61; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass306
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createBehaviourModifiers", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->createBehaviourModifiers(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject306;

// ConsoleClass307 aiSettings.activeBehaviourModifiers: property, 0..1 arguments, returns std::string.
// ELF isObjectActive 0x4362D0, executeObjectMethod 0x436670,
// setArgFromString 0x4363F0, getReturnValueAsString 0x436090, D1 0x436020, D0 0x435FB0.
// argc 0: dice::hfe::ai::AIConsole::getActiveBehaviourModifiers() (call at 0x436755; ledger stop)
// argc 1: dice::hfe::ai::AIConsole::setActiveBehaviourModifiers(std::string) (call at 0x4366EF; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass307
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("activeBehaviourModifiers", 1, 0, 1, "std::string", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->getActiveBehaviourModifiers();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setActiveBehaviourModifiers(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject307;

// ConsoleClass308 aiSettings.setBehaviourModifier: method, 2..2 arguments, no result.
// ELF isObjectActive 0x436840, executeObjectMethod 0x436E10,
// setArgFromString 0x436960, getReturnValueAsString 0x436800, D1 0x4367E0, D0 0x4367A0.
// argc 2: dice::hfe::ai::AIConsole::setBehaviourModifier(std::string, float) (call at 0x436E90; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass308
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setBehaviourModifier", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "float";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setBehaviourModifier(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(float, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(float, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject308;

// ConsoleClass309 aiSettings.getBehaviourModifier: method, 1..1 arguments, returns float.
// ELF isObjectActive 0x437180, executeObjectMethod 0x437520,
// setArgFromString 0x4372A0, getReturnValueAsString 0x436F40, D1 0x436F20, D0 0x436EE0.
// argc 1: dice::hfe::ai::AIConsole::getBehaviourModifier(std::string) (call at 0x437583; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass309
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getBehaviourModifier", 0, 1, 1, "float", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->getBehaviourModifier(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject309;

// ConsoleClass310 aiSettings.listBehaviourModifiers: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x437920, executeObjectMethod 0x437A50,
// setArgFromString 0x437A40, getReturnValueAsString 0x4376E0, D1 0x437670, D0 0x437600.
// argc 0: dice::hfe::ai::AIConsole::listBehaviourModifiers() (call at 0x437A93; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass310
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("listBehaviourModifiers", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->listBehaviourModifiers();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject310;

// ConsoleClass311 aiSettings.showAISettings: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x437E10, executeObjectMethod 0x437F40,
// setArgFromString 0x437F30, getReturnValueAsString 0x437BD0, D1 0x437B60, D0 0x437AF0.
// argc 0: dice::hfe::ai::AIConsole::showAISettings() (call at 0x437F83; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass311
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showAISettings", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->showAISettings();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject311;

// ConsoleClass312 aiSettings.createUCConstant: method, 2..2 arguments, no result.
// ELF isObjectActive 0x438080, executeObjectMethod 0x438650,
// setArgFromString 0x4381A0, getReturnValueAsString 0x438040, D1 0x438020, D0 0x437FE0.
// argc 2: dice::hfe::ai::BotBehaviourManager::createUCConstant(std::string&, float) (call at 0x4386C2; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass312
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createUCConstant", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "float";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return BotBehaviourManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            BotBehaviourManager::m_instance->createUCConstant(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(float, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(float, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject312;

// ConsoleClass313 aiSettings.createUCLinear: method, 3..3 arguments, no result.
// ELF isObjectActive 0x4387B0, executeObjectMethod 0x438F90,
// setArgFromString 0x4388D0, getReturnValueAsString 0x438770, D1 0x438750, D0 0x438710.
// argc 3: dice::hfe::ai::BotBehaviourManager::createUCLinear(std::string&, float, float) (call at 0x439011; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass313
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createUCLinear", 0, 3, 3, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "float";
            m_argTypes[2] = "float";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return BotBehaviourManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            BotBehaviourManager::m_instance->createUCLinear(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(float, 1), BF2_CC_ARG(float, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(float, 2)
        AIMODULE_CC_EXTRACT(float, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject313;

// ConsoleClass314 aiSettings.createUCXInverse: method, 5..5 arguments, no result.
// ELF isObjectActive 0x439100, executeObjectMethod 0x439D00,
// setArgFromString 0x439220, getReturnValueAsString 0x4390C0, D1 0x4390A0, D0 0x439060.
// argc 5: dice::hfe::ai::BotBehaviourManager::createUCXInverse(std::string&, float, float, float, float) (call at 0x439D93; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass314
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createUCXInverse", 0, 5, 5, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "float";
            m_argTypes[2] = "float";
            m_argTypes[3] = "float";
            m_argTypes[4] = "float";
    AIMODULE_CC_TAIL(5)
    virtual bool isObjectActive() const { return BotBehaviourManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 5)
        {
            m_hasReturnValue = false;
            BotBehaviourManager::m_instance->createUCXInverse(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(float, 1), BF2_CC_ARG(float, 2), BF2_CC_ARG(float, 3), BF2_CC_ARG(float, 4));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(float, 2)
        AIMODULE_CC_EXTRACT(float, 3)
        AIMODULE_CC_EXTRACT(float, 4)
        AIMODULE_CC_EXTRACT(float, 5)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject314;

// ConsoleClass315 aiSettings.createLODManager: method, 2..2 arguments, std::string result discarded.
// ELF isObjectActive 0x439E80, executeObjectMethod 0x43A420,
// setArgFromString 0x439FA0, getReturnValueAsString 0x439E40, D1 0x439E20, D0 0x439DE0.
// argc 2: dice::hfe::ai::AIConsole::createLODManager(float, float) (call at 0x43A47E; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass315
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createLODManager", 0, 2, 2, "std::string", false)
            m_argTypes[0] = "float";
            m_argTypes[1] = "float";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->createLODManager(BF2_CC_ARG(float, 0), BF2_CC_ARG(float, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
        AIMODULE_CC_EXTRACT(float, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject315;

// ConsoleClass316 aiSettings.setSensingAgentPatternName: method, 1..1 arguments, no result.
// ELF isObjectActive 0x43A550, executeObjectMethod 0x43A8F0,
// setArgFromString 0x43A670, getReturnValueAsString 0x43A510, D1 0x43A4F0, D0 0x43A4B0.
// argc 1: dice::hfe::ai::AIConsole::setSensingAgentPatternName(std::string const&) (call at 0x43A94D; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass316
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setSensingAgentPatternName", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setSensingAgentPatternName(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject316;

// ConsoleClass317 aiSettings.setSensingAgentNullPattern: method, 1..1 arguments, no result.
// ELF isObjectActive 0x43AA20, executeObjectMethod 0x43ADC0,
// setArgFromString 0x43AB40, getReturnValueAsString 0x43A9E0, D1 0x43A9C0, D0 0x43A980.
// argc 1: dice::hfe::ai::AIConsole::setSensingAgentNullPattern(std::string const&) (call at 0x43AE1D; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass317
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setSensingAgentNullPattern", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setSensingAgentNullPattern(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject317;

// ConsoleClass318 aiSettings.addSensingAgentPattern: method, 2..2 arguments, no result.
// ELF isObjectActive 0x43AEF0, executeObjectMethod 0x43B500,
// setArgFromString 0x43B010, getReturnValueAsString 0x43AEB0, D1 0x43AE90, D0 0x43AE50.
// argc 2: dice::hfe::ai::AIConsole::addSensingAgentPattern(std::string const&, std::string const&) (call at 0x43B57E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass318
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addSensingAgentPattern", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->addSensingAgentPattern(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject318;

// ConsoleClass319 aiSettings.sensingAgentDebug: method, 0..0 arguments, no result.
// ELF isObjectActive 0x43B670, executeObjectMethod 0x43B7A0,
// setArgFromString 0x43B790, getReturnValueAsString 0x43B630, D1 0x43B610, D0 0x43B5D0.
// argc 0: dice::hfe::ai::AIConsole::sensingAgentDebug() (call at 0x43B7D9; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass319
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("sensingAgentDebug", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->sensingAgentDebug();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject319;

// ConsoleClass321 aiSettings.autoSpawnBots: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x43BA80, executeObjectMethod 0x43BDE0,
// setArgFromString 0x43BBA0, getReturnValueAsString 0x43B840, D1 0x43B820, D0 0x43B7E0.
// argc 0: dice::hfe::ai::AISettings::getAutoSpawnBots() const (slot +0x90 at 0x43BE54; ledger done)
// argc 1: dice::hfe::ai::AISettings::setAutoSpawnBots(bool) (slot +0x148 at 0x43BE35; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass321
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("autoSpawnBots", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getAutoSpawnBots();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<AISettings*>(static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings))->setAutoSpawnBots(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject321;

SetConsoleClassDefaults AIBotManagerfullAccessSetConsoleClassDefaults("AIBotManager", 2, 0);

// ConsoleClass327 AIBotManager.setLodLevelTicks: method, 3..3 arguments, no result.
// ELF isObjectActive 0x43BF10, executeObjectMethod 0x43C6E0,
// setArgFromString 0x43C030, getReturnValueAsString 0x43BED0, D1 0x43BEB0, D0 0x43BE70.
// argc 3: dice::hfe::ai::BotManager::setLodLevelTicks(unsigned int, unsigned int, unsigned int) (slot +0x8 at 0x43C741; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass327
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setLodLevelTicks", 0, 3, 3, "void", false)
            m_argTypes[0] = "hfe::uint";
            m_argTypes[1] = "hfe::uint";
            m_argTypes[2] = "hfe::uint";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            IBotManagerModifier::instance->setLodLevelTicks(BF2_CC_ARG(hfe::uint, 0), BF2_CC_ARG(hfe::uint, 1), BF2_CC_ARG(hfe::uint, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(hfe::uint, 1)
        AIMODULE_CC_EXTRACT(hfe::uint, 2)
        AIMODULE_CC_EXTRACT(hfe::uint, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject327;

// ConsoleClass328 AIBotManager.setLodLevelPriority: method, 3..3 arguments, no result.
// ELF isObjectActive 0x43C7F0, executeObjectMethod 0x43CFC0,
// setArgFromString 0x43C910, getReturnValueAsString 0x43C7B0, D1 0x43C790, D0 0x43C750.
// argc 3: dice::hfe::ai::BotManager::setLodLevelPriority(unsigned int, unsigned int, unsigned int) (slot +0xC at 0x43D021; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass328
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setLodLevelPriority", 0, 3, 3, "void", false)
            m_argTypes[0] = "hfe::uint";
            m_argTypes[1] = "hfe::uint";
            m_argTypes[2] = "hfe::uint";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            IBotManagerModifier::instance->setLodLevelPriority(BF2_CC_ARG(hfe::uint, 0), BF2_CC_ARG(hfe::uint, 1), BF2_CC_ARG(hfe::uint, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(hfe::uint, 1)
        AIMODULE_CC_EXTRACT(hfe::uint, 2)
        AIMODULE_CC_EXTRACT(hfe::uint, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject328;

// ConsoleClass329 AIBotManager.setPlannedDecisionMakingThreshold: method, 3..3 arguments, no result.
// ELF isObjectActive 0x43D0D0, executeObjectMethod 0x43D8A0,
// setArgFromString 0x43D1F0, getReturnValueAsString 0x43D090, D1 0x43D070, D0 0x43D030.
// argc 3: dice::hfe::ai::BotManager::setPlannedDecisionMakingThreshold(double const&, double const&, double const&) (slot +0x10 at 0x43D907; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass329
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setPlannedDecisionMakingThreshold", 0, 3, 3, "void", false)
            m_argTypes[0] = "double";
            m_argTypes[1] = "double";
            m_argTypes[2] = "double";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            IBotManagerModifier::instance->setPlannedDecisionMakingThreshold(BF2_CC_ARG(double, 0), BF2_CC_ARG(double, 1), BF2_CC_ARG(double, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(double, 1)
        AIMODULE_CC_EXTRACT(double, 2)
        AIMODULE_CC_EXTRACT(double, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject329;

// ConsoleClass330 AIBotManager.setUnplannedDecisionMakingThreshold: method, 3..3 arguments, no result.
// ELF isObjectActive 0x43D9B0, executeObjectMethod 0x43E180,
// setArgFromString 0x43DAD0, getReturnValueAsString 0x43D970, D1 0x43D950, D0 0x43D910.
// argc 3: dice::hfe::ai::BotManager::setUnplannedDecisionMakingThreshold(double const&, double const&, double const&) (slot +0x14 at 0x43E1E7; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass330
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setUnplannedDecisionMakingThreshold", 0, 3, 3, "void", false)
            m_argTypes[0] = "double";
            m_argTypes[1] = "double";
            m_argTypes[2] = "double";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            IBotManagerModifier::instance->setUnplannedDecisionMakingThreshold(BF2_CC_ARG(double, 0), BF2_CC_ARG(double, 1), BF2_CC_ARG(double, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(double, 1)
        AIMODULE_CC_EXTRACT(double, 2)
        AIMODULE_CC_EXTRACT(double, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject330;

// ConsoleClass331 AIBotManager.setDecisionMakingInterleave: method, 2..2 arguments, no result.
// ELF isObjectActive 0x43E290, executeObjectMethod 0x43E830,
// setArgFromString 0x43E3B0, getReturnValueAsString 0x43E250, D1 0x43E230, D0 0x43E1F0.
// argc 2: dice::hfe::ai::BotManager::setDecisionMakingInterleave(unsigned int, unsigned int) (slot +0x18 at 0x43E886; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass331
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setDecisionMakingInterleave", 0, 2, 2, "void", false)
            m_argTypes[0] = "hfe::uint";
            m_argTypes[1] = "hfe::uint";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            IBotManagerModifier::instance->setDecisionMakingInterleave(BF2_CC_ARG(hfe::uint, 0), BF2_CC_ARG(hfe::uint, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(hfe::uint, 1)
        AIMODULE_CC_EXTRACT(hfe::uint, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject331;

// ConsoleClass332 AIBotManager.setSensingQuotient: method, 2..2 arguments, no result.
// ELF isObjectActive 0x43E930, executeObjectMethod 0x43EED0,
// setArgFromString 0x43EA50, getReturnValueAsString 0x43E8F0, D1 0x43E8D0, D0 0x43E890.
// argc 2: dice::hfe::ai::BotManager::setSensingQuotient(unsigned int, unsigned int) (slot +0x1C at 0x43EF26; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass332
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setSensingQuotient", 0, 2, 2, "void", false)
            m_argTypes[0] = "hfe::uint";
            m_argTypes[1] = "hfe::uint";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            IBotManagerModifier::instance->setSensingQuotient(BF2_CC_ARG(hfe::uint, 0), BF2_CC_ARG(hfe::uint, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(hfe::uint, 1)
        AIMODULE_CC_EXTRACT(hfe::uint, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject332;

// ConsoleClass333 AIBotManager.setSystemQuotient: method, 3..3 arguments, no result.
// ELF isObjectActive 0x43EFD0, executeObjectMethod 0x43F7A0,
// setArgFromString 0x43F0F0, getReturnValueAsString 0x43EF90, D1 0x43EF70, D0 0x43EF30.
// argc 3: dice::hfe::ai::BotManager::setSystemQuotient(unsigned int, unsigned int, unsigned int) (slot +0x20 at 0x43F801; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass333
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setSystemQuotient", 0, 3, 3, "void", false)
            m_argTypes[0] = "hfe::uint";
            m_argTypes[1] = "hfe::uint";
            m_argTypes[2] = "hfe::uint";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            IBotManagerModifier::instance->setSystemQuotient(BF2_CC_ARG(hfe::uint, 0), BF2_CC_ARG(hfe::uint, 1), BF2_CC_ARG(hfe::uint, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(hfe::uint, 1)
        AIMODULE_CC_EXTRACT(hfe::uint, 2)
        AIMODULE_CC_EXTRACT(hfe::uint, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject333;

// ConsoleClass334 AIBotManager.getLodLevelFullTicks: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x43FAB0, executeObjectMethod 0x43FBE0,
// setArgFromString 0x43FBD0, getReturnValueAsString 0x43F870, D1 0x43F850, D0 0x43F810.
// argc 0: dice::hfe::ai::BotManager::getLodLevelFullTicks() const (slot +0x24 at 0x43FC15; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass334
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getLodLevelFullTicks", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getLodLevelFullTicks();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject334;

// ConsoleClass335 AIBotManager.getLodLevelMediumTicks: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x43FEE0, executeObjectMethod 0x440010,
// setArgFromString 0x440000, getReturnValueAsString 0x43FCA0, D1 0x43FC80, D0 0x43FC40.
// argc 0: dice::hfe::ai::BotManager::getLodLevelMediumTicks() const (slot +0x28 at 0x440045; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass335
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getLodLevelMediumTicks", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getLodLevelMediumTicks();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject335;

// ConsoleClass336 AIBotManager.getLodLevelLightTicks: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x440310, executeObjectMethod 0x440440,
// setArgFromString 0x440430, getReturnValueAsString 0x4400D0, D1 0x4400B0, D0 0x440070.
// argc 0: dice::hfe::ai::BotManager::getLodLevelLightTicks() const (slot +0x2C at 0x440475; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass336
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getLodLevelLightTicks", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getLodLevelLightTicks();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject336;

// ConsoleClass337 AIBotManager.getLodLevelFullPriority: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x440740, executeObjectMethod 0x440870,
// setArgFromString 0x440860, getReturnValueAsString 0x440500, D1 0x4404E0, D0 0x4404A0.
// argc 0: dice::hfe::ai::BotManager::getLodLevelFullPriority() const (slot +0x30 at 0x4408A5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass337
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getLodLevelFullPriority", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getLodLevelFullPriority();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject337;

// ConsoleClass338 AIBotManager.getLodLevelMediumPriority: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x440B70, executeObjectMethod 0x440CA0,
// setArgFromString 0x440C90, getReturnValueAsString 0x440930, D1 0x440910, D0 0x4408D0.
// argc 0: dice::hfe::ai::BotManager::getLodLevelMediumPriority() const (slot +0x34 at 0x440CD5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass338
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getLodLevelMediumPriority", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getLodLevelMediumPriority();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject338;

// ConsoleClass339 AIBotManager.getLodLevelLightPriority: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x440FA0, executeObjectMethod 0x4410D0,
// setArgFromString 0x4410C0, getReturnValueAsString 0x440D60, D1 0x440D40, D0 0x440D00.
// argc 0: dice::hfe::ai::BotManager::getLodLevelLightPriority() const (slot +0x38 at 0x441105; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass339
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getLodLevelLightPriority", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getLodLevelLightPriority();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject339;

// ConsoleClass340 AIBotManager.getPlannedDecisionMakingThresholdFull: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x4413D0, executeObjectMethod 0x441500,
// setArgFromString 0x4414F0, getReturnValueAsString 0x441190, D1 0x441170, D0 0x441130.
// argc 0: dice::hfe::ai::BotManager::getPlannedDecisionMakingThresholdFull() const (slot +0x3C at 0x441535; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass340
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getPlannedDecisionMakingThresholdFull", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getPlannedDecisionMakingThresholdFull();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject340;

// ConsoleClass341 AIBotManager.getPlannedDecisionMakingThresholdMedium: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x441800, executeObjectMethod 0x441930,
// setArgFromString 0x441920, getReturnValueAsString 0x4415C0, D1 0x4415A0, D0 0x441560.
// argc 0: dice::hfe::ai::BotManager::getPlannedDecisionMakingThresholdMedium() const (slot +0x40 at 0x441965; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass341
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getPlannedDecisionMakingThresholdMedium", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getPlannedDecisionMakingThresholdMedium();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject341;

// ConsoleClass342 AIBotManager.getPlannedDecisionMakingThresholdLight: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x441C30, executeObjectMethod 0x441D60,
// setArgFromString 0x441D50, getReturnValueAsString 0x4419F0, D1 0x4419D0, D0 0x441990.
// argc 0: dice::hfe::ai::BotManager::getPlannedDecisionMakingThresholdLight() const (slot +0x44 at 0x441D95; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass342
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getPlannedDecisionMakingThresholdLight", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getPlannedDecisionMakingThresholdLight();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject342;

// ConsoleClass343 AIBotManager.getUnplannedDecisionMakingThresholdFull: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x442060, executeObjectMethod 0x442190,
// setArgFromString 0x442180, getReturnValueAsString 0x441E20, D1 0x441E00, D0 0x441DC0.
// argc 0: dice::hfe::ai::BotManager::getUnplannedDecisionMakingThresholdFull() const (slot +0x48 at 0x4421C5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass343
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getUnplannedDecisionMakingThresholdFull", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getUnplannedDecisionMakingThresholdFull();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject343;

// ConsoleClass344 AIBotManager.getUnplannedDecisionMakingThresholdMedium: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x442490, executeObjectMethod 0x4425C0,
// setArgFromString 0x4425B0, getReturnValueAsString 0x442250, D1 0x442230, D0 0x4421F0.
// argc 0: dice::hfe::ai::BotManager::getUnplannedDecisionMakingThresholdMedium() const (slot +0x4C at 0x4425F5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass344
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getUnplannedDecisionMakingThresholdMedium", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getUnplannedDecisionMakingThresholdMedium();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject344;

// ConsoleClass345 AIBotManager.getUnplannedDecisionMakingThresholdLight: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x4428C0, executeObjectMethod 0x4429F0,
// setArgFromString 0x4429E0, getReturnValueAsString 0x442680, D1 0x442660, D0 0x442620.
// argc 0: dice::hfe::ai::BotManager::getUnplannedDecisionMakingThresholdLight() const (slot +0x50 at 0x442A25; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass345
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getUnplannedDecisionMakingThresholdLight", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getUnplannedDecisionMakingThresholdLight();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject345;

// ConsoleClass346 AIBotManager.getPlannedDecisionMakingInterleave: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x442CF0, executeObjectMethod 0x442E20,
// setArgFromString 0x442E10, getReturnValueAsString 0x442AB0, D1 0x442A90, D0 0x442A50.
// argc 0: dice::hfe::ai::BotManager::getPlannedDecisionMakingInterleave() const (slot +0x54 at 0x442E55; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass346
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getPlannedDecisionMakingInterleave", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getPlannedDecisionMakingInterleave();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject346;

// ConsoleClass347 AIBotManager.getUnplannedDecisionMakingInterleave: method, 0..0 arguments, returns hfe::uint.
// ELF isObjectActive 0x443120, executeObjectMethod 0x443250,
// setArgFromString 0x443240, getReturnValueAsString 0x442EE0, D1 0x442EC0, D0 0x442E80.
// argc 0: dice::hfe::ai::BotManager::getUnplannedDecisionMakingInterleave() const (slot +0x58 at 0x443285; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass347
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getUnplannedDecisionMakingInterleave", 0, 0, 0, "hfe::uint", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getUnplannedDecisionMakingInterleave();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::uint m_returnValue;
};
BF2_CC_NAME consoleObject347;

// ConsoleClass348 AIBotManager.getSensingQuotient: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x443550, executeObjectMethod 0x443680,
// setArgFromString 0x443670, getReturnValueAsString 0x443310, D1 0x4432F0, D0 0x4432B0.
// argc 0: dice::hfe::ai::BotManager::getSensingQuotient() const (slot +0x5C at 0x4436B5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass348
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getSensingQuotient", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getSensingQuotient();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject348;

// ConsoleClass349 AIBotManager.getMemoryUpdateQuotient: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x443980, executeObjectMethod 0x443AB0,
// setArgFromString 0x443AA0, getReturnValueAsString 0x443740, D1 0x443720, D0 0x4436E0.
// argc 0: dice::hfe::ai::BotManager::getMemoryUpdateQuotient() const (slot +0x60 at 0x443AE5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass349
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getMemoryUpdateQuotient", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getMemoryUpdateQuotient();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject349;

// ConsoleClass350 AIBotManager.getSystemDecisionMakingQuotient: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x443DB0, executeObjectMethod 0x443EE0,
// setArgFromString 0x443ED0, getReturnValueAsString 0x443B70, D1 0x443B50, D0 0x443B10.
// argc 0: dice::hfe::ai::BotManager::getSystemDecisionMakingQuotient() const (slot +0x64 at 0x443F15; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass350
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getSystemDecisionMakingQuotient", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getSystemDecisionMakingQuotient();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject350;

// ConsoleClass351 AIBotManager.getSystemPathfindingQuotient: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x4441E0, executeObjectMethod 0x444310,
// setArgFromString 0x444300, getReturnValueAsString 0x443FA0, D1 0x443F80, D0 0x443F40.
// argc 0: dice::hfe::ai::BotManager::getSystemPathfindingQuotient() const (slot +0x68 at 0x444345; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass351
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getSystemPathfindingQuotient", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getSystemPathfindingQuotient();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject351;

// ConsoleClass352 AIBotManager.getSystemSensingQuotient: method, 0..0 arguments, returns double.
// ELF isObjectActive 0x444610, executeObjectMethod 0x444740,
// setArgFromString 0x444730, getReturnValueAsString 0x4443D0, D1 0x4443B0, D0 0x444370.
// argc 0: dice::hfe::ai::BotManager::getSystemSensingQuotient() const (slot +0x6C at 0x444775; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass352
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getSystemSensingQuotient", 0, 0, 0, "double", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IBotManagerModifier::instance->getSystemSensingQuotient();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    double m_returnValue;
};
BF2_CC_NAME consoleObject352;

// ConsoleClass353 AIBotManager.enableBotScriptDebugger: method, 1..1 arguments, no result.
// ELF isObjectActive 0x444840, executeObjectMethod 0x444BA0,
// setArgFromString 0x444960, getReturnValueAsString 0x444800, D1 0x4447E0, D0 0x4447A0.
// argc 1: dice::hfe::ai::BotManager::enableBotScriptDebugger(bool) (slot +0xA8 at 0x444BEB; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass353
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("enableBotScriptDebugger", 0, 1, 1, "void", false)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IBotManagerModifier::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            IBotManagerModifier::instance->enableBotScriptDebugger(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject353;

SetConsoleClassDefaults aiStrategyfullAccessSetConsoleClassDefaults("aiStrategy", 2, 0);

// ConsoleClass361 aiStrategy.createStrategy: method, 1..1 arguments, no result.
// ELF isObjectActive 0x444CA0, executeObjectMethod 0x445050,
// setArgFromString 0x444DD0, getReturnValueAsString 0x444C60, D1 0x444C40, D0 0x444C00.
// argc 1: dice::hfe::ai::StrategyManager::createStrategy(std::string) (call at 0x4450BE; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass361
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createStrategy", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->createStrategy(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject361;

// ConsoleClass362 aiStrategy.setActiveStrategy: method, 1..1 arguments, no result.
// ELF isObjectActive 0x4451B0, executeObjectMethod 0x445560,
// setArgFromString 0x4452E0, getReturnValueAsString 0x445170, D1 0x445150, D0 0x445110.
// argc 1: dice::hfe::ai::StrategyManager::setActiveStrategy(std::string) (call at 0x4455CE; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass362
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setActiveStrategy", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->setActiveStrategy(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject362;

// ConsoleClass363 aiStrategy.getActiveStrategyName: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x445940, executeObjectMethod 0x445A80,
// setArgFromString 0x445A70, getReturnValueAsString 0x445700, D1 0x445690, D0 0x445620.
// argc 0: dice::hfe::ai::StrategyManager::getActiveStrategyName() (call at 0x445AC0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass363
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getActiveStrategyName", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->getActiveStrategyName();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject363;

// ConsoleClass364 aiStrategy.aggression: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x445DB0, executeObjectMethod 0x446120,
// setArgFromString 0x445EE0, getReturnValueAsString 0x445B70, D1 0x445B50, D0 0x445B10.
// argc 0: dice::hfe::ai::Strategy::getAggression() const (call at 0x446186; ledger done)
// argc 1: dice::hfe::ai::Strategy::setAggression(float) (call at 0x44616E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass364
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("aggression", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategy()->getAggression();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->setAggression(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject364;

// ConsoleClass365 aiStrategy.timeLimit: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x446440, executeObjectMethod 0x4467B0,
// setArgFromString 0x446570, getReturnValueAsString 0x446200, D1 0x4461E0, D0 0x4461A0.
// argc 0: inline accessor, reads the member at +0x20 (m_f20)
// argc 1: inline accessor, stores the member at +0x20 (m_f20)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass365
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("timeLimit", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategy()->m_f20;
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->m_f20 = BF2_CC_ARG(float, 0);
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject365;

// ConsoleClass366 aiStrategy.getPrerequisiteWeight: method, 0..0 arguments, returns float.
// ELF isObjectActive 0x446AC0, executeObjectMethod 0x446C00,
// setArgFromString 0x446BF0, getReturnValueAsString 0x446880, D1 0x446860, D0 0x446820.
// argc 0: dice::hfe::ai::Strategy::getPrerequisiteWeight() const (call at 0x446C30; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass366
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getPrerequisiteWeight", 0, 0, 0, "float", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategy()->getPrerequisiteWeight();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject366;

// ConsoleClass367 aiStrategy.setPrerequisiteWeight: method, 1..1 arguments, no result.
// ELF isObjectActive 0x446D00, executeObjectMethod 0x447070,
// setArgFromString 0x446E30, getReturnValueAsString 0x446CC0, D1 0x446CA0, D0 0x446C60.
// argc 1: dice::hfe::ai::Strategy::setPrerequisiteWeight(float) (call at 0x4470B8; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass367
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setPrerequisiteWeight", 0, 1, 1, "void", false)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->setPrerequisiteWeight(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject367;

// ConsoleClass368 aiStrategy.listStrategies: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x4473F0, executeObjectMethod 0x447530,
// setArgFromString 0x447520, getReturnValueAsString 0x4471B0, D1 0x447140, D0 0x4470D0.
// argc 0: dice::hfe::ai::StrategyManager::listStrategies() (call at 0x447570; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass368
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("listStrategies", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->listStrategies();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject368;

// ConsoleClass369 aiStrategy.setTreeModifier: method, 2..2 arguments, float result discarded.
// ELF isObjectActive 0x447660, executeObjectMethod 0x447C10,
// setArgFromString 0x447790, getReturnValueAsString 0x447620, D1 0x447600, D0 0x4475C0.
// argc 2: dice::hfe::ai::Strategy::setTreeModifier(dice::hfe::ai::TemperatureTreeNode, float) (call at 0x447C63; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass369
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setTreeModifier", 0, 2, 2, "float", false)
            m_argTypes[0] = "TemperatureTreeNode";
            m_argTypes[1] = "float";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->setTreeModifier(BF2_CC_ARG(TemperatureTreeNode, 0), BF2_CC_ARG(float, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(TemperatureTreeNode, 1, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT(float, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject369;

// ConsoleClass370 aiStrategy.getTreeModifier: method, 1..1 arguments, returns float.
// ELF isObjectActive 0x447F10, executeObjectMethod 0x448280,
// setArgFromString 0x448040, getReturnValueAsString 0x447CD0, D1 0x447CB0, D0 0x447C70.
// argc 1: dice::hfe::ai::Strategy::getTreeModifier(dice::hfe::ai::TemperatureTreeNode) const (call at 0x4482BB; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass370
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getTreeModifier", 0, 1, 1, "float", true)
            m_argTypes[0] = "TemperatureTreeNode";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategy()->getTreeModifier(BF2_CC_ARG(TemperatureTreeNode, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(TemperatureTreeNode, 1, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject370;

// ConsoleClass371 aiStrategy.setTreeModifierCombatStrength: method, 3..3 arguments, float result discarded.
// ELF isObjectActive 0x448390, executeObjectMethod 0x448B70,
// setArgFromString 0x4484C0, getReturnValueAsString 0x448350, D1 0x448330, D0 0x4482F0.
// argc 3: dice::hfe::ai::Strategy::setTreeModifierCombatStrength(dice::hfe::ai::TemperatureTreeNode, dice::hfe::ai::CombatStrengthType, float) (call at 0x448BD2; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass371
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setTreeModifierCombatStrength", 0, 3, 3, "float", false)
            m_argTypes[0] = "TemperatureTreeNode";
            m_argTypes[1] = "CombatStrengthType";
            m_argTypes[2] = "float";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->setTreeModifierCombatStrength(BF2_CC_ARG(TemperatureTreeNode, 0), BF2_CC_ARG(CombatStrengthType, 1), BF2_CC_ARG(float, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(TemperatureTreeNode, 1, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(CombatStrengthType, 2, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT(float, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject371;

// ConsoleClass372 aiStrategy.getTreeModifierCombatStrength: method, 2..2 arguments, returns float.
// ELF isObjectActive 0x448E80, executeObjectMethod 0x449430,
// setArgFromString 0x448FB0, getReturnValueAsString 0x448C40, D1 0x448C20, D0 0x448BE0.
// argc 2: dice::hfe::ai::Strategy::getTreeModifierCombatStrength(dice::hfe::ai::TemperatureTreeNode, dice::hfe::ai::CombatStrengthType) const (call at 0x449479; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass372
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getTreeModifierCombatStrength", 0, 2, 2, "float", true)
            m_argTypes[0] = "TemperatureTreeNode";
            m_argTypes[1] = "CombatStrengthType";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategy()->getTreeModifierCombatStrength(BF2_CC_ARG(TemperatureTreeNode, 0), BF2_CC_ARG(CombatStrengthType, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(TemperatureTreeNode, 1, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(CombatStrengthType, 2, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject372;

// ConsoleClass373 aiStrategy.numberOfAttacks: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x449780, executeObjectMethod 0x449AF0,
// setArgFromString 0x4498B0, getReturnValueAsString 0x449510, D1 0x4494F0, D0 0x4494B0.
// argc 0: dice::hfe::ai::Strategy::getNumberOfAttacks() const (call at 0x449B56; ledger done)
// argc 1: dice::hfe::ai::Strategy::setNumberOfAttacks(unsigned int) (call at 0x449B3E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass373
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("numberOfAttacks", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategy()->getNumberOfAttacks();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->setNumberOfAttacks(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject373;

// ConsoleClass374 aiStrategy.numberOfDefences: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x449E40, executeObjectMethod 0x44A1B0,
// setArgFromString 0x449F70, getReturnValueAsString 0x449BD0, D1 0x449BB0, D0 0x449B70.
// argc 0: dice::hfe::ai::Strategy::getNumberOfDefences() const (call at 0x44A216; ledger done)
// argc 1: dice::hfe::ai::Strategy::setNumberOfDefences(unsigned int) (call at 0x44A1FE; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass374
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("numberOfDefences", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategy()->getNumberOfDefences();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->setNumberOfDefences(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject374;

// ConsoleClass375 aiStrategy.setPrerequisite: method, 1..1 arguments, no result.
// ELF isObjectActive 0x44A2D0, executeObjectMethod 0x44A680,
// setArgFromString 0x44A400, getReturnValueAsString 0x44A290, D1 0x44A270, D0 0x44A230.
// argc 1: dice::hfe::ai::StrategyManager::setPrerequisite(std::string) (call at 0x44A6EE; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass375
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setPrerequisite", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->setPrerequisite(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject375;

// ConsoleClass376 aiStrategy.addRequiredPrecedingStrategy: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x44A9E0, executeObjectMethod 0x44AD90,
// setArgFromString 0x44AB10, getReturnValueAsString 0x44A7A0, D1 0x44A780, D0 0x44A740.
// argc 1: dice::hfe::ai::StrategyManager::addRequiredPrecedingStrategy(std::string) (call at 0x44ADF0; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass376
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addRequiredPrecedingStrategy", 0, 1, 1, "bool", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->addRequiredPrecedingStrategy(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject376;

// ConsoleClass377 aiStrategy.addProhibitedPrecedingStrategy: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x44B110, executeObjectMethod 0x44B4C0,
// setArgFromString 0x44B240, getReturnValueAsString 0x44AED0, D1 0x44AEB0, D0 0x44AE70.
// argc 1: dice::hfe::ai::StrategyManager::addProhibitedPrecedingStrategy(std::string) (call at 0x44B520; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass377
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addProhibitedPrecedingStrategy", 0, 1, 1, "bool", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->addProhibitedPrecedingStrategy(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject377;

// ConsoleClass378 aiStrategy.setStrategicObjectsModifier: method, 2..3 arguments, no result.
// ELF isObjectActive 0x44B640, executeObjectMethod 0x44BE20,
// setArgFromString 0x44B770, getReturnValueAsString 0x44B600, D1 0x44B5E0, D0 0x44B5A0.
// argc 2: dice::hfe::ai::Strategy::setStrategicObjectsModifier(dice::hfe::ai::AIStrategicObject::StrategicObjectType, float) (call at 0x44BEBE; ledger done)
// argc 3: dice::hfe::ai::Strategy::setStrategicObjectsModifier(dice::hfe::ai::AIStrategicObject::StrategicObjectType, float, dice::hfe::ai::StatusMap::Status) (call at 0x44BE90; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass378
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setStrategicObjectsModifier", 0, 2, 3, "void", false)
            m_argTypes[0] = "AIStrategicObject::StrategicObjectType";
            m_argTypes[1] = "float";
            m_argTypes[2] = "StatusMap::Status";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->setStrategicObjectsModifier(BF2_CC_ARG(AIStrategicObject::StrategicObjectType, 0), BF2_CC_ARG(float, 1));
        }
        else if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->setStrategicObjectsModifier(BF2_CC_ARG(AIStrategicObject::StrategicObjectType, 0), BF2_CC_ARG(float, 1), BF2_CC_ARG(StatusMap::Status, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(AIStrategicObject::StrategicObjectType, 1, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT(float, 2)
        AIMODULE_CC_EXTRACT_WITH(StatusMap::Status, 3, dice::hfe::ai::operator>>)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject378;

// ConsoleClass379 aiStrategy.getStrategicObjectsModifier: method, 1..2 arguments, returns float.
// ELF isObjectActive 0x44C170, executeObjectMethod 0x44C720,
// setArgFromString 0x44C2A0, getReturnValueAsString 0x44BF30, D1 0x44BF10, D0 0x44BED0.
// argc 1: dice::hfe::ai::Strategy::getStrategicObjectsModifier(dice::hfe::ai::AIStrategicObject::StrategicObjectType) const (call at 0x44C7AD; ledger done)
// argc 2: dice::hfe::ai::Strategy::getStrategicObjectsModifier(dice::hfe::ai::AIStrategicObject::StrategicObjectType, dice::hfe::ai::StatusMap::Status) const (call at 0x44C773; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass379
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getStrategicObjectsModifier", 0, 1, 2, "float", true)
            m_argTypes[0] = "AIStrategicObject::StrategicObjectType";
            m_argTypes[1] = "StatusMap::Status";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategy()->getStrategicObjectsModifier(BF2_CC_ARG(AIStrategicObject::StrategicObjectType, 0));
        }
        else if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategy()->getStrategicObjectsModifier(BF2_CC_ARG(AIStrategicObject::StrategicObjectType, 0), BF2_CC_ARG(StatusMap::Status, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(AIStrategicObject::StrategicObjectType, 1, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StatusMap::Status, 2, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject379;

// ConsoleClass380 aiStrategy.clearAllStrategicObjectsModifiers: method, 0..0 arguments, no result.
// ELF isObjectActive 0x44C860, executeObjectMethod 0x44C9A0,
// setArgFromString 0x44C990, getReturnValueAsString 0x44C820, D1 0x44C800, D0 0x44C7C0.
// argc 0: dice::hfe::ai::Strategy::clearAllStrategicObjectsModifiers() (call at 0x44C9D6; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass380
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("clearAllStrategicObjectsModifiers", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveStrategy() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveStrategy()->clearAllStrategicObjectsModifiers();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject380;

// ConsoleClass381 aiStrategy.addSpecificObjectModifier: method, 2..2 arguments, returns bool.
// ELF isObjectActive 0x44CC80, executeObjectMethod 0x44D260,
// setArgFromString 0x44CDB0, getReturnValueAsString 0x44CA40, D1 0x44CA20, D0 0x44C9E0.
// argc 2: dice::hfe::ai::StrategyManager::addSpecificObjectModifier(std::string, float) (call at 0x44D2CF; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass381
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addSpecificObjectModifier", 0, 2, 2, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "float";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->addSpecificObjectModifier(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(float, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(float, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject381;

// ConsoleClass382 aiStrategy.completeStrategies: method, 0..0 arguments, no result.
// ELF isObjectActive 0x44D3F0, executeObjectMethod 0x44D530,
// setArgFromString 0x44D520, getReturnValueAsString 0x44D3B0, D1 0x44D390, D0 0x44D350.
// argc 0: dice::hfe::ai::StrategyManager::completeStrategies() (call at 0x44D566; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass382
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("completeStrategies", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->completeStrategies();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject382;

// ConsoleClass385 aiStrategy.createPrerequisite: method, 1..1 arguments, no result.
// ELF isObjectActive 0x44D610, executeObjectMethod 0x44D9C0,
// setArgFromString 0x44D740, getReturnValueAsString 0x44D5D0, D1 0x44D5B0, D0 0x44D570.
// argc 1: dice::hfe::ai::StrategyManager::createPrerequisite(std::string) (call at 0x44DA2E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass385
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createPrerequisite", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->createPrerequisite(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject385;

// ConsoleClass386 aiStrategy.setActivePrerequisite: method, 1..1 arguments, no result.
// ELF isObjectActive 0x44DB20, executeObjectMethod 0x44DED0,
// setArgFromString 0x44DC50, getReturnValueAsString 0x44DAE0, D1 0x44DAC0, D0 0x44DA80.
// argc 1: dice::hfe::ai::StrategyManager::setActivePrerequisite(std::string) (call at 0x44DF3E; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass386
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setActivePrerequisite", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->setActivePrerequisite(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject386;

// ConsoleClass387 aiStrategy.getActivePrerequisiteName: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x44E2B0, executeObjectMethod 0x44E3F0,
// setArgFromString 0x44E3E0, getReturnValueAsString 0x44E070, D1 0x44E000, D0 0x44DF90.
// argc 0: dice::hfe::ai::StrategyManager::getActivePrerequisiteName() (call at 0x44E430; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass387
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getActivePrerequisiteName", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->getActivePrerequisiteName();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject387;

// ConsoleClass388 aiStrategy.listPrerequisites: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x44E7A0, executeObjectMethod 0x44E8E0,
// setArgFromString 0x44E8D0, getReturnValueAsString 0x44E560, D1 0x44E4F0, D0 0x44E480.
// argc 0: dice::hfe::ai::StrategyManager::listPrerequisites() (call at 0x44E920; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass388
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("listPrerequisites", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->listPrerequisites();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject388;

// ConsoleClass389 aiStrategy.addCondition: method, 1..2 arguments, no result.
// ELF isObjectActive 0x44EA10, executeObjectMethod 0x44EFF0,
// setArgFromString 0x44EB40, getReturnValueAsString 0x44E9D0, D1 0x44E9B0, D0 0x44E970.
// argc 1: dice::hfe::ai::StrategyManager::addCondition(std::string) (call at 0x44F10A; ledger done)
// argc 2: dice::hfe::ai::StrategyManager::addCondition(std::string, float) (call at 0x44F081; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass389
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addCondition", 0, 1, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "float";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->addCondition(BF2_CC_ARG(std::string, 0));
        }
        else if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->addCondition(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(float, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(float, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject389;

// ConsoleClass392 aiStrategy.setActiveCondition: method, 1..1 arguments, no result.
// ELF isObjectActive 0x44F1F0, executeObjectMethod 0x44F5A0,
// setArgFromString 0x44F320, getReturnValueAsString 0x44F1B0, D1 0x44F190, D0 0x44F150.
// argc 1: dice::hfe::ai::StrategyManager::setActiveCondition(std::string) (call at 0x44F60E; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass392
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setActiveCondition", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->setActiveCondition(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject392;

// ConsoleClass393 aiStrategy.getActiveConditionName: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x44F980, executeObjectMethod 0x44FAC0,
// setArgFromString 0x44FAB0, getReturnValueAsString 0x44F740, D1 0x44F6D0, D0 0x44F660.
// argc 0: dice::hfe::ai::StrategyManager::getActiveConditionName() (call at 0x44FB00; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass393
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getActiveConditionName", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->getActiveConditionName();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject393;

// ConsoleClass394 aiStrategy.listConditions: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x44FE70, executeObjectMethod 0x44FFB0,
// setArgFromString 0x44FFA0, getReturnValueAsString 0x44FC30, D1 0x44FBC0, D0 0x44FB50.
// argc 0: dice::hfe::ai::StrategyManager::listConditions() (call at 0x44FFF0; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass394
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("listConditions", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->listConditions();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject394;

// ConsoleClass395 aiStrategy.createConstantCondition: method, 6..6 arguments, returns bool.
// ELF isObjectActive 0x4502E0, executeObjectMethod 0x451100,
// setArgFromString 0x450410, getReturnValueAsString 0x4500A0, D1 0x450080, D0 0x450040.
// argc 6: dice::hfe::ai::StrategyManager::createConstantCondition(std::string, dice::hfe::ai::StrategyCondition::CompareType, dice::hfe::ai::StrategyCondition::Compare, dice::hfe::ai::StrategyCondition::ConditionSide, dice::hfe::ai::StrategyCondition::Conditional, float) (call at 0x4511A5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass395
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createConstantCondition", 0, 6, 6, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "StrategyCondition::CompareType";
            m_argTypes[2] = "StrategyCondition::Compare";
            m_argTypes[3] = "StrategyCondition::ConditionSide";
            m_argTypes[4] = "StrategyCondition::Conditional";
            m_argTypes[5] = "float";
    AIMODULE_CC_TAIL(6)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 6)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->createConstantCondition(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(StrategyCondition::CompareType, 1), BF2_CC_ARG(StrategyCondition::Compare, 2), BF2_CC_ARG(StrategyCondition::ConditionSide, 3), BF2_CC_ARG(StrategyCondition::Conditional, 4), BF2_CC_ARG(float, 5));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::CompareType, 2, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::Compare, 3, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::ConditionSide, 4, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::Conditional, 5, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT(float, 6)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject395;

// ConsoleClass396 aiStrategy.createHomogenousCondition: method, 4..4 arguments, returns bool.
// ELF isObjectActive 0x4514C0, executeObjectMethod 0x451EC0,
// setArgFromString 0x4515F0, getReturnValueAsString 0x451280, D1 0x451260, D0 0x451220.
// argc 4: dice::hfe::ai::StrategyManager::createHomogenousCondition(std::string, dice::hfe::ai::StrategyCondition::CompareType, dice::hfe::ai::StrategyCondition::Compare, dice::hfe::ai::StrategyCondition::Conditional) (call at 0x451F47; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass396
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createHomogenousCondition", 0, 4, 4, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "StrategyCondition::CompareType";
            m_argTypes[2] = "StrategyCondition::Compare";
            m_argTypes[3] = "StrategyCondition::Conditional";
    AIMODULE_CC_TAIL(4)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 4)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->createHomogenousCondition(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(StrategyCondition::CompareType, 1), BF2_CC_ARG(StrategyCondition::Compare, 2), BF2_CC_ARG(StrategyCondition::Conditional, 3));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::CompareType, 2, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::Compare, 3, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::Conditional, 4, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject396;

// ConsoleClass397 aiStrategy.createHeterogeneousCondition: method, 6..6 arguments, returns bool.
// ELF isObjectActive 0x452260, executeObjectMethod 0x453080,
// setArgFromString 0x452390, getReturnValueAsString 0x452020, D1 0x452000, D0 0x451FC0.
// argc 6: dice::hfe::ai::StrategyManager::createHeterogeneousCondition(std::string, dice::hfe::ai::StrategyCondition::CompareType, dice::hfe::ai::StrategyCondition::Compare, dice::hfe::ai::StrategyCondition::ConditionSide, dice::hfe::ai::StrategyCondition::Conditional, dice::hfe::ai::StrategyCondition::Conditional) (call at 0x453125; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass397
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createHeterogeneousCondition", 0, 6, 6, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "StrategyCondition::CompareType";
            m_argTypes[2] = "StrategyCondition::Compare";
            m_argTypes[3] = "StrategyCondition::ConditionSide";
            m_argTypes[4] = "StrategyCondition::Conditional";
            m_argTypes[5] = "StrategyCondition::Conditional";
    AIMODULE_CC_TAIL(6)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 6)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->createHeterogeneousCondition(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(StrategyCondition::CompareType, 1), BF2_CC_ARG(StrategyCondition::Compare, 2), BF2_CC_ARG(StrategyCondition::ConditionSide, 3), BF2_CC_ARG(StrategyCondition::Conditional, 4), BF2_CC_ARG(StrategyCondition::Conditional, 5));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::CompareType, 2, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::Compare, 3, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::ConditionSide, 4, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::Conditional, 5, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::Conditional, 6, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject397;

// ConsoleClass398 aiStrategy.createAreaCondition: method, 3..3 arguments, returns bool.
// ELF isObjectActive 0x453440, executeObjectMethod 0x453C80,
// setArgFromString 0x453570, getReturnValueAsString 0x453200, D1 0x4531E0, D0 0x4531A0.
// argc 3: dice::hfe::ai::StrategyManager::createAreaCondition(std::string, std::string, dice::hfe::ai::StatusMap::Status) (call at 0x453D1D; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass398
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createAreaCondition", 0, 3, 3, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
            m_argTypes[2] = "StatusMap::Status";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->createAreaCondition(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1), BF2_CC_ARG(StatusMap::Status, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
        AIMODULE_CC_EXTRACT_WITH(StatusMap::Status, 3, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject398;

// ConsoleClass399 aiStrategy.setConditionStrength: method, 1..1 arguments, no result.
// ELF isObjectActive 0x453E80, executeObjectMethod 0x4541F0,
// setArgFromString 0x453FB0, getReturnValueAsString 0x453E40, D1 0x453E20, D0 0x453DE0.
// argc 1: dice::hfe::ai::StrategyCondition::setConditionStrength(dice::hfe::ai::StrategyCondition::ConditionStrength) (call at 0x454238; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass399
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setConditionStrength", 0, 1, 1, "void", false)
            m_argTypes[0] = "StrategyCondition::ConditionStrength";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveCondition() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveCondition()->setConditionStrength(BF2_CC_ARG(StrategyCondition::ConditionStrength, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(StrategyCondition::ConditionStrength, 1, dice::hfe::ai::operator>>)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject399;

// ConsoleClass400 aiStrategy.getConditionStrength: method, 0..0 arguments, returns StrategyCondition::ConditionStrength.
// ELF isObjectActive 0x4544E0, executeObjectMethod 0x454620,
// setArgFromString 0x454610, getReturnValueAsString 0x4542A0, D1 0x454280, D0 0x454240.
// argc 0: dice::hfe::ai::StrategyCondition::getConditionStrength() const (call at 0x454650; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass400
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getConditionStrength", 0, 0, 0, "StrategyCondition::ConditionStrength", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveCondition() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveCondition()->getConditionStrength();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    StrategyCondition::ConditionStrength m_returnValue;
};
BF2_CC_NAME consoleObject400;

// ConsoleClass401 aiStrategy.setIsAbortCondition: method, 1..1 arguments, no result.
// ELF isObjectActive 0x454720, executeObjectMethod 0x454A90,
// setArgFromString 0x454850, getReturnValueAsString 0x4546E0, D1 0x4546C0, D0 0x454680.
// argc 1: dice::hfe::ai::StrategyCondition::setIsAbortCondition(bool) (call at 0x454AD6; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass401
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setIsAbortCondition", 0, 1, 1, "void", false)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveCondition() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveCondition()->setIsAbortCondition(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject401;

// ConsoleClass402 aiStrategy.isAbortCondition: method, 0..0 arguments, returns bool.
// ELF isObjectActive 0x454D80, executeObjectMethod 0x454EC0,
// setArgFromString 0x454EB0, getReturnValueAsString 0x454B40, D1 0x454B20, D0 0x454AE0.
// argc 0: dice::hfe::ai::StrategyCondition::isAbortCondition() const (call at 0x454EF0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass402
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("isAbortCondition", 0, 0, 0, "bool", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveCondition() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveCondition()->isAbortCondition();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject402;

// ConsoleClass403 aiStrategy.completeConditions: method, 0..0 arguments, no result.
// ELF isObjectActive 0x454FC0, executeObjectMethod 0x455100,
// setArgFromString 0x4550F0, getReturnValueAsString 0x454F80, D1 0x454F60, D0 0x454F20.
// argc 0: dice::hfe::ai::StrategyManager::completeConditions() (call at 0x455136; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass403
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("completeConditions", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->completeConditions();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject403;

// ConsoleClass404 aiStrategy.targetValue: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x4553E0, executeObjectMethod 0x455750,
// setArgFromString 0x455510, getReturnValueAsString 0x4551A0, D1 0x455180, D0 0x455140.
// argc 0: dice::hfe::ai::StrategyCondition::getTargetValue() const (call at 0x4557BF; ledger done)
// argc 1: dice::hfe::ai::StrategyCondition::setTargetValue(float) (call at 0x45579E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass404
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("targetValue", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveCondition() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveCondition()->getTargetValue();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveCondition()->setTargetValue(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject404;

// ConsoleClass407 aiStrategy.forceStrategy: method, 2..2 arguments, returns std::string.
// ELF isObjectActive 0x455B00, executeObjectMethod 0x4560E0,
// setArgFromString 0x455C30, getReturnValueAsString 0x4558C0, D1 0x455850, D0 0x4557E0.
// argc 2: dice::hfe::ai::StrategyManager::forceStrategy(std::string, int) (call at 0x456159; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass407
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("forceStrategy", 0, 2, 2, "std::string", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "int";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->forceStrategy(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(int, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(int, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject407;

// ConsoleClass408 aiStrategy.enableDebug: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x456490, executeObjectMethod 0x456800,
// setArgFromString 0x4565C0, getReturnValueAsString 0x456250, D1 0x456230, D0 0x4561F0.
// argc 0: dice::hfe::ai::StrategyManager::getEnableDebug() (call at 0x45686F; ledger done)
// argc 1: dice::hfe::ai::StrategyManager::setEnableDebug(bool) (call at 0x456850; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass408
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("enableDebug", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return StrategyManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = StrategyManager::getInstance()->getEnableDebug();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            StrategyManager::getInstance()->setEnableDebug(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject408;

// ConsoleClass411 aiStrategy.thresholdSpeed: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x456B30, executeObjectMethod 0x456E90,
// setArgFromString 0x456C50, getReturnValueAsString 0x4568F0, D1 0x4568D0, D0 0x456890.
// argc 0: dice::hfe::ai::AIConsole::getFastThreshold() (call at 0x456EF6; ledger stop)
// argc 1: dice::hfe::ai::AIConsole::setFastThreshold(float) (call at 0x456EDB; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass411
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("thresholdSpeed", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->getFastThreshold();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->setFastThreshold(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject411;

SetConsoleClassDefaults AIStrategicAreafullAccessSetConsoleClassDefaults("AIStrategicArea", 2, 0);

// ConsoleClass417 AIStrategicArea.create: method, 5..6 arguments, returns bool.
// ELF isObjectActive 0x4571B0, executeObjectMethod 0x4580E0,
// setArgFromString 0x4572E0, getReturnValueAsString 0x456F70, D1 0x456F50, D0 0x456F10.
// argc 5: dice::hfe::ai::AIStrategicObjectManager::create(std::string, dice::hfe::Vec2, dice::hfe::Vec2, float, float, std::string const&) (call at 0x458399; ledger done)
// argc 6: dice::hfe::ai::AIStrategicObjectManager::create(std::string, dice::hfe::Vec2, dice::hfe::Vec2, float, float, std::string const&) (call at 0x45820B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass417
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("create", 0, 5, 6, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "Vec2";
            m_argTypes[2] = "Vec2";
            m_argTypes[3] = "float";
            m_argTypes[4] = "float";
            m_argTypes[5] = "std::string";
    AIMODULE_CC_TAIL(6)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 5)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->create(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(Vec2, 1), BF2_CC_ARG(Vec2, 2), BF2_CC_ARG(float, 3), BF2_CC_ARG(float, 4));
        }
        else if (m_argCount == 6)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->create(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(Vec2, 1), BF2_CC_ARG(Vec2, 2), BF2_CC_ARG(float, 3), BF2_CC_ARG(float, 4), BF2_CC_ARG(std::string, 5));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(Vec2, 2)
        AIMODULE_CC_EXTRACT(Vec2, 3)
        AIMODULE_CC_EXTRACT(float, 4)
        AIMODULE_CC_EXTRACT(float, 5)
        AIMODULE_CC_EXTRACT(std::string, 6)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject417;

// ConsoleClass418 AIStrategicArea.setFlankPosition: method, 2..2 arguments, no result.
// ELF isObjectActive 0x4584D0, executeObjectMethod 0x458BC0,
// setArgFromString 0x458600, getReturnValueAsString 0x458490, D1 0x458470, D0 0x458430.
// argc 2: dice::hfe::ai::AIStrategicObjectManager::setFlankPosition(dice::hfe::Vec3, dice::hfe::Vec3) (call at 0x458C53; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass418
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setFlankPosition", 0, 2, 2, "void", false)
            m_argTypes[0] = "Vec3";
            m_argTypes[1] = "Vec3";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIStrategicObjectManager::getInstance()->setFlankPosition(BF2_CC_ARG(Vec3, 0), BF2_CC_ARG(Vec3, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec3, 1)
        AIMODULE_CC_EXTRACT(Vec3, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject418;

// ConsoleClass419 AIStrategicArea.createFromControlPoint: method, 3..4 arguments, returns bool.
// ELF isObjectActive 0x458F00, executeObjectMethod 0x459950,
// setArgFromString 0x459030, getReturnValueAsString 0x458CC0, D1 0x458CA0, D0 0x458C60.
// argc 3: dice::hfe::ai::AIStrategicObjectManager::createFromControlPoint(std::string const&, unsigned int, float, std::string const&) (call at 0x459AC9; ledger done)
// argc 4: dice::hfe::ai::AIStrategicObjectManager::createFromControlPoint(std::string const&, unsigned int, float, std::string const&) (call at 0x4599EB; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass419
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createFromControlPoint", 0, 3, 4, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "u32";
            m_argTypes[2] = "float";
            m_argTypes[3] = "std::string";
    AIMODULE_CC_TAIL(4)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->createFromControlPoint(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(u32, 1), BF2_CC_ARG(float, 2));
        }
        else if (m_argCount == 4)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->createFromControlPoint(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(u32, 1), BF2_CC_ARG(float, 2), BF2_CC_ARG(std::string, 3));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(u32, 2)
        AIMODULE_CC_EXTRACT(float, 3)
        AIMODULE_CC_EXTRACT(std::string, 4)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject419;

// ConsoleClass420 AIStrategicArea.removeActive: method, 0..0 arguments, returns bool.
// ELF isObjectActive 0x459DD0, executeObjectMethod 0x459F10,
// setArgFromString 0x459F00, getReturnValueAsString 0x459B90, D1 0x459B70, D0 0x459B30.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::removeActive() (call at 0x459F40; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass420
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("removeActive", 0, 0, 0, "bool", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->removeActive();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject420;

// ConsoleClass421 AIStrategicArea.setActive: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x45A210, executeObjectMethod 0x45A5C0,
// setArgFromString 0x45A340, getReturnValueAsString 0x459FD0, D1 0x459FB0, D0 0x459F70.
// argc 1: dice::hfe::ai::AIStrategicObjectManager::setActive(std::string) (call at 0x45A620; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass421
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setActive", 0, 1, 1, "bool", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->setActive(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject421;

// ConsoleClass422 AIStrategicArea.getActive: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x45A9C0, executeObjectMethod 0x45AB00,
// setArgFromString 0x45AAF0, getReturnValueAsString 0x45A780, D1 0x45A710, D0 0x45A6A0.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::getActive() const (call at 0x45AB40; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass422
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getActive", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->getActive();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject422;

// ConsoleClass423 AIStrategicArea.addNeighbour: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x45AE30, executeObjectMethod 0x45B1E0,
// setArgFromString 0x45AF60, getReturnValueAsString 0x45ABF0, D1 0x45ABD0, D0 0x45AB90.
// argc 1: dice::hfe::ai::AIStrategicObjectManager::addNeighbour(std::string) (call at 0x45B240; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass423
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addNeighbour", 0, 1, 1, "bool", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->addNeighbour(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject423;

// ConsoleClass424 AIStrategicArea.addTwoWayNeighbour: method, 2..2 arguments, returns bool.
// ELF isObjectActive 0x45B560, executeObjectMethod 0x45BB80,
// setArgFromString 0x45B690, getReturnValueAsString 0x45B320, D1 0x45B300, D0 0x45B2C0.
// argc 2: dice::hfe::ai::AIStrategicObjectManager::addTwoWayNeighbour(std::string, std::string) (call at 0x45BC0E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass424
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addTwoWayNeighbour", 0, 2, 2, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->addTwoWayNeighbour(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject424;

// ConsoleClass425 AIStrategicArea.getNeighbours: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x45BFF0, executeObjectMethod 0x45C130,
// setArgFromString 0x45C120, getReturnValueAsString 0x45BDB0, D1 0x45BD40, D0 0x45BCD0.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::getNeighbours() (call at 0x45C170; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass425
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getNeighbours", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->getNeighbours();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject425;

// ConsoleClass426 AIStrategicArea.layer: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x45C490, executeObjectMethod 0x45C800,
// setArgFromString 0x45C5C0, getReturnValueAsString 0x45C220, D1 0x45C200, D0 0x45C1C0.
// argc 0: inline accessor, reads the member at +0x190 (m_i400)
// argc 1: inline accessor, stores the member at +0x190 (m_i400)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass426
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("layer", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_i400;
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategicArea()->m_i400 = BF2_CC_ARG(int, 0);
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject426;

// ConsoleClass427 AIStrategicArea.setVisible: method, 1..1 arguments, no result.
// ELF isObjectActive 0x45C920, executeObjectMethod 0x45CC90,
// setArgFromString 0x45CA50, getReturnValueAsString 0x45C8E0, D1 0x45C8C0, D0 0x45C880.
// argc 1: verified inline body, normalized visibility store
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass427
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setVisible", 0, 1, 1, "void", false)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            const bool value = BF2_CC_ARG(bool, 0);
            m_hasReturnValue = false;
            getActiveStrategicArea()->m_visible = (value != 0);
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject427;

// ConsoleClass428 AIStrategicArea.getVisible: method, 0..0 arguments, returns bool.
// ELF isObjectActive 0x45CF80, executeObjectMethod 0x45D0C0,
// setArgFromString 0x45D0B0, getReturnValueAsString 0x45CD40, D1 0x45CD20, D0 0x45CCE0.
// argc 0: verified inline body, normalized visibility load
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass428
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getVisible", 0, 0, 0, "bool", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = (getActiveStrategicArea()->m_visible != 0);
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject428;

// ConsoleClass429 AIStrategicArea.drawVehicleSearchRadi: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x45D3C0, executeObjectMethod 0x45D730,
// setArgFromString 0x45D4F0, getReturnValueAsString 0x45D180, D1 0x45D160, D0 0x45D120.
// argc 0: verified inline body, vehicle-search-radius drawing switch
// argc 1: verified inline body, vehicle-search-radius drawing switch
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass429
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("drawVehicleSearchRadi", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->m_drawVehicleSearchRadii;
        }
        else if (m_argCount == 1)
        {
            const bool value = BF2_CC_ARG(bool, 0);
            m_hasReturnValue = false;
            AIStrategicObjectManager::getInstance()->m_drawVehicleSearchRadii = value;
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject429;

// ConsoleClass430 AIStrategicArea.removeAllNeighbours: method, 0..0 arguments, no result.
// ELF isObjectActive 0x45D840, executeObjectMethod 0x45D980,
// setArgFromString 0x45D970, getReturnValueAsString 0x45D800, D1 0x45D7E0, D0 0x45D7A0.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::removeAllNeighbours() (call at 0x45D9B6; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass430
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("removeAllNeighbours", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            AIStrategicObjectManager::getInstance()->removeAllNeighbours();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject430;

// ConsoleClass431 AIStrategicArea.setPosition: method, 1..1 arguments, no result.
// ELF isObjectActive 0x45DA60, executeObjectMethod 0x45DE30,
// setArgFromString 0x45DB90, getReturnValueAsString 0x45DA20, D1 0x45DA00, D0 0x45D9C0.
// argc 1: dice::hfe::ai::AIStrategicArea::setPosition(dice::hfe::Vec2 const&) (call at 0x45DE84; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass431
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setPosition", 0, 1, 1, "void", false)
            m_argTypes[0] = "hfe::Vec2";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategicArea()->setPosition(BF2_CC_ARG(hfe::Vec2, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(hfe::Vec2, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject431;

// ConsoleClass432 AIStrategicArea.getPosition: method, 0..0 arguments, returns hfe::Vec2.
// ELF isObjectActive 0x45E160, executeObjectMethod 0x45E2A0,
// setArgFromString 0x45E290, getReturnValueAsString 0x45DEF0, D1 0x45DED0, D0 0x45DE90.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::getPosition() const (call at 0x45E2E5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass432
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getPosition", 0, 0, 0, "hfe::Vec2", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->getPosition();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::Vec2 m_returnValue;
};
BF2_CC_NAME consoleObject432;

// ConsoleClass433 AIStrategicArea.centerPosition: property, 0..1 arguments, returns hfe::Vec2.
// ELF isObjectActive 0x45E5F0, executeObjectMethod 0x45E9C0,
// setArgFromString 0x45E720, getReturnValueAsString 0x45E380, D1 0x45E360, D0 0x45E320.
// argc 0: dice::hfe::ai::AIStrategicArea::getCenterPosition() const (call at 0x45EA38; ledger done)
// argc 1: dice::hfe::ai::AIStrategicArea::setCenterPosition(dice::hfe::Vec2 const&) (call at 0x45EA1A; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass433
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("centerPosition", 1, 0, 1, "hfe::Vec2", true)
            m_argTypes[0] = "hfe::Vec2";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->getCenterPosition();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategicArea()->setCenterPosition(BF2_CC_ARG(hfe::Vec2, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(hfe::Vec2, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::Vec2 m_returnValue;
};
BF2_CC_NAME consoleObject433;

// ConsoleClass434 AIStrategicArea.topLeftCorner: property, 0..1 arguments, returns hfe::Vec2.
// ELF isObjectActive 0x45ED20, executeObjectMethod 0x45F0F0,
// setArgFromString 0x45EE50, getReturnValueAsString 0x45EAB0, D1 0x45EA90, D0 0x45EA50.
// argc 0: inline accessor, reads the member at +0x110 (m_centre)
// argc 1: dice::hfe::ai::AIStrategicArea::setTopLeftCorner(dice::hfe::Vec2 const&) (call at 0x45F14A; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass434
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("topLeftCorner", 1, 0, 1, "hfe::Vec2", true)
            m_argTypes[0] = "hfe::Vec2";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_centre;
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategicArea()->setTopLeftCorner(BF2_CC_ARG(hfe::Vec2, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(hfe::Vec2, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    hfe::Vec2 m_returnValue;
};
BF2_CC_NAME consoleObject434;

// ConsoleClass435 AIStrategicArea.list: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x45F4A0, executeObjectMethod 0x45F5E0,
// setArgFromString 0x45F5D0, getReturnValueAsString 0x45F260, D1 0x45F1F0, D0 0x45F180.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::list() const (call at 0x45F620; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass435
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("list", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->list();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject435;

// ConsoleClass436 AIStrategicArea.addObjectTypeFlag: method, 1..1 arguments, returns int.
// ELF isObjectActive 0x45F940, executeObjectMethod 0x45FCB0,
// setArgFromString 0x45FA70, getReturnValueAsString 0x45F6D0, D1 0x45F6B0, D0 0x45F670.
// argc 1: dice::hfe::ai::AIStrategicObjectManager::addObjectTypeFlag(dice::hfe::ai::AIStrategicObject::StrategicObjectType) (call at 0x45FCEB; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass436
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addObjectTypeFlag", 0, 1, 1, "int", true)
            m_argTypes[0] = "AIStrategicObject::StrategicObjectType";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->addObjectTypeFlag(BF2_CC_ARG(AIStrategicObject::StrategicObjectType, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(AIStrategicObject::StrategicObjectType, 1, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject436;

// ConsoleClass437 AIStrategicArea.setObjectTypeFlags: method, 1..1 arguments, returns int.
// ELF isObjectActive 0x45FFF0, executeObjectMethod 0x460360,
// setArgFromString 0x460120, getReturnValueAsString 0x45FD80, D1 0x45FD60, D0 0x45FD20.
// argc 1: dice::hfe::ai::AIStrategicObjectManager::setObjectTypeFlags(int) (call at 0x46039B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass437
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setObjectTypeFlags", 0, 1, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->setObjectTypeFlags(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject437;

// ConsoleClass438 AIStrategicArea.getObjectTypeFlags: method, 0..0 arguments, returns int.
// ELF isObjectActive 0x4606A0, executeObjectMethod 0x4607E0,
// setArgFromString 0x4607D0, getReturnValueAsString 0x460430, D1 0x460410, D0 0x4603D0.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::getObjectTypeFlags() (call at 0x460810; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass438
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getObjectTypeFlags", 0, 0, 0, "int", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->getObjectTypeFlags();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject438;

// ConsoleClass439 AIStrategicArea.removeObjectTypeFlag: method, 1..1 arguments, returns int.
// ELF isObjectActive 0x460B10, executeObjectMethod 0x460E80,
// setArgFromString 0x460C40, getReturnValueAsString 0x4608A0, D1 0x460880, D0 0x460840.
// argc 1: dice::hfe::ai::AIStrategicObjectManager::removeObjectTypeFlag(dice::hfe::ai::AIStrategicObject::StrategicObjectType) (call at 0x460EBB; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass439
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("removeObjectTypeFlag", 0, 1, 1, "int", true)
            m_argTypes[0] = "AIStrategicObject::StrategicObjectType";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->removeObjectTypeFlag(BF2_CC_ARG(AIStrategicObject::StrategicObjectType, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(AIStrategicObject::StrategicObjectType, 1, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject439;

// ConsoleClass440 AIStrategicArea.setStatus: method, 2..2 arguments, returns StatusMap::Status.
// ELF isObjectActive 0x461190, executeObjectMethod 0x461740,
// setArgFromString 0x4612C0, getReturnValueAsString 0x460F50, D1 0x460F30, D0 0x460EF0.
// argc 2: verified inline body, unchecked status stores: saved pair first, current pair second, then return the status
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass440
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setStatus", 0, 2, 2, "StatusMap::Status", true)
            m_argTypes[0] = "int";
            m_argTypes[1] = "StatusMap::Status";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            const int side = BF2_CC_ARG(int, 0);
            const StatusMap::Status value = BF2_CC_ARG(StatusMap::Status, 1);
            m_hasReturnValue = true;
            AIStrategicArea* const object = getActiveStrategicArea();
            object->m_status[side + 1] = value;
            object->m_status[side - 1] = value;
            m_returnValue = value;
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
        AIMODULE_CC_EXTRACT_WITH(StatusMap::Status, 2, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    StatusMap::Status m_returnValue;
};
BF2_CC_NAME consoleObject440;

// ConsoleClass441 AIStrategicArea.category: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x461A80, executeObjectMethod 0x461DF0,
// setArgFromString 0x461BB0, getReturnValueAsString 0x461810, D1 0x4617F0, D0 0x4617B0.
// argc 0: verified inline body, inherited category property
// argc 1: verified inline body, inherited category property
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass441
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("category", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_category;
        }
        else if (m_argCount == 1)
        {
            const int value = BF2_CC_ARG(int, 0);
            m_hasReturnValue = false;
            getActiveStrategicArea()->m_category = value;
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject441;

// ConsoleClass442 AIStrategicArea.vehicleSearchRadius: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x462110, executeObjectMethod 0x462480,
// setArgFromString 0x462240, getReturnValueAsString 0x461ED0, D1 0x461EB0, D0 0x461E70.
// argc 0: inline accessor, reads the member at +0x18C (m_f396)
// argc 1: inline accessor, stores the member at +0x18C (m_f396)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass442
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("vehicleSearchRadius", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_f396;
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategicArea()->m_f396 = BF2_CC_ARG(float, 0);
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject442;

// ConsoleClass443 AIStrategicArea.setTakeable: method, 2..2 arguments, no result.
// ELF isObjectActive 0x4625A0, executeObjectMethod 0x462B50,
// setArgFromString 0x4626D0, getReturnValueAsString 0x462560, D1 0x462540, D0 0x462500.
// argc 2: verified inline body, unchecked one-based takeability store
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass443
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setTakeable", 0, 2, 2, "void", false)
            m_argTypes[0] = "int";
            m_argTypes[1] = "bool";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            const int side = BF2_CC_ARG(int, 0);
            const bool value = BF2_CC_ARG(bool, 1);
            m_hasReturnValue = false;
            getActiveStrategicArea()->m_sideEnabled[side - 1] = value;
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
        AIMODULE_CC_EXTRACT(bool, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject443;

// ConsoleClass444 AIStrategicArea.isTakeable: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x462E50, executeObjectMethod 0x4631C0,
// setArgFromString 0x462F80, getReturnValueAsString 0x462C10, D1 0x462BF0, D0 0x462BB0.
// argc 1: verified inline body, unchecked one-based takeability load
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass444
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("isTakeable", 0, 1, 1, "bool", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            const int side = BF2_CC_ARG(int, 0);
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_sideEnabled[side - 1];
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject444;

// ConsoleClass445 AIStrategicArea.removeAllObjectTypeFlags: method, 0..0 arguments, no result.
// ELF isObjectActive 0x4632C0, executeObjectMethod 0x463400,
// setArgFromString 0x4633F0, getReturnValueAsString 0x463280, D1 0x463260, D0 0x463220.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::removeAllObjectTypeFlags() (call at 0x463436; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass445
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("removeAllObjectTypeFlags", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            AIStrategicObjectManager::getInstance()->removeAllObjectTypeFlags();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject445;

// ConsoleClass446 AIStrategicArea.getObjectTypeFlagMapping: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x463760, executeObjectMethod 0x4638A0,
// setArgFromString 0x463890, getReturnValueAsString 0x463520, D1 0x4634B0, D0 0x463440.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::getObjectTypeFlagMapping() const (call at 0x4638E0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass446
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getObjectTypeFlagMapping", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->getObjectTypeFlagMapping();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject446;

// ConsoleClass447 AIStrategicArea.basicTemperature: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x463BD0, executeObjectMethod 0x463F40,
// setArgFromString 0x463D00, getReturnValueAsString 0x463990, D1 0x463970, D0 0x463930.
// argc 0: verified inline body, inherited baseline temperature, the first word of the shared temperature/radius storage
// argc 1: verified inline body, inherited baseline temperature, the first word of the shared temperature/radius storage
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass447
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("basicTemperature", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_size[AIStrategicObject::kBasicTemperature];
        }
        else if (m_argCount == 1)
        {
            const float value = BF2_CC_ARG(float, 0);
            m_hasReturnValue = false;
            getActiveStrategicArea()->m_size[AIStrategicObject::kBasicTemperature] = value;
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject447;

// ConsoleClass448 AIStrategicArea.setOrderPosition: method, 2..2 arguments, returns bool.
// ELF isObjectActive 0x464260, executeObjectMethod 0x4648F0,
// setArgFromString 0x464390, getReturnValueAsString 0x464020, D1 0x464000, D0 0x463FC0.
// argc 2: dice::hfe::ai::AIStrategicObjectManager::setOrderPosition(std::string, dice::hfe::Vec3 const&) (call at 0x46496B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass448
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setOrderPosition", 0, 2, 2, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "Vec3";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->setOrderPosition(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(Vec3, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(Vec3, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject448;

// ConsoleClass449 AIStrategicArea.getOrderPosition: method, 1..1 arguments, returns Vec3.
// ELF isObjectActive 0x464CE0, executeObjectMethod 0x465090,
// setArgFromString 0x464E10, getReturnValueAsString 0x464A40, D1 0x464A20, D0 0x4649E0.
// argc 1: dice::hfe::ai::AIStrategicObjectManager::getOrderPosition(std::string) (call at 0x465100; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass449
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getOrderPosition", 0, 1, 1, "Vec3", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->getOrderPosition(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    Vec3 m_returnValue;
};
BF2_CC_NAME consoleObject449;

// ConsoleClass450 AIStrategicArea.listOrderPositions: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x4654A0, executeObjectMethod 0x4655E0,
// setArgFromString 0x4655D0, getReturnValueAsString 0x465260, D1 0x4651F0, D0 0x465180.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::listOrderPositions() (call at 0x465620; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass450
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("listOrderPositions", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->listOrderPositions();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject450;

// ConsoleClass451 AIStrategicArea.addWayPoint: method, 4..4 arguments, no result.
// ELF isObjectActive 0x465710, executeObjectMethod 0x466290,
// setArgFromString 0x465840, getReturnValueAsString 0x4656D0, D1 0x4656B0, D0 0x465670.
// argc 4: dice::hfe::ai::AIStrategicObjectManager::addWayPoint(dice::hfe::Vec3 const&, std::string const&, std::string const&, std::string const&) (call at 0x46634B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass451
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addWayPoint", 0, 4, 4, "void", false)
            m_argTypes[0] = "Vec3";
            m_argTypes[1] = "std::string";
            m_argTypes[2] = "std::string";
            m_argTypes[3] = "std::string";
    AIMODULE_CC_TAIL(4)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 4)
        {
            m_hasReturnValue = false;
            AIStrategicObjectManager::getInstance()->addWayPoint(BF2_CC_ARG(Vec3, 0), BF2_CC_ARG(std::string, 1), BF2_CC_ARG(std::string, 2), BF2_CC_ARG(std::string, 3));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec3, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
        AIMODULE_CC_EXTRACT(std::string, 3)
        AIMODULE_CC_EXTRACT(std::string, 4)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject451;

// ConsoleClass452 AIStrategicArea.removeWayPoint: method, 4..4 arguments, returns bool.
// ELF isObjectActive 0x466660, executeObjectMethod 0x4671E0,
// setArgFromString 0x466790, getReturnValueAsString 0x466420, D1 0x466400, D0 0x4663C0.
// argc 4: dice::hfe::ai::AIStrategicObjectManager::removeWayPoint(dice::hfe::Vec3 const&, std::string const&, std::string const&, std::string const&) (call at 0x467287; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass452
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("removeWayPoint", 0, 4, 4, "bool", true)
            m_argTypes[0] = "Vec3";
            m_argTypes[1] = "std::string";
            m_argTypes[2] = "std::string";
            m_argTypes[3] = "std::string";
    AIMODULE_CC_TAIL(4)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 4)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->removeWayPoint(BF2_CC_ARG(Vec3, 0), BF2_CC_ARG(std::string, 1), BF2_CC_ARG(std::string, 2), BF2_CC_ARG(std::string, 3));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec3, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
        AIMODULE_CC_EXTRACT(std::string, 3)
        AIMODULE_CC_EXTRACT(std::string, 4)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject452;

// ConsoleClass453 AIStrategicArea.clearWayPoints: method, 3..3 arguments, no result.
// ELF isObjectActive 0x4673C0, executeObjectMethod 0x467C40,
// setArgFromString 0x4674F0, getReturnValueAsString 0x467380, D1 0x467360, D0 0x467320.
// argc 3: dice::hfe::ai::AIStrategicObjectManager::clearWayPoints(std::string const&, std::string const&, std::string const&) (call at 0x467CD7; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass453
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("clearWayPoints", 0, 3, 3, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
            m_argTypes[2] = "std::string";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            AIStrategicObjectManager::getInstance()->clearWayPoints(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1), BF2_CC_ARG(std::string, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
        AIMODULE_CC_EXTRACT(std::string, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject453;

// ConsoleClass454 AIStrategicArea.setWayPointsToDraw: method, 2..2 arguments, no result.
// ELF isObjectActive 0x467DE0, executeObjectMethod 0x468400,
// setArgFromString 0x467F10, getReturnValueAsString 0x467DA0, D1 0x467D80, D0 0x467D40.
// argc 2: dice::hfe::ai::AIStrategicObjectManager::setWayPointsToDraw(std::string const&, std::string const&) (call at 0x46847B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass454
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setWayPointsToDraw", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            AIStrategicObjectManager::getInstance()->setWayPointsToDraw(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject454;

// ConsoleClass455 AIStrategicArea.clearWaypointToDraw: method, 0..0 arguments, no result.
// ELF isObjectActive 0x468570, executeObjectMethod 0x4686B0,
// setArgFromString 0x4686A0, getReturnValueAsString 0x468530, D1 0x468510, D0 0x4684D0.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::clearWaypointToDraw() (call at 0x4686E6; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass455
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("clearWaypointToDraw", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            AIStrategicObjectManager::getInstance()->clearWaypointToDraw();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject455;

// ConsoleClass456 AIStrategicArea.setSide: method, 1..1 arguments, no result.
// ELF isObjectActive 0x468790, executeObjectMethod 0x468B00,
// setArgFromString 0x4688C0, getReturnValueAsString 0x468750, D1 0x468730, D0 0x4686F0.
// argc 1: dice::hfe::ai::AIStrategicObject::setSide(int) (call at 0x468B48; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass456
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setSide", 0, 1, 1, "void", false)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategicArea()->setSide(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject456;

// ConsoleClass457 AIStrategicArea.getSide: method, 0..0 arguments, returns int.
// ELF isObjectActive 0x468E20, executeObjectMethod 0x468F60,
// setArgFromString 0x468F50, getReturnValueAsString 0x468BB0, D1 0x468B90, D0 0x468B50.
// argc 0: dice::hfe::ai::AIStrategicObject::getSide() (call at 0x468F90; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass457
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getSide", 0, 0, 0, "int", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->getSide();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject457;

// ConsoleClass458 AIStrategicArea.enableDebug: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x469260, executeObjectMethod 0x4695D0,
// setArgFromString 0x469390, getReturnValueAsString 0x469020, D1 0x469000, D0 0x468FC0.
// argc 1: verified inline body, store and return the debug switch
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass458
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("enableDebug", 0, 1, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            const bool value = BF2_CC_ARG(bool, 0);
            m_hasReturnValue = true;
            AIStrategicObjectManager::getInstance()->m_debugEnabled = value;
            m_returnValue = value;
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject458;

// ConsoleClass459 AIStrategicArea.addAllowedVehicleGroup: method, 1..1 arguments, no result.
// ELF isObjectActive 0x4696D0, executeObjectMethod 0x469A80,
// setArgFromString 0x469800, getReturnValueAsString 0x469690, D1 0x469670, D0 0x469630.
// argc 1: dice::hfe::ai::AIStrategicObject::addAllowedVehicleGroup(std::string const&) (call at 0x469ADA; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass459
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addAllowedVehicleGroup", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategicArea()->addAllowedVehicleGroup(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject459;

// ConsoleClass460 AIStrategicArea.attachLandingZone: method, 1..1 arguments, no result.
// ELF isObjectActive 0x469BB0, executeObjectMethod 0x469F60,
// setArgFromString 0x469CE0, getReturnValueAsString 0x469B70, D1 0x469B50, D0 0x469B10.
// argc 1: dice::hfe::ai::AIStrategicArea::attachLandingZone(std::string const&) (call at 0x469FC6; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass460
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("attachLandingZone", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveStrategicArea()->attachLandingZone(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject460;

// ConsoleClass461 AIStrategicArea.listLandingZones: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x46A330, executeObjectMethod 0x46A470,
// setArgFromString 0x46A460, getReturnValueAsString 0x46A0F0, D1 0x46A080, D0 0x46A010.
// argc 0: dice::hfe::ai::AIStrategicArea::listLandingZones() (call at 0x46A4B0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass461
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("listLandingZones", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->listLandingZones();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject461;

// ConsoleClass462 AIStrategicArea.getTemperature: method, 1..1 arguments, returns float.
// ELF isObjectActive 0x46A7A0, executeObjectMethod 0x46AB10,
// setArgFromString 0x46A8D0, getReturnValueAsString 0x46A560, D1 0x46A540, D0 0x46A500.
// argc 1: verified inline body, unchecked one-based current-temperature load
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass462
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getTemperature", 0, 1, 1, "float", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            const int side = BF2_CC_ARG(int, 0);
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_f164[side - 1];
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject462;

// ConsoleClass463 AIStrategicArea.getStatus: method, 1..1 arguments, returns StatusMap::Status.
// ELF isObjectActive 0x46AE10, executeObjectMethod 0x46B180,
// setArgFromString 0x46AF40, getReturnValueAsString 0x46ABD0, D1 0x46ABB0, D0 0x46AB70.
// argc 1: verified inline body, unchecked one-based current-status load
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass463
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getStatus", 0, 1, 1, "StatusMap::Status", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            const int side = BF2_CC_ARG(int, 0);
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_status[side - 1];
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    StatusMap::Status m_returnValue;
};
BF2_CC_NAME consoleObject463;

// ConsoleClass464 AIStrategicArea.getNumberOfPresentUnits: method, 1..1 arguments, returns int.
// ELF isObjectActive 0x46B4B0, executeObjectMethod 0x46B820,
// setArgFromString 0x46B5E0, getReturnValueAsString 0x46B240, D1 0x46B220, D0 0x46B1E0.
// argc 1: dice::hfe::ai::AIStrategicObject::getNumberOfPresentUnits(int) const (call at 0x46B85B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass464
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getNumberOfPresentUnits", 0, 1, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->getNumberOfPresentUnits(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject464;

// ConsoleClass465 AIStrategicArea.getNumberOfAvailableAttackers: method, 1..1 arguments, returns int.
// ELF isObjectActive 0x46BB60, executeObjectMethod 0x46BED0,
// setArgFromString 0x46BC90, getReturnValueAsString 0x46B8F0, D1 0x46B8D0, D0 0x46B890.
// argc 1: dice::hfe::ai::AIStrategicObject::getNumberOfAvailableAttackers(int) const (call at 0x46BF0B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass465
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getNumberOfAvailableAttackers", 0, 1, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->getNumberOfAvailableAttackers(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject465;

// ConsoleClass466 AIStrategicArea.getNumberOfAvailableDefenders: method, 1..1 arguments, returns int.
// ELF isObjectActive 0x46C210, executeObjectMethod 0x46C580,
// setArgFromString 0x46C340, getReturnValueAsString 0x46BFA0, D1 0x46BF80, D0 0x46BF40.
// argc 1: dice::hfe::ai::AIStrategicObject::getNumberOfAvailableDefenders(int) const (call at 0x46C5BB; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass466
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getNumberOfAvailableDefenders", 0, 1, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->getNumberOfAvailableDefenders(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject466;

// ConsoleClass467 AIStrategicArea.getNumberOfAssignedUnits: method, 1..1 arguments, returns int.
// ELF isObjectActive 0x46C8C0, executeObjectMethod 0x46CC30,
// setArgFromString 0x46C9F0, getReturnValueAsString 0x46C650, D1 0x46C630, D0 0x46C5F0.
// argc 1: dice::hfe::ai::AIStrategicObject::getNumberOfAssignedUnits(int) const (call at 0x46CC6B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass467
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getNumberOfAssignedUnits", 0, 1, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->getNumberOfAssignedUnits(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject467;

// ConsoleClass468 AIStrategicArea.getCategoryName: method, 1..1 arguments, returns std::string.
// ELF isObjectActive 0x46CFC0, executeObjectMethod 0x46D330,
// setArgFromString 0x46D0F0, getReturnValueAsString 0x46CD80, D1 0x46CD10, D0 0x46CCA0.
// argc 1: dice::hfe::ai::AIStrategicObjectManager::getCategoryName(int) const (call at 0x46D371; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass468
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getCategoryName", 0, 1, 1, "std::string", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->getCategoryName(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject468;

// ConsoleClass469 AIStrategicArea.getCategoryFromName: method, 1..1 arguments, returns int.
// ELF isObjectActive 0x46D670, executeObjectMethod 0x46DA20,
// setArgFromString 0x46D7A0, getReturnValueAsString 0x46D400, D1 0x46D3E0, D0 0x46D3A0.
// argc 1: dice::hfe::ai::AIStrategicObjectManager::getCategoryFromName(std::string const&) const (call at 0x46DA6E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass469
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getCategoryFromName", 0, 1, 1, "int", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->getCategoryFromName(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject469;

// ConsoleClass470 AIStrategicArea.createNewCategory: method, 1..1 arguments, no result.
// ELF isObjectActive 0x46DB60, executeObjectMethod 0x46DF10,
// setArgFromString 0x46DC90, getReturnValueAsString 0x46DB20, D1 0x46DB00, D0 0x46DAC0.
// argc 1: dice::hfe::ai::AIStrategicObjectManager::createNewCategory(std::string const&) (call at 0x46DF6A; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass470
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createNewCategory", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIStrategicObjectManager::getInstance()->createNewCategory(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject470;

// ConsoleClass471 AIStrategicArea.showNeighbours: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x46E240, executeObjectMethod 0x46E5B0,
// setArgFromString 0x46E370, getReturnValueAsString 0x46E000, D1 0x46DFE0, D0 0x46DFA0.
// argc 0: verified inline body, neighbour drawing switch
// argc 1: verified inline body, neighbour drawing switch
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass471
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("showNeighbours", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_showNeighbours;
        }
        else if (m_argCount == 1)
        {
            const bool value = BF2_CC_ARG(bool, 0);
            m_hasReturnValue = false;
            getActiveStrategicArea()->m_showNeighbours = value;
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject471;

// ConsoleClass472 AIStrategicArea.getStrategicAreaNames: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x46E950, executeObjectMethod 0x46EA90,
// setArgFromString 0x46EA80, getReturnValueAsString 0x46E710, D1 0x46E6A0, D0 0x46E630.
// argc 0: dice::hfe::ai::AIStrategicObjectManager::getStrategicAreaNames() (call at 0x46EAD0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass472
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getStrategicAreaNames", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->getStrategicAreaNames();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject472;

// ConsoleClass473 AIStrategicArea.isInside: method, 2..2 arguments, returns bool.
// ELF isObjectActive 0x46EDC0, executeObjectMethod 0x46F400,
// setArgFromString 0x46EEF0, getReturnValueAsString 0x46EB80, D1 0x46EB60, D0 0x46EB20.
// argc 2: dice::hfe::ai::AIStrategicObjectManager::isInside(std::string const&, dice::hfe::Vec2 const&) (call at 0x46F46D; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass473
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("isInside", 0, 2, 2, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "hfe::Vec2";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIStrategicObjectManager::getInstance() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = AIStrategicObjectManager::getInstance()->isInside(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(hfe::Vec2, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(hfe::Vec2, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject473;

// ConsoleClass474 AIStrategicArea.strategicAreaName: property, 0..1 arguments, returns std::string.
// ELF isObjectActive 0x46F7E0, executeObjectMethod 0x46FB90,
// setArgFromString 0x46F910, getReturnValueAsString 0x46F5A0, D1 0x46F530, D0 0x46F4C0.
// argc 0: verified inline body, name load; shipped defect: setter copies and destroys the argument without assigning it
// argc 1: verified inline body, name load; shipped defect: setter copies and destroys the argument without assigning it
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass474
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("strategicAreaName", 1, 0, 1, "std::string", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveStrategicArea() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveStrategicArea()->m_name;
        }
        else if (m_argCount == 1)
        {
            std::string value(BF2_CC_ARG(std::string, 0));
            m_hasReturnValue = false;
            (void)getActiveStrategicArea();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject474;

// ConsoleClass476 AIStrategicArea.saveAll: method, 2..2 arguments, returns bool.
// ELF isObjectActive 0x46FED0, executeObjectMethod 0x4704E0,
// setArgFromString 0x46FFF0, getReturnValueAsString 0x46FC90, D1 0x46FC70, D0 0x46FC30.
// argc 2: dice::hfe::ai::AIConsole::saveAll(std::string const&, std::string const&) (call at 0x470550; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass476
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("saveAll", 0, 2, 2, "bool", true)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "std::string";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = AIConsole::m_instance->saveAll(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(std::string, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject476;

// ConsoleClass477 AIStrategicArea.addAllowedUnit: method, 1..1 arguments, no result.
// ELF isObjectActive 0x470670, executeObjectMethod 0x470A10,
// setArgFromString 0x470790, getReturnValueAsString 0x470630, D1 0x470610, D0 0x4705D0.
// argc 1: dice::hfe::ai::AIConsole::addAllowedUnit(std::string const&) (call at 0x470A6D; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass477
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addAllowedUnit", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->addAllowedUnit(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject477;

// ConsoleClass478 AIStrategicArea.addExpelledUnit: method, 1..1 arguments, no result.
// ELF isObjectActive 0x470B40, executeObjectMethod 0x470EE0,
// setArgFromString 0x470C60, getReturnValueAsString 0x470B00, D1 0x470AE0, D0 0x470AA0.
// argc 1: dice::hfe::ai::AIConsole::addExpelledUnit(std::string const&) (call at 0x470F3D; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass478
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addExpelledUnit", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->addExpelledUnit(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject478;

// ConsoleClass479 AIStrategicArea.addLandingZoneUnit: method, 1..1 arguments, no result.
// ELF isObjectActive 0x471010, executeObjectMethod 0x4713B0,
// setArgFromString 0x471130, getReturnValueAsString 0x470FD0, D1 0x470FB0, D0 0x470F70.
// argc 1: dice::hfe::ai::AIConsole::addLandingZoneUnit(std::string const&) (call at 0x47140D; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass479
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addLandingZoneUnit", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIConsole::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            AIConsole::m_instance->addLandingZoneUnit(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject479;

SetConsoleClassDefaults AILandingZonefullAccessSetConsoleClassDefaults("AILandingZone", 2, 0);

// ConsoleClass485 AILandingZone.activeZone: method, 0..1 arguments, returns std::string.
// ELF isObjectActive 0x471760, executeObjectMethod 0x471B00,
// setArgFromString 0x471880, getReturnValueAsString 0x471520, D1 0x4714B0, D0 0x471440.
// argc 0: dice::hfe::ai::AILandingZoneManager::activeZone() (call at 0x471BB8; ledger done)
// argc 1: dice::hfe::ai::AILandingZoneManager::activeZone(std::string const&) (call at 0x471B5A; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass485
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("activeZone", 0, 0, 1, "std::string", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AILandingZoneManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AILandingZoneManager::m_instance->activeZone();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = AILandingZoneManager::m_instance->activeZone(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject485;

// ConsoleClass486 AILandingZone.createLandingZone: method, 5..5 arguments, no result.
// ELF isObjectActive 0x471C70, executeObjectMethod 0x472990,
// setArgFromString 0x471D90, getReturnValueAsString 0x471C30, D1 0x471C10, D0 0x471BD0.
// argc 5: dice::hfe::ai::AILandingZoneManager::createLandingZone(std::string const&, dice::hfe::Vec2 const&, dice::hfe::Vec2 const&, float, dice::hfe::ai::LandingZoneDirection) (call at 0x472A38; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass486
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createLandingZone", 0, 5, 5, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "Vec2";
            m_argTypes[2] = "Vec2";
            m_argTypes[3] = "float";
            m_argTypes[4] = "LandingZoneDirection";
    AIMODULE_CC_TAIL(5)
    virtual bool isObjectActive() const { return AILandingZoneManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 5)
        {
            m_hasReturnValue = false;
            AILandingZoneManager::m_instance->createLandingZone(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(Vec2, 1), BF2_CC_ARG(Vec2, 2), BF2_CC_ARG(float, 3), BF2_CC_ARG(LandingZoneDirection, 4));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(Vec2, 2)
        AIMODULE_CC_EXTRACT(Vec2, 3)
        AIMODULE_CC_EXTRACT(float, 4)
        AIMODULE_CC_EXTRACT_WITH(LandingZoneDirection, 5, dice::hfe::ai::operator>>)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject486;

// ConsoleClass487 AILandingZone.listZones: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x472D90, executeObjectMethod 0x472EC0,
// setArgFromString 0x472EB0, getReturnValueAsString 0x472B50, D1 0x472AE0, D0 0x472A70.
// argc 0: dice::hfe::ai::AILandingZoneManager::listZones() (call at 0x472F03; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass487
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("listZones", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return AILandingZoneManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AILandingZoneManager::m_instance->listZones();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject487;

// ConsoleClass488 AILandingZone.minCorner: property, 0..1 arguments, returns Vec2.
// ELF isObjectActive 0x473230, executeObjectMethod 0x473600,
// setArgFromString 0x473360, getReturnValueAsString 0x472FC0, D1 0x472FA0, D0 0x472F60.
// argc 0: dice::hfe::ai::AILandingZone::getMinCorner() const (call at 0x473678; ledger done)
// argc 1: dice::hfe::ai::AILandingZone::setMinCorner(dice::hfe::Vec2 const&) (call at 0x47365A; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass488
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("minCorner", 1, 0, 1, "Vec2", true)
            m_argTypes[0] = "Vec2";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveLandingZone() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveLandingZone()->getMinCorner();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveLandingZone()->setMinCorner(BF2_CC_ARG(Vec2, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec2, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    Vec2 m_returnValue;
};
BF2_CC_NAME consoleObject488;

// ConsoleClass489 AILandingZone.maxCorner: property, 0..1 arguments, returns Vec2.
// ELF isObjectActive 0x473960, executeObjectMethod 0x473D30,
// setArgFromString 0x473A90, getReturnValueAsString 0x4736F0, D1 0x4736D0, D0 0x473690.
// argc 0: dice::hfe::ai::AILandingZone::getMaxCorner() const (call at 0x473DA8; ledger done)
// argc 1: dice::hfe::ai::AILandingZone::setMaxCorner(dice::hfe::Vec2 const&) (call at 0x473D8A; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass489
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("maxCorner", 1, 0, 1, "Vec2", true)
            m_argTypes[0] = "Vec2";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveLandingZone() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveLandingZone()->getMaxCorner();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveLandingZone()->setMaxCorner(BF2_CC_ARG(Vec2, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec2, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    Vec2 m_returnValue;
};
BF2_CC_NAME consoleObject489;

// ConsoleClass490 AILandingZone.beachDirection: property, 0..1 arguments, returns LandingZoneDirection.
// ELF isObjectActive 0x474060, executeObjectMethod 0x4743D0,
// setArgFromString 0x474190, getReturnValueAsString 0x473E20, D1 0x473E00, D0 0x473DC0.
// argc 0: dice::hfe::ai::AILandingZone::getBeachDirection() const (call at 0x474436; ledger done)
// argc 1: dice::hfe::ai::AILandingZone::setBeachDirection(dice::hfe::ai::LandingZoneDirection) (call at 0x47441E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass490
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("beachDirection", 1, 0, 1, "LandingZoneDirection", true)
            m_argTypes[0] = "LandingZoneDirection";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveLandingZone() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveLandingZone()->getBeachDirection();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveLandingZone()->setBeachDirection(BF2_CC_ARG(LandingZoneDirection, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT_WITH(LandingZoneDirection, 1, dice::hfe::ai::operator>>)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    LandingZoneDirection m_returnValue;
};
BF2_CC_NAME consoleObject490;

SetConsoleClassDefaults AIPathFindingfullAccessSetConsoleClassDefaults("AIPathFinding", 2, 0);

// ConsoleClass496 AIPathFinding.createMap: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x4746F0, executeObjectMethod 0x474A90,
// setArgFromString 0x474810, getReturnValueAsString 0x4744B0, D1 0x474490, D0 0x474450.
// argc 1: dice::hfe::ai::AIPathfinding::createMap(std::string const&) (slot +0x10 at 0x474AE3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass496
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createMap", 0, 1, 1, "bool", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfindingSetup::instance->createMap(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject496;

// ConsoleClass497 AIPathFinding.deleteMap: method, 1..1 arguments, returns bool.
// ELF isObjectActive 0x474DD0, executeObjectMethod 0x475170,
// setArgFromString 0x474EF0, getReturnValueAsString 0x474B90, D1 0x474B70, D0 0x474B30.
// argc 1: dice::hfe::ai::AIPathfinding::deleteMap(std::string const&) (slot +0x14 at 0x4751C3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass497
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("deleteMap", 0, 1, 1, "bool", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfindingSetup::instance->deleteMap(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject497;

// ConsoleClass498 AIPathFinding.activeMap: property, 0..1 arguments, returns std::string.
// ELF isObjectActive 0x475530, executeObjectMethod 0x4758D0,
// setArgFromString 0x475650, getReturnValueAsString 0x4752F0, D1 0x475280, D0 0x475210.
// argc 0: dice::hfe::ai::AIPathfinding::getActiveMap() const (slot +0x1C at 0x47597A; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::setActiveMap(std::string const&) (slot +0x18 at 0x475933; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass498
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("activeMap", 1, 0, 1, "std::string", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfindingSetup::instance->getActiveMap();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            IAIPathfindingSetup::instance->setActiveMap(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject498;

// ConsoleClass499 AIPathFinding.traceMap: property, 0..1 arguments, returns std::string.
// ELF isObjectActive 0x475CB0, executeObjectMethod 0x476050,
// setArgFromString 0x475DD0, getReturnValueAsString 0x475A70, D1 0x475A00, D0 0x475990.
// argc 0: dice::hfe::ai::AIPathfinding::getTraceMap() const (slot +0x24 at 0x4760FA; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::setTraceMap(std::string const&) (slot +0x20 at 0x4760B3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass499
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("traceMap", 1, 0, 1, "std::string", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfindingSetup::instance->getTraceMap();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            IAIPathfindingSetup::instance->setTraceMap(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject499;

// ConsoleClass500 AIPathFinding.getMapList: method, 0..0 arguments, returns std::string.
// ELF isObjectActive 0x476430, executeObjectMethod 0x476560,
// setArgFromString 0x476550, getReturnValueAsString 0x4761F0, D1 0x476180, D0 0x476110.
// argc 0: dice::hfe::ai::AIPathfinding::getMapList() (slot +0x3C at 0x4765A5; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass500
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("getMapList", 0, 0, 0, "std::string", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfindingSetup::instance->getMapList();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject500;

// ConsoleClass502 AIPathFinding.map.maxSlope: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x4768A0, executeObjectMethod 0x476C10,
// setArgFromString 0x4769D0, getReturnValueAsString 0x476660, D1 0x476640, D0 0x476600.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::getMaxSlope() const (slot +0x1C at 0x476C78; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::MapContainer::setMaxSlope(float) (slot +0x20 at 0x476C60; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass502
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.maxSlope", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveMapContainer()->getMaxSlope();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->setMaxSlope(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject502;

// ConsoleClass503 AIPathFinding.map.headClearance: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x476F30, executeObjectMethod 0x4772A0,
// setArgFromString 0x477060, getReturnValueAsString 0x476CF0, D1 0x476CD0, D0 0x476C90.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::getHeadClearance() const (slot +0x28 at 0x477308; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::MapContainer::setHeadClearance(float) (slot +0x24 at 0x4772F0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass503
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.headClearance", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveMapContainer()->getHeadClearance();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->setHeadClearance(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject503;

// ConsoleClass504 AIPathFinding.map.radius: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x4775C0, executeObjectMethod 0x477930,
// setArgFromString 0x4776F0, getReturnValueAsString 0x477380, D1 0x477360, D0 0x477320.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::getRadius() const (slot +0x30 at 0x477998; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::MapContainer::setRadius(float) (slot +0x2C at 0x477980; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass504
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.radius", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveMapContainer()->getRadius();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->setRadius(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject504;

// ConsoleClass505 AIPathFinding.map.allowedHeightDiff: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x477C50, executeObjectMethod 0x477FC0,
// setArgFromString 0x477D80, getReturnValueAsString 0x477A10, D1 0x4779F0, D0 0x4779B0.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::getAllowedHeightDiff() const (slot +0x38 at 0x478028; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::MapContainer::setAllowedHeightDiff(float) (slot +0x34 at 0x478010; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass505
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.allowedHeightDiff", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveMapContainer()->getAllowedHeightDiff();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->setAllowedHeightDiff(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject505;

// ConsoleClass506 AIPathFinding.map.minPolygonsInLeaf: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x478310, executeObjectMethod 0x478680,
// setArgFromString 0x478440, getReturnValueAsString 0x4780A0, D1 0x478080, D0 0x478040.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::getMinPolygonsInLeaf() const (slot +0xC at 0x4786E8; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::MapContainer::setMinPolygonsInLeaf(int) (slot +0x10 at 0x4786D0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass506
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.minPolygonsInLeaf", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveMapContainer()->getMinPolygonsInLeaf();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->setMinPolygonsInLeaf(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject506;

// ConsoleClass507 AIPathFinding.map.maxDepthInTree: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x4789D0, executeObjectMethod 0x478D40,
// setArgFromString 0x478B00, getReturnValueAsString 0x478760, D1 0x478740, D0 0x478700.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::getMaxDepthInTree() const (slot +0x14 at 0x478DA8; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::MapContainer::setMaxDepthInTree(int) (slot +0x18 at 0x478D90; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass507
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.maxDepthInTree", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveMapContainer()->getMaxDepthInTree();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->setMaxDepthInTree(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject507;

// ConsoleClass509 AIPathFinding.map.addVehicleForClusterCost: method, 1..1 arguments, no result.
// ELF isObjectActive 0x478E60, executeObjectMethod 0x479210,
// setArgFromString 0x478F90, getReturnValueAsString 0x478E20, D1 0x478E00, D0 0x478DC0.
// argc 1: dice::hfe::ai::AIPathfinding::MapContainer::addVehicleForClusterCost(std::string const&) (slot +0x70 at 0x47926C; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass509
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.addVehicleForClusterCost", 0, 1, 1, "void", false)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->addVehicleForClusterCost(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject509;

// ConsoleClass510 AIPathFinding.map.draw: method, 0..0 arguments, no result.
// ELF isObjectActive 0x479340, executeObjectMethod 0x479480,
// setArgFromString 0x479470, getReturnValueAsString 0x479300, D1 0x4792E0, D0 0x4792A0.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::draw() const (slot +0x3C at 0x4794B8; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass510
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.draw", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->draw();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject510;

// ConsoleClass511 AIPathFinding.map.undraw: method, 0..0 arguments, no result.
// ELF isObjectActive 0x479560, executeObjectMethod 0x4796A0,
// setArgFromString 0x479690, getReturnValueAsString 0x479520, D1 0x479500, D0 0x4794C0.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::undraw() const (slot +0x40 at 0x4796D8; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass511
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.undraw", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->undraw();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject511;

// ConsoleClass512 AIPathFinding.map.drawCluster: method, 0..0 arguments, no result.
// ELF isObjectActive 0x479780, executeObjectMethod 0x4798C0,
// setArgFromString 0x4798B0, getReturnValueAsString 0x479740, D1 0x479720, D0 0x4796E0.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::drawCluster() const (slot +0x44 at 0x4798F8; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass512
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.drawCluster", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->drawCluster();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject512;

// ConsoleClass513 AIPathFinding.map.undrawCluster: method, 0..0 arguments, no result.
// ELF isObjectActive 0x4799A0, executeObjectMethod 0x479AE0,
// setArgFromString 0x479AD0, getReturnValueAsString 0x479960, D1 0x479940, D0 0x479900.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::undrawCluster() const (slot +0x48 at 0x479B18; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass513
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.undrawCluster", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->undrawCluster();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject513;

// ConsoleClass514 AIPathFinding.map.drawClusterNeighbourIndices: method, 1..1 arguments, no result.
// ELF isObjectActive 0x479BC0, executeObjectMethod 0x479FD0,
// setArgFromString 0x479CF0, getReturnValueAsString 0x479B80, D1 0x479B60, D0 0x479B20.
// argc 1: dice::hfe::ai::AIPathfinding::MapContainer::drawClusterNeighbourIndices(dice::hfe::Vec3 const&) const (slot +0x4C at 0x47A02E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass514
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.drawClusterNeighbourIndices", 0, 1, 1, "void", false)
            m_argTypes[0] = "Vec3";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->drawClusterNeighbourIndices(BF2_CC_ARG(Vec3, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec3, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject514;

// ConsoleClass516 AIPathFinding.map.buildDebugMeshes: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47A0E0, executeObjectMethod 0x47A220,
// setArgFromString 0x47A210, getReturnValueAsString 0x47A0A0, D1 0x47A080, D0 0x47A040.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::buildDebugMeshes() (slot +0x54 at 0x47A258; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass516
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.buildDebugMeshes", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->buildDebugMeshes();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject516;

// ConsoleClass517 AIPathFinding.map.buildDebugClusterMeshes: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47A300, executeObjectMethod 0x47A440,
// setArgFromString 0x47A430, getReturnValueAsString 0x47A2C0, D1 0x47A2A0, D0 0x47A260.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::buildDebugClusterMeshes() (slot +0x5C at 0x47A478; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass517
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.buildDebugClusterMeshes", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->buildDebugClusterMeshes();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject517;

// ConsoleClass518 AIPathFinding.map.loadGTSDebugMeshes: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47A520, executeObjectMethod 0x47A660,
// setArgFromString 0x47A650, getReturnValueAsString 0x47A4E0, D1 0x47A4C0, D0 0x47A480.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::loadGTSDebugMeshes() (slot +0x50 at 0x47A698; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass518
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.loadGTSDebugMeshes", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->loadGTSDebugMeshes();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject518;

// ConsoleClass519 AIPathFinding.map.unloadDebugMeshes: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47A740, executeObjectMethod 0x47A880,
// setArgFromString 0x47A870, getReturnValueAsString 0x47A700, D1 0x47A6E0, D0 0x47A6A0.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::unloadDebugMeshes() (slot +0x58 at 0x47A8B8; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass519
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.unloadDebugMeshes", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->unloadDebugMeshes();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject519;

// ConsoleClass520 AIPathFinding.map.unloadDebugClusterMeshes: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47A960, executeObjectMethod 0x47AAA0,
// setArgFromString 0x47AA90, getReturnValueAsString 0x47A920, D1 0x47A900, D0 0x47A8C0.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::unloadDebugClusterMeshes() (slot +0x60 at 0x47AAD8; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass520
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.unloadDebugClusterMeshes", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->unloadDebugClusterMeshes();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject520;

// ConsoleClass521 AIPathFinding.map.recalculatePortalCosts: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47AB80, executeObjectMethod 0x47ACC0,
// setArgFromString 0x47ACB0, getReturnValueAsString 0x47AB40, D1 0x47AB20, D0 0x47AAE0.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::recalculatePortalCosts() (slot +0x74 at 0x47ACF8; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass521
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.recalculatePortalCosts", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->recalculatePortalCosts();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject521;

// ConsoleClass524 AIPathFinding.map.loadQuadTrees: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47ADA0, executeObjectMethod 0x47AEE0,
// setArgFromString 0x47AED0, getReturnValueAsString 0x47AD60, D1 0x47AD40, D0 0x47AD00.
// argc 0: dice::hfe::ai::AIPathfinding::MapContainer::loadQuadTrees() (slot +0x68 at 0x47AF18; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass524
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("map.loadQuadTrees", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return getActiveMapContainer() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            getActiveMapContainer()->loadQuadTrees();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject524;

// ConsoleClass525 AIPathFinding.saveQuadTrees: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47AFC0, executeObjectMethod 0x47B0F0,
// setArgFromString 0x47B0E0, getReturnValueAsString 0x47AF80, D1 0x47AF60, D0 0x47AF20.
// argc 0: dice::hfe::ai::AIPathfinding::saveQuadTrees() (slot +0x28 at 0x47B12B; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass525
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("saveQuadTrees", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            IAIPathfindingSetup::instance->saveQuadTrees();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject525;

// ConsoleClass526 AIPathFinding.loadQuadTrees: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47B1D0, executeObjectMethod 0x47B300,
// setArgFromString 0x47B2F0, getReturnValueAsString 0x47B190, D1 0x47B170, D0 0x47B130.
// argc 0: dice::hfe::ai::AIPathfinding::loadQuadTrees() (slot +0x2C at 0x47B33B; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass526
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("loadQuadTrees", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            IAIPathfindingSetup::instance->loadQuadTrees();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject526;

// ConsoleClass527 AIPathFinding.loadGTSData: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47B3E0, executeObjectMethod 0x47B510,
// setArgFromString 0x47B500, getReturnValueAsString 0x47B3A0, D1 0x47B380, D0 0x47B340.
// argc 0: dice::hfe::ai::AIPathfinding::loadGTSData() (slot +0x30 at 0x47B54B; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass527
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("loadGTSData", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            IAIPathfindingSetup::instance->loadGTSData();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject527;

// ConsoleClass528 AIPathFinding.exportGTSMeshes: method, 0..0 arguments, returns bool.
// ELF isObjectActive 0x47B7F0, executeObjectMethod 0x47B920,
// setArgFromString 0x47B910, getReturnValueAsString 0x47B5B0, D1 0x47B590, D0 0x47B550.
// argc 0: dice::hfe::ai::AIPathfinding::exportGTSMeshes() const (slot +0x34 at 0x47B955; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass528
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("exportGTSMeshes", 0, 0, 0, "bool", true)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfindingSetup::instance->exportGTSMeshes();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject528;

// ConsoleClass529 AIPathFinding.exportGTSMeshesWithinBoundary: method, 2..2 arguments, returns bool.
// ELF isObjectActive 0x47BC20, executeObjectMethod 0x47C280,
// setArgFromString 0x47BD40, getReturnValueAsString 0x47B9E0, D1 0x47B9C0, D0 0x47B980.
// argc 2: dice::hfe::ai::AIPathfinding::exportGTSMeshesWithinBoundary(dice::hfe::Vec2 const&, dice::hfe::Vec2 const&) const (slot +0x38 at 0x47C2E1; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass529
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("exportGTSMeshesWithinBoundary", 0, 2, 2, "bool", true)
            m_argTypes[0] = "Vec2";
            m_argTypes[1] = "Vec2";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfindingSetup::instance->exportGTSMeshesWithinBoundary(BF2_CC_ARG(Vec2, 0), BF2_CC_ARG(Vec2, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec2, 1)
        AIMODULE_CC_EXTRACT(Vec2, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject529;

// ConsoleClass530 AIPathFinding.maxWaterDepth: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x47C5B0, executeObjectMethod 0x47C910,
// setArgFromString 0x47C6D0, getReturnValueAsString 0x47C370, D1 0x47C350, D0 0x47C310.
// argc 0: dice::hfe::ai::AIPathfinding::getMaxWaterDepth() const (slot +0x44 at 0x47C978; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::setMaxWaterDepth(float) (slot +0x40 at 0x47C95D; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass530
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("maxWaterDepth", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfindingSetup::instance->getMaxWaterDepth();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            IAIPathfindingSetup::instance->setMaxWaterDepth(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject530;

// ConsoleClass531 AIPathFinding.drawDynamicAIObjects: method, 0..0 arguments, no result.
// ELF isObjectActive 0x47CA30, executeObjectMethod 0x47CB60,
// setArgFromString 0x47CB50, getReturnValueAsString 0x47C9F0, D1 0x47C9D0, D0 0x47C990.
// argc 0: dice::hfe::ai::AIPathfinding::drawDynamicAIObjects() const (slot +0x48 at 0x47CB9B; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass531
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("drawDynamicAIObjects", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return IAIPathfindingSetup::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            IAIPathfindingSetup::instance->drawDynamicAIObjects();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject531;

SetConsoleClassDefaults AIDebugfullAccessSetConsoleClassDefaults("AIDebug", 2, 0);

// ConsoleClass537 AIDebug.textGamma: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x47CE40, executeObjectMethod 0x47D1A0,
// setArgFromString 0x47CF60, getReturnValueAsString 0x47CC00, D1 0x47CBE0, D0 0x47CBA0.
// argc 0: dice::hfe::ai::DPManager::getTextGamma() const (slot +0x58 at 0x47D208; ledger done)
// argc 1: dice::hfe::ai::DPManager::setTextGamma(float) (slot +0x54 at 0x47D1ED; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass537
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("textGamma", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->getTextGamma();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->setTextGamma(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject537;

// ConsoleClass538 AIDebug.draw: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x47D4C0, executeObjectMethod 0x47D820,
// setArgFromString 0x47D5E0, getReturnValueAsString 0x47D280, D1 0x47D260, D0 0x47D220.
// argc 0: dice::hfe::ai::AISettings::getDraw() const (slot +0x80 at 0x47D894; ledger done)
// argc 1: dice::hfe::ai::AISettings::setDraw(bool) (slot +0x138 at 0x47D875; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass538
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("draw", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings) != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getDraw();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            static_cast<AISettings*>(static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings))->setDraw(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject538;

// ConsoleClass539 AIDebug.ToolPathfinding: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x47DB50, executeObjectMethod 0x47DEB0,
// setArgFromString 0x47DC70, getReturnValueAsString 0x47D910, D1 0x47D8F0, D0 0x47D8B0.
// argc 0: dice::hfe::ai::AIPathfinding::getActivatePathfindingDebugTool() const (slot +0x108 at 0x47DF24; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::setActivatePathfindingDebugTool(bool) (slot +0x104 at 0x47DF05; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass539
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("ToolPathfinding", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IAIPathfinding::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfinding::instance->getActivatePathfindingDebugTool();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            IAIPathfinding::instance->setActivatePathfindingDebugTool(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject539;

// ConsoleClass540 AIDebug.pathfindingDebugToolVehicle: property, 0..1 arguments, returns std::string.
// ELF isObjectActive 0x47E260, executeObjectMethod 0x47E600,
// setArgFromString 0x47E380, getReturnValueAsString 0x47E020, D1 0x47DFB0, D0 0x47DF40.
// argc 0: dice::hfe::ai::AIPathfinding::getPathfindingDebugToolVehicle() const (slot +0x110 at 0x47E6AA; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::setPathfindingDebugToolVehicle(std::string const&) (slot +0x10C at 0x47E663; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass540
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("pathfindingDebugToolVehicle", 1, 0, 1, "std::string", true)
            m_argTypes[0] = "std::string";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IAIPathfinding::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfinding::instance->getPathfindingDebugToolVehicle();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            IAIPathfinding::instance->setPathfindingDebugToolVehicle(BF2_CC_ARG(std::string, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
BF2_CC_NAME consoleObject540;

// ConsoleClass541 AIDebug.clusterSearch: property, 0..1 arguments, returns bool.
// ELF isObjectActive 0x47E970, executeObjectMethod 0x47ECD0,
// setArgFromString 0x47EA90, getReturnValueAsString 0x47E730, D1 0x47E710, D0 0x47E6D0.
// argc 0: dice::hfe::ai::AIPathfinding::getClusterSearch() const (slot +0x100 at 0x47ED44; ledger done)
// argc 1: dice::hfe::ai::AIPathfinding::setClusterSearch(bool) (slot +0xFC at 0x47ED25; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass541
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("clusterSearch", 1, 0, 1, "bool", true)
            m_argTypes[0] = "bool";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IAIPathfinding::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = IAIPathfinding::instance->getClusterSearch();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            IAIPathfinding::instance->setClusterSearch(BF2_CC_ARG(bool, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject541;

// ConsoleClass542 AIDebug.setPathfindingDebugToolVehicleId: method, 1..1 arguments, no result.
// ELF isObjectActive 0x47EE00, executeObjectMethod 0x47F160,
// setArgFromString 0x47EF20, getReturnValueAsString 0x47EDC0, D1 0x47EDA0, D0 0x47ED60.
// argc 1: dice::hfe::ai::AIPathfinding::setPathfindingDebugToolVehicleId(int) (slot +0x114 at 0x47F1A3; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass542
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("setPathfindingDebugToolVehicleId", 0, 1, 1, "void", false)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return IAIPathfinding::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            IAIPathfinding::instance->setPathfindingDebugToolVehicleId(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject542;

// ConsoleClass543 AIDebug.tryPath: method, 2..2 arguments, no result.
// ELF isObjectActive 0x47F250, executeObjectMethod 0x47F930,
// setArgFromString 0x47F370, getReturnValueAsString 0x47F210, D1 0x47F1F0, D0 0x47F1B0.
// argc 2: dice::hfe::ai::AIPathfinding::tryPath(dice::hfe::Vec3 const&, dice::hfe::Vec3 const&) (slot +0xF4 at 0x47F9B2; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass543
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("tryPath", 0, 2, 2, "void", false)
            m_argTypes[0] = "Vec3";
            m_argTypes[1] = "Vec3";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return IAIPathfinding::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            IAIPathfinding::instance->tryPath(BF2_CC_ARG(Vec3, 0), BF2_CC_ARG(Vec3, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec3, 1)
        AIMODULE_CC_EXTRACT(Vec3, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject543;

// ConsoleClass544 AIDebug.tryClusterPath: method, 2..2 arguments, no result.
// ELF isObjectActive 0x47FA60, executeObjectMethod 0x480140,
// setArgFromString 0x47FB80, getReturnValueAsString 0x47FA20, D1 0x47FA00, D0 0x47F9C0.
// argc 2: dice::hfe::ai::AIPathfinding::tryClusterPath(dice::hfe::Vec3 const&, dice::hfe::Vec3 const&) (slot +0xF8 at 0x4801C2; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass544
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("tryClusterPath", 0, 2, 2, "void", false)
            m_argTypes[0] = "Vec3";
            m_argTypes[1] = "Vec3";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return IAIPathfinding::instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            IAIPathfinding::instance->tryClusterPath(BF2_CC_ARG(Vec3, 0), BF2_CC_ARG(Vec3, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec3, 1)
        AIMODULE_CC_EXTRACT(Vec3, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject544;

SetConsoleClassDefaults AIOrderfullAccessSetConsoleClassDefaults("AIOrder", 2, 0);

// ConsoleClass551 AIOrder.createPatrolPath: method, 3..3 arguments, no result.
// ELF isObjectActive 0x480270, executeObjectMethod 0x480A50,
// setArgFromString 0x480390, getReturnValueAsString 0x480230, D1 0x480210, D0 0x4801D0.
// argc 3: dice::hfe::ai::UserOrderManager::createPatrolPath(std::string const&, dice::hfe::ai::CombatStrengthType, int) (call at 0x480ABF; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass551
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createPatrolPath", 0, 3, 3, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "CombatStrengthType";
            m_argTypes[2] = "int";
    AIMODULE_CC_TAIL(3)
    virtual bool isObjectActive() const { return UserOrderManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 3)
        {
            m_hasReturnValue = false;
            UserOrderManager::m_instance->createPatrolPath(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(CombatStrengthType, 1), BF2_CC_ARG(int, 2));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT_WITH(CombatStrengthType, 2, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT(int, 3)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject551;

// ConsoleClass553 AIOrder.addPathPoint: method, 1..1 arguments, no result.
// ELF isObjectActive 0x480B90, executeObjectMethod 0x480F60,
// setArgFromString 0x480CC0, getReturnValueAsString 0x480B50, D1 0x480B30, D0 0x480AF0.
// argc 1: dice::hfe::ai::PatrolPath::addPathPoint(dice::hfe::Vec2 const&) (call at 0x480FB4; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass553
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addPathPoint", 0, 1, 1, "void", false)
            m_argTypes[0] = "Vec2";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActivePatrolPathIfManager() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActivePatrolPathIfManager()->addPathPoint(BF2_CC_ARG(Vec2, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec2, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject553;

// ConsoleClass554 AIOrder.requiredResources: property, 0..1 arguments, returns int.
// ELF isObjectActive 0x481290, executeObjectMethod 0x481600,
// setArgFromString 0x4813C0, getReturnValueAsString 0x481020, D1 0x481000, D0 0x480FC0.
// argc 0: dice::hfe::ai::PatrolPath::getRequiredResources() const (call at 0x481666; ledger done)
// argc 1: dice::hfe::ai::PatrolPath::setRequiredResources(int) (call at 0x48164E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass554
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("requiredResources", 1, 0, 1, "int", true)
            m_argTypes[0] = "int";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActivePatrolPathIfManager() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActivePatrolPathIfManager()->getRequiredResources();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActivePatrolPathIfManager()->setRequiredResources(BF2_CC_ARG(int, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    int m_returnValue;
};
BF2_CC_NAME consoleObject554;

// ConsoleClass555 AIOrder.minDistance: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x481920, executeObjectMethod 0x481C90,
// setArgFromString 0x481A50, getReturnValueAsString 0x4816E0, D1 0x4816C0, D0 0x481680.
// argc 0: dice::hfe::ai::PatrolPath::getMinDistance() const (call at 0x481CF6; ledger done)
// argc 1: dice::hfe::ai::PatrolPath::setMinDistance(float) (call at 0x481CDE; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass555
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("minDistance", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActivePatrolPathIfManager() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActivePatrolPathIfManager()->getMinDistance();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActivePatrolPathIfManager()->setMinDistance(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject555;

// ConsoleClass556 AIOrder.patrolSpeed: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x481FB0, executeObjectMethod 0x482320,
// setArgFromString 0x4820E0, getReturnValueAsString 0x481D70, D1 0x481D50, D0 0x481D10.
// argc 0: dice::hfe::ai::PatrolPath::getPatrolSpeed() const (call at 0x482386; ledger done)
// argc 1: dice::hfe::ai::PatrolPath::setPatrolSpeed(float) (call at 0x48236E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass556
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("patrolSpeed", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActivePatrolPathIfManager() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActivePatrolPathIfManager()->getPatrolSpeed();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActivePatrolPathIfManager()->setPatrolSpeed(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject556;

// ConsoleClass559 AIOrder.createGuardPosition: method, 5..5 arguments, no result.
// ELF isObjectActive 0x482440, executeObjectMethod 0x483100,
// setArgFromString 0x482560, getReturnValueAsString 0x482400, D1 0x4823E0, D0 0x4823A0.
// argc 5: dice::hfe::ai::UserOrderManager::createGuardPosition(std::string const&, dice::hfe::Vec2 const&, dice::hfe::ai::CombatStrengthType, int, int) (call at 0x483199; ledger stop)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass559
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createGuardPosition", 0, 5, 5, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "Vec2";
            m_argTypes[2] = "CombatStrengthType";
            m_argTypes[3] = "int";
            m_argTypes[4] = "int";
    AIMODULE_CC_TAIL(5)
    virtual bool isObjectActive() const { return UserOrderManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 5)
        {
            m_hasReturnValue = false;
            UserOrderManager::m_instance->createGuardPosition(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(Vec2, 1), BF2_CC_ARG(CombatStrengthType, 2), BF2_CC_ARG(int, 3), BF2_CC_ARG(int, 4));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT(Vec2, 2)
        AIMODULE_CC_EXTRACT_WITH(CombatStrengthType, 3, dice::hfe::ai::operator>>)
        AIMODULE_CC_EXTRACT(int, 4)
        AIMODULE_CC_EXTRACT(int, 5)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject559;

// ConsoleClass560 AIOrder.minDistance: property, 0..1 arguments, returns float.
// ELF isObjectActive 0x483470, executeObjectMethod 0x4837E0,
// setArgFromString 0x4835A0, getReturnValueAsString 0x483230, D1 0x483210, D0 0x4831D0.
// argc 0: dice::hfe::ai::GuardPosition::getMinDistance() const (call at 0x483846; ledger done)
// argc 1: dice::hfe::ai::GuardPosition::setMinDistance(float) (call at 0x48382E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass560
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("minDistance", 1, 0, 1, "float", true)
            m_argTypes[0] = "float";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveGuardPositionIfManager() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveGuardPositionIfManager()->getMinDistance();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveGuardPositionIfManager()->setMinDistance(BF2_CC_ARG(float, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(float, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
BF2_CC_NAME consoleObject560;

// ConsoleClass561 AIOrder.rotation: property, 0..1 arguments, returns Vec3.
// ELF isObjectActive 0x483B60, executeObjectMethod 0x483F70,
// setArgFromString 0x483C90, getReturnValueAsString 0x4838C0, D1 0x4838A0, D0 0x483860.
// argc 0: dice::hfe::ai::GuardPosition::getRotation() const (call at 0x483FF5; ledger done)
// argc 1: dice::hfe::ai::GuardPosition::setRotation(dice::hfe::Vec3 const&) (call at 0x483FD0; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass561
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("rotation", 1, 0, 1, "Vec3", true)
            m_argTypes[0] = "Vec3";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return getActiveGuardPositionIfManager() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = getActiveGuardPositionIfManager()->getRotation();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            getActiveGuardPositionIfManager()->setRotation(BF2_CC_ARG(Vec3, 0));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec3, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    Vec3 m_returnValue;
};
BF2_CC_NAME consoleObject561;

// ConsoleClass562 AIOrder.addStrategicAreaAbortCondition: method, 2..2 arguments, no result.
// ELF isObjectActive 0x4840C0, executeObjectMethod 0x4846A0,
// setArgFromString 0x4841F0, getReturnValueAsString 0x484080, D1 0x484060, D0 0x484020.
// argc 2: dice::hfe::ai::GuardPosition::addStrategicAreaAbortCondition(std::string const&, dice::hfe::ai::StatusMap::Status) (call at 0x484709; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass562
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("addStrategicAreaAbortCondition", 0, 2, 2, "void", false)
            m_argTypes[0] = "std::string";
            m_argTypes[1] = "StatusMap::Status";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return getActiveGuardPositionIfManager() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            getActiveGuardPositionIfManager()->addStrategicAreaAbortCondition(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(StatusMap::Status, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(std::string, 1)
        AIMODULE_CC_EXTRACT_WITH(StatusMap::Status, 2, dice::hfe::ai::operator>>)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject562;

SetConsoleClassDefaults AICoverfullAccessSetConsoleClassDefaults("AICover", 2, 0);

// ConsoleClass569 AICover.createCoverPosition: method, 2..2 arguments, no result.
// ELF isObjectActive 0x4847D0, executeObjectMethod 0x484E30,
// setArgFromString 0x4848F0, getReturnValueAsString 0x484790, D1 0x484770, D0 0x484730.
// argc 2: dice::hfe::ai::CoverPositionManager::createCoverPosition(dice::hfe::Vec2 const&, dice::hfe::Vec2 const&) (call at 0x484E9E; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass569
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("createCoverPosition", 0, 2, 2, "void", false)
            m_argTypes[0] = "Vec2";
            m_argTypes[1] = "Vec2";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return CoverPositionManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = false;
            CoverPositionManager::m_instance->createCoverPosition(BF2_CC_ARG(Vec2, 0), BF2_CC_ARG(Vec2, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(Vec2, 1)
        AIMODULE_CC_EXTRACT(Vec2, 2)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject569;

// ConsoleClass570 AICover.finaliseCoverPositions: method, 0..0 arguments, no result.
// ELF isObjectActive 0x484F50, executeObjectMethod 0x485080,
// setArgFromString 0x485070, getReturnValueAsString 0x484F10, D1 0x484EF0, D0 0x484EB0.
// argc 0: dice::hfe::ai::CoverPositionManager::finaliseCoverPositions() (call at 0x4850B9; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass570
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("finaliseCoverPositions", 0, 0, 0, "void", false)
    AIMODULE_CC_TAIL(0)
    virtual bool isObjectActive() const { return CoverPositionManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = false;
            CoverPositionManager::m_instance->finaliseCoverPositions();
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
BF2_CC_NAME consoleObject570;

// ConsoleClass571 AICover.search: method, 2..2 arguments, returns bool.
// ELF isObjectActive 0x485360, executeObjectMethod 0x485900,
// setArgFromString 0x485480, getReturnValueAsString 0x485120, D1 0x485100, D0 0x4850C0.
// argc 2: dice::hfe::ai::CoverPositionManager::search(int, int) (call at 0x485944; ledger done)
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass571
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("search", 0, 2, 2, "bool", true)
            m_argTypes[0] = "int";
            m_argTypes[1] = "int";
    AIMODULE_CC_TAIL(2)
    virtual bool isObjectActive() const { return CoverPositionManager::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = CoverPositionManager::m_instance->search(BF2_CC_ARG(int, 0), BF2_CC_ARG(int, 1));
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(int, 1)
        AIMODULE_CC_EXTRACT(int, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
BF2_CC_NAME consoleObject571;

SetConsoleClassDefaults AIRandomfullAccessSetConsoleClassDefaults("AIRandom", 2, 0);

// ConsoleClass576 AIRandom.seed: property, 0..1 arguments, returns u32.
// ELF isObjectActive 0x485C10, executeObjectMethod 0x485F70,
// setArgFromString 0x485D30, getReturnValueAsString 0x4859D0, D1 0x4859B0, D0 0x485970.
// argc 0: verified inline body, seed word at offset zero of a non-polymorphic receiver
// argc 1: verified inline body, seed word at offset zero of a non-polymorphic receiver
#undef BF2_CC_NAME
#define BF2_CC_NAME ConsoleClass576
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl
{
    BF2_CC_HEAD_NAMED("seed", 1, 0, 1, "u32", true)
            m_argTypes[0] = "u32";
    AIMODULE_CC_TAIL(1)
    virtual bool isObjectActive() const { return AIRandom::m_instance != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = AIRandom::m_instance->m_seed;
        }
        else if (m_argCount == 1)
        {
            const u32 value = BF2_CC_ARG(u32, 0);
            m_hasReturnValue = false;
            AIRandom::m_instance->m_seed = value;
        }
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        AIMODULE_CC_EXTRACT(u32, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    u32 m_returnValue;
};
BF2_CC_NAME consoleObject576;

#undef AIMODULE_CC_TAIL
#undef AIMODULE_CC_EXTRACT_WITH
#undef AIMODULE_CC_EXTRACT
#pragma pop_macro("BF2_CC_NAME")
