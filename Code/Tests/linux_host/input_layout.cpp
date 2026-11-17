// [TEMP] [CHOSEN, not recovered] Real InputScript interpreter with a synthetic keyboard layout.
// Only the sink is scaffolding: printable characters require DIKs above Cocoa's 0..127 range.
#include "../../BF2/Platform/InputScript.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {
void require(bool value, const char* name)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
struct KeyEvent
{
    unsigned short dik;
    bool down;
    std::u16string text;
};
struct LayoutSink : dice::hfe::inputscript::IInputScriptSink
{
    std::vector<KeyEvent> keys;
    bool quit = false;
    void scriptKey(unsigned short dik, bool down, const std::u16string& text) override
    { keys.push_back({dik, down, text}); }
    std::u16string scriptKeyCharacter(unsigned short dik, bool shift) override
    {
        if (dik == 141 && !shift) return u"=";
        if (dik == 181 && shift) return u"/";
        if (dik == 179 && !shift) return u",";
        return {};
    }
    void scriptQuit() override { quit = true; }
    void scriptMouseTo(float, float) override { require(false, "unexpected absolute mouse action"); }
    void scriptMouseBy(float, float) override { require(false, "unexpected relative mouse action"); }
    void scriptButton(int, bool) override { require(false, "unexpected button action"); }
    void scriptWheel(int) override { require(false, "unexpected wheel action"); }
};
}

int runInputLayoutCheck()
{
    dice::hfe::inputscript::InputScript script;
    LayoutSink sink;
    require(script.loadFromEnvironment(), "load layout fixture through production interpreter");
    script.tick(sink);
    require(sink.keys.size() == 1 && sink.keys[0].dik == 141 && sink.keys[0].down && sink.keys[0].text == u"=",
            "type resolves its first character to DIK 141 above the old 0x80 scan limit");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!sink.quit && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        script.tick(sink);
    }
    require(sink.quit, "layout script reaches quit after real hold/frame waits");
    const KeyEvent expected[] = {
        {141,true,u"="}, {141,false,{}},
        {42,true,{}}, {181,true,u"/"}, {181,false,{}}, {42,false,{}},
        {179,true,u","}, {179,false,{}}
    };
    require(sink.keys.size() == sizeof(expected) / sizeof(expected[0]), "type emits only expected key transitions");
    for (std::size_t i = 0; i < sink.keys.size(); ++i)
        require(sink.keys[i].dik == expected[i].dik && sink.keys[i].down == expected[i].down &&
                sink.keys[i].text == expected[i].text, "extended DIK presses, releases and Shift preserve typed text");
    std::puts("PASS: InputScript type searches the full Linux DIK domain and preserves Shift/release ordering");
    return 0;
}
