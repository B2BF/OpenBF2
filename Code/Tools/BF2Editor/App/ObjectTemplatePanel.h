// Edits a selected or named template's properties, children and components through undoable console actions.
// Repeated properties use property#index. Mounted archive documents are read-only.
// Also exposes object-archive extraction and packing for loose-object editing.
#ifndef BF2EDITOR_APP_OBJECTTEMPLATEPANEL_H
#define BF2EDITOR_APP_OBJECTTEMPLATEPANEL_H

#include "Framework/EditorConfig.h"

#include <string>
#include <vector>

namespace bf2editor
{

class EditSession;

class ObjectTemplatePanel
{
public:
    ObjectTemplatePanel();

    void draw(EditSession& session, bool* open);

private:
    bool run(const std::string& line, const char* what);

    char        m_template[128];
    std::string m_shown;            // the template whose files are listed
    char        m_newProperty[96];
    char        m_newValue[256];
    char        m_packFolder[260];
    // The value being typed into a line's field: which line (file and index) and its text.
    int         m_editFile;
    int         m_editLine;
    char        m_editValue[256];
    // A child added (its template and position), and the child whose position or rotation is being typed:
    // its row, which field (0 the position, 1 the rotation) and the three numbers.
    char        m_newChild[128];
    float       m_newChildPosition[3];
    float       m_turnAngle = 90.0f;           // a child's turn about the parent
    char        m_armorEffect[128] = {};        // the effect an armor effect line names (O2)
    bool        m_fireModes[3] = { true, false, false };    // single, burst, full (O2)
    char        m_seatChild[128] = {};
    char        m_geometryProperty[96] = {};    // O1
    char        m_geometryValue[96] = {};
    int         m_childRow;
    int         m_childField;
    float       m_childValue[3];
    std::vector<std::string> m_componentClasses;    // EditSession::componentClasses, asked once
    int         m_componentChoice;
    // New templates, clones and new children: the class lists (asked once), the choices and the names typed.
    std::vector<std::string> m_newClasses;
    std::vector<std::string> m_childClasses;
    int         m_newClassChoice;
    int         m_childClassChoice;
    int         m_retypeChoice;
    char        m_newName[128];
    char        m_newFolder[260];
    char        m_cloneName[128];
    char        m_newChildName[128];
    float       m_newChildAt[3];
    bool        m_confirmExtract;
    std::string m_archives;         // EditorObjects.status, asked for when the panel opens and on Refresh
    std::string m_message;
    bool        m_messageFailed;
    // The weapon test's tuning fields: the engine's values (asked again twice a second) and what is being typed.
    std::string m_tuningTemplate;
    std::string m_tuningValue[4];
    char        m_tuningEdit[4][32] = {};
    int         m_tuningFrame = -1000;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_OBJECTTEMPLATEPANEL_H
