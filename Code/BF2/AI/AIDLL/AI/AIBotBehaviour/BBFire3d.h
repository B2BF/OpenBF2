// Aircraft fire behaviour. Scores sensed and spatial-query targets and initializes BBPFire.
// Recovered from the symbolized Linux 1.50 libaidll.so; offsets refer to the ia-32 image.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIRE3D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIRE3D_H

#include "../../../../Math/Vec3.h"
#include "BotBehaviour.h"

#include <map>

namespace dice {
namespace hfe {

namespace ai {

class AIStrategicObject;
class BBPFire;
class Bot;
class BotMain;
class IPIArmament;
class IPIMobile;
class IPIUnit;
class Information;
class UrgeCurve;

// ELF: vtable 0x4BABA0 (13 words), operator new 0x3C, package G-vehicles.
class BBFire3d : public BotBehaviour
{
public:
    struct BBPFeedback
    {
        // ELF 0x20D190 initializes the sole double to -10000.0.
        double m_time;
    };
    BBFire3d(unsigned char, unsigned char, BBPFire*, UrgeCurve*, float*);   // 0x20BCF0
    virtual ~BBFire3d();   // 0x20BEF0

    virtual float calculateUrgency(Bot*, bool, float);   // 0x20C070, slot +0x28; from the base declaration

    void initializeGenerator(BotMain*, int, bool, bool);   // 0x20D0A0
    void initializeGenerator(unsigned int, const Vec3&, BotMain*, int);   // 0x20D130
    BBPFeedback* getBBPFeedback(unsigned int);   // 0x20D190: find or allocate a feedback record
    float updateUrgencyWithTarget(unsigned int, Information*, double, double, int&, BotMain*, const Vec3&, Information*, IPIArmament*, IPIMobile*, IPIUnit*, int, float, AIStrategicObject*, unsigned int&);   // 0x20D230

    // Fields from the constructor 0x20BCF0 and destructor 0x20BEF0 (operator new 0x3C).
    /* +0x24 */ unsigned int* m_unknown24;   // per-bot weapon index; initialized to -1
    /* +0x28 */ double m_double28;           // last feedback cleanup time; initialized to 0.0
    /* +0x30 */ std::map<unsigned int, BBPFeedback*> m_feedback;   // target -> feedback; the destructor only frees the nodes [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIRE3D_H
