// Exposes engine squad operations to the AI.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_BFSQUAD_H
#define AI_AIINTERFACE_BFKNOWLEDGE_BFSQUAD_H

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

class IBFSquad
{
public:
    IBFSquad();
    virtual ~IBFSquad();

    virtual int getNextEmptySquad(int team) const = 0;
    virtual bool joinSquad(int squad, IAIPlayer* player) = 0; // ELF/PE return AL; BotSquad tests it
    virtual int exitSquad(int squad, IAIPlayer* player) = 0;
    virtual bool isSquadLeader(int squad, IAIPlayer* player) = 0;
    virtual int assignAsSquadLeader(int squad, IAIPlayer* player) = 0;
    virtual int retireAsSquadLeader(int squad, IAIPlayer* player) = 0;
    virtual int issueSquadAttackOrder(const Vec3& position, int a, int b, IAIObject* target) = 0;
    virtual int issueSquadMoveOrder(const Vec3& position, int a, int b, IAIObject* target) = 0;
    virtual int issueSquadDefendOrder(const Vec3& position, int a, int b, IAIObject* target) = 0;
    virtual void acceptSquadOrder(int squad, IAIPlayer* player) = 0;
    virtual void declineSquadOrder(int squad, IAIPlayer* player) = 0;
    virtual int acceptCommanderAttackOrder(int squad, IAIPlayer* player) = 0;
    virtual int acceptCommanderDefendOrder(int squad, IAIPlayer* player) = 0;
    virtual int acceptCommanderMoveOrder(int squad, IAIPlayer* player) = 0;
    virtual int declineCommanderAttackOrder(int squad, IAIPlayer* player) = 0;
    virtual int declineCommanderDefendOrder(int squad, IAIPlayer* player) = 0;
    virtual int declineCommanderMoveOrder(int squad, IAIPlayer* player) = 0;
    virtual int requestNewCommanderOrder(int squad, IAIPlayer* player) = 0;
    virtual world::IObject* getCommanderOrder(int a, int b, Vec3& position, IAIObject* target) = 0;
    virtual world::IObject* getSquadOrder(int a, int b, Vec3& position, IAIObject* target) = 0;
};

class BFSquad : public IBFSquad
{
public:
    BFSquad();
    virtual ~BFSquad();

    virtual int getNextEmptySquad(int team) const;
    virtual bool joinSquad(int squad, IAIPlayer* player);
    virtual int exitSquad(int squad, IAIPlayer* player);
    virtual bool isSquadLeader(int squad, IAIPlayer* player);
    virtual int assignAsSquadLeader(int squad, IAIPlayer* player);
    virtual int retireAsSquadLeader(int squad, IAIPlayer* player);
    virtual int issueSquadAttackOrder(const Vec3& position, int a, int b, IAIObject* target);
    virtual int issueSquadMoveOrder(const Vec3& position, int a, int b, IAIObject* target);
    virtual int issueSquadDefendOrder(const Vec3& position, int a, int b, IAIObject* target);
    virtual void acceptSquadOrder(int squad, IAIPlayer* player);
    virtual void declineSquadOrder(int squad, IAIPlayer* player);
    virtual int acceptCommanderAttackOrder(int squad, IAIPlayer* player);
    virtual int acceptCommanderDefendOrder(int squad, IAIPlayer* player);
    virtual int acceptCommanderMoveOrder(int squad, IAIPlayer* player);
    virtual int declineCommanderAttackOrder(int squad, IAIPlayer* player);
    virtual int declineCommanderDefendOrder(int squad, IAIPlayer* player);
    virtual int declineCommanderMoveOrder(int squad, IAIPlayer* player);
    virtual int requestNewCommanderOrder(int squad, IAIPlayer* player);
    virtual world::IObject* getCommanderOrder(int a, int b, Vec3& position, IAIObject* target);
    virtual world::IObject* getSquadOrder(int a, int b, Vec3& position, IAIObject* target);

private:
    // Non-virtual ELF helpers (0x8410BC0, 0x8410D20, 0x8410E70); the int is a SquadOrderType value.
    int issueOrder(const Vec3& position, int a, int b, IAIObject* target, int orderType);
    int acceptCommanderOrder(int squad, IAIPlayer* player, int orderType);
    int declineCommanderOrder(int squad, IAIPlayer* player, int orderType);
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_BFSQUAD_H
