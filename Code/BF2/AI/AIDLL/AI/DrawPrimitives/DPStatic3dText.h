// A timed block of world-space debug text.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATIC3DTEXT_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATIC3DTEXT_H

#include <string>
#include <vector>

#include "DrawPrimitive.h"
#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// sizeof == 0x38 on ia-32 (DPManager::createStatic3dText allocates 0x38).
class DPStatic3dText : public DrawPrimitive
{
public:
    // One line of text.
    DPStatic3dText(float lifeTime, unsigned int colour, const Vec3& position, const std::string& text, int size,
                   AITextAlignmentX alignX, AITextAlignmentY alignY);
    DPStatic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                   const std::vector<std::string>& lines, int size, AITextAlignmentX alignX,
                   AITextAlignmentY alignY);
    virtual ~DPStatic3dText();

    // Always draws and returns true.
    virtual bool draw(IDebugRenderer* renderer);

    // Copies every attribute except the creation time.
    void getAttributes(float& lifeTime, unsigned int& colour, Vec3& position,
                       std::vector<std::string>& lines, int& size, AITextAlignmentX& alignX,
                       AITextAlignmentY& alignY) const;
    // Replaces every attribute except the creation time.
    void setAttributes(float lifeTime, unsigned int colour, const Vec3& position,
                       const std::vector<std::string>& lines, int size, AITextAlignmentX alignX,
                       AITextAlignmentY alignY);

protected:
    /* +0x14 */ Vec3 m_position;
    /* +0x20 */ std::vector<std::string> m_lines;
    /* +0x2C */ int m_size;
    /* +0x30 */ AITextAlignmentX m_alignX;
    /* +0x34 */ AITextAlignmentY m_alignY;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATIC3DTEXT_H
