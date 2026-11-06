// Uniform spatial index of information records for one side. Owns cell lists and indexed records.
// Coordinates are clamped to the grid bounds for cell lookup.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_AIINFORMATIONGRID_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AIKNOWLEDGE_AIINFORMATIONGRID_H

#include <list>
#include <map>
#include <vector>

#include "../../../../Math/Vec3.h"      // Vec2 and Vec3 both live here, and both are typedefs

namespace dice {
namespace hfe {

class Frustum;

namespace ai {

class Information;

// Query predicate with one const virtual method and no virtual destructor. Preserve this ABI.
class IAIInformationGridPredicate
{
public:
    virtual bool includeInformation(Information* information) const = 0; // +0
};

// Information-grid interface. Keep the frustum query before the rectangle query;
// IAIInformationEnvironment exposes those queries in the opposite virtual order.
class IAIInformationGrid
{
public:
    virtual ~IAIInformationGrid();                                       // +0 / +1

    virtual void addInfo(unsigned int id) = 0;                           // +2
    virtual void removeInfo(unsigned int id) = 0;                        // +3
    virtual bool safeRemoveInfo(unsigned int id,
                                IAIInformationGridPredicate* remove) = 0;            // +4
    virtual bool updatePosition(unsigned int id) = 0;                    // +5
    virtual void addAndUpdatePosition(unsigned int id) = 0;              // +6

    virtual void getInformationWithinCircle(std::vector<unsigned int>& out, const Vec2& centre,
                                            float radius,
                                            IAIInformationGridPredicate* keep) const = 0;   // +7
    virtual void getInformationWithinFrustum(std::vector<unsigned int>& out,
                                             const Frustum& frustum,
                                             IAIInformationGridPredicate* keep) const = 0;  // +8
    virtual void getInformationWithinRectangle(std::vector<unsigned int>& out, const Vec2& min,
                                               const Vec2& max,
                                               IAIInformationGridPredicate* keep) const = 0; // +9

    // the sweep comes first. Slot 10 is the non-const two-predicate form; slot 11 is the const
    // one-predicate form. See the header block for how they disagree.
    virtual void getAllInformation(std::vector<unsigned int>& out,
                                   IAIInformationGridPredicate* keep,
                                   IAIInformationGridPredicate* cull) = 0;                   // +10
    virtual void getAllInformation(std::vector<unsigned int>& out,
                                   IAIInformationGridPredicate* keep) const = 0;             // +11
};

// Spatial information index. Offsets describe the original 64-byte object; STL member
// layouts depend on the compiler and do not define this build's size.
class AIInformationGrid : public IAIInformationGrid
{
public:
    // Sixteen-byte grid record. [INFERRED] m_y names the world Z coordinate used by this X/Z index.
    struct InfoInfo
    {
        // zeroes the position and nothing else - see defect 3 in the header block.
        InfoInfo() : m_x(0.0f), m_y(0.0f) {}

        /* +0x00 */ float m_x;
        /* +0x04 */ float m_y;
        /* +0x08 */ float m_radius;
        /* +0x0C */ bool  m_visited;      // the query-time de-duplication mark
    };

    // eight bytes, and the list node around it is sixteen. The constructor's per-cell allocation
    // is `allocate(16)` and a libstdc++ list node carries two pointers, which pins this at 8.
    struct GridCellElem
    {
        /* +0x00 */ unsigned int m_id;
        /* +0x04 */ InfoInfo*    m_info;
    };

    typedef std::list<GridCellElem>          Cell;
    typedef std::map<unsigned int, InfoInfo*> InfoMap;

    AIInformationGrid(int side, int dimX, int dimZ, Vec2 min, Vec2 max);
    virtual ~AIInformationGrid();

    virtual void addInfo(unsigned int id);
    virtual void removeInfo(unsigned int id);
    virtual bool safeRemoveInfo(unsigned int id, IAIInformationGridPredicate* remove);
    virtual bool updatePosition(unsigned int id);
    virtual void addAndUpdatePosition(unsigned int id);

    virtual void getInformationWithinCircle(std::vector<unsigned int>& out, const Vec2& centre,
                                            float radius,
                                            IAIInformationGridPredicate* keep) const;
    virtual void getInformationWithinFrustum(std::vector<unsigned int>& out,
                                             const Frustum& frustum,
                                             IAIInformationGridPredicate* keep) const;
    virtual void getInformationWithinRectangle(std::vector<unsigned int>& out, const Vec2& min,
                                               const Vec2& max,
                                               IAIInformationGridPredicate* keep) const;

    virtual void getAllInformation(std::vector<unsigned int>& out,
                                   IAIInformationGridPredicate* keep,
                                   IAIInformationGridPredicate* cull);
    virtual void getAllInformation(std::vector<unsigned int>& out,
                                   IAIInformationGridPredicate* keep) const;

private:
    // The five non-virtual helpers. None of them is in the vtable.
    void getMinMax(int& minX, int& maxX, int& minY, int& maxY,
                   const Vec2& min, const Vec2& max) const;
    void addElementToCells(unsigned int id, InfoInfo* info);
    void removeElementFromCells(unsigned int id, InfoInfo* info);
    void updateElementInCells(unsigned int id, const InfoInfo& old, InfoInfo* now);
    Cell::iterator findCellElement(int cell, unsigned int id) const;

    /* +0x00 */ // vptr
    /* +0x04 */ int  m_side;       // named by `removeInfo`'s own diagnostic, which prints it
    /* +0x08 */ int  m_dimX;       // paired with x by getMinMax, and the row stride everywhere
    /* +0x0C */ int  m_dimZ;       // paired with y by getMinMax
    /* +0x10 */ Vec2 m_min;
    /* +0x18 */ Vec2 m_max;

    // [UNRESOLVED] Four-byte member left uninitialized and unread by the implemented methods.
    /* +0x20 */ int  m_unwritten20;

    /* +0x24 */ Cell*   m_grid;       // `new Cell[m_dimX * m_dimZ + 1]` - see the header block  // name: AIDLL_r assert 0x101FAC00 (+0x24)
    /* +0x28 */ InfoMap m_infos;       // id -> record; in this libstdc++
    /* +0x34 */ mutable std::vector<InfoInfo*> m_visited;   // the scratchpad; `mutable` is required
                                                            // because three `const` queries fill it
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_AIINFORMATIONGRID_H
