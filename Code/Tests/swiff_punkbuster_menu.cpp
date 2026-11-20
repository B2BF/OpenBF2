// [TEMP] [CHOSEN, not recovered] Content-free check of the per-build PunkBuster policy
// (Game/PunkBuster/PbAvailability.h) and the menu helper that greys the authored checkboxes
// (SwiffPlayer/PunkBusterMenu.h). No game content, sockets or engine startup.
#include "../BF2/External/gameswf/SwiffPlayer/PunkBusterMenu.h"
#include "../BF2/Game/PunkBuster/PbAvailability.h"
#include <cstdio>
#include <initializer_list>

namespace {
using namespace gameswf;
int s_disableCalls = 0;
as_object_interface* s_disableThis = 0;
void recordDisable(const fn_call& fn)
{
    ++s_disableCalls;
    s_disableThis = fn.this_ptr;
}
}

int main()
{
    int checks = 0, failures = 0;
    const auto check = [&](bool condition, const char* description) {
        ++checks;
        if (!condition) {
            ++failures;
            std::fprintf(stderr, "FAIL: %s\n", description);
        }
    };

    // The build answer: only 32-bit Windows can load pbcl.dll.
#if defined(_WIN32) && !defined(_WIN64)
    check(pb::kClientBuildSupported, "x86 Windows build supports PunkBuster");
#else
    check(!pb::kClientBuildSupported, "x64 Windows, macOS and Linux builds have no PunkBuster");
#endif

    // The join flag: the preference only where the build can run PunkBuster.
    check(pb::clientRequest(true, true) && !pb::clientRequest(false, true),
          "a PunkBuster build passes the preference as stored (retail)");
    check(!pb::clientRequest(true, false) && !pb::clientRequest(false, false),
          "a build without PunkBuster never announces it, whatever the profile says");
    check(pb::clientRequest(true) == pb::kClientBuildSupported, "the default follows the build");

    // The server-browser filters: only the PunkBuster mask is dropped, and only without PunkBuster.
    check(!pb::serverFilterApplies(128, false), "stale PunkBuster-only filter is not applied");
    for (int mask : {1, 2, 4, 8, 16, 32, 64, 256, 512, 1024, 2048, 4096, 8192})
        check(pb::serverFilterApplies(mask, false), "every other filter applies without PunkBuster");
    for (int mask : {1, 64, 128, 256, 8192})
        check(pb::serverFilterApplies(mask, true), "every filter applies on a PunkBuster build (retail)");

    // The menu helper: owner.punkbuster.disable(), receiver = the checkbox.
    using dice::hfe::ui::disableAuthoredControl;
    using dice::hfe::ui::kPunkBusterCheckboxMember;
    as_environment env;
    smart_ptr<as_object> page = new as_object;
    smart_ptr<as_object> checkbox = new as_object;
    checkbox->set_member("disable", as_value(recordDisable));
    page->set_member(kPunkBusterCheckboxMember, as_value(checkbox.get_ptr()));
    check(disableAuthoredControl(page.get_ptr(), &env, kPunkBusterCheckboxMember)
              && s_disableCalls == 1 && s_disableThis == checkbox.get_ptr(),
          "the authored checkbox's disable() runs on the checkbox");

    smart_ptr<as_object> otherPage = new as_object;
    check(!disableAuthoredControl(otherPage.get_ptr(), &env, kPunkBusterCheckboxMember) && s_disableCalls == 1,
          "a clip without the checkbox declines quietly");
    smart_ptr<as_object> plainPage = new as_object;
    smart_ptr<as_object> plainBox = new as_object;
    plainPage->set_member(kPunkBusterCheckboxMember, as_value(plainBox.get_ptr()));
    check(!disableAuthoredControl(plainPage.get_ptr(), &env, kPunkBusterCheckboxMember) && s_disableCalls == 1,
          "a control without disable() declines quietly");
    plainBox->set_member("disable", as_value(1.0));
    check(!disableAuthoredControl(plainPage.get_ptr(), &env, kPunkBusterCheckboxMember) && s_disableCalls == 1,
          "a non-function disable member is not called");
    smart_ptr<as_object> valuePage = new as_object;
    valuePage->set_member(kPunkBusterCheckboxMember, as_value(true));
    check(!disableAuthoredControl(valuePage.get_ptr(), &env, kPunkBusterCheckboxMember) && s_disableCalls == 1,
          "a non-object member declines quietly");
    check(!disableAuthoredControl(0, &env, kPunkBusterCheckboxMember)
              && !disableAuthoredControl(page.get_ptr(), 0, kPunkBusterCheckboxMember) && s_disableCalls == 1,
          "no clip or environment declines quietly");

    std::printf("PunkBuster menu policy: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
