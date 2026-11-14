// dice::meme::Tree and TreeNode, BF2.exe 0x83C8C0..0x83D0F0 (see Tree.h for the layout).
//
// The whole-tree passes are free functions in the image (fastcall: System in ECX, the tree in EDX).
// Each link direction has its own walker so a sibling chain only continues the way it came:
//   delete     0x83C900                 one walker, unlinks each sibling before descending into it
//   properties 0x83CF00 / 0x83CAE0 / 0x83CF80   (from a node / along next / along previous)
//   save       0x83CC70 / 0x83C9B0 / 0x83CD10
//   load       0x83CDA0 / 0x83CA40 / 0x83CE60
// Save and load mark each link with streamBool(present, "Bool"); load creates the node through the
// tree's +0x44 and lets it read itself through +0x34, save lets it write itself through +0x38.
#include "Tree.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {

const char kPresentName[] = "Bool";                       // ds:0x94F4D0

char* cloneTreeName(const char* name)                     // 0x737DC0
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* result = static_cast<char*>(::operator new(bytes));
    std::memcpy(result, name, bytes);
    return result;
}

// 0x83C900. Streams the node through +0x2C, then its child, previous and next (each sibling unlinked
// from this node first), then deletes it.
bool destroyNodes(System* system, Tree* tree, Stream* stream, TreeNode*& slot)
{
    TreeNode* node = slot;
    if (!node) return true;
    if (!node->visit0x2c(system, stream)) return false;
    if (!destroyNodes(system, tree, stream, node->m_child)) return false;
    if (node->m_previous) {
        node->m_previous->m_next = nullptr;
        if (!destroyNodes(system, tree, stream, node->m_previous)) return false;
    }
    if (node->m_next) {
        node->m_next->m_previous = nullptr;
        if (!destroyNodes(system, tree, stream, node->m_next)) return false;
    }
    delete slot;
    slot = nullptr;
    return true;
}

bool streamNextProperties(System*, Tree*, Stream*, TreeNode*&);
bool streamPreviousProperties(System*, Tree*, Stream*, TreeNode*&);

bool streamNodeProperties(System* system, Tree* tree, Stream* stream, TreeNode*& slot)   // 0x83CF00
{
    TreeNode* node = slot;
    if (!node) return true;
    if (!node->streamProperties(system, stream)) return false;
    if (!streamNodeProperties(system, tree, stream, slot->m_child)) return false;
    if (!streamNextProperties(system, tree, stream, slot->m_next)) return false;
    return streamPreviousProperties(system, tree, stream, slot->m_previous);
}

bool streamNextProperties(System* system, Tree* tree, Stream* stream, TreeNode*& slot)   // 0x83CAE0
{
    TreeNode* node = slot;
    if (!node) return true;
    if (!node->streamProperties(system, stream)) return false;
    if (!streamNodeProperties(system, tree, stream, slot->m_child)) return false;
    return streamNextProperties(system, tree, stream, slot->m_next);
}

bool streamPreviousProperties(System* system, Tree* tree, Stream* stream, TreeNode*& slot) // 0x83CF80
{
    TreeNode* node = slot;
    if (!node) return true;
    if (!node->streamProperties(system, stream)) return false;
    if (!streamNodeProperties(system, tree, stream, slot->m_child)) return false;
    return streamPreviousProperties(system, tree, stream, slot->m_previous);
}

bool saveNext(System*, Tree*, Stream*, TreeNode*);
bool savePrevious(System*, Tree*, Stream*, TreeNode*);

bool saveNode(System* system, Tree* tree, Stream* stream, TreeNode* node)              // 0x83CC70
{
    bool present = node != nullptr;
    if (!stream->streamBool(&present, kPresentName)) return false;
    if (!present) return true;
    if (!node->visit0x38(system, stream)) return false;
    if (!saveNode(system, tree, stream, node->m_child)) return false;
    if (!saveNext(system, tree, stream, node->m_next)) return false;
    return savePrevious(system, tree, stream, node->m_previous);
}

bool saveNext(System* system, Tree* tree, Stream* stream, TreeNode* node)              // 0x83C9B0
{
    bool present = node != nullptr;
    if (!stream->streamBool(&present, kPresentName)) return false;
    if (!present) return true;
    if (!node->visit0x38(system, stream)) return false;
    if (!saveNode(system, tree, stream, node->m_child)) return false;
    return saveNext(system, tree, stream, node->m_next);
}

bool savePrevious(System* system, Tree* tree, Stream* stream, TreeNode* node)          // 0x83CD10
{
    bool present = node != nullptr;
    if (!stream->streamBool(&present, kPresentName)) return false;
    if (!present) return true;
    if (!node->visit0x38(system, stream)) return false;
    if (!saveNode(system, tree, stream, node->m_child)) return false;
    return savePrevious(system, tree, stream, node->m_previous);
}

bool loadNext(System*, Tree*, Stream*, TreeNode*&, TreeNode*);
bool loadPrevious(System*, Tree*, Stream*, TreeNode*&, TreeNode*);

// [CHOSEN, not recovered] The load walkers' present flag is not initialized in the image (it reuses a
// byte of the stream argument's slot), so a stream that leaves it untouched decides on garbage; it
// starts false here.

// 0x83CDA0: a child of `parent` (null for the root).
bool loadNode(System* system, Tree* tree, Stream* stream, TreeNode*& slot, TreeNode* parent)
{
    slot = nullptr;
    bool present = false;
    if (!stream->streamBool(&present, kPresentName)) return false;
    if (!present) return true;
    slot = tree->createNode(system, parent, nullptr, nullptr, nullptr);
    if (!slot->visit0x34(system, stream)) return false;
    if (!loadNode(system, tree, stream, slot->m_child, slot)) return false;
    if (!loadNext(system, tree, stream, slot->m_next, slot)) return false;
    return loadPrevious(system, tree, stream, slot->m_previous, slot);
}

// 0x83CA40: the next sibling of `previous`.
bool loadNext(System* system, Tree* tree, Stream* stream, TreeNode*& slot, TreeNode* previous)
{
    slot = nullptr;
    bool present = false;
    if (!stream->streamBool(&present, kPresentName)) return false;
    if (!present) return true;
    slot = tree->createNode(system, previous->m_parent, nullptr, previous, nullptr);
    if (!slot->visit0x34(system, stream)) return false;
    if (!loadNode(system, tree, stream, slot->m_child, slot)) return false;
    return loadNext(system, tree, stream, slot->m_next, slot);
}

// 0x83CE60: the previous sibling of `next`.
bool loadPrevious(System* system, Tree* tree, Stream* stream, TreeNode*& slot, TreeNode* next)
{
    slot = nullptr;
    bool present = false;
    if (!stream->streamBool(&present, kPresentName)) return false;
    if (!present) return true;
    slot = tree->createNode(system, next->m_parent, next, nullptr, nullptr);
    if (!slot->visit0x34(system, stream)) return false;
    if (!loadNode(system, tree, stream, slot->m_child, slot)) return false;
    return loadPrevious(system, tree, stream, slot->m_previous, slot);
}

} // namespace

// ---- TreeNode ----

TreeNode::TreeNode(TreeNode* parent, TreeNode* next, TreeNode* previous, TreeNode* child)
    : m_previous(previous), m_next(next), m_child(child), m_parent(parent)
{
}
TreeNode::~TreeNode() {}
bool TreeNode::visit0x2c(System* system, Stream* stream) { return streamProperties(system, stream); }
bool TreeNode::visit0x34(System* system, Stream* stream) { return streamProperties(system, stream); }
bool TreeNode::visit0x38(System* system, Stream* stream) { return streamProperties(system, stream); }

// ---- Tree ----

Tree::Tree() : m_root(nullptr), m_name(nullptr) {}
Tree::~Tree() { ::operator delete(m_name); m_name = nullptr; }   // the nodes are not freed here
// 0x83D010: published on first use; the only caller is Stream::streamTree's null-type branch (0x837F80).
TypeDescriptor* Tree::getClassType()
{
    static ClassTypeDescriptor<Tree, Object> type("dice::meme::Tree");
    return &type;
}
TypeDescriptor* Tree::getType() { return getClassType(); }
unsigned int Tree::getObjectSize() { return sizeof(Tree); }
const char* Tree::getName() { return m_name; }
bool Tree::setName(const char* name)
{
    ::operator delete(m_name);
    m_name = nullptr;
    m_name = cloneTreeName(name);       // freed before the copy, as shipped: self-aliasing is unsafe
    return true;
}
bool Tree::slot_0x20() { return true; }

bool Tree::visit0x2c(System* system, Stream* stream)
{
    if (!Object::visit0x2c(system, stream)) return false;    // 0x82AC70: this->streamProperties
    return destroyNodes(system, this, stream, m_root);
}

bool Tree::streamProperties(System* system, Stream* stream)
{
    if (!Object::streamProperties(system, stream)) return false;   // 0x4553F0: true
    return streamNodeProperties(system, this, stream, m_root);
}

bool Tree::visit0x34(System* system, Stream* stream)
{
    return loadNode(system, this, stream, m_root, nullptr);
}

bool Tree::visit0x38(System* system, Stream* stream)
{
    return saveNode(system, this, stream, m_root);
}

TreeNode* Tree::slot_0x40(System*, void*, bool* found, void*)
{
    *found = false;
    return nullptr;
}

TreeNode* Tree::createNode(System*, TreeNode* parent, TreeNode* next, TreeNode* previous, TreeNode* child)
{
    void* allocation = ::operator new(sizeof(TreeNode));          // 0x737D60(0x14)
    return allocation ? new(allocation) TreeNode(parent, next, previous, child) : nullptr;
}

void Tree::deleteNode(System*, TreeNode*& node)
{
    if (node)
        delete node;
    node = nullptr;
}

#if defined(_M_IX86)
static_assert(sizeof(Tree) == 12, "BFMeme Tree layout");
static_assert(sizeof(TreeNode) == 0x14, "BFMeme TreeNode layout");
#endif

} }
#endif
