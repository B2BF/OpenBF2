// Global gravity, wind and air-density parameters. Source path [INFERRED].
// update is a recovered no-op; PhysicsNodeManager and ResponsePhysicsManager perform simulation.

#include "BasicPhysicsSystem.h"

#include "../Core/Ids.h"             // Interface ids
#include "../Core/ConsoleObjectMacro.h"   // the `physics` console object
// Use the concrete manager declaration for typed listProperties dispatch.
#include "PhysicsNodeManager.h"

namespace dice {
namespace hfe {
namespace world {

// PhysicsNodeManager singleton storage is defined with this concrete type in Core/Globals.cpp.
extern PhysicsNodeManager* physicsNodeManager;

// Starts with one reference and gravity -14.73 m/s^2; preserve the game-specific gravity value.
BasicPhysicsSystem::BasicPhysicsSystem()
    : m_refCount(1)                     // +0x04
    , m_gravity(-14.73f)                // +0x08
    , m_airDensityZeroAtHeight(1000.0f) // +0x30
{
    // Initializes all three vector members to zero.
    m_wind.x = 0.0f;  m_wind.y = 0.0f;  m_wind.z = 0.0f;                    // +0x0C
    m_reportedWind.x = 0.0f; m_reportedWind.y = 0.0f; m_reportedWind.z = 0.0f;
                                                                            // +0x18
    m_unusedVec3.x = 0.0f; m_unusedVec3.y = 0.0f; m_unusedVec3.z = 0.0f;    // +0x24 0x835C27B/7E/81
}

// Owns no heap resources; base destruction is sufficient.
BasicPhysicsSystem::~BasicPhysicsSystem()
{
}

// Reference counting deletes this when the count reaches zero and returns the new count.
void BasicPhysicsSystem::addRef()
{
    ++m_refCount;
}

int BasicPhysicsSystem::getRef()
{
    return m_refCount;
}

int BasicPhysicsSystem::release()
{
    int remaining = --m_refCount;
    if (remaining == 0)
        delete this;                                // 0x835C4F0, call [vtable+0x14] = D0
    return remaining;
}

// Returns the IBase or IPhysicsSystem interface for the corresponding global id, otherwise null.
void* BasicPhysicsSystem::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<BasicPhysicsSystem*>(this);
    if (iid == IID_IPhysicsSystem)
        return const_cast<BasicPhysicsSystem*>(this);
    return 0;
}

// update - 0x835C3B0, 5 bytes. It is empty in the shipped binary. See the file header.
void BasicPhysicsSystem::update(float /*deltaTime*/)
{
}

// Sets or reads gravity in metres per second squared.
void BasicPhysicsSystem::setGravity(float gravity)
{
    m_gravity = gravity;
}

float BasicPhysicsSystem::getGravity()
{
    return m_gravity;                               // 0x835C367, fld [eax+8]
}

// setWind and getWind use different members. update never copies between them;
// getWind therefore retains its initial zero vector. Preserve this shipped behavior.
void BasicPhysicsSystem::setWind(Vec3 wind)
{
    m_wind = wind;                                  // +0x0C
}

Vec3 BasicPhysicsSystem::getWind()
{
    return m_reportedWind;                          // 0x835C3CA -> +0x18. not m_wind. See above.
}

// Sets or reads the height at which air density reaches zero; initially 1000 metres.
void BasicPhysicsSystem::setAirDensityZeroAtHeight(float height)
{
    m_airDensityZeroAtHeight = height;
}

float BasicPhysicsSystem::getAirDensityZeroAtHeight()
{
    return m_airDensityZeroAtHeight;                // 0x835C3F7, fld [eax+30h]
}

// Returns PhysicsNodeManager::listProperties through the typed manager interface.
// Preserve the call even though that implementation currently returns an empty string.
std::string BasicPhysicsSystem::listProperties()
{
    return physicsNodeManager->listProperties();    // 0x835C41A, virtual, slot 11 / +0x2C
}

// The `physics` console object. gravity is a float property on the physics-system singleton
// (dedicated PE executor 0x692420 dispatches the virtual set/get pair on the system pointer).
namespace {
const char* const kPhysicsConsoleTag = "world.physics";
}
BF2_CONSOLE_OBJECT("physics", 2, 0)
BF2_CONSOLE_PROPERTY(gravity, float, static_cast<IPhysicsSystem*>(basicPhysicsSystem),
                     setGravity, getGravity, kPhysicsConsoleTag)

ConsoleObjectList& physicsConsoleObjects()
{
    return consoleBindingList(kPhysicsConsoleTag);
}

} // namespace world
} // namespace hfe
} // namespace dice

// setWind and getWind intentionally access different vectors; getWind retains the zero vector.
// The additional constructor-only vector remains [UNRESOLVED].
