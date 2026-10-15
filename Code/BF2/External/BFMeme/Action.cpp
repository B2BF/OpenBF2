// Full17-slot table94C9F8 and descriptor94F2F8 read. Four-argument false
// default7960B0 is shipped behavior, not a frontier success substitute.
#include "Action.h"
#if !DEDICATED
#include "TypeDescriptor.h"
namespace dice { namespace meme {
TypeDescriptor* Action::getClassType()
{
    static ClassTypeDescriptor<Action, Object> type("dice::meme::Action");
    return &type;
}
TypeDescriptor* Action::getType() { return getClassType(); }
unsigned int Action::getObjectSize() { return sizeof(Action); }
bool Action::execute(System*, const Rect*, const NodeState*, const NodeInputEvent*) { return false; }
#if defined(_M_IX86)
static_assert(sizeof(Action) == 4, "BFMeme Action layout");
#endif
} }
#endif
