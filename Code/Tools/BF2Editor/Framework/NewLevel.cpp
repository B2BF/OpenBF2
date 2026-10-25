// Code/Tools/BF2Editor/Framework/NewLevel.cpp - see NewLevel.h. Editor-only code.
#include "Framework/EditorConfig.h"
#include "Framework/NewLevel.h"

#include "Framework/DdsImage.h"
#include "Framework/LevelCopier.h"
#include "Framework/TerrainRawCompile.h"
#include "Framework/TextureEncode.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <set>

namespace bf2editor
{

namespace
{

// The vertical scales the retail dialog gives the new heightmaps (primary 16-bit, surrounding 8-bit).
const float kPrimaryScaleY = 0.0025f;
const float kSecondaryScaleY = 0.64f;
const int kPatchCells = 128;

std::string lines(const std::vector<std::string>& text, const char* eol)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i)
        out += text[i] + eol;
    return out;
}

std::shared_ptr<const std::string> shared(const std::string& bytes)
{
    return std::make_shared<const std::string>(bytes);
}

// HeightmapEditable::createHeightData's conversion of a height in metres to the map's units.
unsigned rawHeight(int metres, float scaleY)
{
    return static_cast<unsigned>(static_cast<int>(static_cast<float>(metres) / scaleY));
}

// HeightmapCluster::getSectorOfWorldCoords for one axis: sector 0 is [-half, half).
int clusterSector(float v, float size)
{
    const float half = 0.5f * size;
    int sector = 0;
    if (v >= 0.0f)
    {
        for (float t = v; t >= half; t -= size)
            ++sector;
    }
    else if (-half > v)
    {
        for (float t = v; -half > t; t += size)
            --sector;
    }
    return sector;
}

// An uncompressed A8R8G8B8 texture with one level in the header the retail editor's saves write (no pitch, no mip
// count, the alpha cap), every texel `value`.
std::string flatTexture(int width, int height, unsigned value)
{
    unsigned char header[128];
    std::memset(header, 0, sizeof(header));
    std::memcpy(header, "DDS ", 4);
    const auto put = [&header](int offset, unsigned v) {
        header[offset] = static_cast<unsigned char>(v);
        header[offset + 1] = static_cast<unsigned char>(v >> 8);
        header[offset + 2] = static_cast<unsigned char>(v >> 16);
        header[offset + 3] = static_cast<unsigned char>(v >> 24);
    };
    put(4, 124);
    put(8, 0x1007u);                                // caps, height, width, pixel format
    put(12, static_cast<unsigned>(height));
    put(16, static_cast<unsigned>(width));
    put(76, 32);
    put(80, 0x41u);                                 // RGB with alpha
    put(88, 32);
    put(92, 0x00FF0000u);
    put(96, 0x0000FF00u);
    put(100, 0x000000FFu);
    put(104, 0xFF000000u);
    put(108, 0x1002u);                              // a texture with alpha
    std::string out(reinterpret_cast<const char*>(header), sizeof(header));
    const char texel[4] = { static_cast<char>(value), static_cast<char>(value >> 8), static_cast<char>(value >> 16),
                            static_cast<char>(value >> 24) };
    out.reserve(out.size() + static_cast<size_t>(width) * height * 4);
    for (int i = 0; i < width * height; ++i)
        out.append(texel, 4);
    return out;
}

std::string heightdataScript(const NewLevelSettings& s)
{
    const std::string scale = std::to_string(s.scale) + "/0.0025/" + std::to_string(s.scale);
    const std::string low = std::to_string(s.lowResScale()) + "/0.64/" + std::to_string(s.lowResScale());
    const std::string side = std::to_string(s.size + 1), lowSide = std::to_string(s.lowResSize + 1);
    const std::string folder = "Levels/" + s.name + "/";
    std::vector<std::string> t = {
        "heightmapcluster.create HeighmapCluster",
        "heightmapcluster.setClusterSize 3",
        "heightmapcluster.setHeightmapSize " + std::to_string(s.worldSize()),
        "",
        "rem --- primary ---",
        "heightmapcluster.addHeightmap Heightmap 0 0",
        "heightmap.setSize " + side + " " + side,
        "heightmap.setScale " + scale,
        "heightmap.setBitResolution 16",
        "heightmap.setModified 1",
        "heightmap.loadHeightData " + folder + "HeightmapPrimary.raw",
        "heightmap.setMaterialScale 1",
        "heightmap.loadMaterialData " + folder + "HeightmapPrimary.mat",
        "",
        "rem --- secondary ---",
    };
    static const struct { int x, z; const char* suffix; } kSectors[] = {
        { -1, -1, "L1U1" }, { 0, -1, "U1" }, { 1, -1, "R1U1" }, { -1, 0, "L1" }, { 1, 0, "R1" },
        { -1, 1, "L1D1" }, { 0, 1, "D1" }, { 1, 1, "R1D1" } };
    for (size_t i = 0; i < sizeof(kSectors) / sizeof(kSectors[0]); ++i)
    {
        if (i == 3 || i == 5)
            t.push_back("");
        t.push_back("heightmapcluster.addHeightmap Heightmap " + std::to_string(kSectors[i].x) + " " +
                    std::to_string(kSectors[i].z));
        t.push_back("heightmap.setSize " + lowSide + " " + lowSide);
        t.push_back("heightmap.setScale " + low);
        t.push_back("heightmap.setBitResolution 8");
        t.push_back("heightmap.setModified 0");
        t.push_back("heightmap.loadHeightData " + folder + "HeightmapSecondary_" + kSectors[i].suffix + ".raw");
    }
    t.push_back("");
    t.push_back("heightmapcluster.setSeaWaterLevel -10000");
    return lines(t, "\r\n");
}

std::string terrainScript(const NewLevelSettings& s)
{
    const std::string folder = "Levels/" + s.name + "/";
    const std::vector<std::string> t = {
        "if v_arg1 == BF2Editor",
        "terrain.create TerrainEditable",
        "terrain.patchSize 128",
        "terrain.subdividePatches 1",
        "terrain.primaryWorldScale " + std::to_string(s.scale) + "/0.0025/" + std::to_string(s.scale),
        "terrain.secondaryWorldScale " + std::to_string(s.lowResScale()) + "/0.64/" + std::to_string(s.lowResScale()),
        "terrain.patchColormapSize 512",
        "terrain.lowDetailmapSize 512",
        "terrain.detailmapBaseName \"" + folder + "Detailmaps/tx\"",
        "terrain.lowDetailmapBaseName \"" + folder + "LowDetailmaps/tx\"",
        "terrain.colormapBaseName \"" + folder + "Colormaps/tx\"",
        "terrain.lightmapBaseName \"" + folder + "Lightmaps/tx\"",
        "terrain.farSideTiling 2/2",
        "terrain.farTopTilingHi 16",
        "terrain.farTopTilingLow 32",
        "terrain.farYOffset 0",
        "terrain.terrainWaterColor 0/0/0",
        "terrain.init",
        "",
        "else",
        "terrain.create Terrain",
        "terrain.load " + folder + "terraindata.raw",
        "endIf",
        "",
    };
    return lines(t, "\r\n");
}

std::string initScript(const NewLevelSettings& s)
{
    const std::string undergrowth = "Undergrowth.load Levels\\" + s.name + "\\";
    const std::string rule = "rem -----------------------------------------------------------------------------";
    std::vector<std::string> t = {
        "rem *** Generated by BF2Editor ***",
        "if v_arg1 == BF2Editor",
        "",
        "run Heightdata.con",
        "LevelSettings.InitWorld",
        "run Terrain.con BF2Editor",
        "run StaticObjects.con BF2Editor",
        "run Sounds.con",
        "run Sky.con BF2Editor",
        "run Editor/GamePlayObjects.con host",
        "UndergrowthEditable.create",
        undergrowth,
        "run Overgrowth/Overgrowth.con",
        "Overgrowth.editorEnable 1",
        "run AmbientObjects.con BF2Editor",
        "run Water.con",
        "run TriggerableTemplates.con BF2Editor",
        "",
        "else",
        "",
        "run Heightdata.con",
        "run Terrain.con v_arg2",
        "run Sky.con v_arg2",
        "run CompiledRoads.con",
        "run Sounds.con",
        "run tmp.con v_arg1",
        undergrowth,
        "run Overgrowth/Overgrowth.con",
        "run Overgrowth/OvergrowthCollision.con",
        "run AmbientObjects.con",
        "run Water.con",
        "run TriggerableTemplates.con",
        "",
        "endIf",
        "",
        "rem ------------------------------- LevelSettings -------------------------------",
        rule,
        "",
        "gameLogic.setTeamName 1 \"MEC\"",
        "gameLogic.setTeamName 2 \"USMC\"",
        "",
        "gameLogic.setTeamLanguage 1 \"English\"",
        "gameLogic.setTeamLanguage 2 \"English\"",
        "",
        "gameLogic.setTeamFlag 0 \"flag_neutral\"",
        "gameLogic.setTeamFlag 1 \"flag_ch\"",
        "gameLogic.setTeamFlag 2 \"flag_us\"",
        "",
    };
    for (int kit = 0; kit < 7; ++kit)
    {
        t.push_back("gameLogic.setKit 1 " + std::to_string(kit) + " \"\" \"\"");
        t.push_back("gameLogic.setKit 2 " + std::to_string(kit) + " \"\" \"\"");
        t.push_back("");
    }
    const std::vector<std::string> tail = {
        rule,
        "",
        "gameLogic.setBeforeSpawnCamera 0/150/0 0/0/0",
        "",
        "if v_arg1 == BF2Editor",
        "LevelSettings.CustomTextureSuffix \"\"",
        "else",
        "texturemanager.customTextureSuffix \"\"",
        "endIf",
        rule,
        "",
        "GameLogic.MaximumLevelViewDistance 850",
        "",
        rule,
        "",
        "gameLogic.setDefaultNumberOfTicketsEx 16 1 50",
        "gameLogic.setDefaultNumberOfTicketsEx 16 2 50",
        "gameLogic.setDefaultNumberOfTicketsEx 32 1 50",
        "gameLogic.setDefaultNumberOfTicketsEx 32 2 50",
        "gameLogic.setDefaultNumberOfTicketsEx 64 1 50",
        "gameLogic.setDefaultNumberOfTicketsEx 64 2 50",
        "gameLogic.setDefaultNumberOfTicketsEx 128 1 50",
        "gameLogic.setDefaultNumberOfTicketsEx 128 2 50",
        "gameLogic.setDefaultTimeToNextAIWave 8",
        "gameLogic.setTicketLossAtEndPerMin 1000",
        "gameLogic.setTicketLossPerMin 1 10",
        "gameLogic.setTicketLossPerMin 2 10",
        "",
        rule,
        "",
        rule,
        "",
        "renderer.globalStaticMeshLodDistanceScale 1",
        "renderer.globalBundleMeshLodDistanceScale 1",
        "renderer.globalSkinnedMeshLodDistanceScale 1",
    };
    t.insert(t.end(), tail.begin(), tail.end());
    return lines(t, "\r\n");
}

// The sky, light and fog defaults the retail sky settings write for a new level. [CHOSEN] The base hemi map covers
// the level's world size (the retail default level's 2048 m, the only size the golden run shows).
std::string skyScript(const NewLevelSettings& s)
{
    char hemi[256];
    std::snprintf(hemi, sizeof(hemi), "hemiMapManager.setBaseHemiMap Levels/%s/groundhemi 0.000000/0.000000/0.000000 %f "
                  "500.000000", s.name.c_str(), static_cast<double>(s.worldSize()));
    const std::vector<std::string> t = {
        "Lightmanager.init",
        "Lightmanager.skycolor 0.5/0.52/0.55",
        "Lightmanager.ambientcolor 0.509804/0.517647/0.556863",
        "Lightmanager.sunColor 0.7/0.67/0.63",
        "Lightmanager.sunSpecColor 1/1/1",
        "if v_arg1 == BF2Editor",
        "LightSettings.TerrainSunColor 0.97/0.81/0.57",
        "LightSettings.TerrainSkyColor 0.63/0.70/0.75",
        "LightmapSettings.waterSunIntensity 0",
        "else",
        "terrain.sunColor 0.97/0.81/0.57",
        "terrain.GIColor 0.63/0.70/0.75",
        "terrain.waterSunIntensity 0",
        "endIf",
        "Lightmanager.staticSunColor 0.67/0.64/0.55",
        "Lightmanager.staticSpecularColor 1.00/1.00/1.00",
        "Lightmanager.staticSkyColor 0.63/0.70/0.75",
        "Lightmanager.singlePointColor 0.00/0.00/0.00",
        "Lightmanager.sunDirection -0.501456/-0.611777/-0.611777",
        "Lightmanager.enableSun 1",
        "Lightmanager.effectSunColor 1.00/1.00/1.00",
        "Lightmanager.effectShadowColor 1.00/1.00/1.00",
        "Lightmanager.defaultEffectLightAffectionFactor 1",
        "Lightmanager.hemilerpbias 1",
        hemi,
        "Lightmanager.treeAmbientColor 0.00/0.00/0.00",
        "Lightmanager.treeSunColor 1.00/1.00/1.00",
        "Lightmanager.treeSkyColor 1.00/1.00/1.00",
        "",
        "if v_arg1 == BF2Editor",
        "else",
        "run /Common/Sky/SkyDome/skydome.con",
        "run /Common/Sky/Flare/flaresprite.con",
        "endIf",
        "",
        "Skydome.skyTemplate skydome",
        "Skydome.cloudTemplate cloudlayer",
        "Skydome.hasCloudLayer 0",
        "Skydome.hasCloudLayer2 0",
        "Skydome.scrolldirection 0/0",
        "Skydome.scrolldirection2 0/0",
        "Skydome.cloudTexture ",
        "Skydome.cloudTexture2 ",
        "Skydome.skyTexture ",
        "Skydome.domeRotation 0",
        "Skydome.fadeCloudsDistances 1000/1100",
        "Skydome.cloudLerpFactors 0.5/0.5",
        "Skydome.flareTexture ",
        "Skydome.flareDirection 0.00/0.00/0.00",
        "",
        "Renderer.fogColor 128.00/128.00/128.00",
        "Renderer.fogStartEndAndBase 10.00/400.00/1.00/0.50",
        "",
    };
    return lines(t, "\r\n");
}

std::string waterScript()
{
    const std::vector<std::string> t = {
        "rem *** Generated by BF2Editor ***",
        "renderer.waterScroll 0/0",
        "renderer.waterAnimSpeed 0",
        "renderer.waterColor 0/0/0",
        "renderer.waterSpecularColor 0/0/0/0",
        "renderer.waterSpecularPower 0",
        "renderer.waterFogColor 51.50/51.50/128.00",
        "renderer.waterFogStartEndAndBase 10.00/250.00/1.00/0.50",
        "terrain.waterSunIntensity 0.8",
        "",
    };
    return lines(t, "\r\n");
}

std::string terrainLightScript()
{
    std::vector<std::string> t = { "rem ******** Lightmap Generation Settings ********", "if v_arg1 == BF2Editor", "" };
    const std::vector<std::string> settings = terrainLightDefaults();
    t.insert(t.end(), settings.begin(), settings.end());
    t.push_back("");
    t.push_back("endIf");
    return lines(t, "\r\n");
}

// The map description: the game modes and sizes the retail editor lists for a new level. The briefing and the mode
// texts are our own placeholders.
std::string mapDescription(const NewLevelSettings& s)
{
    std::vector<std::string> t = {
        "<map>",
        "\t<name> " + s.name + " </name>",
        "\t<briefing locid=\"LOADINGSCREEN_MAPDESCRIPTION_" + s.name + "\">A new level made with BF2Editor.</briefing>",
        "\t<music> common/sound/menu/music/load_MEC_music.ogg </music>",
        "\t<modes>",
    };
    static const char* const kModes[] = { "gpm_cq", "gpm_sl", "gpm_coop", "sp1", "sp2", "sp3" };
    for (size_t i = 0; i < sizeof(kModes) / sizeof(kModes[0]); ++i)
    {
        t.push_back(std::string("\t\t<mode type=\"") + kModes[i] + "\">");
        t.push_back("\t\t\t<maptype ai=\"1\" players=\"16\" type=\"doubleassault\" "
                    "locid=\"GAMEMODE_DESCRIPTION_doubleassault\">Game mode description.</maptype>");
        t.push_back("\t\t\t<maptype players=\"32\" type=\"headon\" locid=\"GAMEMODE_DESCRIPTION_headon\">Game mode "
                    "description.</maptype>");
        t.push_back("\t\t\t<maptype players=\"64\" type=\"headon\" locid=\"GAMEMODE_DESCRIPTION_headon\">Game mode "
                    "description.</maptype>");
        t.push_back("\t\t</mode>");
    }
    t.push_back("\t</modes>");
    t.push_back("</map>");
    t.push_back("");
    return lines(t, "\n");
}

} // namespace

std::vector<std::string> terrainLightDefaults()
{
    return {
        "LightMapGeneration.init",
        "LightMapGeneration.GILightsBaseDir 0/0.33/-0.66",
        "LightMapGeneration.GILightsIntensity 0.33",
        "LightMapGeneration.GILightsShadowIntensity 1",
        "LightMapGeneration.GILightsFov 45",
        "LightMapGeneration.UseGITopLight 1",
        "LightMapGeneration.GITopLightIntensity 0.1",
        "LightMapGeneration.GILightsViewDistance 10",
        "LightMapGeneration.skyWhite 0.3",
        "LightMapGeneration.numberOfGILights 8",
        "LightMapGeneration.sunLightShadowIntensity 1",
        "LightMapGeneration.sunLightFov 2",
        "LightMapGeneration.sunLightIntensity 1",
    };
}

bool checkNewLevelSettings(const NewLevelSettings& s, std::string& error)
{
    if (!validLevelName(s.name))
    {
        error = "'" + s.name + "' is not a level name (letters, digits and underscores)";
        return false;
    }
    if (s.size != 256 && s.size != 512 && s.size != 1024)
    {
        error = "the size is 256, 512 or 1024";
        return false;
    }
    if (s.scale != 1 && s.scale != 2 && s.scale != 4)
    {
        error = "the scale is 1, 2 or 4";
        return false;
    }
    if (s.defaultHeight < 0 || s.defaultHeight > 100)
    {
        error = "the default height is 0..100";
        return false;
    }
    if (s.lowResSize != 128 && s.lowResSize != 256 && s.lowResSize != 512 && s.lowResSize != 1024)
    {
        error = "the low-res size is 128, 256, 512 or 1024";
        return false;
    }
    if (s.lowResScale() < 1)
    {
        error = "the low-res scale (scale x size / low-res size) may not be 0";
        return false;
    }
    return true;
}

bool buildNewLevel(const NewLevelSettings& s, const NewLevelContent& content, const TerrainRawFile& lodSource,
                   std::vector<NewLevelFile>& files, std::string& error)
{
    files.clear();
    if (!checkNewLevelSettings(s, error))
        return false;
    if (lodSource.patchSize != static_cast<unsigned>(kPatchCells) || lodSource.compactVertices
        || lodSource.lodWords.size() != 21u * 38u + 64u * 6u || lodSource.lodIndices.empty())
    {
        error = "the terraindata.raw lending the terrain LOD block does not have the 128-cell patch grid";
        return false;
    }
    const auto add = [&files](const std::string& path, const std::shared_ptr<const std::string>& bytes) {
        NewLevelFile f;
        f.path = path;
        f.bytes = bytes;
        files.push_back(f);
    };

    add("Heightdata.con", shared(heightdataScript(s)));
    add("Terrain.con", shared(terrainScript(s)));
    add("Init.con", shared(initScript(s)));
    add("Sky.con", shared(skyScript(s)));
    add("Water.con", shared(waterScript()));
    add("Editor/TerrainLight.con", shared(terrainLightScript()));
    add("Info/" + s.name + ".desc", shared(mapDescription(s)));
    add("Lightmaps/Objects/LightmapAtlas.tai",
        shared("# texture file, atlas file, atlas index, u offset, v offset, width, height\r\n"));
    add("GameModes/gpm_cq/16/GamePlayObjects.con", shared("rem *** Generated by BF2Editor ***\r\n"));

    // The heightmaps, flat at the default height: the primary 16-bit with its material grid (material 0), the eight
    // surrounding maps 8-bit.
    const unsigned primaryRaw = rawHeight(s.defaultHeight, kPrimaryScaleY);
    const unsigned secondaryRaw = rawHeight(s.defaultHeight, kSecondaryScaleY);
    const size_t primaryCells = static_cast<size_t>(s.size + 1) * (s.size + 1);
    std::string primary(primaryCells * 2, '\0');
    for (size_t i = 0; i < primaryCells; ++i)
    {
        primary[2 * i] = static_cast<char>(primaryRaw & 0xFFu);
        primary[2 * i + 1] = static_cast<char>((primaryRaw >> 8) & 0xFFu);
    }
    add("HeightmapPrimary.raw", shared(primary));
    add("HeightmapPrimary.mat", shared(std::string(primaryCells, '\0')));
    const auto secondary = shared(std::string(static_cast<size_t>(s.lowResSize + 1) * (s.lowResSize + 1),
                                              static_cast<char>(secondaryRaw & 0xFFu)));
    static const char* const kSuffixes[] = { "D1", "L1", "L1D1", "L1U1", "R1", "R1D1", "R1U1", "U1" };
    for (size_t i = 0; i < sizeof(kSuffixes) / sizeof(kSuffixes[0]); ++i)
        add(std::string("HeightmapSecondary_") + kSuffixes[i] + ".raw", secondary);

    // The compiled terrain: the header of a new terrain, the LOD block of the lending file, the patches compiled from
    // the cluster the heightmaps above make (a point on the primary map's far edge, or past any edge, belongs to the
    // surrounding map there, as HeightmapCluster places it). No layer has weight and no surrounding map is modified, so
    // no layer record and no ring sector is written.
    const int count = s.patchCount();
    TerrainRawFile templ;
    templ.version = 0x1A;
    templ.hasVersion = 1;
    templ.primaryScale[0] = templ.primaryScale[2] = static_cast<float>(s.scale);
    templ.primaryScale[1] = kPrimaryScaleY;
    templ.secondaryScale[0] = templ.secondaryScale[2] = static_cast<float>(s.lowResScale());
    templ.secondaryScale[1] = kSecondaryScaleY;
    templ.word2C = 0xCDCDCDCDu;                     // what the retail writer leaves in it: uninitialised heap
    templ.patchSize = kPatchCells;
    templ.subdividePatches = 1;
    templ.patchCount = static_cast<unsigned>(count);
    templ.detailmapSize = 512;
    templ.colormapSize = 512;
    const std::string folder = "Levels/" + s.name + "/";
    templ.colormapBaseName = folder + "Colormaps/tx";
    templ.detailmapBaseName = folder + "Detailmaps/tx";
    templ.lowDetailmapBaseName = folder + "LowDetailmaps/tx";
    templ.lightmapBaseName = folder + "Lightmaps/tx";
    templ.farSideTiling[0] = templ.farSideTiling[1] = 2.0f;
    templ.farTopTilingHi = 16.0f;
    templ.farTopTilingLow = 32.0f;
    templ.farYOffset = 0.0f;
    templ.sunColor[0] = templ.sunColor[1] = templ.sunColor[2] = 4.0f;
    templ.giColor[0] = 0.8f;
    templ.giColor[1] = 0.7f;
    templ.giColor[2] = 0.6f;
    templ.detailTextures.resize(6);
    for (size_t i = 0; i < templ.detailTextures.size(); ++i)
    {
        templ.detailTextures[i].sideTiling[0] = templ.detailTextures[i].sideTiling[1] = 2.0f;
        templ.detailTextures[i].topTiling = 32.0f;
    }
    templ.lodWords = lodSource.lodWords;
    templ.lodIndices = lodSource.lodIndices;
    templ.compactVertices = 0;
    for (int z = 0; z < count; ++z)
    {
        for (int x = 0; x < count; ++x)
        {
            TerrainRawPatch patch;
            patch.gridX = x - count / 2;
            patch.gridZ = z - count / 2;
            templ.patches.push_back(patch);
        }
    }
    // The heights unrounded, as the cluster answers the retail compile: the heightmap value times the vertical scale.
    const double primaryHeight = static_cast<double>(primaryRaw) * static_cast<double>(kPrimaryScaleY);
    const double secondaryHeight = static_cast<double>(secondaryRaw) * static_cast<double>(kSecondaryScaleY);
    const float world = static_cast<float>(s.worldSize());
    TerrainCompileSource source;
    source.height = [primaryHeight, secondaryHeight, world](float x, float z) {
        const int sx = clusterSector(x, world), sz = clusterSector(z, world);
        if (sx < -1 || sx > 1 || sz < -1 || sz > 1)
            return 0.0;
        return sx == 0 && sz == 0 ? primaryHeight : secondaryHeight;
    };
    source.seaWaterLevel = -10000.0f;
    source.weightSize = 512;
    source.weights = [](int, int) -> const unsigned char* { return 0; };
    TerrainRawFile compiled;
    if (!compileTerrainRaw(templ, source, compiled, error))
        return false;
    add("terraindata.raw", shared(compiled.render()));

    // The patch textures: the retail defaults copied, white lightmaps, black low-detail maps; the level's hemi map,
    // HUD maps and shadow maps.
    const auto colormap = shared(content.colormap);
    const auto detail1 = shared(content.detailmap1);
    const auto detail2 = shared(content.detailmap2);
    const auto lightmap = shared(flatTexture(512, 512, 0xFFFFFFFFu));
    DdsImage black;
    black.width = black.height = 256;
    black.pixels.assign(256 * 256, 0u);
    const auto lowDetail = shared(encodeDxt1(black, true));
    for (int x = 0; x < count; ++x)
    {
        for (int z = 0; z < count; ++z)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "tx%02dx%02d", x, z);
            add(std::string("Colormaps/") + name + ".dds", colormap);
            add(std::string("Detailmaps/") + name + "_1.dds", detail1);
            add(std::string("Detailmaps/") + name + "_2.dds", detail2);
            add(std::string("Lightmaps/") + name + ".dds", lightmap);
            add(std::string("LowDetailmaps/") + name + ".dds", lowDetail);
        }
    }
    add("groundhemi.dds", shared(flatTexture(8, 8, 0xFFFFFFFFu)));
    add("Hud/Minimap/ingameMap.dds", shared(content.ingameMap));
    add("Hud/Minimap/commanderMap.dds", shared(content.commanderMap));
    add("Hud/Minimap/commanderMicromap.dds", shared(content.commanderMicromap));
    const auto shadow = shared(content.shadowmap);
    add("SimpleShadowmap.raw", shadow);
    add("SimpleShadowmapio.raw", shadow);
    return true;
}

bool writeNewLevel(const std::string& target, const std::vector<NewLevelFile>& files, std::string& error)
{
    const std::string staging = target + ".tmp";
    if (GetFileAttributesA(target.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        error = target + " exists already";
        return false;
    }
    if (GetFileAttributesA(staging.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        error = staging + " is there (left by an interrupted run?): remove it first";
        return false;
    }
    std::vector<std::string> folders;           // the folders made, parents first
    std::set<std::string> made;
    const auto makeFolder = [&folders, &made](const std::string& path) {
        if (made.count(path))
            return true;
        if (!CreateDirectoryA(path.c_str(), 0))
            return false;
        made.insert(path);
        folders.push_back(path);
        return true;
    };
    bool ok = makeFolder(staging);
    if (!ok)
        error = "cannot make " + staging;
    size_t written = 0;
    for (; ok && written < files.size(); ++written)
    {
        const std::string& relative = files[written].path;
        for (size_t slash = relative.find('/'); ok && slash != std::string::npos; slash = relative.find('/', slash + 1))
            ok = makeFolder(staging + "/" + relative.substr(0, slash));
        if (ok)
        {
            const std::string& bytes = *files[written].bytes;
            FILE* out = std::fopen((staging + "/" + relative).c_str(), "wb");
            ok = out != 0;
            if (out)
            {
                ok = (bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size());
                ok = std::fclose(out) == 0 && ok;
            }
        }
        if (!ok)
            error = "cannot write " + relative;
    }
    if (ok && !MoveFileExA(staging.c_str(), target.c_str(), 0))
    {
        ok = false;
        error = "cannot move the new level into " + target;
    }
    if (!ok)
    {
        for (size_t i = 0; i < written + 1 && i < files.size(); ++i)
            DeleteFileA((staging + "/" + files[i].path).c_str());
        for (size_t i = folders.size(); i-- > 0;)
            RemoveDirectoryA(folders[i].c_str());
    }
    return ok;
}

} // namespace bf2editor
