// Triggerable behaviour (infantry "Triggerable" slot of aibehaviours.ai): a man-down bot is the
// only one it applies to; its urgency is always 0 here, so it is never chosen by urgency alone.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

#include "BBTriggerable.h"

// static init: ELF 0x224F30, 0x2250A0, 0x2250D0, 0x225140 (iostream Init, colors, triggerables).
#include <iostream>

#include "../AIBot/BotMain.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"

#include <cstddef>
#include <type_traits>

namespace dice {
namespace hfe {
namespace world {
namespace {

// [CHOSEN, not recovered] Type, component names and constructor spelling for the
// three-float elements stored by ELF 0x224F30. No original type name is attested.
struct DebugMeshMaterialColor
{
    float r, g, b;

    DebugMeshMaterialColor(float red, float green, float blue) : r(red), g(green), b(blue) {}
};

static_assert(sizeof(DebugMeshMaterialColor) == 12, "color element stride");
static_assert(offsetof(DebugMeshMaterialColor, r) == 0 &&
              offsetof(DebugMeshMaterialColor, g) == 4 &&
              offsetof(DebugMeshMaterialColor, b) == 8, "color component offsets");
static_assert(std::is_trivially_destructible<DebugMeshMaterialColor>::value,
              "the color table has no registered teardown");

} // anonymous namespace

// ELF 0x4CE3E0, a separate local table. __tcf_1 destroys only the triggerables map.
// [CHOSEN, not recovered] Retain the unreferenced retail object with modern compilers.
#if defined(__GNUC__)
__attribute__((used))
#endif
static DebugMeshMaterialColor debugMeshMaterialColors[10] = {
    {0.66f, 0.66f, 0.185f},
    {0.185f, 0.66f, 0.185f},
    {0.185f, 0.185f, 0.66f},
    {0.185f, 0.66f, 0.66f},
    {0.66f, 0.185f, 0.185f},
    {0.66f, 0.66f, 0.66f},
    {0.66f, 0.185f, 0.66f},
    {0.185f, 0.185f, 0.185f},
    {1.0f, 1.0f, 1.0f},
    {0.0f, 0.0f, 0.0f}
};
static_assert(sizeof(debugMeshMaterialColors) == 0x78, "local color table extent");

} // namespace world
namespace ai {

// ELF 0x224F30 constructs this map after the colors; __tcf_1 ELF 0x2250D0 destroys
// its nodes and sentinel, without deleting the stored Triggerable pointers.
std::map<unsigned int, world::Triggerable*> BBTriggerable::m_triggerables;

// ELF 0x2248F0 (C2) / 0x224AF0 (C1). The base gets getMaxNBots (IAISettings slot 0) as its
// per-bot count, then a second getMaxNBots sizes the +0x08 array (checkPointer'd, not
// initialised) and every base urgency (+0x10) is set to 0.0f (rodata 0x49CB34).
BBTriggerable::BBTriggerable(unsigned char index, unsigned char queue, PlanGenerator* pattern, UrgeCurve* curve,
                             float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[count]);
    for (int i = 0; i < count; ++i)
        m_urgency[i] = 0.0f;
    m_unknown24 = 0;
}

// ELF 0x224CF0 (D2) / 0x224D40 (D1) / 0x224D90 (D0): delete[] +0x08 when set, then the base.
BBTriggerable::~BBTriggerable()
{
    if (m_targets)
        delete[] m_targets;
}

// ELF 0x224DE0: m_urgency[bot->getBotId()] = 0.0f, return it (flds of the same slot).
float BBTriggerable::calculateUrgency(Bot* bot, bool, float)
{
    m_urgency[bot->getBotId()] = 0.0f;
    return m_urgency[bot->getBotId()];
}

// ELF 0x224E40: bot->getPlayer() (+0xBC) ->getIsAlive() (+0x24); alive bots answer getManDown (+0x1B0).
bool BBTriggerable::isApplicable(Information*, Bot* bot)
{
    if (!bot->getPlayer()->getIsAlive())
        return false;
    return bot->getManDown();
}

// ELF 0x224E80: drops the bot's entry from m_triggerables (find + erase), then the pattern
// (+0x0C) unInit(bot) through PlanGenerator slot +0x10.
void BBTriggerable::unInitialise(Bot* bot)
{
    std::map<unsigned int, world::Triggerable*>::iterator it = m_triggerables.find(bot->getBotId());
    if (it != m_triggerables.end())
        m_triggerables.erase(it);
    m_pattern->unInit(bot);
}

} // namespace ai
} // namespace hfe
} // namespace dice
