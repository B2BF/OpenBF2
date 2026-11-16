// [CHOSEN, not recovered] Native client window-message adapter (issue #36).
#ifndef PLATFORM_HOST_WINDOWMESSAGESOURCE_H
#define PLATFORM_HOST_WINDOWMESSAGESOURCE_H

#include "HostEvents.h"
#include "HostWindowTypes.h"
#include "KeyMap.h"

namespace dice { namespace hfe { namespace host {

// Windows-1252 byte for a UTF-16 unit, or -1 when the code page has none (WM_CHAR delivers
// ANSI characters to this narrow-character window class).
inline int cp1252FromUtf16(unsigned short c)
{
    if (c < 0x80 || (c >= 0xA0 && c <= 0xFF))
        return c;
    static const unsigned short high[32] = {
        0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
        0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178 };
    for (int i = 0; i < 32; ++i)
        if (high[i] == c)
            return 0x80 + i;
    return -1;
}

// Translates normalized native host events into the engine window-message vocabulary.
class WindowMessageSource : public host::IHostEventListener
{
public:
    void onAppActivationChanged(bool appActive) override
    {
        send(hostmsg::WM_ACTIVATEAPP, appActive ? 1 : 0, 0);
    }

    void onFocusChanged(bool windowKey, bool appActive) override
    {
        const bool focused = windowKey && appActive;
        if (focused == m_focused)
            return;
        m_focused = focused;
        send(hostmsg::WM_ACTIVATE, focused ? hostmsg::WA_ACTIVE : hostmsg::WA_INACTIVE, 0);
        send(focused ? hostmsg::WM_SETFOCUS : hostmsg::WM_KILLFOCUS, 0, 0);
    }

    void onResize(int width, int height, bool minimized) override
    {
        send(hostmsg::WM_SIZE, minimized ? hostmsg::SIZE_MINIMIZED : hostmsg::SIZE_RESTORED, hostmsg::makeLParam(width, height));
    }

    // A native close request follows the engine's close/destroy message sequence.
    void onCloseRequested() override
    {
        send(hostmsg::WM_CLOSE, 0, 0);
        send(hostmsg::WM_DESTROY, 0, 0);
    }

    // Key messages carry the US-layout virtual key and scan code in lParam bits 16-24.
    // Alt combinations and F10 arrive as system-key messages, as they do on Windows.
    void onKey(const host::HostKeyEvent& event) override
    {
        const unsigned char dik = event.dik;
        const unsigned int vk = win32VirtualKeyFromDik(dik);
        if (!vk)
            return;
        const bool alt = (event.modifiers & host::HostModAlt) != 0;
        const bool sys = alt || vk == hostmsg::VK_F10;
        std::uint32_t lParam = 1u | (static_cast<std::uint32_t>(dik & 0x7F) << 16);
        if (dik & 0x80) lParam |= 1u << 24;                 // extended key
        if (alt)        lParam |= 1u << 29;                 // context code
        if (event.repeat || !event.down) lParam |= 1u << 30;
        if (!event.down) lParam |= 1u << 31;
        const UINT message = event.down ? (sys ? hostmsg::WM_SYSKEYDOWN : hostmsg::WM_KEYDOWN)
                                        : (sys ? hostmsg::WM_SYSKEYUP : hostmsg::WM_KEYUP);
        send(message, vk, static_cast<LPARAM>(lParam));
    }

    // Native editing keys and shortcuts have already been normalized by the backend.
    void onText(const host::HostTextEvent& event) override
    {
        for (unsigned short c : event.characters)
        {
            const int ansi = cp1252FromUtf16(c);
            if (ansi < 0)
                continue;
            send(hostmsg::WM_CHAR, static_cast<WPARAM>(ansi), 1);
        }
    }

    // Windows sends WM_SETCURSOR, then WM_MOUSEMOVE, while the pointer is over the client area.
    // While the mouse is captured (the exclusively acquired DirectInput mouse in-game) the system
    // cursor does not move and Windows sends no mouse messages at all; the menu handlers
    // (bf2MessageHandler -> MainMenuLogic::onMouse*, which queue into the SwiffPlayer) must not see
    // in-game motion or clicks.
    void onMouseMove(const host::HostMouseMoveEvent& event) override
    {
        if (!event.insideView || event.captured)
            return;
        send(hostmsg::WM_SETCURSOR, 0, 1);                               // HTCLIENT
        send(hostmsg::WM_MOUSEMOVE, 0, hostmsg::makeLParam(static_cast<int>(event.x), static_cast<int>(event.y)));
    }

    // CS_DBLCLKS: the second press of a double click arrives as WM_LBUTTONDBLCLK.
    void onMouseButton(const host::HostMouseButtonEvent& event) override
    {
        if (event.captured)
            return;
        const LPARAM where = hostmsg::makeLParam(static_cast<int>(event.x), static_cast<int>(event.y));
        switch (event.button)
        {
        case 0: send(event.down ? (event.clickCount == 2 ? hostmsg::WM_LBUTTONDBLCLK : hostmsg::WM_LBUTTONDOWN) : hostmsg::WM_LBUTTONUP, 0, where); break;
        case 1: send(event.down ? hostmsg::WM_RBUTTONDOWN : hostmsg::WM_RBUTTONUP, 0, where); break;
        case 2: send(event.down ? hostmsg::WM_MBUTTONDOWN : hostmsg::WM_MBUTTONUP, 0, where); break;
        default: break;
        }
    }

    void onMouseWheel(const host::HostMouseWheelEvent& event) override
    {
        if (event.captured)
            return;
        send(hostmsg::WM_MOUSEWHEEL, static_cast<WPARAM>(static_cast<std::uint32_t>(event.delta & 0xFFFF) << 16), 0);
    }

    void resetFocus() { m_focused = false; }

protected:
    virtual void send(UINT message, WPARAM wParam, LPARAM lParam) = 0;

private:
    bool m_focused = false;
};

} } }

#endif
