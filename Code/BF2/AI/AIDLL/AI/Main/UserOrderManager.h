// Owns patrol paths and guard positions and assigns bots to user orders.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_USERORDERMANAGER_H
#define AI_AIDLL_AI_MAIN_USERORDERMANAGER_H

#include <map>
#include <string>

#include "AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// The two ordered objects. Only ever held by pointer here, so a forward declaration is enough; the
// classes themselves belong to the strategic layer (chunk 05).
class GuardPosition;
class PatrolPath;

// `createPatrolPath` and `createGuardPosition` both take one. [UNRESOLVED] enumerators; one dword
// on the stack at every call site.
enum CombatStrengthType : int;

// sizeof == 44, from BF2AI::init step 14. no vtable - the class has no typeinfo in the image.
class UserOrderManager
{
public:
    UserOrderManager();
    ~UserOrderManager();                 // owns both map values

    // written, 10- each
    PatrolPath*    getActivePatrolPath() const;
    GuardPosition* getActiveGuardPosition() const;
    const std::map<std::string, PatrolPath*>&    getPatrolPaths() const;
    const std::map<std::string, GuardPosition*>& getGuardPositions() const;

    // the round-robin tick
    void update();

    // Creation selects the new object even if a duplicate name prevents its registration.
    void createPatrolPath(const std::string&, CombatStrengthType, int);
    void createGuardPosition(const std::string&, const Vec2&, CombatStrengthType, int, int);
    // Returns false when no eligible guard or patrol accepts this side, allowing fallback to SAI.
    bool recruitBot(Bot* bot, int side);

    // First-wins, set by the constructor; `BF2AI::clear` deletes through it.
    static UserOrderManager* m_instance;

private:
    // Owns both maps' pointed-to objects. Offsets describe the original 44-byte ELF object;
    // STL member sizes determine the native layout.
    /*  0 */ PatrolPath* m_activePatrolPath;                        // = 0 getActivePatrolPath
    /*  4 */ std::map<std::string, PatrolPath*> m_patrolPaths;      // getPatrolPaths
    /* 16 */ std::map<std::string, PatrolPath*>::iterator m_patrolCursor;

    // Uninitialized until an active guard position is assigned; do not query it earlier.
    /* 20 */ GuardPosition* m_activeGuardPosition;                  // getActiveGuardPosition

    /* 24 */ std::map<std::string, GuardPosition*> m_guardPositions;    // getGuardPositions
    /* 36 */ std::map<std::string, GuardPosition*>::iterator m_guardCursor;

    // 0 = walking the patrol paths, 1 = walking the guard positions, 2 = one idle tick then back to
    // 0. See update.
    /* 40 */ int m_updatePhase;                                     // = 0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_USERORDERMANAGER_H
