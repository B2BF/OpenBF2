// A timed block of screen-space debug text.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATIC2DTEXT_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATIC2DTEXT_H

#include <string>
#include <vector>

#include "DrawPrimitive.h"
#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// sizeof == 0x34 on ia-32 (DPManager::createStatic2dText allocates 0x34).
class DPStatic2dText : public DrawPrimitive
{
public:
    // One line of text.
    DPStatic2dText(float lifeTime, unsigned int colour, const Vec2& position, const std::string& text, int size,
                   AITextAlignmentX alignX, AITextAlignmentY alignY);
    DPStatic2dText(float lifeTime, unsigned int colour, const Vec2& position,
                   const std::vector<std::string>& lines, int size, AITextAlignmentX alignX,
                   AITextAlignmentY alignY);
    virtual ~DPStatic2dText();

    // Always draws and returns true.
    virtual bool draw(IDebugRenderer* renderer);

    // Copies every attribute except the creation time.
    void getAttributes(float& lifeTime, unsigned int& colour, Vec2& position,
                       std::vector<std::string>& lines, int& size, AITextAlignmentX& alignX,
                       AITextAlignmentY& alignY) const;
    // Replaces every attribute except the creation time.
    void setAttributes(float lifeTime, unsigned int colour, const Vec2& position,
                       const std::vector<std::string>& lines, int size, AITextAlignmentX alignX,
                       AITextAlignmentY alignY);

protected:
    /* +0x14 */ Vec2 m_position;
    /* +0x1C */ std::vector<std::string> m_lines;
    /* +0x28 */ int m_size;
    /* +0x2C */ AITextAlignmentX m_alignX;
    /* +0x30 */ AITextAlignmentY m_alignY;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATIC2DTEXT_H
