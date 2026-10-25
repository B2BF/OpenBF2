// Code/Tools/BF2Editor/App/EditSessionTerrain.cpp

// EditSession's terrain commands (parity phase E; App/TerrainEditing does the engine work). A stroke
// is applied live, tick by tick, and recorded when it ends as one history entry whose scripts name the
// stroke's change: `EditorTerrain.applyChange <id> after` redoes it, `... before` undoes it.
// Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/FreeCamera.h"
#include "App/LevelDocument.h"
#include "Framework/EditorLog.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SourceControl.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace bf2editor
{

namespace
{

// A script's tick: 1/30 s, so a scripted Modify stroke of 30 ticks raises by strength * 0.5 metres at
// the centre - one second of the retail brush.
const float kScriptTick = 1.0f / 30.0f;

std::string lowerCase(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

std::string brushText(const TerrainBrushSettings& b)
{
    char text[256];
    if (b.mode == TerrainBrush::Overgrowth)
        std::snprintf(text, sizeof(text), "brush overgrowth, size %d, paints the %s map, material %d, shadow %d", b.size,
                      b.overgrowthShadow ? "shadow" : "material", b.overgrowthMaterial, b.overgrowthShade);
    else if (b.mode == TerrainBrush::Paint)
        std::snprintf(text, sizeof(text), "brush paint, size %d, strength %d, paints %s, layer %d, colour from %s, colour "
                      "%u/%u/%u", b.size, b.strength, terrainPaintModeName(b.paintMode), b.layer,
                      b.colorFromTexture ? "the layer's texture" : "the colour", b.color & 0xFFu, (b.color >> 8) & 0xFFu,
                      (b.color >> 16) & 0xFFu);
    else
        std::snprintf(text, sizeof(text), "brush %s, size %d, strength %d, raise %d, lower %d, height %.3f",
                      terrainBrushName(b.mode), b.size, b.strength, b.raise ? 1 : 0, b.lower ? 1 : 0, b.height);
    return text;
}

bool parseInt(const std::string& text, int low, int high, int& out)
{
    char* end = 0;
    const long v = std::strtol(text.c_str(), &end, 10);
    if (text.empty() || !end || *end != '\0' || v < low || v > high)
        return false;
    out = static_cast<int>(v);
    return true;
}

} // namespace

bool EditSession::beginTerrainStroke(std::string& error)
{
    if (!m_terrain.available(error))
        return false;
    if (m_terrainBrush.mode == TerrainBrush::Paint)
    {
        // Painting edits the renderer's maps in memory (merge / clear, as retail); there is no height snapshot and no
        // history entry - the retail plugin's undo held heights only (census §13).
        if (!m_editableTerrain.active())
        {
            error = "painting needs the editable terrain (start with +editableTerrain 1)";
            return false;
        }
        if (m_terrainBrush.layer < 0 || m_terrainBrush.layer > 5)
        {
            error = "no layer selected (the Terrain panel's layer, or EditorTerrain.brush layer 0..5)";
            return false;
        }
        m_paintStroke = true;
        return true;
    }
    if (m_terrainBrush.mode == TerrainBrush::Overgrowth)
    {
        // The overgrowth's maps, in the renderer (no history entry: not undoable, as retail).
        if (!m_editableTerrain.overgrowthAvailable(error))
            return false;
        m_paintStroke = true;
        return true;
    }
    return m_terrain.beginStroke(error);
}

bool EditSession::terrainStrokeTick(float x, float z, bool secondary, float frameTime, std::string& error)
{
    if (m_terrainBrush.mode == TerrainBrush::Paint)
    {
        // The retail brush state (census §2.1): the raw cursor, the radius size * the primary tile's spacing.
        int centreX = 0, centreZ = 0;
        float radius = 0.0f;
        if (!m_terrain.brushAt(m_terrainBrush, x, z, centreX, centreZ, radius, error))
            return false;
        return m_editableTerrain.paint(m_terrainBrush, x, z, radius, secondary, error);
    }
    if (m_terrainBrush.mode == TerrainBrush::Overgrowth)
    {
        int centreX = 0, centreZ = 0;
        float radius = 0.0f;
        if (!m_terrain.brushAt(m_terrainBrush, x, z, centreX, centreZ, radius, error))
            return false;
        return m_editableTerrain.paintOvergrowth(m_terrainBrush, x, z, radius, secondary, error);
    }
    return m_terrain.apply(m_terrainBrush, x, z, secondary, frameTime, error);
}

void EditSession::endTerrainStroke()
{
    if (m_paintStroke)
    {
        m_paintStroke = false;
        if (m_terrainBrush.mode == TerrainBrush::Overgrowth)
            logLine(LogLevel::Info, "edit: overgrowth paint (the %s map) - not undoable",
                    m_terrainBrush.overgrowthShadow ? "shadow" : "material");
        else
            logLine(LogLevel::Info, "edit: terrain paint (%s, layer %d) - Merge keeps it, Clear drops it; saved with the "
                    "level", terrainPaintModeName(m_terrainBrush.paintMode), m_terrainBrush.layer);
        return;
    }
    TerrainChange change;
    m_terrain.endStroke(change);
    char label[96];
    std::snprintf(label, sizeof(label), "Terrain %s (%u samples)", terrainBrushName(m_terrainBrush.mode),
                  static_cast<unsigned>(change.samples()));
    recordTerrainChange(change, label);
}

bool EditSession::recordTerrainChange(const TerrainChange& change, const std::string& label)
{
    if (change.tiles.empty())
        return false;
    const int id = m_nextTerrainChange++;
    m_terrainChanges[id] = change;
    HistoryEntry entry;
    entry.label = label;
    char line[64];
    std::snprintf(line, sizeof(line), "EditorTerrain.applyChange %d after", id);
    entry.doScript = line;
    std::snprintf(line, sizeof(line), "EditorTerrain.applyChange %d before", id);
    entry.undoScript = line;
    m_history.record(entry);
    return true;
}

bool EditSession::terrainSectors(std::string& result)
{
    const std::vector<TerrainEditing::Sector> sectors = m_terrain.sectors();
    if (sectors.empty())
    {
        result = "no heightmap cluster";
        return false;
    }
    result.clear();
    for (size_t i = 0; i < sectors.size(); ++i)
    {
        const TerrainEditing::Sector& s = sectors[i];
        char text[96];
        if (s.present)
            std::snprintf(text, sizeof(text), "%s%d,%d %dx%d %d-bit %s", i ? "; " : "", s.x, s.z, s.width, s.width,
                          s.bits, s.modified ? "active" : "inactive");
        else
            std::snprintf(text, sizeof(text), "%s%d,%d none", i ? "; " : "", s.x, s.z);
        result += text;
    }
    return true;
}

bool EditSession::terrainSetSector(int x, int z, bool active, std::string& result)
{
    const std::vector<TerrainEditing::Sector> sectors = m_terrain.sectors();
    bool found = false;
    bool current = false;
    for (size_t i = 0; i < sectors.size(); ++i)
        if (sectors[i].x == x && sectors[i].z == z && sectors[i].present)
        {
            found = true;
            current = sectors[i].modified;
        }
    char text[96];
    if (!found)
    {
        std::snprintf(text, sizeof(text), "no tile in sector %d,%d", x, z);
        result = text;
        return false;
    }
    if (x == 0 && z == 0)
    {
        result = "the primary tile is always saved";
        return false;
    }
    if (current == active)
    {
        std::snprintf(text, sizeof(text), "sector %d,%d is %s already", x, z, active ? "active" : "inactive");
        result = text;
        return true;
    }
    HistoryEntry entry;
    std::snprintf(text, sizeof(text), "%s surrounding tile %d,%d", active ? "Activate" : "Deactivate", x, z);
    entry.label = text;
    std::snprintf(text, sizeof(text), "EditorTerrain.applySector %d %d %d", x, z, active ? 1 : 0);
    entry.doScript = text;
    std::snprintf(text, sizeof(text), "EditorTerrain.applySector %d %d %d", x, z, active ? 0 : 1);
    entry.undoScript = text;
    if (!m_history.execute(entry, result))
        return false;
    result = entry.label;
    return true;
}

bool EditSession::terrainSetSeaLevel(float level, std::string& result)
{
    float current = 0.0f;
    if (!m_terrain.seaLevel(current))
    {
        result = "no heightmap cluster";
        return false;
    }
    if (!(level > -10000.0f && level < 10000.0f))
    {
        result = "the sea level is a height in metres";
        return false;
    }
    char text[96];
    if (current == level)
    {
        std::snprintf(text, sizeof(text), "the sea level is %g already", level);
        result = text;
        return true;
    }
    HistoryEntry entry;
    std::snprintf(text, sizeof(text), "Sea level %g", level);
    entry.label = text;
    std::snprintf(text, sizeof(text), "EditorTerrain.applySeaLevel %.9g", level);
    entry.doScript = text;
    std::snprintf(text, sizeof(text), "EditorTerrain.applySeaLevel %.9g", current);
    entry.undoScript = text;
    if (!m_history.execute(entry, result))
        return false;
    result = entry.label;
    return true;
}

bool EditSession::terrainApplySeaLevel(float level, std::string& error)
{
    return m_terrain.setSeaLevel(level, error);
}

bool EditSession::terrainApplySector(int x, int z, bool active, std::string& error)
{
    return m_terrain.setSectorModified(x, z, active, error);
}

bool EditSession::terrainImportSurrounding(const std::string& path, std::string& result)
{
    if (!beginTerrainStroke(result))
        return false;
    const bool ok = m_terrain.importSurrounding(path, result);
    TerrainChange change;
    m_terrain.endStroke(change);
    if (!ok)
    {
        std::string ignored;
        if (!change.tiles.empty())
            m_terrain.restore(change, false, ignored);
        return false;
    }
    char label[96];
    std::snprintf(label, sizeof(label), "Import surrounding terrain (%u samples)", static_cast<unsigned>(change.samples()));
    result = recordTerrainChange(change, label) ? std::string(label) : std::string("the import changed nothing");
    return true;
}

bool EditSession::terrainSmoothEdges(int width, float amplitude, float falloff, std::string& result)
{
    if (!beginTerrainStroke(result))
        return false;
    const bool ok = m_terrain.smoothEdges(width, amplitude, falloff, result);
    TerrainChange change;
    m_terrain.endStroke(change);
    if (!ok)
        return false;
    char label[96];
    std::snprintf(label, sizeof(label), "Smooth terrain edges (%u samples)", static_cast<unsigned>(change.samples()));
    result = recordTerrainChange(change, label) ? std::string(label) : std::string("smoothing changed nothing");
    return true;
}

void EditSession::setTerrainToolActive(bool active)
{
    if (!active && m_terrain.strokeActive())
        endTerrainStroke();
    // One tool has the mouse at a time: the object brush and the undergrowth tool go off when the terrain
    // brush comes on.
    if (active && m_brushTool)
        setBrushToolActive(false);
    if (active && m_undergrowthTool)
        setUndergrowthToolActive(false);
    m_terrainTool = active;
}

bool EditSession::setTerrainTool(bool active, std::string& result)
{
    if (active && !m_terrain.available(result))
        return false;
    setTerrainToolActive(active);
    result = active ? "the terrain brush is on: the left button paints" : "the terrain brush is off";
    return true;
}

bool EditSession::terrainUnderPixel(float x, float y, float out[3]) const
{
    if (m_width <= 0 || m_height <= 0)
        return false;
    float origin[3], direction[3];
    m_camera.rayThroughPixel(x, y, m_width, m_height, origin, direction);
    const float t = terrainDistance(origin, direction, m_camera.farPlane());
    if (t < 0.0f)
        return false;
    for (int i = 0; i < 3; ++i)
        out[i] = origin[i] + direction[i] * t;
    return true;
}

bool EditSession::terrainInfo(std::string& result)
{
    std::string why;
    result = m_terrain.describe() + "; " + brushText(m_terrainBrush) + (m_terrainTool ? "; tool on" : "; tool off");
    if (!m_terrain.available(why))
        result += "; not editable: " + why;
    else if (m_terrain.changedSinceSave())
        result += "; changed since the level opened or was saved";
    return true;
}

bool EditSession::terrainHeight(float x, float z, std::string& result)
{
    float h = 0.0f;
    if (!m_terrain.heightAt(x, z, h))
    {
        result = "no ground there";
        return false;
    }
    char text[64];
    std::snprintf(text, sizeof(text), "%.3f", h);
    result = text;
    return true;
}

bool EditSession::terrainSetBrush(const std::string& key, const std::string& value, std::string& result)
{
    TerrainBrushSettings b = m_terrainBrush;
    int n = 0;
    if (key == "mode")
    {
        if (!terrainBrushFromName(value, b.mode))
        {
            result = "no brush '" + value + "' (modify, smooth, level, platform, random, paint, overgrowth)";
            return false;
        }
    }
    else if (key == "size")
    {
        if (!parseInt(value, 1, 50, n))
        {
            result = "size is 1..50";
            return false;
        }
        b.size = n;
    }
    else if (key == "strength")
    {
        if (!parseInt(value, 1, 100, n))
        {
            result = "strength is 1..100";
            return false;
        }
        b.strength = n;
    }
    else if (key == "raise" || key == "lower")
    {
        if (!parseInt(value, 0, 1, n))
        {
            result = key + " is 0 or 1";
            return false;
        }
        (key == "raise" ? b.raise : b.lower) = n != 0;
    }
    else if (key == "height")
    {
        char* end = 0;
        const float h = std::strtof(value.c_str(), &end);
        if (value.empty() || !end || *end != '\0')
        {
            result = "height is a number (metres)";
            return false;
        }
        b.height = h;
    }
    else if (key == "paints")
    {
        if (value == "detail")
            b.paintMode = TerrainPaintMode::Detail;
        else if (value == "color" || value == "colour")
            b.paintMode = TerrainPaintMode::Color;
        else if (value == "both")
            b.paintMode = TerrainPaintMode::Both;
        else
        {
            result = "a paint stroke paints detail, color or both";
            return false;
        }
    }
    else if (key == "layer")
    {
        if (!parseInt(value, -1, 5, n))
        {
            result = "layer is 0..5 (-1: none)";
            return false;
        }
        b.layer = n;
    }
    else if (key == "overgrowth")
    {
        if (value != "material" && value != "shadow")
        {
            result = "the overgrowth brush paints the material or the shadow map";
            return false;
        }
        b.overgrowthShadow = value == "shadow";
    }
    else if (key == "material")
    {
        // 0 erases; 16 and above overrun the renderer's per-material arrays (postLoadResources).
        if (!parseInt(value, 1, 15, n))
        {
            result = "an overgrowth material is 1..15 (the ids of the level's Overgrowth.con)";
            return false;
        }
        b.overgrowthMaterial = n;
    }
    else if (key == "shadow")
    {
        if (!parseInt(value, 0, 255, n))
        {
            result = "the shadow value is 0..255";
            return false;
        }
        b.overgrowthShade = n;
    }
    else if (key == "colorSource")
    {
        if (value != "texture" && value != "color")
        {
            result = "the colour source is texture (the layer's colour texture) or color";
            return false;
        }
        b.colorFromTexture = value == "texture";
    }
    else if (key == "color")
    {
        unsigned r = 0, g = 0, bl = 0;
        char tail = 0;
        if (std::sscanf(value.c_str(), "%u/%u/%u%c", &r, &g, &bl, &tail) != 3 || r > 255 || g > 255 || bl > 255)
        {
            result = "the colour is r/g/b, each 0..255";
            return false;
        }
        b.color = 0xFF000000u | (bl << 16) | (g << 8) | r;
    }
    else
    {
        result = "no brush setting '" + key + "' (mode, size, strength, raise, lower, height, paints, layer, colorSource, "
                 "color, overgrowth, material, shadow)";
        return false;
    }
    m_terrainBrush = b;
    result = brushText(b);
    return true;
}

bool EditSession::terrainStroke(float x, float z, int ticks, bool secondary, std::string& result)
{
    if (ticks < 1 || ticks > 10000)
    {
        result = "ticks is 1..10000";
        return false;
    }
    if (!beginTerrainStroke(result))
        return false;
    for (int i = 0; i < ticks; ++i)
    {
        if (!terrainStrokeTick(x, z, secondary, kScriptTick, result))
        {
            endTerrainStroke();
            return false;
        }
    }
    const int before = m_nextTerrainChange;
    const bool painting = m_paintStroke;
    endTerrainStroke();
    if (painting)
    {
        char text[160];
        if (m_terrainBrush.mode == TerrainBrush::Overgrowth)
            std::snprintf(text, sizeof(text), "painted overgrowth %s %d, %d ticks at %.1f/%.1f",
                          m_terrainBrush.overgrowthShadow ? "shadow" : "material",
                          secondary ? 0 : (m_terrainBrush.overgrowthShadow ? m_terrainBrush.overgrowthShade
                                                                           : m_terrainBrush.overgrowthMaterial),
                          ticks, x, z);
        else
            std::snprintf(text, sizeof(text), "painted %s, layer %d, %d ticks at %.1f/%.1f (unmerged)",
                          terrainPaintModeName(m_terrainBrush.paintMode), m_terrainBrush.layer, ticks, x, z);
        result = text;
        return true;
    }
    if (m_nextTerrainChange == before)
    {
        result = "the stroke changed nothing";
        if (m_terrainBrush.mode == TerrainBrush::Platform && secondary)
        {
            char text[96];
            std::snprintf(text, sizeof(text), "height %.3f taken from the ground", m_terrainBrush.height);
            result = text;
        }
        return true;
    }
    const TerrainChange& change = m_terrainChanges[before];
    char text[160];
    std::snprintf(text, sizeof(text), "%s: %u samples in %u tile(s) changed (change %d)",
                  terrainBrushName(m_terrainBrush.mode), static_cast<unsigned>(change.samples()),
                  static_cast<unsigned>(change.tiles.size()), before);
    result = text;
    return true;
}

bool EditSession::groundSet(int layer, const std::string& name, const std::string& value, std::string& result)
{
    if (!m_editableTerrain.active())
    {
        result = "ground properties need the editable terrain (start with +editableTerrain 1)";
        return false;
    }
    if (layer < -1 || layer > 5)
    {
        result = "a ground layer is 0..5 (-1: the terrain's own settings)";
        return false;
    }
    // The value as it stands, for the undo: a layer not in the file yet is taken out again.
    const GroundProperties& ground = m_editableTerrain.ground();
    std::string old;
    const bool existed = layer < 0 || ground.layers.count(layer) != 0;
    const bool known = layer < 0 ? ground.getMisc(name, old) : (existed ? ground.getLayer(layer, name, old) : true);
    if (!known)
    {
        result = "no ground setting '" + name + "'";
        return false;
    }
    // Checked on a copy first, so a refused value records nothing.
    GroundProperties trial = ground;
    std::string error;
    if (!(layer < 0 ? trial.setMisc(name, value, error) : trial.setLayer(layer, name, value, error)))
    {
        result = error;
        return false;
    }
    char head[48];
    std::snprintf(head, sizeof(head), "EditorGround.apply %d ", layer);
    HistoryEntry entry;
    entry.doScript = std::string(head) + name + " \"" + value + "\"\n";
    if (existed)
        entry.undoScript = std::string(head) + name + " \"" + old + "\"\n";
    else
    {
        std::snprintf(head, sizeof(head), "EditorGround.removeLayer %d\n", layer);
        entry.undoScript = head;
    }
    entry.label = layer < 0 ? "Set the terrain's " + name : "Set ground layer " + std::to_string(layer) + " " + name;
    if (!m_history.execute(entry, result))
        return false;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    std::string now;
    if (layer < 0)
        m_editableTerrain.ground().getMisc(name, now);
    else
        m_editableTerrain.ground().getLayer(layer, name, now);
    result = entry.label + " = " + now;
    if (layer >= 0 && name == "material")
        result += " (kept in GroundProperties.con; not applied to the renderer yet)";
    return true;
}

bool EditSession::groundApply(int layer, const std::string& name, const std::string& value, std::string& result)
{
    return m_editableTerrain.setGround(layer, name, value, result);
}

bool EditSession::groundRemoveLayer(int layer, std::string& result)
{
    return m_editableTerrain.removeGroundLayer(layer, result);
}

std::string EditSession::groundListing() const
{
    const GroundProperties& ground = m_editableTerrain.ground();
    std::string text = "ground properties from " + m_editableTerrain.groundSource() + ":";
    for (std::map<int, GroundLayer>::const_iterator it = ground.layers.begin(); it != ground.layers.end(); ++it)
    {
        text += " layer " + std::to_string(it->first) + " {";
        const std::vector<std::string>& names = GroundProperties::layerSettings();
        for (size_t i = 0; i < names.size(); ++i)
        {
            std::string value;
            ground.getLayer(it->first, names[i], value);
            text += (i ? ", " : "") + names[i] + " " + (value.empty() ? std::string("-") : value);
        }
        text += "};";
    }
    const std::vector<std::string>& misc = GroundProperties::miscSettings();
    for (size_t i = 0; i < misc.size(); ++i)
    {
        std::string value;
        ground.getMisc(misc[i], value);
        text += (i ? ", " : " terrain: ") + misc[i] + " " + value;
    }
    if (m_editableTerrain.groundChanged())
        text += "; changed";
    return text;
}

void EditSession::planGroundProperties(SavePlan& plan)
{
    if (!m_editableTerrain.active() || !m_editableTerrain.groundChanged())
        return;
    SavePart& part = plan.part("ground properties");
    if (!m_document || !m_document->isOpen())
    {
        part.refuse("no level document: the ground properties live in the level's Editor folder");
        return;
    }
    part.add(m_document->levelDirectory() + "Editor/GroundProperties.con", m_editableTerrain.ground().render());
    part.onWritten([this]() { m_editableTerrain.markGroundSaved(); });
    part.onReport([](const SavePart&) { return std::string("ground properties saved (Editor/GroundProperties.con)"); });
}

void EditSession::planTerrainMaps(SavePlan& plan)
{
    if (!m_editableTerrain.active() || !m_editableTerrain.painted())
        return;
    SavePart& part = plan.part("painted terrain");
    if (!m_document || !m_document->isOpen())
    {
        part.refuse("no level document: the painted maps live in the level's Editor folder");
        return;
    }
    std::vector<std::pair<std::string, std::string> > files;
    std::string error;
    if (!m_editableTerrain.saveMaps(files, error))
    {
        part.refuse("painted terrain NOT saved: " + error);
        return;
    }
    addStagedFiles(part, files);
    part.onWritten([this]() { m_editableTerrain.markMapsSaved(); });
    part.onReport([](const SavePart& written) {
        char text[256];
        std::snprintf(text, sizeof(text), "painted terrain saved (%d file%s in the level's Editor folder)",
                      static_cast<int>(written.changed()), written.changed() == 1 ? "" : "s");
        return std::string(text);
    });
}

void EditSession::planCompiledTerrain(SavePlan& plan, bool heights, bool surface)
{
    if (!heights && !surface)
        return;
    SavePart& part = plan.part("terrain");
    if (!m_editableTerrain.active())
    {
        part.refuse("terrain NOT saved: saving it compiles the terrain the game draws, which needs the editable terrain "
                    "(start the editor with +editableTerrain 1)");
        return;
    }
    if (!m_document || !m_document->isOpen())
    {
        part.refuse("no level document: the terrain lives in the level's folder");
        return;
    }
    std::vector<std::pair<std::string, std::string> > files;
    std::string note;
    if (!m_editableTerrain.compileTerrain(files, note))
    {
        part.refuse("terrain NOT saved: " + note);
        return;
    }
    const size_t compiledFiles = files.size();
    if (heights)
    {
        // "mods/<mod>/Levels/<name>/" -> the level path Heightdata.con names ("Levels/<name>").
        const std::string& levelDir = m_document->levelDirectory();
        const size_t levels = levelDir.find("Levels/");
        std::string levelPath = levels == std::string::npos ? std::string() : levelDir.substr(levels);
        if (!levelPath.empty() && levelPath[levelPath.size() - 1] == '/')
            levelPath.erase(levelPath.size() - 1);
        char stamp[32];
        std::snprintf(stamp, sizeof(stamp), "/heightmaps_%lu", static_cast<unsigned long>(GetTickCount()));
        const std::string root = m_editableTerrain.stagingRoot() + stamp;
        std::vector<std::string> written;
        std::string error;
        if (levelPath.empty() || !m_terrain.saveHeightmaps(root, levelPath, written, error))
        {
            part.refuse("terrain NOT saved: " + (error.empty() ? "no Levels/ in " + levelDir : error));
            return;
        }
        const std::string prefix = lowerCase(levelPath + "/");
        for (size_t i = 0; i < written.size(); ++i)
        {
            std::string path = written[i];
            std::replace(path.begin(), path.end(), '\\', '/');
            if (lowerCase(path).compare(0, prefix.size(), prefix) == 0)
                files.push_back(std::make_pair(root + "/" + path, path.substr(prefix.size())));
        }
    }
    addStagedFiles(part, files);
    part.onWritten([this, heights]() {
        m_editableTerrain.markCompiled();
        if (heights)
            m_terrain.markSaved();
    });
    part.onReport([compiledFiles, heights](const SavePart& written) {
        char text[256];
        std::snprintf(text, sizeof(text), "terrain compiled and saved (%d of %u files changed: terraindata.raw, %u game "
                      "texture%s%s)", static_cast<int>(written.changed()), static_cast<unsigned>(written.planned()),
                      static_cast<unsigned>(compiledFiles - 1), compiledFiles == 2 ? "" : "s", heights ? ", the heightmaps" : "");
        return std::string(text);
    });
}

void EditSession::addStagedFiles(SavePart& part, const std::vector<std::pair<std::string, std::string> >& files) const
{
    for (size_t i = 0; i < files.size(); ++i)
        part.addStaged(m_document->levelDirectory() + files[i].second, files[i].first);
}

bool EditSession::copyStagedFiles(const std::vector<std::pair<std::string, std::string> >& files, int& written,
                                  std::string& error)
{
    written = 0;
    if (!m_document || !m_document->isOpen())
    {
        error = "no level document: nothing can be written into the level";
        return false;
    }
    SavePlan plan;
    SavePart& part = plan.part("level files");
    addStagedFiles(part, files);
    plan.dropUnchanged();
    const size_t changed = part.changed();
    std::string result;
    if (!writeSave(plan, "lighting", result) || plan.anyRefused())
    {
        error = result;
        return false;
    }
    written = static_cast<int>(changed);
    return true;
}

void EditSession::planOvergrowth(SavePlan& plan, bool required)
{
    if (!m_editableTerrain.active())
    {
        plan.part("overgrowth").refuse("overgrowth NOT saved: it is generated with the editable terrain (+editableTerrain 1)");
        return;
    }
    std::string why;
    if (!m_editableTerrain.overgrowthAvailable(why))
    {
        if (required)
            plan.part("overgrowth").refuse("overgrowth NOT saved: " + why);
        return;
    }
    SavePart& part = plan.part("overgrowth");
    if (!m_document || !m_document->isOpen())
    {
        part.refuse("no level document: the overgrowth lives in the level's folder");
        return;
    }
    std::vector<std::pair<std::string, std::string> > files;
    std::string note;
    if (!m_editableTerrain.saveOvergrowth(files, note))
    {
        part.refuse("overgrowth NOT saved: " + note);
        return;
    }
    addStagedFiles(part, files);
    part.onWritten([this]() { m_editableTerrain.markOvergrowthSaved(); });
    part.onReport([note](const SavePart& written) {
        char text[256];
        std::snprintf(text, sizeof(text), "overgrowth saved (%d file%s in the level's overgrowth folder; %s)",
                      static_cast<int>(written.changed()), written.changed() == 1 ? "" : "s", note.c_str());
        return std::string(text);
    });
}

bool EditSession::saveTerrainPart(std::string& result)
{
    SavePlan plan;
    planGroundProperties(plan);
    planTerrainMaps(plan);
    planCompiledTerrain(plan, m_terrain.changedSinceSave(), true);
    plan.dropUnchanged();
    return writeSave(plan, "terrain", result) && !plan.anyRefused();
}

bool EditSession::saveOvergrowthPart(bool required, std::string& result)
{
    SavePlan plan;
    planOvergrowth(plan, required);
    plan.dropUnchanged();
    return writeSave(plan, "overgrowth", result) && !plan.anyRefused();
}

bool EditSession::terrainApplyChange(int id, const std::string& state, std::string& error)
{
    const std::map<int, TerrainChange>::const_iterator it = m_terrainChanges.find(id);
    if (it == m_terrainChanges.end())
    {
        char text[64];
        std::snprintf(text, sizeof(text), "no terrain change %d", id);
        error = text;
        return false;
    }
    if (state != "before" && state != "after")
    {
        error = "the state is before or after";
        return false;
    }
    return m_terrain.restore(it->second, state == "after", error);
}

bool EditSession::terrainExportHeightmaps(const std::string& directory, std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level document";
        return false;
    }
    // "mods/bf2/Levels/<name>/" -> the level path the .con names ("Levels/<name>").
    const std::string& levelDir = m_document->levelDirectory();
    const size_t levels = levelDir.find("Levels/");
    if (levels == std::string::npos)
    {
        result = "cannot find Levels/ in " + levelDir;
        return false;
    }
    std::string levelPath = levelDir.substr(levels);
    if (!levelPath.empty() && levelPath[levelPath.size() - 1] == '/')
        levelPath.erase(levelPath.size() - 1);
    char full[MAX_PATH];
    if (!GetFullPathNameA(directory.c_str(), MAX_PATH, full, 0))
    {
        result = "bad directory " + directory;
        return false;
    }
    CreateDirectoryA(full, 0);
    std::vector<std::string> written;
    std::string error;
    if (!m_terrain.saveHeightmaps(full, levelPath, written, error))
    {
        result = error;
        return false;
    }
    char text[128];
    std::snprintf(text, sizeof(text), "%u files written under ", static_cast<unsigned>(written.size()));
    result = text + std::string(full);
    for (size_t i = 0; i < written.size(); ++i)
        result += (i ? ", " : ": ") + written[i];
    return true;
}

} // namespace bf2editor
