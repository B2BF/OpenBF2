// [CHOSEN, not recovered] D3DX9 effect state table for the native effect builder. State names
// and value keywords are the public Direct3D 9 render, texture-stage and sampler state names
// (D3DRS_*, D3DTSS_*, D3DSAMP_*) and enum values. IDs are the FX2 numbering that MojoShader's
// effect enums use (render states 0..102 in MOJOSHADER_renderStateType order, then
// texture-stage 103.., transforms/material/lights 121.., shaders 146/147, shader constants
// 148..163, Texture 164, sampler states 165..177, Sampler 178).
#if defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))
#include "FxStates.h"
#include "FxParser.h"
#include <cstring>

namespace bf2fx {
namespace {

struct Keyword { const char* table; const char* name; uint32_t value; };
const Keyword kKeywords[] = {
    {"bool","FALSE",0},{"bool","TRUE",1},
    {"zb","FALSE",0},{"zb","TRUE",1},{"zb","USEW",2},
    {"fill","POINT",1},{"fill","WIREFRAME",2},{"fill","SOLID",3},
    {"shade","FLAT",1},{"shade","GOURAUD",2},{"shade","PHONG",3},
    {"blend","ZERO",1},{"blend","ONE",2},{"blend","SRCCOLOR",3},{"blend","INVSRCCOLOR",4},{"blend","SRCALPHA",5},
    {"blend","INVSRCALPHA",6},{"blend","DESTALPHA",7},{"blend","INVDESTALPHA",8},{"blend","DESTCOLOR",9},
    {"blend","INVDESTCOLOR",10},{"blend","SRCALPHASAT",11},{"blend","BOTHSRCALPHA",12},{"blend","BOTHINVSRCALPHA",13},
    {"blend","BLENDFACTOR",14},{"blend","INVBLENDFACTOR",15},
    {"cull","NONE",1},{"cull","CW",2},{"cull","CCW",3},
    {"cmp","NEVER",1},{"cmp","LESS",2},{"cmp","EQUAL",3},{"cmp","LESSEQUAL",4},{"cmp","GREATER",5},
    {"cmp","NOTEQUAL",6},{"cmp","GREATEREQUAL",7},{"cmp","ALWAYS",8},
    {"fog","NONE",0},{"fog","EXP",1},{"fog","EXP2",2},{"fog","LINEAR",3},
    {"stencilop","KEEP",1},{"stencilop","ZERO",2},{"stencilop","REPLACE",3},{"stencilop","INCRSAT",4},
    {"stencilop","DECRSAT",5},{"stencilop","INVERT",6},{"stencilop","INCR",7},{"stencilop","DECR",8},
    {"mcs","MATERIAL",0},{"mcs","COLOR1",1},{"mcs","COLOR2",2},
    {"vblend","DISABLE",0},{"vblend","1WEIGHTS",1},{"vblend","2WEIGHTS",2},{"vblend","3WEIGHTS",3},
    {"vblend","TWEENING",255},{"vblend","0WEIGHTS",256},
    {"blendop","ADD",1},{"blendop","SUBTRACT",2},{"blendop","REVSUBTRACT",3},{"blendop","MIN",4},{"blendop","MAX",5},
    {"degree","LINEAR",1},{"degree","QUADRATIC",2},{"degree","CUBIC",3},{"degree","QUINTIC",5},
    {"patchedge","DISCRETE",0},{"patchedge","CONTINUOUS",1},
    {"wrap","U",1},{"wrap","V",2},{"wrap","W",4},{"wrap","COORD0",1},{"wrap","COORD1",2},{"wrap","COORD2",4},{"wrap","COORD3",8},
    {"cwe","RED",1},{"cwe","GREEN",2},{"cwe","BLUE",4},{"cwe","ALPHA",8},
    {"top","DISABLE",1},{"top","SELECTARG1",2},{"top","SELECTARG2",3},{"top","MODULATE",4},{"top","MODULATE2X",5},
    {"top","MODULATE4X",6},{"top","ADD",7},{"top","ADDSIGNED",8},{"top","ADDSIGNED2X",9},{"top","SUBTRACT",10},
    {"top","ADDSMOOTH",11},{"top","BLENDDIFFUSEALPHA",12},{"top","BLENDTEXTUREALPHA",13},{"top","BLENDFACTORALPHA",14},
    {"top","BLENDTEXTUREALPHAPM",15},{"top","BLENDCURRENTALPHA",16},{"top","PREMODULATE",17},
    {"top","MODULATEALPHA_ADDCOLOR",18},{"top","MODULATECOLOR_ADDALPHA",19},{"top","MODULATEINVALPHA_ADDCOLOR",20},
    {"top","MODULATEINVCOLOR_ADDALPHA",21},{"top","BUMPENVMAP",22},{"top","BUMPENVMAPLUMINANCE",23},
    {"top","DOTPRODUCT3",24},{"top","MULTIPLYADD",25},{"top","LERP",26},
    {"ta","DIFFUSE",0},{"ta","CURRENT",1},{"ta","TEXTURE",2},{"ta","TFACTOR",3},{"ta","SPECULAR",4},{"ta","TEMP",5},
    {"ta","CONSTANT",6},{"ta","COMPLEMENT",0x10},{"ta","ALPHAREPLICATE",0x20},
    {"ttf","DISABLE",0},{"ttf","COUNT1",1},{"ttf","COUNT2",2},{"ttf","COUNT3",3},{"ttf","COUNT4",4},{"ttf","PROJECTED",256},
    {"tci","PASSTHRU",0},{"tci","CAMERASPACENORMAL",0x10000},{"tci","CAMERASPACEPOSITION",0x20000},
    {"tci","CAMERASPACEREFLECTIONVECTOR",0x30000},{"tci","SPHEREMAP",0x40000},
    {"addr","WRAP",1},{"addr","MIRROR",2},{"addr","CLAMP",3},{"addr","BORDER",4},{"addr","MIRRORONCE",5},
    {"filter","NONE",0},{"filter","POINT",1},{"filter","LINEAR",2},{"filter","ANISOTROPIC",3},
    {"filter","PYRAMIDALQUAD",6},{"filter","GAUSSIANQUAD",7},
    {"light","POINT",1},{"light","SPOT",2},{"light","DIRECTIONAL",3},
};

using V = StateValue;
const StateInfo kRender[] = {
    {"ZEnable",0,V::Int,1,"zb"},{"FillMode",1,V::Int,1,"fill"},{"ShadeMode",2,V::Int,1,"shade"},
    {"ZWriteEnable",3,V::Int,1,"bool"},{"AlphaTestEnable",4,V::Int,1,"bool"},{"LastPixel",5,V::Int,1,"bool"},
    {"SrcBlend",6,V::Int,1,"blend"},{"DestBlend",7,V::Int,1,"blend"},{"CullMode",8,V::Int,1,"cull"},
    {"ZFunc",9,V::Int,1,"cmp"},{"AlphaRef",10,V::Int,1,nullptr},{"AlphaFunc",11,V::Int,1,"cmp"},
    {"DitherEnable",12,V::Int,1,"bool"},{"AlphaBlendEnable",13,V::Int,1,"bool"},{"FogEnable",14,V::Int,1,"bool"},
    {"SpecularEnable",15,V::Int,1,"bool"},{"FogColor",16,V::Int,1,nullptr},{"FogTableMode",17,V::Int,1,"fog"},
    {"FogStart",18,V::Float,1,nullptr},{"FogEnd",19,V::Float,1,nullptr},{"FogDensity",20,V::Float,1,nullptr},
    {"RangeFogEnable",21,V::Int,1,"bool"},{"StencilEnable",22,V::Int,1,"bool"},{"StencilFail",23,V::Int,1,"stencilop"},
    {"StencilZFail",24,V::Int,1,"stencilop"},{"StencilPass",25,V::Int,1,"stencilop"},{"StencilFunc",26,V::Int,1,"cmp"},
    {"StencilRef",27,V::Int,1,nullptr},{"StencilMask",28,V::Int,1,nullptr},{"StencilWriteMask",29,V::Int,1,nullptr},
    {"TextureFactor",30,V::Int,1,nullptr},
    {"Wrap0",31,V::Int,1,"wrap"},{"Wrap1",32,V::Int,1,"wrap"},{"Wrap2",33,V::Int,1,"wrap"},{"Wrap3",34,V::Int,1,"wrap"},
    {"Wrap4",35,V::Int,1,"wrap"},{"Wrap5",36,V::Int,1,"wrap"},{"Wrap6",37,V::Int,1,"wrap"},{"Wrap7",38,V::Int,1,"wrap"},
    {"Wrap8",39,V::Int,1,"wrap"},{"Wrap9",40,V::Int,1,"wrap"},{"Wrap10",41,V::Int,1,"wrap"},{"Wrap11",42,V::Int,1,"wrap"},
    {"Wrap12",43,V::Int,1,"wrap"},{"Wrap13",44,V::Int,1,"wrap"},{"Wrap14",45,V::Int,1,"wrap"},{"Wrap15",46,V::Int,1,"wrap"},
    {"Clipping",47,V::Int,1,"bool"},{"Lighting",48,V::Int,1,"bool"},{"Ambient",49,V::Int,1,nullptr},
    {"FogVertexMode",50,V::Int,1,"fog"},{"ColorVertex",51,V::Int,1,"bool"},{"LocalViewer",52,V::Int,1,"bool"},
    {"NormalizeNormals",53,V::Int,1,"bool"},{"DiffuseMaterialSource",54,V::Int,1,"mcs"},
    {"SpecularMaterialSource",55,V::Int,1,"mcs"},{"AmbientMaterialSource",56,V::Int,1,"mcs"},
    {"EmissiveMaterialSource",57,V::Int,1,"mcs"},{"VertexBlend",58,V::Int,1,"vblend"},
    {"ClipPlaneEnable",59,V::Int,1,nullptr},{"PointSize",60,V::Float,1,nullptr},{"PointSize_Min",61,V::Float,1,nullptr},
    {"PointSpriteEnable",62,V::Int,1,"bool"},{"PointScaleEnable",63,V::Int,1,"bool"},{"PointScale_A",64,V::Float,1,nullptr},
    {"PointScale_B",65,V::Float,1,nullptr},{"PointScale_C",66,V::Float,1,nullptr},
    {"MultiSampleAntialias",67,V::Int,1,"bool"},{"MultiSampleMask",68,V::Int,1,nullptr},
    {"PatchEdgeStyle",69,V::Int,1,"patchedge"},{"DebugMonitorToken",70,V::Int,1,nullptr},
    {"PointSize_Max",71,V::Float,1,nullptr},{"IndexedVertexBlendEnable",72,V::Int,1,"bool"},
    {"ColorWriteEnable",73,V::Int,1,"cwe"},{"TweenFactor",74,V::Float,1,nullptr},{"BlendOp",75,V::Int,1,"blendop"},
    {"PositionDegree",76,V::Int,1,"degree"},{"NormalDegree",77,V::Int,1,"degree"},
    {"ScissorTestEnable",78,V::Int,1,"bool"},{"SlopeScaleDepthBias",79,V::Float,1,nullptr},
    {"AntialiasedLineEnable",80,V::Int,1,"bool"},{"MinTessellationLevel",81,V::Float,1,nullptr},
    {"MaxTessellationLevel",82,V::Float,1,nullptr},{"AdaptiveTess_X",83,V::Float,1,nullptr},
    {"AdaptiveTess_Y",84,V::Float,1,nullptr},{"AdaptiveTess_Z",85,V::Float,1,nullptr},{"AdaptiveTess_W",86,V::Float,1,nullptr},
    {"EnableAdaptiveTessellation",87,V::Int,1,"bool"},{"TwoSidedStencilMode",88,V::Int,1,"bool"},
    {"CCW_StencilFail",89,V::Int,1,"stencilop"},{"CCW_StencilZFail",90,V::Int,1,"stencilop"},
    {"CCW_StencilPass",91,V::Int,1,"stencilop"},{"CCW_StencilFunc",92,V::Int,1,"cmp"},
    {"ColorWriteEnable1",93,V::Int,1,"cwe"},{"ColorWriteEnable2",94,V::Int,1,"cwe"},{"ColorWriteEnable3",95,V::Int,1,"cwe"},
    {"BlendFactor",96,V::Int,1,nullptr},{"SRGBWriteEnable",97,V::Int,1,"bool"},{"DepthBias",98,V::Float,1,nullptr},
    {"SeparateAlphaBlendEnable",99,V::Int,1,"bool"},{"SrcBlendAlpha",100,V::Int,1,"blend"},
    {"DestBlendAlpha",101,V::Int,1,"blend"},{"BlendOpAlpha",102,V::Int,1,"blendop"},
    {"ColorOp",103,V::Int,8,"top"},{"ColorArg0",104,V::Int,8,"ta"},{"ColorArg1",105,V::Int,8,"ta"},
    {"ColorArg2",106,V::Int,8,"ta"},{"AlphaOp",107,V::Int,8,"top"},{"AlphaArg0",108,V::Int,8,"ta"},
    {"AlphaArg1",109,V::Int,8,"ta"},{"AlphaArg2",110,V::Int,8,"ta"},{"ResultArg",111,V::Int,8,"ta"},
    {"BumpEnvMat00",112,V::Float,8,nullptr},{"BumpEnvMat01",113,V::Float,8,nullptr},{"BumpEnvMat10",114,V::Float,8,nullptr},
    {"BumpEnvMat11",115,V::Float,8,nullptr},{"TexCoordIndex",116,V::Int,8,"tci"},{"BumpEnvLScale",117,V::Float,8,nullptr},
    {"BumpEnvLOffset",118,V::Float,8,nullptr},{"TextureTransformFlags",119,V::Int,8,"ttf"},{"Constant",120,V::Int,8,nullptr},
    {"PatchSegments",121,V::Float,1,nullptr},{"FVF",122,V::Int,1,nullptr},
    {"ProjectionTransform",123,V::Float,1,nullptr},{"ViewTransform",124,V::Float,1,nullptr},
    {"WorldTransform",125,V::Float,256,nullptr},{"TextureTransform",126,V::Float,8,nullptr},
    {"MaterialDiffuse",127,V::Float,1,nullptr},{"MaterialAmbient",128,V::Float,1,nullptr},
    {"MaterialSpecular",129,V::Float,1,nullptr},{"MaterialEmissive",130,V::Float,1,nullptr},
    {"MaterialPower",131,V::Float,1,nullptr},{"LightType",132,V::Int,8,"light"},{"LightDiffuse",133,V::Float,8,nullptr},
    {"LightSpecular",134,V::Float,8,nullptr},{"LightAmbient",135,V::Float,8,nullptr},{"LightPosition",136,V::Float,8,nullptr},
    {"LightDirection",137,V::Float,8,nullptr},{"LightRange",138,V::Float,8,nullptr},{"LightFalloff",139,V::Float,8,nullptr},
    {"LightAttenuation0",140,V::Float,8,nullptr},{"LightAttenuation1",141,V::Float,8,nullptr},
    {"LightAttenuation2",142,V::Float,8,nullptr},{"LightTheta",143,V::Float,8,nullptr},{"LightPhi",144,V::Float,8,nullptr},
    {"LightEnable",145,V::Int,8,"bool"},
    {"VertexShader",146,V::VertexShader,1,nullptr},{"PixelShader",147,V::PixelShader,1,nullptr},
    {"VertexShaderConstantF",148,V::Float,256,nullptr},{"VertexShaderConstantB",149,V::Bool,256,nullptr},
    {"VertexShaderConstantI",150,V::Int,256,nullptr},{"VertexShaderConstant",151,V::Float,256,nullptr},
    {"VertexShaderConstant1",152,V::Float,256,nullptr},{"VertexShaderConstant2",153,V::Float,256,nullptr},
    {"VertexShaderConstant3",154,V::Float,256,nullptr},{"VertexShaderConstant4",155,V::Float,256,nullptr},
    {"PixelShaderConstantF",156,V::Float,256,nullptr},{"PixelShaderConstantB",157,V::Bool,256,nullptr},
    {"PixelShaderConstantI",158,V::Int,256,nullptr},{"PixelShaderConstant",159,V::Float,256,nullptr},
    {"PixelShaderConstant1",160,V::Float,256,nullptr},{"PixelShaderConstant2",161,V::Float,256,nullptr},
    {"PixelShaderConstant3",162,V::Float,256,nullptr},{"PixelShaderConstant4",163,V::Float,256,nullptr},
    {"Texture",164,V::Texture,261,nullptr},
    {"AddressU",165,V::Int,261,"addr"},{"AddressV",166,V::Int,261,"addr"},{"AddressW",167,V::Int,261,"addr"},
    {"BorderColor",168,V::Int,261,nullptr},{"MagFilter",169,V::Int,261,"filter"},{"MinFilter",170,V::Int,261,"filter"},
    {"MipFilter",171,V::Int,261,"filter"},{"MipMapLodBias",172,V::Float,261,nullptr},{"MaxMipLevel",173,V::Int,261,nullptr},
    {"MaxAnisotropy",174,V::Int,261,nullptr},{"SRGBTexture",175,V::Int,261,"bool"},{"ElementIndex",176,V::Int,261,nullptr},
    {"DMapOffset",177,V::Int,261,nullptr},{"Sampler",178,V::Sampler,261,nullptr},
};
// sampler_state members: the sampler-state IDs that MojoShader reports (FX2 ID - 160).
const StateInfo kSampler[] = {
    {"Texture",4,V::Texture,1,nullptr},{"AddressU",5,V::Int,1,"addr"},{"AddressV",6,V::Int,1,"addr"},
    {"AddressW",7,V::Int,1,"addr"},{"BorderColor",8,V::Int,1,nullptr},{"MagFilter",9,V::Int,1,"filter"},
    {"MinFilter",10,V::Int,1,"filter"},{"MipFilter",11,V::Int,1,"filter"},{"MipMapLodBias",12,V::Float,1,nullptr},
    {"MaxMipLevel",13,V::Int,1,nullptr},{"MaxAnisotropy",14,V::Int,1,nullptr},{"SRGBTexture",15,V::Int,1,"bool"},
    {"ElementIndex",16,V::Int,1,nullptr},{"DMapOffset",17,V::Int,1,nullptr},
};
} // namespace

const StateInfo* passState(const std::string& name) {
    for(const auto& s:kRender) if(src::iequals(name,s.name)) return &s;
    return nullptr;
}
const StateInfo* samplerState(const std::string& name) {
    for(const auto& s:kSampler) if(src::iequals(name,s.name)) return &s;
    return nullptr;
}
bool stateKeyword(const StateInfo& state,const std::string& word,uint32_t& value) {
    const char* table=state.keywords;
    for(int pass=0;pass<2;++pass) {
        if(table) for(const auto& k:kKeywords) if(!std::strcmp(k.table,table) && src::iequals(word,k.name)) {value=k.value;return true;}
        table="bool"; // TRUE/FALSE are accepted for every state.
    }
    return false;
}
} // namespace bf2fx
#endif
