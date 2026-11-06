#include "DropVehicleEvent.h"

#include "../../Core/EventManager.h"             // eventManager->postEvent
#include "../../Core/IBase.h"
#include "../../IO/BitStream.h"                  // readBits / writeBits
#include "../../Math/Vec3.h"
#include "../GameLogic/SquadManager/ISquadManager.h" // typed +0x70 dispatch

namespace dice {
namespace hfe {

extern IBase* squadManager;                      // ds:0x8ACBFA4 - Core/Globals.cpp (IBase*, no layout)

namespace {
// .rodata dword GEDropVehicle (0x8889260) = 51.
const int GEDropVehicle = 51;

// The notification executeServer raises: category ECGameLogic (0), event id 0x15 = 21.
const unsigned int NOTIFY_DROP_VEHICLE = 0x15u;

inline ISquadManager* squadManagerIface()
{
    return static_cast<ISquadManager*>(squadManager);
}

// The payload the notification carries, read off the stack at 0x8070481..0x80704CF: a Vec3 at
// offset 0 and the team as an int at offset 12. Its real type name is [UNRESOLVED] - it belongs to
// whatever listener handles event 21 in ECGameLogic, and nothing in this binary names it.
struct DropVehicleNotification
{
    Vec3 position;
    int  team;
};
} // namespace

// [INFERRED] argument order - no ELF constructor exists.
DropVehicleEvent::DropVehicleEvent(float positionX, float positionZ, int team, bool field15)
    : GameEvent()
    , m_positionX(positionX)
    , m_positionZ(positionZ)
    , m_team(static_cast<unsigned char>(team))
    , m_field15(field15)
{
}

// PE7B2460, all16 instructions read. Do not delegate to the default ctor:
// ELF 0x840F690 inlines the same producer in BFCommander::dropVehicle.
// shipped defect: m_field15 is not initialized, even though serialize reads it.
DropVehicleEvent::DropVehicleEvent(int team, Vec2 position) : GameEvent()
{
    m_positionX = position.x;
    m_positionZ = position.y;
    m_team = static_cast<unsigned char>(team);
}

int DropVehicleEvent::getType() const
{
    return GEDropVehicle;
}

// an explicit empty override. The dropped vehicle reaches the client as an
// ordinary CreateObjectEvent, not through this one.
void DropVehicleEvent::executeClient()
{
}

// Posts a vehicle-drop request to the game-logic listener when the connection's
// low byte matches the team's commander. Requires an initialized squad manager;
// vehicle creation is handled by the listener.
bool DropVehicleEvent::executeServer(int connectionId)
{
    if (squadManagerIface()->getCommander(m_team)                            // +0x70
            != static_cast<unsigned char>(connectionId))                     // byte-truncated
        return false;

    DropVehicleNotification notification;
    notification.position.x = m_positionX;
    notification.position.y = 0.0f;                                          // hard-zeroed by the ELF
    notification.position.z = m_positionZ;
    notification.team       = m_team;

    eventManager->postEvent(ECGameLogic, NOTIFY_DROP_VEHICLE, &notification, 0.0f);   // +0x14
    return true;
}

// deSerialize - 0x8070280, 167 B. Two raw 32-bit floats, a byte, then one bit.
bool DropVehicleEvent::deSerialize(io::BitStream* stream)
{
    stream->readBits(&m_positionX, 32);
    stream->readBits(&m_positionZ, 32);

    int team = 0;
    stream->readBits(&team, 8);
    m_team = static_cast<unsigned char>(team);

    int field15 = 0;
    stream->readBits(&field15, 1);
    m_field15 = (field15 == 1);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

bool DropVehicleEvent::serialize(io::BitStream* stream)
{
    int type = getType();
    stream->writeBits(&type, gameEventTypeIdBitCount());

    stream->writeBits(&m_positionX, 32);
    stream->writeBits(&m_positionZ, 32);

    int team = m_team;
    stream->writeBits(&team, 8);

    int field15 = m_field15 ? 1 : 0;
    stream->writeBits(&field15, 1);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

} // namespace hfe
} // namespace dice
