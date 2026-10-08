// Console bindings for gameplay object templates. Module registration lives in Core/ModuleRegistry.cpp.
// [INFERRED] Source filename.
#include "../../../Core/ConsoleObjectMacro.h"
#include "../../../Core/Ids.h"                            // CID_TriggerTemplate
#include "../../../Core/StringUtil.h"                     // equalsIgnoreCase
#include "../../../Core/Types.h"                          // U32 - the metadata's spelling
#include "../../../Scene/Object/ObjectTemplateUtil.h"     // world::getActiveTemplate
#include "../../../Scene/Object/ObjectManager.h"          // objectManager->getActiveObject() - the spawner block
#include "../../../Math/Vec3.h"                           // Vec2 / Vec3 and their text forms
#include "../../../IO/Input/Input.h"                     // specialToggleWeaponInput enum parsing
#include "../AnimatedBundleTemplate.h"
#include "../MaskObjectTemplate.h"
#include "../ControlPointTemplate.h"
#include "../IPlayerControlObjectTemplate.h"              // VehicleCategory
#include "../PlayerControlObjectTemplate.h"               // authored exit transform/policy
#include "../ObjectSpawnerTemplate.h"
#include "../RemoteControlledObjectTemplate.h"
#include "../SupplyObjectTemplate.h"
#include "../DestroyableObjectTemplate.h"                 // respawn/corner console block
#include "../AntennaObjectTemplate.h"                     // the antenna.* console block
#include "../ItemContainerTemplate.h"                     // unlockLevel / replaceItem
#include "KitTemplate.h"                                  // the kit console block
#include "KitPart.h"
#include "SupplyDepot.h"
#include "../TargetObjectTemplate.h"
#include "../IObject.h"
#include "../IObjectTemplate.h"
#include "../Projectiles/ProjectileCompTemplates.h"       // TargetType stream pair
#include "../TemplateLeaves.h"                         // LadderTemplate console property
#include "AmbientEffectArea.h"
#include "ObjectSpawner.h"
#include "SoldierTemplate.h"
#include "Trigger.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>                                        // world::addConstantHelper's formatter

namespace dice {
namespace hfe {

namespace io {
void addConstantToAllConsoles(const std::string& name, const std::string& value);   // IO/Console/Console.cpp, 0x843AEF0
}

namespace world {

// Publishes an unsigned constant to all consoles using standard stream formatting.
void addConstantHelper(const std::string& name, unsigned int value)
{
    std::ostringstream text;
    text << value;
    io::addConstantToAllConsoles(name, text.str());
}

// The PCO serializer's two vocabulary writers live in this TU in both retail ports. VehicleCategory
// leaves an out-of-range value blank (ELF 0x857CDF0 / PE 0x47D280); VehicleType maps every
// unhandled value, including the intentional hole at 15, to VTNoVehicleType (ELF 0x857D3F0 /

std::ostream& operator<<(std::ostream& stream, VehicleCategory category)
{
    switch (static_cast<int>(category))
    {
    case 0: stream << "VCLand";              break;
    case 1: stream << "VCSea";               break;
    case 2: stream << "VCAir";               break;
    case 3: stream << "VCHelicopter";        break;
    case 4: stream << "VCNoVehicleCategory"; break;
    default:                                  break;
    }
    return stream;
}

// . Unlike most engine enum readers this one is case-sensitive, accepts both the
// prefixed and short spellings, and maps every unknown token to VCNoVehicleCategory. A one-byte
// numeric token first round-trips through the writer; only 0..4 therefore survive normalization.
std::istream& operator>>(std::istream& stream, VehicleCategory& category)
{
    std::string token;
    stream >> token;
    if (token.length() == 1)
    {
        int numeric = 0;
        if (std::sscanf(token.c_str(), "%d", &numeric) == 1)
        {
            std::ostringstream normalized;
            normalized << static_cast<VehicleCategory>(numeric);
            token = normalized.str();
        }
    }

    if (token == "VCLand" || token == "Land")
        category = static_cast<VehicleCategory>(0);
    else if (token == "VCSea" || token == "Sea")
        category = static_cast<VehicleCategory>(1);
    else if (token == "VCAir" || token == "Air")
        category = static_cast<VehicleCategory>(2);
    else if (token == "VCHelicopter" || token == "Helicopter")
        category = static_cast<VehicleCategory>(3);
    else
        category = static_cast<VehicleCategory>(4);
    return stream;
}

// / 0x47F7B0. The range metadata deliberately extends one value past the named
// vocabulary; 6 therefore remains unnamed and an input spelling that normalizes to it leaves the
// destination unchanged.
std::ostream& operator<<(std::ostream& stream, RemoteControlledObjectType type)
{
    switch (static_cast<int>(type))
    {
    case 0: stream << "RCNone";           break;
    case 1: stream << "RCArtillery";      break;
    case 2: stream << "RCSatellite";      break;
    case 3: stream << "RCUav";            break;
    case 4: stream << "RCHorizontalTurn"; break;
    case 5: stream << "RCVerticalTurn";   break;
    default:                               break;
    }
    return stream;
}

std::istream& operator>>(std::istream& stream, RemoteControlledObjectType& type)
{
    std::string token;
    stream >> token;
    if (token.length() == 1)
    {
        std::ostringstream normalized;
        normalized << static_cast<RemoteControlledObjectType>(std::atoi(token.c_str()));
        token = normalized.str();
    }

    if (equalsIgnoreCase(token, "RCNone"))
        type = RCNone;
    else if (equalsIgnoreCase(token, "RCArtillery"))
        type = RCArtillery;
    else if (equalsIgnoreCase(token, "RCSatellite"))
        type = RCSatellite;
    else if (equalsIgnoreCase(token, "RCUav"))
        type = RCUav;
    else if (equalsIgnoreCase(token, "RCHorizontalTurn"))
        type = RCHorizontalTurn;
    else if (equalsIgnoreCase(token, "RCVerticalTurn"))
        type = RCVerticalTurn;
    return stream;
}

std::ostream& operator<<(std::ostream& stream, VehicleType type)
{
    switch (static_cast<int>(type))
    {
    case 0:  stream << "VTHeavyTank";           break;
    case 1:  stream << "VTLightTank";           break;
    case 2:  stream << "VTArtillery";           break;
    case 3:  stream << "VTApc";                 break;
    case 4:  stream << "VTScoutCar";            break;
    case 5:  stream << "VTFighter";             break;
    case 6:  stream << "VTDiveBomber";          break;
    case 7:  stream << "VTBomber";              break;
    case 8:  stream << "VTAAGun";               break;
    case 9:  stream << "VTStationaryMG";        break;
    case 10: stream << "VTDestroyer";           break;
    case 11: stream << "VTBattleship";          break;
    case 12: stream << "VTCarrier";             break;
    case 13: stream << "VTSubmarine";           break;
    case 14: stream << "VTLcvp";                break;
    case 16: stream << "VTLevelBomber";         break;
    case 17: stream << "VTArmoredCar";          break;
    case 18: stream << "VTTankHunter";          break;
    case 19: stream << "VTATGun";               break;
    case 20: stream << "VTAA";                  break;
    case 21: stream << "VTBoat";                break;
    case 22: stream << "VTHelicopterAttack";    break;
    case 23: stream << "VTHelicopterTransport"; break;
    case 24: stream << "VTSniper";              break;
    case 25: stream << "VTInfantry";            break;
    default: stream << "VTNoVehicleType";        break;
    }
    return stream;
}

// Reads a case-sensitive prefixed or short vehicle-type token. Unknown and numeric tokens select VTNoVehicleType.
// The reader accepts DefGun, but the writer emits that value as VTNoVehicleType, so it does not round-trip.
std::istream& operator>>(std::istream& stream, VehicleType& type)
{
    std::string token;
    stream >> token;
    if (token == "VTHeavyTank" || token == "HeavyTank")
        type = static_cast<VehicleType>(0);
    else if (token == "VTLightTank" || token == "LightTank")
        type = static_cast<VehicleType>(1);
    else if (token == "VTArtillery" || token == "Artillery")
        type = static_cast<VehicleType>(2);
    else if (token == "VTApc" || token == "Apc")
        type = static_cast<VehicleType>(3);
    else if (token == "VTScoutCar" || token == "ScoutCar")
        type = static_cast<VehicleType>(4);
    else if (token == "VTFighter" || token == "Fighter")
        type = static_cast<VehicleType>(5);
    else if (token == "VTDiveBomber" || token == "DiveBomber")
        type = static_cast<VehicleType>(6);
    else if (token == "VTBomber" || token == "Bomber")
        type = static_cast<VehicleType>(7);
    else if (token == "VTAAGun" || token == "AAGun")
        type = static_cast<VehicleType>(8);
    else if (token == "VTStationaryMG" || token == "StationaryMG")
        type = static_cast<VehicleType>(9);
    else if (token == "VTDestroyer" || token == "Destroyer")
        type = static_cast<VehicleType>(10);
    else if (token == "VTBattleship" || token == "Battleship")
        type = static_cast<VehicleType>(11);
    else if (token == "VTCarrier" || token == "Carrier")
        type = static_cast<VehicleType>(12);
    else if (token == "VTSubmarine" || token == "Submarine")
        type = static_cast<VehicleType>(13);
    else if (token == "VTLcvp" || token == "Lcvp")
        type = static_cast<VehicleType>(14);
    else if (token == "VTDefGun" || token == "DefGun")                    // the writer's hole
        type = static_cast<VehicleType>(15);
    else if (token == "VTLevelBomber" || token == "LevelBomber")
        type = static_cast<VehicleType>(16);
    else if (token == "VTArmoredCar" || token == "ArmoredCar")
        type = static_cast<VehicleType>(17);
    else if (token == "VTTankHunter" || token == "TankHunter")
        type = static_cast<VehicleType>(18);
    else if (token == "VTATGun" || token == "ATGun")
        type = static_cast<VehicleType>(19);
    else if (token == "VTHelicopterAttack" || token == "HelicopterAttack")     // 22 before 21/20
        type = static_cast<VehicleType>(22);
    else if (token == "VTHelicopterTransport" || token == "HelicopterTransport")
        type = static_cast<VehicleType>(23);
    else if (token == "VTBoat" || token == "Boat")
        type = static_cast<VehicleType>(21);
    else if (token == "VTAA" || token == "AA")
        type = static_cast<VehicleType>(20);
    else if (token == "VTSniper" || token == "Sniper")
        type = static_cast<VehicleType>(24);
    else if (token == "VTInfantry" || token == "Infantry")
        type = static_cast<VehicleType>(25);
    else
    {
        if (token != "VTNoVehicleType")
            (void)(token == "NoVehicleType");                                // dead test, shipped
        type = static_cast<VehicleType>(26);
    }
    return stream;
}

// Kit-type conversion accepts the seven standard world kit types. Unknown tokens select 13; values above 6 print nothing.
// The AI kit parser supports additional types and is a separate contract.
std::ostream& operator<<(std::ostream& stream, KitType type)
{
    switch (static_cast<int>(type))
    {
    case 0: stream << "KTSpecops"; break;
    case 1: stream << "KTSniper"; break;
    case 2: stream << "KTAssault"; break;
    case 3: stream << "KTSupport"; break;
    case 4: stream << "KTEngineer"; break;
    case 5: stream << "KTMedic"; break;
    case 6: stream << "KTAT"; break;
    default: break;
    }
    return stream;
}

// One token, compared case-sensitively against the prefixed name, the bare name and the number;
// an unknown token stores 13 without introducing a stream error.
std::istream& operator>>(std::istream& stream, KitType& type)
{
    std::string token;
    stream >> token;
    if (token == "KTSpecops" || token == "Specops" || token == "0")
        type = static_cast<KitType>(0);
    else if (token == "KTSniper" || token == "Sniper" || token == "1")
        type = static_cast<KitType>(1);
    else if (token == "KTAssault" || token == "Assault" || token == "2")
        type = static_cast<KitType>(2);
    else if (token == "KTSupport" || token == "Support" || token == "3")
        type = static_cast<KitType>(3);
    else if (token == "KTEngineer" || token == "Engineer" || token == "4")
        type = static_cast<KitType>(4);
    else if (token == "KTMedic" || token == "Medic" || token == "5")
        type = static_cast<KitType>(5);
    else if (token == "KTAT" || token == "AT" || token == "6")
        type = static_cast<KitType>(6);
    else
        type = static_cast<KitType>(13);
    return stream;
}

// . Out-of-range values emit no token.
std::ostream& operator<<(std::ostream& stream, MaskObjectType type)
{
    switch (static_cast<int>(type))
    {
    case 0: stream << "MOTNightVisionGoggles"; break;
    case 1: stream << "MOTGasMask"; break;
    case 2: stream << "MOTNoMask"; break;
    default: break;
    }
    return stream;
}

// . Case-sensitive names, short names and numeric tokens;
// an unknown token selects NoMask without introducing a stream error.
std::istream& operator>>(std::istream& stream, MaskObjectType& type)
{
    std::string token;
    stream >> token;
    if (token == "MOTNightVisionGoggles" || token == "NightVisionGoggles" || token == "0")
        type = static_cast<MaskObjectType>(0);
    else if (token == "MOTGasMask" || token == "GasMask" || token == "1")
        type = static_cast<MaskObjectType>(1);
    else
    {
        if (token != "MOTNoMask" && token != "NoMask")
            (void)(token == "2");
        type = static_cast<MaskObjectType>(2);
    }
    return stream;
}

// Zero-layout bridge for generated bindings whose retail executors read/write protected template
// fields directly. It is friended by only the six templates represented here; no invented public
// gameplay accessor is added to their ABI.
struct GameTemplateConsoleAccess
{
    static MaskObjectTemplate* maskObject()
    {
        return static_cast<MaskObjectTemplate*>(getActiveTemplate(CID_MaskObjectTemplate));
    }
    static Vec3 getMaskOffset() { return maskObject()->m_maskOffset; }
    static void setMaskOffset(const Vec3& value) { maskObject()->m_maskOffset = value; }

    // GrapplingHookRopeTemplate rope constants. The dedicated PE template ctor writes them at
    // +0x240..+0x294 and each generated ConsoleClass executor stores/loads its float directly
    // (PE gate sub_4BC4A0 = getActiveTemplate(40013)).
    static GrapplingHookRopeTemplate* grapplingHookRope()
    {
        return static_cast<GrapplingHookRopeTemplate*>(
            getActiveTemplate(CID_GrapplingHookRopeTemplate));
    }
    static float getThrowStrength() { return grapplingHookRope()->m_throwStrength; }
    static void setThrowStrength(float v) { grapplingHookRope()->m_throwStrength = v; }
    static float getUpThrowBoost() { return grapplingHookRope()->m_upThrowBoost; }
    static void setUpThrowBoost(float v) { grapplingHookRope()->m_upThrowBoost = v; }
    static float getDegradeThrowStrength() { return grapplingHookRope()->m_degradeThrowStrength; }
    static void setDegradeThrowStrength(float v) { grapplingHookRope()->m_degradeThrowStrength = v; }
    static float getThrowStrength2() { return grapplingHookRope()->m_throwStrength2; }
    static void setThrowStrength2(float v) { grapplingHookRope()->m_throwStrength2 = v; }
    static float getUpThrowBoost2() { return grapplingHookRope()->m_upThrowBoost2; }
    static void setUpThrowBoost2(float v) { grapplingHookRope()->m_upThrowBoost2 = v; }
    static float getDegradeThrowStrength2() { return grapplingHookRope()->m_degradeThrowStrength2; }
    static void setDegradeThrowStrength2(float v) { grapplingHookRope()->m_degradeThrowStrength2 = v; }
    static float getDropStrength() { return grapplingHookRope()->m_dropStrength; }
    static void setDropStrength(float v) { grapplingHookRope()->m_dropStrength = v; }
    static float getUpDropBoost() { return grapplingHookRope()->m_upDropBoost; }
    static void setUpDropBoost(float v) { grapplingHookRope()->m_upDropBoost = v; }
    static float getDegradeDropStrength() { return grapplingHookRope()->m_degradeDropStrength; }
    static void setDegradeDropStrength(float v) { grapplingHookRope()->m_degradeDropStrength = v; }
    static float getLinkAttachMovement() { return grapplingHookRope()->m_linkAttachMovement; }
    static void setLinkAttachMovement(float v) { grapplingHookRope()->m_linkAttachMovement = v; }
    static float getLinkAttachTime() { return grapplingHookRope()->m_linkAttachTime; }
    static void setLinkAttachTime(float v) { grapplingHookRope()->m_linkAttachTime = v; }
    static float getDismountCheckOffset() { return grapplingHookRope()->m_dismountCheckOffset; }
    static void setDismountCheckOffset(float v) { grapplingHookRope()->m_dismountCheckOffset = v; }
    static float getMinYNormal() { return grapplingHookRope()->m_minYNormal; }
    static void setMinYNormal(float v) { grapplingHookRope()->m_minYNormal = v; }
    static float getAttachClimberRadius() { return grapplingHookRope()->m_attachClimberRadius; }
    static void setAttachClimberRadius(float v) { grapplingHookRope()->m_attachClimberRadius = v; }
    static float getClimbingSpeed() { return grapplingHookRope()->m_climbingSpeed; }
    static void setClimbingSpeed(float v) { grapplingHookRope()->m_climbingSpeed = v; }
    static float getElasticity() { return grapplingHookRope()->m_elasticity; }
    static void setElasticity(float v) { grapplingHookRope()->m_elasticity = v; }
    static float getAirFriction() { return grapplingHookRope()->m_airFriction; }
    static void setAirFriction(float v) { grapplingHookRope()->m_airFriction = v; }
    static float getClimbableAngle() { return grapplingHookRope()->m_climbableAngle; }
    static void setClimbableAngle(float v) { grapplingHookRope()->m_climbableAngle = v; }
    static float getDismountAngle() { return grapplingHookRope()->m_dismountAngle; }
    static void setDismountAngle(float v) { grapplingHookRope()->m_dismountAngle = v; }
    static float getClimbersAttachForce() { return grapplingHookRope()->m_climbersAttachForce; }
    static void setClimbersAttachForce(float v) { grapplingHookRope()->m_climbersAttachForce = v; }
    static float getAwakeTime() { return grapplingHookRope()->m_awakeTime; }
    static void setAwakeTime(float v) { grapplingHookRope()->m_awakeTime = v; }

    // DropVehicleTemplate: the console field is the recovered m_destroyVehicleTime (PE +0x210).
    static DropVehicleTemplate* activeDropVehicleTemplate()
    {
        return static_cast<DropVehicleTemplate*>(getActiveTemplate(CID_DropVehicleTemplate));
    }
    static int getNoPassengerTimeToLive() { return activeDropVehicleTemplate()->m_destroyVehicleTime; }
    static void setNoPassengerTimeToLive(int v) { activeDropVehicleTemplate()->m_destroyVehicleTime = v; }
    static PlayerControlObjectTemplate* playerControlObject()
    {
        return static_cast<PlayerControlObjectTemplate*>(
            getActiveTemplate(CID_PlayerControlObjectTemplate));
    }
    static io::PlayerInputMap getSpecialToggleWeaponInput()
    {
        return static_cast<io::PlayerInputMap>(playerControlObject()->m_specialToggleWeaponInput);
    }
    static void setSpecialToggleWeaponInput(io::PlayerInputMap input)
    {
        playerControlObject()->m_specialToggleWeaponInput = input;
    }
    static io::PlayerInputMap getRegulateYawInput()
    { return static_cast<io::PlayerInputMap>(playerControlObject()->m_regulateYawInput); }
    static void setRegulateYawInput(io::PlayerInputMap value)
    { playerControlObject()->m_regulateYawInput = value; }
    static io::PlayerInputMap getRegulatePitchInput()
    { return static_cast<io::PlayerInputMap>(playerControlObject()->m_regulatePitchInput); }
    static void setRegulatePitchInput(io::PlayerInputMap value)
    { playerControlObject()->m_regulatePitchInput = value; }
    static io::PlayerInputMap getRegulateRollInput()
    { return static_cast<io::PlayerInputMap>(playerControlObject()->m_regulateRollInput); }
    static void setRegulateRollInput(io::PlayerInputMap value)
    { playerControlObject()->m_regulateRollInput = value; }
    static io::PlayerInputMap getRegulateVerticalPosInput()
    { return static_cast<io::PlayerInputMap>(playerControlObject()->m_regulateVerticalPosInput); }
    static void setRegulateVerticalPosInput(io::PlayerInputMap value)
    { playerControlObject()->m_regulateVerticalPosInput = value; }
    static Vec2 getRegulateYaw() { return playerControlObject()->m_regulateYaw; }
    static void setRegulateYaw(const Vec2& value) { playerControlObject()->m_regulateYaw = value; }
    static Vec2 getRegulatePitch() { return playerControlObject()->m_regulatePitch; }
    static void setRegulatePitch(const Vec2& value) { playerControlObject()->m_regulatePitch = value; }
    static Vec2 getRegulateRoll() { return playerControlObject()->m_regulateRoll; }
    static void setRegulateRoll(const Vec2& value) { playerControlObject()->m_regulateRoll = value; }
    static Vec2 getRegulateVerticalPos() { return playerControlObject()->m_regulateVerticalPos; }
    static void setRegulateVerticalPos(const Vec2& value)
    { playerControlObject()->m_regulateVerticalPos = value; }
    static float getMaxVertRegAngle() { return playerControlObject()->m_maxVertRegAngle; }
    static void setMaxVertRegAngle(float value) { playerControlObject()->m_maxVertRegAngle = value; }
    static float getNoVertRegAngle() { return playerControlObject()->m_noVertRegAngle; }
    static void setNoVertRegAngle(float value) { playerControlObject()->m_noVertRegAngle = value; }
    static void setSoldierExitLocation(const Vec3& position, const Vec3& rotation)
    {
        playerControlObject()->setSoldierExitLocation(position, rotation);
    }
    static void seatInformation(const std::string& parent, const Vec3& position,
                                const Vec3& rotation)
    {
        playerControlObject()->seatInformation(parent, position, rotation);
    }
    static Vec3 getAltSoldierExitPosition()
    {
        return playerControlObject()->m_altSoldierExitPosition;
    }
    static void setAltSoldierExitPosition(const Vec3& position)
    {
        playerControlObject()->m_altSoldierExitPosition = position;
    }
    static bool getHasRestrictedExit()
    {
        return playerControlObject()->m_hasRestrictedExit;
    }
    static void setHasRestrictedExit(bool restricted)
    {
        playerControlObject()->m_hasRestrictedExit = restricted;
    }
    static float getExitSpeedMod()
    {
        return playerControlObject()->m_positionalSpeedModifier;
    }
    static void setExitSpeedMod(float modifier)
    {
        playerControlObject()->m_positionalSpeedModifier = modifier;
    }
    static int getAutoUseAbility() { return playerControlObject()->m_autoUseAbility ? 1 : 0; }
    static void setAutoUseAbility(int value) { playerControlObject()->m_autoUseAbility = value != 0; }
    // ConsoleClass1043 / 1044 (see the binding block): both executors read and store the template
    // field raw (ELF +0x2BC / +0x2C0, BF2.exe +0x3A0 / +0x3A4); neither image has an accessor.
    static U32 getCockpitLod() { return playerControlObject()->m_cockpitLod; }
    static void setCockpitLod(U32 value) { playerControlObject()->m_cockpitLod = value; }
    static U32 getCockpitSubGeom() { return playerControlObject()->m_cockpitSubGeom; }
    static void setCockpitSubGeom(U32 value) { playerControlObject()->m_cockpitSubGeom = value; }

    // Sprint recovery, dissipation and factor keep only positive values; zero, negative and NaN inputs store positive zero.
    static float getPcoSprintRecoverTime() { return playerControlObject()->m_sprintRecoverTime; }
    static void setPcoSprintRecoverTime(float value)
    {
        PlayerControlObjectTemplate* const pco = playerControlObject();
        pco->m_sprintRecoverTime = (0.0f < value) ? value : 0.0f;
    }
    static float getPcoSprintDissipationTime() { return playerControlObject()->m_sprintDissipationTime; }
    static void setPcoSprintDissipationTime(float value)
    {
        PlayerControlObjectTemplate* const pco = playerControlObject();
        pco->m_sprintDissipationTime = (0.0f < value) ? value : 0.0f;
    }
    // [PLATFORM] sprintLimit clamps to [0, 1]. BF2.exe exec 0x52BECC pushes (v, 0.0f, 1.0f) to the
    // image's one float clamp 0x402EE0 (bf2_w32ded: exec 0x4B7540 -> 0x402500, the same body):
    // `fcomi st(1); fcmovnb` takes the upper bound first when v >= 1, then `fcomi st(2); fcmovb`
    // takes the lower bound when below - and fcomi sets CF for unordered too, so a NaN stores 0.
    // The ELF inlines `v < 0 ? 0 : (v > 1 ? 1 : v)` (0x863A0BC..0x863A0D3: ja / fcmovnbe), which
    // keeps a NaN. Signed zero survives both (-0 is neither below 0 nor above 1). The same split,
    // spelled the same way, as PlayerControlObjectTemplate::setListenerObstruction.
    static float getPcoSprintLimit() { return playerControlObject()->m_sprintLimit; }
    static void setPcoSprintLimit(float value)
    {
        PlayerControlObjectTemplate* const pco = playerControlObject();
#if defined(_WIN32)
        const float capped = value >= 1.0f ? 1.0f : value;
        pco->m_sprintLimit = capped >= 0.0f ? capped : 0.0f;
#else
        pco->m_sprintLimit = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
#endif
    }
    static float getPcoSprintFactor() { return playerControlObject()->m_sprintFactor; }
    static void setPcoSprintFactor(float value)
    {
        PlayerControlObjectTemplate* const pco = playerControlObject();
        pco->m_sprintFactor = (0.0f < value) ? value : 0.0f;
    }

    // Category bindings dispatch through the player-control template interface; integer getters are converted to the console enum.
    static VehicleCategory getPcoVehicleCategory()
    {
        return static_cast<VehicleCategory>(playerControlObject()->getVehicleCategory());
    }
    static void setPcoVehicleCategory(VehicleCategory category)
    {
        playerControlObject()->setVehicleCategory(category);
    }
    static VehicleCategory getPcoControlsCategory()
    {
        return static_cast<VehicleCategory>(playerControlObject()->getControlsCategory());
    }
    static void setPcoControlsCategory(VehicleCategory category)
    {
        playerControlObject()->setControlsCategory(category);
    }
    static void setPcoVehicleType(VehicleType type)
    {
        playerControlObject()->setVehicleType(type);
    }

    // Occupancy and exit-policy fields retail registers as plain ObjectTemplate properties, each
    // gated on CID_PlayerControlObjectTemplate (dedicated PE executors 0x4B78F0 etc. read/write the
    // raw field; disableSpawnPointsOnEnter normalizes its stored byte to 0/1).
    static bool getAllowDucking() { return playerControlObject()->m_allowDucking; }
    static void setAllowDucking(bool value) { playerControlObject()->m_allowDucking = value; }
    static bool getDisableInputWhileDucking()
    {
        return playerControlObject()->m_disableInputWhileDucking;
    }
    static void setDisableInputWhileDucking(bool value)
    {
        playerControlObject()->m_disableInputWhileDucking = value;
    }
    static bool getDontClearTeamOnExit() { return playerControlObject()->m_dontClearTeamOnExit; }
    static void setDontClearTeamOnExit(bool value)
    {
        playerControlObject()->m_dontClearTeamOnExit = value;
    }
    static bool getDestroyOnExit() { return playerControlObject()->m_destroyOnExit; }
    static void setDestroyOnExit(bool value) { playerControlObject()->m_destroyOnExit = value; }
    static bool getDontAllowExit() { return playerControlObject()->m_dontAllowExit; }
    static void setDontAllowExit(bool value) { playerControlObject()->m_dontAllowExit = value; }
    static bool getDisableSpawnPointsOnEnter()
    {
        return playerControlObject()->m_disableSpawnPointsOnEnter;
    }
    static void setDisableSpawnPointsOnEnter(bool value)
    {
        playerControlObject()->m_disableSpawnPointsOnEnter = value;
    }
    static bool getToggleWeapon() { return playerControlObject()->m_toggleWeapon; }
    static void setToggleWeapon(bool value) { playerControlObject()->m_toggleWeapon = value; }
    static bool getIsOpenVehicle() { return playerControlObject()->m_isOpenVehicle; }
    static void setIsOpenVehicle(bool value) { playerControlObject()->m_isOpenVehicle = value; }
    static int getCrewKitIndex() { return playerControlObject()->m_crewKitIndex; }
    static void setCrewKitIndex(int value) { playerControlObject()->m_crewKitIndex = value; }
    static float getBoundingRadiusModifierWhenOccupied()
    {
        return playerControlObject()->m_boundingRadiusModifierWhenOccupied;
    }
    static void setBoundingRadiusModifierWhenOccupied(float value)
    {
        playerControlObject()->m_boundingRadiusModifierWhenOccupied = value;
    }
    // addTargetObjectTypeToWatch appends to the watched-type set; duplicates collapse.
    static void addTargetObjectTypeToWatch(int type)
    {
        playerControlObject()->m_targetObjectTypesToWatch.insert(type);
    }
    // Seat name and rotation limits dispatch through the template's own accessors, matching retail's
    // virtual seat-component set/get pair (dedicated PE executors 0x4B27B0 / 0x4B34A0 / 0x4B3670).
    static std::string getSeatParent() { return playerControlObject()->getSeatParent(); }
    static void setSeatParent(const std::string& value) { playerControlObject()->setSeatParent(value); }
    static float getSeatLeftRotationLimit()
    {
        return playerControlObject()->getSeatLeftRotationLimit();
    }
    static void setSeatLeftRotationLimit(float value)
    {
        playerControlObject()->setSeatLeftRotationLimit(value);
    }
    static float getSeatRightRotationLimit()
    {
        return playerControlObject()->getSeatRightRotationLimit();
    }
    static void setSeatRightRotationLimit(float value)
    {
        playerControlObject()->setSeatRightRotationLimit(value);
    }

    static SoldierTemplate* soldier()
    {
        return static_cast<SoldierTemplate*>(getActiveTemplate(CID_SoldierTemplate));
    }
    static void setSoldierGeometry1P(const std::string& value)
    {
        soldier()->m_geometry1P = value;
    }
    static void setSoldierGeometry3P(const std::string& value)
    {
        soldier()->m_geometry3P = value;
    }
    static float getSoldierSprintRecoverTime() { return soldier()->m_sprintRecoverTime; }
    static void setSoldierSprintRecoverTime(float value) { soldier()->m_sprintRecoverTime = value; }
    static float getSoldierSprintDissipationTime() { return soldier()->m_sprintDissipationTime; }
    static void setSoldierSprintDissipationTime(float value)
    {
        soldier()->m_sprintDissipationTime = value;
    }
    static float getSoldierSprintLimit() { return soldier()->m_sprintLimit; }
    static void setSoldierSprintLimit(float value) { soldier()->m_sprintLimit = value; }
    static float getSoldierSprintLossAtJump() { return soldier()->m_sprintLossAtJump; }
    static void setSoldierSprintLossAtJump(float value) { soldier()->m_sprintLossAtJump = value; }
    static float getSoldierGasMaskSprintFactor() { return soldier()->m_gasMaskSprintFactor; }
    static void setSoldierGasMaskSprintFactor(float value) { soldier()->m_gasMaskSprintFactor = value; }
    static float getSoldierRagdollLandedDelay() { return soldier()->m_ragdollLandedDelay; }
    static void setSoldierRagdollLandedDelay(float value) { soldier()->m_ragdollLandedDelay = value; }

    static SupplyObjectTemplate* supply()
    {
        return static_cast<SupplyObjectTemplate*>(getActiveTemplate(CID_SupplyObjectTemplate));
    }
    static float getSupplyRadius() { return supply()->radius; }
    static void setSupplyRadius(float value) { supply()->radius = value; }
    static int getSupplyTeam() { return supply()->team; }
    static void setSupplyTeam(int value) { supply()->team = value; }
    static bool getWorkOnVehicles() { return supply()->workOnVehicles; }
    static void setWorkOnVehicles(bool value) { supply()->workOnVehicles = value; }
    static bool getWorkOnSoldiers() { return supply()->workOnSoldiers; }
    static void setWorkOnSoldiers(bool value) { supply()->workOnSoldiers = value; }
    static float getHealSpeed() { return supply()->healSpeed; }
    static void setHealSpeed(float value) { supply()->healSpeed = value; }
    static float getDamageSpeed() { return supply()->damageSpeed; }
    static void setDamageSpeed(float value) { supply()->damageSpeed = value; }
    static float getRefillAmmoSpeed() { return supply()->refillAmmoSpeed; }
    static void setRefillAmmoSpeed(float value) { supply()->refillAmmoSpeed = value; }
    static float getRestoreSpeed() { return supply()->restoreSpeed; }
    static void setRestoreSpeed(float value) { supply()->restoreSpeed = value; }
    static float getHealStorageSize() { return supply()->healStorageSize; }
    static void setHealStorageSize(float value) { supply()->healStorageSize = value; }
    static float getAmmoStorageSize() { return supply()->ammoStorageSize; }
    static void setAmmoStorageSize(float value) { supply()->ammoStorageSize = value; }
    static float getSharedStorageSize() { return supply()->sharedStorageSize; }
    static void setSharedStorageSize(float value) { supply()->sharedStorageSize = value; }
    static bool getDestroyWhenEmptyStorage() { return supply()->destroyWhenEmptyStorage; }
    static void setDestroyWhenEmptyStorage(bool value) { supply()->destroyWhenEmptyStorage = value; }
    static bool getTeamFromClosestCP() { return supply()->teamFromClosestCP; }
    static void setTeamFromClosestCP(bool value) { supply()->teamFromClosestCP = value; }
    static bool getVisibleOn3dMap() { return supply()->visibleOn3dMap; }
    static void setVisibleOn3dMap(bool value) { supply()->visibleOn3dMap = value; }
    static int get3dMapIcon() { return supply()->mapIcon3d; }
    static void set3dMapIcon(int value) { supply()->mapIcon3d = value; }
    static void addVehicleType(VehicleCategory category)
    {
        supply()->vehicleTypes.push_back(static_cast<int>(category));
    }

    static TargetObjectTemplate* target()
    {
        return static_cast<TargetObjectTemplate*>(getActiveTemplate(CID_TargetObjectTemplate));
    }
    static TargetType getTargetType() { return static_cast<TargetType>(target()->targetType); }
    static void setTargetType(TargetType value) { target()->targetType = static_cast<int>(value); }
    static float getTargetTimeToLive() { return target()->timeToLive; }
    static void setTargetTimeToLive(float value) { target()->setTimeToLive(value); }
    static bool getDisableWhenEmptyVehicle() { return target()->disableWhenEmptyVehicle; }
    static void setDisableWhenEmptyVehicle(bool value) { target()->disableWhenEmptyVehicle = value; }
    static float getDisableWhenEmptyVehicleDelay() { return target()->disableWhenEmptyVehicleDelay; }
    static void setDisableWhenEmptyVehicleDelay(float value)
    {
        target()->disableWhenEmptyVehicleDelay = value;
    }
    static bool getDisableWhenWreck() { return target()->disableWhenWreck; }
    static void setDisableWhenWreck(bool value) { target()->disableWhenWreck = value; }

    static RemoteControlledObjectTemplate* remote()
    {
        return static_cast<RemoteControlledObjectTemplate*>(
            getActiveTemplate(CID_RemoteControlledObjectTemplate));
    }
    static RemoteControlledObjectType getRemoteType()
    {
        return static_cast<RemoteControlledObjectType>(remote()->rcType);
    }
    static void setRemoteType(RemoteControlledObjectType value)
    {
        remote()->rcType = static_cast<int>(value);
    }
    static float getMinTimeBetweenActivation() { return remote()->minTimeBetweenActivation; }
    static void setMinTimeBetweenActivation(float value) { remote()->minTimeBetweenActivation = value; }
    static std::string getUavVehicleTemplate() { return remote()->uavVehicleTemplate; }
    static void setUavVehicleTemplate(const std::string& value) { remote()->uavVehicleTemplate = value; }
    static float getUavVehicleSpeed() { return remote()->uavVehicleSpeed; }
    static void setUavVehicleSpeed(float value) { remote()->uavVehicleSpeed = value; }
    static float getUavVehicleRadius() { return remote()->uavVehicleRadius; }
    static void setUavVehicleRadius(float value) { remote()->uavVehicleRadius = value; }
    static float getUavVehicleFlightHeight() { return remote()->uavVehicleFlightHeight; }
    static void setUavVehicleFlightHeight(float value) { remote()->uavVehicleFlightHeight = value; }
    static float getUavVehicleSampleResolution() { return remote()->uavVehicleSampleResolution; }
    static void setUavVehicleSampleResolution(float value)
    {
        remote()->uavVehicleSampleResolution = value;
    }

    static TurnableRemoteControlledObjectTemplate* turnable()
    {
        return static_cast<TurnableRemoteControlledObjectTemplate*>(
            getActiveTemplate(CID_TurnableRemoteControlledObjectTemplate));
    }
    static float getTurnVelocity() { return turnable()->turnVelocity; }
    static void setTurnVelocity(float value) { turnable()->turnVelocity = value; }

    // Kit console field access.
    static KitTemplate* kit()
    {
        return static_cast<KitTemplate*>(getActiveTemplate(CID_KitTemplate));
    }
    static int   getKitGeometry() { return kit()->m_defaultGeomIndex; }              // +408
    static void  setKitGeometry(int v) { kit()->m_defaultGeomIndex = v; }
    static float getKitTimeToLiveAfterDeath() { return kit()->m_timeToLiveAfterDeath; }   // +412
    static void  setKitTimeToLiveAfterDeath(float v) { kit()->m_timeToLiveAfterDeath = v; }
    // ConsoleClass952 kitType goes through the template's own pair - getKitType 0x8186420 and
    // setKitType 0x8186400 (its executor 0x86211E0 calls both by name); the field is +416.
    static KitType getKitType() { return static_cast<KitType>(kit()->getKitType()); }
    static void    setKitType(KitType v) { kit()->setKitType(static_cast<int>(v)); }
    static int   getKitTeam() { return kit()->m_kitTeam; }                           // +420
    static void  setKitTeam(int v) { kit()->m_kitTeam = v; }
    static int   getKitUnlockIndex() { return kit()->m_creationIndex; }              // +424
    static void  setKitUnlockIndex(int v) { kit()->m_creationIndex = v; }
    static int   getKitDefaultWeaponIndex() { return kit()->m_defaultWeaponIndex; }  // +428
    static void  setKitDefaultWeaponIndex(int v) { kit()->m_defaultWeaponIndex = v; }
    static int   getKitDropGeom() { return kit()->m_dropGeom; }                      // +432
    static void  setKitDropGeom(int v) { kit()->m_dropGeom = v; }
    static const std::string& getKitDropHeadwear() { return kit()->m_dropHeadwear; } // +436, get raw
    static void  setKitDropHeadwear(const std::string& v) { kit()->setDropHeadwear(v); }
    static float getKitSprintStaminaDissipationFactor()                              // +440
        { return kit()->m_sprintStaminaDissipationFactor; }
    static void  setKitSprintStaminaDissipationFactor(float v)
        { kit()->m_sprintStaminaDissipationFactor = v; }
    // GET reads +456 raw; SET goes through the accessor because it also writes the /30 companion
    // at +460 - and aborts at 0.2 or more. See KitTemplate.cpp.
    static float getKitAbilityRestoreRate() { return kit()->m_abilityRestoreRate; }  // +456
    static void  setKitAbilityRestoreRate(float v) { kit()->setAbilityRestoreRate(v); }
    static int   getKitAbilityInVehicleMaterial() { return kit()->m_abilityInVehicleMaterial; } // +464
    static void  setKitAbilityInVehicleMaterial(int v) { kit()->m_abilityInVehicleMaterial = v; }
    static float getKitAbilityInVehicleStrength() { return kit()->m_abilityInVehicleStrength; } // +468
    static void  setKitAbilityInVehicleStrength(float v) { kit()->m_abilityInVehicleStrength = v; }
    static float getKitAbilityInVehicleRadius() { return kit()->m_abilityInVehicleRadius; }     // +472
    static void  setKitAbilityInVehicleRadius(float v) { kit()->m_abilityInVehicleRadius = v; }
    static void  addKitVehicleMod(std::string name, int mod) { kit()->addKitVehicleMod(name, mod); }

    // Typed access to destroyable-object settings.
    static DestroyableObjectTemplate* destroyable()
    {
        return static_cast<DestroyableObjectTemplate*>(
            getActiveTemplate(CID_DestroyableObjectTemplate));
    }
    static bool  getUsedByAi() { return destroyable()->usedByAi; }                   // +0x188
    static void  setUsedByAi(bool v) { destroyable()->usedByAi = v; }
    static bool  getInitiallyWalkable() { return destroyable()->initiallyWalkable; } // +0x189
    static void  setInitiallyWalkable(bool v) { destroyable()->initiallyWalkable = v; }
    // minCorner/maxCorner are float[3] on the class, not Vec3 - the console metadata says Vec3 and
    // the executor moves three dwords, which is the same thing laid out differently.
    static Vec3  getMinCorner()
    {
        const float* c = destroyable()->minCorner;
        Vec3 out;
        out.x = c[0]; out.y = c[1]; out.z = c[2];
        return out;
    }
    static void  setMinCorner(const Vec3& v)
    {
        float* c = destroyable()->minCorner;
        c[0] = v.x; c[1] = v.y; c[2] = v.z;
    }
    static Vec3  getMaxCorner()
    {
        const float* c = destroyable()->maxCorner;
        Vec3 out;
        out.x = c[0]; out.y = c[1]; out.z = c[2];
        return out;
    }
    static void  setMaxCorner(const Vec3& v)
    {
        float* c = destroyable()->maxCorner;
        c[0] = v.x; c[1] = v.y; c[2] = v.z;
    }

    // Antenna console fields, including the two-component sway-angle limit.
    static AntennaObjectTemplate* antenna()
    {
        return static_cast<AntennaObjectTemplate*>(getActiveTemplate(CID_AntennaObjectTemplate));
    }
    static Vec2  getAntennaMaxSwayAngle()                                            // +0x16C
    {
        const float* a = antenna()->maxSwayAngle;
        Vec2 out;
        out.x = a[0]; out.y = a[1];
        return out;
    }
    static void  setAntennaMaxSwayAngle(const Vec2& v)
    {
        float* a = antenna()->maxSwayAngle;
        a[0] = v.x; a[1] = v.y;
    }
    static float getAntennaStiffness() { return antenna()->stiffness; }              // +0x174
    static void  setAntennaStiffness(float v) { antenna()->stiffness = v; }
    static float getAntennaDamping() { return antenna()->damping; }                  // +0x178
    static void  setAntennaDamping(float v) { antenna()->damping = v; }
    static float getAntennaAccelerationFactor() { return antenna()->accelerationFactor; }  // +0x17C
    static void  setAntennaAccelerationFactor(float v) { antenna()->accelerationFactor = v; }
    static float getAntennaSpeedFactor() { return antenna()->speedFactor; }          // +0x180
    static void  setAntennaSpeedFactor(float v) { antenna()->speedFactor = v; }

    // ItemContainerTemplate
    static ItemContainerTemplate* itemContainer()
    {
        return static_cast<ItemContainerTemplate*>(getActiveTemplate(CID_ItemContainerTemplate));
    }
    static int   getUnlockLevel() { return itemContainer()->m_unlockLevel; }         // +400
    static void  setUnlockLevel(int v) { itemContainer()->m_unlockLevel = v; }
    static void  replaceItem(const std::string& item) { itemContainer()->replaceItem(item); }
};

namespace {

// Shared registration tag for game-template and weapon console bindings; matched by string value.
const char* const kObjTemplGameConsoleTag = "world.objTemplGame";

// The six Trigger bindings narrow the cursor the same way (ConsoleClass1198's isObjectActive,
// ): the active template, but only if it IS-A TriggerTemplate.
TriggerTemplate* activeTrigger()
{
    return static_cast<TriggerTemplate*>(getActiveTemplate(CID_TriggerTemplate));
}

// chance SET (ConsoleClass1200 exec 0x864EA40-ish): the ONE of the six that goes through an
// accessor - TriggerTemplate::setChance clamps to [0, 1]. GET reads the field raw (+0x18C).
void objTemplTriggerChanceSet(float v)
{
    activeTrigger()->setChance(v);
}

} // namespace

// The game-object template block - SetConsoleClassDefaults("ObjectTemplate", 2, 9) at 0x8586958.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 9)

// ConsoleClass968/969: ELF exec 0x8624DA0/0x86252F0, PE 0x4AF020/0x4AF2C0.
// maskType calls the typed getter/setter; maskOffset reads/writes Vec3 directly, without a clamp.
BF2_CONSOLE_PROPERTY(maskType, hfe::MaskObjectType, GameTemplateConsoleAccess::maskObject(),
                     setMaskType, getMaskType, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(maskOffset, Vec3, getActiveTemplate(CID_MaskObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getMaskOffset(),
                        GameTemplateConsoleAccess::setMaskOffset, kObjTemplGameConsoleTag)

// Commander gameplay templates. All four groups self-register into this TU's retail list
// (ELF 0x8AD54F8, static initializer _522); each generated isObjectActive narrows the current
// template by the CID named below. The executors are direct field reads/writes except
// TargetObject.timeToLive, whose SET calls setTimeToLive so its cached 30-Hz tick count stays in
// sync, and SupplyObject.addVehicleType, whose method appends without de-duplication.
// SupplyObjectTemplate: PE active gate 0x4B1290. `3dMapIcon` is retail's literal spelling.
BF2_CONSOLE_PROPERTY_FN(radius, float, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getSupplyRadius(),
                        GameTemplateConsoleAccess::setSupplyRadius, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(team, int, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getSupplyTeam(),
                        GameTemplateConsoleAccess::setSupplyTeam, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(workOnVehicles, bool, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getWorkOnVehicles(),
                        GameTemplateConsoleAccess::setWorkOnVehicles, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(workOnSoldiers, bool, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getWorkOnSoldiers(),
                        GameTemplateConsoleAccess::setWorkOnSoldiers, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(healSpeed, float, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getHealSpeed(),
                        GameTemplateConsoleAccess::setHealSpeed, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(damageSpeed, float, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDamageSpeed(),
                        GameTemplateConsoleAccess::setDamageSpeed, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(refillAmmoSpeed, float, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getRefillAmmoSpeed(),
                        GameTemplateConsoleAccess::setRefillAmmoSpeed, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(restoreSpeed, float, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getRestoreSpeed(),
                        GameTemplateConsoleAccess::setRestoreSpeed, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(healStorageSize, float, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getHealStorageSize(),
                        GameTemplateConsoleAccess::setHealStorageSize, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(ammoStorageSize, float, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getAmmoStorageSize(),
                        GameTemplateConsoleAccess::setAmmoStorageSize, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sharedStorageSize, float, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getSharedStorageSize(),
                        GameTemplateConsoleAccess::setSharedStorageSize, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(destroyWhenEmptyStorage, bool,
                        getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDestroyWhenEmptyStorage(),
                        GameTemplateConsoleAccess::setDestroyWhenEmptyStorage,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(teamFromClosestCP, bool, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getTeamFromClosestCP(),
                        GameTemplateConsoleAccess::setTeamFromClosestCP, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(visibleOn3dMap, bool, getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getVisibleOn3dMap(),
                        GameTemplateConsoleAccess::setVisibleOn3dMap, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("3dMapIcon", int,
                              getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                              GameTemplateConsoleAccess::get3dMapIcon(),
                              GameTemplateConsoleAccess::set3dMapIcon,
                              kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(addVehicleType, GameTemplateConsoleAccess::addVehicleType,
                            getActiveTemplate(CID_SupplyObjectTemplate) != 0,
                            VehicleCategory, kObjTemplGameConsoleTag)

// Stock vehicle seats opt into passive kit abilities with an integer normalized to a Boolean.
BF2_CONSOLE_PROPERTY_FN(autoUseAbility, int,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getAutoUseAbility(),
                        GameTemplateConsoleAccess::setAutoUseAbility, kObjTemplGameConsoleTag)

// Vehicle exit position, orientation, restrictions and inherited-speed settings.
BF2_CONSOLE_PROPERTY_FN(hasRestrictedExit, bool,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getHasRestrictedExit(),
                        GameTemplateConsoleAccess::setHasRestrictedExit, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(altSoldierExitPosition, Vec3,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getAltSoldierExitPosition(),
                        GameTemplateConsoleAccess::setAltSoldierExitPosition,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(exitSpeedMod, float,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getExitSpeedMod(),
                        GameTemplateConsoleAccess::setExitSpeedMod, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(setSoldierExitLocation,
                            GameTemplateConsoleAccess::setSoldierExitLocation,
                            getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                            Vec3, Vec3, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(soldierExitPosition, Vec3,
                     GameTemplateConsoleAccess::playerControlObject(),
                     setSoldierExitPosition, getSoldierExitPosition, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(soldierExitRotation, Vec3,
                     GameTemplateConsoleAccess::playerControlObject(),
                     setSoldierExitRotation, getSoldierExitRotation, kObjTemplGameConsoleTag)

// Occupant entry/exit policy, ducking and seat setup. Retail registers each as a plain ObjectTemplate
// property (or, for addTargetObjectTypeToWatch, a one-int method) gated on CID_PlayerControlObjectTemplate.
BF2_CONSOLE_PROPERTY_FN(allowDucking, bool,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getAllowDucking(),
                        GameTemplateConsoleAccess::setAllowDucking, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(disableInputWhileDucking, bool,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDisableInputWhileDucking(),
                        GameTemplateConsoleAccess::setDisableInputWhileDucking,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(dontClearTeamOnExit, bool,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDontClearTeamOnExit(),
                        GameTemplateConsoleAccess::setDontClearTeamOnExit, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(destroyOnExit, bool,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDestroyOnExit(),
                        GameTemplateConsoleAccess::setDestroyOnExit, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(dontAllowExit, bool,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDontAllowExit(),
                        GameTemplateConsoleAccess::setDontAllowExit, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(disableSpawnPointsOnEnter, bool,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDisableSpawnPointsOnEnter(),
                        GameTemplateConsoleAccess::setDisableSpawnPointsOnEnter,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(toggleWeapon, bool,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getToggleWeapon(),
                        GameTemplateConsoleAccess::setToggleWeapon, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(isOpenVehicle, bool,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getIsOpenVehicle(),
                        GameTemplateConsoleAccess::setIsOpenVehicle, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(crewKitIndex, int,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getCrewKitIndex(),
                        GameTemplateConsoleAccess::setCrewKitIndex, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(boundingRadiusModifierWhenOccupied, float,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getBoundingRadiusModifierWhenOccupied(),
                        GameTemplateConsoleAccess::setBoundingRadiusModifierWhenOccupied,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(addTargetObjectTypeToWatch,
                            GameTemplateConsoleAccess::addTargetObjectTypeToWatch,
                            getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                            int, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(seatParent, std::string,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getSeatParent(),
                        GameTemplateConsoleAccess::setSeatParent, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(seatLeftRotationLimit, float,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getSeatLeftRotationLimit(),
                        GameTemplateConsoleAccess::setSeatLeftRotationLimit, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(seatRightRotationLimit, float,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getSeatRightRotationLimit(),
                        GameTemplateConsoleAccess::setSeatRightRotationLimit, kObjTemplGameConsoleTag)

// Grappling-rope simulation constants. Retail registers each as a float->float ObjectTemplate
// property gated on the active GrapplingHookRopeTemplate (CID 40013; PE gate 0x4BC4A0, executor
// field writes at PE +0x244..+0x290 and +0x294). The values drive the recovered rope simulation.
BF2_CONSOLE_PROPERTY_FN(throwStrength, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getThrowStrength(),
                        GameTemplateConsoleAccess::setThrowStrength, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(upThrowBoost, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getUpThrowBoost(),
                        GameTemplateConsoleAccess::setUpThrowBoost, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(degradeThrowStrength, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getDegradeThrowStrength(),
                        GameTemplateConsoleAccess::setDegradeThrowStrength, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(throwStrength2, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getThrowStrength2(),
                        GameTemplateConsoleAccess::setThrowStrength2, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(upThrowBoost2, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getUpThrowBoost2(),
                        GameTemplateConsoleAccess::setUpThrowBoost2, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(degradeThrowStrength2, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getDegradeThrowStrength2(),
                        GameTemplateConsoleAccess::setDegradeThrowStrength2, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(dropStrength, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getDropStrength(),
                        GameTemplateConsoleAccess::setDropStrength, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(upDropBoost, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getUpDropBoost(),
                        GameTemplateConsoleAccess::setUpDropBoost, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(degradeDropStrength, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getDegradeDropStrength(),
                        GameTemplateConsoleAccess::setDegradeDropStrength, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(linkAttachMovement, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getLinkAttachMovement(),
                        GameTemplateConsoleAccess::setLinkAttachMovement, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(linkAttachTime, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getLinkAttachTime(),
                        GameTemplateConsoleAccess::setLinkAttachTime, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(dismountCheckOffset, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getDismountCheckOffset(),
                        GameTemplateConsoleAccess::setDismountCheckOffset, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(minYNormal, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getMinYNormal(),
                        GameTemplateConsoleAccess::setMinYNormal, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(attachClimberRadius, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getAttachClimberRadius(),
                        GameTemplateConsoleAccess::setAttachClimberRadius, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(climbingSpeed, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getClimbingSpeed(),
                        GameTemplateConsoleAccess::setClimbingSpeed, kObjTemplGameConsoleTag)
// Phase-1 deferred command, same rope block (PE +0x280).
BF2_CONSOLE_PROPERTY_FN(elasticity, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getElasticity(),
                        GameTemplateConsoleAccess::setElasticity, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(airFriction, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getAirFriction(),
                        GameTemplateConsoleAccess::setAirFriction, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(climbableAngle, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getClimbableAngle(),
                        GameTemplateConsoleAccess::setClimbableAngle, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(dismountAngle, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getDismountAngle(),
                        GameTemplateConsoleAccess::setDismountAngle, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(climbersAttachForce, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getClimbersAttachForce(),
                        GameTemplateConsoleAccess::setClimbersAttachForce, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(awakeTime, float,
                        getActiveTemplate(CID_GrapplingHookRopeTemplate) != 0,
                        GameTemplateConsoleAccess::getAwakeTime(),
                        GameTemplateConsoleAccess::setAwakeTime, kObjTemplGameConsoleTag)

// DropVehicleTemplate: PE gate sub_4BE080 = getActiveTemplate(40020); the int executor stores the
// recovered m_destroyVehicleTime at PE +0x210 (int -> int).
BF2_CONSOLE_PROPERTY_FN(noPassengerTimeToLive, int,
                        getActiveTemplate(CID_DropVehicleTemplate) != 0,
                        GameTemplateConsoleAccess::getNoPassengerTimeToLive(),
                        GameTemplateConsoleAccess::setNoPassengerTimeToLive, kObjTemplGameConsoleTag)

// Ground-effect and destroyable-window bundle fields exposed as plain ObjectTemplate properties.
namespace {
GroundEffectBundleTemplate* activeGroundEffectBundle()
{
    return static_cast<GroundEffectBundleTemplate*>(
        getActiveTemplate(CID_GroundEffectBundleTemplate));
}
DestroyableWindowsBundleTemplate* activeDestroyableWindowsBundle()
{
    return static_cast<DestroyableWindowsBundleTemplate*>(
        getActiveTemplate(CID_DestroyableWindowsBundleTemplate));
}
// PE executor 0x4AAB70 writes the window count; the value is the int at template +568
// (openBF2 DestroyableWindowsBundleTemplate::numWindows at +0x188).
void objectTemplateSetNumWindows(int count)
{
    activeDestroyableWindowsBundle()->numWindows = count;
}
} // namespace
BF2_CONSOLE_FIELD(maxTriggerDistance, float, activeGroundEffectBundle(),
                  m_maxTriggerDistance, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(clientHitDetection, bool, activeDestroyableWindowsBundle(),
                  clientHitDetection, kObjTemplGameConsoleTag)
// objectTemplate.setNumWindows - dedicated PE gate 0x4AAAF0 =
// `getActiveTemplate(0xC4DC = CID_DestroyableWindowsBundleTemplate) != 0`; ctor 0x4AAA00,
// executor 0x4AAB70, one int argument, void, file-static. Set-only: the shipped executor's only
// arm is argc 1.
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setNumWindows, objectTemplateSetNumWindows,
                                   activeDestroyableWindowsBundle() != 0, int,
                                   kObjTemplGameConsoleTag)

// Stock parachutes preserve the soldier's camera pitch when entering and leaving the seat.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 15)
BF2_CONSOLE_PROPERTY(maintainCameraOnExit, bool,
                     GameTemplateConsoleAccess::playerControlObject(),
                     setMaintainCameraOnExit, getMaintainCameraOnExit, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(maintainCameraOnEnter, bool,
                     GameTemplateConsoleAccess::playerControlObject(),
                     setMaintainCameraOnEnter, getMaintainCameraOnEnter, kObjTemplGameConsoleTag)
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 9)

// ConsoleClass1020/1021: ELF executors 0x862E9F0/0x862F1D0 and folded PE 0x4B3150
// accept exactly (std::string, Vec3, Vec3), gate on CID_PlayerControlObjectTemplate, and
// dispatch seatInformation. The duplicate retail converters have the same argument types.
// Stock parachutes name themselves as the seat parent; without this registration enter()
// instead takes the no-seat-parent fallback and sets the carried soldier's disabled flag.
BF2_CONSOLE_METHOD_3(seatInformation, GameTemplateConsoleAccess::seatInformation,
                     getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                     std::string, Vec3, Vec3, kObjTemplGameConsoleTag)

// Vehicle cockpit LOD and geometry settings used by the local occupant's inside camera.
BF2_CONSOLE_PROPERTY_FN(cockpitLod, U32,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getCockpitLod(),
                        GameTemplateConsoleAccess::setCockpitLod, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cockpitSubGeom, U32,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getCockpitSubGeom(),
                        GameTemplateConsoleAccess::setCockpitSubGeom, kObjTemplGameConsoleTag)

// Vehicle sprint and afterburner properties use range-checked setters and raw getters.
// Soldier bindings with the same names are separate plain stores. Category metadata currently differs from retail.
BF2_CONSOLE_PROPERTY_FN(sprintRecoverTime, float,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getPcoSprintRecoverTime(),
                        GameTemplateConsoleAccess::setPcoSprintRecoverTime,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sprintDissipationTime, float,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getPcoSprintDissipationTime(),
                        GameTemplateConsoleAccess::setPcoSprintDissipationTime,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sprintLimit, float,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getPcoSprintLimit(),
                        GameTemplateConsoleAccess::setPcoSprintLimit, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sprintFactor, float,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getPcoSprintFactor(),
                        GameTemplateConsoleAccess::setPcoSprintFactor, kObjTemplGameConsoleTag)

// ConsoleClass1077: ELF exec 0x863B4D0 reads/writes template +0x2EC; PE 0x4B7C80
// does the same at +0x3E8. Both advertise PlayerInputMap range 0..64. Stock attack-helicopter
// gunner seats bind PIAltFire here; leaving the constructor's PINone rejects their weapon toggle.
BF2_CONSOLE_PROPERTY_FN_RANGED(specialToggleWeaponInput, io::PlayerInputMap,
                               getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                               GameTemplateConsoleAccess::getSpecialToggleWeaponInput(),
                               GameTemplateConsoleAccess::setSpecialToggleWeaponInput,
                               0, 64, kObjTemplGameConsoleTag)

// Stock vehicle scripts configure stabilization through these live template properties.
// Input mappings include PINone; gain pairs and attitude thresholds are stored without clamping.
BF2_CONSOLE_PROPERTY_FN_RANGED(regulateYawInput, io::PlayerInputMap,
                               getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                               GameTemplateConsoleAccess::getRegulateYawInput(),
                               GameTemplateConsoleAccess::setRegulateYawInput,
                               0, 64, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED(regulatePitchInput, io::PlayerInputMap,
                               getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                               GameTemplateConsoleAccess::getRegulatePitchInput(),
                               GameTemplateConsoleAccess::setRegulatePitchInput,
                               0, 64, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED(regulateRollInput, io::PlayerInputMap,
                               getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                               GameTemplateConsoleAccess::getRegulateRollInput(),
                               GameTemplateConsoleAccess::setRegulateRollInput,
                               0, 64, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED(regulateVerticalPosInput, io::PlayerInputMap,
                               getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                               GameTemplateConsoleAccess::getRegulateVerticalPosInput(),
                               GameTemplateConsoleAccess::setRegulateVerticalPosInput,
                               0, 64, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(regulateYaw, Vec2, getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getRegulateYaw(),
                        GameTemplateConsoleAccess::setRegulateYaw, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(regulatePitch, Vec2, getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getRegulatePitch(),
                        GameTemplateConsoleAccess::setRegulatePitch, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(regulateRoll, Vec2, getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getRegulateRoll(),
                        GameTemplateConsoleAccess::setRegulateRoll, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(regulateVerticalPos, Vec2,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getRegulateVerticalPos(),
                        GameTemplateConsoleAccess::setRegulateVerticalPos, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(maxVertRegAngle, float, getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getMaxVertRegAngle(),
                        GameTemplateConsoleAccess::setMaxVertRegAngle, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(noVertRegAngle, float, getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getNoVertRegAngle(),
                        GameTemplateConsoleAccess::setNoVertRegAngle, kObjTemplGameConsoleTag)

// TargetObjectTemplate: PE active gate 0x4BCA00. The TargetType metadata range really is 0..4;
// value 4 is accepted even though the named TargetType writer stops at TTUnique (3).
BF2_CONSOLE_PROPERTY_FN_RANGED(targetType, TargetType,
                               getActiveTemplate(CID_TargetObjectTemplate) != 0,
                               GameTemplateConsoleAccess::getTargetType(),
                               GameTemplateConsoleAccess::setTargetType,
                               0, 4, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(timeToLive, float, getActiveTemplate(CID_TargetObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getTargetTimeToLive(),
                        GameTemplateConsoleAccess::setTargetTimeToLive, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(disableWhenEmptyVehicle, bool,
                        getActiveTemplate(CID_TargetObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDisableWhenEmptyVehicle(),
                        GameTemplateConsoleAccess::setDisableWhenEmptyVehicle,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(disableWhenEmptyVehicleDelay, float,
                        getActiveTemplate(CID_TargetObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDisableWhenEmptyVehicleDelay(),
                        GameTemplateConsoleAccess::setDisableWhenEmptyVehicleDelay,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(disableWhenWreck, bool,
                        getActiveTemplate(CID_TargetObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getDisableWhenWreck(),
                        GameTemplateConsoleAccess::setDisableWhenWreck,
                        kObjTemplGameConsoleTag)

// RemoteControlledObjectTemplate: PE active gate 0x4BD310; turnVelocity is the derived template's
// own block at 0x4BE250. rcType advertises retail's seemingly odd inclusive 0..6 range.
BF2_CONSOLE_PROPERTY_FN_RANGED(rcType, RemoteControlledObjectType,
                               getActiveTemplate(CID_RemoteControlledObjectTemplate) != 0,
                               GameTemplateConsoleAccess::getRemoteType(),
                               GameTemplateConsoleAccess::setRemoteType,
                               0, 6, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(minTimeBetweenActivation, float,
                        getActiveTemplate(CID_RemoteControlledObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getMinTimeBetweenActivation(),
                        GameTemplateConsoleAccess::setMinTimeBetweenActivation,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(uavVehicleTemplate, std::string,
                        getActiveTemplate(CID_RemoteControlledObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getUavVehicleTemplate(),
                        GameTemplateConsoleAccess::setUavVehicleTemplate,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(uavVehicleSpeed, float,
                        getActiveTemplate(CID_RemoteControlledObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getUavVehicleSpeed(),
                        GameTemplateConsoleAccess::setUavVehicleSpeed, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(uavVehicleRadius, float,
                        getActiveTemplate(CID_RemoteControlledObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getUavVehicleRadius(),
                        GameTemplateConsoleAccess::setUavVehicleRadius, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(uavVehicleFlightHeight, float,
                        getActiveTemplate(CID_RemoteControlledObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getUavVehicleFlightHeight(),
                        GameTemplateConsoleAccess::setUavVehicleFlightHeight,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(uavVehicleSampleResolution, float,
                        getActiveTemplate(CID_RemoteControlledObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getUavVehicleSampleResolution(),
                        GameTemplateConsoleAccess::setUavVehicleSampleResolution,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(turnVelocity, float,
                        getActiveTemplate(CID_TurnableRemoteControlledObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getTurnVelocity(),
                        GameTemplateConsoleAccess::setTurnVelocity, kObjTemplGameConsoleTag)

// Trigger timing, radius and event properties. chance uses its validating setter.
BF2_CONSOLE_FIELD(triggerForEachActivatingPlayer, bool, activeTrigger(), m_triggerForEachActivatingPlayer, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(minimumTimeBetweenTriggering, float, activeTrigger(), m_minimumTimeBetweenTriggering, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(averageTimeBetweenTests, float, activeTrigger(), m_averageTimeBetweenTests, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(chance, float, getActiveTemplate(CID_TriggerTemplate) != 0, activeTrigger()->getChance(), objTemplTriggerChanceSet, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(events, int, activeTrigger(), m_events, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(triggerId, int, activeTrigger(), m_triggerId, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(radius, float, activeTrigger(), m_radius, kObjTemplGameConsoleTag)

// Ambient effect properties. playAtEventPosition and playAtPlayerPosition address the same field.
namespace {
AmbientEffectAreaTemplate* activeAmbientArea()
{
    return static_cast<AmbientEffectAreaTemplate*>(getActiveTemplate(CID_AmbientEffectAreaTemplate));
}
} // namespace
BF2_CONSOLE_FIELD(attachToEventObject, bool, activeAmbientArea(), m_attachToEventObject, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(linkedEffectContainer, std::string, activeAmbientArea(), m_linkedEffectContainer, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(numNetworkedEffects, int, activeAmbientArea(), m_numNetworkedEffects, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(projectOnHeightmap, bool, activeAmbientArea(), m_projectOnHeightmap, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(playAtEventPosition, bool, activeAmbientArea(), m_playAtEventPosition, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(playAtPlayerPosition, bool, activeAmbientArea(), m_playAtEventPosition, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(triggerId, int, activeAmbientArea(), m_triggerId, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(radius, float, activeAmbientArea(), m_radius, kObjTemplGameConsoleTag)

// Capture-point properties and supply-group methods.
// team sets the starting team; supplyGroupNeeded accepts an optional boolean defaulting to false.
namespace {
ControlPointTemplate* activeCP()
{
    return static_cast<ControlPointTemplate*>(getActiveTemplate(CID_ControlPointTemplate));
}
void objTemplCPTeam(int team) { activeCP()->setStartTeam(team); }          // ConsoleClass1232
void objTemplCPSupplyGroupNeeded(int team, int group, bool needed = false)
{
    activeCP()->addSupplyGroupNeeded(team, group, needed);
}
} // namespace
BF2_CONSOLE_PROPERTY(startTeam, int, activeCP(), setStartTeam, getStartTeam, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(team, objTemplCPTeam, getActiveTemplate(CID_ControlPointTemplate) != 0, int, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(radius, float, activeCP(), setRadius, getRadius, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(controlPointName, std::string, activeCP(), setControlPointName, getControlPointName, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(controlPointId, int, activeCP(), setControlPointId, getControlPointId, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(disableIfEnemyInsideRadius, bool, activeCP(), setDisableIfEnemyInsideRadius, getDisableIfEnemyInsideRadius, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(disableWhenLosingControl, bool, activeCP(), setDisableWhenLosingControl, getDisableWhenLosingControl, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(loseControlWhenEnemyClose, bool, activeCP(), setLoseControlWhenEnemyClose, getLoseControlWhenEnemyClose, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(loseControlWhenNotClose, bool, activeCP(), setLoseControlWhenNotClose, getLoseControlWhenNotClose, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(timeToGetControl, float, activeCP(), setTimeToGetControl, getTimeToGetControl, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(timeToLoseControl, float, activeCP(), setTimeToLoseControl, getTimeToLoseControl, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(unableToChangeTeam, bool, activeCP(), setUnableToChangeTeam, getUnableToChangeTeam, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(onlyTakeableByTeam, U32, activeCP(), setOnlyTakeableByTeam, getOnlyTakeableByTeam, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(minNrToTakeControl, U32, activeCP(), setMinNrToTakeControl, getMinNrToTakeControl, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(isUnstrategicControlPoint, bool, activeCP(), setIsUnstrategicControlPoint, getIsUnstrategicControlPoint, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(areaValueTeam1, U32, activeCP(), setAreaValueTeam1, getAreaValueTeam1, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(areaValueTeam2, U32, activeCP(), setAreaValueTeam2, getAreaValueTeam2, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(enemyTicketLossWhenCaptured, int, activeCP(), setEnemyTicketLossWhenCaptured, getEnemyTicketLossWhenCaptured, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(showOnMinimap, bool, activeCP(), setShowOnMinimap, getShowOnMinimap, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(signalTriggerId, int, activeCP(), setSignalTriggerId, getSignalTriggerId, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(radiusOffset, Vec3, activeCP(), setRadiusOffset, getRadiusOffset, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(supplyGroupId, int, activeCP(), setGroupId, getGroupId, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(supplyValueNeededTeam1, int, activeCP(), m_supplyValueNeeded[0], kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(supplyValueNeededTeam2, int, activeCP(), m_supplyValueNeeded[1], kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(supplyValueTeam1, int, activeCP(), m_supplyValue[0], kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(supplyValueTeam2, int, activeCP(), m_supplyValue[1], kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_2_3_ACTIVE(supplyGroupNeeded, objTemplCPSupplyGroupNeeded,
                              getActiveTemplate(CID_ControlPointTemplate) != 0,
                              int, int, bool, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(hoistFlag, bool, activeCP(), setHoistFlag, getHoistFlag, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(flagTemplateTeam0, std::string, activeCP(), setFlagTemplateTeam0, getFlagTemplateTeam0, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(flagTemplateTeam1, std::string, activeCP(), setFlagTemplateTeam1, getFlagTemplateTeam1, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(flagTemplateTeam2, std::string, activeCP(), setFlagTemplateTeam2, getFlagTemplateTeam2, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(hoistMinMax, Vec2, activeCP(), setHoistMinMax, getHoistMinMax, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(isHemisphere, bool, activeCP(), setIsHemisphere, getIsHemisphere, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(ForSoldierOnly, bool, activeCP(), setForSoldierOnly, getForSoldierOnly, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(HasGreyCapturingState, bool, activeCP(), setHasGreyCapturingState, getHasGreyCapturingState, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(minimapNameOffset, Vec2, activeCP(), setMinimapNameOffset, getMinimapNameOffset, kObjTemplGameConsoleTag)

// Vehicle and control categories select input maps and gameplay classification.
// Vehicle type uses a one-argument method; categories use getter/setter properties.
BF2_CONSOLE_PROPERTY_FN(vehicleCategory, dice::hfe::world::VehicleCategory,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getPcoVehicleCategory(),
                        GameTemplateConsoleAccess::setPcoVehicleCategory, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(controlsCategory, dice::hfe::world::VehicleCategory,
                        getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getPcoControlsCategory(),
                        GameTemplateConsoleAccess::setPcoControlsCategory, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setVehicleType, GameTemplateConsoleAccess::setPcoVehicleType,
                                   getActiveTemplate(CID_PlayerControlObjectTemplate) != 0,
                                   dice::hfe::world::VehicleType, kObjTemplGameConsoleTag)

// Spawner-template methods and properties. spawnDelay and setSpawnDelay both set minimum and maximum delay.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 0)

// KitPart executors: PE 0x4ACFB0 / 0x4AD230 / 0x4AD4A0; map insert helper 0x4AD510.
// Repeated setTeamGeometry for a team preserves the first entry (map::insert, not assignment).
namespace {
KitPartTemplate* activeKitPart()
{ return static_cast<KitPartTemplate*>(getActiveTemplate(CID_KitPartTemplate)); }
void kitPartTeamGeometry(int team, std::string geometry)
{ activeKitPart()->m_teamGeometry.insert(std::make_pair(team, geometry)); }
SupplyDepotTemplate* activeSupplyDepot()
{ return static_cast<SupplyDepotTemplate*>(getActiveTemplate(CID_SupplyDepotTemplate)); }
}
BF2_CONSOLE_PROPERTY(boneName, std::string, activeKitPart(), setBoneName, getBoneName, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(copyLinksCount, int, activeKitPart(), setCopyLinksCount, getCopyLinksCount, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(setTeamGeometry, kitPartTeamGeometry, activeKitPart() != 0, int, std::string, kObjTemplGameConsoleTag)

// SupplyDepot executors PE 0x4AF520..0x4B03C2. Float radius, int team, two booleans and
// the three fixed-argument authoring methods match the template's ELF symbols.
BF2_CONSOLE_FIELD(radius, float, activeSupplyDepot(), m_radius, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(team, int, activeSupplyDepot(), m_team, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_4(addAmmoType, activeSupplyDepot()->addAmmoType, activeSupplyDepot() != 0, int, int, float, float, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_4(addVehicleType, activeSupplyDepot()->addVehicleType, activeSupplyDepot() != 0, std::string, int, float, float, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_3(setHealth, activeSupplyDepot()->setHealth, activeSupplyDepot() != 0, int, float, float, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(workOnVehicles, bool, activeSupplyDepot(), m_workOnVehicles, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(workOnSoldiers, bool, activeSupplyDepot(), m_workOnSoldiers, kObjTemplGameConsoleTag)

// LadderTemplate::seatAnimationSystem - ConsoleClass1095, ELF executor 0x863E6C0; PE executor
// . This is the first binding after the category-0 defaults call at ELF 0x85933A2.
// Both executors gate on CID_LadderTemplate and call the concrete non-virtual pair directly.
namespace {
LadderTemplate* activeLadderTempl()
{
    return static_cast<LadderTemplate*>(getActiveTemplate(CID_LadderTemplate));
}
// The same property has a PCO-family executor (ELF 0x862D1A0 / PE 0x4B2590).
// Keep both retail active-template gates; parachutes inherit the PCO seat animation owner.
std::string getSeatAnimationSystem()
{
    PlayerControlObjectTemplate* const pco = GameTemplateConsoleAccess::playerControlObject();
    return pco ? pco->getSeatAnimationSystem() : activeLadderTempl()->getSeatAnimationSystem();
}
void setSeatAnimationSystem(const std::string& path)
{
    PlayerControlObjectTemplate* const pco = GameTemplateConsoleAccess::playerControlObject();
    if (pco)
        pco->setSeatAnimationSystem(path);
    else
        activeLadderTempl()->setSeatAnimationSystem(path);
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(seatAnimationSystem, std::string,
                        GameTemplateConsoleAccess::playerControlObject() || activeLadderTempl(),
                        getSeatAnimationSystem(), setSeatAnimationSystem, kObjTemplGameConsoleTag)

namespace {
ObjectSpawnerTemplate* activeSpawner()
{
    return static_cast<ObjectSpawnerTemplate*>(getActiveTemplate(CID_ObjectSpawnerTemplate));
}
void objTemplSpawnerSetObjectTemplate(int index, const std::string& name)
{
    activeSpawner()->setObjectTemplate(static_cast<unsigned int>(index), name);
}
void objTemplSpawnerSpawnDelay(float v)    { activeSpawner()->spawnDelay(v); }        // +0x128
void objTemplSpawnerSetSpawnDelay(float v) { activeSpawner()->setSpawnDelay(v); }     // +0x124
} // namespace
BF2_CONSOLE_METHOD_2_ACTIVE(setObjectTemplate, objTemplSpawnerSetObjectTemplate, getActiveTemplate(CID_ObjectSpawnerTemplate) != 0, int, std::string, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(setSpawnDelay, objTemplSpawnerSetSpawnDelay, getActiveTemplate(CID_ObjectSpawnerTemplate) != 0, float, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(spawnDelay, objTemplSpawnerSpawnDelay, getActiveTemplate(CID_ObjectSpawnerTemplate) != 0, float, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(spawnDelayAtStart, bool, activeSpawner(), setSpawnDelayAtStart, getSpawnDelayAtStart, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(timeToLive, float, activeSpawner(), setTimeToLive, getTimeToLive, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(distance, float, activeSpawner(), setDistance, getDistance, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(team, U32, activeSpawner(), setTeam, getTeam, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(objectTemplate, std::string, activeSpawner(), setObjectTemplate, getObjectTemplate, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(nrOfObjectToSpawn, int, activeSpawner(), setNrOfObjectToSpawn, getNrOfObjectToSpawn, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(maxNrOfObjectSpawned, U32, activeSpawner(), setMaxNrOfObjectSpawned, getMaxNrOfObjectSpawned, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(maxSpawnDelay, float, activeSpawner(), setMaxSpawnDelay, getMaxSpawnDelay, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(minSpawnDelay, float, activeSpawner(), setMinSpawnDelay, getMinSpawnDelay, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(useButtonRadius, float, activeSpawner(), setUseButtonRadius, getUseButtonRadius, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(effectOnSpawn, std::string, activeSpawner(), setEffectOnSpawn, getEffectOnSpawn, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(spawnOffset, Vec3, activeSpawner(), setSpawnOffset, getSpawnOffset, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(damageWhenLost, float, activeSpawner(), setDamageWhenLost, getDamageWhenLost, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(holdObject, bool, activeSpawner(), setHoldObject, getHoldObject, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(teamOnVehicle, bool, activeSpawner(), setTeamOnVehicle, getTeamOnVehicle, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(controlPointId, int, activeSpawner(), m_controlPointId, kObjTemplGameConsoleTag)

// Live spawner properties, available only when the active object has an ObjectSpawnerTemplate.
// Control-point id links the spawner to its capture point.
BF2_CONSOLE_OBJECT("Object", 2, 0)
namespace {
ObjectSpawner* activeSpawnerObject()
{
    IObject* object = objectManager->getActiveObject();
    if (!object)
        return 0;
    const IObjectTemplate* objectTemplate = object->getTemplate();
    if (!objectTemplate || objectTemplate->getClassID() != CID_ObjectSpawnerTemplate)
        return 0;
    return static_cast<ObjectSpawner*>(object);
}
void objSpawnerSetControlPointId(int id) { activeSpawnerObject()->setControlPointId(id); }
void objSpawnerSetVisibleTeam(int team)  { activeSpawnerObject()->setVisibleTeam(team); }
void objSpawnerSetActive(bool active)    { activeSpawnerObject()->setActive(active); }
int  objSpawnerGetVisibleTeam()          { return activeSpawnerObject()->getVisibleTeam(); }
int  objSpawnerGetControlPointId()       { return activeSpawnerObject()->getControlPointId(); }
} // namespace
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getVisibleTeam, objSpawnerGetVisibleTeam, int, activeSpawnerObject() != 0, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(spawnOffset, Vec3, activeSpawnerObject(), setSpawnOffset, getSpawnOffset, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getControlPointId, objSpawnerGetControlPointId, int, activeSpawnerObject() != 0, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(setVisibleTeam, objSpawnerSetVisibleTeam, activeSpawnerObject() != 0, int, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(setControlPointId, objSpawnerSetControlPointId, activeSpawnerObject() != 0, int, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(setActive, objSpawnerSetActive, activeSpawnerObject() != 0, bool, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(team, int, activeSpawnerObject(), setTeamOnStart, getTeamOnStart, kObjTemplGameConsoleTag)

// Restore the console object before registering ObjectTemplate properties.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 9)

// Soldier rig, inventory, sprint and collision-bone authoring.
// Rig setters are methods; numeric properties expose both read and write forms.
namespace {
SoldierTemplate* activeSoldierTempl()
{
    return static_cast<SoldierTemplate*>(getActiveTemplate(CID_SoldierTemplate));
}
void objTemplSoldierSkeleton1P(const std::string& n)        { activeSoldierTempl()->skeleton1P(n); }
void objTemplSoldierSkeleton3P(const std::string& n)        { activeSoldierTempl()->skeleton3P(n); }
void objTemplSoldierAnimSystem1P(const std::string& n)      { activeSoldierTempl()->animationSystem1P(n); }
void objTemplSoldierAnimSystem3P(const std::string& n)      { activeSoldierTempl()->animationSystem3P(n); }
void objTemplSoldierInventorySizeSet(U32 size)
{
    activeSoldierTempl()->m_inventorySize = static_cast<int>(size);   // retail SET is a raw store
}
void objTemplSoldierPoseCameraPos(int pose, const Vec3& p)  { activeSoldierTempl()->poseCameraPosition(pose, p); }
void objTemplSoldierCollisionBone(std::string name, int material, Vec3 offset,
                                  float radius, float length)
{
    activeSoldierTempl()->setSkeletonCollisionBone(name, material, offset, radius, length);
}
} // namespace
BF2_CONSOLE_PROPERTY_SETONLY(geometry1P, std::string,
                             getActiveTemplate(CID_SoldierTemplate) != 0,
                             GameTemplateConsoleAccess::setSoldierGeometry1P,
                             kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_SETONLY(geometry3P, std::string,
                             getActiveTemplate(CID_SoldierTemplate) != 0,
                             GameTemplateConsoleAccess::setSoldierGeometry3P,
                             kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(skeleton1P, objTemplSoldierSkeleton1P, getActiveTemplate(CID_SoldierTemplate) != 0, std::string, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(skeleton3P, objTemplSoldierSkeleton3P, getActiveTemplate(CID_SoldierTemplate) != 0, std::string, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(animationSystem1P, objTemplSoldierAnimSystem1P, getActiveTemplate(CID_SoldierTemplate) != 0, std::string, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(animationSystem3P, objTemplSoldierAnimSystem3P, getActiveTemplate(CID_SoldierTemplate) != 0, std::string, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(poseCameraPosition, objTemplSoldierPoseCameraPos, getActiveTemplate(CID_SoldierTemplate) != 0, int, Vec3, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(inventorySize, U32,
                        getActiveTemplate(CID_SoldierTemplate) != 0,
                        static_cast<U32>(activeSoldierTempl()->getInventorySize()),
                        objTemplSoldierInventorySizeSet, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_5(setSkeletonCollisionBone, objTemplSoldierCollisionBone,
                     getActiveTemplate(CID_SoldierTemplate) != 0,
                     std::string, int, Vec3, float, float, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(soundFilter, std::string, activeSoldierTempl(), setSoundFilter, getSoundFilter, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sprintRecoverTime, float,
                        getActiveTemplate(CID_SoldierTemplate) != 0,
                        GameTemplateConsoleAccess::getSoldierSprintRecoverTime(),
                        GameTemplateConsoleAccess::setSoldierSprintRecoverTime,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sprintDissipationTime, float,
                        getActiveTemplate(CID_SoldierTemplate) != 0,
                        GameTemplateConsoleAccess::getSoldierSprintDissipationTime(),
                        GameTemplateConsoleAccess::setSoldierSprintDissipationTime,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sprintLimit, float,
                        getActiveTemplate(CID_SoldierTemplate) != 0,
                        GameTemplateConsoleAccess::getSoldierSprintLimit(),
                        GameTemplateConsoleAccess::setSoldierSprintLimit,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sprintLossAtJump, float,
                        getActiveTemplate(CID_SoldierTemplate) != 0,
                        GameTemplateConsoleAccess::getSoldierSprintLossAtJump(),
                        GameTemplateConsoleAccess::setSoldierSprintLossAtJump,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(gasMaskSprintFactor, float,
                        getActiveTemplate(CID_SoldierTemplate) != 0,
                        GameTemplateConsoleAccess::getSoldierGasMaskSprintFactor(),
                        GameTemplateConsoleAccess::setSoldierGasMaskSprintFactor,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(ragdollLandedDelay, float,
                        getActiveTemplate(CID_SoldierTemplate) != 0,
                        GameTemplateConsoleAccess::getSoldierRagdollLandedDelay(),
                        GameTemplateConsoleAccess::setSoldierRagdollLandedDelay,
                        kObjTemplGameConsoleTag)

// AnimatedBundleTemplate - the three retail method bindings immediately following the Soldier
// rig block. PE generated executors: skeleton 0x4875C0, animationSystem1P 0x487790 and
// animationSystem3P 0x487980; all are argc==1 / std::string / void and gate on
// CID_AnimatedBundleTemplate (0x949C). The two animation names are deliberately duplicate console
// keys of SoldierTemplate's bindings above: ConsoleObjects is a multimap and skips whichever CID
// gate is inactive. GenericFireArmTemplate answers the AnimatedBundle CID in queryImplementation,
// so every stock handheld weapon reaches these base setters.
namespace {
AnimatedBundleTemplate* activeAnimatedBundleTempl()
{
    return static_cast<AnimatedBundleTemplate*>(getActiveTemplate(CID_AnimatedBundleTemplate));
}
void objTemplAnimatedBundleSkeleton(const std::string& n)   { activeAnimatedBundleTempl()->skeleton(n); }
void objTemplAnimatedBundleAnimSystem1P(const std::string& n)
{
    activeAnimatedBundleTempl()->animationSystem1P(n);
}
void objTemplAnimatedBundleAnimSystem3P(const std::string& n)
{
    activeAnimatedBundleTempl()->animationSystem3P(n);
}
} // namespace
BF2_CONSOLE_METHOD_1_ACTIVE(skeleton, objTemplAnimatedBundleSkeleton,
                            getActiveTemplate(CID_AnimatedBundleTemplate) != 0,
                            std::string, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(animationSystem1P, objTemplAnimatedBundleAnimSystem1P,
                            getActiveTemplate(CID_AnimatedBundleTemplate) != 0,
                            std::string, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(animationSystem3P, objTemplAnimatedBundleAnimSystem3P,
                            getActiveTemplate(CID_AnimatedBundleTemplate) != 0,
                            std::string, kObjTemplGameConsoleTag)

// Kit loadout, geometry, ability and unlock properties. addKitVehicleMod takes a template name and modification value.
BF2_CONSOLE_PROPERTY_FN(kitType, KitType, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitType(),
                        GameTemplateConsoleAccess::setKitType, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(timeToLiveAfterDeath, float, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitTimeToLiveAfterDeath(),
                        GameTemplateConsoleAccess::setKitTimeToLiveAfterDeath, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(kitTeam, int, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitTeam(),
                        GameTemplateConsoleAccess::setKitTeam, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(unlockIndex, int, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitUnlockIndex(),
                        GameTemplateConsoleAccess::setKitUnlockIndex, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(defaultWeaponIndex, int, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitDefaultWeaponIndex(),
                        GameTemplateConsoleAccess::setKitDefaultWeaponIndex, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(addKitVehicleMod, GameTemplateConsoleAccess::addKitVehicleMod,
                                   getActiveTemplate(CID_KitTemplate) != 0,
                                   std::string, int, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sprintStaminaDissipationFactor, float, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitSprintStaminaDissipationFactor(),
                        GameTemplateConsoleAccess::setKitSprintStaminaDissipationFactor, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("geometry.kit", int, getActiveTemplate(CID_KitTemplate) != 0,
                              GameTemplateConsoleAccess::getKitGeometry(),
                              GameTemplateConsoleAccess::setKitGeometry, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("geometry.dropGeom", int, getActiveTemplate(CID_KitTemplate) != 0,
                              GameTemplateConsoleAccess::getKitDropGeom(),
                              GameTemplateConsoleAccess::setKitDropGeom, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(abilityRestoreRate, float, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitAbilityRestoreRate(),
                        GameTemplateConsoleAccess::setKitAbilityRestoreRate, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(dropHeadwear, std::string, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitDropHeadwear(),
                        GameTemplateConsoleAccess::setKitDropHeadwear, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(abilityInVehicleRadius, float, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitAbilityInVehicleRadius(),
                        GameTemplateConsoleAccess::setKitAbilityInVehicleRadius, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(abilityInVehicleStrength, float, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitAbilityInVehicleStrength(),
                        GameTemplateConsoleAccess::setKitAbilityInVehicleStrength, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(abilityInVehicleMaterial, int, getActiveTemplate(CID_KitTemplate) != 0,
                        GameTemplateConsoleAccess::getKitAbilityInVehicleMaterial(),
                        GameTemplateConsoleAccess::setKitAbilityInVehicleMaterial, kObjTemplGameConsoleTag)

// Destructible-object respawn, walkability and bounds properties.
BF2_CONSOLE_FIELD(respawnDelay, float, static_cast<DestroyableObjectTemplate*>(getActiveTemplate(CID_DestroyableObjectTemplate)),
                  respawnDelay, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(nrOfRespawns, int, static_cast<DestroyableObjectTemplate*>(getActiveTemplate(CID_DestroyableObjectTemplate)),
                  nrOfRespawns, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(usedByAi, bool, getActiveTemplate(CID_DestroyableObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getUsedByAi(),
                        GameTemplateConsoleAccess::setUsedByAi, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(initiallyWalkable, bool, getActiveTemplate(CID_DestroyableObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getInitiallyWalkable(),
                        GameTemplateConsoleAccess::setInitiallyWalkable, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(minCorner, Vec3, getActiveTemplate(CID_DestroyableObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getMinCorner(),
                        GameTemplateConsoleAccess::setMinCorner, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(maxCorner, Vec3, getActiveTemplate(CID_DestroyableObjectTemplate) != 0,
                        GameTemplateConsoleAccess::getMaxCorner(),
                        GameTemplateConsoleAccess::setMaxCorner, kObjTemplGameConsoleTag)

// Antenna sway properties. maxSwayAngle contains separate X and Z limits.
BF2_CONSOLE_PROPERTY_FN_NAMED("antenna.maxSwayAngle", Vec2, getActiveTemplate(CID_AntennaObjectTemplate) != 0,
                              GameTemplateConsoleAccess::getAntennaMaxSwayAngle(),
                              GameTemplateConsoleAccess::setAntennaMaxSwayAngle, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("antenna.stiffness", float, getActiveTemplate(CID_AntennaObjectTemplate) != 0,
                              GameTemplateConsoleAccess::getAntennaStiffness(),
                              GameTemplateConsoleAccess::setAntennaStiffness, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("antenna.damping", float, getActiveTemplate(CID_AntennaObjectTemplate) != 0,
                              GameTemplateConsoleAccess::getAntennaDamping(),
                              GameTemplateConsoleAccess::setAntennaDamping, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("antenna.accelerationFactor", float, getActiveTemplate(CID_AntennaObjectTemplate) != 0,
                              GameTemplateConsoleAccess::getAntennaAccelerationFactor(),
                              GameTemplateConsoleAccess::setAntennaAccelerationFactor, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("antenna.speedFactor", float, getActiveTemplate(CID_AntennaObjectTemplate) != 0,
                              GameTemplateConsoleAccess::getAntennaSpeedFactor(),
                              GameTemplateConsoleAccess::setAntennaSpeedFactor, kObjTemplGameConsoleTag)

// AreaObjectTemplate: ELF ConsoleClass1007/1008 executors 0x862C820/0x862CC70;
// /0x4B2430. addLinePoint is a one-argument Vec2 method; radius is a property.
namespace {
AreaObjectTemplate* activeAreaObjectTemplate()
{
    return static_cast<AreaObjectTemplate*>(getActiveTemplate(CID_AreaObjectTemplate));
}
void addAreaLinePoint(const Vec2& point) { activeAreaObjectTemplate()->addLinePoint(point); }
}
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 0)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(addLinePoint, addAreaLinePoint,
    activeAreaObjectTemplate() != 0, Vec2, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY(triggerRadius, float, activeAreaObjectTemplate(),
    setTriggerRadius, getTriggerRadius, kObjTemplGameConsoleTag)

// Item-container unlock level and replacement-item bindings.
BF2_CONSOLE_PROPERTY_FN(unlockLevel, int, getActiveTemplate(CID_ItemContainerTemplate) != 0,
                        GameTemplateConsoleAccess::getUnlockLevel(),
                        GameTemplateConsoleAccess::setUnlockLevel, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(replaceItem, GameTemplateConsoleAccess::replaceItem,
                                   getActiveTemplate(CID_ItemContainerTemplate) != 0,
                                   std::string, kObjTemplGameConsoleTag)

// Triggerable object and target authoring.
// The target input key is type, while its serializer emits targetType. addTriggerableTarget takes one name.
namespace {
TriggerableTemplate* activeTriggerableTemplate()
{ return static_cast<TriggerableTemplate*>(getActiveTemplate(CID_TriggerableTemplate)); }
TriggerableTargetTemplate* activeTriggerableTargetTemplate()
{ return static_cast<TriggerableTargetTemplate*>(getActiveTemplate(CID_TriggerableTargetTemplate)); }
void objTemplTriggerableChance(float value) { activeTriggerableTemplate()->setChance(value); }
void objTemplAddTriggerableTarget(std::string name)
{ activeTriggerableTemplate()->addTriggerableTarget(name); }
}
BF2_CONSOLE_FIELD(radius, float, activeTriggerableTemplate(), m_radius, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(triggerId, int, activeTriggerableTemplate(), m_triggerId, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(events, int, activeTriggerableTemplate(), m_types, kObjTemplGameConsoleTag)
BF2_CONSOLE_PROPERTY_FN(chance, float, activeTriggerableTemplate()!=0,
                        activeTriggerableTemplate()->m_float190, objTemplTriggerableChance,
                        kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(averageTimeBetweenTests, float, activeTriggerableTemplate(), m_float194, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(minimumTimeBetweenTriggering, float, activeTriggerableTemplate(), m_float198, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(triggerForEachActivatingPlayer, bool, activeTriggerableTemplate(), m_bool19C, kObjTemplGameConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(addTriggerableTarget, objTemplAddTriggerableTarget,
                                   activeTriggerableTemplate()!=0, std::string, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(notCycledIfOccupied, bool, activeTriggerableTemplate(), m_bool19D, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(hasPartner, bool, activeTriggerableTemplate(), m_bool19E, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(movementSpeed, float, activeTriggerableTargetTemplate(), m_movementSpeed, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(movementType, int, activeTriggerableTargetTemplate(), m_movementType, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(clockwise, bool, activeTriggerableTargetTemplate(), m_clockwise, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(type, int, activeTriggerableTargetTemplate(), m_targetType, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(delayBeforeStart, float, activeTriggerableTargetTemplate(), m_delayBeforeStart, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(delayBeforeCycle, float, activeTriggerableTargetTemplate(), m_delayBeforeCycle, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(delayBeforeEnd, float, activeTriggerableTargetTemplate(), m_delayBeforeEnd, kObjTemplGameConsoleTag)
BF2_CONSOLE_FIELD(cycles, int, activeTriggerableTargetTemplate(), m_cycles, kObjTemplGameConsoleTag)

// Handed to the console registry by GameLogicObjTemplBFModule::init, the way the original
// registers 0x8AD54F8.
ConsoleObjectList& objTemplGameConsoleObjects()
{
    return consoleBindingList(kObjTemplGameConsoleTag);
}

} // namespace world
} // namespace hfe
} // namespace dice
