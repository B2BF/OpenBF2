// Engine-facing view of the menu player plugin.
#ifndef MENU_ISWIFFPLAYER_H
#define MENU_ISWIFFPLAYER_H
#include "../Core/IBase.h"
#include <stdint.h>
#include <string>
#if !DEDICATED
namespace dice { namespace hfe { namespace ui {

struct ISwiffRender;
struct ISwiffHost;

class ISwiffPlayer : public IBase
{
public:
    virtual void  setScale(float xScale, float yScale) = 0;                //  4 +0x10  init / onResize
    virtual bool  loadMovie(const char* movieName) = 0;                    //  5 +0x14  init: "mainMenu.swf"
    virtual void  update(float deltaTime) = 0;                             //  6 +0x18  MainMenuLogic::update
    virtual void  display() = 0;                                           //  7 +0x1C  MainMenuLogic::display
    virtual bool  isMovieLoaded() = 0;                                     //  8 +0x20  MainMenuLogic::beginFrame
    virtual void  setRender(ISwiffRender* render) = 0;                     //  9 +0x24  init
    virtual ISwiffRender* getRender() = 0;                                 // 10 +0x28
    virtual void  setMousePositionDeferred(int x, int y) = 0;              // 11 +0x2C
    virtual void  queueMouseState(int x, int y, int buttons) = 0;          // 12 +0x30  the mouse slots
    virtual void  notifyKey(int code, int down) = 0;                       // 13 +0x34
    virtual void  reserved14(int a) = 0;                                   // 14 +0x38  shipped nullsub_20
    virtual void  reserved15(int a) = 0;                                   // 15 +0x3C  shipped nullsub_21
    virtual void  notifyChar(int character) = 0;                           // 16 +0x40  handleInput
    virtual void  notifyKeyState(int code, int down) = 0;                  // 17 +0x44  handleInput
    virtual void  resetModifierState() = 0;                                // 18 +0x48
    virtual void  setTextInput(const char* text) = 0;                      // 19 +0x4C  the clipboard paste
    virtual void  getTextInput0(std::string& out) = 0;                     // 20 +0x50  the clipboard copy
    virtual void  getTextInput1(std::string& out) = 0;                     // 21 +0x54  the clipboard cut
    virtual void  setSwiffHost(ISwiffHost* host) = 0;                      // 22 +0x58  init
    virtual ISwiffHost* getSwiffHost() = 0;                                // 23 +0x5C  onResize
    virtual char  renderCapability0(intptr_t a) = 0;                            // 24 +0x60  SwiffHost 0x486E10
    virtual char  renderCapability1(intptr_t a) = 0;                            // 25 +0x64  SwiffHost 0x486E80
    virtual int   getMovieWidth() = 0;                                     // 26 +0x68  init / onResize
    virtual int   getMovieHeight() = 0;                                    // 27 +0x6C  init / onResize
    virtual void  renderSetPairState(int a, int b) = 0;                    // 28 +0x70
    virtual void  legacyModeOff() = 0;                                     // 29 +0x74
    virtual void  legacyModeOn() = 0;                                      // 30 +0x78
    virtual char  getModeFlag() = 0;                                       // 31 +0x7C
    virtual void  setModeFlag(char v) = 0;                                 // 32 +0x80
    virtual void  notifyMouse(int x, int y, int buttons) = 0;              // 33 +0x84
    virtual void  processQueuedMouseState() = 0;                           // 34 +0x88
    virtual void* getBf2VoipClient() = 0;                                  // 35 +0x8C
    virtual void  setVoipUpdateFlag(char v) = 0;                           // 36 +0x90
};

} } }
#endif
#endif
