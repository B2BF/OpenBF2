// Owns a take-cover plan and stores either a fixed point or an object's initial position.

// static init: ELF 0x1F7FD0 (__tcf_0 0x1F8030, _GLOBAL__I 0x1F8060): <iostream>'s Init object only.
#include <iostream>

#include "BAPIWTakeCover.h"

#include "BAPActionInline.h"
#include "../AIKnowledge/Information.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F7CB0 (C1; C2 0x1F7C30)
BAPIWTakeCover::BAPIWTakeCover(const Vec3& position, BAPStatement* statement, int tag)
    : BAPInfoWrapper(statement, tag), m_position(position), m_useObject(false)
{
    m_statement->setParent(this);
}

// ELF 0x1F7E00 (C1; C2 0x1F7D30)
BAPIWTakeCover::BAPIWTakeCover(unsigned int object, BAPStatement* statement, int tag)
    : BAPInfoWrapper(statement, tag), m_position(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_object(object), m_useObject(true)
{
    Information* information = bapinline::info(object);
    if (information)
        m_position = information->getPosition();
    m_statement->setParent(this);
}

// ELF 0x1F7F20 (D1; D2 0x1F7ED0, D0 0x1F7F70)
BAPIWTakeCover::~BAPIWTakeCover()
{
    delete m_statement;
}

// ELF 0x1F8080
int BAPIWTakeCover::getInfoType() const
{
    return 7;
}

} // namespace ai
} // namespace hfe
} // namespace dice
