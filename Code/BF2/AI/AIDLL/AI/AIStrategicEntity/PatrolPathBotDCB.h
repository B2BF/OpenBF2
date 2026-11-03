// Hands the bot's death to its patrol path.
// Placement is [INFERRED]; the class has its own TU in the image.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_PATROLPATHBOTDCB_H
#define AI_AIDLL_AI_AISTRATEGICENTITY_PATROLPATHBOTDCB_H

#include "../AIBot/BotCallBack.h"

namespace dice { namespace hfe { namespace ai {

class Bot;
class PatrolPath;

class PatrolPathBotDCB : public IBotDeathCallBack
{
public:
    // The patrol path is borrowed, not owned.
    PatrolPathBotDCB(Bot* bot, PatrolPath* patrolPath);
    virtual ~PatrolPathBotDCB();
    virtual void reportDeath();

private:
    /* +8 */ PatrolPath* m_patrolPath;
};

} } }

#endif
