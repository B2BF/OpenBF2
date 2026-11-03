// Hands the bot's spawn to its patrol path.
// Placement is [INFERRED]; the class has its own TU in the image.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_PATROLPATHBOTSCB_H
#define AI_AIDLL_AI_AISTRATEGICENTITY_PATROLPATHBOTSCB_H

#include "../AIBot/BotCallBack.h"

namespace dice { namespace hfe { namespace ai {

class Bot;
class PatrolPath;

class PatrolPathBotSCB : public IBotSpawnCallBack
{
public:
    // The patrol path is borrowed, not owned.
    PatrolPathBotSCB(Bot* bot, PatrolPath* patrolPath);
    virtual ~PatrolPathBotSCB();
    virtual void reportSpawn();

private:
    /* +8 */ PatrolPath* m_patrolPath;
};

} } }

#endif
