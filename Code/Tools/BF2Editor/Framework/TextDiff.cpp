// Code/Tools/BF2Editor/Framework/TextDiff.cpp - see TextDiff.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "Framework/TextDiff.h"

#include <cstdio>
#include <unordered_map>

namespace bf2editor
{

namespace
{

// The lines of a text, each with its ending.
std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size())
    {
        const size_t end = text.find('\n', start);
        if (end == std::string::npos)
        {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end + 1 - start));
        start = end + 1;
    }
    return lines;
}

struct Edit
{
    char kind;      // '=', '-' or '+'
    int  before;    // the old line (for '=' and '-')
    int  after;     // the new line (for '=' and '+')
};

// Myers' bisection over two sequences of line numbers (equal lines share a number).
class Differ
{
public:
    Differ(const std::vector<int>& a, const std::vector<int>& b, std::vector<Edit>& out)
        : m_a(a)
        , m_b(b)
        , m_out(out)
    {
    }

    void run(int a0, int a1, int b0, int b1)
    {
        while (a0 < a1 && b0 < b1 && m_a[a0] == m_b[b0])
            emit('=', a0++, b0++);
        int common = 0;
        while (a1 - common > a0 && b1 - common > b0 && m_a[a1 - 1 - common] == m_b[b1 - 1 - common])
            ++common;
        const int aEnd = a1 - common, bEnd = b1 - common;
        if (a0 == aEnd)
            for (int j = b0; j < bEnd; ++j)
                emit('+', -1, j);
        else if (b0 == bEnd)
            for (int i = a0; i < aEnd; ++i)
                emit('-', i, -1);
        else
            bisect(a0, aEnd, b0, bEnd);
        for (int k = 0; k < common; ++k)
            emit('=', aEnd + k, bEnd + k);
    }

private:
    // Past this many differences in one stretch the stretch is shown as replaced whole: still a true diff, only not
    // the shortest, and it keeps a preview of two unrelated files quick.
    static const int kMostDifferences = 20000;

    void emit(char kind, int before, int after)
    {
        Edit edit;
        edit.kind = kind;
        edit.before = before;
        edit.after = after;
        m_out.push_back(edit);
    }

    void bisect(int a0, int a1, int b0, int b1)
    {
        const int n = a1 - a0, m = b1 - b0;
        const int maxD = (n + m + 1) / 2;
        const int offset = maxD;
        const int length = 2 * maxD;
        std::vector<int> v1(static_cast<size_t>(length) + 2, -1), v2(static_cast<size_t>(length) + 2, -1);
        v1[offset + 1] = 0;
        v2[offset + 1] = 0;
        const int delta = n - m;
        const bool front = (delta % 2) != 0;
        int k1start = 0, k1end = 0, k2start = 0, k2end = 0;
        for (int d = 0; d < maxD && d < kMostDifferences; ++d)
        {
            for (int k1 = -d + k1start; k1 <= d - k1end; k1 += 2)
            {
                const int k1o = offset + k1;
                int x1 = (k1 == -d || (k1 != d && v1[k1o - 1] < v1[k1o + 1])) ? v1[k1o + 1] : v1[k1o - 1] + 1;
                int y1 = x1 - k1;
                while (x1 < n && y1 < m && m_a[a0 + x1] == m_b[b0 + y1])
                {
                    ++x1;
                    ++y1;
                }
                v1[k1o] = x1;
                if (x1 > n)
                    k1end += 2;
                else if (y1 > m)
                    k1start += 2;
                else if (front)
                {
                    const int k2o = offset + delta - k1;
                    if (k2o >= 0 && k2o < length && v2[k2o] != -1 && x1 >= n - v2[k2o])
                    {
                        split(a0, a1, b0, b1, x1, y1);
                        return;
                    }
                }
            }
            for (int k2 = -d + k2start; k2 <= d - k2end; k2 += 2)
            {
                const int k2o = offset + k2;
                int x2 = (k2 == -d || (k2 != d && v2[k2o - 1] < v2[k2o + 1])) ? v2[k2o + 1] : v2[k2o - 1] + 1;
                int y2 = x2 - k2;
                while (x2 < n && y2 < m && m_a[a0 + n - x2 - 1] == m_b[b0 + m - y2 - 1])
                {
                    ++x2;
                    ++y2;
                }
                v2[k2o] = x2;
                if (x2 > n)
                    k2end += 2;
                else if (y2 > m)
                    k2start += 2;
                else if (!front)
                {
                    const int k1o = offset + delta - k2;
                    if (k1o >= 0 && k1o < length && v1[k1o] != -1)
                    {
                        const int x1 = v1[k1o];
                        const int y1 = offset + x1 - k1o;
                        if (x1 >= n - x2)
                        {
                            split(a0, a1, b0, b1, x1, y1);
                            return;
                        }
                    }
                }
            }
        }
        for (int i = a0; i < a1; ++i)
            emit('-', i, -1);
        for (int j = b0; j < b1; ++j)
            emit('+', -1, j);
    }

    void split(int a0, int a1, int b0, int b1, int x, int y)
    {
        run(a0, a0 + x, b0, b0 + y);
        run(a0 + x, a1, b0 + y, b1);
    }

    const std::vector<int>& m_a;
    const std::vector<int>& m_b;
    std::vector<Edit>&      m_out;
};

// A line's text without its ending.
std::string shown(const std::string& line)
{
    size_t end = line.size();
    if (end > 0 && line[end - 1] == '\n')
        --end;
    if (end > 0 && line[end - 1] == '\r')
        --end;
    return line.substr(0, end);
}

bool endsLine(const std::string& line)
{
    return !line.empty() && line[line.size() - 1] == '\n';
}

} // namespace

TextDiff diffTexts(const std::string& before, const std::string& after, int context)
{
    TextDiff diff;
    const std::vector<std::string> a = splitLines(before), b = splitLines(after);
    std::unordered_map<std::string, int> numbers;
    std::vector<int> an(a.size()), bn(b.size());
    for (size_t i = 0; i < a.size(); ++i)
        an[i] = numbers.emplace(a[i], static_cast<int>(numbers.size())).first->second;
    for (size_t j = 0; j < b.size(); ++j)
        bn[j] = numbers.emplace(b[j], static_cast<int>(numbers.size())).first->second;
    std::vector<Edit> edits;
    Differ(an, bn, edits).run(0, static_cast<int>(an.size()), 0, static_cast<int>(bn.size()));

    if (context < 0)
        context = 0;
    size_t i = 0;
    while (i < edits.size())
    {
        if (edits[i].kind == '=')
        {
            ++i;
            continue;
        }
        // A hunk: from `context` unchanged lines before this change to `context` after the last change closer than
        // twice that to the one before it.
        size_t first = i;
        for (int c = 0; c < context && first > 0 && edits[first - 1].kind == '='; ++c)
            --first;
        size_t last = i;
        size_t scan = i;
        while (scan < edits.size())
        {
            if (edits[scan].kind != '=')
            {
                last = scan;
                ++scan;
                continue;
            }
            size_t run = scan;
            while (run < edits.size() && edits[run].kind == '=')
                ++run;
            if (run < edits.size() && static_cast<int>(run - scan) <= 2 * context)
            {
                scan = run;
                continue;
            }
            break;
        }
        size_t end = last + 1;
        for (int c = 0; c < context && end < edits.size() && edits[end].kind == '='; ++c)
            ++end;

        DiffHunk hunk;
        int beforeFirst = -1, afterFirst = -1;
        int beforeSeen = 0, afterSeen = 0;      // old and new lines ahead of the hunk
        for (size_t k = 0; k < first; ++k)
        {
            if (edits[k].kind != '+')
                ++beforeSeen;
            if (edits[k].kind != '-')
                ++afterSeen;
        }
        for (size_t k = first; k < end; ++k)
        {
            const Edit& e = edits[k];
            DiffLine line;
            line.kind = e.kind == '=' ? ' ' : e.kind;
            const std::string& raw = e.kind == '+' ? b[static_cast<size_t>(e.after)] : a[static_cast<size_t>(e.before)];
            line.text = shown(raw);
            hunk.lines.push_back(line);
            if (e.kind != '+')
            {
                if (beforeFirst < 0)
                    beforeFirst = e.before;
                ++hunk.beforeCount;
            }
            if (e.kind != '-')
            {
                if (afterFirst < 0)
                    afterFirst = e.after;
                ++hunk.afterCount;
            }
            if (e.kind == '-')
                ++diff.removed;
            else if (e.kind == '+')
                ++diff.added;
            if (!endsLine(raw))
            {
                DiffLine note;
                note.kind = '\\';
                note.text = " No newline at end of file";
                hunk.lines.push_back(note);
            }
        }
        hunk.beforeStart = hunk.beforeCount ? beforeFirst + 1 : beforeSeen;
        hunk.afterStart = hunk.afterCount ? afterFirst + 1 : afterSeen;
        diff.hunks.push_back(hunk);
        i = end;
    }
    return diff;
}

std::string unifiedDiff(const TextDiff& diff, const std::string& beforeName, const std::string& afterName)
{
    std::string text = "--- " + beforeName + "\n+++ " + afterName + "\n";
    for (size_t h = 0; h < diff.hunks.size(); ++h)
    {
        const DiffHunk& hunk = diff.hunks[h];
        char header[96];
        std::snprintf(header, sizeof(header), "@@ -%d,%d +%d,%d @@\n", hunk.beforeStart, hunk.beforeCount, hunk.afterStart,
                      hunk.afterCount);
        text += header;
        for (size_t l = 0; l < hunk.lines.size(); ++l)
            text += std::string(1, hunk.lines[l].kind) + hunk.lines[l].text + "\n";
    }
    return text;
}

bool looksLikeText(const std::string& bytes)
{
    return bytes.find('\0') == std::string::npos;
}

} // namespace bf2editor
