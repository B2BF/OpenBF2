// Path:     attested - the unknown-pose Debug site names
// Game/Objects/Weapons/DeviationComponents/SoldierDeviationComp.cpp:186.
// Contents: complete SoldierDeviationComp runtime/template family recovered from the symbolised
// Linux ELF (0x825DE70..0x825F1BA) and PE (0x5A9340..0x5A9C22).

#include "SoldierDeviationComp.h"

#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>

#include "../GenericFireArm.h"
#include "../../Game/Soldier.h"
#include "../../IObject.h"
#include "../../IPlayer.h"
#include "../../ObjectLayoutAccess.h"
#include "../../../../Core/Debug.h"
#include "../../../../Core/Ids.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Physics/IResponsePhysics.h"
#include "../../../../Physics/SoldierResponse.h"     // m_byteFA (+0xFA), a friend
#include "../../../../Scene/Object/ObjectUtil.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

float mappedInput(const io::PlayerInput& input, io::PlayerInputMap index)
{
    const unsigned int bit = static_cast<unsigned int>(index);
    return (input.mappingFlags & (1ULL << bit)) ? input.inputs[bit] : 0.0f;
}

void decayToZero(float& value, float decrement)
{
    value -= decrement;
    if (value < 0.0f)
        value = 0.0f;
}

// Turn and misc deviation subtract their decay in the rising arm.  Once at/above the ceiling,
// retail decays and then snaps back UP to the ceiling if that decay crossed it.
void updateDecayingAccumulator(float& value, float addition, float decrement, float maximum)
{
    if (addition == 0.0f)
    {
        decayToZero(value, decrement);
    }
    else if (value >= maximum)
    {
        value -= decrement;
        if (value < maximum)
            value = maximum;
    }
    else
    {
        value += addition - decrement;
        if (value > maximum)
            value = maximum;
        if (value < 0.0f)
            value = 0.0f;
    }
}

} // anonymous namespace

SoldierDeviationComp::SoldierDeviationComp(SoldierDeviationCompTemplate* objectTemplate)
    : DeviationComp(objectTemplate)
    , m_soldierRefCount(1)
    , m_turnDeviation(0.0f)
    , m_speedDeviation(0.0f)
    , m_miscDeviation(0.0f)
    , m_poseModifier(1.0f)
    , m_zoomModifier(1.0f)
    , m_zoomed(false)
    , m_soldierRandom()
    , m_aiDeviation(0.0f)
    , m_soldierTemplate(objectTemplate)
{
    // +0x45..+0x47 are untouched padding in both shipped constructors.
}

SoldierDeviationComp::~SoldierDeviationComp()
{
}

void SoldierDeviationComp::addRef() { ++m_soldierRefCount; }
int SoldierDeviationComp::getRef() { return m_soldierRefCount; }

int SoldierDeviationComp::release()
{
    const int remaining = --m_soldierRefCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* SoldierDeviationComp::queryInterface(unsigned int iid) const
{
    SoldierDeviationComp* const self = const_cast<SoldierDeviationComp*>(this);
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IWeaponComp || iid == IID_IWeaponDeviationComp)
        return static_cast<IWeaponDeviationComp*>(self);
    return 0;
}

unsigned int SoldierDeviationComp::getInterfaceID() const
{
    return IID_IWeaponDeviationComp;
}

void SoldierDeviationComp::updateComponent(float, unsigned int)
{
}

// , 1,043 B.  Discrete movement-axis additions, the
// asymmetric speed ceiling, raw ground-contact override and update ordering all match both ports.
void SoldierDeviationComp::updateDeviation(io::PlayerInput input,
                                            bool movementState,
                                            bool firing)
{
    IObject* const soldierObject = findClosestParentHavingNetworkable(m_initObject);
    if (soldierObject && soldierObject->getTemplate()
        && soldierObject->getTemplate()->getClassID() == CID_SoldierTemplate)
    {
        const float yaw = mappedInput(input, io::PIYaw);
        const float throttle = mappedInput(input, io::PIThrottle);
        const float mouseX = mappedInput(input, io::PIMouseLookX);
        const float mouseY = mappedInput(input, io::PIMouseLookY);

        float speedAddition = 0.0f;
        if (std::fabs(throttle) > 0.0099999998f)
            speedAddition += m_soldierTemplate->getSpeedDevAddForward();
        if (std::fabs(yaw) > 0.0099999998f)
            speedAddition += m_soldierTemplate->getSpeedDevAddStrafe();

        // Unlike turn/misc, speed does NOT subtract speedDevSub while rising below the ceiling.
        if (speedAddition == 0.0f)
        {
            decayToZero(m_speedDeviation, m_soldierTemplate->getSpeedDevSub());
        }
        else if (m_speedDeviation >= m_soldierTemplate->getSpeedDevMax())
        {
            m_speedDeviation -= m_soldierTemplate->getSpeedDevSub();
            if (m_speedDeviation < m_soldierTemplate->getSpeedDevMax())
                m_speedDeviation = m_soldierTemplate->getSpeedDevMax();
        }
        else
        {
            m_speedDeviation += speedAddition;
            if (m_speedDeviation > m_soldierTemplate->getSpeedDevMax())
                m_speedDeviation = m_soldierTemplate->getSpeedDevMax();
        }

        float turnAddition = 0.0f;
        if (std::fabs(mouseY) > 0.0099999998f)
            turnAddition += std::fabs(mouseY) * m_soldierTemplate->getTurnDevAddPitch();
        if (std::fabs(mouseX) > 0.0099999998f)
            turnAddition += std::fabs(mouseX) * m_soldierTemplate->getTurnDevAddYaw();
        updateDecayingAccumulator(m_turnDeviation, turnAddition,
                                  m_soldierTemplate->getTurnDevSub(),
                                  m_soldierTemplate->getTurnDevMax());

        const float miscAddition = movementState
            ? m_soldierTemplate->getMiscDevAddJump()
            : 0.0f;
        updateDecayingAccumulator(m_miscDeviation, miscAddition,
                                  m_soldierTemplate->getMiscDevSub(),
                                  m_soldierTemplate->getMiscDevMax());

        SoldierPose pose = static_cast<Soldier*>(soldierObject)->getPose();
        if (!soldierObject->getParent())
        {
            // Retail has no null guard here.  A Soldier is expected to carry response physics;
            // preserve the +0xFA ground-contact read instead of inventing a fallback.
#if defined(_M_IX86)
            static_assert(offsetof(SoldierResponsePhysics, m_byteFA) == 0xFA,
                          "retail soldier ground-contact byte");
#endif
            const SoldierResponsePhysics* const response =
                static_cast<const SoldierResponsePhysics*>(static_cast<IResponsePhysics*>(
                    object_detail::LayoutAccess::responsePhysics(soldierObject)));
            if (!response->m_byteFA)
                pose = SoldierPose_Stand;
        }

        switch (pose)
        {
        case SoldierPose_Stand:
        case SoldierPose_Swim:
            m_poseModifier = m_soldierTemplate->getDevModStand();
            break;
        case SoldierPose_Crouch:
            m_poseModifier = m_soldierTemplate->getDevModCrouch();
            break;
        case SoldierPose_Prone:
            m_poseModifier = m_soldierTemplate->getDevModLie();
            break;
        default:
            Debug(DEBUGTYPE_6,
                  "Game/Objects/Weapons/DeviationComponents/SoldierDeviationComp.cpp", 186, "")
                << "Unknown pose type";
            break;
        }

        m_zoomModifier = m_zoomed ? m_soldierTemplate->getDevModZoom() : 1.0f;
    }

    if (!firing)
        decayToZero(m_fireDeviation, m_soldierTemplate->getFireDevSub());

    const float fireAndMinimum = m_fireDeviation + m_soldierTemplate->getMinDev();
    m_deviation = (fireAndMinimum + m_speedDeviation + m_turnDeviation + m_miscDeviation)
                * m_poseModifier * m_zoomModifier;

    if (m_initObject)
    {
        IPlayer* const player = firearm()->getPlayer();
        if (player && player->getIsAIPlayer())
            m_deviation += m_aiDeviation;
    }
}

void SoldierDeviationComp::setZoom(int zoom)
{
    m_zoomed = zoom > 0;
}

void SoldierDeviationComp::disable()
{
    m_zoomed = false;
}

void SoldierDeviationComp::reset()
{
    m_zoomed = false;
}

float SoldierDeviationComp::getMinDeviation() const
{
    return m_soldierTemplate->getMinDev() * m_soldierTemplate->getDevModLie();
}

float SoldierDeviationComp::getMaxDeviation() const
{
    return (m_soldierTemplate->getFireDevMax()
          + m_soldierTemplate->getMinDev()
          + m_soldierTemplate->getTurnDevMax()
          + m_soldierTemplate->getSpeedDevMax()
          + m_soldierTemplate->getMiscDevMax())
          * m_soldierTemplate->getDevModStand();
}

void SoldierDeviationComp::setAIDeviation(float deviation)
{
    m_aiDeviation = deviation;
}

SoldierDeviationCompTemplate::SoldierDeviationCompTemplate()
    : DeviationCompTemplate()
    , m_soldierRefCount(1)
    , m_turnDevAddPitch(0.0f)
    , m_turnDevAddYaw(0.0f)
    , m_turnDevSub(0.0f)
    , m_turnDevMax(0.0f)
    , m_speedDevAddForward(0.0f)
    , m_speedDevAddStrafe(0.0f)
    , m_speedDevSub(0.0f)
    , m_speedDevMax(0.0f)
    , m_miscDevAddJump(0.0f)
    , m_miscDevSub(0.0f)
    , m_miscDevMax(0.0f)
    , m_devModStand(1.0f)
    , m_devModCrouch(1.0f)
    , m_devModLie(1.0f)
    , m_devModZoom(1.0f)
{
    // The final dword at +0x5C is deliberately not initialized; its role remains unresolved.
}

void SoldierDeviationCompTemplate::addRef() { ++m_soldierRefCount; }
int SoldierDeviationCompTemplate::getRef() { return m_soldierRefCount; }

int SoldierDeviationCompTemplate::release()
{
    const int remaining = --m_soldierRefCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* SoldierDeviationCompTemplate::queryInterface(unsigned int iid) const
{
    SoldierDeviationCompTemplate* const self =
        const_cast<SoldierDeviationCompTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent
        || iid == IID_IWeaponCompTemplate)
        return static_cast<IWeaponCompTemplate*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int SoldierDeviationCompTemplate::getClassID() const
{
    return CID_SoldierDeviationCompTemplate;
}

unsigned int SoldierDeviationCompTemplate::getInterfaceID()
{
    return IID_IWeaponDeviationCompTemplate;
}

IObjectComponent* SoldierDeviationCompTemplate::createComponent()
{
    return new SoldierDeviationComp(this);
}

void SoldierDeviationCompTemplate::objectCreated(IObject*) {}
void SoldierDeviationCompTemplate::objectDestroyed(IObject*) {}
void SoldierDeviationCompTemplate::setTemplate(IObjectTemplate*) {}

void* SoldierDeviationCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_SoldierDeviationCompTemplate || classId == CID_DeviationCompTemplate)
        return this;
    return 0;
}

SoldierDeviationCompTemplate::~SoldierDeviationCompTemplate()
{
}

bool SoldierDeviationCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    if (!DeviationCompTemplate::makeScript(stream))
        return false;

    // These three grouped lines are unconditional in retail, including an all-default template.
    script << "ObjectTemplate.deviation.setTurnDev "
           << m_turnDevMax << ' ' << m_turnDevAddPitch << ' '
           << m_turnDevAddYaw << ' ' << m_turnDevSub << std::endl;
    script << "ObjectTemplate.deviation.setSpeedDev "
           << m_speedDevMax << ' ' << m_speedDevAddForward << ' '
           << m_speedDevAddStrafe << ' ' << m_speedDevSub << std::endl;
    script << "ObjectTemplate.deviation.setMiscDev "
           << m_miscDevMax << ' ' << m_miscDevAddJump << ' '
           << m_miscDevSub << std::endl;

    if (m_devModStand != 1.0f)
        script << "ObjectTemplate.deviation.devModStand " << m_devModStand << std::endl;
    if (m_devModCrouch != 1.0f)
        script << "ObjectTemplate.deviation.devModCrouch " << m_devModCrouch << std::endl;
    if (m_devModLie != 1.0f)
        script << "ObjectTemplate.deviation.devModLie " << m_devModLie << std::endl;
    if (m_devModZoom != 1.0f)
        script << "ObjectTemplate.deviation.devModZoom " << m_devModZoom << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
