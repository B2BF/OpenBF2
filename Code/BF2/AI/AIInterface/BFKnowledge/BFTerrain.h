// Exposes engine terrain dimensions and altitude to the AI.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_BFTERRAIN_H
#define AI_AIINTERFACE_BFKNOWLEDGE_BFTERRAIN_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "AIAdapterTypes.h"

namespace dice {
namespace hfe {

class IBase;

namespace ai {

class IAITerrain
{
public:
    IAITerrain();
    virtual ~IAITerrain();

    virtual float getHeight(float x, float z) const = 0;
    virtual float getAltitude(const Vec3& position) const = 0;
    virtual float getTerrainAltitude(const Vec3& position) const = 0;
    virtual float getTerrainAltitude(float x, float z) const = 0;
    virtual unsigned char getMaterial(float x, float z) const = 0;
    virtual int getMaterialToGiveDamage() const = 0;
    virtual bool isInsideCombatArea(IAIPlayer* player) const = 0;
    virtual void getNormal(float x, float z, Vec3& normal) const = 0;
    virtual float getHeightAndNormal(float x, float z, Vec3& normal, unsigned char& material) const = 0;
    virtual float getSlope(float x, float z) const = 0;
    virtual float getWaterLevel(float x, float z) const = 0;
    virtual float getSeaWaterLevel() const = 0;
    virtual bool findTerrainPoint(const Vec3& from, const Vec3& to, float step, Vec3& hit) const = 0;
    virtual int getWorldSizeX() const = 0;
    virtual int getWorldSizeZ() const = 0;
    virtual int getWorldMinX() const = 0;
    virtual int getWorldMinZ() const = 0;
    virtual int getWorldMaxX() const = 0;
    virtual int getWorldMaxZ() const = 0;
    virtual int getPrimaryWorldMinX() const = 0;
    virtual int getPrimaryWorldMinZ() const = 0;
    virtual int getPrimaryWorldMaxX() const = 0;
    virtual int getPrimaryWorldMaxZ() const = 0;
    virtual bool intersectTerrain(const Vec3& from, const Vec3& to, Vec3& hit, float& distance, Vec3& normal) const = 0;
    virtual bool getTriangle(const Vec3& position, Vec3& a, Vec3& b, Vec3& c, Vec3& normal, unsigned char& material) const = 0;
    virtual bool getTriangleNeighbour(const Vec3& a, const Vec3& b, const Vec3& c, unsigned int edge, Vec3& na, Vec3& nb, Vec3& nc, Vec3& normal, unsigned char& material) const = 0;
    virtual void getAllTerrainPolygons(std::pair<Vec2, Vec2> bounds, std::vector<Vec3>& vertices, std::vector<std::pair<int, int> >& edges, std::list<triple<int, int, int> >& triangles) const = 0;
    virtual void getAllTerrainPolygonsWithinBoundary(std::pair<Vec2, Vec2> bounds, const Vec2& min, const Vec2& max, std::vector<Vec3>& vertices, std::vector<std::pair<int, int> >& edges, std::list<triple<int, int, int> >& triangles) const = 0;
};

class BFTerrain : public IAITerrain
{
public:
    BFTerrain();
    virtual ~BFTerrain();

    virtual float getHeight(float x, float z) const;
    virtual float getAltitude(const Vec3& position) const;
    virtual float getTerrainAltitude(const Vec3& position) const;
    virtual float getTerrainAltitude(float x, float z) const;
    virtual unsigned char getMaterial(float x, float z) const;
    virtual int getMaterialToGiveDamage() const;
    virtual bool isInsideCombatArea(IAIPlayer* player) const;
    virtual void getNormal(float x, float z, Vec3& normal) const;
    virtual float getHeightAndNormal(float x, float z, Vec3& normal, unsigned char& material) const;
    virtual float getSlope(float x, float z) const;
    virtual float getWaterLevel(float x, float z) const;
    virtual float getSeaWaterLevel() const;
    virtual bool findTerrainPoint(const Vec3& from, const Vec3& to, float step, Vec3& hit) const;
    virtual int getWorldSizeX() const;
    virtual int getWorldSizeZ() const;
    virtual int getWorldMinX() const;
    virtual int getWorldMinZ() const;
    virtual int getWorldMaxX() const;
    virtual int getWorldMaxZ() const;
    virtual int getPrimaryWorldMinX() const;
    virtual int getPrimaryWorldMinZ() const;
    virtual int getPrimaryWorldMaxX() const;
    virtual int getPrimaryWorldMaxZ() const;
    virtual bool intersectTerrain(const Vec3& from, const Vec3& to, Vec3& hit, float& distance, Vec3& normal) const;
    virtual bool getTriangle(const Vec3& position, Vec3& a, Vec3& b, Vec3& c, Vec3& normal, unsigned char& material) const;
    virtual bool getTriangleNeighbour(const Vec3& a, const Vec3& b, const Vec3& c, unsigned int edge, Vec3& na, Vec3& nb, Vec3& nc, Vec3& normal, unsigned char& material) const;
    virtual void getAllTerrainPolygons(std::pair<Vec2, Vec2> bounds, std::vector<Vec3>& vertices, std::vector<std::pair<int, int> >& edges, std::list<triple<int, int, int> >& triangles) const;
    virtual void getAllTerrainPolygonsWithinBoundary(std::pair<Vec2, Vec2> bounds, const Vec2& min, const Vec2& max, std::vector<Vec3>& vertices, std::vector<std::pair<int, int> >& edges, std::list<triple<int, int, int> >& triangles) const;

private:
    bool pointInsideBbox(const Vec3& position, const std::pair<Vec2, Vec2>& bounds) const;
    void checkUnconnectedMeshData(std::vector<Vec3>& vertices, std::vector<std::pair<int, int>>& edges,
                                 std::list<triple<int, int, int>>& triangles) const;

    /* +0x00 */ // vptr
    /* +0x04 */ IBase* m_heightmapCluster;   // getSingleton("HeightmapCluster")
    /* +0x08 */ int m_worldSizeX;            // heightmapCluster vtable +0x30
    /* +0x0C */ int m_worldSizeZ;            // the same call, made twice
    /* +0x10 */ int m_worldMinX;             // (int)(-0.5f * worldSizeX)
    /* +0x14 */ int m_worldMinZ;             // (int)(-0.5f * worldSizeZ)
    /* +0x18 */ int m_worldMaxX;             // worldSizeX + worldMinX
    /* +0x1C */ int m_worldMaxZ;             // worldSizeZ + worldMinZ
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_BFTERRAIN_H
