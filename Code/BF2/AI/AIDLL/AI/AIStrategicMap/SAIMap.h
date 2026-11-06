// A grid view of one side's strategic situation: per-square scalar layers, status layers and
// temperature trees, double-buffered (the update writes the buffer that is not current and flips
// at the end), and a resumable update that collects the information ids it will visit.
// Evidence: libaidll.so (ia-32) SAIMap.cpp 0x35FD10..0x364544. Nothing in the image constructs a
// SAIMap. Offsets are ELF; layer names are [UNRESOLVED] where no accessor names them.

#ifndef AI_AIDLL_AI_AISTRATEGICMAP_SAIMAP_H     // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICMAP_SAIMAP_H

#include <vector>

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class Information;
class SAI;
class SAIMapSquare;
class ScalarMap;
class StatusMap;
class TemperatureTreeModifiers;

class SAIMap
{
public:
    // ELF: a 28-byte record owned at +0x88. Only the state, the resume index and the id vector
    // are touched; the last eight bytes are [UNRESOLVED]. Names [INFERRED].
    struct UpdateState
    {
        /* +0x00 */ int m_state;
        /* +0x04 */ int m_index;
        /* +0x08 */ std::vector<unsigned int> m_ids;
        /* +0x14 */ unsigned char m_unresolved14[8];
    };

    // Allocates every layer at dimX * dimY; a null modifiers pointer selects the TU's defaults.
    SAIMap(int side, int dimX, int dimY, Vec2 min, Vec2 max, TemperatureTreeModifiers* modifiers);
    ~SAIMap();

    // Runs the resumable update until the update-time budget is spent (false) or a full pass
    // completed and the buffers flipped (true).
    bool update(SAI* sai, double budget);
    int getSide() const;
    int getMapDimensionX() const;
    int getMapDimensionY() const;
    TemperatureTreeModifiers* getTemperatureTreeModifiers() const;
    void setTemperatureTreeModifiers(TemperatureTreeModifiers* modifiers);
    float getSquareArea() const;                       // +0x24 is never written in the image
    float getSquareBoundingCircleRadius() const;
    float getSquareXSize() const;
    float getSquareYSize() const;
    // The centre of the square, x = id % dimX and y = id / dimX.
    Vec2 getCoordFromSquareId(int id);
    void setMapDimensionX(int x);                      // empty
    void setMapDimensionY(int y);                      // empty
    // The square containing position (x, z); false outside the map.
    bool getSquareId(int& x, int& y, const Vec3& position) const;
    void clearStates(float* a, float* b);
    float setFastThreshold(float threshold);           // fsts: the value stays in st0
    float getFastThreshold();
    void updateInformation(Information* information, SAI* sai, int buffer);

private:
    /* +0x00 */ int m_side;
    /* +0x04 */ int m_dimX;
    /* +0x08 */ int m_dimY;
    /* +0x0C */ Vec2 m_min;
    /* +0x14 */ Vec2 m_max;
    /* +0x1C */ float m_squareXSize;
    /* +0x20 */ float m_squareYSize;
    /* +0x24 */ float m_squareArea;                    // not constructed
    /* +0x28 */ float m_squareBoundingCircleRadius;
    // Four 44-float state rows, paired by clearStates as (A, C) and (B, D).
    /* +0x2C */ float* m_stateA;
    /* +0x30 */ float* m_stateB;
    /* +0x34 */ float* m_stateC;
    /* +0x38 */ float* m_stateD;
    /* +0x3C */ float m_fastThreshold;                 // not constructed
    /* +0x40 */ int m_currentBuffer;                   // 0 or 1; layers below are indexed by it
    /* +0x44 */ ScalarMap* m_layer44[2];               // [UNRESOLVED] role; zeroed
    /* +0x4C */ ScalarMap* m_layer4C[2];               // [UNRESOLVED] role; zeroed
    /* +0x54 */ ScalarMap* m_layer54[2];               // [UNRESOLVED] role; zeroed
    /* +0x5C */ ScalarMap* m_layer5C[2];               // [UNRESOLVED] role; zeroed
    /* +0x64 */ ScalarMap* m_layer64[2];               // [UNRESOLVED] role; zeroed
    /* +0x6C */ ScalarMap* m_layer6C;                  // [UNRESOLVED] role; not initialised
    /* +0x70 */ StatusMap* m_statusMaps[2];            // Neutral
    /* +0x78 */ SAIMapSquare* m_squares[2];
    /* +0x80 */ ScalarMap* m_layer80[2];               // [UNRESOLVED] role; one
    /* +0x88 */ UpdateState* m_updateState;
    /* +0x8C */ TemperatureTreeModifiers* m_modifiers;  // borrowed
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICMAP_SAIMAP_H
