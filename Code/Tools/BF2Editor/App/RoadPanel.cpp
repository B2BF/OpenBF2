// Code/Tools/BF2Editor/App/RoadPanel.cpp - see RoadPanel.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/RoadPanel.h"

#include "App/EditSession.h"
#include "App/TexturePalette.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cstdio>
#include <string>
#include <vector>

namespace bf2editor
{

namespace
{

std::string number(int value)
{
    char text[24];
    std::snprintf(text, sizeof(text), "%d", value);
    return text;
}

std::string pair(const float v[2])
{
    return formatConsoleFloat(v[0]) + "/" + formatConsoleFloat(v[1]);
}

// One road edit from the panel: the console line, its refusal logged.
void run(EditSession& session, const std::string& line)
{
    session.runCommand(line, "roads");
}

// A number set on Enter; true when it was.
bool numberField(const char* label, float& value, float width = 110.0f)
{
    ImGui::SetNextItemWidth(width);
    return ImGui::InputFloat(label, &value, 0.0f, 0.0f, "%g", ImGuiInputTextFlags_EnterReturnsTrue);
}

bool integerField(const char* label, int& value, float width = 110.0f)
{
    ImGui::SetNextItemWidth(width);
    return ImGui::InputInt(label, &value, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
}

bool pairField(const char* label, float v[2], float width = 160.0f)
{
    ImGui::SetNextItemWidth(width);
    return ImGui::InputFloat2(label, v, "%g", ImGuiInputTextFlags_EnterReturnsTrue);
}

} // namespace

void RoadPanel::draw(EditSession& session, bool* open)
{
    // A layout saved before the panel existed: open it beside the Terrain panel the first time.
    ImGuiWindow* neighbour = ImGui::FindWindowByName("Terrain");
    if (!neighbour)
        neighbour = ImGui::FindWindowByName("Level settings");
    if (neighbour && neighbour->DockId != 0)
        ImGui::SetNextWindowDockID(neighbour->DockId, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Roads", open))
    {
        ImGui::End();
        return;
    }
    RoadNetwork& roads = session.roads();
    if (!roads.isOpen())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f),
                           "The level's roads are not open: they need the editor's renderer (RendDX9 built with "
                           "BF2_RENDDX9_EDITOR=1) and an open level.");
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("%s", roads.describe().c_str());
    bool tool = session.roadToolActive();
    if (ImGui::Checkbox("Road tool", &tool))
        run(session, tool ? "EditorRoad.tool 1" : "EditorRoad.tool 0");
    ImGui::SameLine();
    bool shown = session.splinesShown();
    if (ImGui::Checkbox("Show splines", &shown))
        session.setSplinesShown(shown);
    if (tool)
        ImGui::TextDisabled("Click the ground: a point at the end of the active spline (a new spline without one). Drag a "
                            "point to move it; Ctrl+click removes it (or Delete); Alt+click near the active spline "
                            "inserts one. A road runs from the second to the second-to-last point and needs four.");
    if (ImGui::BeginTabBar("##roadtabs"))
    {
        if (ImGui::BeginTabItem("Splines"))
        {
            drawSplines(session);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Roads"))
        {
            drawRoads(session);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Templates"))
        {
            drawTemplates(session);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    if (ImGui::Button("Save roads"))
        run(session, "EditorRoad.save");
    ImGui::SameLine();
    ImGui::TextDisabled("The level's save writes them too, when they changed.");
    ImGui::End();
}

void RoadPanel::drawSplines(EditSession& session)
{
    RoadNetwork& roads = session.roads();
    const std::vector<int> ids = roads.splineIds();
    const int active = session.activeSpline();
    if (ImGui::Button("New spline"))
        run(session, "EditorRoad.splineCreate");
    if (active >= 0)
    {
        ImGui::SameLine();
        if (ImGui::Button("Delete spline"))
            run(session, "EditorRoad.splineDelete " + number(active));
        ImGui::SameLine();
        if (ImGui::Button("Deselect"))
            session.setActiveSpline(-1, -1);
        ImGui::SameLine();
        if (ImGui::Button("Copy"))
            run(session, "EditorRoad.splineCopy " + number(active));
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste"))
        run(session, "EditorRoad.splinePaste");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("A copy of the copied spline beside it (1 m to the side, 1 m further each paste).");
    if (ImGui::BeginListBox("##splines", ImVec2(-1.0f, 6.0f * ImGui::GetTextLineHeightWithSpacing())))
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            SplineRecord spline;
            roads.splineRecord(ids[i], spline);
            const std::string road = roads.roadOnSpline(ids[i]);
            const std::string label = "spline " + number(ids[i]) + " - " + number(static_cast<int>(spline.points.size()))
                                    + " points" + (road.empty() ? std::string() : ", road " + road);
            if (ImGui::Selectable(label.c_str(), ids[i] == active))
                session.setActiveSpline(ids[i], -1);
        }
        ImGui::EndListBox();
    }
    SplineRecord spline;
    if (active < 0 || !roads.splineRecord(active, spline))
    {
        ImGui::TextDisabled("No active spline: pick one above, or click the ground with the road tool.");
        return;
    }
    const std::string id = number(active);
    ImGui::SeparatorText(("Spline " + id).c_str());
    static const char* const kSnaps[] = { "Snap added points", "Snap moved points", "Snap curve samples", "Snap ribbon edges" };
    for (int bit = 0; bit < 4; ++bit)
    {
        bool on = (spline.snapModes & (1 << bit)) != 0;
        if (bit % 2)
            ImGui::SameLine();
        if (ImGui::Checkbox(kSnaps[bit], &on))
            run(session, "EditorRoad.splineSet " + id + " snapModes "
                         + number(on ? (spline.snapModes | (1 << bit)) : (spline.snapModes & ~(1 << bit))));
    }
    float width = spline.lineWidth;
    if (numberField("Ribbon width", width))
        run(session, "EditorRoad.splineSet " + id + " lineWidth " + formatConsoleFloat(width));
    ImGui::SameLine();
    float step = spline.stepSize;
    if (numberField("Step", step))
        run(session, "EditorRoad.splineSet " + id + " stepSize " + formatConsoleFloat(step));
    const int selected = session.activeSplinePoint();
    if (ImGui::BeginTable("##points", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("position");
        ImGui::TableSetupColumn("speed");
        ImGui::TableSetupColumn("delay");
        ImGui::TableSetupColumn("direction");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for (size_t p = 0; p < spline.points.size(); ++p)
        {
            const SplinePointRecord& point = spline.points[p];
            const std::string index = number(static_cast<int>(p));
            ImGui::PushID(static_cast<int>(p));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(index.c_str(), static_cast<int>(p) == selected))
                session.setActiveSpline(active, static_cast<int>(p));
            ImGui::TableNextColumn();
            float position[3] = { point.position[0], point.position[1], point.position[2] };
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::InputFloat3("##position", position, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
                run(session, "EditorRoad.splineMove " + id + " " + index + " " + formatConsoleFloat(position[0]) + "/"
                             + formatConsoleFloat(position[1]) + "/" + formatConsoleFloat(position[2]));
            ImGui::TableNextColumn();
            float speed = point.speed;
            if (numberField("##speed", speed, 60.0f))
                run(session, "EditorRoad.splinePoint " + id + " " + index + " speed " + formatConsoleFloat(speed));
            ImGui::TableNextColumn();
            float delay = point.delay;
            if (numberField("##delay", delay, 60.0f))
                run(session, "EditorRoad.splinePoint " + id + " " + index + " delay " + formatConsoleFloat(delay));
            ImGui::TableNextColumn();
            int direction = point.direction;
            if (integerField("##direction", direction, 50.0f))
                run(session, "EditorRoad.splinePoint " + id + " " + index + " direction " + number(direction));
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Remove"))
                run(session, "EditorRoad.splineRemovePoint " + id + " " + index);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (selected >= 0 && static_cast<size_t>(selected) < spline.points.size())
    {
        char sound[260];
        std::snprintf(sound, sizeof(sound), "%s", spline.points[static_cast<size_t>(selected)].sound.c_str());
        ImGui::SetNextItemWidth(-90.0f);
        if (ImGui::InputTextWithHint("Point sound", "none", sound, sizeof(sound), ImGuiInputTextFlags_EnterReturnsTrue))
            run(session, "EditorRoad.splinePoint " + id + " " + number(selected) + " sound \"" + sound + "\"");
    }

    // Merging, as the retail spline manager did it: one spline marked at the point it joins at, then merged into the
    // active spline at its selected point.
    const int marked = session.markedSpline();
    ImGui::BeginDisabled(selected < 0);
    if (ImGui::Button("Mark for merge"))
        run(session, "EditorRoad.splineMark " + id + " " + number(selected));
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Select the end point the splines join at first.");
    ImGui::SameLine();
    ImGui::BeginDisabled(marked < 0 || marked == active || selected < 0);
    if (ImGui::Button(marked >= 0 && marked != active ? ("Merge spline " + number(marked) + " into this").c_str()
                                                      : "Merge"))
        ImGui::OpenPopup("Merge splines");
    ImGui::EndDisabled();
    if (marked >= 0)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(spline %d marked at point %d)", marked, session.markedSplinePoint());
    }
    if (ImGui::BeginPopupModal("Merge splines", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Do you really want to merge these splines?");
        if (ImGui::Button("Yes"))
        {
            run(session, "EditorRoad.splineMerge " + id + " " + number(selected));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void RoadPanel::drawRoads(EditSession& session)
{
    RoadNetwork& roads = session.roads();
    const std::vector<std::string> names = roads.roadNames();
    const std::vector<std::string> templates = roads.templateNames();
    if (ImGui::BeginListBox("##roads", ImVec2(-1.0f, 6.0f * ImGui::GetTextLineHeightWithSpacing())))
    {
        for (size_t i = 0; i < names.size(); ++i)
        {
            RoadRecord road;
            roads.roadRecord(names[i], road);
            const std::string label = names[i] + " - " + road.templateName + ", spline " + number(road.splineId)
                                    + (roads.ghost(names[i]) ? ", ghost" : ", final");
            if (ImGui::Selectable(label.c_str(), names[i] == m_selectedRoad))
            {
                m_selectedRoad = names[i];
                session.setActiveSpline(road.splineId, -1);
            }
        }
        ImGui::EndListBox();
    }

    if (ImGui::Button("Refresh roads"))
        run(session, "EditorRoad.refresh");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Rebuilds every road from its template.");
    ImGui::SameLine();
    if (ImGui::Button("Find unused road meshes"))
        run(session, "EditorRoad.cleanFolder 0");
    ImGui::SameLine();
    if (ImGui::Button("Move them aside"))
        run(session, "EditorRoad.cleanFolder 1");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Into the level's Roads/Unused; nothing is deleted.");

    // Connect: a new road on the active spline.
    ImGui::SeparatorText("Connect a road");
    const int active = session.activeSpline();
    if (m_newRoadTemplate.empty() && !templates.empty())
        m_newRoadTemplate = templates[0];
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##newroad", "road name", m_newRoadName, sizeof(m_newRoadName));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##newtemplate", m_newRoadTemplate.c_str()))
    {
        for (size_t i = 0; i < templates.size(); ++i)
            if (ImGui::Selectable(templates[i].c_str(), templates[i] == m_newRoadTemplate))
                m_newRoadTemplate = templates[i];
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(active < 0 || !m_newRoadName[0] || m_newRoadTemplate.empty());
    if (ImGui::Button(active < 0 ? "Connect (no active spline)" : ("Connect to spline " + number(active)).c_str()))
    {
        run(session, "EditorRoad.activeTemplate " + m_newRoadTemplate);
        run(session, "EditorRoad.connect " + std::string(m_newRoadName));
        if (roads.hasRoad(m_newRoadName))
        {
            m_selectedRoad = m_newRoadName;
            m_newRoadName[0] = 0;
        }
    }
    ImGui::EndDisabled();
    const std::string connected = active >= 0 ? roads.roadOnSpline(active) : std::string();
    if (!connected.empty())
    {
        ImGui::SameLine();
        if (ImGui::Button(("Disconnect " + connected).c_str()))
        {
            run(session, "EditorRoad.disconnect");
            if (!roads.hasRoad(connected) && m_selectedRoad == connected)
                m_selectedRoad.clear();
        }
    }

    RoadRecord road;
    if (m_selectedRoad.empty() || !roads.roadRecord(m_selectedRoad, road))
        return;
    const std::string name = road.name;
    ImGui::SeparatorText(("Road " + name).c_str());
    if (ImGui::Button("Build final"))
        run(session, "EditorRoad.build " + name + " final");
    ImGui::SameLine();
    if (ImGui::Button("Build ghost"))
        run(session, "EditorRoad.build " + name + " ghost");
    ImGui::SameLine();
    if (ImGui::Button("Build all final"))
        run(session, "EditorRoad.build * final");
    ImGui::SameLine();
    if (ImGui::Button("Apply to terrain"))
        run(session, "EditorRoad.apply " + name);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Sets the ground under the road to the road's surface (one undoable terrain change).");
    ImGui::SameLine();
    if (ImGui::Button("Delete road"))
    {
        run(session, "EditorRoad.delete " + name);
        if (!roads.hasRoad(name))
            m_selectedRoad.clear();
        return;
    }
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("Template", road.templateName.c_str()))
    {
        for (size_t i = 0; i < templates.size(); ++i)
            if (ImGui::Selectable(templates[i].c_str(), templates[i] == road.templateName))
                run(session, "EditorRoad.set " + name + " template " + templates[i]);
        ImGui::EndCombo();
    }
    int spline = road.splineId;
    if (integerField("Spline", spline))
        run(session, "EditorRoad.set " + name + " spline " + number(spline));
    ImGui::SameLine();
    int seed = road.seed;
    if (integerField("Noise seed", seed))
        run(session, "EditorRoad.set " + name + " seed " + number(seed));
    float persistence = road.persistence;
    if (numberField("Persistence", persistence))
        run(session, "EditorRoad.set " + name + " persistence " + formatConsoleFloat(persistence));
    ImGui::SameLine();
    int octaves = road.octaves;
    if (integerField("Octaves", octaves))
        run(session, "EditorRoad.set " + name + " octaves " + number(octaves));
    float range[2] = { road.range[0], road.range[1] };
    if (pairField("Noise range", range))
        run(session, "EditorRoad.set " + name + " range " + pair(range));
    float offset[2] = { road.textureScaleOffset[0], road.textureScaleOffset[1] };
    if (pairField("Texture scale offset", offset))
        run(session, "EditorRoad.set " + name + " textureScaleOffset " + pair(offset));
    int priority = road.priority;
    if (integerField("Priority", priority))
        run(session, "EditorRoad.set " + name + " priority " + number(priority));
    struct Fade { const char* label; const char* key; const float* value; };
    const Fade fades[] = { { "Fade at start, left", "fadeStartLeft", road.fadeStartLeft },
                           { "Fade at start, right", "fadeStartRight", road.fadeStartRight },
                           { "Fade at end, left", "fadeEndLeft", road.fadeEndLeft },
                           { "Fade at end, right", "fadeEndRight", road.fadeEndRight } };
    for (size_t i = 0; i < sizeof(fades) / sizeof(fades[0]); ++i)
    {
        float v[2] = { fades[i].value[0], fades[i].value[1] };
        if (pairField(fades[i].label, v))
            run(session, "EditorRoad.set " + name + " " + fades[i].key + " " + pair(v));
    }
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##rename", "new name", m_renameBuffer, sizeof(m_renameBuffer));
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_renameBuffer[0]);
    if (ImGui::Button("Rename"))
    {
        run(session, "EditorRoad.rename " + name + " " + m_renameBuffer);
        if (roads.hasRoad(m_renameBuffer))
            m_selectedRoad = m_renameBuffer;
        m_renameBuffer[0] = 0;
    }
    ImGui::EndDisabled();
}

void RoadPanel::drawTemplates(EditSession& session)
{
    RoadNetwork& roads = session.roads();
    const std::vector<std::string> names = roads.templateNames();
    if (ImGui::BeginListBox("##templates", ImVec2(-1.0f, 6.0f * ImGui::GetTextLineHeightWithSpacing())))
    {
        for (size_t i = 0; i < names.size(); ++i)
        {
            const size_t users = roads.roadsWithTemplate(names[i]).size();
            const std::string label = names[i] + (users ? " (" + number(static_cast<int>(users)) + " roads)" : std::string());
            if (ImGui::Selectable(label.c_str(), names[i] == m_selectedTemplate))
                m_selectedTemplate = names[i];
        }
        ImGui::EndListBox();
    }
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##newtemplate", "template name", m_newTemplateName, sizeof(m_newTemplateName));
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_newTemplateName[0]);
    const bool copy = roads.hasTemplate(m_selectedTemplate);
    if (ImGui::Button(copy ? ("New, copied from " + m_selectedTemplate).c_str() : "New template"))
    {
        run(session, copy ? "EditorRoad.templateCreateFrom " + std::string(m_newTemplateName) + " " + m_selectedTemplate
                          : "EditorRoad.templateCreate " + std::string(m_newTemplateName));
        if (roads.hasTemplate(m_newTemplateName))
        {
            m_selectedTemplate = m_newTemplateName;
            m_newTemplateName[0] = 0;
        }
    }
    ImGui::EndDisabled();

    RoadTemplateRecord t;
    if (m_selectedTemplate.empty() || !roads.templateRecord(m_selectedTemplate, t))
        return;
    const std::string name = t.name;
    ImGui::SeparatorText(("Template " + name).c_str());
    const std::string set = "EditorRoad.templateSet " + name + " ";
    float width = t.width;
    if (numberField("Width", width))
        run(session, set + "width " + formatConsoleFloat(width));
    ImGui::SameLine();
    float error = t.maxTesselatedError;
    if (numberField("Max tesselated error", error))
        run(session, set + "maxTesselatedError " + formatConsoleFloat(error));
    float blend = t.blendFactor;
    if (numberField("Blend factor", blend))
        run(session, set + "blendFactor " + formatConsoleFloat(blend));
    ImGui::SameLine();
    int priority = t.priority;
    if (integerField("Priority", priority))
        run(session, set + "priority " + number(priority));
    char material[96];
    std::snprintf(material, sizeof(material), "%s", t.material.c_str());
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputTextWithHint("Material", "none", material, sizeof(material), ImGuiInputTextFlags_EnterReturnsTrue))
        run(session, set + "material \"" + material + "\"");
    float fade[2] = { t.fadeOffDistance[0], t.fadeOffDistance[1] };
    if (pairField("Fade-off distance", fade))
        run(session, set + "fadeOffDistance " + pair(fade));
    for (int layer = 0; layer < 2; ++layer)
    {
        ImGui::PushID(layer);
        const std::string n = number(layer);
        ImGui::TextUnformatted(layer == 0 ? "Primary texture layer" : "Secondary texture layer");
        char texture[260];
        std::snprintf(texture, sizeof(texture), "%s", t.textures[layer].c_str());
        ImGui::SetNextItemWidth(-90.0f);
        if (ImGui::InputTextWithHint("##texture", "none", texture, sizeof(texture), ImGuiInputTextFlags_EnterReturnsTrue))
            run(session, set + "texture" + n + " \"" + texture + "\"");
        ImGui::SameLine();
        std::string picked;
        const std::string folder = t.textures[layer].empty() ? std::string("objects\\roads\\textures\\x") : t.textures[layer];
        if (TexturePalette::button("palette", "texture", folder, picked))
            run(session, set + "texture" + n + " \"" + picked + "\"");
        bool mirror = t.mirror[layer];
        if (ImGui::Checkbox("Mirror", &mirror))
            run(session, set + "mirror" + n + " " + (mirror ? "1" : "0"));
        ImGui::SameLine();
        float scale[2] = { t.scale[layer][0], t.scale[layer][1] };
        if (pairField("Scale", scale))
            run(session, set + "scale" + n + " " + pair(scale));
        ImGui::PopID();
    }
    const size_t users = roads.roadsWithTemplate(name).size();
    ImGui::BeginDisabled(users != 0);
    if (ImGui::Button("Delete template"))
    {
        run(session, "EditorRoad.templateDelete " + name);
        if (!roads.hasTemplate(name))
            m_selectedTemplate.clear();
    }
    ImGui::EndDisabled();
    if (users)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(%u roads use it)", static_cast<unsigned>(users));
    }
    ImGui::TextDisabled("A changed template is written into the mod's loose objects folder when the roads are saved.");
}

} // namespace bf2editor
