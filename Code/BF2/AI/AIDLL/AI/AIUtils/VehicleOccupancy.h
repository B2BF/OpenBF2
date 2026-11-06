// [INFERRED] path/name: the separate ELF translation unit is anchored by
// _GLOBAL__I__ZN4dice3hfe2ai15vehicleOccupiedEji , without a filename.
// AIUtils is an attested directory; this filename is chosen for the recovered free function.
#ifndef AI_AIDLL_AI_AIUTILS_VEHICLEOCCUPANCY_H
#define AI_AIDLL_AI_AIUTILS_VEHICLEOCCUPANCY_H

namespace dice { namespace hfe { namespace ai {

class SquadMember;

// Parameter types/namespace from the ELF mangled name; Boolean materialized in both ports.
bool vehicleOccupied(unsigned int handle, int side);

// Returns 1 for another member of the same squad, 0 for another squad or an unassigned
// occupier, and -1 when no qualifying occupation or booking was found.
int vehicleOccupiedBySquadMember(unsigned int handle, SquadMember* member, int side);

} } }
#endif
