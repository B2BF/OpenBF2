// Owns a roaming plan and retains its object handle.

#include "BAPIWRoam.h"


namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F7AC0 (C1; C2 0x1F7A60)
BAPIWRoam::BAPIWRoam(unsigned int object, BAPStatement* statement, int tag)
    : BAPInfoWrapper(statement, tag), m_object(object)
{
    m_statement->setParent(this);
}

// ELF 0x1F7B70 (D1; D2 0x1F7B20, D0 0x1F7BC0)
BAPIWRoam::~BAPIWRoam()
{
    delete m_statement;
}

// ELF 0x1F7C20
int BAPIWRoam::getInfoType() const
{
    return 5;
}

} // namespace ai
} // namespace hfe
} // namespace dice
