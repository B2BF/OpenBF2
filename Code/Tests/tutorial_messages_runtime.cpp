// [TEMP] [CHOSEN, not recovered] Tutorial-option regression fixture.
// Links the production client objects without invoking the application entry point.
// Checks command-line parsing, profile serialization, console dispatch and HUD publication.
// No sockets, renderer, application startup or content.
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string>

#include "../BF2/Core/ConsoleObject.h"
#include "../BF2/Core/ConsoleObjectMacro.h"
#include "../BF2/Core/Random.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/GeneralSettings.h"
#include "../BF2/Game/GameLogic/PlayerVOHelp.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "../BF2/Game/Main/BF2Engine.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/Menu/BF2HudManager.h"
#include "../BF2/Scene/Object/PlayerManager.h"

namespace dice { namespace hfe { ConsoleObjectList& settingsConsoleObjects(); } }

namespace {
unsigned checks = 0, failures = 0;
void check(bool value, const char* description)
{
    ++checks;
    if (!value) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

// Runs one parse with std::cout and the stderr descriptor captured.
struct ParseResult {
    bool accepted;
    std::string usage, diagnostics;
};
ParseResult parseCaptured(dice::hfe::BF2Engine* engine, const char* command)
{
    ParseResult result{};
    std::ostringstream usage;
    std::streambuf* previous = std::cout.rdbuf(usage.rdbuf());
    std::fflush(stderr);
    std::FILE* file = std::tmpfile();
    const int capture = file ? fileno(file) : -1;
    const int saved = dup(STDERR_FILENO);
    if (capture >= 0)
        dup2(capture, STDERR_FILENO);
    result.accepted = engine->parseParameters(command);
    std::fflush(stderr);
    if (capture >= 0) {
        dup2(saved, STDERR_FILENO);
        char buffer[4096];
        lseek(capture, 0, SEEK_SET);
        for (ssize_t n; (n = read(capture, buffer, sizeof buffer)) > 0;)
            result.diagnostics.append(buffer, static_cast<size_t>(n));
    }
    if (file)
        std::fclose(file);
    close(saved);
    std::cout.rdbuf(previous);
    result.usage = usage.str();
    return result;
}

// Case 21 only tests for a non-null player: this opaque presence token is never dereferenced.
struct LocalPlayerObserver : dice::hfe::world::PlayerManager {
    int presence = 0;
    dice::hfe::world::IPlayer* getLocalHumanPlayer() override
    { return reinterpret_cast<dice::hfe::world::IPlayer*>(&presence); }
};

struct TutorialHudObserver : dice::hfe::hud::BF2HudManager {
    unsigned localized = 0, replaced = 0, published = 0;
    std::wstring getLocalizedString(std::string key) override
    {
        check(key == "HUD_HELP_SQUAD_callForTarget", "tutorial keeps its retail localization key");
        ++localized;
        return L"tutorial fixture";
    }
    std::wstring replaceWideInputKeys(std::wstring value) override
    { ++replaced; return value; }
    void showWideHelp(std::wstring text, float seconds) override
    {
        check(text == L"tutorial fixture" && seconds == 9.0f, "tutorial keeps its text and duration");
        ++published;
    }
};
}

// Selected with the macOS linker's -e; the linked production main is never entered.
extern "C" int bf2TutorialMessagesMain(int, char**)
{
    using namespace dice::hfe;
    // Pre-init engine objects have no full-engine shutdown path in this bounded fixture.
    g_system = new System;
    BF2Engine* engine = new BF2Engine(nullptr);
    settings = new SettingsRepostitory;
    check(engine->parseParameters(""), "no arguments accept the profile-controlled default");
    check(!settings->boolExists("GSCommandLineShowTutorialMessages"), "no argument publishes no override");
    // Exercise the real parser and repository with each launch form independently. The
    // maintainers' argv evidence has no +tutorialMessages; append it to that line.
    enum : int { Usage = -1, Refused = -2 };
    struct ParserCase {
        const char* command;
        int overrideValue; // Usage: retail help text; Refused: +tutorialMessages diagnostic.
    };
    const ParserCase parserCases[] = {
        { "+tutorialMessages 0", 0 },
        { "+tutorialMessages 1", 1 },
        { "+tutorialMessages \"0\"", 0 },
        { "+tutorialMessages \"1\"", 1 },
        { "+restart 1 +tutorialMessages 0 +playerName LoopBot +fullscreen 0", 0 },
        { "+modPath mods/bf2 +fullscreen 0 +szx 800 +szy 600 +restart 1"
          " +joinServer 127.0.0.1 +port 16890 +playerName LoopBot +tutorialMessages 0", 0 },
        { "+modPath mods/bf2 +fullscreen 0 +szx 800 +szy 600 +restart 1"
          " +tutorialMessages 0 +joinServer 127.0.0.1 +port 16890 +playerName LoopBot", 0 },
        { "+ranked 0 +tutorialMessages 0", 0 },
        { "+tutorialMessages 2", Refused },
        { "+tutorialMessages", Refused },
        { "+tutorialMessages +fullscreen 0", Refused },
        { "+tutorialMessages \"\"", Refused },
        // +help and +? are retail again: every form prints usage and ends start-up.
        { "+help", Usage },
        { "+help 0", Usage },
        { "+help 1", Usage },
        { "+help \"0\"", Usage },
        { "+help 2", Usage },
        { "+help +fullscreen 0", Usage },
        { "+fullscreen 0 +help 0", Usage },
        { "+?", Usage },
        { "+? 0", Usage },
        { "+? 1", Usage },
    };
    for (const ParserCase& item : parserCases) {
        settings->boolErase("GSCommandLineShowTutorialMessages");
        if (std::strstr(item.command, "+joinServer")) {
            // Sentinels prevent a preceding case from satisfying neighboring-option checks.
            settings->stringSet("GSJoinAddress", "");
            settings->intSet("GSPort", 0);
            settings->intSet("GSWindowSizeX", 0);
            settings->intSet("GSWindowSizeY", 0);
            settings->stringSet("GSModDirectory", "");
            settings->boolSet("skipMovies", false);
            settings->boolSet("GSFullScreen", true);
        }
        const ParseResult parsed = parseCaptured(engine, item.command);
        const bool expectsOverride = item.overrideValue >= 0;
        const bool hasOverride = settings->boolExists("GSCommandLineShowTutorialMessages");
        const bool printedUsage = parsed.usage.find("Usage: BF2.exe <options>") != std::string::npos;
        const bool refusal = parsed.diagnostics.find("+tutorialMessages: expected 0 or 1\n") != std::string::npos;
        check(parsed.accepted == expectsOverride && hasOverride == expectsOverride &&
              (!hasOverride || settings->boolGet("GSCommandLineShowTutorialMessages", true) ==
                               (item.overrideValue == 1)) &&
              printedUsage == (item.overrideValue == Usage) &&
              refusal == (item.overrideValue == Refused),
              item.command);
        if (std::strstr(item.command, "+joinServer")) {
            check(settings->stringGet("GSJoinAddress", "") == "127.0.0.1" &&
                  settings->intGet("GSPort", 0) == 16890 &&
                  settings->intGet("GSWindowSizeX", 0) == 800 &&
                  settings->intGet("GSWindowSizeY", 0) == 600 &&
                  settings->stringGet("GSModDirectory", "") == "mods/bf2" &&
                  settings->boolGet("skipMovies", false) &&
                  !settings->boolGet("GSFullScreen", true),
                  "full launch line retains surrounding option values");
        }
        if (std::strstr(item.command, "+ranked"))
            check(settings->boolGet("GSRanked", false), "+ranked ignores its value even beside +tutorialMessages 0");
    }
    settings->boolErase("GSCommandLineShowTutorialMessages");
    check(!parseCaptured(engine, "+help").accepted, "bare +help retains usage and exit");
    check(!parseCaptured(engine, "+?").accepted, "+? retains usage and exit");
    check(!parseCaptured(engine, "+help 0").accepted, "+help 0 is usage again");
    check(!parseCaptured(engine, "+help 1").accepted, "+help 1 is usage again");
    check(!parseCaptured(engine, "+tutorialMessages 2").accepted, "invalid tutorialMessages value ends start-up");
    check(!settings->boolExists("GSCommandLineShowTutorialMessages"), "usage and refusal never create an override");
    check(engine->parseParameters("+fullscreen 0 +restart 1"), "unrelated options are accepted");
    check(!settings->boolExists("GSCommandLineShowTutorialMessages"),
          "without the switch no override exists, so the profile applies");
    GeneralSettings first, second;
    g_generalSettings = &first;
    mainConsoleObjects->registerConsoleObjects(settingsConsoleObjects());
    io::OldConsole console;
    std::string output;
    const auto execute = [&](const std::string& script) {
        output.clear();
        console.executeLine(script, output, false, false, false, false, false);
    };

    check(first.getShowTutorialMessages(), "tutorial messages default ON");
    check(first.getSettingsScript().find("GeneralSettings.setShowTutorialMessages 1\n") != std::string::npos,
          "ON is serialized in General.con");
    const std::string before = first.getSettingsScript();
    // The new commands work at normal player access without unlocking the developer console.
    console.setAccess(1);
    execute("GeneralSettings.setShowTutorialMessages 0\nGeneralSettings.getShowTutorialMessages");
    check(!first.getShowTutorialMessages() && output == "0", "end-user console can set and read OFF");
    execute("GeneralSettings.setConfirmQuit 1");
    check(!first.getConfirmQuit() && output == "Unauthorised method!",
          "existing general bindings retain their original access policy");
    first.setShowTutorialMessages(true);
    check(first.getSettingsScript() == before, "tutorial setting leaves other serialized options unchanged");

    console.setAccess(2);
    execute("GeneralSettings.setShowTutorialMessages 0\nGeneralSettings.setConfirmQuit 1\n"
            "GeneralSettings.setPlayedVOHelp \"HUD_HELP_TEST\"");
    check(!first.getShowTutorialMessages() && first.getConfirmQuit(), "settings after the new row still execute");
    check(first.checkPlayedVOHelp("HUD_HELP_TEST"), "played-help history remains independent");
    const std::string saved = first.getSettingsScript();
    check(saved.find("GeneralSettings.setShowTutorialMessages 0\n") != std::string::npos,
          "OFF is serialized in General.con");
    g_generalSettings = &second;
    execute(saved);
    // The existing writer also emits setLastAwardsCheckDate, which has no console binding.
    // Preserve that baseline diagnostic rather than attributing it to the new option.
    check(!second.getShowTutorialMessages() && second.getConfirmQuit() && output == "Unknown object or method!",
          "saved script restores OFF and the other options on another settings owner");
    check(second.checkPlayedVOHelp("HUD_HELP_TEST"), "saved script preserves played-help history");
    second.setDefaultValues();
    check(second.getShowTutorialMessages() && second.checkPlayedVOHelp("HUD_HELP_TEST"),
          "Reset restores ON while retaining played-help history");
    second.setShowTutorialMessages(false);
    second.setDefaults();
    execute("GeneralSettings.setConfirmQuit 1");
    check(second.getShowTutorialMessages(), "loading an older script without the option leaves ON");

    // Model an older registry by withholding only the two new bindings. The real interpreter
    // must still execute known rows after the unknown command. This does not run retail BF2.exe.
    mainConsoleObjects->unregisterConsoleObjects(settingsConsoleObjects());
    ConsoleObjectList olderBindings;
    for (ConsoleObject* binding : settingsConsoleObjects()) {
        const char* name = binding->getFunctionName();
        if (std::strcmp(name, "setShowTutorialMessages") && std::strcmp(name, "getShowTutorialMessages"))
            olderBindings.push_back(binding);
    }
    mainConsoleObjects->registerConsoleObjects(olderBindings);
    execute("GeneralSettings.setShowTutorialMessages 0\nGeneralSettings.setConfirmQuit 0");
    check(!second.getConfirmQuit() && output.find("Unknown object or method!") != std::string::npos,
          "an unknown extension row does not stop subsequent general settings");
    mainConsoleObjects->unregisterConsoleObjects(olderBindings);
    mainConsoleObjects->registerConsoleObjects(settingsConsoleObjects());
    g_generalSettings = nullptr;
    execute("GeneralSettings.getShowTutorialMessages");
    // The registry skips inactive bindings before the dispatcher can see them.
    check(output == "Unknown object or method!", "new commands refuse a missing settings owner");
    g_generalSettings = &second;

    // The observer has process lifetime: BF2HudManager's destructor requires the booted input
    // and event systems, which this focused fixture deliberately does not initialize.
    TutorialHudObserver* hud = new TutorialHudObserver;
    LocalPlayerObserver players;
    hudManager = hud;
    world::playerManager = &players;
    PlayerVOHelp help;
    second.setShowTutorialMessages(true);
    const Random initialRandom = Rand;
    check(help.tryMessage(21, 0.0f) && hud->published == 1, "ON publishes a tutorial to the HUD");
    const Random afterOn = Rand;
    const std::string historyBefore = second.getSettingsScript();
    second.setShowTutorialMessages(false);
    Rand = initialRandom;
    check(help.tryMessage(21, 0.0f) && hud->published == 1 && hud->localized == 1 && hud->replaced == 1,
          "OFF suppresses publication and text preparation, retaining the message result");
    check(Rand.m_seed == afterOn.m_seed, "OFF retains the shared random draw");
    second.setShowTutorialMessages(true);
    check(second.getSettingsScript() == historyBefore, "suppression does not mutate profile history");
    check(help.tryMessage(21, 0.0f) && hud->published == 2, "ON restores tutorial publication");

    const std::string profileOn = second.getSettingsScript();
    check(engine->parseParameters("+restart +tutorialMessages 0 +playerName TutorialTester +fullscreen 0"),
          "+tutorialMessages 0 parses alongside other launch options");
    check(settings->boolExists("GSCommandLineShowTutorialMessages") &&
          !settings->boolGet("GSCommandLineShowTutorialMessages", true) &&
          settings->boolGet("skipMovies", false) && !settings->boolGet("GSFullScreen", true),
          "override and neighboring launch options are published");
    Rand = initialRandom;
    check(help.tryMessage(21, 0.0f) && hud->published == 2 && hud->localized == 2 && hud->replaced == 2,
          "+tutorialMessages 0 suppresses text despite profile ON");
    check(Rand.m_seed == afterOn.m_seed, "command-line suppression retains the shared random draw");
    check(second.getSettingsScript() == profileOn, "OFF override never serializes over profile ON");
    second.setDefaultValues();
    execute("GeneralSettings.setShowTutorialMessages 1");
    g_generalSettings = &first;
    first.setShowTutorialMessages(true);
    check(help.tryMessage(21, 0.0f) && hud->published == 2,
          "OFF override survives Reset, script replay and a profile switch");
    g_generalSettings = &second;
    second.setShowTutorialMessages(false);
    const std::string profileOff = second.getSettingsScript();
    check(engine->parseParameters("+tutorialMessages 1"), "+tutorialMessages 1 is accepted");
    check(help.tryMessage(21, 0.0f) && hud->published == 3, "+tutorialMessages 1 forces text despite profile OFF");
    check(second.getSettingsScript() == profileOff, "ON override never serializes over profile OFF");
    execute(profileOff);
    check(engine->parseParameters("+fullscreen 0") && help.tryMessage(21, 0.0f) && hud->published == 4,
          "session override survives profile replay and unrelated parameter parsing");
    check(engine->parseParameters("+tutorialMessages 0 +tutorialMessages 1") && help.tryMessage(21, 0.0f) &&
          hud->published == 5, "last repeated tutorialMessages option wins ON");
    check(engine->parseParameters("+tutorialMessages 1 +tutorialMessages 0") && help.tryMessage(21, 0.0f) &&
          hud->published == 5, "last repeated tutorialMessages option wins OFF");
    check(!parseCaptured(engine, "+help 1").accepted && help.tryMessage(21, 0.0f) && hud->published == 5,
          "+help 1 no longer forces text; the earlier session override stays OFF");
    // Model a fresh session by removing only the session repository entry.
    settings->boolErase("GSCommandLineShowTutorialMessages");
    check(help.tryMessage(21, 0.0f) && hud->published == 5, "without an override profile OFF decides again");
    second.setShowTutorialMessages(true);
    check(help.tryMessage(21, 0.0f) && hud->published == 6, "without an override profile ON decides again");
    hudManager = nullptr;
    world::playerManager = nullptr;
    mainConsoleObjects->unregisterConsoleObjects(settingsConsoleObjects());
    g_generalSettings = nullptr;
    std::printf("tutorial messages: %u/%u passed (profile/console/command line/HUD observer)\n", checks - failures, checks);
    return failures ? 1 : 0;
}
