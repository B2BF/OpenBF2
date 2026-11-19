// [CHOSEN, not recovered] Native adaptation of the HUD layout policy in
// https://github.com/marekzajac97/BF2-HUD-fix (GPL-3.0), by marekzajac97.
// That project credits BadSanta12345's Alpha Project HUD fix as inspiration.
// No ASI hooks, retail addresses or ABI wrappers are used. Issue #33; docs/widescreen-hud.md.
#include "HudWidescreen.h"
#if !DEDICATED
#include "../External/BFMeme/Node.h"
#include "../External/BFMeme/TransformLayout.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <limits>
#include <unordered_map>

namespace dice { namespace hfe { namespace hud { namespace widescreen {
namespace {
bool s_vanilla = false;
std::map<meme::System*, float> s_widths;

enum class HorizontalLayout { Left, Right, FullWidth };

const std::unordered_map<std::string, HorizontalLayout> layouts = {
    {"IngameHudRight", HorizontalLayout::Right},
    {"BottomRightAnimateHud", HorizontalLayout::Right},
    {"BottomRightStaticHud", HorizontalLayout::Right},
    {"Chat", HorizontalLayout::Left},
    {"IngameHudLeft", HorizontalLayout::Left},
    {"BottomLeftStaticHud", HorizontalLayout::Left},
    {"BottomLeftAnimateHud", HorizontalLayout::Left},
    {"Ah1zTVFrame", HorizontalLayout::FullWidth},
    {"F15TVFrame", HorizontalLayout::FullWidth},
    {"F18TVFrame", HorizontalLayout::FullWidth},
    {"HavocTVFrame", HorizontalLayout::FullWidth},
    {"Su30TVFrame", HorizontalLayout::FullWidth},
    {"Su34TVFrame", HorizontalLayout::FullWidth},
    {"Z10TVFrame", HorizontalLayout::FullWidth},
    {"WhiteOutScreen", HorizontalLayout::FullWidth}
};
const char* const rightChildren[] = {
    "ItemSelectionHud", "WarningIconBackground", "WarningIcon", "CloseToMedicBackground",
    "CloseToMedic", "CloseToRepairBackground", "CloseToRepair", "CloseToAmmoBackground",
    "CloseToAmmo", "CloseToMineBackground", "CloseToMine", "ParachuteIcon", "ParachuteBackground",
    "MedalArea", "MinSizeAlpha", "MinSizeMap", "TicketInfo"
};
const char* const leftChildren[] = { "VoipList", "IngameChatInput", "PlayerNames" };
template<std::size_t N> bool contains(const char* name, const char* const (&names)[N])
{
    for (const char* candidate : names)
        if (std::strcmp(name, candidate) == 0) return true;
    return false;
}
void filter(meme::System* system, meme::Node* node, meme::Rect& bounds)
{
    if (enabled(system)) adjustBounds(system, node->getName(), bounds);
}
}

void configureMod(const std::string& modPath)
{
    std::string path = modPath;
    for (char& c : path) {
        if (c == '\\') c = '/';
        else if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    while (!path.empty() && path.back() == '/') path.pop_back();
    s_vanilla = path == "mods/bf2" ||
        (path.size() > 9 && path.compare(path.size() - 9, 9, "/mods/bf2") == 0);
    if (s_vanilla) {
        std::fputs("widescreen HUD: vanilla layout enabled (BF2-HUD-fix inspired)\n", stderr);
        std::fflush(stderr);
    }
}
void attach(meme::System* system)
{
    if (!system || !s_vanilla) return;
    s_widths[system] = 800.0f;
    meme::transformBoundsFilter = &filter;
}
void detach(meme::System* system)
{
    s_widths.erase(system);
    if (s_widths.empty() && meme::transformBoundsFilter == &filter)
        meme::transformBoundsFilter = nullptr;
}
bool enabled(meme::System* system) { return s_widths.find(system) != s_widths.end(); }
void updateViewport(meme::System* system, float pixelWidth, float pixelHeight)
{
    auto it = s_widths.find(system);
    if (it == s_widths.end() || !std::isfinite(pixelWidth) || !std::isfinite(pixelHeight)
        || pixelWidth <= 0.0f || pixelHeight <= 0.0f) return;
    const double logical = 600.0 * static_cast<double>(pixelWidth) / pixelHeight;
    if (logical > std::numeric_limits<float>::max()) return;
    it->second = static_cast<float>(std::max(800.0, logical));
}
float width(meme::System* system)
{
    const auto it = s_widths.find(system);
    return it == s_widths.end() ? 800.0f : it->second;
}
float edgeOffset(meme::System* system) { return (width(system) - 800.0f) * 0.5f; }
const char* parentFor(const std::string& name)
{
    if (contains(name.c_str(), rightChildren)) return "IngameHudRight";
    if (contains(name.c_str(), leftChildren)) return "IngameHudLeft";
    return nullptr;
}
bool makeMovable(const std::string& name)
{
    const auto layout = layouts.find(name);
    return layout != layouts.end() && layout->second != HorizontalLayout::FullWidth;
}
void adjustBounds(meme::System* system, const char* name, meme::Rect& bounds)
{
    if (!name) return;
    const auto viewport = s_widths.find(system);
    if (viewport == s_widths.end()) return;
    const auto layout = layouts.find(name);
    if (layout == layouts.end()) return;
    const float offset = (viewport->second - 800.0f) * 0.5f;
    switch (layout->second) {
    case HorizontalLayout::Left:
        bounds.x -= offset;
        break;
    case HorizontalLayout::Right:
        bounds.x += offset;
        break;
    case HorizontalLayout::FullWidth:
        bounds.x -= offset;
        bounds.width *= viewport->second / 800.0f;
        break;
    }
}
} } } }
#endif
