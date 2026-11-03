// Patrol route and bot recruitment state (libaidll.so PatrolPath.cpp, 0x350230..0x350F80).
// [INFERRED] Source placement.
#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_PATROLPATH_H
#define AI_AIDLL_AI_AISTRATEGICENTITY_PATROLPATH_H

#include "../../../../Math/Vec3.h"
#include <list>
#include <vector>

namespace dice { namespace hfe { namespace ai {

class Bot;
enum CombatStrengthType : int;

class PatrolPath
{
public:
    PatrolPath(CombatStrengthType strengthType, int side);             // ELF 0x350230 (C2; C1 0x350360)
    PatrolPath(const std::vector<Vec3>& path, CombatStrengthType strengthType, int side);
                                                                       // ELF 0x3503E0 (C2; C1 0x3504C0)
    // Ordinary member destruction; bot pointers in the list are not deleted.
    ~PatrolPath();                                                     // ELF 0x3505A0 (D2; D1 0x350620)

    CombatStrengthType getStrengthType() const;
    const std::vector<Vec3>& getPath() const;
    void addPathPoint(const Vec3& point);
    // The terrain height under the point, three metres up.
    void addPathPoint(const Vec2& point);
    void setActive(bool active);
    bool getActive() const;
    bool active() const;                     // the same load as getActive
    void setSide(int side);
    int getSide() const;
    void setMinDistance(float distance);
    float getMinDistance() const;
    void setPatrolSpeed(float speed);
    float getPatrolSpeed() const;
    void setRequiredResources(int resources);
    int getRequiredResources() const;
    // Signed required-minus-current, with no clamp. recruitBot tests the result > 0.
    int neededResources() const;

    // Installs the death/spawn callbacks, starts the patrol (or handles the dead bot) and
    // counts the bot in.
    void recruit(Bot* bot);
    void botIsAlive(Bot* bot);
    void botIsDead(Bot* bot);
    // As shipped, losing a bot also increments the current resources.
    void lost(Bot* bot);
    void update();

private:
    // Member names [INFERRED] from the constructor, getters and recruitment increment.
    /* ELF +0  / PE +0  */ std::vector<Vec3> m_path;
    /* ELF +12 / PE +16 */ CombatStrengthType m_strengthType;
    /* ELF +16 / PE +20 */ bool m_active;
    /* ELF +20 / PE +24 */ int m_side;
    /* ELF +24 / PE +28 */ int m_currentResources;
    /* ELF +28 / PE +32 */ int m_requiredResources;
    /* ELF +32 / PE +36 */ float m_minDistance;
    /* ELF +36 / PE +40 */ float m_patrolSpeed;
    /* ELF +40 / PE +44 */ std::list<Bot*> m_bots;
};

} } }

#endif
