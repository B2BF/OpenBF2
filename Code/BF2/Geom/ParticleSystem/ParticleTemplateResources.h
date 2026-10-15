// [INFERRED] private implementation split for the identical sprite/NSA texture-load sequence.
// BF2 1.50 RendDX9 PE 10176C20 / 101784E0; no corresponding dedicated ELF renderer body.
#ifndef BF2_GEOM_PARTICLESYSTEM_PARTICLETEMPLATERESOURCES_H
#define BF2_GEOM_PARTICLESYSTEM_PARTICLETEMPLATERESOURCES_H

#include "../../RendDX9/Texture.h"
#include "../../RendDX9/TextureAtlas/TextureAtlas.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../../RendDX9Abi/EngineFile.h"
#include <cstdio>
#include <cstdlib>

namespace dice { namespace hfe { namespace geom {

// Manager an atlas-owned region, with no added reference for the caller.
dx9rend::TextureAtlasEntry* particleSystemFindTextureRegion(const std::string& name);

// [CHOSEN helper factoring, recovered body] Both retail loaders reject an empty name before
// releasing the old direct texture or replacing the region. The atlas success arm leaves the
// direct texture null: the region's page owns the texture used by rendering.
inline bool particleTemplateLoadTexture(dx9rend::EngineTexture*& texture,
                                       dx9rend::TextureAtlasEntry*& region,
                                       const std::string& name,
                                       const char* qualifiedName, unsigned int diagnosticLine)
{
    if (name.empty())
        return false;

    dx9rend::textureReleaseRef(texture);
    region = particleSystemFindTextureRegion(name);
    if (region)
        return true;

    std::string path(name);
    path += ".dds";
    if (::renddx9abi::engineFileExists(path))
    {
        dx9rend::EngineTexture* const loaded =
            static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(name));
        // PE 1014DFD0 adopts the loader's reference after releasing the slot. Keep the second
        // release even though the earlier release normally made it null: the load may call out.
        dx9rend::textureReleaseRef(texture);
        texture = loaded;
    }
    else
    {
        // [TEMP] [CHOSEN, not recovered] The only remaining loader boundary is the configurable
        // Debug type-6 dispatch, not the normal texture/atlas path.
        // [RETIRE-WHEN: particle-template-texture-debug-dispatch]
        std::fprintf(stderr, "[FRONTIER] %s (Debug type 6, line %u): File %s not found!\n",
                     qualifiedName, diagnosticLine, name.c_str());
        std::fflush(stderr);
        std::abort();
    }
    return texture != 0;
}

} } }
#endif
