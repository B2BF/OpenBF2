// Debug circle that follows a bot's equipment or an information. Evidence: libaidll.so (ia-32)
// 0x371D40..0x3723DF. Placement attested by the __FILE__ string
// "AI/AIDLL/AI/AIUtils/DebugRendering/DPDynamicCircle.cpp".

#include "DPDynamicPrimitives.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x372330, __tcf_0 0x372390, _GLOBAL__I 0x3723C0).
#include <iostream>

#include "../../../../../Core/Debug.h"
#include "../../../../AIInterface/BFKnowledge/DebugRenderer.h"
#include "../../AIBot/BotMain.h"
#include "../../AIKnowledge/Information.h"
#include "../../AIKnowledge/InformationManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x371D40 (C2; C1 0x371DC0).
DPDynamicCircle::DPDynamicCircle(float lifeTime, unsigned int colour, const Vec3& position,
                                 float radius, Bot* bot, DPDynamicTypes /*type*/, const Vec3& normal,
                                 bool filled, int segments)
    : DPStaticCircle(lifeTime, colour, position, radius, normal, filled, segments)
    , m_bot(bot)
    , m_information(0xFFFFFFFFu)
{
}

// ELF 0x371E40 (C2; C1 0x371EC0).
DPDynamicCircle::DPDynamicCircle(float lifeTime, unsigned int colour, const Vec3& position,
                                 float radius, unsigned int information, DPDynamicTypes /*type*/,
                                 const Vec3& normal, bool filled, int segments)
    : DPStaticCircle(lifeTime, colour, position, radius, normal, filled, segments)
    , m_bot(0)
    , m_information(information)
{
}

// ELF 0x371F40 (D2; D1 0x371F80, D0 0x371FC0).
DPDynamicCircle::~DPDynamicCircle()
{
}

// ELF 0x372000. The circle sits at m_position offset by the information's position; types 0 and 1
// keep m_normal, 2 takes the transformation's third row and 3 its second row. An unknown type
// reports and draws with the zero centre and normal.
bool DPDynamicCircle::draw(IDebugRenderer* renderer)
{
    Information* const information
        = informationManager->getInformation(m_bot ? m_bot->getCurrentEquipment() : m_information);
    if (!information)
        return false;

    Vec3 normal = {0.0f, 0.0f, 0.0f};
    Vec3 centre = {0.0f, 0.0f, 0.0f};
    switch (m_type)
    {
    case 0:
    case 1:
    {
        const Vec3& position = information->getPosition();
        centre.x = m_position.x + position.x;
        centre.y = m_position.y + position.y;
        centre.z = m_position.z + position.z;
        normal = m_normal;
        break;
    }
    case 2:
    {
        const Vec3& position = information->getPosition();
        centre.x = m_position.x + position.x;
        centre.y = m_position.y + position.y;
        centre.z = m_position.z + position.z;
        const Mat4& transformation = information->getTransformation();
        normal.x = transformation.m[8];
        normal.y = transformation.m[9];
        normal.z = transformation.m[10];
        break;
    }
    case 3:
    {
        const Vec3& position = information->getPosition();
        centre.x = m_position.x + position.x;
        centre.y = m_position.y + position.y;
        centre.z = m_position.z + position.z;
        const Mat4& transformation = information->getTransformation();
        normal.x = transformation.m[4];
        normal.y = transformation.m[5];
        normal.z = transformation.m[6];
        break;
    }
    case 4:     // [INFERRED] the jump table has a fifth entry that shares the default target
    default:
        Debug(DebugTypeError, "AI/AIDLL/AI/AIUtils/DebugRendering/DPDynamicCircle.cpp", 97, "")
            << "Invalid type.";
        break;
    }
    renderer->drawCircle(normal, centre, m_radius, m_colour, m_filled, m_segments);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
