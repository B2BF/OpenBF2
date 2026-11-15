// [CHOSEN, not recovered] G4 draw bridge between the D3D9 device (Device.cpp) and Metal
// (MetalBridge.mm). No D3D or Objective-C types cross it.
#ifndef BF2_METAL_DRAWBRIDGE_H
#define BF2_METAL_DRAWBRIDGE_H
#include "MetalBridge.h"
#include <cstdint>
#include <string>
#include <vector>
namespace bf2metal {
struct Program;
// One translated shader. Vertex inputs are MojoShader's attribute(register) slots with their
// D3D declaration usage/index; samplers are texture/sampler slots with MojoShader's dimension
// (0 2D, 1 cube, 2 volume). The MSL already carries the D3D-convention fixups (see Device.cpp).
struct ProgramInput { unsigned reg, usage, index; };
struct ProgramSampler { unsigned slot, kind; };
struct ProgramDesc {
    bool vertex = true;
    bool variant = false; // a pixel shader variant (Shaders.inc FragmentVariant), not the shader's own program
    std::string msl;
    std::vector<ProgramInput> inputs;
    std::vector<ProgramSampler> samplers;
};
Program* createProgram(Frame*, const ProgramDesc&);
// [CHOSEN, not recovered] Shader builds ahead of the first draw (ShaderPrebuild.inc). A pixel shader
// is drawn through variants of its program (Shaders.inc FragmentVariant) that depend on the draw's
// state. backgroundBuilds: programs created now are compiled ahead of their first draw.
bool backgroundBuilds(Frame*);
// One D3D9 occlusion query: the visibility-result slots its draws counted into (one per render
// pass it spanned). Draws carry it while the query is between Issue(BEGIN) and Issue(END).
struct Occlusion;
Occlusion* createOcclusion(Frame*);
void destroyOcclusion(Occlusion*);
void occlusionBegin(Occlusion*); // forgets earlier slots
// 1 with the summed sample count once every command buffer it counted in has completed, 0 while
// pending (flush submits the open command buffer), -1 after a command-buffer failure.
int occlusionResult(Frame*, Occlusion*, bool flush, std::uint64_t& samples);
void destroyProgram(Program*);
struct DrawStream { Buffer* buffer = nullptr; const void* bytes = nullptr; unsigned length = 0, offset = 0, stride = 0; };
struct DrawCall {
    Program* vertex = nullptr;
    Program* fragment = nullptr; // null only when color writes are disabled
    VertexLayout* layout = nullptr;
    StateMapping state = {};
    Texture* color = nullptr; unsigned colorLevel = 0, colorSlice = 0;
    Texture* depth = nullptr; unsigned depthLevel = 0, depthSlice = 0;
    float depthBiasUnits = 0; // D3D normalized depth bias per Metal depth-bias unit
    double viewport[6] = {};  // x, y, width, height, min z, max z
    unsigned scissor[4] = {}; // x, y, width, height; applied always (full target when disabled)
    DrawStream streams[16];
    Texture* textures[21] = {}; // StateMapping sampler numbering: pixel 0..15, displacement 16, vertex 17..20
    std::vector<std::uint8_t> vertexConstants, fragmentConstants;
    float vertexFixup[4] = {}, fragmentAlpha[4] = {}, fragmentFactor[4] = {}; // factor: D3DRS_TEXTUREFACTOR
    float fragmentFog[8] = {}; // fog colour; table mode, end, 1/(end-start), density (Shaders.inc)
    Occlusion* occlusion = nullptr; // the active occlusion query, if any
    unsigned primitive = 0; // D3DPRIMITIVETYPE 1..5; fans arrive as indexed triangle lists
    unsigned count = 0, first = 0;
    int baseVertex = 0;
    unsigned indexSize = 0; // 0 for non-indexed draws, otherwise 2 or 4
    Buffer* indexBuffer = nullptr; const void* indexBytes = nullptr; unsigned indexLength = 0, indexOffset = 0;
};
// Encodes one draw into the frame's current render pass. Returns false with a diagnostic for
// translation/pipeline failures; command failures surface at the next submission.
bool draw(Frame*, const DrawCall&, std::string& error);
}
#endif
