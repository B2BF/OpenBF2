// [CHOSEN, not recovered] Shared font services for rebuilt client plugins (OpenBF2 issue #42).
#pragma once

namespace openbf2 {

constexpr char glyphServiceName[] = "OpenBF2.MenuGlyphs.v1";
constexpr char glyphListenerName[] = "OpenBF2.MenuGlyphListener.v1";

// SwiffPlayer owns the outlines. RendDX9 borrows this service during a fit.
// The "*" list searches fonts by typeface when the movie has no matching export.
struct HiDpiGlyphs
{
    unsigned int (*generation)();
    int (*candidates)(const char* list);
    const char* (*name)(const char* list, int index);
    // Fill w*h coverage values at the supplied EM size and baseline origin.
    bool (*raster)(const char* list, int index, unsigned short code, float em,
                   float x, float y, float* coverage, int w, int h, int samples);
};

// RendDX9 publishes this while a font page is waiting for a movie to load.
struct GlyphListener { void (*movieLoaded)(); };

HiDpiGlyphs& movieGlyphs();
void clearMovieGlyphs();

} // namespace openbf2
