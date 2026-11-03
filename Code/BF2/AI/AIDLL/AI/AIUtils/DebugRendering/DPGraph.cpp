// Debug graph primitive. Evidence: libaidll.so (ia-32) 0x373130..0x37348F.
// [INFERRED] Source placement.

#include "DPGraph.h"

#include "IBFGraph.h"

#include "../../../../AIInterface/BFKnowledge/BFGraphCreator.h"
#include "../../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x373130 (C2; C1 0x373190). Colour 0.
DPGraph::DPGraph(float lifeTime)
    : DrawPrimitive(lifeTime, 0)
{
    m_graph = static_cast<IBFGraphCreator*>(CommonSingletonServer::m_iBFGraphCreator)->createGraph();
}

// ELF 0x3731F0 (D2; D1 0x373240, D0 0x373290).
DPGraph::~DPGraph()
{
    static_cast<IBFGraphCreator*>(CommonSingletonServer::m_iBFGraphCreator)->destroyGraph(m_graph);
}

// ELF 0x3732F0.
bool DPGraph::draw(IDebugRenderer* /*renderer*/)
{
    return true;
}

// ELF 0x373300: m_graph slot 2 (BFGraph::setNumberOfSamples(int)).
void DPGraph::setNumberOfSamples(int samples)
{
    m_graph->setNumberOfSamples(samples);
}

// ELF 0x373330: m_graph slot 3 (BFGraph::setDimensions(float, float, float, float)).
void DPGraph::setDimensions(float x, float y, float width, float height)
{
    m_graph->setDimensions(x, y, width, height);
}

// ELF 0x373370: m_graph slot 4 (BFGraph::setColours(unsigned int, unsigned int, unsigned int)).
void DPGraph::setColours(unsigned int first, unsigned int second, unsigned int third)
{
    m_graph->setColours(first, second, third);
}

// ELF 0x3733A0: m_graph slot 5 (BFGraph::setThresholds(float, float)).
void DPGraph::setThresholds(float low, float high)
{
    m_graph->setThresholds(low, high);
}

// ELF 0x3733D0: m_graph slot 6 (BFGraph::setMinMaxY(float, float)).
void DPGraph::setMinMaxY(float minimum, float maximum)
{
    m_graph->setMinMaxY(minimum, maximum);
}

// ELF 0x373400: m_graph slot 9 (BFGraph::addSample(float)).
void DPGraph::addSample(float sample)
{
    m_graph->addSample(sample);
}

// ELF 0x373430: m_graph slot 7 (BFGraph::setCurrentSampleEnding(std::string const&)).
void DPGraph::setCurrentSampleEnding(const std::string& ending)
{
    m_graph->setCurrentSampleEnding(ending);
}

// ELF 0x373460: m_graph slot 8 (BFGraph::setCurrentSampleOffset(int, int)).
void DPGraph::setCurrentSampleOffset(int x, int y)
{
    m_graph->setCurrentSampleOffset(x, y);
}

} // namespace ai
} // namespace hfe
} // namespace dice
