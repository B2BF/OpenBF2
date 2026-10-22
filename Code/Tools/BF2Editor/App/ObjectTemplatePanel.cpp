// Code/Tools/BF2Editor/App/ObjectTemplatePanel.cpp - see ObjectTemplatePanel.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/ObjectTemplatePanel.h"

#include "App/EditSession.h"
#include "App/PropertyPickers.h"
#include "App/TestHarness.h"
#include "App/TexturePalette.h"
#include "Framework/EditorConsole.h"
#include "Framework/EditorLog.h"

#include "Core/ConsoleObjects.h"
#include "Core/Ids.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/IObjectTemplate.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

namespace bf2editor
{

namespace
{

const ImVec4 kWarning(1.0f, 0.45f, 0.4f, 1.0f);
const ImVec4 kNote(1.0f, 0.8f, 0.4f, 1.0f);

void copyText(char* out, size_t size, const std::string& text)
{
    std::strncpy(out, text.c_str(), size - 1);
    out[size - 1] = 0;
}

// A console argument that may hold blanks.
std::string quoted(const std::string& text)
{
    return text.find_first_of(" \t") == std::string::npos ? text : "\"" + text + "\"";
}

std::string lowered(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

// The console binding of ObjectTemplate.<property>: its argument types and, when the engine gives
// it one, its range. The registry walked once per property name. A name can be bound once per class family with
// different types (timeToLive is a float on particle systems and a random distribution on emitters), and which one a
// line reaches depends on the class that is active, so the types are kept only when every binding of the name agrees
// (and the range only when every binding has the same one); otherwise the property edits as text. Empty when the
// engine binds no such property. The set/get prefix the files may spell is tried off too.
struct PropertyBinding
{
    std::vector<std::string> types;
    bool                     ranged = false;
    float                    low = 0.0f, high = 0.0f;
};

std::string bareProperty(const std::string& property)
{
    std::string bare = lowered(property);
    if (bare.size() > 3 && (bare.compare(0, 3, "set") == 0 || bare.compare(0, 3, "get") == 0))
        bare = bare.substr(3);
    return bare;
}

const PropertyBinding& propertyBinding(const std::string& property)
{
    static std::map<std::string, PropertyBinding> cache;
    const std::string key = lowered(property);
    std::map<std::string, PropertyBinding>::const_iterator found = cache.find(key);
    if (found != cache.end())
        return found->second;
    PropertyBinding& binding = cache[key];
    if (!dice::hfe::mainConsoleObjects)
        return binding;
    const std::string bare = bareProperty(property);
    const dice::hfe::ConsoleObjects::ConsoleObjectList& all = dice::hfe::mainConsoleObjects->getConsoleObjects();
    int bindings = 0;
    bool agree = true;
    std::vector<std::string> types;
    std::pair<std::string, std::string> range;
    for (int pass = 0; pass < 2 && bindings == 0; ++pass)
        for (dice::hfe::ConsoleObjects::ConsoleObjectList::const_iterator it = all.begin(); it != all.end(); ++it)
        {
            dice::hfe::ConsoleObject* object = *it;
            if (!object || !object->getObjectName() || !object->getFunctionName()
                || _stricmp(object->getObjectName(), "ObjectTemplate") != 0
                || lowered(object->getFunctionName()) != (pass == 0 ? key : bare))
                continue;
            std::vector<std::string> these;
            for (int i = 0; i < object->getMaxNoArgs() && i < 8; ++i)
                these.push_back(object->getArgType(i) ? object->getArgType(i) : "");
            // The binding's own range (the retail tweaker's slider), as its text pair.
            const std::pair<std::string, std::string> theirs = object->hasRange() ? object->getRangeAsString()
                                                                                   : std::pair<std::string, std::string>();
            if (bindings++ == 0)
            {
                types = these;
                range = theirs;
            }
            else if (these != types)
                agree = false;
            else if (theirs != range)
                range = std::pair<std::string, std::string>();
        }
    if (!agree)
        return binding;
    binding.types = types;
    char* end1 = 0;
    char* end2 = 0;
    const double low = std::strtod(range.first.c_str(), &end1);
    const double high = std::strtod(range.second.c_str(), &end2);
    if (!range.first.empty() && !range.second.empty() && end1 && !*end1 && end2 && !*end2 && low < high)
    {
        binding.ranged = true;
        binding.low = static_cast<float>(low);
        binding.high = static_cast<float>(high);
    }
    return binding;
}

const std::vector<std::string>& propertyTypes(const std::string& property)
{
    return propertyBinding(property).types;
}

// The names a string property's engine setter accepts (compared case-insensitively), as the tables the setters walk:
// Game/Objects/Fx/ParticleSystemTemplates.cpp (emitter type, emit priority, LOD distance and quality level - all three
// over the Low/Medium/High table, as shipped -, sprite particle type, draw order, mesh blend mode),
// Game/Objects/WindAffector.cpp (wind type) and Game/Objects/Visual/LightSourceTemplate.cpp (light type). A mesh
// particle's drawOrder also matches a third name by the table's adjacency, as shipped; it is not offered.
struct EnumNames
{
    const char* property;
    const char* names[5];
};
const EnumNames kEnumNames[] = {
    { "emittertype", { "Standard", "RadialDirection", "Radial", "Box", 0 } },
    { "emitprio", { "Low", "Medium", "High", 0, 0 } },
    { "loddistance", { "Low", "Medium", "High", 0, 0 } },
    { "qualitylevel", { "Low", "Medium", "High", 0, 0 } },
    { "particletype", { "Sorted", "PointSprite", "Additive", 0, 0 } },
    { "draworder", { "Front", "Back", 0, 0, 0 } },
    { "blendmode", { "AlphaBlend", "Additive", 0, 0, 0 } },
    { "windtype", { "Directional", "Point", "RadialDir", "TurbulenceField", 0 } },
    { "lighttype", { "Directional", "Point", "Spot", "SpotProjector", 0 } },
};

const EnumNames* enumNames(const std::string& property)
{
    const std::string bare = bareProperty(property);
    for (size_t i = 0; i < sizeof(kEnumNames) / sizeof(kEnumNames[0]); ++i)
        if (bare == kEnumNames[i].property)
            return &kEnumNames[i];
    return 0;
}

// The numbers a widget edits. The file's value is read afresh every frame, and a drag moves the numbers a little each
// frame, so while a widget is held it edits its own copy (the numbers from the frame before) until it is let go.
struct HeldNumbers
{
    ImGuiID key = 0;
    float   numbers[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};
HeldNumbers g_held;

void holdBegin(const char* id, float* numbers, int count)
{
    if (g_held.key != 0 && g_held.key == ImGui::GetID(id))
        std::memcpy(numbers, g_held.numbers, sizeof(float) * static_cast<size_t>(count));
}

void holdEnd(const char* id, const float* numbers, int count)
{
    const ImGuiID key = ImGui::GetID(id);
    if (ImGui::IsItemActive())
    {
        g_held.key = key;
        std::memcpy(g_held.numbers, numbers, sizeof(float) * static_cast<size_t>(count));
    }
    else if (g_held.key == key)
        g_held.key = 0;
}

// "a/b/c" -> numbers (missing ones 0).
void readParts(const std::string& value, float* out, int count)
{
    size_t at = 0;
    for (int i = 0; i < count; ++i)
    {
        out[i] = static_cast<float>(std::atof(value.c_str() + at));
        const size_t slash = value.find('/', at);
        if (slash == std::string::npos)
        {
            for (int k = i + 1; k < count; ++k)
                out[k] = 0.0f;
            return;
        }
        at = slash + 1;
    }
}

std::string writeParts(const float* in, int count)
{
    std::string text;
    for (int i = 0; i < count; ++i)
    {
        char number[32];
        std::snprintf(number, sizeof(number), "%g", in[i]);
        text += (i ? "/" : "") + std::string(number);
    }
    return text;
}

// A typed editor for a one-argument property (the retail tweaker's item kinds, in our form): a check box for a
// bool; a whole number or a number, as a slider when the binding has a range; two to four numbers - a colour picker
// for a colour of three, and a graph's curve over a particle's life above its four numbers -; a random distribution;
// the names an enumerated string's setter accepts; and text for anything else. True when the user finished an edit;
// `out` is then the value as the file writes it.
bool typedEditor(const std::vector<std::string>& types, const std::string& property, const std::string& value,
                 std::string& out, bool& typed)
{
    typed = false;
    if (types.size() != 1)
        return false;
    const std::string& type = types[0];
    const PropertyBinding& binding = propertyBinding(property);
    if (type == "bool")
    {
        typed = true;
        bool on = std::atoi(value.c_str()) != 0;
        if (ImGui::Checkbox("##bool", &on))
        {
            out = on ? "1" : "0";
            return true;
        }
        return false;
    }
    if (type == "int" || type == "unsigned int" || type == "U32" || type == "long")
    {
        typed = true;
        float held = static_cast<float>(std::atoi(value.c_str()));
        holdBegin("##int", &held, 1);
        int number = static_cast<int>(held);
        ImGui::SetNextItemWidth(-1.0f);
        if (binding.ranged)
            ImGui::SliderInt("##int", &number, static_cast<int>(binding.low), static_cast<int>(binding.high));
        else
            ImGui::InputInt("##int", &number, 1, 10);
        held = static_cast<float>(number);
        holdEnd("##int", &held, 1);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            char text[32];
            std::snprintf(text, sizeof(text), "%d", number);
            out = text;
            return true;
        }
        return false;
    }
    if (type == "float")
    {
        typed = true;
        float number = static_cast<float>(std::atof(value.c_str()));
        holdBegin("##float", &number, 1);
        ImGui::SetNextItemWidth(-1.0f);
        if (binding.ranged)
            ImGui::SliderFloat("##float", &number, binding.low, binding.high, "%g");
        else
            ImGui::DragFloat("##float", &number, 0.01f, 0.0f, 0.0f, "%g");
        holdEnd("##float", &number, 1);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            out = writeParts(&number, 1);
            return true;
        }
        return false;
    }
    if (type == "RandomDistribution")
    {
        // "<CRD_...>/<value>/<variation>/<relative>", as the engine's reader takes it (Core/Random.cpp: the kind by
        // its exact spelling, an unknown one read as CRD_NONE).
        typed = true;
        static const char* const kinds[4] = { "CRD_NONE", "CRD_UNIFORM", "CRD_EXPONENTIAL", "CRD_NORMAL" };
        const size_t slash = value.find('/');
        const std::string kindText = value.substr(0, slash);
        int kind = 0;
        for (int k = 0; k < 4; ++k)
            if (kindText == kinds[k])
                kind = k;
        float parts[3] = { 1.0f, 0.0f, 0.0f };
        if (slash != std::string::npos)
            readParts(value.substr(slash + 1), parts, 3);
        bool relative = parts[2] != 0.0f;
        // The kind on one line; the value, the variation and the relative flag on the next.
        bool done = false;
        ImGui::SetNextItemWidth(-1.0f);
        done = ImGui::Combo("##crd", &kind, kinds, 4) || done;
        ImGui::SetItemTooltip("the distribution");
        holdBegin("##crdv", parts, 2);
        ImGui::SetNextItemWidth(-(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x));
        ImGui::DragFloat2("##crdv", parts, 0.01f, 0.0f, 0.0f, "%g");
        holdEnd("##crdv", parts, 2);
        done = ImGui::IsItemDeactivatedAfterEdit() || done;
        ImGui::SetItemTooltip("the value and the variation");
        ImGui::SameLine();
        done = ImGui::Checkbox("##crdr", &relative) || done;
        ImGui::SetItemTooltip("relative");
        if (done)
        {
            char text[96];
            std::snprintf(text, sizeof(text), "%s/%g/%g/%d", kinds[kind], parts[0], parts[1], relative ? 1 : 0);
            out = text;
            return true;
        }
        return false;
    }
    if (type == "std::string")
    {
        const EnumNames* names = enumNames(property);
        if (!names)
            return false;
        typed = true;
        int current = -1;
        for (int k = 0; k < 5 && names->names[k]; ++k)
            if (_stricmp(names->names[k], value.c_str()) == 0)
                current = k;
        bool done = false;
        ImGui::SetNextItemWidth(-1.0f);
        const bool open = ImGui::BeginCombo("##enum", current >= 0 ? names->names[current] : value.empty() ? "(none)" : value.c_str());
        if (!open)
            noteTestItem("##enum");             // the closed box, for a scripted click (ImGui reports no label for it)
        if (open)
        {
            for (int k = 0; k < 5 && names->names[k]; ++k)
                if (ImGui::Selectable(names->names[k], k == current) && k != current)
                {
                    out = names->names[k];
                    done = true;
                }
            ImGui::EndCombo();
        }
        return done;
    }
    const int parts = type == "Vec2" ? 2 : type == "Vec3" ? 3 : type == "Vec4" ? 4 : 0;
    if (parts)
    {
        typed = true;
        float numbers[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        readParts(value, numbers, parts);
        holdBegin("##vec", numbers, parts);
        const std::string name = lowered(property);
        const bool colour = parts == 3 && (name.find("color") != std::string::npos || name.find("colour") != std::string::npos)
                            && numbers[0] <= 1.0f && numbers[1] <= 1.0f && numbers[2] <= 1.0f;
        if (parts == 4 && name.size() > 5 && name.compare(name.size() - 5, 5, "graph") == 0)
        {
            // A graph: the engine evaluates x t^3 + y t^2 + z t + w over the particle's life, t from 0 to 1
            // (Game/Objects/Fx/Particle.cpp evaluateTransparencyGraph); the curve is drawn as it would come out.
            float samples[33];
            for (int i = 0; i <= 32; ++i)
            {
                const float t = static_cast<float>(i) / 32.0f;
                samples[i] = ((numbers[0] * t + numbers[1]) * t + numbers[2]) * t + numbers[3];
            }
            ImGui::PlotLines("##curve", samples, 33, 0, 0, FLT_MAX, FLT_MAX, ImVec2(-1.0f, 44.0f));
            ImGui::SetItemTooltip("x t^3 + y t^2 + z t + w over the particle's life (t from 0 to 1), as the engine "
                                  "evaluates a graph");
        }
        ImGui::SetNextItemWidth(-1.0f);
        if (colour)
            ImGui::ColorEdit3("##vec", numbers, ImGuiColorEditFlags_Float);
        else if (parts == 2)
            ImGui::DragFloat2("##vec", numbers, 0.01f, 0.0f, 0.0f, "%g");
        else if (parts == 3)
            ImGui::DragFloat3("##vec", numbers, 0.01f, 0.0f, 0.0f, "%g");
        else
            ImGui::DragFloat4("##vec", numbers, 0.01f, 0.0f, 0.0f, "%g");
        holdEnd("##vec", numbers, parts);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            out = writeParts(numbers, parts);
            return true;
        }
        return false;
    }
    return false;
}

} // namespace

ObjectTemplatePanel::ObjectTemplatePanel()
    : m_editFile(-1)
    , m_editLine(-1)
    , m_childRow(-1)
    , m_childField(-1)
    , m_componentChoice(-1)
    , m_newClassChoice(-1)
    , m_childClassChoice(-1)
    , m_retypeChoice(-1)
    , m_confirmExtract(false)
    , m_messageFailed(false)
{
    m_template[0] = m_newProperty[0] = m_newValue[0] = m_packFolder[0] = m_editValue[0] = m_newChild[0] = 0;
    for (int k = 0; k < 3; ++k)
        m_newChildPosition[k] = m_childValue[k] = m_newChildAt[k] = 0.0f;
    m_newName[0] = m_newFolder[0] = m_cloneName[0] = m_newChildName[0] = 0;
}

namespace
{

// A class picker over `classes`; `choice` is the index picked, -1 for none.
void classCombo(const char* id, const char* hint, const std::vector<std::string>& classes, int& choice)
{
    const bool chosen = choice >= 0 && choice < static_cast<int>(classes.size());
    ImGui::SetNextItemWidth(170.0f);
    if (ImGui::BeginCombo(id, chosen ? classes[static_cast<size_t>(choice)].c_str() : hint))
    {
        for (size_t i = 0; i < classes.size(); ++i)
            if (ImGui::Selectable(classes[i].c_str(), choice == static_cast<int>(i)))
                choice = static_cast<int>(i);
        ImGui::EndCombo();
    }
}

} // namespace

bool ObjectTemplatePanel::run(const std::string& line, const char* what)
{
    const ConsoleLineResult r = runConsoleLine(line);
    logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "%s: %s", what, r.output.c_str());
    m_message = r.output;
    m_messageFailed = r.failed;
    return !r.failed;
}

void ObjectTemplatePanel::draw(EditSession& session, bool* open)
{
    const std::string requested = session.takePanelTemplate();     // EditorObjectTemplate.show
    if (!requested.empty())
        copyText(m_template, sizeof(m_template), requested);
    // Opened beside the Properties panel the first time, never floating over the view.
    ImGuiWindow* neighbour = ImGui::FindWindowByName("Properties");
    if (neighbour && neighbour->DockId != 0)
        ImGui::SetNextWindowDockID(neighbour->DockId, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Object templates", open))
    {
        ImGui::End();
        return;
    }
    ImGui::SetNextItemWidth(-160.0f);
    ImGui::InputTextWithHint("##template", "a template name", m_template, sizeof(m_template));
    ImGui::SameLine();
    if (ImGui::Button("The selected object's"))
    {
        dice::hfe::world::IObject* o = EditSession::object(session.selection().primary());
        if (o && o->getTemplate())
            copyText(m_template, sizeof(m_template), o->getTemplate()->getName());
    }
    if (!m_message.empty())
        ImGui::TextColored(m_messageFailed ? kWarning : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "%s",
                           m_message.c_str());
    // A new template of its own file, and the shown one cloned (both written at once, as retail's Save As).
    if (ImGui::CollapsingHeader("New template or clone"))
    {
        if (m_newClasses.empty())
            m_newClasses = EditSession::templateClassChoices(false);
        classCombo("##newClass", "a class", m_newClasses, m_newClassChoice);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputTextWithHint("##newName", "name", m_newName, sizeof(m_newName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-70.0f);
        ImGui::InputTextWithHint("##newFolder", "folder (Objects/<name>)", m_newFolder, sizeof(m_newFolder));
        ImGui::SameLine();
        if (ImGui::Button("Create") && m_newName[0] && m_newClassChoice >= 0
            && m_newClassChoice < static_cast<int>(m_newClasses.size())
            && run(std::string("EditorObjectTemplate.new ") + m_newClasses[static_cast<size_t>(m_newClassChoice)] + " "
                       + m_newName + (m_newFolder[0] ? " " + quoted(m_newFolder) : std::string()),
                   "template"))
            copyText(m_template, sizeof(m_template), m_newName);
        if (m_template[0])
        {
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputTextWithHint("##cloneName", "the clone's name", m_cloneName, sizeof(m_cloneName));
            ImGui::SameLine();
            if (ImGui::Button("Clone") && m_cloneName[0]
                && run(std::string("EditorObjectTemplate.clone ") + m_template + " " + m_cloneName, "template"))
                copyText(m_template, sizeof(m_template), m_cloneName);
            ImGui::SameLine();
            ImGui::TextDisabled("%s, beside its files", m_template);
            classCombo("##retype", "another class", m_newClasses, m_retypeChoice);
            ImGui::SameLine();
            if (ImGui::Button("Change type") && m_retypeChoice >= 0 && m_retypeChoice < static_cast<int>(m_newClasses.size()))
                run(std::string("EditorObjectTemplate.changeType ") + m_template + " "
                        + m_newClasses[static_cast<size_t>(m_retypeChoice)],
                    "template");
            ImGui::SetItemTooltip("Only a template made in the editor changes its class (the retail editor's rule).");
        }
        ImGui::TextDisabled("Written at once and loaded; not undoable (delete the files to take them away).");
    }

    if (m_template[0])
    {
        std::vector<size_t> files;
        std::string error;
        TemplateDocuments& documents = session.templateDocuments();
        if (!documents.filesOf(m_template, files, error))
            ImGui::TextColored(kWarning, "%s", error.c_str());
        for (size_t f = 0; f < files.size(); ++f)
        {
            const TemplateDocument& document = documents.document(files[f]);
            ImGui::PushID(static_cast<int>(files[f]));
            ImGui::SeparatorText(document.path.c_str());
            if (!document.exists)
            {
                ImGui::TextDisabled("not there");
                ImGui::PopID();
                continue;
            }
            if (document.archived)
                ImGui::TextColored(kNote, "read from a mounted archive: read-only (extract the object archives and use "
                                          "+looseObjects 1 to edit it)");
            else
                ImGui::TextDisabled("loose; saved with the level to %s%s", document.writePath.c_str(),
                                    document.modified() ? " (modified)" : "");
            const std::vector<TemplatePropertyLine> lines = document.file.properties(m_template);
            if (lines.empty())
                ImGui::TextDisabled("no lines of %s here", m_template);
            std::map<std::string, int> seen;          // lower-case property -> lines of it so far
            std::map<std::string, int> total;
            for (size_t k = 0; k < lines.size(); ++k)
            {
                std::string key = lines[k].property;
                for (size_t c = 0; c < key.size(); ++c)
                    key[c] = static_cast<char>(std::tolower(static_cast<unsigned char>(key[c])));
                ++total[key];
            }
            if (!lines.empty() && ImGui::BeginTable("##lines", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("property", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 1.4f);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 20.0f);
                for (size_t k = 0; k < lines.size(); ++k)
                {
                    std::string key = lines[k].property;
                    for (size_t c = 0; c < key.size(); ++c)
                        key[c] = static_cast<char>(std::tolower(static_cast<unsigned char>(key[c])));
                    const int index = seen[key]++;
                    char spec[160];
                    if (total[key] > 1)
                        std::snprintf(spec, sizeof(spec), "%s#%d", lines[k].property.c_str(), index);
                    else
                        std::snprintf(spec, sizeof(spec), "%s", lines[k].property.c_str());
                    ImGui::PushID(static_cast<int>(k));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(spec);
                    ImGui::TableNextColumn();
                    if (document.archived)
                        ImGui::TextUnformatted(lines[k].value.c_str());
                    else
                    {
                        // A typed editor when the engine's binding says what the property holds (H7), else text; a
                        // reference property (a texture, a file, a template, a material) keeps its text with a picker.
                        const bool palette = TexturePalette::serves(lines[k].property);
                        const bool picker = PropertyPickers::serves(lines[k].property);
                        std::string typedValue;
                        bool typed = false;
                        if (!palette && !picker
                            && typedEditor(propertyTypes(lines[k].property), lines[k].property, lines[k].value, typedValue, typed))
                            run(std::string("EditorObjectTemplate.set ") + m_template + " " + spec + " " + quoted(typedValue),
                                "template");
                        if (!typed)
                        {
                            const bool editing = m_editFile == static_cast<int>(files[f]) && m_editLine == static_cast<int>(k);
                            char buffer[256];
                            copyText(buffer, sizeof(buffer), editing ? std::string(m_editValue) : lines[k].value);
                            ImGui::SetNextItemWidth(palette || picker ? -(ImGui::CalcTextSize("...").x + ImGui::GetStyle().FramePadding.x * 2.0f
                                                                          + ImGui::GetStyle().ItemSpacing.x)
                                                                      : -1.0f);
                            if (ImGui::InputText("##value", buffer, sizeof(buffer)))
                            {
                                copyText(m_editValue, sizeof(m_editValue), buffer);
                                m_editFile = static_cast<int>(files[f]);
                                m_editLine = static_cast<int>(k);
                            }
                            if (ImGui::IsItemDeactivatedAfterEdit() && editing)
                            {
                                run(std::string("EditorObjectTemplate.set ") + m_template + " " + spec + " " + quoted(m_editValue),
                                    "template");
                                m_editFile = m_editLine = -1;
                            }
                            if (palette || picker)
                            {
                                ImGui::SameLine();
                                std::string picked;
                                if (palette ? TexturePalette::button("##palette", lines[k].property, lines[k].value, picked)
                                            : PropertyPickers::button("##picker", lines[k].property, lines[k].value, session, picked))
                                    run(std::string("EditorObjectTemplate.set ") + m_template + " " + spec + " " + quoted(picked),
                                        "template");
                            }
                        }
                        else
                            ImGui::SetItemTooltip("%s (%s) - as the file writes it: %s", lines[k].property.c_str(),
                                                  propertyTypes(lines[k].property)[0].c_str(), lines[k].value.c_str());
                        ImGui::TableNextColumn();
                        if (ImGui::SmallButton("x"))
                            run(std::string("EditorObjectTemplate.remove ") + m_template + " " + spec, "template");
                        ImGui::SetItemTooltip("Take this line out of the file (the engine keeps the value until the level "
                                              "loads again).");
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::PopID();
        }
        ImGui::SeparatorText("Add or set a property");
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputTextWithHint("##newProperty", "property", m_newProperty, sizeof(m_newProperty));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-60.0f);
        ImGui::InputTextWithHint("##newValue", "value", m_newValue, sizeof(m_newValue));
        ImGui::SameLine();
        if (ImGui::Button("Set") && m_newProperty[0])
            run(std::string("EditorObjectTemplate.set ") + m_template + " " + m_newProperty + " " + quoted(m_newValue),
                "template");
        ImGui::TextDisabled("A property the engine sets (armor.maxHitPoints, hasMobilePhysics ...) changes at once; a "
                            "line the engine adds (addTemplate, mapMaterial ...) when the level loads again. File > Save "
                            "writes the files.");

        // The viewer: a preview of the template beside the level, never saved.
        ImGui::SeparatorText("Preview");
        if (ImGui::Button("Show here"))
            run(std::string("EditorPreview.show ") + m_template, "preview");
        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            run("EditorPreview.refresh", "preview");
        ImGui::SameLine();
        if (ImGui::Button("Frame"))
            run("EditorPreview.frame", "preview");
        ImGui::SameLine();
        if (ImGui::Button("Reset##preview"))
            run("EditorPreview.reset", "preview");
        ImGui::SameLine();
        if (ImGui::Button("Turntable"))
            run("EditorPreview.turntable 30", "preview");
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            run("EditorPreview.turntable 0", "preview");
        ImGui::SameLine();
        if (ImGui::Button("Through its camera"))
            run("EditorPreview.camera 0", "preview");
        ImGui::SameLine();
        if (ImGui::Button("Clear##preview"))
            run("EditorPreview.clear", "preview");
        ImGui::TextDisabled("Made where the view's centre meets the ground, with a grid, its bounds and its centre drawn; "
                            "Refresh makes it again after an edit.");
        // A weapon: held by the play-test soldier and fired, its common tuning beside it (the retail object editor's
        // weapon test, with the properties a modder tunes while testing).
        const dice::hfe::world::IObjectTemplate* const shownTemplate =
            dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(m_template) : 0;
        if (shownTemplate && shownTemplate->getClassID() == dice::hfe::world::CID_GenericFireArmTemplate)
        {
            ImGui::SeparatorText("Weapon test");
            if (ImGui::Button("Test this weapon"))
                run(std::string("EditorPlayTest.weapon ") + m_template, "play-test");
            ImGui::SetItemTooltip("A soldier where the camera stands, holding this weapon: fire with the left button, R "
                                  "reloads. Press it again for a fresh weapon (a new magazine size takes one).");
            static const char* const kTuning[][2] = { { "fire.roundsPerMinute", "rate of fire (rounds a minute)" },
                                                      { "ammo.reloadTime", "reload time (seconds)" },
                                                      { "ammo.magSize", "rounds in a magazine" },
                                                      { "ammo.nrOfMags", "magazines" } };
            const bool refresh = m_tuningTemplate != m_template || ImGui::GetFrameCount() - m_tuningFrame > 30;
            for (size_t t = 0; t < sizeof(kTuning) / sizeof(kTuning[0]); ++t)
            {
                if (refresh)
                {
                    const ConsoleLineResult value =
                        runConsoleLine(std::string("EditorObjectTemplate.engineValue ") + m_template + " " + kTuning[t][0]);
                    m_tuningValue[t] = value.failed ? std::string() : value.output;
                }
                ImGui::PushID(static_cast<int>(t));
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputText(kTuning[t][1], m_tuningEdit[t], sizeof(m_tuningEdit[t]), ImGuiInputTextFlags_EnterReturnsTrue)
                    && m_tuningEdit[t][0])
                {
                    run(std::string("EditorObjectTemplate.set ") + m_template + " " + kTuning[t][0] + " " + m_tuningEdit[t],
                        "template");
                    m_tuningFrame = -1000;
                }
                // Not being typed in: it shows the engine's value.
                if (!ImGui::IsItemActive())
                    std::snprintf(m_tuningEdit[t], sizeof(m_tuningEdit[t]), "%s", m_tuningValue[t].c_str());
                ImGui::PopID();
            }
            if (refresh)
            {
                m_tuningTemplate = m_template;
                m_tuningFrame = ImGui::GetFrameCount();
            }
            ImGui::TextDisabled("Each change is an undoable edit of the template's file; the held weapon fires and reloads "
                                "at the new rate at once.");
        }
        // Its parts, picked in the view or here and moved with the gizmo ([added]).
        if (session.templatePreview())
        {
            bool shown = session.previewPartsShown();
            if (ImGui::Checkbox("Edit its parts in the view", &shown))
                run(std::string("EditorPreview.showParts ") + (shown ? "1" : "0"), "preview");
            ImGui::SetItemTooltip("Children, seats, cameras and the seats' soldier exit points, drawn on the preview. Click "
                                  "one to pick it: the gizmo moves it (and turns a child in the rotate mode). Letting go "
                                  "writes its line in the template's file, one undoable edit. Esc lets it go.");
            if (shown)
            {
                const std::vector<EditSession::PreviewPart> parts = session.previewParts();
                const int picked = session.pickedPreviewPart();
                if (ImGui::BeginChild("##previewParts", ImVec2(0.0f, 150.0f), ImGuiChildFlags_Borders))
                {
                    for (size_t i = 0; i < parts.size(); ++i)
                    {
                        const EditSession::PreviewPart& part = parts[i];
                        char row[320];
                        std::snprintf(row, sizeof(row), "%*s%s%s%s  (%s)##part%u", part.depth * 2, "",
                                      part.kind == EditSession::PreviewPart::Child ? part.label.c_str() : part.label.c_str(),
                                      part.seat ? "  [seat]" : "", part.camera ? "  [camera]" : "", part.owner.c_str(),
                                      static_cast<unsigned>(i));
                        if (ImGui::Selectable(row, picked == static_cast<int>(i)))
                            run("EditorPreview.pick " + std::to_string(picked == static_cast<int>(i) ? -1 : static_cast<int>(i)),
                                "preview");
                    }
                }
                ImGui::EndChild();
                if (picked >= 0 && picked < static_cast<int>(parts.size()))
                    ImGui::TextDisabled("%s picked: the gizmo moves it (Move / Rotate on the toolbar); Esc lets it go.",
                                        parts[static_cast<size_t>(picked)].label.c_str());
            }
        }

        // The geometry template's settings: the .tweak's GeometryTemplate lines, edited in place.
        ImGui::SeparatorText("Geometry settings (.tweak)");
        {
            const ConsoleLineResult listed = runConsoleLine(std::string("EditorObjectTemplate.geometry ") + m_template);
            ImGui::TextWrapped("%s", listed.output.c_str());
            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputTextWithHint("##geometryProperty", "e.g. setSubGeometryLodDistance#0", m_geometryProperty,
                                     sizeof(m_geometryProperty));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputTextWithHint("##geometryValue", "e.g. 0 0 50", m_geometryValue, sizeof(m_geometryValue));
            ImGui::SameLine();
            if (ImGui::Button("Set##geometry") && m_geometryProperty[0])
                run(std::string("EditorObjectTemplate.setGeometry ") + m_template + " " + m_geometryProperty + " "
                        + quoted(m_geometryValue), "template");
            ImGui::SameLine();
            if (ImGui::Button("Remove##geometry") && m_geometryProperty[0])
                run(std::string("EditorObjectTemplate.removeGeometry ") + m_template + " " + m_geometryProperty, "template");
            ImGui::TextDisabled("LOD distances (setSubGeometryLodDistance <part> <lod> <metres>), lightmap size "
                                "(setLightmapSizeAll), parallax (setMaterialParallaxHeightScale), static gloss "
                                "(setSpecularStaticGloss); read when the level loads again.");
        }

        // The object editor's helpers: back to the files, an armor effect, the fire modes.
        ImGui::SeparatorText("Helpers");
        if (ImGui::Button("Revert to the files"))
            run(std::string("EditorObjectTemplate.revert ") + m_template, "template");
        ImGui::SetItemTooltip("Drop this template's unsaved edits: its files as last read or saved, the template made again "
                              "from them (the undo history is cleared).");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##armorEffect", "an EffectBundle template", m_armorEffect, sizeof(m_armorEffect));
        ImGui::SameLine();
        if (ImGui::Button("Add armor effect") && m_armorEffect[0])
            run(std::string("EditorObjectTemplate.addArmorEffect ") + m_template + " " + m_armorEffect, "template");
        ImGui::Checkbox("single", &m_fireModes[0]);
        ImGui::SameLine();
        ImGui::Checkbox("burst", &m_fireModes[1]);
        ImGui::SameLine();
        ImGui::Checkbox("full", &m_fireModes[2]);
        ImGui::SameLine();
        if (ImGui::Button("Set fire modes"))
        {
            std::string modes;
            const char* const names[3] = { "single", "burst", "full" };
            for (int m = 0; m < 3; ++m)
                if (m_fireModes[m])
                    modes += (modes.empty() ? "" : ",") + std::string(names[m]);
            run(std::string("EditorObjectTemplate.fireRates ") + m_template + " " + (modes.empty() ? "none" : modes), "template");
        }
        ImGui::SetItemTooltip("The fire component's addFireRate lines: the modes ticked, the highest first (as retail wrote "
                              "them).");

        // An effect template: its preview, where the view's centre meets the ground (EditorEffect.*).
        if (EditSession::isEffectTemplate(m_template))
        {
            ImGui::SeparatorText("Effect preview");
            if (ImGui::Button("Preview here"))
                run(std::string("EditorEffect.preview ") + m_template, "effect");
            ImGui::SameLine();
            if (ImGui::Button("Play"))
                run("EditorEffect.play", "effect");
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
                run("EditorEffect.stop", "effect");
            ImGui::SameLine();
            if (ImGui::Button("Pause"))
                run("EditorEffect.pause", "effect");
            ImGui::SameLine();
            if (ImGui::Button("Clear"))
                run("EditorEffect.clear", "effect");
            ImGui::TextDisabled("The preview is never saved. With effect objects selected, Play, Stop and Pause act on "
                                "them instead.");
        }

        // A destroyable object's two checks, as the retail object editor offered them on activation.
        const std::vector<std::string> fixes = session.templateFixes(m_template);
        if (!fixes.empty())
        {
            ImGui::SeparatorText("Destroyable object");
            for (size_t i = 0; i < fixes.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i) + 500);
                const bool armor = fixes[i] == "armor";
                ImGui::TextColored(kNote, armor ? "No Armor component." : "No networkable info.");
                ImGui::SameLine();
                if (ImGui::SmallButton(armor ? "Add Armor" : "Set BasicInfo"))
                    run(armor ? std::string("EditorObjectTemplate.addComponent ") + m_template + " Armor"
                              : std::string("EditorObjectTemplate.set ") + m_template + " setNetworkableInfo BasicInfo",
                        "template");
                ImGui::PopID();
            }
        }

        // The template's components, as the engine holds them.
        std::vector<std::string> components;
        std::string componentError;
        if (session.templateComponents(m_template, components, componentError))
        {
            ImGui::SeparatorText("Components");
            if (components.empty())
                ImGui::TextDisabled("no components");
            for (size_t i = 0; i < components.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::BulletText("%s", components[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("x"))
                    run(std::string("EditorObjectTemplate.removeComponent ") + m_template + " " + components[i], "template");
                ImGui::SetItemTooltip("Take this component out: deleteComponent in the engine, its BeginComp section out "
                                      "of the file.");
                ImGui::PopID();
            }
            if (m_componentClasses.empty())
                m_componentClasses = EditSession::componentClasses();
            const bool chosen = m_componentChoice >= 0 && m_componentChoice < static_cast<int>(m_componentClasses.size());
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo("##componentClass", chosen ? m_componentClasses[static_cast<size_t>(m_componentChoice)].c_str()
                                                             : "a component class"))
            {
                for (size_t i = 0; i < m_componentClasses.size(); ++i)
                    if (ImGui::Selectable(m_componentClasses[i].c_str(), m_componentChoice == static_cast<int>(i)))
                        m_componentChoice = static_cast<int>(i);
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button("Add component") && chosen)
                run(std::string("EditorObjectTemplate.addComponent ") + m_template + " "
                        + m_componentClasses[static_cast<size_t>(m_componentChoice)],
                    "template");
            ImGui::TextDisabled("A component of a kind the template has already (a second fire component) replaces it.");
        }

        // A bundle's children, as the engine holds them; each row names its child `name#n` (the n-th of that
        // name), so an action on it reaches that child and its own lines.
        std::vector<EditSession::TemplateChild> children;
        std::string childError;
        if (session.templateChildren(m_template, children, childError))
        {
            ImGui::SeparatorText("Children");
            std::map<std::string, int> occurrences;       // lower-case name -> children of it so far
            if (children.empty())
                ImGui::TextDisabled("no children");
            else if (ImGui::BeginTable("##children", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("child", ImGuiTableColumnFlags_WidthStretch, 1.2f);
                ImGui::TableSetupColumn("position", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("rotation", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 20.0f);
                ImGui::TableHeadersRow();
                for (size_t i = 0; i < children.size(); ++i)
                {
                    const EditSession::TemplateChild& child = children[i];
                    std::string key = child.name;
                    for (size_t c = 0; c < key.size(); ++c)
                        key[c] = static_cast<char>(std::tolower(static_cast<unsigned char>(key[c])));
                    char spec[160];
                    std::snprintf(spec, sizeof(spec), "%s#%d", child.name.c_str(), occurrences[key]++);
                    float values[2][3] = { { child.position.x, child.position.y, child.position.z },
                                           { child.rotation.x, child.rotation.y, child.rotation.z } };
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(child.name.c_str());
                    // The child helpers, on a right click on its name.
                    if (ImGui::BeginPopupContextItem("childHelpers"))
                    {
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::InputFloat("degrees", &m_turnAngle, 15.0f, 45.0f, "%g");
                        char line[512];
                        if (ImGui::MenuItem("Turn it about the parent"))
                        {
                            std::snprintf(line, sizeof(line), "EditorObjectTemplate.turnChild %s %s %g", m_template, spec, m_turnAngle);
                            run(line, "template");
                        }
                        if (ImGui::MenuItem("Turn it and the children after it"))
                        {
                            std::snprintf(line, sizeof(line), "EditorObjectTemplate.turnChildren %s %s %g", m_template, spec,
                                          m_turnAngle);
                            run(line, "template");
                        }
                        if (ImGui::MenuItem("Reset its place"))
                            run(std::string("EditorObjectTemplate.resetChild ") + m_template + " " + spec, "template");
                        ImGui::Separator();
                        if (ImGui::MenuItem("Make it a barrel"))
                            run(std::string("EditorObjectTemplate.barrel ") + m_template + " " + spec + " 1", "template");
                        if (ImGui::MenuItem("No longer a barrel"))
                            run(std::string("EditorObjectTemplate.barrel ") + m_template + " " + spec + " 0", "template");
                        if (ImGui::MenuItem("Make it this vehicle's seat parent"))
                            run(std::string("EditorObjectTemplate.seatParent ") + m_template + " " + child.name, "template");
                        ImGui::EndPopup();
                    }
                    ImGui::SetItemTooltip("Right-click: turn about the parent, reset, barrel, seat parent.");
                    for (int field = 0; field < 2; ++field)
                    {
                        ImGui::TableNextColumn();
                        const bool editing = m_childRow == static_cast<int>(i) && m_childField == field;
                        float shown[3];
                        for (int k = 0; k < 3; ++k)
                            shown[k] = editing ? m_childValue[k] : values[field][k];
                        ImGui::SetNextItemWidth(-1.0f);
                        if (ImGui::InputFloat3(field == 0 ? "##position" : "##rotation", shown, "%.3f"))
                        {
                            for (int k = 0; k < 3; ++k)
                                m_childValue[k] = shown[k];
                            m_childRow = static_cast<int>(i);
                            m_childField = field;
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit() && m_childRow == static_cast<int>(i) && m_childField == field)
                        {
                            for (int k = 0; k < 3; ++k)
                                values[field][k] = m_childValue[k];
                            char line[512];
                            std::snprintf(line, sizeof(line),
                                          "EditorObjectTemplate.moveChild %s %s %.4f/%.4f/%.4f %.4f/%.4f/%.4f", m_template,
                                          spec, values[0][0], values[0][1], values[0][2], values[1][0], values[1][1],
                                          values[1][2]);
                            run(line, "template");
                            m_childRow = m_childField = -1;
                        }
                        if (field == 1)
                            ImGui::SetItemTooltip("Yaw, pitch and roll in degrees, as ObjectTemplate.setRotation takes them.");
                    }
                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton("x"))
                        run(std::string("EditorObjectTemplate.removeChild ") + m_template + " " + spec, "template");
                    ImGui::SetItemTooltip("Take this child out of the template: its addTemplate line with its position and "
                                          "rotation.");
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputTextWithHint("##newChild", "a template to add as a child", m_newChild, sizeof(m_newChild));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-90.0f);
            ImGui::InputFloat3("##newChildPosition", m_newChildPosition, "%.3f");
            ImGui::SameLine();
            if (ImGui::Button("Add child") && m_newChild[0])
            {
                char line[512];
                std::snprintf(line, sizeof(line), "EditorObjectTemplate.addChild %s %s %.4f/%.4f/%.4f", m_template,
                              m_newChild, m_newChildPosition[0], m_newChildPosition[1], m_newChildPosition[2]);
                run(line, "template");
            }
            // A new child template, made inside this template's file.
            if (m_childClasses.empty())
                m_childClasses = EditSession::templateClassChoices(true);
            classCombo("##childClass", "a new child's class", m_childClasses, m_childClassChoice);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputTextWithHint("##newChildName", "its name", m_newChildName, sizeof(m_newChildName));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-110.0f);
            ImGui::InputFloat3("##newChildAt", m_newChildAt, "%.3f");
            ImGui::SameLine();
            if (ImGui::Button("New child") && m_newChildName[0] && m_childClassChoice >= 0
                && m_childClassChoice < static_cast<int>(m_childClasses.size()))
            {
                char line[512];
                std::snprintf(line, sizeof(line), "EditorObjectTemplate.newChild %s %s %s %.4f/%.4f/%.4f", m_template,
                              m_childClasses[static_cast<size_t>(m_childClassChoice)].c_str(), m_newChildName,
                              m_newChildAt[0], m_newChildAt[1], m_newChildAt[2]);
                run(line, "template");
            }
            ImGui::TextDisabled("Objects already placed keep their children until the level loads again. File > Save "
                                "writes the files.");
        }
    }

    ImGui::SeparatorText("Object archives");
    if (m_archives.empty() || ImGui::SmallButton("Refresh"))
    {
        std::string status;
        session.objectArchivesStatus(status);
        m_archives = status;
    }
    ImGui::TextWrapped("%s", m_archives.c_str());
    if (ImGui::Button("Extract the object archives..."))
        m_confirmExtract = true;
    if (m_confirmExtract)
    {
        ImGui::TextColored(kNote, "Every file of the mod's object archives goes into its loose Objects folder (about "
                                  "1.5 GB for bf2), keeping any file already there. Then start the editor with "
                                  "+looseObjects 1 to edit the templates.");
        if (ImGui::SmallButton("Extract now"))
        {
            run("EditorObjects.extract", "objects");
            m_confirmExtract = false;
            m_archives.clear();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel"))
            m_confirmExtract = false;
    }
    ImGui::SetNextItemWidth(-80.0f);
    ImGui::InputTextWithHint("##pack", "the mod folder to pack into", m_packFolder, sizeof(m_packFolder));
    ImGui::SameLine();
    if (ImGui::Button("Pack") && m_packFolder[0])
        run(std::string("EditorObjects.pack ") + quoted(m_packFolder), "objects");
    ImGui::SetItemTooltip("Objects_client.zip and Objects_server.zip from the mod's loose Objects folder, with the "
                          "retail editor's filters.");
    ImGui::End();
}

} // namespace bf2editor
