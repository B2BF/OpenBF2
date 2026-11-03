// Side-specific spatial queries over AI information. Each side has two information grids.
// Predicates filter units, enemies, cover and other object classes.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_BFINFORMATIONENVIRONMENT_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIKNOWLEDGE_BFINFORMATIONENVIRONMENT_H

#include <vector>

#include "../../../../Math/Vec3.h"      // Vec2 and Vec3 both live here, and both are typedefs

namespace dice {
namespace hfe {

class Frustum;

namespace ai {

class Bot;
class IAIInformationGrid;
class IAIInformationGridPredicate;
class Information;

// Separate query and modifier interfaces for the information environment.
class IAIInformationEnvironment
{
public:
    IAIInformationEnvironment();
    virtual ~IAIInformationEnvironment();

    virtual void getFriendlyUnits(std::vector<unsigned int>&, const Vec3&, Bot*, float) const = 0;
    virtual void getFriendlyAndNeutralUnits(std::vector<unsigned int>&, const Vec3&, Bot*, float) const = 0;
    virtual void getCover(const Bot&, std::vector<unsigned int>&) const = 0;
    // Both return the search radius in st0 (ELF 0x2E7220 / 0x2E72C0), not void.
    virtual float getHardware(std::vector<unsigned int>&, const Vec3&, Bot*) const = 0;
    virtual float getHardware(std::vector<unsigned int>&, const Vec3&, float, int, Information*) const = 0;
    virtual void getEnemyObjects(std::vector<unsigned int>&, Bot*, float) const = 0;
    virtual void getObjectsWithinRadius(std::vector<unsigned int>&, float, const Bot&, bool,
                                        IAIInformationGridPredicate*) const = 0;
    virtual void getObjectsWithinRadius(std::vector<unsigned int>&, const Vec3&, float, int, bool,
                                        IAIInformationGridPredicate*) const = 0;
    virtual void getObjectsWithinRadius(std::vector<unsigned int>&, const Vec2&, float, int, bool,
                                        IAIInformationGridPredicate*) const = 0;
    virtual void getObjectsWithinRectangle(std::vector<unsigned int>&, const Vec2&, const Vec2&, int,
                                           bool, IAIInformationGridPredicate*) const = 0;
    virtual void getObjectsWithinFrustum(std::vector<unsigned int>&, const Frustum&, int, bool,
                                         IAIInformationGridPredicate*) const = 0;
    virtual void getAllInformation(std::vector<unsigned int>&, int, bool,
                                   IAIInformationGridPredicate*) const = 0;
    // ELF address point : one-predicate +0x34, two-predicate +0x38.
    // MSVC reverses the overload group: has two +0x30, one +0x34.
    virtual void getAllInformation(std::vector<unsigned int>&, int, bool,
                                   IAIInformationGridPredicate*, IAIInformationGridPredicate*) = 0;
};

class IAIInformationEnvironmentModifiers
{
public:
    IAIInformationEnvironmentModifiers();
    virtual ~IAIInformationEnvironmentModifiers();

    virtual void addInfo(int, bool, unsigned int) = 0;
    virtual void addAndUpdatePosition(int, bool, unsigned int) = 0;
    virtual void updatePosition(int, bool, unsigned int) = 0;
    virtual void removeInfo(int, bool, unsigned int) = 0;
    virtual void safeRemoveInfo(int, bool, unsigned int) = 0;
    virtual void destroyRemoveInfo(int, bool, unsigned int) = 0;
};

// sizeof is 32 (0x20) in retail and will not be here - two of the four members are real
// `std::vector`s. The left-column offsets are the shipped object's.
class BFInformationEnvironment : public IAIInformationEnvironment,
                                 public IAIInformationEnvironmentModifiers
{
public:
    BFInformationEnvironment();                  // - , complete
    virtual ~BFInformationEnvironment();         // [FRONTIER]

    // IAIInformationEnvironment. All-information, Vec3/Vec2 radius and rectangle wrappers written.
    virtual void getFriendlyUnits(std::vector<unsigned int>&, const Vec3&, Bot*, float) const;
    virtual void getFriendlyAndNeutralUnits(std::vector<unsigned int>&, const Vec3&, Bot*, float) const;
    virtual void getCover(const Bot&, std::vector<unsigned int>&) const;
    virtual float getHardware(std::vector<unsigned int>&, const Vec3&, Bot*) const;
    virtual float getHardware(std::vector<unsigned int>&, const Vec3&, float, int, Information*) const;
    virtual void getEnemyObjects(std::vector<unsigned int>&, Bot*, float) const;
    virtual void getObjectsWithinRadius(std::vector<unsigned int>&, float, const Bot&, bool,
                                        IAIInformationGridPredicate*) const;
    virtual void getObjectsWithinRadius(std::vector<unsigned int>&, const Vec3&, float, int, bool,
                                        IAIInformationGridPredicate*) const;
    virtual void getObjectsWithinRadius(std::vector<unsigned int>&, const Vec2&, float, int, bool,
                                        IAIInformationGridPredicate*) const;
    virtual void getObjectsWithinRectangle(std::vector<unsigned int>&, const Vec2&, const Vec2&, int,
                                           bool, IAIInformationGridPredicate*) const;
    virtual void getObjectsWithinFrustum(std::vector<unsigned int>&, const Frustum&, int, bool,
                                         IAIInformationGridPredicate*) const;
    virtual void getAllInformation(std::vector<unsigned int>&, int, bool,
                                   IAIInformationGridPredicate*) const;

    // Both overloads call the grid's mutating sweep; the const one passes a null cull predicate.
    virtual void getAllInformation(std::vector<unsigned int>&, int, bool, IAIInformationGridPredicate*,
                                   IAIInformationGridPredicate*);

    // IAIInformationEnvironmentModifiers.
    virtual void addInfo(int, bool, unsigned int);
    virtual void addAndUpdatePosition(int, bool, unsigned int);
    virtual void updatePosition(int, bool, unsigned int);
    virtual void removeInfo(int, bool, unsigned int);
    virtual void safeRemoveInfo(int, bool, unsigned int);
    virtual void destroyRemoveInfo(int, bool, unsigned int);

    // the selector. : pick a vector by the bool, index it by the side. See the header
    // block - this is what the trailing `bool` on every query above means.
    IAIInformationGrid* getInformationGrid(int side, bool which) const;

private:
    /* +0x00 */ // vptr - IAIInformationEnvironment
    /* +0x04 */ // vptr - IAIInformationEnvironmentModifiers
    /* +0x08 */ std::vector<IAIInformationGrid*> m_gridsA;
    /* +0x14 */ std::vector<IAIInformationGrid*> m_gridsB;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_BFINFORMATIONENVIRONMENT_H
