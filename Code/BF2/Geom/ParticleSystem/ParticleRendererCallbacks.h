// [INFERRED] private renderer callback declarations beside the attested manager TU.
// PE 10171A00/10170CE0 are thiscall members stored at sprite descriptor +0x2A0.
// The existing plugin's explicit-this fastcall bridge preserves ECX and stack arguments.
#pragma once
#include "../../../RendDX9Abi/ThunkAbi.h"
#include "ParticleRawLayout.h"
#include <vector>
namespace dice { namespace hfe {
class IBase;
namespace dx9rend { class IRenderView; }
namespace geom {
class IParticleSystemInstance;
class IParticleSystemTemplate;
class MeshParticleMesh;
void RD9_THUNK particleRendererCollectSorted(void* RD9_THUNK_EDX, dx9rend::IRenderView*,
    IParticleSystemInstance*, IParticleSystemTemplate*);
void RD9_THUNK particleRendererCollectAdditive(void* RD9_THUNK_EDX, dx9rend::IRenderView*,
    IParticleSystemInstance*, IParticleSystemTemplate*);

// The four renderers embedded in the manager keep their recovered byte layout (ParticleRawLayout.h).
// Each pointer list one of them owns is a std::vector constructed in place in its original 16-byte
// span (the particle and trail renderers' +0x08, the NSA renderer's +0x0C, the mesh renderer's
// +0x1D80; x64 moves them to 24-byte tail slots); this names the list at native `offset`. The manager
// constructs these lists with its renderers.
template<class T>
inline std::vector<T>& particleRendererList(unsigned char* renderer, unsigned int offset)
{
    static_assert(sizeof(std::vector<T>) <= (sizeof(void*) == 4 ? 16 : 24),
                  "a renderer list must fit its span");
    return *reinterpret_cast<std::vector<T>*>(renderer + offset);
}

// The retained mesh list of the MeshParticles renderer (retail +0x1D80).
inline std::vector<MeshParticleMesh*>& meshParticlesRendererMeshes(unsigned char* renderer)
{
    return particleRendererList<MeshParticleMesh*>(renderer, psmMeshRendererOffset(0x1D80));
}

// The MeshParticles renderer embedded in the published manager. MeshParticleMesh::queueDraw
// appends to its retained mesh list at +0x1D80. No null guard on `manager`.
unsigned char* particleSystemManagerMeshParticlesRenderer(IBase* manager);

// The manager's force-sorted byte (retail +0x8BA8), read by the sprite descriptor's slot 114.
// No null guard on `manager`.
unsigned char particleSystemManagerForceSortedParticles(IBase* manager);
} } }
