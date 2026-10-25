// Code/Tools/BF2Editor/App/EditSession.cpp - see EditSession.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/EditorLevel.h"
#include "App/FreeCamera.h"
#include "App/LevelDocument.h"
#include "App/LevelSettings.h"
#include "Framework/EditorLog.h"

#include "Framework/Matrix4.h"

#include "Core/Ids.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/IObjectSpawner.h"
#include "Game/Objects/IObjectTemplate.h"
#include "Game/GameLogic/CombatArea.h"
#include "Game/GameLogic/CombatAreaManager.h"
#include "Math/LocalBoundingBox.h"
#include "Scene/Object/ObjectManager.h"
#include "Scene/Object/ObjectPredicator.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "IO/Console/Console.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>

using dice::hfe::Mat4;
using dice::hfe::Vec3;
using dice::hfe::world::IObject;

namespace bf2editor
{

namespace
{

// Picks visible objects under roots allowed by the session's layer and class filters.
// Mask zero lets intersectLine expand and test children.
class PickPredicator : public dice::hfe::world::ObjectPredicator
{
public:
    explicit PickPredicator(const EditSession& session, bool ignoreSelection = false)
        : m_session(session)
        , m_ignoreSelection(ignoreSelection)
    {
    }
    virtual bool predicate(IObject* object)
    {
        if ((object->getFlags() & IObject::OFIsVisible) == 0)
            return false;
        IObject* root = dice::hfe::world::getRootParent(object);
        if (!root)
            root = object;
        if (m_ignoreSelection && m_session.selection().contains(root->getObjectId()))
            return false;
        return m_session.canPick(root);
    }
    virtual unsigned int getFlags() const { return 0; }
    virtual bool performCulling(IObject*) { return true; }

private:
    PickPredicator& operator=(const PickPredicator&);
    const EditSession& m_session;
    bool               m_ignoreSelection;
};

// Where the ray first enters the object's bounds, in world units along the (unit) ray: its local box
// (Object::getFullBoundingBox) through the inverse of its matrix, or else its visual bounding sphere.
// False when it misses, and when the ray starts inside - the camera cannot click what surrounds it.
bool rayEntersBounds(IObject* object, const float origin[3], const float direction[3], float& distance)
{
    const Mat4& m = object->getAbsoluteTransformation();
    dice::hfe::LocalBoundingBox* box = object->getFullBoundingBox();
    if (box && (box->m_max.x > box->m_min.x || box->m_max.y > box->m_min.y || box->m_max.z > box->m_min.z))
    {
        float inverse[16];
        if (!matrixInvertAffine(m.m, inverse))
            return false;
        float o[3], d[3];
        for (int i = 0; i < 3; ++i)
        {
            o[i] = origin[0] * inverse[i] + origin[1] * inverse[4 + i] + origin[2] * inverse[8 + i] + inverse[12 + i];
            d[i] = direction[0] * inverse[i] + direction[1] * inverse[4 + i] + direction[2] * inverse[8 + i];
        }
        const float lo[3] = { box->m_min.x, box->m_min.y, box->m_min.z };
        const float hi[3] = { box->m_max.x, box->m_max.y, box->m_max.z };
        float enter = -FLT_MAX, leave = FLT_MAX;
        for (int i = 0; i < 3; ++i)
        {
            if (std::fabs(d[i]) < 1e-12f)
            {
                if (o[i] < lo[i] || o[i] > hi[i])
                    return false;
                continue;
            }
            float t0 = (lo[i] - o[i]) / d[i];
            float t1 = (hi[i] - o[i]) / d[i];
            if (t0 > t1)
            {
                const float swap = t0;
                t0 = t1;
                t1 = swap;
            }
            if (t0 > enter)
                enter = t0;
            if (t1 < leave)
                leave = t1;
        }
        if (enter > leave || enter <= 0.0f)
            return false;
        distance = enter;           // affine: the local parameter is the world one
        return true;
    }
    const float radius = object->getVisualBoundingRadius();
    if (radius <= 0.0f)
        return false;
    const float toCentre[3] = { m.m[12] - origin[0], m.m[13] - origin[1], m.m[14] - origin[2] };
    const float along = toCentre[0] * direction[0] + toCentre[1] * direction[1] + toCentre[2] * direction[2];
    const float centreSquared = toCentre[0] * toCentre[0] + toCentre[1] * toCentre[1] + toCentre[2] * toCentre[2];
    if (centreSquared <= radius * radius)
        return false;               // inside the sphere
    const float missSquared = centreSquared - along * along;
    if (along <= 0.0f || missSquared > radius * radius)
        return false;
    distance = along - std::sqrt(radius * radius - missSquared);
    return distance > 0.0f;
}

// A template class id no object has: intersectLine's exclusion argument, when nothing is excluded.
const unsigned int kExcludeNothing = 0xFFFFFFFFu;

Vec3 makeVec3(float x, float y, float z)
{
    Vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

float rowLength(const Mat4& m, int row)
{
    const float* r = &m.m[row * 4];
    return std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
}

bool nearlyEqual(const Mat4& a, const Mat4& b)
{
    for (int i = 0; i < 16; ++i)
    {
        const float scale = std::fabs(b.m[i]) > 1.0f ? std::fabs(b.m[i]) : 1.0f;
        if (std::fabs(a.m[i] - b.m[i]) > 1e-4f * scale)
            return false;
    }
    return true;
}

// A matrix as the console's Mat4 parser reads it (operator>> in Math/Mat4.cpp: "[a/b/c/d]" per row, no
// spaces). %.9g round-trips every float exactly.
std::string matrixText(const Mat4& m)
{
    std::string text;
    char row[128];
    for (int r = 0; r < 4; ++r)
    {
        std::snprintf(row, sizeof(row), "[%.9g/%.9g/%.9g/%.9g]", m.m[r * 4 + 0], m.m[r * 4 + 1], m.m[r * 4 + 2],
                      m.m[r * 4 + 3]);
        text += row;
    }
    return text;
}

// The two lines that set one object's matrix.
std::string transformScript(unsigned id, const Mat4& m)
{
    char line[64];
    std::snprintf(line, sizeof(line), "EditorObject.activate %u\nObject.absoluteTransformation ", id);
    return line + matrixText(m) + "\n";
}

// The history's replay of a creation: the same template, matrix and layer, under the object's first id,
// then its per-object properties - an object made again without them would lose them from the file.
std::string recreateScript(unsigned id, const std::string& templateName, const Mat4& m, int layer,
                           const ObjectProperties& properties)
{
    char head[48], tail[32];
    std::snprintf(head, sizeof(head), "EditorObject.recreate %u ", id);
    std::snprintf(tail, sizeof(tail), " %d\n", layer);
    return head + templateName + " " + matrixText(m) + tail + EditSession::propertiesScript(id, properties);
}

std::string trimmedText(const std::string& text)
{
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

// One engine console line with full access; its answer, trimmed.
std::string engineLine(const std::string& line)
{
    std::string answer;
    dice::hfe::io::mainConsole->executeLine(line + "\n", answer, false, false, false, false, true);
    return trimmedText(answer);
}

} // namespace

EditSession::EditSession(EditorLevel& level, FreeCamera& camera)
    : m_level(level)
    , m_camera(camera)
    , m_document(0)
    , m_mode(GizmoMode::Move)
    , m_space(GizmoSpace::World)
    , m_width(0)
    , m_height(0)
    , m_countsStale(true)
    , m_seenHistoryRevision(0)
    , m_gridStep(1.0f)
    , m_haveCopiedTransform(false)
    , m_markersShown(true)
    , m_activeCombatPoint(-1)
    , m_combatAreasShown(true)
    , m_canRelaunch(false)
    , m_relaunchSize(0)
    , m_summariesRevision(0)
    , m_summariesStale(true)
    , m_compareLayerSet(false)
    , m_nextTerrainChange(1)
    , m_terrainTool(false)
    , m_bundleGrouping(true)
    , m_armedBundleYaw(0.0f)
    , m_random(static_cast<unsigned>(std::time(0)))
    , m_brushTool(false)
    , m_brushEraseRadius(10.0f)
    , m_brushStroke(false)
    , m_brushStrokeErase(false)
    , m_haveLastStamp(false)
    , m_strokeSkipped(0)
    , m_alignStamp(0)
    , m_rotateAroundAnchor(false)
    , m_anchorMoveMode(false)
    , m_effectPreview(0)
    , m_minimapVersion(0)
    , m_undergrowthTried(false)
    , m_undergrowthLoaded(false)
    , m_undergrowthTool(false)
    , m_undergrowthMaterial(1)
    , m_undergrowthRadius(8.0f)
    , m_undergrowthStroke(false)
    , m_undergrowthStrokeErase(false)
    , m_nextUndergrowthChange(1)
    , m_nextTemplateEdit(1)
    , m_templateBatching(false)
    , m_wizard(*this)
    , m_nextMaterialEdit(1)
    , m_commonArchivesMounted(true)
    , m_clipboardOrigin()
    , m_hasCopiedArea(false)
    , m_objectArchivesMounted(true)
{
    m_history.setRunner(&consoleScriptRunner, 0);
    m_viewCentre[0] = m_viewCentre[1] = 0.0f;
    m_lastStamp[0] = m_lastStamp[1] = 0.0f;
}

void EditSession::setViewSize(int width, int height)
{
    m_width = width;
    m_height = height;
}

// Engine lookups

IObject* EditSession::object(unsigned id)
{
    if (id == 0 || !dice::hfe::world::objectManager)
        return 0;
    return dice::hfe::world::objectManager->getRegisteredObject(static_cast<int>(id));
}

bool EditSession::transformOf(unsigned id, Mat4& out)
{
    IObject* o = object(id);
    if (!o)
        return false;
    out = o->getAbsoluteTransformation();
    return true;
}

std::string EditSession::describe(unsigned id)
{
    char text[64];
    IObject* o = object(id);
    if (!o)
    {
        std::snprintf(text, sizeof(text), "#%u (not in the level)", id);
        return text;
    }
    std::snprintf(text, sizeof(text), " #%u", id);
    const dice::hfe::world::IObjectTemplate* objectTemplate = o->getTemplate();
    return (objectTemplate ? objectTemplate->getName() : std::string("(no template)")) + text;
}

Vec3 EditSession::rotationOf(const Mat4& m)
{
    // Object.rotation's get arm: getRotationZXY(m, z, x, y), answered as (-y, -x, -z). The Euler
    // helper reads the rotation rows as they stand, so a scaled matrix is normalised first.
    Mat4 unit = m;
    for (int row = 0; row < 3; ++row)
    {
        const float length = rowLength(m, row);
        if (length > 0.0f)
            for (int column = 0; column < 3; ++column)
                unit.m[row * 4 + column] /= length;
    }
    float z = 0.0f, x = 0.0f, y = 0.0f;
    dice::hfe::getRotationZXY(unit, z, x, y);
    return makeVec3(-y, -x, -z);
}

ObjectProperties EditSession::propertiesOf(IObject* o)
{
    ObjectProperties properties;
    if (!o)
        return properties;
    properties.group = static_cast<int>(o->getGroup());
    properties.disableChildren = o->getDisableChildren();
    properties.notInAI = o->getNotInAI();
    properties.isOvergrowth = o->getIsOvergrowth();
    properties.lightSourceMask = o->getLightSourceMask();
    // ObjectSpawner::queryInterface answers IID_IObjectSpawner with its IObjectSpawner sub-object.
    dice::hfe::world::IObjectSpawner* spawner =
        static_cast<dice::hfe::world::IObjectSpawner*>(o->queryInterface(dice::hfe::world::IID_IObjectSpawner));
    if (spawner)
    {
        properties.isSpawner = true;
        properties.controlPointId = spawner->getControlPointId();
    }
    return properties;
}

std::string EditSession::propertiesScript(unsigned id, const ObjectProperties& p)
{
    char text[256];
    std::snprintf(text, sizeof(text),
                  "EditorObject.activate %u\nObject.group %d\nObject.disableChildren %d\nObject.notInAI %d\n"
                  "Object.isOvergrowth %d\nObject.setLightSourceMask %d\n",
                  id, p.group, p.disableChildren ? 1 : 0, p.notInAI ? 1 : 0, p.isOvergrowth ? 1 : 0, p.lightSourceMask);
    std::string script = text;
    if (p.isSpawner)
    {
        std::snprintf(text, sizeof(text), "Object.setControlPointId %d\n", p.controlPointId);
        script += text;
    }
    return script;
}

void EditSession::setRotation(Mat4& m, const Vec3& yawPitchRoll)
{
    // Object.rotation's set arm: setRotationZXY(m, -roll, -pitch, -yaw), which rewrites only the
    // 3x3 block (translation kept). The rows' scale is put back afterwards.
    float scale[3];
    for (int row = 0; row < 3; ++row)
        scale[row] = rowLength(m, row);
    dice::hfe::setRotationZXY(m, -yawPitchRoll.z, -yawPitchRoll.y, -yawPitchRoll.x);
    for (int row = 0; row < 3; ++row)
        if (scale[row] > 0.0f)
            for (int column = 0; column < 3; ++column)
                m.m[row * 4 + column] *= scale[row];
}

// Picking

float EditSession::terrainDistance(const float origin[3], const float direction[3], float reach) const
{
    // Steps of 2 m along the ray, then halving between the last point above the ground and the first
    // one below it. A ridge thinner than a step can be missed; the editor's picking accepts that.
    const float step = 2.0f;
    float ground = 0.0f;
    if (m_level.groundHeight(origin[0], origin[2], ground) && origin[1] <= ground)
        return 0.0f;
    float above = 0.0f;
    for (float t = step; t <= reach; t += step)
    {
        const float x = origin[0] + direction[0] * t;
        const float y = origin[1] + direction[1] * t;
        const float z = origin[2] + direction[2] * t;
        if (!m_level.groundHeight(x, z, ground) || y > ground)
        {
            above = t;
            continue;
        }
        float below = t;
        for (int i = 0; i < 16; ++i)
        {
            const float mid = (above + below) * 0.5f;
            if (m_level.groundHeight(origin[0] + direction[0] * mid, origin[2] + direction[2] * mid, ground)
                && origin[1] + direction[1] * mid <= ground)
                below = mid;
            else
                above = mid;
        }
        return below;
    }
    return -1.0f;
}

EditSession::MarkerKind EditSession::markerKind(IObject* object) const
{
    const dice::hfe::world::IObjectTemplate* objectTemplate = object ? object->getTemplate() : 0;
    if (!objectTemplate)
        return MarkerNone;
    const unsigned classId = objectTemplate->getClassID();
    std::map<unsigned, int>::const_iterator known = m_markerClasses.find(classId);
    if (known != m_markerClasses.end())
        return static_cast<MarkerKind>(known->second);
    // The template class vocabulary (ObjectTemplate.create's first word), looked up once per class.
    const std::string name = className(classId);
    MarkerKind kind = MarkerNone;
    if (_stricmp(name.c_str(), "ObjectSpawner") == 0)
        kind = MarkerSpawner;
    else if (_stricmp(name.c_str(), "SpawnPoint") == 0)
        kind = MarkerSpawnPoint;
    else if (_stricmp(name.c_str(), "ControlPoint") == 0)
        kind = MarkerControlPoint;
    else if (_stricmp(name.c_str(), "Trigger") == 0)
        kind = MarkerTrigger;
    else if (_stricmp(name.c_str(), "AmbientEffectArea") == 0)
        kind = MarkerEffectArea;
    m_markerClasses[classId] = kind;
    return kind;
}

const char* EditSession::markerName(MarkerKind kind)
{
    switch (kind)
    {
    case MarkerSpawner:      return "object spawner";
    case MarkerSpawnPoint:   return "spawn point";
    case MarkerControlPoint: return "control point";
    case MarkerTrigger:      return "trigger";
    case MarkerEffectArea:   return "effect area";
    default:                 return "";
    }
}

namespace
{

// The console hands a value over with its quotes stripped; the file writes some values quoted (a
// sound's file name, or whatever the line already quoted).
std::string fileValue(const std::string& property, const std::string& value, const std::string& existing)
{
    const bool quoted = (!existing.empty() && existing[0] == '"') || _stricmp(property.c_str(), "soundFilename") == 0;
    if (!quoted || (value.size() >= 2 && value[0] == '"'))
        return value;
    return "\"" + value + "\"";
}

std::string lowerCase(const std::string& text)
{
    std::string out(text);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

std::string unquoted(const std::string& value)
{
    if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"')
        return value.substr(1, value.size() - 2);
    return value;
}

} // namespace

bool EditSession::templateInfo(const std::string& name, std::string& result)
{
    size_t file = 0;
    std::string className;
    if (!m_document || !m_document->templateFile(name, file, className))
    {
        result = "no level file this editor writes defines a template '" + name + "'";
        return false;
    }
    result = className + " " + name + " in " + m_document->fileName(file) + ":";
    const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(file, name);
    for (size_t i = 0; i < lines.size(); ++i)
        result += " " + lines[i].property + " " + lines[i].value + ";";
    return true;
}

bool EditSession::applyTemplateProperty(const std::string& name, const std::string& property, const std::string& value,
                                        std::string& error)
{
    size_t file = 0;
    std::string className;
    if (!m_document || !m_document->templateFile(name, file, className))
    {
        error = "no level file this editor writes defines a template '" + name
              + "' (templates of the Objects tree are the object editor's, WP7)";
        return false;
    }
    if (value.empty())
    {
        error = "a template line takes a value; EditorTemplate.remove takes lines away";
        return false;
    }
    // The value as the file writes it, against the line the file has for that key.
    const std::string key = LevelObjectFile::templateKey(property, value);
    std::string existing;
    const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(file, name);
    for (size_t i = 0; i < lines.size(); ++i)
        if (lines[i].key == key)
            existing = lines[i].value;
    const std::string written = fileValue(property, value, existing);
    // An effect area re-made (an undone delete) runs the client constructor again, which stops the game
    // on a bad linked effect.
    if (_stricmp(property.c_str(), "linkedEffectContainer") == 0 && !validEffectContainer(unquoted(value), error))
        return false;
    // The running level first, with the engine's own lines (each already ran at the load, so none is a
    // stop); the file only when the engine took it.
    const std::string activate = engineLine("ObjectTemplate.activeSafe " + className + " " + name);
    const std::string answer = engineLine("ObjectTemplate." + property + " " + written);
    if (!answer.empty())
    {
        error = "ObjectTemplate." + property + " " + written + " answered: " + answer;
        return false;
    }
    (void)activate;
    m_document->setTemplateValue(file, name, property, written);
    return true;
}

bool EditSession::removeTemplateLine(const std::string& name, const std::string& key, std::string& error)
{
    size_t file = 0;
    std::string className;
    if (!m_document || !m_document->templateFile(name, file, className))
    {
        error = "no level file this editor writes defines a template '" + name + "'";
        return false;
    }
    // The running level keeps the value until it is loaded again: a property has no "unset".
    m_document->setTemplateValue(file, name, key, std::string());
    return true;
}

bool EditSession::setTemplateProperty(const std::string& name, const std::string& property, const std::string& value,
                                      std::string& result)
{
    size_t file = 0;
    std::string className;
    if (!m_document || !m_document->templateFile(name, file, className))
    {
        result = "no level file this editor writes defines a template '" + name
               + "' (templates of the Objects tree are the object editor's, WP7)";
        return false;
    }
    if (value.empty() || property.empty())
    {
        result = "EditorTemplate.set takes a template, a property and a value";
        return false;
    }
    const std::string key = LevelObjectFile::templateKey(property, value);
    std::string old;
    bool present = false;
    const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(file, name);
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (lines[i].key == key)
        {
            old = lines[i].value;
            present = true;
        }
    }
    if (present && unquoted(old) == unquoted(value))
    {
        result = "the template has that value already";
        return true;
    }
    HistoryEntry entry;
    entry.doScript = "EditorTemplate.apply " + name + " " + property + " \"" + unquoted(value) + "\"\n";
    entry.undoScript = present ? "EditorTemplate.apply " + name + " " + property + " \"" + unquoted(old) + "\"\n"
                               : "EditorTemplate.remove " + name + " \"" + key + "\"\n";
    entry.label = "Set " + property + " of " + name;
    if (!m_history.execute(entry, result))
        return false;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + " (" + m_document->fileName(file) + ")";
    return true;
}

bool EditSession::snapSurface(float x, float z, float& height) const
{
    float terrain = 0.0f, water = 0.0f;
    const bool onTerrain = m_level.groundHeight(x, z, terrain);
    if (!onTerrain)
        return false;
    height = terrain;
    if (m_terrain.seaLevel(water) && water > height)
        height = water;
    if (m_snapModes.objects && dice::hfe::world::objectManager)
    {
        // A ray straight down onto the other objects' collision meshes, from high above, the selection left
        // out (retail cast it through the object's centre from a level height above, EditorEngine 0x100333A0).
        PickPredicator predicator(*this, true);
        IObject* hit = 0;
        Vec3 point = makeVec3(0.0f, 0.0f, 0.0f);
        Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
        int part = 0;
        const Vec3 from = makeVec3(x, height + 1000.0f, z);
        const Vec3 delta = makeVec3(0.0f, -2000.0f, 0.0f);
        if (dice::hfe::world::objectManager->intersectLine(hit, point, normal, part, from, delta, &predicator, true, false,
                                                           false, false, kExcludeNothing)
            && hit && point.y > height)
            height = point.y;
    }
    return true;
}

void EditSession::snapGrid(float& x, float& z) const
{
    if (!m_snapModes.grid)
        return;
    float s = m_terrain.spacing();
    if (s <= 0.0f)
        return;
    if (m_snapModes.grid == 2)
        s *= 0.5f;
    // The nearest grid line [CHOSEN: rounded, not retail's whole-metre truncation that broke a 0.5 m half grid].
    x = s * std::floor(x / s + 0.5f);
    z = s * std::floor(z / s + 0.5f);
}

bool EditSession::setSnapMode(const std::string& name, const std::string& value, std::string& result)
{
    const int number = std::atoi(value.c_str());
    if (name == "ground" || name == "objects")
    {
        if (value != "0" && value != "1")
        {
            result = "snap " + name + " is 0 or 1";
            return false;
        }
        (name == "ground" ? m_snapModes.ground : m_snapModes.objects) = number != 0;
    }
    else if (name == "grid")
    {
        if (value != "0" && value != "1" && value != "2")
        {
            result = "snap grid is 0 (off), 1 (the heightmap's grid) or 2 (its half)";
            return false;
        }
        m_snapModes.grid = number;
    }
    else
    {
        result = "no snap mode '" + name + "' (ground, objects, grid)";
        return false;
    }
    result = snapList();
    return true;
}

std::string EditSession::snapList() const
{
    char text[160];
    std::snprintf(text, sizeof(text), "snap: ground %d, objects %d, grid %d (%.2f m)", m_snapModes.ground ? 1 : 0,
                  m_snapModes.objects ? 1 : 0, m_snapModes.grid,
                  m_snapModes.grid ? m_terrain.spacing() * (m_snapModes.grid == 2 ? 0.5f : 1.0f) : 0.0f);
    return text;
}

bool EditSession::setMarkersShown(bool shown, std::string&)
{
    m_markersShown = shown;
    return true;
}

const float EditSession::kMarkerReach = 400.0f;

unsigned EditSession::markerAt(float x, float y, float radius) const
{
    if (!m_markersShown || m_width <= 0 || m_height <= 0 || !dice::hfe::world::objectManager)
        return 0;
    unsigned best = 0;
    float bestDistance = radius * radius;
    const float reach = kMarkerReach;
    const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
    {
        IObject* o = it->second;
        if (!o || !markerShown(markerKind(o)) || !canPick(o))
            continue;
        const Mat4& m = o->getAbsoluteTransformation();
        const float p[3] = { m.m[12], m.m[13], m.m[14] };
        const float dx = p[0] - m_camera.x(), dy = p[1] - m_camera.y(), dz = p[2] - m_camera.z();
        if (dx * dx + dy * dy + dz * dz > reach * reach)
            continue;
        float px = 0.0f, py = 0.0f;
        if (!m_camera.worldToPixel(p, m_width, m_height, px, py))
            continue;
        const float d = (px - x) * (px - x) + (py - y) * (py - y);
        if (d <= bestDistance)
        {
            bestDistance = d;
            best = it->first;
        }
    }
    return best;
}

unsigned EditSession::pick(float x, float y, std::string& detail) const
{
    if (m_width <= 0 || m_height <= 0 || !dice::hfe::world::objectManager)
    {
        detail = "no view to pick in";
        return 0;
    }
    // A marker under the cursor wins: markers are drawn over everything, and their objects have no
    // geometry for the line test below to meet.
    const unsigned marked = markerAt(x, y, 9.0f);
    if (marked)
    {
        detail = std::string("its ") + markerName(markerKind(object(marked))) + " marker";
        return marked;
    }
    float origin[3], direction[3];
    m_camera.rayThroughPixel(x, y, m_width, m_height, origin, direction);
    const float reach = m_camera.farPlane();

    // Objects first: the game's own line test against collision meshes, static and dynamic objects,
    // nearest hit, no terrain arm (its runtime check is still owed under RW-4; the terrain is
    // stepped here instead).
    PickPredicator predicator(*this);
    IObject* hit = 0;
    Vec3 point = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
    int part = 0;
    const Vec3 from = makeVec3(origin[0], origin[1], origin[2]);
    const Vec3 delta = makeVec3(direction[0] * reach, direction[1] * reach, direction[2] * reach);
    const bool objectHit = dice::hfe::world::objectManager->intersectLine(
        hit, point, normal, part, from, delta, &predicator, true, false, false, false, kExcludeNothing);
    float objectDistance = FLT_MAX;
    if (objectHit && hit)
    {
        const float dx = point.x - origin[0], dy = point.y - origin[1], dz = point.z - origin[2];
        objectDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    // Objects that intersectLine cannot see (no collision mesh), by their bounds.
    float boundsDistance = FLT_MAX;
    const unsigned boundsId =
        pickWithoutCollision(origin, direction, objectDistance < reach ? objectDistance : reach, boundsDistance);
    const float nearest = boundsDistance < objectDistance ? boundsDistance : objectDistance;

    const float ground = terrainDistance(origin, direction, nearest < reach ? nearest : reach);
    char text[128];
    if (ground >= 0.0f && ground < nearest)
    {
        std::snprintf(text, sizeof(text), "terrain at %.1f/%.1f/%.1f", origin[0] + direction[0] * ground,
                      origin[1] + direction[1] * ground, origin[2] + direction[2] * ground);
        detail = text;
        return 0;
    }
    if (boundsId != 0 && boundsDistance < objectDistance)
    {
        std::snprintf(text, sizeof(text), "its bounds at %.1f m (no collision mesh)", boundsDistance);
        detail = text;
        return boundsId;
    }
    if (!objectHit || !hit)
    {
        detail = "nothing within reach";
        return 0;
    }
    IObject* root = dice::hfe::world::getRootParent(hit);
    if (!root)
        root = hit;
    std::snprintf(text, sizeof(text), "hit at %.1f/%.1f/%.1f, %.1f m away", point.x, point.y, point.z,
                  objectDistance);
    detail = text;
    return root->getObjectId();
}

// Transform edits

bool EditSession::applyTransforms(const std::vector<TransformChange>& changes, const std::string& label,
                                  std::string& error)
{
    if (changes.empty())
    {
        error = "nothing is selected";
        return false;
    }
    HistoryEntry entry;
    entry.label = label;
    for (size_t i = 0; i < changes.size(); ++i)
        entry.doScript += transformScript(changes[i].id, changes[i].after);
    for (size_t i = changes.size(); i-- > 0;)
        entry.undoScript += transformScript(changes[i].id, changes[i].before);
    if (!m_history.execute(entry, error))
        return false;
    // The console said nothing; the objects must now hold what was asked for.
    for (size_t i = 0; i < changes.size(); ++i)
    {
        Mat4 now;
        if (!transformOf(changes[i].id, now) || !nearlyEqual(now, changes[i].after))
        {
            error = label + ": " + describe(changes[i].id) + " does not hold the new transform";
            return false;
        }
    }
    logLine(LogLevel::Info, "edit: %s", label.c_str());
    return true;
}

std::string EditSession::labelFor(const char* verb) const
{
    if (m_selection.size() == 1)
        return std::string(verb) + " " + describe(m_selection.primary());
    char text[64];
    std::snprintf(text, sizeof(text), "%s %u objects", verb, static_cast<unsigned>(m_selection.size()));
    return text;
}

void EditSession::runUndo()
{
    const ConsoleLineResult r = runConsoleLine("EditorHistory.undo");
    if (r.failed || !r.output.empty())
        logLine(LogLevel::Warning, "undo: %s", r.output.c_str());
}

void EditSession::runRedo()
{
    const ConsoleLineResult r = runConsoleLine("EditorHistory.redo");
    if (r.failed || !r.output.empty())
        logLine(LogLevel::Warning, "redo: %s", r.output.c_str());
}

void EditSession::runSave()
{
    if (m_previewBeforeSave)
    {
        m_saveRequested = true;
        return;
    }
    const ConsoleLineResult r = runConsoleLine("EditorLevel.save");
    logLine(r.failed ? LogLevel::Error : LogLevel::Info, "save: %s", r.output.c_str());
}

namespace
{

// "dock_crane #900, lrgfactorybuilding #573 and 3 more"
std::string objectList(const std::vector<unsigned>& ids)
{
    std::string text;
    const size_t shown = ids.size() < 4 ? ids.size() : 3;
    for (size_t i = 0; i < shown; ++i)
        text += (i ? ", " : "") + EditSession::describe(ids[i]);
    if (ids.size() > shown)
    {
        char more[48];
        std::snprintf(more, sizeof(more), " and %u more", static_cast<unsigned>(ids.size() - shown));
        text += more;
    }
    return text;
}

} // namespace

bool EditSession::levelStatus(std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level document: see the log from when the level opened";
        return false;
    }
    const LevelDocument::Report report = m_document->inspect();
    result = m_document->levelDirectory() + " - " + m_document->describe(report, true);
    if (!report.unsaved.empty())
        result += "; changed but in no file this editor writes (cannot be saved): " + objectList(report.unsaved);
    return true;
}

bool EditSession::saveLevel(std::string& result)
{
    return saveLevelFiles(result, false);
}

bool EditSession::resaveLevel(std::string& result)
{
    return saveLevelFiles(result, true);
}

bool EditSession::saveLevelFiles(std::string& result, bool everything)
{
    dropPendingSave();
    SavePlan plan;
    if (!planLevelSave(plan, everything, result))
        return false;
    plan.dropUnchanged();
    return writeLevelSave(plan, everything, result);
}

bool EditSession::planLevelSave(SavePlan& plan, bool everything, std::string& error)
{
    if (!m_document || !m_document->isOpen())
    {
        error = "no level document to save: see the log from when the level opened";
        return false;
    }
    if (m_restored)
    {
        error = "the level's files were put back from a backup: open the level again before saving (EditorLevel.open)";
        return false;
    }
    // The level's own files, one part each, under a part that answers for them all.
    SavePart& level = plan.part("level");
    const size_t first = plan.parts().size();
    LevelDocument::Report report;
    if (!m_document->planSave(plan, report, everything, error))
        return false;
    const size_t last = plan.parts().size();
    const std::string written = m_document->describe(report, false);
    const SavePlan* const planned = &plan;
    level.onReport([planned, first, last, everything, written](const SavePart&) {
        if (!everything)
            return written == "no changes" ? std::string("nothing to write: no changes") : "saved " + written;
        int files = 0;
        for (size_t p = first; p < last; ++p)
            if (!planned->parts()[p].refused())
                files += static_cast<int>(planned->parts()[p].changed());
        char count[96];
        std::snprintf(count, sizeof(count), "resaved %d level file%s", files, files == 1 ? "" : "s");
        return count + std::string(written == "no changes" ? " (none had changes)" : "; changes: " + written);
    });
    // Written what can be; the rest stays modified, and the save says so.
    if (!report.unsaved.empty())
        plan.part("unsaved objects").refuse("NOT saved - changed objects that belong to no file this editor writes yet: "
                                            + objectList(report.unsaved));
    // The retail layer model (E14): the editor file is saved; every game-mode file the layers make, and Layer.con.
    size_t gameplay = 0;
    if (m_layerModel.active() && m_document->gameplayFile(gameplay))
    {
        const bool editorFilePlanned = plan.plans(m_layerModel.editorPath());
        m_layerModel.planGenerated(m_document->objects(gameplay).render(), plan.part("gameplay layers"), editorFilePlanned);
    }
    // What the terrain compile below follows, taken before the parts on the way are marked saved.
    const bool heightsChanged = m_terrain.changedSinceSave();
    const bool surfaceChanged = m_editableTerrain.active() && (m_editableTerrain.painted() || m_editableTerrain.groundChanged());
    // The undergrowth density map, and the object templates edited with the level (the object editor, WP7).
    planUndergrowth(plan);
    planObjectTemplates(plan);
    // The material files and animation systems edited with the level (the material and animation editors).
    planMaterialFiles(plan);
    planAnimationFiles(plan);
    // The terrain's ground properties (Editor/GroundProperties.con) and the painted terrain maps (its detail maps and
    // colormaps).
    planGroundProperties(plan);
    planTerrainMaps(plan);
    // The mod's and the level's description files (the mod manager's pages).
    if (m_descriptions.modified())
        m_descriptions.planSave(plan.part("descriptions"));
    // The terrain the game draws, compiled when its heights, paint or ground properties changed; the heights are saved
    // with it or not at all - heights alone would leave the game walking on one ground and drawing another.
    planCompiledTerrain(plan, heightsChanged, surfaceChanged);
    // The painted overgrowth (its maps, regenerated plants and collision script).
    if (m_editableTerrain.active() && m_editableTerrain.overgrowthPainted())
        planOvergrowth(plan, true);
    // The editable roads (their files, the compiled roads and the changed road templates).
    planRoads(plan, false);
    // The HUD scripts edited with the level (the HUD editor).
    if (m_hudDocument)
        m_hudDocument->planSave(plan);
    // Every part written: the history's saved point is now.
    plan.part("history").onWritten([this, planned]() {
        if (!planned->anyRefused())
            m_history.markSaved();
    });
    return true;
}

bool EditSession::writeLevelSave(SavePlan& plan, bool everything, std::string& result)
{
    if (!writeSave(plan, everything ? "EditorLevel.resave" : "EditorLevel.save", result))
        return false;
    // The level's own files unchanged but other documents written: the answer says what was written.
    const std::string nothing = "nothing to write: no changes; ";
    if (result.compare(0, nothing.size(), nothing) == 0)
        result.erase(0, nothing.size());
    return !plan.anyRefused();
}

std::string EditSession::unsavedSummary()
{
    if (!m_document || !m_document->isOpen())
        return "The level's object files could not be opened, so nothing can be saved.";
    const LevelDocument::Report report = m_document->inspect();
    std::string text = m_document->describe(report, false) + ".";
    if (!report.unsaved.empty())
        text += " Changed but not savable in this version: " + objectList(report.unsaved) + ".";
    if (m_terrain.changedSinceSave())
        text += m_editableTerrain.active() ? " The terrain heights have changed."
                                           : " The terrain heights have changed; saving them needs the editable terrain "
                                             "(+editableTerrain 1).";
    if (undergrowthChanged())
        text += " The undergrowth has been painted.";
    if (m_editableTerrain.painted())
        text += " The terrain has been painted.";
    if (m_editableTerrain.overgrowthPainted())
        text += " The overgrowth has been painted.";
    if (m_roads.changed())
        text += " The roads changed.";
    if (m_hudDocument)
        text += m_hudDocument->unsavedText();
    const std::vector<size_t> templates = m_templateDocuments.modified();
    for (size_t i = 0; i < templates.size(); ++i)
        text += (i ? ", " : " Object template files changed: ") + m_templateDocuments.document(templates[i]).path
              + (i + 1 == templates.size() ? "." : "");
    if (m_materials.modified())
        text += " The material files changed.";
    if (m_layerModel.active() && m_layerModel.layersChanged())
        text += " The gameplay layers changed.";
    const std::vector<size_t> animations = m_animations.modified();
    for (size_t i = 0; i < animations.size(); ++i)
        text += (i ? ", " : " Animation systems changed: ") + m_animations.document(animations[i]).path
              + (i + 1 == animations.size() ? "." : "");
    if (m_descriptions.modified())
        text += " Description files changed: " + m_descriptions.modifiedList() + ".";
    return text;
}

bool EditSession::hasUnsavedChanges()
{
    if (m_terrain.changedSinceSave() || !m_templateDocuments.modified().empty() || undergrowthChanged()
        || m_materials.modified() || m_descriptions.modified() || animationsModified()
        || (m_layerModel.active() && m_layerModel.layersChanged()) || m_editableTerrain.groundChanged()
        || m_editableTerrain.painted() || m_editableTerrain.overgrowthPainted() || m_roads.changed())
        return true;
    if (!m_document || !m_document->isOpen())
        return m_history.isDirty();
    const LevelDocument::Report report = m_document->inspect();
    for (size_t f = 0; f < report.files.size(); ++f)
        if (report.files[f].changed)
            return true;
    return !report.settings.empty() || !report.unsaved.empty();
}

// IEditorCommands

bool EditSession::activateObject(unsigned id, std::string& error)
{
    if (!dice::hfe::world::objectManager)
    {
        error = "no object manager";
        return false;
    }
    IObject* o = object(resolve(id));
    // A failed activation clears the cursor, so the Object.* lines after it act on nothing - the
    // behaviour the engine gives a failed ObjectTemplate.create - instead of on the previous object.
    dice::hfe::world::objectManager->setActiveObject(o);
    if (!o)
    {
        char text[64];
        std::snprintf(text, sizeof(text), "no object #%u in the level", id);
        error = text;
        return false;
    }
    return true;
}

bool EditSession::selectObject(unsigned id, bool add, std::string& error)
{
    if (id == 0 && !add)
    {
        m_selection.clear();
        return true;
    }
    if (!object(id))
    {
        char text[64];
        std::snprintf(text, sizeof(text), "no object #%u in the level", id);
        error = text;
        return false;
    }
    if (add)
        m_selection.add(id);
    else
        m_selection.set(id);
    return true;
}

bool EditSession::resolveObject(const std::string& reference, unsigned& id, std::string& error)
{
    // A number is an id as it stands (0 included: callers give it their own meaning).
    size_t digits = 0;
    while (digits < reference.size() && reference[digits] >= '0' && reference[digits] <= '9')
        ++digits;
    if (!reference.empty() && digits == reference.size())
    {
        id = static_cast<unsigned>(std::strtoul(reference.c_str(), 0, 10));
        return true;
    }

    // `template` or `template:n`: the n-th live root object of the template, ids ascending.
    std::string name = reference;
    unsigned ordinal = 0;
    const size_t colon = reference.rfind(':');
    if (colon != std::string::npos)
    {
        name = reference.substr(0, colon);
        const std::string number = reference.substr(colon + 1);
        if (number.empty() || number.find_first_not_of("0123456789") != std::string::npos)
        {
            error = "not an object reference: '" + reference + "' (an id, a template, or template:n)";
            return false;
        }
        ordinal = static_cast<unsigned>(std::strtoul(number.c_str(), 0, 10));
    }
    unsigned seen = 0;
    if (dice::hfe::world::objectManager)
    {
        const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
        for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        {
            IObject* candidate = it->second;
            if (!candidate || !candidate->getTemplate()
                || _stricmp(candidate->getTemplate()->getName().c_str(), name.c_str()) != 0)
                continue;
            if (seen++ == ordinal)
            {
                id = it->first;
                return true;
            }
        }
    }
    char text[200];
    std::snprintf(text, sizeof(text), "no object %s: the level has %u of template %s", reference.c_str(), seen,
                  name.c_str());
    error = text;
    return false;
}

void EditSession::pickInto(float x, float y, PickMode mode, std::string& result)
{
    // Objects are what the click selects: Delete removes them again, not the combat area's point or
    // the ambient sound.
    m_activeCombatPoint = -1;
    m_activeSound.clear();
    std::string detail;
    const unsigned id = pick(x, y, detail);
    if (id == 0)
    {
        if (mode == PickMode::Set)
        {
            m_selection.clear();
            result = "nothing selected: " + detail;
        }
        else
        {
            result = "selection unchanged: " + detail;
        }
        return;
    }
    if (mode == PickMode::Toggle)
    {
        m_selection.toggle(id);
        result = describe(id) + (m_selection.contains(id) ? " added" : " removed") + " (" + detail + ")";
        return;
    }
    if (mode == PickMode::Add)
        m_selection.add(id);
    else
        m_selection.set(id);
    result = describe(id) + " (" + detail + ")";
}

bool EditSession::pickAt(float x, float y, std::string& result)
{
    pickInto(x, y, PickMode::Set, result);
    return true;
}

bool EditSession::pickAdd(float x, float y, std::string& result)
{
    pickInto(x, y, PickMode::Add, result);
    return true;
}

bool EditSession::selectionInfo(std::string& result)
{
    result.clear();
    const size_t shown = m_selection.size() < 20 ? m_selection.size() : 20;
    for (size_t i = 0; i < shown; ++i)
    {
        const unsigned id = m_selection.ids()[i];
        IObject* o = object(id);
        if (!o)
            continue;
        const Mat4& m = o->getAbsoluteTransformation();
        const Vec3 angles = rotationOf(m);
        const float position[3] = { m.m[12], m.m[13], m.m[14] };
        const float rotation[3] = { angles.x, angles.y, angles.z };
        char layer[32];
        std::snprintf(layer, sizeof(layer), " layer %d", static_cast<int>(o->getLayer()));
        // The per-object properties that are set (phase B).
        const ObjectProperties p = propertiesOf(o);
        char extra[96] = "";
        std::snprintf(extra, sizeof(extra), "%s%s%s%s mask %d", p.group ? " group " : "",
                      p.group ? std::to_string(p.group).c_str() : "", p.disableChildren ? " disableChildren" : "",
                      p.notInAI ? " notInAI" : "", p.lightSourceMask);
        std::string flags = std::string(extra) + (p.isOvergrowth ? " overgrowth" : "");
        if (p.isSpawner)
            flags += " controlPointId " + std::to_string(p.controlPointId);
        result += (result.empty() ? "" : "; ") + describe(id) + " at " + LevelObjectFile::formatPosition(position)
                  + " rotation " + LevelObjectFile::formatRotation(rotation) + layer + flags;
    }
    if (m_selection.size() > shown)
    {
        char more[48];
        std::snprintf(more, sizeof(more), "; and %u more", static_cast<unsigned>(m_selection.size() - shown));
        result += more;
    }
    if (result.empty())
        result = "nothing is selected";
    return true;
}

bool EditSession::selectTemplate(const std::string& name, std::string& result)
{
    m_selection.clear();
    if (dice::hfe::world::objectManager)
    {
        const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
        for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        {
            IObject* object = it->second;
            if (object && object->getTemplate() && _stricmp(object->getTemplate()->getName().c_str(), name.c_str()) == 0
                && canPick(object))
                m_selection.add(it->first);
        }
    }
    char text[160];
    std::snprintf(text, sizeof(text), "%u objects of template %s selected", static_cast<unsigned>(m_selection.size()),
                  name.c_str());
    result = text;
    return true;
}

bool EditSession::translateSelection(const Vec3& delta, std::string& error)
{
    std::vector<TransformChange> changes;
    for (size_t i = 0; i < m_selection.ids().size(); ++i)
    {
        TransformChange change;
        change.id = m_selection.ids()[i];
        if (!transformOf(change.id, change.before))
            continue;
        change.after = change.before;
        change.after.m[12] += delta.x;
        change.after.m[13] += delta.y;
        change.after.m[14] += delta.z;
        changes.push_back(change);
    }
    return applyTransforms(changes, labelFor("Move"), error);
}

bool EditSession::rotateSelection(const Vec3& yawPitchRollDelta, std::string& error)
{
    std::vector<TransformChange> changes;
    for (size_t i = 0; i < m_selection.ids().size(); ++i)
    {
        TransformChange change;
        change.id = m_selection.ids()[i];
        if (!transformOf(change.id, change.before))
            continue;
        Vec3 angles = rotationOf(change.before);
        angles.x += yawPitchRollDelta.x;
        angles.y += yawPitchRollDelta.y;
        angles.z += yawPitchRollDelta.z;
        change.after = change.before;
        setRotation(change.after, angles);
        changes.push_back(change);
    }
    return applyTransforms(changes, labelFor("Rotate"), error);
}

bool EditSession::setSelectionPosition(const Vec3& position, std::string& error)
{
    TransformChange change;
    change.id = m_selection.primary();
    if (!transformOf(change.id, change.before))
    {
        error = "nothing is selected";
        return false;
    }
    change.after = change.before;
    change.after.m[12] = position.x;
    change.after.m[13] = position.y;
    change.after.m[14] = position.z;
    return applyTransforms(std::vector<TransformChange>(1, change), "Move " + describe(change.id), error);
}

bool EditSession::setSelectionRotation(const Vec3& yawPitchRoll, std::string& error)
{
    TransformChange change;
    change.id = m_selection.primary();
    if (!transformOf(change.id, change.before))
    {
        error = "nothing is selected";
        return false;
    }
    change.after = change.before;
    setRotation(change.after, yawPitchRoll);
    return applyTransforms(std::vector<TransformChange>(1, change), "Rotate " + describe(change.id), error);
}

bool EditSession::undo(std::string& error)
{
    const HistoryEntry* entry = m_history.nextUndo();
    const std::string label = entry ? entry->label : std::string();
    if (!m_history.undo(error))
        return false;
    logLine(LogLevel::Info, "undo: %s", label.c_str());
    return true;
}

bool EditSession::redo(std::string& error)
{
    const HistoryEntry* entry = m_history.nextRedo();
    const std::string label = entry ? entry->label : std::string();
    if (!m_history.redo(error))
        return false;
    logLine(LogLevel::Info, "redo: %s", label.c_str());
    return true;
}

void EditSession::clearHistory(const std::string& reason)
{
    if (m_history.entries().empty())
        return;
    m_history.clear();
    logLine(LogLevel::Warning, "undo history cleared: %s", reason.c_str());
}

bool EditSession::frameSelection(std::string& error)
{
    IObject* o = object(m_selection.primary());
    if (!o)
    {
        error = "nothing is selected";
        return false;
    }
    // The centre and size of the object's box (collision bounds and children, local space), carried
    // into the world; objects without one fall back to their visual bounding radius.
    const Mat4& m = o->getAbsoluteTransformation();
    float centre[3] = { m.m[12], m.m[13], m.m[14] };
    float radius = o->getVisualBoundingRadius();
    dice::hfe::LocalBoundingBox* box = o->getFullBoundingBox();
    if (box && (box->m_max.x > box->m_min.x || box->m_max.y > box->m_min.y || box->m_max.z > box->m_min.z))
    {
        const float local[3] = { (box->m_min.x + box->m_max.x) * 0.5f, (box->m_min.y + box->m_max.y) * 0.5f,
                                 (box->m_min.z + box->m_max.z) * 0.5f };
        for (int i = 0; i < 3; ++i)
            centre[i] = local[0] * m.m[0 + i] + local[1] * m.m[4 + i] + local[2] * m.m[8 + i] + m.m[12 + i];
        const float sx = box->m_max.x - box->m_min.x;
        const float sy = box->m_max.y - box->m_min.y;
        const float sz = box->m_max.z - box->m_min.z;
        radius = 0.5f * std::sqrt(sx * sx + sy * sy + sz * sz) * rowLength(m, 0);
    }
    m_camera.frame(centre, radius);
    return true;
}

bool EditSession::setCameraMode(int mode, std::string& result)
{
    if (mode < 0 || mode > 3)
    {
        result = "the camera mode is 0 (free), 1 (top-down), 2 (orbit) or 3 (follow the terrain)";
        return false;
    }
    // An orbit goes round the selection, or else round the ground in the middle of the view.
    float centre[3];
    Mat4 m;
    if (transformOf(m_selection.primary(), m))
    {
        centre[0] = m.m[12];
        centre[1] = m.m[13];
        centre[2] = m.m[14];
    }
    else
    {
        pointAtViewCentre(centre);
    }
    m_camera.setMode(static_cast<CameraMode>(mode), centre);
    result = std::string("camera: ") + FreeCamera::modeName(m_camera.mode());
    return true;
}

bool EditSession::setCameraPosition(const Vec3& position, std::string& result)
{
    m_camera.place(position.x, position.y, position.z, m_camera.yawDegrees(), m_camera.pitchDegrees());
    char text[96];
    std::snprintf(text, sizeof(text), "camera at %.1f/%.1f/%.1f", m_camera.x(), m_camera.y(), m_camera.z());
    result = text;
    return true;
}

bool EditSession::resetCameraPosition(std::string& result)
{
    // The editor's start: 30 m above the ground south of the map centre, facing north and a little down
    // (EditorApp's default; the map's origin itself lies under the terrain).
    float ground = 100.0f;
    m_level.groundHeight(0.0f, -60.0f, ground);
    m_camera.place(0.0f, ground + 30.0f, -60.0f, 0.0f, -15.0f);
    char text[96];
    std::snprintf(text, sizeof(text), "camera at %.1f/%.1f/%.1f", m_camera.x(), m_camera.y(), m_camera.z());
    result = text;
    return true;
}

bool EditSession::setCameraSpeed(float metresPerSecond, std::string& result)
{
    m_camera.setSpeed(metresPerSecond > 0.0f ? metresPerSecond : FreeCamera::defaultSpeed());
    char text[64];
    std::snprintf(text, sizeof(text), "camera speed %.0f m/s", m_camera.speed());
    result = text;
    return true;
}

bool EditSession::cameraInfo(std::string& result)
{
    char text[160];
    std::snprintf(text, sizeof(text), "camera at %.2f/%.2f/%.2f, yaw %.1f, pitch %.1f, %.0f m/s, %s", m_camera.x(),
                  m_camera.y(), m_camera.z(), m_camera.yawDegrees(), m_camera.pitchDegrees(), m_camera.speed(),
                  FreeCamera::modeName(m_camera.mode()));
    result = text;
    return true;
}

bool EditSession::setSelectionProperty(const std::string& name, const std::string& value, std::string& result)
{
    // The console line each property is set by, and how its value reads.
    struct Property { const char* name; const char* command; bool flag; const char* label; };
    static const Property kProperties[] = {
        { "group", "Object.group", false, "group" },
        { "disableChildren", "Object.disableChildren", true, "disable children" },
        { "notInAI", "Object.notInAI", true, "not in AI" },
        { "isOvergrowth", "Object.isOvergrowth", true, "overgrowth" },
        { "lightSourceMask", "Object.setLightSourceMask", false, "light source mask" },
        { "controlPointId", "Object.setControlPointId", false, "control point id" },
    };
    const Property* property = 0;
    for (size_t i = 0; i < sizeof(kProperties) / sizeof(kProperties[0]); ++i)
        if (_stricmp(kProperties[i].name, name.c_str()) == 0)
            property = &kProperties[i];
    if (!property)
    {
        result = "no property '" + name + "': group, disableChildren, notInAI, isOvergrowth, lightSourceMask or "
                 "controlPointId";
        return false;
    }
    char* end = 0;
    const long number = std::strtol(value.c_str(), &end, 10);
    if (value.empty() || !end || *end != 0 || (property->flag && number != 0 && number != 1)
        || (_stricmp(property->name, "group") == 0 && (number < 0 || number > 255)))
    {
        result = property->flag ? std::string("the value is 0 or 1")
                                : (_stricmp(property->name, "group") == 0 ? std::string("a group is 0 to 255")
                                                                          : std::string("the value is a whole number"));
        return false;
    }
    if (m_selection.empty())
    {
        result = "nothing is selected";
        return false;
    }
    // Only objects whose file keeps the property have it changed, since the change must be saved.
    // The others (Overgrowth/OvergrowthCollision.con's copies, say, which a select-all-of-a-template
    // picks up too) are left as they are and counted in the answer.
    HistoryEntry entry;
    std::vector<std::string> undo;
    std::vector<unsigned> skipped;
    std::vector<unsigned> notSpawners;          // a control point id is a spawner's alone
    const bool spawnerProperty = _stricmp(property->name, "controlPointId") == 0;
    for (size_t i = 0; i < m_selection.ids().size(); ++i)
    {
        const unsigned id = m_selection.ids()[i];
        IObject* o = object(id);
        LevelDocument::RecordRef record;
        if (!o)
            continue;
        if (!m_document || !m_document->recordOf(id, record))
        {
            skipped.push_back(id);
            continue;
        }
        const ObjectProperties now = propertiesOf(o);
        if (spawnerProperty && !now.isSpawner)
        {
            notSpawners.push_back(id);
            continue;
        }
        const int old = _stricmp(property->name, "group") == 0 ? now.group
                      : _stricmp(property->name, "disableChildren") == 0 ? (now.disableChildren ? 1 : 0)
                      : _stricmp(property->name, "notInAI") == 0 ? (now.notInAI ? 1 : 0)
                      : _stricmp(property->name, "isOvergrowth") == 0 ? (now.isOvergrowth ? 1 : 0)
                      : spawnerProperty ? now.controlPointId
                      : now.lightSourceMask;
        if (old == static_cast<int>(number))
            continue;
        char line[160];
        std::snprintf(line, sizeof(line), "EditorObject.activate %u\n%s %ld\n", id, property->command, number);
        entry.doScript += line;
        std::snprintf(line, sizeof(line), "EditorObject.activate %u\n%s %d\n", id, property->command, old);
        undo.push_back(line);
    }
    char skippedText[320] = "";
    if (!skipped.empty())
        std::snprintf(skippedText, sizeof(skippedText), "; %u skipped: they belong to no file this editor writes (%s%s)",
                      static_cast<unsigned>(skipped.size()), describe(skipped[0]).c_str(), skipped.size() > 1 ? ", ..." : "");
    if (!notSpawners.empty())
    {
        char more[160];
        std::snprintf(more, sizeof(more), "; %u skipped: not object spawners (%s%s)", static_cast<unsigned>(notSpawners.size()),
                      describe(notSpawners[0]).c_str(), notSpawners.size() > 1 ? ", ..." : "");
        std::strncat(skippedText, more, sizeof(skippedText) - std::strlen(skippedText) - 1);
    }
    if (entry.doScript.empty())
    {
        if (spawnerProperty && notSpawners.size() + skipped.size() == m_selection.size() && !notSpawners.empty())
        {
            result = std::string("nothing selected is an object spawner with a record, so no control point id was set: refused")
                   + skippedText;
            return false;
        }
        if (skipped.size() == m_selection.size())
        {
            result = std::string("nothing selected belongs to a file this editor writes, so the ") + property->label
                   + " could not be saved: refused" + skippedText;
            return false;
        }
        result = std::string("the selection has that ") + property->label + " already" + skippedText;
        return true;
    }
    for (size_t i = undo.size(); i-- > 0;)
        entry.undoScript += undo[i];
    const size_t changed = undo.size();
    char label[128];
    if (changed == 1 && m_selection.size() == 1)
        std::snprintf(label, sizeof(label), "Set the %s to %ld on %s", property->label, number,
                      describe(m_selection.primary()).c_str());
    else
        std::snprintf(label, sizeof(label), "Set the %s to %ld on %u objects", property->label, number,
                      static_cast<unsigned>(changed));
    entry.label = label;
    if (!m_history.execute(entry, result))
        return false;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + skippedText;
    return true;
}

bool EditSession::setGizmoMode(int mode, std::string& error)
{
    if (mode < 0 || mode > 2)
    {
        error = "the gizmo mode is 0 (select), 1 (move) or 2 (rotate)";
        return false;
    }
    m_mode = static_cast<GizmoMode>(mode);
    return true;
}

bool EditSession::setGizmoSpace(int space, std::string& error)
{
    if (space < 0 || space > 1)
    {
        error = "the gizmo space is 0 (world) or 1 (local)";
        return false;
    }
    m_space = static_cast<GizmoSpace>(space);
    return true;
}

// Objects made and unmade (WP6)

unsigned EditSession::resolve(unsigned id) const
{
    // The chain is short (one link per deletion undone); the bound stops a cycle, which cannot occur.
    for (int step = 0; step < 64; ++step)
    {
        std::map<unsigned, unsigned>::const_iterator next = m_remap.find(id);
        if (next == m_remap.end())
            break;
        id = next->second;
    }
    return id;
}

bool EditSession::makeObject(const std::string& templateName, const Mat4& transform, int layer, unsigned& id,
                             std::string& error)
{
    if (!dice::hfe::io::mainConsole || !dice::hfe::world::objectManager)
    {
        error = "the engine console is not up";
        return false;
    }
    if (templateName.empty() || templateName.find_first_of(" \t/\\") != std::string::npos)
    {
        error = "'" + templateName + "' is not a template name";
        return false;
    }
    // QuadTreeCuller::createObject refuses an object at exactly y = 0: it would never be drawn.
    if (transform.m[13] == 0.0f)
    {
        error = "an object at exactly y = 0 never enters the engine's culler; give it a height";
        return false;
    }
    // Object.create (ObjectTemplateAdm::createObject) makes the object and the active object; an
    // unknown template makes nothing and clears the active object.
    const std::string answer = engineLine("Object.create " + templateName);
    IObject* made = dice::hfe::world::objectManager->getActiveObject();
    if (!made || !made->getTemplate() || _stricmp(made->getTemplate()->getName().c_str(), templateName.c_str()) != 0)
    {
        error = "no object template named '" + templateName + "' (Object.create answered '" + answer + "')";
        return false;
    }
    id = made->getObjectId();
    char layerLine[32];
    std::snprintf(layerLine, sizeof(layerLine), "Object.layer %d", layer);
    const std::string placed = engineLine("Object.absoluteTransformation " + matrixText(transform));
    const std::string layered = placed.empty() ? engineLine(layerLine) : std::string();
    if (!placed.empty() || !layered.empty())
    {
        // Half made is not made: the object goes again.
        made->forceDestroy();
        dice::hfe::world::objectManager->destroyObjects();
        error = "the new " + templateName + " could not be placed: " + (placed.empty() ? layered : placed);
        return false;
    }
    return true;
}

bool EditSession::createObject(const std::string& templateName, const Vec3& position, const Vec3& yawPitchRoll,
                               int layer, std::string& result)
{
    return createObjectOn(templateName, position, yawPitchRoll, layer, 0, result);
}

bool EditSession::createObjectOn(const std::string& templateName, const Vec3& position, const Vec3& yawPitchRoll,
                                 int layer, const float* standOn, std::string& result)
{
    // An object the editor cannot save would be lost without a word, so none is made without the file.
    if (!m_document || !m_document->isOpen())
    {
        result = "the level's objects file is not open, so a new object could not be saved: refused";
        return false;
    }
    if (layer < 0 || layer > 255)
    {
        result = "a layer is 0 to 255";
        return false;
    }
    Mat4 transform;
    for (int i = 0; i < 16; ++i)
        transform.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    setRotation(transform, yawPitchRoll);
    transform.m[12] = position.x;
    transform.m[13] = position.y;
    transform.m[14] = position.z;
    unsigned id = 0;
    if (!makeObject(templateName, transform, layer, id, result))
        return false;
    IObject* made = object(id);
    // Stood on the surface as the retail drop does (its snap, with the snap to the terrain on): the
    // anchor on it, or the bounding box's lowest point for a template without one.
    if (standOn && made)
    {
        const float y = snapPivotHeight(made, *standOn);
        if (y != transform.m[13] && y != 0.0f)
        {
            Mat4 stood = transform;
            stood.m[13] = y;
            std::string error;
            if (setTransformNow(id, stood, error))
                transform = stood;
            else
                logLine(LogLevel::Warning, "place: the snap to the surface did not apply: %s", error.c_str());
        }
    }
    ObjectPlacement placement;
    placement.templateName = made && made->getTemplate() ? made->getTemplate()->getName() : templateName;
    placement.position[0] = transform.m[12];
    placement.position[1] = transform.m[13];
    placement.position[2] = transform.m[14];
    // The angles as given, not as read back from the matrix: the record prints what was asked for.
    placement.rotation[0] = yawPitchRoll.x;
    placement.rotation[1] = yawPitchRoll.y;
    placement.rotation[2] = yawPitchRoll.z;
    placement.layer = layer;
    placement.hasLayer = true;
    m_document->addRecord(0, id, placement);

    // Made already (its id was needed for the undo), so the history records it as done.
    HistoryEntry entry;
    entry.label = "Create " + describe(id);
    entry.doScript = recreateScript(id, placement.templateName, transform, layer, propertiesOf(made));
    char undo[48];
    std::snprintf(undo, sizeof(undo), "EditorObject.remove %u\n", id);
    entry.undoScript = undo;
    m_history.record(entry);
    m_selection.set(id);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

bool EditSession::recreateObject(unsigned originalId, const std::string& templateName, const Mat4& transform,
                                 int layer, std::string& error)
{
    const unsigned previous = resolve(originalId);
    if (object(previous))
    {
        char text[80];
        std::snprintf(text, sizeof(text), "object #%u is still in the level; it is not made twice", previous);
        error = text;
        return false;
    }
    unsigned id = 0;
    if (!makeObject(templateName, transform, layer, id, error))
        return false;
    // Scripts may name the object by its first id or by the one it had last.
    if (previous != id)
        m_remap[previous] = id;
    if (originalId != previous && originalId != id)
        m_remap[originalId] = id;
    LevelDocument::RecordRef record;
    if (m_document && m_document->recordOf(previous, record))
    {
        m_document->rebind(record, id);
        m_document->setRecordDeleted(record, false);
        // Its template, if it went with the object (a no-op for the Objects tree's templates).
        m_document->setTemplateDeleted(record.file, templateName, false);
    }
    else if (m_document && m_document->isOpen())
    {
        // Its record went with a save made since the object was removed (a deleted record leaves the
        // file, and one made and unmade never reached it): the object is new to the file again - the
        // file that defines its template, written again if the save took it out too; StaticObjects.con
        // for the Objects tree's.
        size_t file = 0;
        std::string className;
        if (m_document->templateFile(templateName, file, className))
            m_document->setTemplateDeleted(file, templateName, false);
        else
        {
            std::map<std::string, RemovedTemplate>::const_iterator removed = m_removedTemplates.find(lowerCase(templateName));
            if (removed != m_removedTemplates.end())
            {
                file = removed->second.file;
                className = removed->second.className;
                m_document->addTemplate(file, className, templateName, removed->second.lines);
            }
        }
        ObjectPlacement placement = placementFor(templateName, transform, layer);
        setGameplayFlags(className, placement);
        m_document->addRecord(file, id, placement);
    }
    m_selection.set(id);
    return true;
}

bool EditSession::removeObject(unsigned id, std::string& error)
{
    const unsigned current = resolve(id);
    IObject* doomed = object(current);
    if (!doomed || !dice::hfe::world::objectManager)
    {
        char text[64];
        std::snprintf(text, sizeof(text), "no object #%u in the level", current);
        error = text;
        return false;
    }
    LevelDocument::RecordRef record;
    if (!deletable(current, error))
        return false;
    m_document->recordOf(current, record);
    const std::string templateName = m_document->objects(record.file).placement(record.record).templateName;
    m_selection.remove(current);
    // The engine's own removal (GameLogic's round reset, GameClient::destroyObject): forceDestroy -
    // no wreck, no death effects - then the destroy list drained at once, which also clears the
    // active object if it was this one. The editor's frame never drains it (only updateObjects does).
    doomed->forceDestroy();
    dice::hfe::world::objectManager->destroyObjects();
    m_document->setRecordDeleted(record, true);
    // A template of the file's own goes with its last object, and is kept here so the object can be
    // made again after a save took the block out. The engine keeps the template until the level loads
    // again.
    size_t file = 0;
    std::string className;
    if (m_document->templateFile(templateName, file, className) && file == record.file
        && m_document->liveRecords(templateName) == 0)
    {
        RemovedTemplate removed;
        removed.file = file;
        removed.className = className;
        const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(file, templateName);
        for (size_t i = 0; i < lines.size(); ++i)
            removed.lines.push_back(std::make_pair(lines[i].property, lines[i].value));
        m_removedTemplates[lowerCase(templateName)] = removed;
        m_document->setTemplateDeleted(file, templateName, true);
    }
    if (object(current))
    {
        error = describe(current) + " is still registered after its removal (something holds it)";
        return false;
    }
    return true;
}

bool EditSession::deleteSelection(std::string& result)
{
    if (m_selection.empty())
    {
        result = "nothing is selected";
        return false;
    }
    HistoryEntry entry;
    std::vector<std::string> recreate;
    char line[64];
    for (size_t i = 0; i < m_selection.ids().size(); ++i)
    {
        const unsigned id = m_selection.ids()[i];
        IObject* o = object(id);
        if (!o || !o->getTemplate())
            continue;
        if (!deletable(id, result))
            return false;
        std::snprintf(line, sizeof(line), "EditorObject.remove %u\n", id);
        entry.doScript += line;
        recreate.push_back(recreateScript(id, o->getTemplate()->getName(), o->getAbsoluteTransformation(),
                                          static_cast<int>(o->getLayer()), propertiesOf(o)));
    }
    for (size_t i = recreate.size(); i-- > 0;)
        entry.undoScript += recreate[i];
    entry.label = recreate.size() == 1 ? "Delete " + describe(m_selection.primary()) : labelFor("Delete");
    if (!m_history.execute(entry, result))
        return false;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

void EditSession::pointAtViewCentre(float out[3]) const
{
    float origin[3], direction[3];
    m_camera.rayThroughPixel(m_viewCentre[0], m_viewCentre[1], m_width, m_height, origin, direction);
    const float ground = terrainDistance(origin, direction, 300.0f);
    const float distance = ground > 0.0f ? ground : 20.0f;
    for (int i = 0; i < 3; ++i)
        out[i] = origin[i] + direction[i] * distance;
}

void EditSession::runDelete()
{
    const ConsoleLineResult r = runConsoleLine("EditorSelection.delete");
    if (r.failed)
        logLine(LogLevel::Warning, "delete: %s", r.output.c_str());
}

void EditSession::runDuplicate()
{
    runCommand("EditorSelection.duplicate", "duplicate");
}

void EditSession::runCommand(const std::string& line, const char* what)
{
    const ConsoleLineResult r = runConsoleLine(line);
    if (r.failed || !r.output.empty())
        logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "%s: %s", what, r.output.c_str());
}

void EditSession::runSnapToGround()
{
    const ConsoleLineResult r = runConsoleLine("EditorSelection.snapToGround");
    logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "snap to ground: %s", r.output.c_str());
}

ObjectPlacement EditSession::placementFor(const std::string& templateName, const Mat4& transform, int layer)
{
    ObjectPlacement placement;
    placement.templateName = templateName;
    const Vec3 angles = rotationOf(transform);
    for (int i = 0; i < 3; ++i)
        placement.position[i] = transform.m[12 + i];
    placement.rotation[0] = angles.x;
    placement.rotation[1] = angles.y;
    placement.rotation[2] = angles.z;
    placement.layer = layer;
    placement.hasLayer = true;
    return placement;
}

bool EditSession::deletable(unsigned id, std::string& error) const
{
    LevelDocument::RecordRef record;
    if (!m_document || !m_document->recordOf(id, record))
    {
        error = describe(id) + " belongs to no file this editor writes, so deleting it could not be saved: refused";
        return false;
    }
    if (m_document->takesNewObjects(record.file))
        return true;
    // The other files define a template per object: the object goes, and its template with it when it
    // is the template's last (removeObject).
    const std::string& templateName = m_document->objects(record.file).placement(record.record).templateName;
    size_t file = 0;
    std::string className;
    if (m_document->templateFile(templateName, file, className) && file == record.file)
        return true;
    error = describe(id) + " belongs to " + m_document->fileName(record.file) + ", which does not define its template "
          + templateName + "; only objects whose template their file defines are deleted from it: refused";
    return false;
}

void EditSession::setGameplayFlags(const std::string& className, ObjectPlacement& placement)
{
    placement.hostOnly = _stricmp(className.c_str(), "ObjectSpawner") == 0 || _stricmp(className.c_str(), "ControlPoint") == 0;
    placement.omitZeroRotation = _stricmp(className.c_str(), "ControlPoint") == 0;
}

namespace
{

// The objects of the level files the editor makes, each with a template of its own: the template class,
// the short name a command may use for it, how a label names it, and the file it goes in.
enum KindFile
{
    InGameplayLayer,
    InAmbientObjects
};
struct GameplayKind
{
    const char* className;
    const char* alias;
    const char* label;
    KindFile    file;
};
const GameplayKind kGameplayKinds[] = {
    { "ObjectSpawner", "spawner", "object spawner", InGameplayLayer },
    { "SpawnPoint", "spawnpoint", "spawn point", InGameplayLayer },
    { "ControlPoint", "controlpoint", "control point", InGameplayLayer },
    { "Trigger", "trigger", "trigger", InAmbientObjects },
    { "AmbientEffectArea", "effectarea", "effect area", InAmbientObjects },
};

const GameplayKind* gameplayKind(const std::string& name)
{
    for (size_t i = 0; i < sizeof(kGameplayKinds) / sizeof(kGameplayKinds[0]); ++i)
        if (_stricmp(name.c_str(), kGameplayKinds[i].className) == 0 || _stricmp(name.c_str(), kGameplayKinds[i].alias) == 0)
            return &kGameplayKinds[i];
    return 0;
}

typedef std::vector<std::pair<std::string, std::string> > TemplateLines;

void addLine(TemplateLines& lines, const char* property, const std::string& value)
{
    lines.push_back(std::make_pair(std::string(property), value));
}

} // namespace

std::vector<EditSession::LevelObjectRow> EditSession::levelObjectsOf(const char* className, const char* idKey) const
{
    std::vector<LevelObjectRow> rows;
    if (!m_document || !m_document->isOpen())
        return rows;
    for (size_t f = 0; f < m_document->fileCount(); ++f)
    {
        const LevelObjectFile& objects = m_document->objects(f);
        for (size_t r = 0; r < objects.recordCount(); ++r)
        {
            if (objects.isDeleted(r))
                continue;
            const std::string& name = objects.placement(r).templateName;
            std::string recordClass;
            if (!objects.findTemplate(name, recordClass) || _stricmp(recordClass.c_str(), className) != 0)
                continue;
            LevelObjectRow row;
            row.name = name;
            row.id = -1;
            const std::vector<LevelObjectFile::TemplateLine> lines = objects.templateLines(name);
            for (size_t i = 0; i < lines.size(); ++i)
                if (lines[i].key == idKey)
                    row.id = std::atoi(lines[i].value.c_str());
            // Where the object stands now: a record's placement is read back from the engine at a save.
            LevelDocument::RecordRef ref;
            ref.file = f;
            ref.record = r;
            IObject* o = object(m_document->idOf(ref));
            for (int k = 0; k < 3; ++k)
                row.position[k] = o ? o->getAbsoluteTransformation().m[12 + k] : objects.placement(r).position[k];
            if (o)
                row.layer = static_cast<int>(o->getLayer());
            rows.push_back(row);
        }
    }
    return rows;
}

std::vector<EditSession::LevelObjectRow> EditSession::controlPoints() const
{
    return levelObjectsOf("ControlPoint", "controlpointid");
}

bool EditSession::validEffectContainer(const std::string& name, std::string& error)
{
    // AmbientEffectArea's client constructor stops the game on a linked effect that is missing or
    // neither a BundleTemplate nor an EffectBundleTemplate (a DebugTypeError, which aborts); the editor
    // runs the client's code, so such a name is refused before an area is made or re-made with it.
    const dice::hfe::world::IObjectTemplate* effect =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!effect)
    {
        error = "no template named '" + name + "' to link as the effect area's effect";
        return false;
    }
    const unsigned classId = effect->getClassID();
    if (classId != dice::hfe::world::CID_BundleTemplate && classId != dice::hfe::world::CID_EffectBundleTemplate)
    {
        error = "'" + name + "' is a " + className(classId) + "; an effect area's effect is a Bundle or an EffectBundle";
        return false;
    }
    return true;
}

bool EditSession::createGameplayObject(const std::string& kind, const std::string& name, const Vec3& position,
                                       std::string& result)
{
    return makeLevelObject(kind, name, std::string(), position, result);
}

bool EditSession::createGameplayObjectHere(const std::string& kind, const std::string& name, std::string& result)
{
    float at[3];
    pointAtViewCentre(at);
    return createGameplayObject(kind, name, makeVec3(at[0], at[1], at[2]), result);
}

bool EditSession::createEffectArea(const std::string& name, const std::string& effect, const Vec3& position,
                                   std::string& result)
{
    if (effect.empty())
    {
        result = "a new effect area needs its effect (a Bundle or EffectBundle template)";
        return false;
    }
    return makeLevelObject("AmbientEffectArea", name, effect, position, result);
}

bool EditSession::createEffectAreaHere(const std::string& name, const std::string& effect, std::string& result)
{
    float at[3];
    pointAtViewCentre(at);
    return createEffectArea(name, effect, makeVec3(at[0], at[1], at[2]), result);
}

bool EditSession::makeLevelObject(const std::string& kind, const std::string& name, const std::string& effect,
                                  const Vec3& position, std::string& result)
{
    const GameplayKind* chosen = gameplayKind(kind);
    if (!chosen)
    {
        result = "no kind of level object '" + kind + "': ObjectSpawner (spawner), SpawnPoint (spawnpoint), ControlPoint "
                 "(controlpoint) or Trigger (trigger); an effect area is made with EditorGameplay.createEffectArea";
        return false;
    }
    const std::string cls = chosen->className;
    const std::string label = chosen->label;
    if (cls == "AmbientEffectArea" && effect.empty())
    {
        result = "a new effect area needs its effect: EditorGameplay.createEffectArea <name> <effect> <x/y/z>";
        return false;
    }
    if (!effect.empty() && !validEffectContainer(effect, result))
        return false;
    const bool ambient = chosen->file == InAmbientObjects;
    size_t file = 0;
    if (!m_document || !m_document->isOpen() || !(ambient ? m_document->ambientFile(file) : m_document->gameplayFile(file)))
    {
        result = std::string(ambient ? "the level's AmbientObjects.con" : "the level's gameplay layer file")
               + " is not open, so a new " + label + " could not be saved: refused";
        return false;
    }
    if (name.empty() || name.find_first_of(" \t/\\\"") != std::string::npos)
    {
        result = "'" + name + "' is not a template name (it names the new " + label + "'s template and object)";
        return false;
    }
    size_t definedIn = 0;
    std::string definedClass;
    if (m_document->templateFile(name, definedIn, definedClass)
        || (dice::hfe::world::objectTemplateManager && dice::hfe::world::objectTemplateManager->getTemplate(name)))
    {
        result = "a template named '" + name + "' exists already; a new " + label + " needs a name of its own";
        return false;
    }

    // What it belongs to: the nearest control point (a spawner, a spawn point) or trigger (an effect
    // area); a new control point or trigger gets an id of its own, one above the level's highest.
    const std::vector<LevelObjectRow> owners = ambient ? levelObjectsOf("Trigger", "triggerid") : controlPoints();
    const char* ownerLabel = ambient ? "trigger" : "control point";
    const LevelObjectRow* nearest = 0;
    float best = FLT_MAX;
    int highest = 0;
    for (size_t i = 0; i < owners.size(); ++i)
    {
        highest = std::max(highest, owners[i].id);
        const float dx = owners[i].position[0] - position.x, dz = owners[i].position[2] - position.z;
        if (owners[i].id >= 0 && dx * dx + dz * dz < best)
        {
            best = dx * dx + dz * dz;
            nearest = &owners[i];
        }
    }
    const bool spawner = cls == "ObjectSpawner";
    const bool ownsId = cls == "ControlPoint" || cls == "Trigger";
    char number[32];
    std::snprintf(number, sizeof(number), "%d", highest + 1);
    const std::string newId = number;
    std::snprintf(number, sizeof(number), "%d", nearest ? nearest->id : -1);
    const std::string ownerId = number;
    // The lines every stock template of the kind has (the stock files' own, minus modifiedByUser: the
    // retail editor wrote the user's account name there, and a new object needs none).
    TemplateLines lines;
    if (spawner)
    {
        addLine(lines, "isNotSaveable", "1");
        addLine(lines, "hasMobilePhysics", "0");
    }
    else if (cls == "SpawnPoint")
    {
        addLine(lines, "isNotSaveable", "1");
        if (nearest)
            addLine(lines, "setControlPointId", ownerId);
    }
    else if (cls == "ControlPoint")
    {
        addLine(lines, "setNetworkableInfo", "ControlPointInfo");
        addLine(lines, "isNotSaveable", "1");
        addLine(lines, "hasMobilePhysics", "0");
        addLine(lines, "hasCollisionPhysics", "1");
        addLine(lines, "physicsType", "Mesh");
        lines.push_back(std::make_pair(std::string(), std::string("rem -------------------------------------")));
        addLine(lines, "addTemplate", "flagpole");
        lines.push_back(std::make_pair(std::string(), std::string("rem -------------------------------------")));
        addLine(lines, "setControlPointName", name);
        // [CHOSEN] a radius a player can find (the engine's default is 1 m; the stock ones are 5 to 10).
        addLine(lines, "radius", "10");
        addLine(lines, "controlPointId", newId);
    }
    else if (cls == "Trigger")
    {
        addLine(lines, "isNotSaveable", "1");
        addLine(lines, "hasMobilePhysics", "0");
        addLine(lines, "triggerId", newId);
        // [CHOSEN] the stock dust triggers' values (the engine's defaults - radius 0, chance 0 - never
        // fire).
        addLine(lines, "radius", "10");
        addLine(lines, "events", "1");
        addLine(lines, "chance", "1");
        addLine(lines, "minimumTimeBetweenTriggering", "15");
    }
    else
    {
        addLine(lines, "isNotSaveable", "1");
        addLine(lines, "hasMobilePhysics", "0");
        // [CHOSEN] a radius like the stock ones (5 to 20).
        addLine(lines, "radius", "10");
        if (nearest)
            addLine(lines, "triggerId", ownerId);
        addLine(lines, "linkedEffectContainer", effect);
    }

    // The engine first, with the lines the file will hold; the file only once the engine took them all.
    std::string failed;
    std::string answer = engineLine("ObjectTemplate.create " + cls + " " + name);
    dice::hfe::world::IObjectTemplate* made =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!made)
        failed = "ObjectTemplate.create " + cls + " " + name + " made no template (" + answer + ")";
    if (failed.empty())
    {
        answer = engineLine("ObjectTemplate.activeSafe " + cls + " " + name);
        if (!answer.empty())
            failed = "ObjectTemplate.activeSafe answered: " + answer;
    }
    for (size_t i = 0; i < lines.size() && failed.empty(); ++i)
    {
        if (lines[i].first.empty())
            continue;
        const std::string line = "ObjectTemplate." + lines[i].first + " " + lines[i].second;
        answer = engineLine(line);
        if (!answer.empty())
            failed = line + " answered: " + answer;
    }
    if (!failed.empty())
    {
        result = "the new " + label + "'s template: " + failed + "; nothing was written (the engine keeps what it made "
                 "until the level loads again)";
        return false;
    }
    Mat4 transform;
    for (int i = 0; i < 16; ++i)
        transform.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    transform.m[12] = position.x;
    transform.m[13] = position.y;
    transform.m[14] = position.z;
    const int layer = newObjectLayer(file);
    unsigned id = 0;
    if (!makeObject(name, transform, layer, id, result))
        return false;
    // A spawner belongs to its control point by a line of its record; the new object is still active.
    if (spawner && nearest)
    {
        answer = engineLine("Object.setControlPointId " + ownerId);
        if (!answer.empty())
            logLine(LogLevel::Warning, "gameplay: Object.setControlPointId %s answered: %s", ownerId.c_str(), answer.c_str());
    }
    IObject* madeObject = object(id);
    const ObjectProperties properties = propertiesOf(madeObject);
    ObjectPlacement placement = placementFor(name, transform, layer);
    setGameplayFlags(cls, placement);
    if (properties.isSpawner)
    {
        placement.controlPointId = properties.controlPointId;
        placement.hasControlPointId = properties.controlPointId != -1;
    }
    m_document->addTemplate(file, cls, name, lines);
    m_document->addRecord(file, id, placement);

    // Made already (its id was needed for the undo), so the history records it as done.
    HistoryEntry entry;
    entry.label = "Create " + label + " " + name;
    entry.doScript = recreateScript(id, name, transform, layer, properties);
    char undo[48];
    std::snprintf(undo, sizeof(undo), "EditorObject.remove %u\n", id);
    entry.undoScript = undo;
    m_history.record(entry);
    m_selection.set(id);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + " (" + m_document->fileName(file) + ")";
    if (ownsId)
        result += ", id " + newId;
    else
        result += nearest ? std::string(" for ") + ownerLabel + " " + nearest->name + " (id " + ownerId + ")"
                          : std::string(" - the level has no ") + ownerLabel + " for it yet";
    return true;
}

// Combat areas (phase C)

namespace
{

// A float as the console reads it back exactly (istream >> float).
std::string exactFloat(float value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.9g", value);
    return text;
}

// The engine lines that make the area from nothing: create (which also makes it the console's active
// area), then every property, as CombatAreaConsole.cpp binds them. The engine only appends points, so
// an area is never edited in place.
std::string defineAreaScript(const LevelObjectFile::CombatAreaData& d)
{
    std::string script = "CombatArea.create " + d.name + "\n";
    script += "CombatArea.min " + exactFloat(d.minimum[0]) + "/" + exactFloat(d.minimum[1]) + "\n";
    script += "CombatArea.max " + exactFloat(d.maximum[0]) + "/" + exactFloat(d.maximum[1]) + "\n";
    for (size_t i = 0; i < d.points.size(); ++i)
        script += "CombatArea.addAreaPoint " + exactFloat(d.points[i].first) + "/" + exactFloat(d.points[i].second) + "\n";
    char line[64];
    std::snprintf(line, sizeof(line), "CombatArea.team %d\nCombatArea.vehicles %d\nCombatArea.layer %d\n", d.team,
                  d.vehicles, d.layer);
    script += line;
    std::snprintf(line, sizeof(line), "CombatArea.usedByPathFinding %d\nCombatArea.inverted %d\n",
                  d.usedByPathFinding ? 1 : 0, d.inverted ? 1 : 0);
    script += line;
    return script;
}

// The engine lines that remove it: make it active by name, then delete the active area.
std::string removeAreaScript(const std::string& name)
{
    return "CombatArea.active " + name + "\nCombatArea.deleteActiveCombatArea\n";
}

std::string syncLine(const std::string& name)
{
    return "EditorCombatArea.sync " + name + "\n";
}

} // namespace

bool EditSession::engineCombatArea(const std::string& name, LevelObjectFile::CombatAreaData& out)
{
    dice::hfe::CombatAreaManager* manager = dice::hfe::CombatAreaManager::getInstance();
    if (!manager)
        return false;
    for (size_t i = 0; i < manager->m_areas.size(); ++i)
    {
        const dice::hfe::CombatArea* area = manager->m_areas[i];
        if (!area || _stricmp(area->m_name.c_str(), name.c_str()) != 0)
            continue;
        out = LevelObjectFile::CombatAreaData();
        out.name = area->m_name;
        out.minimum[0] = area->m_minX;
        out.minimum[1] = area->m_minZ;
        out.maximum[0] = area->m_maxX;
        out.maximum[1] = area->m_maxZ;
        for (size_t p = 0; p < area->m_points.size(); ++p)
            out.points.push_back(std::make_pair(area->m_points[p].x, area->m_points[p].y));
        out.team = area->m_team;
        out.vehicles = area->m_vehicleCategory;
        out.layer = area->m_layer;
        out.usedByPathFinding = area->m_usedByPathFinding;
        out.inverted = area->m_isExclusion;
        return true;
    }
    return false;
}

std::vector<LevelObjectFile::CombatAreaData> EditSession::combatAreas()
{
    std::vector<LevelObjectFile::CombatAreaData> areas;
    dice::hfe::CombatAreaManager* manager = dice::hfe::CombatAreaManager::getInstance();
    if (!manager)
        return areas;
    for (size_t i = 0; i < manager->m_areas.size(); ++i)
    {
        LevelObjectFile::CombatAreaData data;
        if (manager->m_areas[i] && engineCombatArea(manager->m_areas[i]->m_name, data))
            areas.push_back(data);
    }
    return areas;
}

bool EditSession::combatAreaEditable(const std::string& name, bool mustExist, LevelObjectFile::CombatAreaData& current,
                                     std::string& error)
{
    size_t file = 0;
    if (!m_document || !m_document->isOpen() || !m_document->gameplayFile(file))
    {
        error = "the level's gameplay layer file is not open, so a combat area change could not be saved: refused";
        return false;
    }
    if (name.empty() || name.find_first_of(" \t/\\\"") != std::string::npos)
    {
        error = "'" + name + "' is not a combat area name";
        return false;
    }
    const bool exists = engineCombatArea(name, current);
    if (mustExist && !exists)
    {
        error = "no combat area named '" + name + "' in the level";
        return false;
    }
    size_t area = 0;
    if (exists && m_document->objects(file).findCombatArea(name, area) && !m_document->combatAreaWritable(file, area))
    {
        error = "combat area " + name + "'s block in " + m_document->fileName(file)
              + " has lines the editor does not write, so it is not rewritten: refused";
        return false;
    }
    return true;
}

bool EditSession::replaceCombatArea(const LevelObjectFile::CombatAreaData& before,
                                    const LevelObjectFile::CombatAreaData& after, const std::string& label,
                                    std::string& result)
{
    HistoryEntry entry;
    entry.label = label;
    entry.doScript = removeAreaScript(before.name) + defineAreaScript(after) + syncLine(before.name);
    entry.undoScript = removeAreaScript(before.name) + defineAreaScript(before) + syncLine(before.name);
    if (!m_history.execute(entry, result))
        return false;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

bool EditSession::syncCombatArea(const std::string& name, std::string& error)
{
    size_t file = 0;
    if (!m_document || !m_document->isOpen() || !m_document->gameplayFile(file))
    {
        error = "the level's gameplay layer file is not open";
        return false;
    }
    LevelObjectFile::CombatAreaData data;
    const bool exists = engineCombatArea(name, data);
    return m_document->syncCombatArea(file, name, exists ? &data : 0, error);
}

bool EditSession::syncCombatAreaManager(std::string& error)
{
    size_t file = 0;
    dice::hfe::CombatAreaManager* manager = dice::hfe::CombatAreaManager::getInstance();
    if (!manager || !m_document || !m_document->isOpen() || !m_document->gameplayFile(file))
    {
        error = "no combat area manager, or the gameplay layer file is not open";
        return false;
    }
    // The file writes a setting it had, and one that now differs from the engine's default (use off,
    // damage 20, 10 s outside - CombatAreaManager's constructor).
    const LevelObjectFile::CombatAreaSettings& loaded = m_document->objects(file).loadedCombatAreaSettings();
    LevelObjectFile::CombatAreaSettings now;
    now.use = manager->m_useCombatAreas;
    now.hasUse = loaded.hasUse || now.use;
    now.damage = manager->m_defaultDamage;
    now.hasDamage = loaded.hasDamage || now.damage != 20.0f;
    now.timeAllowedOutside = manager->m_defaultTimeAllowedOutside;
    now.hasTimeAllowedOutside = loaded.hasTimeAllowedOutside || now.timeAllowedOutside != 10.0f;
    m_document->setCombatAreaSettings(file, now);
    return true;
}

bool EditSession::listCombatAreas(std::string& result)
{
    const std::vector<LevelObjectFile::CombatAreaData> areas = combatAreas();
    result.clear();
    char text[160];
    for (size_t i = 0; i < areas.size(); ++i)
    {
        std::snprintf(text, sizeof(text), "%s (%u points, team %d, vehicles %d, layer %d%s%s)", areas[i].name.c_str(),
                      static_cast<unsigned>(areas[i].points.size()), areas[i].team, areas[i].vehicles, areas[i].layer,
                      areas[i].usedByPathFinding ? ", pathfinding" : "", areas[i].inverted ? ", inverted" : "");
        result += (result.empty() ? "" : "; ") + std::string(text);
    }
    dice::hfe::CombatAreaManager* manager = dice::hfe::CombatAreaManager::getInstance();
    if (manager)
    {
        std::snprintf(text, sizeof(text), "%suse %d, time allowed outside %g s, damage %g", result.empty() ? "" : "; ",
                      manager->m_useCombatAreas ? 1 : 0, manager->m_defaultTimeAllowedOutside, manager->m_defaultDamage);
        result += text;
    }
    if (result.empty())
        result = "no combat areas";
    return true;
}

bool EditSession::combatAreaInfo(const std::string& name, std::string& result)
{
    LevelObjectFile::CombatAreaData data;
    if (!engineCombatArea(name, data))
    {
        result = "no combat area named '" + name + "' in the level";
        return false;
    }
    std::string lines = LevelObjectFile::formatCombatArea(data, "; ");
    result = lines.substr(0, lines.size() > 2 ? lines.size() - 2 : 0);
    return true;
}

bool EditSession::createCombatArea(const std::string& name, const Vec3& centre, float size, std::string& result)
{
    LevelObjectFile::CombatAreaData existing;
    if (!combatAreaEditable(name, false, existing, result))
        return false;
    if (engineCombatArea(name, existing))
    {
        result = "a combat area named '" + name + "' exists already";
        return false;
    }
    if (!(size > 1.0f))
    {
        result = "a new combat area needs a size above 1 m";
        return false;
    }
    // [CHOSEN] a square around the point, its corners the first four points, on the layer the
    // level's other areas use (else the gameplay layer's objects').
    LevelObjectFile::CombatAreaData area;
    area.name = name;
    const float h = size * 0.5f;
    area.points.push_back(std::make_pair(centre.x - h, centre.z - h));
    area.points.push_back(std::make_pair(centre.x + h, centre.z - h));
    area.points.push_back(std::make_pair(centre.x + h, centre.z + h));
    area.points.push_back(std::make_pair(centre.x - h, centre.z + h));
    size_t file = 0;
    m_document->gameplayFile(file);
    const std::vector<LevelObjectFile::CombatAreaData> areas = combatAreas();
    area.layer = m_layerModel.active() ? m_layerModel.activeLayer
               : !areas.empty() ? areas.back().layer : (m_document->fileLayer(file) >= 0 ? m_document->fileLayer(file) : 1);
    HistoryEntry entry;
    entry.label = "Create combat area " + name;
    entry.doScript = defineAreaScript(area) + syncLine(name);
    entry.undoScript = removeAreaScript(name) + syncLine(name);
    if (!m_history.execute(entry, result))
        return false;
    // A file with no manager lines gets the master switch on, or the game ignores its areas.
    dice::hfe::CombatAreaManager* manager = dice::hfe::CombatAreaManager::getInstance();
    if (manager && !manager->m_useCombatAreas)
        logLine(LogLevel::Warning, "combat area: CombatAreaManager.use is off, so the game ignores the areas "
                                   "(EditorCombatArea.manager use 1)");
    m_activeCombatArea = name;
    m_activeCombatPoint = -1;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + " (" + m_document->fileName(file) + ")";
    return true;
}

bool EditSession::copyCombatArea(const std::string& name, std::string& result)
{
    LevelObjectFile::CombatAreaData area;
    if (!engineCombatArea(name, area))
    {
        result = "no combat area named '" + name + "'";
        return false;
    }
    m_copiedArea = area;
    m_hasCopiedArea = true;
    char text[160];
    std::snprintf(text, sizeof(text), "copied the combat area %s (%u points)", area.name.c_str(),
                  static_cast<unsigned>(area.points.size()));
    result = text;
    return true;
}

bool EditSession::pasteCombatArea(const std::string& name, std::string& result)
{
    if (!m_hasCopiedArea)
    {
        result = "no combat area has been copied (EditorCombatArea.copy <name>)";
        return false;
    }
    LevelObjectFile::CombatAreaData existing;
    if (!combatAreaEditable(name, false, existing, result))
        return false;
    if (engineCombatArea(name, existing))
    {
        result = "a combat area named '" + name + "' exists already";
        return false;
    }
    LevelObjectFile::CombatAreaData area = m_copiedArea;
    area.name = name;
    HistoryEntry entry;
    entry.label = "Paste combat area " + name + " (a copy of " + m_copiedArea.name + ")";
    entry.doScript = defineAreaScript(area) + syncLine(name);
    entry.undoScript = removeAreaScript(name) + syncLine(name);
    if (!m_history.execute(entry, result))
        return false;
    m_activeCombatArea = name;
    m_activeCombatPoint = -1;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

bool EditSession::createCombatAreaHere(const std::string& name, float size, std::string& result)
{
    float at[3];
    pointAtViewCentre(at);
    return createCombatArea(name, makeVec3(at[0], at[1], at[2]), size, result);
}

bool EditSession::deleteCombatArea(const std::string& name, std::string& result)
{
    LevelObjectFile::CombatAreaData before;
    if (!combatAreaEditable(name, true, before, result))
        return false;
    HistoryEntry entry;
    entry.label = "Delete combat area " + before.name;
    entry.doScript = removeAreaScript(before.name) + syncLine(before.name);
    entry.undoScript = defineAreaScript(before) + syncLine(before.name);
    if (!m_history.execute(entry, result))
        return false;
    if (_stricmp(m_activeCombatArea.c_str(), name.c_str()) == 0)
        m_activeCombatArea.clear();
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

bool EditSession::moveCombatAreaPoint(const std::string& name, int index, float x, float z, std::string& result)
{
    LevelObjectFile::CombatAreaData before;
    if (!combatAreaEditable(name, true, before, result))
        return false;
    if (index < 0 || index >= static_cast<int>(before.points.size()))
    {
        result = "combat area " + before.name + " has no point " + std::to_string(index);
        return false;
    }
    LevelObjectFile::CombatAreaData after = before;
    after.points[index] = std::make_pair(x, z);
    if (!replaceCombatArea(before, after, "Move point " + std::to_string(index) + " of combat area " + before.name, result))
        return false;
    if (_stricmp(m_activeCombatArea.c_str(), before.name.c_str()) == 0)
        m_activeCombatPoint = index;
    return true;
}

bool EditSession::insertCombatAreaPoint(const std::string& name, int index, float x, float z, std::string& result)
{
    LevelObjectFile::CombatAreaData before;
    if (!combatAreaEditable(name, true, before, result))
        return false;
    if (index < 0 || index > static_cast<int>(before.points.size()))
    {
        result = "a point goes at 0 to " + std::to_string(before.points.size()) + " in combat area " + before.name;
        return false;
    }
    LevelObjectFile::CombatAreaData after = before;
    after.points.insert(after.points.begin() + index, std::make_pair(x, z));
    if (!replaceCombatArea(before, after, "Add point " + std::to_string(index) + " to combat area " + before.name, result))
        return false;
    if (_stricmp(m_activeCombatArea.c_str(), before.name.c_str()) == 0)
        m_activeCombatPoint = index;
    return true;
}

bool EditSession::removeCombatAreaPoint(const std::string& name, int index, std::string& result)
{
    LevelObjectFile::CombatAreaData before;
    if (!combatAreaEditable(name, true, before, result))
        return false;
    if (index < 0 || index >= static_cast<int>(before.points.size()))
    {
        result = "combat area " + before.name + " has no point " + std::to_string(index);
        return false;
    }
    // The engine makes an area a polygon at its third point and never makes it a rectangle again, so
    // fewer than three points would leave a polygon the game tests as nothing.
    if (before.points.size() <= 3)
    {
        result = "combat area " + before.name + " keeps at least three points (delete the area instead)";
        return false;
    }
    LevelObjectFile::CombatAreaData after = before;
    after.points.erase(after.points.begin() + index);
    if (m_activeCombatPoint >= static_cast<int>(after.points.size()))
        m_activeCombatPoint = -1;
    return replaceCombatArea(before, after, "Remove point " + std::to_string(index) + " of combat area " + before.name,
                             result);
}

bool EditSession::setCombatAreaProperty(const std::string& name, const std::string& property, int value,
                                        std::string& result)
{
    LevelObjectFile::CombatAreaData before;
    if (!combatAreaEditable(name, true, before, result))
        return false;
    LevelObjectFile::CombatAreaData after = before;
    const std::string p = lowerCase(property);
    if (p == "team")
        after.team = value;
    else if (p == "vehicles")
        after.vehicles = value;
    else if (p == "layer")
        after.layer = value;
    else if (p == "usedbypathfinding" && (value == 0 || value == 1))
        after.usedByPathFinding = value != 0;
    else if (p == "inverted" && (value == 0 || value == 1))
        after.inverted = value != 0;
    else
    {
        result = "no combat area property '" + property + "' taking " + std::to_string(value)
               + ": team, vehicles, layer, usedByPathFinding (0/1) or inverted (0/1)";
        return false;
    }
    if (LevelObjectFile::formatCombatArea(before, "\n") == LevelObjectFile::formatCombatArea(after, "\n"))
    {
        result = "combat area " + before.name + " has that " + property + " already";
        return true;
    }
    return replaceCombatArea(before, after, "Set " + property + " of combat area " + before.name, result);
}

bool EditSession::setCombatAreaManager(const std::string& property, float value, std::string& result)
{
    size_t file = 0;
    dice::hfe::CombatAreaManager* manager = dice::hfe::CombatAreaManager::getInstance();
    if (!manager || !m_document || !m_document->isOpen() || !m_document->gameplayFile(file))
    {
        result = "no combat area manager, or the gameplay layer file is not open: refused";
        return false;
    }
    const std::string p = lowerCase(property);
    float old = 0.0f;
    std::string command;
    if (p == "use" && (value == 0.0f || value == 1.0f))
    {
        old = manager->m_useCombatAreas ? 1.0f : 0.0f;
        command = "CombatAreaManager.use ";
    }
    else if (p == "timeallowedoutside" && value >= 0.0f)
    {
        old = manager->m_defaultTimeAllowedOutside;
        command = "CombatAreaManager.timeAllowedOutside ";
    }
    else if (p == "damage" && value >= 0.0f)
    {
        old = manager->m_defaultDamage;
        command = "CombatAreaManager.damage ";
    }
    else
    {
        result = "no combat area manager setting '" + property + "' taking that value: use (0/1), timeAllowedOutside "
                 "(seconds) or damage (per second)";
        return false;
    }
    if (old == value)
    {
        result = "the combat area manager has that " + property + " already";
        return true;
    }
    const bool integral = p == "use";
    HistoryEntry entry;
    entry.label = "Set the combat areas' " + property;
    entry.doScript = command + (integral ? std::to_string(static_cast<int>(value)) : exactFloat(value))
                   + "\nEditorCombatArea.syncManager\n";
    entry.undoScript = command + (integral ? std::to_string(static_cast<int>(old)) : exactFloat(old))
                     + "\nEditorCombatArea.syncManager\n";
    if (!m_history.execute(entry, result))
        return false;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

// Comparing gameplay layers (phase C)

const std::vector<LayerSummary>& EditSession::layerSummaries()
{
    // Built again when an edit changed the open layer (the history's revision) or a layer was copied.
    const unsigned revision = m_history.revision();
    if (!m_summariesStale && revision == m_summariesRevision)
        return m_summaries;
    m_summaries.clear();
    m_summariesStale = false;
    m_summariesRevision = revision;
    if (!m_document || !m_document->isOpen())
        return m_summaries;
    size_t open = 0;
    const bool haveOpen = m_document->gameplayFile(open);
    if (haveOpen)
        m_document->inspect();          // the open layer's records as the engine holds them now
    const std::vector<std::pair<std::string, int> > layers = m_document->layersOnDisk();
    for (size_t i = 0; i < layers.size(); ++i)
    {
        const bool isOpen = haveOpen && _stricmp(layers[i].first.c_str(), m_level.gameMode().c_str()) == 0
                         && layers[i].second == m_level.layerSize();
        if (isOpen)
        {
            m_summaries.push_back(summarizeLayer(m_document->objects(open), layers[i].first, layers[i].second));
            continue;
        }
        LevelObjectFile file;
        if (m_document->readLayer(layers[i].first, layers[i].second, file))
            m_summaries.push_back(summarizeLayer(file, layers[i].first, layers[i].second));
    }
    return m_summaries;
}

bool EditSession::isOpenLayer(const std::string& gameMode, int size) const
{
    return _stricmp(gameMode.c_str(), m_level.gameMode().c_str()) == 0 && size == m_level.layerSize();
}

bool EditSession::layersOverview(std::string& result)
{
    const std::vector<LayerSummary>& layers = layerSummaries();
    if (layers.empty())
    {
        result = "no gameplay layers on disk";
        return false;
    }
    result.clear();
    char text[256];
    for (size_t l = 0; l < layers.size(); ++l)
    {
        std::snprintf(text, sizeof(text), "%s%s %d: %u control points, %u spawners, %u spawn points, combat areas",
                      isOpenLayer(layers[l].mode, layers[l].size) ? "*" : "", layers[l].mode.c_str(), layers[l].size,
                      static_cast<unsigned>(layers[l].controlPoints.size()), static_cast<unsigned>(layers[l].spawners.size()),
                      static_cast<unsigned>(layers[l].spawnPoints.size()));
        result += (result.empty() ? "" : "; ") + std::string(text);
        for (size_t a = 0; a < layers[l].combatAreas.size(); ++a)
        {
            std::snprintf(text, sizeof(text), " %s (%u points)", layers[l].combatAreas[a].name.c_str(),
                          static_cast<unsigned>(layers[l].combatAreas[a].points.size()));
            result += text;
        }
    }
    // Each control point's place, and the layers that have one there (with its team).
    const std::vector<ControlPointRow> rows = matchControlPoints(layers, kLayerMergeDistance);
    for (size_t r = 0; r < rows.size(); ++r)
    {
        result += (r == 0 ? " | " : "; ") + rows[r].label + ":";
        for (size_t l = 0; l < layers.size(); ++l)
        {
            if (rows[r].point[l] < 0)
                continue;
            std::snprintf(text, sizeof(text), " %s %d (team %d)", layers[l].mode.c_str(), layers[l].size,
                          layers[l].controlPoints[rows[r].point[l]].team);
            result += text;
        }
    }
    return true;
}

bool EditSession::setCompareLayer(const std::string& gameMode, int size, std::string& result)
{
    if (size == 0 || gameMode.empty())
    {
        m_compareLayerSet = false;
        result = "no layer shown beside the open one";
        return true;
    }
    if (isOpenLayer(gameMode, size))
    {
        result = "that is the open layer";
        return false;
    }
    LevelObjectFile file;
    if (!m_document || !m_document->readLayer(gameMode, size, file))
    {
        result = "the level has no layer " + gameMode + " " + std::to_string(size) + " on disk";
        return false;
    }
    m_compareLayer = summarizeLayer(file, gameMode, size);
    m_compareLayerSet = true;
    char text[160];
    std::snprintf(text, sizeof(text), "showing %s %d beside the open layer: %u control points, %u combat areas", gameMode.c_str(),
                  size, static_cast<unsigned>(m_compareLayer.controlPoints.size()),
                  static_cast<unsigned>(m_compareLayer.combatAreas.size()));
    result = text;
    return true;
}

const float EditSession::kLayerMergeDistance = 50.0f;

bool EditSession::listLayers(std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level document is open";
        return false;
    }
    const std::vector<std::pair<std::string, int> > layers = m_document->layersOnDisk();
    // The map description's list: a layer it lacks is one the game does not offer.
    std::vector<std::pair<std::string, int> > described;
    const bool haveDescription = m_document->describedLayers(described);
    result.clear();
    for (size_t i = 0; i < layers.size(); ++i)
    {
        const bool open = _stricmp(layers[i].first.c_str(), m_level.gameMode().c_str()) == 0
                       && layers[i].second == m_level.layerSize();
        bool listed = false;
        for (size_t k = 0; k < described.size(); ++k)
            listed = listed || (_stricmp(described[k].first.c_str(), layers[i].first.c_str()) == 0
                                && described[k].second == layers[i].second);
        result += (result.empty() ? "" : ", ") + std::string(open ? "*" : "") + layers[i].first + " "
                + std::to_string(layers[i].second) + (haveDescription && !listed ? " (not in the map description)" : "");
    }
    if (result.empty())
        result = "no gameplay layers on disk";
    return true;
}

bool EditSession::switchLayer(const std::string& gameMode, int size, std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level document is open";
        return false;
    }
    const std::vector<std::pair<std::string, int> > layers = m_document->layersOnDisk();
    bool exists = false;
    for (size_t i = 0; i < layers.size(); ++i)
        exists = exists || (_stricmp(layers[i].first.c_str(), gameMode.c_str()) == 0 && layers[i].second == size);
    if (!exists)
    {
        std::string names;
        listLayers(names);
        result = "the level has no layer " + gameMode + " " + std::to_string(size) + " (" + names + ")";
        return false;
    }
    if (_stricmp(gameMode.c_str(), m_level.gameMode().c_str()) == 0 && size == m_level.layerSize())
    {
        result = "that layer is the one open";
        return true;
    }
    if (!m_canRelaunch)
    {
        result = "switching layers starts the editor again, which a hidden or batch run does not do; open the "
                 "layer with +gameMode " + gameMode + " +layerSize " + std::to_string(size);
        return false;
    }
    m_relaunchMode = gameMode;
    m_relaunchSize = size;
    m_relaunchLevel.clear();
    result = "the editor starts again on " + gameMode + " " + std::to_string(size) + " once the changes are saved "
             "or discarded";
    logLine(LogLevel::Info, "layer: %s", result.c_str());
    return true;
}

bool EditSession::duplicateLayer(const std::string& gameMode, int size, std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level document is open";
        return false;
    }
    std::string error;
    bool listed = false;
    std::vector<std::pair<std::string, std::string> > files;
    if (!m_document->duplicateLayer(gameMode, size, files, listed, error) || !writeFilesNow("layer copy", files, error))
    {
        result = "copying the layer: " + error;
        return false;
    }
    m_summariesStale = true;
    result = "copied " + m_level.gameMode() + " " + std::to_string(m_level.layerSize()) + " as " + gameMode + " "
           + std::to_string(size) + (listed ? ", and listed it in the map description"
                                           : "; the map description could not list it (see the log)");
    logLine(LogLevel::Info, "layer: %s", result.c_str());
    return true;
}

bool EditSession::setCombatAreasShown(bool shown, std::string&)
{
    m_combatAreasShown = shown;
    return true;
}

bool EditSession::combatAreaManagerValues(bool& use, float& timeAllowedOutside, float& damage)
{
    dice::hfe::CombatAreaManager* manager = dice::hfe::CombatAreaManager::getInstance();
    if (!manager)
        return false;
    use = manager->m_useCombatAreas;
    timeAllowedOutside = manager->m_defaultTimeAllowedOutside;
    damage = manager->m_defaultDamage;
    return true;
}

bool EditSession::selectCombatArea(const std::string& name, int point, std::string& result)
{
    LevelObjectFile::CombatAreaData area;
    if (name.empty())
    {
        m_activeCombatArea.clear();
        m_activeCombatPoint = -1;
        result = "no combat area selected";
        return true;
    }
    if (!engineCombatArea(name, area))
    {
        result = "no combat area named '" + name + "' in the level";
        return false;
    }
    m_activeCombatArea = area.name;
    m_activeCombatPoint = point >= 0 && point < static_cast<int>(area.points.size()) ? point : -1;
    result = "combat area " + area.name + " selected" +
             (m_activeCombatPoint >= 0 ? ", point " + std::to_string(m_activeCombatPoint) : std::string());
    return true;
}

// Ambient sounds (phase C)

namespace
{

// A position as the stock sound templates write it (the engine's ostream form: six significant digits).
std::string soundPosition(const Vec3& p)
{
    char text[96];
    std::snprintf(text, sizeof(text), "%g/%g/%g", p.x, p.y, p.z);
    return text;
}

} // namespace

std::vector<EditSession::SoundRow> EditSession::sounds() const
{
    std::vector<SoundRow> rows;
    size_t file = 0;
    if (!m_document || !m_document->isOpen() || !m_document->ambientFile(file))
        return rows;
    const std::vector<std::string> names = m_document->objects(file).templateNamesOf("Sound");
    for (size_t i = 0; i < names.size(); ++i)
    {
        SoundRow row;
        row.name = names[i];
        row.positioned = false;
        row.radius = 0.0f;
        row.position[0] = row.position[1] = row.position[2] = 0.0f;
        const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(file, names[i]);
        for (size_t k = 0; k < lines.size(); ++k)
        {
            if (lines[k].key == "position")
                row.positioned = LevelObjectFile::parseTriple(lines[k].value, row.position);
            else if (lines[k].key == "mindistance")
                row.radius = static_cast<float>(std::atof(lines[k].value.c_str()));
            else if (lines[k].key == "soundfilename")
                row.file = unquoted(lines[k].value);
        }
        rows.push_back(row);
    }
    return rows;
}

bool EditSession::soundRow(const std::string& name, SoundRow& out) const
{
    const std::vector<SoundRow> rows = sounds();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (_stricmp(rows[i].name.c_str(), name.c_str()) == 0)
        {
            out = rows[i];
            return true;
        }
    }
    return false;
}

bool EditSession::listSounds(std::string& result)
{
    const std::vector<SoundRow> rows = sounds();
    result.clear();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        char text[160];
        if (rows[i].positioned)
            std::snprintf(text, sizeof(text), "%s at %.0f/%.0f/%.0f (%g m)", rows[i].name.c_str(), rows[i].position[0],
                          rows[i].position[1], rows[i].position[2], rows[i].radius);
        else
            std::snprintf(text, sizeof(text), "%s (everywhere)", rows[i].name.c_str());
        result += (result.empty() ? "" : "; ") + std::string(text);
    }
    if (result.empty())
        result = "no ambient sounds";
    return true;
}

bool EditSession::applySoundPosition(const std::string& name, const Vec3& position, std::string& error)
{
    size_t file = 0;
    SoundRow row;
    if (!m_document || !m_document->ambientFile(file) || !soundRow(name, row))
    {
        error = "no ambient sound named '" + name + "' in AmbientObjects.con";
        return false;
    }
    // Where the game plays it (the template's position, through the engine's own line as a template
    // edit), and the record the retail editor kept at the same place in the file's editor block.
    if (!applyTemplateProperty(row.name, "position", soundPosition(position), error))
        return false;
    LevelDocument::RecordRef record;
    if (m_document->soundRecord(file, row.name, record))
    {
        ObjectPlacement placement = m_document->objects(file).placement(record.record);
        placement.position[0] = position.x;
        placement.position[1] = position.y;
        placement.position[2] = position.z;
        m_document->setRecordPlacement(record, placement);
    }
    return true;
}

bool EditSession::moveSound(const std::string& name, const Vec3& position, std::string& result)
{
    SoundRow row;
    if (!soundRow(name, row) || !row.positioned)
    {
        result = row.name.empty() ? "no ambient sound named '" + name + "' in AmbientObjects.con"
                                  : "ambient sound " + row.name + " plays everywhere (it has no position)";
        return false;
    }
    const Vec3 old = makeVec3(row.position[0], row.position[1], row.position[2]);
    HistoryEntry entry;
    entry.label = "Move ambient sound " + row.name;
    entry.doScript = "EditorSound.apply " + row.name + " " + soundPosition(position) + "\n";
    entry.undoScript = "EditorSound.apply " + row.name + " " + soundPosition(old) + "\n";
    if (!m_history.execute(entry, result))
        return false;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

bool EditSession::removeSound(const std::string& name, std::string& error)
{
    size_t file = 0;
    SoundRow row;
    if (!m_document || !m_document->ambientFile(file) || !soundRow(name, row))
    {
        error = "no ambient sound named '" + name + "' in AmbientObjects.con";
        return false;
    }
    // Kept, so an undo after a save (which takes the blocks out of the file) writes them again.
    RemovedSound removed;
    const std::vector<LevelObjectFile::TemplateLine> lines = m_document->templateLines(file, row.name);
    for (size_t i = 0; i < lines.size(); ++i)
        removed.lines.push_back(std::make_pair(lines[i].property, lines[i].value));
    removed.trigger = m_document->objects(file).templateHasTrigger(row.name);
    LevelDocument::RecordRef record;
    removed.hasRecord = m_document->soundRecord(file, row.name, record);
    if (removed.hasRecord)
    {
        removed.placement = m_document->objects(file).placement(record.record);
        m_document->setRecordDeleted(record, true);
    }
    m_removedSounds[lowerCase(row.name)] = removed;
    m_document->setTemplateDeleted(file, row.name, true);
    if (_stricmp(m_activeSound.c_str(), row.name.c_str()) == 0)
        m_activeSound.clear();
    return true;
}

bool EditSession::restoreSound(const std::string& name, std::string& error)
{
    size_t file = 0;
    if (!m_document || !m_document->ambientFile(file))
    {
        error = "AmbientObjects.con is not open";
        return false;
    }
    std::string className;
    size_t definedIn = 0;
    const std::map<std::string, RemovedSound>::const_iterator removed = m_removedSounds.find(lowerCase(name));
    if (m_document->templateFile(name, definedIn, className) && definedIn == file)
    {
        // Still in the file (no save since): back in.
        m_document->setTemplateDeleted(file, name, false);
        LevelDocument::RecordRef record;
        if (m_document->soundRecord(file, name, record))
            m_document->setRecordDeleted(record, false);
        return true;
    }
    if (removed == m_removedSounds.end())
    {
        error = "the editor has no copy of ambient sound '" + name + "' to put back";
        return false;
    }
    m_document->addTemplate(file, "Sound", name, removed->second.lines, removed->second.trigger);
    if (removed->second.hasRecord)
        m_document->addRecord(file, 0, removed->second.placement);
    return true;
}

bool EditSession::deleteSound(const std::string& name, std::string& result)
{
    SoundRow row;
    if (!soundRow(name, row))
    {
        result = "no ambient sound named '" + name + "' in AmbientObjects.con";
        return false;
    }
    HistoryEntry entry;
    entry.label = "Delete ambient sound " + row.name;
    entry.doScript = "EditorSound.remove " + row.name + "\n";
    entry.undoScript = "EditorSound.restore " + row.name + "\n";
    if (!m_history.execute(entry, result))
        return false;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

bool EditSession::createSound(const std::string& name, const std::string& soundFile, const Vec3& position,
                              std::string& result)
{
    size_t file = 0;
    if (!m_document || !m_document->isOpen() || !m_document->ambientFile(file))
    {
        result = "the level's AmbientObjects.con is not open, so a new ambient sound could not be saved: refused";
        return false;
    }
    if (name.empty() || name.find_first_of(" \t/\\\"") != std::string::npos)
    {
        result = "'" + name + "' is not a template name";
        return false;
    }
    size_t definedIn = 0;
    std::string definedClass;
    if (m_document->templateFile(name, definedIn, definedClass)
        || (dice::hfe::world::objectTemplateManager && dice::hfe::world::objectTemplateManager->getTemplate(name)))
    {
        result = "a template named '" + name + "' exists already; a new ambient sound needs a name of its own";
        return false;
    }
    if (soundFile.empty() || soundFile.find('"') != std::string::npos)
    {
        result = "a new ambient sound needs its sound file (common/sound/levelambients/..., an .ogg)";
        return false;
    }
    // The lines of a stock positioned ambient sound (Strike_at_Karkand's S_Gas_Station_1 and its like),
    // minus modifiedByUser; [CHOSEN] a 20 m reach.
    typedef std::vector<std::pair<std::string, std::string> > Lines;
    Lines lines;
    lines.push_back(std::make_pair(std::string("isNotSaveable"), std::string("1")));
    lines.push_back(std::make_pair(std::string("lowSamples"), std::string("2147483647")));
    lines.push_back(std::make_pair(std::string("mediumSamples"), std::string("2147483647")));
    lines.push_back(std::make_pair(std::string("soundFilename"), "\"" + soundFile + "\""));
    lines.push_back(std::make_pair(std::string("loopCount"), std::string("0")));
    lines.push_back(std::make_pair(std::string("is3dSound"), std::string("0")));
    lines.push_back(std::make_pair(std::string("stopType"), std::string("0")));
    lines.push_back(std::make_pair(std::string("volume"), std::string("1")));
    lines.push_back(std::make_pair(std::string("pitch"), std::string("1")));
    lines.push_back(std::make_pair(std::string("pan"), std::string("0.5")));
    lines.push_back(std::make_pair(std::string("reverbLevel"), std::string("0")));
    lines.push_back(std::make_pair(std::string("position"), soundPosition(position)));
    lines.push_back(std::make_pair(std::string("minDistance"), std::string("20")));
    // The engine's template, so its lines can be edited like the stock sounds'. No Sound.addTrigger
    // runs: the editor plays no sound (+noSound), and the file carries the line for the game.
    std::string failed;
    std::string answer = engineLine("ObjectTemplate.create Sound " + name);
    if (!dice::hfe::world::objectTemplateManager || !dice::hfe::world::objectTemplateManager->getTemplate(name))
        failed = "ObjectTemplate.create Sound " + name + " made no template (" + answer + ")";
    if (failed.empty())
    {
        answer = engineLine("ObjectTemplate.activeSafe Sound " + name);
        if (!answer.empty())
            failed = "ObjectTemplate.activeSafe answered: " + answer;
    }
    for (size_t i = 0; i < lines.size() && failed.empty(); ++i)
    {
        const std::string line = "ObjectTemplate." + lines[i].first + " " + unquoted(lines[i].second);
        answer = engineLine(line);
        if (!answer.empty())
            failed = line + " answered: " + answer;
    }
    if (!failed.empty())
    {
        result = "the new ambient sound's template: " + failed + "; nothing was written";
        return false;
    }
    ObjectPlacement placement;
    placement.templateName = name;
    placement.position[0] = position.x;
    placement.position[1] = position.y;
    placement.position[2] = position.z;
    const int fileLayer = m_document->fileLayer(file);
    placement.layer = fileLayer >= 0 ? fileLayer : 1;
    placement.hasLayer = true;
    m_document->addTemplate(file, "Sound", name, lines, true);
    m_document->addRecord(file, 0, placement);
    HistoryEntry entry;
    entry.label = "Create ambient sound " + name;
    entry.doScript = "EditorSound.restore " + name + "\n";
    entry.undoScript = "EditorSound.remove " + name + "\n";
    m_history.record(entry);
    m_activeSound = name;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + " (" + m_document->fileName(file) + ")";
    return true;
}

bool EditSession::createSoundHere(const std::string& name, const std::string& soundFile, std::string& result)
{
    float at[3];
    pointAtViewCentre(at);
    return createSound(name, soundFile, makeVec3(at[0], at[1], at[2]), result);
}

bool EditSession::selectSound(const std::string& name, std::string& result)
{
    if (name.empty())
    {
        m_activeSound.clear();
        result = "no ambient sound selected";
        return true;
    }
    SoundRow row;
    if (!soundRow(name, row))
    {
        result = "no ambient sound named '" + name + "' in AmbientObjects.con";
        return false;
    }
    m_activeSound = row.name;
    result = "ambient sound " + row.name + " selected";
    return true;
}

bool EditSession::soundInfo(const std::string& name, std::string& result)
{
    SoundRow row;
    if (!soundRow(name, row))
    {
        result = "no ambient sound named '" + name + "' in AmbientObjects.con";
        return false;
    }
    return templateInfo(row.name, result);
}

// Level settings (phase D)

bool EditSession::levelSetting(const std::string& key, std::string& value, std::string& error) const
{
    const SettingSpec* spec = findLevelSetting(key);
    if (!spec)
    {
        error = "no level setting '" + key + "' (EditorLevelSettings.list names them)";
        return false;
    }
    LevelSettingsFile* file = m_document ? m_document->settingsFile(spec->file) : 0;
    if (!file)
    {
        error = std::string("the level has no ") + spec->file + " the editor can write";
        return false;
    }
    std::string fileValue;
    if (!file->value(spec->command, spec->index, spec->branch, fileValue))
    {
        error = std::string(spec->file) + " does not set " + spec->command + (spec->index[0] ? " " : "") + spec->index;
        return false;
    }
    value = settingFromFile(*spec, fileValue);
    return true;
}

bool EditSession::applyLevelSetting(const std::string& key, const std::string& value, std::string& error)
{
    const SettingSpec* spec = findLevelSetting(key);
    LevelSettingsFile* file = spec && m_document ? m_document->settingsFile(spec->file) : 0;
    if (!spec || !file)
    {
        error = !spec ? "no level setting '" + key + "'" : std::string("the level has no ") + spec->file;
        return false;
    }
    if (!validSetting(*spec, value, error))
        return false;
    const std::string fileValue = settingToFile(*spec, value);
    // The view first, with the engine's own line; the file only once the engine took it.
    if (spec->live)
    {
        const std::string line = std::string(spec->command) + (spec->index[0] ? " " : "") + spec->index + " " + fileValue;
        const std::string answer = engineLine(line);
        if (!answer.empty())
        {
            error = line + " answered: " + answer;
            return false;
        }
    }
    file->setValue(spec->command, spec->index, spec->branch, fileValue);
    // The editor-branch twin, where the file has one, keeps the same value.
    std::string twin;
    if (spec->twin && file->value(spec->twin, spec->index, LevelSettingsFile::EditorBranch, twin))
        file->setValue(spec->twin, spec->index, LevelSettingsFile::EditorBranch, fileValue);
    return true;
}

bool EditSession::unsetLevelSetting(const std::string& key, std::string& error)
{
    const SettingSpec* spec = findLevelSetting(key);
    LevelSettingsFile* file = spec && m_document ? m_document->settingsFile(spec->file) : 0;
    if (!spec || !file)
    {
        error = !spec ? "no level setting '" + key + "'" : std::string("the level has no ") + spec->file;
        return false;
    }
    if (!file->removeAdded(spec->command, spec->index, spec->branch))
    {
        error = std::string(spec->file) + "'s own " + spec->command + " line is not taken out; set a value instead";
        return false;
    }
    return true;
}

bool EditSession::setLevelSetting(const std::string& key, const std::string& value, std::string& result)
{
    const SettingSpec* spec = findLevelSetting(key);
    if (!spec)
    {
        result = "no level setting '" + key + "' (EditorLevelSettings.list names them)";
        return false;
    }
    if (!m_document || !m_document->settingsFile(spec->file))
    {
        result = std::string("the level has no ") + spec->file + " the editor can write: refused";
        return false;
    }
    if (!validSetting(*spec, value, result))
        return false;
    std::string old, ignored;
    const bool had = levelSetting(key, old, ignored);
    if (had && settingToFile(*spec, old) == settingToFile(*spec, value))
    {
        result = "the level has that " + std::string(spec->label) + " already";
        return true;
    }
    HistoryEntry entry;
    entry.label = std::string("Set ") + spec->label;
    entry.doScript = "EditorLevelSettings.apply " + std::string(spec->key) + " \"" + value + "\"\n";
    entry.undoScript = had ? "EditorLevelSettings.apply " + std::string(spec->key) + " \"" + old + "\"\n"
                           : "EditorLevelSettings.unset " + std::string(spec->key) + "\n";
    if (!m_history.execute(entry, result))
        return false;
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + " (" + spec->file + ")";
    return true;
}

bool EditSession::listLevelSettings(const std::string& group, std::string& result)
{
    const std::vector<SettingSpec>& specs = levelSettingSpecs();
    result.clear();
    for (size_t i = 0; i < specs.size(); ++i)
    {
        if (!group.empty() && _stricmp(group.c_str(), "all") != 0 && _stricmp(group.c_str(), specs[i].group) != 0)
            continue;
        std::string value, error;
        const bool had = levelSetting(specs[i].key, value, error);
        result += (result.empty() ? "" : "; ") + std::string(specs[i].key) + " = " + (had ? value : std::string("(not set)"));
    }
    if (result.empty())
        result = "no level settings in '" + group + "' (groups: Teams, Kits, Tickets, View, Light, Fog and sky, Water)";
    return true;
}

bool EditSession::spawnCameraFromView(std::string& result)
{
    char value[128];
    std::snprintf(value, sizeof(value), "%g/%g/%g %g/%g/0", m_camera.x(), m_camera.y(), m_camera.z(),
                  m_camera.yawDegrees(), m_camera.pitchDegrees());
    return setLevelSetting("spawnCamera", value, result);
}

bool EditSession::groundHeight(float x, float z, float& height) const
{
    return m_level.groundHeight(x, z, height);
}

bool EditSession::surfaceAt(float x, float y, float out[3], std::string& detail, bool ignoreSelection) const
{
    if (m_width <= 0 || m_height <= 0 || !dice::hfe::world::objectManager)
    {
        detail = "no view to place in";
        return false;
    }
    float origin[3], direction[3];
    m_camera.rayThroughPixel(x, y, m_width, m_height, origin, direction);
    const float reach = m_camera.farPlane();
    // The collision surface first (pick()'s line test and filter), then the terrain up to it.
    PickPredicator predicator(*this, ignoreSelection);
    IObject* hit = 0;
    Vec3 point = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
    int part = 0;
    const Vec3 from = makeVec3(origin[0], origin[1], origin[2]);
    const Vec3 delta = makeVec3(direction[0] * reach, direction[1] * reach, direction[2] * reach);
    float objectDistance = FLT_MAX;
    if (dice::hfe::world::objectManager->intersectLine(hit, point, normal, part, from, delta, &predicator, true, false,
                                                       false, false, kExcludeNothing) && hit)
    {
        const float dx = point.x - origin[0], dy = point.y - origin[1], dz = point.z - origin[2];
        objectDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    const float ground = terrainDistance(origin, direction, objectDistance < reach ? objectDistance : reach);
    if (ground >= 0.0f && ground < objectDistance)
    {
        for (int i = 0; i < 3; ++i)
            out[i] = origin[i] + direction[i] * ground;
        detail = "on the terrain";
        return true;
    }
    if (objectDistance < FLT_MAX)
    {
        out[0] = point.x;
        out[1] = point.y;
        out[2] = point.z;
        IObject* root = dice::hfe::world::getRootParent(hit);
        detail = "on " + describe((root ? root : hit)->getObjectId());
        return true;
    }
    detail = "the view meets neither the terrain nor an object there";
    return false;
}

bool EditSession::placeObjectAt(const std::string& templateName, float x, float y, std::string& result)
{
    float at[3];
    std::string where;
    if (!surfaceAt(x, y, at, where))
    {
        result = "nothing to place " + templateName + " on: " + where;
        return false;
    }
    if (!createObjectOn(templateName, makeVec3(at[0], at[1], at[2]), makeVec3(0.0f, 0.0f, 0.0f), 1, &at[1], result))
        return false;
    result += " " + where;
    return true;
}

bool EditSession::copyable(unsigned id, std::string& error) const
{
    LevelDocument::RecordRef record;
    IObject* o = object(id);
    if (!o || !o->getTemplate())
    {
        error = describe(id) + " cannot be copied";
        return false;
    }
    if (!m_document || !m_document->isOpen())
    {
        error = "the level's objects file is not open, so copies could not be saved: refused";
        return false;
    }
    if (!m_document->recordOf(id, record))
    {
        error = describe(id) + " belongs to no file this editor writes, so a copy could not be saved: refused";
        return false;
    }
    if (!m_document->takesNewObjects(record.file))
    {
        error = describe(id) + " belongs to " + m_document->fileName(record.file)
              + ", which defines a template for each of its objects; only StaticObjects.con's objects are copied or "
                "replaced (a new gameplay object is made with EditorGameplay.create): refused";
        return false;
    }
    return true;
}

bool EditSession::makeObjects(const std::vector<NewObject>& objects, std::vector<unsigned>& made, std::string& error)
{
    made.clear();
    if (!m_document || !m_document->isOpen())
    {
        error = "the level's objects file is not open, so new objects could not be saved: refused";
        return false;
    }
    for (size_t i = 0; i < objects.size(); ++i)
    {
        unsigned id = 0;
        if (!makeObject(objects[i].templateName, objects[i].transform, objects[i].layer, id, error))
        {
            // Half made is not made: the objects made so far go again.
            for (size_t k = made.size(); k-- > 0;)
            {
                std::string ignored;
                removeObject(made[k], ignored);
            }
            made.clear();
            return false;
        }
        IObject* o = object(id);
        const std::string name = o && o->getTemplate() ? o->getTemplate()->getName() : objects[i].templateName;
        ObjectPlacement placement = placementFor(name, objects[i].transform, objects[i].layer);
        if (objects[i].hasProperties && o)
        {
            // A copy takes the original's properties, through the engine's own lines.
            const ObjectProperties& p = objects[i].properties;
            runConsoleScript(propertiesScript(id, p), error);
            placement.group = p.group;
            placement.disableChildren = p.disableChildren;
            placement.notInAI = p.notInAI;
            placement.isOvergrowth = p.isOvergrowth;
            placement.lightSourceMask = p.lightSourceMask;
            placement.hasLightSourceMask = objects[i].hasLightSourceMask;
        }
        m_document->addRecord(0, id, placement);
        made.push_back(id);
    }
    return true;
}

namespace
{

// The history's replays of objects made by the editor: remaking them (redo) and removing them (undo).
void appendMadeScripts(const std::vector<unsigned>& made, std::string& doScript, std::string& undoScript)
{
    std::vector<std::string> removals;
    for (size_t i = 0; i < made.size(); ++i)
    {
        IObject* o = EditSession::object(made[i]);
        if (!o || !o->getTemplate())
            continue;
        doScript += recreateScript(made[i], o->getTemplate()->getName(), o->getAbsoluteTransformation(),
                                   static_cast<int>(o->getLayer()), EditSession::propertiesOf(o));
        char line[48];
        std::snprintf(line, sizeof(line), "EditorObject.remove %u\n", made[i]);
        removals.push_back(line);
    }
    for (size_t i = removals.size(); i-- > 0;)
        undoScript += removals[i];
}

std::string countLabel(const char* verb, size_t count, const char* noun)
{
    char text[96];
    std::snprintf(text, sizeof(text), "%s %u %s", verb, static_cast<unsigned>(count), noun);
    return text;
}

} // namespace

void EditSession::madeScripts(const std::vector<unsigned>& made, std::string& doScript, std::string& undoScript)
{
    appendMadeScripts(made, doScript, undoScript);
}

std::string EditSession::recreateLine(unsigned id, const std::string& templateName, const Mat4& m, int layer,
                                      const ObjectProperties& properties)
{
    return recreateScript(id, templateName, m, layer, properties);
}

bool EditSession::setTransformNow(unsigned id, const Mat4& m, std::string& error)
{
    return runConsoleScript(transformScript(id, m), error);
}

EditSession::NewObject EditSession::copyOf(unsigned id) const
{
    NewObject copy;
    IObject* o = object(id);
    if (!o || !o->getTemplate())
        return copy;
    copy.templateName = o->getTemplate()->getName();
    copy.transform = o->getAbsoluteTransformation();
    copy.layer = static_cast<int>(o->getLayer());
    copy.hasProperties = true;
    copy.properties = propertiesOf(o);
    ObjectPlacement written;
    copy.hasLightSourceMask = m_document && m_document->placementOf(id, written) && written.hasLightSourceMask;
    return copy;
}

bool EditSession::duplicateSelection(std::string& result)
{
    if (m_selection.empty())
    {
        result = "nothing is selected";
        return false;
    }
    // Every object first: a copy goes into StaticObjects.con, so only objects of that file are copied.
    const std::vector<unsigned> originals = m_selection.ids();
    std::vector<NewObject> copies;
    for (size_t i = 0; i < originals.size(); ++i)
    {
        if (!copyable(originals[i], result))
            return false;
        copies.push_back(copyOf(originals[i]));
    }
    std::vector<unsigned> made;
    std::string error;
    if (!makeObjects(copies, made, error))
    {
        result = "copying: " + error;
        return false;
    }
    HistoryEntry entry;
    appendMadeScripts(made, entry.doScript, entry.undoScript);
    entry.label = made.size() == 1 ? "Duplicate " + describe(originals[0]) : countLabel("Duplicate", made.size(), "objects");
    // Made already (their ids were needed for the undo), so the history records the copy as done.
    m_history.record(entry);
    m_selection.clear();
    for (size_t i = 0; i < made.size(); ++i)
        m_selection.add(made[i]);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label + " -> " + describe(made.back());
    return true;
}

bool EditSession::selectInBox(float x0, float y0, float x1, float y1, int mode, std::string& result)
{
    if (mode < 0 || mode > 2)
    {
        result = "the box mode is 0 (replace), 1 (add) or 2 (toggle)";
        return false;
    }
    if (m_width <= 0 || m_height <= 0 || !dice::hfe::world::objectManager)
    {
        result = "no view to select in";
        return false;
    }
    if (x0 > x1)
        std::swap(x0, x1);
    if (y0 > y1)
        std::swap(y0, y1);
    // Every pickable root object whose position lies in the rectangle, in front of the camera and no
    // farther than it draws.
    std::vector<unsigned> inside;
    const float reach = m_camera.farPlane();
    const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
    {
        IObject* o = it->second;
        if (!o || !canPick(o))
            continue;
        const Mat4& m = o->getAbsoluteTransformation();
        const float p[3] = { m.m[12], m.m[13], m.m[14] };
        const float dx = p[0] - m_camera.x(), dy = p[1] - m_camera.y(), dz = p[2] - m_camera.z();
        if (dx * dx + dy * dy + dz * dz > reach * reach)
            continue;
        float px = 0.0f, py = 0.0f;
        if (m_camera.worldToPixel(p, m_width, m_height, px, py) && px >= x0 && px <= x1 && py >= y0 && py <= y1)
            inside.push_back(it->first);
    }
    if (mode == 0)
        m_selection.clear();
    for (size_t i = 0; i < inside.size(); ++i)
    {
        if (mode == 2)
            m_selection.toggle(inside[i]);
        else
            m_selection.add(inside[i]);
    }
    char text[96];
    std::snprintf(text, sizeof(text), "%u objects in the box; %u selected", static_cast<unsigned>(inside.size()),
                  static_cast<unsigned>(m_selection.size()));
    result = text;
    return true;
}

bool EditSession::replaceSelection(const std::string& templateName, std::string& result)
{
    if (m_selection.empty())
    {
        result = "nothing is selected";
        return false;
    }
    // Every object first: each is deleted from StaticObjects.con and its replacement made there.
    const std::vector<unsigned> originals = m_selection.ids();
    std::vector<NewObject> replacements;
    std::string undoRemake;
    for (size_t i = 0; i < originals.size(); ++i)
    {
        IObject* o = object(originals[i]);
        if (!o || !o->getTemplate())
        {
            result = describe(originals[i]) + " cannot be replaced";
            return false;
        }
        // Only StaticObjects.con's objects: one with a template of its own (a trigger, a spawner) would
        // lose its template and gameplay to a static object.
        if (!copyable(originals[i], result) || !deletable(originals[i], result))
            return false;
        NewObject replacement;
        replacement.templateName = templateName;
        replacement.transform = o->getAbsoluteTransformation();
        replacement.layer = static_cast<int>(o->getLayer());
        replacements.push_back(replacement);
        undoRemake += recreateScript(originals[i], o->getTemplate()->getName(), replacement.transform, replacement.layer,
                                     propertiesOf(o));
    }
    // The new objects first: an unknown template fails before anything is removed.
    std::vector<unsigned> made;
    std::string error;
    if (!makeObjects(replacements, made, error))
    {
        result = "replacing with " + templateName + ": " + error;
        return false;
    }
    HistoryEntry entry;
    char line[48];
    for (size_t i = 0; i < originals.size(); ++i)
    {
        std::snprintf(line, sizeof(line), "EditorObject.remove %u\n", originals[i]);
        entry.doScript += line;
    }
    std::string remakeNew, removeNew;
    appendMadeScripts(made, remakeNew, removeNew);
    entry.doScript += remakeNew;
    entry.undoScript = removeNew + undoRemake;
    const std::string replacedName = originals.size() == 1 ? describe(originals[0])
                                                           : countLabel("", originals.size(), "objects").substr(1);
    for (size_t i = 0; i < originals.size(); ++i)
    {
        if (!removeObject(originals[i], error))
            logLine(LogLevel::Error, "replace: %s", error.c_str());
    }
    IObject* first = object(made[0]);
    entry.label = "Replace " + replacedName + " with "
                + (first && first->getTemplate() ? first->getTemplate()->getName() : templateName);
    m_history.record(entry);
    m_selection.clear();
    for (size_t i = 0; i < made.size(); ++i)
        m_selection.add(made[i]);
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

bool EditSession::copyTransform(std::string& result)
{
    const unsigned id = m_selection.primary();
    if (!transformOf(id, m_copiedTransform))
    {
        result = "nothing is selected";
        return false;
    }
    m_haveCopiedTransform = true;
    const Vec3 angles = rotationOf(m_copiedTransform);
    const float position[3] = { m_copiedTransform.m[12], m_copiedTransform.m[13], m_copiedTransform.m[14] };
    const float rotation[3] = { angles.x, angles.y, angles.z };
    result = "copied the transformation of " + describe(id) + ": " + LevelObjectFile::formatPosition(position)
           + " rotation " + LevelObjectFile::formatRotation(rotation);
    return true;
}

bool EditSession::pasteTransform(std::string& result)
{
    if (!m_haveCopiedTransform)
    {
        result = "no transformation has been copied";
        return false;
    }
    std::vector<TransformChange> changes;
    for (size_t i = 0; i < m_selection.ids().size(); ++i)
    {
        TransformChange change;
        change.id = m_selection.ids()[i];
        if (!transformOf(change.id, change.before))
            continue;
        // The copied position and rotation; each object keeps its own scale.
        change.after = m_copiedTransform;
        for (int row = 0; row < 3; ++row)
        {
            const float from = rowLength(m_copiedTransform, row);
            const float to = rowLength(change.before, row);
            if (from > 0.0f)
                for (int column = 0; column < 3; ++column)
                    change.after.m[row * 4 + column] *= to / from;
        }
        if (std::memcmp(change.after.m, change.before.m, sizeof(change.after.m)) != 0)
            changes.push_back(change);
    }
    if (changes.empty())
    {
        result = m_selection.empty() ? "nothing is selected" : "the selection has that transformation already";
        return !m_selection.empty();
    }
    const std::string label = labelFor("Paste the transformation onto");
    if (!applyTransforms(changes, label, result))
        return false;
    result = label;
    return true;
}

bool EditSession::snapSelectionToGrid(float step, std::string& result)
{
    if (!(step > 0.0f))
    {
        result = "the grid step is above 0 metres";
        return false;
    }
    if (m_selection.empty())
    {
        result = "nothing is selected";
        return false;
    }
    // X and Z only: the height stays where the object stands.
    std::vector<TransformChange> changes;
    for (size_t i = 0; i < m_selection.ids().size(); ++i)
    {
        TransformChange change;
        change.id = m_selection.ids()[i];
        if (!transformOf(change.id, change.before))
            continue;
        change.after = change.before;
        change.after.m[12] = std::floor(change.before.m[12] / step + 0.5f) * step;
        change.after.m[14] = std::floor(change.before.m[14] / step + 0.5f) * step;
        if (change.after.m[12] != change.before.m[12] || change.after.m[14] != change.before.m[14])
            changes.push_back(change);
    }
    if (changes.empty())
    {
        result = "the selection is on the grid already";
        return true;
    }
    char grid[48];
    std::snprintf(grid, sizeof(grid), " to the %g m grid", step);
    const std::string label = (changes.size() == 1 ? "Snap " + describe(changes[0].id) : labelFor("Snap")) + grid;
    if (!applyTransforms(changes, label, result))
        return false;
    result = label;
    return true;
}

bool EditSession::copySelection(std::string& result)
{
    if (m_selection.empty())
    {
        result = "nothing is selected";
        return false;
    }
    Mat4 primary;
    if (!transformOf(m_selection.primary(), primary))
    {
        result = "nothing is selected";
        return false;
    }
    std::vector<NewObject> clipboard;
    for (size_t i = 0; i < m_selection.ids().size(); ++i)
    {
        const unsigned id = m_selection.ids()[i];
        if (!copyable(id, result))
            return false;
        NewObject item = copyOf(id);
        for (int k = 0; k < 3; ++k)
            item.transform.m[12 + k] -= primary.m[12 + k];
        // The primary goes first, so a paste lands it on the paste point.
        if (id == m_selection.primary())
            clipboard.insert(clipboard.begin(), item);
        else
            clipboard.push_back(item);
    }
    m_clipboard = clipboard;
    m_lastOffsetArmed = false;                      // a Copy replaces a last-offset copy, as retail's record list did
    for (int k = 0; k < 3; ++k)
        m_clipboardOrigin[k] = primary.m[12 + k];
    result = m_clipboard.size() == 1 ? "copied " + m_clipboard[0].templateName
                                     : countLabel("copied", m_clipboard.size(), "objects");
    return true;
}

bool EditSession::cutSelection(std::string& result)
{
    std::vector<NewObject> previous = m_clipboard;
    if (!copySelection(result))
        return false;
    if (!deleteSelection(result))
    {
        m_clipboard = previous;         // nothing was cut, so the clipboard stays as it was
        return false;
    }
    result = "cut: " + result;
    return true;
}

bool EditSession::pasteClipboard(const Vec3& at, bool atGiven, std::string& result)
{
    if (m_lastOffsetArmed && !atGiven)
        return pasteLastOffset(result);
    if (m_clipboard.empty())
    {
        result = "the clipboard is empty";
        return false;
    }
    float point[3] = { at.x, at.y, at.z };
    if (!atGiven)
        pointAtViewCentre(point);
    std::vector<NewObject> objects = m_clipboard;
    for (size_t i = 0; i < objects.size(); ++i)
        for (int k = 0; k < 3; ++k)
            objects[i].transform.m[12 + k] += point[k];
    std::vector<unsigned> made;
    std::string error;
    if (!makeObjects(objects, made, error))
    {
        result = "pasting: " + error;
        return false;
    }
    HistoryEntry entry;
    appendMadeScripts(made, entry.doScript, entry.undoScript);
    entry.label = made.size() == 1 ? "Paste " + describe(made[0]) : countLabel("Paste", made.size(), "objects");
    m_history.record(entry);
    m_selection.clear();
    for (size_t i = made.size(); i-- > 0;)
        m_selection.add(made[i]);           // the first pasted (the copied primary) becomes the primary
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    result = entry.label;
    return true;
}

bool EditSession::snapSelectionToGround(std::string& result)
{
    if (m_selection.empty())
    {
        result = "nothing is selected";
        return false;
    }
    std::vector<TransformChange> changes;
    std::vector<unsigned> offMap;
    for (size_t i = 0; i < m_selection.ids().size(); ++i)
    {
        TransformChange change;
        change.id = m_selection.ids()[i];
        if (!transformOf(change.id, change.before))
            continue;
        // The retail snap (EditorEngine 0x100333A0): the terrain under the object's anchor, and the pivot
        // at that height less the anchor's (or the bounding box's lowest point); X and Z are kept.
        float anchor[3] = { change.before.m[12], change.before.m[13], change.before.m[14] };
        worldAnchorOf(change.id, anchor);
        float ground = 0.0f;
        if (!m_level.groundHeight(anchor[0], anchor[2], ground))
        {
            offMap.push_back(change.id);
            continue;
        }
        const float y = snapPivotHeight(object(change.id), ground);
        if (change.before.m[13] == y)
            continue;                               // on the ground already
        change.after = change.before;
        change.after.m[13] = y;
        changes.push_back(change);
    }
    if (!offMap.empty())
    {
        result = "not over the terrain: " + objectList(offMap) + "; nothing was moved";
        return false;
    }
    if (changes.empty())
    {
        result = "the selection is on the ground already";
        return true;
    }
    const std::string label = changes.size() == 1 ? "Snap " + describe(changes[0].id) + " to the ground"
                                                   : labelFor("Snap") + " to the ground";
    if (!applyTransforms(changes, label, result))
        return false;
    result = label;
    return true;
}

// Layers and filters

namespace
{

bool validLayer(int layer, std::string& error)
{
    if (layer >= 0 && layer <= 255)
        return true;
    error = "a layer is 0 to 255 (IObject keeps it in one byte)";
    return false;
}

std::string layerLabel(const char* verb, int layer)
{
    char text[64];
    std::snprintf(text, sizeof(text), "%s %d", verb, layer);
    return text;
}

} // namespace

std::string EditSession::className(unsigned classId)
{
    if (!dice::hfe::world::objectTemplateManager)
        return "UnknownTemplateClass";
    return dice::hfe::world::objectTemplateManager->getClassNameFromTemplateClassID(classId);
}

int EditSession::layerOf(IObject* object)
{
    IObject* root = object ? dice::hfe::world::getRootParent(object) : 0;
    if (!root)
        root = object;
    return root ? static_cast<int>(root->getLayer()) : 0;
}

bool EditSession::canPick(IObject* root) const
{
    if (!root || (root->getFlags() & IObject::OFIsVisible) == 0)
        return false;
    if (m_preview && root->getObjectId() == m_preview)
        return false;                               // the template preview is the viewer's, not the level's
    if (isLayerLocked(static_cast<int>(root->getLayer())))
        return false;
    const dice::hfe::world::IObjectTemplate* objectTemplate = root->getTemplate();
    return !objectTemplate || isClassPickable(objectTemplate->getClassID());
}

void EditSession::setClassPickable(unsigned classId, bool pickable)
{
    if (pickable)
        m_unpickableClasses.erase(classId);
    else
        m_unpickableClasses.insert(classId);
    m_countsStale = true;
}

unsigned EditSession::pickWithoutCollision(const float origin[3], const float direction[3], float reach,
                                           float& distance) const
{
    distance = FLT_MAX;
    if (!dice::hfe::world::objectManager)
        return 0;
    unsigned best = 0;
    // Every root object in the grid (no parent, OFHasGrid) - the no-collision cullers included, which
    // the collision ray queries never reach.
    const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
    {
        IObject* object = it->second;
        if (!object)
            continue;
        // intersectLine already tests what it can: collision objects with a collision LOD 0.
        if ((object->getFlags() & IObject::OFHasCollision) != 0 && object->hasCollisionLod(0))
            continue;
        if (!canPick(object))
            continue;
        float t = 0.0f;
        if (rayEntersBounds(object, origin, direction, t) && t <= reach && t < distance)
        {
            distance = t;
            best = it->first;
        }
    }
    return best;
}

void EditSession::recount()
{
    m_countsStale = false;
    std::map<int, int> layers;
    std::map<unsigned, int> classes;
    if (dice::hfe::world::objectManager)
    {
        const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
        for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        {
            IObject* object = it->second;
            if (!object)
                continue;
            ++layers[static_cast<int>(object->getLayer())];
            if (object->getTemplate())
                ++classes[object->getTemplate()->getClassID()];
        }
    }
    // Hidden, locked and unpickable entries stay listed even when nothing is left in them.
    for (std::set<int>::const_iterator it = m_hiddenLayers.begin(); it != m_hiddenLayers.end(); ++it)
        layers[*it] += 0;
    for (std::set<int>::const_iterator it = m_lockedLayers.begin(); it != m_lockedLayers.end(); ++it)
        layers[*it] += 0;
    for (std::set<unsigned>::const_iterator it = m_unpickableClasses.begin(); it != m_unpickableClasses.end(); ++it)
        classes[*it] += 0;

    m_layerRows.clear();
    for (std::map<int, int>::const_iterator it = layers.begin(); it != layers.end(); ++it)
    {
        LayerRow row;
        row.layer = it->first;
        row.objects = it->second;
        row.hidden = isLayerHidden(it->first);
        row.locked = isLayerLocked(it->first);
        m_layerRows.push_back(row);
    }
    m_classRows.clear();
    for (std::map<unsigned, int>::const_iterator it = classes.begin(); it != classes.end(); ++it)
    {
        ClassRow row;
        row.classId = it->first;
        row.name = className(it->first);
        row.objects = it->second;
        row.pickable = isClassPickable(it->first);
        m_classRows.push_back(row);
    }
}

const std::vector<EditSession::LayerRow>& EditSession::layerRows()
{
    if (m_countsStale)
        recount();
    return m_layerRows;
}

const std::vector<EditSession::ClassRow>& EditSession::classRows()
{
    if (m_countsStale)
        recount();
    return m_classRows;
}

void EditSession::update()
{
    updatePreview();                                // the template preview's turntable
    if (m_history.revision() == m_seenHistoryRevision)
        return;
    // An edit, an undo or a redo may have moved objects between layers.
    m_seenHistoryRevision = m_history.revision();
    m_countsStale = true;
    if (!m_hiddenLayers.empty() || !m_hiddenByEditor.empty())
        syncVisibility();
}

void EditSession::syncVisibility()
{
    if (!dice::hfe::world::objectManager)
        return;
    std::set<unsigned> seen;
    const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
    {
        IObject* object = it->second;
        if (!object)
            continue;
        seen.insert(it->first);
        const bool hide = isLayerHidden(static_cast<int>(object->getLayer()));
        const bool hiddenByEditor = m_hiddenByEditor.count(it->first) != 0;
        if (hide && !hiddenByEditor && (object->getFlags() & IObject::OFIsVisible) != 0)
        {
            // Object::updateFlags (Scene/Object/Object.cpp): bit 1 has no dispatch arm, so clearing it
            // only stops the draw; the reserved-mask abort tests the SET mask only.
            object->updateFlags(0, IObject::OFIsVisible);
            m_hiddenByEditor.insert(it->first);
            m_selection.remove(it->first);
        }
        else if (!hide && hiddenByEditor)
        {
            object->updateFlags(IObject::OFIsVisible, 0);
            m_hiddenByEditor.erase(it->first);
        }
    }
    // Forget objects that left the grid.
    for (std::set<unsigned>::iterator it = m_hiddenByEditor.begin(); it != m_hiddenByEditor.end();)
    {
        if (seen.count(*it) == 0)
            m_hiddenByEditor.erase(it++);
        else
            ++it;
    }
}

bool EditSession::setLayerHidden(int layer, bool hidden, std::string& error)
{
    if (!validLayer(layer, error))
        return false;
    if (hidden)
        m_hiddenLayers.insert(layer);
    else
        m_hiddenLayers.erase(layer);
    for (size_t i = 0; i < m_layerModel.layers().size(); ++i)
        if (m_layerModel.layers()[i].id == layer)
            m_layerModel.layers()[i].visible = !hidden;
    ++m_visibilityRevision;
    syncVisibility();
    m_countsStale = true;
    logLine(LogLevel::Info, "layers: layer %d %s", layer, hidden ? "hidden" : "shown");
    return true;
}

bool EditSession::setLayerLocked(int layer, bool locked, std::string& error)
{
    if (!validLayer(layer, error))
        return false;
    if (locked)
        m_lockedLayers.insert(layer);
    else
        m_lockedLayers.erase(layer);
    for (size_t i = 0; i < m_layerModel.layers().size(); ++i)
        if (m_layerModel.layers()[i].id == layer)
            m_layerModel.layers()[i].locked = locked;
    m_countsStale = true;
    logLine(LogLevel::Info, "layers: layer %d %s", layer, locked ? "locked" : "unlocked");
    return true;
}

bool EditSession::selectLayer(int layer, std::string& error)
{
    if (!validLayer(layer, error))
        return false;
    if (isLayerLocked(layer) || isLayerHidden(layer))
    {
        error = layerLabel(isLayerLocked(layer) ? "locked: layer" : "hidden: layer", layer);
        return false;
    }
    m_selection.clear();
    if (!dice::hfe::world::objectManager)
        return true;
    const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        if (it->second && static_cast<int>(it->second->getLayer()) == layer && canPick(it->second))
            m_selection.add(it->first);
    logLine(LogLevel::Info, "layers: %u objects of layer %d selected", static_cast<unsigned>(m_selection.size()),
            layer);
    return true;
}

bool EditSession::setSelectionLayer(int layer, std::string& error)
{
    if (!validLayer(layer, error))
        return false;
    // One undoable edit through the engine's own `Object.layer` (ObjectManagerConsole.cpp).
    HistoryEntry entry;
    std::vector<std::pair<unsigned, int> > before;
    for (size_t i = 0; i < m_selection.ids().size(); ++i)
    {
        IObject* o = object(m_selection.ids()[i]);
        if (!o || static_cast<int>(o->getLayer()) == layer)
            continue;
        before.push_back(std::make_pair(m_selection.ids()[i], static_cast<int>(o->getLayer())));
    }
    if (before.empty())
    {
        error = m_selection.empty() ? "nothing is selected" : layerLabel("already on layer", layer);
        return false;
    }
    char line[96];
    for (size_t i = 0; i < before.size(); ++i)
    {
        std::snprintf(line, sizeof(line), "EditorObject.activate %u\nObject.layer %d\n", before[i].first, layer);
        entry.doScript += line;
    }
    for (size_t i = before.size(); i-- > 0;)
    {
        std::snprintf(line, sizeof(line), "EditorObject.activate %u\nObject.layer %d\n", before[i].first,
                      before[i].second);
        entry.undoScript += line;
    }
    char label[96];
    if (before.size() == 1)
    {
        std::snprintf(label, sizeof(label), "To layer %d: ", layer);
        entry.label = label + describe(before[0].first);
    }
    else
    {
        std::snprintf(label, sizeof(label), "%u objects to layer %d", static_cast<unsigned>(before.size()), layer);
        entry.label = label;
    }
    if (!m_history.execute(entry, error))
        return false;
    for (size_t i = 0; i < before.size(); ++i)
    {
        IObject* o = object(before[i].first);
        if (!o || static_cast<int>(o->getLayer()) != layer)
        {
            error = entry.label + ": " + describe(before[i].first) + " is not on the new layer";
            return false;
        }
    }
    logLine(LogLevel::Info, "edit: %s", entry.label.c_str());
    return true;
}

} // namespace bf2editor
