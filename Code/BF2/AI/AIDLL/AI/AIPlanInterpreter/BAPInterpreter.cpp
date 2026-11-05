// BAPInterpreter: recovered by package A-core from libaidll.so (AI/AIDLL/AI/AIPlanInterpreter/
// BAPInterpreter.cpp in the image's strings). Every body carries its ELF address.

// static init: ELF 0x2A9F80 (__tcf_0 0x2A9FE0, _GLOBAL__I 0x2AA010): <iostream>'s Init object only.
#include <iostream>

#include "BAPInterpreter.h"

#include "BAPEntry.h"
#include "../AIBotActionPlan/BotScript.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../../../../Core/Debug.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2A95E0 (C2; C1 0x2A96A0 is the same body). The rows are filled before the mask array is
// allocated, and neither allocation is checked.
BAPInterpreter::BAPInterpreter(int numStatementTypes)
    : m_numStatementTypes(numStatementTypes)
{
    m_entries = new BAPEntry**[numStatementTypes];
    for (int type = 0; type < m_numStatementTypes; ++type)
    {
        m_entries[type] = new BAPEntry*[NumLodLevels];
        for (int lod = 0; lod < NumLodLevels; ++lod)
            m_entries[type][lod] = 0;
    }

    m_active = new unsigned long long[NumLodLevels];
    for (int lod = 0; lod < NumLodLevels; ++lod)
        m_active[lod] = 0;
}

// ELF 0x2A9760 (D2; D1 0x2A9830, D0 0x2A9900). The entries are not owned: the slots are cleared,
// not deleted (AIConsole builds the entries and keeps them).
BAPInterpreter::~BAPInterpreter()
{
    for (int type = 0; type < m_numStatementTypes; ++type)
    {
        for (int lod = 0; lod < NumLodLevels; ++lod)
        {
            if (m_entries[type][lod])
                m_entries[type][lod] = 0;
        }
        delete[] m_entries[type];
        m_entries[type] = 0;
    }
    delete[] m_entries;
    m_entries = 0;
    delete[] m_active;
    m_active = 0;
}

// ELF 0x2A99D0 (209 instructions).
// 1. A bot that has just spawned (Bot +0x118) interprets nothing this tick: ok, not finished.
// 2. Otherwise the statement type (BotScript* treated as BAPStatement, getId +0x10) and the bot's
//    LOD (+0xC) select the active bit; with it set, the registered entry runs, or without an entry
//    the statement's own execute (slot 0). The callee's result is returned.
// 3. A type that is not active at that LOD is an Error-level diagnostic (which aborts), then
//    ok = false, finished = true, return 0.
bool BAPInterpreter::execute(BotScript* script, Bot* bot, bool& ok, bool& finished)
{
    if (bot->isJustSpawned())
    {
        finished = false;
        ok = true;
        return true;
    }

    BAPStatement* statement = static_cast<BAPStatement*>(script);
    const BAPStatementType type = statement->getId();
    const BotLodLevel lod = bot->getLodLevel();

    if ((m_active[lod] >> type) & 1)
    {
        BAPEntry* entry = m_entries[type][lod];
        if (entry == 0)
            return script->execute(bot, ok, finished);
        return entry->execute(script, bot, ok, finished);
    }

    // Retail: Debug(6, "AI/AIDLL/AI/AIPlanInterpreter/BAPInterpreter.cpp", 154, "") << getName()
    // << " " << getBotId() << " lod level " << lod << " cannot interpret \n" << " (" << type << ") "
    // << type << "." with operator<<(ostream&, BotLodLevel) 0x3C78B0 and (ostream&, BAPStatementType)
    // 0x3C7D90 (Main/AIStreamOperators.cpp, not in the tree yet; requested from ai-main). Until those
    // exist the two enums print as numbers. [CHOSEN, not recovered] [RETIRE-WHEN: ai-stream-operators]
    Debug(DebugTypeError, __FILE__, __LINE__, "")
        << bot->getName() << " " << static_cast<unsigned long>(bot->getBotId()) << " lod level "
        << static_cast<int>(lod) << " cannot interpret \n" << " (" << static_cast<int>(type) << ") "
        << static_cast<int>(type) << ".";

    finished = true;
    ok = false;
    return false;
}

// ELF 0x2A9D10.
bool BAPInterpreter::addEntry(BAPEntry* entry)
{
    return addEntry(static_cast<BotLodLevel>(0), entry);
}

// ELF 0x2A9D50. The entry's target id (+0xC) is its statement type. It is registered at `lod` and
// at every coarser LOD up to the first one that already has an entry, and activated there.
bool BAPInterpreter::addEntry(BotLodLevel lod, BAPEntry* entry)
{
    const int type = entry->getTargetId();
    if (m_numStatementTypes <= type || m_entries[type][lod] != 0)
        return false;

    const unsigned long long bit = 1ULL << type;
    for (int level = lod; level < NumLodLevels; ++level)
    {
        if (m_entries[type][level] != 0)
            break;
        m_entries[type][level] = entry;
        m_active[level] |= bit;
    }
    return true;
}

// ELF 0x2A9E10. Clears every slot from `lod` on that holds this entry (no early exit).
bool BAPInterpreter::removeEntry(BotLodLevel lod, BAPEntry* entry)
{
    const int type = entry->getTargetId();
    if (m_numStatementTypes <= type)
        return false;

    const unsigned long long bit = 1ULL << type;
    for (int level = lod; level < NumLodLevels; ++level)
    {
        if (m_entries[type][level] == entry)
        {
            m_entries[type][level] = 0;
            m_active[level] &= ~bit;
        }
    }
    return true;
}

// ELF 0x2A9ED0. Lets a type run its own execute (no entry) at `lod` and the coarser LODs up to the
// first one that has an entry. Refused when an entry is registered at `lod` itself.
bool BAPInterpreter::activateEntry(BAPStatementType type, BotLodLevel lod)
{
    if (m_numStatementTypes <= type || m_entries[type][lod] != 0)
        return false;

    const unsigned long long bit = 1ULL << type;
    for (int level = lod; level < NumLodLevels; ++level)
    {
        if (m_entries[type][level] != 0)
            break;
        m_active[level] |= bit;
    }
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
