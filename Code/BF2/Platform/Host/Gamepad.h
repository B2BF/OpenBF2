// [CHOSEN, not recovered] Polled native gamepads, implemented by GameController or SDL.
// Shared device contract under issue #36. Main thread only.

#ifndef PLATFORM_HOST_GAMEPAD_H
#define PLATFORM_HOST_GAMEPAD_H

namespace dice {
namespace hfe {
namespace host {

// Button bits, in the order DirectInput enumerates an Xbox 360 / Xbox One controller.
enum GamepadButton
{
    GamepadA = 0, GamepadB = 1, GamepadX = 2, GamepadY = 3,
    GamepadLeftShoulder = 4, GamepadRightShoulder = 5,
    GamepadView = 6,                // Back / Select / Options
    GamepadMenu = 7,                // Start
    GamepadLeftThumb = 8, GamepadRightThumb = 9,
    GamepadButtonCount = 10
};

// One sampled controller state. Sticks are -1..1 with y positive UP (GameController
// convention); triggers 0..1. dpad is a DirectInput POV value: -1 centred, otherwise the
// direction in hundredths of a degree clockwise from up (0, 4500, ..., 31500).
struct GamepadState
{
    bool         connected;
    float        leftX, leftY;
    float        rightX, rightY;
    float        leftTrigger, rightTrigger;
    unsigned int buttons;           // 1 << GamepadButton
    int          dpad;
};

// Number of connected controllers with an extended gamepad profile.
int gamepadCount();

// Samples controller index (0-based, in the backend's connected-controller order).
// Returns false and clears state when that controller is not connected.
bool readGamepad(int index, GamepadState& state);

// Opaque identity of controller index, stable while it stays connected; 0 when absent. Lets a
// device notice that its controller was unplugged and another took its index.
const void* gamepadIdentity(int index);

} // namespace host
} // namespace hfe
} // namespace dice

#endif // PLATFORM_HOST_GAMEPAD_H
