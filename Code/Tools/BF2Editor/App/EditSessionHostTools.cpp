// Editor session tools for selection, ground snapping, level lists, relaunch, help and recording.
// Object/material reports reflect current object templates. Vehicle-pair/team assignments are undoable.
// Batch/hidden sessions refuse help launches. Resave writes every level file, including unchanged ones.
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/EditorLevel.h"
#include "App/FreeCamera.h"
#include "App/LevelDocument.h"
#include "App/LevelSettings.h"
#include "App/ViewRecorder.h"
#include "Framework/EditorLog.h"
#include "Framework/HelpDocuments.h"
#include "Framework/LevelEditorDefs.h"
#include "Framework/ObjectLibrary.h"

#include "Core/Ids.h"
#include "Game/Objects/Base/SimpleObjectTemplate.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/IObjectSpawner.h"
#include "Game/Objects/IObjectTemplate.h"
#include "Physics/Material.h"
#include "Physics/MaterialManager.h"
#include "Scene/Object/ObjectManager.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

namespace dice { namespace hfe { namespace world { extern IBase* materialManager; } } }   // Core/Globals.cpp

namespace bf2editor
{

namespace
{

std::string lowerText(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] >= 'A' && text[i] <= 'Z')
            text[i] = static_cast<char>(text[i] - 'A' + 'a');
    return text;
}

// The material ids a template maps (ObjectTemplate.mapMaterial, kept in its material vector).
void templateMaterials(dice::hfe::world::IObjectTemplate* t, std::set<unsigned>& ids)
{
    if (!t)
        return;
    dice::hfe::world::ISimpleObjectTemplate* simple = static_cast<dice::hfe::world::ISimpleObjectTemplate*>(
        t->queryInterface(dice::hfe::world::IID_ISimpleObjectTemplate));
    if (!simple)
        return;
    std::vector<std::pair<std::string, unsigned int> >* materials = simple->getMaterialVector();
    if (!materials)
        return;
    for (size_t i = 0; i < materials->size(); ++i)
        ids.insert((*materials)[i].second);
}

// The mod's folder name, from the open level's folder (mods/<mod>/Levels/<level>/).
std::string modOf(const std::string& levelDirectory)
{
    const size_t first = levelDirectory.find_first_of("/\\");
    if (first == std::string::npos)
        return std::string();
    const size_t second = levelDirectory.find_first_of("/\\", first + 1);
    return levelDirectory.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1);
}

// The lists of the open mod, read once per mod.
const LevelEditorDefs& levelEditorDefs(const std::string& mod)
{
    static std::string loadedFor;
    static LevelEditorDefs defs;
    if (loadedFor != mod || mod.empty())
    {
        loadedFor = mod;
        std::vector<std::string> problems;
        defs.load(mod, problems);
        for (size_t i = 0; i < problems.size(); ++i)
            logLine(LogLevel::Warning, "level editor lists: %s", problems[i].c_str());
    }
    return defs;
}

// "a\nb" -> a, b.
void splitTwo(const std::string& text, std::string& a, std::string& b)
{
    const size_t newline = text.find('\n');
    a = text.substr(0, newline);
    b = newline == std::string::npos ? std::string() : text.substr(newline + 1);
}

std::string tableOf(const std::map<std::string, int>& counts)
{
    std::string text;
    for (std::map<std::string, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
    {
        char count[32];
        std::snprintf(count, sizeof(count), "\t%d\r\n", it->second);
        text += it->first + count;
    }
    return text;
}

} // namespace

const LevelEditorDefs& EditSession::levelEditorLists() const
{
    return levelEditorDefs(m_document ? modOf(m_document->levelDirectory()) : std::string());
}

bool EditSession::hostTool(const std::string& action, const std::string& argument, std::string& result)
{
    using namespace dice::hfe::world;
    if (action == "supplyGroups" || action == "groupControlPoints" || action == "ungroupControlPoints"
        || action == "connectSupply" || action == "disconnectSupply" || action == "applyGroup" || action == "applySupply")
        return supplyCommand(action, argument, result);
    if (action == "lightSetup" || action == "lightSetupMode" || action == "lightSetupSet" || action == "lightSetupSave")
        return lightSetupCommand(action, argument, result);
    if (action == "pasteInPlace")
    {
        if (m_clipboard.empty())
        {
            result = "the clipboard is empty";
            return false;
        }
        dice::hfe::Vec3 origin;
        origin.x = m_clipboardOrigin[0];
        origin.y = m_clipboardOrigin[1];
        origin.z = m_clipboardOrigin[2];
        return pasteClipboard(origin, true, result);
    }
    if (action == "fromSelection")
    {
        dice::hfe::Mat4 transform;
        if (m_selection.empty() || !transformOf(m_selection.primary(), transform))
        {
            result = "select an object (a spawn point) to look from";
            return false;
        }
        // The heading of the object's forward row (Z), as yaw and pitch.
        const float fx = transform.m[8], fy = transform.m[9], fz = transform.m[10];
        const float yaw = std::atan2(fx, fz) * 57.29578f;
        const float pitch = std::atan2(fy, std::sqrt(fx * fx + fz * fz)) * 57.29578f;
        m_camera.place(transform.m[12], transform.m[13], transform.m[14], yaw, pitch);
        char text[160];
        std::snprintf(text, sizeof(text), "camera at %s: %.1f/%.1f/%.1f, yaw %.1f, pitch %.1f",
                      describe(m_selection.primary()).c_str(), transform.m[12], transform.m[13], transform.m[14], yaw, pitch);
        result = text;
        return true;
    }
    if (action == "snapAll")
    {
        if (!objectManager || !m_document)
        {
            result = "no level is open";
            return false;
        }
        std::vector<TransformChange> changes;
        int offMap = 0;
        const std::map<unsigned int, IObject*>& roots = objectManager->getAllObjectsFromGrid();
        for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        {
            LevelDocument::RecordRef ref;
            if (!it->second || !canPick(it->second) || !m_document->recordOf(it->first, ref))
                continue;
            TransformChange change;
            change.id = it->first;
            if (!transformOf(change.id, change.before))
                continue;
            float anchor[3] = { change.before.m[12], change.before.m[13], change.before.m[14] };
            worldAnchorOf(change.id, anchor);
            float ground = 0.0f;
            if (!m_level.groundHeight(anchor[0], anchor[2], ground))
            {
                ++offMap;
                continue;
            }
            const float y = snapPivotHeight(object(change.id), ground);
            if (change.before.m[13] == y)
                continue;
            change.after = change.before;
            change.after.m[13] = y;
            changes.push_back(change);
        }
        char text[160];
        if (changes.empty())
        {
            std::snprintf(text, sizeof(text), "every object is on the ground already%s", offMap ? " (some lie off the terrain)" : "");
            result = text;
            return true;
        }
        std::snprintf(text, sizeof(text), "Snap all %u objects to the ground", static_cast<unsigned>(changes.size()));
        if (!applyTransforms(changes, text, result))
            return false;
        result = text;
        if (offMap)
        {
            std::snprintf(text, sizeof(text), "; %d off the terrain left where they are", offMap);
            result += text;
        }
        return true;
    }
    if (action == "saveLists")
    {
        std::string levelDir, levelPath, error;
        if (!lightingLevel(levelDir, levelPath, error) || !objectManager || !objectTemplateManager)
        {
            result = error.empty() ? "no level is open" : error;
            return false;
        }
        std::map<std::string, int> statics, spawned;
        std::set<unsigned> materials;
        const std::map<unsigned int, IObject*>& roots = objectManager->getAllObjectsFromGrid();
        for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        {
            IObjectTemplate* t = it->second ? const_cast<IObjectTemplate*>(it->second->getTemplate()) : 0;
            if (!t)
                continue;
            const unsigned classId = t->getClassID();
            if (classId == CID_EnvMapTemplate || classId == CID_ControlPointTemplate || classId == dice::hfe::CID_SpawnPointTemplate)
                continue;
            if (classId == CID_ObjectSpawnerTemplate)
            {
                // Its two teams' templates, from the level file that defines the spawner's template.
                for (size_t f = 0; m_document && f < m_document->fileCount(); ++f)
                {
                    const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(f, t->getName());
                    for (size_t l = 0; l < lines.size(); ++l)
                    {
                        if (_stricmp(lines[l].property.c_str(), "setObjectTemplate") != 0)
                            continue;
                        const size_t space = lines[l].value.find_first_of(" \t");
                        if (space == std::string::npos)
                            continue;
                        const std::string name = lines[l].value.substr(lines[l].value.find_first_not_of(" \t", space));
                        if (name.empty())
                            continue;
                        ++spawned[lowerText(name)];
                        templateMaterials(objectTemplateManager->getTemplate(name), materials);
                    }
                }
                continue;
            }
            ++statics[lowerText(t->getName())];
            templateMaterials(t, materials);
        }
        const std::string objectList = tableOf(statics) + "\r\n" + tableOf(spawned);
        std::string materialList;
        MaterialManager* manager = static_cast<MaterialManager*>(materialManager);
        for (std::set<unsigned>::const_iterator it = materials.begin(); it != materials.end(); ++it)
        {
            Material* material = manager ? manager->getMaterialById(*it) : 0;
            char line[160];
            std::snprintf(line, sizeof(line), "%s: (%u)\r\n", material ? material->getName().c_str() : "unknown material", *it);
            materialList += line;
        }
        const std::string objectPath = levelDir + "Editor/ObjectList.txt";
        const std::string materialPath = levelDir + "Editor/MaterialList.txt";
        if (!writeFilesNow("object and material lists", { std::make_pair(objectPath, objectList),
                                                          std::make_pair(materialPath, materialList) }, error))
        {
            result = error;
            return false;
        }
        char text[240];
        std::snprintf(text, sizeof(text), "wrote %s (%u templates, %u spawned) and %s (%u materials)", objectPath.c_str(),
                      static_cast<unsigned>(statics.size()), static_cast<unsigned>(spawned.size()), materialPath.c_str(),
                      static_cast<unsigned>(materials.size()));
        result = text;
        logLine(LogLevel::Info, "level: %s", result.c_str());
        return true;
    }
    if (action == "selectSpawners")
    {
        IObject* cp = m_selection.empty() ? 0 : object(m_selection.primary());
        const IObjectTemplate* cpTemplate = cp ? cp->getTemplate() : 0;
        if (!cpTemplate || cpTemplate->getClassID() != CID_ControlPointTemplate || !m_document || !objectManager)
        {
            result = "select a control point first";
            return false;
        }
        // Its id, from the level file that defines its template.
        std::string id;
        for (size_t f = 0; f < m_document->fileCount() && id.empty(); ++f)
        {
            const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(f, cpTemplate->getName());
            for (size_t l = 0; l < lines.size(); ++l)
                if (_stricmp(lines[l].property.c_str(), "controlPointId") == 0)
                    id = lines[l].value;
        }
        if (id.empty())
        {
            result = cpTemplate->getName() + " has no controlPointId in the level's files";
            return false;
        }
        const int cpId = std::atoi(id.c_str());
        const unsigned cpObject = m_selection.primary();
        // Every layer's objects are in the view and each game mode numbers its own control points: only objects whose
        // layer meets the control point's (layersMeet) are its spawners.
        const int cpLayer = static_cast<int>(cp->getLayer());
        int points = 0, spawners = 0;
        std::vector<unsigned> linkedIds;
        const std::map<unsigned int, IObject*>& roots = objectManager->getAllObjectsFromGrid();
        for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        {
            IObject* o = it->second;
            const IObjectTemplate* t = o ? o->getTemplate() : 0;
            if (!t || it->first == cpObject || !canPick(o) || !layersMeet(cpLayer, static_cast<int>(o->getLayer())))
                continue;
            IObjectSpawner* spawner = static_cast<IObjectSpawner*>(o->queryInterface(IID_IObjectSpawner));
            if (spawner)
            {
                if (spawner->getControlPointId() == cpId)
                {
                    linkedIds.push_back(it->first);
                    ++spawners;
                }
                continue;
            }
            if (t->getClassID() != dice::hfe::CID_SpawnPointTemplate)
                continue;
            for (size_t f = 0; f < m_document->fileCount(); ++f)
            {
                const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(f, t->getName());
                bool linked = false;
                for (size_t l = 0; l < lines.size(); ++l)
                    if (_stricmp(lines[l].property.c_str(), "setControlPointId") == 0 && std::atoi(lines[l].value.c_str()) == cpId)
                        linked = true;
                if (linked)
                {
                    linkedIds.push_back(it->first);
                    ++points;
                    break;
                }
            }
        }
        // The control point last, so it stays the primary (the gizmo and the Selection panel's object).
        m_selection.clear();
        for (size_t i = 0; i < linkedIds.size(); ++i)
            m_selection.add(linkedIds[i]);
        m_selection.add(cpObject);
        char text[200];
        std::snprintf(text, sizeof(text), "%s (control point %d) with its %d spawn point%s and %d object spawner%s selected",
                      cpTemplate->getName().c_str(), cpId, points, points == 1 ? "" : "s", spawners, spawners == 1 ? "" : "s");
        result = text;
        return true;
    }
    if (action == "terrainView")
    {
        result = m_editableTerrain.describe();
        return true;
    }
    if (action == "terrainShowSector")
    {
        // "x\nz\n0|1"
        std::istringstream in(argument);
        std::string a, b, c;
        std::getline(in, a);
        std::getline(in, b);
        std::getline(in, c);
        if (c != "0" && c != "1")
        {
            result = "EditorTerrain.showSector takes a sector x, z and 0 or 1";
            return false;
        }
        return m_editableTerrain.setSectorShown(std::atoi(a.c_str()), std::atoi(b.c_str()), c == "1", result);
    }
    if (action == "overgrowthGenerate")
        return m_editableTerrain.generateOvergrowth(result);
    if (action == "terrainCompile")
    {
        // The painted maps are written into the staging folder first, so the compile reads the current weights.
        std::vector<std::pair<std::string, std::string> > files;
        return m_editableTerrain.saveMaps(files, result) && m_editableTerrain.compileTerrain(files, result);
    }
    if (action == "terrainShownSectors")
    {
        result.clear();
        for (int z = -1; z <= 1; ++z)
            for (int x = -1; x <= 1; ++x)
                if (m_editableTerrain.sectorShown(x, z))
                    result += (result.empty() ? "" : "; ") + std::to_string(x) + "," + std::to_string(z);
        if (result.empty())
            result = "no surrounding sector shown";
        return true;
    }
    if (action == "groundLayers")
    {
        result = groundListing();
        return true;
    }
    if (action == "groundSet" || action == "groundApply")
    {
        // "<layer>\n<name>\n<value>" (EditorGround.set / apply / misc).
        const size_t first = argument.find('\n');
        const size_t second = first == std::string::npos ? std::string::npos : argument.find('\n', first + 1);
        if (second == std::string::npos)
        {
            result = "EditorGround.set takes a layer, a setting and a value";
            return false;
        }
        const int layer = std::atoi(argument.substr(0, first).c_str());
        const std::string name = argument.substr(first + 1, second - first - 1);
        const std::string value = argument.substr(second + 1);
        if (action == "groundSet")
            return groundSet(layer, name, value, result);
        // A replay (the history's scripts) answers nothing on success, as every replayed line must.
        if (!groundApply(layer, name, value, result))
            return false;
        result.clear();
        return true;
    }
    if (action == "groundRemoveLayer")
    {
        if (!groundRemoveLayer(std::atoi(argument.c_str()), result))
            return false;
        result.clear();
        return true;
    }
    if (action == "terrainMerge" || action == "terrainClearEdits")
    {
        TerrainPaintMode mode = m_terrainBrush.paintMode;
        if (argument == "detail")
            mode = TerrainPaintMode::Detail;
        else if (argument == "color" || argument == "colour")
            mode = TerrainPaintMode::Color;
        else if (argument == "both")
            mode = TerrainPaintMode::Both;
        else if (!argument.empty())
        {
            result = "merge and clear take detail, color or both (none: the paint mode)";
            return false;
        }
        return action == "terrainMerge" ? m_editableTerrain.mergeEdits(mode, result)
                                        : m_editableTerrain.clearEdits(mode, result);
    }
    if (action == "terrainTechnique")
    {
        std::string error;
        if (!m_editableTerrain.setTechnique(std::atoi(argument.c_str()), error))
        {
            result = error;
            return false;
        }
        result = m_editableTerrain.describe();
        return true;
    }
    if (action == "resave")
        return resaveLevel(result);
    if (action.compare(0, 5, "save.") == 0)
        return saveCommand(action.substr(5), argument, result);
    if (action == "lastOffsetCopy")
        return lastOffsetCopy(result);
    if (action.compare(0, 8, "preview.") == 0)
        return previewCommand(action.substr(8), argument, result);
    if (action == "reloadSelected" || action == "reloadAll")
        return reloadObjects(action == "reloadSelected", result);
    if (action == "reposition")
        return repositionObjects(result);
    if (action == "cleanLightmaps")
        return cleanLightmapFolder(argument == "1", result);
    if (action == "template")
    {
        // "<op>\n<a>\n<b>\n<c>" (the EditorObjectTemplate rows pack their arguments so).
        std::string parts[4];
        size_t at = 0;
        for (int i = 0; i < 4; ++i)
        {
            const size_t next = argument.find('\n', at);
            parts[i] = argument.substr(at, next == std::string::npos ? std::string::npos : next - at);
            if (next == std::string::npos)
                break;
            at = next + 1;
        }
        const std::string& op = parts[0];
        if (op == "revert")
            return revertTemplate(parts[1], result);
        if (op == "addLine")
            return addObjectTemplateLine(parts[1], parts[2], parts[3], result);
        if (op == "armorEffect")
            return addArmorEffect(parts[1], parts[2], result);
        if (op == "barrel")
            return setBarrel(parts[1], parts[2], parts[3] == "1", result);
        if (op == "fireRates")
        {
            const std::string modes = "," + parts[2] + ",";
            return setFireRates(parts[1], modes.find(",single,") != std::string::npos, modes.find(",burst,") != std::string::npos,
                                modes.find(",full,") != std::string::npos, result);
        }
        if (op == "seatParent")
            return setSeatParent(parts[1], parts[2], result);
        if (op == "turnChild" || op == "turnChildren")
            return turnTemplateChildren(parts[1], parts[2], static_cast<float>(std::atof(parts[3].c_str())),
                                        op == "turnChildren", result);
        if (op == "resetChild")
            return resetTemplateChild(parts[1], parts[2], result);
        if (op == "geometry")
            return geometryCommand("list", parts[1], "", "", result);
        if (op == "setGeometry")
            return geometryCommand("set", parts[1], parts[2], parts[3], result);
        if (op == "removeGeometry")
            return geometryCommand("remove", parts[1], parts[2], "", result);
        result = "no template operation '" + op + "'";
        return false;
    }
    if (action == "snap")
    {
        const size_t blank = argument.find(' ');
        if (argument.empty())
        {
            result = snapList();
            return true;
        }
        if (blank == std::string::npos)
        {
            result = "give a snap mode and its value";
            return false;
        }
        return setSnapMode(argument.substr(0, blank), argument.substr(blank + 1), result);
    }
    if (action == "measureTool")
        return setMeasureTool(argument == "1", result);
    if (action == "measureAdd")
    {
        float x = 0.0f, z = 0.0f;
        if (std::sscanf(argument.c_str(), "%f %f", &x, &z) != 2)
        {
            result = "give the point's x and z";
            return false;
        }
        return measureAdd(x, z, result);
    }
    if (action == "measureUndo")
        return measureUndo(result);
    if (action == "measureFinish")
        return measureFinish(result);
    if (action == "record")
    {
        std::string error;
        if (!viewRecorder().start(argument, error))
        {
            result = error;
            return false;
        }
        result = "recording the view to " + argument + " (EditorView.stopRecording ends it)";
        return true;
    }
    if (action == "stopRecord")
        return viewRecorder().stop(result);
    if (action == "recordStatus")
    {
        char text[64];
        std::snprintf(text, sizeof(text), "%u frames so far", viewRecorder().frames());
        result = viewRecorder().recording() ? "recording to " + viewRecorder().path() + ", " + text
                                            : std::string("not recording");
        return true;
    }
    if (action == "description")
    {
        // "<action>\n<which>\n<field>\n<value>" (the console rows pack their arguments so).
        std::string parts[4];
        size_t at = 0;
        for (int i = 0; i < 4; ++i)
        {
            const size_t next = argument.find('\n', at);
            parts[i] = argument.substr(at, next == std::string::npos ? std::string::npos : next - at);
            if (next == std::string::npos)
                break;
            at = next + 1;
        }
        return description(parts[0], parts[1], parts[2], parts[3], result);
    }
    if (action == "spawnerPresets" || action == "teams")
    {
        const LevelEditorDefs& defs = levelEditorDefs(m_document ? modOf(m_document->levelDirectory()) : std::string());
        result.clear();
        if (action == "teams")
        {
            for (size_t i = 0; i < defs.teams().size(); ++i)
                result += (i ? ", " : "") + defs.teams()[i].name + " (" + defs.teams()[i].language + ")";
            if (result.empty())
                result = "no team list (bf2editor/mods/<mod>/LevelEditor/LevelSettings.con)";
            return true;
        }
        char count[64];
        std::snprintf(count, sizeof(count), "%u presets: ", static_cast<unsigned>(defs.spawnerPresets().size()));
        for (size_t i = 0; i < defs.spawnerPresets().size(); ++i)
            result += (i ? ", " : "") + defs.spawnerPresets()[i].name;
        result = defs.spawnerPresets().empty() ? std::string("no spawner presets (bf2editor/mods/<mod>/LevelEditor/ObjectSpawnerDef.con)")
                                               : count + result;
        return true;
    }
    if (action == "spawnerPreset")
    {
        std::string templateName, presetName;
        splitTwo(argument, templateName, presetName);
        size_t file = 0;
        std::string className;
        if (!m_document || !m_document->templateFile(templateName, file, className) || _stricmp(className.c_str(), "ObjectSpawner") != 0)
        {
            result = "'" + templateName + "' is no object spawner template of the level's files";
            return false;
        }
        const SpawnerPreset* preset = levelEditorDefs(modOf(m_document->levelDirectory())).findPreset(presetName);
        if (!preset)
        {
            result = "no spawner preset '" + presetName + "' (EditorGameplay.spawnerPresets lists them)";
            return false;
        }
        // The two vehicle lines as they are now, for the undo.
        std::string old[2];
        bool present[2] = { false, false };
        const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(file, templateName);
        for (size_t l = 0; l < lines.size(); ++l)
            for (int team = 0; team < 2; ++team)
                if (lines[l].key == (team == 0 ? "setobjecttemplate 1" : "setobjecttemplate 2"))
                {
                    const size_t blank = lines[l].value.find_first_of(" \t");
                    old[team] = blank == std::string::npos ? std::string() : lines[l].value.substr(lines[l].value.find_first_not_of(" \t", blank));
                    present[team] = true;
                }
        const std::string wanted[2] = { preset->team1, preset->team2 };
        HistoryEntry entry;
        for (int team = 0; team < 2; ++team)
        {
            char index[4];
            std::snprintf(index, sizeof(index), "%d", team + 1);
            entry.doScript += "EditorTemplate.apply " + templateName + " setObjectTemplate \"" + index + " " + wanted[team] + "\"\n";
        }
        for (int team = 1; team >= 0; --team)
        {
            char index[4];
            std::snprintf(index, sizeof(index), "%d", team + 1);
            entry.undoScript += present[team] ? "EditorTemplate.apply " + templateName + " setObjectTemplate \"" + index + " " + old[team] + "\"\n"
                                              : "EditorTemplate.remove " + templateName + " \"setobjecttemplate " + index + "\"\n";
        }
        if (present[0] && present[1] && _stricmp(old[0].c_str(), wanted[0].c_str()) == 0 && _stricmp(old[1].c_str(), wanted[1].c_str()) == 0)
        {
            result = templateName + " has the vehicles of " + preset->name + " already";
            return true;
        }
        entry.label = "Spawner " + templateName + ": vehicles of " + preset->name + " (" + preset->team1 + " / " + preset->team2 + ")";
        if (!m_history.execute(entry, result))
            return false;
        logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
        result = entry.label;
        return true;
    }
    if (action == "team")
    {
        std::string number, teamName;
        splitTwo(argument, number, teamName);
        if (number != "1" && number != "2")
        {
            result = "the team is 1 or 2";
            return false;
        }
        const TeamDefinition* team = levelEditorDefs(m_document ? modOf(m_document->levelDirectory()) : std::string()).findTeam(teamName);
        if (!team)
        {
            result = "no team '" + teamName + "' in the team list (EditorLevelSettings.teams lists them)";
            return false;
        }
        // The name, the language and the seven kits, as the retail dialog fills them from the team's name.
        static const char* const kKits[7][2] = { { "Specops", "light" }, { "Sniper", "light" }, { "Assault", "heavy" },
                                                 { "Support", "heavy" }, { "Engineer", "light" }, { "Medic", "light" },
                                                 { "AT", "heavy" } };
        std::string keys[9], values[9];
        keys[0] = "team" + number + ".name";
        values[0] = team->name;
        keys[1] = "team" + number + ".language";
        values[1] = team->language;
        for (int k = 0; k < 7; ++k)
        {
            char key[32];
            std::snprintf(key, sizeof(key), "team%s.kit%d", number.c_str(), k);
            keys[2 + k] = key;
            values[2 + k] = team->name + "_" + kKits[k][0] + " " + team->name + "_" + kKits[k][1] + "_soldier";
        }
        HistoryEntry entry;
        std::string undo;
        for (int k = 0; k < 9; ++k)
        {
            std::string old, ignored;
            const bool had = levelSetting(keys[k], old, ignored);
            entry.doScript += "EditorLevelSettings.apply " + keys[k] + " \"" + values[k] + "\"\n";
            undo = (had ? "EditorLevelSettings.apply " + keys[k] + " \"" + old + "\"\n" : "EditorLevelSettings.unset " + keys[k] + "\n") + undo;
        }
        entry.undoScript = undo;
        entry.label = "Team " + number + ": " + team->name + " (" + team->language + ") and its seven kits";
        if (!m_history.execute(entry, result))
            return false;
        logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
        result = entry.label;
        return true;
    }
    if (action == "overlays")
    {
        result = overlayList();
        return true;
    }
    if (action == "overlayLinks")
    {
        // What the connections overlay draws: each spawn point's and spawner's control point, and the radii.
        std::vector<OverlayLink> links;
        std::vector<OverlayCircle> circles;
        overlayGeometry(links, circles);
        char text[96];
        std::snprintf(text, sizeof(text), "%u links, %u control point radii", static_cast<unsigned>(links.size()),
                      static_cast<unsigned>(circles.size()));
        result = text;
        for (size_t i = 0; i < links.size(); ++i)
        {
            IObject* from = object(links[i].from);
            IObject* to = object(links[i].to);
            if (from && to && from->getTemplate() && to->getTemplate())
                result += "; " + from->getTemplate()->getName() + " -> " + to->getTemplate()->getName();
        }
        return true;
    }
    if (action == "overlay")
    {
        const size_t blank = argument.find(' ');
        if (blank == std::string::npos)
        {
            result = "give an overlay and its value (EditorView.overlays lists them)";
            return false;
        }
        return setOverlay(argument.substr(0, blank), argument.substr(argument.find_first_not_of(' ', blank)), result);
    }
    if (action == "helpDocuments" || action == "helpOpen")
    {
        const std::vector<HelpDocument> documents = helpDocuments(kHelpFolder);
        if (action == "helpDocuments")
        {
            result.clear();
            for (size_t i = 0; i < documents.size(); ++i)
                result += (i ? "; " : "") + documents[i].label;
            if (result.empty())
                result = std::string("no help documents in ") + kHelpFolder + " (the retail editor's folder beside the game)";
            return true;
        }
        const HelpDocument* document = findHelpDocument(documents, argument);
        if (!document)
        {
            result = "no help document '" + argument + "' (EditorHelp.documents lists them)";
            return false;
        }
        if (!m_canRelaunch)
        {
            result = "a hidden or batch run starts no other program: refused (" + document->path + ")";
            return false;
        }
        return openHelpDocument(*document, result);
    }
    if (action == "helpDescribe")
    {
        static CommandCatalogue catalogue;
        static std::string loadError;
        static bool loaded = false;
        if (!loaded)
        {
            loaded = true;
            catalogue.load(std::string(kHelpFolder) + "/CommandDescriptions.dat", loadError);
        }
        if (!catalogue.size())
        {
            result = "the retail command catalogue could not be read: " + loadError;
            return false;
        }
        const std::string* description = catalogue.describe(argument);
        if (!description)
        {
            result = "'" + argument + "' is not in the retail command catalogue";
            return false;
        }
        result = argument + ": " + (description->empty() ? std::string("(no description)") : *description);
        return true;
    }
    if (action == "copyArea")
        return copyCombatArea(argument, result);
    if (action == "pasteArea")
        return pasteCombatArea(argument, result);
    if (action == "levels" || action == "open")
    {
        std::string levelDir, levelPath, error;
        lightingLevel(levelDir, levelPath, error);
        // mods/<mod>/Levels/<name>/: the level folders beside the open one.
        std::string levels = levelDir;
        while (!levels.empty() && (levels[levels.size() - 1] == '/' || levels[levels.size() - 1] == '\\'))
            levels.erase(levels.size() - 1);
        const size_t slash = levels.find_last_of("/\\");
        levels = slash == std::string::npos ? std::string() : levels.substr(0, slash);
        std::vector<std::string> names;
        WIN32_FIND_DATAA found;
        HANDLE h = levels.empty() ? INVALID_HANDLE_VALUE : FindFirstFileA((levels + "/*").c_str(), &found);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                const std::string name = found.cFileName;
                if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && name != "." && name != ".."
                    && (GetFileAttributesA((levels + "/" + name + "/server.zip").c_str()) != INVALID_FILE_ATTRIBUTES
                        || GetFileAttributesA((levels + "/" + name + "/Init.con").c_str()) != INVALID_FILE_ATTRIBUTES))
                    names.push_back(name);
            } while (FindNextFileA(h, &found));
            FindClose(h);
        }
        std::sort(names.begin(), names.end());
        if (action == "levels")
        {
            result.clear();
            for (size_t i = 0; i < names.size(); ++i)
                result += (i ? ", " : "") + names[i];
            if (result.empty())
                result = "no levels found";
            return true;
        }
        bool known = false;
        std::string exact;
        for (size_t i = 0; i < names.size() && !known; ++i)
            if (_stricmp(names[i].c_str(), argument.c_str()) == 0)
            {
                known = true;
                exact = names[i];
            }
        if (!known)
        {
            result = "no level named " + argument + " in the mod (EditorLevel.levels lists them)";
            return false;
        }
        if (!m_canRelaunch)
        {
            result = "this run cannot start the editor again (a hidden or batch run): refused";
            return false;
        }
        m_relaunchMode = m_level.gameMode();
        m_relaunchSize = m_level.layerSize();
        m_relaunchLevel = exact;
        m_relaunchMod.clear();
        result = "the editor starts again on the level " + exact + " once it closes";
        return true;
    }
    result = "no such tool '" + action + "'";
    return false;
}

} // namespace bf2editor
