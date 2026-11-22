// Engine Python host and event listener. Construction allocates state but does not
// touch Python, so the global host may be constructed before the interpreter.
// The pimpl is also exposed to host-module functions through pyHostData.
// No virtual destructor is part of IEventListener. execute is an inert debug hook;
// script execution uses game/admin initialization and event callbacks.

#ifndef GAME_PYTHON_PYTHONHOST_H     // [INFERRED] guard style
#define GAME_PYTHON_PYTHONHOST_H

#include <map>
#include <string>
#include <vector>

#include <Python.h>                      // External/python-embedded/py-src-2.3.4 — PyObject is in every signature
#include "../IEventListener.h"           // EventCategory, EventNode, IEventListener

namespace dice {
namespace hfe {

// [UNRESOLVED] enumerators. Named by handleEvent's mangled parameter type; the values are not read.
enum ScriptEvent { ScriptEvent_UNRECOVERED = 0 };

class Team;
namespace world { class IObject; class IPlayer; }

namespace python {

class PythonHostData;                    // 80 bytes — see the note below the class
class RadiusTrigger;                     // py/Triggers.cpp (attested, N)

// Derives from IEventListener, and handleEvent must stay the first declared virtual — it is the
// only vtable slot. The destructor is deliberately NON-virtual; see the header block.
class PythonHost : public IEventListener
{
public:
    // [UNRESOLVED] enumerators. Nested — the mangled name of fireTriggerEvent spells it
    // PythonHost::TriggerMode.
    enum TriggerMode { TriggerMode_UNRECOVERED = 0 };

    PythonHost();
    ~PythonHost();

    // Handles integer user-data payloads; EventNode is unused.
    // Payload 4 suspends script notifications, and suspend transitions are not forwarded.
    virtual void handleEvent(EventCategory, unsigned int, EventNode*, void*);

    // lifecycle
    // isInitialized does NOT read a member of PythonHost — it dereferences the pimpl and reads its
    // first byte: `return *(bool*)m_data;`. PythonHostData +0x00 is the initialised flag.
    bool isInitialized() const;
    // Initializes the embedded interpreter and host module; GameServer requests the full, non-minimal path.
    bool initialize(bool);                               //   [CHOSEN] return
    // THE only Py_Finalize IN THE image IS here (0x815F928), guarded by Py_IsInitialized. The
    // destructor does NOT call it — so teardown is shutdown() first, then the atexit destructor.
    // shutdown clears most of the pimpl's PyObject handles first, but NOT +0x10 and NOT +0x48, which
    // the destructor then decrefs after the interpreter is gone. Shipped, and preserved.
    void shutdown();                                     // (the clean-exit path)
    void reinitialize();

    // Initializes a game mode after lowercasing and validating its name against
    // alphanumerics and underscore before generating Python source. Preserve this
    // validation. Failure returns false; the load path's error diagnostic is fatal.
    bool initializeGameMode(const std::string&);
    void uninitializeGameMode();                         // (the unload path)
    bool initializeAdminMod(const std::string&);         //  / PE 0x42A370; explicit bool on every exit

    // A stub — returns "tl: This was only a debug feature!" and does nothing else. See above.
    std::string execute(const std::string&);

    // per-frame
    void update();
    void updateTimers();
    void updateTriggers();
    void updateTriggersFromObject(world::IObject*);

    // timers and triggers
    void insertTimer(PyObject*);
    void removeTimer(PyObject*);
    void fireTriggerEvent(RadiusTrigger&, TriggerMode, world::IObject*);

    // exceptions
    // Both are thin wrappers over the C API, read in full:
    //     exceptionOccured  -> `return PyErr_Occurred() != 0;`   (dice's spelling, one 'r')
    //     clearExceptions   -> a tail jump straight to PyErr_Clear()
    bool exceptionOccured() const;
    void clearExceptions();
    std::string getExceptionText() const;

    // events and players
    void handleEvent(ScriptEvent, PyObject*);            //   — the NON-virtual one -
                                                         //   (the per-event
                                                         //   handler table walk)
    void removePlayer(world::IPlayer*);

    // The `pythonHost.sendCommand` console method's target: raises ConsoleSendCommand (33) with
    // the command and its non-empty-terminated argument tuple. Always returns true.
    bool consoleSendCommand(std::string, std::string, std::string, std::string, std::string,
                            std::string, std::string, std::string, std::string);

private:
    PythonHostData* m_data;                              // +0x04
};

// Access the host pimpl shared by module callbacks. It owns counted Python
// references, timer/handler containers and the trigger cursor. Its native layout
// is private; preserve container ownership and behavior rather than old STL offsets.
PythonHostData* pyHostData();

// Typed access to the private pimpl's Object class (retail x86 +0x10).
PyObject* pyHostObjectClass();

// The singleton accessor the host-module functions use.
PythonHost* pythonHost();

// The object marshaller lives in py/Objects.cpp. sgl_getControlPointsInGroup is its second
// cross-translation-unit caller (after this file's to_py implementation).
PyObject* wrapObject(world::IObject*);

// The buildArgs family - 23 instantiations in the ELF (PythonHost.cpp lists them); the ones with
// bodies here. Each returns an empty PyRef when Python is not initialised.
class PyRef;
PyRef buildArgs(const int& a0);                              // buildArgs<int>            0x807BF10
PyRef buildArgs(const int& a0, const int& a1);               // buildArgs<int,int>        0x8105750
PyRef buildArgs(world::IObject* const& a0, const int& a1);   // buildArgs<IObject*, int>  0x81719B0
PyRef buildArgs(world::IPlayer* const& a0, world::IObject* const& a1);                // enterVehicle's event 23
PyRef buildArgs(world::IPlayer* const& a0, world::IObject* const& a1, const bool& a2); // enterVehicle's event 21
PyRef buildArgs(world::IPlayer* const& a0);                  // buildArgs<IPlayer*>       0x80A4860, 138 B
PyRef buildArgs(world::IPlayer* const& a0, const int& a1);   // buildArgs<IPlayer*, int>  0x815BAB0, 178 B
PyRef buildArgs(world::IPlayer* const& a0, const bool& a1);  // buildArgs<IPlayer*, bool> 0x8105350, 189 B - selectTeam's event 7
// Argument builders for vehicle-death and player-killed callbacks.
PyRef buildArgs(world::IObject* const& a0, world::IPlayer* const& a1);
PyRef buildArgs(world::IPlayer* const& a0, world::IPlayer* const& a1, world::IObject* const& a2,
                const std::map<world::IPlayer*, int>& a3, world::IObject* const& a4);
                                                             // Argument builders for network-triggered player-death and command callbacks.
PyRef buildArgs(world::IPlayer* const& a0, world::IPlayer* const& a1, world::IObject* const& a2,
                world::IObject* const& a3, world::IObject* const& a4);
PyRef buildArgs(const int& a0, const char* const& a1);
// buildArgs<int, std::string> - the data-block twin of the (int, char const*) one above:
// GameServer::handleDataBlock arm id 6 fires Python event 38 with (connectionId, the block bytes).
// read AND (only PyInt_FromLong + PyString_FromString, both already used here).
PyRef buildArgs(const int& a0, const std::string& a1);
// Argument builders for squad cleanup and chat callbacks.
PyRef buildArgs(world::IPlayer* const& a0, const int& a1, const int& a2);               // Squad::removeFromSquad's event 11
PyRef buildArgs(const int& a0, world::IPlayer* const& a1, world::IPlayer* const& a2);   //  / PE 0x460FB0 - SquadManager::setSquadLeader's event 13
PyRef buildArgs(Team* const& a0, world::IPlayer* const& a1, world::IPlayer* const& a2); //  / PE 0x460F10 - CommanderManager::setCommander's event 12
PyRef buildArgs(const int& a0, const std::string& a1, const std::string& a2,
                const int& a3);                                                         // handleStringReceivedEvent's event 39

// Builds command, player, and integer-tuple arguments for PythonCommandEvent.
PyRef buildArgs(const int& a0, world::IPlayer* const& a1, const std::vector<int>& a2);
// Persistence response: ELF 0x82C3250 / PE 0x5CFB90.
PyRef buildArgs(const int& a0, world::IPlayer* const& a1, const std::string& a2);
//  / PE 0x55FED0: soldier-name validation, event 40.
PyRef buildArgs(const std::string&, const std::string&, const int&, const int&, world::IPlayer* const&);
PyRef buildArgs(const int& a0, world::IPlayer* const& a1, const std::vector<std::vector<int>>& a2);

} // namespace python
} // namespace hfe
} // namespace dice

#endif // GAME_PYTHON_PYTHONHOST_H
