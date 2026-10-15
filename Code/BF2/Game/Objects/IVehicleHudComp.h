// Vehicle HUD configuration and sound feedback. Icon paths return strings by value; resolved handles are integers.
// [INFERRED] Source placement.
#ifndef GAME_OBJECTS_IVEHICLEHUDCOMP_H        // [INFERRED] guard style
#define GAME_OBJECTS_IVEHICLEHUDCOMP_H

#include <cstdint>
#include <string>
#include "IObjectComponent.h"
#include "VehicleHudCompTemplate.h"          // VehicleHudInfo / VehicleHudIcons

namespace dice {
namespace hfe {
namespace world {

class IPostProcessComp;

class IVehicleHudComp : public IObjectComponent
{
public:
    virtual int  getGuiIndex(bool) = 0;                     // +0x38 [CHOSEN ret]
    // PE689F20 / ELF8229DF0 return the actual object+50 component pointer;
    // HUD78B1E0 dispatches its getInfo slot. This was incorrectly chosen bool.
    virtual IPostProcessComp* getEnablePostProcessing(int) = 0; // +0x3C
    virtual std::string getVehicleIcon(int) = 0;           // +0x40 hidden sret
    // Texture handles: std::intptr_t is int on x86 (unchanged mangling), pointer-wide on x64.
    virtual std::intptr_t getVehicleIconPtr(int) = 0;      // +0x44 vector<int>
    virtual std::string getAbilityIcon(int) = 0;           // +0x48 hidden sret
    virtual std::intptr_t getAbilityIconPtr(int) = 0;      // +0x4C vector<int>
    virtual bool getShowOnCamMode(int) = 0;                // +0x50 movzx eax,al
    virtual void playingPantingSound(bool) = 0;             // +0x54
    virtual bool getIsPlayingPantingSound() = 0;            // +0x58 [CHOSEN ret]
    virtual void playInjurySound() = 0;                     // +0x5C
    virtual VehicleHudIcons* getHudIcons() = 0;             // +0x60 ; PE HUD 0x74F480
    virtual VehicleHudInfo*  getHudInfo() = 0;              // +0x64 the template's info block
    virtual ~IVehicleHudComp() {}                           // +0x68 +0x6C - declared last on purpose
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_IVEHICLEHUDCOMP_H
