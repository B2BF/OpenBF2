// Loads a level through ClientGameLogic after publishing EditorGame and setting level options.
// Preserves the event order: ECCore 2, load, ECCore 3, ECCore 7.
// Client conversion removes dynamic objects and skips gameplay host blocks.
#ifndef BF2EDITOR_APP_EDITORLEVEL_H
#define BF2EDITOR_APP_EDITORLEVEL_H

#include "Framework/EditorConfig.h"

#include <string>
#include <vector>

namespace bf2editor
{

class EditorGame;

class EditorLevel
{
public:
    EditorLevel();
    ~EditorLevel();

    // gameMode / maxPlayers pick the gameplay-object layer (GameModes/<mode>/<size>/). As the retail
    // editor's load (Init.con's editor branch) has it, the overgrowth is put into editor mode before the
    // post-load event, and the objects the level's Overgrowth/OvergrowthCollision.con made are removed.
    bool open(const std::string& name, const std::string& gameMode, int maxPlayers);
    void close();

    bool isOpen() const { return m_open; }
    const std::string& name() const { return m_name; }

    // The terrain height (world units) under (x, z), from the level's primary heightmap; false with
    // no level or no heightmap.
    bool groundHeight(float x, float z, float& height) const;
    // The primary heightmap's extent in the world: min X, min Z, max X, max Z; false without one.
    bool worldBounds(float bounds[4]) const;

    // The editor's view of the level's gameplay, after open(): the objects the
    // client load leaves out are made too, as the retail editor's load makes them - the lines inside the
    // loaded layer's `if v_arg1 == host` blocks (object spawners, control points) and inside
    // AmbientObjects.con's `if v_arg1 == BF2Editor` block (the ambient sounds). They stay inert: object
    // spawners and control points act only on a server (their handleUpdate asks for IGameServer, which
    // the editor's game object does not answer). The ambient sounds make no world object at all in this
    // engine (a Sound template's createObject returns none, as in the 1.50 game), so their records stay
    // as written. A block with conditionals of its own inside is not run (it is logged). `made` counts
    // the objects the blocks made.
    bool loadEditorContent(int& made);
    // Object lines run through the engine's console: the
    // objects made; the first line that answered with text in `firstComplaint`.
    int runObjectLines(const std::vector<std::string>& lines, std::string& firstComplaint);
    const std::string& gameMode() const { return m_gameMode; }
    int maxPlayers() const { return m_maxPlayers; }
    // The gameplay-object layer the load ran (GameModes/<mode>/<size>/GamePlayObjects.con): GameLogic::load
    // takes the first layer at or above maxPlayers, or the largest when maxPlayers is above them all, and
    // publishes it with Game::setCurrentMapSize.
    int layerSize() const { return m_layerSize; }

    // Tears down the editor's game objects (after close). Called by EngineHost's owner before the
    // engine shuts down.
    void release();
    // The editor's game object (the engine's `game`), once a level was opened.
    EditorGame* editorGame() const { return m_game; }

private:
    EditorLevel(const EditorLevel&);
    EditorLevel& operator=(const EditorLevel&);

    bool ensureGameObjects();
    bool replaceGameLogic();

    EditorGame* m_game;
    bool        m_open;
    std::string m_name;
    std::string m_gameMode;
    int         m_maxPlayers;
    int         m_layerSize;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_EDITORLEVEL_H
