// The control-box profile switch the Soldier makes for the local player (ledger N2, 17a): it writes
// HapticSettings' active control-box values from the profile of what the player is in.
// [CHOSEN, not recovered] file and names: retail inlines all of this in Soldier::Soldier
// (0x5AC784..0x5AC90B), Soldier::onEnter (0x5ACCC6..0x5ADE14) and Soldier::onExit
// (0x5AB611..0x5AB83A). It lives here so the fake-settings check can drive it without a Soldier.
// The Falcon force inputs that were declared here are archived (Code/Archive/NovintFalcon).
#ifndef GAME_OBJECTS_GAME_SOLDIERHAPTICPROFILE_H
#define GAME_OBJECTS_GAME_SOLDIERHAPTICPROFILE_H
#include "../../../Core/BuildConfig.h"
#if !DEDICATED
namespace dice { namespace hfe {
class IHapticSettings;
namespace world {
class IPlayerControlObjectTemplate;

// HapticSettings' per-vehicle profiles, in their interface order.
enum HapticProfile {
    HapticProfileSoldier,
    HapticProfileAir,
    HapticProfileHeli,
    HapticProfileLand,
    HapticProfileSea
};

// The five copies every site makes: the active control-box length and stiffness, aim sensitivity,
// aim sensitivity length and turn sensitivity, each from the profile's own value (HapticSettings +0x48,
// +0x50, +0xA8, +0xB0, +0xB8 set from the profile's getters). Each getter is called once per component,
// as retail does.
void applyHapticProfile(IHapticSettings* settings, HapticProfile profile);

// Soldier::onEnter's choice (0x5ACCC6..0x5AD51F): by the entered object's vehicle type; type 26 by
// its own vehicle category (2 AIR, 3 HELI while its controls category is non-zero, else SOLDIER);
// types 8, 9, 15, 19, 20, 24 and 25 SOLDIER; every other type by the root object's vehicle category
// (0 LAND, 1 SEA, 2 AIR, 3 HELI, other SOLDIER).
HapticProfile hapticProfileForEntered(IPlayerControlObjectTemplate* entered,
                                      IPlayerControlObjectTemplate* root);

} } }
#endif
#endif
