// The game's voice client: VoipClient bound to the local player, the squad/commander channels, push
// to talk, the anti-spam mute and the menu's AudioSettings. BF2.exe 0x4510F0..0x45235C, vtable
// 0x89C1A8 (64 slots), ctor 0x4519B0, 0xC8 bytes. Created by BF2Engine::initEngine (0x40C351) and
// registered as the "Bf2VoipClient" singleton (ds:0x9A4DA0); SwiffPlayer reaches it through
// getSingleton and calls these slots across the DLL boundary, so their order is ABI.
#ifndef BF2_GAME_VOIPCLIENT_BF2VOIPCLIENT_H
#define BF2_GAME_VOIPCLIENT_BF2VOIPCLIENT_H

#include "VoipClient.h"

namespace dice {
namespace hfe {

class BF2VoipClient : public VoipClient
{
public:
    BF2VoipClient();                                                    // 0x4519B0
    virtual ~BF2VoipClient();                                           // +0x00 0x451DB0

    // Overrides of the VoipClient slots.
    virtual bool startSession(std::uint32_t address, std::uint16_t port, std::uint16_t localPort,
                              int quality, bool useMixer);              // +0x2C 0x451B60
    virtual bool setEnabled(bool enabled);                              // +0x50 0x451160
    virtual bool handleIncoming(std::uint8_t* packet, int length, std::uint16_t* sender);  // +0x70 0x451640
    virtual bool think(int* receiveError, int* sendError);              // +0x74 0x451420
    virtual void onCaptureLevel(double level);                          // +0x7C 0x4516D0
    virtual void onVoiceEnded();                                        // +0x80 0x4517C0
    virtual void onIncomingHandled(std::uint16_t* sender);              // +0x84 0x4517F0

    // BF2VoipClient's own slots.
    virtual bool setUserDisabled(bool disabled);                        // +0x98 0x4511D0
    // [INFERRED] name. A slot of its own: VoipClient::update (+0x64) stays the base body, which
    // SwiffPlayer ticks for the menu's voice test.
    virtual bool updateClient();                                        // +0x9C 0x451E00
    virtual bool getUsingLeaderChannel();                               // +0xA0 0x4B72C0 (+0xA0)
    virtual bool updateLeaderChannel(bool pressed);                     // +0xA4 0x4512A0
    virtual bool getPushToTalkEnabled();                                // +0xA8 0x4B7300 (+0xA2)
    virtual void setPushToTalkEnabled(bool enabled);                    // +0xAC 0x4B7310
    virtual bool getPushToTalkPressed();                                // +0xB0 0x4B72E0 (+0xA1)
    virtual bool setPushToTalk(bool pressed);                           // +0xB4 0x451360
    virtual void setLocalMuted(bool muted);                             // +0xB8 0x451820
    virtual void muteFor(double seconds);                               // +0xBC 0x451880
    virtual void slotC0(int, int);                                      // +0xC0 0x5A59F0, empty
    // +0xC4..+0xE0: thin forwards to the microphone mixer singleton (BF2.exe 0x451A90..0x451B4E).
    virtual bool mixerOpen();                                           // +0xC4 -> 0x736BD0
    virtual bool mixerHasBoost();                                       // +0xC8 -> 0x735A40(null)
    virtual bool mixerSetBoost(bool enabled);                           // +0xCC -> 0x735BB0
    virtual bool mixerSetCaptureVolume(float volume);                   // +0xD0 -> 0x735CD0
    virtual bool mixerGetBoost(bool* enabled);                          // +0xD4 -> 0x735D00
    // 0x451B10 takes the float by value and hands the mixer the address of its own argument slot,
    // so the volume read is lost and only the success flag reaches the caller. Reproduced.
    virtual bool mixerGetCaptureVolume(float volume);                   // +0xD8 -> 0x735DD0
    virtual bool mixerSelectMicrophone();                               // +0xDC -> 0x735EA0
    virtual bool mixerSetMonitorMute(bool muted);                       // +0xE0 -> 0x7361E0
    virtual bool isBusy();                                              // +0xE4 0x451A70
    virtual void onDeviceUnplugged(VoipDevice device);                    // +0xE8 0x451940
    virtual bool squadLeaderChannel(bool pressed);                      // +0xEC 0x4513D0
    virtual bool commanderChannel(bool pressed);                        // +0xF0 0x452250
    virtual void updateTalkDucking();                                   // +0xF4 0x4518D0
    virtual bool captureBacklogLow();                                   // +0xF8 0x451610
    virtual bool canTransmit();                                         // +0xFC 0x4518B0

    // The gvFilterCallback on the capture device (0x4510F0) and the unplugged callback (0x451920).
    // The types are gv.h's GVDevice, GVSample (gsi_i16) and GVFrameStamp (gsi_u16).
    static void captureFilter(VoipDevice device, std::int16_t* audio, std::uint16_t frameStamp);
    static void unpluggedCallback(VoipDevice device);

    bool   m_usingLeaderChannel;    // +0xA0
    bool   m_pushToTalkPressed;     // +0xA1
    bool   m_pushToTalkEnabled;     // +0xA2 initial 1
    bool   m_localMuted;            // +0xA3
    double m_talkLimit;             // +0xA8 10.0 seconds of continuous talk before the anti-spam mute
    double m_mutedAt;               // +0xB0 simulation time of setLocalMuted(true), 0 otherwise
    double m_muteDuration;          // +0xB8 5.0
    bool   m_noSquadsSelected;      // +0xC0 commander on the map with no squad selected
    bool   m_userDisabled;          // +0xC1
    bool   m_internetServer;        // +0xC2 initial 1; gameServer->getIsInternetServer at session start

    static VoipBandPassFilter* s_captureFilter;                         // ds:0x9A4DA4
};
#if defined(_M_IX86)
static_assert(sizeof(BF2VoipClient) == 0xC8, "BF2VoipClient must keep the retail x86 allocation size");
#endif

// ds:0x9A4DA0, the "Bf2VoipClient" singleton slot (defined in Game/Main/BF2Engine.cpp).
extern BF2VoipClient* bf2VoipClient;

} // namespace hfe
} // namespace dice

#endif
