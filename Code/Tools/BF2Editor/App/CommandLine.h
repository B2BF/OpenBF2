// Editor command-line parsing with case-insensitive retail switch names.
// Quoted values preserve plus signs and have no fixed length limit. Unknown switches/invalid values warn;
// conflicting save/load level names fail. Scripts run before combined saves, archive use can be disabled,
// and forceLoadPlugin can repeat. These are intentional editor-only differences.
#ifndef BF2EDITOR_APP_COMMANDLINE_H
#define BF2EDITOR_APP_COMMANDLINE_H

#include "Framework/EditorConfig.h"

#include <string>
#include <utility>
#include <vector>

namespace bf2editor
{

// The parts a batch run saves before it exits (+save...AndClose). Retail passes each flag to its
// own engine save call (host 0x40DC40), so they combine.
enum BatchSavePart
{
    SaveAll        = 1 << 0,    // +saveAndClose
    SaveObjects    = 1 << 1,    // +saveObjectsAndClose
    SaveTerrain    = 1 << 2,    // +saveTerrainAndClose
    SaveRoads      = 1 << 3,    // +saveRoadAndClose
    SaveOvergrowth = 1 << 4     // +saveOvergrowthAndClose
};

struct EditorOptions
{
    std::string mod;                        // +loadMod <mod>
    std::string level;                      // +loadLevel <level>, or the level a save switch names
    unsigned    batchSaveParts;             // BatchSavePart bits
    std::string runConFileAndClose;         // +runConFileAndClose <script>
    std::vector<std::string> forceLoadModules;  // +forceLoadPlugin <name>, retail's name kept
    bool        importAll;                  // +importAll
    bool        help;                       // +help or +?
    int         enableAsserts;              // +enableAsserts 0|1, +ignoreAsserts 0|1; -1 = not given
    int         seh;                        // +seh 0|1; -1 = not given
    int         useArchives;                // +useArchives 0|1, +useZipArchives 0|1; -1 = not given
    int         looseObjects;               // +looseObjects 0|1 (editor): leave the object archives unmounted; -1 = not given
    int         looseCommon;                // +looseCommon 0|1 (editor): leave the Common archives unmounted; -1 = not given
    int         editableTerrain;            // +editableTerrain 0|1 (editor): the renderer's TerrainEditable; -1 = not given
    int         hud;                        // +hud 0|1 (editor): the game's HUD in the view (on unless 0); -1 = not given
    int         skipContentCheck;           // +skipBF2ContentCheck 0|1; -1 = not given
    bool        defaultContent;             // +defaultContent 1
    std::string customContent;              // +customContent <file>; ".ini" appended when it has no extension

    // Renderer options, kept as given (validated against the retail values) for the renderer
    // bring-up to apply: skipMips, videoOptions, mergeMaterials, managedTextures, rp, deviceType,
    // deviceBehavior. Name first, value second, in command-line order.
    std::vector<std::pair<std::string, std::string> > rendererOptions;

    // The editor's own switches (not retail).
    int         windowWidth;                // +szx <w>: client width (the game's switch name); -1 = default
    int         windowHeight;               // +szy <h>
    unsigned    maxFrames;                  // +frames <n>: leave the frame loop after n frames; 0 = run
    bool        windowHidden;               // +windowHidden 1: render into a hidden window (automated runs)
    std::string screenshot;                 // +screenshot <file.bmp>: save the last frame's back buffer
    bool        cameraGiven;                // +cameraAt x,y,z,yaw,pitch: where the camera starts (degrees)
    float       camera[5];
    std::string runConFile;                 // +runConFile <script>: run a console script once the level is
                                            // open, then keep going (retail has only +runConFileAndClose)
    std::string settingsFile;               // +settingsFile <path>: the editor's settings, read and written
                                            // even in a +windowHidden run (default: beside the exe)
    std::string gameMode;                   // +gameMode <mode>: the gameplay layer's mode (default gpm_cq)
    int         layerSize;                  // +layerSize 16|32|64|128: its size (default 64); GameLogic::load
                                            // takes the largest layer up to it
    std::string commandLine;                // as given, for a relaunch on another layer

    std::vector<std::string> warnings;      // accepted but questionable input
    std::vector<std::string> errors;        // input the editor must refuse to start with

    EditorOptions();

    bool isBatch() const { return batchSaveParts != 0 || !runConFileAndClose.empty(); }
};

// Splits a command line into tokens: whitespace separates them, and a token that starts with a
// double quote runs to the next double quote (the quotes are removed). An unterminated quote is
// reported in `error` and ends tokenizing.
std::vector<std::string> tokenizeCommandLine(const std::string& commandLine, std::string& error);

EditorOptions parseCommandLine(const std::string& commandLine);

// The +help text.
std::string usageText();

// The command line a relaunch on another gameplay layer runs: the same switches without the ones it
// replaces or must not repeat - +gameMode, +layerSize, +cameraAt (the camera comes back from the
// settings), +runConFile, +frames, +screenshot - then +gameMode and +layerSize; with `level` (Save level
// as), +loadLevel is replaced too; with `mod` (the mod manager's Open), +loadMod.
std::string relaunchCommandLine(const std::string& commandLine, const std::string& gameMode, int layerSize,
                                const std::string& level = std::string(), const std::string& mod = std::string());

} // namespace bf2editor

#endif // BF2EDITOR_APP_COMMANDLINE_H
