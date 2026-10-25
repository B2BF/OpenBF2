// Code/Tools/BF2Editor/Framework/SavePlan.cpp - see SavePlan.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "Framework/SavePlan.h"

#include "Framework/SourceControl.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace bf2editor
{

namespace
{

const char kManifest[] = "manifest.txt";
const char kNextSuffix[] = ".editor-new";

std::string lowered(const std::string& text)
{
    std::string out = text;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

bool isFile(const std::string& path)
{
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool isFolder(const std::string& path)
{
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool readWhole(const std::string& path, std::string& bytes)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in)
        return false;
    std::ostringstream all;
    all << in.rdbuf();
    bytes = all.str();
    return true;
}

bool writeWhole(const std::string& path, const std::string& bytes)
{
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.flush();
    return static_cast<bool>(out);
}

bool fileSize(const std::string& path, unsigned long long& size)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data) || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    size = (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    return true;
}

// Whether two files hold the same bytes (read a block at a time).
bool sameFiles(const std::string& a, const std::string& b)
{
    unsigned long long sizeA = 0, sizeB = 0;
    if (!fileSize(a, sizeA) || !fileSize(b, sizeB) || sizeA != sizeB)
        return false;
    std::ifstream inA(a.c_str(), std::ios::binary), inB(b.c_str(), std::ios::binary);
    if (!inA || !inB)
        return false;
    std::vector<char> blockA(1 << 16), blockB(1 << 16);
    for (;;)
    {
        inA.read(&blockA[0], static_cast<std::streamsize>(blockA.size()));
        inB.read(&blockB[0], static_cast<std::streamsize>(blockB.size()));
        const std::streamsize got = inA.gcount();
        if (got != inB.gcount() || std::memcmp(&blockA[0], &blockB[0], static_cast<size_t>(got)) != 0)
            return false;
        if (got == 0 || !inA || !inB)
            return true;
    }
}

// Whether a file holds `bytes`.
bool holds(const std::string& path, const std::string& bytes)
{
    unsigned long long size = 0;
    if (!fileSize(path, size) || size != bytes.size())
        return false;
    std::string current;
    return readWhole(path, current) && current == bytes;
}

bool makeFolders(const std::string& path)
{
    // Every folder along the path, the first ones allowed to exist already.
    for (size_t i = 1; i <= path.size(); ++i)
    {
        if (i < path.size() && path[i] != '/' && path[i] != '\\')
            continue;
        const std::string part = path.substr(0, i);
        if (part.empty() || part[part.size() - 1] == ':' || part == "/" || part == "//")
            continue;
        if (!CreateDirectoryA(part.c_str(), 0) && GetLastError() != ERROR_ALREADY_EXISTS)
            return false;
    }
    return true;
}

std::string folderOf(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

void removeTree(const std::string& path)
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
                removeTree(child);
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

bool isAbsolute(const std::string& path)
{
    return (path.size() > 1 && path[1] == ':') || (path.size() > 1 && (path[0] == '/' || path[0] == '\\')
                                                   && (path[1] == '/' || path[1] == '\\'));
}

// The local date and time, as a folder name that sorts in time order: 2026-10-09_14-03-12-345.
std::string stampNow()
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    char text[64];
    std::snprintf(text, sizeof(text), "%04u-%02u-%02u_%02u-%02u-%02u-%03u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                  t.wSecond, t.wMilliseconds);
    return text;
}

// How a backup's manifest names a target (relative to the base when it lies under it), and where the original's copy
// goes in the backup folder.
void backupNames(const std::string& target, const std::string& baseFull, std::string& named, std::string& stored)
{
    const std::string full = fullSavePath(target);
    const std::string prefix = lowered(baseFull) + "/";
    if (!baseFull.empty() && lowered(full).compare(0, prefix.size(), prefix) == 0)
    {
        named = full.substr(prefix.size());
        stored = "files/" + named;
        return;
    }
    named = full;
    std::string flat = full;
    flat.erase(std::remove(flat.begin(), flat.end(), ':'), flat.end());
    while (!flat.empty() && flat[0] == '/')
        flat.erase(0, 1);
    stored = "files/_outside/" + flat;
}

// The backups of a level, oldest beyond `keep` removed (only folders holding a manifest: the editor's own).
void pruneBackups(const std::string& root, int keep)
{
    if (keep <= 0)
        return;
    const std::vector<SaveBackup> backups = listSaveBackups(root);
    for (size_t i = static_cast<size_t>(keep); i < backups.size(); ++i)
        removeTree(backups[i].folder);
}

} // namespace

std::string fullSavePath(const std::string& path)
{
    char full[MAX_PATH * 2];
    const DWORD length = GetFullPathNameA(path.c_str(), sizeof(full), full, 0);
    std::string out = length > 0 && length < sizeof(full) ? std::string(full, length) : path;
    std::replace(out.begin(), out.end(), '\\', '/');
    while (out.size() > 3 && out[out.size() - 1] == '/')
        out.erase(out.size() - 1);
    return out;
}

// ---- SavePart -------------------------------------------------------------------------------------------------------

SavePart::SavePart(const std::string& name)
    : m_name(name)
    , m_planned(0)
{
}

void SavePart::put(const PlannedFile& file)
{
    const std::string key = lowered(fullSavePath(file.target));
    for (size_t i = 0; i < m_files.size(); ++i)
    {
        if (lowered(fullSavePath(m_files[i].target)) == key)
        {
            // The first spelling kept: replacing a file under another case would rename it.
            const std::string spelling = m_files[i].target;
            m_files[i] = file;
            m_files[i].target = spelling;
            return;
        }
    }
    m_files.push_back(file);
    ++m_planned;
}

void SavePart::add(const std::string& target, const std::string& bytes, bool always)
{
    PlannedFile file;
    file.target = target;
    file.bytes = bytes;
    file.always = always;
    put(file);
}

void SavePart::addStaged(const std::string& target, const std::string& staged)
{
    PlannedFile file;
    file.target = target;
    file.staged = staged;
    put(file);
}

void SavePart::addRemoval(const std::string& target)
{
    PlannedFile file;
    file.target = target;
    file.remove = true;
    put(file);
}

void SavePart::refuse(const std::string& reason)
{
    m_refusal = reason.empty() ? m_name + " NOT saved" : reason;
    m_files.clear();
}

std::string SavePart::answer() const
{
    if (refused())
        return m_refusal;
    return m_report ? m_report(*this) : std::string();
}

void SavePart::finish()
{
    for (size_t i = 0; i < m_written.size() && !refused(); ++i)
        m_written[i]();
}

void SavePart::replacing()
{
    for (size_t i = 0; i < m_replacing.size() && !refused(); ++i)
        m_replacing[i]();
}

void SavePart::replaced()
{
    for (size_t i = 0; i < m_replaced.size() && !refused(); ++i)
        m_replaced[i]();
}

// ---- SavePlan -------------------------------------------------------------------------------------------------------

SavePart& SavePlan::part(const std::string& name)
{
    m_parts.push_back(SavePart(name));
    return m_parts.back();
}

void SavePlan::dropUnchanged()
{
    for (size_t p = 0; p < m_parts.size(); ++p)
    {
        SavePart& part = m_parts[p];
        std::vector<PlannedFile>& files = part.files();
        std::vector<PlannedFile> kept;
        for (size_t i = 0; i < files.size() && !part.refused(); ++i)
        {
            const PlannedFile& file = files[i];
            if (file.remove)
            {
                if (isFile(file.target))
                    kept.push_back(file);
                continue;
            }
            if (!file.staged.empty() && !isFile(file.staged))
            {
                part.refuse(part.name() + " NOT saved: " + file.staged + " cannot be read");
                break;
            }
            const bool same = isFile(file.target) && (file.staged.empty() ? holds(file.target, file.bytes)
                                                                          : sameFiles(file.staged, file.target));
            if (file.always || !same)
                kept.push_back(file);
        }
        if (!part.refused())
            files.swap(kept);
    }
}

size_t SavePlan::fileCount() const
{
    size_t count = 0;
    for (size_t p = 0; p < m_parts.size(); ++p)
        if (!m_parts[p].refused())
            count += m_parts[p].files().size();
    return count;
}

bool SavePlan::plans(const std::string& target) const
{
    const std::string key = lowered(fullSavePath(target));
    for (size_t p = 0; p < m_parts.size(); ++p)
    {
        if (m_parts[p].refused())
            continue;
        for (size_t i = 0; i < m_parts[p].files().size(); ++i)
            if (lowered(fullSavePath(m_parts[p].files()[i].target)) == key)
                return true;
    }
    return false;
}

bool SavePlan::anyRefused() const
{
    for (size_t p = 0; p < m_parts.size(); ++p)
        if (m_parts[p].refused())
            return true;
    return false;
}

bool SavePlan::content(const PlannedFile& file, std::string& bytes, std::string& error)
{
    if (file.remove)
    {
        bytes.clear();
        return true;
    }
    if (file.staged.empty())
    {
        bytes = file.bytes;
        return true;
    }
    if (!readWhole(file.staged, bytes))
    {
        error = "cannot read " + file.staged;
        return false;
    }
    return true;
}

std::string SavePlan::answers() const
{
    std::string text;
    for (size_t p = 0; p < m_parts.size(); ++p)
    {
        const std::string answer = m_parts[p].answer();
        if (!answer.empty())
            text += (text.empty() ? "" : "; ") + answer;
    }
    return text;
}

void SavePlan::finish()
{
    for (size_t p = 0; p < m_parts.size(); ++p)
        m_parts[p].finish();
}

void SavePlan::replacing()
{
    for (size_t p = 0; p < m_parts.size(); ++p)
        m_parts[p].replacing();
}

void SavePlan::replaced()
{
    for (size_t p = 0; p < m_parts.size(); ++p)
        m_parts[p].replaced();
}

// ---- writing --------------------------------------------------------------------------------------------------------

bool writeSavePlan(SavePlan& plan, const SaveBackupPolicy& policy, SaveWriteResult& result)
{
    result = SaveWriteResult();

    // Source control first: a part with a file it refuses is not saved, and the others are.
    for (size_t p = 0; p < plan.parts().size(); ++p)
    {
        SavePart& part = plan.parts()[p];
        for (size_t i = 0; i < part.files().size() && !part.refused(); ++i)
        {
            const PlannedFile& file = part.files()[i];
            if (!isFile(file.target))
                continue;
            const ScmResult answer = file.remove ? sourceControl().remove(file.target) : sourceControl().openForEdit(file.target);
            if (!answer.ok)
                part.refuse(part.name() + " NOT saved: source control refused " + file.target + ": " + answer.message);
        }
    }

    struct Item
    {
        const PlannedFile* file;        // what is written (the later part's, when two parts plan the target)
        std::string        target;      // the target as first planned
        bool               existed;
        std::string        next;        // the new content beside the target
        std::string        named;       // the manifest's name for the target
        std::string        stored;      // the original's copy in the backup folder
    };
    std::vector<Item> items;
    std::vector<std::string> keys;
    for (size_t p = 0; p < plan.parts().size(); ++p)
    {
        const SavePart& part = plan.parts()[p];
        if (part.refused())
            continue;
        for (size_t i = 0; i < part.files().size(); ++i)
        {
            const std::string key = lowered(fullSavePath(part.files()[i].target));
            const std::vector<std::string>::const_iterator seen = std::find(keys.begin(), keys.end(), key);
            if (seen != keys.end())
            {
                // Planned by an earlier part too: written once, with this part's content.
                items[static_cast<size_t>(seen - keys.begin())].file = &part.files()[i];
                continue;
            }
            Item item;
            item.file = &part.files()[i];
            item.target = item.file->target;
            item.existed = isFile(item.target);
            items.push_back(item);
            keys.push_back(key);
        }
    }
    // A removal of a file that is not there does nothing.
    for (size_t k = items.size(); k-- > 0;)
        if (items[k].file->remove && !items[k].existed)
            items.erase(items.begin() + static_cast<std::ptrdiff_t>(k));
    if (items.empty())
    {
        plan.finish();
        return true;
    }

    // The save's own backup folder.
    const std::string baseFull = policy.base.empty() ? std::string() : fullSavePath(policy.base);
    const std::string stamp = stampNow();
    std::string folder = policy.root + "/" + stamp;
    for (int n = 2; isFolder(folder) && n < 100; ++n)
    {
        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "-%d", n);
        folder = policy.root + "/" + stamp + suffix;
    }
    if (policy.root.empty() || !makeFolders(folder))
    {
        result.error = "could not make the backup folder " + folder + ": nothing was written";
        return false;
    }
    const auto abandon = [&](const std::string& why) {
        for (size_t k = 0; k < items.size(); ++k)
            if (!items[k].next.empty())
                DeleteFileA(items[k].next.c_str());
        removeTree(folder);
        result.error = why + ": nothing was written";
        return false;
    };

    // Every new file beside its target, read back.
    for (size_t i = 0; i < items.size(); ++i)
    {
        Item& item = items[i];
        if (item.file->remove)
            continue;
        const std::string targetFolder = folderOf(item.target);
        if (!targetFolder.empty() && !makeFolders(targetFolder))
            return abandon("could not make the folder " + targetFolder);
        item.next = item.target + kNextSuffix;
        const bool written = item.file->staged.empty()
                                 ? writeWhole(item.next, item.file->bytes) && holds(item.next, item.file->bytes)
                                 : CopyFileA(item.file->staged.c_str(), item.next.c_str(), FALSE) && sameFiles(item.file->staged, item.next);
        if (!written)
            return abandon("could not write " + item.next);
    }

    // Every file replaced or removed copied into the backup folder.
    for (size_t i = 0; i < items.size(); ++i)
    {
        Item& item = items[i];
        backupNames(item.target, baseFull, item.named, item.stored);
        if (!item.existed)
            continue;
        const std::string copy = folder + "/" + item.stored;
        if (!makeFolders(folderOf(copy)) || !CopyFileA(item.target.c_str(), copy.c_str(), FALSE)
            || !sameFiles(item.target, copy))
            return abandon("could not back up " + item.target + " into " + folder);
    }

    // The targets replaced, made or removed in order; a failure puts back each one done before it. Around it, the
    // parts that hold a target open let go of it and take it up again.
    plan.replacing();
    size_t done = 0;
    DWORD code = 0;
    for (; done < items.size(); ++done)
    {
        const Item& item = items[done];
        const bool ok = item.file->remove ? DeleteFileA(item.target.c_str()) != 0
                                          : MoveFileExA(item.next.c_str(), item.target.c_str(),
                                                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!ok)
        {
            code = GetLastError();
            break;
        }
    }
    if (done < items.size())
    {
        std::string notBack;
        for (size_t k = done; k-- > 0;)
        {
            const Item& item = items[k];
            const bool back = item.existed ? CopyFileA((folder + "/" + item.stored).c_str(), item.target.c_str(), FALSE) != 0
                                           : DeleteFileA(item.target.c_str()) != 0;
            if (!back)
                notBack += (notBack.empty() ? "" : ", ") + item.target;
        }
        for (size_t k = done; k < items.size(); ++k)
            if (!items[k].next.empty())
                DeleteFileA(items[k].next.c_str());
        plan.replaced();
        char number[32];
        std::snprintf(number, sizeof(number), " (error %lu)", static_cast<unsigned long>(code));
        const std::string failed = items[done].target + number;
        if (notBack.empty())
        {
            removeTree(folder);
            result.error = "could not replace " + failed + ": nothing was saved, every file is as it was";
        }
        else
            result.error = "could not replace " + failed + "; these could not be put back (their originals are in " + folder
                           + "): " + notBack;
        return false;
    }

    plan.replaced();

    // Source control learns of the files made.
    for (size_t i = 0; i < items.size(); ++i)
        if (!items[i].existed && !items[i].file->remove)
            sourceControl().add(items[i].target);

    // The manifest: what each entry was, and where its original is.
    std::string manifest = "# BF2Editor save backup: the files one save replaced or removed, as they were before it, and "
                           "the files it made\r\nlabel " + policy.label + "\r\ntime " + stamp + "\r\n";
    for (size_t i = 0; i < items.size(); ++i)
    {
        const Item& item = items[i];
        if (!item.existed)
            manifest += "made\t" + item.named + "\r\n";
        else
            manifest += std::string(item.file->remove ? "removed" : "replaced") + "\t" + item.named + "\t" + item.stored + "\r\n";
        result.written.push_back(item.target);
    }
    const std::string manifestPath = folder + "/" + kManifest;
    if (!writeWhole(manifestPath + kNextSuffix, manifest)
        || !MoveFileExA((manifestPath + kNextSuffix).c_str(), manifestPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        result.error = "the files are written, but the backup's manifest could not be: " + folder + " cannot be put back";
    result.backup = folder;
    plan.finish();
    pruneBackups(policy.root, policy.keep);
    return true;
}

// ---- backups --------------------------------------------------------------------------------------------------------

std::vector<SaveBackup> listSaveBackups(const std::string& root)
{
    std::vector<SaveBackup> backups;
    WIN32_FIND_DATAA found;
    const HANDLE search = FindFirstFileA((root + "/*").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return backups;
    do
    {
        const std::string name = found.cFileName;
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || name == "." || name == "..")
            continue;
        SaveBackup backup;
        backup.folder = root + "/" + name;
        backup.name = name;
        std::string text;
        if (!readWhole(backup.folder + "/" + kManifest, text))
            continue;
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if (line.empty() || line[0] == '#')
                continue;
            if (line.compare(0, 6, "label ") == 0)
            {
                backup.label = line.substr(6);
                continue;
            }
            if (line.compare(0, 5, "time ") == 0)
                continue;
            SaveBackupEntry entry;
            const size_t tab = line.find('\t');
            if (tab == std::string::npos)
                continue;
            entry.kind = line.substr(0, tab);
            const size_t second = line.find('\t', tab + 1);
            entry.target = line.substr(tab + 1, second == std::string::npos ? std::string::npos : second - tab - 1);
            if (second != std::string::npos)
                entry.stored = line.substr(second + 1);
            if ((entry.kind == "made" && entry.stored.empty()) || ((entry.kind == "replaced" || entry.kind == "removed") && !entry.stored.empty()))
                backup.entries.push_back(entry);
        }
        backups.push_back(backup);
    } while (FindNextFileA(search, &found));
    FindClose(search);
    std::sort(backups.begin(), backups.end(), [](const SaveBackup& a, const SaveBackup& b) { return a.name > b.name; });
    return backups;
}

bool planBackupRestore(const std::vector<SaveBackup>& backups, size_t last, const std::string& base, SavePart& part,
                       std::string& error)
{
    if (last >= backups.size())
    {
        error = "no such backup";
        return false;
    }
    for (size_t b = 0; b <= last; ++b)
    {
        for (size_t i = 0; i < backups[b].entries.size(); ++i)
        {
            const SaveBackupEntry& entry = backups[b].entries[i];
            const std::string target = isAbsolute(entry.target) || base.empty() ? entry.target : base + "/" + entry.target;
            if (entry.kind == "made")
            {
                part.addRemoval(target);
                continue;
            }
            const std::string stored = backups[b].folder + "/" + entry.stored;
            if (!isFile(stored))
            {
                error = "the backup " + backups[b].name + " has lost its copy of " + entry.target;
                return false;
            }
            part.addStaged(target, stored);
        }
    }
    return true;
}

} // namespace bf2editor
