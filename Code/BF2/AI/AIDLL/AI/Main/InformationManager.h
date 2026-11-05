// Handle pool for AI information. The pool does not own the referenced information objects.
// [INFERRED] Method names represent the observed pool operations.
// Source placement: AIDLL_r.dll __FILE__ "Code/BF2//AI/AIDLL/AI/Main/InformationManager.h" (an inline assert).

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONMANAGER_H
#define AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONMANAGER_H

#include "../../../../Core/SpHandlePool.h"

namespace dice {
namespace hfe {
namespace ai {

// [FRONTIER] the pooled type. `Information` is the AI's knowledge record; the pool only ever stores
// `Information*`, so nothing here needs it complete. Chunk 07 owns it.
class Information;

// sizeof == 4, from `operator new(4u)`: one pointer and nothing else.
class InformationManager
{
public:
    typedef SpHandlePool<Information, 65535> Pool;

    InformationManager();
    ~InformationManager();

    Pool* getPool() const { return m_pool; }

    // Looks up an information handle. [INFERRED] Method name.
    Information* getInformation(Pool::Handle handle) const { return m_pool->get(handle); }

private:
    /* +0x00 */ Pool* m_pool;
};

// Owned by BF2AI::init and released by BF2AI::clear.
extern InformationManager* informationManager;

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONMANAGER_H
