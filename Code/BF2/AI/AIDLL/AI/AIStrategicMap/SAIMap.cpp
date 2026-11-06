// Strategic grid map. Evidence: libaidll.so (ia-32) 0x35FD10..0x364544, the TU named by the
// anonymous-namespace symbols AI_AIDLL_AI_AIStrategicMap_SAIMap.cpp.

#include "SAIMap.h"

#include <cmath>
// <iostream>'s ios_base::Init object and defaultModifiers are this TU's static initialisation
// (ELF 0x364330, __tcf_0 0x3644C0, _GLOBAL__I 0x3644F0).
#include <iostream>

#include "SAIMapSquare.h"
#include "ScalarMap.h"
#include "TemperatureTreeModifiers.h"
#include "../AIKnowledge/AIInformationGrid.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategic/StatusMap.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// ELF: the modifiers a SAIMap uses when constructed without any.
TemperatureTreeModifiers* defaultModifiers = checkPointer(new TemperatureTreeModifiers());

// ELF 0x364510 (weak; vtable 0x4BF808, one slot): information whose security is below 0.1.
class SecurityPredicate : public IAIInformationGridPredicate
{
public:
    bool includeInformation(Information* information) const override
    {
        return 0.1f > information->getSecurity();
    }
};

} // namespace

// ELF 0x35FD10 (C2; C1 0x3618D0). Every allocation goes through checkPointer. The square sizes
// divide by the dimension arguments; the half-diagonal of a square is the bounding radius. Only
// buffer 0 of each layer is initialised. +0x24, +0x3C, +0x6C's values and the update state's
// index are left unwritten, as shipped.
SAIMap::SAIMap(int side, int dimX, int dimY, Vec2 min, Vec2 max, TemperatureTreeModifiers* modifiers)
    : m_side(side)
    , m_dimX(dimX)
    , m_dimY(dimY)
    , m_min(min)
    , m_max(max)
    , m_currentBuffer(0)
    , m_modifiers(modifiers)
{
    m_layer44[0] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer44[1] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer4C[0] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer4C[1] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer54[0] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer54[1] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer5C[0] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer5C[1] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer64[0] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer64[1] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer6C = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_statusMaps[0] = checkPointer(new StatusMap(m_dimX, m_dimY));
    m_statusMaps[1] = checkPointer(new StatusMap(m_dimX, m_dimY));
    m_squares[0] = checkPointer(new SAIMapSquare[m_dimX * m_dimY]);
    m_squares[1] = checkPointer(new SAIMapSquare[m_dimX * m_dimY]);
    m_layer80[0] = checkPointer(new ScalarMap(m_dimX, m_dimY));
    m_layer80[1] = checkPointer(new ScalarMap(m_dimX, m_dimY));

    m_squareXSize = (max.x - min.x) / static_cast<float>(dimX);
    m_squareYSize = (max.y - min.y) / static_cast<float>(dimY);
    Vec2 half;
    half.x = m_squareXSize * 0.5f;
    half.y = m_squareYSize * 0.5f;
    m_squareBoundingCircleRadius = sqrtf(half.x * half.x + half.y * half.y);

    m_updateState = checkPointer(new UpdateState);
    m_updateState->m_state = 0;

    m_stateA = checkPointer(new float[44]);
    m_stateB = checkPointer(new float[44]);
    m_stateC = checkPointer(new float[44]);
    m_stateD = checkPointer(new float[44]);
    clearStates(m_stateA, m_stateC);
    clearStates(m_stateB, m_stateD);

    for (int x = 0; x < m_dimX; ++x)
    {
        for (int y = 0; y < m_dimY; ++y)
        {
            ScalarMap* map = m_layer44[m_currentBuffer];
            map->m_values[y * map->m_width + x] = 0.0f;
            map = m_layer4C[m_currentBuffer];
            map->m_values[y * map->m_width + x] = 0.0f;
            map = m_layer54[m_currentBuffer];
            map->m_values[y * map->m_width + x] = 0.0f;
            map = m_layer5C[m_currentBuffer];
            map->m_values[y * map->m_width + x] = 0.0f;
            map = m_layer64[m_currentBuffer];
            map->m_values[y * map->m_width + x] = 0.0f;
            StatusMap* const status = m_statusMaps[m_currentBuffer];
            status->m_statuses[y * status->m_width + x] = StatusMap::Neutral;
            map = m_layer80[m_currentBuffer];
            map->m_values[y * map->m_width + x] = 1.0f;
        }
    }

    if (!m_modifiers)
        m_modifiers = defaultModifiers;
}

// ELF 0x363490 (D2; D1 0x363790): the layers in member order except that +0x80/+0x84 go before
// the square arrays, then the update state and the four state rows. The modifiers are borrowed.
SAIMap::~SAIMap()
{
    if (m_layer44[0]) delete m_layer44[0];
    if (m_layer44[1]) delete m_layer44[1];
    if (m_layer4C[0]) delete m_layer4C[0];
    if (m_layer4C[1]) delete m_layer4C[1];
    if (m_layer54[0]) delete m_layer54[0];
    if (m_layer54[1]) delete m_layer54[1];
    if (m_layer5C[0]) delete m_layer5C[0];
    if (m_layer5C[1]) delete m_layer5C[1];
    if (m_layer64[0]) delete m_layer64[0];
    if (m_layer64[1]) delete m_layer64[1];
    if (m_layer6C) delete m_layer6C;
    if (m_statusMaps[0]) delete m_statusMaps[0];
    if (m_statusMaps[1]) delete m_statusMaps[1];
    if (m_layer80[0]) delete m_layer80[0];
    if (m_layer80[1]) delete m_layer80[1];
    if (m_squares[0]) delete[] m_squares[0];
    if (m_squares[1]) delete[] m_squares[1];
    if (m_updateState) delete m_updateState;
    if (m_stateA) delete[] m_stateA;
    if (m_stateB) delete[] m_stateB;
    if (m_stateC) delete[] m_stateC;
    if (m_stateD) delete[] m_stateD;
}

// ELF 0x363A90. A jump table over the update state (0..24): 0-3 collect every information id
// of both kinds, 4 add the side's insecure ones (SecurityPredicate as the second predicate),
// 5-7 visit the collected ids (resumable at m_index), 8-23 only advance, 24 flips the buffers
// and answers true. Each step checks the update-time deadline (timer read once) first; other
// states answer false.
bool SAIMap::update(SAI* sai, double budget)
{
    const int buffer = m_currentBuffer == 0;
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double deadline = timer->getUpdateTime() + budget;
    IAIInformationEnvironment* environment;
    switch (m_updateState->m_state)
    {
    case 0: case 1: case 2: case 3:
        environment = static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);
        environment->getAllInformation(m_updateState->m_ids, 0, true, 0, 0);
        environment->getAllInformation(m_updateState->m_ids, 0, false, 0, 0);
        m_updateState->m_state = 4;
        m_updateState->m_index = 0;
        if (timer->getUpdateTime() >= deadline)
            return false;
        // fall through
    case 4:
    {
        environment = static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);
        SecurityPredicate insecure;
        environment->getAllInformation(m_updateState->m_ids, m_side, true, 0, &insecure);
        environment->getAllInformation(m_updateState->m_ids, m_side, false, 0, &insecure);
        m_updateState->m_state = 5;
        m_updateState->m_index = 0;
        if (timer->getUpdateTime() >= deadline)
            return false;
    }
        // fall through
    case 5: case 6: case 7:
        for (unsigned int index = m_updateState->m_index; index < m_updateState->m_ids.size(); ++index)
        {
            if (timer->getUpdateTime() >= deadline)
            {
                m_updateState->m_index = index;
                return false;
            }
            Information* information = informationManager->getInformation(m_updateState->m_ids[index]);
            if (!information || information->isOfType(ITNoTemperature))
                continue;
            if (information->getSecurity() == 0.0f)
                continue;
            updateInformation(information, sai, buffer);
            int it;
            for (Information* secondary = informationManager->getInformation(information->getFirstSecondaryObject(it, m_side));
                 secondary;
                 secondary = informationManager->getInformation(information->getSecondaryObject(it, m_side)))
            {
                if (!secondary->isOfType(ITNoTemperature) && secondary->getSecurity() > 0.0f)
                    updateInformation(secondary, sai, buffer);
            }
        }
        m_updateState->m_state = 8;
        m_updateState->m_index = 0;
        m_updateState->m_ids.resize(0);
        if (timer->getUpdateTime() >= deadline)
            return false;
        // fall through
    case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15:
        m_updateState->m_state = 16;
        m_updateState->m_index = 0;
        if (timer->getUpdateTime() >= deadline)
            return false;
        // fall through
    case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23:
        m_updateState->m_state = 24;
        m_updateState->m_index = 0;
        if (timer->getUpdateTime() >= deadline)
            return false;
        // fall through
    case 24:
        m_updateState->m_state = 0;
        m_currentBuffer = buffer;
        return true;
    default:
        return false;
    }
}

// ELF 0x363FA0.
int SAIMap::getSide() const
{
    return m_side;
}

// ELF 0x363FB0.
int SAIMap::getMapDimensionX() const
{
    return m_dimX;
}

// ELF 0x363FC0.
int SAIMap::getMapDimensionY() const
{
    return m_dimY;
}

// ELF 0x363FD0.
TemperatureTreeModifiers* SAIMap::getTemperatureTreeModifiers() const
{
    return m_modifiers;
}

// ELF 0x363FE0.
void SAIMap::setTemperatureTreeModifiers(TemperatureTreeModifiers* modifiers)
{
    m_modifiers = modifiers;
}

// ELF 0x364000.
float SAIMap::getSquareArea() const
{
    return m_squareArea;
}

// ELF 0x364010.
float SAIMap::getSquareBoundingCircleRadius() const
{
    return m_squareBoundingCircleRadius;
}

// ELF 0x364020.
float SAIMap::getSquareXSize() const
{
    return m_squareXSize;
}

// ELF 0x364030.
float SAIMap::getSquareYSize() const
{
    return m_squareYSize;
}

// ELF 0x364040.
Vec2 SAIMap::getCoordFromSquareId(int id)
{
    Vec2 coord;
    coord.x = static_cast<float>(id % m_dimX) * m_squareXSize + m_min.x + m_squareXSize * 0.5f;
    coord.y = static_cast<float>(id / m_dimX) * m_squareYSize + m_min.y + m_squareYSize * 0.5f;
    return coord;
}

// ELF 0x3640C0: empty.
void SAIMap::setMapDimensionX(int)
{
}

// ELF 0x3640D0: empty.
void SAIMap::setMapDimensionY(int)
{
}

// ELF 0x3640E0: the relative position is formed in float and truncated after the scale.
bool SAIMap::getSquareId(int& x, int& y, const Vec3& position) const
{
    Vec2 relative;
    relative.x = (position.x - m_min.x) / (m_max.x - m_min.x);
    relative.y = (position.z - m_min.y) / (m_max.y - m_min.y);
    x = static_cast<int>(relative.x * static_cast<float>(m_dimX));
    y = static_cast<int>(relative.y * static_cast<float>(m_dimY));
    return x >= 0 && x < m_dimX && y >= 0 && y < m_dimY;
}

// ELF 0x364180: both 44-element rows to zero, the last element of each to one.
void SAIMap::clearStates(float* a, float* b)
{
    for (int i = 0; i <= 43; ++i)
    {
        a[i] = 0.0f;
        b[i] = 0.0f;
    }
    a[43] = 1.0f;
    b[43] = 1.0f;
}

// ELF 0x3641D0.
float SAIMap::setFastThreshold(float threshold)
{
    return m_fastThreshold = threshold;
}

// ELF 0x3641E0.
float SAIMap::getFastThreshold()
{
    return m_fastThreshold;
}

// ELF 0x3641F0. Only the square test and the final strength copy (+0x14 into +0x10) have an
// effect: the side classification, the occupancy query and the clamped strength sum are
// computed and unused, as shipped (the sum is never negative, so the early return never fires).
void SAIMap::updateInformation(Information* information, SAI* sai, int)
{
    int x;
    int y;
    if (!getSquareId(x, y, information->getPosition()))
        return;
    IPIUnit* const unit = information->getUnit();
    float total = 0.0f;
    float strengths[6];
    const bool isFriend = sai->isFriend(information->getSide());
    const bool isEnemy = sai->isEnemy(information->getSide());
    bool neutral = false;
    if (!isFriend && !isEnemy)
        neutral = true;
    else if (unit)
        unit->isOccupied();
    (void)neutral;
    if (unit)
    {
        for (int i = 0; i <= 5; ++i)
        {
            float strength = i < unit->getNBattleStrengths() ? unit->getBattleStrengths()[i] : 0.0f;
            if (!(strength > 0.0f))
                strength = 0.0f;
            strengths[i] = strength;
            total += strength;
        }
        if (0.0f > total)
            return;
    }
    information->setStrength(information->getBasicTemp());
}

} // namespace ai
} // namespace hfe
} // namespace dice
