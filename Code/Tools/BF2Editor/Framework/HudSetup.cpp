// The game's HUD scripts as an editable document (added) - Framework/HudSetup.h.
#include "Framework/HudSetup.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>

namespace bf2editor
{

namespace
{

std::string lower(const std::string& text)
{
    std::string out(text);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

bool blank(char c)
{
    return c == ' ' || c == '\t';
}

// The words of a line as the console splits them: blanks separate, a double-quoted run is one word.
std::vector<HudWord> splitWords(const std::string& text)
{
    std::vector<HudWord> words;
    size_t i = 0;
    while (i < text.size())
    {
        while (i < text.size() && (blank(text[i]) || text[i] == '\r'))
            ++i;
        if (i >= text.size())
            break;
        HudWord word;
        word.begin = i;
        word.quoted = text[i] == '"';
        if (word.quoted)
        {
            const size_t close = text.find('"', i + 1);
            const size_t stop = close == std::string::npos ? text.size() : close;
            word.text = text.substr(i + 1, stop - i - 1);
            i = close == std::string::npos ? text.size() : close + 1;
        }
        else
        {
            while (i < text.size() && !blank(text[i]) && text[i] != '\r')
                ++i;
            word.text = text.substr(word.begin, i - word.begin);
        }
        word.end = i;
        words.push_back(word);
    }
    return words;
}

std::string spelled(const std::string& value, bool wasQuoted)
{
    bool needs = wasQuoted || value.empty();
    for (size_t i = 0; i < value.size() && !needs; ++i)
        needs = blank(value[i]);
    return needs ? "\"" + value + "\"" : value;
}

bool startsWith(const std::string& text, const char* prefix)
{
    return text.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

bool endsWith(const std::string& text, const std::string& suffix)
{
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// "createPictureNode" -> "Picture"; "" for a method that is no create row.
std::string kindOfCreate(const std::string& method)
{
    const std::string low = lower(method);
    if (!startsWith(low, "create") || !endsWith(low, "node") || low.size() <= 10)
        return std::string();
    return method.substr(6, method.size() - 10);
}

std::string folderOf(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

// The script a `run <target>` line in `script` opens, as OldConsole::run resolves it with relative paths on: against
// the running script's folder, from the root with a leading separator, and with .con added when it has no dot.
std::string runTarget(const std::string& script, const std::string& target)
{
    std::string name = target.find('.') == std::string::npos ? target + ".con" : target;
    if (!name.empty() && (name[0] == '/' || name[0] == '\\'))
        name.erase(0, 1);
    else
        name = folderOf(script) + name;
    std::replace(name.begin(), name.end(), '\\', '/');
    return name;
}

} // namespace

// ---- HudScript ------------------------------------------------------------------------------------------------------

HudScript::HudScript()
{
}

void HudScript::parse(const std::string& text)
{
    m_original = text;
    m_lines.clear();
    size_t start = 0;
    while (start < text.size())
    {
        const size_t newline = text.find('\n', start);
        Line line;
        if (newline == std::string::npos)
        {
            line.text = text.substr(start);
            start = text.size();
        }
        else
        {
            const bool crlf = newline > start && text[newline - 1] == '\r';
            line.text = text.substr(start, newline - start - (crlf ? 1 : 0));
            line.end = crlf ? "\r\n" : "\n";
            start = newline + 1;
        }
        m_lines.push_back(line);
    }
    m_start = HudSelection();
    reindex();
}

std::string HudScript::render() const
{
    std::string out;
    for (size_t i = 0; i < m_lines.size(); ++i)
        out += m_lines[i].text + m_lines[i].end;
    return out;
}

void HudScript::reindex()
{
    HudSelection selection = m_start;
    index(selection);
}

void HudScript::index(HudSelection& current, const std::function<void(size_t, HudSelection&)>& visit)
{
    m_start = current;
    m_commands.clear();
    bool inRemBlock = false;
    for (size_t i = 0; i < m_lines.size(); ++i)
    {
        const std::vector<HudWord> words = splitWords(m_lines[i].text);
        if (words.empty())
            continue;
        const std::string first = lower(words[0].text);
        if (first == "beginrem")
        {
            inRemBlock = true;
            continue;
        }
        if (first == "endrem")
        {
            inRemBlock = false;
            continue;
        }
        if (inRemBlock || first == "rem")
            continue;
        HudCommand command;
        command.line = i;
        const size_t dot = words[0].text.find('.');
        if (dot == std::string::npos)
            command.method = words[0].text;
        else
        {
            command.object = words[0].text.substr(0, dot);
            command.method = words[0].text.substr(dot + 1);
        }
        command.args.assign(words.begin() + 1, words.end());
        const bool builder = lower(command.object) == "hudbuilder";
        const std::string method = lower(command.method);
        if (builder && !kindOfCreate(command.method).empty() && command.args.size() >= 2)
        {
            current.name = command.args[1].text;
            current.parent = command.args[0].text;
            current.byCreate = true;
        }
        else if (builder && method == "setactiveobject" && command.args.size() >= 2)
        {
            current.name = command.args[1].text;
            current.parent = command.args[0].text;
            current.byCreate = false;
        }
        command.selected = current.name;
        command.selectedParent = current.parent;
        command.selectedByCreate = current.byCreate;
        m_commands.push_back(command);
        if (visit)
            visit(m_commands.size() - 1, current);
    }
}

std::string HudScript::newline() const
{
    size_t crlf = 0, lf = 0;
    for (size_t i = 0; i < m_lines.size(); ++i)
    {
        if (m_lines[i].end == "\r\n")
            ++crlf;
        else if (m_lines[i].end == "\n")
            ++lf;
    }
    return lf > crlf ? "\n" : "\r\n";
}

bool HudScript::setWord(size_t command, size_t word, const std::string& value)
{
    if (command >= m_commands.size() || word >= m_commands[command].args.size())
        return false;
    const HudWord& old = m_commands[command].args[word];
    std::string& text = m_lines[m_commands[command].line].text;
    text.replace(old.begin, old.end - old.begin, spelled(value, old.quoted));
    reindex();
    return true;
}

bool HudScript::setWords(size_t command, size_t first, const std::vector<std::string>& values)
{
    if (command >= m_commands.size() || first > m_commands[command].args.size())
        return false;
    std::string& text = m_lines[m_commands[command].line].text;
    const std::vector<HudWord> words = splitWords(text);    // the method word, then the arguments
    const size_t have = words.size() - 1;
    const size_t want = first + values.size();
    if (want < have)
    {
        // Drop the surplus words and the blanks before them.
        const size_t from = words[want].end;
        text.erase(from, words.back().end - from);
    }
    else if (want > have)
    {
        std::string extra;
        for (size_t i = have; i < want; ++i)
            extra += " " + spelled(values[i - first], false);
        text.insert(words.back().end, extra);
    }
    // The words both lists have, from the last so earlier positions hold.
    for (size_t i = std::min(have, want); i-- > first;)
        text.replace(words[i + 1].begin, words[i + 1].end - words[i + 1].begin,
                     spelled(values[i - first], words[i + 1].quoted));
    reindex();
    return true;
}

size_t HudScript::insertLine(size_t afterLine, const std::string& object, const std::string& method,
                             const std::vector<std::string>& args)
{
    std::string separator("\t");
    if (afterLine < m_lines.size())
    {
        const std::vector<HudWord> words = splitWords(m_lines[afterLine].text);
        if (words.size() >= 2)
            separator = m_lines[afterLine].text.substr(words[0].end, words[1].begin - words[0].end);
    }
    Line line;
    line.text = object.empty() ? method : object + "." + method;
    for (size_t i = 0; i < args.size(); ++i)
        line.text += (i ? std::string(" ") : separator) + spelled(args[i], false);
    const size_t at = afterLine < m_lines.size() ? afterLine + 1 : m_lines.size();
    if (at == m_lines.size() && !m_lines.empty() && m_lines.back().end.empty())
        m_lines.back().end = newline();         // the new last line keeps the file's lack of a final line end
    else
        line.end = newline();
    m_lines.insert(m_lines.begin() + at, line);
    reindex();
    return at;
}

void HudScript::removeLine(size_t line)
{
    if (line >= m_lines.size())
        return;
    const bool lastWithoutEnd = line + 1 == m_lines.size() && m_lines[line].end.empty();
    m_lines.erase(m_lines.begin() + line);
    if (lastWithoutEnd && !m_lines.empty())
        m_lines.back().end.clear();
    reindex();
}

bool HudScript::setLineText(size_t line, const std::string& text)
{
    if (line >= m_lines.size())
        return false;
    m_lines[line].text = text;
    reindex();
    return true;
}

bool HudScript::insertLineText(size_t line, const std::string& text)
{
    if (line > m_lines.size())
        return false;
    Line added;
    added.text = text;
    if (line == m_lines.size() && !m_lines.empty() && m_lines.back().end.empty())
        m_lines.back().end = newline();         // the new last line keeps the file's lack of a final line end
    else
        added.end = newline();
    m_lines.insert(m_lines.begin() + line, added);
    reindex();
    return true;
}

std::vector<HudWord> HudScript::words(const std::string& text)
{
    return splitWords(text);
}

std::string HudScript::spell(const std::string& value, bool wasQuoted)
{
    return spelled(value, wasQuoted);
}

// ---- HudSetup -------------------------------------------------------------------------------------------------------

HudSetup::HudSetup()
{
}

void HudSetup::clear()
{
    m_paths.clear();
    m_scripts.clear();
    m_missing.clear();
    m_nodes.clear();
}

bool HudSetup::load(const std::string& main, const Reader& read, std::string& error)
{
    clear();
    if (!loadScript(main, read, 0, error))
        return false;
    if (m_scripts.empty())
    {
        error = "cannot read " + main;
        return false;
    }
    index();
    return true;
}

bool HudSetup::loadScript(const std::string& path, const Reader& read, int depth, std::string& error)
{
    if (depth > 16)
    {
        error = "scripts run each other more than 16 deep at " + path;
        return false;
    }
    std::string text;
    if (!read(path, text))
    {
        m_missing.push_back(path);
        return true;
    }
    const size_t self = m_scripts.size();
    m_paths.push_back(path);
    m_scripts.push_back(HudScript());
    m_scripts.back().parse(text);
    // The scripts it runs, in its order (each loaded once per run line, as the console runs it again).
    std::vector<std::string> runs;
    const std::vector<HudCommand>& commands = m_scripts[self].commands();
    for (size_t i = 0; i < commands.size(); ++i)
        if (commands[i].object.empty() && lower(commands[i].method) == "run" && !commands[i].args.empty())
            runs.push_back(runTarget(path, commands[i].args[0].text));
    for (size_t i = 0; i < runs.size(); ++i)
        if (!loadScript(runs[i], read, depth + 1, error))
            return false;
    return true;
}

void HudSetup::index()
{
    // Run order: a script's commands up to a run line, the script it runs, then the rest - the selection a script
    // leaves carries on into the next, as the builder's does.
    struct Runner
    {
        HudSetup& setup;
        std::vector<HudRef> order;
        size_t next;
        explicit Runner(HudSetup& s) : setup(s), next(0) {}

        void run(size_t script, HudSelection& selection)
        {
            ++next;
            setup.m_scripts[script].index(selection, [&](size_t command, HudSelection& current) {
                order.push_back(HudRef{script, command});
                const HudCommand& run = setup.m_scripts[script].commands()[command];
                if (run.object.empty() && lower(run.method) == "run" && !run.args.empty()
                    && next < setup.m_scripts.size()
                    && setup.m_paths[next] == runTarget(setup.m_paths[script], run.args[0].text))
                    this->run(next, current);
            });
        }
    };
    m_nodes.clear();
    if (m_scripts.empty())
        return;
    Runner runner(*this);
    HudSelection selection;
    runner.run(0, selection);

    std::map<std::string, size_t> byName;
    for (size_t i = 0; i < runner.order.size(); ++i)
    {
        const HudRef& ref = runner.order[i];
        const HudCommand& command = this->command(ref);
        const std::string kind = kindOfCreate(command.method);
        if (lower(command.object) != "hudbuilder" || kind.empty() || command.args.size() < 2)
            continue;
        HudNode node;
        node.name = command.args[1].text;
        node.parent = command.args[0].text;
        node.kind = kind;
        node.create = ref;
        node.rectWord = rectWordOf(command.method);
        if (node.rectWord >= 0 && static_cast<size_t>(node.rectWord + 4) > command.args.size())
            node.rectWord = -1;
        byName[node.name] = m_nodes.size();
        m_nodes.push_back(node);
    }
    for (size_t i = 0; i < runner.order.size(); ++i)
    {
        const HudRef& ref = runner.order[i];
        const HudCommand& command = this->command(ref);
        if (lower(command.object) != "hudbuilder" || !kindOfCreate(command.method).empty()
            || lower(command.method) == "setactiveobject" || command.selected.empty())
            continue;
        bool dataObject = false;
        const int node = nodeOfSelection(command.selected, dataObject);
        if (node >= 0)
            m_nodes[node].commands.push_back(ref);
    }
}

int HudSetup::find(const std::string& name) const
{
    for (size_t i = m_nodes.size(); i-- > 0;)
        if (m_nodes[i].name == name)
            return static_cast<int>(i);
    return -1;
}

std::vector<size_t> HudSetup::children(const std::string& parent) const
{
    std::vector<size_t> out;
    for (size_t i = 0; i < m_nodes.size(); ++i)
        if (m_nodes[i].parent == parent)
            out.push_back(i);
    return out;
}

int HudSetup::nodeOfSelection(const std::string& selected, bool& dataObject) const
{
    dataObject = false;
    const int direct = find(selected);
    if (direct >= 0)
        return direct;
    static const char* const kSuffixes[] = {
        "PictureNode", "BarNode", "TextNode", "ObjectMarkerNode", "OccupiedNode", "SelectionNode", "HoverNode",
        "ButtonNode", "ListNode", "EditNode", "TransformListNode", "SliderNode"
    };
    for (size_t i = 0; i < sizeof(kSuffixes) / sizeof(kSuffixes[0]); ++i)
    {
        const std::string suffix(kSuffixes[i]);
        if (!endsWith(selected, suffix))
            continue;
        const int node = find(selected.substr(0, selected.size() - suffix.size()));
        if (node >= 0 && dataObjectName(m_nodes[node].kind, m_nodes[node].name) == selected)
        {
            dataObject = true;
            return node;
        }
    }
    return -1;
}

std::string HudSetup::dataObjectName(const std::string& kind, const std::string& name)
{
    const std::string k = lower(kind);
    if (k == "picture") return name + "PictureNode";
    if (k == "bar" || k == "compass") return name + "BarNode";
    if (k == "text") return name + "TextNode";
    if (k == "objectmarker") return name + "ObjectMarkerNode";
    if (k == "occupied") return name + "OccupiedNode";
    if (k == "objectselection") return name + "SelectionNode";
    if (k == "hover") return name + "HoverNode";
    if (k == "button") return name + "ButtonNode";
    if (k == "list") return name + "ListNode";
    if (k == "edit") return name + "EditNode";
    if (k == "transformlist" || k == "transfomlist") return name + "TransformListNode";
    if (k == "slider") return name + "SliderNode";
    if (k == "minimap" || k == "map") return name;
    return std::string();
}

int HudSetup::rectWordOf(const std::string& createMethod)
{
    const std::string k = lower(kindOfCreate(createMethod));
    if (k == "picture" || k == "text" || k == "button" || k == "edit" || k == "hover" || k == "minimap"
        || k == "objectmarker" || k == "objectselection" || k == "transform" || k == "list" || k == "transformlist"
        || k == "transfomlist")
        return 2;
    if (k == "bar" || k == "compass" || k == "occupied")
        return 3;
    return -1;
}

bool HudSetup::actsOnNode(const std::string& method)
{
    const std::string low = lower(method);
    return startsWith(low, "setnode") || startsWith(low, "addnode");
}

bool HudSetup::rect(size_t node, int out[4]) const
{
    if (node >= m_nodes.size() || m_nodes[node].rectWord < 0)
        return false;
    const HudCommand& create = command(m_nodes[node].create);
    for (int i = 0; i < 4; ++i)
        out[i] = std::atoi(create.args[m_nodes[node].rectWord + i].text.c_str());
    return true;
}

bool HudSetup::setRect(size_t node, const int value[4], std::string& error)
{
    if (node >= m_nodes.size() || m_nodes[node].rectWord < 0)
    {
        error = "the node has no rectangle in its create row";
        return false;
    }
    std::vector<std::string> words;
    for (int i = 0; i < 4; ++i)
        words.push_back(std::to_string(value[i]));
    const HudNode& n = m_nodes[node];
    // Only the four words: a create row's later words (a compass's snap and wrap, a list's row height) stay.
    HudScript& script = m_scripts[n.create.script];
    for (int i = 3; i >= 0; --i)
        script.setWord(n.create.command, static_cast<size_t>(n.rectWord + i), words[i]);
    index();
    return true;
}

bool HudSetup::setWords(const HudRef& ref, size_t first, const std::vector<std::string>& values, std::string& error)
{
    if (ref.script >= m_scripts.size() || ref.command >= m_scripts[ref.script].commands().size())
    {
        error = "no such command";
        return false;
    }
    if (!m_scripts[ref.script].setWords(ref.command, first, values))
    {
        error = "the command has fewer words";
        return false;
    }
    index();
    return true;
}

bool HudSetup::addCommand(size_t node, const std::string& method, const std::vector<std::string>& args, HudRef& added,
                          std::string& error)
{
    if (node >= m_nodes.size())
    {
        error = "no such node";
        return false;
    }
    size_t script = 0, before = 0;
    newRowPlace(node, script, before);
    const std::string object = command(m_nodes[node].create).object;
    const size_t line = m_scripts[script].insertLine(before - 1, object, method, args);
    index();
    const std::vector<HudCommand>& commands = m_scripts[script].commands();
    for (size_t i = 0; i < commands.size(); ++i)
        if (commands[i].line == line)
        {
            added.script = script;
            added.command = i;
            return true;
        }
    error = "the added line made no command";
    return false;
}

bool HudSetup::newRowPlace(size_t node, size_t& script, size_t& line) const
{
    if (node >= m_nodes.size())
        return false;
    const HudNode& n = m_nodes[node];
    script = n.create.script;
    // After the node's last line in the script that creates it, while the builder still has it selected: only within
    // the create's own run of rows (a later setActiveObject block is another selection).
    size_t afterCommand = n.create.command;
    for (size_t i = 0; i < n.commands.size(); ++i)
        if (n.commands[i].script == script && n.commands[i].command > afterCommand)
        {
            bool contiguous = true;
            for (size_t c = afterCommand + 1; c < n.commands[i].command && contiguous; ++c)
            {
                const HudCommand& between = m_scripts[script].commands()[c];
                bool dataObject = false;
                contiguous = nodeOfSelection(between.selected, dataObject) == static_cast<int>(node)
                             && between.selectedByCreate;
            }
            if (contiguous)
                afterCommand = n.commands[i].command;
        }
    line = m_scripts[script].commands()[afterCommand].line + 1;
    return true;
}

bool HudSetup::setLine(size_t script, size_t line, const std::string& text, std::string& error)
{
    if (script >= m_scripts.size() || !m_scripts[script].setLineText(line, text))
    {
        error = "no such line";
        return false;
    }
    index();
    return true;
}

bool HudSetup::insertLine(size_t script, size_t line, const std::string& text, std::string& error)
{
    if (script >= m_scripts.size() || !m_scripts[script].insertLineText(line, text))
    {
        error = "no such line";
        return false;
    }
    index();
    return true;
}

bool HudSetup::removeLine(size_t script, size_t line, std::string& error)
{
    if (script >= m_scripts.size() || line >= m_scripts[script].lineCount())
    {
        error = "no such line";
        return false;
    }
    m_scripts[script].removeLine(line);
    index();
    return true;
}

bool HudSetup::removeCommand(const HudRef& ref, std::string& error)
{
    if (ref.script >= m_scripts.size() || ref.command >= m_scripts[ref.script].commands().size())
    {
        error = "no such command";
        return false;
    }
    m_scripts[ref.script].removeLine(m_scripts[ref.script].commands()[ref.command].line);
    index();
    return true;
}

bool HudSetup::liveSelection(const HudRef& ref, std::string& parent, std::string& name) const
{
    const HudCommand& cmd = command(ref);
    if (cmd.selected.empty())
        return false;
    parent = cmd.selectedParent;
    if (!cmd.selectedByCreate)
    {
        name = cmd.selected;
        return true;
    }
    const int node = find(cmd.selected);
    if (node < 0)
        return false;
    const HudNode& n = m_nodes[node];
    const std::string k = lower(n.kind);
    if (actsOnNode(cmd.method))
        // registerNode leaves the transform's name selected - the map kinds name their transform after the parent.
        name = (k == "minimap" || k == "map") ? n.parent : n.name;
    else
    {
        const std::string data = dataObjectName(n.kind, n.name);
        name = data.empty() ? n.name : data;    // a split or transform node's rows act on the node itself
    }
    return true;
}

std::vector<HudShowCondition> HudSetup::showConditions(size_t node) const
{
    std::vector<HudShowCondition> out;
    std::vector<std::string> seen;
    for (int at = static_cast<int>(node); at >= 0;)
    {
        const HudNode& n = m_nodes[at];
        if (std::find(seen.begin(), seen.end(), n.name) != seen.end())
            break;                              // a parent loop
        seen.push_back(n.name);
        for (size_t i = 0; i < n.commands.size(); ++i)
        {
            const HudCommand& cmd = command(n.commands[i]);
            const std::string method = lower(cmd.method);
            HudShowCondition condition;
            condition.node = n.name;
            if (method == "setnodeshowvariable" && !cmd.args.empty())
            {
                condition.variable = cmd.args[0].text;
                condition.type = "bool";
                condition.value = "1";
            }
            else if (method == "setnodealphavariable" && !cmd.args.empty())
            {
                condition.variable = cmd.args[0].text;
                condition.type = "float";
                condition.value = "1";
            }
            else if (method == "setnodelogicshowvariable" && cmd.args.size() >= 3)
            {
                const std::string op = lower(cmd.args[0].text);
                const int value = std::atoi(cmd.args[2].text.c_str());
                condition.variable = cmd.args[1].text;
                if (op == "equal")
                {
                    condition.type = "int";
                    condition.value = cmd.args[2].text;
                }
                else if (op == "not")
                {
                    // A bool false, or an int other than the value.
                    condition.type = "bool|int";
                    condition.value = std::to_string(value == 0 ? 1 : (value == 1 ? 0 : value + 1));
                }
                else if (op == "and")
                {
                    condition.type = "bool";
                    condition.value = value ? "1" : "0";
                }
                else
                    continue;                   // OR: an alternative, the first condition suffices
            }
            else
                continue;
            out.push_back(condition);
        }
        at = find(n.parent);
    }
    return out;
}

std::vector<size_t> HudSetup::changedScripts() const
{
    std::vector<size_t> out;
    for (size_t i = 0; i < m_scripts.size(); ++i)
        if (m_scripts[i].hasChanges())
            out.push_back(i);
    return out;
}

} // namespace bf2editor
