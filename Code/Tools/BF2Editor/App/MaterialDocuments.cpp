// Code/Tools/BF2Editor/App/MaterialDocuments.cpp - see MaterialDocuments.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/MaterialDocuments.h"

#include "Framework/ObjectLibrary.h"
#include "Framework/SavePlan.h"
#include "Framework/SourceControl.h"

#include "IO/File/FileManager.h"
#include "IO/Stream/IStream.h"

namespace bf2editor
{

namespace
{

// The stock entries' own spelling (the plugin wrote MaterialManagerDefine.con; archive keys and Windows
// paths ignore case, census §4.1).
const char kDefinePath[] = "Common/Material/materialManagerDefine.con";
const char kCellPath[] = "Common/Material/materialManagerSettings.con";

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

MaterialDocuments::MaterialDocuments()
    : m_loaded(false)
{
}

void MaterialDocuments::clear()
{
    m_define = File();
    m_cells = File();
    m_defineFile.parse(std::string());
    m_cellFile.parse(std::string());
    m_loaded = false;
}

bool MaterialDocuments::read(File& file, const std::string& path, std::string& text)
{
    using namespace dice::hfe;
    file.path = path;
    // Archives first, as the engine resolves every read; the loose copy when the archives are skipped.
    const std::string resolved = io::fileManager->fileExistsEx(path, false);
    const std::string loose = io::fileManager->fileExistsEx(path, true);
    file.exists = !resolved.empty();
    file.archived = file.exists && resolved != loose;
    io::fileManager->getWritePath(path, file.writePath);
    text.clear();
    if (file.exists && readThroughEngine(path, text))
        file.savedText = text;
    return file.exists;
}

bool MaterialDocuments::load(std::string& error)
{
    if (m_loaded)
        return true;
    if (!dice::hfe::io::fileManager)
    {
        error = "the engine's file manager is not up";
        return false;
    }
    std::string defineText, cellText;
    const bool define = read(m_define, kDefinePath, defineText);
    const bool cells = read(m_cells, kCellPath, cellText);
    if (!define && !cells)
    {
        error = std::string("the mod has neither ") + kDefinePath + " nor " + kCellPath;
        return false;
    }
    m_defineFile.parse(defineText);
    m_cellFile.parse(cellText);
    m_loaded = true;
    return true;
}

bool MaterialDocuments::modified() const
{
    return m_loaded && (m_defineFile.render() != m_define.savedText || m_cellFile.render() != m_cells.savedText);
}

bool MaterialDocuments::planSave(SavePart& part)
{
    if (!modified())
        return true;
    if (archived())
    {
        part.refuse("the material files are read from the mounted Common archives, so a save would be shadowed: not "
                    "saved (EditorMaterial.extract, then start with +looseCommon 1)");
        return false;
    }
    std::string written;
    const std::string defineText = m_defineFile.render();
    const std::string cellText = m_cellFile.render();
    if (defineText != m_define.savedText)
    {
        part.add(m_define.writePath, defineText);
        part.onWritten([this, defineText]() { m_define.savedText = defineText; });
        written = m_define.writePath;
    }
    if (cellText != m_cells.savedText)
    {
        part.add(m_cells.writePath, cellText);
        part.onWritten([this, cellText]() { m_cells.savedText = cellText; });
        written += (written.empty() ? "" : ", ") + m_cells.writePath;
    }
    part.onReport([written](const SavePart&) { return "saved the material files " + written; });
    return true;
}

} // namespace bf2editor
