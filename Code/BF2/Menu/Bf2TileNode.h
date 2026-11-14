// dice::meme::Bf2TileNode, BF2.exe 0x7CB990..0x7CBBD0: table 0x937D50, descriptor published on first
// use (0x7CBBD0, its only direct caller is the builder cast 0x794F90), factory 0x7CBB80. Built by
// hudBuilder.createTileNode as "<name>TileNode" under a Bf2TransformNode and configured by
// hudBuilder.setTileNodeOptions. It paints `count` untextured quads side by side through the
// context's effects.
#ifndef BF2_MENU_BF2TILENODE_H
#define BF2_MENU_BF2TILENODE_H
#include "../External/BFMeme/Node.h"
#if !DEDICATED
namespace dice { namespace meme {

class Bf2TileNode : public Node
{
public:
    explicit Bf2TileNode(const char* name = nullptr, int count = 0);   // 7CB9E0
    ~Bf2TileNode() override;                                          // 7CBA20
    TypeDescriptor* getType() override;                               // 7CBBD0
    unsigned int getObjectSize() override;                            // 8342C0 (0x18)
    const char* getName() override;                                   // 68E7B0
    bool setName(const char* name) override;                          // 8375C0
    bool slot_0x24() override;                                        // 5A5E20: true
    void paintSelf(System*, const NodeState*, const Rect* bounds, const Rect* clip,
                   RenderContext*) override;                          // 7CBA60
    // +C8, 7CB990: the tile count, the thickness across the run, and the direction (0 = down).
    virtual bool setOptions(int count, int thickness, int horizontal);
    static TypeDescriptor* getClassType();

private:
    char* m_name;          // +08, owned copy
    int   m_count;         // +0C
    int   m_thickness;     // +10
    int   m_horizontal;    // +14
};

} }
#endif
#endif
