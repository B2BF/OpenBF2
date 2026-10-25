// Code/Tools/BF2Editor/App/TerrainPanel.cpp - see TerrainPanel.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/TerrainPanel.h"
#include "App/TestHarness.h"

#include "App/EditSession.h"
#include "App/TexturePalette.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <windows.h>
#include <commdlg.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace bf2editor
{

namespace
{

struct BrushHelp
{
    TerrainBrush mode;
    const char*  label;
    const char*  help;
};

// What each brush does with the settings (the engine's brushes, Physics/HeightmapClusterEditable).
const BrushHelp kBrushes[] = {
    { TerrainBrush::Modify, "Modify",
      "Left raises, middle (or Shift+left) lowers: strength x 0.5 metres a second at the centre, less towards the edge." },
    { TerrainBrush::Smooth, "Smooth",
      "Left pulls each vertex towards the mean of its neighbours, by strength per cent a frame." },
    { TerrainBrush::Level, "Level",
      "Left moves the vertices towards the height at the brush centre (shown under Height), by strength per cent a frame." },
    { TerrainBrush::Platform, "Platform",
      "Left moves the vertices towards Height, by strength per cent a frame; middle (or Shift+left) takes Height from the ground." },
    { TerrainBrush::Random, "Random",
      "Left adds noise of up to strength metres to each vertex, every frame (as shipped: use a low strength)." },
    { TerrainBrush::Paint, "Paint",
      "Left paints the selected layer's detail texture or colour (Paints), middle (or Shift+left) erases; the circle is "
      "what the renderer paints. Merge keeps the strokes, Clear drops the unmerged ones. Saved with the level into its "
      "Editor folder; the game draws the paint after a terrain compile, which this version cannot do yet. Needs the "
      "editable terrain." },
    { TerrainBrush::Overgrowth, "Overgrowth",
      "Left paints the selected overgrowth material (or the shadow map's value) into the level's overgrowth maps, "
      "middle (or Shift+left) erases; the circle is what the renderer paints. The Overgrowth looks show the maps. Not "
      "undoable (the maps live in the renderer, as in retail); the plants themselves change when the overgrowth is "
      "generated, and the level's save writes the maps and the regenerated plants. Needs the editable terrain and a "
      "level with overgrowth." },
};

// A sector's name as the heightmap files spell it (HeightmapSecondary_<name>.raw).
std::string sectorName(int x, int z)
{
    if (!x && !z)
        return "primary";
    std::string name;
    if (x)
        name += x < 0 ? "L1" : "R1";
    if (z)
        name += z < 0 ? "U1" : "D1";
    return name;
}

// One ground setting from the panel: an undoable EditorGround.set (layer -1: the terrain's own settings).
void setGround(EditSession& session, int layer, const std::string& name, const std::string& value)
{
    session.runCommand("EditorGround.set " + std::to_string(layer) + " " + name + " \"" + value + "\"", "terrain");
}

// A texture of a ground layer: its path (as the file writes it, with ".dds") and the texture palette beside it, which
// lists the mod's terrain textures and gives a path without the extension.
void groundTexture(EditSession& session, int layer, const char* name, const char* label, const std::string& value)
{
    ImGui::PushID(name);
    char buffer[260];
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize(label).x - 48.0f);
    if (ImGui::InputTextWithHint("##path", "none", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
        setGround(session, layer, name, buffer);
    ImGui::SameLine();
    std::string picked;
    const std::string folder = value.empty() ? std::string("common\\terrain\\textures\\x") : value;
    if (TexturePalette::button("palette", name, folder, picked))
        setGround(session, layer, name, picked + ".dds");
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::PopID();
}

// A number of a ground layer (or of the terrain, layer -1), set on Enter.
void groundNumber(EditSession& session, int layer, const char* name, const char* label, float value)
{
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::InputFloat(label, &value, 0.0f, 0.0f, "%g", ImGuiInputTextFlags_EnterReturnsTrue))
        setGround(session, layer, name, GroundProperties::number(value));
}

// An x/y pair, set on Enter.
void groundPair(EditSession& session, int layer, const char* name, const char* label, const float pair[2])
{
    float v[2] = { pair[0], pair[1] };
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputFloat2(label, v, "%g", ImGuiInputTextFlags_EnterReturnsTrue))
        setGround(session, layer, name, GroundProperties::number(v[0]) + "/" + GroundProperties::number(v[1]));
}

// The ground properties (Editor/GroundProperties.con, the retail TerrainEditor plugin's file): the six detail layers
// and the terrain's far settings. Every change is one undoable edit, saved with the level.
void drawGroundProperties(EditSession& session, const EditableTerrain& editable)
{
    const GroundProperties& ground = editable.ground();
    ImGui::TextWrapped("From %s%s.", editable.groundSource().c_str(), editable.groundChanged() ? " (changed)" : "");
    for (int layer = 0; layer < 6; ++layer)
    {
        ImGui::PushID(layer);
        const std::map<int, GroundLayer>::const_iterator it = ground.layers.find(layer);
        const GroundLayer l = it != ground.layers.end() ? it->second : GroundLayer();
        std::string title = "Layer " + std::to_string(layer + 1) + ": ";
        if (it == ground.layers.end())
            title += "not in the file";
        else
        {
            const size_t slash = l.detailTexture.find_last_of("\\/");
            title += l.detailTexture.empty() ? std::string("no detail texture")
                                             : l.detailTexture.substr(slash == std::string::npos ? 0 : slash + 1);
        }
        if (ImGui::TreeNode("layer", "%s", title.c_str()))
        {
            groundTexture(session, layer, "detailTexture", "Detail texture", l.detailTexture);
            groundTexture(session, layer, "colorTexture", "Colour texture", l.colorTexture);
            groundNumber(session, layer, "topTiling", "Top tiling", l.topTiling);
            ImGui::SameLine();
            groundNumber(session, layer, "yOffset", "Y offset", l.yOffset);
            groundPair(session, layer, "sideTiling", "Side tiling", l.sideTiling);
            groundPair(session, layer, "colorTiling", "Colour tiling", l.colorTiling);
            bool planeMap = l.planeMap != 0;
            if (ImGui::Checkbox("Plane mapping", &planeMap))
                setGround(session, layer, "planeMap", planeMap ? "1" : "0");
            ImGui::SetItemTooltip("The layer's texture is projected on the three axis planes (cliffs), with the side tiling.");
            ImGui::SameLine();
            bool envMap = l.envMap;
            if (ImGui::Checkbox("Environment map", &envMap))
                setGround(session, layer, "envMap", envMap ? "1" : "0");
            int lowDetail = l.lowDetailType;
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::InputInt("Low-detail type", &lowDetail, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue))
                setGround(session, layer, "lowDetailType", std::to_string(lowDetail));
            char colour[32];
            std::snprintf(colour, sizeof(colour), "%d/%d/%d", l.color[0], l.color[1], l.color[2]);
            ImGui::ColorButton("##swatch", ImVec4(l.color[0] / 255.0f, l.color[1] / 255.0f, l.color[2] / 255.0f, 1.0f),
                               ImGuiColorEditFlags_NoTooltip);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::InputText("Colour (r/g/b)", colour, sizeof(colour), ImGuiInputTextFlags_EnterReturnsTrue))
                setGround(session, layer, "color", colour);
            char material[96];
            std::snprintf(material, sizeof(material), "%s", l.material.c_str());
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::InputTextWithHint("Material", "none", material, sizeof(material), ImGuiInputTextFlags_EnterReturnsTrue))
                setGround(session, layer, "material", material);
            ImGui::TextDisabled("The colour texture is what Paint samples \"from the layer's texture\". The material is "
                                "kept in the file; the renderer does not use it yet.");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (ImGui::TreeNode("Far terrain and LOD"))
    {
        const GroundMisc& m = ground.misc;
        groundNumber(session, -1, "farTopTilingHi", "Far top tiling (high)", m.farTopTilingHi);
        groundNumber(session, -1, "farTopTilingLow", "Far top tiling (low)", m.farTopTilingLow);
        groundPair(session, -1, "farSideTiling", "Far side tiling", m.farSideTiling);
        groundNumber(session, -1, "farYOffset", "Far Y offset", m.farYOffset);
        groundNumber(session, -1, "lodSwitchDistance", "LOD switch distance", m.lodSwitchDistance);
        ImGui::TextDisabled("The far settings are kept in the file; the renderer takes them from Terrain.con.");
        ImGui::TreePop();
    }
}

// The Windows open dialog for one file; false when cancelled.
bool pickFile(const char* filter, char* path, size_t size)
{
    OPENFILENAMEA dialog;
    std::memset(&dialog, 0, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(size);
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&dialog) != 0;
}

} // namespace

TerrainPanel::TerrainPanel()
    : m_changedRevision(0)
    , m_changedKnown(false)
    , m_changed(false)
    , m_smoothWidth(32)
    , m_smoothAmplitude(0.5f)
    , m_smoothFalloff(2.0f)
{
    m_importPath[0] = 0;
}

void TerrainPanel::draw(EditSession& session, bool* open)
{
    // A layout saved before the panel existed: open it beside the Level settings panel the first time.
    ImGuiWindow* neighbour = ImGui::FindWindowByName("Level settings");
    if (neighbour && neighbour->DockId != 0)
        ImGui::SetNextWindowDockID(neighbour->DockId, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Terrain", open))
    {
        ImGui::End();
        return;
    }
    TerrainEditing& terrain = session.terrain();
    std::string why;
    if (!terrain.available(why))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "The terrain cannot be edited: %s.", why.c_str());
        ImGui::End();
        return;
    }

    // The drawn ground: the renderer's TerrainEditable and its looks (the retail terrain render modes), or the compiled
    // terrain when the editor started with +editableTerrain 0.
    EditableTerrain& editable = session.editableTerrain();
    if (editable.active())
    {
        static const char* const kLooks[] = { "Colormap and lightmap", "Grid", "Topographic grid", "Detail layers",
                                              "Undergrowth overlay", "Overgrowth", "Overgrowth shadow", "Base pass" };
        const int current = editable.technique();
        ImGui::SetNextItemWidth(200.0f);
        const bool open = ImGui::BeginCombo("Ground drawn as", current >= 0 && current < 8 ? kLooks[current] : "?");
        if (!open)
            noteTestItem("Ground drawn as");    // the closed box, for a scripted click (ImGui reports no label for it)
        if (open)
        {
            for (int t = 0; t < 8; ++t)
                if (kLooks[t][0] && ImGui::Selectable(kLooks[t], t == current) && t != current)
                    session.runCommand("EditorTerrain.technique " + std::to_string(t), "terrain");
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("The renderer's TerrainEditable follows every height edit (Terrain.con's editor branch). "
                              "Overgrowth, its shadow and the material map stop at the renderer's frontiers.");
    }
    else
        ImGui::TextDisabled("Ground: the compiled terrain (it does not follow height edits).");

    bool tool = session.terrainToolActive();
    if (ImGui::Checkbox("Brush tool (T)", &tool))
        session.runCommand(tool ? "EditorTerrain.tool 1" : "EditorTerrain.tool 0", "terrain");
    ImGui::SetItemTooltip("While it is on, the left button paints in the view and objects are not picked; 1, 2 or 3 "
                          "turn it off.");

    // The brushes, as many to a row as the panel is wide.
    TerrainBrushSettings& brush = session.terrainBrush();
    const BrushHelp* help = 0;
    const ImGuiStyle& style = ImGui::GetStyle();
    float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    for (size_t i = 0; i < sizeof(kBrushes) / sizeof(kBrushes[0]); ++i)
    {
        const float width = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(kBrushes[i].label).x;
        if (i && ImGui::GetItemRectMax().x + style.ItemSpacing.x + width <= rowRight)
            ImGui::SameLine();
        if (ImGui::RadioButton(kBrushes[i].label, brush.mode == kBrushes[i].mode))
            brush.mode = kBrushes[i].mode;
        if (brush.mode == kBrushes[i].mode)
            help = &kBrushes[i];
    }
    if (help)
        ImGui::TextWrapped("%s", help->help);

    const float fieldWidth = -ImGui::CalcTextSize("Strength").x - style.ItemInnerSpacing.x - 2.0f;
    char sizeFormat[48];
    std::snprintf(sizeFormat, sizeof(sizeFormat), "%%d  (%.0f m)", static_cast<float>(brush.size) * terrain.spacing());
    ImGui::SetNextItemWidth(fieldWidth);
    ImGui::SliderInt("Size", &brush.size, 1, 50, sizeFormat, ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("The brush's radius in vertices of the primary heightmap (Ctrl+wheel in the view).");
    ImGui::SetNextItemWidth(fieldWidth);
    ImGui::SliderInt("Strength", &brush.strength, 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Shift+wheel in the view.");
    if (brush.mode == TerrainBrush::Overgrowth)
    {
        // The retail panel's mode 9 controls: the map, the material (the level's Overgrowth.con) or the shadow value.
        if (!m_overgrowthKnown)
        {
            m_overgrowthMaterials = session.editableTerrain().overgrowthMaterials();
            m_overgrowthKnown = true;
        }
        ImGui::TextUnformatted("Paints");
        ImGui::SameLine();
        if (ImGui::RadioButton("Material map", !brush.overgrowthShadow))
            brush.overgrowthShadow = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Shadow map", brush.overgrowthShadow))
            brush.overgrowthShadow = true;
        if (brush.overgrowthShadow)
        {
            ImGui::SetNextItemWidth(fieldWidth);
            ImGui::SliderInt("Shadow", &brush.overgrowthShade, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
        }
        else if (m_overgrowthMaterials.empty())
            ImGui::TextDisabled("This level has no overgrowth materials.");
        else
        {
            std::string current = "?";
            for (size_t i = 0; i < m_overgrowthMaterials.size(); ++i)
                if (m_overgrowthMaterials[i].second == brush.overgrowthMaterial)
                    current = m_overgrowthMaterials[i].first;
            ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::BeginCombo("Material", current.c_str()))
            {
                for (size_t i = 0; i < m_overgrowthMaterials.size(); ++i)
                {
                    char item[96];
                    std::snprintf(item, sizeof(item), "%s (%d)", m_overgrowthMaterials[i].first.c_str(),
                                  m_overgrowthMaterials[i].second);
                    if (ImGui::Selectable(item, m_overgrowthMaterials[i].second == brush.overgrowthMaterial))
                        brush.overgrowthMaterial = m_overgrowthMaterials[i].second;
                }
                ImGui::EndCombo();
            }
        }
        const int look = brush.overgrowthShadow ? 6 : 5;
        if (session.editableTerrain().technique() != look && ImGui::SmallButton("Show the overgrowth map"))
            session.runCommand("EditorTerrain.technique " + std::to_string(look), "terrain");
        if (ImGui::SmallButton("Generate the plants"))
            session.runCommand("EditorTerrain.generateOvergrowth", "terrain");
        ImGui::SetItemTooltip("Regrows the plants from the painted maps (in the editor's staging folder; the level's "
                              "overgrowth files are written when the level is saved).");
    }
    else if (brush.mode == TerrainBrush::Paint)
    {
        // The retail panel's mode 7 controls.
        ImGui::TextUnformatted("Paints");
        ImGui::SameLine();
        if (ImGui::RadioButton("Detail", brush.paintMode == TerrainPaintMode::Detail))
            brush.paintMode = TerrainPaintMode::Detail;
        ImGui::SameLine();
        if (ImGui::RadioButton("Color", brush.paintMode == TerrainPaintMode::Color))
            brush.paintMode = TerrainPaintMode::Color;
        ImGui::SameLine();
        if (ImGui::RadioButton("Both", brush.paintMode == TerrainPaintMode::Both))
            brush.paintMode = TerrainPaintMode::Both;
        static const char* const kLayers[] = { "(none)", "Layer 1", "Layer 2", "Layer 3", "Layer 4", "Layer 5", "Layer 6" };
        int layerItem = brush.layer + 1;
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::Combo("Layer", &layerItem, kLayers, 7))
            brush.layer = layerItem - 1;
        ImGui::SetItemTooltip("The detail layer painted, and the one whose colour texture the colour comes from.");
        if (brush.paintMode != TerrainPaintMode::Detail)
        {
            if (ImGui::RadioButton("From the layer's texture", brush.colorFromTexture))
                brush.colorFromTexture = true;
            ImGui::SameLine();
            if (ImGui::RadioButton("This colour", !brush.colorFromTexture))
                brush.colorFromTexture = false;
            if (!brush.colorFromTexture)
            {
                float rgb[3] = { static_cast<float>(brush.color & 0xFFu) / 255.0f,
                                 static_cast<float>((brush.color >> 8) & 0xFFu) / 255.0f,
                                 static_cast<float>((brush.color >> 16) & 0xFFu) / 255.0f };
                ImGui::SetNextItemWidth(fieldWidth);
                if (ImGui::ColorEdit3("Colour", rgb))
                    brush.color = 0xFF000000u | (static_cast<unsigned>(rgb[2] * 255.0f + 0.5f) << 16)
                                | (static_cast<unsigned>(rgb[1] * 255.0f + 0.5f) << 8) | static_cast<unsigned>(rgb[0] * 255.0f + 0.5f);
            }
        }
        if (ImGui::Button("Merge"))
            session.runCommand(std::string("EditorTerrain.merge ") + terrainPaintModeName(brush.paintMode), "terrain");
        ImGui::SetItemTooltip("Keep the strokes (retail's Merge: the edits become the terrain's own maps).");
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            session.runCommand(std::string("EditorTerrain.clearEdits ") + terrainPaintModeName(brush.paintMode), "terrain");
        ImGui::SetItemTooltip("Drop the strokes made since the last Merge (retail's Clear).");
        if (session.editableTerrain().technique() != 3)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Show the detail layers"))
                session.runCommand("EditorTerrain.technique 3", "terrain");
            ImGui::SetItemTooltip("Strokes show at once in the detail layers look; the colormap look shows colour after Merge.");
        }
    }
    else
    {
        ImGui::Checkbox("Raise", &brush.raise);
        ImGui::SetItemTooltip("Vertices may move up.");
        ImGui::SameLine();
        ImGui::Checkbox("Lower", &brush.lower);
        ImGui::SetItemTooltip("Vertices may move down.");
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::InputFloat("Height", &brush.height, 1.0f, 10.0f, "%.3f");
        ImGui::SetItemTooltip("Metres. Platform builds to it; Level shows the height it levels to.");
    }

    if (editable.active() && ImGui::CollapsingHeader("Ground properties"))
        drawGroundProperties(session, editable);

    // The surrounding terrain (census §10).
    if (ImGui::CollapsingHeader("Surrounding terrain"))
    {
        const std::vector<TerrainEditing::Sector> sectors = terrain.sectors();
        const float cell = 74.0f;
        for (int z = -1; z <= 1; ++z)
        {
            for (int x = -1; x <= 1; ++x)
            {
                const TerrainEditing::Sector* found = 0;
                for (size_t i = 0; i < sectors.size(); ++i)
                    if (sectors[i].x == x && sectors[i].z == z)
                        found = &sectors[i];
                if (x > -1)
                    ImGui::SameLine();
                ImGui::PushID(x * 10 + z);
                const std::string label = sectorName(x, z);
                if (!found || !found->present || (!x && !z))
                {
                    ImGui::BeginDisabled();
                    ImGui::Button(found && found->present ? label.c_str() : "-", ImVec2(cell, cell * 0.5f));
                    ImGui::EndDisabled();
                }
                else
                {
                    const bool active = found->modified;
                    ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.2f, 0.45f, 0.25f, 1.0f) : ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                    if (ImGui::Button(label.c_str(), ImVec2(cell, cell * 0.5f)))
                    {
                        char line[96];
                        std::snprintf(line, sizeof(line), "EditorTerrain.sector %d %d %d", x, z, active ? 0 : 1);
                        session.runCommand(line, "terrain");
                    }
                    ImGui::PopStyleColor();
                    ImGui::SetItemTooltip("%dx%d, %d-bit; %s (a click toggles it; right-click: show it in the view)",
                                          found->width, found->width, found->bits,
                                          active ? "active: saved with the level" : "inactive");
                    // The renderer's ring patch for the sector (retail's Show / Hide): a dot on a shown tile.
                    if (editable.active())
                    {
                        const bool shown = editable.sectorShown(x, z);
                        if (shown)
                            ImGui::GetWindowDrawList()->AddCircleFilled(
                                ImVec2(ImGui::GetItemRectMax().x - 7.0f, ImGui::GetItemRectMin().y + 7.0f), 3.5f,
                                IM_COL32(120, 200, 255, 255));
                        if (ImGui::BeginPopupContextItem("show"))
                        {
                            if (ImGui::MenuItem("Shown in the view", 0, shown))
                            {
                                char line[96];
                                std::snprintf(line, sizeof(line), "EditorTerrain.showSector %d %d %d", x, z, shown ? 0 : 1);
                                session.runCommand(line, "terrain");
                            }
                            ImGui::EndPopup();
                        }
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::TextDisabled("Green tiles are active: Heightdata.con marks them modified.");
        if (editable.active())
        {
            // Every surrounding tile drawn or none (the renderer's ring patches; the view only).
            for (int show = 1; show >= 0; --show)
            {
                if (show == 0)
                    ImGui::SameLine();
                if (ImGui::SmallButton(show ? "Show all surrounding" : "Hide all surrounding"))
                    for (int z = -1; z <= 1; ++z)
                        for (int x = -1; x <= 1; ++x)
                            if ((x || z) && editable.sectorShown(x, z) != (show != 0))
                            {
                                char line[96];
                                std::snprintf(line, sizeof(line), "EditorTerrain.showSector %d %d %d", x, z, show);
                                session.runCommand(line, "terrain");
                            }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(a dot: shown in the view)");
        }
        ImGui::SetNextItemWidth(-150.0f);
        ImGui::InputTextWithHint("##surroundingRaw", "8-bit .raw of (3s-2)x(3s-2) samples", m_importPath, sizeof(m_importPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
            pickFile("Raw heightmaps (*.raw)\0*.raw\0All files\0*.*\0", m_importPath, sizeof(m_importPath));
        ImGui::SameLine();
        if (ImGui::Button("Import") && m_importPath[0])
            session.runCommand(std::string("EditorTerrain.importSurrounding \"") + m_importPath + "\"", "terrain");
        ImGui::SetItemTooltip("Copies the raw into the eight surrounding tiles (one undoable edit).");
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputInt("Width##smooth", &m_smoothWidth);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputFloat("Amplitude##smooth", &m_smoothAmplitude, 0.0f, 0.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputFloat("Falloff##smooth", &m_smoothFalloff, 0.0f, 0.0f, "%.2f");
        if (ImGui::Button("Smooth edges"))
        {
            char line[128];
            std::snprintf(line, sizeof(line), "EditorTerrain.smoothEdges %d %g %g", m_smoothWidth, m_smoothAmplitude,
                          m_smoothFalloff);
            session.runCommand(line, "terrain");
        }
        ImGui::SetItemTooltip("Blends the heights across the primary tile's edges over Width metres (the retail button: "
                              "32, 0.5, 2; a falloff of 0 adds noise of +/- Amplitude instead).");
    }

    // The water height (the cluster's sea level, saved in Heightdata.con).
    float sea = 0.0f;
    if (terrain.seaLevel(sea))
    {
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::InputFloat("Sea level", &sea, 0.5f, 5.0f, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
        {
            char line[96];
            std::snprintf(line, sizeof(line), "EditorTerrain.seaLevel %.3f", sea);
            session.runCommand(line, "terrain");
        }
        ImGui::SetItemTooltip("The level's water height in metres (heightmapcluster.setSeaWaterLevel).");
    }

    ImGui::Separator();
    // The cluster, and whether its heights changed (asked again only after the history moved).
    const unsigned revision = session.history().revision();
    if (!m_changedKnown || revision != m_changedRevision)
    {
        m_changed = terrain.changedSinceSave();
        m_describe = terrain.describe();
        m_changedRevision = revision;
        m_changedKnown = true;
    }
    ImGui::TextWrapped("%s", m_describe.c_str());
    if (m_changed)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "The heights changed since the level was opened.");
        ImGui::TextWrapped("This version cannot save the terrain yet: the game draws the ground from the level's "
                           "compiled terrain (terraindata.raw), which the renderer's editable terrain must write "
                           "together with the heights, and that part is still being recovered. A save writes the "
                           "objects and settings and says the terrain was not saved.");
    }
    if (!editable.active())
        ImGui::TextWrapped("The view does not redraw the edited ground (the editor started with +editableTerrain 0); the "
                           "brush's dots follow the edited heights, and so does everything set on the ground.");

    // Undergrowth: the density map painted with the level's materials (App/EditSessionUndergrowth.cpp).
    if (ImGui::CollapsingHeader("Undergrowth", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const UndergrowthMap* map = session.undergrowthMap();
        const UndergrowthConfig* config = session.undergrowthConfig();
        if (!map || !config)
            ImGui::TextDisabled("This level has no undergrowth (Undergrowth.raw) to paint.");
        else
        {
            bool tool = session.undergrowthToolActive();
            if (ImGui::Checkbox("Undergrowth tool (U)", &tool))
                session.runCommand(tool ? "EditorUndergrowth.tool 1" : "EditorUndergrowth.tool 0", "undergrowth");
            ImGui::SetItemTooltip("The left button paints the material below into the density map, the middle button or "
                                  "Shift+left erases; Ctrl+wheel the radius. One undoable edit per stroke.");
            float radius = session.undergrowthRadius();
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::SliderFloat("Radius##undergrowth", &radius, 0.5f, 100.0f, "%.1f m", ImGuiSliderFlags_Logarithmic))
            {
                std::string ignored;
                session.setUndergrowthRadius(radius, ignored);
            }
            const std::vector<int> counts = map->counts();
            for (size_t i = 0; i < config->materials.size(); ++i)
            {
                const UndergrowthMaterial& material = config->materials[i];
                char label[160];
                std::snprintf(label, sizeof(label), "%s (%d)  %d cells##ug%u", material.name.c_str(), material.id,
                              material.id >= 0 && material.id < 256 ? counts[static_cast<size_t>(material.id)] : 0,
                              static_cast<unsigned>(i));
                if (ImGui::RadioButton(label, session.undergrowthMaterial() == material.id))
                {
                    char line[64];
                    std::snprintf(line, sizeof(line), "EditorUndergrowth.material %d", material.id);
                    session.runCommand(line, "undergrowth");
                }
            }
            ImGui::TextDisabled("%dx%d cells, %.2f m each; %d without undergrowth%s", map->size(), map->size(),
                                session.undergrowthScale(), counts[0],
                                session.undergrowthChanged() ? "; painted - saved with the level" : "");
            ImGui::TextWrapped("The 1.50 renderer keeps the undergrowth it loaded: what is painted grows when the level "
                               "loads again. The tool's dots show the map as painted.");
        }
    }
    ImGui::End();
}

} // namespace bf2editor
