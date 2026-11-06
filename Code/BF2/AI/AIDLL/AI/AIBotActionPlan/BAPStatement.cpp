// BAPStatement: recovered by package A-core from libaidll.so (TU BAPStatement.cpp, 0x1F8210..0x1F8440;
// getControls is weak at 0x1CDB10). Every body carries its ELF address.

// static init: ELF 0x1F8300 (__tcf_0 0x1F8360, _GLOBAL__I 0x1F8390): <iostream>'s Init object only.
#include <iostream>

#include "BAPStatement.h"
#include "../Main/IBotManager.h"
#include "../Main/AIStreamOperators.h"


#include <ostream>
#include <string>

namespace dice {
namespace hfe {
namespace ai {

int BAPStatement::news = 0;
int BAPStatement::destroys = 0;

// ~BAPStatement (0x1F83B0) and getControls (0x1CDB10) are weak in the image: inline in the header.

// ELF 0x1F8210.
void BAPStatement::debug(BotScriptDebugger::DebugLevel, std::string& text)
{
    text = "Standard debug.";
}

// ELF 0x1F8260: IBotManager::instance->tabulate(os) (+0x88), then os << getId() (slot +0x10)
// << endl with operator<<(ostream&, BAPStatementType) 0x3C7D90 (Main/AIStreamOperators.cpp).
void BAPStatement::streamSelf(std::ostream& os) const
{
    IBotManager::instance->tabulate(os);
    os << getId() << std::endl;
}

// ELF 0x1F82D0.
std::ostream& operator<<(std::ostream& os, const BAPStatement& statement)
{
    statement.streamSelf(os);
    return os;
}

} // namespace ai
} // namespace hfe
} // namespace dice
