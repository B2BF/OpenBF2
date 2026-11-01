// Renderer-owned postprocessing state, TV submission and tinnitus frame history.
#ifndef BF2_RENDDX9_POSTPRODUCTIONFRAMEWORK_H
#define BF2_RENDDX9_POSTPRODUCTIONFRAMEWORK_H

namespace dice { namespace hfe { namespace dx9rend {
struct EngineTexture;

float postProductionGetTVInterference();
void postProductionSetTVInterference(float value);
float postProductionGetTVGranularity();
void postProductionSetTVGranularity(float value);
float postProductionGetTVAmbient();
void postProductionSetTVAmbient(float value);
void postProductionGetTVColor(float rgb[3]);
void postProductionSetTVColor(const float rgb[3]);
bool postProductionGetTVEnabled();
void postProductionSetTVEnabled(bool enabled);

void postProductionPrepare();
void postProductionPass();
void postProductionFinish();

// Renderer storage remains private to BF2Render.cpp. Selection does not change
// the retained scene target; setTarget updates both ownership and selection only
// when the requested texture differs from the retained one.
bool rendererPostMultisampleActive();
EngineTexture* rendererPostSceneTexture();
EngineTexture* rendererPostHistoryTexture(unsigned index);
double rendererPostTime();
void rendererPostSelectBackBuffer();
void rendererPostSetTarget(EngineTexture* texture);
void rendererPostResolveScene();
} } }
#endif
