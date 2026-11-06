// Per-square strategic temperature: five top-level tree nodes, each with an accumulated
// temperature and a multiplicative weight. Deeper nodes of the 41-node TemperatureTreeNode tree
// only scale a contribution on its way up to one of the five. Evidence: libaidll.so (ia-32)
// 0x366510..0x366A55; sizeof 40 (the copy constructor copies ten words). Member names [INFERRED].

#ifndef AI_AIDLL_AI_AISTRATEGICMAP_TEMPERATURETREE_H     // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICMAP_TEMPERATURETREE_H

namespace dice {
namespace hfe {
namespace ai {

class TemperatureTreeModifiers;
enum TemperatureTreeNode : int;           // index 0..40

class TemperatureTree
{
public:
    TemperatureTree();
    TemperatureTree(const TemperatureTree& other);

    // Every temperature to 0, every weight to 1.
    void reset();
    // Adds temperature at node (scaled by the modifiers up the tree) and multiplies the weight of
    // every top-level node on the way by weight.
    void addTemperature(TemperatureTreeModifiers* modifiers, TemperatureTreeNode node, float temperature, float weight);
    // As above with the temperature formed from six combat strengths and the node's per-strength
    // modifiers.
    void addTemperature(TemperatureTreeModifiers* modifiers, TemperatureTreeNode node, float* strengths, float weight);
    // As above with the strength sum multiplied by scale first.
    void addTemperature(TemperatureTreeModifiers* modifiers, TemperatureTreeNode node, float* strengths, float scale, float weight);
    // Adds the other tree's temperatures; the weights are untouched.
    void addTemperature(const TemperatureTree& other);
    void propagateToParents(TemperatureTreeModifiers* modifiers, TemperatureTreeNode node, float temperature, float weight);

    /* +0x00 */ float m_temperatures[5];
    /* +0x14 */ float m_weights[5];
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICMAP_TEMPERATURETREE_H
