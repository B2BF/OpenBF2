// World-space debug text meant to follow an AI object. Evidence: libaidll.so (ia-32)
// 0x36F5C0..0x36FB1F. Placement attested by the __FILE__ string
// "AI/AIDLL/AI/AIUtils/DebugRendering/DPAIObject3dText.cpp".

#include "DPDynamicPrimitives.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x36FB20, __tcf_0 0x36FB80, _GLOBAL__I 0x36FBB0; the _GLOBAL__I symbol names this TU's
// DPAIObject3dText constructor, although the ledger files the three rows under CustomCurve.cpp).
#include <iostream>

#include "../../../../../Core/Debug.h"
#include "../../../../AIInterface/BFKnowledge/DebugRenderer.h"
#include "../../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../AIBot/BotMain.h"
#include "../../AIKnowledge/Information.h"
#include "../../AIKnowledge/InformationManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x36F5C0 (C2; C1 0x36F640). m_bot is left unwritten.
DPAIObject3dText::DPAIObject3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                   const std::string& text, unsigned int information,
                                   DPDynamicTypes type, int size, AITextAlignmentX alignX,
                                   AITextAlignmentY alignY)
    : DPStatic3dText(lifeTime, colour, position, text, size, alignX, alignY)
    , m_information(information)
    , m_type(type)
{
}

// ELF 0x36F6C0 (C2; C1 0x36F740). m_bot is left unwritten.
DPAIObject3dText::DPAIObject3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                   const std::vector<std::string>& lines, unsigned int information,
                                   DPDynamicTypes type, int size, AITextAlignmentX alignX,
                                   AITextAlignmentY alignY)
    : DPStatic3dText(lifeTime, colour, position, lines, size, alignX, alignY)
    , m_information(information)
    , m_type(type)
{
}

// ELF 0x36F7C0 (D2; D1 0x36F800, D0 0x36F840).
DPAIObject3dText::~DPAIObject3dText()
{
}

// ELF 0x36F880. The position comes from the information's IAIObject, the type-3 transformation
// from the information itself (as DPDynamic3dText, translation added twice).
// shipped defect: the information is resolved through the unwritten m_bot.
bool DPAIObject3dText::draw(IDebugRenderer* renderer)
{
    Information* const information = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!information)
        return false;
    const Vec3& offset = information->getAIObject()->getPosition();
    Vec3 position;
    position.x = m_position.x + offset.x;
    position.y = m_position.y + offset.y;
    position.z = m_position.z + offset.z;
    switch (m_type)
    {
    case 0:
    case 1:
    case 2:
        break;
    case 3:
    {
        const Mat4& transformation = information->getTransformation();
        Vec3 transformed = {0.0f, 0.0f, 0.0f};
        transformed.x = position.x * transformation.m[0] + position.y * transformation.m[4]
            + position.z * transformation.m[8] + transformation.m[12];
        transformed.y = position.x * transformation.m[1] + position.y * transformation.m[5]
            + position.z * transformation.m[9] + transformation.m[13];
        transformed.z = position.x * transformation.m[2] + position.y * transformation.m[6]
            + position.z * transformation.m[10] + transformation.m[14];
        position = transformed;
        position.x += transformation.m[12];
        position.y += transformation.m[13];
        position.z += transformation.m[14];
        break;
    }
    default:
        Debug(DebugTypeError, "AI/AIDLL/AI/AIUtils/DebugRendering/DPAIObject3dText.cpp", 97, "")
            << "Invalid type.";
        break;
    }
    renderer->draw3dText(position, m_lines, m_size, m_colour, m_alignX, m_alignY);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
