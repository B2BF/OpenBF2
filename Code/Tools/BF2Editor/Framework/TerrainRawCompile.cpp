// Code/Tools/BF2Editor/Framework/TerrainRawCompile.cpp - see TerrainRawCompile.h. Editor-only code.
#include "Framework/EditorConfig.h"
#include "Framework/TerrainRawCompile.h"

#include <cfloat>
#include <cmath>
#include <cstring>
#include <deque>
#include <utility>

namespace bf2editor
{

namespace
{

// The patch size every stock level has and the retail writer's quadtrees assume.
const int kPatchSize = 128;
// A patch layer's LOD records as the retail writer writes them: the root and the 83 nodes after it in level order
// (the last of the 85 nodes is never written).
const int kLayerRecords = 84;
// The word ahead of each patch layer record.
const unsigned kLayerMarker = 0xEEEEEEEEu;
// The texels around a cell that still make a layer present there.
const int kPresenceBorder = 2;
// A layer triangle whose three corners all lie this far under the sea level is left out.
const double kDeepWater = 3.0;

struct Vec
{
    float x;
    float y;
    float z;
};

// QuadPatch's normalise: a length within FLT_EPSILON of 1 is kept, one within FLT_EPSILON of 0 becomes zero.
void normalize(Vec& v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return;
    if (std::fabs(lengthSquared) < FLT_EPSILON)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    v.x = inverse * v.x;
    v.y = inverse * v.y;
    v.z = inverse * v.z;
}

// D3DXComputeBoundingSphere: the points' mean, and the distance of the farthest point from it.
void boundingSphere(const std::vector<Vec>& points, float centre[3], float& radius)
{
    float sum[3] = { 0.0f, 0.0f, 0.0f };
    for (size_t i = 0; i < points.size(); ++i)
    {
        sum[0] += points[i].x;
        sum[1] += points[i].y;
        sum[2] += points[i].z;
    }
    const float count = static_cast<float>(points.size());
    for (int k = 0; k < 3; ++k)
        centre[k] = points.empty() ? 0.0f : sum[k] / count;
    float farthest = 0.0f;
    for (size_t i = 0; i < points.size(); ++i)
    {
        const float dx = points[i].x - centre[0];
        const float dy = points[i].y - centre[1];
        const float dz = points[i].z - centre[2];
        const float d = dx * dx + dy * dy + dz * dz;
        if (d > farthest)
            farthest = d;
    }
    radius = std::sqrt(farthest);
}

void put(std::string& block, size_t offset, const void* value, size_t size)
{
    std::memcpy(&block[offset], value, size);
}

// A vertex's border code (writeCompactVertexData): on a column that is a multiple of `step` cells, the row's place in
// four (3 folded to 1); else on such a row, the column's; 0 elsewhere. The writer's step is a quarter patch.
unsigned char borderCode(int row, int col, int step)
{
    int code = 0;
    if (col % step == 0)
        code = row % 4;
    else if (row % step == 0)
        code = col % 4;
    return static_cast<unsigned char>(code == 3 ? 1 : code);
}

// The quadtree node of a patch layer's triangles.
struct LayerNode
{
    unsigned start = 0;
    unsigned count = 0;
    LayerNode* children[4] = { 0, 0, 0, 0 };
};

void partition(std::vector<unsigned short>& indices, LayerNode& node, int size, int x0, int z0, int side, int leaf,
               std::deque<LayerNode>& pool)
{
    const int half = size >> 1;
    if (half < leaf)
        return;
    unsigned cursor = node.start;
    const unsigned end = node.start + node.count;
    for (int q = 0; q < 4; ++q)
    {
        const int cx = x0 + half * (q % 2);
        const int cz = z0 + half * (q / 2);
        pool.push_back(LayerNode());
        LayerNode& child = pool.back();
        node.children[q] = &child;
        const unsigned first = cursor;
        for (unsigned t = cursor; t < end; t += 3)
        {
            bool inside = true;
            for (int v = 0; v < 3 && inside; ++v)
            {
                const int x = indices[t + v] % side;
                const int z = indices[t + v] / side;
                inside = x >= cx && x <= cx + half && z >= cz && z <= cz + half;
            }
            if (!inside)
                continue;
            for (int v = 0; v < 3; ++v)
                std::swap(indices[t + v], indices[cursor + v]);
            cursor += 3;
        }
        child.start = first;
        child.count = cursor - first;
        partition(indices, child, half, cx, cz, side, leaf, pool);
    }
}

// The patch grid's quadtree (culler) square of node `index` in level order: level, and the square in cells relative
// to the map's corner; children are x half then z half.
struct Square
{
    int level;
    int x;
    int z;
    int size;
};

int levelStart(int level)
{
    int start = 0;
    for (int l = 0, count = 1; l < level; ++l, count *= 4)
        start += count;
    return start;
}

// The level-order index of the node at (ix, iz) (in node squares) of `level`: the children of a node are numbered
// x half first, so the offset in the level interleaves the bits with x lowest.
int nodeIndex(int level, int ix, int iz)
{
    int offset = 0;
    for (int bit = 0; bit < level; ++bit)
        offset |= (((ix >> bit) & 1) << (2 * bit)) | (((iz >> bit) & 1) << (2 * bit + 1));
    return levelStart(level) + offset;
}

// The surrounding sector placement of the game (SurroundingPatch::setupPlacement) for `slot`: the origin and the
// grid-to-world matrix that turns the sector's fine strip towards the primary map.
void sectorPlacement(int slot, int span, float step, int& originX, int& originZ, float m[16])
{
    std::memset(m, 0, 16 * sizeof(float));
    m[10] = 1.0f;
    m[15] = 1.0f;
    const int h = span;
    switch (slot)
    {
    case 0: originX = -h - h / 2; originZ = h - h / 2;   m[1] = step;  m[4] = -step; m[12] = static_cast<float>(originX + h); m[13] = static_cast<float>(originZ); break;
    case 1: originX = -(h / 2);   originZ = h - h / 2;   m[0] = step;  m[5] = step;  m[12] = static_cast<float>(originX);     m[13] = static_cast<float>(originZ); break;
    case 2: originX = h - h / 2;  originZ = h - h / 2;   m[0] = step;  m[5] = step;  m[12] = static_cast<float>(originX);     m[13] = static_cast<float>(originZ); break;
    case 3: originX = h - h / 2;  originZ = -(h / 2);    m[1] = -step; m[4] = step;  m[12] = static_cast<float>(originX);     m[13] = static_cast<float>(originZ + h); break;
    case 4: originX = h - h / 2;  originZ = -h - h / 2;  m[1] = -step; m[4] = step;  m[12] = static_cast<float>(originX);     m[13] = static_cast<float>(originZ + h); break;
    case 5: originX = -(h / 2);   originZ = -h - h / 2;  m[0] = -step; m[5] = -step; m[12] = static_cast<float>(originX + h); m[13] = static_cast<float>(originZ + h); break;
    case 6: originX = -h - h / 2; originZ = -h - h / 2;  m[0] = -step; m[5] = -step; m[12] = static_cast<float>(originX + h); m[13] = static_cast<float>(originZ + h); break;
    default: originX = -h - h / 2; originZ = -(h / 2);   m[1] = step;  m[4] = -step; m[12] = static_cast<float>(originX + h); m[13] = static_cast<float>(originZ); break;
    }
}

class Compiler
{
public:
    Compiler(const TerrainRawFile& templ, const TerrainCompileSource& source)
        : m_t(templ), m_s(source), m_n(static_cast<int>(templ.patchSize)), m_count(static_cast<int>(templ.patchCount)),
          m_scale(templ.primaryScale[0]), m_scaleY(templ.primaryScale[1])
    {
    }

    void patch(TerrainRawPatch& out, int index, int gx, int gz);
    void sector(TerrainRawRingPatch& out, int slot);
    void culler(std::vector<TerrainRawCullerNode>& out);

private:
    float height(float x, float z) const { return static_cast<float>(m_s.height(x, z)); }
    double exactHeight(float x, float z) const { return m_s.height(x, z); }
    void vertices(TerrainRawPatch& out, int gx, int gz, const std::vector<float>& heights);
    void layers(TerrainRawPatch& out, int patch, int gx, int gz);
    void addLayer(TerrainRawPatch& out, unsigned index, std::vector<unsigned short>& indices);
    void heightSubGrid(int x0, int z0, int x1, int z1, int step, const float m[16], int flags, std::vector<float>& out) const;
    void normalSubGrid(int x0, int z0, int x1, int z1, int step, const float m[16], std::vector<Vec>& out) const;

    const TerrainRawFile&       m_t;
    const TerrainCompileSource& m_s;
    int                         m_n;
    int                         m_count;
    float                       m_scale;
    float                       m_scaleY;
};

// Patch `index` (file order) at (gx, gz): the header from its (n + 1)^2 heights (QuadPatchEditable::computeBounds),
// the vertices, the layers.
void Compiler::patch(TerrainRawPatch& out, int index, int gx, int gz)
{
    const int side = m_n + 1;
    std::vector<float> heights(static_cast<size_t>(side) * side);
    std::vector<Vec> points(heights.size());
    out.gridX = gx;
    out.gridZ = gz;
    out.maxHeight = FLT_MIN;
    out.minHeight = FLT_MAX;
    for (int row = 0; row < side; ++row)
    {
        for (int col = 0; col < side; ++col)
        {
            const float x = static_cast<float>(gx * m_n + col) * m_scale;
            const float z = static_cast<float>(gz * m_n + row) * m_scale;
            const float h = height(x, z);
            if (out.maxHeight < h)
                out.maxHeight = h;
            if (!(out.minHeight < h))
                out.minHeight = h;
            heights[row * side + col] = h;
            points[row * side + col].x = x;
            points[row * side + col].y = h;
            points[row * side + col].z = z;
        }
    }
    boundingSphere(points, out.sphereCentre, out.sphereRadius);
    vertices(out, gx, gz, heights);
    layers(out, index, gx, gz);
}

// The 24-byte vertices (writeCompactVertexData): border code, row, column, 0; the height in heightmap units; the
// geomorph deltas of levels 0 and 1 and a zero; the packed normal.
void Compiler::vertices(TerrainRawPatch& out, int gx, int gz, const std::vector<float>& heights)
{
    const int n = m_n;
    const int side = n + 1;
    const int step = static_cast<int>(m_scale);
    const float inverse = 1.0f / m_scaleY;

    // The normals (QuadPatch::buildNormalVertexBuffer): forward differences on an (n + 2)^2 grid starting one step
    // before the patch, then per vertex the mean of the samples left of, at (twice) and above it.
    const int ns = n + 2;
    std::vector<Vec> samples(static_cast<size_t>(ns) * ns);
    const int scaledX = gx * n * step;
    const int scaledZ = gz * n * step;
    for (int row = 0, zOffset = -step; row < ns; ++row, zOffset += step)
    {
        for (int col = 0, xOffset = -step; col < ns; ++col, xOffset += step)
        {
            const int x = scaledX + xOffset;
            const int z = scaledZ + zOffset;
            const float fx = static_cast<float>(x);
            const float fz = static_cast<float>(z);
            const float h0 = height(fx, fz);
            const float fx1 = static_cast<float>(x + step);
            const float h1 = height(fx1, fz);
            Vec alongX = { fx1 - fx, h1 - h0, fz - fz };
            const float fz1 = static_cast<float>(z + step);
            const float h2 = height(fx, fz1);
            Vec alongZ = { fx - fx, h2 - h0, fz1 - fz };
            normalize(alongX);
            normalize(alongZ);
            Vec& s = samples[row * ns + col];
            s.x = alongZ.y * alongX.z - alongZ.z * alongX.y;
            s.y = alongZ.z * alongX.x - alongX.z * alongZ.x;
            s.z = alongX.y * alongZ.x - alongZ.y * alongX.x;
            normalize(s);
        }
    }

    out.vertices.assign(static_cast<size_t>(24) * side * side, '\0');
    for (int row = 0; row < side; ++row)
    {
        for (int col = 0; col < side; ++col)
        {
            const size_t at = static_cast<size_t>(24) * (row * side + col);
            out.vertices[at + 0] = static_cast<char>(borderCode(row, col, n / 4));
            out.vertices[at + 1] = static_cast<char>(row);
            out.vertices[at + 2] = static_cast<char>(col);
            const float h = heights[row * side + col] * inverse;
            put(out.vertices, at + 4, &h, 4);

            // The geomorph deltas: a vertex that drops out at level i (step 2, 4) takes its height
            // minus the mean of the two vertices it lies between at that level, in heightmap units, from the
            // unrounded heights (a flat map's deltas are exactly 0).
            float morph[3] = { 0.0f, 0.0f, 0.0f };
            const int x = col + gx * n;
            const int z = row + gz * n;
            for (int level = 0; level < 2; ++level)
            {
                const int stepL = 1 << (level + 1);
                const int half = stepL >> 1;
                float mean = 0.0f;
                int ax = 0, az = 0, bx = 0, bz = 0;
                bool pair = false;
                if (col % stepL == half)
                {
                    if (row % stepL == half)
                    {
                        ax = x + half; az = z - half; bx = x - half; bz = z + half;
                        pair = true;
                    }
                    else if (row % stepL == 0)
                    {
                        ax = x - half; az = z; bx = x + half; bz = z;
                        pair = true;
                    }
                }
                else if (row % stepL == half && col % stepL == 0)
                {
                    ax = x; az = z - half; bx = x; bz = z + half;
                    pair = true;
                }
                if (pair)
                {
                    const float a = static_cast<float>(
                        exactHeight(static_cast<float>(static_cast<double>(ax) * m_scale),
                                    static_cast<float>(static_cast<double>(az) * m_scale)) / m_scaleY);
                    mean = static_cast<float>(
                        exactHeight(static_cast<float>(static_cast<double>(bx) * m_scale),
                                    static_cast<float>(static_cast<double>(bz) * m_scale)) / m_scaleY + a);
                }
                mean = static_cast<float>(mean / 2.0);
                if (mean != 0.0f)
                {
                    const double own = exactHeight(static_cast<float>(static_cast<double>(x) * m_scale),
                                                   static_cast<float>(static_cast<double>(z) * m_scale));
                    morph[level] = static_cast<float>(own / m_scaleY - mean);
                }
            }
            put(out.vertices, at + 8, morph, 12);

            const Vec& left = samples[(row + 1) * ns + col];
            const Vec& centre = samples[(row + 1) * ns + col + 1];
            const Vec& up = samples[row * ns + col + 1];
            Vec mean;
            mean.x = ((left.x + centre.x) + up.x + centre.x) * 0.25f;
            mean.y = ((left.y + centre.y) + up.y + centre.y) * 0.25f;
            mean.z = ((left.z + centre.z) + up.z + centre.z) * 0.25f;
            normalize(mean);
            const unsigned packed = packTerrainNormal(mean.x, mean.y, mean.z);
            put(out.vertices, at + 20, &packed, 4);
        }
    }
}

// The detail layers (writeDetailCharts): six from the weight maps, then the water record.
void Compiler::layers(TerrainRawPatch& out, int patch, int gx, int gz)
{
    const int n = m_n;
    const int side = n + 1;
    const double sea = static_cast<double>(m_s.seaWaterLevel);
    const int size = m_s.weightSize;
    const int perCell = size / n;
    std::vector<unsigned char> deep(static_cast<size_t>(side) * side), wet(static_cast<size_t>(side) * side);
    for (int row = 0; row < side; ++row)
    {
        for (int col = 0; col < side; ++col)
        {
            const double h = static_cast<double>(height(static_cast<float>(static_cast<double>(col + n * gx) * m_scale),
                                                        static_cast<float>(static_cast<double>(row + n * gz) * m_scale)));
            deep[row * side + col] = sea - kDeepWater > h;
            wet[row * side + col] = h < sea;
        }
    }
    for (int layer = 0; layer < 6; ++layer)
    {
        const unsigned char* const weights = m_s.weights ? m_s.weights(patch, layer) : 0;
        if (!weights)
            continue;
        std::vector<unsigned short> indices;
        for (int z = 0; z < n; ++z)
        {
            for (int x = 0; x < n; ++x)
            {
                bool present = false;
                for (int i = -kPresenceBorder; i < kPresenceBorder + perCell && !present; ++i)
                {
                    for (int j = -kPresenceBorder; j < kPresenceBorder + perCell && !present; ++j)
                    {
                        int tx = i + perCell * x;
                        int tz = j + perCell * z;
                        tx = tx > size - 1 ? size - 1 : tx;
                        tx = tx < 0 ? 0 : tx;
                        tz = tz > size - 1 ? size - 1 : tz;
                        tz = tz < 0 ? 0 : tz;
                        present = weights[tx + size * tz] != 0;
                    }
                }
                if (!present)
                    continue;
                const unsigned short a = static_cast<unsigned short>(x + z * side);
                const unsigned short b = static_cast<unsigned short>(z * side + x + 1);
                const unsigned short c = static_cast<unsigned short>(x + side * (z + 1));
                const unsigned short d = static_cast<unsigned short>(side * (z + 1) + x + 1);
                if (!deep[a] || !deep[b] || !deep[c])
                {
                    indices.push_back(a);
                    indices.push_back(b);
                    indices.push_back(c);
                }
                if (!deep[c] || !deep[b] || !deep[d])
                {
                    indices.push_back(c);
                    indices.push_back(b);
                    indices.push_back(d);
                }
            }
        }
        if (!indices.empty())
            addLayer(out, static_cast<unsigned>(layer), indices);
    }
    std::vector<unsigned short> water;
    for (int z = 0; z < n; ++z)
    {
        for (int x = 0; x < n; ++x)
        {
            const unsigned short a = static_cast<unsigned short>(x + z * side);
            const unsigned short b = static_cast<unsigned short>(z * side + x + 1);
            const unsigned short c = static_cast<unsigned short>(x + side * (z + 1));
            const unsigned short d = static_cast<unsigned short>(side * (z + 1) + x + 1);
            if (wet[a] || wet[b] || wet[c])
            {
                water.push_back(a);
                water.push_back(b);
                water.push_back(c);
            }
            if (wet[c] || wet[b] || wet[d])
            {
                water.push_back(c);
                water.push_back(b);
                water.push_back(d);
            }
        }
    }
    if (!water.empty())
        addLayer(out, 6, water);
}

void Compiler::addLayer(TerrainRawPatch& out, unsigned index, std::vector<unsigned short>& indices)
{
    TerrainRawPatchLayer layer;
    layer.marker = static_cast<int>(kLayerMarker);
    layer.detailIndex = index;
    layer.word0C = static_cast<unsigned>(indices.size() / 3);
    layer.bytes10[0] = 4;
    orderLayerTriangles(indices, m_n, kLayerRecords, layer.lodWords);
    layer.indices.swap(indices);
    out.layers.push_back(layer);
}

// One block of a sector's height stream (SurroundingPatchEditable::heightSubGrid): the cluster height over the
// secondary vertical scale at each grid point, and on the block's last row (flag 1) or last column (flag 2) every
// other point the mean of its two neighbours, which closes the seam with the coarser block after it.
void Compiler::heightSubGrid(int x0, int z0, int x1, int z1, int step, const float m[16], int flags,
                             std::vector<float>& out) const
{
    const float secondaryY = m_t.secondaryScale[1];
    int zIndex = 0;
    for (int z = z0; z < z1 + 1; z += step, ++zIndex)
    {
        const float inverse = 1.0f / secondaryY;
        int xIndex = 0;
        for (int x = x0, xPrevious = x0 - step; x < x1 + 1; x += step, xPrevious += step, ++xIndex)
        {
            float value;
            if ((flags & 1) && z == z1 && xIndex % 2 != 0)
            {
                const float pz = static_cast<float>(z);
                float px = static_cast<float>(xPrevious);
                const float first = height(((pz * m[4] + px * m[0]) + m[8] * 0.0f) + m[12],
                                           ((pz * m[5] + px * m[1]) + m[9] * 0.0f) + m[13]) * inverse;
                px = static_cast<float>(x + step);
                value = (height(((pz * m[4] + px * m[0]) + m[8] * 0.0f) + m[12],
                                ((pz * m[5] + px * m[1]) + m[9] * 0.0f) + m[13]) * inverse + first) * 0.5f;
            }
            else if ((flags & 2) && x == x1 && zIndex % 2 != 0)
            {
                const float px = static_cast<float>(x);
                float pz = static_cast<float>(z - step);
                const float first = height(((pz * m[4] + px * m[0]) + m[8] * 0.0f) + m[12],
                                           ((pz * m[5] + px * m[1]) + m[9] * 0.0f) + m[13]) * inverse;
                pz = static_cast<float>(z + step);
                value = (height(((pz * m[4] + px * m[0]) + m[8] * 0.0f) + m[12],
                                ((pz * m[5] + px * m[1]) + m[9] * 0.0f) + m[13]) * inverse + first) * 0.5f;
            }
            else
            {
                const float px = static_cast<float>(x);
                const float pz = static_cast<float>(z);
                const float worldZ = ((pz * m[5] + px * m[1]) + m[9] * 0.0f) + m[13];
                const float worldX = ((pz * m[4] + m[8] * 0.0f) + px * m[0]) + m[12];
                value = height(worldX, worldZ) * inverse;
            }
            out.push_back(value);
        }
    }
}

// One block of a sector's normal stream: the world points of the block and a one-point border, then per point the mean
// of the six triangle normals around it, normalised and pointing up. 1.50's SurroundingPatchEditable::normalSubGrid
// takes the same crosses with the opposite sign (it lights the editor's own view of the sectors with them); the retail
// editor's terrain save writes them upward, as the game reads them.
void Compiler::normalSubGrid(int x0, int z0, int x1, int z1, int step, const float m[16], std::vector<Vec>& out) const
{
    const int rows = (z1 - z0) / step + 1;
    const int columns = (x1 - x0) / step + 1;
    const int stride = columns + 2;
    std::vector<Vec> grid(static_cast<size_t>(stride) * (rows + 2));
    for (int r = 0, z = z0 - step; r < rows + 2; ++r, z += step)
    {
        const float pz = static_cast<float>(z);
        for (int c = 0, x = x0 - step; c < columns + 2; ++c, x += step)
        {
            const float px = static_cast<float>(x);
            Vec& sample = grid[r * stride + c];
            sample.z = ((pz * m[5] + px * m[1]) + m[9] * 0.0f) + m[13];
            sample.x = ((px * m[0] + pz * m[4]) + m[8] * 0.0f) + m[12];
            sample.y = height(sample.x, sample.z);
        }
    }
    for (int r = 1; r <= rows; ++r)
    {
        for (int c = 1; c <= columns; ++c)
        {
            const Vec& centre = grid[r * stride + c];
            const Vec& upCell = grid[(r - 1) * stride + c];
            const Vec& upRightCell = grid[(r - 1) * stride + c + 1];
            const Vec& rightCell = grid[r * stride + c + 1];
            const Vec& downCell = grid[(r + 1) * stride + c];
            const Vec& downLeftCell = grid[(r + 1) * stride + c - 1];
            const Vec& leftCell = grid[r * stride + c - 1];
            const float ux = upCell.x - centre.x, uy = upCell.y - centre.y, uz = upCell.z - centre.z;
            const float ax = upRightCell.x - centre.x, ay = upRightCell.y - centre.y, az = upRightCell.z - centre.z;
            const float rx = rightCell.x - centre.x, ry = rightCell.y - centre.y, rz = rightCell.z - centre.z;
            const float dx = downCell.x - centre.x, dy = downCell.y - centre.y, dz = downCell.z - centre.z;
            const float bx = downLeftCell.x - centre.x, by = downLeftCell.y - centre.y, bz = downLeftCell.z - centre.z;
            const float lx = leftCell.x - centre.x, ly = leftCell.y - centre.y, lz = leftCell.z - centre.z;
            const float n1x = az * uy - ay * uz, n1y = uz * ax - az * ux, n1z = ay * ux - uy * ax;
            const float n2x = rz * ay - ry * az, n2y = az * rx - rz * ax, n2z = ry * ax - ay * rx;
            const float n3x = dz * ry - dy * rz, n3y = rz * dx - dz * rx, n3z = dy * rx - ry * dx;
            const float n4x = bz * dy - by * dz, n4y = dz * bx - bz * dx, n4z = by * dx - dy * bx;
            const float n5x = lz * by - ly * bz, n5y = bz * lx - lz * bx, n5z = ly * bx - by * lx;
            const float n6x = ly * uz - lz * uy, n6y = lz * ux - uz * lx, n6z = uy * lx - ly * ux;
            Vec normal;
            normal.x = -((((((n2x + n1x) + n3x) + n4x) + n5x) + n6x) * 0.16666667f);
            normal.y = -((((((n2y + n1y) + n3y) + n4y) + n5y) + n6y) * 0.16666667f);
            normal.z = -((((((n2z + n1z) + n3z) + n4z) + n5z) + n6z) * 0.16666667f);
            normalize(normal);
            out.push_back(normal);
        }
    }
}

// Sector `slot`: the header from the full grid (SurroundingPatchEditable::computeBounds: the height range, and the
// bounding sphere of the points with x and z scaled once more by the secondary scale, as shipped), then the height
// and normal streams over the game's stepped grid.
void Compiler::sector(TerrainRawRingPatch& out, int slot)
{
    const int heightmapSize = m_s.heightmapSize ? m_s.heightmapSize : m_count * m_n + 1;
    const int span = static_cast<int>(m_scale) * (heightmapSize - 1);
    const int stepWorld = static_cast<int>(m_t.secondaryScale[0]);
    const int u = span / stepWorld;
    int originX = 0, originZ = 0;
    float m[16];
    sectorPlacement(slot, span, static_cast<float>(stepWorld), originX, originZ, m);

    out.slot = slot;
    out.maxY = FLT_MIN;
    out.minY = FLT_MAX;
    std::vector<Vec> points;
    for (int z = 0; z < span + 1; z += stepWorld)
    {
        for (int x = 0; x < span + 1; x += stepWorld)
        {
            const float h = height(static_cast<float>(x + originX), static_cast<float>(z + originZ));
            if (out.maxY < h)
                out.maxY = h;
            if (!(out.minY < h))
                out.minY = h;
            Vec p;
            p.x = static_cast<float>(x + originX) * m_t.secondaryScale[0];
            p.y = h;
            p.z = static_cast<float>(z + originZ) * m_t.secondaryScale[2];
            points.push_back(p);
        }
    }
    boundingSphere(points, out.centre, out.radius);

    std::vector<float> heights;
    std::vector<Vec> normals;
    if (slot % 2 == 0)
    {
        heightSubGrid(0, 0, u / 4, u / 4, 1, m, 3, heights);
        heightSubGrid(u / 4, 0, u / 2, u / 4, 2, m, 2, heights);
        heightSubGrid(0, u / 4, u / 2, u / 2, 2, m, 3, heights);
        heightSubGrid(u / 2, 0, u, u / 2, 4, m, 0, heights);
        heightSubGrid(0, u / 2, u, u, 4, m, 0, heights);
        normalSubGrid(0, 0, u / 4, u / 4, 1, m, normals);
        normalSubGrid(u / 4, 0, u / 2, u / 4, 2, m, normals);
        normalSubGrid(0, u / 4, u / 2, u / 2, 2, m, normals);
        normalSubGrid(u / 2, 0, u, u / 2, 4, m, normals);
        normalSubGrid(0, u / 2, u, u, 4, m, normals);
    }
    else
    {
        heightSubGrid(0, 0, u, u / 4, 1, m, 1, heights);
        heightSubGrid(0, u / 4, u, u / 2, 2, m, 1, heights);
        heightSubGrid(0, u / 2, u, u, 4, m, 0, heights);
        normalSubGrid(0, 0, u, u / 4, 1, m, normals);
        normalSubGrid(0, u / 4, u, u / 2, 2, m, normals);
        normalSubGrid(0, u / 2, u, u, 4, m, normals);
    }
    out.heights.assign(reinterpret_cast<const char*>(heights.data()), heights.size() * sizeof(float));
    out.normals.assign(reinterpret_cast<const char*>(normals.data()), normals.size() * sizeof(Vec));
}

// The culler: a quadtree over the map from the whole map down to squares of patchSize / 8 cells. A node holds its
// eight same-level neighbours, its box (the height range of the vertices in its square, the far row and column left
// out) and flags: the patch level marks its nodes, the levels under it the first byte, two levels under it the third;
// nodes at and under the patch level name their patch.
void Compiler::culler(std::vector<TerrainRawCullerNode>& out)
{
    const int cells = m_count * m_n;
    const int half = cells / 2;
    const int leaf = m_n / 8;
    const int gridSide = cells + 1;
    std::vector<float> grid(static_cast<size_t>(gridSide) * gridSide);
    for (int z = -half; z <= half; ++z)
        for (int x = -half; x <= half; ++x)
            grid[(z + half) * gridSide + (x + half)] = height(static_cast<float>(x) * m_scale, static_cast<float>(z) * m_scale);

    std::vector<Square> squares(1, Square{ 0, 0, 0, cells });
    for (size_t k = 0; k < squares.size(); ++k)
    {
        const Square s = squares[k];
        if (s.size <= leaf)
            continue;
        for (int c = 0; c < 4; ++c)
            squares.push_back(Square{ s.level + 1, s.x + s.size / 2 * (c % 2), s.z + s.size / 2 * (c / 2), s.size / 2 });
    }
    int patchLevel = 0;
    while ((cells >> patchLevel) > m_n)
        ++patchLevel;

    out.assign(squares.size(), TerrainRawCullerNode());
    // Leaves from the grid, the others from their children (the same square without the far edge).
    std::vector<float> lo(squares.size()), hi(squares.size());
    for (size_t k = squares.size(); k-- > 0;)
    {
        const Square& s = squares[k];
        float top = FLT_MIN, bottom = FLT_MAX;
        if (s.size <= leaf)
        {
            for (int z = s.z; z < s.z + s.size; ++z)
            {
                for (int x = s.x; x < s.x + s.size; ++x)
                {
                    const float h = grid[z * gridSide + x];
                    if (top < h)
                        top = h;
                    if (h < bottom)
                        bottom = h;
                }
            }
        }
        else
        {
            for (int c = 0; c < 4; ++c)
            {
                const size_t child = 4 * k + 1 + c;
                if (top < hi[child])
                    top = hi[child];
                if (lo[child] < bottom)
                    bottom = lo[child];
            }
        }
        lo[k] = bottom;
        hi[k] = top;
    }
    for (size_t k = 0; k < squares.size(); ++k)
    {
        const Square& s = squares[k];
        TerrainRawCullerNode& node = out[k];
        const int count = cells / s.size;
        const int ix = s.x / s.size;
        const int iz = s.z / s.size;
        int slot = 0;
        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx == 0 && dz == 0)
                    continue;
                const int nx = ix + dx;
                const int nz = iz + dz;
                node.neighbour[slot++] = (nx < 0 || nz < 0 || nx >= count || nz >= count)
                                             ? 0xFFFF : static_cast<unsigned short>(nodeIndex(s.level, nx, nz));
            }
        }
        node.minimum[0] = static_cast<float>(s.x - half) * m_scale;
        node.minimum[1] = lo[k];
        node.minimum[2] = static_cast<float>(s.z - half) * m_scale;
        node.maximum[0] = static_cast<float>(s.x - half + s.size) * m_scale;
        node.maximum[1] = hi[k];
        node.maximum[2] = static_cast<float>(s.z - half + s.size) * m_scale;
        node.byteEC = s.level > patchLevel ? 1 : 0;
        node.hasPatchList = s.level == patchLevel ? 1 : 0;
        node.byteEE = s.level == patchLevel + 2 ? 1 : 0;
        node.patchIndex = s.level >= patchLevel
                              ? static_cast<unsigned char>((s.z / m_n) * m_count + s.x / m_n) : static_cast<unsigned char>(0xFF);
    }
}

} // namespace

unsigned packTerrainNormal(float x, float y, float z)
{
    const int px = static_cast<int>(static_cast<double>(x) * 127.0) + 127;
    const int py = static_cast<int>(static_cast<double>(y) * 127.0) + 127;
    const int pz = static_cast<int>(static_cast<double>(z) * 127.0) + 127;
    return (static_cast<unsigned>(static_cast<unsigned char>(px)) << 16) |
           (static_cast<unsigned>(static_cast<unsigned char>(py)) << 8) |
           static_cast<unsigned>(static_cast<unsigned char>(pz)) | 0xFF000000u;
}

void orderLayerTriangles(std::vector<unsigned short>& indices, int patchSize, int recordCount,
                         std::vector<unsigned>& lodWords)
{
    std::deque<LayerNode> pool;                 // keeps the children's addresses as it grows
    LayerNode root;
    root.count = static_cast<unsigned>(indices.size());
    partition(indices, root, patchSize, 0, 0, patchSize + 1, patchSize / 8, pool);
    std::vector<const LayerNode*> order(1, &root);
    for (size_t i = 0; i < order.size(); ++i)
        for (int c = 0; c < 4; ++c)
            if (order[i]->children[c])
                order.push_back(order[i]->children[c]);
    lodWords.clear();
    for (int i = 0; i < recordCount && i < static_cast<int>(order.size()); ++i)
    {
        lodWords.push_back(order[i]->start);
        lodWords.push_back(order[i]->count);
        for (int k = 0; k < 4; ++k)
            lodWords.push_back(0);
    }
}

bool compileTerrainRaw(const TerrainRawFile& templ, const TerrainCompileSource& source, TerrainRawFile& out,
                       std::string& error)
{
    if (!source.height)
    {
        error = "no height source";
        return false;
    }
    if (templ.patchSize != static_cast<unsigned>(kPatchSize) || templ.patches.size() != templ.patchCount * templ.patchCount
        || templ.compactVertices)
    {
        error = "the level's terraindata.raw is not a grid the compile handles (patch size 128, 24-byte vertices)";
        return false;
    }
    if (source.weightSize < kPatchSize || source.weightSize % kPatchSize)
    {
        error = "the weight maps are not a whole number of texels per cell";
        return false;
    }
    out = templ;
    if (source.replaceDetailTextures)
        out.detailTextures = source.detailTextures;
    if (source.replaceFar)
    {
        out.farSideTiling[0] = source.farSideTiling[0];
        out.farSideTiling[1] = source.farSideTiling[1];
        out.farTopTilingHi = source.farTopTilingHi;
        out.farTopTilingLow = source.farTopTilingLow;
        out.farYOffset = source.farYOffset;
    }
    Compiler compiler(templ, source);
    float top = FLT_MIN, bottom = FLT_MAX;
    for (size_t p = 0; p < out.patches.size(); ++p)
    {
        TerrainRawPatch fresh;
        compiler.patch(fresh, static_cast<int>(p), templ.patches[p].gridX, templ.patches[p].gridZ);
        if (top < fresh.maxHeight)
            top = fresh.maxHeight;
        if (fresh.minHeight < bottom)
            bottom = fresh.minHeight;
        out.patches[p] = fresh;
    }
    out.maxHeight = top;
    out.minHeight = bottom;
    out.ring.clear();
    for (size_t s = 0; s < source.ringSlots.size(); ++s)
    {
        TerrainRawRingPatch sector;
        compiler.sector(sector, source.ringSlots[s]);
        out.ring.push_back(sector);
    }
    compiler.culler(out.culler);
    return true;
}

} // namespace bf2editor
