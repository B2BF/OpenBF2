// Occupied-vehicle HUD node. [INFERRED] Header placement. Native accessors bridge the incomplete Node interface.
#ifndef MENU_BFOCCUPIEDVEHICLENODE_H
#define MENU_BFOCCUPIEDVEHICLENODE_H
#include "../External/BFMeme/Node.h"
#if !DEDICATED
namespace dice { namespace meme {
class BfOccupiedVehicleData;

class BfOccupiedVehicleNode : public Node {
public:
    // Defaults [INFERRED] from factory7C8BA0, which supplies a null name and
    // three zeros.
    BfOccupiedVehicleNode(const char* name = nullptr, int seat = 0,
                          int width = 0, int height = 0);
    ~BfOccupiedVehicleNode() override;                    // see below
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;                            // 5A5E20, true
    bool streamProperties(System*, Stream*) override;
    void paintSelf(System*, const NodeState*, const Rect* bounds,
                   const Rect* clip, RenderContext*) override;
    static TypeDescriptor* getClassType();                // 7C8BD0, metadata9369A8

    // [CHOSEN, not recovered] Native occupied-vehicle node property access.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setDataForHud(BfOccupiedVehicleData* data);       // 7CDFC0, +C8
    // Index 0 binds x and every other index binds y; a null pointer is the one
    // refusal. The pointee is an INT - paintSelf `fild`s it into the position.
    bool setPositionBindingForHud(int index, int* value);  // 7C8A90, +CC

protected:
    friend struct BfOccupiedVehicleNodeFixture;
    // +08, borrowed; streamed as "BfOccupiedVehicleData" with that descriptor.
    BfOccupiedVehicleData* m_data;       // +08
    int m_seat;                          // +0C, ctor argument, streamed "Position"
    bool m_drawDebugPicture;             // +10, streamed "Draw debug pic"
    char* m_name;                        // +14, owned clone - never freed
    int m_zero;                          // +18, the default binding target
    int* m_positionX;                    // +1C, starts &m_zero
    int* m_positionY;                    // +20, starts &m_zero
    float m_width, m_height;             // +24/+28, both fild'd from the ctor
};
#if defined(_M_IX86)
static_assert(sizeof(BfOccupiedVehicleNode) == 44, "native BfOccupiedVehicleNode storage");
#endif
} }
#endif
#endif
