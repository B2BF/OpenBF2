// Code/Tools/BF2Editor/Framework/TextDiff.h

// A line diff of two texts, for the save preview: the hunks, with a few unchanged lines around each change, that turn
// the old text into the new one. Myers' O(ND) algorithm, bisected so that its memory grows only with the texts'
// length. A line ends at '\n' and keeps a '\r' before it, so a line whose ending alone changed shows as changed.
// Engine-free (checked in bf2_editor_checks). Editor-only code (D3).
#ifndef BF2EDITOR_FRAMEWORK_TEXTDIFF_H
#define BF2EDITOR_FRAMEWORK_TEXTDIFF_H

#include "Framework/EditorConfig.h"

#include <string>
#include <vector>

namespace bf2editor
{

struct DiffLine
{
    char        kind = ' ';     // ' ' in both texts, '-' only in the old one, '+' only in the new one, '\\' a note
    std::string text;           // the line without its ending
};

struct DiffHunk
{
    int                   beforeStart = 0;   // its first old line, from 1 (the line before it when it has none)
    int                   beforeCount = 0;
    int                   afterStart = 0;
    int                   afterCount = 0;
    std::vector<DiffLine> lines;
};

struct TextDiff
{
    std::vector<DiffHunk> hunks;
    int                   removed = 0;       // lines only in the old text
    int                   added = 0;         // lines only in the new text
};

// The diff of two texts, with `context` unchanged lines around each change.
TextDiff diffTexts(const std::string& before, const std::string& after, int context = 3);

// A diff in the unified format: the ---/+++ lines naming the two sides, then each hunk under its @@ line.
std::string unifiedDiff(const TextDiff& diff, const std::string& beforeName, const std::string& afterName);

// Whether bytes read as text (no NUL byte in them); the preview shows other files by their sizes.
bool looksLikeText(const std::string& bytes);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_TEXTDIFF_H
