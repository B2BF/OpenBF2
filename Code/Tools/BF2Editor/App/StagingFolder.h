// Staging folders of the editor's own: the renderer writes what the editor compiles or bakes (compiled roads, object
// lightmaps and their atlas) into one of these, never into the level, and the editor copies the files in. A staging
// folder lives under the game folder - <game>/bf2editor_temp/<kind>_<process id> - so the engine's file manager and the
// renderer can name it by an absolute path. Editor-only code (D3).
#ifndef BF2EDITOR_APP_STAGINGFOLDER_H
#define BF2EDITOR_APP_STAGINGFOLDER_H

#include "Framework/EditorConfig.h"

#include <string>
#include <vector>

namespace bf2editor
{

// Makes the staging folder for `kind` ("roads", "lightmaps") empty and answers its absolute path with forward slashes.
// False with `error` when the game folder's path holds a dot: the renderer cuts texture names at their last dot.
bool makeStagingFolder(const char* kind, std::string& root, std::string& error);

// Deletes a folder and everything in it (the editor's own staging only).
void removeFolderTree(const std::string& path);

// Every file under root/relative, as paths relative to root with forward slashes.
void listFolderFiles(const std::string& root, const std::string& relative, std::vector<std::string>& out);

// Puts a staging folder in front of the engine's file paths for the lifetime of the object - a relative write lands in
// it, a read finds its files first, a listing covers it - and names it in the Editor_LevelSavePath setting the
// renderer's level writers prefix their paths with. Both are restored on destruction.
class StagingRedirect
{
public:
    explicit StagingRedirect(const std::string& root);
    ~StagingRedirect();

private:
    StagingRedirect(const StagingRedirect&);
    StagingRedirect& operator=(const StagingRedirect&);

    std::string m_path;
    std::string m_previous;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_STAGINGFOLDER_H
