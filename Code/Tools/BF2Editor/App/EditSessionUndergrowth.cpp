// Code/Tools/BF2Editor/App/EditSessionUndergrowth.cpp

// EditSession's undergrowth. 1.50 ships no undergrowth editing: the
// five editor hooks of RendDX9's UndergrowthSystem are empty bodies, and the 2006 editor replaced the whole
// system with an editable one (UndergrowthSystemEditable). The editor keeps the level's density map itself
// (Framework/UndergrowthMap) and paints it as the 2006 editable's paint hook did - a disc of one material
// id, or 0 - one undoable edit per stroke, and writes Undergrowth.raw whole with the level. The renderer
// keeps the map it loaded, so the painted undergrowth grows when the level loads again.
// Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/LevelDocument.h"
#include "Framework/EditorLog.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SourceControl.h"

#include <cstdio>
#include <cstdlib>

namespace bf2editor
{

bool EditSession::loadUndergrowth(std::string& error)
{
    if (m_undergrowthTried)
    {
        if (!m_undergrowthLoaded)
            error = "the level's undergrowth could not be read (see the log)";
        return m_undergrowthLoaded;
    }
    m_undergrowthTried = true;
    if (!m_document || !m_document->isOpen())
    {
        error = "no level document";
        return false;
    }
    const std::string folder = m_document->levelDirectory();
    std::string raw, config;
    if (!ObjectLibrary::readTextFile(folder + "Undergrowth.raw", raw, error))
    {
        error = "the level has no Undergrowth.raw";
        logLine(LogLevel::Info, "undergrowth: %s", error.c_str());
        return false;
    }
    if (!m_undergrowth.load(raw, error))
    {
        logLine(LogLevel::Warning, "undergrowth: %s", error.c_str());
        return false;
    }
    std::string why;
    if (ObjectLibrary::readTextFile(folder + "Undergrowth.cfg", config, why) && !m_undergrowthConfig.parse(config, why))
        logLine(LogLevel::Warning, "undergrowth: Undergrowth.cfg: %s", why.c_str());
    m_undergrowthSaved = raw;
    m_undergrowthConfigSaved = m_undergrowthConfig.render();
    m_undergrowthLoaded = true;
    char text[160];
    std::snprintf(text, sizeof(text), "Undergrowth.raw %dx%d, %u materials in Undergrowth.cfg", m_undergrowth.size(),
                  m_undergrowth.size(), static_cast<unsigned>(m_undergrowthConfig.materials.size()));
    logLine(LogLevel::Info, "undergrowth: %s", text);
    return true;
}

const UndergrowthMap* EditSession::undergrowthMap()
{
    std::string ignored;
    return loadUndergrowth(ignored) ? &m_undergrowth : 0;
}

const UndergrowthConfig* EditSession::undergrowthConfig()
{
    std::string ignored;
    return loadUndergrowth(ignored) ? &m_undergrowthConfig : 0;
}

bool EditSession::undergrowthChanged() const
{
    return m_undergrowthLoaded && (m_undergrowth.bytes() != m_undergrowthSaved || undergrowthConfigChanged());
}

bool EditSession::undergrowthConfigChanged() const
{
    return m_undergrowthLoaded && m_undergrowthConfig.render() != m_undergrowthConfigSaved;
}

bool EditSession::recordUndergrowthConfig(const std::string& before, const std::string& label, std::string& result)
{
    const std::string after = m_undergrowthConfig.render();
    if (after == before)
    {
        result = "Undergrowth.cfg has that already";
        return true;
    }
    const int id = m_nextUndergrowthChange++;
    m_undergrowthConfigEdits[id] = std::make_pair(before, after);
    HistoryEntry entry;
    entry.label = label;
    char line[80];
    std::snprintf(line, sizeof(line), "EditorUndergrowth.applyConfig %d after\n", id);
    entry.doScript = line;
    std::snprintf(line, sizeof(line), "EditorUndergrowth.applyConfig %d before\n", id);
    entry.undoScript = line;
    m_history.record(entry);
    logLine(LogLevel::Info, "edit: %s", label.c_str());
    result = label + " (Undergrowth.cfg; the undergrowth takes it when the level loads again)";
    return true;
}

bool EditSession::setUndergrowthSetting(const std::string& key, float value, std::string& result)
{
    if (!loadUndergrowth(result))
        return false;
    const std::string before = m_undergrowthConfig.render();
    for (size_t i = 0; i < m_undergrowthConfig.settings.size(); ++i)
    {
        if (_stricmp(m_undergrowthConfig.settings[i].first.c_str(), key.c_str()) != 0)
            continue;
        m_undergrowthConfig.settings[i].second = value;
        char label[128];
        std::snprintf(label, sizeof(label), "Set the undergrowth's %s to %g", m_undergrowthConfig.settings[i].first.c_str(), value);
        return recordUndergrowthConfig(before, label, result);
    }
    result = "Undergrowth.cfg has no setting '" + key + "' (ViewDistance, ViewDistanceFadeScale, ViewDistanceHeightScale, "
             "ViewDistanceHeight2Scale, ViewDistanceStreamingScale, PatchSubdivide, SwayScale, LightingScale, AlphaRef)";
    return false;
}

bool EditSession::setUndergrowthMaterialHeight(int id, float value, std::string& result)
{
    if (!loadUndergrowth(result))
        return false;
    UndergrowthMaterial* material = m_undergrowthConfig.materialById(id);
    if (!material)
    {
        result = "no undergrowth material with that id";
        return false;
    }
    const std::string before = m_undergrowthConfig.render();
    material->generalHeight = value;
    char label[160];
    std::snprintf(label, sizeof(label), "Set the height of the undergrowth material %s to %g", material->name.c_str(), value);
    return recordUndergrowthConfig(before, label, result);
}

bool EditSession::setUndergrowthTypeProperty(int id, const std::string& typeName, const std::string& key,
                                             const std::string& value, std::string& result)
{
    if (!loadUndergrowth(result))
        return false;
    UndergrowthMaterial* material = m_undergrowthConfig.materialById(id);
    UndergrowthType* type = 0;
    for (size_t i = 0; material && i < material->types.size(); ++i)
        if (_stricmp(material->types[i].name.c_str(), typeName.c_str()) == 0)
            type = &material->types[i];
    if (!type)
    {
        result = "no undergrowth type '" + typeName + "' in that material";
        return false;
    }
    const std::string before = m_undergrowthConfig.render();
    char* end = 0;
    const float number = static_cast<float>(std::strtod(value.c_str(), &end));
    const bool isNumber = !value.empty() && end && *end == 0;
    if (_stricmp(key.c_str(), "randomSizeScale") == 0)
    {
        const size_t slash = value.find('/');
        char* end0 = 0;
        char* end1 = 0;
        const float a = slash == std::string::npos ? 0.0f : static_cast<float>(std::strtod(value.substr(0, slash).c_str(), &end0));
        const float b = slash == std::string::npos ? 0.0f : static_cast<float>(std::strtod(value.c_str() + slash + 1, &end1));
        if (slash == std::string::npos || !end0 || *end0 || !end1 || *end1)
        {
            result = "randomSizeScale is min/max";
            return false;
        }
        type->randomSizeScale[0] = a;
        type->randomSizeScale[1] = b;
    }
    else if (!isNumber)
    {
        result = key + " takes a number";
        return false;
    }
    else if (_stricmp(key.c_str(), "density") == 0)
        type->density = number;
    else if (_stricmp(key.c_str(), "variation") == 0)
        type->variation = number;
    else if (_stricmp(key.c_str(), "terrainColorScale") == 0)
        type->terrainColorScale = number;
    else if (_stricmp(key.c_str(), "typeSwayScale") == 0)
        type->typeSwayScale = number;
    else if (_stricmp(key.c_str(), "skew") == 0 && (number == 0.0f || number == 1.0f))
        type->skew = number != 0.0f;
    else
    {
        result = "a type's editable values are density, variation, terrainColorScale, typeSwayScale, skew (0 or 1) and "
                 "randomSizeScale (min/max)";
        return false;
    }
    return recordUndergrowthConfig(before, "Set " + key + " of the undergrowth type " + type->name + " to " + value, result);
}

bool EditSession::applyUndergrowthConfig(int id, const std::string& state, std::string& error)
{
    std::map<int, std::pair<std::string, std::string> >::const_iterator found = m_undergrowthConfigEdits.find(id);
    if (found == m_undergrowthConfigEdits.end() || (state != "before" && state != "after"))
    {
        error = "no Undergrowth.cfg edit with that id, or the state is not before or after";
        return false;
    }
    return m_undergrowthConfig.parse(state == "after" ? found->second.second : found->second.first, error);
}

bool EditSession::undergrowthInfo(std::string& result)
{
    if (!loadUndergrowth(result))
        return false;
    const std::vector<int> counts = m_undergrowth.counts();
    char text[200];
    std::snprintf(text, sizeof(text), "Undergrowth.raw %dx%d (%.2f m a cell), %d cells without undergrowth; painting %d "
                  "within %.1f m%s; materials:", m_undergrowth.size(), m_undergrowth.size(), undergrowthScale(), counts[0],
                  m_undergrowthMaterial, m_undergrowthRadius, undergrowthChanged() ? "; painted since the save" : "");
    result = text;
    for (size_t i = 0; i < m_undergrowthConfig.materials.size(); ++i)
    {
        const UndergrowthMaterial& material = m_undergrowthConfig.materials[i];
        std::snprintf(text, sizeof(text), " %s %d (%d cells)", material.name.c_str(), material.id,
                      material.id >= 0 && material.id < 256 ? counts[static_cast<size_t>(material.id)] : 0);
        result += (i ? "," : "") + std::string(text);
    }
    return true;
}

void EditSession::setUndergrowthToolActive(bool active)
{
    if (!active && m_undergrowthStroke)
        endUndergrowthStroke();
    // One tool has the mouse at a time.
    if (active && m_terrainTool)
        setTerrainToolActive(false);
    if (active && m_brushTool)
        setBrushToolActive(false);
    m_undergrowthTool = active;
}

bool EditSession::setUndergrowthTool(bool active, std::string& result)
{
    if (active && !loadUndergrowth(result))
        return false;
    setUndergrowthToolActive(active);
    char text[160];
    std::snprintf(text, sizeof(text), "the undergrowth tool is on: the left button paints material %d, the middle button "
                  "or Shift+left erases", m_undergrowthMaterial);
    result = active ? std::string(text) : std::string("the undergrowth tool is off");
    return true;
}

bool EditSession::setUndergrowthMaterial(int id, std::string& result)
{
    if (!loadUndergrowth(result))
        return false;
    if (id < 1 || id > 255 || !m_undergrowthConfig.materialById(id))
    {
        result = "no undergrowth material with that id in Undergrowth.cfg (EditorUndergrowth.info lists them)";
        return false;
    }
    m_undergrowthMaterial = id;
    result = "painting " + m_undergrowthConfig.materialById(id)->name;
    return true;
}

bool EditSession::setUndergrowthRadius(float radius, std::string& result)
{
    if (!(radius >= 0.5f && radius <= 200.0f))
    {
        result = "the undergrowth brush's radius is 0.5 to 200 metres";
        return false;
    }
    m_undergrowthRadius = radius;
    char text[64];
    std::snprintf(text, sizeof(text), "the undergrowth brush reaches %g m", radius);
    result = text;
    return true;
}

bool EditSession::beginUndergrowthStroke(bool erase, std::string& error)
{
    if (m_undergrowthStroke)
        endUndergrowthStroke();
    if (!loadUndergrowth(error))
        return false;
    if (!(undergrowthScale() > 0.0f))
    {
        error = "no heightmap to lay the undergrowth map on";
        return false;
    }
    m_undergrowthStroke = true;
    m_undergrowthStrokeErase = erase;
    m_undergrowthStrokeStart = m_undergrowth.bytes();
    return true;
}

bool EditSession::undergrowthStrokeTick(float x, float z, std::string& error)
{
    if (!m_undergrowthStroke)
    {
        error = "no undergrowth stroke under way";
        return false;
    }
    m_undergrowth.paintDisc(x, z, m_undergrowthRadius, undergrowthScale(),
                            m_undergrowthStrokeErase ? 0 : static_cast<unsigned char>(m_undergrowthMaterial));
    return true;
}

void EditSession::endUndergrowthStroke()
{
    if (!m_undergrowthStroke)
        return;
    m_undergrowthStroke = false;
    const UndergrowthChange change = m_undergrowth.changeSince(m_undergrowthStrokeStart);
    m_undergrowthStrokeStart.clear();
    if (change.empty())
        return;
    const int id = m_nextUndergrowthChange++;
    m_undergrowthChanges[id] = change;
    HistoryEntry entry;
    char text[160];
    std::snprintf(text, sizeof(text), "%s undergrowth (%dx%d cells)", m_undergrowthStrokeErase ? "Erase" : "Paint",
                  change.columns, change.rows);
    entry.label = text;
    std::snprintf(text, sizeof(text), "EditorUndergrowth.applyChange %d after\n", id);
    entry.doScript = text;
    std::snprintf(text, sizeof(text), "EditorUndergrowth.applyChange %d before\n", id);
    entry.undoScript = text;
    // Made already, stroke by stroke, so the history records it as done.
    m_history.record(entry);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
}

bool EditSession::undergrowthDab(float x, float z, bool erase, std::string& result)
{
    if (!beginUndergrowthStroke(erase, result))
        return false;
    std::string error;
    undergrowthStrokeTick(x, z, error);
    const UndergrowthChange change = m_undergrowth.changeSince(m_undergrowthStrokeStart);
    endUndergrowthStroke();
    char text[160];
    std::snprintf(text, sizeof(text), "%s %d cells of undergrowth around %.1f/%.1f", erase ? "erased" : "painted",
                  change.empty() ? 0 : change.columns * change.rows, x, z);
    result = change.empty() ? std::string("nothing changed (the cells hold that already)") : std::string(text);
    return true;
}

bool EditSession::applyUndergrowthChange(int id, const std::string& state, std::string& error)
{
    std::map<int, UndergrowthChange>::const_iterator found = m_undergrowthChanges.find(id);
    if (found == m_undergrowthChanges.end() || (state != "before" && state != "after"))
    {
        error = "no undergrowth change with that id, or the state is not before or after";
        return false;
    }
    m_undergrowth.apply(found->second, state == "after");
    return true;
}

void EditSession::planUndergrowth(SavePlan& plan)
{
    if (!undergrowthChanged())
        return;
    SavePart& part = plan.part("undergrowth");
    std::string answer;
    if (undergrowthConfigChanged())
    {
        const std::string configPath = m_document->levelDirectory() + "Undergrowth.cfg";
        const std::string text = m_undergrowthConfig.render();
        part.add(configPath, text);
        part.onWritten([this, text]() { m_undergrowthConfigSaved = text; });
        answer = "saved " + configPath;
    }
    if (m_undergrowth.bytes() != m_undergrowthSaved)
    {
        const std::string path = m_document->levelDirectory() + "Undergrowth.raw";
        const std::string bytes = m_undergrowth.bytes();
        part.add(path, bytes);
        part.onWritten([this, bytes]() { m_undergrowthSaved = bytes; });
        answer += (answer.empty() ? "" : "; ") + std::string("saved ") + path;
    }
    part.onReport([answer](const SavePart&) { return answer; });
}

} // namespace bf2editor
