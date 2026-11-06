// [INFERRED] Source placement.
// [FRONTIER] Remaining methods stop by name until implemented.
// [TEMP] Temporary implementation boundaries remain.
// [CHOSEN, not recovered] Temporary implementation boundaries remain.
// [RETIRE-WHEN: aidll-01-adapters] Complete and validate the remaining BFGraphCreator operations.

#include "BFGraphCreator.h"

#include "AIFrontier.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/AIUtils/DebugRendering/IBFGraph.h"
#include "../../../Core/ClassManager.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// First wins in, exact match out.
IBFGraphCreator::IBFGraphCreator()
{
    if (!CommonSingletonServer::m_iBFGraphCreator)
        classManager->registerSimpleSingleton(std::string("IBFGraphCreator"), this);
}

IBFGraphCreator::~IBFGraphCreator()
{
    if (CommonSingletonServer::m_iBFGraphCreator == this)
        classManager->unregisterSimpleSingleton(std::string("IBFGraphCreator"));
}

BFGraphCreator::BFGraphCreator()
{
}

BFGraphCreator::~BFGraphCreator()
{
}

IBFGraph* BFGraphCreator::createGraph()
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFGraphCreator::createGraph");
}

void BFGraphCreator::destroyGraph(IBFGraph* graph)
{
    // ELF 0x842E610: `if (graph) graph->[vtable +0x04]` - Itanium's deleting destructor, which is what
    // `delete` compiles to. The null guard is dice's and is kept.
    if (graph)
        delete graph;
}

} // namespace ai
} // namespace hfe
} // namespace dice
