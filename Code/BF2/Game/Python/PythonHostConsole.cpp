// The `pythonHost` console object, registered by ModuleGame. [CHOSEN, not recovered] Separate
// translation unit; the original keeps these bindings in the 0x8B1704C list's TU beside sv and
// renderer. Content drives it from .con scripts with `pythonHost.sendCommand command [args...]`.
#include "../../Core/ConsoleObjectMacro.h"
#include "PythonHost.h"
#include <string>

namespace dice {
namespace hfe {

namespace {

// Stands in for the original's file-scope list (ELF 0x8B1704C). ModuleGame::init registers it.
const char* const kPythonHostConsoleTag = "hfe.pythonHost";

} // namespace

// One to nine raw strings; the executor pads the missing ones with "" and stores
// PythonHost::consoleSendCommand's bool as the reply.
#define BF2_PYTHONHOST_SEND_COMMAND(fnName, listTag)                                               \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 1, 9, "bool", true)                                                     \
            for (int i = 0; i < 9; ++i)                                                            \
                m_argTypes[i] = "std::string";                                                     \
    BF2_CC_TAIL(9, listTag)                                                                        \
    virtual bool isObjectActive() const { return python::pythonHost() != 0; }                      \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        std::string args[9];                                                                       \
        for (int i = 0; i < m_argCount && i < 9; ++i)                                              \
            args[i] = BF2_CC_ARG(std::string, i);                                                  \
        m_hasReturnValue = true;                                                                   \
        m_returnValue = python::pythonHost()->consoleSendCommand(                                  \
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);      \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(std::string, 1)                                                             \
        BF2_CC_CONVERT(std::string, 2)                                                             \
        BF2_CC_CONVERT(std::string, 3)                                                             \
        BF2_CC_CONVERT(std::string, 4)                                                             \
        BF2_CC_CONVERT(std::string, 5)                                                             \
        BF2_CC_CONVERT(std::string, 6)                                                             \
        BF2_CC_CONVERT(std::string, 7)                                                             \
        BF2_CC_CONVERT(std::string, 8)                                                             \
        BF2_CC_CONVERT(std::string, 9)                                                             \
    }                                                                                              \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    bool m_returnValue;                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// PE 0x761160.
BF2_CONSOLE_OBJECT("pythonHost", 2, 0)

// PE ctor 0x4CD740.
BF2_PYTHONHOST_SEND_COMMAND(sendCommand, kPythonHostConsoleTag)

#undef BF2_PYTHONHOST_SEND_COMMAND

// Handed to the console registry by ModuleGame::init, the way the original registers 0x8B1704C.
ConsoleObjectList& pythonHostConsoleObjects()
{
    return consoleBindingList(kPythonHostConsoleTag);
}

} // namespace hfe
} // namespace dice
