// [CHOSEN, not recovered] Opt-in vanilla HUD layout, issue #33.
// Inspired by marekzajac97/BF2-HUD-fix (GPL-3.0); see docs/widescreen-hud.md for credit and scope.
#ifndef MENU_HUDWIDESCREEN_H
#define MENU_HUDWIDESCREEN_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include <string>
namespace dice { namespace meme { class System; struct Rect; } }
namespace dice { namespace hfe { namespace hud { namespace widescreen {
// Process startup only. Invalid values leave the request unchanged and return false.
bool setRequested(const std::string& value);
bool requested();
// Select the vanilla policy after all startup arguments have been parsed.
void configureMod(const std::string& modPath);
// Attach before constructing the HUD tree; detach on close/destruction. UI thread only.
void attach(meme::System*);
void detach(meme::System*);
bool enabled(meme::System*);
// Invalid/zero dimensions retain the last valid layout; inactive systems stay at 800x600.
void updateViewport(meme::System*, float pixelWidth, float pixelHeight);
float width(meme::System*);
float edgeOffset(meme::System*);

// Vanilla node construction policy. Null parent means keep the script's parent unchanged.
const char* parentFor(const std::string& name);
bool makeMovable(const std::string& name);
// Adjust a freshly computed rectangle, never persistent authored node coordinates.
void adjustBounds(meme::System*, const char* name, meme::Rect&);
} } } }
#endif
#endif
