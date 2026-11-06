// AI view of an engine player, implemented by AIPlayer in the executable.
// [INFERRED] Return types are derived from implementations; preserve virtual declaration order.
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_AIPLAYER_H      // [INFERRED] guard style
#define AI_AIINTERFACE_BFKNOWLEDGE_AIPLAYER_H

#include <string>

namespace dice {
namespace hfe {

namespace world { class IPlayer; }

struct Mat4;

namespace ai {

class IAIObject;
class IWeapon;

class IAIPlayer
{
public:
    virtual ~IAIPlayer();                                           // +0x00/+0x04,

    virtual void reset() = 0;                                       // +0x08
    virtual world::IPlayer* getPlayer() const = 0;                  // +0x0C
    virtual void setPlayer(int playerId) = 0;                       // +0x10
    virtual void setPlayer(world::IPlayer* player) = 0;             // +0x14
    virtual const std::string& getName() const = 0;                 // +0x18
    virtual int getId() const = 0;                                  // +0x1C
    virtual int getInputId() const = 0;                             // +0x20
    virtual bool getIsAlive() const = 0;                            // +0x24
    virtual void setIsAlive(bool alive) = 0;                        // +0x28
    virtual bool isManDown() const = 0;                             // +0x2C
    virtual bool isClimbingLadder() const = 0;                      // +0x30
    virtual bool isParachuting() const = 0;                         // +0x34
    virtual void setSpawnGroup(int group) = 0;                      // +0x38
    virtual void setSpawnKit(int kit) = 0;                          // +0x3C
    virtual int getSpawnGroup() const = 0;                          // +0x40
    virtual int getSpawnKit() const = 0;                            // +0x44
    virtual unsigned int getPlayerControlObjectId() = 0;             // +0x48
    virtual IAIObject* getSoldier() = 0;                            // +0x4C
    virtual IAIObject* getDefaultVehicle() = 0;                     // +0x50
    virtual IAIObject* getCurrentVehicle() = 0;                     // +0x54
    // Returns the wrapped player's field of view as a float.
    virtual float getFieldOfView() = 0;                             // +0x58
    virtual float getFieldOfViewModifier() = 0;                     // +0x5C
    virtual float getStandardFieldOfView() = 0;                     // +0x60
    virtual float getViewDistance() = 0;                            // +0x64
    // the one this tree calls. `AISettings::setViewDistance` reaches it once per live bot.
    virtual void setViewDistance(float distance) = 0;               // +0x68
    virtual int getSide() = 0;                                      // +0x6C
    virtual void setSide(int side) = 0;                             // +0x70
    virtual int getNumShotsFired(IWeapon* weapon) = 0;              // +0x74
    virtual void removePlayerFromGame() = 0;                        // +0x78
    virtual void setName(const std::string& name) = 0;              // +0x7C
    virtual int getScore() = 0;                                     // +0x80
    virtual void setIsAIPlayer(bool isAI) = 0;                      // +0x84
    virtual bool getIsAIPlayer() = 0;                               // +0x88
    virtual const void* getCameraTransformation() const = 0;        // +0x8C [INFERRED] a Mat4
    virtual const void* getPlayerInput() const = 0;                 // +0x90 [INFERRED]
};

// Forwards player state and actions to a borrowed world::IPlayer.
// The wrapped player must outlive this object.
class AIPlayer : public IAIPlayer
{
public:
    explicit AIPlayer(world::IPlayer* player);
    AIPlayer();
    virtual ~AIPlayer();

    virtual void reset();
    virtual world::IPlayer* getPlayer() const;
    virtual void setPlayer(int playerId);
    virtual void setPlayer(world::IPlayer* player);
    virtual const std::string& getName() const;
    virtual int getId() const;
    virtual int getInputId() const;
    virtual bool getIsAlive() const;
    virtual void setIsAlive(bool alive);
    virtual bool isManDown() const;
    virtual bool isClimbingLadder() const;
    virtual bool isParachuting() const;
    virtual void setSpawnGroup(int group);
    virtual void setSpawnKit(int kit);
    virtual int getSpawnGroup() const;
    virtual int getSpawnKit() const;                                // forwards getKit
    virtual unsigned int getPlayerControlObjectId();
    virtual IAIObject* getSoldier();
    virtual IAIObject* getDefaultVehicle();
    virtual IAIObject* getCurrentVehicle();
    virtual float getFieldOfView();
    virtual float getFieldOfViewModifier();
    virtual float getStandardFieldOfView();
    virtual float getViewDistance();
    virtual void setViewDistance(float distance);
    virtual int getSide();
    virtual void setSide(int side);
    virtual int getNumShotsFired(IWeapon* weapon);
    virtual void removePlayerFromGame();                            // bf2 0x83FC3D0
    virtual void setName(const std::string& name);
    virtual int getScore();
    virtual void setIsAIPlayer(bool isAI);
    virtual bool getIsAIPlayer();
    virtual const void* getCameraTransformation() const;
    virtual const void* getPlayerInput() const;

    // ds: - one `Mat4`, allocated by whichever `AIPlayer` is constructed first and never
    // freed. See the class note. [INFERRED] name; the lazy-init shape and the identity fill are read.
    static Mat4* identityMatrix;

private:
    // layout, (`operator new(0x10)`), read store by store out of the constructor
    /*  +0 */ // vptr
    /*  +4 */ world::IPlayer* m_player;      // the constructor's argument; every forward's receiver
    /*  +8 */ int m_i8;                      // = 0, and `reset` sets it back to 0. [UNRESOLVED]
    /* +12 */ float m_viewDistance;          // seeded from IPlayer::getViewDistance
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_AIPLAYER_H
