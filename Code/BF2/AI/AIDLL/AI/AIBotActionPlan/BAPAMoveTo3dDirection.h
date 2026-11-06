// Three-dimensional direction move with optional first-execution target initialization.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO3DDIRECTION_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO3DDIRECTION_H

#include "../../../../Math/Vec3.h"
#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B80C0 (9 words), package H-deferred.
class BAPAMoveTo3dDirection : public BAPAction
{
public:
    BAPAMoveTo3dDirection(const Vec3&, float, float, const Vec3&, BAPCondition*, unsigned long long, float, int);   // 0x1D71F0
    BAPAMoveTo3dDirection(float, float, float, BAPCondition*, unsigned long long, float, int);   // 0x1D74A0
    virtual ~BAPAMoveTo3dDirection();   // 0x1D7640

    virtual bool execute(Bot*, bool&, bool&);   // 0x1D7710, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1D78E0, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1D79F0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1D78F0, slot +0x14; from the base declaration
    virtual bool isFinished(Bot*);   // 0x1D79B0, slot +0x20; [INFERRED] ELF int
    // [INFERRED] names; plane-interpreter readers distinguish scalar and vector state.
    /* +0x14 */ float m_maxSpeed;
    /* +0x18 */ float m_maxArriveSpeed;
    /* +0x1C */ float m_currentAltitude; // left uninitialized by both constructors
    /* +0x20 */ float m_altitude;
    /* +0x24 */ Vec3 m_vector24; // [INFERRED] previous velocity sample; EntryHeliMoveToDirection (ELF 0x2B6D30)
    /* +0x30 */ Vec3 m_target;
    /* +0x3C */ Vec3 m_direction;
    /* +0x48 */ Vec3 m_forward;
    /* +0x54 */ Vec3 m_up;
    /* +0x60 */ Vec3 m_right;
    /* +0x6C */ BAPCondition* m_endCondition;
    /* +0x70 */ Vec3 m_vector70;
    /* +0x7C */ Vec3 m_vector7C;
    /* +0x88 */ int m_state;
    /* +0x8C */ float m_distance; // first constructor leaves this uninitialized
    /* +0x90 */ bool m_initialize;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO3DDIRECTION_H
