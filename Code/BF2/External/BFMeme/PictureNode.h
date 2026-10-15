// Picture node using the known Node prefix; unsupported suffix calls retain diagnostic stops.
#ifndef BFMEME_PICTURENODE_H
#define BFMEME_PICTURENODE_H
#include "Node.h"
#if !DEDICATED
namespace dice { namespace meme {
class PictureNode : public Node {
public:
    explicit PictureNode(TextureHandle texture);
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;
    void paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override;
    static TypeDescriptor* getClassType();                // 836320, factory8362F0
protected:
    TextureHandle m_texture;                             // +08, signed texture handle
};
#if defined(_M_IX86)
static_assert(sizeof(PictureNode)==12, "native PictureNode prefix storage");
#endif
} }
#endif
#endif
