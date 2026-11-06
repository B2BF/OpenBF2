// Handles player orders in the AI module.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_AIPLAYERCOURIER_H
#define AI_AIDLL_AI_MAIN_AIPLAYERCOURIER_H

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

#include "AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// Registers the first IAIPlayerCourier in the simple-singleton registry.
// It has no roster static, so teardown resolves it by name.
class IAIPlayerCourier
{
public:
    IAIPlayerCourier();
    virtual ~IAIPlayerCourier();
    // The engine side (AIInterface/IAIPlayerCourier.h) reads the first three as world::IObject*
    // and the fourth as an unsigned grid id; the ELF names are const (0x3B2E90..0x3B3040).
    virtual world::IObject* getSelectedVehicle(int playerId) const = 0;
    virtual world::IObject* getRepairTarget(int playerId) const = 0;
    virtual world::IObject* getReviveTarget(int playerId) const = 0;
    virtual unsigned int getMineDefuseTarget(int playerId) const = 0;
};

// AIPlayerCourier - sizeof == 4, from BF2AI::init step 12.
class AIPlayerCourier : public IAIPlayerCourier
{
public:
    AIPlayerCourier();
    virtual ~AIPlayerCourier();

    // IAIPlayerCourier
    virtual world::IObject* getSelectedVehicle(int playerId) const;
    virtual world::IObject* getRepairTarget(int playerId) const;
    virtual world::IObject* getReviveTarget(int playerId) const;
    virtual unsigned int getMineDefuseTarget(int playerId) const;

};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIPLAYERCOURIER_H
