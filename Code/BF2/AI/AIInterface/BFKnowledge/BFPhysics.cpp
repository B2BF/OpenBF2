// [INFERRED] Source placement.

#include "BFPhysics.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../../../Physics/BasicPhysicsSystem.h"   // IPhysicsSystem::getGravity (+0x20)

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// First wins in, exact match out.
// ELF 0x840E790 / 0x840E810.
IAIPhysics::IAIPhysics()
{
    if (!CommonSingletonServer::m_iaiPhysics)
        classManager->registerSimpleSingleton(std::string("IAIPhysics"), this);
}

// ELF 0x840E890 / 0x840E910 / 0x840E990.
IAIPhysics::~IAIPhysics()
{
    if (CommonSingletonServer::m_iaiPhysics == this)
        classManager->unregisterSimpleSingleton(std::string("IAIPhysics"));
}

// ELF 0x840B740 / 0x840B770.
BFPhysics::BFPhysics()
{
}

// ELF 0x840B7A0 / 0x840B7C0 / 0x840B7E0.
BFPhysics::~BFPhysics()
{
}

// bf2 0x840B810: a forwarder to world::basicPhysicsSystem's getGravity (IPhysicsSystem +0x20),
// no null guard.
// ELF 0x840B810.
float BFPhysics::getGravity() const
{
    return static_cast<world::IPhysicsSystem*>(world::basicPhysicsSystem)->getGravity();
}

} // namespace ai
} // namespace hfe
} // namespace dice
