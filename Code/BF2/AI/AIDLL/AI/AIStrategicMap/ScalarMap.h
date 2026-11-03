// A row-major grid of floats over the strategic map. Evidence: libaidll.so (ia-32)
// 0x364640..0x365FF8; sizeof 12 (width, height, the owned array). Member names [INFERRED].

#ifndef AI_AIDLL_AI_AISTRATEGICMAP_SCALARMAP_H     // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICMAP_SCALARMAP_H

namespace dice {
namespace hfe {
namespace ai {

class ScalarMap
{
public:
    // Allocates width * height uninitialised values (checkPointer reports a failed allocation).
    ScalarMap(int width, int height);
    ~ScalarMap();

    // Writes the 3x3 (edges 2x3, corners 2x2) box average of this map into result, which must
    // have this map's size. Interior rows are resumable: row is the row to continue at (0 starts
    // at the first interior row, -1 skips them); when the update-time budget (IBFTimer
    // getUpdateTime() >= deadline) runs out the next row is stored and false returned. A 1x1 map
    // answers true at once; any other size reports the shipped "Does not work" error first.
    bool smooth(ScalarMap& result, double deadline, int& row) const;
    // indices[0..count) receive the indices of the count largest values, largest last; with fewer
    // candidates above -1e20 the remaining entries are -1.
    void sortLargest(int count, int* indices) const;

    /* +0x0 */ int m_width;
    /* +0x4 */ int m_height;
    /* +0x8 */ float* m_values;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICMAP_SCALARMAP_H
