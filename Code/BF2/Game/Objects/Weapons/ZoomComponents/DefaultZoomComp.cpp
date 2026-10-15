// Path:     [INFERRED] from the recovered ZoomComponents family placement.
// Contents: server-reachable DefaultZoomComp and complete DefaultZoomCompTemplate leaf recovered
// from ELF 0x829BED0..0x829D77A and cross-checked against PE 0x5A58xx..0x5A64xx.
// The client-only WeaponHud notification tail is the named frontier at changeZoom().

#include "DefaultZoomComp.h"

#include <cstddef>
#include <sstream>
#include <string>

#include "../GenericFireArm.h"
#include "../../Game/Soldier.h"
#include "../../IObject.h"
#include "../../IPlayer.h"
#include "../../ObjectLayoutAccess.h"
#include "../../GeometryPartAccess.h"
#include "../../../../Core/EventManager.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Scene/Object/PlayerManager.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

// geom::IGeometry is still unrecovered.  This is the complete bounded surface used by DefaultZoom.
// Its virtual destructor differs across the two retail ABIs, shifting every geometry slot after it.
// The slots follow the compiler's destructor ABI, so native MSVC x64 takes the PE arm too.
#if defined(_MSC_VER)
typedef int (__thiscall *GeometryGetIntFn)(void*);
typedef void (__thiscall *GeometrySetLodFn)(void*, int);
typedef void* (__thiscall *GeometryGetTemplateFn)(void*);
const std::size_t kGeometryGetTemplateSlot = 0x38 / 4;
const std::size_t kGeometryGetSubgeomSlot = 0x70 / 4;
const std::size_t kGeometrySetLodSlot = 0x78 / 4;
const std::size_t kGeometryGetLodSlot = 0x7C / 4;
const std::size_t kGeometryGetLodCountSlot = 0x80 / 4;
#else
typedef int (*GeometryGetIntFn)(void*);
typedef void (*GeometrySetLodFn)(void*, int);
typedef void* (*GeometryGetTemplateFn)(void*);
const std::size_t kGeometryGetTemplateSlot = 0x3C / 4;
const std::size_t kGeometryGetSubgeomSlot = 0x74 / 4;
const std::size_t kGeometrySetLodSlot = 0x7C / 4;
const std::size_t kGeometryGetLodSlot = 0x80 / 4;
const std::size_t kGeometryGetLodCountSlot = 0x84 / 4;
#endif

void** geometryVtable(void* geometry)
{
    return *reinterpret_cast<void***>(geometry);
}

int geometryGetSubgeom(void* geometry)
{
    return reinterpret_cast<GeometryGetIntFn>(
        geometryVtable(geometry)[kGeometryGetSubgeomSlot])(geometry);
}

void geometrySetLod(void* geometry, int lod)
{
    reinterpret_cast<GeometrySetLodFn>(
        geometryVtable(geometry)[kGeometrySetLodSlot])(geometry, lod);
}

int geometryGetLod(void* geometry)
{
    return reinterpret_cast<GeometryGetIntFn>(
        geometryVtable(geometry)[kGeometryGetLodSlot])(geometry);
}

int geometryGetLodCount(void* geometry)
{
    return reinterpret_cast<GeometryGetIntFn>(
        geometryVtable(geometry)[kGeometryGetLodCountSlot])(geometry);
}

void* geometryGetTemplate(void* geometry)
{
    return reinterpret_cast<GeometryGetTemplateFn>(
        geometryVtable(geometry)[kGeometryGetTemplateSlot])(geometry);
}

} // namespace

DefaultZoomComp::DefaultZoomComp(DefaultZoomCompTemplate* objectTemplate)
    : ZoomComp(objectTemplate)
    , m_refCount(1)
    , m_currentZoom(0)
    , m_changeTick(0)
    , m_pendingZoomLod(false)
    , m_signalToggle(false)
    , m_pendingFov(false)
    , m_dword28(0)
    , m_waitingForBusy(false)
    , m_defaultTemplate(objectTemplate)
{
}

DefaultZoomComp::~DefaultZoomComp()
{
}

void DefaultZoomComp::addRef()
{
    ++m_refCount;
}

int DefaultZoomComp::getRef()
{
    return m_refCount;
}

int DefaultZoomComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = m_refCount == 1;
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* DefaultZoomComp::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return const_cast<DefaultZoomComp*>(this);
    if (iid == IID_IWeaponComp || iid == IID_IWeaponZoomComp)
    {
        return static_cast<IWeaponZoomComp*>(
            const_cast<DefaultZoomComp*>(this));
    }
    return 0;
}

unsigned int DefaultZoomComp::getInterfaceID() const
{
    return IID_IWeaponZoomComp;
}

void DefaultZoomComp::updateComponent(float, unsigned int)
{
    IPlayer* const player = firearm()->getPlayer();

    if (m_pendingZoomLod)
    {
        const unsigned int completesAt =
            m_changeTick + static_cast<unsigned int>(m_defaultTemplate->getZoomDelayInTicks());
        if (completesAt <= static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            setZoomLod(m_defaultTemplate->getZoomLod());
            m_pendingZoomLod = false;
        }
    }

    if (m_pendingFov)
    {
        const unsigned int completesAt =
            m_changeTick + static_cast<unsigned int>(m_defaultTemplate->getChangeFovDelayInTicks());
        if (completesAt <= static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            if (player)
                player->setFov(m_defaultTemplate->getZoomFactors()[m_currentZoom], 0.0f);
            m_pendingFov = false;
        }
    }

    if (player && !player->getOverheadCameraEnabled())
    {
        if (!m_pendingFov
            && player->getEndFov() != m_defaultTemplate->getZoomFactors()[m_currentZoom])
        {
            player->setFov(m_defaultTemplate->getZoomFactors()[m_currentZoom], 0.0f);
        }

        const int lod = getZoomLod();
        if (!m_pendingZoomLod && m_currentZoom != 0 && lod != -1
            && lod != m_defaultTemplate->getZoomLod())
        {
            setZoomLod(m_defaultTemplate->getZoomLod());
        }
    }

    if (m_waitingForBusy)
    {
        IWeaponComp* const self =
            static_cast<IWeaponComp*>(static_cast<IWeaponZoomComp*>(this));
        if (!firearm()->busyExcept(self, false))
        {
            m_signalToggle = true;
            m_waitingForBusy = false;
        }
    }
}

bool DefaultZoomComp::requestFire(bool, bool)
{
    return true;
}

void DefaultZoomComp::postFire(WeaponFireEvent&)
{
}

void DefaultZoomComp::fireComplete(unsigned int)
{
    if (m_defaultTemplate->getZoomOutAfterFire() && m_currentZoom != 0)
        m_waitingForBusy = true;
}

void DefaultZoomComp::reload(ReloadState)
{
    if (m_currentZoom != 0)
        changeZoom(0, false);
}

bool DefaultZoomComp::busy()
{
    if (m_pendingZoomLod || m_waitingForBusy)
        return true;
    return m_changeTick + static_cast<unsigned int>(m_defaultTemplate->getZoomDelayInTicks())
        > static_cast<unsigned int>(WorldPref::getTickCount());
}

void DefaultZoomComp::reset()
{
    m_pendingZoomLod = false;
    IPlayer* const player = firearm()->getPlayer();
    if (!player || m_currentZoom == 0)
        return;

    m_currentZoom = 0;
    player->setFov(m_defaultTemplate->getZoomFactors()[0], m_defaultTemplate->getZoomDelay());
    if (m_defaultTemplate->getZoomLod() > 0)
        setZoomLod(0);
    if (is1P())
        eventManager->postEvent(ECHud, 31, this, 0.0f);
}

bool DefaultZoomComp::getSignalToggle()
{
    const bool result = m_signalToggle;
    m_signalToggle = false;
    return result;
}

void DefaultZoomComp::toggle(TemplateMessage message)
{
    if (message != static_cast<TemplateMessage>(7))
        return;

    const std::vector<float>& factors = m_defaultTemplate->getZoomFactors();
    if (factors.size() <= 1)
        return;

    // Retail deliberately continues scanning after changing the index.
    for (unsigned int i = 0; i < factors.size(); ++i)
    {
        if (static_cast<int>(i) == m_currentZoom)
        {
            ++i;
            if (i < factors.size())
                changeZoom(static_cast<int>(i), false);
            else
                changeZoom(0, false);
        }
    }
}

void DefaultZoomComp::changeTarget(int)
{
}

void DefaultZoomComp::setCameraNr(int camera)
{
    changeZoom(camera, true);
}

int DefaultZoomComp::getCameraNr() const
{
    return m_currentZoom;
}

void DefaultZoomComp::changeViewMode()
{
    if (m_currentZoom != 0 && !is1P())
        changeZoom(0, false);
}

bool DefaultZoomComp::allowEffects() const
{
    if (m_defaultTemplate->getDisableMuzzleWhenZoomed() != true)
        return true;
    if (m_currentZoom == 0)
        return true;
    return is1P() != true;
}

void DefaultZoomComp::setZoomLod(int lod)
{
    if (lod != 0 && !is1P())
        return;

    void* const geometry = object_detail::LayoutAccess::geometry(
        static_cast<IObject*>(firearm()));
    if (!geometry || geometryGetSubgeom(geometry) != 0)
        return;
    if (lod >= geometryGetLodCount(geometry))
        return;

    if (lod == 0
        && geometryGetTemplate(geometry)
        && geometryGetLodCount(geometry) > 2
        && firearm()->getWeaponState() <= 0
        && geometry_part::templateNameEquals(geometryGetTemplate(geometry), "insgr_rpg"))
    {
        geometrySetLod(geometry, 2);
    }
    else
    {
        geometrySetLod(geometry, lod);
    }
}

float DefaultZoomComp::getMouseSpeedMod()
{
    return m_currentZoom > 0 ? m_defaultTemplate->getMouseSpeedMod() : 1.0f;
}

float DefaultZoomComp::getZoomFactor(unsigned int index) const
{
    const std::vector<float>& factors = m_defaultTemplate->getZoomFactors();
    return index < factors.size() ? factors[index] : -1.0f;
}

void DefaultZoomComp::changeZoom(int camera, bool immediate)
{
    IPlayer* const player = firearm()->getPlayer();
    if (!player || camera == m_currentZoom)
        return;

    m_changeTick = static_cast<unsigned int>(WorldPref::getTickCount());
    const std::vector<float>& factors = m_defaultTemplate->getZoomFactors();
    if (immediate)
    {
        player->setFov(factors[camera], 0.0f);
    }
    else if (m_defaultTemplate->getChangeFovDelay() == -1.0f)
    {
        player->setFov(factors[camera], m_defaultTemplate->getZoomDelay());
    }
    else if (camera == 0)
    {
        player->setFov(factors[0], 0.0f);
    }
    else
    {
        m_pendingFov = true;
    }

    m_currentZoom = camera;
    if (m_defaultTemplate->getZoomLod() > 0)
    {
        if (camera != 0)
        {
            if (m_defaultTemplate->getZoomDelayInTicks() == 0 || immediate)
                setZoomLod(m_defaultTemplate->getZoomLod());
            else
                m_pendingZoomLod = true;
        }
        else
        {
            setZoomLod(0);
        }
    }

    // [FRONTIER] [RETIRE-WHEN: unrecovered-factories-registered]
    // Retail's remaining arm reads the firearm's Object +0x5C WeaponHud component, calls its
    // specialized +0x50 getter, tests the returned object's +0x18 field, and only then posts the
    // zoom HUD event for a local first-person player. WeaponHud remains factory-dark under RW-6
    // and a dedicated server's is1P() is false, so the arm is unreachable in this slice. It must
    // land with the typed WeaponHud interface rather than being mis-dispatched through an ELF slot.
}

int DefaultZoomComp::getZoomLod() const
{
    void* const geometry = object_detail::LayoutAccess::geometry(
        static_cast<IObject*>(firearm()));
    if (geometry && geometryGetSubgeom(geometry) == 0)
        return geometryGetLod(geometry);
    return -1;
}

bool DefaultZoomComp::is1P() const
{
    IPlayer* const player = firearm()->getPlayer();
    if (!player)
        return false;
    if (player != playerManager->getLocalHumanPlayer())
        return false;
    return player->hasFirstPersonView();
}

DefaultZoomCompTemplate::DefaultZoomCompTemplate()
    : ZoomCompTemplate()
    , m_refCount(1)
    , m_zoomFactors(1, 0.0f)
    , m_zoomLod(0)
    , m_zoomOutAfterFire(false)
    , m_mouseSpeedMod(1.0f)
    , m_disableMuzzleWhenZoomed(false)
{
    setZoomDelay(0.5f);
    setChangeFovDelay(-1.0f);
}

DefaultZoomCompTemplate::~DefaultZoomCompTemplate()
{
}

void DefaultZoomCompTemplate::addRef()
{
    ++m_refCount;
}

int DefaultZoomCompTemplate::getRef()
{
    return m_refCount;
}

int DefaultZoomCompTemplate::release()
{
    const int remaining = m_refCount - 1;
    const bool last = m_refCount == 1;
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* DefaultZoomCompTemplate::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent || iid == IID_IWeaponCompTemplate)
        return const_cast<DefaultZoomCompTemplate*>(this);
    if (iid == io::IID_IConsoleSaveable)
    {
        return static_cast<io::IConsoleSaveable*>(
            const_cast<DefaultZoomCompTemplate*>(this));
    }
    return 0;
}

unsigned int DefaultZoomCompTemplate::getClassID() const
{
    return CID_DefaultZoomCompTemplate;
}

IObjectComponent* DefaultZoomCompTemplate::createComponent()
{
    return new DefaultZoomComp(this);
}

void* DefaultZoomCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_DefaultZoomCompTemplate || classId == CID_ZoomCompTemplate)
        return this;
    return 0;
}

void DefaultZoomCompTemplate::addZoomFactor(float value)
{
    for (unsigned int i = 0; i < m_zoomFactors.size(); ++i)
    {
        if (m_zoomFactors[i] == value)
            return;
    }
    m_zoomFactors.push_back(value);
}

void DefaultZoomCompTemplate::setZoomDelay(float value)
{
    m_zoomDelay = value;
    m_zoomDelayInTicks = static_cast<int>(value * 30.0f);
}

void DefaultZoomCompTemplate::setChangeFovDelay(float value)
{
    m_changeFovDelay = value;
    m_changeFovDelayInTicks = static_cast<int>(value * 30.0f);
}

bool DefaultZoomCompTemplate::makeScript(io::IStream* stream)
{
    if (!ZoomCompTemplate::makeScript(stream))
        return false;

    std::ostringstream script;
    if (m_zoomDelay != 0.5f)
        script << "ObjectTemplate.zoom.zoomDelay " << m_zoomDelay << std::endl;
    if (m_zoomLod != 0)
        script << "ObjectTemplate.zoom.zoomLod " << m_zoomLod << std::endl;
    for (unsigned int i = 0; i < m_zoomFactors.size(); ++i)
        script << "ObjectTemplate.zoom.addZoomFactor " << m_zoomFactors[i] << std::endl;
    if (m_changeFovDelay != -1.0f)
        script << "ObjectTemplate.zoom.changeFovDelay " << m_changeFovDelay << std::endl;
    if (m_zoomOutAfterFire)
        script << "ObjectTemplate.zoom.zoomOutAfterFire " << m_zoomOutAfterFire << std::endl;
    if (m_mouseSpeedMod != 1.0f)
        script << "ObjectTemplate.zoom.mouseSpeedMod " << m_mouseSpeedMod << std::endl;
    if (m_disableMuzzleWhenZoomed)
        script << "ObjectTemplate.zoom.disableMuzzleWhenZoomed " << m_disableMuzzleWhenZoomed;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
