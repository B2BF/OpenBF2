// Roads and splines in the edit session: the module opened with the level (App/RoadNetwork), the retail rows passed
// through, the editor's own EditorRoad.* commands - queries, and edits recorded as the states of the objects they change
// so the history can replay them both ways - and the save. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/LevelDocument.h"
#include "Framework/EditorLog.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SourceControl.h"

#include "Core/SettingsRepostitory.h"
#include "IO/File/FileManager.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>

namespace bf2editor
{

namespace
{

std::string lowered(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

std::string number(int value)
{
    char text[24];
    std::snprintf(text, sizeof(text), "%d", value);
    return text;
}

bool toInt(const std::string& text, int& value)
{
    if (text.empty())
        return false;
    char* end = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (*end)
        return false;
    value = static_cast<int>(parsed);
    return true;
}

bool toFloat(const std::string& text, float& value)
{
    if (text.empty())
        return false;
    char* end = 0;
    const double parsed = std::strtod(text.c_str(), &end);
    if (*end)
        return false;
    value = static_cast<float>(parsed);
    return true;
}

bool toBool(const std::string& text, bool& value)
{
    const std::string t = lowered(text);
    if (t == "1" || t == "true")
        value = true;
    else if (t == "0" || t == "false")
        value = false;
    else
        return false;
    return true;
}

std::string vec2Text(const float v[2])
{
    return formatConsoleFloat(v[0]) + "/" + formatConsoleFloat(v[1]);
}

std::string vec3Text(const float v[3])
{
    return formatConsoleFloat(v[0]) + "/" + formatConsoleFloat(v[1]) + "/" + formatConsoleFloat(v[2]);
}

// The road keys EditorRoad.set takes, with the retail rows' spellings beside the editor's.
std::string roadKey(const std::string& key)
{
    const std::string k = lowered(key);
    if (k == "persistance")
        return "persistence";
    if (k == "prio")
        return "priority";
    if (k.compare(0, 8, "fadedist") == 0)
        return "fade" + k.substr(10);           // fadeDistAtStartLeft -> fadestartleft
    return k;
}

// [CHOSEN] a new spline's terrain snapping: added and moved points take the ground's height.
const int kNewSplineSnapModes = 3;

} // namespace

bool EditSession::openRoads(const std::string& levelName, std::string& report)
{
    m_roadEdits.clear();
    const std::string levelPath =
        (dice::hfe::settings ? dice::hfe::settings->stringGet("GSLevelDirectory", "Levels/") : std::string("Levels/"))
        + levelName + "/";
    return m_roads.open(levelName, levelPath, report);
}

bool EditSession::roadCommand(const std::string& object, const std::string& method, const std::vector<std::string>& args,
                              std::string& result)
{
    result.clear();
    if (_stricmp(object.c_str(), "EditorRoad") == 0)
        return roadEditCommand(method, args, result);
    ConsoleRow row;
    row.object = object;
    row.method = method;
    row.args = args;
    for (size_t i = 0; i < row.args.size(); ++i)
    {
        std::string& a = row.args[i];
        if (a.size() >= 2 && a[0] == '"' && a[a.size() - 1] == '"')
            a = a.substr(1, a.size() - 2);
    }
    return m_roads.retailRow(row, result);
}

// ---- edits -----------------------------------------------------------------------------------------------------------

bool EditSession::applyRoadChange(const RoadChange& change, bool after, std::string& error)
{
    const bool exists = after ? change.existsAfter : change.existedBefore;
    switch (change.kind)
    {
    case 0:
        return m_roads.applySpline(change.splineId, exists ? (after ? &change.splineAfter : &change.splineBefore) : 0,
                                   error);
    case 1:
        return m_roads.applyTemplate(change.name,
                                     exists ? (after ? &change.templateAfter : &change.templateBefore) : 0, error);
    default:
        return m_roads.applyRoad(change.name, exists ? (after ? &change.roadAfter : &change.roadBefore) : 0, error);
    }
}

bool EditSession::applyRoadEdit(int id, const std::string& state, std::string& error)
{
    const std::map<int, RoadEdit>::const_iterator found = m_roadEdits.find(id);
    if (found == m_roadEdits.end() || (state != "before" && state != "after"))
    {
        error = "no such road edit (EditorRoad.applyEdit <id> <before|after>)";
        return false;
    }
    const std::vector<RoadChange>& changes = found->second.changes;
    if (state == "after")
    {
        for (size_t i = 0; i < changes.size(); ++i)
            if (!applyRoadChange(changes[i], true, error))
                return false;
    }
    else
    {
        for (size_t i = changes.size(); i-- > 0;)
            if (!applyRoadChange(changes[i], false, error))
                return false;
    }
    return true;
}

bool EditSession::recordRoadEdit(const RoadEdit& edit, const std::string& label, std::string& result)
{
    const int id = m_nextRoadEdit++;
    m_roadEdits[id] = edit;
    HistoryEntry entry;
    entry.label = label;
    char line[80];
    std::snprintf(line, sizeof(line), "EditorRoad.applyEdit %d after\n", id);
    entry.doScript = line;
    std::snprintf(line, sizeof(line), "EditorRoad.applyEdit %d before\n", id);
    entry.undoScript = line;
    if (!m_history.execute(entry, result))
    {
        m_roadEdits.erase(id);
        return false;
    }
    logLine(LogLevel::Info, "edit: %s", label.c_str());
    result.clear();
    return true;
}

bool EditSession::roadEditCommand(const std::string& method, const std::vector<std::string>& args, std::string& result)
{
    const std::string m = lowered(method);
    const auto arg = [&args](size_t i) { return i < args.size() ? args[i] : std::string(); };
    if (m == "applyedit")
    {
        int id = 0;
        if (!toInt(arg(0), id))
        {
            result = "EditorRoad.applyEdit <id> <before|after>";
            return false;
        }
        return applyRoadEdit(id, arg(1), result);
    }
    if (!m_roads.isOpen())
    {
        result = "the level's roads are not open (they need the editor's RendDX9 image and an open level)";
        return false;
    }

    // ---- queries ----
    if (m == "info")
    {
        result = m_roads.describe();
        return true;
    }
    if (m == "splines")
    {
        const std::vector<int> ids = m_roads.splineIds();
        for (size_t i = 0; i < ids.size(); ++i)
        {
            SplineRecord s;
            m_roads.splineRecord(ids[i], s);
            const std::string road = m_roads.roadOnSpline(ids[i]);
            result += (i ? "\n" : "") + number(ids[i]) + ": " + number(static_cast<int>(s.points.size()))
                    + " points, snap " + number(s.snapModes) + ", width " + formatConsoleFloat(s.lineWidth) + ", step "
                    + formatConsoleFloat(s.stepSize) + (road.empty() ? std::string() : ", road " + road);
        }
        if (ids.empty())
            result = "no splines";
        return true;
    }
    if (m == "roads")
    {
        const std::vector<std::string> names = m_roads.roadNames();
        for (size_t i = 0; i < names.size(); ++i)
        {
            RoadRecord r;
            m_roads.roadRecord(names[i], r);
            result += (i ? "\n" : "") + names[i] + ": spline " + number(r.splineId) + ", template " + r.templateName
                    + (m_roads.ghost(names[i]) ? ", ghost" : ", final");
        }
        if (names.empty())
            result = "no roads";
        return true;
    }
    if (m == "templates")
    {
        const std::vector<std::string> names = m_roads.templateNames();
        for (size_t i = 0; i < names.size(); ++i)
        {
            const size_t users = m_roads.roadsWithTemplate(names[i]).size();
            result += (i ? "\n" : "") + names[i] + (users ? " (" + number(static_cast<int>(users)) + " roads)" : "");
        }
        if (names.empty())
            result = "no road templates";
        return true;
    }
    if (m == "spline")
    {
        int id = 0;
        SplineRecord s;
        if (!toInt(arg(0), id) || !m_roads.splineRecord(id, s))
        {
            result = "no spline " + arg(0);
            return false;
        }
        result = "spline " + number(id) + ": snap " + number(s.snapModes) + ", width " + formatConsoleFloat(s.lineWidth)
               + ", step " + formatConsoleFloat(s.stepSize);
        for (size_t p = 0; p < s.points.size(); ++p)
        {
            const SplinePointRecord& point = s.points[p];
            result += "\n" + number(static_cast<int>(p)) + ": " + vec3Text(point.position);
            if (point.speed != 60.0f || point.delay != 0.0f || point.direction || !point.sound.empty())
                result += " speed " + formatConsoleFloat(point.speed) + " delay " + formatConsoleFloat(point.delay)
                        + " direction " + number(point.direction) + (point.sound.empty() ? "" : " sound " + point.sound);
        }
        return true;
    }
    if (m == "road")
    {
        RoadRecord r;
        if (!m_roads.roadRecord(arg(0), r))
        {
            result = "no road " + arg(0);
            return false;
        }
        result = r.name + ": spline " + number(r.splineId) + ", template " + r.templateName + ", seed " + number(r.seed)
               + ", textureScaleOffset " + vec2Text(r.textureScaleOffset) + ", persistence "
               + formatConsoleFloat(r.persistence) + ", range " + vec2Text(r.range) + ", octaves " + number(r.octaves)
               + ", priority " + number(r.priority) + ", fades " + vec2Text(r.fadeStartLeft) + " "
               + vec2Text(r.fadeStartRight) + " " + vec2Text(r.fadeEndLeft) + " " + vec2Text(r.fadeEndRight)
               + (m_roads.ghost(r.name) ? ", ghost" : ", final");
        return true;
    }
    if (m == "template")
    {
        RoadTemplateRecord t;
        if (!m_roads.templateRecord(arg(0), t))
        {
            result = "no road template " + arg(0);
            return false;
        }
        result = roadTemplateConText(t);
        return true;
    }

    // ---- view state and saving (not edits) ----
    if (m == "tool")
    {
        bool on = false;
        if (!toBool(arg(0), on))
        {
            result = "EditorRoad.tool <0|1>";
            return false;
        }
        return setRoadTool(on, result);
    }
    if (m == "activespline")
    {
        int id = -1, point = -1;
        if (!toInt(arg(0), id) || (!arg(1).empty() && !toInt(arg(1), point)) || (id >= 0 && !m_roads.hasSpline(id)))
        {
            result = "EditorRoad.activeSpline <id|-1> [<point>]: no spline " + arg(0);
            return false;
        }
        setActiveSpline(id, point);
        result = id < 0 ? "no active spline" : "spline " + number(id) + " is the active spline";
        return true;
    }
    if (m == "showsplines")
    {
        bool on = false;
        if (!toBool(arg(0), on))
        {
            result = "EditorRoad.showSplines <0|1>";
            return false;
        }
        setSplinesShown(on);
        result = on ? "the splines are drawn" : "the splines are hidden";
        return true;
    }
    if (m == "build")
    {
        const std::string mode = lowered(arg(1));
        if (mode != "final" && mode != "ghost")
        {
            result = "EditorRoad.build <road|*> <final|ghost>";
            return false;
        }
        const std::vector<std::string> names =
            arg(0) == "*" ? m_roads.roadNames() : std::vector<std::string>(1, arg(0));
        for (size_t i = 0; i < names.size(); ++i)
            if (!m_roads.setGhost(names[i], mode == "ghost", result))
                return false;
        logLine(LogLevel::Info, "roads: %u built %s", static_cast<unsigned>(names.size()), mode.c_str());
        return true;
    }
    if (m == "apply")
    {
        // The heights it sets, recorded as one terrain change (the history's EditorTerrain.applyChange).
        if (!m_roads.hasRoad(arg(0)))
        {
            result = "no road " + arg(0);
            return false;
        }
        std::string error;
        if (!m_terrain.beginStroke(error))
        {
            result = error;
            return false;
        }
        const bool applied = m_roads.projectToTerrain(arg(0), error);
        TerrainChange change;
        m_terrain.endStroke(change);
        if (!applied)
        {
            result = error;
            return false;
        }
        // The heights that moved (the change holds its rectangles whole), and where they landed.
        unsigned moved = 0;
        std::vector<float> landed;
        const float scale = m_terrain.verticalScale();
        for (size_t t = 0; t < change.tiles.size(); ++t)
        {
            const TerrainTileChange& tile = change.tiles[t];
            const size_t bytes = static_cast<size_t>(tile.sampleBytes);
            for (size_t i = 0; bytes && i + bytes <= tile.before.size() && i + bytes <= tile.after.size(); i += bytes)
            {
                if (std::memcmp(&tile.before[i], &tile.after[i], bytes) == 0)
                    continue;
                ++moved;
                const unsigned raw = bytes == 2 ? tile.after[i] | (static_cast<unsigned>(tile.after[i + 1]) << 8)
                                                : tile.after[i];
                landed.push_back(static_cast<float>(raw) * scale);
            }
        }
        // [CHOSEN, not recovered] a projection that put every height on one level while the road rises is refused and
        // undone: the renderer drew it with a state that masked the height channel (RoadEditable::projectToTerrain).
        RoadRecord road;
        SplineRecord spline;
        if (moved && m_roads.roadRecord(arg(0), road) && m_roads.splineRecord(road.splineId, spline)
            && roadProjectionCameBackFlat(spline, landed))
        {
            std::string undone;
            m_terrain.restore(change, false, undone);
            char text[256];
            std::snprintf(text, sizeof(text), "road %s NOT applied: the projection put all %u heights it changed at one "
                          "level (%.3f m) while the road rises; the terrain is as it was - apply again", arg(0).c_str(),
                          moved, landed.front());
            result = text;
            logLine(LogLevel::Error, "roads: %s", result.c_str());
            return false;
        }
        char label[160];
        std::snprintf(label, sizeof(label), "Apply road %s to the terrain (%u heights changed)", arg(0).c_str(), moved);
        if (moved)
            recordTerrainChange(change, label);
        result = moved ? std::string(label) : "road " + arg(0) + " changed no height";
        logLine(LogLevel::Info, "roads: %s", result.c_str());
        return true;
    }
    if (m == "save")
    {
        if (!saveRoads(true, result))
            return false;
        if (result.empty())
            result = "roads: nothing to save (the level has no editable roads)";
        logLine(LogLevel::Info, "%s", result.c_str());
        return true;
    }

    if (m == "splinecopy")
    {
        // The retail copy: the spline remembered, the next paste 1 m to the side.
        int id = activeSpline();
        if ((!arg(0).empty() && !toInt(arg(0), id)) || !m_roads.hasSpline(id))
        {
            result = "EditorRoad.splineCopy [<id>]: no spline " + (arg(0).empty() ? std::string("is active") : arg(0));
            return false;
        }
        m_copiedSpline = id;
        m_splinePasteOffset = 1.0f;
        result = "spline " + number(id) + " copied";
        return true;
    }
    if (m == "splinemark")
    {
        // The retail "select for merge": a spline and the point it joins at (its selected point).
        int id = activeSpline(), point = activeSplinePoint();
        SplineRecord marked;
        if ((!arg(0).empty() && !toInt(arg(0), id)) || (!arg(1).empty() && !toInt(arg(1), point))
            || !m_roads.splineRecord(id, marked) || point < 0 || point >= static_cast<int>(marked.points.size()))
        {
            result = "EditorRoad.splineMark [<id> <point>]: a spline and one of its points (the active spline and its "
                     "selected point when none are named)";
            return false;
        }
        m_markedSpline = id;
        m_markedSplinePoint = point;
        result = "spline " + number(id) + " marked for a merge at point " + number(point);
        return true;
    }
    if (m == "activetemplate")
    {
        if (!m_roads.hasTemplate(arg(0)))
        {
            result = "EditorRoad.activeTemplate <template>: no road template " + arg(0);
            return false;
        }
        m_activeRoadTemplate = arg(0);
        result = "road template " + arg(0) + " is the active template";
        return true;
    }
    if (m == "refresh")
    {
        char text[96];
        std::snprintf(text, sizeof(text), "%d roads rebuilt from their templates", m_roads.refreshRoads());
        result = text;
        logLine(LogLevel::Info, "roads: %s", result.c_str());
        return true;
    }
    if (m == "cleanfolder")
    {
        bool move = false;
        if (!arg(0).empty() && !toBool(arg(0), move))
        {
            result = "EditorRoad.cleanFolder [0|1]";
            return false;
        }
        return cleanRoadFolder(move, result);
    }
    if (m == "connect")
    {
        // The retail Connect: a road named by the user on the active spline, with the active template.
        const int spline = activeSpline();
        const std::string templateName = arg(1).empty() ? m_activeRoadTemplate : arg(1);
        if (spline < 0)
        {
            result = "No active spline!";
            return false;
        }
        if (templateName.empty() || !m_roads.hasTemplate(templateName))
        {
            result = "No active RoadTemplate! [" + templateName + "]";
            return false;
        }
        std::vector<std::string> createArgs;
        createArgs.push_back(arg(0));
        createArgs.push_back(number(spline));
        createArgs.push_back(templateName);
        return roadEditCommand("create", createArgs, result);
    }
    if (m == "disconnect")
    {
        // The retail Disconnect: the active spline's road removed.
        const int spline = activeSpline();
        if (spline < 0)
        {
            result = "No active spline!";
            return false;
        }
        const std::string road = m_roads.roadOnSpline(spline);
        if (road.empty())
        {
            result = "Spline has no road!";
            return false;
        }
        return roadEditCommand("delete", std::vector<std::string>(1, road), result);
    }

    // ---- edits ----
    RoadEdit edit;
    RoadChange change;
    std::string label;
    if (m == "splinepaste")
    {
        // The retail paste: the copied spline's offset copy (the renderer's, 1 m to the side and 1 m more each paste)
        // made a new spline and the active one.
        if (!m_roads.hasSpline(m_copiedSpline))
        {
            result = "no spline is copied (EditorRoad.splineCopy)";
            return false;
        }
        SplineRecord pasted;
        if (!m_roads.offsetSplineRecord(m_copiedSpline, m_splinePasteOffset, pasted, result))
            return false;
        const int id = m_roads.nextSplineId();
        pasted.id = id;
        change.kind = 0;
        change.splineId = id;
        change.existsAfter = true;
        change.splineAfter = pasted;
        edit.changes.push_back(change);
        char text[128];
        std::snprintf(text, sizeof(text), "Paste spline %d (spline %d, %s m to the side)", id, m_copiedSpline,
                      formatConsoleFloat(m_splinePasteOffset).c_str());
        if (!recordRoadEdit(edit, text, result))
            return false;
        m_splinePasteOffset += 1.0f;
        setActiveSpline(id, -1);
        result = text;
        return true;
    }
    if (m == "splinemerge")
    {
        // The retail merge (EditorEngine 0x100B0B10) of the marked spline and the active one, each at its point: when the
        // active spline's point is not its first, the marked spline's points follow its last - in order from a marked
        // first point, reversed otherwise - and the marked spline goes; when it is the first, the active spline's points
        // follow the marked spline's last and the active spline goes. The kept spline becomes the active one.
        int id = activeSpline(), point = activeSplinePoint();
        if ((!arg(0).empty() && !toInt(arg(0), id)) || (!arg(1).empty() && !toInt(arg(1), point)))
        {
            result = "EditorRoad.splineMerge [<id> <point>]";
            return false;
        }
        const int marked = markedSpline();
        SplineRecord active, other;
        if (marked < 0 || !m_roads.splineRecord(marked, other))
        {
            result = "no spline is marked for a merge (EditorRoad.splineMark)";
            return false;
        }
        if (!m_roads.splineRecord(id, active) || point < 0 || point >= static_cast<int>(active.points.size()))
        {
            result = "EditorRoad.splineMerge [<id> <point>]: a spline and one of its points to merge the marked one into";
            return false;
        }
        if (id == marked)
        {
            result = "the marked spline is the one to merge into";
            return false;
        }
        const bool keepActive = point != 0;
        const SplineRecord& kept = keepActive ? active : other;
        const SplineRecord& gone = keepActive ? other : active;
        // [CHOSEN, not recovered] a spline carrying a road is not merged away (retail left its road without a spline).
        const std::string goneRoad = m_roads.roadOnSpline(gone.id);
        if (!goneRoad.empty())
        {
            result = "spline " + number(gone.id) + " carries road " + goneRoad + ": disconnect it first";
            return false;
        }
        SplineRecord merged = kept;
        const bool reversed = keepActive && m_markedSplinePoint != 0;
        for (size_t i = 0; i < gone.points.size(); ++i)
            merged.points.push_back(gone.points[reversed ? gone.points.size() - 1 - i : i]);
        RoadChange keptChange;
        keptChange.kind = 0;
        keptChange.splineId = kept.id;
        keptChange.existedBefore = keptChange.existsAfter = true;
        keptChange.splineBefore = kept;
        keptChange.splineAfter = merged;
        RoadChange goneChange;
        goneChange.kind = 0;
        goneChange.splineId = gone.id;
        goneChange.existedBefore = true;
        goneChange.splineBefore = gone;
        edit.changes.push_back(keptChange);
        edit.changes.push_back(goneChange);
        const std::string text = "Merge spline " + number(gone.id) + " into spline " + number(kept.id);
        if (!recordRoadEdit(edit, text, result))
            return false;
        m_markedSpline = -1;
        m_markedSplinePoint = -1;
        setActiveSpline(kept.id, -1);
        result = text + " (" + number(static_cast<int>(merged.points.size())) + " points)";
        return true;
    }
    if (m == "splinecreate")
    {
        int id = m_roads.nextSplineId();
        if (!arg(0).empty() && !toInt(arg(0), id))
        {
            result = "EditorRoad.splineCreate [<id>]";
            return false;
        }
        if (id < 0 || m_roads.hasSpline(id))
        {
            result = "spline " + number(id) + " exists";
            return false;
        }
        change.kind = 0;
        change.splineId = id;
        change.existsAfter = true;
        change.splineAfter.id = id;
        change.splineAfter.snapModes = kNewSplineSnapModes;
        edit.changes.push_back(change);
        label = "Create spline " + number(id);
        if (!recordRoadEdit(edit, label, result))
            return false;
        setActiveSpline(id, -1);
        return true;
    }
    else if (m == "splineadd" || m == "splineinsert" || m == "splinemove" || m == "splineremovepoint"
             || m == "splineset" || m == "splinepoint" || m == "splinedelete")
    {
        int id = 0;
        SplineRecord before;
        if (!toInt(arg(0), id) || !m_roads.splineRecord(id, before))
        {
            result = "no spline " + arg(0);
            return false;
        }
        change.kind = 0;
        change.splineId = id;
        change.existedBefore = change.existsAfter = true;
        change.splineBefore = before;
        SplineRecord after = before;
        const int count = static_cast<int>(before.points.size());
        int index = count;
        float position[3];
        if (m == "splineadd" || m == "splineinsert" || m == "splinemove")
        {
            const std::string where = m == "splineadd" ? arg(1) : arg(2);
            if ((m != "splineadd" && !toInt(arg(1), index)) || !parseVec3(where, position)
                || index < 0 || index > (m == "splinemove" ? count - 1 : count))
            {
                result = m == "splineadd" ? "EditorRoad.splineAdd <id> <x/y/z>"
                                          : "EditorRoad." + method + " <id> <index> <x/y/z> (index 0.." +
                                                number(m == "splinemove" ? count - 1 : count) + ")";
                return false;
            }
            // The renderer snaps points it is given only when told to; the record holds where the point ends up.
            float ground = 0.0f;
            const int snapBit = m == "splinemove" ? 2 : 1;
            if ((before.snapModes & snapBit) && groundHeight(position[0], position[2], ground))
                position[1] = ground;
            SplinePointRecord point;
            if (m == "splinemove")
                point = before.points[static_cast<size_t>(index)];
            point.position[0] = position[0];
            point.position[1] = position[1];
            point.position[2] = position[2];
            if (m == "splinemove")
                after.points[static_cast<size_t>(index)] = point;
            else
                after.points.insert(after.points.begin() + index, point);
            label = (m == "splinemove" ? "Move point " : "Add point ") + number(index) + " of spline " + number(id);
        }
        else if (m == "splineremovepoint")
        {
            if (!toInt(arg(1), index) || index < 0 || index >= count)
            {
                result = "EditorRoad.splineRemovePoint <id> <index> (index 0.." + number(count - 1) + ")";
                return false;
            }
            after.points.erase(after.points.begin() + index);
            label = "Remove point " + number(index) + " of spline " + number(id);
        }
        else if (m == "splineset")
        {
            const std::string key = lowered(arg(1));
            int i = 0;
            float f = 0.0f;
            if (key == "snapmodes" && toInt(arg(2), i))
                after.snapModes = i;
            else if (key == "linewidth" && toFloat(arg(2), f))
                after.lineWidth = f;
            else if (key == "stepsize" && toFloat(arg(2), f) && f > 0.0f)
                after.stepSize = f;
            else
            {
                result = "EditorRoad.splineSet <id> <snapModes|lineWidth|stepSize> <value>";
                return false;
            }
            label = "Set " + arg(1) + " of spline " + number(id);
        }
        else if (m == "splinepoint")
        {
            const std::string key = lowered(arg(2));
            int i = 0;
            float f = 0.0f;
            if (!toInt(arg(1), index) || index < 0 || index >= count)
            {
                result = "EditorRoad.splinePoint <id> <index> <speed|delay|direction|sound> <value>";
                return false;
            }
            SplinePointRecord& point = after.points[static_cast<size_t>(index)];
            if (key == "speed" && toFloat(arg(3), f))
                point.speed = f;
            else if (key == "delay" && toFloat(arg(3), f))
                point.delay = f;
            else if (key == "direction" && toInt(arg(3), i))
                point.direction = i;
            else if (key == "sound")
                point.sound = arg(3);
            else
            {
                result = "EditorRoad.splinePoint <id> <index> <speed|delay|direction|sound> <value>";
                return false;
            }
            label = "Set " + arg(2) + " of point " + number(index) + " of spline " + number(id);
        }
        else    // splinedelete: its road first
        {
            const std::string road = m_roads.roadOnSpline(id);
            if (!road.empty())
            {
                RoadChange roadChange;
                roadChange.kind = 2;
                roadChange.name = road;
                roadChange.existedBefore = true;
                m_roads.roadRecord(road, roadChange.roadBefore);
                edit.changes.push_back(roadChange);
            }
            change.existsAfter = false;
            label = "Delete spline " + number(id) + (road.empty() ? std::string() : " and road " + road);
        }
        change.splineAfter = after;
        edit.changes.push_back(change);
    }
    else if (m == "templatecreate")
    {
        const std::string name = arg(0);
        if (name.empty() || name.find_first_of(" \t\"\\/") != std::string::npos || m_roads.hasTemplate(name))
        {
            result = name.empty() ? "EditorRoad.templateCreate <name> [<copied from>]"
                                  : "a road template needs a new name without blanks, quotes or slashes";
            return false;
        }
        change.kind = 1;
        change.name = name;
        change.existsAfter = true;
        if (!arg(1).empty() && !m_roads.templateRecord(arg(1), change.templateAfter))
        {
            result = "no road template " + arg(1);
            return false;
        }
        change.templateAfter.name = name;
        edit.changes.push_back(change);
        label = "Create road template " + name;
    }
    else if (m == "templateset")
    {
        RoadTemplateRecord before;
        if (!m_roads.templateRecord(arg(0), before))
        {
            result = "no road template " + arg(0);
            return false;
        }
        RoadTemplateRecord after = before;
        const std::string key = lowered(arg(1));
        const std::string value = arg(2);
        float f = 0.0f, v[2];
        int i = 0;
        bool b = false;
        if (key == "width" && toFloat(value, f))
            after.width = f;
        else if (key == "maxtesselatederror" && toFloat(value, f))
            after.maxTesselatedError = f;
        else if (key == "blendfactor" && toFloat(value, f))
            after.blendFactor = f;
        else if ((key == "priority" || key == "prio") && toInt(value, i))
            after.priority = i;
        else if (key == "material")
            after.material = value;
        else if (key == "fadeoffdistance" && parseVec2(value, v))
        {
            after.fadeOffDistance[0] = v[0];
            after.fadeOffDistance[1] = v[1];
        }
        else if ((key == "texture0" || key == "texture1"))
            after.textures[key[7] - '0'] = value;
        else if ((key == "mirror0" || key == "mirror1") && toBool(value, b))
            after.mirror[key[6] - '0'] = b;
        else if ((key == "scale0" || key == "scale1") && parseVec2(value, v))
        {
            after.scale[key[5] - '0'][0] = v[0];
            after.scale[key[5] - '0'][1] = v[1];
        }
        else
        {
            result = "EditorRoad.templateSet <name> <width|maxTesselatedError|blendFactor|priority|material|"
                     "fadeOffDistance|texture0|texture1|mirror0|mirror1|scale0|scale1> <value>";
            return false;
        }
        change.kind = 1;
        change.name = before.name;
        change.existedBefore = change.existsAfter = true;
        change.templateBefore = before;
        change.templateAfter = after;
        edit.changes.push_back(change);
        label = "Set " + arg(1) + " of road template " + before.name;
    }
    else if (m == "templatedelete")
    {
        RoadTemplateRecord before;
        if (!m_roads.templateRecord(arg(0), before))
        {
            result = "no road template " + arg(0);
            return false;
        }
        const std::vector<std::string> users = m_roads.roadsWithTemplate(before.name);
        if (!users.empty())
        {
            result = "road template " + before.name + " carries road " + users[0];
            return false;
        }
        change.kind = 1;
        change.name = before.name;
        change.existedBefore = true;
        change.templateBefore = before;
        edit.changes.push_back(change);
        label = "Delete road template " + before.name;
    }
    else if (m == "create")
    {
        RoadRecord after;
        after.name = arg(0);
        int seed = static_cast<int>(GetTickCount() & 0x7FFFFFFF);
        if (after.name.empty() || after.name.find_first_of(" \t\"\\/") != std::string::npos
            || !toInt(arg(1), after.splineId) || arg(2).empty() || (!arg(3).empty() && !toInt(arg(3), seed)))
        {
            result = "EditorRoad.create <name> <spline id> <template> [<noise seed>] (a name without blanks, quotes or "
                     "slashes)";
            return false;
        }
        if (m_roads.hasRoad(after.name))
        {
            result = "Road: [" + after.name + "] already exists!";
            return false;
        }
        after.templateName = arg(2);
        after.seed = seed;
        change.kind = 2;
        change.name = after.name;
        change.existsAfter = true;
        change.roadAfter = after;
        edit.changes.push_back(change);
        label = "Create road " + after.name;
    }
    else if (m == "set" || m == "delete" || m == "rename")
    {
        RoadRecord before;
        if (!m_roads.roadRecord(arg(0), before))
        {
            result = "no road " + arg(0);
            return false;
        }
        change.kind = 2;
        change.name = before.name;
        change.existedBefore = true;
        change.roadBefore = before;
        if (m == "delete")
        {
            edit.changes.push_back(change);
            label = "Delete road " + before.name;
        }
        else if (m == "rename")
        {
            const std::string name = arg(1);
            if (name.empty() || name.find_first_of(" \t\"\\/") != std::string::npos || m_roads.hasRoad(name))
            {
                result = "EditorRoad.rename <road> <new name>: a new name without blanks, quotes or slashes";
                return false;
            }
            edit.changes.push_back(change);                 // the old name goes
            RoadChange added;
            added.kind = 2;
            added.name = name;
            added.existsAfter = true;
            added.roadAfter = before;
            added.roadAfter.name = name;
            edit.changes.push_back(added);                  // and the new one comes
            label = "Rename road " + before.name + " to " + name;
        }
        else
        {
            RoadRecord after = before;
            const std::string key = roadKey(arg(1));
            const std::string value = arg(2);
            float f = 0.0f, v[2];
            int i = 0;
            float* pair = 0;
            if (key == "texturescaleoffset")
                pair = after.textureScaleOffset;
            else if (key == "range")
                pair = after.range;
            else if (key == "fadestartleft")
                pair = after.fadeStartLeft;
            else if (key == "fadestartright")
                pair = after.fadeStartRight;
            else if (key == "fadeendleft")
                pair = after.fadeEndLeft;
            else if (key == "fadeendright")
                pair = after.fadeEndRight;
            if (pair && parseVec2(value, v))
            {
                pair[0] = v[0];
                pair[1] = v[1];
            }
            else if (!pair && key == "persistence" && toFloat(value, f))
                after.persistence = f;
            else if (!pair && key == "octaves" && toInt(value, i))
                after.octaves = i;
            else if (!pair && key == "priority" && toInt(value, i))
                after.priority = i;
            else if (!pair && key == "seed" && toInt(value, i))
                after.seed = i;
            else if (!pair && key == "spline" && toInt(value, i))
                after.splineId = i;
            else if (!pair && key == "template" && !value.empty())
                after.templateName = value;
            else
            {
                result = "EditorRoad.set <road> <textureScaleOffset|persistence|range|octaves|priority|fadeStartLeft|"
                         "fadeStartRight|fadeEndLeft|fadeEndRight|seed|spline|template> <value>";
                return false;
            }
            change.existsAfter = true;
            change.roadAfter = after;
            edit.changes.push_back(change);
            label = "Set " + arg(1) + " of road " + before.name;
        }
    }
    else
    {
        result = "EditorRoad." + method + " is not a road command (docs/editor-console.md)";
        return false;
    }
    return recordRoadEdit(edit, label, result);
}

bool EditSession::setRoadTool(bool active, std::string& result)
{
    if (active && !m_roads.isOpen())
    {
        result = "the level's roads are not open (they need the editor's RendDX9 image and an open level)";
        return false;
    }
    // One tool has the mouse at a time.
    if (active && m_terrainTool)
        setTerrainToolActive(false);
    if (active && m_brushTool)
        setBrushToolActive(false);
    if (active && m_undergrowthTool)
        setUndergrowthToolActive(false);
    m_roadTool = active;
    result = active ? "the road tool is on: a click on the ground adds a point to the active spline"
                    : "the road tool is off";
    return true;
}

// ---- saving ----------------------------------------------------------------------------------------------------------

void EditSession::planRoads(SavePlan& plan, bool force)
{
    if (!m_roads.isOpen() || (!m_roads.changed() && !(force && m_roads.hasEditableRoads())))
        return;
    SavePart& part = plan.part("roads");
    if (!m_document || !m_document->isOpen())
    {
        part.refuse("roads: the level's files are not open, so nothing can be saved");
        return;
    }
    std::vector<std::pair<std::string, std::string> > levelFiles, templateFiles;
    std::string staged;
    if (!m_roads.stageSave(m_document->levelDirectory(), force, levelFiles, templateFiles, staged))
    {
        part.refuse("roads: " + staged);
        return;
    }
    addStagedFiles(part, levelFiles);
    // The templates go where the engine writes the mod's files (FileManager::getWritePath: its loose folder).
    const auto key = [](const std::string& path) {
        std::string k = fullSavePath(path);
        for (size_t c = 0; c < k.size(); ++c)
            k[c] = static_cast<char>(std::tolower(static_cast<unsigned char>(k[c])));
        return k;
    };
    std::set<std::string> templateTargets;
    for (size_t i = 0; i < templateFiles.size(); ++i)
    {
        std::string target;
        dice::hfe::io::fileManager->getWritePath(templateFiles[i].second, target);
        part.addStaged(target, templateFiles[i].first);
        templateTargets.insert(key(target));
    }
    part.onWritten([this]() { m_roads.markSaved(); });
    part.onReport([staged, templateTargets, key](const SavePart& written) {
        size_t templates = 0;
        for (size_t i = 0; i < written.files().size(); ++i)
            templates += templateTargets.count(key(written.files()[i].target));
        return staged + "; " + number(static_cast<int>(written.changed() - templates)) + " level files and "
               + number(static_cast<int>(templates)) + " template files written";
    });
}

bool EditSession::saveRoads(bool force, std::string& result)
{
    SavePlan plan;
    planRoads(plan, force);
    plan.dropUnchanged();
    return writeSave(plan, "roads", result) && !plan.anyRefused();
}

// The retail Clean Road Folder (LevelEditor 0x1000FA20) opened every Roads/*.mesh no editable road compiles for delete,
// which on a stock level is every road it has. [CHOSEN, not recovered] a mesh CompiledRoads.con loads is kept too, and
// unused meshes are reported, or moved into Roads/Unused - never deleted.
bool EditSession::cleanRoadFolder(bool move, std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level is open";
        return false;
    }
    const std::string folder = m_document->levelDirectory() + "Roads";
    std::vector<std::string> meshes;
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((folder + "/*.mesh").c_str(), &found);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                meshes.push_back(found.cFileName);
        } while (FindNextFileA(h, &found));
        FindClose(h);
    }
    std::set<std::string> used;
    const std::vector<std::string> roads = m_roads.isOpen() ? m_roads.roadNames() : std::vector<std::string>();
    for (size_t i = 0; i < roads.size(); ++i)
        used.insert(lowered(roads[i] + "_compiled.mesh"));
    std::string compiled, error;
    if (ObjectLibrary::readTextFile(m_document->levelDirectory() + "CompiledRoads.con", compiled, error))
    {
        const CompiledRoadsFile file = parseCompiledRoads(compiled);
        for (size_t i = 0; i < file.blocks.size(); ++i)
        {
            const std::string& mesh = file.blocks[i].mesh;
            used.insert(lowered(mesh.substr(mesh.find_last_of("/\\") == std::string::npos ? 0
                                                                                           : mesh.find_last_of("/\\") + 1)));
        }
    }
    std::vector<std::string> unused;
    for (size_t i = 0; i < meshes.size(); ++i)
        if (!used.count(lowered(meshes[i])))
            unused.push_back(meshes[i]);
    result = number(static_cast<int>(meshes.size())) + " road meshes in " + folder + ", "
           + number(static_cast<int>(unused.size())) + " used by no road";
    for (size_t i = 0; i < unused.size(); ++i)
        result += (i ? ", " : ": ") + unused[i];
    if (move && !unused.empty())
    {
        const std::string target = folder + "/Unused";
        CreateDirectoryA(target.c_str(), 0);
        int moved = 0;
        for (size_t i = 0; i < unused.size(); ++i)
            if (MoveFileExA((folder + "/" + unused[i]).c_str(), (target + "/" + unused[i]).c_str(), MOVEFILE_REPLACE_EXISTING))
                ++moved;
        result += "; " + number(moved) + " moved into Roads/Unused";
    }
    else if (!unused.empty())
        result += " (reported only; EditorRoad.cleanFolder 1 moves them into Roads/Unused)";
    logLine(LogLevel::Info, "roads: %s", result.c_str());
    return true;
}

} // namespace bf2editor
