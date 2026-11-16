// [TEMP] [CHOSEN, not recovered] Client test tool: a scripted input source for unattended client
// runs (docs/macos-c-input-script.md). Off unless BF2_INPUT_SCRIPT=<file> is set. The platform host
// runs one script tick per pump and turns its actions into the input its real devices produce:
//   macOS   CocoaHost (Platform/macOS) builds HostKeyEvent, HostTextEvent, HostMouse*Event and
//           HostRawMouseEvent values for the window-message source and the Mac input devices.
//   Win32   WinInputScript (Platform/Windows) sends window messages to the game window (menus,
//           Swiff, console, chat) and feeds DirectInput device data to KeyboardDevice and
//           MouseDevice (the in-game path).
//   Linux   SDLHost uses the shared native host key/event contract and SDLInputScript names.
// While a script is active the host ignores real input, reports focus, and leaves the real cursor
// alone. Key codes are the host's own: kVK_ codes on macOS, DIK_ codes (scan codes) on Win32.
// Client images only (!DEDICATED). [RETIRE-WHEN: client-test-automation-replaced]
#ifndef PLATFORM_INPUTSCRIPT_H
#define PLATFORM_INPUTSCRIPT_H

#include <string>
#include <vector>

namespace dice {
namespace hfe {
namespace inputscript {

// What the script needs from the host. Implemented by CocoaHost::Impl (macOS) and the Win32 sink
// in WinInputScript.cpp.
class IInputScriptSink
{
public:
    // A key transition (host key code). text (may be empty) is the characters the press carries
    // (NSEvent.characters on macOS, the WM_CHAR sequence on Win32).
    virtual void scriptKey(unsigned short keyCode, bool down, const std::u16string& text) = 0;
    // Absolute pointer position in back-buffer pixels (origin top left).
    virtual void scriptMouseTo(float x, float y) = 0;
    // Relative device motion (x right, y down), the in-game mouse-look path.
    virtual void scriptMouseBy(float dx, float dy) = 0;
    virtual void scriptButton(int button, bool down) = 0;
    virtual void scriptWheel(int delta) = 0;            // DirectInput units, +120 a notch away
    // The character a key produces with the current layout (macOS: UCKeyTranslate; Win32: ToUnicode).
    virtual std::u16string scriptKeyCharacter(unsigned short keyCode, bool shift) = 0;
    virtual void scriptQuit() = 0;
protected:
    ~IInputScriptSink() {}
};

// [TEMP] The game side of aim / face / walk-to (BF2Engine.cpp, BF2_INPUT_SCRIPT runs only):
// fills the local player's view (camera position, forward) and, for a named target, its world
// position. Targets: "enemy", "friend", "player" (nearest other alive player, chest height) and
// "x/y/z" (a world position, e.g. a vehicle spawner from GamePlayObjects.con). Returns false when
// there is no local player or no such target.
typedef bool (*ScriptQueryFn)(const char* target, float viewPos[3], float viewFwd[3], float targetPos[3]);
void setScriptQuery(ScriptQueryFn fn);

class InputScript
{
public:
    // Reads BF2_INPUT_SCRIPT. Returns false (inactive) when unset or unreadable.
    bool loadFromEnvironment();
    bool active() const { return m_active; }
    // One tick per host pump. Executes steps until one has to wait.
    void tick(IInputScriptSink& sink);

    // Capture requests (macOS: MetalBridge's present capture, BF2_PRESENT_PNG; Win32: the sink's
    // window capture). Returns true and the label once per request.
    bool takeCaptureRequest(std::string& label);
    // Queues a capture for the renderer. A request is not proof that an image has been saved.
    void requestCapture(const std::string& label);

private:
    enum Op
    {
        OpWaitFrames, OpWaitMs, OpWaitLog, OpKeyDown, OpKeyUp, OpText, OpMoveTo, OpMoveBy,
        OpButtonDown, OpButtonUp, OpWheel, OpCapture, OpLog, OpQuit, OpTimeout, OpEnd,
        OpAim, OpFace, OpWalkTo, OpWaitLogElse, OpLabel
    };
    struct Step
    {
        Op op;
        int line;
        int a, b;
        float x, y;
        std::string text;
        std::string label;          // wait-log-else: the label to jump to on timeout
        std::u16string chars;
    };

    bool parse(const std::string& path);
    bool parseLine(const std::string& line, int number);
    void addKeyPress(unsigned short keyCode, bool shift, const std::u16string& text, int line, int holdMs);
    void addWaitFrames(int frames, int line);
    void addWaitMs(int ms, int line);
    bool expandText(const std::u16string& text, int line);
    bool logContains(const std::string& needle);
    void report(const char* fmt, ...) const;
    void finish(const char* why);
    // aim / face / walk-to: one control step; true when the step is complete.
    bool steer(Step& s, IInputScriptSink& sink);

    bool m_active = false;
    bool m_finished = false;
    std::vector<Step> m_steps;
    std::size_t m_pc = 0;
    unsigned long m_frame = 0;
    // Wait state of the current step.
    bool m_waiting = false;
    unsigned long m_waitFrame = 0;
    double m_waitStart = 0.0;
    double m_timeoutSeconds = 180.0;
    double m_startSeconds = 0.0;
    // Log scan: the file stdout/stderr is redirected to (or BF2_INPUT_SCRIPT_LOG).
    std::string m_logPath;
    std::string m_logTail;               // unmatched text since the last wait-log match
    long long m_logOffset = 0;
    // Pending text expansion: kept as steps after parsing needs the layout, done at first tick.
    IInputScriptSink* m_sink = nullptr;
    std::vector<std::string> m_captures;
    bool m_shiftHeld = false;
    float m_pointerScale = 1.2f;         // HUD pixels per mouse count (pointer, hud-click)
    // Steering state (aim / face / walk-to): counts per radian, learnt from the view's response.
    float m_gainX = 300.0f, m_gainY = -300.0f;   // initial guesses; y down looks down
    float m_lastYaw = 0.0f, m_lastPitch = 0.0f, m_lastDx = 0.0f, m_lastDy = 0.0f;
    bool m_haveLast = false;
    bool m_walkKeyDown = false;
    unsigned long m_steerFrame = 0;
};

InputScript& inputScript();

// Host key codes, implemented once per platform (MacInputScript.mm: kVK_; WinInputScript.cpp:
// DIK_). lookup takes a lower-case IDKey_ name without the prefix ("escape", "leftshift"...).
namespace keys {
bool lookup(const std::string& name, unsigned short& code);
// The characters a press carries for keys the layout translation reports as control characters.
std::u16string controlCharacters(unsigned short code);
bool isModifier(unsigned short code);
bool isShift(unsigned short code);
unsigned short leftShift();
unsigned short grave();          // the console key, left of 1
unsigned short enter();
unsigned short w();              // walk-to's forward key
} // namespace keys

} // namespace inputscript
} // namespace hfe
} // namespace dice


// Polled by MetalBridge's present capture (BF2_PRESENT_PNG); a weak default there answers false
// in images without the script.
extern "C" bool openbf2InputScriptTakeCapture(char* label, unsigned long size);

#endif // PLATFORM_INPUTSCRIPT_H
