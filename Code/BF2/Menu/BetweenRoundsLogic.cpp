// Handles input and presentation for the between-rounds menu.
#include "../Core/BuildConfig.h"
#if !DEDICATED
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include "../Platform/macOS/CocoaHost.h"   // [CHOSEN, not recovered] cursor and key characters
#include "../Platform/macOS/MacKeyMap.h"
#endif
#include "BetweenRoundsLogic.h"
#include "IMenuTeamManager.h"
#include "ISwiffHost.h"
#include "ISwiffPlayer.h"
#include "ISwiffRender.h"
#include "../Core/ClassManager.h"
#include "../Core/SmartItf.h"               // classManager
#include "../Core/Ids.h"
#include "../Core/EventManager.h"
#include "../Game/ControlSettings.h"
#include "../Game/IGame.h"
#include "../Game/Main/BF2Engine.h"
#include "../Game/Main/BF2EngineSetup.h"
#include "../Game/Main/BF2Console.h"
#include "../Game/Main/BF2ClientSeams.h"    // g_controlSettings
#include "../IO/Input/Input.h"

namespace dice { namespace hfe {

extern BF2Engine* bf2Engine;                          // Core/Globals.cpp; PE dword_986F48
namespace io { extern IBase* controlMapManager; }     // Core/Globals.cpp; PE 0x9FFCB4
namespace io { extern IBase* inputDeviceManager; }    // Core/Globals.cpp; PE 0x9FFCB0

// Translates a DirectInput scan code using the keyboard layout cached on first use.
// Returns the number of characters written, or zero when keyboard-state retrieval fails.
#if defined(__APPLE__)
// [CHOSEN, not recovered] macOS: the key code for the scan code, translated with the current
// layout and the Shift/Caps Lock state, as ToAsciiEx translates with GetKeyboardState. Writes one
// character and returns 1, or returns 0 when the key produces none or one outside the 8-bit range
// the ANSI call answers in.
int menuScanCodeToChar(unsigned int scanCode, unsigned short* character)
{
    macos::CocoaHost& host = macos::CocoaHost::instance();
    const unsigned short keyCode =
        macos::macKeyCodeFromDik(static_cast<unsigned char>(scanCode), host.isISOKeyboard());
    if (keyCode == 0xFFFF)
        return 0;
    const std::u16string text = host.keyCharacter(keyCode, host.modifierFlags());
    if (text.size() != 1 || text[0] > 0xFF)
        return 0;
    character[0] = static_cast<unsigned short>(text[0]);
    return 1;
}
#else
int menuScanCodeToChar(unsigned int scanCode, unsigned short* character)
{
    static const HKL layout = GetKeyboardLayout(0);
    static BYTE keyState[256];
    if (!GetKeyboardState(keyState))
        return 0;
    return ToAsciiEx(MapVirtualKeyExA(scanCode, MAPVK_VSC_TO_VK, layout),
                     scanCode, keyState, character, 0, layout);
}
#endif

// The string is constructed between the console pointer and the mouse state, which is
// where the member sits; the four tail stores follow it.
BetweenRoundsLogic::BetweenRoundsLogic()
    : m_refCount(1), m_swiffActive(false), m_frameActive(false), m_player(0), m_host(0), m_render(0),
      m_console(0), m_mouseDown(false), m_mouseX(0), m_mouseY(0), m_escapeHandled(false)
{
}

// the vptr is reseated, deactivate runs, then the string member is destroyed.
BetweenRoundsLogic::~BetweenRoundsLogic()
{
    deactivate();
}

IBase* BetweenRoundsLogic::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    BetweenRoundsLogic* logic = new BetweenRoundsLogic();
    IBase* result = static_cast<IBase*>(logic->queryInterface(iid));
    if (!result)
        logic->release();
    return result;
}

void  BetweenRoundsLogic::addRef() { ++m_refCount; }
int   BetweenRoundsLogic::getRef() { return m_refCount; }
int   BetweenRoundsLogic::release()
{
    const int left = --m_refCount;
    if (!left)
        delete this;                                   // slot +0x5C(1)
    return left;
}
void* BetweenRoundsLogic::queryInterface(unsigned int iid) const
{
    if (iid == 1 || iid == IID_IMenuLogic)
        return const_cast<BetweenRoundsLogic*>(this);
    return 0;
}
unsigned int BetweenRoundsLogic::getClassId() const { return CID_BetweenRoundsLogic; }

// Initializes the loading-screen player, host and console.
// Early failures leave the active flag set and require normal teardown of partial state.
bool BetweenRoundsLogic::init()
{
    bf2Engine->bf2EngineSetup->initSwiff();                                   // 0x446CB0, result dropped
    eventManager->postEvent(static_cast<EventCategory>(10), 1, 0, 0.0f);      // ECMenuMenuEvent_Enable
    m_swiffActive = true;
    m_player = static_cast<ui::ISwiffPlayer*>(classManager->getSingleton(std::string("SwiffPlayer")));
    if (!m_player)
        return false;
    m_host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(std::string("SwiffHost")));
    if (!m_host)
        return false;
    m_player->setSwiffHost(m_host);                                           // +0x58
    m_host->setFlagEC4(false);                                                // +0x2C8
    if (!m_host->getFlagE81())                                                // +0x280
        m_host->setFlagE81(true);                                             // +0x27C
    m_host->setFlag28(true);                                                  // +0x074
    m_render = static_cast<ui::ISwiffRender*>(classManager->getSingleton(std::string("SwiffRender")));
    if (!m_render)
        return false;
    m_host->setFlagEA8(false);                                                // +0x2AC
    IBase* teams = classManager->getSingleton(std::string("MenuTeamManager"));
    static_cast<IMenuTeamManager*>(teams)->init();                            // unguarded in the binary
    io::IControlMapManager* maps = static_cast<io::IControlMapManager*>(io::controlMapManager);
    if (maps->getActive())                                                    // +0x34, asked twice as shipped
        m_activeControlMapName = maps->getActive()->getName();                // +0x14
    static_cast<IControlSettings*>(g_controlSettings)->apply();               // +0x78
    m_render->init();                                                         // +0x10
    m_player->setRender(m_render);                                            // +0x24
    m_player->loadMovie("endOfRound.swf");                                    // +0x14, result dropped
    unsigned int width = 800;
    unsigned int height = 600;
    m_host->getViewportSize(&width, &height);                                 // +0x1DC
    const float movieWidth  = static_cast<float>(static_cast<unsigned int>(m_player->getMovieWidth()));    // +0x68
    const float movieHeight = static_cast<float>(static_cast<unsigned int>(m_player->getMovieHeight()));   // +0x6C
    m_player->setScale(static_cast<float>(width) / movieWidth,
                       static_cast<float>(height) / movieHeight);             // +0x10
    // 0x4A3995 dispatches this through the object's OWN vtable slot +0x3C rather than calling the
    // body directly; a plain virtual call from source is that.
    setConsole(bf2Engine->bf2Console);                                        // BF2Engine +0x14
    io::IInputDevice* mouse = static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->getInputDevice(io::IDFMouse);
    mouse->deActivate();                                                      // +0x20, unguarded in the binary
#if defined(__APPLE__)
    macos::CocoaHost::instance().showCursor(true);                            // [CHOSEN] ShowCursor(TRUE)
    macos::CocoaHost::instance().setCursor(m_host->getCursor(0));             // +0x6C, SetCursor
#else
    ShowCursor(TRUE);
    SetCursor(static_cast<HCURSOR>(m_host->getCursor(0)));                    // +0x6C
#endif
    // Retail also sets the Falcon's mouse mode here (ds:0x978FC4); it is archived with the device
    // (Code/Archive/NovintFalcon).
    return true;
}

// the mouse device back on, the control-map backups dropped, then - only when the menu
// had been enabled - the renderer flag lowered, the (10, 2) Disable event and the SwiffPlayer.dll
// unload. The m_host null test guards only the flag; MainMenuLogic has no equivalent call at all.
void BetweenRoundsLogic::deactivate()
{
    io::IInputDevice* mouse = static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->getInputDevice(io::IDFMouse);
    mouse->activate();                                                        // +0x1C
    static_cast<IControlSettings*>(g_controlSettings)->deleteControlMapBackups();   // +0x7C
    if (m_swiffActive)
    {
        if (m_host)
            m_host->setRenderFlag(false);                                     // +0x320
        eventManager->postEvent(static_cast<EventCategory>(10), 2, 0, 0.0f);  // ECMenuMenuEvent_Disable
        bf2Engine->bf2EngineSetup->shutdownSwiff();
    }
    m_swiffActive = false;
    m_frameActive = false;
#if defined(__APPLE__)
    macos::CocoaHost::instance().setCursor(0);                                // [CHOSEN] SetCursor(0)
    macos::CocoaHost::instance().showCursor(false);                           //   ShowCursor(FALSE)
#else
    SetCursor(0);
    ShowCursor(FALSE);
#endif
}

ui::ISwiffPlayer* BetweenRoundsLogic::getSwiffPlayer() const { return m_player; }
ui::ISwiffHost*   BetweenRoundsLogic::getSwiffHost() const   { return m_host; }
ui::ISwiffRender* BetweenRoundsLogic::getSwiffRender() const { return m_render; }

// the movie, then the input devices, both with the same unclamped wall delta. The second
// call is what MainMenuLogic::update does not do.
void BetweenRoundsLogic::update(double wallDelta)
{
    m_player->update(static_cast<float>(wallDelta));                          // +0x18
    static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->update(static_cast<float>(wallDelta));   // +0x24
}

// the cursor is the busy one whenever the host is mid-request, and the frame is active on
// the renderer's word alone - unlike MainMenuLogic, the loaded-movie test is absent, so a frame
// begins here before endOfRound.swf has finished loading.
// The host is re-fetched from the player for each of the two questions, as shipped.
bool BetweenRoundsLogic::beginFrame()
{
    const bool pending = m_player->getSwiffHost()->getFlag7B8();              // +0x5C then +0xCC
    if (m_player->getSwiffHost()->getFlagEA8() || pending)                    // +0x5C then +0x2A8
#if defined(__APPLE__)
        macos::CocoaHost::instance().setCursor(m_host->getCursor(1));         // +0x6C, the busy cursor
    else
        macos::CocoaHost::instance().setCursor(m_host->getCursor(m_host->getBusy()));// +0x2B8 then +0x6C
#else
        SetCursor(static_cast<HCURSOR>(m_host->getCursor(1)));                // +0x6C, the busy cursor
    else
        SetCursor(static_cast<HCURSOR>(m_host->getCursor(m_host->getBusy())));// +0x2B8 then +0x6C
#endif
    if (!m_render->beginFrame())                                              // +0xCC
        return false;
    m_frameActive = true;
    return true;
}

void BetweenRoundsLogic::display() { m_player->display(); }                   // +0x1C
void BetweenRoundsLogic::endFrame()
{
    m_render->endFrame();                                                     // +0xD0
    m_frameActive = false;
}

// Queries menu input without clipboard handling.
bool BetweenRoundsLogic::handleInput(io::GameInput&)
{
    m_player->queueMouseState(m_mouseX, m_mouseY, m_mouseDown);               // +0x30
    io::IKeyboardDevice* keyboard = static_cast<io::IKeyboardDevice*>(
        static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->getInputDevice(io::IDFKeyboard));
    if (!keyboard)
        return false;
    for (unsigned int key = 1; key < 0xFF; ++key)
    {
        if (keyboard->keyPressed(static_cast<io::InputDeviceKeys>(key)))      // +0x34
        {
            unsigned short character[2];
            if (key == 15 || key == 28 || key == 14
                || (character[0] = 0, character[1] = 0,
                    menuScanCodeToChar(key, character) != 1))
            {
                m_player->notifyKeyState(static_cast<int>(key), 1);           // +0x44
            }
            else if ((character[0] >= 0x20 && character[0] <= 0x7E)
                     || (character[0] >= 0xA0 && character[0] <= 0xFF))
            {
                m_player->notifyChar(character[0]);                           // +0x40
            }
        }
        else if (keyboard->keyReleased(static_cast<io::InputDeviceKeys>(key)))// +0x38
        {
            m_player->notifyKeyState(static_cast<int>(key), 0);
        }
    }
    return true;
}

void BetweenRoundsLogic::setConsole(BF2Console* console) { m_console = console; }   // +0x18

// Returns null even when a console has been assigned.
BF2Console* BetweenRoundsLogic::getConsole() const { return 0; }

// 0x5A59F0 is nullsub_1. This screen does not rescale with the window; the scale init computed
// stands for the whole load. MainMenuLogic's slot rescales the player and caches the inverses.
void BetweenRoundsLogic::onResize(unsigned int, unsigned int) {}

// Handles Escape once for this menu instance; subsequent presses have no effect.
void BetweenRoundsLogic::onEscape()
{
    if (game && game->isPlayingDemo())                                        // PE +0xDC
        return;
    if (m_escapeHandled || !game)
        return;
    if (game->getAbortLoadLevel() || m_host->getFlag7B8())                    // PE +0x118 / +0xCC
    {
        if (game)
        {
            if (m_host->getFlag7B8() == 1)                                    // +0xCC, asked again
            {
                eventManager->postEvent(ECCore, 14, 0, 0.0f);
                m_host->setFlagF04(true);                                     // +0x2F4
                m_host->setFlagEA8(true);                                     // +0x2AC
                m_host->setFlag7B8(false);                                    // +0xD0
                m_escapeHandled = true;
            }
        }
    }
    else
    {
        MenuEvent event;
        event.type = 15;
        m_host->pushEvent(event);                                             // +0x2C
        m_escapeHandled = true;
    }
}

// Caches the pointer position for forwarding during the next frame.
void BetweenRoundsLogic::onMouseMove(int x, int y)
{
    m_mouseX = x;
    m_mouseY = y;
}
void BetweenRoundsLogic::onMouseButtonDown()
{
    if (m_host->getBusy())                                                    // +0x2B8
        return;
    m_mouseDown = true;
    m_player->legacyModeOff();                                                // +0x74
}
void BetweenRoundsLogic::onMouseButtonUp()
{
    m_mouseDown = false;
}
void BetweenRoundsLogic::onMouseDoubleClick()
{
    if (m_host->getBusy())                                                    // +0x2B8
        return;
    m_player->legacyModeOn();                                                 // +0x78
}

} }
#endif
