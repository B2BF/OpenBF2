// Shared path containers, node records and reusable A* search resources.

#ifndef AI_AIDLL_AI_MAIN_AIPATHFINDINGSEARCHTYPES_H
#define AI_AIDLL_AI_MAIN_AIPATHFINDINGSEARCHTYPES_H

#include <list>
#include <set>
#include <utility>
#include <vector>

#include "IAIPathfinding.h"     // ai::triple, and Vec3 through AIDllTypes.h
#include "CommonSingletonServer.h"                           // PathfindingEngine::update's clock
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

// Navigation path result. The node vector holds one (polygon, entered-from polygon, entry point)
// triple per step, in travel order.
class QTPath
{
public:
    QTPath();
    ~QTPath();

    // [INFERRED] Inline accessors as calculateCostOfPath reads them: an index outside the vector
    // (signed compare) yields UINT_MAX and a null position.
    unsigned int getPolygon(unsigned int index) const
    {
        return static_cast<int>(index) < static_cast<int>(m_nodes.size())
                   ? m_nodes[index].first
                   : 0xFFFFFFFFu;
    }
    const Vec3* getPosition(unsigned int index) const
    {
        return static_cast<int>(index) < static_cast<int>(m_nodes.size()) ? &m_nodes[index].third
                                                                           : 0;
    }

    // m_i0 and m_i20 keep their placeholder names because AIBot code already writes them.
    /*  +0 */ int m_i0;                                       // = 0; [INFERRED] the first step still ahead
    /*  +4 */ std::vector<triple<unsigned int, unsigned int, Vec3> > m_nodes;
    /* +16 */ int m_island;                                   // = -1; internalLocalSearch stores the quad tree
    /* +20 */ float m_i20;                                    // = 0; the path cost: buildPath copies the goal's f
};

// Cluster-level path result: (cluster, entry portal) pairs in travel order, for one map, island
// and cluster cost type.
class ClusterNodePath
{
public:
    // +4 .. +15 deliberately not initialised - the constructor steps over them.
    ClusterNodePath();
    ~ClusterNodePath();

    // Stores the map and looks up the vehicle's cluster cost type for it.
    void setVehicle(int map, int vehicleId);
    // The entry portal of step `index`: its position and polygon. False past the end or for the
    // start step (no entry portal).
    bool getPoint(Vec3& position, unsigned int& polygon, unsigned int index) const;
    // The entry portal position of step m_i0 + `index`, or Vec3::getZero().
    const Vec3& getPoint(unsigned int index) const;

    /*  +0 */ int m_i0;                                       // = 0; the current step (BotNavigation resets it)
    /*  +4 */ int m_map;
    /*  +8 */ int m_costType;                                 // IAIPathfinding::getClusterCostType
    /* +12 */ int m_island;                                   // clusterSearch writes it
    /* +16 */ std::vector<std::pair<unsigned short, unsigned short> > m_nodes;
};

// What QTSearchNodeManager::getNeighbourOfNode found for a neighbour. [INFERRED] enumerator
// names; the values are those the search switches on.
namespace nodeStatus {
enum Status
{
    kUnvisited = 0,     // new: costed and to be opened
    kOpen = 1,
    kClosed = 2,
    kGoal = 3,          // the goal (or an additional goal) polygon
    kInvalid = 4        // no further edge, impassable material or a disallowed cluster
};
} // namespace nodeStatus

// Search-node tree record. `m_left` doubles as the free-list link, and `flatten` threads a whole
// tree through it.
template <class T>
class BSTNode
{
public:
    BSTNode()
        : m_data(0)
    {
        // +4 .. +12 deliberately not initialised: only +0 is written; insert sets the children and
        // the tree's free list writes +8.
    }

    /*  +0 */ T* m_data;
    /*  +4 */ BSTNode<T>* m_parent;
    /*  +8 */ BSTNode<T>* m_left;     // smaller f cost; also the free-list link
    /* +12 */ BSTNode<T>* m_right;    // equal or larger f cost
};

// The A* open list: an unbalanced binary tree ordered by the nodes' f cost, with its minimum
// cached. Removed tree records go to a free list and are reused.
template <class Node, class SearchNode>
class BinarySearchTree
{
public:
    explicit BinarySearchTree(int n)
        : m_count(0)
        , m_free(0)
        , m_root(0)
    {
        // m_min deliberately not initialised: the constructor writes +0, +4 and +12 only.
        for (int i = n; i > 0; --i)
        {
            BSTNode<SearchNode>* const node = new BSTNode<SearchNode>();
            node->m_left = m_free;
            m_free = node;
        }
    }

    ~BinarySearchTree();

    void insert(SearchNode* data);
    SearchNode* pop();
    SearchNode* remove(SearchNode* data);
    BSTNode<SearchNode>* get(SearchNode* data, BSTNode<SearchNode>* node);
    void clear();

    /*  +0 */ int m_count;
    /*  +4 */ BSTNode<SearchNode>* m_free;
    /*  +8 */ BSTNode<SearchNode>* m_min;
    /* +12 */ BSTNode<SearchNode>* m_root;

private:
    void insert(BSTNode<SearchNode>* at, BSTNode<SearchNode>* node);
    void merge(BSTNode<SearchNode>* at, BSTNode<SearchNode>* node);
    BSTNode<SearchNode>* getLeftmostChildOfTree(BSTNode<SearchNode>* node) const;
    void flatten(BSTNode<SearchNode>* node, BSTNode<SearchNode>*& last);
};

// The hash key of a stored element: a search node's own key. ClusterNodeManager's status
// records overload it (their key is their node's), found by argument-dependent lookup.
template <class SearchNode>
inline auto hashKeyOf(const SearchNode* node) -> decltype(node->m_key)
{
    return node->m_key;
}

// [INFERRED] Helper names. QTSearchNode uses signed division, then adds the capacity for a
// negative key (even when its remainder is zero). ClusterNode uses unsigned division instead.
inline int hashIndexForKey(int key, int capacity)
{
    int index = key % capacity;
    if (key < 0)
        index += capacity;
    return index;
}

inline int hashIndexForKey(unsigned int key, int capacity)
{
    return hashIndexForKey(static_cast<int>(key), capacity);
}

inline int hashIndexForKey(unsigned short key, int capacity)
{
    return static_cast<unsigned int>(key) % static_cast<unsigned int>(capacity);
}

// CPNode overloads this with its cover position's index (no modulus), and hashKeyEquals with
// its cover position pointer. The hooks are found by argument-dependent lookup.
template <class SearchNode>
inline int hashIndexOf(const SearchNode* node, int capacity)
{
    return hashIndexForKey(hashKeyOf(node), capacity);
}

// [INFERRED] Whether two stored elements carry the same key; see hashIndexOf.
template <class SearchNode>
inline bool hashKeyEquals(const SearchNode* a, const SearchNode* b)
{
    return hashKeyOf(a) == hashKeyOf(b);
}

// One hash bucket. Non-empty buckets are threaded on a doubly linked "used" list so clearing and
// iteration only visit those.
template <class SearchNode>
struct HashBucket
{
    // Inlined in Hash's array construction (ELF 0x3B1EB0, also 0x35B3A0).
    HashBucket()
        : m_nextUsed(0), m_prevUsed(0), m_head(0)
    {
    }

    HashBucket* m_nextUsed;
    HashBucket* m_prevUsed;
    SearchNode* m_head;
};

template <class Node, class NodeManager, class SearchNode>
class Hash;

// Forward iterator over a Hash: the bucket, the node in it, and the owning hash.
template <class Node, class NodeManager, class SearchNode>
struct HashIterator
{
    HashBucket<SearchNode>* m_bucket;
    SearchNode* m_node;
    Hash<Node, NodeManager, SearchNode>* m_hash;

    bool operator==(const HashIterator& other) const
    {
        return m_bucket == other.m_bucket && m_node == other.m_node;
    }
    bool operator!=(const HashIterator& other) const { return !(*this == other); }

    // ELF 0x321E00 (the OCData instantiation): the next node in the bucket, else the next used
    // bucket's head.
    HashIterator& operator++()
    {
        m_node = m_node->m_next;
        if (m_node == 0)
        {
            m_bucket = m_bucket->m_nextUsed;
            if (m_bucket != 0)
                m_node = m_bucket->m_head;
        }
        return *this;
    }
};

// Intrusive chained hash of search nodes keyed by Node::m_key, chained through the nodes' own
// m_next / m_prev words. The modulus is the constructor's n; n + 1 buckets are allocated.
template <class Node, class NodeManager, class SearchNode>
class Hash
{
public:
    typedef HashIterator<Node, NodeManager, SearchNode> iterator;

    // ELF instances: 0x3B1CD0 (QTSearchNode), 0x3B1EB0 (ClusterNode), 0x321D60
    // (ClusterNodeManager::OCData), 0x35B3A0 (CPNode).
    explicit Hash(unsigned int n)
        : m_buckets(new HashBucket<SearchNode>[n + 1])
        , m_used(0)
        , m_count(0)
        , m_capacity((int)n)
        , m_nodeManager(0)
    {
        // The bucket constructors run before the fields above are initialized. m_begin's
        // bucket and node are not written here; begin() fills them.
        m_end.m_bucket = 0;
        m_end.m_node = 0;
        m_end.m_hash = this;
        m_begin.m_hash = this;
    }

    void insert(SearchNode* node);
    SearchNode* get(SearchNode* node) const;
    void remove(SearchNode* node);

    // Inlined in every retail user: empties the buckets on the used list without touching the
    // nodes, which the node manager owns.
    void clear()
    {
        m_count = 0;
        for (HashBucket<SearchNode>* bucket = m_used; bucket != 0; bucket = bucket->m_nextUsed)
            bucket->m_head = 0;
        m_used = 0;
    }

    // Inlined in handleOpenCacheUnderflow (ELF 0x35A5E0 / 0x3B0D00): refreshes the cached
    // begin iterator even when empty.
    iterator begin()
    {
        if (m_count == 0)
            m_begin = m_end;
        else
        {
            m_begin.m_bucket = m_used;
            m_begin.m_node = m_used->m_head;
        }
        return m_begin;
    }
    const iterator& end() const { return m_end; }

    /*  +0 */ iterator m_begin;
    /* +12 */ iterator m_end;
    /* +24 */ HashBucket<SearchNode>* m_buckets;
    /* +28 */ HashBucket<SearchNode>* m_used;
    /* +32 */ int m_count;
    /* +36 */ int m_capacity;
    /* +40 */ NodeManager* m_nodeManager;    // = 0, written by the owner right after construction

private:
    // The bucket of a node (hashIndexOf).
    HashBucket<SearchNode>* bucketOf(const SearchNode* node) const
    {
        return &m_buckets[hashIndexOf(node, m_capacity)];
    }
};

// Orders search nodes by f cost for handleOpenCacheUnderflow's multiset. Retail defines it in an
// anonymous namespace of AIPathfinding.cpp (attested by the multiset's mangled names); it is here
// because the templates that use it are defined in this header.
template <class SearchNode>
struct NodeCompare
{
    bool operator()(const SearchNode* a, const SearchNode* b) const { return a->m_f < b->m_f; }
};

namespace astarStatus {
// Values of AStarResource::m_status. [INFERRED] names; the values are the ones the search writes.
enum
{
    kIdle = 0,
    kSearching = 1,
    kFound = 2,
    kFailed = 3,
    kInvalid = 4      // localSearch's answer for an unusable request; never stored
};
} // namespace astarStatus

// State for one in-flight A* search. Its node manager and hash tables belong to the search resource.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
class AStarResource
{
public:
    explicit AStarResource(NodeManager* nodeManager)
        : m_status(0)
        , m_maxIterations(0)
        , m_maxSearchDepth(-1)
        , m_iterations(0)
        , m_openSize(0)
        , m_hashSize(0)
        , m_owner(-1)
        , m_nodeManager(nodeManager)
        , m_worstOpen(0)
        , m_open(0)
        , m_openOverflow(0)
        , m_closed(0)
        , m_goal(0)
    {
        // m_i4 deliberately not initialised - see above.
    }

    // Runs A* steps until the deadline, the goal, exhaustion or a limit.
    void update(double deadline);
    void pushOpenNode(SearchNode* node);
    SearchNode* popOpenNode();
    void handleOpenCacheUnderflow();

    /*  +0 */ int m_status;          // astarStatus; PathfindingEngine::getStatus reads this word
    /*  +4 */ int m_i4;              // never written
    /*  +8 */ int m_maxIterations;   // 0 = no limit (the counter never equals it)
    /* +12 */ int m_maxSearchDepth;  // -1 = no limit
    /* +16 */ int m_iterations;      // closed nodes this search
    /* +20 */ int m_openSize;        // the tree's capacity before nodes overflow into the hash
    /* +24 */ int m_hashSize;        // = the int handed to both Hash constructors
    /* +28 */ int m_owner;           // = -1 free; acquireResource stores the caller's owner id
    /* +32 */ NodeManager* m_nodeManager;
    /* +36 */ SearchNode* m_worstOpen;   // the largest f cost refilled into the tree
    /* +40 */ BinarySearchTree<Node, SearchNode>* m_open;
    /* +44 */ Hash<Node, NodeManager, SearchNode>* m_openOverflow;
    /* +48 */ std::vector<SearchNode*> m_v48;   // [UNRESOLVED] handleOpenCacheUnderflow only resizes it to 0
    /* +60 */ Hash<Node, NodeManager, SearchNode>* m_closed;
    /* +64 */ SearchNode* m_goal;    // the goal node once found; buildPath starts from it
};

// Pathfinding engine and its pool of reusable search resources.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
class PathfindingEngine
{
public:
    PathfindingEngine() {}
    // ELF 0x3AE720 (QTSearchNode), 0x3AE930 (ClusterNode), 0x3593F0 (CPNode); weak, one per
    // instantiation, identical apart from the node manager's calls.
    ~PathfindingEngine();

    // [INFERRED] Steps active searches round-robin until now + budget, at most once per
    // search per call, moving each processed search from front to back.
    void update(double budget);

    /*  +0 */ std::vector<AStarResource<Node, NodeManager, SearchNode, Key, Path>*> m_searches;
    /* +12 */ std::list<unsigned int> m_available;    // [INFERRED] the free list
    /* +16 */ std::list<unsigned int> m_inFlight;     // [INFERRED] - isLockComplete waits on this one
};

// The timer is fetched once; the in-flight count once, before the pass (libstdc++'s O(n) size).
// The walk keeps its own iterator but always erases the front and re-appends its id, which is the
// same element on every step because each processed id moves to the back. The deadline test comes
// before the count test in both images.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void PathfindingEngine<Node, NodeManager, SearchNode, Key, Path>::update(double budget)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double deadline = timer->getUpdateTime() + budget;
    const unsigned int count = static_cast<unsigned int>(m_inFlight.size());
    unsigned int processed = 0;
    std::list<unsigned int>::iterator it = m_inFlight.begin();
    while (deadline > timer->getUpdateTime() && processed < count)
    {
        ++processed;
        const unsigned int id = *it;
        m_searches[id]->update(deadline);
        ++it;
        m_inFlight.erase(m_inFlight.begin());
        m_inFlight.insert(m_inFlight.end(), id);
    }
}

// Every search resource is first returned and reset inline (the releaseResource and
// internalLocalSearch sequences, in that order), then its parts are deleted: node manager, open
// tree, both hashes (bucket array first: Hash has no destructor of its own, as in
// ~ClusterNodeManager), and the resource itself, whose only implicit member is m_v48. Null slots
// are skipped. The bound is re-read from the vector on every step. The two lists and the vector
// are then destroyed in reverse declaration order (+16, +12, +0).
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
PathfindingEngine<Node, NodeManager, SearchNode, Key, Path>::~PathfindingEngine()
{
    for (unsigned int i = 0; i < m_searches.size(); ++i)
    {
        AStarResource<Node, NodeManager, SearchNode, Key, Path>* const search = m_searches[i];
        if (search == 0)
            continue;

        search->m_maxIterations = 0;
        search->m_maxSearchDepth = -1;
        search->m_nodeManager->returnReset();
        search->m_owner = -1;
        search->m_iterations = 0;
        search->m_status = astarStatus::kIdle;
        search->m_worstOpen = 0;
        search->m_open->clear();
        search->m_openOverflow->clear();
        search->m_closed->clear();
        search->m_nodeManager->reset();

        delete search->m_nodeManager;
        delete search->m_open;
        if (search->m_openOverflow != 0)
        {
            if (search->m_openOverflow->m_buckets != 0)
                delete[] search->m_openOverflow->m_buckets;
            delete search->m_openOverflow;
        }
        if (search->m_closed != 0)
        {
            if (search->m_closed->m_buckets != 0)
                delete[] search->m_closed->m_buckets;
            delete search->m_closed;
        }
        delete search;
    }
}

// BinarySearchTree. Instantiation addresses are the QTSearchNode ones in the ELF.

// ELF 0x3B0750 (QTSearchNode), 0x35A490 (CPNode), 0x3B07C0 (ClusterNode).
// Pops every element, then frees the record pool.
template <class Node, class SearchNode>
BinarySearchTree<Node, SearchNode>::~BinarySearchTree()
{
    while (m_count > 0)
        pop();
    while (m_free != 0)
    {
        BSTNode<SearchNode>* const node = m_free;
        m_free = node->m_left;
        delete node;
    }
}

// ELF 0x3AF950 (QTSearchNode), 0x35A230 (CPNode), 0x3AFB20 (ClusterNode).
// Equal costs go right; the cached minimum moves only to a strictly cheaper node.
template <class Node, class SearchNode>
void BinarySearchTree<Node, SearchNode>::insert(SearchNode* data)
{
    BSTNode<SearchNode>* node = m_free;
    if (node != 0)
        m_free = node->m_left;
    else
        node = new BSTNode<SearchNode>();
    node->m_left = 0;
    node->m_right = 0;
    node->m_data = data;

    if (m_count == 0)
    {
        m_min = node;
        m_root = node;
        node->m_parent = 0;
        ++m_count;
        return;
    }

    BSTNode<SearchNode>* const root = m_root;
    if (root->m_data->m_f > data->m_f)
    {
        if (root->m_left == 0)
        {
            root->m_left = node;
            node->m_parent = root;
        }
        else
            insert(root->m_left, node);
    }
    else
    {
        if (root->m_right == 0)
        {
            root->m_right = node;
            node->m_parent = root;
        }
        else
            insert(root->m_right, node);
    }

    if (m_min->m_data->m_f > node->m_data->m_f)
        m_min = node;
    ++m_count;
}

// ELF 0x3B2BD0 (QTSearchNode), 0x35BA00 (CPNode), 0x3B2C40 (ClusterNode).
// Descends from `at` to a free child slot.
template <class Node, class SearchNode>
void BinarySearchTree<Node, SearchNode>::insert(BSTNode<SearchNode>* at, BSTNode<SearchNode>* node)
{
    for (;;)
    {
        if (at->m_data->m_f > node->m_data->m_f)
        {
            if (at->m_left == 0)
            {
                at->m_left = node;
                break;
            }
            at = at->m_left;
        }
        else
        {
            if (at->m_right == 0)
            {
                at->m_right = node;
                break;
            }
            at = at->m_right;
        }
    }
    node->m_parent = at;
}

// ELF 0x3B2CB0 (QTSearchNode), 0x35BA70 (CPNode), 0x3B2D00 (ClusterNode).
// insert with a null subtree accepted.
template <class Node, class SearchNode>
void BinarySearchTree<Node, SearchNode>::merge(BSTNode<SearchNode>* at, BSTNode<SearchNode>* node)
{
    if (node == 0)
        return;
    insert(at, node);
}

// ELF 0x3B2C10 (QTSearchNode), 0x35BA40 (CPNode), 0x3B2C80 (ClusterNode).
template <class Node, class SearchNode>
BSTNode<SearchNode>*
BinarySearchTree<Node, SearchNode>::getLeftmostChildOfTree(BSTNode<SearchNode>* node) const
{
    BSTNode<SearchNode>* leftmost = 0;
    while (node != 0)
    {
        leftmost = node;
        node = node->m_left;
    }
    return leftmost;
}

// ELF 0x3AFA40 (QTSearchNode), 0x35A320 (CPNode), 0x3AFC10 (ClusterNode).
// Removes and returns the cheapest node, or null when empty.
template <class Node, class SearchNode>
SearchNode* BinarySearchTree<Node, SearchNode>::pop()
{
    BSTNode<SearchNode>* const min = m_min;
    if (min == 0)
        return 0;

    --m_count;
    if (min->m_right != 0)
    {
        if (min->m_parent != 0)
        {
            min->m_parent->m_left = min->m_right;
            min->m_right->m_parent = min->m_parent;
            m_min = min->m_right->m_left != 0 ? getLeftmostChildOfTree(min->m_right->m_left)
                                              : min->m_right;
        }
        else
        {
            m_root = min->m_right;
            min->m_right->m_parent = 0;
            m_min = getLeftmostChildOfTree(min->m_right);
        }
    }
    else
    {
        m_min = min->m_parent;
        if (m_min != 0)
            m_min->m_left = 0;
        else
        {
            m_min = min->m_right;
            m_root = min->m_right;
            if (m_min != 0)
            {
                m_min->m_parent = 0;
                while (m_min->m_left != 0)
                    m_min = m_min->m_left;
            }
        }
    }

    min->m_left = m_free;
    m_free = min;
    return min->m_data;
}

// ELF 0x3B2910 (QTSearchNode), 0x35B900 (CPNode), 0x3B2AD0 (ClusterNode).
// Depth-first by key; the left subtree recursively, the right iteratively.
template <class Node, class SearchNode>
BSTNode<SearchNode>* BinarySearchTree<Node, SearchNode>::get(SearchNode* data,
                                                             BSTNode<SearchNode>* node)
{
    while (node != 0)
    {
        if (data->m_key == node->m_data->m_key)
            return node;
        BSTNode<SearchNode>* const found = get(data, node->m_left);
        if (found != 0)
            return found;
        node = node->m_right;
    }
    return 0;
}

// ELF 0x3B0B90 (QTSearchNode), 0x35A940 (CPNode), 0x3B1060 (ClusterNode).
// Removes the node with data's key. The cached minimum is only maintained through
// pop; a non-minimum removal leaves it alone. The removed node's left subtree is merged under its
// right one.
template <class Node, class SearchNode>
SearchNode* BinarySearchTree<Node, SearchNode>::remove(SearchNode* data)
{
    if (m_count == 0)
        return 0;
    if (m_min->m_data->m_key == data->m_key)
        return pop();

    BSTNode<SearchNode>* const node = get(data, m_root);
    if (node == 0)
        return 0;

    --m_count;
    BSTNode<SearchNode>* const parent = node->m_parent;
    if (parent != 0)
    {
        BSTNode<SearchNode>** const slot =
            parent->m_left == node ? &parent->m_left : &parent->m_right;
        if (node->m_right != 0)
        {
            *slot = node->m_right;
            node->m_right->m_parent = node->m_parent;
            merge(*slot, node->m_left);
        }
        else
        {
            *slot = node->m_left;
            if (node->m_left != 0)
                node->m_left->m_parent = node->m_parent;
        }
    }
    else if (node->m_right != 0)
    {
        m_root = node->m_right;
        node->m_right->m_parent = 0;
        merge(node->m_right, node->m_left);
    }
    else
    {
        // Retail does not test the left child here.
        m_root = node->m_left;
        node->m_left->m_parent = 0;
    }

    node->m_left = m_free;
    m_free = node;
    return node->m_data;
}

// ELF 0x3B2360 (QTSearchNode), 0x35B720 (CPNode), 0x3B25E0 (ClusterNode).
// Threads the subtree into one chain through m_left: the node, its left chain, then
// its right chain. `last` receives the chain's tail.
template <class Node, class SearchNode>
void BinarySearchTree<Node, SearchNode>::flatten(BSTNode<SearchNode>* node,
                                                 BSTNode<SearchNode>*& last)
{
    if (node->m_left == 0)
    {
        if (node->m_right == 0)
        {
            last = node;
            return;
        }
        flatten(node->m_right, last);
        node->m_left = node->m_right;
        return;
    }

    BSTNode<SearchNode>* leftLast;
    flatten(node->m_left, leftLast);
    if (node->m_right == 0)
    {
        last = leftLast;
        return;
    }
    flatten(node->m_right, last);
    leftLast->m_left = node->m_right;
}

// ELF 0x3B22F0 (QTSearchNode), 0x35B6B0 (CPNode), 0x3B2570 (ClusterNode).
// Returns every record to the free list in one splice.
template <class Node, class SearchNode>
void BinarySearchTree<Node, SearchNode>::clear()
{
    if (m_root == 0)
        return;
    BSTNode<SearchNode>* last;
    flatten(m_root, last);
    last->m_left = m_free;
    m_free = m_root;
    m_count = 0;
    m_root = 0;
    m_min = 0;
}

// Hash. Instantiation addresses are the QTSearchNode ones in the ELF.

// ELF 0x3B24B0 (QTSearchNode), 0x359B20 (CPNode), 0x3B2730 (ClusterNode).
// Pushes at the bucket head; an empty bucket joins the front of the used list.
template <class Node, class NodeManager, class SearchNode>
void Hash<Node, NodeManager, SearchNode>::insert(SearchNode* node)
{
    HashBucket<SearchNode>* const bucket = bucketOf(node);
    SearchNode* const head = bucket->m_head;
    if (head != 0)
        head->m_prev = node;
    else
    {
        bucket->m_nextUsed = m_used;
        if (m_used != 0)
            m_used->m_prevUsed = bucket;
        bucket->m_prevUsed = 0;
        m_used = bucket;
    }
    node->m_next = head;
    node->m_prev = 0;
    bucket->m_head = node;
    ++m_count;
}

// ELF 0x3B28B0 (QTSearchNode; also 0x3B2A80 ClusterNode, 0x321E40 ClusterNodeManager::OCData,
// 0x35B8C0 CPNode). The stored node with the same key, or null.
template <class Node, class NodeManager, class SearchNode>
SearchNode* Hash<Node, NodeManager, SearchNode>::get(SearchNode* node) const
{
    for (SearchNode* entry = bucketOf(node)->m_head; entry != 0; entry = entry->m_next)
    {
        if (hashKeyEquals(entry, node))
            return entry;
    }
    return 0;
}

// ELF 0x3B2970 (QTSearchNode), 0x35B960 (CPNode), 0x3B2B30 (ClusterNode).
// Unlinks a stored node; a bucket that empties leaves the used list.
template <class Node, class NodeManager, class SearchNode>
void Hash<Node, NodeManager, SearchNode>::remove(SearchNode* node)
{
    if (node->m_prev != 0)
        node->m_prev->m_next = node->m_next;
    else
    {
        HashBucket<SearchNode>* const bucket = bucketOf(node);
        bucket->m_head = node->m_next;
        if (bucket->m_head == 0)
        {
            if (m_used == bucket)
                m_used = bucket->m_nextUsed;
            else
                bucket->m_prevUsed->m_nextUsed = bucket->m_nextUsed;
            if (bucket->m_nextUsed != 0)
                bucket->m_nextUsed->m_prevUsed = bucket->m_prevUsed;
        }
    }
    --m_count;
    if (node->m_next != 0)
        node->m_next->m_prev = node->m_prev;
}

// AStarResource. Instantiation addresses are the QTSearchNode ones in the ELF.

// ELF 0x3B2410 (QTSearchNode), 0x35B7D0 (CPNode), 0x3B2690 (ClusterNode).
// Marks the node open. It goes into the tree only while the tree is non-empty and
// the node is cheaper than the worst node last refilled; a full tree is first flushed, cheapest
// first, into the overflow hash. Everything else goes straight to the overflow hash.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::pushOpenNode(SearchNode* node)
{
    m_nodeManager->setNodeOpen(node);
    if (m_open->m_count != 0 && m_worstOpen->m_f > node->m_f)
    {
        if (m_open->m_count >= m_openSize)
        {
            do
                m_openOverflow->insert(m_open->pop());
            while (m_open->m_count != 0);
        }
        m_open->insert(node);
        return;
    }
    m_openOverflow->insert(node);
}

// ELF 0x3B2860 (QTSearchNode), 0x35B870 (CPNode), 0x3B2A30 (ClusterNode).
// The cheapest open node, refilling the tree from the overflow hash when it is
// empty; null when both are empty.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
SearchNode* AStarResource<Node, NodeManager, SearchNode, Key, Path>::popOpenNode()
{
    for (;;)
    {
        if (m_open->m_count != 0)
            return m_open->pop();
        if (m_openOverflow->m_count == 0)
            return 0;
        handleOpenCacheUnderflow();
    }
}

// ELF 0x3B0830 (QTSearchNode), 0x35A5E0 (CPNode), 0x3B0D00 (ClusterNode).
// Moves the m_openSize cheapest overflow nodes into the tree and remembers the most
// expensive of them. The candidates are kept in a multiset by f cost; displacing the current worst
// erases it by key, which removes every candidate with that same cost (retail behaviour).
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::handleOpenCacheUnderflow()
{
    m_v48.resize(0);

    Hash<Node, NodeManager, SearchNode>* const hash = m_openOverflow;
    typename Hash<Node, NodeManager, SearchNode>::iterator it = hash->begin();
    if (it == hash->end())
        return;

    std::multiset<SearchNode*, NodeCompare<SearchNode> > best;
    SearchNode* worst = it.m_node;
    for (int taken = 0; taken < m_openSize && it != hash->end(); ++taken)
    {
        best.insert(it.m_node);
        if (it.m_node->m_f > worst->m_f)
            worst = it.m_node;
        ++it;
    }

    while (it != hash->end())
    {
        if (!(it.m_node->m_f > worst->m_f))
        {
            typename std::multiset<SearchNode*, NodeCompare<SearchNode> >::iterator last =
                best.end();
            --last;
            best.erase(*last);
            best.insert(it.m_node);
            ++it;
            last = best.end();
            --last;
            worst = *last;
        }
        else
            ++it;
    }

    for (typename std::multiset<SearchNode*, NodeCompare<SearchNode> >::iterator i = best.begin();
         i != best.end(); ++i)
    {
        m_openOverflow->remove(*i);
        m_open->insert(*i);
    }
    m_worstOpen = worst;
}

// ELF 0x3AEEA0 (QTSearchNode), 0x359BC0 (CPNode), 0x3AF400 (ClusterNode).
// One time slice of the search. Each popped node is expanded edge by edge (never
// back into its own parent); new neighbours are opened, cheaper routes to open or closed nodes
// re-open those, and reaching the goal closes the search. A node whose edges are exhausted is
// closed and counted against the iteration and depth limits.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::update(double deadline)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    if (m_status != astarStatus::kSearching)
        return;

    for (;;)
    {
        if (!(deadline > timer->getUpdateTime()))
            return;

        SearchNode* const node = popOpenNode();
        if (node == 0)
        {
            m_status = astarStatus::kFailed;
            return;
        }

        nodeStatus::Status status;
        Key edge;
        SearchNode* neighbour = node->expandFirst(m_nodeManager, status, edge);
        while (neighbour != 0 && node->m_parent != 0 && neighbour->m_key == node->m_parent->m_key)
        {
            m_nodeManager->recycleNode(neighbour);
            neighbour = node->expand(m_nodeManager, status, edge);
        }

        while (neighbour != 0)
        {
            switch (status)
            {
            case nodeStatus::kUnvisited:
                pushOpenNode(neighbour);
                break;

            case nodeStatus::kOpen:
            {
                SearchNode* existing = m_openOverflow->get(neighbour);
                if (existing == 0)
                {
                    BSTNode<SearchNode>* const record = m_open->get(neighbour, m_open->m_root);
                    existing = record != 0 ? record->m_data : 0;
                }
                if (m_nodeManager->updateNode(node, existing, m_maxSearchDepth))
                {
                    SearchNode* const hashed = m_openOverflow->get(neighbour);
                    if (hashed != 0)
                        m_openOverflow->remove(hashed);
                    else
                        m_open->remove(neighbour);
                    pushOpenNode(existing);
                }
                m_nodeManager->recycleNode(neighbour);
                break;
            }

            case nodeStatus::kClosed:
            {
                SearchNode* const existing = m_closed->get(neighbour);
                if (m_nodeManager->updateNode(node, existing, m_maxSearchDepth))
                {
                    m_closed->remove(existing);
                    pushOpenNode(existing);
                }
                m_nodeManager->recycleNode(neighbour);
                break;
            }

            case nodeStatus::kGoal:
                m_goal = neighbour;
                neighbour->m_parent = node;
                m_nodeManager->setNodeClosed(node);
                m_closed->insert(node);
                m_status = astarStatus::kFound;
                break;

            case nodeStatus::kInvalid:
                m_nodeManager->recycleNode(neighbour);
                break;

            default:
                break;
            }

            if (m_status == astarStatus::kFound)
                return;

            neighbour = node->expand(m_nodeManager, status, edge);
            while (neighbour != 0 && node->m_parent != 0 &&
                   neighbour->m_key == node->m_parent->m_key)
            {
                m_nodeManager->recycleNode(neighbour);
                neighbour = node->expand(m_nodeManager, status, edge);
            }
        }

        if (m_status == astarStatus::kFound)
            return;

        const int depth = node->m_depth;
        m_nodeManager->setNodeClosed(node);
        m_closed->insert(node);
        ++m_iterations;
        if (m_iterations == m_maxIterations || m_maxSearchDepth == depth)
        {
            m_status = astarStatus::kFailed;
            return;
        }
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIPATHFINDINGSEARCHTYPES_H
