// [CHOSEN, not recovered] See FFVertex.h. The CPU model (process) and the generated MSL
// (vertexMsl) follow the same steps in the same order; keep them in step. Matrices are uploaded
// row by row; Metal's float4x4(a, b, c, d) takes them as columns, so `M * v` in MSL is the D3D
// row-vector product v * M.
// Built in the renderer image only (the engine-wide source glob also lists it; Device.cpp has the
// same guard). The ff_reference checks define BF2_FF_STANDALONE.
#if (defined(__APPLE__) && defined(BF2_IMAGE_RENDDX9)) || defined(BF2_FF_STANDALONE)
#include "FFVertex.h"
#include <cmath>
#include <cstring>
#include <sstream>

namespace bf2ff {
namespace {
// D3DTSS_TCI_* in the upper half of D3DTSS_TEXCOORDINDEX
constexpr unsigned kPassThru = 0, kCameraNormal = 1, kCameraPosition = 2, kCameraReflection = 3, kSphereMap = 4;
constexpr unsigned kProjected = 256;   // D3DTTFF_PROJECTED

unsigned matrixCount(const Key& k) { return k.indexedBlend ? 256u : k.vertexBlend + 1u; }

// Constant buffer layout, in float4 slots.
struct Layout {
    unsigned matrices, wv, nv, proj, texture, fog, lighting, lights, size;
    explicit Layout(const Key& k) {
        matrices = matrixCount(k);
        wv = 0;                       // world_i * view, 4 slots each
        nv = wv + 4 * matrices;       // inverse transpose of (world_i * view), 3 slots each (xyz used)
        proj = nv + 3 * matrices;     // projection, 4 slots
        texture = proj + 4;           // texture transforms of the outputs, 4 slots each
        fog = texture + 4 * k.outputs;// start, end, density, mode
        lighting = fog + 1;           // ambient, material diffuse/ambient/specular/emissive, (power)
        lights = lighting + 6;        // per light 7 slots (pack)
        size = k.lighting ? lights + 7 * k.lights : lighting;
    }
};

void mul(const float* a, const float* b, float* r) {          // r = a * b, row-major 4x4
    float t[16];
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
        float s = 0; for (int x = 0; x < 4; ++x) s += a[i * 4 + x] * b[x * 4 + j];
        t[i * 4 + j] = s;
    }
    std::memcpy(r, t, sizeof t);
}
// 3x3 inverse transpose of the upper-left of m (normals: n' = n * inverse(M)^T applied as row vector,
// i.e. n' = n * transpose(inverse(M3))). Stored row-major 3x3 in r[0..8]. A singular matrix gives 0.
void normalMatrix(const float* m, float* r) {
    const float a = m[0], b = m[1], c = m[2], d = m[4], e = m[5], f = m[6], g = m[8], h = m[9], i = m[10];
    const float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    const float D = -(b * i - c * h), E = a * i - c * g, F = -(a * h - b * g);
    const float G = b * f - c * e, H = -(a * f - c * d), I = a * e - b * d;
    const float det = a * A + b * B + c * C;
    const float s = det != 0 ? 1.0f / det : 0.0f;
    // inverse(M3) = adj / det, adj = cofactor^T; inverse transpose = cofactor / det
    r[0] = A * s; r[1] = B * s; r[2] = C * s;
    r[3] = D * s; r[4] = E * s; r[5] = F * s;
    r[6] = G * s; r[7] = H * s; r[8] = I * s;
}
void xform4(const float* v, const float* m, float* r) {        // r = v * m (row vector)
    float t[4];
    for (int c = 0; c < 4; ++c) t[c] = v[0] * m[c] + v[1] * m[4 + c] + v[2] * m[8 + c] + v[3] * m[12 + c];
    std::memcpy(r, t, sizeof t);
}
void xform3n(const float* n, const float* m3, float* r) {      // r = n * m3 (3x3 row-major)
    float t[3];
    for (int c = 0; c < 3; ++c) t[c] = n[0] * m3[c] + n[1] * m3[3 + c] + n[2] * m3[6 + c];
    std::memcpy(r, t, sizeof t);
}
float saturate(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
} // namespace

std::string Key::str() const {
    std::ostringstream s;
    s << transformed << weights << indices << indicesColor << normal << diffuse << specular << ':' << texSets;
    for (unsigned i = 0; i < texSets && i < 8; ++i) s << texDims[i];
    s << ':' << vertexBlend << indexedBlend << fog << fogMode << rangeFog << normalizeNormals << localViewer << ':' << outputs;
    for (unsigned i = 0; i < outputs && i < 8; ++i) s << ',' << tci[i] << '/' << ttff[i];
    if (lighting) {
        s << ":L" << colorVertex << specularEnable << diffuseSource << specularSource << ambientSource << emissiveSource << ':';
        for (unsigned i = 0; i < lights && i < 8; ++i) s << lightTypes[i];
    }
    return s.str();
}

const char* unsupported(const Key& k) {
    if (k.vertexBlend > 3) return "IDirect3DDevice9::Draw fixed-function D3DRS_VERTEXBLEND tweening";
    if (k.fog && (k.fogMode < 1 || k.fogMode > 3)) return nullptr;   // FOGVERTEXMODE NONE: no vertex fog factor
    return nullptr;
}

void pack(const Key& k, const Constants& c, std::vector<std::uint8_t>& out) {
    const Layout L(k);
    std::vector<float> f(L.size * 4, 0.0f);
    for (unsigned i = 0; i < L.matrices; ++i) {
        float wv[16]; mul(c.world[i], c.view, wv);
        std::memcpy(&f[(L.wv + 4 * i) * 4], wv, sizeof wv);
        float n[9]; normalMatrix(wv, n);
        for (int r = 0; r < 3; ++r) for (int cc = 0; cc < 3; ++cc) f[(L.nv + 3 * i + r) * 4 + cc] = n[r * 3 + cc];
    }
    std::memcpy(&f[L.proj * 4], c.projection, 64);
    for (unsigned s = 0; s < k.outputs; ++s) std::memcpy(&f[(L.texture + 4 * s) * 4], c.texture[s], 64);
    f[L.fog * 4 + 0] = c.fogStart; f[L.fog * 4 + 1] = c.fogEnd; f[L.fog * 4 + 2] = c.fogDensity;
    f[L.fog * 4 + 3] = float(k.fogMode);
    if (k.lighting) {
        std::memcpy(&f[L.lighting * 4], c.ambient, 16);
        std::memcpy(&f[(L.lighting + 1) * 4], c.material, 64);
        f[(L.lighting + 5) * 4] = c.material[16];
        for (unsigned i = 0; i < k.lights; ++i) {
            // camera space: [0] diffuse [1] specular [2] ambient [3] position, range
            // [4] normalized direction, falloff [5] attenuation 0/1/2, cos(theta/2) [6] cos(phi/2)
            const Constants::Light& Lt = c.light[i];
            float* d = &f[(L.lights + 7 * i) * 4];
            std::memcpy(d, Lt.diffuse, 16); std::memcpy(d + 4, Lt.specular, 16); std::memcpy(d + 8, Lt.ambient, 16);
            const float p4[4] = {Lt.position[0], Lt.position[1], Lt.position[2], 1};
            xform4(p4, c.view, d + 12); d[15] = Lt.range;
            const float d4[4] = {Lt.direction[0], Lt.direction[1], Lt.direction[2], 0};
            float t[4]; xform4(d4, c.view, t);
            const float l = std::sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
            for (int j = 0; j < 3; ++j) d[16 + j] = l > 0 ? t[j] / l : 0;
            d[19] = Lt.falloff;
            d[20] = Lt.attenuation0; d[21] = Lt.attenuation1; d[22] = Lt.attenuation2; d[23] = std::cos(Lt.theta * 0.5f);
            d[24] = std::cos(Lt.phi * 0.5f);
        }
    }
    // transformed vertices read the viewport instead (screen -> clip); reuse the proj slots
    if (k.transformed) std::memcpy(&f[L.proj * 4], c.viewport, sizeof c.viewport);
    out.assign(reinterpret_cast<const std::uint8_t*>(f.data()), reinterpret_cast<const std::uint8_t*>(f.data() + f.size()));
}

// ---- CPU model ----------------------------------------------------------------------------------
namespace {
// D3D9 lighting ("Mathematics of Lighting"): in camera space, with the material colours taken from
// the vertex colours per D3DMCS_* when COLORVERTEX is on and the colour is in the stream.
const float* source(const Key& k, unsigned src, const float* material, const VertexIn& in) {
    if (k.colorVertex && src == 1 && k.diffuse) return in.diffuse;
    if (k.colorVertex && src == 2 && k.specular) return in.specular;
    return material;
}
void light(const Key& k, const Constants& c, const VertexIn& in, const float* pv, const float* nv, VertexOut& out) {
    const float* md = source(k, k.diffuseSource, c.material + 0, in);
    const float* ma = source(k, k.ambientSource, c.material + 4, in);
    const float* ms = source(k, k.specularSource, c.material + 8, in);
    const float* me = source(k, k.emissiveSource, c.material + 12, in);
    const float power = c.material[16];
    float amb[3] = {c.ambient[0], c.ambient[1], c.ambient[2]}, dif[3] = {}, spe[3] = {};
    float n[3] = {nv[0], nv[1], nv[2]};
    if (!k.normal) n[0] = n[1] = n[2] = 0;
    float eye[3] = {0, 0, -1};   // vertex to camera, non-local viewer
    if (k.localViewer) {
        const float l = std::sqrt(pv[0] * pv[0] + pv[1] * pv[1] + pv[2] * pv[2]);
        if (l > 0) for (int j = 0; j < 3; ++j) eye[j] = -pv[j] / l;
    }
    for (unsigned i = 0; i < k.lights; ++i) {
        const Constants::Light& L = c.light[i];
        float ldir[3], atten = 1, spot = 1;
        if (L.type == 3) {   // directional: towards the light = -direction (camera space)
            const float d[4] = {L.direction[0], L.direction[1], L.direction[2], 0};
            float t[4]; xform4(d, c.view, t);
            const float l = std::sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
            for (int j = 0; j < 3; ++j) ldir[j] = l > 0 ? -t[j] / l : 0;
        } else {
            const float p4[4] = {L.position[0], L.position[1], L.position[2], 1};
            float lp[4]; xform4(p4, c.view, lp);
            float v[3] = {lp[0] - pv[0], lp[1] - pv[1], lp[2] - pv[2]};
            const float d = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            if (d > L.range) continue;
            for (int j = 0; j < 3; ++j) ldir[j] = d > 0 ? v[j] / d : 0;
            const float a = L.attenuation0 + L.attenuation1 * d + L.attenuation2 * d * d;
            atten = a != 0 ? 1.0f / a : 1.0f;
            if (L.type == 2) {
                const float d4[4] = {L.direction[0], L.direction[1], L.direction[2], 0};
                float sd[4]; xform4(d4, c.view, sd);
                const float sl = std::sqrt(sd[0] * sd[0] + sd[1] * sd[1] + sd[2] * sd[2]);
                const float rho = sl > 0 ? -(ldir[0] * sd[0] + ldir[1] * sd[1] + ldir[2] * sd[2]) / sl : 0;
                const float ct = std::cos(L.theta * 0.5f), cp = std::cos(L.phi * 0.5f);
                if (rho > ct) spot = 1;
                else if (rho <= cp) spot = 0;
                else spot = std::pow((rho - cp) / (ct - cp), L.falloff);
            }
        }
        const float f = atten * spot;
        for (int j = 0; j < 3; ++j) amb[j] += L.ambient[j] * f;
        const float ndl = n[0] * ldir[0] + n[1] * ldir[1] + n[2] * ldir[2];
        if (ndl > 0) {
            for (int j = 0; j < 3; ++j) dif[j] += L.diffuse[j] * ndl * f;
            if (k.specularEnable) {
                float h[3] = {eye[0] + ldir[0], eye[1] + ldir[1], eye[2] + ldir[2]};
                const float hl = std::sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
                const float ndh = hl > 0 ? (n[0] * h[0] + n[1] * h[1] + n[2] * h[2]) / hl : 0;
                if (ndh > 0) { const float sp = std::pow(ndh, power) * f; for (int j = 0; j < 3; ++j) spe[j] += L.specular[j] * sp; }
            }
        }
    }
    for (int j = 0; j < 3; ++j) out.diffuse[j] = saturate(me[j] + ma[j] * amb[j] + md[j] * dif[j]);
    out.diffuse[3] = saturate(md[3]);
    // With SPECULARENABLE the specular output is the lit colour with the specular source's alpha;
    // without it the vertex specular passes through (checked against ProcessVertices).
    if (k.specularEnable) { for (int j = 0; j < 3; ++j) out.specular[j] = saturate(ms[j] * spe[j]); out.specular[3] = saturate(ms[3]); }
}
} // namespace

void process(const Key& k, const Constants& c, const VertexIn& in, VertexOut& out) {
    std::memset(&out, 0, sizeof out);
    const float* vp = c.viewport;
    if (k.transformed) {
        const float rhw = in.position[3], w = rhw != 0 ? 1.0f / rhw : 1.0f;
        const float nx = (in.position[0] - vp[0]) / vp[2] * 2 - 1, ny = 1 - (in.position[1] - vp[1]) / vp[3] * 2;
        const float nz = vp[5] != vp[4] ? (in.position[2] - vp[4]) / (vp[5] - vp[4]) : 0.0f;
        out.clip[0] = nx * w; out.clip[1] = ny * w; out.clip[2] = nz * w; out.clip[3] = w;
        std::memcpy(out.screen, in.position, sizeof out.screen);
        std::memcpy(out.diffuse, in.diffuse, sizeof out.diffuse);
        std::memcpy(out.specular, in.specular, sizeof out.specular);
        for (unsigned s = 0; s < k.outputs; ++s) {
            const unsigned set = k.tci[s] & 0xFFFF;
            if (set < k.texSets) std::memcpy(out.tex[s], in.tex[set], sizeof out.tex[s]);
            else { out.tex[s][3] = 1; }
        }
        out.fog = k.fog ? saturate(in.specular[3]) : 1.0f;   // FOGVERTEXMODE has no effect on XYZRHW input
        return;
    }
    // vertex blending in camera space: sum_i w_i * (p * (W_i * V)), the last weight is 1 - sum
    const unsigned n = k.vertexBlend + 1;
    float weight[4] = {1, 0, 0, 0};
    if (k.vertexBlend) {
        float sum = 0;
        for (unsigned i = 0; i < k.vertexBlend; ++i) { weight[i] = i < unsigned(k.weights) ? in.weights[i] : 0.0f; sum += weight[i]; }
        weight[k.vertexBlend] = 1 - sum;
    }
    float pv[4] = {0, 0, 0, 0}, nv[3] = {0, 0, 0};
    for (unsigned i = 0; i < n; ++i) {
        const unsigned m = k.indexedBlend ? in.indices[i] : i;   // UBYTE4 bytes; D3DCOLOR: VertexIn holds them unswizzled
        float wv[16]; mul(c.world[m], c.view, wv);
        float p[4]; const float pos[4] = {in.position[0], in.position[1], in.position[2], 1};
        xform4(pos, wv, p);
        for (int j = 0; j < 4; ++j) pv[j] += weight[i] * p[j];
        if (k.normal) {
            float nm[9]; normalMatrix(wv, nm);
            float t[3]; xform3n(in.normal, nm, t);
            for (int j = 0; j < 3; ++j) nv[j] += weight[i] * t[j];
        }
    }
    if (k.normalizeNormals) {
        const float l = std::sqrt(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);
        if (l > 0) for (float& x : nv) x /= l;
    }
    xform4(pv, c.projection, out.clip);
    const float rhw = out.clip[3] != 0 ? 1.0f / out.clip[3] : 0.0f;
    out.screen[0] = vp[0] + (1 + out.clip[0] * rhw) * vp[2] * 0.5f;
    out.screen[1] = vp[1] + (1 - out.clip[1] * rhw) * vp[3] * 0.5f;
    out.screen[2] = vp[4] + out.clip[2] * rhw * (vp[5] - vp[4]);
    out.screen[3] = rhw;
    std::memcpy(out.diffuse, in.diffuse, sizeof out.diffuse);
    std::memcpy(out.specular, in.specular, sizeof out.specular);
    if (k.lighting) light(k, c, in, pv, nv, out);
    // vertex fog: camera-space z, or the distance with RANGEFOGENABLE
    out.fog = k.fog ? saturate(in.specular[3]) : 1.0f;   // FOGVERTEXMODE NONE: the vertex's own factor
    if (k.fog && k.fogMode >= 1 && k.fogMode <= 3) {
        const float d = k.rangeFog ? std::sqrt(pv[0] * pv[0] + pv[1] * pv[1] + pv[2] * pv[2]) : pv[2];
        float f = 1;
        if (k.fogMode == 3) f = c.fogEnd != c.fogStart ? (c.fogEnd - d) / (c.fogEnd - c.fogStart) : 1.0f;
        else if (k.fogMode == 1) f = std::exp(-c.fogDensity * d);
        else f = std::exp(-(c.fogDensity * d) * (c.fogDensity * d));
        out.fog = saturate(f);
    }
    // texture coordinates per output (stage)
    for (unsigned s = 0; s < k.outputs; ++s) {
        const unsigned gen = k.tci[s] >> 16, set = k.tci[s] & 0xFFFF;
        float v[4] = {0, 0, 0, 1};
        unsigned dims = 2;
        if (gen == kPassThru) {
            dims = set < k.texSets ? k.texDims[set] : 2;
            if (set < k.texSets) for (unsigned j = 0; j < dims; ++j) v[j] = in.tex[set][j];
            for (unsigned j = dims; j < 4; ++j) v[j] = j == dims ? 1.0f : 0.0f;    // (u, v) -> (u, v, 1, 0)
            if (dims == 4) v[3] = in.tex[set][3];
        } else {
            float r[3];
            if (gen == kCameraNormal) { std::memcpy(r, nv, sizeof r); }
            else if (gen == kCameraPosition) { std::memcpy(r, pv, sizeof r); }
            else {
                // reflection of the eye vector about the camera-space normal
                float e[3] = {0, 0, 1};
                if (k.localViewer) {
                    const float l = std::sqrt(pv[0] * pv[0] + pv[1] * pv[1] + pv[2] * pv[2]);
                    if (l > 0) for (int j = 0; j < 3; ++j) e[j] = pv[j] / l;
                }
                const float d = nv[0] * e[0] + nv[1] * e[1] + nv[2] * e[2];
                for (int j = 0; j < 3; ++j) r[j] = e[j] - 2 * d * nv[j];
                if (gen == kSphereMap) {
                    const float m = 2 * std::sqrt(r[0] * r[0] + r[1] * r[1] + (r[2] - 1) * (r[2] - 1));
                    r[0] = m != 0 ? r[0] / m + 0.5f : 0.5f; r[1] = m != 0 ? r[1] / m + 0.5f : 0.5f; r[2] = 0;
                }
            }
            dims = gen == kSphereMap ? 2 : 3;
            v[0] = r[0]; v[1] = r[1]; v[2] = gen == kSphereMap ? 1.0f : r[2]; v[3] = gen == kSphereMap ? 0.0f : 1.0f;
        }
        const unsigned count = k.ttff[s] & 0xFF;
        if (count >= 1 && count <= 4) {
            float t[4]; xform4(v, c.texture[s], t);
            for (unsigned j = 0; j < 4; ++j) out.tex[s][j] = j < count ? t[j] : (j == 3 ? 1.0f : 0.0f);
        } else {
            for (unsigned j = 0; j < 4; ++j) out.tex[s][j] = j < dims ? v[j] : (j == 3 ? 1.0f : 0.0f);
        }
    }
}

// ---- MSL ----------------------------------------------------------------------------------------
std::string vertexMsl(const Key& k, std::vector<Input>& inputs) {
    const Layout L(k);
    inputs.clear();
    std::ostringstream in, body;
    unsigned reg = 0;
    auto attr = [&](const char* type, const char* name, unsigned usage, unsigned index) {
        in << "\t" << type << " " << name << " [[attribute(" << reg << ")]];\n";
        inputs.push_back({reg++, usage, index});
    };
    attr("float4", "position", k.transformed ? PositionT : Position, 0);
    if (!k.transformed && k.weights) attr("float4", "weights", BlendWeight, 0);
    if (!k.transformed && k.indices) attr("float4", "indices", BlendIndices, 0);
    if (!k.transformed && k.normal) attr("float4", "normal", Normal, 0);   // every input is a float4: one layout for stage_in and buffers
    if (k.diffuse) attr("float4", "diffuse", Color, 0);
    if (k.specular) attr("float4", "specular", Color, 1);
    for (unsigned i = 0; i < k.texSets && i < 8; ++i) {
        std::string name = "tex" + std::to_string(i);
        attr("float4", name.c_str(), TexCoord, i);
    }
    std::ostringstream out;
    out << "struct bf2_ff_in {\n" << in.str() << "};\n"
        << "struct bf2_ff_out {\n\tfloat4 position [[position, invariant]];\n\tfloat4 color0 [[user(color0)]];\n"
           "\tfloat4 color1 [[user(color1)]];\n";
    for (unsigned s = 0; s < k.outputs; ++s) out << "\tfloat4 texcoord" << s << " [[user(texcoord" << s << ")]];\n";
    out << "\tfloat4 fog [[user(fog)]];\n};\n";
    // The body is a plain function so a compute kernel can run it too (Code/Tests/ff_reference).
    out << "static bf2_ff_out bf2_ff_transform(thread const bf2_ff_in& input, constant float4 *c, float4 bf2_fixup) {\n"
        << "\tbf2_ff_out output;\n";
    body << "\toutput.color0 = " << (k.diffuse ? "input.diffuse" : "float4(1.0)") << ";\n";
    body << "\toutput.color1 = " << (k.specular ? "input.specular" : "float4(0.0)") << ";\n";
    body << "\toutput.fog = float4(" << (k.fog ? (k.specular ? "saturate(input.specular.a)" : "0.0") : "1.0") << ");\n";
    if (k.transformed) {
        const unsigned v = L.proj;   // viewport X, Y, W, H in c[v], MinZ, MaxZ in c[v+1].xy
        body << "\tconst float4 vp = c[" << v << "]; const float4 vz = c[" << v + 1 << "];\n"
             << "\tconst float w = input.position.w != 0.0 ? 1.0 / input.position.w : 1.0;\n"
             << "\tconst float nz = vz.y != vz.x ? (input.position.z - vz.x) / (vz.y - vz.x) : 0.0;\n"
             << "\toutput.position = float4(((input.position.x - vp.x) / vp.z * 2.0 - 1.0) * w,"
                " (1.0 - (input.position.y - vp.y) / vp.w * 2.0) * w, nz * w, w);\n";
        for (unsigned s = 0; s < k.outputs; ++s) {
            const unsigned set = k.tci[s] & 0xFFFF;
            body << "\toutput.texcoord" << s << " = " << (set < k.texSets ? "input.tex" + std::to_string(set) : std::string("float4(0.0, 0.0, 0.0, 1.0)")) << ";\n";
        }
    } else {
        // blending
        const unsigned n = k.vertexBlend + 1;
        body << "\tfloat w[4] = {1.0, 0.0, 0.0, 0.0};\n";
        if (k.vertexBlend) {
            body << "\tfloat sum = 0.0;\n";
            for (unsigned i = 0; i < k.vertexBlend; ++i) {
                const char comp = "xyzw"[i];
                body << "\tw[" << i << "] = " << (i < unsigned(k.weights) ? std::string("input.weights.") + comp : std::string("0.0")) << "; sum += w[" << i << "];\n";
            }
            body << "\tw[" << k.vertexBlend << "] = 1.0 - sum;\n";
        }
        body << "\tfloat4 pv = float4(0.0); float3 nv = float3(0.0);\n"
             << "\tconst float4 p = float4(input.position.xyz, 1.0);\n";
        for (unsigned i = 0; i < n; ++i) {
            const char comp = "xyzw"[i];
            // D3DCOLOR blend indices arrive as 0..1 (bgra swizzled by the fetch); UBYTE4 as 0..255
            // UBYTE4 arrives as 0..255; D3DCOLOR as 0..1 in the order a vertex shader reads it
            const std::string m = !k.indexedBlend ? std::to_string(i)
                : k.indicesColor ? "uint(input.indices." + std::string(1, comp) + " * 255.0 + 0.5)"
                : "uint(input.indices." + std::string(1, comp) + ")";
            body << "\t{ const uint m = " << m << ";\n"
                 << "\t  const float4x4 wv = float4x4(c[" << L.wv << " + 4 * m], c[" << L.wv << " + 4 * m + 1], c["
                 << L.wv << " + 4 * m + 2], c[" << L.wv << " + 4 * m + 3]);\n"
                 << "\t  pv += w[" << i << "] * (wv * p);\n";
            if (k.normal)
                body << "\t  const float3x3 nm = float3x3(c[" << L.nv << " + 3 * m].xyz, c[" << L.nv << " + 3 * m + 1].xyz, c["
                     << L.nv << " + 3 * m + 2].xyz);\n\t  nv += w[" << i << "] * (nm * input.normal.xyz);\n";
            body << "\t}\n";
        }
        if (k.normalizeNormals) body << "\tif (length(nv) > 0.0) nv = normalize(nv);\n";
        body << "\tconst float4x4 proj = float4x4(c[" << L.proj << "], c[" << L.proj + 1 << "], c[" << L.proj + 2 << "], c[" << L.proj + 3 << "]);\n"
             << "\toutput.position = proj * pv;\n";
        if (k.lighting) {
            auto src = [&](unsigned v, unsigned slot) -> std::string {
                if (k.colorVertex && v == 1 && k.diffuse) return "input.diffuse";
                if (k.colorVertex && v == 2 && k.specular) return "input.specular";
                return "c[" + std::to_string(L.lighting + 1 + slot) + "]";
            };
            body << "\t{ const float4 md = " << src(k.diffuseSource, 0) << ", ma = " << src(k.ambientSource, 1)
                 << ", ms = " << src(k.specularSource, 2) << ", me = " << src(k.emissiveSource, 3) << ";\n"
                 << "\t  const float power = c[" << L.lighting + 5 << "].x;\n"
                 << "\t  float3 amb = c[" << L.lighting << "].rgb, dif = float3(0.0), spe = float3(0.0);\n"
                 << "\t  const float3 n = " << (k.normal ? "nv" : "float3(0.0)") << ";\n"
                 << "\t  float3 eye = float3(0.0, 0.0, -1.0);\n";
            if (k.localViewer) body << "\t  if (length(pv.xyz) > 0.0) eye = -pv.xyz / length(pv.xyz);\n";
            for (unsigned i = 0; i < k.lights; ++i) {
                const unsigned b = L.lights + 7 * i;
                const std::string C = "c[" + std::to_string(b);
                body << "\t  {\n\t    float3 ldir; float f = 1.0;\n";
                if (k.lightTypes[i] == 3) body << "\t    ldir = -" << C << " + 4].xyz;\n";
                else {
                    body << "\t    const float3 v = " << C << " + 3].xyz - pv.xyz; const float d = length(v);\n"
                         << "\t    ldir = d > 0.0 ? v / d : float3(0.0);\n"
                         << "\t    const float a = " << C << " + 5].x + " << C << " + 5].y * d + " << C << " + 5].z * d * d;\n"
                         << "\t    f = d > " << C << " + 3].w ? 0.0 : (a != 0.0 ? 1.0 / a : 1.0);\n";
                    if (k.lightTypes[i] == 2)
                        body << "\t    const float rho = -dot(ldir, " << C << " + 4].xyz), ct = " << C << " + 5].w, cp = " << C << " + 6].x;\n"
                             << "\t    f *= rho > ct ? 1.0 : rho <= cp ? 0.0 : pow((rho - cp) / (ct - cp), " << C << " + 4].w);\n";
                }
                body << "\t    amb += " << C << " + 2].rgb * f;\n"
                     << "\t    const float ndl = dot(n, ldir);\n"
                     << "\t    if (ndl > 0.0 && f > 0.0) {\n\t      dif += " << C << "].rgb * ndl * f;\n";
                if (k.specularEnable)
                    body << "\t      const float3 h = eye + ldir; const float ndh = length(h) > 0.0 ? dot(n, h) / length(h) : 0.0;\n"
                         << "\t      if (ndh > 0.0) spe += " << C << " + 1].rgb * pow(ndh, power) * f;\n";
                body << "\t    }\n\t  }\n";
            }
            body << "\t  output.color0 = saturate(float4(me.rgb + ma.rgb * amb + md.rgb * dif, md.a));\n";
            if (k.specularEnable) body << "\t  output.color1 = saturate(float4(ms.rgb * spe, ms.a));\n";
            body << "\t}\n";
        }
        if (k.fog && k.fogMode >= 1 && k.fogMode <= 3) {
            body << "\t{ const float4 f = c[" << L.fog << "];\n"
                 << "\t  const float d = " << (k.rangeFog ? "length(pv.xyz)" : "pv.z") << ";\n";
            if (k.fogMode == 3) body << "\t  const float v = f.y != f.x ? (f.y - d) / (f.y - f.x) : 1.0;\n";
            else if (k.fogMode == 1) body << "\t  const float v = exp(-f.z * d);\n";
            else body << "\t  const float v = exp(-(f.z * d) * (f.z * d));\n";
            body << "\t  output.fog = float4(saturate(v)); }\n";
        }
        for (unsigned s = 0; s < k.outputs; ++s) {
            const unsigned gen = k.tci[s] >> 16, set = k.tci[s] & 0xFFFF;
            const unsigned count = k.ttff[s] & 0xFF;
            unsigned dims = 2;
            body << "\t{ float4 v = float4(0.0, 0.0, 0.0, 1.0);\n";
            if (gen == kPassThru) {
                dims = set < k.texSets ? k.texDims[set] : 2;
                if (set < k.texSets) {
                    const char* sw[5] = {"", "x", "xy", "xyz", "xyzw"};
                    body << "\t  v." << sw[dims] << " = input.tex" << set << "." << sw[dims] << ";\n";
                }
                for (unsigned j = dims; j < 4; ++j) body << "\t  v." << "xyzw"[j] << " = " << (j == dims ? "1.0" : "0.0") << ";\n";
            } else {
                if (gen == kCameraNormal) body << "\t  const float3 r = nv;\n";
                else if (gen == kCameraPosition) body << "\t  const float3 r = pv.xyz;\n";
                else {
                    body << "\t  float3 e = float3(0.0, 0.0, 1.0);\n";
                    if (k.localViewer) body << "\t  if (length(pv.xyz) > 0.0) e = pv.xyz / length(pv.xyz);\n";
                    body << "\t  float3 r = e - 2.0 * dot(nv, e) * nv;\n";
                    if (gen == kSphereMap)
                        body << "\t  const float m = 2.0 * sqrt(r.x * r.x + r.y * r.y + (r.z - 1.0) * (r.z - 1.0));\n"
                                "\t  r = m != 0.0 ? float3(r.x / m + 0.5, r.y / m + 0.5, 0.0) : float3(0.5, 0.5, 0.0);\n";
                }
                dims = gen == kSphereMap ? 2 : 3;
                body << (gen == kSphereMap ? "\t  v = float4(r.xy, 1.0, 0.0);\n" : "\t  v = float4(r, 1.0);\n");
            }
            if (count >= 1 && count <= 4) {
                const unsigned t = L.texture + 4 * s;
                body << "\t  const float4 t = float4x4(c[" << t << "], c[" << t + 1 << "], c[" << t + 2 << "], c[" << t + 3 << "]) * v;\n"
                     << "\t  output.texcoord" << s << " = float4(" << (count > 0 ? "t.x" : "0.0") << ", " << (count > 1 ? "t.y" : "0.0") << ", "
                     << (count > 2 ? "t.z" : "0.0") << ", " << (count > 3 ? "t.w" : "1.0") << ");\n";
            } else {
                body << "\t  output.texcoord" << s << " = float4(v.x, " << (dims > 1 ? "v.y" : "0.0") << ", "
                     << (dims > 2 ? "v.z" : "0.0") << ", " << (dims > 3 ? "v.w" : "1.0") << ");\n";
            }
            body << "\t}\n";
        }
        (void)kProjected;
    }
    // the draw's viewport fixup (D3D9 viewport and half-pixel centre on the clamped Metal viewport)
    body << "\toutput.position.xy = output.position.xy * bf2_fixup.zw + bf2_fixup.xy * output.position.w;\n";
    out << body.str() << "\treturn output;\n}\n"
        << "vertex bf2_ff_out bf2_ff_vertex(\n\tbf2_ff_in input [[stage_in]],\n\tconstant float4 *c [[buffer(0)]],\n"
           "\tconstant float4 &bf2_fixup [[buffer(20)]]\n) {\n\treturn bf2_ff_transform(input, c, bf2_fixup);\n}\n";
    return out.str();
}

} // namespace bf2ff
#endif
