// [CHOSEN, not recovered] D3D9 fixed-function vertex processing (0.3.0 pieces 1-3), after the public
// D3D9 reference ("Fixed Function Vertex Pipeline", "Vertex Blending", "Vertex Fog", "Texture
// Coordinate Processing", D3DTSS_TEXCOORDINDEX / D3DTEXTURETRANSFORMFLAGS) and checked against the
// real d3d9 runtime's ProcessVertices (Code/Tests/ff_reference). Plain C++, no Metal: one key (the
// state that changes the program), the generated MSL vertex function, its constant buffer, and a CPU
// model that computes the same per-vertex results (ProcessVertices and the tests use it).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace bf2ff {

// D3DDECLUSAGE values the program reads.
enum Usage : unsigned { Position = 0, BlendWeight = 1, BlendIndices = 2, Normal = 3, PSize = 4, TexCoord = 5,
                        Color = 10, PositionT = 9 };

struct Key {
    // vertex input (from the FVF or the declaration's usages)
    bool transformed = false;      // POSITIONT / D3DFVF_XYZRHW: no transform, screen-space input
    int weights = 0;               // blend weights in the stream (0..3)
    bool indices = false;          // BLENDINDICES (D3DFVF_LASTBETA_UBYTE4 / D3DCOLOR) present
    bool indicesColor = false;     // ... as D3DCOLOR (fetched normalized, BGRA-swizzled like any colour)
    bool normal = false, diffuse = false, specular = false;
    unsigned texSets = 0;          // texture coordinate sets in the stream (0..8)
    unsigned texDims[8] = {};      // components of each set (1..4)
    // render state
    unsigned vertexBlend = 0;      // D3DRS_VERTEXBLEND: 0, 1..3 weights (256 tweening: refused)
    bool indexedBlend = false;     // D3DRS_INDEXEDVERTEXBLENDENABLE
    bool fog = false;              // vertex fog in effect: FOGENABLE with FOGTABLEMODE NONE
    unsigned fogMode = 0;          // D3DRS_FOGVERTEXMODE (D3DFOG_*); NONE: the specular alpha is the factor
    bool rangeFog = false;         // D3DRS_RANGEFOGENABLE
    bool normalizeNormals = false; // D3DRS_NORMALIZENORMALS
    bool localViewer = true;       // D3DRS_LOCALVIEWER (reflection vectors)
    // texture stages 0..7: D3DTSS_TEXCOORDINDEX and D3DTSS_TEXTURETRANSFORMFLAGS
    unsigned tci[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    unsigned ttff[8] = {};
    unsigned outputs = 8;          // texture coordinate outputs written (texcoord0..outputs-1)
    // lighting (piece 2): D3DRS_LIGHTING on untransformed input
    bool lighting = false;
    bool colorVertex = true;       // D3DRS_COLORVERTEX
    bool specularEnable = false;   // D3DRS_SPECULARENABLE
    unsigned diffuseSource = 1, specularSource = 2, ambientSource = 0, emissiveSource = 0; // D3DMCS_*
    unsigned lightTypes[8] = {};   // D3DLIGHTTYPE of each enabled light in order, 0 = none
    unsigned lights = 0;           // enabled lights (at most 8)
    std::string str() const;       // program cache key
};

// Everything the program reads besides the vertex: D3D9 row-vector matrices.
struct Constants {
    float world[256][16];          // D3DTS_WORLDMATRIX(n)
    float view[16], projection[16];
    float texture[8][16];          // D3DTS_TEXTURE0..7
    float viewport[6];             // X, Y, Width, Height, MinZ, MaxZ
    float fogStart = 0, fogEnd = 1, fogDensity = 1;
    float ambient[4] = {};         // D3DRS_AMBIENT as rgba
    float material[17] = {};       // D3DMATERIAL9: diffuse, ambient, specular, emissive rgba, power
    struct Light {                 // D3DLIGHT9 in world space (the program moves it to camera space)
        unsigned type = 0;
        float diffuse[4] = {}, specular[4] = {}, ambient[4] = {};
        float position[3] = {}, direction[3] = {};
        float range = 0, falloff = 0, attenuation0 = 0, attenuation1 = 0, attenuation2 = 0, theta = 0, phi = 0;
    } light[8];                    // the enabled lights in order (Key::lights)
};

// One vertex as the program sees it: unused fields are ignored per Key.
struct VertexIn {
    float position[4] = {0, 0, 0, 1};
    float weights[4] = {};
    unsigned char indices[4] = {};
    float normal[3] = {};
    float diffuse[4] = {1, 1, 1, 1};   // D3D9 default colours when the stream has none
    float specular[4] = {0, 0, 0, 0};
    float tex[8][4] = {};
};
struct VertexOut {
    float clip[4];                 // clip-space position (before the viewport transform)
    float screen[4];               // ProcessVertices' XYZRHW (viewport applied, rhw = 1/w)
    float diffuse[4], specular[4];
    float tex[8][4];
    float fog;                     // fog factor 0..1 (1 = no fog); ProcessVertices puts it in specular alpha
};

// MSL inputs: Metal attribute index `reg` for a declaration usage/index.
struct Input { unsigned reg, usage, index; };

// The vertex function `bf2_ff_vertex`, outputs named like MojoShader's (position, user(color0/1),
// user(texcoordN), user(fog)), constants at buffer(0) laid out by pack(), the draw's viewport fixup
// at buffer(20) (as patchMsl adds to programmable vertex shaders). No #include line.
std::string vertexMsl(const Key& k, std::vector<Input>& inputs);
// The constant buffer for vertexMsl's program.
void pack(const Key& k, const Constants& c, std::vector<std::uint8_t>& out);
// CPU model of the same program.
void process(const Key& k, const Constants& c, const VertexIn& in, VertexOut& out);
// Refusal reason for state this module does not implement yet, or nullptr.
const char* unsupported(const Key& k);

} // namespace bf2ff
