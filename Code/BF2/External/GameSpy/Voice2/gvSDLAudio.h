/* [CHOSEN, not recovered] SDL3 Voice2 hardware integration, issue #36. */
#ifndef GV_SDL_AUDIO_H
#define GV_SDL_AUDIO_H
#include "gvMain.h"
GVBool gviHardwareStartup(void);
void gviHardwareCleanup(void);
void gviHardwareThink(void);
void gviHardwarePrepareCodecChange(void);
int gviHardwareListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types);
GVDevice gviHardwareNewDevice(GVDeviceID deviceID, GVDeviceType type);
#endif
