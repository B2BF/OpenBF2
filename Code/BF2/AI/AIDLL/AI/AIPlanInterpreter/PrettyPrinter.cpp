// Evidence: libaidll.so 1.5.3153 (ia-32), ELF 0x1CAC00..0x1CD520. [INFERRED] placement beside
// BAPDebugger in AIPlanInterpreter (no __FILE__ string).

// static init: ELF 0x1CD160 (__tcf_0 0x1CD1C0, _GLOBAL__I 0x1CD1F0): <iostream>'s Init object only.
#include <iostream>

#include "PrettyPrinter.h"

#include <sstream>
#include <string>

#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIBotActionPlan/BAPTypes.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AIStreamOperators.h"                     // operator<<(ostream&, BAPStatementType)

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1CAC40 (C1), 0x1CAC00 (C2).
PrettyPrinter::PrettyPrinter(BAPStatement* statement, Bot* bot)
    : m_statement(statement)
    , m_bot(bot)
    , m_root(0)
{
    update();
}

// ELF 0x1CACB0 (D1), 0x1CAC80 (D2).
PrettyPrinter::~PrettyPrinter()
{
    destroyPrettyTree(m_root);
}

// ELF 0x1CACE0. The old tree is freed first, but m_root keeps pointing at it until the new root
// is stored.
void PrettyPrinter::update()
{
    destroyPrettyTree(m_root);
    m_root = new Node;
    m_root->m_statement = m_statement;
    m_root->m_depth = 0;
    m_root->m_specialInfo = PrettyActive;
    prettyPrintScriptNode(m_root);
}

// ELF 0x1CAD90. Children first, then the node.
void PrettyPrinter::destroyPrettyTree(Node* node)
{
    if (!node)
        return;
    for (std::list<Node*>::iterator child = node->m_children.begin();
         child != node->m_children.end(); ++child)
        destroyPrettyTree(*child);
    delete node;
}

// ELF 0x1CAE60. The node's text starts from the statement type's name; combiners, IF, WHILE and
// the info wrapper add one child node per sub-statement (depth + 1), and IF and WHILE add marker
// nodes at the node's own depth. A child is on the active path only where the node is: every
// child of a parallel combiner, the current child of a serial one, the IF branch last taken, the
// WHILE body and the wrapped statement. The parallel combiner's finished flags are fetched and
// not used (shipped).
void PrettyPrinter::prettyPrintScriptNode(Node* node)
{
    if (!node || !node->m_statement)
        return;

    std::stringstream typeStream;
    typeStream << node->m_statement->getId();

    switch (node->m_statement->getId())
    {
    case BAPSTParallel:
    {
        BAPCombinerParallel* const parallel = static_cast<BAPCombinerParallel*>(node->m_statement);
        std::stringstream text;
        text << typeStream.str();
        if (parallel->m_debugId != -1)
            text << "(" << parallel->m_debugId << ")";
        node->m_text = text.str();

        parallel->getIsFinished();
        for (std::list<BAPStatement*>::iterator it = parallel->m_statements.begin();
             it != parallel->m_statements.end(); ++it)
        {
            Node* child = new Node;
            child->m_statement = *it;
            child->m_depth = node->m_depth + 1;
            if (node->m_specialInfo == PrettyActive)
                child->m_specialInfo = PrettyActive;
            else
                child->m_specialInfo = PrettyNormal;
            prettyPrintScriptNode(child);
            node->m_children.push_back(child);
            node->m_separators.push_back(std::string("---"));
        }
        break;
    }

    case BAPSTSerial:
    {
        BAPCombinerSerial* const serial = static_cast<BAPCombinerSerial*>(node->m_statement);
        std::stringstream text;
        text << typeStream.str();
        if (serial->m_debugId != -1)
            text << "(" << serial->m_debugId << ")";
        node->m_text = text.str();

        const std::list<BAPStatement*>::iterator current = serial->m_current;
        for (std::list<BAPStatement*>::iterator it = serial->m_statements.begin();
             it != serial->m_statements.end(); ++it)
        {
            Node* child = new Node;
            child->m_statement = *it;
            child->m_depth = node->m_depth + 1;
            if (it == current && node->m_specialInfo == PrettyActive)
                child->m_specialInfo = PrettyActive;
            else
                child->m_specialInfo = PrettyNormal;
            prettyPrintScriptNode(child);
            node->m_children.push_back(child);
            node->m_separators.push_back(std::string("---"));
        }
        break;
    }

    case BAPSTIf:
    {
        BAPFlowCIf* const flow = static_cast<BAPFlowCIf*>(node->m_statement);
        node->m_text = "IF";
        BAPStatement* const trueStatement = flow->getTrueStatement();
        BAPStatement* const active = flow->getActiveStatementDebug();

        Node* trueNode = new Node;
        trueNode->m_statement = trueStatement;
        trueNode->m_depth = node->m_depth + 1;
        trueNode->m_specialInfo = PrettyNormal;
        if (node->m_specialInfo == PrettyActive && trueStatement == active)
            trueNode->m_specialInfo = PrettyActive;
        prettyPrintScriptNode(trueNode);
        node->m_children.push_back(trueNode);

        BAPStatement* const falseStatement = flow->getFalseStatement();
        if (falseStatement)
        {
            Node* falseNode = new Node;
            falseNode->m_statement = falseStatement;
            falseNode->m_depth = node->m_depth + 1;
            falseNode->m_specialInfo = PrettyNormal;
            if (node->m_specialInfo == PrettyActive && falseStatement == active)
                falseNode->m_specialInfo = PrettyActive;

            // the ELSE line goes in before the branch is printed
            Node* elseNode = new Node;
            elseNode->m_text = "ELSE";
            elseNode->m_depth = node->m_depth;
            elseNode->m_specialInfo = PrettyNormal;
            node->m_children.push_back(elseNode);

            prettyPrintScriptNode(falseNode);
            node->m_children.push_back(falseNode);
        }

        Node* endNode = new Node;
        endNode->m_text = "ENDIF";
        endNode->m_depth = node->m_depth;
        endNode->m_specialInfo = PrettyNormal;
        node->m_children.push_back(endNode);
        break;
    }

    case BAPSTWhile:
    {
        BAPFlowCWhile* const flow = static_cast<BAPFlowCWhile*>(node->m_statement);
        node->m_text = typeStream.str();

        Node* loopNode = new Node;
        loopNode->m_statement = flow->getLoopStatement();
        loopNode->m_depth = node->m_depth + 1;
        loopNode->m_specialInfo = node->m_specialInfo;
        prettyPrintScriptNode(loopNode);
        node->m_children.push_back(loopNode);

        Node* endNode = new Node;
        endNode->m_text = "ENDWHILE";
        endNode->m_depth = node->m_depth;
        endNode->m_specialInfo = PrettyNormal;
        node->m_children.push_back(endNode);
        break;
    }

    case BAPSTTrigger:
    {
        BAPATrigger* const trigger = static_cast<BAPATrigger*>(node->m_statement);
        std::stringstream text;
        text << typeStream.str() << " ";
        if (trigger->m_started)
            text << "Trig ";
        else
            text << "NoTr ";
        node->m_text = text.str();
        break;
    }

    case BAPSTTriggerContinously:
    {
        BAPATriggerContinously* const trigger =
            static_cast<BAPATriggerContinously*>(node->m_statement);
        std::stringstream text;
        text << typeStream.str() << " ";
        if (trigger->m_started)
            text << "Trig ";
        else
            text << "NoTr ";
        node->m_text = text.str();
        break;
    }

    case BAPSTInfoWrapper:
    {
        BAPInfoWrapper* const wrapper = static_cast<BAPInfoWrapper*>(node->m_statement);
        node->m_text = typeStream.str();

        Node* child = new Node;
        child->m_statement = wrapper->m_statement;
        child->m_depth = node->m_depth + 1;
        child->m_specialInfo = node->m_specialInfo;
        prettyPrintScriptNode(child);
        node->m_children.push_back(child);
        break;
    }

    default:
        node->m_text = typeStream.str();
        break;
    }
}

// ELF 0x1CCC00.
void PrettyPrinter::getLineStrings(std::list<std::string>& lines) const
{
    getLinesRecursive(lines, m_root);
}

// ELF 0x1CCC40. Each child's lines are collected in a list of their own and spliced on.
void PrettyPrinter::getLinesRecursive(std::list<std::string>& lines, Node* node) const
{
    lines.insert(lines.end(), node->m_text);
    for (std::list<Node*>::iterator child = node->m_children.begin();
         child != node->m_children.end(); ++child)
    {
        std::list<std::string> childLines;
        getLinesRecursive(childLines, *child);
        lines.splice(lines.end(), childLines);
    }
}

// ELF 0x1CCE60.
void PrettyPrinter::getLineDepths(std::list<int>& depths) const
{
    getDepthsRecursive(depths, m_root);
}

// ELF 0x1CCEA0.
void PrettyPrinter::getDepthsRecursive(std::list<int>& depths, Node* node) const
{
    depths.insert(depths.end(), node->m_depth);
    for (std::list<Node*>::iterator child = node->m_children.begin();
         child != node->m_children.end(); ++child)
    {
        std::list<int> childDepths;
        getDepthsRecursive(childDepths, *child);
        depths.splice(depths.end(), childDepths);
    }
}

// ELF 0x1CCFE0.
void PrettyPrinter::getLineSpecialInfos(std::list<PrettySpecialInfo>& infos) const
{
    getSpecialInfosRecursive(infos, m_root);
}

// ELF 0x1CD020.
void PrettyPrinter::getSpecialInfosRecursive(std::list<PrettySpecialInfo>& infos, Node* node) const
{
    infos.insert(infos.end(), node->m_specialInfo);
    for (std::list<Node*>::iterator child = node->m_children.begin();
         child != node->m_children.end(); ++child)
    {
        std::list<PrettySpecialInfo> childInfos;
        getSpecialInfosRecursive(childInfos, *child);
        infos.splice(infos.end(), childInfos);
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
