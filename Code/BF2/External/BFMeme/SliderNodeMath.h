// Path and names [INFERRED]. Arithmetic shared by SliderNode (BF2.exe 0x833660..) and its clone
// BfFixedSliderNode (0x7D3D50..): retail repeats each of these inline or as small helpers in both.
// x87 intermediates are carried in double; stores are float where the shipped code stores.
#ifndef BFMEME_SLIDERNODEMATH_H
#define BFMEME_SLIDERNODEMATH_H
#include "Node.h"
#include "Style.h"   // Rect
#include "System.h"
#if !DEDICATED
namespace dice { namespace meme { namespace slider {
// The span every body re-derives with one `fcomp`: only a maximum strictly below the minimum
// takes the reversed subtraction, so NaN falls with the normal case.
inline double orderedSpan(float minimum, float maximum)
{
    return maximum < minimum ? static_cast<double>(minimum) - maximum
                             : static_cast<double>(maximum) - minimum;
}
// The negative-encoded NodeState value (`fadd 1.0; fchs` on an ordered negative), as
// TransformNode's copy; zero, positive and NaN pass through.
inline float decodedState(float value)
{
    return static_cast<float>(value < 0.0f ? -(static_cast<double>(value) + 1.0) : value);
}
// 838F40: the transition rate a child context carries (TransformNode's stateRate).
inline float stateRate(const NodeState& state)
{
    const float current = decodedState(state.current);
    const float target = decodedState(state.target);
    if (target < current) return -3.0f;
    if (target > current) return 3.0f;
    return 0.0f;
}
// 838EF0: a press is held - the pointer is locked (6CFB80 nonzero) and the node's current
// state is ordered-negative (the pressed encoding).
inline bool pressHeld(const NodeState* state, System* system)
{
    return system->getLockCount() != 0 && state->current < 0.0f;
}
// 74DBA0: clamps a point into the rectangle, far edges included; each test is the shipped
// `fcomp`/`jp` pair, so an unordered coordinate is left untouched.
inline void clampToRect(float point[2], const Rect* rect)
{
    if (point[0] < rect->x) point[0] = rect->x;
    else {
        const float right = rect->width + rect->x;
        if (right < point[0]) point[0] = right;
    }
    if (point[1] < rect->y) point[1] = rect->y;
    else {
        const float bottom = rect->height + rect->y;
        if (bottom < point[1]) point[1] = bottom;
    }
}
// The pointer-over-thumb test of update(): edges included, unordered coordinates pass.
inline bool pointerInside(const float* pointer, const Rect& thumb)
{
    return !(pointer[0] < thumb.x) && !(pointer[1] < thumb.y)
        && !(static_cast<double>(thumb.width) + thumb.x < pointer[0])
        && !(static_cast<double>(thumb.height) + thumb.y < pointer[1]);
}
} } }
#endif
#endif
