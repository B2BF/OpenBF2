// Code/Tools/BF2Editor/App/TestHarness.cpp - see TestHarness.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/TestHarness.h"

#include "App/EditSession.h"
#include "App/FreeCamera.h"
#include "App/MinimapPanel.h"
#include "App/ResourceBrowser.h"

#include "Game/Objects/IObject.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace bf2editor
{

namespace
{

// The labelled items ImGui drew, as its test-engine hooks report them (below): the frame being drawn and the last one
// drawn whole, each item's label, the window it is in and the middle of its box (window pixels).
struct SeenItem
{
    std::string label;          // as shown: up to a "##"
    std::string raw;            // whole, "##" part and all (for an item whose label is only an id)
    std::string window;
    float       x, y;
};
struct SeenFrame
{
    int                   frame = -1;
    std::vector<SeenItem> items;
};
SeenFrame g_drawing, g_drawn;

void noteItem(ImGuiContext* ctx, const char* label)
{
    if (!ctx || !label)
        return;
    if (ctx->FrameCount != g_drawing.frame)
    {
        if (g_drawing.frame >= 0)
            std::swap(g_drawn, g_drawing);
        g_drawing.frame = ctx->FrameCount;
        g_drawing.items.clear();
    }
    // The label as shown: up to a "##" (what follows only makes the id).
    const char* end = std::strstr(label, "##");
    SeenItem item;
    item.label.assign(label, end ? static_cast<size_t>(end - label) : std::strlen(label));
    item.raw = label;
    item.window = ctx->CurrentWindow ? ctx->CurrentWindow->Name : "";
    const ImRect& box = ctx->LastItemData.Rect;
    item.x = (box.Min.x + box.Max.x) * 0.5f;
    item.y = (box.Min.y + box.Max.y) * 0.5f;
    g_drawing.items.push_back(item);
}

} // namespace

void noteTestItem(const char* label)
{
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx && ctx->TestEngineHookItems)
        noteItem(ctx, label);
}

TestHarness::TestHarness(EditSession& session, const FreeCamera& camera)
    : m_session(session)
    , m_camera(camera)
    , m_wait(0)
    , m_dragSeenDown(false)
{
    m_mouse[0] = m_mouse[1] = 0.0f;
    // ImGui calls the test-engine hooks only while this is set (scripted runs; the hooks cost nothing otherwise).
    if (ImGui::GetCurrentContext())
        ImGui::GetCurrentContext()->TestEngineHookItems = true;
}

void TestHarness::beforeUi()
{
    if (m_dragPath.empty())
        return;
    // An external drag source (ImGuiDragDropFlags_SourceExtern) must be submitted every frame the drag
    // lasts; ImGui drops the payload the frame after the button comes up, as for a drag from a panel.
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (m_dragSeenDown)
        {
            m_dragPath.clear();
            m_dragSeenDown = false;
        }
        return;
    }
    m_dragSeenDown = true;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern))
    {
        ImGui::SetDragDropPayload(ResourceBrowser::templatePayloadType(), m_dragPath.c_str(), m_dragPath.size() + 1);
        ImGui::EndDragDropSource();
    }
}

int TestHarness::takeWait()
{
    const int frames = m_wait;
    m_wait = 0;
    return frames;
}

bool TestHarness::wait(int frames, std::string& error)
{
    if (frames < 1 || frames > 6000)
    {
        error = "EditorTest.wait takes 1 to 6000 frames";
        return false;
    }
    m_wait = frames;
    return true;
}

bool TestHarness::mouseMove(float x, float y, std::string&)
{
    m_mouse[0] = x;
    m_mouse[1] = y;
    ImGui::GetIO().AddMousePosEvent(x, y);
    return true;
}

bool TestHarness::mouseBy(float dx, float dy, std::string& error)
{
    return mouseMove(m_mouse[0] + dx, m_mouse[1] + dy, error);
}

bool TestHarness::mouseToObject(unsigned id, std::string& result)
{
    const unsigned target = id ? m_session.resolve(id) : m_session.selection().primary();
    dice::hfe::Mat4 m;
    if (!EditSession::transformOf(target, m))
    {
        result = id ? "no such object in the level" : "nothing is selected";
        return false;
    }
    const float origin[3] = { m.m[12], m.m[13], m.m[14] };
    float x = 0.0f, y = 0.0f;
    if (!m_camera.worldToPixel(origin, m_session.viewWidth(), m_session.viewHeight(), x, y))
    {
        result = EditSession::describe(target) + " is behind the camera";
        return false;
    }
    mouseMove(x, y, result);
    char text[96];
    std::snprintf(text, sizeof(text), "the mouse is at %.1f %.1f", x, y);
    result = text;
    return true;
}

bool TestHarness::mouseToPart(int index, std::string& result)
{
    const std::vector<EditSession::PreviewPart> parts = m_session.previewParts();
    if (index < 0 || index >= static_cast<int>(parts.size()))
    {
        result = "the preview has no part " + std::to_string(index);
        return false;
    }
    const dice::hfe::Mat4& m = parts[static_cast<size_t>(index)].world;
    const float origin[3] = { m.m[12], m.m[13], m.m[14] };
    float x = 0.0f, y = 0.0f;
    if (!m_camera.worldToPixel(origin, m_session.viewWidth(), m_session.viewHeight(), x, y))
    {
        result = "the part is behind the camera";
        return false;
    }
    mouseMove(x, y, result);
    char text[96];
    std::snprintf(text, sizeof(text), "the mouse is at %.1f %.1f", x, y);
    result = text;
    return true;
}

bool TestHarness::mouseToAreaPoint(const std::string& area, int index, std::string& result)
{
    LevelObjectFile::CombatAreaData data;
    if (!EditSession::engineCombatArea(area, data) || index < 0 || index >= static_cast<int>(data.points.size()))
    {
        result = "no point " + std::to_string(index) + " in a combat area named '" + area + "'";
        return false;
    }
    // Where the view draws its handle (ViewportTools::areaPointOnScreen): a metre above the terrain.
    float ground = 0.0f;
    if (!m_session.groundHeight(data.points[index].first, data.points[index].second, ground))
        ground = m_camera.y() - 20.0f;
    const float at[3] = { data.points[index].first, ground + 1.0f, data.points[index].second };
    float x = 0.0f, y = 0.0f;
    if (!m_camera.worldToPixel(at, m_session.viewWidth(), m_session.viewHeight(), x, y))
    {
        result = "that point is behind the camera";
        return false;
    }
    mouseMove(x, y, result);
    char text[96];
    std::snprintf(text, sizeof(text), "the mouse is at %.1f %.1f", x, y);
    result = text;
    return true;
}

bool TestHarness::mouseToAreaEdge(const std::string& area, int index, std::string& result)
{
    LevelObjectFile::CombatAreaData data;
    if (!EditSession::engineCombatArea(area, data) || index < 0 || index >= static_cast<int>(data.points.size()))
    {
        result = "no point " + std::to_string(index) + " in a combat area named '" + area + "'";
        return false;
    }
    // The midpoint of the two handles on the screen, which is where the view's edge test measures.
    float screen[2][2];
    for (int end = 0; end < 2; ++end)
    {
        const std::pair<float, float>& p = data.points[(index + end) % data.points.size()];
        float ground = 0.0f;
        if (!m_session.groundHeight(p.first, p.second, ground))
            ground = m_camera.y() - 20.0f;
        const float at[3] = { p.first, ground + 1.0f, p.second };
        if (!m_camera.worldToPixel(at, m_session.viewWidth(), m_session.viewHeight(), screen[end][0], screen[end][1]))
        {
            result = "that edge is behind the camera";
            return false;
        }
    }
    const float x = (screen[0][0] + screen[1][0]) * 0.5f, y = (screen[0][1] + screen[1][1]) * 0.5f;
    mouseMove(x, y, result);
    char text[96];
    std::snprintf(text, sizeof(text), "the mouse is at %.1f %.1f", x, y);
    result = text;
    return true;
}

bool TestHarness::mouseToSound(const std::string& name, std::string& result)
{
    EditSession::SoundRow row;
    if (!m_session.soundRow(name, row) || !row.positioned)
    {
        result = "no positioned ambient sound named '" + name + "'";
        return false;
    }
    float x = 0.0f, y = 0.0f;
    if (!m_camera.worldToPixel(row.position, m_session.viewWidth(), m_session.viewHeight(), x, y))
    {
        result = "that sound is behind the camera";
        return false;
    }
    mouseMove(x, y, result);
    char text[96];
    std::snprintf(text, sizeof(text), "the mouse is at %.1f %.1f", x, y);
    result = text;
    return true;
}

bool TestHarness::mouseToGround(float x, float z, std::string& result)
{
    float point[3] = { x, 0.0f, z };
    if (!m_session.groundHeight(x, z, point[1]))
    {
        result = "no ground there";
        return false;
    }
    float px = 0.0f, py = 0.0f;
    if (!m_camera.worldToPixel(point, m_session.viewWidth(), m_session.viewHeight(), px, py))
    {
        result = "that point is behind the camera";
        return false;
    }
    mouseMove(px, py, result);
    char text[96];
    std::snprintf(text, sizeof(text), "the mouse is at %.1f %.1f", px, py);
    result = text;
    return true;
}

bool TestHarness::mouseToMinimap(float x, float z, std::string& result)
{
    float px = 0.0f, py = 0.0f;
    if (!minimapScreenOf(x, z, px, py))
    {
        result = "the Minimap panel has drawn no map";
        return false;
    }
    mouseMove(px, py, result);
    char text[96];
    std::snprintf(text, sizeof(text), "the mouse is at %.1f %.1f", px, py);
    result = text;
    return true;
}

bool TestHarness::mouseToItem(const std::string& item, std::string& result)
{
    std::string label = item, window;
    int nth = 0;
    const size_t bar = label.find('|');
    if (bar != std::string::npos)
    {
        window = label.substr(0, bar);
        label = label.substr(bar + 1);
    }
    const size_t at = label.rfind('@');
    if (at != std::string::npos && at + 1 < label.size() && label.find_first_not_of("0123456789", at + 1) == std::string::npos)
    {
        nth = std::atoi(label.c_str() + at + 1);
        label = label.substr(0, at);
    }
    // The last frame drawn whole; the one being drawn when that has none.
    const SeenFrame* frames[2] = { &g_drawn, &g_drawing };
    for (int f = 0; f < 2; ++f)
    {
        int seen = 0;
        const std::vector<SeenItem>& items = frames[f]->items;
        for (size_t i = 0; i < items.size(); ++i)
        {
            if ((items[i].label != label && items[i].raw != label)
                || (!window.empty() && items[i].window.compare(0, window.size(), window) != 0))
                continue;
            if (seen++ != nth)
                continue;
            mouseMove(items[i].x, items[i].y, result);
            char text[160];
            std::snprintf(text, sizeof(text), "the mouse is at %.1f %.1f (%s)", items[i].x, items[i].y, items[i].window.c_str());
            result = text;
            return true;
        }
    }
    result = "no item labelled '" + label + "'" + (nth ? " @" + std::to_string(nth) : std::string())
           + (window.empty() ? std::string() : " in a window named " + window + "...") + " was drawn";
    return false;
}

bool TestHarness::mouseToAxis(int axis, float pixels, std::string& result)
{
    if (axis < 0 || axis > 2)
    {
        result = "the axis is 0 (X), 1 (Y) or 2 (Z)";
        return false;
    }
    // The gizmo is the picked preview part's while one is picked, else the selection's.
    dice::hfe::Mat4 m;
    const std::vector<EditSession::PreviewPart> parts =
        m_session.previewPartsShown() && m_session.pickedPreviewPart() >= 0 ? m_session.previewParts()
                                                                            : std::vector<EditSession::PreviewPart>();
    if (!parts.empty() && m_session.pickedPreviewPart() < static_cast<int>(parts.size()))
        m = parts[static_cast<size_t>(m_session.pickedPreviewPart())].world;
    else if (!EditSession::transformOf(m_session.selection().primary(), m))
    {
        result = "nothing is selected";
        return false;
    }
    // The world axis through the object's origin, as projected: the direction the gizmo draws it.
    const float origin[3] = { m.m[12], m.m[13], m.m[14] };
    float tip[3] = { origin[0], origin[1], origin[2] };
    tip[axis] += 1.0f;
    float x0, y0, x1, y1;
    const int width = m_session.viewWidth(), height = m_session.viewHeight();
    if (!m_camera.worldToPixel(origin, width, height, x0, y0) || !m_camera.worldToPixel(tip, width, height, x1, y1))
    {
        result = "the axis is behind the camera";
        return false;
    }
    const float dx = x1 - x0, dy = y1 - y0;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1e-3f)
    {
        result = "the axis points at the camera";
        return false;
    }
    mouseMove(x0 + dx / length * pixels, y0 + dy / length * pixels, result);
    char text[96];
    std::snprintf(text, sizeof(text), "the mouse is at %.1f %.1f", m_mouse[0], m_mouse[1]);
    result = text;
    return true;
}

bool TestHarness::mouseButton(int button, bool down, std::string& error)
{
    if (button < 0 || button > 2)
    {
        error = "the button is 0 (left), 1 (right) or 2 (middle)";
        return false;
    }
    ImGui::GetIO().AddMouseButtonEvent(button, down);
    return true;
}

bool TestHarness::typeText(const std::string& text, std::string& error)
{
    if (text.empty())
    {
        error = "EditorTest.type takes the text to type";
        return false;
    }
    ImGui::GetIO().AddInputCharactersUTF8(text.c_str());
    return true;
}

bool TestHarness::mouseWheel(float steps, std::string&)
{
    ImGui::GetIO().AddMouseWheelEvent(0.0f, steps);
    return true;
}

bool TestHarness::key(const std::string& name, bool down, std::string& error)
{
    ImGuiIO& io = ImGui::GetIO();
    // The modifiers are sent as ImGui's modifier keys, which set io.KeyCtrl and friends. The Win32
    // backend releases a LeftShift it does not see held (ImGui_ImplWin32_ProcessKeyEventsWorkarounds), so
    // Shift is sent as the modifier alone.
    if (_stricmp(name.c_str(), "ctrl") == 0)
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, down);
        io.AddKeyEvent(ImGuiKey_LeftCtrl, down);
        return true;
    }
    if (_stricmp(name.c_str(), "shift") == 0)
    {
        io.AddKeyEvent(ImGuiMod_Shift, down);
        return true;
    }
    if (_stricmp(name.c_str(), "alt") == 0)
    {
        io.AddKeyEvent(ImGuiMod_Alt, down);
        io.AddKeyEvent(ImGuiKey_LeftAlt, down);
        return true;
    }
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
    {
        const char* keyName = ImGui::GetKeyName(static_cast<ImGuiKey>(k));
        if (keyName && _stricmp(keyName, name.c_str()) == 0)
        {
            io.AddKeyEvent(static_cast<ImGuiKey>(k), down);
            return true;
        }
    }
    error = "no ImGui key named '" + name + "'";
    return false;
}

bool TestHarness::dragTemplate(const std::string& path, std::string& error)
{
    if (ResourceBrowser::templateOf(path).empty())
    {
        error = "'" + path + "' is not a template file under Objects/";
        return false;
    }
    m_dragPath = path;
    m_dragSeenDown = false;
    return true;
}

} // namespace bf2editor

// ImGui's test-engine hooks (the editor's ImGui is built with IMGUI_ENABLE_TEST_ENGINE, CMakeLists.txt). Only the
// item's label and box are kept, for EditorTest.mouseToItem; there is no test engine.
void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID, const ImRect&, const ImGuiLastItemData*)
{
}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext* ctx, ImGuiID, const char* label, ImGuiItemStatusFlags)
{
    bf2editor::noteItem(ctx, label);
}

void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...)
{
}

const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID)
{
    return 0;
}
