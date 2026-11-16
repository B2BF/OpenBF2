// Client window and engine-specific window subclass. Definitions are client-only;
// dedicated code shares the declaration and keeps its window pointer null.
// [INFERRED] BF2Window.cpp placement follows link order.
// [UNRESOLVED] The base window class's original source location is unknown.

#ifndef GAME_MAIN_BF2WINDOW_H       // [INFERRED] guard style
#define GAME_MAIN_BF2WINDOW_H

#if defined(_WIN32)
#include <windows.h>
#elif (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))
// [CHOSEN, not recovered] macOS port: HWND/UINT/WPARAM/LPARAM/LRESULT in dice::hfe, and the
// Win32 message ids BF2Window_macos.mm synthesizes from Cocoa events.
#include "../../Platform/macOS/HostWindowTypes.h"
#endif
#include <list>
#include <string>

namespace dice {
namespace hfe {

// A window message handler as the list stores it. Return value is ignored by the wndproc (see above).
typedef int (*MessageHandler)(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

class Window                                 // [INFERRED] name - base, TU [UNRESOLVED]
{
public:
    Window();
    ~Window();                               // NOT virtual; see the header block

    // +0x00  . Registers the class (named after the title), creates the window either
    //        fullscreen (WS_EX_TOPMOST | WS_POPUP|WS_VISIBLE at the screen size) or windowed
    //        (WS_OVERLAPPEDWINDOW at (x, y), client area width x height plus frame metrics), hides
    //        the cursor if asked, shows and foregrounds it. x/y == -1 mean "50".
    virtual bool create(const std::string& title, int width, int height, bool fullscreen,
                        bool hideCursor, int iconId, int x, int y);
    virtual bool isCreated() const;          // +0x04   - m_hwnd != 0
    virtual bool getHasFocus() const;        // +0x08   - +0x39 (BF2::handleInGameWinMessages reads this)
    virtual const std::string& getTitle() const;   // +0x0C   - &m_title
    virtual int  getWidth() const;           // +0x10   - +0x30
    virtual int  getHeight() const;          // +0x14   - +0x34
    virtual bool unresolved18() const;       // +0x18  `return true` (3 B, folded with other constant-true bodies) [UNRESOLVED]
    virtual void destroy();                  // +0x1C   - PostQuitMessage(0); DestroyWindow; m_hwnd = 0
    virtual void show(bool visible);         // +0x20   - ShowWindow SW_SHOW/SW_HIDE, SetFocus when shown
    virtual bool getVisible() const;         // +0x24   - +0x3A
    virtual bool getFullscreen() const;      // +0x28   - +0x38
    virtual HWND getHwnd() const;            // +0x2C   - +0x10
    virtual void addMessageHandler(MessageHandler handler);     // +0x30   - push_back
    virtual void removeMessageHandler(MessageHandler handler);  // +0x34   - remove by value

    // the wndproc create() registers. Static: it reaches the window through
    // `mainWindow`, not through GWL_USERDATA (the `this` passed as lpParam to CreateWindowExA is
    // never read back).
#if defined(_WIN32)
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
#else
    // [CHOSEN, not recovered] macOS: called by BF2Window_macos.mm with synthesized messages.
    static LRESULT wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
#endif

    friend class SwiffHost;                  //  clears m_hideCursor directly
protected:
    std::list<MessageHandler> m_handlers;    // +0x04
    HWND        m_hwnd;                      // +0x10
    std::string m_title;                     // +0x14
    int         m_width;                     // +0x30
    int         m_height;                    // +0x34
    bool        m_fullscreen;                // +0x38
    bool        m_hasFocus;                  // +0x39
    bool        m_visible;                   // +0x3A
    bool        m_hideCursor;                // +0x3B
};

class BF2Window : public Window              // [INFERRED] name - the class BF2Engine::init constructs
{
public:
    BF2Window();                             // Window() then the vptr
    virtual ~BF2Window();                    // +0x38   - deleting destructor: vptr, Window::~Window, free

    // +0x3C   - create() with x/y from the console Vars opt-window-x / opt-window-y
    //        (default -1 = "50" inside create). +wx / +wy on the command line set those Vars.
    virtual bool createFromSettings(const std::string& title, int width, int height,
                                    bool fullscreen, bool hideCursor, int iconId);
    virtual void setHwnd(HWND hwnd);         // +0x40   - m_hwnd = hwnd (a folded 13-byte body) [INFERRED]
    // +0x44   - SetWindowPos(hwnd, 0, 0, 0, width + 2*SM_CXFRAME,
    //        height + SM_CYCAPTION + 2*SM_CYFRAME, SWP_NOMOVE)
    virtual void resizeClient(int width, int height);
    // +0x48   - swaps the window between the windowed style (WS_OVERLAPPEDWINDOW,
    //        no extended style) and the fullscreen one (WS_POPUP|WS_VISIBLE, WS_EX_TOPMOST), shows,
    //        foregrounds, updates, then flips m_fullscreen.
    virtual void toggleFullscreenStyle();
};

// dword_9EB078: the one window, published by Window::Window and cleared by Window::~Window.
extern Window* mainWindow;                   // [INFERRED] name

} // namespace hfe
} // namespace dice

#endif // GAME_MAIN_BF2WINDOW_H
