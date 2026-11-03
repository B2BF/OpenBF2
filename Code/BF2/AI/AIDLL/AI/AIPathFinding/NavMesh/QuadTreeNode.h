// Navigation quad-tree node. File loading initializes its stored data.
// Generation subdivides the navigation polygons during QuadTree::buildQuadTree.

#ifndef AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QUADTREENODE_H
#define AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QUADTREENODE_H

#include "../../Main/AIDllTypes.h"
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

class MaterialManager;
class QuadTree;

class QuadTreeNode
{
public:
    QuadTreeNode();
    ~QuadTreeNode();

    // Copies the children, the leaf flag, the count and the first index; the two bytes at +0x12
    // are not copied.
    QuadTreeNode& operator=(const QuadTreeNode& other);
    bool create(QuadTree*, unsigned int&, const Vec2&, float, int,
                 const std::vector<unsigned int>&, int);
    bool polygonInside(const Vec2&, float, QuadTree*, unsigned int);
    int countPolygonsInside(const std::vector<unsigned int>&, QuadTree*, const Vec2&, float,
                            std::vector<unsigned int>&);

    bool findPolygon(const QuadTree*, const Vec2&, float, const Vec3&, unsigned int&,
                     MaterialManager*, float) const;
    // The debug twin of findPolygon: descends the same way and reads every corner of the leaf's
    // polygons without printing or returning anything.
    void printPolygonsInNode(const QuadTree*, const Vec2&, float, const Vec3&, unsigned int&,
                             MaterialManager*, float) const;

    // A branch stores four node-array indices, with zero meaning "no child". A leaf instead uses
    // the byte count and first index to address QuadTree::m_nodePolygons. The two bytes at +0x12
    // remain unread by this query.
    /* +0x00 */ unsigned int   m_children[4];
    /* +0x10 */ unsigned char  m_isLeaf;
    /* +0x11 */ unsigned char  m_polygonCount;
    /* +0x12 */ unsigned char  m_unread12[2];
    /* +0x14 */ unsigned int   m_firstPolygon;
};

typedef char QuadTreeNodeIsTheFileRecord[sizeof(QuadTreeNode) == 24 ? 1 : -1];

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QUADTREENODE_H
