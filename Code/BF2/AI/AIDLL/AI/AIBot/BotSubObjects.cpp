// Aim-correction state initialization.

#include "BotSubObjects.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1A2210, __tcf_0 0x1A2270, _GLOBAL__I 0x1A22A0).
#include <iostream>

#include "../Main/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

namespace dice {
namespace hfe {
namespace ai {

// Initializes aim-correction state while leaving byte ranges +0..+3 and +68..+83
// untouched. [UNRESOLVED] The contents of those ranges remain unknown.
FireCorrectionData::FireCorrectionData()
    : m_flag4(true)
    , m_flag5(true)
    , m_flag84(false)
{
    // +0..+3 and +68..+83 are deliberately untouched - see above.
    for (int i = 0; i < 3; ++i)
        m_firedTarget[i] = 0.0f;
    Vec3* const zeroed[4] = { &m_missedPosition, &m_aimDirection, &m_vec44, &m_predictedTarget };
    for (int i = 0; i < 4; ++i)
        zeroed[i]->x = zeroed[i]->y = zeroed[i]->z = 0.0f;
    // +88..+120 zeroed: the three points.
    Vec3* const points[3] = { &m_projectilePrePoint, &m_projectilePostPoint, &m_projectileTargetPoint };
    for (int i = 0; i < 3; ++i)
        points[i]->x = points[i]->y = points[i]->z = 0.0f;
}

// ELF 0x1A2170 (160 bytes).
void FireCorrectionData::newTarget(unsigned int handle)
{
    m_flag5 = true;
    m_flag4 = true;
    m_flag6 = true;
    m_double72 = -1.0f;
    Information* const information = informationManager->getInformation(handle);
    m_vec44.x = 0.0f; m_vec44.y = 0.0f; m_vec44.z = 0.0f;
    if (information)
        m_shotTarget = information->getAIObject()->getObject();     // IAIObject slot +0x20
    else
        m_shotTarget = 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
