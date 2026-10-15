// Path: [INFERRED], beside VehicleHudCompTemplate.h.
// ELF: template 0x822A2A0..0x822D5C9, runtime 0x8229AA0..0x822A29C.
// PE: template ctor 0x65B650/vtable 0x905DC8, runtime ctor 0x68A270/vtable 0x90B910.
#include "VehicleHudCompTemplate.h"

#include "ICameraObject.h"
#include "IObject.h"
#include "IPostProcessComp.h"
#include "IPlayer.h"
#include "IVehicleHudComp.h"
#include "IWeaponHudComp.h"
#include "ObjectLayoutAccess.h"
#include "WeaponHudCompTemplate.h" // WeaponHudInfo returned by IWeaponHudComp
#include "../../Core/BuildConfig.h"
#include "../../Core/Ids.h"
#include "../../Sound/BfSound.h"     // g_bfSound - the vehicle sounds
#include "../../Core/StringUtil.h"
#include "../../IO/Stream/IStream.h"
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <sstream>

#if !DEDICATED
#include "../../Menu/BF2HudManager.h"
#include "../../Menu/Ref2System.h"
namespace dice { namespace hfe { extern hud::BF2HudManager* hudManager; } }
#endif

namespace dice {
namespace hfe {
namespace world {

namespace {

// The handle is std::intptr_t: int on x86, the full texture address on native x64.
std::intptr_t resolveVehicleHudIcon(std::string name)
{
#if !DEDICATED
    // deliberately calls getSystem twice on the success path.
    if (hudManager && hudManager->getSystem())
        return hudManager->getSystem()->loadTexture(name.c_str(), true);
#else
    (void)name;
#endif
    return 0;
}

IPlayerControlObject* findPlayerControlObject(IObject* object)
{
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

IWeaponHudComp* cachedWeaponHud(IObject* object)
{
    return static_cast<IWeaponHudComp*>(object_detail::LayoutAccess::weaponHudComponent(object));
}

IPostProcessComp* cachedPostProcess(IObject* object)
{
    return static_cast<IPostProcessComp*>(object_detail::LayoutAccess::postProcessComponent(object));
}

} // namespace

class VehicleHudComp : public IVehicleHudComp
{
public:
    explicit VehicleHudComp(VehicleHudCompTemplate* componentTemplate)
        : m_refCount(1), m_template(componentTemplate), m_isPlayingPantingSound(false)
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
        if (iid == IID_IBase || iid == IID_IObjectComponent || iid == IID_IVehicleHudComp)
            return const_cast<VehicleHudComp*>(this);
        return 0;
    }
    virtual unsigned int getInterfaceID() const { return IID_IVehicleHudComp; }
    virtual void init(IObject* object) { m_object = object; }
    virtual void setObject(IObject* object) { m_object = object; }
    virtual IObject* getObject() const { return m_object; }
    virtual bool hasUpdate() const { return false; }
    virtual void handleUpdate(float) {}

    virtual int getGuiIndex(bool sprinting)
    {
        int cameraMode = 1;
        if (sprinting && m_template->getDisableOnSprint())
            return 0;

        IPlayerControlObject* pco = findPlayerControlObject(m_object);
        if (!pco)
            return ownGuiIndex(cameraMode);

        ICameraObject* camera = pco->getActiveCamera();
        if (camera)
            cameraMode = camera->getViewMode();

        IWeaponHudComp* weaponHud = 0;
        if (m_template->getUseChildHud())
        {
            IPlayer* player = pco->getOccupingPlayer();
            if (!player)
                return ownGuiIndex(cameraMode);
            IObject* defaultVehicle = player->getDefaultVehicle();
            if (!defaultVehicle)
                return ownGuiIndex(cameraMode);
            IPlayerControlObject* childPco = findPlayerControlObject(defaultVehicle);
            if (!childPco)
                return ownGuiIndex(cameraMode);
            IObject* weapon = childPco->getActiveWeapon(0);
            if (!weapon || (weapon->getFlags() & IObject::OFIsDisabled) != 0)
                return ownGuiIndex(cameraMode);
            weaponHud = cachedWeaponHud(weapon);
        }
        else
        {
            IObject* weapon = pco->getActiveWeapon(0);
            if (!weapon || (weapon->getFlags() & IObject::OFIsDisabled) != 0)
                return ownGuiIndex(cameraMode);
            weaponHud = cachedWeaponHud(weapon);
            if (!weaponHud)
                return ownGuiIndex(cameraMode);

            if (weaponHud->getGuiIndex(sprinting) != -1)
            {
                if (!weaponHud->getShowOnCamMode(cameraMode))
                {
                    if (weaponHud->getHudInfo()->showOnCamModes.size() > 1)
                        return 0;
                    if (weaponHud->getHudInfo()->showOnCamModes.size() == 1
                        && !weaponHud->getShowOnCamMode(-1))
                        return 0;
                }
                return weaponHud->getGuiIndex(sprinting);
            }

            // Retail repeats the player-control lookup before selecting weapon slot one.
            IPlayerControlObject* secondaryPco = findPlayerControlObject(m_object);
            IObject* secondaryWeapon = secondaryPco->getActiveWeapon(1);
            if (!secondaryWeapon)
                return ownGuiIndex(cameraMode);
            weaponHud = cachedWeaponHud(secondaryWeapon);
        }

        if (weaponHud && weaponHud->getShowOnCamMode(cameraMode))
            return weaponHud->getGuiIndex(sprinting);
        return ownGuiIndex(cameraMode);
    }

    virtual IPostProcessComp* getEnablePostProcessing(int guiIndex)
    {
        if (guiIndex == m_template->getGuiIndex()
            && guiIndex == m_template->getEnablePostProcessingOnGuiIndex())
        {
            return cachedPostProcess(m_object);
        }

        IPlayerControlObject* pco = findPlayerControlObject(m_object);
        if (!pco)
            return 0;

        IObject* weapon = pco->getActiveWeapon(0);
        if (!weapon)
            return 0;
        IWeaponHudComp* weaponHud = cachedWeaponHud(weapon);
        if (!weaponHud)
            return 0;
        if (guiIndex == weaponHud->getHudInfo()->enablePostProcessingOnGuiIndex)
            return cachedPostProcess(weapon);

        // Retail repeats the lookup and tries the secondary active-weapon slot.
        pco = findPlayerControlObject(m_object);
        weapon = pco->getActiveWeapon(1);
        if (!weapon)
            return 0;
        weaponHud = cachedWeaponHud(weapon);
        if (weaponHud && guiIndex == weaponHud->getHudInfo()->enablePostProcessingOnGuiIndex)
            return cachedPostProcess(weapon);
        return 0;
    }

    virtual std::string getVehicleIcon(int index)
    {
        std::vector<std::string>::const_iterator icon = m_template->m_vehicleHudIcons.vehicleIcons.begin();
        std::string result;
        if (index < m_template->getVehicleIconNumber())
            icon += index;
        if (icon != m_template->m_vehicleHudIcons.vehicleIcons.end())
            result = *icon;
        return result;
    }

    virtual std::intptr_t getVehicleIconPtr(int index)
    {
        std::vector<std::intptr_t>::const_iterator icon =
            m_template->m_vehicleHudIcons.vehicleIconPtrs.begin();
        if (index < m_template->getVehicleIconNumber())
            icon += index;
        if (icon != m_template->m_vehicleHudIcons.vehicleIconPtrs.end())
            return *icon;
        return 0;
    }

    virtual std::string getAbilityIcon(int index)
    {
        std::vector<std::string>::const_iterator icon = m_template->m_vehicleHudIcons.abilityIcons.begin();
        std::string result;
        if (index < m_template->getAbilityIconNumber())
            icon += index;
        if (icon != m_template->m_vehicleHudIcons.abilityIcons.end())
            result = *icon;
        return result;
    }

    virtual std::intptr_t getAbilityIconPtr(int index)
    {
        std::vector<std::intptr_t>::const_iterator icon =
            m_template->m_vehicleHudIcons.abilityIconPtrs.begin();
        if (index < m_template->getAbilityIconNumber())
            icon += index;
        if (icon != m_template->m_vehicleHudIcons.abilityIconPtrs.end())
            return *icon;
        return 0;
    }

    virtual bool getShowOnCamMode(int mode) { return m_template->getShowOnCamMode(mode); }

    virtual void playingPantingSound(bool)
    {
        // Shipped defect: the argument is ignored and the state byte is never updated.
        g_bfSound->playSound(m_template->m_vehicleHudInfo.pantingSound);   // ds:A23DC0 +2C
    }

    virtual bool getIsPlayingPantingSound() { return m_isPlayingPantingSound; }

    virtual void playInjurySound()
    {
        g_bfSound->playSound(m_template->m_vehicleHudInfo.injurySound);    // ds:A23DC0 +2C
    }

    virtual VehicleHudIcons* getHudIcons() { return &m_template->m_vehicleHudIcons; }
    virtual VehicleHudInfo* getHudInfo() { return &m_template->m_vehicleHudInfo; }
    virtual ~VehicleHudComp() {}

private:
    int ownGuiIndex(int cameraMode)
    {
        if (!m_template->getShowOnCamMode(cameraMode))
            return 0;
        return m_template->getGuiIndex();
    }

    int m_refCount;
    VehicleHudCompTemplate* m_template;
    IObject* m_object;
    bool m_isPlayingPantingSound;
};

VehicleHudCompTemplate::VehicleHudCompTemplate()
    : m_refCount(1), m_needsBuild(true), m_unknownString10(), m_unknownString14(),
      m_vehicleHudIcons()
{
    // m_vehicleHudInfo is deliberately omitted above: retail leaves unknownBool untouched.
    m_vehicleHudIcons.vehicleIcons.push_back(std::string());
    m_vehicleHudIcons.abilityIcons.push_back(std::string());
    m_vehicleHudIcons.miniMapIcon = "";
    m_vehicleHudIcons.typeIcon = "";
    m_vehicleHudIcons.vehicleIconPtrs.push_back(0);
    m_vehicleHudIcons.abilityIconPtrs.push_back(0);
    m_vehicleHudIcons.miniMapIconPtr = 0;
    m_vehicleHudIcons.typeIconPtr = 0;
    m_vehicleHudIcons.spottedIconPtr = 0;

    m_vehicleHudInfo.vehicleType = -1;
    m_vehicleHudInfo.showOnCamModes.push_back(1);
    m_vehicleHudInfo.showInfo = true;
    m_vehicleHudInfo.disableOnSprint = false;
    m_vehicleHudInfo.miniMapIconLeaderSizeValue = 1;
    m_vehicleHudInfo.miniMapIconLeaderSize = "Medium";
    m_vehicleHudInfo.hudName = "";
    m_vehicleHudInfo.hasTurretIcon = false;
    m_vehicleHudInfo.usePlayerIcon = false;
    m_vehicleHudInfo.useChildHud = false;
    m_vehicleHudInfo.standardHelpEnabled = true;
    m_vehicleHudInfo.showCrossHair = true;
    m_vehicleHudInfo.isCoDriver = false;
    m_vehicleHudInfo.enablePostProcessingOnGuiIndex = -1;
    m_vehicleHudInfo.miniMapIconSize = 16;
    m_vehicleHudInfo.spottedIconSize = 16;
    m_vehicleHudInfo.guiIndex = 0;
    m_vehicleHudInfo.vehicleIconPos.x = 0.0f;
    m_vehicleHudInfo.vehicleIconPos.y = 0.0f;
    m_vehicleHudInfo.maxClimbSpeed = 10;
    m_vehicleHudInfo.useSelectionIcons = true;
    m_vehicleHudInfo.displaySelectionOnEnter = false;
    m_vehicleHudInfo.useVehicleCommRose = true;
    m_vehicleHudInfo.pantingSound = "";
    m_vehicleHudInfo.injurySound = "";
}

VehicleHudCompTemplate::~VehicleHudCompTemplate() {}
void VehicleHudCompTemplate::addRef() { ++m_refCount; }
int VehicleHudCompTemplate::getRef() { return m_refCount; }
int VehicleHudCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        delete this;
    return remaining;
}
void* VehicleHudCompTemplate::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent || iid == IID_IVehicleHudCompTemplate)
        return const_cast<VehicleHudCompTemplate*>(this);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<VehicleHudCompTemplate*>(this));
    return 0;
}
unsigned int VehicleHudCompTemplate::getClassID() const { return CID_VehicleHudCompTemplate; }
unsigned int VehicleHudCompTemplate::getInterfaceID() { return IID_IVehicleHudCompTemplate; }
IObjectComponent* VehicleHudCompTemplate::createComponent()
{
    if (m_needsBuild)
        m_needsBuild = false;
    return new VehicleHudComp(this);
}
void VehicleHudCompTemplate::objectCreated(IObject*) {}
void VehicleHudCompTemplate::objectDestroyed(IObject*) {}
void VehicleHudCompTemplate::setTemplate(IObjectTemplate*) {}
void* VehicleHudCompTemplate::queryImplementation(unsigned int cid)
{
    return cid == getClassID() ? this : 0;
}

void VehicleHudCompTemplate::setGuiIndex(int value) { m_vehicleHudInfo.guiIndex = value; }
int VehicleHudCompTemplate::getGuiIndex() { return m_vehicleHudInfo.guiIndex; }
void VehicleHudCompTemplate::setHudName(std::string value)
{
    m_vehicleHudInfo.hudName = value;
    m_vehicleHudInfo.hudSid = value;
}
void VehicleHudCompTemplate::setHudSid(std::string value) { setHudName(value); }
std::string VehicleHudCompTemplate::getHudName() { return m_vehicleHudInfo.hudName; }
std::string VehicleHudCompTemplate::getHudSid() { return m_vehicleHudInfo.hudSid; }

void VehicleHudCompTemplate::setVehicleIcon(const std::string& value)
{
    m_vehicleHudIcons.vehicleIcons.clear();
    m_vehicleHudIcons.vehicleIcons.push_back(value);
    m_vehicleHudIcons.vehicleIconPtrs.clear();
    m_vehicleHudIcons.vehicleIconPtrs.push_back(resolveVehicleHudIcon(value));
}
void* VehicleHudCompTemplate::getVehicleIcon(int index)
{
    std::vector<std::string>::iterator icon = m_vehicleHudIcons.vehicleIcons.begin();
    if (index < getVehicleIconNumber())
        icon += index;
    return &*icon;
}
void VehicleHudCompTemplate::addVehicleIcon(const std::string& value)
{
    m_vehicleHudIcons.vehicleIcons.push_back(value);
    m_vehicleHudIcons.vehicleIconPtrs.push_back(resolveVehicleHudIcon(value));
}
int VehicleHudCompTemplate::getVehicleIconNumber() const
{
    return static_cast<int>(m_vehicleHudIcons.vehicleIcons.size());
}

void VehicleHudCompTemplate::setAbilityIcon(const std::string& value)
{
    m_vehicleHudIcons.abilityIcons.clear();
    m_vehicleHudIcons.abilityIcons.push_back(value);
    m_vehicleHudIcons.abilityIconPtrs.clear();
    m_vehicleHudIcons.abilityIconPtrs.push_back(resolveVehicleHudIcon(value));
}
void* VehicleHudCompTemplate::getAbilityIcon(int index)
{
    std::vector<std::string>::iterator icon = m_vehicleHudIcons.abilityIcons.begin();
    if (index < getAbilityIconNumber())
        icon += index;
    return &*icon;
}
void VehicleHudCompTemplate::addAbilityIcon(const std::string& value)
{
    m_vehicleHudIcons.abilityIcons.push_back(value);
    m_vehicleHudIcons.abilityIconPtrs.push_back(resolveVehicleHudIcon(value));
}
int VehicleHudCompTemplate::getAbilityIconNumber() const
{
    return static_cast<int>(m_vehicleHudIcons.abilityIcons.size());
}

void VehicleHudCompTemplate::setShowCrossHair(bool value) { m_vehicleHudInfo.showCrossHair = value; }
bool VehicleHudCompTemplate::getShowCrossHair() { return m_vehicleHudInfo.showCrossHair; }
void VehicleHudCompTemplate::setUsePlayerIcon(bool value) { m_vehicleHudInfo.usePlayerIcon = value; }
bool VehicleHudCompTemplate::getUsePlayerIcon() { return m_vehicleHudInfo.usePlayerIcon; }
void VehicleHudCompTemplate::setDisableOnSprint(bool value) { m_vehicleHudInfo.disableOnSprint = value; }
bool VehicleHudCompTemplate::getDisableOnSprint() { return m_vehicleHudInfo.disableOnSprint; }
void VehicleHudCompTemplate::setUseChildHud(bool value) { m_vehicleHudInfo.useChildHud = value; }
bool VehicleHudCompTemplate::getUseChildHud() { return m_vehicleHudInfo.useChildHud; }
void VehicleHudCompTemplate::setStandardHelpEnabled(bool value) { m_vehicleHudInfo.standardHelpEnabled = value; }
bool VehicleHudCompTemplate::getStandardHelpEnabled() { return m_vehicleHudInfo.standardHelpEnabled; }
void VehicleHudCompTemplate::setVehicleIconPos(const Vec2& value) { m_vehicleHudInfo.vehicleIconPos = value; }
const Vec2& VehicleHudCompTemplate::getVehicleIconPos() { return m_vehicleHudInfo.vehicleIconPos; }
void VehicleHudCompTemplate::setHasTurretIcon(bool value) { m_vehicleHudInfo.hasTurretIcon = value; }
bool VehicleHudCompTemplate::getHasTurretIcon() { return m_vehicleHudInfo.hasTurretIcon; }
void VehicleHudCompTemplate::setShowInfo(bool value) { m_vehicleHudInfo.showInfo = value; }
bool VehicleHudCompTemplate::getShowInfo() { return m_vehicleHudInfo.showInfo; }
void VehicleHudCompTemplate::setIsCoDriver(bool value) { m_vehicleHudInfo.isCoDriver = value; }
bool VehicleHudCompTemplate::getIsCoDriver() { return m_vehicleHudInfo.isCoDriver; }

void VehicleHudCompTemplate::setShowOnCamMode(int mode)
{
    m_vehicleHudInfo.showOnCamModes.clear();
    m_vehicleHudInfo.showOnCamModes.push_back(mode);
}
void VehicleHudCompTemplate::addShowOnCamMode(int mode) { m_vehicleHudInfo.showOnCamModes.push_back(mode); }
bool VehicleHudCompTemplate::getShowOnCamMode(int mode)
{
    for (std::vector<int>::const_iterator it = m_vehicleHudInfo.showOnCamModes.begin();
         it != m_vehicleHudInfo.showOnCamModes.end(); ++it)
    {
        if (*it == mode)
            return true;
    }
    return false;
}

void VehicleHudCompTemplate::setMiniMapIcon(std::string value)
{
    m_vehicleHudIcons.miniMapIcon = value;
    m_vehicleHudIcons.miniMapIconPtr = resolveVehicleHudIcon(value);
}
std::string VehicleHudCompTemplate::getMiniMapIcon() { return m_vehicleHudIcons.miniMapIcon; }
void VehicleHudCompTemplate::setTypeIcon(std::string value)
{
    m_vehicleHudIcons.typeIcon = value;
    m_vehicleHudIcons.typeIconPtr = resolveVehicleHudIcon(value);
}
std::string VehicleHudCompTemplate::getTypeIcon() { return m_vehicleHudIcons.typeIcon; }
void VehicleHudCompTemplate::setMiniMapIconSize(int value) { m_vehicleHudInfo.miniMapIconSize = value; }
int VehicleHudCompTemplate::getMiniMapIconSize() { return m_vehicleHudInfo.miniMapIconSize; }
void VehicleHudCompTemplate::setMiniMapIconLeaderSize(std::string value)
{
    m_vehicleHudInfo.miniMapIconLeaderSize = value;
    if (getLowerCase(value) == "small")
        m_vehicleHudInfo.miniMapIconLeaderSizeValue = 0;
    else if (getLowerCase(value) == "medium")
        m_vehicleHudInfo.miniMapIconLeaderSizeValue = 1;
    else if (getLowerCase(value) == "large")
        m_vehicleHudInfo.miniMapIconLeaderSizeValue = 2;
}
std::string VehicleHudCompTemplate::getMiniMapIconLeaderSize()
{
    return m_vehicleHudInfo.miniMapIconLeaderSize;
}
void VehicleHudCompTemplate::setSpottedIcon(const std::string& value)
{
    m_vehicleHudIcons.spottedIcon = value;
    m_vehicleHudIcons.spottedIconPtr = resolveVehicleHudIcon(value);
}
std::string VehicleHudCompTemplate::getSpottedIcon() { return m_vehicleHudIcons.spottedIcon; }
void VehicleHudCompTemplate::setSpottedIconSize(int value) { m_vehicleHudInfo.spottedIconSize = value; }
int VehicleHudCompTemplate::getSpottedIconSize() { return m_vehicleHudInfo.spottedIconSize; }
void VehicleHudCompTemplate::setMaxClimbSpeed(int value) { m_vehicleHudInfo.maxClimbSpeed = value; }
int VehicleHudCompTemplate::getMaxClimbSpeed() { return m_vehicleHudInfo.maxClimbSpeed; }
void VehicleHudCompTemplate::setEnablePostProcessingOnGuiIndex(int value)
{
    m_vehicleHudInfo.enablePostProcessingOnGuiIndex = value;
}
int VehicleHudCompTemplate::getEnablePostProcessingOnGuiIndex()
{
    return m_vehicleHudInfo.enablePostProcessingOnGuiIndex;
}
void VehicleHudCompTemplate::setVehicleType(int value) { m_vehicleHudInfo.vehicleType = value; }
int VehicleHudCompTemplate::getVehicleType() { return m_vehicleHudInfo.vehicleType; }
void VehicleHudCompTemplate::setUseSelectionIcons(bool value) { m_vehicleHudInfo.useSelectionIcons = value; }
bool VehicleHudCompTemplate::getUseSelectionIcons() { return m_vehicleHudInfo.useSelectionIcons; }
void VehicleHudCompTemplate::setDisplaySelectionOnEnter(bool value)
{
    m_vehicleHudInfo.displaySelectionOnEnter = value;
}
bool VehicleHudCompTemplate::getDisplaySelectionOnEnter()
{
    return m_vehicleHudInfo.displaySelectionOnEnter;
}
void VehicleHudCompTemplate::setUseVehicleCommRose(bool value)
{
    m_vehicleHudInfo.useVehicleCommRose = value;
}
bool VehicleHudCompTemplate::getUseVehicleCommRose() { return m_vehicleHudInfo.useVehicleCommRose; }
void VehicleHudCompTemplate::setPantingSound(std::string value) { m_vehicleHudInfo.pantingSound = value; }
std::string VehicleHudCompTemplate::getPantingSound() { return m_vehicleHudInfo.pantingSound; }
void VehicleHudCompTemplate::setInjurySound(std::string value) { m_vehicleHudInfo.injurySound = value; }
std::string VehicleHudCompTemplate::getInjurySound() { return m_vehicleHudInfo.injurySound; }

bool VehicleHudCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream out;
    if (!m_vehicleHudInfo.hudName.empty())
        out << "ObjectTemplate.vehicleHud.hudName \"" << m_vehicleHudInfo.hudSid << "\"" << std::endl;
    if (!m_vehicleHudIcons.typeIcon.empty())
        out << "ObjectTemplate.vehicleHud.typeIcon " << m_vehicleHudIcons.typeIcon << std::endl;
    if (!m_vehicleHudIcons.miniMapIcon.empty())
        out << "ObjectTemplate.vehicleHud.miniMapIcon " << m_vehicleHudIcons.miniMapIcon << std::endl;
    if (m_vehicleHudInfo.miniMapIconSize != 16)
        out << "ObjectTemplate.vehicleHud.miniMapIconSize " << m_vehicleHudInfo.miniMapIconSize << std::endl;
    if (!m_vehicleHudIcons.spottedIcon.empty())
        out << "ObjectTemplate.vehicleHud.spottedIcon " << m_vehicleHudIcons.spottedIcon << std::endl;
    if (m_vehicleHudInfo.spottedIconSize != 16)
        out << "ObjectTemplate.vehicleHud.spottedIconSize " << m_vehicleHudInfo.spottedIconSize << std::endl;

    // Shipped quirk: it emits the default spelling "Medium", and omits non-default spellings.
    if (m_vehicleHudInfo.miniMapIconLeaderSize == "Medium")
        out << "ObjectTemplate.vehicleHud.miniMapIconLeaderSize "
            << m_vehicleHudInfo.miniMapIconLeaderSize << std::endl;
    if (m_vehicleHudInfo.hasTurretIcon)
        out << "ObjectTemplate.vehicleHud.hasTurretIcon " << m_vehicleHudInfo.hasTurretIcon << std::endl;
    if (!m_vehicleHudInfo.showInfo)
        out << "ObjectTemplate.vehicleHud.showInfo " << m_vehicleHudInfo.showInfo << std::endl;
    if (m_vehicleHudInfo.disableOnSprint)
        out << "ObjectTemplate.vehicleHud.disableOnSprint " << m_vehicleHudInfo.disableOnSprint << std::endl;

    const int firstCameraMode = m_vehicleHudInfo.showOnCamModes.front();
    if (firstCameraMode != 1)
        out << "ObjectTemplate.vehicleHud.showOnCamMode " << firstCameraMode << std::endl;
    for (std::size_t i = 1; i < m_vehicleHudInfo.showOnCamModes.size(); ++i)
        out << "ObjectTemplate.vehicleHud.addShowOnCamMode " << m_vehicleHudInfo.showOnCamModes[i] << std::endl;

    // Both prefixes are capitalized AbilityHud in the shipped vehicle serializer.
    if (!m_vehicleHudInfo.pantingSound.empty())
        out << "ObjectTemplate.AbilityHud.pantingSound " << m_vehicleHudInfo.pantingSound << std::endl;
    if (!m_vehicleHudInfo.injurySound.empty())
        out << "ObjectTemplate.AbilityHud.injurySound " << m_vehicleHudInfo.injurySound << std::endl;
    if (m_vehicleHudInfo.isCoDriver)
        out << "ObjectTemplate.vehicleHud.isCoDriver " << m_vehicleHudInfo.isCoDriver << std::endl;
    if (!m_vehicleHudInfo.showCrossHair)
        out << "ObjectTemplate.vehicleHud.showCrosshair " << m_vehicleHudInfo.showCrossHair << std::endl;
    if (m_vehicleHudInfo.usePlayerIcon)
        out << "ObjectTemplate.vehicleHud.usePlayerIcon " << m_vehicleHudInfo.usePlayerIcon << std::endl;
    if (!m_vehicleHudInfo.standardHelpEnabled)
        out << "ObjectTemplate.vehicleHud.standardHelpEnabled " << m_vehicleHudInfo.standardHelpEnabled << std::endl;
    if (m_vehicleHudInfo.useChildHud)
        out << "ObjectTemplate.vehicleHud.useChildHud " << m_vehicleHudInfo.useChildHud << std::endl;
    if (m_vehicleHudInfo.guiIndex != 0)
        out << "ObjectTemplate.vehicleHud.guiIndex " << m_vehicleHudInfo.guiIndex << std::endl;
    if (m_vehicleHudInfo.maxClimbSpeed != 10)
        out << "ObjectTemplate.vehicleHud.maxClimbSpeed " << m_vehicleHudInfo.maxClimbSpeed << std::endl;
    if (std::fabs(m_vehicleHudInfo.vehicleIconPos.x) + std::fabs(m_vehicleHudInfo.vehicleIconPos.y) > FLT_EPSILON)
        out << "ObjectTemplate.vehicleHud.vehicleIconPos " << m_vehicleHudInfo.vehicleIconPos << std::endl;
    if (m_vehicleHudInfo.enablePostProcessingOnGuiIndex != -1)
        out << "ObjectTemplate.vehicleHud.enablePostProcessingOnGuiIndex "
            << m_vehicleHudInfo.enablePostProcessingOnGuiIndex << std::endl;
    if (m_vehicleHudInfo.vehicleType != -1)
        out << "ObjectTemplate.vehicleHud.vehicleType " << m_vehicleHudInfo.vehicleType << std::endl;
    if (!m_vehicleHudInfo.useSelectionIcons)
        out << "ObjectTemplate.vehicleHud.useSelectionIcons " << m_vehicleHudInfo.useSelectionIcons << std::endl;
    if (m_vehicleHudInfo.displaySelectionOnEnter)
        out << "ObjectTemplate.vehicleHud.displaySelectionOnEnter "
            << m_vehicleHudInfo.displaySelectionOnEnter << std::endl;
    if (!m_vehicleHudInfo.useVehicleCommRose)
        out << "ObjectTemplate.vehicleHud.useVehicleCommRose " << m_vehicleHudInfo.useVehicleCommRose << std::endl;

    if (!m_vehicleHudIcons.vehicleIcons.front().empty())
    {
        out << "ObjectTemplate.vehicleHud.vehicleIcon " << m_vehicleHudIcons.vehicleIcons.front() << std::endl;
        for (std::size_t i = 1; i < m_vehicleHudIcons.vehicleIcons.size(); ++i)
            out << "ObjectTemplate.vehicleHud.addVehicleIcon " << m_vehicleHudIcons.vehicleIcons[i] << std::endl;
    }
    if (!m_vehicleHudIcons.abilityIcons.front().empty())
    {
        out << "ObjectTemplate.abilityHud.abilityIcon " << m_vehicleHudIcons.abilityIcons.front() << std::endl;
        for (std::size_t i = 1; i < m_vehicleHudIcons.abilityIcons.size(); ++i)
            out << "ObjectTemplate.abilityHud.addAbilityIcon " << m_vehicleHudIcons.abilityIcons[i] << std::endl;
    }

    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

#if defined(_M_IX86)
static_assert(sizeof(VehicleHudComp) == 0x14, "VehicleHudComp must retain its retail x86 size");
#endif

} // namespace world
} // namespace hfe
} // namespace dice
