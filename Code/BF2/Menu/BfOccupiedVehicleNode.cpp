// Occupied-vehicle HUD node properties and drawing. [INFERRED] Source placement.
#include "BfOccupiedVehicleNode.h"
#if !DEDICATED
#include "BfOccupiedVehicleData.h"
#include "../External/BFMeme/Effect.h"   // drawContextTexture (82D790)
#include "../External/BFMeme/Quad.h"
#include "../External/BFMeme/Stream.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor class name. Counter A22290; construction and destruction
// do not adjust it, exactly as in the other Bf2 node descriptors.
int bfOccupiedVehicleNodeInstances;
class BfOccupiedVehicleNodeTypeDescriptor final : public TypeDescriptor
{
public:
    BfOccupiedVehicleNodeTypeDescriptor()
        : TypeDescriptor("dice::meme::BfOccupiedVehicleNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override // operator new(0x2C), then (null, 0, 0, 0)
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Allocation and construction are real; publication retains Node33.
        void* allocation = ::operator new(sizeof(BfOccupiedVehicleNode));
        return allocation ? new(allocation) BfOccupiedVehicleNode(nullptr, 0, 0, 0) : nullptr;
    }
    void addInstance() override { ++bfOccupiedVehicleNodeInstances; }
    void removeInstance() override { --bfOccupiedVehicleNodeInstances; } // 7C8AF0, no zero guard
    int getInstanceCount() override { return bfOccupiedVehicleNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(BfOccupiedVehicleNode); }
};

char* cloneOccupiedName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes); // no allocation-null guard in the original
    return copy;
}
// 737EE0 takes the address of the pointer: it frees the clone and nulls it.
void freeOccupiedName(char*& name)
{
    ::operator delete(name);
    name = nullptr;
}
}

// Both position pointers start at the node's OWN zero word, so an
// element that never gets a pos-variable row still paints at the bounds origin.
BfOccupiedVehicleNode::BfOccupiedVehicleNode(const char* name, int seat,
                                             int width, int height)
    : m_data(nullptr), m_seat(0), m_drawDebugPicture(false), m_name(nullptr),
      m_zero(0), m_positionX(&m_zero), m_positionY(&m_zero),
      m_width(0.0f), m_height(0.0f)
{
    freeOccupiedName(m_name);            // clears the initially null clone, as shipped
    m_name = cloneOccupiedName(name);
    m_seat = seat;
    m_width = static_cast<float>(width);   // fild
    m_height = static_cast<float>(height); // fild
}

// Runs base teardown. The existing implementation does not free the cloned name.
BfOccupiedVehicleNode::~BfOccupiedVehicleNode() {}

unsigned int BfOccupiedVehicleNode::getObjectSize()
{ return sizeof(BfOccupiedVehicleNode); }
const char* BfOccupiedVehicleNode::getName() { return m_name; }
bool BfOccupiedVehicleNode::setName(const char* name)
{
    freeOccupiedName(m_name);
    m_name = cloneOccupiedName(name);   // free-before-copy aliasing defect retained
    return true;
}
bool BfOccupiedVehicleNode::slot_0x24() { return true; }

bool BfOccupiedVehicleNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    if (!stream->streamInt(&m_seat, "Position")) return false;
    if (!stream->streamBool(&m_drawDebugPicture, "Draw debug pic")) return false;
    return stream->streamData(system, reinterpret_cast<Data*&>(m_data),
                              "BfOccupiedVehicleData",
                              BfOccupiedVehicleData::getClassType());
}

bool BfOccupiedVehicleNode::setDataForHud(BfOccupiedVehicleData* data) // 7CDFC0, +C8
{ m_data = data; return true; }

bool BfOccupiedVehicleNode::setPositionBindingForHud(int index, int* value) // 7C8A90, +CC
{
    if (!value) return false;
    if (index == 0) m_positionX = value;
    else m_positionY = value;
    return true;
}

// The seat's icon, drawn at the bound position offset from the element's
// own bounds. A node with no data paints nothing and reports nothing.
void BfOccupiedVehicleNode::paintSelf(System* system, const NodeState*,
                                      const Rect* bounds, const Rect* clip,
                                      RenderContext* context)
{
    if (!m_data) return;
    const TextureHandle texture = m_data->getOccupantTexture(system, m_seat);
    Rect placed;
    placed.x = static_cast<float>(*m_positionX) + bounds->x;   // fild then fadd
    placed.y = static_cast<float>(*m_positionY) + bounds->y;
    placed.width = m_width;
    placed.height = m_height;
    placed.z = 0.0f;
    Quad quad(&placed, clip);
    if (!texture) {
        // An unresolved seat icon is still drawn, just tinted fully
        // transparent black. Retail builds that colour by overwriting the tail
        // of `placed` in place and passing its address - harmless, because the
        // Quad constructor has already consumed the rect, but recorded here
        // rather than reproduced as aliasing.
        const Color transparent = {0.0f, 0.0f, 0.0f, 0.0f};
        colorQuad(&quad, &transparent);
    }
    // The node's OWN context goes on, not context->parent: this is a node's
    // paint, so its own effect chain applies to the icon.
    drawContextTexture(context, system, texture, &quad);
}

TypeDescriptor* BfOccupiedVehicleNode::getClassType()
{
    static BfOccupiedVehicleNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* BfOccupiedVehicleNode::getType() { return getClassType(); }
namespace {
TypeDescriptor* const registeredBfOccupiedVehicleNodeType =
    BfOccupiedVehicleNode::getClassType();
}

#if defined(_M_IX86)
static_assert(sizeof(BfOccupiedVehicleNodeTypeDescriptor) == 16, "client BF2 occupied metadata");
static_assert(sizeof(BfOccupiedVehicleNode) == 44, "native BfOccupiedVehicleNode storage");
#endif
} }
#endif
