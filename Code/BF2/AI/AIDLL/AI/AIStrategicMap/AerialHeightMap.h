// Cached ground heights for aerial navigation. Sampling requires in-range coordinates.
// An empty map reports the missing-height sentinel 9999.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AISTRATEGICMAP_AERIALHEIGHTMAP_H
#define AI_AIDLL_AI_AISTRATEGICMAP_AERIALHEIGHTMAP_H

#include "../Main/AIDllTypes.h"     // Vec3
#include "../../../../Math/Vec3.h"  // Vec2 lives here too, and both are typedefs - never
                                    // forward-declare either one; see AILandingZoneManager.h.

namespace dice {
namespace hfe {
namespace ai {

class AerialHeightMap
{
public:
    AerialHeightMap();
    ~AerialHeightMap();

    float getAltitude(const Vec3&);
    float getGroundHeight(const Vec3&);
    float getGroundHeight(float, float);

    void createHeightMap(int samplesPerAxis = 4);   // ConsoleClass161 passes 4 without an argument

    // Returns the sampled height as an unsigned 16-bit integer for height-map storage.
    unsigned short computeHeightAt(int cellX, int cellZ, int samplesPerAxis);

    void saveHeightMapToFile();
    void readHeightMapFromFile();

    // the layout, from the constructor, the sampler and the two file paths
    /*  +0 */ float m_cellSize;          // = 16.0f the sampler divides by it
    /*  +4 */ Vec2  m_origin;            // = (0, 0) the terrain's minimum corner once built
    // `delete[]` in the destructor, so it is an array of 16-bit heights, `new[]`-allocated by
    // `createHeightMap` or by the reader. Indexed `x + m_width * z`, i.e. row-major in Z.
    /* +12 */ unsigned short* m_heights; // = 0
    // Dimensions are uninitialized until the map is created or loaded. Sampling is safe
    // before then only because it checks the null height buffer before reading the stride.
    /* +16 */ int m_width;
    /* +20 */ int m_height;              // Grid height.

    // last wins, and the destructor nulls it unconditionally rather than testing `== this` - the
    // same pair of habits `IAIPathfinding` shows.
    static AerialHeightMap* m_instance;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICMAP_AERIALHEIGHTMAP_H
