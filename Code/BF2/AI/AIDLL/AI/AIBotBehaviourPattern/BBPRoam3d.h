// Airborne roaming toward a per-bot information handle.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPROAM3D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPROAM3D_H

#include "BBPRoam.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC660 (7 words), package H-deferred.
class BBPRoam3d : public BBPRoam
{
public:
    explicit BBPRoam3d(int);   // 0x29BAC0
    virtual ~BBPRoam3d();   // 0x29BB70

    virtual BotScript* createPlan(Bot*);   // 0x29BC60, slot +0x08; from the base declaration
    virtual int getRoamType();   // 0x29C140, slot +0x18; from the base declaration

    void init(unsigned int, Bot*);   // 0x29BC10; [INFERRED] ELF void

private:
    /* +0x10 */ unsigned int* m_objects; // [INFERRED] name; uninitialized per-bot handles
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPROAM3D_H
