// Cover-information accessors.

#include "IPICover.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2EE3F0, __tcf_0 0x2EE450, _GLOBAL__I 0x2EE480).
#include <iostream>

#include "Information.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../AIInterface/BFKnowledge/PlanePolygon.h"

namespace dice {
namespace hfe {
namespace ai {

IPICover::IPICover(Information* information)
{
    m_information = information;
    IAIObject* const object = information->getAIObject();
    if (object->isCover())
    {
        m_outline = object->getCover()->getObjectOutline();
    }
    else
    {
        BoundingBox* const box = object->getPhysical()->getCurrentBoundingBox();
        m_outline = new PlanePolygon(box, information->getTransformation());
        // The paired instructions show scalar deallocation and no visible destructor call.
        ::operator delete(box);
    }
}

IPICover::~IPICover()
{
    delete m_outline;
}

int IPICover::getType()
{
    return 1;
}

void IPICover::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

void IPICover::setTime(double /*time*/)
{
    delete m_outline;
    m_outline = m_information->getAIObject()->getCover()->getObjectOutline();
}

IPlanePolygon* IPICover::getLocalOutline()
{
    return m_information->getAIObject()->getCover()->getLocalObjectOutline();
}

IPlanePolygon* IPICover::getOutline()
{
    return m_outline;
}

float IPICover::getVolume()
{
    return m_information->getAIObject()->getPhysical()->getVolume();
}

float IPICover::getCoverValue()
{
    return m_information->getAIObject()->getCover()->getCoverValue();
}

float IPICover::getHeight()
{
    return m_information->getAIObject()->getCover()->getHeight();
}

unsigned int IPICover::getPortalCount() const
{
    return m_information->getAIObject()->getCover()->getPortalCount();
}

unsigned int IPICover::getPortalVertexCount(unsigned int portal) const
{
    return m_information->getAIObject()->getCover()->getPortalVertexCount(portal);
}

const Vec3& IPICover::getPortalVertex(unsigned int portal, unsigned int vertex) const
{
    return m_information->getAIObject()->getCover()->getPortalVertex(portal, vertex);
}

void IPICover::getPortal(unsigned int portal, std::vector<Vec3>& out) const
{
    m_information->getAIObject()->getCover()->getPortal(portal, out);
}

const Vec3& IPICover::getPortalCenter(unsigned int portal) const
{
    return m_information->getAIObject()->getCover()->getPortalCenter(portal);
}

Vec3 IPICover::getPortalExtentDirection(unsigned int portal) const
{
    const Vec3& normal = getPortalNormal(portal);
    Vec3 direction;
    direction.x = normal.z;
    direction.y = 0.0f;
    direction.z = -normal.x;
    return direction;
}

const Vec3& IPICover::getPortalNormal(unsigned int portal) const
{
    return m_information->getAIObject()->getCover()->getPortalNormal(portal);
}

float IPICover::getPortalXZExtentR(unsigned int portal) const
{
    return m_information->getAIObject()->getCover()->getPortalXZExtentR(portal);
}

float IPICover::getPortalXZExtentL(unsigned int portal) const
{
    return m_information->getAIObject()->getCover()->getPortalXZExtentL(portal);
}

float IPICover::getTransparency()
{
    return m_information->getAIObject()->getCover()->getTransparency();
}

int IPICover::getPathfindingPlaneCount() const
{
    return m_information->getAIObject()->getCover()->getPathfindingPlaneCount();
}

const Vec3& IPICover::getPathfindingPlaneCorner(int plane, int corner) const
{
    return m_information->getAIObject()->getCover()->getPathfindingPlaneCorner(plane, corner);
}

} // namespace ai
} // namespace hfe
} // namespace dice
