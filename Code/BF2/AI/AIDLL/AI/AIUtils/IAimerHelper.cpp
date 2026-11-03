// Aimer singleton registration. [INFERRED] Source placement.

#include "IAimerHelper.h"

#include "../../../../Core/ClassManager.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// ELF 0x2EAA00 (C1; C2 0x2EA960).
IAimerHelper::IAimerHelper()
{
    // shipped defect: the cached singleton is looked up as "AimerHelper", not this name.
    if (!CommonSingletonServer::m_iAimerHelper)
        classManager->registerSimpleSingleton("IAimerHelper", this);
}

// ELF 0x2EAB30 (D1; D2 0x2EAAA0, D0 0x2EABC0).
IAimerHelper::~IAimerHelper()
{
    if (CommonSingletonServer::m_iAimerHelper == this)
        classManager->unregisterSimpleSingleton("IAimerHelper");
}

} // namespace ai
} // namespace hfe
} // namespace dice
