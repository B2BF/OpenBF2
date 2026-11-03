// Abstract expression interface used by the otherwise unconstructed expression paths.
// [INFERRED] Header placement beside BAPAEvaluate, whose constructor names this type.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPEXPRESSION_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPEXPRESSION_H

namespace dice {
namespace hfe {
namespace ai {

class BAPExpression
{
public:
    // Deletion calls ia-32 +0x04 at ELF 0x1D1E09/0x24B89D/0x24B8A7;
    // amd-64 +0x08 at 0x23E32E/0x2A9870/0x2A985E establish the destructor pair.
    // Neither image emits a BAPExpression constructor, destructor, vtable or descendant RTTI.
    // Declaration only: there is no attested destructor body to implement.
    virtual ~BAPExpression();

    // [CHOSEN, not recovered] Name and void return under the unimplemented-slot exception.
    // No vtable exists in either ELF. Calls at ia-32 0x1D1E91/0x24B7CD/0x24B7D7
    // (+0x08), amd-64 0x23E1DB/0x2A97D0/0x2A97C0 (+0x10), pass only this
    // (stack / rdi) and discard the result. No expression instance is constructed.
    virtual void unimplementedExpressionSlot2() = 0;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif
