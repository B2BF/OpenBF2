// Requests a vehicle drop at the supplied map X/Z coordinates, with Y set to zero.
// The trailing bit is transmitted but unused by this event.
// [INFERRED] The four-scalar constructor remains inferred.

#ifndef GAME_GAMEEVENTS_DROPVEHICLEEVENT_H     // [INFERRED] guard style
#define GAME_GAMEEVENTS_DROPVEHICLEEVENT_H

#include "../Common/GameEvent.h"
#include "../../Math/Vec3.h"

namespace dice {
namespace hfe {

class DropVehicleEvent : public GameEvent                    // 0x18
{
public:
    // [INFERRED] argument order - no ELF constructor exists.
    DropVehicleEvent(float positionX, float positionZ, int team, bool field15);
    // The commander producer leaves the trailing serialized byte untouched.
    DropVehicleEvent(int team, Vec2 position);
    DropVehicleEvent()
        : m_positionX(0.0f), m_positionZ(0.0f), m_team(0), m_field15(false) {}
    virtual ~DropVehicleEvent() {}

    virtual int  getType() const;                            // GEDropVehicle (51)
    virtual void executeClient();                            // empty
    virtual bool executeServer(int connectionId);
    virtual bool deSerialize(io::BitStream* stream);
    virtual bool serialize(io::BitStream* stream);

protected:
    /* +0x0C */ float         m_positionX;
    /* +0x10 */ float         m_positionZ;
    /* +0x14 */ unsigned char m_team;
    /* +0x15 */ bool          m_field15;
};

} // namespace hfe
} // namespace dice

#endif // GAME_GAMEEVENTS_DROPVEHICLEEVENT_H
