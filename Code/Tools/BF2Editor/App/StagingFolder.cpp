// Code/Tools/BF2Editor/App/StagingFolder.cpp - see StagingFolder.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/StagingFolder.h"

#include "Core/SettingsRepostitory.h"
#include "IO/File/FileManager.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>

namespace bf2editor
{

namespace
{

const char kStagingParent[] = "bf2editor_temp";
const char kSavePathKey[] = "Editor_LevelSavePath";

} // namespace

bool makeStagingFolder(const char* kind, std::string& root, std::string& error)
{
    char directory[MAX_PATH] = "";
    GetCurrentDirectoryA(MAX_PATH, directory);
    root = directory;
    std::replace(root.begin(), root.end(), '\\', '/');
    if (root.find('.') != std::string::npos)
    {
        error = "the game folder's path (" + root + ") holds a dot: the renderer cuts texture names at a dot, so it "
                "cannot write into a staging folder there";
        return false;
    }
    char stamp[96];
    std::snprintf(stamp, sizeof(stamp), "/%s/%s_%lu", kStagingParent, kind, static_cast<unsigned long>(GetCurrentProcessId()));
    root += stamp;
    removeFolderTree(root);
    CreateDirectoryA((root.substr(0, root.find_last_of('/'))).c_str(), 0);
    if (!CreateDirectoryA(root.c_str(), 0) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        error = "could not make the staging folder " + root;
        return false;
    }
    return true;
}

void removeFolderTree(const std::string& path)
{
    WIN32_FIND_DATAA found;
    const HANDLE search = FindFirstFileA((path + "/*").c_str(), &found);
    if (search != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string name = found.cFileName;
            if (name == "." || name == "..")
                continue;
            const std::string child = path + "/" + name;
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                removeFolderTree(child);
            else
            {
                SetFileAttributesA(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(child.c_str());
            }
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    RemoveDirectoryA(path.c_str());
}

void listFolderFiles(const std::string& root, const std::string& relative, std::vector<std::string>& out)
{
    WIN32_FIND_DATAA found;
    const std::string pattern = root + "/" + (relative.empty() ? std::string() : relative + "/") + "*";
    const HANDLE search = FindFirstFileA(pattern.c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const std::string name = found.cFileName;
        if (name == "." || name == "..")
            continue;
        const std::string path = relative.empty() ? name : relative + "/" + name;
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            listFolderFiles(root, path, out);
        else
            out.push_back(path);
    } while (FindNextFileA(search, &found));
    FindClose(search);
}

StagingRedirect::StagingRedirect(const std::string& root)
    : m_path(root + "/")
    , m_previous(dice::hfe::settings->stringGet(kSavePathKey, std::string()))
{
    dice::hfe::settings->stringSet(kSavePathKey, root);
    dice::hfe::io::fileManager->addPath(m_path);
}

StagingRedirect::~StagingRedirect()
{
    dice::hfe::io::fileManager->removePath(m_path);
    dice::hfe::settings->stringSet(kSavePathKey, m_previous);
}

} // namespace bf2editor
