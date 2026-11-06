// Connects engine requests to AI pathfinding.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_BFPATHFINDINGCOURIER_H
#define AI_AIDLL_AI_MAIN_BFPATHFINDINGCOURIER_H

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

#include "AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// Pathfinding-courier interface. Registration guards the roster static, which may remain
// stale until updateStaticMethods refreshes it.
class IAIPathfindingCourier
{
public:
    IAIPathfindingCourier();
    virtual ~IAIPathfindingCourier();
    // ELF table 2E8200..2E85A0 mangles every query const. The three predicates
    // forward bool (AL, zero-extended on ELF). PE table 1027C550 agrees; MSVC
    // reverses the adjacent isValidPosition overloads: simple query +0C, full
    // query +10, versus ELF +14/+10. Let the compiler form the native table.
    virtual int getNumberOfVehicles() const = 0;
    virtual bool isVehicleUsed(unsigned int) const = 0;
    virtual bool isValidPosition(int, int, const Vec3&) const = 0;
    virtual bool isValidPosition(const Vec3&) const = 0;
    // Returns by value (ELF `ret $4`: hidden result pointer), not void.
    virtual Vec3 getLastValidSquadleaderPosition(const Vec3&) const = 0;
    virtual int getVehicleNumberFromName(const std::string&) const = 0;
    // bool
    // with no widening before `ret 10h` - an int return would need the `movzx` it lacks. (The ELF's
    // `movzx eax, al` is g++ 3.x's habit for every bool return, e.g. InformationFactory's.)
    virtual bool findClosestValidInfantryPolygon(Vec3&, const Vec3&, const Vec3&, unsigned int) const = 0;
};

// BFPathfindingCourier - sizeof == 4, from BF2AI::init step 5.
class BFPathfindingCourier : public IAIPathfindingCourier
{
public:
    BFPathfindingCourier();
    virtual ~BFPathfindingCourier();

    // IAIPathfindingCourier
    virtual int getNumberOfVehicles() const;
    virtual bool isVehicleUsed(unsigned int) const;
    virtual bool isValidPosition(int, int, const Vec3&) const;
    virtual bool isValidPosition(const Vec3&) const;
    virtual Vec3 getLastValidSquadleaderPosition(const Vec3&) const;
    virtual int getVehicleNumberFromName(const std::string&) const;
    virtual bool findClosestValidInfantryPolygon(Vec3&, const Vec3&, const Vec3&, unsigned int) const;

};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_BFPATHFINDINGCOURIER_H
