// [INFERRED] Source placement: beside its header, whose path AIDLL_r.dll confirms (Main/InformationManager.h).

#include "InformationManager.h"

namespace dice {
namespace hfe {
namespace ai {

// The .bss global. `BF2AI::init` stores into it and `BF2AI::clear` nulls it; nothing else writes it.
InformationManager* informationManager = 0;

// Allocates the information pool without checkPointer.
InformationManager::InformationManager()
{
    m_pool = new Pool();
}

// Frees pool storage when present. The pool does not own its Information objects;
// callers must release those separately before destroying the manager.
InformationManager::~InformationManager()
{
    if (m_pool)
        operator delete(m_pool);
}

} // namespace ai
} // namespace hfe
} // namespace dice
