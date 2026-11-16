// [CHOSEN, not recovered] macOS port: GameController polling. See Platform/Host/Gamepad.h.

#include "../Host/Gamepad.h"

#import <GameController/GameController.h>

#if !__has_feature(objc_arc)
#error "MacGamepad.mm must be compiled with -fobjc-arc"
#endif

namespace dice {
namespace hfe {
namespace host {

namespace {

GCController* extendedController(int index)
{
    if (index < 0)
        return nil;
    int found = 0;
    for (GCController* controller in [GCController controllers])
    {
        if (!controller.extendedGamepad)
            continue;
        if (found == index)
            return controller;
        ++found;
    }
    return nil;
}

int povFromDpad(GCControllerDirectionPad* dpad)
{
    const bool up = dpad.up.pressed, down = dpad.down.pressed;
    const bool left = dpad.left.pressed, right = dpad.right.pressed;
    if (up && right)   return 4500;
    if (right && down) return 13500;
    if (down && left)  return 22500;
    if (left && up)    return 31500;
    if (up)            return 0;
    if (right)         return 9000;
    if (down)          return 18000;
    if (left)          return 27000;
    return -1;
}

} // namespace

int gamepadCount()
{
    int count = 0;
    for (GCController* controller in [GCController controllers])
        if (controller.extendedGamepad)
            ++count;
    return count;
}

bool readGamepad(int index, GamepadState& state)
{
    state = GamepadState();
    state.dpad = -1;
    GCController* controller = extendedController(index);
    GCExtendedGamepad* pad = controller.extendedGamepad;
    if (!pad)
        return false;
    state.connected    = true;
    state.leftX        = pad.leftThumbstick.xAxis.value;
    state.leftY        = pad.leftThumbstick.yAxis.value;
    state.rightX       = pad.rightThumbstick.xAxis.value;
    state.rightY       = pad.rightThumbstick.yAxis.value;
    state.leftTrigger  = pad.leftTrigger.value;
    state.rightTrigger = pad.rightTrigger.value;
    unsigned int buttons = 0;
    if (pad.buttonA.pressed)                 buttons |= 1u << GamepadA;
    if (pad.buttonB.pressed)                 buttons |= 1u << GamepadB;
    if (pad.buttonX.pressed)                 buttons |= 1u << GamepadX;
    if (pad.buttonY.pressed)                 buttons |= 1u << GamepadY;
    if (pad.leftShoulder.pressed)            buttons |= 1u << GamepadLeftShoulder;
    if (pad.rightShoulder.pressed)           buttons |= 1u << GamepadRightShoulder;
    if (pad.buttonOptions.pressed)           buttons |= 1u << GamepadView;
    if (pad.buttonMenu.pressed)              buttons |= 1u << GamepadMenu;
    if (pad.leftThumbstickButton.pressed)    buttons |= 1u << GamepadLeftThumb;
    if (pad.rightThumbstickButton.pressed)   buttons |= 1u << GamepadRightThumb;
    state.buttons = buttons;
    state.dpad = povFromDpad(pad.dpad);
    return true;
}

const void* gamepadIdentity(int index)
{
    return (__bridge const void*)extendedController(index);
}

} // namespace host
} // namespace hfe
} // namespace dice
