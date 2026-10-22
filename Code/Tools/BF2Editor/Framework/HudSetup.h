// The game's HUD scripts (Menu/HUD/HudSetup/*.con) as an editable document - added: the retail editor left them to
// DICE's MEME editor. hudBuilder's create rows make named nodes; every other row acts on the object the last create
// or setActiveObject selected. Lossless: untouched lines keep their bytes and line ends, an edit rewrites one word
// in place, and an added line takes its neighbours' separators and the file's line end.
#ifndef BF2EDITOR_FRAMEWORK_HUDSETUP_H
#define BF2EDITOR_FRAMEWORK_HUDSETUP_H

#include "Framework/EditorConfig.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace bf2editor
{

// One word of a command line: its text without quotes, and where it stands in the line, quotes included.
struct HudWord
{
    std::string text;
    size_t      begin;
    size_t      end;
    bool        quoted;
};

// A console line of a HUD script (rem lines and beginrem/endrem blocks are not commands).
struct HudCommand
{
    size_t               line;
    std::string          object;        // "hudBuilder" as spelt; "" for run and other undotted words
    std::string          method;        // "createPictureNode", "setNodeColor", "run"
    std::vector<HudWord> args;
    // The selection the command runs with (hudBuilder's active object and name): set by the last create - whose
    // node then answers both the node rows and its kind's own rows (through its data object) - or setActiveObject.
    std::string          selected;
    std::string          selectedParent;
    bool                 selectedByCreate;
};

// hudBuilder's selection: the active object's name and parent, and whether a create (rather than setActiveObject)
// made it.
struct HudSelection
{
    std::string name;
    std::string parent;
    bool        byCreate = false;
};

// One script: its lines and commands. index() needs the selection the scripts run before it left.
class HudScript
{
public:
    HudScript();

    void parse(const std::string& text);
    std::string render() const;
    bool hasChanges() const { return render() != m_original; }
    // The text as it is now counts as saved.
    void markSaved() { m_original = render(); }
    // The text as parsed or last marked saved.
    const std::string& original() const { return m_original; }

    // The commands found from `selection` at the script's start; `selection` is left as the last command leaves it.
    // `visit`, when given, sees each command as it is indexed and may change the selection (a run line runs another
    // script, which leaves its own).
    void index(HudSelection& selection,
               const std::function<void(size_t command, HudSelection& selection)>& visit = nullptr);
    const std::vector<HudCommand>& commands() const { return m_commands; }
    size_t lineCount() const { return m_lines.size(); }
    const std::string& line(size_t index) const { return m_lines[index].text; }

    // A word of a command rewritten in place (quoted when it was, or holds blanks, or is empty). False when the
    // command has no such word.
    bool setWord(size_t command, size_t word, const std::string& value);
    // The command's words from `first` on replaced by `values`: existing words in place, extra ones appended after
    // a blank, missing ones dropped with the blanks before them.
    bool setWords(size_t command, size_t first, const std::vector<std::string>& values);
    // A command line inserted after `afterLine`: `<object>.<method><sep><args>`, the separator copied from that line's
    // first gap (a tab when it has none). Returns the new line's index.
    size_t insertLine(size_t afterLine, const std::string& object, const std::string& method,
                      const std::vector<std::string>& args);
    // The line taken out.
    void removeLine(size_t line);
    // A line's text replaced (its line end kept), or a whole line put before `line` (at the end for lineCount())
    // with the file's line end.
    bool setLineText(size_t line, const std::string& text);
    bool insertLineText(size_t line, const std::string& text);
    // The words of a line as the console splits them (the method word first).
    static std::vector<HudWord> words(const std::string& text);
    // A word as a line spells it: quoted when it was, or holds blanks, or is empty.
    static std::string spell(const std::string& value, bool wasQuoted);

private:
    struct Line
    {
        std::string text;       // without its line end
        std::string end;        // "\r\n", "\n" or "" (the last line without one)
    };
    std::string newline() const;
    // index() again from the last start, without a visitor (the set re-indexes in run order after an edit).
    void reindex();

    std::vector<Line>       m_lines;
    std::vector<HudCommand> m_commands;
    std::string             m_original;
    HudSelection            m_start;
};

// A command of the set: its script and its index in that script.
struct HudRef
{
    size_t script;
    size_t command;
};

// A node a create row makes.
struct HudNode
{
    std::string         name;
    std::string         parent;
    std::string         kind;           // "Picture" from createPictureNode, as spelt
    HudRef              create;
    int                 rectWord;       // the create's word holding x (then y, width, height); -1 for none
    std::vector<HudRef> commands;       // the rows acting on it (its own and its data object's), in run order
};

// A condition a node's display needs, from its show rows and its parents': `variable` set to `value`.
struct HudShowCondition
{
    std::string variable;
    std::string type;                   // "bool", "int" or "float" as the row reads it; "bool|int" for NOT
    std::string value;                  // what satisfies it ("1", "0", a number)
    std::string node;                   // the node whose row asks for it
};

// The scripts HudSetupMain.con runs, in run order, and the nodes they make.
class HudSetup
{
public:
    // Reads `path` with `read`, returning false (and leaving `text` alone) for a script it cannot read.
    typedef std::function<bool(const std::string& path, std::string& text)> Reader;

    HudSetup();

    // `main` and every script it runs, recursively: `run <file>` resolves against the running script's folder.
    // A script that cannot be read is listed in `missing` and skipped, as the console skips it.
    bool load(const std::string& main, const Reader& read, std::string& error);
    void clear();

    size_t scriptCount() const { return m_scripts.size(); }
    const std::string& scriptPath(size_t script) const { return m_paths[script]; }
    const HudScript& script(size_t script) const { return m_scripts[script]; }
    const std::vector<std::string>& missing() const { return m_missing; }
    const HudCommand& command(const HudRef& ref) const { return m_scripts[ref.script].commands()[ref.command]; }

    const std::vector<HudNode>& nodes() const { return m_nodes; }
    // The node of that name (the last create of it); -1 for none.
    int find(const std::string& name) const;
    // The nodes made under `parent`, in run order.
    std::vector<size_t> children(const std::string& parent) const;
    // The node a selected name stands for: the node itself or its data object ("<name>PictureNode", ...); -1 for
    // none. `dataObject` tells which.
    int nodeOfSelection(const std::string& selected, bool& dataObject) const;
    // The data object name the kind's own rows act on ("" for a split or transform node; the node's name itself for
    // the map kinds, whose transform takes the parent's name).
    static std::string dataObjectName(const std::string& kind, const std::string& name);
    // The create word holding x for that create row (-1 for none: split, map, slider).
    static int rectWordOf(const std::string& createMethod);
    // Whether a hudBuilder row acts on the node (by its name) rather than its data object.
    static bool actsOnNode(const std::string& method);

    // The node's rectangle from its create row. False for a node without one.
    bool rect(size_t node, int out[4]) const;
    bool setRect(size_t node, const int value[4], std::string& error);
    // A command's words from `first` on set.
    bool setWords(const HudRef& ref, size_t first, const std::vector<std::string>& values, std::string& error);
    // A row added for the node after its last line in the script that creates it. The new command's ref.
    bool addCommand(size_t node, const std::string& method, const std::vector<std::string>& args, HudRef& added,
                    std::string& error);
    bool removeCommand(const HudRef& ref, std::string& error);
    // Line-level edits (the history's rows): a script's line replaced, inserted before `line`, or taken out.
    bool setLine(size_t script, size_t line, const std::string& text, std::string& error);
    bool insertLine(size_t script, size_t line, const std::string& text, std::string& error);
    bool removeLine(size_t script, size_t line, std::string& error);
    // Where a new row for the node goes: the script that creates it and the line the row is put before.
    bool newRowPlace(size_t node, size_t& script, size_t& line) const;

    // What the HUD's builder must select to run that command again on the live HUD: the selection the script ran
    // it with, resolved to the node's name or its data object as the row needs. False when it has none.
    bool liveSelection(const HudRef& ref, std::string& parent, std::string& name) const;
    // The conditions that show the node: its own and its parents' show rows (an OR row's alternative is left out).
    std::vector<HudShowCondition> showConditions(size_t node) const;

    // The scripts an edit changed; one marked saved.
    std::vector<size_t> changedScripts() const;
    void markSaved(size_t script) { if (script < m_scripts.size()) m_scripts[script].markSaved(); }

private:
    void index();
    bool loadScript(const std::string& path, const Reader& read, int depth, std::string& error);

    std::vector<std::string> m_paths;
    std::vector<HudScript>   m_scripts;
    std::vector<std::string> m_missing;
    std::vector<HudNode>     m_nodes;
};

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_HUDSETUP_H
