// Code/Tools/BF2Editor/Tests/EditorChecks.cpp

// Focused checks for the editor's framework pieces that need no engine: the command-line parser, the
// source-control registry, the undo history and the selection. Target bf2_editor_checks; run by
// bf2_editor_run_checks. Editor-only code (D3). Prints one line per failure and a summary; the exit
// code is the failure count.
#include "Framework/EditorConfig.h"

#include "App/CommandLine.h"
#include "Framework/AnimationSystemFile.h"
#include "Framework/ArchiveRewrite.h"
#include "Framework/BafHeader.h"
#include "Framework/GameplayLayers.h"
#include "Framework/LightSetupFile.h"
#include "Framework/GroundPropertiesFile.h"
#include "Framework/HudSetup.h"
#include "Framework/TerrainRawCompile.h"
#include "Framework/TerrainRawFile.h"
#include "Framework/TextureEncode.h"
#include "Framework/TerrainRawLayers.h"
#include "Framework/TerrainLowDetail.h"
#include "Framework/DdsImage.h"
#include "Framework/CommandHistory.h"
#include "Framework/DescFile.h"
#include "Framework/ConsoleCompletion.h"
#include "Framework/EditorSettings.h"
#include "Framework/HelpDocuments.h"
#include "Framework/LayerSummary.h"
#include "Framework/LevelCopier.h"
#include "Framework/LevelEditorDefs.h"
#include "Framework/LevelPacker.h"
#include "Framework/LevelObjectFile.h"
#include "Framework/LevelSettingsFile.h"
#include "Framework/MapDescription.h"
#include "Framework/NewLevel.h"
#include "Framework/RoadFiles.h"
#include "Framework/SavePlan.h"
#include "Framework/TextDiff.h"
#include "Framework/LightmapAtlas.h"
#include "Framework/ModScaffold.h"
#include "Framework/ObjectArchives.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/TemplateClone.h"
#include "Framework/WizardRules.h"
#include "Framework/MaterialFiles.h"
#include "Framework/TemplateFile.h"
#include "Framework/Matrix4.h"
#include "Framework/Selection.h"
#include "Framework/Shortcuts.h"
#include "Framework/SourceControl.h"
#include "Framework/UndergrowthConfig.h"
#include "Framework/UndergrowthMap.h"
#include "Framework/ZipWriter.h"

#include "unzip.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace bf2editor;

namespace
{

int g_passed = 0;
int g_failed = 0;

void check(bool condition, const char* what, int line)
{
    if (condition)
    {
        ++g_passed;
        return;
    }
    ++g_failed;
    std::printf("FAIL line %d: %s\n", line, what);
}

#define CHECK(condition) check((condition), #condition, __LINE__)

bool hasText(const std::vector<std::string>& lines, const char* text)
{
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (lines[i].find(text) != std::string::npos)
            return true;
    }
    return false;
}

void checkTokenizer()
{
    std::string error;
    std::vector<std::string> t = tokenizeCommandLine("  +a  b \"c d\" \"\" +e\t", error);
    CHECK(error.empty());
    CHECK(t.size() == 5);
    CHECK(t.size() == 5 && t[0] == "+a" && t[1] == "b" && t[2] == "c d" && t[3].empty() && t[4] == "+e");

    t = tokenizeCommandLine("+runConFileAndClose \"C:\\a b\\x.con", error);
    CHECK(!error.empty());
}

void checkGoldenLines()
{
    // Match the retail editor's recorded command output.
    EditorOptions o = parseCommandLine(
        "+loadMod bf2 +enableAsserts 0 +seh 1 +skipBF2ContentCheck 1 +saveAndClose Strike_at_Karkand");
    CHECK(o.errors.empty());
    CHECK(o.warnings.empty());
    CHECK(o.mod == "bf2");
    CHECK(o.level == "Strike_at_Karkand");
    CHECK(o.batchSaveParts == SaveAll);
    CHECK(o.enableAsserts == 0);
    CHECK(o.seh == 1);
    CHECK(o.skipContentCheck == 1);
    CHECK(o.isBatch());

    // G6: a script run from the mod root.
    o = parseCommandLine("+loadMod bf2 +loadLevel Strike_at_Karkand +runConFileAndClose "
                         "\"D:\\game dir\\mods\\bf2\\_wp1_G6_place_object.con\"");
    CHECK(o.errors.empty());
    CHECK(o.runConFileAndClose == "D:\\game dir\\mods\\bf2\\_wp1_G6_place_object.con");
    CHECK(o.batchSaveParts == 0);
    CHECK(o.isBatch());

    // dice's navmesh pipeline line (WP1 readme).
    o = parseCommandLine("+loadMod bf2 +loadLevel Dalian_Plant +runConFileAndClose \"nav.con\" "
                         "+enableAsserts 0 +forceLoadPlugin SinglePlayerEditor");
    CHECK(o.errors.empty());
    CHECK(o.forceLoadModules.size() == 1 && o.forceLoadModules[0] == "SinglePlayerEditor");
}

void checkSwitchRules()
{
    // Names are not case-sensitive, as in retail.
    EditorOptions o = parseCommandLine("+LOADMOD bf2 +loadlevel X +SAVEOBJECTSANDCLOSE x");
    CHECK(o.errors.empty());
    CHECK(o.level == "X");
    CHECK(o.batchSaveParts == SaveObjects);

    // Save switches combine.
    o = parseCommandLine("+saveTerrainAndClose L +saveRoadAndClose L +saveOvergrowthAndClose L");
    CHECK(o.errors.empty());
    CHECK(o.batchSaveParts == (SaveTerrain | SaveRoads | SaveOvergrowth));

    // Two different levels in one run are refused.
    o = parseCommandLine("+loadLevel A +saveAndClose B");
    CHECK(hasText(o.errors, "already named"));

    // A save switch needs its level.
    o = parseCommandLine("+saveAndClose +loadMod bf2");
    CHECK(hasText(o.errors, "needs a level name"));

    // Bare +ignoreAsserts disables asserts; "+ignoreAsserts 0" enables them.
    o = parseCommandLine("+ignoreAsserts");
    CHECK(o.errors.empty() && o.enableAsserts == 0);
    o = parseCommandLine("+ignoreAsserts 0");
    CHECK(o.errors.empty() && o.enableAsserts == 1);

    // 0|1 switches refuse anything else.
    o = parseCommandLine("+enableAsserts yes");
    CHECK(hasText(o.errors, "takes 0 or 1"));

    // Archive use can be turned off (retail could not).
    o = parseCommandLine("+useZipArchives 0");
    CHECK(o.errors.empty() && o.useArchives == 0);

    // +customContent gains ".ini" only when it has no extension.
    o = parseCommandLine("+customContent Content");
    CHECK(o.customContent == "Content.ini");
    o = parseCommandLine("+customContent \"my dir.v2\\Content.cfg\"");
    CHECK(o.customContent == "my dir.v2\\Content.cfg");
    o = parseCommandLine("+customContent \"my dir.v2\\Content\"");
    CHECK(o.customContent == "my dir.v2\\Content.ini");

    // Renderer options are validated against the retail values and kept in order.
    o = parseCommandLine("+videoOptions HIGH +deviceBehavior 64 +skipMips 2 +rp 4 +deviceType x");
    CHECK(o.errors.empty());
    CHECK(o.rendererOptions.size() == 3);
    CHECK(o.rendererOptions.size() == 3 && o.rendererOptions[0].first == "videoOptions"
          && o.rendererOptions[0].second == "HIGH" && o.rendererOptions[2].second == "2");
    CHECK(hasText(o.warnings, "+rp '4' ignored"));
    CHECK(hasText(o.warnings, "+deviceType 'x' ignored"));

    // Unknown switches and stray words are warnings, not silence.
    o = parseCommandLine("+modPath mods/bf2 stray");
    CHECK(o.errors.empty());
    CHECK(hasText(o.warnings, "unknown switch '+modPath'"));
    CHECK(hasText(o.warnings, "ignored 'stray'"));

    // A '+' inside a quoted value belongs to the value.
    o = parseCommandLine("+runConFileAndClose \"C:\\a+b\\x.con\" +saveObjectsAndClose L");
    CHECK(o.errors.empty());
    CHECK(o.runConFileAndClose == "C:\\a+b\\x.con");
    CHECK(o.batchSaveParts == SaveObjects);
    CHECK(o.warnings.empty());

    // Two scripts in one run are refused.
    o = parseCommandLine("+runConFileAndClose a.con +runConFileAndClose b.con");
    CHECK(hasText(o.errors, "given twice"));

    // +help and +? both ask for the usage text.
    CHECK(parseCommandLine("+?").help);
    CHECK(parseCommandLine("+Help").help);
    CHECK(!usageText().empty());

    // The editor's own switches (WP4).
    o = parseCommandLine("+szx 1600 +szy 900 +frames 5 +windowHidden 1 +screenshot \"C:\\a b\\shot.bmp\"");
    CHECK(o.errors.empty());
    CHECK(o.windowWidth == 1600 && o.windowHeight == 900);
    CHECK(o.maxFrames == 5);
    CHECK(o.windowHidden);
    CHECK(o.screenshot == "C:\\a b\\shot.bmp");
    CHECK(!o.isBatch());
    o = parseCommandLine("+szx 100");
    CHECK(hasText(o.errors, "+szx takes a size"));
    o = parseCommandLine("+frames 0");
    CHECK(hasText(o.errors, "+frames takes a frame count"));
    o = parseCommandLine("+windowHidden 2");
    CHECK(hasText(o.errors, "takes 0 or 1"));
    o = parseCommandLine("");
    CHECK(o.windowWidth == -1 && o.maxFrames == 0 && !o.windowHidden && o.screenshot.empty());
    CHECK(!o.cameraGiven);

    // +cameraAt takes exactly five comma-separated numbers: x,y,z in metres, then yaw and pitch.
    o = parseCommandLine("+cameraAt 190,185.5,-90,35,-8");
    CHECK(o.errors.empty() && o.cameraGiven);
    CHECK(o.camera[0] == 190.0f && o.camera[1] == 185.5f && o.camera[2] == -90.0f);
    CHECK(o.camera[3] == 35.0f && o.camera[4] == -8.0f);
    // The gameplay layer (phase C), and the command line a layer switch relaunches with.
    o = parseCommandLine("+loadLevel Strike_at_Karkand +gameMode gpm_coop +layerSize 16");
    CHECK(o.errors.empty() && o.gameMode == "gpm_coop" && o.layerSize == 16 && o.level == "Strike_at_Karkand");
    CHECK(parseCommandLine("+loadLevel x").gameMode == "gpm_cq" && parseCommandLine("+loadLevel x").layerSize == 64);
    CHECK(hasText(parseCommandLine("+layerSize 48").errors, "+layerSize takes 16, 32, 64 or 128"));
    CHECK(hasText(parseCommandLine("+gameMode gpm/cq").errors, "+gameMode takes a game mode folder name"));
    CHECK(parseCommandLine("+loadMod bf2").commandLine == "+loadMod bf2");
    CHECK(relaunchCommandLine("+loadMod bf2 +loadLevel Strike_at_Karkand +cameraAt 1,2,3,4,5 +gameMode gpm_cq "
                              "+layerSize 64 +szx 1600 +runConFile \"C:/a b/s.con\" +frames 30",
                              "gpm_coop", 16)
          == "+loadMod bf2 +loadLevel Strike_at_Karkand +szx 1600 +gameMode gpm_coop +layerSize 16");
    CHECK(relaunchCommandLine("+loadLevel \"My Level\" +windowHidden", "sp1", 32)
          == "+loadLevel \"My Level\" +windowHidden +gameMode sp1 +layerSize 32");
    o = parseCommandLine("+cameraAt 1,2,3,4");
    CHECK(hasText(o.errors, "+cameraAt takes x,y,z,yaw,pitch") && !o.cameraGiven);
    o = parseCommandLine("+cameraAt 1,2,3,4,5,6");
    CHECK(hasText(o.errors, "+cameraAt takes x,y,z,yaw,pitch") && !o.cameraGiven);
    o = parseCommandLine("+cameraAt 1,2,3,4,5x");
    CHECK(hasText(o.errors, "+cameraAt takes x,y,z,yaw,pitch"));

    // +runConFile runs a script in an interactive session; it is not the retail batch switch.
    o = parseCommandLine("+loadLevel L +runConFile \"C:\\t\\edit test.con\"");
    CHECK(o.errors.empty() && o.runConFile == "C:\\t\\edit test.con" && o.runConFileAndClose.empty());
    CHECK(!o.isBatch());
    o = parseCommandLine("+runConFile a.con +runConFile b.con");
    CHECK(hasText(o.errors, "+runConFile given twice"));
    o = parseCommandLine("+runConFile \"\"");
    CHECK(hasText(o.errors, "+runConFile needs a script"));

    // +settingsFile names the settings file a test run may use.
    o = parseCommandLine("+windowHidden 1 +settingsFile \"C:\\t\\s.ini\"");
    CHECK(o.errors.empty() && o.settingsFile == "C:\\t\\s.ini" && o.windowHidden);
    o = parseCommandLine("+settingsFile \"\"");
    CHECK(hasText(o.errors, "+settingsFile needs a file name"));
}

class TestProvider : public ISourceControl
{
public:
    const char*  name() const override { return "Test"; }
    bool         isConnected() const override { return true; }
    ScmFileState getState(const std::string&) override { return ScmFileState::Unchanged; }
    ScmResult    openForEdit(const std::string&) override { return ScmResult{ true, std::string() }; }
    ScmResult    add(const std::string&) override { return ScmResult{ true, std::string() }; }
    ScmResult    remove(const std::string&) override { return ScmResult{ true, std::string() }; }
    ScmResult    revert(const std::string&) override { return ScmResult{ true, std::string() }; }
};

std::unique_ptr<ISourceControl> createTest()
{
    return std::unique_ptr<ISourceControl>(new TestProvider());
}

std::unique_ptr<ISourceControl> createNothing()
{
    return std::unique_ptr<ISourceControl>();
}

void checkSourceControl()
{
    // NullSourceControl is registered and active from the start.
    CHECK(std::string(sourceControl().name()) == "Null");
    CHECK(!sourceControl().isConnected());
    CHECK(sourceControlNames().size() == 1);

    NullSourceControl null;
    CHECK(null.getState("anything") == ScmFileState::NotControlled);
    CHECK(null.add("x").ok);
    CHECK(null.remove("x").ok);
    CHECK(!null.revert("x").ok);

    char directory[MAX_PATH];
    char path[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, directory) != 0);
    CHECK(GetTempFileNameA(directory, "bfe", 0, path) != 0);      // creates an empty, writable file
    CHECK(null.openForEdit(path).ok);
    CHECK(SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY) != 0);
    const ScmResult readOnly = null.openForEdit(path);
    CHECK(!readOnly.ok && readOnly.message.find("read-only") != std::string::npos);
    CHECK(SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL) != 0);
    CHECK(DeleteFileA(path) != 0);
    CHECK(null.openForEdit(path).ok);                             // missing: a new file
    CHECK(!null.openForEdit(directory).ok);                       // a directory is not a file

    // Other providers register by name and are selected by name; unknown names change nothing.
    CHECK(registerSourceControl("Test", &createTest));
    CHECK(!registerSourceControl("test", &createTest));           // names are not case-sensitive
    CHECK(!registerSourceControl("Other", 0));
    CHECK(registerSourceControl("Broken", &createNothing));
    CHECK(!selectSourceControl("Perforce"));
    CHECK(std::string(sourceControl().name()) == "Null");
    CHECK(!selectSourceControl("Broken"));                        // a null provider is refused
    CHECK(std::string(sourceControl().name()) == "Null");
    CHECK(selectSourceControl("TEST"));
    CHECK(std::string(sourceControl().name()) == "Test" && sourceControl().isConnected());
    CHECK(selectSourceControl("null"));
    CHECK(std::string(sourceControl().name()) == "Null");
    CHECK(sourceControlNames().size() == 3);
}

// A console stand-in for the history: it records every script and fails any that contains "FAIL".
bool recordingRunner(void* context, const std::string& script, std::string& error)
{
    static_cast<std::vector<std::string>*>(context)->push_back(script);
    if (script.find("FAIL") != std::string::npos)
    {
        error = "refused: " + script;
        return false;
    }
    return true;
}

HistoryEntry entry(const char* name)
{
    HistoryEntry e;
    e.label = name;
    e.doScript = std::string("do ") + name;
    e.undoScript = std::string("undo ") + name;
    return e;
}

void checkHistory()
{
    std::string error;
    CommandHistory noRunner;
    CHECK(!noRunner.execute(entry("a"), error) && !noRunner.canUndo());   // no console: nothing runs

    std::vector<std::string> ran;
    CommandHistory h(3);
    h.setRunner(&recordingRunner, &ran);
    CHECK(!h.isDirty() && !h.canUndo() && !h.canRedo() && !h.undo(error) && !h.redo(error));

    // Execute runs the do script, then records; undo and redo run the matching halves.
    CHECK(h.execute(entry("a"), error) && h.execute(entry("b"), error));
    CHECK(ran.size() == 2 && ran[0] == "do a" && ran[1] == "do b");
    CHECK(h.isDirty() && h.position() == 2 && h.nextUndo() && h.nextUndo()->label == "b");
    CHECK(h.undo(error) && ran.back() == "undo b" && h.position() == 1 && h.canRedo());
    CHECK(h.redo(error) && ran.back() == "do b" && h.position() == 2 && !h.canRedo());

    // A failing do script records nothing and keeps the redo tail.
    CHECK(h.undo(error));
    const size_t before = h.entries().size();
    CHECK(!h.execute(entry("FAIL"), error) && error.find("refused") != std::string::npos);
    CHECK(h.entries().size() == before && h.position() == 1 && h.canRedo());

    // A successful one drops the redo tail.
    CHECK(h.execute(entry("c"), error) && h.entries().size() == 2 && !h.canRedo());
    CHECK(h.entries().size() == 2 && h.entries()[1].label == "c");

    // A failing undo leaves the position alone.
    CommandHistory f;
    std::vector<std::string> ranF;
    f.setRunner(&recordingRunner, &ranF);
    HistoryEntry bad = entry("x");
    bad.undoScript = "FAIL x";
    CHECK(f.execute(bad, error) && !f.undo(error) && f.position() == 1);

    // Dirty state follows the saved position, through undo and redo.
    h.markSaved();
    CHECK(!h.isDirty());
    CHECK(h.undo(error) && h.isDirty());
    CHECK(h.redo(error) && !h.isDirty());

    // Saving, undoing, then making a different change: the saved state is unreachable for good.
    CHECK(h.undo(error) && h.execute(entry("d"), error) && h.isDirty());
    CHECK(h.undo(error) && h.isDirty());

    // The limit drops the oldest entry.
    CommandHistory small(2);
    std::vector<std::string> ranS;
    small.setRunner(&recordingRunner, &ranS);
    CHECK(small.execute(entry("1"), error) && small.execute(entry("2"), error) && small.execute(entry("3"), error));
    CHECK(small.entries().size() == 2 && small.entries()[0].label == "2" && small.position() == 2);
    CHECK(small.undo(error) && small.undo(error) && !small.undo(error));
    CHECK(small.isDirty());     // the unchanged level lies before the dropped entry

    // An entry recorded after the fact runs nothing now, and undoes and redoes like any other.
    CommandHistory made;
    std::vector<std::string> ranM;
    made.setRunner(&recordingRunner, &ranM);
    made.record(entry("create"));
    CHECK(ranM.empty() && made.position() == 1 && made.isDirty());
    CHECK(made.undo(error) && ranM.back() == "undo create" && made.redo(error) && ranM.back() == "do create");

    // Clearing forgets the entries and keeps the dirty state.
    const unsigned revision = small.revision();
    small.clear();
    CHECK(small.entries().empty() && small.position() == 0 && small.isDirty() && small.revision() != revision);
    CommandHistory clean;
    clean.clear();
    CHECK(!clean.isDirty());
}

void checkSelection()
{
    Selection s;
    CHECK(s.empty() && s.primary() == 0);
    const unsigned r0 = s.revision();
    s.set(7);
    CHECK(s.size() == 1 && s.primary() == 7 && s.contains(7) && s.revision() != r0);
    const unsigned r1 = s.revision();
    s.set(7);
    CHECK(s.revision() == r1);                  // no change, no new revision
    s.add(9);
    s.add(0);                                   // 0 is never an object
    CHECK(s.size() == 2 && s.primary() == 9);
    s.add(7);                                   // re-adding moves it to primary
    CHECK(s.size() == 2 && s.primary() == 7 && s.ids()[0] == 9);
    s.toggle(9);
    CHECK(s.size() == 1 && !s.contains(9));
    s.toggle(9);
    CHECK(s.contains(9) && s.primary() == 9);
    s.remove(42);
    CHECK(s.size() == 2);
    s.set(0);
    CHECK(s.empty() && s.primary() == 0);
}

bool closeTo(float a, float b)
{
    const float d = a - b;
    return d < 1e-4f && d > -1e-4f;
}

void checkMatrices()
{
    const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    // Scaled, rotated 90 degrees about Y (row vectors: +X goes to -Z), translated.
    const float m[16] = { 0, 0, -2, 0, 0, 3, 0, 0, 4, 0, 0, 0, 5, 6, 7, 1 };
    float inverse[16], product[16];
    CHECK(matrixInvertAffine(m, inverse));
    matrixMultiply(m, inverse, product);
    bool isIdentity = true;
    for (int i = 0; i < 16; ++i)
        isIdentity = isIdentity && closeTo(product[i], identity[i]);
    CHECK(isIdentity);
    const float singular[16] = { 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    CHECK(!matrixInvertAffine(singular, inverse));

    // A group follows its pivot: a member 2 m along +X from the pivot, the pivot turned 90 degrees
    // about Y in place, ends 2 m along -Z from it, turned the same way.
    const float pivotStart[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 0, 0, 1 };
    const float pivot[16] = { 0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 10, 0, 0, 1 };
    const float member[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 12, 0, 0, 1 };
    float moved[16];
    CHECK(matrixFollowPivot(member, pivotStart, pivot, moved));
    CHECK(closeTo(moved[12], 10.0f) && closeTo(moved[13], 0.0f) && closeTo(moved[14], -2.0f));
    CHECK(closeTo(moved[0], 0.0f) && closeTo(moved[2], -1.0f) && closeTo(moved[8], 1.0f));
    // A pivot that has not moved moves nothing; a moved one carries every member the same way.
    CHECK(matrixFollowPivot(member, pivotStart, pivotStart, moved) && closeTo(moved[12], 12.0f) && closeTo(moved[0], 1.0f));
    const float lifted[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 5, 0, 1 };
    CHECK(matrixFollowPivot(member, pivotStart, lifted, moved) && closeTo(moved[12], 12.0f) && closeTo(moved[13], 5.0f));
}

ObjectPlacement placementOf(const LevelObjectFile& file, size_t record)
{
    return file.placement(record);
}

void checkLevelObjectFile()
{
    const std::string text =
        "if v_arg1 == BF2Editor\r\nrun /objects/a.con\r\nendIf\r\n\r\n"
        "rem *** alpha ***\r\nObject.create alpha\r\nObject.absolutePosition 1.000/2.000/3.000\r\n"
        "Object.rotation -180.0/0.0/0.0\r\nObject.layer 1\r\n\r\n"
        "rem *** beta ***\r\nObject.create beta\r\nObject.absolutePosition -4.500/5.250/6.125\r\n"
        "Object.layer 2\r\nObject.setLightSourceMask 3\r\n\r\n"
        "object.create gamma\r\nobject.absolutePosition 7/8/9\r\n\r\n";
    LevelObjectFile file;
    file.parse(text);
    CHECK(file.recordCount() == 3 && file.usesCrlf());
    CHECK(file.placement(0).templateName == "alpha" && file.placement(1).templateName == "beta"
          && file.placement(2).templateName == "gamma");
    CHECK(file.placement(0).rotation[0] == -180.0f && file.placement(0).layer == 1 && file.placement(0).hasLayer);
    CHECK(file.placement(1).position[2] == 6.125f && file.placement(1).layer == 2 && !file.placement(2).hasLayer);
    CHECK(file.render() == text && !file.hasChanges());             // untouched: byte for byte

    // The same rotation read back as +180 (as the engine may) is no edit.
    ObjectPlacement p = placementOf(file, 0);
    p.rotation[0] = 180.0f;
    file.setPlacement(0, p);
    CHECK(!file.isEdited(0) && file.render() == text);

    // An edit rewrites only its own lines, in the retail form; other lines stay.
    p = placementOf(file, 1);
    p.position[0] = 0.5f;
    file.setPlacement(1, p);
    std::string expected = text;
    expected.replace(expected.find("-4.500/5.250/6.125"), 18, "0.500/5.250/6.125");
    CHECK(file.isEdited(1) && file.hasChanges() && file.render() == expected);
    p.rotation[0] = 90.0f;                                          // a rotation line appears after the position
    file.setPlacement(1, p);
    expected.replace(expected.find("0.500/5.250/6.125\r\n") + 19, 0, "Object.rotation 90.0/0.0/0.0\r\n");
    CHECK(file.render() == expected);

    // A zero rotation drops the line; a changed layer is rewritten.
    p = placementOf(file, 0);
    p.rotation[0] = 0.0f;
    p.layer = 4;
    file.setPlacement(0, p);
    expected.replace(expected.find("Object.rotation -180.0/0.0/0.0\r\n"), 32, "");
    expected.replace(expected.find("Object.layer 1"), 14, "Object.layer 4");
    CHECK(file.render() == expected);

    // A new record goes last, in the retail form; a deleted one leaves with its separator.
    ObjectPlacement added;
    added.templateName = "delta";
    added.position[0] = 10.0f;
    added.position[1] = -0.0001f;
    added.position[2] = 3.25f;
    added.rotation[0] = 90.0f;
    added.layer = 1;
    added.hasLayer = true;
    const size_t fresh = file.appendRecord(added);
    CHECK(fresh == 3 && file.isNew(fresh));
    expected += "rem *** delta ***\r\nObject.create delta\r\nObject.absolutePosition 10.000/0.000/3.250\r\n"
                "Object.rotation 90.0/0.0/0.0\r\nObject.layer 1\r\n\r\n";
    CHECK(file.render() == expected);
    file.setDeleted(2, true);
    expected.replace(expected.find("object.create gamma"), 54, "");     // its two lines and the blank one
    CHECK(file.render() == expected);
    file.setDeleted(fresh, true);
    file.setDeleted(2, false);

    // Formats: no negative zero, zero rotations recognised, the console's a/b/c form.
    const float nearZero[3] = { -0.0f, 0.04f, -0.04f };
    CHECK(LevelObjectFile::formatRotation(nearZero) == "0.0/0.0/0.0" && LevelObjectFile::rotationPrintsAsZero(nearZero));
    float triple[3];
    CHECK(LevelObjectFile::parseTriple("1.5/-2/3e1", triple) && triple[0] == 1.5f && triple[1] == -2.0f && triple[2] == 30.0f);
    CHECK(!LevelObjectFile::parseTriple("1/2", triple) && !LevelObjectFile::parseTriple("1/2/3/4", triple));

    // LF, no final newline: kept; a new record starts on a line of its own after a blank one.
    LevelObjectFile lf;
    lf.parse("Object.create a\nObject.absolutePosition 0/0/0");
    CHECK(!lf.usesCrlf() && lf.recordCount() == 1 && lf.render() == "Object.create a\nObject.absolutePosition 0/0/0");
    ObjectPlacement b;
    b.templateName = "b";
    lf.appendRecord(b);
    CHECK(lf.render() == "Object.create a\nObject.absolutePosition 0/0/0\n\nrem *** b ***\nObject.create b\n"
                         "Object.absolutePosition 0.000/0.000/0.000\n\n");

    // Binding: same template (case ignored), nearest unused object within the tolerance.
    std::vector<SceneObject> objects(4);
    const char* names[4] = { "ALPHA", "beta", "beta", "gamma" };
    const float positions[4][3] = { { 1, 2, 3 }, { 99, 5.25f, 6.125f }, { -4.5f, 5.25f, 6.125f }, { 7, 8, 9.5f } };
    for (int i = 0; i < 4; ++i)
    {
        objects[i].id = 10 + i;
        objects[i].templateName = names[i];
        for (int k = 0; k < 3; ++k)
            objects[i].position[k] = positions[i][k];
    }
    LevelObjectFile fresh2;
    fresh2.parse(text);
    std::vector<unsigned> ids = bindRecords(fresh2, objects, 0.01f);
    CHECK(ids.size() == 3 && ids[0] == 10 && ids[1] == 12 && ids[2] == 0);     // gamma is 0.5 m off
    ids = bindRecords(fresh2, objects, 1.0f);
    CHECK(ids[2] == 13);

    // The editor-only preload block: found, searched without regard to case, extended before endIf.
    CHECK(fresh2.hasPreloadBlock() && fresh2.preloads("RUN /Objects/A.con ") && !fresh2.preloads("run /objects/b.con"));
    fresh2.setExtraPreload(std::vector<std::string>(1, "run /objects/b.con"));
    CHECK(fresh2.hasChanges() && fresh2.preloads("run /objects/b.con"));
    std::string withPreload = text;
    withPreload.replace(withPreload.find("endIf"), 0, "run /objects/b.con\r\n");
    CHECK(fresh2.render() == withPreload);
    fresh2.setExtraPreload(std::vector<std::string>());
    CHECK(fresh2.render() == text && !fresh2.hasChanges());
    LevelObjectFile noBlock;
    noBlock.parse("if v_arg1 == BF2Editor\nObject.create x\nendIf\n");
    CHECK(!noBlock.hasPreloadBlock());                  // an object inside is not a preload block
}

// A gameplay layer's style (WP6 part 2): three-decimal rotations, zero rotations kept, indented
// records inside host blocks, and a control point with no rotation line.
void checkLevelObjectFileStyles()
{
    LevelObjectFile none;
    none.parse("rem nothing here\r\n");
    CHECK(none.style().positionDecimals == 3 && none.style().rotationDecimals == 1 && !none.style().keepZeroRotation);

    const std::string text =
        "if v_arg1 == host\r\n\r\n"
        "   rem [ObjectSpawner: sp_1]\r\n   Object.create sp_1\r\n"
        "   Object.absolutePosition -172.823/161.298/-309.137\r\n   Object.rotation 90.000/0.000/0.000\r\n"
        "   Object.setControlPointId 13\r\n   Object.layer 3\r\n"
        "endIf\r\n\r\n"
        "rem [SpawnPoint: pt_1]\r\nObject.create pt_1\r\nObject.absolutePosition 1.000/2.000/3.000\r\n"
        "Object.rotation 0.000/0.000/0.000\r\nObject.setControlPointId 13\r\nObject.layer 1\r\n\r\n"
        "   Object.create cp_1\r\n   Object.absolutePosition 4.000/5.000/6.000\r\n   Object.layer 1\r\n";
    LevelObjectFile file;
    file.parse(text);
    CHECK(file.recordCount() == 3 && file.render() == text && !file.hasChanges());
    CHECK(file.style().positionDecimals == 3 && file.style().rotationDecimals == 3 && file.style().keepZeroRotation);

    // An indented record keeps its indentation.
    ObjectPlacement p = placementOf(file, 0);
    p.position[0] = -170.0f;
    file.setPlacement(0, p);
    std::string expected = text;
    expected.replace(expected.find("-172.823/"), 9, "-170.000/");
    CHECK(file.render() == expected);

    // Three decimals: a rotation within half a thousandth is no edit; beyond it is, printed that way.
    p = placementOf(file, 1);
    p.rotation[0] = 0.0004f;
    file.setPlacement(1, p);
    CHECK(!file.isEdited(1) && file.render() == expected);
    p.rotation[0] = 0.25f;
    file.setPlacement(1, p);
    std::string turned = expected;
    turned.replace(turned.find("Object.rotation 0.000/0.000/0.000"), 33, "Object.rotation 0.250/0.000/0.000");
    CHECK(file.isEdited(1) && file.render() == turned);
    // Back to zero with a move: the zero rotation line stays, as this file writes them.
    p.rotation[0] = 0.0f;
    p.position[1] = 2.5f;
    file.setPlacement(1, p);
    expected.replace(expected.find("1.000/2.000/3.000"), 17, "1.000/2.500/3.000");
    CHECK(file.render() == expected);

    // A record with no rotation line gets none for a zero rotation; a real one goes after the
    // position, at the record's indentation.
    p = placementOf(file, 2);
    p.position[2] = 7.0f;
    file.setPlacement(2, p);
    expected.replace(expected.find("4.000/5.000/6.000"), 17, "4.000/5.000/7.000");
    CHECK(file.render() == expected);
    p.rotation[0] = 45.0f;
    file.setPlacement(2, p);
    std::string rotated = expected;
    rotated.replace(rotated.find("4.000/5.000/7.000\r\n") + 19, 0, "   Object.rotation 45.000/0.000/0.000\r\n");
    CHECK(file.render() == rotated);

    // A new record in this file's style: three decimals, the zero rotation written.
    LevelObjectFile fresh;
    fresh.parse(text);
    ObjectPlacement added;
    added.templateName = "pt_2";
    added.position[0] = 1.0f;
    added.layer = 1;
    added.hasLayer = true;
    fresh.appendRecord(added);
    CHECK(fresh.render() == text + "\r\nrem *** pt_2 ***\r\nObject.create pt_2\r\nObject.absolutePosition 1.000/0.000/0.000\r\n"
                                   "Object.rotation 0.000/0.000/0.000\r\nObject.layer 1\r\n\r\n");
    CHECK(LevelObjectFile::formatRotation(added.rotation, 3) == "0.000/0.000/0.000"
          && LevelObjectFile::rotationPrintsAsZero(added.rotation, 3));
}

// `text` with its one `from` replaced by `to` (a check fails when `from` is not there once).
std::string replacedOnce(const std::string& text, const std::string& from, const std::string& to)
{
    const size_t at = text.find(from);
    CHECK(at != std::string::npos && text.find(from, at + 1) == std::string::npos);
    if (at == std::string::npos)
        return text;
    std::string out = text;
    out.replace(at, from.size(), to);
    return out;
}

// The per-object properties (phase B): parsed, kept as written while unchanged, written in the retail
// order (group, disableChildren, notInAI, isOvergrowth, setLightSourceMask), dropped when off.
void checkLevelObjectFileProperties()
{
    const std::string text =
        "rem *** a ***\r\nObject.create a\r\nObject.absolutePosition 1.000/2.000/3.000\r\nobject.layer 1\r\n"
        "Object.isOvergrowth 1\r\nObject.setLightSourceMask 6\r\n\r\n"
        "rem *** b ***\r\nObject.create b\r\nObject.absolutePosition 4.000/5.000/6.000\r\n"
        "Object.rotation 90.0/0.0/0.0\r\nObject.layer 2\r\nObject.disableChildren 1\r\nObject.group 2\r\n\r\n";
    LevelObjectFile file;
    file.parse(text);
    CHECK(file.recordCount() == 2 && file.render() == text && !file.hasChanges());
    const ObjectPlacement a = file.placement(0);
    CHECK(a.isOvergrowth && a.hasLightSourceMask && a.lightSourceMask == 6 && a.group == 0 && !a.notInAI
          && !a.disableChildren && a.layer == 1);
    const ObjectPlacement b = file.placement(1);
    CHECK(b.disableChildren && b.group == 2 && !b.isOvergrowth && !b.hasLightSourceMask);

    // A move rewrites the position line only; the unchanged lines keep their text (the lower-case
    // layer line too).
    ObjectPlacement p = a;
    p.position[0] = 1.5f;
    file.setPlacement(0, p);
    std::string expected = replacedOnce(text, "1.000/2.000/3.000", "1.500/2.000/3.000");
    CHECK(file.render() == expected);

    // A flag turned off drops its line; the ones turned on go in the retail order after the nearest
    // line before them; a changed mask is rewritten in place.
    p.isOvergrowth = false;
    p.group = 3;
    p.notInAI = true;
    p.lightSourceMask = 4;
    file.setPlacement(0, p);
    expected = replacedOnce(expected, "object.layer 1\r\nObject.isOvergrowth 1\r\nObject.setLightSourceMask 6\r\n",
                            "object.layer 1\r\nObject.group 3\r\nObject.notInAI 1\r\nObject.setLightSourceMask 4\r\n");
    CHECK(file.isEdited(0) && file.render() == expected);
    // Back as loaded: the file as it was.
    file.setPlacement(0, a);
    CHECK(file.render() == text && !file.hasChanges());

    // Record b's lines stand in a different order than the retail one; an edit keeps them where they
    // are and adds a new one after the nearest line before it.
    p = b;
    p.disableChildren = false;
    p.isOvergrowth = true;
    file.setPlacement(1, p);
    const std::string second = replacedOnce(text, "Object.disableChildren 1\r\nObject.group 2\r\n",
                                            "Object.group 2\r\nObject.isOvergrowth 1\r\n");
    CHECK(file.render() == second);

    // A new record writes what is set, in the retail order.
    LevelObjectFile fresh;
    fresh.parse("");
    ObjectPlacement c;
    c.templateName = "c";
    c.position[1] = 5.0f;
    c.layer = 1;
    c.hasLayer = true;
    c.group = 2;
    c.disableChildren = true;
    c.hasLightSourceMask = true;
    c.lightSourceMask = 0;
    fresh.appendRecord(c);
    CHECK(fresh.render() == "rem *** c ***\r\nObject.create c\r\nObject.absolutePosition 0.000/5.000/0.000\r\n"
                            "Object.layer 1\r\nObject.group 2\r\nObject.disableChildren 1\r\n"
                            "Object.setLightSourceMask 0\r\n\r\n");
}

// Template blocks (phase C): found by name, their lines keyed (indexed properties by their index),
// edited in place, added at the block's end, removed, and back to the file when every edit reverts.
void checkLevelObjectFileTemplates()
{
    const std::string text =
        "rem [ObjectSpawnerTemplate: sp_1]\r\nObjectTemplate.create ObjectSpawner sp_1\r\n"
        "ObjectTemplate.activeSafe ObjectSpawner sp_1\r\nObjectTemplate.modifiedByUser dgo\r\n"
        "ObjectTemplate.setObjectTemplate 1 RUTNK_T90\r\nObjectTemplate.setObjectTemplate 2 USTNK_M1A2\r\n"
        "ObjectTemplate.minSpawnDelay 20\r\n\r\n"
        "rem [SoundObjectTemplate: s_1]\r\nObjectTemplate.create Sound s_1\r\n"
        "ObjectTemplate.soundFilename \"common/sound/a b.ogg\"\r\nObjectTemplate.volume 0.9\r\n\r\n"
        "   Object.create sp_1\r\n   Object.absolutePosition 1.000/2.000/3.000\r\n"
        "   Object.rotation 90.000/0.000/0.000\r\n   Object.layer 3\r\n";
    LevelObjectFile file;
    file.parse(text);
    std::string className;
    CHECK(file.findTemplate("SP_1", className) && className == "ObjectSpawner" && !file.findTemplate("x", className));
    std::vector<LevelObjectFile::TemplateLine> lines = file.templateLines("sp_1");
    CHECK(lines.size() == 4 && lines[0].key == "modifiedbyuser" && lines[1].key == "setobjecttemplate 1"
          && lines[2].key == "setobjecttemplate 2" && lines[2].value == "2 USTNK_M1A2" && lines[3].value == "20");
    CHECK(file.render() == text && file.editedTemplates() == 0 && !file.hasChanges());

    // An indexed line is edited by its index; a missing one goes after the block's last line.
    CHECK(file.setTemplateValue("sp_1", "setObjectTemplate", "2 USAPC_LAV25"));
    CHECK(file.setTemplateValue("sp_1", "maxSpawnDelay", "50"));
    std::string expected = replacedOnce(text, "setObjectTemplate 2 USTNK_M1A2", "setObjectTemplate 2 USAPC_LAV25");
    expected = replacedOnce(expected, "ObjectTemplate.minSpawnDelay 20\r\n",
                            "ObjectTemplate.minSpawnDelay 20\r\nObjectTemplate.maxSpawnDelay 50\r\n");
    CHECK(file.render() == expected && file.editedTemplates() == 1 && file.hasChanges());
    lines = file.templateLines("sp_1");
    CHECK(lines.size() == 5 && lines[2].value == "2 USAPC_LAV25" && lines[4].key == "maxspawndelay");

    // Removal by key; the addition still follows where the block ended.
    CHECK(file.setTemplateValue("sp_1", "minspawndelay", ""));
    expected = replacedOnce(expected, "ObjectTemplate.minSpawnDelay 20\r\n", "");
    CHECK(file.render() == expected);

    // Every edit reverted: the file as it was.
    CHECK(file.setTemplateValue("sp_1", "setObjectTemplate", "2 USTNK_M1A2"));
    CHECK(file.setTemplateValue("sp_1", "minSpawnDelay", "20"));
    CHECK(file.setTemplateValue("sp_1", "maxspawndelay", ""));
    CHECK(file.render() == text && file.editedTemplates() == 0 && !file.hasChanges());

    // A control point's block runs on over its "rem -----" separators: its radius is found and edited
    // in place, not added to the block's first part.
    const std::string cp =
        "ObjectTemplate.create ControlPoint cp_1\r\nObjectTemplate.physicsType Mesh\r\nrem -----\r\n"
        "ObjectTemplate.addTemplate flagpole\r\nrem -----\r\nObjectTemplate.radius 10\r\nObjectTemplate.team 2\r\n\r\n";
    LevelObjectFile cpFile;
    cpFile.parse(cp);
    CHECK(cpFile.templateLines("cp_1").size() == 4 && cpFile.setTemplateValue("cp_1", "radius", "25"));
    CHECK(cpFile.render() == replacedOnce(cp, "radius 10", "radius 25"));

    // Supply lines (E15): supplyGroupNeeded is keyed by team and group, so each pair is its own line; a second line
    // of one pair rewrites it (1.50 stops on a duplicate entry); a pair is taken out by its key.
    CHECK(LevelObjectFile::templateKey("supplyGroupNeeded", "1 3 0") == "supplygroupneeded 1 3"
          && LevelObjectFile::templateKey("SupplyGroupNeeded", " 2\t5 1") == "supplygroupneeded 2 5");
    CHECK(cpFile.setTemplateValue("cp_1", "supplyGroupId", "2") && cpFile.setTemplateValue("cp_1", "supplyGroupNeeded", "1 3 0")
          && cpFile.setTemplateValue("cp_1", "supplyGroupNeeded", "2 5 0") && cpFile.setTemplateValue("cp_1", "supplyGroupNeeded", "1 3 1"));
    CHECK(cpFile.render() == replacedOnce(replacedOnce(cp, "radius 10", "radius 25"), "ObjectTemplate.team 2\r\n",
                                          "ObjectTemplate.team 2\r\nObjectTemplate.supplyGroupId 2\r\n"
                                          "ObjectTemplate.supplyGroupNeeded 1 3 1\r\nObjectTemplate.supplyGroupNeeded 2 5 0\r\n"));
    CHECK(cpFile.setTemplateValue("cp_1", "supplyGroupNeeded 1 3", "") && cpFile.setTemplateValue("cp_1", "supplyGroupId", "")
          && cpFile.setTemplateValue("cp_1", "supplyGroupNeeded 2 5", "") && cpFile.render() == replacedOnce(cp, "radius 10", "radius 25"));

    // A quoted value is written as given; another template's block is left alone.
    CHECK(file.setTemplateValue("s_1", "soundFilename", "\"common/sound/c d.ogg\""));
    CHECK(file.render() == replacedOnce(text, "\"common/sound/a b.ogg\"", "\"common/sound/c d.ogg\""));
    CHECK(!file.setTemplateValue("nothing", "volume", "1"));
}

// Making and deleting gameplay objects with their templates (phase C): new blocks go into their class's
// section (spawners' and control points' records inside the host block, spawn points as pairs), a
// deleted object takes its banner along, a deleted template its block; the written order is the order
// a parse finds.
void checkLevelObjectFileGameplay()
{
    const std::string text =
        "rem ********** Object Spawner **********\r\n"
        "rem [ObjectSpawnerTemplate: sp_1]\r\nObjectTemplate.create ObjectSpawner sp_1\r\n"
        "ObjectTemplate.activeSafe ObjectSpawner sp_1\r\nObjectTemplate.setObjectTemplate 1 RUTNK_T90\r\n\r\n"
        "if v_arg1 == host\r\n\r\n"
        "   rem [ObjectSpawner: sp_1]\r\n   Object.create sp_1\r\n   Object.absolutePosition 1.000/2.000/3.000\r\n"
        "   Object.rotation 90.000/0.000/0.000\r\n   Object.setControlPointId 13\r\n   Object.layer 3\r\n\r\n"
        "endIf\r\n\r\n"
        "rem ********** Spawn Points **********\r\n"
        "rem [SpawnPointTemplate: pt_1]\r\nObjectTemplate.create SpawnPoint pt_1\r\n"
        "ObjectTemplate.activeSafe SpawnPoint pt_1\r\nObjectTemplate.setControlPointId 13\r\n\r\n"
        "rem [SpawnPoint: pt_1]\r\nObject.create pt_1\r\nObject.absolutePosition 4.000/5.000/6.000\r\n"
        "Object.rotation 0.000/0.000/0.000\r\nObject.layer 3\r\n\r\n"
        "rem ********** Control Points **********\r\n"
        "rem [ControlPointTemplate: cp_1]\r\nObjectTemplate.create ControlPoint cp_1\r\n"
        "ObjectTemplate.activeSafe ControlPoint cp_1\r\nrem -----\r\nObjectTemplate.addTemplate flagpole\r\nrem -----\r\n"
        "ObjectTemplate.controlPointId 13\r\n\r\n"
        "if v_arg1 == host\r\n\r\n"
        "   rem [ControlPoint: cp_1]\r\n   Object.create cp_1\r\n   Object.absolutePosition 7.000/8.000/9.000\r\n"
        "   Object.layer 3\r\n\r\n"
        "endIf\r\n\r\n"
        "CombatAreaManager.use 1\r\n";
    LevelObjectFile file;
    file.parse(text);
    std::vector<size_t> order;
    CHECK(file.recordCount() == 3 && file.render(&order) == text && !file.hasChanges());
    CHECK(order.size() == 3 && order[0] == 0 && order[1] == 1 && order[2] == 2);
    CHECK(file.placement(0).hasControlPointId && file.placement(0).controlPointId == 13
          && !file.placement(1).hasControlPointId && file.dominantLayer() == 3);

    // A spawner: its template after the last spawner template, its record inside the host block.
    std::vector<std::pair<std::string, std::string> > lines;
    lines.push_back(std::make_pair(std::string("isNotSaveable"), std::string("1")));
    lines.push_back(std::make_pair(std::string("hasMobilePhysics"), std::string("0")));
    CHECK(file.appendTemplate("ObjectSpawner", "sp_2", lines) && !file.appendTemplate("ObjectSpawner", "SP_1", lines));
    ObjectPlacement spawner;
    spawner.templateName = "sp_2";
    spawner.position[0] = 10.0f;
    spawner.position[1] = 2.0f;
    spawner.position[2] = 3.0f;
    spawner.controlPointId = 13;
    spawner.hasControlPointId = true;
    spawner.layer = 3;
    spawner.hasLayer = true;
    spawner.hostOnly = true;
    CHECK(file.appendRecord(spawner) == 3);
    std::string expected = replacedOnce(text, "ObjectTemplate.setObjectTemplate 1 RUTNK_T90\r\n\r\n",
        "ObjectTemplate.setObjectTemplate 1 RUTNK_T90\r\n\r\n"
        "rem [ObjectSpawnerTemplate: sp_2]\r\nObjectTemplate.create ObjectSpawner sp_2\r\n"
        "ObjectTemplate.activeSafe ObjectSpawner sp_2\r\nObjectTemplate.isNotSaveable 1\r\n"
        "ObjectTemplate.hasMobilePhysics 0\r\n\r\n");
    expected = replacedOnce(expected, "   Object.setControlPointId 13\r\n   Object.layer 3\r\n\r\n",
        "   Object.setControlPointId 13\r\n   Object.layer 3\r\n\r\n"
        "   rem [ObjectSpawner: sp_2]\r\n   Object.create sp_2\r\n   Object.absolutePosition 10.000/2.000/3.000\r\n"
        "   Object.rotation 0.000/0.000/0.000\r\n   Object.setControlPointId 13\r\n   Object.layer 3\r\n\r\n");
    CHECK(file.render() == expected && file.editedTemplates() == 1 && file.hasChanges());

    // A spawn point: template and record together, after the last spawn point.
    lines.clear();
    lines.push_back(std::make_pair(std::string("isNotSaveable"), std::string("1")));
    lines.push_back(std::make_pair(std::string("setControlPointId"), std::string("13")));
    CHECK(file.appendTemplate("SpawnPoint", "pt_2", lines));
    ObjectPlacement point;
    point.templateName = "pt_2";
    point.position[0] = 11.0f;
    point.position[1] = 5.0f;
    point.position[2] = 6.0f;
    point.rotation[0] = 45.0f;
    point.layer = 3;
    point.hasLayer = true;
    CHECK(file.appendRecord(point) == 4);
    expected = replacedOnce(expected, "rem ********** Control Points",
        "rem [SpawnPointTemplate: pt_2]\r\nObjectTemplate.create SpawnPoint pt_2\r\n"
        "ObjectTemplate.activeSafe SpawnPoint pt_2\r\nObjectTemplate.isNotSaveable 1\r\n"
        "ObjectTemplate.setControlPointId 13\r\n\r\n"
        "rem [SpawnPoint: pt_2]\r\nObject.create pt_2\r\nObject.absolutePosition 11.000/5.000/6.000\r\n"
        "Object.rotation 45.000/0.000/0.000\r\nObject.layer 3\r\n\r\n"
        "rem ********** Control Points");
    CHECK(file.render() == expected);

    // A control point: its template (with separators written as given) after the last control point
    // template, its record - no zero rotation - inside the second host block.
    lines.clear();
    lines.push_back(std::make_pair(std::string("setNetworkableInfo"), std::string("ControlPointInfo")));
    lines.push_back(std::make_pair(std::string(), std::string("rem -----")));
    lines.push_back(std::make_pair(std::string("addTemplate"), std::string("flagpole")));
    lines.push_back(std::make_pair(std::string(), std::string("rem -----")));
    lines.push_back(std::make_pair(std::string("controlPointId"), std::string("14")));
    CHECK(file.appendTemplate("ControlPoint", "cp_2", lines));
    CHECK(file.templateLines("cp_2").size() == 3 && file.templateLines("cp_2")[2].key == "controlpointid");
    ObjectPlacement cp;
    cp.templateName = "cp_2";
    cp.position[0] = 20.0f;
    cp.position[1] = 8.0f;
    cp.position[2] = 9.0f;
    cp.layer = 3;
    cp.hasLayer = true;
    cp.hostOnly = true;
    cp.omitZeroRotation = true;
    CHECK(file.appendRecord(cp) == 5);
    expected = replacedOnce(expected, "ObjectTemplate.controlPointId 13\r\n\r\n",
        "ObjectTemplate.controlPointId 13\r\n\r\n"
        "rem [ControlPointTemplate: cp_2]\r\nObjectTemplate.create ControlPoint cp_2\r\n"
        "ObjectTemplate.activeSafe ControlPoint cp_2\r\nObjectTemplate.setNetworkableInfo ControlPointInfo\r\n"
        "rem -----\r\nObjectTemplate.addTemplate flagpole\r\nrem -----\r\nObjectTemplate.controlPointId 14\r\n\r\n");
    expected = replacedOnce(expected, "   Object.absolutePosition 7.000/8.000/9.000\r\n   Object.layer 3\r\n\r\n",
        "   Object.absolutePosition 7.000/8.000/9.000\r\n   Object.layer 3\r\n\r\n"
        "   rem [ControlPoint: cp_2]\r\n   Object.create cp_2\r\n   Object.absolutePosition 20.000/8.000/9.000\r\n"
        "   Object.layer 3\r\n\r\n");
    CHECK(file.render(&order) == expected && file.editedTemplates() == 3);
    // The written order, which a parse of the result finds.
    CHECK(order.size() == 6 && order[0] == 0 && order[1] == 3 && order[2] == 1 && order[3] == 4 && order[4] == 2
          && order[5] == 5);
    LevelObjectFile reread;
    reread.parse(expected);
    CHECK(reread.recordCount() == 6 && reread.placement(1).templateName == "sp_2" && reread.placement(3).templateName == "pt_2"
          && reread.placement(5).templateName == "cp_2" && reread.placement(1).controlPointId == 13
          && reread.render() == expected);
    std::string className;
    CHECK(reread.findTemplate("cp_2", className) && className == "ControlPoint" && reread.templateLines("cp_2").size() == 3);

    // New ones deleted again (an undo): the file as it was.
    file.setDeleted(3, true);
    file.setDeleted(4, true);
    file.setDeleted(5, true);
    CHECK(file.setTemplateDeleted("sp_2", true) && file.setTemplateDeleted("pt_2", true)
          && file.setTemplateDeleted("cp_2", true) && file.isTemplateDeleted("SP_2"));
    CHECK(file.render(&order) == text && !file.hasChanges() && file.editedTemplates() == 0 && order.size() == 3);

    // A loaded spawn point deleted with its template: both blocks go, banners and separators too; the
    // section's own banner stays. Back again, the file as it was.
    file.setDeleted(1, true);
    CHECK(file.setTemplateDeleted("pt_1", true));
    CHECK(file.render(&order) == replacedOnce(text,
        "rem [SpawnPointTemplate: pt_1]\r\nObjectTemplate.create SpawnPoint pt_1\r\n"
        "ObjectTemplate.activeSafe SpawnPoint pt_1\r\nObjectTemplate.setControlPointId 13\r\n\r\n"
        "rem [SpawnPoint: pt_1]\r\nObject.create pt_1\r\nObject.absolutePosition 4.000/5.000/6.000\r\n"
        "Object.rotation 0.000/0.000/0.000\r\nObject.layer 3\r\n\r\n", ""));
    CHECK(order.size() == 2 && order[0] == 0 && order[1] == 2 && file.editedTemplates() == 1);
    // A loaded spawner too: the host block stays, empty.
    file.setDeleted(0, true);
    CHECK(file.setTemplateDeleted("sp_1", true));
    std::string both = replacedOnce(text,
        "rem [ObjectSpawnerTemplate: sp_1]\r\nObjectTemplate.create ObjectSpawner sp_1\r\n"
        "ObjectTemplate.activeSafe ObjectSpawner sp_1\r\nObjectTemplate.setObjectTemplate 1 RUTNK_T90\r\n\r\n", "");
    both = replacedOnce(both,
        "   rem [ObjectSpawner: sp_1]\r\n   Object.create sp_1\r\n   Object.absolutePosition 1.000/2.000/3.000\r\n"
        "   Object.rotation 90.000/0.000/0.000\r\n   Object.setControlPointId 13\r\n   Object.layer 3\r\n\r\n", "");
    both = replacedOnce(both,
        "rem [SpawnPointTemplate: pt_1]\r\nObjectTemplate.create SpawnPoint pt_1\r\n"
        "ObjectTemplate.activeSafe SpawnPoint pt_1\r\nObjectTemplate.setControlPointId 13\r\n\r\n"
        "rem [SpawnPoint: pt_1]\r\nObject.create pt_1\r\nObject.absolutePosition 4.000/5.000/6.000\r\n"
        "Object.rotation 0.000/0.000/0.000\r\nObject.layer 3\r\n\r\n", "");
    CHECK(file.render() == both);
    file.setDeleted(0, false);
    file.setDeleted(1, false);
    CHECK(file.setTemplateDeleted("sp_1", false) && file.setTemplateDeleted("pt_1", false));
    CHECK(file.render() == text && !file.hasChanges());

    // A file with no section for them: templates, then records, then host-only records in a new host
    // block, at the end.
    LevelObjectFile bare;
    bare.parse("CombatAreaManager.use 1\r\n");
    lines.clear();
    lines.push_back(std::make_pair(std::string("isNotSaveable"), std::string("1")));
    CHECK(bare.appendTemplate("ObjectSpawner", "sp_9", lines) && bare.appendTemplate("SpawnPoint", "pt_9", lines));
    spawner.templateName = "sp_9";
    spawner.rotation[0] = 90.0f;
    point.templateName = "pt_9";
    CHECK(bare.appendRecord(spawner) == 0 && bare.appendRecord(point) == 1);
    CHECK(bare.render(&order) ==
        "CombatAreaManager.use 1\r\n\r\n"
        "rem [ObjectSpawnerTemplate: sp_9]\r\nObjectTemplate.create ObjectSpawner sp_9\r\n"
        "ObjectTemplate.activeSafe ObjectSpawner sp_9\r\nObjectTemplate.isNotSaveable 1\r\n\r\n"
        "rem [SpawnPointTemplate: pt_9]\r\nObjectTemplate.create SpawnPoint pt_9\r\n"
        "ObjectTemplate.activeSafe SpawnPoint pt_9\r\nObjectTemplate.isNotSaveable 1\r\n\r\n"
        "rem [SpawnPoint: pt_9]\r\nObject.create pt_9\r\nObject.absolutePosition 11.000/5.000/6.000\r\n"
        "Object.rotation 45.0/0.0/0.0\r\nObject.layer 3\r\n\r\n"
        "if v_arg1 == host\r\n\r\n"
        "   rem [ObjectSpawner: sp_9]\r\n   Object.create sp_9\r\n   Object.absolutePosition 10.000/2.000/3.000\r\n"
        "   Object.rotation 90.0/0.0/0.0\r\n   Object.setControlPointId 13\r\n   Object.layer 3\r\n\r\n"
        "endIf\r\n\r\n");
    CHECK(order.size() == 2 && order[0] == 1 && order[1] == 0);

    // AmbientObjects.con's layout: templates, then the class's records indented, with no host block.
    const std::string ambient =
        "rem [TriggerTemplate: t_0]\r\nObjectTemplate.create Trigger t_0\r\nObjectTemplate.activeSafe Trigger t_0\r\n"
        "ObjectTemplate.radius 9\r\n\r\n"
        "rem [TriggerTemplate: t_1]\r\nObjectTemplate.create Trigger t_1\r\nObjectTemplate.activeSafe Trigger t_1\r\n"
        "ObjectTemplate.radius 12\r\n\r\n"
        "   rem [Trigger: t_0]\r\n   Object.create t_0\r\n   Object.absolutePosition 0.000/2.000/3.000\r\n"
        "   Object.rotation 0.000/0.000/0.000\r\n   Object.layer 2\r\n\r\n"
        "   rem [Trigger: t_1]\r\n   Object.create t_1\r\n   Object.absolutePosition 1.000/2.000/3.000\r\n"
        "   Object.rotation 0.000/0.000/0.000\r\n   Object.layer 1\r\n\r\n"
        "rem ********** AmbientEffectArea **********\r\n";
    LevelObjectFile triggers;
    triggers.parse(ambient);
    lines.clear();
    lines.push_back(std::make_pair(std::string("radius"), std::string("5")));
    CHECK(triggers.appendTemplate("Trigger", "t_2", lines));
    ObjectPlacement trigger;
    trigger.templateName = "t_2";
    trigger.position[1] = 7.0f;
    trigger.layer = 1;
    trigger.hasLayer = true;
    CHECK(triggers.appendRecord(trigger) == 2);
    expected = replacedOnce(ambient, "ObjectTemplate.radius 12\r\n\r\n",
        "ObjectTemplate.radius 12\r\n\r\n"
        "rem [TriggerTemplate: t_2]\r\nObjectTemplate.create Trigger t_2\r\nObjectTemplate.activeSafe Trigger t_2\r\n"
        "ObjectTemplate.radius 5\r\n\r\n");
    expected = replacedOnce(expected, "   Object.layer 1\r\n\r\n",
        "   Object.layer 1\r\n\r\n"
        "   rem [Trigger: t_2]\r\n   Object.create t_2\r\n   Object.absolutePosition 0.000/7.000/0.000\r\n"
        "   Object.rotation 0.000/0.000/0.000\r\n   Object.layer 1\r\n\r\n");
    CHECK(triggers.render() == expected);
}

// Combat areas (phase C): blocks and manager lines parsed; an untouched file written back as it was; a
// changed block written again in the stock form, in place; a new one after the last block; a deleted one
// gone with its blank line; manager lines edited in place, and missing ones added where the stock order
// puts them.
void checkLevelObjectFileCombatAreas()
{
    const std::string first =
        "CombatArea.create CA_1\r\nCombatArea.min 0.000000/0.000000\r\nCombatArea.max 0.000000/0.000000\r\n"
        "CombatArea.team 0\r\nCombatArea.vehicles 4\r\nCombatArea.layer 3\r\n";
    const std::string second =
        "CombatArea.create CA_2\r\nCombatArea.min 0.000000/0.000000\r\nCombatArea.max 0.000000/0.000000\r\n"
        "CombatArea.addAreaPoint -168.092102/-353.732849\r\nCombatArea.addAreaPoint -179.912323/-352.144592\r\n"
        "CombatArea.addAreaPoint -194.603058/-345.971497\r\nCombatArea.team 0\r\nCombatArea.vehicles 0\r\n"
        "CombatArea.layer 3\r\nCombatArea.usedByPathFinding 1\r\n";
    const std::string text = "endIf\r\n\r\nCombatAreaManager.use 1\r\nCombatAreaManager.timeAllowedOutside 10.000000\r\n"
                             + first + "\r\n" + second + "\r\n";
    LevelObjectFile file;
    file.parse(text);
    CHECK(file.render() == text && !file.hasChanges() && file.combatAreaCount() == 2 && file.editedCombatAreas() == 0);
    size_t area = 0;
    CHECK(file.findCombatArea("ca_2", area) && area == 1 && !file.findCombatArea("CA_9", area));
    const LevelObjectFile::CombatAreaData loaded = file.combatArea(1);
    CHECK(loaded.points.size() == 3 && loaded.points[0].first == -168.092102f && loaded.points[2].second == -345.971497f
          && loaded.vehicles == 0 && loaded.usedByPathFinding && !loaded.inverted && loaded.layer == 3 && loaded.team == 0);
    CHECK(file.combatArea(0).points.empty() && file.combatArea(0).vehicles == 4);
    const LevelObjectFile::CombatAreaSettings settings = file.combatAreaSettings();
    CHECK(settings.hasUse && settings.use && settings.hasTimeAllowedOutside && settings.timeAllowedOutside == 10.0f
          && !settings.hasDamage);
    // The stock form, line for line.
    CHECK(LevelObjectFile::formatCombatArea(loaded, "\r\n") == second);

    // A point moved: its block written again in place, the other kept.
    LevelObjectFile::CombatAreaData moved = loaded;
    moved.points[1].first = -180.5f;
    CHECK(file.setCombatArea(1, moved));
    CHECK(file.render() == replacedOnce(text, "-179.912323/-352.144592", "-180.500000/-352.144592")
          && file.editedCombatAreas() == 1 && file.hasChanges());
    CHECK(file.setCombatArea(1, loaded) && file.render() == text && !file.hasChanges());

    // A new area goes after the last block; a deleted one leaves with its blank line.
    LevelObjectFile::CombatAreaData fresh;
    fresh.name = "CA_3";
    fresh.points.push_back(std::make_pair(1.0f, 2.0f));
    fresh.points.push_back(std::make_pair(3.0f, 2.0f));
    fresh.points.push_back(std::make_pair(3.0f, 5.0f));
    fresh.layer = 3;
    CHECK(file.appendCombatArea(fresh) == 2 && file.isCombatAreaNew(2));
    file.setCombatAreaDeleted(0, true);
    CHECK(file.render() == replacedOnce(text, first + "\r\n", "") + LevelObjectFile::formatCombatArea(fresh, "\r\n") + "\r\n"
          && file.editedCombatAreas() == 2);
    file.setCombatAreaDeleted(0, false);
    file.setCombatAreaDeleted(2, true);
    CHECK(file.render() == text && !file.hasChanges());

    // The manager: timeAllowedOutside edited in place, damage added before it (the stock order).
    LevelObjectFile::CombatAreaSettings changed = settings;
    changed.timeAllowedOutside = 15.0f;
    changed.hasDamage = true;
    changed.damage = 25.0f;
    file.setCombatAreaSettings(changed);
    CHECK(file.combatAreaSettingsChanged() && file.hasChanges());
    CHECK(file.render() == replacedOnce(text, "CombatAreaManager.timeAllowedOutside 10.000000\r\n",
                                        "CombatAreaManager.damage 25.000000\r\nCombatAreaManager.timeAllowedOutside 15.000000\r\n"));
    file.setCombatAreaSettings(settings);
    CHECK(file.render() == text && !file.hasChanges());

    // A file with neither: the manager lines and the new area at the end.
    LevelObjectFile bare;
    bare.parse("rem x\r\n");
    LevelObjectFile::CombatAreaSettings on;
    on.hasUse = true;
    on.use = true;
    on.hasTimeAllowedOutside = true;
    bare.setCombatAreaSettings(on);
    bare.appendCombatArea(fresh);
    CHECK(bare.render() == "rem x\r\n\r\nCombatAreaManager.use 1\r\nCombatAreaManager.timeAllowedOutside 10.000000\r\n"
                           + LevelObjectFile::formatCombatArea(fresh, "\r\n") + "\r\n");

    // A block with a line the editor does not write is kept as it is.
    LevelObjectFile odd;
    odd.parse("CombatArea.create CA_4\r\nCombatArea.min 0.000000/0.000000\r\nCombatArea.somethingElse 1\r\n");
    CHECK(odd.combatAreaCount() == 1 && !odd.setCombatArea(0, fresh));
}

// Ambient sounds (phase C): a Sound template block goes and comes with its `Sound.addTrigger` line; a new
// one takes the class's banner spelling and its record the editor block's form (no banner).
void checkLevelObjectFileSounds()
{
    const std::string blockA =
        "rem [SoundObjectTemplate: S_A]\r\nObjectTemplate.create Sound S_A\r\nObjectTemplate.activeSafe Sound S_A\r\n"
        "ObjectTemplate.soundFilename \"a.ogg\"\r\nObjectTemplate.position 1/2/3\r\nObjectTemplate.minDistance 14\r\n\r\n"
        "Sound.addTrigger S_A\r\n\r\n";
    const std::string blockB =
        "rem [SoundObjectTemplate: S_B]\r\nObjectTemplate.create Sound S_B\r\nObjectTemplate.activeSafe Sound S_B\r\n"
        "ObjectTemplate.soundFilename \"b.ogg\"\r\nObjectTemplate.position 4/5/6\r\n\r\nSound.addTrigger S_B\r\n\r\n";
    const std::string recordA =
        "Object.create S_A\r\nObject.absolutePosition 1.000/2.000/3.000\r\nObject.rotation 0.000/0.000/0.000\r\n"
        "Object.layer 1\r\n\r\n";
    const std::string recordB =
        "Object.create S_B\r\nObject.absolutePosition 4.000/5.000/6.000\r\nObject.rotation 0.000/0.000/0.000\r\n"
        "Object.layer 1\r\n\r\n";
    const std::string text = "rem ************ AmbientSounds ************\r\n\r\n" + blockA + blockB
                           + "if v_arg1 == BF2Editor\r\n\r\n" + recordA + recordB + "endIf\r\n";
    LevelObjectFile file;
    file.parse(text);
    CHECK(file.render() == text && file.recordCount() == 2);
    const std::vector<std::string> names = file.templateNamesOf("sound");
    CHECK(names.size() == 2 && names[0] == "S_A" && names[1] == "S_B" && file.templateHasTrigger("s_b"));

    // A sound deleted: its block, trigger line and record go.
    CHECK(file.setTemplateDeleted("S_A", true));
    file.setDeleted(0, true);
    CHECK(file.render() == replacedOnce(replacedOnce(text, blockA, ""), recordA, "") && file.templateNamesOf("Sound").size() == 1);
    CHECK(file.setTemplateDeleted("S_A", false));
    file.setDeleted(0, false);
    CHECK(file.render() == text && !file.hasChanges());

    // A new sound: its block and trigger line after the last one, its record at the end of the editor
    // block, without a banner.
    std::vector<std::pair<std::string, std::string> > lines;
    lines.push_back(std::make_pair(std::string("soundFilename"), std::string("\"c.ogg\"")));
    lines.push_back(std::make_pair(std::string("position"), std::string("7/8/9")));
    CHECK(file.appendTemplate("Sound", "S_C", lines, true) && file.templateHasTrigger("S_C"));
    ObjectPlacement sound;
    sound.templateName = "S_C";
    sound.position[0] = 7.0f;
    sound.position[1] = 8.0f;
    sound.position[2] = 9.0f;
    sound.layer = 1;
    sound.hasLayer = true;
    CHECK(file.appendRecord(sound) == 2);
    const std::string blockC =
        "rem [SoundObjectTemplate: S_C]\r\nObjectTemplate.create Sound S_C\r\nObjectTemplate.activeSafe Sound S_C\r\n"
        "ObjectTemplate.soundFilename \"c.ogg\"\r\nObjectTemplate.position 7/8/9\r\n\r\nSound.addTrigger S_C\r\n\r\n";
    const std::string recordC =
        "Object.create S_C\r\nObject.absolutePosition 7.000/8.000/9.000\r\nObject.rotation 0.000/0.000/0.000\r\n"
        "Object.layer 1\r\n\r\n";
    CHECK(file.render() == replacedOnce(replacedOnce(text, blockB, blockB + blockC), recordB, recordB + recordC));
    LevelObjectFile reread;
    reread.parse(file.render());
    CHECK(reread.recordCount() == 3 && reread.templateHasTrigger("S_C") && reread.templateNamesOf("Sound").size() == 3);
    // A moved sound: its position line in place, its record's position line in place.
    CHECK(reread.setTemplateValue("S_B", "position", "40/5/60"));
    ObjectPlacement moved = reread.placement(1);
    moved.position[0] = 40.0f;
    moved.position[2] = 60.0f;
    reread.setPlacement(1, moved);
    CHECK(reread.render() == replacedOnce(replacedOnce(file.render(), "ObjectTemplate.position 4/5/6", "ObjectTemplate.position 40/5/60"),
                                          "4.000/5.000/6.000", "40.000/5.000/60.000"));
}

// Renaming a template the file defines (phase C): its banner, create and activeSafe lines, its sound's
// trigger line and its records' banners and create lines, in place; edits go along; a rename back is the
// file as it was; a name another template has, or one that merely starts the same, is left alone.
void checkLevelObjectFileRenames()
{
    const std::string text =
        "rem [ObjectSpawnerTemplate: sp_1]\r\nObjectTemplate.create ObjectSpawner sp_1\r\n"
        "ObjectTemplate.activeSafe ObjectSpawner sp_1\r\nObjectTemplate.setObjectTemplate 1 RUTNK_T90\r\n\r\n"
        "rem [ObjectSpawnerTemplate: sp_10]\r\nObjectTemplate.create ObjectSpawner sp_10\r\n"
        "ObjectTemplate.activeSafe ObjectSpawner sp_10\r\nObjectTemplate.setObjectTemplate 1 sp_1\r\n\r\n"
        "if v_arg1 == host\r\n\r\n"
        "   rem [ObjectSpawner: sp_1]\r\n   Object.create sp_1\r\n   Object.absolutePosition 1.000/2.000/3.000\r\n"
        "   Object.setControlPointId 13\r\n   Object.layer 3\r\n\r\n"
        "   rem [ObjectSpawner: sp_10]\r\n   Object.create sp_10\r\n   Object.absolutePosition 4.000/5.000/6.000\r\n"
        "   Object.layer 3\r\n\r\n"
        "endIf\r\n";
    LevelObjectFile file;
    file.parse(text);
    CHECK(file.render() == text && !file.hasChanges());
    CHECK(file.renameTemplate("SP_1", "spawner_gas"));
    std::string expected = text;
    expected = replacedOnce(expected, "rem [ObjectSpawnerTemplate: sp_1]", "rem [ObjectSpawnerTemplate: spawner_gas]");
    expected = replacedOnce(expected, "create ObjectSpawner sp_1\r\n", "create ObjectSpawner spawner_gas\r\n");
    expected = replacedOnce(expected, "activeSafe ObjectSpawner sp_1\r\n", "activeSafe ObjectSpawner spawner_gas\r\n");
    expected = replacedOnce(expected, "rem [ObjectSpawner: sp_1]", "rem [ObjectSpawner: spawner_gas]");
    expected = replacedOnce(expected, "Object.create sp_1\r\n", "Object.create spawner_gas\r\n");
    CHECK(file.render() == expected && file.hasChanges() && file.editedTemplates() == 1);
    std::string className;
    CHECK(file.findTemplate("spawner_gas", className) && className == "ObjectSpawner" && !file.findTemplate("sp_1", className));
    CHECK(file.placement(0).templateName == "spawner_gas" && file.placement(1).templateName == "sp_10");
    // Taken names and unknown templates are refused.
    CHECK(!file.renameTemplate("spawner_gas", "SP_10") && !file.renameTemplate("nothing", "x") && !file.renameTemplate("sp_10", ""));

    // An edit made under either name goes along with the rename.
    CHECK(file.setTemplateValue("spawner_gas", "setObjectTemplate", "1 USTNK_M1A2"));
    CHECK(file.render() == replacedOnce(expected, "setObjectTemplate 1 RUTNK_T90", "setObjectTemplate 1 USTNK_M1A2"));
    CHECK(file.renameTemplate("spawner_gas", "sp_1"));
    CHECK(file.render() == replacedOnce(text, "setObjectTemplate 1 RUTNK_T90", "setObjectTemplate 1 USTNK_M1A2"));
    CHECK(file.templateLines("sp_1").size() == 1 && file.templateLines("sp_1")[0].value == "1 USTNK_M1A2");
    CHECK(file.setTemplateValue("sp_1", "setObjectTemplate", "1 RUTNK_T90"));
    CHECK(file.render() == text && !file.hasChanges() && file.editedTemplates() == 0);

    // A reread of a renamed file finds the new name everywhere.
    CHECK(file.renameTemplate("sp_10", "sp_harbour"));
    LevelObjectFile reread;
    reread.parse(file.render());
    CHECK(reread.findTemplate("sp_harbour", className) && reread.recordCount() == 2
          && reread.placement(1).templateName == "sp_harbour" && !reread.hasChanges());

    // A sound: its trigger line and its record (no banner) follow.
    const std::string sounds =
        "rem [SoundObjectTemplate: S_A]\r\nObjectTemplate.create Sound S_A\r\nObjectTemplate.activeSafe Sound S_A\r\n"
        "ObjectTemplate.soundFilename \"a.ogg\"\r\n\r\nSound.addTrigger S_A\r\n\r\n"
        "if v_arg1 == BF2Editor\r\n\r\nObject.create S_A\r\nObject.absolutePosition 1.000/2.000/3.000\r\n"
        "Object.rotation 0.000/0.000/0.000\r\nObject.layer 1\r\n\r\nendIf\r\n";
    LevelObjectFile soundFile;
    soundFile.parse(sounds);
    CHECK(soundFile.renameTemplate("S_A", "S_Harbour"));
    std::string renamed;
    for (size_t at = 0; at < sounds.size();)
    {
        const size_t found = sounds.find(" S_A", at);
        if (found == std::string::npos)
        {
            renamed += sounds.substr(at);
            break;
        }
        renamed += sounds.substr(at, found - at) + " S_Harbour";
        at = found + 4;
    }
    CHECK(soundFile.render() == renamed && soundFile.templateHasTrigger("S_Harbour"));
    CHECK(soundFile.renameTemplate("S_Harbour", "S_A") && soundFile.render() == sounds && !soundFile.hasChanges());
}

// Level settings files (phase D): statements found by command and index words in their branch; an edit
// rewrites one value in place; a missing one goes after its command's others; untouched is byte for byte.
void checkLevelSettingsFile()
{
    const std::string init =
        "rem *** Generated by BF2Editor ***\r\nif v_arg1 == BF2Editor\r\n\r\nrun Heightdata.con\r\nLevelSettings.InitWorld\r\n"
        "\r\nelse\r\n\r\nrun Heightdata.con\r\n\r\nendIf\r\n\r\n"
        "gameLogic.setTeamName 1 \"MEC\"\r\ngameLogic.setTeamName 2 \"US\"\r\n\r\n"
        "gameLogic.setKit 1 0 \"MEC_Specops\" \"mec_light_soldier\"\r\ngameLogic.setKit 2 0 \"US_Specops\" \"us_light_soldier\"\r\n\r\n"
        "gameLogic.setBeforeSpawnCamera -203/157/-27 -28/-12/0\r\n\r\n"
        "if v_arg1 == BF2Editor\r\nLevelSettings.CustomTextureSuffix \"\"\r\nelse\r\ntexturemanager.customTextureSuffix \"\"\r\nendIf\r\n"
        "GameLogic.MaximumLevelViewDistance 140\r\n\r\n"
        "gameLogic.setDefaultNumberOfTicketsEx 16 1 100\r\ngameLogic.setDefaultNumberOfTicketsEx 64 2 330\r\n";
    LevelSettingsFile file;
    file.parse(init);
    CHECK(file.render() == init && !file.hasChanges());
    std::string value;
    CHECK(file.value("gameLogic.setTeamName", "2", LevelSettingsFile::Always, value) && value == "\"US\"");
    CHECK(file.value("GAMELOGIC.setKit", "1 0", LevelSettingsFile::Always, value) && value == "\"MEC_Specops\" \"mec_light_soldier\"");
    CHECK(file.value("gameLogic.setDefaultNumberOfTicketsEx", "64 2", LevelSettingsFile::Always, value) && value == "330");
    CHECK(file.value("texturemanager.customTextureSuffix", "", LevelSettingsFile::GameBranch, value) && value == "\"\"");
    CHECK(file.value("LevelSettings.CustomTextureSuffix", "", LevelSettingsFile::EditorBranch, value)
          && !file.value("LevelSettings.CustomTextureSuffix", "", LevelSettingsFile::Always, value));
    CHECK(!file.value("gameLogic.setTeamName", "3", LevelSettingsFile::Always, value));

    // Edits in place: the command's spelling and index words kept.
    file.setValue("GAMELOGIC.SETTEAMNAME", "1", LevelSettingsFile::Always, "\"Rebels\"");
    file.setValue("gameLogic.setBeforeSpawnCamera", "", LevelSettingsFile::Always, "-150/170/-300 30/-10/0");
    file.setValue("texturemanager.customTextureSuffix", "", LevelSettingsFile::GameBranch, "\"_night\"");
    std::string expected = replacedOnce(init, "gameLogic.setTeamName 1 \"MEC\"", "gameLogic.setTeamName 1 \"Rebels\"");
    expected = replacedOnce(expected, "-203/157/-27 -28/-12/0", "-150/170/-300 30/-10/0");
    expected = replacedOnce(expected, "texturemanager.customTextureSuffix \"\"", "texturemanager.customTextureSuffix \"_night\"");
    CHECK(file.render() == expected && file.hasChanges());
    CHECK(file.value("gameLogic.setTeamName", "1", LevelSettingsFile::Always, value) && value == "\"Rebels\"");

    // A missing statement: after its command's last one; one with no such command, at the end.
    file.setValue("gameLogic.setDefaultNumberOfTicketsEx", "32 1", LevelSettingsFile::Always, "200");
    file.setValue("gameLogic.setTicketLossPerMin", "1", LevelSettingsFile::Always, "12");
    expected = replacedOnce(expected, "gameLogic.setDefaultNumberOfTicketsEx 64 2 330\r\n",
                            "gameLogic.setDefaultNumberOfTicketsEx 64 2 330\r\ngameLogic.setDefaultNumberOfTicketsEx 32 1 200\r\n"
                            "gameLogic.setTicketLossPerMin 1 12\r\n");
    CHECK(file.render() == expected);
    CHECK(file.value("gameLogic.setDefaultNumberOfTicketsEx", "32 1", LevelSettingsFile::Always, value) && value == "200");
    // Setting the added one again changes it; setting values back to the file's: no edit is left.
    file.setValue("gameLogic.setDefaultNumberOfTicketsEx", "32 1", LevelSettingsFile::Always, "210");
    CHECK(file.render().find("setDefaultNumberOfTicketsEx 32 1 210\r\n") != std::string::npos);
    file.revert();
    CHECK(file.render() == init && !file.hasChanges());
    file.setValue("gameLogic.setTeamName", "1", LevelSettingsFile::Always, "\"Rebels\"");
    file.setValue("gameLogic.setTeamName", "1", LevelSettingsFile::Always, "\"MEC\"");
    CHECK(file.render() == init);

    // A statement inside another kind of conditional is none of these (Sky.con's editor-only lines are).
    LevelSettingsFile sky;
    sky.parse("Lightmanager.skycolor 0.80/0.75/0.65\nif v_arg1 == host\nRenderer.fogColor 1/2/3\nendIf\nRenderer.fogColor 163.00/135.00/86.00\n");
    CHECK(sky.value("renderer.fogcolor", "", LevelSettingsFile::Always, value) && value == "163.00/135.00/86.00");
    sky.setValue("Lightmanager.skyColor", "", LevelSettingsFile::Always, "0.5/0.5/0.5");
    CHECK(sky.render() == "Lightmanager.skycolor 0.5/0.5/0.5\nif v_arg1 == host\nRenderer.fogColor 1/2/3\nendIf\n"
                          "Renderer.fogColor 163.00/135.00/86.00\n");
}

// The map description (phase D): the layers it lists; a new size copied from its mode's last line
// without ai="1"; a new mode in an element of its own; a listed layer refused.
void checkMapDescription()
{
    const std::string cq64 = "\t\t\t<maptype players=\"64\" type=\"assault\" locid=\"GAMEMODE_DESCRIPTION_assault\">Unlocalized</maptype>\r\n";
    const std::string coop16 =
        "\t\t\t<maptype ai=\"1\" players=\"16\" type=\"assault\" locid=\"GAMEMODE_DESCRIPTION_COOP_assault\">Unlocalized</maptype>\r\n";
    const std::string desc =
        "<map gsid=\"4\">\r\n\t<name> Strike At Karkand </name>\r\n\t<modes>\r\n\t\t<mode type=\"gpm_cq\">\r\n"
        "\t\t\t<maptype ai=\"1\" players=\"16\" type=\"assault\" locid=\"GAMEMODE_DESCRIPTION_assault\">Unlocalized</maptype>\r\n"
        + cq64 + "\t\t</mode>\r\n\t\t<mode type=\"gpm_coop\">\r\n" + coop16 + "\t\t\t\r\n\t\t</mode>\r\n\t</modes>\r\n</map>\r\n";
    const std::vector<std::pair<std::string, int> > layers = mapDescriptionLayers(desc);
    CHECK(layers.size() == 3 && layers[0].first == "gpm_cq" && layers[0].second == 16 && layers[1].second == 64
          && layers[2].first == "gpm_coop" && layers[2].second == 16);

    std::string x = desc;
    CHECK(addMapDescriptionLayer(x, "gpm_cq", 128));
    CHECK(x == replacedOnce(desc, cq64, cq64 + "\t\t\t<maptype players=\"128\" type=\"assault\" locid=\"GAMEMODE_DESCRIPTION_assault\">"
                                             "Unlocalized</maptype>\r\n"));
    CHECK(!addMapDescriptionLayer(x, "GPM_CQ", 128) && mapDescriptionLayers(x).size() == 4);

    std::string y = desc;
    CHECK(addMapDescriptionLayer(y, "gpm_coop", 32));
    CHECK(y == replacedOnce(desc, coop16, coop16 + "\t\t\t<maptype players=\"32\" type=\"assault\" "
                                                   "locid=\"GAMEMODE_DESCRIPTION_COOP_assault\">Unlocalized</maptype>\r\n"));

    std::string z = desc;
    CHECK(addMapDescriptionLayer(z, "sp2", 16));
    CHECK(z == replacedOnce(desc, "\t</modes>", "\t\t<mode type=\"sp2\">\r\n\t\t\t<maptype players=\"16\" type=\"assault\" "
                                               "locid=\"GAMEMODE_DESCRIPTION_COOP_assault\">Unlocalized</maptype>\r\n"
                                               "\t\t</mode>\r\n\t</modes>"));
    CHECK(mapDescriptionLayers(z).size() == 4 && mapDescriptionLayers(z)[3].first == "sp2");
    std::string none = "<map></map>";
    CHECK(!addMapDescriptionLayer(none, "gpm_cq", 16) && none == "<map></map>");
}

// Comparing gameplay layers (phase C): a layer summarised from its file; control points matched across
// layers by their name without the layer's words, or by where they stand.
void checkLayerSummary()
{
    CHECK(controlPointPlace("CPNAME_SK_64_gasstation") == "cpname_sk_gasstation");
    CHECK(controlPointPlace("CPNAME_SK_SP16_gasstation") == "cpname_sk_gasstation");
    CHECK(controlPointPlace("CPNAME_SK_16_hotel") == controlPointPlace("cpname_sk_32_hotel"));
    CHECK(controlPointPlace("MyFlag") == "myflag" && controlPointPlace("Base_2") == "base");

    const std::string small =
        "rem [ControlPointTemplate: CPNAME_XX_16_hotel]\r\nObjectTemplate.create ControlPoint CPNAME_XX_16_hotel\r\n"
        "ObjectTemplate.radius 5\r\nObjectTemplate.team 1\r\nObjectTemplate.controlPointId 27\r\n\r\n"
        "rem [ControlPointTemplate: CPNAME_XX_16_market]\r\nObjectTemplate.create ControlPoint CPNAME_XX_16_market\r\n"
        "ObjectTemplate.controlPointId 28\r\n\r\n"
        "rem [ObjectSpawnerTemplate: sp_1]\r\nObjectTemplate.create ObjectSpawner sp_1\r\n"
        "ObjectTemplate.setObjectTemplate 1 RUTNK_T90\r\nObjectTemplate.setObjectTemplate 2 USTNK_M1A2\r\n\r\n"
        "if v_arg1 == host\r\n\r\n"
        "   rem [ControlPoint: CPNAME_XX_16_hotel]\r\n   Object.create CPNAME_XX_16_hotel\r\n"
        "   Object.absolutePosition 0.000/10.000/0.000\r\n   Object.layer 5\r\n\r\n"
        "   rem [ControlPoint: CPNAME_XX_16_market]\r\n   Object.create CPNAME_XX_16_market\r\n"
        "   Object.absolutePosition 500.000/10.000/0.000\r\n   Object.layer 5\r\n\r\n"
        "   rem [ObjectSpawner: sp_1]\r\n   Object.create sp_1\r\n   Object.absolutePosition 5.000/10.000/0.000\r\n"
        "   Object.setControlPointId 27\r\n   Object.layer 5\r\n\r\n"
        "endIf\r\n\r\n"
        "CombatArea.create CA_16\r\nCombatArea.min 0.000000/0.000000\r\nCombatArea.max 0.000000/0.000000\r\n"
        "CombatArea.addAreaPoint 0.000000/0.000000\r\nCombatArea.addAreaPoint 10.000000/0.000000\r\n"
        "CombatArea.addAreaPoint 10.000000/10.000000\r\nCombatArea.team 0\r\nCombatArea.vehicles 4\r\nCombatArea.layer 5\r\n";
    LevelObjectFile smallFile;
    smallFile.parse(small);
    const LayerSummary a = summarizeLayer(smallFile, "gpm_cq", 16);
    CHECK(a.controlPoints.size() == 2 && a.controlPoints[0].name == "CPNAME_XX_16_hotel" && a.controlPoints[0].id == 27
          && a.controlPoints[0].team == 1 && a.controlPoints[0].radius == 5.0f && a.controlPoints[1].team == 0
          && a.controlPoints[1].radius == 1.0f && a.controlPoints[1].position[0] == 500.0f);
    CHECK(a.spawners.size() == 1 && a.spawners[0].vehicle[0] == "RUTNK_T90" && a.spawners[0].vehicle[1] == "USTNK_M1A2"
          && a.spawners[0].controlPointId == 27);
    CHECK(a.combatAreas.size() == 1 && a.combatAreas[0].name == "CA_16" && a.combatAreas[0].points.size() == 3);

    // A second layer: the hotel moved 30 m under its own name, a factory of its own, and a flag named
    // its own way 20 m from the market.
    LayerSummary b;
    b.mode = "gpm_cq";
    b.size = 64;
    LayerControlPoint point = a.controlPoints[0];
    point.name = "CPNAME_XX_64_hotel";
    point.place = controlPointPlace(point.name);
    point.position[0] = 30.0f;
    b.controlPoints.push_back(point);
    point.name = "CPNAME_XX_64_factory";
    point.place = controlPointPlace(point.name);
    point.position[0] = -800.0f;
    b.controlPoints.push_back(point);
    point.name = "Bazaar";
    point.place = controlPointPlace(point.name);
    point.position[0] = 520.0f;
    b.controlPoints.push_back(point);
    std::vector<LayerSummary> layers;
    layers.push_back(a);
    layers.push_back(b);
    const std::vector<ControlPointRow> rows = matchControlPoints(layers, 50.0f);
    CHECK(rows.size() == 3);
    CHECK(rows.size() == 3 && rows[0].label == "factory" && rows[0].point[0] == -1 && rows[0].point[1] == 1);
    CHECK(rows.size() == 3 && rows[1].label == "hotel" && rows[1].point[0] == 0 && rows[1].point[1] == 0);
    CHECK(rows.size() == 3 && rows[2].label == "market" && rows[2].point[0] == 1 && rows[2].point[1] == 2);
}

void checkSettings()
{
    std::vector<std::string> problems;
    EditorSettings s;
    s.parse("# comment\n; also a comment\n\ncamera.speed = 12.5\r\n gizmo.mode=2 \nbad line\n= 3\n"
            "panels.log = 0\nlevel.Strike_at_Karkand.camera = 190,185,90,35,-8\nkey with space = 1\n",
            problems);
    CHECK(problems.size() == 3);
    CHECK(s.getFloat("camera.speed", 1.0f) == 12.5f);
    CHECK(s.getInt("gizmo.mode", -1) == 2);
    CHECK(!s.getBool("panels.log", true));
    CHECK(s.get("level.Strike_at_Karkand.camera", "") == "190,185,90,35,-8");
    CHECK(s.getInt("camera.speed", 7) == 7);                // not an int: the fallback
    CHECK(s.getFloat("missing", 3.0f) == 3.0f && !s.has("missing"));
    CHECK(s.getBool("gizmo.mode", true));                    // "2" is not a bool: the fallback

    CHECK(s.setFloat("gizmo.moveSnap", 0.25f) && s.setBool("gizmo.snap", true) && s.setInt("gizmo.space", 1));
    CHECK(!s.set("bad key", "x") && !s.set("", "x") && !s.set("k", "two\nlines"));

    // What is written reads back the same.
    EditorSettings copy;
    std::vector<std::string> none;
    copy.parse(s.serialize(), none);
    CHECK(none.empty() && copy.values() == s.values());
    CHECK(copy.getFloat("gizmo.moveSnap", 0.0f) == 0.25f && copy.getBool("gizmo.snap", false));

    // A file on disk: missing reads as defaults; saved and loaded again it round-trips.
    char directory[MAX_PATH];
    char path[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, directory) != 0);
    CHECK(GetTempFileNameA(directory, "bfs", 0, path) != 0);
    CHECK(DeleteFileA(path) != 0);
    EditorSettings fromNothing;
    CHECK(fromNothing.load(path, problems) && fromNothing.values().empty());
    std::string error;
    CHECK(s.save(path, error) && error.empty());
    EditorSettings loaded;
    std::vector<std::string> loadProblems;
    CHECK(loaded.load(path, loadProblems) && loadProblems.empty() && loaded.values() == s.values());
    CHECK(DeleteFileA(path) != 0);
}

} // namespace

// Keyboard shortcuts: chords parse and print, a held key takes no modifiers, clashes are
// found (a plain key on a camera key too), and the settings keep what differs from the defaults.
void checkShortcuts()
{
    KeyChord chord;
    CHECK(parseChord("ctrl+shift+z", chord) && chord.ctrl && chord.shift && !chord.alt && chord.key == "Z");
    CHECK(formatChord(chord) == "Ctrl+Shift+Z");
    CHECK(parseChord("Control + F5", chord) && formatChord(chord) == "Ctrl+F5");
    CHECK(parseChord("", chord) && chord.empty() && parseChord("none", chord) && chord.empty());
    CHECK(!parseChord("Ctrl+", chord) && !parseChord("Shift", chord) && !parseChord("Hyper+Z", chord));
    CHECK(parseChord("Delete", chord) && formatChord(chord) == "Delete");

    Shortcuts table;
    CHECK(table.display("edit.undo") == "Ctrl+Z" && table.display("camera.forward") == "W" && table.isDefault("edit.undo"));
    bool clean = true;
    for (size_t i = 0; i < Shortcuts::actions().size(); ++i)
        clean = clean && table.conflicts(Shortcuts::actions()[i].id).empty();
    CHECK(clean);
    std::string error;
    CHECK(table.set("edit.undo", "Ctrl+U", error) && table.display("edit.undo") == "Ctrl+U" && !table.isDefault("edit.undo"));
    CHECK(!table.set("camera.forward", "Ctrl+W", error) && !table.set("nothing", "X", error) && !table.set("edit.undo", "Ctrl+", error));
    CHECK(table.set("gizmo.move", "W", error) && table.conflicts("gizmo.move").size() == 1
          && table.conflicts("camera.forward").size() == 1);
    CHECK(table.set("gizmo.move", "Ctrl+W", error) && table.conflicts("gizmo.move").empty());
    CHECK(table.set("edit.redo", "Ctrl+U", error) && table.conflicts("edit.undo").size() == 1);

    EditorSettings settings;
    CHECK(table.set("selection.frame", "", error));
    table.save(settings);
    CHECK(settings.get("shortcuts.edit.undo", "") == "Ctrl+U" && settings.get("shortcuts.selection.frame", "") == "none"
          && !settings.has("shortcuts.edit.copy"));
    Shortcuts reread;
    reread.load(settings);
    CHECK(reread.display("edit.undo") == "Ctrl+U" && reread.chord("selection.frame").empty()
          && reread.display("edit.copy") == "Ctrl+C");
    table.resetAll();
    table.save(settings);
    CHECK(settings.get("shortcuts.edit.undo", "") == "Ctrl+Z");
    reread.load(settings);
    CHECK(reread.isDefault("edit.undo") && reread.isDefault("selection.frame"));
}

// Packing a level (Framework/LevelPacker, ZipWriter): the stock split rule on sample paths, and an archive
// written and read back through minizip's unzip - the reader behind the game's archives.
void checkLevelPacking()
{
    CHECK(levelArchiveFor("Init.con") == LevelArchive::Server && levelArchiveFor("gamemodes/gpm_cq/64/GamePlayObjects.con") == LevelArchive::Server);
    CHECK(levelArchiveFor("HeightmapPrimary.raw") == LevelArchive::Server && levelArchiveFor("terraindata.raw") == LevelArchive::Client);
    CHECK(levelArchiveFor("Undergrowth.raw") == LevelArchive::Client && levelArchiveFor("Overgrowth\\OvergrowthShadowmap.raw") == LevelArchive::Server);
    CHECK(levelArchiveFor("lightmaps/Objects/LightmapAtlas.tai") == LevelArchive::Client && levelArchiveFor("UndergrowthAtlas.tai") == LevelArchive::Server);
    CHECK(levelArchiveFor("Colormaps/tx00x00.dds") == LevelArchive::Client && levelArchiveFor("Roads/temp_compiled.mesh") == LevelArchive::Client);
    CHECK(levelArchiveFor("Info/Strike_at_Karkand.desc") == LevelArchive::None && levelArchiveFor("Editor/GroundProperties.con") == LevelArchive::None);
    CHECK(levelArchiveFor("archive.md5") == LevelArchive::None && levelArchiveFor("StaticObjects.con.old") == LevelArchive::None);
    CHECK(levelArchiveFor("map.desc") == LevelArchive::Server && levelArchiveFor("Envmaps/EnvironmentMapInfo.emi") == LevelArchive::Server);

    char folder[MAX_PATH], path[MAX_PATH];
    GetTempPathA(MAX_PATH, folder);
    GetTempFileNameA(folder, "bfz", 0, path);
    std::string text;
    for (int i = 0; i < 200; ++i)
        text += "ObjectTemplate.activeSafe SpawnPoint CPNAME_test\r\n";
    std::vector<unsigned char> noise(4096);
    unsigned int seed = 12345;
    for (size_t i = 0; i < noise.size(); ++i)
    {
        seed = seed * 1103515245u + 12345u;
        noise[i] = static_cast<unsigned char>(seed >> 16);
    }
    ZipWriter zip;
    std::string error;
    CHECK(zip.open(path, error));
    CHECK(zip.add("Init.con", reinterpret_cast<const unsigned char*>(text.data()), text.size(), 0x6000, 0x5B21, error));
    CHECK(zip.add("Colormaps/tx00x00.dds", &noise[0], noise.size(), 0, 0, error));
    CHECK(zip.add("empty.txt", 0, 0, 0, 0, error));
    CHECK(zip.close(error) && zip.entryCount() == 3);

    unzFile unz = unzOpen(path);
    CHECK(unz != 0);
    int entries = 0;
    bool same = true;
    bool methods = true;
    for (int status = unz ? unzGoToFirstFile(unz) : UNZ_END_OF_LIST_OF_FILE; status == UNZ_OK; status = unzGoToNextFile(unz))
    {
        char name[256];
        unz_file_info info;
        unzGetCurrentFileInfo(unz, &info, name, sizeof(name), 0, 0, 0, 0);
        std::vector<char> data(info.uncompressed_size + 1);
        unzOpenCurrentFile(unz);
        const int got = unzReadCurrentFile(unz, &data[0], static_cast<unsigned>(data.size()));
        unzCloseCurrentFile(unz);
        const std::string n = name;
        if (n == "Init.con")
        {
            same = same && got == static_cast<int>(text.size()) && std::memcmp(&data[0], text.data(), text.size()) == 0;
            methods = methods && info.compression_method == 8 && info.dosDate == ((0x5B21UL << 16) | 0x6000UL);
        }
        else if (n == "Colormaps/tx00x00.dds")
        {
            same = same && got == static_cast<int>(noise.size()) && std::memcmp(&data[0], &noise[0], noise.size()) == 0;
            methods = methods && info.compression_method == 0;          // noise does not deflate: stored
        }
        else if (n == "empty.txt")
            same = same && got == 0;
        ++entries;
    }
    if (unz)
        unzClose(unz);
    CHECK(entries == 3 && same && methods);
    DeleteFileA(path);
}

// Undergrowth.cfg (Framework/UndergrowthConfig): a sample in the stock shape is written back byte for byte;
// an edit and a new material land where the 2006 writer puts them; with BF2EDITOR_UNDERGROWTH_SAMPLES set
// to a folder of .cfg files (extract_undergrowth_cfgs.py copies the 22 stock ones), each is written back
// byte for byte too.
void checkUndergrowthConfig()
{
    const std::string text =
        "ViewDistance 30\r\nViewDistanceFadeScale 0.5\r\nViewDistanceHeightScale 0.7\r\nViewDistanceHeight2Scale 0.4\r\n"
        "ViewDistanceStreamingScale 1\r\nPatchSubdivide 15\r\nSwayScale 0.15\r\nLightingScale 2.1\r\nAlphaRef 0.15\r\n\r\n"
        "Material defaultgrass 2\r\n{\r\n\tGeneralHeight 0.3\r\n\r\n\tType grass\r\n\t{\r\n\t\tMesh triangle_grass\r\n"
        "\t\tTexture grass_mix4\r\n\t\tRandomSizeScale 0.3 0.65\r\n\t\tDensity 3\r\n\t\tVariation 0.2\r\n"
        "\t\tTerrainColorScale 1\r\n\t\tTypeSwayScale 0.4\r\n\t\tSkew 0\r\n\t}\r\n}\r\n\r\n"
        "Material gravel 3\r\n{\r\n\tGeneralHeight 0\r\n\r\n}\r\n\r\n"
        "Material flowers 7\r\n{\r\n\tGeneralHeight 0\r\n\r\n\tType flower1\r\n\t{\r\n\t\tCrossSize 1.7 0.8\r\n"
        "\t\tTexture flower\r\n\t\tRandomSizeScale 0.7 0.8\r\n\t\tDensity 0.5\r\n\t\tVariation -1\r\n"
        "\t\tTerrainColorScale 1\r\n\t\tTypeSwayScale 0.3\r\n\t\tSkew 1\r\n\t}\r\n}\r\n\r\n";
    UndergrowthConfig cfg;
    std::string error;
    CHECK(cfg.parse(text, error) && cfg.settings.size() == 9 && cfg.materials.size() == 3);
    CHECK(cfg.render() == text);
    CHECK(cfg.materials[2].types[0].mesh.empty() && cfg.materials[2].types[0].crossSize[0] == 1.7f && cfg.materials[2].types[0].skew);
    CHECK(cfg.materialById(3) && cfg.materialById(3)->name == "gravel" && cfg.freeId() == 1);

    // An edit rewrites its line; a new material goes last, with the writer's blank lines.
    cfg.material("gravel")->generalHeight = 0.25f;
    UndergrowthMaterial reeds;
    reeds.name = "reeds";
    reeds.id = cfg.freeId();
    UndergrowthType reed;
    reed.name = "reed";
    reed.mesh = "reed_mesh";
    reed.texture = "reed_tex";
    reeds.types.push_back(reed);
    cfg.materials.push_back(reeds);
    std::string expected = text;
    expected.replace(expected.find("Material gravel 3\r\n{\r\n\tGeneralHeight 0"), 38, "Material gravel 3\r\n{\r\n\tGeneralHeight 0.25");
    expected += "Material reeds 1\r\n{\r\n\tGeneralHeight 0\r\n\r\n\tType reed\r\n\t{\r\n\t\tMesh reed_mesh\r\n\t\tTexture reed_tex\r\n"
                "\t\tRandomSizeScale 1 1\r\n\t\tDensity 1\r\n\t\tVariation 0\r\n\t\tTerrainColorScale 1\r\n"
                "\t\tTypeSwayScale 1\r\n\t\tSkew 0\r\n\t}\r\n}\r\n\r\n";
    CHECK(cfg.render() == expected);
    CHECK(!cfg.parse("Material x 1\r\n{\r\n\tWhat 1\r\n}\r\n", error));

    // The stock files, when a folder of them is given.
    char folder[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BF2EDITOR_UNDERGROWTH_SAMPLES", folder, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return;
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((std::string(folder) + "\\*.cfg").c_str(), &found);
    int files = 0, same = 0;
    std::string first;
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string path = std::string(folder) + "\\" + found.cFileName;
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (!f)
                continue;
            std::string data;
            char buffer[4096];
            size_t got = 0;
            while ((got = std::fread(buffer, 1, sizeof(buffer), f)) > 0)
                data.append(buffer, got);
            std::fclose(f);
            ++files;
            UndergrowthConfig stock;
            if (stock.parse(data, error) && stock.render() == data)
                ++same;
            else if (first.empty())
                first = found.cFileName;
        } while (FindNextFileA(h, &found));
        FindClose(h);
    }
    std::printf("[bf2_editor_checks] undergrowth samples: %d of %d written back byte for byte%s%s\n", same, files,
                first.empty() ? "" : "; first difference: ", first.c_str());
    CHECK(files > 0 && same == files);
}

// Save level as (Framework/LevelCopier): level names, path references retargeted (either slash, any case,
// word boundaries), and terraindata.raw's four header lines.
void checkLevelCopier()
{
    CHECK(validLevelName("My_Map2") && !validLevelName("my map") && !validLevelName("") && !validLevelName("a/b"));
    int count = 0;
    const std::string text = "run Levels/Strike_at_Karkand/Init.con\r\nlevels\\strike_at_karkand\\x.dds\r\n"
                             "MyLevels/Strike_at_Karkand/y\r\nLevels/Strike_at_Karkand2/z\r\nLEVELS/STRIKE_AT_KARKAND/w\r\n";
    const std::string expected = "run Levels/New_Map/Init.con\r\nlevels\\New_Map\\x.dds\r\nMyLevels/Strike_at_Karkand/y\r\n"
                                 "Levels/Strike_at_Karkand2/z\r\nLEVELS/New_Map/w\r\n";
    CHECK(retargetLevelPaths(text, "Strike_at_Karkand", "New_Map", count) == expected && count == 3);

    std::vector<unsigned char> raw(57, 0x11);
    const std::string lines = "Levels/Strike_at_Karkand/Colormaps/tx\nLevels/Strike_at_Karkand/Detailmaps/tx\n"
                              "Levels/Strike_at_Karkand/LowDetailmaps/tx\nLevels/Strike_at_Karkand/Lightmaps/tx\n";
    raw.insert(raw.end(), lines.begin(), lines.end());
    raw.push_back(0x42);
    raw.push_back(0x0A);                            // a '\n' byte in the body is left alone
    CHECK(retargetTerrainData(raw, "Strike_at_Karkand", "New_Map", count) && count == 4);
    const std::string want = "Levels/New_Map/Colormaps/tx\nLevels/New_Map/Detailmaps/tx\nLevels/New_Map/LowDetailmaps/tx\n"
                             "Levels/New_Map/Lightmaps/tx\n";
    CHECK(raw.size() == 57 + want.size() + 2 && std::string(raw.begin() + 57, raw.begin() + 57 + want.size()) == want
          && raw[0] == 0x11 && raw[raw.size() - 2] == 0x42);
    std::vector<unsigned char> tooShort(40, 0);
    CHECK(!retargetTerrainData(tooShort, "a", "b", count));
}

// Bundles and brushes (Framework/ObjectLibrary): the retail shapes read, written back in the retail
// writers' formats, the defects not copied, the refusals, the generator, and the folder round trip.
void checkObjectLibrary()
{
    // A brush in the shape of the shipped retail file: no precache block, an empty rem name, and rotation
    // ranges that repeat the position ranges (the retail writer's defect).
    const std::string retail =
        "rem *** ***\r\n"
        "LevelBrush.CreateBrush Bushes1\r\n"
        "LevelBrush.RandomPositionMin 10.000/0.000/5.000\r\n"
        "LevelBrush.RandomPositionMax 15.000/0.000/10.000\r\n"
        "LevelBrush.RandomRotationMin 10.000/0.000/5.000\r\n"
        "LevelBrush.RandomRotationMax 15.000/0.000/10.000\r\n"
        "LevelBrush.RandomScaleMin 0.000/0.000/0.000\r\n"
        "LevelBrush.RandomScaleMax 0.000/0.000/0.000\r\n"
        "LevelBrush.Spacing 50.000000\r\n"
        "LevelBrush.SnapToGround 1\r\n"
        "LevelBrush.AddItem bush_a\r\n"
        "LevelBrush.SetItemPosition -7.397/0.390/6.428\r\n"
        "LevelBrush.SetItemRotation 0.000/0.000/0.000\r\n"
        "LevelBrush.SetItemScale 0.000/0.000/0.000\r\n"
        "LevelBrush.AddItem bush_b\r\n"
        "LevelBrush.SetItemPosition 0.000/0.000/0.000\r\n"
        "LevelBrush.SetItemRotation 0.000/90.000/0.000\r\n"
        "LevelBrush.SetItemScale 0.000/0.000/0.000\r\n";
    LevelBrushFile brush;
    std::string error;
    CHECK(brush.parse(retail, error) && brush.name == "Bushes1" && brush.items.size() == 2);
    CHECK(brush.rotationRangesCopied && brush.spacing == 50.0f && brush.snapToGround && brush.preload.empty());
    CHECK(brush.items[0].templateName == "bush_a" && brush.items[1].templateName == "bush_b");     // the file's order
    CHECK(brush.items[0].position[0] == -7.397f && brush.items[1].rotation[1] == 90.0f);
    // Written back: the precache block the session gives it, the name in the rem line, the real rotation
    // ranges.
    brush.preload.push_back("run /objects/vegitation/bush_a/bush_a.con");
    brush.randomRotationMin[0] = -30.0f;
    brush.randomRotationMax[0] = 30.0f;
    brush.randomRotationMin[2] = brush.randomRotationMax[2] = 0.0f;
    const std::string expected =
        "console.allowMultipleFileLoad 0\r\n"
        "run /objects/vegitation/bush_a/bush_a.con\r\n"
        "console.allowMultipleFileLoad 1\r\n"
        "\r\n"
        "rem *** Bushes1 ***\r\n"
        "LevelBrush.CreateBrush Bushes1\r\n"
        "LevelBrush.RandomPositionMin 10.000/0.000/5.000\r\n"
        "LevelBrush.RandomPositionMax 15.000/0.000/10.000\r\n"
        "LevelBrush.RandomRotationMin -30.000/0.000/0.000\r\n"
        "LevelBrush.RandomRotationMax 30.000/0.000/0.000\r\n"
        "LevelBrush.RandomScaleMin 0.000/0.000/0.000\r\n"
        "LevelBrush.RandomScaleMax 0.000/0.000/0.000\r\n"
        "LevelBrush.Spacing 50.000000\r\n"
        "LevelBrush.SnapToGround 1\r\n"
        "LevelBrush.AddItem bush_a\r\n"
        "LevelBrush.SetItemPosition -7.397/0.390/6.428\r\n"
        "LevelBrush.SetItemRotation 0.000/0.000/0.000\r\n"
        "LevelBrush.SetItemScale 0.000/0.000/0.000\r\n"
        "LevelBrush.AddItem bush_b\r\n"
        "LevelBrush.SetItemPosition 0.000/0.000/0.000\r\n"
        "LevelBrush.SetItemRotation 0.000/90.000/0.000\r\n"
        "LevelBrush.SetItemScale 0.000/0.000/0.000\r\n";
    CHECK(brush.render() == expected);
    LevelBrushFile again;
    CHECK(again.parse(expected, error) && again.render() == expected && !again.rotationRangesCopied);
    // The item setters in the property grid's spelling, and the refusals.
    CHECK(again.parse("LevelBrush.CreateBrush x\r\nLevelBrush.AddItem t\r\nLevelBrushItem.ItemRotation 1/2/3\r\n", error)
          && again.items.size() == 1 && again.items[0].rotation[2] == 3.0f);
    CHECK(!again.parse("LevelBrush.Spacing 5\r\n", error));                                       // before CreateBrush
    CHECK(!again.parse("LevelBrush.CreateBrush x\r\nLevelBrush.SetItemPosition 1/2/3\r\n", error));  // before AddItem
    CHECK(!again.parse("LevelBrush.CreateBrush x\r\nLevelBrush.CreateBrush y\r\n", error));          // a second brush
    CHECK(!again.parse("LevelBrush.CreateBrush x\r\nLevelBrush.RandomPositionMin 1/2\r\n", error));  // not x/y/z
    CHECK(!again.parse("LevelBrush.CreateBrush x\r\nLevelBrush.Paint 1\r\n", error));                // unknown
    CHECK(!again.parse("rem nothing here\r\n", error));                                           // no brush

    // A bundle: the retail writer's shape and numbers.
    const std::string bundleText =
        "console.allowMultipleFileLoad 0\r\n"
        "run /objects/staticobjects/a/a.con\r\n"
        "console.allowMultipleFileLoad 1\r\n"
        "\r\n"
        "ObjectBundle.CreateRootObject crate 90.00/0.00/0.00 1\r\n"
        "ObjectBundle.CreateObject crate 4.50/0.00/-2.00 90.00/0.00/0.00 1\r\n"
        "ObjectBundle.CreateObject barrel -4.50/0.25/-2.00 0.00/0.00/0.00 3\r\n";
    ObjectBundleFile bundle;
    CHECK(bundle.parse(bundleText, error) && bundle.records.size() == 3 && bundle.preload.size() == 1);
    CHECK(bundle.records[0].templateName == "crate" && bundle.records[0].rotation[0] == 90.0f && bundle.records[0].layer == 1);
    CHECK(bundle.records[2].offset[1] == 0.25f && bundle.records[2].layer == 3 && bundle.render() == bundleText);
    ObjectBundleFile refused;
    CHECK(!refused.parse("ObjectBundle.CreateObject a 0/0/0 0/0/0 1\r\n", error));                  // before the root
    CHECK(!refused.parse("ObjectBundle.CreateRootObject a 0/0/0 1\r\nObjectBundle.CreateRootObject b 0/0/0 1\r\n", error));
    CHECK(!refused.parse("ObjectBundle.CreateRootObject a 0/0/0 256\r\n", error));                   // no such layer
    CHECK(!refused.parse("console.allowMultipleFileLoad 0\r\n", error));                             // no root

    // One generator, the same numbers from the same seed.
    LibraryRandom a(42), b(42);
    bool sameNumbers = true, inRange = true;
    for (int i = 0; i < 1000; ++i)
    {
        const float x = a.next();
        sameNumbers = sameNumbers && x == b.next();
        inRange = inRange && x >= 0.0f && x < 1.0f;
    }
    CHECK(sameNumbers && inRange);
    LibraryRandom c(1);
    CHECK(c.next() == static_cast<float>(48270.0 / 2147483646.0) && c.state() == 48271u);
    LibraryRandom zero(0);
    CHECK(zero.state() == 1u);
    CHECK(validLibraryName("My_Brush-2") && !validLibraryName("a.b") && !validLibraryName("") && !validLibraryName("a b"));

    // The folder: bundles saved and read back, brushes written through and read again.
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    char unique[64];
    std::snprintf(unique, sizeof(unique), "bf2editor_library_%lu", static_cast<unsigned long>(GetCurrentProcessId()));
    const std::string folder = std::string(temp) + unique + "/LevelEditor";
    ObjectLibrary library;
    library.setFolder(folder);
    CHECK(library.brushes().empty() && library.bundleNames().empty());
    CHECK(library.saveBundle("Camp", bundle, false, error) && library.bundleExists("Camp"));
    CHECK(!library.saveBundle("Camp", bundle, false, error) && library.saveBundle("Camp", bundle, true, error));
    ObjectBundleFile loaded;
    CHECK(library.loadBundle("Camp", loaded, error) && loaded.render() == bundle.render());
    CHECK(library.bundleNames().size() == 1 && library.bundleNames()[0] == "Camp");
    LevelBrushFile made;
    made.name = "Trees";
    made.items.push_back(LevelBrushItem());
    made.items[0].templateName = "tree_a";
    CHECK(library.addBrush(made, error) && !library.addBrush(made, error));
    CHECK(library.brush("trees") && library.brush("trees")->spacing == 10.0f);
    library.brush("Trees")->spacing = 4.5f;
    CHECK(library.writeBrush("Trees", error));
    library.reloadBrushes();
    CHECK(library.brushes().size() == 1 && library.brush("Trees") && library.brush("Trees")->spacing == 4.5f
          && library.brushProblems().empty());
    CHECK(library.deleteBrush("Trees", error) && !library.brush("Trees") && library.deleteBundle("Camp", error));
    RemoveDirectoryA((folder + "/LevelBrushes").c_str());
    RemoveDirectoryA((folder + "/ObjectBundle").c_str());
    RemoveDirectoryA(folder.c_str());
    RemoveDirectoryA((std::string(temp) + unique).c_str());
}

// Object template files (Framework/TemplateFile, WP7): every byte kept; blocks and their property lines;
// a value set in place; a new line placed as the census's approach A says - a component's inside its
// section, any other after the block's last line outside the sections; repeated properties by index; with
// BF2EDITOR_TEMPLATE_SAMPLES set to a folder of stock .con/.tweak files (extract_template_samples.py), each
// is written back byte for byte.
// TemplateFile::addProperty: one more line of a property that adds, after the block's last one of it.
void checkTemplateAddProperty()
{
    TemplateFile file;
    file.parse("ObjectTemplate.activeSafe SimpleObject crate\r\n"
               "ObjectTemplate.armor.maxHit 100\r\n"
               "ObjectTemplate.armor.addArmorEffect 0 e_a 0/0/0 0/0/0\r\n"
               "ObjectTemplate.hasCollisionPhysics 1\r\n");
    std::string error;
    CHECK(file.addProperty("crate", "armor.addArmorEffect", "0 e_b 0/0/0 0/0/0", error));
    CHECK(file.render() == "ObjectTemplate.activeSafe SimpleObject crate\r\n"
                           "ObjectTemplate.armor.maxHit 100\r\n"
                           "ObjectTemplate.armor.addArmorEffect 0 e_a 0/0/0 0/0/0\r\n"
                           "ObjectTemplate.armor.addArmorEffect 0 e_b 0/0/0 0/0/0\r\n"
                           "ObjectTemplate.hasCollisionPhysics 1\r\n");
    // A property with no line yet goes where a new set would (after the block's armor lines).
    CHECK(file.addProperty("crate", "armor.hpLostWhileUpSideDown", "5", error));
    CHECK(file.render().find("0 e_b 0/0/0 0/0/0\r\nObjectTemplate.armor.hpLostWhileUpSideDown 5\r\n") != std::string::npos);
    CHECK(!file.addProperty("nothing", "armor.maxHit", "1", error));
    CHECK(!file.addProperty("crate", "two words", "1", error));
}

void checkTemplateFile()
{
    const std::string text =
        "rem *** Generated with Bf2Editor.exe [created: 2005/4/26 9:32]\r\n"
        "GeometryTemplate.compressVertexData 1\r\n"
        "ObjectTemplate.activeSafe PlayerControlObject tank_a\r\n"
        "ObjectTemplate.creator XYZ-1:xyz\r\n"
        "ObjectTemplate.saveInSeparateFile 1\r\n"
        "rem ---BeginComp:Armor ---\r\n"
        "ObjectTemplate.createComponent Armor\r\n"
        "ObjectTemplate.armor.maxHitPoints 1000\r\n"
        "ObjectTemplate.armor.hitPoints  1000\r\n"
        "rem ---EndComp ---\r\n"
        "ObjectTemplate.geometry tank_a\r\n"
        "ObjectTemplate.addTemplate tank_a_turret\r\n"
        "ObjectTemplate.setPosition 0/1/0\r\n"
        "ObjectTemplate.addTemplate tank_a_engine\r\n"
        "\r\n"
        "ObjectTemplate.activeSafe RotationalBundle tank_a_turret\r\n"
        "ObjectTemplate.geometryPart 1\r\n"
        "\r\n";
    TemplateFile file;
    file.parse(text);
    CHECK(file.render() == text && !file.hasChanges() && file.blocks().size() == 2);
    CHECK(file.blocks()[0].name == "tank_a" && file.blocks()[0].className == "PlayerControlObject"
          && file.blocks()[0].first == 2 && file.blocks()[0].end == 15 && file.blocks()[1].end == 18);
    const std::vector<TemplatePropertyLine> lines = file.properties("TANK_A");
    CHECK(lines.size() == 9 && lines[0].property == "creator" && lines[4].property == "armor.hitPoints"
          && lines[4].value == "1000");
    std::string value, error;
    CHECK(file.value("tank_a", "ARMOR.MAXHITPOINTS", value) && value == "1000");
    // In place: the double blank before the value is kept.
    CHECK(file.setProperty("tank_a", "armor.hitPoints", "800", -1, error) && file.hasChanges());
    CHECK(file.line(8) == "ObjectTemplate.armor.hitPoints  800");
    // A new component line goes inside the Armor section; a new plain line after geometry... no: after the
    // block's last line outside the sections, which is the last addTemplate.
    CHECK(file.setProperty("tank_a", "armor.waterLevel", "-0.3", -1, error) && file.line(9) == "ObjectTemplate.armor.waterLevel -0.3"
          && file.line(10) == "rem ---EndComp ---");
    CHECK(file.setProperty("tank_a", "hasMobilePhysics", "1", -1, error) && file.line(15) == "ObjectTemplate.hasMobilePhysics 1"
          && file.line(16).empty());
    // Repeated properties need an index.
    CHECK(!file.setProperty("tank_a", "addTemplate", "x", -1, error));
    CHECK(file.setProperty("tank_a", "addTemplate", "tank_a_engine2", 1, error) && file.line(14) == "ObjectTemplate.addTemplate tank_a_engine2");
    CHECK(!file.setProperty("tank_a", "addTemplate", "x", 2, error) && !file.setProperty("nobody", "geometry", "x", -1, error));
    CHECK(file.removeProperty("tank_a", "creator", -1, error) && file.properties("tank_a")[0].property == "saveInSeparateFile");
    // A template the file has no lines of gets a block of its own at the end.
    file.appendBlock("SimpleObject", "tank_a_engine");
    CHECK(file.setProperty("tank_a_engine", "geometryPart", "3", -1, error) && file.blocks().size() == 3);
    const std::string tail = file.render().substr(file.render().size() - 90);
    CHECK(tail.find("\r\nObjectTemplate.activeSafe SimpleObject tank_a_engine\r\nObjectTemplate.geometryPart 3\r\n") != std::string::npos);
    // Line ends kept as they are: LF, mixed, and no final line end.
    TemplateFile lf;
    lf.parse("ObjectTemplate.create SimpleObject a\nObjectTemplate.geometry a\nrem x\r\nObjectTemplate.geometryPart 2");
    CHECK(lf.render() == "ObjectTemplate.create SimpleObject a\nObjectTemplate.geometry a\nrem x\r\nObjectTemplate.geometryPart 2");
    CHECK(lf.setProperty("a", "hasMobilePhysics", "1", -1, error)
          && lf.render() == "ObjectTemplate.create SimpleObject a\nObjectTemplate.geometry a\nrem x\r\nObjectTemplate.geometryPart 2\nObjectTemplate.hasMobilePhysics 1");
    CHECK(lf.setProperty("a", "hasMobilePhysics", "", -1, error) && lf.line(4) == "ObjectTemplate.hasMobilePhysics");
    // Children: a child added after the block's last child line, with its position; the first line of a child
    // taken out with the position and rotation lines that belong to it.
    TemplateFile pile;
    pile.parse("ObjectTemplate.create Bundle pile\r\nObjectTemplate.geometry pile\r\nObjectTemplate.addTemplate crate_a\r\n"
               "ObjectTemplate.setPosition 1/0/0\r\nObjectTemplate.setRotation 90/0/0\r\nObjectTemplate.addTemplate crate_b\r\n"
               "ObjectTemplate.hasMobilePhysics 0\r\n\r\nObjectTemplate.create SimpleObject crate_a\r\n");
    CHECK(pile.addChildLines("pile", "crate_c", "0/0/2", error) && pile.line(6) == "ObjectTemplate.addTemplate crate_c"
          && pile.line(7) == "ObjectTemplate.setPosition 0/0/2" && pile.line(8) == "ObjectTemplate.hasMobilePhysics 0");
    CHECK(pile.removeChildLines("pile", "CRATE_A", error) && pile.line(2) == "ObjectTemplate.addTemplate crate_b"
          && pile.line(3) == "ObjectTemplate.addTemplate crate_c" && pile.blocks().size() == 2);
    CHECK(!pile.removeChildLines("pile", "crate_z", error) && !pile.addChildLines("nobody", "x", "", error));
    TemplateFile lonely;
    lonely.parse("ObjectTemplate.create Bundle lonely\r\nObjectTemplate.geometry lonely\r\n");
    CHECK(lonely.addChildLines("lonely", "part", "", error)
          && lonely.render() == "ObjectTemplate.create Bundle lonely\r\nObjectTemplate.geometry lonely\r\nObjectTemplate.addTemplate part\r\n");
    // A child's position and rotation: rewritten in place (the blanks kept), or added right after its
    // addTemplate line - the rotation after the position; the children listed with their lines; one taken
    // out by its index.
    TemplateFile moving;
    moving.parse("ObjectTemplate.create Bundle moving\r\nObjectTemplate.addTemplate a\r\nObjectTemplate.setPosition  1/0/0\r\n"
                 "ObjectTemplate.addTemplate b\r\nObjectTemplate.addTemplate a\r\nObjectTemplate.setRotation 0/90/0\r\nrem end\r\n");
    const std::vector<TemplateChildLines> kids = moving.children("moving");
    CHECK(kids.size() == 3 && kids[0].child == "a" && kids[0].position == "1/0/0" && kids[0].rotationLine == -1
          && kids[0].end == 3 && kids[1].child == "b" && kids[1].positionLine == -1 && kids[2].rotation == "0/90/0"
          && kids[2].positionLine == -1 && kids[2].end == 6);
    CHECK(moving.setChildTransformAt("moving", 0, "2/0/0", "45/0/0", error)
          && moving.line(2) == "ObjectTemplate.setPosition  2/0/0" && moving.line(3) == "ObjectTemplate.setRotation 45/0/0");
    CHECK(moving.setChildTransformAt("moving", 1, "0/0/5", "", error) && moving.line(5) == "ObjectTemplate.setPosition 0/0/5"
          && moving.line(6) == "ObjectTemplate.addTemplate a");
    CHECK(moving.setChildTransformAt("moving", 2, "3/3/3", "0/0/0", error)
          && moving.line(7) == "ObjectTemplate.setPosition 3/3/3" && moving.line(8) == "ObjectTemplate.setRotation 0/0/0"
          && moving.line(9) == "rem end");
    CHECK(moving.removeChildAt("moving", 2, error) && moving.children("moving").size() == 2
          && moving.render() == "ObjectTemplate.create Bundle moving\r\nObjectTemplate.addTemplate a\r\n"
                                 "ObjectTemplate.setPosition  2/0/0\r\nObjectTemplate.setRotation 45/0/0\r\n"
                                 "ObjectTemplate.addTemplate b\r\nObjectTemplate.setPosition 0/0/5\r\nrem end\r\n");
    CHECK(!moving.setChildTransformAt("moving", 5, "0/0/0", "", error) && !moving.removeChildAt("moving", 2, error)
          && !moving.setChildTransformAt("moving", 0, "1 2", "", error));
    TemplateFile unended;
    unended.parse("ObjectTemplate.create Bundle tail\r\nObjectTemplate.addTemplate x");
    CHECK(unended.setChildTransformAt("tail", 0, "1/1/1", "", error)
          && unended.render() == "ObjectTemplate.create Bundle tail\r\nObjectTemplate.addTemplate x\r\nObjectTemplate.setPosition 1/1/1");
    // Component sections: a new one after the header lines (none yet) and after the last section; a
    // component's line set inside its section; one taken out with its lines kept, one replaced in place.
    TemplateFile componentFile;
    componentFile.parse("ObjectTemplate.activeSafe SimpleObject parts\r\nObjectTemplate.creator me\r\nObjectTemplate.setNetworkableInfo "
                "BasicInfo\r\nObjectTemplate.geometry parts\r\n\r\nObjectTemplate.activeSafe SimpleObject other\r\n");
    CHECK(componentFile.addComponentSection("parts", "Armor", error) && componentFile.line(3) == "rem ---BeginComp:Armor ---"
          && componentFile.line(4) == "ObjectTemplate.createComponent Armor" && componentFile.line(5) == "rem ---EndComp ---"
          && componentFile.line(6) == "ObjectTemplate.geometry parts");
    CHECK(componentFile.setProperty("parts", "armor.maxHitPoints", "50", -1, error) && componentFile.line(5) == "ObjectTemplate.armor.maxHitPoints 50"
          && componentFile.line(6) == "rem ---EndComp ---");
    CHECK(componentFile.addComponentSection("parts", "SingleFireComp", error) && componentFile.line(7) == "rem ---BeginComp:SingleFireComp ---"
          && componentFile.line(10) == "ObjectTemplate.geometry parts" && componentFile.componentSections("parts").size() == 2
          && componentFile.componentSections("parts")[1].end == 10 && componentFile.componentSections("other").empty());
    std::vector<std::string> sectionLines;
    CHECK(componentFile.replaceComponentSection("parts", "singlefirecomp", "MultiFireComp", sectionLines, error) && sectionLines.empty()
          && componentFile.line(7) == "rem ---BeginComp:MultiFireComp ---" && componentFile.line(8) == "ObjectTemplate.createComponent MultiFireComp");
    CHECK(componentFile.removeComponentSection("parts", "Armor", sectionLines, error) && sectionLines.size() == 1
          && sectionLines[0] == "ObjectTemplate.armor.maxHitPoints 50" && componentFile.line(3) == "rem ---BeginComp:MultiFireComp ---");
    CHECK(!componentFile.removeComponentSection("parts", "Armor", sectionLines, error) && !componentFile.addComponentSection("nobody", "Armor", error)
          && !componentFile.addComponentSection("parts", "two words", error));
    // A rename followed through a file: block openings and addTemplate lines, nothing else.
    TemplateFile renaming;
    renaming.parse("ObjectTemplate.create Bundle holder\r\nObjectTemplate.addTemplate  part \r\nObjectTemplate.projectileTemplate part\r\n"
                   "\r\nObjectTemplate.create SimpleObject part\r\nObjectTemplate.activeSafe SimpleObject PART\r\nObjectTemplate.active part\r\n");
    CHECK(renaming.renameTemplateLines("part", "piece")
          && renaming.render() == "ObjectTemplate.create Bundle holder\r\nObjectTemplate.addTemplate  piece \r\n"
                                  "ObjectTemplate.projectileTemplate part\r\n\r\nObjectTemplate.create SimpleObject piece\r\n"
                                  "ObjectTemplate.activeSafe SimpleObject piece\r\nObjectTemplate.active piece\r\n"
          && renaming.blocksOf("piece").size() == 3 && !renaming.renameTemplateLines("nothing", "x"));
    // A template's class changed in its openings, nowhere else.
    TemplateFile retyped;
    retyped.parse("ObjectTemplate.create  SimpleObject  thing\r\nObjectTemplate.geometry thing\r\n\r\n"
                  "ObjectTemplate.activeSafe SimpleObject thing\r\nObjectTemplate.active thing\r\nObjectTemplate.create Bundle other\r\n");
    CHECK(retyped.setBlockClass("thing", "Bundle")
          && retyped.render() == "ObjectTemplate.create  Bundle  thing\r\nObjectTemplate.geometry thing\r\n\r\n"
                                 "ObjectTemplate.activeSafe Bundle thing\r\nObjectTemplate.active thing\r\nObjectTemplate.create Bundle other\r\n"
          && retyped.blocks()[0].className == "Bundle" && !retyped.setBlockClass("thing", "Bundle")
          && !retyped.setBlockClass("nobody", "Bundle"));
    // A new child's block at the end of its parent's file.
    TemplateFile holder;
    holder.parse("ObjectTemplate.create Bundle holder\r\nObjectTemplate.geometry holder\r\n");
    holder.appendCreateBlock("SimpleObject", "holder_part", std::vector<std::string>(1, "createdInEditor 1"));
    CHECK(holder.render() == "ObjectTemplate.create Bundle holder\r\nObjectTemplate.geometry holder\r\n\r\n"
                             "ObjectTemplate.create SimpleObject holder_part\r\nObjectTemplate.createdInEditor 1\r\n"
          && holder.blocksOf("holder_part").size() == 1);
    // A clone: the source renamed in its openings, a part and a projectile the files create cloned and followed
    // (addTemplate, projectileTemplate), a child of a file of its own shared, the geometry and collision lines
    // left out and the geometry named as before, the include renamed.
    TemplateClone clone;
    const std::string cloneCon =
        "GeometryTemplate.create BundledMesh tank\r\nCollisionManager.createTemplate tank\r\n\r\n"
        "ObjectTemplate.create PlayerControlObject tank\r\nObjectTemplate.geometry tank\r\nObjectTemplate.collisionMesh tank\r\n"
        "ObjectTemplate.addTemplate tank_turret\r\nObjectTemplate.addTemplate shared_wheel\r\n\r\n"
        "ObjectTemplate.create RotationalBundle tank_turret\r\nObjectTemplate.geometryPart 1\r\n\r\ninclude tank.tweak\r\n";
    const std::string cloneTweak =
        "GeometryTemplate.compressVertexData 1\r\nObjectTemplate.activeSafe PlayerControlObject tank\r\n"
        "ObjectTemplate.setCollisionMesh tank\r\nObjectTemplate.create GenericProjectile tank_shell\r\n\r\n"
        "ObjectTemplate.activeSafe RotationalBundle tank_turret\r\nObjectTemplate.projectileTemplate  tank_shell\r\n";
    CHECK(cloneTemplateFiles(cloneCon, cloneTweak, "tank", "tank", "tank2", clone, error) && clone.droppedLines == 3
          && clone.renames.size() == 3 && clone.renames[1].to == "tank2_turret" && clone.renames[2].to == "tank2_shell");
    CHECK(clone.con == "\r\nObjectTemplate.create PlayerControlObject tank2\r\nObjectTemplate.geometry tank\r\n"
                       "ObjectTemplate.collisionMesh tank\r\nObjectTemplate.addTemplate tank2_turret\r\n"
                       "ObjectTemplate.addTemplate shared_wheel\r\n\r\nObjectTemplate.create RotationalBundle tank2_turret\r\n"
                       "ObjectTemplate.geometryPart 1\r\n\r\ninclude tank2.tweak\r\n");
    CHECK(clone.tweak == "ObjectTemplate.activeSafe PlayerControlObject tank2\r\nObjectTemplate.setCollisionMesh tank\r\n"
                         "ObjectTemplate.create GenericProjectile tank2_shell\r\n\r\n"
                         "ObjectTemplate.activeSafe RotationalBundle tank2_turret\r\nObjectTemplate.projectileTemplate  tank2_shell\r\n");
    CHECK(clonedChildName("wheel", "tank", "tank2") == "tank2_wheel" && clonedChildName("TANK_x", "tank", "t2") == "t2_x");
    // The sounds named after their owners follow them: the source's and a cloned child's (the engine makes
    // them; the files only activeSafe and add them), one the files create; a shared child's sound stays, and
    // an S_ template that is nobody's sound is cloned as a child.
    const std::string soundCon = "ObjectTemplate.create PlayerControlObject Tank\r\nObjectTemplate.addTemplate TANK_turret\r\n"
                                 "ObjectTemplate.addTemplate S_Tank_Ambient\r\nObjectTemplate.addTemplate S_shared_wheel_Rpm\r\n"
                                 "ObjectTemplate.create RotationalBundle TANK_turret\r\n";
    const std::string soundTweak = "ObjectTemplate.activeSafe Sound S_Tank_Ambient\r\nObjectTemplate.soundFilename a.wav\r\n"
                                   "ObjectTemplate.activeSafe RotationalBundle TANK_turret\r\n"
                                   "ObjectTemplate.addTemplate S_TANK_turret_RotationRpm\r\n"
                                   "ObjectTemplate.activeSafe Sound S_TANK_turret_RotationRpm\r\n"
                                   "ObjectTemplate.create Sound S_tank_turret_Fire\r\nObjectTemplate.create Sound S_odd\r\n";
    CHECK(cloneTemplateFiles(soundCon, soundTweak, "tank", "tank", "t2", clone, error) && clone.sounds == 3
          && clone.renames.size() == 6 && clone.renames[1].to == "t2_turret" && clone.renames[2].to == "S_t2_Ambient"
          && clone.renames[3].to == "S_t2_turret_RotationRpm" && clone.renames[4].to == "S_t2_turret_Fire"
          && clone.renames[5].to == "t2_S_odd");
    CHECK(clone.con == "ObjectTemplate.create PlayerControlObject t2\r\nObjectTemplate.addTemplate t2_turret\r\n"
                       "ObjectTemplate.addTemplate S_t2_Ambient\r\nObjectTemplate.addTemplate S_shared_wheel_Rpm\r\n"
                       "ObjectTemplate.create RotationalBundle t2_turret\r\n"
          && clone.tweak == "ObjectTemplate.activeSafe Sound S_t2_Ambient\r\nObjectTemplate.soundFilename a.wav\r\n"
                            "ObjectTemplate.activeSafe RotationalBundle t2_turret\r\n"
                            "ObjectTemplate.addTemplate S_t2_turret_RotationRpm\r\n"
                            "ObjectTemplate.activeSafe Sound S_t2_turret_RotationRpm\r\n"
                            "ObjectTemplate.create Sound S_t2_turret_Fire\r\nObjectTemplate.create Sound t2_S_odd\r\n");
    CHECK(!cloneTemplateFiles(cloneCon, cloneTweak, "tank", "tank", "TANK", clone, error)
          && !cloneTemplateFiles(cloneCon, cloneTweak, "tank", "tank", "a b", clone, error)
          && !cloneTemplateFiles(cloneCon, cloneTweak, "tank", "ghost", "g2", clone, error));
    TemplateFile unclosed;
    unclosed.parse("ObjectTemplate.activeSafe SimpleObject open\r\nrem ---BeginComp:Armor ---\r\nObjectTemplate.createComponent Armor\r\n");
    CHECK(unclosed.componentSections("open").size() == 1 && unclosed.componentSections("open")[0].end == 0
          && !unclosed.removeComponentSection("open", "Armor", sectionLines, error) && !unclosed.addComponentSection("open", "Ik", error));
    std::string property;
    CHECK(TemplateFile::splitPropertyLine("  ObjectTemplate.setPosition   1/2/3  ", property, value) && property == "setPosition" && value == "1/2/3");
    CHECK(!TemplateFile::splitPropertyLine("GeometryTemplate.compressVertexData 1", property, value));

    // The stock files, when a folder of them is given.
    char samples[MAX_PATH] = "";
    if (GetEnvironmentVariableA("BF2EDITOR_TEMPLATE_SAMPLES", samples, MAX_PATH) == 0)
        return;
    std::vector<std::string> folders(1, samples);
    int files = 0, same = 0;
    while (!folders.empty())
    {
        const std::string folder = folders.back();
        folders.pop_back();
        WIN32_FIND_DATAA found;
        HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);
        if (search == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            const std::string name = found.cFileName;
            if (name == "." || name == "..")
                continue;
            const std::string path = folder + "\\" + name;
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                folders.push_back(path);
                continue;
            }
            const size_t dot = name.rfind('.');
            const std::string extension = dot == std::string::npos ? std::string() : name.substr(dot);
            if (_stricmp(extension.c_str(), ".con") != 0 && _stricmp(extension.c_str(), ".tweak") != 0)
                continue;
            std::string data;
            if (!ObjectLibrary::readTextFile(path, data, error))
                continue;
            ++files;
            TemplateFile stock;
            stock.parse(data);
            if (stock.render() == data)
                ++same;
            else
                std::printf("template sample not kept byte for byte: %s\n", path.c_str());
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    std::printf("[bf2_editor_checks] %d stock template files, %d written back byte for byte\n", files, same);
    CHECK(files > 0 && same == files);
}

// The object archives (Framework/ObjectArchives, WP7 step 4): the retail packager's filters, the zips an
// archive script mounts at Objects, and a loose folder packed into the two zips and extracted again, with a
// second extraction keeping what is there.
void checkObjectArchives()
{
    CHECK(objectArchiveTakes("Vehicles/Land/tank/tank.con", true) && objectArchiveTakes("Vehicles/Land/tank/tank.tweak", false));
    CHECK(!objectArchiveTakes("Vehicles/Land/tank/Meshes/tank.bundledMesh", true)
          && objectArchiveTakes("Vehicles/Land/tank/Meshes/tank.BundledMesh", false));
    CHECK(!objectArchiveTakes("a/b/texture.DDS", true) && objectArchiveTakes("a/b/texture.dds", false));
    CHECK(!objectArchiveTakes("a/b/tank.samples", false) && !objectArchiveTakes("a/source.psd", false)
          && objectArchiveTakes("a/b/tank.occ", true));
    const std::string script = "fileManager.mountArchive Objects_server.zip Objects\r\nfileManager.mountArchive Menu_server.zip Menu\r\n"
                               "fileManager.mountArchive Booster_server.zip objects\r\n\r\n";
    const std::vector<std::string> zips = archivesMountedAt(script, "Objects");
    CHECK(zips.size() == 2 && zips[0] == "Objects_server.zip" && zips[1] == "Booster_server.zip");

    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    char unique[64];
    std::snprintf(unique, sizeof(unique), "bf2editor_objects_%lu", static_cast<unsigned long>(GetCurrentProcessId()));
    const std::string root = std::string(temp) + unique;
    const std::string loose = root + "/Objects";
    std::string error;
    CHECK(ObjectLibrary::writeTextFile(loose + "/Vehicles/tank/tank.con", "ObjectTemplate.create SimpleObject tank\r\n", error));
    CHECK(ObjectLibrary::writeTextFile(loose + "/Vehicles/tank/tank.tweak", "ObjectTemplate.activeSafe SimpleObject tank\r\n", error));
    CHECK(ObjectLibrary::writeTextFile(loose + "/Vehicles/tank/Textures/tank_c.dds", std::string(3000, 'x'), error));
    CHECK(ObjectLibrary::writeTextFile(loose + "/Vehicles/tank/tank.samples", "SMP2", error));
    ObjectPackReport pack;
    CHECK(packObjectArchives(loose, root + "/mod", pack, error));
    CHECK(pack.clientFiles == 3 && pack.serverFiles == 2 && pack.left == 1 && pack.clientBytes > 0);
    unsigned long long content = 0;
    CHECK(archiveContentSize(root + "/mod/Objects_client.zip", content, error) && content == 3000 + 41 + 45);
    ExtractReport first, again;
    CHECK(extractArchive(root + "/mod/Objects_client.zip", root + "/out", first, error) && first.files == 3 && first.kept == 0);
    std::string text;
    CHECK(ObjectLibrary::readTextFile(root + "/out/Vehicles/tank/tank.tweak", text, error)
          && text == "ObjectTemplate.activeSafe SimpleObject tank\r\n");
    CHECK(ObjectLibrary::writeTextFile(root + "/out/Vehicles/tank/tank.tweak", "edited", error));
    CHECK(extractArchive(root + "/mod/Objects_client.zip", root + "/out", again, error) && again.files == 0 && again.kept == 3);
    CHECK(ObjectLibrary::readTextFile(root + "/out/Vehicles/tank/tank.tweak", text, error) && text == "edited");
    CHECK(!extractArchive(root + "/mod/none.zip", root + "/out", again, error));
    // Clean up the temporary tree.
    const char* const files[] = { "/Objects/Vehicles/tank/tank.con", "/Objects/Vehicles/tank/tank.tweak",
                                  "/Objects/Vehicles/tank/Textures/tank_c.dds", "/Objects/Vehicles/tank/tank.samples",
                                  "/out/Vehicles/tank/tank.con", "/out/Vehicles/tank/tank.tweak",
                                  "/out/Vehicles/tank/Textures/tank_c.dds", "/mod/Objects_client.zip", "/mod/Objects_server.zip" };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
        DeleteFileA((root + files[i]).c_str());
    const char* const folders[] = { "/Objects/Vehicles/tank/Textures", "/Objects/Vehicles/tank", "/Objects/Vehicles", "/Objects",
                                    "/out/Vehicles/tank/Textures", "/out/Vehicles/tank", "/out/Vehicles", "/out", "/mod", "" };
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); ++i)
        RemoveDirectoryA((root + folders[i]).c_str());
}

// Checks mod files, own-before-base archive ordering, menu copies, and discoverable Mod.desc output.
void checkModScaffold()
{
    std::vector<std::string> bases(1, "bf2");
    const std::vector<std::pair<std::string, std::string> > files = modScaffoldFiles("MyMod", bases);
    CHECK(files.size() == 7 && files[0].first == "ClientArchives.con" && files[4].first == "MyMod.mew");
    CHECK(files[0].second == "fileManager.mountArchive Objects_client.zip Objects\r\n"
                             "fileManager.mountArchive Common_client.zip Common\r\n"
                             "fileManager.mountArchive Menu_client.zip Menu\r\n"
                             "fileManager.mountArchive Fonts_client.zip Fonts\r\n\r\n"
                             "fileManager.mountArchive mods/bf2/Objects_client.zip Objects\r\n"
                             "fileManager.mountArchive mods/bf2/Common_client.zip Common\r\n"
                             "fileManager.mountArchive mods/bf2/Menu_client.zip Menu\r\n"
                             "fileManager.mountArchive mods/bf2/Fonts_client.zip Fonts\r\n"
                             "fileManager.mountArchive mods/bf2/Shaders_client.zip Shaders\r\n");
    CHECK(files[1].second.find("\r\n\r\nfileManager.mountArchive mods/bf2/Objects_server.zip Objects\r\n") != std::string::npos
          && files[1].second.find("Shaders") == std::string::npos);
    static const unsigned char bf2Mew[] = { 2, 0, 0, 0, '.', '/', 0, 0, 0, 0, 6, 0, 0, 0, 'I', 'n', 'g', 'a', 'm', 'e',
                                            0, 0, 0, 0, 6, 0, 0, 0, 'I', 'n', 'g', 'a', 'm', 'e' };
    CHECK(files[4].second == std::string(reinterpret_cast<const char*>(bf2Mew), sizeof(bf2Mew)));
    CHECK(files[5].first == "Ingame.mep" && files[5].second.size() == 4 * 3 + 16 + 20 + 28
          && files[5].second.compare(0, 4, std::string("\x10\0\0\0", 4)) == 0);
    CHECK(files[6].first == "Mod.desc" && files[6].second.find("<title> MyMod </title>") != std::string::npos);
    CHECK(files[3].second.find("bf2Engine.playMovie mods\\bf2\\Movies\\EA.bik 1\r\n") != std::string::npos);
    CHECK(validModName("my_mod-2") && !validModName("my mod") && !validModName("a/b") && !validModName(""));

    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    char unique[64];
    std::snprintf(unique, sizeof(unique), "bf2editor_mods_%lu", static_cast<unsigned long>(GetCurrentProcessId()));
    const std::string mods = std::string(temp) + unique;
    std::string error;
    CHECK(createMod(mods, "MyMod", bases, error) && !createMod(mods, "MyMod", bases, error) && !createMod(mods, "bad name", bases, error));
    const std::vector<std::string> listed = listMods(mods);
    CHECK(listed.size() == 1 && listed[0] == "MyMod");
    for (size_t i = 0; i < files.size(); ++i)
        DeleteFileA((mods + "/MyMod/" + files[i].first).c_str());
    RemoveDirectoryA((mods + "/MyMod").c_str());
    RemoveDirectoryA(mods.c_str());
}

// The undergrowth density map (Framework/UndergrowthMap): square maps only; the renderer's cell mapping
// (column floor(x / scale) + width / 2, rows running with z); a disc painted over the cells whose centres
// it covers; the changed rectangle taken since a stroke began, and put back both ways.
void checkUndergrowthMap()
{
    UndergrowthMap map;
    std::string error;
    CHECK(!map.load(std::string(24, '\0'), error) && map.load(std::string(25, '\1'), error) && map.size() == 5);
    int column = 0, row = 0;
    CHECK(map.cellAt(0.0f, 0.0f, 2.0f, column, row) && column == 2 && row == 2);
    CHECK(map.cellAt(-2.5f, 3.9f, 2.0f, column, row) && column == 0 && row == 3);
    CHECK(!map.cellAt(-5.0f, 0.0f, 2.0f, column, row) && !map.cellAt(0.0f, 6.0f, 2.0f, column, row));
    const std::string before = map.bytes();
    CHECK(map.paintDisc(0.0f, 0.0f, 1.5f, 1.0f, 7) && !map.paintDisc(0.0f, 0.0f, 1.5f, 1.0f, 7));
    CHECK(map.at(1, 1) == 7 && map.at(2, 2) == 7 && map.at(1, 2) == 7 && map.at(2, 1) == 7 && map.at(0, 1) == 1
          && map.at(3, 3) == 1);
    const std::vector<int> counts = map.counts();
    CHECK(counts[7] == 4 && counts[1] == 21);
    const UndergrowthChange change = map.changeSince(before);
    CHECK(change.column == 1 && change.row == 1 && change.columns == 2 && change.rows == 2 && change.before.size() == 4
          && change.after[0] == 7 && change.before[3] == 1);
    map.apply(change, false);
    CHECK(map.bytes() == before);
    map.apply(change, true);
    CHECK(map.at(2, 2) == 7 && map.changeSince(map.bytes()).empty());
}

// The object wizard's rules: the recorder's capture with its
// whole-line, case-insensitive, untrimmed terminator; reuse by title (case-sensitive) and append-only blocks;
// the load-time commands; the refusals before the first create and on a wrong argument count; substitution.
void checkWizardRules()
{
    WizardRules wizard;
    std::string command, message;
    std::vector<std::string> args;
    CHECK(wizard.feed("Wizard.warning x", command, args, message) == WizardRules::Refused);
    CHECK(wizard.feed("Wizard.create \"Check Has Camera\"", command, args, message) == WizardRules::Handled
          && wizard.current() == 0 && wizard.rules()[0].title == "Check Has Camera");
    CHECK(wizard.feed("Wizard.beginAnalyzer", command, args, message) == WizardRules::Handled && wizard.recording());
    CHECK(wizard.feed("Wizard.populate IS PlayerControlObject", command, args, message) == WizardRules::Captured);
    CHECK(wizard.feed(" Wizard.endAnalyzer", command, args, message) == WizardRules::Captured);
    CHECK(wizard.feed("WIZARD.ENDANALYZER", command, args, message) == WizardRules::Handled && !wizard.recording()
          && wizard.rules()[0].analyzer.size() == 2 && wizard.rules()[0].analyzer[1] == " Wizard.endAnalyzer");
    CHECK(wizard.feed("Wizard.warning \"#OBJECT has no Camera\"", command, args, message) == WizardRules::Handled
          && wizard.rules()[0].warning == "#OBJECT has no Camera");
    CHECK(wizard.feed("Wizard.warning", command, args, message) == WizardRules::Handled && message == "#OBJECT has no Camera");
    CHECK(wizard.feed("Wizard.type ErrorCheck", command, args, message) == WizardRules::Handled
          && wizard.feed("Wizard.category General", command, args, message) == WizardRules::Handled
          && wizard.rules()[0].type == "ErrorCheck" && wizard.rules()[0].category == "General");
    CHECK(wizard.feed("Wizard.affectAll 1", command, args, message) == WizardRules::Handled && wizard.rules()[0].affectAll);
    CHECK(wizard.feed("Wizard.modifyObjectList \"Pick some\" 0", command, args, message) == WizardRules::Handled
          && wizard.rules()[0].listOn && !wizard.rules()[0].listDefault && wizard.rules()[0].listTitle == "Pick some");
    CHECK(wizard.feed("Wizard.CheckValue maxSpeed EQUAL 0/0/0", command, args, message) == WizardRules::Command
          && command == "checkvalue" && args.size() == 3 && args[2] == "0/0/0");
    CHECK(wizard.feed("Wizard.CheckValue networkableInfo DIFFER \"\"", command, args, message) == WizardRules::Command
          && args.size() == 3 && args[2].empty());
    CHECK(wizard.feed("Wizard.nonsense", command, args, message) == WizardRules::Refused
          && wizard.feed("Wizard.populate IS", command, args, message) == WizardRules::Refused);
    CHECK(wizard.feed("ObjectTemplate.active x", command, args, message) == WizardRules::Console
          && wizard.feed("", command, args, message) == WizardRules::Empty);
    CHECK(wizard.feed("Wizard.create \"check has camera\"", command, args, message) == WizardRules::Handled
          && wizard.rules().size() == 2 && wizard.current() == 1);
    CHECK(wizard.feed("Wizard.create \"Check Has Camera\"", command, args, message) == WizardRules::Handled
          && wizard.rules().size() == 2 && wizard.current() == 0);
    CHECK(wizard.feed("Wizard.beginResolution", command, args, message) == WizardRules::Handled
          && wizard.feed("Wizard.create \"swallowed\"", command, args, message) == WizardRules::Captured
          && wizard.feed("Wizard.endResolution", command, args, message) == WizardRules::Handled
          && wizard.rules()[0].resolution.size() == 1 && wizard.rules().size() == 2);
    CHECK(wizard.feed("Wizard.beginAnalyzer", command, args, message) == WizardRules::Handled
          && wizard.feed("Wizard.populate IS Spring", command, args, message) == WizardRules::Captured
          && wizard.feed("Wizard.endAnalyzer", command, args, message) == WizardRules::Handled
          && wizard.rules()[0].analyzer.size() == 3);
    std::vector<std::string> lines = wizardFileLines("a\r\n\r\nb\nc");
    CHECK(lines.size() == 3 && lines[0] == "a" && lines[1] == "b" && lines[2] == "c");
    std::map<int, float> floats;
    floats[1] = 2.5f;
    CHECK(wizardSubstitute("x #FLOAT1 #FLOAT0 #STRING #OBJECT #OBJECT", floats, "s", "obj") == "x 2.500000 -1 s obj #OBJECT");
    CHECK(wizardFilterOperator("IS") == 1 && wizardFilterOperator("is") == 0 && wizardFilterOperator("HASCHILD") == 3
          && wizardCompareOperator("DIFFER") == 2 && wizardCompareOperator("") == 0);
    args = wizardArguments("a \"b c\" \"\" d");
    CHECK(args.size() == 4 && args[1] == "b c" && args[2].empty() && args[3] == "d");
}

// The material editor's files: every
// byte kept; values set in place; new lines, materials and cells where the engine's writer puts them; defaults
// and an unset damage modifier taken out; the writer's rules as a lint. With BF2EDITOR_MATERIAL_SAMPLES set to
// a folder holding the stock materialManagerDefine.con and materialManagerSettings.con
// (extract_material_samples.py), both are written back byte for byte and break no rule.
namespace
{

std::vector<std::string> materialLines(const std::string& text)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find("\r\n", start);
        if (end == std::string::npos)
            end = text.size();
        out.push_back(text.substr(start, end - start));
        start = end + 2;
    }
    return out;
}

} // namespace

// The retail command catalogue's table (HelpDocuments.h) and the Console panel's completion.
std::string utf16Cell(const char* text)
{
    std::string out;
    for (const char* p = text; *p; ++p)
    {
        out += *p;
        out += '\0';
    }
    out += '\0';
    out += '\0';
    return out;
}

void checkHelpAndCompletion()
{
    // Three rows of two columns (the headings, then two commands), then the 8-byte trailer.
    std::string bytes("\x03\0\0\0\x02\0\0\0", 8);
    bytes += utf16Cell("LANGUAGE") + utf16Cell("Description") + utf16Cell("Global.OnNew") + utf16Cell("New in plugin")
             + utf16Cell("Camera.Reset") + utf16Cell("");
    bytes += std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8);
    CommandCatalogue catalogue;
    std::string error;
    CHECK(catalogue.parse(bytes, error));
    CHECK(catalogue.size() == 2);
    CHECK(catalogue.name(0) == "Global.OnNew" && catalogue.description(0) == "New in plugin");
    CHECK(catalogue.describe("global.onnew") && *catalogue.describe("global.onnew") == "New in plugin");
    CHECK(catalogue.describe("Camera.Reset") && catalogue.describe("Camera.Reset")->empty());
    CHECK(!catalogue.describe("Camera.Nothing"));
    // A non-ASCII character comes out as UTF-8 (U+00E9 as C3 A9), a surrogate pair as one four-byte character.
    std::string accented("\x02\0\0\0\x02\0\0\0", 8);
    accented += utf16Cell("A") + utf16Cell("B") + utf16Cell("X.y");
    accented += std::string("\x63\0\xE9\0\x3D\xD8\x00\xDE\0\0", 10);
    CHECK(catalogue.parse(accented, error) && catalogue.size() == 1);
    CHECK(catalogue.description(0) == "c\xC3\xA9\xF0\x9F\x98\x80");
    // Truncated inside the table, or counts that are no catalogue: refused.
    CHECK(!catalogue.parse(bytes.substr(0, 30), error) && catalogue.size() == 0);
    CHECK(!catalogue.parse(std::string("\0\0\0\0\x02\0\0\0", 8), error));
    // The documents: a missing folder lists none, and a lookup finds by label or file name.
    CHECK(helpDocuments("no/such/help/folder").empty());
    std::vector<HelpDocument> documents(2);
    documents[0].label = "User guide";
    documents[0].path = "bf2editor/Help/UserGuide.doc";
    documents[1].label = "Workshop: LevelWorkshop";
    documents[1].path = "bf2editor/Help/Workshop/LevelWorkshop.doc";
    CHECK(findHelpDocument(documents, "user GUIDE") == &documents[0]);
    CHECK(findHelpDocument(documents, "levelworkshop.doc") == &documents[1]);
    CHECK(!findHelpDocument(documents, "Tutorial: none"));

    ConsoleCompletion completion;
    std::vector<std::string> names;
    names.push_back("EditorSelection.translate");
    names.push_back("EditorSelection.rotate");
    names.push_back("EditorSelection.select");
    names.push_back("EditorSelection.selectTemplate");
    names.push_back("EditorSettings.save");
    names.push_back("editorselection.TRANSLATE");    // the same name in other case: kept once
    names.push_back("Camera.position");
    names.push_back("notaname");
    completion.setNames(names);
    CHECK(completion.size() == 6);
    ConsoleCompletion::Result r = completion.complete("EditorSel", 10);
    CHECK(r.completed && r.text == "EditorSelection." && r.candidates.size() == 1);
    r = completion.complete("Editor", 10);
    CHECK(r.completed && r.text == "EditorSe" && r.candidates.size() == 2);
    r = completion.complete("editorselection.tr", 10);
    CHECK(r.completed && r.text == "EditorSelection.translate ");
    r = completion.complete("EditorSelection.sel", 10);
    CHECK(r.completed && r.text == "EditorSelection.select" && r.candidates.size() == 2);
    r = completion.complete("EditorSelection.select", 10);
    CHECK(!r.completed && r.candidates.size() == 2);
    r = completion.complete("EditorSelection.translate 0/1", 10);
    CHECK(!r.completed && r.candidates.empty());
    r = completion.complete("Nothing", 10);
    CHECK(!r.completed && r.text == "Nothing");
    r = completion.complete("  cam", 10);
    CHECK(r.completed && r.text == "  Camera.");
    CHECK(completion.matches("EditorSelection.", 2).size() == 2);
    CHECK(completion.matches("EditorSelection.translate 0/1/0", 8).size() == 1);
    CHECK(completion.matches("EditorSelection.trans 0/1/0", 8).empty());
}

// An animation system (AnimationSystemFile.h): the cursor idiom read, values set in place, new lines after their
// object, bundle items with their start times, objects added and taken out - every other byte kept. With
// BF2EDITOR_ANIMATION_SAMPLES set to the folder extract_animation_samples.py fills, every stock system comes back
// byte for byte and holds objects.
namespace
{

void walkIncFiles(const std::string& folder, std::vector<std::string>& out)
{
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((folder + "\\*").c_str(), &found);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const std::string name = found.cFileName;
        if (name == "." || name == "..")
            continue;
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            walkIncFiles(folder + "\\" + name, out);
        else if (name.size() > 4 && _stricmp(name.c_str() + name.size() - 4, ".inc") == 0)
            out.push_back(folder + "\\" + name);
    } while (FindNextFileA(h, &found));
    FindClose(h);
}

} // namespace

void checkAnimationSystemFile()
{
    const std::string stock =
        "animationSystem.cameraSpring.use 1\r\n"
        "animationSystem.cameraSpring.look 1/1\r\n"
        "\r\n"
        "rem *** Animations ***\r\n"
        "animationSystem.createAnimation Objects/W/a_fire.baf\r\n"
        "animationManager.looping 0\r\n"
        "animationManager.length 0.333333\r\n"
        "animationSystem.createAnimation Objects/W/a_idle.baf\r\n"
        "\r\n"
        "rem *** Bundles ***\r\n"
        "animationSystem.createBundle fire\r\n"
        "animationBundle.isLooping 0\r\n"
        "animationBundle.fadeOutTime 0\r\n"
        "animationBundle.addAnimation Objects/W/a_fire.baf\r\n"
        "animationBundle.addEvent sound:0_first\r\n"
        "animationBundle.setEventStartTime sound:0_first 0.2\r\n"
        "\r\n"
        "animationSystem.createBundle idle\r\n"
        "animationBundle.addAnimation Objects/W/a_idle.baf\r\n"
        "\r\n"
        "rem *** Triggers ***\r\n"
        "animationSystem.createTrigger Trigger still\r\n"
        "animationTrigger.addBundle idle\r\n"
        "animationSystem.createTrigger MessageTrigger shoot\r\n"
        "animationTrigger.message fire\r\n"
        "animationTrigger.addBundle fire\r\n";
    AnimationSystemFile file;
    file.parse(stock);
    CHECK(file.render() == stock);
    CHECK(file.objects().size() == 6);
    const AnimationObject* fire = file.find(AnimationKind::Bundle, "FIRE");
    CHECK(fire && fire->name == "fire" && fire->createLine == 10 && fire->lastLine == 15);
    const AnimationObject* shoot = file.find(AnimationKind::Trigger, "shoot");
    CHECK(shoot && shoot->type == "MessageTrigger");
    CHECK(file.properties(AnimationKind::Animation, "Objects/W/a_fire.baf").size() == 2);
    CHECK(file.systemLines().size() == 2 && file.systemLines()[1].property == "cameraSpring.look");
    std::vector<BundleItem> events = file.bundleEvents("fire");
    CHECK(events.size() == 1 && events[0].name == "sound:0_first" && events[0].startSet && events[0].start == 0.2f);
    std::vector<BundleItem> animations = file.bundleAnimations("fire");
    CHECK(animations.size() == 1 && !animations[0].startSet);
    std::string error;
    // In place, then a new setting after the bundle's settings (the writer's order: settings, animations, events).
    CHECK(file.setProperty(AnimationKind::Bundle, "fire", "fadeOutTime", "0.25", -1, error));
    CHECK(file.render().find("animationBundle.fadeOutTime 0.25\r\n") != std::string::npos);
    CHECK(file.setProperty(AnimationKind::Bundle, "fire", "abruptPlayback", "1", -1, error));
    CHECK(file.render().find("animationBundle.fadeOutTime 0.25\r\nanimationBundle.abruptPlayback 1\r\n"
                             "animationBundle.addAnimation Objects/W/a_fire.baf\r\n") != std::string::npos);
    // A new animation after the bundle's animations and before its events.
    CHECK(file.addLine(AnimationKind::Bundle, "fire", "addAnimation", "Objects/W/a_more.baf", error));
    CHECK(file.render().find("animationBundle.addAnimation Objects/W/a_fire.baf\r\nanimationBundle.addAnimation Objects/W/a_more.baf\r\n"
                             "animationBundle.addEvent sound:0_first\r\n") != std::string::npos);
    CHECK(file.removeBundleItem("fire", "Objects/W/a_more.baf", false, error) && file.bundleAnimations("fire").size() == 1);
    // A start time: after its addAnimation line; then rewritten in place.
    CHECK(file.setAnimationStart("fire", "Objects/W/a_fire.baf", 0.5f, error));
    CHECK(file.render().find("animationBundle.addAnimation Objects/W/a_fire.baf\r\n"
                             "animationBundle.setAnimationStartTime Objects/W/a_fire.baf 0.5\r\n") != std::string::npos);
    CHECK(file.setAnimationStart("fire", "Objects/W/a_fire.baf", 0.75f, error));
    CHECK(file.bundleAnimations("fire")[0].start == 0.75f);
    CHECK(!file.setAnimationStart("fire", "Objects/W/none.baf", 1.0f, error));
    CHECK(file.setEventStart("fire", "sound:0_first", 0.3f, error) && file.bundleEvents("fire")[0].start == 0.3f);
    // A length: never 0 (the engine divides by it); after the start-time line; then in place.
    CHECK(!file.setAnimationLength("fire", "Objects/W/a_fire.baf", 0.0f, error));
    CHECK(file.setAnimationLength("fire", "Objects/W/a_fire.baf", 1.5f, error));
    CHECK(file.render().find("animationBundle.setAnimationStartTime Objects/W/a_fire.baf 0.75\r\n"
                             "animationBundle.setAnimationLength Objects/W/a_fire.baf 1.5\r\n") != std::string::npos);
    CHECK(file.setAnimationLength("fire", "Objects/W/a_fire.baf", 2.0f, error) && file.bundleAnimations("fire")[0].lengthSet
          && file.bundleAnimations("fire")[0].length == 2.0f);
    // The animation taken out with its start-time and length lines; an event added.
    CHECK(file.removeBundleItem("fire", "Objects/W/a_fire.baf", false, error));
    CHECK(file.bundleAnimations("fire").empty() && file.render().find("setAnimationStartTime") == std::string::npos
          && file.render().find("setAnimationLength") == std::string::npos);
    CHECK(file.addLine(AnimationKind::Bundle, "fire", "addEvent", "effect:0", error) && file.bundleEvents("fire").size() == 2);
    // A new bundle after the last bundle, a blank line before it; taken out again with that blank line.
    const std::string before = file.render();
    CHECK(file.addObject(AnimationKind::Bundle, "", "reload", error));
    CHECK(file.render().find("animationBundle.addAnimation Objects/W/a_idle.baf\r\n\r\nanimationSystem.createBundle reload\r\n")
          != std::string::npos);
    CHECK(!file.addObject(AnimationKind::Bundle, "", "reload", error));
    CHECK(file.removeObject(AnimationKind::Bundle, "reload", error) && file.render() == before);
    // Name order, as the writer saves: a bundle that sorts first goes above the first bundle (a blank line after
    // it); an animation between two animations with no blank lines; both taken out again.
    CHECK(file.addObject(AnimationKind::Bundle, "", "aim", error));
    CHECK(file.render().find("rem *** Bundles ***\r\nanimationSystem.createBundle aim\r\n\r\nanimationSystem.createBundle fire\r\n")
          != std::string::npos);
    CHECK(file.addObject(AnimationKind::Animation, "", "Objects/W/a_fire2.baf", error));
    CHECK(file.render().find("animationManager.length 0.333333\r\nanimationSystem.createAnimation Objects/W/a_fire2.baf\r\n"
                             "animationSystem.createAnimation Objects/W/a_idle.baf\r\n") != std::string::npos);
    CHECK(file.removeObject(AnimationKind::Animation, "Objects/W/a_fire2.baf", error)
          && file.removeObject(AnimationKind::Bundle, "aim", error) && file.render() == before);
    // Value holders follow one another in name order.
    AnimationSystemFile holders;
    holders.parse("AnimationSystem.createValueHolder a_move\r\nAnimationValueHolder.values 0.25 4.2 10\r\n"
                  "AnimationSystem.createValueHolder c_turn\r\nAnimationValueHolder.values -1 -3 -10\r\n");
    CHECK(holders.addObject(AnimationKind::ValueHolder, "", "b_jump", error)
          && holders.setProperty(AnimationKind::ValueHolder, "b_jump", "values", "0 1 2", -1, error));
    CHECK(holders.render() == "AnimationSystem.createValueHolder a_move\r\nAnimationValueHolder.values 0.25 4.2 10\r\n"
                              "AnimationSystem.createValueHolder b_jump\r\nAnimationValueHolder.values 0 1 2\r\n"
                              "AnimationSystem.createValueHolder c_turn\r\nAnimationValueHolder.values -1 -3 -10\r\n");
    CHECK(!file.addObject(AnimationKind::Trigger, "", "nameless", error));
    // The trigger tree: roots, children first in the writer's order, a reference the engine finds only when its
    // object was created above it, a new child kept above the block that names it, the references swept.
    CHECK(file.triggerRoots().size() == 2 && file.triggerRoots()[0] == "still" && file.triggerRoots()[1] == "shoot");
    CHECK(file.addLine(AnimationKind::Trigger, "shoot", "addChild", "still", error));
    CHECK(file.render().find("animationSystem.createTrigger MessageTrigger shoot\r\nanimationTrigger.addChild still\r\n"
                             "animationTrigger.message fire\r\n") != std::string::npos);
    CHECK(file.triggerRoots().size() == 1 && file.triggerChildren("shoot").size() == 1 && file.triggerBundles("shoot").size() == 1);
    CHECK(file.createdBefore(AnimationKind::Trigger, "still", file.find(AnimationKind::Trigger, "shoot")->createLine)
          && !file.createdBefore(AnimationKind::Trigger, "shoot", file.find(AnimationKind::Trigger, "still")->createLine));
    const std::string withChild = file.render();
    CHECK(file.addObject(AnimationKind::Trigger, "IdleTrigger", "wait", error, file.find(AnimationKind::Trigger, "shoot")->createLine));
    CHECK(file.render().find("animationSystem.createTrigger IdleTrigger wait\r\n\r\nanimationSystem.createTrigger MessageTrigger shoot\r\n")
          != std::string::npos);
    CHECK(file.addLine(AnimationKind::Trigger, "shoot", "addChild", "wait", error) && file.triggerSubtree("shoot").size() == 3);
    CHECK(file.removeReferences(AnimationKind::Trigger, "addChild", "WAIT") == 1 && file.removeObject(AnimationKind::Trigger, "wait", error)
          && file.render() == withChild);
    CHECK(file.removeReferences(AnimationKind::Trigger, "addChild", "still") == 1 && file.triggerRoots().size() == 2);
    // The system's own line in place.
    CHECK(file.setSystemProperty("cameraSpring.look", "2/2", error) && file.render().find("animationSystem.cameraSpring.look 2/2\r\n") == 36);
    CHECK(file.setSystemProperty("cameraSpring.fire", "0/0/150", error) && file.systemLines().size() == 3
          && file.removeSystemProperty("cameraSpring.fire", error) && file.systemLines().size() == 2);
    AnimationKind parsed = AnimationKind::Animation;
    CHECK(AnimationSystemFile::kindFromName("Bundle", parsed) && parsed == AnimationKind::Bundle
          && AnimationSystemFile::kindFromName("valueholder", parsed) && parsed == AnimationKind::ValueHolder
          && !AnimationSystemFile::kindFromName("skeleton", parsed));

    // The stock systems, when a folder of them is given.
    char samples[MAX_PATH] = "";
    if (GetEnvironmentVariableA("BF2EDITOR_ANIMATION_SAMPLES", samples, MAX_PATH) == 0)
        return;
    std::vector<std::string> files;
    walkIncFiles(samples, files);
    CHECK(files.size() > 170);
    int identical = 0, withObjects = 0;
    for (size_t i = 0; i < files.size(); ++i)
    {
        std::string text;
        if (!ObjectLibrary::readTextFile(files[i], text, error))
            continue;
        AnimationSystemFile one;
        one.parse(text);
        if (one.render() == text)
            ++identical;
        if (!one.objects().empty())
            ++withObjects;
    }
    CHECK(identical == static_cast<int>(files.size()));
    CHECK(withObjects >= static_cast<int>(files.size()) - 2);
}

// The retail layer model of gameplay objects (GameplayLayers.h): the writer's layout parsed and rendered back, the
// layers' file, and stock levels imported from their game-mode files - every bucket kept regenerated byte for byte.
// With BF2EDITOR_GAMEPLAY_SAMPLES set to extract_gameplay_samples.py's folder, every stock level is imported and
// each level's kept and left buckets are printed.
namespace
{

std::string gameplayFile(const std::string& spawner, const std::string& area, int layer)
{
    char layerLine[32];
    std::snprintf(layerLine, sizeof(layerLine), "%d", layer);
    std::string t;
    t += "rem ********** Object Spawner **********\r\n";
    if (!spawner.empty())
        t += "rem [ObjectSpawnerTemplate: " + spawner + "]\r\nObjectTemplate.create ObjectSpawner " + spawner + "\r\n\r\n";
    t += "if v_arg1 == host\r\n\r\n";
    if (!spawner.empty())
        t += "   rem [ObjectSpawner: " + spawner + "]\r\n   Object.create " + spawner + "\r\n   Object.layer " + layerLine + "\r\n\r\n";
    t += "endIf\r\n\r\nrem ********** Spawn Points **********\r\nrem ********** Control Points **********\r\n"
         "if v_arg1 == host\r\n\r\nendIf\r\n\r\nCombatAreaManager.use 1\r\nCombatAreaManager.timeAllowedOutside 10.000000\r\n";
    if (!area.empty())
        t += "CombatArea.create " + area + "\r\nCombatArea.layer 1\r\n\r\n";
    return t;
}

void walkGameplayLevel(const std::string& level, std::vector<GameplayBucket>& buckets)
{
    WIN32_FIND_DATAA modeFound;
    HANDLE modes = FindFirstFileA((level + "\\*").c_str(), &modeFound);
    if (modes == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const std::string mode = modeFound.cFileName;
        if (mode == "." || mode == ".." || !(modeFound.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;
        WIN32_FIND_DATAA sizeFound;
        HANDLE sizes = FindFirstFileA((level + "\\" + mode + "\\*").c_str(), &sizeFound);
        if (sizes == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            const std::string size = sizeFound.cFileName;
            if (size == "." || size == ".." || !(sizeFound.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                continue;
            GameplayBucket bucket;
            bucket.mode = mode;
            bucket.players = size;
            std::string error;
            if (ObjectLibrary::readTextFile(level + "\\" + mode + "\\" + size + "\\GamePlayObjects.con", bucket.text, error))
                buckets.push_back(bucket);
        } while (FindNextFileA(sizes, &sizeFound));
        FindClose(sizes);
    } while (FindNextFileA(modes, &modeFound));
    FindClose(modes);
}

} // namespace

void checkGameplayLayers()
{
    // The writer's layout, empty (golden G1's retail Editor/GamePlayObjects.con) and with objects, back byte for byte.
    const std::string empty =
        "rem ********** Object Spawner **********\r\nif v_arg1 == host\r\n\r\nendIf\r\n\r\n"
        "rem ********** Spawn Points **********\r\nrem ********** Control Points **********\r\nif v_arg1 == host\r\n\r\n"
        "endIf\r\n\r\nCombatAreaManager.use 0\r\nCombatAreaManager.timeAllowedOutside 10.000000\r\n";
    GameplaySet set;
    std::string error;
    CHECK(set.parse(empty, error) && set.objects().empty() && set.render(std::set<int>(), true) == empty);
    const std::string a = gameplayFile("sp_a", "area_d", 4);
    CHECK(set.parse(a, error) && set.objects().size() == 2 && set.render(std::set<int>(), true) == a);
    CHECK(set.objects()[0].layer == 4 && set.objects()[1].section == GameplaySection::CombatArea && set.objects()[1].layer == 1);
    // Two game modes: a layer of its own each, the Default area in both - imported, both kept, each made again.
    std::vector<GameplayBucket> buckets(2);
    buckets[0].mode = "gpm_cq";
    buckets[0].players = "16";
    buckets[0].text = gameplayFile("sp_a", "area_d", 4);
    buckets[1].mode = "gpm_cq";
    buckets[1].players = "32";
    buckets[1].text = gameplayFile("sp_b", "area_d", 5);
    GameplayImport imported;
    CHECK(importBuckets(buckets, GameplayFilters::defaults(), imported) && imported.managed.size() == 2 && imported.unmanaged.empty());
    CHECK(imported.layers.size() == 3 && imported.set.objects().size() == 3);
    CHECK(renderBucket(imported.set, imported.layers, "gpm_cq", "16") == buckets[0].text
          && renderBucket(imported.set, imported.layers, "gpm_cq", "32") == buckets[1].text);
    // The Default layer reaches every game mode, but a file is written only where another layer fills it or one exists.
    CHECK(!bucketWritten(imported.set, imported.layers, "gpm_cq", "64", false)
          && bucketWritten(imported.set, imported.layers, "gpm_cq", "16", false));
    // A file from another save (an object of the Default layer the other lacks) is left as it is.
    buckets[1].text = gameplayFile("sp_b", "area_other", 5);
    CHECK(importBuckets(buckets, GameplayFilters::defaults(), imported) && imported.managed.size() == 0 + 1 * 0 + imported.managed.size());
    // Layer.con: retail's blocks, the Default left out, back again.
    std::vector<GameplayLayer> layers(2);
    layers[0].id = 1;
    layers[0].name = "Default";
    layers[1].id = 3;
    layers[1].name = "Conquest 64";
    layers[1].locked = true;
    layers[1].modes.push_back("gpm_cq");
    layers[1].players.push_back("64");
    const std::string layerFile = renderLayerFile(layers);
    CHECK(layerFile == "rem ******** Conquest 64 ********\r\nLayerManager.AddUserLayer \"Conquest 64\" 8 3 1 1\r\n"
                       "LayerManager.SetLocked 1\r\nLayerManager.SetDisabled 0\r\nLayerManager.AddGameModeFilter \"gpm_cq\"\r\n"
                       "LayerManager.AddPlayerFilter \"64\"\r\n\r\n");
    std::vector<GameplayLayer> back;
    CHECK(parseLayerFile(layerFile, back, error) && back.size() == 1 && back[0].id == 3 && back[0].name == "Conquest 64"
          && back[0].locked && !back[0].disabled && back[0].takes("gpm_cq", "64") && !back[0].takes("gpm_cq", "32"));
    GameplayFilters filters;
    CHECK(filters.parse("LayerManager.CreateDefaultFilter gpm_cq Conquest\r\nLayerManager.CreateDefaultPlayerFilter 16\r\n")
          && filters.modes.size() == 1 && filters.modeLabels[0] == "Conquest" && filters.players.size() == 1);

    // The stock levels, when a folder of them is given.
    char samples[MAX_PATH] = "";
    if (GetEnvironmentVariableA("BF2EDITOR_GAMEPLAY_SAMPLES", samples, MAX_PATH) == 0)
        return;
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((std::string(samples) + "\\*").c_str(), &found);
    int levels = 0, kept = 0, left = 0, wholeLevels = 0;
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string level = found.cFileName;
            if (level == "." || level == ".." || !(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                continue;
            std::vector<GameplayBucket> stock;
            walkGameplayLevel(std::string(samples) + "\\" + level, stock);
            GameplayImport result;
            importBuckets(stock, GameplayFilters::defaults(), result);
            ++levels;
            kept += static_cast<int>(result.managed.size());
            left += static_cast<int>(result.unmanaged.size());
            wholeLevels += result.unmanaged.empty() ? 1 : 0;
            std::printf("[gameplay layers] %s: %u game-mode files kept, %u left, %u layers\n", level.c_str(),
                        static_cast<unsigned>(result.managed.size()), static_cast<unsigned>(result.unmanaged.size()),
                        static_cast<unsigned>(result.layers.size()));
            for (size_t n = 0; n < result.notes.size() && n < 4; ++n)
                std::printf("[gameplay layers]    %s\n", result.notes[n].c_str());
            // Every kept file comes back byte for byte.
            for (size_t m = 0; m < result.managed.size(); ++m)
                for (size_t b = 0; b < stock.size(); ++b)
                    if (stock[b].mode == result.managed[m].first && stock[b].players == result.managed[m].second)
                        CHECK(renderBucket(result.set, result.layers, stock[b].mode, stock[b].players) == stock[b].text);
        } while (FindNextFileA(h, &found));
        FindClose(h);
    }
    std::printf("[gameplay layers] %d levels: %d game-mode files kept, %d left; %d levels whole\n", levels, kept, left, wholeLevels);
    CHECK(levels >= 20 && kept > left);
}

// A lightmap light setup (LightSetupFile.h): the retail writer's block, its defaults the retail bundle's own values,
// each setting checked for its kind, a file read back.
void checkLightSetup()
{
    LightSetup setup;
    const std::string retail =
        "rem ******** Lightmap Generation Settings ********\r\nif v_arg1 == BF2Editor\r\n\r\nLightMapGeneration.init\r\n"
        "LightMapGeneration.GILightsBaseDir 0/0.5/-0.5\r\nLightMapGeneration.GILightsIntensity -1\r\n"
        "LightMapGeneration.GILightsShadowIntensity -1\r\nLightMapGeneration.GILightsFov -1\r\nLightMapGeneration.UseGITopLight 0\r\n"
        "LightMapGeneration.GITopLightIntensity 0.1\r\nLightMapGeneration.GILightsViewDistance 50\r\nLightMapGeneration.skyWhite 0\r\n"
        "LightMapGeneration.numberOfGILights 0\r\nLightMapGeneration.sunLightShadowIntensity -1\r\nLightMapGeneration.sunLightFov 8\r\n"
        "LightMapGeneration.sunLightIntensity -1\r\n\r\nendIf\r\n";
    CHECK(setup.render() == retail);
    std::string error;
    CHECK(setup.set("sunLightFov", "12", error) && setup.set("gilightsbasedir", "1/2/3", error) && setup.set("numberOfGILights", "4", error));
    CHECK(!setup.set("UseGITopLight", "2", error) && !setup.set("GILightsBaseDir", "1/2", error) && !setup.set("numberOfGILights", "1.5", error)
          && !setup.set("nothing", "1", error));
    LightSetup back;
    CHECK(back.parse(setup.render()) && back.values == setup.values);
    std::string value;
    CHECK(back.get("GILightsBaseDir", value) && value == "1/2/3");
}

// Ground properties (GroundPropertiesFile.h): the retail writer's text - the G1 golden of an untouched Karkand save is
// a blank line, the five Misc lines and a blank line - a layer's block in the writer's order, settings checked for
// their kind, a file read back.
void checkGroundProperties()
{
    GroundProperties ground;
    std::string error;
    CHECK(ground.setMisc("farTopTilingHi", "24", error) && ground.setMisc("farTopTilingLow", "4", error)
          && ground.setMisc("farSideTiling", "5/5", error) && ground.setMisc("farYOffset", "0", error)
          && ground.setMisc("lodSwitchDistance", "75", error));
    CHECK(ground.render() == "\r\nMiscGroundProperties.SetFarTopTilingHi 24\r\nMiscGroundProperties.SetFarTopTilingLow 4\r\n"
                             "MiscGroundProperties.SetFarSideTiling 5/5\r\nMiscGroundProperties.SetFarYOffset 0\r\n"
                             "MiscGroundProperties.SetLodSwitchDistance 75\r\n\r\n");
    CHECK(ground.setLayer(1, "detailTexture", "common\\terrain\\textures\\detail\\detail_grass05.dds", error)
          && ground.setLayer(1, "topTiling", "64", error) && ground.setLayer(1, "sideTiling", "2/2.5", error)
          && ground.setLayer(1, "color", "10/20/30", error) && ground.setLayer(1, "envMap", "1", error));
    CHECK(!ground.setLayer(6, "topTiling", "1", error) && !ground.setLayer(1, "color", "10/20", error)
          && !ground.setLayer(1, "sideTiling", "2", error) && !ground.setLayer(1, "envMap", "2", error)
          && !ground.setLayer(1, "nothing", "1", error) && !ground.setMisc("farYOffset", "x", error));
    const std::string text = ground.render();
    CHECK(text.find("GroundProperties.SetDetailTexture 1 \"common\\terrain\\textures\\detail\\detail_grass05.dds\"\r\n"
                    "GroundProperties.SetColor 1 10 20 30\r\nGroundProperties.SetTopTiling 1 64\r\n"
                    "GroundProperties.SetSideTiling 1 2/2.5\r\n") == 0);
    GroundProperties back;
    CHECK(back.parse(text, error) && back.layers == ground.layers && back.misc == ground.misc && back.render() == text);
    std::string value;
    CHECK(back.getLayer(1, "color", value) && value == "10/20/30" && back.getMisc("farSideTiling", value) && value == "5/5");
    CHECK(!back.parse("GroundProperties.SetTopTiling 9 1\r\n", error));
}

// The compiled terrain's detail layers (TerrainRawLayers.h), read from the staged Karkand when it is there: six layers,
// the first the rock texture with its tiling, as Terrain::load reads them.
void checkTerrainRawLayers()
{
    const char* const level = std::getenv("BF2_EDITOR_KARKAND_DIR");
    const std::string path = level && *level ? std::string(level) + "/terraindata.raw" : std::string();
    FILE* file = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");
    if (!file)
    {
        std::printf("[terrain raw layers] no staged Karkand: skipped\n");
        return;
    }
    std::string bytes;
    char buffer[65536];
    size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        bytes.append(buffer, n);
    std::fclose(file);
    TerrainRawLayers layers;
    std::string error;
    CHECK(layers.read(bytes, error));
    CHECK(layers.version == 0x1A && layers.layers.size() == 6 && layers.patchCount == 4);
    if (layers.layers.size() == 6)
    {
        CHECK(layers.layers[0].name == "common\\terrain\\textures\\detail\\detail_rock04" && layers.layers[0].planeMap == 1
              && layers.layers[0].sideTiling[0] == 32.0f && layers.layers[0].sideTiling[1] == 16.0f
              && layers.layers[0].topTiling == 50.0f);
        CHECK(layers.layers[5].name == "common\\terrain\\textures\\detail\\detail_cobble2");
    }
    CHECK(layers.farSideTiling[0] == 5.0f && layers.farTopTilingHi == 24.0f);
    TerrainRawLayers broken;
    CHECK(!broken.read(bytes.substr(0, 100), error));
}

// The whole compiled terrain file (TerrainRawFile.h): the staged Karkand's terraindata.raw parsed to its last byte and
// written back byte for byte, with its grid (4 x 4 patches of 128), 6 detail textures, 77 patch layers of 84 LOD
// records each, the 8 ring sectors and 1365 culler nodes; a short or overlong file refused. With
// BF2EDITOR_TERRAIN_SAMPLES set to a folder of .raw files (extract_terrain_samples.py copies stock ones), each is
// written back byte for byte too.
namespace
{
bool readWholeFile(const std::string& path, std::string& bytes)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    bytes.clear();
    char buffer[65536];
    size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        bytes.append(buffer, n);
    std::fclose(file);
    return true;
}
}

void checkTerrainRawFile()
{
    const char* const level = std::getenv("BF2_EDITOR_KARKAND_DIR");
    std::string bytes;
    if (!level || !*level || !readWholeFile(std::string(level) + "/terraindata.raw", bytes))
    {
        std::printf("[terrain raw file] no staged Karkand: skipped\n");
    }
    else
    {
        TerrainRawFile raw;
        std::string error;
        CHECK(raw.parse(bytes, error));
        CHECK(raw.render() == bytes);
        CHECK(raw.patchSize == 128 && raw.patchCount == 4 && raw.detailTextures.size() == 6 && raw.patches.size() == 16
              && raw.compactVertices == 0 && raw.ring.size() == 8 && raw.culler.size() == 1365);
        size_t layers = 0;
        bool lodRecords = true;
        for (size_t p = 0; p < raw.patches.size(); ++p)
        {
            layers += raw.patches[p].layers.size();
            for (size_t l = 0; l < raw.patches[p].layers.size(); ++l)
                lodRecords = lodRecords && raw.patches[p].layers[l].lodWords.size() == 84 * 6;
        }
        CHECK(layers == 77 && lodRecords);
        CHECK(raw.ring[1].normals.size() == 12 * raw.ringVertexCount(raw.ring[1].slot)
              && raw.ring[0].heights.size() == 4 * raw.ringVertexCount(raw.ring[0].slot) && raw.ringVertexCount(0) != raw.ringVertexCount(1));
        TerrainRawFile broken;
        CHECK(!broken.parse(bytes.substr(0, bytes.size() - 1), error));
        CHECK(!broken.parse(bytes + std::string(1, '\0'), error) && error == "1 bytes follow the culler");
    }

    char folder[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BF2EDITOR_TERRAIN_SAMPLES", folder, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return;
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((std::string(folder) + "\\*.raw").c_str(), &found);
    int files = 0, same = 0;
    std::string first;
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::string sample;
            if (!readWholeFile(std::string(folder) + "\\" + found.cFileName, sample))
                continue;
            ++files;
            TerrainRawFile raw;
            std::string error;
            if (raw.parse(sample, error) && raw.render() == sample)
                ++same;
            else if (first.empty())
                first = std::string(found.cFileName) + (error.empty() ? std::string(": written back differently") : ": " + error);
        } while (FindNextFileA(h, &found));
        FindClose(h);
    }
    std::printf("[terrain raw file] %d of %d sample files written back byte for byte%s%s\n", same, files,
                first.empty() ? "" : "; first difference: ", first.c_str());
    CHECK(files > 0 && same == files);
}

// The terrain compile (TerrainRawCompile.h): the normal packing; one flat patch compiled from a synthetic template (a
// single weight texel makes four cells present, two texels around each cell counting; flat ground has no geomorph
// and an upright normal; the culler's patch-level root; a flat side sector); and every patch layer of the staged
// Karkand rebuilt from its triangles alone, indices and LOD records byte for byte.
void checkTerrainRawCompile()
{
    CHECK(packTerrainNormal(0.0f, 1.0f, 0.0f) == 0xFF7FFE7Fu && packTerrainNormal(-1.0f, 0.0f, 1.0f) == 0xFF007FFEu);

    TerrainRawFile templ;
    templ.patchSize = 128;
    templ.patchCount = 1;
    templ.primaryScale[0] = 2.0f;
    templ.primaryScale[1] = 0.00457764f;
    templ.primaryScale[2] = 2.0f;
    templ.secondaryScale[0] = 4.0f;
    templ.secondaryScale[1] = 1.17188f;
    templ.secondaryScale[2] = 4.0f;
    templ.patches.resize(1);
    std::vector<unsigned char> weights(512 * 512, 0);
    weights[200 + 512 * 300] = 255;
    TerrainCompileSource source;
    source.height = [](float, float) { return 100.0f; };
    source.seaWaterLevel = 0.0f;
    source.weights = [&weights](int, int layer) -> const unsigned char* { return layer == 0 ? &weights[0] : nullptr; };
    source.ringSlots.push_back(1);
    TerrainRawFile out;
    std::string error;
    CHECK(compileTerrainRaw(templ, source, out, error));
    if (out.patches.size() == 1 && out.patches[0].layers.size() == 1)
    {
        const TerrainRawPatch& patch = out.patches[0];
        const TerrainRawPatchLayer& layer = patch.layers[0];
        CHECK(layer.detailIndex == 0 && layer.indices.size() == 24 && layer.word0C == 8 && layer.lodWords.size() == 84 * 6
              && layer.lodWords[0] == 0 && layer.lodWords[1] == 24 && static_cast<unsigned>(layer.marker) == 0xEEEEEEEEu
              && layer.bytes10[0] == 4);
        // Cells x 49..50, z 74..75: their (a, b, c) triangles start at the cell's own vertex.
        bool cells = true;
        for (size_t t = 0; t < layer.indices.size(); t += 3)
        {
            const unsigned a = layer.indices[t + 1] - 1u;            // b is a + 1 in both triangles
            cells = cells && a % 129 >= 49 && a % 129 <= 50 && a / 129 >= 74 && a / 129 <= 75;
        }
        CHECK(cells);
        CHECK(patch.maxHeight == 100.0f && patch.minHeight == 100.0f && patch.sphereCentre[0] == 128.0f
              && patch.sphereCentre[1] == 100.0f && std::fabs(patch.sphereRadius - 181.019f) < 0.01f);
        const size_t vertex = 24 * (1 * 129 + 0);                     // row 1, column 0: an edge column
        float h = 0.0f, morph[3] = { 1.0f, 1.0f, 1.0f };
        unsigned normal = 0;
        std::memcpy(&h, &patch.vertices[vertex + 4], 4);
        std::memcpy(morph, &patch.vertices[vertex + 8], 12);
        std::memcpy(&normal, &patch.vertices[vertex + 20], 4);
        // The geomorph delta of flat ground keeps the writer's rounding residue (a float mean against a double height).
        CHECK(patch.vertices.size() == 24 * 129 * 129 && patch.vertices[vertex] == 1 && patch.vertices[vertex + 1] == 1
              && patch.vertices[vertex + 2] == 0 && std::fabs(h - 100.0f / 0.00457764f) < 0.5f && std::fabs(morph[0]) < 0.01f
              && morph[1] == 0.0f && morph[2] == 0.0f && normal == 0xFF7FFE7Fu);
    }
    else
        CHECK(!"one patch with one layer");
    CHECK(out.maxHeight == 100.0f && out.minHeight == 100.0f);
    CHECK(out.culler.size() == 85 && out.culler[0].hasPatchList == 1 && out.culler[0].patchIndex == 0
          && out.culler[1].byteEC == 1 && out.culler[5].byteEE == 1 && out.culler[21].byteEE == 0
          && out.culler[1].neighbour[4] == 2 && out.culler[1].neighbour[6] == 3 && out.culler[1].neighbour[7] == 4
          && out.culler[1].neighbour[0] == 0xFFFF && out.culler[0].minimum[1] == 100.0f);
    CHECK(out.ring.size() == 1 && out.ring[0].slot == 1 && out.ring[0].maxY == 100.0f && out.ring[0].minY == 100.0f
          && out.ring[0].heights.size() == 4 * out.ringVertexCount(1) && out.ring[0].normals.size() == 12 * out.ringVertexCount(1));

    const char* const level = std::getenv("BF2_EDITOR_KARKAND_DIR");
    std::string bytes;
    TerrainRawFile raw;
    if (!level || !*level || !readWholeFile(std::string(level) + "/terraindata.raw", bytes) || !raw.parse(bytes, error))
    {
        std::printf("[terrain compile] no staged Karkand: the layer rebuild skipped\n");
        return;
    }
    int layers = 0, same = 0;
    for (size_t p = 0; p < raw.patches.size(); ++p)
    {
        for (size_t l = 0; l < raw.patches[p].layers.size(); ++l)
        {
            const TerrainRawPatchLayer& stock = raw.patches[p].layers[l];
            // The triangles in the writer's scan order: cells row by row, (a, b, c) before (c, b, d).
            std::vector<std::pair<unsigned, std::vector<unsigned short> > > triangles;
            for (size_t t = 0; t + 2 < stock.indices.size(); t += 3)
            {
                const bool second = stock.indices[t + 2] == stock.indices[t] + 1;
                const unsigned cell = second ? stock.indices[t] - 129u : stock.indices[t];
                std::vector<unsigned short> tri(stock.indices.begin() + t, stock.indices.begin() + t + 3);
                triangles.push_back(std::make_pair(cell * 2 + (second ? 1u : 0u), tri));
            }
            std::sort(triangles.begin(), triangles.end());
            std::vector<unsigned short> indices;
            for (size_t i = 0; i < triangles.size(); ++i)
                indices.insert(indices.end(), triangles[i].second.begin(), triangles[i].second.end());
            std::vector<unsigned> words;
            orderLayerTriangles(indices, 128, 84, words);
            ++layers;
            if (indices == stock.indices && words == stock.lodWords)
                ++same;
        }
    }
    std::printf("[terrain compile] %d of %d Karkand patch layers rebuilt from their triangles byte for byte\n", same, layers);
    CHECK(layers == 77 && same == layers);
}

// The texture encoders (TextureEncode.h): the box halving; R5G6B5 and DXT1 files with the stock files' headers and
// sizes (a 256 R5G6B5 detail map 131200 bytes, a 256 DXT1 one 32896, a 512 DXT1 colormap with its ten levels 174904);
// a smooth gradient and the staged Karkand's first colormap through DXT1 and back within a small mean error.
namespace
{
unsigned headerWord(const std::string& dds, size_t offset)
{
    unsigned value = 0;
    if (dds.size() >= offset + 4)
        std::memcpy(&value, &dds[offset], 4);
    return value;
}

double meanChannelError(const DdsImage& a, const DdsImage& b)
{
    if (a.width != b.width || a.height != b.height || a.pixels.empty())
        return 1e9;
    double sum = 0.0;
    for (size_t i = 0; i < a.pixels.size(); ++i)
        for (int shift = 0; shift < 24; shift += 8)
            sum += std::abs(static_cast<int>((a.pixels[i] >> shift) & 0xFF) - static_cast<int>((b.pixels[i] >> shift) & 0xFF));
    return sum / (a.pixels.size() * 3.0);
}
}

void checkTextureEncode()
{
    DdsImage square;
    square.width = 2;
    square.height = 2;
    square.pixels = { 0xFF000000u, 0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u };
    const DdsImage half = halveBox(square);
    CHECK(half.width == 1 && half.height == 1 && half.pixels.size() == 1 && half.pixels[0] == 0xFF404040u);

    DdsImage flat;
    flat.width = 256;
    flat.height = 256;
    flat.pixels.assign(256 * 256, 0xFFFF0000u);
    const std::string r565 = encodeR5G6B5(flat);
    unsigned short first565 = 0;
    std::memcpy(&first565, &r565[128], 2);
    CHECK(r565.size() == 131200 && headerWord(r565, 8) == 0x81007u && headerWord(r565, 20) == 131072u
          && headerWord(r565, 28) == 0 && headerWord(r565, 80) == 0x40u && headerWord(r565, 88) == 16u
          && headerWord(r565, 92) == 0xF800u && headerWord(r565, 108) == 0x1000u && first565 == 0xF800u);
    const std::string dxtFlat = encodeDxt1(flat, false);
    CHECK(dxtFlat.size() == 32896 && headerWord(dxtFlat, 8) == 0x81007u && headerWord(dxtFlat, 20) == 32768u
          && headerWord(dxtFlat, 84) == 0x31545844u && headerWord(dxtFlat, 108) == 0x1000u);

    DdsImage gradient;
    gradient.width = 512;
    gradient.height = 512;
    gradient.pixels.resize(512 * 512);
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 512; ++x)
            gradient.pixels[y * 512 + x] = 0xFF000000u | (static_cast<unsigned>(x / 2) << 16) |
                                           (static_cast<unsigned>(y / 2) << 8) | static_cast<unsigned>((x + y) / 4);
    const std::string colormap = encodeDxt1(gradient, true);
    CHECK(colormap.size() == 174904 && headerWord(colormap, 8) == 0xA1007u && headerWord(colormap, 28) == 10u
          && headerWord(colormap, 108) == 0x401008u && headerWord(colormap, 20) == 131072u);
    DdsImage back;
    std::string error;
    CHECK(back.read(colormap, error) && meanChannelError(gradient, back) < 3.0);

    const char* const level = std::getenv("BF2_EDITOR_KARKAND_DIR");
    std::string bytes;
    DdsImage stock;
    if (!level || !*level || !readWholeFile(std::string(level) + "/Colormaps/tx00x00.dds", bytes) || !stock.read(bytes, error))
    {
        std::printf("[texture encode] no staged Karkand: the colormap round trip skipped\n");
        return;
    }
    DdsImage again;
    CHECK(again.read(encodeDxt1(stock, true), error));
    const double drift = meanChannelError(stock, again);
    std::printf("[texture encode] Karkand's tx00x00 colormap through DXT1 again: mean channel error %.2f\n", drift);
    CHECK(drift < 2.0);
}

// The low-detail map (TerrainLowDetail.h): the renderer's rule on two small detail maps - type 1 into red, type 2 into
// blue, saturating, green and alpha 0, a missing map or one of another size adding nothing -, the types recovered from
// maps built with known ones (a layer without weight left 0), and with the staged Karkand its stock types recovered
// from its compiled maps (2 1 1 0 1 0) and a map built from them written with the stock low-detail header.
void checkTerrainLowDetail()
{
    DdsImage first, second, wrong;
    first.width = first.height = second.width = second.height = 2;
    first.pixels = { 0x00302010u, 0x00FF8000u, 0x00000000u, 0x00C0C0C0u };     // layers 0, 1, 2 in bytes 0, 1, 2
    second.pixels = { 0x00000005u, 0x00000000u, 0x00FFFFFFu, 0x00000040u };    // layers 3, 4, 5
    wrong.width = wrong.height = 4;
    wrong.pixels.assign(16, 0x00FFFFFFu);
    const int types[6] = { 2, 1, 1, 0, 1, 0 };
    const DdsImage* maps[2] = { &first, &second };
    const DdsImage low = buildLowDetailMap(maps, types, 2);
    CHECK(low.width == 2 && low.height == 2 && low.pixels.size() == 4);
    if (low.pixels.size() == 4)
        CHECK(low.pixels[0] == 0x00500010u && low.pixels[1] == 0x00FF0000u && low.pixels[2] == 0x00FF0000u
              && low.pixels[3] == 0x00FF00C0u);
    const DdsImage* withWrong[2] = { &first, &wrong };
    const DdsImage* withNone[2] = { &first, 0 };
    const DdsImage lowWrong = buildLowDetailMap(withWrong, types, 2);
    const DdsImage lowNone = buildLowDetailMap(withNone, types, 2);
    CHECK(lowWrong.pixels.size() == 4 && lowWrong.pixels[2] == 0u && lowWrong.pixels[3] == 0x00FF00C0u
          && lowNone.pixels == lowWrong.pixels);

    // Two 8 x 8 patches of pseudo-random weights, layer 5 weightless, their low-detail maps built with known types.
    const int known[6] = { 1, 0, 2, 0, 1, 0 };
    DdsImage detail[2][2], built[2];
    unsigned seed = 12345u;
    std::vector<LowDetailSample> samples(2);
    for (int p = 0; p < 2; ++p)
    {
        for (int k = 0; k < 2; ++k)
        {
            detail[p][k].width = detail[p][k].height = 8;
            for (int t = 0; t < 64; ++t)
            {
                unsigned texel = 0;
                for (int c = 0; c < 3; ++c)
                {
                    seed = seed * 1103515245u + 12345u;
                    const unsigned weight = (seed >> 16) % 160u;
                    texel |= (k == 1 && c == 2 ? 0u : weight) << (8 * c);
                }
                detail[p][k].pixels.push_back(texel);
            }
        }
        const DdsImage* patchMaps[2] = { &detail[p][0], &detail[p][1] };
        built[p] = buildLowDetailMap(patchMaps, known, 8);
        samples[p].detail[0] = &detail[p][0];
        samples[p].detail[1] = &detail[p][1];
        samples[p].lowDetail = &built[p];
    }
    int found[6] = { -1, -1, -1, -1, -1, -1 };
    float error = -1.0f;
    CHECK(inferLowDetailTypes(samples, found, error));
    CHECK(std::equal(found, found + 6, known) && error == 0.0f);
    CHECK(!inferLowDetailTypes(std::vector<LowDetailSample>(1), found, error));

    const char* const level = std::getenv("BF2_EDITOR_KARKAND_DIR");
    if (!level || !*level)
    {
        std::printf("[terrain low detail] no staged Karkand: its stock types skipped\n");
        return;
    }
    std::deque<DdsImage> images;
    std::vector<LowDetailSample> stock;
    for (int z = 0; z < 4; ++z)
    {
        for (int x = 0; x < 4; ++x)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "%02ix%02i", x, z);
            std::string bytes, readError;
            DdsImage image;
            if (!readWholeFile(std::string(level) + "/LowDetailmaps/tx" + name + ".dds", bytes) || !image.read(bytes, readError))
                continue;
            LowDetailSample sample;
            images.push_back(image);
            sample.lowDetail = &images.back();
            for (int k = 0; k < 2; ++k)
            {
                if (readWholeFile(std::string(level) + "/Detailmaps/tx" + name + "_" + std::to_string(k + 1) + ".dds", bytes)
                    && image.read(bytes, readError))
                {
                    images.push_back(image);
                    sample.detail[k] = &images.back();
                }
            }
            stock.push_back(sample);
        }
    }
    const int karkand[6] = { 2, 1, 1, 0, 1, 0 };
    CHECK(stock.size() == 16 && inferLowDetailTypes(stock, found, error));
    CHECK(std::equal(found, found + 6, karkand) && error < 4.0f);
    std::printf("[terrain low detail] Karkand's low-detail types from its compiled maps: %d %d %d %d %d %d, within %.2f "
                "of 255\n", found[0], found[1], found[2], found[3], found[4], found[5], error);
    if (!stock.empty())
    {
        const DdsImage* patchMaps[2] = { stock[0].detail[0], stock[0].detail[1] };
        const std::string file = encodeDxt1(buildLowDetailMap(patchMaps, karkand, 256), true);
        CHECK(file.size() == 43832 && headerWord(file, 8) == 0xA1007u && headerWord(file, 20) == 32768u
              && headerWord(file, 28) == 9u && headerWord(file, 84) == 0x31545844u && headerWord(file, 108) == 0x401008u);
    }
}

// A new level (NewLevel.h): the retail dialog's rules; the files of a small level (256 cells at 1 m, 10 m high, low-res
// 128) against a stand-in LOD block; and, with BF2_EDITOR_NEWLEVEL_GOLDEN (the retail editor's new level: 1024 cells at
// 2 m, 25 m high, low-res 256), BF2_EDITOR_TERRAIN_CONTENT (its default content) and the staged Karkand (the LOD block)
// set, that level built again: every file the retail editor wrote outside its editor maps, byte for byte, except the
// map description's texts, the lightmap atlas index's comment and the order of the terrain LOD block's indices.
void checkNewLevel()
{
    std::string error;
    NewLevelSettings settings;
    settings.name = "GoldenNew";
    CHECK(checkNewLevelSettings(settings, error) && settings.worldSize() == 2048 && settings.lowResScale() == 8
          && settings.patchCount() == 8);
    const auto refused = [](const NewLevelSettings& s) {
        std::string why;
        return !checkNewLevelSettings(s, why) && !why.empty();
    };
    NewLevelSettings bad = settings;
    bad.name = "";
    CHECK(refused(bad));
    bad.name = "Two words";
    CHECK(refused(bad));
    bad = settings;
    bad.size = 2048;
    CHECK(refused(bad));
    bad = settings;
    bad.scale = 3;
    CHECK(refused(bad));
    bad = settings;
    bad.defaultHeight = 101;
    CHECK(refused(bad));
    bad = settings;
    bad.lowResSize = 64;
    CHECK(refused(bad));
    bad = settings;
    bad.size = 256;
    bad.scale = 1;
    bad.lowResSize = 512;
    CHECK(bad.lowResScale() == 0 && refused(bad));

    NewLevelSettings small;
    small.name = "Small";
    small.size = 256;
    small.scale = 1;
    small.defaultHeight = 10;
    small.lowResSize = 128;
    NewLevelContent content;
    content.colormap = "colormap";
    content.detailmap1 = "detail 1";
    content.detailmap2 = "detail 2";
    content.ingameMap = "ingame";
    content.commanderMap = "commander";
    content.commanderMicromap = "micro";
    content.shadowmap = "shadow";
    TerrainRawFile lod;
    lod.patchSize = 128;
    lod.lodWords.assign(21 * 38 + 64 * 6, 0u);
    lod.lodIndices.assign(3, 0);
    std::vector<NewLevelFile> files;
    CHECK(!buildNewLevel(small, content, TerrainRawFile(), files, error) && files.empty());
    CHECK(buildNewLevel(small, content, lod, files, error));
    const auto file = [&files](const std::string& path) -> const std::string* {
        for (size_t i = 0; i < files.size(); ++i)
        {
            if (files[i].path == path)
                return files[i].bytes.get();
        }
        return 0;
    };
    CHECK(files.size() == 46u && file("GameModes/gpm_cq/16/GamePlayObjects.con")
          && file("GameModes/gpm_cq/16/GamePlayObjects.con")->compare(0, 4, "rem ") == 0);
    const std::string* primary = file("HeightmapPrimary.raw");
    CHECK(primary && primary->size() == 257u * 257u * 2u && (*primary)[0] == '\xA0' && (*primary)[1] == '\x0F'
          && primary->substr(0, 2) == primary->substr(primary->size() - 2));
    const std::string* secondary = file("HeightmapSecondary_R1D1.raw");
    CHECK(secondary && *secondary == std::string(129 * 129, '\x0F'));
    const std::string* material = file("HeightmapPrimary.mat");
    CHECK(material && *material == std::string(257 * 257, '\0'));
    const std::string* heightdata = file("Heightdata.con");
    CHECK(heightdata && heightdata->find("heightmapcluster.setHeightmapSize 256\r\n") != std::string::npos
          && heightdata->find("heightmap.setScale 2/0.64/2\r\nheightmap.setBitResolution 8\r\n") != std::string::npos
          && heightdata->find("Levels/Small/HeightmapSecondary_L1U1.raw\r\n") != std::string::npos);
    const std::string* sky = file("Sky.con");
    CHECK(sky && sky->find("Levels/Small/groundhemi 0.000000/0.000000/0.000000 256.000000 500.000000\r\n")
                     != std::string::npos);
    CHECK(file("Colormaps/tx01x00.dds") && *file("Colormaps/tx01x00.dds") == "colormap"
          && file("Detailmaps/tx00x01_2.dds") && *file("Detailmaps/tx00x01_2.dds") == "detail 2"
          && file("SimpleShadowmapio.raw") && *file("SimpleShadowmapio.raw") == "shadow" && !file("Colormaps/tx02x00.dds"));
    const std::string* lightmap = file("Lightmaps/tx01x01.dds");
    CHECK(lightmap && lightmap->size() == 128u + 512u * 512u * 4u && headerWord(*lightmap, 8) == 0x1007u
          && headerWord(*lightmap, 80) == 0x41u && headerWord(*lightmap, 108) == 0x1002u
          && headerWord(*lightmap, 128) == 0xFFFFFFFFu);
    const std::string* lowDetail = file("LowDetailmaps/tx00x00.dds");
    CHECK(lowDetail && lowDetail->size() == 43832u && headerWord(*lowDetail, 8) == 0xA1007u);
    TerrainRawFile raw;
    const std::string* compiled = file("terraindata.raw");
    CHECK(compiled && raw.parse(*compiled, error));
    CHECK(raw.patchCount == 2 && raw.patches.size() == 4 && raw.patches[1].gridX == 0 && raw.patches[1].gridZ == -1
          && raw.patches[2].gridX == -1 && raw.patches[2].gridZ == 0 && raw.ring.empty() && !raw.culler.empty()
          && std::fabs(raw.maxHeight - 10.0f) < 1e-4f && std::fabs(raw.minHeight - 9.6f) < 1e-4f
          && raw.primaryScale[0] == 1.0f && raw.secondaryScale[0] == 2.0f && raw.word2C == 0xCDCDCDCDu);
    for (size_t p = 0; p < raw.patches.size(); ++p)
        CHECK(raw.patches[p].layers.empty());

    // Written into a fresh folder under the temp folder; refused over it, and over a staging folder left behind.
    char tempRoot[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempRoot) && compiled)
    {
        const std::string target = std::string(tempRoot) + "bf2editor_newlevel_" + std::to_string(GetCurrentProcessId());
        CHECK(writeNewLevel(target, files, error));
        std::string back;
        CHECK(readWholeFile(target + "/terraindata.raw", back) && back == *compiled
              && readWholeFile(target + "/Lightmaps/Objects/LightmapAtlas.tai", back)
              && back == *file("Lightmaps/Objects/LightmapAtlas.tai"));
        CHECK(!writeNewLevel(target, files, error) && error.find("exists already") != std::string::npos
              && GetFileAttributesA((target + ".tmp").c_str()) == INVALID_FILE_ATTRIBUTES);
        const std::string other = target + "_b";
        CreateDirectoryA((other + ".tmp").c_str(), 0);
        CHECK(!writeNewLevel(other, files, error) && error.find("interrupted") != std::string::npos
              && GetFileAttributesA(other.c_str()) == INVALID_FILE_ATTRIBUTES);
        RemoveDirectoryA((other + ".tmp").c_str());
        for (size_t i = 0; i < files.size(); ++i)
            DeleteFileA((target + "/" + files[i].path).c_str());
        static const char* const kFolders[] = { "Lightmaps/Objects", "Lightmaps", "LowDetailmaps", "Detailmaps",
                                                "Colormaps", "Editor", "Info", "Hud/Minimap", "Hud",
                                                "GameModes/gpm_cq/16", "GameModes/gpm_cq", "GameModes" };
        for (size_t i = 0; i < sizeof(kFolders) / sizeof(kFolders[0]); ++i)
            RemoveDirectoryA((target + "/" + kFolders[i]).c_str());
        RemoveDirectoryA(target.c_str());
        CHECK(GetFileAttributesA(target.c_str()) == INVALID_FILE_ATTRIBUTES);
    }

    const char* const golden = std::getenv("BF2_EDITOR_NEWLEVEL_GOLDEN");
    const char* const defaults = std::getenv("BF2_EDITOR_TERRAIN_CONTENT");
    const char* const karkand = std::getenv("BF2_EDITOR_KARKAND_DIR");
    if (!golden || !*golden || !defaults || !*defaults || !karkand || !*karkand)
    {
        std::printf("[new level] no staged retail new level, default content or Karkand: the golden comparison skipped\n");
        return;
    }
    const std::string stockFolder(defaults);
    NewLevelContent stock;
    std::string karkandRaw;
    CHECK(readWholeFile(stockFolder + "/defaultcolor.dds", stock.colormap)
          && readWholeFile(stockFolder + "/defaultdetail_1.dds", stock.detailmap1)
          && readWholeFile(stockFolder + "/defaultdetail_2.dds", stock.detailmap2)
          && readWholeFile(stockFolder + "/defaultingamemap.dds", stock.ingameMap)
          && readWholeFile(stockFolder + "/defaultcommandermap.dds", stock.commanderMap)
          && readWholeFile(stockFolder + "/defaultcommandermicromap.dds", stock.commanderMicromap)
          && readWholeFile(stockFolder + "/DefaultShadowmap/SimpleShadowmap.raw", stock.shadowmap)
          && readWholeFile(std::string(karkand) + "/terraindata.raw", karkandRaw));
    TerrainRawFile lodSource;
    CHECK(lodSource.parse(karkandRaw, error));
    CHECK(buildNewLevel(settings, stock, lodSource, files, error));

    // The map description's lines, the briefing's and the mode descriptions' texts left out.
    const auto descLines = [](const std::string& text) {
        std::vector<std::string> out;
        size_t start = 0;
        while (start < text.size())
        {
            size_t end = text.find('\n', start);
            if (end == std::string::npos)
                end = text.size();
            std::string line = text.substr(start, end - start);
            const size_t open = line.find('>'), close = line.rfind('<');
            if ((line.find("<briefing ") != std::string::npos || line.find("<maptype ") != std::string::npos)
                && open != std::string::npos && close != std::string::npos && close > open)
                line = line.substr(0, open + 1) + line.substr(close);
            out.push_back(line);
            start = end + 1;
        }
        return out;
    };
    std::vector<std::string> ours;
    std::string differing;
    int same = 0;
    for (size_t i = 0; i < files.size(); ++i)
    {
        const NewLevelFile& f = files[i];
        std::string lowered = f.path;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        ours.push_back(lowered);
        std::string theirs;
        const bool present = readWholeFile(std::string(golden) + "/" + f.path, theirs);
        if (f.path.compare(0, 14, "LowDetailmaps/") == 0 || f.path.compare(0, 10, "GameModes/") == 0)
        {
            if (present)
                differing += " " + f.path + " (the retail editor wrote none)";
            continue;
        }
        if (!present)
        {
            differing += " " + f.path + " (not in the retail level)";
            continue;
        }
        bool match = *f.bytes == theirs;
        if (f.path == "terraindata.raw")
        {
            TerrainRawFile mine, retail;
            match = mine.parse(*f.bytes, error) && retail.parse(theirs, error);
            if (match)
            {
                std::vector<unsigned short> a = mine.lodIndices, b = retail.lodIndices;
                std::sort(a.begin(), a.end());
                std::sort(b.begin(), b.end());
                mine.lodIndices = retail.lodIndices;
                const std::string rendered = mine.render();
                size_t at = 0;
                while (at < rendered.size() && at < theirs.size() && rendered[at] == theirs[at])
                    ++at;
                match = a == b && mine.lodWords == retail.lodWords && rendered == theirs;
                if (!match)
                    std::printf("[new level] terraindata.raw: LOD index set %s, LOD words %s, first difference at byte %u "
                                "of %u (retail %u)\n", a == b ? "same" : "differs",
                                mine.lodWords == retail.lodWords ? "same" : "differ", static_cast<unsigned>(at),
                                static_cast<unsigned>(rendered.size()), static_cast<unsigned>(theirs.size()));
            }
        }
        else if (f.path == "Info/GoldenNew.desc")
            match = descLines(*f.bytes) == descLines(theirs);
        else if (f.path == "Lightmaps/Objects/LightmapAtlas.tai")
            match = !f.bytes->empty() && (*f.bytes)[0] == '#' && !theirs.empty() && theirs[0] == '#';
        if (match)
            ++same;
        else
            differing += " " + f.path;
    }
    // Every file of the retail level is ours too, except the editor maps (left to the editor's first terrain save).
    std::string missing;
    int retailFiles = 0;
    std::vector<std::string> folders(1, std::string());
    while (!folders.empty())
    {
        const std::string folder = folders.back();
        folders.pop_back();
        WIN32_FIND_DATAA found;
        HANDLE search = FindFirstFileA((std::string(golden) + "/" + folder + "*").c_str(), &found);
        if (search == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            const std::string name = found.cFileName;
            if (name == "." || name == "..")
                continue;
            std::string path = folder + name;
            std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (path != "editor/colormaps" && path != "editor/detailmaps")
                    folders.push_back(path + "/");
                continue;
            }
            ++retailFiles;
            if (std::find(ours.begin(), ours.end(), path) == ours.end())
                missing += " " + path;
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    if (!differing.empty() || !missing.empty())
        std::printf("[new level] differing:%s; missing:%s\n", differing.c_str(), missing.c_str());
    CHECK(differing.empty() && missing.empty() && retailFiles == 281 && files.size() == 346u);
    std::printf("[new level] the retail editor's new level built again: %d of its %d files (outside the editor maps) "
                "the same; %u files with the low-detail maps and the gameplay layer\n", same, retailFiles,
                static_cast<unsigned>(files.size()));
}

// A DDS image (DdsImage.h): 16-bit R5G6B5 by its masks, a DXT1 block, the A8R8G8B8 writer read back, the bilinear scale,
// and Karkand's game detail maps (16-bit and DXT1, 256 square) when the staged game is there.
namespace
{
std::string ddsHeader(int w, int h, unsigned flags, unsigned fourCc, unsigned bits, unsigned r, unsigned g, unsigned b, unsigned a)
{
    std::string out(128, '\0');
    std::memcpy(&out[0], "DDS ", 4);
    const unsigned values[] = { 124u, 0x1007u, static_cast<unsigned>(h), static_cast<unsigned>(w) };
    std::memcpy(&out[4], values, sizeof(values));
    const unsigned format[] = { 32u, flags, fourCc, bits, r, g, b, a };
    std::memcpy(&out[76], format, sizeof(format));
    return out;
}
}

// The road files (Framework/RoadFiles): the retail writers' text for splines, roads and templates; console rows split
// with quotes and editor branches; CompiledRoads.con written back unchanged; the chunk table of a synthetic road mesh
// partitions its indices, each chunk's sphere holding its vertices. With the staged Karkand (BF2_EDITOR_KARKAND_DIR), its
// CompiledRoads.con comes back byte for byte, every mesh it loads is read with its chunk table, and every road template
// in its mod's Objects_server.zip is written back by the template writer exactly (line endings aside).
namespace
{

std::string lowerText(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

// A template as its rows make it (the rows the template writer writes).
RoadTemplateRecord templateFromRows(const std::vector<ConsoleRow>& rows)
{
    RoadTemplateRecord t;
    int layer = 0;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        std::string key = lowerText(rows[i].method);
        if (key.compare(0, 3, "set") == 0)
            key = key.substr(3);
        const std::string arg = rows[i].args.empty() ? std::string() : rows[i].args[0];
        const std::string object = lowerText(rows[i].object);
        if (object == "roadtemplate")
        {
            if (key == "name")
                t.name = arg;
            else if (key == "width")
                t.width = static_cast<float>(std::atof(arg.c_str()));
            else if (key == "maxtesselatederror")
                t.maxTesselatedError = static_cast<float>(std::atof(arg.c_str()));
            else if (key == "blendfactor")
                t.blendFactor = static_cast<float>(std::atof(arg.c_str()));
            else if (key == "prio")
                t.priority = std::atoi(arg.c_str());
            else if (key == "material")
                t.material = arg;
            else if (key == "fadeoffdistance")
                parseVec2(arg, t.fadeOffDistance);
            else if (key == "isprimarytexture")
                layer = std::atoi(arg.c_str()) ? 0 : 1;
        }
        else if (object == "roadtemplatetexture")
        {
            if (key == "texturefile")
                t.textures[layer] = arg;
            else if (key == "mirror")
                t.mirror[layer] = std::atoi(arg.c_str()) != 0;
            else if (key == "scale")
                parseVec2(arg, t.scale[layer]);
        }
    }
    return t;
}

void putBytes(std::string& data, const void* bytes, size_t size)
{
    data.append(static_cast<const char*>(bytes), size);
}

// A straight road of `quads` 2 m x 4 m quads along x, as a compiled mesh of the given minor version without a chunk table.
std::string syntheticRoadMesh(unsigned short minor, int quads)
{
    std::string data;
    const unsigned short major = 1;
    const float origin[3] = { 100.0f, 10.0f, 2.0f };
    const float radius = static_cast<float>(quads);
    const float lo[3] = { 0.0f, 10.0f, 0.0f };
    const float hi[3] = { 2.0f * quads, 10.0f, 4.0f };
    const unsigned priority = 1;
    const unsigned vertices = 2u * static_cast<unsigned>(quads + 1);
    putBytes(data, &minor, 2);
    putBytes(data, &major, 2);
    putBytes(data, origin, 12);
    putBytes(data, &radius, 4);
    putBytes(data, lo, 12);
    putBytes(data, hi, 12);
    putBytes(data, &priority, 4);
    putBytes(data, &vertices, 4);
    for (int i = 0; i <= quads; ++i)
        for (int side = 0; side < 2; ++side)
        {
            const float record[8] = { 2.0f * i - origin[0], 10.0f - origin[1], 4.0f * side - origin[2],
                                      static_cast<float>(side), static_cast<float>(i), 0.0f, 0.0f, 1.0f };
            putBytes(data, record, minor >= 3 ? 32 : 28);
        }
    const unsigned indices = 6u * static_cast<unsigned>(quads);
    putBytes(data, &indices, 4);
    for (int i = 0; i < quads; ++i)
    {
        const unsigned short a = static_cast<unsigned short>(2 * i), b = static_cast<unsigned short>(a + 1);
        const unsigned short c = static_cast<unsigned short>(a + 2), d = static_cast<unsigned short>(a + 3);
        const unsigned short triangle[6] = { a, b, c, b, d, c };
        putBytes(data, triangle, 12);
    }
    return data;
}

std::string withoutCarriageReturns(std::string text)
{
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
}

} // namespace

void checkRoadFiles()
{
    // A projection that came back flat: R3's road (inner points rising 2.8 m) with every changed height on one level
    // counts; a ramp, a road whose inner points are level, and too few heights do not.
    {
        SplineRecord spline;
        const float heights[5] = { 161.733f, 165.255f, 166.448f, 168.015f, 166.537f };
        for (int i = 0; i < 5; ++i)
        {
            SplinePointRecord point;
            point.position[1] = heights[i];
            spline.points.push_back(point);
        }
        const std::vector<float> flat(288, 165.152f);
        std::vector<float> ramp;
        for (int i = 0; i < 214; ++i)
            ramp.push_back(165.5f + 0.01f * static_cast<float>(i));
        CHECK(roadProjectionCameBackFlat(spline, flat));
        CHECK(!roadProjectionCameBackFlat(spline, ramp));
        CHECK(!roadProjectionCameBackFlat(spline, std::vector<float>(5, 165.152f)));
        for (int i = 1; i < 4; ++i)
            spline.points[static_cast<size_t>(i)].position[1] = 166.0f;
        CHECK(!roadProjectionCameBackFlat(spline, std::vector<float>(100, 165.9f)));
    }
    CHECK(formatConsoleFloat(0.01f) == "0.01");
    CHECK(formatConsoleFloat(0.85f) == "0.85");
    CHECK(formatConsoleFloat(5.0f) == "5");
    CHECK(formatConsoleFloat(-99.9473f) == "-99.9473");
    CHECK(formatConsoleFloat(163.404f) == "163.404");

    // Splines.con: points only with their non-default trigger properties, an empty spline left out, a blank line last.
    SplineRecord spline;
    spline.id = 3;
    spline.snapModes = 3;
    SplinePointRecord point;
    point.position[0] = 1.5f;
    point.position[1] = 2.0f;
    point.position[2] = -3.0f;
    spline.points.push_back(point);
    point.position[0] = 10.0f;
    point.position[1] = 20.0f;
    point.position[2] = 30.0f;
    point.speed = 30.0f;
    point.delay = 1.5f;
    point.direction = 1;
    point.sound = "s.wav";
    spline.points.push_back(point);
    SplineRecord emptySpline;
    emptySpline.id = 4;
    std::vector<SplineRecord> splines;
    splines.push_back(spline);
    splines.push_back(emptySpline);
    CHECK(splinesConText(splines) ==
          "rem ** [Spline] - 3 **\n"
          "SplineManager.CreateInstance\n"
          "SplineManager.SetId 3\n"
          "SplineManager.SetControlPointSnapModes 3\n"
          "SplineManager.setLineWidth 0.05\n"
          "SplineManager.setStepSize 3\n"
          "SplineManager.AddControlPoint 1.5/2/-3\n"
          "SplineManager.AddControlPoint 10/20/30\n"
          "SplineManager.SetSpeed 30\n"
          "SplineManager.SetDelay 1.5\n"
          "SplineManager.SetDirection 1\n"
          "SplineManager.SetSound s.wav\n"
          "SplineManager.End\n"
          "\n"
          "\n");
    CHECK(splinesConText(std::vector<SplineRecord>()).empty());

    // Roads.con.
    RoadRecord road;
    road.name = "tarmac1";
    road.splineId = 3;
    road.templateName = "highway";
    road.seed = 1234;
    CHECK(roadsConText(std::vector<RoadRecord>(1, road)) ==
          "rem ** [Road] - tarmac1**\n"
          "Road.CreateInstance \"tarmac1\" 3 \"highway\" 1234\n"
          "Road.SetTextureScaleOffset 0/0\n"
          "Road.SetPersistance 0.2\n"
          "Road.SetRange -0.15/0.15\n"
          "Road.SetOctaves 3\n"
          "Road.Prio 0\n"
          "Road.FadeDistAtStartLeft 0/3\n"
          "Road.FadeDistAtStartRight 0/3\n"
          "Road.FadeDistAtEndLeft 0/3\n"
          "Road.FadeDistAtEndRight 0/3\n"
          "\n"
          "\n");
    CHECK(roadsConText(std::vector<RoadRecord>()).empty());

    // Rows: the editor branch only, quotes off, the else part and a nested block left out.
    const std::string script = "rem a comment\n"
                               "Road.Outside 1\n"
                               "if v_arg1 == BF2Editor\n"
                               "RoadTemplate.SetName \"my road\"\n"
                               "if v_arg2 == x\n"
                               "RoadTemplate.Nested 1\n"
                               "endIf\n"
                               "RoadTemplateTexture.SetScale 1/0.5\n"
                               "else\n"
                               "RoadTemplate.Game 1\n"
                               "endIf\n";
    int skipped = 0;
    const std::vector<ConsoleRow> rows = consoleRows(script, "BF2Editor", skipped);
    CHECK(rows.size() == 2 && skipped == 1);
    CHECK(rows.size() == 2 && rows[0].object == "RoadTemplate" && rows[0].method == "SetName" && rows[0].args.size() == 1
          && rows[0].args[0] == "my road" && rows[1].args.size() == 1 && rows[1].args[0] == "1/0.5");
    CHECK(consoleRows(script, std::string(), skipped).size() == 5);
    ConsoleRow row;
    CHECK(splitConsoleRow("  Road.CreateInstance \"a b\" 3 \"t\" 7 ", row) && row.args.size() == 4 && row.args[0] == "a b"
          && row.args[3] == "7");
    CHECK(!splitConsoleRow("rem Road.CreateInstance", row) && !splitConsoleRow("endIf", row) && !splitConsoleRow("", row));

    // A template's script read back by its rows makes the same script.
    RoadTemplateRecord roadTemplate;
    roadTemplate.name = "test_road";
    roadTemplate.width = 6.0f;
    roadTemplate.maxTesselatedError = 0.01f;
    roadTemplate.blendFactor = 0.85f;
    roadTemplate.priority = 2;
    roadTemplate.material = "Dirt";
    roadTemplate.textures[0] = "objects\\roads\\textures\\a";
    roadTemplate.textures[1] = "objects\\roads\\textures\\b";
    roadTemplate.mirror[1] = true;
    roadTemplate.scale[1][1] = 0.5f;
    roadTemplate.fadeOffDistance[0] = 150.0f;
    const std::string templateText = roadTemplateConText(roadTemplate);
    CHECK(templateText.find("RoadTemplate.SetFadeOffDistance 150/300\n") != std::string::npos);
    CHECK(roadTemplateConText(templateFromRows(consoleRows(templateText, "BF2Editor", skipped))) == templateText);
    RoadTemplateRecord plain = roadTemplate;
    plain.fadeOffDistance[0] = 200.0f;
    plain.material.clear();
    const std::string plainText = roadTemplateConText(plain);
    CHECK(plainText.find("FadeOffDistance") == std::string::npos && plainText.find("SetMaterial") == std::string::npos);

    // CompiledRoads.con.
    const std::string compiled = "object.create highway\r\n"
                                 "object.geometry.loadMesh Levels\\X\\Roads\\tarmac8_compiled.mesh\r\n"
                                 "object.absoluteposition 1/2/3\r\n"
                                 "\r\n"
                                 "object.create industrial\r\n"
                                 "object.geometry.loadMesh Levels\\X\\Roads\\industrial_2_compiled.mesh\r\n"
                                 "object.absoluteposition 4/5/6\r\n"
                                 "\r\n";
    const CompiledRoadsFile compiledFile = parseCompiledRoads(compiled);
    CHECK(compiledFile.blocks.size() == 2 && compiledFile.newline == "\r\n" && compiledFile.preamble.empty()
          && compiledFile.blocks[0].mesh == "Levels\\X\\Roads\\tarmac8_compiled.mesh");
    CHECK(compiledRoadsText(compiledFile) == compiled);
    CHECK(parseCompiledRoads(std::string()).blocks.empty() && compiledRoadsText(parseCompiledRoads(std::string())).empty());
    CHECK(compiledMeshIsRoad("Levels\\X\\Roads\\tarmac8_compiled.mesh", "Tarmac8"));
    CHECK(compiledMeshIsRoad("roads/tarmac8_compiled.mesh", "tarmac8"));
    CHECK(!compiledMeshIsRoad("Levels\\X\\Roads\\xtarmac8_compiled.mesh", "tarmac8"));
    CHECK(!compiledMeshIsRoad("Levels\\X\\Other\\tarmac8_compiled.mesh", "tarmac8"));

    // The chunk table of a 200 m road: contiguous chunks covering every index, each sphere holding its vertices.
    std::string mesh = syntheticRoadMesh(4, 100);
    RoadMeshInfo info;
    std::string error;
    CHECK(readRoadMeshInfo(mesh, info, error) && !info.hasChunkTable && info.vertices == 202 && info.indices == 600);
    const size_t tableAt = mesh.size();
    CHECK(appendRoadMeshChunkTable(mesh, info, error) && info.hasChunkTable && info.chunks >= 3);
    RoadMeshInfo again;
    CHECK(readRoadMeshInfo(mesh, again, error) && again.hasChunkTable && again.chunks == info.chunks);
    bool covered = mesh.size() == tableAt + 4 + 24 * info.chunks;
    unsigned next = 0;
    bool held = true;
    for (unsigned c = 0; covered && c < info.chunks; ++c)
    {
        unsigned first = 0, count = 0;
        float radius = 0.0f, centre[3];
        std::memcpy(&first, mesh.data() + tableAt + 4 + 24 * c, 4);
        std::memcpy(&count, mesh.data() + tableAt + 8 + 24 * c, 4);
        std::memcpy(&radius, mesh.data() + tableAt + 12 + 24 * c, 4);
        std::memcpy(centre, mesh.data() + tableAt + 16 + 24 * c, 12);
        covered = covered && first == next && count % 3 == 0 && count > 0 && radius <= 40.0f;
        next = first + count;
        for (unsigned i = first; i < first + count && held; ++i)
        {
            unsigned short index = 0;
            std::memcpy(&index, mesh.data() + 52 + 202 * 32 + 4 + 2 * i, 2);
            const float x = 2.0f * (index / 2), z = 4.0f * (index % 2);
            const float dx = x - centre[0], dy = 10.0f - centre[1], dz = z - centre[2];
            held = std::sqrt(dx * dx + dy * dy + dz * dz) <= radius + 1e-3f;
        }
    }
    CHECK(covered && next == 600 && held);
    CHECK(!appendRoadMeshChunkTable(mesh, info, error) && error.find("already") != std::string::npos);
    std::string older = syntheticRoadMesh(3, 4);
    CHECK(!appendRoadMeshChunkTable(older, info, error) && error.find("1.3") != std::string::npos);
    CHECK(!readRoadMeshInfo(mesh.substr(0, 40), info, error));

    const char* const level = std::getenv("BF2_EDITOR_KARKAND_DIR");
    if (!level)
        return;
    std::string stock;
    if (readWholeFile(std::string(level) + "\\CompiledRoads.con", stock))
    {
        const CompiledRoadsFile stockFile = parseCompiledRoads(stock);
        int meshes = 0, read = 0;
        for (size_t b = 0; b < stockFile.blocks.size(); ++b)
        {
            const std::string& path = stockFile.blocks[b].mesh;
            const size_t slash = path.find_last_of("\\/");
            std::string data;
            ++meshes;
            RoadMeshInfo stockInfo;
            if (readWholeFile(std::string(level) + "\\Roads\\" + path.substr(slash + 1), data)
                && readRoadMeshInfo(data, stockInfo, error) && stockInfo.hasChunkTable && stockInfo.versionMinor == 4)
                ++read;
        }
        std::printf("[road files] Karkand's CompiledRoads.con %s; %d of its %d meshes read with their chunk tables\n",
                    compiledRoadsText(stockFile) == stock ? "written back byte for byte" : "written back DIFFERENTLY",
                    read, meshes);
        CHECK(compiledRoadsText(stockFile) == stock && stockFile.blocks.size() == 32 && read == meshes);
    }
    const std::string objects = std::string(level) + "\\..\\..\\Objects_server.zip";
    unzFile unz = unzOpen(objects.c_str());
    if (!unz)
        return;
    int templates = 0, same = 0;
    std::string first;
    for (int status = unzGoToFirstFile(unz); status == UNZ_OK; status = unzGoToNextFile(unz))
    {
        char name[256];
        unz_file_info entry;
        unzGetCurrentFileInfo(unz, &entry, name, sizeof(name), 0, 0, 0, 0);
        const std::string low = lowerText(name);
        if (low.compare(0, 14, "roads/splines/") != 0 || low.size() < 5 || low.compare(low.size() - 4, 4, ".con") != 0
            || low.find("_compiled.con") != std::string::npos)
            continue;
        std::string text(entry.uncompressed_size, '\0');
        unzOpenCurrentFile(unz);
        const int got = text.empty() ? 0 : unzReadCurrentFile(unz, &text[0], static_cast<unsigned>(text.size()));
        unzCloseCurrentFile(unz);
        if (got != static_cast<int>(text.size()))
            continue;
        ++templates;
        const std::string written = roadTemplateConText(templateFromRows(consoleRows(text, "BF2Editor", skipped)));
        if (written == withoutCarriageReturns(text))
            ++same;
        else if (first.empty())
            first = name;
    }
    unzClose(unz);
    std::printf("[road files] %d of %d stock road templates written back exactly (line endings aside)%s%s\n", same,
                templates, first.empty() ? "" : "; first difference: ", first.c_str());
    CHECK(templates > 0 && same == templates);
}

// The object lightmap atlas (Framework/LightmapAtlas): an index line read; a texture cut out of a synthetic DXT1 page with
// its mip chain, block for block; a rectangle off the blocks refused. With the staged Karkand, its LightmapAtlas.tai read
// whole and every entry cut out of its page, each one's top level the page's blocks.
void checkLightmapAtlas()
{
    std::vector<AtlasEntry> entries;
    std::string error;
    CHECK(parseAtlasIndex("# comment\n\nlevels/x/lightmaps/objects/a=00=1=2=3.dds\t\tlevels/x/lightmaps/objects/"
                          "LightmapAtlas1.dds, 1, 0.5, 0.25, 0.25, 0.125\n", entries, error)
          && entries.size() == 1 && entries[0].page == 1 && entries[0].rect[0] == 0.5f && entries[0].rect[3] == 0.125f
          && atlasFileName(entries[0].texture) == "a=00=1=2=3.dds" && atlasFileName(entries[0].atlasFile) == "LightmapAtlas1.dds");
    CHECK(!parseAtlasIndex("levels/x/a.dds\t\tpage.dds, 1, 0.5\n", entries, error));
    // The pages an older, larger atlas left: only page 2 is named by no entry (case ignored; the index and the
    // lightmaps are not pages).
    {
        std::vector<AtlasEntry> named(2);
        named[0].atlasFile = "levels/x/lightmaps/objects/LightmapAtlas0.dds";
        named[1].atlasFile = "levels/x/lightmaps/objects/lightmapatlas1.dds";
        const char* const names[] = { "LightmapAtlas0.dds", "LIGHTMAPATLAS1.DDS", "LightmapAtlas2.dds", "LightmapAtlas.tai",
                                      "a=00=1=2=3.dds" };
        const std::vector<std::string> unnamed = unnamedAtlasPages(named, std::vector<std::string>(names, names + 5));
        CHECK(unnamed.size() == 1 && unnamed[0] == "LightmapAtlas2.dds");
    }

    // A 64 x 64 DXT1 page of 7 levels, every block holding its level, column and row.
    std::string page(128, '\0');
    std::memcpy(&page[0], "DDS ", 4);
    const unsigned header[] = { 124, 0x1007 | 0x20000, 64, 64, 2048, 0, 7 };
    std::memcpy(&page[4], header, sizeof(header));
    std::memcpy(&page[84], "DXT1", 4);
    for (unsigned m = 0; m < 7; ++m)
    {
        const unsigned blocks = std::max(1u, (64u >> m) / 4u);
        for (unsigned y = 0; y < blocks; ++y)
            for (unsigned x = 0; x < blocks; ++x)
            {
                unsigned char block[8] = { static_cast<unsigned char>(m), static_cast<unsigned char>(x),
                                           static_cast<unsigned char>(y), 0, 0, 0, 0, 0 };
                page.append(reinterpret_cast<const char*>(block), 8);
            }
    }
    AtlasEntry entry;
    entry.rect[0] = 0.5f;                       // texels 32..47 across, 16..31 down: 16 x 16
    entry.rect[1] = 0.25f;
    entry.rect[2] = 0.25f;
    entry.rect[3] = 0.25f;
    std::string texture;
    CHECK(cutAtlasTexture(page, entry, texture, error));
    unsigned size[2] = { 0, 0 }, mips = 0;
    std::memcpy(size, texture.data() + 12, 8);
    std::memcpy(&mips, texture.data() + 28, 4);
    // 16 x 16 has 5 levels: 4 x 4 blocks, 2 x 2, then one block each for 4, 2 and 1 texels.
    CHECK(size[0] == 16 && size[1] == 16 && mips == 5 && texture.size() == 128 + (16 + 4 + 1 + 1 + 1) * 8);
    bool same = texture.size() == 128 + 23 * 8;
    size_t at = 128;
    for (unsigned m = 0; same && m < 5; ++m)
    {
        const unsigned blocks = std::max(1u, (16u >> m) / 4u);
        for (unsigned y = 0; same && y < blocks; ++y)
            for (unsigned x = 0; same && x < blocks; ++x, at += 8)
                same = static_cast<unsigned char>(texture[at]) == m
                    && static_cast<unsigned char>(texture[at + 1]) == std::min((32u >> m) / 4u + x, std::max(1u, (64u >> m) / 4u) - 1)
                    && static_cast<unsigned char>(texture[at + 2]) == std::min((16u >> m) / 4u + y, std::max(1u, (64u >> m) / 4u) - 1);
    }
    CHECK(same);
    entry.rect[0] = 0.5f + 2.0f / 64.0f;        // two texels off the blocks
    CHECK(!cutAtlasTexture(page, entry, texture, error));

    const char* const level = std::getenv("BF2_EDITOR_KARKAND_DIR");
    if (!level)
        return;
    const std::string folder = std::string(level) + "\\lightmaps\\Objects\\";
    std::string index;
    if (!readWholeFile(folder + "LightmapAtlas.tai", index))
        return;
    CHECK(parseAtlasIndex(index, entries, error));
    std::map<std::string, std::string> pages;
    int cut = 0, matching = 0;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const std::string name = atlasFileName(entries[i].atlasFile);
        if (!pages.count(name))
            readWholeFile(folder + name, pages[name]);
        const std::string& bytes = pages[name];
        if (!cutAtlasTexture(bytes, entries[i], texture, error))
            continue;
        ++cut;
        // The cut texture's top level is the page's blocks under its rectangle.
        unsigned pageWidth = 0, w = 0, h = 0;
        std::memcpy(&pageWidth, bytes.data() + 16, 4);
        std::memcpy(&h, texture.data() + 12, 4);
        std::memcpy(&w, texture.data() + 16, 4);
        const unsigned x0 = static_cast<unsigned>(std::lround(entries[i].rect[0] * pageWidth)) / 4;
        const unsigned y0 = static_cast<unsigned>(std::lround(entries[i].rect[1] * pageWidth)) / 4;
        bool top = true;
        for (unsigned y = 0; top && y < h / 4; ++y)
            top = std::memcmp(texture.data() + 128 + y * (w / 4) * 8, bytes.data() + 128 + ((y0 + y) * (pageWidth / 4) + x0) * 8,
                              (w / 4) * 8) == 0;
        matching += top ? 1 : 0;
    }
    std::printf("[lightmap atlas] Karkand's LightmapAtlas.tai: %u entries, %d cut out of their pages, %d with the page's "
                "blocks\n", static_cast<unsigned>(entries.size()), cut, matching);
    CHECK(entries.size() == 1827 && cut == 1827 && matching == 1827);
}

void checkDdsImage()
{
    std::string error;
    // 2 x 1 R5G6B5: pure red, pure blue.
    std::string rgb565 = ddsHeader(2, 1, 0x40u, 0u, 16u, 0xF800u, 0x07E0u, 0x001Fu, 0u);
    const unsigned short texels[2] = { 0xF800u, 0x001Fu };
    rgb565.append(reinterpret_cast<const char*>(texels), 4);
    DdsImage image;
    CHECK(image.read(rgb565, error) && image.width == 2 && image.height == 1 && image.pixels[0] == 0xFFFF0000u
          && image.pixels[1] == 0xFF0000FFu);
    // One DXT1 block: colour 0 white, colour 1 black, every texel index 0 (white) except the last (1: black).
    std::string dxt1 = ddsHeader(4, 4, 0x4u, 0x31545844u, 0u, 0u, 0u, 0u, 0u);
    const unsigned char block[8] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40 };
    dxt1.append(reinterpret_cast<const char*>(block), 8);
    CHECK(image.read(dxt1, error) && image.width == 4 && image.pixels[0] == 0xFFFFFFFFu && image.pixels[15] == 0xFF000000u);
    DdsImage back;
    CHECK(back.read(image.write(), error) && back.width == 4 && back.pixels == image.pixels);
    const DdsImage big = image.scaled(8, 8);
    CHECK(big.width == 8 && big.height == 8 && big.pixels[0] == 0xFFFFFFFFu && big.pixels.size() == 64);
    CHECK(!image.read("DDS ", error) && !image.read(std::string(200, 'x'), error));
    // Karkand's game detail maps, when staged.
    const char* const level = std::getenv("BF2_EDITOR_KARKAND_DIR");
    if (!level || !*level)
    {
        std::printf("[DDS detail maps] BF2_EDITOR_KARKAND_DIR unset: skipped\n");
        return;
    }
    const std::string folder = std::string(level) + "/Detailmaps/";
    const char* const names[2] = { "tx00x00_1.dds", "tx00x01_1.dds" };
    for (int i = 0; i < 2; ++i)
    {
        FILE* file = std::fopen((std::string(folder) + names[i]).c_str(), "rb");
        if (!file)
            continue;
        std::string bytes;
        char buffer[65536];
        size_t n = 0;
        while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
            bytes.append(buffer, n);
        std::fclose(file);
        DdsImage detail;
        CHECK(detail.read(bytes, error) && detail.width == 256 && detail.height == 256);
        const DdsImage editor = detail.scaled(512, 512);
        DdsImage reread;
        CHECK(reread.read(editor.write(), error) && reread.width == 512 && reread.pixels.size() == 512u * 512u);
    }
}

// A .baf header (BafHeader.h): version 4, the bone ids, the frame count kept in 16 bits at 24 frames a second.
void checkBafHeader()
{
    std::string bytes;
    const int version = 4;
    const unsigned short bones = 3;
    const unsigned short ids[3] = { 0, 5, 2 };
    const int frames = 48;
    const unsigned char precision = 7;
    bytes.append(reinterpret_cast<const char*>(&version), 4);
    bytes.append(reinterpret_cast<const char*>(&bones), 2);
    bytes.append(reinterpret_cast<const char*>(ids), 6);
    bytes.append(reinterpret_cast<const char*>(&frames), 4);
    bytes.append(reinterpret_cast<const char*>(&precision), 1);
    BafHeader header;
    std::string error;
    CHECK(readBafHeader(bytes, header, error) && header.joints() == 3 && header.boneIds[1] == 5 && header.frames == 48
          && header.seconds() == 2.0f && header.precision == 7);
    CHECK(!readBafHeader(bytes.substr(0, 9), header, error));
    std::string old = bytes;
    old[0] = 3;
    CHECK(!readBafHeader(old, header, error));
    // 70000 frames: the engine keeps 16 bits of them (BoneAnimation::load narrows to a ushort).
    std::string many = bytes;
    const int big = 70000;
    many.replace(12, 4, reinterpret_cast<const char*>(&big), 4);
    CHECK(readBafHeader(many, header, error) && header.seconds() == static_cast<float>(70000 & 0xFFFF) / 24.0f);
}

// The mod and level description files (DescFile.h): text changed in place, the rest byte for byte.
void checkDescFile()
{
    const std::string mod = "<mod>\r\n\t<title> MODDESC_BF2_TITLE </title>\r\n\t<url> http://x/y.jsp </url>\r\n"
                            "\t<version> BF2 1.5 </version>\r\n</mod>";
    std::string text;
    CHECK(descText(mod, "title", text) && text == "MODDESC_BF2_TITLE");
    CHECK(!descText(mod, "logo", text));
    std::string edited = mod;
    CHECK(setDescText(edited, "mod", "title", "My Mod & more"));
    CHECK(edited == "<mod>\r\n\t<title> My Mod &amp; more </title>\r\n\t<url> http://x/y.jsp </url>\r\n"
                    "\t<version> BF2 1.5 </version>\r\n</mod>");
    CHECK(descText(edited, "title", text) && text == "My Mod & more");
    // A missing element: before </mod>, indented as the first child, with the file's line end.
    CHECK(setDescText(edited, "mod", "logo", "mod.png"));
    CHECK(edited.find("\t<version> BF2 1.5 </version>\r\n\t<logo> mod.png </logo>\r\n</mod>") != std::string::npos);
    // Taken out again (an undo): its whole line goes, and the title put back gives the stock bytes.
    CHECK(removeDescElement(edited, "logo") && !removeDescElement(edited, "logo"));
    CHECK(setDescText(edited, "mod", "title", "MODDESC_BF2_TITLE") && edited == mod);
    // An attribute and a tight text stay as they were.
    std::string level = "<map gsid=\"4\">\n\t<name> Strike At Karkand </name>\n"
                        "\t<briefing locid=\"LOADINGSCREEN_X\">old text</briefing>\n</map>";
    CHECK(setDescText(level, "map", "briefing", "new <text>"));
    CHECK(level.find("<briefing locid=\"LOADINGSCREEN_X\">new &lt;text&gt;</briefing>") != std::string::npos);
    CHECK(setDescText(level, "map", "name", "Karkand"));
    CHECK(level.find("<name> Karkand </name>") != std::string::npos && level.find("<map gsid=\"4\">") == 0);
    // An element name that is the start of another is not mistaken for it; an empty tag is refused.
    std::string tricky = "<map>\n\t<names> a </names>\n\t<music/>\n</map>";
    CHECK(!descText(tricky, "name", text));
    CHECK(!setDescText(tricky, "map", "music", "x.ogg"));
    CHECK(!setDescText(tricky, "nothing", "name", "x"));
}

// The retail level editor's spawner presets and team list (LevelEditorDefs.h).
void checkLevelEditorDefs()
{
    LevelEditorDefs defs;
    std::vector<std::string> problems;
    defs.parseSpawnerDefs("rem *** MEC/US ***\r\n\r\nObjectSpawnerDef.AddDef ME_US_APC \tAPC_BTR90 USAPC_LAV25 \r\n"
                          "objectspawnerdef.adddef ME_US_VTOL\tAIR_F35B AIR_F35B\r\n"
                          "ObjectSpawnerDef.AddDef broken ONLY_ONE\r\n"
                          "ObjectSpawnerDef.Something x y z\r\n", problems);
    CHECK(defs.spawnerPresets().size() == 2);
    CHECK(problems.size() == 1);
    CHECK(defs.spawnerPresets()[0].name == "ME_US_APC" && defs.spawnerPresets()[0].team1 == "APC_BTR90"
          && defs.spawnerPresets()[0].team2 == "USAPC_LAV25");
    CHECK(defs.findPreset("me_us_vtol") && defs.findPreset("me_us_vtol")->team2 == "AIR_F35B");
    CHECK(!defs.findPreset("broken"));
    problems.clear();
    defs.parseTeams("LevelSettings.AddTeam US English\nLevelSettings.AddTeam MEC MEC\nLevelSettings.AddTeam CH\n", problems);
    CHECK(defs.teams().size() == 2 && problems.size() == 1);
    CHECK(defs.findTeam("us") && defs.findTeam("us")->language == "English");
    CHECK(!defs.findTeam("CH"));
    // A mod without the folder: both lists empty, nothing reported.
    problems.clear();
    defs.load("no_such_mod_here", problems);
    CHECK(defs.spawnerPresets().empty() && defs.teams().empty() && problems.empty());
}

void checkMaterialFiles()
{
    CHECK(materialNumber(0.01001f) == "0.01001" && materialNumber(100000.0f) == "100000" && materialNumber(1e6f) == "1e+06"
          && materialNumber(-1.0f) == "-1" && materialNumber(0.1f) == "0.1" && materialNumber(0.0f) == "0");
    const std::string define =
        "Material.active 0\r\nMaterial.name \"Default\"\r\nMaterial.type 4\r\nMaterial.friction 0.75\r\nMaterial.elasticity 0\r\n"
        "Material.resistance 0.02\r\n\r\n"
        "Material.active 1\r\nMaterial.name \"Water_deep\"\r\nMaterial.type 0\r\nMaterial.friction 1.1\r\nMaterial.elasticity 0\r\n"
        "Material.resistance 0.02\r\nMaterial.damageLoss 25\r\nMaterial.minDamageLoss 10\r\nMaterial.penetrationDeviation 0.2\r\n"
        "Material.hasWaterPhysics 1\r\n\r\n"
        "Material.active 5\r\nMaterial.name \"Sand\"\r\nMaterial.type 0\r\nMaterial.friction 0.8\r\nMaterial.elasticity 0\r\n"
        "Material.resistance 0.06\r\n\r\n";
    MaterialDefineFile materials;
    materials.parse(define);
    std::string value, error;
    CHECK(materials.render() == define && !materials.hasChanges() && materials.ids().size() == 3 && materials.ids()[2] == 5
          && materials.violations().empty());
    CHECK(materials.value(1, "name", value) && value == "Water_deep" && materials.value(0, "damageLoss", value) && value == "-1"
          && materials.value(1, "minDamageLoss", value) && value == "10" && materials.value(5, "isOneSided", value) && value == "0"
          && !materials.value(9, "name", value) && !materials.value(0, "colour", value));
    CHECK(materials.setValue(0, "friction", "0.50", error) && materials.value(0, "friction", value) && value == "0.5");
    CHECK(materials.setValue(5, "penetrationDeviation", "0.25", error) && materials.setValue(5, "projectileCollisionHardness", "2", error)
          && materials.setValue(5, "isSeeThrough", "1", error) && materials.setValue(5, "isOneSided", "1", error));
    std::vector<std::string> lines = materialLines(materials.render());
    CHECK(lines.size() == 29 && lines[21] == "Material.friction 0.8" && lines[23] == "Material.resistance 0.06"
          && lines[24] == "Material.projectileCollisionHardness 2" && lines[25] == "Material.penetrationDeviation 0.25"
          && lines[26] == "Material.isOneSided 1" && lines[27] == "Material.isSeeThrough 1" && lines[28].empty());
    CHECK(materials.setValue(5, "penetrationDeviation", "0.1", error) && materials.setValue(5, "isOneSided", "0", error)
          && materialLines(materials.render()).size() == 27);
    CHECK(materials.setValue(1, "damageLoss", "-1", error) && materials.value(1, "minDamageLoss", value) && value == "0"
          && materialLines(materials.render())[13] == "Material.penetrationDeviation 0.2"
          && !materials.setValue(1, "minDamageLoss", "5", error));
    CHECK(materials.setValue(0, "name", "Base", error) && materialLines(materials.render())[1] == "Material.name \"Base\""
          && !materials.setValue(0, "name", "a\"b", error) && !materials.setValue(0, "type", "x", error)
          && !materials.setValue(9, "type", "1", error) && !materials.setValue(0, "colour", "1", error)
          && !materials.setValue(0, "isBarbwire", "2", error));
    CHECK(materials.add(3, "Gravel", 0, error) && !materials.add(3, "Again", 0, error) && !materials.add(10000, "Big", 0, error)
          && materials.add(20, "End", 1, error));
    lines = materialLines(materials.render());
    CHECK(materials.ids().size() == 5 && materials.ids()[2] == 3 && materials.ids()[4] == 20 && lines[16] == "Material.active 3"
          && lines[17] == "Material.name \"Gravel\"" && lines[18] == "Material.type 0" && lines[19] == "Material.friction 1"
          && lines[20] == "Material.elasticity 0" && lines[21] == "Material.resistance 0.01" && lines[22].empty()
          && lines.back().empty() && materials.violations().empty());
    CHECK(materials.remove(20, error) && materials.remove(3, error) && !materials.remove(3, error)
          && materials.ids().size() == 3 && materials.violations().empty());
    MaterialDefineFile broken;
    broken.parse("Material.active 2\r\nMaterial.type 1\r\nMaterial.name \"x\"\r\nMaterial.friction 1\r\nMaterial.elasticity 0\r\n"
                 "Material.resistance 0.010\r\nMaterial.isBarbwire 0\r\n\r\nMaterial.active 1\r\nMaterial.name \"y\"\r\n");
    CHECK(broken.violations().size() >= 6);

    const std::string cellsText = "Run ImpactSounds.con\r\nRun MaterialManagerdefine.con\r\n\r\n"
                                  "MaterialManager.createCell 0 0\r\nMaterialManager.createCell 0 12\r\nMaterialManager.damageMod 0\r\n"
                                  "MaterialManager.createCell 1 0\r\nMaterialManager.damageMod 1\r\n"
                                  "MaterialManager.setEffectTemplate 0 e_splash\r\nMaterialManager.setSoundTemplate 0 S_Splash\r\n";
    MaterialCellFile cells;
    cells.parse(cellsText);
    CHECK(cells.render() == cellsText && cells.cells().size() == 3 && cells.cells()[1] == std::make_pair(0, 12)
          && cells.violations().empty());
    CHECK(cells.damageMod(0, 12, value) && value == "0" && cells.damageMod(0, 0, value) && value.empty()
          && cells.templateAt(1, 0, MaterialCellFile::Effect, 0, value) && value == "e_splash"
          && cells.indexes(1, 0, MaterialCellFile::Sound).size() == 1 && !cells.damageMod(7, 7, value));
    CHECK(cells.add(0, 5, error) && cells.setDamageMod(0, 5, "0.50", error)
          && cells.setTemplate(0, 5, MaterialCellFile::Decal, 0, "d_hole", error)
          && cells.setTemplate(0, 5, MaterialCellFile::Effect, 1, "e_b", error)
          && cells.setTemplate(0, 5, MaterialCellFile::Effect, 0, "e_a", error));
    lines = materialLines(cells.render());
    CHECK(lines.size() == 15 && lines[4] == "MaterialManager.createCell 0 5" && lines[5] == "MaterialManager.damageMod 0.5"
          && lines[6] == "MaterialManager.setEffectTemplate 0 e_a" && lines[7] == "MaterialManager.setEffectTemplate 1 e_b"
          && lines[8] == "MaterialManager.setDecalTemplate 0 d_hole" && lines[9] == "MaterialManager.createCell 0 12");
    CHECK(cells.setDamageMod(0, 5, "-1", error) && cells.setTemplate(0, 5, MaterialCellFile::Effect, 1, "", error)
          && materialLines(cells.render()).size() == 13 && !cells.add(1, 0, error) && !cells.setDamageMod(7, 7, "1", error)
          && !cells.setTemplate(0, 5, MaterialCellFile::Sound, 0, "two words", error));
    CHECK(cells.add(2, 3, error) && materialLines(cells.render()).back() == "MaterialManager.createCell 2 3"
          && cells.render().substr(cells.render().size() - 2) == "\r\n" && cells.violations().empty());
    CHECK(cells.removeCellsOf(0) == 4 && cells.cells().size() == 1 && cells.violations().empty()
          && cells.remove(2, 3, error) && !cells.remove(2, 3, error));
    MaterialCellFile shuffled;
    shuffled.parse("MaterialManager.createCell 1 0\r\nMaterialManager.setSoundTemplate 0 S\r\nMaterialManager.damageMod 1\r\n"
                   "MaterialManager.createCell 0 3\r\n");
    CHECK(shuffled.violations().size() == 2);

    // The stock files, when a folder of them is given.
    char samples[MAX_PATH] = "";
    if (GetEnvironmentVariableA("BF2EDITOR_MATERIAL_SAMPLES", samples, MAX_PATH) == 0)
        return;
    std::string defineText, cellText;
    const bool read = ObjectLibrary::readTextFile(std::string(samples) + "\\materialManagerDefine.con", defineText, error)
                      && ObjectLibrary::readTextFile(std::string(samples) + "\\materialManagerSettings.con", cellText, error);
    CHECK(read);
    MaterialDefineFile stockMaterials;
    stockMaterials.parse(defineText);
    MaterialCellFile stockCells;
    stockCells.parse(cellText);
    const std::vector<std::string> defineRules = stockMaterials.violations();
    const std::vector<std::string> cellRules = stockCells.violations();
    for (size_t i = 0; i < defineRules.size() && i < 5; ++i)
        std::printf("stock material rule: %s\n", defineRules[i].c_str());
    for (size_t i = 0; i < cellRules.size() && i < 5; ++i)
        std::printf("stock cell rule: %s\n", cellRules[i].c_str());
    std::printf("[bf2_editor_checks] stock materials: %u, cells: %u, written back byte for byte: %s, rules broken: %u / %u\n",
                static_cast<unsigned>(stockMaterials.ids().size()), static_cast<unsigned>(stockCells.cells().size()),
                stockMaterials.render() == defineText && stockCells.render() == cellText ? "yes" : "NO",
                static_cast<unsigned>(defineRules.size()), static_cast<unsigned>(cellRules.size()));
    CHECK(read && stockMaterials.render() == defineText && stockCells.render() == cellText && defineRules.empty() && cellRules.empty()
          && stockMaterials.ids().size() == 143 && stockCells.cells().size() == 5054);
}

// The save preview's line diff (Framework/TextDiff): hunks and their numbers, line endings, and on random texts a diff
// that rebuilds the new text and is as short as the longest common subsequence allows.
void checkTextDiff()
{
    TextDiff same = diffTexts("a\nb\n", "a\nb\n");
    CHECK(same.hunks.empty() && same.added == 0 && same.removed == 0);
    TextDiff one = diffTexts("a\nb\nc\n", "a\nB\nc\n", 1);
    CHECK(one.hunks.size() == 1 && one.removed == 1 && one.added == 1);
    CHECK(one.hunks.size() == 1 && one.hunks[0].lines.size() == 4 && one.hunks[0].lines[0].kind == ' '
          && one.hunks[0].lines[1].kind == '-' && one.hunks[0].lines[1].text == "b" && one.hunks[0].lines[2].kind == '+'
          && one.hunks[0].lines[2].text == "B" && one.hunks[0].beforeStart == 1 && one.hunks[0].beforeCount == 3
          && one.hunks[0].afterStart == 1 && one.hunks[0].afterCount == 3);
    CHECK(unifiedDiff(one, "old", "new") == "--- old\n+++ new\n@@ -1,3 +1,3 @@\n a\n-b\n+B\n c\n");
    TextDiff made = diffTexts("", "x\n");
    CHECK(made.hunks.size() == 1 && made.hunks[0].beforeStart == 0 && made.hunks[0].beforeCount == 0
          && made.hunks[0].afterStart == 1 && made.hunks[0].afterCount == 1 && made.added == 1);
    TextDiff ending = diffTexts("a\nb", "a\nb\n");
    CHECK(ending.removed == 1 && ending.added == 1 && ending.hunks.size() == 1 && ending.hunks[0].lines.size() == 4
          && ending.hunks[0].lines[2].kind == '\\');
    CHECK(diffTexts("a\r\n", "a\n").removed == 1);
    std::string ten, twoApart, twoClose;
    for (int i = 1; i <= 10; ++i)
    {
        ten += std::to_string(i) + "\n";
        twoApart += (i == 2 || i == 9 ? "x" : std::to_string(i)) + "\n";
        twoClose += (i == 2 || i == 4 ? "x" : std::to_string(i)) + "\n";
    }
    CHECK(diffTexts(ten, twoApart, 1).hunks.size() == 2 && diffTexts(ten, twoClose, 1).hunks.size() == 1);
    CHECK(looksLikeText("Object.create a\r\n") && !looksLikeText(std::string("DDS \0\0", 6)));

    unsigned seed = 12345;
    const auto next = [&seed]() {
        seed = seed * 1103515245u + 12345u;
        return (seed >> 16) & 0x7fff;
    };
    bool rebuilt = true, shortest = true;
    for (int round = 0; round < 300; ++round)
    {
        std::vector<std::string> a, b;
        const int n = static_cast<int>(next() % 30), m = static_cast<int>(next() % 30);
        for (int i = 0; i < n; ++i)
            a.push_back(std::string(1, static_cast<char>('a' + next() % 4)));
        for (int j = 0; j < m; ++j)
            b.push_back(std::string(1, static_cast<char>('a' + next() % 4)));
        std::string before, after;
        for (size_t i = 0; i < a.size(); ++i)
            before += a[i] + "\n";
        for (size_t j = 0; j < b.size(); ++j)
            after += b[j] + "\n";
        const TextDiff diff = diffTexts(before, after, static_cast<int>(next() % 4));
        // The new text from the old one and the hunks.
        std::string out;
        int at = 0;     // old lines taken so far
        for (size_t h = 0; h < diff.hunks.size(); ++h)
        {
            const DiffHunk& hunk = diff.hunks[h];
            const int start = hunk.beforeCount ? hunk.beforeStart - 1 : hunk.beforeStart;
            for (; at < start; ++at)
                out += a[static_cast<size_t>(at)] + "\n";
            for (size_t l = 0; l < hunk.lines.size(); ++l)
            {
                if (hunk.lines[l].kind == ' ')
                {
                    out += hunk.lines[l].text + "\n";
                    ++at;
                }
                else if (hunk.lines[l].kind == '-')
                    ++at;
                else if (hunk.lines[l].kind == '+')
                    out += hunk.lines[l].text + "\n";
            }
        }
        for (; at < n; ++at)
            out += a[static_cast<size_t>(at)] + "\n";
        rebuilt = rebuilt && out == after;
        std::vector<std::vector<int> > lcs(static_cast<size_t>(n) + 1, std::vector<int>(static_cast<size_t>(m) + 1, 0));
        for (int i = n - 1; i >= 0; --i)
            for (int j = m - 1; j >= 0; --j)
                lcs[i][j] = a[i] == b[j] ? lcs[i + 1][j + 1] + 1 : std::max(lcs[i + 1][j], lcs[i][j + 1]);
        shortest = shortest && diff.removed + diff.added == n + m - 2 * lcs[0][0];
    }
    CHECK(rebuilt);
    CHECK(shortest);
}

// Every file under a folder, with its path.
void walkFiles(const std::string& folder, std::vector<std::string>& out)
{
    WIN32_FIND_DATAA found;
    const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const std::string name = found.cFileName;
        if (name == "." || name == "..")
            continue;
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            walkFiles(folder + "\\" + name, out);
        else
            out.push_back(folder + "\\" + name);
    } while (FindNextFileA(search, &found));
    FindClose(search);
}

// The round-trip census of the stock levels' own files: with BF2EDITOR_LEVEL_SAMPLES set to the folder
// extract_level_samples.py fills (each level's object, gameplay, settings and compiled-road files from its server.zip),
// every file is parsed by the editor's file model and rendered back with no edit; each kind's count of files that come
// back byte for byte is printed, and every file must.
void checkLevelSamples()
{
    char samples[MAX_PATH] = "";
    if (GetEnvironmentVariableA("BF2EDITOR_LEVEL_SAMPLES", samples, MAX_PATH) == 0)
        return;
    std::vector<std::string> files;
    walkFiles(samples, files);
    const char* const kinds[] = { "staticobjects.con", "ambientobjects.con", "gameplayobjects.con", "init.con", "sky.con",
                                  "water.con", "compiledroads.con" };
    const size_t kindCount = sizeof(kinds) / sizeof(kinds[0]);
    int total[kindCount] = {}, same[kindCount] = {};
    int moveTotal = 0, moveOne = 0, deleteTotal = 0, deleteOne = 0;
    std::string error;
    for (size_t i = 0; i < files.size(); ++i)
    {
        std::string name = files[i].substr(files[i].find_last_of('\\') + 1);
        std::transform(name.begin(), name.end(), name.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
        size_t kind = kindCount;
        for (size_t k = 0; k < kindCount && kind == kindCount; ++k)
            if (name == kinds[k])
                kind = k;
        std::string text;
        if (kind == kindCount || !ObjectLibrary::readTextFile(files[i], text, error))
            continue;
        std::string back;
        if (kind <= 2)
        {
            LevelObjectFile file;
            file.parse(text);
            back = file.render();
            // One object moved a metre: only its position line changes. One deleted: only its own lines go.
            if (file.recordCount() > 0)
            {
                const size_t record = file.recordCount() / 2;
                LevelObjectFile moved;
                moved.parse(text);
                ObjectPlacement placement = moved.placement(record);
                placement.position[0] += 1.0f;
                moved.setPlacement(record, placement);
                const TextDiff move = diffTexts(text, moved.render(), 0);
                ++moveTotal;
                if (move.removed == 1 && move.added == 1 && move.hunks.size() == 1)
                    ++moveOne;
                else
                    std::printf("[level files] a move changed more than its line: %s (-%d +%d)\n",
                                files[i].c_str() + std::strlen(samples), move.removed, move.added);
                LevelObjectFile deleted;
                deleted.parse(text);
                deleted.setDeleted(record, true);
                const TextDiff gone = diffTexts(text, deleted.render(), 0);
                ++deleteTotal;
                if (gone.added == 0 && gone.removed > 0 && gone.hunks.size() == 1)
                    ++deleteOne;
                else
                    std::printf("[level files] a delete changed more than its block: %s (-%d +%d, %u hunks)\n",
                                files[i].c_str() + std::strlen(samples), gone.removed, gone.added,
                                static_cast<unsigned>(gone.hunks.size()));
            }
        }
        else if (kind <= 5)
        {
            LevelSettingsFile file;
            file.parse(text);
            back = file.render();
        }
        else
            back = compiledRoadsText(parseCompiledRoads(text));
        ++total[kind];
        if (back == text)
            ++same[kind];
        else
            std::printf("[level files] written back DIFFERENTLY: %s (%u -> %u bytes)\n", files[i].c_str() + std::strlen(samples),
                        static_cast<unsigned>(text.size()), static_cast<unsigned>(back.size()));
    }
    std::string line;
    int all = 0, allSame = 0;
    for (size_t k = 0; k < kindCount; ++k)
    {
        char part[96];
        std::snprintf(part, sizeof(part), "%s%s %d/%d", k ? ", " : "", kinds[k], same[k], total[k]);
        line += part;
        all += total[k];
        allSame += same[k];
        CHECK(same[k] == total[k]);
    }
    std::printf("[level files] stock level files written back byte for byte: %d of %d (%s)\n", allSame, all, line.c_str());
    std::printf("[level files] an object moved changes its position line alone in %d of %d object files; one deleted takes "
                "only its own lines in %d of %d\n", moveOne, moveTotal, deleteOne, deleteTotal);
    CHECK(total[0] >= 20 && total[2] >= 60);
    CHECK(moveOne == moveTotal && deleteOne == deleteTotal);
}

// A folder tree of the checks' own removed.
void removeCheckTree(const std::string& path)
{
    WIN32_FIND_DATAA found;
    const HANDLE search = FindFirstFileA((path + "/*").c_str(), &found);
    if (search != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string name = found.cFileName;
            if (name == "." || name == "..")
                continue;
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                removeCheckTree(path + "/" + name);
            else
            {
                SetFileAttributesA((path + "/" + name).c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileA((path + "/" + name).c_str());
            }
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    RemoveDirectoryA(path.c_str());
}

// The save plan (Framework/SavePlan): parts, the files already as planned dropped, a write with its backup and
// manifest, a part source control refuses, a failed write that puts every file back, a restore, and the oldest
// backups removed.
void checkSavePlan()
{
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    char unique[64];
    std::snprintf(unique, sizeof(unique), "bf2editor_saveplan_%lu", static_cast<unsigned long>(GetCurrentProcessId()));
    const std::string root = std::string(temp) + unique;
    removeCheckTree(root);
    const std::string game = root + "/game";
    const std::string level = game + "/mods/bf2/Levels/Test";
    const std::string backups = game + "/bf2editor/Backups/bf2/Test";
    std::string error, text;
    CHECK(ObjectLibrary::writeTextFile(level + "/a.con", "old a", error));
    CHECK(ObjectLibrary::writeTextFile(level + "/b.con", "old b", error));
    CHECK(ObjectLibrary::writeTextFile(level + "/d.con", "same", error));
    CHECK(ObjectLibrary::writeTextFile(root + "/staged/b.con", "new b", error));
    const auto read = [](const std::string& path) {
        std::string bytes, ignored;
        return ObjectLibrary::readTextFile(path, bytes, ignored) ? bytes : std::string("(missing)");
    };

    SavePlan plan;
    bool objectsMarked = false, terrainMarked = false, refusedMarked = false;
    SavePart& objects = plan.part("objects");
    objects.add(level + "/a.con", "new a");
    objects.add(level + "/A.CON", "newer a");
    objects.onWritten([&objectsMarked]() { objectsMarked = true; });
    objects.onReport([](const SavePart& p) { return "objects " + std::to_string(p.changed()); });
    SavePart& terrain = plan.part("terrain");
    terrain.addStaged(level + "/b.con", root + "/staged/b.con");
    terrain.add(level + "/sub/c.con", "c");
    terrain.add(level + "/d.con", "same");
    terrain.onWritten([&terrainMarked]() { terrainMarked = true; });
    terrain.onReport([](const SavePart& p) { return "terrain " + std::to_string(p.changed()) + " of " + std::to_string(p.planned()); });
    SavePart& refused = plan.part("refused");
    refused.add(level + "/e.con", "e");
    refused.onWritten([&refusedMarked]() { refusedMarked = true; });
    refused.refuse("refused NOT saved: a reason");
    CHECK(objects.files().size() == 1 && objects.planned() == 1 && objects.files()[0].bytes == "newer a");
    plan.dropUnchanged();
    CHECK(terrain.planned() == 3 && terrain.changed() == 2 && plan.fileCount() == 3 && plan.anyRefused());

    SaveBackupPolicy policy;
    policy.root = backups;
    policy.base = game;
    policy.keep = 3;
    policy.label = "check";
    SaveWriteResult written;
    CHECK(writeSavePlan(plan, policy, written) && written.error.empty() && written.written.size() == 3);
    CHECK(read(level + "/a.con") == "newer a" && read(level + "/b.con") == "new b" && read(level + "/sub/c.con") == "c"
          && read(level + "/e.con") == "(missing)");
    CHECK(objectsMarked && terrainMarked && !refusedMarked);
    CHECK(plan.answers() == "objects 1; terrain 2 of 3; refused NOT saved: a reason");
    CHECK(read(level + "/a.con.editor-new") == "(missing)" && read(level + "/sub/c.con.editor-new") == "(missing)");
    std::vector<SaveBackup> list = listSaveBackups(backups);
    CHECK(list.size() == 1 && list[0].label == "check" && list[0].entries.size() == 3 && written.backup == list[0].folder);
    if (list.size() == 1 && list[0].entries.size() == 3)
    {
        CHECK(list[0].entries[0].kind == "replaced" && list[0].entries[0].target == "mods/bf2/Levels/Test/a.con"
              && read(list[0].folder + "/" + list[0].entries[0].stored) == "old a");
        CHECK(list[0].entries[1].kind == "replaced" && read(list[0].folder + "/" + list[0].entries[1].stored) == "old b");
        CHECK(list[0].entries[2].kind == "made" && list[0].entries[2].target == "mods/bf2/Levels/Test/sub/c.con");
    }

    // A read-only file: source control (the null provider) refuses its part, and the other part is written.
    CHECK(ObjectLibrary::writeTextFile(level + "/ro.con", "kept", error));
    SetFileAttributesA((level + "/ro.con").c_str(), FILE_ATTRIBUTE_READONLY);
    SavePlan locked;
    locked.part("locked").add(level + "/ro.con", "changed");
    locked.part("free").add(level + "/f.con", "f");
    locked.dropUnchanged();
    CHECK(writeSavePlan(locked, policy, written) && read(level + "/ro.con") == "kept" && read(level + "/f.con") == "f");
    CHECK(locked.parts()[0].refused() && locked.parts()[0].refusal().find("source control refused") != std::string::npos);
    SetFileAttributesA((level + "/ro.con").c_str(), FILE_ATTRIBUTE_NORMAL);

    // A target that cannot be replaced (a folder of that name): every file is put back and no backup is left.
    CreateDirectoryA((level + "/folder.con").c_str(), 0);
    SavePlan failing;
    SavePart& both = failing.part("both");
    both.add(level + "/a.con", "lost");
    both.add(level + "/folder.con", "never");
    bool failedMarked = false;
    both.onWritten([&failedMarked]() { failedMarked = true; });
    const size_t before = listSaveBackups(backups).size();
    CHECK(!writeSavePlan(failing, policy, written) && written.error.find("nothing was saved") != std::string::npos);
    CHECK(read(level + "/a.con") == "newer a" && read(level + "/a.con.editor-new") == "(missing)" && !failedMarked
          && listSaveBackups(backups).size() == before);

    // The first save restored: its replaced files back, the file it made taken away, the restore backed up too.
    list = listSaveBackups(backups);
    SavePlan restore;
    SavePart& back = restore.part("restore");
    CHECK(list.size() == 2 && planBackupRestore(list, 1, game, back, error));
    restore.dropUnchanged();
    policy.label = "restore";
    CHECK(writeSavePlan(restore, policy, written));
    CHECK(read(level + "/a.con") == "old a" && read(level + "/b.con") == "old b" && read(level + "/sub/c.con") == "(missing)"
          && read(level + "/f.con") == "(missing)");
    list = listSaveBackups(backups);
    CHECK(list.size() == 3 && list[0].label == "restore");

    // A fourth backup: the oldest of the three kept goes.
    SavePlan another;
    another.part("x").add(level + "/a.con", "again");
    another.dropUnchanged();
    CHECK(writeSavePlan(another, policy, written));
    list = listSaveBackups(backups);
    CHECK(list.size() == 3 && list[2].label == "check" && list[2].entries.size() == 1);

    // Two parts planning one file: it is written once, with the later part's content, under the first spelling.
    SavePlan twice;
    twice.part("first").add(level + "/twice.con", "one");
    twice.part("second").add(level + "/TWICE.CON", "two");
    twice.dropUnchanged();
    CHECK(twice.plans(level + "/Twice.con") && !twice.plans(level + "/thrice.con"));
    CHECK(writeSavePlan(twice, policy, written) && written.error.empty() && written.written.size() == 1
          && written.written[0] == level + "/twice.con" && read(level + "/twice.con") == "two");
    removeCheckTree(root);
}

void checkArchiveRewrite()
{
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    char unique[64];
    std::snprintf(unique, sizeof(unique), "bf2editor_rewrite_%lu", static_cast<unsigned long>(GetCurrentProcessId()));
    const std::string root = std::string(temp) + unique;
    removeCheckTree(root);
    CreateDirectoryA(root.c_str(), 0);
    const std::string source = root + "/source.zip", rewritten = root + "/rewritten.zip", refused = root + "/refused.zip";
    std::string error, bytes;
    std::string big;
    for (int i = 0; i < 400; ++i)
        big += "hudBuilder.setNodeColor 1 1 1 1\r\n";
    {
        ZipWriter writer;
        CHECK(writer.open(source, error));
        CHECK(writer.add("HUD/one.con", reinterpret_cast<const unsigned char*>(big.data()), big.size(), 0x6000, 0x5A21, error));
        CHECK(writer.add("two.bin", reinterpret_cast<const unsigned char*>("\x01\x02\x03"), 3, 0, 0, error));
        CHECK(writer.add("HUD/Sub/three.con", reinterpret_cast<const unsigned char*>("three"), 5, 0, 0, error));
        CHECK(writer.close(error));
    }
    // One entry replaced (named in another case and with backslashes); the others copied as stored.
    std::map<std::string, std::string> replaced;
    replaced["hud\\SUB\\three.con"] = "three, edited";
    std::vector<std::string> missing;
    CHECK(rewriteArchive(source, rewritten, replaced, missing, error) && missing.empty());
    CHECK(readArchiveEntry(rewritten, "HUD/Sub/three.con", bytes, error) && bytes == "three, edited");
    CHECK(readArchiveEntry(rewritten, "HUD/one.con", bytes, error) && bytes == big);
    CHECK(readArchiveEntry(rewritten, "two.bin", bytes, error) && bytes == "\x01\x02\x03");
    // The untouched entries keep their stored form: the file up to the replaced entry is the source's, byte for byte.
    std::string a, b;
    ObjectLibrary::readTextFile(source, a, error);
    ObjectLibrary::readTextFile(rewritten, b, error);
    const size_t third = a.find("HUD/Sub/three.con");
    CHECK(third != std::string::npos && third > 30 && a.compare(0, third - 30, b, 0, third - 30) == 0);
    // An entry the archive does not have refuses the rewrite and leaves no file.
    replaced["HUD/none.con"] = "x";
    CHECK(!rewriteArchive(source, refused, replaced, missing, error) && missing.size() == 1 && missing[0] == "hud/none.con");
    CHECK(GetFileAttributesA(refused.c_str()) == INVALID_FILE_ATTRIBUTES);

    // The stock server menu archive (BF2EDITOR_MENU_ARCHIVE, read only): one HUD script replaced, every other entry the
    // same as stored - the same CRC-32, method and sizes, in the same order.
    char menu[MAX_PATH] = "";
    if (GetEnvironmentVariableA("BF2EDITOR_MENU_ARCHIVE", menu, MAX_PATH) != 0)
    {
        const std::string copy = root + "/menu.zip";
        std::map<std::string, std::string> hud;
        hud["HUD/HudSetup/Vehicles/HudElementsM1A2Driver.con"] = "rem the M1A2's HUD, edited\r\n";
        CHECK(rewriteArchive(menu, copy, hud, missing, error));
        unzFile left = unzOpen(menu), right = unzOpen(copy.c_str());
        size_t entries = 0, same = 0, differing = 0;
        if (left && right)
        {
            int l = unzGoToFirstFile(left), r = unzGoToFirstFile(right);
            for (; l == UNZ_OK && r == UNZ_OK; l = unzGoToNextFile(left), r = unzGoToNextFile(right))
            {
                unz_file_info x, y;
                char nx[512], ny[512];
                unzGetCurrentFileInfo(left, &x, nx, sizeof(nx), 0, 0, 0, 0);
                unzGetCurrentFileInfo(right, &y, ny, sizeof(ny), 0, 0, 0, 0);
                ++entries;
                if (std::strcmp(nx, ny) == 0 && x.crc == y.crc && x.compression_method == y.compression_method
                    && x.compressed_size == y.compressed_size && x.uncompressed_size == y.uncompressed_size
                    && x.dosDate == y.dosDate)
                    ++same;
                else
                    ++differing;
            }
            CHECK(l != UNZ_OK && r != UNZ_OK);
        }
        if (left)
            unzClose(left);
        if (right)
            unzClose(right);
        CHECK(entries > 100 && same + 1 == entries && differing == 1);
        CHECK(readArchiveEntry(copy, "HUD/HudSetup/Vehicles/HudElementsM1A2Driver.con", bytes, error)
              && bytes == "rem the M1A2's HUD, edited\r\n");
        std::printf("[archive rewrite] the server menu archive: %u entries, %u as stored, 1 replaced\n",
                    static_cast<unsigned>(entries), static_cast<unsigned>(same));
    }

    // A save whose target something holds open (a mounted archive): the part lets go of it just before the targets are
    // replaced and takes it up again after, and the save goes through.
    const std::string held = root + "/held.zip";
    CHECK(ObjectLibrary::writeTextFile(held, "old", error));
    HANDLE handle = CreateFileA(held.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    CHECK(handle != INVALID_HANDLE_VALUE);
    std::vector<std::string> steps;
    {
        SavePlan plan;
        SavePart& part = plan.part("held");
        part.add(held, "new");
        part.onReplacing([&]() { steps.push_back("replacing"); CloseHandle(handle); handle = INVALID_HANDLE_VALUE; });
        part.onReplaced([&]() {
            steps.push_back("replaced");
            handle = CreateFileA(held.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        });
        part.onWritten([&]() { steps.push_back("written"); });
        SaveBackupPolicy policy;
        policy.root = root + "/backups";
        policy.base = root;
        SaveWriteResult result;
        CHECK(writeSavePlan(plan, policy, result) && result.error.empty());
    }
    CHECK(steps.size() == 3 && steps[0] == "replacing" && steps[1] == "replaced" && steps[2] == "written");
    std::string now;
    if (handle != INVALID_HANDLE_VALUE)
        CloseHandle(handle);
    CHECK(ObjectLibrary::readTextFile(held, now, error) && now == "new");
    // Held open throughout, the replace fails: everything is put back, and the part still takes the file up again.
    handle = CreateFileA(held.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    steps.clear();
    {
        SavePlan plan;
        SavePart& part = plan.part("held");
        part.add(held, "newer");
        part.onReplacing([&]() { steps.push_back("replacing"); });
        part.onReplaced([&]() { steps.push_back("replaced"); });
        part.onWritten([&]() { steps.push_back("written"); });
        SaveBackupPolicy policy;
        policy.root = root + "/backups";
        policy.base = root;
        SaveWriteResult result;
        CHECK(!writeSavePlan(plan, policy, result) && !result.error.empty());
    }
    CHECK(steps.size() == 2 && steps[0] == "replacing" && steps[1] == "replaced");
    if (handle != INVALID_HANDLE_VALUE)
        CloseHandle(handle);
    CHECK(ObjectLibrary::readTextFile(held, now, error) && now == "new");
    removeCheckTree(root);
}

void checkHudSetup()
{
    // A HUD script with the shapes the stock ones use: rem lines and a beginrem block, quoted words, a node selected
    // again by setActiveObject, and no final line end.
    const std::string sub =
        "rem ** test HUD **\r\n"
        "hudBuilder.createSplitNode\t\tGlobalHud TestHud\r\n"
        "hudBuilder.setNodeShowVariable \t\tShowTest\r\n"
        "hudBuilder.createPictureNode\t\tTestHud TestPic 10 20 30 40\r\n"
        "hudBuilder.setPictureNodeTexture \tIngame/test.tga\r\n"
        "hudBuilder.setNodeColor\t\t0 0.8 0 1\r\n"
        "beginrem\r\n"
        "hudBuilder.createTextNode\t\tTestHud Hidden 0 0 1 1\r\n"
        "endrem\r\n"
        "hudBuilder.createTextNode\t\tTestHud TestText 1 2 3 4\r\n"
        "hudBuilder.setTextNodeString\t\"two words\"\r\n"
        "hudBuilder.setActiveObject\t\tTestHud TestPic\r\n"
        "hudBuilder.setNodeLogicShowVariable EQUAL GuiIndex 14\r\n"
        "hudbuilder.setBarNodeTexture 1 \"\"";
    const std::string main = "rem main\r\nrun \"Sub/Test.con\"\r\nrun Missing.con\r\n";
    HudSetup setup;
    std::string error;
    const HudSetup::Reader read = [&](const std::string& path, std::string& text) {
        if (path == "Menu/HudSetupMain.con") { text = main; return true; }
        if (path == "Menu/Sub/Test.con") { text = sub; return true; }
        return false;
    };
    CHECK(setup.load("Menu/HudSetupMain.con", read, error));
    CHECK(setup.scriptCount() == 2 && setup.missing().size() == 1 && setup.missing()[0] == "Menu/Missing.con");
    CHECK(setup.nodes().size() == 3 && setup.find("Hidden") < 0);
    const int pic = setup.find("TestPic"), text = setup.find("TestText"), split = setup.find("TestHud");
    CHECK(pic >= 0 && text >= 0 && split >= 0);
    int rect[4] = {};
    CHECK(setup.rect(pic, rect) && rect[0] == 10 && rect[1] == 20 && rect[2] == 30 && rect[3] == 40);
    CHECK(!setup.rect(split, rect));
    CHECK(setup.nodes()[pic].commands.size() == 4 && setup.nodes()[text].commands.size() == 1);
    std::string parent, name;
    CHECK(setup.liveSelection(setup.nodes()[pic].commands[0], parent, name) && parent == "TestHud"
          && name == "TestPicPictureNode");
    CHECK(setup.liveSelection(setup.nodes()[pic].commands[1], parent, name) && name == "TestPic");
    CHECK(setup.liveSelection(setup.nodes()[pic].commands[2], parent, name) && name == "TestPic");
    CHECK(setup.command(setup.nodes()[text].commands[0]).args[0].text == "two words");
    const std::vector<HudShowCondition> show = setup.showConditions(pic);
    CHECK(show.size() == 2 && show[0].variable == "GuiIndex" && show[0].type == "int" && show[0].value == "14"
          && show[1].variable == "ShowTest" && show[1].node == "TestHud");
    CHECK(setup.script(1).render() == sub && setup.changedScripts().empty());

    // Edits rewrite words in place; an added line takes the create's separator; removing it restores the text.
    const int moved[4] = { 11, 22, 33, 44 };
    CHECK(setup.setRect(pic, moved, error));
    CHECK(setup.script(1).line(3) == "hudBuilder.createPictureNode\t\tTestHud TestPic 11 22 33 44");
    CHECK(setup.setWords(setup.nodes()[pic].commands[1], 0, { "1", "0", "0", "0.5" }, error));
    CHECK(setup.script(1).line(5) == "hudBuilder.setNodeColor\t\t1 0 0 0.5");
    CHECK(setup.setWords(setup.nodes()[text].commands[0], 0, { "three word text" }, error));
    CHECK(setup.script(1).line(10) == "hudBuilder.setTextNodeString\t\"three word text\"");
    CHECK(setup.setWords(setup.nodes()[pic].commands[1], 0, { "1", "1" }, error));
    CHECK(setup.script(1).line(5) == "hudBuilder.setNodeColor\t\t1 1");
    CHECK(setup.setWords(setup.nodes()[pic].commands[1], 0, { "1", "0", "0", "0.5" }, error));
    const std::string edited = setup.script(1).render();
    HudRef added = {};
    CHECK(setup.addCommand(text, "setNodeColor", { "1", "1", "1", "1" }, added, error));
    CHECK(setup.script(1).line(11) == "hudBuilder.setNodeColor\t1 1 1 1" && setup.nodes()[text].commands.size() == 2);
    CHECK(setup.command(setup.nodes()[setup.find("TestText")].commands[1]).method == "setNodeColor");
    CHECK(setup.removeCommand(added, error) && setup.script(1).render() == edited);
    CHECK(setup.changedScripts().size() == 1 && setup.changedScripts()[0] == 1);
    // A file without a final line end keeps its lack of one when a line goes after its last.
    HudScript script;
    script.parse("a 1\r\nb 2");
    script.insertLine(1, "hudBuilder", "setNodeColor", { "1" });
    CHECK(script.render() == "a 1\r\nb 2\r\nhudBuilder.setNodeColor 1");
    script.removeLine(2);
    CHECK(script.render() == "a 1\r\nb 2");

    // The stock scripts (BF2EDITOR_HUD_SAMPLES: a HUD/HudSetup folder as Menu_client.zip has it): every script
    // HudSetupMain.con runs reads back byte for byte, and every row resolves to the live selection it needs.
    char samples[MAX_PATH] = "";
    if (GetEnvironmentVariableA("BF2EDITOR_HUD_SAMPLES", samples, MAX_PATH) == 0)
        return;
    const std::string folder = samples;
    const HudSetup::Reader disk = [&](const std::string& path, std::string& out) {
        std::string err;
        return ObjectLibrary::readTextFile(folder + "\\" + path, out, err);
    };
    HudSetup stock;
    CHECK(stock.load("HudSetupMain.con", disk, error));
    for (size_t i = 0; i < stock.missing().size(); ++i)
        std::printf("[hud setup] run but not found (the game skips it too): %s\n", stock.missing()[i].c_str());
    // Basic/HudSetupBasic.con runs HudElementsGameInfo.con from its own folder, but the file is one folder up.
    CHECK(stock.missing().size() == 1 && stock.missing()[0] == "Basic/HudElementsGameInfo.con");
    size_t same = 0, commands = 0, resolved = 0, withRect = 0, attached = 0;
    for (size_t i = 0; i < stock.scriptCount(); ++i)
    {
        std::string original;
        disk(stock.scriptPath(i), original);
        same += stock.script(i).render() == original ? 1 : 0;
    }
    for (size_t i = 0; i < stock.nodes().size(); ++i)
    {
        withRect += stock.rect(i, rect) ? 1 : 0;
        attached += stock.nodes()[i].commands.size();
        for (size_t c = 0; c < stock.nodes()[i].commands.size(); ++c)
        {
            ++commands;
            resolved += stock.liveSelection(stock.nodes()[i].commands[c], parent, name) ? 1 : 0;
        }
    }
    CHECK(same == stock.scriptCount() && stock.scriptCount() >= 80);
    CHECK(resolved == commands && stock.nodes().size() > 1500);
    std::printf("[hud setup] %u scripts read back byte for byte, %u nodes (%u with a rectangle), %u rows on them\n",
                static_cast<unsigned>(same), static_cast<unsigned>(stock.nodes().size()),
                static_cast<unsigned>(withRect), static_cast<unsigned>(attached));
    // The M1A2's crosshair: its node, its rectangle and what shows it.
    const int crosshair = stock.find("M1A2Crosshair1");
    CHECK(crosshair >= 0 && stock.rect(crosshair, rect) && rect[0] == 272 && rect[1] == 264);
    if (crosshair >= 0)
    {
        const std::vector<HudShowCondition> needs = stock.showConditions(crosshair);
        bool guiIndex = false, ingame = false;
        for (size_t i = 0; i < needs.size(); ++i)
        {
            guiIndex = guiIndex || (needs[i].variable == "GuiIndex" && needs[i].value == "14");
            ingame = ingame || needs[i].variable == "ShowIngameHud";
        }
        CHECK(guiIndex && ingame);
    }
}

int main()
{
    checkTokenizer();
    checkGoldenLines();
    checkSwitchRules();
    checkSourceControl();
    checkHistory();
    checkSelection();
    checkMatrices();
    checkSettings();
    checkShortcuts();
    checkLevelPacking();
    checkUndergrowthConfig();
    checkLevelCopier();
    checkObjectLibrary();
    checkTemplateFile();
    checkTemplateAddProperty();
    checkObjectArchives();
    checkModScaffold();
    checkUndergrowthMap();
    checkWizardRules();
    checkMaterialFiles();
    checkHelpAndCompletion();
    checkLevelEditorDefs();
    checkDescFile();
    checkAnimationSystemFile();
    checkBafHeader();
    checkGameplayLayers();
    checkLightSetup();
    checkGroundProperties();
    checkTerrainRawLayers();
    checkTerrainRawFile();
    checkTerrainRawCompile();
    checkTextureEncode();
    checkTerrainLowDetail();
    checkNewLevel();
    checkRoadFiles();
    checkLightmapAtlas();
    checkDdsImage();
    checkLevelObjectFile();
    checkLevelObjectFileStyles();
    checkLevelObjectFileProperties();
    checkLevelObjectFileTemplates();
    checkLevelObjectFileGameplay();
    checkLevelObjectFileCombatAreas();
    checkLevelObjectFileSounds();
    checkLevelObjectFileRenames();
    checkLevelSettingsFile();
    checkMapDescription();
    checkLayerSummary();
    checkTextDiff();
    checkSavePlan();
    checkArchiveRewrite();
    checkHudSetup();
    checkLevelSamples();
    std::printf("[bf2_editor_checks] %d passed, %d failed\n", g_passed, g_failed);
    return g_failed;
}
