// [TEMP] [CHOSEN, not recovered] Mac fake-SDK check only (bf2_falcon_update_check): the four
// Win32 names FalconDevice.cpp and NovintHFX.h use. The check defines the two functions.
#ifndef BF2_TESTS_NOVINT_FAKE_WINDOWS_H
#define BF2_TESTS_NOVINT_FAKE_WINDOWS_H
typedef unsigned int DWORD;
typedef void* HWND;
extern "C" DWORD GetTickCount();
extern "C" HWND GetForegroundWindow();
#endif
