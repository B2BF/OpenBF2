// Picture node drawing and descriptor registration.
#include "PictureNode.h"
#if !DEDICATED
#include "Stream.h"
#include "Quad.h"
#include "Effect.h"
#include "TypeDescriptor.h"
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor class name; counterA3A0B4 is separate from metadata.
int pictureNodeInstances;
class PictureNodeTypeDescriptor final : public TypeDescriptor
{
public:
    PictureNodeTypeDescriptor() : TypeDescriptor("dice::meme::PictureNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Publication policy only; allocation/null branch/construction are real.
        void* allocation = ::operator new(sizeof(PictureNode));
        return allocation ? new(allocation) PictureNode(0) : nullptr;
    }
    // Inherited destructor/description retain the no-unlink/null bodies.
    void addInstance() override { ++pictureNodeInstances; }
    void removeInstance() override { --pictureNodeInstances; } // 836270, no zero guard
    int getInstanceCount() override { return pictureNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(PictureNode); }
};
#if defined(_M_IX86)
static_assert(sizeof(PictureNodeTypeDescriptor) == 16, "PictureNode metadata storage");
#endif
}

PictureNode::PictureNode(TextureHandle texture) : Node(), m_texture(texture) {}
TypeDescriptor* PictureNode::getClassType()
{
    static PictureNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* PictureNode::getType() { return getClassType(); }
namespace {
// CRT96E1EC ->871750 unconditionally sets the guard before construction,
// then storesA3A0CC. Native static lifetime owns the descriptor and teardown.
TypeDescriptor* const registeredPictureNodeType = PictureNode::getClassType();
}

unsigned int PictureNode::getObjectSize() { return sizeof(PictureNode); }
bool PictureNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    return stream->streamTexture(system, &m_texture, "Picture");
}
void PictureNode::paintSelf(System* system, const NodeState*, const Rect* bounds,
                            const Rect* clip, RenderContext* context)
{
    Quad quad(bounds, clip);
    drawContextTexture(context, system, m_texture, &quad);
}
} }
#endif
