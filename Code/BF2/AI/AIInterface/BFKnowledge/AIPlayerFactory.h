// Creates AI player wrappers.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_AIPLAYERFACTORY_H
#define AI_AIINTERFACE_BFKNOWLEDGE_AIPLAYERFACTORY_H

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

class IAIObject;

// bf2 0x842B700, the first function of AIPlayerFactory.cpp's text (exported, `T`): whether any
// unit of the object's primary object or of its secondary objects other than `exclude` is
// occupied. AIEventListener's vehicle-exit arm uses it.
bool isOccupiedObject(IAIObject* object, IAIObject* exclude);

class IAIPlayerFactory
{
public:
    IAIPlayerFactory();
    virtual ~IAIPlayerFactory();

    virtual IAIPlayer* create() = 0;
    virtual IAIPlayer* create(int id) = 0;
    virtual IAIPlayer* create(const std::string& name) = 0;
    virtual IAIPlayer* create(world::IPlayer* player) = 0;
    virtual void destroy(IAIPlayer* player) = 0;
    virtual int getPlayerIdFromName(const std::string& name) = 0;
    virtual IAIPlayer* getPlayerFromId(int id) = 0;
    virtual void setIdToPlayerMapping(IAIPlayer* player) = 0;
    virtual void removeIdToPlayerMapping(IAIPlayer* player) = 0;
    virtual const std::vector<IAIPlayer*>& getAllPlayers() = 0;
    virtual IAIPlayer* getPlayerFromLocalHumanPlayer() = 0;
};

class AIPlayerFactory : public IAIPlayerFactory
{
public:
    AIPlayerFactory();
    virtual ~AIPlayerFactory();

    virtual IAIPlayer* create();
    virtual IAIPlayer* create(int id);
    virtual IAIPlayer* create(const std::string& name);
    virtual IAIPlayer* create(world::IPlayer* player);
    virtual void destroy(IAIPlayer* player);
    virtual int getPlayerIdFromName(const std::string& name);
    virtual IAIPlayer* getPlayerFromId(int id);
    virtual void setIdToPlayerMapping(IAIPlayer* player);
    virtual void removeIdToPlayerMapping(IAIPlayer* player);
    virtual const std::vector<IAIPlayer*>& getAllPlayers();
    virtual IAIPlayer* getPlayerFromLocalHumanPlayer();

private:
    /* +0x00 */ // vptr
    // the constructor resizes it to 256 nulls. 256 is the player-id space, so this is an
    // id-indexed table, not a list: setIdToPlayerMapping writes m_players[id].
    /* +0x04 */ std::vector<IAIPlayer*> m_players;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_AIPLAYERFACTORY_H
