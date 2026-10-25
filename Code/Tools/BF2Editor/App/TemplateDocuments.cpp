// Code/Tools/BF2Editor/App/TemplateDocuments.cpp - see TemplateDocuments.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/TemplateDocuments.h"

#include "Framework/ObjectLibrary.h"
#include "Framework/SavePlan.h"
#include "Framework/SourceControl.h"

#include "Game/Objects/IObjectTemplate.h"
#include "IO/File/FileManager.h"
#include "IO/Stream/IStream.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <cstring>

namespace bf2editor
{

namespace
{

// The file as the engine reads it: from a mounted archive or a loose file, whichever it resolves.
bool readThroughEngine(const std::string& path, std::string& text)
{
    using namespace dice::hfe;
    io::IStream* stream = io::fileManager->open(path, io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0);
    if (!stream)
        return false;
    char buffer[65536];
    for (;;)
    {
        const unsigned int got = stream->read(buffer, sizeof(buffer));
        if (got == 0)
            break;
        text.append(buffer, got);
    }
    stream->release();
    return true;
}

bool endsWith(const std::string& text, const char* tail)
{
    const size_t n = std::strlen(tail);
    return text.size() >= n && _stricmp(text.c_str() + text.size() - n, tail) == 0;
}

// <dir>/<file name without its extension>.tweak, as the retail writer names a root's tweak file.
std::string tweakBeside(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    const size_t dot = path.rfind('.');
    const std::string stem = dot != std::string::npos && (slash == std::string::npos || dot > slash) ? path.substr(0, dot) : path;
    return stem + ".tweak";
}

} // namespace

int TemplateDocuments::find(const std::string& path) const
{
    for (size_t i = 0; i < m_documents.size(); ++i)
        if (_stricmp(m_documents[i].path.c_str(), path.c_str()) == 0)
            return static_cast<int>(i);
    return -1;
}

size_t TemplateDocuments::open(const std::string& path)
{
    const int known = find(path);
    if (known >= 0)
        return static_cast<size_t>(known);
    using namespace dice::hfe;
    TemplateDocument document;
    document.path = path;
    // Archives first, as the engine resolves every read (FileManager::fileExistsEx); the loose copy
    // alone when the archives are skipped. An archive that holds the file shadows any loose copy.
    const std::string resolved = io::fileManager->fileExistsEx(path, false);
    const std::string loose = io::fileManager->fileExistsEx(path, true);
    document.exists = !resolved.empty();
    document.archived = document.exists && resolved != loose;
    io::fileManager->getWritePath(path, document.writePath);
    if (document.exists && readThroughEngine(path, document.savedText))
        document.file.parse(document.savedText);
    else
        document.file.parse(std::string());
    m_documents.push_back(document);
    return m_documents.size() - 1;
}

bool TemplateDocuments::filesOf(const std::string& templateName, std::vector<size_t>& files, std::string& error)
{
    using namespace dice::hfe;
    files.clear();
    world::IObjectTemplate* objectTemplate =
        world::objectTemplateManager ? world::objectTemplateManager->getTemplate(templateName) : 0;
    if (!objectTemplate)
    {
        error = "no template named " + templateName;
        return false;
    }
    const std::string working = world::objectTemplateManager->getWorkingFolderAndFile(objectTemplate->getName());
    if (working.empty())
    {
        error = "the engine keeps no file for " + objectTemplate->getName()
              + " (made by a script or by the engine itself, not by a template file)";
        return false;
    }
    files.push_back(open(working));
    if (!endsWith(working, ".tweak"))
        files.push_back(open(tweakBeside(working)));
    return true;
}

int TemplateDocuments::editTarget(const std::string& templateName, const std::string& property, std::string& error)
{
    std::vector<size_t> files;
    if (!filesOf(templateName, files, error))
        return -1;
    // The file that holds the line, the .tweak first: its lines run after the .con's (the .con includes
    // it). A new line goes to the .tweak when there is one, as the retail editor's saves put every
    // setting there; else to the defining file itself.
    size_t target = files[0];
    for (size_t i = 0; i < files.size(); ++i)
        if (endsWith(m_documents[files[i]].path, ".tweak") && m_documents[files[i]].exists)
            target = files[i];
    bool held = false;
    for (size_t pass = 0; pass < 2 && !held; ++pass)
    {
        for (size_t i = 0; i < files.size() && !held; ++i)
        {
            const TemplateDocument& candidate = m_documents[files[i]];
            if (!candidate.exists || endsWith(candidate.path, ".tweak") != (pass == 0))
                continue;
            const std::vector<TemplatePropertyLine> lines = candidate.file.properties(templateName);
            for (size_t k = 0; k < lines.size() && !held; ++k)
                if (_stricmp(lines[k].property.c_str(), property.c_str()) == 0)
                {
                    target = files[i];
                    held = true;
                }
        }
    }
    TemplateDocument& document = m_documents[target];
    if (!document.exists)
    {
        error = document.path + " is not there";
        return -1;
    }
    if (document.archived)
    {
        error = document.path + " is read from a mounted archive, so an edit could not be saved (the archive shadows "
                "any loose copy): extract the object archives (EditorObjects.extract) and start the editor with "
                "+looseObjects 1 to edit templates";
        return -1;
    }
    return static_cast<int>(target);
}

std::vector<size_t> TemplateDocuments::modified() const
{
    std::vector<size_t> list;
    for (size_t i = 0; i < m_documents.size(); ++i)
        if (m_documents[i].modified())
            list.push_back(i);
    return list;
}

bool TemplateDocuments::planSave(size_t index, SavePart& part, std::string& error)
{
    TemplateDocument& document = m_documents[index];
    const std::string text = document.file.render();
    if (text == document.savedText)
        return true;
    if (document.archived)
    {
        error = document.path + " is read from a mounted archive: not saved";
        return false;
    }
    part.add(document.writePath, text);
    part.onWritten([this, index, text]() { m_documents[index].savedText = text; });
    return true;
}

} // namespace bf2editor
