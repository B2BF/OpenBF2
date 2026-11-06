// Owns AI landing zones. Constructed when the DLL loads and destroyed by BF2AI::clear.
// reset is its only virtual method; the destructor is nonvirtual.
// Header placement [INFERRED]: beside its .cpp, whose path AIDLL_r.dll confirms (Main/AILandingZoneManager.cpp).

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_AILANDINGZONEMANAGER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_AILANDINGZONEMANAGER_H

#include <map>
#include <string>
#include <vector>

#include "../../../../Math/Vec3.h"

// Vec2 and Vec3 are Math aliases, so do not forward-declare them as classes in this namespace.
namespace dice {
namespace hfe {
namespace ai {

class AILandingZone;

// Defined, with its enumerators, in AILandingZone.h.
enum LandingZoneDirection : int;

// sizeof is 40 (0x28) in retail, from the `operator new(0x28)` in this file's static initialiser
// and it will not be 40 here - two of the members are real STL containers and a third is a string.
class AILandingZoneManager
{
public:
    AILandingZoneManager();

    // Not virtual. Deletes the zones.
    ~AILandingZoneManager();                         // ELF 0x399C50 (D1; D0 0x399D60)

    // The class's only virtual, and the only entry in its vtable.
    // ELF 0x39A350: deletes the zones and answers true (`mov $0x1,%eax`).
    virtual bool reset();

    // ELF 0x399E70 / 0x399E90 / 0x399ED0: the active zone's name, or "No valid zone is active"
    // when there is none; the int and name forms first select the zone (0 when not found).
    const std::string& activeZone();
    const std::string& activeZone(int index);
    const std::string& activeZone(const std::string& name);
    AILandingZone* getActiveZone();                  // ELF 0x399F40

    // ELF 0x399F50: 0 for an index out of range.
    AILandingZone* getAILandingZone(int index);
    // ELF 0x399F80: the zone registered under the name, 0 when there is none.
    AILandingZone* getAILandingZone(const std::string& name);
    // ELF 0x399FE0. This is the method `AIRenderUtil`'s constructor calls.
    int getNumberOfLandingZones() const;

    // ELF 0x39A000: a new zone with the next id, registered by name and made active.
    AILandingZone* createLandingZone(const std::string& name, const Vec2& minCorner,
                                     const Vec2& maxCorner, float height,
                                     LandingZoneDirection beachDirection);
    // ELF 0x39A460 / 0x39A4A0: the first zone containing the position.
    bool isInsideLandingZone(const Vec3& position) const;
    bool isInsideLandingZone(const Vec3& position, AILandingZone*& zone) const;
    // ELF 0x39A510: "<name> <min> <max> <beach direction>" per zone.
    std::string listZones();

    // set by this file'S static initialiser, not by any `init`. See the header block.
    static AILandingZoneManager* m_instance;

private:
    /* +0x00 */ // vptr - one slot, `reset`
    /* +0x04 */ std::map<std::string, AILandingZone*> m_zonesByName;
    /* +0x10 */ std::vector<AILandingZone*> m_zones;      // what getNumberOfLandingZones measures

    // The active zone (activeZone / createLandingZone), 0 for none.
    /* +0x1C */ AILandingZone* m_activeZone;
    // [UNRESOLVED] Initialized to zero; no reader in this class.
    /* +0x20 */ int m_unresolved20;

    // constructed from A literal, and the literal is the class's whole personality:
    // "No valid zone is active".
    /* +0x24 */ std::string m_activeZoneName;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_AILANDINGZONEMANAGER_H
