// Edits template statements in place while preserving unrelated file contents.
// Activates the template before querying bindings; get/set properties are applied live and read back.
// Additive commands and removed lines take effect on reload to avoid duplicate engine state.
// Properties without a supported engine binding are refused.
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/LevelDocument.h"
#include "Framework/EditorLog.h"
#include "Framework/Matrix4.h"
#include "Framework/ObjectArchives.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SourceControl.h"
#include "Framework/TemplateClone.h"
#include "Framework/TemplateFile.h"

#include "Core/ConsoleObject.h"
#include "Core/ConsoleObjects.h"
#include "Core/SettingsRepostitory.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/IObjectTemplate.h"
#include "IO/Console/Console.h"
#include "IO/File/FileManager.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "Scene/Object/ObjectManager.h"
#include "Core/Ids.h"
#include "Core/SmartItf.h"
#include "Game/Objects/IObjectTemplateComponent.h"
#include "Game/Objects/Base/BundleTemplate.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>

namespace bf2editor
{

namespace
{

// One engine console line with full access; its answer, trimmed (EditSession.cpp's engineLine).
std::string engineLine(const std::string& line)
{
    std::string answer;
    dice::hfe::io::mainConsole->executeLine(line + "\n", answer, false, false, false, false, true);
    const size_t begin = answer.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    return answer.substr(begin, answer.find_last_not_of(" \t\r\n") - begin + 1);
}

// The template made active, then each line run through the engine; false with the first refusal (every
// line still runs).
bool runTemplateLines(const std::string& className, const std::string& name, const std::vector<std::string>& lines,
                      std::string& error)
{
    const std::string activated = engineLine("ObjectTemplate.activeSafe " + className + " " + name);
    if (!activated.empty())
    {
        error = "the engine refused ObjectTemplate.activeSafe " + className + " " + name + ": " + activated;
        return false;
    }
    std::string refusal;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const std::string answer = engineLine(lines[i]);
        if (!answer.empty() && refusal.empty())
            refusal = lines[i] + ": " + answer;
    }
    if (!refusal.empty())
    {
        error = "the engine refused " + refusal;
        return false;
    }
    return true;
}

// A new child template made (`ObjectTemplate.create <class> <name>`, `createdInEditor 1`), stamped with its
// parent's file as the engine stamps a template a file makes, and added to the parent at `transform`.
bool makeNewChild(dice::hfe::world::IBundleTemplate* bundle, const std::string& className, const std::string& name,
                  const dice::hfe::Mat4& transform, const std::string& file, std::string& error)
{
    const std::string answer = engineLine("ObjectTemplate.create " + className + " " + name);
    if (!dice::hfe::world::objectTemplateManager->getTemplate(name))
    {
        error = "the engine made no " + className + " named " + name + " (" + answer + ")";
        return false;
    }
    engineLine("ObjectTemplate.createdInEditor 1");
    dice::hfe::world::objectTemplateManager->setWorkingFolderAndFile(name, file);
    bundle->addTemplate(name);
    bundle->setTransformation(transform, bundle->getNoTemplates() - 1);
    return true;
}

// A template renamed in the engine: its own name (setName, as retail's rename did first), the manager's
// lookup (renameTemplate - in 1.50 it only adds the new name, the old one stays), and each bundle holding it
// as a child (renameChildTemplate). Returns how many bundles followed.
int renameInEngine(const std::string& from, const std::string& to)
{
    dice::hfe::world::IObjectTemplate* objectTemplate = dice::hfe::world::objectTemplateManager->getTemplate(from);
    if (!objectTemplate)
        return 0;
    objectTemplate->setName(to);
    dice::hfe::world::objectTemplateManager->renameTemplate(from, to);
    int parents = 0;
    std::list<dice::hfe::world::IObjectTemplate*>* all = dice::hfe::world::objectTemplateManager->getGlobalTemplateList();
    if (!all)
        return 0;
    for (std::list<dice::hfe::world::IObjectTemplate*>::iterator it = all->begin(); it != all->end(); ++it)
    {
        dice::hfe::world::IObjectTemplate* candidate = *it;
        if (!candidate)
            continue;
        dice::hfe::world::IBundleTemplate* bundle = static_cast<dice::hfe::world::IBundleTemplate*>(
            candidate->queryInterface(dice::hfe::world::IID_IBundleTemplate));
        if (!bundle)
            continue;
        bool holds = false;
        for (int i = 0; i < bundle->getNoTemplates() && !holds; ++i)
            holds = _stricmp(bundle->getTemplate(i).c_str(), from.c_str()) == 0;
        if (!holds)
            continue;
        bundle->renameChildTemplate(from, to);
        ++parents;
    }
    return parents;
}

// A template made again from its files: deleted, then every ObjectTemplate line of its blocks run in file
// order (its defining file first), as a level load runs them; its defining file stamped again. `refused`
// gets the lines the engine answered.
bool rebuildFromFiles(TemplateDocuments& documents, const std::vector<size_t>& files, const std::string& name,
                      const std::string& workingFile, std::vector<std::string>& refused, std::string& error,
                      bool deleteFirst = true)
{
    // Not deleted first (a revert of a template level objects use): the lines only run again over it.
    if (deleteFirst)
        dice::hfe::world::objectTemplateManager->deleteTemplate(name);
    for (size_t f = 0; f < files.size(); ++f)
    {
        const TemplateDocument& document = documents.document(files[f]);
        if (!document.exists)
            continue;
        const std::vector<TemplateBlock>& blocks = document.file.blocks();
        for (size_t b = 0; b < blocks.size(); ++b)
        {
            if (_stricmp(blocks[b].name.c_str(), name.c_str()) != 0)
                continue;
            for (size_t i = blocks[b].first; i < blocks[b].end; ++i)
            {
                const std::string& raw = document.file.line(i);
                const size_t begin = raw.find_first_not_of(" \t");
                if (begin == std::string::npos || _strnicmp(raw.c_str() + begin, "ObjectTemplate.", 15) != 0)
                    continue;
                const size_t end = raw.find_last_not_of(" \t");
                const std::string text = raw.substr(begin, end - begin + 1);
                // An opening answers the template it made or found (a pointer); a property line answers nothing
                // unless the engine refused it.
                const bool opening = i == blocks[b].first;
                const std::string answer = engineLine(text);
                if (!answer.empty() && !opening)
                    refused.push_back(text + " (" + answer + ")");
            }
        }
    }
    if (!dice::hfe::world::objectTemplateManager->getTemplate(name))
    {
        error = "its files made no template " + name;
        return false;
    }
    dice::hfe::world::objectTemplateManager->setWorkingFolderAndFile(name, workingFile);
    return true;
}

dice::hfe::world::IBundleTemplate* bundleOf(dice::hfe::world::IObjectTemplate* objectTemplate);

// Does the template tree under `tree` hold `wanted` (itself or a child, at any depth)?
bool templateHolds(const dice::hfe::world::IObjectTemplate* tree, const dice::hfe::world::IObjectTemplate* wanted, int depth)
{
    if (!tree || depth > 16)
        return false;
    if (tree == wanted)
        return true;
    dice::hfe::world::IBundleTemplate* bundle = bundleOf(const_cast<dice::hfe::world::IObjectTemplate*>(tree));
    if (!bundle)
        return false;
    for (int i = 0; i < bundle->getNoTemplates(); ++i)
        if (templateHolds(dice::hfe::world::objectTemplateManager->getTemplate(bundle->getTemplate(i)), wanted, depth + 1))
            return true;
    return false;
}

// "addTemplate#1" -> addTemplate, 1; "geometry" -> geometry, -1.
bool splitIndex(const std::string& spec, std::string& property, int& which)
{
    const size_t hash = spec.find('#');
    property = spec.substr(0, hash);
    which = -1;
    if (hash == std::string::npos)
        return !property.empty();
    char* end = 0;
    const long value = std::strtol(spec.c_str() + hash + 1, &end, 10);
    if (hash + 1 >= spec.size() || !end || *end != 0 || value < 0)
        return false;
    which = static_cast<int>(value);
    return !property.empty();
}

dice::hfe::world::IBundleTemplate* bundleOf(dice::hfe::world::IObjectTemplate* objectTemplate)
{
    return objectTemplate ? static_cast<dice::hfe::world::IBundleTemplate*>(
                                objectTemplate->queryInterface(dice::hfe::world::IID_IBundleTemplate))
                          : 0;
}

// A position or a rotation as the object files print one: up to four decimals, zero as 0.
std::string vectorText(const dice::hfe::Vec3& p)
{
    const float v[3] = { p.x, p.y, p.z };
    std::string out;
    for (int i = 0; i < 3; ++i)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%.4f", v[i]);
        std::string number = text;
        while (!number.empty() && number[number.size() - 1] == '0')
            number.erase(number.size() - 1);
        if (!number.empty() && number[number.size() - 1] == '.')
            number.erase(number.size() - 1);
        if (number == "-0" || number.empty())
            number = "0";
        out += (i ? "/" : "") + number;
    }
    return out;
}

// The index of the first (or last) child of that name; -1 for none.
int childIndex(dice::hfe::world::IBundleTemplate* bundle, const std::string& child, bool last)
{
    int found = -1;
    for (int i = 0; i < bundle->getNoTemplates(); ++i)
    {
        if (_stricmp(bundle->getTemplate(i).c_str(), child.c_str()) != 0)
            continue;
        found = i;
        if (!last)
            break;
    }
    return found;
}

bool structuralLine(const std::string& property)
{
    const char* const structural[] = { "create", "activeSafe", "active", "createComponent", "deleteComponent" };
    for (size_t i = 0; i < sizeof(structural) / sizeof(structural[0]); ++i)
        if (_stricmp(property.c_str(), structural[i]) == 0)
            return true;
    return false;
}

// The spelling a template's files already give a property. The console resolves `setX` onto the property `x`
// (ConsoleObjects::getConsoleObject strips a set prefix when `setX` itself is not bound), and the stock files
// spell most such rows that way - 644 setNetworkableInfo lines against 23 networkableInfo - so a line of the
// other spelling that resolves to the same binding is the one an edit changes. The template must be active
// (the lookup is class-gated); `binding` is what `property` resolves to. `property` itself when no file
// spells it otherwise.
std::string fileSpelling(TemplateDocuments& documents, const std::string& templateName, const std::string& property,
                         dice::hfe::ConsoleObject* binding)
{
    std::vector<size_t> files;
    std::string error;
    if (!binding || !dice::hfe::mainConsoleObjects || !documents.filesOf(templateName, files, error))
        return property;
    for (size_t f = 0; f < files.size(); ++f)
    {
        const TemplateDocument& document = documents.document(files[f]);
        if (!document.exists)
            continue;
        const std::vector<TemplatePropertyLine> lines = document.file.properties(templateName);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            const std::string& spelt = lines[i].property;
            if (_stricmp(spelt.c_str(), property.c_str()) == 0)
                return property;
            const bool prefixed = spelt.size() == property.size() + 3 && _strnicmp(spelt.c_str(), "set", 3) == 0
                                  && _stricmp(spelt.c_str() + 3, property.c_str()) == 0;
            const bool stripped = property.size() == spelt.size() + 3 && _strnicmp(property.c_str(), "set", 3) == 0
                                  && _stricmp(property.c_str() + 3, spelt.c_str()) == 0;
            if ((prefixed || stripped) && dice::hfe::mainConsoleObjects->getConsoleObject("ObjectTemplate", spelt) == binding)
                return spelt;
        }
    }
    return property;
}

} // namespace

bool EditSession::objectTemplateFiles(const std::string& name, std::string& result)
{
    std::vector<size_t> files;
    if (!m_templateDocuments.filesOf(name, files, result))
        return false;
    result.clear();
    for (size_t i = 0; i < files.size(); ++i)
    {
        const TemplateDocument& document = m_templateDocuments.document(files[i]);
        char blocks[64];
        std::snprintf(blocks, sizeof(blocks), ", %u blocks of %s", static_cast<unsigned>(document.file.blocksOf(name).size()),
                      name.c_str());
        result += (i ? "; " : "") + document.path
                + (!document.exists ? std::string(" (none)")
                                    : document.archived ? std::string(" (from a mounted archive: read-only)")
                                                        : " (loose, saved to " + document.writePath + ")")
                + (document.exists ? std::string(blocks) : std::string()) + (document.modified() ? ", modified" : "");
    }
    return true;
}

bool EditSession::objectTemplateLines(const std::string& name, std::string& result)
{
    std::vector<size_t> files;
    if (!m_templateDocuments.filesOf(name, files, result))
        return false;
    result.clear();
    for (size_t i = 0; i < files.size(); ++i)
    {
        const TemplateDocument& document = m_templateDocuments.document(files[i]);
        const std::vector<TemplatePropertyLine> lines = document.file.properties(name);
        if (lines.empty())
            continue;
        result += (result.empty() ? "" : " || ") + document.path + ":";
        for (size_t k = 0; k < lines.size(); ++k)
            result += (k ? "; " : " ") + lines[k].property + (lines[k].value.empty() ? "" : " " + lines[k].value);
    }
    if (result.empty())
        result = "no lines of " + name + " in its files";
    return true;
}

bool EditSession::objectTemplateEngineValue(const std::string& name, const std::string& property, std::string& result)
{
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate || !dice::hfe::mainConsoleObjects)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string className =
        dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(objectTemplate->getClassID());
    const std::string activated = engineLine("ObjectTemplate.activeSafe " + className + " " + objectTemplate->getName());
    if (!activated.empty())
    {
        result = activated;
        return false;
    }
    dice::hfe::ConsoleObject* binding = dice::hfe::mainConsoleObjects->getConsoleObject("ObjectTemplate", property);
    if (!binding || !binding->isSetGet())
    {
        result = "ObjectTemplate." + property + " is not a property the engine answers for " + objectTemplate->getName();
        return false;
    }
    result = engineLine("ObjectTemplate." + property);
    return true;
}

namespace
{

// Rejects particle/decal textures absent from the mounted mod before engine loaders can
// abort. An empty value preserves the current texture and is allowed.
bool engineSafeValue(const std::string& property, const std::string& value, std::string& error)
{
    std::string bare = property;
    for (size_t i = 0; i < bare.size(); ++i)
        bare[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(bare[i])));
    if (bare.compare(0, 3, "set") == 0)
        bare = bare.substr(3);
    if ((bare != "texturename" && bare != "decaltexturename") || value.empty() || !dice::hfe::io::fileManager)
        return true;
    if (dice::hfe::io::fileManager->fileExists(value + ".dds"))
        return true;
    error = "no texture " + value + ".dds in the mod: the engine stops on a texture it cannot find, so the value is refused";
    return false;
}

} // namespace

bool EditSession::setObjectTemplateProperty(const std::string& name, const std::string& spec, const std::string& value,
                                            std::string& result)
{
    std::string property;
    int which = -1;
    if (!splitIndex(spec, property, which) || structuralLine(property))
    {
        result = "'" + spec + "' is not a property line the object editor sets (a property, or property#index for one "
                 "of several lines)";
        return false;
    }
    if (!engineSafeValue(property, value, result))
        return false;
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate || !dice::hfe::mainConsoleObjects)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    const std::string className =
        dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(objectTemplate->getClassID());
    const std::string activated = engineLine("ObjectTemplate.activeSafe " + className + " " + realName);
    dice::hfe::ConsoleObject* binding =
        activated.empty() ? dice::hfe::mainConsoleObjects->getConsoleObject("ObjectTemplate", property) : 0;
    if (!binding)
    {
        result = activated.empty()
                     ? "the 1.50 engine has no ObjectTemplate." + property + " for a " + className
                           + " (unbound, or a component the template does not have), so the game would ignore the line: refused"
                     : "the engine could not make " + realName + " active: " + activated;
        return false;
    }
    // The engine is spoken to by the spelling given (the console resolves either); the files by the one they
    // already use for this binding, so its line changes instead of a second one being added.
    const std::string engineProperty = property;
    property = fileSpelling(m_templateDocuments, realName, property, binding);
    std::string error;
    const int target = m_templateDocuments.editTarget(realName, property, error);
    if (target < 0)
    {
        result = error;
        return false;
    }
    TemplateDocument& document = m_templateDocuments.document(static_cast<size_t>(target));
    TemplateEdit edit;
    edit.file = static_cast<size_t>(target);
    edit.before = document.file.render();
    edit.templateName = realName;
    edit.className = className;
    edit.property = engineProperty;
    edit.engine = binding->isSetGet() && which < 0;
    if (document.file.blocksOf(realName).empty())
        document.file.appendBlock(className, realName);
    if (!document.file.setProperty(realName, property, value, which, error))
    {
        document.file.parse(edit.before);
        result = error;
        return false;
    }
    if (edit.engine)
    {
        // The engine's own lines first, as the retail tweaker ran them; its answer to a set is empty.
        const std::string activated = engineLine("ObjectTemplate.activeSafe " + className + " " + realName);
        edit.engineBefore = engineLine("ObjectTemplate." + engineProperty);
        const std::string answer = activated.empty() ? engineLine("ObjectTemplate." + engineProperty + " " + value) : activated;
        if (!answer.empty())
        {
            document.file.parse(edit.before);
            result = "the engine refused ObjectTemplate." + property + " " + value + ": " + answer;
            return false;
        }
        edit.engineAfter = engineLine("ObjectTemplate." + engineProperty);
    }
    edit.after = document.file.render();
    const std::string label = "Set " + spec + " of " + realName;
    recordTemplateEdit(edit, label);
    result = label + " in " + document.path
           + (edit.engine ? std::string() : std::string(" (the file only: the engine adds such lines, so it takes the "
                                                        "change when the level loads again)"));
    return true;
}

bool EditSession::removeObjectTemplateProperty(const std::string& name, const std::string& spec, std::string& result)
{
    std::string property;
    int which = -1;
    if (!splitIndex(spec, property, which) || structuralLine(property))
    {
        result = "'" + spec + "' is not a property line the object editor removes";
        return false;
    }
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    std::string error;
    const int target = m_templateDocuments.editTarget(realName, property, error);
    if (target < 0)
    {
        result = error;
        return false;
    }
    TemplateDocument& document = m_templateDocuments.document(static_cast<size_t>(target));
    TemplateEdit edit;
    edit.file = static_cast<size_t>(target);
    edit.before = document.file.render();
    edit.engine = false;
    edit.templateName = realName;
    edit.property = property;
    if (!document.file.removeProperty(realName, property, which, error))
    {
        result = error + " (in " + document.path + ")";
        return false;
    }
    edit.after = document.file.render();
    const int id = m_nextTemplateEdit++;
    m_templateEdits[id] = edit;
    HistoryEntry entry;
    entry.label = "Remove " + spec + " from " + realName;
    char line[80];
    std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d after\n", id);
    entry.doScript = line;
    std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d before\n", id);
    entry.undoScript = line;
    m_history.record(entry);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + " in " + document.path + " (the engine keeps its value until the level loads again)";
    return true;
}

bool EditSession::resetAnchor(std::string& result)
{
    // The anchor line the template's file had when it was last saved, put back as one undoable edit - or,
    // when it had none, the line taken out and the engine's anchor set to 0/0/0 (what a template without
    // the line holds), in the same edit.
    dice::hfe::world::IObject* o = object(m_selection.primary());
    if (!o || !o->getTemplate())
    {
        result = "nothing is selected";
        return false;
    }
    const std::string name = o->getTemplate()->getName();
    std::string error;
    const int target = m_templateDocuments.editTarget(name, "anchor", error);
    if (target < 0)
    {
        result = error;
        return false;
    }
    TemplateDocument& document = m_templateDocuments.document(static_cast<size_t>(target));
    TemplateFile saved;
    saved.parse(document.savedText);
    std::string value, now;
    const bool had = saved.value(name, "anchor", value);
    const bool has = document.file.value(name, "anchor", now);
    if (had ? (has && now == value) : !has)
    {
        result = "the anchor of " + name + " is as saved already";
        return true;
    }
    if (had)
        return setObjectTemplateProperty(name, "anchor", value, result);
    const std::string className =
        dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(o->getTemplate()->getClassID());
    TemplateEdit edit;
    edit.file = static_cast<size_t>(target);
    edit.before = document.file.render();
    edit.engine = true;
    edit.templateName = name;
    edit.className = className;
    edit.property = "anchor";
    const std::string activated = engineLine("ObjectTemplate.activeSafe " + className + " " + name);
    if (!activated.empty())
    {
        result = activated;
        return false;
    }
    edit.engineBefore = engineLine("ObjectTemplate.anchor");
    if (!document.file.removeProperty(name, "anchor", -1, error))
    {
        result = error;
        return false;
    }
    const std::string answer = engineLine("ObjectTemplate.anchor 0/0/0");
    if (!answer.empty())
    {
        document.file.parse(edit.before);
        result = "the engine refused ObjectTemplate.anchor 0/0/0: " + answer;
        return false;
    }
    edit.engineAfter = "0/0/0";
    edit.after = document.file.render();
    const int id = m_nextTemplateEdit++;
    m_templateEdits[id] = edit;
    HistoryEntry entry;
    entry.label = "Reset the anchor of " + name;
    char line[80];
    std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d after\n", id);
    entry.doScript = line;
    std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d before\n", id);
    entry.undoScript = line;
    m_history.record(entry);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + " (its line taken out of " + document.path + ")";
    return true;
}

bool EditSession::applyObjectTemplateEdit(int id, const std::string& state, std::string& error)
{
    std::map<int, TemplateEdit>::const_iterator found = m_templateEdits.find(id);
    if (found == m_templateEdits.end() || (state != "before" && state != "after"))
    {
        error = "no template edit with that id, or the state is not before or after";
        return false;
    }
    const TemplateEdit& edit = found->second;
    const bool after = state == "after";
    m_templateDocuments.document(edit.file).file.parse(after ? edit.after : edit.before);
    if (edit.childOp == 6)
    {
        // The class change, one way or the other: every file as it was (or is to be), then the template made
        // again from them.
        for (size_t i = 0; i < edit.extraFiles.size(); ++i)
            m_templateDocuments.document(edit.extraFiles[i]).file.parse(after ? edit.extraAfter[i] : edit.extraBefore[i]);
        std::vector<size_t> files(1, edit.file);
        files.insert(files.end(), edit.extraFiles.begin(), edit.extraFiles.end());
        std::vector<std::string> refused;
        return rebuildFromFiles(m_templateDocuments, files, edit.templateName, edit.workingFile, refused, error);
    }
    if (edit.childOp == 5)
    {
        // The rename, one way or the other: the engine's name and its parents' child lists, and the level.
        const std::string from = after ? edit.templateName : edit.child;
        const std::string to = after ? edit.child : edit.templateName;
        renameInEngine(from, to);
        if (m_document)
            m_document->renameObjectRecords(from, to);
        return true;
    }
    if (edit.childOp == 4)
    {
        // A new child template: made again with its lines and added, or taken off and deleted.
        dice::hfe::world::IBundleTemplate* bundle =
            bundleOf(dice::hfe::world::objectTemplateManager->getTemplate(edit.templateName));
        if (!bundle)
        {
            error = "no bundle template named " + edit.templateName;
            return false;
        }
        if (after)
            return makeNewChild(bundle, edit.childClass, edit.child, edit.childTransform,
                                m_templateDocuments.document(edit.file).path, error);
        const int index = childIndex(bundle, edit.child, true);
        if (index >= 0)
            bundle->removeTemplate(index);
        dice::hfe::world::objectTemplateManager->deleteTemplate(edit.child);
        return true;
    }
    if (edit.childOp != 0)
    {
        dice::hfe::world::IBundleTemplate* bundle =
            bundleOf(dice::hfe::world::objectTemplateManager->getTemplate(edit.templateName));
        if (!bundle)
        {
            error = "no bundle template named " + edit.templateName;
            return false;
        }
        // Added and put back go on at the end; taken out and undone-add take off the last of that name; a move
        // sets the matrix of the child it moved (undo and redo find the list as the move left it).
        const bool add = (edit.childOp == 1) == after;
        if (edit.childOp == 3)
        {
            if (edit.childAt < 0 || edit.childAt >= bundle->getNoTemplates()
                || _stricmp(bundle->getTemplate(edit.childAt).c_str(), edit.child.c_str()) != 0)
            {
                error = edit.templateName + "'s child " + edit.child + " is no longer where it was moved: refused";
                return false;
            }
            bundle->setTransformation(after ? edit.childTransform : edit.childBefore, edit.childAt);
        }
        else if (add)
        {
            bundle->addTemplate(edit.child);
            bundle->setTransformation(edit.childTransform, bundle->getNoTemplates() - 1);
        }
        else
        {
            const int index = childIndex(bundle, edit.child, true);
            if (index < 0)
            {
                error = edit.templateName + " has no child " + edit.child + " to take out";
                return false;
            }
            bundle->removeTemplate(index);
        }
    }
    if (edit.componentOp != 0)
    {
        // Added: createComponent again, or deleteComponent to undo it; taken out (or replaced): deleteComponent
        // again, or createComponent with its section's lines run again to undo it.
        std::vector<std::string> lines;
        if (after)
            lines.push_back(edit.componentOp == 1 ? "ObjectTemplate.createComponent " + edit.componentAdded
                                                  : "ObjectTemplate.deleteComponent " + edit.componentRemoved);
        else
        {
            if (edit.componentOp == 1)
                lines.push_back("ObjectTemplate.deleteComponent " + edit.componentAdded);
            if (!edit.componentRemoved.empty())
            {
                lines.push_back("ObjectTemplate.createComponent " + edit.componentRemoved);
                lines.insert(lines.end(), edit.componentLines.begin(), edit.componentLines.end());
            }
        }
        if (!runTemplateLines(edit.className, edit.templateName, lines, error))
            return false;
    }
    if (edit.engine)
    {
        const std::string activated = engineLine("ObjectTemplate.activeSafe " + edit.className + " " + edit.templateName);
        const std::string answer = activated.empty()
            ? engineLine("ObjectTemplate." + edit.property + " " + (after ? edit.engineAfter : edit.engineBefore))
            : activated;
        if (!answer.empty())
        {
            error = "the engine refused ObjectTemplate." + edit.property + ": " + answer;
            return false;
        }
    }
    return true;
}

void EditSession::planObjectTemplates(SavePlan& plan)
{
    const std::vector<size_t> modified = m_templateDocuments.modified();
    if (modified.empty())
        return;
    SavePart& part = plan.part("object templates");
    std::string answer;
    for (size_t i = 0; i < modified.size(); ++i)
    {
        std::string error;
        if (!m_templateDocuments.planSave(modified[i], part, error))
        {
            part.refuse("object template NOT saved: " + error);
            return;
        }
        answer += (i ? ", " : "saved the object template files ") + m_templateDocuments.document(modified[i]).writePath;
    }
    part.onReport([answer](const SavePart&) { return answer; });
}

bool EditSession::saveObjectTemplates(std::string& result)
{
    SavePlan plan;
    planObjectTemplates(plan);
    plan.dropUnchanged();
    return writeSave(plan, "EditorObjectTemplate.save", result) && !plan.anyRefused();
}

// The mod's object archives (WP7 step 4)

namespace
{

std::string modDirectory()
{
    return dice::hfe::settings ? dice::hfe::settings->stringGet("GSModDirectory", "") : std::string();
}

// The zips the mod's archive scripts mount at Objects (Objects_*.zip and Booster_*.zip in bf2).
std::vector<std::string> objectArchivesOf(const std::string& mod)
{
    std::vector<std::string> zips;
    const char* const scripts[] = { "/ServerArchives.con", "/ClientArchives.con" };
    for (size_t i = 0; i < 2; ++i)
    {
        std::string text, error;
        if (!ObjectLibrary::readTextFile(mod + scripts[i], text, error))
            continue;
        const std::vector<std::string> found = archivesMountedAt(text, "Objects");
        for (size_t k = 0; k < found.size(); ++k)
            zips.push_back(mod + "/" + found[k]);
    }
    return zips;
}

std::string folderKey(const std::string& path)
{
    char full[MAX_PATH];
    std::string key = GetFullPathNameA(path.c_str(), MAX_PATH, full, 0) ? std::string(full) : path;
    for (size_t i = 0; i < key.size(); ++i)
        key[i] = key[i] == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(key[i])));
    while (!key.empty() && key[key.size() - 1] == '\\')
        key.erase(key.size() - 1);
    return key;
}

} // namespace

bool EditSession::objectArchivesStatus(std::string& result)
{
    const std::string mod = modDirectory();
    const std::vector<std::string> zips = objectArchivesOf(mod);
    result = m_objectArchivesMounted ? "the object archives are mounted (templates they hold are read-only): "
                                     : "loose objects (+looseObjects 1): the object archives are not mounted: ";
    for (size_t i = 0; i < zips.size(); ++i)
        result += (i ? ", " : "") + zips[i];
    const DWORD attributes = GetFileAttributesA((mod + "/Objects").c_str());
    result += (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
                  ? "; the loose folder is " + mod + "/Objects"
                  : "; the mod has no loose Objects folder";
    return true;
}

bool EditSession::extractObjectArchives(std::string& result)
{
    const std::string mod = modDirectory();
    const std::vector<std::string> zips = objectArchivesOf(mod);
    if (zips.empty())
    {
        result = "the mod's archive scripts mount no archives at Objects";
        return false;
    }
    // All of them, uncompressed, must fit, with room to spare.
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
        std::snprintf(text, sizeof(text), "not enough disk space: the object archives hold %.1f GB, and %.1f GB are free",
                      static_cast<double>(needed) / 1073741824.0, static_cast<double>(available.QuadPart) / 1073741824.0);
        result = text;
        return false;
    }
    ExtractReport report;
    for (size_t i = 0; i < zips.size(); ++i)
    {
        logLine(LogLevel::Info, "objects: extracting %s", zips[i].c_str());
        if (!extractArchive(zips[i], mod + "/Objects", report, result))
            return false;
    }
    char text[200];
    std::snprintf(text, sizeof(text), "extracted %d files (%.1f MB) into %s/Objects; %d were there already and kept. "
                  "Start the editor with +looseObjects 1 to edit templates", report.files,
                  static_cast<double>(report.bytes) / 1048576.0, mod.c_str(), report.kept);
    result = text;
    logLine(LogLevel::Info, "objects: %s", result.c_str());
    return true;
}

bool EditSession::packObjectArchives(const std::string& modFolder, std::string& result)
{
    const std::string mod = modDirectory();
    if (modFolder.empty())
    {
        result = "EditorObjects.pack takes the mod folder to write Objects_client.zip and Objects_server.zip into";
        return false;
    }
    if (m_objectArchivesMounted && folderKey(modFolder) == folderKey(mod))
    {
        result = "the running mod's object archives are mounted: packing over them is refused (start with "
                 "+looseObjects 1, or pack into another mod folder)";
        return false;
    }
    if (!m_templateDocuments.modified().empty())
    {
        result = "object templates have unsaved changes: save first (the pack takes the files as saved)";
        return false;
    }
    ObjectPackReport report;
    if (!bf2editor::packObjectArchives(mod + "/Objects", modFolder, report, result))
        return false;
    char text[240];
    std::snprintf(text, sizeof(text), "packed %s/Objects: Objects_client.zip %d files (%llu bytes), Objects_server.zip %d "
                  "files (%llu bytes), %d left out, into ", mod.c_str(), report.clientFiles, report.clientBytes,
                  report.serverFiles, report.serverBytes, report.left);
    result = text + modFolder;
    logLine(LogLevel::Info, "objects: %s", result.c_str());
    return true;
}

// A bundle template's children

namespace
{

// A child's rotation as ObjectTemplate.setRotation takes it: BundleTemplate::setRotation hands the three
// angles to setRotationZXY negated and in reverse order, and BundleTemplate::getRotation reads them back
// the same way.
dice::hfe::Vec3 rotationOf(const dice::hfe::Mat4& m)
{
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    dice::hfe::getRotationZXY(m, a, b, c);
    dice::hfe::Vec3 r;
    r.x = -c;
    r.y = -b;
    r.z = -a;
    return r;
}

void setRotation(dice::hfe::Mat4& m, const dice::hfe::Vec3& r)
{
    dice::hfe::setRotationZXY(m, -r.z, -r.y, -r.x);
}

// BundleTemplate::makeMemberScript's test for writing a line: beyond 0.001 for a position, 0.01 for a
// rotation.
bool beyond(const dice::hfe::Vec3& v, float tolerance)
{
    return std::fabs(v.x) > tolerance || std::fabs(v.y) > tolerance || std::fabs(v.z) > tolerance;
}

// "x/y/z" as a console Vec3; a missing number is 0.
dice::hfe::Vec3 vectorOf(const std::string& text)
{
    dice::hfe::Vec3 v;
    v.x = v.y = v.z = 0.0f;
    std::sscanf(text.c_str(), "%f/%f/%f", &v.x, &v.y, &v.z);
    return v;
}

// A child's matrix as its file lines build it: the identity, the setPosition, the setRotation.
dice::hfe::Mat4 transformOf(const TemplateChildLines& lines)
{
    dice::hfe::Mat4 m = dice::hfe::Mat4::getIdentity();
    const dice::hfe::Vec3 p = vectorOf(lines.position);
    m.m[12] = p.x;
    m.m[13] = p.y;
    m.m[14] = p.z;
    if (lines.rotationLine >= 0)
        setRotation(m, vectorOf(lines.rotation));
    return m;
}

bool sameTransform(const dice::hfe::Mat4& a, const dice::hfe::Mat4& b)
{
    for (int i = 0; i < 15; ++i)
        if (i % 4 != 3 && std::fabs(a.m[i] - b.m[i]) > 0.002f)
            return false;
    return true;
}

// The index of the n-th child of that name (0: the first); -1 for none.
int childIndexAt(dice::hfe::world::IBundleTemplate* bundle, const std::string& child, int occurrence)
{
    for (int i = 0; i < bundle->getNoTemplates(); ++i)
        if (_stricmp(bundle->getTemplate(i).c_str(), child.c_str()) == 0 && occurrence-- == 0)
            return i;
    return -1;
}

// The file line of a child the engine holds - the n-th child of that name, with that matrix. The engine
// ran the template's files in order (the defining file, then its .tweak), each addTemplate line appending
// one child, so the n-th child of a name is the n-th addTemplate line of that name - until an undone
// removal has put a child back at the end of the engine's list. The child's position and rotation,
// compared with each line's, pick the line then. False when no file of the template adds that child.
bool findChildLine(TemplateDocuments& documents, const std::vector<size_t>& files, const std::string& templateName,
                   const std::string& child, int occurrence, const dice::hfe::Mat4& transform, size_t& file,
                   size_t& which)
{
    std::vector<size_t> candidateFile;
    std::vector<size_t> candidateChild;
    std::vector<bool>   candidateSame;
    for (size_t f = 0; f < files.size(); ++f)
    {
        const TemplateDocument& document = documents.document(files[f]);
        if (!document.exists)
            continue;
        const std::vector<TemplateChildLines> list = document.file.children(templateName);
        for (size_t k = 0; k < list.size(); ++k)
        {
            if (_stricmp(list[k].child.c_str(), child.c_str()) != 0)
                continue;
            candidateFile.push_back(files[f]);
            candidateChild.push_back(k);
            candidateSame.push_back(sameTransform(transformOf(list[k]), transform));
        }
    }
    const size_t count = candidateFile.size();
    if (count == 0)
        return false;
    const size_t nth = occurrence >= 0 && static_cast<size_t>(occurrence) < count ? static_cast<size_t>(occurrence)
                                                                                   : count - 1;
    size_t pick = candidateSame[nth] ? nth : count;
    for (size_t k = 0; k < count && pick == count; ++k)
        if (candidateSame[k])
            pick = k;
    if (pick == count)
        pick = nth;
    file = candidateFile[pick];
    which = candidateChild[pick];
    return true;
}

// A child named `name` or `name#n` of a bundle template, found in the engine's list and in the template's
// files (a file an archive holds is refused).
struct ChildTarget
{
    dice::hfe::world::IBundleTemplate* bundle;
    std::string templateName;           // as the engine spells it
    std::string child;                  // likewise
    int         index;                  // in the engine's list
    size_t      file;                   // the document whose block adds it
    size_t      which;                  // its index in that block's children
};

bool findChildTarget(TemplateDocuments& documents, const std::string& name, const std::string& spec, ChildTarget& target,
                     std::string& error)
{
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    target.bundle = bundleOf(objectTemplate);
    if (!target.bundle)
    {
        error = objectTemplate ? name + " is not a bundle: it has no children" : "no template named " + name;
        return false;
    }
    target.templateName = objectTemplate->getName();
    std::string child;
    int occurrence = -1;
    if (!splitIndex(spec, child, occurrence))
    {
        error = "'" + spec + "' names no child: a template name, or name#n for the n-th child of that name";
        return false;
    }
    if (occurrence < 0)
        occurrence = 0;
    target.index = childIndexAt(target.bundle, child, occurrence);
    if (target.index < 0)
    {
        error = target.templateName + " has no child " + spec;
        return false;
    }
    target.child = target.bundle->getTemplate(target.index);
    std::vector<size_t> files;
    if (!documents.filesOf(target.templateName, files, error))
        return false;
    if (!findChildLine(documents, files, target.templateName, child, occurrence,
                       target.bundle->getTransformation(target.index), target.file, target.which))
    {
        error = "no file of " + target.templateName + " adds the child " + child + " (a component or the engine made it): "
                "refused";
        return false;
    }
    if (documents.document(target.file).archived)
    {
        error = documents.document(target.file).path + " is read from a mounted archive, so the change could not be "
                "saved: refused";
        return false;
    }
    return true;
}

} // namespace

void EditSession::recordTemplateEdit(const TemplateEdit& edit, const std::string& label)
{
    const int id = m_nextTemplateEdit++;
    m_templateEdits[id] = edit;
    if (m_templateBatching)
    {
        m_templateBatch.push_back(id);
        logLine(LogLevel::Info, "edit (in a batch): %s", label.c_str());
        return;
    }
    HistoryEntry entry;
    entry.label = label;
    char line[80];
    std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d after\n", id);
    entry.doScript = line;
    std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d before\n", id);
    entry.undoScript = line;
    m_history.record(entry);
    logLine(LogLevel::Info, "edit: %s", label.c_str());
}

bool EditSession::templateChildren(const std::string& name, std::vector<TemplateChild>& children, std::string& error) const
{
    children.clear();
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    dice::hfe::world::IBundleTemplate* bundle = bundleOf(objectTemplate);
    if (!bundle)
    {
        error = objectTemplate ? name + " is not a bundle: it has no children" : "no template named " + name;
        return false;
    }
    for (int i = 0; i < bundle->getNoTemplates(); ++i)
    {
        const dice::hfe::Mat4& m = bundle->getTransformation(i);
        TemplateChild child;
        child.name = bundle->getTemplate(i);
        child.position.x = m.m[12];
        child.position.y = m.m[13];
        child.position.z = m.m[14];
        child.rotation = rotationOf(m);
        children.push_back(child);
    }
    return true;
}

bool EditSession::objectTemplateChildren(const std::string& name, std::string& result)
{
    std::vector<TemplateChild> children;
    if (!templateChildren(name, children, result))
        return false;
    result.clear();
    for (size_t i = 0; i < children.size(); ++i)
    {
        char index[16];
        std::snprintf(index, sizeof(index), "%u ", static_cast<unsigned>(i));
        result += (i ? "; " : "") + std::string(index) + children[i].name + " at " + vectorText(children[i].position);
        if (beyond(children[i].rotation, 0.01f))
            result += " turned " + vectorText(children[i].rotation);
    }
    if (result.empty())
        result = name + " has no children";
    return true;
}

bool EditSession::addTemplateChild(const std::string& name, const std::string& child, const dice::hfe::Vec3& position,
                                   std::string& result)
{
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    dice::hfe::world::IBundleTemplate* bundle = bundleOf(objectTemplate);
    dice::hfe::world::IObjectTemplate* childTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(child) : 0;
    if (!bundle || !childTemplate)
    {
        result = !objectTemplate ? "no template named " + name
               : !bundle        ? name + " is not a bundle: it takes no children"
                                : "no template named " + child;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    if (_stricmp(childTemplate->getName().c_str(), realName.c_str()) == 0)
    {
        result = "a template cannot be its own child";
        return false;
    }
    std::string error;
    const int target = m_templateDocuments.editTarget(realName, "addTemplate", error);
    if (target < 0)
    {
        result = error;
        return false;
    }
    TemplateDocument& document = m_templateDocuments.document(static_cast<size_t>(target));
    TemplateEdit edit;
    edit.file = static_cast<size_t>(target);
    edit.before = document.file.render();
    edit.templateName = realName;
    edit.childOp = 1;
    edit.child = childTemplate->getName();
    edit.childTransform = dice::hfe::Mat4::getIdentity();
    edit.childTransform.m[12] = position.x;
    edit.childTransform.m[13] = position.y;
    edit.childTransform.m[14] = position.z;
    if (document.file.blocksOf(realName).empty())
        document.file.appendBlock(dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(objectTemplate->getClassID()),
                                  realName);
    if (!document.file.addChildLines(realName, edit.child, beyond(position, 0.001f) ? vectorText(position) : std::string(),
                                     error))
    {
        document.file.parse(edit.before);
        result = error;
        return false;
    }
    edit.after = document.file.render();
    bundle->addTemplate(edit.child);
    bundle->setTransformation(edit.childTransform, bundle->getNoTemplates() - 1);
    const std::string label = "Add the child " + edit.child + " to " + realName;
    recordTemplateEdit(edit, label);
    result = label + " in " + document.path + " (objects made from now on have it)";
    return true;
}

bool EditSession::moveTemplateChild(const std::string& name, const std::string& child, const dice::hfe::Vec3& position,
                                    const dice::hfe::Vec3& rotation, std::string& result)
{
    ChildTarget target;
    if (!findChildTarget(m_templateDocuments, name, child, target, result))
        return false;
    TemplateDocument& document = m_templateDocuments.document(target.file);
    const TemplateChildLines lines = document.file.children(target.templateName)[target.which];
    TemplateEdit edit;
    edit.file = target.file;
    edit.before = document.file.render();
    edit.templateName = target.templateName;
    edit.childOp = 3;
    edit.child = target.child;
    edit.childAt = target.index;
    edit.childBefore = target.bundle->getTransformation(target.index);
    edit.childTransform = edit.childBefore;
    edit.childTransform.m[12] = position.x;
    edit.childTransform.m[13] = position.y;
    edit.childTransform.m[14] = position.z;
    setRotation(edit.childTransform, rotation);
    // A zero position or rotation writes no line the child does not have already (makeMemberScript's rule).
    const std::string positionLine =
        beyond(position, 0.001f) || lines.positionLine >= 0 ? vectorText(position) : std::string();
    const std::string rotationLine =
        beyond(rotation, 0.01f) || lines.rotationLine >= 0 ? vectorText(rotation) : std::string();
    std::string error;
    if (!document.file.setChildTransformAt(target.templateName, target.which, positionLine, rotationLine, error))
    {
        document.file.parse(edit.before);
        result = error;
        return false;
    }
    edit.after = document.file.render();
    target.bundle->setTransformation(edit.childTransform, target.index);
    const std::string label = "Move the child " + edit.child + " of " + target.templateName;
    recordTemplateEdit(edit, label);
    result = label + " in " + document.path + " (objects made from now on have it there)";
    return true;
}

bool EditSession::removeTemplateChild(const std::string& name, const std::string& child, std::string& result)
{
    ChildTarget target;
    if (!findChildTarget(m_templateDocuments, name, child, target, result))
        return false;
    TemplateDocument& document = m_templateDocuments.document(target.file);
    TemplateEdit edit;
    edit.file = target.file;
    edit.before = document.file.render();
    edit.templateName = target.templateName;
    edit.childOp = 2;
    edit.child = target.child;
    edit.childTransform = target.bundle->getTransformation(target.index);
    std::string error;
    if (!document.file.removeChildAt(target.templateName, target.which, error))
    {
        result = error;
        return false;
    }
    edit.after = document.file.render();
    target.bundle->removeTemplate(target.index);
    const std::string label = "Take the child " + edit.child + " out of " + target.templateName;
    recordTemplateEdit(edit, label);
    result = label + " in " + document.path + " (objects made from now on lack it)";
    return true;
}

// A template's components

namespace
{

const char kComponentClassPrefix[] = "dice.hfe.world.TemplateComponent.";

// A template component's class as ObjectTemplate.createComponent names it: the ClassManager's name after its
// last '.' (the retail editor's component menu cuts it the same way).
std::string componentName(dice::hfe::world::IObjectTemplateComponent* component)
{
    std::string className;
    if (!component || !dice::hfe::classManager
        || !dice::hfe::classManager->getClassName(component->getClassID(), className))
        return std::string();
    const size_t dot = className.rfind('.');
    return dot == std::string::npos ? className : className.substr(dot + 1);
}

// The file of the template whose block has a section of that component (the defining file before the
// .tweak); -1 for none.
int sectionFile(TemplateDocuments& documents, const std::vector<size_t>& files, const std::string& templateName,
                const std::string& component)
{
    for (size_t f = 0; f < files.size(); ++f)
    {
        const TemplateDocument& document = documents.document(files[f]);
        if (!document.exists)
            continue;
        const std::vector<TemplateComponentSection> sections = document.file.componentSections(templateName);
        for (size_t k = 0; k < sections.size(); ++k)
            if (_stricmp(sections[k].component.c_str(), component.c_str()) == 0)
                return static_cast<int>(files[f]);
    }
    return -1;
}

} // namespace

std::vector<std::string> EditSession::componentClasses()
{
    std::vector<std::string> names;
    if (!dice::hfe::classManager)
        return names;
    std::vector<std::string> classes;
    dice::hfe::classManager->getAllClassIncludingSubstring(kComponentClassPrefix, classes);
    const size_t prefix = sizeof(kComponentClassPrefix) - 1;
    for (size_t i = 0; i < classes.size(); ++i)
        if (classes[i].size() > prefix && classes[i].compare(0, prefix, kComponentClassPrefix) == 0)
            names.push_back(classes[i].substr(prefix));
    std::sort(names.begin(), names.end());
    return names;
}

bool EditSession::objectTemplateComponentClasses(std::string& result)
{
    const std::vector<std::string> classes = componentClasses();
    result.clear();
    for (size_t i = 0; i < classes.size(); ++i)
        result += (i ? " " : "") + classes[i];
    if (result.empty())
        result = "this build registers no template component classes";
    return true;
}

bool EditSession::templateComponents(const std::string& name, std::vector<std::string>& components, std::string& error) const
{
    components.clear();
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate)
    {
        error = "no template named " + name;
        return false;
    }
    dice::hfe::world::IObjectTemplate::ComponentMap* map = objectTemplate->getComponentMap();
    if (!map)
        return true;
    for (dice::hfe::world::IObjectTemplate::ComponentMap::iterator it = map->begin(); it != map->end(); ++it)
    {
        const std::string component = componentName(it->second.get());
        if (!component.empty())
            components.push_back(component);
    }
    return true;
}

std::vector<std::string> EditSession::templateFixes(const std::string& name) const
{
    std::vector<std::string> fixes;
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    // Retail compared the class exactly (getClassID), so a class derived from it is not asked about.
    if (!objectTemplate || objectTemplate->getClassID() != dice::hfe::world::CID_DestroyableObjectTemplate)
        return fixes;
    if (objectTemplate->getNetworkableInfo().empty())
        fixes.push_back("networkableInfo");
    std::vector<std::string> components;
    std::string error;
    templateComponents(name, components, error);
    bool armor = false;
    for (size_t i = 0; i < components.size() && !armor; ++i)
        armor = _stricmp(components[i].c_str(), "Armor") == 0;
    if (!armor)
        fixes.push_back("armor");
    return fixes;
}

bool EditSession::objectTemplateFixes(const std::string& name, std::string& result)
{
    if (!dice::hfe::world::objectTemplateManager || !dice::hfe::world::objectTemplateManager->getTemplate(name))
    {
        result = "no template named " + name;
        return false;
    }
    const std::vector<std::string> fixes = templateFixes(name);
    result.clear();
    for (size_t i = 0; i < fixes.size(); ++i)
        result += std::string(i ? "; " : "") + (fixes[i] == "armor"
                      ? "armor: no Armor component (EditorObjectTemplate.addComponent " + name + " Armor)"
                      : "networkableInfo: none (EditorObjectTemplate.set " + name + " setNetworkableInfo BasicInfo)");
    if (result.empty())
        result = "none";
    return true;
}

bool EditSession::objectTemplateComponents(const std::string& name, std::string& result)
{
    std::vector<std::string> components;
    if (!templateComponents(name, components, result))
        return false;
    result.clear();
    for (size_t i = 0; i < components.size(); ++i)
        result += (i ? " " : "") + components[i];
    if (result.empty())
        result = name + " has no components";
    return true;
}

bool EditSession::addTemplateComponent(const std::string& name, const std::string& component, std::string& result)
{
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    const std::string className =
        dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(objectTemplate->getClassID());
    const std::string fullName = std::string(kComponentClassPrefix) + component;
    unsigned int classId = 0;
    if (component.empty() || !dice::hfe::classManager || !dice::hfe::classManager->getClassID(fullName, classId))
    {
        result = "this build registers no template component " + component + " (EditorObjectTemplate.componentClasses "
                 "lists them)";
        return false;
    }
    // Its kind: the interface a throwaway instance answers, asked the way SimpleObjectTemplate's
    // createComponent and deleteComponent ask it. The template's component of that kind is the one it replaces.
    dice::hfe::IBase* probe =
        dice::hfe::classManager->createInstance(fullName, dice::hfe::world::IID_IObjectTemplateComponent, 0);
    if (!probe)
    {
        result = "the engine could not make a " + component + " component";
        return false;
    }
    const unsigned int kind = static_cast<dice::hfe::world::IObjectTemplateComponent*>(probe)->getInterfaceID();
    probe->release();
    std::string replaced;
    dice::hfe::world::IObjectTemplate::ComponentMap* map = objectTemplate->getComponentMap();
    if (map)
    {
        dice::hfe::world::IObjectTemplate::ComponentMap::iterator held = map->find(kind);
        if (held != map->end())
            replaced = componentName(held->second.get());
    }
    if (!replaced.empty() && _stricmp(replaced.c_str(), component.c_str()) == 0)
    {
        result = realName + " has the component " + replaced + " already";
        return false;
    }
    std::vector<size_t> files;
    std::string error;
    if (!m_templateDocuments.filesOf(realName, files, error))
    {
        result = error;
        return false;
    }
    // A replacement goes where the section it replaces is; a new section beside the template's others, else
    // where a new line goes (the .tweak).
    int target = -1;
    if (!replaced.empty())
    {
        target = sectionFile(m_templateDocuments, files, realName, replaced);
        if (target < 0)
        {
            result = component + " would replace " + realName + "'s " + replaced + " component, which has no section in a "
                     "file of the template (a hand-written createComponent line, or the engine made it): refused";
            return false;
        }
    }
    else
    {
        for (size_t f = 0; f < files.size(); ++f)
            if (m_templateDocuments.document(files[f]).exists
                && !m_templateDocuments.document(files[f]).file.componentSections(realName).empty())
                target = static_cast<int>(files[f]);
        if (target < 0)
            target = m_templateDocuments.editTarget(realName, "createComponent", error);
        if (target < 0)
        {
            result = error;
            return false;
        }
    }
    TemplateDocument& document = m_templateDocuments.document(static_cast<size_t>(target));
    if (document.archived)
    {
        result = document.path + " is read from a mounted archive, so the change could not be saved: refused";
        return false;
    }
    TemplateEdit edit;
    edit.file = static_cast<size_t>(target);
    edit.before = document.file.render();
    edit.templateName = realName;
    edit.className = className;
    edit.componentOp = 1;
    edit.componentAdded = component;
    edit.componentRemoved = replaced;
    bool filed = false;
    if (!replaced.empty())
        filed = document.file.replaceComponentSection(realName, replaced, component, edit.componentLines, error);
    else
    {
        if (document.file.blocksOf(realName).empty())
            document.file.appendBlock(className, realName);
        filed = document.file.addComponentSection(realName, component, error);
    }
    if (!filed)
    {
        document.file.parse(edit.before);
        result = error;
        return false;
    }
    // The engine: createComponent, which puts it in place of a component of the same kind.
    const std::vector<std::string> lines(1, "ObjectTemplate.createComponent " + component);
    std::vector<std::string> now;
    std::string listError;
    bool made = runTemplateLines(className, realName, lines, error) && templateComponents(realName, now, listError);
    made = made && std::find(now.begin(), now.end(), component) != now.end();
    if (!made)
    {
        document.file.parse(edit.before);
        result = error.empty() ? "the engine made no " + component + " component for " + realName : error;
        return false;
    }
    edit.after = document.file.render();
    const std::string label = replaced.empty() ? "Add the component " + component + " to " + realName
                                               : "Replace " + realName + "'s component " + replaced + " with " + component;
    recordTemplateEdit(edit, label);
    result = label + " in " + document.path;
    return true;
}

bool EditSession::removeTemplateComponent(const std::string& name, const std::string& component, std::string& result)
{
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    const std::string className =
        dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(objectTemplate->getClassID());
    std::vector<std::string> components;
    std::string error;
    templateComponents(realName, components, error);
    std::string held;
    for (size_t i = 0; i < components.size(); ++i)
        if (_stricmp(components[i].c_str(), component.c_str()) == 0)
            held = components[i];
    if (held.empty())
    {
        result = realName + " has no component " + component;
        return false;
    }
    std::vector<size_t> files;
    if (!m_templateDocuments.filesOf(realName, files, error))
    {
        result = error;
        return false;
    }
    const int target = sectionFile(m_templateDocuments, files, realName, held);
    if (target < 0)
    {
        result = realName + "'s " + held + " component has no section in a file of the template (a hand-written "
                 "createComponent line, or the engine made it): refused";
        return false;
    }
    TemplateDocument& document = m_templateDocuments.document(static_cast<size_t>(target));
    if (document.archived)
    {
        result = document.path + " is read from a mounted archive, so the change could not be saved: refused";
        return false;
    }
    TemplateEdit edit;
    edit.file = static_cast<size_t>(target);
    edit.before = document.file.render();
    edit.templateName = realName;
    edit.className = className;
    edit.componentOp = 2;
    edit.componentRemoved = held;
    if (!document.file.removeComponentSection(realName, held, edit.componentLines, error))
    {
        document.file.parse(edit.before);
        result = error;
        return false;
    }
    const std::vector<std::string> lines(1, "ObjectTemplate.deleteComponent " + held);
    if (!runTemplateLines(className, realName, lines, error))
    {
        document.file.parse(edit.before);
        result = error;
        return false;
    }
    edit.after = document.file.render();
    const std::string label = "Take the component " + held + " out of " + realName;
    recordTemplateEdit(edit, label);
    result = label + " in " + document.path;
    return true;
}

// The Object templates panel's template

bool EditSession::showObjectTemplate(const std::string& name, std::string& result)
{
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    m_panelTemplate = objectTemplate->getName();
    result = "the Object templates panel shows " + m_panelTemplate;
    return true;
}

std::string EditSession::takePanelTemplate()
{
    std::string name;
    name.swap(m_panelTemplate);
    return name;
}

// New templates, clones and new children

namespace
{

bool validTemplateName(const std::string& name)
{
    return !name.empty() && name.find_first_of(" \t\r\n/\\\"#") == std::string::npos;
}

std::string folderOf(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

std::string stemOf(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    const std::string file = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = file.rfind('.');
    return dot == std::string::npos ? file : file.substr(0, dot);
}

bool endsWithText(const std::string& text, const char* tail)
{
    const size_t n = std::strlen(tail);
    return text.size() >= n && _stricmp(text.c_str() + text.size() - n, tail) == 0;
}

// The registered class a name gives (its registered spelling), or "".
std::string templateClass(const std::string& className)
{
    const unsigned int id = dice::hfe::world::objectTemplateManager->getClassIDFromTemplateClassName(className);
    return id != 0 ? dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(id) : std::string();
}

// Is the engine able to read the file already (an archive or a loose file)? Or is one on disk where it would
// be written?
bool fileTaken(const std::string& path, std::string& writePath)
{
    dice::hfe::io::fileManager->getWritePath(path, writePath);
    return !dice::hfe::io::fileManager->fileExistsEx(path, false).empty()
        || GetFileAttributesA(writePath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

} // namespace

bool EditSession::newTemplate(const std::string& className, const std::string& name, const std::string& folder,
                              std::string& result)
{
    if (!dice::hfe::world::objectTemplateManager || !dice::hfe::io::mainConsole || !dice::hfe::io::fileManager)
    {
        result = "the engine console is not up";
        return false;
    }
    if (!validTemplateName(name))
    {
        result = "'" + name + "' is not a template name";
        return false;
    }
    if (dice::hfe::world::objectTemplateManager->getTemplate(name))
    {
        result = "a template named " + name + " exists already";
        return false;
    }
    const std::string realClass = templateClass(className);
    if (realClass.empty())
    {
        result = "no template class named " + className;
        return false;
    }
    const std::string path = (folder.empty() ? "Objects/" + name : folder) + "/" + name + ".con";
    std::string writePath, error;
    if (fileTaken(path, writePath))
    {
        result = path + " is there already: refused";
        return false;
    }
    const std::string text = "ObjectTemplate.create " + realClass + " " + name + "\r\nObjectTemplate.saveInSeparateFile 1\r\n"
                             "ObjectTemplate.createdInEditor 1\r\n";
    if (!writeFilesNow("new template", { std::make_pair(writePath, text) }, error))
    {
        result = error;
        return false;
    }
    const std::string answer = engineLine("run " + path);
    if (!dice::hfe::world::objectTemplateManager->getTemplate(name))
    {
        result = "wrote " + writePath + ", but running it made no template " + name + " (" + answer + ")";
        return false;
    }
    result = "made the " + realClass + " " + name + " in " + path + " (written now: not an undoable edit)";
    logLine(LogLevel::Info, "template: %s", result.c_str());
    return true;
}

bool EditSession::cloneTemplate(const std::string& source, const std::string& name, const std::string& folder,
                                std::string& result)
{
    if (!dice::hfe::world::objectTemplateManager || !dice::hfe::io::mainConsole || !dice::hfe::io::fileManager)
    {
        result = "the engine console is not up";
        return false;
    }
    dice::hfe::world::IObjectTemplate* objectTemplate = dice::hfe::world::objectTemplateManager->getTemplate(source);
    if (!objectTemplate)
    {
        result = "no template named " + source;
        return false;
    }
    if (!validTemplateName(name))
    {
        result = "'" + name + "' is not a template name";
        return false;
    }
    const std::string realSource = objectTemplate->getName();
    std::vector<size_t> files;
    std::string error;
    if (!m_templateDocuments.filesOf(realSource, files, error))
    {
        result = error;
        return false;
    }
    const TemplateDocument& defining = m_templateDocuments.document(files[0]);
    if (!endsWithText(defining.path, ".con") || !defining.exists)
    {
        result = realSource + " is defined in " + defining.path + ", not in a .con of its own: clone the template that "
                 "file belongs to";
        return false;
    }
    const std::string tweakText =
        files.size() > 1 && m_templateDocuments.document(files[1]).exists ? m_templateDocuments.document(files[1]).file.render()
                                                                           : std::string();
    TemplateClone clone;
    if (!cloneTemplateFiles(defining.file.render(), tweakText, stemOf(defining.path), realSource, name, clone, error))
    {
        result = error;
        return false;
    }
    for (size_t i = 0; i < clone.renames.size(); ++i)
        if (dice::hfe::world::objectTemplateManager->getTemplate(clone.renames[i].to))
        {
            result = "a template named " + clone.renames[i].to + " exists already";
            return false;
        }
    const std::string dir = folder.empty() ? folderOf(defining.path) : folder;
    const std::string conPath = (dir.empty() ? std::string() : dir + "/") + name + ".con";
    const std::string tweakPath = (dir.empty() ? std::string() : dir + "/") + name + ".tweak";
    std::string conWrite, tweakWrite;
    if (fileTaken(conPath, conWrite) || (!clone.tweak.empty() && fileTaken(tweakPath, tweakWrite)))
    {
        result = (fileTaken(conPath, conWrite) ? conPath : tweakPath) + " is there already: refused";
        return false;
    }
    // Both written as one save before the .con runs (its include needs the .tweak).
    std::vector<std::pair<std::string, std::string> > written;
    if (!clone.tweak.empty())
        written.push_back(std::make_pair(tweakWrite, clone.tweak));
    written.push_back(std::make_pair(conWrite, clone.con));
    if (!writeFilesNow("template clone", written, error))
    {
        result = error;
        return false;
    }
    const std::string answer = engineLine("run " + conPath);
    if (!dice::hfe::world::objectTemplateManager->getTemplate(name))
    {
        result = "wrote " + conWrite + ", but running it made no template " + name + " (" + answer + ")";
        return false;
    }
    char counts[240];
    const unsigned children = static_cast<unsigned>(clone.renames.size() - 1 - clone.sounds);
    std::snprintf(counts, sizeof(counts), "; %u child template%s cloned with it, %d sound template%s following their owners, "
                  "%d geometry and collision line%s left out (the clone shares the source's geometry)", children,
                  children == 1 ? "" : "s", clone.sounds, clone.sounds == 1 ? "" : "s", clone.droppedLines,
                  clone.droppedLines == 1 ? "" : "s");
    result = "cloned " + realSource + " as " + name + " in " + conPath + (clone.tweak.empty() ? "" : " and its .tweak") + counts
           + " (written now: not an undoable edit)";
    logLine(LogLevel::Info, "template: %s", result.c_str());
    return true;
}

bool EditSession::newTemplateChild(const std::string& parent, const std::string& className, const std::string& name,
                                   const dice::hfe::Vec3& position, std::string& result)
{
    if (!dice::hfe::world::objectTemplateManager || !dice::hfe::io::mainConsole)
    {
        result = "the engine console is not up";
        return false;
    }
    dice::hfe::world::IObjectTemplate* parentTemplate = dice::hfe::world::objectTemplateManager->getTemplate(parent);
    dice::hfe::world::IBundleTemplate* bundle = bundleOf(parentTemplate);
    if (!bundle)
    {
        result = parentTemplate ? parent + " is not a bundle: it takes no children" : "no template named " + parent;
        return false;
    }
    if (!validTemplateName(name))
    {
        result = "'" + name + "' is not a template name";
        return false;
    }
    if (dice::hfe::world::objectTemplateManager->getTemplate(name))
    {
        result = "a template named " + name + " exists already";
        return false;
    }
    const std::string realClass = templateClass(className);
    if (realClass.empty())
    {
        result = "no template class named " + className;
        return false;
    }
    const std::string realParent = parentTemplate->getName();
    std::string error;
    const int target = m_templateDocuments.editTarget(realParent, "addTemplate", error);
    if (target < 0)
    {
        result = error;
        return false;
    }
    TemplateDocument& document = m_templateDocuments.document(static_cast<size_t>(target));
    TemplateEdit edit;
    edit.file = static_cast<size_t>(target);
    edit.before = document.file.render();
    edit.templateName = realParent;
    edit.childOp = 4;
    edit.child = name;
    edit.childClass = realClass;
    edit.childTransform = dice::hfe::Mat4::getIdentity();
    edit.childTransform.m[12] = position.x;
    edit.childTransform.m[13] = position.y;
    edit.childTransform.m[14] = position.z;
    if (document.file.blocksOf(realParent).empty())
        document.file.appendBlock(dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(parentTemplate->getClassID()),
                                  realParent);
    if (!document.file.addChildLines(realParent, name, beyond(position, 0.001f) ? vectorText(position) : std::string(), error))
    {
        document.file.parse(edit.before);
        result = error;
        return false;
    }
    document.file.appendCreateBlock(realClass, name, std::vector<std::string>(1, "createdInEditor 1"));
    edit.after = document.file.render();
    if (!makeNewChild(bundle, realClass, name, edit.childTransform, document.path, error))
    {
        document.file.parse(edit.before);
        result = error;
        return false;
    }
    const std::string label = "Add a new " + realClass + " " + name + " to " + realParent;
    recordTemplateEdit(edit, label);
    result = label + " in " + document.path;
    return true;
}

std::vector<std::string> EditSession::templateClassChoices(bool children)
{
    std::vector<std::string> names;
    if (!dice::hfe::world::objectTemplateManager)
        return names;
    std::string text, error;
    if (ObjectLibrary::readTextFile("bf2editor/Settings/Init/ResourceInit.con", text, error))
    {
        const std::string key = children ? "ResourceHandler.AddAvailableChildResource" : "ResourceHandler.AddAvailableNewResource";
        size_t start = 0;
        while (start < text.size())
        {
            size_t end = text.find('\n', start);
            if (end == std::string::npos)
                end = text.size();
            const std::string line = text.substr(start, end - start);
            start = end + 1;
            if (line.size() <= key.size() || _strnicmp(line.c_str(), key.c_str(), key.size()) != 0)
                continue;
            const size_t open = line.find('"', key.size());
            const size_t close = open == std::string::npos ? std::string::npos : line.find('"', open + 1);
            if (close == std::string::npos)
                continue;
            const std::string name = templateClass(line.substr(open + 1, close - open - 1));
            if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end())
                names.push_back(name);
        }
    }
    if (names.empty())
    {
        std::list<std::pair<unsigned int, std::string> > classes;
        dice::hfe::world::objectTemplateManager->getTemplateClasses(classes);
        for (std::list<std::pair<unsigned int, std::string> >::const_iterator it = classes.begin(); it != classes.end(); ++it)
            names.push_back(it->second);
        std::sort(names.begin(), names.end());
    }
    return names;
}

// Inherit settings

namespace
{

// The rows inheritance leaves alone [CHOSEN, retail's filter sub_1002DE60 unread]: what names the template
// itself, its mesh or its authorship, and `active`, whose value is the active template.
bool inheritedRow(const std::string& row)
{
    const char* const kept[] = { "active", "activeSafe", "create", "geometry", "collisionMesh", "setCollisionMesh",
                                 "creator", "modifiedByUser", "saveInSeparateFile", "createdInEditor" };
    for (size_t i = 0; i < sizeof(kept) / sizeof(kept[0]); ++i)
        if (_stricmp(row.c_str(), kept[i]) == 0)
            return false;
    return !row.empty();
}

} // namespace

bool EditSession::inheritTemplateSettings(const std::string& target, const std::string& source, std::string& result)
{
    if (!dice::hfe::world::objectTemplateManager || !dice::hfe::io::mainConsole)
    {
        result = "the engine console is not up";
        return false;
    }
    dice::hfe::world::IObjectTemplate* to = dice::hfe::world::objectTemplateManager->getTemplate(target);
    dice::hfe::world::IObjectTemplate* from = dice::hfe::world::objectTemplateManager->getTemplate(source);
    if (!to || !from)
    {
        result = "no template named " + (to ? source : target);
        return false;
    }
    if (to == from)
    {
        result = "a template cannot inherit its own settings";
        return false;
    }
    const std::string realTarget = to->getName();
    const std::string realSource = from->getName();
    const std::string sourceClass =
        dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(from->getClassID());
    const std::string targetClass =
        dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(to->getClassID());
    // The source's set/get rows and their values, with the source active (EH+0x108 steps 1 and 3).
    std::string refusal = engineLine("ObjectTemplate.activeSafe " + sourceClass + " " + realSource);
    if (!refusal.empty())
    {
        result = "the engine could not make " + realSource + " active: " + refusal;
        return false;
    }
    const std::string rows = engineLine("Console.getActiveMethods ObjectTemplate");
    std::vector<std::pair<std::string, std::string> > values;
    size_t start = 0;
    while (start < rows.size())
    {
        size_t end = rows.find_first_of("\r\n", start);
        if (end == std::string::npos)
            end = rows.size();
        std::string row = rows.substr(start, end - start);
        start = end + 1;
        const size_t blank = row.find_first_not_of(" \t");
        row = blank == std::string::npos ? std::string() : row.substr(blank);
        if (_strnicmp(row.c_str(), "ObjectTemplate.", 15) == 0)
            row = row.substr(15);
        if (!inheritedRow(row) || engineLine("Console.isSetGet ObjectTemplate " + row) != "1")
            continue;
        values.push_back(std::make_pair(row, engineLine("ObjectTemplate." + row)));
    }
    if (values.empty())
    {
        result = "the engine answered no set/get properties for " + realSource + " (Console.getActiveMethods: '" + rows.substr(0, 80) + "')";
        return false;
    }
    std::vector<std::string> sourceComponents, targetComponents;
    std::string error;
    templateComponents(realSource, sourceComponents, error);
    templateComponents(realTarget, targetComponents, error);
    // One batch: the components the target lacks first (step 2), then the differing properties (step 4).
    m_templateBatching = true;
    m_templateBatch.clear();
    int components = 0, properties = 0;
    std::string refused;
    for (size_t i = 0; i < sourceComponents.size(); ++i)
    {
        bool has = false;
        for (size_t k = 0; k < targetComponents.size() && !has; ++k)
            has = _stricmp(sourceComponents[i].c_str(), targetComponents[k].c_str()) == 0;
        if (has)
            continue;
        std::string r;
        if (addTemplateComponent(realTarget, sourceComponents[i], r))
            ++components;
        else
            refused += (refused.empty() ? "" : "; ") + sourceComponents[i] + ": " + r;
    }
    for (size_t i = 0; i < values.size(); ++i)
    {
        engineLine("ObjectTemplate.activeSafe " + targetClass + " " + realTarget);
        if (engineLine("ObjectTemplate." + values[i].first) == values[i].second)
            continue;
        std::string r;
        if (setObjectTemplateProperty(realTarget, values[i].first, values[i].second, r))
            ++properties;
        else
            refused += (refused.empty() ? "" : "; ") + values[i].first + ": " + r;
    }
    m_templateBatching = false;
    const std::vector<int> ids = m_templateBatch;
    m_templateBatch.clear();
    char counts[160];
    std::snprintf(counts, sizeof(counts), "%d component%s and %d propert%s of %u set/get", components,
                  components == 1 ? "" : "s", properties, properties == 1 ? "y" : "ies", static_cast<unsigned>(values.size()));
    if (ids.empty())
    {
        result = realTarget + " has " + realSource + "'s settings already (" + counts + " to change)"
               + (refused.empty() ? std::string() : "; refused: " + refused);
        return refused.empty();
    }
    HistoryEntry entry;
    entry.label = "Inherit " + realSource + "'s settings into " + realTarget;
    char line[80];
    for (size_t i = 0; i < ids.size(); ++i)
    {
        std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d after\n", ids[i]);
        entry.doScript += line;
        std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d before\n", ids[ids.size() - 1 - i]);
        entry.undoScript += line;
    }
    m_history.record(entry);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + ": " + counts + " changed, one undoable edit" + (refused.empty() ? std::string() : "; refused: " + refused);
    return true;
}

// Rename

bool EditSession::renameObjectTemplate(const std::string& name, const std::string& newName, std::string& result)
{
    if (!dice::hfe::world::objectTemplateManager || !dice::hfe::io::mainConsole)
    {
        result = "the engine console is not up";
        return false;
    }
    dice::hfe::world::IObjectTemplate* objectTemplate = dice::hfe::world::objectTemplateManager->getTemplate(name);
    if (!objectTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    if (!validTemplateName(newName))
    {
        result = "'" + newName + "' is not a template name";
        return false;
    }
    if (dice::hfe::world::objectTemplateManager->getTemplate(newName))
    {
        result = "a template named " + newName + " exists already (the engine keeps an old name too after a rename)";
        return false;
    }
    const std::string realName = objectTemplate->getName();
    size_t levelFile = 0;
    std::string levelClass;
    if (m_document && m_document->templateFile(realName, levelFile, levelClass))
    {
        result = realName + " is defined by the level: rename it with EditorGameplay.rename";
        return false;
    }
    // Every file that opens its block or adds it as a child: its own, and its parents'.
    std::vector<size_t> files;
    std::string error;
    if (!m_templateDocuments.filesOf(realName, files, error))
    {
        result = error;
        return false;
    }
    std::list<dice::hfe::world::IObjectTemplate*>* all = dice::hfe::world::objectTemplateManager->getGlobalTemplateList();
    for (std::list<dice::hfe::world::IObjectTemplate*>::iterator it = all ? all->begin() : std::list<dice::hfe::world::IObjectTemplate*>::iterator();
         all && it != all->end(); ++it)
    {
        dice::hfe::world::IBundleTemplate* bundle = bundleOf(*it);
        if (!bundle || *it == objectTemplate)
            continue;
        bool holds = false;
        for (int i = 0; i < bundle->getNoTemplates() && !holds; ++i)
            holds = _stricmp(bundle->getTemplate(i).c_str(), realName.c_str()) == 0;
        std::vector<size_t> parentFiles;
        std::string ignored;
        if (holds && m_templateDocuments.filesOf((*it)->getName(), parentFiles, ignored))
            files.insert(files.end(), parentFiles.begin(), parentFiles.end());
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    for (size_t i = 0; i < files.size(); ++i)
    {
        const TemplateDocument& document = m_templateDocuments.document(files[i]);
        if (document.exists && document.archived)
        {
            TemplateFile probe = document.file;
            if (probe.renameTemplateLines(realName, newName))
            {
                result = document.path + " names " + realName + " and is read from a mounted archive, so it could not "
                         "follow the rename: refused";
                return false;
            }
        }
    }
    // One batch: each file that names it, then the engine and the level.
    m_templateBatching = true;
    m_templateBatch.clear();
    int changedFiles = 0;
    for (size_t i = 0; i < files.size(); ++i)
    {
        TemplateDocument& document = m_templateDocuments.document(files[i]);
        if (!document.exists)
            continue;
        TemplateEdit edit;
        edit.file = files[i];
        edit.before = document.file.render();
        if (!document.file.renameTemplateLines(realName, newName))
            continue;
        edit.after = document.file.render();
        edit.templateName = realName;
        recordTemplateEdit(edit, "Rename " + realName + " in " + document.path);
        ++changedFiles;
    }
    TemplateEdit renaming;
    renaming.file = files.empty() ? 0 : files[0];
    renaming.before = renaming.after = m_templateDocuments.document(renaming.file).file.render();
    renaming.templateName = realName;
    renaming.childOp = 5;
    renaming.child = newName;
    const int parents = renameInEngine(realName, newName);
    const size_t records = m_document ? m_document->renameObjectRecords(realName, newName) : 0;
    recordTemplateEdit(renaming, "Rename " + realName + " to " + newName);
    m_templateBatching = false;
    const std::vector<int> ids = m_templateBatch;
    m_templateBatch.clear();
    HistoryEntry entry;
    entry.label = "Rename the template " + realName + " to " + newName;
    char line[80];
    for (size_t i = 0; i < ids.size(); ++i)
    {
        std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d after\n", ids[i]);
        entry.doScript += line;
        std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d before\n", ids[ids.size() - 1 - i]);
        entry.undoScript += line;
    }
    m_history.record(entry);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    char counts[200];
    std::snprintf(counts, sizeof(counts), ": %d file%s, %d parent%s in the engine, %u object%s of the level; the file names "
                  "stay, and other properties that name it are not changed", changedFiles, changedFiles == 1 ? "" : "s",
                  parents, parents == 1 ? "" : "s", static_cast<unsigned>(records), records == 1 ? "" : "s");
    result = entry.label + counts;
    return true;
}

// Change type

bool EditSession::changeTemplateType(const std::string& name, const std::string& className, std::string& result)
{
    if (!dice::hfe::world::objectTemplateManager || !dice::hfe::io::mainConsole || !dice::hfe::world::objectManager)
    {
        result = "the engine console is not up";
        return false;
    }
    dice::hfe::world::IObjectTemplate* objectTemplate = dice::hfe::world::objectTemplateManager->getTemplate(name);
    if (!objectTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    // Retail's condition (M+0xC8): "Can only change template type if object has been created in editor!"
    if ((objectTemplate->getFlags() & 0x80u) == 0)
    {
        result = realName + " was not made in the editor (createdInEditor 1): only such a template changes its type";
        return false;
    }
    const std::string realClass = templateClass(className);
    const std::string oldClass =
        dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(objectTemplate->getClassID());
    if (realClass.empty())
    {
        result = "no template class named " + className;
        return false;
    }
    if (_stricmp(realClass.c_str(), oldClass.c_str()) == 0)
    {
        result = realName + " is a " + oldClass + " already";
        return false;
    }
    // The engine deletes the template to make it again: no object of the level may stand on it.
    const std::map<unsigned int, dice::hfe::world::IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, dice::hfe::world::IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        if (it->second && templateHolds(it->second->getTemplate(), objectTemplate, 0))
        {
            char id[32];
            std::snprintf(id, sizeof(id), " #%u", it->first);
            result = "an object of the level uses " + realName + " (" + it->second->getTemplate()->getName() + id
                   + "): remove its objects first";
            return false;
        }
    std::vector<size_t> files;
    std::string error;
    if (!m_templateDocuments.filesOf(realName, files, error))
    {
        result = error;
        return false;
    }
    const std::string workingFile = dice::hfe::world::objectTemplateManager->getWorkingFolderAndFile(realName);
    TemplateEdit edit;
    edit.templateName = realName;
    edit.className = oldClass;
    edit.childClass = realClass;
    edit.childOp = 6;
    edit.workingFile = workingFile;
    bool first = true;
    int changed = 0;
    for (size_t i = 0; i < files.size(); ++i)
    {
        TemplateDocument& document = m_templateDocuments.document(files[i]);
        if (!document.exists)
            continue;
        if (document.archived)
        {
            result = document.path + " is read from a mounted archive, so the change could not be saved: refused";
            return false;
        }
        const std::string before = document.file.render();
        TemplateFile probe = document.file;
        const bool renamed = probe.setBlockClass(realName, realClass);
        if (first)
        {
            edit.file = files[i];
            edit.before = before;
            first = false;
        }
        else
        {
            edit.extraFiles.push_back(files[i]);
            edit.extraBefore.push_back(before);
        }
        if (renamed)
            ++changed;
    }
    if (first || changed == 0)
    {
        result = "no file of " + realName + " opens it with a class to change";
        return false;
    }
    // The files first, then the engine's template made again from them.
    m_templateDocuments.document(edit.file).file.setBlockClass(realName, realClass);
    edit.after = m_templateDocuments.document(edit.file).file.render();
    for (size_t i = 0; i < edit.extraFiles.size(); ++i)
    {
        TemplateFile& file = m_templateDocuments.document(edit.extraFiles[i]).file;
        file.setBlockClass(realName, realClass);
        edit.extraAfter.push_back(file.render());
    }
    std::vector<size_t> all(1, edit.file);
    all.insert(all.end(), edit.extraFiles.begin(), edit.extraFiles.end());
    std::vector<std::string> refused;
    if (!rebuildFromFiles(m_templateDocuments, all, realName, workingFile, refused, error))
    {
        // Put everything back as it was, the engine's template too.
        m_templateDocuments.document(edit.file).file.parse(edit.before);
        for (size_t i = 0; i < edit.extraFiles.size(); ++i)
            m_templateDocuments.document(edit.extraFiles[i]).file.parse(edit.extraBefore[i]);
        std::vector<std::string> ignored;
        std::string restoreError;
        rebuildFromFiles(m_templateDocuments, all, realName, workingFile, ignored, restoreError);
        result = error;
        return false;
    }
    const std::string label = "Change " + realName + " from a " + oldClass + " to a " + realClass;
    recordTemplateEdit(edit, label);
    char counts[64];
    std::snprintf(counts, sizeof(counts), " (%d file%s", changed, changed == 1 ? "" : "s");
    result = label + counts + "; made again from its files";
    if (!refused.empty())
    {
        std::snprintf(counts, sizeof(counts), "; %u line%s the new class refuses, kept in the file: ",
                      static_cast<unsigned>(refused.size()), refused.size() == 1 ? "" : "s");
        result += counts;
        for (size_t i = 0; i < refused.size() && i < 4; ++i)
            result += (i ? ", " : "") + refused[i];
    }
    result += ")";
    return true;
}

// Template edit batches and the object wizard

void EditSession::beginTemplateBatch()
{
    m_templateBatching = true;
    m_templateBatch.clear();
}

void EditSession::endTemplateBatch(const std::string& label)
{
    m_templateBatching = false;
    const std::vector<int> ids = m_templateBatch;
    m_templateBatch.clear();
    if (ids.empty())
        return;
    HistoryEntry entry;
    entry.label = label;
    char line[80];
    for (size_t i = 0; i < ids.size(); ++i)
    {
        std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d after\n", ids[i]);
        entry.doScript += line;
        std::snprintf(line, sizeof(line), "EditorObjectTemplate.applyEdit %d before\n", ids[ids.size() - 1 - i]);
        entry.undoScript += line;
    }
    m_history.record(entry);
    logLine(LogLevel::Info, "edit: %s", label.c_str());
}

bool EditSession::wizardCommand(const std::string& action, const std::string& argument, std::string& result)
{
    if (action == "load")
        return m_wizard.load(result);
    if (action == "run")
        return m_wizard.run(argument.empty() ? std::string("General") : argument, result);
    if (action == "execute")
        return m_wizard.execute(result);
    if (action == "answer")
        return m_wizard.answer(argument, result);
    if (action == "cancel")
        return m_wizard.cancel(result);
    if (action == "checklistOk")
        return m_wizard.checklistDone(result);
    if (action == "rules")
    {
        if (!m_wizard.loaded() && !m_wizard.load(result))
            return false;
        const std::vector<WizardRule>& rules = m_wizard.rules().rules();
        result.clear();
        for (size_t i = 0; i < rules.size(); ++i)
        {
            char counts[64];
            std::snprintf(counts, sizeof(counts), " [%u/%u lines]", static_cast<unsigned>(rules[i].analyzer.size()),
                          static_cast<unsigned>(rules[i].resolution.size()));
            result += (i ? "; " : "") + rules[i].title + " (" + rules[i].category + ", " + rules[i].type + ")" + counts;
        }
        if (result.empty())
            result = "no rules";
        return true;
    }
    if (action == "items")
    {
        const std::vector<ObjectWizard::Item>& items = m_wizard.items();
        const char* const groups[] = { "Errors", "Warnings", "Utils" };
        result.clear();
        for (size_t i = 0; i < items.size(); ++i)
        {
            char index[16];
            std::snprintf(index, sizeof(index), "%u ", static_cast<unsigned>(i));
            result += (i ? "; " : "") + std::string(index) + "[" + (items[i].group >= 0 ? groups[items[i].group] : "Other") + "] "
                    + items[i].label + (items[i].ticked ? " (ticked)" : "") + (items[i].readOnly ? " (read-only)" : "")
                    + (items[i].status.empty() ? std::string() : " (" + items[i].status + ")");
        }
        if (result.empty())
            result = "no items (EditorWizard.run <category> first)";
        return true;
    }
    if (action == "tick" || action == "check")
    {
        // "<index|label> <0|1>": the label may hold blanks, so the switch is the last word.
        const size_t blank = argument.find_last_of(' ');
        if (blank == std::string::npos)
        {
            result = "give an index (or an item's label) and 0 or 1";
            return false;
        }
        const std::string which = argument.substr(0, blank);
        const bool on = std::atoi(argument.c_str() + blank + 1) != 0;
        size_t index = static_cast<size_t>(-1);
        if (!which.empty() && which.find_first_not_of("0123456789") == std::string::npos)
            index = static_cast<size_t>(std::atoi(which.c_str()));
        else if (action == "tick")
        {
            const std::vector<ObjectWizard::Item>& items = m_wizard.items();
            for (size_t i = 0; i < items.size() && index == static_cast<size_t>(-1); ++i)
                if (_stricmp(items[i].label.c_str(), which.c_str()) == 0)
                    index = i;
        }
        if (action == "tick")
            return m_wizard.tick(index, on, result);
        std::vector<bool>& rows = m_wizard.checklist();
        if (m_wizard.waiting() != ObjectWizard::Wait::Checklist || index >= rows.size())
        {
            result = "no such checklist row";
            return false;
        }
        rows[index] = on;
        result = m_wizard.promptChoices()[index] + (on ? ": ticked" : ": not ticked");
        return true;
    }
    if (action == "prompt")
    {
        switch (m_wizard.waiting())
        {
        case ObjectWizard::Wait::None:
            result = "nothing is asked";
            break;
        case ObjectWizard::Wait::Checklist:
        {
            result = "checklist: " + m_wizard.promptTitle();
            const std::vector<bool>& rows = m_wizard.checklist();
            for (size_t i = 0; i < rows.size(); ++i)
                result += (i ? ", " : ": ") + m_wizard.promptChoices()[i] + (rows[i] ? " [x]" : " [ ]");
            break;
        }
        default:
            result = m_wizard.promptTitle() + ": " + m_wizard.promptText();
            if (!m_wizard.promptPreset().empty())
                result += " (preset " + m_wizard.promptPreset() + ")";
            for (size_t i = 0; i < m_wizard.promptChoices().size(); ++i)
                result += (i ? "/" : " - ") + m_wizard.promptChoices()[i];
            break;
        }
        return true;
    }
    result = "no wizard action '" + action + "'";
    return false;
}

// Additional template properties and child transforms.

bool EditSession::addObjectTemplateLine(const std::string& name, const std::string& property, const std::string& value,
                                        std::string& result)
{
    if (!engineSafeValue(property, value, result))
        return false;
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate || !dice::hfe::mainConsoleObjects)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    const std::string className =
        dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(objectTemplate->getClassID());
    const std::string activated = engineLine("ObjectTemplate.activeSafe " + className + " " + realName);
    dice::hfe::ConsoleObject* binding =
        activated.empty() ? dice::hfe::mainConsoleObjects->getConsoleObject("ObjectTemplate", property) : 0;
    if (!binding)
    {
        result = realName + " has no ObjectTemplate." + property + " (a " + className
               + " without the component it belongs to), so the game would ignore the line: refused";
        return false;
    }
    std::string error;
    const int target = m_templateDocuments.editTarget(realName, property, error);
    if (target < 0)
    {
        result = error;
        return false;
    }
    TemplateDocument& document = m_templateDocuments.document(static_cast<size_t>(target));
    TemplateEdit edit;
    edit.file = static_cast<size_t>(target);
    edit.before = document.file.render();
    edit.templateName = realName;
    edit.className = className;
    edit.property = property;
    if (document.file.blocksOf(realName).empty())
        document.file.appendBlock(className, realName);
    if (!document.file.addProperty(realName, property, value, error))
    {
        document.file.parse(edit.before);
        result = error;
        return false;
    }
    edit.after = document.file.render();
    const std::string label = "Add " + property + " " + value + " to " + realName;
    recordTemplateEdit(edit, label);
    result = label + " in " + document.path + " (the file: the engine takes such a line when the level loads again)";
    return true;
}

namespace
{

// The lines of `property` in the template's edit target's block, with their values (the barrel and fire-rate
// helpers read what is there before they change it).
std::vector<std::string> propertyValues(TemplateDocuments& documents, const std::string& name, const std::string& property)
{
    std::vector<std::string> values;
    std::string error;
    const int target = documents.editTarget(name, property, error);
    if (target < 0)
        return values;
    const std::vector<TemplatePropertyLine> lines = documents.document(static_cast<size_t>(target)).file.properties(name);
    for (size_t i = 0; i < lines.size(); ++i)
        if (_stricmp(lines[i].property.c_str(), property.c_str()) == 0)
            values.push_back(lines[i].value);
    return values;
}

std::string trimmedValue(const std::string& text)
{
    const size_t begin = text.find_first_not_of(" \t");
    const size_t end = text.find_last_not_of(" \t");
    return begin == std::string::npos ? std::string() : text.substr(begin, end - begin + 1);
}

} // namespace

bool EditSession::addArmorEffect(const std::string& name, const std::string& effect, std::string& result)
{
    // The retail picker offered the EffectBundle templates (ObjectEditor 0x100241C0) and sent
    // "ObjectTemplate.armor.addArmorEffect 0 %s 0/0/0 0/0/0": at 0 hit points, at the object's origin.
    dice::hfe::world::IObjectTemplate* bundle =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(effect) : 0;
    if (!bundle || bundle->getClassID() != dice::hfe::world::CID_EffectBundleTemplate)
    {
        result = "'" + effect + "' is no EffectBundle template";
        return false;
    }
    return addObjectTemplateLine(name, "armor.addArmorEffect", "0 " + bundle->getName() + " 0/0/0 0/0/0", result);
}

bool EditSession::setBarrel(const std::string& parent, const std::string& child, bool on, std::string& result)
{
    // Retail's Make / Remove Barrel: the parent made active, then ObjectTemplate.fire.addBarrelName <child> or
    // removeBarrelName (ObjectEditor's tree menu, 0x100216A0); here the parent's addBarrelName line of the child.
    ChildTarget target;
    if (!findChildTarget(m_templateDocuments, parent, child, target, result))
        return false;
    const std::vector<std::string> barrels = propertyValues(m_templateDocuments, target.templateName, "fire.addBarrelName");
    int which = -1;
    for (size_t i = 0; i < barrels.size() && which < 0; ++i)
        if (_stricmp(trimmedValue(barrels[i]).c_str(), target.child.c_str()) == 0)
            which = static_cast<int>(i);
    if (on)
    {
        if (which >= 0)
        {
            result = target.child + " is a barrel of " + target.templateName + " already";
            return true;
        }
        return addObjectTemplateLine(target.templateName, "fire.addBarrelName", target.child, result);
    }
    if (which < 0)
    {
        result = target.child + " is no barrel of " + target.templateName;
        return false;
    }
    char spec[64];
    std::snprintf(spec, sizeof(spec), "fire.addBarrelName#%d", which);
    return removeObjectTemplateProperty(target.templateName, spec, result);
}

bool EditSession::setFireRates(const std::string& name, bool single, bool burst, bool full, std::string& result)
{
    // Retail's Set Fire Rate (0x100258D0) cleared the fire component's list and added 2, 1, 0 for the boxes
    // ticked, in that order, so the saved addFireRate lines run from the highest mode down.
    if (!single && !burst && !full)
    {
        result = "tick at least one fire mode (single, burst, full)";
        return false;
    }
    std::string probe;
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    const size_t existing = propertyValues(m_templateDocuments, realName, "fire.addFireRate").size();
    beginTemplateBatch();
    bool ok = true;
    for (size_t i = 0; i < existing && ok; ++i)
        ok = removeObjectTemplateProperty(realName, "fire.addFireRate#0", probe);
    const bool modes[3] = { full, burst, single };
    const char* const values[3] = { "2", "1", "0" };
    for (int m = 0; m < 3 && ok; ++m)
        if (modes[m])
            ok = addObjectTemplateLine(realName, "fire.addFireRate", values[m], probe);
    std::string label = "Fire modes of " + realName + ":";
    label += full ? " full" : "";
    label += burst ? " burst" : "";
    label += single ? " single" : "";
    endTemplateBatch(label);
    result = ok ? label : probe;
    return ok;
}

bool EditSession::setSeatParent(const std::string& vehicle, const std::string& seat, std::string& result)
{
    // Changes seatParent to the selected template while preserving the seat position.
    dice::hfe::world::IObjectTemplate* seatTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(seat) : 0;
    if (!seatTemplate)
    {
        result = "no template named " + seat;
        return false;
    }
    std::string place = "0/0/0 0/0/0";
    const std::vector<std::string> lines = propertyValues(m_templateDocuments, vehicle, "seatInformation");
    if (!lines.empty())
    {
        const std::string value = trimmedValue(lines.back());
        const size_t blank = value.find_first_of(" \t");
        if (blank != std::string::npos)
            place = trimmedValue(value.substr(blank));
    }
    return setObjectTemplateProperty(vehicle, "seatInformation", seatTemplate->getName() + " " + place, result);
}

bool EditSession::turnTemplateChildren(const std::string& parent, const std::string& child, float degrees, bool following,
                                       std::string& result)
{
    // Rotates the child, or it and following siblings, about the parent's vertical axis.
    // Updates both position and yaw; positive angles follow positive yaw.
    if (!(degrees > -360.0f && degrees < 360.0f))
    {
        result = "the angle is in degrees, -360 to 360";
        return false;
    }
    ChildTarget first;
    if (!findChildTarget(m_templateDocuments, parent, child, first, result))
        return false;
    const int last = following ? first.bundle->getNoTemplates() - 1 : first.index;
    const float a = degrees * 3.14159265f / 180.0f;
    const float c = std::cos(a), s = std::sin(a);
    beginTemplateBatch();
    bool ok = true;
    int turned = 0;
    std::string why;
    for (int i = first.index; i <= last && ok; ++i)
    {
        // Its spec: name#n, the n-th child of that name.
        const std::string name = first.bundle->getTemplate(i);
        int occurrence = 0;
        for (int k = 0; k < i; ++k)
            if (_stricmp(first.bundle->getTemplate(k).c_str(), name.c_str()) == 0)
                ++occurrence;
        char spec[300];
        std::snprintf(spec, sizeof(spec), "%s#%d", name.c_str(), occurrence);
        const dice::hfe::Mat4 m = first.bundle->getTransformation(i);
        dice::hfe::Vec3 position;
        position.x = m.m[12] * c + m.m[14] * s;
        position.y = m.m[13];
        position.z = -m.m[12] * s + m.m[14] * c;
        dice::hfe::Vec3 rotation = rotationOf(m);
        rotation.x += degrees;
        while (rotation.x > 180.0f)
            rotation.x -= 360.0f;
        while (rotation.x <= -180.0f)
            rotation.x += 360.0f;
        ok = moveTemplateChild(first.templateName, spec, position, rotation, why);
        if (ok)
            ++turned;
    }
    char label[300];
    std::snprintf(label, sizeof(label), "Turn %d child%s of %s by %g degrees about it", turned, turned == 1 ? "" : "ren",
                  first.templateName.c_str(), degrees);
    endTemplateBatch(label);
    result = ok ? std::string(label) : why;
    return ok;
}

bool EditSession::resetTemplateChild(const std::string& parent, const std::string& child, std::string& result)
{
    // Retail's Reset Relative Transform (0x10020E50): position and rotation 0/0/0.
    dice::hfe::Vec3 zero;
    zero.x = zero.y = zero.z = 0.0f;
    return moveTemplateChild(parent, child, zero, zero, result);
}

bool EditSession::revertTemplate(const std::string& name, std::string& result)
{
    // Restores last-read or saved text and rebuilds the template, then clears undo history.
    // If level objects use it, replay without deletion leaves added children/components
    // until the next level load.
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    std::vector<size_t> files;
    std::string error;
    if (!m_templateDocuments.filesOf(realName, files, error))
    {
        result = error;
        return false;
    }
    int reverted = 0;
    for (size_t i = 0; i < files.size(); ++i)
        if (m_templateDocuments.document(files[i]).exists && m_templateDocuments.document(files[i]).modified())
            ++reverted;
    if (!reverted)
    {
        result = realName + " has no unsaved edits to revert (its files are as last read or saved)";
        return false;
    }
    for (size_t i = 0; i < files.size(); ++i)
    {
        TemplateDocument& document = m_templateDocuments.document(files[i]);
        if (document.exists && document.modified())
            document.file.parse(document.savedText);
    }
    bool inUse = false;
    const std::map<unsigned int, dice::hfe::world::IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, dice::hfe::world::IObject*>::const_iterator it = roots.begin(); it != roots.end() && !inUse; ++it)
        inUse = it->second && templateHolds(it->second->getTemplate(), objectTemplate, 0);
    const std::string workingFile = dice::hfe::world::objectTemplateManager->getWorkingFolderAndFile(realName);
    std::vector<std::string> refused;
    if (!rebuildFromFiles(m_templateDocuments, files, realName, workingFile, refused, error, !inUse))
    {
        result = "reverting " + realName + ": " + error;
        return false;
    }
    clearHistory("the object template " + realName + " was reverted to its files");
    char text[96];
    std::snprintf(text, sizeof(text), " (%d file%s)", reverted, reverted == 1 ? "" : "s");
    result = "reverted " + realName + " to its files" + text;
    if (inUse)
        result += "; objects of the level use it, so its lines ran again over it (a child or component an edit added "
                  "stays until the level loads again)";
    if (!refused.empty())
        result += "; the engine refused " + refused[0];
    logLine(LogLevel::Info, "template: %s", result.c_str());
    return true;
}

bool EditSession::geometryCommand(const std::string& action, const std::string& name, const std::string& spec,
                                  const std::string& value, std::string& result)
{
    // The geometry template's settings: the GeometryTemplate lines of the template's .tweak
    // (setSubGeometryLodDistance, setLightmapSizeAll, setMaterialParallaxHeightScale, setSpecularStaticGloss ...),
    // edited in place; the geometry reads them when the level loads again (the retail object editor said as much).
    dice::hfe::world::IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    const std::string realName = objectTemplate->getName();
    std::vector<size_t> files;
    std::string error;
    if (!m_templateDocuments.filesOf(realName, files, error))
    {
        result = error;
        return false;
    }
    int tweak = -1;
    for (size_t i = 0; i < files.size(); ++i)
    {
        const std::string& path = m_templateDocuments.document(files[i]).path;
        if (path.size() > 6 && _stricmp(path.c_str() + path.size() - 6, ".tweak") == 0)
            tweak = static_cast<int>(files[i]);
    }
    if (tweak < 0 || !m_templateDocuments.document(static_cast<size_t>(tweak)).exists)
    {
        result = realName + " has no .tweak to keep geometry settings in";
        return false;
    }
    TemplateDocument& document = m_templateDocuments.document(static_cast<size_t>(tweak));
    if (action == "list")
    {
        const std::vector<TemplatePropertyLine> lines = document.file.geometryLines();
        result = document.path + ":";
        for (size_t i = 0; i < lines.size(); ++i)
            result += (i ? "; " : " ") + lines[i].property + " " + lines[i].value;
        if (lines.empty())
            result += " no geometry lines";
        return true;
    }
    if (document.archived)
    {
        result = document.path + " is read from a mounted archive, so the change could not be saved: refused";
        return false;
    }
    std::string property;
    int which = -1;
    if (!splitIndex(spec, property, which))
    {
        result = "'" + spec + "' is not a geometry property (a property, or property#index for one of several lines)";
        return false;
    }
    TemplateEdit edit;
    edit.file = static_cast<size_t>(tweak);
    edit.before = document.file.render();
    edit.templateName = realName;
    const bool ok = action == "set" ? document.file.setGeometryProperty(property, value, which, error)
                  : action == "remove" ? document.file.removeGeometryProperty(property, which, error) : false;
    if (!ok)
    {
        document.file.parse(edit.before);
        result = action == "set" || action == "remove" ? error : "no geometry action '" + action + "'";
        return false;
    }
    edit.after = document.file.render();
    const std::string label = (action == "set" ? "Set GeometryTemplate." : "Remove GeometryTemplate.") + spec + " of " + realName;
    recordTemplateEdit(edit, label);
    result = label + " in " + document.path + " (the geometry reads it when the level loads again)";
    return true;
}

// ---- The preview's parts (the object editor's view) --------------------------------------------------------------
// [added] (issue draft 14): retail's object editor placed a child through its tree's fields; here a part of the
// previewed template - a child, or a seat's soldier exit point - is picked in the view and moved with the gizmo.

namespace
{

dice::hfe::Mat4 product(const dice::hfe::Mat4& a, const dice::hfe::Mat4& b)
{
    dice::hfe::Mat4 out;
    matrixMultiply(a.m, b.m, out.m);
    return out;
}

// The height PlayerControlObject::_getExitPosition adds to the world point of a seat's exit.
const float kExitLift = 0.5f;

} // namespace

std::vector<EditSession::PreviewPart> EditSession::previewParts()
{
    std::vector<PreviewPart> parts;
    dice::hfe::world::IObject* preview = m_preview ? object(m_preview) : 0;
    dice::hfe::world::IObjectTemplate* root =
        preview && dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(m_previewTemplate)
                                                           : 0;
    if (root)
        collectPreviewParts(root, preview->getAbsoluteTransformation(), 0, parts);
    return parts;
}

void EditSession::collectPreviewParts(dice::hfe::world::IObjectTemplate* objectTemplate, const dice::hfe::Mat4& world,
                                      int depth, std::vector<PreviewPart>& parts)
{
    if (!objectTemplate || depth > 12 || parts.size() > 1024)
        return;
    const std::string name = objectTemplate->getName();
    if (objectTemplate->queryInterface(dice::hfe::world::IID_IPlayerControlObjectTemplate))
        addExitParts(name, world, depth, parts);
    dice::hfe::world::IBundleTemplate* bundle = bundleOf(objectTemplate);
    if (!bundle)
        return;
    const auto lowered = [](std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
        return text;
    };
    // The children a line of the template's files adds: the ones a part may be. The engine adds others of its own
    // (a seat's S_<seat>_Ambient sounds), which no edit can move.
    std::map<std::string, int> written;
    std::vector<size_t> files;
    std::string error;
    const bool haveFiles = m_templateDocuments.filesOf(name, files, error);
    for (size_t f = 0; haveFiles && f < files.size(); ++f)
    {
        const std::vector<TemplateChildLines> lines = m_templateDocuments.document(files[f]).file.children(name);
        for (size_t c = 0; c < lines.size(); ++c)
            ++written[lowered(lines[c].child)];
    }
    // A child is named as the child edits name it: its template and its occurrence among the children of that name.
    std::map<std::string, int> seen;
    for (int i = 0; i < bundle->getNoTemplates(); ++i)
    {
        const std::string child = bundle->getTemplate(i);
        const std::string key = lowered(child);
        const int occurrence = seen[key]++;
        if (haveFiles && occurrence >= written[key])
            continue;
        dice::hfe::world::IObjectTemplate* childTemplate = dice::hfe::world::objectTemplateManager->getTemplate(child);
        PreviewPart part;
        part.kind = PreviewPart::Child;
        part.owner = name;
        part.child = child + "#" + std::to_string(occurrence);
        part.label = child;
        part.frame = world;
        part.world = product(bundle->getTransformation(i), world);
        part.depth = depth + 1;
        part.seat = childTemplate && childTemplate->queryInterface(dice::hfe::world::IID_IPlayerControlObjectTemplate);
        part.camera = childTemplate && childTemplate->getClassID() == dice::hfe::world::CID_CameraTemplate;
        parts.push_back(part);
        collectPreviewParts(childTemplate, part.world, depth + 1, parts);
    }
}

void EditSession::addExitParts(const std::string& seat, const dice::hfe::Mat4& world, int depth, std::vector<PreviewPart>& parts)
{
    // The lines as the engine ran them: the defining file, then its .tweak - the last one sets the value.
    std::vector<size_t> files;
    std::string error, exit, alternative;
    if (!m_templateDocuments.filesOf(seat, files, error))
        return;
    for (size_t f = 0; f < files.size(); ++f)
    {
        std::string value;
        if (m_templateDocuments.document(files[f]).file.value(seat, "setSoldierExitLocation", value))
            exit = value;
        if (m_templateDocuments.document(files[f]).file.value(seat, "altSoldierExitPosition", value))
            alternative = value;
    }
    const std::string texts[2] = { exit, alternative };
    for (int k = 0; k < 2; ++k)
    {
        if (texts[k].empty())
            continue;
        const size_t blank = texts[k].find_first_of(" \t");
        const dice::hfe::Vec3 local = vectorOf(texts[k].substr(0, blank));
        // A zero point is no exit of the seat's own: the engine takes its parent seat's then.
        if (!beyond(local, 0.0f))
            continue;
        PreviewPart part;
        part.kind = k == 0 ? PreviewPart::Exit : PreviewPart::AltExit;
        part.owner = seat;
        part.label = k == 0 ? "exit" : "alternative exit";
        if (k == 0 && blank != std::string::npos)
            part.exitRotation = texts[k].substr(texts[k].find_first_not_of(" \t", blank));
        part.frame = world;
        part.world = dice::hfe::Mat4::getIdentity();
        const float p[3] = { local.x, local.y, local.z };
        for (int c = 0; c < 3; ++c)
            part.world.m[12 + c] = p[0] * world.m[c] + p[1] * world.m[4 + c] + p[2] * world.m[8 + c] + world.m[12 + c];
        part.world.m[13] += kExitLift;
        part.depth = depth + 1;
        parts.push_back(part);
    }
}

bool EditSession::movePreviewPart(size_t index, const dice::hfe::Mat4& world, std::string& result)
{
    const std::vector<PreviewPart> parts = previewParts();
    if (index >= parts.size())
    {
        result = "the preview has no part " + std::to_string(index) + " (EditorPreview.parts lists them)";
        return false;
    }
    const PreviewPart& part = parts[index];
    dice::hfe::Mat4 inverse;
    if (!matrixInvertAffine(part.frame.m, inverse.m))
    {
        result = "the part's frame cannot be inverted";
        return false;
    }
    bool moved = false;
    if (part.kind == PreviewPart::Child)
    {
        const dice::hfe::Mat4 local = product(world, inverse);
        dice::hfe::Vec3 position;
        position.x = local.m[12];
        position.y = local.m[13];
        position.z = local.m[14];
        moved = moveTemplateChild(part.owner, part.child, position, rotationOf(local), result);
    }
    else
    {
        const float p[3] = { world.m[12], world.m[13] - kExitLift, world.m[14] };
        dice::hfe::Vec3 local;
        local.x = p[0] * inverse.m[0] + p[1] * inverse.m[4] + p[2] * inverse.m[8] + inverse.m[12];
        local.y = p[0] * inverse.m[1] + p[1] * inverse.m[5] + p[2] * inverse.m[9] + inverse.m[13];
        local.z = p[0] * inverse.m[2] + p[1] * inverse.m[6] + p[2] * inverse.m[10] + inverse.m[14];
        const std::string value = vectorText(local) + (part.kind == PreviewPart::Exit
                                                           ? " " + (part.exitRotation.empty() ? std::string("0/0/0") : part.exitRotation)
                                                           : std::string());
        moved = setObjectTemplateProperty(part.owner, part.kind == PreviewPart::Exit ? "setSoldierExitLocation"
                                                                                     : "altSoldierExitPosition",
                                          value, result);
    }
    if (!moved)
        return false;
    // The preview made again from the template, so it shows the change; the same part stays picked.
    std::string refreshed;
    const int picked = m_pickedPart;
    previewCommand("refresh", std::string(), refreshed);
    m_pickedPart = picked;
    return true;
}

} // namespace bf2editor
