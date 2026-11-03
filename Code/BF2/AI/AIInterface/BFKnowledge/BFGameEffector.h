// Applies AI actions to engine players and objects.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_BFGAMEEFFECTOR_H
#define AI_AIINTERFACE_BFKNOWLEDGE_BFGAMEEFFECTOR_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "AIAdapterTypes.h"

namespace dice {
namespace hfe {

class IBase;

namespace ai {

class IAIGameEffector
{
public:
    IAIGameEffector();
    virtual ~IAIGameEffector();

    virtual int enterVehicle(IAIPlayer* player, IAIObject* vehicle) = 0;
    virtual int exitVehicle(IAIPlayer* player) = 0;
    virtual int terminatePlayer(int playerId) = 0;
    virtual int terminatePlayer(IAIPlayer* player) = 0;
    virtual int addBot(int& a, int& b, std::string& name) = 0;
    virtual int respawnBot(IAIPlayer* player) = 0;
    virtual void forceSpawnBot(IAIPlayer* player, const Vec3& position, const Vec3& rotation) = 0;
    virtual int giveDamage(IAIObject* target, world::IPlayer* from, int type, float amount) = 0;
    virtual void playEffect(const Vec3& position, int a, int b) = 0;
    virtual void playSoundEffects(double dt) = 0;
    virtual int suicide(IAIPlayer* player) = 0;
};

class BFGameEffector : public IAIGameEffector
{
public:
    BFGameEffector();
    virtual ~BFGameEffector();

    virtual int enterVehicle(IAIPlayer* player, IAIObject* vehicle);
    virtual int exitVehicle(IAIPlayer* player);
    virtual int terminatePlayer(int playerId);
    virtual int terminatePlayer(IAIPlayer* player);
    virtual int addBot(int& a, int& b, std::string& name);
    virtual int respawnBot(IAIPlayer* player);
    virtual void forceSpawnBot(IAIPlayer* player, const Vec3& position, const Vec3& rotation);
    virtual int giveDamage(IAIObject* target, world::IPlayer* from, int type, float amount);
    virtual void playEffect(const Vec3& position, int a, int b);
    virtual void playSoundEffects(double dt);
    virtual int suicide(IAIPlayer* player);

private:
    void queueSoundEffect(const Vec3& position, int a, int b);

    // [INFERRED] Element name. Both ELF destructors iterate in 20-byte steps.
    // [UNRESOLVED] Entry fields: the ELF queue/play methods are empty, so no writer exists.
    struct SoundEffect
    {
        unsigned char unread[20];
    };

    /* +0x00 */ // vptr
    /* +0x04 */ std::vector<SoundEffect> m_soundEffects; // [INFERRED] name; native STL layout
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_BFGAMEEFFECTOR_H
