// Shared engine input, gearing, running-state and sound behavior for Engine and CarEngine.
// [INFERRED] Source placement; neutral field names retain unresolved meanings.

#ifndef GAME_OBJECTS_GAME_ENGINEBASE_H     // [INFERRED] guard style
#define GAME_OBJECTS_GAME_ENGINEBASE_H

#include "RotationalBundle.h"
#include "../../../Core/WeakPtr.h"

namespace dice {
namespace hfe {
class EngineSound;
class EngineNetworkable;                    // the wire's typed field reads
namespace world {

class EngineTemplate;

class EngineBase : public RotationalBundle
{
public:
    explicit EngineBase(const EngineTemplate* objectTemplate);
    virtual ~EngineBase();

    virtual void init();
    virtual void disableCachedObject();
    virtual void enableCachedObject();
    virtual void handleFrameUpdate(float dt);
    virtual void handlePlayerInput(IPlayer*, const io::PlayerInput&, float, unsigned int);
    virtual void handleMessage(TemplateMessage, IPlayer*, bool);
    virtual void initNetworkableComp();

    // the six own slots, +0x1A0..+0x1B4
    virtual void  reset();
    virtual float getGearRatio(int gear) const;
    virtual float getCurrentTorque() const;
    virtual int   getEngineType();                                          // (non-const, as shipped)
    virtual int   getGearChange() const;                                    // 0
    virtual void  setGearChange(int);                                       // nothing
    // (updatePhysics / getCurrentRatio at +0x1B8 / +0x1BC are engine's own two - CarEngine's table
    // ends at +0x1B4 like this one's.)

    // non-virtual
    void  initSound();
    void  updateEngineSound();
    float getEngineIntensity();
    float mapInputToWantedAngle(float, float, float);
    float mapWantedAngleToAngle(float, float, float, float);
    void  accumulate(IPlayer*, Vec3&, float);
    float getCurrentDifferentialRPM(float relativeX) const;
    static void initSoundTemplates(EngineTemplate* objectTemplate);

    // A driven wheel's suspension node reads the gear, the sound input and +0x1DC straight out of
    // its engine every tick (PhysicsSpring::updatePhysics' differential branch) - the same
    // precedent as ResponsePhysics' `friend class PhysicsNode`.
    friend class PhysicsSpring;
    friend class ResponsePhysics;               // addFriction / addFrictionNew: the wheel's drive and
                                                // feedback accumulators (+0x1DC, +0x1E4, +0x1E8, +0x1EC, +0x1F0)
    friend class ::dice::hfe::EngineNetworkable;   // the wire's baseline capture reads the gear /
    // Rotor reads the engine-running state and rotation inputs directly.
    friend class Rotor;
    friend class EffectBundle; // client PE 0x641EB0 / ELF 0x81FB0B0 reads m_engineRunning
    friend class ::dice::hfe::EngineSound; // sway reads m_gear directly: ELF +0x1E8 / PE +0x200
    friend class ObjectSpawner;   // the hold arm reads m_input (retail +0x1C4) directly

protected:
    /* 0x1B0 */ const EngineTemplate* m_template;
    /* 0x1B4 */ bool  m_engineRunning;
    /* 0x1B5 */ bool  m_wrecked;
    /* 0x1B8 */ float m_heat;                    // Engine::handleUpdate's 0..1 heat accumulator
    /* 0x1BC */ float m_accumulatedInput;         // accumulate()'s clamped persistent z input
    /* 0x1C0 */ float m_soundRpm;
    /* 0x1C4 */ float m_input;
    /* 0x1C8 */ float m_float1C8;
    /* 0x1CC */ float m_float1CC;
    /* 0x1D0 */ float m_float1D0;
    /* 0x1D4 */ float m_float1D4;
    /* 0x1D8 */ float m_currentAngle;
    /* 0x1DC */ bool  m_byte1DC;
    /* 0x1E0 */ int   m_dword1E0;
    /* 0x1E4 */ float m_soundInput;
    /* 0x1E8 */ int   m_gear;
    /* 0x1EC */ float m_dword1EC;                // a float: the wheels' rpm / drive sum (ResponsePhysics::addFriction[New]) [UNRESOLVED name]
    /* 0x1F0 */ int   m_dword1F0;                // ... and how many wheels summed into it
    /* 0x1F4 */ float m_float1F4;
    /* 0x1F8 */ EngineSound* m_engineSound;
    /* 0x1FC */ IObject* m_lightSource;
    /* 0x200 */ WeakPtr<IObject> m_driveWheel;
    /* 0x204 */ int   m_dword204;
    /* 0x208 */ bool  m_byte208;
    /* 0x20C */ float m_runningTimer;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_GAME_ENGINEBASE_H
