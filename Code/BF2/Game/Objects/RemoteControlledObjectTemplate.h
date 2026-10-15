// Remote-controlled object configuration. Member names follow console properties.
// [INFERRED] Source placement.

#ifndef GAME_OBJECTS_REMOTECONTROLLEDOBJECTTEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_REMOTECONTROLLEDOBJECTTEMPLATE_H

#include "Base/BundleTemplate.h"

#include <iosfwd>
#include <string>

namespace dice {
namespace hfe {
class GameLogic;
class ServerConnection;
namespace world {

class UAVVehicleManager;
class UAVVehicle;
class CommanderToolsManager;
class ArtilleryHandler;
class TurnableRemoteControlledObject;
struct GameTemplateConsoleAccess;

// The exact ObjectTemplate.rcType console vocabulary (PE 0x47D990 / 0x47F7B0). The ranged
// binding advertises 0..6 even though the named writer has cases only through 5; that upper
// unnamed value is shipped behavior and is deliberately preserved.
enum RemoteControlledObjectType : int
{
    RCNone = 0,
    RCArtillery = 1,
    RCSatellite = 2,
    RCUav = 3,
    RCHorizontalTurn = 4,
    RCVerticalTurn = 5
};

std::ostream& operator<<(std::ostream&, RemoteControlledObjectType);
std::istream& operator>>(std::istream&, RemoteControlledObjectType&);

class RemoteControlledObjectTemplate : public BundleTemplate
{
public:
    friend class hfe::GameLogic;              // the RCO registry reads rcType
    friend class hfe::ServerConnection;       // getSpecialCommanderObjects reads rcType
    friend class UAVVehicleManager;            // UAV spawn/config reads the +0x188..+0x198 block
    friend class UAVVehicle;                   // the live drone reads uavVehicleSpeed each tick
    friend class CommanderToolsManager;        // commander-toy aggregation reads rcType
    friend class ArtilleryHandler;             // artillery discovery reads rcType/min activation time
    friend struct GameTemplateConsoleAccess;   // recovered ObjectTemplate.* binding executors
    RemoteControlledObjectTemplate();
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;
    virtual unsigned int getClassID() const;
    virtual ~RemoteControlledObjectTemplate();
    virtual void* queryImplementation(unsigned int);
    virtual IObject* createObject() const;
    virtual bool  makeScript(io::IStream*);      // complete, emits type/cooldown

protected:
    /* 0x17C */ int   m_refCount;                 // read directly from addRef / getRef

    // offsets read from the console bindings; types inferred
    /* 0x180 */ int   rcType;
    /* 0x184 */ float minTimeBetweenActivation;   // [INFERRED] float - a time

    // UAV template name.
    /* 0x188 */ std::string uavVehicleTemplate;   // the ctor constructs a
                                                  // std::string here from the empty literal, so
                                                  // the "int or std::string" question is settled.
                                                  // It names a template, as the name implies.
    /* 0x18C */ float uavVehicleSpeed;            // [INFERRED] float
    /* 0x190 */ float uavVehicleRadius;           // [INFERRED] float
    /* 0x194 */ float uavVehicleFlightHeight;     // [INFERRED] float
    // float, not int: UAVVehicleManager::initUAVVehicle loads it with `fld dword ptr` in both
    // shipped ports, compares it as a float and divides a converted integer sample index by it.
    /* 0x198 */ float uavVehicleSampleResolution;
};

// Remote-controlled template with turn velocity and a separate leaf reference count.
class TurnableRemoteControlledObjectTemplate : public RemoteControlledObjectTemplate
{
public:
    friend class TurnableRemoteControlledObject; // mission update reads turnVelocity/rcType
    friend struct GameTemplateConsoleAccess;     // recovered turnVelocity binding executor
    TurnableRemoteControlledObjectTemplate();     // read in full

    virtual void  addRef();                       // refcount +0x19C
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;
    virtual unsigned int getClassID() const;
    virtual ~TurnableRemoteControlledObjectTemplate();
    virtual void* queryImplementation(unsigned int);    // hop -> the base's chain
    virtual IObject* createObject() const;        // new(0x198) TurnableRemoteControlled
                                                  // Object, complete
    virtual bool  makeScript(io::IStream*);       // complete

protected:
    /* 0x19C */ int   m_refCount;                 // ctor 1
    /* 0x1A0 */ float turnVelocity;               // ctor 0 - [INFERRED] float, and the ctor's
                                                  // zero store is consistent with that width
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_REMOTECONTROLLEDOBJECTTEMPLATE_H
