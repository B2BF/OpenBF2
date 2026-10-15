// Path: [INFERRED] beside VehicleHudCompTemplate.h; the type name is ELF-attested.
#ifndef GAME_OBJECTS_VEHICLEHUDICONS_H
#define GAME_OBJECTS_VEHICLEHUDICONS_H

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace dice { namespace hfe { namespace world {

#if defined(_M_IX86)
static_assert(std::is_same<std::intptr_t, int>::value,
              "x86 icon handles keep the retail int layout and mangling");
#endif

// ELF constructor 0x822CE50; BF2.exe constructor 0x65B020. The containing
// template initializes the three scalar handles after this constructor returns.
// Retail ELF size 0x48, VC7.1 size 0xA0. Native STL determines this build's size;
// never copy either retail layout over these containers.
struct VehicleHudIcons
{
    VehicleHudIcons();

    std::vector<std::string> vehicleIcons;    // ELF +00; PE +00
    std::vector<std::string> abilityIcons;    // ELF +0C; PE +10
    std::string miniMapIcon;                 // ELF +18; PE +20
    std::string typeIcon;                    // ELF +1C; PE +3C
    std::string spottedIcon;                 // ELF +20; PE +58
    // Element type is attested by vector<int> manglings in the ELF setters.
    // On the client these integer handles carry Ref2System texture addresses, so they are
    // std::intptr_t: int on x86 (unchanged layout and mangling), pointer-wide on native x64.
    std::vector<std::intptr_t> vehicleIconPtrs;   // ELF +24; PE +74
    std::vector<std::intptr_t> abilityIconPtrs;   // ELF +30; PE +84
    std::intptr_t miniMapIconPtr;                // ELF +3C; PE +94
    std::intptr_t typeIconPtr;                   // ELF +40; PE +98
    std::intptr_t spottedIconPtr;                // ELF +44; PE +9C
};

} } }

#endif
