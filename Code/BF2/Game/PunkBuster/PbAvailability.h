// [CHOSEN, not recovered] Whether this client build can run PunkBuster at all (owner request,
// 2026-11-20). Retail has no such query: mainMenu.swf's PunkBuster checkbox (GeneralSettings
// get/setAllowPunkBuster, the GSClPunkBuster preference) is always live, and BF2Engine::startGame
// passes GSClPunkBuster to IGameClient::joinServer unchecked (BF2.exe 0x4052A1..0x4052E7). The only
// pb/pbcl.dll test in BF2.exe is PB_set_cl_punkbuster's (0x4D953B), which gates writes from the
// vendor DLL. The vendor client exists only as 32-bit Windows DLLs, so the x64 Windows, macOS and
// Linux clients can never run it. On those builds the preference is ignored: a join announces no
// PunkBuster (a PunkBuster server refuses it at once with denial 31), the menu shows the option off
// and unavailable, and the server browser's "PunkBuster servers only" filter is not applied. The
// stored GSClPunkBuster preference is left untouched, so a profile shared with an x86 client keeps
// it; the stored filter bit is cleared by the movie itself when it seeds the filter panel.
#ifndef GAME_PUNKBUSTER_PBAVAILABILITY_H
#define GAME_PUNKBUSTER_PBAVAILABILITY_H

namespace pb {

#if defined(_WIN32) && !defined(_WIN64)
const bool kClientBuildSupported = true;
#else
const bool kClientBuildSupported = false;
#endif

// The PunkBuster flag a join announces: the profile's preference, where the build can run PunkBuster.
inline bool clientRequest(bool preference, bool buildSupported = kClientBuildSupported)
{
    return buildSupported && preference;
}

// mainMenu.swf's filter panel (sprite 491, setPunkbusterFilter) stores "bf2_anticheat = 1" under
// this Multiplay toggle-filter mask.
const int kPunkBusterServerFilterMask = 128;

// Whether a stored toggle filter takes part in the server-browser query.
inline bool serverFilterApplies(int mask, bool buildSupported = kClientBuildSupported)
{
    return buildSupported || mask != kPunkBusterServerFilterMask;
}

} // namespace pb

#endif
