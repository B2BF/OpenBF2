// Plan-system enums shared by the action, condition, behaviour and interpreter classes.
// [INFERRED] file placement and enumerator spellings where noted; values are attested.
// S0 skeleton (docs/macos-ai-behaviour-plan.md). Owner: package A-core.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPTYPES_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPTYPES_H

namespace dice {
namespace hfe {
namespace ai {

// Statement type: BAPStatement::getId. Values from every getId body; names from the 44 BAPST*
// strings the .ai files use (aiSettings.activateDefaultInterpreter). Same `: int` spelling as the
// forward declarations in Main/AIConsole.h and Main/AISettingsInterfaces.h.
// operator<<(ostream&, BAPStatementType) is libaidll.so 0x3C7D90 (a 45-case jump table).
enum BAPStatementType : int
{
    BAPSTParallel              = 0,    // BAPCombinerParallel
    BAPSTSerial                = 1,    // BAPCombinerSerial
    BAPSTPlanEnd               = 2,    // BAPFlowCEnd
    BAPSTIf                    = 3,    // BAPFlowCIf
    BAPSTWait                  = 4,    // BAPFlowCWait
    BAPSTWhile                 = 5,    // BAPFlowCWhile
    BAPSTAimAt                 = 6,    // BAPAAimAt, BAPAAimAt3d
    BAPSTMoveTo                = 7,    // BAPAMoveTo, BAPAMoveTo3d
    BAPSTMoveToDirection       = 8,    // BAPAMoveToDirection, BAPAMoveTo3dDirection
    BAPSTMoveToObject          = 9,    // BAPAMoveToObject, BAPAMoveTo3dObject
    BAPSTMoveAwayFromObject    = 10,   // BAPAMoveAwayFrom3dObject
    BAPSTTrigger               = 11,
    BAPSTTriggerContinously    = 12,
    BAPSTIdle                  = 13,
    BAPSTResetControls         = 14,
    BAPSTInfoWrapper           = 15,
    BAPSTAimingWrapper         = 16,
    BAPSTFinishedWrapper       = 17,
    BAPSTWeaponWrapper         = 18,
    BAPSTLookAtWrapper         = 19,
    BAPSTSense                 = 20,
    BAPSTOverrideSensingAgent  = 21,
    BAPSTPlaceC4               = 22,
    BAPSTLookAt                = 23,   // BAPALook
    BAPSTTurn                  = 24,
    BAPSTRoll3d                = 25,
    BAPSTSoldierPose           = 26,
    BAPSTUpdateVehicle         = 27,
    BAPSTChangeWeapon          = 28,
    BAPSTICChangeVehicle       = 29,
    // 30: no class returns it and the stream operator prints nothing for it. [UNRESOLVED]
    BAPSTICTurnLock            = 31,
    BAPSTEvaluate              = 32,
    BAPSTNotifyBot             = 33,
    BAPSTPhysicsEnable         = 34,
    BAPSTSetSensingAgentFocus  = 35,
    BAPSTMergeSoldierInputs    = 36,
    BAPSTSendRadioMessage      = 37,
    BAPSTDebug                 = 38,
    BAPSTForgetTarget          = 39,
    BAPSTUseFlareOnMissileLock = 40,
    BAPSTRunOverObject         = 41,
    BAPSTHonkHorn              = 42,
    BAPSTEvaluateCondition     = 43,
    BAPSTRevive                = 44
};

// Condition type: BAPCondition::getConditionType. Values from the getConditionType bodies and
// operator<<(ostream&, BAPConditionType) 0x3C7AE0 (59 cases; its strings in the comments).
// [INFERRED] enumerator names (BAPCT + class stem). Retail quirks kept by the classes, not here:
// BAPConPointInFront returns 23 (ObjectInFront), ValidPitchAiming/ValidYawAiming return 32.
enum BAPConditionType : int
{
    BAPCTLookDeviation          = 0,
    BAPCTPrecision              = 1,
    BAPCTPrecision3d            = 2,
    BAPCTBombPrecision3d        = 3,
    BAPCTAnd                    = 4,
    BAPCTEndOfPath              = 5,
    BAPCTFalse                  = 6,    // "false"
    BAPCTGeneric1Arg            = 7,    // "Generic1Arg"; no class returns it
    BAPCTGeneric2Arg            = 8,    // "Generic2Arg"; no class returns it
    BAPCTInsideBattleZone       = 9,
    BAPCTIteration              = 10,
    BAPCTLineOfFire             = 11,
    BAPCTPointDistance          = 12,
    BAPCTPointInFront           = 13,
    BAPCTNot                    = 14,
    BAPCTDistanceToProj         = 15,
    BAPCTStrategicObjectStatus  = 16,
    BAPCTObjectBehind           = 17,
    BAPCTObjectDistance         = 18,
    BAPCTObjectDistanceToPoint  = 19,
    BAPCTObjectEnemyOccupied    = 20,
    BAPCTObjectExists           = 21,
    BAPCTObjectHealth           = 22,
    BAPCTObjectInFront          = 23,
    BAPCTObjectInLandingZone    = 24,
    BAPCTObjectInsideFrustum    = 25,
    BAPCTObjectLineOfFire       = 26,
    BAPCTObjectManDown          = 27,
    BAPCTObjectMoved            = 28,
    BAPCTObjectOccupied         = 29,
    BAPCTObjectSpeed            = 30,
    BAPCTObjectTouchingLand     = 31,   // "ObjectTouchingGround"
    BAPCTObjectValidAiming      = 32,
    BAPCTObjectValidPosition    = 33,
    BAPCTObjectVisible          = 34,
    BAPCTOr                     = 35,
    BAPCTPointAt                = 36,
    BAPCTPosition               = 37,
    BAPCTSharedCondition        = 38,
    BAPCTTime                   = 39,
    BAPCTTrue                   = 40,   // "true"
    BAPCTWeaponMagAmmo          = 41,
    BAPCTWeaponTotalAmmo        = 42,
    BAPCTTotalAmmo              = 43,   // "TotalWeaponAmmo"
    BAPCTWeaponReadyToFire      = 44,
    BAPCTWeaponTargetReachable  = 45,
    BAPCTWeaponHeat             = 46,
    BAPCTWeaponHasLockOnTarget  = 47,
    // 48: no class returns it and the stream operator prints nothing for it. [UNRESOLVED]
    BAPCTIsTriggered            = 49,
    BAPCTAllowFire              = 50,   // "Allow fire"
    BAPCTIsSprinting            = 51,   // "Sprinting"
    BAPCTWeaponIsZoomed         = 52,   // "Zoomed"
    BAPCTWeaponDetonatorIsActive = 53,  // "Detonator active"
    BAPCTWeaponIsActive         = 54,   // "Weapon active"
    BAPCTOpLessThan             = 55,   // BAPConOpLessThan<float>
    BAPCTOkToThrowGrenade       = 56,   // "Ok to throw grenade"
    BAPCTSplashDamage           = 57,   // "Splash damage"
    BAPCTTriggerableActive      = 58    // "Triggerable Active"
};

// Free enums seen only in mangled parameter lists. [UNRESOLVED] enumerators; the same `: int`
// forward declarations already used by Main/ and AIBot/. BotLodLevel is defined in AIBot/BotMain.h
// (operator<< 0x3C78B0 prints BLLNone/BLLLight/BLLFull/BLLMedium).
enum BotLodLevel : int;
enum AIRadioMessageType : int;
enum AIRadioChannelType : int;
enum AvoidSide : int;

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPTYPES_H
