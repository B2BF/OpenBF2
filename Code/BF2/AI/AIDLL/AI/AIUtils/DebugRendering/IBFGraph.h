// Engine debug-graph interface, as the AI DLL calls it through DPGraph.
// [CHOSEN, not recovered] Declared AIDLL-side because the engine-side AIInterface tree has no
// declaration yet; move it next to BFGraphCreator.h once that tree adds one.
// Evidence: bf2 (ia-32) vtable for dice::hfe::ai::BFGraph at 0x88DB320 (slots below, D1/D0 first)
// and IBFGraph at 0x88DB440 (all eight pure); names and parameter types from the BFGraph symbols.
// Return types [INFERRED] void: the engine bodies only forward to an inner object, and every DLL
// call site (DPGraph's setters) discards the result register.

#ifndef AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_IBFGRAPH_H
#define AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_IBFGRAPH_H

#include <string>

namespace dice {
namespace hfe {
namespace ai {

class IBFGraph
{
public:
    virtual ~IBFGraph() {}

    virtual void setNumberOfSamples(int samples) = 0;                                  // slot 2
    virtual void setDimensions(float x, float y, float width, float height) = 0;       // slot 3
    virtual void setColours(unsigned int first, unsigned int second, unsigned int third) = 0;  // 4
    virtual void setThresholds(float low, float high) = 0;                             // slot 5
    virtual void setMinMaxY(float minimum, float maximum) = 0;                         // slot 6
    virtual void setCurrentSampleEnding(const std::string& ending) = 0;                // slot 7
    virtual void setCurrentSampleOffset(int x, int y) = 0;                             // slot 8
    virtual void addSample(float sample) = 0;                                          // slot 9
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_IBFGRAPH_H
