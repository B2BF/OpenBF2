// Random behaviour (infantry "Random" slot of aibehaviours.ai). Despite the name it reacts to a
// grenade warning: once BotNavigation reports one, the behaviour wins with urgency 1000 until
// 5 s after the grenade's time has passed.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

#include "BBRandom.h"

// static init: ELF 0x21AB30, 0x21AC40, 0x21AC70 (iostream Init, then the local color table).
#include <iostream>

#include "../AIBot/BotMain.h"
#include "../AIBotBehaviourPattern/BBPRandom.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIUtils/checkPointer.h"
#include "../AIBot/BotNavigation.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

#include <vector>
#include <cstddef>
#include <type_traits>

namespace dice {
namespace hfe {
namespace world {
namespace {

// [CHOSEN, not recovered] Type, component names and constructor spelling for the
// three-float elements stored by ELF 0x21AB30. No original type name is attested.
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

// ELF 0x4CE300, local symbol, ten elements. __tcf_0 destroys only iostream Init.
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

// ELF 0x21A250 (C2) / 0x21A470 (C1).
BBRandom::BBRandom(unsigned char index, unsigned char queue, PlanGenerator* pattern, UrgeCurve* curve,
                   float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_grenadePosition.x = 0.0f;   // 0x21A2BF: rodata 0x49CB34 into +0x38/+0x3C/+0x40
    m_grenadePosition.y = 0.0f;
    m_grenadePosition.z = 0.0f;
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[count]);
    for (int i = 0; i < count; ++i)
        m_urgency[i] = 0.0f;
    m_grenadeWarning = false;     // 0x21A341
    m_warningTime = 0.0;          // 0x21A345: fstl / fstpl of the same 0.0f
    m_grenadeTime = 0.0;
}

// ELF 0x21A690 (D2) / 0x21A6E0 (D1) / 0x21A730 (D0)
BBRandom::~BBRandom()
{
    if (m_targets)
        delete[] m_targets;
}

// ELF 0x21A780 (261 insns).
float BBRandom::calculateUrgency(Bot* bot, bool active, float)
{
    PlanGenerator* pattern = m_pattern;
    if (!active)
    {
        // 0x21AA90: an initiated pattern keeps the stored urgency; otherwise uninit and 0.
        if (pattern->isInitiated(bot))
            return m_urgency[bot->getBotId()];
        pattern->unInit(bot);
        return m_urgency[bot->getBotId()] = 0.0f;   // `fsts` - the stored value is returned
    }

    float urgency = 0.0f;
    Vec3 grenadePosition;          // -0x28, zeroed
    grenadePosition.x = 0.0f;
    grenadePosition.y = 0.0f;
    grenadePosition.z = 0.0f;

    if (!m_grenadeWarning)
    {
        // 0x21A7CC: BotNavigation::getGrenadeWarning(Vec3&, double&) returns bool (`testb %al`),
        // then BotNavigation's bool at +0x114 (m_flag276) must be set too.
        // 0x21A7E8 call, 0x21A87B `cmpb $0x0, 0x114(%eax)` on a second getBotNavigation.
        double grenadeTime;        // -0x60
        if (bot->getBotNavigation()->getGrenadeWarning(grenadePosition, grenadeTime)
            && bot->getBotNavigation()->getGrenadeFlag())
        {
            m_grenadeWarning = true;
            m_warningTime = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
            m_grenadeTime = grenadeTime;
            m_grenadePosition = grenadePosition;
        }
    }
    else
    {
        // 0x21A8C8: `fldl +0x30; faddl +0x28; fucomi; jbe` - nothing until the grenade's time has
        // passed; 5 s (rodata 0x49CDAC, a float) after it the warning is dropped, but this tick
        // still answers it.
        const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        const double end = m_grenadeTime + m_warningTime;
        if (now > end)
        {
            if (now > end + 5.0f)
                m_grenadeWarning = false;

            // 0x21A8FF: ignore the bot's own equipment object (no null check in the image) and
            // trace from one metre above it to the grenade; the result is not used (retail).
            std::vector<IAIObject*> ignore;
            Information* info = informationManager->getInformation(bot->getCurrentEquipment());
            ignore.push_back(info->getAIObject());
            IAIObjectEnvironment* environment =
                static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
            const Vec3& position = info->getPosition();
            Vec3 up;
            up.x = 0.0f;
            up.y = 1.0f;
            up.z = 0.0f;
            Vec3 from;
            from.x = position.x + up.x;
            from.y = position.y + up.y;
            from.z = position.z + up.z;
            bool hit;                  // -0x65
            environment->collideLineWithWorld(from, m_grenadePosition, ignore, false, 0.7f, false, hit);   // slot +0x40

            static_cast<BBPRandom*>(m_pattern)->init(bot);
            m_urgency[bot->getBotId()] = 1000.0f;   // rodata 0x49CF48
            return 1000.0f;                          // the local, not a reload (0x21AA42)
        }
    }

    // 0x21A7F1: `flds 0.001; flds urgency; fxch; fucompi; jbe init` - init unless below 0.001.
    if (urgency < 0.001f)
        pattern->unInit(bot);
    else
        static_cast<BBPRandom*>(m_pattern)->init(bot);
    m_urgency[bot->getBotId()] = urgency;
    return m_urgency[bot->getBotId()];
}

// ELF 0x21AAF0: alive bots answer getManDown (+0x1B0).
bool BBRandom::isApplicable(Information*, Bot* bot)
{
    if (!bot->getPlayer()->getIsAlive())
        return false;
    return bot->getManDown();
}

} // namespace ai
} // namespace hfe
} // namespace dice
