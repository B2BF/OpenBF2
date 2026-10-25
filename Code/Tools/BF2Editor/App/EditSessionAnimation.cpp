// Undoable animation-system and value-holder file edits for the selected animation object.
// [CHOSEN] changes take effect after level reload; mutating a playing system would invalidate
// BundlePlayer's raw iterators and pointers. Validate assets, trigger types, nonzero lengths/ranges,
// acyclic links and definition-before-use ordering before changing text.
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "Framework/BafHeader.h"
#include "Framework/EditorLog.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SourceControl.h"

#include "Animation/BoneAnimation/AnimationSystem.h"
#include "Animation/BoneAnimation/ValueHolder.h"
#include "IO/File/FileManager.h"
#include "IO/Stream/IStream.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <set>

namespace bf2editor
{

namespace
{

// The engine's vocabulary

// The trigger factory's twelve types (TriggerManager::createTrigger compares them case-sensitively) and the
// family getType() answers, which gates the type's own console rows (Trigger.h).
struct TriggerTypeInfo
{
    const char* name;
    int         family;     // 0 Trigger, 1 Pose, 2 Idle, 3 Random, 4 Message, 5 Movement
};
const TriggerTypeInfo kTriggerTypes[] = {
    { "Trigger", 0 }, { "PoseTrigger", 1 }, { "IdleTrigger", 2 }, { "RandomTrigger", 3 }, { "MessageTrigger", 4 },
    { "SwitchMessageTrigger", 4 }, { "MovementTrigger", 5 }, { "ForwardTrigger", 5 }, { "SideTrigger", 5 },
    { "UpTrigger", 5 }, { "TurnTrigger", 5 }, { "LookAroundTrigger", 5 },
};
const int kTriggerTypeCount = static_cast<int>(sizeof(kTriggerTypes) / sizeof(kTriggerTypes[0]));

enum class ValueType { Float, PositiveFloat, Bool, Int, UInt, Vec2, Vec3, Holder, Range3 };

// The bound value rows of each kind (ModuleBoneAnimation.cpp), with the engine's default when no line sets
// one (the constructors in BoneAnimation.cpp, Bundle.cpp, Trigger.cpp, ValueHolder.cpp). `family` is the
// trigger family a trigger row needs (-1 any).
struct PropertySpec
{
    AnimationKind kind;
    const char*   name;
    ValueType     type;
    int           family;
    const char*   fallback;
};
const PropertySpec kProperties[] = {
    { AnimationKind::Animation, "length", ValueType::PositiveFloat, -1, "its .baf's" },
    { AnimationKind::Animation, "looping", ValueType::Bool, -1, "1" },
    { AnimationKind::Animation, "fadeInTime", ValueType::Float, -1, "0" },
    { AnimationKind::Animation, "ignoreMotherOrientation", ValueType::Int, -1, "-1" },
    { AnimationKind::Animation, "noiceFreq", ValueType::Int, -1, "0" },
    { AnimationKind::Animation, "noiceAmplitude", ValueType::Float, -1, "0" },
    { AnimationKind::Bundle, "isLooping", ValueType::Bool, -1, "1" },
    { AnimationKind::Bundle, "abruptPlayback", ValueType::Bool, -1, "0" },
    { AnimationKind::Bundle, "playBackward", ValueType::Bool, -1, "0" },
    { AnimationKind::Bundle, "fadeInTime", ValueType::Float, -1, "0.2" },
    { AnimationKind::Bundle, "fadeOutTime", ValueType::Float, -1, "0.1" },
    { AnimationKind::Bundle, "playForever", ValueType::Bool, -1, "0" },
    { AnimationKind::Bundle, "useSpeedAsTime", ValueType::Bool, -1, "0" },
    { AnimationKind::Bundle, "jumpToLastAnimationAtStop", ValueType::Bool, -1, "0" },
    { AnimationKind::Bundle, "length", ValueType::PositiveFloat, -1, "1" },
    { AnimationKind::Trigger, "fadeInTime", ValueType::Float, -1, "0" },
    { AnimationKind::Trigger, "idleTime", ValueType::Vec2, 2, "5/10" },
    { AnimationKind::Trigger, "message", ValueType::Int, 4, "0" },
    { AnimationKind::Trigger, "valueHolder", ValueType::Holder, 5, "none" },
    { AnimationKind::Trigger, "useDirection", ValueType::Bool, 5, "0" },
    { AnimationKind::Trigger, "triggerOnAcceleration", ValueType::Bool, 5, "0" },
    { AnimationKind::ValueHolder, "values", ValueType::Range3, -1, "0 0 0" },
    { AnimationKind::ValueHolder, "passOnMessage", ValueType::UInt, -1, "0" },
    { AnimationKind::ValueHolder, "stopOnMessage", ValueType::UInt, -1, "0" },
};

// The system's own rows: the camera spring (only while cameraSpring.use is 1, getActiveCameraSpring).
struct SystemSpec
{
    const char* name;
    ValueType   type;
};
const SystemSpec kSystemProperties[] = {
    { "cameraSpring.use", ValueType::Bool }, { "cameraSpring.amplitude", ValueType::Vec3 },
    { "cameraSpring.zoomDamping", ValueType::Vec3 }, { "cameraSpring.stiffness", ValueType::Vec3 },
    { "cameraSpring.move", ValueType::Vec3 }, { "cameraSpring.hit", ValueType::Vec3 },
    { "cameraSpring.fire", ValueType::Vec3 }, { "cameraSpring.look", ValueType::Vec2 },
};

const char kReadOnly[] = " is read from a mounted archive, which shadows any loose copy, so an edit could never be saved: "
                         "extract the Objects archives (EditorObjects.extract) and start with +looseObjects 1";

// Small helpers

std::string lower(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] >= 'A' && text[i] <= 'Z')
            text[i] = static_cast<char>(text[i] - 'A' + 'a');
    return text;
}

bool same(const std::string& a, const std::string& b)
{
    return _stricmp(a.c_str(), b.c_str()) == 0;
}

std::string trimmed(const std::string& text)
{
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

std::string number(int value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%d", value);
    return text;
}

std::string slashes(std::string path)
{
    for (size_t i = 0; i < path.size(); ++i)
        if (path[i] == '\\')
            path[i] = '/';
    return path;
}

bool oneWord(const std::string& text)
{
    return !text.empty() && text.find_first_of(" \t\r\n\"") == std::string::npos;
}

std::vector<std::string> splitOn(const std::string& text, const char* separators)
{
    std::vector<std::string> parts;
    size_t i = 0;
    while (i < text.size())
    {
        while (i < text.size() && std::strchr(separators, text[i]))
            ++i;
        size_t j = i;
        while (j < text.size() && !std::strchr(separators, text[j]))
            ++j;
        if (j > i)
            parts.push_back(text.substr(i, j - i));
        i = j;
    }
    return parts;
}

bool parseFloat(const std::string& text, float& value)
{
    if (text.empty())
        return false;
    char* end = 0;
    const double parsed = std::strtod(text.c_str(), &end);
    if (!end || *end != 0)
        return false;
    value = static_cast<float>(parsed);
    return true;
}

bool parseInt(const std::string& text, long& value)
{
    if (text.empty())
        return false;
    char* end = 0;
    value = std::strtol(text.c_str(), &end, 10);
    return end && *end == 0;
}

const TriggerTypeInfo* triggerType(const std::string& name)
{
    for (int i = 0; i < kTriggerTypeCount; ++i)
        if (same(kTriggerTypes[i].name, name))
            return &kTriggerTypes[i];
    return 0;
}

std::string triggerTypeList()
{
    std::string list;
    for (int i = 0; i < kTriggerTypeCount; ++i)
        list += (i ? ", " : "") + std::string(kTriggerTypes[i].name);
    return list;
}

const PropertySpec* propertySpec(AnimationKind kind, const std::string& name)
{
    for (size_t i = 0; i < sizeof(kProperties) / sizeof(kProperties[0]); ++i)
        if (kProperties[i].kind == kind && same(kProperties[i].name, name))
            return &kProperties[i];
    return 0;
}

std::string propertyList(AnimationKind kind, int family)
{
    std::string list;
    for (size_t i = 0; i < sizeof(kProperties) / sizeof(kProperties[0]); ++i)
        if (kProperties[i].kind == kind && (kProperties[i].family < 0 || kProperties[i].family == family))
            list += (list.empty() ? "" : ", ") + std::string(kProperties[i].name);
    return list;
}

const SystemSpec* systemSpec(const std::string& name)
{
    for (size_t i = 0; i < sizeof(kSystemProperties) / sizeof(kSystemProperties[0]); ++i)
        if (same(kSystemProperties[i].name, name))
            return &kSystemProperties[i];
    return 0;
}

// A value as its row takes it: checked, and normalised where the console accepts more than one spelling.
bool checkValue(ValueType type, const std::string& given, std::string& value, std::string& error)
{
    const std::string text = trimmed(given);
    float f = 0.0f;
    long n = 0;
    switch (type)
    {
    case ValueType::Float:
        if (!parseFloat(text, f))
            break;
        value = text;
        return true;
    case ValueType::PositiveFloat:
        if (!parseFloat(text, f))
            break;
        if (!(f > 0.0f))
        {
            error = "a length is more than 0 (the engine divides by it)";
            return false;
        }
        value = text;
        return true;
    case ValueType::Bool:
        if (text == "1" || same(text, "true"))
            value = "1";
        else if (text == "0" || same(text, "false"))
            value = "0";
        else
            break;
        return true;
    case ValueType::Int:
    case ValueType::UInt:
        if (!parseInt(text, n) || (type == ValueType::UInt && n < 0))
            break;
        value = text;
        return true;
    case ValueType::Vec2:
    case ValueType::Vec3:
    {
        const std::vector<std::string> parts = splitOn(text, "/");
        const size_t want = type == ValueType::Vec2 ? 2 : 3;
        if (parts.size() != want || std::count(text.begin(), text.end(), '/') != static_cast<int>(want - 1))
            break;
        for (size_t i = 0; i < parts.size(); ++i)
            if (!parseFloat(parts[i], f))
            {
                error = "'" + given + "' is not " + (want == 2 ? "x/y" : "x/y/z") + " in numbers";
                return false;
            }
        value = text;
        return true;
    }
    case ValueType::Holder:
        if (!oneWord(text))
            break;
        value = text;
        return true;
    case ValueType::Range3:
    {
        const std::vector<std::string> parts = splitOn(text, " \t/");
        float v[3] = { 0.0f, 0.0f, 0.0f };
        if (parts.size() != 3 || !parseFloat(parts[0], v[0]) || !parseFloat(parts[1], v[1]) || !parseFloat(parts[2], v[2]))
        {
            error = "a value holder's values are three numbers: min normal max";
            return false;
        }
        if (v[0] == v[2])
        {
            error = "min and max are the same, and a trigger bound to the holder divides by their difference";
            return false;
        }
        value = parts[0] + " " + parts[1] + " " + parts[2];
        return true;
    }
    }
    if (error.empty())
        error = "'" + given + "' is not a value of that kind";
    return false;
}

// The file's first bytes through the engine, as it reads them (a mounted archive first).
bool readEngineHead(const std::string& path, std::string& bytes, unsigned int limit)
{
    using namespace dice::hfe;
    if (!io::fileManager)
        return false;
    io::IStream* stream = io::fileManager->open(path, io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0);
    if (!stream)
        return false;
    char buffer[4096];
    while (bytes.size() < limit)
    {
        const unsigned int got = stream->read(buffer, sizeof(buffer));
        if (got == 0)
            break;
        bytes.append(buffer, got);
    }
    stream->release();
    return true;
}

bool readEngineFile(const std::string& path, std::string& text)
{
    return readEngineHead(path, text, 0xFFFFFFFFu);
}

bool engineHas(const std::string& path)
{
    return dice::hfe::io::fileManager && !dice::hfe::io::fileManager->fileExistsEx(path, false).empty();
}

std::string kindLabel(AnimationKind kind)
{
    switch (kind)
    {
    case AnimationKind::Animation:   return "animation";
    case AnimationKind::Bundle:      return "bundle";
    case AnimationKind::Trigger:     return "trigger";
    default:                         return "value holder";
    }
}

// The trigger tree as one line: name(Type)[bundles]{children}, a child the engine skips at load marked so.
void treeText(const AnimationSystemFile& file, const std::string& trigger, std::set<std::string>& path, std::string& out)
{
    const AnimationObject* o = file.find(AnimationKind::Trigger, trigger);
    if (!o)
    {
        out += trigger + "(missing)";
        return;
    }
    out += o->name + "(" + o->type + ")";
    if (!path.insert(lower(o->name)).second)
    {
        out += "(loop)";
        return;
    }
    const std::vector<AnimationProperty> lines = file.properties(AnimationKind::Trigger, o->name);
    std::string bundles;
    std::vector<std::pair<std::string, bool> > children;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const bool child = same(lines[i].property, "addChild");
        if (!child && !same(lines[i].property, "addBundle"))
            continue;
        const AnimationKind kind = child ? AnimationKind::Trigger : AnimationKind::Bundle;
        const bool found = file.createdBefore(kind, lines[i].value, lines[i].line);
        if (child)
            children.push_back(std::make_pair(lines[i].value, found));
        else
            bundles += (bundles.empty() ? "" : ",") + lines[i].value + (found ? "" : "(skipped at load)");
    }
    if (!bundles.empty())
        out += "[" + bundles + "]";
    if (!children.empty())
    {
        out += "{";
        for (size_t i = 0; i < children.size(); ++i)
        {
            if (i)
                out += ", ";
            if (!children[i].second)
                out += children[i].first + "(skipped at load)";
            else
                treeText(file, children[i].first, path, out);
        }
        out += "}";
    }
    path.erase(lower(o->name));
}

std::string folderOf(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

std::string nameOf(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Every match of `from` (no case) replaced by `to`; how many.
int replaceNoCase(std::string& text, const std::string& from, const std::string& to)
{
    if (from.empty())
        return 0;
    const std::string low = lower(text);
    const std::string needle = lower(from);
    std::string out;
    int count = 0;
    size_t at = 0;
    for (;;)
    {
        const size_t hit = low.find(needle, at);
        if (hit == std::string::npos)
            break;
        out += text.substr(at, hit - at) + to;
        at = hit + from.size();
        ++count;
    }
    out += text.substr(at);
    text = out;
    return count;
}

} // namespace

// Documents, the selection, the history

std::vector<EditSession::AnimationPropertyInfo> EditSession::animationProperties(AnimationKind kind, const std::string& triggerType)
{
    const TriggerTypeInfo* info = kind == AnimationKind::Trigger ? bf2editor::triggerType(triggerType) : 0;
    const int family = info ? info->family : -1;
    std::vector<AnimationPropertyInfo> out;
    for (size_t i = 0; i < sizeof(kProperties) / sizeof(kProperties[0]); ++i)
        if (kProperties[i].kind == kind && (kProperties[i].family < 0 || kProperties[i].family == family))
        {
            AnimationPropertyInfo p;
            p.name = kProperties[i].name;
            p.fallback = kProperties[i].fallback;
            out.push_back(p);
        }
    return out;
}

std::vector<std::string> EditSession::animationTriggerTypes()
{
    std::vector<std::string> out;
    for (int i = 0; i < kTriggerTypeCount; ++i)
        out.push_back(kTriggerTypes[i].name);
    return out;
}

std::vector<std::string> EditSession::animationSystemRows()
{
    std::vector<std::string> out;
    for (size_t i = 0; i < sizeof(kSystemProperties) / sizeof(kSystemProperties[0]); ++i)
        out.push_back(kSystemProperties[i].name);
    return out;
}

std::vector<std::string> EditSession::animationSystemList()
{
    std::vector<std::string> systems;
    if (!dice::anim::animationSystemTemplateManager)
        return systems;
    const std::vector<std::string> lines = splitOn(dice::anim::animationSystemTemplateManager->list(), "\r\n");
    for (size_t i = 0; i < lines.size(); ++i)
        if (!trimmed(lines[i]).empty())
            systems.push_back(trimmed(lines[i]));
    return systems;
}

int EditSession::valueHolderDocument(std::string& error)
{
    size_t index = 0;
    if (!m_animations.open(slashes(dice::anim::ValueHolderManager::getFilename()), index, error))
        return -1;
    return static_cast<int>(index);
}

bool EditSession::bafInfo(const std::string& path, float& seconds, int& joints)
{
    const std::string key = lower(slashes(path));
    std::map<std::string, std::pair<float, int> >::const_iterator known = m_bafInfo.find(key);
    if (known == m_bafInfo.end())
    {
        std::pair<float, int> info(-1.0f, -1);
        std::string bytes, error;
        BafHeader header;
        if (readEngineHead(path, bytes, 2048) && readBafHeader(bytes, header, error))
            info = std::make_pair(header.seconds(), header.joints());
        known = m_bafInfo.insert(std::make_pair(key, info)).first;
    }
    seconds = known->second.first;
    joints = known->second.second;
    return joints >= 0;
}

bool EditSession::recordAnimationEdit(size_t document, const std::string& before, const std::string& label, std::string& result)
{
    AnimationDocument& d = m_animations.document(document);
    AnimationEdit edit;
    edit.document = document;
    edit.before = before;
    edit.after = d.file.render();
    if (edit.after == edit.before)
    {
        result = "no change";
        return true;
    }
    const int id = m_nextAnimationEdit++;
    m_animationEdits[id] = edit;
    HistoryEntry entry;
    entry.label = label;
    char line[80];
    std::snprintf(line, sizeof(line), "EditorAnimation.applyEdit %d after\n", id);
    entry.doScript = line;
    std::snprintf(line, sizeof(line), "EditorAnimation.applyEdit %d before\n", id);
    entry.undoScript = line;
    if (!m_history.execute(entry, result))
    {
        d.file.parse(before);
        m_animationEdits.erase(id);
        return false;
    }
    logLine(LogLevel::Info, "edit: %s", label.c_str());
    result = label;
    return true;
}

bool EditSession::applyAnimationEdit(int id, const std::string& state, std::string& error)
{
    std::map<int, AnimationEdit>::const_iterator found = m_animationEdits.find(id);
    if (found == m_animationEdits.end() || (state != "before" && state != "after"))
    {
        error = "no such animation edit (EditorAnimation.applyEdit <id> <before|after>)";
        return false;
    }
    if (found->second.document >= m_animations.count())
    {
        error = "the animation system of that edit is no longer open";
        return false;
    }
    m_animations.document(found->second.document).file.parse(state == "after" ? found->second.after : found->second.before);
    return true;
}

void EditSession::planAnimationFiles(SavePlan& plan)
{
    if (!m_animations.modified().empty())
        m_animations.planSave(plan.part("animation systems"));
}

bool EditSession::saveAnimationFiles(std::string& result)
{
    SavePlan plan;
    planAnimationFiles(plan);
    plan.dropUnchanged();
    return writeSave(plan, "EditorAnimation.save", result) && !plan.anyRefused();
}

bool EditSession::animationTimeline(const std::string& bundle, AnimationTimeline& timeline, std::string& error)
{
    if (m_animationCurrent < 0)
    {
        error = "no animation system is open (EditorAnimation.open <path>)";
        return false;
    }
    const AnimationSystemFile& file = m_animations.document(static_cast<size_t>(m_animationCurrent)).file;
    const AnimationObject* o = file.find(AnimationKind::Bundle, bundle);
    if (!o)
    {
        error = "no bundle " + bundle + " in the system";
        return false;
    }
    timeline = AnimationTimeline();
    timeline.bundle = o->name;
    float furthest = 0.0f;
    const std::vector<BundleItem> animations = file.bundleAnimations(o->name);
    for (size_t i = 0; i < animations.size(); ++i)
    {
        AnimationBar bar;
        bar.name = animations[i].name;
        bar.start = animations[i].start;
        bar.lengthSet = animations[i].lengthSet;
        int joints = 0;
        float own = -1.0f;
        bafInfo(bar.name, own, joints);
        bar.original = own;
        bar.length = own;
        // The animation's own length line, unless the bundle names one (both set the shared animation; the
        // bundle's line, inside the bundle, is the later one at load in the writer's layout).
        const std::vector<AnimationProperty> settings = file.properties(AnimationKind::Animation, bar.name);
        for (size_t k = 0; k < settings.size(); ++k)
        {
            float value = 0.0f;
            if (same(settings[k].property, "length") && parseFloat(settings[k].value, value) && value > 0.0f)
                bar.length = value;
        }
        if (bar.lengthSet)
            bar.length = animations[i].length;
        if (bar.length > 0.0f && bar.start + bar.length > furthest)
            furthest = bar.start + bar.length;
        timeline.bars.push_back(bar);
    }
    const std::vector<BundleItem> events = file.bundleEvents(o->name);
    for (size_t i = 0; i < events.size(); ++i)
    {
        AnimationPoint point;
        point.name = events[i].name;
        point.time = events[i].start;
        if (point.time > furthest)
            furthest = point.time;
        timeline.points.push_back(point);
    }
    timeline.span = furthest;
    const std::vector<AnimationProperty> lines = file.properties(AnimationKind::Bundle, o->name);
    for (size_t i = 0; i < lines.size(); ++i)
    {
        float value = 0.0f;
        if (same(lines[i].property, "length") && parseFloat(lines[i].value, value))
        {
            timeline.span = value;
            timeline.lengthSet = true;
        }
    }
    return true;
}

// The command

bool EditSession::animationCommand(const std::string& action, const std::vector<std::string>& args, std::string& result)
{
    const std::string a0 = args.size() > 0 ? args[0] : std::string();
    const std::string a1 = args.size() > 1 ? args[1] : std::string();
    std::string error;
    if (action == "systems")
    {
        const std::vector<std::string> systems = animationSystemList();
        char head[64];
        std::snprintf(head, sizeof(head), "%u systems", static_cast<unsigned>(systems.size()));
        result = head;
        for (size_t i = 0; i < systems.size(); ++i)
            result += (i ? "; " : ": ") + systems[i];
        return true;
    }
    if (action == "open")
    {
        size_t index = 0;
        if (trimmed(a0).empty())
        {
            result = "EditorAnimation.open <the system's path, e.g. Objects/Weapons/Handheld/at_mine/AnimationSystem1p.inc>";
            return false;
        }
        if (!m_animations.open(slashes(trimmed(a0)), index, result))
            return false;
        m_animationCurrent = static_cast<int>(index);
        m_animationKind.clear();
        m_animationName.clear();
        return animationCommand("status", std::vector<std::string>(), result);
    }
    if (action == "save")
    {
        if (!saveAnimationFiles(result))
            return false;
        if (result.empty())
            result = "nothing to save: the animation systems have no changes";
        return true;
    }
    if (action == "joints")
    {
        const std::string folder = slashes(trimmed(a0));
        const bool list = same(a1, "list");
        if (folder.empty() || (!a1.empty() && !list && !same(a1, "verify")))
        {
            result = "EditorAnimation.joints <folder> [verify|list]";
            return false;
        }
        std::list<dice::hfe::io::FileInfo> files;
        if (dice::hfe::io::fileManager)
            dice::hfe::io::fileManager->findFiles(folder + "/", true, files);
        std::map<int, int> counts;
        std::vector<std::pair<std::string, int> > found;
        std::string unreadable, overLimits;
        std::set<std::string> seen;     // the server and client archives both hold the animations
        for (std::list<dice::hfe::io::FileInfo>::const_iterator it = files.begin(); it != files.end(); ++it)
        {
            if (it->isDirectory || it->name.size() < 5 || _stricmp(it->name.c_str() + it->name.size() - 4, ".baf") != 0)
                continue;
            const std::string path = folder + "/" + slashes(it->name);
            if (!seen.insert(lower(path)).second)
                continue;
            std::string bytes;
            BafHeader header;
            if (!readEngineHead(path, bytes, 2048) || !readBafHeader(bytes, header, error))
            {
                unreadable += (unreadable.empty() ? "" : ", ") + path;
                continue;
            }
            // The retail plugin's own loader refused these (census §8).
            if (header.frames >= 1000 || header.joints() >= 200)
                overLimits += (overLimits.empty() ? "" : ", ") + path;
            ++counts[header.joints()];
            found.push_back(std::make_pair(path, header.joints()));
        }
        if (found.empty() && unreadable.empty())
        {
            result = "no .baf files under " + folder;
            return false;
        }
        int common = -1, most = 0;
        for (std::map<int, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
            if (it->second > most)
            {
                most = it->second;
                common = it->first;
            }
        char head[160];
        std::snprintf(head, sizeof(head), "%u animations under %s; %d of them have %d joints", static_cast<unsigned>(found.size()),
                      folder.c_str(), most, common);
        result = head;
        std::string others;
        for (size_t i = 0; i < found.size(); ++i)
        {
            if (list)
                logLine(LogLevel::Info, "animation joints: %s %d", found[i].first.c_str(), found[i].second);
            if (found[i].second != common)
                others += (others.empty() ? "" : ", ") + found[i].first + " (" + number(found[i].second) + ")";
        }
        result += others.empty() ? "; none differs" : "; differing: " + others;
        if (!overLimits.empty())
            result += "; past the retail loader's 1000 frames or 200 joints: " + overLimits;
        if (!unreadable.empty())
            result += "; unreadable: " + unreadable;
        if (list)
            result += " (each file's count is in the log)";
        return true;
    }
    if (action == "copySystems" || action == "point")
        return animationEdit(action, args, result);
    if (action == "addValueHolder" || (action == "list" && same(a0, "valueholder")))
        return animationEdit(action, args, result);
    if (m_animationCurrent < 0)
    {
        result = "no animation system is open (EditorAnimation.open <path>, EditorAnimation.systems lists the level's)";
        return false;
    }
    AnimationDocument& document = m_animations.document(static_cast<size_t>(m_animationCurrent));
    const AnimationSystemFile& file = document.file;
    if (action == "status")
    {
        int counts[4] = { 0, 0, 0, 0 };
        for (size_t i = 0; i < file.objects().size(); ++i)
            ++counts[static_cast<int>(file.objects()[i].kind)];
        char text[160];
        std::snprintf(text, sizeof(text), ": %d animations, %d bundles, %d triggers", counts[0], counts[1], counts[2]);
        result = document.path + text + (counts[3] ? ", " + number(counts[3]) + " value holders" : std::string()) + "; "
               + (document.archived ? std::string("read from a mounted archive (read-only)") : "loose, saved to " + document.writePath)
               + (document.modified() ? "; modified" : "");
        return true;
    }
    if (action == "tree")
    {
        const std::vector<std::string> roots = file.triggerRoots();
        result.clear();
        for (size_t i = 0; i < roots.size(); ++i)
        {
            std::set<std::string> path;
            if (i)
                result += "; ";
            treeText(file, roots[i], path, result);
        }
        if (result.empty())
            result = "no triggers";
        return true;
    }
    if (action == "list")
    {
        AnimationKind kind = AnimationKind::Bundle;
        if (!AnimationSystemFile::kindFromName(a0, kind))
        {
            result = "EditorAnimation.list <animation|bundle|trigger|valueholder>";
            return false;
        }
        result.clear();
        for (size_t i = 0; i < file.objects().size(); ++i)
            if (file.objects()[i].kind == kind)
                result += (result.empty() ? "" : "; ") + file.objects()[i].name
                        + (kind == AnimationKind::Trigger ? " (" + file.objects()[i].type + ")" : std::string());
        if (result.empty())
            result = "no " + kindLabel(kind) + "s";
        return true;
    }
    if (action == "select")
    {
        AnimationKind kind = AnimationKind::Bundle;
        if (same(a0, "none"))
        {
            m_animationKind.clear();
            m_animationName.clear();
            result = "nothing selected";
            return true;
        }
        if (!AnimationSystemFile::kindFromName(a0, kind) || a1.empty())
        {
            result = "EditorAnimation.select <animation|bundle|trigger|valueholder> <name>";
            return false;
        }
        const AnimationSystemFile* in = &file;
        if (kind == AnimationKind::ValueHolder)
        {
            const int holders = valueHolderDocument(error);
            if (holders < 0)
            {
                result = error;
                return false;
            }
            in = &m_animations.document(static_cast<size_t>(holders)).file;
        }
        const AnimationObject* o = in->find(kind, a1);
        std::string name = o ? o->name : std::string();
        if (!o && kind == AnimationKind::Animation)
        {
            // An animation a bundle plays, with no createAnimation block of its own (the engine loads it then).
            for (size_t i = 0; i < file.objects().size() && name.empty(); ++i)
            {
                if (file.objects()[i].kind != AnimationKind::Bundle)
                    continue;
                const std::vector<BundleItem> items = file.bundleAnimations(file.objects()[i].name);
                for (size_t k = 0; k < items.size() && name.empty(); ++k)
                    if (same(items[k].name, a1))
                        name = items[k].name;
            }
        }
        if (name.empty())
        {
            result = "no " + kindLabel(kind) + " " + a1 + " in " + (kind == AnimationKind::ValueHolder ? std::string("the value holders")
                                                                                                         : document.path);
            return false;
        }
        m_animationKind = AnimationSystemFile::kindName(kind);
        m_animationName = name;
        result = kindLabel(kind) + " " + name;
        return true;
    }
    if (action == "selected" || action == "timeline")
    {
        AnimationKind kind = AnimationKind::Bundle;
        if (m_animationName.empty() || !AnimationSystemFile::kindFromName(m_animationKind, kind))
        {
            result = "nothing is selected (EditorAnimation.select <kind> <name>)";
            return false;
        }
        if (action == "timeline")
        {
            if (kind != AnimationKind::Bundle)
            {
                result = "the timeline shows a bundle: select one";
                return false;
            }
            AnimationTimeline timeline;
            if (!animationTimeline(m_animationName, timeline, result))
                return false;
            result = timeline.bundle + ": length " + AnimationSystemFile::number(timeline.span) + (timeline.lengthSet ? " (set)" : "");
            for (size_t i = 0; i < timeline.bars.size(); ++i)
            {
                const AnimationBar& bar = timeline.bars[i];
                result += "; " + bar.name + " at " + AnimationSystemFile::number(bar.start) + " for "
                        + (bar.length > 0.0f ? AnimationSystemFile::number(bar.length) : std::string("?"))
                        + (bar.original > 0.0f ? " (its own " + AnimationSystemFile::number(bar.original) + ")"
                                               : std::string(" (its .baf unread)"));
            }
            for (size_t i = 0; i < timeline.points.size(); ++i)
                result += "; event " + timeline.points[i].name + " at " + AnimationSystemFile::number(timeline.points[i].time);
            return true;
        }
        const AnimationSystemFile* in = &file;
        if (kind == AnimationKind::ValueHolder)
        {
            const int holders = valueHolderDocument(error);
            if (holders < 0)
            {
                result = error;
                return false;
            }
            in = &m_animations.document(static_cast<size_t>(holders)).file;
        }
        const AnimationObject* o = in->find(kind, m_animationName);
        result = kindLabel(kind) + " " + m_animationName + (o && !o->type.empty() ? " (" + o->type + ")" : std::string());
        if (!o && kind != AnimationKind::Animation)
        {
            result += ": no longer in the file";
            return false;
        }
        const std::vector<AnimationProperty> lines = in->properties(kind, m_animationName);
        for (size_t i = 0; i < lines.size(); ++i)
            result += (i ? "; " : ": ") + lines[i].property + (lines[i].value.empty() ? "" : " " + lines[i].value);
        if (lines.empty())
            result += o ? ": no lines" : ": played by a bundle, no createAnimation block";
        return true;
    }
    return animationEdit(action, args, result);
}

// The edits

bool EditSession::animationEdit(const std::string& action, const std::vector<std::string>& args, std::string& result)
{
    const std::string a0 = args.size() > 0 ? args[0] : std::string();
    const std::string a1 = args.size() > 1 ? args[1] : std::string();
    const std::string a2 = args.size() > 2 ? args[2] : std::string();
    std::string error;

    // The template side (A6): point a template at a system, or copy another template's pair for it
    if (action == "point")
    {
        const char* property = same(a1, "1p") ? "animationSystem1P" : same(a1, "3p") ? "animationSystem3P"
                             : same(a1, "seat") ? "seatAnimationSystem" : 0;
        const std::string path = slashes(trimmed(a2));
        if (!property || a0.empty() || path.empty())
        {
            result = "EditorAnimation.point <template> <1p|3p|seat> <system path>";
            return false;
        }
        if (!engineHas(path))
        {
            result = "the engine finds no " + path + " (the template would load an empty system)";
            return false;
        }
        return setObjectTemplateProperty(a0, property, path, result);
    }
    if (action == "copySystems")
    {
        if (a0.empty() || a1.empty())
        {
            result = "EditorAnimation.copySystems <template> <source template>";
            return false;
        }
        // The source's 1p and 3p systems, from its files.
        std::vector<size_t> sourceFiles, targetFiles;
        if (!m_templateDocuments.filesOf(a1, sourceFiles, result) || !m_templateDocuments.filesOf(a0, targetFiles, result))
            return false;
        const char* const kProps[2] = { "animationSystem1P", "animationSystem3P" };
        std::string sourcePath[2];
        for (size_t f = 0; f < sourceFiles.size(); ++f)
        {
            const std::vector<TemplatePropertyLine> lines = m_templateDocuments.document(sourceFiles[f]).file.properties(a1);
            for (size_t k = 0; k < lines.size(); ++k)
                for (int p = 0; p < 2; ++p)
                    if (same(lines[k].property, kProps[p]))
                        sourcePath[p] = slashes(trimmed(lines[k].value));
        }
        if (sourcePath[0].empty() && sourcePath[1].empty())
        {
            result = a1 + "'s files name no animationSystem1P or animationSystem3P";
            return false;
        }
        if (targetFiles.empty() || m_templateDocuments.document(targetFiles[0]).archived)
        {
            result = a0 + "'s files are read from a mounted archive: its folder cannot take new files";
            return false;
        }
        const std::string targetFolder = folderOf(slashes(m_templateDocuments.document(targetFiles[0]).path));
        const std::string targetWrite = folderOf(slashes(m_templateDocuments.document(targetFiles[0]).writePath));
        const std::string sourceFolder = folderOf(slashes(m_templateDocuments.document(sourceFiles[0]).path));
        // The retail rename (census §4.10): the source folder's name, then the part after its first '_'.
        const std::string from = nameOf(sourceFolder);
        const std::string to = nameOf(targetFolder);
        const size_t fromUnder = from.find('_');
        const size_t toUnder = to.find('_');
        std::string newPath[2], newText[2];
        int renamed = 0;
        for (int p = 0; p < 2; ++p)
        {
            if (sourcePath[p].empty())
                continue;
            if (!readEngineFile(sourcePath[p], newText[p]))
            {
                result = "the engine finds no " + sourcePath[p];
                return false;
            }
            renamed += replaceNoCase(newText[p], from, to);
            if (fromUnder != std::string::npos && toUnder != std::string::npos)
                renamed += replaceNoCase(newText[p], from.substr(fromUnder + 1), to.substr(toUnder + 1));
            newPath[p] = targetFolder + "/" + nameOf(sourcePath[p]);
            if (engineHas(newPath[p]))
            {
                result = newPath[p] + " exists already: point the template at it (EditorAnimation.point)";
                return false;
            }
            // Every animation the copy names must exist: the game stops on a missing .baf.
            AnimationSystemFile copy;
            copy.parse(newText[p]);
            std::string missing;
            int missingCount = 0;
            std::set<std::string> seen;
            for (size_t i = 0; i < copy.objects().size(); ++i)
            {
                std::vector<std::string> bafs;
                if (copy.objects()[i].kind == AnimationKind::Animation)
                    bafs.push_back(copy.objects()[i].name);
                else if (copy.objects()[i].kind == AnimationKind::Bundle)
                {
                    const std::vector<BundleItem> items = copy.bundleAnimations(copy.objects()[i].name);
                    for (size_t k = 0; k < items.size(); ++k)
                        bafs.push_back(items[k].name);
                }
                for (size_t k = 0; k < bafs.size(); ++k)
                    if (seen.insert(lower(bafs[k])).second && !engineHas(bafs[k]) && ++missingCount <= 3)
                        missing += (missing.empty() ? "" : ", ") + bafs[k];
            }
            if (missingCount > 0)
            {
                result = "the copy of " + sourcePath[p] + " would name " + number(missingCount) + " animations that do not "
                         "exist (the game stops on a missing .baf): " + missing + (missingCount > 3 ? " ..." : "")
                         + " - put the .baf files in place first";
                return false;
            }
        }
        // Written at once (one backed-up save), as a clone is; the template lines are undoable template edits.
        std::string written;
        std::vector<std::pair<std::string, std::string> > files;
        for (int p = 0; p < 2; ++p)
        {
            if (newPath[p].empty())
                continue;
            files.push_back(std::make_pair(targetWrite + "/" + nameOf(newPath[p]), newText[p]));
            written += (written.empty() ? "" : " and ") + newPath[p];
        }
        if (!writeFilesNow("animation copy", files, error))
        {
            result = error;
            return false;
        }
        std::string pointed;
        for (int p = 0; p < 2; ++p)
            if (!newPath[p].empty() && !setObjectTemplateProperty(a0, kProps[p], newPath[p], pointed))
            {
                result = "wrote " + written + ", but " + a0 + " was not pointed at it: " + pointed;
                return false;
            }
        result = "copied " + written + " from " + a1 + "'s systems (" + number(renamed) + " names changed) and pointed " + a0
               + " at them";
        logLine(LogLevel::Info, "animation: %s", result.c_str());
        return true;
    }

    // The value holders' own file
    if (action == "addValueHolder" || action == "list")
    {
        const int holders = valueHolderDocument(error);
        if (holders < 0)
        {
            result = error;
            return false;
        }
        AnimationDocument& d = m_animations.document(static_cast<size_t>(holders));
        if (action == "list")
        {
            result.clear();
            for (size_t i = 0; i < d.file.objects().size(); ++i)
                if (d.file.objects()[i].kind == AnimationKind::ValueHolder)
                    result += (result.empty() ? "" : "; ") + d.file.objects()[i].name;
            if (result.empty())
                result = "no value holders";
            return true;
        }
        if (d.archived)
        {
            result = d.path + kReadOnly;
            return false;
        }
        std::string values;
        if (!oneWord(a0))
        {
            result = "EditorAnimation.addValueHolder <name> <\"min normal max\">";
            return false;
        }
        if (!checkValue(ValueType::Range3, a1, values, result))
            return false;
        const std::string before = d.file.render();
        if (!d.file.addObject(AnimationKind::ValueHolder, "", a0, error)
            || !d.file.setProperty(AnimationKind::ValueHolder, a0, "values", values, -1, error))
        {
            d.file.parse(before);
            result = error;
            return false;
        }
        m_animationKind = "valueholder";
        m_animationName = a0;
        return recordAnimationEdit(static_cast<size_t>(holders), before, "Add the value holder " + a0, result);
    }

    // The open system
    AnimationKind kind = AnimationKind::Bundle;
    const bool selected = !m_animationName.empty() && AnimationSystemFile::kindFromName(m_animationKind, kind);
    size_t target = static_cast<size_t>(m_animationCurrent);
    if (selected && kind == AnimationKind::ValueHolder
        && (action == "set" || action == "unset" || action == "remove"))
    {
        const int holders = valueHolderDocument(error);
        if (holders < 0)
        {
            result = error;
            return false;
        }
        target = static_cast<size_t>(holders);
    }
    AnimationDocument& d = m_animations.document(target);
    AnimationSystemFile& file = d.file;
    const std::string before = file.render();
    const std::string name = m_animationName;
    const AnimationObject* object = selected ? file.find(kind, name) : 0;
    std::string label;

    // What each edit needs selected.
    const bool needsBundle = action == "addAnimation" || action == "addEvent" || action == "removeAnimation"
                          || action == "removeEvent" || action == "setStart" || action == "setLength";
    const bool needsTrigger = action == "link" || action == "unlink";
    const bool needsAny = action == "set" || action == "unset" || action == "remove";
    const bool known = needsBundle || needsTrigger || needsAny || action == "setSystem" || action == "unsetSystem"
                    || action == "addTrigger" || action == "addBundle";
    if (!known)
    {
        result = "no animation action '" + action + "'";
        return false;
    }
    if (d.archived)
    {
        result = d.path + kReadOnly;
        return false;
    }
    if ((needsBundle && (!selected || kind != AnimationKind::Bundle || !object))
        || (needsTrigger && (!selected || kind != AnimationKind::Trigger || !object))
        || (needsAny && (!selected || (!object && kind != AnimationKind::Animation))))
    {
        result = std::string("select ") + (needsBundle ? "a bundle" : needsTrigger ? "a trigger" : "an object")
               + " first (EditorAnimation.select <kind> <name>)";
        return false;
    }

    if (action == "set" || action == "unset")
    {
        const PropertySpec* spec = propertySpec(kind, a0);
        int family = -1;
        if (kind == AnimationKind::Trigger && object)
        {
            const TriggerTypeInfo* info = triggerType(object->type);
            family = info ? info->family : -1;
        }
        if (!spec || (spec->family >= 0 && spec->family != family))
        {
            result = "'" + a0 + "' is not a property of " + (kind == AnimationKind::Trigger && object ? "a " + object->type : "a " + kindLabel(kind))
                   + " (" + propertyList(kind, family) + ")";
            return false;
        }
        if (action == "unset")
        {
            if (!file.removeProperty(kind, name, spec->name, -1, error))
            {
                result = error;
                return false;
            }
            label = "Take out " + std::string(spec->name) + " of the " + kindLabel(kind) + " " + name + " (the engine's "
                    + spec->fallback + " then)";
        }
        else
        {
            std::string value;
            if (!checkValue(spec->type, a1, value, result))
                return false;
            if (spec->type == ValueType::Holder)
            {
                const int holders = valueHolderDocument(error);
                if (holders < 0 || !m_animations.document(static_cast<size_t>(holders)).file.find(AnimationKind::ValueHolder, value))
                {
                    result = "no value holder " + value + " in " + slashes(dice::anim::ValueHolderManager::getFilename())
                           + " (the trigger would hold none)";
                    return false;
                }
            }
            // An animation a bundle plays with no block of its own gets one, in the Animations section.
            if (kind == AnimationKind::Animation && !object)
            {
                const AnimationObject* firstBundle = 0;
                for (size_t i = 0; i < file.objects().size() && !firstBundle; ++i)
                    if (file.objects()[i].kind == AnimationKind::Bundle)
                        firstBundle = &file.objects()[i];
                if (!file.addObject(AnimationKind::Animation, "", name, error, firstBundle ? firstBundle->createLine : static_cast<size_t>(-1)))
                {
                    result = error;
                    return false;
                }
            }
            if (!file.setProperty(kind, name, spec->name, value, -1, error))
            {
                file.parse(before);
                result = error;
                return false;
            }
            label = "Set " + std::string(spec->name) + " " + value + " of the " + kindLabel(kind) + " " + name;
        }
    }
    else if (action == "setSystem" || action == "unsetSystem")
    {
        const SystemSpec* spec = systemSpec(a0);
        if (!spec)
        {
            std::string list;
            for (size_t i = 0; i < sizeof(kSystemProperties) / sizeof(kSystemProperties[0]); ++i)
                list += (i ? ", " : "") + std::string(kSystemProperties[i].name);
            result = "'" + a0 + "' is not one of the system's rows (" + list + ")";
            return false;
        }
        const bool use = same(spec->name, "cameraSpring.use");
        bool springOn = false;
        const std::vector<AnimationProperty> lines = file.systemLines();
        for (size_t i = 0; i < lines.size(); ++i)
            if (same(lines[i].property, "cameraSpring.use"))
                springOn = trimmed(lines[i].value) == "1";
        if (action == "unsetSystem")
        {
            if (!file.removeSystemProperty(spec->name, error))
            {
                result = error;
                return false;
            }
            label = "Take out " + std::string(spec->name) + " of " + d.path;
        }
        else
        {
            std::string value;
            if (!checkValue(spec->type, a1, value, result))
                return false;
            if (!use && !springOn)
            {
                result = "the camera spring is off in this system (setSystem cameraSpring.use 1 first): the engine ignores its "
                         "other rows without one";
                return false;
            }
            if (!file.setSystemProperty(spec->name, value, error))
            {
                result = error;
                return false;
            }
            label = "Set " + std::string(spec->name) + " " + value + " of " + d.path;
        }
    }
    else if (action == "addTrigger")
    {
        const TriggerTypeInfo* info = triggerType(a0);
        if (!info)
        {
            result = "'" + a0 + "' is not a trigger type the engine makes (" + triggerTypeList() + ")";
            return false;
        }
        if (!oneWord(a1) || file.find(AnimationKind::Trigger, a1))
        {
            result = oneWord(a1) ? "the system has a trigger " + a1 + " already" : "EditorAnimation.addTrigger <type> <name>";
            return false;
        }
        const bool child = selected && kind == AnimationKind::Trigger && object;
        const std::string parent = child ? object->name : std::string();
        if (!file.addObject(AnimationKind::Trigger, info->name, a1, error, child ? object->createLine : static_cast<size_t>(-1))
            || (child && !file.addLine(AnimationKind::Trigger, parent, "addChild", a1, error)))
        {
            file.parse(before);
            result = error;
            return false;
        }
        label = "Add the " + std::string(info->name) + " " + a1 + (child ? " under " + parent : std::string(" as a root trigger"));
        m_animationKind = "trigger";
        m_animationName = a1;
    }
    else if (action == "addBundle")
    {
        if (!oneWord(a0) || file.find(AnimationKind::Bundle, a0))
        {
            result = oneWord(a0) ? "the system has a bundle " + a0 + " already" : "EditorAnimation.addBundle <name>";
            return false;
        }
        const bool played = selected && kind == AnimationKind::Trigger && object;
        const std::string trigger = played ? object->name : std::string();
        // A new bundle loops by default (Bundle's constructor), so retail's `isLooping 1` adds no line.
        if (!file.addObject(AnimationKind::Bundle, "", a0, error, played ? object->createLine : static_cast<size_t>(-1))
            || (played && !file.addLine(AnimationKind::Trigger, trigger, "addBundle", a0, error)))
        {
            file.parse(before);
            result = error;
            return false;
        }
        label = "Add the bundle " + a0 + (played ? " played by " + trigger : std::string());
        m_animationKind = "bundle";
        m_animationName = a0;
    }
    else if (action == "addAnimation")
    {
        const std::string path = slashes(trimmed(a0));
        if (path.size() < 5 || _stricmp(path.c_str() + path.size() - 4, ".baf") != 0)
        {
            result = "an animation is a .baf file (EditorAnimation.addAnimation <path>)";
            return false;
        }
        if (!engineHas(path))
        {
            result = "the engine finds no " + path + " (the game stops on a missing .baf)";
            return false;
        }
        const std::vector<BundleItem> items = file.bundleAnimations(name);
        for (size_t i = 0; i < items.size(); ++i)
            if (same(items[i].name, path))
            {
                result = "the bundle " + name + " plays " + path + " already";
                return false;
            }
        if (!file.find(AnimationKind::Animation, path)
            && !file.addObject(AnimationKind::Animation, "", path, error, object->createLine))
        {
            result = error;
            return false;
        }
        if (!file.addLine(AnimationKind::Bundle, name, "addAnimation", path, error))
        {
            file.parse(before);
            result = error;
            return false;
        }
        label = "Add " + path + " to the bundle " + name;
        // Retail asked when the animation outlasts the bundle's own length line (census §4).
        AnimationTimeline timeline;
        std::string ignored;
        if (animationTimeline(name, timeline, ignored) && timeline.lengthSet)
            for (size_t i = 0; i < timeline.bars.size(); ++i)
                if (same(timeline.bars[i].name, path) && timeline.bars[i].length > timeline.span)
                    label += " (it outlasts the bundle's length " + AnimationSystemFile::number(timeline.span) + ")";
    }
    else if (action == "addEvent")
    {
        if (!oneWord(a0))
        {
            result = "EditorAnimation.addEvent <name> (effect:<n>, sound:<n> or a name)";
            return false;
        }
        // The engine keeps event names lowercased (Bundle::BundleEvent), so two spellings are one event.
        const std::vector<BundleItem> items = file.bundleEvents(name);
        for (size_t i = 0; i < items.size(); ++i)
            if (same(items[i].name, a0))
            {
                result = "the bundle " + name + " has the event " + items[i].name + " already";
                return false;
            }
        if (!file.addLine(AnimationKind::Bundle, name, "addEvent", a0, error))
        {
            result = error;
            return false;
        }
        label = "Add the event " + a0 + " to the bundle " + name;
    }
    else if (action == "removeAnimation" || action == "removeEvent")
    {
        const bool event = action == "removeEvent";
        const std::string item = event ? trimmed(a0) : slashes(trimmed(a0));
        if (!file.removeBundleItem(name, item, event, error))
        {
            result = error;
            return false;
        }
        label = "Take " + std::string(event ? "the event " : "") + item + " out of the bundle " + name;
    }
    else if (action == "setStart")
    {
        const bool event = same(a0, "event");
        float time = 0.0f;
        if ((!event && !same(a0, "animation")) || !parseFloat(trimmed(a2), time))
        {
            result = "EditorAnimation.setStart <animation|event> <name> <seconds>";
            return false;
        }
        const std::string item = event ? trimmed(a1) : slashes(trimmed(a1));
        if (!(event ? file.setEventStart(name, item, time, error) : file.setAnimationStart(name, item, time, error)))
        {
            result = error;
            return false;
        }
        label = "Start " + item + " at " + AnimationSystemFile::number(time) + " in the bundle " + name;
    }
    else if (action == "setLength")
    {
        float length = 0.0f;
        const std::string item = slashes(trimmed(a0));
        if (!parseFloat(trimmed(a1), length))
        {
            result = "EditorAnimation.setLength <.baf> <seconds>";
            return false;
        }
        if (!file.setAnimationLength(name, item, length, error))
        {
            result = error;
            return false;
        }
        // Bundle::setAnimationLength sets the shared animation: every bundle and system that plays it.
        label = "Play " + item + " over " + AnimationSystemFile::number(length) + " s (in every bundle that plays it)";
    }
    else if (action == "link" || action == "unlink")
    {
        const bool child = same(a0, "child");
        if ((!child && !same(a0, "bundle")) || !oneWord(a1))
        {
            result = "EditorAnimation." + action + " <child|bundle> <name>";
            return false;
        }
        const char* property = child ? "addChild" : "addBundle";
        const AnimationKind linked = child ? AnimationKind::Trigger : AnimationKind::Bundle;
        const std::vector<std::string> current = child ? file.triggerChildren(name) : file.triggerBundles(name);
        int at = -1;
        for (size_t i = 0; i < current.size(); ++i)
            if (same(current[i], a1) && at < 0)
                at = static_cast<int>(i);
        if (action == "unlink")
        {
            if (at < 0 || !file.removeProperty(AnimationKind::Trigger, name, property, current.size() > 1 ? at : -1, error))
            {
                result = at < 0 ? "the trigger " + name + " has no " + (child ? "child " : "bundle ") + a1 : error;
                return false;
            }
            label = child ? "Unlink the trigger " + a1 + " from " + name : "Stop " + name + " playing the bundle " + a1;
        }
        else
        {
            const AnimationObject* other = file.find(linked, a1);
            if (!other)
            {
                result = "no " + kindLabel(linked) + " " + a1 + " in the system";
                return false;
            }
            if (at >= 0)
            {
                result = "the trigger " + name + (child ? " has the child " : " plays the bundle ") + a1 + " already";
                return false;
            }
            if (child)
            {
                const std::vector<std::string> under = file.triggerSubtree(a1);
                for (size_t i = 0; i < under.size(); ++i)
                    if (same(under[i], name))
                    {
                        result = a1 + " holds " + name + " already, so the link would close a loop of triggers";
                        return false;
                    }
            }
            if (!file.createdBefore(linked, a1, file.insertionLine(AnimationKind::Trigger, name, property)))
            {
                result = a1 + " is created below " + name + "'s block, and the engine meets a link only to what it has made "
                         "already: the link would be skipped at load";
                return false;
            }
            if (!file.addLine(AnimationKind::Trigger, name, property, other->name, error))
            {
                result = error;
                return false;
            }
            label = child ? "Link the trigger " + other->name + " under " + name : "Let " + name + " play the bundle " + other->name;
        }
    }
    else if (action == "remove")
    {
        if (kind == AnimationKind::Trigger)
        {
            // As TriggerManager::deleteTrigger: the trigger and every trigger under it, out of every parent.
            const std::vector<std::string> doomed = file.triggerSubtree(name);
            for (size_t i = 0; i < doomed.size(); ++i)
            {
                file.removeReferences(AnimationKind::Trigger, "addChild", doomed[i]);
                if (!file.removeObject(AnimationKind::Trigger, doomed[i], error))
                {
                    file.parse(before);
                    result = error;
                    return false;
                }
            }
            label = "Delete the trigger " + name
                  + (doomed.size() > 1 ? " and the " + number(static_cast<int>(doomed.size()) - 1) + " under it" : std::string());
        }
        else if (kind == AnimationKind::Bundle)
        {
            // As BundleManager::deleteBundle: out of every trigger that plays it.
            const int links = file.removeReferences(AnimationKind::Trigger, "addBundle", name);
            if (!file.removeObject(AnimationKind::Bundle, name, error))
            {
                file.parse(before);
                result = error;
                return false;
            }
            label = "Delete the bundle " + name + (links ? " (played by " + number(links) + " triggers)" : std::string());
        }
        else if (kind == AnimationKind::Animation)
        {
            if (!object || !file.removeObject(AnimationKind::Animation, name, error))
            {
                result = object ? error : name + " has no createAnimation block; take it out of its bundles (removeAnimation)";
                return false;
            }
            label = "Delete the settings of " + name + " (bundles that play it play it at the engine's defaults)";
        }
        else
        {
            if (!file.removeObject(AnimationKind::ValueHolder, name, error))
            {
                result = error;
                return false;
            }
            label = "Delete the value holder " + name + " (triggers that name it then hold none)";
        }
        m_animationKind.clear();
        m_animationName.clear();
    }
    return recordAnimationEdit(target, before, label, result);
}

} // namespace bf2editor
