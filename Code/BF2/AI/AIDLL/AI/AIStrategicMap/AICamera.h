// Camera access for the AI. Placement next to TemperatureTreeModifiers is [INFERRED] from the
// image's address order; whether AICamera is a class or a namespace is not attested by the mangled
// name, and a class with a static member is [CHOSEN].
#ifndef AI_AIDLL_AI_AISTRATEGICMAP_AICAMERA_H
#define AI_AIDLL_AI_AISTRATEGICMAP_AICAMERA_H

#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace world {
class IPlayer;
}
namespace ai {

class AICamera
{
public:
    // Copies the transformation of the player's current camera (taken at time 0) into `transform`.
    // False, with `transform` untouched, when the player has no camera.
    static bool GetCameraTrans(const world::IPlayer& player, Mat4& transform);
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICMAP_AICAMERA_H
