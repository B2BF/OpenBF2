// The editor's HUD and its HUD editor (added) - App/HudEditor.h.
#include "App/HudEditor.h"

#include "App/EditSession.h"
#include "App/ImGuiHost.h"
#include "Framework/ArchiveRewrite.h"
#include "Framework/EditorLog.h"

#include "External/BFMeme/TransformNode.h"
#include "External/BFMeme/TypeDescriptor.h"
#include "Game/HudInformationLayer.h"
#include "Game/Main/BF2ClientSeams.h"
#include "IO/Console/Console.h"
#include "IO/File/FileManager.h"
#include "IO/File/ZipArchive.h"
#include "IO/Stream/IStream.h"
#include "Menu/BF2HudManager.h"
#include "Menu/Bf2BarNode.h"
#include "Menu/Bf2ButtonNode.h"
#include "Menu/Bf2CompassNode.h"
#include "Menu/Bf2PictureNode.h"
#include "Menu/Bf2TextNode.h"
#include "Menu/Bf2TransformNode.h"
#include "Menu/Ref2System.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <direct.h>
#include <list>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace bf2editor
{

namespace
{

const char kMainScript[] = "Menu/HUD/HudSetup/HudSetupMain.con";
const float kCanvasWidth = 800.0f;     // the HUD's Ref2System canvas (BF2HudManager::init)
const float kCanvasHeight = 600.0f;

dice::hfe::HudInformationLayer* informationLayer()
{
    return static_cast<dice::hfe::HudInformationLayer*>(dice::hfe::g_hudInformationLayer);
}

std::string engineLine(const std::string& line)
{
    std::string answer;
    dice::hfe::io::mainConsole->executeLine(line + "\n", answer, false, false, false, false, true);
    return answer;
}

bool readThroughEngine(const std::string& path, std::string& text)
{
    using namespace dice::hfe;
    io::IStream* stream = io::fileManager->open(path, io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0);
    if (!stream)
        return false;
    char buffer[65536];
    for (;;)
    {
        const unsigned int got = stream->read(buffer, sizeof(buffer));
        if (got == 0)
            break;
        text.append(buffer, got);
    }
    stream->release();
    return true;
}

bool engineFileExists(const std::string& path)
{
    using namespace dice::hfe;
    io::IStream* stream = io::fileManager->open(path, io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0);
    if (!stream)
        return false;
    stream->release();
    return true;
}

std::string lowerOf(const std::string& text)
{
    std::string out(text);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

// A line as one console word for the history's rows: '%', blanks and quotes percent-encoded ("%00" for none).
std::string encodeLine(const std::string& text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '%' || c == ' ' || c == '\t' || c == '"')
        {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
            out += hex;
        }
        else
            out += c;
    }
    return out.empty() ? std::string("%00") : out;
}

std::string decodeLine(const std::string& text)
{
    if (text == "%00")
        return std::string();
    std::string out;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1]))
            && std::isxdigit(static_cast<unsigned char>(text[i + 2])))
        {
            out += static_cast<char>(std::strtol(text.substr(i + 1, 2).c_str(), 0, 16));
            i += 2;
        }
        else
            out += text[i];
    }
    return out;
}

bool isNumber(const std::string& text)
{
    if (text.empty())
        return false;
    char* end = 0;
    std::strtod(text.c_str(), &end);
    return end && !*end;
}

// The live HUD's object of that name when it is a T (the builder's own type walk); 0 otherwise.
template<class T> T* liveAs(const std::string& name)
{
    using namespace dice;
    if (!hfe::hudManager || !hfe::hudManager->getSystem() || name.empty())
        return 0;
    meme::Object* object = hfe::hudManager->getSystem()->findNamedObject(name.c_str());
    if (!object)
        return 0;
    for (meme::TypeDescriptor* type = object->getType(); type; type = type->getBaseType())
        if (type == T::getClassType())
            return static_cast<T*>(object);
    return 0;
}

bool liveExists(const std::string& name)
{
    using namespace dice;
    return hfe::hudManager && hfe::hudManager->getSystem() && !name.empty()
        && hfe::hudManager->getSystem()->findNamedObject(name.c_str()) != 0;
}

// The rows the editor gives the live HUD again: those whose builder function cannot fail on a well-formed value
// once its object is there (a failing builder row ends the process: Debug(DebugTypeError) exits). Others wait for
// the next start. `type` is the object the row needs: 0 the node's transform, 1 a picture, 2 a text, 3 a bar,
// 4 a button, 5 a compass; `numbers` how many of its words, from the first, must be numbers.
struct LiveRow
{
    const char* method;
    int         type;
    int         words;
    int         numbers;
};
const LiveRow kLiveRows[] = {
    { "setnodecolor", 0, 4, 4 },
    { "setnodeintime", 0, 1, 1 },
    { "setnodeouttime", 0, 1, 1 },
    { "setnodepos", 0, 2, 2 },
    { "setnodesize", 0, 2, 2 },
    { "setnodeoffset", 0, 2, 2 },
    { "setpicturenodetexture", 1, 1, 0 },
    { "setpicturenoderotation", 1, 1, 1 },
    { "setpicturenodecenterpoint", 1, 2, 2 },
    { "setpicturenodebordercolor", 1, 4, 4 },
    { "settextnodestring", 2, 1, 0 },
    { "setbarnodetexture", 3, 2, 1 },
    { "setbuttonnodetexture", 4, 2, 1 },
    { "setcompassnodetexture", 5, 2, 1 },
};

const LiveRow* liveRow(const std::string& method)
{
    const std::string low = lowerOf(method);
    for (size_t i = 0; i < sizeof(kLiveRows) / sizeof(kLiveRows[0]); ++i)
        if (low == kLiveRows[i].method)
            return &kLiveRows[i];
    return 0;
}

bool liveTypeMatches(int type, const std::string& name)
{
    using namespace dice::meme;
    switch (type)
    {
    case 0: return liveAs<Bf2TransformNode>(name) != 0;
    case 1: return liveAs<Bf2PictureNode>(name) != 0;
    case 2: return liveAs<Bf2TextNode>(name) != 0;
    case 3: return liveAs<Bf2BarNode>(name) != 0;
    case 4: return liveAs<Bf2ButtonNode>(name) != 0;
    case 5: return liveAs<Bf2CompassNode>(name) != 0;
    }
    return false;
}

// Where the HUD's last update put each script-made node on the canvas (Menu/Bf2TransformNode's editor hook).
struct CanvasRect
{
    float x, y, width, height;
};
std::unordered_map<const void*, CanvasRect>& updatedRects()
{
    static std::unordered_map<const void*, CanvasRect> rects;
    return rects;
}
void nodeUpdated(dice::meme::Bf2TransformNode* node, float x, float y, float width, float height)
{
    // The HUD's own space has its origin at the canvas centre (System::setSize puts the root rectangle's corner at
    // minus half the size); the editor's canvas has it at the top-left corner, as the scripts' areas count.
    const CanvasRect rect = { x + kCanvasWidth * 0.5f, y + kCanvasHeight * 0.5f, width, height };
    updatedRects()[node] = rect;
}

// The entry an archive holds for a virtual path (its mount folder left off): tried from the whole path down to its
// file name. `bytes` receives the entry's content.
bool archiveEntryOf(const std::string& archive, const std::string& path, std::string& entry, std::string& bytes)
{
    size_t from = 0;
    for (;;)
    {
        std::string error;
        const std::string candidate = path.substr(from);
        if (readArchiveEntry(archive, candidate, bytes, error))
        {
            entry = candidate;
            return true;
        }
        const size_t slash = path.find('/', from);
        if (slash == std::string::npos)
            return false;
        from = slash + 1;
    }
}

void makeFolders(const std::string& path)
{
    for (size_t i = 0; i <= path.size(); ++i)
        if (i == path.size() || path[i] == '/' || path[i] == '\\')
            _mkdir(path.substr(0, i).c_str());
}

bool fileOnDisk(const std::string& path)
{
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    std::fclose(file);
    return true;
}

// An archive the engine let go of during a save, to be mounted again where and as it was.
struct ReleasedArchive
{
    std::string filename;
    std::string mount;
    bool        contentCheck;
};

void mountAgain(const ReleasedArchive& archive)
{
    using namespace dice::hfe;
    io::IArchive* mounted = io::fileManager->mountArchive(archive.filename, archive.mount, archive.contentCheck);
    if (!mounted)
    {
        // Mounting after start-up waits for permission (FileManager::mountArchive's first gate): given for this one.
        io::fileManager->permitMountArchives(true);
        mounted = io::fileManager->mountArchive(archive.filename, archive.mount, archive.contentCheck);
        io::fileManager->permitMountArchives(false);
    }
    if (mounted)
        logLine(LogLevel::Info, "hud: %s mounted again at %s", archive.filename.c_str(), archive.mount.c_str());
    else
        logLine(LogLevel::Error, "hud: %s could not be mounted again at %s - start the editor again before reading "
                "anything from it", archive.filename.c_str(), archive.mount.c_str());
}

// The rows the Add row field offers, with words to start from.
const char* const kRowTemplates[][2] = {
    { "setNodeColor", "1 1 1 1" },
    { "setNodeShowVariable", "ShowIngameHud" },
    { "setNodeAlphaVariable", "" },
    { "setNodeLogicShowVariable", "EQUAL GuiIndex 1" },
    { "setNodeInTime", "0.5" },
    { "setNodeOutTime", "0.5" },
    { "addNodeAlphaShowEffect", "" },
    { "setPictureNodeTexture", "Ingame/GeneralIcons/empty.tga" },
    { "setPictureNodeRotation", "0" },
    { "setTextNodeString", "text" },
    { "setTextNodeStyle", "Fonts/vehicleHudFont_6.dif 2" },
    { "setTextNodeStringVariable", "" },
};

} // namespace

HudEditor::HudEditor(EditSession& session)
    : m_session(session)
{
    dice::meme::editorHudNodeUpdated = &nodeUpdated;
    if (load(m_loadError))
        logLine(LogLevel::Info, "hud: %u scripts, %u parts from %s%s", static_cast<unsigned>(m_setup.scriptCount()),
                static_cast<unsigned>(m_setup.nodes().size()), kMainScript,
                m_setup.missing().empty() ? "" : " (some scripts it runs are missing; the game skips them too)");
    else
        logLine(LogLevel::Warning, "hud: the HUD editor has no scripts: %s", m_loadError.c_str());
}

HudEditor::~HudEditor()
{
    dice::meme::editorHudNodeUpdated = 0;
    updatedRects().clear();
}

bool HudEditor::load(std::string& error)
{
    return m_setup.load(kMainScript, &readThroughEngine, error);
}

void HudEditor::update(float seconds, bool inGame)
{
    using namespace dice::hfe;
    if (!hudManager)
        return;
    if (inGame != m_inGame)
    {
        // The play test's game state drives the HUD's variables: the preview's holds step aside meanwhile.
        if (inGame)
            restoreAutoHeld();
        else
            select(m_selected);
        m_inGame = inGame;
    }
    const bool shown = inGame || m_preview;
    // [CHOSEN, not recovered] The play test's soldier, or the preview, stands in for the spawn the client's HUD hears as
    // GameLogic event 14 (the local player's default vehicle): the spawn screen (mode 1) gives way to the in-game HUD.
    if (shown && hudManager->getMode() == 1)
        hudManager->setMode(0, true);
    // Hidden otherwise, through the renderer's own row so the console can still change it.
    const int drawn = shown ? 1 : 0;
    if (drawn != m_drawn)
    {
        m_drawn = drawn;
        engineLine(drawn ? "renderer.drawHud 1" : "renderer.drawHud 0");
    }
    if (m_preview && !inGame)
    {
        applyHeld();
        // Without a local player in a vehicle BF2HudManager::update leaves the widgets alone; the preview runs them.
        if (hudManager->getSystem())
            hudManager->getSystem()->update(seconds);
    }
    // The commands the HUD queued this frame, as BF2Engine's main loop runs them after its frame.
    hudManager->executeQueuedConsoleCommands();
}

bool HudEditor::resolveHeld(const std::string& name, const std::string& types, const std::string& value,
                            HeldVariable& out) const
{
    dice::hfe::HudInformationLayer* const layer = informationLayer();
    if (!layer)
        return false;
    out.name = name;
    out.intValue = 0;
    out.floatValue = 0.0f;
    char* end = 0;
    if (types.find("bool") != std::string::npos && layer->getBoolByName(name))
    {
        if (value != "0" && value != "1" && value != "true" && value != "false")
            return false;
        out.type = 1;
        out.intValue = value == "1" || value == "true" ? 1 : 0;
        return true;
    }
    if (types.find("int") != std::string::npos && layer->getIntByName(name))
    {
        out.type = 0;
        out.intValue = static_cast<int>(std::strtol(value.c_str(), &end, 10));
        return !value.empty() && !*end;
    }
    if (types.find("float") != std::string::npos && layer->getFloatByName(name))
    {
        out.type = 2;
        out.floatValue = static_cast<float>(std::strtod(value.c_str(), &end));
        return !value.empty() && !*end;
    }
    return false;
}

void HudEditor::applyHeld()
{
    dice::hfe::HudInformationLayer* const layer = informationLayer();
    if (!layer)
        return;
    // What shows the selected part first, then EditorHud.set's holds, which win.
    for (int pass = 0; pass < 2; ++pass)
    {
        const std::vector<HeldVariable>& list = pass == 0 ? m_autoHeld : m_held;
        for (size_t i = 0; i < list.size(); ++i)
        {
            const HeldVariable& held = list[i];
            if (held.type == 0)
            {
                if (int* value = layer->getIntByName(held.name))
                    *value = held.intValue;
            }
            else if (held.type == 1)
            {
                if (bool* value = layer->getBoolByName(held.name))
                    *value = held.intValue != 0;
            }
            else if (float* value = layer->getFloatByName(held.name))
                *value = held.floatValue;
        }
    }
}

void HudEditor::restoreAutoHeld()
{
    dice::hfe::HudInformationLayer* const layer = informationLayer();
    if (!layer)
        return;
    for (std::map<std::string, HeldVariable>::const_iterator it = m_saved.begin(); it != m_saved.end(); ++it)
    {
        const HeldVariable& saved = it->second;
        if (saved.type == 0)
        {
            if (int* value = layer->getIntByName(saved.name))
                *value = saved.intValue;
        }
        else if (saved.type == 1)
        {
            if (bool* value = layer->getBoolByName(saved.name))
                *value = saved.intValue != 0;
        }
        else if (float* value = layer->getFloatByName(saved.name))
            *value = saved.floatValue;
    }
    m_saved.clear();
}

void HudEditor::select(int node)
{
    restoreAutoHeld();
    m_autoHeld.clear();
    m_selected = node >= 0 && static_cast<size_t>(node) < m_setup.nodes().size() ? node : -1;
    ++m_generation;
    if (m_selected < 0)
        return;
    dice::hfe::HudInformationLayer* const layer = informationLayer();
    const std::vector<HudShowCondition> conditions = m_setup.showConditions(static_cast<size_t>(m_selected));
    for (size_t i = 0; i < conditions.size(); ++i)
    {
        HeldVariable held;
        if (!resolveHeld(conditions[i].variable, conditions[i].type, conditions[i].value, held))
            continue;
        bool known = false;
        for (size_t k = 0; k < m_autoHeld.size() && !known; ++k)
            known = m_autoHeld[k].name == held.name;
        if (known)
            continue;               // the nearest row asking for it wins
        m_autoHeld.push_back(held);
        // Its value now, put back when the part is no longer selected.
        HeldVariable saved = held;
        if (layer)
        {
            if (held.type == 0 && layer->getIntByName(held.name))
                saved.intValue = *layer->getIntByName(held.name);
            else if (held.type == 1 && layer->getBoolByName(held.name))
                saved.intValue = *layer->getBoolByName(held.name) ? 1 : 0;
            else if (held.type == 2 && layer->getFloatByName(held.name))
                saved.floatValue = *layer->getFloatByName(held.name);
        }
        m_saved[held.name] = saved;
    }
}

bool HudEditor::canvasRect(size_t node, float out[4]) const
{
    using namespace dice::meme;
    if (node >= m_setup.nodes().size())
        return false;
    const HudNode& n = m_setup.nodes()[node];
    int rect[4] = {};
    if (!m_setup.rect(node, rect))
        return false;
    out[0] = static_cast<float>(rect[0]);
    out[1] = static_cast<float>(rect[1]);
    out[2] = static_cast<float>(rect[2]);
    out[3] = static_cast<float>(rect[3]);
    const std::string kind = lowerOf(n.kind);
    // Where the HUD's last update put it - the areas' slides and the parents' bindings included.
    if (kind != "minimap" && kind != "map")
        if (Bf2TransformNode* own = liveAs<Bf2TransformNode>(n.name))
        {
            const std::unordered_map<const void*, CanvasRect>::const_iterator it = updatedRects().find(own);
            if (it != updatedRects().end())
            {
                out[0] = it->second.x;
                out[1] = it->second.y;
                out[2] = it->second.width;
                out[3] = it->second.height;
                return true;
            }
            out[0] = own->getPosition().x;
            out[1] = own->getPosition().y;
            out[2] = own->getSize().x;
            out[3] = own->getSize().y;
        }
    // Not updated yet (hidden): the parents' positions: the scripts' transforms, then the area the scripts hang their parts on (a node the
    // HUD's own data makes - TopLeft, BottomRightStatic - taken as placed on the canvas itself).
    std::string parent = n.parent;
    for (int guard = 0; guard < 64 && !parent.empty(); ++guard)
    {
        const int at = m_setup.find(parent);
        TransformNode* transform = 0;
        if (at < 0 || (lowerOf(m_setup.nodes()[at].kind) != "minimap" && lowerOf(m_setup.nodes()[at].kind) != "map"))
            transform = liveAs<TransformNode>(parent);
        if (transform)
        {
            out[0] += transform->getPosition().x;
            out[1] += transform->getPosition().y;
        }
        if (at < 0)
            break;
        parent = m_setup.nodes()[at].parent;
    }
    return true;
}

bool HudEditor::rowText(const HudRef& ref, std::string& words) const
{
    const HudCommand& command = m_setup.command(ref);
    const std::string& line = m_setup.script(ref.script).line(command.line);
    const std::vector<HudWord> all = HudScript::words(line);
    words = all.size() >= 2 ? line.substr(all[1].begin, all.back().end - all[1].begin) : std::string();
    return true;
}

bool HudEditor::runEdit(const std::string& label, const std::string& doScript, const std::string& undoScript,
                        std::string& result)
{
    HistoryEntry entry;
    entry.label = label;
    entry.doScript = doScript;
    entry.undoScript = undoScript;
    std::string error;
    if (!m_session.history().execute(entry, error))
    {
        result = error;
        logLine(LogLevel::Warning, "hud: %s: %s", label.c_str(), error.c_str());
        return false;
    }
    result = label + (m_lastNote.empty() ? std::string() : " (" + m_lastNote + ")");
    return true;
}

bool HudEditor::applyLiveRect(size_t node, const int rect[4])
{
    using namespace dice::meme;
    const HudNode& n = m_setup.nodes()[node];
    const std::string kind = lowerOf(n.kind);
    if (kind == "minimap" || kind == "map")
        return false;           // their transform takes the parent's name
    Bf2TransformNode* transform = liveAs<Bf2TransformNode>(n.name);
    if (!transform)
        return false;
    transform->setPositionForHud(static_cast<float>(rect[0]), static_cast<float>(rect[1]));
    transform->setSizeForHud(static_cast<float>(rect[2]), static_cast<float>(rect[3]));
    return true;
}

void HudEditor::applyLive(size_t script, size_t line, std::string& note)
{
    note.clear();
    if (!dice::hfe::hudManager)
        return;
    const char* const later = "the live HUD shows it after the editor starts again";
    // The command now on that line, if any.
    const std::vector<HudCommand>& commands = m_setup.script(script).commands();
    size_t index = commands.size();
    for (size_t i = 0; i < commands.size(); ++i)
        if (commands[i].line == line)
            index = i;
    if (index == commands.size())
    {
        note = later;
        return;
    }
    const HudCommand& command = commands[index];
    const HudRef ref = { script, index };
    for (size_t n = 0; n < m_setup.nodes().size(); ++n)
    {
        const HudNode& node = m_setup.nodes()[n];
        if (node.create.script == script && node.create.command == index)
        {
            int rect[4] = {};
            note = m_setup.rect(n, rect) && applyLiveRect(n, rect) ? "applied to the live HUD" : later;
            return;
        }
    }
    const LiveRow* row = lowerOf(command.object) == "hudbuilder" ? liveRow(command.method) : 0;
    std::string parent, name;
    if (!row || !m_setup.liveSelection(ref, parent, name))
    {
        note = later;
        return;
    }
    bool wellFormed = command.args.size() == static_cast<size_t>(row->words);
    for (int i = 0; i < row->numbers && wellFormed; ++i)
        wellFormed = isNumber(command.args[i].text);
    if (!wellFormed)
    {
        note = "not applied to the live HUD: the row's words do not fit it";
        return;
    }
    if (!liveTypeMatches(row->type, name))
    {
        note = "not applied to the live HUD: it has no such part (" + name + ")";
        return;
    }
    // The selection the script ran the row with, then the row itself.
    engineLine("hudBuilder.setActiveObject " + HudScript::spell(parent, false) + " " + HudScript::spell(name, false));
    std::string words;
    rowText(ref, words);
    engineLine("hudBuilder." + command.method + " " + words);
    note = "applied to the live HUD";
    const std::string low = lowerOf(command.method);
    if (low == "setpicturenodetexture" || low == "setbarnodetexture" || low == "setbuttonnodetexture"
        || low == "setcompassnodetexture")
    {
        const std::string texture = command.args.back().text;
        if (!texture.empty() && !engineFileExists("Menu/HUD/Texture/" + texture))
            note += "; Menu/HUD/Texture/" + texture + " is not there, so nothing draws";
    }
}

// ---- the panel ------------------------------------------------------------------------------------------------------

void HudEditor::drawTree(const std::string& parent, int depth)
{
    if (depth > 32)
        return;
    const std::vector<size_t> children = m_setup.children(parent);
    for (size_t i = 0; i < children.size(); ++i)
    {
        const HudNode& node = m_setup.nodes()[children[i]];
        const bool leaf = m_setup.children(node.name).empty();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (leaf)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (static_cast<int>(children[i]) == m_selected)
            flags |= ImGuiTreeNodeFlags_Selected;
        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(children[i] + 1), flags, "%s  (%s)",
                                            node.name.c_str(), node.kind.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            select(static_cast<int>(children[i]));
        if (open && !leaf)
        {
            drawTree(node.name, depth + 1);
            ImGui::TreePop();
        }
    }
}

void HudEditor::drawPanel(bool* open, const float view[4], bool inGame)
{
    if (!ImGui::Begin("HUD editor", open))
    {
        ImGui::End();
        claimViewMouse(false);
        return;
    }
    if (!m_loadError.empty())
        ImGui::TextWrapped("No HUD scripts: %s", m_loadError.c_str());
    bool preview = m_preview;
    if (ImGui::Checkbox("Preview the HUD", &preview))
    {
        std::string result;
        setHudPreview(preview, result);
    }
    ImGui::SetItemTooltip("The game's HUD drawn over the view without the play test; the selected part's show variables "
                          "are held so it appears. Drag the outlined part in the view to move it, its corner to size it.");
    ImGui::SameLine();
    const std::vector<size_t> changed = m_setup.changedScripts();
    ImGui::TextDisabled("%u scripts, %u parts, %u changed", static_cast<unsigned>(m_setup.scriptCount()),
                        static_cast<unsigned>(m_setup.nodes().size()), static_cast<unsigned>(changed.size()));

    // Real game: the play test, where the game itself drives the HUD.
    if (ImGui::CollapsingHeader("Real game"))
    {
        ImGui::TextWrapped("The play test drives the HUD as the game does; edits made here show in it at once.");
        if (ImGui::Button("Play as a soldier"))
            m_session.runCommand("EditorPlayTest.soldier", "hud");
        ImGui::SameLine();
        if (ImGui::Button("Drive the selected vehicle spawner"))
            m_session.runCommand("EditorPlayTest.drive", "hud");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##driveTemplate", "vehicle template, e.g. usaav_m6", m_driveTemplate,
                                 sizeof(m_driveTemplate));
        ImGui::SameLine();
        if (ImGui::Button("Drive it") && m_driveTemplate[0])
            m_session.runCommand(std::string("EditorPlayTest.driveTemplate ") + m_driveTemplate, "hud");
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##hudFilter", "find a part by name", m_filter, sizeof(m_filter));
    ImGui::BeginChild("##hudTree", ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.42f), ImGuiChildFlags_Borders);
    if (m_filter[0])
    {
        const std::string wanted = lowerOf(m_filter);
        for (size_t i = 0; i < m_setup.nodes().size(); ++i)
        {
            const HudNode& node = m_setup.nodes()[i];
            if (lowerOf(node.name).find(wanted) == std::string::npos)
                continue;
            char label[160];
            std::snprintf(label, sizeof(label), "%s  (%s, in %s)##%u", node.name.c_str(), node.kind.c_str(),
                          node.parent.c_str(), static_cast<unsigned>(i));
            if (ImGui::Selectable(label, static_cast<int>(i) == m_selected))
                select(static_cast<int>(i));
        }
    }
    else
    {
        // The vehicle and weapon HUDs by the index their templates name, then every area's parts.
        if (ImGui::TreeNodeEx("Vehicle and weapon HUDs (GuiIndex)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (size_t i = 0; i < m_setup.nodes().size(); ++i)
            {
                const HudNode& node = m_setup.nodes()[i];
                for (size_t c = 0; c < node.commands.size(); ++c)
                {
                    const HudCommand& row = m_setup.command(node.commands[c]);
                    if (lowerOf(row.method) != "setnodelogicshowvariable" || row.args.size() < 3
                        || lowerOf(row.args[0].text) != "equal" || row.args[1].text != "GuiIndex")
                        continue;
                    char label[160];
                    std::snprintf(label, sizeof(label), "%s  (GuiIndex %s)##g%u", node.name.c_str(),
                                  row.args[2].text.c_str(), static_cast<unsigned>(i));
                    if (ImGui::Selectable(label, static_cast<int>(i) == m_selected))
                        select(static_cast<int>(i));
                    break;
                }
            }
            ImGui::TreePop();
        }
        // The areas: parents no script creates (the HUD's own data makes them).
        std::vector<std::string> roots;
        for (size_t i = 0; i < m_setup.nodes().size(); ++i)
        {
            const std::string& parent = m_setup.nodes()[i].parent;
            if (m_setup.find(parent) < 0 && std::find(roots.begin(), roots.end(), parent) == roots.end())
                roots.push_back(parent);
        }
        for (size_t r = 0; r < roots.size(); ++r)
            if (ImGui::TreeNodeEx(roots[r].c_str(), ImGuiTreeNodeFlags_SpanAvailWidth, "%s  (area)", roots[r].c_str()))
            {
                drawTree(roots[r], 0);
                ImGui::TreePop();
            }
    }
    ImGui::EndChild();

    if (m_selected >= 0 && static_cast<size_t>(m_selected) < m_setup.nodes().size())
    {
        const HudNode node = m_setup.nodes()[m_selected];
        const HudCommand& create = m_setup.command(node.create);
        ImGui::Text("%s", node.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s in %s - %s line %u", node.kind.c_str(), node.parent.c_str(),
                            m_setup.scriptPath(node.create.script).c_str(), static_cast<unsigned>(create.line + 1));
        // The rectangle: dragging a field moves the live part at once; the script changes when the drag ends.
        int rect[4] = {};
        if (m_setup.rect(m_selected, rect))
        {
            ImGui::SetNextItemWidth(-1.0f);
            if (!ImGui::IsAnyItemActive() || m_rowEditsFor != m_generation)
                std::copy(rect, rect + 4, m_rectEdit);
            if (ImGui::DragInt4("##rect", m_rectEdit, 1.0f))
                applyLiveRect(m_selected, m_rectEdit);
            ImGui::SetItemTooltip("x, y, width, height on the 800x600 HUD canvas, from the parent's corner");
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                std::string result;
                setHudRect(node.name, m_rectEdit[0], m_rectEdit[1], m_rectEdit[2], m_rectEdit[3], result);
            }
        }
        // The rows acting on it, each editable as the script spells its words.
        if (m_rowEditsFor != m_generation)
        {
            m_rowEdits.assign(node.commands.size(), std::string());
            for (size_t i = 0; i < node.commands.size(); ++i)
                rowText(node.commands[i], m_rowEdits[i]);
            m_rowEditsFor = m_generation;
        }
        int removeRow = -1;
        if (ImGui::BeginTable("##hudRows", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            for (size_t i = 0; i < node.commands.size() && i < m_rowEdits.size(); ++i)
            {
                const HudCommand& row = m_setup.command(node.commands[i]);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.method.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s line %u", m_setup.scriptPath(node.commands[i].script).c_str(),
                                      static_cast<unsigned>(row.line + 1));
                ImGui::TableNextColumn();
                char buffer[512];
                std::snprintf(buffer, sizeof(buffer), "%s", m_rowEdits[i].c_str());
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::InputText("##words", buffer, sizeof(buffer)))
                    m_rowEdits[i] = buffer;
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    std::string result;
                    setHudRow(node.name, static_cast<int>(i), m_rowEdits[i], result);
                }
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("x"))
                    removeRow = static_cast<int>(i);
                ImGui::SetItemTooltip("Take the row out");
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeRow >= 0)
        {
            std::string result;
            removeHudRow(node.name, removeRow, result);
        }
        // A new row.
        ImGui::SetNextItemWidth(170.0f);
        if (ImGui::BeginCombo("##addMethod", m_addMethod[0] ? m_addMethod : "add a row..."))
        {
            for (size_t i = 0; i < sizeof(kRowTemplates) / sizeof(kRowTemplates[0]); ++i)
                if (ImGui::Selectable(kRowTemplates[i][0]))
                {
                    std::snprintf(m_addMethod, sizeof(m_addMethod), "%s", kRowTemplates[i][0]);
                    std::snprintf(m_addWords, sizeof(m_addWords), "%s", kRowTemplates[i][1]);
                }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-60.0f);
        ImGui::InputText("##addWords", m_addWords, sizeof(m_addWords));
        ImGui::SameLine();
        if (ImGui::Button("Add") && m_addMethod[0])
        {
            std::string result;
            addHudRow(node.name, m_addMethod, m_addWords, result);
        }
        // What shows it, and what the last edit did.
        const std::vector<HudShowCondition> show = m_setup.showConditions(m_selected);
        if (!show.empty())
        {
            ImGui::TextDisabled("Shown when:");
            for (size_t i = 0; i < show.size(); ++i)
                ImGui::BulletText("%s = %s  (%s)", show[i].variable.c_str(), show[i].value.c_str(), show[i].node.c_str());
        }
        // The two sliding areas take their place from the soldier's and vehicle's own HUD code, which runs only with a
        // player: the preview cannot put their parts where the game does.
        std::string area = node.parent;
        for (int guard = 0; guard < 64; ++guard)
        {
            const int up = m_setup.find(area);
            if (up < 0)
                break;
            area = m_setup.nodes()[up].parent;
        }
        if (area == "BottomLeftAnimate" || area == "BottomRightAnimate")
            ImGui::TextWrapped("%s slides in with the soldier or vehicle: the preview cannot place it as the game does. "
                               "Use Real game, press Esc to free the mouse, and edit it there.", area.c_str());
        if (!m_lastNote.empty())
            ImGui::TextWrapped("Last edit: %s", m_lastNote.c_str());
    }
    ImGui::End();

    // The outline: over the preview, and over the play test (where the game places every part) while the mouse is free.
    if ((m_preview || inGame) && m_selected >= 0)
        drawOverlay(view);
    else
        claimViewMouse(false);
}

void HudEditor::drawOverlay(const float view[4])
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float sx = viewport->Size.x / kCanvasWidth;
    const float sy = viewport->Size.y / kCanvasHeight;
    const ImVec2 origin = viewport->Pos;
    ImDrawList* draw = ImGui::GetBackgroundDrawList(const_cast<ImGuiViewport*>(viewport));
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool inView = mouse.x >= view[0] && mouse.y >= view[1] && mouse.x < view[0] + view[2]
                        && mouse.y < view[1] + view[3];

    // The selected part's children, faint, then the part itself with its size handle.
    const std::string name = m_setup.nodes()[m_selected].name;
    const std::vector<size_t> children = m_setup.children(name);
    for (size_t i = 0; i < children.size(); ++i)
    {
        float c[4];
        if (canvasRect(children[i], c))
            draw->AddRect(ImVec2(origin.x + c[0] * sx, origin.y + c[1] * sy),
                          ImVec2(origin.x + (c[0] + c[2]) * sx, origin.y + (c[1] + c[3]) * sy), IM_COL32(120, 200, 255, 110));
    }
    float r[4];
    if (!canvasRect(m_selected, r))
    {
        claimViewMouse(false);
        return;
    }
    const ImVec2 a(origin.x + r[0] * sx, origin.y + r[1] * sy);
    const ImVec2 b(origin.x + (r[0] + r[2]) * sx, origin.y + (r[1] + r[3]) * sy);
    const ImVec2 handle(b.x - 8.0f, b.y - 8.0f);
    draw->AddRect(a, b, IM_COL32(255, 220, 60, 255), 0.0f, 2.0f);
    draw->AddRectFilled(handle, b, IM_COL32(255, 220, 60, 255));
    draw->AddText(ImVec2(a.x, a.y - 16.0f), IM_COL32(255, 220, 60, 255), name.c_str());

    const bool overHandle = mouse.x >= handle.x - 2.0f && mouse.y >= handle.y - 2.0f && mouse.x <= b.x + 2.0f
                            && mouse.y <= b.y + 2.0f;
    const bool overPart = mouse.x >= a.x && mouse.y >= a.y && mouse.x <= b.x && mouse.y <= b.y;
    int rect[4] = {};
    const bool hasRect = m_setup.rect(m_selected, rect);
    // The view's own tools step back while the pointer is on the part (or a drag is under way).
    const bool uiUnder = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    claimViewMouse(m_dragMode != 0 || (inView && !uiUnder && hasRect && (overPart || overHandle)));
    if (!hasRect)
        return;
    if (m_dragMode == 0 && inView && !uiUnder && (overPart || overHandle) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_dragMode = overHandle ? 2 : 1;
        std::copy(rect, rect + 4, m_dragStart);
        m_dragMouse[0] = mouse.x;
        m_dragMouse[1] = mouse.y;
    }
    if (m_dragMode != 0)
    {
        const int dx = static_cast<int>(std::floor((mouse.x - m_dragMouse[0]) / sx + 0.5f));
        const int dy = static_cast<int>(std::floor((mouse.y - m_dragMouse[1]) / sy + 0.5f));
        int moved[4] = { m_dragStart[0], m_dragStart[1], m_dragStart[2], m_dragStart[3] };
        if (m_dragMode == 1)
        {
            moved[0] += dx;
            moved[1] += dy;
        }
        else
        {
            moved[2] = std::max(1, moved[2] + dx);
            moved[3] = std::max(1, moved[3] + dy);
        }
        applyLiveRect(m_selected, moved);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_dragMode = 0;
            if (moved[0] != rect[0] || moved[1] != rect[1] || moved[2] != rect[2] || moved[3] != rect[3])
            {
                std::string result;
                setHudRect(name, moved[0], moved[1], moved[2], moved[3], result);
            }
        }
    }
}

// ---- the console rows -----------------------------------------------------------------------------------------------

std::string HudEditor::describeHud() const
{
    using namespace dice::hfe;
    std::ostringstream out;
    out << "HUD " << (m_inGame ? "in the play test" : m_preview ? "previewed" : "hidden");
    if (hudManager)
        out << ", mode " << hudManager->getMode();
    out << ", " << m_setup.scriptCount() << " scripts, " << m_setup.nodes().size() << " parts, "
        << m_setup.changedScripts().size() << " changed";
    if (m_selected >= 0)
        out << ", selected " << m_setup.nodes()[m_selected].name;
    for (int pass = 0; pass < 2; ++pass)
    {
        const std::vector<HeldVariable>& list = pass == 0 ? m_autoHeld : m_held;
        if (list.empty())
            continue;
        out << (pass == 0 ? ", showing it:" : ", held:");
        for (size_t i = 0; i < list.size(); ++i)
        {
            const HeldVariable& held = list[i];
            out << (i ? ", " : " ") << held.name << " = ";
            if (held.type == 2)
                out << held.floatValue;
            else if (held.type == 1)
                out << (held.intValue ? "true" : "false");
            else
                out << held.intValue;
        }
    }
    return out.str();
}

bool HudEditor::setHudPreview(bool on, std::string& result)
{
    m_preview = on;
    if (!on)
        restoreAutoHeld();
    else
        select(m_selected);     // what shows the selected part held again
    result = on ? "the HUD is previewed (drawn without the play test, the in-game mode)" : "the HUD preview is off";
    logLine(LogLevel::Info, "hud: %s", result.c_str());
    return true;
}

bool HudEditor::setHudVariable(const std::string& name, const std::string& value, std::string& result)
{
    for (size_t i = 0; i < m_held.size(); ++i)
        if (m_held[i].name == name)
        {
            m_held.erase(m_held.begin() + i);
            break;
        }
    if (value == "-")
    {
        result = name + " is no longer held";
        return true;
    }
    if (!informationLayer())
    {
        result = "the HUD has no information layer";
        return false;
    }
    HeldVariable held;
    if (!resolveHeld(name, "bool|int|float", value, held))
    {
        result = "the HUD has no int, bool or float variable " + name + " that takes " + value;
        return false;
    }
    m_held.push_back(held);
    applyHeld();
    result = name + " held at " + value;
    return true;
}

bool HudEditor::selectHudNode(const std::string& name, std::string& result)
{
    if (name == "-")
    {
        select(-1);
        result = "no part selected";
        return true;
    }
    const int node = m_setup.find(name);
    if (node < 0)
    {
        result = "no HUD part " + name;
        return false;
    }
    select(node);
    result = "selected " + name + " (" + m_setup.nodes()[node].kind + ")";
    return true;
}

std::string HudEditor::describeHudNode(const std::string& name) const
{
    const int at = m_setup.find(name);
    if (at < 0)
        return "no HUD part " + name;
    const HudNode& node = m_setup.nodes()[at];
    std::ostringstream out;
    out << node.name << " (" << node.kind << ") in " << node.parent << ", " << m_setup.scriptPath(node.create.script)
        << " line " << m_setup.command(node.create).line + 1;
    int rect[4] = {};
    if (m_setup.rect(at, rect))
        out << ", rect " << rect[0] << "/" << rect[1] << "/" << rect[2] << "/" << rect[3];
    float canvas[4] = {};
    if (canvasRect(at, canvas))
        out << ", on the canvas " << canvas[0] << "/" << canvas[1] << "/" << canvas[2] << "/" << canvas[3];
    out << (liveExists(node.name) ? ", live" : ", not in the live HUD");
    for (size_t i = 0; i < node.commands.size(); ++i)
    {
        std::string words;
        rowText(node.commands[i], words);
        out << "\n  row " << i << ": " << m_setup.command(node.commands[i]).method << " " << words;
    }
    const std::vector<HudShowCondition> show = m_setup.showConditions(at);
    for (size_t i = 0; i < show.size(); ++i)
        out << "\n  shown when " << show[i].variable << " = " << show[i].value << " (" << show[i].node << ")";
    return out.str();
}

bool HudEditor::editHudLine(int op, int script, int line, const std::string& text, std::string& result)
{
    std::string error;
    bool ok = false;
    if (script < 0 || line < 0)
        error = "no such line";
    else if (op == 0)
        ok = m_setup.setLine(script, line, decodeLine(text), error);
    else if (op == 1)
        ok = m_setup.insertLine(script, line, decodeLine(text), error);
    else
        ok = m_setup.removeLine(script, line, error);
    if (!ok)
    {
        result = error;
        return false;
    }
    ++m_generation;
    m_lastNote.clear();
    if (op != 2)
        applyLive(script, line, m_lastNote);
    else
        m_lastNote = "the live HUD keeps the row until the editor starts again";
    char where[64];
    std::snprintf(where, sizeof(where), " line %d", line + 1);
    result = m_setup.scriptPath(script) + where + ": " + m_lastNote;
    logLine(LogLevel::Info, "hud: %s", result.c_str());
    return true;
}

bool HudEditor::setHudRect(const std::string& name, int x, int y, int width, int height, std::string& result)
{
    const int at = m_setup.find(name);
    int rect[4] = {};
    if (at < 0 || !m_setup.rect(at, rect))
    {
        result = at < 0 ? "no HUD part " + name : name + " has no rectangle in its create row";
        return false;
    }
    const HudNode& node = m_setup.nodes()[at];
    const HudCommand& create = m_setup.command(node.create);
    const std::string before = m_setup.script(node.create.script).line(create.line);
    // The new line through a copy of it, so only its four words change.
    HudScript copy;
    copy.parse(before);
    const int value[4] = { x, y, width, height };
    for (int i = 3; i >= 0; --i)
        copy.setWord(0, static_cast<size_t>(node.rectWord + i), std::to_string(value[i]));
    const std::string after = copy.line(0);
    if (after == before)
    {
        result = name + " is already there";
        return true;
    }
    char rows[128];
    std::snprintf(rows, sizeof(rows), "EditorHud.setLine %u %u ", static_cast<unsigned>(node.create.script),
                  static_cast<unsigned>(create.line));
    char label[160];
    std::snprintf(label, sizeof(label), "HUD: %s to %d/%d, %dx%d", name.c_str(), x, y, width, height);
    return runEdit(label, rows + encodeLine(after), rows + encodeLine(before), result);
}

bool HudEditor::setHudRow(const std::string& name, int row, const std::string& words, std::string& result)
{
    const int at = m_setup.find(name);
    if (at < 0 || row < 0 || static_cast<size_t>(row) >= m_setup.nodes()[at].commands.size())
    {
        result = at < 0 ? "no HUD part " + name : name + " has no such row";
        return false;
    }
    const HudRef ref = m_setup.nodes()[at].commands[row];
    const HudCommand& command = m_setup.command(ref);
    const std::string before = m_setup.script(ref.script).line(command.line);
    const std::vector<HudWord> all = HudScript::words(before);
    // The method word and the gap after it stay as written; the words follow.
    const std::string head = all.size() >= 2 ? before.substr(0, all[1].begin) : before + (words.empty() ? "" : " ");
    const std::string after = head + words;
    if (after == before)
    {
        result = "no change";
        return true;
    }
    char rows[128];
    std::snprintf(rows, sizeof(rows), "EditorHud.setLine %u %u ", static_cast<unsigned>(ref.script),
                  static_cast<unsigned>(command.line));
    return runEdit("HUD: " + name + " " + command.method, rows + encodeLine(after), rows + encodeLine(before), result);
}

bool HudEditor::addHudRow(const std::string& name, const std::string& method, const std::string& words,
                          std::string& result)
{
    const int at = m_setup.find(name);
    size_t script = 0, line = 0;
    if (at < 0 || !m_setup.newRowPlace(at, script, line))
    {
        result = "no HUD part " + name;
        return false;
    }
    if (method.empty() || method.find_first_of(" \t\"") != std::string::npos)
    {
        result = "a row's method is one word, e.g. setNodeColor";
        return false;
    }
    const std::string low = lowerOf(method);
    if (low.compare(0, 6, "create") == 0 || low == "setactiveobject")
    {
        result = "a row acts on the part; new parts and selections are not made through rows yet";
        return false;
    }
    // The separator the line before uses after its method word.
    const std::string previous = m_setup.script(script).line(line - 1);
    const std::vector<HudWord> all = HudScript::words(previous);
    const std::string separator = all.size() >= 2 ? previous.substr(all[0].end, all[1].begin - all[0].end) : "\t";
    const std::string object = m_setup.command(m_setup.nodes()[at].create).object;
    const std::string text = object + "." + method + (words.empty() ? std::string() : separator + words);
    char doRow[96], undoRow[96];
    std::snprintf(doRow, sizeof(doRow), "EditorHud.insertLine %u %u ", static_cast<unsigned>(script),
                  static_cast<unsigned>(line));
    std::snprintf(undoRow, sizeof(undoRow), "EditorHud.removeLine %u %u", static_cast<unsigned>(script),
                  static_cast<unsigned>(line));
    return runEdit("HUD: " + name + " + " + method, doRow + encodeLine(text), undoRow, result);
}

bool HudEditor::removeHudRow(const std::string& name, int row, std::string& result)
{
    const int at = m_setup.find(name);
    if (at < 0 || row < 0 || static_cast<size_t>(row) >= m_setup.nodes()[at].commands.size())
    {
        result = at < 0 ? "no HUD part " + name : name + " has no such row";
        return false;
    }
    const HudRef ref = m_setup.nodes()[at].commands[row];
    const HudCommand& command = m_setup.command(ref);
    const std::string text = m_setup.script(ref.script).line(command.line);
    char doRow[96], undoRow[96];
    std::snprintf(doRow, sizeof(doRow), "EditorHud.removeLine %u %u", static_cast<unsigned>(ref.script),
                  static_cast<unsigned>(command.line));
    std::snprintf(undoRow, sizeof(undoRow), "EditorHud.insertLine %u %u ", static_cast<unsigned>(ref.script),
                  static_cast<unsigned>(command.line));
    return runEdit("HUD: " + name + " - " + command.method, doRow, undoRow + encodeLine(text), result);
}

void HudEditor::planSave(SavePlan& plan)
{
    using namespace dice::hfe;
    const std::vector<size_t> changed = m_setup.changedScripts();
    if (changed.empty())
        return;
    SavePart& part = plan.part("HUD scripts");
    const std::string mod = lowerOf(m_session.modFolder()) + "/";
    // archive -> entry -> new text, and the mount folders whose archives are written.
    std::map<std::string, std::map<std::string, std::string> > entries;
    std::vector<std::string> mounts;
    std::string names, written;
    for (size_t i = 0; i < changed.size(); ++i)
    {
        const std::string& path = m_setup.scriptPath(changed[i]);
        const std::string text = m_setup.script(changed[i]).render();
        names += (i ? ", " : "") + path;
        // Every archive mounted on its folder that holds it - the game reads the first, other images other ones.
        const size_t slash = path.find_last_of('/');
        std::vector<io::IArchive*>* archives =
            io::fileManager->findArchives(slash == std::string::npos ? std::string() : path.substr(0, slash + 1));
        bool held = false;
        for (size_t a = 0; archives && a < archives->size(); ++a)
        {
            if (!(*archives)[a]->fileExists(path))
                continue;
            const std::string archive = static_cast<io::ZipArchive*>((*archives)[a])->getFilename();
            std::string entry, before;
            if (!archiveEntryOf(archive, path, entry, before))
            {
                part.refuse("the HUD scripts NOT saved: " + path + " cannot be read in " + archive);
                return;
            }
            if (before != m_setup.script(changed[i]).original())
            {
                part.refuse("the HUD scripts NOT saved: " + path + " in " + archive
                            + " changed on disk since the editor read it");
                return;
            }
            if (lowerOf(archive).compare(0, mod.size(), mod) != 0)
            {
                part.refuse("the HUD scripts NOT saved: " + path + " comes from " + archive + ", outside the mod "
                            + m_session.modFolder() + " - give the mod its own copy of that archive first");
                return;
            }
            entries[archive][entry] = text;
            const std::string mount = path.substr(0, path.size() - entry.size());
            if (std::find(mounts.begin(), mounts.end(), mount) == mounts.end())
                mounts.push_back(mount);
            held = true;
        }
        if (!held)
        {
            // No archive holds it: the mod's loose file.
            const std::string loose = m_session.modFolder() + "/" + path;
            if (!fileOnDisk(loose))
            {
                part.refuse("the HUD scripts NOT saved: no archive of " + path + " and no " + loose);
                return;
            }
            part.add(loose, text);
            written += (written.empty() ? "" : ", ") + loose;
        }
    }
    // Each archive written again into the editor's staging folder: the changed entries replaced, the rest as stored.
    const std::string staging = "bf2editor/Temp/HudSave";
    makeFolders(staging);
    static unsigned serial = 0;
    std::vector<std::string> staged;
    for (std::map<std::string, std::map<std::string, std::string> >::const_iterator it = entries.begin();
         it != entries.end(); ++it)
    {
        const size_t slash = it->first.find_last_of("/\\");
        char prefix[48];
        std::snprintf(prefix, sizeof(prefix), "%lu_%u_", static_cast<unsigned long>(std::time(0)), ++serial);
        const std::string name = staging + "/" + prefix + (slash == std::string::npos ? it->first : it->first.substr(slash + 1));
        std::vector<std::string> missing;
        std::string error;
        if (!rewriteArchive(it->first, name, it->second, missing, error))
        {
            part.refuse("the HUD scripts NOT saved: " + error);
            return;
        }
        part.addStaged(it->first, name);
        staged.push_back(name);
        written += (written.empty() ? "" : ", ") + it->first;
    }
    // The engine keeps its archives open: it lets go of every archive on those mounts while they are replaced, and
    // mounts them again in their order after (their entries are read again - the written ones moved).
    std::shared_ptr<std::vector<ReleasedArchive> > released(new std::vector<ReleasedArchive>());
    part.onReplacing([mounts, released]() {
        for (size_t m = 0; m < mounts.size(); ++m)
        {
            std::vector<io::IArchive*>* archives = io::fileManager->findArchives(mounts[m]);
            if (!archives)
                continue;
            std::vector<ReleasedArchive> these;
            for (size_t a = 0; a < archives->size(); ++a)
            {
                ReleasedArchive archive;
                archive.filename = static_cast<io::ZipArchive*>((*archives)[a])->getFilename();
                archive.mount = mounts[m].substr(0, mounts[m].size() - 1);
                archive.contentCheck = (*archives)[a]->getContentCheck();
                these.push_back(archive);
            }
            for (size_t a = 0; a < these.size(); ++a)
                io::fileManager->unmountArchive("*" + lowerOf(these[a].filename));
            released->insert(released->end(), these.begin(), these.end());
        }
    });
    part.onReplaced([released]() {
        for (size_t a = 0; a < released->size(); ++a)
            mountAgain((*released)[a]);
        released->clear();
    });
    part.onWritten([this, changed, staged]() {
        for (size_t i = 0; i < changed.size(); ++i)
            m_setup.markSaved(changed[i]);
        for (size_t i = 0; i < staged.size(); ++i)
            std::remove(staged[i].c_str());
    });
    const std::string report = "saved the HUD scripts " + names + " into " + written;
    part.onReport([report](const SavePart&) { return report; });
}

bool HudEditor::hudScreenPoint(const std::string& name, int which, float& x, float& y, std::string& error) const
{
    const int node = m_setup.find(name);
    float r[4];
    if (node < 0 || !canvasRect(node, r))
    {
        error = node < 0 ? "no HUD part " + name : name + " has no rectangle";
        return false;
    }
    // As drawOverlay maps the canvas onto the window.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float sx = viewport->Size.x / kCanvasWidth;
    const float sy = viewport->Size.y / kCanvasHeight;
    if (which == 1)
    {
        x = viewport->Pos.x + (r[0] + r[2]) * sx - 4.0f;
        y = viewport->Pos.y + (r[1] + r[3]) * sy - 4.0f;
    }
    else
    {
        x = viewport->Pos.x + (r[0] + r[2] * 0.5f) * sx;
        y = viewport->Pos.y + (r[1] + r[3] * 0.5f) * sy;
    }
    return true;
}

std::string HudEditor::unsavedText() const
{
    const std::vector<size_t> changed = m_setup.changedScripts();
    if (changed.empty())
        return std::string();
    std::string text = " The HUD scripts changed:";
    for (size_t i = 0; i < changed.size(); ++i)
        text += (i ? ", " : " ") + m_setup.scriptPath(changed[i]);
    return text + ".";
}

std::string HudEditor::describeHudChanges() const
{
    const std::vector<size_t> changed = m_setup.changedScripts();
    if (changed.empty())
        return "the HUD scripts are as loaded";
    std::ostringstream out;
    out << changed.size() << " HUD script(s) changed:";
    for (size_t i = 0; i < changed.size(); ++i)
    {
        const std::string& path = m_setup.scriptPath(changed[i]);
        out << " " << path;
        const size_t slash = path.find_last_of('/');
        std::vector<dice::hfe::io::IArchive*>* archives =
            dice::hfe::io::fileManager->findArchives(slash == std::string::npos ? std::string() : path.substr(0, slash + 1));
        if (!archives)
            continue;
        for (size_t a = 0; a < archives->size(); ++a)
            out << " [archive " << a << " of " << archives->size() << ": "
                << static_cast<dice::hfe::io::ZipArchive*>((*archives)[a])->getFilename()
                << ((*archives)[a]->fileExists(path) ? ", has it]" : "]");
    }
    const std::list<std::string>* paths = static_cast<const std::list<std::string>*>(dice::hfe::io::fileManager->getPathList());
    if (paths)
        for (std::list<std::string>::const_iterator it = paths->begin(); it != paths->end(); ++it)
            out << " {path " << *it << "}";
    return out.str();
}

} // namespace bf2editor
