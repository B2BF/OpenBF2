// Two-team artillery manager and strike handler.

#ifndef GAME_OBJECTS_GAME_ARTILLERYHANDLER_H
#define GAME_OBJECTS_GAME_ARTILLERYHANDLER_H

#include "../../../Core/WeakPtr.h"
#include "../../../Math/Vec3.h"

#include <list>

namespace dice {
namespace hfe {
namespace world {

class IObject;
class RemoteControlledObject;

struct ArtilleryObjectStruct
{
    RemoteControlledObject* artillery;
    RemoteControlledObject* horizontalTurnObject;
    RemoteControlledObject* verticalTurnObject;
    bool missionFailed;
};

class ArtilleryHandler
{
public:
    explicit ArtilleryHandler(int team);
    virtual ~ArtilleryHandler();

    void findArtilleryPieces();
    void update(float deltaTime);
    bool setTarget(Vec2& target);
    bool setTarget(IObject* target);
    void requestFire();
    void failedFire();
    void doFire();
    bool haveAnyArtillery();
    // [INFERRED] name: inlined into BFCommander::gameHasArtillery (bf2 0x840FCE0), which tests
    // the list header at +4 against its first node.
    bool hasArtilleryPieces() const { return !m_artillery.empty(); }

private:
    void findHelperObjects(ArtilleryObjectStruct& artillery);
    bool calcArtilleryAngles(Vec3& from, Vec3& to, float velocity, float gravity,
                             RemoteControlledObject* horizontalTurnObject,
                             RemoteControlledObject* verticalTurnObject,
                             Vec3& projectileDirection, Vec3& horizontalDirection,
                             Vec3& verticalDirection);
    void reportFire(const Vec3& position, bool accepted);

    std::list<ArtilleryObjectStruct> m_artillery;
    int                              m_team;
    WeakPtr<IObject>                 m_targetObject;
    int                              m_minIntervalTicks;
    Vec2                             m_target;
    int                              m_fireAtTick;
    bool                             m_firePending;
    float                            m_activationScale;            // ctor 2.0; role unread
    int                              m_fireDelayTicks;
    bool                             m_missionActive;
};

class ArtilleryManager
{
public:
    ArtilleryManager();
    ~ArtilleryManager();

    void update(float deltaTime);
    void init();
    void removeAll();
    void reset();
    void refresh();

    // Retail performs this exact one-based, unbounded array access. Team 0 and values above 2
    // are not made safe here because ArtilleryEvent's wire behavior depends on the shipped quirk.
    ArtilleryHandler* getHandler(unsigned char team) { return m_handlers[team - 1]; }

private:
    ArtilleryHandler* m_handlers[2];
};

extern ArtilleryManager* artilleryManager;                        // Core/Globals.cpp

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_GAME_ARTILLERYHANDLER_H
