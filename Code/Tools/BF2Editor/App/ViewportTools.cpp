// Code/Tools/BF2Editor/App/ViewportTools.cpp - see ViewportTools.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/ViewportTools.h"

#include "App/EditSession.h"
#include "App/FreeCamera.h"
#include "App/ImGuiHost.h"
#include "App/ResourceBrowser.h"
#include "App/ShortcutKeys.h"
#include "Framework/EditorConsole.h"
#include "Framework/EditorLog.h"
#include "Framework/Matrix4.h"

#include "Core/Ids.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/IObjectTemplate.h"
#include "Scene/Object/ObjectManager.h"
#include "Math/LocalBoundingBox.h"
#include "RendMisc/IRenderView.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using dice::hfe::Mat4;
using dice::hfe::world::IObject;

namespace bf2editor
{

namespace
{

// Where the ray through window pixel (x, y) meets the horizontal plane at `height`; false when it does
// not (the ray runs level or away from the plane).
bool pointOnPlane(const FreeCamera& camera, int width, int height, float x, float y, float planeHeight,
                  float out[3])
{
    float origin[3], direction[3];
    camera.rayThroughPixel(x, y, width, height, origin, direction);
    if (direction[1] > -1e-4f && direction[1] < 1e-4f)
        return false;
    const float t = (planeHeight - origin[1]) / direction[1];
    if (t <= 0.0f)
        return false;
    for (int i = 0; i < 3; ++i)
        out[i] = origin[i] + direction[i] * t;
    return true;
}

// A point of the Catmull-Rom segment from p1 to p2 (the tangents half the neighbours' differences), as the renderer's
// EditorSpline samples its curve (GeomMisc/EditorSpline.cpp, catmullRomPoint).
void catmullRom(const float p0[3], const float p1[3], const float p2[3], const float p3[3], float t, float out[3])
{
    const float t2 = t * t, t3 = t2 * t;
    const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f, h01 = 3.0f * t2 - 2.0f * t3;
    const float h10 = t3 - 2.0f * t2 + t, h11 = t3 - t2;
    for (int k = 0; k < 3; ++k)
        out[k] = p1[k] * h00 + p2[k] * h01 + 0.5f * (p2[k] - p0[k]) * h10 + 0.5f * (p3[k] - p1[k]) * h11;
}

// The curve samples of a spline's segment i (to the next point), `steps` + 1 of them.
void splineSegment(const SplineRecord& spline, size_t i, int steps, std::vector<std::vector<float> >& out)
{
    const size_t n = spline.points.size();
    const float* p0 = spline.points[i ? i - 1 : 0].position;
    const float* p1 = spline.points[i].position;
    const float* p2 = spline.points[i + 1].position;
    const float* p3 = spline.points[i + 2 < n ? i + 2 : n - 1].position;
    out.clear();
    for (int k = 0; k <= steps; ++k)
    {
        std::vector<float> p(3);
        catmullRom(p0, p1, p2, p3, static_cast<float>(k) / static_cast<float>(steps), &p[0]);
        out.push_back(p);
    }
}

// A world point to window pixels through view * projection; false when it lies behind the camera.
bool project(const float viewProjection[16], const float p[3], float width, float height, ImVec2& out)
{
    const float* m = viewProjection;
    const float x = p[0] * m[0] + p[1] * m[4] + p[2] * m[8] + m[12];
    const float y = p[0] * m[1] + p[1] * m[5] + p[2] * m[9] + m[13];
    const float w = p[0] * m[3] + p[1] * m[7] + p[2] * m[11] + m[15];
    if (w < 0.05f)
        return false;
    out.x = (x / w * 0.5f + 0.5f) * width;
    out.y = (0.5f - y / w * 0.5f) * height;
    return true;
}

// The object's local box (collision bounds and children, Object::getFullBoundingBox), or a cube of
// its visual bounding radius when it has none.
void localBox(IObject* object, float minimum[3], float maximum[3])
{
    dice::hfe::LocalBoundingBox* box = object->getFullBoundingBox();
    if (box && (box->m_max.x > box->m_min.x || box->m_max.y > box->m_min.y || box->m_max.z > box->m_min.z))
    {
        minimum[0] = box->m_min.x; minimum[1] = box->m_min.y; minimum[2] = box->m_min.z;
        maximum[0] = box->m_max.x; maximum[1] = box->m_max.y; maximum[2] = box->m_max.z;
        return;
    }
    float r = object->getVisualBoundingRadius();
    if (r <= 0.0f)
        r = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        minimum[i] = -r;
        maximum[i] = r;
    }
}

void drawBox(ImDrawList* list, IObject* object, const float viewProjection[16], float width, float height,
             ImU32 colour, float thickness)
{
    float minimum[3], maximum[3];
    localBox(object, minimum, maximum);
    const Mat4& m = object->getAbsoluteTransformation();
    ImVec2 screen[8];
    bool visible[8];
    for (int corner = 0; corner < 8; ++corner)
    {
        const float local[3] = { (corner & 1) ? maximum[0] : minimum[0], (corner & 2) ? maximum[1] : minimum[1],
                                 (corner & 4) ? maximum[2] : minimum[2] };
        float world[3];
        for (int i = 0; i < 3; ++i)
            world[i] = local[0] * m.m[0 + i] + local[1] * m.m[4 + i] + local[2] * m.m[8 + i] + m.m[12 + i];
        visible[corner] = project(viewProjection, world, width, height, screen[corner]);
    }
    // The twelve edges join corners that differ in one axis bit.
    for (int corner = 0; corner < 8; ++corner)
        for (int bit = 1; bit < 8; bit <<= 1)
        {
            if (corner & bit)
                continue;
            const int other = corner | bit;
            if (visible[corner] && visible[other])
                list->AddLine(screen[corner], screen[other], colour, thickness);
        }
}

} // namespace

// How near the move gizmo's centre a press must be to slide the selection (ImGuizmo draws the centre
// circle 6 pixels wide), and how far a press must travel before it is a box.
const float kCentreRadius = 8.0f;
const float kBoxThreshold = 4.0f;

ViewportTools::ViewportTools()
    : m_gizmoShown(false)
    , m_dragging(false)
    , m_groundDrag(false)
    , m_dragDouble(false)
    , m_camera(0)
    , m_resources(0)
    , m_groundStartValid(false)
    , m_groundOffsetValid(false)
    , m_groundOffset(0.0f)
    , m_dragId(0)
    , m_dragMode(0)
    , m_snap(false)
    , m_moveSnap(1.0f)
    , m_rotateSnap(15.0f)
    , m_pressed(false)
    , m_pressDouble(false)
    , m_boxing(false)
    , m_pressX(0.0f)
    , m_pressY(0.0f)
    , m_areaDrag(false)
    , m_areaPoint(-1)
    , m_areaMoved(false)
    , m_areaTargetValid(false)
    , m_soundDrag(false)
    , m_soundMoved(false)
    , m_soundTargetValid(false)
    , m_soundHeight(0.0f)
    , m_terrainStroke(false)
    , m_terrainButton(0)
    , m_terrainSecondary(false)
    , m_anchorDragging(false)
    , m_undergrowthStroke(false)
    , m_undergrowthButton(0)
    , m_brushStroke(false)
    , m_brushButton(0)
{
    m_soundTarget[0] = m_soundTarget[1] = m_soundTarget[2] = 0.0f;
    m_soundPress[0] = m_soundPress[1] = 0.0f;
    m_areaTarget[0] = m_areaTarget[1] = m_areaTarget[2] = 0.0f;
    m_areaPress[0] = m_areaPress[1] = 0.0f;
}

void ViewportTools::draw(EditSession& session, const FreeCamera& camera, dice::hfe::dx9rend::IRenderView* view,
                         const ImVec2& viewPos, const ImVec2& viewSize)
{
    (void)viewSize;
    if (!view)
        return;
    // This frame's matrices: the world-to-view inverse of what FreeCamera::apply just handed
    // setViewMatrix, and the renderer's own projection (IRenderView +0x6C rebuilds it from the fov,
    // aspect and planes apply() wrote - RendMisc/RenderView.cpp rebuildProjection).
    float worldToView[16];
    camera.worldToView(worldToView);
    const Mat4* projection = view->getProjectionMatrixPtr();
    if (!projection)
        return;
    float viewProjection[16];
    matrixMultiply(worldToView, projection->m, viewProjection);

    session.setGridStep(m_moveSnap);
    m_camera = &camera;
    drawCombatAreas(session, viewProjection);
    drawSplines(session, viewProjection);
    drawCompareLayer(session, viewProjection);
    drawOverlays(session, viewProjection);
    drawPreviewParts(session, viewProjection);
    drawMarkers(session, viewProjection);
    drawSounds(session, viewProjection);
    drawOutlines(session, viewProjection);
    drawAnchor(session, viewProjection);
    drawMeasure(session, viewProjection);
    handleDrop(session, viewProjection);
    if (session.terrainToolActive() || m_terrainStroke)
    {
        // The terrain brush has the mouse: no gizmo, no handles, no picks. A drag under way when the
        // brush came on is finished as it stands; a handle's drag is dropped (it had changed nothing).
        if (m_dragging)
            finishDrag(session);
        m_areaDrag = false;
        m_soundDrag = false;
        m_gizmoShown = false;
        m_pressed = false;
        handleTerrain(session);
        drawTerrainBrush(session, viewProjection);
    }
    else if (session.undergrowthToolActive() || m_undergrowthStroke)
    {
        if (m_dragging)
            finishDrag(session);
        m_areaDrag = false;
        m_soundDrag = false;
        m_gizmoShown = false;
        m_pressed = false;
        handleUndergrowth(session);
        drawUndergrowthCursor(session, viewProjection);
    }
    else if (session.brushToolActive() || m_brushStroke)
    {
        // The object brush has the mouse, as the terrain brush does.
        if (m_dragging)
            finishDrag(session);
        m_areaDrag = false;
        m_soundDrag = false;
        m_gizmoShown = false;
        m_pressed = false;
        handleBrush(session);
        drawBrushCursor(session, viewProjection);
    }
    else if (!session.armedBundle().empty())
    {
        if (m_dragging)
            finishDrag(session);
        m_gizmoShown = false;
        m_pressed = false;
        handleBundlePlacement(session, viewProjection);
    }
    else if (session.measureToolActive())
    {
        // The measure tool has the mouse: a click adds the terrain point under it.
        if (m_dragging)
            finishDrag(session);
        m_gizmoShown = false;
        m_pressed = false;
        handleMeasure(session);
    }
    else if (session.roadToolActive() || m_splineDrag)
    {
        // The road tool has the mouse: no gizmo, no object picks.
        if (m_dragging)
            finishDrag(session);
        m_areaDrag = false;
        m_soundDrag = false;
        m_gizmoShown = false;
        m_pressed = false;
        handleSplines(session, viewProjection);
    }
    // The template preview's parts take a press first while they are shown: a mark picks a part, and a picked part
    // has the gizmo.
    else if (handlePreviewParts(session, worldToView, projection->m, viewProjection))
    {
        if (m_dragging)
            finishDrag(session);
        m_gizmoShown = false;
        m_pressed = false;
    }
    // A combat area's point handle, then an ambient sound's marker, take a press before the gizmo and
    // the pick do.
    else if (handleCombatAreas(session, viewProjection) || handleSounds(session, viewProjection))
    {
        m_gizmoShown = false;
        m_pressed = false;
    }
    else
    {
        handleGizmo(session, worldToView, projection->m, viewProjection);
        handleClicks(session);
    }
    handleShortcuts(session, camera);
    drawToolbar(session, viewPos);
}

void ViewportTools::drawAnchor(EditSession& session, const float viewProjection[16])
{
    const unsigned id = session.selection().primary();
    IObject* o = EditSession::object(id);
    float local[3], world[3];
    if (!o || !EditSession::anchorPosOf(o, local) || (local[0] == 0.0f && local[1] == 0.0f && local[2] == 0.0f)
        || !EditSession::worldAnchorOf(id, world))
        return;
    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 point;
    if (!project(viewProjection, world, io.DisplaySize.x, io.DisplaySize.y, point))
        return;
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const float r = 6.0f;
    list->AddQuadFilled(ImVec2(point.x, point.y - r), ImVec2(point.x + r, point.y), ImVec2(point.x, point.y + r),
                        ImVec2(point.x - r, point.y), IM_COL32(255, 230, 60, 200));
    list->AddQuad(ImVec2(point.x, point.y - r), ImVec2(point.x + r, point.y), ImVec2(point.x, point.y + r),
                  ImVec2(point.x - r, point.y), IM_COL32(40, 30, 0, 255), 1.5f);
}

void ViewportTools::drawOutlines(EditSession& session, const float viewProjection[16])
{
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    // The object under the cursor in yellow.
    if (session.overlays().hoverBox && !pointerOverUi() && !m_dragging)
    {
        std::string detail;
        const unsigned under = session.pick(io.MousePos.x, io.MousePos.y, detail);
        IObject* hovered = EditSession::object(under);
        if (hovered && !session.selection().contains(under))
            drawBox(list, hovered, viewProjection, io.DisplaySize.x, io.DisplaySize.y, IM_COL32(255, 255, 0, 200), 1.0f);
    }
    // The selection's boxes (retail's selection display: none or bounding box).
    if (!session.overlays().selectionBoxes)
        return;
    const std::vector<unsigned>& ids = session.selection().ids();
    for (size_t i = 0; i < ids.size(); ++i)
    {
        IObject* object = EditSession::object(ids[i]);
        if (!object)
            continue;
        const bool primary = ids[i] == session.selection().primary();
        drawBox(list, object, viewProjection, io.DisplaySize.x, io.DisplaySize.y,
                primary ? IM_COL32(255, 170, 40, 255) : IM_COL32(255, 230, 120, 200), primary ? 2.0f : 1.0f);
    }
}

// A symbol per marked object (EditSession::markerKind): colour by kind, the template's name beside it
// when it is near. Drawn in the background list, under the panels and the gizmo.
const float kMarkerReach = EditSession::kMarkerReach;
const float kMarkerFade = 250.0f;

void ViewportTools::drawMarkers(EditSession& session, const float viewProjection[16])
{
    if (!session.markersShown() || !dice::hfe::world::objectManager)
        return;
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const FreeCamera& camera = session.camera();
    const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
    {
        IObject* object = it->second;
        const EditSession::MarkerKind kind = session.markerKind(object);
        if (!session.markerShown(kind) || (object->getFlags() & IObject::OFIsVisible) == 0)
            continue;
        const Mat4& m = object->getAbsoluteTransformation();
        const float p[3] = { m.m[12], m.m[13], m.m[14] };
        const float dx = p[0] - camera.x(), dy = p[1] - camera.y(), dz = p[2] - camera.z();
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        // Markers stop at kMarkerReach, fading out over its last stretch: a whole map's markers at
        // once, far into the fog, would only cover the view. Picking follows the same reach.
        if (distanceSquared > kMarkerReach * kMarkerReach)
            continue;
        ImVec2 at;
        if (!project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, at))
            continue;
        const float distance = std::sqrt(distanceSquared);
        const float fade = distance < kMarkerFade ? 1.0f : 1.0f - (distance - kMarkerFade) / (kMarkerReach - kMarkerFade);
        const int alpha = static_cast<int>(235.0f * fade);
        ImU32 colour = IM_COL32(255, 255, 255, alpha);
        switch (kind)
        {
        case EditSession::MarkerSpawner:      colour = IM_COL32(255, 150, 40, alpha); break;
        case EditSession::MarkerSpawnPoint:   colour = IM_COL32(90, 220, 90, alpha); break;
        case EditSession::MarkerControlPoint: colour = IM_COL32(80, 150, 255, alpha); break;
        case EditSession::MarkerTrigger:      colour = IM_COL32(200, 110, 255, alpha); break;
        case EditSession::MarkerEffectArea:   colour = IM_COL32(60, 220, 220, alpha); break;
        default: break;
        }
        const bool selected = session.selection().contains(it->first);
        const float size = selected ? 7.0f : 5.0f;
        if (kind == EditSession::MarkerControlPoint)
        {
            // A flag: pole and pennant.
            list->AddLine(ImVec2(at.x, at.y), ImVec2(at.x, at.y - 16.0f), colour, 2.0f);
            list->AddTriangleFilled(ImVec2(at.x, at.y - 16.0f), ImVec2(at.x + 10.0f, at.y - 12.0f),
                                    ImVec2(at.x, at.y - 8.0f), colour);
        }
        else if (kind == EditSession::MarkerSpawnPoint)
        {
            list->AddTriangleFilled(ImVec2(at.x, at.y - size - 3.0f), ImVec2(at.x - size, at.y + size - 1.0f),
                                    ImVec2(at.x + size, at.y + size - 1.0f), colour);
        }
        else if (kind == EditSession::MarkerSpawner)
        {
            list->AddRectFilled(ImVec2(at.x - size, at.y - size * 0.6f), ImVec2(at.x + size, at.y + size * 0.6f), colour);
        }
        else
        {
            list->AddCircle(at, size + 2.0f, colour, 16, 2.0f);
        }
        if (selected)
            list->AddCircle(at, size + 6.0f, IM_COL32(255, 170, 40, 255), 20, 2.0f);
        // Names only near: far off they would cover the view.
        if (distanceSquared < 90.0f * 90.0f && object->getTemplate())
            list->AddText(ImVec2(at.x + 9.0f, at.y - 7.0f), colour, object->getTemplate()->getName().c_str());
    }
}

// Combat areas (phase C)

namespace
{

// An area's colour: by team (0 any, 1, 2), an exclusion in magenta.
ImU32 areaColour(const LevelObjectFile::CombatAreaData& area, int alpha)
{
    if (area.inverted)
        return IM_COL32(230, 90, 230, alpha);
    switch (area.team)
    {
    case 1:  return IM_COL32(255, 95, 75, alpha);
    case 2:  return IM_COL32(85, 150, 255, alpha);
    default: return IM_COL32(255, 215, 70, alpha);
    }
}

// Drawn this near, fading over the last stretch: a boundary runs round the whole map.
const float kAreaReach = 900.0f;
const float kAreaFade = 300.0f;

} // namespace

bool ViewportTools::areaPointOnScreen(EditSession& session, const std::pair<float, float>& point,
                                      const float viewProjection[16], ImVec2& out) const
{
    const ImGuiIO& io = ImGui::GetIO();
    float ground = 0.0f;
    if (!session.groundHeight(point.first, point.second, ground))
        ground = session.camera().y() - 20.0f;
    const float p[3] = { point.first, ground + 1.0f, point.second };
    return project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, out);
}

void ViewportTools::drawCombatAreas(EditSession& session, const float viewProjection[16])
{
    if (!session.combatAreasShown())
        return;
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const FreeCamera& camera = session.camera();
    const std::vector<LevelObjectFile::CombatAreaData> areas = EditSession::combatAreas();
    for (size_t a = 0; a < areas.size(); ++a)
    {
        LevelObjectFile::CombatAreaData area = areas[a];
        const bool active = _stricmp(area.name.c_str(), session.activeCombatArea().c_str()) == 0;
        // The point being dragged, where it would go.
        if (active && m_areaDrag && m_areaMoved && m_areaTargetValid && m_areaPoint < static_cast<int>(area.points.size()))
            area.points[m_areaPoint] = std::make_pair(m_areaTarget[0], m_areaTarget[2]);
        const size_t n = area.points.size();
        if (n < 2)
            continue;
        // Each edge follows the terrain: a sample every 8 m, a metre above the ground.
        for (size_t i = 0; i < n; ++i)
        {
            const std::pair<float, float>& from = area.points[i];
            const std::pair<float, float>& to = area.points[(i + 1) % n];
            const float ex = to.first - from.first, ez = to.second - from.second;
            const float length = std::sqrt(ex * ex + ez * ez);
            int steps = static_cast<int>(length / 8.0f) + 1;
            if (steps > 64)
                steps = 64;
            ImVec2 previous;
            bool previousVisible = false;
            for (int s = 0; s <= steps; ++s)
            {
                const float t = static_cast<float>(s) / static_cast<float>(steps);
                const float x = from.first + ex * t, z = from.second + ez * t;
                float ground = 0.0f;
                if (!session.groundHeight(x, z, ground))
                    ground = camera.y() - 20.0f;
                const float p[3] = { x, ground + 1.0f, z };
                const float dx = p[0] - camera.x(), dy = p[1] - camera.y(), dz = p[2] - camera.z();
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                ImVec2 at;
                const bool visible = distance < kAreaReach && project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, at);
                if (visible && previousVisible)
                {
                    const float fade = distance < kAreaFade ? 1.0f : 1.0f - (distance - kAreaFade) / (kAreaReach - kAreaFade);
                    list->AddLine(previous, at, areaColour(area, static_cast<int>((active ? 255.0f : 190.0f) * fade)),
                                  active ? 3.0f : 2.0f);
                }
                previous = at;
                previousVisible = visible;
            }
        }
        if (!active)
            continue;
        // The selected area's points: handles to drag, the selected one ringed.
        for (size_t i = 0; i < n; ++i)
        {
            ImVec2 at;
            if (!areaPointOnScreen(session, area.points[i], viewProjection, at))
                continue;
            list->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 4.0f), ImVec2(at.x + 4.0f, at.y + 4.0f), areaColour(area, 255));
            list->AddRect(ImVec2(at.x - 4.0f, at.y - 4.0f), ImVec2(at.x + 4.0f, at.y + 4.0f), IM_COL32(20, 20, 20, 255));
            if (static_cast<int>(i) == session.activeCombatPoint() || (m_areaDrag && static_cast<int>(i) == m_areaPoint))
                list->AddCircle(at, 9.0f, IM_COL32(255, 170, 40, 255), 16, 2.0f);
        }
    }
}

bool ViewportTools::handleCombatAreas(EditSession& session, const float viewProjection[16])
{
    const ImGuiIO& io = ImGui::GetIO();
    const std::string name = session.activeCombatArea();
    LevelObjectFile::CombatAreaData area;
    if (name.empty() || !session.combatAreasShown() || !EditSession::engineCombatArea(name, area))
    {
        m_areaDrag = false;
        return false;
    }
    char line[256];
    if (m_areaDrag)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const float dx = io.MousePos.x - m_areaPress[0], dy = io.MousePos.y - m_areaPress[1];
            if (dx * dx + dy * dy > kBoxThreshold * kBoxThreshold)
                m_areaMoved = true;
            if (m_areaMoved)
            {
                std::string detail;
                m_areaTargetValid = session.surfaceAt(io.MousePos.x, io.MousePos.y, m_areaTarget, detail);
            }
            return true;
        }
        // Released: moved, one undoable edit; in place, the point is selected.
        m_areaDrag = false;
        if (m_areaMoved && m_areaTargetValid)
            std::snprintf(line, sizeof(line), "EditorCombatArea.movePoint %s %d %.3f %.3f", area.name.c_str(), m_areaPoint,
                          m_areaTarget[0], m_areaTarget[2]);
        else
            std::snprintf(line, sizeof(line), "EditorCombatArea.select %s %d", area.name.c_str(), m_areaPoint);
        session.runCommand(line, "combat area");
        return true;
    }
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || pointerOverUi() || ImGui::GetDragDropPayload())
        return false;
    // A press on one of the selected area's points drags it.
    int nearest = -1;
    float best = 8.0f * 8.0f;
    for (size_t i = 0; i < area.points.size(); ++i)
    {
        ImVec2 at;
        if (!areaPointOnScreen(session, area.points[i], viewProjection, at))
            continue;
        const float dx = at.x - io.MousePos.x, dy = at.y - io.MousePos.y;
        if (dx * dx + dy * dy < best)
        {
            best = dx * dx + dy * dy;
            nearest = static_cast<int>(i);
        }
    }
    if (nearest >= 0)
    {
        m_areaDrag = true;
        m_areaPoint = nearest;
        m_areaMoved = false;
        m_areaTargetValid = false;
        m_areaPress[0] = io.MousePos.x;
        m_areaPress[1] = io.MousePos.y;
        // The point is what Delete removes now, not the selected objects.
        session.selection().clear();
        return true;
    }
    // Alt+click on one of its edges puts a point there.
    if (!io.KeyAlt)
        return false;
    const size_t n = area.points.size();
    int edge = -1;
    best = 6.0f * 6.0f;
    for (size_t i = 0; i < n && n >= 2; ++i)
    {
        ImVec2 a, b;
        if (!areaPointOnScreen(session, area.points[i], viewProjection, a)
            || !areaPointOnScreen(session, area.points[(i + 1) % n], viewProjection, b))
            continue;
        const float vx = b.x - a.x, vy = b.y - a.y;
        const float lengthSquared = vx * vx + vy * vy;
        float t = lengthSquared > 0.0f ? ((io.MousePos.x - a.x) * vx + (io.MousePos.y - a.y) * vy) / lengthSquared : 0.0f;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const float px = a.x + vx * t - io.MousePos.x, py = a.y + vy * t - io.MousePos.y;
        if (px * px + py * py < best)
        {
            best = px * px + py * py;
            edge = static_cast<int>(i);
        }
    }
    float at[3];
    std::string detail;
    if (edge < 0 || !session.surfaceAt(io.MousePos.x, io.MousePos.y, at, detail))
        return false;
    std::snprintf(line, sizeof(line), "EditorCombatArea.insertPoint %s %d %.3f %.3f", area.name.c_str(), edge + 1, at[0], at[2]);
    session.runCommand(line, "combat area");
    return true;
}

// Splines and the road tool (App/RoadNetwork)

void ViewportTools::drawSplines(EditSession& session, const float viewProjection[16])
{
    RoadNetwork& roads = session.roads();
    if (!session.splinesShown() || !roads.isOpen())
        return;
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const int active = session.activeSpline();
    const int activePoint = session.activeSplinePoint();
    const std::vector<int> ids = roads.splineIds();
    std::vector<std::vector<float> > samples;
    for (size_t s = 0; s < ids.size(); ++s)
    {
        SplineRecord spline;
        if (!roads.splineRecord(ids[s], spline) || spline.points.empty())
            continue;
        const size_t n = spline.points.size();
        if (m_splineDrag && m_splineMoved && m_splineTargetValid && ids[s] == m_splineDragId && m_splineDragPoint >= 0
            && static_cast<size_t>(m_splineDragPoint) < n)
            for (int k = 0; k < 3; ++k)
                spline.points[static_cast<size_t>(m_splineDragPoint)].position[k] = m_splineTarget[k];
        const bool isActive = ids[s] == active;
        const std::string road = roads.roadOnSpline(ids[s]);
        const ImU32 colour = isActive ? IM_COL32(255, 200, 40, 255)
                                      : !road.empty() ? IM_COL32(120, 200, 255, 220) : IM_COL32(210, 210, 210, 200);
        for (size_t i = 0; i + 1 < n; ++i)
        {
            splineSegment(spline, i, 8, samples);
            ImVec2 previous;
            bool havePrevious = false;
            for (size_t k = 0; k < samples.size(); ++k)
            {
                ImVec2 at;
                const bool on = project(viewProjection, &samples[k][0], io.DisplaySize.x, io.DisplaySize.y, at);
                if (on && havePrevious)
                    list->AddLine(previous, at, colour, isActive ? 2.5f : 1.5f);
                previous = at;
                havePrevious = on;
            }
        }
        for (size_t i = 0; i < n; ++i)
        {
            ImVec2 at;
            if (!project(viewProjection, spline.points[i].position, io.DisplaySize.x, io.DisplaySize.y, at))
                continue;
            const bool selected = isActive && static_cast<int>(i) == activePoint;
            const float radius = selected ? 6.0f : isActive ? 4.5f : 3.0f;
            list->AddCircleFilled(at, radius, selected ? IM_COL32(60, 255, 60, 255) : colour);
            list->AddCircle(at, radius, IM_COL32(0, 0, 0, 200));
        }
        ImVec2 at;
        if (project(viewProjection, spline.points[0].position, io.DisplaySize.x, io.DisplaySize.y, at))
        {
            char text[160];
            std::snprintf(text, sizeof(text), "spline %d%s%s", ids[s], road.empty() ? "" : ": ", road.c_str());
            list->AddText(ImVec2(at.x + 7.0f, at.y - 18.0f), colour, text);
        }
    }
}

bool ViewportTools::handleSplines(EditSession& session, const float viewProjection[16])
{
    RoadNetwork& roads = session.roads();
    const ImGuiIO& io = ImGui::GetIO();
    if (!roads.isOpen())
    {
        m_splineDrag = false;
        return false;
    }
    char line[256];
    if (m_splineDrag)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const float dx = io.MousePos.x - m_splinePress[0], dy = io.MousePos.y - m_splinePress[1];
            if (dx * dx + dy * dy > kBoxThreshold * kBoxThreshold)
                m_splineMoved = true;
            if (m_splineMoved)
                m_splineTargetValid = session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, m_splineTarget);
            return true;
        }
        // Released: moved, one undoable edit; in place, the point is selected.
        m_splineDrag = false;
        session.setActiveSpline(m_splineDragId, m_splineDragPoint);
        if (m_splineMoved && m_splineTargetValid)
        {
            std::snprintf(line, sizeof(line), "EditorRoad.splineMove %d %d %.3f/%.3f/%.3f", m_splineDragId,
                          m_splineDragPoint, m_splineTarget[0], m_splineTarget[1], m_splineTarget[2]);
            session.runCommand(line, "road tool");
        }
        return true;
    }
    if (pointerOverUi() || ImGui::GetDragDropPayload())
        return false;
    const int active = session.activeSpline();
    // Delete: the selected point goes.
    if (active >= 0 && session.activeSplinePoint() >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        std::snprintf(line, sizeof(line), "EditorRoad.splineRemovePoint %d %d", active, session.activeSplinePoint());
        session.runCommand(line, "road tool");
        session.setActiveSpline(active, -1);
        return true;
    }
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return false;
    // A press on any spline's point: Ctrl removes it, otherwise it is dragged (and its spline becomes the active one).
    int bestSpline = -1, bestPoint = -1;
    float best = 8.0f * 8.0f;
    const std::vector<int> ids = roads.splineIds();
    for (size_t s = 0; s < ids.size(); ++s)
    {
        SplineRecord spline;
        if (!roads.splineRecord(ids[s], spline))
            continue;
        for (size_t i = 0; i < spline.points.size(); ++i)
        {
            ImVec2 at;
            if (!project(viewProjection, spline.points[i].position, io.DisplaySize.x, io.DisplaySize.y, at))
                continue;
            const float dx = at.x - io.MousePos.x, dy = at.y - io.MousePos.y;
            if (dx * dx + dy * dy < best || (dx * dx + dy * dy == best && ids[s] == active))
            {
                best = dx * dx + dy * dy;
                bestSpline = ids[s];
                bestPoint = static_cast<int>(i);
            }
        }
    }
    if (bestSpline >= 0)
    {
        session.selection().clear();
        if (io.KeyCtrl)
        {
            std::snprintf(line, sizeof(line), "EditorRoad.splineRemovePoint %d %d", bestSpline, bestPoint);
            session.runCommand(line, "road tool");
            session.setActiveSpline(bestSpline, -1);
            return true;
        }
        m_splineDrag = true;
        m_splineMoved = false;
        m_splineTargetValid = false;
        m_splineDragId = bestSpline;
        m_splineDragPoint = bestPoint;
        m_splinePress[0] = io.MousePos.x;
        m_splinePress[1] = io.MousePos.y;
        session.setActiveSpline(bestSpline, bestPoint);
        return true;
    }
    float at[3];
    if (!session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, at))
        return false;
    // Alt+click near the active spline's curve: a point inserted in that segment.
    SplineRecord current;
    if (io.KeyAlt && active >= 0 && roads.splineRecord(active, current) && current.points.size() >= 2)
    {
        int segment = -1;
        best = 10.0f * 10.0f;
        std::vector<std::vector<float> > samples;
        for (size_t i = 0; i + 1 < current.points.size(); ++i)
        {
            splineSegment(current, i, 8, samples);
            for (size_t k = 0; k + 1 < samples.size(); ++k)
            {
                ImVec2 a, b;
                if (!project(viewProjection, &samples[k][0], io.DisplaySize.x, io.DisplaySize.y, a)
                    || !project(viewProjection, &samples[k + 1][0], io.DisplaySize.x, io.DisplaySize.y, b))
                    continue;
                const float vx = b.x - a.x, vy = b.y - a.y;
                const float lengthSquared = vx * vx + vy * vy;
                float t = lengthSquared > 0.0f ? ((io.MousePos.x - a.x) * vx + (io.MousePos.y - a.y) * vy) / lengthSquared
                                               : 0.0f;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                const float px = a.x + vx * t - io.MousePos.x, py = a.y + vy * t - io.MousePos.y;
                if (px * px + py * py < best)
                {
                    best = px * px + py * py;
                    segment = static_cast<int>(i);
                }
            }
        }
        if (segment >= 0)
        {
            std::snprintf(line, sizeof(line), "EditorRoad.splineInsert %d %d %.3f/%.3f/%.3f", active, segment + 1, at[0],
                          at[1], at[2]);
            session.runCommand(line, "road tool");
            session.setActiveSpline(active, segment + 1);
            return true;
        }
    }
    // A click on the ground: a point at the end of the active spline, a new spline when there is none.
    int target = active;
    if (target < 0)
    {
        target = roads.nextSplineId();
        std::snprintf(line, sizeof(line), "EditorRoad.splineCreateAs %d", target);
        const ConsoleLineResult made = runConsoleLine(line);
        if (made.failed || !roads.hasSpline(target))
        {
            logLine(LogLevel::Warning, "road tool: %s", made.output.c_str());
            return true;
        }
    }
    SplineRecord before;
    roads.splineRecord(target, before);
    std::snprintf(line, sizeof(line), "EditorRoad.splineAdd %d %.3f/%.3f/%.3f", target, at[0], at[1], at[2]);
    session.runCommand(line, "road tool");
    session.setActiveSpline(target, static_cast<int>(before.points.size()));
    return true;
}

// Ambient sounds (phase C)

void ViewportTools::drawSounds(EditSession& session, const float viewProjection[16])
{
    const EditSession::OverlayOptions& overlays = session.overlays();
    if (!session.markersShown() || (overlays.soundMarkers == 0 && overlays.soundReach != 2))
        return;
    const float scale = overlays.soundMarkers == 3 ? 2.0f : overlays.soundMarkers == 2 ? 1.5f : 1.0f;
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const FreeCamera& camera = session.camera();
    const std::vector<EditSession::SoundRow> rows = session.sounds();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        EditSession::SoundRow row = rows[i];
        if (!row.positioned)
            continue;
        const bool active = _stricmp(row.name.c_str(), session.activeSound().c_str()) == 0;
        if (active && m_soundDrag && m_soundMoved && m_soundTargetValid)
            for (int k = 0; k < 3; ++k)
                row.position[k] = m_soundTarget[k];
        const float dx = row.position[0] - camera.x(), dy = row.position[1] - camera.y(), dz = row.position[2] - camera.z();
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared > kMarkerReach * kMarkerReach && !active)
            continue;
        ImVec2 at;
        if (!project(viewProjection, row.position, io.DisplaySize.x, io.DisplaySize.y, at))
            continue;
        const float distance = std::sqrt(distanceSquared);
        const float fade = distance < kMarkerFade ? 1.0f : 1.0f - (distance - kMarkerFade) / (kMarkerReach - kMarkerFade);
        const int alpha = static_cast<int>(235.0f * (active ? 1.0f : (fade > 0.0f ? fade : 0.0f)));
        const ImU32 colour = IM_COL32(250, 140, 200, alpha);
        if (overlays.soundMarkers > 0)
        {
            // A speaker: a box and a cone, at the overlay's size.
            const float s = scale;
            list->AddRectFilled(ImVec2(at.x - 5.0f * s, at.y - 3.0f * s), ImVec2(at.x - 1.0f * s, at.y + 3.0f * s), colour);
            list->AddTriangleFilled(ImVec2(at.x - 1.0f * s, at.y), ImVec2(at.x + 5.0f * s, at.y - 6.0f * s),
                                    ImVec2(at.x + 5.0f * s, at.y + 6.0f * s), colour);
            if (distanceSquared < 90.0f * 90.0f || active)
                list->AddText(ImVec2(at.x + 9.0f * s, at.y - 7.0f), colour, row.name.c_str());
            if (active)
                list->AddCircle(at, 11.0f * s, IM_COL32(255, 170, 40, 255), 20, 2.0f);
        }
        // Its reach (minDistance): the selected sound's, or every sound's, as the overlay says.
        const bool reach = overlays.soundReach == 2 || (overlays.soundReach == 1 && active);
        if (!reach)
            continue;
        if (overlays.soundReachOnGround && row.radius > 0.0f && row.radius < 5000.0f)
        {
            std::vector<std::pair<float, float> > loop;
            for (int s = 0; s < 48; ++s)
            {
                const float angle = 6.2831853f * static_cast<float>(s) / 48.0f;
                loop.push_back(std::make_pair(row.position[0] + std::cos(angle) * row.radius,
                                              row.position[2] + std::sin(angle) * row.radius));
            }
            drawGroundLoop(session, loop, viewProjection, IM_COL32(250, 140, 200, 255), 170, 1.5f, false);
            continue;
        }
        if (row.radius > 0.0f && row.radius < 5000.0f)
        {
            ImVec2 previous;
            bool previousVisible = false;
            for (int s = 0; s <= 48; ++s)
            {
                const float angle = 6.2831853f * static_cast<float>(s) / 48.0f;
                const float p[3] = { row.position[0] + std::cos(angle) * row.radius, row.position[1],
                                     row.position[2] + std::sin(angle) * row.radius };
                ImVec2 point;
                const bool visible = project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, point);
                if (visible && previousVisible)
                    list->AddLine(previous, point, IM_COL32(250, 140, 200, 160), 1.5f);
                previous = point;
                previousVisible = visible;
            }
            // The sphere's two upright circles, fainter.
            for (int plane = 0; plane < 2; ++plane)
            {
                previousVisible = false;
                for (int s = 0; s <= 48; ++s)
                {
                    const float angle = 6.2831853f * static_cast<float>(s) / 48.0f;
                    const float across = std::cos(angle) * row.radius, up = std::sin(angle) * row.radius;
                    const float p[3] = { row.position[0] + (plane == 0 ? across : 0.0f), row.position[1] + up,
                                         row.position[2] + (plane == 1 ? across : 0.0f) };
                    ImVec2 point;
                    const bool visible = project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, point);
                    if (visible && previousVisible)
                        list->AddLine(previous, point, IM_COL32(250, 140, 200, 90), 1.0f);
                    previous = point;
                    previousVisible = visible;
                }
            }
        }
    }
}

void ViewportTools::handleMeasure(EditSession& session)
{
    const ImGuiIO& io = ImGui::GetIO();
    std::string result;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !pointerOverUi())
    {
        float at[3];
        if (session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, at))
            session.measureAdd(at[0], at[2], result);
    }
    // Backspace takes the last point off, Enter gives the length and clears (the retail handler's keys).
    if (!io.WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
            session.measureUndo(result);
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
            session.measureFinish(result);
    }
}

void ViewportTools::drawMeasure(EditSession& session, const float viewProjection[16])
{
    const std::vector<dice::hfe::Vec3>& points = session.measurePoints();
    if (points.empty())
        return;
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const ImU32 yellow = IM_COL32(255, 255, 0, 230);
    ImVec2 previous;
    bool previousVisible = false;
    for (size_t i = 0; i < points.size(); ++i)
    {
        const float p[3] = { points[i].x, points[i].y + 0.2f, points[i].z };
        ImVec2 at;
        const bool visible = project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, at);
        if (visible)
            list->AddCircleFilled(at, 3.5f, yellow, 10);
        if (visible && previousVisible)
            list->AddLine(previous, at, yellow, 2.0f);
        previous = at;
        previousVisible = visible;
    }
    if (previousVisible)
    {
        char text[48];
        std::snprintf(text, sizeof(text), "%.2f m", session.measureLength());
        list->AddText(ImVec2(previous.x + 8.0f, previous.y - 16.0f), yellow, text);
    }
}

void ViewportTools::drawOverlays(EditSession& session, const float viewProjection[16])
{
    const EditSession::OverlayOptions& overlays = session.overlays();
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const FreeCamera& camera = session.camera();
    // The template preview's aids: a 1 m grid under it, its bounds and its centre.
    IObject* preview = session.templatePreview() ? EditSession::object(session.templatePreview()) : 0;
    if (preview)
    {
        const Mat4& m = preview->getAbsoluteTransformation();
        for (int axis = 0; axis < 2; ++axis)
            for (int line = -10; line <= 10; ++line)
            {
                float a[3], b[3];
                a[1] = b[1] = m.m[13] + 0.02f;
                if (axis == 0)
                {
                    a[0] = b[0] = m.m[12] + static_cast<float>(line);
                    a[2] = m.m[14] - 10.0f;
                    b[2] = m.m[14] + 10.0f;
                }
                else
                {
                    a[2] = b[2] = m.m[14] + static_cast<float>(line);
                    a[0] = m.m[12] - 10.0f;
                    b[0] = m.m[12] + 10.0f;
                }
                ImVec2 sa, sb;
                if (project(viewProjection, a, io.DisplaySize.x, io.DisplaySize.y, sa)
                    && project(viewProjection, b, io.DisplaySize.x, io.DisplaySize.y, sb))
                    list->AddLine(sa, sb, line == 0 ? IM_COL32(200, 200, 200, 160) : IM_COL32(150, 150, 150, 70), 1.0f);
            }
        drawBox(list, preview, viewProjection, io.DisplaySize.x, io.DisplaySize.y, IM_COL32(90, 220, 255, 200), 1.0f);
        float minimum[3], maximum[3];
        localBox(preview, minimum, maximum);
        const float local[3] = { (minimum[0] + maximum[0]) * 0.5f, (minimum[1] + maximum[1]) * 0.5f, (minimum[2] + maximum[2]) * 0.5f };
        float centre[3];
        for (int k = 0; k < 3; ++k)
            centre[k] = local[0] * m.m[0 + k] + local[1] * m.m[4 + k] + local[2] * m.m[8 + k] + m.m[12 + k];
        ImVec2 c;
        if (project(viewProjection, centre, io.DisplaySize.x, io.DisplaySize.y, c))
        {
            list->AddLine(ImVec2(c.x - 6.0f, c.y), ImVec2(c.x + 6.0f, c.y), IM_COL32(255, 80, 80, 230), 2.0f);
            list->AddLine(ImVec2(c.x, c.y - 6.0f), ImVec2(c.x, c.y + 6.0f), IM_COL32(255, 80, 80, 230), 2.0f);
        }
    }
    if (session.markersShown() && (overlays.connections || overlays.controlPointRadius || overlays.supplyLines))
    {
        std::vector<EditSession::OverlayLink> links;
        std::vector<EditSession::OverlayCircle> circles;
        session.overlayGeometry(links, circles);
        if (overlays.connections)
        {
            // A line from each spawn point and spawner to its control point, near enough to read.
            for (size_t i = 0; i < links.size(); ++i)
            {
                if (links[i].kind != 0)
                    continue;
                IObject* from = EditSession::object(links[i].from);
                IObject* to = EditSession::object(links[i].to);
                if (!from || !to)
                    continue;
                const Mat4& a = from->getAbsoluteTransformation();
                const Mat4& b = to->getAbsoluteTransformation();
                const float p[3] = { a.m[12], a.m[13], a.m[14] };
                const float q[3] = { b.m[12], b.m[13], b.m[14] };
                const float dx = p[0] - camera.x(), dz = p[2] - camera.z();
                if (dx * dx + dz * dz > kMarkerReach * kMarkerReach)
                    continue;
                ImVec2 sp, sq;
                if (project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, sp)
                    && project(viewProjection, q, io.DisplaySize.x, io.DisplaySize.y, sq))
                    list->AddLine(sp, sq, IM_COL32(255, 210, 90, 150), 1.0f);
            }
        }
        if (overlays.supplyLines)
        {
            // Supply groups at map scale (no reach limit): members of a group joined in white, each control point's
            // line to a group team 1 needs in red and team 2 in blue, team 2's raised so both show.
            for (size_t i = 0; i < links.size(); ++i)
            {
                if (links[i].kind == 0)
                    continue;
                IObject* from = EditSession::object(links[i].from);
                IObject* to = EditSession::object(links[i].to);
                if (!from || !to)
                    continue;
                const Mat4& a = from->getAbsoluteTransformation();
                const Mat4& b = to->getAbsoluteTransformation();
                const float lift = links[i].kind == 3 ? 4.0f : 2.0f;
                const float p[3] = { a.m[12], a.m[13] + lift, a.m[14] };
                const float q[3] = { b.m[12], b.m[13] + lift, b.m[14] };
                const ImU32 colour = links[i].kind == 1 ? IM_COL32(235, 235, 235, 200)
                                   : links[i].kind == 2 ? IM_COL32(230, 70, 60, 230) : IM_COL32(70, 130, 240, 230);
                ImVec2 sp, sq;
                if (project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, sp)
                    && project(viewProjection, q, io.DisplaySize.x, io.DisplaySize.y, sq))
                    list->AddLine(sp, sq, colour, links[i].kind == 1 ? 1.5f : 2.0f);
            }
        }
        if (overlays.controlPointRadius)
        {
            // The radius a soldier must be within to take the point, along the ground.
            for (size_t i = 0; i < circles.size(); ++i)
            {
                IObject* point = EditSession::object(circles[i].id);
                if (!point || circles[i].radius > 2000.0f)
                    continue;
                const Mat4& m = point->getAbsoluteTransformation();
                std::vector<std::pair<float, float> > loop;
                for (int s = 0; s < 48; ++s)
                {
                    const float angle = 6.2831853f * static_cast<float>(s) / 48.0f;
                    loop.push_back(std::make_pair(m.m[12] + std::cos(angle) * circles[i].radius,
                                                  m.m[14] + std::sin(angle) * circles[i].radius));
                }
                drawGroundLoop(session, loop, viewProjection, IM_COL32(80, 150, 255, 255), 200, 2.0f, false);
            }
        }
    }
    float seaLevel = 0.0f;
    if (overlays.seaLevel && session.terrain().seaLevel(seaLevel))
    {
        // A grid at the sea level around the camera, every 25 m, fading towards its edge.
        const float step = 25.0f, half = 300.0f;
        const float cx = std::floor(camera.x() / step) * step, cz = std::floor(camera.z() / step) * step;
        for (int axis = 0; axis < 2; ++axis)
            for (float line = -half; line <= half; line += step)
            {
                ImVec2 previous;
                bool previousVisible = false;
                for (float t = -half; t <= half; t += 10.0f)
                {
                    const float x = axis == 0 ? cx + line : cx + t;
                    const float z = axis == 0 ? cz + t : cz + line;
                    const float p[3] = { x, seaLevel, z };
                    const float dx = x - camera.x(), dz = z - camera.z();
                    const float distance = std::sqrt(dx * dx + dz * dz);
                    ImVec2 at;
                    const bool visible = distance < half && project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, at);
                    if (visible && previousVisible)
                        list->AddLine(previous, at, IM_COL32(70, 160, 255, static_cast<int>(150.0f * (1.0f - distance / half))), 1.0f);
                    previous = at;
                    previousVisible = visible;
                }
            }
    }
    if (overlays.boundingSpheres)
    {
        // Each selected object's bounding sphere (its box's centre and half diagonal), as the circle it makes.
        float right[3];
        {
            float matrix[16];
            camera.cameraToWorld(matrix);
            right[0] = matrix[0];
            right[1] = matrix[1];
            right[2] = matrix[2];
        }
        const std::vector<unsigned>& ids = session.selection().ids();
        for (size_t i = 0; i < ids.size(); ++i)
        {
            IObject* object = EditSession::object(ids[i]);
            if (!object)
                continue;
            float minimum[3], maximum[3];
            localBox(object, minimum, maximum);
            const Mat4& m = object->getAbsoluteTransformation();
            const float local[3] = { (minimum[0] + maximum[0]) * 0.5f, (minimum[1] + maximum[1]) * 0.5f,
                                     (minimum[2] + maximum[2]) * 0.5f };
            float centre[3];
            for (int k = 0; k < 3; ++k)
                centre[k] = local[0] * m.m[0 + k] + local[1] * m.m[4 + k] + local[2] * m.m[8 + k] + m.m[12 + k];
            const float ex = maximum[0] - minimum[0], ey = maximum[1] - minimum[1], ez = maximum[2] - minimum[2];
            const float radius = 0.5f * std::sqrt(ex * ex + ey * ey + ez * ez);
            const float edge[3] = { centre[0] + right[0] * radius, centre[1] + right[1] * radius, centre[2] + right[2] * radius };
            ImVec2 c, e;
            if (!project(viewProjection, centre, io.DisplaySize.x, io.DisplaySize.y, c)
                || !project(viewProjection, edge, io.DisplaySize.x, io.DisplaySize.y, e))
                continue;
            const float screenRadius = std::sqrt((e.x - c.x) * (e.x - c.x) + (e.y - c.y) * (e.y - c.y));
            list->AddCircle(c, screenRadius, IM_COL32(120, 255, 160, 170), 48, 1.5f);
        }
    }
    if (overlays.lightSources && dice::hfe::world::objectManager)
    {
        // Light source objects (their template's class LightSource): a sun mark, the name when near.
        const std::map<unsigned int, IObject*>& roots = dice::hfe::world::objectManager->getAllObjectsFromGrid();
        for (std::map<unsigned int, IObject*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        {
            IObject* object = it->second;
            const dice::hfe::world::IObjectTemplate* t = object ? object->getTemplate() : 0;
            if (!t || t->getClassID() != dice::hfe::world::CID_LightSourceTemplate)
                continue;
            const Mat4& m = object->getAbsoluteTransformation();
            const float p[3] = { m.m[12], m.m[13], m.m[14] };
            const float dx = p[0] - camera.x(), dy = p[1] - camera.y(), dz = p[2] - camera.z();
            const float distanceSquared = dx * dx + dy * dy + dz * dz;
            if (distanceSquared > kMarkerReach * kMarkerReach)
                continue;
            ImVec2 at;
            if (!project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, at))
                continue;
            const ImU32 colour = IM_COL32(255, 240, 120, 230);
            list->AddCircleFilled(at, 4.0f, colour, 12);
            for (int ray = 0; ray < 8; ++ray)
            {
                const float angle = 0.7853982f * static_cast<float>(ray);
                list->AddLine(ImVec2(at.x + std::cos(angle) * 6.0f, at.y + std::sin(angle) * 6.0f),
                              ImVec2(at.x + std::cos(angle) * 10.0f, at.y + std::sin(angle) * 10.0f), colour, 1.5f);
            }
            if (distanceSquared < 90.0f * 90.0f)
                list->AddText(ImVec2(at.x + 12.0f, at.y - 7.0f), colour, t->getName().c_str());
        }
    }
}

bool ViewportTools::handleSounds(EditSession& session, const float viewProjection[16])
{
    const ImGuiIO& io = ImGui::GetIO();
    char line[256];
    if (m_soundDrag)
    {
        EditSession::SoundRow row;
        if (!session.soundRow(m_soundName, row))
        {
            m_soundDrag = false;
            return false;
        }
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const float dx = io.MousePos.x - m_soundPress[0], dy = io.MousePos.y - m_soundPress[1];
            if (dx * dx + dy * dy > kBoxThreshold * kBoxThreshold)
                m_soundMoved = true;
            if (m_soundMoved)
            {
                // Over the surface under the cursor, as high above it as the sound was.
                std::string detail;
                float at[3];
                if (session.surfaceAt(io.MousePos.x, io.MousePos.y, at, detail))
                {
                    m_soundTarget[0] = at[0];
                    m_soundTarget[1] = at[1] + m_soundHeight;
                    m_soundTarget[2] = at[2];
                    m_soundTargetValid = true;
                }
            }
            return true;
        }
        m_soundDrag = false;
        if (m_soundMoved && m_soundTargetValid)
        {
            std::snprintf(line, sizeof(line), "EditorSound.move %s %.3f/%.3f/%.3f", row.name.c_str(), m_soundTarget[0],
                          m_soundTarget[1], m_soundTarget[2]);
            session.runCommand(line, "sound");
        }
        return true;
    }
    if (!session.markersShown() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left) || pointerOverUi()
        || ImGui::GetDragDropPayload())
        return false;
    // A press on a sound's marker selects the sound and starts to drag it.
    const std::vector<EditSession::SoundRow> rows = session.sounds();
    const FreeCamera& camera = session.camera();
    int nearest = -1;
    float best = 9.0f * 9.0f;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (!rows[i].positioned)
            continue;
        const float dx = rows[i].position[0] - camera.x(), dy = rows[i].position[1] - camera.y(),
                    dz = rows[i].position[2] - camera.z();
        if (dx * dx + dy * dy + dz * dz > kMarkerReach * kMarkerReach)
            continue;
        ImVec2 at;
        if (!project(viewProjection, rows[i].position, io.DisplaySize.x, io.DisplaySize.y, at))
            continue;
        const float mx = at.x - io.MousePos.x, my = at.y - io.MousePos.y;
        if (mx * mx + my * my < best)
        {
            best = mx * mx + my * my;
            nearest = static_cast<int>(i);
        }
    }
    if (nearest < 0)
        return false;
    const EditSession::SoundRow& row = rows[nearest];
    std::snprintf(line, sizeof(line), "EditorSound.select %s", row.name.c_str());
    session.runCommand(line, "sound");
    session.selection().clear();
    m_soundDrag = true;
    m_soundMoved = false;
    m_soundTargetValid = false;
    m_soundName = row.name;
    m_soundPress[0] = io.MousePos.x;
    m_soundPress[1] = io.MousePos.y;
    float ground = 0.0f;
    m_soundHeight = session.groundHeight(row.position[0], row.position[2], ground) ? row.position[1] - ground : 0.0f;
    return true;
}

void ViewportTools::drawGroundLoop(EditSession& session, const std::vector<std::pair<float, float> >& points,
                                   const float viewProjection[16], unsigned colour, int alpha, float thickness, bool dashed)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const FreeCamera& camera = session.camera();
    const size_t n = points.size();
    int segment = 0;
    for (size_t i = 0; i < n && n >= 2; ++i)
    {
        const std::pair<float, float>& from = points[i];
        const std::pair<float, float>& to = points[(i + 1) % n];
        const float ex = to.first - from.first, ez = to.second - from.second;
        const float length = std::sqrt(ex * ex + ez * ez);
        int steps = static_cast<int>(length / 8.0f) + 1;
        if (steps > 64)
            steps = 64;
        ImVec2 previous;
        bool previousVisible = false;
        for (int s = 0; s <= steps; ++s)
        {
            const float t = static_cast<float>(s) / static_cast<float>(steps);
            const float x = from.first + ex * t, z = from.second + ez * t;
            float ground = 0.0f;
            if (!session.groundHeight(x, z, ground))
                ground = camera.y() - 20.0f;
            const float p[3] = { x, ground + 1.0f, z };
            const float dx = p[0] - camera.x(), dy = p[1] - camera.y(), dz = p[2] - camera.z();
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            ImVec2 at;
            const bool visible = distance < kAreaReach && project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, at);
            if (visible && previousVisible && (!dashed || (segment & 1) == 0))
            {
                const float fade = distance < kAreaFade ? 1.0f : 1.0f - (distance - kAreaFade) / (kAreaReach - kAreaFade);
                const int a = static_cast<int>(static_cast<float>(alpha) * fade);
                list->AddLine(previous, at, (colour & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT),
                              thickness);
            }
            if (s > 0)
                ++segment;
            previous = at;
            previousVisible = visible;
        }
    }
}

void ViewportTools::drawCompareLayer(EditSession& session, const float viewProjection[16])
{
    const LayerSummary* layer = session.compareLayer();
    if (!layer)
        return;
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const FreeCamera& camera = session.camera();
    const ImU32 white = IM_COL32(240, 240, 240, 255);
    // Its combat areas, dashed.
    for (size_t a = 0; a < layer->combatAreas.size(); ++a)
        drawGroundLoop(session, layer->combatAreas[a].points, viewProjection, white, 200, 2.0f, true);
    // Its control points: an outlined flag and the layer's name for it.
    for (size_t c = 0; c < layer->controlPoints.size(); ++c)
    {
        const LayerControlPoint& point = layer->controlPoints[c];
        const float dx = point.position[0] - camera.x(), dy = point.position[1] - camera.y(), dz = point.position[2] - camera.z();
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared > kAreaReach * kAreaReach)
            continue;
        ImVec2 at;
        if (!project(viewProjection, point.position, io.DisplaySize.x, io.DisplaySize.y, at))
            continue;
        list->AddLine(ImVec2(at.x, at.y), ImVec2(at.x, at.y - 20.0f), white, 2.0f);
        list->AddTriangle(ImVec2(at.x, at.y - 20.0f), ImVec2(at.x + 12.0f, at.y - 15.0f), ImVec2(at.x, at.y - 10.0f), white, 1.5f);
        char label[160];
        std::snprintf(label, sizeof(label), "%s %d: %s", layer->mode.c_str(), layer->size, point.name.c_str());
        if (distanceSquared < 300.0f * 300.0f)
            list->AddText(ImVec2(at.x + 14.0f, at.y - 24.0f), white, label);
    }
}

void ViewportTools::handleGizmo(EditSession& session, const float view[16], const float projection[16],
                                const float viewProjection[16])
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::BeginFrame();
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetOrthographic(false);
    // Each arrow keeps pointing along its own +axis. ImGuizmo's default turns an axis round when its
    // negative half faces the camera, which makes the arrows useless for telling +X from -X.
    ImGuizmo::AllowAxisFlip(false);
    // The render viewport is the whole client area, which is ImGui's display.
    ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

    const unsigned id = session.selection().primary();
    Mat4 current;
    const bool shown = session.gizmoMode() != GizmoMode::Select && EditSession::transformOf(id, current);
    if (m_dragging && (!shown || id != m_dragId))
        finishDrag(session);                        // the selection or the mode changed under the drag
    m_gizmoShown = shown;
    if (!shown)
        return;
    // The rotate gizmo can sit on the primary object's anchor and turn the selection about it.
    float anchor[3];
    if (session.gizmoMode() == GizmoMode::Rotate && session.rotateAroundAnchor() && EditSession::worldAnchorOf(id, anchor))
        for (int k = 0; k < 3; ++k)
            current.m[12 + k] = anchor[k];
    if (session.anchorMoveMode() && session.gizmoMode() == GizmoMode::Move)
    {
        if (m_dragging)
            finishDrag(session);
        handleAnchorGizmo(session, view, projection, id, current);
        return;
    }
    m_anchorDragging = false;
    const ImGuizmo::MODE space = session.gizmoSpace() == GizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    // The move gizmo's centre slides the selection over what is under the cursor, as the retail
    // editor's centre box does. The press is taken here, before ImGuizmo sees it, so its own centre
    // handle (a move in the screen plane) is never reached.
    if (!m_dragging && session.gizmoMode() == GizmoMode::Move && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && !pointerOverUi())
    {
        const float origin[3] = { current.m[12], current.m[13], current.m[14] };
        ImVec2 centre;
        if (project(viewProjection, origin, io.DisplaySize.x, io.DisplaySize.y, centre))
        {
            const float dx = io.MousePos.x - centre.x, dy = io.MousePos.y - centre.y;
            if (dx * dx + dy * dy <= kCentreRadius * kCentreRadius)
                beginDrag(session, current, true);
        }
    }
    if (m_groundDrag)
    {
        // Drawn, not driven: the gizmo shows at the moving pivot and takes no input.
        ImGuizmo::Enable(false);
        ImGuizmo::Manipulate(view, projection, ImGuizmo::TRANSLATE, space, m_pivot.m);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            finishDrag(session);
            return;
        }
        // The point grabbed stays under the cursor: the cursor's ray meets the horizontal plane through
        // the pivot, and the selection moves across by as much as that point has moved since the press.
        // Its height follows the terrain: it rises and sinks by the terrain's difference between the two
        // places. A press that does not move moves nothing, exactly.
        float at[3];
        if (m_groundStartValid && m_camera
            && pointOnPlane(*m_camera, session.viewWidth(), session.viewHeight(), io.MousePos.x, io.MousePos.y,
                            m_pivotStart.m[13], at))
        {
            m_pivot = m_pivotStart;
            float x = m_pivotStart.m[12] + (at[0] - m_groundStart[0]);
            float z = m_pivotStart.m[14] + (at[2] - m_groundStart[2]);
            session.snapGrid(x, z);                     // the snap modes
            float ground = 0.0f;
            m_pivot.m[12] = x;
            m_pivot.m[14] = z;
            IObject* primary = EditSession::object(m_dragId);
            if (session.snapModes().ground && primary && session.snapSurface(x, z, ground))
                m_pivot.m[13] = EditSession::snapPivotHeight(primary, ground);
            else if (m_groundOffsetValid && session.groundHeight(x, z, ground))
                m_pivot.m[13] = m_pivotStart.m[13] + (ground - m_groundOffset);
            previewDrag(session);
        }
        return;
    }

    // Idle, the gizmo sits on the primary object; while dragging it carries its own matrix.
    Mat4 pivot = m_dragging ? m_pivot : current;
    const Mat4 before = pivot;
    const bool rotate = session.gizmoMode() == GizmoMode::Rotate;
    float snap[3];
    for (int i = 0; i < 3; ++i)
        snap[i] = rotate ? m_rotateSnap : m_moveSnap;
    const bool snapping = m_snap != io.KeyShift;
    // A panel under the cursor keeps the gizmo out of the way, except for a drag already under way.
    ImGuizmo::Enable(m_dragging || !pointerOverUi());
    const bool changed = ImGuizmo::Manipulate(view, projection, rotate ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE,
                                              space, pivot.m, 0, snapping ? snap : 0);
    if (ImGuizmo::IsUsing() && !m_dragging)
        beginDrag(session, before, false);
    if (!m_dragging)
        return;
    m_pivot = pivot;
    // The drag previews on the objects themselves; the edit is made when the button comes up.
    if (changed)
        previewDrag(session);
    if (!ImGuizmo::IsUsing())
        finishDrag(session);
}

void ViewportTools::handleAnchorGizmo(EditSession& session, const float view[16], const float projection[16],
                                      unsigned id, const Mat4& transform)
{
    float world[3];
    if (!EditSession::worldAnchorOf(id, world))
        return;
    Mat4 pivot;
    if (m_anchorDragging)
        pivot = m_anchorPivot;
    else
    {
        for (int i = 0; i < 16; ++i)
            pivot.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        pivot.m[12] = world[0];
        pivot.m[13] = world[1];
        pivot.m[14] = world[2];
    }
    const ImGuiIO& io = ImGui::GetIO();
    float snap[3] = { m_moveSnap, m_moveSnap, m_moveSnap };
    ImGuizmo::Enable(m_anchorDragging || !pointerOverUi());
    ImGuizmo::Manipulate(view, projection, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, pivot.m, 0,
                         (m_snap != io.KeyShift) ? snap : 0);
    if (ImGuizmo::IsUsing())
    {
        m_anchorDragging = true;
        m_anchorPivot = pivot;
        return;
    }
    if (!m_anchorDragging)
        return;
    m_anchorDragging = false;
    // The new anchor in the object's frame, less the template's anchor offset (getAnchorPos adds it back;
    // retail set the whole point as the anchor, offset included).
    float inverse[16];
    if (!matrixInvertAffine(transform.m, inverse))
        return;
    float local[3];
    for (int k = 0; k < 3; ++k)
        local[k] = pivot.m[12] * inverse[k] + pivot.m[13] * inverse[4 + k] + pivot.m[14] * inverse[8 + k] + inverse[12 + k];
    IObject* o = EditSession::object(id);
    if (o && o->getTemplate())
    {
        const dice::hfe::Vec3& offset = o->getTemplate()->getAnchorOffset();
        local[0] -= offset.x;
        local[1] -= offset.y;
        local[2] -= offset.z;
    }
    char line[160];
    std::snprintf(line, sizeof(line), "EditorSelection.setAnchor %.4f/%.4f/%.4f", local[0], local[1], local[2]);
    session.runCommand(line, "anchor");
}

void ViewportTools::beginDrag(EditSession& session, const Mat4& pivotStart, bool ground)
{
    const ImGuiIO& io = ImGui::GetIO();
    m_dragging = true;
    m_groundDrag = ground;
    m_dragMode = static_cast<int>(ground ? GizmoMode::Move : session.gizmoMode());
    m_pivotStart = pivotStart;
    m_pivot = pivotStart;
    m_dragItems.clear();
    m_dragDouble = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    if (ground)
    {
        // Where the cursor meets the pivot's horizontal plane as the slide starts, and the terrain
        // height under the pivot (the slide's height follows the terrain's difference from it).
        m_groundStartValid = m_camera && pointOnPlane(*m_camera, session.viewWidth(), session.viewHeight(),
                                                      io.MousePos.x, io.MousePos.y, pivotStart.m[13], m_groundStart);
        m_groundOffsetValid = session.groundHeight(pivotStart.m[12], pivotStart.m[14], m_groundOffset);
    }
    // Ctrl held as the drag starts: copies are made where the selection stands and they are what the
    // drag moves; the originals stay. Copies that cannot be made (the objects belong to a file that
    // defines their templates) leave nothing to move.
    if (io.KeyCtrl)
    {
        std::string result;
        const bool copied = session.duplicateSelection(result);
        logLine(copied ? LogLevel::Info : LogLevel::Warning, "copy: %s", result.c_str());
        if (!copied)
        {
            m_dragId = session.selection().primary();
            return;
        }
    }
    m_dragId = session.selection().primary();
    const std::vector<unsigned>& ids = session.selection().ids();
    for (size_t i = 0; i < ids.size(); ++i)
    {
        DragItem item;
        item.id = ids[i];
        if (EditSession::transformOf(item.id, item.start))
            m_dragItems.push_back(item);
    }
}

void ViewportTools::previewDrag(EditSession& session)
{
    for (size_t i = 0; i < m_dragItems.size(); ++i)
    {
        IObject* object = EditSession::object(m_dragItems[i].id);
        Mat4 moved;
        if (!object || !matrixFollowPivot(m_dragItems[i].start.m, m_pivotStart.m, m_pivot.m, moved.m))
            continue;
        // A free move snaps each of the other objects too: onto the grid and down on the ground (V1).
        if (m_groundDrag && m_dragItems[i].id != m_dragId)
        {
            session.snapGrid(moved.m[12], moved.m[14]);
            float ground = 0.0f;
            if (session.snapModes().ground && session.snapSurface(moved.m[12], moved.m[14], ground))
                moved.m[13] = EditSession::snapPivotHeight(object, ground);
        }
        object->setAbsoluteTransformation(moved);
    }
}

void ViewportTools::finishDrag(EditSession& session)
{
    m_dragging = false;
    m_groundDrag = false;
    std::vector<EditSession::TransformChange> changes;
    for (size_t i = 0; i < m_dragItems.size(); ++i)
    {
        EditSession::TransformChange change;
        change.id = m_dragItems[i].id;
        change.before = m_dragItems[i].start;
        if (!EditSession::transformOf(change.id, change.after))
            continue;                               // the object left the level mid-drag
        if (std::memcmp(change.after.m, change.before.m, sizeof(change.after.m)) != 0)
            changes.push_back(change);
    }
    if (changes.empty())
    {
        // A click on the gizmo that moved nothing; a double click on its centre opens the numeric
        // fields, as a double click on the object does.
        if (m_dragDouble)
            openSelectionPanel(session);
        return;
    }
    const char* verb = m_dragMode == static_cast<int>(GizmoMode::Rotate) ? "Rotate" : "Move";
    std::string label = std::string(verb) + " ";
    if (changes.size() == 1)
    {
        label += EditSession::describe(changes[0].id);
    }
    else
    {
        char count[32];
        std::snprintf(count, sizeof(count), "%u objects", static_cast<unsigned>(changes.size()));
        label += count;
    }
    const unsigned revision = session.history().revision();
    std::string error;
    if (!session.applyTransforms(changes, label, error))
    {
        // Not recorded: the preview was not an edit, so the objects go back to where the history
        // says they are.
        if (session.history().revision() == revision)
        {
            for (size_t i = 0; i < changes.size(); ++i)
            {
                IObject* object = EditSession::object(changes[i].id);
                if (object)
                    object->setAbsoluteTransformation(changes[i].before);
            }
        }
        logLine(LogLevel::Error, "%s", error.c_str());
    }
}

void ViewportTools::openSelectionPanel(EditSession& session)
{
    runConsoleLine("EditorView.panel Selection 1");
    ImGui::SetWindowFocus("Selection");
    logLine(LogLevel::Info, "view: the Selection panel, for %s", EditSession::describe(session.selection().primary()).c_str());
}

void ViewportTools::setSnap(bool snap, float moveSnap, float rotateSnap)
{
    m_snap = snap;
    if (moveSnap > 0.0f)
        m_moveSnap = moveSnap;
    if (rotateSnap > 0.0f)
        m_rotateSnap = rotateSnap;
}

void ViewportTools::handleClicks(EditSession& session)
{
    const ImGuiIO& io = ImGui::GetIO();
    // A drag and drop under way (a template from the Resources panel) is the drop's, not a click.
    if (ImGui::GetDragDropPayload())
    {
        m_pressed = false;
        return;
    }
    // A press in the 3D view that no gizmo took (the gizmo and its centre are handled before this).
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_pressed = !pointerOverUi() && !m_dragging
                 && !(m_gizmoShown && (ImGuizmo::IsOver() || ImGuizmo::IsUsing()));
        m_pressDouble = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        m_boxing = false;
        m_pressX = io.MousePos.x;
        m_pressY = io.MousePos.y;
    }
    if (!m_pressed)
        return;
    if (m_dragging)
    {
        m_pressed = false;                          // a drag took the press after all
        return;
    }
    const float dx = io.MousePos.x - m_pressX, dy = io.MousePos.y - m_pressY;
    if (!m_boxing && dx * dx + dy * dy > kBoxThreshold * kBoxThreshold)
        m_boxing = true;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (m_boxing)
        {
            ImDrawList* list = ImGui::GetBackgroundDrawList();
            const ImVec2 a(m_pressX < io.MousePos.x ? m_pressX : io.MousePos.x,
                           m_pressY < io.MousePos.y ? m_pressY : io.MousePos.y);
            const ImVec2 b(m_pressX < io.MousePos.x ? io.MousePos.x : m_pressX,
                           m_pressY < io.MousePos.y ? io.MousePos.y : m_pressY);
            list->AddRectFilled(a, b, IM_COL32(255, 170, 40, 40));
            list->AddRect(a, b, IM_COL32(255, 170, 40, 220), 0.0f, 1.5f);
        }
        return;
    }

    // The button came up.
    m_pressed = false;
    const PickMode mode = io.KeyCtrl ? PickMode::Toggle : (io.KeyShift ? PickMode::Add : PickMode::Set);
    if (m_boxing)
    {
        const char* command = mode == PickMode::Toggle ? "boxToggle" : (mode == PickMode::Add ? "boxAdd" : "box");
        char line[160];
        std::snprintf(line, sizeof(line), "EditorSelection.%s %.1f %.1f %.1f %.1f", command, m_pressX, m_pressY,
                      io.MousePos.x, io.MousePos.y);
        session.runCommand(line, "select");
        return;
    }
    std::string result;
    session.pickInto(io.MousePos.x, io.MousePos.y, mode, result);
    logLine(LogLevel::Info, "pick: %s", result.c_str());
    // A double click opens the numeric position and rotation fields.
    if (m_pressDouble && session.selection().primary())
        openSelectionPanel(session);
}

// A drag from the Resources panel: nothing takes it over a panel, so it is the view's when no ImGui
// window is under the cursor (the dockspace's central node passes the mouse through). The payload
// lives until the frame after the button comes up, so the release is seen here.
void ViewportTools::handleDrop(EditSession& session, const float viewProjection[16])
{
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if (!payload || !payload->IsDataType(ResourceBrowser::templatePayloadType()) || !payload->Data
        || payload->DataSize <= 0 || static_cast<const char*>(payload->Data)[payload->DataSize - 1] != 0)
        return;
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        return;
    const std::string templateName = ResourceBrowser::templateOf(static_cast<const char*>(payload->Data));
    if (templateName.empty())
        return;
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        char line[320];
        std::snprintf(line, sizeof(line), "EditorObject.placeAt %s %.1f %.1f", templateName.c_str(), io.MousePos.x,
                      io.MousePos.y);
        const ConsoleLineResult r = runConsoleLine(line);
        logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "place: %s", r.output.c_str());
        if (!r.failed && m_resources)
            m_resources->noteUsed(static_cast<const char*>(payload->Data));
        return;
    }
    float at[3];
    std::string where;
    ImDrawList* list = ImGui::GetForegroundDrawList();
    ImVec2 point;
    if (session.surfaceAt(io.MousePos.x, io.MousePos.y, at, where)
        && project(viewProjection, at, io.DisplaySize.x, io.DisplaySize.y, point))
    {
        list->AddCircle(point, 9.0f, IM_COL32(255, 170, 40, 255), 20, 2.0f);
        list->AddLine(ImVec2(point.x - 14.0f, point.y), ImVec2(point.x + 14.0f, point.y), IM_COL32(255, 170, 40, 255));
        list->AddLine(ImVec2(point.x, point.y - 14.0f), ImVec2(point.x, point.y + 14.0f), IM_COL32(255, 170, 40, 255));
        char text[160];
        std::snprintf(text, sizeof(text), "%.1f/%.1f/%.1f %s", at[0], at[1], at[2], where.c_str());
        list->AddText(ImVec2(point.x + 14.0f, point.y + 10.0f), IM_COL32(255, 230, 160, 255), text);
    }
    else
    {
        list->AddText(ImVec2(io.MousePos.x + 14.0f, io.MousePos.y + 24.0f), IM_COL32(255, 120, 100, 255),
                      "nothing to place it on here");
    }
}

void ViewportTools::handleShortcuts(EditSession& session, const FreeCamera& camera)
{
    // Every key comes from the shortcut table (App/ShortcutKeys; Help > Keyboard shortcuts changes
    // them). A plain key is the camera's while the right button looks around.
    const bool looking = camera.isLooking();
    struct Pressed
    {
        bool looking;
        bool operator()(const char* id) const
        {
            if (!shortcutPressed(id))
                return false;
            const KeyChord& chord = shortcuts().chord(id);
            return chord.ctrl || chord.alt || !looking;
        }
    };
    const Pressed pressed = { looking };
    const bool selected = !session.selection().empty();
    if (pressed("edit.undo"))
        session.runUndo();
    if (pressed("edit.redo") || pressed("edit.redoAlt"))
        session.runRedo();
    if (pressed("file.save"))
        session.runSave();
    if (pressed("edit.duplicate") && selected)
        session.runDuplicate();
    if (pressed("camera.resetSpeed"))
        session.runCommand("EditorCamera.resetSpeed", "camera");
    if (pressed("camera.free"))
        session.runCommand("EditorCamera.mode 0", "camera");
    if (pressed("camera.topDown"))
        session.runCommand("EditorCamera.mode 1", "camera");
    if (pressed("camera.orbit"))
        session.runCommand("EditorCamera.mode 2", "camera");
    if (pressed("edit.copy") && selected)
        session.runCommand("EditorClipboard.copy", "copy");
    if (pressed("edit.cut") && selected)
        session.runCommand("EditorClipboard.cut", "cut");
    if (pressed("edit.paste") && session.clipboardSize() != 0)
        session.runCommand("EditorClipboard.paste", "paste");
    const char* const modes[] = { "gizmo.select", "gizmo.move", "gizmo.rotate" };
    for (int i = 0; i < 3; ++i)
    {
        if (!pressed(modes[i]))
            continue;
        session.setMode(static_cast<GizmoMode>(i));
        if (session.terrainToolActive())
            session.runCommand("EditorTerrain.tool 0", "terrain");
        if (session.brushToolActive())
            session.runCommand("EditorBrush.tool 0", "brush");
        if (session.undergrowthToolActive())
            session.runCommand("EditorUndergrowth.tool 0", "undergrowth");
    }
    if (pressed("terrain.tool"))
        session.runCommand(session.terrainToolActive() ? "EditorTerrain.tool 0" : "EditorTerrain.tool 1", "terrain");
    if (pressed("brush.tool"))
        session.runCommand(session.brushToolActive() ? "EditorBrush.tool 0" : "EditorBrush.tool 1", "brush");
    if (pressed("undergrowth.tool"))
        session.runCommand(session.undergrowthToolActive() ? "EditorUndergrowth.tool 0" : "EditorUndergrowth.tool 1",
                           "undergrowth");
    if (pressed("gizmo.space"))
        session.setSpace(session.gizmoSpace() == GizmoSpace::World ? GizmoSpace::Local : GizmoSpace::World);
    if (pressed("selection.clear"))
    {
        session.selection().clear();
        std::string ignored;
        if (session.activeCombatPoint() >= 0)
            session.selectCombatArea(session.activeCombatArea(), -1, ignored);
        session.selectSound(std::string(), ignored);
    }
    if (pressed("edit.delete"))
    {
        // A selected combat area point goes before any object: clicking the point cleared the objects.
        if (session.activeCombatPoint() >= 0 && !session.activeCombatArea().empty())
        {
            char line[200];
            std::snprintf(line, sizeof(line), "EditorCombatArea.removePoint %s %d", session.activeCombatArea().c_str(),
                          session.activeCombatPoint());
            session.runCommand(line, "combat area");
        }
        else if (selected)
            session.runDelete();
        else if (!session.activeSound().empty())
            session.runCommand("EditorSound.delete " + session.activeSound(), "sound");
    }
    if (pressed("selection.snapToGround") && !session.selection().empty())
        session.runSnapToGround();
    if (pressed("selection.frame"))
    {
        std::string error;
        if (!session.frameSelection(error))
            logLine(LogLevel::Warning, "frame: %s", error.c_str());
    }
}

void ViewportTools::drawToolbar(EditSession& session, const ImVec2& viewPos)
{
    ImGui::SetNextWindowPos(ImVec2(viewPos.x + 8.0f, viewPos.y + 8.0f));
    ImGui::SetNextWindowBgAlpha(0.8f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
                                 | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
                                 | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("##ViewportToolbar", 0, flags))
    {
        // The gizmo's modes and the two brushes are one choice: picking a mode turns a brush off.
        const bool terrain = session.terrainToolActive() || session.undergrowthToolActive();
        const bool painting = session.brushToolActive();
        const int mode = static_cast<int>(session.gizmoMode());
        const char* const modeLabels[] = { "Select (1)", "Move (2)", "Rotate (3)" };
        for (int i = 0; i < 3; ++i)
        {
            if (i)
                ImGui::SameLine();
            if (ImGui::RadioButton(modeLabels[i], !terrain && !painting && mode == i))
            {
                session.setMode(static_cast<GizmoMode>(i));
                if (terrain)
                    session.runCommand("EditorTerrain.tool 0", "terrain");
                if (painting)
                    session.runCommand("EditorBrush.tool 0", "brush");
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Terrain (T)", terrain) && !terrain)
            session.runCommand("EditorTerrain.tool 1", "terrain");
        ImGui::SameLine();
        if (ImGui::RadioButton("Paint (B)", painting) && !painting)
            session.runCommand("EditorBrush.tool 1", "brush");

        ImGui::SameLine(0.0f, 16.0f);
        int space = static_cast<int>(session.gizmoSpace());
        ImGui::RadioButton("World", &space, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Local (X)", &space, 1);
        session.setSpace(static_cast<GizmoSpace>(space));

        if (session.gizmoMode() == GizmoMode::Move && !terrain && !painting)
        {
            ImGui::SameLine();
            bool anchorMode = session.anchorMoveMode();
            if (ImGui::Checkbox("Move the anchor", &anchorMode))
                session.runCommand(anchorMode ? "EditorGizmo.anchorMode 1" : "EditorGizmo.anchorMode 0", "gizmo");
            ImGui::SetItemTooltip("The gizmo moves the primary object's anchor - its template's, so every object of "
                                  "the template - and the template file keeps it (an undoable edit).");
        }
        if (session.gizmoMode() == GizmoMode::Rotate && !terrain && !painting)
        {
            ImGui::SameLine();
            bool aboutAnchor = session.rotateAroundAnchor();
            if (ImGui::Checkbox("About the anchor", &aboutAnchor))
                session.runCommand(aboutAnchor ? "EditorGizmo.pivot 1" : "EditorGizmo.pivot 0", "gizmo");
            ImGui::SetItemTooltip("Turn the selection about the primary object's anchor instead of its pivot.");
        }

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::Checkbox("Snap", &m_snap);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        if (session.gizmoMode() == GizmoMode::Rotate)
            ImGui::DragFloat("##rotateSnap", &m_rotateSnap, 0.5f, 1.0f, 90.0f, "%.0f deg");
        else
            ImGui::DragFloat("##moveSnap", &m_moveSnap, 0.05f, 0.05f, 100.0f, "%.2f m");

        const unsigned primary = session.selection().primary();
        if (session.terrainToolActive())
        {
            const TerrainBrushSettings& brush = session.terrainBrush();
            ImGui::Text("Terrain: %s, size %d, strength %d", terrainBrushName(brush.mode), brush.size, brush.strength);
            ImGui::TextDisabled("Left paints, middle or Shift+left the other action; Shift+wheel strength, Ctrl+wheel size.");
        }
        else if (session.undergrowthToolActive())
        {
            ImGui::Text("Undergrowth: material %d, radius %.1f m", session.undergrowthMaterial(), session.undergrowthRadius());
            ImGui::TextDisabled("Left paints, middle or Shift+left erases; Ctrl+wheel the radius. Shown after a reload.");
        }
        else if (session.brushToolActive())
        {
            ImGui::Text("Paint: brush %s, erase radius %.1f m", session.activeBrush().c_str(), session.brushEraseRadius());
            ImGui::TextDisabled("Left paints, middle or Shift+left erases the brush's objects; Ctrl+wheel the erase radius.");
        }
        else if (!session.armedBundle().empty())
            ImGui::Text("Click the terrain to place the bundle %s (Esc cancels).", session.armedBundle().c_str());
        else if (primary)
            ImGui::Text("Selected: %s", EditSession::describe(primary).c_str());
        else
            ImGui::TextDisabled("Click an object to select it, or drag a box (Esc clears, F frames it).");
    }
    ImGui::End();
}

// The terrain brush (phase E)

void ViewportTools::handleTerrain(EditSession& session)
{
    const ImGuiIO& io = ImGui::GetIO();
    TerrainBrushSettings& brush = session.terrainBrush();
    // The retail plugin's wheel (WM_MOUSEWHEEL, census §2.2): Shift changes the strength by 5, Ctrl the
    // size by 1; the camera leaves those turns to the brush (FreeCamera::setModifiedWheelReserved).
    if (io.MouseWheel != 0.0f && !pointerOverUi() && (io.KeyShift || io.KeyCtrl))
    {
        const int steps = io.MouseWheel > 0.0f ? 1 : -1;
        if (io.KeyCtrl)
        {
            brush.size += steps;
            brush.size = brush.size < 1 ? 1 : (brush.size > 50 ? 50 : brush.size);
        }
        else
        {
            brush.strength += 5 * steps;
            brush.strength = brush.strength < 1 ? 1 : (brush.strength > 100 ? 100 : brush.strength);
        }
    }
    if (!m_terrainStroke)
    {
        if (!session.terrainToolActive() || pointerOverUi() || ImGui::GetDragDropPayload())
            return;
        // The left button is the brush's action; the other one is the middle button (the right one
        // looks around, as the retail editor's look button did) or Shift with the left.
        const bool left = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool middle = !left && ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
        if (!left && !middle)
            return;
        std::string error;
        if (!session.beginTerrainStroke(error))
        {
            logLine(LogLevel::Warning, "terrain: %s", error.c_str());
            return;
        }
        m_terrainStroke = true;
        m_terrainButton = left ? ImGuiMouseButton_Left : ImGuiMouseButton_Middle;
        m_terrainSecondary = middle || io.KeyShift;
        m_terrainError.clear();
    }
    // Once a frame while the button is held, wherever the cursor wanders (the retail plugin's armed
    // button keeps a stroke going outside the view too); the stroke ends when the button comes up.
    if (!ImGui::IsMouseDown(m_terrainButton) || !session.terrainStrokeActive())
    {
        m_terrainStroke = false;
        session.endTerrainStroke();
        return;
    }
    float at[3];
    if (!session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, at))
        return;
    std::string error;
    if (!session.terrainStrokeTick(at[0], at[2], m_terrainSecondary, io.DeltaTime, error))
    {
        if (error != m_terrainError)
            logLine(LogLevel::Warning, "terrain: %s", error.c_str());
        m_terrainError = error;
    }
}

void ViewportTools::drawTerrainBrush(EditSession& session, const float viewProjection[16])
{
    const ImGuiIO& io = ImGui::GetIO();
    if (pointerOverUi() && !m_terrainStroke)
        return;
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    float at[3];
    if (!session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, at))
    {
        list->AddText(ImVec2(io.MousePos.x + 14.0f, io.MousePos.y + 12.0f), IM_COL32(255, 120, 100, 255),
                      "no terrain under the cursor");
        return;
    }
    const TerrainBrushSettings& brush = session.terrainBrush();
    int centreX = 0, centreZ = 0;
    float radius = 0.0f;
    std::string error;
    if (!session.terrain().brushAt(brush, at[0], at[2], centreX, centreZ, radius, error))
        return;
    const ImU32 orange = IM_COL32(255, 170, 40, 255);
    if (brush.mode == TerrainBrush::Overgrowth)
    {
        // Overgrowth: the renderer paints the whole radius around the raw cursor (OverGrowth::paintMaterial).
        std::vector<std::pair<float, float> > ring;
        for (int i = 0; i < 64; ++i)
        {
            const float angle = 6.2831853f * static_cast<float>(i) / 64.0f;
            ring.push_back(std::make_pair(at[0] + std::cos(angle) * radius, at[2] + std::sin(angle) * radius));
        }
        drawGroundLoop(session, ring, viewProjection, IM_COL32(110, 230, 110, 255), 235, 2.0f, false);
        return;
    }
    if (brush.mode == TerrainBrush::Paint)
    {
        // Paint: what the renderer paints - a quarter of the radius it is given (TerrainEditable R2), the circle the
        // retail plugin drew for mode 7 - around the raw cursor.
        std::vector<std::pair<float, float> > paintRing;
        for (int i = 0; i < 64; ++i)
        {
            const float angle = 6.2831853f * static_cast<float>(i) / 64.0f;
            paintRing.push_back(std::make_pair(at[0] + std::cos(angle) * radius * 0.25f, at[2] + std::sin(angle) * radius * 0.25f));
        }
        drawGroundLoop(session, paintRing, viewProjection, IM_COL32(120, 220, 255, 255), 235, 2.0f, false);
        return;
    }
    // The circle along the ground.
    std::vector<std::pair<float, float> > ring;
    const int segments = 64;
    for (int i = 0; i < segments; ++i)
    {
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(segments);
        ring.push_back(std::make_pair(static_cast<float>(centreX) + std::cos(angle) * radius,
                                      static_cast<float>(centreZ) + std::sin(angle) * radius));
    }
    drawGroundLoop(session, ring, viewProjection, orange, 235, 2.0f, false);
    // A dot on every vertex the brush moves, as the retail cursor drew them, brighter where the falloff
    // moves it more; thinned out when there are very many (a wide brush), so the view stays readable.
    std::vector<TerrainEditing::BrushSample> samples;
    session.terrain().brushSamples(centreX, centreZ, radius, samples);
    const size_t limit = 2500;
    const size_t stride = samples.size() > limit ? (samples.size() + limit - 1) / limit : 1;
    for (size_t i = 0; i < samples.size(); i += stride)
    {
        float ground = 0.0f;
        if (!session.groundHeight(samples[i].x, samples[i].z, ground))
            continue;
        const float p[3] = { samples[i].x, ground + 0.2f, samples[i].z };
        ImVec2 dot;
        if (!project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, dot))
            continue;
        const int alpha = 70 + static_cast<int>(185.0f * samples[i].weight);
        list->AddRectFilled(ImVec2(dot.x - 1.5f, dot.y - 1.5f), ImVec2(dot.x + 1.5f, dot.y + 1.5f),
                            IM_COL32(255, 210, 120, alpha));
    }
    // Platform: the height it builds to, as a circle at that height.
    if (brush.mode == TerrainBrush::Platform)
    {
        ImVec2 previous;
        bool previousVisible = false;
        for (int i = 0; i <= segments; ++i)
        {
            const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(segments);
            const float p[3] = { static_cast<float>(centreX) + std::cos(angle) * radius, brush.height,
                                 static_cast<float>(centreZ) + std::sin(angle) * radius };
            ImVec2 point;
            const bool visible = project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, point);
            if (visible && previousVisible)
                list->AddLine(previous, point, IM_COL32(90, 200, 255, 220), 1.5f);
            previous = point;
            previousVisible = visible;
        }
    }
    char text[160];
    std::snprintf(text, sizeof(text), "%s %d (%.0f m), strength %d  %.1f m", terrainBrushName(brush.mode), brush.size,
                  radius, brush.strength, at[1]);
    list->AddText(ImVec2(io.MousePos.x + 16.0f, io.MousePos.y + 14.0f), IM_COL32(255, 230, 160, 255), text);
}

// The object paint brush and bundle placing

void ViewportTools::handleBrush(EditSession& session)
{
    const ImGuiIO& io = ImGui::GetIO();
    // Ctrl+wheel: the erase radius, a metre a turn (the camera leaves modified turns to the brushes).
    if (io.MouseWheel != 0.0f && io.KeyCtrl && !pointerOverUi())
    {
        float radius = session.brushEraseRadius() + (io.MouseWheel > 0.0f ? 1.0f : -1.0f);
        radius = radius < 0.5f ? 0.5f : (radius > 500.0f ? 500.0f : radius);
        std::string ignored;
        session.setBrushEraseRadius(radius, ignored);
    }
    if (!m_brushStroke)
    {
        if (!session.brushToolActive() || pointerOverUi() || ImGui::GetDragDropPayload())
            return;
        const bool left = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool middle = !left && ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
        if (!left && !middle)
            return;
        std::string error;
        if (!session.beginBrushStroke(middle || io.KeyShift, error))
        {
            logLine(LogLevel::Warning, "brush: %s", error.c_str());
            return;
        }
        m_brushStroke = true;
        m_brushButton = left ? ImGuiMouseButton_Left : ImGuiMouseButton_Middle;
        m_brushError.clear();
    }
    // Once a frame while the button is held; the stroke ends, and becomes one edit, when it comes up.
    if (!ImGui::IsMouseDown(m_brushButton) || !session.brushStrokeActive())
    {
        m_brushStroke = false;
        session.endBrushStroke();
        return;
    }
    float at[3];
    if (!session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, at))
        return;
    std::string error;
    if (!session.brushStrokeTick(at, error))
    {
        if (error != m_brushError)
            logLine(LogLevel::Warning, "brush: %s", error.c_str());
        m_brushError = error;
    }
}

void ViewportTools::drawBrushCursor(EditSession& session, const float viewProjection[16])
{
    const ImGuiIO& io = ImGui::GetIO();
    if (pointerOverUi() && !m_brushStroke)
        return;
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    float at[3];
    if (!session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, at))
    {
        list->AddText(ImVec2(io.MousePos.x + 14.0f, io.MousePos.y + 12.0f), IM_COL32(255, 120, 100, 255),
                      "no terrain under the cursor");
        return;
    }
    const LevelBrushFile* brush = session.activeBrushFile();
    const bool erasing = m_brushStroke ? session.brushStrokeErases() : (io.KeyShift || ImGui::IsMouseDown(ImGuiMouseButton_Middle));
    // The erase circle, or the spacing: the next stamp comes once the cursor leaves it.
    const float radius = erasing ? session.brushEraseRadius() : (brush ? brush->spacing : 1.0f);
    std::vector<std::pair<float, float> > ring;
    const int segments = 48;
    for (int i = 0; i < segments; ++i)
    {
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(segments);
        ring.push_back(std::make_pair(at[0] + std::cos(angle) * radius, at[2] + std::sin(angle) * radius));
    }
    drawGroundLoop(session, ring, viewProjection, erasing ? IM_COL32(255, 90, 80, 255) : IM_COL32(120, 220, 120, 255), 235,
                   2.0f, !erasing);
    char text[200];
    if (erasing)
        std::snprintf(text, sizeof(text), "erase %s's objects within %.1f m", brush ? brush->name.c_str() : "the brush", radius);
    else
        std::snprintf(text, sizeof(text), "%s: %u items, every %.1f m", brush ? brush->name.c_str() : "no brush",
                      brush ? static_cast<unsigned>(brush->items.size()) : 0u, radius);
    list->AddText(ImVec2(io.MousePos.x + 16.0f, io.MousePos.y + 14.0f), IM_COL32(210, 255, 210, 255), text);
}

void ViewportTools::handleBundlePlacement(EditSession& session, const float viewProjection[16])
{
    const ImGuiIO& io = ImGui::GetIO();
    const std::string name = session.armedBundle();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        session.armBundle(std::string(), 0.0f);
        logLine(LogLevel::Info, "bundle: placing %s cancelled", name.c_str());
        return;
    }
    if (pointerOverUi())
        return;
    float at[3];
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    if (!session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, at))
    {
        list->AddText(ImVec2(io.MousePos.x + 14.0f, io.MousePos.y + 12.0f), IM_COL32(255, 120, 100, 255),
                      "no terrain under the cursor");
        return;
    }
    ImVec2 point;
    if (project(viewProjection, at, io.DisplaySize.x, io.DisplaySize.y, point))
    {
        list->AddCircle(point, 10.0f, IM_COL32(255, 170, 40, 255), 20, 2.0f);
        list->AddLine(ImVec2(point.x - 15.0f, point.y), ImVec2(point.x + 15.0f, point.y), IM_COL32(255, 170, 40, 255));
        list->AddLine(ImVec2(point.x, point.y - 15.0f), ImVec2(point.x, point.y + 15.0f), IM_COL32(255, 170, 40, 255));
    }
    char text[200];
    std::snprintf(text, sizeof(text), "place %s here (Esc cancels)", name.c_str());
    list->AddText(ImVec2(io.MousePos.x + 16.0f, io.MousePos.y + 14.0f), IM_COL32(255, 230, 160, 255), text);
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return;
    char line[320];
    std::snprintf(line, sizeof(line), "EditorBundle.place %s %.3f/%.3f/%.3f %g", name.c_str(), at[0], at[1], at[2],
                  session.armedBundleYaw());
    const ConsoleLineResult r = runConsoleLine(line);
    logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "bundle: %s", r.output.c_str());
    if (!r.failed)
        session.armBundle(std::string(), 0.0f);
}

// The undergrowth tool

void ViewportTools::handleUndergrowth(EditSession& session)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.MouseWheel != 0.0f && io.KeyCtrl && !pointerOverUi())
    {
        float radius = session.undergrowthRadius() + (io.MouseWheel > 0.0f ? 1.0f : -1.0f);
        radius = radius < 0.5f ? 0.5f : (radius > 200.0f ? 200.0f : radius);
        std::string ignored;
        session.setUndergrowthRadius(radius, ignored);
    }
    if (!m_undergrowthStroke)
    {
        if (!session.undergrowthToolActive() || pointerOverUi() || ImGui::GetDragDropPayload())
            return;
        const bool left = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool middle = !left && ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
        if (!left && !middle)
            return;
        std::string error;
        if (!session.beginUndergrowthStroke(middle || io.KeyShift, error))
        {
            logLine(LogLevel::Warning, "undergrowth: %s", error.c_str());
            return;
        }
        m_undergrowthStroke = true;
        m_undergrowthButton = left ? ImGuiMouseButton_Left : ImGuiMouseButton_Middle;
    }
    if (!ImGui::IsMouseDown(m_undergrowthButton) || !session.undergrowthStrokeActive())
    {
        m_undergrowthStroke = false;
        session.endUndergrowthStroke();
        return;
    }
    float at[3];
    if (!session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, at))
        return;
    std::string error;
    session.undergrowthStrokeTick(at[0], at[2], error);
}

void ViewportTools::drawUndergrowthCursor(EditSession& session, const float viewProjection[16])
{
    const ImGuiIO& io = ImGui::GetIO();
    if (pointerOverUi() && !m_undergrowthStroke)
        return;
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    float at[3];
    if (!session.terrainUnderPixel(io.MousePos.x, io.MousePos.y, at))
    {
        list->AddText(ImVec2(io.MousePos.x + 14.0f, io.MousePos.y + 12.0f), IM_COL32(255, 120, 100, 255),
                      "no terrain under the cursor");
        return;
    }
    const UndergrowthMap* map = session.undergrowthMap();
    const float scale = session.undergrowthScale();
    const float radius = session.undergrowthRadius();
    const bool erasing = m_undergrowthStroke ? session.undergrowthStrokeErases()
                                             : (io.KeyShift || ImGui::IsMouseDown(ImGuiMouseButton_Middle));
    std::vector<std::pair<float, float> > ring;
    const int segments = 48;
    for (int i = 0; i < segments; ++i)
    {
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(segments);
        ring.push_back(std::make_pair(at[0] + std::cos(angle) * radius, at[2] + std::sin(angle) * radius));
    }
    drawGroundLoop(session, ring, viewProjection, erasing ? IM_COL32(255, 90, 80, 255) : IM_COL32(150, 230, 90, 255), 235,
                   2.0f, false);
    // The cells around the cursor, a dot each coloured by material (none left out), out to twice the radius.
    if (map && scale > 0.0f)
    {
        static const ImU32 palette[] = { IM_COL32(230, 90, 90, 220), IM_COL32(90, 200, 90, 220), IM_COL32(90, 140, 240, 220),
                                         IM_COL32(230, 200, 70, 220), IM_COL32(200, 90, 220, 220), IM_COL32(70, 210, 210, 220),
                                         IM_COL32(240, 150, 60, 220), IM_COL32(170, 170, 170, 220) };
        const float reach = radius * 2.0f;
        int column0 = 0, row0 = 0, column1 = 0, row1 = 0;
        map->cellAt(at[0] - reach, at[2] - reach, scale, column0, row0);
        map->cellAt(at[0] + reach, at[2] + reach, scale, column1, row1);
        column0 = column0 < 0 ? 0 : column0;
        row0 = row0 < 0 ? 0 : row0;
        column1 = column1 >= map->size() ? map->size() - 1 : column1;
        row1 = row1 >= map->size() ? map->size() - 1 : row1;
        const int step = (column1 - column0) > 120 ? (column1 - column0) / 120 + 1 : 1;
        for (int row = row0; row <= row1; row += step)
        {
            for (int column = column0; column <= column1; column += step)
            {
                const unsigned char id = map->at(column, row);
                if (id == 0)
                    continue;
                float x = 0.0f, z = 0.0f;
                map->cellPosition(column, row, scale, x, z);
                x += scale * 0.5f;
                z += scale * 0.5f;
                float ground = 0.0f;
                if (!session.groundHeight(x, z, ground))
                    continue;
                const float p[3] = { x, ground + 0.15f, z };
                ImVec2 dot;
                if (!project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, dot))
                    continue;
                list->AddRectFilled(ImVec2(dot.x - 1.5f, dot.y - 1.5f), ImVec2(dot.x + 1.5f, dot.y + 1.5f),
                                    palette[id % (sizeof(palette) / sizeof(palette[0]))]);
            }
        }
    }
    char text[160];
    if (erasing)
        std::snprintf(text, sizeof(text), "erase undergrowth within %.1f m", radius);
    else
        std::snprintf(text, sizeof(text), "undergrowth material %d within %.1f m", session.undergrowthMaterial(), radius);
    list->AddText(ImVec2(io.MousePos.x + 16.0f, io.MousePos.y + 14.0f), IM_COL32(210, 255, 180, 255), text);
}

// ---- The template preview's parts ([added], EditSession::previewParts) ---------------------------------------------

namespace
{

const float kPartRadius = 9.0f;             // pixels round a part's mark that pick it
const float kPartAxis = 0.35f;              // metres of each axis drawn on a child
const float kExitFigure = 1.7f;             // metres of the figure standing on an exit point

// The part under the cursor within kPartRadius, the nearest; -1 for none.
int partUnder(const std::vector<EditSession::PreviewPart>& parts, const float viewProjection[16], const ImVec2& mouse)
{
    const ImGuiIO& io = ImGui::GetIO();
    int nearest = -1;
    float best = kPartRadius * kPartRadius;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const float p[3] = { parts[i].world.m[12], parts[i].world.m[13], parts[i].world.m[14] };
        ImVec2 at;
        if (!project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, at))
            continue;
        const float dx = at.x - mouse.x, dy = at.y - mouse.y;
        if (dx * dx + dy * dy <= best)
        {
            best = dx * dx + dy * dy;
            nearest = static_cast<int>(i);
        }
    }
    return nearest;
}

} // namespace

void ViewportTools::drawPreviewParts(EditSession& session, const float viewProjection[16])
{
    if (!session.previewPartsShown() || !session.templatePreview())
        return;
    const std::vector<EditSession::PreviewPart> parts = session.previewParts();
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const int picked = session.pickedPreviewPart();
    const int hovered = pointerOverUi() ? -1 : partUnder(parts, viewProjection, io.MousePos);
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const EditSession::PreviewPart& part = parts[i];
        const bool isPicked = static_cast<int>(i) == picked;
        const Mat4& world = isPicked && m_partDragging ? m_partPivot : part.world;
        const float p[3] = { world.m[12], world.m[13], world.m[14] };
        ImVec2 at;
        if (!project(viewProjection, p, io.DisplaySize.x, io.DisplaySize.y, at))
            continue;
        const int alpha = isPicked || static_cast<int>(i) == hovered ? 255 : 170;
        ImU32 colour = IM_COL32(235, 235, 235, alpha);
        if (part.kind == EditSession::PreviewPart::Child)
        {
            // Its axes, red X, green Y, blue Z, as the child's place turns them.
            const ImU32 axes[3] = { IM_COL32(230, 70, 70, alpha), IM_COL32(80, 210, 80, alpha), IM_COL32(80, 130, 240, alpha) };
            for (int k = 0; k < 3; ++k)
            {
                const float tip[3] = { p[0] + world.m[k * 4 + 0] * kPartAxis, p[1] + world.m[k * 4 + 1] * kPartAxis,
                                       p[2] + world.m[k * 4 + 2] * kPartAxis };
                ImVec2 end;
                if (project(viewProjection, tip, io.DisplaySize.x, io.DisplaySize.y, end))
                    list->AddLine(at, end, axes[k], 1.5f);
            }
            if (part.seat)
                colour = IM_COL32(255, 160, 40, alpha);
            else if (part.camera)
                colour = IM_COL32(80, 220, 255, alpha);
            list->AddCircleFilled(at, part.seat || part.camera ? 4.5f : 3.0f, colour, 12);
        }
        else
        {
            // A figure standing where a soldier leaving the seat is put.
            colour = part.kind == EditSession::PreviewPart::Exit ? IM_COL32(90, 230, 120, alpha) : IM_COL32(170, 240, 120, alpha);
            const float head[3] = { p[0], p[1] + kExitFigure, p[2] };
            ImVec2 top;
            if (project(viewProjection, head, io.DisplaySize.x, io.DisplaySize.y, top))
            {
                list->AddLine(at, top, colour, 2.0f);
                if (part.kind == EditSession::PreviewPart::Exit)
                    list->AddCircleFilled(top, 4.0f, colour, 12);
                else
                    list->AddCircle(top, 4.0f, colour, 12, 1.5f);
            }
            list->AddCircle(at, 4.0f, colour, 12, 1.5f);
        }
        if (isPicked)
            list->AddCircle(at, kPartRadius, IM_COL32(255, 230, 60, 255), 16, 2.0f);
        if (isPicked || static_cast<int>(i) == hovered || part.seat || part.camera || part.kind != EditSession::PreviewPart::Child)
        {
            const std::string text = part.kind == EditSession::PreviewPart::Child ? part.label : part.label + " (" + part.owner + ")";
            list->AddText(ImVec2(at.x + 7.0f, at.y - 15.0f), colour, text.c_str());
        }
    }
}

bool ViewportTools::handlePreviewParts(EditSession& session, const float view[16], const float projection[16],
                                       const float viewProjection[16])
{
    if (!session.previewPartsShown() || !session.templatePreview())
    {
        m_partDragging = false;
        return false;
    }
    const std::vector<EditSession::PreviewPart> parts = session.previewParts();
    int picked = session.pickedPreviewPart();
    if (picked >= static_cast<int>(parts.size()))
    {
        session.pickPreviewPart(-1);
        picked = -1;
    }
    const ImGuiIO& io = ImGui::GetIO();
    if (picked >= 0 && !m_partDragging && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !io.WantCaptureKeyboard)
    {
        session.pickPreviewPart(-1);
        return true;
    }
    bool overGizmo = false;
    if (picked >= 0)
    {
        // The gizmo's frame, as handleGizmo sets it (this handler runs instead of it).
        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::AllowAxisFlip(false);
        ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
        const EditSession::PreviewPart& part = parts[static_cast<size_t>(picked)];
        Mat4 pivot = m_partDragging ? m_partPivot : part.world;
        // A child moves or turns, as the gizmo mode says; an exit point only moves, in world axes.
        const bool child = part.kind == EditSession::PreviewPart::Child;
        const bool rotate = child && session.gizmoMode() == GizmoMode::Rotate;
        const ImGuizmo::MODE space = child && session.gizmoSpace() == GizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        float snap[3];
        for (int k = 0; k < 3; ++k)
            snap[k] = rotate ? m_rotateSnap : m_moveSnap;
        ImGuizmo::Enable(m_partDragging || !pointerOverUi());
        ImGuizmo::Manipulate(view, projection, rotate ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE, space, pivot.m, 0,
                             m_snap != io.KeyShift ? snap : 0);
        if (ImGuizmo::IsUsing())
        {
            m_partDragging = true;
            m_partPivot = pivot;
            return true;
        }
        if (m_partDragging)
        {
            // Let go: the template's line written, one undoable edit.
            m_partDragging = false;
            std::string result;
            const bool moved = session.movePreviewPart(static_cast<size_t>(picked), m_partPivot, result);
            logLine(moved ? LogLevel::Info : LogLevel::Warning, "part: %s", result.c_str());
            return true;
        }
        overGizmo = ImGuizmo::IsOver();
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !pointerOverUi() && !overGizmo)
    {
        const int under = partUnder(parts, viewProjection, io.MousePos);
        if (under >= 0)
        {
            session.pickPreviewPart(under);
            const EditSession::PreviewPart& part = parts[static_cast<size_t>(under)];
            logLine(LogLevel::Info, "part: picked %d, %s of %s", under,
                    (part.kind == EditSession::PreviewPart::Child ? part.child : part.label).c_str(), part.owner.c_str());
            return true;
        }
        // A click elsewhere lets the part go and is the view's again.
        session.pickPreviewPart(-1);
        return false;
    }
    return picked >= 0;
}

} // namespace bf2editor
