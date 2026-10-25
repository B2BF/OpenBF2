// The road and spline files of the retail editor (the LevelEditor plugin's road handler and EditorEngine's spline
// manager), engine-free: Editor/Splines.con, Editor/Roads.con and a road template's editor script written as the
// retail writers wrote them (lines ending LF, numbers in std::ostream's default formatting), their console rows split
// back into words, CompiledRoads.con read and rewritten with the level's other compiled roads kept, and the chunk table
// the 1.50 road compile leaves out of a compiled mesh appended. Editor-only code (D3).
#ifndef BF2EDITOR_FRAMEWORK_ROADFILES_H
#define BF2EDITOR_FRAMEWORK_ROADFILES_H

#include "Framework/EditorConfig.h"

#include <string>
#include <vector>

namespace bf2editor
{

// One control point of a spline as Splines.con records it. The trigger properties are written only when they differ
// from the control point's defaults (speed 60, delay 0, direction 0, no sound).
struct SplinePointRecord
{
    float       position[3] = { 0.0f, 0.0f, 0.0f };
    float       speed = 60.0f;
    float       delay = 0.0f;
    int         direction = 0;
    std::string sound;
};

// A spline: its id (the key roads name it by), the terrain snap bits (1 added points, 2 moved points, 4 curve samples,
// 8 ribbon edges), the drawn ribbon's width, the sampling step and the points.
struct SplineRecord
{
    int                            id = -1;
    int                            snapModes = 0;
    float                          lineWidth = 0.05f;
    float                          stepSize = 3.0f;
    std::vector<SplinePointRecord> points;
};

// A road: its name (the compiled mesh's name), the spline and template it is built from, the edge noise's seed and
// settings, the first texture layer's scale offset, the draw priority and the four edge fades (distance pairs).
struct RoadRecord
{
    std::string name;
    int         splineId = -1;
    std::string templateName;
    int         seed = 0;
    float       textureScaleOffset[2] = { 0.0f, 0.0f };
    float       persistence = 0.2f;
    float       range[2] = { -0.15f, 0.15f };
    int         octaves = 3;
    int         priority = 0;
    float       fadeStartLeft[2] = { 0.0f, 3.0f };
    float       fadeStartRight[2] = { 0.0f, 3.0f };
    float       fadeEndLeft[2] = { 0.0f, 3.0f };
    float       fadeEndRight[2] = { 0.0f, 3.0f };
};

// A road template (a road surface): width in metres (the retail script stores it as a whole number), the tessellation's
// error limit, the blend factor, the draw priority, the material by name (empty: none written), the two texture layers
// (0 the primary) with their mirror flags and scales, and the fade-off distance pair.
struct RoadTemplateRecord
{
    std::string name;
    float       width = 1.0f;
    float       maxTesselatedError = 0.25f;
    float       blendFactor = 1.0f;
    int         priority = 0;
    std::string material;
    std::string textures[2];
    bool        mirror[2] = { false, false };
    float       scale[2][2] = { { 1.0f, 1.0f }, { 1.0f, 1.0f } };
    float       fadeOffDistance[2] = { 200.0f, 300.0f };
};

// A number as the retail writers streamed it (std::ostream's default: six significant digits, %g).
std::string formatConsoleFloat(float value);

// Editor/Splines.con: every spline with at least one point, in the order given (the retail writer's order is by id).
// Empty when none has a point - the retail writer then left the file empty.
std::string splinesConText(const std::vector<SplineRecord>& splines);
// Editor/Roads.con, the roads in the order given (the retail writer's order is by spline id). Empty without roads.
std::string roadsConText(const std::vector<RoadRecord>& roads);
// A road template's editor script (<objects>/Roads/Splines/<name>.con): the `if v_arg1 == BF2Editor` branch the retail
// writer wrote. [added] A RoadTemplate.SetFadeOffDistance row when the pair differs from the default; retail never
// wrote one, so a template's fade-off reverted to the default on its next compile.
std::string roadTemplateConText(const RoadTemplateRecord& roadTemplate);

// One console row: the object and method ("Road", "SetRange") and the arguments, a double-quoted argument taken whole
// without its quotes. False for a line that is not a row (blank, a comment, no dot in the first word).
struct ConsoleRow
{
    std::string              object;
    std::string              method;
    std::vector<std::string> args;
};
bool splitConsoleRow(const std::string& line, ConsoleRow& row);
// The rows of a script, or only those inside its `if v_arg1 == <argument>` blocks when `argument` is not empty (the
// blocks' own nested conditions are refused: they are counted in `skipped`).
std::vector<ConsoleRow> consoleRows(const std::string& text, const std::string& argument, int& skipped);
// "a/b" and "a/b/c" as the console's vector types read them.
bool parseVec2(const std::string& text, float out[2]);
bool parseVec3(const std::string& text, float out[3]);

// CompiledRoads.con: the lines before the first `object.create`, then one block per compiled road - its object.*
// lines, the mesh it loads taken out - each written back followed by a blank line, in the file's line ending.
struct CompiledRoadBlock
{
    std::vector<std::string> lines;
    std::string              mesh;          // the object.geometry.loadMesh argument, empty without one
};
struct CompiledRoadsFile
{
    std::vector<std::string>       preamble;
    std::vector<CompiledRoadBlock> blocks;
    std::string                    newline = "\r\n";
};
CompiledRoadsFile parseCompiledRoads(const std::string& text);
std::string compiledRoadsText(const CompiledRoadsFile& file);
// Whether a block's mesh is road `name`'s compiled mesh (".../Roads/<name>_compiled.mesh", either slash, any case).
bool compiledMeshIsRoad(const std::string& mesh, const std::string& name);

// What a compiled road mesh holds.
struct RoadMeshInfo
{
    int      versionMajor = 0;
    int      versionMinor = 0;
    unsigned vertices = 0;
    unsigned indices = 0;
    unsigned chunks = 0;            // in its chunk table; 0 without one
    bool     hasChunkTable = false;
};
// Reads a compiled road mesh (any version the 1.50 reader takes). False with `error` when it is cut short.
bool readRoadMeshInfo(const std::string& data, RoadMeshInfo& info, std::string& error);
// Appends the chunk table a 1.4 mesh needs and the 1.50 compile does not write: u32 count, then per chunk u32 first
// index, u32 index count, f32 radius and the world-space centre of the chunk's bounding sphere (the centroid of the
// vertices it uses, as D3DXComputeBoundingSphere computes one, and the farthest of them). [CHOSEN, not recovered] the
// split: triangles in the compile's order, a new chunk when the next triangle would take the chunk beyond 1024
// triangles or its vertices beyond a 32 m horizontal half-diagonal (the stock chunks' spheres are 10-50 m; the 2006
// editor's tessellator split its own pieces, a rule not carried over). False with `error` for a mesh that is not 1.4,
// is cut short or already has a table.
bool appendRoadMeshChunkTable(std::string& data, RoadMeshInfo& info, std::string& error);

// Whether a road's projection into the terrain came back as one flat level: the spline's inner control points (the ones
// the road is built between) rise by more than half a metre, yet every height the projection changed landed within a
// centimetre of the others. A projection drawn with its colour writes masked does this - every texel decodes to the
// footprint's lowest point. `heights` are the changed samples' new heights in metres; fewer than eight never count.
bool roadProjectionCameBackFlat(const SplineRecord& spline, const std::vector<float>& heights);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_ROADFILES_H
