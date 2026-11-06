// Navigation quad-tree node construction and polygon lookup.

#include "QuadTreeNode.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x313300, __tcf_0 0x313360, _GLOBAL__I 0x313390).
#include <iostream>

#include "QuadTree.h"
#include "../Material/MaterialManager.h"
#include "../../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x312900. Retail iterates to the end and ignores the requested value.
void checkVector(const std::vector<unsigned int>& values, unsigned int)
{
    for (std::vector<unsigned int>::const_iterator i = values.begin(); i != values.end(); ++i) {}
}

// ELF 0x312930. Reserves a branch before its children; empty children consume no node.
bool QuadTreeNode::create(QuadTree* tree, unsigned int& nodeCount, const Vec2& minimum,
    float size, int depth, const std::vector<unsigned int>& polygons, int count)
{
    static int maxPolygonCount = 20; // Original .data 0x4BF158.
    bool leaf = depth == tree->m_maxDepthInTree;
    std::vector<unsigned int> children[4];
    if (depth < tree->m_gtsBuildValue)
        leaf = false;
    if (!leaf)
        leaf = count <= tree->m_minPolygonsInLeaf;
    int counts[4];
    if (!leaf)
    {
        const float half = size * 0.5f;
        Vec2 childMinimum = minimum;
        counts[0] = countPolygonsInside(polygons, tree, childMinimum, half, children[0]);
        childMinimum.y += half;
        counts[1] = countPolygonsInside(polygons, tree, childMinimum, half, children[1]);
        childMinimum.x += half;
        childMinimum.y -= half;
        counts[2] = countPolygonsInside(polygons, tree, childMinimum, half, children[2]);
        childMinimum.y += half;
        counts[3] = countPolygonsInside(polygons, tree, childMinimum, half, children[3]);
        if (static_cast<float>(count) /
            static_cast<float>(counts[0] + counts[1] + counts[2] + counts[3]) < 0.5f &&
            count < maxPolygonCount)
            leaf = true;
    }
    if (leaf)
    {
        if (count == 0)
            return false;
        ++nodeCount;
        m_isLeaf = 1;
        // shipped defect: the count narrows to a byte, while all supplied polygons are copied.
        m_polygonCount = static_cast<unsigned char>(count);
        m_firstPolygon = static_cast<unsigned int>(tree->m_vector18.size());
        for (int i = 0; i < count; ++i)
            tree->m_vector18.push_back(polygons[i]);
        return true;
    }

    ++nodeCount;
    const float half = size * 0.5f;
    m_children[0] = nodeCount;
    if (!tree->m_nodes[nodeCount].create(tree, nodeCount, minimum, half, depth + 1,
                                        children[0], counts[0]))
        m_children[0] = 0;
    Vec2 childMinimum = {minimum.x, minimum.y + half};
    m_children[1] = nodeCount;
    if (!tree->m_nodes[nodeCount].create(tree, nodeCount, childMinimum, half, depth + 1,
                                        children[1], counts[1]))
        m_children[1] = 0;
    childMinimum.x += half;
    childMinimum.y -= half;
    m_children[2] = nodeCount;
    if (!tree->m_nodes[nodeCount].create(tree, nodeCount, childMinimum, half, depth + 1,
                                        children[2], counts[2]))
        m_children[2] = 0;
    childMinimum.y += half;
    m_children[3] = nodeCount;
    if (!tree->m_nodes[nodeCount].create(tree, nodeCount, childMinimum, half, depth + 1,
                                        children[3], counts[3]))
        m_children[3] = 0;
    return true;
}

// ELF 0x312850. Appends matches and returns the full resulting vector size.
int QuadTreeNode::countPolygonsInside(const std::vector<unsigned int>& polygons, QuadTree* tree,
    const Vec2& minimum, float size, std::vector<unsigned int>& output)
{
    for (std::vector<unsigned int>::const_iterator p = polygons.begin(); p != polygons.end(); ++p)
        if (polygonInside(minimum, size, tree, *p))
            output.push_back(*p);
    return static_cast<int>(output.size());
}

// ELF 0x312480. Tests polygon vertices, square corners, then all projected edge pairs.
bool QuadTreeNode::polygonInside(const Vec2& minimum, float size, QuadTree* tree,
                                 unsigned int polygon)
{
    const Vec3 square[4] = {{minimum.x, 0.0f, minimum.y},
        {minimum.x + size, 0.0f, minimum.y},
        {minimum.x + size, 0.0f, minimum.y + size},
        {minimum.x, 0.0f, minimum.y + size}};
    const Polygon* p = tree->m_polygons + polygon;
    const int count = p->vertexCount;
    for (int corner = 0; corner < count; ++corner)
    {
        const Vec3* v = tree->getVertexForPolygon(polygon, static_cast<unsigned char>(corner));
        if (minimum.x <= v->x && v->x <= minimum.x + size &&
            minimum.y <= v->z && v->z <= minimum.y + size)
            return true;
    }
    for (int corner = 0; corner < 4; ++corner)
    {
        bool inside = true;
        for (int edge = 0; edge < count && inside; ++edge)
        {
            const Vec3* a = tree->getVertexForPolygon(polygon, static_cast<unsigned char>(edge));
            const Vec3* b = tree->getSafeVertexForPolygon(polygon,
                static_cast<unsigned char>(edge + 1));
            const Vec3 direction = {b->x - a->x, 0.0f, b->z - a->z};
            const Vec3 offset = {square[corner].x - a->x, 0.0f, square[corner].z - a->z};
            const Vec3 cross = {0.0f * direction.z - offset.z * 0.0f,
                offset.z * direction.x - direction.z * offset.x,
                offset.x * 0.0f - direction.x * 0.0f};
            inside = cross.x * p->m_normal.x + cross.y * p->m_normal.y +
                     cross.z * p->m_normal.z < 0.0f;
        }
        if (inside)
            return true;
    }
    for (int edge = 0; edge < count; ++edge)
    {
        Vec3 a = *tree->getVertexForPolygon(polygon, static_cast<unsigned char>(edge));
        const Vec3* next = tree->getSafeVertexForPolygon(polygon,
            static_cast<unsigned char>(edge + 1));
        Vec3 b = {next->x, 0.0f, next->z};
        a.y = 0.0f;
        for (int side = 0; side < 4; ++side)
            if (lineSegmentIntersect(a, b, square[side], square[(side + 1) % 4]))
                return true;
    }
    return false;
}

// ELF 0x312420 (C2; C1 0x312440). The children are left unset.
QuadTreeNode::QuadTreeNode()
    : m_isLeaf(0)
    , m_polygonCount(0)
    , m_firstPolygon(0)
{
}

// ELF 0x312460 (D2; D1 0x312470).
QuadTreeNode::~QuadTreeNode()
{
}

// ELF 0x3132C0.
QuadTreeNode& QuadTreeNode::operator=(const QuadTreeNode& other)
{
    for (int i = 0; i < 4; ++i)
        m_children[i] = other.m_children[i];
    m_isLeaf = other.m_isLeaf;
    m_polygonCount = other.m_polygonCount;
    m_firstPolygon = other.m_firstPolygon;
    return *this;
}

// ELF 0x312DD0. A branch selects exactly one quadrant. Child index zero means absent; non-zero indices address
// QuadTree::m_nodes directly. Equality belongs to the positive-X / positive-Z halves.
// A leaf examines its byte-counted slice of m_nodePolygons. It rejects downward/vertical polygon
// normals and negative material costs, then keeps the candidate with the smallest squared vertical
// projection distance. Equal distances retain the first polygon. The output is untouched when no
// candidate succeeds. `materials` is deliberately unguarded on the leaf path.
bool QuadTreeNode::findPolygon(const QuadTree* tree, const Vec2& minimum, float size,
                               const Vec3& point, unsigned int& polygon,
                               MaterialManager* materials, float allowedHeightDiff) const
{
    if (!m_isLeaf)
    {
        const float half = size * 0.5f;
        const float middleX = minimum.x + half;
        const float middleZ = minimum.y + half;

        unsigned int child;
        Vec2 childMinimum;

        if (middleX > point.x)
        {
            childMinimum.x = minimum.x;
            if (middleZ > point.z)
            {
                childMinimum.y = minimum.y;
                child = m_children[0];
            }
            else
            {
                childMinimum.y = middleZ;
                child = m_children[1];
            }
        }
        else
        {
            childMinimum.x = middleX;
            if (middleZ > point.z)
            {
                childMinimum.y = minimum.y;
                child = m_children[2];
            }
            else
            {
                childMinimum.y = middleZ;
                child = m_children[3];
            }
        }

        if (!child)
            return false;

        return tree->m_nodes[child].findPolygon(tree, childMinimum, half, point, polygon,
                                                materials, allowedHeightDiff);
    }

    Polygon* best = 0;
    unsigned int bestPolygon = 0;
    float bestDistance = 100000000.0f;

    for (unsigned int i = 0; i < m_polygonCount; ++i)
    {
        const unsigned int candidateIndex = tree->m_nodePolygons[m_firstPolygon + i];
        Polygon* const candidate = &tree->m_polygons[candidateIndex];

        // Both shipped comparisons are rejection tests. Their unordered (NaN) case therefore
        // continues into the geometry test: reject normal.y <= 0, then reject cost < 0.
        if (!(candidate->m_normal.y <= 0.0f)
            && !(materials->getMaterialCost(candidate->m_material) < 0.0f))
        {
            float distanceSquared = 0.0f;
            if (tree->withinPolygon(candidate, point, distanceSquared, allowedHeightDiff)
                && distanceSquared < bestDistance)
            {
                bestDistance = distanceSquared;
                best = candidate;
                bestPolygon = candidateIndex;
            }
        }
    }

    if (!best)
        return false;

    polygon = bestPolygon;
    return true;
}

// ELF 0x313080. Descends like findPolygon (child index zero ends the walk); a leaf only reads
// each corner of its polygons through getVertexForPolygon. The child's minimum is the parent's
// plus a 0 / half offset per axis (the zero offsets are real additions in the listing), except
// for the first quadrant, which passes the parent's.
void QuadTreeNode::printPolygonsInNode(const QuadTree* tree, const Vec2& minimum, float size,
                                       const Vec3& point, unsigned int& polygon,
                                       MaterialManager* materials, float allowedHeightDiff) const
{
    if (m_isLeaf)
    {
        const unsigned char count = m_polygonCount;
        for (int i = 0; i < count; ++i)
        {
            const unsigned int candidate = tree->m_nodePolygons[m_firstPolygon + i];
            const Polygon& candidatePolygon = tree->m_polygons[candidate];
            for (int corner = 0; corner < candidatePolygon.vertexCount; ++corner)
                tree->getVertexForPolygon(candidate, static_cast<unsigned char>(corner));
        }
        return;
    }

    const float half = size * 0.5f;
    Vec2 childMinimum;
    const Vec2* next = &childMinimum;
    unsigned int child;

    if (minimum.x + half > point.x)
    {
        if (minimum.y + half > point.z)
        {
            child = m_children[0];
            next = &minimum;
        }
        else
        {
            child = m_children[1];
            childMinimum.x = minimum.x + 0.0f;
            childMinimum.y = minimum.y + half;
        }
    }
    else
    {
        childMinimum.x = minimum.x + half;
        if (minimum.y + half > point.z)
        {
            child = m_children[2];
            childMinimum.y = minimum.y + 0.0f;
        }
        else
        {
            child = m_children[3];
            childMinimum.y = minimum.y + half;
        }
    }

    if (!child)
        return;

    tree->m_nodes[child].printPolygonsInNode(tree, *next, half, point, polygon, materials,
                                             allowedHeightDiff);
}

} // namespace ai
} // namespace hfe
} // namespace dice
