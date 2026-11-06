// [FRONTIER] Remaining methods stop by name until implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BFInformationEnvironment operations.

#include "BFInformationEnvironment.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2E7AD0, __tcf_0 0x2E7B30, _GLOBAL__I 0x2E7B60).
#include <iostream>

#include <string>

#include "AIInformationGrid.h"                             // the grids the constructor builds
#include "Information.h"                                   // removeAllContacts
#include "../Main/InformationManager.h"                            // the information pool
#include "IPIMobile.h"                                     // getCover's search radius
#include "../AIBot/BotMain.h"                              // Bot +0xD8 / +0xE0
#include "../AIUtils/VehicleOccupancy.h"                   // InfoEnemyPredicate
#include "../AIUtils/checkPointer.h"                       // inlined four times by the constructor
#include "../../../../Core/ClassManager.h"
#include "../../../../Core/SmartItf.h"           // the classManager global
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"                      // getNSides, getInformationGridDimension
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"        // getWorldSizeX / getWorldSizeZ

namespace dice {
namespace hfe {
namespace ai {

// Registers query and modifier interfaces in their respective singleton rosters.
IAIInformationEnvironment::IAIInformationEnvironment()
{
    if (!AISingletonServer::m_iaiInformationEnvironment)
        classManager->registerSimpleSingleton(std::string("IAIInformationEnvironment"), this);
}

IAIInformationEnvironment::~IAIInformationEnvironment()
{
    if (AISingletonServer::m_iaiInformationEnvironment == this)
        classManager->unregisterSimpleSingleton(std::string("IAIInformationEnvironment"));
}

IAIInformationEnvironmentModifiers::IAIInformationEnvironmentModifiers()
{
    if (!CommonSingletonServer::m_iaiInformationEnvironmentModifiers)
        classManager->registerSimpleSingleton(std::string("IAIInformationEnvironmentModifiers"), this);
}

IAIInformationEnvironmentModifiers::~IAIInformationEnvironmentModifiers()
{
    if (CommonSingletonServer::m_iaiInformationEnvironmentModifiers == this)
        classManager->unregisterSimpleSingleton(std::string("IAIInformationEnvironmentModifiers"));
}

// Creates two spatial grids for each side, including neutral side zero.
// Grid bounds run from zero to the terrain dimensions.
BFInformationEnvironment::BFInformationEnvironment()
{
    const IAITerrain* const terrain =
        static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

    for (int side = 0; side < settings->getNSides() + 1; ++side)
    {
        // Queries terrain dimensions and grid settings separately for each grid allocation.
        // Preserve the repeated virtual calls instead of sharing cached values.
        {
            // Not centred on the origin - see the block above, and `AIInformationGrid::getMinMax`
            // which divides by the maximum corner alone and so requires this rectangle to start at
            // the origin.
            // `Vec2` is a plain aggregate here, with no two-argument constructor; the binary sets
            // the four floats field by field and so does this.
            Vec2 min;
            min.x = 0.0f;
            min.y = 0.0f;

            Vec2 max;
            max.x = static_cast<float>(terrain->getWorldSizeX());
            max.y = static_cast<float>(terrain->getWorldSizeZ());

            // One number, asked for twice - see the block above. The ELF evaluates the third
            // argument before the second, which is GCC pushing right to left; unobservable, because
            // it is one function called twice.
            m_gridsA.push_back(checkPointer(
                new AIInformationGrid(side, settings->getInformationGridDimension(),
                                      settings->getInformationGridDimension(), min, max)));
        }

        {
            Vec2 min;
            min.x = 0.0f;
            min.y = 0.0f;

            Vec2 max;
            max.x = static_cast<float>(terrain->getWorldSizeX());
            max.y = static_cast<float>(terrain->getWorldSizeZ());

            m_gridsB.push_back(checkPointer(
                new AIInformationGrid(side, settings->getInformationGridDimension(),
                                      settings->getInformationGridDimension(), min, max)));
        }
    }
}

// [FRONTIER] Unimplemented environment queries and modifiers stop by name.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

// ELF 0x2E6C30. Deletes every grid of the second vector, then of the first (null slots skipped,
// the end re-read after each delete); the vectors free themselves.
BFInformationEnvironment::~BFInformationEnvironment()
{
    for (std::vector<IAIInformationGrid*>::iterator i = m_gridsB.begin(); i != m_gridsB.end(); ++i)
    {
        if (*i != 0)
            delete *i;
    }
    for (std::vector<IAIInformationGrid*>::iterator i = m_gridsA.begin(); i != m_gridsA.end(); ++i)
    {
        if (*i != 0)
            delete *i;
    }
}

namespace {

// The query predicates of this TU (anonymous in retail: vtables ELF 0x4BE548..0x4BE5A8, bodies
// 0x2E7E80..0x2E8040, all weak). +4 is each one's single field.

// Units of one side.
class InfoSideUnitPredicate : public IAIInformationGridPredicate
{
public:
    explicit InfoSideUnitPredicate(int side) : m_side(side) {}
    // ELF 0x2E7F20. The side first, then the unit plug-in.
    virtual bool includeInformation(Information* information) const
    {
        return information->getSide() == m_side && information->getUnit() != 0;
    }
    int m_side;
};

// Units of one side or of side 0.
class InfoSideAndNeutralUnitPredicate : public IAIInformationGridPredicate
{
public:
    explicit InfoSideAndNeutralUnitPredicate(int side) : m_side(side) {}
    // ELF 0x2E7F70. The unit plug-in first; getSide is called a second time for the 0 test.
    virtual bool includeInformation(Information* information) const
    {
        if (information->getUnit() == 0)
            return false;
        return information->getSide() == m_side || information->getSide() == 0;
    }
    int m_side;
};

// Any unit.
class InfoUnitPredicate : public IAIInformationGridPredicate
{
public:
    // ELF 0x2E7F00.
    virtual bool includeInformation(Information* information) const
    {
        return information->getUnit() != 0;
    }
};

// Everything but one record.
class InfoCullSelfPredicate : public IAIInformationGridPredicate
{
public:
    explicit InfoCullSelfPredicate(Information* self) : m_self(self) {}
    // ELF 0x2E8020.
    virtual bool includeInformation(Information* information) const
    {
        return m_self != information;
    }
    Information* m_self;
};

// Both of two predicates, the first first.
class CompositeInfoPredicate : public IAIInformationGridPredicate
{
public:
    CompositeInfoPredicate(IAIInformationGridPredicate* first, IAIInformationGridPredicate* second)
        : m_first(first), m_second(second)
    {
    }
    // ELF 0x2E7FC0.
    virtual bool includeInformation(Information* information) const
    {
        return m_first->includeInformation(information) && m_second->includeInformation(information);
    }
    IAIInformationGridPredicate* m_first;    // +4
    IAIInformationGridPredicate* m_second;   // +8
};

// Cover objects of one side or of side 0. ITStructure accepts any side; ITAir and ITNaval reject.
// Bit names: InformationType (AIDLL_r 0x100D3010); AIDLL_r's version of this predicate (0x101A83C0)
// asserts "Object has no cover plugin but is of type ITCover" on the ITCover bit.
class InfoCoverPredicate : public IAIInformationGridPredicate
{
public:
    explicit InfoCoverPredicate(int side) : m_side(side) {}
    // ELF 0x2E7E80.
    virtual bool includeInformation(Information* information) const
    {
        if (!information->isOfType(ITCover) || information->getCover() == 0)
            return false;
        if (information->isOfType(ITStructure))
            return true;
        const int side = information->getSide();
        const bool sideOk = m_side == side || side == 0;
        return sideOk && !information->isOfType(ITAir)
               && !information->isOfType(ITNaval);
    }
    int m_side;
};

// Units of another side, when vehicleOccupied says the record counts for that side.
class InfoEnemyPredicate : public IAIInformationGridPredicate
{
public:
    explicit InfoEnemyPredicate(int side) : m_side(side) {}
    // ELF 0x2E8040.
    virtual bool includeInformation(Information* information) const
    {
        if (information->getUnit() == 0 || m_side == information->getSide())
            return false;
        return vehicleOccupied(information->getHandle(), m_side);
    }
    int m_side;
};

// The bot's own record through Bot +0xD8 (getCurrentEquipment).
Information* informationOf(const Bot* bot)
{
    return informationManager->getInformation(bot->getCurrentEquipment());
}

} // anonymous namespace

// ELF 0x2E6F40. `out` is cleared after the lookup; a bot without a record gets nothing. The
// bot's side is read twice (predicate, then the query's side argument).
void BFInformationEnvironment::getFriendlyUnits(std::vector<unsigned int>& out,
                                                const Vec3& position, Bot* bot, float radius) const
{
    Information* const self = informationOf(bot);
    out.erase(out.begin(), out.end());
    if (self == 0)
        return;
    InfoSideUnitPredicate keep(bot->getSide());
    getObjectsWithinRadius(out, position, radius, bot->getSide(), true, &keep);
}

// ELF 0x2E7030. As getFriendlyUnits, side-0 units included.
void BFInformationEnvironment::getFriendlyAndNeutralUnits(std::vector<unsigned int>& out,
                                                          const Vec3& position, Bot* bot,
                                                          float radius) const
{
    Information* const self = informationOf(bot);
    out.erase(out.begin(), out.end());
    if (self == 0)
        return;
    InfoSideAndNeutralUnitPredicate keep(bot->getSide());
    getObjectsWithinRadius(out, position, radius, bot->getSide(), true, &keep);
}

// ELF 0x2E7120. Both grids (`which` true, then false) within the bot's mobile plug-in's cover
// search radius; `out` is appended to. Retail reads the record's mobile plug-in without testing
// the record for null.
void BFInformationEnvironment::getCover(const Bot& bot, std::vector<unsigned int>& out) const
{
    Information* const self = informationOf(&bot);
    IPIMobile* const mobile = self->getMobile();
    if (mobile == 0)
        return;
    const float radius = mobile->getCoverSearchRadius();
    InfoCoverPredicate keep(bot.getSide());
    getObjectsWithinRadius(out, radius, bot, true, &keep);
    getObjectsWithinRadius(out, radius, bot, false, &keep);
}

// ELF 0x2E7220. Units within 75 m except the bot's own record; 0 (and nothing) without a record.
float BFInformationEnvironment::getHardware(std::vector<unsigned int>& out, const Vec3& position,
                                            Bot* bot) const
{
    Information* const self = informationOf(bot);
    if (self == 0)
        return 0.0f;
    return getHardware(out, position, 75.0f, bot->getSide(), self);
}

// ELF 0x2E72C0. Units within `radius` (minus `self` when given), side `side` and side 0 of the
// first grid; answers the radius.
float BFInformationEnvironment::getHardware(std::vector<unsigned int>& out, const Vec3& position,
                                            float radius, int side, Information* self) const
{
    InfoUnitPredicate units;
    InfoCullSelfPredicate notSelf(self);
    CompositeInfoPredicate both(&units, &notSelf);
    IAIInformationGridPredicate* const keep =
        self != 0 ? static_cast<IAIInformationGridPredicate*>(&both) : &units;
    getObjectsWithinRadius(out, position, radius, side, true, keep);
    return radius;
}

// ELF 0x2E7360. A negative radius means 600 m when the bot's record has object flag 0x10 and
// 75 m otherwise (retail does not test the record for null there).
void BFInformationEnvironment::getEnemyObjects(std::vector<unsigned int>& out, Bot* bot,
                                               float radius) const
{
    Information* const self = informationOf(bot);
    if (radius < 0.0f)
        radius = self->isOfType(ITAir) ? 600.0f : 75.0f;
    InfoEnemyPredicate keep(bot->getSide());
    getObjectsWithinRadius(out, radius, *bot, true, &keep);
}

// ELF 0x2E7430. Around the bot's record position, for its side; nothing without a record.
void BFInformationEnvironment::getObjectsWithinRadius(std::vector<unsigned int>& out, float radius,
                                                     const Bot& bot, bool which,
                                                     IAIInformationGridPredicate* keep) const
{
    Information* const self = informationOf(&bot);
    if (self == 0)
        return;
    const int side = bot.getSide();
    getObjectsWithinRadius(out, self->getPosition(), radius, side, which, keep);
}

// ELF 0x2E74F0: flatten XZ, then delegate
// virtually to the Vec2 overload (ELF +0x28 / PE +0x1C).
void BFInformationEnvironment::getObjectsWithinRadius(std::vector<unsigned int>& out,
                                                     const Vec3& position, float radius,
                                                     int side, bool which,
                                                     IAIInformationGridPredicate* keep) const
{
    Vec2 flat;
    flat.x = 0.0f;
    flat.y = 0.0f;
    vec3ToVec2XZ(position, flat);
    getObjectsWithinRadius(out, flat, radius, side, which, keep);
}

// ELF 0x2E7570. Query side zero first
// then look up the requested side after that callback. Both calls remain when
// side is zero. The grid query is const; output is appended to, never cleared.
void BFInformationEnvironment::getObjectsWithinRadius(std::vector<unsigned int>& out,
                                                     const Vec2& position, float radius,
                                                     int side, bool which,
                                                     IAIInformationGridPredicate* keep) const
{
    getInformationGrid(0, which)->getInformationWithinCircle(out, position, radius, keep);
    getInformationGrid(side, which)->getInformationWithinCircle(out, position, radius, keep);
}

// Query neutral side zero first
// then select the requested side after that callback. Both calls borrow the same corners
// output and predicate. Side zero is queried twice when requested; output is never cleared.
void BFInformationEnvironment::getObjectsWithinRectangle(std::vector<unsigned int>& out,
                                                        const Vec2& min, const Vec2& max,
                                                        int side, bool which,
                                                        IAIInformationGridPredicate* keep) const
{
    getInformationGrid(0, which)->getInformationWithinRectangle(out, min, max, keep);
    getInformationGrid(side, which)->getInformationWithinRectangle(out, min, max, keep);
}

// ELF 0x2E76E0. Side 0 first, then `side` (side 0 twice when asked for it), as the radius and
// rectangle queries.
void BFInformationEnvironment::getObjectsWithinFrustum(std::vector<unsigned int>& out,
                                                      const Frustum& frustum, int side, bool which,
                                                      IAIInformationGridPredicate* keep) const
{
    getInformationGrid(0, which)->getInformationWithinFrustum(out, frustum, keep);
    getInformationGrid(side, which)->getInformationWithinFrustum(out, frustum, keep);
}

void BFInformationEnvironment::getAllInformation(std::vector<unsigned int>& out, int side, bool which,
                                                 IAIInformationGridPredicate* keep) const
{
    // Const environment access still invokes the mutable
    // grid sweep, including stale-record removal; it does not use the read-only overload.
    getInformationGrid(side, which)->getAllInformation(out, keep, nullptr);
}

void BFInformationEnvironment::getAllInformation(std::vector<unsigned int>& out, int side, bool which,
                                                 IAIInformationGridPredicate* keep,
                                                 IAIInformationGridPredicate* cull)
{
    // No side/grid guard and no clearing of the output.
    getInformationGrid(side, which)->getAllInformation(out, keep, cull);
}

// the side's grid (unit or other, by `which`) takes the handle
// IAIInformationGrid +2 (ELF +0x08, PE +0x04). No side guard, as in getInformationGrid. The PE
// inlines the grid lookup; the ELF calls it.
void BFInformationEnvironment::addInfo(int side, bool which, unsigned int handle)
{
    getInformationGrid(side, which)->addInfo(handle);
}

// the same, through +6 addAndUpdatePosition (ELF +0x18, PE
// +0x14).
void BFInformationEnvironment::addAndUpdatePosition(int side, bool which, unsigned int handle)
{
    getInformationGrid(side, which)->addAndUpdatePosition(handle);
}

// ELF 0x2E78E0: the grid's +5 updatePosition (ELF +0x14). The ELF zero-extends the grid's bool
// into its own result; every caller in both images drops it, so the declared void stays.
void BFInformationEnvironment::updatePosition(int side, bool which, unsigned int handle)
{
    getInformationGrid(side, which)->updatePosition(handle);
}

// ELF 0x2E7930: the information's +8 removeAllContacts first, then the grid's +3 removeInfo
// (ELF +0x0C). The handle is resolved through the information pool inline; a stale handle
// yields null and the shipped call dereferences it unguarded.
void BFInformationEnvironment::removeInfo(int side, bool which, unsigned int handle)
{
    informationManager->getInformation(handle)->removeAllContacts();
    getInformationGrid(side, which)->removeInfo(handle);
}

// ELF 0x2E79C0: as removeInfo, but through the grid's +4 safeRemoveInfo (ELF +0x10) with a null
// removal predicate. The grid's bool is not used.
void BFInformationEnvironment::safeRemoveInfo(int side, bool which, unsigned int handle)
{
    informationManager->getInformation(handle)->removeAllContacts();
    getInformationGrid(side, which)->safeRemoveInfo(handle, nullptr);
}

// ELF 0x2E7A50: the grid's +4 safeRemoveInfo with a null predicate, without touching the
// information's contacts.
void BFInformationEnvironment::destroyRemoveInfo(int side, bool which, unsigned int handle)
{
    getInformationGrid(side, which)->safeRemoveInfo(handle, nullptr);
}

// Selects one of two grid sets by which, then indexes it by side.
// The caller must supply a valid side; no bounds check is performed.
IAIInformationGrid* BFInformationEnvironment::getInformationGrid(int side, bool which) const
{
    return which ? m_gridsB[side] : m_gridsA[side];
}

} // namespace ai
} // namespace hfe
} // namespace dice
