// Exposes engine debug rendering to the AI.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_DEBUGRENDERER_H
#define AI_AIINTERFACE_BFKNOWLEDGE_DEBUGRENDERER_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "AIAdapterTypes.h"

namespace dice {
namespace hfe {

class IBase;

namespace ai {

class IDebugRenderer
{
public:
    IDebugRenderer();
    virtual ~IDebugRenderer();

    virtual void drawAll() = 0;
    virtual void reset() = 0;
    virtual int drawLine(const std::vector<Vec3>& points, unsigned int colour, float width) const = 0;
    virtual int drawCircle(const Vec3& normal, const Vec3& centre, float radius, unsigned int colour, bool filled, int segments) const = 0;
    virtual int drawScreenText(const Vec2& position, const std::vector<std::string>& lines, int size, unsigned int colour, AITextAlignmentX ax, AITextAlignmentY ay) const = 0;
    virtual int draw3dText(const Vec3& position, const std::vector<std::string>& lines, int size, unsigned int colour, AITextAlignmentX ax, AITextAlignmentY ay) const = 0;
    virtual int drawTriangle(const std::vector<Vec3>& vertices, const std::vector<unsigned int>& indices, bool filled) const = 0;
    virtual int drawMesh(Vec3* vertices, unsigned int* indices, int count, unsigned int* colours, bool filled) const = 0;
};

class DebugRenderer : public IDebugRenderer
{
public:
    DebugRenderer();
    virtual ~DebugRenderer();

    virtual void drawAll();
    virtual void reset();
    virtual int drawLine(const std::vector<Vec3>& points, unsigned int colour, float width) const;
    virtual int drawCircle(const Vec3& normal, const Vec3& centre, float radius, unsigned int colour, bool filled, int segments) const;
    virtual int drawScreenText(const Vec2& position, const std::vector<std::string>& lines, int size, unsigned int colour, AITextAlignmentX ax, AITextAlignmentY ay) const;
    virtual int draw3dText(const Vec3& position, const std::vector<std::string>& lines, int size, unsigned int colour, AITextAlignmentX ax, AITextAlignmentY ay) const;
    virtual int drawTriangle(const std::vector<Vec3>& vertices, const std::vector<unsigned int>& indices, bool filled) const;
    virtual int drawMesh(Vec3* vertices, unsigned int* indices, int count, unsigned int* colours, bool filled) const;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_DEBUGRENDERER_H
