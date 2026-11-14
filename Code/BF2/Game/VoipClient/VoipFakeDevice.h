// [CHOSEN, not recovered] A synthetic GameSpy Voice device for tests and gates (owner, 2026-11-13: the
// gates run without a microphone and without the macOS microphone permission prompt). It is the SDK's
// custom device with the two capture methods the BF2 client uses (gvCapturePacket and
// gvGetAvailableCaptureBytes), which the custom device leaves unsupported, served from a generated
// signal through the SDK's own custom capture path (threshold, hold, capture filter, encoder).
// Playback is drained at the same frame rate into the SDK source list, so talking state updates as
// with hardware. Nothing opens an audio device.
//
// VoipClient::initVoice uses it instead of gvListDevices/gvNewDevice when BF2_VOIP_CAPTURE=fake is
// set; the voice unit test installs it directly.
#ifndef BF2_GAME_VOIPCLIENT_VOIPFAKEDEVICE_H
#define BF2_GAME_VOIPCLIENT_VOIPFAKEDEVICE_H

#include <cstdint>

#include "VoipClient.h"

namespace dice {
namespace hfe {

namespace VoipFakeDevice {

// Millisecond clock the device paces itself by; VoipClient::tickCount unless a test installs one.
typedef std::uint32_t (*Clock)();
void setClock(Clock clock);

// The generated capture signal: a 300 Hz tone at the given 16-bit amplitude for burstMs, then
// silence for gapMs, repeating. Defaults: amplitude 12000, 1500 ms on, 1500 ms off.
void setSignal(int amplitude, std::uint32_t burstMs, std::uint32_t gapMs);

// True when BF2_VOIP_CAPTURE=fake.
bool requested();

// A started capture+playback device (GV_CAPTURE_AND_PLAYBACK); null if the SDK refuses.
VoipDevice create();

// Counters for tests.
int capturedFrames();     // frames handed to the capture path
int playedFrames();       // frames drained from playback

} // namespace VoipFakeDevice

} // namespace hfe
} // namespace dice

#endif
