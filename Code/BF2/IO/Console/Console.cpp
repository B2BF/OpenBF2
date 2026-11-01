// Console script parsing and dispatch through the shared binding registry.

#include "Console.h"
#include "ConsoleCompiler.h"                 // [INFERRED] path
#include "../Input/GameInput.h"
#include "../File/FileManager.h"             // [INFERRED] path; IO/File/FileManager.cpp attested
#include "../File/File.h"                    // io::File - the raii handle run() loads a script with
#include "../../Core/StringUtil.h"
#include "../../Core/ConsoleObjects.h"    // the binding registry handleCommand dispatches through
#include "../../Core/ConsoleObject.h"           // compareIgnoreCase(const char*, const char*, int)
                                             // the 3-arg overload addConstantToAllConsoles uses
#include "../../Core/EventManager.h"         // eventManager - executeLine's "rcon " post

#include <cstring>
#include <cstdio>                            // sprintf - autoCompletion pastes the argument match
#include <cctype>                            // tolower - autoCompletion's common-prefix loop
#include <cstdlib>


// run() executes scripts in a nested scope; include() executes in the caller's scope and access level.
// Both resolve files through FileManager. A missing explicitly named .tweak include succeeds.

// run() resolves scripts through FileManager and preserves include/run semantics.


namespace dice {
namespace hfe {

// Case-insensitive compare, used pervasively by the console. Two overloads exist:
// (const char*, const char*) and (const char*, const char*, int).
int compareIgnoreCase(const char* a, const char* b);
int compareIgnoreCase(const char* a, const char* b, int n);

namespace io {

extern OldConsole* mainConsole;              // _ZN4dice3hfe2io11mainConsoleE
extern OldConsole* g_executingConsole;       // ds:0x8AC3C88 - see handleCommand

namespace {
#if !DEDICATED
// [TEMP] [CHOSEN, not recovered] Local console testing requested by the owner.
// Only keyboard Enter/Tab can establish this scope; calls outside that scope retain their
// access policy. Scripts invoked by an unlocked command inherit its local scope.
// [RETIRE-WHEN: client-console-testing] Remove after developer-console policy
// is settled and its interactive and remote authorization checks are validated.
thread_local OldConsole* s_unlockedInteractiveConsole = nullptr;

bool interactiveFullAccessEnabled()
{
    return value && std::strcmp(value, "0") != 0;
}

class InteractiveAccessScope
{
public:
    explicit InteractiveAccessScope(OldConsole* console)
        : m_previous(s_unlockedInteractiveConsole)
    {
        if (interactiveFullAccessEnabled())
            s_unlockedInteractiveConsole = console;
    }
    ~InteractiveAccessScope() { s_unlockedInteractiveConsole = m_previous; }
private:
    OldConsole* m_previous;
};
#endif

bool isUnlockedInteractiveConsole(const OldConsole* console)
{
#if !DEDICATED
    return s_unlockedInteractiveConsole == console;
#else
    (void)console;
    return false;
#endif
}
} // namespace

// Shared mutable output sink for callers that discard console output.
std::string Console::ignoredString_;

// Evaluates a console expression and writes a diagnostic to error on invalid syntax.
// Equality compares strings; ordering uses atof; boolean operations use atoi.
// Unary negation arrives as lhs="!" or "not", op=operand and an empty rhs.
// There is no nested-expression parser; false can mean either a false condition or an error.
bool console_expr(const std::string& lhs, const std::string& op, const std::string& rhs,
                  std::string& error)
{
    error.clear();

    // Empty left operand: only equality is legal
    if (lhs.empty())
    {
        if (op != "=" && op != "==" && op != "equals")
        {
            error = "Error: Syntax error in expression!";
            return false;
        }
        return lhs == rhs;
    }

    // The left operand is parsed both ways up front; which one is used depends on the operator.
    const float lhsNum  = static_cast<float>(atof(lhs.c_str()));
    const bool  lhsBool = atoi(lhs.c_str()) != 0;

    if (op.empty())
        return lhsBool;                 // a bare operand is truthiness-tested via atoi

    // Unary form: the operator sits in the left slot and the operand in the middle.
    if ((lhs == "!" || lhs == "not") && rhs.empty())
        return atoi(op.c_str()) == 0;

    // An operator with no right operand (or vice versa) is a syntax error.
    if ((!op.empty() && rhs.empty()) || (op.empty() && !rhs.empty()))
    {
        error = "Error: Syntax error in expression!";
        return false;
    }

    const float rhsNum  = static_cast<float>(atof(rhs.c_str()));
    const bool  rhsBool = atoi(rhs.c_str()) != 0;

    if (op == "="  || op == "==" || op == "equals")            return lhs == rhs;   // string
    if (op == "!=" || op == "notEquals")                       return lhs != rhs;   // string
    if (op == ">"  || op == "greaterThan")                     return lhsNum >  rhsNum;
    if (op == ">=" || op == "greaterOrEqualThan")              return lhsNum >= rhsNum;
    if (op == "<"  || op == "lessThan")                        return lhsNum <  rhsNum;
    if (op == "<=" || op == "lessOrEqualThan")                 return lhsNum <= rhsNum;
    if (op == "&"  || op == "&&" || op == "and")               return lhsBool && rhsBool;
    if (op == "|"  || op == "||" || op == "or")                return lhsBool || rhsBool;

    error = "Error: Unknown operand in expression \"" + op + "\"!";
    return false;
}

bool OldConsole::calcBooleanExpr(const std::string& lhs, const std::string& op,
                                 const std::string& rhs, bool& result, std::string& error)
{
    // ELF 0x843e910 (69 bytes) - a thin forwarder, identical in shape to
    // console::Parser::calcBooleanExpr at 0x845b330.
    result = console_expr(lhs, op, rhs, error);
    return error.empty();
}

// Tokenizes console arguments. Quoting and variable expansion begin at expandFrom; earlier arguments stay literal.
int OldConsole::getArgs(std::istringstream& in, std::string* out, std::string* raw,
                        int maxArgs, std::string& error, int expandFrom)
{
    int count = 0;

    while (!in.eof() && count < maxArgs)
    {
        bool inQuotes = false;
        std::string token;

        in >> std::ws;                                  // skip leading whitespace
        if (in.eof())
            break;

        for (;;)
        {
            int c = in.get();
            if (c == EOF)
                break;
            if (!inQuotes && (c == ' ' || c == '\t'))   // only space and tab delimit
                break;

            if (c == '"')
            {
                if (inQuotes)
                    break;                              // closing quote ends the token
                inQuotes = true;                        // opening quote
            }
            else
            {
                token += static_cast<char>(c);          // quote chars are never kept
            }
        }

        if (in.eof() && inQuotes)
        {
            error = "no closing '\"'!";
            return -1;
        }

        // Retain the original token alongside its expanded value.
        raw[count] = token;
        out[count] = token;

        if (count >= expandFrom)
        {
            if (!exchangeVarsAndConsts(raw[count], out[count], error))
                return -1;
        }
        else if (inQuotes)
        {
            std::ostringstream os;
            os << "'\"' not allowed in arg " << count << "!";
            error = os.str();
            return -1;
        }

        ++count;
    }

    return count;
}

// Expands a whole variable or constant token, leaving other text unchanged.
// Reference prefixes are case-insensitive; embedded string interpolation is unsupported.
bool OldConsole::exchangeVarsAndConsts(const std::string& token, std::string& out,
                                       std::string& error)
{
    if (compareIgnoreCase(token.c_str(), "v_", 2) == 0)
    {
        if (exchangeVariable(token, out))
            return true;
        error = "Unknown variable " + token + "!";
        return false;
    }

    if (compareIgnoreCase(token.c_str(), "c_", 2) != 0)
    {
        out = token;                                    // neither prefix - verbatim
        return true;
    }

    if (exchangeConstant(token, out))
        return true;
    error = "Unknown constant " + token + "!";
    return false;
}

// executeLine - Structure recovered.

// Takes a whole buffer, not one line: it splits on '\n' internally, so a single call can carry an
// entire file. Per-line buffer is 1023 bytes.
void OldConsole::executeLine(const std::string& command, std::string& out,
                             bool outputToConsole, bool outputError,
                             bool addToHistory, bool suppressEcho, bool fullAccess)
{
    std::istringstream stream(command);
    char buf[1024];

    while (stream.getline(buf, 1023, '\n'))
    {
        std::string line(buf);

        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);                // tolerate CRLF

        if (addToHistory)
            addToCommandHistory(line);

        if (outputToConsole)
            output(m_prompt + line);

        // Posts the text after "rcon " as a synchronous remote-console event and returns a newline.
        // Uses the raw line buffer, including a trailing carriage return when present.
        if (line.size() > 4 && line.find("rcon ") == 0)
        {
            eventManager->postEvent(ECNetwork, 18, buf + 5, 0.0f);   // ELF +0x14 / PE +0x10
            out += "\n";
            continue;
        }

        if (line.empty())
            continue;

        if (m_consoleCompiler)
            m_consoleCompiler->addLine(line);           // compiled-.con capture

        std::istringstream ls(line);
        std::string firstToken;
        ls >> firstToken;

        AccessType access;
        if (fullAccess)                 access = static_cast<AccessType>(7);
        else if (m_access & 2)          access = static_cast<AccessType>(7);
        else                            access = static_cast<AccessType>(1);

        std::string result;
        const int status = handleCommand(line, result, access);

        switch (status)
        {
        case 1:                                          // nothing to report - silent (0x844C950)
            break;

        case 0:                                          // there is result text - report it
            if (m_outputRedirectFile.empty())            // field at 1108
            {
                if (outputError)
                    output(result);
            }
            else
            {
                // Redirect to a file instead of the console. Append mode from the flag at 1112.
                // A failure here emits "Failed to open file for writing: " + name.
            }
            break;

        case 2:                                          // error, unless the command was `run`
            if (firstToken != "run" && outputError)
                output("Error: " + result);
            break;

        case 3:                                          // echo - the `echo` keyword's own status
            // Echo output is enabled either by the console echo setting or the error-output flag.
            if ((!suppressEcho && m_echo) || outputError)
                output("Echo: " + result);
            break;

        default:
            if (outputError)
                output(result);
            break;
        }

        m_outputRedirectFile.clear();                    // redirection is per-command
        out += result;
    }
}

// Keep unknown-command and argument errors consistent with the shipped console.


// Looks up a case-insensitive name from the current scope outward.
// Returns true and replaces out on the first match; negative depth or a missing name returns false.

bool OldConsole::exchangeVariable(const std::string& name, std::string& out)
{
    for (int i = m_depth; i >= 0; --i)           // innermost first — see the shadowing note
    {
        std::map<std::string, std::string, NoCaseStringCompare>::const_iterator it =
            m_levels[i].m_variables.find(name);
        if (it != m_levels[i].m_variables.end())
        {
            out.assign(it->second);
            return true;
        }
    }
    return false;
}

// Identical, over the constants map at Level + 12 instead of Level + 0.
bool OldConsole::exchangeConstant(const std::string& name, std::string& out)
{
    for (int i = m_depth; i >= 0; --i)
    {
        std::map<std::string, std::string, NoCaseStringCompare>::const_iterator it =
            m_levels[i].m_constants.find(name);
        if (it != m_levels[i].m_constants.end())
        {
            out.assign(it->second);
            return true;
        }
    }
    return false;
}

// Assigns the innermost existing variable. Returns false when no declaration exists; never creates one.
bool OldConsole::setVariable(const std::string& name, const std::string& value)
{
    for (int i = m_depth; i >= 0; --i)               // innermost first, exactly as the lookups walk
    {
        std::map<std::string, std::string, NoCaseStringCompare>::iterator it =
            m_levels[i].m_variables.find(name);
        if (it != m_levels[i].m_variables.end())
        {
            it->second.assign(value);
            return true;
        }
    }
    return false;                                    // undeclared - setVariable does not create
}


// Adds a nonempty command to history.
// [CHOSEN, not fully recovered] Evicts the oldest command when the configured limit is reached.
void OldConsole::addToCommandHistory(const std::string& command)
{
    if (command.empty())            // read: the first test in the function
        return;

    m_commandHistory.push_back(command);

    // [CHOSEN] trim from the front. See the note above.
    while (m_maxCommandHistorySize > 0 &&
           static_cast<int>(m_commandHistory.size()) > m_maxCommandHistorySize)
    {
        m_commandHistory.pop_front();
    }
}

// Dispatches a command with the requested access and returns a status for executeLine.
// An installed hook runs before parsing; output redirection is then separated from the command.
// The dispatcher remains partial: some paths are [CHOSEN, not recovered]. Verify each binding's
// field or method semantics before extending dispatch.
int OldConsole::handleCommand(const std::string& cmd, std::string& result, AccessType access)
{
    // read: the hook is consulted first, before anything is parsed.
    if (m_handleCommandHook != 0)
    {
        if (m_handleCommandHook(cmd.c_str(), static_cast<int>(access)))
            return 1;
    }

    // read: the command is searched for '>' — output redirection.
    std::string line = cmd;
    const std::string::size_type redirect = line.find('>');
    if (redirect != std::string::npos)
    {
        // [CHOSEN] the split. The binary parses a filename and an append flag into
        // m_outputRedirectFile / m_outputRedirectAppend; the exact tokenisation was not read.
        m_outputRedirectFile = line.substr(redirect + 1);
        line = line.substr(0, redirect);
    }

    // trim
    while (!line.empty() && (line[0] == ' ' || line[0] == '\t')) line.erase(0, 1);
    while (!line.empty() && (line[line.size()-1] == ' ' || line[line.size()-1] == '\t' ||
                             line[line.size()-1] == '\r' || line[line.size()-1] == '\n'))
        line.erase(line.size()-1);

    if (line.empty())
        return 1;

    // [CHOSEN] `rem` handling here. The real function has a full keyword table.
    if (line.find("rem ", 0) == 0 || line == "rem")
        return 1;

    // Handles conditional branches and script execution. Skipped branches only process nesting keywords.
    // [CHOSEN interim] Branch state is stored by parser depth; move it into Level when its [UNRESOLVED] tail fields are identified.
    {
        struct BranchState { bool executing; bool branchTaken; int skipNesting; };
        static std::vector<BranchState> s_branch[10 + 1];
        std::vector<BranchState>& stack = s_branch[(m_depth >= 0 && m_depth <= 10) ? m_depth : 0];

        // Comment and no-execution blocks skip statements and support nested begin/end pairs.
        // [CHOSEN interim] Their counters are stored by parser depth until the Level fields are identified.
        static int s_remDepth[10 + 1];
        static int s_noExecDepth[10 + 1];
        const int levelIdx = (m_depth >= 0 && m_depth <= 10) ? m_depth : 0;
        int& remDepth    = s_remDepth[levelIdx];
        int& noExecDepth = s_noExecDepth[levelIdx];

        std::istringstream kin(line);
        std::string kw;
        kin >> kw;

        // An active block comment swallows everything except its own nesting tokens
        if (remDepth > 0)
        {
            if (compareIgnoreCase(kw, "beginRem") == 0)
            {
                if (remDepth > 9) { result = "\"beginRem\" max depth has been reached!"; return 2; }
                ++remDepth;
                return 1;
            }
            if (compareIgnoreCase(kw, "endRem") == 0)
            {
                --remDepth;
                return 1;
            }
            return 1;                                    // inside the block: skipped
        }
        if (noExecDepth > 0)
        {
            if (compareIgnoreCase(kw, "beginNoExecution") == 0)
            {
                if (noExecDepth > 9) { result = "\"beginNoExecution\" max depth has been reached!"; return 2; }
                ++noExecDepth;
                return 1;
            }
            if (compareIgnoreCase(kw, "endNoExecution") == 0)
            {
                --noExecDepth;
                return 1;
            }
            return 1;
        }

        const bool skipping = !stack.empty() && !stack.back().executing;

        if (compareIgnoreCase(kw, "if") == 0)
        {
            if (skipping)
            {
                ++stack.back().skipNesting;              // nested if in a dead branch: count only
                return 1;
            }
            std::string lhs, op, rhs, value;
            kin >> lhs; kin >> op; kin >> rhs;
            if (exchangeVariable(lhs, value) || exchangeConstant(lhs, value)) lhs = value;
            if (exchangeVariable(rhs, value) || exchangeConstant(rhs, value)) rhs = value;
            std::string error;
            const bool cond = console_expr(lhs, op, rhs, error);
            BranchState next;
            next.executing = cond;
            next.branchTaken = cond;
            next.skipNesting = 0;
            stack.push_back(next);
            return 1;
        }
        if (compareIgnoreCase(kw, "elseIf") == 0)
        {
            if (skipping && stack.back().skipNesting > 0) return 1;
            if (stack.empty()) { result = "Can not have an \"elseIf\" without an \"if\"!"; return 2; }
            if (stack.back().branchTaken) { stack.back().executing = false; return 1; }
            std::string lhs, op, rhs, value;
            kin >> lhs; kin >> op; kin >> rhs;
            if (exchangeVariable(lhs, value) || exchangeConstant(lhs, value)) lhs = value;
            if (exchangeVariable(rhs, value) || exchangeConstant(rhs, value)) rhs = value;
            std::string error;
            const bool cond = console_expr(lhs, op, rhs, error);
            stack.back().executing = cond;
            stack.back().branchTaken = cond;
            return 1;
        }
        if (compareIgnoreCase(kw, "else") == 0)
        {
            if (skipping && stack.back().skipNesting > 0) return 1;
            if (stack.empty()) { result = "Can not have an \"else\" without an \"if\"!"; return 2; }
            stack.back().executing = !stack.back().branchTaken;
            stack.back().branchTaken = true;
            return 1;
        }
        if (compareIgnoreCase(kw, "endIf") == 0)
        {
            if (skipping && stack.back().skipNesting > 0) { --stack.back().skipNesting; return 1; }
            if (stack.empty()) { result = "Can not have an \"endIf\" without an \"if\"!"; return 2; }
            stack.pop_back();
            return 1;
        }

        if (skipping)
            return 1;                                    // a dead branch swallows every other line
                                                         // Block-comment directives are handled only on an executing branch.
        if (compareIgnoreCase(kw, "beginRem") == 0)
        {
            if (m_depth == 0) { result = "\"beginRem\" not allowed on the highest level!"; return 2; }
            ++remDepth;
            return 1;
        }
        if (compareIgnoreCase(kw, "endRem") == 0)
        {
            result = "Can not have a \"endRem\" without a \"beginRem\"!";
            return 2;
        }
        if (compareIgnoreCase(kw, "beginNoExecution") == 0)
        {
            if (m_depth == 0) { result = "\"beginNoExecution\" not allowed on the highest level!"; return 2; }
            ++noExecDepth;
            return 1;
        }
        if (compareIgnoreCase(kw, "endNoExecution") == 0)
        {
            result = "Can not have a \"endNoExecution\" without a \"beginNoExecution\"!";
            return 2;
        }

        // quit and exit invoke the optional exit callback, then report the same message unconditionally.
        // They work at any depth and return status 0 so executeLine can display the result.
        if (compareIgnoreCase(kw, "quit") == 0 || compareIgnoreCase(kw, "exit") == 0)
        {
            if (m_exitCallBack != 0)                       // +0x158
                m_exitCallBack(m_exitCallBackData);        // +0x15C
            result = "*** Game will exit! **";
            return 0;
        }

        // Parses the filename and up to eight script arguments, stripping quotes and expanding variables and constants.
        if (compareIgnoreCase(kw, "run") == 0)
        {
            std::string tokens[9], raw[9], argError;
            const int expandFrom = (m_currentLevel && m_currentLevel->m_unknown18) ? 11 : 1;
            if (getArgs(kin, tokens, raw, 9, argError, expandFrom - 1) < 0)
            {
                result = argError;
                return 0;
            }
            std::string out;
            run(tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], tokens[5], tokens[6],
                tokens[7], tokens[8], out);                                    // virtual, as the
            return 1;                                                          //   ELF dispatches
        }

        // Includes a script using the same argument parsing as run.
        if (compareIgnoreCase(kw, "include") == 0)
        {
            // Same tokenisation as `run` above: the stock content never quotes an
            // include argument (0 of 1,968 in Objects_*.zip), so this one was latent, not measured.
            std::string tokens[9], raw[9], argError;
            const int expandFrom = (m_currentLevel && m_currentLevel->m_unknown18) ? 11 : 1;
            if (getArgs(kin, tokens, raw, 9, argError, expandFrom - 1) < 0)
            {
                result = argError;
                return 0;
            }
            std::string out;
            include(tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], tokens[5], tokens[6],
                    tokens[7], tokens[8], out);                                // virtual +0x30
            return 1;
        }
    }

    // The two exchange functions above are the real scope-stack lookups, used as the original
    // does: innermost level outward, first hit wins.
    {
        std::string value;
        if (exchangeVariable(line, value) || exchangeConstant(line, value))
        {
            result = value;
            return 1;
        }
    }

    // Looks up the console binding and invokes it with the parsed arguments.
    {
        std::istringstream in(line);
        std::string token;

        if ((in >> token) && !token.empty())
        {
            const std::string::size_type dot = token.find('.');
            if (dot != std::string::npos && dot > 0 && dot + 1 < token.size())
            {
                const std::string objectName = token.substr(0, dot);
                const std::string methodName = token.substr(dot + 1);

                ConsoleObject* binding = mainConsoleObjects
                    ? mainConsoleObjects->getConsoleObject(objectName, methodName) : 0;

                if (!binding)
                {
                    result = "Unknown object or method!";          // 0x8440E79's null path
                    return 0;
                }

                // Parses up to twelve method arguments. Expansion starts at expandFrom; an unknown variable or constant fails the line.
                std::string args[12];
                std::string rawArgs[12];
                const int expandFrom = (m_currentLevel && m_currentLevel->m_unknown18) ? 11 : 1;
                std::string argError;
                const int argc = getArgs(in, args, rawArgs, 12, argError, expandFrom - 1);
                if (argc < 0)
                {
                    result = argError;
                    return 0;
                }

                if (!binding->isActive())                          // vtbl +0x30
                {
                    result = "Method is not active!";
                    return 0;
                }

                // Bindings carrying access bit 4 use the archive-specific policy; others require an intersecting caller access mask.
                const int methodAccess = binding->getAccess();          // vtbl +0x18
                if (isUnlockedInteractiveConsole(this))
                {
                    // The local testing scope bypasses access only, not activity or argument checks.
                }
                else if (methodAccess & 4)
                {
                    // The privileged policy: the script must have come from an archive and the
                    // caller must hold bit 2.
                    if (!m_fromArchive)             { result = "Unauthorised method!"; return 0; }
                    if (!(access & 2))              { result = "Unauthorised method!"; return 0; }
                }
                else if (!(static_cast<int>(access) & methodAccess))
                {
                    result = "Unauthorised method!";
                    return 0;
                }

                // getType() is called four times in the original; once into a local is behaviourally
                // identical because the binding cannot change underneath it.
                const int type = binding->getType();               // vtbl +0x14
                if (type == 2 && argc != 0) { result = "Property is only readable!"; return 0; }
                if (type == 3 && argc == 0) { result = "Property is only setable!";  return 0; }
                // Properties accept zero arguments for a read or one for a write.
                if (type == 1 && argc > 1)  { result = "Properties only takes one argument!"; return 0; }

                if (argc < binding->getMinNoArgs())                // vtbl +0x28
                {
                    result = "Too few arguments, " + token;
                    return 0;
                }
                if (argc > binding->getMaxNoArgs())                // vtbl +0x2C
                {
                    result = "Too many arguments, " + token;
                    return 0;
                }

                // the noExecution gate, 0x8440F2F-0x8440F40. Every check above still runs and can
                // still report - only the invocation is suppressed inside a beginNoExecution block.
                if (m_currentLevel && m_currentLevel->m_beginNoExecutionDepth > 0)
                    return 1;

                // Publish the executing console before invoking the binding.
                g_executingConsole = this;

                result = binding->execute(args, argc);             // vtbl +0x58, returns by value

                // Status 0 exposes the binding's result to output or redirection; status 1 would suppress it.
                // An empty result remains silent through output().
                return 0;
            }
        }
    }

    // [CHOSEN] the failure report. The original produces specific messages per failure mode.
    result = "unknown command: " + line;
    return 0;
}


void Console::setConsoleActive(bool active)
{
    m_consoleActive = active;
}

bool Console::getConsoleActive()
{
    return m_consoleActive;
}

void Console::loadCommandHistory(const std::string&)
{
}

void Console::saveCommandHistory(const std::string&)
{
}

// Initializes parser containers and resets the level. m_unknown14 remains uninitialized.
OldConsole::Level::Level()
{
    reset();
}

// Clears the parser level, including its scoped variables and constants.
void OldConsole::Level::reset()
{
    m_variables.clear();
    m_constants.clear();

    m_unknown18 = false;
    m_lines1C.clear();
    m_whileDepth = 0;

    m_string24.assign("");
    m_string28.assign("");
    m_string2C.assign("");

    m_beginRemDepth         = 0;
    m_beginNoExecutionDepth = 0;
    m_unknown38             = 0;

    m_executing   = false;
    m_branchTaken = false;
    m_ifOpen      = false;
    m_elseSeen    = false;

    m_unknown40 = false;
}

// Constructs the console at full access with empty global scope and enabled echo/error output.
// History and completion containers use the rebuilt STL layout; this is not a retail object overlay.
// Chosen initialization of fields the retail constructor leaves unwritten is marked at each member.
OldConsole::OldConsole()
    : m_consoleCompiler(0)
    , m_maxHistorySize(1024)
    , m_maxCommandHistorySize(30)
    , m_maxLineSize(128)
    , m_fullAccess(false)
    , m_prompt("> ")
    , m_scrollPageSize(8)          // +0x108, read from the ctor body
    , m_scrollPage(0)              // [CHOSEN] - the original ctor does not write +0x10C
    , m_useRelativePaths(false)
    , m_allowMultipleFileLoad(true)
    , m_access(2)
    , m_autoComplLength(10000)     // +0x160, read from the ctor body - the sentinel, see above
    , m_autoComplNoDot(false)      // [CHOSEN] - the original ctor does not write +0x164
    , m_autoComplPresses(0)        // [CHOSEN] - nor +0x168
    , m_exitCallBack(0)
    , m_exitCallBackData(0)
    , m_echoErrors(true)
    , m_echo(true)
    , m_currentLevel(m_levels)
    , m_depth(0)
    , m_fromArchive(false)
    , m_outputRedirectAppend(false)
    , m_handleCommandHook(0)
{
    // m_levels[10] default-construct themselves - ten Level::Level calls, which is the loop at the
    // tail of the original. m_workingPath / m_workingFile / m_outputRedirectFile likewise start
    // empty, which is what the binary's three refcount bumps on the shared empty rep amount to.
    m_currentLineBuffer[0] = '\0';

    // The base's flag, cleared by the original's very last statement.
    m_consoleActive = false;
}

// Destroys member containers. Does not invoke the exit callback or free the console compiler.
OldConsole::~OldConsole()
{
}

// Splits, wraps and buffers console text for getLines; it does not write to a device.
// Drops empty lines, wraps after maxLineSize + 1 characters and prefixes continuations with one
// space. History eviction removes the oldest entry. [CHOSEN, not recovered] std::string accumulation
// replaces the retail fixed stack buffer, whose exact capacity remains [UNRESOLVED].
void OldConsole::output(const std::string& text)
{
    std::string line;

    std::string::size_type i = 0;
    for (;;)
    {
        while (i < text.size() && text[i] != '\n')
        {
            line += text[i];
            ++i;

            // after the append, and strictly greater - see the note above.
            if (static_cast<int>(line.size()) > m_maxLineSize)
            {
                pushOutputLine(line);
                line.assign(1, ' ');          // the continuation indent, counted in the next line
            }
        }

        if (!line.empty())                    // empty lines are dropped, never pushed
            pushOutputLine(line);
        line.clear();

        if (i >= text.size())
            break;
        ++i;                                  // step over the '\n' and start the next line
    }
}

// The bounded push, inlined at both sites in the ELF. Split out here because it is written twice in
// the binary for the same reason a compiler inlines it, not because dice had two of them.
void OldConsole::pushOutputLine(const std::string& line)
{
    if (static_cast<int>(m_lines.size()) >= m_maxHistorySize)
        m_lines.pop_front();                  // evict the oldest, then push - never drops the new

    m_lines.push_back(line);
    ++m_linesWritten;
}

// Returns matching names and values, listed from outermost to innermost scope.
// The filter is a case-insensitive prefix; an empty filter matches everything.
std::string OldConsole::listVariables(const std::string& filter)
{
    std::string out;

    for (int level = 0; level <= m_depth; ++level)     // outermost first - see the note above
    {
        const std::map<std::string, std::string, NoCaseStringCompare>& table =
            m_levels[level].m_variables;

        for (std::map<std::string, std::string, NoCaseStringCompare>::const_iterator it =
                 table.begin(); it != table.end(); ++it)
        {
            // Prefix match bounded by the filter's length, so an empty filter matches everything.
            if (compareIgnoreCase(filter.c_str(), it->first.c_str(),
                                  static_cast<int>(filter.length())) != 0)
                continue;

            std::ostringstream line;
            line << it->first << " " << it->second;
            if (level)
                line << " (" << level << ")";
            line << std::endl;

            out.append(line.str());
        }
    }

    return out;
}

// Identical, over the constants map at Level + 12 instead of Level + 0.
std::string OldConsole::listConstants(const std::string& filter)
{
    std::string out;

    for (int level = 0; level <= m_depth; ++level)
    {
        const std::map<std::string, std::string, NoCaseStringCompare>& table =
            m_levels[level].m_constants;

        for (std::map<std::string, std::string, NoCaseStringCompare>::const_iterator it =
                 table.begin(); it != table.end(); ++it)
        {
            if (compareIgnoreCase(filter.c_str(), it->first.c_str(),
                                  static_cast<int>(filter.length())) != 0)
                continue;

            std::ostringstream line;
            line << it->first << " " << it->second;
            if (level)
                line << " (" << level << ")";
            line << std::endl;

            out.append(line.str());
        }
    }

    return out;
}

// Updates the edited line, history recall or scroll position from a key.
// Enter and Tab use the local testing access policy, or end-user access on mainConsole
// when that policy is disabled. Delete clears the whole line.
// [UNRESOLVED] Return contract beyond the current void declaration.
void OldConsole::updateAsciiKey(char key)
{
    switch (key)
    {
    case 13:                                    // Enter - execute the line
    {
#if !DEDICATED
        InteractiveAccessScope interactiveAccess(this);
        if (isUnlockedInteractiveConsole(this))
            output("[CHOSEN] Local client console full access enabled for this command.");
#endif
        m_currentLineBuffer[m_currentLineLength]     = '\n';
        m_currentLineBuffer[m_currentLineLength + 1] = '\0';

        std::string result;
        const int savedAccess = mainConsole->getAccess();
        mainConsole->setAccess(isUnlockedInteractiveConsole(this) ? 2 : 1);

        executeLine(std::string(m_currentLineBuffer), result, true, true, true, false, false);

        m_currentLineBuffer[0] = '\0';
        m_currentLineLength    = 0;
        mainConsole->setAccess(savedAccess);
        break;
    }

    case 8:                                     // backspace
        if (m_currentLineLength <= 0)
            break;
        --m_currentLineLength;
        m_currentLineBuffer[m_currentLineLength] = '\0';
        break;

    case 127:                                   // Delete - clears the whole line, not one character
        m_currentLineBuffer[0] = '\0';
        m_currentLineLength    = 0;
        break;

    case 9:                                     // Tab - autocomplete with the same local access policy
    {
#if !DEDICATED
        InteractiveAccessScope interactiveAccess(this);
#endif
        const int savedAccess = mainConsole->getAccess();
        mainConsole->setAccess(isUnlockedInteractiveConsole(this) ? 2 : 1);
        autoCompletion();
        mainConsole->setAccess(savedAccess);
        break;
    }

    case 16:                                    // command history: previous
    case 14:                                    // command history: next
    {
        const int count = static_cast<int>(m_commandHistory.size());
        if (!count)
            break;

        m_historyCursor = (key == 16) ? m_historyCursor - 1 : m_historyCursor + 1;

        // A ring, not a clamp - both ends wrap.
        if (m_historyCursor < 0)
            m_historyCursor = count - 1;
        if (m_historyCursor >= count)
            m_historyCursor = 0;

        const std::string& recalled = m_commandHistory[m_historyCursor];
        std::strcpy(m_currentLineBuffer, recalled.c_str());
        m_currentLineLength = static_cast<int>(recalled.length());
        break;
    }

    case 2:                                     // PageUp - scroll back
    case 6:                                     // PageDown - scroll forward
    {
        const int pages = static_cast<int>(m_lines.size()) / m_scrollPageSize;
        if (!pages)
            break;

        m_scrollPage = (key == 2) ? m_scrollPage + 1 : m_scrollPage - 1;

        // clamped at both ends, unlike the history ring above.
        if (m_scrollPage < 0)
            m_scrollPage = 0;
        if (m_scrollPage >= pages)
            m_scrollPage = pages - 1;
        break;
    }

    case 0:                                     // explicitly ignored
        break;

    default:                                    // a printable character
        if (m_currentLineLength >= m_maxLineSize)
            break;                              // the gate that bounds interactive input to 128
        m_currentLineBuffer[m_currentLineLength] = key;
        ++m_currentLineLength;
        m_currentLineBuffer[m_currentLineLength] = '\0';
        break;
    }

    // the tail, on every path. See the block above - these are the multiplies, kept as written.
    m_historyCursor = m_historyCursor * ((key == 16 || key == 14) ? 1 : 0);
    m_scrollPage    = m_scrollPage    * ((key == 2  || key == 6)  ? 1 : 0);
}

// Complete the first two tokens; repeated Tab presses reveal candidates and signatures.
// Preserve access filtering, registration order and the asymmetric command/argument behavior.
// Retail buffer bounds and the argument-prefix off-by-one are recorded defects, not repaired here.
// Argument-registration methods are empty in the dedicated image.
void OldConsole::autoCompletion()
{
    char* const line = m_currentLineBuffer;

    // The temporary the ELF hands to the stringbuf and drops immediately; named here only because
    // `std::istringstream in(std::string(line));` would declare a function.
    const std::string typed(line);
    std::istringstream in(typed);

    std::string command;                            // the first whitespace-delimited token
    std::string argument;                           // the second; empty when there is none
    in >> command >> argument;

    // The repeat-press counter. m_autoComplLength is 10000 out of the constructor, so the first
    // Tab of a session takes the else arm even on an empty line.
    const int length = static_cast<int>(std::strlen(line));
    if (m_autoComplLength == length)
        ++m_autoComplPresses;
    else
        m_autoComplPresses = 0;

    // reads the previous call's m_autoComplNoDot - this call has not written it yet.
    if (m_autoComplPresses == 1 || (!m_autoComplNoDot && m_autoComplPresses == 2))
    {
        std::string echo;
        echo = m_prompt;
        echo += line;
        output(echo);                               // vtable +20
    }

    if (argument[0] != '\0')
    {
        // Argument completion uses the registered candidates; this implementation leaves the table empty.
        std::vector<std::string> matches;

        for (std::map<std::string, bool>::const_iterator it = m_autoComplArguments.begin();
             it != m_autoComplArguments.end(); ++it)
        {
            if (compareIgnoreCase(argument.c_str(), it->first.c_str(),
                                  static_cast<int>(argument.length())) == 0)
                matches.push_back(it->first);
        }

        if (matches.size() == 1)
        {
            std::sprintf(line, "%s %s", command.c_str(), matches[0].c_str());
        }
        else if (matches.size() > 1)
        {
            if (m_autoComplPresses == 2)
            {
                for (std::vector<std::string>::size_type i = 0; i < matches.size(); ++i)
                    output(matches[i]);             // the line itself is left alone
            }
            else
            {
                // The longest common case-insensitive prefix, capped at 100 characters. `differs`
                // is set once and never cleared, so the outer loop stops at the first disagreement.
                bool differs = false;
                int  j       = 0;
                for (j = 0; j < 100; ++j)
                {
                    for (std::vector<std::string>::size_type k = 1; k < matches.size(); ++k)
                    {
                        if (static_cast<int>(matches[k].length()) >= j - 1)   // j-1, as shipped
                        {
                            if (std::tolower(matches[0][j]) != std::tolower(matches[k][j]))
                                differs = true;
                        }
                        else
                        {
                            differs = true;
                        }
                    }

                    if (differs)
                        break;
                }

                const std::string common(matches[0], 0, j);
                std::sprintf(line, "%s %s", command.c_str(), common.c_str());
            }
        }
        // matches.size() == 0 falls through with the line untouched.

        m_autoComplLength   = static_cast<int>(std::strlen(line));
        m_currentLineLength = m_autoComplLength;
        return;
    }

    // Split command completion at the first dot, or at the terminator when no dot exists.
    std::string::size_type dot = 0;
    for (dot = 0; ; ++dot)
    {
        if (command[dot] == '\0')
            break;
        if (command[dot] == '.')
            break;
    }

    std::string object = command.substr(0, dot);
    m_autoComplNoDot   = (command[dot] != '.');

    const ConsoleObjects::ConsoleObjectList& objects = mainConsoleObjects->getConsoleObjects();

    std::string completion;                         // the candidate text, rebuilt per binding
    completion.erase();                             // the binary clears it explicitly; kept

    int                  shortest   = 10000;        // the shortest candidate matched so far
    const ConsoleObject* firstMatch = 0;            // supplies the text that replaces the line
    const ConsoleObject* lastMatch  = 0;            // supplies the yardstick the others are held to
    std::list<const ConsoleObject*> matched;

    if (m_autoComplNoDot)
    {
        // An object name. One offer per name, deduplicated through a local hash table.
        Hash<std::string, std::string, StringHashNoCaseHashFunc> seen;   // 3,011 buckets
        const int n = static_cast<int>(object.length());

        for (ConsoleObjects::ConsoleObjectList::const_iterator it = objects.begin();
             it != objects.end(); ++it)
        {
            const ConsoleObject* const obj = *it;

            if (!obj->isActive())
                continue;
            if (!isUnlockedInteractiveConsole(this) && (m_access & 2) == 0 && (obj->getAccess() & 2) != 0)
                continue;

            // A hit assigns the stored candidate into `completion` and then does nothing with it -
            // vestigial in the original, kept because the assignment is what the binary performs.
            if (seen.get(std::string(obj->getObjectName()), completion))
                continue;

            completion = std::string(obj->getObjectName()) + ".";

            if (compareIgnoreCase(object.c_str(), completion.c_str(), n) == 0)
            {
                lastMatch = obj;
                if (!firstMatch)
                    firstMatch = obj;
                if (static_cast<int>(completion.length()) < shortest)
                    shortest = static_cast<int>(completion.length());

                matched.push_back(obj);

                if (m_autoComplPresses == 1)
                    output(completion);
            }

            // Recorded whether or not it matched, so a non-matching name is not re-tested either.
            const std::string  name   = std::string(obj->getObjectName());
            const unsigned int bucket = seen.bucketOf(name);
            if (!seen.exists(name, bucket))
                seen.insert(name, completion, bucket);
        }
    }
    else
    {
        // a method on one named object. No de-duplication: the names are already distinct. --
        const int n = static_cast<int>(command.length());

        for (ConsoleObjects::ConsoleObjectList::const_iterator it = objects.begin();
             it != objects.end(); ++it)
        {
            const ConsoleObject* const obj = *it;

            if (!obj->isActive())
                continue;
            if (!isUnlockedInteractiveConsole(this) && (m_access & 2) == 0 && (obj->getAccess() & 2) != 0)
                continue;
            if (compareIgnoreCase(object.c_str(), obj->getObjectName()) != 0)
                continue;

            completion = std::string(obj->getObjectName()) + "." + obj->getFunctionName() + " ";

            if (compareIgnoreCase(command.c_str(), completion.c_str(), n) != 0)
                continue;

            lastMatch = obj;
            if (!firstMatch)
                firstMatch = obj;
            if (static_cast<int>(completion.length()) < shortest)
                shortest = static_cast<int>(completion.length());

            matched.push_back(obj);

            if (m_autoComplPresses == 1)
                output(completion);

            if (m_autoComplPresses == 2)
            {
                // the signature line: " <arg> <arg> [<opt> <opt>] -> <return> (<min>, <max>)".
                // The bracket opens at the first argument past getMinNoArgs and closes after the
                // last, so a binding with no optional arguments prints no brackets at all.
                // getMaxNoArgs and getMinNoArgs are re-read every iteration, as shipped.
                const bool optional = (obj->getMinNoArgs() != obj->getMaxNoArgs());

                for (int i = 0; i < obj->getMaxNoArgs(); ++i)
                {
                    if (optional && i == obj->getMinNoArgs())
                        completion += " [";
                    else
                        completion += " ";

                    completion += mainConsoleObjects->typeAsString(std::string(obj->getArgType(i)));
                }

                if (optional)
                    completion += "]";

                completion += " -> ";
                completion += mainConsoleObjects->typeAsString(std::string(obj->getReturnType()));

                if (obj->hasRange())
                {
                    // vtable +60, and it really does answer with two strings - see ConsoleObject.h.
                    const std::pair<std::string, std::string> range = obj->getRangeAsString();
                    completion += " (";
                    completion += range.first;
                    completion += ", ";
                    completion += range.second;
                    completion += ")";
                }

                output(completion);
            }
        }
    }

    // The length the line has right now, before any rewrite - so a completion that finds nothing
    // still counts as a press against the next one.
    m_autoComplLength = length;

    if (!lastMatch)
        return;

    // Grow the accepted prefix until one of the matches disagrees with lastMatch, or until it
    // reaches the shortest candidate. Zeroing `shortest` is how the loop reports a disagreement.
    int accepted = static_cast<int>(command.length());
    while (accepted <= shortest)
    {
        ++accepted;

        bool differs = false;
        for (std::list<const ConsoleObject*>::const_iterator it = matched.begin();
             it != matched.end(); ++it)
        {
            // The ELF spells these as two separate `if`s on the same flag and re-tests it on the
            // fallthrough; an if/else is the same control flow with the same temporaries.
            if (m_autoComplNoDot)
            {
                const std::string candidate = std::string((*it)->getObjectName()) + ".";
                const std::string yardstick = std::string(lastMatch->getObjectName()) + ".";
                if (compareIgnoreCase(yardstick.c_str(), candidate.c_str(), accepted) != 0)
                    differs = true;
            }
            else
            {
                // The displayed candidate omits the trailing space used during matching.
                const std::string candidate = std::string((*it)->getObjectName()) + "."
                                            + (*it)->getFunctionName();
                const std::string yardstick = std::string(lastMatch->getObjectName()) + "."
                                            + lastMatch->getFunctionName();
                if (compareIgnoreCase(yardstick.c_str(), candidate.c_str(), accepted) != 0)
                    differs = true;
            }

            if (differs)
                break;
        }

        if (differs)
            shortest = 0;
    }

    const int keep = accepted - 1;

    std::string full;
    if (m_autoComplNoDot)
        full = std::string(firstMatch->getObjectName()) + ". ";
    else
        full = std::string(firstMatch->getObjectName()) + "." + firstMatch->getFunctionName() + " ";

    std::strncpy(line, full.c_str(), keep);         // unbounded against the 132-byte buffer
    m_autoComplLength   = keep;
    line[keep]          = '\0';
    m_currentLineLength = keep;
}

// Processes one key, writes the current edit line and appends newly emitted output to newLines.
// An extended-key prefix is retained for the next call and returns without updating outputs.
void OldConsole::update(char key, std::string& currentLine, std::vector<std::string>& newLines)
{
    char translated = key;

    if (m_pendingExtendedKey)
    {
        if (translated == 72) translated = 16;      // Up
        if (translated == 80) translated = 14;      // Down
        m_pendingExtendedKey = false;               // cleared even if it was neither
    }

    if (translated == -32)                          // 0xE0 - the prefix; nothing else happens
    {
        m_pendingExtendedKey = true;
        return;
    }

    updateAsciiKey(translated);                     // virtual, vtable +24

    currentLine.assign(m_currentLineBuffer, std::strlen(m_currentLineBuffer));

    // Everything output() buffered since the last call. See the note above.
    int first = static_cast<int>(m_lines.size()) - m_linesWritten;
    if (first < 0)
        first = 0;

    for (; first < static_cast<int>(m_lines.size()); ++first)
        newLines.push_back(m_lines[first]);

    m_linesWritten = 0;
}

// Applies the first active mapped navigation input; each axis must exceed 0.5.
void OldConsole::updateGameInput(const GameInput& input)
{
    const int savedAccess = mainConsole->getAccess();
    mainConsole->setAccess(1);                       // mainConsole, not this - as run() does

    if ((input.mappingFlags & (1ULL << GIUp)) && input.inputs[GIUp] > 0.5f)
        updateAsciiKey(16);
    else if ((input.mappingFlags & (1ULL << GIDown)) && input.inputs[GIDown] > 0.5f)
        updateAsciiKey(14);
    else if ((input.mappingFlags & (1ULL << GIPageUp)) && input.inputs[GIPageUp] > 0.5f)
        updateAsciiKey(2);
    else if ((input.mappingFlags & (1ULL << GIPageDown)) && input.inputs[GIPageDown] > 0.5f)
        updateAsciiKey(6);

    mainConsole->setAccess(savedAccess);
}

// Stores the borrowed compiler pointer without destroying any previous compiler.
void OldConsole::setConsoleCompiler(ConsoleCompiler* compiler)
{
    m_consoleCompiler = compiler;
}

// No per-frame key-macro update is required.
// The fifth member of this class in that state, after the loadCommandHistory quartet. Five bytes
// is a push/pop/ret: there is no body to write, and inventing one would be fiction.
void OldConsole::updateKeyMacros(const GameInput&) { }

// Binds script to key 1 through 12. Returns false for other keys; an empty script is a valid replacement.
bool OldConsole::bindKeyToConsoleScript(int key, const std::string& script)
{
    if (static_cast<unsigned int>(key - 1) > 11u)
        return false;

    m_keyBindings[key - 1].assign(script);
    return true;
}

// Returns the working file and line formatted as file(line).
std::string OldConsole::getWorkingFileAndLine() const
{
    return formatString("%s(%d)", getWorkingFile().c_str(), m_currentScriptLine);
}

// Replaces the edited line. The fixed 256-byte copy exceeds the declared 132-byte buffer;
// callers must account for this existing overflow behavior.
void OldConsole::setCurrentLine(std::string& line)
{
    std::strncpy(m_currentLineBuffer, line.c_str(), 256);   // 256 into 132 - see above
    m_currentLineLength = static_cast<int>(std::strlen(m_currentLineBuffer));
}

// Stores the history limit and trims oldest entries.
// The command-history setter trims against the output-history limit, preserving the existing behavior.
void OldConsole::setMaxHistorySize(int size)
{
    m_maxHistorySize = size;

    while (static_cast<int>(m_lines.size()) > m_maxHistorySize)
        m_lines.pop_front();
}

void OldConsole::setMaxCommandHistorySize(int size)
{
    m_maxCommandHistorySize = size;

    // m_maxHistorySize, not m_maxCommandHistorySize. Deliberate - see the block above.
    while (static_cast<int>(m_commandHistory.size()) > m_maxHistorySize)
        m_commandHistory.pop_front();
}

// Appends the visible history window and current prompt to out without clearing it.
// rows reserves space for the prompt and, when scrolled back, a 60-character '+' marker.
// The current input line is always appended, even when no history fits.
void OldConsole::getLines(int rows, std::vector<std::string>& out) const
{
    const int total = static_cast<int>(m_lines.size());

    // Built up front, exactly as both ports do, and pushed last.
    std::string currentLine = m_prompt;

    int scrollOffset = 0;
    int count        = total;

    if (total > rows - 1)
    {
        count        = rows - 1;
        scrollOffset = total - (rows - 1) - m_scrollPageSize * m_scrollPage;
        if (scrollOffset < 0)
            scrollOffset = 0;
    }

    if (m_scrollPage)
        --count;                        // the truncation bar takes a row from the history

    for (int i = 0; i < count; ++i)
        out.push_back(m_lines[scrollOffset + i]);

    if (m_scrollPage)
    {
        // 60 '+' characters. ELF 0x88DC700.
        out.push_back(std::string(
            "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"));
    }

    currentLine.append(m_currentLineBuffer);
    out.push_back(currentLine);
}

// Variable and constant names are case-insensitive and local to their declaration scope.
// addVariable overwrites locally, addConstant preserves an existing value, and setVariable
// updates the nearest existing declaration without creating one. Level zero persists across scripts.

// Sets a variable in the current scope. Returns true for a new name, false when replacing an existing value.
bool OldConsole::addVariable(const std::string& name, const std::string& value)
{
    Level& level = m_levels[m_depth];

    std::map<std::string, std::string, NoCaseStringCompare>::iterator it =
        level.m_variables.find(name);

    if (it == level.m_variables.end())
    {
        level.m_variables.insert(std::make_pair(name, value));
        return true;                                // inserted
    }

    it->second.assign(value);                       // ELF: std::string::assign(node + 20, value)
    return false;                                   // overwrote
}

// Adds a constant to the current scope. Returns false and keeps the old value when already declared.
bool OldConsole::addConstant(const std::string& name, const std::string& value)
{
    Level& level = m_levels[m_depth];

    if (level.m_constants.find(name) != level.m_constants.end())
        return false;                               // already present - left alone, no diagnostic

    level.m_constants.insert(std::make_pair(name, value));
    return true;
}


void OldConsole::setEchoErrors(bool v)            { m_echoErrors = v; }             // +0x16D
bool OldConsole::getEchoErrors() const            { return m_echoErrors; }
void OldConsole::setEcho(bool v)                  { m_echo = v; }                   // +0x16E
bool OldConsole::getEcho() const                  { return m_echo; }

void OldConsole::setMaxLineSize(int v)            { m_maxLineSize = v; }            // +0xF8
int  OldConsole::getMaxLineSize() const           { return m_maxLineSize; }
int  OldConsole::getMaxHistorySize() const        { return m_maxHistorySize; }      // +0x40
int  OldConsole::getMaxCommandHistorySize() const { return m_maxCommandHistorySize; } // +0x70

void OldConsole::setUseRelativePaths(bool v)      { m_useRelativePaths = v; }       // +0x114
bool OldConsole::getUseRelativePaths() const      { return m_useRelativePaths; }
void OldConsole::setAllowMultipleFileLoad(bool v) { m_allowMultipleFileLoad = v; }  // +0x120
bool OldConsole::getAllowMultipleFileLoad() const { return m_allowMultipleFileLoad; }

// getWorkingPath/getWorkingFile are `add eax, 118h` / `add eax, 11Ch` - they hand back the member
// itself, not a copy. setWorkingPath is a tail call into std::string::assign on the same address.
const std::string& OldConsole::getWorkingPath() const   { return m_workingPath; }   // +0x118
void OldConsole::setWorkingPath(const std::string& p)   { m_workingPath.assign(p); }
const std::string& OldConsole::getWorkingFile() const   { return m_workingFile; }   // +0x11C

void OldConsole::setAccess(int v)                 { m_access = v; }                 // +0x124
int  OldConsole::getAccess() const                { return m_access; }

// Stores the exit callback and its user data for console-driven shutdown.
void OldConsole::setExitCallBack(void (*fn)(void*), void* userData)
{
    m_exitCallBack     = fn;        // +0x158
    m_exitCallBackData = userData;  // +0x15C
}

// Installs the hook consulted before normal command handling.
void OldConsole::setHandleCommandHook(bool (*hook)(const char*, int))
{
    m_handleCommandHook = hook;     // +0x45C
}

// Retail no-ops: history persistence and completion-argument updates are disabled in this image.
// These empty bodies do not represent unresolved implementations. [UNRESOLVED] corresponding
// client-image behavior; do not infer it from these dedicated-server bodies.
void OldConsole::loadCommandHistory(const std::string&)      { }
void OldConsole::saveCommandHistory(const std::string&)      { }
void OldConsole::addAutoComplArgument(const std::string&)    { }
void OldConsole::removeAutoComplArgument(const std::string&) { }

// Pushes or pops a script scope and updates m_currentLevel.
// Push returns false at depth 9; pop returns false at depth 0. Entry resets the new scope and
// exit resets the departing scope. Neither operation clears the persistent global scope.
bool OldConsole::incDepth()
{
    if (m_depth > 8)
        return false;

    ++m_depth;
    m_levels[m_depth].reset();              // the level being entered, after the increment
    m_currentLevel = &m_levels[m_depth];
    return true;
}

bool OldConsole::decDepth()
{
    if (m_depth <= 0)
        return false;

    m_levels[m_depth].reset();              // the level being left, before the decrement
    --m_depth;
    m_currentLevel = &m_levels[m_depth];
    return true;
}

// Runs a script with full access and clears the flag afterward.
// Nested calls do not restore the outer flag's prior value.
bool OldConsole::runFullAccess(const std::string& file,
                               const std::string& a1, const std::string& a2,
                               const std::string& a3, const std::string& a4,
                               const std::string& a5, const std::string& a6,
                               const std::string& a7, const std::string& a8,
                               std::string& result)
{
    m_fullAccess = true;

    const bool ok = run(file, a1, a2, a3, a4, a5, a6, a7, a8, result);   // virtual, vtable +0x28

    m_fullAccess = false;      // not the saved previous value - see above
    return ok;
}

// Runs filePath through FileManager in a nested console scope and restores access afterward.
// An input path without any dot receives .con; relative resolution uses m_workingPath when enabled.
// Arguments become v_arg1..v_arg8 and v_noArg; the script's v_result is copied to result.
// Returns false on file open/read failure. A true return does not report individual command success.
bool OldConsole::run(const std::string& filePath,
                     const std::string& arg1, const std::string& arg2,
                     const std::string& arg3, const std::string& arg4,
                     const std::string& arg5, const std::string& arg6,
                     const std::string& arg7, const std::string& arg8,
                     std::string& result)
{
    File file;
    std::string fileName = filePath;

    // Append .con only when the entire path contains no dot, including directory components.
    if (filePath.find('.') == std::string::npos)
        fileName = filePath + ".con";

    // Resolves relative script paths against the working directory; a leading separator is stripped.
    if (m_useRelativePaths)
    {
        const char first = fileName.c_str()[0];
        if (first == '/' || first == '\\')
            fileName = std::string(fileName.c_str() + 1);
        else
            fileName = m_workingPath + fileName;
    }
    fixPathDelimiters(fileName, '/');

    // FILEMODE_READ = 2. The third and fourth arguments are literal 0 and 2 in the binary;
    // [UNRESOLVED] what FileAccess 0 and flags 2 mean.
    if (!file.open(fileName, static_cast<IFileSystem::FileMode>(2),
                   static_cast<IFileSystem::FileAccess>(0), 2))
    {
        result = "Could not open file: " + fileName + "!";
        return false;
    }

    // Recorded before the access decision below, and consumed later by handleCommand.
    m_fromArchive = fileManager ? fileManager->isLastFileOpenedFromArchive() : false;

    // Loose scripts run at end-user access when archives are mounted, unless full access was requested.
    // The temporary access change applies to mainConsole, even when another console runs the script.
    int access = getAccess();
    if (!m_fullAccess && !m_fromArchive && fileManager->hasMountedArchives())
        access = 1;

    const int savedAccess = mainConsole->getAccess();
    mainConsole->setAccess(access);

    const int fileSize = file.getSize();
    if (fileSize > 0)
    {
        // +2, because the buffer gets a newline and a NUL appended below.
        char* text = new char[fileSize + 2];

        const unsigned int bytesRead = file.read(text, fileSize);
        if (bytesRead != static_cast<unsigned int>(fileSize))
        {
            result = "Could not read file: " + fileName + "!";
            mainConsole->setAccess(savedAccess);
            delete[] text;                     // see the leak note at the end of this function
            return false;
        }

        // A newline is appended to every script. That is why a .con file whose last line has no
        // trailing newline still executes that line - the parser never sees an unterminated tail.
        text[bytesRead]     = '\n';
        text[bytesRead + 1] = '\0';

        const int savedDepth = m_depth;
        incDepth();                            // a fresh Level: the script's variables are scoped

        const std::string savedScript     = m_currentScript;
        const int         savedScriptLine = m_currentScriptLine;
        m_currentScript     = fileName;
        m_currentScriptLine = 0;

        const std::string savedWorkingPath = m_workingPath;
        const std::string savedWorkingFile = m_workingFile;

        if (m_useRelativePaths)
        {
            // Prefer the final forward slash, falling back to a backslash when absent.
            std::string::size_type slash = fileName.rfind('/');
            if (slash == std::string::npos)
                slash = fileName.rfind('\\');

            m_workingPath = fileName.substr(0, slash + 1);
            if (m_consoleCompiler)
                m_consoleCompiler->setWorkingDir(m_workingPath);
        }
        m_workingFile = fileName;

        // Argument count is the highest nonempty slot; empty arguments between populated slots still count.
        int argCount = 0;
        if      (!arg8.empty()) argCount = 8;
        else if (!arg7.empty()) argCount = 7;
        else if (!arg6.empty()) argCount = 6;
        else if (!arg5.empty()) argCount = 5;
        else if (!arg4.empty()) argCount = 4;
        else if (!arg3.empty()) argCount = 3;
        else if (!arg2.empty()) argCount = 2;
        else if (!arg1.empty()) argCount = 1;

        std::ostringstream count;
        count << argCount;

        addVariable("v_noArg", count.str());   // virtual, vtable +0x40
        addVariable("v_arg1", arg1);
        addVariable("v_arg2", arg2);
        addVariable("v_arg3", arg3);
        addVariable("v_arg4", arg4);
        addVariable("v_arg5", arg5);
        addVariable("v_arg6", arg6);
        addVariable("v_arg7", arg7);
        addVariable("v_arg8", arg8);
        addVariable("v_result", "");           // seeded empty, read back below

        // Pass the entire script to executeLine, which handles line splitting.
        std::string discarded;
        executeLine(std::string(text), discarded, false, false, false, false, true);

        // The script's return value comes back through the variable, not through executeLine.
        exchangeVariable("v_result", result);

        m_workingPath = savedWorkingPath;
        m_workingFile = savedWorkingFile;
        if (m_consoleCompiler)
            m_consoleCompiler->setWorkingDir(m_workingPath);

        m_currentScript     = savedScript;
        m_currentScriptLine = savedScriptLine;

        // A while loop, not A single decDepth(). A script that opens blocks (if/while/beginRem)
        // and never closes them leaves the console deeper than it started; this unwinds all of it.
        // Deliberate robustness against malformed content, and it is why one bad .con cannot
        // permanently corrupt the console's nesting state.
        while (savedDepth < m_depth)
            decDepth();

        delete[] text;
    }

    mainConsole->setAccess(savedAccess);
    return true;
}

// [CHOSEN, not recovered] The short-read failure path frees its file buffer, avoiding the original leak.

// Executes file in the caller's scope and access level, exposing arguments as console variables.
// Relative resolution uses m_workingPath; names without any dot receive .inc. Working-file state
// is restored afterward and v_result is copied to result. Returns false on open/read failure,
// except that a missing file explicitly requested with the .tweak extension succeeds.
bool OldConsole::include(const std::string& file,
                         const std::string& arg1, const std::string& arg2,
                         const std::string& arg3, const std::string& arg4,
                         const std::string& arg5, const std::string& arg6,
                         const std::string& arg7, const std::string& arg8,
                         std::string& result)
{
    File f;
    std::string fileName = file;

    // Same "no dot ANYWHERE" test as run() - not "no extension". A dot in a directory name
    // suppresses the default just as it does there. Only the default itself differs: .inc.
    if (file.find('.') == std::string::npos)
        fileName = file + ".inc";

    // A leading separator resolves from the virtual mount root; other names use the working path.
    if (m_useRelativePaths)
    {
        const char first = fileName.c_str()[0];
        if (first == '/' || first == '\\')
            fileName = std::string(fileName.c_str() + 1);
        else
            fileName = m_workingPath + fileName;
    }

    fixPathDelimiters(fileName, '/');

    // FILEMODE_READ = 2; the 0 and 2 are literal in both ports. [UNRESOLVED] what FileAccess 0 and
    // flags 2 mean - carried over from run(), same call.
    if (!f.open(fileName, static_cast<IFileSystem::FileMode>(2),
                static_cast<IFileSystem::FileAccess>(0), 2))
    {
        // `file`, not `fileName` - see point 2 in the block above. compareIgnoreCase answers 0
        // on a match, so this is "the caller asked for a .tweak": succeed silently.
        if (compareIgnoreCase(extractFileExt(file).c_str(), "tweak") == 0)
            return true;

        result = "Could not open file: " + fileName + "!";
        return false;
    }

    const int fileSize = f.getSize();
    if (fileSize > 0)
    {
        char* text = new char[fileSize + 2];        // +2: a newline and a NUL are appended below

        const unsigned int bytesRead = f.read(text, fileSize);
        if (bytesRead != static_cast<unsigned int>(fileSize))
        {
            result = "Could not read file: " + fileName + "!";
            delete[] text;                          // see the leak note at the end of run()
            return false;
        }

        // The same unconditional newline run() appends, and for the same reason: executeLine is
        // handed the whole file at once and must never see an unterminated final statement.
        text[bytesRead]     = '\n';
        text[bytesRead + 1] = '\0';

        const std::string savedScript     = m_currentScript;
        const int         savedScriptLine = m_currentScriptLine;
        m_currentScript     = fileName;
        m_currentScriptLine = 0;

        const std::string savedWorkingPath = m_workingPath;
        const std::string savedWorkingFile = m_workingFile;

        if (m_useRelativePaths)
        {
            // Derive the working directory from the final separator; a path without one yields an empty directory.
            std::string::size_type slash = fileName.rfind('/');
            if (slash == std::string::npos)
                slash = fileName.rfind('\\');

            m_workingPath = fileName.substr(0, slash + 1);
        }
        m_workingFile = fileName;

        // outside the test above, unlike run(), so an include with relative paths disabled still
        // pushes the (unchanged) working path into the compiler. Both ports agree.
        if (m_consoleCompiler)
            m_consoleCompiler->setWorkingDir(m_workingPath);

        // The highest non-empty slot, not the number of non-empty ones - identical to run(), so an
        // empty middle argument is indistinguishable from an absent one here too.
        int argCount = 0;
        if      (!arg8.empty()) argCount = 8;
        else if (!arg7.empty()) argCount = 7;
        else if (!arg6.empty()) argCount = 6;
        else if (!arg5.empty()) argCount = 5;
        else if (!arg4.empty()) argCount = 4;
        else if (!arg3.empty()) argCount = 3;
        else if (!arg2.empty()) argCount = 2;
        else if (!arg1.empty()) argCount = 1;

        std::ostringstream count;
        count << argCount;

        addVariable("v_noArg", count.str());        // virtual, vtable +0x40
        addVariable("v_arg1", arg1);
        addVariable("v_arg2", arg2);
        addVariable("v_arg3", arg3);
        addVariable("v_arg4", arg4);
        addVariable("v_arg5", arg5);
        addVariable("v_arg6", arg6);
        addVariable("v_arg7", arg7);
        addVariable("v_arg8", arg8);
        addVariable("v_result", "");                // seeded empty; see the trap note above

        // The whole file as one string, exactly as run() does it - the line splitting lives inside
        // executeLine. The five trailing flags are literal in both ports.
        std::string discarded;
        executeLine(std::string(text), discarded, false, false, false, false, true);

        exchangeVariable("v_result", result);

        m_workingPath = savedWorkingPath;
        m_workingFile = savedWorkingFile;
        if (m_consoleCompiler)
            m_consoleCompiler->setWorkingDir(m_workingPath);

        m_currentScript     = savedScript;
        m_currentScriptLine = savedScriptLine;

        // no depth unwind here. run() has `while (savedDepth < m_depth) decDepth();` to repair a
        // script that left blocks open; include() has nothing, because it never pushed. An included
        // file that opens an `if` and never closes it therefore leaves the includer deeper than it
        // started, and only the includer's own run() unwinds it. Confirmed absent in both ports.
        delete[] text;
    }

    // An empty file (size <= 0) skips everything above and still answers true.
    return true;
}

// Adds a c_-prefixed constant to mainConsole. Other names are ignored.
void addConstantToAllConsoles(const std::string& name, const std::string& value)
{
    if (compareIgnoreCase(name.c_str(), "c_", 2) == 0)
        mainConsole->addConstant(name, value);
}

// The main console is a statically constructed object; module registration publishes its address.
namespace {
OldConsole g_mainConsole;      // ds:0x8AC3820 - built by static init
}

// [UNRESOLVED] Name and purpose of this batch-local list; executeLines clears it before each batch.
static std::list<std::string> s_batchLines;
// ds:0x8AC3C88. handleCommand stores the console it is dispatching for into this immediately
// before invoking a binding (0x8443256), so the binding can reach back. [INFERRED] name.
OldConsole* g_executingConsole = 0;

OldConsole* mainConsole = &g_mainConsole;      // ds:0x898CBE4, .data relocation

// Executes each input line with an appended newline and appends one result per line.
// The output list is not cleared. The three flags are forwarded to executeLine.
void OldConsole::executeLines(const std::list<std::string>& lines, std::list<std::string>& results,
                              bool outputToConsole, bool outputError, bool addToHistory)
{
    // Clear the batch state once before executing any line.
    s_batchLines.clear();

    for (std::list<std::string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
    {
        std::string command = *it;
        command.append("\n");

        std::string result;
        executeLine(command, result, outputToConsole, outputError, addToHistory, false, false);

        results.insert(results.end(), result);
    }
}
} // namespace io
} // namespace hfe
} // namespace dice
