// [TEMP] [CHOSEN, not recovered] Content-free decision and production VM regression; the three
// missing-content cases (clip, frame, navigation) expect a quiet false and a live process.
#include "../BF2/External/gameswf/SwiffPlayer/OfflineConnectPopup.h"
#include "base/tu_file.h"
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace {
using namespace gameswf;
// Forwards every movie call to a real instance. The subclasses below change one answer each,
// so a case can reach a branch the real content cannot (a goto that fails after its label
// lookup succeeded).
struct ForwardingMovie : movie
{
    movie* m_target;
    explicit ForwardingMovie(movie* target) : m_target(target) {}
    void set_member(const tu_stringi& n, const as_value& v) override { m_target->set_member(n, v); }
    bool get_member(const tu_stringi& n, as_value* v) override { return m_target->get_member(n, v); }
    movie_definition* get_movie_definition() override { return m_target->get_movie_definition(); }
    int get_current_frame() const override { return m_target->get_current_frame(); }
    bool has_looped() const override { return m_target->has_looped(); }
    void restart() override { m_target->restart(); }
    void advance(float dt) override { m_target->advance(dt); }
    void goto_frame(int f) override { m_target->goto_frame(f); }
    bool goto_labeled_frame(const char* l) override { return m_target->goto_labeled_frame(l); }
    void display() override {}
    void set_play_state(play_state s) override { m_target->set_play_state(s); }
    play_state get_play_state() const override { return m_target->get_play_state(); }
    void set_background_color(const rgba&) override {}
    rgba get_background_color() const override { return rgba(); }
    void set_background_alpha(float) override {}
    float get_background_alpha() const override { return 1.0f; }
    void set_display_viewport(int, int, int, int) override {}
    void notify_mouse_state(int, int, int) override {}
    void set_variable(const char*, const char*) override {}
    void set_variable(const char*, const wchar_t*) override {}
    const char* get_variable(const char*) const override { return ""; }
    const char* call_method(const unsigned short*) override { return ""; }
    const char* call_method(const char*) override { return ""; }
    void set_visible(bool) override {}
    bool get_visible() const override { return true; }
    void* get_userdata() override { return 0; }
    void set_userdata(void*) override {}
    movie* get_root_movie() override { return this; }
    as_environment* get_action_environment() override { return m_target->get_action_environment(); }
};
// The popups clip whose label lookup succeeds but whose navigation does not.
struct GotoFailsClip : ForwardingMovie
{
    using ForwardingMovie::ForwardingMovie;
    bool goto_labeled_frame(const char*) override { return false; }
};
// A root that answers "popups" with the substitute clip.
struct SubstituteRoot : ForwardingMovie
{
    movie* m_popups;
    SubstituteRoot(movie* root, movie* popups) : ForwardingMovie(root), m_popups(popups) {}
    bool get_member(const tu_stringi& n, as_value* v) override
    {
        if (n == "popups") { *v = as_value(static_cast<as_object_interface*>(m_popups)); return true; }
        return ForwardingMovie::get_member(n, v);
    }
};
}

int main(int argc, char** argv)
{
    using dice::hfe::ui::shouldOpenOfflineConnectPopup;
    int checks = 0, failures = 0;
    const auto check = [&](bool actual, bool expected, const char* description) {
        ++checks;
        if (actual != expected) {
            ++failures;
            std::fprintf(stderr, "FAIL: %s\n", description);
        }
    };
    check(shouldOpenOfflineConnectPopup(true, 1, "connecttoip", true), true,
          "offline visible main-menu request selects the authored frame");
    for (double status : {0.0, 2.0, -1.0, 1.5, std::numeric_limits<double>::quiet_NaN()})
        check(shouldOpenOfflineConnectPopup(true, status, "connecttoip", true), false,
              "absent, online or unexpected profile status is unchanged");
    for (const char* popup : {"", "connectToIp", "connecttoipExtra", "login", "quit", "options"})
        check(shouldOpenOfflineConnectPopup(true, 1, popup, true), false,
              "every other popup is unchanged");
    check(shouldOpenOfflineConnectPopup(false, 1, "connecttoip", true), false,
          "other movies are unchanged");
    check(shouldOpenOfflineConnectPopup(true, 1, nullptr, true), false,
          "non-string selection is unchanged");
    check(shouldOpenOfflineConnectPopup(true, 1, "connecttoip", false), false,
          "hidden stale selection is not reopened");

    if (argc < 2) return 2;
    const auto verify = [&](bool condition, const char* description) { check(condition, true, description); };
    using namespace gameswf;
    using dice::hfe::ui::openOfflineConnectPopup;
    tu_file input(argv[1], "rb");
    if (input.get_error()) return 2;
    movie_definition* definition = create_movie_no_recurse(&input,
        DO_NOT_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
    if (!definition) return 2;
    movie_interface* instance = definition->create_instance();
    instance->advance(0.0f);
    movie* root = instance->get_root_movie();
    if (argc == 3 && std::strcmp(argv[2], "missing-popup") == 0) {
        // This fixture names its sprite differently; display-list children cannot
        // be hidden by assigning an undefined script member with the same name.
        root->set_member("loginStatus", as_value(1.0));
        root->set_member("selectedPopup", as_value("connecttoip"));
        verify(!openOfflineConnectPopup(root, true), "absent popups clip declines quietly");
        verify(!openOfflineConnectPopup(root, true), "a repeated decline stays quiet and alive");
        std::printf("Offline Connect to IP missing popups: %d/%d checks passed\n", checks - failures, checks);
        return failures ? 1 : 0;
    }
    as_value popup, original;
    verify(root->get_member("popups", &popup), "fixture creates a real popup sprite");
    if (!popup.to_object()) return 2;
    movie* clip = popup.to_object()->to_movie();
    root->get_member("openPopup", &original);
    verify(original.to_as_function() != nullptr, "fixture defines an authored openPopup callback");
    if (!original.to_as_function()) return 2;
    root->set_member("loginStatus", as_value(1.0));
    root->set_member("flushObject", as_value("kept"));
    const auto request = [&](const char* name) {
        as_environment* env = root->get_action_environment();
        env->push(as_value(name));
        as_value result;
        (*original.to_as_function())(fn_call(&result, root, env, 1, env->get_top_index()));
        env->drop(1);
    };
    const auto text = [&](const char* key, const char* expected) {
        as_value v;
        return root->get_member(key, &v) && std::strcmp(v.to_string(), expected) == 0;
    };
    request("connecttoip");
    verify(clip->get_current_frame() == 0, "offline authored request stops before frame selection");
    if (argc == 3 && std::strcmp(argv[2], "missing-label") == 0) {
        verify(!openOfflineConnectPopup(root, true) && clip->get_current_frame() == 0,
               "absent connecttoip frame declines quietly, popup untouched");
        std::printf("Offline Connect to IP missing label: %d/%d checks passed\n", checks - failures, checks);
        return failures ? 1 : 0;
    }
    {
        // A navigation that fails after the label lookup succeeded declines the same way.
        GotoFailsClip* failing = new GotoFailsClip(clip);
        failing->add_ref();
        SubstituteRoot* substitute = new SubstituteRoot(root, failing);
        substitute->add_ref();
        verify(!openOfflineConnectPopup(substitute, true) && clip->get_current_frame() == 0,
               "failed connecttoip navigation declines quietly, popup untouched");
        substitute->drop_ref();
        failing->drop_ref();
    }
    verify(!openOfflineConnectPopup(root, false), "other movie does not navigate");
    verify(openOfflineConnectPopup(root, true), "offline request selects the real authored label");
    verify(clip->get_current_frame() == 1 && clip->get_play_state() == movie_interface::STOP,
          "dialog is selected and stopped");
    clip->set_member("enteredAddress", as_value("127.0.0.1"));
    verify(!openOfflineConnectPopup(root, true), "next update does not restart the dialog");
    as_value entered;
    clip->get_member("enteredAddress", &entered);
    verify(std::strcmp(entered.to_string(), "127.0.0.1") == 0, "field state survives updates");
    verify(text("loginStatus", "1") && text("selectedPopup", "connecttoip") && text("flushObject", "kept"),
          "offline status and authored bookkeeping survive");
    as_value callback;
    root->get_member("openPopup", &callback);
    verify(callback.to_as_function() == original.to_as_function(), "authored callback is never replaced");

    // Model only Cancel's observed bookkeeping and navigation, then reopen.
    root->set_member("selectedPopup", as_value(""));
    clip->goto_labeled_frame("noDialog");
    verify(!openOfflineConnectPopup(root, true) && clip->get_current_frame() == 0,
          "Cancel stays closed");
    request("connecttoip");
    verify(openOfflineConnectPopup(root, true), "Cancel then reopen selects the dialog again");
    clip->goto_labeled_frame("noDialog");
    clip->set_member("_visible", as_value(false));
    verify(!openOfflineConnectPopup(root, true), "hidden selected popup stays hidden");
    for (double status : {0.0, 2.0}) {
        root->set_member("loginStatus", as_value(status));
        request("connecttoip");
        verify(!openOfflineConnectPopup(root, true) && clip->get_current_frame() == 0,
              "absent/online profile retains authored navigation");
    }
    root->set_member("loginStatus", as_value(1.0));
    request("quit");
    verify(!openOfflineConnectPopup(root, true) && text("selectedPopup", "quit"),
          "other popup selection is preserved");
    root->set_member("selectedPopup", as_value(1.0));
    verify(!openOfflineConnectPopup(root, true), "non-string movie selection is ignored");
    release_script_objects();
    instance->drop_ref();
    definition->drop_ref();
    clear();
    std::printf("Offline Connect to IP: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
