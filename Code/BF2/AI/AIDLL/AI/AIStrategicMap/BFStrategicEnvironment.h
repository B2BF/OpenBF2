// Exposes the AI strategic environment.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AISTRATEGICMAP_BFSTRATEGICENVIRONMENT_H
#define AI_AIDLL_AI_AISTRATEGICMAP_BFSTRATEGICENVIRONMENT_H

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// IAIStrategicEnvironment - 14 vtable slots.
// The first-wins guard reads AISingletonServer::m_iaiStrategicEnvironment - the DLL-side half of the roster.
class IAIStrategicEnvironment
{
public:
    IAIStrategicEnvironment();
    virtual ~IAIStrategicEnvironment();
    // Return types and constness from the ELF bodies (x87 returns, the ISAI row pointers).
    virtual float* getEnemyStrengths(Bot* bot) const = 0;
    virtual float* getEnemyTypes(Bot* bot) const = 0;
    virtual float getTemperature(Bot* bot) = 0;
    virtual float getTemperature(int object, int side) = 0;
    virtual float getSecurity(Bot* bot) = 0;
    virtual float getSecurity(int object, int side) = 0;
    virtual float getControl(Bot* bot) = 0;
    virtual float getControl(int object, int side) = 0;
    virtual float getFriendlyBattleStr(Bot* bot) = 0;
    virtual float getFriendlyBattleStr(int object, int side) = 0;
    virtual float getHostileBattleStr(Bot* bot) = 0;
    virtual float getHostileBattleStr(int object, int side) = 0;
};

// BFStrategicEnvironment - sizeof == 4, from BF2AI::init step 6.
class BFStrategicEnvironment : public IAIStrategicEnvironment
{
public:
    BFStrategicEnvironment();
    virtual ~BFStrategicEnvironment();

    // IAIStrategicEnvironment
    virtual float* getEnemyStrengths(Bot* bot) const;          // ELF 0x2E8860
    virtual float* getEnemyTypes(Bot* bot) const;              // ELF 0x2E88D0
    virtual float getTemperature(Bot* bot);                    // ELF 0x2E8940
    virtual float getTemperature(int object, int side);        // ELF 0x2E89C0
    virtual float getSecurity(Bot* bot);                       // ELF 0x2E8A10
    virtual float getSecurity(int object, int side);           // ELF 0x2E8A90
    virtual float getControl(Bot* bot);                        // ELF 0x2E8AE0
    virtual float getControl(int object, int side);            // ELF 0x2E8B60
    virtual float getFriendlyBattleStr(Bot* bot);              // ELF 0x2E8BB0
    virtual float getFriendlyBattleStr(int object, int side);  // ELF 0x2E8C30
    virtual float getHostileBattleStr(Bot* bot);               // ELF 0x2E8C80
    virtual float getHostileBattleStr(int object, int side);   // ELF 0x2E8D00

};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICMAP_BFSTRATEGICENVIRONMENT_H
