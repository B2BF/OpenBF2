// Code/Tools/BF2Editor/App/DescriptionDocuments.h

// The open mod's mod.desc and the open level's Info/<level>.desc as the editor edits them. They are
// loose files beside the mod and the level (the game reads Info/ loose); held in memory, changed through the
// history like the other documents, and planned by the level save (Framework/SavePlan). Editor-only code (D3).
#ifndef BF2EDITOR_APP_DESCRIPTIONDOCUMENTS_H
#define BF2EDITOR_APP_DESCRIPTIONDOCUMENTS_H

#include "Framework/EditorConfig.h"

#include <string>

namespace bf2editor
{

class SavePart;

class DescriptionDocuments
{
public:
    enum Which { Mod, Level };

    // From the open level's folder ("mods/<mod>/Levels/<level>/"): mods/<mod>/mod.desc and
    // Info/<level>.desc. Read once; a missing file stays missing (its fields cannot be set).
    void load(const std::string& levelDirectory);
    bool loaded() const { return m_loaded; }

    bool exists(Which which) const { return file(which).exists; }
    const std::string& path(Which which) const { return file(which).path; }
    const std::string& text(Which which) const { return file(which).text; }
    // The fields a page edits, in its order: "title desc url logo icon music version" / "name briefing music".
    static const char* const* fields(Which which, size_t& count);
    static bool validField(Which which, const std::string& field);

    bool value(Which which, const std::string& field, std::string& out) const;
    // The field set (an empty `value` with `remove` takes the element out: an undo of one that was added).
    bool apply(Which which, const std::string& field, const std::string& value, bool remove, std::string& error);

    bool modified() const;
    std::string modifiedList() const;           // "mods/bf2/mod.desc, ..."
    // The modified files into `part`, taken as saved once written; its answer "saved <paths>".
    void planSave(SavePart& part);

private:
    struct File
    {
        std::string path;
        std::string text;
        std::string savedText;
        bool        exists;
        File() : exists(false) {}
    };
    const File& file(Which which) const { return which == Mod ? m_mod : m_level; }
    File& file(Which which) { return which == Mod ? m_mod : m_level; }

    File m_mod;
    File m_level;
    bool m_loaded;

public:
    DescriptionDocuments() : m_loaded(false) {}
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_DESCRIPTIONDOCUMENTS_H
