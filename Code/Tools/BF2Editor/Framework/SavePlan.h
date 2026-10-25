// Code/Tools/BF2Editor/Framework/SavePlan.h

// The files one save writes, gathered before any of them is written, so that the save can be shown first and then
// written as a whole. A plan is made of parts (the level's objects, its terrain, its roads ...), each with its files,
// the step that marks it saved and the answer it gives. Writing a plan puts every new file beside its target, copies
// each file it replaces into a backup folder of the save's own, and only then replaces the targets; when one cannot be
// replaced, the files replaced before it are put back and the new ones taken away, so nothing is half saved. A
// level's backups, newest first, can each be put back the same way, which is itself a save with a backup.
//
// [CHOSEN, not recovered] the retail editor wrote each file as it went and kept no backup (issue draft 13: save
// preview, backups, all-or-nothing saves). Engine-free (checked in bf2_editor_checks). Editor-only code (D3).
#ifndef BF2EDITOR_FRAMEWORK_SAVEPLAN_H
#define BF2EDITOR_FRAMEWORK_SAVEPLAN_H

#include "Framework/EditorConfig.h"

#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace bf2editor
{

// One file a part writes or takes away.
struct PlannedFile
{
    std::string target;         // the file replaced, made or removed
    std::string bytes;          // its new content, when `staged` is empty
    std::string staged;         // a file that holds its new content (a staging folder's or a backup's)
    bool        always = false; // written even when the target holds that content already (a resave)
    bool        remove = false; // the target is taken away (a restore of a file a save made)
};

// One part of a save: its files, what marks it saved once they are written, and its answer.
class SavePart
{
public:
    explicit SavePart(const std::string& name);

    const std::string& name() const { return m_name; }

    // A file with its new content, or with its content in a staged file, or one to take away. A target added twice
    // keeps the later content under the first spelling (paths compared without regard to case or slash direction).
    void add(const std::string& target, const std::string& bytes, bool always = false);
    void addStaged(const std::string& target, const std::string& staged);
    void addRemoval(const std::string& target);

    // Runs once the plan's files are written, after the steps added before it: the part marks itself saved.
    void onWritten(const std::function<void()>& step) { m_written.push_back(step); }
    // Run just before the plan's targets are replaced (every new file is beside its target and every backup made), and
    // right after the replacing ends, whether it succeeded or not: a part whose targets the engine holds open (a mounted
    // archive) lets go of them and takes them up again.
    void onReplacing(const std::function<void()>& step) { m_replacing.push_back(step); }
    void onReplaced(const std::function<void()>& step) { m_replaced.push_back(step); }
    void replacing();
    void replaced();
    // What the save answers for the part once written; given the part (its counts). An empty answer adds nothing.
    void onReport(const std::function<std::string(const SavePart&)>& report) { m_report = report; }

    // The part is not saved: its files are dropped and `reason` is its answer.
    void refuse(const std::string& reason);
    bool refused() const { return !m_refusal.empty(); }
    const std::string& refusal() const { return m_refusal; }

    const std::vector<PlannedFile>& files() const { return m_files; }
    std::vector<PlannedFile>& files() { return m_files; }
    // Files added, before the plan dropped the ones whose target already holds their content.
    size_t planned() const { return m_planned; }
    // Files the plan writes (after the drop).
    size_t changed() const { return m_files.size(); }

    // The answer: the refusal, or the report's (empty when the part has none).
    std::string answer() const;
    void finish();

private:
    void put(const PlannedFile& file);

    std::string                                   m_name;
    std::vector<PlannedFile>                      m_files;
    size_t                                        m_planned;
    std::vector<std::function<void()> >           m_written;
    std::vector<std::function<void()> >           m_replacing;
    std::vector<std::function<void()> >           m_replaced;
    std::function<std::string(const SavePart&)>   m_report;
    std::string                                   m_refusal;
};

class SavePlan
{
public:
    // A new part, after the others (its reference stays valid while the plan lives).
    SavePart& part(const std::string& name);

    // Takes out every file whose target already holds its new content (unless `always`), and every removal of a
    // target that is not there. A part with a staged file that cannot be read is refused.
    void dropUnchanged();

    const std::deque<SavePart>& parts() const { return m_parts; }
    std::deque<SavePart>& parts() { return m_parts; }
    // Files to write in all parts not refused.
    size_t fileCount() const;
    // Whether a part not refused writes `target` already (compared as SavePart::add compares).
    bool plans(const std::string& target) const;
    bool anyRefused() const;

    // The new content of a planned file.
    static bool content(const PlannedFile& file, std::string& bytes, std::string& error);

    // Every part's answer in order, joined with "; " (empty answers left out).
    std::string answers() const;
    // Runs each written part's step (writeSavePlan calls it once every file is written).
    void finish();
    // Each part's onReplacing and onReplaced steps (writeSavePlan calls them around replacing the targets).
    void replacing();
    void replaced();

private:
    std::deque<SavePart> m_parts;
};

// Where a level's save backups go.
struct SaveBackupPolicy
{
    std::string root;       // this level's backup folder (<game folder>/bf2editor/Backups/<mod>/<level>)
    std::string base;       // the folder backed-up paths are kept relative to (the game folder)
    int         keep = 10;  // backups kept per level, the oldest removed after a save; 0 keeps every one
    std::string label;      // what wrote the plan ("EditorLevel.save", "restore 2026-10-09_14-03-12-345" ...)
};

// What writing a plan did.
struct SaveWriteResult
{
    std::string              backup;    // the save's backup folder ("" when nothing was written)
    std::vector<std::string> written;   // the targets replaced, made or removed
    std::string              error;     // why nothing was written, and anything that could not be put back
};

// Writes every file of the plan's parts that are not refused, or none of them (see the file comment). A part that
// source control refuses is refused and the rest written. A target two parts plan is written once, with the later
// part's content. True when the files are written (or there were none) and
// the parts' steps have run.
bool writeSavePlan(SavePlan& plan, const SaveBackupPolicy& policy, SaveWriteResult& result);

// One save's backup.
struct SaveBackupEntry
{
    std::string kind;       // "replaced", "made" or "removed"
    std::string target;     // the file, relative to the policy's base when it lies under it
    std::string stored;     // the original's copy, relative to the backup's folder ("" for a made file)
};

struct SaveBackup
{
    std::string                  folder;     // its folder
    std::string                  name;       // the folder's name: the save's date and time
    std::string                  label;      // what wrote it
    std::vector<SaveBackupEntry> entries;
};

// The backups in a level's backup folder, newest first. Folders without a readable manifest are left out.
std::vector<SaveBackup> listSaveBackups(const std::string& root);

// Adds to `part` what puts every file back as it was before the saves backups[0..last] wrote it (the list newest
// first, as listSaveBackups gives it): a file several of them wrote gets its state from before the oldest.
bool planBackupRestore(const std::vector<SaveBackup>& backups, size_t last, const std::string& base, SavePart& part,
                       std::string& error);

// The full path of `path`, with forward slashes.
std::string fullSavePath(const std::string& path);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_SAVEPLAN_H
