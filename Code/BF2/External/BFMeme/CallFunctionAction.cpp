// Function-call action; the function pointer is borrowed and never destroyed by the action.
#include "CallFunctionAction.h"
#if !DEDICATED
#include "Function.h"
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
CallFunctionAction::CallFunctionAction() : m_function(nullptr) {}
CallFunctionAction::~CallFunctionAction() {}                     // 835660, shared with SetVariableAction
TypeDescriptor* CallFunctionAction::getClassType()
{
    static ClassTypeDescriptor<CallFunctionAction, Action> type("dice::meme::CallFunctionAction");
    return &type;
}
TypeDescriptor* CallFunctionAction::getType() { return getClassType(); }
unsigned int CallFunctionAction::getObjectSize() { return sizeof(CallFunctionAction); }

// The tail normalises the stream result with setne, so a nonzero that
// is not 1 still reports true; reproduced by the explicit comparison.
bool CallFunctionAction::streamProperties(System* system, Stream* stream)
{
    if (!Action::streamProperties(system, stream)) return false;
    return stream->streamFunction(system, reinterpret_cast<Function*&>(m_function),
                                  "Function", nullptr) != false;
}

// No diagnostic on the empty arm - an action with no Function simply
// reports false, and only the first argument reaches the Function.
bool CallFunctionAction::execute(System* system, const Rect*, const NodeState*,
                                 const NodeInputEvent*)
{
    if (!m_function) return false;
    return m_function->call(system);
}
#if defined(_M_IX86)
static_assert(sizeof(CallFunctionAction) == 8, "client CallFunctionAction");
#endif
} }
#endif
