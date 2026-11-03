// A timed debug graph backed by an engine IBFGraph.
// [INFERRED] Source placement (the TU is DPGraph.cpp; it links between the attested
// AIUtils/DebugRendering/DPDynamicLine.cpp and DPManager.cpp).

#ifndef AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_DPGRAPH_H
#define AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_DPGRAPH_H

#include <string>

#include "../../DrawPrimitives/DrawPrimitive.h"
#include "../../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// sizeof == 0x18 on ia-32. The graph is created through IBFGraphCreator in the constructor and
// destroyed through it in the destructor; every setter forwards to it.
class DPGraph : public DrawPrimitive
{
public:
    explicit DPGraph(float lifeTime);
    virtual ~DPGraph();

    // The engine graph draws itself; this only reports the primitive alive.
    virtual bool draw(IDebugRenderer* renderer);

    void setNumberOfSamples(int samples);
    void setDimensions(float x, float y, float width, float height);
    void setColours(unsigned int first, unsigned int second, unsigned int third);
    void setThresholds(float low, float high);
    void setMinMaxY(float minimum, float maximum);
    void addSample(float sample);
    void setCurrentSampleEnding(const std::string& ending);
    void setCurrentSampleOffset(int x, int y);

protected:
    /* +0x14 */ IBFGraph* m_graph;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_DPGRAPH_H
