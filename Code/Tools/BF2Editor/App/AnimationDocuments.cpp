// Code/Tools/BF2Editor/App/AnimationDocuments.cpp - see AnimationDocuments.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/AnimationDocuments.h"

#include "Framework/ObjectLibrary.h"
#include "Framework/SavePlan.h"
#include "Framework/SourceControl.h"

#include "IO/File/FileManager.h"
#include "IO/Stream/IStream.h"

namespace bf2editor
{

namespace
{

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

} // namespace

int AnimationDocuments::find(const std::string& path) const
{
    for (size_t i = 0; i < m_documents.size(); ++i)
        if (_stricmp(m_documents[i].path.c_str(), path.c_str()) == 0)
            return static_cast<int>(i);
    return -1;
}

bool AnimationDocuments::open(const std::string& path, size_t& index, std::string& error)
{
    const int known = find(path);
    if (known >= 0)
    {
        index = static_cast<size_t>(known);
        return true;
    }
    using namespace dice::hfe;
    if (!io::fileManager)
    {
        error = "the engine's file manager is not up";
        return false;
    }
    if (path.size() < 5 || _stricmp(path.c_str() + path.size() - 4, ".inc") != 0)
    {
        error = "an animation system is an .inc file";
        return false;
    }
    const std::string resolved = io::fileManager->fileExistsEx(path, false);
    const std::string loose = io::fileManager->fileExistsEx(path, true);
    std::string text;
    if (resolved.empty() || !readThroughEngine(path, text))
    {
        error = "the engine finds no " + path;
        return false;
    }
    AnimationDocument document;
    document.path = path;
    document.archived = resolved != loose;
    io::fileManager->getWritePath(path, document.writePath);
    document.savedText = text;
    document.file.parse(text);
    m_documents.push_back(document);
    index = m_documents.size() - 1;
    return true;
}

std::vector<size_t> AnimationDocuments::modified() const
{
    std::vector<size_t> out;
    for (size_t i = 0; i < m_documents.size(); ++i)
        if (m_documents[i].modified())
            out.push_back(i);
    return out;
}

bool AnimationDocuments::planSave(SavePart& part)
{
    std::string written;
    for (size_t i = 0; i < m_documents.size(); ++i)
    {
        AnimationDocument& d = m_documents[i];
        if (!d.modified())
            continue;
        if (d.archived)
        {
            part.refuse(d.path + " is read from a mounted archive, so a save would be shadowed: not saved "
                        "(EditorObjects.extract, then start with +looseObjects 1)");
            return false;
        }
        const std::string text = d.file.render();
        part.add(d.writePath, text);
        part.onWritten([this, i, text]() { m_documents[i].savedText = text; });
        written += (written.empty() ? "" : ", ") + d.path;
    }
    if (!written.empty())
        part.onReport([written](const SavePart&) { return "saved the animation systems " + written; });
    return true;
}

} // namespace bf2editor
