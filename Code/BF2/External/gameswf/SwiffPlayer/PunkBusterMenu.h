// [CHOSEN, not recovered] The menu half of the per-build PunkBuster policy (Game/PunkBuster/
// PbAvailability.h). Retail's natives only read and write GeneralSettings; nothing in BF2.exe or
// SwiffPlayer.dll greys the PunkBuster checkboxes. Where the build cannot run PunkBuster, the
// natives that seed them (GeneralSettings.getAllowPunkBuster on the join pages, sprites 500/508,
// and Multiplay.getToggleFilter(128) in the filter panel, sprite 491) answer false and call the
// authored checkbox's own disable(), the dice.UIs.Form method the movie itself uses for
// filtersButton. Missing content declines quietly: the answer stays false and nothing else changes.
#ifndef BF2_SWIFFPLAYER_PUNKBUSTERMENU_H
#define BF2_SWIFFPLAYER_PUNKBUSTERMENU_H

#include "../gameswf_action.h"

namespace dice { namespace hfe { namespace ui {

// The member name both authored PunkBuster checkboxes carry on the clip that seeds them.
const char kPunkBusterCheckboxMember[] = "punkbuster";

// Calls owner.<member>.disable() when the clip has that control and the control has that method.
// Returns whether the method ran.
inline bool disableAuthoredControl(gameswf::as_object_interface* owner, gameswf::as_environment* env,
                                   const char* member)
{
    if (!owner || !env) return false;
    gameswf::as_value control;
    if (!owner->get_member(member, &control) || !control.to_object()) return false;
    gameswf::as_object_interface* object = control.to_object();
    gameswf::as_value method;
    if (!object->get_member("disable", &method)) return false;
    if (!method.to_as_function() && !method.to_c_function()) return false;
    gameswf::call_method0(method, env, object);
    return true;
}

} } }
#endif
