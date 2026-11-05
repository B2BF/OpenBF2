// World-space debug text that follows a bot's equipment or an information. Evidence: libaidll.so
// (ia-32) 0x371500..0x371D3F. Placement attested by the __FILE__ string
// "AI/AIDLL/AI/AIUtils/DebugRendering/DPDynamic3dText.cpp".

#include "DPDynamicPrimitives.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x371C90, __tcf_0 0x371CF0, _GLOBAL__I 0x371D20).
#include <iostream>

#include "../../../../../Core/Debug.h"
#include "../../../../AIInterface/BFKnowledge/DebugRenderer.h"
#include "../../AIBot/BotMain.h"
#include "../../AIKnowledge/Information.h"
#include "../../Main/InformationManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x371500 (C2; C1 0x371580).
DPDynamic3dText::DPDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                 const std::string& text, Bot* bot, DPDynamicTypes type, int size,
                                 AITextAlignmentX alignX, AITextAlignmentY alignY)
    : DPStatic3dText(lifeTime, colour, position, text, size, alignX, alignY)
    , m_bot(bot)
    , m_information(0xFFFFFFFFu)
    , m_type(type)
{
}

// ELF 0x371600 (C2; C1 0x371680).
DPDynamic3dText::DPDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                 const std::vector<std::string>& lines, Bot* bot, DPDynamicTypes type,
                                 int size, AITextAlignmentX alignX, AITextAlignmentY alignY)
    : DPStatic3dText(lifeTime, colour, position, lines, size, alignX, alignY)
    , m_bot(bot)
    , m_information(0xFFFFFFFFu)
    , m_type(type)
{
}

// ELF 0x371700 (C2; C1 0x371780).
DPDynamic3dText::DPDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                 const std::string& text, unsigned int information,
                                 DPDynamicTypes type, int size, AITextAlignmentX alignX,
                                 AITextAlignmentY alignY)
    : DPStatic3dText(lifeTime, colour, position, text, size, alignX, alignY)
    , m_bot(0)
    , m_information(information)
    , m_type(type)
{
}

// ELF 0x371800 (C2; C1 0x371880).
DPDynamic3dText::DPDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                 const std::vector<std::string>& lines, unsigned int information,
                                 DPDynamicTypes type, int size, AITextAlignmentX alignX,
                                 AITextAlignmentY alignY)
    : DPStatic3dText(lifeTime, colour, position, lines, size, alignX, alignY)
    , m_bot(0)
    , m_information(information)
    , m_type(type)
{
}

// ELF 0x371900 (D2; D1 0x371940, D0 0x371980).
DPDynamic3dText::~DPDynamic3dText()
{
}

// ELF 0x3719C0. The text sits at m_position offset by the information's position; types 0-2 use
// that directly, type 3 transforms it and then adds the translation a second time (shipped
// defect, kept). A negative or larger type reports and still draws.
bool DPDynamic3dText::draw(IDebugRenderer* renderer)
{
    Information* const information
        = informationManager->getInformation(m_bot ? m_bot->getCurrentEquipment() : m_information);
    // shipped defect: no null test before the virtual call.
    const Vec3& offset = information->getPosition();
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
        Debug(DebugTypeError, "AI/AIDLL/AI/AIUtils/DebugRendering/DPDynamic3dText.cpp", 141, "")
            << "Invalid type.";
        break;
    }
    renderer->draw3dText(position, m_lines, m_size, m_colour, m_alignX, m_alignY);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
