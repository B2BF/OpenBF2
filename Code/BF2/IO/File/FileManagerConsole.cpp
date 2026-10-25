// Console bindings for file and settings operations. [CHOSEN] Source placement.
#include "../../Core/ConsoleObjectMacro.h"
#include "FileManager.h"
#include "../SettingsManager.h"
#include "../Console/Console.h"              // io::mainConsole - console.allowMultipleFileLoad's receiver
#include "../../Core/SettingsRepostitory.h"
#include "../../Core/EventManager.h"
#include "../../Core/StringUtil.h"           // compareIgnoreCase - the Console reflection rows
#include <cstdio>                           // sprintf - Console_getEnumName
#include <string>

namespace dice {
namespace hfe {
namespace io {

namespace {

// Stands in for the original's file-scope list. ModuleIO::init registers it.
const char* const kFileConsoleTag = "io.FileManager";

// Stores a registered string setting and posts its change event only when the value differs.
bool consoleSettingsStringSet(std::string& name, const std::string& value)
{
    if (settingsManager->m_registeredVars.find(name) == settingsManager->m_registeredVars.end())
        return false;
    const bool changed = settings->stringGet(name, std::string("UnSet")) != value;
    settings->stringSet(name, value);
    if (changed && eventManager)
        eventManager->postEvent(EventCategory(5), 1, &name, 0.0f);
    return true;
}

// Mounts the named archive at mountPath, defaulting contentCheck to true. The console binding discards the result.
void consoleMountArchive(const std::string& archiveName, const std::string& mountPath,
                         bool contentCheck = true)
{
    if (fileManager)
        fileManager->mountArchive(archiveName, mountPath, contentCheck);
}

} // namespace

// Console registry-reflection bindings. They read mainConsoleObjects and return strings.
// getActiveMethods lists active rows for an object or component prefix; isSetGet reports properties.
// Preserve argument counts/types and register this Console block before the other console families.
// Bindings that require activeMethodConsole remain unpublished until its dispatch-time assignment
// exists; publishing those executors against the current null pointer would fault.

// The helpers (external linkage in retail: _ZN4dice3hfe2io...Console_<name>...)
namespace {

// [CHOSEN] Returns the part of name before its first dot, or the whole name when no dot exists.
std::string consoleNameBeforeDot(const std::string& name)
{
    std::string part(name);
    const std::string::size_type dot = part.find(".");
    if (dot != std::string::npos)
        part = part.substr(0, dot);
    return part;
}

// The member part: everything after the first '.', or "" (dedicated 0x640DDF, BF2.exe 0x68EFED).
// [CHOSEN] name.
std::string consoleNameAfterDot(const std::string& name)
{
    std::string part;
    const std::string::size_type dot = name.find(".");
    if (dot != std::string::npos)
        part = name.substr(dot + 1, name.length() - dot - 1);
    return part;
}

const char* const kConsoleInvalidName  = "ERROR: Objectname or/and functionname are invalide";
// hasRange and getCategory spell it without the final 'e' - in all three images.
const char* const kConsoleInvalidName2 = "ERROR: Objectname or/and functionname are invalid";

} // namespace

// getMaxNoArgs (+0x2C) argument types, space-separated (getArgType +0x24). A negative count still
// prints argType 0: the shipped test is `count != 0`, not `count > 0`.
std::string Console_getArgumentTypes(const std::string& objectName, const std::string& methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName);
    const int count = object->getMaxNoArgs();
    if (count)
    {
        result.append(object->getArgType(0));
        for (int i = 1; i < count; ++i)
        {
            result.append(" ");
            result.append(object->getArgType(i));
        }
    }
    return result;
}

// Returns an enum value's display name by temporarily setting the console property and restoring its text value.
// The query invokes the property and may therefore trigger setter side effects.
std::string Console_getEnumName(const std::string& objectName, const std::string& methodName, int value)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName);
    std::string args[1];
    const std::string original = object->execute(args, 0);
    std::string previous("");                       // a dead copy of the saved value, as shipped
    previous = original;
    char buffer[112];                               // ELF 112 B; both PEs reserve 100
    std::sprintf(buffer, "%d", value);
    args[0] = buffer;
    object->execute(args, 1);
    result = object->execute(args, 0);
    args[0] = original;
    object->execute(args, 1);
    return result;
}

// getRangeAsString (+0x3C): "<min> <max>" - " " for a row with no range.
std::string Console_getRange(const std::string& objectName, const std::string& methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName);
    const std::pair<std::string, std::string> range = object->getRangeAsString();
    result.append(range.first);
    result.append(" ");
    result.append(range.second);
    return result;
}

// getDeltaAsString (+0x40).
std::string Console_getDelta(const std::string& objectName, const std::string& methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName);
    result = object->getDeltaAsString();
    return result;
}

// hasBundledType (+0x50): "1" / "0".
std::string Console_hasBundledType(const std::string& objectName, const std::string& methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName);
    result.append(object->hasBundledType() ? "1" : "0");
    return result;
}

// getTypeDescription (+0x54).
std::string Console_getTypeDescription(const std::string& objectName, const std::string& methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName);
    result = object->getTypeDescription();
    return result;
}

// getReturnType (+0x20).
std::string Console_getReturnType(const std::string& objectName, const std::string& methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName);
    result.append(object->getReturnType());
    return result;
}

// isDynamic (+0x48): "1" / "0". This helper and the next three take the member name by value
// (mangled `...ERKSsSs`).
std::string Console_isDynamic(const std::string& objectName, std::string methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName);
    result.append(object->isDynamic() ? "1" : "0");
    return result;
}

// isSetGet (+0x4C): "1" for a set/get property (type 1), "0" otherwise.
std::string Console_isSetGet(const std::string& objectName, std::string methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName);
    result.append(object->isSetGet() ? "1" : "0");
    return result;
}

// hasRange (+0x34): "1" / "0".
std::string Console_hasRange(const std::string& objectName, std::string methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName2);
    result.append(object->hasRange() ? "1" : "0");
    return result;
}

// getCategory (+0x1C, an unsigned char): the name of the binding's defaults-block category. This
// table is what names the category ids the generated blocks carry (ObjectTemplate (2, 11) = Flags,
// (2, 18) = Light, (2, 24) = WindAffector, ...).
std::string Console_getCategory(const std::string& objectName, std::string methodName)
{
    std::string result;
    ConsoleObject* object = mainConsoleObjects->getConsoleObject(objectName, methodName);
    if (!object)
        return std::string(kConsoleInvalidName2);
    switch (object->getCategory())
    {
    case 0:  result.append("Default"); break;
    case 1:  result.append("Physics"); break;
    case 2:  result.append("Damage"); break;
    case 3:  result.append("ResourceTree"); break;
    case 4:  result.append("SourceControl"); break;
    case 5:  result.append("Movement"); break;
    case 6:  result.append("Output"); break;
    case 7:  result.append("Rotation"); break;
    case 8:  result.append("Camera"); break;
    case 9:  result.append("Hud"); break;
    case 10: result.append("Ammo"); break;
    case 11: result.append("Flags"); break;
    case 12: result.append("Environment"); break;
    case 13: result.append("EditorEngineSettings"); break;
    case 14: result.append("AI"); break;
    case 15: result.append("PlayerControlObject"); break;
    case 16: result.append("FireArm"); break;
    case 17: result.append("Sound"); break;
    case 18: result.append("Light"); break;
    case 19: result.append("Particle"); break;
    case 20: result.append("Projectile"); break;
    case 21: result.append("Emitter"); break;
    case 22: result.append("Rendering"); break;
    case 23: result.append("Animation"); break;
    case 24: result.append("WindAffector"); break;
    case 25: result.append("EffectBundleSettings"); break;
    case 26: result.append("AlphaIntensity"); break;
    case 27: result.append("EmitFrequency"); break;
    case 28: result.append("EmitSpeed"); break;
    case 29: result.append("StartAge"); break;
    case 30: result.append("SoundIntensity"); break;
    case 31: result.append("AgeScale"); break;
    case 32: result.append("LodSettings"); break;
    default: result.append("Undefined"); break;
    }
    return result;
}

// Every registered row whose object name matches the text before the first '.' (case-insensitive)
// and whose isActive() holds - so for ObjectTemplate, only the rows that accept the current template -
// one function name per line, in registration order. With "Object.member" the list keeps only the
// rows whose own name, cut at its first '.', equals <member> (case-insensitive): "ObjectTemplate.armor"
// yields the armor.* rows.
std::string Console_getActiveMethods(const std::string& name)
{
    const std::string objectName = consoleNameBeforeDot(name);
    const std::string memberFilter = consoleNameAfterDot(name);
    std::string result;
    const ConsoleObjects::ConsoleObjectList& objects = mainConsoleObjects->getConsoleObjects();
    for (ConsoleObjects::ConsoleObjectList::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        ConsoleObject* object = *it;
        if (!object->isActive())                                                        // +0x30
            continue;
        if (compareIgnoreCase(object->getObjectName(), objectName.c_str()) != 0)        // +0x0C
            continue;
        if (!memberFilter.empty())
        {
            const std::string member = consoleNameBeforeDot(std::string(object->getFunctionName()));
            if (compareIgnoreCase(memberFilter.c_str(), member.c_str()) != 0)
                continue;
        }
        result.append(object->getFunctionName());                                       // +0x10, re-read
        result.append("\n");
    }
    return result;
}

// The twelve rows
BF2_CONSOLE_OBJECT("Console", 2, 0)

// Shared two-string reflection binding. Explicit class-name suffixes avoid collisions with line-generated names.
#define BF2_IO_CONSOLE_REFLECTION_ROW_2(ClassName, fnLiteral, target)                              \
namespace {                                                                                        \
class ClassName : public ConsoleObjectBaseImpl                                                     \
{                                                                                                  \
public:                                                                                            \
    ClassName()                                                                                    \
    {                                                                                              \
        m_objectName     = consoleDefaultName();                                                   \
        m_access         = consoleDefaultAccess();                                                 \
        m_category       = consoleDefaultCategory();                                               \
        m_functionName   = (fnLiteral);                                                            \
        m_type           = 0;                                                                      \
        m_minNoArgs      = 2;                                                                      \
        m_maxNoArgs      = 2;                                                                      \
        m_returnType     = "std::string";                                                          \
        m_hasReturnValue = false;                                                                  \
        m_isDynamic      = false;                                                                  \
        m_argTypes[0]    = "std::string";                                                          \
        m_argTypes[1]    = "std::string";                                                          \
        for (int i = 0; i < 2; ++i)                                                                \
            m_argNeedsConversion[i] = (std::string(m_argTypes[i]) == "std::string") ? 0 : -1;     \
        consoleBindingList(kFileConsoleTag).push_back(this);                                       \
    }                                                                                              \
    virtual bool isObjectActive() const { return true; }                                           \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 2) return;                                                               \
        std::string objectName = BF2_CC_ARG(std::string, 0);                                       \
        std::string methodName = BF2_CC_ARG(std::string, 1);                                       \
        m_hasReturnValue = true;                                                                   \
        m_returnValue = target(objectName, methodName);                                            \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(std::string, 1)                                                             \
        BF2_CC_CONVERT(std::string, 2)                                                             \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    std::string m_returnValue;                                     /* +0xA4 */                     \
};                                                                                                 \
static ClassName s_##ClassName;                                                                    \
}

BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass443_getArgumentTypes, "getArgumentTypes", Console_getArgumentTypes)

// Console.getEnumName <object> <member> <int> - ConsoleClass444: argc exactly 3, the third argument
// converted as an int (setArgFromString index 3, ELF 0x8510510) and read before hasReturnValue is set.
namespace {
class ConsoleClass444_getEnumName : public ConsoleObjectBaseImpl
{
public:
    ConsoleClass444_getEnumName()
    {
        m_objectName     = consoleDefaultName();
        m_access         = consoleDefaultAccess();
        m_category       = consoleDefaultCategory();
        m_functionName   = "getEnumName";
        m_type           = 0;
        m_minNoArgs      = 3;
        m_maxNoArgs      = 3;
        m_returnType     = "std::string";
        m_hasReturnValue = false;
        m_isDynamic      = false;
        m_argTypes[0]    = "std::string";
        m_argTypes[1]    = "std::string";
        m_argTypes[2]    = "int";
        for (int i = 0; i < 3; ++i)
            m_argNeedsConversion[i] = (std::string(m_argTypes[i]) == "std::string") ? 0 : -1;
        consoleBindingList(kFileConsoleTag).push_back(this);
    }
    virtual bool isObjectActive() const { return true; }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 3) return;
        std::string objectName = BF2_CC_ARG(std::string, 0);
        std::string methodName = BF2_CC_ARG(std::string, 1);
        const int value = BF2_CC_ARG(int, 2);
        m_hasReturnValue = true;
        m_returnValue = Console_getEnumName(objectName, methodName, value);
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
        BF2_CC_CONVERT(std::string, 2)
        BF2_CC_CONVERT(int, 3)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;                                     // +0xA4
};
static ConsoleClass444_getEnumName s_consoleClass444;
} // namespace

BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass445_getRange,           "getRange",           Console_getRange)
BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass446_getReturnType,      "getReturnType",      Console_getReturnType)
BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass447_getDelta,           "getDelta",           Console_getDelta)
BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass448_hasBundledType,     "hasBundledType",     Console_hasBundledType)
BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass449_getTypeDescription, "getTypeDescription", Console_getTypeDescription)
BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass450_isDynamic,          "isDynamic",          Console_isDynamic)
BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass451_isSetGet,           "isSetGet",           Console_isSetGet)
BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass452_hasRange,           "hasRange",           Console_hasRange)
BF2_IO_CONSOLE_REFLECTION_ROW_2(ConsoleClass453_getCategory,        "getCategory",        Console_getCategory)
#undef BF2_IO_CONSOLE_REFLECTION_ROW_2

// Console.getActiveMethods <object>[.<member>] - ConsoleClass454: argc exactly 1.
namespace {
class ConsoleClass454_getActiveMethods : public ConsoleObjectBaseImpl
{
public:
    ConsoleClass454_getActiveMethods()
    {
        m_objectName     = consoleDefaultName();
        m_access         = consoleDefaultAccess();
        m_category       = consoleDefaultCategory();
        m_functionName   = "getActiveMethods";
        m_type           = 0;
        m_minNoArgs      = 1;
        m_maxNoArgs      = 1;
        m_returnType     = "std::string";
        m_hasReturnValue = false;
        m_isDynamic      = false;
        m_argTypes[0]    = "std::string";
        m_argNeedsConversion[0] = (std::string(m_argTypes[0]) == "std::string") ? 0 : -1;
        consoleBindingList(kFileConsoleTag).push_back(this);
    }
    virtual bool isObjectActive() const { return true; }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1) return;
        std::string name = BF2_CC_ARG(std::string, 0);
        m_hasReturnValue = true;
        m_returnValue = Console_getActiveMethods(name);
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;                                     // +0xA4
};
static ConsoleClass454_getActiveMethods s_consoleClass454;
} // namespace

// console.allowMultipleFileLoad - dedicated PE defaults 0x770C50 ("Console", 2, 0); ctor 0x646B37
// (vtable off_7F17F8, property bool, m_isDynamic = 0). isObjectActive 0x70FC1B is unconditionally
// true; executor 0x646BF6 dispatches IConsole +124/+128 (set/get, OldConsole fields at +0x120). The
// receiver is the main console singleton; like the shipped executor it is not null-checked.
namespace {
class ConsoleClass_allowMultipleFileLoad : public ConsoleObjectBaseImpl
{
public:
    ConsoleClass_allowMultipleFileLoad()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "allowMultipleFileLoad";
        m_type = 1;
        m_minNoArgs = 0;
        m_maxNoArgs = 1;
        m_returnType = "bool";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "bool";
        m_argNeedsConversion[0] = -1;
        consoleBindingList(kFileConsoleTag).push_back(this);
    }
    bool isObjectActive() const override { return true; }
    void executeObjectMethod() override
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            mainConsole->setAllowMultipleFileLoad(BF2_CC_ARG(bool, 0));
        }
        else if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = mainConsole->getAllowMultipleFileLoad();
        }
    }
    void setArgFromString(int index, const std::string& value) override { BF2_CC_CONVERT(bool, 1) }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
static ConsoleClass_allowMultipleFileLoad s_consoleAllowMultipleFileLoad;
} // namespace

// Binding defaults are set independently for each console object.
BF2_CONSOLE_OBJECT("SettingsManager", 1, 0)
namespace {
class ConsoleClass503 : public ConsoleObjectBaseImpl
{
public:
    ConsoleClass503()
    {
        m_objectName = "SettingsManager";
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "stringSet";
        m_type = 0;
        m_minNoArgs = m_maxNoArgs = 2;
        m_returnType = "bool";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "std::string";
        m_argTypes[1] = "std::string";
        m_argNeedsConversion[0] = m_argNeedsConversion[1] = 0;
        consoleBindingList(kFileConsoleTag).push_back(this);
    }
    virtual bool isObjectActive() const { return true; }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 2) return;
        std::string name = BF2_CC_ARG(std::string, 0);
        std::string value = BF2_CC_ARG(std::string, 1);
        m_hasReturnValue = true;
        m_returnValue = consoleSettingsStringSet(name, value);
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
        BF2_CC_CONVERT(std::string, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
static ConsoleClass503 s_settingsStringSet;
}

// SettingsManager.boolSet / SettingsManager.floatGet - dedicated PE defaults 0x770E66
// ("SettingsManager", access 1, category 0); ctors 0x646FB9 / 0x647DFC (vtable off_7F1930 /
// off_7F1D20, both file-static, m_isDynamic = 0). isObjectActive 0x70FC1B is unconditionally true.
// boolSet takes (std::string, bool) and returns bool; floatGet takes std::string and returns float.
// Both first require the name to be a registered variable and otherwise answer false / -1.0
// (PE helpers 0x64715B / 0x647F5D), then delegate to settings->boolGet/boolSet and settings->floatGet.
namespace {
bool consoleSettingsBoolSet(std::string& name, bool value)
{
    if (settingsManager->m_registeredVars.find(name) == settingsManager->m_registeredVars.end())
        return false;
    const bool changed = settings->boolGet(name, false) != value;
    settings->boolSet(name, value);
    if (changed && eventManager)
        eventManager->postEvent(EventCategory(5), 1, &name, 0.0f);
    return true;
}

float consoleSettingsFloatGet(const std::string& name)
{
    if (settingsManager->m_registeredVars.find(name) == settingsManager->m_registeredVars.end())
        return -1.0f;
    return settings->floatGet(name, -1.0f);
}
} // namespace

namespace {
class ConsoleClass_boolSet : public ConsoleObjectBaseImpl
{
public:
    ConsoleClass_boolSet()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "boolSet";
        m_type = 0;
        m_minNoArgs = 2;
        m_maxNoArgs = 2;
        m_returnType = "bool";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "std::string";
        m_argTypes[1] = "bool";
        m_argNeedsConversion[0] = 0;
        m_argNeedsConversion[1] = -1;
        consoleBindingList(kFileConsoleTag).push_back(this);
    }
    bool isObjectActive() const override { return true; }
    void executeObjectMethod() override
    {
        if (m_argCount != 2) return;
        std::string name = BF2_CC_ARG(std::string, 0);
        const bool value = BF2_CC_ARG(bool, 1);
        m_hasReturnValue = true;
        m_returnValue = consoleSettingsBoolSet(name, value);
    }
    void setArgFromString(int index, const std::string& value) override
    {
        BF2_CC_CONVERT(std::string, 1)
        BF2_CC_CONVERT(bool, 2)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    bool m_returnValue;
};
static ConsoleClass_boolSet s_settingsBoolSet;

class ConsoleClass_floatGet : public ConsoleObjectBaseImpl
{
public:
    ConsoleClass_floatGet()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "floatGet";
        m_type = 0;
        m_minNoArgs = 1;
        m_maxNoArgs = 1;
        m_returnType = "float";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "std::string";
        m_argNeedsConversion[0] = 0;
        consoleBindingList(kFileConsoleTag).push_back(this);
    }
    bool isObjectActive() const override { return true; }
    void executeObjectMethod() override
    {
        if (m_argCount != 1) return;
        std::string name = BF2_CC_ARG(std::string, 0);
        m_hasReturnValue = true;
        m_returnValue = consoleSettingsFloatGet(name);
    }
    void setArgFromString(int index, const std::string& value) override { BF2_CC_CONVERT(std::string, 1) }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    float m_returnValue;
};
static ConsoleClass_floatGet s_settingsFloatGet;
} // namespace
BF2_CONSOLE_OBJECT("FileManager", 2, 0)

// mountArchive accepts an optional contentCheck argument.
BF2_CONSOLE_METHOD_2_3(mountArchive, consoleMountArchive, std::string, std::string, bool, kFileConsoleTag)
// fileManager.fileExists - dedicated PE defaults 0x771186 ("FileManager", access 2, category 0);
// ctor 0x64AC82 (vtable off_7F2D70, method, m_isDynamic = 1). isObjectActive 0x64B16F is
// `fileManager != 0`; executor 0x64ACF6 dispatches IFileManager +0x1C (FileManager::fileExists).
namespace {
bool consoleFileExists(const std::string& name) { return fileManager->fileExists(name); }
} // namespace
BF2_CONSOLE_METHOD_1_RET_ACTIVE(fileExists, consoleFileExists, bool, fileManager != 0,
                                std::string, kFileConsoleTag)
// Returns the file-manager command list registered by ModuleIO.
// unmountArchive is available only through C++, with no console binding.
ConsoleObjectList& fileManagerConsoleObjects()
{
    return consoleBindingList(kFileConsoleTag);
}

} // namespace io
} // namespace hfe
} // namespace dice
