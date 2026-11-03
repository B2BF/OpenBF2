// Debug circle that follows an AI object. Evidence: libaidll.so (ia-32)
// 0x36FBD0..0x37012F. Placement attested by the __FILE__ string
// "AI/AIDLL/AI/AIUtils/DebugRendering/DPAIObjectCircle.cpp".

#include "DPDynamicPrimitives.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x370080, __tcf_0 0x3700E0, _GLOBAL__I 0x370110).
#include <iostream>

#include "../../../../../Core/Debug.h"
#include "../../../../AIInterface/BFKnowledge/DebugRenderer.h"
#include "../../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../AIKnowledge/Information.h"
#include "../../AIKnowledge/InformationManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x36FBD0 (C2; C1 0x36FC40).
DPAIObjectCircle::DPAIObjectCircle(float lifeTime, unsigned int colour, const Vec3& position,
                                   float radius, unsigned int information, DPDynamicTypes /*type*/,
                                   const Vec3& normal, bool filled, int segments)
    : DPStaticCircle(lifeTime, colour, position, radius, normal, filled, segments)
    , m_information(information)
{
}

// ELF 0x36FCB0 (D2; D1 0x36FCF0, D0 0x36FD30).
DPAIObjectCircle::~DPAIObjectCircle()
{
}

// ELF 0x36FD70. DPDynamicCircle::draw over the information's IAIObject: the circle sits at
// m_position offset by the object's position; types 0 and 1
// keep m_normal, 2 takes the transformation's third row and 3 its second row. An unknown type
// reports and draws with the zero centre and normal.
bool DPAIObjectCircle::draw(IDebugRenderer* renderer)
{
    Information* const information = informationManager->getInformation(m_information);
    if (!information)
        return false;
    IAIObject* const object = information->getAIObject();

    Vec3 normal = {0.0f, 0.0f, 0.0f};
    Vec3 centre = {0.0f, 0.0f, 0.0f};
    switch (m_type)
    {
    case 0:
    case 1:
    {
        const Vec3& position = object->getPosition();
        centre.x = m_position.x + position.x;
        centre.y = m_position.y + position.y;
        centre.z = m_position.z + position.z;
        normal = m_normal;
        break;
    }
    case 2:
    {
        const Vec3& position = object->getPosition();
        centre.x = m_position.x + position.x;
        centre.y = m_position.y + position.y;
        centre.z = m_position.z + position.z;
        const Mat4& transformation = object->getTransformation();
        normal.x = transformation.m[8];
        normal.y = transformation.m[9];
        normal.z = transformation.m[10];
        break;
    }
    case 3:
    {
        const Vec3& position = object->getPosition();
        centre.x = m_position.x + position.x;
        centre.y = m_position.y + position.y;
        centre.z = m_position.z + position.z;
        const Mat4& transformation = object->getTransformation();
        normal.x = transformation.m[4];
        normal.y = transformation.m[5];
        normal.z = transformation.m[6];
        break;
    }
    case 4:     // [INFERRED] the jump table has a fifth entry that shares the default target
    default:
        Debug(DebugTypeError, "AI/AIDLL/AI/AIUtils/DebugRendering/DPAIObjectCircle.cpp", 73, "")
            << "Invalid type.";
        break;
    }
    renderer->drawCircle(normal, centre, m_radius, m_colour, m_filled, m_segments);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
