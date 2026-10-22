// Code/Tools/BF2Editor/App/CommandLine.cpp - see CommandLine.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/CommandLine.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace bf2editor
{

EditorOptions::EditorOptions()
    : batchSaveParts(0)
    , importAll(false)
    , help(false)
    , enableAsserts(-1)
    , seh(-1)
    , useArchives(-1)
    , looseObjects(-1)
    , looseCommon(-1)
    , editableTerrain(-1)
    , hud(-1)
    , skipContentCheck(-1)
    , defaultContent(false)
    , windowWidth(-1)
    , windowHeight(-1)
    , maxFrames(0)
    , windowHidden(false)
    , cameraGiven(false)
    , gameMode("gpm_cq")
    , layerSize(64)
{
    for (int i = 0; i < 5; ++i)
        camera[i] = 0.0f;
}

namespace
{

enum SwitchId
{
    SwHelp,
    SwLoadLevel,
    SwLoadMod,
    SwImportAll,
    SwRenderer,             // kept as given for the renderer bring-up
    SwDefaultContent,
    SwCustomContent,
    SwEnableAsserts,
    SwIgnoreAsserts,
    SwUseArchives,
    SwSeh,
    SwSave,
    SwRunConFile,
    SwSkipContentCheck,
    SwForceLoadPlugin,
    // the editor's own
    SwWindowWidth,
    SwWindowHeight,
    SwFrames,
    SwWindowHidden,
    SwScreenshot,
    SwCameraAt,
    SwRunConFileKeepOpen,
    SwSettingsFile,
    SwGameMode,
    SwLayerSize,
    SwLooseObjects,
    SwLooseCommon,
    SwEditableTerrain,
    SwHud
};

struct SwitchSpec
{
    const char* name;
    SwitchId    id;
    unsigned    savePart;       // SwSave only
    const char* allowed;        // SwRenderer: accepted values, space-separated; 0 = a number >= 0
};

// The retail table (0x40D020), in its order. "seh" is in the retail table but handled outside the
// parser; it is parsed here.
const SwitchSpec kSwitches[] = {
    { "help",                   SwHelp,             0,              0 },
    { "?",                      SwHelp,             0,              0 },
    { "loadLevel",              SwLoadLevel,        0,              0 },
    { "loadMod",                SwLoadMod,          0,              0 },
    { "importAll",              SwImportAll,        0,              0 },
    { "rp",                     SwRenderer,         0,              "2 3" },
    { "deviceType",             SwRenderer,         0,              "1 2" },
    { "deviceBehavior",         SwRenderer,         0,              "32 64 80" },
    { "skipMips",               SwRenderer,         0,              0 },
    { "videoOptions",           SwRenderer,         0,              "low medium high" },
    { "mergeMaterials",         SwRenderer,         0,              0 },
    { "managedTextures",        SwRenderer,         0,              "0 1" },
    { "defaultContent",         SwDefaultContent,   0,              0 },
    { "customContent",          SwCustomContent,    0,              0 },
    { "enableAsserts",          SwEnableAsserts,    0,              0 },
    { "ignoreAsserts",          SwIgnoreAsserts,    0,              0 },
    { "useZipArchives",         SwUseArchives,      0,              0 },
    { "useArchives",            SwUseArchives,      0,              0 },
    { "seh",                    SwSeh,              0,              0 },
    { "saveAndClose",           SwSave,             SaveAll,        0 },
    { "saveTerrainAndClose",    SwSave,             SaveTerrain,    0 },
    { "saveRoadAndClose",       SwSave,             SaveRoads,      0 },
    { "saveOvergrowthAndClose", SwSave,             SaveOvergrowth, 0 },
    { "saveObjectsAndClose",    SwSave,             SaveObjects,    0 },
    { "runConFileAndClose",     SwRunConFile,       0,              0 },
    { "skipBF2ContentCheck",    SwSkipContentCheck, 0,              0 },
    { "forceLoadPlugin",        SwForceLoadPlugin,  0,              0 },
    // The editor's own switches (not in the retail table).
    { "szx",                    SwWindowWidth,      0,              0 },
    { "szy",                    SwWindowHeight,     0,              0 },
    { "frames",                 SwFrames,           0,              0 },
    { "windowHidden",           SwWindowHidden,     0,              0 },
    { "screenshot",             SwScreenshot,       0,              0 },
    { "cameraAt",               SwCameraAt,         0,              0 },
    { "runConFile",             SwRunConFileKeepOpen, 0,            0 },
    { "settingsFile",           SwSettingsFile,     0,              0 },
    { "gameMode",               SwGameMode,         0,              0 },
    { "layerSize",              SwLayerSize,        0,              "16 32 64 128" },
    { "looseObjects",           SwLooseObjects,     0,              0 },
    { "looseCommon",            SwLooseCommon,      0,              0 },
    { "editableTerrain",        SwEditableTerrain,  0,              0 },
    { "hud",                    SwHud,              0,              0 },
};

bool equalsIgnoreCase(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && _stricmp(a.c_str(), b.c_str()) == 0;
}

const SwitchSpec* findSwitch(const std::string& name)
{
    for (size_t i = 0; i < sizeof(kSwitches) / sizeof(kSwitches[0]); ++i)
    {
        if (equalsIgnoreCase(name, kSwitches[i].name))
            return &kSwitches[i];
    }
    return 0;
}

bool isNumber(const std::string& text)
{
    if (text.empty() || text.size() > 9)
        return false;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(text[i])))
            return false;
    }
    return true;
}

bool inList(const std::string& value, const char* allowed)
{
    std::string list(allowed);
    size_t start = 0;
    while (start <= list.size())
    {
        size_t end = list.find(' ', start);
        if (end == std::string::npos)
            end = list.size();
        if (equalsIgnoreCase(value, list.substr(start, end - start)))
            return true;
        start = end + 1;
    }
    return false;
}

// A 0|1 switch. Anything else is an error: these switches change what the editor does.
bool readFlag(const std::string& sw, const std::string& value, int& out, EditorOptions& options)
{
    if (value == "0" || value == "1")
    {
        out = value == "1" ? 1 : 0;
        return true;
    }
    options.errors.push_back("+" + sw + " takes 0 or 1, not '" + value + "'");
    return false;
}

void nameLevel(const std::string& sw, const std::string& value, EditorOptions& options)
{
    if (value.empty())
        options.errors.push_back("+" + sw + " needs a level name");
    else if (options.level.empty())
        options.level = value;
    else if (!equalsIgnoreCase(options.level, value))
        options.errors.push_back("+" + sw + " names level '" + value + "', but level '" + options.level
                                 + "' was already named; give one level per run");
}

bool hasExtension(const std::string& path)
{
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    return dot != std::string::npos && (slash == std::string::npos || dot > slash);
}

} // namespace

std::vector<std::string> tokenizeCommandLine(const std::string& commandLine, std::string& error)
{
    std::vector<std::string> tokens;
    error.clear();
    size_t i = 0;
    const size_t n = commandLine.size();
    while (i < n)
    {
        while (i < n && std::isspace(static_cast<unsigned char>(commandLine[i])))
            ++i;
        if (i >= n)
            break;
        if (commandLine[i] == '"')
        {
            const size_t close = commandLine.find('"', i + 1);
            if (close == std::string::npos)
            {
                error = "unterminated quote in the command line: " + commandLine.substr(i);
                break;
            }
            tokens.push_back(commandLine.substr(i + 1, close - i - 1));
            i = close + 1;
        }
        else
        {
            const size_t start = i;
            while (i < n && !std::isspace(static_cast<unsigned char>(commandLine[i])))
                ++i;
            tokens.push_back(commandLine.substr(start, i - start));
        }
    }
    return tokens;
}

EditorOptions parseCommandLine(const std::string& commandLine)
{
    EditorOptions options;
    options.commandLine = commandLine;
    std::string tokenError;
    const std::vector<std::string> tokens = tokenizeCommandLine(commandLine, tokenError);
    if (!tokenError.empty())
        options.errors.push_back(tokenError);

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const std::string& token = tokens[i];
        if (token.empty() || token[0] != '+')
        {
            options.warnings.push_back("ignored '" + token + "': it is not a switch or a switch's value");
            continue;
        }
        const std::string name = token.substr(1);

        // A switch's value is the next token unless that token is itself a switch. A quoted empty
        // string ("") is a value.
        bool hasValue = false;
        std::string value;
        if (i + 1 < tokens.size() && (tokens[i + 1].empty() || tokens[i + 1][0] != '+'))
        {
            hasValue = true;
            value = tokens[++i];
        }

        const SwitchSpec* spec = findSwitch(name);
        if (!spec)
        {
            options.warnings.push_back("unknown switch '+" + name + "' ignored");
            continue;
        }
        const std::string sw = spec->name;

        switch (spec->id)
        {
        case SwHelp:
            options.help = true;
            break;
        case SwLoadLevel:
            nameLevel(sw, value, options);
            break;
        case SwLoadMod:
            if (value.empty())
                options.errors.push_back("+loadMod needs a mod name");
            else
                options.mod = value;
            break;
        case SwImportAll:
            options.importAll = true;
            break;
        case SwRenderer:
            if (!hasValue || value.empty())
                options.warnings.push_back("+" + sw + " without a value ignored");
            else if (spec->allowed ? !inList(value, spec->allowed) : !isNumber(value))
                options.warnings.push_back("+" + sw + " '" + value + "' ignored: expected "
                                           + (spec->allowed ? std::string(spec->allowed) : std::string("a number")));
            else
                options.rendererOptions.push_back(std::make_pair(sw, value));
            break;
        case SwDefaultContent:
        {
            int flag = 0;
            if (readFlag(sw, value, flag, options))
                options.defaultContent = flag == 1;
            break;
        }
        case SwCustomContent:
            if (value.empty())
                options.errors.push_back("+customContent needs a file name");
            else
                options.customContent = hasExtension(value) ? value : value + ".ini";
            break;
        case SwEnableAsserts:
            readFlag(sw, value, options.enableAsserts, options);
            break;
        case SwIgnoreAsserts:
        {
            // Retail enables asserts only for "+ignoreAsserts 0"; the bare switch disables them.
            int ignore = 1;
            if (hasValue && !readFlag(sw, value, ignore, options))
                break;
            options.enableAsserts = ignore ? 0 : 1;
            break;
        }
        case SwUseArchives:
            readFlag(sw, value, options.useArchives, options);
            break;
        case SwLooseObjects:
            readFlag(sw, value, options.looseObjects, options);
            break;
        case SwLooseCommon:
            readFlag(sw, value, options.looseCommon, options);
            break;
        case SwEditableTerrain:
            readFlag(sw, value, options.editableTerrain, options);
            break;
        case SwHud:
            readFlag(sw, value, options.hud, options);
            break;
        case SwSeh:
            readFlag(sw, value, options.seh, options);
            break;
        case SwSave:
            nameLevel(sw, value, options);
            options.batchSaveParts |= spec->savePart;
            break;
        case SwRunConFile:
            if (value.empty())
                options.errors.push_back("+runConFileAndClose needs a script file");
            else if (!options.runConFileAndClose.empty())
                options.errors.push_back("+runConFileAndClose given twice; run one script per run");
            else
                options.runConFileAndClose = value;
            break;
        case SwSkipContentCheck:
            readFlag(sw, value, options.skipContentCheck, options);
            break;
        case SwForceLoadPlugin:
            if (value.empty())
                options.errors.push_back("+forceLoadPlugin needs a module name");
            else
                options.forceLoadModules.push_back(value);
            break;
        case SwWindowWidth:
        case SwWindowHeight:
            if (!isNumber(value) || std::atoi(value.c_str()) < 320)
                options.errors.push_back("+" + sw + " takes a size in pixels (320 or more), not '" + value + "'");
            else if (spec->id == SwWindowWidth)
                options.windowWidth = std::atoi(value.c_str());
            else
                options.windowHeight = std::atoi(value.c_str());
            break;
        case SwFrames:
            if (!isNumber(value) || std::atoi(value.c_str()) < 1)
                options.errors.push_back("+frames takes a frame count (1 or more), not '" + value + "'");
            else
                options.maxFrames = static_cast<unsigned>(std::atoi(value.c_str()));
            break;
        case SwWindowHidden:
        {
            int hidden = 0;
            if (readFlag(sw, value, hidden, options))
                options.windowHidden = hidden == 1;
            break;
        }
        case SwScreenshot:
            if (value.empty())
                options.errors.push_back("+screenshot needs a file name");
            else
                options.screenshot = value;
            break;
        case SwRunConFileKeepOpen:
            if (value.empty())
                options.errors.push_back("+runConFile needs a script");
            else if (!options.runConFile.empty())
                options.errors.push_back("+runConFile given twice");
            else
                options.runConFile = value;
            break;
        case SwSettingsFile:
            if (value.empty())
                options.errors.push_back("+settingsFile needs a file name");
            else
                options.settingsFile = value;
            break;
        case SwGameMode:
        {
            bool word = !value.empty();
            for (size_t k = 0; k < value.size(); ++k)
                word = word && (std::isalnum(static_cast<unsigned char>(value[k])) || value[k] == '_');
            if (!word)
                options.errors.push_back("+gameMode takes a game mode folder name (gpm_cq, gpm_coop, sp1...), not '"
                                         + value + "'");
            else
                options.gameMode = value;
            break;
        }
        case SwLayerSize:
            if (!inList(value, spec->allowed))
                options.errors.push_back("+layerSize takes 16, 32, 64 or 128, not '" + value + "'");
            else
                options.layerSize = std::atoi(value.c_str());
            break;
        case SwCameraAt:
        {
            float v[5];
            char tail = 0;
            if (std::sscanf(value.c_str(), "%f,%f,%f,%f,%f%c", &v[0], &v[1], &v[2], &v[3], &v[4], &tail) != 5)
            {
                options.errors.push_back("+cameraAt takes x,y,z,yaw,pitch (metres and degrees), not '" + value + "'");
                break;
            }
            for (int i = 0; i < 5; ++i)
                options.camera[i] = v[i];
            options.cameraGiven = true;
            break;
        }
        }
    }
    return options;
}

std::string usageText()
{
    return
        "BF2Editor.exe [switches]   (switch names are not case-sensitive)\n"
        "  +help, +?                       show this text\n"
        "  +loadMod <mod>                  open a mod, e.g. bf2\n"
        "  +loadLevel <level>              open a level of that mod\n"
        "  +saveAndClose <level>           open the level, save all of it, exit\n"
        "  +saveObjectsAndClose <level>    ... save only its objects\n"
        "  +saveTerrainAndClose <level>    ... save only its terrain (compiled again even when unchanged)\n"
        "  +saveRoadAndClose <level>       ... save only its roads (every editable road compiled)\n"
        "  +saveOvergrowthAndClose <level> ... save only its overgrowth, regenerated (the save switches combine;\n"
        "                                  terrain and overgrowth need the editable terrain, +editableTerrain 1)\n"
        "  +runConFileAndClose <script>    run a console script, then do any saves asked for, then exit\n"
        "  +forceLoadPlugin <module>       start an editing module that is not started by default\n"
        "  +importAll                      run the import-all command\n"
        "  +enableAsserts 0|1, +ignoreAsserts [0|1]\n"
        "  +seh 0|1                        0: no exception handler; 1: write crash dumps without asking\n"
        "  +useArchives 0|1, +useZipArchives 0|1   read content from the mod's archives (read-only)\n"
        "  +skipBF2ContentCheck 0|1, +defaultContent 0|1, +customContent <file[.ini]>\n"
        "  renderer: +skipMips <n>, +videoOptions low|medium|high, +mergeMaterials <n>,\n"
        "            +managedTextures 0|1, +rp 2|3, +deviceType 1|2, +deviceBehavior 32|64|80\n"
        "  (editor) +szx <w> +szy <h>          window client size\n"
        "  (editor) +frames <n>                stop after n frames (test runs)\n"
        "  (editor) +windowHidden 0|1          render into a hidden window (test runs)\n"
        "  (editor) +screenshot <file.bmp>     save the last frame's image\n"
        "  (editor) +cameraAt x,y,z,yaw,pitch  where the camera starts in the level (degrees)\n"
        "  (editor) +runConFile <script>       run a console script once the level is open, then keep going\n"
        "  (editor) +settingsFile <file>       the editor's settings file (default: beside the exe; a\n"
        "                                      +windowHidden run uses none unless this names one)\n"
        "  (editor) +gameMode <mode>           the gameplay layer's game mode (default gpm_cq)\n"
        "  (editor) +layerSize 16|32|64|128    its size: the largest layer up to it loads (default 64)\n"
        "  (editor) +looseObjects 0|1          1: mount the mod's archives but those at Objects, so templates are\n"
        "                                      read from (and saved to) the mod's loose Objects folder\n"
        "  (editor) +looseCommon 0|1           1: ... nor those at Common, so the material files are read from (and\n"
        "                                      saved to) the mod's loose Common folder (EditorMaterial.extract)\n"
        "  (editor) +editableTerrain 0|1       1 (the default): once the level is open, its terrain becomes the renderer's\n"
        "                                      TerrainEditable (Terrain.con's editor branch), so height edits show;\n"
        "                                      0 keeps the compiled terrain\n"
        "  (editor) +hud 0|1                   the game's HUD (Menu/HUD/HudSetup) in the view - the play test's\n"
        "                                      soldier and vehicle HUDs, and the HUD editor; on unless 0\n";
}

std::string relaunchCommandLine(const std::string& commandLine, const std::string& gameMode, int layerSize,
                                const std::string& level, const std::string& mod)
{
    static const char* const kDropped[] = { "gameMode", "layerSize", "cameraAt", "runConFile", "frames", "screenshot",
                                            "loadLevel" };
    std::string error;
    const std::vector<std::string> tokens = tokenizeCommandLine(commandLine, error);
    std::string out;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const std::string& token = tokens[i];
        const bool isSwitch = !token.empty() && token[0] == '+';
        // A switch's value is the token after it, unless that is a switch too.
        const bool hasValue = isSwitch && i + 1 < tokens.size() && !(tokens[i + 1].size() && tokens[i + 1][0] == '+');
        bool dropped = false;
        // +loadLevel stays unless the relaunch opens another level.
        const size_t droppable = level.empty() ? sizeof(kDropped) / sizeof(kDropped[0]) - 1
                                               : sizeof(kDropped) / sizeof(kDropped[0]);
        for (size_t k = 0; isSwitch && k < droppable; ++k)
            dropped = dropped || equalsIgnoreCase(token.substr(1), kDropped[k]);
        dropped = dropped || (isSwitch && !mod.empty() && equalsIgnoreCase(token.substr(1), "loadMod"));
        if (dropped)
        {
            if (hasValue)
                ++i;
            continue;
        }
        const bool quote = token.find_first_of(" \t") != std::string::npos;
        out += (out.empty() ? "" : " ") + (quote ? "\"" + token + "\"" : token);
    }
    char size[16];
    std::snprintf(size, sizeof(size), "%d", layerSize);
    return out + (out.empty() ? "" : " ") + (mod.empty() ? std::string() : "+loadMod " + mod + " ")
           + (level.empty() ? std::string() : "+loadLevel " + level + " ") + "+gameMode " + gameMode + " +layerSize " + size;
}

} // namespace bf2editor
