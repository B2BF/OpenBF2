// Hands the bot's death to its guard position.
// Placement is [INFERRED]; the class has its own TU in the image.
#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_GUARDPOSITIONBOTDCB_H
#define AI_AIDLL_AI_AISTRATEGICENTITY_GUARDPOSITIONBOTDCB_H

#include "../AIBot/BotCallBack.h"

namespace dice { namespace hfe { namespace ai {

class Bot;
class GuardPosition;

class GuardPositionBotDCB : public IBotDeathCallBack
{
public:
    // The guard position is borrowed, not owned.
    GuardPositionBotDCB(Bot* bot, GuardPosition* guardPosition);
    virtual ~GuardPositionBotDCB();
    virtual void reportDeath();

private:
    /* +8 */ GuardPosition* m_guardPosition;
};

} } }

#endif
