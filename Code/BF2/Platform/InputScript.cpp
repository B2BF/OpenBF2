// [TEMP] [CHOSEN, not recovered] Client test tool: the platform-independent script engine
// (InputScript.h, docs/macos-c-input-script.md). The hosts and their key tables live in
// Platform/macOS (CocoaHost.mm, MacInputScript.mm) and Platform/Windows (WinInputScript.cpp).
// [RETIRE-WHEN: client-test-automation-replaced]
#include "InputScript.h"
#if !DEDICATED && (defined(__APPLE__) || defined(_WIN32))
#if defined(_WIN32)
#include <windows.h>                        // the redirected stdout file (GetFinalPathNameByHandleA)
#else
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#endif
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace dice {
namespace hfe {
namespace inputscript {

namespace {

double nowSeconds()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Key by name: an IDKey_ name (prefix optional, any case) from the host table, or 0xNN.
bool lookupKey(std::string name, unsigned short& code)
{
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (name.compare(0, 6, "idkey_") == 0) name = name.substr(6);
    if (name.size() > 2 && name[0] == '0' && name[1] == 'x')
    {
        code = static_cast<unsigned short>(std::strtoul(name.c_str(), nullptr, 16));
        return true;
    }
    return keys::lookup(name, code);
}

std::u16string utf16FromUtf8(const std::string& s)
{
    std::u16string out;
    for (std::size_t i = 0; i < s.size();)
    {
        unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned int cp = c; std::size_t n = 1;
        if (c >= 0xF0 && i + 3 < s.size() + 0) { cp = ((c & 7u) << 18) | ((s[i+1] & 0x3Fu) << 12) | ((s[i+2] & 0x3Fu) << 6) | (s[i+3] & 0x3Fu); n = 4; }
        else if (c >= 0xE0 && i + 2 < s.size()) { cp = ((c & 0xFu) << 12) | ((s[i+1] & 0x3Fu) << 6) | (s[i+2] & 0x3Fu); n = 3; }
        else if (c >= 0xC0 && i + 1 < s.size()) { cp = ((c & 0x1Fu) << 6) | (s[i+1] & 0x3Fu); n = 2; }
        if (cp < 0x10000) out.push_back(static_cast<char16_t>(cp));
        i += n;
    }
    return out;
}

std::string trim(const std::string& s)
{
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// The rest of a line, without surrounding quotes.
std::string restArgument(const std::string& rest)
{
    std::string t = trim(rest);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"')
        t = t.substr(1, t.size() - 2);
    return t;
}

// The file the harness redirects stdout (else stderr) into, or empty.
std::string redirectedOutputPath()
{
#if defined(_WIN32)
    for (DWORD which : {STD_OUTPUT_HANDLE, STD_ERROR_HANDLE})
    {
        const HANDLE h = GetStdHandle(which);
        char buf[MAX_PATH * 2];
        if (h && h != INVALID_HANDLE_VALUE && GetFileType(h) == FILE_TYPE_DISK)
        {
            const DWORD n = GetFinalPathNameByHandleA(h, buf, sizeof(buf), FILE_NAME_NORMALIZED);
            if (n > 0 && n < sizeof(buf))
            {
                std::string path(buf, n);
                if (path.compare(0, 4, "\\\\?\\") == 0)   // the \\?\ prefix
                    path = path.substr(4);
                return path;
            }
        }
    }
#else
    for (int fd : {1, 2})
    {
        struct stat st;
        char buf[MAXPATHLEN];
        if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && fcntl(fd, F_GETPATH, buf) == 0)
            return buf;
    }
#endif
    return std::string();
}

const int kHoldMs = 60;                     // a key or button held at least this long ...
const int kHoldFrames = 3;                  // ... and over this many pumps (input samples)

} // namespace

namespace { ScriptQueryFn s_scriptQuery = nullptr; }
void setScriptQuery(ScriptQueryFn fn) { s_scriptQuery = fn; }

InputScript& inputScript()
{
    static InputScript script;
    return script;
}

#if defined(__APPLE__)
// BF2_HITCH_TRACE (Platform/macOS/HitchTrace.cpp) records each report line as a labelled moment
// of its frame trace. This weak default serves programs built without the trace.
extern "C" __attribute__((weak)) void openbf2HitchNote(const char*) {}
#endif

void InputScript::report(const char* fmt, ...) const
{
    char text[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    std::fprintf(stderr, "[TEMP] [INPUT-SCRIPT] t=%.2fs frame=%lu %s\n",
                 nowSeconds() - m_startSeconds, m_frame, text);
#if defined(__APPLE__)
    openbf2HitchNote(text);
#endif
}

void InputScript::finish(const char* why)
{
    m_finished = true;
    report("%s", why);
}

void InputScript::addWaitFrames(int frames, int line)
{
    Step s{}; s.op = OpWaitFrames; s.a = frames; s.line = line; m_steps.push_back(s);
}

void InputScript::addWaitMs(int ms, int line)
{
    Step s{}; s.op = OpWaitMs; s.a = ms; s.line = line; m_steps.push_back(s);
}

void InputScript::addKeyPress(unsigned short keyCode, bool shift, const std::u16string& text, int line, int holdMs)
{
    if (shift)
    {
        Step d{}; d.op = OpKeyDown; d.a = keys::leftShift(); d.line = line; m_steps.push_back(d);
        addWaitFrames(1, line);
    }
    Step d{}; d.op = OpKeyDown; d.a = keyCode; d.chars = text; d.line = line; m_steps.push_back(d);
    addWaitFrames(kHoldFrames, line);
    addWaitMs(holdMs, line);
    Step u{}; u.op = OpKeyUp; u.a = keyCode; u.line = line; m_steps.push_back(u);
    if (shift)
    {
        addWaitFrames(1, line);
        Step su{}; su.op = OpKeyUp; su.a = keys::leftShift(); su.line = line; m_steps.push_back(su);
    }
    addWaitFrames(2, line);
}

bool InputScript::parseLine(const std::string& raw, int number)
{
    std::string line = trim(raw);
    if (line.empty() || line[0] == '#')
        return true;
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    std::string rest;
    std::getline(in, rest);
    std::istringstream args(rest);
    Step s{};
    s.line = number;
    auto key = [&](const std::string& name, unsigned short& code) {
        if (lookupKey(name, code)) return true;
        report("line %d: unknown key '%s'", number, name.c_str());
        return false;
    };
    if (cmd == "wait-frames") { args >> s.a; addWaitFrames(s.a, number); }
    else if (cmd == "wait-ms") { args >> s.a; addWaitMs(s.a, number); }
    else if (cmd == "wait-log") { s.op = OpWaitLog; s.text = restArgument(rest); m_steps.push_back(s); }
    else if (cmd == "timeout") { s.op = OpTimeout; args >> s.x; m_steps.push_back(s); }
    else if (cmd == "wait-log-else")
    {
        // wait-log-else SECONDS LABEL TEXT: as wait-log, but after SECONDS jump to `label LABEL`.
        args >> s.x >> s.label;
        std::string text;
        std::getline(args, text);
        s.op = OpWaitLogElse; s.text = restArgument(text); m_steps.push_back(s);
    }
    else if (cmd == "label") { s.op = OpLabel; args >> s.label; m_steps.push_back(s); }
    else if (cmd == "key" || cmd == "key-down" || cmd == "key-up")
    {
        std::string name; int hold = kHoldMs;
        args >> name >> hold;
        unsigned short code;
        if (!key(name, code)) return false;
        if (cmd == "key")
        {
            // The press carries the characters NSEvent would: computed at run time (layout).
            Step d{}; d.op = OpKeyDown; d.a = code; d.b = 1; d.line = number; m_steps.push_back(d);
            addWaitFrames(kHoldFrames, number);
            addWaitMs(hold, number);
            Step u{}; u.op = OpKeyUp; u.a = code; u.line = number; m_steps.push_back(u);
            addWaitFrames(2, number);
        }
        else
        {
            s.op = cmd == "key-down" ? OpKeyDown : OpKeyUp; s.a = code; s.b = 1; m_steps.push_back(s);
        }
    }
    else if (cmd == "type") { s.op = OpText; s.chars = utf16FromUtf8(restArgument(rest)); m_steps.push_back(s); }
    else if (cmd == "console")
    {
        // The shipped console: the key left of 1 toggles it, Enter executes the line.
        const std::string text = restArgument(rest);
        Step k{}; k.op = OpKeyDown; k.a = keys::grave(); k.b = 1; k.line = number; m_steps.push_back(k);
        addWaitFrames(kHoldFrames, number); addWaitMs(kHoldMs, number);
        Step ku{}; ku.op = OpKeyUp; ku.a = keys::grave(); ku.line = number; m_steps.push_back(ku);
        addWaitFrames(10, number);
        Step t{}; t.op = OpText; t.chars = utf16FromUtf8(text); t.line = number; m_steps.push_back(t);
        Step e{}; e.op = OpKeyDown; e.a = keys::enter(); e.b = 1; e.line = number; m_steps.push_back(e);
        addWaitFrames(kHoldFrames, number); addWaitMs(kHoldMs, number);
        Step eu{}; eu.op = OpKeyUp; eu.a = keys::enter(); eu.line = number; m_steps.push_back(eu);
        addWaitFrames(10, number);
        m_steps.push_back(k); addWaitFrames(kHoldFrames, number); addWaitMs(kHoldMs, number);
        m_steps.push_back(ku); addWaitFrames(5, number);
    }
    else if (cmd == "move") { s.op = OpMoveTo; args >> s.x >> s.y; m_steps.push_back(s); }
    else if (cmd == "move-rel") { s.op = OpMoveBy; args >> s.x >> s.y; m_steps.push_back(s); }
    else if (cmd == "button-down" || cmd == "button-up")
    {
        s.op = cmd == "button-down" ? OpButtonDown : OpButtonUp; args >> s.a; m_steps.push_back(s);
    }
    else if (cmd == "click")
    {
        // click X Y [button]: move there, settle, press, hold, release.
        float x = 0, y = 0; int button = 0;
        args >> x >> y >> button;
        Step m{}; m.op = OpMoveTo; m.x = x; m.y = y; m.line = number; m_steps.push_back(m);
        addWaitFrames(5, number);
        Step d{}; d.op = OpButtonDown; d.a = button; d.line = number; m_steps.push_back(d);
        addWaitFrames(kHoldFrames, number); addWaitMs(kHoldMs, number);
        Step u{}; u.op = OpButtonUp; u.a = button; u.line = number; m_steps.push_back(u);
        addWaitFrames(5, number);
    }
    else if (cmd == "pointer-scale") { args >> m_pointerScale; if (m_pointerScale <= 0) m_pointerScale = 1.2f; }
    else if (cmd == "pointer" || cmd == "hud-click")
    {
        // The in-game HUD pointer (BF2HudManager::handleGameInput) moves by mapped relative mouse
        // input and clamps to the 800x600 HUD space, so it is homed to the top-left corner and then
        // moved by X/scale, Y/scale counts (pointer-scale: HUD pixels per count, default 1.2 with
        // the shipped Controls.con and hudManager.setPointerMouseSensitivity 40).
        float x = 0, y = 0; int button = 0;
        args >> x >> y >> button;
        Step home{}; home.op = OpMoveBy; home.x = -4000; home.y = -4000; home.line = number; m_steps.push_back(home);
        addWaitFrames(4, number);
        Step to{}; to.op = OpMoveBy; to.x = x / m_pointerScale; to.y = y / m_pointerScale; to.line = number; m_steps.push_back(to);
        addWaitFrames(4, number);
        if (cmd == "hud-click")
        {
            Step d{}; d.op = OpButtonDown; d.a = button; d.line = number; m_steps.push_back(d);
            addWaitFrames(kHoldFrames, number); addWaitMs(kHoldMs, number);
            Step u{}; u.op = OpButtonUp; u.a = button; u.line = number; m_steps.push_back(u);
            addWaitFrames(5, number);
        }
    }
    else if (cmd == "aim" || cmd == "face" || cmd == "walk-to")
    {
        // aim TARGET [tol-deg] [timeout-ms]; face TARGET [tol-deg] [timeout-ms] (yaw only);
        // walk-to TARGET RADIUS [timeout-ms] (W held, yaw steered, until within RADIUS metres).
        std::string target; float p1 = 0, p2 = 0;
        args >> target >> p1 >> p2;
        s.op = cmd == "aim" ? OpAim : cmd == "face" ? OpFace : OpWalkTo;
        s.text = target;
        s.x = p1 > 0 ? p1 : (s.op == OpWalkTo ? 3.0f : 2.0f);
        s.y = p2 > 0 ? p2 : (s.op == OpWalkTo ? 30000.0f : 5000.0f);
        m_steps.push_back(s);
    }
    else if (cmd == "wheel") { s.op = OpWheel; args >> s.a; m_steps.push_back(s); }
    else if (cmd == "capture") { s.op = OpCapture; s.text = restArgument(rest); m_steps.push_back(s); }
    else if (cmd == "log") { s.op = OpLog; s.text = restArgument(rest); m_steps.push_back(s); }
    else if (cmd == "quit") { s.op = OpQuit; m_steps.push_back(s); }
    else if (cmd == "end") { s.op = OpEnd; m_steps.push_back(s); }
    else
    {
        report("line %d: unknown command '%s'", number, cmd.c_str());
        return false;
    }
    return true;
}

bool InputScript::parse(const std::string& path)
{
    std::ifstream in(path.c_str());
    if (!in)
    {
        report("cannot read %s; script off", path.c_str());
        return false;
    }
    std::string line;
    int number = 0;
    while (std::getline(in, line))
        if (!parseLine(line, ++number))
            return false;
    return true;
}

bool InputScript::loadFromEnvironment()
{
    const char* path = std::getenv("BF2_INPUT_SCRIPT");
    if (!path || !*path)
        return false;
    m_startSeconds = nowSeconds();
    if (!parse(path))
        return false;
    if (const char* log = std::getenv("BF2_INPUT_SCRIPT_LOG"))
        m_logPath = log;
    else
        m_logPath = redirectedOutputPath();
    m_active = true;
    report("loaded %s: %zu steps; log %s", path, m_steps.size(),
           m_logPath.empty() ? "(none: wait-log unavailable)" : m_logPath.c_str());
    return true;
}

bool InputScript::logContains(const std::string& needle)
{
    if (m_logPath.empty())
        return false;
    std::fflush(stdout);
    std::fflush(stderr);
    std::ifstream log(m_logPath.c_str(), std::ios::binary);
    if (!log)
        return false;
    char buf[65536];
    log.seekg(static_cast<std::streamoff>(m_logOffset));
    while (log)
    {
        log.read(buf, sizeof(buf));
        const std::streamsize n = log.gcount();
        if (n <= 0)
            break;
        m_logOffset += n;
        m_logTail.append(buf, static_cast<std::size_t>(n));
    }
    log.close();
    // The script's own report lines quote the needle; they never match.
    for (std::size_t at = m_logTail.find(needle); at != std::string::npos;
         at = m_logTail.find(needle, at + 1))
    {
        const std::size_t begin = m_logTail.rfind('\n', at);
        const std::size_t lineStart = begin == std::string::npos ? 0 : begin + 1;
        const std::size_t lineEnd = m_logTail.find('\n', at);
        const std::string line = m_logTail.substr(lineStart, lineEnd == std::string::npos
                                                                 ? std::string::npos : lineEnd - lineStart);
        if (line.find("[INPUT-SCRIPT]") != std::string::npos)
            continue;
        m_logTail.erase(0, at + needle.size());   // the next wait-log matches after this one
        return true;
    }
    // Keep the scan bounded; a needle never spans more than its own length.
    if (m_logTail.size() > (1u << 20))
        m_logTail.erase(0, m_logTail.size() - needle.size());
    return false;
}

bool InputScript::takeCaptureRequest(std::string& label)
{
    if (m_captures.empty())
        return false;
    label = m_captures.front();
    m_captures.erase(m_captures.begin());
    return true;
}

bool InputScript::expandText(const std::u16string& text, int line)
{
    std::vector<Step> expanded;
    std::vector<Step> saved;
    saved.swap(m_steps);
    for (char16_t c : text)
    {
        unsigned short found = 0xFFFF;
        bool shift = false;
        for (unsigned short code = 0; code < 0x80 && found == 0xFFFF; ++code)
        {
            if (keys::isModifier(code))
                continue;
            for (int sh = 0; sh < 2; ++sh)
            {
                const std::u16string chars = m_sink->scriptKeyCharacter(code, sh != 0);
                if (chars.size() == 1 && chars[0] == c) { found = code; shift = sh != 0; break; }
            }
        }
        if (found == 0xFFFF)
        {
            m_steps.swap(saved);
            report("line %d: no key produces U+%04X on this layout", line, static_cast<unsigned>(c));
            return false;
        }
        addKeyPress(found, shift, std::u16string(1, c), line, 20);
    }
    expanded.swap(m_steps);
    m_steps.swap(saved);
    m_steps.insert(m_steps.begin() + static_cast<std::ptrdiff_t>(m_pc) + 1, expanded.begin(), expanded.end());
    return true;
}

void InputScript::tick(IInputScriptSink& sink)
{
    if (!m_active)
        return;
    ++m_frame;
    if (m_finished)
        return;
    m_sink = &sink;
    while (m_pc < m_steps.size())
    {
        Step& s = m_steps[m_pc];
        switch (s.op)
        {
        case OpWaitFrames:
        case OpWaitMs:
        case OpWaitLog:
        case OpWaitLogElse:
        {
            if (!m_waiting)
            {
                m_waiting = true;
                m_waitFrame = m_frame;
                m_waitStart = nowSeconds();
                if (s.op == OpWaitLog || s.op == OpWaitLogElse)
                    report("line %d: wait-log \"%s\"", s.line, s.text.c_str());
            }
            bool done = false;
            if (s.op == OpWaitFrames) done = m_frame - m_waitFrame >= static_cast<unsigned long>(s.a);
            else if (s.op == OpWaitMs) done = (nowSeconds() - m_waitStart) * 1000.0 >= s.a;
            else done = logContains(s.text);
            if (!done && s.op == OpWaitLogElse && nowSeconds() - m_waitStart > s.x)
            {
                size_t target = m_pc + 1;
                while (target < m_steps.size() && !(m_steps[target].op == OpLabel && m_steps[target].label == s.label))
                    ++target;
                report("line %d: no \"%s\" after %.0f s; jumping to label %s", s.line, s.text.c_str(), s.x, s.label.c_str());
                m_waiting = false;
                m_pc = target;   // the label step itself is a no-op
                return;
            }
            if (!done)
            {
                if ((s.op == OpWaitLog || s.op == OpWaitLogElse) && nowSeconds() - m_waitStart > m_timeoutSeconds)
                {
                    char why[512];
                    std::snprintf(why, sizeof(why), "line %d: TIMEOUT after %.0f s waiting for \"%s\"; script stopped",
                                  s.line, m_timeoutSeconds, s.text.c_str());
                    finish(why);
                }
                return;
            }
            if (s.op == OpWaitLog || s.op == OpWaitLogElse)
                report("line %d: matched \"%s\"", s.line, s.text.c_str());
            m_waiting = false;
            break;
        }
        case OpLabel:
            break;
        case OpTimeout:
            m_timeoutSeconds = s.x > 0 ? s.x : 180.0;
            break;
        case OpKeyDown:
        {
            std::u16string text = s.chars;
            if (s.b == 1 && text.empty())
            {
                text = keys::controlCharacters(static_cast<unsigned short>(s.a));
                if (text.empty() && !keys::isModifier(static_cast<unsigned short>(s.a)))
                    text = sink.scriptKeyCharacter(static_cast<unsigned short>(s.a), m_shiftHeld);
            }
            if (keys::isShift(static_cast<unsigned short>(s.a))) m_shiftHeld = true;
            sink.scriptKey(static_cast<unsigned short>(s.a), true, text);
            break;
        }
        case OpKeyUp:
            if (keys::isShift(static_cast<unsigned short>(s.a))) m_shiftHeld = false;
            sink.scriptKey(static_cast<unsigned short>(s.a), false, std::u16string());
            break;
        case OpText:
            if (!expandText(s.chars, s.line))
            {
                finish("text expansion failed; script stopped");
                return;
            }
            break;
        case OpMoveTo:     sink.scriptMouseTo(s.x, s.y); break;
        case OpMoveBy:     sink.scriptMouseBy(s.x, s.y); break;
        case OpButtonDown: sink.scriptButton(s.a, true); break;
        case OpButtonUp:   sink.scriptButton(s.a, false); break;
        case OpWheel:      sink.scriptWheel(s.a * 120); break;
        case OpCapture:
            m_captures.push_back(s.text.empty() ? std::string("capture") : s.text);
            report("line %d: capture %s", s.line, m_captures.back().c_str());
            break;
        case OpLog:
            report("line %d: %s", s.line, s.text.c_str());
            break;
        case OpQuit:
            report("line %d: quit", s.line);
            ++m_pc;
            m_finished = true;
            sink.scriptQuit();
            return;
        case OpEnd:
            m_pc = m_steps.size();
            continue;
        case OpAim:
        case OpFace:
        case OpWalkTo:
            if (!m_waiting)
            {
                m_waiting = true;
                m_waitStart = nowSeconds();
                m_haveLast = false;
                m_steerFrame = 0;
            }
            if (!steer(s, sink))
                return;
            m_waiting = false;
            break;
        }
        ++m_pc;
    }
    finish("script complete");
}

bool InputScript::steer(Step& s, IInputScriptSink& sink)
{
    const double elapsedMs = (nowSeconds() - m_waitStart) * 1000.0;
    auto release = [&]() {
        if (m_walkKeyDown) { sink.scriptKey(keys::w(), false, std::u16string()); m_walkKeyDown = false; }
    };
    float view[3], fwd[3], target[3];
    bool ok = s_scriptQuery && s_scriptQuery(s.text.c_str(), view, fwd, target);
    if (!ok)
    {
        if (elapsedMs > s.y)
        {
            release();
            report("line %d: %s %s: no target / no local player; step skipped", s.line,
                   s.op == OpWalkTo ? "walk-to" : s.op == OpAim ? "aim" : "face", s.text.c_str());
            return true;
        }
        return false;
    }
    const float dx = target[0] - view[0], dy = target[1] - view[1], dz = target[2] - view[2];
    const float horizontal = std::sqrt(dx * dx + dz * dz);
    const float yaw = std::atan2(fwd[0], fwd[2]);
    const float pitch = std::atan2(fwd[1], std::sqrt(fwd[0] * fwd[0] + fwd[2] * fwd[2]));
    auto wrap = [](float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; };
    const float yawErr = wrap(std::atan2(dx, dz) - yaw);
    const float pitchErr = s.op == OpAim ? std::atan2(dy, horizontal) - pitch : 0.0f;
    const float tol = s.x * 3.14159265f / 180.0f;
    const char* name = s.op == OpWalkTo ? "walk-to" : s.op == OpAim ? "aim" : "face";
    // Learn counts per radian from the last correction (sign included).
    if (m_haveLast)
    {
        const float dYaw = wrap(yaw - m_lastYaw), dPitch = pitch - m_lastPitch;
        if (std::fabs(m_lastDx) > 5 && std::fabs(dYaw) > 0.003f) m_gainX = 0.5f * m_gainX + 0.5f * (m_lastDx / dYaw);
        if (std::fabs(m_lastDy) > 5 && std::fabs(dPitch) > 0.003f) m_gainY = 0.5f * m_gainY + 0.5f * (m_lastDy / dPitch);
    }
    bool done;
    if (s.op == OpWalkTo)
        done = horizontal <= s.x;
    else
        done = std::fabs(yawErr) <= tol && std::fabs(pitchErr) <= tol;
    if (done || elapsedMs > s.y)
    {
        release();
        report("line %d: %s %s: %s; dist %.1f m (horizontal %.1f, dy %.1f), yaw err %.1f deg, pitch err %.1f deg, gain %.0f/%.0f counts/rad",
               s.line, name, s.text.c_str(), done ? "reached" : "TIMEOUT", std::sqrt(dx * dx + dy * dy + dz * dz),
               horizontal, dy, yawErr * 57.2958f, pitchErr * 57.2958f, m_gainX, m_gainY);
        return true;
    }
    if (s.op == OpWalkTo && !m_walkKeyDown)
    {
        sink.scriptKey(keys::w(), true, u"w");
        m_walkKeyDown = true;
    }
    // One correction every 4 pumps (the view follows the input a frame or two later).
    if (m_steerFrame && m_frame - m_steerFrame < 4)
        return false;
    m_steerFrame = m_frame;
    auto clampf = [](float v, float lim) { return v > lim ? lim : v < -lim ? -lim : v; };
    float mx = std::fabs(yawErr) > tol * 0.5f ? clampf(yawErr * m_gainX * 0.8f, 250.0f) : 0.0f;
    float my = std::fabs(pitchErr) > tol * 0.5f ? clampf(pitchErr * m_gainY * 0.8f, 150.0f) : 0.0f;
    if (std::fabs(mx) < 1 && mx != 0) mx = mx > 0 ? 1.0f : -1.0f;
    if (std::fabs(my) < 1 && my != 0) my = my > 0 ? 1.0f : -1.0f;
    m_lastYaw = yaw; m_lastPitch = pitch; m_lastDx = mx; m_lastDy = my; m_haveLast = true;
    if (mx != 0 || my != 0)
        sink.scriptMouseBy(mx, my);
    return false;
}

} // namespace inputscript
} // namespace hfe
} // namespace dice

extern "C" bool openbf2InputScriptTakeCapture(char* label, unsigned long size)
{
    std::string text;
    if (!dice::hfe::inputscript::inputScript().takeCaptureRequest(text))
        return false;
    if (label && size)
    {
        std::strncpy(label, text.c_str(), size - 1);
        label[size - 1] = 0;
        for (char* c = label; *c; ++c)           // a file-name component
            if (!std::isalnum(static_cast<unsigned char>(*c)) && *c != '-' && *c != '_')
                *c = '_';
    }
    return true;
}

#endif // !DEDICATED && (__APPLE__ || _WIN32)
