// Client main-menu lifecycle and input handling. [INFERRED] Source placement.
#include "../Core/BuildConfig.h"
#if !DEDICATED
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include "../Platform/macOS/CocoaHost.h"   // [CHOSEN, not recovered] cursor, modifiers, clipboard
#endif
#include "MainMenuLogic.h"
#include "IMenuTeamManager.h"
#include "ISwiffHost.h"
#include "ISwiffPlayer.h"
#include "ISwiffRender.h"
#include "../Core/ClassManager.h"
#include "../Core/SmartItf.h"               // classManager
#include "../Core/Ids.h"
#include "../Core/EventManager.h"
#include "../Game/ControlSettings.h"
#include "../Game/GlobalSettings.h"
#include "../Game/IGame.h"
#include "../Game/LocalProfileManager.h"
#include "../Game/Utility/Vars.h"
#include "../Game/Main/BF2Engine.h"
#include "../Game/Main/BF2EngineSetup.h"
#include "../Game/Main/BF2Window.h"
#include "../Game/Main/BF2Console.h"
#include "../Game/Main/BF2ClientSeams.h"    // g_globalSettings, g_controlSettings, the haptic seam
#include "../IO/File/FileManager.h"
#include "../IO/Console/Console.h"
#include "../IO/Input/Input.h"
#include <cstring>
#include <ctime>

namespace dice { namespace hfe {

extern BF2Engine* bf2Engine;                          // Core/Globals.cpp; PE dword_986F48
namespace io { extern IBase* controlMapManager; }     // Core/Globals.cpp; PE 0x9FFCB4
namespace io { extern IBase* inputDeviceManager; }    // Core/Globals.cpp; PE 0x9FFCB0
// The menu key ring BF2's window procedure fills (Game/Main/BF2.cpp; PE 0x9FFB90/0x9FFC9C/

extern char menuKeyBuffer[256];
extern int  menuKeyCount;
extern int  menuKeyRead;
extern int  menuKeyWrite;

namespace {

bool s_firstInitDone = false;                          // byte_9A6B78: the profile check and precache.con run once

// the next queued menu character, or 0 when the ring is empty.
char popMenuKey()
{
    int index = menuKeyRead;
    if (!menuKeyCount && menuKeyRead == menuKeyWrite)
        return 0;
    if (menuKeyRead > 255)
        index = 0;
    const char key = menuKeyBuffer[index];
    const bool underflow = --menuKeyCount < 0;
    menuKeyRead = index + 1;
    if (underflow)
        menuKeyCount = 0;
    return key;
}

void runMenuScript(const char* file)
{
    io::mainConsole->run(std::string(file), std::string(""), std::string(""), std::string(""),
                         std::string(""), std::string(""), std::string(""), std::string(""),
                         std::string(""), io::Console::ignoredString_);
}

} // namespace

MainMenuLogic::MainMenuLogic()
    : m_refCount(1), m_swiffActive(false), m_frameActive(false), m_player(0), m_host(0), m_render(0),
      m_console(0), m_mouseX(0), m_mouseY(0), m_scaleX(0.0f), m_scaleY(0.0f), m_mouseButtonState(0),
      m_activatedAt(0)
{
}

// the deactivation, then the string member.
MainMenuLogic::~MainMenuLogic()
{
    deactivate();
}

IBase* MainMenuLogic::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    MainMenuLogic* logic = new MainMenuLogic();
    IBase* result = static_cast<IBase*>(logic->queryInterface(iid));
    if (!result)
        logic->release();
    return result;
}

void  MainMenuLogic::addRef() { ++m_refCount; }
int   MainMenuLogic::getRef() { return m_refCount; }
int   MainMenuLogic::release()
{
    const int left = --m_refCount;
    if (!left)
        delete this;                                   // slot +0x5C(1)
    return left;
}
void* MainMenuLogic::queryInterface(unsigned int iid) const
{
    if (iid == 1 || iid == IID_IMenuLogic)
        return const_cast<MainMenuLogic*>(this);
    return 0;
}
unsigned int MainMenuLogic::getClassId() const { return CID_MainMenuLogic; }

// Initializes the menu movie, input map, renderer and host services in startup order.
// Loads global profile settings and precache commands once, then resets haptic feedback.
bool MainMenuLogic::init()
{
    if (!s_firstInitDone)
    {
        IBase* profiles = classManager->getSingleton(std::string("LocalProfileManager"));
        std::string fileName = static_cast<IGlobalSettings*>(g_globalSettings)->getFileName();
        std::string root = static_cast<ILocalProfileManager*>(profiles)->getProfilesPath();
        std::string path(root);
        path += fileName;
        if (io::fileManager->fileExists(path))
            static_cast<ILocalProfileManager*>(profiles)->loadProfileFile(fileName);
        else
            static_cast<ILocalProfileManager*>(profiles)->saveGlobalSettings();
    }
    bf2Engine->bf2EngineSetup->initSwiff();                                   // 0x446CB0, result dropped
    eventManager->postEvent(static_cast<EventCategory>(10), 1, 0, 0.0f);      // ECMenuMenuEvent_Enable
    m_swiffActive = true;
    m_player = static_cast<ui::ISwiffPlayer*>(classManager->getSingleton(std::string("SwiffPlayer")));
    if (!m_player)
        return false;
    m_host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(std::string("SwiffHost")));
    if (!m_host)
        return false;
    m_host->loadCredits();                                                    // +0x14
    m_host->setFlagEC4(false);                                                // +0x2C8
    m_host->resetStatsCache();                                                // +0x2EC
    m_host->setFlagF04(false);                                                // +0x2F4
    const int* disableSwiff = Vars::getInt("disable-swiff", 0);
    if (*disableSwiff)
        m_host->setFlagE81(false);                                            // +0x27C
    else if (!m_host->getFlagE81())                                           // +0x280
        m_host->setFlagE81(true);
    m_player->setSwiffHost(m_host);                                           // +0x58
    m_render = static_cast<ui::ISwiffRender*>(classManager->getSingleton(std::string("SwiffRender")));
    if (!m_render)
        return false;
    IBase* teams = classManager->getSingleton(std::string("MenuTeamManager"));
    static_cast<IMenuTeamManager*>(teams)->init();                            // unguarded in the binary
    io::IControlMapManager* maps = static_cast<io::IControlMapManager*>(io::controlMapManager);
    if (maps->getActive())                                                    // +0x34, asked twice as shipped
        m_activeControlMapName = maps->getActive()->getName();                // +0x14
    static_cast<IControlSettings*>(g_controlSettings)->apply();               // +0x78
    m_render->init();                                                         // +0x10
    m_player->setRender(m_render);                                            // +0x24
    m_player->loadMovie("mainMenu.swf");                                      // +0x14, result dropped
    unsigned int width = 800;
    unsigned int height = 600;
    m_host->getViewportSize(&width, &height);                                 // +0x1DC
    const float movieWidth  = static_cast<float>(static_cast<unsigned int>(m_player->getMovieWidth()));    // +0x68
    const float movieHeight = static_cast<float>(static_cast<unsigned int>(m_player->getMovieHeight()));   // +0x6C
    m_player->setScale(static_cast<float>(width) / movieWidth, static_cast<float>(height) / movieHeight);   // +0x10
    m_scaleX = movieWidth / static_cast<float>(width);
    m_scaleY = movieHeight / static_cast<float>(height);
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
    if (!s_firstInitDone)
    {
        runMenuScript("menu/external/flashmenu/precache.con");
        s_firstInitDone = true;
    }
    time(&m_activatedAt);
    // 0x4A3490..0x4A3535 zeroes the Falcon's force effects; none exists without the archived device
    // (Code/Archive/NovintFalcon).
    return true;
}

// 0x4A2B90 (98 B): the mouse device back on, the control-map backups dropped, the (10, 2)
// Disable event and the SwiffPlayer.dll unload when the menu had been enabled, the cursor hidden.
void MainMenuLogic::deactivate()
{
    io::IInputDevice* mouse = static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->getInputDevice(io::IDFMouse);
    mouse->activate();                                                        // +0x1C
    static_cast<IControlSettings*>(g_controlSettings)->deleteControlMapBackups();   // +0x7C
    if (m_swiffActive)
    {
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

ui::ISwiffPlayer* MainMenuLogic::getSwiffPlayer() const { return m_player; }
ui::ISwiffHost*   MainMenuLogic::getSwiffHost() const   { return m_host; }
ui::ISwiffRender* MainMenuLogic::getSwiffRender() const { return m_render; }

// the frame's wall-clock delta, narrowed to the player's float.
void MainMenuLogic::update(double wallDelta)
{
    m_player->update(static_cast<float>(wallDelta));                          // +0x18
}

// the cursor follows the host's busy state; a frame is active only when the renderer
// and the loaded movie both agree.
bool MainMenuLogic::beginFrame()
{
    ui::ISwiffHost* host = m_host;
    const int busy = host->getBusy();                                         // +0x2B8
#if defined(__APPLE__)
    macos::CocoaHost::instance().setCursor(host->getCursor(busy));            // +0x6C, [CHOSEN] SetCursor
#else
    SetCursor(static_cast<HCURSOR>(host->getCursor(busy)));                   // +0x6C
#endif
    if (!m_render->beginFrame() || !m_player->isMovieLoaded())                // +0xCC / +0x20
        return false;
    m_frameActive = true;
    return true;
}
void MainMenuLogic::display() { m_player->display(); }                        // +0x1C
void MainMenuLogic::endFrame()
{
    m_render->endFrame();                                                     // +0xD0
    m_frameActive = false;
}

// Forwards one queued character or navigation-key state to the menu, then releases inactive keys.
// The GameInput argument is unused. [CHOSEN] Returns true; the caller ignores the result.
bool MainMenuLogic::handleInput(io::GameInput&)
{
    io::IKeyboardDevice* keyboard = static_cast<io::IKeyboardDevice*>(
        static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->getInputDevice(io::IDFKeyboard));
    if (!keyboard)
        return false;
    const char character = popMenuKey();
    bool releaseSweep = false;
    switch (static_cast<unsigned char>(character))
    {
    case 8:    m_player->notifyKeyState(14, 1);  releaseSweep = true; break;     // DIK_BACK
    case 0x7F: m_player->notifyKeyState(211, 1); releaseSweep = true; break;     // DIK_DELETE
    case 9:    m_player->notifyKeyState(15, 1);  releaseSweep = true; break;     // DIK_TAB
    case 0xD:
    case 0xA:  m_player->notifyKeyState(28, 1);  releaseSweep = true; break;     // DIK_RETURN
    default:   break;
    }
    if (!releaseSweep)
    {
        const unsigned char code = static_cast<unsigned char>(character);
        if (code >= 0x20 && code <= 0x7E && code != 96)
        {
            if (!handleClipboardShortcut(character))
                m_player->notifyChar(character);                              // +0x40
            releaseSweep = true;
        }
    }
    if (releaseSweep)
    {
        for (unsigned int key = 1; key < 0xFF; ++key)
            if (keyboard->keyReleased(static_cast<io::InputDeviceKeys>(key)))  // +0x38
                m_player->notifyKeyState(static_cast<int>(key), 0);           // +0x44
        return true;
    }
    static const int navigationKeys[12] = { 199, 207, 42, 54, 56, 184, 208, 200, 203, 205, 29, 157 };
    for (unsigned int i = 0; i < 12; ++i)
    {
        const int key = navigationKeys[i];
        if (keyboard->keyPressed(static_cast<io::InputDeviceKeys>(key)))       // +0x34
            m_player->notifyKeyState(key, 1);
        else if (keyboard->keyReleased(static_cast<io::InputDeviceKeys>(key)))
            m_player->notifyKeyState(key, 0);
    }
    return true;
}

// 0x4A2E10 (263 B): with Ctrl held (and not Alt), 'v' pastes CF_TEXT into the player's text
// input, 'c' and 'x' copy the player's text out; any other Ctrl character is swallowed.
#if defined(__APPLE__)
// [CHOSEN, not recovered] macOS: the same arms on the Cocoa host. Control or Command counts as
// Ctrl and Option as Alt (BF2.cpp's controlKeyDown/altKeyDown); the clipboard text is
// Windows-1252, the CF_TEXT counterpart. A clipboard the host cannot open or read is skipped, as
// a failed OpenClipboard or an absent CF_TEXT is.
bool MainMenuLogic::handleClipboardShortcut(char character)
{
    macos::CocoaHost& host = macos::CocoaHost::instance();
    const unsigned int modifiers = host.modifierFlags();
    const bool control = (modifiers & (macos::HostModControl | macos::HostModCommand)) != 0;
    const bool alt = (modifiers & macos::HostModOption) != 0;
    if (!control || (control && alt))
        return false;
    if (character != 'v')
    {
        if (character == 'c' || character == 'x')
        {
            std::string text;
            if (character == 'c')
                m_player->getTextInput0(text);                                // +0x50
            else
                m_player->getTextInput1(text);                                // +0x54
            host.setClipboardText(std::string(text.c_str()));                 // up to the terminator
        }
        return true;
    }
    std::string text;
    if (host.getClipboardText(text))
        m_player->setTextInput(text.c_str());                                 // +0x4C
    return true;
}
#else
bool MainMenuLogic::handleClipboardShortcut(char character)
{
    if (!GetAsyncKeyState(VK_CONTROL) || (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_MENU)))
        return false;
    if (character != 'v')
    {
        if (character == 'c' || character == 'x')
        {
            HWND window = bf2Engine->window->getHwnd();
            if (OpenClipboard(window))
            {
                EmptyClipboard();
                std::string text;
                if (character == 'c')
                    m_player->getTextInput0(text);                            // +0x50
                else
                    m_player->getTextInput1(text);                            // +0x54
                HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
                char* out = static_cast<char*>(GlobalLock(handle));
                const char* in = text.c_str();
                char c;
                do
                {
                    c = *in;
                    *out++ = *in++;
                }
                while (c);
                GlobalUnlock(handle);
                SetClipboardData(CF_TEXT, handle);
                CloseClipboard();
            }
        }
        return true;
    }
    HWND window = bf2Engine->window->getHwnd();
    if (!OpenClipboard(window))
        return true;
    if (IsClipboardFormatAvailable(CF_TEXT) || IsClipboardFormatAvailable(CF_OEMTEXT))
    {
        HANDLE data = GetClipboardData(CF_TEXT);
        if (data)
        {
            std::string text(static_cast<const char*>(GlobalLock(data)));
            m_player->setTextInput(text.c_str());                             // +0x4C
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return true;
}
#endif

// the console pointer, and the console taken inactive when one is given.
void MainMenuLogic::setConsole(BF2Console* console)
{
    m_console = console;
    if (console)
        console->setConsoleActive(false);
}
BF2Console* MainMenuLogic::getConsole() const { return m_console; }

// 0x4A2C80 (176 B): BF2Engine::handleEvent(11, 2)'s packed size - the player rescaled to the new
// viewport, the scales cached, and the host's +0x2C8 flag raised.
void MainMenuLogic::onResize(unsigned int width, unsigned int height)
{
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const float movieWidth  = static_cast<float>(static_cast<unsigned int>(m_player->getMovieWidth()));
    const float movieHeight = static_cast<float>(static_cast<unsigned int>(m_player->getMovieHeight()));
    m_player->setScale(w / movieWidth, h / movieHeight);
    m_scaleX = movieWidth / w;
    m_scaleY = movieHeight / h;
    m_player->getSwiffHost()->setFlagEC4(true);                               // +0x5C, then +0x2C8
}

// Closes the menu when a game is active, loading is not being aborted, and the menu has been open
// for more than three seconds. Does nothing while capturing a key binding.
void MainMenuLogic::onEscape()
{
    time_t now;
    time(&now);
    const double elapsed = difftime(now, m_activatedAt);
    if (game && !game->getAbortLoadLevel() && elapsed > 3.0)
    {
        IBase* controls = classManager->getSingleton(std::string("ControlSettings"));
        if (!static_cast<IControlSettings*>(controls)->isMappingInput())      // +0x88
        {
            MenuEvent event;
            event.type = 9;
            m_host->pushEvent(event);                                         // +0x2C
        }
    }
}

// 0x4A2D30..0x4A2DD0: moves always reach the player; presses only while the host is not busy.
void MainMenuLogic::onMouseMove(int x, int y)
{
    m_mouseX = x;
    m_mouseY = y;
    if (m_player)
        m_player->queueMouseState(x, y, m_mouseButtonState);                  // +0x30
}
void MainMenuLogic::onMouseButtonDown()
{
    if (m_host->getBusy())
        return;
    if (m_player)
        m_player->queueMouseState(m_mouseX, m_mouseY, 1);
    m_mouseButtonState = 1;
}
void MainMenuLogic::onMouseButtonUp()
{
    if (m_player)
        m_player->queueMouseState(m_mouseX, m_mouseY, 0);
    m_mouseButtonState = 0;
}
void MainMenuLogic::onMouseDoubleClick()
{
    if (m_host->getBusy())
        return;
    if (m_player)
        m_player->queueMouseState(m_mouseX, m_mouseY, 2);
    m_mouseButtonState = 2;
}
void MainMenuLogic::nullsub60() {}                                            // nullsub_2

} }
#endif
