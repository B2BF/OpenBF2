// [CHOSEN, not recovered] Owner-approved offline LAN dialog policy.
// Shared by every rebuilt client platform.
#ifndef BF2_SWIFFPLAYER_OFFLINECONNECTPOPUP_H
#define BF2_SWIFFPLAYER_OFFLINECONNECTPOPUP_H

#include <cstring>
#include <cstdio>
#include "../gameswf_impl.h"

namespace dice { namespace hfe { namespace ui {

// Only an explicit, visible main-menu request from an offline profile is eligible.
// A null popupName represents an absent or non-string selection.
inline bool shouldOpenOfflineConnectPopup(bool liveMainMenu, double loginStatus,
                                         const char* popupName, bool visible)
{
    return liveMainMenu && loginStatus == 1.0 && visible &&
        popupName && std::strcmp(popupName, "connecttoip") == 0;
}

// Missing menu content declines quietly (owner decision, 2026-11-13). This matches what
// retail does offline. The authored popups.flush gate (sprite 457, +0x1AF..+0x1C2) exits
// while the cached loginStatus is below 2, and the native side never navigates, so AVM1
// leaves the selection in place and no dialog appears. A decline is logged once per
// process; it is not a stop.
inline bool declineOfflineConnectPopup(const char* reason)
{
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        std::fprintf(stderr, "[SwiffPlayer] offline connecttoip declined: %s\n", reason);
        std::fflush(stderr);
    }
    return false;
}

// Complete the authored request after movie actions and before display. The movie
// keeps its offline status, callbacks, flushObject, selectedPopup and close flow.
// Returns true only when this pass actually selects the dialog frame; any missing piece of
// the authored content answers false, with no dialog.
inline bool openOfflineConnectPopup(gameswf::movie* root, bool liveMainMenu)
{
    if (!liveMainMenu || !root) return false;
    gameswf::as_value status, selection;
    root->get_member("loginStatus", &status);
    root->get_member("selectedPopup", &selection);
    const char* name = selection.get_type() == gameswf::as_value::STRING
        ? selection.to_string() : nullptr;
    if (!shouldOpenOfflineConnectPopup(true,
            status.get_type() == gameswf::as_value::NUMBER ? status.to_number() : 0.0,
            name, true)) return false;

    gameswf::as_value popup, visible;
    if (!root->get_member("popups", &popup) || !popup.to_object() || !popup.to_object()->to_movie())
        return declineOfflineConnectPopup("main-menu popups clip absent");
    gameswf::movie* clip = popup.to_object()->to_movie();
    if (!clip->get_member("_visible", &visible) || !visible.to_bool()) return false;

    int frame = -1;
    auto* definition = static_cast<gameswf::movie_definition_sub*>(clip->get_movie_definition());
    if (!definition || !definition->get_labeled_frame("connecttoip", &frame))
        return declineOfflineConnectPopup("popups connecttoip frame absent");
    // Do not restart the dialog or its editable fields on subsequent updates.
    if (clip->get_current_frame() == frame) return false;
    if (!clip->goto_labeled_frame("connecttoip"))
        return declineOfflineConnectPopup("popups connecttoip navigation failed");
    clip->set_play_state(gameswf::movie_interface::STOP);
    return true;
}

} } }
#endif
