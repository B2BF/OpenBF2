// Builds a printable tree of a bot action plan: one node per statement plus IF/ELSE/ENDIF and
// ENDWHILE marker lines, each with its indentation depth and whether it lies on the active path.
// Evidence: libaidll.so 1.5.3153 (ia-32), ELF 0x1CAC00..0x1CD520; the TU's static-init symbol
// names PrettyPrinter's constructor. Nothing else in the DLL refers to the class.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_PRETTYPRINTER_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_PRETTYPRINTER_H

#include <list>
#include <string>

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;
class Bot;

// Per-line marker handed out by getLineSpecialInfos: 1 for lines on the plan's active path (the
// root, the running child of a serial combiner, the chosen IF branch), else 0. Four bytes.
// [INFERRED] an enum (the mangled name does not tell enum from class) and the enumerator names.
enum PrettySpecialInfo
{
    PrettyNormal = 0,
    PrettyActive = 1
};

// 12 bytes: the plan, the bot (stored, never read) and the tree's root.
class PrettyPrinter
{
public:
    // 24 bytes. The implicit constructor leaves the statement, depth and marker unset; the
    // ELSE/ENDIF/ENDWHILE marker nodes never get a statement.
    struct Node
    {
        /* +0x00 */ BAPStatement* m_statement;
        /* +0x04 */ std::string m_text;
        /* +0x08 */ int m_depth;
        /* +0x0C */ PrettySpecialInfo m_specialInfo;
        /* +0x10 */ std::list<Node*> m_children;
        /* +0x14 */ std::list<std::string> m_separators;   // "---" per combiner child; never read
    };

    // Builds the tree at once (update).
    PrettyPrinter(BAPStatement* statement, Bot* bot);   // ELF 0x1CAC40 (C1), 0x1CAC00 (C2)
    ~PrettyPrinter();                                   // ELF 0x1CACB0 (D1), 0x1CAC80 (D2)

    // Rebuilds the tree from the plan's current state.
    void update();
    void destroyPrettyTree(Node* node);
    // Fills `node` (text and children) from its statement; nothing for a null node or statement.
    void prettyPrintScriptNode(Node* node);

    // The tree in pre-order, one entry per line, appended to the caller's list.
    void getLineStrings(std::list<std::string>& lines) const;
    void getLinesRecursive(std::list<std::string>& lines, Node* node) const;
    void getLineDepths(std::list<int>& depths) const;
    void getDepthsRecursive(std::list<int>& depths, Node* node) const;
    void getLineSpecialInfos(std::list<PrettySpecialInfo>& infos) const;
    void getSpecialInfosRecursive(std::list<PrettySpecialInfo>& infos, Node* node) const;

private:
    /* +0x00 */ BAPStatement* m_statement;
    /* +0x04 */ Bot* m_bot;
    /* +0x08 */ Node* m_root;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_PRETTYPRINTER_H
