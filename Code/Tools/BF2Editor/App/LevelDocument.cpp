// Code/Tools/BF2Editor/App/LevelDocument.cpp - see LevelDocument.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/LevelDocument.h"

#include "App/EditSession.h"
#include "App/GameplayLayerModel.h"
#include "Framework/EditorLog.h"
#include "Framework/MapDescription.h"
#include "Framework/SavePlan.h"
#include "Framework/SourceControl.h"

#include "Game/GameLogic/CombatArea.h"
#include "Game/GameLogic/CombatAreaManager.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/IObjectTemplate.h"
#include "Scene/Object/ObjectManager.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>

using dice::hfe::Mat4;
using dice::hfe::world::IObject;

namespace bf2editor
{

namespace
{

bool isFile(const std::string& path)
{
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// `relative` ("mods/bf2/Levels/X/GameModes/gpm_cq/64/GamePlayObjects.con") spelled as it is on disk,
// one component at a time, so a save replaces the file under its own name (the stock folders are
// "gamemodes", not "GameModes"); false when a component does not exist.
bool onDisk(const std::string& relative, std::string& actual)
{
    actual.clear();
    size_t start = 0;
    for (;;)
    {
        size_t slash = relative.find('/', start);
        if (slash == std::string::npos)
            slash = relative.size();
        const std::string component = relative.substr(start, slash - start);
        if (component.empty() || component.find_first_of("*?") != std::string::npos)
            return false;
        WIN32_FIND_DATAA found;
        const HANDLE search = FindFirstFileA((actual + component).c_str(), &found);
        if (search == INVALID_HANDLE_VALUE)
            return false;
        FindClose(search);
        actual += found.cFileName;
        if (slash == relative.size())
            return true;
        actual += '/';
        start = slash + 1;
    }
}

bool readWhole(const std::string& path, std::string& text)
{
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    if (!in)
        return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    text = buffer.str();
    return true;
}

bool sameTransform(const Mat4& a, const Mat4& b)
{
    return std::memcmp(a.m, b.m, sizeof(a.m)) == 0;
}

} // namespace

LevelDocument::LevelDocument()
    : m_open(false)
{
}

LevelDocument::Snapshot LevelDocument::snapshotOf(IObject* object)
{
    Snapshot snapshot;
    snapshot.transform = object->getAbsoluteTransformation();
    snapshot.layer = static_cast<int>(object->getLayer());
    const ObjectProperties p = EditSession::propertiesOf(object);
    snapshot.group = p.group;
    snapshot.disableChildren = p.disableChildren;
    snapshot.notInAI = p.notInAI;
    snapshot.isOvergrowth = p.isOvergrowth;
    snapshot.lightSourceMask = p.lightSourceMask;
    snapshot.controlPointId = p.controlPointId;
    return snapshot;
}

bool LevelDocument::gameplayFile(size_t& file) const
{
    const std::string suffix = "gameplayobjects.con";
    for (size_t f = 0; f < m_files.size(); ++f)
    {
        std::string name = m_files[f].name;
        for (size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
        if (name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        {
            file = f;
            return true;
        }
    }
    return false;
}

LevelSettingsFile* LevelDocument::settingsFile(const std::string& name)
{
    for (size_t i = 0; i < m_settings.size(); ++i)
        if (_stricmp(m_settings[i].name.c_str(), name.c_str()) == 0)
            return &m_settings[i].file;
    return 0;
}

void LevelDocument::planSettingsFile(SavePlan& plan, size_t index, bool always)
{
    SettingsFile& settings = m_settings[index];
    const std::string text = settings.file.render();
    if (text == settings.file.text() && !always)
        return;
    SavePart& part = plan.part(settings.name);
    part.add(settings.path, text, always);
    part.onWritten([this, index, text]() { m_settings[index].file.parse(text); });
}

bool LevelDocument::ambientFile(size_t& file) const
{
    for (size_t f = 0; f < m_files.size(); ++f)
    {
        if (_stricmp(m_files[f].name.c_str(), "AmbientObjects.con") == 0)
        {
            file = f;
            return true;
        }
    }
    return false;
}

std::vector<std::pair<std::string, int> > LevelDocument::layersOnDisk() const
{
    std::vector<std::pair<std::string, int> > layers;
    std::string modes;
    if (!onDisk(m_levelDirectory + "GameModes", modes))
        return layers;
    WIN32_FIND_DATAA found;
    const HANDLE search = FindFirstFileA((modes + "/*").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return layers;
    do
    {
        const std::string mode = found.cFileName;
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || mode == "." || mode == "..")
            continue;
        static const int kSizes[] = { 16, 32, 64, 128 };
        for (size_t i = 0; i < sizeof(kSizes) / sizeof(kSizes[0]); ++i)
        {
            char relative[64];
            std::snprintf(relative, sizeof(relative), "/%d/GamePlayObjects.con", kSizes[i]);
            if (isFile(modes + "/" + mode + relative))
                layers.push_back(std::make_pair(mode, kSizes[i]));
        }
    } while (FindNextFileA(search, &found));
    FindClose(search);
    std::sort(layers.begin(), layers.end());
    return layers;
}

bool LevelDocument::readLayer(const std::string& gameMode, int size, LevelObjectFile& out) const
{
    char relative[128];
    std::snprintf(relative, sizeof(relative), "GameModes/%s/%d/GamePlayObjects.con", gameMode.c_str(), size);
    std::string path, text;
    if (gameMode.find_first_of("/\\.:") != std::string::npos || !onDisk(m_levelDirectory + relative, path) || !isFile(path)
        || !readWhole(path, text))
        return false;
    out.parse(text);
    return true;
}

std::string LevelDocument::descriptionPath() const
{
    // The level's folder name is the last component of its directory.
    const std::string directory = m_levelDirectory.substr(0, m_levelDirectory.size() - 1);
    const std::string level = directory.substr(directory.rfind('/') + 1);
    std::string path;
    if (!onDisk(m_levelDirectory + "Info/" + level + ".desc", path) || !isFile(path))
        return std::string();
    return path;
}

bool LevelDocument::describedLayers(std::vector<std::pair<std::string, int> >& layers) const
{
    std::string xml;
    const std::string path = descriptionPath();
    if (path.empty() || !readWhole(path, xml))
        return false;
    layers = mapDescriptionLayers(xml);
    return true;
}

bool LevelDocument::duplicateLayer(const std::string& gameMode, int size,
                                   std::vector<std::pair<std::string, std::string> >& files, bool& listed,
                                   std::string& error)
{
    listed = false;
    files.clear();
    size_t file = 0;
    if (!m_open || !gameplayFile(file))
    {
        error = "no gameplay layer is open to copy";
        return false;
    }
    if (gameMode.empty() || gameMode.find_first_of("/\\.:* \t") != std::string::npos
        || (size != 16 && size != 32 && size != 64 && size != 128))
    {
        error = "a layer is a game mode folder name and a size of 16, 32, 64 or 128";
        return false;
    }
    // The copy is of the file as saved: unsaved changes would be in neither layer's file.
    if (m_files[file].objects.render() != m_files[file].text)
    {
        error = "the open layer has unsaved changes; save it first (the copy is of the file as saved)";
        return false;
    }
    std::string modes;
    if (!onDisk(m_levelDirectory + "GameModes", modes))
    {
        error = m_levelDirectory + "GameModes is not a folder";
        return false;
    }
    char sizeText[16];
    std::snprintf(sizeText, sizeof(sizeText), "%d", size);
    std::string modeFolder;
    if (!onDisk(m_levelDirectory + "GameModes/" + gameMode, modeFolder))
        modeFolder = modes + "/" + gameMode;
    const std::string sizeFolder = modeFolder + "/" + sizeText;
    const std::string target = sizeFolder + "/GamePlayObjects.con";
    if (isFile(target))
    {
        error = target + " exists already";
        return false;
    }
    files.push_back(std::make_pair(target, m_files[file].text));

    // The map description lists the new layer, or the game does not offer it.
    const std::string description = descriptionPath();
    std::string xml;
    if (description.empty() || !readWhole(description, xml))
    {
        logLine(LogLevel::Warning, "document: the level has no Info/<level>.desc to list the new layer in");
        return true;
    }
    if (!addMapDescriptionLayer(xml, gameMode, size))
        return true;                                        // listed already, or no modes to add it to
    files.push_back(std::make_pair(description, xml));
    listed = true;
    return true;
}

int LevelDocument::liveRecords(const std::string& templateName) const
{
    int count = 0;
    for (size_t f = 0; f < m_files.size(); ++f)
    {
        const LevelObjectFile& objects = m_files[f].objects;
        for (size_t r = 0; r < objects.recordCount(); ++r)
            if (!objects.isDeleted(r) && _stricmp(objects.placement(r).templateName.c_str(), templateName.c_str()) == 0)
                ++count;
    }
    return count;
}

bool LevelDocument::addTemplate(size_t file, const std::string& className, const std::string& name,
                                const std::vector<std::pair<std::string, std::string> >& lines, bool addTrigger)
{
    return file < m_files.size() && m_files[file].objects.appendTemplate(className, name, lines, addTrigger);
}

bool LevelDocument::soundRecord(size_t file, const std::string& name, RecordRef& ref) const
{
    if (file >= m_files.size())
        return false;
    const LevelObjectFile& objects = m_files[file].objects;
    bool found = false;
    for (size_t r = 0; r < objects.recordCount(); ++r)
    {
        if (_stricmp(objects.placement(r).templateName.c_str(), name.c_str()) != 0)
            continue;
        if (!found || (objects.isDeleted(ref.record) && !objects.isDeleted(r)))
        {
            ref.file = file;
            ref.record = r;
            found = true;
        }
    }
    return found;
}

void LevelDocument::setRecordPlacement(const RecordRef& ref, const ObjectPlacement& placement)
{
    if (ref.file < m_files.size() && ref.record < m_files[ref.file].objects.recordCount())
        m_files[ref.file].objects.setPlacement(ref.record, placement);
}

bool LevelDocument::setTemplateDeleted(size_t file, const std::string& name, bool deleted)
{
    return file < m_files.size() && m_files[file].objects.setTemplateDeleted(name, deleted);
}

bool LevelDocument::renameTemplate(size_t file, const std::string& from, const std::string& to)
{
    return file < m_files.size() && m_files[file].objects.renameTemplate(from, to);
}

size_t LevelDocument::renameObjectRecords(const std::string& from, const std::string& to)
{
    size_t count = 0;
    for (size_t f = 0; f < m_files.size(); ++f)
        count += m_files[f].objects.renameObjectRecords(from, to);
    return count;
}

bool LevelDocument::syncCombatArea(size_t file, const std::string& name, const LevelObjectFile::CombatAreaData* data,
                                   std::string& error)
{
    if (file >= m_files.size())
    {
        error = "no such file";
        return false;
    }
    LevelObjectFile& objects = m_files[file].objects;
    size_t area = 0;
    const bool known = objects.findCombatArea(name, area);
    if (!data)
    {
        if (known)
            objects.setCombatAreaDeleted(area, true);
        return true;
    }
    if (!known)
    {
        objects.appendCombatArea(*data);
        return true;
    }
    objects.setCombatAreaDeleted(area, false);
    if (!objects.setCombatArea(area, *data))
    {
        error = "combat area " + name + "'s block in " + m_files[file].name + " has lines the editor does not write";
        return false;
    }
    return true;
}

bool LevelDocument::templateFile(const std::string& name, size_t& file, std::string& className) const
{
    for (size_t f = 0; f < m_files.size(); ++f)
    {
        if (m_files[f].objects.findTemplate(name, className))
        {
            file = f;
            return true;
        }
    }
    return false;
}

std::vector<LevelObjectFile::TemplateLine> LevelDocument::templateLines(size_t file, const std::string& name) const
{
    return m_files[file].objects.templateLines(name);
}

bool LevelDocument::setTemplateValue(size_t file, const std::string& name, const std::string& property,
                                     const std::string& value)
{
    return m_files[file].objects.setTemplateValue(name, property, value);
}

bool LevelDocument::placementOf(unsigned id, ObjectPlacement& out) const
{
    RecordRef ref;
    if (!recordOf(id, ref))
        return false;
    out = m_files[ref.file].objects.placement(ref.record);
    return true;
}

bool LevelDocument::open(const std::string& mod, const std::string& level, const std::string& gameMode, int layerSize,
                         std::string& error, GameplayLayerModel* layers,
                         const std::function<void(const std::vector<std::string>&)>& runLines)
{
    m_open = false;
    m_files.clear();
    m_settings.clear();
    m_recordById.clear();
    m_snapshot.clear();

    // The level's own folder under the mod. A level the mod takes from bf2 would be written into bf2,
    // so it is not opened for saving.
    const std::string inMod = "mods/" + mod + "/Levels/" + level + "/StaticObjects.con";
    std::string staticPath, initPath, archivePath;
    if (!onDisk(inMod, staticPath) || !isFile(staticPath))
    {
        // A new level (EditorLevel.create, as the retail editor made one) has no objects file until its first
        // save: a loose level folder with its Init.con and no archives starts with an empty StaticObjects.con.
        const std::string folder = "mods/" + mod + "/Levels/" + level + "/";
        if (onDisk(folder + "Init.con", initPath) && isFile(initPath) && !onDisk(folder + "server.zip", archivePath))
        {
            m_levelDirectory = initPath.substr(0, initPath.rfind('/') + 1);
            File file;
            file.path = m_levelDirectory + "StaticObjects.con";
            file.name = "StaticObjects.con";
            file.objects.parse(file.text);
            m_files.push_back(file);
            logLine(LogLevel::Info, "document: %sStaticObjects.con is not there yet (a new level): the first save of an "
                    "object writes it", m_levelDirectory.c_str());
        }
        else
        {
            if (_stricmp(mod.c_str(), "bf2") != 0 && isFile("mods/bf2/Levels/" + level + "/StaticObjects.con"))
                error = "the level comes from mods/bf2, not from mods/" + mod + "; copy it into the mod to save it";
            else
                error = inMod + " is not a file: the level may be packed in archives - extract it to save it";
            return false;
        }
    }
    else
    {
        m_levelDirectory = staticPath.substr(0, staticPath.rfind('/') + 1);
        if (!openFile("StaticObjects.con", true, error))
            return false;
    }
    std::string warning;
    if (!openFile("AmbientObjects.con", false, warning))
        logLine(LogLevel::Warning, "document: %s", warning.c_str());
    // The gameplay objects: the retail layer model's editor file when it holds (E14), else the loaded game-mode file.
    bool editorFile = false;
    if (layers && !gameMode.empty() && layerSize > 0)
    {
        std::string report;
        layers->open(m_levelDirectory, mod, gameMode, layerSize, report);
        logLine(layers->active() ? LogLevel::Info : LogLevel::Warning, "document: %s", report.c_str());
        for (size_t i = 0; i < layers->notes().size(); ++i)
            logLine(LogLevel::Info, "document: gameplay layers: left as it is - %s", layers->notes()[i].c_str());
        if (layers->active())
        {
            // The other layers' objects, which the engine's load of one game-mode file did not make.
            const std::vector<std::string> lines = layers->missingObjectLines([](const std::string& name, bool area) {
                if (area)
                {
                    const dice::hfe::CombatAreaManager* manager = dice::hfe::CombatAreaManager::getInstance();
                    for (size_t i = 0; manager && i < manager->m_areas.size(); ++i)
                        if (manager->m_areas[i] && _stricmp(manager->m_areas[i]->m_name.c_str(), name.c_str()) == 0)
                            return true;
                    return false;
                }
                return dice::hfe::world::objectTemplateManager && dice::hfe::world::objectTemplateManager->getTemplate(name) != 0;
            });
            if (!lines.empty() && runLines)
                runLines(lines);
            File file;
            file.path = layers->editorPath();
            file.name = file.path.substr(m_levelDirectory.size());
            file.text = layers->editorText();
            file.objects.parse(file.text);
            if (file.objects.render() == file.text)
            {
                m_files.push_back(file);
                editorFile = true;
            }
            else
                logLine(LogLevel::Warning, "document: %s cannot be written back unchanged; the loaded game-mode file is edited "
                        "instead", file.path.c_str());
        }
    }
    if (!editorFile && !gameMode.empty() && layerSize > 0)
    {
        if (layers)
            layers->close();
        char layer[128];
        std::snprintf(layer, sizeof(layer), "GameModes/%s/%d/GamePlayObjects.con", gameMode.c_str(), layerSize);
        if (!openFile(layer, false, warning))
            logLine(LogLevel::Warning, "document: %s", warning.c_str());
    }

    // The settings files (phase D), each only when it can be written back unchanged.
    static const char* const kSettingsFiles[] = { "Init.con", "Sky.con", "Water.con" };
    for (size_t i = 0; i < sizeof(kSettingsFiles) / sizeof(kSettingsFiles[0]); ++i)
    {
        SettingsFile settings;
        std::string text;
        if (!onDisk(m_levelDirectory + kSettingsFiles[i], settings.path) || !isFile(settings.path)
            || !readWhole(settings.path, text))
        {
            logLine(LogLevel::Info, "document: %s%s is not a file; its settings cannot be saved", m_levelDirectory.c_str(),
                    kSettingsFiles[i]);
            continue;
        }
        settings.name = settings.path.substr(m_levelDirectory.size());
        settings.file.parse(text);
        if (settings.file.render() != text)
        {
            logLine(LogLevel::Warning, "document: %s: the editor cannot write it back unchanged, so it will not save it",
                    settings.path.c_str());
            continue;
        }
        m_settings.push_back(settings);
    }

    // Every root object as the load left it, and the candidates for binding.
    std::vector<SceneObject> scene;
    if (dice::hfe::world::objectManager)
    {
        const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
        for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        {
            IObject* object = it->second;
            if (!object)
                continue;
            const Mat4& m = object->getAbsoluteTransformation();
            m_snapshot[it->first] = snapshotOf(object);
            if (!object->getTemplate())
                continue;
            SceneObject entry;
            entry.id = it->first;
            entry.templateName = object->getTemplate()->getName();
            entry.position[0] = m.m[12];
            entry.position[1] = m.m[13];
            entry.position[2] = m.m[14];
            scene.push_back(entry);
        }
    }

    // File after file; an object bound to a record is not offered to the next file.
    for (size_t f = 0; f < m_files.size(); ++f)
    {
        File& file = m_files[f];
        file.ids = bindRecords(file.objects, scene, 0.01f);
        const std::set<unsigned> used(file.ids.begin(), file.ids.end());
        std::vector<SceneObject> rest;
        rest.reserve(scene.size());
        for (size_t i = 0; i < scene.size(); ++i)
            if (!used.count(scene[i].id))
                rest.push_back(scene[i]);
        scene.swap(rest);
        indexIds(f);

        const size_t bound = boundRecords(f);
        logLine(LogLevel::Info, "document: %s: %u object records, %u bound to loaded objects, %u kept as written",
                file.path.c_str(), static_cast<unsigned>(file.objects.recordCount()), static_cast<unsigned>(bound),
                static_cast<unsigned>(file.objects.recordCount() - bound));
        // Which templates found no loaded object, most frequent first (the log's explanation of the rest).
        std::map<std::string, int> unbound;
        for (size_t r = 0; r < file.ids.size(); ++r)
            if (!file.ids[r])
                ++unbound[file.objects.placement(r).templateName];
        std::vector<std::pair<int, std::string> > ranked;
        for (std::map<std::string, int>::const_iterator it = unbound.begin(); it != unbound.end(); ++it)
            ranked.push_back(std::make_pair(-it->second, it->first));
        std::sort(ranked.begin(), ranked.end());
        std::string names;
        for (size_t i = 0; i < ranked.size() && i < 8; ++i)
        {
            char count[16];
            std::snprintf(count, sizeof(count), "%d x ", -ranked[i].first);
            names += (i ? ", " : "") + std::string(count) + ranked[i].second;
        }
        if (!names.empty())
            logLine(LogLevel::Info, "document: %s: records with no loaded object: %s%s", file.name.c_str(),
                    names.c_str(), ranked.size() > 8 ? ", ..." : "");
    }
    m_open = true;
    return true;
}

bool LevelDocument::openFile(const std::string& name, bool required, std::string& error)
{
    std::string path;
    if (!onDisk(m_levelDirectory + name, path) || !isFile(path))
    {
        error = m_levelDirectory + name + " is not a file" +
                (required ? std::string() : std::string(": its objects cannot be saved"));
        return false;
    }
    File file;
    file.path = path;
    file.name = path.substr(m_levelDirectory.size());
    if (!readWhole(path, file.text))
    {
        error = "cannot read " + path;
        return false;
    }
    file.objects.parse(file.text);
    // Whatever the save does, a file the parser cannot give back unchanged is not saved.
    if (file.objects.render() != file.text)
    {
        error = path + ": the editor cannot write this file back unchanged, so it will not save it";
        return false;
    }
    m_files.push_back(file);
    return true;
}

void LevelDocument::indexIds(size_t file)
{
    const std::vector<unsigned>& ids = m_files[file].ids;
    for (size_t r = 0; r < ids.size(); ++r)
    {
        if (!ids[r])
            continue;
        RecordRef ref;
        ref.file = file;
        ref.record = r;
        m_recordById[ids[r]] = ref;
    }
}

size_t LevelDocument::boundRecords(size_t file) const
{
    const std::vector<unsigned>& ids = m_files[file].ids;
    return static_cast<size_t>(ids.size() - std::count(ids.begin(), ids.end(), 0u));
}

bool LevelDocument::recordOf(unsigned id, RecordRef& ref) const
{
    std::map<unsigned, RecordRef>::const_iterator found = m_recordById.find(id);
    if (found == m_recordById.end())
        return false;
    ref = found->second;
    return true;
}

unsigned LevelDocument::idOf(const RecordRef& ref) const
{
    if (ref.file >= m_files.size() || ref.record >= m_files[ref.file].ids.size())
        return 0;
    return m_files[ref.file].ids[ref.record];
}

LevelDocument::RecordRef LevelDocument::addRecord(size_t f, unsigned id, const ObjectPlacement& placement)
{
    File& file = m_files[f];
    RecordRef ref;
    ref.file = f;
    ref.record = file.objects.appendRecord(placement);
    file.ids.resize(file.objects.recordCount(), 0);
    file.ids[ref.record] = id;
    if (id)
    {
        m_recordById[id] = ref;
        // Later changes are measured from the object as it was made.
        IObject* object = EditSession::object(id);
        if (object)
            m_snapshot[id] = snapshotOf(object);
    }
    return ref;
}

void LevelDocument::setRecordDeleted(const RecordRef& ref, bool deleted)
{
    m_files[ref.file].objects.setDeleted(ref.record, deleted);
}

void LevelDocument::rebind(const RecordRef& ref, unsigned id)
{
    if (ref.file >= m_files.size() || ref.record >= m_files[ref.file].ids.size())
        return;
    std::vector<unsigned>& ids = m_files[ref.file].ids;
    const unsigned previous = ids[ref.record];
    if (previous)
    {
        m_recordById.erase(previous);
        // The object made again stands for the one loaded: its later changes are measured from the
        // same starting point.
        std::map<unsigned, Snapshot>::const_iterator snapshot = m_snapshot.find(previous);
        if (snapshot != m_snapshot.end() && id)
            m_snapshot[id] = snapshot->second;
    }
    ids[ref.record] = id;
    if (id)
        m_recordById[id] = ref;
}

void LevelDocument::updatePreload()
{
    // The templates of objects made in the editor, where the level's preload block lacks them. The
    // path is the file the template was defined in (ObjectTemplateManager::getWorkingFolderAndFile),
    // written as the block writes its own: "run /<path>". Only StaticObjects.con takes new objects.
    if (m_files.empty())
        return;
    LevelObjectFile& objects = m_files[0].objects;
    std::vector<std::string> extra;
    objects.setExtraPreload(extra);
    if (!objects.hasPreloadBlock() || !dice::hfe::world::objectTemplateManager)
        return;
    for (size_t r = 0; r < objects.recordCount(); ++r)
    {
        if (!objects.isNew(r) || objects.isDeleted(r))
            continue;
        std::string path = dice::hfe::world::objectTemplateManager->getWorkingFolderAndFile(objects.placement(r).templateName);
        if (path.empty())
            continue;
        for (size_t i = 0; i < path.size(); ++i)
            if (path[i] == '\\')
                path[i] = '/';
        const std::string line = "run " + (path[0] == '/' ? path : "/" + path);
        if (!objects.preloads(line))
        {
            extra.push_back(line);
            objects.setExtraPreload(extra);
        }
    }
}

bool LevelDocument::changedSinceOpen(unsigned id) const
{
    std::map<unsigned, Snapshot>::const_iterator found = m_snapshot.find(id);
    IObject* object = EditSession::object(id);
    if (found == m_snapshot.end() || !object)
        return false;
    const Snapshot now = snapshotOf(object);
    const Snapshot& then = found->second;
    return !sameTransform(now.transform, then.transform) || now.layer != then.layer || now.group != then.group
        || now.disableChildren != then.disableChildren || now.notInAI != then.notInAI
        || now.isOvergrowth != then.isOvergrowth || now.lightSourceMask != then.lightSourceMask
        || now.controlPointId != then.controlPointId;
}

void LevelDocument::pullFromEngine()
{
    for (size_t f = 0; f < m_files.size(); ++f)
    {
        File& file = m_files[f];
        for (size_t r = 0; r < file.objects.recordCount(); ++r)
        {
            const unsigned id = file.ids[r];
            if (!id || file.objects.isDeleted(r))
                continue;
            IObject* object = EditSession::object(id);
            if (!object)
                continue;
            // An object as it was loaded keeps the record's own values: nothing is re-derived from the
            // engine's matrix unless the object changed.
            if (!file.objects.isNew(r) && !changedSinceOpen(id))
            {
                file.objects.setPlacement(r, file.objects.loadedPlacement(r));
                continue;
            }
            ObjectPlacement placement = file.objects.placement(r);
            const Mat4& m = object->getAbsoluteTransformation();
            for (int i = 0; i < 3; ++i)
                placement.position[i] = m.m[12 + i];
            const dice::hfe::Vec3 angles = EditSession::rotationOf(m);
            placement.rotation[0] = angles.x;
            placement.rotation[1] = angles.y;
            placement.rotation[2] = angles.z;
            const int layer = static_cast<int>(object->getLayer());
            std::map<unsigned, Snapshot>::const_iterator snapshot = m_snapshot.find(id);
            if (file.objects.isNew(r) || placement.hasLayer || snapshot == m_snapshot.end()
                || layer != snapshot->second.layer)
            {
                placement.layer = layer;
                placement.hasLayer = true;
            }
            // The per-object properties as the engine holds them. The mask is written once it was
            // written before or has changed; the others by value (LevelObjectFile).
            const ObjectProperties p = EditSession::propertiesOf(object);
            placement.group = p.group;
            placement.disableChildren = p.disableChildren;
            placement.notInAI = p.notInAI;
            placement.isOvergrowth = p.isOvergrowth;
            placement.lightSourceMask = p.lightSourceMask;
            if (snapshot == m_snapshot.end() || p.lightSourceMask != snapshot->second.lightSourceMask)
                placement.hasLightSourceMask = true;
            // A spawner's control point, written unless it is -1 (ObjectSpawner::makeScript's rule).
            if (p.isSpawner)
            {
                placement.controlPointId = p.controlPointId;
                placement.hasControlPointId = p.controlPointId != -1;
            }
            file.objects.setPlacement(r, placement);
        }
    }
}

LevelDocument::Report LevelDocument::inspect()
{
    pullFromEngine();
    updatePreload();
    Report report;
    report.moved = report.added = report.deleted = 0;
    for (size_t f = 0; f < m_files.size(); ++f)
    {
        const LevelObjectFile& objects = m_files[f].objects;
        FileReport counts;
        counts.moved = counts.added = counts.deleted = 0;
        for (size_t r = 0; r < objects.recordCount(); ++r)
        {
            if (objects.isNew(r))
                counts.added += objects.isDeleted(r) ? 0 : 1;
            else if (objects.isDeleted(r))
                ++counts.deleted;
            else if (objects.isEdited(r))
                ++counts.moved;
        }
        counts.templates = objects.editedTemplates();
        counts.areas = objects.editedCombatAreas();
        counts.areaSettings = objects.combatAreaSettingsChanged();
        counts.changed = objects.hasChanges();
        report.moved += counts.moved;
        report.added += counts.added;
        report.deleted += counts.deleted;
        report.files.push_back(counts);
    }
    for (std::map<unsigned, Snapshot>::const_iterator it = m_snapshot.begin(); it != m_snapshot.end(); ++it)
        if (m_recordById.find(it->first) == m_recordById.end() && changedSinceOpen(it->first))
            report.unsaved.push_back(it->first);
    for (size_t i = 0; i < m_settings.size(); ++i)
        if (m_settings[i].file.hasChanges())
            report.settings.push_back(m_settings[i].name);
    return report;
}

std::string LevelDocument::describe(const Report& report, bool everyFile) const
{
    std::string text;
    for (size_t f = 0; f < m_files.size() && f < report.files.size(); ++f)
    {
        const FileReport& counts = report.files[f];
        if (!everyFile && !counts.changed)
            continue;
        char numbers[96];
        if (takesNewObjects(f) || counts.added || counts.deleted)
            std::snprintf(numbers, sizeof(numbers), "%d changed, %d added, %d deleted", counts.moved, counts.added,
                          counts.deleted);
        else
            std::snprintf(numbers, sizeof(numbers), "%d changed", counts.moved);
        if (counts.templates)
        {
            char templates[48];
            std::snprintf(templates, sizeof(templates), ", %d template%s", counts.templates, counts.templates == 1 ? "" : "s");
            std::strcat(numbers, templates);
        }
        if (counts.areas)
        {
            char areas[48];
            std::snprintf(areas, sizeof(areas), ", %d combat area%s", counts.areas, counts.areas == 1 ? "" : "s");
            std::strcat(numbers, areas);
        }
        if (counts.areaSettings)
            std::strcat(numbers, ", combat area settings");
        text += (text.empty() ? "" : "; ") + m_files[f].name + ": " + numbers;
    }
    for (size_t i = 0; i < report.settings.size(); ++i)
        text += (text.empty() ? "" : "; ") + report.settings[i] + ": changed";
    return text.empty() ? std::string("no changes") : text;
}

void LevelDocument::planFile(SavePlan& plan, size_t f, bool always)
{
    File& file = m_files[f];
    std::vector<size_t> order;
    const std::string text = file.objects.render(&order);
    if (text == file.text && !always)
        return;                                             // nothing in this file changed
    SavePart& part = plan.part(file.name);
    // The text must read back as the records it is written from, or they could not be bound to it again.
    LevelObjectFile check;
    check.parse(text);
    if (check.recordCount() != order.size())
    {
        part.refuse(file.path + " NOT saved: it would read back with a different number of objects");
        return;
    }
    part.add(file.path, text, always);
    part.onWritten([this, f, text, order]() { bindWritten(f, text, order); });
}

void LevelDocument::bindWritten(size_t f, const std::string& text, const std::vector<size_t>& order)
{
    File& file = m_files[f];
    std::vector<unsigned> ids;
    for (size_t k = 0; k < order.size(); ++k)
        ids.push_back(file.ids[order[k]]);
    for (size_t r = 0; r < file.ids.size(); ++r)
    {
        std::map<unsigned, RecordRef>::iterator entry = m_recordById.find(file.ids[r]);
        if (file.ids[r] && entry != m_recordById.end() && entry->second.file == f)
            m_recordById.erase(entry);
    }
    file.text = text;
    file.objects.parse(text);
    if (file.objects.recordCount() != ids.size())
    {
        m_open = false;
        logLine(LogLevel::Error, "document: %s was written, but reads back with a different number of objects; reopen the level",
                file.path.c_str());
        return;
    }
    file.ids = ids;
    indexIds(f);
    for (size_t r = 0; r < file.ids.size(); ++r)
    {
        IObject* object = EditSession::object(file.ids[r]);
        if (!object)
            continue;
        m_snapshot[file.ids[r]] = snapshotOf(object);
    }
}

bool LevelDocument::planSave(SavePlan& plan, Report& report, bool always, std::string& error)
{
    if (!m_open)
    {
        error = "no level document is open";
        return false;
    }
    report = inspect();
    for (size_t f = 0; f < m_files.size(); ++f)
        planFile(plan, f, always);
    for (size_t i = 0; i < m_settings.size(); ++i)
        planSettingsFile(plan, i, always);
    return true;
}

} // namespace bf2editor
