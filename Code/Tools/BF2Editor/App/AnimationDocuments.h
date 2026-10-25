// Code/Tools/BF2Editor/App/AnimationDocuments.h

// The animation systems the editor has opened: each `.inc` read through the engine's file manager -
// what the engine loads, from a mounted archive or a loose file - and kept as a Framework/AnimationSystemFile,
// edited in place. A system read from a mounted archive is read-only (a loose copy would stay shadowed), as the
// object editor's archived files are; `+looseObjects 1` after EditorObjects.extract edits them. The level save
// plans the modified ones (Framework/SavePlan). Editor-only code (D3).
#ifndef BF2EDITOR_APP_ANIMATIONDOCUMENTS_H
#define BF2EDITOR_APP_ANIMATIONDOCUMENTS_H

#include "Framework/EditorConfig.h"

#include "Framework/AnimationSystemFile.h"

#include <string>
#include <vector>

namespace bf2editor
{

class SavePart;

struct AnimationDocument
{
    std::string         path;           // the engine's name ("Objects/Weapons/Handheld/at_mine/AnimationSystem1p.inc")
    std::string         writePath;      // where a save writes it (the mod's own folder)
    bool                archived;       // read from a mounted archive: read-only
    std::string         savedText;
    AnimationSystemFile file;
    bool modified() const { return file.render() != savedText; }
};

class AnimationDocuments
{
public:
    // A system opened (read on first use); its index to `index`. False with `error` when the engine has no
    // such file.
    bool open(const std::string& path, size_t& index, std::string& error);
    size_t count() const { return m_documents.size(); }
    AnimationDocument& document(size_t index) { return m_documents[index]; }
    const AnimationDocument& document(size_t index) const { return m_documents[index]; }
    int find(const std::string& path) const;
    std::vector<size_t> modified() const;
    // The modified ones into `part`, taken as saved once written; its answer "saved the animation systems <paths>".
    // The part is refused (false) when one of them is archived.
    bool planSave(SavePart& part);
    void clear() { m_documents.clear(); }

private:
    std::vector<AnimationDocument> m_documents;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_ANIMATIONDOCUMENTS_H
