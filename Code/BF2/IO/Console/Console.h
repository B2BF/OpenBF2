// Console interface and .con interpreter. Both console implementations use the shared binding registry.

#ifndef IO_CONSOLE_CONSOLE_H        // [INFERRED] guard style
#define IO_CONSOLE_CONSOLE_H

#include <string>
#include <deque>
#include <map>
#include <vector>
#include <list>
#include <sstream>

// NoCaseStringCompare lives in IO/File/IFileSystem.h. The console's per-level variable and constant
// maps use it, which is why `sv.MaxPlayers` and `sv.maxplayers` are the same key — content relies on
// that, so the comparator is part of the behaviour rather than a convenience.
#include "../File/IFileSystem.h"

namespace dice {
namespace hfe {

enum AccessType : int;              // [UNRESOLVED] enumerators; observed values 1 and 7 (bitmask).
                                    //   `: int` = MSVC's default width, required by GCC for an opaque enum

namespace io {

class ConsoleCompiler;
class GameInput;

// Shared console interface. Preserve all declarations in this order so retail plugins dispatch to the correct slots.
class Console
{
public:
    // Shared mutable sink for discarded output. Its contents are not a stable per-call result.
    static std::string ignoredString_;

    virtual ~Console() {}

    // Exact shared vtable order. Offsets below are ELF byte offsets, including the Itanium header's
    // destructor pair; the corresponding PE offsets are four bytes earlier after the destructor.
    virtual void setConsoleCompiler(ConsoleCompiler*) = 0;                            // +8
    virtual void executeLine(const std::string& cmd, std::string& out,
                             bool outputToConsole, bool outputError,
                             bool addToHistory, bool, bool) = 0;                     // +12
    virtual void executeLines(const std::list<std::string>&, std::list<std::string>&,
                              bool, bool, bool) = 0;                                  // +16
    virtual void output(const std::string&) = 0;                                      // +20
    virtual void updateAsciiKey(char) = 0;                                            // +24
    virtual void updateGameInput(const GameInput&) = 0;                               // +28
    virtual void updateKeyMacros(const GameInput&) = 0;                               // +32
    virtual void getLines(int, std::vector<std::string>&) const = 0;                  // +36
    virtual bool run(const std::string& file, const std::string& a1, const std::string& a2,
                     const std::string& a3, const std::string& a4, const std::string& a5,
                     const std::string& a6, const std::string& a7, const std::string& a8,
                     std::string& out) = 0;                                            // +40
    virtual bool runFullAccess(const std::string& file, const std::string& a1,
                               const std::string& a2, const std::string& a3,
                               const std::string& a4, const std::string& a5,
                               const std::string& a6, const std::string& a7,
                               const std::string& a8, std::string& out) = 0;           // +44
    virtual bool include(const std::string& file, const std::string& a1,
                         const std::string& a2, const std::string& a3,
                         const std::string& a4, const std::string& a5,
                         const std::string& a6, const std::string& a7,
                         const std::string& a8, std::string& out) = 0;                 // +48
    virtual bool bindKeyToConsoleScript(int key, const std::string& script) = 0;       // +52
    virtual void setExitCallBack(void (*fn)(void*), void* userData) = 0;               // +56
    virtual void update(char, std::string&, std::vector<std::string>&) = 0;            // +60
    virtual bool addVariable(const std::string& name, const std::string& value) = 0;   // +64
    virtual bool addConstant(const std::string& name, const std::string& value) = 0;   // +68
    virtual std::string listVariables(const std::string& filter) = 0;                  // +72
    virtual std::string listConstants(const std::string& filter) = 0;                  // +76
    virtual void setEchoErrors(bool) = 0;                                              // +80
    virtual bool getEchoErrors() const = 0;                                            // +84
    virtual void setEcho(bool) = 0;                                                    // +88
    virtual bool getEcho() const = 0;                                                  // +92
    virtual void setMaxLineSize(int) = 0;                                              // +96
    virtual int getMaxLineSize() const = 0;                                            // +100
    virtual void setMaxHistorySize(int) = 0;                                           // +104
    virtual int getMaxHistorySize() const = 0;                                         // +108
    virtual void setMaxCommandHistorySize(int) = 0;                                    // +112
    virtual int getMaxCommandHistorySize() const = 0;                                  // +116
    virtual void setUseRelativePaths(bool) = 0;                                        // +120
    virtual bool getUseRelativePaths() const = 0;                                      // +124
    virtual void setAllowMultipleFileLoad(bool) = 0;                                   // +128
    virtual bool getAllowMultipleFileLoad() const = 0;                                 // +132
    virtual const std::string& getWorkingPath() const = 0;                             // +136
    virtual void setWorkingPath(const std::string&) = 0;                               // +140
    virtual const std::string& getWorkingFile() const = 0;                             // +144
    virtual std::string getWorkingFileAndLine() const = 0;                             // +148
    virtual void setAccess(int) = 0;                                                   // +152
    virtual int getAccess() const = 0;                                                 // +156
    virtual void setCurrentLine(std::string&) = 0;                                     // +160
    virtual void setConsoleActive(bool active);
    virtual bool getConsoleActive();
                                                                                       // not const
    virtual void loadCommandHistory(const std::string&);
    virtual void saveCommandHistory(const std::string&);
    virtual void addAutoComplArgument(const std::string&) = 0;                         // +180
    virtual void removeAutoComplArgument(const std::string&) = 0;                      // +184
    virtual void setHandleCommandHook(bool (*hook)(const char*, int)) = 0;             // +188
protected:
    /* +0x04 */ bool m_consoleActive;   // the only field either body touches
};

// Free function, not a member. The whole boolean-expression evaluator.
// Exactly three operands: there is no nesting and no sub-expressions.
bool console_expr(const std::string& lhs, const std::string& op, const std::string& rhs,
                  std::string& error);

class OldConsole : public Console
{
public:
    OldConsole();
    ~OldConsole() override;

    // Vtable order (offsets are the slot, and callers dispatch by it)
    void setConsoleCompiler(ConsoleCompiler*) override;
    virtual void executeLine(const std::string& cmd, std::string& out,
                             bool outputToConsole, bool outputError,
                             bool addToHistory, bool, bool) override;
    // Executes the lines and appends their results, forwarding the three execution flags.
    void executeLines(const std::list<std::string>&, std::list<std::string>&,
                      bool, bool, bool) override;
    void output(const std::string&) override;
    void updateAsciiKey(char) override;
    void updateGameInput(const GameInput&) override;
    void updateKeyMacros(const GameInput&) override;                               // +32  0x844d5c0  empty (5 B)
    void getLines(int, std::vector<std::string>&) const override;
    bool run(const std::string& file, const std::string& a1, const std::string& a2,
             const std::string& a3, const std::string& a4, const std::string& a5,
             const std::string& a6, const std::string& a7, const std::string& a8,
             std::string& out) override;
    bool runFullAccess(const std::string& file, const std::string& a1, const std::string& a2,
                       const std::string& a3, const std::string& a4, const std::string& a5,
                       const std::string& a6, const std::string& a7, const std::string& a8,
                       std::string& out) override;
    bool include(const std::string& file, const std::string& a1, const std::string& a2,
                 const std::string& a3, const std::string& a4, const std::string& a5,
                 const std::string& a6, const std::string& a7, const std::string& a8,
                 std::string& out) override;
    // bool, not void - `v3 = 0; if (in range) { store; return 1; } return v3;`. Same correction
    // as the rest of this class's answer-a-question members.
    bool bindKeyToConsoleScript(int key, const std::string& script) override;
    void setExitCallBack(void (*fn)(void*), void* userData) override;
    void update(char, std::string&, std::vector<std::string>&) override;
    // addVariable replaces existing values; addConstant preserves them. Both return true only for a new declaration.
    bool addVariable(const std::string& name, const std::string& value) override;
    bool addConstant(const std::string& name, const std::string& value) override;
    // Returns formatted lines for variables matching filter.
    std::string listVariables(const std::string& filter) override;
    std::string listConstants(const std::string& filter) override;
    void setEchoErrors(bool) override;                                             // +80
    bool getEchoErrors() const override;                                           // +84
    void setEcho(bool) override;                                                   // +88
    bool getEcho() const override;                                                 // +92
    void setMaxLineSize(int) override;                                             // +96
    int getMaxLineSize() const override;                                           // +100
    // Sets the history limit and trims retained entries. This setter has no return value.
    void setMaxHistorySize(int) override;
    int getMaxHistorySize() const override;                                        // +108
    void setMaxCommandHistorySize(int) override;
    int getMaxCommandHistorySize() const override;                                 // +116
    void setUseRelativePaths(bool) override;                                       // +120
    bool getUseRelativePaths() const override;                                     // +124
    void setAllowMultipleFileLoad(bool) override;                                  // +128
    bool getAllowMultipleFileLoad() const override;                                // +132
    const std::string& getWorkingPath() const override;                            // +136
    void setWorkingPath(const std::string&) override;                              // +140
    const std::string& getWorkingFile() const override;                            // +144
    std::string getWorkingFileAndLine() const override;
    void setAccess(int) override;
    int getAccess() const override;                                                // +156
    void setCurrentLine(std::string&) override;
    // +164 / +168 are Console::setConsoleActive / getConsoleActive, inherited
    void loadCommandHistory(const std::string&) override;                          // +172 stub (5 B)
    void saveCommandHistory(const std::string&) override;                          // +176 stub (5 B)
    // the two stubs that leave m_autoComplArguments empty. They are the only writers of the
    // +0x448 map that autoCompletion's second-token branch reads, and both are five bytes in the
    // shipped dedicated server, so argument completion is dead on this image by construction -
    // not unimplemented here. See Console.cpp.
    void addAutoComplArgument(const std::string&) override;                        // +180 stub (5 B)
    void removeAutoComplArgument(const std::string&) override;                     // +184 stub (5 B)
    void setHandleCommandHook(bool (*hook)(const char*, int)) override;

    // non-virtual
    int  handleCommand(const std::string& cmd, std::string& result, AccessType access);
    int  getArgs(std::istringstream& in, std::string* out, std::string* raw,
                 int maxArgs, std::string& error, int expandFrom);
    bool exchangeVarsAndConsts(const std::string& token, std::string& out,
                               std::string& error);
    bool exchangeVariable(const std::string& name, std::string& out);
    bool exchangeConstant(const std::string& name, std::string& out);
    // [CHOSEN, not recovered] Shared helper for bounded output-line insertion; no public API or virtual slot.
    void pushOutputLine(const std::string& line);

    // [CHOSEN, not recovered] True only while this console is dispatching a line that originates from
    // a script opened out of a mounted archive. m_fromArchive is sampled per run() (and left sticky
    // afterwards), so it is qualified by m_depth, which is non-zero only inside an active script run;
    // rcon, Python and interactive input dispatch at depth 0 and therefore read false. Used by the
    // openbf2.addModDll extension to refuse archive-sourced native-code loads (B2BF/OpenBF2#39).
    bool isDispatchingArchiveScript() const { return m_fromArchive && m_depth > 0; }

    // Assigns a console variable and returns whether the assignment succeeds.
    bool setVariable(const std::string& name, const std::string& value);
    bool calcBooleanExpr(const std::string& lhs, const std::string& op,
                         const std::string& rhs, bool& result, std::string& error);
    void addAlias(std::string&, std::string&, std::string&);
    void addToCommandHistory(const std::string&);
    // Tab completion. Called only from updateAsciiKey's key-9 case, which sandboxes it at access 1.
    // fully it drives m_autoComplLength / m_autoComplNoDot /
    // m_autoComplPresses and m_autoComplArguments, all declared below.
    void autoCompletion();
    void setPrompt(const std::string&);
    // Returns whether the requested console-depth change was accepted.
    bool incDepth();
    bool decDepth();

    // Saved parser state for one nesting level. Unidentified fields remain [UNRESOLVED].
    // STL sizes vary across toolchains; the original byte size is not a native layout contract.
    struct Level
    {
        Level();
        void reset();

        // Level + 0  — searched by exchangeVariable
        std::map<std::string, std::string, NoCaseStringCompare> m_variables;
        // Level + 12 — searched by exchangeConstant
        std::map<std::string, std::string, NoCaseStringCompare> m_constants;

        // [UNRESOLVED] Neither construction nor reset initializes this field.
        int  m_unknown14;

        // +0x18. reset() zeroes it; the constructor does not. [UNRESOLVED] role.
        bool m_unknown18;

        // +0x1C. Constructed empty, cleared by reset(). [UNRESOLVED] role - it sits beside the
        // while-nesting counter below, which invites the guess that it records a loop body, but
        // nothing in these two bodies shows what is pushed into it, so the guess is not made here.
        std::list<std::string> m_lines1C;

        // +0x20  while-block nesting depth - not an iteration count (see the block above).
        int  m_whileDepth;

        // Per-level strings start empty and are cleared by reset().
        std::string m_string24;
        std::string m_string28;
        std::string m_string2C;

        // +0x30 / +0x34 — per-level block-comment nesting counters, ceiling 10 each.
        int  m_beginRemDepth;
        int  m_beginNoExecutionDepth;

        // +0x38. Zeroed by reset(). [UNRESOLVED] role.
        int  m_unknown38;

        // Level + 0x3C..0x3F, per the flag notes above. reset() clears all four with a single
        // dword store, which is how they were shown to be adjacent bytes rather than four ints.
        bool m_executing;
        bool m_branchTaken;
        bool m_ifOpen;
        bool m_elseSeen;

        // +0x40. Zeroed by reset() as a byte. [UNRESOLVED] role.
        bool m_unknown40;
    };

private:
    // Abridged: the full layout is 1120 bytes / 91 fields. Only the named ones are listed.
    /*    0 */ void*            pVMT;
    /*    8 */ ConsoleCompiler* m_consoleCompiler;   // when set, every line is also compiled

    // Wrapped output waiting for readers, bounded by m_maxHistorySize. Oldest lines are evicted first.
    /*   24 */ std::deque<std::string> m_lines;
    /*   64 */ int              m_maxHistorySize;
    // Counts output lines written. [UNRESOLVED] Its reader is unidentified; getLines does not reset it.
    /*   68 */ int              m_linesWritten;
    /*  112 */ int              m_maxCommandHistorySize;
    /*  116 */ char             m_currentLineBuffer[132];   // 116..247
    /*  248 */ int              m_maxLineSize;
    // +0xFC. The number of characters currently in m_currentLineBuffer. setCurrentLine recomputes
    // it with an inlined strlen; updateAsciiKey uses it as the insertion cursor and gates typing
    // with `if (m_currentLineLength >= m_maxLineSize) ignore the key`, which is what bounds
    // interactive input to 128 characters.
    /*  252 */ int              m_currentLineLength;
    /*  256 */ bool             m_fullAccess;
    /*  260 */ std::string      m_prompt;

    // Scrollback page size in lines and current page index.
    /*  264 */ int              m_scrollPageSize;   // +0x108, ctor sets 8
    // Zero displays live output. Keys other than PageUp/PageDown return to that position.
    // [CHOSEN] Initialized to zero to match the original static instance.
    /*  268 */ int              m_scrollPage;
    /*  276 */ bool             m_useRelativePaths;
    /*  280 */ std::string      m_workingPath;
    /*  284 */ std::string      m_workingFile;
    /*  288 */ bool             m_allowMultipleFileLoad;
    // +0x110. Cursor into m_commandHistory for Up/Down recall. updateAsciiKey steps it and wraps it
    // at both ends - past the start jumps to the last entry, past the end jumps to 0 - and its tail
    // multiplies it by (key was Up or Down), so any other key resets it to 0.
    /*  272 */ int              m_historyCursor;
    /*  292 */ int              m_access;                   // AccessType bitmask

    // Console scripts bound to function keys 1 through 12.
    /*  296 */ std::string      m_keyBindings[12];

    // [INFERRED] Completion state; the previous completed line length distinguishes repeated Tab presses.
    /*  352 */ int              m_autoComplLength;
    // +0x164. true when the token last completed contained no '.', i.e. the completion was an
    // object name rather than an object.method. It survives between calls, and the echo condition
    // at the top of autoCompletion reads the previous call's value.
    // [CHOSEN] initialiser false - the original ctor does not write +0x164 or +0x168 at all, the
    // same call m_scrollPage documents.
    /*  356 */ bool             m_autoComplNoDot;
    // +0x168. Consecutive Tab presses on an unchanged line: 0 on the first, 1 on the second
    // (which lists the candidates), 2 on the third (which lists argument types and ranges).
    /*  360 */ int              m_autoComplPresses;

    // +0x16C. Set when the previous key was the 0xE0 extended-key prefix, so the next call knows to
    // translate the scancode that follows. update() owns it; nothing else touches it.
    /*  364 */ bool             m_pendingExtendedKey;
    /*  344 */ void           (*m_exitCallBack)(void*);
    /*  348 */ void*            m_exitCallBackData;
    /*  365 */ bool             m_echoErrors;
    /*  366 */ bool             m_echo;
    // Ten script scopes, each with case-insensitive variables/constants and its own control-flow state.
    // reset discards that scope's declarations. beginRem and beginNoExecution have independent nesting
    // limits, separate from script depth; whileDepth counts nested blocks, not loop iterations.
    // Keep the branch-taken/open/else state and unknown fields in their declared order.
    /*  376 */ Level            m_levels[10];
    /* 1056 */ Level*           m_currentLevel; // Cached pointer to the current nesting level.
    /* 1060 */ int              m_depth;        // Current index into m_levels.
    // Script name and line are saved and restored around nested execution.
    /*  368 */ std::string      m_currentScript;            // +0x170
    /*  372 */ int              m_currentScriptLine;        // +0x174

    /* 1064 */ bool             m_fromArchive; // Whether run opened the script from an archive.
    // Case-sensitive completion-argument map; dedicated-build mutators leave it empty.
    /* 1096 */ std::map<std::string, bool> m_autoComplArguments;

    /* 1108 */ std::string      m_outputRedirectFile; // [INFERRED] Per-command output destination.
    /* 1112 */ bool             m_outputRedirectAppend;     // +0x458. [INFERRED] - the append flag
                                                            // noted alongside 1108. Not yet read
                                                            // from disassembly.
    /* 1116 */ bool           (*m_handleCommandHook)(const char*, int);

    // Previously entered commands, oldest first.
    /*   72 */ std::deque<std::string> m_commandHistory;    // +0x48
    // [UNRESOLVED] the remaining ~60 fields, including m_variables / m_constants storage.
};

// Process-wide console used to execute .con scripts.
extern OldConsole* mainConsole;

// The console handleCommand is currently dispatching for; set immediately before a binding runs so a
// binding can reach back to its caller (e.g. isDispatchingArchiveScript). Null before the first dispatch.
extern OldConsole* g_executingConsole;

} // namespace io
} // namespace hfe
} // namespace dice

#endif // IO_CONSOLE_CONSOLE_H
