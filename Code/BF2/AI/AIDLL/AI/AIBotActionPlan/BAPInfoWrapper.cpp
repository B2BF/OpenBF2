// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F5740 (__tcf_0 0x1F58D0, __tcf_1 0x1F5900, _GLOBAL__I 0x1F5970): <iostream>'s Init object, then dice::hfe::ai::infoTypes.
#include <iostream>

#include "BAPInfoWrapper.h"

#include "BAPActionInline.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"
#include "../Main/IBotManager.h"
#include "../AIBot/BotMain.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x4CE220, built by the TU's static initialiser 0x1F5740 (destroyed by __tcf_1 0x1F5900).
std::string infoTypes[10] = { "Fire", "DefuseMines", "MoveTo", "Avoid", "Idle", "Roam", "Scout",
                              "TakeCover", "Change", "Default" };

// ELF 0x1F5470 (C1; C2 0x1F5410). The child's parent is set by the subclasses, not here.
BAPInfoWrapper::BAPInfoWrapper(BAPStatement* statement, int tag)
{
    m_controls = 0;
    m_patternId = tag;
    m_statement = statement;
}

// ELF 0x1F5510 (D1; D2 0x1F54D0, D0 0x1F5550): base only, the child is not deleted here.
BAPInfoWrapper::~BAPInfoWrapper()
{
}

// ELF 0x1F55A0: `bot->getPlanInterpreter()->execute(child, bot, ok, failed)` (slot +8).
bool BAPInfoWrapper::execute(Bot* bot, bool& ok, bool& failed)
{
    PlanInterpreter* interpreter = static_cast<BotMain*>(bot)->getPlanInterpreter();
    return interpreter->execute(m_statement, bot, ok, failed);
}

// ELF 0x1F5990: the child's controls.
unsigned long long BAPInfoWrapper::getControls()
{
    return m_statement->getControls();
}

// ELF 0x1F59B0
BAPStatementType BAPInfoWrapper::getId() const
{
    return BAPSTInfoWrapper;   // 15
}

// ELF 0x1F59C0
bool BAPInfoWrapper::hasDefault()
{
    return true;
}

// ELF 0x1F5600
void BAPInfoWrapper::streamSelf(std::ostream& out) const
{
    IBotManager::instance->tabulate(out);
    const int type = getInfoType();
    out << "InfoWrapper(" << infoTypes[type] << ")" << std::endl;
    IBotManager::instance->tabulate(out);
    out << '{' << std::endl;
    IBotManager::instance->increaseTabLevel();
    if (m_statement)
        out << *m_statement;   // operator<<(std::ostream&, const BAPStatement&) 0x1F82D0
    IBotManager::instance->decreaseTabLevel();
    IBotManager::instance->tabulate(out);
    out << '}' << std::endl;
}

// ELF 0x1F59D0: "Default"
int BAPInfoWrapper::getInfoType() const
{
    return 9;
}

} // namespace ai
} // namespace hfe
} // namespace dice
