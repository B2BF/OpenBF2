// Editor soldier play-test using the 1.50 local player, kit, camera and 1/30-second client update order.
// The temporary soldier/kit stay outside the level document and are destroyed on exit.
// Enter/exit uses the server's entry-point search. Damage and respawning remain disabled without authority.
#ifndef BF2EDITOR_APP_PLAYTEST_H
#define BF2EDITOR_APP_PLAYTEST_H

#include "Framework/EditorConfig.h"
#include "Framework/EditorConsole.h"

#include "Core/WeakPtr.h"

#include <string>
#include <vector>

namespace dice { namespace hfe {
namespace dx9rend { class IRenderView; }
namespace io { class PlayerInput; }
namespace world { class IObject; class IPlayer; }
} }

namespace bf2editor
{

class EditSession;
class EditorGame;
class FreeCamera;

// What the editor's window hands the play-test for one frame.
struct PlayTestInput
{
    bool  keys[256];            // Win32 virtual keys held (only while the view has the keyboard)
    bool  buttons[2];           // the left and right mouse buttons held (only while the mouse is captured)
    float lookX;                // mouse counts since the last frame (only while the mouse is captured)
    float lookY;
    PlayTestInput();
};

class PlayTest : public IPlayTestCommands
{
public:
    PlayTest(EditSession& session, FreeCamera& camera, EditorGame& game);
    ~PlayTest();

    bool active() const { return m_active; }

    // Soldier mode on: the soldier stands where the editor camera is, on the ground below it.
    bool enterSoldier(std::string& result);
    // Soldier mode off: out of every seat, back in the free camera, the soldier and its kit destroyed; the editor camera
    // continues from the player's eyes.
    bool leave(std::string& result);
    // Before the level closes or the editor exits: soldier mode off and the player removed.
    void shutdown();
    // The vehicle test drive: a vehicle made as a play-test object - the selected object
    // spawner's vehicle for the soldier's team (else the other team's), or the template named - where the spawner stands
    // or beside the soldier, and the player's soldier seated in it as the driver. Soldier mode starts first when it is
    // off. The vehicle is destroyed with the soldier when soldier mode ends.
    bool drive(const std::string& templateName, std::string& result);
    // The weapon test (the retail object editor's "enter a weapon": its editor soldier given the weapon): a soldier
    // made with no kit where the editor camera stands, given the weapon template (Soldier::addItem by name) and
    // holding it. Soldier mode starts again when it is on. The fire and ammo components read their template as
    // they go, so a rate of fire or a reload time changed in the template reaches the held weapon at once; a
    // magazine's size takes a new weapon (another test). Refused for a template that is not a GenericFireArm.
    bool testWeapon(const std::string& templateName, std::string& result);

    // The team and kit the next soldier is made from (gameLogic.setKit's numbering: teams 1 and 2, kits 0-6).
    bool setKit(int team, int kit, std::string& result);
    // A scripted input: `name` (an io::PlayerInputMap name, such as PIThrottle) held at `value` for `steps` steps.
    bool hold(const std::string& name, float value, int steps, std::string& result);
    // One line: the player, what it is in, where, the world tick.
    std::string describe() const;

    // Runs the fixed 1/30 s steps that `seconds` of real time owe, each with this frame's input.
    void frame(float seconds, const PlayTestInput& input);
    // The render view from the player's camera, as BF2Render::render points it.
    bool applyView(dice::hfe::dx9rend::IRenderView* view, int width, int height, float farPlane, float frameTime) const;

    // IPlayTestCommands (EditorPlayTest.*).
    bool playTestActive() const override { return m_active; }
    bool enterSoldierMode(std::string& result) override { return enterSoldier(result); }
    bool leaveSoldierMode(std::string& result) override { return leave(result); }
    bool setPlayTestKit(int team, int kit, std::string& result) override { return setKit(team, kit, result); }
    bool holdPlayTestInput(const std::string& name, float value, int steps, std::string& result) override
    {
        return hold(name, value, steps, result);
    }
    bool drivePlayTestVehicle(const std::string& templateName, std::string& result) override
    {
        return drive(templateName, result);
    }
    bool testPlayTestWeapon(const std::string& templateName, std::string& result) override
    {
        return testWeapon(templateName, result);
    }
    std::string describePlayTest() const override { return describe(); }

private:
    PlayTest(const PlayTest&);
    PlayTest& operator=(const PlayTest&);

    struct Held
    {
        int   input;
        float value;
        int   steps;
    };

    bool ensurePlayer(std::string& result);
    dice::hfe::world::IPlayer* player() const;
    dice::hfe::world::IObject* soldier() const;
    bool groundBelow(const float at[3], float& height) const;
    void step(float dt, const PlayTestInput& input);
    void buildInput(const PlayTestInput& source, dice::hfe::io::PlayerInput& out);
    void toggleEntry(bool usePressed);
    void seatAndCameraKeys(const dice::hfe::io::PlayerInput& input);
    void destroyMadeObjects();
    // The stock control map the player's seat takes (EditorGame::enterVehicle's choice): 0 infantry, 1 land or sea,
    // 2 air, 3 helicopter.
    int controlCategory() const;

    EditSession&       m_session;
    FreeCamera&        m_camera;
    EditorGame&        m_game;
    bool               m_active;
    int                m_playerId;
    // The soldier and the kit made for this run, held weakly: the engine may destroy either (a kit is consumed by the
    // pickup), and an object id can be reused.
    dice::hfe::WeakPtr<dice::hfe::world::IObject> m_soldier;
    dice::hfe::WeakPtr<dice::hfe::world::IObject> m_kitObject;
    std::vector<dice::hfe::WeakPtr<dice::hfe::world::IObject> > m_vehicles;   // made by drive()
    bool               m_noKit = false;    // the next soldier takes no kit (a weapon test)
    int                m_team;
    int                m_kit;
    double             m_owed;              // real seconds not yet stepped
    int                m_savedGameStatus;   // the client's game status before soldier mode set it to playing
    unsigned char      m_savedViews[2];     // the server info's allowExternalViews and allowNoseCam before the run
    double             m_lastToggle;        // world time of the last enter or exit (the 1 s cooldown)
    bool               m_previousKeys[256];  // the keys of the last step (the edge-triggered rows)
    unsigned           m_steps;
    std::vector<Held>  m_held;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_PLAYTEST_H
