// [TEMP] [CHOSEN, not recovered] Opt-in native client captures for issue #36.
// [RETIRE-WHEN: client-test-automation-replaced] Retain until the shared scenario
// runner supplies equivalent labelled captures and timing records.
#pragma once
struct IDirect3DSwapChain9;

// Saves a pending shared input-script capture when BF2_CAPTURE_DIR is configured.
// Called before Present so discard swap chains still contain the rendered frame.
void bf2LinuxCaptureBeforePresent(IDirect3DSwapChain9* chain);
