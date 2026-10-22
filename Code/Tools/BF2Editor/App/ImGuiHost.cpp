// Code/Tools/BF2Editor/App/ImGuiHost.cpp - see ImGuiHost.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/ImGuiHost.h"

#include "Framework/EditorLog.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <d3d9.h>

// Declared by imgui_impl_win32.h only under a guard the header leaves to the application.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace bf2editor
{

ImGuiHost::ImGuiHost()
    : m_up(false)
    , m_frameOpen(false)
{
}

ImGuiHost::~ImGuiHost()
{
    shutdown();
}

bool ImGuiHost::init(HWND window, IDirect3DDevice9* device, const std::string& iniPath)
{
    if (m_up)
        return true;
    if (!window || !device)
    {
        logLine(LogLevel::Error, "imgui: no %s to draw with", window ? "D3D9 device" : "window");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    m_iniPath = iniPath;
    io.IniFilename = m_iniPath.empty() ? 0 : m_iniPath.c_str();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(window))
    {
        logLine(LogLevel::Error, "imgui: the Win32 backend refused the window");
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplDX9_Init(device))
    {
        logLine(LogLevel::Error, "imgui: the DX9 backend refused the device");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    m_up = true;
    logLine(LogLevel::Info, "imgui: %s up on the engine's device", IMGUI_VERSION);
    return true;
}

void ImGuiHost::shutdown()
{
    if (!m_up)
        return;
    if (m_frameOpen)
        endFrame();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_up = false;
}

bool ImGuiHost::handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    if (!m_up)
        return false;
    result = ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
    return result != 0;
}

void ImGuiHost::beginFrame()
{
    if (!m_up || m_frameOpen)
        return;
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    m_frameOpen = true;
}

void ImGuiHost::endFrame()
{
    if (!m_up || !m_frameOpen)
        return;
    ImGui::Render();
    m_frameOpen = false;
}

void ImGuiHost::renderDrawData()
{
    if (!m_up)
        return;
    if (ImDrawData* data = ImGui::GetDrawData())
        ImGui_ImplDX9_RenderDrawData(data);
}

void ImGuiHost::onDeviceLost()
{
    if (m_up)
        ImGui_ImplDX9_InvalidateDeviceObjects();
}

void ImGuiHost::onDeviceReset()
{
    if (m_up)
        ImGui_ImplDX9_CreateDeviceObjects();
}

bool ImGuiHost::wantsMouse() const
{
    return m_up && ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiHost::wantsKeyboard() const
{
    return m_up && ImGui::GetIO().WantCaptureKeyboard;
}

namespace
{
bool g_viewMouseClaimed = false;
}

void claimViewMouse(bool claimed)
{
    g_viewMouseClaimed = claimed;
}

bool pointerOverUi()
{
    return g_viewMouseClaimed || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)
        || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

bool keyboardForView()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return false;
    return !io.WantCaptureKeyboard || !pointerOverUi();
}

} // namespace bf2editor
