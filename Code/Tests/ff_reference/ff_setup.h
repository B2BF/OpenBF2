// [CHOSEN, not recovered] ff_cases.cpp case -> FFVertex.h key, constants and input vertex, shared by
// ff_check and ff_metal_check. `mats` are the case's 8 matrices (world0-3, view, projection,
// texture0-1), from the reference file in ff_check so libm differences in building them do not enter.
#pragma once
#include "ff_cases.h"
#include "../../BF2/Platform/Gfx/Metal/FFVertex.h"
#include <cstring>

namespace ffref {
inline void unpackColor(unsigned c, float* out) {   // D3DCOLOR (A8R8G8B8) -> rgba 0..1
    out[0] = ((c >> 16) & 255) / 255.0f; out[1] = ((c >> 8) & 255) / 255.0f;
    out[2] = (c & 255) / 255.0f; out[3] = (c >> 24) / 255.0f;
}
inline void setup(const Case& c, const Mat* mats, bf2ff::Key& k, bf2ff::Constants& K) {
    k = bf2ff::Key();
    k.weights = c.weights; k.indices = c.indexedBlend;
    k.normal = c.normal; k.diffuse = c.diffuse; k.specular = c.specular;
    k.texSets = unsigned(c.texSets);
    for (int i = 0; i < c.texSets; ++i) k.texDims[i] = 2;
    k.vertexBlend = unsigned(c.vertexBlend); k.indexedBlend = c.indexedBlend;
    k.fog = c.fogEnable; k.fogMode = unsigned(c.fogVertexMode); k.rangeFog = c.rangeFog;
    k.normalizeNormals = c.normalizeNormals; k.localViewer = c.localViewer;
    k.outputs = 2;
    for (int s = 0; s < 2; ++s) { k.tci[s] = c.tci[s]; k.ttff[s] = c.ttff[s]; }
    k.lighting = c.lighting; k.colorVertex = c.colorVertex; k.specularEnable = c.specularEnable;
    k.diffuseSource = unsigned(c.diffuseSource); k.specularSource = unsigned(c.specularSource);
    k.ambientSource = unsigned(c.ambientSource); k.emissiveSource = unsigned(c.emissiveSource);
    std::memset(&K, 0, sizeof K);
    for (int i = 0; i < 256; ++i) K.world[i][0] = K.world[i][5] = K.world[i][10] = K.world[i][15] = 1;
    for (int i = 0; i < 4; ++i) std::memcpy(K.world[i], mats[i].m, 64);
    std::memcpy(K.view, mats[4].m, 64); std::memcpy(K.projection, mats[5].m, 64);
    std::memcpy(K.texture[0], mats[6].m, 64); std::memcpy(K.texture[1], mats[7].m, 64);
    std::memcpy(K.viewport, c.viewport, sizeof K.viewport);
    K.fogStart = c.fogStart; K.fogEnd = c.fogEnd; K.fogDensity = c.fogDensity;
    unpackColor(c.ambient, K.ambient);
    std::memcpy(K.material, c.material, sizeof K.material);
    k.lights = 0;
    for (int i = 0; i < 2; ++i) {
        const Light& L = c.lights[i];
        if (!L.type) continue;
        bf2ff::Constants::Light& d = K.light[k.lights];
        d.type = unsigned(L.type);
        std::memcpy(d.diffuse, L.diffuse, 16); std::memcpy(d.specular, L.specular, 16); std::memcpy(d.ambient, L.ambient, 16);
        std::memcpy(d.position, L.position, 12); std::memcpy(d.direction, L.direction, 12);
        d.range = L.range; d.falloff = L.falloff; d.attenuation0 = L.attenuation0; d.attenuation1 = L.attenuation1;
        d.attenuation2 = L.attenuation2; d.theta = L.theta; d.phi = L.phi;
        k.lightTypes[k.lights++] = d.type;
    }
}
inline void vertexIn(const Case& c, const Vertex& v, bf2ff::VertexIn& x) {
    x = bf2ff::VertexIn();
    x.position[0] = v.x; x.position[1] = v.y; x.position[2] = v.z; x.position[3] = 1;
    x.weights[0] = v.w0; x.weights[1] = v.w1; x.weights[2] = v.w2;
    for (int i = 0; i < 4; ++i) x.indices[i] = (unsigned char)(v.indices >> (8 * i));
    x.normal[0] = v.nx; x.normal[1] = v.ny; x.normal[2] = v.nz;
    if (c.diffuse) unpackColor(v.diffuse, x.diffuse);
    if (c.specular) unpackColor(v.specular, x.specular);
    x.tex[0][0] = v.u0; x.tex[0][1] = v.v0; x.tex[0][3] = 1; x.tex[1][0] = v.u1; x.tex[1][1] = v.v1; x.tex[1][3] = 1;
}
} // namespace ffref
