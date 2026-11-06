// Per-bot view of the engine squad. Squad membership is maintained by the engine adapter.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOT_BOTSQUAD_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_BOTSQUAD_H

#include "../../../../Math/Vec3.h"     // Vec3 is a typedef - never forward-declare it

namespace dice {
namespace hfe {
namespace ai {

class BotMain;
class IAIObject;
class Information;

class BotSquad
{
public:
    // attested as a nested type: `SquadCoordinator::isSameMission(BotSquad::OrderType, ...)` and
    // `SquadManager::setSquadReceivedOrder(BotSquad::OrderType, int, bool)` both name it.
    enum OrderType
    {
        OrderAttack = 0,
        OrderDefend = 1,
        OrderMove   = 2,
        // 3 is [UNRESOLVED] - see the header block
        OrderNone   = 4
    };

    explicit BotSquad(BotMain* owner);
    ~BotSquad();                             // - empty, and that is a finding
    void reset();                            // - empty, see the header

    // the four order predicates, one `cmp` each.
    bool hasAttackOrder();
    bool hasDefendOrder();
    bool hasMoveOrderOrder();                // dice's name
    bool hasNoOrder();

    // membership. joinSquad written; remaining methods are [FRONTIER].
    bool joinSquad(int squad);
    bool createSquad();
    void exitSquad();
    bool isSquadLeader() const;
    void applyAsSquadLeader(int a);
    void retireAsSquadLeader(int a);

    // giving orders (a squad leader)
    void issueSquadAttackOrder(const Vec3& position, Information* info);
    void issueSquadMoveOrder(const Vec3& position, Information* info);
    void issueSquadDefendOrder(const Vec3& position, Information* info);

    // taking orders. `recieve`, not `receive`.
    void recieveAttackSquadOrder();
    void recieveDefendSquadOrder();
    void recieveMoveSquadOrder();
    void recieveAttackCommanderOrder();
    void recieveDefendCommanderOrder();
    void recieveMoveCommanderOrder();
    void acceptCommanderOrder();
    void declineCommanderOrder();
    void acceptSquadOrder();
    void declineSquadOrder();

    // [INFERRED] inline accessor: CommanderInterface's Bot overloads (ELF 0x3232D0..) read +4
    // directly.
    int getSquadId() const { return m_squadId; }

private:
    /*  0 */ BotMain* m_bot;        // the ctor argument
    /*  4 */ int m_squadId;         // = 0; `joinSquad` sets it on success. `createSquad` does not
                                    // see the note in the .cpp.
    /*  8 */ OrderType m_order;     // = OrderNone (4)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_BOTSQUAD_H
