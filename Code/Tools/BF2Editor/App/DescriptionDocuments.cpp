// Code/Tools/BF2Editor/App/DescriptionDocuments.cpp - see DescriptionDocuments.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/DescriptionDocuments.h"

#include "Framework/DescFile.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SavePlan.h"
#include "Framework/SourceControl.h"

#include <windows.h>

#include <fstream>
#include <sstream>

namespace bf2editor
{

namespace
{

bool readWhole(const std::string& path, std::string& text)
{
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    if (!in)
        return false;
    std::ostringstream bytes;
    bytes << in.rdbuf();
    text = bytes.str();
    return true;
}

const char* const kModFields[] = { "title", "desc", "url", "logo", "icon", "music", "version" };
const char* const kLevelFields[] = { "name", "briefing", "music" };

} // namespace

void DescriptionDocuments::load(const std::string& levelDirectory)
{
    if (m_loaded)
        return;
    m_loaded = true;
    // "mods/<mod>/Levels/<level>/": the level's name, and the mod's folder two levels up.
    std::string level = levelDirectory;
    while (!level.empty() && (level[level.size() - 1] == '/' || level[level.size() - 1] == '\\'))
        level.erase(level.size() - 1);
    const size_t slash = level.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? level : level.substr(slash + 1);
    const std::string levels = slash == std::string::npos ? std::string() : level.substr(0, slash);
    const size_t modSlash = levels.find_last_of("/\\");
    const std::string mod = modSlash == std::string::npos ? std::string() : levels.substr(0, modSlash);
    m_mod.path = (mod.empty() ? std::string() : mod + "/") + "mod.desc";
    m_level.path = level + "/Info/" + name + ".desc";
    m_mod.exists = readWhole(m_mod.path, m_mod.text);
    m_level.exists = readWhole(m_level.path, m_level.text);
    m_mod.savedText = m_mod.text;
    m_level.savedText = m_level.text;
}

const char* const* DescriptionDocuments::fields(Which which, size_t& count)
{
    if (which == Mod)
    {
        count = sizeof(kModFields) / sizeof(kModFields[0]);
        return kModFields;
    }
    count = sizeof(kLevelFields) / sizeof(kLevelFields[0]);
    return kLevelFields;
}

bool DescriptionDocuments::validField(Which which, const std::string& field)
{
    size_t count = 0;
    const char* const* names = fields(which, count);
    for (size_t i = 0; i < count; ++i)
        if (field == names[i])
            return true;
    return false;
}

bool DescriptionDocuments::value(Which which, const std::string& field, std::string& out) const
{
    const File& f = file(which);
    return f.exists && descText(f.text, field, out);
}

bool DescriptionDocuments::apply(Which which, const std::string& field, const std::string& value, bool remove,
                                 std::string& error)
{
    File& f = file(which);
    if (!f.exists)
    {
        error = f.path + " is not there";
        return false;
    }
    if (!validField(which, field))
    {
        error = "'" + field + "' is not a field of " + f.path;
        return false;
    }
    if (remove)
    {
        removeDescElement(f.text, field);
        return true;
    }
    if (!setDescText(f.text, which == Mod ? "mod" : "map", field, value))
    {
        error = "the " + field + " of " + f.path + " cannot be set in place (an empty tag, or no root element)";
        return false;
    }
    return true;
}

bool DescriptionDocuments::modified() const
{
    return (m_mod.exists && m_mod.text != m_mod.savedText) || (m_level.exists && m_level.text != m_level.savedText);
}

std::string DescriptionDocuments::modifiedList() const
{
    std::string list;
    if (m_mod.exists && m_mod.text != m_mod.savedText)
        list = m_mod.path;
    if (m_level.exists && m_level.text != m_level.savedText)
        list += (list.empty() ? "" : ", ") + m_level.path;
    return list;
}

void DescriptionDocuments::planSave(SavePart& part)
{
    const std::string list = modifiedList();
    if (list.empty())
        return;
    if (m_mod.exists && m_mod.text != m_mod.savedText)
    {
        const std::string text = m_mod.text;
        part.add(m_mod.path, text);
        part.onWritten([this, text]() { m_mod.savedText = text; });
    }
    if (m_level.exists && m_level.text != m_level.savedText)
    {
        const std::string text = m_level.text;
        part.add(m_level.path, text);
        part.onWritten([this, text]() { m_level.savedText = text; });
    }
    part.onReport([list](const SavePart&) { return "saved " + list; });
}

} // namespace bf2editor
