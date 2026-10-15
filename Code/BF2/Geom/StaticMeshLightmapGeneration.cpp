
// Static-mesh silhouette rendering for lightmap generation and shadow passes.


#include "StaticMesh.h"

// RendDX9-only, like every StaticMesh TU: the engine and client globs compile this file as an
// empty translation unit, exactly as they do StaticMesh.cpp.
#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/Frontier.h"        // renddx9abi::logf - the two "removed" arms
#include <string>
#include "../Math/Mat4.h"
#include "../RendDX9/RendDX9Base.h"           // the effect seams, rendererGetRenderView
#include "../RendMisc/IRenderView.h"
#include "StaticMeshRenderer.h"               // the shadow effect, the declarations, the flags
#include "StaticMeshTemplate.h"

namespace dice {
namespace hfe {

namespace dx9rend {
// The stream / draw / state-manager seams, declared per-TU exactly as StaticMesh.cpp and
// SkyDome.cpp declare them (RendDX9Base.h publishes the effect seams but not these).
void vertexBufferBindStream(void* wrapper, unsigned int stream);
void indexBufferBind(void* wrapper);
void vertexDeclarationBind(void* wrapper);
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices,
                                  unsigned int startIndex);
void stateManagerPush();
void stateManagerPop();
}

namespace geom {

namespace {

// `push name; lea ecx, local; call string-ctor; ...; call sub_1001E3A0` - the shipped body builds
// a temporary std::string for every parameter lookup.  A formatting helper, not a recovered one.
dx9rend::ShaderParameter* staticMeshShadowParameter(dx9rend::Effect* effect, const char* name)
{
    return dx9rend::effectGetShaderParameterTyped(effect, std::string(name));
}

} // anonymous namespace


// drawSilhouette - +0x1C, sub_10199350.  Attested to StaticMeshLightmapGeneration.cpp line 278.

void StaticMesh::drawSilhouette(void* unused, int legacyArm)
{
    StaticMeshTemplate* const t = static_cast<StaticMeshTemplate*>(m_template);
    if (!t->m_loaded)                                                     // +0x4C
        return;

    StaticMeshRenderer* const smr = staticMeshRenderer();
    if (t->m_hasAlphaMaterial || !smr->m_flags60[3])                      // +0x186, +0x63
    {
        drawSilhouetteSlow(unused, legacyArm);
        return;
    }

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();  // +0x48
    if (m_byte358)
    {
        fullInvert(*m_transform, m_matrix290);
        m_byte358 = 0;
    }

    if (legacyArm != 0)
    {
        ::renddx9abi::logf("StaticMeshLightmapGeneration.cpp:278 MattiasH removed me");
        return;
    }

    Mat4 worldViewProjection = Mat4::getIdentity();
    Mat4 worldView = Mat4::getIdentity();
    const Mat4 inverseView(*view->getInverseViewMatrixPtr());             // +0x68, sub_10030DD0
    worldView.fullMult(*m_transform, inverseView);
    worldViewProjection.fullMult(worldView, *view->getProjectionMatrixPtr());   // +0x6C
    (void)view->getViewMatrixPtr();                                       // +0x64, discarded

    t->mergeShadowIndexRanges();
    dx9rend::vertexDeclarationBind(smr->m_decl38);                        // +0x38
    dx9rend::vertexBufferBindStream(t->m_vertexStream, 0);
    dx9rend::indexBufferBind(t->m_indexBuffer);
    smr->m_worldViewProj->setValue(&worldViewProjection, 0);              // +0x48, slot +0x18

    dx9rend::effectSetTechniqueByName(smr->m_shadowEffect, std::string("lightmapGeneration"));
    dx9rend::effectBegin(smr->m_shadowEffect);
    dx9rend::effectBeginPass(smr->m_shadowEffect, 0);

    const StaticMeshLod* const lodRecord = m_dword30C;
    dx9rend::rendererDrawIndexedPrimitive(
        lodRecord->mergedPrimitiveCount, t->m_primitiveType,
        static_cast<int>(lodRecord->vertexStart), 0, lodRecord->mergedVertexCount, 0);
    dx9rend::effectEnd(smr->m_shadowEffect);
}


// drawSilhouetteSlow - sub_10198DF0.  Line 207 of the same file.

void StaticMesh::drawSilhouetteSlow(void* unused, int legacyArm)
{
    (void)unused;
    StaticMeshTemplate* const t = static_cast<StaticMeshTemplate*>(m_template);
    StaticMeshRenderer* const smr = staticMeshRenderer();
    dx9rend::Effect* const effect = smr->m_shadowEffect;

    dx9rend::stateManagerPush();
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();  // +0x48
    if (m_byte358)
    {
        fullInvert(*m_transform, m_matrix290);
        m_byte358 = 0;
    }

    {
        const float* const packed = staticMeshPackVec4(
            reinterpret_cast<const float*>(&t->m_decompressScale), 1.0f);    // template +0x138
        staticMeshShadowParameter(effect, "POSUNPACK")->setValue(packed, 0);
    }
    staticMeshShadowParameter(effect, "TEXUNPACK")->setFloat(t->m_uvDecompress);   // +0x24

    if (legacyArm != 0)
    {
        ::renddx9abi::logf("StaticMeshLightmapGeneration.cpp:207 Mattiash removed this");
    }
    else
    {
        Mat4 worldViewProjection = Mat4::getIdentity();
        Mat4 worldView = Mat4::getIdentity();
        const Mat4 inverseView(*view->getInverseViewMatrixPtr());         // +0x68
        worldView.fullMult(*m_transform, inverseView);
        worldViewProjection.fullMult(worldView, *view->getProjectionMatrixPtr());   // +0x6C
        (void)view->getViewMatrixPtr();                                   // +0x64, discarded

        dx9rend::vertexDeclarationBind(t->m_compressVertexData ? smr->m_decl3C : smr->m_decl38);
        dx9rend::vertexBufferBindStream(t->m_vertexStream, 0);
        if (t->m_uvStreams[0])                                            // +0x158
            dx9rend::vertexBufferBindStream(t->m_uvStreams[0], 1);
        dx9rend::indexBufferBind(t->m_indexBuffer);
        smr->m_worldViewProj->setValue(&worldViewProjection, 0);

        const unsigned int materialCount = static_cast<unsigned int>(getMaterialCount());
        for (unsigned int i = 0; i < materialCount; ++i)
        {
            StaticMeshMaterial* const material =
                m_dword310->data() + i;
            const ShaderMaterial* const shaderMaterial = material->m_shaderMaterial;   // +0x28
            if (!materialVisible(material, 0))
                continue;

            StaticMeshMaterial* const typed = material;
            const bool alphaTested = typed->m_alphaMode == 2u;
            if (alphaTested)
            {
                staticMeshShadowParameter(effect, "AlphaTest")->setBool(1);     // slot +0x1C
                dx9rend::effectSetTechniqueByName(effect, std::string("lightmapGenerationAlphaTest"));
                staticMeshShadowParameter(effect, "TEXLAYER0")->setValue(
                    shaderMaterial->textures[1], 0);
            }
            else
            {
                dx9rend::effectSetTechniqueByName(effect, std::string("lightmapGeneration"));
            }
            dx9rend::effectBegin(effect);
            dx9rend::effectBeginPass(effect, 0);
            dx9rend::rendererDrawIndexedPrimitive(
                typed->m_primitiveCount, t->m_primitiveType,
                static_cast<int>(typed->m_vertexStart), 0,
                typed->m_vertexCount, typed->m_indexStart);
            dx9rend::effectEnd(effect);
            if (alphaTested)
                staticMeshShadowParameter(effect, "AlphaTest")->setBool(0);
        }
    }

    dx9rend::stateManagerPop();
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
