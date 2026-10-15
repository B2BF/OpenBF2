// HUD function action borrowing its Function; destruction does not delete the target.
#include "Bf2CallFunctionAction.h"
#if !DEDICATED
#include "../External/BFMeme/TypeDescriptor.h"
namespace dice { namespace meme {
Bf2CallFunctionAction::Bf2CallFunctionAction() {}         // 7CFF80, base then vptr only
TypeDescriptor* Bf2CallFunctionAction::getClassType()
{
    static ClassTypeDescriptor<Bf2CallFunctionAction, CallFunctionAction>
        type("dice::meme::Bf2CallFunctionAction");
    return &type;
}
TypeDescriptor* Bf2CallFunctionAction::getType() { return getClassType(); }
bool Bf2CallFunctionAction::setFunction(Function* function)
{
    m_function = function;
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(Bf2CallFunctionAction) == 8, "client Bf2CallFunctionAction");
#endif
} }
#endif
