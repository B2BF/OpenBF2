// Combines commands assigned to the same HUD button slot instead of replacing the prior action.
#include "Bf2ActionListAction.h"
#if !DEDICATED
#include "../External/BFMeme/TypeDescriptor.h"
namespace dice { namespace meme {
Bf2ActionListAction::Bf2ActionListAction() {}             // 7CB910, base then vptr only
TypeDescriptor* Bf2ActionListAction::getClassType()
{
    static ClassTypeDescriptor<Bf2ActionListAction, ActionListAction>
        type("dice::meme::Bf2ActionListAction");
    return &type;
}
TypeDescriptor* Bf2ActionListAction::getType() { return getClassType(); }

// Every action runs; the reported value is the last one's, and an empty
// list reports false. No payload null guard, exactly as the base body.
bool Bf2ActionListAction::execute(System* system, const Rect* bounds,
                                  const NodeState* state, const NodeInputEvent* event)
{
    ObjectCollection* actions = &m_actions;
    bool result = false;
    for (ObjectCollectionPosition* position = actions->first(); position;
         position = actions->next(position))
    {
        Action* action = static_cast<Action*>(actions->getObject(position));
        result = action->execute(system, bounds, state, event);
    }
    return result;
}

// first() runs before the size test and its result is thrown away on
// every non-empty list; reproduced because it is a real virtual call.
bool Bf2ActionListAction::addAction(System* system, Action* action)
{
    ObjectCollection* actions = &m_actions;
    ObjectCollectionPosition* position = actions->first();
    if (actions->size() > 0) position = actions->last();
    return actions->insert(system, position, action);
}
#if defined(_M_IX86)
static_assert(sizeof(Bf2ActionListAction) == 24, "client Bf2ActionListAction");
#endif
} }
#endif
