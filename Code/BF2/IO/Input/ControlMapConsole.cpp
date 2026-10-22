// Console bindings for the active control map and its manager. File split [INFERRED].
// The console checks object activity before invoking executors; executors require a live target.
// Only create and both setAxisScale overloads are available in the dedicated image.
// Preserve command spelling, omitted-argument defaults and the declared overload order.
// [PLATFORM] manager names are passed by value in PE and by const reference in ELF;
// these typed bindings use the ELF declarations.

#include "../../Core/BuildConfig.h"

#include "../../Core/ConsoleObjectMacro.h"
#include "Input.h"

#include <string>

namespace dice {
namespace hfe {
namespace io {

extern IBase* controlMapManager;                       // Core/Globals.cpp; PE ds:0x9FFCB4
extern IBase* inputDeviceManager;                      // Core/Globals.cpp; PE ds:0x9FFCB0

namespace {

const char* const kControlMapConsoleTag = "io.ControlMap";

// The two accessors the executors reach the engine through. Neither guards anything - see above.
IControlMapManager* manager()
{
    return static_cast<IControlMapManager*>(static_cast<void*>(controlMapManager));
}
IControlMap* activeMap()                               // 0x68F80F: `controlMapManager->[+0x34]()`
{
    return manager()->getActive();
}

// Sets an input device's axis scale. Returns false when the device is unavailable.
// The returned device must support the advanced input interface.
bool consoleSetDeviceAxisScale(InputDeviceFlags device, InputDeviceAxes axis, float scale)
{
    IInputDevice* dev = inputDeviceManager
        ? static_cast<IInputDeviceManager*>(static_cast<void*>(inputDeviceManager))->getInputDevice(device)
        : 0;
    if (!dev)
        return false;
    static_cast<IAdvancedInputDevice*>(dev)->setAxisScale(axis, scale);
    return true;
}

// The three manager rows (guard 0x695810)
void consoleCreate(const std::string& name)           { manager()->create(name); }            // +0x14
void consoleSetActive(const std::string& name)        { manager()->setActive(name); }         // +0x30 (MSVC: the string overload)
void consoleDeleteControlMap(const std::string& name) { manager()->deleteControlMap(name); }  // +0x28 (MSVC: the string overload)

// The map rows with a fixed arity (guard 0x695E0C)
void consoleDump()                                    { activeMap()->dump(true); }            // +0xB4, literal 1
void consoleSetButtonRiseTime(float t)                { activeMap()->setButtonRiseTime(t); }  // +0x5C
void consoleSetButtonFallTime(float t)                { activeMap()->setButtonFallTime(t); }  // +0x60
void consoleSetDoubleTapTime(float t)                 { activeMap()->setDoubleTapTime(t); }   // +0x64
void consoleSetAxisScale(InputDeviceAxes axis, float scale) { activeMap()->setAxisScale(axis, scale); } // +0x98
void consoleSetYawFactor(float f)                     { activeMap()->setYawFactor(f); }       // +0xA0
void consoleSetPitchFactor(float f)                   { activeMap()->setPitchFactor(f); }     // +0xA4

// The three properties: get on argc 0 (hasReturnValue true), set on argc 1
bool  consoleGetInvertMouse()                         { return activeMap()->getInvertMouse(); }          // +0x84
void  consoleSetInvertMouse(bool invert)              { activeMap()->setInvertMouse(invert); }           // +0x80
float consoleGetMouseSensitivity()                    { return activeMap()->getMouseSensitivity(); }     // +0x8C
void  consoleSetMouseSensitivity(float s)             { activeMap()->setMouseSensitivity(s); }           // +0x88
float consoleGetKeyboardSensitivity()                 { return activeMap()->getKeyboardSensitivity(); }  // +0x94
void  consoleSetKeyboardSensitivity(float s)          { activeMap()->setKeyboardSensitivity(s); }        // +0x90

// The eight mapping rows. Each names the ELF-order overload its MSVC slot resolves to.
// 0x695F47 -> +0x3C = addAxisMapping(int, Flags, Axes, bool, bool)                 [ELF +0x2C]
void consoleAddAxisToAxisMapping(int axisId, InputDeviceFlags device, InputDeviceAxes axis, bool a, bool b)
{ activeMap()->addAxisMapping(axisId, device, axis, a, b); }
// Maps the first button to positive input and the second to negative input.
void consoleAddButtonsToAxisMapping(int axisId, InputDeviceFlags device, InputDeviceButtons positive, InputDeviceButtons negative, bool a)
{ activeMap()->addAxisMapping(axisId, device, positive, negative, a); }
// Maps the first key to positive input and the second to negative input.
void consoleAddKeysToAxisMapping(int axisId, InputDeviceFlags device, InputDeviceKeys positive, InputDeviceKeys negative, bool a)
{ activeMap()->addAxisMapping(axisId, device, positive, negative, a); }
// The console parses the fourth token as a key/button enum, then passes its value as device flags.
// Preserve this existing descriptor mismatch when forwarding mixed-device bindings.
void consoleAddKeyAndButtonToAxisMapping(int axisId, InputDeviceFlags device, InputDeviceKeys key, InputDeviceKeys device2, InputDeviceButtons button, bool a)
{ activeMap()->addAxisMapping(axisId, device, key, static_cast<InputDeviceFlags>(device2), button, a); }
void consoleAddButtonAndKeyToAxisMapping(int axisId, InputDeviceFlags device, InputDeviceButtons button, InputDeviceButtons device2, InputDeviceKeys key, bool a)
{ activeMap()->addAxisMapping(axisId, device, button, static_cast<InputDeviceFlags>(device2), key, a); }
// 0x696D8C -> +0x4C = addTriggerMapping(int, Flags, Buttons, float, bool)          [ELF +0x44]
void consoleAddButtonToTriggerMapping(int triggerId, InputDeviceFlags device, InputDeviceButtons button, float f, bool b)
{ activeMap()->addTriggerMapping(triggerId, device, button, f, b); }
// 0x697036 -> +0x48 = addTriggerMapping(int, Flags, Keys, float, bool)             [ELF +0x48]
void consoleAddKeyToTriggerMapping(int triggerId, InputDeviceFlags device, InputDeviceKeys key, float f, bool b)
{ activeMap()->addTriggerMapping(triggerId, device, key, f, b); }
// 0x6975EF -> +0x44 = addTriggerMapping(int, int, Flags, Axes, bool, float, float) [ELF +0x4C]
// Both executor arms push two `fldz` for the last two floats; only the bool is ever parsed.
void consoleAddAxisToTriggerMapping(int triggerId, int secondaryId, InputDeviceFlags device, InputDeviceAxes axis, bool b)
{ activeMap()->addTriggerMapping(triggerId, secondaryId, device, axis, b, 0.0f, 0.0f); }

} // namespace

// `ControlMap` - constructors 0x69567A..0x6978B5 in BF2.exe, defaults at 0x85F890 (access 1,
// category 0). Rows in the binary's constructor order.
BF2_CONSOLE_OBJECT("ControlMap", 1, 0)

// ctor 0x69567A, exec 0x6956EB. argc exactly 1. Registered in both images (ELF 0x8506D93).
BF2_CONSOLE_METHOD_1_ACTIVE(create, consoleCreate, controlMapManager != 0,
                            std::string, kControlMapConsoleTag)
// The active map's own axis-scale override (PE ctor 0x649F8A, metadata io::InputDeviceAxes/float,
// return void). Registered in both images.
BF2_CONSOLE_METHOD_2_ACTIVE(setAxisScale, consoleSetAxisScale, activeMap() != 0,
                            io::InputDeviceAxes, float, kControlMapConsoleTag)

// `InputDevices` - SetConsoleClassDefaults 0x770F56 ("InputDevices", access 1, category 0). The
// device-axis-scale command (PE ctor 0x64831F, argc 3, return bool) lives on this object, not on
// ControlMap; it is registered in both images.
BF2_CONSOLE_OBJECT("InputDevices", 1, 0)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(setAxisScale, consoleSetDeviceAxisScale, bool, true,
                                InputDeviceFlags, InputDeviceAxes, float, kControlMapConsoleTag)

#if !DEDICATED
BF2_CONSOLE_OBJECT("ControlMap", 1, 0)
// ctor 0x69579F, exec 0x69581C.
BF2_CONSOLE_METHOD_1_ACTIVE(setActive, consoleSetActive, controlMapManager != 0,
                            std::string, kControlMapConsoleTag)
// ctor 0x6958D0, exec 0x695941.
BF2_CONSOLE_METHOD_1_ACTIVE(deleteControlMap, consoleDeleteControlMap, controlMapManager != 0,
                            std::string, kControlMapConsoleTag)
// ctor 0x6959F5, exec 0x695A47. argc exactly 0; the executor passes a literal 1 to dump(bool).
BF2_CONSOLE_METHOD_0_ACTIVE(dump, consoleDump, activeMap() != 0, kControlMapConsoleTag)
// ctor 0x695A68, exec 0x695B26.
BF2_CONSOLE_METHOD_1_ACTIVE(setButtonRiseTime, consoleSetButtonRiseTime, activeMap() != 0,
                            float, kControlMapConsoleTag)
// ctor 0x695B4B, exec 0x695C09.
BF2_CONSOLE_METHOD_1_ACTIVE(setButtonFallTime, consoleSetButtonFallTime, activeMap() != 0,
                            float, kControlMapConsoleTag)
// ctor 0x695C2E, exec 0x695CEC.
BF2_CONSOLE_METHOD_1_ACTIVE(setDoubleTapTime, consoleSetDoubleTapTime, activeMap() != 0,
                            float, kControlMapConsoleTag)

// ctor 0x695D11, exec 0x695F47. argTypes {int, InputDeviceFlags, InputDeviceAxes, bool, bool}.
BF2_CONSOLE_METHOD_4_5_ACTIVE(addAxisToAxisMapping, consoleAddAxisToAxisMapping, activeMap() != 0,
                              int, InputDeviceFlags, InputDeviceAxes, bool, bool, false,
                              kControlMapConsoleTag)
// ctor 0x695FC8, exec 0x6961F2. {int, InputDeviceFlags, InputDeviceButtons, InputDeviceButtons, bool}.
BF2_CONSOLE_METHOD_4_5_ACTIVE(addButtonsToAxisMapping, consoleAddButtonsToAxisMapping, activeMap() != 0,
                              int, InputDeviceFlags, InputDeviceButtons, InputDeviceButtons, bool, false,
                              kControlMapConsoleTag)
// ctor 0x696273, exec 0x6964F9. {int, InputDeviceFlags, InputDeviceKeys, InputDeviceKeys, InputDeviceButtons, bool}.
BF2_CONSOLE_METHOD_5_6_ACTIVE(addKeyAndButtonToAxisMapping, consoleAddKeyAndButtonToAxisMapping, activeMap() != 0,
                              int, InputDeviceFlags, InputDeviceKeys, InputDeviceKeys, InputDeviceButtons, bool, false,
                              kControlMapConsoleTag)
// ctor 0x696591, exec 0x696817. {int, InputDeviceFlags, InputDeviceButtons, InputDeviceButtons, InputDeviceKeys, bool}.
BF2_CONSOLE_METHOD_5_6_ACTIVE(addButtonAndKeyToAxisMapping, consoleAddButtonAndKeyToAxisMapping, activeMap() != 0,
                              int, InputDeviceFlags, InputDeviceButtons, InputDeviceButtons, InputDeviceKeys, bool, false,
                              kControlMapConsoleTag)
// ctor 0x6968AF, exec 0x696AD9. {int, InputDeviceFlags, InputDeviceKeys, InputDeviceKeys, bool}.
BF2_CONSOLE_METHOD_4_5_ACTIVE(addKeysToAxisMapping, consoleAddKeysToAxisMapping, activeMap() != 0,
                              int, InputDeviceFlags, InputDeviceKeys, InputDeviceKeys, bool, false,
                              kControlMapConsoleTag)
// ctor 0x696B5A, exec 0x696D8C. {int, InputDeviceFlags, InputDeviceButtons, float, bool}.
BF2_CONSOLE_METHOD_4_5_ACTIVE(addButtonToTriggerMapping, consoleAddButtonToTriggerMapping, activeMap() != 0,
                              int, InputDeviceFlags, InputDeviceButtons, float, bool, false,
                              kControlMapConsoleTag)
// Preserve the command's capital A; lookup is case-insensitive.
BF2_CONSOLE_METHOD_4_5_ACTIVE(AddKeyToTriggerMapping, consoleAddKeyToTriggerMapping, activeMap() != 0,
                              int, InputDeviceFlags, InputDeviceKeys, float, bool, false,
                              kControlMapConsoleTag)

// ctor 0x6971F6, exec 0x6972B4.
BF2_CONSOLE_METHOD_1_ACTIVE(setYawFactor, consoleSetYawFactor, activeMap() != 0,
                            float, kControlMapConsoleTag)
// ctor 0x6972DC, exec 0x69739A.
BF2_CONSOLE_METHOD_1_ACTIVE(setPitchFactor, consoleSetPitchFactor, activeMap() != 0,
                            float, kControlMapConsoleTag)
// ctor 0x6973C2, exec 0x6975EF. {int, int, InputDeviceFlags, InputDeviceAxes, bool}.
BF2_CONSOLE_METHOD_4_5_ACTIVE(addAxisToTriggerMapping, consoleAddAxisToTriggerMapping, activeMap() != 0,
                              int, int, InputDeviceFlags, InputDeviceAxes, bool, false,
                              kControlMapConsoleTag)

// ctor 0x69768A, exec 0x697748 - type 1, argc 0..1, returnType "bool". dice's capital I is kept.
BF2_CONSOLE_PROPERTY_FN(InvertMouse, bool, activeMap() != 0,
                        consoleGetInvertMouse(), consoleSetInvertMouse, kControlMapConsoleTag)
// ctor 0x6977A3, exec 0x697861 - type 1, argc 0..1, returnType "float".
BF2_CONSOLE_PROPERTY_FN(mouseSensitivity, float, activeMap() != 0,
                        consoleGetMouseSensitivity(), consoleSetMouseSensitivity, kControlMapConsoleTag)
// ctor 0x6978B5, exec 0x697973 - type 1, argc 0..1, returnType "float".
BF2_CONSOLE_PROPERTY_FN(keyboardSensitivity, float, activeMap() != 0,
                        consoleGetKeyboardSensitivity(), consoleSetKeyboardSensitivity, kControlMapConsoleTag)
#endif // !dedicated

// Handed to the console registry by ModuleIO::init (Core/ModuleRegistry.cpp), beside the
// FileManager list from the same retail translation unit.
ConsoleObjectList& controlMapConsoleObjects()
{
    return consoleBindingList(kControlMapConsoleTag);
}

} // namespace io
} // namespace hfe
} // namespace dice
