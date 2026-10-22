// Edits material files in place and mirrors each undoable edit into the running engine.
// Removed cells are emptied because the engine has no cell-removal operation.
// Reselects an existing cell afterward so console reflection cannot create an empty selected cell.
// Rejects IDs >= 10000 and missing or wrong-class templates before invoking engine commands.
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "Framework/EditorLog.h"
#include "Framework/ObjectArchives.h"
#include "Framework/ObjectLibrary.h"

#include "Core/SettingsRepostitory.h"
#include "Game/Objects/IObjectTemplate.h"
#include "IO/Console/Console.h"
#include "Physics/MaterialManager.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dice { namespace hfe { namespace world { extern IBase* materialManager; } } }   // Core/Globals.cpp

namespace bf2editor
{

namespace
{

const char* const kTypeNames[] = { "Ground", "World", "Projectiles", "Vehicles", "Misc" };

std::string typeLabel(const std::string& type)
{
    const int t = std::atoi(type.c_str());
    return t >= 0 && t < 5 ? std::string(kTypeNames[t]) : "type " + type;
}

std::string number(int value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%d", value);
    return text;
}

bool validId(int id)
{
    return id >= 0 && id < 10000;
}

// One engine console line with full access; its answer, trimmed.
std::string engineAnswer(const std::string& line)
{
    std::string answer;
    dice::hfe::io::mainConsole->executeLine(line + "\n", answer, false, false, false, false, true);
    const size_t begin = answer.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    return answer.substr(begin, answer.find_last_not_of(" \t\r\n") - begin + 1);
}

dice::hfe::world::MaterialManager* manager()
{
    return static_cast<dice::hfe::world::MaterialManager*>(dice::hfe::world::materialManager);
}

const char* const kTemplateRows[] = { "MaterialManager.setEffectTemplate", "MaterialManager.setDecalTemplate",
                                      "MaterialManager.setSoundTemplate" };
// The class a slot's template must be (census §6.1: EffectBundle, Decal, Sound).
const char* const kTemplateClasses[] = { "EffectBundle", "Decal", "Sound" };

std::string modDirectory()
{
    return dice::hfe::settings ? dice::hfe::settings->stringGet("GSModDirectory", "") : std::string();
}

// The zips the mod's archive scripts mount at Common (Common_server.zip and Common_client.zip in bf2).
std::vector<std::string> commonArchivesOf(const std::string& mod)
{
    std::vector<std::string> zips;
    const char* const scripts[] = { "/ServerArchives.con", "/ClientArchives.con" };
    for (size_t i = 0; i < 2; ++i)
    {
        std::string text, error;
        if (!ObjectLibrary::readTextFile(mod + scripts[i], text, error))
            continue;
        const std::vector<std::string> found = archivesMountedAt(text, "Common");
        for (size_t k = 0; k < found.size(); ++k)
            zips.push_back(mod + "/" + found[k]);
    }
    return zips;
}

// A cell's engine lines that make it what `cells` says (`clear`: what an empty cell is).
std::string cellLines(const MaterialCellFile& cells, int a, int b, bool clear)
{
    std::string lines = "MaterialManager.createCell " + number(a) + " " + number(b) + "\n";
    std::string damage;
    cells.damageMod(a, b, damage);
    lines += "MaterialManager.damageMod " + (clear || damage.empty() ? std::string("-1") : damage) + "\n";
    for (int k = 0; k < 3; ++k)
    {
        const MaterialCellFile::Kind kind = static_cast<MaterialCellFile::Kind>(k);
        const std::vector<int> indexes = cells.indexes(a, b, kind);
        for (size_t i = 0; i < indexes.size(); ++i)
        {
            std::string name;
            cells.templateAt(a, b, kind, indexes[i], name);
            lines += std::string(kTemplateRows[k]) + " " + number(indexes[i]) + " " + (clear ? std::string("NoObjectTemplate") : name)
                   + "\n";
        }
    }
    return lines;
}

// A material's engine lines from the define file: every property, defaults included.
std::string materialLines(const MaterialDefineFile& defines, int id)
{
    std::string lines = "Material.active " + number(id) + "\n";
    const std::vector<std::string>& properties = MaterialDefineFile::properties();
    for (size_t i = 0; i < properties.size(); ++i)
    {
        std::string value;
        defines.value(id, properties[i], value);
        lines += "Material." + properties[i] + " " + (properties[i] == "name" ? "\"" + value + "\"" : value) + "\n";
    }
    return lines;
}

} // namespace

bool EditSession::loadMaterials(std::string& error)
{
    return m_materials.load(error);
}

bool EditSession::materialsModified() const
{
    return m_materials.modified();
}

void EditSession::planMaterialFiles(SavePlan& plan)
{
    if (m_materials.modified())
        m_materials.planSave(plan.part("material files"));
}

bool EditSession::saveMaterialFiles(std::string& result)
{
    SavePlan plan;
    planMaterialFiles(plan);
    plan.dropUnchanged();
    return writeSave(plan, "EditorMaterial.save", result) && !plan.anyRefused();
}

void EditSession::selectExistingMaterialCell()
{
    const std::vector<std::pair<int, int> > cells = m_materials.cells().cells();
    if (!cells.empty() && dice::hfe::io::mainConsole)
        engineAnswer("MaterialManager.createCell " + number(cells.back().first) + " " + number(cells.back().second));
}

bool EditSession::recordMaterialEdit(const MaterialEdit& edit, const std::string& label, std::string& result)
{
    const int id = m_nextMaterialEdit++;
    m_materialEdits[id] = edit;
    HistoryEntry entry;
    entry.label = label;
    char line[80];
    std::snprintf(line, sizeof(line), "EditorMaterial.applyEdit %d after\n", id);
    entry.doScript = line;
    std::snprintf(line, sizeof(line), "EditorMaterial.applyEdit %d before\n", id);
    entry.undoScript = line;
    if (!m_history.execute(entry, result))
    {
        m_materialEdits.erase(id);
        return false;
    }
    logLine(LogLevel::Info, "edit: %s", label.c_str());
    result = label;
    return true;
}

bool EditSession::applyMaterialEdit(int id, const std::string& state, std::string& error)
{
    std::map<int, MaterialEdit>::const_iterator found = m_materialEdits.find(id);
    if (found == m_materialEdits.end() || (state != "before" && state != "after"))
    {
        error = "no such material edit (EditorMaterial.applyEdit <id> <before|after>)";
        return false;
    }
    const MaterialEdit& edit = found->second;
    const bool after = state == "after";
    m_materials.defines().parse(after ? edit.defineAfter : edit.defineBefore);
    m_materials.cells().parse(after ? edit.cellsAfter : edit.cellsBefore);
    const std::vector<int>& removals = after ? edit.removeDo : edit.removeUndo;
    for (size_t i = 0; i < removals.size(); ++i)
        if (manager() && validId(removals[i]))
            manager()->removeMaterial(static_cast<unsigned int>(removals[i]));
    if (dice::hfe::io::mainConsole)
    {
        const std::string& script = after ? edit.engineDo : edit.engineUndo;
        size_t start = 0;
        while (start < script.size())
        {
            size_t end = script.find('\n', start);
            if (end == std::string::npos)
                end = script.size();
            const std::string line = script.substr(start, end - start);
            start = end + 1;
            if (line.empty())
                continue;
            const std::string answer = engineAnswer(line);
            if (!answer.empty())
                logLine(LogLevel::Warning, "material: the engine answered %s -> %s", line.c_str(), answer.c_str());
        }
    }
    selectExistingMaterialCell();
    return true;
}

bool EditSession::materialCommand(const std::string& action, const std::vector<std::string>& args, std::string& result)
{
    std::string error;
    if (action == "extract")
        return extractCommonArchives(result);
    if (!m_materials.load(error))
    {
        result = error;
        return false;
    }
    MaterialDefineFile& defines = m_materials.defines();
    MaterialCellFile& cells = m_materials.cells();
    if (action == "status")
    {
        char counts[96];
        std::snprintf(counts, sizeof(counts), "%u materials, %u cells", static_cast<unsigned>(defines.ids().size()),
                      static_cast<unsigned>(cells.cells().size()));
        result = m_materials.definePath() + ", " + m_materials.cellPath() + ": " + counts + "; "
               + (m_materials.archived() ? "read from the mounted Common archives (read-only)"
                                         : "loose, saved to " + m_materials.defineWritePath())
               + (m_materials.modified() ? "; modified" : "");
        return true;
    }
    if (action == "list")
    {
        const std::vector<int> ids = defines.ids();
        result.clear();
        for (size_t i = 0; i < ids.size(); ++i)
        {
            std::string name, type;
            defines.value(ids[i], "name", name);
            defines.value(ids[i], "type", type);
            result += (i ? "; " : "") + number(ids[i]) + " " + name + " (" + typeLabel(type) + ")";
        }
        if (result.empty())
            result = "no materials";
        return true;
    }
    const int first = args.size() > 0 ? std::atoi(args[0].c_str()) : -1;
    const int second = args.size() > 1 ? std::atoi(args[1].c_str()) : -1;
    if (action == "info")
    {
        if (!defines.has(first))
        {
            result = "no material " + number(first);
            return false;
        }
        const std::vector<std::string>& properties = MaterialDefineFile::properties();
        std::string name;
        defines.value(first, "name", name);
        result = "material " + number(first) + " " + name + ":";
        for (size_t i = 1; i < properties.size(); ++i)
        {
            std::string value;
            defines.value(first, properties[i], value);
            result += (i > 1 ? ", " : " ") + properties[i] + " " + value
                    + (properties[i] == "type" ? " (" + typeLabel(value) + ")" : std::string());
        }
        return true;
    }
    if (action == "cells")
    {
        const std::vector<std::pair<int, int> > all = cells.cells();
        result.clear();
        for (size_t i = 0; i < all.size(); ++i)
        {
            if (all[i].first != first)
                continue;
            std::string damage;
            cells.damageMod(all[i].first, all[i].second, damage);
            result += (result.empty() ? "" : "; ") + number(all[i].first) + " " + number(all[i].second)
                    + (damage.empty() ? std::string() : " damageMod " + damage);
        }
        if (result.empty())
            result = "material " + number(first) + " attacks with no cell";
        return true;
    }
    if (action == "cell")
    {
        if (!cells.has(first, second))
        {
            result = "no cell " + number(first) + " " + number(second);
            return false;
        }
        std::string damage;
        cells.damageMod(first, second, damage);
        result = "cell " + number(first) + " " + number(second) + ": damageMod " + (damage.empty() ? std::string("unset") : damage);
        for (int k = 0; k < 3; ++k)
        {
            const MaterialCellFile::Kind kind = static_cast<MaterialCellFile::Kind>(k);
            const std::vector<int> indexes = cells.indexes(first, second, kind);
            for (size_t i = 0; i < indexes.size(); ++i)
            {
                std::string name;
                cells.templateAt(first, second, kind, indexes[i], name);
                result += std::string("; ") + MaterialCellFile::kindName(kind) + " " + number(indexes[i]) + " " + name;
            }
        }
        return true;
    }
    if (action == "save")
    {
        if (!saveMaterialFiles(result))
            return false;
        if (result.empty())
            result = "nothing to save: the material files have no changes";
        return true;
    }
    // The edits
    if (m_materials.archived())
    {
        result = "the material files are read from the mounted Common archives, which shadow any loose copy, so an "
                 "edit could never be saved: extract them (EditorMaterial.extract) and start with +looseCommon 1";
        return false;
    }
    MaterialEdit edit;
    edit.defineBefore = defines.render();
    edit.cellsBefore = cells.render();
    MaterialDefineFile newDefines = defines;
    MaterialCellFile newCells = cells;
    std::string label;
    if (action == "set")
    {
        if (args.size() < 3)
        {
            result = "EditorMaterial.set <id> <property> <value>";
            return false;
        }
        std::string old, name;
        if (!defines.value(first, args[1], old))
        {
            result = !defines.has(first) ? "no material " + number(first) : "'" + args[1] + "' is not a material property";
            return false;
        }
        if (!newDefines.setValue(first, args[1], args[2], error))
        {
            result = error;
            return false;
        }
        std::string now;
        newDefines.value(first, args[1], now);
        const bool quoted = _stricmp(args[1].c_str(), "name") == 0;
        defines.value(first, "name", name);
        edit.engineDo = "Material.active " + number(first) + "\nMaterial." + args[1] + " " + (quoted ? "\"" + now + "\"" : now) + "\n";
        edit.engineUndo = "Material.active " + number(first) + "\nMaterial." + args[1] + " " + (quoted ? "\"" + old + "\"" : old) + "\n";
        label = "Set " + args[1] + " of material " + number(first) + " (" + name + ")";
    }
    else if (action == "add" || action == "addAs")
    {
        if (args.empty())
        {
            result = "EditorMaterial.add <name>, or addAs <name> <id> <type>";
            return false;
        }
        int id = action == "addAs" && args.size() > 1 ? std::atoi(args[1].c_str()) : -1;
        const int type = action == "addAs" && args.size() > 2 ? std::atoi(args[2].c_str()) : 1;
        if (id < 0)
        {
            // The retail Add's id: the first free one of 0..254 (createMaterial).
            for (int candidate = 0; candidate < 255 && id < 0; ++candidate)
                if (!defines.has(candidate))
                    id = candidate;
            if (id < 0)
            {
                result = "ids 0 to 254 are all taken: give one (EditorMaterial.addAs <name> <id> <type>)";
                return false;
            }
        }
        if (type < 0 || type > 4)
        {
            result = "a material type is 0 Ground, 1 World, 2 Projectiles, 3 Vehicles or 4 Misc";
            return false;
        }
        if (!newDefines.add(id, args[0], type, error))
        {
            result = error;
            return false;
        }
        edit.engineDo = materialLines(newDefines, id);
        edit.removeUndo.push_back(id);
        label = "Add material " + number(id) + " (" + args[0] + ")";
    }
    else if (action == "remove")
    {
        std::string name;
        if (!defines.value(first, "name", name))
        {
            result = "no material " + number(first);
            return false;
        }
        if (!newDefines.remove(first, error))
        {
            result = error;
            return false;
        }
        const std::vector<std::pair<int, int> > all = cells.cells();
        std::string clear, restore;
        int removed = 0;
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i].first == first || all[i].second == first)
            {
                clear += cellLines(cells, all[i].first, all[i].second, true);
                restore += cellLines(cells, all[i].first, all[i].second, false);
                ++removed;
            }
        newCells.removeCellsOf(first);
        edit.removeDo.push_back(first);
        edit.engineDo = clear;
        edit.engineUndo = materialLines(defines, first) + restore;
        label = "Remove material " + number(first) + " (" + name + ") and its " + number(removed) + " cell"
              + (removed == 1 ? "" : "s");
    }
    else if (action == "addCell" || action == "removeCell")
    {
        if (!validId(first) || !validId(second) || !defines.has(first) || !defines.has(second))
        {
            result = "a cell joins two materials of the file (the writer saves no other)";
            return false;
        }
        if (action == "addCell")
        {
            if (!newCells.add(first, second, error))
            {
                result = error;
                return false;
            }
            edit.engineDo = "MaterialManager.createCell " + number(first) + " " + number(second) + "\n";
            edit.engineUndo = cellLines(newCells, first, second, true);
            label = "Add the cell " + number(first) + " " + number(second);
        }
        else
        {
            if (!cells.has(first, second))
            {
                result = "no cell " + number(first) + " " + number(second);
                return false;
            }
            edit.engineDo = cellLines(cells, first, second, true);
            edit.engineUndo = cellLines(cells, first, second, false);
            newCells.remove(first, second, error);
            label = "Remove the cell " + number(first) + " " + number(second);
        }
    }
    else if (action == "setDamageMod")
    {
        if (args.size() < 3)
        {
            result = "EditorMaterial.setDamageMod <attacker> <defender> <value> (a negative value unsets it)";
            return false;
        }
        std::string old;
        if (!cells.damageMod(first, second, old))
        {
            result = "no cell " + number(first) + " " + number(second) + " (EditorMaterial.addCell first)";
            return false;
        }
        if (!newCells.setDamageMod(first, second, args[2], error))
        {
            result = error;
            return false;
        }
        std::string now;
        newCells.damageMod(first, second, now);
        const std::string select = "MaterialManager.createCell " + number(first) + " " + number(second) + "\n";
        edit.engineDo = select + "MaterialManager.damageMod " + (now.empty() ? std::string("-1") : now) + "\n";
        edit.engineUndo = select + "MaterialManager.damageMod " + (old.empty() ? std::string("-1") : old) + "\n";
        label = "Set the damage modifier of the cell " + number(first) + " " + number(second);
    }
    else if (action == "setTemplate")
    {
        MaterialCellFile::Kind kind = MaterialCellFile::Effect;
        if (args.size() < 5 || !MaterialCellFile::kindFromName(args[2], kind))
        {
            result = "EditorMaterial.setTemplate <attacker> <defender> <effect|decal|sound> <index> <template | \"\">";
            return false;
        }
        const int index = std::atoi(args[3].c_str());
        const std::string name = args[4];
        if (!name.empty())
        {
            dice::hfe::world::IObjectTemplate* t =
                dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
            const std::string className = t ? dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(t->getClassID())
                                            : std::string();
            if (!t || _stricmp(className.c_str(), kTemplateClasses[kind]) != 0)
            {
                result = !t ? "no template named " + name + " (the game would bind it as nothing)"
                            : name + " is of class " + className + ", not " + kTemplateClasses[kind];
                return false;
            }
        }
        std::string old;
        if (!cells.templateAt(first, second, kind, index, old))
        {
            result = "no cell " + number(first) + " " + number(second) + " (EditorMaterial.addCell first)";
            return false;
        }
        if (!newCells.setTemplate(first, second, kind, index, name, error))
        {
            result = error;
            return false;
        }
        const std::string select = "MaterialManager.createCell " + number(first) + " " + number(second) + "\n";
        const std::string row = std::string(kTemplateRows[kind]) + " " + number(index) + " ";
        edit.engineDo = select + row + (name.empty() ? std::string("NoObjectTemplate") : name) + "\n";
        edit.engineUndo = select + row + (old.empty() ? std::string("NoObjectTemplate") : old) + "\n";
        label = std::string(name.empty() ? "Clear the " : "Set the ") + MaterialCellFile::kindName(kind) + " " + number(index)
              + " of the cell " + number(first) + " " + number(second);
    }
    else
    {
        result = "no material action '" + action + "'";
        return false;
    }
    edit.defineAfter = newDefines.render();
    edit.cellsAfter = newCells.render();
    if (edit.defineAfter == edit.defineBefore && edit.cellsAfter == edit.cellsBefore && edit.engineDo.empty()
        && edit.removeDo.empty())
    {
        result = "no change";
        return true;
    }
    return recordMaterialEdit(edit, label, result);
}

bool EditSession::extractCommonArchives(std::string& result)
{
    const std::string mod = modDirectory();
    const std::vector<std::string> zips = commonArchivesOf(mod);
    if (zips.empty())
    {
        result = "the mod's archive scripts mount no archives at Common";
        return false;
    }
    unsigned long long needed = 0;
    for (size_t i = 0; i < zips.size(); ++i)
    {
        unsigned long long size = 0;
        if (!archiveContentSize(zips[i], size, result))
            return false;
        needed += size;
    }
    ULARGE_INTEGER available;
    if (!GetDiskFreeSpaceExA(mod.c_str(), &available, 0, 0)
        || available.QuadPart < needed + needed / 10 + (512ull << 20))
    {
        char text[160];
        std::snprintf(text, sizeof(text), "not enough disk space: the Common archives hold %.1f MB",
                      static_cast<double>(needed) / 1048576.0);
        result = text;
        return false;
    }
    ExtractReport report;
    for (size_t i = 0; i < zips.size(); ++i)
    {
        logLine(LogLevel::Info, "materials: extracting %s", zips[i].c_str());
        if (!extractArchive(zips[i], mod + "/Common", report, result))
            return false;
    }
    char text[240];
    std::snprintf(text, sizeof(text), "extracted %d files (%.1f MB) into %s/Common; %d were there already and kept. "
                  "Start the editor with +looseCommon 1 to edit the material files", report.files,
                  static_cast<double>(report.bytes) / 1048576.0, mod.c_str(), report.kept);
    result = text;
    logLine(LogLevel::Info, "materials: %s", result.c_str());
    return true;
}

} // namespace bf2editor
