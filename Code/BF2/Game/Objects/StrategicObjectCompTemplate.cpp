// Path: [INFERRED], beside the existing template and runtime component headers.
// ELF 8228EB0..8229A90, BF2.exe table906B88/ctor65E0E0 and all slot bodies.
// The dedicated PE retains StrategicObjectComp but omits this HUD template's
// strings/factory; the dedicated implementation below follows the symbolized ELF.
#include "StrategicObjectCompTemplate.h"
#include "StrategicObjectComp.h"
#include "../../Core/Ids.h"
#include "../../IO/Stream/IStream.h"
#include <sstream>
#include <cstdint>
#if !DEDICATED
#include "../../Menu/BF2HudManager.h"
#include "../../Menu/Ref2System.h"
namespace dice { namespace hfe { extern hud::BF2HudManager* hudManager; } }
#endif

namespace dice { namespace hfe { namespace world {

StrategicObjectCompTemplate::StrategicObjectCompTemplate()
    : m_refCount(1), m_mapIcon3D(-1), m_isOnePiece(false)
{
    m_strategicObjectName.assign("");
    m_part = 0;
    m_destroyedIcon.assign("");
    m_intactIcon.assign("");
    m_destroyedIconPtr = 0;
    m_intactIconPtr = 0;
}
StrategicObjectCompTemplate::~StrategicObjectCompTemplate() {}
void StrategicObjectCompTemplate::addRef() { ++m_refCount; }
int StrategicObjectCompTemplate::getRef() { return m_refCount; }
int StrategicObjectCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        delete this;
    return remaining;
}
void* StrategicObjectCompTemplate::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent || iid == IID_IStrategicObjectCompTemplate)
        return const_cast<StrategicObjectCompTemplate*>(this);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<StrategicObjectCompTemplate*>(this));
    return 0;
}
unsigned int StrategicObjectCompTemplate::getClassID() const { return CID_StrategicObjectCompTemplate; }
unsigned int StrategicObjectCompTemplate::getInterfaceID() { return IID_IStrategicObjectCompTemplate; }
void* StrategicObjectCompTemplate::queryImplementation(unsigned int cid)
{
    return cid == getClassID() ? this : 0;
}
IObjectComponent* StrategicObjectCompTemplate::createComponent() { return new StrategicObjectComp(this); }
void StrategicObjectCompTemplate::objectCreated(IObject*) {}   // ELF 8229870, empty
void StrategicObjectCompTemplate::objectDestroyed(IObject*) {} // ELF 8229880, empty
void StrategicObjectCompTemplate::setTemplate(IObjectTemplate*) {} // ELF 8229890, empty
void StrategicObjectCompTemplate::setStrategicObjectName(std::string value) { m_strategicObjectName = value; }
const std::string& StrategicObjectCompTemplate::getStrategicObjectName() { return m_strategicObjectName; }

namespace {
// [INFERRED] helper name. BF2.exe 65A2F0 takes the string by value, calls
// getSystem twice and resolves with loadTexture(name, true). ELF inlines null.
// The handle is std::intptr_t: int on x86, the full texture address on native x64.
std::intptr_t resolveStrategicIcon(std::string name)
{
#if !DEDICATED
    if (hudManager && hudManager->getSystem())
        return hudManager->getSystem()->loadTexture(name.c_str(), true);
#endif
    return 0;
}
}
void StrategicObjectCompTemplate::setDestroyedIcon(std::string value)
{
    m_destroyedIcon = value;
    m_destroyedIconPtr = resolveStrategicIcon(value);
}
void StrategicObjectCompTemplate::setIntactIcon(std::string value)
{
    m_intactIcon = value;
    m_intactIconPtr = resolveStrategicIcon(value);
}
const std::string& StrategicObjectCompTemplate::getDestroyedIcon() { return m_destroyedIcon; }
const std::string& StrategicObjectCompTemplate::getIntactIcon() { return m_intactIcon; }
std::intptr_t StrategicObjectCompTemplate::getDestroyedIconPtr() { return m_destroyedIconPtr; }
std::intptr_t StrategicObjectCompTemplate::getIntactIconPtr() { return m_intactIconPtr; }
void StrategicObjectCompTemplate::setPart(int value) { m_part = value; }
int StrategicObjectCompTemplate::getPart() { return m_part; }
void StrategicObjectCompTemplate::set3dMapIcon(int value) { m_mapIcon3D = value; }
int StrategicObjectCompTemplate::get3dMapIcon() const { return m_mapIcon3D; }
bool StrategicObjectCompTemplate::has3dMapIcon() const { return m_mapIcon3D != -1; }
void StrategicObjectCompTemplate::setIsOnePiece(bool value) { m_isOnePiece = value; }
bool StrategicObjectCompTemplate::getIsOnePiece() const { return m_isOnePiece; }

// ELF 82291B0 / client PE 65DF40: authored values only, omit defaults, one write.
bool StrategicObjectCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream out;
    if (!m_strategicObjectName.empty())
        out << "ObjectTemplate.StrategicObject.strategicObjectName " << m_strategicObjectName << std::endl;
    if (!m_destroyedIcon.empty())
        out << "ObjectTemplate.StrategicObject.destroyedIcon " << m_destroyedIcon << std::endl;
    if (!m_intactIcon.empty())
        out << "ObjectTemplate.StrategicObject.intactIcon " << m_intactIcon << std::endl;
    if (m_part)
        out << "ObjectTemplate.StrategicObject.part " << m_part << std::endl;
    if (m_mapIcon3D != -1)
        out << "ObjectTemplate.StrategicObject.3dMapIcon " << m_mapIcon3D << std::endl;
    if (m_isOnePiece)
        out << "ObjectTemplate.StrategicObject.isOnePiece " << m_isOnePiece << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}
} } }
