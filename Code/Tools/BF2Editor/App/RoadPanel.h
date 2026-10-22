// Code/Tools/BF2Editor/App/RoadPanel.h

// The Roads panel: the level's editable roads (App/RoadNetwork) - the view's road tool and spline display, the
// splines (snap bits, ribbon width, step, every point with its trigger properties), the roads (template, spline, edge
// noise, texture offset, priority, edge fades, ghost or final) and the road templates (width, error, blend, priority,
// material, fade-off, both texture layers) - each change one undoable EditorRoad edit; and the roads' save. The retail
// editor split these over its LevelEditor road dialog and EditorEngine's spline tools. Editor-only code (D3).
#ifndef BF2EDITOR_APP_ROADPANEL_H
#define BF2EDITOR_APP_ROADPANEL_H

#include "Framework/EditorConfig.h"

#include <string>

namespace bf2editor
{

class EditSession;

class RoadPanel
{
public:
    void draw(EditSession& session, bool* open);

private:
    void drawSplines(EditSession& session);
    void drawRoads(EditSession& session);
    void drawTemplates(EditSession& session);

    std::string m_selectedRoad;
    std::string m_selectedTemplate;
    std::string m_newRoadTemplate;      // the template a new road is made with
    char        m_newRoadName[64] = "";
    char        m_newTemplateName[64] = "";
    char        m_renameBuffer[64] = "";
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_ROADPANEL_H
