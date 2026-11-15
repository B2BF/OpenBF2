// [CHOSEN, not recovered] Optional native layout policy; no object or vtable layout changes.
// https://github.com/kr4uzi/OpenBF2/issues/33
#ifndef BFMEME_TRANSFORMLAYOUT_H
#define BFMEME_TRANSFORMLAYOUT_H
#include "../../Core/BuildConfig.h"
#if !DEDICATED
#include "Style.h"
#include "../../Math/Vec3.h"
namespace dice { namespace meme {
class Node;
class System;
typedef void (*TransformBoundsFilter)(System*, Node*, Rect&);
extern TransformBoundsFilter transformBoundsFilter;

// Compute the same bounds for painting, input and traversal, without changing authored geometry.
Rect layoutTransformBounds(System*, Node*, const Rect& parent,
                           const hfe::Vec2& position, const hfe::Vec2& size);
} }
#endif
#endif
