// Level tools for repeated-offset copies, terrain measurement, reload and height-scale adjustment.
// Reload recreates objects from in-memory templates and preserves matrices/history ID resolution [CHOSEN].
// Unused lightmaps are reported and only moved to Unused when requested [CHOSEN]; they are never deleted.
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/EditorLevel.h"
#include "App/LevelDocument.h"
#include "Framework/EditorLog.h"
#include "Framework/LightmapAtlas.h"
#include "Framework/ObjectLibrary.h"

#include "Game/Objects/IObject.h"
#include "Core/Ids.h"
#include "Game/Objects/IObjectTemplate.h"
#include "Scene/Object/ObjectManager.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>

using dice::hfe::Mat4;
using dice::hfe::world::IObject;

namespace bf2editor
{

// E3: last-offset copy

bool EditSession::lastOffsetCopy(std::string& result)
{
    if (m_selection.size() != 2)
    {
        result = "select exactly two objects of one template (the first, then the second)";
        return false;
    }
    // The selection keeps the order the objects were added in: A first, B second.
    const unsigned a = m_selection.ids()[0], b = m_selection.ids()[1];
    IObject* first = object(a);
    IObject* second = object(b);
    if (!first || !second || !first->getTemplate() || first->getTemplate() != second->getTemplate())
    {
        result = "the two objects must be of one template";
        return false;
    }
    if (!copyable(a, result))
        return false;
    m_lastOffset = copyOf(a);
    for (int k = 0; k < 3; ++k)
        m_lastOffset.transform.m[12 + k] = 0.0f;            // only the rotation and the template are kept
    const Mat4& ma = first->getAbsoluteTransformation();
    const Mat4& mb = second->getAbsoluteTransformation();
    m_lastOffsetStep[0] = mb.m[12] - ma.m[12];
    m_lastOffsetStep[1] = mb.m[14] - ma.m[14];
    m_lastOffsetArmed = true;
    m_clipboard.clear();                                    // retail's record list held only this one entry
    char text[200];
    std::snprintf(text, sizeof(text), "last-offset copy of %s armed: each Paste adds one %.2f / %.2f from the primary",
                  m_lastOffset.templateName.c_str(), m_lastOffsetStep[0], m_lastOffsetStep[1]);
    result = text;
    return true;
}

bool EditSession::pasteLastOffset(std::string& result)
{
    // The base: the primary object, or the point under the view's centre when nothing is selected.
    float base[3];
    Mat4 primary;
    const bool hasPrimary = !m_selection.empty() && transformOf(m_selection.primary(), primary);
    if (hasPrimary)
    {
        base[0] = primary.m[12];
        base[1] = primary.m[13];
        base[2] = primary.m[14];
    }
    else
        pointAtViewCentre(base);
    NewObject item = m_lastOffset;
    item.transform.m[12] = base[0] + m_lastOffsetStep[0];
    item.transform.m[13] = base[1];
    item.transform.m[14] = base[2] + m_lastOffsetStep[1];
    if (hasPrimary && object(m_selection.primary()))
        item.layer = static_cast<int>(object(m_selection.primary())->getLayer());
    std::vector<NewObject> objects(1, item);
    std::vector<unsigned> made;
    std::string error;
    if (!makeObjects(objects, made, error) || made.empty())
    {
        result = "pasting the last-offset copy: " + error;
        return false;
    }
    // Always set down on the ground, as retail's paste forced the ground snap.
    IObject* o = object(made[0]);
    float ground = 0.0f;
    if (o && m_level.groundHeight(item.transform.m[12], item.transform.m[14], ground))
    {
        Mat4 m = o->getAbsoluteTransformation();
        m.m[13] = snapPivotHeight(o, ground);
        setTransformNow(made[0], m, error);
    }
    HistoryEntry entry;
    madeScripts(made, entry.doScript, entry.undoScript);
    entry.label = "Paste " + describe(made[0]) + " at the last offset";
    m_history.record(entry);
    // Armed again from the old primary and the new object: the same step, the primary's rotation.
    if (hasPrimary)
    {
        const NewObject next = copyOf(m_selection.primary());
        if (!next.templateName.empty())
        {
            m_lastOffset = next;
            for (int k = 0; k < 3; ++k)
                m_lastOffset.transform.m[12 + k] = 0.0f;
        }
    }
    m_selection.set(made[0]);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

// E1: the measure tool

bool EditSession::setMeasureTool(bool on, std::string& result)
{
    m_measureTool = on;
    result = on ? "measure tool on: click the ground to add points, Backspace takes the last off, Enter gives the length"
                : "measure tool off (its points stay)";
    return true;
}

bool EditSession::measureAdd(float x, float z, std::string& result)
{
    float y = 0.0f;
    if (!m_level.groundHeight(x, z, y))
    {
        result = "no terrain there";
        return false;
    }
    dice::hfe::Vec3 point;
    point.x = x;
    point.y = y;
    point.z = z;
    m_measurePoints.push_back(point);
    char text[160];
    std::snprintf(text, sizeof(text), "point %u at %.2f / %.2f / %.2f; %.3f m so far", static_cast<unsigned>(m_measurePoints.size()),
                  x, y, z, measureLength());
    result = text;
    return true;
}

bool EditSession::measureUndo(std::string& result)
{
    if (m_measurePoints.empty())
    {
        result = "no measure point to take off";
        return false;
    }
    m_measurePoints.pop_back();
    char text[96];
    std::snprintf(text, sizeof(text), "%u points; %.3f m so far", static_cast<unsigned>(m_measurePoints.size()), measureLength());
    result = text;
    return true;
}

bool EditSession::measureFinish(std::string& result)
{
    char text[64];
    std::snprintf(text, sizeof(text), "Length: %f", measureLength());     // the retail line's format
    result = text;
    logLine(LogLevel::Info, "measure: %s (%u points)", text, static_cast<unsigned>(m_measurePoints.size()));
    m_measurePoints.clear();
    return true;
}

float EditSession::measureLength() const
{
    double length = 0.0;
    for (size_t i = 1; i < m_measurePoints.size(); ++i)
    {
        const double dx = m_measurePoints[i].x - m_measurePoints[i - 1].x;
        const double dy = m_measurePoints[i].y - m_measurePoints[i - 1].y;
        const double dz = m_measurePoints[i].z - m_measurePoints[i - 1].z;
        length += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return static_cast<float>(length);
}

// E4: reload objects

bool EditSession::reloadObjects(bool selectedOnly, std::string& result)
{
    if (!m_document || !m_document->isOpen() || !dice::hfe::world::objectManager)
    {
        result = "no level is open";
        return false;
    }
    std::set<const dice::hfe::world::IObjectTemplate*> templates;
    if (selectedOnly)
    {
        for (size_t i = 0; i < m_selection.ids().size(); ++i)
        {
            IObject* o = object(m_selection.ids()[i]);
            if (o && o->getTemplate())
                templates.insert(o->getTemplate());
        }
        if (templates.empty())
        {
            result = "nothing is selected";
            return false;
        }
    }
    struct Remade
    {
        unsigned                 before;
        unsigned                 after;
        LevelDocument::RecordRef record;
    };
    std::vector<Remade> remade;
    const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
    {
        Remade r;
        r.before = it->first;
        r.after = 0;
        if (!it->second || !it->second->getTemplate() || !m_document->recordOf(it->first, r.record))
            continue;
        if (selectedOnly && !templates.count(it->second->getTemplate()))
            continue;
        remade.push_back(r);
    }
    if (remade.empty())
    {
        result = "no object of the level's files to reload";
        return false;
    }
    // All new objects first, then the old ones go (as the rename makes objects again).
    for (size_t i = 0; i < remade.size(); ++i)
    {
        IObject* old = object(remade[i].before);
        const Mat4 transform = old->getAbsoluteTransformation();
        const ObjectProperties properties = propertiesOf(old);
        std::string why;
        if (!makeObject(old->getTemplate()->getName(), transform, static_cast<int>(old->getLayer()), remade[i].after, why)
            || !runConsoleScript(propertiesScript(remade[i].after, properties), why))
        {
            for (size_t k = 0; k <= i; ++k)
            {
                IObject* extra = remade[k].after ? object(remade[k].after) : 0;
                if (extra)
                    extra->forceDestroy();
            }
            dice::hfe::world::objectManager->destroyObjects();
            result = "making " + describe(remade[i].before) + " again: " + why;
            return false;
        }
    }
    for (size_t i = 0; i < remade.size(); ++i)
    {
        IObject* old = object(remade[i].before);
        if (old)
            old->forceDestroy();
    }
    dice::hfe::world::objectManager->destroyObjects();
    for (size_t i = 0; i < remade.size(); ++i)
    {
        m_remap[remade[i].before] = remade[i].after;
        for (std::map<unsigned, unsigned>::iterator it = m_remap.begin(); it != m_remap.end(); ++it)
            if (it->second == remade[i].before)
                it->second = remade[i].after;
        m_document->rebind(remade[i].record, remade[i].after);
        if (m_selection.contains(remade[i].before))
        {
            m_selection.remove(remade[i].before);
            m_selection.add(remade[i].after);
        }
    }
    char text[160];
    std::snprintf(text, sizeof(text), "reloaded %u object%s from the templates in memory", static_cast<unsigned>(remade.size()),
                  remade.size() == 1 ? "" : "s");
    result = text;
    if (selectedOnly)
    {
        std::snprintf(text, sizeof(text), " (every object of %u template%s)", static_cast<unsigned>(templates.size()),
                      templates.size() == 1 ? "" : "s");
        result += text;
    }
    logLine(LogLevel::Info, "level: %s", result.c_str());
    return true;
}

// E6: re-position objects after a vertical-scale change

bool EditSession::repositionObjects(std::string& result)
{
    const float current = m_terrain.verticalScale();
    if (current <= 0.0f || m_verticalScaleAtLoad <= 0.0f || !dice::hfe::world::objectManager)
    {
        result = "the level has no terrain whose scale is known";
        return false;
    }
    char text[200];
    if (current == m_verticalScaleAtLoad)
    {
        std::snprintf(text, sizeof(text), "the terrain's vertical scale is the one the level opened with (%g): nothing to move",
                      current);
        result = text;
        return true;
    }
    const float ratio = m_verticalScaleAtLoad / current;
    std::vector<TransformChange> changes;
    const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
    {
        IObject* o = it->second;
        const dice::hfe::world::IObjectTemplate* t = o ? o->getTemplate() : 0;
        if (!t || t->getClassID() == dice::hfe::world::CID_PlayerControlObjectTemplate
            || t->getClassID() == dice::hfe::world::CID_FreeCameraTemplate)
            continue;
        TransformChange change;
        change.id = it->first;
        if (!transformOf(change.id, change.before))
            continue;
        float h = 0.0f;
        if (!m_level.groundHeight(change.before.m[12], change.before.m[14], h))
            continue;
        change.after = change.before;
        change.after.m[13] = change.before.m[13] + h - ratio * h;
        if (change.after.m[13] != change.before.m[13])
            changes.push_back(change);
    }
    std::snprintf(text, sizeof(text), "Re-position %u objects for the terrain's vertical scale %g (it was %g)",
                  static_cast<unsigned>(changes.size()), current, m_verticalScaleAtLoad);
    if (!changes.empty() && !applyTransforms(changes, text, result))
        return false;
    m_verticalScaleAtLoad = current;
    result = text;
    return true;
}

// L2: clean the lightmap folder

bool EditSession::cleanLightmapFolder(bool move, std::string& result)
{
    std::string levelDir, levelPath, error;
    if (!lightingLevel(levelDir, levelPath, error) || !dice::hfe::world::objectManager)
    {
        result = error.empty() ? "no level is open" : error;
        return false;
    }
    const std::string folder = levelDir + "Lightmaps/Objects";
    std::vector<std::string> files, atlasPages;
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((folder + "/*.dds").c_str(), &found);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string name = found.cFileName;
            std::string lowered = name;
            for (size_t i = 0; i < lowered.size(); ++i)
                lowered[i] = static_cast<char>(lowered[i] >= 'A' && lowered[i] <= 'Z' ? lowered[i] - 'A' + 'a' : lowered[i]);
            if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                (lowered.find("lightmapatlas") == std::string::npos ? files : atlasPages).push_back(name);
        } while (FindNextFileA(h, &found));
        FindClose(h);
    }
    // The pages an older, larger atlas left: the folder's index names every page the game reads.
    std::vector<AtlasEntry> entries;
    std::string index, why;
    const bool indexRead = ObjectLibrary::readTextFile(folder + "/LightmapAtlas.tai", index, why)
                           && parseAtlasIndex(index, entries, why);
    const std::vector<std::string> stalePages = indexRead ? unnamedAtlasPages(entries, atlasPages) : std::vector<std::string>();
    // The objects by template (lower case) and truncated position.
    std::set<std::string> used;
    const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
    {
        IObject* o = it->second;
        if (!o || !o->getTemplate())
            continue;
        std::string name = o->getTemplate()->getName();
        for (size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(name[i] >= 'A' && name[i] <= 'Z' ? name[i] - 'A' + 'a' : name[i]);
        const Mat4& m = o->getAbsoluteTransformation();
        char key[400];
        std::snprintf(key, sizeof(key), "%s|%d|%d|%d", name.c_str(), static_cast<int>(m.m[12]), static_cast<int>(m.m[13]),
                      static_cast<int>(m.m[14]));
        used.insert(key);
    }
    std::vector<std::string> unused;
    for (size_t f = 0; f < files.size(); ++f)
    {
        // <template>=<field>...=<x>=<y>=<z>.dds: the template before the first '=', the three numbers after the
        // last three; a name without them is unused, as retail counted it.
        const std::string& name = files[f];
        std::vector<size_t> marks;
        for (size_t i = 0; i < name.size(); ++i)
            if (name[i] == '=')
                marks.push_back(i);
        if (marks.size() < 3)
        {
            unused.push_back(name);
            continue;
        }
        std::string templateName = name.substr(0, marks[0]);
        for (size_t i = 0; i < templateName.size(); ++i)
            templateName[i] = static_cast<char>(templateName[i] >= 'A' && templateName[i] <= 'Z' ? templateName[i] - 'A' + 'a'
                                                                                                 : templateName[i]);
        const int x = std::atoi(name.c_str() + marks[marks.size() - 3] + 1);
        const int y = std::atoi(name.c_str() + marks[marks.size() - 2] + 1);
        const int z = std::atoi(name.c_str() + marks[marks.size() - 1] + 1);
        char key[400];
        std::snprintf(key, sizeof(key), "%s|%d|%d|%d", templateName.c_str(), x, y, z);
        if (!used.count(key))
            unused.push_back(name);
    }
    char text[200];
    std::snprintf(text, sizeof(text), "%u object lightmap%s in ", static_cast<unsigned>(files.size()), files.size() == 1 ? "" : "s");
    result = text + folder;
    std::snprintf(text, sizeof(text), ", %u unused", static_cast<unsigned>(unused.size()));
    result += text;
    for (size_t i = 0; i < unused.size(); ++i)
        result += (i ? ", " : ": ") + unused[i];
    std::snprintf(text, sizeof(text), "; %u atlas page%s", static_cast<unsigned>(atlasPages.size()), atlasPages.size() == 1 ? "" : "s");
    result += text;
    if (!indexRead)
        result += " (no LightmapAtlas.tai read: left out)";
    else if (stalePages.empty())
        result += ", all named by LightmapAtlas.tai";
    else
    {
        std::snprintf(text, sizeof(text), ", %u named by no entry of LightmapAtlas.tai", static_cast<unsigned>(stalePages.size()));
        result += text;
        for (size_t i = 0; i < stalePages.size(); ++i)
            result += (i ? ", " : ": ") + stalePages[i];
    }
    // Stale pages go aside with the unused lightmaps.
    unused.insert(unused.end(), stalePages.begin(), stalePages.end());
    if (move && !unused.empty())
    {
        const std::string target = folder + "/Unused";
        CreateDirectoryA(target.c_str(), 0);
        int moved = 0;
        for (size_t i = 0; i < unused.size(); ++i)
            if (MoveFileExA((folder + "/" + unused[i]).c_str(), (target + "/" + unused[i]).c_str(), MOVEFILE_REPLACE_EXISTING))
                ++moved;
        std::snprintf(text, sizeof(text), "; %d moved into Lightmaps/Objects/Unused", moved);
        result += text;
    }
    else if (!unused.empty())
        result += " (reported only, as retail did without source control; EditorLighting.cleanLightmaps 1 moves them into Lightmaps/Objects/Unused)";
    logLine(LogLevel::Info, "lighting: %s", result.c_str());
    return true;
}

} // namespace bf2editor
