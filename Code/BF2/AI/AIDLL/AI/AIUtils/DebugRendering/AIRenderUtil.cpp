// [INFERRED] Source placement.

// static init: ELF 0x3BA330 (__tcf_0 0x3BA390, _GLOBAL__I 0x3BA3C0): <iostream>'s Init object only.
#include <iostream>
#include <cmath>                                        // drawArrow's length
#include <sstream>                                      // setCoordinate, renderStrategies

#include "AIRenderUtil.h"

#include "../../../../AIInterface/BFKnowledge/BFTerrain.h"     // IAITerrain::getTerrainAltitude
#include "../../AIStrategic/SAI.h"
#include "../../AIStrategic/Strategy.h"
#include "../../AIStrategicEntity/CoverPosition/CoverPosition.h"
#include "../../AIStrategicEntity/CoverPosition/CoverPositionManager.h"
#include "../../Main/AIMain.h"                           // IAIMain::m_instance->getSAI
#include "../../Main/AIPathfinding.h"
#include "../../Main/AIPathfindingSearchTypes.h"         // QTPath, ClusterNodePath
#include "../AIVectorMath.h"
#include "../../AIStrategicEntity/GuardPosition.h"
#include "../../AIStrategicEntity/PatrolPath.h"
#include "../checkPointer.h"
#include "../../AIStrategicEntity/AILandingZone.h"
#include "../../AIStrategicEntity/AILandingZoneManager.h"
#include "../../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../../AIStrategicEntity/AIStrategicArea.h"
#include "../../DrawPrimitives/DPManager.h"
#include "../../DrawPrimitives/DPStaticCircle.h"
#include "../../DrawPrimitives/DPStaticLine.h"
#include "../../Main/AIConsole.h"
#include "../../Main/AIDllFrontier.h"
#include "../../Main/AISettingsInterfaces.h"
#include "../../Main/BotManager.h"
#include "../../AIBot/BotMain.h"                         // Bot::getBotId
#include "../../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

AIRenderUtil* AIRenderUtil::m_instance = 0;

// Primitive records require a live draw manager. Each deletePrimitives fetches it once
// and releases each handle through the matching primitive-type operation.

AIRenderUtil::BotPrimitives::BotPrimitives()
    : m_line0(0)
    , m_line1(0)
    , m_circle0(0)
    , m_circle1(0)
    , m_line2(0)
    , m_line3(0)
    , m_text3d(0)
{
}

AIRenderUtil::BotPrimitives::BotPrimitives(DrawPrimitive* line0, DrawPrimitive* line1,
                                           DrawPrimitive* circle0, DrawPrimitive* circle1,
                                           DrawPrimitive* line2, DrawPrimitive* line3,
                                           DrawPrimitive* text3d)
    : m_line0(line0)
    , m_line1(line1)
    , m_circle0(circle0)
    , m_circle1(circle1)
    , m_line2(line2)
    , m_line3(line3)
    , m_text3d(text3d)
{
}

// declared and empty on purpose - see the header. Removing it would change which temporaries get
// a destructor call in the emitted code.
// ELF 0x3B44B0 / 0x3B44F0. Copies the seven borrowed primitive handles.
AIRenderUtil::BotPrimitives::BotPrimitives(const BotPrimitives& other)
    : m_line0(other.m_line0), m_line1(other.m_line1), m_circle0(other.m_circle0),
      m_circle1(other.m_circle1), m_line2(other.m_line2), m_line3(other.m_line3), m_text3d(other.m_text3d)
{
}

AIRenderUtil::BotPrimitives::~BotPrimitives()
{
}

void AIRenderUtil::BotPrimitives::deletePrimitives()
{
    IDPManager* const manager = static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager);

    manager->deleteLine(m_line0);
    manager->deleteLine(m_line1);
    manager->deleteCircle(m_circle0);
    manager->deleteCircle(m_circle1);
    manager->deleteLine(m_line2);
    manager->deleteLine(m_line3);
    manager->delete3dText(m_text3d);
}

// ELF 0x3B3790 / 0x3B3860.
AIRenderUtil::StrategicAreaPrimitives::StrategicAreaPrimitives()
    : m_line0(0)
    , m_line1(0)
    , m_circle0(0)
    , m_circle1(0)
    , m_text3d(0)
{
    m_circles.resize(2, static_cast<DrawPrimitive*>(0));
}

// ELF 0x3B3930 / 0x3B3B00. Copies both vectors before resizing the circle vector.
AIRenderUtil::StrategicAreaPrimitives::StrategicAreaPrimitives(const StrategicAreaPrimitives& other)
    : m_line0(other.m_line0), m_line1(other.m_line1), m_lines(other.m_lines),
      m_circle0(other.m_circle0), m_circle1(other.m_circle1), m_text3d(other.m_text3d),
      m_circles(other.m_circles)
{
    m_circles.resize(2, static_cast<DrawPrimitive*>(0));
}

// ELF 0x3B4050 / 0x3B40C0. Only the vectors' storage is released; handles are borrowed here.
AIRenderUtil::StrategicAreaPrimitives::~StrategicAreaPrimitives()
{
}

// ELF 0x3B3CD0 / 0x3B3E90.
AIRenderUtil::StrategicAreaPrimitives::StrategicAreaPrimitives(
        DrawPrimitive* line0, DrawPrimitive* line1, const std::vector<DrawPrimitive*>& lines,
        DrawPrimitive* circle0, DrawPrimitive* circle1, DrawPrimitive* text3d,
        const std::vector<DrawPrimitive*>& circles)
    : m_line0(line0)
    , m_line1(line1)
    , m_lines(lines)
    , m_circle0(circle0)
    , m_circle1(circle1)
    , m_text3d(text3d)
    , m_circles(circles)
{
    m_circles.resize(2, static_cast<DrawPrimitive*>(0));
}

void AIRenderUtil::StrategicAreaPrimitives::deletePrimitives()
{
    IDPManager* const manager = static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager);

    manager->deleteLine(m_line0);
    manager->deleteLine(m_line1);

    for (std::vector<DrawPrimitive*>::iterator it = m_lines.begin(); it != m_lines.end(); ++it)
        manager->deleteLine(*it);

    m_lines.resize(0);

    manager->deleteCircle(m_circle0);
    manager->deleteCircle(m_circle1);
    manager->delete3dText(m_text3d);

    // Deletes circle primitives without clearing the vector; its entries remain after cleanup.
    for (std::vector<DrawPrimitive*>::iterator it = m_circles.begin(); it != m_circles.end(); ++it)
        manager->deleteCircle(*it);
}

AIRenderUtil::LandingZonePrimitives::LandingZonePrimitives()
    : m_line0(0)
    , m_line1(0)
    , m_line2(0)
    , m_line3(0)
    , m_text3d(0)
{
}

AIRenderUtil::LandingZonePrimitives::LandingZonePrimitives(DrawPrimitive* line0,
                                                           DrawPrimitive* line1,
                                                           DrawPrimitive* line2,
                                                           DrawPrimitive* line3,
                                                           DrawPrimitive* text3d)
    : m_line0(line0)
    , m_line1(line1)
    , m_line2(line2)
    , m_line3(line3)
    , m_text3d(text3d)
{
}

// ELF 0x3B4350 / 0x3B4380.
AIRenderUtil::LandingZonePrimitives::LandingZonePrimitives(const LandingZonePrimitives& other)
    : m_line0(other.m_line0), m_line1(other.m_line1), m_line2(other.m_line2),
      m_line3(other.m_line3), m_text3d(other.m_text3d)
{
}

void AIRenderUtil::LandingZonePrimitives::deletePrimitives()
{
    IDPManager* const manager = static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager);

    manager->deleteLine(m_line0);
    manager->deleteLine(m_line1);
    manager->deleteLine(m_line2);
    manager->deleteLine(m_line3);
    manager->delete3dText(m_text3d);
}

AIRenderUtil::PatrolPathPrimitives::PatrolPathPrimitives()
    : m_text3d(0)
{
}

AIRenderUtil::PatrolPathPrimitives::PatrolPathPrimitives(
        DrawPrimitive* text3d,
        const std::vector<std::pair<DrawPrimitive*, DrawPrimitive*> >& segments)
    : m_segments(segments)
    , m_text3d(text3d)
{
}

// ELF 0x3B46C0 / 0x3B4760.
AIRenderUtil::PatrolPathPrimitives::PatrolPathPrimitives(const PatrolPathPrimitives& other)
    : m_segments(other.m_segments), m_text3d(other.m_text3d)
{
}

void AIRenderUtil::PatrolPathPrimitives::deletePrimitives()
{
    IDPManager* const manager = static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager);

    // One line and one circle per patrol node: the leg and the waypoint marker.
    for (std::vector<std::pair<DrawPrimitive*, DrawPrimitive*> >::iterator it = m_segments.begin();
         it != m_segments.end(); ++it)
    {
        manager->deleteLine(it->first);
        manager->deleteCircle(it->second);
    }

    m_segments.resize(0);
    manager->delete3dText(m_text3d);
}

AIRenderUtil::GuardPositionPrimitives::GuardPositionPrimitives()
    : m_marker(static_cast<DrawPrimitive*>(0), static_cast<DrawPrimitive*>(0))
    , m_text3d(0)
{
}

AIRenderUtil::GuardPositionPrimitives::GuardPositionPrimitives(
        DrawPrimitive* text3d, const std::pair<DrawPrimitive*, DrawPrimitive*>& marker)
    : m_marker(marker)
    , m_text3d(text3d)
{
}

// ELF 0x3B4A70 / 0x3B4A90.
AIRenderUtil::GuardPositionPrimitives::GuardPositionPrimitives(const GuardPositionPrimitives& other)
    : m_marker(other.m_marker), m_text3d(other.m_text3d)
{
}

void AIRenderUtil::GuardPositionPrimitives::deletePrimitives()
{
    IDPManager* const manager = static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager);

    manager->deleteCircle(m_marker.first);
    manager->deleteLine(m_marker.second);
    manager->delete3dText(m_text3d);
}

AIRenderUtil::CoverPositionPrimitives::CoverPositionPrimitives()
    : m_line0(0)
    , m_line1(0)
{
}

AIRenderUtil::CoverPositionPrimitives::CoverPositionPrimitives(
        DrawPrimitive* line0, DrawPrimitive* line1, const std::vector<DrawPrimitive*>& linesA,
        const std::vector<DrawPrimitive*>& linesB, const std::vector<DrawPrimitive*>& linesC)
    : m_line0(line0)
    , m_line1(line1)
    , m_linesA(linesA)
    , m_linesB(linesB)
    , m_linesC(linesC)
{
}

// ELF 0x3B4C10 / 0x3B4E10.
AIRenderUtil::CoverPositionPrimitives::CoverPositionPrimitives(const CoverPositionPrimitives& other)
    : m_line0(other.m_line0), m_line1(other.m_line1), m_linesA(other.m_linesA),
      m_linesB(other.m_linesB), m_linesC(other.m_linesC)
{
}

void AIRenderUtil::CoverPositionPrimitives::deletePrimitives()
{
    IDPManager* const manager = static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager);

    manager->deleteLine(m_line0);
    manager->deleteLine(m_line1);

    // All three vectors are emptied here - compare StrategicAreaPrimitives, which forgets one.
    for (std::vector<DrawPrimitive*>::iterator it = m_linesA.begin(); it != m_linesA.end(); ++it)
        manager->deleteLine(*it);
    m_linesA.resize(0);

    for (std::vector<DrawPrimitive*>::iterator it = m_linesB.begin(); it != m_linesB.end(); ++it)
        manager->deleteLine(*it);
    m_linesB.resize(0);

    for (std::vector<DrawPrimitive*>::iterator it = m_linesC.begin(); it != m_linesC.end(); ++it)
        manager->deleteLine(*it);
    m_linesC.resize(0);
}

// Sizes debug-primitive collections for bots, strategic areas and landing zones.
AIRenderUtil::AIRenderUtil()
    : m_text2dA(0)
    , m_text2dB(0)
{
    if (!m_instance)
        m_instance = this;

    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

    const int maxBots = settings->getMaxNBots();
    if (maxBots != -1)
        m_botPrimitives.resize(maxBots);

    m_strategicAreaPrimitives.resize(
        AIStrategicObjectManager::getInstance()->getNumberOfStrategicObjects());

    m_landingZonePrimitives.resize(
        AILandingZoneManager::m_instance->getNumberOfLandingZones());

    m_pathCircleA = 0;
    m_pathCircleB = 0;
    m_pathLine = 0;
}

// `unrenderAll` first, and that is what makes the two maps leak-free - see the header block.
AIRenderUtil::~AIRenderUtil()
{
    if (m_instance == this)
        m_instance = 0;

    unrenderAll();
}

bool AIRenderUtil::init()
{
    return true;
}

// empty in retail - recovered, not stubbed.
void AIRenderUtil::exit()
{
}

// The thirteen empty bodies. Five bytes each in the shipped DLL. See the file header: these are
// recovered source, and a diagnostic stop in any of them would be a fabrication.

void AIRenderUtil::renderBot(BotMain*, const std::pair<float, float>&,
                             const std::pair<float, float>&, float, const std::pair<float, float>&)
{
}

void AIRenderUtil::renderPath(int, bool, const std::vector<Vec3>&, unsigned int)
{
}

void AIRenderUtil::renderWaypointList(BotMain*)
{
}

void AIRenderUtil::renderPathFinding(BotMain*)
{
}

void AIRenderUtil::renderAiming(BotMain*)
{
}

void AIRenderUtil::renderStrategicArea(AIStrategicArea*)
{
    // Its unrender counterpart at is of real work. Nothing this build draws
    // can ever need destroying, and the destroy path is kept anyway.
}

void AIRenderUtil::renderLandingZone(AILandingZone*, const Vec3&)
{
}

void AIRenderUtil::renderBotStats(BotMain*, const std::pair<float, float>&,
                                  const std::pair<float, float>&, float,
                                  const std::pair<float, float>&)
{
}

void AIRenderUtil::renderBehaviourStats(BotMain*, std::vector<std::string>&)
{
}

void AIRenderUtil::renderObstacleInfo(Bot*)
{
}

void AIRenderUtil::initAIMeshRendering()
{
}

void AIRenderUtil::cleanUpAIMeshRendering()
{
}

void AIRenderUtil::renderAIMeshes()
{
}

// Draws a two-point sensing line: side 1 is red, other sides blue, with a pale color
// when unseen. Clears and refills the member vertex vector on every call.
void AIRenderUtil::addSensingLine(const Vec3& from, const Vec3& to, int side, bool visible)
{
    m_sensingLine.resize(0);
    m_sensingLine.push_back(from);
    m_sensingLine.push_back(to);

    unsigned int colour;
    unsigned int paleColour;

    if (side == 1)
    {
        colour     = 0xFFFF0000u;      // red
        paleColour = 0xFFFFAAAAu;      // pale red
    }
    else
    {
        colour     = 0xFF0000FFu;      // blue
        paleColour = 0xFFAAAAFFu;      // pale blue
    }

    if (!visible)
        colour = paleColour;

    static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
        ->createStaticLine(0.1f, colour, m_sensingLine, 0.03f);
}

// Clears debug rendering. The manager null check covers only the two direct calls;
// primitive-record cleanup reached afterward still requires a live manager.
void AIRenderUtil::unrenderAll()
{
    if (CommonSingletonServer::m_iDPManager)
    {
        IDPManager* const manager = static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager);

        manager->delete2dText(m_text2dA);
        m_text2dA = 0;
        manager->delete2dText(m_text2dB);
        m_text2dB = 0;
    }

    unrenderAllBots();
    unrenderAllStrategicAreas();
    unrenderAllPatrolPaths();
    unrenderAllGuardPositions();
    unrenderAllCoverPositions();
    unrendenderPathfindingDebug();

    AIConsole::m_instance->removeCameraText();
}

// ELF 0x3B7410: the bot's id (+0xE8 getBotId) indexes the primitive records unchecked.
void AIRenderUtil::unrenderBot(Bot* bot)
{
    m_botPrimitives[bot->getBotId()].deletePrimitives();
}

// ELF 0x3B7460: with a bot manager, every index below getBotCount (+0x08) goes through
// getBotFromId (+0x18) to unrenderBot. The count is read once.
void AIRenderUtil::unrenderAllBots()
{
    IBotManager* const bots = IBotManager::instance;
    if (!bots)
        return;
    const int count = bots->getBotCount();
    for (int i = 0; i < count; ++i)
        unrenderBot(bots->getBotFromId(static_cast<unsigned int>(i)));
}

// ELF 0x3B7FD0: the area's index (+0xF0, AIStrategicObject::m_index) selects its primitive
// record, then each landing zone's id (AILandingZone::getId) selects a landing-zone record. The
// zone vector is fetched again for every end() comparison.
void AIRenderUtil::unrenderStrategicArea(AIStrategicArea* area)
{
    m_strategicAreaPrimitives[area->getIndex()].deletePrimitives();
    for (std::vector<AILandingZone*>::iterator zone = area->getLandingZones().begin();
         zone != area->getLandingZones().end(); ++zone)
        m_landingZonePrimitives[(*zone)->getId()].deletePrimitives();
}

// Copies the strategic-area map before iterating it to remove debug rendering.
// Preserve the by-value snapshot and its owned string keys.
void AIRenderUtil::unrenderAllStrategicAreas()
{
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    if (!manager)
        return;

    manager->undrawWayPoints();

    const std::map<std::string, AIStrategicArea*> areas = manager->getStrategicAreas();
    for (std::map<std::string, AIStrategicArea*>::const_iterator it = areas.begin();
         it != areas.end(); ++it)
        unrenderStrategicArea(it->second);
}

// Destroys primitives, deletes their owned record, then erases the map entry.
// Existing entries must hold nonnull records; the later null check does not guard first use.
void AIRenderUtil::unrenderPatrolPath(PatrolPath* path)
{
    const std::map<PatrolPath*, PatrolPathPrimitives*>::iterator it =
        m_patrolPathPrimitives.find(path);

    if (it == m_patrolPathPrimitives.end())
        return;

    it->second->deletePrimitives();
    if (it->second)
        delete it->second;

    m_patrolPathPrimitives.erase(it);
}

void AIRenderUtil::unrenderAllPatrolPaths()
{
    std::map<PatrolPath*, PatrolPathPrimitives*>::iterator it = m_patrolPathPrimitives.begin();
    while (it != m_patrolPathPrimitives.end())
    {
        it->second->deletePrimitives();
        if (it->second)
            delete it->second;

        m_patrolPathPrimitives.erase(it++);
    }
}

void AIRenderUtil::unrenderGuardPosition(GuardPosition* position)
{
    const std::map<GuardPosition*, GuardPositionPrimitives*>::iterator it =
        m_guardPositionPrimitives.find(position);

    if (it == m_guardPositionPrimitives.end())
        return;

    it->second->deletePrimitives();
    if (it->second)
        delete it->second;

    m_guardPositionPrimitives.erase(it);
}

void AIRenderUtil::unrenderAllGuardPositions()
{
    std::map<GuardPosition*, GuardPositionPrimitives*>::iterator it =
        m_guardPositionPrimitives.begin();

    while (it != m_guardPositionPrimitives.end())
    {
        it->second->deletePrimitives();
        if (it->second)
            delete it->second;

        m_guardPositionPrimitives.erase(it++);
    }
}

// an upper bound and no lower one - a negative index indexes before the buffer. Preserved.
void AIRenderUtil::unrenderCoverPosition(int index)
{
    if (index < static_cast<int>(m_coverPositionPrimitives.size()))
        m_coverPositionPrimitives[index].deletePrimitives();
}

void AIRenderUtil::unrenderAllCoverPositions()
{
    for (std::vector<CoverPositionPrimitives>::iterator it = m_coverPositionPrimitives.begin();
         it != m_coverPositionPrimitives.end(); ++it)
        it->deletePrimitives();
}

// dice's spelling - "unrendender". Kept.
// this is what names the last three members: two circles and a line, by their delete slots.
void AIRenderUtil::unrendenderPathfindingDebug()
{
    if (!CommonSingletonServer::m_iDPManager)
        return;

    IDPManager* const manager = static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager);

    manager->deleteCircle(m_pathCircleA);
    m_pathCircleA = 0;
    manager->deleteCircle(m_pathCircleB);
    m_pathCircleB = 0;
    manager->deleteLine(m_pathLine);
    m_pathLine = 0;
}

// Grows primitive storage when the world gains entities; never shrinks it.
// Preserves the second size read used to compute the resize argument.

void AIRenderUtil::updateNumberOfStrategicAreas()
{
    const int count = AIStrategicObjectManager::getInstance()->getNumberOfStrategicObjects();

    if (count > static_cast<int>(m_strategicAreaPrimitives.size()))
        m_strategicAreaPrimitives.resize(count);
}

void AIRenderUtil::updateNumberOfLandingZones()
{
    const int count = AILandingZoneManager::m_instance->getNumberOfLandingZones();

    if (count > static_cast<int>(m_landingZonePrimitives.size()))
        m_landingZonePrimitives.resize(count);
}

// ELF 0x3B65C0. Replaces the text lines with one "x/y/z" line for `position` and shows them as
// the white 2D text at the screen origin, creating m_text2dA on first use and updating it after.
// [INFERRED] The three '/'-separated inserts may be an inlined Vec3 stream operator.
void AIRenderUtil::setCoordinate(const Vec3& position)
{
    m_text.resize(0);
    std::stringstream stream;
    stream << position.x << '/' << position.y << '/' << position.z;
    m_text.push_back(stream.str());

    if (m_text2dA)
    {
        const Vec2 origin = {0.0f, 0.0f};
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)
            ->set2dTextAttributes(m_text2dA, 1.0e9f, 0xFFFFFFFFu, origin, m_text, 0,
                                  static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(0));
    }
    else
    {
        const Vec2 origin = {0.0f, 0.0f};
        m_text2dA = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
            ->createStatic2dText(1.0e9f, 0xFFFFFFFFu, origin, m_text, 0,
                                 static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(0));
    }
}

// ELF 0x3B6C10. Replaces the text lines with the active strategy of SAI sides 1 and 2, each as
// "name a b c" with the three values getActiveStrategy reports, and shows them as white 2D text
// twelve units down (m_text2dB). The strategy pointers are used unchecked (shipped).
void AIRenderUtil::renderStrategies()
{
    m_text.resize(0);
    std::stringstream stream;
    SAI* const side1 = IAIMain::m_instance->getSAI(1);
    SAI* const side2 = IAIMain::m_instance->getSAI(2);
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;

    stream << side1->getActiveStrategy(a, b, c)->getName() << " " << a << " " << b << " " << c;
    m_text.push_back(stream.str());
    stream.str("");

    stream << side2->getActiveStrategy(a, b, c)->getName() << " " << a << " " << b << " " << c;
    m_text.push_back(stream.str());

    if (m_text2dB)
    {
        const Vec2 origin = {0.0f, 12.0f};
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)
            ->set2dTextAttributes(m_text2dB, 1.0e9f, 0xFFFFFFFFu, origin, m_text, 0,
                                  static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(0));
    }
    else
    {
        const Vec2 origin = {0.0f, 12.0f};
        m_text2dB = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
            ->createStatic2dText(1.0e9f, 0xFFFFFFFFu, origin, m_text, 0,
                                 static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(0));
    }
}

// ELF 0x3B7670. One static polyline in m_sensingLine: from `position` to the tip
// (`position` + `direction`), then over three barbs back to the tip. The barbs sit at 70% of the
// length, offset by a tenth of the length along the frame's first row, a twentieth along its
// second row, and a tenth against the first row. The line is 3% of the length wide. The frame is
// getDofTransformation's (third row forward), as in drawAxisPoint. The ia-32 build forms
// 1 - 0.3 at run time, so the three fractions were locals in the source.
void AIRenderUtil::drawArrow(float lifeTime, unsigned int colour, const Vec3& position,
                             const Vec3& direction)
{
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y
                                   + direction.z * direction.z);
    const Mat4 frame = getDofTransformation(direction);
    Vec3 point = {0.0f, 0.0f, 0.0f};
    const float barbWidth = 0.1f;
    const float barbLength = 0.3f;
    const float barbHeight = 0.05f;

    m_sensingLine.resize(0, point);
    m_sensingLine.push_back(position);

    // the tip
    point.x = position.x + length * frame.m[8];
    point.y = position.y + length * frame.m[9];
    point.z = position.z + length * frame.m[10];
    m_sensingLine.push_back(point);

    // first barb: back along forward, out along the first row
    point.x = position.x + length * frame.m[8] * (1.0f - barbLength) + length * frame.m[0] * barbWidth;
    point.y = position.y + length * frame.m[9] * (1.0f - barbLength) + length * frame.m[1] * barbWidth;
    point.z = position.z + length * frame.m[10] * (1.0f - barbLength) + length * frame.m[2] * barbWidth;
    m_sensingLine.push_back(point);

    // second barb: out along the second row
    point.x = position.x + length * frame.m[8] * (1.0f - barbLength) + length * frame.m[4] * barbHeight;
    point.y = position.y + length * frame.m[9] * (1.0f - barbLength) + length * frame.m[5] * barbHeight;
    point.z = position.z + length * frame.m[10] * (1.0f - barbLength) + length * frame.m[6] * barbHeight;
    m_sensingLine.push_back(point);

    // third barb: against the first row
    point.x = position.x + length * frame.m[8] * (1.0f - barbLength) - length * frame.m[0] * barbWidth;
    point.y = position.y + length * frame.m[9] * (1.0f - barbLength) - length * frame.m[1] * barbWidth;
    point.z = position.z + length * frame.m[10] * (1.0f - barbLength) - length * frame.m[2] * barbWidth;
    m_sensingLine.push_back(point);

    // back to the tip
    point.x = position.x + length * frame.m[8];
    point.y = position.y + length * frame.m[9];
    point.z = position.z + length * frame.m[10];
    m_sensingLine.push_back(point);

    static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
        ->createStaticLine(lifeTime, colour, m_sensingLine, length * 0.03f);
}

// ELF 0x3B7DD0. Three static lines through `position`, one per axis of the frame
// getDofTransformation builds from `direction` (third row, then second, then first), each from
// position - axis to position + axis and a quarter of `size` wide.
void AIRenderUtil::drawAxisPoint(float lifeTime, unsigned int colour, const Vec3& position,
                                 const Vec3& direction, float size)
{
    const Mat4 frame = getDofTransformation(direction);
    Vec3 end;
    Vec3 start;
    size = size * 0.25f;

    end.x = position.x + frame.m[8];
    end.y = position.y + frame.m[9];
    end.z = position.z + frame.m[10];
    start.x = position.x - frame.m[8];
    start.y = position.y - frame.m[9];
    start.z = position.z - frame.m[10];
    static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
        ->createStaticLine(lifeTime, colour, start, end, size);

    end.x = position.x + frame.m[4];
    end.y = position.y + frame.m[5];
    end.z = position.z + frame.m[6];
    start.x = position.x - frame.m[4];
    start.y = position.y - frame.m[5];
    start.z = position.z - frame.m[6];
    static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
        ->createStaticLine(lifeTime, colour, start, end, size);

    end.x = position.x + frame.m[0];
    end.y = position.y + frame.m[1];
    end.z = position.z + frame.m[2];
    start.x = position.x - frame.m[0];
    start.y = position.y - frame.m[1];
    start.z = position.z - frame.m[2];
    static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
        ->createStaticLine(lifeTime, colour, start, end, size);
}

// ELF 0x3B83E0. Once per patrol path: for every waypoint a short vertical post (built in
// m_sensingLine, which serves as the scratch point list) and a unit circle, recorded as a
// (line, circle) segment; then the name five units above the first waypoint. The text handle is
// not stored (shipped, kept). The path is fetched again for every end() comparison.
void AIRenderUtil::renderPatrolPath(const std::string& name, PatrolPath* path)
{
    if (m_patrolPathPrimitives.find(path) != m_patrolPathPrimitives.end())
        return;
    PatrolPathPrimitives* const primitives = checkPointer(new PatrolPathPrimitives());
    IDPStaticCreator* const creator = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator);

    for (std::vector<Vec3>::const_iterator waypoint = path->getPath().begin();
         waypoint != path->getPath().end(); ++waypoint)
    {
        const Vec3 point = *waypoint;
        Vec3 vertex = {0.0f, 0.0f, 0.0f};
        m_sensingLine.resize(0, vertex);
        vertex.x = point.x;
        vertex.y = point.y - 2.5f;
        vertex.z = point.z + 0.1f;
        m_sensingLine.push_back(vertex);
        vertex.x = point.x;
        vertex.y = 2.5f + point.y;
        vertex.z = point.z - 0.1f;
        m_sensingLine.push_back(vertex);
        DrawPrimitive* const line = creator->createStaticLine(1.0e9f, 0xFF4422u, m_sensingLine, 0.2f);

        vertex.x = 0.0f;
        vertex.y = 1.0f;
        vertex.z = 0.0f;
        DrawPrimitive* const circle
            = creator->createStaticCircle(1.0e9f, 0xFF4422u, *waypoint, 1.0f, vertex, false, 255);
        primitives->m_segments.push_back(std::make_pair(line, circle));
    }

    const Vec3& first = path->getPath()[0];
    Vec3 textPosition;
    textPosition.x = first.x;
    textPosition.y = first.y + 5.0f;
    textPosition.z = first.z;
    static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
        ->createStatic3dText(1.0e9f, 0xFFFFFFFFu, textPosition, name, 0,
                             static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(0));
    m_patrolPathPrimitives.insert(std::make_pair(path, primitives));
}

// ELF 0x3B8BC0. Once per guard position: a circle of the minimum distance one unit above it, the
// name four units higher, and a line below the name. The text handle is not stored (the record's
// m_text3d stays at its default), and the line's upper end subtracts the circle height rather than
// an offset, so it ends near world height 4 (both shipped, kept).
void AIRenderUtil::renderGuardPosition(const std::string& name, GuardPosition* guardPosition)
{
    if (m_guardPositionPrimitives.find(guardPosition) != m_guardPositionPrimitives.end())
        return;
    GuardPositionPrimitives* const primitives = checkPointer(new GuardPositionPrimitives());
    IDPStaticCreator* const creator = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator);

    Vec3 position = guardPosition->getPosition();
    position.y = position.y + 1.0f;
    const float circleHeight = position.y;
    const Vec3 up = {0.0f, 1.0f, 0.0f};
    primitives->m_marker.first = creator->createStaticCircle(
        1.0e9f, 0x117799u, position, guardPosition->getMinDistance(), up, false, 255);

    position.y = 4.0f + position.y;
    static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
        ->createStatic3dText(1.0e9f, 0xFFFFFFFFu, position, name, 0,
                             static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(0));
    m_guardPositionPrimitives.insert(std::make_pair(guardPosition, primitives));

    Vec3 start;
    start.x = position.x - 0.0f;
    start.y = position.y - circleHeight;
    start.z = position.z - 0.0f;
    Vec3 end;
    end.x = position.x - 0.0f;
    end.y = position.y - 5.0f;
    end.z = position.z - 0.0f;
    primitives->m_marker.second = creator->createStaticLine(1.0e9f, 0x117799u, start, end, 0.1f);
}

// ELF 0x3B9130. Once per cover position (the record's first line is the marker): its box as a
// closed outline half a unit above the terrain, a post at its centre, and one line from the
// centre to each linked position, five units higher per link kind: move neighbours (linesA, to
// the other box's first corner plus half a unit, grey), attackable links (linesB, to the first
// corner, coloured by the link type) and defendable links (linesC, likewise). The record vector
// grows to fit the index; a negative index is not caught (shipped). The first loop fetches the
// creator singleton again; the other calls use the copy taken on entry.
void AIRenderUtil::renderCoverPosition(int index)
{
    CoverPositionManager* const manager = CoverPositionManager::m_instance;
    CoverPosition* const cover = manager->m_coverPositions[index];
    if (!cover)
        return;

    if (index >= static_cast<int>(m_coverPositionPrimitives.size()))
        m_coverPositionPrimitives.resize(index + 1);
    CoverPositionPrimitives& primitives = m_coverPositionPrimitives[index];
    if (primitives.m_line0)
        return;

    IDPStaticCreator* const creator = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator);
    const IAITerrain* const terrain = static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain);

    const Vec3 corner0 = {cover->m_min.x,
                          terrain->getTerrainAltitude(cover->m_min.x, cover->m_min.y) + 0.5f,
                          cover->m_min.y};
    const Vec3 corner1 = {cover->m_max.x,
                          terrain->getTerrainAltitude(cover->m_max.x, cover->m_min.y) + 0.5f,
                          cover->m_min.y};
    const Vec3 corner2 = {cover->m_max.x,
                          terrain->getTerrainAltitude(cover->m_max.x, cover->m_max.y) + 0.5f,
                          cover->m_max.y};
    const Vec3 corner3 = {cover->m_min.x,
                          terrain->getTerrainAltitude(cover->m_min.x, cover->m_max.y) + 0.5f,
                          cover->m_max.y};

    const Vec3 zero = {0.0f, 0.0f, 0.0f};
    m_sensingLine.resize(0, zero);
    m_sensingLine.push_back(corner0);
    m_sensingLine.push_back(corner1);
    m_sensingLine.push_back(corner2);
    m_sensingLine.push_back(corner3);
    m_sensingLine.push_back(corner0);
    primitives.m_line0 = creator->createStaticLine(1.0e9f, 0x77777777u, m_sensingLine, 0.2f);

    Vec3 centre = {0.0f, 0.0f, 0.0f};
    centre.x = cover->m_min.x + (cover->m_max.x - cover->m_min.x) * 0.5f;
    centre.z = cover->m_min.y + (cover->m_max.y - cover->m_min.y) * 0.5f;
    centre.y = terrain->getTerrainAltitude(centre.x, centre.z);

    Vec3 top;
    top.x = centre.x + 0.0f;
    top.y = centre.y + 15.0f;
    top.z = centre.z + 0.0f;
    Vec3 foot;
    foot.x = centre.x + 0.1f;
    foot.y = centre.y + 0.0f;
    foot.z = centre.z + 0.1f;
    primitives.m_line1 = creator->createStaticLine(1.0e9f, 0x77777777u, foot, top, 0.2f);

    centre.y = centre.y + 5.0f;
    Vec3 end = {0.0f, 0.0f, 0.0f};
    for (std::set<int>::const_iterator it = cover->m_moveNeighbours.begin();
         it != cover->m_moveNeighbours.end(); ++it)
    {
        const CoverPosition* const other = manager->m_coverPositions[*it];
        if (!other)
            continue;
        end.x = other->m_min.x + 0.5f;
        end.z = other->m_min.y + 0.5f;
        end.y = terrain->getTerrainAltitude(end.x, end.z) + 5.0f;
        primitives.m_linesA.push_back(
            static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
                ->createStaticLine(1.0e9f, 0xAAAAAAAAu, centre, end, 0.1f));
    }

    centre.y = centre.y + 5.0f;
    for (std::map<int, ConnectionType>::const_iterator it = cover->m_attackable.begin();
         it != cover->m_attackable.end(); ++it)
    {
        const CoverPosition* const other = manager->m_coverPositions[it->first];
        if (!other)
            continue;
        unsigned int colour;
        switch (it->second.m_type)
        {
        case ConnectionType::kPartial: colour = 0xBBBB00u; break;
        case ConnectionType::kBlocked: colour = 0xBB0000u; break;
        case ConnectionType::kVisible: colour = 0x00BB00u; break;
        default:                       colour = 0;         break;
        }
        end.x = other->m_min.x;
        end.z = other->m_min.y;
        end.y = terrain->getTerrainAltitude(end.x, end.z) + 10.0f;
        primitives.m_linesB.push_back(creator->createStaticLine(1.0e9f, colour, centre, end, 0.1f));
    }

    centre.y = centre.y + 5.0f;
    for (std::map<int, ConnectionType>::const_iterator it = cover->m_defendable.begin();
         it != cover->m_defendable.end(); ++it)
    {
        const CoverPosition* const other = manager->m_coverPositions[it->first];
        if (!other)
            continue;
        unsigned int colour;
        switch (it->second.m_type)
        {
        case ConnectionType::kPartial: colour = 0xBBBB00u; break;
        case ConnectionType::kBlocked: colour = 0xBB0000u; break;
        case ConnectionType::kVisible: colour = 0x00BB00u; break;
        default:                       colour = 0;         break;
        }
        end.x = other->m_min.x;
        end.z = other->m_min.y;
        end.y = terrain->getTerrainAltitude(end.x, end.z) + 15.0f;
        primitives.m_linesC.push_back(creator->createStaticLine(1.0e9f, colour, centre, end, 0.1f));
    }
}

// ELF 0x3B9D00 (dice's spelling, "redender"). The pathfinding debug tool's markers: the found
// path as one line (m_pathLine) once both search points are set, and a circle at each set search
// point (m_pathCircleA, m_pathCircleB), each created on first use and updated after. Setting the
// first point again deletes the line, and an unset point deletes its circle. The plain path is
// read through its own step cursor, which this advances; a cursor that does not start at zero
// runs off the end and copies through a null position (shipped). The cluster path skips its
// start step and ignores getPoint's answer.
void AIRenderUtil::redenderPathfindingDebug()
{
    AIPathfinding* const pathfinding = static_cast<AIPathfinding*>(IAIPathfinding::instance);

    if (m_pathLine)
    {
        if (pathfinding->getSearchPointToSet() == 1)
        {
            static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteLine(m_pathLine);
            m_pathLine = 0;
        }
    }
    else if (pathfinding->getSearchPointToSet() == 2)
    {
        const bool cluster = pathfinding->getClusterSearch();
        const unsigned int colour = cluster ? 0xFF00FF44u : 0xFFFF8800u;
        IDPStaticCreator* const creator = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator);
        std::vector<Vec3> points;

        if (!cluster)
        {
            QTPath* const path = pathfinding->getDebugPath();
            for (unsigned int i = 0; i < path->m_nodes.size(); ++i)
            {
                // [INFERRED] an inlined QTPath step accessor (unsigned compare, post-increment of
                // the cursor); no out-of-line copy exists to name it.
                const Vec3* position = 0;
                if (static_cast<unsigned int>(path->m_i0) < path->m_nodes.size())
                    position = &path->m_nodes[path->m_i0++].third;
                points.push_back(*position);
            }
        }
        else
        {
            const ClusterNodePath* const path = pathfinding->getDebugClusterPath();
            Vec3 point = {0.0f, 0.0f, 0.0f};
            unsigned int polygon;
            for (unsigned int i = 1; i < path->m_nodes.size(); ++i)
            {
                path->getPoint(point, polygon, i);
                points.push_back(point);
            }
        }

        m_pathLine = creator->createStaticLine(1.0e9f, colour, points, 0.3f);
    }

    if (pathfinding->getSearchPointToSet() > 0)
    {
        const Vec3 up = {0.0f, 1.0f, 0.0f};
        if (m_pathCircleA)
            static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)
                ->setCircleAttributes(m_pathCircleA, 1.0e9f, 0xFFFF8800u,
                                      *pathfinding->getSearchPoint(0), 1.0f, up, false, 255);
        else
            m_pathCircleA = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
                ->createStaticCircle(1.0e9f, 0xFFFF8800u, *pathfinding->getSearchPoint(0), 1.0f, up,
                                     false, 255);
    }
    else
    {
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteCircle(m_pathCircleA);
        m_pathCircleA = 0;
    }

    if (pathfinding->getSearchPointToSet() == 2)
    {
        const Vec3 up = {0.0f, 1.0f, 0.0f};
        if (m_pathCircleB)
            static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)
                ->setCircleAttributes(m_pathCircleB, 1.0e9f, 0xFFFF8800u,
                                      *pathfinding->getSearchPoint(1), 1.0f, up, false, 255);
        else
            m_pathCircleB = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
                ->createStaticCircle(1.0e9f, 0xFFFF8800u, *pathfinding->getSearchPoint(1), 1.0f, up,
                                     false, 255);
    }
    else
    {
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteCircle(m_pathCircleB);
        m_pathCircleB = 0;
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
