// Debug polyline that follows a bot's equipment or an information. Evidence: libaidll.so (ia-32)
// 0x3723E0..0x37312F. Placement attested by the __FILE__ string
// "AI/AIDLL/AI/AIUtils/DebugRendering/DPDynamicLine.cpp".

#include "DPDynamicPrimitives.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x373080, __tcf_0 0x3730E0, _GLOBAL__I 0x373110).
#include <iostream>

#include <cmath>

#include "../../../../../Core/Debug.h"
#include "../../../../AIInterface/BFKnowledge/DebugRenderer.h"
#include "../../AIBot/BotMain.h"
#include "../../AIKnowledge/Information.h"
#include "../../AIKnowledge/InformationManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x3723E0 (C2; C1 0x372470).
DPDynamicLine::DPDynamicLine(float lifeTime, unsigned int colour, const Vec3& start, const Vec3& end,
                             float width, Bot* bot, DPDynamicTypes type)
    : DPStaticLine(lifeTime, colour, start, end, width)
    , m_bot(bot)
    , m_information(0xFFFFFFFFu)
    , m_type(type)
{
}

// ELF 0x372600 (C2; C1 0x372690).
DPDynamicLine::DPDynamicLine(float lifeTime, unsigned int colour, const Vec3& start, const Vec3& end,
                             float width, unsigned int information, DPDynamicTypes type)
    : DPStaticLine(lifeTime, colour, start, end, width)
    , m_bot(0)
    , m_information(information)
    , m_type(type)
{
}

// ELF 0x372500 (C2; C1 0x372580).
DPDynamicLine::DPDynamicLine(float lifeTime, unsigned int colour, const std::vector<Vec3>& points,
                             float width, Bot* bot, DPDynamicTypes type)
    : DPStaticLine(lifeTime, colour, points, width)
    , m_bot(bot)
    , m_information(0xFFFFFFFFu)
    , m_type(type)
{
}

// ELF 0x372720 (C2; C1 0x3727A0).
DPDynamicLine::DPDynamicLine(float lifeTime, unsigned int colour, const std::vector<Vec3>& points,
                             float width, unsigned int information, DPDynamicTypes type)
    : DPStaticLine(lifeTime, colour, points, width)
    , m_bot(0)
    , m_information(information)
    , m_type(type)
{
}

// ELF 0x372820 (D2; D1 0x3728A0, D0 0x372920).
DPDynamicLine::~DPDynamicLine()
{
}

// ELF 0x3729A0. Type 2 derives a yaw from the transformation's third row (x and z normalised by
// their absolute sum, acos of the clamped x, +-pi when z is negative) and rotates every point by
// it before adding the translation. Type 3 transforms every point and then adds the translation a
// second time (shipped defect, kept). An unknown type reports and draws the empty point list.
bool DPDynamicLine::draw(IDebugRenderer* renderer)
{
    m_drawPoints.resize(0);
    Information* const information
        = informationManager->getInformation(m_bot ? m_bot->getCurrentEquipment() : m_information);
    if (!information)
        return false;

    switch (m_type)
    {
    case 0:
    {
        const Vec3& position = information->getPosition();
        for (std::vector<Vec3>::const_iterator point = m_points.begin(); point != m_points.end(); ++point)
        {
            Vec3 placed;
            placed.x = point->x + position.x;
            placed.y = point->y + position.y;
            placed.z = point->z + position.z;
            m_drawPoints.push_back(placed);
        }
        break;
    }
    case 1:
        m_drawPoints.push_back(information->getPosition());
        m_drawPoints.insert(m_drawPoints.end(), m_points.begin(), m_points.end());
        break;
    case 2:
    {
        const Mat4& transformation = information->getTransformation();
        Vec3 rotated = {0.0f, 0.0f, 0.0f};
        const float scale = 1.0f / (std::fabs(transformation.m[8]) + std::fabs(transformation.m[10]));
        float x = transformation.m[8] * scale;
        const float z = transformation.m[10] * scale;
        if (x < -1.0f)
            x = -1.0f;
        else if (x > 1.0f)
            x = 1.0f;
        float angle = std::acos(x);
        if (z < 0.0f)
        {
            if (angle < 0.0f)
                angle -= 3.14159265f;
            else
                angle += 3.14159265f;
        }
        for (std::vector<Vec3>::const_iterator point = m_points.begin(); point != m_points.end(); ++point)
        {
            rotateXZ(*point, rotated, angle);
            Vec3 placed;
            placed.x = rotated.x + transformation.m[12];
            placed.y = rotated.y + transformation.m[13];
            placed.z = rotated.z + transformation.m[14];
            m_drawPoints.push_back(placed);
        }
        break;
    }
    case 3:
    {
        const Mat4& transformation = information->getTransformation();
        Vec3 transformed = {0.0f, 0.0f, 0.0f};
        for (std::vector<Vec3>::const_iterator point = m_points.begin(); point != m_points.end(); ++point)
        {
            transformed.x = point->x * transformation.m[0] + point->y * transformation.m[4]
                + point->z * transformation.m[8] + transformation.m[12];
            transformed.y = point->x * transformation.m[1] + point->y * transformation.m[5]
                + point->z * transformation.m[9] + transformation.m[13];
            transformed.z = point->x * transformation.m[2] + point->y * transformation.m[6]
                + point->z * transformation.m[10] + transformation.m[14];
            Vec3 placed;
            placed.x = transformed.x + transformation.m[12];
            placed.y = transformed.y + transformation.m[13];
            placed.z = transformed.z + transformation.m[14];
            m_drawPoints.push_back(placed);
        }
        break;
    }
    case 4:     // [INFERRED] the jump table has a fifth entry that shares the default target
    default:
        Debug(DebugTypeError, "AI/AIDLL/AI/AIUtils/DebugRendering/DPDynamicLine.cpp", 160, "")
            << "Invalid type.";
        break;
    }
    renderer->drawLine(m_drawPoints, m_colour, m_width);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
