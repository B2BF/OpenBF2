// Base of every AI debug primitive: a lifetime, a colour and one draw slot.
// [INFERRED] Source placement (the TU is DrawPrimitive.cpp; the directory is not attested).

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DRAWPRIMITIVE_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DRAWPRIMITIVE_H

namespace dice {
namespace hfe {
namespace ai {

class IDebugRenderer;

// A timed debug primitive. The creation time is taken from the game timer when the primitive is
// constructed; DPManager deletes it once expired() reports true or draw() returns false.
class DrawPrimitive
{
public:
    DrawPrimitive(float lifeTime, unsigned int colour);
    virtual ~DrawPrimitive();

    // Draws the primitive through the renderer. Returns false when the primitive should be
    // discarded (the dynamic primitives do so once their object is gone).
    virtual bool draw(IDebugRenderer* renderer) = 0;

    // True once `time` lies strictly beyond creation time plus lifetime.
    bool expired(double time) const;
    // expired() against the current game time.
    bool expired() const;

protected:
    /* +0x04 */ double m_creationTime;      // ia-32: 4-byte aligned, so the double sits at +4
    /* +0x0C */ float m_lifeTime;
    /* +0x10 */ unsigned int m_colour;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DRAWPRIMITIVE_H
