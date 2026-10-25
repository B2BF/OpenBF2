// Code/Tools/BF2Editor/App/ImGuiHost.h

// Dear ImGui inside the editor: one context, the Win32 platform backend on the
// editor window and the DX9 renderer backend on the engine'S OWN IDirect3DDevice9 (RendDX9's device).
// ImGui draws after the scene and before Present. Editor-only code (D3).
#ifndef BF2EDITOR_APP_IMGUIHOST_H
#define BF2EDITOR_APP_IMGUIHOST_H

#include "Framework/EditorConfig.h"

#include <windows.h>

#include <string>

struct IDirect3DDevice9;

namespace bf2editor
{

class ImGuiHost
{
public:
    ImGuiHost();
    ~ImGuiHost();

    // iniPath: where ImGui keeps the dock layout ("" = no layout file).
    bool init(HWND window, IDirect3DDevice9* device, const std::string& iniPath);
    void shutdown();
    bool isUp() const { return m_up; }

    // Feed every window message here first. Returns true when ImGui consumed it (the caller then
    // returns `result` from its window procedure).
    bool handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);

    // One frame: beginFrame(), build the UI with ImGui calls, endFrame(); then, inside the device's
    // BeginScene/EndScene after the scene, renderDrawData().
    void beginFrame();
    void endFrame();
    void renderDrawData();

    // Around IDirect3DDevice9::Reset: ImGui's device objects live in D3DPOOL_DEFAULT.
    void onDeviceLost();
    void onDeviceReset();

    // Whether ImGui wants the mouse / keyboard this frame (the viewport camera must then leave them).
    bool wantsMouse() const;
    bool wantsKeyboard() const;

private:
    ImGuiHost(const ImGuiHost&);
    ImGuiHost& operator=(const ImGuiHost&);

    bool        m_up;
    bool        m_frameOpen;
    std::string m_iniPath;      // ImGui keeps the pointer; the string must outlive the context
};

// pointerOverUi reserves panel/menu/popup input without using ImGuizmo's capture flag.
// keyboardForView accepts keys when text entry is inactive and UI focus does not need them.
bool pointerOverUi();
bool keyboardForView();
// A tool drawn over the view (the HUD editor's outlines) takes the view's mouse while `claimed`: pointerOverUi then
// answers true, so the world's own tools step back.
void claimViewMouse(bool claimed);

} // namespace bf2editor

#endif // BF2EDITOR_APP_IMGUIHOST_H
