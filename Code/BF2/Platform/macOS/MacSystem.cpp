// [CHOSEN, not recovered] macOS port: see MacSystem.h.
#include "MacSystem.h"

#if (defined(__APPLE__) || defined(BF2_LINUX_CLIENT)) && !(defined(DEDICATED) && DEDICATED)

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <crt_externs.h>     // _NSGetEnviron
#include <mach-o/dyld.h>     // _NSGetExecutablePath
#else
#include <SDL3/SDL.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <iconv.h>
extern char** environ;
#endif
#include <spawn.h>
#include <sys/types.h>
#include <cstdint>
#include <vector>

namespace dice {
namespace hfe {
namespace macos {

std::vector<std::string> splitCommandLine(const std::string& commandLine)
{
    std::vector<std::string> args;
    const char* p = commandLine.c_str();
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            ++p;
        if (!*p)
            break;
        std::string arg;
        bool quoted = false;
        while (*p && (quoted || !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))) {
            if (*p == '\\') {
                std::size_t slashes = 0;
                while (*p == '\\') { ++slashes; ++p; }
                if (*p == '"') {
                    arg.append(slashes / 2, '\\');
                    if (slashes % 2) { arg += '"'; ++p; }
                } else {
                    arg.append(slashes, '\\');
                }
                continue;
            }
            if (*p == '"') {
                if (quoted && p[1] == '"') { arg += '"'; p += 2; continue; }
                quoted = !quoted;
                ++p;
                continue;
            }
            arg += *p++;
        }
        args.push_back(arg);
    }
    return args;
}

bool relaunchExecutable(const std::string& commandLine)
{
#if defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(0, &size);
    std::vector<char> path(size + 1, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0)
        return false;
#else
    // [CHOSEN, not recovered] Linux uses its own executable and the same argv splitting policy.
    std::vector<char> path(256);
    for (;;) {
        const ssize_t size = readlink("/proc/self/exe", path.data(), path.size());
        if (size < 0) return false;
        if (static_cast<std::size_t>(size) < path.size()) { path[size] = 0; break; }
        path.resize(path.size() * 2);
    }
#endif
    std::vector<std::string> args = splitCommandLine(commandLine);
    std::vector<char*> argv;
    argv.push_back(path.data());
    for (std::size_t i = 0; i < args.size(); ++i)
        argv.push_back(&args[i][0]);
    argv.push_back(0);
    pid_t child = 0;
#if defined(__APPLE__)
    return posix_spawn(&child, path.data(), 0, 0, argv.data(), *_NSGetEnviron()) == 0;
#else
    return posix_spawn(&child, path.data(), 0, 0, argv.data(), environ) == 0;
#endif
}

void showMessageBox(const char* text, const char* caption)
{
#if defined(__APPLE__)
    CFStringRef title = CFStringCreateWithCString(kCFAllocatorDefault, caption ? caption : "",
                                                  kCFStringEncodingWindowsLatin1);
    CFStringRef message = CFStringCreateWithCString(kCFAllocatorDefault, text ? text : "",
                                                    kCFStringEncodingWindowsLatin1);
    CFOptionFlags response = 0;
    CFUserNotificationDisplayAlert(0, kCFUserNotificationPlainAlertLevel, 0, 0, 0,
                                   title, message, CFSTR("OK"), 0, 0, &response);
    if (message) CFRelease(message);
    if (title) CFRelease(title);
#else
    // [CHOSEN, not recovered] SDL dialog strings use UTF-8; engine strings use Windows-1252.
    auto utf8 = [](const char* text) {
        const std::string input = text ? text : "";
        iconv_t converter = iconv_open("UTF-8", "WINDOWS-1252");
        if (converter == reinterpret_cast<iconv_t>(-1)) return input;
        std::vector<char> output(input.size() * 4 + 1, 0);
        char* source = const_cast<char*>(input.c_str());
        char* destination = output.data();
        std::size_t sourceLeft = input.size(), destinationLeft = output.size() - 1;
        const bool converted = iconv(converter, &source, &sourceLeft, &destination, &destinationLeft) != static_cast<std::size_t>(-1);
        iconv_close(converter);
        return converted ? std::string(output.data()) : input;
    };
    const std::string title = utf8(caption), message = utf8(text);
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title.c_str(), message.c_str(), nullptr)) {
        std::fprintf(stderr, "[SDL dialog] %s: %s (%s)\n", title.c_str(), message.c_str(), SDL_GetError());
        std::fflush(stderr);
    }
#endif
}

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // __APPLE__ && !DEDICATED
