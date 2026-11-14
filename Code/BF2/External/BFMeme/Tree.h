// dice::meme::Tree and its node, BF2.exe 0x83C8C0..0x83D0F0: Tree table 0x94F440 (19 slots, descriptor
// 0x94F4D8 "dice::meme::Tree", base Object, factory 0x83CFF0), TreeNode table 0x94F490. Path and the
// node class name are [INFERRED]; the node is not registered (its type slot is Object's).
//
// A tree is a root node plus a name. Nodes are linked as a doubly linked sibling list (+4 previous,
// +8 next) with a first-child link (+0xC) and a parent link (+0x10). Every whole-tree pass walks
// child, then next, then previous from the root, and each sibling direction only continues in its
// own direction.
#ifndef BFMEME_TREE_H
#define BFMEME_TREE_H
#include "Object.h"
#if !DEDICATED
namespace dice { namespace meme {

class TreeNode : public Object
{
public:
    TreeNode(TreeNode* parent, TreeNode* next, TreeNode* previous, TreeNode* child);
    ~TreeNode() override;                                 // 0x83CC40
    // 0x83C8F0, shared by the three visit slots: a tail call to streamProperties.
    bool visit0x2c(System*, Stream*) override;
    bool visit0x34(System*, Stream*) override;
    bool visit0x38(System*, Stream*) override;

    TreeNode* m_previous;                                 // +0x04
    TreeNode* m_next;                                     // +0x08
    TreeNode* m_child;                                    // +0x0C
    TreeNode* m_parent;                                   // +0x10
};

class Tree : public Object
{
public:
    Tree();                                               // factory 0x83CFF0: both fields zero
    ~Tree() override;                                     // 0x83CB80: frees the name only
    TypeDescriptor* getType() override;                   // 0x83D010
    unsigned int getObjectSize() override;                // 0x834800 (12)
    const char* getName() override;                       // 0x68E7B0
    bool setName(const char*) override;                   // 0x8375C0
    bool slot_0x20() override;                            // 0x5A5E20: true
    bool visit0x2c(System*, Stream*) override;            // 0x83CBC0: stream, then destroy every node
    bool streamProperties(System*, Stream*) override;     // 0x83D050: every node's properties
    bool visit0x34(System*, Stream*) override;            // 0x83D090: load the node structure
    bool visit0x38(System*, Stream*) override;            // 0x83D0B0: save the node structure

    // +0x40, 0x83C8C0: clears the flag and answers null. [UNRESOLVED] name and parameters.
    virtual TreeNode* slot_0x40(System*, void*, bool* found, void*);
    // +0x44, 0x83CC00: a 0x14-byte node with its links set; the System is unused.
    virtual TreeNode* createNode(System*, TreeNode* parent, TreeNode* next, TreeNode* previous,
                                 TreeNode* child);
    // +0x48, 0x83C8D0: deletes the node (if any) and clears the pointer.
    virtual void deleteNode(System*, TreeNode*& node);

    static TypeDescriptor* getClassType();

    TreeNode* m_root;                                     // +0x04
    char*     m_name;                                     // +0x08, owned copy
};

} }
#endif
#endif
