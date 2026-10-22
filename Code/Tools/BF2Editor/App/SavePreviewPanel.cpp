// Code/Tools/BF2Editor/App/SavePreviewPanel.cpp - see SavePreviewPanel.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/SavePreviewPanel.h"

#include "App/EditSession.h"
#include "App/LevelDocument.h"
#include "Framework/EditorLog.h"

#include "imgui.h"

#include <windows.h>
#include <shellapi.h>

#include <cstdio>

namespace bf2editor
{

namespace
{

const ImVec4 kRemoved(1.0f, 0.45f, 0.4f, 1.0f);
const ImVec4 kAdded(0.45f, 0.85f, 0.45f, 1.0f);
const ImVec4 kHunk(0.45f, 0.75f, 1.0f, 1.0f);
const ImVec4 kRefused(1.0f, 0.62f, 0.3f, 1.0f);

// A target as the preview shows it: from the level's folder when it lies there, else from the game folder.
std::string shownPath(const std::string& target, const std::string& levelDirectory)
{
    if (!levelDirectory.empty() && target.size() > levelDirectory.size()
        && _strnicmp(target.c_str(), levelDirectory.c_str(), levelDirectory.size()) == 0)
        return target.substr(levelDirectory.size());
    return target;
}

std::string sizeText(unsigned long long size)
{
    char text[48];
    if (size < 10 * 1024)
        std::snprintf(text, sizeof(text), "%llu bytes", size);
    else if (size < 10ull * 1024 * 1024)
        std::snprintf(text, sizeof(text), "%.1f KB", static_cast<double>(size) / 1024.0);
    else
        std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(size) / (1024.0 * 1024.0));
    return text;
}

bool sizeOf(const std::string& path, unsigned long long& size)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data) || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    size = (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    return true;
}

// "changed: 1.2 KB -> 1.3 KB", "new: 80 bytes", "removed".
std::string changeText(const PlannedFile& file)
{
    if (file.remove)
        return "removed";
    unsigned long long before = 0, after = file.bytes.size();
    if (!file.staged.empty())
        sizeOf(file.staged, after);
    if (!sizeOf(file.target, before))
        return "new: " + sizeText(after);
    return "changed: " + sizeText(before) + " -> " + sizeText(after);
}

std::vector<std::string> splitText(const std::string& text)
{
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size())
    {
        const size_t end = text.find('\n', start);
        lines.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return lines;
}

} // namespace

void SavePreviewPanel::draw(EditSession& session, bool& previewOption)
{
    if (m_requested)
    {
        m_requested = false;
        m_selected = -1;
        m_diff.clear();
        m_planned = session.previewSave(false, m_summary);
        if (!m_planned)
            logLine(LogLevel::Warning, "save preview: %s", m_summary.c_str());
        ImGui::OpenPopup("Save the level");
    }
    drawPreview(session, previewOption);
    if (m_showBackups)
        drawBackups(session);
}

void SavePreviewPanel::selectFile(EditSession& session, int index)
{
    m_selected = index;
    std::string text;
    session.pendingSaveDiff(static_cast<size_t>(index), text);
    m_diff = splitText(text);
}

void SavePreviewPanel::drawPreview(EditSession& session, bool& previewOption)
{
    ImGui::SetNextWindowSize(ImVec2(900.0f, 620.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Save the level", 0))
        return;
    const SavePlan* plan = session.pendingSave();
    const std::string level = session.document() ? session.document()->levelDirectory() : std::string();
    if (!m_planned || !plan)
    {
        ImGui::TextWrapped("The save could not be planned: %s", m_summary.c_str());
        if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const size_t files = plan->fileCount();
    if (files == 0)
        ImGui::TextWrapped("The save writes no file: every file already holds what it would write.");
    else
        ImGui::TextWrapped("The save writes %u file%s, all of them or none. What they replace is kept in %s (the newest %d "
                           "saves of the level are kept; File > Backups puts one back).",
                           static_cast<unsigned>(files), files == 1 ? "" : "s", session.backupFolder().c_str(),
                           session.backupsKept());
    for (size_t p = 0; p < plan->parts().size(); ++p)
        if (plan->parts()[p].refused())
            ImGui::TextColored(kRefused, "Not saved: %s", plan->parts()[p].refusal().c_str());

    // The files, then the selected one's diff.
    const float listHeight = ImGui::GetContentRegionAvail().y * 0.38f;
    if (ImGui::BeginTable("files", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV
                                          | ImGuiTableFlags_Resizable, ImVec2(0.0f, listHeight)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Part", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableHeadersRow();
        int index = 0;
        for (size_t p = 0; p < plan->parts().size(); ++p)
        {
            const SavePart& part = plan->parts()[p];
            if (part.refused())
                continue;
            for (size_t i = 0; i < part.files().size(); ++i, ++index)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(part.name().c_str());
                ImGui::TableNextColumn();
                ImGui::PushID(index);
                if (ImGui::Selectable(shownPath(part.files()[i].target, level).c_str(), m_selected == index,
                                      ImGuiSelectableFlags_SpanAllColumns))
                    selectFile(session, index);
                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(changeText(part.files()[i]).c_str());
            }
        }
        ImGui::EndTable();
    }
    const float buttons = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginChild("diff", ImVec2(0.0f, -buttons), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (m_selected < 0)
            ImGui::TextDisabled("Pick a file to see what changes in it.");
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_diff.size()));
        while (clipper.Step())
            for (int l = clipper.DisplayStart; l < clipper.DisplayEnd; ++l)
            {
                const std::string& line = m_diff[static_cast<size_t>(l)];
                const char kind = line.empty() ? ' ' : line[0];
                if (kind == '+' && line.compare(0, 3, "+++") != 0)
                    ImGui::TextColored(kAdded, "%s", line.c_str());
                else if (kind == '-' && line.compare(0, 3, "---") != 0)
                    ImGui::TextColored(kRemoved, "%s", line.c_str());
                else if (kind == '@')
                    ImGui::TextColored(kHunk, "%s", line.c_str());
                else
                    ImGui::TextUnformatted(line.c_str());
            }
    }
    ImGui::EndChild();
    ImGui::Checkbox("Show this before every save", &previewOption);
    ImGui::SetItemTooltip("Off: File > Save and Ctrl+S write at once (every save still keeps a backup).");
    char write[64];
    std::snprintf(write, sizeof(write), files ? "Write %u file%s" : "Save", static_cast<unsigned>(files), files == 1 ? "" : "s");
    if (ImGui::Button(write))
    {
        std::string result;
        const bool saved = session.writePendingSave(result);
        logLine(saved ? LogLevel::Info : LogLevel::Error, "save: %s", result.c_str());
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        session.dropPendingSave();
        logLine(LogLevel::Info, "save: cancelled after the preview; nothing was written");
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void SavePreviewPanel::drawBackups(EditSession& session)
{
    if (m_refreshBackups)
    {
        m_refreshBackups = false;
        m_backups = session.backups();
        if (m_backupSelected >= static_cast<int>(m_backups.size()))
            m_backupSelected = -1;
    }
    ImGui::SetNextWindowSize(ImVec2(760.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Backups", &m_showBackups))
    {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Each backup holds what one save of the level replaced, as it was, and the files it made (%s). The "
                       "newest %d are kept (Tools > Options).", session.backupFolder().c_str(), session.backupsKept());
    if (ImGui::Button("Refresh"))
        m_refreshBackups = true;
    ImGui::SameLine();
    if (ImGui::Button("Open the folder"))
        ShellExecuteA(0, "open", session.backupFolder().c_str(), 0, 0, SW_SHOWNORMAL);
    if (m_backups.empty())
        ImGui::TextDisabled("No backups yet: the first save makes one.");
    const float listHeight = ImGui::GetContentRegionAvail().y * 0.45f;
    if (ImGui::BeginTable("backups", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV,
                          ImVec2(0.0f, listHeight)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Saved", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("By", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();
        for (size_t b = 0; b < m_backups.size(); ++b)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(b));
            if (ImGui::Selectable(m_backups[b].name.c_str(), m_backupSelected == static_cast<int>(b),
                                  ImGuiSelectableFlags_SpanAllColumns))
                m_backupSelected = static_cast<int>(b);
            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(m_backups[b].label.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u", static_cast<unsigned>(m_backups[b].entries.size()));
        }
        ImGui::EndTable();
    }
    const bool chosen = m_backupSelected >= 0 && m_backupSelected < static_cast<int>(m_backups.size());
    if (ImGui::BeginChild("entries", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 2.5f), ImGuiChildFlags_Borders))
    {
        if (!chosen)
            ImGui::TextDisabled("Pick a backup to see its files.");
        else
            for (size_t e = 0; e < m_backups[static_cast<size_t>(m_backupSelected)].entries.size(); ++e)
            {
                const SaveBackupEntry& entry = m_backups[static_cast<size_t>(m_backupSelected)].entries[e];
                ImGui::Text("%-8s %s", entry.kind.c_str(), entry.target.c_str());
            }
    }
    ImGui::EndChild();
    const bool unsaved = session.hasUnsavedChanges();
    ImGui::BeginDisabled(!chosen || unsaved);
    if (ImGui::Button("Put the level back to before this save..."))
        m_confirmRestore = true;
    ImGui::EndDisabled();
    if (unsaved)
        ImGui::SetItemTooltip("Save or undo the level's changes first: a restore opens the level again.");
    if (!m_backupResult.empty())
        ImGui::TextWrapped("%s", m_backupResult.c_str());
    if (m_confirmRestore)
    {
        ImGui::OpenPopup("Restore a backup");
        m_confirmRestore = false;
    }
    if (ImGui::BeginPopupModal("Restore a backup", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (chosen)
        {
            ImGui::TextWrapped("Every file this save and the %d saves after it wrote goes back to how it was before %s. The "
                               "restore is backed up too, and the editor then starts again on the level.",
                               m_backupSelected, m_backups[static_cast<size_t>(m_backupSelected)].name.c_str());
            if (ImGui::Button("Restore"))
            {
                std::string result;
                const bool restored = session.restoreBackup(m_backups[static_cast<size_t>(m_backupSelected)].name, result);
                logLine(restored ? LogLevel::Info : LogLevel::Error, "restore: %s", result.c_str());
                m_backupResult = result;
                m_refreshBackups = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}

} // namespace bf2editor
