// Code/Tools/BF2Editor/App/RoadNetwork.cpp - see RoadNetwork.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/RoadNetwork.h"

#include "App/StagingFolder.h"

#include "Framework/EditorLog.h"

#include "Core/ClassManager.h"
#include "Core/Ids.h"
#include "Core/SettingsRepostitory.h"
#include "Core/SmartItf.h"
#include "Geom/IRoadEditable.h"
#include "Geom/IRoadEditableRenderer.h"
#include "Geom/IRoadTemplate.h"
#include "GeomMisc/IEditorSpline.h"
#include "GeomMisc/IPerlinNoise.h"
#include "IO/File/File.h"
#include "IO/File/FileManager.h"
#include "Physics/Material.h"
#include "Physics/MaterialManager.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <sstream>

namespace dice { namespace hfe { namespace world { extern IBase* materialManager; } } }   // Core/Globals.cpp

namespace bf2editor
{

namespace
{

using dice::hfe::IBase;
using dice::hfe::Vec2;
using dice::hfe::Vec3;
using dice::hfe::geom::IEditorSpline;
using dice::hfe::geom::IPerlinNoise;
using dice::hfe::geom::IRoadEditable;
using dice::hfe::geom::IRoadEditableRenderer;
using dice::hfe::geom::IRoadTemplate;

const char kTemplateFolder[] = "Objects/Roads/Splines/";
// What the retail road handler gave a new road's edge noise.
const float kNoisePersistence = 0.2f;
const float kNoiseRange[2] = { -0.15f, 0.15f };
const int   kNoiseOctaves = 3;

std::string trimmed(const std::string& text)
{
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

std::string lowered(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

bool sameName(const std::string& a, const std::string& b)
{
    return _stricmp(a.c_str(), b.c_str()) == 0;
}

std::string number(int value)
{
    char text[24];
    std::snprintf(text, sizeof(text), "%d", value);
    return text;
}

// The retail rows name properties with or without the console's "set" prefix (Road.SetRange, Road.Prio).
std::string propertyKey(const std::string& method)
{
    const std::string low = lowered(method);
    return low.compare(0, 3, "set") == 0 && low.size() > 3 ? low.substr(3) : low;
}

bool parseInt(const std::string& text, int& value)
{
    const std::string t = trimmed(text);
    if (t.empty())
        return false;
    char* end = 0;
    const long parsed = std::strtol(t.c_str(), &end, 10);
    if (end == t.c_str() || *end)
        return false;
    value = static_cast<int>(parsed);
    return true;
}

bool parseFloat(const std::string& text, float& value)
{
    const std::string t = trimmed(text);
    if (t.empty())
        return false;
    char* end = 0;
    const double parsed = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || *end)
        return false;
    value = static_cast<float>(parsed);
    return true;
}

bool parseBool(const std::string& text, bool& value)
{
    const std::string t = lowered(trimmed(text));
    if (t == "1" || t == "true")
        value = true;
    else if (t == "0" || t == "false")
        value = false;
    else
        return false;
    return true;
}

Vec2 vec2(const float v[2])
{
    Vec2 out;
    out.x = v[0];
    out.y = v[1];
    return out;
}

Vec3 vec3(const float v[3])
{
    Vec3 out;
    out.x = v[0];
    out.y = v[1];
    out.z = v[2];
    return out;
}

void put2(float out[2], const Vec2& v)
{
    out[0] = v.x;
    out[1] = v.y;
}

// A file as the engine's file manager reads it (the mod's archives or its loose folders).
bool readEngineFile(const std::string& path, std::string& data)
{
    using namespace dice::hfe;
    if (!io::fileManager)
        return false;
    io::File file;
    if (!file.open(path, io::IFileSystem::FILEMODE_READ, static_cast<io::IFileSystem::FileAccess>(0), 0))
        return false;
    const int size = file.getSize();
    if (size < 0)
        return false;
    data.resize(static_cast<size_t>(size));
    return size == 0 || file.read(&data[0], static_cast<unsigned>(size)) == static_cast<unsigned>(size);
}

void makeFolders(const std::string& path)
{
    for (size_t i = 0; i < path.size(); ++i)
        if ((path[i] == '/' || path[i] == '\\') && i > 2)
            CreateDirectoryA(path.substr(0, i).c_str(), 0);
}

bool writeWhole(const std::string& path, const std::string& data)
{
    makeFolders(path);
    const HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    const BOOL ok = data.empty() || WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, 0);
    CloseHandle(file);
    return ok && written == data.size();
}

bool readWhole(const std::string& path, std::string& data)
{
    const HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    const DWORD size = GetFileSize(file, 0);
    data.resize(size);
    DWORD read = 0;
    const BOOL ok = size == 0 || ReadFile(file, &data[0], size, &read, 0);
    CloseHandle(file);
    return ok && read == size;
}

dice::hfe::world::MaterialManager* materials()
{
    return static_cast<dice::hfe::world::MaterialManager*>(dice::hfe::world::materialManager);
}

IRoadEditableRenderer* roadQueue()
{
    using namespace dice::hfe;
    IBase* const singleton = classManager ? classManager->getSingleton("RoadEditableRenderer") : 0;
    return singleton ? static_cast<IRoadEditableRenderer*>(singleton->queryInterface(geom::IID_IRoadEditableRenderer)) : 0;
}

// The object a class makes, asked for one interface; null when the class or the interface is missing.
IBase* makeInstance(unsigned classId, unsigned iid)
{
    return dice::hfe::classManager ? dice::hfe::classManager->createInstance(classId, iid, 0) : 0;
}

} // namespace

bool RoadNetwork::NoCase::operator()(const std::string& a, const std::string& b) const
{
    return _stricmp(a.c_str(), b.c_str()) < 0;
}

RoadNetwork::RoadNetwork()
{
}

RoadNetwork::~RoadNetwork()
{
    close();
}

// ---- objects ---------------------------------------------------------------------------------------------------------

IEditorSpline* RoadNetwork::createSplineObject(std::string& error)
{
    IBase* const made = makeInstance(dice::hfe::geom::CID_EditorSpline, dice::hfe::geom::IID_IEditorSpline);
    if (!made)
    {
        error = "the renderer made no EditorSpline (class 81033): the editor's RendDX9 image registers it";
        return 0;
    }
    return static_cast<IEditorSpline*>(made);
}

IRoadTemplate* RoadNetwork::createTemplateObject(std::string& error)
{
    IBase* const made = makeInstance(dice::hfe::geom::CID_RoadTemplate, dice::hfe::geom::IID_IRoadTemplate);
    if (!made)
    {
        error = "the renderer made no RoadTemplate (class 821008): the editor's RendDX9 image registers it";
        return 0;
    }
    return static_cast<IRoadTemplate*>(made);
}

bool RoadNetwork::createRoad(const std::string& name, int splineId, const std::string& templateName, int seed,
                             std::string& error)
{
    if (name.empty())
    {
        error = "a road needs a name";
        return false;
    }
    if (findRoad(name))
    {
        error = "Road: [" + name + "] already exists!";
        return false;
    }
    std::map<int, Spline>::iterator spline = m_splines.find(splineId);
    if (spline == m_splines.end())
    {
        error = "Trying to create road: [" + name + "], but could not find spline: [" + number(splineId) + "]!";
        return false;
    }
    const std::string onSpline = roadOnSpline(splineId);
    if (!onSpline.empty())
    {
        error = "spline " + number(splineId) + " already has road " + onSpline;
        return false;
    }
    Template* const roadTemplate = findTemplate(templateName);
    if (!roadTemplate)
    {
        error = "No active RoadTemplate! [" + templateName + "]";
        return false;
    }
    if (roadTemplate->object->getTextureFile(0).empty())
    {
        error = "The RoadTemplate: [" + roadTemplate->name + "] lacks primary texture!";
        return false;
    }
    if (roadTemplate->object->getTextureFile(1).empty())
    {
        error = "The RoadTemplate: [" + roadTemplate->name + "] lacks secondary texture!";
        return false;
    }
    if (spline->second.object->getControlPointCount() <= 1)
    {
        error = "spline " + number(splineId) + " has fewer than two control points";
        return false;
    }
    IRoadEditableRenderer* const queue = roadQueue();
    if (!queue)
    {
        error = "the renderer has no editable-road queue (RoadEditableRenderer)";
        return false;
    }
    IBase* const made = makeInstance(dice::hfe::geom::CID_RoadEditable, dice::hfe::geom::IID_IRoadEditable);
    IBase* const noiseMade = makeInstance(dice::hfe::geom::CID_PerlinNoise, dice::hfe::geom::IID_IPerlinNoise);
    if (!made || !noiseMade)
    {
        if (made)
            made->release();
        if (noiseMade)
            noiseMade->release();
        error = "the renderer made no RoadEditable / PerlinNoise (classes 821011, 81035)";
        return false;
    }
    Road road;
    road.name = name;
    road.splineId = splineId;
    road.templateName = roadTemplate->name;
    road.object = static_cast<IRoadEditable*>(made);
    road.noise = static_cast<IPerlinNoise*>(noiseMade);
    if (seed != -1)
        road.noise->setSeed(seed);
    road.noise->setPersistence(kNoisePersistence);
    road.noise->setOutputRange(vec2(kNoiseRange));
    road.noise->setOctaves(kNoiseOctaves);
    // Every setter rebuilds; the template last builds the road once (retail set the template second and the noise last).
    road.object->setSpline(spline->second.object);
    road.object->setNoise(road.noise);
    road.object->setName(name);
    road.object->setTemplate(roadTemplate->object);
    queue->add(road.object);
    m_roads.push_back(road);
    return true;
}

void RoadNetwork::releaseRoad(Road& road)
{
    // The road's destructor takes it off the renderer's queue.
    if (road.object)
        road.object->release();
    if (road.noise)
        road.noise->release();
    road.object = 0;
    road.noise = 0;
}

RoadNetwork::Road* RoadNetwork::findRoad(const std::string& name)
{
    for (size_t i = 0; i < m_roads.size(); ++i)
        if (sameName(m_roads[i].name, name))
            return &m_roads[i];
    return 0;
}

const RoadNetwork::Road* RoadNetwork::findRoad(const std::string& name) const
{
    return const_cast<RoadNetwork*>(this)->findRoad(name);
}

RoadNetwork::Template* RoadNetwork::findTemplate(const std::string& name)
{
    for (size_t i = 0; i < m_templates.size(); ++i)
        if (sameName(m_templates[i].name, name))
            return &m_templates[i];
    return 0;
}

const RoadNetwork::Template* RoadNetwork::findTemplate(const std::string& name) const
{
    return const_cast<RoadNetwork*>(this)->findTemplate(name);
}

void RoadNetwork::rebuildRoadOn(int splineId)
{
    for (size_t i = 0; i < m_roads.size(); ++i)
        if (m_roads[i].splineId == splineId && m_roads[i].object)
        {
            if (m_loading)
                m_pendingRebuild.insert(m_roads[i].name);
            else
                m_roads[i].object->invalidate();
        }
}

// ---- open and close --------------------------------------------------------------------------------------------------

void RoadNetwork::runRows(const std::vector<ConsoleRow>& rows, const std::string& source, int& refused)
{
    for (size_t i = 0; i < rows.size(); ++i)
    {
        std::string result;
        if (retailRow(rows[i], result))
            continue;
        ++refused;
        ++m_refusedRows;
        if (m_refusals.size() < 8)
            m_refusals.push_back(source + ": " + rows[i].object + "." + rows[i].method + ": " + result);
    }
}

bool RoadNetwork::open(const std::string& levelName, const std::string& levelPath, std::string& report)
{
    using namespace dice::hfe;
    close();
    m_levelName = levelName;
    m_levelPath = levelPath;
    if (!classManager || !io::fileManager)
    {
        report = "roads: the engine is not up";
        return false;
    }
    if (!roadQueue())
    {
        report = "roads: the renderer has no editable-road queue (RoadEditableRenderer): the editor's RendDX9 image "
                 "registers it";
        return false;
    }
    m_open = true;
    m_loading = true;

    // The mod's templates: the editor branch of every Objects/Roads/Splines/<name>.con (the game reads only the
    // compiled pair beside it).
    std::list<io::FileInfo> found;
    io::fileManager->findFiles(kTemplateFolder, false, found);
    // The file manager lists no loose file in a folder an archive covers (Objects/ is Objects_server.zip's), so the
    // templates saved into a mod's loose objects folder are found on disk, in every path the engine reads from.
    const std::list<std::string>* const paths =
        static_cast<const std::list<std::string>*>(io::fileManager->getPathList());
    for (std::list<std::string>::const_iterator path = paths ? paths->begin() : std::list<std::string>::const_iterator();
         paths && path != paths->end(); ++path)
    {
        std::string folder = *path;
        if (!folder.empty() && folder[folder.size() - 1] != '/' && folder[folder.size() - 1] != '\\')
            folder += "/";
        WIN32_FIND_DATAA entry;
        const HANDLE search = FindFirstFileA((folder + kTemplateFolder + "*.con").c_str(), &entry);
        if (search == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            io::FileInfo info = io::FileInfo();
            info.name = entry.cFileName;
            info.isDirectory = false;
            found.push_back(info);
        } while (FindNextFileA(search, &entry));
        FindClose(search);
    }
    std::set<std::string, NoCase> seen;
    int templateFiles = 0;
    for (std::list<io::FileInfo>::const_iterator it = found.begin(); it != found.end(); ++it)
    {
        if (it->isDirectory)
            continue;
        std::string name = it->name;
        const size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos)
            name = name.substr(slash + 1);
        const std::string low = lowered(name);
        if (low.size() < 5 || low.compare(low.size() - 4, 4, ".con") != 0
            || (low.size() > 13 && low.compare(low.size() - 13, 13, "_compiled.con") == 0) || !seen.insert(name).second)
            continue;
        std::string text;
        const std::string path = kTemplateFolder + name;
        if (!readEngineFile(path, text))
            continue;
        int skipped = 0, refused = 0;
        const std::vector<ConsoleRow> rows = consoleRows(text, "BF2Editor", skipped);
        m_currentSource = path;
        runRows(rows, path, refused);
        // A template begun and never named is dropped (retail kept it unnamed in its list).
        if (m_buildTemplate && m_buildTemplateName.empty())
            m_buildTemplate->release();
        m_buildTemplate = 0;
        m_buildTemplateName.clear();
        ++templateFiles;
    }
    m_currentSource.clear();

    // The level's splines and roads.
    int splineRows = 0, roadRows = 0;
    const char* const files[] = { "Editor/Splines.con", "Editor/Roads.con" };
    for (int f = 0; f < 2; ++f)
    {
        std::string text;
        if (!readEngineFile(levelPath + files[f], text))
            continue;
        m_hadFiles = true;
        int skipped = 0, refused = 0;
        const std::vector<ConsoleRow> rows = consoleRows(text, std::string(), skipped);
        (f == 0 ? splineRows : roadRows) = static_cast<int>(rows.size());
        m_currentSource = levelPath + files[f];
        runRows(rows, files[f], refused);
        if (f == 0 && m_buildSpline)
        {
            m_buildSpline->release();       // begun, never ended
            m_buildSpline = 0;
            m_buildSplineId = -1;
        }
    }
    m_currentSource.clear();
    m_activeRoad.clear();
    m_loading = false;
    for (std::set<std::string, NoCase>::const_iterator it = m_pendingRebuild.begin(); it != m_pendingRebuild.end(); ++it)
        if (Road* road = findRoad(*it))
            road->object->invalidate();
    m_pendingRebuild.clear();
    for (size_t i = 0; i < m_roads.size(); ++i)
        m_loadedRoads.insert(m_roads[i].name);
    m_changed = false;

    char text[256];
    std::snprintf(text, sizeof(text), "roads: %u templates from %d files, %u splines, %u roads",
                  static_cast<unsigned>(m_templates.size()), templateFiles, static_cast<unsigned>(m_splines.size()),
                  static_cast<unsigned>(m_roads.size()));
    report = text;
    if (m_refusedRows)
        report += "; " + number(m_refusedRows) + " rows refused, the first: " + m_refusals[0];
    logLine(m_refusedRows ? LogLevel::Warning : LogLevel::Info, "%s", report.c_str());
    for (size_t i = 1; i < m_refusals.size(); ++i)
        logLine(LogLevel::Warning, "roads: refused %s", m_refusals[i].c_str());
    return true;
}

void RoadNetwork::close()
{
    for (size_t i = 0; i < m_roads.size(); ++i)
        releaseRoad(m_roads[i]);
    m_roads.clear();
    for (std::map<int, Spline>::iterator it = m_splines.begin(); it != m_splines.end(); ++it)
        if (it->second.object)
            it->second.object->release();
    m_splines.clear();
    for (size_t i = 0; i < m_templates.size(); ++i)
        if (m_templates[i].object)
            m_templates[i].object->release();
    m_templates.clear();
    if (m_buildSpline)
        m_buildSpline->release();
    if (m_buildTemplate && m_buildTemplateName.empty())
        m_buildTemplate->release();
    m_buildSpline = 0;
    m_buildSplineId = -1;
    m_buildTemplate = 0;
    m_buildTemplateName.clear();
    m_activeRoad.clear();
    m_loadedRoads.clear();
    m_pendingRebuild.clear();
    m_refusals.clear();
    m_refusedRows = 0;
    m_open = m_changed = m_hadFiles = m_loading = false;
}

// ---- the retail rows -------------------------------------------------------------------------------------------------

bool RoadNetwork::retailRow(const ConsoleRow& row, std::string& result)
{
    result.clear();
    if (!m_open)
    {
        result = "no level's roads are open";
        return false;
    }
    const std::string object = lowered(row.object);
    if (object == "splinemanager")
        return splineRow(row, result);
    if (object == "road")
        return roadRow(row, result);
    if (object == "roadtemplate")
        return templateRow(row, result);
    if (object == "roadtemplatetexture")
        return textureRow(row, result);
    result = row.object + " is not a road object";
    return false;
}

bool RoadNetwork::splineRow(const ConsoleRow& row, std::string& result)
{
    const std::string method = lowered(row.method);
    const std::string arg = row.args.empty() ? std::string() : row.args[0];
    if (method == "createinstance")
    {
        if (m_buildSpline)
            m_buildSpline->release();       // begun and never ended (retail leaked it)
        m_buildSpline = createSplineObject(result);
        m_buildSplineId = -1;
        return m_buildSpline != 0;
    }
    if (method == "end")
    {
        if (!m_buildSpline)
        {
            result = "SplineManager.End with no spline begun (SplineManager.CreateInstance)";
            return false;
        }
        // [CHOSEN] a spline ended without SetId takes the next free id; retail registered it under -1 again and
        // again (its map kept the first).
        const int id = m_buildSplineId >= 0 ? m_buildSplineId : nextSplineId();
        if (m_splines.count(id))
        {
            m_buildSpline->release();
            m_buildSpline = 0;
            m_buildSplineId = -1;
            result = "a spline with id " + number(id) + " exists";
            return false;
        }
        m_splines[id].object = m_buildSpline;
        m_buildSpline = 0;
        m_buildSplineId = -1;
        if (!m_loading)
            m_changed = true;
        return true;
    }
    if (!m_buildSpline)
    {
        result = "SplineManager." + row.method + " with no spline begun (SplineManager.CreateInstance)";
        return false;
    }
    IEditorSpline* const spline = m_buildSpline;
    int i = 0;
    float f = 0.0f;
    float v[3];
    if (method == "setid" && parseInt(arg, i))
        m_buildSplineId = i;
    else if (method == "setcontrolpointsnapmodes" && parseInt(arg, i))
        spline->setSnapFlags(i);
    else if (method == "setlinewidth" && parseFloat(arg, f))
        spline->setLineWidth(f);
    else if (method == "setstepsize" && parseFloat(arg, f))
        spline->setStepSize(f);
    else if (method == "addcontrolpoint" && parseVec3(arg, v))
    {
        // As the retail spline manager loaded points: unsnapped, where the file put them.
        const int snap = spline->getSnapFlags();
        spline->setSnapFlags(0);
        spline->addControlPoint(vec3(v));
        spline->setSnapFlags(snap);
    }
    else if (method == "setspeed" || method == "setdelay" || method == "setdirection" || method == "setsound")
    {
        // The point the row follows; the renderer's "last point" setters write before the block on an empty spline.
        if (spline->getControlPointCount() < 1)
        {
            result = "SplineManager." + row.method + " before any SplineManager.AddControlPoint";
            return false;
        }
        if (method == "setspeed" && parseFloat(arg, f))
            spline->setLastControlPointSpeed(f);
        else if (method == "setdelay" && parseFloat(arg, f))
            spline->setLastControlPointDelay(f);
        else if (method == "setdirection" && parseInt(arg, i))
            spline->setLastControlPointDirection(i);
        else if (method == "setsound")
            spline->setLastControlPointSound(arg);
        else
        {
            result = "SplineManager." + row.method + ": a bad value '" + arg + "'";
            return false;
        }
    }
    else if (method == "setid" || method == "setcontrolpointsnapmodes" || method == "setlinewidth"
             || method == "setstepsize" || method == "addcontrolpoint")
    {
        result = "SplineManager." + row.method + ": a bad value '" + arg + "'";
        return false;
    }
    else
    {
        result = "SplineManager." + row.method + " is a retail editor row this editor does not take";
        return false;
    }
    return true;
}

bool RoadNetwork::roadRow(const ConsoleRow& row, std::string& result)
{
    const std::string method = lowered(row.method);
    if (method == "createinstance")
    {
        int splineId = 0, seed = -1;
        if (row.args.size() < 3 || !parseInt(row.args[1], splineId) || (row.args.size() > 3 && !parseInt(row.args[3], seed)))
        {
            result = "Road.CreateInstance \"<name>\" <spline id> \"<template>\" [<noise seed>]";
            return false;
        }
        if (!createRoad(row.args[0], splineId, row.args[2], seed, result))
            return false;
        m_activeRoad = row.args[0];
        if (!m_loading)
            m_changed = true;
        return true;
    }
    Road* const road = findRoad(m_activeRoad);
    if (!road)
    {
        result = "Road." + row.method + " with no road made (Road.CreateInstance)";
        return false;
    }
    const std::string key = propertyKey(row.method);
    const std::string arg = row.args.empty() ? std::string() : row.args[0];
    float v[2];
    float f = 0.0f;
    int i = 0;
    bool rebuild = false;
    if (key == "texturescaleoffset" && parseVec2(arg, v))
    {
        road->object->setTextureScaleOffset(0, vec2(v));       // no rebuild of its own
        rebuild = true;
    }
    else if (key == "persistance" && parseFloat(arg, f))
    {
        road->noise->setPersistence(f);
        rebuild = true;
    }
    else if (key == "range" && parseVec2(arg, v))
    {
        road->noise->setOutputRange(vec2(v));
        rebuild = true;
    }
    else if (key == "octaves" && parseInt(arg, i))
    {
        road->noise->setOctaves(i);
        rebuild = true;
    }
    else if (key == "prio" && parseInt(arg, i))
        road->object->setPriority(i);
    else if (key == "fadedistatstartleft" && parseVec2(arg, v))
        road->object->setFadeDistAtStartLeft(vec2(v));
    else if (key == "fadedistatstartright" && parseVec2(arg, v))
        road->object->setFadeDistAtStartRight(vec2(v));
    else if (key == "fadedistatendleft" && parseVec2(arg, v))
        road->object->setFadeDistAtEndLeft(vec2(v));
    else if (key == "fadedistatendright" && parseVec2(arg, v))
        road->object->setFadeDistAtEndRight(vec2(v));
    else if (key == "texturescaleoffset" || key == "persistance" || key == "range" || key == "octaves" || key == "prio"
             || key.compare(0, 8, "fadedist") == 0)
    {
        result = "Road." + row.method + ": a bad value '" + arg + "'";
        return false;
    }
    else
    {
        result = "Road." + row.method + " is a retail editor row this editor does not take";
        return false;
    }
    if (rebuild)
    {
        if (m_loading)
            m_pendingRebuild.insert(road->name);
        else
            road->object->invalidate();
    }
    if (!m_loading)
        m_changed = true;
    return true;
}

bool RoadNetwork::templateRow(const ConsoleRow& row, std::string& result)
{
    const std::string method = lowered(row.method);
    if (method == "createinstance")
    {
        if (m_buildTemplate && m_buildTemplateName.empty())
            m_buildTemplate->release();
        m_buildTemplate = createTemplateObject(result);
        m_buildTemplateName.clear();
        m_textureLayer = 0;
        return m_buildTemplate != 0;
    }
    if (!m_buildTemplate)
    {
        result = "RoadTemplate." + row.method + " with no template begun (RoadTemplate.CreateInstance)";
        return false;
    }
    IRoadTemplate* const object = m_buildTemplate;
    const std::string key = propertyKey(row.method);
    const std::string arg = row.args.empty() ? std::string() : row.args[0];
    float v[2];
    float f = 0.0f;
    int i = 0;
    bool b = false;
    if (key == "name")
    {
        if (!m_buildTemplateName.empty())
        {
            result = "the template begun is already named " + m_buildTemplateName;
            return false;
        }
        if (arg.empty() || findTemplate(arg))
        {
            object->release();
            m_buildTemplate = 0;
            result = arg.empty() ? "a template needs a name" : "a template named " + arg + " exists";
            return false;
        }
        object->setName(arg);
        Template entry;
        entry.name = arg;
        entry.object = object;
        entry.file = m_currentSource;
        entry.changed = !m_loading;
        m_templates.push_back(entry);
        m_buildTemplateName = arg;
        if (!m_loading)
            m_changed = true;
        return true;
    }
    if (key == "width" && parseFloat(arg, f))
        object->setWidth(f);
    else if (key == "maxtesselatederror" && parseFloat(arg, f))
        object->setMaxTesselatedError(0, f);
    else if (key == "blendfactor" && parseFloat(arg, f))
        object->setBlendFactor(f);
    else if (key == "fadeoffdistance" && parseVec2(arg, v))
        object->setFadeOffDistance(vec2(v));
    else if (key == "prio" && parseInt(arg, i))
        object->setPriority(i);
    else if (key == "material")
    {
        const unsigned id = materials() ? materials()->getMaterialId(arg) : 0;
        if (!id && materials() && !sameName(arg, "Default"))
            logLine(LogLevel::Warning, "roads: template %s: material %s is not one of the level's; material 0 taken",
                    m_buildTemplateName.c_str(), arg.c_str());
        object->setMaterial(static_cast<int>(id));
    }
    else if (key == "isprimarytexture" && parseBool(arg, b))
        m_textureLayer = b ? 0 : 1;
    else if (key == "width" || key == "maxtesselatederror" || key == "blendfactor" || key == "fadeoffdistance"
             || key == "prio" || key == "isprimarytexture")
    {
        result = "RoadTemplate." + row.method + ": a bad value '" + arg + "'";
        return false;
    }
    else
    {
        result = "RoadTemplate." + row.method + " is a retail editor row this editor does not take";
        return false;
    }
    if (!m_loading && !m_buildTemplateName.empty())
        if (Template* entry = findTemplate(m_buildTemplateName))
        {
            entry->changed = m_changed = true;
            const std::vector<std::string> users = roadsWithTemplate(entry->name);
            for (size_t r = 0; r < users.size(); ++r)
                if (Road* road = findRoad(users[r]))
                    road->object->invalidate();
        }
    return true;
}

bool RoadNetwork::textureRow(const ConsoleRow& row, std::string& result)
{
    if (!m_buildTemplate)
    {
        result = "RoadTemplateTexture." + row.method + " with no template begun (RoadTemplate.CreateInstance)";
        return false;
    }
    IRoadTemplate* const object = m_buildTemplate;
    const std::string key = propertyKey(row.method);
    const std::string arg = row.args.empty() ? std::string() : row.args[0];
    float v[2];
    bool b = false;
    if (key == "texturefile")
        object->setTextureFile(m_textureLayer, arg);
    else if (key == "mirror" && parseBool(arg, b))
        object->setMirror(static_cast<unsigned>(m_textureLayer), b);
    else if (key == "scale" && parseVec2(arg, v))
        object->setTextureScale(m_textureLayer, vec2(v));
    else if (key == "mirror" || key == "scale")
    {
        result = "RoadTemplateTexture." + row.method + ": a bad value '" + arg + "'";
        return false;
    }
    else
    {
        result = "RoadTemplateTexture." + row.method + " is a retail editor row this editor does not take";
        return false;
    }
    if (!m_loading && !m_buildTemplateName.empty())
        if (Template* entry = findTemplate(m_buildTemplateName))
        {
            entry->changed = m_changed = true;
            const std::vector<std::string> users = roadsWithTemplate(entry->name);
            for (size_t r = 0; r < users.size(); ++r)
                if (Road* road = findRoad(users[r]))
                    road->object->setTemplate(entry->object);     // its textures again, and a rebuild
        }
    return true;
}

// ---- queries ---------------------------------------------------------------------------------------------------------

std::vector<int> RoadNetwork::splineIds() const
{
    std::vector<int> ids;
    for (std::map<int, Spline>::const_iterator it = m_splines.begin(); it != m_splines.end(); ++it)
        ids.push_back(it->first);
    return ids;
}

std::vector<std::string> RoadNetwork::roadNames() const
{
    std::vector<std::string> names;
    for (size_t i = 0; i < m_roads.size(); ++i)
        names.push_back(m_roads[i].name);
    std::sort(names.begin(), names.end(), NoCase());
    return names;
}

std::vector<std::string> RoadNetwork::templateNames() const
{
    std::vector<std::string> names;
    for (size_t i = 0; i < m_templates.size(); ++i)
        names.push_back(m_templates[i].name);
    std::sort(names.begin(), names.end(), NoCase());
    return names;
}

bool RoadNetwork::hasRoad(const std::string& name) const
{
    return findRoad(name) != 0;
}

bool RoadNetwork::hasTemplate(const std::string& name) const
{
    return findTemplate(name) != 0;
}

std::string RoadNetwork::roadOnSpline(int id) const
{
    for (size_t i = 0; i < m_roads.size(); ++i)
        if (m_roads[i].splineId == id)
            return m_roads[i].name;
    return std::string();
}

std::vector<std::string> RoadNetwork::roadsWithTemplate(const std::string& name) const
{
    std::vector<std::string> names;
    for (size_t i = 0; i < m_roads.size(); ++i)
        if (sameName(m_roads[i].templateName, name))
            names.push_back(m_roads[i].name);
    return names;
}

int RoadNetwork::nextSplineId() const
{
    return m_splines.empty() ? 0 : m_splines.rbegin()->first + 1;
}

bool RoadNetwork::splineRecord(int id, SplineRecord& record) const
{
    std::map<int, Spline>::const_iterator it = m_splines.find(id);
    if (it == m_splines.end() || !it->second.object)
        return false;
    readSplineRecord(it->second.object, id, record);
    return true;
}

bool RoadNetwork::offsetSplineRecord(int id, float offset, SplineRecord& record, std::string& error) const
{
    std::map<int, Spline>::const_iterator it = m_splines.find(id);
    if (it == m_splines.end() || !it->second.object)
    {
        error = "no spline " + std::to_string(id);
        return false;
    }
    IEditorSpline* const copy = it->second.object->makeOffsetCopy(offset);
    if (!copy)
    {
        error = "the renderer made no copy of spline " + std::to_string(id);
        return false;
    }
    readSplineRecord(copy, -1, record);
    copy->release();
    return true;
}

int RoadNetwork::refreshRoads()
{
    int refreshed = 0;
    for (size_t i = 0; i < m_roads.size(); ++i)
        if (m_roads[i].object)
        {
            m_roads[i].object->invalidate();
            ++refreshed;
        }
    return refreshed;
}

void RoadNetwork::readSplineRecord(IEditorSpline* spline, int id, SplineRecord& record)
{
    record = SplineRecord();
    record.id = id;
    record.snapModes = spline->getSnapFlags();
    record.lineWidth = spline->getLineWidth();
    record.stepSize = spline->getStepSize();
    const int count = spline->getControlPointCount();
    for (int p = 0; p < count; ++p)
    {
        SplinePointRecord point;
        // The record's leading Vec3 is its position (GeomMisc/SplineControlPoint.h: m_position at +0x00).
        const Vec3* const position = reinterpret_cast<const Vec3*>(spline->getControlPoint(p));
        point.position[0] = position->x;
        point.position[1] = position->y;
        point.position[2] = position->z;
        point.speed = spline->getControlPointSpeed(p);
        point.delay = spline->getControlPointDelay(p);
        point.direction = spline->getControlPointDirection(p);
        point.sound = spline->getControlPointSound(p);
        record.points.push_back(point);
    }
}

bool RoadNetwork::roadRecord(const std::string& name, RoadRecord& record) const
{
    const Road* const road = findRoad(name);
    if (!road || !road->object || !road->noise)
        return false;
    record = RoadRecord();
    record.name = road->name;
    record.splineId = road->splineId;
    record.templateName = road->templateName;
    record.seed = road->noise->getSeed();
    put2(record.textureScaleOffset, road->object->getTextureScaleOffset(0));
    record.persistence = road->noise->getPersistence();
    put2(record.range, road->noise->getOutputRange());
    record.octaves = road->noise->getOctaves();
    record.priority = road->object->getPriority();
    put2(record.fadeStartLeft, road->object->getFadeDistAtStartLeft());
    put2(record.fadeStartRight, road->object->getFadeDistAtStartRight());
    put2(record.fadeEndLeft, road->object->getFadeDistAtEndLeft());
    put2(record.fadeEndRight, road->object->getFadeDistAtEndRight());
    return true;
}

bool RoadNetwork::templateRecord(const std::string& name, RoadTemplateRecord& record) const
{
    const Template* const entry = findTemplate(name);
    if (!entry || !entry->object)
        return false;
    IRoadTemplate* const object = entry->object;
    record = RoadTemplateRecord();
    record.name = entry->name;
    record.width = object->getWidth();
    record.maxTesselatedError = object->getMaxTesselatedError(0);
    record.blendFactor = object->getBlendFactor();
    record.priority = object->getPriority();
    // As the retail writer named it: the material manager's material of the template's id, none when it has none.
    dice::hfe::world::Material* const material =
        materials() ? materials()->getMaterialById(static_cast<unsigned>(object->getMaterial())) : 0;
    record.material = material ? material->getName() : std::string();
    for (int layer = 0; layer < 2; ++layer)
    {
        record.textures[layer] = object->getTextureFile(layer);
        record.mirror[layer] = object->getMirror(static_cast<unsigned>(layer));
        put2(record.scale[layer], object->getTextureScale(layer));
    }
    put2(record.fadeOffDistance, object->getFadeOffDistance());
    return true;
}

// ---- states ----------------------------------------------------------------------------------------------------------

void RoadNetwork::writeSplineState(IEditorSpline* spline, const SplineRecord& record)
{
    spline->setLineWidth(record.lineWidth);
    spline->setStepSize(record.stepSize);
    spline->clearControlPoints();
    spline->setSnapFlags(0);                // the points go where the record has them
    for (size_t p = 0; p < record.points.size(); ++p)
    {
        const SplinePointRecord& point = record.points[p];
        spline->addControlPoint(vec3(point.position));
        spline->setLastControlPointSpeed(point.speed);
        spline->setLastControlPointDelay(point.delay);
        spline->setLastControlPointDirection(point.direction);
        spline->setLastControlPointSound(point.sound);
    }
    spline->setSnapFlags(record.snapModes);
}

void RoadNetwork::writeTemplateState(IRoadTemplate* object, const RoadTemplateRecord& record)
{
    object->setWidth(record.width);
    object->setMaxTesselatedError(0, record.maxTesselatedError);
    object->setBlendFactor(record.blendFactor);
    object->setPriority(record.priority);
    object->setMaterial(record.material.empty() || !materials()
                            ? 0 : static_cast<int>(materials()->getMaterialId(record.material)));
    for (int layer = 0; layer < 2; ++layer)
    {
        object->setTextureFile(layer, record.textures[layer]);
        object->setMirror(static_cast<unsigned>(layer), record.mirror[layer]);
        object->setTextureScale(layer, vec2(record.scale[layer]));
    }
    object->setFadeOffDistance(vec2(record.fadeOffDistance));
}

void RoadNetwork::writeRoadState(Road& road, const RoadRecord& record)
{
    road.noise->setSeed(record.seed);
    road.noise->setPersistence(record.persistence);
    road.noise->setOutputRange(vec2(record.range));
    road.noise->setOctaves(record.octaves);
    road.object->setTextureScaleOffset(0, vec2(record.textureScaleOffset));
    road.object->setPriority(record.priority);
    road.object->setFadeDistAtStartLeft(vec2(record.fadeStartLeft));
    road.object->setFadeDistAtStartRight(vec2(record.fadeStartRight));
    road.object->setFadeDistAtEndLeft(vec2(record.fadeEndLeft));
    road.object->setFadeDistAtEndRight(vec2(record.fadeEndRight));    // the last setter rebuilds with all of them
}

bool RoadNetwork::applySpline(int id, const SplineRecord* record, std::string& error)
{
    if (!m_open)
    {
        error = "no level's roads are open";
        return false;
    }
    std::map<int, Spline>::iterator it = m_splines.find(id);
    if (!record)
    {
        if (it == m_splines.end())
            return true;
        const std::string road = roadOnSpline(id);
        if (!road.empty())
        {
            error = "spline " + number(id) + " carries road " + road;
            return false;
        }
        it->second.object->release();
        m_splines.erase(it);
        m_changed = true;
        return true;
    }
    if (it == m_splines.end())
    {
        IEditorSpline* const spline = createSplineObject(error);
        if (!spline)
            return false;
        it = m_splines.insert(std::make_pair(id, Spline())).first;
        it->second.object = spline;
    }
    writeSplineState(it->second.object, *record);
    rebuildRoadOn(id);
    m_changed = true;
    return true;
}

bool RoadNetwork::applyTemplate(const std::string& name, const RoadTemplateRecord* record, std::string& error)
{
    if (!m_open)
    {
        error = "no level's roads are open";
        return false;
    }
    Template* entry = findTemplate(name);
    if (!record)
    {
        if (!entry)
            return true;
        const std::vector<std::string> users = roadsWithTemplate(name);
        if (!users.empty())
        {
            error = "template " + name + " carries road " + users[0];
            return false;
        }
        entry->object->release();
        m_templates.erase(m_templates.begin() + (entry - &m_templates[0]));
        m_changed = true;
        return true;
    }
    if (!entry)
    {
        IRoadTemplate* const object = createTemplateObject(error);
        if (!object)
            return false;
        object->setName(name);
        Template made;
        made.name = name;
        made.object = object;
        m_templates.push_back(made);
        entry = &m_templates.back();
    }
    writeTemplateState(entry->object, *record);
    entry->changed = true;
    m_changed = true;
    const std::vector<std::string> users = roadsWithTemplate(name);
    for (size_t r = 0; r < users.size(); ++r)
        if (Road* road = findRoad(users[r]))
            road->object->setTemplate(entry->object);
    return true;
}

bool RoadNetwork::applyRoad(const std::string& name, const RoadRecord* record, std::string& error)
{
    if (!m_open)
    {
        error = "no level's roads are open";
        return false;
    }
    Road* road = findRoad(name);
    if (!record)
    {
        if (!road)
            return true;
        releaseRoad(*road);
        m_roads.erase(m_roads.begin() + (road - &m_roads[0]));
        m_changed = true;
        return true;
    }
    // A road on another spline or template, or renamed, is made again.
    if (road && (road->splineId != record->splineId || !sameName(road->templateName, record->templateName)
                 || road->name != record->name))
    {
        releaseRoad(*road);
        m_roads.erase(m_roads.begin() + (road - &m_roads[0]));
        road = 0;
    }
    if (!road)
    {
        if (!createRoad(record->name, record->splineId, record->templateName, record->seed, error))
            return false;
        road = findRoad(record->name);
    }
    writeRoadState(*road, *record);
    m_changed = true;
    return true;
}

bool RoadNetwork::setGhost(const std::string& name, bool ghost, std::string& error)
{
    Road* const road = findRoad(name);
    if (!road)
    {
        error = "no road " + name;
        return false;
    }
    road->object->setGhost(ghost);
    return true;
}

bool RoadNetwork::projectToTerrain(const std::string& name, std::string& error)
{
    Road* const road = findRoad(name);
    if (!road)
    {
        error = "no road " + name;
        return false;
    }
    const std::map<int, Spline>::const_iterator spline = m_splines.find(road->splineId);
    if (spline == m_splines.end() || spline->second.object->getControlPointCount() < 4)
    {
        error = "road " + name + " needs a spline of four points to be applied to the terrain";
        return false;
    }
    road->object->projectToTerrain();
    road->object->invalidate();
    return true;
}

bool RoadNetwork::ghost(const std::string& name) const
{
    const Road* const road = findRoad(name);
    return road && road->object->getGhost();
}

std::string RoadNetwork::describe() const
{
    if (!m_open)
        return "roads: not open";
    char text[160];
    std::snprintf(text, sizeof(text), "%u templates, %u splines, %u roads%s", static_cast<unsigned>(m_templates.size()),
                  static_cast<unsigned>(m_splines.size()), static_cast<unsigned>(m_roads.size()),
                  m_changed ? ", changed" : "");
    std::string line = text;
    if (m_refusedRows)
        line += "; the open refused " + number(m_refusedRows) + " rows";
    return line;
}

// ---- saving ----------------------------------------------------------------------------------------------------------

bool RoadNetwork::stageSave(const std::string& levelDirectory, bool force,
                            std::vector<std::pair<std::string, std::string> >& levelFiles,
                            std::vector<std::pair<std::string, std::string> >& templateFiles, std::string& result)
{
    using namespace dice::hfe;
    levelFiles.clear();
    templateFiles.clear();
    result.clear();
    if (!m_open)
    {
        result = "no level's roads are open";
        return false;
    }
    if (!m_changed && !(force && hasEditableRoads()))
        return true;
    if (!settings || !io::fileManager)
    {
        result = "the engine is not up";
        return false;
    }

    // A staging folder of the editor's own, emptied: <game folder>/bf2editor_temp/roads_<process id>.
    std::string root;
    if (!makeStagingFolder("roads", root, result))
        return false;
    const std::string level = root + "/" + m_levelPath;           // the level's files, staged under their engine path
    makeFolders(level + "Roads/");
    makeFolders(level + "Editor/");

    // The changed templates: their editor script, then the renderer's compile beside it (_compiled.con, .dat).
    const std::string templateFolder = root + "/" + kTemplateFolder;
    makeFolders(templateFolder);
    int templatesWritten = 0;
    for (size_t t = 0; t < m_templates.size(); ++t)
    {
        if (!m_templates[t].changed)
            continue;
        RoadTemplateRecord record;
        templateRecord(m_templates[t].name, record);
        const std::string name = m_templates[t].name;
        const std::string script = templateFolder + name + ".con";
        if (!writeWhole(script, roadTemplateConText(record)))
        {
            result = "could not write " + script;
            return false;
        }
        std::string compileFolder = templateFolder.substr(0, templateFolder.size() - 1);
        std::replace(compileFolder.begin(), compileFolder.end(), '/', '\\');
        m_templates[t].object->compile(compileFolder);
        const char* const suffixes[] = { ".con", "_compiled.con", "_compiled.dat" };
        for (int s = 0; s < 3; ++s)
        {
            const std::string staged = templateFolder + name + suffixes[s];
            if (GetFileAttributesA(staged.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                result = "the template compile wrote no " + staged;
                return false;
            }
            templateFiles.push_back(std::make_pair(staged, std::string(kTemplateFolder) + name + suffixes[s]));
        }
        ++templatesWritten;
    }

    // The editor's own files: every spline by id, every road by its spline's id (the retail writers' orders).
    std::vector<SplineRecord> splines;
    for (std::map<int, Spline>::const_iterator it = m_splines.begin(); it != m_splines.end(); ++it)
    {
        SplineRecord record;
        if (splineRecord(it->first, record))
            splines.push_back(record);
    }
    std::vector<RoadRecord> roads;
    for (std::map<int, Spline>::const_iterator it = m_splines.begin(); it != m_splines.end(); ++it)
    {
        const std::string name = roadOnSpline(it->first);
        RoadRecord record;
        if (!name.empty() && roadRecord(name, record))
            roads.push_back(record);
    }
    // [CHOSEN] a level that never had the files and has no spline left gets none (retail wrote them empty).
    if (m_hadFiles || !splines.empty())
    {
        if (!writeWhole(level + "Editor/Splines.con", splinesConText(splines))
            || !writeWhole(level + "Editor/Roads.con", roadsConText(roads)))
        {
            result = "could not write the staged Editor/Splines.con, Editor/Roads.con";
            return false;
        }
        levelFiles.push_back(std::make_pair(level + "Editor/Splines.con", std::string("Editor/Splines.con")));
        levelFiles.push_back(std::make_pair(level + "Editor/Roads.con", std::string("Editor/Roads.con")));
    }

    // The roads compiled by the renderer into the staging folder: each appends its entry to the staged
    // CompiledRoads.con (opened for append: it must exist) and writes Roads/<road>_compiled.mesh.
    std::string levelArgument = m_levelPath;
    while (!levelArgument.empty() && (levelArgument[levelArgument.size() - 1] == '/' || levelArgument[levelArgument.size() - 1] == '\\'))
        levelArgument.erase(levelArgument.size() - 1);
    std::replace(levelArgument.begin(), levelArgument.end(), '/', '\\');
    const std::string stagedCompiled = level + "CompiledRoads.con";
    if (!writeWhole(stagedCompiled, std::string()))
    {
        result = "could not write " + stagedCompiled;
        return false;
    }
    std::vector<std::string> compiled, skipped;
    {
        StagingRedirect redirect(root);
        for (size_t r = 0; r < roads.size(); ++r)
        {
            Road* const road = findRoad(roads[r].name);
            if (road && road->object->compile(levelArgument))
                compiled.push_back(road->name);
            else
                skipped.push_back(roads[r].name);
        }
    }
    std::string ours;
    if (!readWhole(stagedCompiled, ours))
    {
        result = "could not read " + stagedCompiled;
        return false;
    }
    for (size_t c = 0; c < compiled.size(); ++c)
    {
        const std::string mesh = level + "Roads/" + compiled[c] + "_compiled.mesh";
        std::string data, error;
        RoadMeshInfo info;
        if (!readWhole(mesh, data) || !appendRoadMeshChunkTable(data, info, error) || !writeWhole(mesh, data))
        {
            result = "road " + compiled[c] + ": its compiled mesh " + (error.empty() ? "could not be read" : error);
            return false;
        }
        levelFiles.push_back(std::make_pair(mesh, "Roads/" + compiled[c] + "_compiled.mesh"));
    }

    // CompiledRoads.con: the level's own entries, less those of the roads this editor makes (loaded or new), then ours.
    std::string existing;
    if (!readWhole(levelDirectory + "CompiledRoads.con", existing))
        readEngineFile(m_levelPath + "CompiledRoads.con", existing);
    CompiledRoadsFile merged = parseCompiledRoads(existing);
    if (existing.empty())
        merged.newline = "\n";
    std::set<std::string, NoCase> editable = m_loadedRoads;
    for (size_t r = 0; r < m_roads.size(); ++r)
        editable.insert(m_roads[r].name);
    size_t kept = 0;
    for (size_t b = 0; b < merged.blocks.size();)
    {
        bool editableBlock = false;
        for (std::set<std::string, NoCase>::const_iterator it = editable.begin(); it != editable.end() && !editableBlock;
             ++it)
            editableBlock = compiledMeshIsRoad(merged.blocks[b].mesh, *it);
        if (editableBlock)
            merged.blocks.erase(merged.blocks.begin() + b);
        else
        {
            ++b;
            ++kept;
        }
    }
    const CompiledRoadsFile fresh = parseCompiledRoads(ours);
    merged.blocks.insert(merged.blocks.end(), fresh.blocks.begin(), fresh.blocks.end());
    if (!writeWhole(stagedCompiled, compiledRoadsText(merged)))
    {
        result = "could not write " + stagedCompiled;
        return false;
    }
    levelFiles.push_back(std::make_pair(stagedCompiled, std::string("CompiledRoads.con")));

    char text[256];
    std::snprintf(text, sizeof(text), "roads: %u splines, %u roads compiled (%u other compiled roads kept), %d templates",
                  static_cast<unsigned>(splines.size()), static_cast<unsigned>(compiled.size()),
                  static_cast<unsigned>(kept), templatesWritten);
    result = text;
    if (!skipped.empty())
    {
        result += "; not compiled (no geometry: a road needs four control points):";
        for (size_t s = 0; s < skipped.size(); ++s)
            result += " " + skipped[s];
    }
    return true;
}

void RoadNetwork::markSaved()
{
    m_changed = false;
    for (size_t t = 0; t < m_templates.size(); ++t)
        m_templates[t].changed = false;
    m_loadedRoads.clear();
    for (size_t r = 0; r < m_roads.size(); ++r)
        m_loadedRoads.insert(m_roads[r].name);
    m_hadFiles = true;
}

} // namespace bf2editor
