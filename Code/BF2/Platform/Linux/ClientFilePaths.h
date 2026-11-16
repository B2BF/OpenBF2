// [CHOSEN, not recovered] Native Linux client physical paths, issue #36.
// FileManager keeps its archive/search order. This boundary gives the Windows content's
// case-insensitive names their existing disk spelling on case-sensitive Linux filesystems.
#ifndef BF2_LINUX_CLIENTFILEPATHS_H
#define BF2_LINUX_CLIENTFILEPATHS_H

#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <string>

namespace dice { namespace hfe { namespace linuxclient {

inline bool samePathComponent(const std::string& left, const char* right)
{
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (!right[i]) return false;
        unsigned char a = static_cast<unsigned char>(left[i]);
        unsigned char b = static_cast<unsigned char>(right[i]);
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return false;
    }
    return right[left.size()] == 0;
}

// Normalizes separators and resolves existing components, preferring an exact match. A missing
// suffix retains its spelling for file creation. Ambiguous case-only matches produce an empty
// path and a diagnostic, so the OS call fails instead of choosing an arbitrary asset.
inline void resolveClientPath(std::string& path)
{
    for (char& c : path) if (c == '\\') c = '/';
    if (path.empty()) return;
    struct stat info;
    if (lstat(path.c_str(), &info) == 0) return;

    std::string resolved = path[0] == '/' ? "/" : "";
    std::size_t at = path[0] == '/' ? 1 : 0;
    while (at < path.size()) {
        const std::size_t end = path.find('/', at);
        const std::size_t count = end == std::string::npos ? path.size() - at : end - at;
        const std::string component = path.substr(at, count);
        std::string actual = component;
        const std::string candidate = resolved + component;
        if (!component.empty() && component != "." && component != ".." && lstat(candidate.c_str(), &info) != 0) {
            DIR* directory = opendir(resolved.empty() ? "." : resolved.c_str());
            if (directory) {
                std::string match;
                bool ambiguous = false;
                while (const dirent* entry = readdir(directory)) {
                    if (!samePathComponent(component, entry->d_name)) continue;
                    if (!match.empty()) { ambiguous = true; break; }
                    match = entry->d_name;
                }
                closedir(directory);
                if (ambiguous) {
                    std::fprintf(stderr, "[Linux paths] ambiguous case-insensitive path: %s\n", candidate.c_str());
                    std::fflush(stderr);
                    path.clear();
                    return;
                }
                if (!match.empty()) actual = match;
                else { path = resolved + path.substr(at); return; }
            } else { path = resolved + path.substr(at); return; }
        }
        resolved += actual;
        if (end == std::string::npos) break;
        resolved += '/';
        at = end + 1;
    }
    path.swap(resolved);
}

} } }
#endif
