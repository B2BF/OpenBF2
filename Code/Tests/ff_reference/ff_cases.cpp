// [CHOSEN, not recovered] See ff_cases.h. Matrices are built from exact decimal inputs with float
// arithmetic only, the same way on both platforms; they are inputs to D3D9, not reference values.
#include "ff_cases.h"
#include <cmath>
#include <cstring>
#include <vector>

namespace ffref {
namespace {
Mat identity() { Mat r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
Mat mul(const Mat& a, const Mat& b) {
    Mat r{};
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
        float s = 0; for (int k = 0; k < 4; ++k) s += a.m[i * 4 + k] * b.m[k * 4 + j];
        r.m[i * 4 + j] = s;
    }
    return r;
}
Mat translate(float x, float y, float z) { Mat r = identity(); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
Mat scale(float x, float y, float z) { Mat r{}; r.m[0] = x; r.m[5] = y; r.m[10] = z; r.m[15] = 1; return r; }
Mat rotY(float a) { Mat r = identity(); const float c = std::cos(a), s = std::sin(a); r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r; }
Mat rotX(float a) { Mat r = identity(); const float c = std::cos(a), s = std::sin(a); r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r; }
// D3DXMatrixPerspectiveFovLH, written out
Mat perspective(float fovy, float aspect, float zn, float zf) {
    Mat r{}; const float ys = 1.0f / std::tan(fovy / 2), xs = ys / aspect;
    r.m[0] = xs; r.m[5] = ys; r.m[10] = zf / (zf - zn); r.m[11] = 1; r.m[14] = -zn * zf / (zf - zn);
    return r;
}
Mat ortho(float w, float h, float zn, float zf) {
    Mat r{}; r.m[0] = 2 / w; r.m[5] = 2 / h; r.m[10] = 1 / (zf - zn); r.m[14] = zn / (zn - zf); r.m[15] = 1;
    return r;
}

const Vertex kVertices[6] = {
    // x     y     z     w0    w1    w2    indices      nx    ny    nz    diffuse     specular    u0    v0    u1    v1
    { -1.0f, -1.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0x03020100u, 0.0f, 0.0f, -1.0f, 0xFFFF0000u, 0x80202020u, 0.0f, 1.0f, 0.25f, 0.5f },
    {  1.0f, -1.0f, 3.0f, 0.5f, 0.5f, 0.0f, 0x00010203u, 0.6f, 0.0f, -0.8f, 0xFF00FF00u, 0x40404040u, 1.0f, 1.0f, 0.75f, 0.5f },
    {  0.0f,  1.5f, 4.0f, 0.25f, 0.25f, 0.25f, 0x01000302u, 0.0f, 0.707107f, -0.707107f, 0xFF0000FFu, 0x00FFFFFFu, 0.5f, 0.0f, 0.5f, 0.25f },
    {  2.5f,  0.5f, 8.0f, 0.75f, 0.0f, 0.25f, 0x02030001u, -0.6f, 0.8f, 0.0f, 0x80FFFFFFu, 0xFF000000u, 2.0f, -1.0f, 1.5f, 0.0f },
    { -3.0f,  2.0f, 20.0f, 0.0f, 1.0f, 0.0f, 0x03000102u, 0.0f, 1.0f, 0.0f, 0x00808080u, 0x10101010u, -0.5f, 0.5f, 0.0f, 1.0f },
    {  0.5f, -2.0f, 50.0f, 0.1f, 0.2f, 0.3f, 0x00000000u, 1.0f, 0.0f, 0.0f, 0xFFFFFF00u, 0x7F7F7F7Fu, 0.125f, 0.875f, 0.375f, 0.625f },
};

Light directional(float dx, float dy, float dz) {
    Light l{}; l.type = 3;
    const float d[4] = { 0.8f, 0.7f, 0.6f, 1.0f }, s[4] = { 1.0f, 1.0f, 1.0f, 1.0f }, a[4] = { 0.1f, 0.1f, 0.2f, 1.0f };
    std::memcpy(l.diffuse, d, sizeof d); std::memcpy(l.specular, s, sizeof s); std::memcpy(l.ambient, a, sizeof a);
    l.direction[0] = dx; l.direction[1] = dy; l.direction[2] = dz;
    return l;
}
Light point(float x, float y, float z, float range, float a0, float a1, float a2) {
    Light l = directional(0, 0, 1); l.type = 1;
    l.position[0] = x; l.position[1] = y; l.position[2] = z; l.range = range;
    l.attenuation0 = a0; l.attenuation1 = a1; l.attenuation2 = a2;
    return l;
}
Light spot(float x, float y, float z, float dx, float dy, float dz, float theta, float phi, float falloff) {
    Light l = point(x, y, z, 100, 1, 0.05f, 0.0f); l.type = 2;
    l.direction[0] = dx; l.direction[1] = dy; l.direction[2] = dz;
    l.theta = theta; l.phi = phi; l.falloff = falloff;
    return l;
}

Case base(const char* name) {
    Case c{};
    c.name = name;
    c.normal = true; c.diffuse = true; c.specular = true; c.texSets = 2;
    c.colorVertex = true; c.diffuseSource = 1; c.specularSource = 2; c.ambientSource = 0; c.emissiveSource = 0;
    c.ambient = 0xFF202020u;
    c.tci[0] = 0; c.tci[1] = 1;
    for (int i = 0; i < 4; ++i) c.world[i] = identity();
    c.view = identity();
    c.projection = perspective(1.0f, 4.0f / 3.0f, 1.0f, 100.0f);
    c.texture[0] = c.texture[1] = identity();
    const float vp[6] = { 0, 0, 800, 600, 0, 1 };
    std::memcpy(c.viewport, vp, sizeof vp);
    const float mat[17] = { 1, 1, 1, 1, 0.5f, 0.5f, 0.5f, 1, 0.8f, 0.8f, 0.8f, 1, 0.05f, 0.0f, 0.0f, 1, 16.0f };
    std::memcpy(c.material, mat, sizeof mat);
    return c;
}

std::vector<Case> build() {
    std::vector<Case> v;
    // ---- piece 1: transforms, viewport, blending, fog, texgen, texture transforms (lighting off) ----
    { Case c = base("transform-identity-world"); v.push_back(c); }
    { Case c = base("transform-world-view");
      c.world[0] = mul(mul(scale(1.5f, 0.5f, 2.0f), rotY(0.3f)), translate(0.5f, -0.25f, 1.0f));
      c.view = mul(translate(0.0f, -1.0f, 2.0f), rotX(0.2f)); v.push_back(c); }
    { Case c = base("transform-ortho-viewport");
      c.projection = ortho(10.0f, 7.5f, 0.5f, 60.0f);
      const float vp[6] = { 100, 50, 640, 480, 0.25f, 0.75f }; std::memcpy(c.viewport, vp, sizeof vp); v.push_back(c); }
    { Case c = base("blend-1-weight"); c.weights = 1; c.vertexBlend = 1;
      c.world[1] = translate(2.0f, 0.0f, 1.0f); v.push_back(c); }
    { Case c = base("blend-2-weights"); c.weights = 2; c.vertexBlend = 2;
      c.world[1] = translate(2.0f, 0.0f, 1.0f); c.world[2] = mul(rotY(0.5f), translate(0, 1, 0)); v.push_back(c); }
    { Case c = base("blend-3-weights"); c.weights = 3; c.vertexBlend = 3;
      c.world[1] = translate(2.0f, 0.0f, 1.0f); c.world[2] = mul(rotY(0.5f), translate(0, 1, 0));
      c.world[3] = scale(0.5f, 2.0f, 1.0f); v.push_back(c); }
    { Case c = base("blend-indexed-2"); c.weights = 2; c.indexedBlend = true; c.vertexBlend = 2;
      c.world[1] = translate(2.0f, 0.0f, 1.0f); c.world[2] = mul(rotY(0.5f), translate(0, 1, 0));
      c.world[3] = scale(0.5f, 2.0f, 1.0f); v.push_back(c); }
    { Case c = base("fog-vertex-linear"); c.fogEnable = true; c.fogVertexMode = 3; c.fogStart = 2.5f; c.fogEnd = 30.0f; v.push_back(c); }
    { Case c = base("fog-vertex-exp"); c.fogEnable = true; c.fogVertexMode = 1; c.fogDensity = 0.08f; v.push_back(c); }
    { Case c = base("fog-vertex-exp2"); c.fogEnable = true; c.fogVertexMode = 2; c.fogDensity = 0.05f; v.push_back(c); }
    { Case c = base("fog-vertex-linear-range"); c.fogEnable = true; c.fogVertexMode = 3; c.rangeFog = true;
      c.fogStart = 2.5f; c.fogEnd = 30.0f; c.view = translate(1.0f, 0.5f, 0.0f); v.push_back(c); }
    { Case c = base("tci-camera-normal"); c.tci[0] = 0x00010000u; c.ttff[0] = 3; c.view = rotY(0.4f); v.push_back(c); }
    { Case c = base("tci-camera-position"); c.tci[0] = 0x00020000u; c.ttff[0] = 3; c.view = translate(0, 0, 1); v.push_back(c); }
    { Case c = base("tci-camera-reflection"); c.tci[0] = 0x00030000u; c.ttff[0] = 3; c.view = rotX(0.25f); v.push_back(c); }
    { Case c = base("tci-spheremap"); c.tci[0] = 0x00040000u; c.ttff[0] = 0; v.push_back(c); }
    { Case c = base("texture-transform-count2");
      c.ttff[0] = 2; c.texture[0] = mul(scale(2.0f, 0.5f, 1.0f), translate(0.25f, 0.125f, 0.0f)); v.push_back(c); }
    { Case c = base("texture-transform-count3-projected");
      c.tci[0] = 0x00020000u; c.ttff[0] = 3 | 256; c.texture[0] = perspective(0.8f, 1.0f, 0.5f, 50.0f); v.push_back(c); }
    { Case c = base("texture-transform-stage1-swap"); c.tci[0] = 1; c.tci[1] = 0;
      c.ttff[1] = 2; c.texture[1] = rotX(0.5f); v.push_back(c); }
    // ---- piece 2: lighting and materials ----
    { Case c = base("light-directional"); c.lighting = true; c.lights[0] = directional(0.3f, -0.5f, 0.8f); v.push_back(c); }
    { Case c = base("light-directional-specular-localviewer"); c.lighting = true; c.specularEnable = true; c.localViewer = true;
      c.lights[0] = directional(0.3f, -0.5f, 0.8f); v.push_back(c); }
    { Case c = base("light-directional-specular-infinite"); c.lighting = true; c.specularEnable = true; c.localViewer = false;
      c.lights[0] = directional(-0.2f, -0.2f, 1.0f); v.push_back(c); }
    { Case c = base("light-point-attenuation"); c.lighting = true; c.specularEnable = true;
      c.lights[0] = point(0.0f, 2.0f, 1.0f, 25.0f, 0.5f, 0.1f, 0.01f); v.push_back(c); }
    { Case c = base("light-point-range-cutoff"); c.lighting = true;
      c.lights[0] = point(0.0f, 0.0f, 2.0f, 5.0f, 1.0f, 0.0f, 0.0f); v.push_back(c); }
    { Case c = base("light-spot"); c.lighting = true; c.specularEnable = true;
      c.lights[0] = spot(0.0f, 0.0f, -2.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.9f, 1.5f); v.push_back(c); }
    { Case c = base("light-two-lights-normalize"); c.lighting = true; c.normalizeNormals = true;
      c.world[0] = scale(2.0f, 2.0f, 2.0f);
      c.lights[0] = directional(0.3f, -0.5f, 0.8f); c.lights[1] = point(1.0f, 1.0f, 1.0f, 50.0f, 0.0f, 0.2f, 0.0f); v.push_back(c); }
    { Case c = base("light-material-sources-material"); c.lighting = true; c.colorVertex = false;
      c.lights[0] = directional(0.3f, -0.5f, 0.8f); v.push_back(c); }
    { Case c = base("light-material-sources-vertex"); c.lighting = true; c.specularEnable = true;
      c.diffuseSource = 2; c.specularSource = 1; c.ambientSource = 1; c.emissiveSource = 2;
      c.lights[0] = directional(0.3f, -0.5f, 0.8f); v.push_back(c); }
    { Case c = base("light-no-normals"); c.lighting = true; c.normal = false;
      c.lights[0] = directional(0.3f, -0.5f, 0.8f); v.push_back(c); }
    { Case c = base("light-blend-2-fog"); c.lighting = true; c.weights = 2; c.vertexBlend = 2;
      c.world[1] = translate(2.0f, 0.0f, 1.0f); c.world[2] = rotY(0.5f);
      c.fogEnable = true; c.fogVertexMode = 3; c.fogStart = 2.0f; c.fogEnd = 40.0f;
      c.lights[0] = point(0.0f, 2.0f, 1.0f, 25.0f, 0.5f, 0.1f, 0.01f); v.push_back(c); }
    // ---- piece 3: ProcessVertices with vertices outside the view volume (behind the camera, in
    // front of the near plane, beside the frustum), for the "clip" variant ----
    { Case c = base("clip-outside"); c.world[0] = mul(translate(0.0f, 0.0f, -2.5f), scale(1.0f, 1.0f, 1.0f));
      c.world[0].m[12] = 0.0f; c.world[0].m[0] = 3.0f; v.push_back(c); }
    return v;
}
} // namespace

const Vertex* vertices(int& count) { count = 6; return kVertices; }
const Case* cases(int& count) {
    static const std::vector<Case> all = build();
    count = static_cast<int>(all.size());
    return all.data();
}

} // namespace ffref
