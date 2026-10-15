// Path: [CHOSEN, not recovered] split out of RaShader.h so owners that embed these blocks (Terrain,
// StaticMesh) can size them without the D3DX headers.
// The Ra shader parameter holders and the lighting, object and light-array parameter blocks. Each
// block starts with its vptr and the address of its name/offset registry (RaShader.cpp); each
// holder starts with the binder table that applies its value to an effect.

#ifndef BF2_RENDDX9_RA_RAPARAMETERBLOCKS_H
#define BF2_RENDDX9_RA_RAPARAMETERBLOCKS_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace dice {
namespace hfe {
namespace dx9rend {
namespace Ra {

// Parameter holders pair a binder table with value storage. RaParamVector holds raw element bytes;
// its binder table supplies the element stride (64 for matrices, 16 for vectors, 1 for bytes).
// RaParamPair is the matrix-pointer-array holder: `a` the address of the matrix pointers, `b` their
// count. `a` is pointer-sized: unsigned int on x86, eight bytes on x64 (issue #7).

struct RaParamFloat  { void* vptr; float value; };                        // off_101E148C / 1E1E80
struct RaParamInt    { void* vptr; int   value; };
struct RaParamRef    { void* vptr; void* value; };
struct RaParamVec3   { void* vptr; float value[3]; };
struct RaParamVec4   { void* vptr; float value[4]; };
struct RaParamPair   { void* vptr; uintptr_t a; unsigned int b; };
struct RaParamMat4   { void* vptr; float value[16]; };
struct RaParamVector { void* vptr; std::vector<unsigned char> value; };

#if defined(_M_IX86)
static_assert(sizeof(RaParamFloat)  == 0x08, "float holder");
static_assert(sizeof(RaParamInt)    == 0x08, "int holder");
static_assert(sizeof(RaParamRef)    == 0x08, "ref holder");
static_assert(sizeof(RaParamVec3)   == 0x10, "vec3 holder");
static_assert(sizeof(RaParamVec4)   == 0x14, "vec4 holder");
static_assert(sizeof(RaParamPair)   == 0x0C, "pair holder");
static_assert(sizeof(RaParamMat4)   == 0x44, "mat4 holder");
#endif

// Every holder's value follows its binder-table pointer, which is where the binders read it.
static_assert(offsetof(RaParamFloat, value) == sizeof(void*)
                  && offsetof(RaParamInt, value) == sizeof(void*)
                  && offsetof(RaParamRef, value) == sizeof(void*)
                  && offsetof(RaParamVec3, value) == sizeof(void*)
                  && offsetof(RaParamVec4, value) == sizeof(void*)
                  && offsetof(RaParamPair, a) == sizeof(void*)
                  && offsetof(RaParamMat4, value) == sizeof(void*)
                  && offsetof(RaParamVector, value) == sizeof(void*),
              "holder value at self + sizeof(void*)");


// Lighting parameters for lightmaps, bones, and shadow cascades.
// The holder at +0x08 has no shader-visible name. The `// +0xNN` comments are retail offsets; the
// native block holds std::vectors (issue #7), so offsets from shadowViewProjArray on differ.
// Constructed in place by raLightingParametersConstruct (sub_10045240) and destroyed by
// raLightingParametersDestroy (sub_10044120).
struct RaLightingParameters
{
    RaLightingParameters();
    ~RaLightingParameters();

    void*        vptr;                 // +0x000  off_101E1EB4
    const void*  registry;             // +0x004  &unk_1023E790 - this block's offset vector
    RaParamVec4  unnamed008;           // +0x008  off_101E1480, 1,1,1,1  (no registered name)
    RaParamRef   terrainLightMap;      // +0x01C  off_101E1488            "TerrainLightMap"      28
    RaParamRef   lightMap;             // +0x024  off_101E1488            "LightMap"             36
    RaParamVec4  lightMapOffset;       // +0x02C  off_101E1480, 1,1,1,1   "LightMapOffset"       44
    RaParamPair  matBones;             // +0x040  off_101E1E8C, 0, 0      "MatBones"             64
    RaParamVec4  overGrowthAmbient;    // +0x04C  off_101E1480, 1,1,1,1   "OverGrowthAmbient"    76
    RaParamMat4  shadowViewProj;       // +0x060  off_101E1E94, identity  "ShadowViewProj"       96
    RaParamVector shadowViewProjArray; // +0x0A4  off_101E1EA4, stride 64 "ShadowViewProjArray" 164
    RaParamVector clampViewProjArray;  // +0x0B8  off_101E1EA4, stride 64 "ClampViewProjArray"  184
    RaParamVector shadowPosArray;      // +0x0CC  off_101E1EAC, stride 16 "ShadowPosArray"      204
    RaParamVector shadowSizeArray;     // +0x0E0  off_101E1EAC, stride 16 "ShadowSizeArray"     224
    RaParamInt   shadowCount;          // +0x0F4  off_101E14A8, 0         "ShadowCount"         244
    // +0x0FC  16-byte clip planes, applied by RaShader::applyInstanceParameters.
    std::vector<unsigned char> tail0FC;
    unsigned int tail10C;              // +0x10C  the render-entry pool zeroes it on every acquire

private:
    RaLightingParameters(const RaLightingParameters&);
    RaLightingParameters& operator=(const RaLightingParameters&);
};

// Assigns one parameter vector's bytes. Self-assignment does nothing; an empty source releases
// the destination's storage, as the shipped vector assignment does.
inline void raAssignParamBytes(std::vector<unsigned char>& destination,
                               const std::vector<unsigned char>& source)
{
    if (&destination == &source)
        return;
    if (source.empty())
        std::vector<unsigned char>().swap(destination);
    else
        destination.assign(source.begin(), source.end());
}

// block 2 - sub_100458E0 / sub_1004D900.  0x224 bytes.  The per-object transform, wind and hemi
// block; six identity matrices and nine scalar/vector holders.  A StaticMesh instance embeds one
// at its +0x38 (Geom/StaticMesh.h), and its draw writes world, viewProjection,
// worldViewProjection, shadowProjMat, shadowTrapMat, objectSpaceCamPos, windSpeed,
// objectSpaceWorldUp and tail220 by name.
// Constructed in place by raObjectParametersConstruct (sub_100458E0) and destroyed by
// raObjectParametersDestroy (sub_1004D900). The `// +0xNN` comments are retail offsets; the native
// userData holder is a std::vector (issue #7), so offsets after it differ.
struct RaObjectParameters
{
    RaObjectParameters();
    ~RaObjectParameters();

    void*        vptr;                 // +0x000  off_101E2210
    const void*  registry;             // +0x004  &unk_1023E7D0
    RaParamMat4  world;                // +0x008  off_101E1E94, identity  "World"                 8
    RaParamMat4  viewProjection;       // +0x04C  off_101E1E94, identity  "ViewProjection"       76
    RaParamMat4  worldViewProjection;  // +0x090  off_101E1E94, identity  "WorldViewProjection" 144
    RaParamMat4  shadowProjMat;        // +0x0D4  off_101E1E94, identity  "ShadowProjMat"       212
    RaParamMat4  shadowOccProjMat;     // +0x118  off_101E1E94, identity  "ShadowOccProjMat"    280
    RaParamMat4  shadowTrapMat;        // +0x15C  off_101E1E94, identity  "ShadowTrapMat"       348
    RaParamPair  geomBones;            // +0x1A0  off_101E1E8C, 0, 0      "GeomBones"           416
    RaParamFloat transparency;         // +0x1AC  off_101E1E80, 1.0f      "Transparency"        428
    RaParamVec4  objectSpaceCamPos;    // +0x1B4  off_101E1480, 1,1,1,1   "ObjectSpaceCamPos"   436
    RaParamFloat windSpeed;            // +0x1C8  off_101E148C, 0         "WindSpeed"           456
    RaParamVec3  windDir;              // +0x1D0  off_101E1494, 0,0,0     "WindDir"             464
    RaParamVector userData;            // +0x1E0  off_101E1E9C, bytes     "UserData"            480
    RaParamFloat heightOverTerrain;    // +0x1F4  off_101E148C, 0         "HeightOverTerrain"   500
    RaParamVec4  hemiMapSkyColor;      // +0x1FC  off_101E1480, 1,1,1,1   "HemiMapSkyColor"     508
    RaParamVec3  objectSpaceWorldUp;   // +0x210  off_101E1494, 0,0,0     "ObjectSpaceWorldUp"  528
    // +0x220.  NOT a holder and not registered: the StaticMesh draw (0x100FC7D0) stores the
    // instance's camera distance here as a float, and no recovered body reads it back.  Kept as
    // the raw word the constructor leaves alone.
    unsigned int tail220;              // +0x220

private:
    RaObjectParameters(const RaObjectParameters&);
    RaObjectParameters& operator=(const RaObjectParameters&);
};

// block 3 - sub_10044F70.  0x1C bytes.  The light-array block; its destructor is inlined at
// every call site (one store, republishing the +0x08 holder vptr).
struct RaLightArrayParameters
{
    void*       vptr;        // +0x00  off_101E2210
    const void* registry;    // +0x04  &unk_1023E770
    RaParamRef  lights;      // +0x08  off_101DD3FC, 0          "Lights"      8   -> one RaLightRecord
    void*       lights1xVptr;// +0x10  off_101E1E88             "Lights_1X"  16
    void*       lights1xSelf;// +0x14  this + 8
    void*       lights1xFn;  // +0x18  sub_10046660
};

#if defined(_M_IX86)
static_assert(sizeof(RaLightArrayParameters) == 0x1C,
              "sub_1004BDC0's stack slot for it spans ebp-0x38 .. ebp-0x1C");
#endif

} // namespace Ra
} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_RENDDX9_RA_RAPARAMETERBLOCKS_H
