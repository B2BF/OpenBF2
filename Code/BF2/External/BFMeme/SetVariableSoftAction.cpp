// [INFERRED] path. Complete table94CF98, metadata94CD4C and every differing
// body/factory/eager instruction read.
#include "SetVariableSoftAction.h"
#if !DEDICATED
#include "Data.h"
#include "Node.h"
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
namespace {
// PE832A76..AD8 arithmetic, isolated to preserve x87 intermediates and the
// FST32-before-fcomp order. Names/helper boundary [INFERRED], native x86 only.
// Move toward target by delta*speed and clamp only on ordered overshoot.
// Equal/unordered initial samples perform no write; unordered candidates survive.
bool chooseSoftValue(float current, float target, float delta, float speed, float& value)
{
    float rounded;
    bool changed = false;
#if defined(_M_IX86)
    __asm {
        fld target
        fld current
        fcomp st(1)
        fnstsw ax
        test ah, 5
        jp decreasing
        fld delta
        fmul speed
        fadd current
        fst rounded
        fcomp st(1)
        fnstsw ax
        test ah, 41h
        jz use_target
    keep_candidate:
        fstp st(0)
        jmp write_value
    decreasing:
        fld current
        fcomp st(1)
        fnstsw ax
        test ah, 41h
        jnz unchanged
        fld delta
        fmul speed
        fsubr current
        fst rounded
        fcomp st(1)
        fnstsw ax
        test ah, 5
        jp keep_candidate
    use_target:
        fstp rounded
    write_value:
        mov changed, 1
        jmp done
    unchanged:
        fstp st(0)
    done:
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // An ordered current below the target moves up, above it moves down; equal or unordered
    // samples write nothing. Only an ordered overshoot of the unrounded candidate clamps.
    if (current < target) {
        const double candidate = static_cast<double>(delta) * speed + current;
        rounded = candidate > target ? target : static_cast<float>(candidate);
        changed = true;
    } else if (current > target) {
        const double candidate = current - static_cast<double>(delta) * speed;
        rounded = candidate < target ? target : static_cast<float>(candidate);
        changed = true;
    }
#endif
    if (changed) value = rounded;
    return changed;
}
}
SetVariableSoftAction::SetVariableSoftAction() : m_speed(100.0f) {} // 8331A0..AC
TypeDescriptor* SetVariableSoftAction::getClassType()
{
    static ClassTypeDescriptor<SetVariableSoftAction, SetVariableAction>
        type("dice::meme::SetVariableSoftAction");
    return &type;
}
TypeDescriptor* SetVariableSoftAction::getType() { return getClassType(); }
unsigned int SetVariableSoftAction::getObjectSize() { return sizeof(SetVariableSoftAction); }
namespace {
// CRT96E17C ->870F70; same descriptorA39DA4, independent counterA39D44.
TypeDescriptor* const registeredSetVariableSoftActionType = SetVariableSoftAction::getClassType();
}
bool SetVariableSoftAction::streamProperties(System* system, Stream* stream)
{
    if (!SetVariableAction::streamProperties(system, stream)) return false;
    return stream->streamFloat(&m_speed, "Speed") != false;
}
bool SetVariableSoftAction::execute(System* system, const Rect*, const NodeState*,
                                    const NodeInputEvent* event)
{
    if (m_variable && m_data && event->type == 22)
    {
        const float delta = event->first; // Snapshot before either callback; no event null guard.
        const float current = m_variable->getFloat(system);
        const float target = m_data->getFloat(system); // Reload source after first callback.
        float value;
        if (chooseSoftValue(current, target, delta, m_speed, value))
            m_variable->setFloat(system, value); // Fresh receiver and table, unlike base execute.
    }
    return false;
}
#if defined(_M_IX86)
static_assert(sizeof(SetVariableSoftAction) == 16, "client SetVariableSoftAction");
static_assert(sizeof(ClassTypeDescriptor<SetVariableSoftAction, SetVariableAction>) == 16,
              "SetVariableSoftAction metadata");
#endif
} }
#endif
