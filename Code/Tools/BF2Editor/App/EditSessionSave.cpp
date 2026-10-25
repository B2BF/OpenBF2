// Code/Tools/BF2Editor/App/EditSessionSave.cpp - the save preview, backups and restores of EditSession (see the
// declarations in EditSession.h and Framework/SavePlan.h). [CHOSEN, not recovered]: the retail editor wrote each file
// as it went and kept no backup (issue draft 13). Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/EditorLevel.h"
#include "App/LevelDocument.h"
#include "Framework/EditorLog.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SavePlan.h"
#include "Framework/TextDiff.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace bf2editor
{

namespace
{

std::string count(size_t n, const char* one, const char* many)
{
    char text[64];
    std::snprintf(text, sizeof(text), "%u %s", static_cast<unsigned>(n), n == 1 ? one : many);
    return text;
}

bool fileSize(const std::string& path, unsigned long long& size)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data) || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    size = (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    return true;
}

std::string bytes(unsigned long long size)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%llu", size);
    return text;
}

// The game folder: the editor runs in it, and backups keep paths relative to it.
std::string gameFolder()
{
    char folder[MAX_PATH] = "";
    GetCurrentDirectoryA(MAX_PATH, folder);
    return fullSavePath(folder);
}

// The file of the plan's parts that are saved, counted in order; 0 when there are fewer.
const PlannedFile* plannedFile(const SavePlan& plan, size_t index, const SavePart** part)
{
    for (size_t p = 0; p < plan.parts().size(); ++p)
    {
        const SavePart& candidate = plan.parts()[p];
        if (candidate.refused())
            continue;
        if (index < candidate.files().size())
        {
            if (part)
                *part = &candidate;
            return &candidate.files()[index];
        }
        index -= candidate.files().size();
    }
    return 0;
}

// "changed, 1200 -> 1260 bytes", "new, 80 bytes", "removed".
std::string plannedChange(const PlannedFile& file)
{
    if (file.remove)
        return "removed";
    unsigned long long before = 0, after = file.bytes.size();
    if (!file.staged.empty())
        fileSize(file.staged, after);
    if (!fileSize(file.target, before))
        return "new, " + bytes(after) + " bytes";
    return "changed, " + bytes(before) + " -> " + bytes(after) + " bytes";
}

// What the plan writes, one file after another (numbered for EditorLevel.previewDiff), then what it does not save.
std::string describePlan(const SavePlan& plan, const std::string& backups)
{
    const size_t files = plan.fileCount();
    std::string text = files == 0 ? std::string("the save writes no file")
                                  : "the save writes " + count(files, "file", "files") + " (what they replace is kept in "
                                        + backups + ")";
    size_t index = 0;
    for (size_t p = 0; p < plan.parts().size(); ++p)
    {
        const SavePart& part = plan.parts()[p];
        if (part.refused())
            continue;
        for (size_t i = 0; i < part.files().size(); ++i, ++index)
            text += std::string(index ? "; #" : ": #") + std::to_string(index) + " " + part.files()[i].target + " ["
                    + part.name() + "] " + plannedChange(part.files()[i]);
    }
    for (size_t p = 0; p < plan.parts().size(); ++p)
        if (plan.parts()[p].refused())
            text += "; " + plan.parts()[p].refusal();
    return text;
}

} // namespace

std::string EditSession::backupFolder() const
{
    // "mods/<mod>/Levels/<level>/" -> bf2editor/Backups/<mod>/<level>.
    std::string mod = "mod", level = "level";
    if (m_document && !m_document->levelDirectory().empty())
    {
        std::string folder = m_document->levelDirectory();
        while (!folder.empty() && (folder[folder.size() - 1] == '/' || folder[folder.size() - 1] == '\\'))
            folder.erase(folder.size() - 1);
        const size_t slash = folder.find_last_of("/\\");
        level = slash == std::string::npos ? folder : folder.substr(slash + 1);
        std::string lowered = folder;
        for (size_t i = 0; i < lowered.size(); ++i)
            lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
        const size_t levels = lowered.find("/levels/");
        if (levels != std::string::npos)
        {
            const std::string modFolder = folder.substr(0, levels);
            const size_t modSlash = modFolder.find_last_of("/\\");
            mod = modSlash == std::string::npos ? modFolder : modFolder.substr(modSlash + 1);
        }
    }
    return "bf2editor/Backups/" + mod + "/" + level;
}

std::string EditSession::modFolder() const
{
    // "mods/<mod>/Levels/<level>/" -> "mods/<mod>".
    if (!m_document)
        return std::string();
    const std::string folder = m_document->levelDirectory();
    std::string lowered = folder;
    for (size_t i = 0; i < lowered.size(); ++i)
        lowered[i] = lowered[i] == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
    const size_t levels = lowered.find("/levels/");
    return levels == std::string::npos ? std::string() : folder.substr(0, levels);
}

bool EditSession::writeSave(SavePlan& plan, const std::string& label, std::string& result)
{
    SaveBackupPolicy policy;
    policy.root = backupFolder();
    policy.base = gameFolder();
    policy.keep = m_backupsKept;
    policy.label = label;
    SaveWriteResult written;
    if (!writeSavePlan(plan, policy, written))
    {
        result = "NOT saved: " + written.error;
        return false;
    }
    result = plan.answers();
    if (!written.backup.empty())
        logLine(LogLevel::Info, "save: %s written; what they replaced is kept in %s",
                count(written.written.size(), "file", "files").c_str(), written.backup.c_str());
    if (!written.error.empty())
        logLine(LogLevel::Warning, "save: %s", written.error.c_str());
    return true;
}

bool EditSession::writeFilesNow(const std::string& label, const std::vector<std::pair<std::string, std::string> >& files,
                                std::string& error)
{
    SavePlan plan;
    SavePart& part = plan.part(label);
    for (size_t i = 0; i < files.size(); ++i)
        part.add(files[i].first, files[i].second);
    plan.dropUnchanged();
    std::string answer;
    if (!writeSave(plan, label, answer) || plan.anyRefused())
    {
        error = answer;
        return false;
    }
    return true;
}

bool EditSession::previewSave(bool everything, std::string& result)
{
    dropPendingSave();
    std::unique_ptr<SavePlan> plan(new SavePlan);
    if (!planLevelSave(*plan, everything, result))
        return false;
    plan->dropUnchanged();
    m_pendingSave = std::move(plan);
    m_pendingEverything = everything;
    m_pendingRevision = m_history.revision();
    m_pendingState = unsavedSummary();
    result = describePlan(*m_pendingSave, backupFolder());
    return true;
}

bool EditSession::writePendingSave(std::string& result)
{
    if (!m_pendingSave)
    {
        result = "no save is waiting: preview one first (EditorLevel.preview)";
        return false;
    }
    if (m_history.revision() != m_pendingRevision || unsavedSummary() != m_pendingState)
    {
        dropPendingSave();
        result = "the level changed after the preview: nothing was written; preview the save again";
        return false;
    }
    std::unique_ptr<SavePlan> plan = std::move(m_pendingSave);
    return writeLevelSave(*plan, m_pendingEverything, result);
}

bool EditSession::pendingSaveDiff(size_t index, std::string& result)
{
    if (!m_pendingSave)
    {
        result = "no save is waiting: preview one first (EditorLevel.preview)";
        return false;
    }
    const PlannedFile* file = plannedFile(*m_pendingSave, index, 0);
    if (!file)
    {
        result = "the pending save has no file #" + std::to_string(index);
        return false;
    }
    std::string before, after, error;
    const bool existed = ObjectLibrary::readTextFile(file->target, before, error);
    if (file->remove)
    {
        result = file->target + " is taken away (" + bytes(before.size()) + " bytes)";
        return true;
    }
    if (!SavePlan::content(*file, after, error))
    {
        result = error;
        return false;
    }
    if (!looksLikeText(before) || !looksLikeText(after))
    {
        result = file->target + " is not a text file: " + (existed ? bytes(before.size()) + " -> " : std::string("new, "))
                 + bytes(after.size()) + " bytes" + (existed && before == after ? " (the same bytes, written again)" : "");
        return true;
    }
    const TextDiff diff = diffTexts(before, after);
    result = unifiedDiff(diff, existed ? file->target : std::string("(no file)"), file->target + " (as saved)");
    if (diff.hunks.empty())
        result += "(the same text, written again)\n";
    return true;
}

std::vector<SaveBackup> EditSession::backups() const
{
    return listSaveBackups(backupFolder());
}

bool EditSession::listBackups(std::string& result)
{
    const std::vector<SaveBackup> list = backups();
    if (list.empty())
    {
        result = "no backups of this level yet (they go to " + backupFolder() + ")";
        return true;
    }
    result = count(list.size(), "backup", "backups") + " in " + backupFolder() + ", newest first";
    for (size_t b = 0; b < list.size(); ++b)
    {
        size_t replaced = 0, made = 0, removed = 0;
        for (size_t e = 0; e < list[b].entries.size(); ++e)
        {
            const std::string& kind = list[b].entries[e].kind;
            replaced += kind == "replaced";
            made += kind == "made";
            removed += kind == "removed";
        }
        result += std::string(b ? "; " : ": ") + list[b].name + " " + list[b].label + " (" + count(replaced, "file", "files")
                  + " replaced, " + std::to_string(made) + " made, " + std::to_string(removed) + " removed)";
    }
    return true;
}

bool EditSession::restoreBackup(const std::string& name, std::string& result)
{
    if (!m_document || !m_document->isOpen())
    {
        result = "no level is open";
        return false;
    }
    if (hasUnsavedChanges())
    {
        result = "the level has unsaved changes: save them or undo them first (a restore opens the level again)";
        return false;
    }
    const std::vector<SaveBackup> list = backups();
    size_t index = list.size();
    for (size_t b = 0; b < list.size() && index == list.size(); ++b)
        if (list[b].name == name)
            index = b;
    // "#n": the n-th newest (#0 the last save's).
    if (index == list.size() && name.size() > 1 && name[0] == '#'
        && name.find_first_not_of("0123456789", 1) == std::string::npos)
        index = std::min(static_cast<size_t>(std::strtoul(name.c_str() + 1, 0, 10)), list.size());
    if (index == list.size())
    {
        result = "no backup " + name + " of this level (EditorLevel.backups lists them; #0 is the newest)";
        return false;
    }
    SavePlan plan;
    SavePart& part = plan.part("restore");
    std::string error;
    if (!planBackupRestore(list, index, gameFolder(), part, error))
    {
        result = error;
        return false;
    }
    plan.dropUnchanged();
    const size_t files = part.changed();
    if (files == 0)
    {
        result = "the level's files are as they were before " + name + " already: nothing to put back";
        return true;
    }
    std::string answer;
    if (!writeSave(plan, "restore " + list[index].name, answer) || plan.anyRefused())
    {
        result = answer;
        return false;
    }
    dropPendingSave();
    m_restored = true;
    result = "put back " + count(files, "file", "files") + (files == 1 ? " as it was" : " as they were") + " before the save "
             + list[index].name + (index ? " and the " + count(index, "save", "saves") + " after it" : std::string())
             + " (the restore has a backup of its own)";
    if (m_canRelaunch)
    {
        m_relaunchMode = m_level.gameMode();
        m_relaunchSize = m_level.layerSize();
        m_relaunchLevel.clear();
        m_relaunchMod.clear();
        result += "; the editor starts again on the level";
    }
    else
        result += "; open the level again to see them (this run cannot start the editor again)";
    logLine(LogLevel::Info, "restore: %s", result.c_str());
    return true;
}

bool EditSession::saveCommand(const std::string& action, const std::string& argument, std::string& result)
{
    if (action == "preview" || action == "previewAll")
        return previewSave(action == "previewAll", result);
    if (action == "diff")
    {
        char* end = 0;
        const long index = std::strtol(argument.c_str(), &end, 10);
        if (argument.empty() || *end || index < 0)
        {
            result = "EditorLevel.previewDiff takes a file's number from the preview";
            return false;
        }
        return pendingSaveDiff(static_cast<size_t>(index), result);
    }
    if (action == "write")
        return writePendingSave(result);
    if (action == "drop")
    {
        result = m_pendingSave ? "the pending save is dropped: nothing was written" : "no save was waiting";
        dropPendingSave();
        return true;
    }
    if (action == "backups")
        return listBackups(result);
    if (action == "restore")
        return restoreBackup(argument, result);
    result = "no such save action '" + action + "'";
    return false;
}

} // namespace bf2editor
