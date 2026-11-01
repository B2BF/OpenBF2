// Fullscreen grids for hemi blits, TV rendering and tinnitus history capture.
// Other private shader passes and parameter paths remain unimplemented.

#ifndef BF2_GEOMMISC_FSQUADDRAWER_H
#define BF2_GEOMMISC_FSQUADDRAWER_H

namespace dice {
namespace hfe {

namespace dx9rend {
struct EngineTexture;
}

namespace geom {

class FSQuadDrawer;

// dword_10249978 in RendDX9.dll.  ModuleGeomMisc::init publishes it and close destroys it.  Retail
// close deliberately leaves the pointer value behind after freeing the object; the implementation
// preserves that writeback quirk.
extern FSQuadDrawer* g_FSQuadDrawer;

// The allocation/construction and destruction/free halves of ModuleGeomMisc::init/close.
void initializeFSQuadDrawer();
void destroyFSQuadDrawer();

// PE HemiMapManager::makeHemiMap passes (17, source, destination, true, 0) to the private general
// FSQuadDrawer submission method.  This is that exact, deliberately narrow operation.
void fsQuadDrawerDrawHemiMap(dx9rend::EngineTexture* source,
                             dx9rend::EngineTexture* destination);

// Draws the viewport-sized quad with the effect/pass already selected by the caller.
void fsQuadDrawerDrawCurrentEffect();
// Pass zero, with state push/pop and temporary-backbuffer selection.
void fsQuadDrawerBlitToBackBuffer(dx9rend::EngineTexture* source);
// Quarter-size temporal history capture using the stock cheap Gaussian pass.
void fsQuadDrawerCaptureTinnitus(dx9rend::EngineTexture* source,
                                dx9rend::EngineTexture* destination);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOMMISC_FSQUADDRAWER_H
