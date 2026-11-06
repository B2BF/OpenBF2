// A shot a bot was told about: where it came from, where it went, its per-type battle strengths
// and when it was fired. BotSenses keeps them in its incoming-fire list (+84) and, per shooter,
// in its attacker map (+100); BotMain::event_projectile hands them over.
// Evidence: libaidll.so 1.5.3153 (ia-32). Attested file AIKnowledge/FireObject.cpp (FILE symbol
// beside 0x2D8710, between DangerObject.cpp and Information.cpp); constructors 0x2D8090 /
// 0x2D8280, destructors 0x2D8470 / 0x2D84A0 (not virtual), isWithinRadius 0x2D84D0,
// getBattleStrength 0x2D86B0. Size 0x2C (ia-32).

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_FIREOBJECT_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIKNOWLEDGE_FIREOBJECT_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class FireObject
{
public:
    // ELF 0x2D8090. Copies IAISettings::getNBattleStrengths() floats from `strengths` into an owned
    // array (checkPointer(new float[n])).
    FireObject(Vec3 origin, Vec3 target, float* strengths, bool direct, double time, float decay);
    ~FireObject();                          // ELF 0x2D8470: delete[] the strengths

    // ELF 0x2D84D0: |distance of `position` from the shot's XZ line| <= radius.
    bool isWithinRadius(const Vec3& position, float radius);
    // ELF 0x2D86B0: strengths[type] / ((float)(now - time) * decay + 1).
    float getBattleStrength(int type);

    bool isDirect() const { return m_direct; }      // [INFERRED] inline reader (BotSenses reads +0)
    double getTime() const { return m_time; }       // [INFERRED] inline reader (BotSenses reads +0x20)
    // [INFERRED] inline reader: FireDanger::getPosition copies +0x08..+0x10 (ELF 0x220623).
    const Vec3& getOrigin() const { return m_origin; }
    const Vec3& getTarget() const { return m_target; }   // [INFERRED] inline reader (SensingAgent::evaluateFireObjects reads +0x14)

private:
    /* +0x00 */ bool   m_direct;        // updateFireObjects weights a direct shot x10
    /* +0x04 */ float* m_strengths;     // new float[getNBattleStrengths()]
    /* +0x08 */ Vec3   m_origin;
    /* +0x14 */ Vec3   m_target;
    /* +0x20 */ double m_time;
    /* +0x28 */ float  m_decay;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_FIREOBJECT_H
