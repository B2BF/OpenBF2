// [CHOSEN] Macro names reconstruct dice's generator; expansion behavior is recovered from the ELF.
// Each binding registers an anonymous ConsoleClass<line> instance. Preserve one-based argument indices,
// base m_args access, type spelling, optional-argument dispatch and the binding's activity check.
#ifndef CORE_CONSOLEOBJECTMACRO_H
#define CORE_CONSOLEOBJECTMACRO_H

#include "ConsoleObjectBase.h"
#include "ConsoleObjects.h"
#include <string>
#include <sstream>
#include <utility>
#include <list>

namespace dice {
namespace hfe {

typedef std::list<ConsoleObject*> ConsoleObjectList;

// [CHOSEN] shape. In the original each translation unit has its OWN file-scope list and the owning
// module registers exactly that one. A keyed accessor preserves the property callers depend on -
// init() and close() must see the same list - without inventing a global per file. Same reasoning as
// ModuleRegistry.cpp's listAt(). Function-local static, so it exists before any generated ctor runs.
ConsoleObjectList& consoleBindingList(const char* tag);

// The three globals SetConsoleClassDefaults writes (ELF 0x8ABBC30 / 34 / 38).
const char*   consoleDefaultName();
unsigned int  consoleDefaultAccess();
unsigned char consoleDefaultCategory();

// Token paste with __LINE__ needs two levels, or you paste the literal text
#define BF2_CC_CAT2(a, b)  a##b
#define BF2_CC_CAT(a, b)   BF2_CC_CAT2(a, b)
#define BF2_CC_NAME        BF2_CC_CAT(ConsoleClass, __LINE__)
#define BF2_CC_INSTANCE    BF2_CC_CAT(s_consoleClass, __LINE__)

// Sets this file's console defaults. The first name character is lowercased for command lookup.
#define BF2_CONSOLE_OBJECT(objectName, accessLevel, categoryId)                                    \
    static ::dice::hfe::SetConsoleClassDefaults                                                    \
        BF2_CC_CAT(s_consoleDefaults, __LINE__)((objectName), (accessLevel), (categoryId));

// Converts one-based arguments into persistent per-type storage.
// Extract into a temporary and commit only on success so malformed input preserves the previous value.
// String extraction takes one token; normal string arguments bypass this through execute's raw branch.
#define BF2_CC_CONVERT(T, oneBasedIndex)                                                           \
        if (index == (oneBasedIndex))                                                              \
        {                                                                                          \
            static T s;                                                                            \
            T tmp = s;                                                                             \
            std::istringstream in(value);                                                          \
            in >> tmp;                                                                             \
            if (!in.fail())                                                                        \
                s = tmp;                                                                           \
            m_args[(oneBasedIndex) - 1] = reinterpret_cast<std::string*>(&s);                      \
        }

// The executor's read of argument slot i (zero-based). For std::string this is exactly the raw
// pointer execute() planted; for a converting type the slot holds the typed static's address that
// BF2_CC_CONVERT planted - 559's executor reads both through the same slots.
#define BF2_CC_ARG(T, zeroBasedIndex)  (*reinterpret_cast<T*>(m_args[(zeroBasedIndex)]))

// Constructor helpers. Fill m_argTypes between the head and tail so the conversion loop
// never reads uninitialized type pointers during static initialization.

// typeVal: 0 = method, 1 = set/get property (see the m_type table in ConsoleObjectBase.h).
// min/max: the accepted argc range; a property is 0..1 (127/242 ctors), an optional-argument
// method stores its real span (559 stores 2 and 3; 626 stores 1 and 9).
#define BF2_CC_HEAD(fnName, typeVal, minArgs, maxArgs, retTypeName, isDyn)                         \
    public:                                                                                        \
        BF2_CC_NAME()                                                                              \
        {                                                                                          \
            m_objectName     = ::dice::hfe::consoleDefaultName();                                  \
            m_access         = ::dice::hfe::consoleDefaultAccess();                                \
            m_category       = ::dice::hfe::consoleDefaultCategory();                              \
            m_functionName   = #fnName;                                                            \
            m_type           = (typeVal);                                                          \
            m_minNoArgs      = (minArgs);                                                          \
            m_maxNoArgs      = (maxArgs);                                                          \
            m_returnType     = (retTypeName);                                                      \
            m_hasReturnValue = false;                                                              \
            m_isDynamic      = (isDyn);

// Same generated constructor shape, but with a quoted console spelling. SupplyObjectTemplate's
// property is literally `3dMapIcon`, which cannot be supplied as a C++ preprocessor identifier.
#define BF2_CC_HEAD_NAMED(fnNameLiteral, typeVal, minArgs, maxArgs, retTypeName, isDyn)             \
    public:                                                                                        \
        BF2_CC_NAME()                                                                              \
        {                                                                                          \
            m_objectName     = ::dice::hfe::consoleDefaultName();                                  \
            m_access         = ::dice::hfe::consoleDefaultAccess();                                \
            m_category       = ::dice::hfe::consoleDefaultCategory();                              \
            m_functionName   = (fnNameLiteral);                                                    \
            m_type           = (typeVal);                                                          \
            m_minNoArgs      = (minArgs);                                                          \
            m_maxNoArgs      = (maxArgs);                                                          \
            m_returnType     = (retTypeName);                                                      \
            m_hasReturnValue = false;                                                              \
            m_isDynamic      = (isDyn);

#define BF2_CC_TAIL(nArgs, listTag)                                                                \
            for (int i = 0; i < (nArgs); ++i)                                                      \
                m_argNeedsConversion[i] =                                                          \
                    (std::string(m_argTypes[i]) == "std::string") ? 0 : -1;                        \
            ::dice::hfe::consoleBindingList(listTag).push_back(this);                              \
        }

// Default metadata hooks for bindings without ranges. Ranged bindings use BF2_CC_RANGED_HOOKS instead.
#define BF2_CC_COMMON_HOOKS                                                                        \
    virtual bool objectHasRange() const { return false; }                                          \
    virtual std::pair<const void*, const void*> getObjectRange() const                             \
        { return std::pair<const void*, const void*>(0, 0); }                                      \
    virtual std::pair<std::string, std::string> getObjectRangeAsString() const                     \
        { return std::pair<std::string, std::string>(); }                                          \
    virtual void checkObjectRange(bool) { }

// Publishes mutable enum bounds and their addresses. Values clamp only when requested;
// execute() validates without clamping, preserving acceptance of upper sentinel values.
#define BF2_CC_RANGED_HOOKS(T, minValue, maxValue)                                                 \
    virtual bool objectHasRange() const { return true; }                                           \
    virtual std::pair<const void*, const void*> getObjectRange() const                             \
    {                                                                                              \
        m_rangeMin = (minValue);                                                                   \
        m_rangeMax = (maxValue);                                                                   \
        return std::pair<const void*, const void*>(&m_rangeMin, &m_rangeMax);                      \
    }                                                                                              \
    virtual std::pair<std::string, std::string> getObjectRangeAsString() const                     \
    {                                                                                              \
        m_rangeMin = (minValue);                                                                   \
        m_rangeMax = (maxValue);                                                                   \
        std::ostringstream minText;                                                                \
        std::ostringstream maxText;                                                                \
        minText << m_rangeMin;                                                                     \
        maxText << m_rangeMax;                                                                     \
        return std::pair<std::string, std::string>(minText.str(), maxText.str());                  \
    }                                                                                              \
    virtual void checkObjectRange(bool clamp)                                                     \
    {                                                                                              \
        if (m_argCount < 1)                                                                        \
            return;                                                                                \
        m_rangeMin = (minValue);                                                                   \
        m_rangeMax = (maxValue);                                                                   \
        T& value = BF2_CC_ARG(T, 0);                                                               \
        const int numeric = static_cast<int>(value);                                               \
        if (clamp && numeric < m_rangeMin)                                                         \
            value = static_cast<T>(m_rangeMin);                                                    \
        else if (clamp && numeric > m_rangeMax)                                                    \
            value = static_cast<T>(m_rangeMax);                                                    \
    }

// The formatting override for a binding that returns a value - 127's 474-byte shape: empty string
// unless the executor set the flag, else the value through an ostringstream with no manipulators
// (so bool prints 1/0, float prints 6 significant digits - the same defaults the .con constant
// helpers in ModuleRegistry.cpp document).
#define BF2_CC_RETURN_AS_STRING                                                                    \
    virtual std::string getReturnValueAsString() const                                             \
    {                                                                                              \
        if (!m_hasReturnValue)                                                                     \
            return std::string();                                                                  \
        std::ostringstream out;                                                                    \
        out << m_returnValue;                                                                      \
        return out.str();                                                                          \
    }

// The 33-byte constant form the void bindings carry (same code as the base's, overridden anyway -
// uniform emission again; ConsoleClass162's at 0x83E9800).
#define BF2_CC_NO_RETURN_AS_STRING                                                                 \
    virtual std::string getReturnValueAsString() const { return std::string(); }

// Registers a fixed-arity void command that is always active.
// Bindings whose receiver may be absent must use an activeExpr variant.
#define BF2_CONSOLE_METHOD_1(fnName, target, A0, listTag)                                          \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 1, 1, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return true; }                                           \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 1) return;      /* outside the range: nothing, no diagnostic */          \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0));                                                                 \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

#define BF2_CONSOLE_METHOD_2(fnName, target, A0, A1, listTag)                                      \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 2, 2, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
    BF2_CC_TAIL(2, listTag)                                                                        \
    virtual bool isObjectActive() const { return true; }                                           \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 2) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                                              \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Binds a zero-argument command with an availability check and no return value.
#define BF2_CONSOLE_METHOD_0_ACTIVE(fnName, target, activeExpr, listTag)                            \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 0, 0, "void", true)                                                     \
    BF2_CC_TAIL(0, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 0) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target();                                                                                  \
    }                                                                                              \
    virtual void setArgFromString(int, const std::string&) { }                                     \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a static zero-argument void command with a receiver-availability check.
#define BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(fnName, target, activeExpr, listTag)                     \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 0, 0, "void", false)                                                    \
    BF2_CC_TAIL(0, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 0) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target();                                                                                  \
    }                                                                                              \
    virtual void setArgFromString(int, const std::string&) { }                                     \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a zero-argument command with a typed result and receiver-availability check.
#define BF2_CONSOLE_METHOD_0_RET_ACTIVE(fnName, target, RET, activeExpr, listTag)                   \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 0, 0, #RET, true)                                                       \
    BF2_CC_TAIL(0, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 0) return;                                                               \
        m_hasReturnValue = true;                                                                   \
        m_returnValue = target();                                                                  \
    }                                                                                              \
    virtual void setArgFromString(int, const std::string&) { }                                     \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    RET m_returnValue;                                                                             \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Binds a two-argument command with an availability check.
#define BF2_CONSOLE_METHOD_2_ACTIVE(fnName, target, activeExpr, A0, A1, listTag)                    \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 2, 2, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
    BF2_CC_TAIL(2, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 2) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                                              \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a one-argument void command with a receiver-availability check.
#define BF2_CONSOLE_METHOD_1_ACTIVE(fnName, target, activeExpr, A0, listTag)                        \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 1, 1, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 1) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0));                                                                 \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// File-static generated methods use the same executors as the active fixed-arity families, but
// carry m_isDynamic=false. GameLogic's content vocabulary is the recovered exemplar.
#define BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(fnName, target, activeExpr, A0, listTag)                 \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 1, 1, "void", false)                                                    \
            m_argTypes[0] = #A0;                                                                   \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 1) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0));                                                                 \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

#define BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(fnName, target, activeExpr, A0, A1, listTag)             \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 2, 2, "void", false)                                                    \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
    BF2_CC_TAIL(2, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 2) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                                              \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

#define BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(fnName, target, activeExpr, A0, A1, A2, listTag)         \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 3, 3, "void", false)                                                    \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
    BF2_CC_TAIL(3, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 3) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2));                           \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

#define BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(fnName, target, activeExpr, A0, A1, A2, A3, listTag)     \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 4, 4, "void", false)                                                    \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
            m_argTypes[3] = #A3;                                                                   \
    BF2_CC_TAIL(4, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 4) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2), BF2_CC_ARG(A3, 3));        \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
        BF2_CC_CONVERT(A3, 4)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a static void command accepting one or two arguments. Any target result is discarded.
#define BF2_CONSOLE_METHOD_1_2_STATIC_ACTIVE(fnName, target, activeExpr, A0, A1, listTag)           \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 1, 2, "void", false)                                                    \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
    BF2_CC_TAIL(2, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)                                                                       \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0));                                                             \
        }                                                                                          \
        else if (m_argCount == 2)                                                                  \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                                          \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a dynamic void command accepting one or two arguments. The one-argument arm uses the
// target's C++ default argument; both arms gate on activeExpr.
#define BF2_CONSOLE_METHOD_1_2_ACTIVE(fnName, target, activeExpr, A0, A1, listTag)                  \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 1, 2, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
    BF2_CC_TAIL(2, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)                                                                       \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0));                                                             \
        }                                                                                          \
        else if (m_argCount == 2)                                                                  \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                                          \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a void command accepting two or three arguments. The short form uses the target's C++ default argument.
#define BF2_CONSOLE_METHOD_2_3(fnName, target, A0, A1, A2, listTag)                                \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 2, 3, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
    BF2_CC_TAIL(3, listTag)                                                                        \
    virtual bool isObjectActive() const { return true; }                                           \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 2)              /* 0x8523EC5: the two-arg arm, default fills arg 3 */    \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                                          \
        }                                                                                          \
        else if (m_argCount == 3)         /* 0x8523F35: the parsed third argument */               \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2));                       \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// The active-target twin of family 2. Its execution shape is identical; only isObjectActive is
// recovered from a live target test, as it is for generated object-template bindings.
#define BF2_CONSOLE_METHOD_2_3_ACTIVE(fnName, target, activeExpr, A0, A1, A2, listTag)              \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 2, 3, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
    BF2_CC_TAIL(3, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 2)                                                                       \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                                          \
        }                                                                                          \
        else if (m_argCount == 3)                                                                  \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2));                       \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Gated method with two or three arguments and a return value on both arms.
// A2DEFAULT supplies the missing third argument. Upstream lookup must honor activeExpr because
// the executor trusts its receiver without another null check.
#define BF2_CONSOLE_METHOD_2_3_RET_ACTIVE(fnName, target, RET, activeExpr, A0, A1, A2, A2DEFAULT, listTag) namespace {                                                                                        class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      {                                                                                                      BF2_CC_HEAD(fnName, 0, 2, 3, #RET, true)                                                                   m_argTypes[0] = #A0;                                                                               m_argTypes[1] = #A1;                                                                               m_argTypes[2] = #A2;                                                                       BF2_CC_TAIL(3, listTag)                                                                            virtual bool isObjectActive() const { return (activeExpr); }                                       virtual void executeObjectMethod()                                                                 {                                                                                                      if (m_argCount == 2)              /* 0x87DA1F0: the third argument is defaulted */                 {                                                                                                      m_hasReturnValue = true;                                                                           m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), (A2DEFAULT));                     }                                                                                                  else if (m_argCount == 3)         /* 0x87DA26C: the parsed third argument */                       {                                                                                                      m_hasReturnValue = true;                                                                           m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2));               }                                                                                              }                                                                                                  virtual void setArgFromString(int index, const std::string& value)                                 {                                                                                                      BF2_CC_CONVERT(A0, 1)                                                                              BF2_CC_CONVERT(A1, 2)                                                                              BF2_CC_CONVERT(A2, 3)                                                                          }                                                                                                  BF2_CC_RETURN_AS_STRING                                                                            BF2_CC_COMMON_HOOKS                                                                            private:                                                                                               RET m_returnValue;                                                                             };                                                                                                 static BF2_CC_NAME BF2_CC_INSTANCE;                                                                }

// Registers a three-argument void command with a receiver-availability check.
#define BF2_CONSOLE_METHOD_3(fnName, target, activeExpr, A0, A1, A2, listTag)                      \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 3, 3, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
    BF2_CC_TAIL(3, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 3) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2));                           \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a four-argument void command with a receiver-availability check.
// Type names select converted values or raw string arguments.
#define BF2_CONSOLE_METHOD_4(fnName, target, activeExpr, A0, A1, A2, A3, listTag)            \
namespace {                                                                                  \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                \
{                                                                                            \
    BF2_CC_HEAD(fnName, 0, 4, 4, "void", true)                                               \
            m_argTypes[0] = #A0;                                                             \
            m_argTypes[1] = #A1;                                                             \
            m_argTypes[2] = #A2;                                                             \
            m_argTypes[3] = #A3;                                                             \
    BF2_CC_TAIL(4, listTag)                                                                  \
    virtual bool isObjectActive() const { return (activeExpr); }                             \
    virtual void executeObjectMethod()                                                       \
    {                                                                                        \
        if (m_argCount != 4) return;                                                         \
        m_hasReturnValue = false;                                                            \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2), BF2_CC_ARG(A3, 3));  \
    }                                                                                        \
    virtual void setArgFromString(int index, const std::string& value)                       \
    {                                                                                        \
        BF2_CC_CONVERT(A0, 1)                                                                \
        BF2_CC_CONVERT(A1, 2)                                                                \
        BF2_CC_CONVERT(A2, 3)                                                                \
        BF2_CC_CONVERT(A3, 4)                                                                \
    }                                                                                        \
    BF2_CC_NO_RETURN_AS_STRING                                                               \
    BF2_CC_COMMON_HOOKS                                                                      \
};                                                                                           \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                          \
}

// Registers a five-argument void command with a receiver-availability check.
#define BF2_CONSOLE_METHOD_5(fnName, target, activeExpr, A0, A1, A2, A3, A4, listTag)        \
namespace {                                                                                  \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                \
{                                                                                            \
    BF2_CC_HEAD(fnName, 0, 5, 5, "void", true)                                               \
            m_argTypes[0] = #A0;                                                             \
            m_argTypes[1] = #A1;                                                             \
            m_argTypes[2] = #A2;                                                             \
            m_argTypes[3] = #A3;                                                             \
            m_argTypes[4] = #A4;                                                             \
    BF2_CC_TAIL(5, listTag)                                                                  \
    virtual bool isObjectActive() const { return (activeExpr); }                             \
    virtual void executeObjectMethod()                                                       \
    {                                                                                        \
        if (m_argCount != 5) return;                                                         \
        m_hasReturnValue = false;                                                            \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2), BF2_CC_ARG(A3, 3),   \
               BF2_CC_ARG(A4, 4));                                                           \
    }                                                                                        \
    virtual void setArgFromString(int index, const std::string& value)                       \
    {                                                                                        \
        BF2_CC_CONVERT(A0, 1)                                                                \
        BF2_CC_CONVERT(A1, 2)                                                                \
        BF2_CC_CONVERT(A2, 3)                                                                \
        BF2_CC_CONVERT(A3, 4)                                                                \
        BF2_CC_CONVERT(A4, 5)                                                                \
    }                                                                                        \
    BF2_CC_NO_RETURN_AS_STRING                                                               \
    BF2_CC_COMMON_HOOKS                                                                      \
};                                                                                           \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                          \
}

// Registers a two-argument command with a typed result, formatted through an output stream.
// Pointer results use the host standard library's pointer formatting.
#define BF2_CONSOLE_METHOD_2_RET(fnName, target, RET, A0, A1, listTag)                             \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 2, 2, #RET, true)                                                       \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
    BF2_CC_TAIL(2, listTag)                                                                        \
    virtual bool isObjectActive() const { return true; }                                           \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 2) return;                                                               \
        m_hasReturnValue = true;                                                                   \
        m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                              \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    RET m_returnValue;                                                                             \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers commands with typed results and receiver-availability checks.
#define BF2_CONSOLE_METHOD_1_RET_ACTIVE(fnName, target, RET, activeExpr, A0, listTag)              \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 1, 1, #RET, true)                                                       \
            m_argTypes[0] = #A0;                                                                   \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 1) return;                                                               \
        m_hasReturnValue = true;                                                                   \
        m_returnValue = target(BF2_CC_ARG(A0, 0));                                                 \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    RET m_returnValue;                                                                             \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a command accepting zero or one argument with a typed result and an availability check.
// The executor supplies the receiver itself: with no argument it evaluates argZeroExpr (the active
// template/object) exactly as the shipped executor reads objectTemplateManager/objectManager; with
// one it passes the parsed proxy's pointer, which stays null for an unknown name. The file-static
// generated methods (ObjectTemplate.type/printScript, Object.listObjectsOfTemplate) carry
// m_isDynamic=false.
#define BF2_CONSOLE_METHOD_0_1_RET_STATIC_ACTIVE(fnName, target, RET, activeExpr, A0, argZeroExpr, listTag) \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 0, 1, #RET, false)                                                      \
            m_argTypes[0] = #A0;                                                                   \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 0)                                                                       \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = target(argZeroExpr);                                                   \
        }                                                                                          \
        else if (m_argCount == 1)                                                                  \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = target(BF2_CC_ARG(A0, 0).ptr);                                         \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    RET m_returnValue;                                                                             \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a returning command accepting one or two arguments. The target supplies the status text.
#define BF2_CONSOLE_METHOD_1_2_RET_ACTIVE(fnName, target, RET, activeExpr, A0, A1, listTag)         \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 1, 2, #RET, true)                                                       \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
    BF2_CC_TAIL(2, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)                                                                       \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = target(BF2_CC_ARG(A0, 0));                                             \
        }                                                                                          \
        else if (m_argCount == 2)                                                                  \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                          \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    RET m_returnValue;                                                                             \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

#define BF2_CONSOLE_METHOD_2_RET_ACTIVE(fnName, target, RET, activeExpr, A0, A1, listTag)          \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 2, 2, #RET, true)                                                       \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
    BF2_CC_TAIL(2, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 2) return;                                                               \
        m_hasReturnValue = true;                                                                   \
        m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                              \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    RET m_returnValue;                                                                             \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a command with three required and two optional arguments, a typed result and an availability check.
// The macro supplies omitted defaults; other argument counts do not invoke the target.
#define BF2_CONSOLE_METHOD_3_5_RET_ACTIVE(fnName, target, RET, activeExpr, A0, A1, A2, A3, A4, A3DEFAULT, A4DEFAULT, listTag) \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 3, 5, #RET, true)                                                       \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
            m_argTypes[3] = #A3;                                                                   \
            m_argTypes[4] = #A4;                                                                   \
    BF2_CC_TAIL(5, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 3)              /* 0x853A1xx: both optional arguments defaulted */       \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2),        \
                                   (A3DEFAULT), (A4DEFAULT));                                      \
        }                                                                                          \
        else if (m_argCount == 4)         /* the fifth defaulted */                                \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2),        \
                                   BF2_CC_ARG(A3, 3), (A4DEFAULT));                                \
        }                                                                                          \
        else if (m_argCount == 5)         /* everything parsed */                                  \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2),        \
                                   BF2_CC_ARG(A3, 3), BF2_CC_ARG(A4, 4));                          \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
        BF2_CC_CONVERT(A3, 4)                                                                      \
        BF2_CC_CONVERT(A4, 5)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    RET m_returnValue;                                                                             \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Set/get property: one argument calls the setter without a result; zero calls the getter.
// objExpr selects the receiver. Upstream lookup checks availability; execution does not recheck it.
#define BF2_CONSOLE_PROPERTY(fnName, T, objExpr, setFn, getFn, listTag)                            \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                                         \
            m_argTypes[0] = #T;                                                                    \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (objExpr) != 0; }                                 \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)              /* SET */                                                \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            (objExpr)->setFn(BF2_CC_ARG(T, 0));                                                    \
        }                                                                                          \
        else if (m_argCount == 0)         /* GET */                                                \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = (objExpr)->getFn();                                                    \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(T, 1)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    T m_returnValue;   /* +0xA2/+0xA4 in the originals - after the base, padding decides */        \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a property using explicit getter and setter expressions.
#define BF2_CONSOLE_PROPERTY_FN(fnName, T, activeExpr, getExpr, setCall, listTag)                  \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                                         \
            m_argTypes[0] = #T;                                                                    \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)              /* SET */                                                \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            setCall(BF2_CC_ARG(T, 0));                                                             \
        }                                                                                          \
        else if (m_argCount == 0)         /* GET */                                                \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = (getExpr);                                                             \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(T, 1)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    T m_returnValue;                                                                               \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// A property whose console spelling is not a legal C++ identifier. Its generated body is the
// same as BF2_CONSOLE_PROPERTY_FN; only the constructor takes an explicit string literal.
#define BF2_CONSOLE_PROPERTY_FN_NAMED(fnNameLiteral, T, activeExpr, getExpr, setCall, listTag)     \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD_NAMED(fnNameLiteral, 1, 0, 1, #T, true)                                            \
            m_argTypes[0] = #T;                                                                    \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)                                                                       \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            setCall(BF2_CC_ARG(T, 0));                                                             \
        }                                                                                          \
        else if (m_argCount == 0)                                                                  \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = (getExpr);                                                             \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(T, 1)                                                                       \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    T m_returnValue;                                                                               \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// A ranged enum property. PE proves the return member at +0xA4 and the two mutable int range
// members immediately after it at +0xA8/+0xAC for both recovered exemplars.
#define BF2_CONSOLE_PROPERTY_FN_RANGED(fnName, T, activeExpr, getExpr, setCall, minValue, maxValue, listTag) \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                                         \
            m_argTypes[0] = #T;                                                                    \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)                                                                       \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            setCall(BF2_CC_ARG(T, 0));                                                             \
        }                                                                                          \
        else if (m_argCount == 0)                                                                  \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = (getExpr);                                                             \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(T, 1)                                                                       \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_RANGED_HOOKS(T, minValue, maxValue)                                                     \
private:                                                                                           \
    T m_returnValue;                                                                               \
    mutable int m_rangeMin;                                                                        \
    mutable int m_rangeMax;                                                                        \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Publishes bounds in the property's own type, preserving fractional limits.
// Bounds clamp only when requested. Per-binding delta strings are not modeled here.
#define BF2_CC_RANGED_HOOKS_T(T, minValue, maxValue)                                               \
    virtual bool objectHasRange() const { return true; }                                           \
    virtual std::pair<const void*, const void*> getObjectRange() const                             \
    {                                                                                              \
        m_rangeMin = (minValue);                                                                   \
        m_rangeMax = (maxValue);                                                                   \
        return std::pair<const void*, const void*>(&m_rangeMin, &m_rangeMax);                      \
    }                                                                                              \
    virtual std::pair<std::string, std::string> getObjectRangeAsString() const                     \
    {                                                                                              \
        m_rangeMin = (minValue);                                                                   \
        m_rangeMax = (maxValue);                                                                   \
        std::ostringstream minText;                                                                \
        std::ostringstream maxText;                                                                \
        minText << m_rangeMin;                                                                     \
        maxText << m_rangeMax;                                                                     \
        return std::pair<std::string, std::string>(minText.str(), maxText.str());                  \
    }                                                                                              \
    virtual void checkObjectRange(bool clamp)                                                     \
    {                                                                                              \
        if (m_argCount < 1)                                                                        \
            return;                                                                                \
        m_rangeMin = (minValue);                                                                   \
        m_rangeMax = (maxValue);                                                                   \
        T& value = BF2_CC_ARG(T, 0);                                                               \
        if (value >= m_rangeMin)                                                                   \
        {                                                                                          \
            if (value > m_rangeMax && clamp)                                                       \
                value = m_rangeMax;                                                                \
        }                                                                                          \
        else if (clamp)                                                                            \
            value = m_rangeMin;                                                                    \
    }

#define BF2_CONSOLE_PROPERTY_FN_RANGED_T(fnName, T, activeExpr, getExpr, setCall, minValue, maxValue, listTag) \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                                         \
            m_argTypes[0] = #T;                                                                    \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)                                                                       \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            setCall(BF2_CC_ARG(T, 0));                                                             \
        }                                                                                          \
        else if (m_argCount == 0)                                                                  \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = (getExpr);                                                             \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(T, 1)                                                                       \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_RANGED_HOOKS_T(T, minValue, maxValue)                                                   \
private:                                                                                           \
    T m_returnValue;                                                                               \
    mutable T m_rangeMin;                                                                          \
    mutable T m_rangeMax;                                                                          \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a write-only property. A zero-argument read leaves the existing return-value state unchanged.
#define BF2_CONSOLE_PROPERTY_SETONLY(fnName, T, activeExpr, setCall, listTag)                      \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                                         \
            m_argTypes[0] = #T;                                                                    \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)              /* SET - the only arm, as shipped */                     \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            setCall(BF2_CC_ARG(T, 0));                                                             \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(T, 1)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    T m_returnValue;                                                                               \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers a property backed directly by a member rather than accessor methods.
#define BF2_CONSOLE_FIELD(fnName, T, objExpr, member, listTag)                                     \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                                         \
            m_argTypes[0] = #T;                                                                    \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (objExpr) != 0; }                                 \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)              /* SET - the inlined field write */                      \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            (objExpr)->member = BF2_CC_ARG(T, 0);                                                  \
        }                                                                                          \
        else if (m_argCount == 0)         /* GET - the inlined field read */                       \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = (objExpr)->member;                                                     \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(T, 1)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    T m_returnValue;                                                                               \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Method-shaped get/set binding with a string reply on both arms.
// The setter returns status (empty means success); the getter formats its value in the executor.
// activeExpr must guard unavailable receivers before execution.
#define BF2_CONSOLE_GETSET_1(fnName, A0, activeExpr, getExpr, setCall, listTag)                    \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 0, 1, "std::string", true)                                              \
            m_argTypes[0] = #A0;                                                                   \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 1)              /* SET - the status string is the reply */               \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            m_returnValue = setCall(BF2_CC_ARG(A0, 0));                                            \
        }                                                                                          \
        else if (m_argCount == 0)         /* GET - format the live value here */                   \
        {                                                                                          \
            m_hasReturnValue = true;                                                               \
            std::ostringstream out;                                                                \
            out << (getExpr);                                                                      \
            m_returnValue = out.str();                                                             \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    std::string m_returnValue;                                                                     \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

} // namespace hfe
} // namespace dice


// Registers a three-argument command with a typed result and availability check.
#define BF2_CONSOLE_METHOD_3_RET_ACTIVE(fnName, target, RET, activeExpr, A0, A1, A2, listTag)      \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 3, 3, #RET, true)                                                       \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
    BF2_CC_TAIL(3, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 3) return;                                                               \
        m_hasReturnValue = true;                                                                   \
        m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2));           \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    RET m_returnValue;                                                                             \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// Registers void commands with one optional trailing argument and a receiver-availability check.
// The macro supplies the omitted argument explicitly; activeExpr must protect any required receiver.
#define BF2_CONSOLE_METHOD_4_5_ACTIVE(fnName, target, activeExpr, A0, A1, A2, A3, A4, A4DEFAULT, listTag)\
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 4, 5, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
            m_argTypes[3] = #A3;                                                                   \
            m_argTypes[4] = #A4;                                                                   \
    BF2_CC_TAIL(5, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 4)              /* the trailing argument is defaulted */                 \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2), BF2_CC_ARG(A3, 3),     \
                   (A4DEFAULT));                                                                   \
        }                                                                                          \
        else if (m_argCount == 5)         /* the parsed trailing argument */                       \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2), BF2_CC_ARG(A3, 3),     \
                   BF2_CC_ARG(A4, 4));                                                             \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
        BF2_CC_CONVERT(A3, 4)                                                                      \
        BF2_CC_CONVERT(A4, 5)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

#define BF2_CONSOLE_METHOD_5_6_ACTIVE(fnName, target, activeExpr, A0, A1, A2, A3, A4, A5, A5DEFAULT, listTag)\
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 5, 6, "void", true)                                                     \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
            m_argTypes[3] = #A3;                                                                   \
            m_argTypes[4] = #A4;                                                                   \
            m_argTypes[5] = #A5;                                                                   \
    BF2_CC_TAIL(6, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount == 5)              /* the trailing argument is defaulted */                 \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2), BF2_CC_ARG(A3, 3),     \
                   BF2_CC_ARG(A4, 4), (A5DEFAULT));                                                \
        }                                                                                          \
        else if (m_argCount == 6)         /* the parsed trailing argument */                       \
        {                                                                                          \
            m_hasReturnValue = false;                                                              \
            target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2), BF2_CC_ARG(A3, 3),     \
                   BF2_CC_ARG(A4, 4), BF2_CC_ARG(A5, 5));                                          \
        }                                                                                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
        BF2_CC_CONVERT(A3, 4)                                                                      \
        BF2_CC_CONVERT(A4, 5)                                                                      \
        BF2_CC_CONVERT(A5, 6)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

#endif // CORE_CONSOLEOBJECTMACRO_H
