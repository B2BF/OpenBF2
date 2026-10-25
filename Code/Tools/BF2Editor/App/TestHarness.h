// Scripted editor input through ImGui's event queue, including hidden-window runs.
// EditorTest commands move/click the mouse, type, press keys, target scene/UI items and drag templates.
// wait pauses subsequent script lines for frames. Pixel coordinates are window-relative; item targets use
// label@n for an occurrence or window|label to select a window. A dragged template lasts until mouse-up.
#ifndef BF2EDITOR_APP_TESTHARNESS_H
#define BF2EDITOR_APP_TESTHARNESS_H

#include "Framework/EditorConfig.h"

#include "Framework/EditorConsole.h"

#include <string>

namespace bf2editor
{

class EditSession;
class FreeCamera;

// A widget ImGui reports no label for (a combo's box, an image button): the item just drawn, under `label`, for
// EditorTest.mouseToItem. Nothing unless a scripted run turned ImGui's test-engine hooks on.
void noteTestItem(const char* label);

class TestHarness : public ITestHarness
{
public:
    TestHarness(EditSession& session, const FreeCamera& camera);

    // Each frame, after ImGui's NewFrame and before the UI: keeps a scripted drag-and-drop alive.
    void beforeUi();
    // The frames a wait line asked for since the last call, then 0.
    int takeWait();

    // ITestHarness
    bool wait(int frames, std::string& error);
    bool mouseMove(float x, float y, std::string& error);
    bool mouseBy(float dx, float dy, std::string& error);
    bool mouseToObject(unsigned id, std::string& result);
    bool mouseToPart(int index, std::string& result);
    bool mouseToAreaPoint(const std::string& area, int index, std::string& result);
    bool mouseToAreaEdge(const std::string& area, int index, std::string& result);
    bool mouseToSound(const std::string& name, std::string& result);
    bool mouseToGround(float x, float z, std::string& result);
    bool mouseToMinimap(float x, float z, std::string& result);
    bool mouseToItem(const std::string& item, std::string& result);
    bool typeText(const std::string& text, std::string& error);
    bool mouseToAxis(int axis, float pixels, std::string& result);
    bool mouseButton(int button, bool down, std::string& error);
    bool mouseWheel(float steps, std::string& error);
    bool key(const std::string& name, bool down, std::string& error);
    bool dragTemplate(const std::string& path, std::string& error);

private:
    TestHarness(const TestHarness&);
    TestHarness& operator=(const TestHarness&);

    EditSession&      m_session;
    const FreeCamera& m_camera;
    int               m_wait;
    float             m_mouse[2];
    std::string       m_dragPath;       // "" when no scripted drag is under way
    bool              m_dragSeenDown;   // the left button was down while the drag was held
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_TESTHARNESS_H
