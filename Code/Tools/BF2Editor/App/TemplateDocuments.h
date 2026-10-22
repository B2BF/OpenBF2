// Loads a template's defining .con and matching .tweak through the engine filesystem for in-place editing.
// Mounted archive files are read-only because archives shadow loose files; loose mode leaves archives unmounted.
// A save plans each modified file at getWritePath (Framework/SavePlan writes it through source control, atomically,
// and verifies it).
#ifndef BF2EDITOR_APP_TEMPLATEDOCUMENTS_H
#define BF2EDITOR_APP_TEMPLATEDOCUMENTS_H

#include "Framework/EditorConfig.h"

#include "Framework/TemplateFile.h"

#include <string>
#include <vector>

namespace bf2editor
{

class SavePart;

struct TemplateDocument
{
    std::string  path;          // the engine's name for it ("objects/vehicles/land/x/x.tweak")
    std::string  writePath;     // where a save writes it (the mod's own folder)
    bool         exists;        // the engine found it (a template may have no .tweak)
    bool         archived;      // read from a mounted archive: read-only
    std::string  savedText;     // the file as read or last saved
    TemplateFile file;
    bool modified() const { return file.render() != savedText; }
};

class TemplateDocuments
{
public:
    // The files of a template (its defining file first, then its .tweak), read on first use; their
    // indexes go to `files`. False with `error` when the engine has no such template or no file for it.
    bool filesOf(const std::string& templateName, std::vector<size_t>& files, std::string& error);
    // The file an edit of a template's property goes to: the one whose block of the template holds a line
    // of the property (its .tweak before its defining file), else its .tweak when there is one, else its
    // defining file - the caller adds a block of the template when the file has none. -1 with `error` when
    // that file is read-only or missing.
    int editTarget(const std::string& templateName, const std::string& property, std::string& error);

    size_t count() const { return m_documents.size(); }
    TemplateDocument& document(size_t index) { return m_documents[index]; }
    const TemplateDocument& document(size_t index) const { return m_documents[index]; }
    std::vector<size_t> modified() const;
    // The file, when it changed, into `part`; the document takes it as saved once written. False with `error` for
    // a file read from a mounted archive.
    bool planSave(size_t index, SavePart& part, std::string& error);
    // Everything forgotten (the level closes; a relaunch reads the files again).
    void clear() { m_documents.clear(); }

private:
    int find(const std::string& path) const;
    size_t open(const std::string& path);

    std::vector<TemplateDocument> m_documents;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_TEMPLATEDOCUMENTS_H
