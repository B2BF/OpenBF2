// [TEMP] Content-free tests of issue #33's intentional layout policy.
// System pointers are identity tokens, never dereferenced. This does not model a renderer,
// publish HUD nodes, or claim native gameplay coverage.
#include "../BF2/Menu/HudWidescreen.h"
#include "../BF2/External/BFMeme/TransformLayout.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

// [TEMP] The framework's callback storage only. Production owns it in TransformNode.cpp;
// this fixture links the real HUD policy without the renderer/Node publication dependencies.
namespace dice { namespace meme { TransformBoundsFilter transformBoundsFilter = nullptr; } }
namespace ws = dice::hfe::hud::widescreen;
using dice::meme::Rect;
namespace {
unsigned checks = 0, failures = 0;
void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
}
bool near(float a, float b) { return std::fabs(a - b) < 0.002f; }
float pixelX(float x, float canvas, float pixels) { return (x / canvas + 0.5f) * pixels; }
}
int main()
{
    int firstToken, secondToken;
    auto* first = reinterpret_cast<dice::meme::System*>(&firstToken);
    auto* second = reinterpret_cast<dice::meme::System*>(&secondToken);
    ws::configureMod("mods/bf2");
    ws::attach(first);
    check(!ws::requested() && !ws::enabled(first), "disabled by default");
    check(ws::setRequested("1"), "explicit opt in");
    check(!ws::setRequested("true") && !ws::setRequested("-1")
        && !ws::setRequested("1.0") && !ws::setRequested("") && ws::requested(),
        "invalid option cannot change the requested state");
    ws::configureMod("mods/xpack");
    ws::attach(first);
    check(!ws::enabled(first) && ws::requested(), "unsupported mod retains request for next restart");
    ws::configureMod("Mods\\BF2\\");
    ws::attach(first);
    check(ws::enabled(first) && dice::meme::transformBoundsFilter, "vanilla HUD installs policy");
    check(!ws::enabled(second) && ws::width(second) == 800.0f, "other UI systems stay unchanged");

    // Golden logical widths include 4:3, 5:4, 16:10, 16:9, 2560x1080 and 32:9; resize back last.
    const float cases[][3] = {{800,600,800}, {1280,1024,800}, {1920,1200,960}, {1920,1080,1066.666667f},
        {2560,1080,1422.222222f}, {5120,1440,2133.333333f}, {800,600,800}};
    for (const auto& row : cases) {
        ws::updateViewport(first, row[0], row[1]);
        check(near(ws::width(first), row[2]), "golden canvas width after resolution change");
        const float canvas = ws::width(first);
        const float scale = row[0] / canvas;
        const Rect authored = {-400, -300, 800, 600, 7};
        Rect left = authored, right = authored, overlay = authored, center = authored;
        ws::adjustBounds(first, "Chat", left);
        ws::adjustBounds(first, "IngameHudRight", right);
        ws::adjustBounds(first, "WhiteOutScreen", overlay);
        ws::adjustBounds(first, "Crosshair", center);
        check(near(pixelX(left.x, canvas, row[0]), 0), "chat anchor reaches left screen edge");
        check(near(pixelX(right.x + right.width, canvas, row[0]), row[0]),
            "right group reaches right screen edge");
        check(near(pixelX(overlay.x, canvas, row[0]), 0)
            && near(pixelX(overlay.x + overlay.width, canvas, row[0]), row[0]),
            "fullscreen overlay spans both screen edges");
        check(center.x == authored.x && center.width == authored.width,
            "unknown/centered nodes retain authored geometry");
        check(left.y == -300 && right.height == 600 && overlay.z == 7,
            "vertical geometry and depth are unchanged");
        if (row[0] / row[1] >= 4.0f / 3.0f)
            check(near(scale * 100, row[1] / 6), "100-unit square has equal pixel width/height");
        for (int frame = 0; frame < 100; ++frame) {
            Rect next = authored;
            ws::adjustBounds(first, "Chat", next);
            check(next.x == left.x && next.width == left.width, "no per-frame offset accumulation");
        }
    }
    ws::updateViewport(first, 1920, 1080);
    const float valid = ws::width(first);
    ws::updateViewport(first, 1920, 0);
    ws::updateViewport(first, -1, 1080);
    ws::updateViewport(first, std::numeric_limits<float>::infinity(), 1080);
    ws::updateViewport(first, 1920, std::numeric_limits<float>::quiet_NaN());
    check(ws::width(first) == valid, "minimized/invalid viewports preserve last valid layout");
    check(std::strcmp(ws::parentFor("TicketInfo"), "IngameHudRight") == 0
        && std::strcmp(ws::parentFor("VoipList"), "IngameHudLeft") == 0
        && !ws::parentFor("Crosshair"), "representative vanilla parenting rules");
    check(ws::makeMovable("BottomRightStaticHud") && !ws::makeMovable("Crosshair"),
        "only named edge groups become movable");
    ws::attach(second);
    ws::detach(first);
    check(!ws::enabled(first) && ws::enabled(second) && dice::meme::transformBoundsFilter,
        "closing one HUD preserves another live policy");
    ws::detach(second);
    check(!dice::meme::transformBoundsFilter, "last close removes callback");
    ws::attach(first);
    check(ws::width(first) == 800, "recreated HUD starts without stale resolution");
    ws::detach(first);
    ws::setRequested("0");
    ws::attach(first);
    check(!ws::enabled(first), "explicit opt out");
    std::printf("Widescreen HUD policy: %u/%u checks passed.\n", checks-failures, checks);
    return failures ? 1 : 0;
}
