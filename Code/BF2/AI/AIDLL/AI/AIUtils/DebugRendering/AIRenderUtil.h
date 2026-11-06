// Owns AI debug-drawing handles allocated by the primitive manager.
// The dedicated build leaves per-frame render operations empty; console-driven operations remain active.
// [RETIRE-WHEN: botmain-bases] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_AIRENDERUTIL_H     // [INFERRED] guard style
#define AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_AIRENDERUTIL_H

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../../../../../Math/Vec3.h"      // Vec2 and Vec3 are both typedefs in here

namespace dice {
namespace hfe {

class Frustum;

namespace ai {

class AILandingZone;
class AIStrategicArea;
class Bot;
class BotMain;
class DrawPrimitive;
class GuardPosition;
class PatrolPath;

// AI debug-rendering state. Offsets describe the original 116-byte object; its eight
// STL containers make the native layout compiler-dependent.
class AIRenderUtil
{
public:
    // The six primitive records. Field names are [INFERRED] from the delete slot each is routed to;
    // the binary knows them only as `DrawPrimitive*`.
    struct BotPrimitives
    {
        BotPrimitives();
        BotPrimitives(const BotPrimitives&);
        BotPrimitives(DrawPrimitive* line0, DrawPrimitive* line1, DrawPrimitive* circle0,
                      DrawPrimitive* circle1, DrawPrimitive* line2, DrawPrimitive* line3,
                      DrawPrimitive* text3d);

        // Explicit empty destructor; primitive deletion is a separate operation.
        ~BotPrimitives();

        void deletePrimitives();

        /* +0x00 */ DrawPrimitive* m_line0;
        /* +0x04 */ DrawPrimitive* m_line1;
        /* +0x08 */ DrawPrimitive* m_circle0;
        /* +0x0C */ DrawPrimitive* m_circle1;
        /* +0x10 */ DrawPrimitive* m_line2;
        /* +0x14 */ DrawPrimitive* m_line3;
        /* +0x18 */ DrawPrimitive* m_text3d;
    };

    struct StrategicAreaPrimitives
    {
        StrategicAreaPrimitives();
        // Every constructor resizes the circle vector to exactly two entries after copying it.
        StrategicAreaPrimitives(const StrategicAreaPrimitives&);
        ~StrategicAreaPrimitives();
        StrategicAreaPrimitives(DrawPrimitive* line0, DrawPrimitive* line1,
                                const std::vector<DrawPrimitive*>& lines, DrawPrimitive* circle0,
                                DrawPrimitive* circle1, DrawPrimitive* text3d,
                                const std::vector<DrawPrimitive*>& circles);

        void deletePrimitives();

        /* +0x00 */ DrawPrimitive* m_line0;
        /* +0x04 */ DrawPrimitive* m_line1;
        /* +0x08 */ std::vector<DrawPrimitive*> m_lines;
        /* +0x14 */ DrawPrimitive* m_circle0;
        /* +0x18 */ DrawPrimitive* m_circle1;
        /* +0x1C */ DrawPrimitive* m_text3d;
        /* +0x20 */ std::vector<DrawPrimitive*> m_circles;    // not emptied by deletePrimitives
    };

    struct LandingZonePrimitives
    {
        LandingZonePrimitives();
        LandingZonePrimitives(const LandingZonePrimitives&);
        LandingZonePrimitives(DrawPrimitive* line0, DrawPrimitive* line1, DrawPrimitive* line2,
                              DrawPrimitive* line3, DrawPrimitive* text3d);

        void deletePrimitives();

        /* +0x00 */ DrawPrimitive* m_line0;
        /* +0x04 */ DrawPrimitive* m_line1;
        /* +0x08 */ DrawPrimitive* m_line2;
        /* +0x0C */ DrawPrimitive* m_line3;
        /* +0x10 */ DrawPrimitive* m_text3d;
    };

    struct PatrolPathPrimitives
    {
        PatrolPathPrimitives();
        PatrolPathPrimitives(const PatrolPathPrimitives&);
        // the text comes first here and last in memory - see the header block.
        PatrolPathPrimitives(DrawPrimitive* text3d,
                             const std::vector<std::pair<DrawPrimitive*, DrawPrimitive*> >& segments);

        void deletePrimitives();

        /* +0x00 */ std::vector<std::pair<DrawPrimitive*, DrawPrimitive*> > m_segments;  // line, circle
        /* +0x0C */ DrawPrimitive* m_text3d;
    };

    struct GuardPositionPrimitives
    {
        GuardPositionPrimitives();
        GuardPositionPrimitives(const GuardPositionPrimitives&);
        // Same inversion as PatrolPathPrimitives.
        GuardPositionPrimitives(DrawPrimitive* text3d,
                                const std::pair<DrawPrimitive*, DrawPrimitive*>& marker);

        void deletePrimitives();

        /* +0x00 */ std::pair<DrawPrimitive*, DrawPrimitive*> m_marker;   // circle, line
        /* +0x08 */ DrawPrimitive* m_text3d;
    };

    struct CoverPositionPrimitives
    {
        CoverPositionPrimitives();
        CoverPositionPrimitives(const CoverPositionPrimitives&);
        CoverPositionPrimitives(DrawPrimitive* line0, DrawPrimitive* line1,
                                const std::vector<DrawPrimitive*>& linesA,
                                const std::vector<DrawPrimitive*>& linesB,
                                const std::vector<DrawPrimitive*>& linesC);

        void deletePrimitives();

        /* +0x00 */ DrawPrimitive* m_line0;
        /* +0x04 */ DrawPrimitive* m_line1;
        /* +0x08 */ std::vector<DrawPrimitive*> m_linesA;
        /* +0x14 */ std::vector<DrawPrimitive*> m_linesB;
        /* +0x20 */ std::vector<DrawPrimitive*> m_linesC;
    };

    AIRenderUtil();
    ~AIRenderUtil();

    bool init();                                                          // - `return true`
    void exit();                                                          // - empty in retail

    // the render half. Every one of these with a five-byte body is empty in retail.
    void setCoordinate(const Vec3& position);
    void renderStrategies();
    void renderBot(BotMain*, const std::pair<float, float>&, const std::pair<float, float>&,
                   float, const std::pair<float, float>&);                // - empty
    void addSensingLine(const Vec3& from, const Vec3& to, int side, bool visible);
    void drawArrow(float lifeTime, unsigned int colour, const Vec3& position, const Vec3& direction);
    void drawAxisPoint(float, unsigned int, const Vec3&, const Vec3&, float);
    void renderPath(int, bool, const std::vector<Vec3>&, unsigned int);   // - empty
    void renderWaypointList(BotMain*);                                    // - empty
    void renderPathFinding(BotMain*);                                     // - empty
    void renderAiming(BotMain*);                                          // - empty
    void renderStrategicArea(AIStrategicArea*);                           // - empty
    void renderLandingZone(AILandingZone*, const Vec3&);                  // - empty
    void renderBotStats(BotMain*, const std::pair<float, float>&, const std::pair<float, float>&,
                        float, const std::pair<float, float>&);           // - empty
    void renderBehaviourStats(BotMain*, std::vector<std::string>&);       // - empty
    void renderPatrolPath(const std::string&, PatrolPath*);
    void renderGuardPosition(const std::string&, GuardPosition*);
    void renderCoverPosition(int);
    void redenderPathfindingDebug();
                                                                          // dice's spelling
    void renderObstacleInfo(Bot*);                                        // - empty
    void initAIMeshRendering();                                           // - empty
    void cleanUpAIMeshRendering();                                        // - empty
    void renderAIMeshes();                                                // - empty

    // the unrender half.
    void unrenderAll();
    void unrenderBot(Bot*);
    void unrenderAllBots();
    void unrenderStrategicArea(AIStrategicArea*);
    void unrenderAllStrategicAreas();
    void unrenderPatrolPath(PatrolPath*);
    void unrenderAllPatrolPaths();
    void unrenderGuardPosition(GuardPosition*);
    void unrenderAllGuardPositions();
    void unrenderCoverPosition(int);
    void unrenderAllCoverPositions();
    void unrendenderPathfindingDebug();
                                                                          // dice's spelling again

    void updateNumberOfStrategicAreas();
    void updateNumberOfLandingZones();

    // the self-installing singleton. First-wins in the constructor, cleared by the destructor if
    // it is still us. `BF2AI::clear` reaches the object only through this. -ish.
    static AIRenderUtil* m_instance;

private:
    /* +0x00 */ std::vector<Vec3> m_sensingLine;      // rebuilt from scratch by every addSensingLine
    /* +0x0C */ std::vector<std::string> m_text;      // the 2D text lines setCoordinate and
                                                      // renderStrategies rebuild
    /* +0x18 */ std::vector<BotPrimitives> m_botPrimitives;              // indexed by Bot::getBotId
    /* +0x24 */ std::vector<StrategicAreaPrimitives> m_strategicAreaPrimitives;
    /* +0x30 */ std::vector<LandingZonePrimitives> m_landingZonePrimitives;
    /* +0x3C */ std::map<PatrolPath*, PatrolPathPrimitives*> m_patrolPathPrimitives;
    /* +0x48 */ std::map<GuardPosition*, GuardPositionPrimitives*> m_guardPositionPrimitives;
    /* +0x54 */ std::vector<CoverPositionPrimitives> m_coverPositionPrimitives;

    // The five loose handles, named by the delete slot each goes to.
    /* +0x60 */ DrawPrimitive* m_text2dA;             // setCoordinate's text; delete2dText, from unrenderAll
    /* +0x64 */ DrawPrimitive* m_text2dB;             // renderStrategies' text; delete2dText, from unrenderAll
    /* +0x68 */ DrawPrimitive* m_pathCircleA;         // deleteCircle, from unrendenderPathfindingDebug
    /* +0x6C */ DrawPrimitive* m_pathCircleB;         // deleteCircle, from unrendenderPathfindingDebug
    /* +0x70 */ DrawPrimitive* m_pathLine;            // deleteLine, from unrendenderPathfindingDebug
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIUTILS_DEBUGRENDERING_AIRENDERUTIL_H
