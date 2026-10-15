// [INFERRED] path. Full primary94DB38, metadata94DAF8 and differing bodies
// read. ObjectList's complete22-slot implementation supplies real serialization.
#include "ActionListAction.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
ActionListAction::ActionListAction() : m_actions(Action::getClassType()) {} // 834DD1..DE0
ActionListAction::~ActionListAction() {}
TypeDescriptor* ActionListAction::getClassType()
{
    static ClassTypeDescriptor<ActionListAction, Action> type("dice::meme::ActionListAction");
    return &type;
}
TypeDescriptor* ActionListAction::getType() { return getClassType(); }
unsigned int ActionListAction::getObjectSize() { return sizeof(ActionListAction); }
namespace {
// CRT96E1C4 ->871490, descriptorA39FA0 and independent counterA39F9C.
TypeDescriptor* const registeredActionListActionType = ActionListAction::getClassType();
}
bool ActionListAction::streamProperties(System* system, Stream* stream)
{
    if (!Action::streamProperties(system, stream)) return false;
    return stream->streamCollection(system, &m_actions, "Action list") != false;
}
bool ActionListAction::execute(System* system, const Rect* bounds, const NodeState* state,
                               const NodeInputEvent* event)
{
    ObjectCollection* actions = &m_actions;
    for (ObjectCollectionPosition* position = actions->first(); position;
         position = actions->next(position))
    {
        // No payload null guard or alternate type conversion in the PE.
        Action* action = static_cast<Action*>(actions->getObject(position));
        if (action->execute(system, bounds, state, event)) return true;
    }
    return false;
}
#if defined(_M_IX86)
static_assert(sizeof(ActionListAction) == 24, "client ActionListAction");
#endif
} }
#endif
