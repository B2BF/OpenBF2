// Path: [INFERRED], beside WeaponHudCompTemplate.h.
// ELF: template 0x822EBE0..0x823106C, runtime 0x822E550..0x822EBDC.
// PE: template ctor 0x65C8A0/vtable 0x9063A0, runtime ctor 0x68A820/vtable 0x90BA20.
#include "WeaponHudCompTemplate.h"

#include "ICameraObject.h"
#include "IObject.h"
#include "IWeaponHudComp.h"
#include "IWeaponObject.h"
#include "ObjectLayoutAccess.h"
#include "Projectiles/IProjectileObject.h"
#include "../../Core/Ids.h"
#include "../../Sound/BfSound.h"     // g_bfSound - the lock and overheat sounds
#include "../../IO/Stream/IStream.h"
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

#include <cstddef>
#include <sstream>

namespace dice {
namespace hfe {
namespace world {

namespace {

// NOT the same as world::findOwningPlayerControlObject (IObject.h, PE 0x440060): that one tests
// the object itself first and guards every step. This one deliberately starts at the parent. Both
// are shipped; keep them apart.
IPlayerControlObject* findParentPlayerControlObject(IObject* object)
{
    object = object->getParent(); // Retail intentionally does not guard the component's object.
    while (object)
    {
        IPlayerControlObject* pco = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));
        if (pco)
            return pco;
        object = object->getParent();
    }
    return 0;
}

} // namespace

class WeaponHudComp : public IWeaponHudComp
{
public:
    explicit WeaponHudComp(WeaponHudCompTemplate* componentTemplate)
        : m_refCount(1), m_template(componentTemplate),
          m_lockState(static_cast<WarningState>(0))
    {
        // m_object is intentionally untouched. Retail initializes it only through init/setObject.
    }

    virtual void addRef() { ++m_refCount; }
    virtual int getRef() { return m_refCount; }
    virtual int release()
    {
        const int remaining = --m_refCount;
        if (!remaining)
            delete this;
        return remaining;
    }
    virtual void* queryInterface(unsigned int iid) const
    {
        if (iid == IID_IBase || iid == IID_IObjectComponent || iid == IID_IWeaponHudComp)
            return const_cast<WeaponHudComp*>(this);
        return 0;
    }
    virtual unsigned int getInterfaceID() const { return IID_IWeaponHudComp; }
    virtual void init(IObject* object) { m_object = object; }
    virtual void setObject(IObject* object) { m_object = object; }
    virtual IObject* getObject() const { return m_object; }
    virtual bool hasUpdate() const { return false; }
    virtual void handleUpdate(float) {}

    virtual int getGuiIndex(bool sprinting)
    {
        if (sprinting && m_template->getDisableOnSprint())
            return 0;
        if (m_template->getAltGuiIndex() == -1)
            return m_template->getGuiIndex();

        IWeaponObject* weapon = m_object
            ? static_cast<IWeaponObject*>(m_object->queryInterface(IID_IWeaponObject)) : 0;
        if (m_template->getSwitchGuiOnCamera())
        {
            IPlayerControlObject* pco = findParentPlayerControlObject(m_object);
            if (!pco || !weapon)
                return m_template->getGuiIndex();

            ICameraObject* camera = pco->getActiveCamera();
            if (weapon->getChangeCameraId() == camera->getCameraId())
                return m_template->getAltGuiIndex();

            IObject* ownObject = static_cast<IObject*>(m_object->queryInterface(IID_IObject));
            if (!getRootParent(ownObject))
                return m_template->getGuiIndex();

            camera = pco->getActiveCamera();
            if (!camera)
                return m_template->getGuiIndex();
            IObject* cameraObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));
            IObject* cameraRoot = getRootParent(cameraObject);
            if (cameraRoot->queryInterface(IID_IProjectileObject))
                return m_template->getAltGuiIndex();
            return m_template->getGuiIndex();
        }

        if (weapon && weapon->getZoom())
            return m_template->getAltGuiIndex();
        return m_template->getGuiIndex();
    }

    virtual std::string getCrossHairIcon()
    {
        IWeaponObject* weapon = m_object
            ? static_cast<IWeaponObject*>(m_object->queryInterface(IID_IWeaponObject)) : 0;
        if (weapon && weapon->isFiring())
        {
            // Both binaries destroy the first by-value result after the emptiness test, then call
            // the virtual getter again to materialize the returned copy.
            bool hasAlternate;
            {
                const std::string alternate = m_template->getAltCrosshairIcon();
                hasAlternate = !alternate.empty();
            }
            if (hasAlternate)
                return m_template->getAltCrosshairIcon();
        }
        return m_template->getCrosshairIcon();
    }

    virtual void setLockState(WarningState state)
    {
        if (state == m_lockState)
            return;

        const int next = static_cast<int>(state);
        const int previous = static_cast<int>(m_lockState);
        // The sound owner is dice::hfe::g_bfSound: ds:A23DC0 +2C playSound / +34 stopSound.
        if (next == 0 && previous != 0)
        {
            g_bfSound->stopSound(m_template->m_hudInfo.firstLockSound);
            g_bfSound->stopSound(m_template->m_hudInfo.secondLockSound);
        }
        else if (next == 2 && previous != 2)
        {
            g_bfSound->stopSound(m_template->m_hudInfo.firstLockSound);
            g_bfSound->playSound(m_template->m_hudInfo.secondLockSound);
        }
        else if (next == 1 && previous != 1)
        {
            g_bfSound->stopSound(m_template->m_hudInfo.secondLockSound);
            g_bfSound->playSound(m_template->m_hudInfo.firstLockSound);
        }
        m_lockState = state;
    }

    virtual void playOverheatSound()
    {
        g_bfSound->playSound(m_template->m_hudInfo.overheatSound);         // ds:A23DC0 +2C
    }

    virtual bool getShowOnCamMode(int mode)
    {
        if (m_template->getShowOnCamMode(mode))
            return true;

        IPlayerControlObject* pco = findParentPlayerControlObject(m_object);
        if (!pco)
            return false;
        ICameraObject* camera = pco->getActiveCamera();
        if (!camera)
            return false;
        IObject* cameraObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));
        IObject* cameraRoot = getRootParent(cameraObject);
        return cameraRoot->queryInterface(IID_IProjectileObject) != 0;
    }

    virtual WeaponHudIcons* getHudIcons() { return &m_template->m_hudIcons; }
    virtual WeaponHudInfo* getHudInfo() { return &m_template->m_hudInfo; }
    virtual ~WeaponHudComp() {}
    virtual WarningState getLockState() { return m_lockState; }

private:
    int m_refCount;
    WeaponHudCompTemplate* m_template;
    IObject* m_object;
    WarningState m_lockState;
};

WeaponHudCompTemplate::WeaponHudCompTemplate()
    : m_refCount(1), m_needsBuild(true), m_unknownString10(), m_unknownString14(),
      m_hudIcons()
{
    // m_hudInfo is deliberately omitted above: retail leaves unknownBool0/1 untouched.
    // Retail explicitly assigns only these first six paths; all ten were already constructed empty.
    m_hudIcons.weaponIcon = "";
    m_hudIcons.altWeaponIcon = "";
    m_hudIcons.selectIcon = "";
    m_hudIcons.specialAbilityIcon = "";
    m_hudIcons.crosshairIcon = "";
    m_hudIcons.altCrosshairIcon = "";

    m_hudInfo.showOnCamModes.push_back(1);
    m_hudInfo.crosshairIconSize = 32;
    m_hudInfo.hudName = " ";
    m_hudInfo.guiIndex = 5;
    m_hudInfo.altGuiIndex = -1;
    m_hudInfo.deviationFactor = 10;
    m_hudInfo.maxVisualDeviation = 1000;
    m_hudInfo.minVisualDeviation = 0;
    m_hudInfo.enablePostProcessingOnGuiIndex = -1;
    m_hudInfo.hasMag = true;
    m_hudInfo.hasFireRate = true;
    m_hudInfo.hasRangeFinder = false;
    m_hudInfo.switchGuiOnCamera = false;
    m_hudInfo.enableMouse = false;
    m_hudInfo.showClips = true;
    m_hudInfo.showAmmo = true;
    m_hudInfo.showInfo = true;
    m_hudInfo.disableOnSprint = false;
    m_hudInfo.displaySelectOnActivation = false;
    m_hudInfo.firstLockSound = "";
    m_hudInfo.secondLockSound = "";
    m_hudInfo.overheatSound = "";
}

WeaponHudCompTemplate::~WeaponHudCompTemplate() {}
void WeaponHudCompTemplate::addRef() { ++m_refCount; }
int WeaponHudCompTemplate::getRef() { return m_refCount; }
int WeaponHudCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        delete this;
    return remaining;
}
void* WeaponHudCompTemplate::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent || iid == IID_IWeaponHudCompTemplate)
        return const_cast<WeaponHudCompTemplate*>(this);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<WeaponHudCompTemplate*>(this));
    return 0;
}
unsigned int WeaponHudCompTemplate::getClassID() const { return CID_WeaponHudCompTemplate; }
unsigned int WeaponHudCompTemplate::getInterfaceID() { return IID_IWeaponHudCompTemplate; }
IObjectComponent* WeaponHudCompTemplate::createComponent()
{
    if (m_needsBuild)
        m_needsBuild = false;
    return new WeaponHudComp(this);
}
void WeaponHudCompTemplate::objectCreated(IObject*) {}
void WeaponHudCompTemplate::objectDestroyed(IObject*) {}
void WeaponHudCompTemplate::setTemplate(IObjectTemplate*) {}
void* WeaponHudCompTemplate::queryImplementation(unsigned int cid)
{
    return cid == getClassID() ? this : 0;
}

void WeaponHudCompTemplate::setGuiIndex(int value) { m_hudInfo.guiIndex = value; }
int WeaponHudCompTemplate::getGuiIndex() { return m_hudInfo.guiIndex; }
void WeaponHudCompTemplate::setAltGuiIndex(int value) { m_hudInfo.altGuiIndex = value; }
int WeaponHudCompTemplate::getAltGuiIndex() { return m_hudInfo.altGuiIndex; }
void WeaponHudCompTemplate::setHudName(std::string value)
{
    m_hudInfo.hudName = value;
    m_hudInfo.hudSid = value;
}
void WeaponHudCompTemplate::setHudSid(std::string value) { setHudName(value); }
std::string WeaponHudCompTemplate::getHudName() { return m_hudInfo.hudName; }
std::string WeaponHudCompTemplate::getHudSid() { return m_hudInfo.hudSid; }
void WeaponHudCompTemplate::setEnableMouse(bool value) { m_hudInfo.enableMouse = value; }
bool WeaponHudCompTemplate::getEnableMouse() { return m_hudInfo.enableMouse; }

void WeaponHudCompTemplate::setWeaponIcon(std::string value) { m_hudIcons.weaponIcon = value; }
std::string WeaponHudCompTemplate::getWeaponIcon() { return m_hudIcons.weaponIcon; }
void WeaponHudCompTemplate::setAltWeaponIcon(std::string value) { m_hudIcons.altWeaponIcon = value; }
std::string WeaponHudCompTemplate::getAltWeaponIcon() { return m_hudIcons.altWeaponIcon; }
void WeaponHudCompTemplate::setSpecialAbilityIcon(std::string value)
{
    m_hudIcons.specialAbilityIcon = value;
}
std::string WeaponHudCompTemplate::getSpecialAbilityIcon() { return m_hudIcons.specialAbilityIcon; }
void WeaponHudCompTemplate::setSelectIcon(std::string value) { m_hudIcons.selectIcon = value; }
std::string WeaponHudCompTemplate::getSelectIcon() { return m_hudIcons.selectIcon; }
void WeaponHudCompTemplate::setCrosshairIcon(std::string value) { m_hudIcons.crosshairIcon = value; }
std::string WeaponHudCompTemplate::getCrosshairIcon() { return m_hudIcons.crosshairIcon; }
void WeaponHudCompTemplate::setCrosshairIconSize(int value) { m_hudInfo.crosshairIconSize = value; }
int WeaponHudCompTemplate::getCrosshairIconSize() { return m_hudInfo.crosshairIconSize; }
void WeaponHudCompTemplate::setAltCrosshairIcon(std::string value)
{
    m_hudIcons.altCrosshairIcon = value;
}
std::string WeaponHudCompTemplate::getAltCrosshairIcon() { return m_hudIcons.altCrosshairIcon; }
void WeaponHudCompTemplate::setAmmoBarFrontIcon(std::string value)
{
    m_hudIcons.ammoBarFrontIcon = value;
}
std::string WeaponHudCompTemplate::getAmmoBarFrontIcon() { return m_hudIcons.ammoBarFrontIcon; }
void WeaponHudCompTemplate::setAmmoBarBackIcon(std::string value)
{
    m_hudIcons.ammoBarBackIcon = value;
}
std::string WeaponHudCompTemplate::getAmmoBarBackIcon() { return m_hudIcons.ammoBarBackIcon; }
void WeaponHudCompTemplate::setClipIcon(std::string value) { m_hudIcons.clipIcon = value; }
std::string WeaponHudCompTemplate::getClipIcon() { return m_hudIcons.clipIcon; }
void WeaponHudCompTemplate::setAmmoIcon(std::string value) { m_hudIcons.ammoIcon = value; }
std::string WeaponHudCompTemplate::getAmmoIcon() { return m_hudIcons.ammoIcon; }

void WeaponHudCompTemplate::setHasFireRate(bool value) { m_hudInfo.hasFireRate = value; }
bool WeaponHudCompTemplate::getHasFireRate() { return m_hudInfo.hasFireRate; }
void WeaponHudCompTemplate::setHasMag(bool value) { m_hudInfo.hasMag = value; }
bool WeaponHudCompTemplate::getHasMag() { return m_hudInfo.hasMag; }
void WeaponHudCompTemplate::setHasRangeFinder(bool value) { m_hudInfo.hasRangeFinder = value; }
bool WeaponHudCompTemplate::getHasRangeFinder() { return m_hudInfo.hasRangeFinder; }
void WeaponHudCompTemplate::setSwitchGuiOnCamera(bool value) { m_hudInfo.switchGuiOnCamera = value; }
bool WeaponHudCompTemplate::getSwitchGuiOnCamera() { return m_hudInfo.switchGuiOnCamera; }
void WeaponHudCompTemplate::setShowOnCamMode(int mode)
{
    m_hudInfo.showOnCamModes.clear();
    m_hudInfo.showOnCamModes.push_back(mode);
}
void WeaponHudCompTemplate::addShowOnCamMode(int mode) { m_hudInfo.showOnCamModes.push_back(mode); }
bool WeaponHudCompTemplate::getShowOnCamMode(int mode)
{
    for (std::vector<int>::const_iterator it = m_hudInfo.showOnCamModes.begin();
         it != m_hudInfo.showOnCamModes.end(); ++it)
    {
        if (*it == mode)
            return true;
    }
    return false;
}
void WeaponHudCompTemplate::setShowAmmo(bool value) { m_hudInfo.showAmmo = value; }
bool WeaponHudCompTemplate::getShowAmmo() { return m_hudInfo.showAmmo; }
void WeaponHudCompTemplate::setShowClips(bool value) { m_hudInfo.showClips = value; }
bool WeaponHudCompTemplate::getShowClips() { return m_hudInfo.showClips; }
void WeaponHudCompTemplate::setDeviationFactor(int value) { m_hudInfo.deviationFactor = value; }
int WeaponHudCompTemplate::getDeviationFactor() { return m_hudInfo.deviationFactor; }
void WeaponHudCompTemplate::setMaxVisualDeviation(int value) { m_hudInfo.maxVisualDeviation = value; }
int WeaponHudCompTemplate::getMaxVisualDeviation() { return m_hudInfo.maxVisualDeviation; }
void WeaponHudCompTemplate::setMinVisualDeviation(int value) { m_hudInfo.minVisualDeviation = value; }
int WeaponHudCompTemplate::getMinVisualDeviation() { return m_hudInfo.minVisualDeviation; }
void WeaponHudCompTemplate::setEnablePostProcessingOnGuiIndex(int value)
{
    m_hudInfo.enablePostProcessingOnGuiIndex = value;
}
int WeaponHudCompTemplate::getEnablePostProcessingOnGuiIndex()
{
    return m_hudInfo.enablePostProcessingOnGuiIndex;
}
void WeaponHudCompTemplate::setFirstLockSound(std::string value) { m_hudInfo.firstLockSound = value; }
std::string WeaponHudCompTemplate::getFirstLockSound() { return m_hudInfo.firstLockSound; }
void WeaponHudCompTemplate::setSecondLockSound(std::string value) { m_hudInfo.secondLockSound = value; }
std::string WeaponHudCompTemplate::getSecondLockSound() { return m_hudInfo.secondLockSound; }
void WeaponHudCompTemplate::setOverheatSound(std::string value) { m_hudInfo.overheatSound = value; }
std::string WeaponHudCompTemplate::getOverheatSound() { return m_hudInfo.overheatSound; }
void WeaponHudCompTemplate::setShowInfo(bool value) { m_hudInfo.showInfo = value; }
bool WeaponHudCompTemplate::getShowInfo() { return m_hudInfo.showInfo; }
void WeaponHudCompTemplate::setDisableOnSprint(bool value) { m_hudInfo.disableOnSprint = value; }
bool WeaponHudCompTemplate::getDisableOnSprint() { return m_hudInfo.disableOnSprint; }
void WeaponHudCompTemplate::setDisplaySelectOnActivation(bool value)
{
    m_hudInfo.displaySelectOnActivation = value;
}
bool WeaponHudCompTemplate::getDisplaySelectOnActivation()
{
    return m_hudInfo.displaySelectOnActivation;
}

bool WeaponHudCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream out;
    if (!m_hudIcons.weaponIcon.empty())
        out << "ObjectTemplate.weaponHud.weaponIcon " << m_hudIcons.weaponIcon << std::endl;
    if (!m_hudIcons.altWeaponIcon.empty())
        out << "ObjectTemplate.weaponHud.altWeaponIcon " << m_hudIcons.altWeaponIcon << std::endl;
    if (!m_hudIcons.selectIcon.empty())
        out << "ObjectTemplate.weaponHud.selectIcon " << m_hudIcons.selectIcon << std::endl;
    if (!m_hudIcons.specialAbilityIcon.empty())
        out << "ObjectTemplate.weaponHud.specialAbilityIcon " << m_hudIcons.specialAbilityIcon << std::endl;
    if (!m_hudIcons.crosshairIcon.empty())
        out << "ObjectTemplate.weaponHud.crosshairIcon " << m_hudIcons.crosshairIcon << std::endl;
    if (m_hudInfo.crosshairIconSize != 32)
        out << "ObjectTemplate.weaponHud.crosshairIconSize " << m_hudInfo.crosshairIconSize << std::endl;
    if (!m_hudIcons.altCrosshairIcon.empty())
        out << "ObjectTemplate.weaponHud.altCrosshairIcon " << m_hudIcons.altCrosshairIcon << std::endl;
    if (m_hudInfo.hudName != " ")
        out << "ObjectTemplate.weaponHud.hudName \"" << m_hudInfo.hudSid << "\"" << std::endl;
    if (m_hudInfo.guiIndex != 5)
        out << "ObjectTemplate.weaponHud.guiIndex " << m_hudInfo.guiIndex << std::endl;
    if (m_hudInfo.altGuiIndex != -1)
        out << "ObjectTemplate.weaponHud.altGuiIndex " << m_hudInfo.altGuiIndex << std::endl;
    if (m_hudInfo.deviationFactor != 10)
        out << "ObjectTemplate.weaponHud.deviationFactor " << m_hudInfo.deviationFactor << std::endl;
    if (m_hudInfo.maxVisualDeviation != 1000)
        out << "ObjectTemplate.weaponHud.maxVisualDeviation " << m_hudInfo.maxVisualDeviation << std::endl;
    if (m_hudInfo.minVisualDeviation != 0)
        out << "ObjectTemplate.weaponHud.minVisualDeviation " << m_hudInfo.minVisualDeviation << std::endl;
    if (!m_hudInfo.hasMag)
        out << "ObjectTemplate.weaponHud.hasMag " << m_hudInfo.hasMag << std::endl;
    if (!m_hudInfo.hasFireRate)
        out << "ObjectTemplate.weaponHud.hasFireRate " << m_hudInfo.hasFireRate << std::endl;
    if (m_hudInfo.hasRangeFinder)
        out << "ObjectTemplate.weaponHud.hasRangeFinder " << m_hudInfo.hasRangeFinder << std::endl;
    if (m_hudInfo.switchGuiOnCamera)
        out << "ObjectTemplate.weaponHud.switchGuiOnCamera " << m_hudInfo.switchGuiOnCamera << std::endl;

    const int firstCameraMode = m_hudInfo.showOnCamModes.front();
    if (firstCameraMode != 1)
        out << "ObjectTemplate.weaponHud.showOnCamMode " << firstCameraMode << std::endl;
    for (std::size_t i = 1; i < m_hudInfo.showOnCamModes.size(); ++i)
        out << "ObjectTemplate.weaponHud.addShowOnCamMode " << m_hudInfo.showOnCamModes[i] << std::endl;

    if (m_hudInfo.enableMouse)
        out << "ObjectTemplate.weaponHud.enableMouse " << m_hudInfo.enableMouse << std::endl;
    if (!m_hudInfo.showAmmo)
        out << "ObjectTemplate.weaponHud.showAmmo " << m_hudInfo.showAmmo << std::endl;
    if (!m_hudInfo.showClips)
        out << "ObjectTemplate.weaponHud.showClips " << m_hudInfo.showClips << std::endl;
    if (!m_hudInfo.showInfo)
        out << "ObjectTemplate.weaponHud.showInfo " << m_hudInfo.showInfo << std::endl;
    if (m_hudInfo.disableOnSprint)
        out << "ObjectTemplate.weaponHud.disableOnSprint " << m_hudInfo.disableOnSprint << std::endl;
    if (m_hudInfo.displaySelectOnActivation)
        out << "ObjectTemplate.weaponHud.displaySelectOnActivation "
            << m_hudInfo.displaySelectOnActivation << std::endl;
    if (m_hudInfo.enablePostProcessingOnGuiIndex != -1)
        out << "ObjectTemplate.weaponHud.enablePostProcessingOnGuiIndex "
            << m_hudInfo.enablePostProcessingOnGuiIndex << std::endl;
    if (!m_hudInfo.firstLockSound.empty())
        out << "ObjectTemplate.weaponHud.firstLockSound " << m_hudInfo.firstLockSound << std::endl;
    if (!m_hudInfo.secondLockSound.empty())
        out << "ObjectTemplate.weaponHud.secondLockSound " << m_hudInfo.secondLockSound << std::endl;
    if (!m_hudInfo.overheatSound.empty())
        out << "ObjectTemplate.weaponHud.overheatSound " << m_hudInfo.overheatSound << std::endl;

    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

#if defined(_M_IX86)
static_assert(sizeof(WeaponHudComp) == 0x14, "WeaponHudComp must retain its retail x86 size");
#endif

} // namespace world
} // namespace hfe
} // namespace dice
