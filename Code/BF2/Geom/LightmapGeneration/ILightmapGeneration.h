// Lightmap generator interface (IID_ILightmapGeneration, Core/Ids.h), the typed surface a host
// drives the renderer's generator through. [INFERRED] header placement beside the attested
// Geom/LightmapGeneration/LightmapGeneration.cpp; slot names follow the 2006 editor's console
// bindings and the dedicated ELF's DistributedLightRenderer::performTraceObject (+0x14, +0x1C).
// [CHOSEN, not recovered] the string type follows ITerrainEditable.h: the renderer image uses its
// host-string carrier, every other image the current std::string.
//
// A host reaches the object as the ClassManager simple singleton "LightmapGeneration" after the
// renderer created it (LightmapGeneration.h: createLightmapGenerationSingleton). The registered
// pointer is the object itself, and this interface is its primary base.

#ifndef BF2_GEOM_LIGHTMAPGENERATION_ILIGHTMAPGENERATION_H
#define BF2_GEOM_LIGHTMAPGENERATION_ILIGHTMAPGENERATION_H

#include "../../Core/IBase.h"

#if defined(BF2_IMAGE_RENDDX9)
#include "../../../RendDX9Abi/PluginAbi.h"    // renddx9abi::HostString
#else
#include <string>
#endif

namespace dice {
namespace hfe {
namespace geom {

#if defined(BF2_IMAGE_RENDDX9)
typedef ::renddx9abi::HostString LightmapGenerationString;
#else
typedef std::string LightmapGenerationString;
#endif

// What the generator hands its progress callback after every batch of samples.
struct LightmapGenerationProgress
{
    float                    percentDone;   // +0x00  finished share of the trace's samples, 0..100
    LightmapGenerationString status;        // +0x04  "Patch %d : [... left , total: ...]" or the object line
};

// Answer false to abort the run: the generator then stops after the current batch.
// `user` is the value given to setProgressCallback. __fastcall: progress in ECX, user in EDX.
typedef bool (__fastcall* LightmapProgressCallback)(LightmapGenerationProgress* progress,
                                                    void* user);

class ILightmapGeneration : public IBase
{
public:
    // Traces every registered object whose geometry answers ILightSampleable. Always true.
    virtual bool traceAllObjects() = 0;                                                  // +0x10
    // Traces the object manager's active object. False when it is not traceable.
    virtual bool traceActiveObject() = 0;                                                // +0x14
    // Traces the object the object manager finds under `name` (its name hash). [INFERRED] name.
    virtual bool traceObject(const LightmapGenerationString& name) = 0;                  // +0x18
    // Traces terrain patch `patch` (an ILightSampling block index), or every patch for -1.
    // False when there is no editable terrain or no sampler could be created.
    virtual bool traceTerrain(int patch) = 0;                                            // +0x1C
    // Installs the progress callback (null for none) and its user value. [INFERRED] name.
    virtual void setProgressCallback(LightmapProgressCallback callback, void* user) = 0; // +0x20

protected:
    ~ILightmapGeneration() {}
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_LIGHTMAPGENERATION_ILIGHTMAPGENERATION_H
