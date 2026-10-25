// Viewport selection, transform gizmos and level-editing tools.
// Drags and brush strokes preview live and commit one undoable edit on release.
// Picking is suppressed while a painting tool owns the gesture. Camera movement uses FreeCamera.
// Overlays use the current render-view matrices and ImGui's background list, beneath the panels.
// Editor-only code.
#ifndef BF2EDITOR_APP_VIEWPORTTOOLS_H
#define BF2EDITOR_APP_VIEWPORTTOOLS_H

#include "Framework/EditorConfig.h"

#include "Math/Mat4.h"

#include <string>
#include <utility>
#include <vector>

namespace dice { namespace hfe { namespace dx9rend { class IRenderView; } } }

struct ImVec2;

namespace bf2editor
{

class EditSession;
class FreeCamera;
class ResourceBrowser;

class ViewportTools
{
public:
    ViewportTools();

    // Call between ImGuiHost::beginFrame and endFrame, after FreeCamera::apply wrote this frame's
    // view. `viewPos`/`viewSize` is the dockspace's central node (where the toolbar goes).
    void draw(EditSession& session, const FreeCamera& camera, dice::hfe::dx9rend::IRenderView* view,
              const ImVec2& viewPos, const ImVec2& viewSize);

private:
    void drawToolbar(EditSession& session, const ImVec2& viewPos);
    void drawOutlines(EditSession& session, const float viewProjection[16]);
    // The primary object's anchor (where its template stands on the ground), when it has one.
    void drawAnchor(EditSession& session, const float viewProjection[16]);
    void drawMarkers(EditSession& session, const float viewProjection[16]);
    void drawCombatAreas(EditSession& session, const float viewProjection[16]);
    void drawSounds(EditSession& session, const float viewProjection[16]);
    void drawCompareLayer(EditSession& session, const float viewProjection[16]);
    // The view overlays: the control point connections and
    // radii, the sea-level plane, the selection's bounding spheres and the light source markers.
    void drawOverlays(EditSession& session, const float viewProjection[16]);
    // The measure tool: its clicks and keys, and the line through its points.
    void handleMeasure(EditSession& session);
    void drawMeasure(EditSession& session, const float viewProjection[16]);
    // A closed loop of (x, z) points along the terrain, a metre above it (8 m samples, fading with distance).
    void drawGroundLoop(EditSession& session, const std::vector<std::pair<float, float> >& points,
                        const float viewProjection[16], unsigned colour, int alpha, float thickness, bool dashed);
    // A press on an ambient sound's marker and its drag; true when they took the mouse this frame.
    bool handleSounds(EditSession& session, const float viewProjection[16]);
    // The selected combat area's point handles; true when they took the mouse this frame.
    bool handleCombatAreas(EditSession& session, const float viewProjection[16]);
    bool areaPointOnScreen(EditSession& session, const std::pair<float, float>& point, const float viewProjection[16],
                           ImVec2& out) const;
    void handleGizmo(EditSession& session, const float view[16], const float projection[16],
                     const float viewProjection[16]);
    // The move gizmo in anchor mode (EditorGizmo.anchorMode): it sits on the primary object's anchor, in
    // world axes, and a drag moves the anchor - EditorSelection.setAnchor when the button comes up.
    void handleAnchorGizmo(EditSession& session, const float view[16], const float projection[16], unsigned id,
                           const dice::hfe::Mat4& transform);
    void beginDrag(EditSession& session, const dice::hfe::Mat4& pivotStart, bool ground);
    void previewDrag(EditSession& session);
    // A double click: the Selection panel's numeric fields, focused.
    void openSelectionPanel(EditSession& session);
    void handleClicks(EditSession& session);
    void handleDrop(EditSession& session, const float viewProjection[16]);
    void handleShortcuts(EditSession& session, const FreeCamera& camera);
    void finishDrag(EditSession& session);
    // The terrain brush: the wheel, a stroke's press, ticks and release, and the cursor.
    void handleTerrain(EditSession& session);
    void drawTerrainBrush(EditSession& session, const float viewProjection[16]);
    // The object paint brush, the same way, and its cursor.
    void handleBrush(EditSession& session);
    void drawBrushCursor(EditSession& session, const float viewProjection[16]);
    // The undergrowth tool: a stroke's press, ticks and release, the wheel, and a cursor that marks the
    // density map's cells around it by material (the renderer shows the new undergrowth only after a
    // reload).
    void handleUndergrowth(EditSession& session);
    void drawUndergrowthCursor(EditSession& session, const float viewProjection[16]);
    // A bundle armed for placing: the click that places it, Esc, and where it would go.
    void handleBundlePlacement(EditSession& session, const float viewProjection[16]);
    // The level's splines (EditorRoad.showSplines): each curve as the renderer's spline evaluates it (Catmull-Rom
    // through every point), its points - the active spline's larger, its selected point green - and a label naming its
    // road; a point being dragged is drawn where it would go.
    void drawSplines(EditSession& session, const float viewProjection[16]);
    // The road tool's mouse (EditSession::setRoadTool): true when it took the mouse this frame.
    bool handleSplines(EditSession& session, const float viewProjection[16]);
    // The template preview's parts (EditSession::previewParts, [added]): a mark and axes for each child, a figure
    // for each seat's exit point, names for seats, cameras, exits and the picked part. A click on a mark picks it;
    // the picked part gets the gizmo (a move, or a turn in the rotate mode, for a child), and letting go writes it
    // (EditSession::movePreviewPart). True when the parts took the mouse this frame.
    void drawPreviewParts(EditSession& session, const float viewProjection[16]);
    bool handlePreviewParts(EditSession& session, const float view[16], const float projection[16],
                            const float viewProjection[16]);
    bool                  m_partDragging = false;
    dice::hfe::Mat4       m_partPivot;      // the picked part's gizmo matrix while dragging

public:
    // The Resources panel, told which templates were placed from it (its recent list).
    void setResources(ResourceBrowser* resources) { m_resources = resources; }

    // The snap settings, for the editor's settings file.
    bool  snap() const { return m_snap; }
    float moveSnap() const { return m_moveSnap; }
    float rotateSnap() const { return m_rotateSnap; }
    void  setSnap(bool snap, float moveSnap, float rotateSnap);

private:
    // A drag moves the whole selection rigidly with the gizmo: the primary object is the pivot, and
    // every selected object keeps its placement relative to it (start * inverse(pivotStart) * pivot).
    struct DragItem
    {
        unsigned        id;
        dice::hfe::Mat4 start;
    };

    bool                  m_gizmoShown;     // this frame drew a gizmo (its hover state is current)
    bool                  m_dragging;
    bool                  m_groundDrag;     // the drag slides over the surface (the move gizmo's centre)
    bool                  m_dragDouble;     // the press that began the drag was a double click
    const FreeCamera*     m_camera;         // this frame's camera (draw())
    ResourceBrowser*      m_resources;
    bool                  m_groundStartValid;
    float                 m_groundStart[3]; // where the cursor met the pivot's plane when the slide began
    bool                  m_groundOffsetValid;
    float                 m_groundOffset;   // the terrain height under the pivot when the slide began
    unsigned              m_dragId;         // the pivot: the primary selection when the drag began
    int                   m_dragMode;       // the GizmoMode the drag started in
    dice::hfe::Mat4       m_pivotStart;
    dice::hfe::Mat4       m_pivot;          // the gizmo's matrix while dragging
    std::vector<DragItem> m_dragItems;
    bool                  m_snap;
    float                 m_moveSnap;       // metres
    float                 m_rotateSnap;     // degrees

    // A left press in the view that no gizmo took: a click when it comes up where it went down, a box
    // when it moved.
    bool                  m_pressed;
    bool                  m_pressDouble;
    // A combat area point dragged: which, whether it moved, where it would go.
    bool                  m_areaDrag;
    int                   m_areaPoint;
    bool                  m_areaMoved;
    bool                  m_areaTargetValid;
    float                 m_areaTarget[3];
    float                 m_areaPress[2];
    // An ambient sound dragged: which, whether it moved, where it would go, its height above the ground.
    bool                  m_soundDrag;
    bool                  m_soundMoved;
    bool                  m_soundTargetValid;
    std::string           m_soundName;
    float                 m_soundTarget[3];
    float                 m_soundPress[2];
    float                 m_soundHeight;
    bool                  m_boxing;
    float                 m_pressX;
    float                 m_pressY;
    // A terrain stroke under way: the button that holds it, and whether it is the other action.
    bool                  m_terrainStroke;
    int                   m_terrainButton;
    bool                  m_terrainSecondary;
    std::string           m_terrainError;   // the last tick's refusal, logged once
    // An anchor dragged in anchor mode, and the gizmo's matrix while it is.
    bool                  m_anchorDragging;
    dice::hfe::Mat4       m_anchorPivot;
    // An undergrowth stroke under way: the button that holds it.
    bool                  m_undergrowthStroke;
    int                   m_undergrowthButton;
    // A spline point dragged by the road tool: which, whether it moved, where it would go.
    bool                  m_splineDrag = false;
    bool                  m_splineMoved = false;
    int                   m_splineDragId = -1;
    int                   m_splineDragPoint = -1;
    bool                  m_splineTargetValid = false;
    float                 m_splineTarget[3] = { 0.0f, 0.0f, 0.0f };
    float                 m_splinePress[2] = { 0.0f, 0.0f };
    // An object brush stroke under way: the button that holds it.
    bool                  m_brushStroke;
    int                   m_brushButton;
    std::string           m_brushError;     // the last tick's refusal, logged once
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_VIEWPORTTOOLS_H
