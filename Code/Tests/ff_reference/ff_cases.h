// [CHOSEN, not recovered] Fixed-function vertex processing reference cases (0.3.0 pieces 1-3).
// Shared by the Windows generator (ff_reference_win.cpp, real d3d9 ProcessVertices with software
// vertex processing) and the macOS checks (Code/Tests/macos_metal_device). Plain data only: the
// generator writes what the real runtime produces for every case, the macOS side compares against it.
#pragma once

namespace ffref {

struct Mat { float m[16]; };   // D3DMATRIX, row-major, row vectors (D3D9 convention)

// Input vertex: position, normal, two blend weights, packed blend indices (UBYTE4, x = first), diffuse,
// specular, and two 2D texture coordinate sets.
struct Vertex {
    float x, y, z;
    float w0, w1, w2;       // used per case: 0..3 weights
    unsigned indices;       // D3DFVF_LASTBETA_UBYTE4 byte order
    float nx, ny, nz;
    unsigned diffuse, specular;
    float u0, v0, u1, v1;
};

struct Light {
    int type;               // 0 = none, else D3DLIGHTTYPE
    float diffuse[4], specular[4], ambient[4];
    float position[3], direction[3];
    float range, falloff, attenuation0, attenuation1, attenuation2, theta, phi;
};

struct Case {
    const char* name;
    // FVF parts of the input stream
    int weights;            // 0..3 (D3DFVF_XYZB1..B3 for 1..3; with indexedBlend the last beta is the indices)
    bool indexedBlend;      // D3DFVF_LASTBETA_UBYTE4 + D3DRS_INDEXEDVERTEXBLENDENABLE
    bool normal, diffuse, specular;
    int texSets;            // 0..2
    // render states
    int vertexBlend;        // D3DRS_VERTEXBLEND: 0 disable, 1..3 weights, 256 = D3DVBF_TWEENING (unused here)
    bool lighting, colorVertex, localViewer, normalizeNormals, specularEnable;
    int diffuseSource, specularSource, ambientSource, emissiveSource;   // D3DMCS_*
    unsigned ambient;       // D3DRS_AMBIENT
    bool fogEnable; int fogVertexMode; bool rangeFog;                    // D3DFOG_*
    float fogStart, fogEnd, fogDensity;
    // per stage 0/1: D3DTSS_TEXCOORDINDEX (index | D3DTSS_TCI_*) and D3DTSS_TEXTURETRANSFORMFLAGS
    unsigned tci[2], ttff[2];
    // transforms: world[0..3] (D3DTS_WORLDMATRIX(n)), view, projection, texture[0..1]
    Mat world[4], view, projection, texture[2];
    float viewport[6];      // X, Y, Width, Height, MinZ, MaxZ
    float material[17];     // diffuse rgba, ambient rgba, specular rgba, emissive rgba, power
    Light lights[2];
};

// The case table's size: the generator and the macOS checks refuse a table or a reference file
// with any other count (16 piece-1 transform/fog/texgen cases, 13 piece-2 lighting cases, 1 piece-3 clipping case).
const int kVertexCount = 6;
const int kCaseCount = 30;

// The fixed input vertices (6) and the case list. Built in code so both sides use the same floats.
const Vertex* vertices(int& count);
const Case* cases(int& count);

} // namespace ffref
