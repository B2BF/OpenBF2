// Hands the bot's spawn to its guard position.
// Placement is [INFERRED]; the class has its own TU in the image.
#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_GUARDPOSITIONBOTSCB_H
#define AI_AIDLL_AI_AISTRATEGICENTITY_GUARDPOSITIONBOTSCB_H

#include "../AIBot/BotCallBack.h"

namespace dice { namespace hfe { namespace ai {

class Bot;
class GuardPosition;

class GuardPositionBotSCB : public IBotSpawnCallBack
{
public:
    // The guard position is borrowed, not owned.
    GuardPositionBotSCB(Bot* bot, GuardPosition* guardPosition);
    virtual ~GuardPositionBotSCB();
    virtual void reportSpawn();

private:
    /* +8 */ GuardPosition* m_guardPosition;
};

} } }

#endif
