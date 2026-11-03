// Debug primitives that follow a bot's equipment or an AI object, drawn relative to it.
// Placement: each class's TU path is attested by its __FILE__ string
// (AI/AIDLL/AI/AIUtils/DebugRendering/DP*.cpp); this shared header name is [INFERRED].

#ifndef AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_DPDYNAMICPRIMITIVES_H
#define AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_DPDYNAMICPRIMITIVES_H

#include <string>
#include <vector>

#include "../../DrawPrimitives/DPStatic3dText.h"
#include "../../DrawPrimitives/DPStaticCircle.h"
#include "../../DrawPrimitives/DPStaticLine.h"
#include "../../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// The DPDynamic* primitives resolve their information either through Bot::getCurrentEquipment
// (when constructed with a bot) or through a stored information handle (bot null). The
// DPAIObject* primitives resolve a stored information handle and use its IAIObject. The
// [UNRESOLVED] DPDynamicTypes values select how the stored geometry is placed:
// 0 offset by the position, 1 (line: prefix the position), 2 yaw-rotated about the
// transformation (circle: normal = third matrix row), 3 fully transformed (circle: second row).

// sizeof == 0x44 on ia-32.
class DPDynamicCircle : public DPStaticCircle
{
public:
    // shipped defect: neither constructor stores `type`, so m_type is never initialised.
    DPDynamicCircle(float lifeTime, unsigned int colour, const Vec3& position, float radius, Bot* bot,
                    DPDynamicTypes type, const Vec3& normal, bool filled, int segments);
    DPDynamicCircle(float lifeTime, unsigned int colour, const Vec3& position, float radius,
                    unsigned int information, DPDynamicTypes type, const Vec3& normal, bool filled,
                    int segments);
    virtual ~DPDynamicCircle();

    // Returns false once the information no longer resolves.
    virtual bool draw(IDebugRenderer* renderer);

protected:
    /* +0x38 */ Bot* m_bot;
    /* +0x3C */ unsigned int m_information;
    /* +0x40 */ DPDynamicTypes m_type;
};

// sizeof == 0x3C on ia-32.
class DPDynamicLine : public DPStaticLine
{
public:
    DPDynamicLine(float lifeTime, unsigned int colour, const Vec3& start, const Vec3& end, float width,
                  Bot* bot, DPDynamicTypes type);
    DPDynamicLine(float lifeTime, unsigned int colour, const Vec3& start, const Vec3& end, float width,
                  unsigned int information, DPDynamicTypes type);
    DPDynamicLine(float lifeTime, unsigned int colour, const std::vector<Vec3>& points, float width,
                  Bot* bot, DPDynamicTypes type);
    DPDynamicLine(float lifeTime, unsigned int colour, const std::vector<Vec3>& points, float width,
                  unsigned int information, DPDynamicTypes type);
    virtual ~DPDynamicLine();

    // Rebuilds m_drawPoints from m_points; returns false once the information no longer resolves.
    virtual bool draw(IDebugRenderer* renderer);

protected:
    /* +0x24 */ Bot* m_bot;
    /* +0x28 */ unsigned int m_information;
    /* +0x2C */ DPDynamicTypes m_type;
    /* +0x30 */ std::vector<Vec3> m_drawPoints;
};

// sizeof == 0x44 on ia-32.
class DPDynamic3dText : public DPStatic3dText
{
public:
    DPDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position, const std::string& text,
                    Bot* bot, DPDynamicTypes type, int size, AITextAlignmentX alignX,
                    AITextAlignmentY alignY);
    DPDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                    const std::vector<std::string>& lines, Bot* bot, DPDynamicTypes type, int size,
                    AITextAlignmentX alignX, AITextAlignmentY alignY);
    DPDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position, const std::string& text,
                    unsigned int information, DPDynamicTypes type, int size, AITextAlignmentX alignX,
                    AITextAlignmentY alignY);
    DPDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                    const std::vector<std::string>& lines, unsigned int information,
                    DPDynamicTypes type, int size, AITextAlignmentX alignX, AITextAlignmentY alignY);
    virtual ~DPDynamic3dText();

    // shipped defect: the information is not null-tested, so an unresolved one is dereferenced.
    virtual bool draw(IDebugRenderer* renderer);

protected:
    /* +0x38 */ Bot* m_bot;
    /* +0x3C */ unsigned int m_information;
    /* +0x40 */ DPDynamicTypes m_type;
};

// sizeof == 0x40 on ia-32.
class DPAIObjectCircle : public DPStaticCircle
{
public:
    // shipped defect: `type` is not stored, so m_type is never initialised.
    DPAIObjectCircle(float lifeTime, unsigned int colour, const Vec3& position, float radius,
                     unsigned int information, DPDynamicTypes type, const Vec3& normal, bool filled,
                     int segments);
    virtual ~DPAIObjectCircle();

    virtual bool draw(IDebugRenderer* renderer);

protected:
    /* +0x38 */ unsigned int m_information;
    /* +0x3C */ DPDynamicTypes m_type;
};

// sizeof == 0x38 on ia-32.
class DPAIObjectLine : public DPStaticLine
{
public:
    DPAIObjectLine(float lifeTime, unsigned int colour, const Vec3& start, const Vec3& end,
                   float width, unsigned int information, DPDynamicTypes type);
    DPAIObjectLine(float lifeTime, unsigned int colour, const std::vector<Vec3>& points, float width,
                   unsigned int information, DPDynamicTypes type);
    virtual ~DPAIObjectLine();

    virtual bool draw(IDebugRenderer* renderer);

protected:
    /* +0x24 */ unsigned int m_information;
    /* +0x28 */ DPDynamicTypes m_type;
    /* +0x2C */ std::vector<Vec3> m_drawPoints;
};

// sizeof == 0x44 on ia-32.
class DPAIObject3dText : public DPStatic3dText
{
public:
    DPAIObject3dText(float lifeTime, unsigned int colour, const Vec3& position, const std::string& text,
                     unsigned int information, DPDynamicTypes type, int size,
                     AITextAlignmentX alignX, AITextAlignmentY alignY);
    DPAIObject3dText(float lifeTime, unsigned int colour, const Vec3& position,
                     const std::vector<std::string>& lines, unsigned int information,
                     DPDynamicTypes type, int size, AITextAlignmentX alignX, AITextAlignmentY alignY);
    virtual ~DPAIObject3dText();

    // shipped defect: resolves its information through m_bot, which no constructor writes;
    // m_information is stored but never read.
    virtual bool draw(IDebugRenderer* renderer);

protected:
    /* +0x38 */ Bot* m_bot;
    /* +0x3C */ unsigned int m_information;
    /* +0x40 */ DPDynamicTypes m_type;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_DPDYNAMICPRIMITIVES_H
